#!/usr/bin/env python3
"""S0 item 1 (B665): J2's delayed read, engine-free (SPEC-WAVETABLE WT-24, WT-25, WT-F42; CONTRACT §7 J2).

Origin: horde lead session, 2026-11-09, B665 (CONTRACT §5.6 item 1).

WHAT IT PROVES. A synced table member read at theta - tau*f/f_int (the table read delayed by tau, no filter E), with
the restart's step residual s(x) - [x >= tau] and corner residual x + Q(x) - (x - tau)+ added at their natural
timing (x samples after the undelayed restart), is (a) a band-limited restart, by QS-1's alias meter at 44.1 kHz
and by its harmonics against an ideal additive synced read, and (b) holds DT-3 (WT-F42): the first 16 harmonics of
a held C4 below 18 kHz within 0.5 dB between 44.1, 48 and 96 kHz, on Sine -> saw frame 15 and Vowel frame 7, at
r = 2.5 and 3.5, s = 0. It also measures WT-25's question: whether tau = -A-bar (E's pinned moment, carried over)
leaves the restart corrections' mean at zero.

THE DESIGNS RENDERED. `delayed` (J2's proposal); `E` (ED-1 as written: E on the undelayed read plus r2, q2, the
engine's law today, for reference); `naive` (no correction).
MUST-FAIL CONTROL: `nodelay`, B-51's mismatch: the same residuals' steps (s - 1) on an undelayed read, curved
corners uncorrected (the pre-round-42 law); `nodelay+q`, the same with the corner residual Q(x). Both must miss
DT-3. MUST-READ-ZERO CONTROL: r = 1 and r = 2 (no restart step, no corner): the corrections add exactly 0 and the
output equals the plain synced read bit for bit.

CHOICES THE SPEC LEAVES OPEN (stated, and their sensitivity printed): the step table is the clean-room engine's
(0.47 f_s, Kaiser 8.3, 32 taps; tau = -A-bar = 2.48980 samples); the residual length is the table's 32 samples;
the level is one per octave with no pitch crossfade (at C4 both ratios sit outside WT-5's crossfade band, H = 16 at
every rate, so DT-3 sees only the restart law); one member, bandLimit 1, no DC removal in the render (the mean is
reported separately). Sensitivity: tau +- 0.25, + 0.01, + 0.001, tau rounded to 2 samples, residuals truncated to 24 and 16 samples,
and QS-1's reference table design (0.4725, beta 8).

Run: python3 -I tools/wavetable_s0/j2_delayed_read.py   (about a minute)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import _common as cm

RATES = (44100.0, 48000.0, 96000.0)
CASES = (("sine-saw", 15), ("vowel", 7))
RATIOS = (2.5, 3.5)
# frac r = 0.5 restarts a sine-series table (Sine -> saw, Vowel: im only) between two of its zero crossings, so
# WT-F42's own ratios carry no step (Δ = 0 exactly), only a corner. 2.75 and 3.25 add restarts with a step; both stay
# at H = 16 outside WT-5's crossfade band at C4 and C6 at every rate.
STEP_RATIOS = (2.75, 3.25)
PREROLL = 128


def synced_value(a, b, phase):
    """y(phase) for the band-limited level (harmonics 1..H of the table), phase in table cycles."""
    h = np.arange(1, len(a) + 1)
    ang = 2 * np.pi * np.outer(phase, h)
    return np.cos(ang) @ a + np.sin(ang) @ b


def synced_slope(a, b, phase):
    h = np.arange(1, len(a) + 1)
    ang = 2 * np.pi * np.outer(phase, h)
    return (np.cos(ang) @ (b * h) - np.sin(ang) @ (a * h)) * 2 * np.pi


def frac(x):
    return x - np.floor(x)


def render(a, b, r, f, fs, L, design, st, tau=None, rlen=32):
    """One member, L samples of steady state (a pre-roll is rendered and dropped)."""
    tau = st.tau if tau is None else tau
    n = np.arange(-PREROLL, L, dtype=float)
    inc = f / fs
    P = n * inc                                    # unwrapped member phase (cycles)
    delayed = design == "delayed"
    Pd = P - tau * inc if delayed else P
    y = synced_value(a, b, frac(r * frac(Pd)))     # the naive synced read (delayed or not)
    if design == "E":
        y = np.convolve(y, st.E)[:len(y)]          # ED-1's filter on the uncorrected read (causal)
    if design in ("delayed", "E", "nodelay", "nodelay+q"):
        fr = frac(r)
        y_pre, y_post = synced_value(a, b, np.array([fr, 0.0]))
        d_step = y_post - y_pre                    # Δ = y(φ+) - y(φ-) (WT-24), exact 0 at a whole ratio
        g_pre, g_post = synced_slope(a, b, np.array([fr, 0.0]))
        d_corner = inc * r * (g_post - g_pre)      # Δσ per sample
        if fr == 0.0:
            d_step = d_corner = 0.0                # skipped as a branch at a whole ratio (bit-identical to computing it)
        if d_step != 0.0 or d_corner != 0.0:
            m0 = int(np.floor(P[0])) - 1
            m1 = int(np.floor(P[-1]))
            for m in range(m0, m1 + 1):
                te = m / inc                       # the undelayed restart, in samples
                lo = int(np.ceil(te)) + PREROLL
                hi = min(lo + rlen + 1, len(n))
                if hi <= 0 or lo >= len(n):
                    continue
                lo = max(lo, 0)
                x = n[lo:hi] - te
                inside = (x >= 0) & (x < rlen)
                if design == "delayed":
                    ind = np.floor(Pd[lo:hi]) >= m              # the delayed read has restarted at this sample
                    ramp = np.where(ind, (Pd[lo:hi] - m) / inc, 0.0)
                    rs = st.s(x) - ind
                    rq = x + st.Q(x) - ramp
                elif design == "E":
                    k = np.floor(x).astype(int)
                    kk = np.clip(k, 0, 32)
                    rs = st.s(x) - st.Ec[kk]
                    rq = x + st.Q(x) - (x * st.Ec[kk] - st.Em[kk])
                else:
                    rs = st.s(x) - 1.0
                    rq = st.Q(x) if design == "nodelay+q" else np.zeros_like(x)
                y[lo:hi] += np.where(inside, d_step * rs + d_corner * rq, 0.0)
    return y[PREROLL:]


def ideal_harmonics(a, b, r, kmax):
    """Complex amplitudes X_k (k = 0..kmax) of one member cycle of y(r * frac P), P in [0, 1): exact."""
    h = np.arange(1, len(a) + 1)[:, None]
    k = np.arange(kmax + 1)[None, :]

    def I(nu):
        out = np.ones_like(nu, dtype=complex)
        nz = nu != 0
        out[nz] = (np.exp(2j * np.pi * nu[nz]) - 1) / (2j * np.pi * nu[nz])
        return out

    X = ((a - 1j * b)[:, None] / 2 * I(h * r - k) + (a + 1j * b)[:, None] / 2 * I(-h * r - k)).sum(0)
    return X


def dt3_levels(y, f, fs, nh=16):
    """DT-3's method: 1 s, 4-term Blackman-Harris, the power within +-4 bins of each harmonic (dB)."""
    N = len(y)
    j = np.arange(N)
    w = 0.35875 - 0.48829 * np.cos(2 * np.pi * j / N) + 0.14128 * np.cos(4 * np.pi * j / N) - 0.01168 * np.cos(6 * np.pi * j / N)
    p = np.abs(np.fft.rfft(y * w)) ** 2 * 4.0 / (N * np.sum(w * w))
    df = fs / N
    out = []
    for k in range(1, nh + 1):
        fk = k * f
        if fk >= 18000.0:
            break
        c = int(round(fk / df))
        out.append(10 * np.log10(p[c - 4:c + 5].sum() + 1e-300))   # 20 log10 of the amplitude
    return np.array(out)


