#!/usr/bin/env python3
"""binary_hygiene_check -- no identity strings inside tracked binary files or archives (B446 W1b).

UNWIRED: B446 -- a tracked bytecode file embeds a machine path and its removal awaits the human's approval, so wiring now would turn `verify fast` red

TO WIRE (one line, in the change that removes that file): add `python3 tools/` followed by this
file's own name and ` || ok=1` to fast() in ./verify, after the private_name_gate line.

WHY. The text leak gates read `git grep -I`, and `-I` SKIPS binary files. A
compiled Python module records the path of the source it was built from, an image
or archive can carry one in its metadata, and a zip hides members from every text
tool. That gap is the vector the repo-hygiene playbook warns about, and it was
real here: a tracked bytecode file carried this machine's home path on the public
branch for weeks.

WHAT IT SCANS. Every tracked file (read from the INDEX, not the working tree)
that `git grep -I` would skip: git's own test, a NUL byte in the first 8000
bytes; plus any file with archive magic or a .zip name, plus symlink targets
(a link's target is text git grep never reads). Submodule gitlinks are not files
and are skipped. Archives are opened: zip (every member, member names, nested
archives to depth 3), gzip, bzip2 and xz. Un-openable archive kinds (7z, rar) are
RED rather than skipped. Files it does NOT look inside: compressed chunks of an
image format (PNG zTXt/iTXt), and anything a .gitattributes `-diff`/`binary`
override would hide from git grep (none today; the repo's attributes are
`text=auto` only).

WHAT IT LOOKS FOR, byte-level, with the placeholder allowances of the vendored
text gate (`<name>`, `$USER`, `{x}`, `%x`, `@x` right after the root are examples,
not identities):
  1. a POSIX home path, `/Users/<name>/` or `/home/<name>/`;
  2. a Windows home path, with a single or doubled backslash;
  3. the dash-encoded session-folder form, `-Users-<name>-`;
  4. every name in `.leakcheck-names`, case-sensitive like the text gate, resolved
     through the git common dir so it works from a worktree (same lookup as
     tools/private_name_check.py). Absent: the private-name part is reported as
     NOT RUN, loudly; the path scans still run.

FAIL CLOSED. A blob git cannot return, an archive that will not open, an
encrypted member, a member over the size cap, nesting past the depth cap, a names
file that will not compile, an empty index: each is RED, not a skip.

OUTPUT. Path and KIND of match only; never the matched text, never a member name
(an archive member is reported as its outer path and "inside an archive").

MUST-FAIL CONTROLS, every run: planted bytes of every kind above are caught; the
placeholder forms and a clean binary read clean; a zip, a nested zip, a gzip and a
zip whose only hit is a member NAME are each caught; a corrupt zip, an unreadable
blob and an empty index are RED; a real compiled-bytecode blob built with a fake
source path is caught; and end to end, a scratch repo holding a planted binary
file fails and the same repo with a clean binary passes. The planted strings are
assembled from pieces so this file never contains one.
"""
import bz2
import io
import lzma
import marshal
import os
import pathlib
import re
import subprocess
import sys
import tempfile
import zipfile
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
NAMES = ".leakcheck-names"
NUL_WINDOW = 8000                    # git's own binary test (buffer_is_binary)
MAX_BYTES = 256 * 1024 * 1024        # per archive, decompressed, all members
MAX_DEPTH = 3
PH = rb"(?![<${@%])"                 # the vendored gate's placeholder allowance
NAME = rb"[^/\x00-\x1f]{1,128}"
IDENT = [
    (re.compile(rb"/(?:Users|home)/" + PH + NAME + rb"/"), "machine-absolute home path"),
    (re.compile(rb"[A-Za-z]:\\+Users\\+" + PH + rb"[^\\\x00-\x1f]"), "machine-absolute Windows home path"),
    (re.compile(rb"(?:\A|(?<=[/\s(=]))-(?:Users|home)-" + PH + rb"[^-/\s\x00-\x1f]{1,128}-"),
     "dash-encoded home path"),
]
ARCHIVE_UNSUPPORTED = (b"7z\xbc\xaf\x27\x1c", b"Rar!\x1a\x07")
ZIP_MAGIC = (b"PK\x03\x04", b"PK\x05\x06", b"PK\x07\x08")


class Unreadable(Exception):
    """Raised for anything that must be RED rather than skipped."""


