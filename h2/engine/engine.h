/*
 * engine.h — horde 2's composed engine: horde's swarm drives the member
 * trajectories and SCALPEL's blades are evaluated on them. ROADMAP B385; design
 * and parity plan: docs/port/h2-engine.md.
 *
 * PARITY TARGET (ADR-187 item 3): docs/design/scalpel-horde-engine.js over
 * reference/scalpel/prototype/razor-core.js and reference/swarmsaw.html's
 * SwarmSynth, at the blobs h2/README.md pins. Checked by
 * tools/h2_engine_parity_check.cpp. The blade path is copied from
 * h2/cores/scalpel/razor_core.h (phase 1a's proven port), the swarm law is in
 * swarm.h, and the composition is transcribed from the composed JS. Where the JS
 * composes by subclassing RazorCore and swapping one of its statics, this is one
 * class and each override is the code at its call site.
 *
 * WHAT THE COMPOSITION IS, in the order the render meets it:
 *   - Two phase integrators on purpose: the swarm advances its own phase φ_H
 *     with SwarmSynth's arithmetic, and each blade reads φ_S, started at
 *     frac(φ_H + ½) (horde's saw jumps at 0, SCALPEL's at ½) and advanced by the
 *     blade step with the same increments.
 *   - The voice law is horde's (B310, ADR-083): a note takes a free slot, else the
 *     quietest releasing tail, else the oldest held voice. A repeated note never
 *     reuses the sounding one, so its release keeps ringing.
 *   - The voice cap (B323) is an INPUT, not a clock read. Over it, the quietest
 *     releasing tails fade out over 8 ms (a linear ramp, never a cut), and a held
 *     note is never culled. At a full cap, capPolicy (B375) refuses the note
 *     (default), steals the oldest held voice with the fade, or replaces it at once.
 *   - The first tick (B325): a note-on takes a look-ahead swarm tick so the member
 *     frequencies exist before the first sample reads them. The real first tick
 *     then runs from the restored swarm where it always would have, so the swarm's
 *     trajectory does not depend on the look-ahead.
 *   - ADR-184 A2: Rotate spread is bipolar, and a Quantized Cut spread rounds half
 *     away from zero, so a negative spread mirrors a positive one exactly.
 *   - Gravity (B335, ADR-008, ADR-086 + A1): held voices settle toward just
 *     ratios on a fixed-time grid, between render segments, so the trajectory is
 *     independent of the host's block size.
 *   - The ensemble (B335, ADR-077, ADR-078): onset scatter with timing correction,
 *     attack and release scatter, and per-partial envelopes, from a persistent
 *     seeded stream whose offsets carry across notes.
 *   - ADR-189 D1-D3, each off by default: carrier ADAA, a BLEP scanner that
 *     follows the xin phase push, and a 5 kHz one-pole in the feedback loop.
 *
 * NUMBERS: doubles, built -ffp-contract=off (h2 rule 7: the shipped build is the
 * tested build), with JS semantics where they differ from C++ (js.h).
 * RANDOMNESS: four mulberry32 streams, each consumed in the JS's order: the
 * host-seeded stream (seedRandom; the JS's Math.random), the placement stream,
 * one per swarm, and the ensemble stream.
 * REAL TIME: every buffer is in the object. render() allocates nothing, locks
 * nothing and reads no clock. set(), setString() and the note and cap calls run
 * on the render thread BETWEEN render() calls (a CLAP host delivers parameter
 * events inside process()), never concurrently with one; none allocates.
 * DOMAIN: N is held to 1..9, a poly pool of 0 ignores note-ons, and notes are
 * 0..127. Inside the domain these clamps never act.
 */
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "blade.h"
#include "js.h"
#include "swarm.h"

namespace horde2::engine {

class Engine {
 public:
  static constexpr int kVoices = 8;
  static constexpr double TAU = 6.283185307179586;

  EventLog* events = nullptr;   // parity-check counters; null in any product build
#ifdef H2_ENGINE_FAULTS
  // Must-fail controls for tools/h2_engine_parity_check.cpp only. Undefined
  // elsewhere, every site folds to `false` and H2E_EPS(x) to x.
  int fault = 0;
  double faultEps = 0;
#define H2E_FAULT(n) (fault == (n))
#define H2E_EPS(x) (fault == 0 ? (x) * (1 + faultEps) : (x))
  // X1 (fault 13) scales the ring's xm by 1 + 1e-9 from the first sample; X1-late
  // (14) from this frame on, so its first frames match the clean render bit for
  // bit (ADR-187 A2). evSample is the engine's own count of samples rendered.
  // X1-late's scale is 1 + faultEps when faultEps is set (the printed margin
  // sweep), else the judged 1 + 1e-9.
  static constexpr uint64_t kX1LateFrame = 128;
#define H2E_EPS13(x) (fault == 13 ? (x) * (1 + 1e-9) \
                     : (fault == 14 && evSample >= kX1LateFrame) ? (x) * (1 + (faultEps != 0 ? faultEps : 1e-9)) : (x))
  void armFaults() { if (fault == 9) field.faultKsm(0.08); }
#else
#define H2E_FAULT(n) false
#define H2E_EPS(x) (x)
#define H2E_EPS13(x) (x)
#endif
#ifdef H2_ENGINE_STAGES
  // B441 cost attribution, for tools/measure_h2_engine.cpp --stages ONLY (its own
  // CMake target, measure_h2_engine_stages). Each set bit SKIPS one stage, so the
  // stage's cost reads as the full render's time minus the time without it. The
  // output is wrong by design while a bit is set. Undefined elsewhere (the shipped,
  // parity and ledger builds), every site folds to `false` and compiles to the code
  // that was there before: parity is untouched by construction, and checked.
  enum Stage : unsigned {
    kStageTick = 1,      // the swarm's control tick, per voice every 16 samples (tickSwarm)
    kStagePhase = 2,     // the swarm's per-sample phase advance (phi_H); the blade step still reads dph
    kStageBlade = 4,     // the blade evaluation at the step's end phase (out(): both blades, twins)
    kStageBlep = 8,      // every PolyBLEP: tryE/scan and the two probe evaluations per edge (hAt)
    kStageDc = 16,       // the per-cycle DC estimate's refresh (dcEst / dcPair)
    kStageDecim = 32,    // the output biquads that run at the oversampled rate
    kStageOut = 64,      // the DC blocker and the output tanh
    kStageCouple = 128,  // couple() + spread(), per active voice every 32 samples
  };
  unsigned stageOff = 0;
#define H2E_SKIP(bit) ((stageOff & Engine::bit) != 0u)
#else
#define H2E_SKIP(bit) false
#endif

  explicit Engine(double sampleRate) : sr(sampleRate), field(sampleRate) {
    for (int vi = 0; vi < kVoices; vi++) {
      for (int i = 0; i < kMembers; i++) {
        Member& m = voices[vi].m[i];
        m.i = i;
        m.ns.seed = i * 97; m.ns2.seed = i * 97 + 7;
        m.bx.g = &m.g2; m.bx.ns3.seed = i * 97 + 13; m.bx.ns4.seed = i * 97 + 19;
        m.ns.aaReal = m.ns2.aaReal = m.bx.ns3.aaReal = m.bx.ns4.aaReal = true;
      }
    }
    sc.inside = true; sc.draws = false;
    sc2.inside = true; sc2.draws = false;
    bxs.k = 1; bxs.ns3.inside = true; bxs.ns3.draws = false; bxs.ns4.inside = true; bxs.ns4.draws = false;
    gravGrid = static_cast<int>(js::max(1, js::round(sr * 256 / 44100)));
    setOS(2);
  }
  // Members point into their own voice (bx.g -> g2), so a copy would alias.
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  // The host-seeded stream (the JS host installs mulberry32(seed) as Math.random).
  void seedRandom(uint32_t seed) { rng.a = seed; }

  // One key. Returns false for a key the engine does not have.
  bool set(const char* key, double value) {
    if (std::strcmp(key, "rotSpread") == 0 || std::strcmp(key, "rotSpread2") == 0) {
      // A2: the blade law gates rotation on the magnitude; the sign is applied in spread()
      (key[9] == '2' ? rotSign2 : rotSign1) = value < 0 ? -1 : 1;
      value = std::fabs(value);
    }
    if (std::strcmp(key, "polyMode") == 0 && value != d.polyMode) {
      for (Voice& v : voices) { v.gate = false; v.stage = 4; }
      stackN = 0;
    }
    if (std::strcmp(key, "os") == 0) { if (value != os) setOS(value); return true; }
    if (double SP::*p = tKey(key)) { t.*p = value; return true; }
    if (double DP::*p = dKey(key)) {
      d.*p = value;
      if (p == &DP::N) d.N = js::min(kMembers, js::max(1, std::floor(d.N)));   // domain
      return true;
    }
    return false;
  }
  bool setString(const char* key, const char* value) {
    if (std::strcmp(key, "kCustom") != 0) return false;
    std::strncpy(d.kCustom, value, DP::kCustomCap - 1);
    d.kCustom[DP::kCustomCap - 1] = 0;
    parseCustom();
    for (int r = 0; r < 10; r++) rlValid[9][r] = false;
    return true;
  }
  // Start a patch AT its values instead of gliding in from the defaults.
  void snap() { for (int i = 0; i < kTKeys; i++) s.*tKeys()[i].p = t.*tKeys()[i].p; }

  // The host's voice cap (0 = none) and the policy at a full cap: 0 refuse, 1 steal, 2 replace.
  void setVoiceCap(double n) { voiceCap = js::max(0, std::floor(js::truthy(n) ? n : 0)); }
  void setCapPolicy(double n) { const double x = std::floor(js::truthy(n) ? n : 0); capPolicy = x == 1 || x == 2 ? x : 0; }

  void noteOn(int note, double freq, double vel) {
    if (note < 0 || note > 127) return;   // domain
    if (js::truthy(d.polyMode)) { monoOn(note, freq, vel); return; }
    const int P = poolSize();
    if (P == 0) return;                   // domain
    Voice* v = nullptr;
#ifdef H2_ENGINE_FAULTS
    if (fault == 6) { faultOracleVoiceLaw(note, freq, vel, P); return; }
#endif
    // At the cap a note replaces a sounding voice instead of adding one, by the
    // same tiers; below it (or with no cap) this is B310's law exactly.
    const bool busy = voiceCap > 0 && liveCount() >= voiceCap;
    v = tierPick(P, busy ? kSkipInactive : kSkipNone);
    if (!v && busy && capPolicy != 2) {
      if (capPolicy != 1) { refused_++; return; }
      // STEAL: release the oldest held voice into the cull's fade (so it no longer
      // counts as live) and play the note in a free slot. With none free, fall
      // through to horde's full-pool law.
      Voice* h = nullptr;
      for (int i = 0; i < P; i++) { Voice& x = voices[i]; if (x.active && x.gate && !x.cull && (!h || x.age < h->age)) h = &x; }
      Voice* f = tierPick(P, kSkipActive);
      if (h && f) {
        h->gate = false; h->stage = 4;
        h->cull = true; h->cullG = 1; stolen_++;
        v = f;
      }
    }
    if (!v) for (int i = 0; i < P; i++) { Voice& x = voices[i]; if ((!busy || x.active) && (!v || x.age < v->age)) v = &x; }
    if (!v) return;   // domain: a cap counting voices outside a shrunk pool
    startVoice(*v, note, freq, vel, true, true);
    v->freq = v->freqT = freq;
    couple(*v);
    spread(*v);
  }
  void noteOff(int note) {
    if (js::truthy(d.polyMode)) {
      stackRemove(note);
      Voice& v = voices[0];
      if (!v.active || !v.gate || v.note != note) return;
      if (stackN) {   // fall back to the newest key still held, gliding there without a retrigger
        const int n = stack[stackN - 1];
        v.note = n; v.freqT = nf[n];
        if (s.glide <= 1) v.freq = v.freqT;
      } else { v.gate = false; v.stage = 4; }
      return;
    }
    // every gated voice with this key: a host that merges two offs cannot strand a note
    for (Voice& v : voices) if (v.active && v.note == note && v.gate) { v.gate = false; v.stage = 4; }
  }
  void retune(int note, double freq) { for (Voice& v : voices) if (v.active && v.note == note) v.freq = freq; }
  void panic() { for (Voice& v : voices) { v.gate = false; v.stage = 4; } }

  void render(double* L, double* R, int n) {
    if (voiceCap > 0) cull();
    if (!(d.grav >= 0.005)) {
      renderBlock(L, R, n);
      gravAccum = (gravAccum + n) % gravGrid;
    } else {
      // segments end on gravity's fixed-time grid; gravity steps between them
      for (int done = 0; done < n;) {
        const int seg = n - done < gravGrid - gravAccum ? n - done : gravGrid - gravAccum;
        renderBlock(L + done, R + done, seg);
        gravAccum += seg; done += seg;
        if (gravAccum >= gravGrid) { gravityStep(gravGrid / sr); gravAccum = 0; }
      }
    }
    // keep-phase: the newest sounding swarm
    const Voice* lv = nullptr;
    for (const Voice& v : voices) if (v.active && (!lv || v.age > lv->age)) lv = &v;
    if (lv) std::memcpy(field.lastPhase, lv->sw.phase, sizeof field.lastPhase);
  }

  // host-facing load readouts: tails culled, notes refused and held voices stolen
  double culled() const { return culled_; }
  double refused() const { return refused_; }
  double stolen() const { return stolen_; }
  const Voice& voice(int i) const { return voices[i]; }

