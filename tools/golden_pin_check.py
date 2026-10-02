#!/usr/bin/env python3
"""golden_pin_check -- every golden pinned in h2/README.md still IS the pinned blob.

WIRED: ./verify fast

WHY. h2_engine_parity_check judges the composed engine against golden files
pinned BY CONTENT (ADR-187 item 3): h2/README.md carries `<path>@<git blob>`.
That check lives in `./verify full`, which is human-paced and never runs in CI.
B407 (#897) edited one meta line in docs/design/scalpel-interface-lab.html, a
pinned golden; the PIN row went red, but ci.yml ignores docs/** and docs.yml runs
only `verify fast`, so the break merged unseen and was reverted in #905
(LIBRARY L0073). This check is the cheap half of that PIN row, moved into
`fast`, so a pinned file cannot move without a gate in every lane seeing it.

WHAT IT CHECKS. Every backticked `<path>@<40-hex>` in the pin source
(PIN_SOURCES) against `git hash-object --no-filters <path>` on the working
tree. Red when a pinned file is missing or its blob differs; the message names
the path, the pinned blob and the actual blob.

THE PIN SOURCE IS NOT A SECOND LIST. h2/README.md is the only file the pin logic
of h2_engine_parity_check (its `slurp("h2/README.md")` PIN rows) and of
h2_scalpel_parity_check / h2_rules_check (check_pin) reads. If a future check
reads pins from another file, add it to PIN_SOURCES in the same PR.

MUST-FAIL CONTROLS (LIBRARY L0032), on in-memory data, every run, never
touching the repo's files: (1) a pin altered by one hex digit reads red, (2) a
missing path reads red, (3) a README holding zero pins fails as BLIND (a
pattern that stops matching must not read as "all pins hold"). A positive
control (an intact pin reads green) keeps the judge from being always-red.

--no-filters: the parity stream hashes raw bytes (createHash over the file), so
an autocrlf/eol filter must not be allowed to make this check disagree with it.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PIN_SOURCES = ["h2/README.md"]
# Backticked, so prose that merely mentions "@" never reads as a pin; the same
# `<path>@<blob>` shape the parity check's PIN row searches for.
PIN_RE = re.compile(r"`([^`\s@]+)@([0-9a-f]{40})`")
REPIN = "re-pin on purpose (ADR-187 item 3), never by editing the pin to match"


def parse_pins(text):
    """-> [(path, pinned_blob)], first occurrence order, duplicates collapsed."""
    seen, out = set(), []
    for path, blob in PIN_RE.findall(text):
        if (path, blob) not in seen:
            seen.add((path, blob))
            out.append((path, blob))
    return out


def judge(source, text, blob_of):
    """Pure judge for ONE pin source. blob_of(path) -> hex blob, or None when the
    file is missing. -> list of failure strings; an empty pin list is a failure."""
    pins = parse_pins(text)
    if not pins:
        return [f"{source}: holds ZERO `<path>@<blob>` pins; the check is blind, not green "
                f"(did the pin format change?)"]
    fails = []
    for path, pinned in pins:
        actual = blob_of(path)
        if actual is None:
            fails.append(f"{source}: pinned file {path} is MISSING (pinned {pinned}); {REPIN}")
        elif actual != pinned:
            fails.append(f"{source}: {path} pinned {pinned} but the file is now {actual}; {REPIN}")
    return fails


def git_blob(path):
    """Working-tree blob id, or None if the file is not there."""
    if not (ROOT / path).is_file():
        return None
    r = subprocess.run(["git", "hash-object", "--no-filters", "--", path],
                       cwd=ROOT, capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 and r.stdout.strip() else None


def selftest():
    good = "a/b.js@" + "1" * 40
    readme = f"prose, then `{good}` and `c/d.html@{'2' * 40}` end"
    blobs = {"a/b.js": "1" * 40, "c/d.html": "2" * 40}
    if judge("synthetic", readme, blobs.get):
        return "positive control: intact pins read red"
    if len(judge("synthetic", readme, {**blobs, "a/b.js": "1" * 39 + "2"}.get)) != 1:
        return "control 1: a pin whose blob differs by one hex digit did not read red"
    if len(judge("synthetic", readme, {"a/b.js": "1" * 40}.get)) != 1:
        return "control 2: a missing path did not read red"
    if not judge("synthetic", "no pins here, only a@b and `x@123`", blobs.get):
        return "control 3: a README holding zero pins passed instead of failing as blind"
    return None


def main():
    err = selftest()
    if err:
        print("golden_pin_check: FAILED (the check itself is broken) -- " + err, file=sys.stderr)
        return 1
    fails, n = [], 0
    for src in PIN_SOURCES:
        p = ROOT / src
        if not p.is_file():
            fails.append(f"{src}: pin source is missing")
            continue
        text = p.read_text(encoding="utf-8")
        n += len(parse_pins(text))
        fails += judge(src, text, git_blob)
    if fails:
        print(f"golden_pin_check: FAILED -- {len(fails)} pin problem(s):", file=sys.stderr)
        for f in fails:
            print("    " + f, file=sys.stderr)
        return 1
    print(f"golden_pin_check: GREEN ({n} pinned goldens in {', '.join(PIN_SOURCES)} match their working-tree blobs; "
          f"must-fail controls fired: one-digit blob, missing path, zero pins)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
