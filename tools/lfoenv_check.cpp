/*
 * lfoenv_check — B171's oracle: the two LFOs and the two extra envelopes are
 * real modulation SOURCES with the timing, shapes, determinism and polarity
 * they claim. WIRED into `./verify full` (ADR-180 §1 inverted the default: a
 * new check is wired in the PR that creates it).
 * WIRED: ./verify full.
 *
 * WHAT IT GATES, section by section. Each section answers one acceptance
 * clause of the brief, and each carries a control that MUST read the other way
 * — the repo's standing lesson is that a probe here confirms whatever it
 * expects unless something in the same run is forced to disagree (L0032).
 *
 *   A  RATE, AND SAMPLE-RATE INDEPENDENCE. LFO 1 as a sine at 1 Hz completes
 *      one cycle in 1.000 s at 44.1/48/96 kHz, measured by counting zero
 *      up-crossings of the live source slot, tolerance one mod tick. CONTROL:
 *      at 4 Hz the same measurement reads four cycles, so the counter is
 *      measuring the LFO and not the render length.
 *   B  TEMPO SYNC. At 120 bpm, beats=1 gives 0.500 s per cycle (the delay's
 *      own law, `seconds = beats * 60/bpm`, read as a rate). CONTROL: beats=2
 *      halves the rate; and the FREE knob is ignored in sync mode.
 *   C  SHAPES. Over one cycle each shape's min/max/mean is what its name says:
 *      sine and triangle ±1 mean ~0; saw up rising with exactly ONE wrap per
 *      cycle (an arbitrary one-cycle window always holds one); square exactly
 *      two-valued; sample & hold piecewise-constant with a new value per wrap.
 *      CONTROLS: the square's value set has size 2 and the sine's does not, and
 *      the sine falls repeatedly where the saw drops once.
 *   D  DETERMINISM. Two instances at the same seed produce BIT-IDENTICAL S&H
 *      sequences. CONTROL (must differ): a different seed produces a different
 *      sequence — without it, "identical" would also be satisfied by an LFO
 *      that never drew at all.
 *   E  RETRIGGER. With retrig on, a note-on rewinds the phase to the Start
 *      Phase knob. CONTROL: with retrig OFF (free-running) the same note-on
 *      does NOT rewind it.
 *   E2 THE MOD PAGE'S PICTURE IS THE LIVE SOURCE (B177 note, 2026-09-21). The
 *      shell publishes each LFO's cycle for the GUI; this walks the live source
 *      slot phase by phase, on a grid where one mod tick is exactly one
 *      published point, and asserts they agree to the publisher's own precision.
 *      It is what stops the deleted JS shape law from being re-forked: the cure
 *      for B177's second copy was structural, and this is what keeps it so.
 *      CONTROLS: the same walk against ANOTHER shape's cycle fails; a different
 *      patch seed publishes a different S&H first step.
 *   F  ENV 3/4. THE ADSR KNOBS ARE ONE-POLE TIME CONSTANTS, not times-to-peak.
 *      That is ENV 2's law (ADR-135/162) and ENV 3/4 run the same one, so the
 *      level is 1-exp(-t/tau) and the stage snaps to exactly 1 once it passes
 *      0.99 — about 4.6 tau, NOT 1 tau. (A first pass at this section asserted
 *      "reaches 1 in the attack seconds" and failed at 0.91 against a correct
 *      envelope; the knob's meaning is the finding, not a bug.) Measured here
 *      at 7 tau with ==, not a tolerance, at 44.1/48/96 kHz; and a release
 *      snaps to EXACTLY 0.0, which is the part worth pinning because a
 *      one-pole only ever approaches. CONTROL: ENV 4 at a far longer attack
 *      has NOT reached 1 at that same instant, so "reaches 1" is not "the
 *      probe reads 1 always".
 *   G  THE CHUNK. A route from slot 18 and a route from slot 21 round-trip
 *      through the state chunk with their slots intact — the slot index is
 *      frozen patch data, and a source appended in the wrong place would
 *      re-aim every saved route silently.
 *   H  POLARITY, END TO END. An LFO on an AS-IS route drives a destination
 *      BELOW its base (it is bipolar); the same LFO on a UNIPOLAR route never
 *      goes below base. CONTROL: ENV 3 (unipolar) on an as-is route never goes
 *      below base either, so "below base" is a property of the source and not
 *      of every route.
 *   I  THE MUST-READ-ZERO CONTROL. Slot 22 is unassigned; it reads EXACTLY 0
 *      and a route from it leaves its destination at exactly base. If this
 *      ever reads non-zero, every "the LFO moved it" assertion above is
 *      measuring something other than the LFO.
 *
 * Reuses tools/notefuzz_scaffold.inc (the stub host, event lists, note/param
 * makers) rather than growing another copy of the CLAP scaffold.
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include <clap/clap.h>

#include "../src/hypersaw_clap_entry.h"
#include "../src/hypersaw_debug.h"

namespace
{
#include "notefuzz_scaffold.inc"

int g_failures = 0;
void check(bool ok, const std::string &what, const std::string &detail = "")
{
  std::printf("  %s  %s%s%s\n", ok ? "ok  " : "FAIL", what.c_str(),
              detail.empty() ? "" : "  —  ", detail.c_str());
  if (!ok) g_failures++;
}
std::string num(double v)
{
  char b[64];
  std::snprintf(b, sizeof b, "%.6g", v);
  return b;
}

/* B171 parameter ids, spelled once. */
enum : clap_id
{
  kSeed = 3,
  kLfo1Rate = 269, kLfo1Shape = 270, kLfo1Sync = 271, kLfo1Beats = 272,
  kLfo1Retrig = 273, kLfo1Phase = 274,
  kLfo2Rate = 275, kLfo2Shape = 276,
  kEnv3A = 281, kEnv3D = 282, kEnv3S = 283, kEnv3R = 284,
  kEnv4A = 285,
  kDetune = 4,      // a continuous, non-stepped destination every build exposes
  kInertia = 11,    // a second one, global — stepped destinations are refused
};
constexpr int kSlotLfo1 = 18, kSlotLfo2 = 19, kSlotEnv3 = 20, kSlotEnv4 = 21;
constexpr int kSlotUnassigned = 22;
// The shell's mod tick: 256 frames, the grid modStep() runs on. Every timing
// tolerance below is expressed in ticks, because the tick IS the resolution.
constexpr double kTickFrames = 256.0;

