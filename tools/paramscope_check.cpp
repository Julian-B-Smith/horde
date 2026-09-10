/* paramscope_check — a parameter's SCOPE must be what it claims.
 *
 * Two assertions, and neither is meaningful without the other:
 *
 *   GLOBAL params reach EVERY oscillator. `oscOfId()` returns 0 for every
 *   global id, so `cores[oscOfId(id)].setParam(...)` wrote them into
 *   oscillator 0 and nowhere else. Measured before the fix: with the Attack
 *   knob at 1.5 s, oscillator 1 reached 90% at 0.955 s while oscillator 2 sat
 *   at 0.007 s — its compiled-in default. Every global core param behaved that
 *   way, so a two-oscillator patch was half-configured and the second half
 *   silently ignored the panel. Third instance of L0028's shape, after the
 *   note/lifecycle fan-out and pan motion.
 *
 *   PER-OSCILLATOR params reach ONLY their own. This is the vacuity control:
 *   "fan everything out to everything" would satisfy the first assertion
 *   perfectly and destroy per-oscillator addressing, and a suite that only
 *   tested the first would go green on it.
 *
 * Detection is by emitted audio through the public CLAP surface — no state
 * reads, because a round-trip through one broken accessor agrees with itself
 * (the state_check trap).
 *
 * EVERY MEASUREMENT GETS A FRESH PLUGIN INSTANCE. `plug_reset()` clears gates
 * and MPE bend; it does NOT restore parameter values or core internals, so
 * scenarios run back-to-back in one instance contaminate each other. Measured:
 * this exact per-oscillator assertion read 0.955 s standalone and 0.034 s when
 * four unrelated renders ran first. Re-ordering made it pass, which is luck —
 * the next assertion added would re-break it. A fresh instance makes the suite
 * order-independent by construction rather than by arrangement.
 */
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#include "../src/hypersaw_clap_entry.h"

static const clap_host_t kHost = {CLAP_VERSION_INIT, nullptr, "paramscope_check", "-", "-", "1.0",
                                  [](const clap_host_t *, const char *) -> const void * { return nullptr; },
                                  [](const clap_host_t *) {}, [](const clap_host_t *) {},
                                  [](const clap_host_t *) {}};

struct EvList
{
  clap_input_events_t in{};
  std::vector<const clap_event_header_t *> ev;
};
static uint32_t evSize(const clap_input_events_t *l) { return (uint32_t)((EvList *)l->ctx)->ev.size(); }
static const clap_event_header_t *evGet(const clap_input_events_t *l, uint32_t i)
{
  return ((EvList *)l->ctx)->ev[i];
}
static bool outPush(const clap_output_events_t *, const clap_event_header_t *) { return true; }

static int failures = 0;
static void check(bool ok, const char *what, const char *detail)
{
  std::printf("%-6s %s  (%s)\n", ok ? "OK" : "FAIL", what, detail);
  if (!ok) failures++;
}

static constexpr double kPi = 3.14159265358979323846;   // M_PI is absent under MSVC
static double goertzel(const std::vector<float> &x, double freq, double sr)
{
  const double w = 2.0 * kPi * freq / sr;
  const double c = 2.0 * std::cos(w);
  double s1 = 0, s2 = 0;
  for (float v : x) { const double s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
  const double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w);
  return 2.0 * std::sqrt(re * re + im * im) / (double)x.size();
}

/* ---- THE TRUTH SWEEPS (2026-08-21, human mandate: "start from basics").
   The enable-button saga ran three GUI patches deep before the real defect
   surfaced: readParam(1150) indexed shell state with the BASE id (d->id/1000
   = 0), so oscillator 2's readback mirrored oscillator 1 forever. Every layer
   above told the truth about a lie below it. These sweeps make that CLASS of
   defect unshippable:

   1. DEFAULT TRUTH -- fresh instance: get_value(id) == param_info default for
      EVERY id, bases and twins. The representation IS the engine at rest.
   2. ROUND-TRIP TRUTH -- set every id to a distinct in-range value; readback
      must return it (stepped: floored). Any split between the apply chain and
      the read chain -- the exact aliasing above -- fires here.

   Future modules walk through both by construction: the sweeps enumerate the
   plugin's own param_info, so there is nothing to remember to add. */
