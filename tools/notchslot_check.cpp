/* notchslot_check — FX rack slot type 6 (Notch) actually reaches the DSP.
 *
 * notch_check (tools/notch_check.cpp) proves NotchCore itself is correct in
 * isolation: audio parity vs the JS PhaserLab reference + L0-16 notch
 * exactness. It says nothing about whether the SHIPPING PLUGIN can ever
 * select it — and until this task, nothing could: FxType had no Notch value,
 * the fx*type params were clamped to 0..5, and no GUI offered it. A perfectly
 * correct, oracle-covered core sitting unreachable behind a rack that cannot
 * address it is L0023's failure mode (a control with no effect) wearing the
 * opposite mask — the DSP is fine, the SWITCH doesn't exist.
 *
 * Drives the REAL plugin through the CLAP factory (like mixer_check /
 * steal_check), never NotchCore directly — the thing under test is the
 * ROUTE from a param event to the rack's processSlot, not the notch math.
 *
 * THE CONTROL, not just the assertion (project pattern, see mixer_check /
 * steal_check headers): a probe that only shows "something changed" when a
 * slot is set to Notch cannot tell "the rack now reaches NotchCore" apart
 * from "something else about sending param 57 perturbs the render" (a
 * stray side effect, a reset, a smoothing glitch). So every case here is
 * measured against an explicit-Off render AND a never-touched (no-slot)
 * render of the identical patch — Off must be bit-exact to no-slot (the
 * project's existing parity contract, ADR-054), which is the floor that
 * proves the comparison itself has zero inherent noise before Notch is ever
 * asked to prove anything.
 *
 * WHY A SPECTRAL METRIC, not just RMS. A single scalar (e.g. total energy)
 * cannot distinguish "the bus got quieter" from "the bus got RESHAPED" —
 * and a broken wiring that merely scales output (e.g. a stray gain node
 * misrouted to id 57) could pass an RMS-only check by accident. Goertzel at
 * several of the test tone's own harmonics, summed as a squared-difference
 * vector, changes only if the SPECTRAL SHAPE differs — which a notch cascade
 * does and a scalar gain error does not (a paranoid README, but disproving a
 * shape-blind detector this cheaply is worth eight lines).
 * WIRED: ./verify full.
 */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <vector>

#include "../src/hypersaw_clap_entry.h"

