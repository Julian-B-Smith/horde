/*
 * station_core.h — STATION engine: 3-operator phase modulation + LFSR noise.
 *
 * STATUS: PORT PHASE 1 (core + oracle only). NOT wired into the shell, NOT in
 * `./verify`. The shell seam (id block, source row, GUI page) is phase 2 under
 * a separate brief; `tools/station_check.cpp` drives this header directly.
 *
 * ── WHAT "CORRECT" MEANS HERE ────────────────────────────────────────────────
 * Correctness is parity with reference/station.html's `StationCore` (ADR-003:
 * the prototype is the spec-in-code) at the L0-1 bar, eps = 1e-6 RMS, PLUS the
 * behavioural rows SPEC-STATION §11/§12 state that parity structurally cannot
 * see. Never "plausible-sounding audio". The goldens are rendered live out of
 * the HTML by tools/golden/gen_station_goldens.mjs — nothing is forked.
 *
 * ── PARITY-EXACT (bit-for-bit with the lab, every literal copied verbatim) ───
 *  · `kPmConst = 0.1591549` — the lab's TRUNCATED 1/(2π) (:267). The true
 *    0.15915494309189535 is a relative error of 2.708e-7, which is an RMS diff
 *    of 2.156e-6 at cell index 8 — 2.2x OVER the eps=1e-6 gate, and 7.635e-7
 *    (76 % of the whole budget) at the DEFAULT patch's own 2.6 cell. Reading
 *    SPEC-STATION §4's law ("p += cell * out_src / 2pi") and writing the pretty
 *    constant is correct by the spec and WRONG BY THE ORACLE. Do not "fix" it.
 *    (docs/audits/2026-09-18-station-lab-audit.md S3 / §2.3 / §6.4.)
 *  · The ONE-SAMPLE DELAY on every matrix tap (`prev[]`, SPEC §2). This is the
 *    defining semantics, not an approximation: it is what makes arbitrary
 *    routing cycles unconditionally stable. A port that reads the CURRENT
 *    sample is a different instrument — station_check's lag-residual row is the
 *    detector, and it reads the opposite answer for the other hypothesis.
 *    NOTE: the audit's proposed detector for this (the Bessel sideband sign
 *    pattern, §4.4) DOES NOT WORK and must not be used — one sample of delay on
 *    a sinusoidal modulator is a pure phase rotation and |Jn| is invariant
 *    under it. tools/labharness/station_check.mjs S8 disproved it by planting a
 *    no-delay build that reproduced the pattern to 3 decimals.
 *  · The envelope's FORWARD-EULER one-pole `lvl += (target-lvl)*(4.6/t)`, NOT
 *    `1-exp(-4.6/t)`. "Correcting" it changes every envelope shape (audit §2.2).
 *    So are the absolute segment exits 0.004 / 0.0005 and `max(0.0005, a/1000)`.
 *  · `kTau = 6.283185307179586` (full-double 2pi — this one is NOT truncated).
 *  · The 0.35 per-slot mix gain, the LINEAR pan law, noise output +-0.7
 *    (SPEC §6 as amended by ADR-177 §3), the LFSR taps (bit0^bit1 LONG /
 *    bit0^bit6 SHORT), the Nyquist MUTE in Hz (`f >= SR*0.5`, and a muted
 *    operator still steps its envelope), the noise clock `rate*SR*0.5` with
 *    KEYTRK against 261.63 Hz, and the pitch-env curve.
 *  · The per-voice LFSR seed derivation `mulberry32(seed ^ slot*2654435761)`
 *    forced odd. SPEC §11 item 7 says the DERIVATION is not a parity item — the
 *    RULE is (nonzero, per voice, deterministic per note) plus the periods
 *    32767/93 and an N-voice/1-voice noise RMS ratio of ~sqrt(N), not N. It is
 *    copied anyway because copying it costs nothing and buys bit-parity on
 *    every noise scenario.
 *  · Voice slots are handed out in ascending index order from a fresh core, so
 *    the k-th note takes slot k — the same slot the lab's `voices.length` gives
 *    it. That is what makes the seeds match, so the allocator's order is a
 *    parity commitment, not an implementation detail.
 *  · KNOWN LAB DEFECTS DELIBERATELY PRESERVED, because SPEC §11 does not list
 *    them as divergences and inventing one is out of this brief's scope:
 *    an operator switched OFF mid-note FREEZES its envelope (audit S9), the pan
 *    law is linear and so is a 3 dB gain control (S10), and the release runs to
 *    163 % of its stated time (S13). Each is an ADR owed, not a bug to fix here.
 *
 * ── DELIBERATELY DIVERGENT (SPEC-STATION §11, built AS SPECIFIED) ────────────
 *  1. DRW pure branch is BAND-LIMITED (§5): the 32x4-bit table's exact spectrum
 *     (<=16 partials) resynthesised into a harmonic-count-indexed mipmap, not
 *     the lab's linear interpolation. The RAW branch stays bit-parity, so
 *     `pure = 0` DRW scenarios are parity-gated and `pure > 0` DRW is covered
 *     behaviourally (it must beat the lab's -43.7 dB alias floor at MIDI 96).
 *  2. RATIO is CONTINUOUS (§3.1). Free, as it happens: the lab's `coarse` is
 *     already a double and only its UI slider steps at 0.5, so the lab's values
 *     are exact points of this law and parity holds at them.
 *  3. ADDED, ALL OFF BY DEFAULT so the default patch stays bit-parity (§3.4/§7):
 *     FREE phase mode + per-op phase offset, RING (ops 2/3, mix path only), and
 *     STEPPED envelope quantisation.
 *  4. Polyphony 16 with RELEASE-FADE stealing (§8; the lab caps at 8 and
 *     hard-shifts, a measured 68 %-of-peak step). Parity is gated at <= 8
 *     sounding voices, where no steal happens and `fadeGain` is exactly 1.0.
 *  5. NO master `tanh` (§11.5) and no master gain — engine output is clean;
 *     saturation belongs to the downstream chain. The lab's monitoring stage is
 *     therefore INVERTED (atanh) by the golden generator rather than replicated,
 *     so parity is measured at full engine amplitude with the whole eps budget.
 *  6. Usable standalone (§11.6): header-only, no shell, no allocation after
 *     construction, no wall-clock, mulberry32 streams only.
 *  + 5 ms control-rate smoothing on the 12 matrix cells (§4 — "preset recall
 *    must be click-free"). The lab has NO smoothing anywhere, so this is
 *    build-side work with nothing to match; it is bit-inert for a static patch
 *    because the smoother primes ON the target at the first render.
 *  + A one-pole DC blocker on the engine output (5 Hz, sample-rate-scaled).
 *    This is a PARITY item, not a divergence, as of the human's 2026-09-19
 *    ruling: SPEC-STATION §11 item 7 (landing on branch `station-dc-blocker`)
 *    says "port it exactly" — same form, same R = exp(-2pi*f_c/f_s), same
 *    position (after the level/pan sum, before the master gain the port does
 *    not have), two states per CHANNEL, and the same 1e-30 flush. Verified
 *    against BOTH lab variants: goldens from main (blocker absent, manifest
 *    @dcblock=0, core bypassed) and from that branch (blocker present,
 *    @dcblock=1, core enabled) both pass at eps = 1e-6.
 *
 * ── NOT PORTED (audit §6.3) ──────────────────────────────────────────────────
 * The master tanh, `performance.now()`, ScriptProcessorNode, the scope ring
 * buffer (a render-loop side effect — visualiser data goes through the shell's
 * snapshot path), `voices.splice`/`voices.shift` (audio-thread container
 * mutation; here a fixed array + an active flag), and the whole UI section.
 *
 * ── SPEC §10 ADDRESS -> FIELD MAP (phase 2's id table binds to these) ────────
 * The patch is a plain public struct rather than a string-keyed setParam: the
 * strcmp chain would be ~100 lines of surface that the shell's id switch has to
 * duplicate anyway. The mapping is 1:1 and mechanical:
 *   op{n}.on/wave/mode/ratio/semis/fine/fixed/lvl/pan/pw/pure/qnt/phase/retrig
 *                                    -> patch.ops[n-1].{on,wave,mode,coarse,
 *                                       semis,fine,fixed,lvl,pan,pw,pure,qnt,
 *                                       phase,retrig}
 *   op{2,3}.sync / op{2,3}.ring      -> patch.ops[n-1].{sync,ring}
 *   op{n}.env.{a,d,s,r,loop,step}    -> patch.ops[n-1].env.*
 *   ns.{on,mode,rate,ktrk,lvl,pan}   -> patch.noise.*     ns.env.* -> patch.noise.env.*
 *   mtx[src][dst]                    -> patch.matrix[src][dst]   (src 0..3 = OP1,OP2,OP3,NS)
 *   penv.{amt,dec}                   -> patch.pitchEnv.{amt,dec}
 *   (wave RAM)                       -> setTable() / loadFactoryTable()
 * Ranges and defaults are §10's; this header does not clamp — the shell's
 * parameter layer owns range, exactly as it does for every other core.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "force_core.h"

namespace hypersaw
{

class StationCore
{
 public:
  static constexpr int kOps = 3;     // OP1..OP3
  static constexpr int kSlots = 4;   // OP1..OP3 + NS (matrix source rows, envelope count)
  static constexpr int kPoly = 16;   // SPEC §8 (the lab caps at 8 — divergence §11.4)
  static constexpr int kTable = 32;  // Wave RAM: 32 samples x 4 bit (§5)

  // The lab's full-double 2pi (reference/station.html:148) — NOT truncated.
  static constexpr double kTau = 6.283185307179586;
  // The lab's TRUNCATED 1/(2pi) (:267). See the header note: this literal is
  // load-bearing for the eps=1e-6 parity gate and station_check proves it.
  static constexpr double kPmConst = 0.1591549;
  // Wave-RAM mipmap: one band-limited table per ALLOWED-HARMONIC COUNT. A
  // 32-sample table carries at most 16 partials, so 17 levels (0..16) cover the
  // whole keyboard at any sample rate and the level index is just
  // floor(nyquist / f) clamped — no octave arithmetic to get wrong.
  static constexpr int kMipLevels = 17;
  static constexpr int kMipLen = 64;
  // DC blocker corner, in Hz, converted per sample rate (ADR-009 bans
  // hand-tuned per-tick constants).
  static constexpr double kDcHz = 5.0;
  // Release-fade for a stolen voice (§11.4). 2 ms of linear fade-to-zero before
  // the slot is reused; the stolen note is what waits, never the new one's
  // envelope shape.
  static constexpr double kStealFadeS = 0.002;
  // Matrix-cell smoothing time (§4: "~5 ms").
  static constexpr double kCellSmoothS = 0.005;

  enum Wave { kSin = 0, kTri, kSaw, kPls, kQtr, kDrw };
  enum Mode { kRatio = 0, kPitch, kFixed };
  enum Ring { kRingOff = 0, kRingOp1, kRingOp2 };

  struct Env
  {
    double a = 3, d = 420, s = 0.55, r = 260;  // ms, ms, level, ms
    int loop = 0;                              // §7 LOOP: cycle A->D->A->D while gated
    int step = 0;                              // §7 STEPPED: 0 = OFF, else 2..16 levels
  };

  struct Op
  {
    int on = 1, wave = kSin, mode = kRatio;
    double coarse = 1;    // RATIO, continuous 0.25..16 (§11.2)
    double fine = 0;      // cents
    double semis = 0;     // PITCH mode semitones
    double fixed = 220;   // FIXED mode Hz
    double lvl = 0.85, pan = 0, pw = 0.5, pure = 1;
    int qnt = 0;          // phase steps per cycle: 0 = OFF, else 4/8/16/32/64
    int sync = 0;         // ops 2/3 only: hard-sync to OP1's wrap
    int ring = kRingOff;  // ops 2/3 only: mix-path ring partner
    double phase = 0;     // §3.4 phase offset, 0..1 turns (§10 states 0..360 deg)
    int retrig = 1;       // 1 = RETRIG (lab behaviour), 0 = FREE
    Env env;
  };

  struct Noise
  {
    int on = 0, mode = 0;  // mode 0 = LONG (32767), 1 = SHORT (93)
    double rate = 0.35;
    int ktrk = 1;
    double lvl = 0.5, pan = 0;
    Env env{1, 120, 0, 80, 0, 0};
  };

  struct Patch
  {
    Op ops[kOps];
    Noise noise;
    double matrix[kSlots][kOps]{};  // [source][dest]; source 3 = NS
    struct
    {
      double amt = 0, dec = 80;
    } pitchEnv;
    uint8_t table[kTable]{};  // Wave RAM, values 0..15
    uint32_t seed = 1024;     // Wave RAM randomise seed (ADR-122), persists with device state

    // The lab's boot patch, verbatim (reference/station.html mkState()): a soft
    // EP -- OP2 at ratio 2 into OP1 at index 2.6, OP3 at ratio 14 at index 1.1.
    // SPEC-STATION §10's "Default patch = prototype boot patch".
    Patch()
    {
      ops[0].coarse = 1;  ops[0].fixed = 220; ops[0].lvl = 0.85;
      ops[0].env = Env{3, 420, 0.55, 260, 0, 0};
      ops[1].coarse = 2;  ops[1].fixed = 440; ops[1].lvl = 0;
      ops[1].env = Env{2, 180, 0, 120, 0, 0};
      ops[2].coarse = 14; ops[2].fixed = 880; ops[2].lvl = 0;
      ops[2].env = Env{2, 60, 0, 60, 0, 0};
      matrix[1][0] = 2.6;
      matrix[2][0] = 1.1;
    }
  };

  struct EnvState
  {
    int stage = 0;  // 0 A, 1 D, 2 S, 3 R, 4 done
    double lvl = 0;
  };

  struct Voice
  {
    int note = -1;
    double freq = 0;
    bool gate = false, active = false;
    double ph[kOps]{};    // [0,1) turns
    double prev[kSlots]{};  // the one-sample delay (SPEC §2)
    EnvState env[kSlots];
    double pT = 0;  // pitch-env sample counter; double, so it cannot overflow (audit §2.9)
    uint32_t lfsr = 0;
    double nphase = 0, nout = 0;
    long age = -1;
    // §11.4 release-fade stealing. `fadeGain` is exactly 1.0 unless this slot is
    // being stolen, so it is bit-inert for every parity scenario.
    double fadeGain = 1;
    bool stealing = false;
    int pendNote = -1;
    double pendFreq = 0;
  };

  explicit StationCore(double sampleRate) { setSampleRate(sampleRate); reset(); }

  // Public patch, in the SpectraCore idiom: the shell writes fields directly.
  // Writing `patch.table` by hand does NOT rebuild the mipmap — use setTable().
  Patch patch;

  // TEST HOOK, and the only one. `station_check` flips this to the true
  // 1/(2pi) to PROVE the truncated literal is load-bearing, then restores it.
  // Nothing in the shell may ever write it; it exists so the proof lives in an
  // oracle instead of in a comment nobody re-runs.
  double pmConst = kPmConst;

  // Audit S8 measured PLS at pw 0.1 sitting at -1.9 dB DC and SHORT noise at
  // -29.9 dB against a -61..-65 dB DC-free control — both envelope-multiplied at
  // the source, so every note-on is a thump and 16 voices sum theirs. The
  // human's ruling (2026-09-19) puts a blocker on the engine output and
  // SPEC-STATION §11.7 makes it a PARITY item. `dcBlock = false` is the bypass
  // the DC oracle row needs as its control, and is also how parity is taken
  // against a lab build that does not yet carry the blocker; the golden
  // manifest records which, measured behaviourally, never assumed.
  bool dcBlock = true;

  void setSampleRate(double sampleRate)
  {
    sr = sampleRate;
    stealFadeStep = 1.0 / std::max(1.0, kStealFadeS * sr);
    cellCoef = forcecore::onePoleCoef(kCellSmoothS, sr);
    // One-pole DC blocker y[n] = x[n] - x[n-1] + R*y[n-1]; R from the corner in
    // Hz, so the response is the same filter at every sample rate (ADR-009).
    // The lab derives R per render call because its audio graph assigns
    // `core.SR` AFTER construction, which would strand a cached 48 k pole on a
    // 44.1 k host. That failure mode does not exist here — `sr` is private and
    // setSampleRate is its only writer — so the coefficient is cached, and the
    // 44.1 k parity scenarios are what prove the two agree.
    dcR = std::exp(-kTau * kDcHz / sr);
    rebuildMip();
  }
  double sampleRate() const { return sr; }

  // Full reset: all voices silent, all filter/smoother state cleared, the
  // factory BELL table loaded (the lab's constructor does exactly this).
  void reset()
  {
    for (int i = 0; i < kPoly; i++) voices[i] = Voice{};
    noteCounter = 0;
    dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0;
    freePh[0] = freePh[1] = freePh[2] = 0;
    smoothPrimed = false;
    loadFactoryTable(kBell);
  }

  void allOff()
  {
    for (int i = 0; i < kPoly; i++) voices[i] = Voice{};
    dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0;
  }

  // ── Wave RAM ──────────────────────────────────────────────────────────────
  enum Factory { kFacSin = 0, kFacSaw, kFacSqr, kBell, kRnd };

  // The five prototype generators (:170-176). RND draws 32 values from one
  // mulberry32 stream reseeded from patch.seed, so a "randomise" click is one
  // reproducible draw rather than 32 free calls (ADR-122).
  void loadFactoryTable(Factory f)
  {
    uint32_t rng = patch.seed;
    for (int i = 0; i < kTable; i++)
    {
      double v = 0;
      switch (f)
      {
        case kFacSin: v = jsRound((std::sin(i / 32.0 * kTau) * .5 + .5) * 15); break;
        case kFacSaw: v = jsRound(i / 31.0 * 15); break;
        case kFacSqr: v = i < 16 ? 15 : 0; break;
        case kBell:
          v = jsRound(((std::sin(i / 32.0 * kTau) + .55 * std::sin(i / 32.0 * 3 * kTau)) * .42 + .5) * 15);
          break;
        case kRnd: v = std::floor(forcecore::rngNext(rng) * 16); break;
      }
      patch.table[i] = (uint8_t)std::max(0.0, std::min(15.0, v));
    }
    rebuildMip();
  }
  void setTable(const uint8_t *t)
  {
    std::memcpy(patch.table, t, kTable);
    rebuildMip();
  }
  void setTableSample(int i, int v)
  {
    patch.table[i & (kTable - 1)] = (uint8_t)std::max(0, std::min(15, v));
    rebuildMip();
  }

  // ── Notes ────────────────────────────────────────────────────────────────
  // Returns the slot. SPEC §10 declares no velocity parameter, so none is taken
  // here: an unused argument is invented surface, and the shell scales at the
  // mixer like every other core.
  int noteOn(int midi, double freq)
  {
    const int slot = alloc();
    Voice &v = voices[slot];
    if (v.active && !v.stealing)
    {
      // §11.4: the victim fades out over kStealFadeS and the new note starts in
      // the same slot when the fade reaches zero. The lab's `voices.shift()`
      // has no fade at all (a measured 68 %-of-peak step).
      v.gate = false;
      v.stealing = true;
      v.pendNote = midi;
      v.pendFreq = freq;
      return slot;
    }
    startVoice(v, slot, midi, freq);
    return slot;
  }
  void noteOff(int midi)
  {
    for (auto &v : voices)
      if (v.active && v.gate && v.note == midi) v.gate = false;
  }
  void noteOffSlot(int slot)
  {
    if (slot >= 0 && slot < kPoly && voices[slot].active) voices[slot].gate = false;
  }

  Voice &voiceAt(int slot) { return voices[slot]; }
  const Voice &voiceAt(int slot) const { return voices[slot]; }
  int activeVoices() const
  {
    int n = 0;
    for (const auto &v : voices) n += v.active ? 1 : 0;
    return n;
  }

  // ── Render ───────────────────────────────────────────────────────────────
  // WRITES (does not accumulate), matching the lab's `L[n]=l`. No allocation,
  // no wall-clock, no branch on denormals: the envelope's absolute exits snap
  // to zero long before the subnormal range (audit §2.9).
  void render(float *outL, float *outR, int nSamples)
  {
    const Patch &s = patch;
    if (!smoothPrimed)
    {
      std::memcpy(mtx, s.matrix, sizeof(mtx));
      smoothPrimed = true;
    }
    // FREE phase runs against middle C — the same reference KEYTRK already
    // uses — so it is one accumulator per op rather than one per voice. Skipped
    // entirely when every op is RETRIG, which is the default and the parity path.
    bool anyFree = false;
    for (int i = 0; i < kOps; i++) anyFree = anyFree || (s.ops[i].retrig == 0);
    const bool anySync = s.ops[1].sync || s.ops[2].sync;

    for (int n = 0; n < nSamples; n++)
    {
      for (int r = 0; r < kSlots; r++)
        for (int c = 0; c < kOps; c++) mtx[r][c] += (s.matrix[r][c] - mtx[r][c]) * cellCoef;
      if (anyFree)
        for (int i = 0; i < kOps; i++)
        {
          freePh[i] += opFreq(s.ops[i], 261.63) / sr;
          freePh[i] -= std::floor(freePh[i]);
        }

      double ml = 0, mr = 0;
      for (int vi = 0; vi < kPoly; vi++)
      {
        Voice &v = voices[vi];
        if (!v.active) continue;

        double pmul = 1;
        if (s.pitchEnv.amt != 0)
        {
          const double dec = std::max(0.005, s.pitchEnv.dec / 1000) * sr;
          pmul = std::pow(2, s.pitchEnv.amt * std::exp(-v.pT / dec * 4.6) / 12);
        }
        v.pT++;
        const double base = v.freq * pmul;

        double nSig = 0;
        if (s.noise.on)
        {
          const double nEnv = envStep(v.env[3], s.noise.env, v.gate);
          double nf = s.noise.rate * sr * 0.5;
          if (s.noise.ktrk) nf *= base / 261.63;
          v.nphase += nf / sr;
          while (v.nphase >= 1)
          {
            v.nphase -= 1;
            const uint32_t b0 = v.lfsr & 1u;
            const uint32_t tap = s.noise.mode ? ((v.lfsr >> 6) & 1u) : ((v.lfsr >> 1) & 1u);
            v.lfsr = (v.lfsr >> 1) | (((b0 ^ tap) & 1u) << 14);
            v.nout = (v.lfsr & 1u) ? 0.7 : -0.7;
          }
          nSig = v.nout * nEnv;
        }
        else
        {
          envStep(v.env[3], s.noise.env, v.gate);
        }

        // audit §3.1: the lab computes OP1's frequency twice per sample, and
        // `wrapped` is only read when OP2 or OP3 has SYNC on — false in the
        // default patch and in five of the six algorithm presets.
        const bool wrapped = anySync && (v.ph[0] + opFreq(s.ops[0], base) / sr) >= 1;
        double outs[kOps] = {0, 0, 0};
        for (int i = 0; i < kOps; i++)
        {
          const Op &o = s.ops[i];
          // Preserved lab defect (audit S9): an op switched OFF mid-note skips
          // envStep, so its envelope freezes and re-enabling clicks. SPEC §11
          // does not list it as a divergence, so it stays a parity item.
          if (!o.on) { outs[i] = 0; continue; }
          const double f = opFreq(o, base);
          // The Nyquist limit is in Hz and MUTES the operator rather than
          // detuning it (ADR-177 §3). A muted operator still steps its envelope,
          // so a pitch env sweeping back under Nyquist finds no stale level.
          if (f >= sr * 0.5) { outs[i] = 0; envStep(v.env[i], o.env, v.gate); continue; }
          const double dt = f / sr;
          // §3.4: sync resets to the phase OFFSET, not to 0. Default offset 0
          // is the lab's `v.ph[i]=0`.
          if (i > 0 && o.sync && wrapped) v.ph[i] = o.phase;
          v.ph[i] += dt;
          if (v.ph[i] >= 1) v.ph[i] -= 1;
          const double pm = mtx[0][i] * v.prev[0] + mtx[1][i] * v.prev[1] +
                            mtx[2][i] * v.prev[2] + mtx[3][i] * v.prev[3];
          double p = v.ph[i] + pm * pmConst;
          p -= std::floor(p);
          outs[i] = waveOut(o, p, dt, f) * envStep(v.env[i], o.env, v.gate);
        }
        v.prev[0] = outs[0];
        v.prev[1] = outs[1];
        v.prev[2] = outs[2];
        v.prev[3] = nSig;

        for (int i = 0; i < kOps; i++)
        {
          const Op &o = s.ops[i];
          if (!o.on || o.lvl <= 0) continue;
          // §3.4 RING: mix path only, never the matrix tap (prev[] is already
          // written above). kRingOff leaves the expression at *1.0 — exact.
          double mixv = outs[i];
          if (o.ring == kRingOp1) mixv *= outs[0];
          else if (o.ring == kRingOp2) mixv *= outs[1];
          const double g = mixv * o.lvl * 0.35 * v.fadeGain;
          ml += g * (1 - std::max(0.0, o.pan));
          mr += g * (1 - std::max(0.0, -o.pan));
        }
        if (s.noise.on && s.noise.lvl > 0)
        {
          const double g = nSig * s.noise.lvl * 0.35 * v.fadeGain;
          ml += g * (1 - std::max(0.0, s.noise.pan));
          mr += g * (1 - std::max(0.0, -s.noise.pan));
        }

        if (v.stealing)
        {
          v.fadeGain -= stealFadeStep;
          if (v.fadeGain <= 0) startVoice(v, vi, v.pendNote, v.pendFreq);
        }
        else if (!v.gate)
        {
          bool alive = false;
          for (int i = 0; i < kOps; i++)
            if (s.ops[i].on && v.env[i].stage < 4) alive = true;
          if (s.noise.on && v.env[3].stage < 4) alive = true;
          if (!alive) v.active = false;
        }
      }

      if (dcBlock)
      {
        double yl = ml - dcX[0] + dcR * dcY[0];
        double yr = mr - dcX[1] + dcR * dcY[1];
        // SPEC §12 "no branches on denormals (flush-to-zero)". A 5 Hz one-pole
        // takes ~2.6 s to decay from note level into the subnormal range and
        // then sits there: MEASURED 25 418 subnormal output samples in the 6 s
        // after note-off without this line. 1e-30 is -600 dB — eight orders
        // above the subnormal threshold and inaudible by any margin. The
        // comparison is a select, not a branch, on every compiler that matters.
        yl = std::fabs(yl) < 1e-30 ? 0.0 : yl;
        yr = std::fabs(yr) < 1e-30 ? 0.0 : yr;
        dcX[0] = ml;
        dcX[1] = mr;
        dcY[0] = yl;
        dcY[1] = yr;
        ml = yl;
        mr = yr;
      }
      outL[n] = (float)ml;
      outR[n] = (float)mr;
    }
  }

 private:
  // JS Math.round is floor(x+0.5) — it rounds HALF UP, where std::round rounds
  // half AWAY FROM ZERO. The BELL generator's argument goes negative, so the two
  // disagree there and the default Wave RAM would be off by one.
  static double jsRound(double x) { return std::floor(x + 0.5); }

  // The two `pow` elisions are PROVABLY bit-identical, not approximations:
  // IEEE-754 makes pow(2, +-0) exactly 1.0 and x * 1.0 exactly x. They matter
  // because `fine` is 0 in every factory patch, so the lab pays a `pow` per
  // operator per sample for a multiply by one (audit §3.1 counts 4 per sample).
  static double opFreq(const Op &o, double base)
  {
    if (o.mode == kRatio)
      return o.fine == 0 ? base * o.coarse : base * o.coarse * std::pow(2, o.fine / 1200);
    if (o.mode == kPitch)
    {
      const double st = o.semis + o.fine / 100;
      return st == 0 ? base : base * std::pow(2, st / 12);
    }
    return o.fixed;
  }

  static double blep(double t, double dt)
  {
    if (t < dt) { t /= dt; return t + t - t * t - 1; }
    if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
    return 0;
  }

  static double tri(double x) { return 1 - 4 * std::fabs(x - 0.5); }

  double waveOut(const Op &o, double p, double dt, double f) const
  {
    const double q = o.qnt > 1 ? (std::floor(p * o.qnt) / o.qnt) : p;
    const bool needRaw = o.pure < 1, needPure = o.pure > 0;
    double raw = 0, pure = 0;
    switch (o.wave)
    {
      case kSin:
        if (needRaw) raw = std::sin(q * kTau);
        if (needPure) pure = std::sin(p * kTau);
        break;
      case kTri:
        if (needRaw) raw = tri(q);
        if (needPure) pure = tri(p);
        break;
      case kSaw:
        if (needRaw) raw = 2 * q - 1;
        if (needPure) pure = 2 * p - 1 - blep(p, dt);
        break;
      case kPls:
        if (needRaw) raw = q < o.pw ? 1 : -1;
        if (needPure) pure = (p < o.pw ? 1 : -1) + blep(p, dt) - blep(std::fmod(p - o.pw + 1, 1.0), dt);
        break;
      case kQtr:
        // 16 amplitude levels (Game Boy CH3). SPEC §3.3 sanctions the pure
        // branch being a smooth triangle, which is why it equals kTri's.
        if (needRaw) raw = jsRound((tri(q) * .5 + .5) * 15) / 7.5 - 1;
        if (needPure) pure = tri(p);
        break;
      case kDrw:
      {
        if (needRaw)
        {
          const int i = (int)std::floor(q * 32) & 31;
          raw = patch.table[i] / 7.5 - 1;
        }
        // DIVERGENCE §11.1: band-limited, not the lab's linear interpolation.
        if (needPure) pure = mipRead(p, f);
        break;
      }
      default: break;
    }
    // audit §3.3: at PURE = 1 (the default) the raw branch is computed and
    // multiplied by zero — 2.3 M wasted sines/s at 16 voices. Skipping a branch
    // whose crossfade weight is exactly 0 is algebraically exact; that it is
    // also BIT-exact is what the parity rows prove, not what this comment claims.
    if (o.pure >= 1) return pure;
    if (o.pure <= 0) return raw;
    return raw + (pure - raw) * o.pure;
  }

  // Wave-RAM mipmap read. Level = how many of the table's <=16 partials fit
  // under Nyquist at this operator's frequency, so the branch is band-limited
  // by construction rather than by a filter.
  double mipRead(double p, double f) const
  {
    int lvl = (f > 0) ? (int)std::floor(sr * 0.5 / f) : kMipLevels - 1;
    lvl = std::max(0, std::min(kMipLevels - 1, lvl));
    const double x = p * kMipLen;
    const int i0 = (int)std::floor(x) & (kMipLen - 1);
    const int i1 = (i0 + 1) & (kMipLen - 1);
    const double fr = x - std::floor(x);
    const double *t = mip[lvl];
    return t[i0] * (1 - fr) + t[i1] * fr;
  }

  // Control rate, allocation free. The table is user-drawable live, so this is
  // called on every edit; 17 levels x 64 points x <=16 partials is ~35 k flops.
  void rebuildMip()
  {
    double a[kMipLevels] = {}, b[kMipLevels] = {};
    for (int i = 0; i < kTable; i++)
    {
      const double x = patch.table[i] / 7.5 - 1;
      a[0] += x / kTable;
      for (int k = 1; k < kMipLevels; k++)
      {
        const double th = kTau * k * i / kTable;
        const double sc = (k == kTable / 2) ? 1.0 / kTable : 2.0 / kTable;
        a[k] += sc * x * std::cos(th);
        b[k] += sc * x * std::sin(th);
      }
    }
    for (int lvl = 0; lvl < kMipLevels; lvl++)
      for (int j = 0; j < kMipLen; j++)
      {
        double y = a[0];
        for (int k = 1; k <= lvl; k++)
        {
          const double th = kTau * k * j / kMipLen;
          y += a[k] * std::cos(th) + b[k] * std::sin(th);
        }
        mip[lvl][j] = y;
      }
  }

  // Envelope, verbatim from the lab (:192-200) except for the STEPPED return.
  // The FORWARD-EULER coefficient 4.6/t is deliberate — see the header.
  double envStep(EnvState &e, const Env &p, bool gate) const
  {
    if (!gate && e.stage < 3) e.stage = 3;
    switch (e.stage)
    {
      case 0:
      {
        const double t = std::max(0.0005, p.a / 1000) * sr;
        e.lvl += 1 / t;
        if (e.lvl >= 1) { e.lvl = 1; e.stage = 1; }
        break;
      }
      case 1:
      {
        const double t = std::max(0.001, p.d / 1000) * sr;
        e.lvl += (p.s - e.lvl) * (4.6 / t);
        if (std::fabs(e.lvl - p.s) < 0.004) { e.lvl = p.s; e.stage = p.loop ? 0 : 2; }
        break;
      }
      case 2: e.lvl = p.s; break;
      case 3:
      {
        const double t = std::max(0.001, p.r / 1000) * sr;
        e.lvl += (0 - e.lvl) * (4.6 / t);
        if (e.lvl < 0.0005) { e.lvl = 0; e.stage = 4; }
        break;
      }
      default: break;
    }
    // §7 STEPPED: quantise the OUTPUT, never the state — quantising the state
    // would stall the one-pole at its first step.
    if (p.step >= 2) return jsRound(e.lvl * (p.step - 1)) / (p.step - 1);
    return e.lvl;
  }

  // SPEC §6 / §11.7: nonzero, per voice, deterministic per note. Copied from the
  // lab so the noise streams are bit-identical (ADR-177 §3).
  uint32_t lfsrSeed(int slot) const
  {
    uint32_t st = patch.seed ^ (uint32_t)((int32_t)((uint32_t)slot * 2654435761u));
    return (uint32_t)((int)std::floor(forcecore::rngNext(st) * 32767) | 1) & 0x7FFFu;
  }

  // Ascending-index-first, then oldest. From a fresh core the k-th note takes
  // slot k, which is what makes the LFSR seeds match the lab's `voices.length`.
  int alloc()
  {
    for (int i = 0; i < kPoly; i++)
      if (!voices[i].active) return i;
    int best = 0;
    for (int i = 1; i < kPoly; i++)
      if (voices[i].age < voices[best].age) best = i;
    return best;
  }

  void startVoice(Voice &v, int slot, int midi, double freq)
  {
    v = Voice{};
    v.note = midi;
    v.freq = freq;
    v.gate = true;
    v.active = true;
    v.age = noteCounter++;
    v.lfsr = lfsrSeed(slot);
    for (int i = 0; i < kOps; i++)
      v.ph[i] = patch.ops[i].retrig ? patch.ops[i].phase : (freePh[i] + patch.ops[i].phase);
  }

  double sr = 48000;
  double dcR = 0, dcX[2]{}, dcY[2]{};
  double cellCoef = 1, mtx[kSlots][kOps]{};
  bool smoothPrimed = false;
  double freePh[kOps]{};
  double stealFadeStep = 1;
  double mip[kMipLevels][kMipLen]{};
  Voice voices[kPoly];
  long noteCounter = 0;
};

}  // namespace hypersaw
