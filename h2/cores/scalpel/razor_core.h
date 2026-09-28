/*
 * razor_core.h — horde 2's SCALPEL blade engine, a FAITHFUL C++ port of the
 * parity oracle reference/scalpel/prototype/razor-core.js (v1.1 as ingested,
 * blade interplay included). ROADMAP B332, phase 1a.
 *
 * WHAT THIS FILE IS. The oracle's class `RazorCore`, transcribed line for line:
 * the same structure, the same order of operations, the same member-state
 * layout, and the same order of Math.random draws. It is NOT the composed engine
 * (horde's swarm driving these blades; docs/design/scalpel-horde-engine.js) —
 * that is phase 1b. Nothing here is a design decision; where the oracle has a
 * quirk, the quirk is ported (docs/port/scalpel-phase-1a.md lists them as
 * divergence CANDIDATES for the ADR-187 ledger; none is diverged here).
 *
 * WHERE IT LIVES AND WHY (ADR-186 item 4): horde 2's cores are COPIED FORWARD
 * into h2/cores/ and their own namespace. The namespace is load-bearing: the
 * legacy cores in src/ are header-only too, and one link that saw two classes of
 * the same name would be a silent ODR violation. Nothing in src/ includes this
 * file and this file includes nothing from src/.
 *
 * PARITY CONTRACT (ADR-187 item 6; oracle: tools/h2_scalpel_parity_check.cpp).
 * The parity build compiles this in DOUBLES with -ffp-contract=off (clang's
 * default `on` fuses a*b+c inside one expression, which V8 never does). The
 * JS-semantics traps are handled by the `js::` helpers below, never by the
 * std:: equivalent that "looks the same":
 *   - Math.round is floor(x + 0.5), NOT std::round (they differ at negative
 *     halves: Math.round(-2.5) = -2, std::round(-2.5) = -3);
 *   - Math.min/Math.max return NaN if either operand is NaN and order ±0
 *     (std::min/max return an operand depending on argument order);
 *   - `switch` on a JS number is strict equality, so a non-integral selector
 *     takes `default` — js::sel() maps it to a value no case matches, where a
 *     C++ int cast would truncate 1.5 to case 1;
 *   - JS truthiness of a number is "non-zero and not NaN" — C++ reads NaN as
 *     true, so every `if (x)` on a double goes through js::truthy();
 *   - Math.pow(±1, ±Infinity) and Math.pow(x, NaN) are NaN in JS, 1 in C.
 * The five Math.random sites (static rnd(), startVoice's phase/modX/random-law
 * draws, settle(), the drift law's Box-Muller pair) consume ONE mulberry32
 * stream, the same generator the JS harness installs over Math.random
 * (tools/labharness/composed_engine_check.mjs:85). A different draw order is a
 * different instrument, so the draws stay in the oracle's order even where a
 * reorder would be "equivalent".
 *
 * WHAT IS NOT BIT-EXACT BY CONSTRUCTION: libm. V8 carries its own sin, cos,
 * exp, log, atan2, asin, tanh and pow; this file calls the platform's. They can
 * differ in the last bit (LIBRARY L0066), which a chaotic regime amplifies
 * (ADR-065). That is measured by the parity check, not hidden here.
 *
 * REAL-TIME SAFETY: all state is preallocated in the object (8 voices x 9
 * members, the note stack, the rule-list cache). render() allocates nothing,
 * locks nothing and reads no clock. set()/setString() are message-thread calls
 * in the oracle and here; setString() copies into a fixed buffer.
 *
 * NON-INVASIVE EVENT COUNTERS: if `events` is non-null, every PolyBLEP
 * correction (tryE and scan) and every blade-window entry is reported with its
 * oversampled tick and member id. The pointer is only read; it never changes
 * the arithmetic, and it is null in any shipping build.
 *
 * FAULT INJECTION (H2_SCALPEL_FAULTS): compiled ONLY into the parity check, so
 * its must-fail controls plant real port faults into this code rather than into
 * a copy of it. Undefined elsewhere, the sites fold to `false`.
 *
 * DOMAIN. Inputs the oracle cannot survive are clamped here rather than read
 * out of bounds: N outside 1..9 (the JS indexes PANS[N-1] and throws), a poly
 * pool of zero (the JS throws on an undefined voice), and note numbers outside
 * 0..127 (the JS keys its mono note table by any number). Inside the domain the
 * clamps are inert.
 */
#pragma once

#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace horde2::scalpel {

// ---- JavaScript number semantics (ADR-187 item 6) ---------------------------
namespace js {
inline double round(double x) { return std::floor(x + 0.5); }
inline double max(double a, double b) {
  if (std::isnan(a) || std::isnan(b)) return std::nan("");
  if (a > b) return a;
  if (b > a) return b;
  return std::signbit(a) ? b : a;   // equal: prefers +0 over -0
}
inline double min(double a, double b) {
  if (std::isnan(a) || std::isnan(b)) return std::nan("");
  if (a < b) return a;
  if (b < a) return b;
  return std::signbit(a) ? a : b;   // equal: prefers -0 over +0
}
inline double sign(double x) {
  if (std::isnan(x)) return x;
  if (x > 0) return 1;
  if (x < 0) return -1;
  return x;                          // ±0 keeps its sign
}
inline bool truthy(double x) { return x != 0 && !std::isnan(x); }
// switch selector: an integral value selects its case; anything else matches none.
inline int sel(double x) {
  return (x == std::floor(x) && std::fabs(x) < 1e9) ? static_cast<int>(x) : INT_MIN;
}
inline double pow(double x, double y) {
  if (std::isnan(y)) return std::nan("");
  if (std::fabs(x) == 1 && std::isinf(y)) return std::nan("");
  return std::pow(x, y);
}
inline double frac(double x) { return x - std::floor(x); }
}  // namespace js

// The generator the JS harness installs over Math.random (mulberry32). Unsigned
// 32-bit wraparound reproduces Math.imul and the `| 0` / `>>> 0` coercions.
struct Mulberry32 {
  uint32_t a = 0;
  double next() {
    a = a + 0x6D2B79F5u;
    uint32_t t = (a ^ (a >> 15)) * (1u | a);
    t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
    return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
  }
};

// Blade-event sink (see header). kind: 1 = edge/base BLEP (tryE), 2 = carrier
// BLEP (scan), 3 = blade-1 window entry, 4 = blade-2 window entry. Two
// independent 32-bit order-sensitive hashes over (tick, id*8 + kind), so two
// streams agree only if counts, times, members and kinds agree in order.
struct EventLog {
  uint64_t count[5] = {0, 0, 0, 0, 0};
  uint32_t h1 = 2166136261u, h2 = 0;
  void add(uint32_t tick, uint32_t id, uint32_t kind) {
    count[kind]++;
    const uint32_t w = id * 8u + kind;
    h1 = (h1 ^ tick) * 16777619u;
    h1 = (h1 ^ w) * 16777619u;
    h2 = h2 * 31u + tick;
    h2 = h2 * 31u + w;
  }
};

// Every field voice(), gate(), scan() and dcEst() read. The oracle passes its
// main parameter object `s` and blade 2's view `g` through the same functions;
// here the main object derives from this, so both bind to one signature.
struct Blade {
  double mode = 4, hot = 2, base = 0, w = .2, depth = 1, hard = 0, lock = 0, mirror = 0,
         fmType = 0, I = 2, mshape = 0, mEff = 3.37, m = 3.37, colB = 0;   // RazorCore.g2()
};

