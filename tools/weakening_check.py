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
  dsp_guard_tagged  The same markers, on a line that carries the tag `dsp_guard: boundary` or
                 `dsp_guard: latch`. Only those two spellings are tags: `dsp_guard: fine` is
                 not one, and neither is the bare word "boundary" in a comment. In C/C++ the
                 tag counts only inside a comment that OPENS on the marker's own physical line
                 (`// ...` or `/* ... */`). A tag in a string literal, in code, on the line
                 above or below, or in a block comment opened on an earlier line tags nothing.
                 Other file kinds under src/ and h2/ have no lexer here, so there the tag
                 counts anywhere on the marker's line. A tagged marker is counted here INSTEAD
                 of under dsp_guard, never in neither, so a tag cannot lower the total.

DSP_GUARD APPROVALS (B448 P14; the law's "Symptom clamping" rule and its edit D).
  dsp_guard         --approve refuses unless REF contains `traces/<name>.md` (a file directly
                    under traces/; if REF names several, the first is the one checked), that
                    file is readable, it names FILE (the whole path: `src/x.hpp` or
                    `lib/src/x.h` does not name `src/x.h`), and it has a root-cause section:
                    a markdown heading of any level whose text starts with "Root cause" (any
                    case; `## Root cause` or `## Root cause: the wrap rounds to len`),
                    followed by at least one non-blank line before the next heading. The
                    words in running prose are not a heading, and neither is a `#` line
                    inside a fenced code block. "The guard hides X; X happens because Y" is
                    what belongs there.
  dsp_guard_tagged  --approve needs a non-empty REF like every other category, and no trace.
                    Edit D: input validation at a trust boundary is exempt from the
                    root-cause rule, and an output guard is allowed if it latches and
                    reports. The tag is the author's claim and only the human's reading
                    tests it, so --approve prints EVERY guard line in the file, tagged and
                    untagged, with both categories' counts (see RETAG OR NEW).
  WHY A TAG IS NOT AN EXEMPTION. If a tag made a guard uncounted, an agent could add a clamp,
  write "boundary" beside it, and need nobody: the tag would quiet the ratchet. So a tag only
  chooses WHICH approval the human gives (one that needs no trace); it never removes the need
  for one.
  IN THE ORACLE, NOT ONLY ON --approve. Every ordinary run re-applies the trace rule, against
  the tree, to each dsp_guard ledger entry whose `approved` text is not the initial-baseline
  text. So a reference written by hand, a trace that was never committed (red in CI, which
  sees only what is committed) and a trace deleted later are all red. Entries that carry the
  initial-baseline text are not re-judged.
  A MOVE LEAVES NO HEADROOM. Tagging an existing guard moves it from one category to the
  other, so the category it left would keep its old allowance and let that many NEW guards in
  unseen. An --approve in either dsp_guard category therefore also lowers the other
  category's entry for the same file to the tree's current count (removing it at 0), in the
  same write, and says so.
  RETAG OR NEW. One file; the ledger allows u0 untagged and t0 tagged; "d" is tree minus ledger.
    change made in one go         d untagged  d tagged  reads red as                 may hide
    1 k old guards retagged       -k          +k        dsp_guard_tagged +k          0
    2 k new tagged guards          0          +k        dsp_guard_tagged +k          min(k, u0)
    3 m new untagged guards       +m           0        dsp_guard +m (needs a trace) 0
    4 k retagged + m new untagged  m-k        +k        dsp_guard_tagged +k, and     min(k, m)
                                                        dsp_guard +(m-k) if m > k
    5 k retagged + j new tagged   -k          k+j       dsp_guard_tagged +(k+j)      min(j, u0-k)
  "may hide" is the most NEW UNTAGGED guards that the same two numbers could equally stand
  for: max(0, min(d tagged, u0) + min(d untagged, 0)). In row 4 that many really are hidden.
  Row 2 and row 4 with m = k produce the SAME two numbers from the same ledger, so no rule on
  counts can make one red and the other not: asking row 4 for a trace would ask every honest
  new tagged guard for one too, wherever the file already holds an untagged guard, and edit D
  exempts those. What the tool does instead: the red line and --approve both state the "may
  hide" number, and --approve prints every guard line so the human can read them all.
  LIMITS. The tool checks that a trace exists, names the file and has the section. It cannot
  tell whether the root cause is true (a section that says only "TBD" passes), whether the
  path leads through a symlink, or whether a `latch` guard really latches: the human reads the
  trace and the lines. It cannot tell a retagged old guard plus a new untagged clamp from an
  honest new tagged guard (RETAG OR NEW), and like any counter it cannot see one guard removed
  and another added in the same change. An entry that carries the initial-baseline text is
  not re-judged, so a count raised by hand under that text is not seen here
  (ledger_owner_check and code-owner review are the guard for hand edits).

SCOPE. Tracked files, and new files that are not ignored (`git ls-files --cached --others
--exclude-standard`): src/ h2/ engine2/ tools/ tests/ verify
.github/workflows/ CMakeLists.txt cmake/. (engine2/ joined in B535, repo audit 2026-10-21 H4:
the clean-room engine replaces h2/engine, and a skipped test or a muted warning there must
not be invisible. Every category sees all of engine2/, except dsp_guard and dsp_guard_tagged,
which see engine2/core/ only: the suite under engine2/tests asserts isfinite on outputs, which
is a test, not a guard on the signal, and the same split already holds for tests/ and tools/.)
Excluded: libs/, reference/ (third-party and
spec-as-code), and this checker + its baseline (their own pattern text would count).
Binary files (a NUL byte) are skipped; a tracked file deleted in the working tree is
skipped; any other unreadable file is RED, and so is a scan that finds no file at all.
LIMIT: a marker assembled by macro, or a sanitizer option split across a line, is not seen.

USAGE.
  weakening_check.py                         verify mode (default): controls, then compare, then
                                             the trace rule on the ledger's dsp_guard entries
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
with a trace that has no root-cause section, an empty one, the words only in prose, the
heading only inside a code fence, or one that names only a longer path, is refused each time
and writes nothing; the same guard citing a proper trace is accepted and reads green. A
tagged guard is still red until approved, is counted under dsp_guard_tagged and not
dsp_guard, and is approved without a trace. A tag on the line above or below, a tag word
that is not one of the two, the bare word "boundary", and a tag in a string or in code each
tag nothing. A ledger entry written by hand with a bare or dangling reference is red in the
ordinary check, and an initial-baseline entry is not re-judged. After a tagged approval the
untagged allowance is lowered, so new untagged guards read red from the lowered number (and
the mirror). The five RETAG OR NEW rows read as the table says, and --approve's printed
lines carry every guard line, both counts and the "may hide" number. If any control fails to
go red (or green where it must), this check is red. No wall-clock values.
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
SCOPE = ("src", "h2", "engine2", "tools", "tests", "verify", ".github/workflows", "CMakeLists.txt", "cmake")
EXCLUDE_PREFIXES = ("libs/", "reference/")
EXCLUDE_FILES = (SELF_REL, BASELINE_REL)
CPP_EXTS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".ipp", ".tpp", ".inc", ".m", ".mm"}

