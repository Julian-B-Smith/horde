/*
 * measure_fx — computational-cost bench for the rack's FX modules. ROADMAP
 * B262 (human 2026-09-24: "Let's definitely run a cost bench, keeping in mind
 * that some FX haven't been created yet (we don't have a proper compressor or
 * multiband compressor or EQ yet, or real filters built, etc.)"). This is the
 * budget the corner-bus / patch-model question (B258/B263) needs: how many
 * FX racks a patch may run at once.
 *
 * UNWIRED: this is a timing measurement, not a pass/fail gate — same
 * contract as tools/measure_cpu.cpp and tools/measure_modsources.cpp (both
 * cited below): it prints, never judges, and carries no threshold to weaken.
 * It is also outside tools/test_table_check.py's census, which globs only
 * "_check.cpp"/"_check.py" in tools/ — this file is named measure_fx.cpp,
 * the same shape as measure_cpu.cpp/measure_alias.cpp/measure_modsources.cpp,
 * none of which the census sees either.
 *
 * WHAT THIS FILE MEASURES, IN C++ (the ROADMAP row's "MEASURED" half):
 *   - Each of the rack's nine module types (FxType Drive..Delay, fx_rack.h;
 *     kFxTypeLabels, hypersaw_clap.cpp) in one slot of a real hypersaw::FxRack
 *     (the shipped class, header-only — no CLAP link needed for this half),
 *     at 44.1 / 48 / 96 kHz, at DEFAULT settings and at the module's MOST
 *     EXPENSIVE reachable settings (see the per-module comments at each
 *     setup*() function below for what "worst" means and why, read out of
 *     fx_rack.h/time_core.h/delay_core.h/notch_core.h).
 *   - A full 4-slot rack at its worst-case module combination (the four
 *     individually most expensive types at their worst settings) and at a
 *     TYPICAL 4-slot combination (Drive -> Filter -> Comp -> Delay, all at
 *     default settings — a plausible ordinary chain), both at all three
 *     rates.
 *   - The engine's own baseline, for scale, by REUSING measure_cpu.cpp's
 *     OWN METHOD: this file links the same ${PROJECT_NAME}-impl library,
 *     drives the shipped plugin through the same CLAP factory, and times the
 *     same "8 held notes (keys 48+3k), 128-sample blocks" cell measure_cpu.cpp
 *     uses, at n=8 voices/note, single oscillator, 44.1 kHz — cited here as
 *     "measure_cpu's method: 8 held notes, 128-sample blocks, 44.1 kHz", not
 *     re-derived. This is the ONLY part of this file that links CLAP; the FX
 *     module cells below are FxRack-direct, matching the "reduce, never
 *     invent" instinct — the rack is the shipped object, so no shell is
 *     needed to reach it.
 *
 * WHAT THIS FILE DOES NOT MEASURE (the ROADMAP row's "PROJECTED" half — the
 * parametric EQ/compressor, multiband compressor, real filters, the reverb
 * FDN, MAW): none of those modules exist in src/fx_rack.h yet (grep
 * kFxTypeLabels — nine entries, Off..Delay, none of the five above). They are
 * estimated in the PR's trace from lab/reference operation counts and, where
 * a JS reference can be timed headlessly in Node, a JS->C++ ratio — that work
 * is scratch (Node has no CMake target here) and reported in prose, not this
 * binary, per the brief's "OUTPUT, in the trace and the PR body" split.
 *
 * CALIBRATION CONTROL. A fixed FLOP loop (`calibLoopMs`), same shape as
 * measure_cpu.cpp's refLoopMs() / measure_modsources.cpp's calibLoopMs(),
 * `volatile`-sunk, min-of-3, printed before the tables so a reader can see
 * the timer measuring real work rather than trust the ns/sample numbers on
 * faith.
 *
 * THE SINK. Every timed block's first sample is folded into a `volatile
 * double` accumulator that is printed at the end (`sinkTotal`) — the
 * optimiser cannot prove that value unobserved, so it cannot delete the
 * FxRack::processStereo() calls being timed.
 *
 * METHOD, per cell: a freshly constructed FxRack at the target sample rate,
 * one (or four) slot(s) configured per setup*(), a 128-sample stereo block
 * refilled from a PRECOMPUTED deterministic noise buffer (mulberry32, fixed
 * seed) before every call — precomputed so the RNG's own cost is not timed
 * inside the measured loop, and refilled (not reused in place) so a module
 * that processes in place (every FX slot does) is fed fresh representative
 * input every block rather than its own decaying output. 50 blocks of
 * untimed warm-up (filter/comp/comb transients settle), then 5 seconds of
 * audio-equivalent processed and timed, minimum of 3 reps. Release (-O3)
 * only — a -O0 number is meaningless here for the same reason global
 * CLAUDE.md's "Test in Release, never Debug" states for the plugin itself.
 *
 * Standalone in the sense that FxRack needs no plugin shell; only the one
 * engine-baseline cell links CLAP.
 */
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"
#include "../src/fx_rack.h"
#include "build_stamp.h"   // generated every build (CMake target)

