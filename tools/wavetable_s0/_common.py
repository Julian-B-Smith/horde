"""Shared engine-free pieces for the wavetable S0 items (B665). Not a gate; imported by the item scripts.

Origin: horde lead session, 2026-11-09, B665 (CONTRACT §5.6, S0's pending engine-free items).

Everything is numpy, double precision, deterministic (no RNG, no clock). The scripts run with `python3 -I`,
which drops the script's directory from sys.path, so each script inserts its own directory before importing
this module.

What lives here, and where its law comes from:
- the two factory tables the items use (SPEC-WAVETABLE WT-40, the QUADRANT bench's formulas at 1,024 harmonics,
  Sine -> saw negated for WT-20's polarity), peak-normalised by WT-2;
- WT-16's loudness gain (K-weighted RMS against the band-limited +-1 saw at C3);
- WT-4's level choice (H * r <= c, levels H = floor(rho^k));
- ED-1's band-limited step table, built exactly as the clean-room engine builds it (`core/edges/tables.cpp` at
  e72a092: cutoff 0.47 f_s, Kaiser beta 8.3, 32 taps, 128 grid points per tap, real-cepstrum minimum phase,
  read by the engine's 4-point cubic), and QS-1's reference design (0.4725, beta 8.0) for sensitivity;
- QS-1's alias meter (odd whole cycles in 65,536 samples, rectangular window, non-harmonic bins 20 Hz to 20 kHz,
  capped at 140 dB).
"""
import numpy as np

NH = 1024          # harmonics per frame (WT-1)
NFRAMES = 16
C3 = 130.8127826502993
A4 = 440.0


def note_hz(midi):
    return A4 * 2.0 ** ((midi - 69) / 12.0)


# --- factory tables (WT-40) --------------------------------------------------------------------------------------

def frame_coeffs(table, f):
    """(re, im) arrays for harmonics 1..1024 of factory `table` at frame index f (0..15), peak-normalised (WT-2).
    Harmonic n contributes re*cos(2 pi n phi) + im*sin(2 pi n phi)."""
    t = f / (NFRAMES - 1)
    n = np.arange(1, NH + 1, dtype=float)
    re = np.zeros(NH)
    if table == "sine-saw":
        k = 3.5 * (1 - t) ** 1.5
        im = -np.exp(-(n - 1) * k) / n          # negated: WT-20's polarity, the base wave's saw rises
    elif table == "vowel":
        A, I, B, G = [730, 1090, 2440], [270, 2290, 3010], [90, 120, 170], [1, .6, .32]
        im = 0.12 / n
        for j in range(3):
            F = A[j] + (I[j] - A[j]) * t
            im = im + G[j] * np.exp(-.5 * ((n * 110 - F) / B[j]) ** 2)
    else:
        raise ValueError(table)
    peak = np.max(np.abs(render_cycle(re, im, 8192)))
    return re / peak, im / peak


