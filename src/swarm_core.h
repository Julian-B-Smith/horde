/*
 * swarm_core.h — C++ port of the SAW reference core (SwarmSynth in
 * reference/swarmsaw.html, ADR-003). Correctness = L0-1 parity (ε = 1e-6 RMS vs the JS
 * renders), NOT plausible audio — so this is a statement-by-statement
 * transcription, and every deviation from the obvious C++ idiom below is
 * load-bearing:
 *
 * - All state is double (JS numbers are f64); output buffers are float with
 *   read-modify-write rounding per store, because the reference accumulates
 *   into a Float32Array (each += rounds to f32 — skipping that rounding is
 *   an audible-in-the-oracle divergence).
 * - The RNG reproduces mulberry32 under JS int semantics: |0 (ToInt32),
 *   Math.imul (low-32 multiply), >>> (uint32 shift). uint32_t arithmetic is
 *   bit-identical to all of these.
 * - Expression shapes match the reference (e.g. sigma floor before use,
 *   couple computed from the PREVIOUS tick's phases, per-swarm local `tick`
 *   counter with the shared counter advanced once per render call).
 * - Header-only, pure, allocation-free after construction, no wall-clock
 *   (SPEC §5.7); tools and the plugin both include it.
 *
 * Known parity caveat (measured, not assumed): V8's transcendentals are
 * fdlibm-based and may differ from libm by ULPs; chaotic regimes can amplify
 * that. The parity harness reports per-scenario RMS so any such divergence
 * is visible and attributable.
 */
#pragma once

#include <algorithm>  // std::stable_sort — the ADR-070 pan fan; stability is parity-relevant
#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>

#include "force_core.h"
#include "glide_core.h"

namespace hypersaw
{

constexpr int kMaxV = 32;
// B38 SIZING INSTRUMENT, not a feature: the voice-retire threshold as a
// compile-time override so the bench can measure cost-vs-threshold. The
// default is the shipped literal, so every normal build -- and therefore every
// golden -- is bit-identical; only a bench built with -DHZ_CULL_ENV=... moves.
// The eventual B38 knob replaces this with a parameter; a macro is the
// measurement's tool, never the product's.
#ifndef HZ_CULL_ENV
#define HZ_CULL_ENV 1e-4
#endif
constexpr int kPoly = 16;  // raised from 8 (2026-07-18: user hit the ceiling
                           // at ~6-7 held notes). 16 voices x 32 osc = trivial
                           // CPU; well inside the ADR-006 spike headroom.
constexpr int kTick = 16;
// ADR-086: the fixed grid gravity integrates on, in samples. Chosen by
// measurement, not taste — see the comment at its use site in render().
// ADR-086 Amendment 1: the grid is a fixed TIME, not a fixed sample count.
// 256 samples at 44.1 kHz was a duration that shrank as the rate rose (5.81 ms
// there, 2.67 ms at 96 k), so Euler truncation error — and therefore gravity's
// trajectory — tracked the sample rate: settle time drifted +0.42% at 96 k,
// monotonically. That is the same defect one level down, relocated rather than
// closed. Expressed in seconds it obeys ADR-009 like every other time constant.
// The value is exactly 256/44100 so `gravGridSamples()` returns exactly 256 at
// 44.1 kHz and every golden stays bit-identical.
/* SAW SHAPE (glass) banks — ported verbatim from reference/swarmsaw.html, ADR-094.
   Contents are the lab's PLACEHOLDER profiles (its own comment says the real
   ones are to be measured from synths); a fold moves code, it does not improve
   it, or parity stops meaning anything. */
inline double sawShapeAnchor(int k, double ph)
{
  // 0 glass (parabola arch) · 1 soft · 2 hollow (triangle) · 3 pure (sine) · 4 reedy (+3rd)
  const double s2 = 2 * ph - 1, tri = 1 - 4 * std::fabs(ph - 0.5), par = 0.6667 - 2 * s2 * s2;
  if (k <= 0) return par;
  if (k == 1) return 0.5 * par + 0.5 * tri;
  if (k == 2) return tri;
  if (k == 3) return std::sin(6.283185307 * ph);
  return 0.8 * std::sin(6.283185307 * ph) + 0.2 * std::sin(18.849555922 * ph);
}
inline double sawBaseAnchor(int k, double ph)
{
  const double r = 2 * ph - 1;
  const double sgn = r < 0 ? -1.0 : (r > 0 ? 1.0 : 0.0);   // Math.sign
  if (k <= 1) return sgn * std::pow(std::fabs(r), 1.4);    // 1 curved
  if (k == 2) return 0.85 * r + 0.15 * (0.6667 - 2 * r * r);  // 2 fat
  if (k == 3) return std::tanh(2.2 * r) / 0.97574;         // 3 driven
  return sgn * std::pow(std::fabs(r), 0.6);                // 4 aggressive
}

/* ADR-101: the anchors above are the REFERENCE definitions and stay the source
   of truth; the render loop reads these TABLES of them instead. Measured before
   this existed: sawBase 0.5 alone took a 16-voice patch from 3.71% to 8.07% of a
   core, all four stages to 12.38% — pow() per sample per voice, ~11M/s.
   16384 cells + linear interpolation keeps the divergence far under the parity
   bar: every anchor except the pow pair is smooth or piecewise-linear (error
   ~2e-8; the triangle's corner falls exactly on a grid point at this power-of-
   two size); the pow cusp at r=0 is the worst cell at ~5e-7 for ONE sample
   neighbourhood per cycle, which the saw-shape goldens measure at RMS — the
   verify run after this change is the recorded evidence. N+1 points, not N:
   ph lives in [0,1) but the last CELL still needs f(1-) as its right edge, or
   the wrap would interpolate +1 straight down to -1 across one cell.
   Static tables, built at load — never on the audio thread. */
constexpr int kAnchorN = 16384;
struct AnchorTables
{
  double base[5][kAnchorN + 1];
  double shape[5][kAnchorN + 1];
  AnchorTables()
  {
    for (int k = 0; k < 5; k++)
      for (int i = 0; i <= kAnchorN; i++)
      {
        const double ph = (double)i / kAnchorN;
        base[k][i] = sawBaseAnchor(k, ph);
        shape[k][i] = sawShapeAnchor(k, ph);
      }
  }
};
inline const AnchorTables &anchorTables() { static const AnchorTables t; return t; }
inline double sawBaseTab(int k, double ph)
{
  const double x = ph * kAnchorN;
  const int i = (int)x;
  const double f = x - i;
  const double *t = anchorTables().base[k];
  return t[i] * (1 - f) + t[i + 1] * f;
}
inline double sawShapeTab(int k, double ph)
{
  const double x = ph * kAnchorN;
  const int i = (int)x;
  const double f = x - i;
  const double *t = anchorTables().shape[k];
  return t[i] * (1 - f) + t[i + 1] * f;
}

constexpr double kGravGridSeconds = 256.0 / 44100.0;   // 5.805 ms
constexpr double kTau = 6.283185307;   // matches the reference's literal
constexpr double kPiRef = 3.14159265;  // ditto — NOT M_PI, parity over precision

struct Params
{
  double n = 7, dist = 1, seed = 1234, detune = 0.28, law = 0, K = 0, onset = 0,
         dissolve = 0.63, driftDepth = 0, driftRate = 0.4, inertia = 0, rtone = 0,
         normExp = 0.75, width = 0.8, mono = 0, digital = 1, vol = 0.4, retrig = 1;
  // ADSR (ADR-021): defaults reproduce the reference AR bit-exactly — at
  // sustainL >= 1 the render loop takes the reference's exact expressions,
  // and attackS/releaseS defaults are the reference's own constants. L0-1
  // goldens are the regression proof; change defaults only with an ADR.
  double attackS = 0.003, decayS = 0.16, sustainL = 1.0, releaseS = 0.16;
  // Tempo-grid law (law == 3; ADR-005/ADR-022): ported expression-for-
  // expression from the DYNAMICS reference (reference/swarmdynamics.html beatQ path).
  // bpm is host-owned (CLAP transport), beatMult is cycles-per-beat.
  double bpm = 120, beatMult = 1;
  // Dynamics layer (Phase 3, ADR-023): topology / Sakaguchi lag / Daido
  // poles / consonance gravity, ported expression-for-expression from the
  // DYNAMICS reference. Defaults reproduce the SAW reference bit-exactly
  // (topo 0, alpha 0, poles 1, grav 0 leave every SAW expression untouched);
  // the DYN golden set proves the non-default paths. lpOut=0 bypasses the
  // R->tone output pole — the one structural difference between the two
  // references (DynSynth has no output filter).
  double topo = 0, reach = 5, mu = 0.6, alpha = 0, poles = 1, grav = 0, basin = 35, lpOut = 1;
    // ADR-094 saw shape (glass), folded from the detune lab. BOTH default to 0
    // and each stage is guarded, so an untouched patch is bit-identical.
    double sawBase = 0, sawProfile = 0, round = 0, roundHi = 0;
    // B38: retire threshold in dB. -80 is the shipped constant expressed in the
    // unit the player hears the trade in; see the guard at the use site for why
    // the default must convert back EXACTLY.
    double voiceCull = -80;
  // Two-cluster A/B balance (ADR-051): 0 = both clusters full K (bit-inert
  // default); >0 scales cluster B's intra-coupling by kB = 1-2*balance, so one
  // knob sweeps symmetric → B-uncoupled (0.5) → B-splayed (1.0, kB=-1).
  double balance = 0;
  // Voice-mode support (ADR-026): glide time in seconds for retargetNote().
  // Pure superset — the poly/default path never sets a glide target, so all
  // reference expressions stay bit-untouched.
  double glide = 0;
  // ADR-096 note travel: the five-law GlideCore replaces the hard-wired one-pole
  // this site used to run inline. `tau` is NOT read from here -- it is overridden
  // from `glide` above at the use site, because id 33 stays the single source of
  // the note lag and already holds SECONDS in shipped presets.
  hypersaw::GlideCore::Params noteLaw = [] {
    hypersaw::GlideCore::Params q;
    q.model = hypersaw::GlideCore::kLag;   // = what id 33 has always done
    // tau tracks `glide`, which defaults 0 — GlideCore's own 60 ms default
    // would make a BARE core glide out of the box. Invisible while ADR-076's
    // polyGlide flag gated the arming; the moment ADR-102 removed that flag,
    // parity went red (153/156, worst 8.4e-02 at dyn-gravity): the goldens
    // strike sequential notes and every one of them started gliding. The
    // shell's twin lambda fixed this same default on 2026-08-19; the core's
    // did not, and nothing could tell until the flag stopped hiding it.
    q.tau = 0;
    return q;
  }();
  // Live tune factor (ADR-027): one multiplicative pitch for octave/semi/
  // cents/bend, applied to the fundamental at law evaluation so it bends
  // SOUNDING notes. Default 1.0 is bit-inert (x * 1.0 == x in IEEE754).
  double tune = 1.0;
  // Absolute-K mode (ADR-004): bypass sigma-normalization (strict-chimera /
  // identical-oscillator experiments; L0-10 note). Guarded — default 0 is
  // bit-inert.
  double absK = 0;
  // Phase scatter (ADR-033): partial-random phase init. 0 = follow the
  // legacy retrig toggle EXACTLY (bit-inert, rng untouched); > 0 overrides:
  // phase_i = rng * scatter (so 1.0 reproduces the retrig-off stream).
  double scatter = 0;
  // Pan scatter (ADR-035): blend pan positions toward a seeded permutation.
  // Legacy pan order is monotone in the detune offset (pan = x[i]*width), so
  // spatial order == frequency order and sweeps march across the field in
  // series — the 2026-07-18 report. 0 = bit-inert legacy; the permutation
  // draws from its OWN stream, never the phase/drift stream.
  double panScatter = 0;
  // Waveshape morph (ADR-058): 0 = saw (bit-inert), 1 = band-limited square.
  double shape = 0;
  // Tone tilt (ADR-060, folded from reference/swarmsaw.html): bipolar per-voice one-pole.
  // 0 = inert; >0 darkens (LP), <0 thins (HP). cutoff rises as sqrt(f/f0).
  double tilt = 0;
  // Hi-tame (ADR-061): equal-loudness per-voice roll-off, gain (f0/f)^hiTame.
  // 0 = inert; >0 turns the higher voices down so a tall stack isn't harsh.
  double hiTame = 0;
  // ADR-062: drift mode (0 walk / 1 sine / 2 sample&hold; 0 = original walk) +
  // keep-phase (1 = note-on continues from last phases; 0 = retrig/random).
  double driftMode = 0, keepPhase = 0;
  // ADR-063: opt-in frequency glide, a time constant in SECONDS (ADR-009 —
  // seconds in, per-tick/per-sample coefficients out). 0 = off, bit-identical.
  double freqGlide = 0;
  // ADR-064: pan motion (depth + mode 0 drift / 1 sweep) and the centre pin,
  // which scales drift + pan motion by each voice's distance from the fundamental.
  double panMotion = 0, panMode = 0, motionCenter = 0;
  // ADR-065 harmonic law (law 4): harmReach sets the top rung — at detune 1 voice
  // i lands on f0*(1 + harmReach*i); 1 = the natural series 1,2,3,...,n.
  // NAMED harmReach, not reach: `reach` is already the RING-topology radius (p.reach).
  double harmReach = 1;
  // ADR-066 stretch law (law 5): the cents offset is itself stretched by
  // (1 + stretchB*x^2), so outer voices spread disproportionately — piano/bell
  // inharmonicity. 0 is algebraically law 0, so this is the whole law.
  double stretchB = 0;
  // ADR-068 octave spread + root anchor: spread multiplies detune (1..24 takes
  // law 0 from +/-1 st to +/-2 oct); anchor shifts every x by -anchor*xmin so at
  // 1 the lowest voice sits on the root. Defaults bit-inert (x*1, x-0).
  double spread = 1, anchor = 0;
  // ADR-069 sync pivot: 0 = mean-field (reference), 1 = root-pinned pacemaker —
  // every voice entrains to the voice nearest f0; pitch-stable collapse.
  double pivotMode = 0;
  // ADR-070 pan image: layout 0 = alternating pitch-ranked fan (the NEW default
  // — root centred, ensemble widens as it climbs), 1 = legacy x-proportional.
  double panLayout = 0, panCurve = 0.5, panInvert = 0;
  // ADR-074 super-width mode (width > 1 only; inert at width <= 1):
  // 0 = wide (F: seat steepening + per-voice ITD, clean — the default),
  // 1 = pulse (A: the original ADR-025 M/S boost, polarity cross-feed),
  // 2 = smear (D: allpass side, frequency-dependent inversion).
  double superMode = 0;
  // ADR-075 oscillator oversampling: 0 = off (bit-exact, the reference path),
  // 1 = 2x. Opt-in like ADR-063's freqGlide, so every golden stays green.
  double oversample = 0;
  // ADR-076 poly glide: 0 = off (bit-exact), 1 = every new voice glides in
  // from the last-played pitch. Reuses the mono retarget's glide machinery
  // (ADR-026) and the existing `glide` TIME knob; core+shell only, so the JS
  // reference — which has no glide at all — is untouched.
  double polyGlide = 1   /* matches the shell default; DSP no longer reads it (ADR-102) */;
  // ADR-076 glide source: 0 = VOICE-based (bend only while another key is
  // still HELD — the legato reading, and the same rule the mono path has used
  // since 2026-07-18), 1 = MEMORY-based (always bend from the last-played
  // pitch, even after a rest). Human, 2026-08-03: memory-based is wanted but
  // not always, so it is a mode rather than the behaviour.
  double glideMode = 0;
  // ADR-077 ensemble onset timing (Vorberg/Wing linear phase correction, the
  // 2026-07-28 research headline, LIBRARY L0019). Voices enter at slightly
  // DIFFERENT times, and those times are produced by mutual error correction
  // rather than independent jitter — which is what carries the serial
  // structure listeners actually judge ensembles by.
  //   off_i <- off_i - alpha*(off_i - mean_off) + motorNoise_i
  // onsetScatter is the motor-noise sigma in MILLISECONDS and is the master
  // switch: 0 = off = bit-exact. alpha 0 random-walks apart, ~0.25 is the
  // measured near-optimal for real quartets, 1 collapses to i.i.d. jitter
  // (which is exactly what conventional humanize does, and is audibly worse).
  double onsetScatter = 0, onsetAlpha = 0.25, attackScatter = 0;
  // ADR-078 per-voice envelopes (increment 2). 0 = one shared envelope for the
  // swarm (the reference path, bit-exact); 1 = every voice runs its own ADSR,
  // so entries AND releases can differ. relScatter spreads the release times
  // the way attackScatter spreads the attacks — a chord that decays as players
  // rather than as one gate. Per-voice state is also what a mod matrix wants
  // as a SOURCE later (human, 2026-08-03: "voice-state-based modulation").
  double voiceEnv = 0, relScatter = 0;
};

// Consonance gravity ratio set (SPEC Layer 3, ADR-008) — the DYNAMICS
// reference's exact values; octave-folded snap targets.
constexpr double kRatios[13] = {1.0,       16.0 / 15, 9.0 / 8, 6.0 / 5, 5.0 / 4,
                                4.0 / 3,   7.0 / 5,   3.0 / 2, 8.0 / 5, 5.0 / 3,
                                16.0 / 9,  15.0 / 8,  2.0};
// Math.PI (full precision) — the DYNAMICS reference uses it for the alpha
// degrees->radians conversion, unlike the truncated kPiRef literals.
constexpr double kPiFull = 3.141592653589793;

class SwarmCore
{
 public:
  /* B81/B82 — THE PER-VOICE SEAM (increment 1, ADR-148). A tap, when
     installed, receives each NOTE's contribution to the block as DOUBLE
     buffers before it reaches the shared bus; the core then re-adds the
     (possibly filtered) buffers into the bus with the exact float-store
     arithmetic of the direct path, IN THE SAME NOTE ORDER — so a tap whose
     function leaves the buffers untouched is bit-identical to no tap at all,
     and tap == nullptr IS the original code path, pointer-for-pointer.

     DOUBLE scratch is load-bearing: the direct path computes
     (float)((double)out + lp*g) — capturing lp*g pre-rounded to float and
     adding THAT would change the double addition and break the identity.

     The width/tanh tail runs after all notes in both modes, untouched: the
     filter sits per-note, the bus character stays the bus's.

     Raw function pointer + ctx, not std::function: this runs on the audio
     thread and must never allocate or type-erase. */
  typedef void (*NoteTapFn)(void *ctx, int slot, int midi, double *l, double *r, int frames);
  void setNoteTap(NoteTapFn fn, void *ctx) { tapFn = fn; tapCtx = ctx; }
  static constexpr int kTapMax = 2048;   // > gravity grid at any real rate (1114 @192k)