# (regex, restrict) per category; restrict is None (whole scope), or a tuple of path
# prefixes/names the category applies to.
VERIFY_AND_CI = ("verify", ".github/workflows/")
DSP_PATHS = ("src/", "h2/", "engine2/core/")
DSP_GUARD = re.compile(r"\bstd::(?:isfinite|isnan|clamp)\b|(?<![\w:.>])(?:isfinite|isnan)\s*\(")
# The tag that moves a marker from dsp_guard to dsp_guard_tagged. Same physical line only: a
# counter cannot tell which following line a tag on its own line was meant for.
DSP_TAG = re.compile(r"\bdsp_guard:\s*(?:boundary|latch)\b")
COMMENT_OPEN = re.compile(r"//|/\*")
# The two dsp_guard categories are one population split by the tag, so a guard can MOVE between
# them. Each names the other here; approved() uses it so a move leaves no headroom behind.
DSP_PAIR = {"dsp_guard": "dsp_guard_tagged", "dsp_guard_tagged": "dsp_guard"}
# A dsp_guard approval's REF must contain one of these. `[\w.-]+` has no slash, so the name
# cannot climb out of traces/ (traces/../x.md does not match).
TRACE_REF = re.compile(r"(?<![\w./-])traces/[\w.-]+\.md(?![\w-])")
# Column 0 on purpose: four spaces of indent is a markdown code block, and outside_fences()
# indents every fenced line so that a heading shown as an example is not taken for a section.
ROOT_CAUSE_HEADING = re.compile(r"^#{1,6}[ \t]+root cause\b[^\n]*$", re.I | re.M)
ANY_HEADING = re.compile(r"^#{1,6}[ \t]+\S", re.M)
FENCE = re.compile(r"^ {0,3}(```|~~~)")
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


def tag_in_comment(text, line_start, line):
    """True iff a tag on this C/C++ line sits in a comment that opens on the same line.

    strip_noncode blanks comments and strings alike, so it cannot say which of the two a tag
    is in. What it can say is whether the lexer is in code at an offset: a token appended
    there survives. So each `//` or `/*` before the tag is asked that question; one that is
    in code really opens a comment (one inside a string or an earlier comment does not), and
    the tag is in a comment if that comment reaches it. The comment's own words are never
    lexed as code, which matters: an apostrophe in "the host's value" would open a char
    literal and swallow the tag."""
    def in_code(at):
        return strip_noncode(text[:line_start + at] + "QQ").endswith("QQ")

    for tag in DSP_TAG.finditer(line):
        at = 0
        while True:
            m = COMMENT_OPEN.search(line, at, tag.start())
            if not m:
                break
            if not in_code(m.start()):
                at = m.start() + 1                      # +1, not +2: in `*//*` the next opener overlaps
                continue
            if m.group() == "//":
                return True                             # a line comment runs to the end of the line
            close = line.find("*/", m.end(), tag.start())
            if close < 0:
                return True                             # the block comment is still open at the tag
            at = close + 2
    return False