def scan(data, names_re, depth=0, budget=None):
    """-> (set of kind strings). Raises Unreadable. `names_re` may be None."""
    if budget is None:
        budget = [MAX_BYTES]
    kinds = set()

    def raw(b):
        for rx, kind in IDENT:
            if rx.search(b):
                kinds.add(kind)
        if names_re is not None and names_re.search(b):
            kinds.add("private name")

    def inner(b):
        budget[0] -= len(b)
        if budget[0] < 0:
            raise Unreadable("archive expands past the size cap")
        if depth + 1 > MAX_DEPTH:
            raise Unreadable("archives nested past the depth cap")
        for k in scan(b, names_re, depth + 1, budget):
            kinds.add(f"{k} (inside an archive)" if "inside an archive" not in k else k)

    raw(data)
    head = data[:8]
    if any(head.startswith(m) for m in ZIP_MAGIC):
        try:
            with zipfile.ZipFile(io.BytesIO(data)) as zf:
                for info in zf.infolist():
                    # leading "/" because member names are stored without one
                    raw(b"/" + info.filename.encode("utf-8", "surrogateescape"))
                    if info.is_dir():
                        continue
                    if info.flag_bits & 0x1:
                        raise Unreadable("encrypted archive member")
                    if info.file_size > budget[0]:
                        raise Unreadable("archive member over the size cap")
                    inner(zf.read(info))
        except Unreadable:
            raise
        except (zipfile.BadZipFile, NotImplementedError, RuntimeError, zlib.error, EOFError,
                OSError, ValueError) as e:
            raise Unreadable(f"zip will not open ({type(e).__name__})")
    elif head.startswith(b"\x1f\x8b"):
        d = zlib.decompressobj(31)
        try:
            out = d.decompress(data, MAX_BYTES + 1)
        except zlib.error as e:
            raise Unreadable(f"gzip will not open ({type(e).__name__})")
        if len(out) > MAX_BYTES:
            raise Unreadable("gzip expands past the size cap")
        inner(out)
    elif head.startswith(b"BZh"):
        try:
            out = bz2.BZ2Decompressor().decompress(data, MAX_BYTES + 1)
        except (OSError, ValueError, EOFError) as e:
            raise Unreadable(f"bzip2 will not open ({type(e).__name__})")
        if len(out) > MAX_BYTES:
            raise Unreadable("bzip2 expands past the size cap")
        inner(out)
    elif head.startswith(b"\xfd7zXZ\x00"):
        try:
            out = lzma.LZMADecompressor().decompress(data, MAX_BYTES + 1)
        except (lzma.LZMAError, EOFError) as e:
            raise Unreadable(f"xz will not open ({type(e).__name__})")
        if len(out) > MAX_BYTES:
            raise Unreadable("xz expands past the size cap")
        inner(out)
    elif any(data.startswith(m) for m in ARCHIVE_UNSUPPORTED):
        raise Unreadable("an archive format this check cannot open")
    return kinds


def is_candidate(path, mode, data):
    """Would `git grep -I` skip it, or is it an archive / a symlink target?"""
    if mode == "120000":
        return True
    if b"\x00" in data[:NUL_WINDOW]:
        return True
    return path.lower().endswith(".zip") or data[:4] in ZIP_MAGIC