// The oracle's `s` (smoothed) and `t` (target) objects: the 64 target keys plus
// the fields render()/spread() copy in from `d`. Initial values are the JS
// constructor's (s and t start equal; checked by hand against razor-core.js:315-320).
struct SP : Blade {
  double b2mix = 0, colK = 0, I2 = 2, m2 = 3.37, mHz2 = 660, morph2 = .5, mspread2 = 0, ispread2 = 0,
         rotRate2 = 0, rotSpread2 = 0, kRuleAmt2 = 1, bspread2 = 0, kspread2 = 0, wspread2 = 0,
         dspread2 = 0, benvA2 = 2, benvD2 = 250, benvK2 = 0, benvW2 = 0, benvVel2 = .5, kRuleAmt = 1,
         w2 = .2, k2 = 3, kHz2 = 800, c2 = .35, depth2 = 1, hard2 = 0, xm = 0, fb = 0, benvA = 2,
         benvD = 250, benvK = 0, benvW = 0, benvVel = .5, glide = 60, driftRate = .5, morph = .5,
         wspread = 0, dspread = 0, mspread = 0, ispread = 0, k = 6, kHz = 1320, mHz = 660, c = .875,
         rotRate = 0, rotSpread = 0, gain = .35, detune = 14, K = .35, bspread = 0, kspread = 0,
         width = .7, A = 4, D = 400, Sus = .85, R = 280, bend = 0;
  // s-only fields (copied from d at block start / in spread())
  double b2order = 0, lock2 = -1, mirror2 = -1, b2fm = 0, fmType2 = 0, mshape2 = 0, mUnit2 = 0,
         b2sp = 0, kRule2 = 0, kRule = 0, mode2 = 4, hot2 = 2, b2on = 0, aa = 1, kq = 0, law = 0;
  SP() { mode = 0; hot = 2; base = 0; w = .25; depth = 1; hard = 0; lock = 0; mirror = 0;
         fmType = 0; I = 2; mshape = 0; mEff = 3.37; m = 3.37; colB = 0; }
};

// The oracle's `d` object (discrete, block-rate). kCustom is the one string.
struct DP {
  double mode = 0, hot = 2, base = 0, lock = 0, N = 5, phaseMode = 2, poly = 6, mshape = 0, kq = 0,
         fmType = 0, mUnit = 0, mirror = 0, dcMode = 2, law = 0, frame = 0, frame2 = -1,
         rot2Follow = 1, lock2 = -1, mirror2 = -1, b2fm = 0, fmType2 = 0, mshape2 = 0, mUnit2 = 0,
         cScale = 0, rotSync = 1, panOrder = 0, aa = 1, polyMode = 0, glideAlways = 0, b2on = 0,
         mode2 = 4, hot2 = 2, kRule = 0, b2sp = 0, kRule2 = 0, b2env = 0, b2order = 0;
  static constexpr int kCustomCap = 256;
  char kCustom[kCustomCap] = "1, 5/4, 3/2";
};

// Per-blade state (the oracle's `ns` objects). `draws` separates a member's
// state (rnd() consumes Math.random) from the scratch states sc/sc2/bxs (their
// rnd() returns this.val and draws nothing).
struct NS {
  double idx = 0, val = 0;
  bool inside = false;
  double g = 0, seed = 0, acc = 0, xin = 0, cacc = 0, cd = 0, ov = 0, pv = 0;
  bool draws = true;
};

struct BX {
  Blade* g = nullptr;
  double c = 0, k = 3, modX = 0, mr = 0, mn = 1;
  NS ns3, ns4;
};

struct Member {
  double phi = 0, modX = 0, inc = 0, prev = 0, dc = 0, dcS = 0;
  bool dcInit = true;
  double rv[14] = {}, rvT[14] = {};
  double dcWait = 0, lead = 0, kMul = 1, rot2 = 0, rotOff2 = 0, iMul2 = 1, mr2 = 0, mn2 = 1,
         cOff2 = 0, kAdd2 = 0, kMul2 = 1, wMul2 = 1, dAdd2 = 0, rot = 0, cOff = 0, kAdd = 0,
         rotOff = 0, wMul = 1, dAdd = 0, iMul = 1, mor = 0, kEff = 6, mr = 0, mn = 1;
  NS ns, ns2;
  double y1 = 0, y2 = 0;
  Blade g2;   // storage for bx.g (the oracle's per-member g2() object)
  BX bx;
};

struct Voice {
  bool active = false;
  double note = std::nan(""), freq = 110, freqT = 110, fc = 110, vel = 1, env = 0;
  int stage = 0;
  bool gate = false;
  double age = 0, r = 1, rot = 0;
  Member m[9];
  double rot2 = 0, be = 0;
  int bst = 0;
  double bv = 1, kE = 1, wE = 1, be2 = 0;
  int bst2 = 0;
  double bv2 = 1, kE2 = 1, wE2 = 1, gr = 1;
  double xb[9] = {};
};

struct Biquad { double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0; };

class RazorCore {
 public:
  static constexpr int kVoices = 8, kMembers = 9;
  static constexpr double TAU = 6.283185307179586;

  EventLog* events = nullptr;   // non-invasive counters (see header)
#ifdef H2_SCALPEL_FAULTS
  int fault = 0;                // must-fail controls only (tools/h2_scalpel_parity_check.cpp)
#define H2_FAULT(n) (fault == (n))
#else
#define H2_FAULT(n) false
#endif

  explicit RazorCore(double sampleRate) : sr(sampleRate) {
    for (int v = 0; v < kVoices; v++) initVoice(voices[v]);
    sc.inside = true; sc.draws = false;
    sc2.inside = true; sc2.draws = false;
    bxs.k = 1; bxs.ns3.inside = true; bxs.ns3.draws = false; bxs.ns4.inside = true; bxs.ns4.draws = false;
    os = 0; setOS(2);
  }

  // Members hold pointers into their own voice (bx.g -> g2), so a copy would alias.
  RazorCore(const RazorCore&) = delete;
  RazorCore& operator=(const RazorCore&) = delete;

  // The harness's Math.random replacement: mulberry32(seed >>> 0).
  void seedRandom(uint32_t seed) { rng.a = seed; }