def dsp_guard_markers(rel, text):
    """-> [(line_number, tagged)], one per dsp_guard marker in this file, in file order.

    C/C++ is matched on code only, but the tag lives in a comment, so the tag is looked up on
    the ORIGINAL line. strip_noncode blanks with spaces and keeps newlines, so a match offset
    in the stripped text is the same offset, on the same line, of the original."""
    cpp = pathlib.PurePosixPath(rel).suffix in CPP_EXTS
    body = strip_noncode(text) if cpp else text
    out = []
    for m in DSP_GUARD.finditer(body):
        start = text.rfind("\n", 0, m.start()) + 1
        end = text.find("\n", m.start())
        line = text[start:len(text) if end < 0 else end]
        tagged = tag_in_comment(text, start, line) if cpp else bool(DSP_TAG.search(line))
        out.append((body.count("\n", 0, m.start()) + 1, tagged))
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


def outside_fences(text):
    """The trace with every fenced code line indented by four, so a heading shown inside a fence
    is no heading. Indented, not deleted: a root-cause section whose body is a code block is
    still a section with something in it."""
    out, fence = [], None
    for line in text.split("\n"):
        m = FENCE.match(line)
        if fence is None and m:
            fence = m.group(1)
        elif fence is None:
            out.append(line)
            continue
        elif m and m.group(1) == fence:                 # ``` closes ```, never ~~~
            fence = None
        out.append("    " + line)
    return "\n".join(out)


def root_cause_trace(root, f, ref):
    """-> None when ref meets the rule in DSP_GUARD APPROVALS for file f, else why it does not.

    Mechanical on purpose: a trace that exists, names the file and has a non-empty root-cause
    section. Whether the cause is TRUE is the human's reading, not this function's."""
    names = TRACE_REF.findall(ref)
    if not names:
        return (f"a dsp_guard approval must cite its root cause. REF must contain "
                f"traces/<name>.md, a trace with a '## Root cause' section that names {f}. "
                f"(If the guard is input validation at a trust boundary, or an output guard "
                f"that latches and reports, tag its line {TAG_WORDS} instead: it is then "
                f"counted, and approved, as dsp_guard_tagged, which needs no trace.)")
    rel = names[0]
    try:
        text = outside_fences((pathlib.Path(root) / rel).read_text(encoding="utf-8"))
    except (OSError, ValueError) as e:
        return f"{rel} cannot be read ({type(e).__name__}) -- a dsp_guard approval cites a trace that exists"
    head = ROOT_CAUSE_HEADING.search(text)
    if not head:
        return (f"{rel} has no root-cause section (a heading that starts 'Root cause', e.g. "
                f"'## Root cause'; the words in prose or inside a code fence are not one)")
    rest = text[head.end():]
    nxt = ANY_HEADING.search(rest)
    if not (rest[:nxt.start()] if nxt else rest).strip():
        return f"the root-cause section of {rel} is empty"
    # The whole path, not a substring of a longer one: src/x.hpp, src/x.h.bak and lib/src/x.h
    # do not name src/x.h. A sentence's full stop after the path is allowed.
    if not re.search(r"(?<![\w./-])" + re.escape(f) + r"(?![\w/-]|\.\w)", text):
        return f"{rel} does not name {f} -- a trace about another file explains nothing here"
    return None


def ledger_trace_problems(baseline, root=ROOT):
    """-> [message]: the trace rule, re-applied to the LEDGER on every ordinary run.

    Without this the rule lived only inside --approve, so a reference typed into the ledger by
    hand, or a trace deleted after its approval, was never looked at again. Entries that carry
    the initial-baseline text predate the rule and are not re-judged."""
    out = []
    for f, e in sorted(baseline.get("dsp_guard", {}).items()):
        if e["approved"] == INIT_REF:
            continue
        why_not = root_cause_trace(root, f, e["approved"])
        if why_not:
            out.append(f"{f}: the ledger's dsp_guard approval {e['approved']!r} does not stand: {why_not}. "
                       f"Restore the trace, or have the human correct the reference in {BASELINE_REL}")
    return out


def approved(counts, baseline, cat, f, ref, root=ROOT):
    """-> (new_baseline, error). Raises exactly one entry to the current count; `root` is where
    a cited trace is looked up.

    For the two dsp_guard categories it also LOWERS the other category's entry for the same
    file to the tree's count (see A MOVE LEAVES NO HEADROOM). The lowered entry keeps its own
    reference, as --tighten would."""
    if not ref or not ref.strip():
        return None, "refused: --approve needs a non-empty ref (an ADR / ROADMAP / trace id the human gave)"
    if cat not in CATEGORIES:
        return None, f"refused: unknown category {cat!r} (one of {', '.join(sorted(CATEGORIES))})"
    new = counts.get(cat, {}).get(f, 0)
    old = baseline.get(cat, {}).get(f, {}).get("count", 0)
    if new <= old:
        return None, f"refused: {cat} {f} is {new}, baseline {old} -- nothing to approve (--tighten lowers)"
    if cat == "dsp_guard":
        why_not = root_cause_trace(root, f, ref)
        if why_not:
            return None, f"refused: {why_not}"
    out = json.loads(json.dumps(baseline))
    out.setdefault(cat, {})[f] = {"count": new, "approved": ref.strip()}
    other = DSP_PAIR.get(cat)
    if other and f in out.get(other, {}):
        now = counts.get(other, {}).get(f, 0)
        if now == 0:
            del out[other][f]
            if not out[other]:
                del out[other]                          # as --tighten: no empty category left behind
        elif now < out[other][f]["count"]:
            out[other][f]["count"] = now
    return out, None