/* A plugin driven at an arbitrary sample rate, with the knobs and the clock a
   section needs. The block size is the mod tick, so one step() == one tick. */
struct Rig
{
  const clap_plugin_t *p = nullptr;
  double sr = kSR;
  std::vector<float> L, R;
  clap_audio_buffer_t out{};
  clap_process_t proc{};
  clap_event_transport_t transport{};
  float *ch[2];
  long blocks = 0;

  void boot(double rate = kSR, double bpm = 0)
  {
    sr = rate;
    auto *f = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    p = f->create_plugin(f, &kHost, "com.lifted-truck.hypersaw");
    p->init(p);
    p->activate(p, sr, 32, (uint32_t)kTickFrames);
    p->start_processing(p);
    L.assign((size_t)kTickFrames, 0);
    R.assign((size_t)kTickFrames, 0);
    ch[0] = L.data();
    ch[1] = R.data();
    out.data32 = ch;
    out.channel_count = 2;
    proc.frames_count = (uint32_t)kTickFrames;
    proc.audio_outputs = &out;
    proc.audio_outputs_count = 1;
    proc.out_events = &kOut;
    if (bpm > 0) setTempo(bpm);
  }
  void step(EvList &e)
  {
    e.finalize();
    proc.in_events = &e.list;
    p->process(p, &proc);
    blocks++;
  }
  void tick(int n = 1) { for (int i = 0; i < n; i++) { EvList e; step(e); } }
  void set(clap_id id, double v) { EvList e; e.params.push_back(mkParam(id, v)); step(e); }
  void note(int m, bool on)
  {
    EvList e;
    e.notes.push_back(mkNote(on ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF, 0, (int16_t)m,
                             -1, on ? 0.8 : 0));
    step(e);
  }
  /* The host's tempo, delivered on clap_process_t::transport — the door
     process() actually reads it through (`p->transport->tempo` -> core.p.bpm),
     so the sync branch under test is the shipped one. */
  void setTempo(double bpm)
  {
    transport.header.size = sizeof(transport);
    transport.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    transport.header.type = CLAP_EVENT_TRANSPORT;
    transport.flags = CLAP_TRANSPORT_HAS_TEMPO;
    transport.tempo = bpm;
    proc.transport = &transport;
  }
  /* The BASE of a destination — what readParam reports under ADR-136, which is
     deliberately NOT the applied value. Read through the CLAP params extension
     rather than through a new export: the base is already observable. */
  double base(clap_id id) const
  {
    auto *ext = (const clap_plugin_params_t *)p->get_extension(p, CLAP_EXT_PARAMS);
    double v = 0;
    return ext && ext->get_value(p, id, &v) ? v : -1e300;
  }
  /* Load a patch through the GUI's own JSON path, headless. `routes` is the
     canonical `src:dest:depth[:pol];` chunk — the route's polarity rides the
     chunk, so this needs no test hook of its own. The apply is QUEUED; the
     ticks let it land. */
  void patch(const std::string &params, const std::string &routes)
  {
    const std::string js = "{\"plugin\":\"HYPERSAW\",\"schema\":3,\"params\":{" + params +
                           "},\"modRoutes\":\"" + routes + "\"}";
    hypersaw_debug_apply(p, js.c_str());
    tick(4);
  }
  double src(int slot) const { return hypersaw_debug_modsrc(p, slot); }
  double secPerTick() const { return kTickFrames / sr; }
  void kill() { p->stop_processing(p); p->deactivate(p); p->destroy(p); }
};