  // set({key: value}) for one numeric key. Returns false for an unknown key (the
  // oracle ignores those silently; the harness wants to know).
  bool set(const char* key, double value) {
    if (std::strcmp(key, "polyMode") == 0 && value != d.polyMode) {
      for (Voice& v : voices) { v.gate = false; v.stage = 4; }
      stackN = 0;
    }
    if (std::strcmp(key, "os") == 0) { if (value != os) setOS(value); return true; }
    if (double SP::*p = tKey(key)) { t.*p = value; return true; }
    if (double DP::*p = dKey(key)) {
      d.*p = value;
      if (p == &DP::N) d.N = js::min(9, js::max(1, std::floor(d.N)));   // domain clamp (header)
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
  // Object.assign(c.s, c.t): what the oracle's own render-goldens.js does after
  // loading a preset, so a patch starts at its values rather than gliding in.
  void snap() { for (int i = 0; i < kTKeys; i++) s.*tKeys()[i].p = t.*tKeys()[i].p; }

  void noteOn(int note, double freq, double vel) {
    if (note < 0 || note > 127) return;   // domain (header)
    if (js::truthy(d.polyMode)) { monoOn(note, freq, vel); return; }
    const int P = poolSize();
    if (P == 0) return;                   // domain (header)
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
  void noteOff(int note) {
    if (js::truthy(d.polyMode)) {
      stackRemove(note);
      Voice& v = voices[0];
      if (!v.active || !v.gate || v.note != note) return;
      if (stackN) {
        const int n = stack[stackN - 1];
        v.note = n; v.freqT = nf[n];
        if (s.glide <= 1) v.freq = v.freqT;
      } else { v.gate = false; v.stage = 4; }
      return;
    }
    for (Voice& v : voices) if (v.active && v.note == note && v.gate) { v.gate = false; v.stage = 4; }
  }
  void retune(int note, double freq) { for (Voice& v : voices) if (v.active && v.note == note) v.freq = freq; }
  void panic() { for (Voice& v : voices) { v.gate = false; v.stage = 4; } }

  // render(L, R): one block. T is double for the parity build; the oracle writes
  // Float32Array, so a float instantiation reproduces the bench's own output.
  template <class T> void render(T* L, T* R, int n);

  // read-only views for oracles
  const Voice& voice(int i) const { return voices[i]; }
  const SP& smoothed() const { return s; }
  double oversample() const { return os; }

 private:
  // ---- parameter tables (key -> field) ------------------------------------
  struct TKey { const char* k; double SP::* p; bool perSample; };
  struct DKey { const char* k; double DP::* p; };
  static constexpr int kTKeys = 64;
  static const TKey* tKeys() {
    // The oracle's `this.t`, in its declaration order. perSample marks the 12 keys
    // excluded from the 16-sample loop (razor-core.js:321) because render()
    // glides them every sample instead.
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
    };
    for (const DKey& e : K) if (std::strcmp(e.k, k) == 0) return e.p;
    return nullptr;
  }

  // ---- static helpers of the oracle (razor-core.js:10-60) -------------------
  double rnd() { return rng.next() * 2 - 1; }
  double nsRnd(NS& ns) { return ns.draws ? rnd() : ns.val; }
  static double hash(double i) { const double x = std::sin(i * 127.1 + 311.7) * 43758.5453; return (x - std::floor(x)) * 2 - 1; }
  double wave(double s_, double x) const {
    switch (js::sel(s_)) {
      case 0: return std::sin(6.283185307179586 * x);
      case 1: return x < 0.25 ? 4 * x : (x < 0.75 ? 2 - 4 * x : 4 * x - 4);
      case 2: { const double y = x + 0.5; return 2 * (y - std::floor(y)) - 1; }
      case 4: { const double y = x + 0.5; return 1 - 2 * (y - std::floor(y)); }
      case 6: {
        const double r = js::truthy(gmr) ? gmr : 0;   // RazorCore.mr || 0
        if (r < 1e-4) return std::sin(6.283185307179586 * x);
        const double th = 6.283185307179586 * (x + 0.5);
        return -std::atan2(r * std::sin(th), 1 - r * std::cos(th)) / gmn;
      }
      default: return x < 0.5 ? 1 : -1;
    }
  }
  double mod(double shape, double x, double seed) const {
    if (shape == 7) return hash(std::floor(x) + seed);
    if (shape == 5) {
      const double i = std::floor(x), f = x - i, sm = f * f * (3 - 2 * f);
      const double a = hash(i + seed), b = hash(i + 1 + seed);
      return a + (b - a) * sm;
    }
    return wave(shape, x - std::floor(x));
  }
  static double F(double sh, double x) {
    switch (js::sel(sh)) {
      case 0: return (1 - std::cos(6.283185307179586 * x)) * 0.15915494309189535;
      case 1: { const double f = x - std::floor(x); return f < 0.25 ? 2 * f * f : f < 0.75 ? 2 * f - 2 * f * f - 0.25 : 2 * f * f - 4 * f + 2; }
      case 2: { double y = x + 0.5; y -= std::floor(y); return y * y - y; }
      case 4: { double y = x + 0.5; y -= std::floor(y); return y - y * y; }
      default: { const double f = x - std::floor(x); return f < 0.5 ? f : 1 - f; }
    }
  }
  static double crushAvg(double sh, double st, double kk, double j) { return (F(sh, st + (j + 1) / kk) - F(sh, st + j / kk)) * kk; }
  double crushLevel(double sh, double st, double kk, double hp, double sl) const {
    if (hp < 0) return wave(sh, st - std::floor(st));
    const double j = std::floor(hp), f = hp - j, a = crushAvg(sh, st, kk, j);
    if (sl > 0.001 && f < sl) {
      const double prev = j == 0 ? wave(sh, st - std::floor(st)) : crushAvg(sh, st, kk, j - 1);
      return prev + (a - prev) * f / sl;
    }
    return a;
  }
  static bool isFM(const Blade& p) { return p.mode == 1 || p.mode == 2; }
  static double panSlot(int N, int i) {
    static const double PANS[9][9] = {
      {0.0}, {-1.0, 1.0}, {-1.0, 1.0, 0.0}, {-0.3333, 1.0, -1.0, 0.3333}, {-0.5, 1.0, 0.0, -1.0, 0.5},
      {-0.2, 0.6, -1.0, 1.0, -0.6, 0.2}, {-0.3333, 1.0, -1.0, 0.6667, -0.6667, 0.3333, 0.0},
      {-0.1429, 0.4286, -0.7143, 1.0, -1.0, 0.7143, -0.4286, 0.1429},
      {-0.25, 1.0, -0.75, -0.5, 0.0, 0.5, 0.75, -1.0, 0.25}};
    return PANS[N - 1][i];
  }
  double home(double x, double k) const {
    double dd = x - (H2_FAULT(1) ? std::round(x) : js::round(x));
    dd -= dd * k;
    return std::fabs(dd) < 1e-6 ? 0 : dd;
  }

  // ---- cut-rate rules (razor-core.js:70-102) --------------------------------
  // parseFloat: the longest prefix JS's StrDecimalLiteral accepts, then strtod on
  // exactly that prefix (strtod alone also accepts hex, "inf" and "nan").
  static double jsParseFloat(const char* s, int n) {
    int i = 0;
    if (i < n && (s[i] == '+' || s[i] == '-')) i++;
    if (n - i >= 8 && std::strncmp(s + i, "Infinity", 8) == 0) return s[0] == '-' ? -INFINITY : INFINITY;
    int digits = 0;
    while (i < n && s[i] >= '0' && s[i] <= '9') { i++; digits++; }
    if (i < n && s[i] == '.') { i++; while (i < n && s[i] >= '0' && s[i] <= '9') { i++; digits++; } }
    if (!digits) return std::nan("");
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
      int j = i + 1;
      if (j < n && (s[j] == '+' || s[j] == '-')) j++;
      int ed = 0;
      while (j < n && s[j] >= '0' && s[j] <= '9') { j++; ed++; }
      if (ed) i = j;
    }
    char buf[128];
    if (i > 127) i = 127;
    std::memcpy(buf, s, i); buf[i] = 0;
    return std::strtod(buf, nullptr);
  }
  // parseRatios(d.kCustom). Only the first 9 ratios and the count can matter:
  // ruleList indexes CU[j % len] for j < N <= 9.
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
  // ruleList(rule, N, custom): the oracle memoises per key; so does this, in a
  // fixed table (rules 1..9 x N 1..9; rule 9's rows are dropped when kCustom
  // changes). Values are a pure function of the key either way.
  const double* ruleList(double rule, int N) {
    const int r = js::sel(rule);
    if (r >= 1 && r <= 9 && rlValid[r][N]) return rl[r][N];
    double* L = (r >= 1 && r <= 9) ? rl[r][N] : rlTmp;
    static const double PR[9] = {2, 3, 5, 7, 11, 13, 17, 19, 23};
    auto chord = [](const double* set, int len, int j) { return set[j % len] * std::pow(2, std::floor(static_cast<double>(j) / len)); };
    static const double MAJ[3] = {1, 5.0 / 4, 3.0 / 2}, MIN[3] = {1, 6.0 / 5, 3.0 / 2};
    for (int j = 0; j < N; j++) {
      switch (r) {
        case 1: L[j] = j + 1; break;
        case 2: L[j] = 1.0 / (j + 1); break;
        case 3: L[j] = std::pow(2, j); break;
        case 4: L[j] = chord(MAJ, 3, j); break;
        case 5: L[j] = chord(MIN, 3, j); break;
        case 6: L[j] = std::pow(1.5, j); break;
        case 7: L[j] = std::pow(1.6180339887, j); break;
        case 8: L[j] = PR[j] / 2; break;
        default: L[j] = r == 9 ? cu[j % cuLen] * std::pow(2, std::floor(static_cast<double>(j) / cuLen)) : 1;
      }
    }
    if (r >= 1 && r <= 9) rlValid[r][N] = true;
    return L;
  }
  void kSpread(double rule, double amt, double spreadAmt, double q, int N, double pv, double out[2]) {
    out[0] = 0; out[1] = 1;
    if (!js::truthy(rule)) {
      const double rq = H2_FAULT(1) ? std::round(spreadAmt) : js::round(spreadAmt);
      double ko = (js::truthy(q) ? rq : spreadAmt) * pv;
      if (js::truthy(q)) ko = js::sign(ko) * (H2_FAULT(1) ? std::round(std::fabs(ko)) : js::round(std::fabs(ko)));
      out[0] = ko;
    } else if (N > 1) {
      const double* L = ruleList(rule, N);
      const double x = js::min(N - 1, js::max(0, (pv + 0.5) * (N - 1)));
      double r;
      if (js::truthy(q)) r = L[static_cast<int>(H2_FAULT(1) ? std::round(x) : js::round(x))];
      else { const double a = std::floor(x), b = js::min(N - 1, a + 1), f = x - a;
             r = std::exp(std::log(L[static_cast<int>(a)]) * (1 - f) + std::log(L[static_cast<int>(b)]) * f); }
      out[1] = js::pow(r, amt);
    }
  }
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
    else {
      m.iMul2 = js::pow(2, s.ispread2 * 4 * pn(13));
      m.cOff2 = s.bspread2 * pn(7);
      double k2s[2];
      kSpread(s.kRule2, s.kRuleAmt2, s.kspread2, s.kq, N, pn(8), k2s);
      m.kAdd2 = k2s[0]; m.kMul2 = k2s[1];
      m.wMul2 = js::pow(2, s.wspread2 * 4 * pn(9));
      m.dAdd2 = s.dspread2 * 2 * pn(10);
    }
    {
      const double mor2 = js::min(1, js::max(0, s.morph2 + (js::truthy(s.b2sp) ? s.mspread2 * 2 * pn(12) : s.mspread * 2 * pn(6))));
      const double lock2 = s.lock2 < 0 ? s.lock : s.lock2;
      const double k2 = js::min((lock2 == 2 ? js::max(0.05, s.kHz2 / js::max(1, fi) + m.kAdd2) : js::max(0.25, s.k2 + m.kAdd2)) * m.kMul2, 0.9 * nyq / js::max(1, fi));
      const double w2 = js::min(1, s.w2 * m.wMul2);
      const double fh2 = (lock2 == 1 ? k2 / js::max(w2, 1e-3) : k2) * fi;
      const double rmax2 = js::min(0.995, js::pow(0.01, fh2 / nyq)), r2 = 1 - js::pow(1 - rmax2, mor2);
      m.mr2 = r2; m.mn2 = r2 > 1e-4 ? std::asin(r2) : 1;
    }
  }