 private:
  NoteTapFn tapFn = nullptr;
  void *tapCtx = nullptr;
  double tapScratchL[kTapMax], tapScratchR[kTapMax];

 public:
  struct Voice
  {
    double phase[kMaxV], driftS[kMaxV], couple[kMaxV], vf[kMaxV], eff[kMaxV], mom[kMaxV];
    double f0 = 220, f0cur = 220;
    int midi = -1, gate = 0;
    double vel = 1.0, press = 1.0, pressSm = 1.0;   // ADR-084 (1.0 = inert)
    double env = 0, Kenv = 0, KsmS = 0, KsmP = 0;
    double R = 0, RN = 0, psi = 0, sigma = 0, RA = 0, RB = 0, RQ = 0;
    long age = -1;
    uint32_t rngState = 1;
    int fresh = 1;
    int inAttack = 1;  // ADSR phase flag; unread on the reference-exact path
    int glideActive = 0;
    double glideTarget = 0;
    hypersaw::GlideCore travel{2756.0, /*bendLane=*/false};
    double lpL = 0, lpR = 0, lpc = 1;
    double vlp[kMaxV] = {0}, vlpc[kMaxV];  // per-voice tone-tilt state (vlpc init 1 in ctor)
    double hg[kMaxV];                      // per-voice hi-tame gain (init 1 in ctor)
    double rnd[kMaxV];                     // ADR-094 per-voice roundness (0 = untouched)
    double driftPh[kMaxV] = {0}, driftHoldT[kMaxV] = {0};  // sine / S&H drift-mode state
    double vfSm[kMaxV] = {0}, fRun[kMaxV] = {0};           // ADR-063 frequency-glide state
    float itdRing[kMaxV][256] = {};                        // ADR-074 far-channel rings
    int itdW = 0;
    double osZL[64] = {0}, osZR[64] = {0};                 // ADR-075 decimator history
    int osW = 0;
    double onsD[kMaxV] = {0};      // ADR-077 samples until this voice enters
    // The INITIAL onset delay, kept because onsD decrements to zero and the
    // envelope visualiser needs what the voice was actually given. Viz-only
    // bookkeeping: never read in the audio path, so parity is untouched.
    double onsD0[kMaxV] = {0};
    double onsE[kMaxV] = {0};      // its own 0->1 entry ramp (also ADR-078 env)
    double onsC[kMaxV] = {0};      // that ramp's per-sample attack coefficient
    double relC[kMaxV] = {0};      // ADR-078 per-voice release coefficient
    int vAtk[kMaxV] = {0};         // ADR-078 per-voice attack-stage flag
    int vfInit = 0;                                        // 0 → snap the glide on the next tick
    double cdist[kMaxV] = {0};                             // ADR-064 distance from the fundamental
    // Per-note expression tuning factor (ADR-036, MPE): 1.0 is bit-inert
    // (x * 1.0 == x in IEEE), same guarantee ADR-027 leans on for p.tune.
    double noteTune = 1.0;
  };

  explicit SwarmCore(double sampleRate) : sr(sampleRate)
  {
    // One traveller per VOICE (poly glide, ADR-076, gives each its own flight).
    // GlideCore fixes its control rate at construction, so re-rate them all here
    // rather than reaching into cr: the member initialiser is only right at 44.1k.
    for (auto &v : voices) v.travel = hypersaw::GlideCore(sampleRate / kTick, /*bendLane=*/false);
    {   // ADR-075 halfband: windowed sinc (Blackman), unity DC gain
      const int T = kHbTaps;
      const double m = (T - 1) * 0.5, cut = 0.235;
      double g = 0;
      for (int i = 0; i < T; i++)
      {
        const double k = i - m;
        const double sinc = (k == 0) ? 2 * cut : std::sin(2 * kPiRef * cut * k) / (kPiRef * k);
        const double win = 0.42 - 0.5 * std::cos(2 * kPiRef * i / (T - 1))
                                + 0.08 * std::cos(4 * kPiRef * i / (T - 1));
        hb[i] = sinc * win;
        g += hb[i];
      }
      for (int i = 0; i < T; i++) hb[i] /= g;
    }
    for (auto &s : voices)
    {
      std::memset(s.phase, 0, sizeof(s.phase));
      std::memset(s.driftS, 0, sizeof(s.driftS));
      std::memset(s.couple, 0, sizeof(s.couple));
      std::memset(s.vf, 0, sizeof(s.vf));
      std::memset(s.eff, 0, sizeof(s.eff));
      std::memset(s.mom, 0, sizeof(s.mom));
      std::memset(s.vlp, 0, sizeof(s.vlp));
      for (int i = 0; i < kMaxV; i++) { s.vlpc[i] = 1.0; s.hg[i] = 1.0; s.rnd[i] = 0.0; }  // tilt/hi-tame passthrough
    }
    rebuild();
  }

