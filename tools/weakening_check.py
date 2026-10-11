#!/usr/bin/env python3
"""weakening_check -- a ratchet on gate-weakening markers (B448 A2, ADR-197).

WIRED: ./verify fast

WHY. Armor's "quiet disabling" failure mode (docs/strategy/blind-spot-armor.md): a
test skipped, a warning suppressed, a sanitizer turned off "temporarily", a gate
declared UNWIRED, a failing step softened with `|| true` -- each makes a problem
look solved. No list of rules stops it; a counter does. Every marker below is
counted per (category, file) against docs/armor/weakening-baseline.json, and ANY
increase is red until the human approves it, recorded as a ref in that entry's
`approved` field. This check COUNTS and COMPARES; it never judges whether a marker
is justified. A decrease is good news and prints a note.

CATEGORIES (each pattern is here for a reason; patterns are per-line, case-sensitive
unless stated, and one combined regex per category so overlapping spellings count once):
  skip           A test or gate that no longer runs. GTEST_SKIP / DISABLED_ (gtest);
                 pytest.skip|skipif|xfail and unittest.skip* (python); it|test|describe
                 .skip (the .mjs oracles); `return 0; // skip` early-outs (case-insensitive);
                 and an echo/printf of SKIP/SKIPPED in `verify`, which is how a gate turns
                 into a pass (a gate that prints SKIPPED exits 0).
  suppress       A diagnostic muted instead of fixed. NOLINT*, NOSONAR, `# noqa`,
                 `# type: ignore`, `#pragma clang|GCC diagnostic ignored`, MSVC
                 `#pragma warning(disable`, and `-Wno-<warning>` (a *-Wno-error=* is also
                 counted: it demotes an error to a warning).
  sanitizer_off  A sanitizer blind or non-fatal. no_sanitize (also matches
                 __attribute__((no_sanitize...)), -fno-sanitize=..., -fsanitize-recover,
                 halt_on_error=0, suppressions=, detect_leaks=0. NOT counted:
                 -fno-sanitize-recover, the STRENGTHENING flag (it makes UBSan abort), so
                 it is excluded by lookahead. Env options are matched on their own spelling,
                 not only inside ASAN/TSAN/UBSAN_OPTIONS, because a YAML env block puts the
                 variable and its value on different lines.
  soft_fail      A failure that cannot fail the run. In `verify` and .github/workflows only:
                 `continue-on-error: true`, `|| true`, `|| :`, and `if: false`.
  unwired        A check declared not wired into verify: any `UNWIRED:` text. The
                 test_table_check rule (wired-or-explained) allows it, so this counter is
                 what makes the explanation visible and capped.
  dsp_guard      Symptom clamping (armor: "Symptom clamping"). In src/ and h2/ only:
                 std::isfinite, std::isnan, std::clamp, bare isfinite( and isnan(. C/C++
                 files are matched on code only (comments, strings blanked with
                 banned_api_check.strip_noncode) so prose never counts. An approval here
                 must cite a root cause: see DSP_GUARD APPROVALS.
  dsp_guard_tagged  The same markers, on a line that carries the text `dsp_guard: boundary`
                 or `dsp_guard: latch` (in a comment on the SAME physical line; a tag on
                 the line above tags nothing). A tagged marker is counted here INSTEAD of
                 under dsp_guard, never in neither, so a tag cannot lower the total.

DSP_GUARD APPROVALS (B448 P14; the law's "Symptom clamping" rule and its edit D).
  dsp_guard         --approve refuses unless REF contains `traces/<name>.md` (a file directly
                    under traces/; if REF names several, the first is the one checked), that
                    file is readable, it names FILE (the path, as text), and it has a
                    root-cause section: a markdown heading of any level
                    whose text starts with "Root cause" (any case; `## Root cause` or
                    `## Root cause: the wrap rounds to len`), followed by at least one
                    non-blank line before the next heading. "The guard hides X; X happens
                    because Y" is what belongs there.
  dsp_guard_tagged  --approve needs a non-empty REF like every other category, and no trace.
                    Edit D: input validation at a trust boundary is exempt from the
                    root-cause rule, and an output guard is allowed if it latches and
                    reports. --approve prints each tagged line, because the tag is the
                    author's claim and only the human's reading tests it.
  WHY A TAG IS NOT AN EXEMPTION. If a tag made a guard uncounted, an agent could add a clamp,
  write "boundary" beside it, and need nobody: the tag would quiet the ratchet. So a tag only
  chooses WHICH approval the human gives (one that needs no trace); it never removes the need
  for one. Entries already in the baseline are not re-judged; the rule acts on --approve only.
  LIMITS. The tool checks that a trace exists, names the file and has the section; it cannot
  tell whether the root cause is true, nor whether a `latch` guard really latches. A ledger
  edited by hand skips --approve and this rule with it (ledger_owner_check and code-owner
  review are the guard there).

SCOPE. Tracked files, and new files that are not ignored (`git ls-files --cached --others
--exclude-standard`): src/ h2/ tools/ tests/ verify
.github/workflows/ CMakeLists.txt cmake/. Excluded: libs/, reference/ (third-party and
spec-as-code), and this checker + its baseline (their own pattern text would count).
Binary files (a NUL byte) are skipped; a tracked file deleted in the working tree is
skipped; any other unreadable file is RED, and so is a scan that finds no file at all.
LIMIT: a marker assembled by macro, or a sanitizer option split across a line, is not seen.

USAGE.
  weakening_check.py                         verify mode (default): controls, then compare
  weakening_check.py --init                  write the baseline from this tree (refuses if one exists)
  weakening_check.py --tighten               lower baseline entries the tree has fallen below; never raises
  weakening_check.py --approve CAT FILE REF  raise ONE entry to the current count; REF (an ADR /
                                             ROADMAP / trace ref, non-empty) is recorded. For
                                             dsp_guard REF must cite a trace with a root-cause
                                             section (DSP_GUARD APPROVALS). The human's decision,
                                             never the agent's own.

MUST-FAIL CONTROLS, every run, in a scratch git repo with a copied baseline: one marker
planted per category reads red; the clean layout reads green; a new file with markers
counts as an increase from 0; --tighten never raises; --approve with an empty ref is
refused and writes nothing; --approve then clears the planted marker. For dsp_guard: a
planted guard approved with a bare ref, with a missing trace, with a file outside traces/,
with a trace that has no root-cause section, an empty one, or one that does not name the
file, is refused each time and writes nothing; the same guard citing a proper trace is
accepted and reads green. A tagged guard is still red until approved, is counted under
dsp_guard_tagged and not dsp_guard, and is approved without a trace; a tag on the line above
tags nothing. If any control fails to go red (or green where it must), this check is red.
No wall-clock values.
"""
import argparse
import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from banned_api_check import strip_noncode  # noqa: E402  (reuse the C/C++ lexer, not a copy)

