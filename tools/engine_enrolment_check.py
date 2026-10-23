#!/usr/bin/env python3
"""engine_enrolment_check -- every engine and module under h2/ (and engine2/core) is in the harness.

WIRED: ./verify fast

WHY (B448 phase 2, package P2, ADR-209; armor row S5, "happy-path tests").
docs/strategy/blind-spot-armor-phase2.md section 2 rule 4: "A small registry lists
every engine and module under h2/. A new one turns R4, R5, R6, R9, R10 and S5 red
until it joins the harness." The fuzz, the invariance matrix and the soak loop over
the harness's enrolled engines, so an engine that is not enrolled is simply never
tested, and nothing would say so. This check is what says so.

WHAT IT DOES. It FINDS the units itself: every directory under h2/ that directly
holds a C or C++ file, tracked or new-and-not-ignored (`ls-files -co
--exclude-standard`; a brand-new directory is exactly the case). Nobody has to
remember to list one. Then, against docs/armor/engines.json:
  UNENROLLED  a found directory has no registry entry.
  TOPLEVEL    a code file sits directly in h2/ and TOP_LEVEL_SUPPORT does not name it, or
              names a file that is gone.
  STALE       a registry entry names a directory that holds no code.
  SHAPE       an entry is malformed: an unknown kind, a duplicate, an engine or
              module with no id or adapter, an exemption with no reason, or a
              `harness` entry that is not h2/harness.
  ADAPTER     an enrolled unit's adapter file is missing, is outside h2/harness/,
              or includes no file from the unit's own directory (an adapter that
              wraps something else enrols nothing).
  TABLE       the unit has no row in h2/harness/adapters.h, that file does not
              include its adapter, or the table has an engine row no entry owns.
  PATCHES     h2/harness/patches.json has no reference patch for the unit, none
              that is expected to be loud (must-read-loud would then test nothing),
              or a patch for an engine that is not enrolled.
  REFERENCE   a `test-reference` unit is included by an enrolled unit or by an
              adapter: it is product code after all and must be enrolled.

ENGINE2 (B535, repo audit 2026-10-21 H4). engine2/ is the clean-room engine ADR-223 says replaces
h2/engine, and a discovery that stops at h2/ would stay green over code it never read. Three rules,
all of them constants below with their reasons, and no new registry kind:
  engine2/core  is ONE unit (the builder adapts its layout, engine2/README.md BD-1, so its
                directories are not units). While EVERY code file in it is a stub (nothing but
                comments, blank lines, `#pragma once` and `#include`: today's empty headers and
                one-line translation unit) it needs no entry and the green line says PENDING. The
                day a file in it holds a line of real code, it is UNENROLLED until docs/armor/
                engines.json lists `engine2/core` as an engine (adapter, table row, patches: the
                same demands as h2/engine). That is the point: engine code arriving over a green
                check it never read is the failure this closes.
  engine2/{tests,tools,wasm}  hold code that is not the engine (E2_NON_ENGINE names each with its
                reason); they are not units. This is the ONLY exemption.
  anywhere else in engine2/   a code file outside core/ and E2_NON_ENGINE is STRAY, as TOPLEVEL is
                for h2/: a new engine directory dropped beside core/ is seen, not skipped.

WHAT IT DOES NOT PROVE, AND WHERE THAT IS PROVED. This check is STATIC: it reads
files and builds nothing. It cannot tell an adapter that drives its engine from one
that returns silence. That is h2/harness/selftest.mjs's job: on the built render
tool it renders every reference patch of every enrolled engine and holds each to
its `expect`, with its own controls (an adapter that returns silence must FAIL
must-read-loud; each loud patch must read exact zero with no note played; a tone
of known level must read that level).
  THE SELFTEST IS NOT RUN BY ./verify YET, and this check's green line says so on
  every run. It needs a compiled binary, and this file may not build one: a compile
  line in a tools/ script is a `direct_compiles` entry of tools/build_flags_check.py,
  which only the human's approval can pin, and CMakeLists.txt was outside this
  package's brief. h2/harness/README.md gives the two ways to wire it.
Also not proved: a new engine entered as `test-reference` on the day it is created
is not caught unless something enrolled includes it. The registry is an approval
ledger and belongs under the same code-owner review as the others.

MUST-FAIL CONTROLS, every run (LIBRARY L0032: a probe must prove
it can go red). Each plants one fault in a copy of the tree's model, asserts the
plant changed it, and demands the matching verdict by name, once on a small
synthetic tree and once on the REAL one:
  a new engine directory with no entry (top level, under cores/, and two levels
  down); the engine's entry deleted; the engine relabelled `harness`; an unknown
  kind; its adapter missing; its adapter including nothing of the engine; its table
  row gone; a table row no entry owns; its patches gone; its loud patches gone; a
  patch for an unknown engine; a stale entry; an exemption with no reason; the
  engine including a test reference; for engine2: real code in engine2/core with no entry,
  a code file in an unnamed engine2 directory, one directly in engine2/.
The conforming synthetic tree must read green (the must-read-zero: a judge that is
always red would pass every plant); so must the same tree with a new STUB header in
engine2/core and a code file under engine2/tests (the quiet controls). One more control
runs the DISCOVERY end to end in a scratch repository: a staged directory, a new unstaged
one, an ignored one and one with no code must read as found, found, not found and not
found, and engine2 code, tracked or new, must be listed.

Exit 0 green, 1 red. No clock, no randomness.
"""
import copy
import json
import os
import pathlib
import posixpath
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
# the C/C++ lexer is reused, not copied (and no lint-suppression comment here: weakening_check counts those)
from banned_api_check import strip_noncode