def git(cwd, *args, **kw):
    env = {k: v for k, v in os.environ.items()
           if k not in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_COMMON_DIR")}
    return subprocess.run(["git", *args], cwd=cwd, env=env, capture_output=True, **kw)


def index_entries(cwd):
    """-> [(path, mode, sha)] for every non-gitlink index entry. Raises Unreadable."""
    r = git(cwd, "ls-files", "-s", "-z")
    if r.returncode != 0:
        raise Unreadable(f"git ls-files failed (exit {r.returncode})")
    out = []
    for rec in r.stdout.split(b"\0"):
        if not rec:
            continue
        meta, _, path = rec.partition(b"\t")
        mode, sha, _stage = meta.decode().split(" ")
        if mode == "160000":
            continue                                    # a submodule, not a file
        out.append((path.decode("utf-8", "surrogateescape"), mode, sha))
    if not out:
        raise Unreadable("the index lists no files -- a scan of nothing is blind, not clean")
    return out


def read_blobs(cwd, shas):
    """-> {sha: bytes or None} via one `git cat-file --batch`; None = unreadable."""
    r = git(cwd, "cat-file", "--batch", input="\n".join(sorted(set(shas))).encode() + b"\n")
    got, buf, pos = {}, r.stdout, 0
    for sha in sorted(set(shas)):
        nl = buf.find(b"\n", pos)
        if nl < 0:
            got[sha] = None
            continue
        hdr = buf[pos:nl].split(b" ")
        if len(hdr) != 3 or hdr[1] != b"blob":
            got[sha] = None
            pos = nl + 1
            continue
        size = int(hdr[2])
        got[sha] = buf[nl + 1:nl + 1 + size]
        pos = nl + 1 + size + 1
    return got


def names_regex(cwd):
    """-> (compiled or None, note). Same lookup as private_name_check.py (kept
    local: that file is a gate script, this one must also run standalone)."""
    cands = []
    r = git(cwd, "rev-parse", "--path-format=absolute", "--git-common-dir", text=True)
    if r.returncode == 0 and r.stdout.strip():
        cands.append(pathlib.Path(r.stdout.strip()).parent / NAMES)
    r = git(cwd, "rev-parse", "--show-toplevel", text=True)
    if r.returncode == 0 and r.stdout.strip():
        cands.append(pathlib.Path(r.stdout.strip()) / NAMES)
    for c in cands:
        if c.is_file():
            lines = [l for l in c.read_text(encoding="utf-8").splitlines()
                     if l.strip() and not l.lstrip().startswith("#")]
            if not lines:
                return None, "names file is empty"
            try:
                return re.compile("|".join(lines).encode("utf-8")), None
            except re.error as e:
                raise Unreadable(f"names file does not compile ({e})")
    return None, "names file not found here or in the main checkout"


def check(cwd, names_re, reader=read_blobs):
    """-> (problems, n_scanned). Each problem is a printable line, path + kind only."""
    try:
        entries = index_entries(cwd)
    except Unreadable as e:
        return [f"(index): {e}"], 0
    blobs = reader(cwd, [sha for _, _, sha in entries])
    problems, scanned = [], 0
    for path, mode, sha in entries:
        data = blobs.get(sha)
        if data is None:
            problems.append(f"{path}: cannot be read from git -- fail closed")
            continue
        if not is_candidate(path, mode, data):
            continue
        scanned += 1
        if path.lower().endswith(".zip") and data[:4] not in ZIP_MAGIC:
            problems.append(f"{path}: named .zip but is not a readable zip -- fail closed")
            continue
        try:
            kinds = scan(data, names_re)
        except Unreadable as e:
            problems.append(f"{path}: {e} -- fail closed")
            continue
        for k in sorted(kinds):
            problems.append(f"{path}: {k}")
    return problems, scanned


# ---- controls --------------------------------------------------------------
# Every planted string is built from pieces: this file must not contain one, or
# the text gates (which read it) would flag the controls themselves.
def _posix(root="Users", who="someone"):
    return ("/" + root + "/" + who + "/proj/x.py").encode()


def _win(sep="\\"):
    return ("C:" + sep + "Users" + sep + "bob" + sep + "x").encode()


def _dash():
    return (" -" + "Users" + "-someone-Documents").encode()


def _zip(members):
    b = io.BytesIO()
    with zipfile.ZipFile(b, "w", zipfile.ZIP_DEFLATED) as z:
        for n, d in members:
            z.writestr(n, d)
    return b.getvalue()


def selftest():
    fake = "Zq" + "PlantedFake"
    names = re.compile(fake.encode())
    pad = b"\x00\x01\x02 junk "
    flag = {
        "posix path": pad + _posix() + pad,
        "home path": pad + _posix("home", "u") + pad,
        "windows path": pad + _win() + pad,
        "windows doubled": pad + _win("\\\\") + pad,
        "dash form": pad + _dash() + pad,
        "private name": pad + fake.encode() + pad,
        "zip member": _zip([("a.bin", pad + _posix() + pad)]),
        "zip member name only": _zip([(_posix().decode().lstrip("/") + "/n", b"clean")]),
        "nested zip": _zip([("in.zip", _zip([("b.bin", pad + _posix() + pad)]))]),
        "gzip": _gz(pad + _posix() + pad),
        "compiled bytecode": marshal.dumps(compile("x = 1", _posix().decode() + "m.py", "exec")),
    }
    for name, data in flag.items():
        try:
            if not scan(data, names):
                return f"selftest: '{name}' was NOT caught"
        except Unreadable as e:
            return f"selftest: '{name}' raised instead of being caught ({e})"
    clean = {
        "placeholder angle": pad + b"/Users/<user>/x /home/<u>/y" + pad,
        "placeholder dollar": pad + b"/home/$USER/x" + pad,
        "placeholder windows": pad + b"C:\\Users\\<u>\\x" + pad,
        "placeholder dash": pad + b" -Users-<name>-x" + pad,
        "ordinary binary": bytes(range(256)) * 8,
        "clean zip": _zip([("a.txt", b"hello"), ("d/b.bin", b"\x00\x01")]),
        "no-slash tail": pad + b"/Users/trailing-no-slash",
    }
    for name, data in clean.items():
        try:
            got = scan(data, names)
        except Unreadable as e:
            return f"selftest: clean case '{name}' raised ({e})"
        if got:
            return f"selftest: clean case '{name}' was flagged ({sorted(got)})"
    if scan(pad + fake.encode(), None):
        return "selftest: a private name was reported with no names file loaded"

    for name, data in {
        "corrupt zip": b"PK\x03\x04" + b"\x00garbage" * 20,
        "unsupported archive": b"7z\xbc\xaf\x27\x1c" + b"\x00" * 32,
        "corrupt gzip": b"\x1f\x8b" + b"\x00bad" * 10,
        "nesting past the cap": _nest(MAX_DEPTH + 1),
    }.items():
        try:
            scan(data, None)
            return f"selftest: '{name}' did not fail closed"
        except Unreadable:
            pass

    probs, _ = check(ROOT, None, reader=lambda cwd, shas: {s: None for s in shas})
    if not probs:
        return "selftest: an unreadable blob read clean (fail-open)"

    with tempfile.TemporaryDirectory() as td:
        td = pathlib.Path(td)
        if git(td, "init", "-q").returncode != 0:
            return "selftest: could not create a scratch repo"
        (td / "t.txt").write_text("text\n")
        (td / "clean.bin").write_bytes(b"\x00\x01clean\x02")
        git(td, "add", "t.txt", "clean.bin")
        probs, n = check(td, names)
        if probs or n != 1:
            return f"selftest: a clean scratch repo read red or miscounted ({probs}, {n})"
        (td / "fake.zip").write_bytes(b"not a zip, but long enough\n")
        git(td, "add", "fake.zip")
        probs, _ = check(td, names)
        if not any(p.startswith("fake.zip:") for p in probs):
            return "selftest: a file named .zip that is not a zip did not fail closed"
        git(td, "rm", "-q", "--cached", "fake.zip")
        (td / "planted.bin").write_bytes(pad + _posix() + pad)
        git(td, "add", "planted.bin")
        probs, _ = check(td, names)
        if probs != ["planted.bin: machine-absolute home path"]:
            return f"selftest: the planted binary file was not reported exactly ({probs})"
        git(td, "rm", "-q", "--cached", "planted.bin")
        git(td, "rm", "-q", "--cached", "t.txt", "clean.bin")
        probs, _ = check(td, names)
        if not probs:
            return "selftest: an empty index read clean (blind scanner)"
    return None


def _gz(b):
    c = zlib.compressobj(wbits=31)
    return c.compress(b) + c.flush()


def _nest(depth):
    data = b"\x00clean"
    for _ in range(depth):
        data = _zip([("n.zip", data)])
    return data


def main():
    bad = selftest()
    if bad:
        print(f"binary_hygiene_check: FAILED -- {bad}", file=sys.stderr)
        return 1
    try:
        names_re, note = names_regex(ROOT)
    except Unreadable as e:
        print(f"binary_hygiene_check: FAILED -- {e}", file=sys.stderr)
        return 1
    if names_re is None:
        print(f"binary_hygiene_check: WARNING -- private-name scan NOT RUN ({note}); "
              "path scans still ran.", file=sys.stderr)
    problems, scanned = check(ROOT, names_re)
    if problems:
        print("binary_hygiene_check: FAILED -- identity strings in tracked binary files or "
              "archives (path and kind only):", file=sys.stderr)
        for p in problems:
            print(f"  {p}", file=sys.stderr)
        return 1
    print(f"binary_hygiene_check: GREEN ({scanned} binary files/archives scanned; controls ok)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