BASELINE_REL = "docs/armor/weakening-baseline.json"
SELF_REL = "tools/weakening_check.py"
SCOPE = ("src", "h2", "tools", "tests", "verify", ".github/workflows", "CMakeLists.txt", "cmake")
EXCLUDE_PREFIXES = ("libs/", "reference/")
EXCLUDE_FILES = (SELF_REL, BASELINE_REL)
CPP_EXTS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".ipp", ".tpp", ".inc", ".m", ".mm"}

# (regex, restrict) per category; restrict is None (whole scope), or a tuple of path
# prefixes/names the category applies to.
VERIFY_AND_CI = ("verify", ".github/workflows/")
DSP_PATHS = ("src/", "h2/")
DSP_GUARD = re.compile(r"\bstd::(?:isfinite|isnan|clamp)\b|(?<![\w:.>])(?:isfinite|isnan)\s*\(")
# The tag that moves a marker from dsp_guard to dsp_guard_tagged. Same physical line only: a
# counter cannot tell which following line a tag on its own line was meant for.
DSP_TAG = re.compile(r"\bdsp_guard:\s*(?:boundary|latch)\b")
# A dsp_guard approval's REF must contain one of these. `[\w.-]+` has no slash, so the name
# cannot climb out of traces/ (traces/../x.md does not match).
TRACE_REF = re.compile(r"(?<![\w./-])traces/[\w.-]+\.md(?![\w-])")
ROOT_CAUSE_HEADING = re.compile(r"^#{1,6}[ \t]+root cause\b[^\n]*$", re.I | re.M)
ANY_HEADING = re.compile(r"^#{1,6}[ \t]+\S", re.M)
CATEGORIES = {
    "skip": (re.compile(
        r"GTEST_SKIP|\bDISABLED_[A-Za-z0-9_]"
        r"|\bpytest\.(?:mark\.)?(?:skip|skipif|xfail)\b|\bunittest\.skip\w*"
        r"|\b(?:it|test|describe)\.skip\b"
        r"|(?i:return\s+(?:0|true)\s*;\s*//\s*skip)"), None),
    "suppress": (re.compile(
        r"NOLINT|NOSONAR|#\s*noqa\b|#\s*type:\s*ignore\b"
        r"|#\s*pragma\s+(?:clang|GCC)\s+diagnostic\s+ignored"
        r"|#\s*pragma\s+warning\s*\(\s*disable"
        r"|(?<![\w-])-Wno-[A-Za-z]"), None),
    "sanitizer_off": (re.compile(
        r"no_sanitize|-fno-sanitize(?!-recover)|-fsanitize-recover"
        r"|halt_on_error\s*=\s*0|suppressions\s*=|detect_leaks\s*=\s*0"), None),
    "soft_fail": (re.compile(
        r"continue-on-error:\s*true\b|\|\|\s*true\b|\|\|\s*:(?=\s*(?:$|[;)&|#]))"
        r"|^\s*(?:-\s*)?if:\s*(?:false|\$\{\{\s*false\s*\}\})\s*$", re.M), VERIFY_AND_CI),
    "unwired": (re.compile(r"UNWIRED:"), None),
    "dsp_guard": (DSP_GUARD, DSP_PATHS),
    "dsp_guard_tagged": (DSP_GUARD, DSP_PATHS),     # the same markers, split by line tag in count_text
}
# `verify`-only: an echo/printf that announces a SKIP, per-line (a gate that prints SKIPPED exits 0).
VERIFY_SKIP_PRINT = re.compile(r"(?:echo|printf)\b[^\n]*\bSKIP(?:PED)?\b")


