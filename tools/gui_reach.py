#!/usr/bin/env python3
"""gui_reach — every declared param must be REACHABLE in at least one shipped GUI.

L0023, third occurrence and the one that finally became a gate. The lesson says
a parameter range widened without its UI control ships a feature that is fully
implemented, fully tested, host-automatable, and absent from the product — and
that NO audio oracle can see it, because the audio path is correct. It has now
cost: FX types 0..5 shipping with dropdowns offering 0..3 for two weeks; a panic
button that existed everywhere except the interface that ships; and the discovery
(2026-08-12) that gui2 reaches 18 of 105 params while being someone's daily
driver.

Prose did not prevent any of those. This is the enforcement.

WHAT IS GATED: a param unreachable in EVERY GUI is a build failure. That is the
original bug and it is unambiguous.

WHAT IS REPORTED, NOT GATED: per-GUI coverage. gui2 is an in-progress renovation
and failing the build over its gaps would block all work — but the number is
printed on every run so the gap cannot be forgotten again, which is precisely how
it got this far. A number nobody sees is prose with extra steps.
"""
import re, sys, pathlib

root = pathlib.Path(__file__).resolve().parent.parent
decl = (root / "src/hypersaw_clap.cpp").read_text(encoding="utf-8")
rows = re.findall(r'\{\s*(\d+),\s*"([A-Za-z0-9_]+)",\s*"([^"]*)"', decl)
params = {int(i): (k, n) for i, k, n in rows if int(i) < 1000}

# Deliberately not user-facing, each with its reason. L0036: a deliberate absence
# needs a test, and this list IS that test — adding to it is a visible decision,
# not a silent omission.
EXEMPT = {
    70: "inertiaCurve — dev-only, labelled (dev) in the param table",
}

guis = {}
for p in sorted((root / "src/gui").glob("*.html")):
    guis[p.name] = {int(x) for x in re.findall(r'data-p="(\d+)"', p.read_text(encoding="utf-8"))}

# ---- PATCH-SCOPE PINNING -------------------------------------------------
# Some params are dispatched by RAW id in the shell (`id == 43`,
# `id >= 57 && id <= 64`, …) rather than by base id, because they address ONE
# shared object — the FX rack, the SPECTRA engine, the engine selector — not a
# per-oscillator core. A GUI control for one of those MUST carry data-fixed, or
# `effId()` remaps it (57 -> 1057) when a non-first oscillator is selected, it
# matches no dispatch branch, and the control silently does nothing.
#
# Found 2026-08-12 while adding the gui2 FX page: gui.html had 29 such controls
# unpinned, so selecting oscillator 2 killed the whole FX rack, the whole SPECTRA
# surface and the engine selector — with every oracle green, because the audio
# path is correct and the events simply never arrive. L0028's role-vs-instance
# shape, reached through the GUI rather than the event loop.
#
# The ranges are PARSED FROM THE SHELL, not hardcoded, so a new raw-id family is
# covered the day it is written rather than the day someone remembers this.
shell = (root / "src/hypersaw_clap.cpp").read_text(encoding="utf-8").split("\n")
patch_scope = set()
for n, line in enumerate(shell):
    ranges = re.findall(r'id >= (\d+) && id <= (\d+)', line)
    ones = re.findall(r'if \(id == (\d+)\)', line)
    if not (ranges or ones):
        continue
    # TWO semantic anchors, and the second was learned the hard way. (1) The body
    # must dispatch to a SHARED object — the same `id >= N && id <= M` shape also
    # appears in range clamps. (2) The branch must RETURN. `if (id == 14)
    # spectra.setParam("swidth", …)` touches a shared object and then FALLS
    # THROUGH to the per-oscillator core, so width is dual-scope and pinning it
    # would break per-oscillator width. A branch that handles-and-stops is
    # patch-scope; one that handles-and-continues is not.
    # findall, not search: line 1394 carries TWO ranges and `search` silently
    # dropped the second (65-68, the SPECTRA ADSR).
    body = "\n".join(shell[n:n + 8])
    if not re.search(r'\b(rack|spectra|engineSel)\b', body):
        continue
    if "return;" not in body:
        continue
    for lo, hi in ranges:
        patch_scope.update(range(int(lo), int(hi) + 1))
    for one in ones:
        patch_scope.add(int(one))
patch_scope &= set(params)

unpinned = []
for name, path in [(p.name, p) for p in sorted((root / "src/gui").glob("*.html"))]:
    text = path.read_text(encoding="utf-8")
    for i in sorted(patch_scope):
        if re.search(r'data-p="%d"(?![^>]*data-fixed)' % i, text):
            unpinned.append((name, i))

reachable_anywhere = set().union(*guis.values()) if guis else set()
orphans = sorted(set(params) - reachable_anywhere - set(EXEMPT))

for name, ids in sorted(guis.items()):
    hit = len(ids & set(params))
    print("  %-12s reaches %3d / %d params" % (name, hit, len(params)))
if EXEMPT:
    print("  exempt: %s" % ", ".join(EXEMPT.values()))

print("  patch-scope params (raw-id dispatch, must be data-fixed): %d" % len(patch_scope))

if unpinned:
    print("gui_reach: RED — patch-scope control(s) not pinned with data-fixed;", file=sys.stderr)
    print("  effId() will remap these and the control will silently do nothing:", file=sys.stderr)
    for name, i in unpinned:
        print("    %-12s %4d %-16s %s" % (name, i, params[i][0], params[i][1]), file=sys.stderr)
    sys.exit(1)

if orphans:
    print("gui_reach: RED — %d param(s) reachable in NO gui:" % len(orphans), file=sys.stderr)
    for i in orphans:
        print("    %4d %-16s %s" % (i, params[i][0], params[i][1]), file=sys.stderr)
    sys.exit(1)
print("gui_reach: GREEN (every declared param is reachable in some GUI)")