  // Read back any param by key — same map as setParam, so the two can never
  // drift apart (the 2026-07-18 readParam bug class).
  double getParam(const std::string &k) const
  {
    const double *slot = const_cast<SwarmCore *>(this)->paramSlot(k);
    return slot ? *slot : 0.0;
  }

  // Mirrors the JS setParam(k, v) including its rebuild triggers.
  bool setParam(const std::string &k, double v)
  {
    double *slot = paramSlot(k);
    if (!slot) return false;
    *slot = v;
    // `glide` IS the lag law's time constant at the core level — that is what
    // ADR-026 defined it as, and trajectory_check drives SwarmCore directly with
    // it. The shell resolves the link and calls setNoteLaw() AFTERWARDS, so a
    // FOLLOWing lane still ends up with bendTau; this only keeps the core usable
    // on its own terms. (Caught by ADR-026 non-legato going red when the core
    // stopped reading it: 220 -> 274.8 Hz, an 8x-fast glide.)
    if (k == "glide") p.noteLaw.tau = v * 1000.0;   // seconds -> ms
    if (k == "n" || k == "dist" || k == "seed" || k == "width" || k == "topo" ||
        k == "panScatter" || k == "law" || k == "panLayout" || k == "panCurve" ||
        k == "panInvert" || k == "superMode" || k == "oversample")
      rebuild();  // law/pan* added by ADR-070 (fan ranks by pitch); idempotent
    return true;
  }

  // Returns the swarm slot index so the shell can track host note identity
  // (CLAP NOTE_END bookkeeping). DSP behavior unchanged — parity-neutral.
  int noteOn(int midi, double f)
  {
    // Checked BEFORE alloc: alloc() can steal a voice that is still gated, and
    // asking afterwards would misreport whether anything was actually held.
    bool anotherHeld = false;
    for (const auto &sw : voices)
      if (sw.gate) { anotherHeld = true; break; }
    Voice &s = alloc();
    const int slot = (int)(&s - &voices[0]);
    initVoice(s, midi, f);
    // ADR-076 poly glide: start at the last-played pitch and glide to this
    // one. Deliberately NOT gated on whether that note is still sounding —
    // "remembers the position of the last played note(s) and always begins
    // with a bend" (human, 2026-07-31). lastNoteF persists across silence, so
    // the first note after a rest still bends in from wherever you last were.
    // ADR-103, three sources: 0 = another key held; 1 = held OR the previous
    // note still ringing (a release tail is "the last note" while you can hear
    // it; silence resets the phrase); 2 = always, silence included. Ringing is
    // measured the same way the render decides audibility (env > 1e-3), so the
    // glide source dies at exactly the moment the ear loses the note.
    bool anyRinging = anotherHeld;
    if (!anyRinging && p.glideMode > 0.5 && p.glideMode < 1.5)
      for (const auto &v : voices)
        if (v.env > 1e-3) { anyRinging = true; break; }
    const bool glideSourceOk =
        p.glideMode >= 1.5 ? true : (p.glideMode > 0.5 ? anyRinging : anotherHeld);
    // ADR-102 (human 2026-08-21): poly glide is no longer a toggle — it is ON
    // whenever a travel law is engaged. "Hide it and set it so it's on as long
    // as mono isn't toggled and a bend law is set"; mono never reaches this
    // path (the shell routes mono through retargetNote), so the law is the only
    // question left. p.polyGlide stays declared for state compat and is no
    // longer read here.
    if (noteTravels() && glideSourceOk && lastNoteF > 0 && lastNoteF != f)
    {
      s.f0 = lastNoteF;
      s.f0cur = lastNoteF;
      s.glideTarget = f;
      s.glideActive = 1;
      s.travel.reset(std::log2(s.f0) * 12.0);
    }
    lastNoteF = f;
    return slot;
  }

  // Mono/legato retarget (ADR-026): reuse a sounding voice for a new note.
  // legatoKeepPhase: keep phases/envelope running (classic legato); false
  // re-strikes the voice in place (phases/rng/attack) but still glides.
  // With p.glide <= 0 the pitch change is immediate.
  void retargetNote(int slot, int midi, double f, bool legatoKeepPhase)
  {
    Voice &s = voices[slot];
    if (!legatoKeepPhase)
    {
      const double keepF0 = s.f0, keepF0cur = s.f0cur;
      initVoice(s, midi, f);
      if (noteTravels())
      {
        // initVoice has already moved f0 TO the target, so without this restore
        // the glide has zero distance to cover and the re-strike lands dead on
        // pitch. This asked `p.glide > 0` until 2026-08-20, which was right when
        // lag was the only law and wrong for every law that carries its own
        // time: "mono on and legato off doesn't seem to apply glide on the
        // retrigger" (human). Measured: const-rate re-strike read 441 flat.
        s.f0 = keepF0;      // strike from the CURRENT pitch, glide to the new
        s.f0cur = keepF0cur;
      }
    }
    else
    {
      s.midi = midi;
      s.gate = 1;
    }
    if (noteTravels())
    {
      s.glideTarget = f;
      s.glideActive = 1;
      s.travel.reset(std::log2(s.f0) * 12.0);
    }
    else
    {
      const double ratio = f / s.f0;
      s.f0 = f;
      s.f0cur *= ratio;  // preserve gravity offsets multiplicatively
      s.glideActive = 0;
    }
    lastNoteF = f;   // ADR-076: mono retargets update the memory too
  }

  void initVoice(Voice &s, int midi, double f)
  {
    s.midi = midi;
    s.f0 = f;
    s.f0cur = f;
    s.vel = 1.0;      // set by the shell AFTER noteOn (keeps the reference signature)
    s.press = 1.0;    // pressure re-arms per note; hosts that never send it stay inert
    s.pressSm = 1.0;
    s.gate = 1;
    s.age = noteCounter++;
    // ADR-056: signed quadratic → bipolar onset lock. onset >= 0 reproduces
    // the reference's 8*onset^2 bit-exactly (fabs(onset)==onset there); onset < 0
    // makes Kenv negative — an initial SPLAY burst instead of a sync burst.
    s.Kenv = 8 * p.onset * std::fabs(p.onset);
    s.KsmS = 0;
    s.KsmP = 0;
    s.fresh = 1;
    s.inAttack = 1;
    s.lpL = 0;
    s.lpR = 0;
    std::memset(s.vlp, 0, sizeof(s.vlp));  // tone-tilt one-pole state
    // ADR-077/078 per-voice onset state. The COEFFICIENTS are needed whenever
    // either feature is on — an earlier version computed them only inside the
    // onsetScatter branch, so voiceEnv with scatter 0 left every attack
    // coefficient at 0 and the swarm rendered SILENCE. The timing correction
    // itself still only runs when scatter is on.
    const bool perVoice = (p.onsetScatter > 0) || (p.voiceEnv > 0.5);
    if (perVoice)
    {
      const int n = (int)p.n;
      for (int i = 0; i < n; i++)
      {
        s.onsD[i] = 0;
        s.onsE[i] = 0;
        s.vAtk[i] = 1;
        const double jit = 1 + gaussT() * p.attackScatter * 0.6;
        const double atk = std::max(0.002, p.attackS * std::max(0.15, jit));
        s.onsC[i] = 1 - std::exp(-1 / (atk * sr));
        const double rjit = 1 + gaussT() * p.relScatter * 0.6;
        const double rel = std::max(0.002, p.releaseS * std::max(0.15, rjit));
        s.relC[i] = 1 - std::exp(-1 / (rel * sr));
      }
      if (p.onsetScatter > 0)
      {
        // Vorberg/Wing: correct toward the ensemble mean, then add motor noise
        double mean = 0;
        for (int i = 0; i < n; i++) mean += tOff[i];
        mean /= (n > 0 ? n : 1);
        const double sig = p.onsetScatter * 0.001;
        for (int i = 0; i < n; i++)
          tOff[i] += -p.onsetAlpha * (tOff[i] - mean) + gaussT() * sig;
        double m2 = 0;
        for (int i = 0; i < n; i++) m2 += tOff[i];
        m2 /= (n > 0 ? n : 1);
        for (int i = 0; i < n; i++) s.onsD[i] = (tOff[i] - m2) * sr;
        // shift so the EARLIEST voice starts at 0 — the note must not feel
        // late overall, only internally spread
        double lo = s.onsD[0];
        for (int i = 1; i < n; i++) lo = std::min(lo, s.onsD[i]);
        for (int i = 0; i < n; i++) s.onsD[i] -= lo;
      }
      for (int i = 0; i < n; i++) s.onsD0[i] = s.onsD[i];
    }
    s.vfInit = 0;                          // ADR-063: snap the glide to the new note
    s.noteTune = 1.0;  // per-note expression resets with a fresh strike;
                       // legato retargets keep the incoming bend (MPE streams
                       // continue across mono retargets)
    for (int i = 0; i < kMaxV; i++) s.mom[i] = 0;
    // (seed|0) + age*7919 + 1 under ToInt32 — uint32 wrap is bit-identical
    s.rngState = (uint32_t)((int64_t)toInt32(p.seed) + (int64_t)s.age * 7919 + 1);
    for (int i = 0; i < kMaxV; i++)
    {
      s.driftS[i] = 0; s.driftPh[i] = i * 0.13; s.driftHoldT[i] = 0;
      if (p.keepPhase != 0)
        s.phase[i] = lastPhase[i];  // ADR-062 keep-phase: continue from last (matches swarmsaw precedence)
      else if (p.scatter > 0)
        s.phase[i] = rngNext(s.rngState) * p.scatter;  // ADR-033 partial scatter
      else
        s.phase[i] = (p.retrig != 0) ? 0.0 : rngNext(s.rngState);
    }
    s.glideActive = 0;
  }

  // MPE per-note tuning (ADR-036): relative semitones from the host's
  // note-expression stream; 0 restores the exactly-inert 1.0 factor.
  // ADR-096: the note lane's law arrives as a whole struct rather than through
  // setParam's key map, for the same reason the bend law does in the shell — it
  // is one law with nine coupled fields, and nine string keys could be set into
  // an inconsistent half-state between them.
  void setNoteLaw(const hypersaw::GlideCore::Params &e) { p.noteLaw = e; }

  // Does the note lane actually travel? Lag keeps its historical gate exactly --
  // lag with tau 0 IS instant, and every patch that stored glide=0 must stay
  // snapping. The other laws carry their own time and arm on selection alone.
  bool noteTravels() const
  {
    const int m = (int)p.noteLaw.model;
    if (m == hypersaw::GlideCore::kOff) return false;
    if (m == hypersaw::GlideCore::kLag) return p.noteLaw.tau > 0;
    return true;
  }

  void setNoteExpr(int slot, double semis)
  {
    if (slot < 0 || slot >= kPoly) return;
    voices[slot].noteTune = semis == 0.0 ? 1.0 : std::pow(2.0, semis / 12.0);
  }

  // ADR-084 velocity + MPE pressure -> per-voice gain (superset; both default
  // 1.0, and x1.0 is bit-identical, so every golden and pre-existing caller is
  // untouched). Pressure is a TARGET smoothed per control tick (ADR-009: the
  // constant is seconds, ~20 ms) — expression streams arrive at UI rate and a
  // raw multiply would zipper.
  void setNoteVelocity(int slot, double v)
  {
    if (slot >= 0 && slot < kPoly) voices[slot].vel = std::max(0.0, std::min(1.0, v));
  }
  void setNotePressure(int slot, double v)
  {
    if (slot >= 0 && slot < kPoly) voices[slot].press = std::max(0.0, std::min(1.0, v));
  }