def in_restrict(rel, restrict):
    return restrict is None or any(rel == r or rel.startswith(r) for r in restrict)


def dsp_guard_markers(rel, text):
    """-> [(line_number, tagged)], one per dsp_guard marker in this file, in file order.

    C/C++ is matched on code only, but the tag lives in a comment, so the tag is looked up on
    the ORIGINAL line. strip_noncode blanks with spaces and keeps newlines, so a match offset
    in the stripped text falls on the same line of the original."""
    body = strip_noncode(text) if pathlib.PurePosixPath(rel).suffix in CPP_EXTS else text
    lines = text.split("\n")
    out = []
    for m in DSP_GUARD.finditer(body):
        ln = body.count("\n", 0, m.start())
        out.append((ln + 1, bool(DSP_TAG.search(lines[ln]))))
    return out


def count_text(rel, text):
    """-> {category: n} for one file's text (zero categories omitted)."""
    out = {}
    for cat, (rx, restrict) in CATEGORIES.items():
        if not in_restrict(rel, restrict):
            continue
        if cat == "dsp_guard_tagged":
            continue                                    # counted with dsp_guard, just below
        if cat == "dsp_guard":
            marks = dsp_guard_markers(rel, text)
            n = sum(1 for _, tagged in marks if not tagged)
            if len(marks) - n:
                out["dsp_guard_tagged"] = len(marks) - n
        else:
            n = len(rx.findall(text))
        if cat == "skip" and rel == "verify":
            n += len(VERIFY_SKIP_PRINT.findall(text))
        if n:
            out[cat] = n
    return out


def git_env():
    return {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}


def list_scope_files(cwd):
    # --others --exclude-standard is load-bearing: a new file that is not yet tracked and not
    # ignored is about to be committed. Reading tracked files only, a local verify run before
    # `git add` passed while CI, which sees the file committed, failed (PR #1048, 2026-10-11).
    r = subprocess.run(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard", "--", *SCOPE],
                       cwd=cwd, capture_output=True, text=True, env=git_env())
    if r.returncode != 0:
        return None, f"git ls-files failed (exit {r.returncode})"
    files = [f for f in r.stdout.split("\0") if f
             and not f.startswith(EXCLUDE_PREFIXES) and f not in EXCLUDE_FILES]
    return sorted(set(files)), None


def scan(cwd):
    """-> (counts {cat: {file: n}}, problems, n_files)."""
    files, err = list_scope_files(cwd)
    if err:
        return {}, [err], 0
    if not files:
        return {}, ["no tracked file found in scope -- a scan that saw nothing is blind, not clean"], 0
    counts, problems, seen = {}, [], 0
    for f in files:
        p = pathlib.Path(cwd) / f
        try:
            raw = p.read_bytes()
        except (FileNotFoundError, IsADirectoryError):
            continue                                    # deleted in the working tree / submodule
        except OSError as e:
            problems.append(f"{f}: unreadable ({type(e).__name__}) -- fail closed")
            continue
        seen += 1
        if b"\0" in raw[:8192]:
            continue                                    # binary: no text marker to see
        for cat, n in count_text(f, raw.decode("utf-8", errors="replace")).items():
            counts.setdefault(cat, {})[f] = n
    return counts, problems, seen