/* Seconds per cycle, measured by counting UP-crossings of zero in the live
   source slot over `seconds` of ticks. Counting crossings (rather than timing
   one) averages the tick quantisation over the whole window, which is what
   makes a one-tick tolerance meaningful at 96 kHz as well as 44.1. */
double measureCycleSeconds(Rig &r, int slot, double seconds, int *cyclesOut = nullptr)
{
  const int ticks = (int)(seconds / r.secPerTick());
  double prev = r.src(slot);
  int cycles = 0;
  for (int i = 0; i < ticks; i++)
  {
    r.tick();
    const double v = r.src(slot);
    if (prev < 0.0 && v >= 0.0) cycles++;
    prev = v;
  }
  if (cyclesOut) *cyclesOut = cycles;
  return cycles > 0 ? (ticks * r.secPerTick()) / cycles : 0.0;
}

/* One cycle of `slot` sampled per tick, given the cycle length in seconds. */
std::vector<double> sampleCycle(Rig &r, int slot, double cycleSeconds)
{
  const int ticks = std::max(8, (int)(cycleSeconds / r.secPerTick()));
  std::vector<double> v;
  v.reserve((size_t)ticks);
  for (int i = 0; i < ticks; i++) { r.tick(); v.push_back(r.src(slot)); }
  return v;
}

/* ---- A: rate, and sample-rate independence ---------------------------- */
void sectionRate()
{
  std::printf("\n-- A. rate: 1 Hz is one cycle per second, at any sample rate --\n");
  for (double sr : {44100.0, 48000.0, 96000.0})
  {
    Rig r;
    r.boot(sr);
    r.set(kLfo1Rate, 1.0);
    const double got = measureCycleSeconds(r, kSlotLfo1, 6.0);
    const double tol = r.secPerTick();
    check(std::fabs(got - 1.0) <= tol,
          "1 Hz -> 1.000 s/cycle at " + num(sr) + " Hz",
          num(got) + " s (tol " + num(tol) + " = one tick)");
    r.kill();
  }
  // CONTROL: the counter must FOLLOW the rate, or section A measured the
  // render length and would read "1 cycle" for any LFO at all.
  Rig c;
  c.boot();
  c.set(kLfo1Rate, 4.0);
  int cycles = 0;
  measureCycleSeconds(c, kSlotLfo1, 3.0, &cycles);
  check(cycles >= 11 && cycles <= 13, "control: 4 Hz over 3 s reads ~12 cycles, not 3",
        std::to_string(cycles) + " cycles");
  c.kill();
}

/* ---- B: tempo sync ----------------------------------------------------- */
void sectionTempo()
{
  std::printf("\n-- B. tempo sync: beats * 60/bpm, the delay's own law --\n");
  Rig r;
  r.boot(kSR, 120.0);
  r.set(kLfo1Rate, 7.0);    // deliberately wrong: sync mode must ignore it
  r.set(kLfo1Sync, 1.0);
  r.set(kLfo1Beats, 1.0);
  const double got = measureCycleSeconds(r, kSlotLfo1, 6.0);
  check(std::fabs(got - 0.5) <= r.secPerTick(),
        "120 bpm, 1 beat -> 0.500 s/cycle (and the free rate knob is ignored)", num(got) + " s");
  r.set(kLfo1Beats, 2.0);
  const double got2 = measureCycleSeconds(r, kSlotLfo1, 8.0);
  check(std::fabs(got2 - 1.0) <= r.secPerTick(),
        "control: 2 beats -> 1.000 s/cycle (the beats knob is read, not ignored)",
        num(got2) + " s");
  r.kill();
}