 private:
  // ---- parameter tables -------------------------------------------------------
  struct TKey { const char* k; double SP::* p; bool perSample; };
  struct DKey { const char* k; double DP::* p; };
  static constexpr int kTKeys = 64;
  // The 64 target keys in the oracle's order. perSample marks the 12 the render
  // glides every sample instead of every 16.
  static const TKey* tKeys() {
    static const TKey K[kTKeys] = {
      {"b2mix", &SP::b2mix, true}, {"colK", &SP::colK, false}, {"colB", &SP::colB, false},
      {"I2", &SP::I2, false}, {"m2", &SP::m2, false}, {"mHz2", &SP::mHz2, false},
      {"morph2", &SP::morph2, false}, {"mspread2", &SP::mspread2, false}, {"ispread2", &SP::ispread2, false},
      {"rotRate2", &SP::rotRate2, false}, {"rotSpread2", &SP::rotSpread2, false}, {"kRuleAmt2", &SP::kRuleAmt2, false},
      {"bspread2", &SP::bspread2, false}, {"kspread2", &SP::kspread2, false}, {"wspread2", &SP::wspread2, false},
      {"dspread2", &SP::dspread2, false}, {"benvA2", &SP::benvA2, false}, {"benvD2", &SP::benvD2, false},
      {"benvK2", &SP::benvK2, false}, {"benvW2", &SP::benvW2, false}, {"benvVel2", &SP::benvVel2, false},
      {"kRuleAmt", &SP::kRuleAmt, false}, {"w2", &SP::w2, true}, {"k2", &SP::k2, true},
      {"kHz2", &SP::kHz2, true}, {"c2", &SP::c2, true}, {"depth2", &SP::depth2, false},
      {"hard2", &SP::hard2, false}, {"xm", &SP::xm, false}, {"fb", &SP::fb, false},
      {"benvA", &SP::benvA, false}, {"benvD", &SP::benvD, false}, {"benvK", &SP::benvK, false},
      {"benvW", &SP::benvW, false}, {"benvVel", &SP::benvVel, false}, {"glide", &SP::glide, false},
      {"driftRate", &SP::driftRate, false}, {"morph", &SP::morph, false}, {"wspread", &SP::wspread, false},
      {"dspread", &SP::dspread, false}, {"mspread", &SP::mspread, false}, {"ispread", &SP::ispread, false},
      {"w", &SP::w, true}, {"k", &SP::k, true}, {"kHz", &SP::kHz, true},
      {"mHz", &SP::mHz, false}, {"c", &SP::c, true}, {"rotRate", &SP::rotRate, false},
      {"rotSpread", &SP::rotSpread, false}, {"hard", &SP::hard, true}, {"depth", &SP::depth, true},
      {"I", &SP::I, true}, {"m", &SP::m, false}, {"gain", &SP::gain, false},
      {"detune", &SP::detune, false}, {"K", &SP::K, false}, {"bspread", &SP::bspread, false},
      {"kspread", &SP::kspread, false}, {"width", &SP::width, false}, {"A", &SP::A, false},
      {"D", &SP::D, false}, {"S", &SP::Sus, false}, {"R", &SP::R, false}, {"bend", &SP::bend, false},
    };
    return K;
  }
  static double SP::* tKey(const char* k) {
    for (int i = 0; i < kTKeys; i++) if (std::strcmp(tKeys()[i].k, k) == 0) return tKeys()[i].p;
    return nullptr;
  }
  static double DP::* dKey(const char* k) {
    static const DKey K[] = {
      {"mode", &DP::mode}, {"hot", &DP::hot}, {"base", &DP::base}, {"lock", &DP::lock}, {"N", &DP::N},
      {"phaseMode", &DP::phaseMode}, {"poly", &DP::poly}, {"mshape", &DP::mshape}, {"kq", &DP::kq},
      {"fmType", &DP::fmType}, {"mUnit", &DP::mUnit}, {"mirror", &DP::mirror}, {"dcMode", &DP::dcMode},
      {"law", &DP::law}, {"frame", &DP::frame}, {"frame2", &DP::frame2}, {"rot2Follow", &DP::rot2Follow},
      {"lock2", &DP::lock2}, {"mirror2", &DP::mirror2}, {"b2fm", &DP::b2fm}, {"fmType2", &DP::fmType2},
      {"mshape2", &DP::mshape2}, {"mUnit2", &DP::mUnit2}, {"cScale", &DP::cScale}, {"rotSync", &DP::rotSync},
      {"panOrder", &DP::panOrder}, {"aa", &DP::aa}, {"polyMode", &DP::polyMode},
      {"glideAlways", &DP::glideAlways}, {"b2on", &DP::b2on}, {"mode2", &DP::mode2}, {"hot2", &DP::hot2},
      {"kRule", &DP::kRule}, {"b2sp", &DP::b2sp}, {"kRule2", &DP::kRule2}, {"b2env", &DP::b2env},
      {"b2order", &DP::b2order},
      {"dist", &DP::dist}, {"seed", &DP::seed}, {"h.law", &DP::hLaw}, {"bpm", &DP::bpm},
      {"beatMult", &DP::beatMult}, {"harmReach", &DP::harmReach}, {"stretchB", &DP::stretchB},
      {"spread", &DP::spread}, {"anchor", &DP::anchor}, {"onset", &DP::onset}, {"dissolve", &DP::dissolve},
      {"driftDepth", &DP::driftDepth}, {"h.driftRate", &DP::hDriftRate}, {"driftMode", &DP::driftMode},
      {"motionCenter", &DP::motionCenter}, {"inertia", &DP::inertia}, {"inertiaCurve", &DP::inertiaCurve},
      {"freqGlide", &DP::freqGlide}, {"keepPhase", &DP::keepPhase}, {"pivotMode", &DP::pivotMode},
      {"grav", &DP::grav}, {"basin", &DP::basin}, {"onsetScatter", &DP::onsetScatter},
      {"onsetAlpha", &DP::onsetAlpha}, {"attackScatter", &DP::attackScatter}, {"voiceEnv", &DP::voiceEnv},
      {"relScatter", &DP::relScatter}, {"aaCarrier", &DP::aaCarrier}, {"aaXin", &DP::aaXin},
      {"aaLoop", &DP::aaLoop},
    };
    for (const DKey& e : K) if (std::strcmp(e.k, k) == 0) return e.p;
    return nullptr;
  }