# ---- baseline: {category: {file: {"count": N, "approved": ref}}}, sorted, deterministic ----

INIT_REF = "baseline 2026-10-08"


def load_baseline(path):
    """-> (baseline, error). A missing or malformed baseline is RED, never an empty one."""
    try:
        data = json.loads(pathlib.Path(path).read_text(encoding="utf-8"))
    except FileNotFoundError:
        return None, f"{BASELINE_REL} is missing (create it once with --init)"
    except (OSError, ValueError) as e:
        return None, f"{BASELINE_REL} unreadable ({type(e).__name__})"
    if not isinstance(data, dict):
        return None, f"{BASELINE_REL}: top level is not an object"
    for cat, files in data.items():
        if cat not in CATEGORIES or not isinstance(files, dict):
            return None, f"{BASELINE_REL}: bad category {cat!r}"
        for f, e in files.items():
            if (not isinstance(e, dict) or not isinstance(e.get("count"), int) or e["count"] < 1
                    or not isinstance(e.get("approved"), str) or not e["approved"].strip()):
                return None, f"{BASELINE_REL}: bad entry {cat}/{f} (need count >= 1 and a non-empty approved ref)"
    return data, None


def dump_baseline(data):
    return json.dumps(data, indent=2, sort_keys=True) + "\n"


def compare(counts, baseline):
    """-> (increases, decreases), each [(cat, file, old, new)] sorted. A new file is an increase from 0."""
    inc, dec = [], []
    for cat in sorted(set(counts) | set(baseline)):
        cur, base = counts.get(cat, {}), baseline.get(cat, {})
        for f in sorted(set(cur) | set(base)):
            new, old = cur.get(f, 0), base.get(f, {}).get("count", 0)
            if new > old:
                inc.append((cat, f, old, new))
            elif new < old:
                dec.append((cat, f, old, new))
    return inc, dec


def tightened(counts, baseline):
    """Baseline lowered to the tree where the tree is lower. Never raises, never adds an entry."""
    out = {}
    for cat, files in baseline.items():
        for f, e in files.items():
            new = counts.get(cat, {}).get(f, 0)
            if new >= 1:
                out.setdefault(cat, {})[f] = {"count": min(new, e["count"]), "approved": e["approved"]}
    return out


TAG_WORDS = "'dsp_guard: boundary' or 'dsp_guard: latch'"


def root_cause_trace(root, f, ref):
    """-> (why, error) for a dsp_guard approval of file f: the rule in DSP_GUARD APPROVALS.

    Mechanical on purpose: a trace that exists, names the file and has a non-empty root-cause
    section. Whether the cause is TRUE is the human's reading, not this function's."""
    names = TRACE_REF.findall(ref)
    if not names:
        return None, (f"refused: a dsp_guard approval must cite its root cause. REF must contain "
                      f"traces/<name>.md, a trace with a '## Root cause' section that names {f}. "
                      f"(If the guard is input validation at a trust boundary, or an output guard "
                      f"that latches and reports, tag its line {TAG_WORDS} instead: it is then "
                      f"counted, and approved, as dsp_guard_tagged, which needs no trace.)")
    rel = names[0]
    try:
        text = (pathlib.Path(root) / rel).read_text(encoding="utf-8")
    except (OSError, ValueError) as e:
        return None, f"refused: {rel} cannot be read ({type(e).__name__}) -- a dsp_guard approval cites a trace that exists"
    head = ROOT_CAUSE_HEADING.search(text)
    if not head:
        return None, f"refused: {rel} has no root-cause section (a heading that starts 'Root cause', e.g. '## Root cause')"
    rest = text[head.end():]
    nxt = ANY_HEADING.search(rest)
    if not (rest[:nxt.start()] if nxt else rest).strip():
        return None, f"refused: the root-cause section of {rel} is empty"
    if f not in text:
        return None, f"refused: {rel} does not name {f} -- a trace about another file explains nothing here"
    return f"{rel} names {f} and has a root-cause section (read it: the tool cannot tell whether the cause is true)", None