/* ---- C: the shapes ----------------------------------------------------- */
void sectionShapes()
{
  std::printf("\n-- C. shapes: each is what its name says, over one cycle --\n");
  auto cycleOf = [](int shape) {
    Rig r;
    r.boot();
    r.set(kLfo1Shape, (double)shape);
    r.set(kLfo1Rate, 1.0);
    r.tick(2);
    auto v = sampleCycle(r, kSlotLfo1, 1.0);
    r.kill();
    return v;
  };
  auto stats = [](const std::vector<double> &v, double *lo, double *hi, double *mean) {
    *lo = 1e300; *hi = -1e300; double s = 0;
    for (double x : v) { *lo = std::min(*lo, x); *hi = std::max(*hi, x); s += x; }
    *mean = v.empty() ? 0 : s / (double)v.size();
  };
  double lo, hi, mean;

  const auto sine = cycleOf(0);
  stats(sine, &lo, &hi, &mean);
  check(hi > 0.99 && lo < -0.99 && std::fabs(mean) < 0.02, "sine spans ±1 with mean 0",
        "[" + num(lo) + ", " + num(hi) + "] mean " + num(mean));

  /* A one-cycle window taken at an arbitrary phase contains EXACTLY ONE wrap,
     so "monotone" is stated as "rising everywhere except one discontinuity".
     Asserting plain monotonicity is what a first pass here did, and it failed
     against a correct saw: the window boundary is not the cycle boundary. */
  const auto saw = cycleOf(2);
  int drops = 0;
  for (size_t i = 1; i < saw.size(); i++) if (saw[i] < saw[i - 1]) drops++;
  stats(saw, &lo, &hi, &mean);
  check(drops == 1 && hi > 0.9 && lo < -0.9,
        "saw up rises monotonically, with exactly ONE wrap per cycle, ±1",
        "[" + num(lo) + ", " + num(hi) + "] " + std::to_string(drops) + " drop(s)");
  int sineDrops = 0;
  for (size_t i = 1; i < sine.size(); i++) if (sine[i] < sine[i - 1]) sineDrops++;
  check(sineDrops > 10, "control: the sine over the same window falls repeatedly",
        std::to_string(sineDrops) + " drop(s)");

  const auto sq = cycleOf(4);
  std::set<double> sqVals(sq.begin(), sq.end());
  std::set<double> sineVals(sine.begin(), sine.end());
  check(sqVals.size() == 2 && sqVals.count(1.0) && sqVals.count(-1.0),
        "square takes exactly two values, +1 and -1",
        std::to_string(sqVals.size()) + " distinct");
  check(sineVals.size() > 8, "control: the sine over the same window is NOT two-valued",
        std::to_string(sineVals.size()) + " distinct");

  const auto tri = cycleOf(1);
  stats(tri, &lo, &hi, &mean);
  check(hi > 0.9 && lo < -0.9 && std::fabs(mean) < 0.05, "triangle spans ±1 with mean 0",
        "[" + num(lo) + ", " + num(hi) + "] mean " + num(mean));

  /* sample & hold: piecewise constant, one NEW value per wrap. At 1 Hz on a
     ~172 Hz tick a cycle holds ~172 identical samples, so the number of
     distinct values over four cycles must be about four, not four hundred. */
  Rig r;
  r.boot();
  r.set(kLfo1Shape, 5.0);
  r.set(kLfo1Rate, 1.0);
  r.tick(2);
  const auto sh = sampleCycle(r, kSlotLfo1, 4.0);
  r.kill();
  std::set<double> shVals(sh.begin(), sh.end());
  int changes = 0;
  for (size_t i = 1; i < sh.size(); i++) if (sh[i] != sh[i - 1]) changes++;
  check(changes >= 3 && changes <= 5,
        "sample & hold changes once per wrap (4 cycles -> ~4 steps)",
        std::to_string(changes) + " steps, " + std::to_string(shVals.size()) + " distinct");
  check((int)sh.size() > 4 * changes,
        "control: it is piecewise CONSTANT (far more samples than steps)",
        std::to_string(sh.size()) + " samples");
}