def judged_spread(levels_by_rate):
    """The largest level difference between any two rates over the judged harmonics (within 80 dB of the strongest)."""
    L = np.array(levels_by_rate)
    judged = L.max(0) >= L.max() - 80.0
    return np.max(L[:, judged].max(0) - L[:, judged].min(0))


def main():
    st = cm.StepTable()
    f4 = cm.note_hz(60)
    print(f"step table: cutoff 0.47, Kaiser 8.3; tau = -A-bar = {st.tau:.6f} samples")
    tables = {}
    for name, fr in CASES:
        re, im = cm.frame_coeffs(name, fr)
        tables[(name, fr)] = (re, im)

    # ---- DT-3 (WT-F42) on each design --------------------------------------------------------------------------
    designs = ("delayed", "E", "nodelay", "nodelay+q", "naive")
    print("\nDT-3 (WT-F42): largest harmonic-level spread between 44.1/48/96 kHz, harmonics 1-16 < 18 kHz, held C4 "
          "(dB; pass <= 0.5)")
    print(f"{'case':22s} " + " ".join(f"{d:>10s}" for d in designs) + "   |delayed - ideal| max")
    for (name, fr), (re, im) in tables.items():
        for r in RATIOS + STEP_RATIOS:
            H = cm.level_for(r * f4, 44100.0)
            assert all(cm.level_for(r * f4, fs) == H for fs in RATES)
            a, b = re[:H], im[:H]
            row = []
            for d in designs:
                lv = [dt3_levels(render(a, b, r, f4, fs, int(fs), d, st), f4, fs) for fs in RATES]
                row.append(judged_spread(lv))
            X = ideal_harmonics(a, b, r, 16)
            ideal = 20 * np.log10(2 * np.abs(X[1:]))
            dev = max(np.max(np.abs(dt3_levels(render(a, b, r, f4, fs, int(fs), "delayed", st), f4, fs) - ideal))
                      for fs in RATES)
            y_pre, y_post = synced_value(a, b, np.array([frac(r), 0.0]))
            print(f"{name}/{fr} r={r} H={H:<3d}   " + " ".join(f"{v:10.3f}" for v in row)
                  + f"   {dev:.3f} dB   (step Δ = {y_post - y_pre:+.3f})")

    # ---- the must-read-zero control: r = 1 and r = 2 ------------------------------------------------------------
    print("\nmust-read-zero control (no restart step or corner): max |corrected - plain synced read|")
    for (name, fr), (re, im) in tables.items():
        for r in (1.0, 2.0):
            H = cm.level_for(r * f4, 44100.0)
            a, b = re[:H], im[:H]
            fs = 44100.0
            out = render(a, b, r, f4, fs, 4096, "delayed", st)
            n = np.arange(4096, dtype=float)
            plain = synced_value(a, b, frac(r * frac(n * f4 / fs - st.tau * f4 / fs)))
            lv = [dt3_levels(render(a, b, r, f4, q, int(q), "delayed", st), f4, q) for q in RATES]
            print(f"  {name}/{fr} r={r:.0f}: max diff {np.max(np.abs(out - plain)):.1e}; DT-3 spread {judged_spread(lv):.4f} dB")

    # ---- band-limited restart: QS-1's meter at 44.1 kHz ----------------------------------------------------------
    print("\nband-limited restart, QS-1's meter at 44.1 kHz (dB, capped 140; ideal additive is the clean control)")
    print(f"{'case':26s} " + " ".join(f"{d:>10s}" for d in designs) + "      ideal")
    qtables = dict(tables)
    qtables[("sine-saw", 0)] = cm.frame_coeffs("sine-saw", 0)   # a sine: no curvature at the sample scale
    for (name, fr), (re, im) in qtables.items():
        for midi, label in ((60, "C4"), (84, "C6")):
            k = cm.qs1_cycles(cm.note_hz(midi))
            fs = 44100.0
            f = k * fs / cm.QS1_LEN
            for r in RATIOS + STEP_RATIOS:
                H = cm.level_for(r * f, fs)
                a, b = re[:H], im[:H]
                row = [cm.qs1_alias_db(render(a, b, r, f, fs, cm.QS1_LEN, d, st), k, fs) for d in designs]
                kmax = int(20000.0 // f)
                X = ideal_harmonics(a, b, r, kmax)
                n = np.arange(cm.QS1_LEN)
                ph = (n * k % cm.QS1_LEN) / cm.QS1_LEN
                ideal = np.real(np.exp(2j * np.pi * np.outer(ph, np.arange(1, kmax + 1))) @ (2 * X[1:]))
                print(f"{name}/{fr} {label} r={r} H={H:<3d}  " + " ".join(f"{v:10.1f}" for v in row)
                      + f" {cm.qs1_alias_db(ideal, k, fs):10.1f}")

    # ---- WT-25: the restart corrections' mean -------------------------------------------------------------------
    print("\nWT-25: area of the restart residuals (samples x unit height), averaged over the edge fraction δ")
    dl = np.linspace(0, 1, 4096, endpoint=False)
    xs = np.arange(0, 32)[None, :] + dl[:, None]
    for label, tau in (("tau = -A-bar", st.tau), ("tau + 0.25", st.tau + 0.25), ("tau = 2 (rounded)", 2.0)):
        rs = st.s(xs) - (xs >= tau)
        rq = xs + st.Q(xs) - np.maximum(xs - tau, 0.0)
        A = rs.sum(1)
        print(f"  {label:18s} step: mean over δ {A.mean():+.3e}, range over δ [{A.min():+.4f}, {A.max():+.4f}];"
              f" corner: mean {rq.sum(1).mean():+.4f}, tail q(32-) {rq[:, -1].mean():+.2e}")
    k_ = np.floor(xs).astype(int)
    rqE = xs + st.Q(xs) - (xs * st.Ec[k_] - st.Em[k_])
    print(f"  ED-1's E (for comparison): step mean {(st.s(xs) - st.Ec[k_]).sum(1).mean():+.3e}, corner mean {rqE.sum(1).mean():+.4f}")
    print("  the render's DC from the corrections (delayed design minus the uncorrected delayed read, 1 s at 44.1 kHz):")
    for (name, fr), (re, im) in tables.items():
        for r in RATIOS + STEP_RATIOS:
            H = cm.level_for(r * f4, 44100.0)
            a, b = re[:H], im[:H]
            fs = 44100.0
            y = render(a, b, r, f4, fs, 44100, "delayed", st)
            n = np.arange(44100, dtype=float)
            y0 = synced_value(a, b, frac(r * frac(n * f4 / fs - st.tau * f4 / fs)))
            dc = np.mean(y - y0)
            rms = np.sqrt(np.mean((y - np.mean(y)) ** 2))
            print(f"    {name}/{fr} r={r}: corrections' mean {dc:+.2e} ({20 * np.log10(abs(dc) / rms + 1e-30):.1f} dB re the output RMS)")

    # ---- sensitivity to the open details -----------------------------------------------------------------------
    print("\nsensitivity (delayed design): DT-3 spread (dB) / QS-1 at C6, 44.1 kHz (dB), Sine -> saw 15 at r = 2.5, 3.5, 2.75, 3.25, then Vowel 7 the same")
    ref = cm.StepTable(0.4725, 8.0)
    variants = (("as built", st, None, 32), ("tau + 0.25", st, st.tau + 0.25, 32), ("tau - 0.25", st, st.tau - 0.25, 32),
                ("tau + 0.01", st, st.tau + 0.01, 32), ("tau + 0.001", st, st.tau + 0.001, 32),
                ("tau = 2", st, 2.0, 32), ("residual 24", st, None, 24), ("residual 16", st, None, 16),
                ("QS-1 ref table", ref, None, 32))
    k6 = cm.qs1_cycles(cm.note_hz(84))
    f6 = k6 * 44100.0 / cm.QS1_LEN
    for label, table, tau, rlen in variants:
        cells = []
        for (name, fr), (re, im) in tables.items():
            for r in RATIOS + STEP_RATIOS:
                H = cm.level_for(r * f4, 44100.0)
                a, b = re[:H], im[:H]
                lv = [dt3_levels(render(a, b, r, f4, fs, int(fs), "delayed", table, tau, rlen), f4, fs) for fs in RATES]
                H6 = cm.level_for(r * f6, 44100.0)
                q = cm.qs1_alias_db(render(re[:H6], im[:H6], r, f6, 44100.0, cm.QS1_LEN, "delayed", table, tau, rlen), k6)
                cells.append(f"{judged_spread(lv):.3f}/{q:.1f}")
        print(f"  {label:15s} " + "  ".join(f"{c:>12s}" for c in cells))


if __name__ == "__main__":
    main()