def approved(counts, baseline, cat, f, ref, root=ROOT):
    """-> (new_baseline, error, why). Raises exactly one entry to the current count.

    `why` is the plain-words reason a dsp_guard / dsp_guard_tagged approval was accepted (None
    for the other categories); `root` is where a cited trace is looked up."""
    if not ref or not ref.strip():
        return None, "refused: --approve needs a non-empty ref (an ADR / ROADMAP / trace id the human gave)", None
    if cat not in CATEGORIES:
        return None, f"refused: unknown category {cat!r} (one of {', '.join(sorted(CATEGORIES))})", None
    new = counts.get(cat, {}).get(f, 0)
    old = baseline.get(cat, {}).get(f, {}).get("count", 0)
    if new <= old:
        return None, f"refused: {cat} {f} is {new}, baseline {old} -- nothing to approve (--tighten lowers)", None
    why = None
    if cat == "dsp_guard":
        why, err = root_cause_trace(root, f, ref)
        if err:
            return None, err, None
    elif cat == "dsp_guard_tagged":
        why = (f"every marker counted here sits on a line tagged {TAG_WORDS}, so no trace is needed "
               f"(the tag is the author's claim: read the lines)")
    out = json.loads(json.dumps(baseline))
    out.setdefault(cat, {})[f] = {"count": new, "approved": ref.strip()}
    return out, None, why


def increase_message(cat, f, old, new):
    msg = (f"{f}: {cat} {old} -> {new}. An increase is a gate-weakening event: it needs the "
           f"human's approval, recorded as a ROADMAP/ADR/trace ref in the baseline's \"approved\" "
           f"field ({SELF_REL} --approve {cat} {f} <ref>)")
    if cat == "dsp_guard":
        msg += (f". For dsp_guard the ref must cite traces/<name>.md with a '## Root cause' section "
                f"that names the file; a boundary check or a latching output guard is tagged "
                f"{TAG_WORDS} on its line instead, and still needs the human's approval")
    elif cat == "dsp_guard_tagged":
        msg += ". A tagged guard needs no trace, but it is counted and it needs the human's approval like any other"
    return msg


# ---- must-fail controls, in a scratch git repo ----

LAYOUT = {
    "src/a.cpp": "int f() { return 1; }\n",
    "h2/b.h": "#pragma once\n",
    "tools/t.py": "print(1)\n",
    "tests/x.txt": "x\n",
    "verify": "#!/bin/sh\necho ok\n",
    ".github/workflows/ci.yml": "name: ci\n",
    "CMakeLists.txt": "project(x)\n",
    "cmake/x.cmake": "# x\n",
}
# category -> (file, appended text): one marker each, must read exactly (cat, file, 0, 1)
PLANTS = {
    "skip": ("tools/t.py", "@pytest.mark.skip\n"),
    "suppress": ("src/a.cpp", "int x;  // NOLINT\n"),
    "sanitizer_off": ("tools/t.py", "# -fno-sanitize=undefined\n"),
    "soft_fail": ("verify", "false || true\n"),
    "unwired": ("tools/t.py", "# UNWIRED: not yet\n"),
    "dsp_guard": ("src/a.cpp", "bool g(float v) { return std::isfinite(v); }\n"),
    # exactly (dsp_guard_tagged, 0, 1) also proves the tag did not ALSO leave it in dsp_guard,
    # and that a tagged guard is still an increase: a tag never reads green on its own
    "dsp_guard_tagged": ("h2/b.h", "inline bool gb(float v) { return std::isfinite(v); }  // dsp_guard: boundary\n"),
}
# text that must count as nothing: scope, strengthening-flag and comment/prose traps
MUST_READ_ZERO = {
    "the strengthening flag -fno-sanitize-recover": ("tools/t.py", "# -fno-sanitize-recover=all\n"),
    "isfinite in a C++ comment": ("src/a.cpp", "// std::isfinite(v) is NOT used here\n"),
    "isfinite in a C++ string": ("src/a.cpp", 'const char* s = "std::isnan(";\n'),
    "isfinite outside src/ and h2/": ("tools/t.py", "# std::isfinite(v)\n"),
    "|| true outside verify and workflows": ("tools/t.py", "x = 'a || true'\n"),
    "-Wno- inside an identifier": ("src/a.cpp", "int a-Wno-b = 0;\n"),
    "a member isnan(": ("src/a.cpp", "int y = obj.isnan(1);\n"),
    "a dsp_guard tag on a line with no guard": ("src/a.cpp", "int z = 0;  // dsp_guard: boundary\n"),
    "a tagged guard outside src/ and h2/": ("tools/t.py", "# std::isnan(v)  dsp_guard: latch\n"),
}


def _git(td, *args):
    return subprocess.run(["git", *args], cwd=td, capture_output=True, env=git_env()).returncode == 0


