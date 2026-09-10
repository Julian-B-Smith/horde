#!/usr/bin/env python3
"""depends_check -- the feature-dependency graph must stay honest (ADR-108).

WHY. Three systems used to answer "does this parameter matter right now?"
independently: shown_when (GUI), the morph field (which happily flipped a
parameter whose enabling law was off), and the engine's own guards, which are
the truth the other two approximated. `depends` is now the single declaration
that feeds the first two. This gate defends the third relationship -- the one
no generator can enforce -- by checking the declaration against the code.

WHAT IT CHECKS.
  1. STRUCTURE: every clause names a parameter the shell declares, and a value
     inside that parameter's range. A typo'd key silently means "always live",
     which is the failure mode this class of table has every time.
  2. GENERATION: the generated header is current. A stale depends_graph.h means
     the morph hierarchy is running on a graph nobody can see.
  3. DRIFT (advisory, printed not failed): parameters the ENGINE guards behind a
     mode check but the table does not describe. Advisory because a guard is not
     always a dependency -- `sawBase > 0.001` is a magnitude test, not a mode --
     and turning judgement into a hard gate is how exemption lists start.

WHAT IT DOES NOT CHECK. Whether a declared dependency is CORRECT. Nothing here
can know that `harmReach` belongs to law 4 except by reading the branch that
uses it, which is a human act. The gate keeps the declaration well-formed and
current; it does not pretend to keep it true.
"""
import re
import subprocess
import sys
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
TSV = ROOT / "src/param_presentation.tsv"
CLAP = ROOT / "src/hypersaw_clap.cpp"
HDR = ROOT / "src/depends_graph.h"
CORE = ROOT / "src/swarm_core.h"

fail = []
rows = [l.split("\t") for l in TSV.read_text(encoding="utf-8").split("\n")
        if l and not l.startswith("#")]
hdr = rows[0]
di, ai = hdr.index("depends"), hdr.index("address")

src = CLAP.read_text(encoding="utf-8")
params = {}
for m in re.finditer(r'\{(\d+), "([A-Za-z0-9_]+)", "[^"]*", ([-0-9.]+), ([-0-9.]+)', src):
    params[m.group(2)] = (int(m.group(1)), float(m.group(3)), float(m.group(4)))

declared = 0
for r in rows[1:]:
    if len(r) <= di or not r[di].strip():
        continue
    declared += 1
    if r[di].strip() == "never":
        continue        # explicitly retired; see the grammar note in the table
    for grp in r[di].split(";"):
        for cl in grp.split(","):
            if "=" not in cl:
                fail.append(f"{r[ai]}: clause {cl!r} has no '='")
                continue
            key, vals = cl.split("=", 1)
            if key not in params:
                fail.append(f"{r[ai]}: depends on {key!r}, which no parameter declares")
                continue
            _, lo, hi = params[key]
            for v in vals.split("|"):
                try:
                    fv = float(v)
                except ValueError:
                    fail.append(f"{r[ai]}: {key}={v!r} is not a number")
                    continue
                if fv < lo - 1e-9 or fv > hi + 1e-9:
                    fail.append(f"{r[ai]}: {key}={v} is outside [{lo:g}, {hi:g}]")

before = HDR.read_text(encoding="utf-8") if HDR.exists() else ""
subprocess.run([sys.executable, str(ROOT / "tools/gen_depends_header.py")],
               capture_output=True, cwd=ROOT)
if HDR.read_text(encoding="utf-8") != before:
    fail.append("depends_graph.h was stale -- run tools/gen_depends_header.py")

# 3. advisory drift: engine mode-guards with no declaration
core = CORE.read_text(encoding="utf-8")
guarded = set(re.findall(r'p\.law == (\d+)', core))
advisory = []
if guarded:
    lawdeps = {r[di].strip() for r in rows[1:] if len(r) > di and "law=" in r[di]}
    for g in sorted(guarded):
        if not any(f"law={g}" in d for d in lawdeps):
            advisory.append(f"engine guards law=={g} but no parameter declares it")

if fail:
    print("depends_check: FAILED", file=sys.stderr)
    for f in fail[:15]:
        print("  " + f, file=sys.stderr)
    sys.exit(1)
for a in advisory:
    print("  note  " + a)
print(f"depends_check: GREEN ({declared} declared dependencies, header current"
      + (f", {len(advisory)} advisory)" if advisory else ")"))