ROOT = pathlib.Path(__file__).resolve().parent.parent
REGISTRY = "docs/armor/engines.json"
TABLE = "h2/harness/adapters.h"
PATCHES = "h2/harness/patches.json"
HARNESS_DIR = "h2/harness"
E2_CORE = "engine2/core"
# Directories of engine2/ whose code is not the engine. Each is exempt from discovery, with its reason.
E2_NON_ENGINE = {
    "engine2/tests": "the acceptance suite and its runner: they test the engine, they are not an engine or a module",
    "engine2/tools": "tooling (the click detector); no engine code",
    "engine2/wasm": "the WebAssembly build stub; build files, no engine code",
}
SELFTEST = "h2/harness/selftest.mjs"
CODE_EXTS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".ipp", ".tpp", ".inc"}
ENROLLED_KINDS = ("engine", "module")
EXEMPT_KINDS = ("test-reference", "harness")
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.M)
# One engine row of adapters.h: {"<id>", false, &make...}, (a control row says true)
TABLE_ROW = re.compile(r'^\s*\{\s*"([^"]+)"\s*,\s*false\s*,', re.M)


# ---- discovery -----------------------------------------------------------------

def list_h2_files(cwd):
    """-> (sorted repo-relative files under h2/ and engine2/, error). Tracked files and new files
    git does not ignore, so a directory nobody has committed yet is seen."""
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    r = subprocess.run(["git", "ls-files", "-co", "--exclude-standard", "-z", "--", "h2", "engine2"],
                       cwd=cwd, capture_output=True, text=True, env=env)
    if r.returncode != 0:
        return None, f"git ls-files failed (exit {r.returncode}): {r.stderr.strip()[:200]}"
    files = [f for f in r.stdout.split("\0") if f and (pathlib.Path(cwd) / f).is_file()]
    return sorted(set(files)), None


# Code files that sit directly in h2/, by name. The level used to be skipped whole, so a new
# engine dropped there was seen by nothing (a critic planted one, 2026-10-11). A file here is
# support code that every unit may include and that is no engine or module itself; anything
# else directly in h2/ is red until it moves into a unit or is entered here with its reason.
TOP_LEVEL_SUPPORT = {
    "h2/flush_to_zero.h": "the flush-to-zero switch (ADR-211): floating-point environment, no engine or module",
}


def units_of(files):
    """The directories under h2/ that directly hold C or C++ code, plus engine2/core as one
    unit when any code is in it."""
    out = {posixpath.dirname(f) for f in files
           if f.startswith("h2/") and posixpath.splitext(f)[1] in CODE_EXTS and posixpath.dirname(f) != "h2"}
    if core_code(files):
        out.add(E2_CORE)
    return sorted(out)


def core_code(files):
    """The C or C++ files anywhere under engine2/core."""
    return [f for f in files if f.startswith(E2_CORE + "/") and posixpath.splitext(f)[1] in CODE_EXTS]