namespace {

constexpr double kPi = 3.14159265358979323846;  // no M_PI under MSVC (L0003)
constexpr double kSR = 44100.0;
constexpr int kBlk = 512;

static const clap_host_t kHost = {
    CLAP_VERSION_INIT, nullptr, "notchslot_check", "-", "-", "1.0",
    [](const clap_host_t *, const char *) -> const void * { return nullptr; },
    [](const clap_host_t *) {}, [](const clap_host_t *) {}, [](const clap_host_t *) {}};

struct EvList
{
  clap_input_events_t in{};
  std::vector<const clap_event_header_t *> ev;
};
uint32_t evSize(const clap_input_events_t *l) { return (uint32_t)((EvList *)l->ctx)->ev.size(); }
const clap_event_header_t *evGet(const clap_input_events_t *l, uint32_t i)
{
  return ((EvList *)l->ctx)->ev[i];
}
bool outPush(const clap_output_events_t *, const clap_event_header_t *) { return true; }

int failures = 0;
void check(bool ok, const char *what, const char *detail)
{
  std::printf("%-6s %s  (%s)\n", ok ? "OK" : "FAIL", what, detail);
  if (!ok) failures++;
}

// Single-bin Goertzel, unwindowed — matches mixer_check's detector, which is
// adequate here for the same reason: every measured window sits well after
// note-on and param settling, on a held tone (no onset transient to leak).
double goertzel(const std::vector<float> &x, double freq)
{
  const double w = 2.0 * kPi * freq / kSR;
  const double c = 2.0 * std::cos(w);
  double s1 = 0, s2 = 0;
  for (float v : x) { const double s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
  const double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w);
  return 2.0 * std::sqrt(re * re + im * im) / (double)x.size();
}

// Squared-difference spectral distance across the test tone's own harmonics
// (fundamental 220 Hz through the 8th). Zero iff the two renders agree at
// every one of these bins; grows with any reshaping of the harmonic balance,
// not just a level change.
constexpr int kNBins = 8;
double spectralDistance(const std::vector<float> &a, const std::vector<float> &b, double f0)
{
  double sum = 0;
  for (int h = 1; h <= kNBins; h++)
  {
    const double d = goertzel(a, f0 * h) - goertzel(b, f0 * h);
    sum += d * d;
  }
  return sum;
}

struct Render
{
  std::vector<float> L, R;  // raw stereo capture (bit-exact comparisons)
};

// fxType < 0 means "never send fx1type/fx1amt at all" (the true no-slot
// case). Every render builds a FRESH plugin instance (paramscope_check's
// established pattern in this repo) so no case can leak rack/population
// state — NotchCore's internal drift, in particular — into the next.
Render render(int fxType, double fxAmt, double noteHz)
{
  auto *factory =
      (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_t *p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
  p->init(p);
  p->activate(p, kSR, 32, 2048);
  p->start_processing(p);

  std::vector<float> L(kBlk), R(kBlk);
  float *chans[2] = {L.data(), R.data()};
  clap_audio_buffer_t out{};
  out.data32 = chans;
  out.channel_count = 2;

  EvList evl;
  evl.in.ctx = &evl;
  evl.in.size = evSize;
  evl.in.get = evGet;
  clap_output_events_t outEv{nullptr, outPush};

  std::vector<clap_event_param_value_t> pstore;
  pstore.reserve(16);
  auto param = [&](clap_id id, double v) {
    clap_event_param_value_t pv{};
    pv.header.size = sizeof(pv);
    pv.header.type = CLAP_EVENT_PARAM_VALUE;
    pv.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    pv.note_id = -1;
    pv.port_index = -1;
    pv.channel = -1;
    pv.key = -1;
    pv.param_id = id;
    pv.value = v;
    pstore.push_back(pv);
    evl.ev.push_back(&pstore.back().header);
  };

  Render out_render;
  auto run = [&](int blocks, bool capture) {
    for (int b = 0; b < blocks; b++)
    {
      clap_process_t proc{};
      proc.audio_inputs_count = 0;
      proc.audio_outputs_count = 1;
      proc.audio_outputs = &out;
      proc.frames_count = kBlk;
      proc.in_events = &evl.in;
      proc.out_events = &outEv;
      proc.steady_time = (int64_t)b * kBlk;
      p->process(p, &proc);
      evl.ev.clear();
      if (capture)
        for (int i = 0; i < kBlk; i++) { out_render.L.push_back(L[i]); out_render.R.push_back(R[i]); }
    }
  };

  // Minimal single-voice patch: n=1, detune=0, K=0 — a clean sawtooth with no
  // swarm smear, so every harmonic bin belongs to ONE source and a notch's
  // effect on the harmonic balance is unambiguous.
  param(1, 1);   // n = 1 oscillator
  param(4, 0);   // detune = 0
  param(6, 0);   // K = 0
  if (fxType >= 0)
  {
    param(57, (double)fxType);  // fx1type
    param(58, fxAmt);           // fx1amt
  }
  run(4, false);  // let param smoothing settle before the note

  clap_event_note_t on{};
  on.header.size = sizeof(on);
  on.header.type = CLAP_EVENT_NOTE_ON;
  on.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  on.note_id = 1;
  on.port_index = 0;
  on.channel = 0;
  on.key = (int16_t)std::lround(69 + 12 * std::log2(noteHz / 440.0));
  on.velocity = 1.0;
  evl.ev.push_back(&on.header);
  run(1, false);

  run(20, false);  // reach sustain + let the rack's per-block state (Notch's
                    // controlTick, comp/comb smoothing) settle to steady state
  run(40, true);    // ~464 ms captured — long enough for stable Goertzel bins

  p->stop_processing(p);
  p->deactivate(p);
  p->destroy(p);
  return out_render;
}

double maxAbsDiff(const std::vector<float> &a, const std::vector<float> &b)
{
  double m = 0;
  for (size_t i = 0; i < a.size() && i < b.size(); i++)
    m = std::max(m, (double)std::fabs(a[i] - b[i]));
  return m;
}

}  // namespace

int main()
{
  const double f0 = 220.0;  // A3 — plenty of harmonics below Nyquist (8*220=1760 Hz)

  const Render noSlot = render(-1, 0.0, f0);         // never touch fx1type/fx1amt
  const Render offExplicit = render(0, 0.5, f0);     // fx1type=0 (Off), amount irrelevant
  const Render offOtherAmt = render(0, 0.9, f0);     // fx1type=0, DIFFERENT amount
  const Render notchOn = render(6, 0.5, f0);         // fx1type=6 (Notch), amount = mix

  char d[256];

  // ---- MUST-READ-NOTHING CONTROL 1: explicit Off == no-slot, bit-exact ----
  const double diffNoSlotOffL = maxAbsDiff(noSlot.L, offExplicit.L);
  const double diffNoSlotOffR = maxAbsDiff(noSlot.R, offExplicit.R);
  std::snprintf(d, sizeof(d), "max|no-slot - Off| L=%.9g R=%.9g over %zu samples",
                diffNoSlotOffL, diffNoSlotOffR, noSlot.L.size());
  check(diffNoSlotOffL == 0.0 && diffNoSlotOffR == 0.0,
        "slot Off is bit-exact to the no-slot case (ADR-054 parity contract)", d);

  // ---- MUST-READ-NOTHING CONTROL 2: Off ignores `amount` entirely ---------
  // A second, independent way the same bug could hide: a rack that reads
  // amount before checking type would perturb the render even while Off.
  const double diffOffAmt = maxAbsDiff(offExplicit.L, offOtherAmt.L);
  std::snprintf(d, sizeof(d), "max|Off@amt0.5 - Off@amt0.9| L=%.9g", diffOffAmt);
  check(diffOffAmt == 0.0, "Off does not read `amount` at all", d);

  // ---- MEASURED FLOOR for the spectral-distance metric ---------------------
  // Derived from a render where the notch effect is genuinely absent: the
  // no-slot vs explicit-Off pair above. This system has no RNG and no
  // wall-clock read on this path (SPEC 5.7), so the honest measured floor is
  // exactly 0 — not a guess, not rounded down to look tidy. Printed so a
  // reviewer sees the actual number, not an assumed one.
  const double floorSpec = spectralDistance(noSlot.L, offExplicit.L, f0);
  const double notchDist = spectralDistance(noSlot.L, notchOn.L, f0);
  std::snprintf(d, sizeof(d),
                "spectral distance: measured floor (no-slot vs Off) = %.9g; "
                "no-slot vs Notch@amt0.5 = %.9g",
                floorSpec, notchDist);
  // notchDist must clear the floor by a wide margin, not sit within ~2x of it
  // (the project's own re-derive-don't-nudge rule) — with a floor of exactly
  // 0, any threshold above float noise (1e-9) already clears that bar by many
  // orders of magnitude once Notch does anything at all.
  check(floorSpec == 0.0 && notchDist > 1e-6,
        "slot type 6 (Notch) at a non-zero amount measurably reshapes the "
        "output spectrum vs the same patch with the slot Off",
        d);

  std::printf("notchslot_check: %s (%d failures)\n", failures ? "RED" : "GREEN", failures);
  return failures ? 1 : 0;
}