/* ---- D: determinism ----------------------------------------------------- */
void sectionDeterminism()
{
  std::printf("\n-- D. the S&H stream is seeded: same seed, same bits --\n");
  auto run = [](double seed) {
    Rig r;
    r.boot();
    r.set(kSeed, seed);
    r.set(kLfo1Shape, 5.0);
    r.set(kLfo1Rate, 8.0);
    r.tick(2);
    auto v = sampleCycle(r, kSlotLfo1, 3.0);
    r.kill();
    return v;
  };
  const auto a = run(1234), b = run(1234), c = run(4321);
  check(a == b, "two instances at seed 1234 produce BIT-IDENTICAL S&H values",
        std::to_string(a.size()) + " samples compared with ==");
  check(!a.empty() && a != c, "control (must differ): seed 4321 produces a different stream");
  // And the stream must actually have MOVED, or == would be satisfied by silence.
  std::set<double> vals(a.begin(), a.end());
  check(vals.size() > 3, "control: the compared stream is not a constant",
        std::to_string(vals.size()) + " distinct values");
}

/* ---- E: retrigger ------------------------------------------------------- */
void sectionRetrig()
{
  std::printf("\n-- E. retrig rewinds the phase; free-running does not --\n");
  /* A saw-up LFO makes phase directly observable: value = 2*phase - 1, so a
     rewind to Start Phase 0 is a jump to exactly -1. Read the shape, not an
     internal — the phase is not exported, and exporting it would be exporting
     the thing under test. */
  auto probe = [](int retrigMode) {
    Rig r;
    r.boot();
    r.set(kLfo1Shape, 2.0);       // saw up
    r.set(kLfo1Rate, 0.5);        // slow: half a cycle per 1 s
    r.set(kLfo1Phase, 0.0);
    r.set(kLfo1Retrig, (double)retrigMode);
    r.tick(150);                  // ~0.87 s in: well away from the start
    const double before = r.src(kSlotLfo1);
    r.note(60, true);
    const double after = r.src(kSlotLfo1);
    r.kill();
    return std::pair<double, double>(before, after);
  };
  const auto on = probe(1), off = probe(0);
  check(on.first > -0.5 && on.second < -0.9,
        "retrig on: a note-on rewinds to Start Phase (saw reads ~-1)",
        num(on.first) + " -> " + num(on.second));
  check(off.second > off.first - 0.05 && off.second > -0.5,
        "control: free-running does NOT rewind on the same note-on",
        num(off.first) + " -> " + num(off.second));
}