def selftest():
    with tempfile.TemporaryDirectory() as tmp:
        td = pathlib.Path(tmp)
        for rel, body in LAYOUT.items():
            (td / rel).parent.mkdir(parents=True, exist_ok=True)
            (td / rel).write_text(body)
        if not (_git(td, "init", "-q") and _git(td, "add", "--", *LAYOUT)):
            return "selftest: could not create a scratch repo"
        bpath = td / BASELINE_REL
        bpath.parent.mkdir(parents=True, exist_ok=True)

        def base():
            return load_baseline(bpath)[0]

        def verdict():
            return compare(scan(td)[0], base())

        counts, probs, _ = scan(td)
        if probs or counts:
            return f"selftest: the clean scratch layout read dirty ({probs}, {counts})"
        bpath.write_text(dump_baseline({}))             # the "copied baseline": empty, for a clean tree
        if verdict() != ([], []):
            return "selftest: the clean scratch layout did not read green"

        def with_text(rel, extra):
            p = td / rel
            orig = p.read_text()
            p.write_text(orig + extra)
            try:
                return verdict()[0]
            finally:
                p.write_text(orig)

        for cat, (rel, extra) in PLANTS.items():
            got = with_text(rel, extra)
            if got != [(cat, rel, 0, 1)]:
                return f"selftest: a planted {cat} marker in {rel} did NOT read red as exactly 0 -> 1 (got {got})"
        for what, (rel, extra) in MUST_READ_ZERO.items():
            got = with_text(rel, extra)
            if got:
                return f"selftest: {what} was counted ({got}) and must read zero"

        # a NEW tracked file carrying a marker is an increase from 0, not exempt as 'new'
        (td / "tools/new_check.py").write_text("# UNWIRED: x\n")
        _git(td, "add", "--", "tools/new_check.py")
        if verdict()[0] != [("unwired", "tools/new_check.py", 0, 1)]:
            return "selftest: a new file carrying a marker did not read as an increase from 0"
        _git(td, "rm", "-q", "-f", "--", "tools/new_check.py")

        # the same file NOT yet tracked is still an increase; an ignored one is not seen
        (td / "tools/untracked_check.py").write_text("# UNWIRED: x\n")
        if verdict()[0] != [("unwired", "tools/untracked_check.py", 0, 1)]:
            return "selftest: an untracked new file carrying a marker did not read as an increase from 0"
        (td / "tools/untracked_check.py").unlink()
        (td / ".gitignore").write_text("tools/ignored_check.py\n")
        (td / "tools/ignored_check.py").write_text("# UNWIRED: x\n")
        if verdict()[0]:
            return "selftest: an ignored file was counted and must read zero"
        (td / "tools/ignored_check.py").unlink()
        (td / ".gitignore").unlink()

        # a tag on the line ABOVE tags nothing: the guard stays in dsp_guard, where it needs a trace
        got = with_text("src/a.cpp", "// dsp_guard: boundary\nbool g2(float v) { return std::isnan(v); }\n")
        if got != [("dsp_guard", "src/a.cpp", 0, 1)]:
            return f"selftest: a tag on the line above a guard was honoured (got {got}); only a same-line tag counts"
        got = with_text("h2/b.h", "inline bool gl(float v) { return std::isnan(v); }  // dsp_guard: latch\n")
        if got != [("dsp_guard_tagged", "h2/b.h", 0, 1)]:
            return f"selftest: a guard tagged 'latch' did not read as exactly one dsp_guard_tagged increase (got {got})"

        # dsp_guard --approve: the root-cause rule. One planted guard, six refs that must each be
        # refused and write nothing, then the one that must be accepted.
        rel, extra = PLANTS["dsp_guard"]
        (td / rel).write_text(LAYOUT[rel] + extra)
        counts = scan(td)[0]
        cause = f"# t\n\n## Root cause\n\nThe guard in {rel} hides a divide by zero at note-on.\n\n## Next\n"
        scratch_traces = {
            "traces/good.md": cause,
            "traces/no-cause.md": f"# t\n\n{rel}: added a guard. No cause was looked for.\n",
            "traces/empty-cause.md": f"# t\n\n{rel}\n\n## Root cause\n\n## Next\n\ntext\n",
            "traces/other-file.md": cause.replace(rel, "src/elsewhere.cpp"),
            "docs/outside.md": cause,
        }
        for trel, body in scratch_traces.items():
            (td / trel).parent.mkdir(parents=True, exist_ok=True)
            (td / trel).write_text(body)
        before = bpath.read_bytes()
        # what -> (ref, words the refusal must contain): each row must be refused for ITS reason,
        # or a row could pass because a different rule happened to fire first
        refused = {
            "a bare ref": ("ADR-TEST", "must cite its root cause"),
            "a trace that does not exist": ("traces/absent.md", "cannot be read"),
            "a trace with no root-cause section": ("traces/no-cause.md", "has no root-cause section"),
            "a trace whose root-cause section is empty": ("traces/empty-cause.md", "is empty"),
            "a trace that names another file": ("traces/other-file.md", "does not name"),
            "a file outside traces/": ("docs/outside.md", "must cite its root cause"),
            "a path that climbs out of traces/": ("traces/../docs/outside.md", "must cite its root cause"),
        }
        for what, (ref, words) in refused.items():
            out, err, _ = approved(counts, base(), "dsp_guard", rel, ref, root=td)
            if out is not None or not err or words not in err:
                return f"selftest: a dsp_guard --approve citing {what} ({ref}) was NOT refused for that reason (got {err!r})"
        if bpath.read_bytes() != before:
            return "selftest: a refused dsp_guard --approve changed the baseline"
        out, err, why = approved(counts, base(), "dsp_guard", rel, "ADR-TEST, traces/good.md", root=td)
        if err or not why or out != {"dsp_guard": {rel: {"count": 1, "approved": "ADR-TEST, traces/good.md"}}}:
            return f"selftest: a dsp_guard --approve citing a proper trace was not accepted ({err}, {out})"
        bpath.write_text(dump_baseline(out))
        if verdict() != ([], []):
            return "selftest: a dsp_guard marker approved with a root-cause trace still read red"
        bpath.write_bytes(before)
        (td / rel).write_text(LAYOUT[rel])

        # a TAGGED guard: red until approved; approved with a bare ref and no trace; and the
        # tag buys nothing for dsp_guard itself (there is no dsp_guard increase to approve)
        rel, extra = PLANTS["dsp_guard_tagged"]
        (td / rel).write_text(LAYOUT[rel] + extra)
        counts = scan(td)[0]
        if verdict()[0] != [("dsp_guard_tagged", rel, 0, 1)]:
            return "selftest: a tagged guard read green before any approval (a tag must never approve itself)"
        out, err, _ = approved(counts, base(), "dsp_guard", rel, "traces/good.md", root=td)
        if out is not None or not err:
            return "selftest: a tagged guard was approvable under dsp_guard, where it is not counted"
        out, err, why = approved(counts, base(), "dsp_guard_tagged", rel, "ADR-TEST", root=td)
        if err or not why or out != {"dsp_guard_tagged": {rel: {"count": 1, "approved": "ADR-TEST"}}}:
            return f"selftest: a tagged guard was not approved with a bare ref ({err}, {out})"
        bpath.write_text(dump_baseline(out))
        if verdict() != ([], []):
            return "selftest: an approved tagged guard still read red"
        # ... and an UNTAGGED guard beside the approved tagged one is still red, as dsp_guard
        (td / rel).write_text(LAYOUT[rel] + extra + "inline bool gu(float v) { return std::isnan(v); }\n")
        if verdict()[0] != [("dsp_guard", rel, 0, 1)]:
            return "selftest: an untagged guard beside an approved tagged one did not read red as dsp_guard"
        bpath.write_bytes(before)
        (td / rel).write_text(LAYOUT[rel])
        for trel in scratch_traces:
            (td / trel).unlink()

        # --approve: an empty or blank ref is refused and writes nothing; a real ref raises one entry
        rel, extra = PLANTS["unwired"]
        (td / rel).write_text(LAYOUT[rel] + extra)
        counts = scan(td)[0]
        before = bpath.read_bytes()
        for ref in ("", "   ", None):
            out, err, _ = approved(counts, base(), "unwired", rel, ref)
            if out is not None or not err:
                return f"selftest: --approve with ref {ref!r} was not refused"
        if bpath.read_bytes() != before:
            return "selftest: a refused --approve changed the baseline"
        out, err, _ = approved(counts, base(), "unwired", rel, "ADR-TEST")
        if err or out != {"unwired": {rel: {"count": 1, "approved": "ADR-TEST"}}}:
            return f"selftest: --approve with a ref did not raise exactly one entry ({err}, {out})"
        bpath.write_text(dump_baseline(out))
        if verdict() != ([], []):
            return "selftest: an approved marker still read red"

        # --tighten never raises: with the tree ABOVE the baseline it changes nothing ...
        (td / rel).write_text(LAYOUT[rel] + extra + extra)          # 2 markers vs baseline 1
        counts = scan(td)[0]
        if tightened(counts, base()) != base():
            return "selftest: --tighten changed a baseline the tree is ABOVE (it must never raise)"
        if not verdict()[0]:
            return "selftest: a second marker over an approved one did not read red"
        # ... and with the tree BELOW it lowers, down to removal at zero
        (td / rel).write_text(LAYOUT[rel])
        counts = scan(td)[0]
        if verdict() != ([], [("unwired", rel, 1, 0)]):
            return f"selftest: a fixed marker did not read as a decrease only ({verdict()})"
        if tightened(counts, base()) != {}:
            return "selftest: --tighten did not drop an entry the tree has fallen to zero on"

        # a hand-damaged or missing baseline is red, never read as empty
        bpath.write_text('{"skip": {"x": {"count": 0, "approved": ""}}}')
        if load_baseline(bpath)[1] is None:
            return "selftest: a malformed baseline entry was accepted"
        bpath.unlink()
        if load_baseline(bpath)[1] is None:
            return "selftest: a missing baseline was accepted"
    return None


