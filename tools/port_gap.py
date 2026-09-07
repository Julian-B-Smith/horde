#!/usr/bin/env python3
"""port_gap — which reference parameters never reached their C++ port?

WHY THIS EXISTS. Three unported reference behaviours surfaced in two days, each
found by the human playing the plugin rather than by anything we ran: the plain
pitch wheel (ADR-096 aftermath), the quantiser's `qTime` step gate, and per-note
MPE bend inertia (ADR-097). They share a shape — the reference had it, a gate
covered the NEIGHBOURING case, and nothing ever asked whether this one was
covered at all.

Parity cannot ask that question. Parity certifies agreement over the surface the
reference RENDERS with the settings a golden happens to use (LIBRARY L0031); a
parameter the goldens never move is invisible to it, and a parameter the port
never DECLARED is invisible twice over.

WHAT THIS IS AND IS NOT. It is a lexical diff of parameter NAMES: the keys of a
lab's defaults object against the fields its C++ core declares. That makes it a
lead generator, not an oracle — a name present in both proves nothing about
behaviour, and a name missing from the port may be a deliberate omission (lab
bench furniture, a display-only meter). Every hit needs a human read. It is
therefore NOT wired into ./verify: a check whose output is "go look at these"
cannot be a pass/fail gate without inviting a rubber stamp.
"""
import re, sys, pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent

# (reference, port, why-this-pairing)
PAIRS = [
    ("reference/swarmsaw.html",      "src/swarm_core.h",      "SAW oscillator (ADR-011/012 lineage)", {}),
    ("reference/swarmdynamics.html", "src/swarm_core.h",      "dynamics layer, same core", {
        # RESOLVED, not ignored: each of these IS in the port, under another name.
        "beatQ":  "became detune law 3, not a flag — swarm_core.h:93 ports the beatQ path",
        "swidth": "renamed `width`; same expression, pan = x[i]*width — swarm_core.h:138",
    }),
    ("reference/swarmspectra.html",  "src/spectra_core.h",    "SPECTRA per-partial sibling", {}),
    ("reference/swarmfilter.html",   "src/filter_core.h",     "Track E filter", {}),
    ("reference/swarmphaser.html",   "src/notch_core.h",      "Track E phaser/notch", {}),
    ("reference/swarmtime.html",     "src/time_core.h",       "Track E time", {}),
    ("reference/swarmalator.html",   "src/swarmalator_core.h","experimental swarmalator (ADR-048)", {}),
    # ADDED 2026-08-27. morph_core.h's own header names this lab as its
    # reference -- "ported from docs/design/quantum-morph-lab.html. The lab is
    # the reference" -- but the pair was never registered here, so nothing
    # compared them and an unported control was invisible. That is exactly how
    # `timing` (Immediate / Next note) sat in the reference and never reached
    # the engine: not a regression, a port gap with no gate looking at it.
    ("docs/design/quantum-morph-lab.html", "src/morph_core.h", "quantum morph (ADR-104)", {
        # The lab is a full instrument bench; morph_core is the assignment law
        # and nothing else. The shell owns everything below.
        "x": "morphX, shell-owned (id 152)",
        "y": "morphY, shell-owned (id 153)",
        "temp": "morphTemp, shell-owned (id 154)",
        "coup": "morphCoup, shell-owned (id 155)",
        "seed": "morphSeed, shell-owned (id 156)",
        "glide": "morphGlide, shell-owned (id 158)",
        "contMode": "morphMode, shell-owned (id 157) — quantum/blend",
        "arp":   "bench arpeggiator, not morph surface",
        "bpm":   "bench transport; the plugin takes tempo from the host",
        "vol":   "bench level",
        "arate": "bench arp rate",
        "adepth":"bench arp depth",
        "sel":   "bench UI selection state",
    }),
    ("docs/design/bend-lab.html", "src/glide_core.h", "travel laws (ADR-093/096/097)", {
        # bend-lab drives a demo oscillator so you can HEAR the law. Those are
        # bench furniture; glide_core is the travel law and nothing else.
        "n":      "bench oscillator voice count, not travel-law surface",
        "detune": "bench oscillator detune",
        "drift":  "bench oscillator drift",
        "vol":    "bench oscillator level",
    }),
]