  // ---- the blade (shapes, modulators, the blade evaluation) -------------------
  double rnd() { return rng.next() * 2 - 1; }
  double nsRnd(NS& ns) { return ns.draws ? rnd() : ns.val; }
  // Pure: reads `sh` only through js::sel(sh), and the members gmr/gmn only in case 6.
  // B441 C2 relies on exactly this to reuse one blade's base wave for the other.
  double wave(double sh, double x) const {
    switch (js::sel(sh)) {
      case 0: return std::sin(6.283185307179586 * x);
      case 1: return x < 0.25 ? 4 * x : (x < 0.75 ? 2 - 4 * x : 4 * x - 4);
      case 2: { const double y = x + 0.5; return 2 * (y - std::floor(y)) - 1; }
      case 4: { const double y = x + 0.5; return 1 - 2 * (y - std::floor(y)); }   // ramp down
      case 6: {
        // sine -> saw: the closed form of sum r^n sin(nθ)/n, r capped per member so
        // the harmonics die out before Nyquist
        const double r = js::truthy(gmr) ? gmr : 0;
        if (r < 1e-4) return std::sin(6.283185307179586 * x);
        const double th = 6.283185307179586 * (x + 0.5);
        return -std::atan2(r * std::sin(th), 1 - r * std::cos(th)) / gmn;
      }
      default: return x < 0.5 ? 1 : -1;
    }
  }
  // modulator shapes; x is an unbounded phase, so noise keeps moving in free mode
  double mod(double shape, double x, double seed) const {
    if (shape == 7) return blade::hash(std::floor(x) + seed);   // S&H noise, new each cycle
    if (shape == 5) {
      const double i = std::floor(x), f = x - i, sm = f * f * (3 - 2 * f);
      const double a = blade::hash(i + seed), b = blade::hash(i + 1 + seed);
      return a + (b - a) * sm;
    }
    return wave(shape, x - std::floor(x));
  }
  // the held level at hold-phase hp; with slew, each step glides in from the previous
  double crushLevel(double sh, double st, double kk, double hp, double sl) const {
    if (hp < 0) return wave(sh, st - std::floor(st));
    const double j = std::floor(hp), f = hp - j, a = blade::crushAvg(sh, st, kk, j);
    if (sl > 0.001 && f < sl) {
      const double prev = j == 0 ? wave(sh, st - std::floor(st)) : blade::crushAvg(sh, st, kk, j - 1);
      return prev + (a - prev) * f / sl;
    }
    return a;
  }
  // B441 C2: the base wave of shape `a` at phase x equals that of shape `b` at the same x
  // whatever gmr/gmn hold, when both select the same case and that case is not 6 (the
  // only case reading them). Then one evaluation serves both blades.
  static bool sharesBase(double a, double b) { const int k = js::sel(a); return k == js::sel(b) && k != 6; }
  // the pitch-FM integrator: advances ns.acc, returns the unwrapped increment
  double fmStep(const Blade& p, NS& ns, double e, bool entered, double kk, double modX, double dphi) {
    if (p.fmType != 1 || !blade::isFM(p)) return 0;
    if (entered && p.mode == 1) ns.acc = 0;
    const double mv = mod(p.mshape, p.mode == 1 ? p.mEff * e : modX, ns.seed);
    // exponential deviation: ±(1 + I/10) as a ratio at full swing (symmetric in cents)
    const double dd = (js::pow(1 + 0.1 * p.I, mv) - 1) * kk * dphi;
    ns.acc += dd; ns.acc -= std::floor(ns.acc);
    return dd;
  }
  // One blade at phase phi. `inp` is what it transforms: the base wave, or in
  // serial interplay base + λ·(the lower blade).
  // `base` is wave(p.base, phi) under the gmr/gmn in force for this blade. The caller
  // computes it (B441 C2), so one evaluation serves the blade, the `- base` its caller
  // takes beside it (twins, blade 2, the DC estimates) and, when sharesBase holds,
  // blade 2: each phase's base is evaluated once per output, not up to three times.
  // It is the same pure call on the same inputs, so not a bit moves.
  double voiceOut(const Blade& p, double phi, double base, double c, double k, double modX, NS& ns, bool hasInp = false,
                  double inp = 0) {
    if (aaOn && ns.aaReal) return voiceAA(p, phi, base, c, k, modX, ns, hasInp, inp);
    return voicePlain(p, phi, base, c, k, modX, ns, hasInp, inp);
  }
  double voicePlain(const Blade& p, double phi, double base, double c, double k, double modX, NS& ns, bool hasInp, double inp) {
    const double xin0 = !hasInp ? base : inp;
    const double w = p.w;
    if (w < 0.004) { ns.g = 0; ns.inside = false; return xin0; }
    double st = c - w * 0.5; st -= std::floor(st);
    double e = phi - st; if (e < 0) e += 1;
    if (e >= w) { ns.g = 0; ns.inside = false; return xin0; }
    const double kk = p.lock == 1 ? k / w : k;
    const double er = p.mirror == 1 && e > w * 0.5 ? w - e : e;   // reflect: a palindrome blade
    const double hp = kk * er;
    double hot = 0;
    switch (js::sel(p.mode)) {
      // ns.cacc: collision pitch's extra carrier phase; ns.ov: overlap with the other blade (bite)
      case 0: { const double cp = hp + ns.xin + ns.cacc; hot = wave(p.hot, cp - std::floor(cp)); break; }
      case 1: case 2: {
        const double Ib = ns.ov > 0 ? p.I * (1 + 4 * p.colB * ns.ov) : p.I;
        const double cp = ns.xin + ns.cacc + (p.fmType == 1 ? hp + ns.acc
          : hp + Ib * 0.15915494309189535 * mod(p.mshape, p.mode == 1 ? p.mEff * er : modX, ns.seed));
        hot = wave(p.hot, cp - std::floor(cp)); break;
      }
      case 3: {
        const double idx = std::floor(hp);
        if (!ns.inside || idx != ns.idx) { ns.idx = idx; ns.val = nsRnd(ns); }
        hot = ns.val; break;
      }
      case 4: hot = std::sin(1.5707963267948966 * (1 + (k - 1) * 0.25) * (ns.ov > 0 ? 1 + 2 * p.colB * ns.ov : 1) * xin0); break;   // fold
      case 5: { const double cp = hp + ns.xin + ns.cacc; hot = xin0 * wave(p.hot, cp - std::floor(cp)); break; }                  // ring
      case 6: {   // crush: each hold level is the base's average over its interval
        const double wEff = p.mirror == 1 ? w * 0.5 : w, hpEnd = kk * wEff, sl = p.hard;
        hot = crushLevel(p.base, st, kk, hp, sl);
        if (sl > 0.001) {   // land on the base at the exit over the last `slew` intervals
          const double rlE = js::min(sl, hpEnd), h0 = hpEnd - rlE;
          if (rlE > 1e-9 && hp > h0) {
            const double from = crushLevel(p.base, st, kk, h0, sl);
            const double x = st + wEff, tgt = wave(p.base, x - std::floor(x));
            hot = from + (tgt - from) * (hp - h0) / rlE;
          }
        }
        if (hasInp) {   // serial: the lower blade's content is sampled and held per step
          const double extra = inp - base, j = std::floor(hp), f = hp - j;
          if (!ns.inside || j != ns.idx) { ns.pv = ns.inside ? ns.val : extra; ns.idx = j; ns.val = extra; }
          double ex = sl > 0.001 && f < sl ? ns.pv + (ns.val - ns.pv) * f / sl : ns.val;
          if (sl > 0.001) { const double rlE = js::min(sl, hpEnd), h0 = hpEnd - rlE; if (rlE > 1e-9 && hp > h0) ex += (extra - ex) * (hp - h0) / rlE; }
          hot += ex;
        }
        ns.inside = true; ns.g = 1;
        return xin0 + p.depth * (hot - xin0);
      }
      default: hot = xin0;
    }
    ns.inside = true;
    const double tE = p.hard * w * 0.5;
    double g = 1;
    if (tE > 1e-9) {
      if (e < tE) g = 0.5 - 0.5 * std::cos(3.141592653589793 * e / tE);
      else if (e > w - tE) g = 0.5 - 0.5 * std::cos(3.141592653589793 * (w - e) / tE);
    }
    ns.g = g;
    return xin0 + g * p.depth * (hot - xin0);
  }
  // ADR-189 D1: the carrier sample becomes its MEAN over the phase it swept since
  // the previous internal sample, (F(cp) - F(cp0)) / (cp - cp0), which band-limits
  // every edge of the carrier to first order whatever pushed the phase. The plain
  // evaluation runs first (it owns the blade state); its carrier phase is
  // recomputed with the same expressions, so where D1 does not apply the result is
  // the plain one bit for bit. Half an internal sample of carrier-content lag is
  // left uncompensated: a centred mean needs the next phase, i.e. real latency.
  double voiceAA(const Blade& p, double phi, double base, double c, double k, double modX, NS& ns, bool hasInp, double inp) {
    const bool was = ns.inside;
    const double y = voicePlain(p, phi, base, c, k, modX, ns, hasInp, inp);
    const double md = p.mode;
    if (!ns.inside || !blade::d1Takes(p, aaBand)) { ns.aOk = false; return y; }
    const double w = p.w;
    double st = c - w * 0.5; st -= std::floor(st);
    double e = phi - st; if (e < 0) e += 1;
    const double kk = p.lock == 1 ? k / w : k;
    const double er = p.mirror == 1 && e > w * 0.5 ? w - e : e;
    const double hp = kk * er;
    double cp;
    if (md == 0 || md == 5) cp = hp + ns.xin + ns.cacc;
    else {
      const double Ib = ns.ov > 0 ? p.I * (1 + 4 * p.colB * ns.ov) : p.I;
      cp = ns.xin + ns.cacc + (p.fmType == 1 ? hp + ns.acc
        : hp + Ib * 0.15915494309189535 * mod(p.mshape, md == 1 ? p.mEff * er : modX, ns.seed));
    }
    const double cp0 = ns.acp;
    const bool ok = was && ns.aOk;
    ns.acp = cp; ns.aOk = true;
    if (!ok) return y;   // blade entry, or D1 just turned on: no previous phase
    const double dcp = cp - cp0;
    double hot;
    // a near-frozen carrier: the quotient's limit, the carrier at the step's midpoint
    if (std::fabs(dcp) < 1e-7) { const double x = cp - 0.5 * dcp; hot = wave(p.hot, x - std::floor(x)); }
    else hot = (blade::F(p.hot, cp) - blade::F(p.hot, cp0)) / dcp;
    const double xin0 = !hasInp ? base : inp;
    if (md == 5) hot = xin0 * hot;   // ring: the carrier times the input
    return xin0 + ns.g * p.depth * (hot - xin0);
  }
  // the full output: the blade, plus its twin half a cycle later, plus blade 2.
  // `base` is wave(p.base, phi) under the gmr/gmn in force at the call (voiceOut).
  double out(const SP& p, double phi, double base, double c, double k, double modX, NS& ns, NS& ns2, BX* bx) {
    if (bx && p.b2mix > 1e-6) return outSerial(p, phi, base, c, k, modX, ns, ns2, *bx);
    double y = voiceOut(p, phi, base, c, k, modX, ns);
    double ph2 = -1, base2 = 0;   // base2: wave(p.base, ph2) under this gmr, set iff twin1
    const bool twin1 = p.mirror >= 2;
    if (twin1) {
      ph2 = phi - 0.5; if (ph2 < 0) ph2 += 1;
      ns2.acc = ns.acc; ns2.xin = ns.xin;
      base2 = wave(p.base, ph2);
      const double d2 = voiceOut(p, ph2, base2, c, k, modX, ns2) - base2;
      y += p.mirror == 2 ? -d2 : d2;
    }
    if (bx) {   // blade 2: its own view, centre, rate and state, on the same base
      const Blade& g = *bx->g;
      const double mr0 = gmr, mn0 = gmn;
      gmr = bx->mr; gmn = bx->mn;
      // blade 1's base is blade 2's whenever no gmr/gmn read can tell them apart
      const bool same = sharesBase(g.base, p.base);
      const double gb = same ? base : wave(g.base, phi);
      y += voiceOut(g, phi, gb, bx->c, bx->k, bx->modX, bx->ns3) - gb;
      if (g.mirror >= 2) {
        if (ph2 < 0) { ph2 = phi - 0.5; if (ph2 < 0) ph2 += 1; }
        bx->ns4.acc = bx->ns3.acc; bx->ns4.xin = bx->ns3.xin;
        const double gb2 = same && twin1 ? base2 : wave(g.base, ph2);
        const double d4 = voiceOut(g, ph2, gb2, bx->c, bx->k, bx->modX, bx->ns4) - gb2;
        y += g.mirror == 2 ? -d4 : d4;
      }
      gmr = mr0; gmn = mn0;
    }
    return y;
  }
  // shared: the caller's base (blade 1's, under blade 1's gmr) is this blade's too
  struct Desc { const Blade* p; double c, k, modX; NS* st; NS* st2; double mr, mn; bool shared; };
  double ev(const Desc& D, double ph, double base, NS& st, bool hasInp = false, double inp = 0) {
    gmr = D.mr; gmn = D.mn;
    const double b = D.shared ? base : wave(D.p->base, ph);
    return voiceOut(*D.p, ph, b, D.c, D.k, D.modX, st, hasInp, inp);
  }
  // serial interplay: the upper blade transforms base + λ·(the lower blade's full
  // contribution, twins included). `base` as in out().
  double outSerial(const SP& p, double phi, double base, double c, double k, double modX, NS& ns, NS& ns2, BX& bx) {
    const Blade& g = *bx.g;
    const double lam = p.b2mix;
    const bool up2 = !js::truthy(p.b2order);   // default: blade 2 over blade 1
    double ph2 = phi - 0.5; if (ph2 < 0) ph2 += 1;
    const double base2 = wave(p.base, ph2);
    const double mr0 = gmr, mn0 = gmn;
    const Desc d1{&p, c, k, modX, &ns, &ns2, mr0, mn0, true},
               d2{&g, bx.c, bx.k, bx.modX, &bx.ns3, &bx.ns4, bx.mr, bx.mn, sharesBase(g.base, p.base)};
    const Desc& A = up2 ? d1 : d2;
    const Desc& B = up2 ? d2 : d1;
    const double sa = A.p->mirror == 2 ? -1 : A.p->mirror == 3 ? 1 : 0;
    const double sb = B.p->mirror == 2 ? -1 : B.p->mirror == 3 ? 1 : 0;
    const double dA = ev(A, phi, base, *A.st) - base;
    double dA2 = 0;
    if (js::truthy(sa) || js::truthy(sb)) { A.st2->acc = A.st->acc; A.st2->xin = A.st->xin; dA2 = ev(A, ph2, base2, *A.st2) - base2; }
    const double Lw = dA + sa * dA2, L2 = dA2 + sa * dA;
    const double x1 = base + lam * Lw, dB = ev(B, phi, base, *B.st, true, x1) - x1;
    double dB2 = 0;
    if (js::truthy(sb)) { B.st2->acc = B.st->acc; B.st2->xin = B.st->xin; const double x2 = base2 + lam * L2; dB2 = ev(B, ph2, base2, *B.st2, true, x2) - x2; }
    gmr = mr0; gmn = mn0;
    return base + Lw + dB + sb * dB2;
  }
  // blade 2's settings: units and mirror follow blade 1 (-1) or are its own; its FM
  // follows blade 1 unless b2fm
  void fillG2(Blade& g, double w2e, double dep2, double I2e, double mEff2) {
    g.mode = s.mode2; g.hot = s.hot2; g.base = s.base; g.w = w2e; g.depth = dep2; g.hard = s.hard2;
    g.lock = s.lock2 < 0 ? s.lock : s.lock2; g.mirror = s.mirror2 < 0 ? s.mirror : s.mirror2;
    g.fmType = js::truthy(s.b2fm) ? s.fmType2 : s.fmType; g.mshape = js::truthy(s.b2fm) ? s.mshape2 : s.mshape;
    g.I = I2e; g.mEff = mEff2; g.m = js::truthy(s.b2fm) ? s.m2 : s.m; g.colB = s.colB;
  }