static void truthSweeps(const clap_plugin_factory_t *factory)
{
  const clap_plugin_t *p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
  p->init(p);
  p->activate(p, 44100.0, 32, 256);
  p->start_processing(p);
  auto *par = (const clap_plugin_params_t *)p->get_extension(p, CLAP_EXT_PARAMS);
  clap_output_events_t outEv{nullptr, outPush};
  const uint32_t n = par->count(p);
  int defBad = 0;
  for (uint32_t i = 0; i < n; i++)
  {
    clap_param_info_t info{};
    if (!par->get_info(p, i, &info)) continue;
    double v = 0;
    par->get_value(p, info.id, &v);
    if (std::fabs(v - info.default_value) > 1e-9)
    {
      if (defBad < 6)
        std::printf("  DEFAULT LIE id %u (%s): info %.6g, readback %.6g\n",
                    info.id, info.name, info.default_value, v);
      defBad++;
    }
  }
  char d1[64];
  std::snprintf(d1, sizeof(d1), "%u ids, %d lie(s)", n, defBad);
  check(defBad == 0, "fresh instance: every readback equals its declared default", d1);

  EvList evs;
  evs.in.ctx = &evs; evs.in.size = evSize; evs.in.get = evGet;
  std::vector<clap_event_param_value_t> pool;
  pool.reserve(n);
  std::vector<std::pair<clap_id, double>> want;
  for (uint32_t i = 0; i < n; i++)
  {
    clap_param_info_t info{};
    if (!par->get_info(p, i, &info)) continue;
    // Grid-snapped by design (their own gates cover the snap): 23 beatMult,
    // 148 bendQTimeSync. An exemption here is a CLAIM with a reason, never a
    // silence -- anything else that snaps must either be declared stepped or
    // earn its line here.
    if ((info.id % 1000) == 23 || (info.id % 1000) == 148) continue;
    double v = info.min_value + (info.max_value - info.min_value) * 0.37;
    if (info.flags & CLAP_PARAM_IS_STEPPED) v = std::floor(v);
    want.push_back({info.id, v});
    clap_event_param_value_t ev{};
    ev.header.size = sizeof(ev);
    ev.header.type = CLAP_EVENT_PARAM_VALUE;
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.param_id = info.id; ev.note_id = -1; ev.port_index = -1;
    ev.channel = -1; ev.key = -1; ev.value = v;
    pool.push_back(ev);
  }
  for (auto &e : pool) evs.ev.push_back(&e.header);
  par->flush(p, &evs.in, &outEv);
  int rtBad = 0;
  for (auto &w : want)
  {
    double v = 0;
    par->get_value(p, w.first, &v);
    if (std::fabs(v - w.second) > 1e-6)
    {
      if (rtBad < 6)
        std::printf("  ROUND-TRIP LIE id %u: set %.6g, read %.6g\n", w.first, w.second, v);
      rtBad++;
    }
  }
  char d2[64];
  std::snprintf(d2, sizeof(d2), "%zu ids, %d lie(s)", want.size(), rtBad);
  check(rtBad == 0, "round trip: every id returns the value it was set to", d2);
  p->stop_processing(p);
  p->deactivate(p);
  p->destroy(p);
}