/* ---- E2: the PUBLISHED CYCLE is the sequence the source generates -------- */
void sectionPublishedCycle()
{
  std::printf("\n-- E2. the MOD page's LFO picture IS the live source (B177 note) --\n");
  /* WHY THIS SECTION EXISTS. B177 drew each LFO from a JS transcription of
     `lfoShapeAt` inside gui2.html: two copies of one law, nothing gating them,
     so a shape edit could have left the picture confidently wrong. The cure is
     structural — the shell publishes the cycle and the transcription is gone —
     but "one law" is a property of today's source, not an invariant, and the
     next author can re-fork it in one line. This pins the relationship the
     deletion bought: the published points must BE the values the mod tick
     puts in the source slot, walked phase by phase.

     THE PHASE GRID IS EXACT BY CONSTRUCTION. One block here is one mod tick,
     so with rate = sr / (tickFrames * N) the phase advances by exactly 1/N a
     tick, and after a retrigger to Start Phase 0 the k-th tick lands on the
     k-th published point. N is the publisher's own resolution (128 segments).

     DISCONTINUITIES ARE EXCLUDED, AND THE EXCLUSION IS DERIVED, NOT LISTED:
     any index where the PUBLISHED curve itself jumps more than 0.1 between
     neighbours is skipped, because a sub-ULP phase difference across the
     square's edge or the saw's wrap is a sign flip, not a disagreement. An
     exclusion nobody states is how a gate rots (the subdiv_check note), so the
     count of skipped indices is printed.

     CALIBRATED WITH TWO PLANTS IN THE PUBLISHER, and the second one is here
     because the first measured a boundary rather than the law (L0033):
       * publishing `lfoShapeAt(shape, k/N + 0.01, ...)` — a phase shift — fails
         shapes 0-3 at 0.0628 / 0.04 / 0.02 / 0.02 and DOES NOT FAIL THE SQUARE.
         That is the exclusion above doing exactly what it says: a square is
         two-valued, so the only index a small phase shift can move is the one
         straddling its edge, and that index is skipped. The square's row is
         therefore blind to phase, by construction, and this sentence is the
         record of it.
       * publishing `0.9 * lfoShapeAt(...)` — an amplitude scale — fails all
         five, square included, at ~0.1. Between them the rows are sensitive to
         both kinds of divergence a re-fork would introduce. */
  constexpr int N = 128;
  auto published = [](Rig &r, int lfoIndex, std::vector<double> &pts) {
    std::vector<char> buf(1 << 15);
    hypersaw_debug_lfocycle(r.p, buf.data(), (uint32_t)buf.size());
    const std::string j(buf.data());
    pts.clear();
    size_t at = 0;
    for (int k = 0; k <= lfoIndex; k++)      // the k-th "pts":[ is the k-th LFO
    {
      at = j.find("\"pts\":[", at);
      if (at == std::string::npos) return false;
      at += 7;
    }
    while (at < j.size() && j[at] != ']')
    {
      pts.push_back(std::atof(j.c_str() + at));
      const size_t comma = j.find(',', at), close = j.find(']', at);
      if (comma == std::string::npos || comma > close) break;
      at = comma + 1;
    }
    return pts.size() == (size_t)N + 1;
  };

  for (int shape : {0, 1, 2, 3, 4})          // S&H (5) is not a function of phase
  {
    Rig r;
    r.boot();
    r.set(kLfo1Shape, (double)shape);
    r.set(kLfo1Sync, 0.0);
    r.set(kLfo1Rate, kSR / (kTickFrames * (double)N));
    r.set(kLfo1Phase, 0.0);
    r.set(kLfo1Retrig, 1.0);
    std::vector<double> pts;
    if (!published(r, 0, pts))
    {
      check(false, "shape " + std::to_string(shape) + ": the publisher returned " +
                       std::to_string(pts.size()) + " points, want " + std::to_string(N + 1));
      r.kill();
      continue;
    }
    r.note(60, true);                        // retrig -> phase 0, then this tick advances 1/N
    double worst = 0;
    int checked = 0, skipped = 0;
    for (int k = 1; k < N; k++)
    {
      const double live = r.src(kSlotLfo1);
      const bool edge = std::fabs(pts[k] - pts[k - 1]) > 0.1 ||
                        std::fabs(pts[k + 1] - pts[k]) > 0.1;
      if (edge) skipped++;
      else { worst = std::max(worst, std::fabs(live - pts[k])); checked++; }
      r.tick();
    }
    /* 1e-7 is the publisher's own precision (%.7f), not a comfort margin: the
       two sides run the SAME function, so the only legal difference is the
       JSON's last digit. At the publisher's first %.5f this row read 5e-6 and
       failed, which is the check doing its job on the transport. */
    check(checked > 0 && worst < 1e-7,
          "shape " + std::to_string(shape) + ": every published point is the live source value",
          num(worst) + " worst over " + std::to_string(checked) + " phases (" +
              std::to_string(skipped) + " skipped at a jump)");
    r.kill();
  }

  /* CONTROL, and it is the one that matters: the same walk against ANOTHER
     shape's published cycle must disagree grossly. Without it the row above
     would also pass if both sides had quietly collapsed to a constant, or if
     `published` were reading a stale buffer that happened to be zeroed. */
  Rig a, b;
  a.boot(); b.boot();
  a.set(kLfo1Shape, 0.0);                    // sine, the walked instrument
  b.set(kLfo1Shape, 2.0);                    // saw up, the foreign picture
  a.set(kLfo1Sync, 0.0);
  a.set(kLfo1Rate, kSR / (kTickFrames * (double)N));
  a.set(kLfo1Phase, 0.0);
  a.set(kLfo1Retrig, 1.0);
  std::vector<double> foreign;
  const bool got = published(b, 0, foreign);
  a.note(60, true);
  double worstForeign = 0;
  if (got)
    for (int k = 1; k < N; k++)
    {
      worstForeign = std::max(worstForeign, std::fabs(a.src(kSlotLfo1) - foreign[k]));
      a.tick();
    }
  check(got && worstForeign > 0.5,
        "control: walked against the WRONG shape's cycle, the same comparison fails",
        num(worstForeign));
  a.kill(); b.kill();

  /* S&H's steps are published from the LFO's own seeded stream, so the
     picture's first step must be the value the first wrap actually holds.
     CONTROL: a different patch seed must move it — otherwise "the steps come
     from the seed" would also be satisfied by a hard-coded list. */
  auto firstStep = [&](double seed) {
    Rig r;
    r.boot();
    r.set(kSeed, seed);
    r.set(kLfo1Shape, 5.0);
    r.set(kLfo1Rate, 2.0);
    std::vector<char> buf(1 << 15);
    hypersaw_debug_lfocycle(r.p, buf.data(), (uint32_t)buf.size());
    const std::string j(buf.data());
    const size_t at = j.find("\"sh\":[");
    const double pub = at == std::string::npos ? 1e300 : std::atof(j.c_str() + at + 6);
    double live = 0;
    for (int k = 0; k < 400; k++) { r.tick(); live = r.src(kSlotLfo1); if (live != 0.0) break; }
    r.kill();
    return std::pair<double, double>(pub, live);
  };
  const auto s1 = firstStep(1.0), s2 = firstStep(2.0);
  check(std::fabs(s1.first - s1.second) < 1e-7,
        "S&H: the published first step is the value the first wrap holds",
        num(s1.first) + " vs live " + num(s1.second));
  check(std::fabs(s1.first - s2.first) > 1e-7,
        "control: a different patch seed publishes a different first step",
        num(s1.first) + " vs " + num(s2.first));
}

