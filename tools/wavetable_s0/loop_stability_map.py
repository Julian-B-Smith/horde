#!/usr/bin/env python3
"""S0 item 3 (B665): the loop's stability map on table members, and I_back (SPEC-WAVETABLE WT-31, WT-65;
CONTRACT §7 D2, H6), engine-free by B521's method (SPEC-ENGINE BL-12's Measure, C.14's law).

Origin: horde lead session, 2026-11-09, B665 (CONTRACT §5.6 item 3).

WHAT IT PROVES.
(A) WT-31: a table member's `crossMod` and `feedback` act by C.14's law with xin added to the READ phase:
    y0[n] = g T(theta[n] + xin[n]), yhat = d y0 (loopDrive d; C.16's estimate is 0, a table has no DC and no
    transformation runs), xin[n] = 1/2 (crossMod yhat_{i+1}[n-1] + feedback 1/2 (yhat[n-1] + yhat[n-2])); one member,
    so it is its own neighbour (C.14). B521's method: 44.1 kHz, crossMod 1 and feedback 1, the note set and settled
    (the loop starts from rest at the note), a twin whose `feedback` steps to 0.999 at 1 s and back to 1 at 1.01 s
    through PA-1 (two one-poles of 6 ms in v, the Pow law, snapping within 1e-6); CONTRACTING if over 1.5 s..2.5 s the
    RMS of the twins' difference is below 1e-6 of the output's RMS, as rendered or with the twin shifted by k P
    samples, 0 < |k| < 12. Pitches C1..C6 as P = 1349, 674, 337, 168, 84, 42 samples; loopDrive 0.14..1.00 in steps
    of 0.01. Tables: Sine -> saw frames 0 and 15, Vowel frame 7, each at WT-4's level for the pitch and WT-16's
    loudness gain g.
(B) WT-65: the 1 <-> 2 loop on Sine -> saw frame 0: osc 2 renders first and reads osc 1 one sample late, averaged,
    phi2 = theta + (D_b / 2 pi) 1/2 (m1[n-1] + m1[n-2]) (the back-edge), then phi1 = theta + (D_f / 2 pi) m2[n] (the
    forward edge), m = g T(phi) (WT-61, WT-62, WT-63, BL-5's phase-FM form), same pitch on both. The twin steps D_b
    by x0.999 (in v, D = 10 v^2) at 1 s and back at 1.01 s; the heard output is osc 1. Mapped over D_b and D_f.

CONTROLS. Must read stable: depth 0 (loopDrive 0 lanes, so xin = 0: the twins are identical). Must read
unstable: the saw table (frame 15) at loopDrive 1, whose linearised loop gain is about 2 g d > 1 on its ramps.

CHOICES. The read is linear interpolation in an 8,192-point cycle of the level (the loop's dynamics, not its
aliasing, are measured here; the read's error is item 5's). The delayed read of J2 shifts every read by the same
tau f/f_int and does not change the loop. Standard quality only (C.14: the loop steps at the host rate at both
qualities; High's read is the same function sampled twice as often, not mapped here).

Run: python3 -I tools/wavetable_s0/loop_stability_map.py   (a few minutes; one process, nice it)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import _common as cm

FS = 44100.0
PERIODS = ((1349, "C1"), (674, "C2"), (337, "C3"), (168, "C4"), (84, "C5"), (42, "C6"))
TABLES = (("sine-saw", 0), ("sine-saw", 15), ("vowel", 7))
NTAB = 8192
W0, W1 = int(1.5 * FS), int(2.5 * FS)
STEP_ON, STEP_OFF = int(1.0 * FS), int(1.01 * FS)
SHIFTS = 11


def table_for(name, frame, period):
    re, im = cm.frame_coeffs(name, frame)
    H = cm.level_for(FS / period, FS)
    g = cm.loudness_gain(re, im)
    cyc = g * cm.render_cycle(re, im, NTAB, H)
    return np.concatenate([cyc, cyc[:1]]), H, g


def read(tabs, phase):
    """Linear read; tabs is (lanes, NTAB + 1), phase per lane in cycles."""
    x = (phase - np.floor(phase)) * NTAB
    i = np.minimum(x.astype(np.int64), NTAB - 1)
    u = x - i
    rows = np.arange(tabs.shape[0])
    return tabs[rows, i] * (1 - u) + tabs[rows, i + 1] * u


def pa1_track(target_fn, n_total):
    """PA-1: two one-poles of 6 ms in series on v, snapping when both are within 1e-6 of the target."""
    a = np.exp(-1.0 / (0.006 * FS))
    v1 = v2 = target_fn(0)
    out = np.empty(n_total)
    for n in range(n_total):
        t = target_fn(n)
        v1 = t + a * (v1 - t)
        v2 = v1 + a * (v2 - v1)
        if abs(v1 - t) < 1e-6 and abs(v2 - t) < 1e-6:
            v1 = v2 = t
        out[n] = v2
    return out


class Judge:
    """B521's contraction measure over the window, with whole-period shifts of the twin either way."""

    def __init__(self, lanes, period):
        self.P = period
        self.L = SHIFTS * period + 1
        self.ringA = np.zeros((self.L, lanes))
        self.ringB = np.zeros((self.L, lanes))
        self.s = np.zeros((2 * SHIFTS + 1, lanes))
        self.cnt = np.zeros(2 * SHIFTS + 1)
        self.pw = np.zeros(lanes)
        self.mean = np.zeros(lanes)

    def push(self, n, ya, yb):
        L, P = self.L, self.P
        self.ringA[n % L] = ya
        self.ringB[n % L] = yb
        if W0 <= n < W1:
            self.s[SHIFTS] += (ya - yb) ** 2
            self.cnt[SHIFTS] += 1
            self.pw += ya * ya
            self.mean += ya
            for k in range(1, SHIFTS + 1):
                self.s[SHIFTS + k] += (ya - self.ringB[(n - k * P) % L]) ** 2
                self.cnt[SHIFTS + k] += 1
        for k in range(1, SHIFTS + 1):          # the twin shifted the other way: A[t - kP] against B[t]
            if W0 <= n - k * P < W1:
                self.s[SHIFTS - k] += (self.ringA[(n - k * P) % L] - yb) ** 2
                self.cnt[SHIFTS - k] += 1

    def ratio(self):
        cnt = (W1 - W0)
        rms = np.sqrt(self.pw / cnt - (self.mean / cnt) ** 2)
        d = np.sqrt(self.s / np.maximum(self.cnt, 1)[:, None])
        return d.min(0) / np.maximum(rms, 1e-30)