namespace
{
#include "notefuzz_scaffold.inc"

using hypersaw::FxRack;
using hypersaw::FxType;
using Clock = std::chrono::steady_clock;

double elapsedMs(Clock::time_point a, Clock::time_point b)
{
  return std::chrono::duration<double>(b - a).count() * 1000.0;
}

/* ---------------------------------------------------------------- calibration
 * Same shape as measure_cpu.cpp's refLoopMs() / measure_modsources.cpp's
 * calibLoopMs(): a fixed, deterministic FLOP loop timed on THIS run, min of
 * 3, `volatile`-sunk. Not compared to a recorded quiet-machine figure (this
 * file does not judge, per the UNWIRED note above) — it exists so a reader
 * can see the timer measuring real work before trusting the numbers below.
 */
double calibLoopMs()
{
  double best = 1e9;
  for (int rep = 0; rep < 3; rep++)
  {
    volatile double sink = 0;
    const auto t0 = Clock::now();
    double acc = 0;
    for (long long i = 0; i < 50000000; i++)
      acc += std::sqrt((double)i + 1.0) * std::sin((double)i * 1e-6);
    sink = acc;
    (void)sink;
    const auto t1 = Clock::now();
    best = std::min(best, elapsedMs(t0, t1));
  }
  return best;
}

/* mulberry32, the repo's own deterministic stream (SPEC §5.7: no wall-clock
   reads; this file follows the same seeded-PRNG convention the DSP cores
   use, even though this bench's own audio-thread rule does not apply to a
   host-side timing tool). */
uint32_t mulberry32(uint32_t &s)
{
  s += 0x6D2B79F5u;
  uint32_t t = s;
  t = (t ^ (t >> 15)) * (t | 1u);
  t ^= t + (t ^ (t >> 7)) * (t | 61u);
  return t ^ (t >> 14);
}

constexpr int kBlk = 128;          // matches measure_cpu's E-6 block size
constexpr double kSeconds = 5.0;   // audio-equivalent, not wall time
constexpr int kReps = 3;
constexpr int kWarmupBlocks = 50;

volatile double sinkTotal = 0.0;   // the CANNOT-BE-ELIDED sink, printed at the end

struct NoiseBuf
{
  std::vector<float> data;
  explicit NoiseBuf(double amp, uint32_t seed = 0x9E3779B9u)
  {
    data.resize(kBlk);
    uint32_t s = seed;
    for (int i = 0; i < kBlk; i++)
    {
      const double u = (double)mulberry32(s) / 4294967296.0;   // [0,1)
      data[i] = (float)((u * 2.0 - 1.0) * amp);
    }
  }
};

struct Cell { double nsPerSample; double pct; };

/* Times a rack already configured by the caller's setup lambda. `amp` picks
   the deterministic noise buffer's peak — 0.5 for a representative "default"
   signal, 0.9 for "worst" (loud enough to keep Comp's gain-reduction branch
   and Delay/Drive's soft-limiter engaged every block, so a worst-case
   parameter setting is also exercised by a worst-case signal). */
template <typename Setup>
Cell timeRack(Setup setup, double sr, double amp)
{
  const NoiseBuf noise(amp);
  double bestNs = 1e18;
  for (int rep = 0; rep < kReps; rep++)
  {
    FxRack rack;
    rack.setSampleRate(sr);
    setup(rack);
    std::vector<float> L(kBlk), R(kBlk);
    auto refill = [&]() { std::copy(noise.data.begin(), noise.data.end(), L.begin());
                          std::copy(noise.data.begin(), noise.data.end(), R.begin()); };
    for (int b = 0; b < kWarmupBlocks; b++) { refill(); rack.processStereo(L.data(), R.data(), kBlk); }
    const long long totalSamples = (long long)(kSeconds * sr);
    const int NB = (int)(totalSamples / kBlk);
    const auto t0 = Clock::now();
    for (int b = 0; b < NB; b++)
    {
      refill();
      rack.processStereo(L.data(), R.data(), kBlk);
      sinkTotal += (double)L[0] + (double)R[kBlk - 1];   // folds real output into an observed global
    }
    const auto t1 = Clock::now();
    const double elapsedS = std::chrono::duration<double>(t1 - t0).count();
    const double ns = elapsedS * 1e9 / ((double)NB * kBlk);
    bestNs = std::min(bestNs, ns);
  }
  const double pct = bestNs * sr / 1e9 * 100.0;   // ns/sample * samples/s of audio -> fraction of a core, as %
  return {bestNs, pct};
}

/* ---- per-module setup: "default" and "worst", read out of the code ------- *
 * Each function documents what it read to decide "worst", because the rack
 * does not expose every knob a core has (Notch's band count, for one — see
 * setupNotch below).
 */

// Drive: dry/wet tanh, amount 0..1 = pre-gain 1x..4x into tanh. Branch-free
// per sample either way; "worst" is the heaviest pre-gain (amount 1).
void setupDrive(FxRack &r, bool worst) { r.setType(0, (int)FxType::Drive); r.setAmount(0, worst ? 1.0 : 0.5); }

// Filter: one-pole LP, amount 0..1 = coefficient only. Branch-free; "worst"
// is nominal (amount 1, heaviest cutoff) since cost does not vary with it.
void setupFilter(FxRack &r, bool worst) { r.setType(0, (int)FxType::Filter); r.setAmount(0, worst ? 1.0 : 0.5); }

// Gain: one multiply/sample/channel, cost invariant to amount.
void setupGain(FxRack &r, bool worst) { r.setType(0, (int)FxType::Gain); r.setAmount(0, worst ? 1.0 : 0.5); }

// Comp (fx_rack.h renderWet, FxType::Comp): the gain-reduction branch
// (`amt > 0.005 && compEnv > 0.4`) and the brickwall branch
// (`pk > 0.98`) are both conditional; "worst" is amount 1 fed a loud signal
// (amp=0.9 at the call site) so BOTH branches are taken every sample, which
// a quiet/low-amount signal would skip.
void setupComp(FxRack &r, bool worst) { r.setType(0, (int)FxType::Comp); r.setAmount(0, worst ? 1.0 : 0.5); }

// Comb (fx_rack.h renderWet, FxType::Comb; kCombLines=8): the per-sample cost
// is `for (auto &c : combs) if (c.key>=0) ...` — O(active lines), NOT O(1).
// "worst" is all 8 lines held (a full chord); "default" is a single sustained
// note, since the rack's own comment calls it "the lab's per-voice comb
// re-hosted bus-side" — one voice is the minimal live case, not zero.
// Frequencies span the swarm's playable range (>= 20 Hz per the core's own
// buffer-sizing comment, setSampleRate) so no line clamps to the buffer's
// floor delay identically to another.
void setupComb(FxRack &r, bool worst)
{
  r.setType(0, (int)FxType::Comb);
  r.setAmount(0, worst ? 1.0 : 0.5);
  r.setTone(0, 0.5);
  static const double freqs[8] = {110.0, 146.8, 196.0, 220.0, 293.7, 349.2, 440.0, 523.3};
  const int lines = worst ? 8 : 1;
  for (int i = 0; i < lines; i++) r.noteOn(60 + i, freqs[i]);
}

// Notch (notch_core.h via fx_rack.h's Notch case): the rack drives ONLY
// `mix` (= amount) through to the core — band count (p.nb, default 6 of a
// possible kNBMax=12) and feedback (p.feedback, default 0.4) stay at the
// core's OWN construction defaults, per fx_rack.h's comment at the Notch enum
// entry ("population/topology stay at core defaults until the rack grows a
// per-slot param page"). So there is NO rack-reachable "worst" distinct from
// default for Notch today — both cells below run nb=6. Recorded, not
// invented: this is what the shipped rack can actually ask the core to do.
void setupNotch(FxRack &r, bool worst) { r.setType(0, (int)FxType::Notch); r.setAmount(0, worst ? 1.0 : 0.5); }

// Echo (time_core.h mode 0 via fx_rack.h's ADR-131 setTimeParam, key 2 =
// "nb"/taps, CLAP param range [2,12] — hypersaw_clap.cpp fx1taps: {202,
// "fx1taps", ..., 2, 12, 8, ...}). "worst" is taps=12 (kNBMax, time_core.h) —
// the per-sample cost is O(nb) (two O(n) loops in TimeCore::processSample,
// mode 0). regen (amount) does not change the loop count, only saturation
// input, so amount=1 for "worst" is included for signal realism, not cost.
void setupEcho(FxRack &r, bool worst)
{
  r.setType(0, (int)FxType::Echo);
  r.setTimeParam(0, 2, worst ? 12 : 8);   // taps/lines
  r.setTimeParam(0, 0, 0.55);             // size (default)
  r.setTimeParam(0, 1, 0.6);              // spread (default)
  r.setTimeParam(0, 3, 0.4);              // damp (default)
  r.setTimeParam(0, 4, 0.2);              // noise (default; parity-neutral, cost-neutral)
  r.setTimeParam(0, 5, 0.7);              // stereo (default)
  r.setTimeParam(0, 6, 1.0);              // dist (default)
  r.setAmount(0, worst ? 1.0 : 0.5);      // regen
}

// Room (time_core.h mode 1): same taps knob, same O(nb) argument, plus the
// FDN mix step (also O(nb)) — worst is again taps=12.
void setupRoom(FxRack &r, bool worst)
{
  r.setType(0, (int)FxType::Room);
  r.setTimeParam(0, 2, worst ? 12 : 8);
  r.setTimeParam(0, 0, 0.55);
  r.setTimeParam(0, 1, 0.6);
  r.setTimeParam(0, 3, 0.4);
  r.setTimeParam(0, 4, 0.2);
  r.setTimeParam(0, 5, 0.7);
  r.setTimeParam(0, 6, 1.0);
  r.setAmount(0, worst ? 1.0 : 0.5);
}

// Delay (delay_core.h via fx_rack.h's Delay case): processStereo's cost is
// dominated by two ALWAYS-COMPUTED read-lerps plus two conditional one-poles
// gated on `damp > 1e-6` (useLp) and `loopHp > 1e-6` (useHp). The class's OWN
// defaults (damp=0.35, loopHp=60, delay_core.h:63-64) already have BOTH
// branches on — turning them off would be CHEAPER, not more expensive — so
// there is no rack-reachable setting that costs more than default. "worst"
// below sets the same damp/loopHp explicitly (documenting that fact) and
// only differs from "default" in `amount` (-> feedback) and crossfeed,
// neither of which changes the per-sample operation count. Recorded rather
// than invented, same as Notch above.
void setupDelay(FxRack &r, bool worst)
{
  r.setType(0, (int)FxType::Delay);
  r.setDelayParam(0, 0, 375.0);   // timeMs (default)
  r.setDelayParam(0, 1, 0.0);     // sync off (default)
  r.setDelayParam(0, 3, 1.0);     // offsetR (default)
  r.setDelayParam(0, 5, worst ? 1.0 : 0.0);   // crossfeed (cost-neutral; included for signal realism)
  r.setDelayParam(0, 6, 0.35);    // damp (default; already the "useLp" branch on)
  r.setDelayParam(0, 7, 60.0);    // loopHp (default; already the "useHp" branch on)
  r.setAmount(0, worst ? 1.0 : 0.5);          // feedback
}

struct ModuleSpec
{
  const char *label;
  void (*setup)(FxRack &, bool);
};

const ModuleSpec kModules[] = {
    {"Drive",  setupDrive},
    {"Filter", setupFilter},
    {"Gain",   setupGain},
    {"Comp",   setupComp},
    {"Comb",   setupComb},
    {"Notch",  setupNotch},
    {"Echo",   setupEcho},
    {"Room",   setupRoom},
    {"Delay",  setupDelay},
};
constexpr int kNumModules = sizeof(kModules) / sizeof(kModules[0]);

// The typical 4-slot rack: a plausible ordinary chain, all slots at DEFAULT
// settings (Drive -> Filter -> Comp -> Delay).
void setupTypicalRack(FxRack &r)
{
  r.setType(0, (int)FxType::Drive);  r.setAmount(0, 0.5);
  r.setType(1, (int)FxType::Filter); r.setAmount(1, 0.5);
  r.setType(2, (int)FxType::Comp);   r.setAmount(2, 0.5);
  r.setType(3, (int)FxType::Delay);
  r.setDelayParam(3, 0, 375.0); r.setDelayParam(3, 3, 1.0);
  r.setDelayParam(3, 6, 0.35);  r.setDelayParam(3, 7, 60.0);
  r.setAmount(3, 0.5);
}

// The worst 4-slot rack's module choice: filled in main() with the four
// individually most expensive module types at their worst settings, once
// those are measured (so the choice is evidence-driven, not guessed). The
// setup*() helpers above all hardcode slot 0 (needed for the single-slot
// cells), so the 4-slot worst rack is built directly in main() by replaying
// each picked module's worst-setting calls against its own slot rather than
// calling kModules[...].setup(), which would overwrite slot 0 four times.
struct WorstRackPick { int idx[4]; };

/* ---------------------------------------------------------------- engine baseline
 * REUSES measure_cpu.cpp's OWN METHOD (its header, and the pattern cited in
 * this file's header comment): the shipped plugin through the CLAP factory,
 * 8 held notes (keys 48+3k), 128-sample blocks, 44.1 kHz, swarm size n=8,
 * single oscillator — one of measure_cpu.cpp's own five audited cells
 * ("1 osc, n=8"). Not re-measuring a NEW scenario; citing the same one for
 * direct comparison against the FX numbers above, from the same build.
 */
double engineBaselinePct()
{
  hypersaw_entry_init("");
  auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  double best = 1e9;
  for (int rep = 0; rep < kReps; rep++)
  {
    const clap_plugin_t *p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
    p->init(p); p->activate(p, kSR, 32, kBlock); p->start_processing(p);
    std::vector<float> L(kBlock), R(kBlock);
    float *chans[2] = {L.data(), R.data()};
    clap_audio_buffer_t out{}; out.data32 = chans; out.channel_count = 2;
    clap_process_t proc{}; proc.frames_count = kBlock; proc.audio_outputs = &out;
    proc.audio_outputs_count = 1; proc.out_events = &kOut;
    auto once = [&](EvList &e) { e.finalize(); proc.in_events = &e.list; p->process(p, &proc); };
    {
      EvList e;
      e.params.push_back(mkParam(1, 8));      // swarm size n=8, osc 1
      e.params.push_back(mkParam(1001, 8));   // osc 2's own stride (kept off)
      once(e);
    }
    for (int k = 0; k < 8; k++)
    { EvList e; e.notes.push_back(mkNote(CLAP_EVENT_NOTE_ON, 0, (int16_t)(48 + k * 3), k + 1)); once(e); }
    for (int b = 0; b < 200; b++) { EvList e; once(e); }   // warm-up, untimed
    const int NB = (int)(kSeconds * kSR / kBlock);
    const auto t0 = Clock::now();
    for (int b = 0; b < NB; b++) { EvList e; once(e); }
    const auto t1 = Clock::now();
    const double cpu = std::chrono::duration<double>(t1 - t0).count();
    const double audio = NB * (double)kBlock / kSR;
    best = std::min(best, cpu / audio);
    p->stop_processing(p); p->deactivate(p); p->destroy(p);
  }
  hypersaw_entry_deinit();
  return best * 100.0;
}

}  // namespace