  // ---- cut-rate rules and member spreads ------------------------------------
  // parseFloat: the longest prefix JS's StrDecimalLiteral accepts, then strtod on
  // exactly that prefix (strtod alone also takes hex, "inf" and "nan").
  static double jsParseFloat(const char* str, int n) {
    int i = 0;
    if (i < n && (str[i] == '+' || str[i] == '-')) i++;
    if (n - i >= 8 && std::strncmp(str + i, "Infinity", 8) == 0) return str[0] == '-' ? -INFINITY : INFINITY;
    int digits = 0;
    while (i < n && str[i] >= '0' && str[i] <= '9') { i++; digits++; }
    if (i < n && str[i] == '.') { i++; while (i < n && str[i] >= '0' && str[i] <= '9') { i++; digits++; } }
    if (!digits) return std::nan("");
    if (i < n && (str[i] == 'e' || str[i] == 'E')) {
      int j = i + 1;
      if (j < n && (str[j] == '+' || str[j] == '-')) j++;
      int ed = 0;
      while (j < n && str[j] >= '0' && str[j] <= '9') { j++; ed++; }
      if (ed) i = j;
    }
    char buf[128];
    if (i > 127) i = 127;
    std::memcpy(buf, str, i); buf[i] = 0;
    return std::strtod(buf, nullptr);
  }
  // the custom ratio list. Only the first 9 ratios and the count can matter
  // (the rule list indexes CU[j % len] for j < N <= 9).
  void parseCustom() {
    cuLen = 0;
    const char* p = d.kCustom;
    auto isSep = [](char ch) { return ch == ',' || ch == ';' || ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f'; };
    while (*p) {
      while (*p && isSep(*p)) p++;
      const char* tok = p;
      while (*p && !isSep(*p)) p++;
      const int tn = static_cast<int>(p - tok);
      if (!tn) continue;
      int slashes = 0, at = -1;
      for (int i = 0; i < tn; i++) if (tok[i] == '/') { if (!slashes) at = i; slashes++; }
      const double v = slashes == 1 ? jsParseFloat(tok, at) / jsParseFloat(tok + at + 1, tn - at - 1)
                                    : jsParseFloat(tok, tn);
      if (std::isfinite(v) && v > 0) { if (cuLen < 9) cu[cuLen] = v; cuLen++; }
    }
    if (!cuLen) { cu[0] = 1; cuLen = 1; }
  }
  // a rule's ratio per member slot (member 0 is the root), memoised per (rule, N)
  const double* ruleList(double rule, int N) {
    const int r = js::sel(rule);
    if (r >= 1 && r <= 9 && rlValid[r][N]) return rl[r][N];
    double* L = (r >= 1 && r <= 9) ? rl[r][N] : rlTmp;
    static const double PR[9] = {2, 3, 5, 7, 11, 13, 17, 19, 23};
    auto chord = [](const double* set, int len, int j) { return set[j % len] * std::pow(2, std::floor(static_cast<double>(j) / len)); };
    static const double MAJ[3] = {1, 5.0 / 4, 3.0 / 2}, MIN[3] = {1, 6.0 / 5, 3.0 / 2};
    for (int j = 0; j < N; j++) {
      switch (r) {
        case 1: L[j] = j + 1; break;                        // harmonic series
        case 2: L[j] = 1.0 / (j + 1); break;                // undertones
        case 3: L[j] = std::pow(2, j); break;               // octaves
        case 4: L[j] = chord(MAJ, 3, j); break;             // just major, stacked
        case 5: L[j] = chord(MIN, 3, j); break;             // just minor, stacked
        case 6: L[j] = std::pow(1.5, j); break;             // fifths
        case 7: L[j] = std::pow(1.6180339887, j); break;    // golden: maximally inharmonic
        case 8: L[j] = PR[j] / 2; break;                    // primes over 2
        default: L[j] = r == 9 ? cu[j % cuLen] * std::pow(2, std::floor(static_cast<double>(j) / cuLen)) : 1;
      }
    }
    if (r >= 1 && r <= 9) rlValid[r][N] = true;
    return L;
  }
  void kSpread(double rule, double amt, double spreadAmt, double q, int N, double pv, double outK[2]) {
    outK[0] = 0; outK[1] = 1;
    if (!js::truthy(rule)) {
      double ko = (js::truthy(q) ? js::round(spreadAmt) : spreadAmt) * pv;
      if (js::truthy(q)) ko = js::sign(ko) * js::round(std::fabs(ko));
      outK[0] = ko;
    } else if (N > 1) {   // the spread law picks each member's slot in the rule's list
      const double* L = ruleList(rule, N);
      const double x = js::min(N - 1, js::max(0, (pv + 0.5) * (N - 1)));
      double r;
      if (js::truthy(q)) r = L[static_cast<int>(js::round(x))];
      else { const double a = std::floor(x), b = js::min(N - 1, a + 1), f = x - a;
             r = std::exp(std::log(L[static_cast<int>(a)]) * (1 - f) + std::log(L[static_cast<int>(b)]) * f); }
      outK[1] = js::pow(r, amt);
    }
  }
  // a member's position under the spread law: 0 gradient, 1 and 4 its random slot,
  // 2 alternate, 3 its lead on the swarm's mean field
  static double pnOf(const Member& m, int j, int i, int N, double law) {
    const double g = N > 1 ? static_cast<double>(i) / (N - 1) - 0.5 : 0, alt = N > 1 ? (i % 2 ? 0.5 : -0.5) : 0;
    return N < 2 ? 0 : law == 0 ? g : (law == 1 || law == 4) ? m.rv[j] : law == 2 ? alt : m.lead;
  }
  void spreadMember(Member& m, int i, int N, double law, double fi, double nyq) {
    auto pn = [&](int j) { return pnOf(m, j, i, N, law); };
    m.cOff = s.bspread * pn(0);
    double ks[2];
    kSpread(s.kRule, s.kRuleAmt, s.kspread, s.kq, N, pn(1), ks);
    const double ko = ks[0];
    m.kAdd = ko; m.kMul = ks[1];
    m.rotOff = s.rotSpread * 2 * pn(2);
    m.rotOff2 = js::truthy(s.b2sp) ? s.rotSpread2 * 2 * pn(11) : m.rotOff;
    m.wMul = js::pow(2, s.wspread * 4 * pn(3));
    m.dAdd = s.dspread * 2 * pn(4);
    m.iMul = js::pow(2, s.ispread * 4 * pn(5));
    m.mor = js::min(1, js::max(0, s.morph + s.mspread * 2 * pn(6)));
    const double ki = js::min((s.lock == 2 ? js::max(0.05, s.kHz / js::max(1, fi) + ko) : js::max(0.25, s.k + ko)) * m.kMul, 0.9 * nyq / js::max(1, fi));
    m.kEff = ki;
    const double wi = js::min(1, s.w * m.wMul);
    const double fh = (s.lock == 1 ? ki / js::max(wi, 1e-3) : ki) * fi;
    const double rmax = js::min(0.995, js::pow(0.01, fh / nyq));
    const double r = 1 - js::pow(1 - rmax, m.mor);
    m.mr = r; m.mn = r > 1e-4 ? std::asin(r) : 1;
    if (!js::truthy(s.b2sp)) { m.cOff2 = m.cOff; m.kAdd2 = m.kAdd; m.kMul2 = m.kMul; m.wMul2 = m.wMul; m.dAdd2 = m.dAdd; m.iMul2 = m.iMul; }
    else {   // blade 2's own amounts, on independent random slots (7..13)
      m.iMul2 = js::pow(2, s.ispread2 * 4 * pn(13));
      m.cOff2 = s.bspread2 * pn(7);
      double k2s[2];
      kSpread(s.kRule2, s.kRuleAmt2, s.kspread2, s.kq, N, pn(8), k2s);
      m.kAdd2 = k2s[0]; m.kMul2 = k2s[1];
      m.wMul2 = js::pow(2, s.wspread2 * 4 * pn(9));
      m.dAdd2 = s.dspread2 * 2 * pn(10);
    }
    {   // blade 2's sine->saw cap, from blade 2's own carrier frequency
      const double mor2 = js::min(1, js::max(0, s.morph2 + (js::truthy(s.b2sp) ? s.mspread2 * 2 * pn(12) : s.mspread * 2 * pn(6))));
      const double lock2 = s.lock2 < 0 ? s.lock : s.lock2;
      const double k2 = js::min((lock2 == 2 ? js::max(0.05, s.kHz2 / js::max(1, fi) + m.kAdd2) : js::max(0.25, s.k2 + m.kAdd2)) * m.kMul2, 0.9 * nyq / js::max(1, fi));
      const double w2 = js::min(1, s.w2 * m.wMul2);
      const double fh2 = (lock2 == 1 ? k2 / js::max(w2, 1e-3) : k2) * fi;
      const double rmax2 = js::min(0.995, js::pow(0.01, fh2 / nyq)), r2 = 1 - js::pow(1 - rmax2, mor2);
      m.mr2 = r2; m.mn2 = r2 > 1e-4 ? std::asin(r2) : 1;
    }
  }
  // Every 32 samples and at note-on. The random law glides each member's slots
  // toward the note's draw; the drift law walks them (Ornstein-Uhlenbeck).
  void spread(Voice& v) {
    const int N = static_cast<int>(d.N);
    // A2 (2): under Quantize, Cut spread rounds half AWAY from zero so that -x
    // mirrors +x. An integer passes the law's own floor(x + 0.5) unchanged, so only
    // a negative half differs from the blade oracle.
    const bool q = js::truthy(d.kq) && !H2E_FAULT(1);
    const double k1 = s.kspread, k2 = s.kspread2;
    if (q) { s.kspread = roundAway(k1); s.kspread2 = roundAway(k2); }
    const double nyq = sr * os * 0.5, dt = 32 / sr;
    s.kq = d.kq; s.lock = d.lock; s.kRule = d.kRule; s.b2sp = d.b2sp; s.kRule2 = d.kRule2;
    s.lock2 = d.lock2; s.mirror2 = d.mirror2;
    if (d.law == 1) {
      const double a = 1 - std::exp(-dt / 0.02);
      for (int i = 0; i < N; i++) { Member& m = v.m[i]; for (int j = 0; j < 14; j++) m.rv[j] += (m.rvT[j] - m.rv[j]) * a; }
    } else if (d.law == 4) {
      const double th = 6.283185307179586 * s.driftRate, sig = 0.289 * std::sqrt(2 * th * dt), dec = std::exp(-th * dt);
      for (int i = 0; i < N; i++) {
        Member& m = v.m[i];
        for (int j = 0; j < 14; j++) {
          const double u = js::max(1e-12, rng.next());
          const double z = std::sqrt(-2 * std::log(u)) * std::cos(6.283185307179586 * rng.next());
          const double x = m.rv[j] * dec + sig * z;
          m.rv[j] = x > 0.75 ? 0.75 : x < -0.75 ? -0.75 : x;
        }
      }
    }
    for (int i = 0; i < N; i++) spreadMember(v.m[i], i, N, d.law, v.m[i].inc, nyq);
    if (q) { s.kspread = k1; s.kspread2 = k2; }
    // A2 (1): Rotate spread is bipolar. The law ran on the magnitude; the sign
    // turns the members the other way. Blade 2 follows unless it owns its spreads.
    if ((rotSign1 > 0 && rotSign2 > 0) || H2E_FAULT(10)) return;
    const bool own = js::truthy(d.b2sp);
    for (int i = 0; i < N; i++) {
      Member& m = v.m[i];
      if (rotSign1 < 0) m.rotOff = -m.rotOff;
      m.rotOff2 = own ? (rotSign2 < 0 ? -m.rotOff2 : m.rotOff2) : m.rotOff;
    }
  }
  static double roundAway(double x) { return x < 0 ? -js::round(-x) : js::round(x); }
  // glide a wrapped rotation offset back to 0 the shortest way round (rotation switched off)
  double home(double x, double k) const {
    double dd = x - js::round(x);
    dd -= dd * k;
    return std::fabs(dd) < 1e-6 ? 0 : dd;
  }

  // ---- voices: allocation, the cap, start ------------------------------------
  int poolSize() const {   // voices.slice(0, poly): ToIntegerOrInfinity, negative from the end
    const double p = std::isnan(d.poly) ? 0 : std::trunc(d.poly);
    const double e = p < 0 ? js::max(kVoices + p, 0) : js::min(p, kVoices);
    return static_cast<int>(e);
  }
  int liveCount() const { int n = 0; for (const Voice& v : voices) if (v.active && !v.cull) n++; return n; }
  enum Skip { kSkipNone, kSkipInactive, kSkipActive, kSkipInactiveOrFading };
  static bool skipped(const Voice& x, Skip k) {
    switch (k) {
      case kSkipInactive: return !x.active;
      case kSkipActive: return x.active;
      case kSkipInactiveOrFading: return !x.active || x.cull;
      default: return false;
    }
  }
  // ADR-083 tiers 1 and 2 over the first P voices: the oldest FADED slot (not
  // gated, env < 1e-3), else the QUIETEST releasing tail (age breaks ties; strict
  // `<` lets ties fall to pool order). Never a gated voice: tier 3 is note-on's.
  Voice* tierPick(int P, Skip k) {
    Voice* v = nullptr;
    for (int i = 0; i < P; i++) { Voice& x = voices[i]; if (!x.gate && x.env < 1e-3 && !skipped(x, k) && (!v || x.age < v->age)) v = &x; }
    if (v) return v;
    for (int i = 0; i < P; i++) {
      Voice& x = voices[i];
      if (!x.gate && !skipped(x, k) && (!v || x.env < v->env || (x.env == v->env && x.age < v->age))) v = &x;
    }
    return v;
  }
#ifdef H2_ENGINE_FAULTS
  // Must-fail control V1 (the parity check only): the blade oracle's own voice law,
  // same-note reuse then the first free slot then the oldest, which B310 replaced.
  void faultOracleVoiceLaw(int note, double freq, double vel, int P) {
    Voice* v = nullptr;
    for (int i = 0; i < P; i++) if (voices[i].active && voices[i].note == note) { v = &voices[i]; break; }
    const bool fresh = !v;
    if (!v) for (int i = 0; i < P; i++) if (!voices[i].active) { v = &voices[i]; break; }
    if (!v) { v = &voices[0]; for (int i = 0; i < P; i++) if (voices[i].age < v->age) v = &voices[i]; }
    startVoice(*v, note, freq, vel, fresh, true);
    v->freq = v->freqT = freq;
    couple(*v);
    spread(*v);
  }
#endif
  // B323: over the cap, mark the quietest releasing tails to fade out
  void cull() {
    if (H2E_FAULT(11)) return;   // must-fail control: the cull a no-op
    int live = liveCount();
    while (live > voiceCap) {
      Voice* v = tierPick(kVoices, kSkipInactiveOrFading);
      if (H2E_FAULT(12)) {         // must-fail control: the oldest sounding voice, held or not
        v = nullptr;
        for (Voice& x : voices) if (x.active && !x.cull && (!v || x.age < v->age)) v = &x;
      }
      if (!v) break;   // only held voices remain: never culled
      v->cull = true; v->cullG = 1; live--; culled_++;
    }
  }
  void stackRemove(int note) {
    int w = 0;
    for (int i = 0; i < stackN; i++) if (stack[i] != note) stack[w++] = stack[i];
    stackN = w;
  }
  // mono: one voice and a note stack, last-note priority. polyMode 1 retriggers on
  // every note, 2 (legato) only when no key is held. Glide slides between
  // overlapping notes, or always with glideAlways.
  void monoOn(int note, double freq, double vel) {
    Voice& v = voices[0];
    stackRemove(note); stack[stackN++] = note; nf[note] = freq;
    const bool held = v.active && v.gate, fresh = !v.active;
    const bool retrig = !held || d.polyMode == 1;
    const bool glide = !fresh && (held || js::truthy(d.glideAlways)) && s.glide > 1;
    startVoice(v, note, freq, vel, fresh, retrig);
    v.freqT = freq; if (!glide) v.freq = freq;
    couple(v);
    spread(v);
  }
  void startVoice(Voice& v, int note, double freq, double vel, bool fresh, bool retrig) {
    v.cull = false;   // a re-allocated slot is a new voice, never still fading
    if (fresh) {
      if (!v.active) v.env = 0;
      // These draws come from the host stream and the start phase is overwritten
      // below by the swarm's. They are taken anyway: every later draw depends on them.
      for (Member& m : v.m) {
        if (H2E_FAULT(2)) { m.modX = rng.next() * 1000; m.phi = d.phaseMode == 1 ? 0 : rng.next(); }
        else { m.phi = d.phaseMode == 1 ? 0 : rng.next(); m.modX = rng.next() * 1000; }
        m.bx.modX = m.modX + 317; m.prev = 0;
        m.ns.inside = false; m.ns.acc = 0; m.ns2.inside = false; m.inc = freq;
        m.dc = 0; m.dcS = 0; m.dcInit = true; m.y1 = 0; m.y2 = 0; m.bx.ns3.inside = false; m.bx.ns3.acc = 0; m.bx.ns4.inside = false;
        m.fu = 0; m.f1 = 0; m.f2 = 0;   // D3's loop starts from silence, as the loop itself does
      }
      v.freq = v.freqT = freq;
    }
    v.note = note; v.vel = vel; v.gate = true; v.active = true; v.age = ++age;
    if (retrig) {
      v.stage = 1; v.be = 0; v.bst = 1; v.bv = 1 - s.benvVel + s.benvVel * vel;
      v.be2 = 0; v.bst2 = 1; v.bv2 = 1 - s.benvVel2 + s.benvVel2 * vel;
      // 'note' rotation: every note starts with its blade exactly at Position
      if (js::truthy(d.rotSync) || fresh) { v.rot = 0; v.rot2 = 0; for (Member& m : v.m) { m.rot = 0; m.rot2 = 0; } }
      // the random law rolls a new distribution per note-on; a fresh voice jumps to
      // it, a retriggered one glides there so a sounding note does not click
      for (Member& m : v.m) for (int j = 0; j < 14; j++) { m.rvT[j] = rng.next() - 0.5; if (fresh) m.rv[j] = m.rvT[j]; }
      dcCnt = 0;
    }
    if (!fresh) {
      // A retrigger keeps the swarm and the ensemble draws; each member's own
      // envelope re-enters its attack from where it stands, as the voice's does.
      if (retrig && v.pv) for (Member& m : v.m) m.eS = 1;
      return;
    }
    v.gOn = false;   // ADR-008: the settle is per chord, so a note-on resets to ET
    v.pv = false;
    syncSwarm();
    field.noteOn(v.sw, freq * js::pow(2, s.bend / 12));
    v.sn = nBase;
    for (Member& m : v.m) { m.j = 0; m.phi = js::frac(v.sw.phase[m.i] + 0.5); }   // φ_S = frac(φ_H + ½)
    armMembers(v);
    // B325: the member frequencies must exist before the first sample reads them
    // (the cut rate in Hz, Hz modulators, the first DC estimate). Taken only when
    // the first tick falls on the note's first sample, as it always does in
    // 16-aligned blocks.
    v.tick0 = false;
    if ((v.sn & 15) == 0 && !H2E_FAULT(8)) lookAhead(v, true);
  }

  // ---- the swarm drive ---------------------------------------------------------
  // horde's parameters into the swarm. Member placement is redone only when one of
  // its inputs moved.
  void syncSwarm() {
    SwarmParams& p = field.p;
    bool re = false;
    if (p.n != d.N) { p.n = d.N; re = true; }
    if (p.dist != d.dist) { p.dist = d.dist; re = true; }
    if (p.seed != d.seed) { p.seed = d.seed; re = true; }
    if (p.law != d.hLaw) { p.law = d.hLaw; re = true; }
    if (re) field.rebuild();
    p.detune = t.detune / 100;   // SCALPEL's d cents is horde's knob d/100
    p.K = t.K;
    p.onset = d.onset; p.dissolve = d.dissolve;
    p.driftDepth = d.driftDepth; p.driftRate = d.hDriftRate; p.driftMode = d.driftMode;
    p.motionCenter = d.motionCenter;
    // the shell's inertia taper: the core receives knob^curve (sqrt exactly at 0.5)
    const double c = d.inertiaCurve, k = d.inertia;
    p.inertia = c == 0.5 ? std::sqrt(k) : js::pow(k, c);
    p.freqGlide = d.freqGlide; p.keepPhase = d.keepPhase; p.pivotMode = d.pivotMode;
    p.harmReach = d.harmReach; p.stretchB = d.stretchB; p.spread = d.spread; p.anchor = d.anchor;
    p.retrig = d.phaseMode == 0 ? 0 : 1;   // random = retrig off; aligned and settled = retrig on
    p.bpm = d.bpm; p.beatMult = d.beatMult;
    gCoefS = p.freqGlide > 0 ? 1 - std::exp(-1 / (p.freqGlide * 0.25 * sr)) : 0;
  }
  void tickSwarm(Voice& v) {
    syncSwarm();
    v.sw.fBase = v.freq;
    v.sw.f0 = H2E_EPS(f0Of(v));
    field.controlTick(v.sw);
  }
  // B325: take (first) or re-take the look-ahead tick. The snapshot excludes the
  // phases: the tick never moves them, and they are the swarm's start.
  void lookAhead(Voice& v, bool first) {
    if (first) v.pre0 = v.sw;
    else unLookAhead(v);
    tickSwarm(v);
    v.tick0 = true;
  }
  void unLookAhead(Voice& v) {
    double ph[kMembers];
    std::memcpy(ph, v.sw.phase, sizeof ph);
    v.sw = v.pre0;
    std::memcpy(v.sw.phase, ph, sizeof ph);
    v.tick0 = false;
  }
  // The swarm's pitch: the played pitch until gravity moves the voice, then
  // gravity's f0cur, riding the played pitch multiplicatively so a bend or glide
  // keeps the settled interval.
  double f0Of(Voice& v) {
    const double fb = v.freq * js::pow(2, s.bend / 12);
    if (!v.gOn) return fb;
    if (fb != v.gfb) { v.gf0 *= fb / v.gfb; v.gfb = fb; }
    return v.gf0;
  }
  // The blade law's member bookkeeping (each member's lead on the mean field, for
  // frames and spread law 3), then the member frequencies from the swarm.
  void couple(Voice& v) {
    const int N = static_cast<int>(d.N);
    v.fc = v.freq;
    double sx = 0, sy = 0;
    for (int i = 0; i < N; i++) { const double a = TAU * v.m[i].phi; sx += std::cos(a); sy += std::sin(a); }
    if (N > 1) {
      const double psi = std::atan2(sy / N, sx / N) / TAU;
      for (int i = 0; i < N; i++) { double l = v.m[i].phi - psi + 0.5; l -= std::floor(l); v.m[i].lead = l - 0.5; }
    } else v.m[0].lead = 0;
    Swarm& S = v.sw;
    // the pitch moved since the look-ahead and no sample has rendered: look again
    if (v.tick0 && (S.fBase != v.freq || S.f0 != f0Of(v))) lookAhead(v, false);
    if (!S.vfInit) {   // no tick yet this note: the played pitch
      const double f = v.freq * js::pow(2, s.bend / 12);
      for (int i = 0; i < N; i++) v.m[i].inc = f;
      return;
    }
    v.fc = S.fBase;   // v.gr = v.freq / v.fc then carries a glide between ticks
    const bool glideOn = field.p.freqGlide > 0;
    for (int i = 0; i < N; i++) v.m[i].inc = js::max(0, glideOn ? S.fRun[i] : S.eff[i]);
  }

  // ---- B335: gravity ------------------------------------------------------------
  // Every pair of HELD voices, by pitch, is pulled toward the nearest octave-folded
  // just ratio when it lies inside the basin: each note moves err·3·grav·dt/2 cents,
  // in opposite directions. A release tail keeps its pitch.
  void gravityStep(double dtB) {
    const double g = d.grav;
    if (g < 0.005) return;
    static const double RATIOS[13] = {1, 16.0 / 15, 9.0 / 8, 6.0 / 5, 5.0 / 4, 4.0 / 3, 7.0 / 5, 3.0 / 2, 8.0 / 5, 5.0 / 3, 16.0 / 9, 15.0 / 8, 2};
    Voice* act[kVoices];
    int na = 0;
    for (Voice& v : voices) if (v.gate) {
      if (!v.gOn) { v.gf0 = v.gfb = v.freq * js::pow(2, s.bend / 12); }
      else f0Of(v);
      act[na++] = &v;
    }
    if (na < 2) return;
    for (int i = 1; i < na; i++) {   // stable, by pitch
      Voice* x = act[i];
      int j = i;
      while (j > 0 && x->gf0 - act[j - 1]->gf0 < 0) { act[j] = act[j - 1]; j--; }
      act[j] = x;
    }
    const double rate = g * 3;   // full gravity settles in about a third of a second
    for (int a = 0; a < na - 1; a++) {
      for (int b = a + 1; b < na; b++) {
        Voice& lo = *act[a];
        Voice& hi = *act[b];
        const double r = hi.gf0 / lo.gf0;
        const double oct = std::floor(std::log2(r));
        const double rf = r / js::pow(2, oct);
        int bi = 0;
        double be = 1e9;
        for (int i = 0; i < 13; i++) {
          const double e = std::fabs(1200 * std::log2(rf / RATIOS[i]));
          if (e < be) { be = e; bi = i; }
        }
        const double err = 1200 * std::log2(rf / RATIOS[bi]);   // + = the interval is sharp
        if (std::fabs(err) > d.basin) continue;
        const double move = err * rate * dtB * 0.5;   // cents, each note
        hi.gf0 *= js::pow(2, -move / 1200);
        lo.gf0 *= js::pow(2, move / 1200);
        hi.gOn = true; lo.gOn = true;
      }
    }
  }

  // ---- B335: the ensemble ---------------------------------------------------------
  double gaussT() {
    double u = tRng.next(); if (u < 1e-9) u = 1e-9;
    const double w = tRng.next();
    return std::sqrt(-2 * std::log(u)) * std::cos(6.283185307 * w);
  }
  // The ensemble stream and its offsets re-derive when the seed changes, never
  // otherwise: tOff is the memory the ensemble carries across notes (ADR-077).
  void ensSync() {
    const double sd = d.seed;
    if (!ensSeeded || ensSeed != sd) {
      ensSeeded = true; ensSeed = sd;
      tRng.a = static_cast<uint32_t>(js::toInt32(sd)) * 2654435761u + 0x9E3779B8u;
      for (double& x : tOff) x = 0;
    }
  }
  // A fresh note's members: an attack and a release time factor each (drawn when
  // either feature is on, even at scatter 0, so the stream's order is fixed), then
  // with onset scatter the timing correction of the persistent offsets,
  //   tOff_i <- tOff_i - alpha·(tOff_i - mean) + N(0, scatter),
  // re-centred, in samples, shifted so the earliest member starts at once.
  void armMembers(Voice& v) {
    const int N = static_cast<int>(d.N);
    v.pv = d.onsetScatter > 0 || d.voiceEnv > 0.5;
    if (!v.pv) return;
    ensSync();
    const double As = s.A * 0.001;
    for (int i = 0; i < N; i++) {
      Member& m = v.m[i];
      m.onsD = 0; m.onsE = 0; m.hold = false; m.pg = 1;
      const double jit = 1 + gaussT() * d.attackScatter * 0.6;
      m.aMul = js::max(0.15, jit);
      m.onsC = 1 - std::exp(-1 / (js::max(0.002, As * m.aMul) * sr));
      const double rjit = 1 + gaussT() * d.relScatter * 0.6;
      m.rMul = js::max(0.15, rjit);
    }
    if (d.onsetScatter > 0) {
      double mean = 0;
      for (int i = 0; i < N; i++) mean += tOff[i];
      mean /= N;
      const double sig = d.onsetScatter * 0.001;
      for (int i = 0; i < N; i++) tOff[i] += -d.onsetAlpha * (tOff[i] - mean) + gaussT() * sig;
      double m2 = 0;
      for (int i = 0; i < N; i++) m2 += tOff[i];
      m2 /= N;
      for (int i = 0; i < N; i++) v.m[i].onsD = (tOff[i] - m2) * sr;
      double lo = v.m[0].onsD;
      for (int i = 1; i < N; i++) lo = js::min(lo, v.m[i].onsD);
      for (int i = 0; i < N; i++) v.m[i].onsD -= lo;
    }
    // a member that waits enters from silence; one that enters at once starts where
    // the voice's envelope stands
    for (int i = 0; i < N; i++) { Member& m = v.m[i]; m.eS = 1; m.eE = m.onsD > 0 ? 0 : v.env; }
  }
  // One sample of the members' entry, from member 0's first step, after the swarm's
  // tick and before any member advances:
  //   - a waiting member counts down, plays nothing and does not advance;
  //   - with onset scatter alone, an entry ramp rides on the voice envelope;
  //   - with per-partial envelopes, every member runs the voice's ADSR with its own
  //     drawn attack and release, and the voice envelope becomes the loudest member
  //     (so liveness, the voice law and the cull key off it); each member's gain is
  //     its level over that one.
  // A gain of exactly 1 leaves a member's output untouched.
  void memberStep(Voice& v) {
    const int N = static_cast<int>(d.N);
    const bool ens = d.onsetScatter > 0, venv = d.voiceEnv > 0.5;
    if (!ens && !venv) { for (int i = 0; i < N; i++) { v.m[i].hold = false; v.m[i].pg = 1; } return; }
    double vMax = 0;
    const double Sus = s.Sus, dC = eDC;
    for (int i = 0; i < N; i++) {
      Member& m = v.m[i];
      if (m.onsD > 0) { m.onsD -= 1; m.hold = true; m.pg = 0; continue; }
      m.hold = false;
      if (venv) {
        if (m.eCall != rCall) {   // this member's rates, once per render call as the voice's are
          m.eCall = rCall;
          m.eAi = 1 / js::max(1, js::max(2, eA * m.aMul) * 0.001 * sr);
          m.eRc = 1 - std::exp(-4 / js::max(1, js::max(2, eR * m.rMul) * 0.001 * sr));
        }
        if (!v.gate && m.eS != 0) m.eS = 4;
        if (m.eS == 1) {
          m.eE += m.eAi;
          if (m.eE >= 1) { m.eE = 1; m.eS = 2; }
        } else if (m.eS == 2) m.eE += (Sus - m.eE) * dC;
        else if (m.eS == 4) {
          m.eE -= m.eE * m.eRc;
          if (m.eE < 1e-4) { m.eE = 0; m.eS = 0; }
        }
        if (m.eE > vMax) vMax = m.eE;
      } else {
        m.onsE += (1 - m.onsE) * m.onsC;
        m.pg = m.onsE;
      }
    }
    if (venv) {
      v.env = vMax;
      for (int i = 0; i < N; i++) { Member& m = v.m[i]; if (!m.hold) m.pg = vMax > 0 ? m.eE / vMax : 0; }
    }
  }
  // the state the render reads once per call, taken just before the call
  void preCall() {
    rCall++;
    if (!pvLive) return;
    eA = s.A; eR = s.R;
    eDC = 1 - std::exp(-4 / js::max(1, s.D * 0.001 * sr));
  }

  // ---- anti-aliasing ----------------------------------------------------------------
  void setOS(double n) {
    os = n;
    const double fs = sr * n, fc = 0.45 * sr;
    bqL[0] = mk(fs, fc, 0.5412); bqL[1] = mk(fs, fc, 1.3066);
    bqR[0] = mk(fs, fc, 0.5412); bqR[1] = mk(fs, fc, 1.3066);
    loopA = 1 - std::exp(-TAU * kLoopFc / (sr * n));   // D3's one-pole, per internal sample
    for (Voice& v : voices) for (Member& m : v.m) m.j = 0;
  }
  static Biquad mk(double fs, double fc, double Q) {
    const double w0 = 6.283185307179586 * fc / fs, cs = std::cos(w0), al = std::sin(w0) / (2 * Q);
    const double a0 = 1 + al, b0 = (1 - cs) / 2;
    Biquad b; b.b0 = b0 / a0; b.b1 = (1 - cs) / a0; b.b2 = b0 / a0; b.a1 = (-2 * cs) / a0; b.a2 = (1 - al) / a0;
    return b;
  }
  static double bqf(Biquad& f, double x) {
    const double y = f.b0 * x + f.z1;
    f.z1 = f.b1 * x - f.a1 * y + f.z2;
    f.z2 = f.b2 * x - f.a2 * y;
    return y;
  }
  // the step across a discontinuity at E, measured on scratch copies of the member's state
  double hAt(Member& m, double E, double c, double k, const SP& p) {
    sc.seed = m.ns.seed; sc.acc = m.ns.acc; sc2.seed = m.ns2.seed;
    sc.idx = m.ns.idx; sc.val = m.ns.val; sc.inside = true;
    sc2.idx = m.ns2.idx; sc2.val = m.ns2.val; sc2.inside = true;
    sc.xin = m.ns.xin; sc2.xin = m.ns.xin;
    sc.cacc = m.ns.cacc; sc.ov = m.ns.ov; sc.pv = m.ns.pv; sc2.cacc = 0; sc2.ov = 0; sc2.pv = m.ns2.pv;
    BX* bx = nullptr;
    if (js::truthy(p.b2on)) {
      BX& src = m.bx; BX& b = bxs; bx = &b;
      b.g = src.g; b.c = src.c; b.k = src.k; b.modX = src.modX; b.mr = src.mr; b.mn = src.mn;
      b.ns3.seed = src.ns3.seed; b.ns3.acc = src.ns3.acc; b.ns3.idx = src.ns3.idx; b.ns3.val = src.ns3.val; b.ns3.inside = true; b.ns3.xin = src.ns3.xin;
      b.ns4.seed = src.ns4.seed; b.ns4.idx = src.ns4.idx; b.ns4.val = src.ns4.val; b.ns4.inside = true;
      b.ns3.cacc = src.ns3.cacc; b.ns3.ov = src.ns3.ov; b.ns3.pv = src.ns3.pv; b.ns4.cacc = 0; b.ns4.ov = 0; b.ns4.pv = src.ns4.pv;
    }
    const double pa = js::frac(E + 1e-7);
    const double a = out(p, pa, wave(p.base, pa), c, k, m.modX, sc, sc2, bx);
    sc.idx = m.ns.idx; sc.inside = true; sc2.idx = m.ns2.idx; sc2.inside = true;
    if (bx) { bx->ns3.idx = m.bx.ns3.idx; bx->ns3.inside = true; bx->ns4.idx = m.bx.ns4.idx; bx->ns4.inside = true; }
    const double pb = js::frac(E - 1e-7);
    const double b = out(p, pb, wave(p.base, pb), c, k, m.modX, sc, sc2, bx);
    return a - b;
  }
  // the blade's mean over one cycle, for the per-cycle DC correction
  double dcEst(NS& ns, double modX, double c, double k, const Blade& p) {
    const double w = p.w;
    dcJ = 0;
    if (w < 0.004) return 0;   // noise is estimated at its mean, zero
    const double fac = p.mirror == 2 ? 0 : p.mirror == 3 ? 2 : 1;
    if (!js::truthy(fac)) return 0;
    double st = c - w * 0.5; st -= std::floor(st);
    const double kk = p.lock == 1 ? k / w : k;
    // a sync blade of a closed-form wave: the hard-edged integral is exact, and soft
    // edges subtract what the tapers remove, integrated over the tapers only
    if (p.mode == 0 && p.hot != 6) {
      const double hotInt = p.mirror == 1 ? 2 * (blade::F(p.hot, kk * w * 0.5) - blade::F(p.hot, 0)) / kk
                                          : (blade::F(p.hot, kk * w) - blade::F(p.hot, 0)) / kk;
      double total = hotInt - (blade::F(p.base, st + w) - blade::F(p.base, st));
      const double tE = p.hard * w * 0.5;
      if (tE > 1e-9) {
        const double Jt = js::min(512, js::max(32, std::ceil(kk * tE * 32)));
        dcJ = 2 * Jt;
        sc.inside = true;
        double cut = 0;
        for (int side = 0; side < 2; side++) for (double j = 0; j < Jt; j++) {
          const double e = side == 0 ? (j + 0.5) / Jt * tE : w - (j + 0.5) / Jt * tE;
          const double er = p.mirror == 1 && e > w * 0.5 ? w - e : e;
          double hp = kk * er; hp -= std::floor(hp);
          double phi = st + e; phi -= std::floor(phi);
          const double g = 0.5 - 0.5 * std::cos(3.141592653589793 * (side == 0 ? e : w - e) / tE);
          cut += (1 - g) * (wave(p.hot, hp) - wave(p.base, phi));
        }
        total -= cut * tE / Jt;
      }
      return fac * p.depth * total;
    }
    // numeric: enough points to resolve every carrier cycle, crush step and
    // modulator cycle, so the grid can never lock onto the blade's own period
    double feats = kk * w;
    if (blade::isFM(p)) feats *= 1 + 0.5 * p.I;
    feats += p.mEff * w;
    const double J = js::min(1024, js::max(48, std::ceil(feats * 32)));
    dcJ = J;
    sc.seed = ns.seed; sc.acc = ns.acc; sc.idx = ns.idx; sc.val = p.mode == 3 ? 0 : ns.val; sc.xin = ns.xin;
    double sum = 0;
    for (double j = 0; j < J; j++) {
      double phi = st + (j + 0.5) / J * w; phi -= std::floor(phi);
      sc.inside = j > 0;
      const double bw = wave(p.base, phi);
      sum += voiceOut(p, phi, bw, c, k, modX, sc) - bw;
    }
    return fac * w * sum / J;
  }
  // serial interplay: the composite's mean over one cycle, numerically
  double dcPair(Member& m, double c, double k, const SP& p) {
    BX& bx = m.bx; const Blade& g = *bx.g; BX& b = bxs;
    const double kk1 = p.w >= 0.004 ? (p.lock == 1 ? k / js::max(p.w, 1e-3) : k) : 0;
    const double kk2 = g.w >= 0.004 ? (g.lock == 1 ? bx.k / js::max(g.w, 1e-3) : bx.k) : 0;
    const double feats = kk1 * (blade::isFM(p) ? 1 + 0.5 * p.I : 1) + kk2 * (blade::isFM(g) ? 1 + 0.5 * g.I : 1) + 2 * (p.mEff + g.mEff) + 4;
    const double J = js::min(2048, js::max(64, std::ceil(feats * 32)));
    dcJ = J;
    auto z = [](NS& dd, const NS& src) { dd.seed = src.seed; dd.acc = src.acc; dd.xin = src.xin; dd.cacc = 0; dd.ov = 0; dd.cd = 0; dd.idx = 0; dd.val = 0; dd.pv = 0; dd.inside = false; };
    z(sc, m.ns); z(sc2, m.ns2);
    b.g = bx.g; b.c = bx.c; b.k = bx.k; b.modX = bx.modX; b.mr = bx.mr; b.mn = bx.mn;
    z(b.ns3, bx.ns3); z(b.ns4, bx.ns4);
    double sum = 0;
    for (double j = 0; j < J; j++) {
      const double phi = (j + 0.5) / J;
      const double bw = wave(p.base, phi);
      sum += out(p, phi, bw, c, k, m.modX, sc, sc2, &b) - bw;
    }
    return sum / J;
  }
  // PolyBLEP on the discontinuities inside one blade (or its twin), tracked in
  // carrier phase: carrier wraps, and crush steps.
  //   D1 on: a carrier D1 band-limits gets no wrap BLEP (replaced, never stacked).
  //   D2 on: the phase the scanner follows includes the xin push, as the carrier's
  //   does. The step's xin is added at both ends: the BLEP's height is probed with
  //   the current xin, so an edge placed on an interpolated xin would be probed off
  //   the edge. Crush steps sit at integer hold phases that xin does not move.
  void scan(Member& m, double st, double p0, double dphi, double c, double k, const SP& p, double dAcc,
            double modX0, const Blade& g, double kB, NS& ns, double modX1) {
    const bool carrier = g.mode == 0 || g.mode == 5 || blade::isFM(g);
    if (js::truthy(d.aaCarrier) && blade::d1Takes(g, true)) return;
    const double w = g.w;
    double off = -1, step = 1;
    if (carrier && (g.hot == 2 || g.hot == 4)) { off = 0.5; step = 1; }
    else if (carrier && g.hot == 3) { off = 0; step = 0.5; }
    else if (g.mode == 6 && g.hard <= 0.001) { off = 0; step = 1; }
    if (off < 0) return;
    double e0 = p0 - st; e0 -= std::floor(e0);
    if (e0 >= w) return;
    const double kk = g.lock == 1 ? kB / w : kB;
    const double eEnd = js::min(e0 + dphi, w), tmax = (eEnd - e0) / dphi;
    const double hw = w * 0.5;
    const bool refl = g.mirror == 1;
    const double r0 = refl && e0 > hw ? w - e0 : e0, r1 = refl && eEnd > hw ? w - eEnd : eEnd;
    double o0 = 0, o1 = 0;
    if (blade::isFM(g)) {
      if (g.fmType == 1) { o1 = ns.acc; o0 = o1 - dAcc; }
      else {
        const double scl = 0.15915494309189535 * g.I;
        o0 = scl * mod(g.mshape, g.mode == 1 ? g.mEff * r0 : modX0, ns.seed);
        o1 = scl * mod(g.mshape, g.mode == 1 ? g.mEff * r1 : modX1, ns.seed);
      }
    }
    if (js::truthy(ns.cd)) { o0 += ns.cacc - ns.cd; o1 += ns.cacc; }
    if (carrier && js::truthy(d.aaXin)) { o0 += ns.xin; o1 += ns.xin; }   // D2
    const double cp0 = kk * r0 + o0, cp1 = kk * r1 + o0 + (o1 - o0) * tmax;
    const double lo = js::min(cp0, cp1), hi = js::max(cp0, cp1);
    if (!(hi - lo > 1e-12 && hi - lo < 8)) return;
    double j = std::floor((lo - off) / step) + 1;
    for (int q = 0; q < 6; q++, j++) {
      const double pos = off + j * step; if (pos > hi) break;
      if (pos <= lo || pos == 0) continue;
      const double tau = (pos - cp0) / (cp1 - cp0) * tmax;
      if (tau > 0 && tau <= 1) { emit(2); addE(m, p0 + tau * dphi, tau, c, k, p); }
    }
  }
  void addE(Member& m, double E, double tau, double c, double k, const SP& p) {
    const double h = hAt(m, E, c, k, p);
    blepHeld -= 0.5 * h * tau * tau;
    blepOut += 0.5 * h * (1 - tau) * (1 - tau);
  }
  void tryE(Member& m, double E, double p0, double dphi, double c, double k, const SP& p) {
    double dd = E - p0; dd -= std::floor(dd);
    if (dd > 0 && dd <= dphi) { emit(1); addE(m, E, dd / dphi, c, k, p); }
  }
  void emit(uint32_t kind) { if (events) events->add(evTick + (H2E_FAULT(5) ? 1u : 0u), evId, kind); }

  // ---- one member, one oversampled step ----------------------------------------------
  // The swarm's part at the member's first step of each sample (member 0 ticks the
  // swarm on the global 16-sample grid and runs the entries), then its gain, then
  // the blade with D3's loop around it.
  double stepMember(Voice& v, Member& m, double c, double k) {
    if (m.j == 0) {
      Swarm& S = v.sw;
      const int i = m.i;
      if (i == 0) {
        const bool due = H2E_FAULT(7) ? (v.sn & 15) == 1 : (v.sn & 15) == 0;
        if (due && !H2E_SKIP(kStageTick)) { if (v.tick0) unLookAhead(v); tickSwarm(v); }
        v.sn++;
        if (v.pv) memberStep(v);
      }
      const bool glideOn = field.p.freqGlide > 0;
      if (glideOn) S.fRun[i] += gCoefS * (S.eff[i] - S.fRun[i]);
      if (!(v.pv && m.hold)) {   // a waiting member's phase stands still
        const double f = glideOn ? S.fRun[i] : S.eff[i];
        if (!H2E_SKIP(kStagePhase)) {
          const double dph = js::max(0, f) / sr;
          double ph = S.phase[i] + dph;
          ph -= std::floor(ph);
          S.phase[i] = ph;
        }
        m.dph = js::max(0, f) / (sr * os);
      }
    }
    if (++m.j >= os) m.j = 0;
    // the member's gain rides the pan gains, which the render rebuilds per call:
    // the call's first step snapshots them
    if (pvLive) {
      const int q = m.i;
      if (glCall != rCall) { glCall = rCall; std::memcpy(glB, gl, sizeof gl); std::memcpy(grB, gr, sizeof gr); }
      const double g = v.pv ? m.pg : 1;
      gl[q] = glB[q] * g; gr[q] = grB[q] * g;
    }
    if (v.pv && m.hold) return 0;   // not started: no output, no blade step
    const bool xOn = s.xm > 0.0005 || s.fb > 0.0005;
    if (xOn && js::truthy(d.aaLoop)) loopIn(v, m);
    const double y = stepBlade(m, m.dph, c, k);
    // D3's taps: the blade's own output through the loop filter, kept running while
    // the loop is on whatever D3 says, so switching D3 on starts from a warm filter
    if (xOn) { m.fu += loopA * (y - m.fu); m.f2 = m.f1; m.f1 = m.fu; }
    return y;
  }
  // ADR-189 D3: the loop's input, the blade law's own formula over the FILTERED taps.
  // The ring's next member has not stepped yet this sample, except round the ring
  // (member 0, stepped first), whose tap was kept before it stepped.
  void loopIn(Voice& v, Member& m) {
    const int N = static_cast<int>(d.N), q = m.i, nb = q + 1 == N ? 0 : q + 1;
    if (q == 0) v.fx0 = m.fu;
    const double xin = 0.5 * (H2E_EPS13(s.xm) * (nb == 0 ? v.fx0 : v.m[nb].fu) + s.fb * 0.5 * (m.f1 + m.f2));
    m.ns.xin = xin; m.ns2.xin = xin; m.bx.ns3.xin = xin; m.bx.ns4.xin = xin;
  }
  // the blade step: advance, collide, evaluate, then PolyBLEP every known
  // discontinuity crossed in this step (with one sample of latency)
  double stepBlade(Member& m, double dphi, double c, double k) {
    SP& p = s;
    const double w = p.w, p0 = m.phi;
    NS& ns = m.ns;
    double p1 = p0 + dphi; p1 -= std::floor(p1); m.phi = p1;
    const double modX0 = m.modX;
    m.modX += p.mEff * dphi; if (m.modX > 65536) m.modX -= 65536;
    double st = c - w * 0.5; st -= std::floor(st);
    double e0 = p0 - st; e0 -= std::floor(e0);
    double e1 = p1 - st; e1 -= std::floor(e1);
    const bool on = w >= 0.004;
    const double kk = on ? (p.lock == 1 ? k / w : k) : k;
    if (on && e1 < e0) emit(3);
    const double dAcc = on ? fmStep(p, ns, e1, e1 < e0, kk, m.modX, dphi) : 0;
    BX* bx = js::truthy(p.b2on) ? &m.bx : nullptr;
    double dAcc2 = 0, st3 = 0, modX20 = 0;
    bool on2 = false;
    if (bx) {   // blade 2 shares blade 1's modulator unless it has FM of its own
      if (js::truthy(p.b2fm)) { modX20 = bx->modX; bx->modX += bx->g->mEff * dphi; if (bx->modX > 65536) bx->modX -= 65536; }
      else { modX20 = modX0; bx->modX = m.modX; }
      const Blade& g = *bx->g;
      const double w2 = g.w;
      on2 = w2 >= 0.004;
      if (on2) {
        st3 = bx->c - w2 * 0.5; st3 -= std::floor(st3);
        double a0 = p0 - st3; a0 -= std::floor(a0); double a1 = p1 - st3; a1 -= std::floor(a1);
        const double mr0 = gmr; gmr = bx->mr;
        if (a1 < a0) emit(4);
        dAcc2 = fmStep(g, bx->ns3, a1, a1 < a0, g.lock == 1 ? bx->k / w2 : bx->k, bx->modX, dphi);
        gmr = mr0;
      }
    }
    if (bx && on && on2 && (js::truthy(p.colK) || js::truthy(p.colB))) {
      const Blade& g = *bx->g;
      const double w2 = g.w;
      double a0 = p0 - st3; a0 -= std::floor(a0); double a1 = p1 - st3; a1 -= std::floor(a1);
      const double ov = blade::gate(e1, w, p.hard, p.mode) * blade::gate(a1, w2, g.hard, g.mode);
      if (js::truthy(p.b2order)) { blade::collide(p.colK, ns, e0, e1, kk, ov, dphi); bx->ns3.ov = 0; bx->ns3.cacc = 0; bx->ns3.cd = 0; }
      else { blade::collide(p.colK, bx->ns3, a0, a1, g.lock == 1 ? bx->k / w2 : bx->k, ov, dphi); ns.ov = 0; ns.cacc = 0; ns.cd = 0; }
    } else if (js::truthy(ns.ov) || js::truthy(ns.cacc) || (bx && (js::truthy(bx->ns3.ov) || js::truthy(bx->ns3.cacc)))) {
      ns.ov = 0; ns.cacc = 0; ns.cd = 0; if (bx) { bx->ns3.ov = 0; bx->ns3.cacc = 0; bx->ns3.cd = 0; }
    }
    const double x = H2E_SKIP(kStageBlade) ? 0 : out(p, p1, wave(p.base, p1), c, k, m.modX, ns, m.ns2, bx);
    blepHeld = 0; blepOut = 0;
    if (!H2E_SKIP(kStageBlep) && js::truthy(p.aa) && dphi > 0 && dphi < 0.5) {
      const double b = p.base;
      if (b == 2 || b == 4) tryE(m, 0.5, p0, dphi, c, k, p);
      else if (b == 3) { tryE(m, 0, p0, dphi, c, k, p); tryE(m, 0.5, p0, dphi, c, k, p); }
      if (on) {
        if (!H2E_FAULT(3)) tryE(m, st, p0, dphi, c, k, p);
        if (w < 0.9999) tryE(m, st + w, p0, dphi, c, k, p);
        scan(m, st, p0, dphi, c, k, p, dAcc, modX0, p, k, ns, m.modX);
        if (p.mirror >= 2) {
          const double st2 = st + 0.5;
          tryE(m, st2, p0, dphi, c, k, p);
          if (w < 0.9999) tryE(m, st2 + w, p0, dphi, c, k, p);
          scan(m, st2 - std::floor(st2), p0, dphi, c, k, p, dAcc, modX0, p, k, ns, m.modX);
        }
      }
      if (on2) {
        const Blade& g = *bx->g;
        const double w2 = g.w;
        tryE(m, st3, p0, dphi, c, k, p);
        if (w2 < 0.9999) tryE(m, st3 + w2, p0, dphi, c, k, p);
        scan(m, st3, p0, dphi, c, k, p, dAcc2, modX20, g, bx->k, bx->ns3, bx->modX);
        if (g.mirror >= 2) {
          const double st4 = st3 + 0.5;
          tryE(m, st4, p0, dphi, c, k, p);
          if (w2 < 0.9999) tryE(m, st4 + w2, p0, dphi, c, k, p);
          scan(m, st4 - std::floor(st4), p0, dphi, c, k, p, dAcc2, modX20, g, bx->k, bx->ns3, bx->modX);
        }
      }
    }
    const double o = m.prev + blepOut;
    m.prev = x + blepHeld;
    return o;
  }

  // ---- the render -------------------------------------------------------------------
  // One block between gravity steps. D1 starts every state fresh when it has just
  // turned on (there is no previous carrier phase).
  void renderBlock(double* L, double* R, int n) {
    const bool aa1 = js::truthy(d.aaCarrier);
    if (aa1 && !aaWas) for (Voice& v : voices) for (Member& m : v.m) { m.ns.aOk = false; m.ns2.aOk = false; m.bx.ns3.aOk = false; m.bx.ns4.aOk = false; }
    aaWas = aa1;
    aaOn = aa1;
    aaBand = js::truthy(d.aa);
    renderCalls(L, R, n);
    aaOn = false;
  }
  // With no cull running: one call. While a culled voice fades, one call per sample,
  // with every fading voice's envelope scaled after each, so its gain follows
  // 1 - t/8 ms sample by sample. A per-sample call re-reads the per-call setup (the
  // pan law, the envelope rates), as the JS does, and its output passes through a
  // float, as the JS's one-sample Float32Array does.
  void renderCalls(double* L, double* R, int n) {
    bool fading = false;
    pvLive = false;
    for (const Voice& v : voices) if (v.active) { if (v.cull) fading = true; if (v.pv) pvLive = true; }
    if (!fading) { preCall(); renderCall(L, R, n); nBase += static_cast<uint64_t>(n); return; }
    const double step = 1 / (kCullFade * sr);
    for (int i = 0; i < n; i++) {
      preCall();
      double l1, r1;
      renderCall(&l1, &r1, 1);
      L[i] = static_cast<double>(static_cast<float>(l1));
      R[i] = static_cast<double>(static_cast<float>(r1));
      nBase++;
      for (Voice& v : voices) if (v.cull && v.active) {
        const double g = v.cullG - step;
        if (g <= 0) { v.env = 0; v.active = false; v.cull = false; }   // freed at the ramp's end
        else {
          // a per-partial voice's envelope is its loudest member: scale the members
          const double f = g / v.cullG;
          v.env *= f; v.cullG = g;
          if (v.pv) for (Member& m : v.m) m.eE *= f;
        }
      }
    }
  }
  void renderCall(double* L, double* R, int n);

  // ---- state ----------------------------------------------------------------------
  static constexpr double kCullFade = 0.008;   // B323's fade, seconds: 5-10 ms asked, clickless on a sine
  // D3's loop filter. The highest cutoff measured to clear the rate-locked limit
  // cycle at every os on the patches that had it (ADR-189 A1, the B355 trace).
  static constexpr double kLoopFc = 5000;

  double sr;
  SwarmField field;
  double age = 0;
  int coupleTick = 0, smoothTick = 0;   // countdowns: couple and spread every 32 samples, the smoother every 16
  SP t, s;
  DP d;
  Voice voices[kVoices];
  double gl[kMembers] = {}, gr[kMembers] = {};
  NS sc, sc2;
  BX bxs;
  int dcCnt = 0;
  double hx[2] = {}, hy[2] = {}, gRot = 0, gRot2 = 0;
  int stack[128] = {};
  int stackN = 0;
  double nf[128] = {};
  // the PolyBLEP residuals of the step in progress: for the sample going out now
  // (the blade runs one sample late) and for the one held for the next step
  double blepOut = 0, blepHeld = 0;
  double os = 0;
  Biquad bqL[2], bqR[2];
  double gmr = 0, gmn = 1;   // the sine->saw band-limit of the blade being evaluated
  double dcJ = 0;            // the last DC estimate's point count
  Mulberry32 rng;
  double cu[9] = {1, 5.0 / 4, 3.0 / 2};
  int cuLen = 3;
  double rl[10][10][9] = {};
  bool rlValid[10][10] = {};
  double rlTmp[9] = {};
  double rotSign1 = 1, rotSign2 = 1;   // A2: the signs of Rotate spread 1 and 2
  // the swarm drive
  uint64_t nBase = 0;   // samples rendered: the swarms' global tick count
  double gCoefS = 0;    // the per-sample leg of the frequency glide
  // the cap
  double voiceCap = 0, capPolicy = 0, culled_ = 0, refused_ = 0, stolen_ = 0;
  // gravity's fixed-time grid: 256 samples at 44.1 kHz, 279 at 48
  int gravGrid = 256, gravAccum = 0;
  // the ensemble
  double tOff[kMembers] = {};
  Mulberry32 tRng;
  double ensSeed = 0;
  bool ensSeeded = false;
  // per-member gains on the pan gains, and the per-call envelope times
  double rCall = 0, glCall = -1;
  double glB[kMembers] = {}, grB[kMembers] = {};
  bool pvLive = false;
  double eA = 0, eR = 0, eDC = 0;
  // ADR-189
  double loopA = 0;
  bool aaWas = false, aaOn = false, aaBand = true;
  // event context (read only when `events` is set)
  uint32_t evTick = 0, evId = 0;
  uint64_t evSample = 0;
  // B441 C1: each member's overrides of the shared parameters (and its cut phase and
  // cut rate), computed at a sample's first oversampled step and replayed at the rest
  struct MemberOv { double w = 0, depth = 0, I = 0, c = 0, kq = 0, mEff = 0; };
  MemberOv memberOv[kVoices][kMembers] = {};
};

// The blade engine's render, with the swarm driving the members. Per sample: the
// smoothers, the swarm-linked spreads every 32 samples, the voice envelopes and
// rotation; per oversampled step: every member of every voice; then the output
// filters, the DC blocker and the tanh.
inline void Engine::renderCall(double* L, double* R, int n) {
  const double a = 1 - std::exp(-16 / (0.012 * sr));
  const double a1 = 1 - std::exp(-1 / (0.012 * sr));
  s.mode = d.mode; s.hot = d.hot; s.base = d.base; s.lock = d.lock; s.mshape = d.mshape; s.fmType = d.fmType; s.mirror = d.mirror; s.aa = d.aa; s.mEff = s.m;
  s.b2on = d.b2on; s.mode2 = d.mode2; s.hot2 = d.hot2; s.b2order = d.b2order;
  s.lock2 = d.lock2; s.mirror2 = d.mirror2; s.b2fm = d.b2fm; s.fmType2 = d.fmType2; s.mshape2 = d.mshape2; s.mUnit2 = d.mUnit2;
  s.kq = d.kq; s.law = d.law; s.kRule = d.kRule; s.b2sp = d.b2sp; s.kRule2 = d.kRule2;
  const int N = static_cast<int>(d.N);
  for (int i = 0; i < N; i++) {
    const double pn = N > 1 ? static_cast<double>(i) / (N - 1) - 0.5 : 0;
    const double pos = js::truthy(d.panOrder) ? pn * 2 : blade::panSlot(N, i);
    const double ang = (pos * s.width + 1) * 3.141592653589793 / 4;
    gl[i] = std::cos(ang); gr[i] = std::sin(ang);
  }
  const double norm = 1 / std::sqrt(static_cast<double>(N));
  const double attInc = 1 / js::max(1, s.A * 0.001 * sr);
  const double dC = 1 - std::exp(-4 / js::max(1, s.D * 0.001 * sr));
  const double rC = 1 - std::exp(-4 / js::max(1, s.R * 0.001 * sr));
  const bool dcOn = d.dcMode == 2;
  const double hpR = 1 - 6.283185307179586 * 8 / sr;
  for (int i = 0; i < n; i++, evSample++) {
    if (--smoothTick <= 0) {
      smoothTick = 16;
      for (int q = 0; q < kTKeys; q++) { const TKey& kk = tKeys()[q]; if (!kk.perSample) s.*kk.p += (t.*kk.p - s.*kk.p) * a; }
    }
    // the audio-critical parameters glide every sample, so cut-rate and width moves never step
    s.k += (t.k - s.k) * a1; s.kHz += (t.kHz - s.kHz) * a1; s.w += (t.w - s.w) * a1; s.c += (t.c - s.c) * a1;
    s.depth += (t.depth - s.depth) * a1; s.I += (t.I - s.I) * a1; s.hard += (t.hard - s.hard) * a1;
    s.w2 += (t.w2 - s.w2) * a1; s.k2 += (t.k2 - s.k2) * a1; s.kHz2 += (t.kHz2 - s.kHz2) * a1; s.c2 += (t.c2 - s.c2) * a1;
    s.b2mix += (t.b2mix - s.b2mix) * a1;
    if (--coupleTick <= 0) { coupleTick = 32; if (!H2E_SKIP(kStageCouple)) for (Voice& v : voices) if (v.active) { couple(v); spread(v); } }
    const double gk = 1 - std::exp(-3 / js::max(1, s.glide * 0.001 * sr));
    const double beA = 1 / js::max(1, s.benvA * 0.001 * sr), beD = 1 - std::exp(-4 / js::max(1, s.benvD * 0.001 * sr));
    const double beA2 = 1 / js::max(1, s.benvA2 * 0.001 * sr), beD2 = 1 - std::exp(-4 / js::max(1, s.benvD2 * 0.001 * sr));
    for (Voice& v : voices) {
      if (!v.active) continue;
      if (v.freq != v.freqT) {   // the pitch glide, in log frequency
        const double lr = std::log(v.freqT / v.freq);
        v.freq = std::fabs(lr) < 1e-5 ? v.freqT : v.freq * std::exp(lr * gk);
      }
      v.gr = v.freq / v.fc;
      // blade envelope: linear attack, exponential decay to zero; scales cut rate and width
      if (v.bst == 1) { v.be += beA; if (v.be >= 1) { v.be = 1; v.bst = 2; } }
      else if (v.bst == 2) { v.be -= v.be * beD; if (v.be < 1e-4) { v.be = 0; v.bst = 0; } }
      const double bE = v.be * v.bv;
      v.kE = js::truthy(s.benvK) ? js::pow(2, s.benvK * 4 * bE) : 1;
      v.wE = js::truthy(s.benvW) ? js::pow(2, s.benvW * 3 * bE) : 1;
      if (js::truthy(d.b2env)) {   // blade 2's own envelope
        if (v.bst2 == 1) { v.be2 += beA2; if (v.be2 >= 1) { v.be2 = 1; v.bst2 = 2; } }
        else if (v.bst2 == 2) { v.be2 -= v.be2 * beD2; if (v.be2 < 1e-4) { v.be2 = 0; v.bst2 = 0; } }
        const double bE2 = v.be2 * v.bv2;
        v.kE2 = js::truthy(s.benvK2) ? js::pow(2, s.benvK2 * 4 * bE2) : 1;
        v.wE2 = js::truthy(s.benvW2) ? js::pow(2, s.benvW2 * 3 * bE2) : 1;
      } else { v.kE2 = v.kE; v.wE2 = v.wE; }
      // the voice's amplitude ADSR: linear attack, exponential decay and release
      if (v.stage == 1) { v.env += attInc; if (v.env >= 1) { v.env = 1; v.stage = 2; } }
      else if (v.stage == 2) v.env += (s.Sus - v.env) * dC;
      else if (v.stage == 4) { v.env -= v.env * rC; if (v.env < 1e-4) { v.env = 0; v.active = false; } }
    }
    double yl = 0, yr = 0;
    const bool dcTick = dcOn && --dcCnt <= 0;
    if (dcTick) dcCnt = 256;
    const double w0 = s.w, d0 = s.depth, I0 = s.I, rs = s.rotRate / sr;
    // rotation is bipolar: a negative rate runs the blade backwards round the cycle
    const bool rOn = std::fabs(t.rotRate) > 0.004, sOn = t.rotSpread > 0.004;
    const double hk = 1 - std::exp(-1 / (0.03 * sr));
    if (rOn) { gRot += rs; gRot -= std::floor(gRot); } else gRot = home(gRot, hk);
    const bool rotPerNote = js::truthy(d.rotSync);
    // blade 2 rotates with blade 1, or on a clock of its own
    const bool own2 = js::truthy(s.b2on) && !js::truthy(d.rot2Follow);
    const double rs2 = s.rotRate2 / sr;
    const bool rOn2 = std::fabs(t.rotRate2) > 0.004;
    const bool sOn2 = js::truthy(d.b2sp) ? t.rotSpread2 > 0.004 : sOn;
    if (own2) { if (rOn2) { gRot2 += rs2; gRot2 -= std::floor(gRot2); } else gRot2 = home(gRot2, hk); }
    for (Voice& v : voices) {
      if (!v.active) continue;
      if (rOn) { v.rot += rs; v.rot -= std::floor(v.rot); } else v.rot = home(v.rot, hk);
      if (own2) { if (rOn2) { v.rot2 += rs2; v.rot2 -= std::floor(v.rot2); } else v.rot2 = home(v.rot2, hk); }
      for (int q = 0; q < N; q++) {
        Member& mm = v.m[q];
        if (sOn) { mm.rot += mm.rotOff / sr; mm.rot -= std::floor(mm.rot); } else mm.rot = home(mm.rot, hk);
        if (own2) { if (sOn2) { mm.rot2 += mm.rotOff2 / sr; mm.rot2 -= std::floor(mm.rot2); } else mm.rot2 = home(mm.rot2, hk); }
      }
    }
    for (int j = 0; j < os; j++) {   // int against double, as JS's `j < os`
      if (events) evTick = static_cast<uint32_t>(static_cast<double>(evSample) * os + j);
      double accL = 0, accR = 0;
      for (int vi = 0; vi < kVoices; vi++) {
        Voice& v = voices[vi];
        if (!v.active) continue;
        double vl = 0, vr = 0;
        const bool xOn = s.xm > 0.0005 || s.fb > 0.0005;
        double* xb = v.xb;
        if (xOn) for (int q = 0; q < N; q++) xb[q] = v.m[q].y1;
        for (int q = 0; q < N; q++) {
          Member& mm = v.m[q];
          // cross-member modulation: a phase push from the next member round the ring; feedback: from itself
          const double xin = xOn ? 0.5 * (H2E_EPS13(s.xm) * xb[(q + 1) % N] + s.fb * 0.5 * (mm.y1 + mm.y2)) : 0;
          mm.ns.xin = xin; mm.ns2.xin = xin; mm.bx.ns3.xin = xin; mm.bx.ns4.xin = xin;
          // the member's overrides of the shared parameters, restored after the voices.
          // B441 C1: computed at the sample's first step and replayed at the rest, the
          // same expressions in the same order evaluated once, so bit for bit what a
          // per-step evaluation gives. That holds because every input is written only
          // per sample or slower, never inside this j loop: w0/d0/I0 and the s.*/d.*
          // fields read here (the smoothers, render-call setup), v.rot/rot2, gRot/gRot2,
          // mm.rot/rot2, v.gr, v.kE/kE2, v.wE/wE2 (the per-sample voice pass above),
          // and mm.inc, lead, cOff*, kAdd*, kMul*, wMul*, dAdd*, iMul*, mr*, mn* (couple
          // and spread, every 32 samples, above). Inside the loop, stepMember's swarm
          // work (tickSwarm, unLookAhead, memberStep) writes v.sw, v.tick0, v.gf0/gfb,
          // v.env and the members' entry and envelope state; the DC estimates write the
          // scratch states sc/sc2/bxs and only borrow gmr/gmn; fillG2 never reads the
          // four fields this block overrides. Blade 2's view (mm.bx.c/k/mr/mn and m.g2
          // through bx.g) is written by nothing else, so it persists and is skipped
          // outright. A voice's `active` changes only per sample or between calls.
          MemberOv& o = memberOv[vi][q];
          if (j == 0) {
            o.w = w0 < 0.004 ? w0 : js::min(1, w0 * mm.wMul * v.wE);
            o.depth = js::min(1, js::max(0, d0 + mm.dAdd));
            o.I = I0 * mm.iMul;
            const double rotAll = (rotPerNote ? v.rot : gRot) + mm.rot, fr2 = d.frame2 < 0 ? d.frame : d.frame2;
            o.c = s.c + rotAll + mm.cOff + (js::truthy(d.frame) ? mm.lead : 0); o.c -= std::floor(o.c);
            const double fi = js::max(1, mm.inc * v.gr);
            const double kCap = 0.45 * sr * os / fi;   // every carrier under the oversampled Nyquist
            o.kq = js::min((d.lock == 2 ? js::max(0.05, s.kHz / fi + mm.kAdd) : js::max(0.25, s.k + mm.kAdd)) * mm.kMul * v.kE, kCap);
            if (js::truthy(s.b2on)) {
              BX& bx = mm.bx;
              const double rot2All = own2 ? (rotPerNote ? v.rot2 : gRot2) + mm.rot2 : rotAll;
              bx.c = s.c2 + rot2All + mm.cOff2 + (js::truthy(fr2) ? mm.lead : 0); bx.c -= std::floor(bx.c);
              const double lock2 = d.lock2 < 0 ? d.lock : d.lock2;
              bx.k = js::min((lock2 == 2 ? js::max(0.05, s.kHz2 / fi + mm.kAdd2) : js::max(0.25, s.k2 + mm.kAdd2)) * mm.kMul2 * v.kE2, kCap);
              const double mEff2 = js::truthy(d.b2fm) ? (js::truthy(d.mUnit2) ? s.mHz2 / fi : s.m2) : (js::truthy(d.mUnit) ? s.mHz / fi : s.m);
              fillG2(*bx.g, s.w2 < 0.004 ? s.w2 : js::min(1, s.w2 * mm.wMul2 * v.wE2), js::min(1, js::max(0, s.depth2 + mm.dAdd2)),
                     (js::truthy(d.b2fm) ? s.I2 : I0) * mm.iMul2, mEff2);
              bx.mr = mm.mr2; bx.mn = mm.mn2;
            }
            o.mEff = js::truthy(d.mUnit) ? s.mHz / fi : s.m;
          }
          s.w = o.w; s.depth = o.depth; s.I = o.I;
          gmr = mm.mr; gmn = mm.mn;
          const double c = o.c, kq = o.kq;
          s.mEff = o.mEff;
          if (events) evId = static_cast<uint32_t>(vi * kMembers + q);
          double y = stepMember(v, mm, c, kq);
          if (xOn) { mm.y2 = mm.y1; mm.y1 = y; }
          if (dcOn) {
            // big numeric estimates refresh less often, so the cost stays about constant
            if (dcTick && j == 0 && !H2E_SKIP(kStageDc) && (--mm.dcWait <= 0 || mm.dcInit)) {
              if (js::truthy(s.b2on) && s.b2mix > 1e-6) mm.dc = dcPair(mm, c, kq, s);
              else {
                mm.dc = dcEst(mm.ns, mm.modX, c, kq, s);
                const double j1 = dcJ;
                if (js::truthy(s.b2on)) {
                  const double mr0 = gmr, mn0 = gmn; gmr = mm.bx.mr; gmn = mm.bx.mn;
                  mm.dc += dcEst(mm.bx.ns3, mm.bx.modX, mm.bx.c, mm.bx.k, *mm.bx.g); dcJ += j1;
                  gmr = mr0; gmn = mn0;
                }
              }
              mm.dcWait = js::max(1, std::ceil(dcJ / 64));
              if (mm.dcInit) { mm.dcS = mm.dc; mm.dcInit = false; }
            }
            mm.dcS += (mm.dc - mm.dcS) * 0.003;
            y -= mm.dcS;
          }
          vl += y * gl[q]; vr += y * gr[q];
        }
        const double amp = v.env * v.vel * norm;
        accL += vl * amp; accR += vr * amp;
      }
      s.w = w0; s.depth = d0; s.I = I0;
      if (os > 1 && !H2E_SKIP(kStageDecim)) {
        accL = bqf(bqL[1], bqf(bqL[0], accL));
        accR = bqf(bqR[1], bqf(bqR[0], accR));
      }
      yl = accL; yr = accR;
    }
    // cross-mod and feedback make a cycle depend on the previous sample, which the
    // per-cycle estimate cannot see: the blocker drains any residual offset
    if (H2E_SKIP(kStageOut)) { L[i] = yl; R[i] = yr; continue; }
    if (d.dcMode == 1 || (d.dcMode == 2 && (s.xm > 0.0005 || s.fb > 0.0005 || (js::truthy(s.b2on) && (js::truthy(s.colK) || js::truthy(s.colB)))))) {
      const double ol = yl - hx[0] + hpR * hy[0]; hx[0] = yl; hy[0] = ol; yl = ol;
      const double orr = yr - hx[1] + hpR * hy[1]; hx[1] = yr; hy[1] = orr; yr = orr;
    }
    L[i] = std::tanh(yl * s.gain * 1.6);
    R[i] = std::tanh(yr * s.gain * 1.6);
  }
}

#undef H2E_FAULT
#undef H2E_EPS
#undef H2E_EPS13
#undef H2E_SKIP

}  // namespace horde2::engine