def run_self_loop(period, drives):
    """Part (A): every table x drive at one pitch, both twins in one array."""
    lanes_t, info = [], []
    for name, frame in TABLES:
        tab, H, g = table_for(name, frame, period)
        info.append((name, frame, H, g))
        lanes_t.extend([tab] * len(drives))
    tabs = np.array(lanes_t)
    d = np.tile(np.asarray(drives, float), len(TABLES))
    lanes = len(d)
    v_hi, v_lo = 1.0, np.sqrt(0.999)
    t_end = W1 + SHIFTS * period          # the twin shifted forward needs k P samples past the window
    fb_b = pa1_track(lambda n: v_lo if STEP_ON <= n < STEP_OFF else v_hi, t_end) ** 2
    ya1 = np.zeros(lanes); ya2 = np.zeros(lanes); yb1 = np.zeros(lanes); yb2 = np.zeros(lanes)
    judge = Judge(lanes, period)
    for n in range(t_end):
        theta = (n % period) / period
        xa = 0.5 * (ya1 + 0.5 * (ya1 + ya2))                   # crossMod 1 (own neighbour), feedback 1
        xb = 0.5 * (yb1 + fb_b[n] * 0.5 * (yb1 + yb2))
        oa = read(tabs, theta + xa)
        ob = read(tabs, theta + xb)
        ya2, ya1 = ya1, d * oa
        yb2, yb1 = yb1, d * ob
        judge.push(n, oa, ob)
    return judge.ratio().reshape(len(TABLES), len(drives)), info


def run_pm_loop(period, pairs, tab):
    """Part (B): the 1 <-> 2 loop for each (D_f, D_b) pair at one pitch."""
    Df = np.array([p[0] for p in pairs], float)
    Db = np.array([p[1] for p in pairs], float)
    lanes = len(pairs)
    tabs = np.repeat(tab[None, :], lanes, axis=0)
    t_end = W1 + SHIFTS * period
    scale = pa1_track(lambda n: np.sqrt(0.999) if STEP_ON <= n < STEP_OFF else 1.0, t_end) ** 2
    a1 = np.zeros(lanes); a2 = np.zeros(lanes); b1 = np.zeros(lanes); b2 = np.zeros(lanes)
    judge = Judge(lanes, period)
    k = 1.0 / (2 * np.pi)
    for n in range(t_end):
        theta = (n % period) / period
        m2a = read(tabs, theta + k * Db * 0.5 * (a1 + a2))
        m2b = read(tabs, theta + k * Db * scale[n] * 0.5 * (b1 + b2))
        m1a = read(tabs, theta + k * Df * m2a)
        m1b = read(tabs, theta + k * Df * m2b)
        a2, a1 = a1, m1a
        b2, b1 = b1, m1b
        judge.push(n, m1a, m1b)
    return judge.ratio()