def may_hide(counts, baseline, f):
    """-> the most NEW UNTAGGED guards that f's two counts could equally stand for, hidden
    behind retagged old ones (the "may hide" column of RETAG OR NEW). 0 means the numbers
    allow no such reading: either no old untagged guard existed to retag, or the untagged
    count fell by as much as the tagged count rose."""
    def delta(cat):
        return counts.get(cat, {}).get(f, 0) - baseline.get(cat, {}).get(f, {}).get("count", 0)
    u0 = baseline.get("dsp_guard", {}).get(f, {}).get("count", 0)
    return max(0, min(delta("dsp_guard_tagged"), u0) + min(delta("dsp_guard"), 0))


def hide_warning(n, f):
    return (f"the tool cannot tell which guard is new: the same counts would also result from "
            f"tagging old guards in {f} while adding up to {n} NEW UNTAGGED guard(s), which "
            f"would need a trace. Read every guard line in the file, not only the tagged ones")


def approval_lines(cat, f, ref, counts, baseline, new, src):
    """-> the lines --approve prints before it writes; `src` is f's text. A function, not prints
    in main(), so the self-test reads exactly what the human reads."""
    def allowed(ledger, c):
        return ledger.get(c, {}).get(f, {}).get("count", 0)

    ref, old, n = ref.strip(), allowed(baseline, cat), allowed(new, cat)
    out = [f"weakening_check: APPROVING (the human's call only): this raises the allowed '{cat}' "
           f"weakening markers in {f} from {old} to {n}, so the extra marker(s) stop reading red; "
           f"ref {ref}. Writing {BASELINE_REL}."]
    if cat in DSP_PAIR and n - old > 1:
        # "newly tagged", not "new", for the tagged category: the tool cannot say they are new
        kind = "NEW" if cat == "dsp_guard" else "NEWLY TAGGED"
        out.append(f"weakening_check: *** APPROVING {n - old} {kind} GUARDS under ONE reference. "
                   f"Each is its own guard; the reference has to answer for every one of them. ***")
    if cat == "dsp_guard":
        out.append(f"weakening_check: accepted because {TRACE_REF.findall(ref)[0]} names {f} and has a "
                   f"root-cause section (read it: the tool cannot tell whether the cause is true).")
    other = DSP_PAIR.get(cat)
    if other and allowed(new, other) != allowed(baseline, other):
        out.append(f"weakening_check: also LOWERING the allowed '{other}' markers in {f} from "
                   f"{allowed(baseline, other)} to {allowed(new, other)}: the tree holds that many now, and "
                   f"the old allowance would let the difference back in as new guards nobody approved.")
    if cat == "dsp_guard_tagged":
        out.append(f"weakening_check: accepted with no trace because each marker counted here sits on a "
                   f"line tagged {TAG_WORDS}. The tag is the author's claim, not a finding.")
        for c in ("dsp_guard", "dsp_guard_tagged"):
            out.append(f"  {c}: ledger {allowed(baseline, c)} -> {allowed(new, c)}; "
                       f"the tree holds {counts.get(c, {}).get(f, 0)}")
        hidden = may_hide(counts, baseline, f)
        out.append("weakening_check: " + (hide_warning(hidden, f) if hidden else
                   f"by the counts alone no new untagged guard is hidden behind a retag in {f}, but a "
                   f"counter cannot see a swap and cannot tell which guard is new. Read every guard line") + ":")
        lines = src.split("\n")
        # dict(): the tag belongs to the line, so a line with two markers prints once
        for ln, tagged in sorted(dict(dsp_guard_markers(f, src)).items()):
            out.append(f"  {f}:{ln}: [{'tagged' if tagged else 'UNTAGGED'}] {lines[ln - 1].strip()}")
    if len(new) != len(baseline):
        out.append(f"weakening_check: the ledger now has {len(new)} categories, not {len(baseline)}. "
                   f"docs/armor/dashboard.html embeds that number and must be regenerated: "
                   f"python3 tools/armor_dashboard.py")
    return out


def increase_message(cat, f, old, new, hidden=0):
    """`hidden` is may_hide() for a dsp_guard_tagged row (0 for every other category)."""
    msg = (f"{f}: {cat} {old} -> {new}. An increase is a gate-weakening event: it needs the "
           f"human's approval, recorded as a ROADMAP/ADR/trace ref in the baseline's \"approved\" "
           f"field ({SELF_REL} --approve {cat} {f} <ref>)")
    if cat == "dsp_guard":
        msg += (f". For dsp_guard the ref must cite traces/<name>.md with a '## Root cause' section "
                f"that names the file; a boundary check or a latching output guard is tagged "
                f"{TAG_WORDS} on its line instead, and still needs the human's approval")
    elif cat == "dsp_guard_tagged":
        msg += ". A tagged guard needs no trace, but it is counted and it needs the human's approval like any other"
        if hidden:
            msg += ". Also, " + hide_warning(hidden, f)
    return msg


# ---- must-fail controls, in a scratch git repo ----