  void noteOff(int midi)
  {
    for (auto &s : voices)
      if (s.gate && s.midi == midi) s.gate = 0;
  }

  void allOff()
  {
    for (auto &s : voices) s.gate = 0;
  }

  // ADR-100: hard kill — gate AND envelope to zero, so nothing rings. allOff()
  // is a release (tails decay musically); this is the oscillator being switched
  // OFF, where a 5-second tail continuing would contradict the switch.
  void killAll()
  {
    for (auto &s : voices) { s.gate = 0; s.env = 0; }
  }

  // Read-only accessors for the shell (viz feed; NOTE_END bookkeeping).
  // Not used by the DSP path; parity-neutral.
  int centerIndex() const { return centerIdx; }
  const Voice &voiceAt(int i) const { return voices[i]; }
  double panBaseAt(int i) const { return panBase[i]; }  // viz feed (voice map)
  // Effective pan for the viz: the motion-modulated seat when pan motion is
  // live, the base seat otherwise (the motion block only writes panEffV while
  // panMotion > 0.001, so panEffV would go stale the moment motion stops).
  double panEffAt(int i) const { return p.panMotion > 0.001 ? panEffV[i] : panBase[i]; }

  const Voice *focus() const
  {
    const Voice *best = nullptr;
    for (const auto &s : voices)
      if ((s.gate || s.env > 1e-3) && (!best || s.age > best->age)) best = &s;
    return best;
  }

  // Consonance gravity (ADR-008; DYN reference exact): once per render call,
  // pull each gated pair's f0cur toward the nearest folded ratio inside the
  // basin. grav < 0.005 returns untouched — the SAW-parity path.
  int gravAccum = 0;   // ADR-086: samples owed to the gravity grid
  // Rounded, never truncated: at 44.1 kHz this must land on exactly 256 or the
  // goldens move for no reason. lround(44100 * 256/44100) == 256 exactly.
  int gravGridSamples() const
  {
    const int g = (int)std::lround(sr * kGravGridSeconds);
    return g < 1 ? 1 : g;
  }

  void gravityStep(double dtB)
  {
    gravCount = 0;
    const double g = p.grav;
    if (g < 0.005) return;
    Voice *act[kPoly];
    int na = 0;
    for (auto &s : voices)
      if (s.gate) act[na++] = &s;
    if (na < 2) return;
    // insertion sort ascending by f0cur (stable; matches JS sort for the
    // distinct-frequency case)
    for (int i = 1; i < na; i++)
      for (int j = i; j > 0 && act[j]->f0cur < act[j - 1]->f0cur; j--) std::swap(act[j], act[j - 1]);
    const double rate = g * 3;
    for (int a = 0; a < na - 1; a++)
      for (int b = a + 1; b < na; b++)
      {
        Voice *lo = act[a], *hi = act[b];
        const double r = hi->f0cur / lo->f0cur;
        const double oct = std::floor(std::log2(r));
        const double rf = r / std::pow(2, oct);
        int bi = 0;
        double be = 1e9;
        for (int i = 0; i < 13; i++)
        {
          const double e = std::fabs(1200 * std::log2(rf / kRatios[i]));
          if (e < be)
          {
            be = e;
            bi = i;
          }
        }
        const double err = 1200 * std::log2(rf / kRatios[bi]);
        if (std::fabs(err) > p.basin) continue;
        const double move = err * rate * dtB * 0.5;
        hi->f0cur *= std::pow(2, -move / 1200);
        lo->f0cur *= std::pow(2, move / 1200);
        if (gravCount < 32)
        {
          gravPairs[gravCount][0] = bi;
          gravPairs[gravCount][1] = (int)oct;
          gravErr[gravCount] = err;
          gravCount++;
        }
      }
  }

  /* ADR-086. The public entry SEGMENTS the render so gravity steps on a fixed
     grid *interleaved with the audio*, whatever block size the caller uses.

     The first attempt only fixed the step SIZE — accumulate samples, then run
     `while (accum >= grid) gravityStep(grid/sr)` at the top of render. That is
     not enough and the subdivision probe said so immediately: with one whole
     call every step fires BEFORE any audio is written, while chunked calls
     spread the same steps through the buffer. Same steps, different placement,
     same 1.03 divergence. Gravity has to advance *between* segments of audio,
     which means the loop belongs here and not at the top. */
  void render(float *outL, float *outR, int frames)
  {
    advancePanMotion(frames);
    int done = 0;
    while (done < frames)
    {
      const int grid = gravGridSamples();
      const int room = grid - gravAccum;
      const int seg = (frames - done) < room ? (frames - done) : room;
      renderSeg(outL + done, outR + done, seg);
      gravAccum += seg;
      done += seg;
      if (gravAccum >= grid)
      {
        gravityStep((double)grid / sr);
        gravAccum = 0;
      }
    }
  }

private:
  bool panMotionOn = false;   // ADR-086: set by advancePanMotion(), read by renderSeg

  // Per-CALL, deliberately — see the note in renderSeg.
  void advancePanMotion(int frames)
  {
    const int n = (int)p.n;
    const double pmv = p.panMotion;
    if (pmv > 0.001)
    {
      const double dtB = (double)frames / sr;
      const int pmode = (int)p.panMode;
      const Voice *cdFoc = focus();
      if (pmode == 1) { panSweepPh += 0.1 * dtB; panSweepPh -= std::floor(panSweepPh); }
      const double sweep = pmv * std::sin(kTau * panSweepPh);
      for (int i = 0; i < n; i++)
      {
        double off;
        if (pmode == 1) off = sweep;
        else { panPh[i] += (0.08 + i * 0.021) * dtB; panPh[i] -= std::floor(panPh[i]); off = pmv * std::sin(kTau * panPh[i]); }
        if (p.motionCenter > 0 && cdFoc) off *= 1 - p.motionCenter * (1 - cdFoc->cdist[i]);
        const double pv = std::max(-1.0, std::min(1.0, panBase[i] + off));
        panEffV[i] = pv;  // viz feed only (voice map animates pan motion)
        const double th = (pv + 1) * 0.25 * kPiRef;
        panLm[i] = std::cos(th); panRm[i] = std::sin(th);
      }
      panMotionOn = true;
    }
    else panMotionOn = false;
  }