# UNION EVERY CANDIDATE BLOCK. Two heuristics were tried and both produced a
# scan that could not fire:
#   1. "longest object literal" picked swarmalator's CSS-in-JS style block, so
#      that lab was compared on `background, border, color` and its engine
#      parameters were never looked at.
#   2. "first `this.p =`" picked bend-lab's DEMO OSCILLATOR defaults, not its
#      travel-law defaults — proven by planting the pre-2026-08-20 port (qTime
#      deleted) and watching the scan report the same five names as before.
# Selecting ONE block means betting on which block matters, and a wrong bet is
# silent. So take every literal that looks like defaults and union the keys:
# over-collection is visible (a name appears and someone reads it) whereas
# under-collection is not.
KEY = re.compile(r'([A-Za-z_$][A-Za-z0-9_$]*)\s*:', re.M)

def js_params(path):
    src = (ROOT / path).read_text(errors="ignore")
    keys = set()
    for m in re.finditer(r'\{[^{}]{40,4000}\}', src, re.S):
        body = m.group(0)
        k = set(KEY.findall(body))
        if len(k) < 4:
            continue
        if body.count(';') > len(k):      # a statement block, not a literal
            continue
        keys |= k
    if not keys:
        raise SystemExit(f"port_gap: no defaults-shaped literal found in {path}")
    # NARROW TO WHAT THE LAB LETS YOU SET. Union-of-literals catches the real
    # parameters but also every DOM helper, theme colour and per-note state field
    # in the file — 108 leads, which is a list nobody reads, and an unread list is
    # the same as no list. A lab PARAMETER is something the bench exposes a
    # control for, so intersect with the ids the HTML actually declares. That is a
    # definition rather than an exemption list: it needs no maintenance and it
    # cannot quietly grow to cover a real gap.
    ids = set(re.findall(r'id="([A-Za-z_$][A-Za-z0-9_$]*)"', src))
    return keys & ids

def cpp_fields(path):
    src = (ROOT / path).read_text(errors="ignore")
    names = set()
    # declared struct fields: `double a = 1, b = 2;` / `int mask[12];`
    for m in re.finditer(r'^\s*(?:double|float|int|bool|long)\s+([^;]+);', src, re.M):
        for part in m.group(1).split(','):
            n = re.match(r'\s*([A-Za-z_][A-Za-z0-9_]*)', part)
            if n:
                names.add(n.group(1))
    # and anything addressable by key through setParam's map
    names |= set(re.findall(r'"([A-Za-z_][A-Za-z0-9_]*)"\s*\)?\s*(?:==|\?|:|,)', src))
    names |= set(re.findall(r'k\s*==\s*"([A-Za-z_][A-Za-z0-9_]*)"', src))
    return names

# Names that are lab furniture, not engine surface. Listed rather than pattern-
# matched so each exemption is a claim someone made on purpose.
BENCH = {
    "bendRange", "wobF", "mpe", "applyTo",      # bend-lab bench controls / routing UI
    # CSS-in-JS style blocks live in the same files and match the same shape
    "background", "border", "color", "cursor", "family", "padding", "radius",
    "spacing", "weight", "font", "margin", "opacity", "transform", "transition",
    "display", "position", "top", "bottom", "flex", "grid", "gap", "zIndex",
    "stroke", "fill", "align", "justify", "overflow", "content", "shadow",
    "left", "right", "size", "lineWidth", "textAlign", "globalAlpha",
    "value", "label", "name", "type", "id", "min", "max", "step", "title",
    "width", "height", "x", "y", "w", "h", "r", "g", "b", "a",
}

def main():
    rows = []
    for ref, port, why, resolved in PAIRS:
        if not (ROOT / ref).exists() or not (ROOT / port).exists():
            rows.append((ref, port, None, "MISSING FILE"))
            continue
        raw = js_params(ref) - cpp_fields(port) - BENCH
        gaps = sorted(raw - set(resolved))
        rows.append((ref, port, gaps, why, sorted(raw & set(resolved)), resolved))
    total = 0
    for ref, port, gaps, why, known, resolved in rows:
        if gaps is None:
            print(f"  {ref:34s} -> {port:26s} {why}")
            continue
        mark = "OK " if not gaps else "LEAD"
        print(f"  {mark} {ref:32s} -> {port:24s} {len(gaps)} unmatched ({why})")
        for g in gaps:
            print(f"        ? {g}")
        for k in known:
            print(f"        . {k}: {resolved[k]}")
        total += len(gaps)
    print(f"port_gap: {total} unexplained name(s). Explained names are printed with "
          f"their reason so an exemption stays a claim, not a silence.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
