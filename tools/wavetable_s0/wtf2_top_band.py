#!/usr/bin/env python3
"""S0 item 2 (B665): WT-F2's design figures, engine-free (SPEC-WAVETABLE WT-4, WT-5, WT-F2; CONTRACT §7 C1).

Origin: horde lead session, 2026-11-09, B665 (CONTRACT §5.6 item 2).

WHAT IT PROVES. The 16-20 kHz energy of a saw table read through WT-4's levels (H * r <= c, c = min(20 kHz,
0.45 f_int), levels H = floor(rho^k)) and WT-5's pitch crossfade (the upper third of each level's range in log read
rate, mixed with the darker neighbour by C.0's raised cosine), against an ideal band-limited saw (every harmonic
below 20 kHz at 1/n), per level spacing rho = 2, 2^1/2, 2^1/4, 2^1/8: at C6 and at every semitone C2 to B5, at
44.1 and 48 kHz. Re-derives the drafter's -6.84 / -5.30 / -2.54 dB at C6.

CONTROL (must read 0 dB): the ideal saw against itself. DETECTOR PLANT (D): a saw with its top four harmonics
below 20 kHz removed, which the meter must read below 0. MUTATION PLANT (M): the pitch crossfade disabled, which
at one level per octave must read C6 at -5.30 against the design's -6.84 (outside 0.5 dB).

The figure is energy, not a render: harmonic n's amplitude is (1 - w) [n <= H] + w [n <= H_darker] times 1/n, w the
crossfade weight, summed over harmonics in [16 kHz, 20 kHz). A render of the same spectrum reads the same number
(the read's interpolation error is item 5's, 110 dB and more below). "Empty" means no table energy in the band.
Equal temperament at A4 = 440 Hz.

Run: python3 -I tools/wavetable_s0/wtf2_top_band.py   (seconds)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import _common as cm

SPACINGS = (("one per octave", 2.0), ("half octave", 2 ** 0.5), ("quarter octave", 2 ** 0.25), ("eighth octave", 2 ** 0.125))


def amplitudes(f, f_int, rho, crossfade=True):
    """Per-harmonic gain (0..1) of a table read at f: the level, and its darker neighbour in WT-5's band."""
    c = cm.ceiling(f_int)
    Hs = cm.level_sizes(rho)
    i = max(j for j, H in enumerate(Hs) if H * f <= c)   # this level: H_i * f <= c < H_{i+1} * f
    H = Hs[i]
    w = 0.0
    if crossfade and i > 0 and i + 1 < len(Hs):
        r_lo, r_hi = c / Hs[i + 1], c / H                 # the level's range in read rate
        u_start = np.log(r_lo) + (2.0 / 3.0) * (np.log(r_hi) - np.log(r_lo))
        if np.log(f) > u_start:
            u = (np.log(f) - u_start) / (np.log(r_hi) - u_start)
            w = 0.5 * (1 - np.cos(np.pi * u))
    n = np.arange(1, cm.NH + 1)
    g = (1 - w) * (n <= H) + (w * (n <= Hs[i - 1]) if i > 0 else 0.0)
    return g, H, w


def band_db(gain, f, drop_top=0):
    n = np.arange(1, cm.NH + 1)
    fn = n * f
    ideal = (fn < 20000.0)
    band = (fn >= 16000.0) & (fn < 20000.0)
    ref = np.sum((1.0 / n[band]) ** 2)
    g = gain.copy()
    if drop_top:
        top = n[ideal][-drop_top:]
        g[top - 1] = 0.0
    e = np.sum((g[band] / n[band]) ** 2)
    return -np.inf if e == 0 else 10 * np.log10(e / ref)


def main():
    c6 = cm.note_hz(84)
    print("WT-F2: 16-20 kHz energy of a saw table vs an ideal band-limited saw (dB)")
    for fs in (44100.0, 48000.0):
        print(f"\n  f_int = {fs / 1000:g} kHz (c = {cm.ceiling(fs):.0f} Hz)")
        print(f"  {'spacing':16s} {'C6':>8s} {'C6 no xfade':>12s} {'C2-B5 worst':>12s} {'empty notes':>12s} {'median':>8s}")
        for label, rho in SPACINGS:
            g, H, w = amplitudes(c6, fs, rho)
            at_c6 = band_db(g, c6)
            g0, _, _ = amplitudes(c6, fs, rho, crossfade=False)
            no_x = band_db(g0, c6)
            vals, empty = [], []
            for midi in range(36, 84):          # C2 .. B5
                f = cm.note_hz(midi)
                v = band_db(amplitudes(f, fs, rho)[0], f)
                vals.append(v)
                if not np.isfinite(v):
                    empty.append(midi)
            finite = [v for v in vals if np.isfinite(v)]
            names = ",".join(note_name(m) for m in empty) if empty else "none"
            print(f"  {label:16s} {at_c6:8.2f} {no_x:12.2f} {min(vals):12.2f} {len(empty):12d} {np.median(finite):8.2f}"
                  f"   (C6: H={H}, crossfade w={w:.3f}; empty: {names})")
            if label == "quarter octave" or label == "eighth octave":
                print(f"  {'':16s} worst non-empty C2-B5: {min(finite):.2f} dB")
    print("\ncontrol: the ideal saw against itself at C6, 44.1 kHz:",
          f"{band_db((np.arange(1, cm.NH + 1) * c6 < 20000.0).astype(float), c6):.2f} dB")
    print("detector plant (D): the ideal saw with its top four harmonics below 20 kHz removed, C6 and C4, 44.1 kHz:",
          f"{band_db((np.arange(1, cm.NH + 1) * c6 < 20000.0).astype(float), c6, drop_top=4):.2f},",
          f"{band_db((np.arange(1, cm.NH + 1) * cm.note_hz(60) < 20000.0).astype(float), cm.note_hz(60), drop_top=4):.2f} dB")
    g0, _, _ = amplitudes(c6, 44100.0, 2.0, crossfade=False)
    print(f"mutation plant (M): crossfade disabled, one per octave, C6, 44.1 kHz: {band_db(g0, c6):.2f} dB"
          f" (design {band_db(amplitudes(c6, 44100.0, 2.0)[0], c6):.2f}; outside 0.5 dB: "
          f"{abs(band_db(g0, c6) - band_db(amplitudes(c6, 44100.0, 2.0)[0], c6)) > 0.5})")


def note_name(m):
    return ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"][m % 12] + str(m // 12 - 1)


if __name__ == "__main__":
    main()
