#!/usr/bin/env python3
"""private_name_check -- no private sibling's real name in a tracked or new file (ADR-014).

WIRED: ./verify fast (replaces the inline private_name_gate body; B446 P1 W1b)

WHY THIS IS A TOOL AND NOT SHELL. The names live in `.leakcheck-names`, which is
untracked by design (the tracked gate must never contain the thing it forbids).
Untracked means an agent worktree never has it, so the old inline gate read
`[ -f .leakcheck-names ]` in the worktree, found nothing, and printed a SKIPPED
line that every implementer's `./verify fast` scrolled past: 45 of 49 worktrees
audited on 2026-10-05 were running the gate blind (B446 P0 lane 4, L4-M2).

THE LOOKUP. The file is resolved through `git rev-parse --git-common-dir`, whose
parent directory is the main checkout in EVERY worktree of the repo and in the
main checkout itself (`labs_preview.sh` uses the same trick). This checkout's own
copy is the fallback, so a plain clone behaves as before.

WHEN THE FILE IS ABSENT EVERYWHERE. Printed as a WARNING, exit 0, never silent,
and never a failure by default: the file is machine-local by design, so CI and
anyone's fresh clone of this public repo cannot have it, and a hard failure would
make `./verify` unpassable for them. The owner's machine can make it a failure
with HORDE_REQUIRE_LEAKCHECK_NAMES=1 (set it where the file is expected, e.g. in
an agent's environment).

WHAT IT CHECKS. Case-SENSITIVE, by design (a case-insensitive first draft tripped
on ordinary prose): `git grep --untracked -nIP` of the names joined as one
alternation (every non-comment, non-blank line of the file is one PCRE) over the
working tree, excluding PRIVATE-NOTES.md (the alias map) and the names file.
Unlike the shell gate it replaces: (1) a git/PCRE error is RED, not swallowed (the
old `|| true` read a malformed pattern as "no hits"); (2) a hit prints
`path:line` only, never the line, so the name is not echoed into transcripts and
CI logs. Open the file at that line to see it.

MUST-FAIL CONTROLS, every run, in a scratch repo with a FAKE name, never touching
the real file or tree: (1) in a worktree whose own tree lacks the names file but
whose common dir has it, a planted name is caught; (2) the same worktree without
the plant reads clean (the must-read-zero control: a detector that is always red
proves nothing); (3) with the names file absent everywhere the lookup says so and
the strict switch turns it red.
"""
import os
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
NAMES = ".leakcheck-names"
STRICT_ENV = "HORDE_REQUIRE_LEAKCHECK_NAMES"
EXCLUDES = [":(exclude)PRIVATE-NOTES.md", f":(exclude){NAMES}"]