  void renderSeg(float *outL, float *outR, int frames)
  {
    // ADR-086: gravity integrates on a FIXED GRID, not once per render call.
    // It is explicit Euler on a nonlinear ODE, so stepping with dt = the block
    // length made the result depend on how a buffer was SUBDIVIDED — one call
    // of n frames and n/256 chunked calls disagreed (measured 1.03 max sample
    // difference at grav 0.5, and 0 with gravity off). That made renders
    // non-reproducible across host buffer sizes, and made oscillator 0 (one
    // whole call) integrate differently from oscillators 1..N (kMixChunk).
    // A constant dt driven by an accumulator depends only on the cumulative
    // sample count, never on its subdivision.
    // 256 and not the 16-sample control tick: with 10 held notes gravity is
    // O(gated^2) per step, and a 16-sample grid measured +66% CPU to buy a
    // settling difference of 0.001 cents. 256 costs +2%.

    const int n = (int)p.n;
    // ADR-101 hoists: anchor index + blend fraction are pure functions of the
    // params, invariant across the segment.
    const bool sawBaseOn = p.sawBase > 0.001;
    const double sawSbF = std::max(0.0, std::min(4.0, p.sawBase * 4));
    const int sawBi0 = std::min(3, (int)std::floor(sawSbF));
    const double sawBfr = sawSbF - sawBi0;
    const double sawSpF = std::max(0.0, std::min(4.0, p.sawProfile * 4));
    const int sawPi0 = std::min(3, (int)std::floor(sawSpF));
    const double sawPfr = sawSpF - sawPi0;
    const double gain = p.vol * 0.9 / std::pow((double)n, p.normExp);
    // ADR-021: at the defaults these are the reference's exact expressions
    // (attackS = 0.003, releaseS = 0.16 — same operands, same doubles).
    const double atk = forcecore::onePoleCoef(p.attackS, sr);
    const double rel = forcecore::onePoleCoef(p.releaseS, sr);
    const double dec = forcecore::onePoleCoef(p.decayS, sr);
    // ADR-063: per-sample leg of the frequency glide — a quarter of the
    // control-rate time constant, seconds -> coefficient. 0 leaves the path alone.
    const bool glideOn = p.freqGlide > 0;
    const double gCoefS = glideOn ? 1 - std::exp(-1 / (p.freqGlide * 0.25 * sr)) : 0;
    for (int i = 0; i < frames; i++) { outL[i] = 0.0f; outR[i] = 0.0f; }
    // pan motion (ADR-064, parity with reference/swarmsaw.html): slow LFOs sweep the base pan
    // once per block. mode 0 = independent per-voice drift, 1 = one shared sweep.
    // Centre pin scales the offset by distance from the fundamental.
    // ADR-086: pan motion is advanced ONCE PER CALL by advancePanMotion(),
    // not here. It is also a per-render-call integrator (phase += rate * dtB,
    // sampled once and held across the block), so segmenting the render for
    // gravity silently changed its update rate too — which broke parity on
    // nine SAW pan scenarios whose reference (reference/swarmsaw.html) was never part of
    // this ADR. Gravity was ratified for a fixed grid; pan motion was not.
    // Keeping it per-call is what confines this change to what was approved.
    const double *PL = panMotionOn ? panLm : panL;
    const double *PR = panMotionOn ? panRm : panR;
    /* B38: dB -> linear, once per tick (~2756/s; unmeasurable beside the voice
       loop). The DEFAULT IS SPECIAL-CASED and that is not laziness: pow(10,-4)
       is not guaranteed to return the same bits as the literal 1e-4, and a
       one-ULP difference in the retire threshold changes which sample a voice
       stops on -- which is a golden diff. The branch makes the shipped default
       bit-identical by construction rather than by hoping libm agrees. */
    const double cullEnv = (p.voiceCull <= -79.9999)
                             ? HZ_CULL_ENV
                             : std::pow(10.0, p.voiceCull / 20.0);
    const int osSub = p.oversample > 0.5 ? 2 : 1;   // ADR-075
    const bool ensOn = p.onsetScatter > 0;         // ADR-077
    const bool vEnvOn = p.voiceEnv > 0.5;          // ADR-078
    for (auto &s : voices)
    {
      if (!s.gate && s.env < cullEnv)
      {
        // RETIRE THE SLOT PROPERLY. The envelope is a one-pole: it asymptotes
        // toward zero and never arrives, so a skipped voice used to sit just
        // under the threshold FOREVER. Anything inspecting slot state then saw
        // every note ever played (human, 2026-08-03: "every time a note is
        // played it returns the whole note history"). Zeroing here also clears
        // the output pole, so no residual charge can outlive the note — the
        // ghost-voice failure mode this session went hunting for.
        if (s.env != 0.0)
        {
          s.env = 0;
          s.lpL = 0;
          s.lpR = 0;
          s.midi = -1;
          for (int i = 0; i < kMaxV; i++) { s.onsE[i] = 0; s.onsD[i] = 0; s.onsD0[i] = 0; }
        }
        continue;
      }
      // B81 seam: with a tap installed this note renders into double scratch;
      // frames > kTapMax (impossible at real rates, guarded anyway) falls back
      // to the direct path rather than overrunning.
      const bool tapping = tapFn != nullptr && frames <= kTapMax;
      if (tapping)
        for (int z = 0; z < frames; z++) { tapScratchL[z] = 0.0; tapScratchR[z] = 0.0; }
      int tick = this->tick;
      for (int smp = 0; smp < frames; smp++)
      {
        if (tick == 0) controlTick(s);
        tick = (tick + 1) & (kTick - 1);
        if (glideOn) for (int i = 0; i < n; i++) s.fRun[i] += gCoefS * (s.eff[i] - s.fRun[i]);
        double l = 0, r = 0;
        double vMax = 0;   // ADR-078: loudest voice envelope, for liveness
        // ADR-075: at 2x the voice sum runs TWICE per output sample with half
        // the phase step, and the two sub-samples feed a halfband decimator.
        // Everything after the sum (output pole, envelope, ITD head) stays at
        // 1x. osSub == 1 leaves the original path bit-identical.
        for (int u = 0; u < osSub; u++)
        {
        double ls = 0, rs = 0;
        for (int i = 0; i < n; i++)
        {
          // ADR-077: a voice that has not entered yet contributes nothing AND
          // does not advance its phase — it has not started playing.
          if ((ensOn || vEnvOn) && s.onsD[i] > 0) { s.onsD[i] -= 1; continue; }
          const double f = glideOn ? s.fRun[i] : s.eff[i];
          const double dph = std::max(0.0, f) / (sr * osSub);
          double ph = s.phase[i] + dph;
          ph -= std::floor(ph);
          s.phase[i] = ph;
          const double naive = 2 * ph - 1;
          double v = naive;
          if (p.digital > 0)
          {
            const double d = std::max(dph, 1e-6);
            double bl = 0;
            if (ph < d) { const double t = ph / d; bl = t + t - t * t - 1; }
            else if (ph > 1 - d) { const double t = (ph - 1) / d; bl = t * t + t + t + 1; }
            v = naive - p.digital * bl;
          }
          /* ADR-094 saw shape (glass). Placed HERE — immediately after the BLEP
             and BEFORE the ADR-058 shape morph — because that is where
             reference/swarmsaw.html puts it. ADR-058 is a C++-only superset with no
             reference, so it is the stage that must yield position; putting it
             first would order the reference-defined stages differently from the
             reference and parity would only notice once both were non-zero. */
          /* ADR-101: tables, not the libm anchors (see AnchorTables); the
             anchor SELECTION (index + blend fraction) depends only on the
             params, so it is hoisted to the segment head — it was recomputed
             per sample per voice, clamp, floor and all. A zero-weight side of
             the blend is skipped: at an exact anchor position that halves the
             lookups, and bfr multiplying an unevaluated table is the same
             arithmetic as bfr multiplying an evaluated one by 0... except it
             is not evaluated. */
          if (sawBaseOn)
          {
            const double b0 = sawBi0 == 0 ? v : sawBaseTab(sawBi0, ph);
            v = sawBfr <= 0.0 ? b0
                              : b0 * (1 - sawBfr) + sawBaseTab(sawBi0 + 1, ph) * sawBfr;
          }
          if (s.rnd[i] > 0.001)
          {
            const double shaped =
                sawPfr <= 0.0 ? sawShapeTab(sawPi0, ph)
                              : sawShapeTab(sawPi0, ph) * (1 - sawPfr)
                                    + sawShapeTab(sawPi0 + 1, ph) * sawPfr;
            v = v * (1 - s.rnd[i]) + shaped * s.rnd[i];
          }
          // ADR-058 waveshape morph: v = saw − shape·saw(ph+½). shape 0 = saw
          // (bit-exact, guarded); shape 1 = a band-limited square (the two
          // half-cycle-offset saws' difference). Both saws carry the SAME
          // polyBLEP correction, so the morph stays anti-aliased. C++-only
          // superset — no reference/swarmsaw.html reference for shape>0 (like ADR-025).
          if (p.shape > 0)
          {
            double ph2 = ph + 0.5;
            if (ph2 >= 1) ph2 -= 1;
            double saw2 = 2 * ph2 - 1;
            if (p.digital > 0)
            {
              const double d = std::max(dph, 1e-6);
              double bl2 = 0;
              if (ph2 < d) { const double t = ph2 / d; bl2 = t + t - t * t - 1; }
              else if (ph2 > 1 - d) { const double t = (ph2 - 1) / d; bl2 = t * t + t + t + 1; }
              saw2 = (2 * ph2 - 1) - p.digital * bl2;
            }
            v = v - p.shape * saw2;
          }
          if (s.vlpc[i] < 1) { s.vlp[i] += s.vlpc[i] * (v - s.vlp[i]); v = tiltHP ? (v - s.vlp[i]) : s.vlp[i]; }
          if (p.hiTame > 0) v *= s.hg[i];
          if (vEnvOn)
          {
            // ADR-078: a full per-voice ADSR. Same arithmetic as the shared
            // envelope (ADR-021), run once per voice — attack while gated,
            // decay toward sustain, own release coefficient when released.
            if (s.gate)
            {
              if (p.sustainL >= 1.0) s.onsE[i] += (1 - s.onsE[i]) * s.onsC[i];
              else if (s.vAtk[i])
              {
                s.onsE[i] += (1 - s.onsE[i]) * s.onsC[i];
                if (s.onsE[i] >= 0.995) s.vAtk[i] = 0;
              }
              else s.onsE[i] += (p.sustainL - s.onsE[i]) * dec;
            }
            else s.onsE[i] += (0 - s.onsE[i]) * s.relC[i];
            v *= s.onsE[i];
            if (s.onsE[i] > vMax) vMax = s.onsE[i];
          }
          else if (ensOn)
          {   // ADR-077 entry only: fade in on the voice's own attack so a late
              // entry does not click in at the shared envelope's current level
            s.onsE[i] += (1 - s.onsE[i]) * s.onsC[i];
            v *= s.onsE[i];
          }
          if (p.mono != 0) { ls += v * 0.7071; rs += v * 0.7071; }
          else if (itdSamp[i] > 0)
          {
            // ADR-074 mode F: near channel direct, far channel from the
            // voice's delay ring — a plain delayed copy, never inverted.
            const int rw = s.itdW & (kItdRing - 1);
            const float far = s.itdRing[i][(rw - itdSamp[i] + kItdRing) & (kItdRing - 1)];
            s.itdRing[i][rw] = (float)v;
            if (PL[i] >= PR[i]) { ls += v * PL[i]; rs += (double)far * PR[i]; }
            else { rs += v * PR[i]; ls += (double)far * PL[i]; }
          }
          else { ls += v * PL[i]; rs += v * PR[i]; }
        }
        s.itdW++;   // ADR-074/075: one tick per SUB-sample; itdSamp scales with OS
        if (osSub == 1) { l = ls; r = rs; }
        else
        {
          // push into the decimator history; emit on the second sub-sample
          s.osZL[s.osW & 63] = ls; s.osZR[s.osW & 63] = rs;
          s.osW++;
          if (u == 1)
          {
            double aL = 0, aR = 0;
            for (int t = 0; t < kHbTaps; t++)
            {
              const int k = (s.osW - 1 - t) & 63;
              aL += hb[t] * s.osZL[k]; aR += hb[t] * s.osZR[k];
            }
            // NO gain compensation. The halfband is normalised to unity DC
            // gain and both sub-samples are REAL samples (not zero-stuffed),
            // so filtering + taking one of two preserves amplitude. The first
            // version multiplied by 2 "to restore energy" — that is the
            // UPSAMPLING rule, wrong here, and it made OS +5.9 dB louder,
            // which soft-clipped the tanh at high K (human caught it by ear:
            // "oversampling is louder ... the waveform seems to get clipped").
            l = aL; r = aR;
          }
        }
        }
        if (p.lpOut != 0)
        {
          s.lpL += s.lpc * (l - s.lpL);
          s.lpR += s.lpc * (r - s.lpR);
        }
        else
        {
          // DYN-reference config: no output pole (the one structural
          // difference between the references; ADR-023)
          s.lpL = l;
          s.lpR = r;
        }
        // ADR-021 envelope. sustainL >= 1: the reference's exact arithmetic
        // ((1-env)*atk while gated, (0-env)*rel released) — decay never
        // engages, parity preserved. sustainL < 1: attack->decay machine,
        // deliberately divergent (superset behavior).
        if (vEnvOn)
        {
          // The shared envelope becomes pure BOOKKEEPING: every liveness,
          // voice-steal and NOTE_END test keys off s.env, so it tracks the
          // loudest voice and all that machinery keeps working unchanged
          // while the AUDIO is enveloped per voice above.
          s.env = vMax;
        }
        else if (s.gate)
        {
          if (p.sustainL >= 1.0)
          {
            s.env += (1 - s.env) * atk;
          }
          else if (s.inAttack)
          {
            s.env += (1 - s.env) * atk;
            if (s.env >= 0.995) s.inAttack = 0;
          }
          else
          {
            s.env += (p.sustainL - s.env) * dec;
          }
        }
        else
        {
          s.env += (0 - s.env) * rel;
        }
        // ADR-084: velocity and smoothed pressure scale the voice. Both default
        // 1.0 (exact), so the multiply is bit-inert for goldens and old hosts.
        const double g = (vEnvOn ? gain : gain * s.env) * s.vel * s.pressSm;
        if (tapping)
        {
          // raw doubles; the float-store rounding happens at the flush below,
          // with the same expression shape as the direct path
          tapScratchL[smp] += s.lpL * g;
          tapScratchR[smp] += s.lpR * g;
        }
        else
        {
          // Float32Array += semantics: round to f32 on every store
          outL[smp] = (float)((double)outL[smp] + s.lpL * g);
          outR[smp] = (float)((double)outR[smp] + s.lpR * g);
        }
      }
      if (tapping)
      {
        tapFn(tapCtx, (int)(&s - &voices[0]), s.midi, tapScratchL, tapScratchR, frames);
        for (int smp = 0; smp < frames; smp++)
        {
          outL[smp] = (float)((double)outL[smp] + tapScratchL[smp]);
          outR[smp] = (float)((double)outR[smp] + tapScratchR[smp]);
        }
      }
    }
    this->tick = (this->tick + frames) & (kTick - 1);
    if (p.width > 1.0 && (int)p.superMode != 0)
    {
      // ADR-074: super-width is now a 3-mode system; the post-mix stage serves
      // the two CHARACTER modes only. Mode F (default 0, clean) acts entirely
      // at the seats/ITD above and needs no post stage.
      if ((int)p.superMode == 1)
      {
        // mode A "pulse" — the original ADR-025 M/S boost, kept verbatim. Its
        // negative cross-term (L' = 1.5L - 0.5R at width 1.5) injects
        // phase-inverted opposite-side voices: audible pulse-like combing and
        // waveform up-cliffs. DOCUMENTED CHARACTER, not a defect — pinned as
        // the expected exception in waveshape_check.
        const double sideGain = 1 + (p.width - 1) * 2;
        for (int smp = 0; smp < frames; smp++)
        {
          const double mid = ((double)outL[smp] + (double)outR[smp]) * 0.5;
          const double side = ((double)outL[smp] - (double)outR[smp]) * 0.5 * sideGain;
          outL[smp] = (float)(mid + side);
          outR[smp] = (float)(mid - side);
        }
      }
      else
      {
        // mode D "smear" — one-pole allpass on the side channel before the
        // boost: the inversion still exists but is frequency-dependent, which
        // reads as motion/smear rather than a broadband pulse. Same documented-
        // character status as mode A.
        const double apc = 1 - std::exp(-kTau * 700.0 / sr);
        const double sideGain = 1 + (p.width - 1) * 1.2;
        for (int smp = 0; smp < frames; smp++)
        {
          const double mid = ((double)outL[smp] + (double)outR[smp]) * 0.5;
          double side = ((double)outL[smp] - (double)outR[smp]) * 0.5;
          apZ += apc * (side - apZ);
          side = (2 * apZ - side) * sideGain;
          outL[smp] = (float)(mid + side);
          outR[smp] = (float)(mid - side);
        }
      }
    }
    for (int smp = 0; smp < frames; smp++)
    {
      outL[smp] = (float)std::tanh((double)outL[smp]);
      outR[smp] = (float)std::tanh((double)outR[smp]);
    }
    const Voice *foc = focus();
    if (foc) std::memcpy(lastPhase, foc->phase, sizeof(lastPhase));   // ADR-062 keep-phase snapshot
  }

public:

  Params p;

 private:
  static int32_t toInt32(double v) { return (int32_t)(int64_t)v; }

  // mulberry32 shared with the Track E force system (ADR-034 unification —
  // the one piece of arithmetic the two dynamics families genuinely share).
  // Parity 51/51 proves the delegation is bit-neutral.
  static double rngNext(uint32_t &state) { return forcecore::rngNext(state); }

  double *paramSlot(const std::string &k)
  {
    if (k == "n") return &p.n;
    if (k == "dist") return &p.dist;
    if (k == "seed") return &p.seed;
    if (k == "detune") return &p.detune;
    if (k == "law") return &p.law;
    if (k == "K") return &p.K;
    if (k == "onset") return &p.onset;
    if (k == "dissolve") return &p.dissolve;
    if (k == "driftDepth") return &p.driftDepth;
    if (k == "driftRate") return &p.driftRate;
    if (k == "inertia") return &p.inertia;
    if (k == "rtone") return &p.rtone;
    if (k == "normExp") return &p.normExp;
    if (k == "width") return &p.width;
    if (k == "mono") return &p.mono;
    if (k == "digital") return &p.digital;
    if (k == "vol") return &p.vol;
    if (k == "retrig") return &p.retrig;
    if (k == "attack") return &p.attackS;
    if (k == "decay") return &p.decayS;
    if (k == "sustain") return &p.sustainL;
    if (k == "release") return &p.releaseS;
    if (k == "bpm") return &p.bpm;
    if (k == "beatMult") return &p.beatMult;
    if (k == "topo") return &p.topo;
    if (k == "reach") return &p.reach;
    if (k == "mu") return &p.mu;
    if (k == "balance") return &p.balance;
    if (k == "alpha") return &p.alpha;
    if (k == "poles") return &p.poles;
    if (k == "grav") return &p.grav;
    if (k == "basin") return &p.basin;
    if (k == "lpOut") return &p.lpOut;
    if (k == "glide") return &p.glide;
    if (k == "tune") return &p.tune;
    if (k == "absK") return &p.absK;
    if (k == "scatter") return &p.scatter;
    if (k == "panScatter") return &p.panScatter;
    if (k == "shape") return &p.shape;  // ADR-058 waveshape morph
    if (k == "sawBase") return &p.sawBase;        // ADR-094 saw shape (glass)
    if (k == "sawProfile") return &p.sawProfile;
    if (k == "round") return &p.round;
    if (k == "roundHi") return &p.roundHi;
    if (k == "tilt") return &p.tilt;    // ADR-060 tone tilt
    // "toneTilt" is the CLAP-facing alias for the same field (ADR-072): the
    // shell mirrors unguarded param keys into BOTH cores, and SPECTRA already
    // owns the key "tilt" (id 45 Amp Tilt) — a param named "tilt" would write
    // both engines. The collision is retired at the interface, not guarded.
    if (k == "toneTilt") return &p.tilt;
    if (k == "hiTame") return &p.hiTame;  // ADR-061 hi-tame equal-loudness
    if (k == "driftMode") return &p.driftMode;  // ADR-062 drift modes
    if (k == "keepPhase") return &p.keepPhase;  // ADR-062 keep-phase
    if (k == "freqGlide") return &p.freqGlide;  // ADR-063 frequency glide
    if (k == "panMotion") return &p.panMotion;    // ADR-064 pan motion
    if (k == "panMode") return &p.panMode;        // ADR-064 pan-motion mode
    if (k == "motionCenter") return &p.motionCenter;  // ADR-064 centre pin
    if (k == "harmReach") return &p.harmReach;        // ADR-065 harmonic-law reach
    if (k == "stretchB") return &p.stretchB;          // ADR-066 stretch-law inharmonicity
    if (k == "spread") return &p.spread;              // ADR-068 detune multiplier
    if (k == "anchor") return &p.anchor;              // ADR-068 root anchor
    if (k == "pivotMode") return &p.pivotMode;        // ADR-069 sync pivot
    if (k == "panLayout") return &p.panLayout;        // ADR-070 pan image
    if (k == "panCurve") return &p.panCurve;          // ADR-070 fan curve
    if (k == "panInvert") return &p.panInvert;        // ADR-070 fan invert
    if (k == "superMode") return &p.superMode;        // ADR-074 super-width mode
    if (k == "oversample") return &p.oversample;      // ADR-075 2x OS
    if (k == "polyGlide") return &p.polyGlide;        // ADR-076 poly glide
    if (k == "glideMode") return &p.glideMode;        // ADR-076 voice vs memory
    if (k == "onsetScatter") return &p.onsetScatter;  // ADR-077 (ms, 0 = off)
    if (k == "onsetAlpha") return &p.onsetAlpha;      // ADR-077 correction gain
    if (k == "attackScatter") return &p.attackScatter;
    if (k == "voiceEnv") return &p.voiceEnv;          // ADR-078 per-voice ADSR
    if (k == "voiceCull") return &p.voiceCull;        // B38 retire threshold (dB)
    if (k == "relScatter") return &p.relScatter;
    return nullptr;
  }

  void rebuild()
  {
    const int n = (int)p.n;
    grng = (uint32_t)(toInt32(p.seed) + 1);
    if ((int)p.topo == 2)
    {
      // bimodal placement tied to the two-cluster topology (DYN reference
      // exact): cluster A on [-0.85,-0.35], cluster B on [0.35,0.85].
      const int h = n >> 1;
      for (int i = 0; i < h; i++)
      {
        const double t = h == 1 ? 0.5 : (double)i / (h - 1);
        x[i] = -0.85 + 0.5 * t;
      }
      for (int i = h; i < n; i++)
      {
        const double t = (n - h) == 1 ? 0.5 : (double)(i - h) / (n - h - 1);
        x[i] = 0.35 + 0.5 * t;
      }
      finishRebuild(n);
      return;
    }
    static const double JP[7] = {-1, -0.5715, -0.1774, 0, 0.181, 0.565, 0.9766};
    for (int i = 0; i < n; i++)
    {
      const double u = (n == 1) ? 0.5 : (double)i / (n - 1);
      double xv;
      if (p.dist == 0) { xv = 2 * u - 1; }
      else if (p.dist == 1)
      {
        const double pos = u * 6;  // (JP.length - 1)
        const int a = (int)std::floor(pos);
        const int b = a + 1 < 6 ? a + 1 : 6;
        xv = JP[a] + (JP[b] - JP[a]) * (pos - a);
      }
      else if (p.dist == 2)
      {
        const double u1 = std::max(rngNext(grng), 1e-9), u2 = rngNext(grng);
        xv = std::sqrt(-2 * std::log(u1)) * std::cos(kTau * u2) / 2.5;
        if (xv > 1) xv = 1;
        if (xv < -1) xv = -1;
      }
      else if (p.dist == 3)
      {
        xv = std::tan(kPiRef * (rngNext(grng) - 0.5)) / 4;
        if (xv > 1) xv = 1;
        if (xv < -1) xv = -1;
      }
      else
      {
        // GOLDEN (ADR-067, parity with reference/swarmsaw.html): low-discrepancy irrational
        // placement, (i+1)*phi^-1 mod 1 — even-but-inharmonic, no rng draws.
        // std::fmod == JS % here (both operands positive).
        xv = 2 * std::fmod((i + 1) * 0.6180339887498949, 1.0) - 1;
      }
      x[i] = xv;
    }
    if (n == 1) x[0] = 0;
    finishRebuild(n);
  }

  void finishRebuild(int n)
  {
    centerIdx = 0;
    for (int i = 1; i < n; i++)
      if (std::fabs(x[i]) < std::fabs(x[centerIdx])) centerIdx = i;
    // lowest raw-x voice for the root anchor (ADR-068), mirroring swarmsaw
    xmin = x[0];
    for (int i = 1; i < n; i++)
      if (x[i] < xmin) xmin = x[i];
    // Pan scatter (ADR-035): at 0 the arithmetic below is the legacy path
    // exactly (goldens are the proof). At > 0, blend each voice's pan
    // position toward a seeded permutation of the legacy positions — the
    // permutation stream is derived from the seed but INDEPENDENT of the
    // phase/drift stream, so engaging it never shifts any other draw.
    int perm[kMaxV];
    const double ps = p.panScatter;
    if (ps > 0)
    {
      for (int i = 0; i < n; i++) perm[i] = i;
      uint32_t prng = (uint32_t)(int64_t)p.seed * 2654435761u ^ 0x9E3779B9u;
      for (int i = n - 1; i > 0; i--)
      {
        const int j = (int)(rngNext(prng) * (i + 1));
        const int t = perm[i];
        perm[i] = perm[j];
        perm[j] = t;
      }
    }
    double pos[kMaxV];
    const double wCap = std::min(1.0, p.width);  // super-width (ADR-025) acts later
    if ((int)p.panLayout == 1)
    {
      // legacy image (pre-ADR-070): pan proportional to raw x
      for (int i = 0; i < n; i++)
        pos[i] = std::max(-1.0, std::min(1.0, x[i])) * wCap;
    }
    else
    {
      // ALTERNATING PITCH-RANKED FAN (ADR-070, the new DEFAULT — parity with
      // reference/swarmsaw.html): rank by pitch (by index when harmonic), rank r steps out
      // from centre on alternating sides, distance reshaped by panCurve; invert
      // flips the triangle. Rank 0 (the fundamental) sits exactly centre.
      // std::stable_sort, NOT std::sort: JS Array.sort is stable (ES2019), and
      // gaussian/cauchy clamp at +/-1 so ties are REACHABLE — an unstable sort
      // would order ties differently and break parity.
      int idx[kMaxV];
      for (int i = 0; i < n; i++) idx[i] = i;
      if ((int)p.law != 4)
        std::stable_sort(idx, idx + n, [&](int a, int b) { return x[a] < x[b]; });
      // ADR-074 mode F seat steepening: width > 1 pushes seats outward via a
      // curve exponent (audition law from the width lab). 1.0 at width <= 1 or
      // in other modes, so the reference regime is bit-untouched.
      const double steep = ((int)p.superMode == 0 && p.width > 1.0)
                               ? 1.0 / (1.0 + 2.0 * (p.width - 1.0)) : 1.0;
      const double gamma = std::pow(6, 0.5 - p.panCurve) * steep;
      for (int r = 0; r < n; r++)
      {
        // PARITY FORK (ADR-073, parity with reference/swarmsaw.html): odd n keeps rank 0
        // at dead centre (ADR-070's image); EVEN n has no centre seat — pairs
        // sit symmetrically at ±(k+0.5)/(n/2). The old law degenerated at n=2
        // to centre + hard side, lopsided at any width.
        double d;
        if (n == 1) d = 0.0;
        else if (n % 2 == 1) d = (double)r / (n - 1);
        else d = ((double)(r / 2) + 0.5) / (n / 2.0);
        if (p.panInvert != 0) d = 1 - d;
        d = std::pow(d, gamma);
        pos[idx[r]] = std::max(-1.0, std::min(1.0, (r % 2 == 0 ? -1.0 : 1.0) * d * wCap));
      }
    }
    for (int i = 0; i < n; i++)
    {
      double pan = pos[i];
      if (ps > 0) pan += (pos[perm[i]] - pos[i]) * ps;
      // ADR-074 mode F per-voice ITD: width beyond 1 delays each voice's FAR
      // channel in proportion to its seat distance (0.6 ms max at width 2 —
      // the audition value). Precedence-effect width; every cross-feed
      // coefficient stays non-negative, unlike mode A's M/S boost whose
      // negative cross-term injected phase-inverted saws (the up-cliff bug).
      // 0.3 ms coefficient, NOT the 0.6 ms audition value (measured 2026-08-03,
      // human-approved): both width metrics SATURATE above ~0.15 ms, so 0.6
      // bought no measurable width while pushing mono-sum comb nulls down to
      // ~833 Hz (first null ~ sr/2N). At 0.3 the fan stays inside natural
      // interaural delay (0.51 ms = head width / c) through width 1.5 and only
      // reaches 0.6 ms at width 2.
      // ITD is in SUB-SAMPLES: with ADR-075 oversampling the voice loop (and
      // this ring) tick at 2x, so the sample count doubles to keep the same
      // delay in SECONDS. rebuild() re-runs on `oversample` for this reason.
      const int osMul = p.oversample > 0.5 ? 2 : 1;
      itdSamp[i] = ((int)p.superMode == 0 && p.width > 1.0)
                       ? (int)std::lround(std::fabs(pan) * (p.width - 1.0) * 2.0
                                          * 0.0003 * sr * osMul) : 0;
      if (itdSamp[i] > kItdRing - 2) itdSamp[i] = kItdRing - 2;
      const double th = (pan + 1) * 0.25 * kPiRef;
      panBase[i] = pan;   // signed base pan (post-scatter), kept for pan motion (ADR-064)
      panL[i] = std::cos(th);
      panR[i] = std::sin(th);
    }
  }