def is_stub(text):
    """True when the file holds nothing but comments, blank lines, `#pragma once` and `#include`."""
    for line in strip_noncode(text).split("\n"):
        t = line.strip()
        if t and not re.match(r"#\s*(?:pragma\s+once|include\b)", t):
            return False
    return True


def stray_engine2(files):
    """Code files in engine2/ that are neither under core/ nor under a E2_NON_ENGINE directory."""
    return sorted(f for f in files
                  if f.startswith("engine2/") and posixpath.splitext(f)[1] in CODE_EXTS
                  and not f.startswith(E2_CORE + "/")
                  and not any(f.startswith(d + "/") for d in E2_NON_ENGINE))


def stray_top_level(files):
    """Code files directly in h2/ that TOP_LEVEL_SUPPORT does not name."""
    return sorted(f for f in files
                  if posixpath.splitext(f)[1] in CODE_EXTS and posixpath.dirname(f) == "h2"
                  and f not in TOP_LEVEL_SUPPORT)


def load_model(cwd):
    """-> (model, error). The model is everything judge() reads, so a control can
    plant a fault in a copy of it without touching the tree."""
    files, err = list_h2_files(cwd)
    if err:
        return None, err
    root = pathlib.Path(cwd)
    texts = {}
    for f in files:
        if posixpath.splitext(f)[1] in CODE_EXTS:
            texts[f] = (root / f).read_text(encoding="utf-8", errors="replace")
    model = {"files": files, "texts": texts}
    for key, rel in (("registry", REGISTRY), ("patches", PATCHES)):
        try:
            model[key] = json.loads((root / rel).read_text(encoding="utf-8"))
        except FileNotFoundError:
            return None, f"{rel} is missing"
        except ValueError as e:
            return None, f"{rel} is not JSON ({e})"
    return model, None


# ---- the judge -------------------------------------------------------------------

def includes_of(rel, text):
    """The quoted includes of one file, resolved against its own directory."""
    base = posixpath.dirname(rel)
    return [posixpath.normpath(posixpath.join(base, p)) for p in INCLUDE.findall(text)]