def _env():
    # An ambient GIT_DIR (inside a hook, say) would redirect every call below
    # to the wrong repository.
    return {k: v for k, v in os.environ.items()
            if k not in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_COMMON_DIR")}


def git(cwd, *args, check=True):
    return subprocess.run(["git", *args], cwd=cwd, env=_env(), capture_output=True,
                          text=True, check=check)


def names_file(cwd):
    """The names file visible from `cwd`, or None. Common-dir parent first (the
    main checkout, same answer from every worktree), then this checkout."""
    cands = []
    r = git(cwd, "rev-parse", "--path-format=absolute", "--git-common-dir", check=False)
    if r.returncode == 0 and r.stdout.strip():
        cands.append(pathlib.Path(r.stdout.strip()).parent / NAMES)
    r = git(cwd, "rev-parse", "--show-toplevel", check=False)
    if r.returncode == 0 and r.stdout.strip():
        cands.append(pathlib.Path(r.stdout.strip()) / NAMES)
    for c in cands:
        if c.is_file():
            return c
    return None


def pattern(path):
    """Same join as the shell gate: every line that is not blank or a comment."""
    lines = [l.rstrip("\n") for l in path.read_text(encoding="utf-8").splitlines()]
    return "|".join(l for l in lines if l.strip() and not l.lstrip().startswith("#"))


def hits(cwd, pat):
    """-> (list of 'path:line', error string or None). -z so a path holding a
    colon cannot corrupt the parse; a record is path NUL line NUL text."""
    r = git(cwd, "grep", "--untracked", "-nIPz", "-e", f"({pat})", "--", ".", *EXCLUDES,
            check=False)
    if r.returncode == 1:
        return [], None
    if r.returncode != 0:
        return [], f"git grep failed (exit {r.returncode}) — a malformed pattern in {NAMES}?"
    out = []
    for rec in r.stdout.split("\n"):
        parts = rec.split("\0")
        if len(parts) >= 2 and parts[0]:
            out.append(f"{parts[0]}:{parts[1]}")
    return out, None


def check(cwd, require):
    """-> (exit code, stderr lines). The whole policy for one checkout."""
    nf = names_file(cwd)
    if nf is None:
        msg = (f"verify: WARNING — {NAMES} not found here or in the main checkout: the "
               "private-name leak check DID NOT RUN. Expected in CI and in clones by anyone "
               "but the owner; on the owner's machine it means this gate is blind.")
        if require:
            return 1, [msg.replace("WARNING", "FAILED"), f"  ({STRICT_ENV} is set)"]
        return 0, [msg]
    try:
        pat = pattern(nf)
    except (OSError, UnicodeDecodeError) as e:
        return 1, [f"verify: private-name check FAILED — cannot read {NAMES}: {type(e).__name__}"]
    if not pat:
        return 0, []
    found, err = hits(cwd, pat)
    if err:
        return 1, [f"verify: private-name check FAILED — {err}"]
    if found:
        return 1, ["verify: LEAK — a private sibling's real name in a tracked or new file "
                   "(alias it; map in PRIVATE-NOTES.md). Open each at the line; the text is "
                   "deliberately not echoed:"] + [f"    {h}" for h in found]
    return 0, []


def selftest():
    """Controls on a scratch repo + worktree and a FAKE name. -> failure string or None."""
    fake = "Zq" + "PlantedFakeName"       # not a real name; split so no scan ever matches this file
    with tempfile.TemporaryDirectory() as td:
        td = pathlib.Path(td)
        main, wt = td / "main", td / "wt"
        main.mkdir()
        cfg = ["-c", "commit.gpgsign=false", "-c", "core.hooksPath=/dev/null",
               "-c", "user.name=t", "-c", "user.email=t@t.invalid"]
        try:
            git(main, "init", "-q")
            (main / "a.txt").write_text("ordinary prose\n")
            git(main, "add", "a.txt")
            git(main, *cfg, "commit", "-qm", "x")
            git(main, "worktree", "add", "-q", str(wt), "-b", "ctl")
        except subprocess.CalledProcessError as e:
            return f"selftest: scratch repo setup failed ({e.stderr.strip()[:120]})"

        if names_file(wt) is not None:
            return "selftest: a names file was found before any was written"
        (main / NAMES).write_text(f"# comment\n\n{fake}\n")
        if (wt / NAMES).exists():
            return "selftest: the worktree unexpectedly holds the names file"
        nf = names_file(wt)
        if nf is None or nf.resolve() != (main / NAMES).resolve():
            return "selftest: the worktree did not resolve the main checkout's names file"

        rc, _ = check(wt, False)
        if rc != 0:
            return "selftest: a clean worktree read red (the detector is always-red)"
        (wt / "planted.txt").write_text(f"hello {fake} world\n")
        rc, lines = check(wt, False)
        if rc == 0:
            return "selftest: a planted name in a worktree was NOT caught (the gate is blind there)"
        if any(fake in l for l in lines):
            return "selftest: a hit echoed the name it found"
        (wt / "planted.txt").unlink()

        (main / NAMES).unlink()
        rc, lines = check(wt, False)
        if rc != 0 or not any("WARNING" in l for l in lines):
            return "selftest: an absent names file was not a visible warning"
        rc, _ = check(wt, True)
        if rc == 0:
            return f"selftest: {STRICT_ENV} did not turn an absent names file red"
    return None


def main():
    bad = selftest()
    if bad:
        print(f"private_name_check: FAILED — {bad}", file=sys.stderr)
        return 1
    rc, lines = check(ROOT, os.environ.get(STRICT_ENV) == "1")
    for l in lines:
        print(l, file=sys.stderr)
    if rc == 0 and not any("WARNING" in l for l in lines):
        print("private_name_check: GREEN (controls ok)")
    return rc


if __name__ == "__main__":
    sys.exit(main())