def largest_stable(values, ratios, limit=1e-6):
    """The largest value below which every grid point contracts (the first failure ends the run)."""
    best = None
    for v, r in zip(values, ratios):
        if r < limit:
            best = v
        else:
            break
    return best


def main():
    drives = [0.0] + [round(0.14 + 0.01 * i, 2) for i in range(87)]
    print("(A) WT-31: table member, crossMod 1 + feedback 1 on the read phase, B521's contraction measure, 44.1 kHz")
    print("    columns: the largest loopDrive d with every grid point from 0.14 up contracting; the first failing d;"
          " the count of contracting points above it")
    summary = {}
    for period, label in PERIODS:
        R, info = run_self_loop(period, drives)
        for t, (name, frame, H, g) in enumerate(info):
            r = R[t]
            assert r[0] == 0.0, "depth-0 control: the twins must be identical"
            lim = largest_stable(drives[1:], r[1:])
            first_fail = next((dd for dd, rr in zip(drives[1:], r[1:]) if rr >= 1e-6), None)
            above = sum(1 for dd, rr in zip(drives[1:], r[1:]) if first_fail is not None and dd > first_fail and rr < 1e-6)
            summary[(name, frame, label)] = (lim, first_fail, above, H, g, r[-1])
            print(f"  {name}/{frame:<2d} {label} (P={period:4d}, H={H:3d}, g={g:.3f}): stable to d = {lim}, first fail "
                  f"{first_fail}, contracting above it: {above}; at d = 1: ratio {r[-1]:.2e}; depth-0 control {r[0]:.1e}")
    print("\n  table limits (min over C1..C6):")
    for name, frame in TABLES:
        lims = [summary[(name, frame, lab)][0] for _, lab in PERIODS]
        print(f"    {name}/{frame}: " + ", ".join(f"{lab} {summary[(name, frame, lab)][0]}" for _, lab in PERIODS)
              + f"  -> {min((l for l in lims if l is not None), default=None)}"
              + ("" if all(l is not None for l in lims) else "  (some pitch never contracts)"))
    saw_d1 = [summary[("sine-saw", 15, lab)][5] for _, lab in PERIODS]
    print(f"  must-fail control, saw table at d = 1: ratios {', '.join(f'{x:.1e}' for x in saw_d1)} "
          f"(all >= 1e-6: {all(x >= 1e-6 for x in saw_d1)})")

    print("\n(B) WT-65: the 1 <-> 2 loop on Sine -> saw frame 0 (back-edge D_b one sample late, forward D_f), same pitch")
    tabs = {p: table_for("sine-saw", 0, p) for p, _ in PERIODS}
    Dbs = [round(0.25 * i, 2) for i in range(1, 41)]          # 0.25 .. 10
    forwards = ("sym", 1.0, 2.5, 5.0, 10.0)
    limits = {}
    for period, label in PERIODS:
        tab, H, g = tabs[period]
        pairs = [((db if f == "sym" else f), db) for f in forwards for db in Dbs]
        R = run_pm_loop(period, pairs, tab).reshape(len(forwards), len(Dbs))
        row = []
        for i, f in enumerate(forwards):
            lim = largest_stable(Dbs, R[i])
            limits[(label, f)] = lim
            row.append(f"{'D_f=D_b' if f == 'sym' else f'D_f={f:g}'}: {lim}")
        print(f"  {label} (H={H}, g={g:.3f}): largest D_b contracting from 0.25 up -> " + "; ".join(row))
    print("\n  I_back candidates (min over C1..C6):")
    for f in forwards:
        vals = [limits[(lab, f)] for _, lab in PERIODS]
        ok = [v for v in vals if v is not None]
        print(f"    {'D_f = D_b' if f == 'sym' else f'D_f = {f:g}'}: {min(ok) if len(ok) == len(vals) else 'none (some pitch fails at 0.25)'}")


if __name__ == "__main__":
    main()