def judge(model):
    """-> sorted list of failure strings, each starting with its verdict name. Pure."""
    fails = []
    files, texts = model["files"], model["texts"]
    found = units_of(files)
    if not found:
        return ["BLIND: no directory with C or C++ code was found under h2/ (a scan that saw nothing is not a clean tree)"]
    reg = model["registry"]
    units = reg.get("units") if isinstance(reg, dict) else None
    if not isinstance(units, list):
        return [f"SHAPE: {REGISTRY} has no `units` list"]

    by_path, ids = {}, {}
    for u in units:
        path = u.get("path") if isinstance(u, dict) else None
        if not isinstance(path, str) or not path:
            fails.append(f"SHAPE: {REGISTRY} has an entry with no path")
            continue
        if path in by_path:
            fails.append(f"SHAPE: {path} is listed twice in {REGISTRY}")
            continue
        by_path[path] = u
        kind = u.get("kind")
        if kind in ENROLLED_KINDS:
            uid, adapter = u.get("id"), u.get("adapter")
            if not isinstance(uid, str) or not uid or not isinstance(adapter, str) or not adapter:
                fails.append(f"SHAPE: {path} is an {kind} and needs an `id` and an `adapter`")
            elif uid in ids:
                fails.append(f"SHAPE: the id {uid!r} is used by both {ids[uid]} and {path}")
            else:
                ids[uid] = path
        elif kind in EXEMPT_KINDS:
            why = u.get("why_not_enrolled")
            if not isinstance(why, str) or not why.strip():
                fails.append(f"SHAPE: {path} is not enrolled ({kind}) and gives no `why_not_enrolled`")
            if kind == "harness" and path != HARNESS_DIR:
                fails.append(f"SHAPE: {path} is entered as `harness`; only {HARNESS_DIR} is the harness")
        else:
            fails.append(f"SHAPE: {path} has kind {kind!r}; the kinds are {', '.join(ENROLLED_KINDS + EXEMPT_KINDS)}")

    for f in stray_top_level(files):
        fails.append(f"TOPLEVEL: {f} is code directly in h2/ that nothing names. Move it into a unit, or "
                     f"enter it in TOP_LEVEL_SUPPORT with the reason it is support code and no engine")
    for f in sorted(TOP_LEVEL_SUPPORT):
        if f not in files:
            fails.append(f"TOPLEVEL: {f} is named in TOP_LEVEL_SUPPORT and does not exist (a stale entry)")

    for f in stray_engine2(files):
        fails.append(f"STRAY: {f} is engine2 code outside {E2_CORE}/ and outside {', '.join(sorted(E2_NON_ENGINE))}. "
                     f"Move it into {E2_CORE}/ (one unit), or name its directory in E2_NON_ENGINE with the reason it is no engine")
    for d in found:
        if d == E2_CORE and d not in by_path:
            real = [f for f in core_code(files) if not is_stub(texts.get(f, ""))]
            if real:
                more = f" and {len(real) - 1} more" if len(real) > 1 else ""
                fails.append(f"UNENROLLED: {E2_CORE} holds code ({real[0]}{more}) and has no entry in {REGISTRY}. "
                             f"The engine has arrived: enrol it as an engine (h2/harness/README.md) before it is trusted")
            continue                                   # stub-only: PENDING, no entry needed
        if d not in by_path:
            fails.append(f"UNENROLLED: {d} holds code and has no entry in {REGISTRY}. Enrol it "
                         f"(h2/harness/README.md) or enter it with the reason it is not an engine or a module")
    for path in by_path:
        if path not in found:
            fails.append(f"STALE: {REGISTRY} lists {path}, which holds no C or C++ code")

    table = texts.get(TABLE)
    rows = TABLE_ROW.findall(table) if table is not None else []
    if table is None:
        fails.append(f"TABLE: {TABLE} is missing")
    table_includes = includes_of(TABLE, table) if table is not None else []
    patches = model["patches"].get("patches") if isinstance(model["patches"], dict) else None
    if not isinstance(patches, list):
        fails.append(f"PATCHES: {PATCHES} has no `patches` list")
        patches = []

    enrolled_dirs = {path for path, u in by_path.items() if u.get("kind") in ENROLLED_KINDS}
    adapters = set()
    for uid, path in sorted(ids.items()):
        adapter = by_path[path]["adapter"]
        adapters.add(adapter)
        text = texts.get(adapter)
        if text is None:
            fails.append(f"ADAPTER: {path} ({uid}) names the adapter {adapter}, which does not exist")
        elif posixpath.dirname(adapter) != HARNESS_DIR:
            fails.append(f"ADAPTER: {path} ({uid}): the adapter {adapter} is not in {HARNESS_DIR}/")
        elif not any(inc.startswith(path + "/") for inc in includes_of(adapter, text)):
            fails.append(f"ADAPTER: {adapter} includes no file from {path}/, so it does not wrap {uid}")
        if table is not None:
            if uid not in rows:
                fails.append(f"TABLE: {TABLE} has no engine row for {uid!r} ({path})")
            if adapter not in table_includes:
                fails.append(f"TABLE: {TABLE} does not include {adapter}")
        mine = [p for p in patches if isinstance(p, dict) and p.get("engine") == uid]
        if not mine:
            fails.append(f"PATCHES: {PATCHES} has no reference patch for {uid!r} ({path})")
        elif not any(p.get("expect") == "loud" for p in mine):
            fails.append(f"PATCHES: no reference patch of {uid!r} expects `loud`, so the must-read-loud control would test nothing")
    for row in rows:
        if row not in ids:
            fails.append(f"TABLE: {TABLE} has an engine row {row!r} that no entry in {REGISTRY} owns")
    for p in patches:
        eng = p.get("engine") if isinstance(p, dict) else None
        if eng not in ids:
            fails.append(f"PATCHES: the patch {p.get('id') if isinstance(p, dict) else p!r} is for {eng!r}, which is not an enrolled engine")

    for path, u in sorted(by_path.items()):
        if u.get("kind") != "test-reference":
            continue
        for f, text in sorted(texts.items()):
            product = posixpath.dirname(f) in enrolled_dirs or f in adapters
            if product and any(inc.startswith(path + "/") for inc in includes_of(f, text)):
                fails.append(f"REFERENCE: {f} includes a file from {path}, which is entered as a test reference: "
                             f"it is product code and must be enrolled")
    return sorted(set(fails))


# ---- must-fail controls of the static half -------------------------------------------

