#!/usr/bin/env python3
"""S0 item 5 (B665): WT-6's read designs, alias figures by QS-1's exact method (SPEC-WAVETABLE WT-6, WT-F1).

Origin: horde lead session, 2026-11-09, B665 (CONTRACT §5.6 item 5).

WHAT IT PROVES. On a 1/n-saw table at the level WT-4 picks (one per octave, H * f <= c, no pitch crossfade), one
member, 44.1 kHz, QS-1's method (an odd whole number of cycles in 65,536 samples, rectangular window, double
precision, every non-harmonic bin 20 Hz..20 kHz, capped at 140 dB), at C1, C2, C4, C6, C8, the alias ratio of:
  - the 6-point Lagrange read at N = min(8192, max(2048, 16 H))   (WT-6's built design)
  - the 4-point cubic at N = max(2048, 32 H), as 4-point Lagrange and as the engine's cubic (Catmull-Rom, the
    read `core/edges/edges.h` uses); WT-6 does not name which, so both are printed
  - the 8-point Lagrange at N = min(4096, max(2048, 16 H))        (WT-6's fallback)
  - the 2-point linear at N = 2048                                 (the cost bench's first design: MUST FAIL)
  - direct additive synthesis                                      (MUST READ CLEAN: the cap)
against WT-F1's floors (QS-1's: 110, 104, 98, 93 dB at C2, C4, C6, C8; C1 reported).

The table holds N samples of one cycle of the level (harmonics 1..H, exact by inverse FFT), read periodically (the
guard samples' effect: no wrap mid-kernel). The read position is phi * N, phi the member phase, exact in binary
at these pitches. Lagrange kernels are centred: the K points at floor(x) - K/2 + 1 .. floor(x) + K/2.

Run: python3 -I tools/wavetable_s0/wt6_read_alias.py   (under a minute)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import _common as cm

FS = 44100.0
NOTES = (("C1", 24), ("C2", 36), ("C4", 60), ("C6", 84), ("C8", 108))
FLOORS = {"C2": 110, "C4": 104, "C6": 98, "C8": 93}


def lagrange_read(table, x, K):
    N = len(table)
    i = np.floor(x).astype(np.int64)
    u = x - i
    offs = np.arange(-K // 2 + 1, K // 2 + 1)
    out = np.zeros_like(x)
    for j in offs:
        w = np.ones_like(u)
        for m in offs:
            if m != j:
                w *= (u - m) / (j - m)
        out += w * table[(i + j) % N]
    return out


def catmull_read(table, x):
    N = len(table)
    i = np.floor(x).astype(np.int64)
    u = x - i
    a, b, c, d = (table[(i + j) % N] for j in (-1, 0, 1, 2))
    return b + u * (0.5 * (c - a) + u * ((a - 2.5 * b + 2 * c - 0.5 * d) + u * (0.5 * (d - a) + 1.5 * (b - c))))


def main():
    n_saw = np.arange(1, cm.NH + 1)
    im = 1.0 / n_saw
    re = np.zeros(cm.NH)
    designs = ("6-pt Lagrange", "4-pt Lagrange", "4-pt Catmull", "8-pt Lagrange", "2-pt linear", "additive")
    rows = {}
    for label, midi in NOTES:
        k = cm.qs1_cycles(cm.note_hz(midi), FS)
        f = k * FS / cm.QS1_LEN
        H = cm.level_for(f, FS)
        n = np.arange(cm.QS1_LEN, dtype=np.int64)
        phase = (n * k % cm.QS1_LEN) / cm.QS1_LEN          # exact in binary
        sizes = {"6-pt Lagrange": min(8192, max(2048, 16 * H)), "4-pt Lagrange": max(2048, 32 * H),
                 "4-pt Catmull": max(2048, 32 * H), "8-pt Lagrange": min(4096, max(2048, 16 * H)), "2-pt linear": 2048}
        row = {}
        for d in designs:
            if d == "additive":
                X =np.zeros(cm.QS1_LEN // 2 + 1, dtype=complex)
                X[k * np.arange(1, H + 1)] = (re[:H] - 1j * im[:H]) * (cm.QS1_LEN / 2.0)
                y = np.fft.irfft(X, cm.QS1_LEN)
            else:
                N = sizes[d]
                table = cm.render_cycle(re, im, N, H)
                x = phase * N
                if d == "2-pt linear":
                    y = lagrange_read(table, x, 2)
                elif d == "4-pt Catmull":
                    y = catmull_read(table, x)
                else:
                    y = lagrange_read(table, x, int(d[0]))
            row[d] = (cm.qs1_alias_db(y, k, FS), sizes.get(d))
        rows[label] = (H, f, row)
    print("WT-6 read designs, QS-1's method at 44.1 kHz on a 1/n-saw table (alias ratio, dB, capped 140; N in brackets)")
    print(f"{'note':5s} {'H':>4s} {'f (Hz)':>9s} " + " ".join(f"{d:>17s}" for d in designs) + "   floor")
    for label, (H, f, row) in rows.items():
        cells = []
        for d in designs:
            v, N = row[d]
            cells.append(f"{v:7.1f}" + (f" [{N:5d}]" if N else "        ") + "  ")
        print(f"{label:5s} {H:4d} {f:9.3f} " + " ".join(f"{c:>17s}" for c in cells) + f"   {FLOORS.get(label, '-')}")
    mem = {}
    for d, rule in (("6-pt Lagrange", lambda H: min(8192, max(2048, 16 * H))), ("8-pt Lagrange", lambda H: min(4096, max(2048, 16 * H))),
                    ("4-pt (32 H)", lambda H: max(2048, 32 * H)), ("2-pt linear", lambda H: 2048)):
        Ns = [rule(H) for H in cm.level_sizes(2.0)]
        mem[d] = (sum(Ns) * 4 / 1024, max(Ns) * 4 / 1024)
    print("\nmemory of one one-per-octave level set (float32, no guards): " +
          "; ".join(f"{d} {t:.0f} KiB (largest {l:.0f} KiB)" for d, (t, l) in mem.items()))
    fails = [(label, d) for label, (H, f, row) in rows.items() for d in designs
             if label in FLOORS and row[d][0] < FLOORS[label]]
    print("below a floor:", ", ".join(f"{d} at {label}" for label, d in fails) or "none")


if __name__ == "__main__":
    main()