int main()
{
  const double SR = 44100.0;
  const int BLK = 512;
  const uint32_t S = 1000;                       // ADR-082 oscillator id stride
  auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_t *p = nullptr;
  auto freshPlugin = [&]() {
    if (p) { p->stop_processing(p); p->deactivate(p); p->destroy(p); }
    truthSweeps(factory);
    p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
    p->init(p);
    p->activate(p, SR, 32, 2048);
    p->start_processing(p);
  };

  std::vector<float> L(BLK), R(BLK);
  float *chans[2] = {L.data(), R.data()};
  clap_audio_buffer_t out{};
  out.data32 = chans; out.channel_count = 2;
  EvList evl;
  evl.in.ctx = &evl; evl.in.size = evSize; evl.in.get = evGet;
  clap_output_events_t outEv{nullptr, outPush};

  // Params are queued into a stable store: the event list holds raw pointers,
  // so a reallocating vector would dangle mid-block.
  // Reserved generously AND cleared per render below. The event list holds raw
  // pointers into this vector, so a reallocation dangles them — and this probe
  // makes ~70 param events across its renders, which silently overflowed a
  // 64-slot reservation and produced an attack time of 0.034 s where the same
  // configuration standalone gave 0.955 s. The probe was broken, not the code.
  std::vector<clap_event_param_value_t> store;
  store.reserve(1024);
  auto param = [&](clap_id id, double v) {
    clap_event_param_value_t pv{};
    pv.header.size = sizeof(pv); pv.header.type = CLAP_EVENT_PARAM_VALUE;
    pv.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    pv.note_id = -1; pv.port_index = -1; pv.channel = -1; pv.key = -1;
    pv.param_id = id; pv.value = v;
    store.push_back(pv);
    evl.ev.push_back(&store.back().header);
  };

  auto run = [&](int blocks, std::vector<float> *capture) {
    for (int b = 0; b < blocks; b++)
    {
      clap_process_t proc{};
      proc.audio_inputs_count = 0; proc.audio_outputs_count = 1; proc.audio_outputs = &out;
      proc.frames_count = BLK; proc.in_events = &evl.in; proc.out_events = &outEv;
      proc.steady_time = (int64_t)b * BLK;
      p->process(p, &proc);
      evl.ev.clear();
      if (capture)
        for (int i = 0; i < BLK; i++) capture->push_back(0.5f * (L[i] + R[i]));
    }
  };

  // Render one oscillator alone from a fresh state. The other is silenced by
  // VOLUME, not solo: solo's gain is one-pole smoothed and its state survives
  // plug_reset, so the second render would start mid-ramp.
  auto renderSolo = [&](uint32_t which, const std::vector<std::pair<clap_id,double>> &ps) {
    freshPlugin();
    store.clear(); evl.ev.clear();
    for (uint32_t o = 0; o < 2; o++)
    {
      param(o * S + 1, 5);       // voices
      param(o * S + 3, 1234);    // seed
      param(o * S + 4, 0.2);     // detune
      param(o * S + 14, 1.0);    // width, so the mono fold is observable
      param(o * S + 150, 1);   // osc 2 ships OFF since ADR-099 A1 — switch first
      param(o * S + 17, which == o ? 0.4 : 0.0);
    }
    for (const auto &kv : ps) param(kv.first, kv.second);
    run(4, nullptr);
    clap_event_note_t n{};
    n.header.size = sizeof(n); n.header.type = CLAP_EVENT_NOTE_ON;
    n.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    n.note_id = 1; n.port_index = 0; n.channel = 0; n.key = 60; n.velocity = 1.0;
    evl.ev.push_back(&n.header);
    std::vector<float> buf;
    run(90, &buf);
    return buf;
  };
  auto timeTo90 = [&](const std::vector<float> &b) {
    double peak = 0;
    for (float v : b) peak = std::fmax(peak, std::fabs((double)v));
    for (size_t i = 0; i < b.size(); i++)
      if (std::fabs((double)b[i]) >= 0.9 * peak) return (double)i / 44100.0;
    return -1.0;
  };

  char d[200];
  const double LONG = 1.5;

  // ---- 1. a GLOBAL param reaches every oscillator -------------------------
  // `inertia` (11) is global and, unlike a stepped switch, produces a
  // continuously measurable difference. Observed through attack time is not
  // possible for it, so use the amp envelope's global sibling: `release` left
  // the global set with A12, so use `voiceMono`-free `inertia` via glide is
  // awkward — instead assert on a param that is BOTH global and audible:
  // mono fold (15) collapses the stereo image, so |L-R| goes to zero.
  {
    auto lrDiff = [&](uint32_t which, double monoOn) {
      freshPlugin();
      store.clear(); evl.ev.clear();
      for (uint32_t o = 0; o < 2; o++)
      {
        param(o * S + 1, 5); param(o * S + 3, 1234);
        param(o * S + 4, 0.2); param(o * S + 14, 1.0);
        param(o * S + 150, 1);   // osc 2 ships OFF since ADR-099 A1 — switch first
        param(o * S + 17, which == o ? 0.4 : 0.0);
      }
      param(15, monoOn);                       // GLOBAL mono fold
      run(4, nullptr);
      clap_event_note_t n{};
      n.header.size = sizeof(n); n.header.type = CLAP_EVENT_NOTE_ON;
      n.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      n.note_id = 2; n.port_index = 0; n.channel = 0; n.key = 60; n.velocity = 1.0;
      evl.ev.push_back(&n.header);
      run(30, nullptr);
      double acc = 0; int cnt = 0;
      for (int b = 0; b < 20; b++)
      {
        clap_process_t proc{};
        proc.audio_inputs_count = 0; proc.audio_outputs_count = 1; proc.audio_outputs = &out;
        proc.frames_count = BLK; proc.in_events = &evl.in; proc.out_events = &outEv;
        proc.steady_time = (int64_t)b * BLK;
        p->process(p, &proc);
        evl.ev.clear();
        for (int i = 0; i < BLK; i++) { acc += std::fabs((double)L[i] - R[i]); cnt++; }
      }
      return acc / cnt;
    };
    const double w1 = lrDiff(0, 0), w2 = lrDiff(1, 0);      // wide
    const double m1 = lrDiff(0, 1), m2 = lrDiff(1, 1);      // mono folded
    std::snprintf(d, sizeof(d), "|L-R| wide %.5f/%.5f -> mono %.5f/%.5f (osc1/osc2)", w1, w2, m1, m2);
    check(w1 > 1e-4 && w2 > 1e-4 && m1 < 0.05 * w1 && m2 < 0.05 * w2,
          "a GLOBAL param reaches every oscillator", d);
  }

  // ---- 2. a PER-OSCILLATOR param reaches only its own ----------------------
  // The vacuity control. Fanning everything to everything would pass assertion
  // 1 perfectly and silently destroy addressing.
  {
    /* {166,0},{167,0}: this probe MEASURES attack via time-to-90%-of-peak,
       which silently assumed a near-unison default — a beating detuned swarm
       peaks mid-render and reads "slow" regardless of its actual attack.
       That assumption became false on 2026-08-30 when the macros started
       defaulting centred (detune rested at 0.5 through default routes).
       ADR-156 removed those routes (detune rests at 0.28 with no macro on
       it), so the pins below are inert today — kept as the probe stating
       its assumption, so a future default route cannot silently re-break
       it. The detector-shares-assumption trap, caught by its own failure. */
    const double a1 = timeTo90(renderSolo(0, {{166, 0}, {167, 0}, {19, LONG}}));
    const double a2 = timeTo90(renderSolo(1, {{166, 0}, {167, 0}, {19, LONG}}));
    std::snprintf(d, sizeof(d), "id 19 alone -> osc1 90%% at %.3f s, osc2 at %.3f s", a1, a2);
    check(a1 > 0.5 && a2 < 0.1, "a PER-OSC param reaches ONLY its own oscillator", d);

    const double b1 = timeTo90(renderSolo(0, {{166, 0}, {167, 0}, {19, LONG}, {1000 + 19, LONG}}));
    const double b2 = timeTo90(renderSolo(1, {{19, LONG}, {1000 + 19, LONG}}));
    std::snprintf(d, sizeof(d), "ids 19 and 1019 -> osc1 %.3f s, osc2 %.3f s", b1, b2);
    check(b1 > 0.5 && b2 > 0.5, "addressing the second oscillator's copy works", d);
  }

  p->stop_processing(p);
  p->deactivate(p);
  p->destroy(p);
  std::printf("paramscope_check: %s (%d failures)\n", failures ? "RED" : "GREEN", failures);
  return failures ? 1 : 0;
}