def clean_model():
    """A small conforming tree, independent of the real one: the must-read-zero."""
    texts = {
        "h2/engine/engine.h": "#pragma once\n",
        "h2/cores/ref/ref.h": "#pragma once\n",
        "h2/harness/harness.h": "#pragma once\n",
        "engine2/core/api/engine.h": "#pragma once\n// a stub: the engine is not built yet\n",
        "engine2/core/api/engine.cpp": '#include "engine.h"\n',
        "engine2/tests/runner.cpp": "int main() { return 0; }\n",
        "h2/harness/adapter_composed.h": '#include "../engine/engine.h"\n#include "harness.h"\n',
        TABLE: '#include "adapter_composed.h"\ninline constexpr AdapterRow kAdapters[] = {\n'
               '  {"composed", false, &makeComposedAdapter},\n  {"control/silent", true, &makeSilentControl},\n};\n',
    }
    return {
        "files": sorted(list(texts) + ["h2/README.md", "h2/harness/patches.json"] + sorted(TOP_LEVEL_SUPPORT)),
        "texts": texts,
        "registry": {"units": [
            {"path": "h2/engine", "kind": "engine", "id": "composed", "adapter": "h2/harness/adapter_composed.h"},
            {"path": "h2/cores/ref", "kind": "test-reference", "why_not_enrolled": "a test reference"},
            {"path": "h2/harness", "kind": "harness", "why_not_enrolled": "the harness"},
        ]},
        "patches": {"patches": [{"id": "p/loud", "engine": "composed", "expect": "loud"},
                                {"id": "p/silent", "engine": "composed", "expect": "silent"}]},
    }


def first_enrolled(model):
    units = model["registry"].get("units") if isinstance(model["registry"], dict) else None
    for u in units if isinstance(units, list) else []:
        if isinstance(u, dict) and u.get("kind") in ENROLLED_KINDS and u.get("id") and u.get("adapter"):
            return u
    return None


def add_file(m, rel, text):
    m["files"] = sorted(set(m["files"]) | {rel})
    m["texts"][rel] = text