  Voice &alloc()
  {
    // Three-tier steal policy (ADR-083, deliberate divergence from the JS
    // reference's steal-oldest). The reference steals the oldest voice
    // REGARDLESS of gate — and under an arpeggio the oldest voice is precisely
    // the note being deliberately held: release tails occupy slots for ~1.1 s
    // (env < 1e-3 at tau 0.16 s), a 9-note/s arp keeps ~10 tails alive, the
    // pool fills, and the sustain is sacrificed first. Measured: stolen ~11
    // arp notes in, held-note f0 power to 7% of its beating floor.
    //   1) a FREE slot (released and faded) — oldest first, as before;
    //   2) a RELEASING tail — quietest first (least audible loss), age as
    //      tiebreak. An arp recycles its own tails and never touches holds;
    //   3) only when every slot is GATED: oldest held note (unavoidable).
    // Tiers 1 and 3 match the reference exactly; goldens never overflow the
    // pool, so parity is untouched (verified: 147/147 unchanged).
    Voice *best = nullptr;
    for (auto &s : voices)
      if (!s.gate && s.env < 1e-3)
        if (!best || s.age < best->age) best = &s;
    if (best) return *best;
    for (auto &s : voices)
      if (!s.gate)
        if (!best || s.env < best->env || (s.env == best->env && s.age < best->age))
          best = &s;
    if (best) return *best;
    for (auto &s : voices)
      if (!best || s.age < best->age) best = &s;
    return *best;
  }

  static double erb(double f) { return 24.7 * (4.37 * f / 1000 + 1); }