int main()
{
  const double calib = calibLoopMs();
  std::printf("measure_fx — build %s\n", HYPERSAW_BUILD_STAMP);
  std::printf("calibration: fixed FLOP loop, min of 3 = %.2f ms (proves the timer sees real work)\n\n",
              calib);

  const double rates[3] = {44100.0, 48000.0, 96000.0};

  // ---- per-module table -----------------------------------------------
  std::printf("## Per-module cost, one slot, stereo\n\n");
  std::printf("| module | 44.1k default (ns/smp, %%core) | 44.1k worst | 48k default | 48k worst | 96k default | 96k worst |\n");
  std::printf("|---|---|---|---|---|---|---|\n");
  double worstPctAt44k[kNumModules];
  for (int m = 0; m < kNumModules; m++)
  {
    std::printf("| %s ", kModules[m].label);
    for (int r = 0; r < 3; r++)
    {
      const double sr = rates[r];
      const Cell def = timeRack([&](FxRack &rack) { kModules[m].setup(rack, false); }, sr, 0.5);
      const Cell wst = timeRack([&](FxRack &rack) { kModules[m].setup(rack, true); }, sr, 0.9);
      if (r == 0) worstPctAt44k[m] = wst.pct;
      std::printf("| %.1f ns, %.4f%% | %.1f ns, %.4f%% ", def.nsPerSample, def.pct, wst.nsPerSample, wst.pct);
    }
    std::printf("|\n");
  }

  // Pick the four individually most expensive modules (by worst-case %CPU at
  // 44.1 kHz) as the "worst" 4-slot rack, so the combination is
  // evidence-driven rather than an a-priori guess.
  int order[kNumModules];
  for (int i = 0; i < kNumModules; i++) order[i] = i;
  std::sort(order, order + kNumModules, [&](int a, int b) { return worstPctAt44k[a] > worstPctAt44k[b]; });
  WorstRackPick pick{{order[0], order[1], order[2], order[3]}};
  std::printf("\nWorst 4-slot rack picked by measurement: %s, %s, %s, %s (highest worst-case %%CPU at 44.1 kHz)\n",
              kModules[pick.idx[0]].label, kModules[pick.idx[1]].label,
              kModules[pick.idx[2]].label, kModules[pick.idx[3]].label);

  // ---- full-rack table --------------------------------------------------
  std::printf("\n## Full 4-slot rack\n\n");
  std::printf("| rack | 44.1 kHz | 48 kHz | 96 kHz |\n|---|---|---|---|\n");
  double typicalPct44k = 0, worstRackPct44k = 0;
  {
    std::printf("| typical (Drive->Filter->Comp->Delay, default) ");
    for (int r = 0; r < 3; r++)
    {
      const Cell c = timeRack([&](FxRack &rack) { setupTypicalRack(rack); }, rates[r], 0.5);
      if (r == 0) typicalPct44k = c.pct;
      std::printf("| %.1f ns, %.4f%% ", c.nsPerSample, c.pct);
    }
    std::printf("|\n");
  }
  {
    std::printf("| worst (4 costliest modules, worst settings) ");
    for (int r = 0; r < 3; r++)
    {
      FxRack rack; rack.setSampleRate(rates[r]);
      // Slot 0..3: the four costliest module types, each at ITS OWN worst
      // settings, replicated from the per-module setup*() bodies above
      // (kept in one place there; duplicated here only because those
      // functions hardcode slot 0 for the single-slot cells).
      for (int slot = 0; slot < 4; slot++)
      {
        const int mi = pick.idx[slot];
        const char *label = kModules[mi].label;
        if (std::strcmp(label, "Drive") == 0) { rack.setType(slot, (int)FxType::Drive); rack.setAmount(slot, 1.0); }
        else if (std::strcmp(label, "Filter") == 0) { rack.setType(slot, (int)FxType::Filter); rack.setAmount(slot, 1.0); }
        else if (std::strcmp(label, "Gain") == 0) { rack.setType(slot, (int)FxType::Gain); rack.setAmount(slot, 1.0); }
        else if (std::strcmp(label, "Comp") == 0) { rack.setType(slot, (int)FxType::Comp); rack.setAmount(slot, 1.0); }
        else if (std::strcmp(label, "Comb") == 0)
        {
          rack.setType(slot, (int)FxType::Comb); rack.setAmount(slot, 1.0); rack.setTone(slot, 0.5);
          static const double freqs[8] = {110.0, 146.8, 196.0, 220.0, 293.7, 349.2, 440.0, 523.3};
          for (int i = 0; i < 8; i++) rack.noteOn(60 + i, freqs[i]);
        }
        else if (std::strcmp(label, "Notch") == 0) { rack.setType(slot, (int)FxType::Notch); rack.setAmount(slot, 1.0); }
        else if (std::strcmp(label, "Echo") == 0 || std::strcmp(label, "Room") == 0)
        {
          rack.setType(slot, std::strcmp(label, "Echo") == 0 ? (int)FxType::Echo : (int)FxType::Room);
          rack.setTimeParam(slot, 2, 12); rack.setTimeParam(slot, 0, 0.55); rack.setTimeParam(slot, 1, 0.6);
          rack.setTimeParam(slot, 3, 0.4); rack.setTimeParam(slot, 4, 0.2); rack.setTimeParam(slot, 5, 0.7);
          rack.setTimeParam(slot, 6, 1.0); rack.setAmount(slot, 1.0);
        }
        else if (std::strcmp(label, "Delay") == 0)
        {
          rack.setType(slot, (int)FxType::Delay);
          rack.setDelayParam(slot, 0, 375.0); rack.setDelayParam(slot, 3, 1.0);
          rack.setDelayParam(slot, 5, 1.0); rack.setDelayParam(slot, 6, 0.35); rack.setDelayParam(slot, 7, 60.0);
          rack.setAmount(slot, 1.0);
        }
      }
      const NoiseBuf noise(0.9);
      std::vector<float> L(kBlk), R(kBlk);
      auto refill = [&]() { std::copy(noise.data.begin(), noise.data.end(), L.begin());
                            std::copy(noise.data.begin(), noise.data.end(), R.begin()); };
      double bestNs = 1e18;
      for (int rep = 0; rep < kReps; rep++)
      {
        for (int b = 0; b < kWarmupBlocks; b++) { refill(); rack.processStereo(L.data(), R.data(), kBlk); }
        const long long totalSamples = (long long)(kSeconds * rates[r]);
        const int NB = (int)(totalSamples / kBlk);
        const auto t0 = Clock::now();
        for (int b = 0; b < NB; b++) { refill(); rack.processStereo(L.data(), R.data(), kBlk); sinkTotal += L[0]; }
        const auto t1 = Clock::now();
        const double elapsedS = std::chrono::duration<double>(t1 - t0).count();
        bestNs = std::min(bestNs, elapsedS * 1e9 / ((double)NB * kBlk));
      }
      const double pct = bestNs * rates[r] / 1e9 * 100.0;
      if (r == 0) worstRackPct44k = pct;
      std::printf("| %.1f ns, %.4f%% ", bestNs, pct);
    }
    std::printf("|\n");
  }

  // ---- engine baseline ---------------------------------------------------
  const double enginePct = engineBaselinePct();
  std::printf("\n## Engine baseline (measure_cpu's method: 8 held notes, 128-sample blocks, "
              "44.1 kHz, n=8 voices/note, 1 osc)\n\n%.4f%% of one core\n", enginePct);

  // ---- budget table (44.1 kHz; linear projection to 2/4 racks — the shipped
  //      code has exactly ONE FxRack instance per voice chain today, so 2/4
  //      racks is a projection for the corner-bus patch model B263 is
  //      deciding, not a measurement of code that exists) --------------------
  std::printf("\n## Budget: engine + N racks, 44.1 kHz (racks beyond 1 are a LINEAR "
              "PROJECTION from the single measured rack — no multi-rack code exists yet)\n\n");
  std::printf("| config | typical %%core | worst %%core |\n|---|---|---|\n");
  std::printf("| engine only | %.4f%% | %.4f%% |\n", enginePct, enginePct);
  for (int n = 1; n <= 4; n *= 2)
  {
    std::printf("| engine + %d rack%s | %.4f%% | %.4f%% |\n", n, n == 1 ? "" : "s",
                enginePct + n * typicalPct44k, enginePct + n * worstRackPct44k);
  }

  std::printf("\nsink (ignore; proves the loop was not eliminated): %.6e\n", sinkTotal);
  return 0;
}