def plants(model):
    """-> [(name, mutate, verdict, needle)] for a model that has an enrolled unit."""
    eng = first_enrolled(model)
    path, uid, adapter = eng["path"], eng["id"], eng["adapter"]
    src = next(f for f in sorted(model["texts"]) if posixpath.dirname(f) == path)

    def unit(m):
        return next(u for u in m["registry"]["units"] if u.get("path") == path)

    def new_dir(rel):
        return lambda m: add_file(m, rel, "#pragma once\n")

    def drop_entry(m):
        m["registry"]["units"] = [u for u in m["registry"]["units"] if u.get("path") != path]

    def relabel(m):
        unit(m).update(kind="harness", why_not_enrolled="relabelled")

    def bad_kind(m):
        unit(m)["kind"] = "experimental"

    def no_adapter(m):
        del m["texts"][adapter]
        m["files"] = [f for f in m["files"] if f != adapter]

    def hollow_adapter(m):
        m["texts"][adapter] = '#include "harness.h"\n'

    def no_row(m):
        m["texts"][TABLE] = "\n".join(l for l in m["texts"][TABLE].splitlines() if f'{{"{uid}", false' not in l) + "\n"

    def ghost_row(m):
        m["texts"][TABLE] += '  {"ghost", false, &makeGhost},\n'

    def no_patches(m):
        m["patches"]["patches"] = [p for p in m["patches"]["patches"] if p.get("engine") != uid]

    def no_loud(m):
        for p in m["patches"]["patches"]:
            if p.get("engine") == uid:
                p["expect"] = "silent"

    def ghost_patch(m):
        m["patches"]["patches"].append({"id": "ghost/patch", "engine": "ghost", "expect": "loud"})

    def stale(m):
        m["registry"]["units"].append({"path": "h2/gone", "kind": "test-reference", "why_not_enrolled": "planted"})

    def no_reason(m):
        next(u for u in m["registry"]["units"] if u.get("kind") in EXEMPT_KINDS)["why_not_enrolled"] = "  "

    def reference_included(m):
        add_file(m, "h2/cores/planted/planted.h", "#pragma once\n")
        m["registry"]["units"].append({"path": "h2/cores/planted", "kind": "test-reference", "why_not_enrolled": "planted"})
        up = "/".join([".."] * len(path.split("/")))
        m["texts"][src] += f'\n#include "{up}/h2/cores/planted/planted.h"\n'

    def e2_real_code(m):
        add_file(m, "engine2/core/planted/planted.cpp", "int planted() { return 1; }\n")

    return [
        ("real code in engine2/core, no entry", e2_real_code, "UNENROLLED", "engine2/core "),
        ("a code file in an unnamed engine2 directory", new_dir("engine2/newengine/newengine.h"), "STRAY", "engine2/newengine/newengine.h "),
        ("a code file directly in engine2/", new_dir("engine2/rogue.cpp"), "STRAY", "engine2/rogue.cpp "),
        ("a new engine directory at the top level", new_dir("h2/newengine/newengine.h"), "UNENROLLED", "h2/newengine "),
        ("a new code file directly in h2/", new_dir("h2/rogue_engine.h"), "TOPLEVEL", "h2/rogue_engine.h "),
        ("a new directory under cores/", new_dir("h2/cores/newcore/core.h"), "UNENROLLED", "h2/cores/newcore "),
        ("a new module two levels down", new_dir("h2/modules/filter/filter.cpp"), "UNENROLLED", "h2/modules/filter "),
        ("the engine's entry deleted", drop_entry, "UNENROLLED", path + " "),
        ("the engine relabelled `harness`", relabel, "SHAPE", "only " + HARNESS_DIR),
        ("the engine's kind unknown", bad_kind, "SHAPE", "has kind"),
        ("the adapter file missing", no_adapter, "ADAPTER", "does not exist"),
        ("the adapter including nothing of the engine", hollow_adapter, "ADAPTER", "includes no file from"),
        ("the engine's table row gone", no_row, "TABLE", "no engine row"),
        ("a table row no entry owns", ghost_row, "TABLE", "'ghost'"),
        ("the engine's reference patches gone", no_patches, "PATCHES", "no reference patch"),
        ("no patch expected to be loud", no_loud, "PATCHES", "expects `loud`"),
        ("a patch for an unknown engine", ghost_patch, "PATCHES", "ghost/patch"),
        ("a stale entry", stale, "STALE", "h2/gone"),
        ("an exemption with no reason", no_reason, "SHAPE", "why_not_enrolled"),
        ("the engine including a test reference", reference_included, "REFERENCE", "h2/cores/planted"),
    ]


def discovery_control():
    """-> problem string or None. Runs list_h2_files + units_of in a scratch repository."""
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    with tempfile.TemporaryDirectory(prefix="enrolment-") as tmp:
        root = pathlib.Path(tmp)
        for rel, text in (("h2/README.md", "x\n"), ("h2/staged/a.h", "x\n"), ("h2/fresh/deep/b.cpp", "x\n"),
                          ("h2/ignored/c.h", "x\n"), ("h2/prose/notes.md", "x\n"), (".gitignore", "h2/ignored/\n"),
                          ("engine2/core/api/a.cpp", "x\n"), ("engine2/core/fresh/b.h", "x\n"),
                          ("engine2/tests/t.cpp", "x\n"), ("engine2/data/x.json", "x\n")):
            (root / rel).parent.mkdir(parents=True, exist_ok=True)
            (root / rel).write_text(text)
        for cmd in (["git", "init", "-q"], ["git", "add", "h2/staged/a.h", "engine2/core/api/a.cpp"]):
            r = subprocess.run(cmd, cwd=tmp, capture_output=True, text=True, env=env)
            if r.returncode != 0:
                return f"the discovery control could not run `{' '.join(cmd)}` ({r.stderr.strip()[:120]})"
        files, err = list_h2_files(tmp)
        if err:
            return f"the discovery control: {err}"
        got, want = units_of(files), ["engine2/core", "h2/fresh/deep", "h2/staged"]
        if got != want:
            return (f"the discovery control found {got}, want {want} (a staged directory and a new one are units; "
                    f"an ignored one, one with no code, and h2/ itself are not; engine2/core is one unit, tracked or new)")
        e2 = sorted(f for f in files if f.startswith("engine2/"))
        want_e2 = ["engine2/core/api/a.cpp", "engine2/core/fresh/b.h", "engine2/data/x.json", "engine2/tests/t.cpp"]
        if e2 != want_e2:
            return f"the discovery control listed {e2} under engine2/, want {want_e2} (staged and new files are both seen)"
    return None