  // ---- the blade (razor-core.js:163-312) -----------------------------------
  double fmStep(const Blade& p, NS& ns, double e, bool entered, double kk, double modX, double dphi) {
    if (p.fmType != 1 || !isFM(p)) return 0;
    if (entered && p.mode == 1) ns.acc = 0;
    const double mv = mod(p.mshape, p.mode == 1 ? p.mEff * e : modX, ns.seed);
    const double dd = (js::pow(1 + 0.1 * p.I, mv) - 1) * kk * dphi;
    ns.acc += dd; ns.acc -= std::floor(ns.acc);
    return dd;
  }
  double voiceOut(const Blade& p, double phi, double c, double k, double modX, NS& ns, bool hasInp = false, double inp = 0) {
    const double base = wave(p.base, phi), xin0 = !hasInp ? base : inp;
    const double w = p.w;
    if (w < 0.004) { ns.g = 0; ns.inside = false; return xin0; }
    double st = c - w * 0.5; st -= std::floor(st);
    double e = phi - st; if (e < 0) e += 1;
    if (e >= w) { ns.g = 0; ns.inside = false; return xin0; }
    const double kk = p.lock == 1 ? k / w : k;
    const double er = p.mirror == 1 && e > w * 0.5 ? w - e : e;
    const double hp = kk * er;
    double hot = 0, x;
    switch (js::sel(p.mode)) {
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
      case 4: hot = std::sin(1.5707963267948966 * (1 + (k - 1) * 0.25) * (ns.ov > 0 ? 1 + 2 * p.colB * ns.ov : 1) * xin0); break;
      case 5: { const double cp = hp + ns.xin + ns.cacc; hot = xin0 * wave(p.hot, cp - std::floor(cp)); break; }
      case 6: {
        const double wEff = p.mirror == 1 ? w * 0.5 : w, hpEnd = kk * wEff, sl = p.hard;
        hot = crushLevel(p.base, st, kk, hp, sl);
        if (sl > 0.001) {
          const double rlE = js::min(sl, hpEnd), h0 = hpEnd - rlE;
          if (rlE > 1e-9 && hp > h0) {
            const double from = crushLevel(p.base, st, kk, h0, sl);
            x = st + wEff; const double tgt = wave(p.base, x - std::floor(x));
            hot = from + (tgt - from) * (hp - h0) / rlE;
          }
        }
        if (hasInp) {
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
    const double t_ = p.hard * w * 0.5;
    double g = 1;
    if (t_ > 1e-9) {
      if (e < t_) g = 0.5 - 0.5 * std::cos(3.141592653589793 * e / t_);
      else if (e > w - t_) g = 0.5 - 0.5 * std::cos(3.141592653589793 * (w - e) / t_);
    }
    ns.g = g;
    return xin0 + g * p.depth * (hot - xin0);
  }
  static double gate(double e, double w, double hard, double mode) {
    if (w < 0.004 || e >= w) return 0;
    if (mode == 6) return 1;
    const double t_ = hard * w * 0.5;
    if (t_ > 1e-9) { if (e < t_) return 0.5 - 0.5 * std::cos(3.141592653589793 * e / t_); if (e > w - t_) return 0.5 - 0.5 * std::cos(3.141592653589793 * (w - e) / t_); }
    return 1;
  }
  static void collide(double amt, NS& nsU, double eU0, double eU1, double kkU, double ov, double dphi) {
    nsU.ov = ov;
    if (eU1 < eU0) nsU.cacc = 0;
    const double dd = js::truthy(amt) ? (js::pow(4, amt * ov) - 1) * kkU * dphi : 0;
    nsU.cacc += dd; nsU.cd = dd;
  }
  double out(const SP& p, double phi, double c, double k, double modX, NS& ns, NS& ns2, BX* bx) {
    if (bx && p.b2mix > 1e-6) return outSerial(p, phi, c, k, modX, ns, ns2, *bx);
    double y = voiceOut(p, phi, c, k, modX, ns);
    double ph2 = -1;
    if (p.mirror >= 2) {
      ph2 = phi - 0.5; if (ph2 < 0) ph2 += 1;
      ns2.acc = ns.acc; ns2.xin = ns.xin;
      const double d2 = voiceOut(p, ph2, c, k, modX, ns2) - wave(p.base, ph2);
      y += p.mirror == 2 ? -d2 : d2;
    }
    if (bx) {
      const Blade& g = *bx->g;
      const double mr0 = gmr, mn0 = gmn;
      gmr = bx->mr; gmn = bx->mn;
      y += voiceOut(g, phi, bx->c, bx->k, bx->modX, bx->ns3) - wave(g.base, phi);
      if (g.mirror >= 2) {
        if (ph2 < 0) { ph2 = phi - 0.5; if (ph2 < 0) ph2 += 1; }
        bx->ns4.acc = bx->ns3.acc; bx->ns4.xin = bx->ns3.xin;
        const double d4 = voiceOut(g, ph2, bx->c, bx->k, bx->modX, bx->ns4) - wave(g.base, ph2);
        y += g.mirror == 2 ? -d4 : d4;
      }
      gmr = mr0; gmn = mn0;
    }
    return y;
  }
  struct Desc { const Blade* p; double c, k, modX; NS* st; NS* st2; double mr, mn; };
  double ev(const Desc& D, double ph, NS& st, bool hasInp = false, double inp = 0) {
    gmr = D.mr; gmn = D.mn;
    return voiceOut(*D.p, ph, D.c, D.k, D.modX, st, hasInp, inp);
  }
  double outSerial(const SP& p, double phi, double c, double k, double modX, NS& ns, NS& ns2, BX& bx) {
    const Blade& g = *bx.g;
    const double lam = p.b2mix;
    const bool up2 = !js::truthy(p.b2order);
    const double base = wave(p.base, phi);
    double ph2 = phi - 0.5; if (ph2 < 0) ph2 += 1;
    const double base2 = wave(p.base, ph2);
    const double mr0 = gmr, mn0 = gmn, mr1 = mr0, mn1 = mn0;
    const Desc d1{&p, c, k, modX, &ns, &ns2, mr1, mn1}, d2{&g, bx.c, bx.k, bx.modX, &bx.ns3, &bx.ns4, bx.mr, bx.mn};
    const Desc& A = up2 ? d1 : d2;
    const Desc& B = up2 ? d2 : d1;
    const Blade& pa = *A.p;
    const double sa = pa.mirror == 2 ? -1 : pa.mirror == 3 ? 1 : 0;
    const Blade& pb = *B.p;
    const double sb = pb.mirror == 2 ? -1 : pb.mirror == 3 ? 1 : 0;
    const double dA = ev(A, phi, *A.st) - base;
    double dA2 = 0;
    if (js::truthy(sa) || js::truthy(sb)) { A.st2->acc = A.st->acc; A.st2->xin = A.st->xin; dA2 = ev(A, ph2, *A.st2) - base2; }
    const double L = dA + sa * dA2, L2 = dA2 + sa * dA;
    const double x1 = base + lam * L, dB = ev(B, phi, *B.st, true, x1) - x1;
    double dB2 = 0;
    if (js::truthy(sb)) { B.st2->acc = B.st->acc; B.st2->xin = B.st->xin; const double x2 = base2 + lam * L2; dB2 = ev(B, ph2, *B.st2, true, x2) - x2; }
    gmr = mr0; gmn = mn0;
    return base + L + dB + sb * dB2;
  }
  void fillG2(Blade& g, double w2e, double dep2, double I2e, double mEff2) {
    g.mode = s.mode2; g.hot = s.hot2; g.base = s.base; g.w = w2e; g.depth = dep2; g.hard = s.hard2;
    g.lock = s.lock2 < 0 ? s.lock : s.lock2; g.mirror = s.mirror2 < 0 ? s.mirror : s.mirror2;
    g.fmType = js::truthy(s.b2fm) ? s.fmType2 : s.fmType; g.mshape = js::truthy(s.b2fm) ? s.mshape2 : s.mshape;
    g.I = I2e; g.mEff = mEff2; g.m = js::truthy(s.b2fm) ? s.m2 : s.m; g.colB = s.colB;
  }

  // ---- voices (razor-core.js:338-481) ---------------------------------------
  void initVoice(Voice& v) {
    for (int i = 0; i < kMembers; i++) {
      Member& m = v.m[i];
      m.ns.seed = i * 97; m.ns2.seed = i * 97 + 7;
      m.bx.g = &m.g2; m.bx.ns3.seed = i * 97 + 13; m.bx.ns4.seed = i * 97 + 19;
    }
  }
  int poolSize() const {
    // this.voices.slice(0, d.poly): ToIntegerOrInfinity, negative counts from the end.
    const double p = std::isnan(d.poly) ? 0 : std::trunc(d.poly);
    const double e = p < 0 ? js::max(kVoices + p, 0) : js::min(p, kVoices);
    return static_cast<int>(e);
  }
  void stackRemove(int note) {
    int w = 0;
    for (int i = 0; i < stackN; i++) if (stack[i] != note) stack[w++] = stack[i];
    stackN = w;
  }
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
    if (fresh) {
      if (!v.active) v.env = 0;
      for (Member& m : v.m) {
        if (H2_FAULT(2)) {   // must-fail control: the two draws swapped
          m.modX = rng.next() * 1000; m.phi = d.phaseMode == 1 ? 0 : rng.next();
        } else {
          m.phi = d.phaseMode == 1 ? 0 : rng.next(); m.modX = rng.next() * 1000;
        }
        m.bx.modX = m.modX + 317; m.prev = 0;
        m.ns.inside = false; m.ns.acc = 0; m.ns2.inside = false; m.inc = freq;
        m.dc = 0; m.dcS = 0; m.dcInit = true; m.y1 = 0; m.y2 = 0; m.bx.ns3.inside = false; m.bx.ns3.acc = 0; m.bx.ns4.inside = false;
      }
    }
    if (fresh) v.freq = v.freqT = freq;
    v.note = note; v.vel = vel; v.gate = true; v.active = true; v.age = ++age;
    if (retrig) {
      v.stage = 1; v.be = 0; v.bst = 1; v.bv = 1 - s.benvVel + s.benvVel * vel;
      v.be2 = 0; v.bst2 = 1; v.bv2 = 1 - s.benvVel2 + s.benvVel2 * vel;
    }
    if (!retrig) return;
    if (js::truthy(d.rotSync) || fresh) { v.rot = 0; v.rot2 = 0; for (Member& m : v.m) { m.rot = 0; m.rot2 = 0; } }
    for (Member& m : v.m) for (int j = 0; j < 14; j++) { m.rvT[j] = rng.next() - 0.5; if (fresh) m.rv[j] = m.rvT[j]; }
    if (fresh && d.phaseMode == 2 && d.N > 1) { v.freq = freq; settle(v); }
    dcCnt = 0;
  }
  double keff(const Voice& v) const {
    const double f = v.freq * js::pow(2, s.bend / 12);
    const double maxDev = 6.283185307179586 * f * (js::pow(2, s.detune / 1200) - 1);
    const double floor_ = 6.283185307179586 * 3 * (js::truthy(d.cScale) ? f / 110 : 1);
    return s.K * (1.5 * maxDev + floor_);
  }
  void settle(Voice& v) {
    const int N = static_cast<int>(d.N);
    const double Ke = std::fabs(keff(v));
    if (Ke < 1e-3) return;
    const double dt = js::min(0.01, 0.15 / Ke), f = v.freq * js::pow(2, s.bend / 12);
    if (s.K > 0) for (int i = 0; i < N; i++) v.m[i].phi = rng.next() * 0.15;
    for (int it = 0; it < 150; it++) {
      couple(v);
      for (int i = 0; i < N; i++) { Member& m = v.m[i]; m.phi += (m.inc - f) * dt; m.phi -= std::floor(m.phi); }
    }
  }
  void couple(Voice& v) {
    const int N = static_cast<int>(d.N);
    const double f = v.freq * js::pow(2, s.bend / 12);
    v.fc = v.freq;
    const double Keff = keff(v);
    const int H = N < 2 ? 1 : (s.K < 0 ? (N - 1 < 6 ? N - 1 : 6) : 1);
    for (int h = 1; h <= H; h++) {
      double sx = 0, sy = 0;
      for (int i = 0; i < N; i++) { const double a = TAU * h * v.m[i].phi; sx += std::cos(a); sy += std::sin(a); }
      Rx[h] = sx / N; Ry[h] = sy / N;
    }
    v.r = std::hypot(Rx[1], Ry[1]);   // viz-only in the oracle (posted, never rendered)
    if (N > 1) {
      const double psi = std::atan2(Ry[1], Rx[1]) / TAU;
      for (int i = 0; i < N; i++) { double l = v.m[i].phi - psi + 0.5; l -= std::floor(l); v.m[i].lead = l - 0.5; }
    } else v.m[0].lead = 0;
    for (int i = 0; i < N; i++) {
      const double cents = N > 1 ? s.detune * (2.0 * i / (N - 1) - 1) : 0;
      const double fi = f * js::pow(2, cents / 1200);
      double coup = 0;
      if (N > 1) for (int h = 1; h <= H; h++) {
        const double a = TAU * h * v.m[i].phi;
        coup += (Ry[h] * std::cos(a) - Rx[h] * std::sin(a)) / h;
      }
      v.m[i].inc = js::max(0, fi + Keff * coup / TAU);
    }
  }
  void spread(Voice& v) {
    const int N = static_cast<int>(d.N);
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
  }

  // ---- output filters (razor-core.js:503-519) -------------------------------
  static Biquad mk(double fs, double fc, double Q) {
    const double w0 = 6.283185307179586 * fc / fs, cs = std::cos(w0), al = std::sin(w0) / (2 * Q);
    const double a0 = 1 + al, b0 = (1 - cs) / 2;
    Biquad b; b.b0 = b0 / a0; b.b1 = (1 - cs) / a0; b.b2 = b0 / a0; b.a1 = (-2 * cs) / a0; b.a2 = (1 - al) / a0;
    return b;
  }
  void setOS(double n) {
    os = n;
    const double fs = sr * n, fc = 0.45 * sr;
    bqL[0] = mk(fs, fc, 0.5412); bqL[1] = mk(fs, fc, 1.3066);
    bqR[0] = mk(fs, fc, 0.5412); bqR[1] = mk(fs, fc, 1.3066);
  }
  static double bqf(Biquad& f, double x) {
    const double y = f.b0 * x + f.z1;
    f.z1 = f.b1 * x - f.a1 * y + f.z2;
    f.z2 = f.b2 * x - f.a2 * y;
    return y;
  }

  // ---- anti-aliasing (razor-core.js:520-723) --------------------------------
  double hAt(Member& m, double E, double c, double k, const SP& s_) {
    sc.seed = m.ns.seed; sc.acc = m.ns.acc; sc2.seed = m.ns2.seed;
    sc.idx = m.ns.idx; sc.val = m.ns.val; sc.inside = true;
    sc2.idx = m.ns2.idx; sc2.val = m.ns2.val; sc2.inside = true;
    sc.xin = m.ns.xin; sc2.xin = m.ns.xin;
    sc.cacc = m.ns.cacc; sc.ov = m.ns.ov; sc.pv = m.ns.pv; sc2.cacc = 0; sc2.ov = 0; sc2.pv = m.ns2.pv;
    BX* bx = nullptr;
    if (js::truthy(s_.b2on)) {
      BX& src = m.bx; BX& b = bxs; bx = &b;
      b.g = src.g; b.c = src.c; b.k = src.k; b.modX = src.modX; b.mr = src.mr; b.mn = src.mn;
      b.ns3.seed = src.ns3.seed; b.ns3.acc = src.ns3.acc; b.ns3.idx = src.ns3.idx; b.ns3.val = src.ns3.val; b.ns3.inside = true; b.ns3.xin = src.ns3.xin;
      b.ns4.seed = src.ns4.seed; b.ns4.idx = src.ns4.idx; b.ns4.val = src.ns4.val; b.ns4.inside = true;
      b.ns3.cacc = src.ns3.cacc; b.ns3.ov = src.ns3.ov; b.ns3.pv = src.ns3.pv; b.ns4.cacc = 0; b.ns4.ov = 0; b.ns4.pv = src.ns4.pv;
    }
    const double a = out(s_, js::frac(E + 1e-7), c, k, m.modX, sc, sc2, bx);
    sc.idx = m.ns.idx; sc.inside = true; sc2.idx = m.ns2.idx; sc2.inside = true;
    if (bx) { bx->ns3.idx = m.bx.ns3.idx; bx->ns3.inside = true; bx->ns4.idx = m.bx.ns4.idx; bx->ns4.inside = true; }
    const double b = out(s_, js::frac(E - 1e-7), c, k, m.modX, sc, sc2, bx);
    return a - b;
  }
  double dcEst(NS& ns, double modX, double c, double k, const Blade& s_) {
    const double w = s_.w;
    dcJ = 0;
    if (w < 0.004) return 0;
    const double fac = s_.mirror == 2 ? 0 : s_.mirror == 3 ? 2 : 1;
    if (!js::truthy(fac)) return 0;
    double st = c - w * 0.5; st -= std::floor(st);
    const double kk = s_.lock == 1 ? k / w : k;
    if (s_.mode == 0 && s_.hot != 6) {
      const double hotInt = s_.mirror == 1 ? 2 * (F(s_.hot, kk * w * 0.5) - F(s_.hot, 0)) / kk : (F(s_.hot, kk * w) - F(s_.hot, 0)) / kk;
      double total = hotInt - (F(s_.base, st + w) - F(s_.base, st));
      const double t_ = s_.hard * w * 0.5;
      if (t_ > 1e-9) {
        const double Jt = js::min(512, js::max(32, std::ceil(kk * t_ * 32)));
        dcJ = 2 * Jt;
        sc.inside = true;
        double cut = 0;
        for (int side = 0; side < 2; side++) for (double j = 0; j < Jt; j++) {
          const double e = side == 0 ? (j + 0.5) / Jt * t_ : w - (j + 0.5) / Jt * t_;
          const double er = s_.mirror == 1 && e > w * 0.5 ? w - e : e;
          double hp = kk * er; hp -= std::floor(hp);
          double phi = st + e; phi -= std::floor(phi);
          const double g = 0.5 - 0.5 * std::cos(3.141592653589793 * (side == 0 ? e : w - e) / t_);
          cut += (1 - g) * (wave(s_.hot, hp) - wave(s_.base, phi));
        }
        total -= cut * t_ / Jt;
      }
      return fac * s_.depth * total;
    }
    double feats = kk * w;
    if (isFM(s_)) feats *= 1 + 0.5 * s_.I;
    feats += s_.mEff * w;
    const double J = js::min(1024, js::max(48, std::ceil(feats * 32)));
    dcJ = J;
    sc.seed = ns.seed; sc.acc = ns.acc; sc.idx = ns.idx; sc.val = s_.mode == 3 ? 0 : ns.val; sc.xin = ns.xin;
    double sum = 0;
    for (double j = 0; j < J; j++) {
      double phi = st + (j + 0.5) / J * w; phi -= std::floor(phi);
      sc.inside = j > 0;
      sum += voiceOut(s_, phi, c, k, modX, sc) - wave(s_.base, phi);
    }
    return fac * w * sum / J;
  }
  double dcPair(Member& m, double c, double k, const SP& s_) {
    BX& bx = m.bx; const Blade& g = *bx.g; BX& b = bxs;
    const double kk1 = s_.w >= 0.004 ? (s_.lock == 1 ? k / js::max(s_.w, 1e-3) : k) : 0;
    const double kk2 = g.w >= 0.004 ? (g.lock == 1 ? bx.k / js::max(g.w, 1e-3) : bx.k) : 0;
    const double feats = kk1 * (isFM(s_) ? 1 + 0.5 * s_.I : 1) + kk2 * (isFM(g) ? 1 + 0.5 * g.I : 1) + 2 * (s_.mEff + g.mEff) + 4;
    const double J = js::min(2048, js::max(64, std::ceil(feats * 32)));
    dcJ = J;
    auto z = [](NS& dd, const NS& src) { dd.seed = src.seed; dd.acc = src.acc; dd.xin = src.xin; dd.cacc = 0; dd.ov = 0; dd.cd = 0; dd.idx = 0; dd.val = 0; dd.pv = 0; dd.inside = false; };
    z(sc, m.ns); z(sc2, m.ns2);
    b.g = bx.g; b.c = bx.c; b.k = bx.k; b.modX = bx.modX; b.mr = bx.mr; b.mn = bx.mn;
    z(b.ns3, bx.ns3); z(b.ns4, bx.ns4);
    double sum = 0;
    for (double j = 0; j < J; j++) {
      const double phi = (j + 0.5) / J;
      sum += out(s_, phi, c, k, m.modX, sc, sc2, &b) - wave(s_.base, phi);
    }
    return sum / J;
  }
  void scan(Member& m, double st, double p0, double dphi, double c, double k, const SP& s_, double dAcc,
            double modX0, const Blade& g, double kB, NS& nsB, double modX1) {
    const double w = g.w;
    NS& ns = nsB;
    double off = -1, step = 1;
    const bool carrier = g.mode == 0 || g.mode == 5 || isFM(g);
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
    if (isFM(g)) {
      if (g.fmType == 1) { o1 = ns.acc; o0 = o1 - dAcc; }
      else {
        const double scl = 0.15915494309189535 * g.I;
        o0 = scl * mod(g.mshape, g.mode == 1 ? g.mEff * r0 : modX0, ns.seed);
        o1 = scl * mod(g.mshape, g.mode == 1 ? g.mEff * r1 : modX1, ns.seed);
      }
    }
    if (js::truthy(ns.cd)) { o0 += ns.cacc - ns.cd; o1 += ns.cacc; }
    const double cp0 = kk * r0 + o0, cp1 = kk * r1 + o0 + (o1 - o0) * tmax;
    const double lo = js::min(cp0, cp1), hi = js::max(cp0, cp1);
    if (!(hi - lo > 1e-12 && hi - lo < 8)) return;
    double j = std::floor((lo - off) / step) + 1;
    for (int q = 0; q < 6; q++, j++) {
      const double pos = off + j * step; if (pos > hi) break;
      if (pos <= lo || pos == 0) continue;
      const double tau = (pos - cp0) / (cp1 - cp0) * tmax;
      if (tau > 0 && tau <= 1) { emit(2); addE(m, p0 + tau * dphi, tau, c, k, s_); }
    }
  }
  void addE(Member& m, double E, double tau, double c, double k, const SP& s_) {
    const double h = hAt(m, E, c, k, s_);
    cc -= 0.5 * h * tau * tau;
    cp += 0.5 * h * (1 - tau) * (1 - tau);
  }
  void tryE(Member& m, double E, double p0, double dphi, double c, double k, const SP& s_) {
    double dd = E - p0; dd -= std::floor(dd);
    if (dd > 0 && dd <= dphi) { emit(1); addE(m, E, dd / dphi, c, k, s_); }
  }
  void emit(uint32_t kind) { if (events) events->add(evTick, evId, kind); }
  double stepM(Member& m, double dphi, double c, double k, SP& s_) {
    const double w = s_.w, p0 = m.phi;
    NS& ns = m.ns;
    double p1 = p0 + dphi; p1 -= std::floor(p1); m.phi = p1;
    const double modX0 = m.modX;
    m.modX += s_.mEff * dphi; if (m.modX > 65536) m.modX -= 65536;
    double st = c - w * 0.5; st -= std::floor(st);
    double e0 = p0 - st; e0 -= std::floor(e0);
    double e1 = p1 - st; e1 -= std::floor(e1);
    const bool on = w >= 0.004;
    const double kk = on ? (s_.lock == 1 ? k / w : k) : k;
    if (on && e1 < e0) emit(3);
    const double dAcc = on ? fmStep(s_, ns, e1, e1 < e0, kk, m.modX, dphi) : 0;
    BX* bx = js::truthy(s_.b2on) ? &m.bx : nullptr;
    double dAcc2 = 0, st3 = 0, modX20 = 0;
    bool on2 = false;
    if (bx) {
      if (js::truthy(s_.b2fm)) { modX20 = bx->modX; bx->modX += bx->g->mEff * dphi; if (bx->modX > 65536) bx->modX -= 65536; }
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
    if (bx && on && on2 && (js::truthy(s_.colK) || js::truthy(s_.colB))) {
      const Blade& g = *bx->g;
      const double w2 = g.w;
      double a0 = p0 - st3; a0 -= std::floor(a0); double a1 = p1 - st3; a1 -= std::floor(a1);
      const double ov = gate(e1, w, s_.hard, s_.mode) * gate(a1, w2, g.hard, g.mode);
      if (js::truthy(s_.b2order)) { collide(s_.colK, ns, e0, e1, kk, ov, dphi); bx->ns3.ov = 0; bx->ns3.cacc = 0; bx->ns3.cd = 0; }
      else { collide(s_.colK, bx->ns3, a0, a1, g.lock == 1 ? bx->k / w2 : bx->k, ov, dphi); ns.ov = 0; ns.cacc = 0; ns.cd = 0; }
    } else if (js::truthy(ns.ov) || js::truthy(ns.cacc) || (bx && (js::truthy(bx->ns3.ov) || js::truthy(bx->ns3.cacc)))) {
      ns.ov = 0; ns.cacc = 0; ns.cd = 0; if (bx) { bx->ns3.ov = 0; bx->ns3.cacc = 0; bx->ns3.cd = 0; }
    }
    const double x = out(s_, p1, c, k, m.modX, ns, m.ns2, bx);
    cc = 0; cp = 0;
    if (js::truthy(s_.aa) && dphi > 0 && dphi < 0.5) {
      const double b = s_.base;
      if (b == 2 || b == 4) tryE(m, 0.5, p0, dphi, c, k, s_);
      else if (b == 3) { tryE(m, 0, p0, dphi, c, k, s_); tryE(m, 0.5, p0, dphi, c, k, s_); }
      if (on) {
        if (!H2_FAULT(3)) tryE(m, st, p0, dphi, c, k, s_);   // must-fail control 3 skips the blade-entry BLEP
        if (w < 0.9999) tryE(m, st + w, p0, dphi, c, k, s_);
        scan(m, st, p0, dphi, c, k, s_, dAcc, modX0, s_, k, ns, m.modX);
        if (s_.mirror >= 2) {
          const double st2 = st + 0.5;
          tryE(m, st2, p0, dphi, c, k, s_);
          if (w < 0.9999) tryE(m, st2 + w, p0, dphi, c, k, s_);
          scan(m, st2 - std::floor(st2), p0, dphi, c, k, s_, dAcc, modX0, s_, k, ns, m.modX);
        }
      }
      if (on2) {
        const Blade& g = *bx->g;
        const double w2 = g.w;
        tryE(m, st3, p0, dphi, c, k, s_);
        if (w2 < 0.9999) tryE(m, st3 + w2, p0, dphi, c, k, s_);
        scan(m, st3, p0, dphi, c, k, s_, dAcc2, modX20, g, bx->k, bx->ns3, bx->modX);
        if (g.mirror >= 2) {
          const double st4 = st3 + 0.5;
          tryE(m, st4, p0, dphi, c, k, s_);
          if (w2 < 0.9999) tryE(m, st4 + w2, p0, dphi, c, k, s_);
          scan(m, st4 - std::floor(st4), p0, dphi, c, k, s_, dAcc2, modX20, g, bx->k, bx->ns3, bx->modX);
        }
      }
    }
    const double o = m.prev + cp;
    m.prev = x + cc;
    return o;
  }

  // ---- state ----------------------------------------------------------------
  double sr;
  double age = 0;
  int cnt = 0, vc = 0;
  SP t, s;
  DP d;
  int sm = 0;
  Voice voices[kVoices];
  double Rx[8] = {}, Ry[8] = {};
  double gl[9] = {}, gr[9] = {};
  NS sc, sc2;
  BX bxs;
  int dcCnt = 0;
  double hx[2] = {}, hy[2] = {}, gRot = 0, gRot2 = 0;
  int stack[128] = {};
  int stackN = 0;
  double nf[128] = {};
  double cc = 0, cp = 0;
  double os = 0;
  Biquad bqL[2], bqR[2];
  double gmr = 0, gmn = 1;   // RazorCore.mr / RazorCore.mn (class statics in the oracle)
  double dcJ = 0;            // this._dcJ
  Mulberry32 rng;
  double cu[9] = {1, 5.0 / 4, 3.0 / 2};
  int cuLen = 3;
  double rl[10][10][9] = {};
  bool rlValid[10][10] = {};
  double rlTmp[9] = {};
  // event context (read only when `events` is set)
  uint32_t evTick = 0, evId = 0;
  uint64_t evSample = 0;
};

template <class T> void RazorCore::render(T* L, T* R, int n) {
  const double a = 1 - std::exp(-16 / (0.012 * sr));
  const double a1 = 1 - std::exp(-1 / (0.012 * sr));
  s.mode = d.mode; s.hot = d.hot; s.base = d.base; s.lock = d.lock; s.mshape = d.mshape; s.fmType = d.fmType; s.mirror = d.mirror; s.aa = d.aa; s.mEff = s.m;
  s.b2on = d.b2on; s.mode2 = d.mode2; s.hot2 = d.hot2; s.b2order = d.b2order;
  s.lock2 = d.lock2; s.mirror2 = d.mirror2; s.b2fm = d.b2fm; s.fmType2 = d.fmType2; s.mshape2 = d.mshape2; s.mUnit2 = d.mUnit2;
  s.kq = d.kq; s.law = d.law; s.kRule = d.kRule; s.b2sp = d.b2sp; s.kRule2 = d.kRule2;
  const int N = static_cast<int>(d.N);
  for (int i = 0; i < N; i++) {
    const double pn = N > 1 ? static_cast<double>(i) / (N - 1) - 0.5 : 0;
    const double pos = js::truthy(d.panOrder) ? pn * 2 : panSlot(N, i);
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
    if (--sm <= 0) {
      sm = 16;
      for (int q = 0; q < kTKeys; q++) { const TKey& kk = tKeys()[q]; if (!kk.perSample) s.*kk.p += (t.*kk.p - s.*kk.p) * a; }
    }
    s.k += (t.k - s.k) * a1; s.kHz += (t.kHz - s.kHz) * a1; s.w += (t.w - s.w) * a1; s.c += (t.c - s.c) * a1;
    s.depth += (t.depth - s.depth) * a1; s.I += (t.I - s.I) * a1; s.hard += (t.hard - s.hard) * a1;
    s.w2 += (t.w2 - s.w2) * a1; s.k2 += (t.k2 - s.k2) * a1; s.kHz2 += (t.kHz2 - s.kHz2) * a1; s.c2 += (t.c2 - s.c2) * a1;
    s.b2mix += (t.b2mix - s.b2mix) * a1;
    if (--cnt <= 0) { cnt = 32; for (Voice& v : voices) if (v.active) { couple(v); spread(v); } }
    const double gk = 1 - std::exp(-3 / js::max(1, s.glide * 0.001 * sr));
    const double beA = 1 / js::max(1, s.benvA * 0.001 * sr), beD = 1 - std::exp(-4 / js::max(1, s.benvD * 0.001 * sr));
    const double beA2 = 1 / js::max(1, s.benvA2 * 0.001 * sr), beD2 = 1 - std::exp(-4 / js::max(1, s.benvD2 * 0.001 * sr));
    for (Voice& v : voices) {
      if (!v.active) continue;
      if (v.freq != v.freqT) {
        const double lr = std::log(v.freqT / v.freq);
        v.freq = std::fabs(lr) < 1e-5 ? v.freqT : v.freq * std::exp(lr * gk);
      }
      v.gr = v.freq / v.fc;
      if (v.bst == 1) { v.be += beA; if (v.be >= 1) { v.be = 1; v.bst = 2; } }
      else if (v.bst == 2) { v.be -= v.be * beD; if (v.be < 1e-4) { v.be = 0; v.bst = 0; } }
      const double bE = v.be * v.bv;
      v.kE = js::truthy(s.benvK) ? js::pow(2, s.benvK * 4 * bE) : 1;
      v.wE = js::truthy(s.benvW) ? js::pow(2, s.benvW * 3 * bE) : 1;
      if (js::truthy(d.b2env)) {
        if (v.bst2 == 1) { v.be2 += beA2; if (v.be2 >= 1) { v.be2 = 1; v.bst2 = 2; } }
        else if (v.bst2 == 2) { v.be2 -= v.be2 * beD2; if (v.be2 < 1e-4) { v.be2 = 0; v.bst2 = 0; } }
        const double bE2 = v.be2 * v.bv2;
        v.kE2 = js::truthy(s.benvK2) ? js::pow(2, s.benvK2 * 4 * bE2) : 1;
        v.wE2 = js::truthy(s.benvW2) ? js::pow(2, s.benvW2 * 3 * bE2) : 1;
      } else { v.kE2 = v.kE; v.wE2 = v.wE; }
      if (v.stage == 1) { v.env += attInc; if (v.env >= 1) { v.env = 1; v.stage = 2; } }
      else if (v.stage == 2) v.env += (s.Sus - v.env) * dC;
      else if (v.stage == 4) { v.env -= v.env * rC; if (v.env < 1e-4) { v.env = 0; v.active = false; } }
    }
    double yl = 0, yr = 0;
    const bool dcTick = dcOn && --dcCnt <= 0;
    if (dcTick) dcCnt = 256;
    const double w0 = s.w, d0 = s.depth, I0 = s.I, rs = s.rotRate / sr;
    const bool rOn = std::fabs(t.rotRate) > 0.004, sOn = t.rotSpread > 0.004;
    const double hk = 1 - std::exp(-1 / (0.03 * sr));
    if (rOn) { gRot += rs; gRot -= std::floor(gRot); } else gRot = home(gRot, hk);
    const bool rotBaseNull = js::truthy(d.rotSync);
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
    for (int j = 0; j < os; j++) {   // int vs double compare, as JS's `j < os`
      evTick = static_cast<uint32_t>(static_cast<double>(evSample) * os + j);
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
          const double xin = xOn ? 0.5 * (s.xm * xb[(q + 1) % N] + s.fb * 0.5 * (mm.y1 + mm.y2)) : 0;
          mm.ns.xin = xin; mm.ns2.xin = xin; mm.bx.ns3.xin = xin; mm.bx.ns4.xin = xin;
          s.w = w0 < 0.004 ? w0 : js::min(1, w0 * mm.wMul * v.wE);
          s.depth = js::min(1, js::max(0, d0 + mm.dAdd));
          s.I = I0 * mm.iMul;
          gmr = mm.mr; gmn = mm.mn;
          const double rotAll = (rotBaseNull ? v.rot : gRot) + mm.rot, fr2 = d.frame2 < 0 ? d.frame : d.frame2;
          double c = s.c + rotAll + mm.cOff + (js::truthy(d.frame) ? mm.lead : 0); c -= std::floor(c);
          const double fi = js::max(1, mm.inc * v.gr);
          const double kCap = 0.45 * sr * os / fi;
          const double kq = js::min((d.lock == 2 ? js::max(0.05, s.kHz / fi + mm.kAdd) : js::max(0.25, s.k + mm.kAdd)) * mm.kMul * v.kE, kCap);
          if (js::truthy(s.b2on)) {
            BX& bx = mm.bx;
            const double rot2All = own2 ? (rotBaseNull ? v.rot2 : gRot2) + mm.rot2 : rotAll;
            bx.c = s.c2 + rot2All + mm.cOff2 + (js::truthy(fr2) ? mm.lead : 0); bx.c -= std::floor(bx.c);
            const double lock2 = d.lock2 < 0 ? d.lock : d.lock2;
            bx.k = js::min((lock2 == 2 ? js::max(0.05, s.kHz2 / fi + mm.kAdd2) : js::max(0.25, s.k2 + mm.kAdd2)) * mm.kMul2 * v.kE2, kCap);
            const double mEff2 = js::truthy(d.b2fm) ? (js::truthy(d.mUnit2) ? s.mHz2 / fi : s.m2) : (js::truthy(d.mUnit) ? s.mHz / fi : s.m);
            fillG2(*bx.g, s.w2 < 0.004 ? s.w2 : js::min(1, s.w2 * mm.wMul2 * v.wE2), js::min(1, js::max(0, s.depth2 + mm.dAdd2)),
                   (js::truthy(d.b2fm) ? s.I2 : I0) * mm.iMul2, mEff2);
            bx.mr = mm.mr2; bx.mn = mm.mn2;
          }
          s.mEff = js::truthy(d.mUnit) ? s.mHz / fi : s.m;
          evId = static_cast<uint32_t>(vi * kMembers + q);
          double y = stepM(mm, mm.inc * v.gr / (sr * os), c, kq, s);
          if (xOn) { mm.y2 = mm.y1; mm.y1 = y; }
          if (dcOn) {
            if (dcTick && j == 0 && (--mm.dcWait <= 0 || mm.dcInit)) {
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
      if (os > 1) {
        accL = bqf(bqL[1], bqf(bqL[0], accL));
        accR = bqf(bqR[1], bqf(bqR[0], accR));
      }
      yl = accL; yr = accR;
    }
    if (d.dcMode == 1 || (d.dcMode == 2 && (s.xm > 0.0005 || s.fb > 0.0005 || (js::truthy(s.b2on) && (js::truthy(s.colK) || js::truthy(s.colB)))))) {
      const double ol = yl - hx[0] + hpR * hy[0]; hx[0] = yl; hy[0] = ol; yl = ol;
      const double orr = yr - hx[1] + hpR * hy[1]; hx[1] = yr; hy[1] = orr; yr = orr;
    }
    L[i] = static_cast<T>(std::tanh(yl * s.gain * 1.6));
    R[i] = static_cast<T>(std::tanh(yr * s.gain * 1.6));
    if (--vc <= 0) vc = static_cast<int>(std::floor(sr / 30));   // viz cadence; nothing is posted here
  }
}

#undef H2_FAULT

}  // namespace horde2::scalpel