/* ---- F: ENV 3 / ENV 4 --------------------------------------------------- */
void sectionEnvelopes()
{
  std::printf("\n-- F. ENV 3/4: attack reaches EXACTLY 1, release snaps to EXACTLY 0 --\n");
  for (double sr : {44100.0, 48000.0, 96000.0})
  {
    Rig r;
    r.boot(sr);
    constexpr double kTau = 0.05;
    r.set(kEnv3A, kTau);
    r.set(kEnv3S, 1.0);           // hold at the top so "reached 1" is unambiguous
    r.set(kEnv4A, 1.5);           // the control: far slower than ENV 3's
    r.note(60, true);
    r.tick((int)(7.0 * kTau / r.secPerTick()));   // 7 tau: past the 4.6-tau snap
    const double e3 = r.src(kSlotEnv3), e4 = r.src(kSlotEnv4);
    check(e3 == 1.0, "ENV 3 reaches EXACTLY 1.0 by 7 attack-tau at " + num(sr) + " Hz",
          num(e3));
    check(e4 < 0.5, "control: ENV 4 at a 1.5 s attack has NOT (same instant, same probe)",
          num(e4));
    // Release: a one-pole only approaches zero, so the snap is asserted with ==.
    r.set(kEnv3R, 0.02);
    r.note(60, false);
    r.tick((int)(1.0 / r.secPerTick()));
    check(r.src(kSlotEnv3) == 0.0, "ENV 3 snaps to EXACTLY 0.0 after release at " + num(sr),
          num(r.src(kSlotEnv3)));
    r.kill();
  }
}

/* ---- G: the chunk ------------------------------------------------------- */
void sectionChunk()
{
  std::printf("\n-- G. a route from slot 18 and slot 21 survives the chunk --\n");
  Rig r;
  r.boot();
  check(hypersaw_test_mod_add(r.p, kSlotLfo1, kDetune), "route added on slot 18 (LFO 1)");
  // Inertia (11), not the Detune Law (5): stepped destinations are refused by
  // modAddRoute, so a check that asked for one would fail for a reason that has
  // nothing to do with slot 21.
  check(hypersaw_test_mod_add(r.p, kSlotEnv4, kInertia), "route added on slot 21 (ENV 4)");
  char blob[16384];
  hypersaw_debug_state(r.p, blob, sizeof blob);
  const std::string json = blob;
  r.kill();

  Rig q;
  q.boot();
  check(hypersaw_debug_apply(q.p, json.c_str()), "state applied to a fresh instance");
  q.tick(4);
  const std::string routes = hypersaw_debug_modroutes(q.p);
  std::printf("     %s\n", routes.c_str());
  check(routes.find("\"src\":18") != std::string::npos, "slot 18 came back as slot 18");
  check(routes.find("\"src\":21") != std::string::npos, "slot 21 came back as slot 21");
  q.kill();
}