# ---- CLI ----

def _write(text_path, data):
    p = pathlib.Path(text_path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(dump_baseline(data), encoding="utf-8")


def main(argv=None):
    ap = argparse.ArgumentParser(description="gate-weakening marker ratchet (B448 A2)")
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--init", action="store_true", help="write the baseline from this tree (refuses if one exists)")
    mode.add_argument("--tighten", action="store_true", help="lower baseline entries; never raises")
    mode.add_argument("--approve", nargs=3, metavar=("CATEGORY", "FILE", "REF"),
                      help="raise one entry to the current count; REF must be non-empty")
    args = ap.parse_args(argv)
    bpath = ROOT / BASELINE_REL

    if args.approve:                      # refuse the empty ref before any scan: cheapest, clearest
        if not args.approve[2].strip():
            print("weakening_check: refused: --approve needs a non-empty ref", file=sys.stderr)
            return 1

    bad = selftest()
    if bad:
        print(f"weakening_check: FAILED -- {bad}", file=sys.stderr)
        return 1
    counts, problems, _ = scan(ROOT)
    if problems:
        print("weakening_check: FAILED -- the scan is not trustworthy:", file=sys.stderr)
        for p in problems:
            print(f"  {p}", file=sys.stderr)
        return 1

    if args.init:
        if bpath.exists():
            print(f"weakening_check: refused: {BASELINE_REL} exists (--approve raises, --tighten lowers)", file=sys.stderr)
            return 1
        _write(bpath, {c: {f: {"count": n, "approved": INIT_REF} for f, n in sorted(fs.items())}
                       for c, fs in sorted(counts.items())})
        print(f"weakening: baseline written ({sum(n for fs in counts.values() for n in fs.values())} markers)")
        return 0

    baseline, err = load_baseline(bpath)
    if err:
        print(f"weakening_check: FAILED -- {err}", file=sys.stderr)
        return 1

    if args.approve:
        cat, f, ref = args.approve
        new, err, why = approved(counts, baseline, cat, f, ref)
        if err:
            print(f"weakening_check: {err}", file=sys.stderr)
            return 1
        old = baseline.get(cat, {}).get(f, {}).get("count", 0)
        print(f"weakening_check: APPROVING (the human's call only): this raises the allowed '{cat}' "
              f"weakening markers in {f} from {old} to {new[cat][f]['count']}, so the extra "
              f"marker(s) stop reading red; ref {ref.strip()}. Writing {BASELINE_REL}.")
        if why:
            print(f"weakening_check: accepted because {why}.")
        if cat == "dsp_guard_tagged":                   # show the claim the human is approving
            src = (ROOT / f).read_text(encoding="utf-8", errors="replace")
            lines = src.split("\n")
            for ln in sorted({ln for ln, tagged in dsp_guard_markers(f, src) if tagged}):
                print(f"  {f}:{ln}: {lines[ln - 1].strip()}")
        _write(bpath, new)
        print(f"weakening: approved {cat} {f} ->{new[cat][f]['count']} ({ref.strip()})")
        return 0

    if args.tighten:
        _write(bpath, tightened(counts, baseline))
        baseline = load_baseline(bpath)[0]

    inc, dec = compare(counts, baseline)
    for cat, f, old, new in dec:
        print(f"note: {cat} {f}: {old} -> {new} (a decrease; --tighten locks it in)")
    if inc:
        print("weakening_check: FAILED -- gate-weakening markers rose above the baseline:", file=sys.stderr)
        for row in inc:
            print(f"  {increase_message(*row)}", file=sys.stderr)
        return 1
    total = sum(n for fs in counts.values() for n in fs.values())
    print(f"weakening: {total} markers across {len(counts)} categories, 0 increases")
    return 0


if __name__ == "__main__":
    sys.exit(main())