  void controlTick(Voice &s)
  {
    // ADR-084: ~20 ms pressure smoothing, seconds -> per-tick coefficient
    s.pressSm += (s.press - s.pressSm) * (1 - std::exp(-(kTick / sr) / 0.02));
    if (std::fabs(s.pressSm - s.press) < 1e-6) s.pressSm = s.press;
    const int n = (int)p.n;
    const double dt = kTick / sr;
    // ADR-063 frequency glide (parity with reference/swarmsaw.html): seconds -> coefficient.
    const bool firstTick = !s.vfInit;
    const bool glideOn = p.freqGlide > 0;
    const double gCoefT = glideOn ? 1 - std::exp(-dt / p.freqGlide) : 0;
    if (s.glideActive)
    {
      // ADR-096: the five-law traveller, stepped in SEMITONES. This site ran a
      // one-pole in HERTZ for ADR-026/076; the law is identical (glide_core.h:110
      // is the same `x += (target-x)*(1-exp(-dt/tau))`) but the DOMAIN is not, and
      // Hz-linear travel accelerates audibly at the bottom of a wide interval.
      // Semitones is the domain the bench measured and the goldens were sliced in.
      // `p.noteLaw` arrives COMPLETE, tau included. It used to be overridden here
      // from `glide` unconditionally, which silently un-followed the bend lane:
      // a lane set to "follow bend law" on the LAG law read id 33 (default 0)
      // instead of bendTau, so lag read as broken. The shell owns which of the
      // two taus is live, because the shell owns the link.
      // base aligns the lane's log2(f)*12 space with MIDI pitch classes: MIDI =
      // 12*log2(f) + (69 - 12*log2(440)), and without this shift the scale mask
      // was misaligned by 0.376 st — every scale-quantised note landed off the
      // grid by a constant 37.6 cents.
      constexpr double kLogFreqToMidi = 69.0 - 12.0 * 8.781359713524660;  // 12*log2(440)
      const double newF0 =
          std::exp2(s.travel.step(std::log2(s.glideTarget) * 12.0, p.noteLaw,
                                  kLogFreqToMidi) / 12.0);
      const double ratio = newF0 / s.f0;
      s.f0 = newF0;
      s.f0cur *= ratio;
      if (std::fabs(s.f0 - s.glideTarget) < 0.01)
      {
        const double snap = s.glideTarget / s.f0;
        s.f0 = s.glideTarget;
        s.f0cur *= snap;
        s.glideActive = 0;
      }
    }
    s.Kenv *= std::exp(-dt / std::max(0.01, p.dissolve));
    if (p.driftDepth > 0)
    {
      // ADR-062 drift modes (0 walk = original 1/f; 1 sine; 2 sample&hold), parity with reference/swarmsaw.html
      const int dm = (int)p.driftMode;
      const double rate = (0.2 + p.driftRate * 8);
      for (int i = 0; i < n; i++)
      {
        if (dm == 1)
        {
          s.driftPh[i] += (0.05 + p.driftRate * 4) * (0.6 + i * 0.09) * dt;
          s.driftS[i] = std::sin(6.283185307 * s.driftPh[i]);
        }
        else if (dm == 2)
        {
          s.driftHoldT[i] -= dt;
          if (s.driftHoldT[i] <= 0) { s.driftS[i] = rngNext(s.rngState) * 2 - 1; s.driftHoldT[i] = 0.03 + (1 - p.driftRate) * 0.6; }
        }
        else
        {
          s.driftS[i] += (rngNext(s.rngState) - 0.5) * 2 * std::sqrt(rate * dt);
          s.driftS[i] -= s.driftS[i] * 0.4 * dt;
          if (s.driftS[i] > 1) s.driftS[i] = 1;
          if (s.driftS[i] < -1) s.driftS[i] = -1;
        }
      }
    }
    double mean = 0;
    // ADR-027/036; tune and noteTune at 1.0 -> bit-identical
    const double f0c = s.f0cur * p.tune * s.noteTune;
    // ADR-068 octave spread + root anchor, threading through EVERY law below
    // (including tempo-grid, which the lab lacks — uniform placement semantics,
    // recorded in the ADR). Defaults bit-inert: detune*1 == detune, x - 0 == x.
    const double dep = p.detune * p.spread;
    for (int i = 0; i < n; i++)
    {
      double f;
      const double xv = x[i] - p.anchor * xmin;
      if (p.law == 0) { f = f0c * std::pow(2, (xv * dep * 100) / 1200); }
      else if (p.law == 1) { f = f0c + xv * dep * 20; }
      else if (p.law == 3)
      {
        // tempo-grid (ADR-022): cents placement, then snap the Hz offset to
        // the nearest multiple of u — every pairwise beat rate becomes an
        // exact grid multiple. Expression ported verbatim from the DYNAMICS
        // reference. Drift (below) deliberately loosens the grid when used.
        const double u = (p.bpm / 60.0) * p.beatMult;
        const double df = f0c * (std::pow(2, (xv * dep * 100) / 1200) - 1);
        f = f0c + std::round(df / u) * u;
      }
      // HARMONIC (ADR-065, parity with reference/swarmsaw.html): voice i morphs from unison
      // (dep 0) up to its partial — at dep 1, f0*(1 + harmReach*i). The voice
      // INDEX is the rung, so this law ignores the distribution (xv) and anchor
      // (inherently root-anchored); spread still scales it (ADR-068).
      // NOTE law 3 is the tempo-grid law (ADR-022), hence harmonic takes index 4.
      else if (p.law == 4) { f = f0c * (1 + dep * p.harmReach * i); }
      // STRETCH (ADR-066, parity with reference/swarmsaw.html): cents placement with the
      // offset stretched by (1 + stretchB*x^2) — outer voices spread further,
      // piano/bell inharmonicity. Law index 5: 3 is tempo-grid, 4 is harmonic.
      else if (p.law == 5)
      {
        const double rat = std::pow(2, (xv * dep * 100) / 1200) - 1;
        f = f0c * (1 + rat * (1 + p.stretchB * xv * xv));
      }
      else { f = f0c + xv * dep * 0.35 * erb(f0c); }
      // centre pin (ADR-064): scale drift by distance from the fundamental (prev tick)
      if (p.driftDepth > 0) { const double mw = 1 - p.motionCenter * (1 - s.cdist[i]); f *= std::pow(2, (s.driftS[i] * p.driftDepth * mw) / 1200); }
      const double target = std::max(1.0, f);
      if (glideOn)
      {
        if (firstTick) s.vfSm[i] = target; else s.vfSm[i] += gCoefT * (target - s.vfSm[i]);
        s.vf[i] = s.vfSm[i];
      }
      else s.vf[i] = target;
      mean += s.vf[i];
    }
    s.vfInit = 1;
    mean /= n;
    if (p.motionCenter > 0)
    {
      double maxdev = 1e-9;
      for (int i = 0; i < n; i++) { const double dd = std::fabs(s.vf[i] - s.f0); if (dd > maxdev) maxdev = dd; }
      for (int i = 0; i < n; i++) s.cdist[i] = std::fabs(s.vf[i] - s.f0) / maxdev;
    }
    double varsum = 0;
    for (int i = 0; i < n; i++)
    {
      const double d = s.vf[i] - mean;
      varsum += d * d;
    }
    s.sigma = std::max(0.08, std::sqrt(varsum / n));
    // per-voice tone tilt (ADR-060, parity with reference/swarmsaw.html): pitch-tracked
    // one-pole. >0 darken (LP), <0 thin (HP); cutoff rises as sqrt(f/f0). 0 = inert.
    const double tm = std::fabs(p.tilt);
    tiltHP = p.tilt < 0;
    const double Ht = tm <= 0.005 ? 0 : (p.tilt > 0 ? 2 * std::pow(200.0, 1 - tm) : 0.1 * std::pow(24.0, tm));
    const double nyqt = sr * 0.5;
    for (int i = 0; i < n; i++)
    {
      if (Ht <= 0) s.vlpc[i] = 1;
      else { const double fc = std::min(nyqt * 0.98, Ht * s.f0 * std::sqrt(std::max(1.0, s.vf[i] / s.f0))); s.vlpc[i] = 1 - std::exp(-kTau * fc / sr); }
    }
    // hi-tame (ADR-061, parity with reference/swarmsaw.html): equal-loudness roll-off,
    // gain (f0/f)^hiTame turns the higher voices down. hiTame 0 → inert.
    if (p.hiTame > 0) for (int i = 0; i < n; i++) s.hg[i] = std::pow(s.f0 / std::max(s.f0, s.vf[i]), p.hiTame);
    // ADR-094 per-voice roundness: how far this voice morphs toward the profile
    // shape, optionally scaled by its position in the spread (roundHi > 0 =>
    // higher voices round more). Zero when `round` is zero, which is what keeps
    // the whole feature inert at its default.
    if (p.round > 0.001)
    {
      double xmin = x[0], xmax = x[0];
      for (int i = 1; i < n; i++) { if (x[i] < xmin) xmin = x[i]; if (x[i] > xmax) xmax = x[i]; }
      const double span = (xmax - xmin) != 0.0 ? (xmax - xmin) : 1.0;
      for (int i = 0; i < n; i++)
      {
        const double up = ((int)p.law == 4) ? ((n == 1) ? 0.0 : (double)i / (n - 1))
                                            : (x[i] - xmin) / span;
        s.rnd[i] = std::max(0.0, std::min(1.0, p.round * (1 + p.roundHi * (2 * up - 1))));
      }
    }
    else for (int i = 0; i < n; i++) s.rnd[i] = 0.0;
    const double km = 4 * p.K * std::fabs(p.K);
    // absK (ADR-004/ADR-033): coupling in absolute units of 2.5 Hz (max
    // pull 4*K^2*2.5 = 10 Hz at knob 1) so identical-oscillator states are
    // reachable with real authority; default path untouched.
    const double sigmaU = (p.absK != 0) ? 2.5 : s.sigma;
    // ADR-056 bipolar onset: route the signed Kenv by sign — positive adds to
    // sync (unchanged), negative adds to splay (x3, matching the steady splay
    // gain). For onset >= 0 (Kenv >= 0) this is bit-identical to the reference:
    // max(0,Kenv)==Kenv adds to syncT, max(0,-Kenv)==0 leaves splayT untouched.
    const double syncT = (std::max(0.0, km) + std::max(0.0, s.Kenv)) * sigmaU;
    const double splayT = (std::max(0.0, -km) * 3 + std::max(0.0, -s.Kenv) * 3) * sigmaU;
    s.KsmS += (syncT - s.KsmS) * 0.08;
    s.KsmP += (splayT - s.KsmP) * 0.08;
    double sx = 0, sy = 0;
    for (int i = 0; i < n; i++)
    {
      const double a = s.phase[i] * kTau;
      sx += std::cos(a);
      sy += std::sin(a);
    }
    sx /= n;
    sy /= n;
    s.R = std::sqrt(sx * sx + sy * sy);
    s.psi = std::atan2(sy, sx);
    double nx = 0, ny = 0;
    for (int i = 0; i < n; i++)
    {
      const double a = s.phase[i] * kTau * n;
      nx += std::cos(a);
      ny += std::sin(a);
    }
    s.RN = std::sqrt(nx * nx + ny * ny) / n;
    // Topology / Sakaguchi / Daido (ADR-023, DYN reference exact). SAW
    // defaults (topo 0, alpha 0, poles 1) reduce every expression to the SAW
    // reference's own: sin(psi - theta - 0.0) is bit-equal to sin(psi -
    // theta), and the splay term only exists on the mean-field path (the SAW
    // reference has no topologies; the DYN reference has no splay).
    const double alphaR = p.alpha * kPiFull / 180;
    const int topo = (int)p.topo;
    if (topo == 0)
    {
      const int q = (int)p.poles;
      if (q > 1)
      {
        double qx = 0, qy = 0;
        for (int i = 0; i < n; i++)
        {
          const double a = s.phase[i] * kTau * q;
          qx += std::cos(a);
          qy += std::sin(a);
        }
        qx /= n;
        qy /= n;
        s.RQ = std::sqrt(qx * qx + qy * qy);
        const double psiQ = std::atan2(qy, qx);
        for (int i = 0; i < n; i++)
          s.couple[i] = s.KsmS * s.RQ * std::sin(psiQ - s.phase[i] * kTau * q - alphaR);
      }
      else
      {
        s.RQ = 0;
        // ROOT-PINNED PACEMAKER (ADR-069, parity with reference/swarmsaw.html): pivotMode 1
        // entrains every voice to the FUNDAMENTAL (voice nearest f0); sin(root-self)
        // is zero for the root, so it stays and the swarm folds onto the played
        // pitch. Applies on THIS path only (topo 0, poles 1 — the lab has neither
        // topology nor Daido poles; other combinations are gated off, see ADR).
        // Lab-measured trait, kept: the pacemaker drops the R scaler, so its
        // onset just off K=0 is a touch stronger than mean-field's. alphaR rides
        // along uniformly (0 in the SAW reference -> bit-equal).
        const bool pivotRoot = p.pivotMode != 0;
        int rootIdx = 0;
        if (pivotRoot)
        {
          double rootD = std::fabs(s.vf[0] - s.f0);
          for (int i = 1; i < n; i++)
          {
            const double dd = std::fabs(s.vf[i] - s.f0);
            if (dd < rootD) { rootD = dd; rootIdx = i; }
          }
        }
        const int c0 = pivotRoot ? rootIdx : (centerIdx < n ? centerIdx : 0);
        for (int i = 0; i < n; i++)
        {
          double c = pivotRoot
                       ? s.KsmS * std::sin(kTau * (s.phase[rootIdx] - s.phase[i]) - alphaR)
                       : s.KsmS * s.R * std::sin(s.psi - s.phase[i] * kTau - alphaR);
          if (s.KsmP > 0.001)
            c += s.KsmP * std::sin(kTau * (s.phase[c0] + (double)(i - c0) / n - s.phase[i]));
          s.couple[i] = c;
        }
      }
      s.RA = 0;
      s.RB = 0;
    }
    else if (topo == 1)
    {
      const int r = std::min((int)p.reach, (n - 1) >> 1);
      for (int i = 0; i < n; i++)
      {
        const double ti = s.phase[i] * kTau;
        double acc = 0;
        for (int d = 1; d <= r; d++)
        {
          const int jl = (i - d + n) % n, jr = (i + d) % n;
          acc += std::sin(s.phase[jl] * kTau - ti - alphaR);
          acc += std::sin(s.phase[jr] * kTau - ti - alphaR);
        }
        s.couple[i] = s.KsmS * acc / (2 * r);
      }
      s.RA = 0;
      s.RB = 0;
    }
    else
    {
      const int h = n >> 1;
      const double m = p.mu;
      double ax = 0, ay = 0, bx = 0, by = 0;
      for (int i = 0; i < h; i++)
      {
        const double a = s.phase[i] * kTau;
        ax += std::cos(a);
        ay += std::sin(a);
      }
      for (int i = h; i < n; i++)
      {
        const double a = s.phase[i] * kTau;
        bx += std::cos(a);
        by += std::sin(a);
      }
      ax /= h;
      ay /= h;
      bx /= (n - h);
      by /= (n - h);
      const double RA = std::hypot(ax, ay), psiA = std::atan2(ay, ax);
      const double RB = std::hypot(bx, by), psiB = std::atan2(by, bx);
      s.RA = RA;
      s.RB = RB;
      const double norm = 1 + m;
      // A/B balance (ADR-051, DYN reference exact): cluster A keeps gain 1, B is
      // scaled by kB. balance 0 -> kB 1 -> s.couple = s.KsmS*c (bit-identical).
      const double kB = 1 - 2 * p.balance;
      for (int i = 0; i < n; i++)
      {
        const double ti = s.phase[i] * kTau;
        double c;
        double kGain;
        if (i < h) { c = (RA * std::sin(psiA - ti - alphaR) + m * RB * std::sin(psiB - ti - alphaR)) / norm; kGain = 1; }
        else { c = (RB * std::sin(psiB - ti - alphaR) + m * RA * std::sin(psiA - ti - alphaR)) / norm; kGain = kB; }
        s.couple[i] = s.KsmS * kGain * c;
      }
    }
    const double w = p.inertia;
    if (w <= 0.001)
    {
      for (int i = 0; i < n; i++)
      {
        s.eff[i] = s.vf[i] + s.couple[i];
        s.mom[i] = 0;
      }
      s.fresh = 0;
    }
    else
    {
      if (s.fresh)
      {
        for (int i = 0; i < n; i++)
        {
          s.eff[i] = s.vf[i] + s.couple[i];
          s.mom[i] = 0;
        }
        s.fresh = 0;
      }
      const double w0 = kTau * (8 * (1 - w) + 0.6);
      const double S = w0 * w0, D = 0.9 * w0;  // zeta = 0.45
      for (int i = 0; i < n; i++)
      {
        const double target = s.vf[i] + s.couple[i];
        s.mom[i] += (target - s.eff[i]) * S * dt;
        s.mom[i] *= std::exp(-D * dt);
        s.eff[i] += s.mom[i] * dt;
      }
    }
    double fc = 18000;
    if (p.rtone > 0.001) fc = 16000 * std::pow(2, -6 * p.rtone * s.R);
    else if (p.rtone < -0.001) fc = 16000 * std::pow(2, -6 * (-p.rtone) * (1 - s.R));
    fc = std::max(120.0, std::min(18000.0, fc));
    s.lpc = 1 - std::exp(-kTau * fc / sr);
    if (glideOn && firstTick) for (int i = 0; i < n; i++) s.fRun[i] = s.eff[i];  // snap the per-sample glide
  }

  double sr;

 public:
  // Gravity readout (per render call): ratio index into kRatios, octave
  // fold, live cents error. Display feed only — never read by the DSP.
  int gravCount = 0;
  int gravPairs[32][2] = {{0}};
  double gravErr[32] = {0};

 private:
  double x[kMaxV] = {0}, panL[kMaxV] = {0}, panR[kMaxV] = {0};
  double panBase[kMaxV] = {0};                              // ADR-064 signed base pan
  static constexpr int kItdRing = 256;   // 1.2 ms at 192 kHz fits
  int itdSamp[kMaxV] = {0};              // ADR-074 per-voice far-channel delay
  double apZ = 0;                        // ADR-074 mode D allpass state
  double lastNoteF = 0;                  // ADR-076: pitch the next note bends FROM
  // ADR-077: the ensemble's timing state persists ACROSS notes — the whole
  // point is that each note's offsets are corrected from the previous ones.
  double tOff[kMaxV] = {0};
  uint32_t tRng = 12345;
  double gaussT()
  {   // Box-Muller from the core's own stream; no wall clock, seeded, portable
    double u = rngNext(tRng); if (u < 1e-9) u = 1e-9;
    const double v = rngNext(tRng);
    return std::sqrt(-2 * std::log(u)) * std::cos(kTau * v);
  }
  // ADR-075 halfband decimator: 63-tap windowed sinc, cutoff 0.235 of the 2x
  // rate (~20.7 kHz). The spike measured -0.83 dB droop at 15 kHz at this
  // length vs -3.44 dB at 1x; longer taps buy only the 20 kHz corner, which
  // sits at 0.91x Nyquist inside any decimator's transition band and is
  // deliberately not chased (ADR-075's bounded claim).
  static constexpr int kHbTaps = 63;
  double hb[kHbTaps] = {0};
  double panEffV[kMaxV] = {0};                              // viz-only: motion-modulated seat
                                                            // (named panBase: rebuild has a local `pan`)
  double panPh[kMaxV] = {0}, panLm[kMaxV] = {0}, panRm[kMaxV] = {0};
  double panSweepPh = 0;
  double lastPhase[kMaxV] = {0};   // keep-phase snapshot of the focus swarm
  long noteCounter = 0;
  int tick = 0;
  int centerIdx = 0;
  double xmin = 0;  // lowest raw x, for the root anchor (ADR-068)
  bool tiltHP = false;  // tone-tilt sign (ADR-060), set each control tick
  uint32_t grng = 1;
  Voice voices[kPoly];
};

}  // namespace hypersaw