/* ---- H: polarity, end to end -------------------------------------------- */
void sectionPolarity()
{
  std::printf("\n-- H. an LFO is BIPOLAR: an as-is route reaches below base --\n");
  /* The route's polarity rides the CHUNK (`src:dest:depth:pol`), so the whole
     section goes through the shipped load path and needs no test hook: 0 is
     as-is, 1 unipolar. Destination is Detune (id 4), continuous 0..1, base
     0.28 — the same destination polarity_check's own end-to-end section uses. */
  auto sweep = [](int slot, int pol) {
    Rig r;
    r.boot();
    char routes[64];
    std::snprintf(routes, sizeof routes, "%d:%u:0.25:%d;", slot, (unsigned)kDetune, pol);
    r.patch("\"lfo1Rate\":4,\"lfo1Shape\":0,\"env3S\":1,\"env3A\":0.01", routes);
    if (slot == kSlotEnv3) r.note(60, true);
    double lo = 1e300, hi = -1e300;
    for (int i = 0; i < 200; i++)
    {
      r.tick();
      const double v = hypersaw_test_mod_applied(r.p, kDetune);
      if (v < -1e299) continue;   // the dest has not been touched yet
      lo = std::min(lo, v);
      hi = std::max(hi, v);
    }
    const double b = r.base(kDetune);
    r.kill();
    return std::vector<double>{lo, hi, b};
  };
  const auto asis = sweep(kSlotLfo1, 0);
  check(asis[0] < asis[2] - 1e-9 && asis[1] > asis[2] + 1e-9,
        "as-is: the destination swings BOTH sides of base",
        "[" + num(asis[0]) + ", " + num(asis[1]) + "] base " + num(asis[2]));
  const auto uni = sweep(kSlotLfo1, 1);
  check(uni[0] >= uni[2] - 1e-9 && uni[1] > uni[2] + 1e-9,
        "unipolar route: the same LFO never goes below base",
        "[" + num(uni[0]) + ", " + num(uni[1]) + "] base " + num(uni[2]));
  const auto env = sweep(kSlotEnv3, 0);
  check(env[0] >= env[2] - 1e-9,
        "control: ENV 3 on an AS-IS route never goes below base (it is unipolar)",
        "[" + num(env[0]) + ", " + num(env[1]) + "] base " + num(env[2]));
}

/* ---- I: the must-read-zero control -------------------------------------- */
void sectionZero()
{
  std::printf("\n-- I. CONTROL: an unassigned slot reads EXACTLY 0 --\n");
  Rig r;
  r.boot();
  char routes[64];
  std::snprintf(routes, sizeof routes, "%d:%u:0.25;", kSlotUnassigned, (unsigned)kDetune);
  r.patch("\"lfo1Rate\":4", routes);
  const double base = r.base(kDetune);
  double lo = 1e300, hi = -1e300, srcMax = 0;
  for (int i = 0; i < 200; i++)
  {
    r.tick();
    srcMax = std::max(srcMax, std::fabs(r.src(kSlotUnassigned)));
    const double v = hypersaw_test_mod_applied(r.p, kDetune);
    if (v < -1e299) continue;
    lo = std::min(lo, v);
    hi = std::max(hi, v);
  }
  check(srcMax == 0.0, "slot 22 reads exactly 0 over 200 ticks", num(srcMax));
  check(lo == hi && lo == base, "a route from it leaves the destination at exactly base",
        num(lo) + " vs base " + num(base));
  // And the probe itself must refuse an out-of-range slot rather than read 0 —
  // 0 is a legitimate reading here, so a silent clamp would forge this very row.
  const double oob = r.src(99);
  check(std::isnan(oob), "control: the probe returns NaN past the table, not 0", num(oob));
  r.kill();
}

}  // namespace

int main()
{
  std::printf("lfoenv_check — B171 LFO 1/2 (slots 18/19) and ENV 3/4 (slots 20/21)\n");
  sectionRate();
  sectionTempo();
  sectionShapes();
  sectionDeterminism();
  sectionRetrig();
  sectionPublishedCycle();
  sectionEnvelopes();
  sectionChunk();
  sectionPolarity();
  sectionZero();
  std::printf("\n%s lfoenv_check: %d failure(s)\n", g_failures ? "FAIL" : "OK  ", g_failures);
  return g_failures ? 1 : 0;
}