LAYOUT = {
    "src/a.cpp": "int f() { return 1; }\n",
    "h2/b.h": "#pragma once\n",
    "engine2/core/e.h": "#pragma once\n",
    "engine2/tests/t.cpp": "int t() { return 0; }\n",
    "engine2/CMakeLists.txt": "project(e)\n",
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
# B535: the same kinds of marker planted under engine2/, one per file kind that lives there. Each
# must read exactly (cat, file, 0, 1), through the same scan() as the verdict.
ENGINE2_PLANTS = {
    "skip": ("engine2/tests/t.cpp", "GTEST_SKIP();\n"),
    "suppress": ("engine2/core/e.h", "int x;  // NOLINT\n"),
    "sanitizer_off": ("engine2/CMakeLists.txt", "add_compile_options(-fno-sanitize=undefined)\n"),
    "unwired": ("engine2/tests/t.cpp", "// UNWIRED: not yet\n"),
    "dsp_guard": ("engine2/core/e.h", "inline bool g(float v) { return std::isfinite(v); }\n"),
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
    "isfinite in the engine2 suite (outside engine2/core/)": ("engine2/tests/t.cpp", "bool ok(float v) { return std::isfinite(v); }\n"),
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
        for cat, (rel, extra) in ENGINE2_PLANTS.items():
            got = with_text(rel, extra)
            if got != [(cat, rel, 0, 1)]:
                return f"selftest: a planted {cat} marker in {rel} (engine2/) did NOT read red as exactly 0 -> 1 (got {got})"
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

        # WHICH LINES A TAG TAGS. Each row is appended to src/a.cpp and must read as exactly these
        # increases: `plain` is one guard that needs a trace, `tagged` one that does not. Every
        # `plain` row is a way a tag could be honoured where nobody wrote one; "boundary" is
        # ordinary DSP vocabulary, so the bare-word row matters most.
        guard = "bool q(float v) { return std::isnan(v); }"
        plain, tagged = [("dsp_guard", "src/a.cpp", 0, 1)], [("dsp_guard_tagged", "src/a.cpp", 0, 1)]
        tag_rows = {
            "a tag on the line above a guard": (f"// dsp_guard: boundary\n{guard}\n", plain),
            "an untagged guard directly above a tagged one": (f"{guard}\n{guard}  // dsp_guard: boundary\n", plain + tagged),
            "a guard marked 'dsp_guard: fine'": (f"{guard}  // dsp_guard: fine\n", plain),
            "a guard whose comment only says 'boundary'": (f"{guard}  // wrap at the boundary\n", plain),
            "a tag in a string literal": (f'const char* n = "dsp_guard: boundary"; {guard}\n', plain),
            "a tag in a string that holds //": (f'const char* n = "// dsp_guard: latch"; {guard}\n', plain),
            "a tag in a string after a closed comment": (f'/* c */ const char* n = "dsp_guard: latch"; {guard}\n', plain),
            "a tag in a string after a // that sits inside an earlier block comment": (f'/* a\n // b */ const char* n = "dsp_guard: latch"; {guard}\n', plain),
            "a tag written as code": (f"dsp_guard: boundary = 0; {guard}\n", plain),
            "a tag in a block comment opened on the line above": (f"/* why\n dsp_guard: boundary */ {guard}\n", plain),
            "a guard tagged 'latch'": (f"{guard}  // dsp_guard: latch\n", tagged),
            "a tag in a block comment before the guard": (f"/* dsp_guard: latch */ {guard}\n", tagged),
            "a tag after an apostrophe in its comment": (f"{guard}  // the host's value, don't trust it. dsp_guard: boundary\n", tagged),
            "a tag in a comment after a string that holds //": (f'const char* u = "a//b"; {guard}  // dsp_guard: boundary\n', tagged),
            "a tag in a line comment after two closed block comments": (f"/* c */ {guard} /* d *//* e */ // dsp_guard: latch\n", tagged),
            "a tag in a block comment that opens as an earlier one closes": (f"/* a\n b *//* dsp_guard: latch */ {guard}\n", tagged),
        }
        for what, (extra, want) in tag_rows.items():
            got = with_text("src/a.cpp", extra)
            if got != want:
                return f"selftest: {what} read as {got} and must read as {want}"
        # not C/C++: no lexer, so the documented rule is the tag anywhere on the marker's line
        if count_text("src/notes.txt", "isnan(v)  dsp_guard: latch\n") != {"dsp_guard_tagged": 1}:
            return "selftest: a tagged guard in a non-C/C++ file under src/ did not read as dsp_guard_tagged"

        # dsp_guard --approve: the root-cause rule. One planted guard, the refs that must each be
        # refused and write nothing, then the ones that must be accepted.
        rel, extra = PLANTS["dsp_guard"]
        (td / rel).write_text(LAYOUT[rel] + extra)
        counts = scan(td)[0]
        cause = f"# t\n\n## Root cause\n\nThe guard in {rel} hides a divide by zero at note-on.\n\n## Next\n"
        scratch_traces = {
            "traces/good.md": cause,
            # the section body is only a code block: fenced lines are set aside as headings, not as content
            "traces/good-code-body.md": f"# t\n\n`{rel}`\n\n## Root cause\n\n```\nx / 0\n```\n",
            "traces/good-full-stop.md": f"# t\n\n## Root cause\n\nA divide by zero at note-on, in {rel}.\n",
            "traces/no-cause.md": f"# t\n\n{rel}: added a guard. No cause was looked for.\n",
            "traces/empty-cause.md": f"# t\n\n{rel}\n\n## Root cause\n\n## Next\n\ntext\n",
            "traces/prose-cause.md": f"# t\n\n{rel}: the root cause is a divide by zero.\nRoot cause: see above.\n",
            "traces/fenced-cause.md": f"# t\n\n{rel}\n\n```\n## Root cause\n\nan example of the heading, not a section\n```\n",
            "traces/other-file.md": cause.replace(rel, "src/elsewhere.cpp"),
            "traces/longer-path.md": cause.replace(rel, f"{rel}p and lib/{rel} and {rel}.bak"),
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
            "a trace with 'root cause' only in prose": ("traces/prose-cause.md", "has no root-cause section"),
            "a trace whose heading is inside a code fence": ("traces/fenced-cause.md", "has no root-cause section"),
            "a trace that names another file": ("traces/other-file.md", "does not name"),
            "a trace that names only longer paths": ("traces/longer-path.md", "does not name"),
            "a file outside traces/": ("docs/outside.md", "must cite its root cause"),
            "a path that climbs out of traces/": ("traces/../docs/outside.md", "must cite its root cause"),
        }
        for what, (ref, words) in refused.items():
            out, err = approved(counts, base(), "dsp_guard", rel, ref, root=td)
            if out is not None or not err or words not in err:
                return f"selftest: a dsp_guard --approve citing {what} ({ref}) was NOT refused for that reason (got {err!r})"
        if bpath.read_bytes() != before:
            return "selftest: a refused dsp_guard --approve changed the baseline"
        for ref in ("ADR-TEST, traces/good.md", "traces/good-code-body.md", "traces/good-full-stop.md"):
            out, err = approved(counts, base(), "dsp_guard", rel, ref, root=td)
            if err or out != {"dsp_guard": {rel: {"count": 1, "approved": ref}}}:
                return f"selftest: a dsp_guard --approve citing a proper trace ({ref}) was not accepted ({err}, {out})"
        bpath.write_text(dump_baseline(out))
        if verdict() != ([], []):
            return "selftest: a dsp_guard marker approved with a root-cause trace still read red"

        # THE SAME RULE IN THE ORDINARY CHECK (M1). The ledger is judged by what its reference
        # says today, not by how the entry got there: these are entries typed in by hand.
        def by_hand(ref, cat="dsp_guard"):
            return {cat: {rel: {"count": 1, "approved": ref}}}

        stale_rows = {
            "a bare reference": ("ADR-TEST", "must cite its root cause"),
            "a trace that does not exist": ("traces/absent.md", "cannot be read"),
            "a trace with no root-cause section": ("traces/no-cause.md", "has no root-cause section"),
        }
        for what, (ref, words) in stale_rows.items():
            got = ledger_trace_problems(by_hand(ref), root=td)
            if len(got) != 1 or words not in got[0] or rel not in got[0]:
                return f"selftest: a dsp_guard ledger entry citing {what} was NOT red for that reason in the ordinary check (got {got})"
        if ledger_trace_problems(by_hand("ADR-TEST, traces/good.md"), root=td):
            return "selftest: a dsp_guard ledger entry citing a proper trace read red in the ordinary check"
        if ledger_trace_problems(by_hand(INIT_REF), root=td):
            return "selftest: an initial-baseline dsp_guard entry was re-judged (it predates the rule and must be left alone)"
        if ledger_trace_problems(by_hand("ADR-TEST", cat="dsp_guard_tagged"), root=td):
            return "selftest: a dsp_guard_tagged ledger entry was asked for a trace (a tagged approval needs none)"
        (td / "traces/good.md").unlink()                # a trace deleted after its approval dangles
        if len(ledger_trace_problems(by_hand("ADR-TEST, traces/good.md"), root=td)) != 1:
            return "selftest: a dsp_guard ledger entry whose trace was deleted did not read red in the ordinary check"
        (td / "traces/good.md").write_text(cause)
        bpath.write_bytes(before)
        (td / rel).write_text(LAYOUT[rel])

        # a TAGGED guard: red until approved; approved with a bare ref and no trace; and the
        # tag buys nothing for dsp_guard itself (there is no dsp_guard increase to approve)
        rel, extra = PLANTS["dsp_guard_tagged"]
        (td / rel).write_text(LAYOUT[rel] + extra)
        counts = scan(td)[0]
        if verdict()[0] != [("dsp_guard_tagged", rel, 0, 1)]:
            return "selftest: a tagged guard read green before any approval (a tag must never approve itself)"
        out, err = approved(counts, base(), "dsp_guard", rel, "traces/good.md", root=td)
        if out is not None or not err:
            return "selftest: a tagged guard was approvable under dsp_guard, where it is not counted"
        out, err = approved(counts, base(), "dsp_guard_tagged", rel, "ADR-TEST", root=td)
        if err or out != {"dsp_guard_tagged": {rel: {"count": 1, "approved": "ADR-TEST"}}}:
            return f"selftest: a tagged guard was not approved with a bare ref ({err}, {out})"
        bpath.write_text(dump_baseline(out))
        if verdict() != ([], []):
            return "selftest: an approved tagged guard still read red"
        # ... and an UNTAGGED guard beside the approved tagged one is still red, as dsp_guard
        (td / rel).write_text(LAYOUT[rel] + extra + "inline bool gu(float v) { return std::isnan(v); }\n")
        if verdict()[0] != [("dsp_guard", rel, 0, 1)]:
            return "selftest: an untagged guard beside an approved tagged one did not read red as dsp_guard"
        bpath.write_bytes(before)

        # RETAG OR NEW: the docstring's table, one row per case, on h2/b.h. `two_old` is a ledger
        # that allows the two old untagged guards. Each row: (ledger, the file's guard lines,
        # increases, decreases, may_hide).
        old1 = "inline bool o1(float v) { return std::isnan(v); }"
        old2 = "inline bool o2(float v) { return std::isfinite(v); }"
        new1 = "inline float n1(float v) { return std::clamp(v, 0.f, 1.f); }"
        new2 = "inline float n2(float v) { return std::clamp(v, -1.f, 1.f); }"
        tag = "  // dsp_guard: boundary"
        D, T = "dsp_guard", "dsp_guard_tagged"
        two_old = {D: {rel: {"count": 2, "approved": INIT_REF}}}

        def reads(ledger, guard_lines):
            (td / rel).write_text(LAYOUT[rel] + "".join(line + "\n" for line in guard_lines))
            seen = scan(td)[0]
            return seen, compare(seen, ledger) + (may_hide(seen, ledger, rel),)

        # Rows 2 and 4 expect the SAME reading from the same ledger. That is the limit the
        # docstring states, pinned here on purpose: whoever finds a rule that tells them apart
        # changes row 4, and until then nobody can believe the tool already does.
        cases = {
            "1, one old guard retagged": (two_old, [old1 + tag, old2], ([(T, rel, 0, 1)], [(D, rel, 2, 1)], 0)),
            "2, one new tagged guard": (two_old, [old1, old2, new1 + tag], ([(T, rel, 0, 1)], [], 1)),
            "2, one new tagged guard where no untagged guard was allowed": ({}, [new1 + tag], ([(T, rel, 0, 1)], [], 0)),
            "3, one new untagged guard": (two_old, [old1, old2, new1], ([(D, rel, 2, 3)], [], 0)),
            "4, one old guard retagged and one new untagged guard": (two_old, [old1 + tag, old2, new1], ([(T, rel, 0, 1)], [], 1)),
            "4, one old guard retagged and two new untagged guards": (two_old, [old1 + tag, old2, new1, new2], ([(D, rel, 2, 3), (T, rel, 0, 1)], [], 1)),
            "5, one old guard retagged and one new tagged guard": (two_old, [old1 + tag, old2, new1 + tag], ([(T, rel, 0, 2)], [(D, rel, 2, 1)], 1)),
        }
        for what, (ledger, guard_lines, want) in cases.items():
            got = reads(ledger, guard_lines)[1]
            if got != want:
                return f"selftest: RETAG OR NEW row {what}: read (increases, decreases, may_hide) = {got}, and must read {want}"

        # A MOVE LEAVES NO HEADROOM (H1). Both old guards are retagged and the human approves the
        # tagged entry. The untagged allowance of 2 must go in the same write, or two NEW untagged
        # clamps would later read as "no change": green, no human, no trace.
        seen, _ = reads(two_old, [old1 + tag, old2 + tag])
        out, err = approved(seen, two_old, T, rel, "ADR-TEST", root=td)
        if err or out != {T: {rel: {"count": 2, "approved": "ADR-TEST"}}}:
            return f"selftest: approving two retagged guards did not remove the untagged allowance they left behind ({err}, {out})"
        said = "\n".join(approval_lines(T, rel, "ADR-TEST", seen, two_old, out, (td / rel).read_text()))
        for words in ("also LOWERING the allowed 'dsp_guard' markers in h2/b.h from 2 to 0", "APPROVING 2 NEWLY TAGGED GUARDS"):
            if words not in said:
                return f"selftest: --approve of two retagged guards did not print {words!r}"
        if "dashboard.html" in said:
            return "selftest: --approve said the dashboard is stale although the ledger's category count did not change"
        bpath.write_text(dump_baseline(out))
        (td / rel).write_text((td / rel).read_text() + new1 + "\n" + new2 + "\n")
        if verdict() != ([(D, rel, 0, 2)], []):
            return (f"selftest: two new untagged guards, added after two old ones were retagged and approved, "
                    f"did not read red as dsp_guard 0 -> 2 (got {verdict()})")
        # one of two retagged: the allowance drops to what is left and keeps its own reference
        seen, _ = reads(two_old, [old1 + tag, old2])
        out, err = approved(seen, two_old, T, rel, "ADR-TEST", root=td)
        if err or out != {D: {rel: {"count": 1, "approved": INIT_REF}}, T: {rel: {"count": 1, "approved": "ADR-TEST"}}}:
            return f"selftest: approving one retagged guard did not lower the untagged allowance from 2 to 1 ({err}, {out})"
        # the lowering never RAISES: with the untagged count above its allowance it stays red
        seen, _ = reads(two_old, [old1, old2, new1, new2 + tag])
        out, err = approved(seen, two_old, T, rel, "ADR-TEST", root=td)
        if err or out.get(D) != two_old[D]:
            return f"selftest: a tagged approval changed an untagged allowance the tree is ABOVE ({err}, {out})"
        # the mirror: two tagged guards lose their tags and are approved under a trace; the tagged
        # allowance must go, or two new tagged guards would later need nobody
        (td / "traces/b.md").write_text(cause.replace("src/a.cpp", rel))
        two_tagged = {T: {rel: {"count": 2, "approved": "ADR-TEST"}}}
        seen, _ = reads(two_tagged, [old1, old2])
        out, err = approved(seen, two_tagged, D, rel, "traces/b.md", root=td)
        if err or out != {D: {rel: {"count": 2, "approved": "traces/b.md"}}}:
            return f"selftest: approving two untagged guards did not remove the tagged allowance they left behind ({err}, {out})"
        said = "\n".join(approval_lines(D, rel, "traces/b.md", seen, two_tagged, out, ""))
        for words in ("APPROVING 2 NEW GUARDS", "traces/b.md names h2/b.h", "also LOWERING the allowed 'dsp_guard_tagged'"):
            if words not in said:
                return f"selftest: --approve of two guards under one trace did not print {words!r}"
        bpath.write_text(dump_baseline(out))
        (td / rel).write_text((td / rel).read_text() + new1 + tag + "\n" + new2 + tag + "\n")
        if verdict() != ([(T, rel, 0, 2)], []):
            return f"selftest: two new tagged guards, added after two old tags were removed and approved, did not read red as 0 -> 2 (got {verdict()})"
        (td / "traces/b.md").unlink()

        # WHAT --approve SHOWS (H2). Row 4 above: the human is asked to approve one tagged guard,
        # and the new clamp is one of the UNTAGGED lines. Every guard line must be printed, with
        # both categories' numbers and the plain sentence, or the new clamp is never seen.
        seen, _ = reads(two_old, [old1 + tag, old2, new1])
        out, err = approved(seen, two_old, T, rel, "ADR-TEST", root=td)
        said = "\n".join(approval_lines(T, rel, "ADR-TEST", seen, two_old, out or {}, (td / rel).read_text()))
        must_say = (f"h2/b.h:2: [tagged] {old1}", f"h2/b.h:3: [UNTAGGED] {old2}", f"h2/b.h:4: [UNTAGGED] {new1}",
                    "dsp_guard: ledger 2 -> 2; the tree holds 2", "dsp_guard_tagged: ledger 0 -> 1; the tree holds 1",
                    "cannot tell which guard is new", "up to 1 NEW UNTAGGED guard(s)", "dashboard.html")
        for words in must_say:
            if err or words not in said:
                return f"selftest: --approve dsp_guard_tagged did not print {words!r} ({err})"
        if "GUARDS under ONE" in said:
            return "selftest: --approve of ONE new guard announced several"
        if "up to 1 NEW UNTAGGED" not in increase_message(T, rel, 0, 1, hidden=may_hide(seen, two_old, rel)):
            return "selftest: the red line for a tagged increase that may hide a new untagged guard did not say so"
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
            out, err = approved(counts, base(), "unwired", rel, ref)
            if out is not None or not err:
                return f"selftest: --approve with ref {ref!r} was not refused"
        if bpath.read_bytes() != before:
            return "selftest: a refused --approve changed the baseline"
        out, err = approved(counts, base(), "unwired", rel, "ADR-TEST")
        if err or out != {"unwired": {rel: {"count": 1, "approved": "ADR-TEST"}}}:
            return f"selftest: --approve with a ref did not raise exactly one entry ({err}, {out})"
        if any("GUARD" in line for line in approval_lines("unwired", rel, "ADR-TEST", counts, base(), out, "")):
            return "selftest: --approve of a category that is not a dsp_guard one printed dsp_guard lines"
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
        new, err = approved(counts, baseline, cat, f, ref)
        if err:
            print(f"weakening_check: {err}", file=sys.stderr)
            return 1
        # errors="replace" as in scan(): the same text the counts were taken from
        src = (ROOT / f).read_text(encoding="utf-8", errors="replace") if cat == "dsp_guard_tagged" else ""
        print("\n".join(approval_lines(cat, f, ref, counts, baseline, new, src)))
        _write(bpath, new)
        print(f"weakening: approved {cat} {f} ->{new[cat][f]['count']} ({ref.strip()})")
        return 0

    if args.tighten:
        _write(bpath, tightened(counts, baseline))
        baseline = load_baseline(bpath)[0]

    inc, dec = compare(counts, baseline)
    stale = ledger_trace_problems(baseline)
    for cat, f, old, new in dec:
        print(f"note: {cat} {f}: {old} -> {new} (a decrease; --tighten locks it in)")
    if inc:
        print("weakening_check: FAILED -- gate-weakening markers rose above the baseline:", file=sys.stderr)
        for row in inc:
            hidden = may_hide(counts, baseline, row[1]) if row[0] == "dsp_guard_tagged" else 0
            print(f"  {increase_message(*row, hidden=hidden)}", file=sys.stderr)
    if stale:
        print("weakening_check: FAILED -- a dsp_guard approval in the ledger does not cite a root cause:",
              file=sys.stderr)
        for line in stale:
            print(f"  {line}", file=sys.stderr)
    if inc or stale:
        return 1
    total = sum(n for fs in counts.values() for n in fs.values())
    print(f"weakening: {total} markers across {len(counts)} categories, 0 increases")
    return 0


if __name__ == "__main__":
    sys.exit(main())