def controls(model):
    """-> (problems, fired, the trees planted in). A non-empty `problems` means this
    check cannot be trusted."""
    problems, fired = [], 0
    zero = judge(clean_model())
    if zero:
        problems.append(f"must-read-zero: the conforming synthetic tree read red: {zero[0]}")
    bases = [("synthetic", clean_model())]
    # The real tree too, when it is whole enough to plant in: the judge must catch
    # each fault in the files as they really are (the real table's row format, the
    # real adapter's include), not only in a tree written to suit it.
    eng = first_enrolled(model)
    if (eng and TABLE in model["texts"] and eng["adapter"] in model["texts"]
            and isinstance(model["patches"], dict) and isinstance(model["patches"].get("patches"), list)
            and any(posixpath.dirname(f) == eng["path"] for f in model["texts"])):
        bases.append(("real", model))
    for base_name, base in bases:
        for name, mutate, verdict, needle in plants(base):
            m = copy.deepcopy(base)
            mutate(m)
            if m == base:
                problems.append(f"control [{base_name}] {name}: the plant changed nothing (its anchor is gone)")
                continue
            if any(f.startswith(verdict + ":") and needle in f for f in judge(m)):
                fired += 1
            else:
                problems.append(f"control [{base_name}] {name}: did NOT read {verdict} (looked for {needle!r})")
    # QUIET controls: what must NOT turn the check red. A judge that is red for every engine2 change would
    # pass every plant above and make the stub-only allowance a lie.
    quiet = [("a new stub header in engine2/core", lambda m: add_file(m, "engine2/core/newconcept/newconcept.h",
                                                                       "#pragma once\n// Pan and density.\n#include \"x.h\"\n")),
             ("a code file under engine2/tests", lambda m: add_file(m, "engine2/tests/items/item_x.cpp", "int x() { return 1; }\n"))]
    for base_name, base in bases:
        for name, mutate in quiet:
            m = copy.deepcopy(base)
            mutate(m)
            if m == base:
                problems.append(f"quiet control [{base_name}] {name}: the change did nothing (its anchor is gone)")
            elif judge(m) != judge(base):
                problems.append(f"quiet control [{base_name}] {name}: turned the check red: {sorted(set(judge(m)) - set(judge(base)))[0]}")
            else:
                fired += 1
    err = discovery_control()
    if err:
        problems.append(err)
    else:
        fired += 1
    return problems, fired, [b[0] for b in bases]


def main():
    model, err = load_model(ROOT)
    if err:
        print(f"engine_enrolment_check: FAILED -- {err}", file=sys.stderr)
        return 1
    problems, fired, bases = controls(model)
    if problems:
        print(f"engine_enrolment_check: FAILED (the check itself is broken) -- {len(problems)} control(s) misbehaved:", file=sys.stderr)
        for p in problems:
            print("    " + p, file=sys.stderr)
        return 1
    fails = judge(model)
    if fails:
        print(f"engine_enrolment_check: FAILED -- {len(fails)} problem(s):", file=sys.stderr)
        for f in fails:
            print("    " + f, file=sys.stderr)
        return 1
    units = model["registry"]["units"]
    core = core_code(model["files"])
    e2_state = (f"{E2_CORE} enrolled" if any(u["path"] == E2_CORE for u in units) else
                f"{E2_CORE} PENDING: {len(core)} stub files, no engine code yet; the first real line there turns this red until it is enrolled")
    enrolled = [f"{u['id']} ({u['path']})" for u in units if u["kind"] in ENROLLED_KINDS]
    exempt = [f"{u['path']} ({u['kind']})" for u in units if u["kind"] in EXEMPT_KINDS]
    print(f"engine_enrolment_check: GREEN ({e2_state}; {len(units)} units under h2/: {len(enrolled)} enrolled [{', '.join(enrolled)}], "
          f"{len(exempt)} not enrolled with a reason [{', '.join(exempt)}]; {len(model['patches']['patches'])} reference patches; "
          f"{fired} must-fail controls fired on the {' and the '.join(bases)} tree, the conforming tree read green. "
          f"STATIC: that each adapter makes sound is {SELFTEST}'s to show, in ./verify full)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
