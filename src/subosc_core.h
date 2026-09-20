/*
 * subosc_core.h — SUB OSC: the honest sub. One oscillator, seven shapes, pitch
 * offsets, a one-pole tone, optional hard sync. No swarm, no coupling.
 *
 * STATUS: PORT PHASE 1 (core + oracle only). NOT wired into the shell, NO ids,
 * NO routing source row, NO GUI. The shell seam (source row 2 — ADR-178, the
 * eight reserved source rows of ADR-088's amendment) is phase 2 under a
 * separate brief. `tools/subosc_check.cpp` drives this header directly and
 * `./verify full` runs that chain.
 *
 * ── ONE INSTANCE PER VOICE, AND THAT IS THE CLASS'S ASSUMPTION ──────────────
 * SPEC-SUBOSC §1 ("one oscillator, one voice") and §8.2 (a routing SOURCE row)
 * make the sub a PER-VOICE source beside the two SWARM oscillators, so this
 * class holds exactly ONE note's state: one phase, one envelope, one filter,
 * one RNG stream. `noteOn` REPLACES the sounding note rather than layering it.
 * Phase 2 therefore instantiates one SubOscCore per voice and hands each its
 * own note and its own master phase, exactly as it does for an oscillator — it
 * does NOT instantiate one per device and expect polyphony from it. (The
 * parameters are per-device and identical across the instances; nothing in the
 * class assumes that, so a future per-voice parameter costs no restructuring.)
 *
 * ── WHAT "CORRECT" MEANS HERE ───────────────────────────────────────────────
 * Parity with reference/subosc.html's `SubOscCore` (ADR-003: the prototype IS
 * the spec-in-code) at the L0-1 bar, eps = 1e-6 RMS, at BOTH 48 000 and
 * 44 100 Hz, PLUS the behavioural rows SPEC-SUBOSC §10 states that parity
 * structurally cannot see. Never "plausible-sounding audio". The goldens are
 * rendered live out of the HTML by tools/golden/gen_subosc_goldens.mjs —
 * nothing is forked, so the reference cannot move without the gate noticing.
 *
 * ── PARITY-EXACT (every law and literal copied verbatim) ────────────────────
 * The whole DSP is parity-exact; there is no divergence in this port, which is
 * the point of a module born AFTER the SAW audit rather than before it (§9:
 * the lab already does what a port would otherwise have to fix). Named:
 *  · The seven shapes (§3), including the NAIVE triangle — declared limit L1,
 *    NOT a defect to fix here: fixing it in the port and not in the lab would
 *    break parity and hide the limit. The BLAMP is ruling R2's, at the lab.
 *  · The polyBLEP (`blep`) and the pulse-as-two-BLEP-saws construction — the
 *    repo's existing idiom, reference/swarmsaw.html:626-633 / swarm_core.h:974,
 *    copied rather than re-derived so the §10.1 alias floors stay comparable.
 *  · BUMP's peak normaliser: the 32-interval grid + 24-step bisection search of
 *    `subBumpPeak`, NOT the analytic bound 1/(1+a) (ADR-178 ruling R7; the
 *    bound left the shape up to 3.01 dB quiet — spec limit L7 records the
 *    superseded measurement). The search is reproduced step for step because
 *    a "better" root finder would land on a different double.
 *  · The pitch law, the 0.49*sr phase-increment cap, the tone clamp to
 *    0.45*sr, the TPT coefficient g/(1+g) with g = tan(pi*fc/sr), the linear AR
 *    in seconds, the sync reset TO THE START PHASE (§6, not to 0), the
 *    mulberry32 stream re-seeded at every note-on, and the 1e-20 flush floor.
 *  · `Math.round`'s half-UP rule for stepped parameters is reproduced as
 *    floor(x + 0.5) — std::round rounds half AWAY FROM ZERO, which would send
 *    octave -1.5 to -2 where the lab sends it to -1. (JS's one further quirk,
 *    Math.round(0.49999999999999994) == 0 where floor(x+0.5) == 1, is not
 *    reproduced: it is unreachable through a clamped parameter table and
 *    reproducing it would cost a branch on the audio-parameter path.)
 *
 * MEASURED, not assumed: `std::pow` and V8's `Math.pow` agree BIT-FOR-BIT on
 * the pitch law over all 9600 (note, octave, semitone, fine) combinations this
 * port can reach (checked 2026-09-19, this Mac), which is what lets the phase
 * increment — the one quantity an error in accumulates — be recomputed rather
 * than dumped into the golden manifest. `std::tan` differs from V8's by 1 ulp
 * on 5 of 15 (rate, cutoff) pairs; that is a 1e-16 relative move in one filter
 * coefficient and is 10 orders inside the eps = 1e-6 gate.
 *
 * ── NOT PORTED ──────────────────────────────────────────────────────────────
 * The lab's audio graph, its fake master-phase oscillator (§2: "the master
 * phase is an INPUT"; in the device the voice hands over oscillator 1's phase),
 * its tubes, its keyboard and the whole UI section.
 *
 * ── PROVISIONAL SURFACE (§5.3), flagged so the port is not read as a claim ───
 * `attack` and `release` are the LAB's, ported as the lab has them, and are
 * expected to be STRUCK at the shell: the voice envelope replaces them
 * (SPEC-SUBOSC §5.3 and open ruling R4). They are here because parity needs
 * them and because a note has to stop clicking while the module has no voice
 * around it — not because a per-module envelope is the design.
 *
 * ── HOUSE RULES ─────────────────────────────────────────────────────────────
 * Header-only, framework-free, no allocation after construction, no wall-clock,
 * mulberry32 (SPEC §5.7) and no other stream. Every time constant is a NUMBER
 * OF SECONDS converted at the current sample rate (ADR-009 / audit A1): there
 * is not one hand-tuned per-tick literal here. Every parameter enters through
 * ONE clamped table and `setParam` THROWS on an unknown key (§7 / audit A2) —
 * that throw is a programmer-error path, not a runtime one: the string form is
 * a control-path call and phase 2's id switch uses the `Param` overload, which
 * cannot name a key that does not exist. `recalc()` runs a bounded search (282
 * transcendentals, §3) and belongs on the control path with it.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

#include "force_core.h"  // mulberry32: SPEC §5.7's one permitted stream

namespace hypersaw
{

class SubOscCore
{
 public:
  // Waveform ids (§3). Stepped ⇒ structural under ADR-173's derivation.
  enum Wave { kSine = 0, kTri, kSquare, kSaw, kPulse, kNoise, kBump, kWaveCount };

  // §7's rows, in the lab's declaration order. The enum is the fast key; the
  // address strings live in the table beside it so there is exactly one list.
  enum Param
  {
    kWave = 0, kWidth, kBumpAmt, kBumpPhase, kOctave, kSemis, kFine, kLevel,
    kPhase, kKeytrack, kTone, kSync, kAttack, kRelease, kSeed, kParamCount
  };

  struct ParamSpec
  {
    const char *key;                 // §7's `Core key`
    double min, max, step, def;      // step 0 = continuous ⇒ morphable (ADR-173)
  };

  // THE TABLE IS THE ONLY WRITER (§7). Range, quantisation and default in one
  // place, with no second copy anywhere — the fix for audit A2, where the SAW
  // core's only cap lives in the shell's parameter row and every tool that
  // drives the core directly can walk `n` past kMaxV into a segfault.
  static constexpr ParamSpec kParamTable[kParamCount] = {
      {"wave", 0, kWaveCount - 1, 1, kSaw},
      {"width", 0.05, 0.95, 0, 0.5},
      // bump only. The 0.6 ceiling is not taste: above it the third harmonic
      // dominates and the shape stops reading as a sub at all.
      {"bumpAmt", 0, 0.6, 0, 0.35},
      {"bumpPhase", -3.141592653589793, 3.141592653589793, 0, -0.25},
      /* B181 note 1 (human 2026-09-20: "Sub should be able to reach 3 octaves
         below"). ONLY THE FLOOR MOVED — the default stays -1, so every stored
         patch renders bit-identically and nothing existing changes meaning.
         At -3 the lowest reachable fundamental is MIDI 0 less 36 semitones =
         1.02197 Hz: the phase increment there is 2.3e-5 at 44.1 kHz, six
         orders above the f32 normal floor, the tone stage and the BUMP peak
         search are both independent of f0, and the 0.49*sr increment cap only
         ever clamps the OTHER end. Measured, not assumed — subosc_check's
         bottom-octave row. */
      {"octave", -3, 0, 1, -1},
      {"semis", -12, 12, 1, 0},
      {"fine", -100, 100, 0, 0},
      {"level", 0, 1, 0, 0.8},
      {"phase", 0, 1, 0, 0},
      {"keytrack", 0, 1, 1, 1},
      {"tone", 30, 20000, 0, 20000},
      {"sync", 0, 1, 1, 0},
      {"attack", 0.0005, 0.5, 0, 0.005},
      {"release", 0.002, 2, 0, 0.08},
      {"seed", 0, 4294967295.0, 1, 1},
  };

  // Keytrack OFF pins the module to C2 (§4; open ruling R1) — a sub that
  // ignores the keyboard still has to sound at SOME pitch.
  static constexpr int kKeytrackOffMidi = 36;
  // Flush-to-zero floor for the tone filter state, -400 dB: inaudible by four
  // orders and the reason the release tail cannot decay into float32
  // subnormals (§10.5). `flushFloor` below is the oracle's handle on it.
  static constexpr double kFlush = 1e-20;
  static constexpr double kTwoPi = 6.283185307179586;
  static constexpr double kPi = 3.141592653589793;
  // BUMP peak search (§3): 32 intervals over [0, pi) — f' is a degree-3 trig
  // polynomial with at most 3 zeros there — then 24 bisection steps, which
  // closes each bracket to pi/32/2^24 = 6e-9 rad.
  static constexpr int kBumpPeakGrid = 32, kBumpPeakBisect = 24;

  explicit SubOscCore(double sampleRate) { setSampleRate(sampleRate); }

  // TEST HOOK, and the only one (the StationCore::pmConst idiom): the denormal
  // row plants 0 here to prove its detector can see the defect at all — with
  // the floor removed the 2 s tail idles in float32 subnormals. Nothing in the
  // shell may ever write it.
  double flushFloor = kFlush;

  /* ── THE SHELL'S PITCH INPUT, IN SEMITONES (B181 notes 2 and 4) ────────────
     A continuous offset summed into §4's pitch law, and deliberately NOT a §7
     parameter row. Three reasons, in order of weight:
       · §7's table is the only writer of range/step/default and the shell's id
         map is POSITIONAL (`id - 4000` IS the enum index) with the block's gate
         frozen at 4015 — a sixteenth table row would collide with a shipped
         CLAP id, and moving a frozen id to make room is the worse trade.
       · It is an INPUT, not a setting: the same category as `master`, the
         per-sample sync phase §2 already calls an input. The shell composes it
         from the sub's glide (note 2) and its pitch-mod offset (note 4) and
         writes ONE number per instance — one routing layer, not two writers of
         the same quantity (L0029).
       · The lab has no analogue and needs none: at 0 the law is `m + 0.0`,
         which is `m` exactly, so parity is untouched and every golden is inert.
     Unbounded here on purpose — the shell's parameter row declares the range a
     player can ask for, and the 0.49*sr increment cap below is what makes any
     value safe. */
  double pitchOffsetSt = 0;

  // Read-only observables, as the lab exposes them: the current envelope value
  // and the peak since a reader last cleared it. Written by render(), never by
  // the shell.
  double env = 0;
  double peak = 0;

  void setSampleRate(double sampleRate)
  {
    // A stubbed or absent rate must not silently produce NaN coefficients, so
    // the fallback is explicit (the lab's `typeof === 'number' && > 0` guard).
    sr_ = (std::isfinite(sampleRate) && sampleRate > 0) ? sampleRate : 44100;
    recalc();
  }
  double sampleRate() const { return sr_; }

  // ── parameters ────────────────────────────────────────────────────────────
  // One law, two keys. The enum overload is the implementation and the one the
  // shell's id switch will call; the string overload is §7's address form and
  // exists for the oracle, the golden manifest and any tool that drives the
  // core directly. An unknown address is an ERROR, not a silent no-op: a
  // silently-ignored setParam is how a control ships wired to nothing and no
  // audio oracle ever sees it (LIBRARY L0023).
  void setParam(Param p, double v)
  {
    const ParamSpec &d = kParamTable[p];
    double x = v;
    if (!std::isfinite(x)) x = d.def;
    // JS Math.round is half-UP, not half-away-from-zero — see the header.
    if (d.step != 0) x = std::floor(x / d.step + 0.5) * d.step;
    p_[p] = std::min(d.max, std::max(d.min, x));
    recalc();
  }

  void setParam(const char *key, double v)
  {
    const int i = indexOf(key);
    if (i < 0)
    {
      char msg[128];
      std::snprintf(msg, sizeof msg, "SubOscCore::setParam: unknown parameter \"%.64s\"", key);
      throw std::runtime_error(msg);
    }
    setParam((Param)i, v);
  }

  double param(Param p) const { return p_[p]; }

  // -1 when the address is not §7's. Public because the golden manifest's
  // loader wants to name the offending key before it fails.
  static int indexOf(const char *key)
  {
    for (int i = 0; i < kParamCount; i++)
    {
      const char *a = kParamTable[i].key, *b = key;
      while (*a && *a == *b) { a++; b++; }
      if (*a == 0 && *b == 0) return i;
    }
    return -1;
  }

  // Running frequency (§4). Recomputed per render call, never integrated, so it
  // is a pure function of the parameters and cannot make the output depend on
  // the block size (audit A10).
  double freqHz() const
  {
    const double base = p_[kKeytrack] != 0 ? midi_ : (double)kKeytrackOffMidi;
    // `pitchOffsetSt` is the shell's input (see the member's comment). At its
    // default 0 this is `m + 0.0`, which is bit-identical to the lab's law.
    const double m = base + 12 * p_[kOctave] + p_[kSemis] + pitchOffsetSt;
    return 440 * std::pow(2, (m - 69) / 12) * std::pow(2, p_[kFine] / 1200);
  }

  // ── note lifecycle ────────────────────────────────────────────────────────
  void noteOn(int midi, double vel = 1)
  {
    midi_ = midi;
    vel_ = std::min(1.0, std::max(0.0, std::isfinite(vel) ? vel : 1.0));
    ph_ = p_[kPhase];
    mPrev_ = -1;
    // Re-seeded per note (audit A3 / D3): the stream is a pure function of
    // (seed, note) and not of session history. The SAW engine's ensemble stream
    // is the counter-example — same seed, different history, RMS diff 0.137.
    rs_ = (uint32_t)p_[kSeed];
    stage_ = kStageAttack;
  }

  void noteOff()
  {
    if (stage_ != kStageIdle) stage_ = kStageRelease;
  }

  // Hard reset. Not a note event: for a host stop / voice steal. Total, by
  // design — §10.2 gates that five notes of history followed by allOff() is
  // bit-identical to a fresh core.
  void allOff()
  {
    stage_ = kStageIdle;
    env = 0;
    z_ = 0;
    ph_ = p_[kPhase];
    mPrev_ = -1;
  }

  // ── render ────────────────────────────────────────────────────────────────
  // `master` is an optional per-sample phase input in [0,1): the voice supplies
  // oscillator 1's phase (§2/§6). Null (or sync off) means no sync, which is
  // the lab's `p.sync === 1 && master` condition exactly. Mono by construction:
  // the same sample goes to both channels; pan and width are the voice's.
  void render(float *outL, float *outR, int n, const float *master = nullptr)
  {
    const int wave = (int)p_[kWave];
    const double w = p_[kWidth], start = p_[kPhase];
    const double bumpA = p_[kBumpAmt], bumpPhi = p_[kBumpPhase], bumpN = bumpNorm_;
    const double gain = p_[kLevel] * vel_;
    const double G = g_, atk = atkInc_, rel = relInc_;
    const bool syncOn = (p_[kSync] == 1 && master != nullptr);
    // Phase increment from the running frequency, capped just under Nyquist so
    // no fine/semis/octave combination can run the phase backwards.
    const double dph = std::min(freqHz(), 0.49 * sr_) / sr_;
    double ph = ph_, z = z_, e = env, pk = peak, mPrev = mPrev_;
    int stage = stage_;
    const double flush = flushFloor;

    for (int i = 0; i < n; i++)
    {
      ph += dph;
      ph -= std::floor(ph);
      if (syncOn)
      {
        const double m = master[i];
        if (mPrev >= 0 && m < mPrev) ph = start;  // master wrapped: hard reset
        mPrev = m;
      }

      double v = shapeAt(wave, ph, dph, w, bumpA, bumpPhi, bumpN, rs_);

      // Linear AR in seconds (§5.3, PROVISIONAL). Linear rather than
      // exponential because a linear ramp reaches exactly 0 and exactly 1 in
      // exactly attack/release seconds at every sample rate — no time constant
      // to drift, no denormal floor.
      if (stage == kStageAttack)
      {
        e += atk;
        if (e >= 1) { e = 1; stage = kStageSustain; }
      }
      else if (stage == kStageRelease)
      {
        e -= rel;
        if (e <= 0) { e = 0; stage = kStageIdle; }
      }

      v *= e * gain;

      // TPT one-pole lowpass (§5.1 / divergence D4 from the SAW engine's naive
      // pole). The envelope is applied BEFORE the filter so the filter's own
      // decay is what rings after the note ends — the tail §10.5 exercises.
      const double dv = (v - z) * G;
      const double y = dv + z;
      z = y + dv;
      if (z < flush && z > -flush) z = 0;

      outL[i] = (float)y;
      outR[i] = (float)y;
      const double a = y < 0 ? -y : y;
      if (a > pk) pk = a;
    }

    ph_ = ph;
    z_ = z;
    env = e;
    stage_ = stage;
    mPrev_ = mPrev;
    peak = pk;
  }

  /* ── THE SHAPE, AT ONE PHASE — §3's seven cases, and the ONLY copy ─────────
     Extracted from render()'s inner loop for B181 note 3, which needs the
     shape for a DISPLAY as well as for the audio. It was extracted rather
     than reimplemented for the display because this repo has already paid for
     the alternative twice: ADR-110 ("when they were two copies, any edit to
     one was a map that lied about the sound") and, three days ago, B177's GUI
     computing the LFO shape independently of the shell. A display that
     disagrees with the sound is worse than no display — it is a confident
     wrong answer — and the only structural cure is that there is nothing to
     disagree WITH.
     The body is render()'s verbatim; parity (eps 1e-6 over 60 goldens at two
     rates) and §10.2's bit-identity rows are what certify that the move
     changed no arithmetic. `rng` is stepped in place, so the noise case is a
     function of the stream's position exactly as it was inside the loop. */
  static double shapeAt(int wave, double ph, double dph, double w, double bumpA, double bumpPhi,
                        double bumpNorm, uint32_t &rng)
  {
    switch (wave)
    {
      case kSine: return std::sin(kTwoPi * ph);
      case kTri:
      {
        // Quarter-turn offset so the shape starts at 0 rising, like the sine
        // — a sub that starts at full scale is a click the start-phase
        // control cannot remove. NAIVE: no BLAMP (limit L1).
        double t = ph + 0.25;
        t -= std::floor(t);
        return 1 - 4 * std::fabs(t - 0.5);
      }
      case kSquare: return pulseAt(ph, dph, 0.5);
      case kSaw: return (2 * ph - 1) - blep(ph, dph);
      case kPulse: return pulseAt(ph, dph, w);
      case kBump: return bumpAt(ph, bumpA, bumpPhi, bumpNorm);
      // kNoise, and the lab's `default:` — noise ignores pitch entirely (L3).
      default: return 2 * forcecore::rngNext(rng) - 1;
    }
  }

  // The normaliser the current (a, phi) implies — the same reciprocal of the
  // same bounded search recalc() stores, so a reader outside the class can ask
  // for it without a second law or a second cached copy.
  double bumpNorm() const { return bumpNorm_; }

  // ── the shape laws, exposed for the oracle ────────────────────────────────
  // polyBLEP: one period of the correction, the exact idiom the repo already
  // trusts (reference/swarmsaw.html:626-633, ported at swarm_core.h:974-981).
  // Copied rather than re-derived: a second BLEP in the tree is a second thing
  // to keep in agreement, and §10.1's alias floors are only comparable to the
  // SAW engine's if the correction is the same one.
  static double blep(double ph, double dph)
  {
    const double d = std::max(dph, 1e-6);
    if (ph < d) { const double t = ph / d; return t + t - t * t - 1; }
    if (ph > 1 - d) { const double t = (ph - 1) / d; return t * t + t + t + 1; }
    return 0;
  }

  // Pulse of duty w as the DIFFERENCE OF TWO BLEP SAWS, so both edges carry the
  // same correction at every width (the construction swarm_core.h:1010-1029
  // already uses). w = 0.5 is the square, which is therefore not a second code
  // path.
  static double pulseAt(double ph, double dph, double w)
  {
    double p2 = ph - w;
    p2 -= std::floor(p2);
    const double s1 = (2 * ph - 1) - blep(ph, dph);
    const double s2 = (2 * p2 - 1) - blep(p2, dph);
    return (2 * w - 1) - (s1 - s2);
  }

  // BUMP (§3): a sine with a phase-shifted third harmonic, so one period reads
  // as a big bump followed by a slightly smaller one. NO BLEP AND NONE IS
  // NEEDED — two partials have nothing above 3*f0 to fold — which holds while
  // 3*f0 < Nyquist and lapses above f0 = sr/6 (limit L6).
  static double bumpAt(double ph, double a, double phi, double norm)
  {
    const double th = kTwoPi * ph;
    return (std::sin(th) + a * std::sin(3 * th + phi)) * norm;
  }

  // Peak of |sin(th) + a*sin(3th + phi)| over one period. NO CLOSED FORM: the
  // stationary points solve a sextic in tan(th/2). So it is a bounded search,
  // legitimate here only because it runs at PARAMETER-SET time and never per
  // sample. Half-wave antisymmetry (two ODD partials ⇒ y(th+pi) = -y(th)) makes
  // [0, pi) enough. The grid samples |f| as well as f' and seeds the running
  // peak with them, so a hypothetical missed bracket degrades the answer by
  // O(h^2) instead of returning something wrong — and the peak can never be
  // returned as 0, which is what makes the reciprocal safe by construction
  // rather than by a guard. Bisection, not Newton: no division, no escape from
  // the bracket, always converges.
  static double bumpPeak(double a, double phi)
  {
    const double h = kPi / kBumpPeakGrid;
    auto d = [&](double th) { return std::cos(th) + 3 * a * std::cos(3 * th + phi); };
    auto f = [&](double th) { return std::fabs(std::sin(th) + a * std::sin(3 * th + phi)); };
    double pk = f(0), d0 = d(0), th0 = 0;
    for (int i = 1; i <= kBumpPeakGrid; i++)
    {
      const double th1 = i * h, d1 = d(th1), f1 = f(th1);
      if (f1 > pk) pk = f1;
      if ((d0 < 0 && d1 > 0) || (d0 > 0 && d1 < 0))
      {
        double lo = th0, hi = th1, dlo = d0;
        for (int k = 0; k < kBumpPeakBisect; k++)
        {
          const double mid = 0.5 * (lo + hi), dm = d(mid);
          if ((dlo < 0 && dm < 0) || (dlo > 0 && dm > 0)) { lo = mid; dlo = dm; }
          else hi = mid;
        }
        const double fm = f(0.5 * (lo + hi));
        if (fm > pk) pk = fm;
      }
      d0 = d1;
      th0 = th1;
    }
    return pk;
  }

 private:
  enum Stage { kStageIdle = 0, kStageAttack, kStageSustain, kStageRelease };

  // Coefficients derived from SECONDS and HERTZ at the current sample rate
  // (ADR-009). Called from setParam and setSampleRate only — never from
  // render, which is why there is no transcendental in the sample loop.
  void recalc()
  {
    // Clamped BELOW Nyquist as well as at 20 kHz: tan(pi*fc/sr) diverges at
    // fc = sr/2, and a cutoff the user cannot reach is cheaper than a filter
    // that can blow up. Safety by construction, not by vigilance.
    const double fc = std::min(p_[kTone], 0.45 * sr_);
    const double g = std::tan(kPi * fc / sr_);
    g_ = g / (1 + g);
    atkInc_ = 1 / std::max(1.0, p_[kAttack] * sr_);
    relInc_ = 1 / std::max(1.0, p_[kRelease] * sr_);
    // The MEASURED peak is the normaliser (ADR-178 R7), so the bump peaks at
    // exactly 1 for every (a, phi) instead of at 0.75-0.71. bumpPeak returns a
    // value >= the grid maximum of |f|, bounded below by the shape's RMS
    // sqrt((1+a^2)/2) >= 0.707 — the reciprocal cannot divide by zero.
    bumpNorm_ = 1 / bumpPeak(p_[kBumpAmt], p_[kBumpPhase]);
  }

  double sr_ = 44100;
  double p_[kParamCount] = {kSaw, 0.5, 0.35, -0.25, -1, 0, 0, 0.8, 0, 1, 20000, 0, 0.005, 0.08, 1};
  double ph_ = 0;       // oscillator phase in [0,1)
  double z_ = 0;        // tone filter state
  double mPrev_ = -1;   // last master phase; < 0 = no edge yet this note
  double vel_ = 1;
  double g_ = 0, atkInc_ = 0, relInc_ = 0, bumpNorm_ = 1;
  int stage_ = kStageIdle;
  int midi_ = 60;
  uint32_t rs_ = 1;     // mulberry32 state, stepped in place
};

}  // namespace hypersaw