def render_cycle(re, im, N, H=None):
    """One cycle at N points from harmonics 1..H (default all), exact (inverse real FFT)."""
    H = len(re) if H is None else H
    X = np.zeros(N // 2 + 1, dtype=complex)
    X[1:H + 1] = (re[:H] - 1j * im[:H]) * (N / 2.0)
    return np.fft.irfft(X, N)


# --- WT-16's loudness gain ---------------------------------------------------------------------------------------

def k_weight_mag(freq):
    """|K(f)| of ITU-R BS.1770 (pre-filter shelf times RLB high-pass), the standard 48 kHz biquads."""
    z = np.exp(-2j * np.pi * np.asarray(freq, float) / 48000.0)
    b1, a1 = [1.53512485958697, -2.69169618940638, 1.19839281085285], [1.0, -1.69065929318241, 0.73248077421585]
    b2, a2 = [1.0, -2.0, 1.0], [1.0, -1.99004745483398, 0.99007225036621]
    h1 = (b1[0] + b1[1] * z + b1[2] * z * z) / (a1[0] + a1[1] * z + a1[2] * z * z)
    h2 = (b2[0] + b2[1] * z + b2[2] * z * z) / (a2[0] + a2[1] * z + a2[2] * z * z)
    return np.abs(h1 * h2)


def loudness_gain(re, im):
    """WT-16 (1): the gain that matches a frame's K-weighted RMS at C3 to the band-limited +-1 saw's."""
    n = np.arange(1, NH + 1)
    keep = n * C3 < 20000.0
    kw = k_weight_mag(n[keep] * C3) ** 2
    frame = np.sum(0.5 * (re[keep] ** 2 + im[keep] ** 2) * kw)
    saw = np.sum(0.5 * (2.0 / (np.pi * n[keep])) ** 2 * kw)
    return np.sqrt(saw / frame)


# --- WT-4's levels -----------------------------------------------------------------------------------------------

def level_sizes(rho):
    """The distinct level sizes H = floor(rho^k), k = 0, 1, ..., up to 1,024 (WT-4)."""
    out, k = [], 0
    while True:
        H = int(np.floor(rho ** k + 1e-9))
        if H > NH:
            break
        if not out or H != out[-1]:
            out.append(H)
        k += 1
    return out


def ceiling(f_int):
    """c = min(20 kHz, 0.45 f_int) (ADR-384 #8)."""
    return min(20000.0, 0.45 * f_int)


def level_for(read_rate, f_int, rho=2.0):
    """The largest level H with H * read_rate <= c (no crossfade)."""
    c = ceiling(f_int)
    Hs = [H for H in level_sizes(rho) if H * read_rate <= c]
    return Hs[-1] if Hs else 0


# --- ED-1's step table (the clean-room engine's construction) ---------------------------------------------------

class StepTable:
    """s(x), the 32-tap minimum-phase band-limited step, on the engine's grid; Q(x) its integral of s - 1; E."""

    TAPS = 32
    PHASES = 128

    def __init__(self, cutoff=0.47, beta=8.3):
        N, P = self.TAPS * self.PHASES, self.PHASES
        n = np.arange(N)
        x = (n - (N - 1) / 2.0) / P
        arg = 2 * np.pi * cutoff * x
        sinc = np.where(x == 0, 1.0, np.sin(arg) / np.where(x == 0, 1.0, arg))
        rr = 2.0 * n / (N - 1) - 1
        h = 2 * cutoff * sinc * np.i0(beta * np.sqrt(np.maximum(0.0, 1 - rr * rr))) / np.i0(beta)
        h /= np.sum(h) / P
        M = N * 64
        a = np.zeros(M, dtype=complex)
        a[:N] = h
        a = np.fft.ifft(np.log(np.maximum(np.abs(np.fft.fft(a)), 1e-100)))
        a[1:M // 2] *= 2.0
        a[M // 2 + 1:] = 0
        a[0] = a[0].real
        a[M // 2] = a[M // 2].real
        a = np.fft.ifft(np.exp(np.fft.fft(a))).real
        s = np.concatenate([[0.0], np.cumsum(a[:N] / P)])
        step = s / s[N] - 1.0                       # s - 1 on t = j / P, j = 0..N
        step[N] = 0.0
        ramp = np.concatenate([[0.0], np.cumsum(0.5 * (step[:-1] + step[1:]) / P)])
        self.abar = ramp[N]                         # A-bar: the residual's area averaged over the edge fraction
        self.tau = -self.abar                       # ED-1: tau = -A-bar
        # grid with guards for the 4-point read: index 1 + j holds t = j / P
        self._step = np.concatenate([[2 * step[0] - step[1]], step, [0.0, 0.0]])
        self._ramp = np.concatenate([[0.0], ramp - self.abar, [0.0, 0.0]])
        self._ramp[0] = 2 * self._ramp[1] - self._ramp[2]
        # E (ED-1): s' at whole samples, ends halved, unit sum, first moment pinned to tau by moving E[0], E[1]
        K = self.TAPS
        g = np.arange(K + 1) * P
        slope = np.empty(K + 1)
        slope[0], slope[K] = a[0], a[N - 1]
        slope[1:K] = 0.5 * (a[g[1:K] - 1] + a[g[1:K]])
        e = slope * np.where((np.arange(K + 1) == 0) | (np.arange(K + 1) == K), 0.5, 1.0)
        e /= e.sum()
        shift = self.tau - np.sum(np.arange(K + 1) * e)
        e[1] += shift
        e[0] -= shift
        self.E = e
        self.Ec = np.cumsum(e)                      # sum_{j <= k} E[j]
        self.Em = np.cumsum(np.arange(K + 1) * e)   # sum_{j <= k} j E[j]

    def _read(self, grid, t):
        x = np.clip(t, 0, self.TAPS) * self.PHASES
        i = np.minimum(x.astype(np.int64), self.TAPS * self.PHASES)
        u = x - i
        a, b, c, d = grid[i], grid[i + 1], grid[i + 2], grid[i + 3]
        return b + u * (0.5 * (c - a) + u * ((a - 2.5 * b + 2 * c - 0.5 * d) + u * (0.5 * (d - a) + 1.5 * (b - c))))

    def s(self, x):
        """The band-limited step, 0 before 0, 1 from 32 on."""
        x = np.asarray(x, float)
        return np.where(x < 0, 0.0, np.where(x >= self.TAPS, 1.0, self._read(self._step, x) + 1.0))

    def Q(self, x):
        """Integral from 0 of s - 1 (reaches A-bar at 32)."""
        x = np.asarray(x, float)
        return np.where(x <= 0, 0.0, np.where(x >= self.TAPS, self.abar, self._read(self._ramp, x) + self.abar))


# --- QS-1's alias meter ------------------------------------------------------------------------------------------

QS1_LEN = 65536


def qs1_cycles(f_target, fs=44100.0):
    """The odd whole number of cycles in 65,536 samples nearest the target pitch."""
    k = int(round(f_target * QS1_LEN / fs))
    if k % 2 == 0:
        k += 1 if f_target * QS1_LEN / fs > k else -1
    return k


def qs1_alias_db(y, k, fs=44100.0, cap=140.0):
    """Harmonic power over alias power in the non-harmonic bins 20 Hz..20 kHz, capped (QS-1)."""
    X = np.fft.rfft(y)
    p = np.abs(X) ** 2
    p[0] = 0.0
    bins = np.arange(len(p))
    freq = bins * fs / len(y)
    harm = (bins % k == 0) & (bins > 0)
    band = (freq >= 20.0) & (freq <= 20000.0)
    hp = p[harm].sum()
    ap = p[band & ~harm].sum()
    if ap <= 0:
        return cap
    return min(cap, 10 * np.log10(hp / ap))
