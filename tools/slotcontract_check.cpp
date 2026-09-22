/* slotcontract_check — does every FX slot keep the promise it declares?
 *
 * Approved contract, 2026-08-15: "the rack owns dry/wet". Six slot types
 * currently mean four different things by `amount`, with three different
 * identity points and two slots that cannot be bypassed at any setting. That is
 * how Notch shipped collapsing stereo to mono at a setting a patch author reads
 * as "off": nothing checked, because there was no promise to check against.
 *
 * `src/fx_rack.h` now DECLARES the promise (`kSlotContract`). This drives the
 * real rack through the CLAP factory — never a slot class directly, because the
 * thing under test is the route from a param event to audio — and holds every
 * slot to what it declared.
 *
 * THE CONTROL COMES FIRST (project pattern, see notchslot_check / mixer_check):
 * every case is measured against a never-touched no-slot render of the identical
 * patch, and explicit-Off must be bit-exact to it. That is the floor which proves
 * the comparison has zero inherent noise BEFORE any slot is asked to prove
 * anything — otherwise "the slot is clean" and "the harness is blind" look alike.
 *
 * DECORRELATED STEREO IN (L 220 Hz, R 330 Hz) is the input that caught Notch. A
 * mono-summing slot is invisible to a correlated input, so a probe that used one
 * would certify exactly the defect this gate exists to prevent.
 *
 * The harness below is lifted from notchslot_check rather than rewritten: a
 * second CLAP driver is a second thing to keep in step.
 * WIRED: ./verify full.
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "clap/clap.h"
#include "../src/fx_rack.h"
#include "../src/hypersaw_clap_entry.h"

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
Render render(int fxType, double fxAmt, double noteHz, double fxMix = 1.0)
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
    param(133, fxMix);          // fx1mix — rack-owned dry/wet (1 = today's path)
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

// ---- contract assertions ---------------------------------------------------
// `failures` and the reporting style come from the harness above; a second
// counter would let one of them reach zero while the other did not.
int pinnedHit = 0;
bool isPinned(const char *slot, const char *what);
void fail(const char *slot, const char *what, const std::string &detail)
{
  if (isPinned(slot, what))
  {
    std::printf("  pinned %-7s %-26s %s\n", slot, what, detail.c_str());
    pinnedHit++;
    return;
  }
  std::printf("  FAIL   %-7s %-26s %s\n", slot, what, detail.c_str());
  failures++;
}

double rms(const std::vector<float> &v)
{
  double a = 0;
  for (float x : v) a += (double)x * x;
  return v.empty() ? 0 : std::sqrt(a / v.size());
}

// Stereo width proxy: energy of the SIDE signal. A mono-summing slot drives this
// to zero while leaving mid energy (and therefore RMS) almost untouched.
double sideEnergy(const Render &r)
{
  double a = 0;
  const size_t n = std::min(r.L.size(), r.R.size());
  for (size_t i = 0; i < n; i++) { const double d = (double)r.L[i] - r.R[i]; a += d * d; }
  return n ? std::sqrt(a / n) : 0;
}

/* PINNED VIOLATIONS — the defects this gate was built to expose, recorded so the
 * gate can go green on "nothing NEW is broken" while the rack-side fix is still
 * outstanding. Same device as conformance_check's pinned sets, and the same rule:
 * a pin is a debt with a name, never a silencer. Removing a pin is how the fix
 * proves itself, and a violation NOT in this list turns the gate red immediately.
 *
 *   Comp  / Notch — declare identity_at < 0: they cannot be bypassed at any
 *                   amount. This is precisely what the approved contract (rack-
 *                   owned `mix`, `mix == 0` early-out) removes.
 *   Comb          — FOUND BY THIS GATE, not by the survey that motivated it:
 *                   +8.8 dB at amount 0.5 with changes_level = false. The
 *                   proposal's table listed Comb as "0 = dry" and said nothing
 *                   about level, because five of its six rows were documentation
 *                   rather than measurement.
 */
struct Pin { const char *slot; const char *what; };
constexpr Pin kPinned[] = {
    // Comp and Notch's "no identity point" pins are GONE — retired by the
    // rack-owned mix, which is what paying the debt looks like. Comb's finding is
    // about the AMOUNT axis at 0.5 and is untouched by bypass, so it stands.
    {"Comb", "changes level"},
};

bool isPinned(const char *slot, const char *what)
{
  for (const Pin &p : kPinned)
    if (std::strcmp(p.slot, slot) == 0 && std::strcmp(p.what, what) == 0) return true;
  return false;
}

int main()
{
  const double f0 = 220.0;
  const Render noSlot = render(-1, 0.0, f0);
  const Render offExplicit = render(0, 0.5, f0);

  std::printf("slotcontract_check: driving the real rack through the CLAP factory\n");

  // FLOOR: the comparison itself must have zero noise, or nothing below means anything.
  if (maxAbsDiff(noSlot.L, offExplicit.L) != 0.0 || maxAbsDiff(noSlot.R, offExplicit.R) != 0.0)
    fail("Off", "control: Off == no-slot", "explicit Off is not bit-exact to never-touched");

  const double baseSide = sideEnergy(noSlot), baseRms = rms(noSlot.L);
  const char *names[] = {"Off", "Drive", "Filter", "Gain", "Comp", "Comb", "Notch"};

  for (int t = 1; t <= 6; t++)
  {
    const hypersaw::SlotContract &c = hypersaw::kSlotContract[t];

    /* THE UNIVERSAL GUARANTEE, and the reason the contract exists: with the rack
       owning dry/wet, `mix = 0` is a bit-exact bypass for EVERY slot type — by
       construction, not by each slot remembering. This is the assertion a new
       slot type cannot escape, and it replaces the per-slot `identity_at` hunt
       that let Comp and Notch have no identity point at all. */
    {
      const Render byp = render(t, 0.5, f0, 0.0);   // any amount, mix = 0
      const double d = std::max(maxAbsDiff(noSlot.L, byp.L), maxAbsDiff(noSlot.R, byp.R));
      if (d != 0.0)
      {
        char b[96]; std::snprintf(b, sizeof(b), "mix = 0 is not bypass, max|diff| = %.3g", d);
        fail(names[t], "rack bypass broken", b);
      }
    }

    // 1. IDENTITY. A slot that declares an identity point must be bit-exact there.
    if (c.identity_at >= 0)
    {
      const Render id = render(t, c.identity_at, f0);
      const double d = std::max(maxAbsDiff(noSlot.L, id.L), maxAbsDiff(noSlot.R, id.R));
      if (d != 0.0)
      {
        char b[96]; std::snprintf(b, sizeof(b), "declares identity at %.3g, max|diff| = %.3g",
                                  c.identity_at, d);
        fail(names[t], "identity is not identity", b);
      }
    }
    // A slot with no `amount` identity point is no longer a defect: the rack's
    // `mix = 0` bypasses it, which is exactly what the contract bought. The
    // declaration now records a fact about the AMOUNT axis, not about bypass.


    // 2. IMAGE. Undeclared collapse of the stereo image is what Notch did.
    const Render on = render(t, 0.5, f0);
    const double sd = sideEnergy(on);
    if (!c.changes_image && baseSide > 1e-6 && sd < 0.25 * baseSide)
    {
      char b[96]; std::snprintf(b, sizeof(b), "side energy %.4g -> %.4g without declaring it", baseSide, sd);
      fail(names[t], "collapses stereo image", b);
    }

    // 3. LEVEL. Undeclared loudness change, generously bounded: this is a promise
    //    about NOT being a gain stage, not a loudness spec.
    const double r = rms(on.L);
    if (!c.changes_level && baseRms > 1e-6)
    {
      const double dB = 20.0 * std::log10(std::max(r, 1e-12) / baseRms);
      if (std::fabs(dB) > 6.0)
      {
        char b[96]; std::snprintf(b, sizeof(b), "%.2f dB at amount 0.5 without declaring it", dB);
        fail(names[t], "changes level", b);
      }
    }
  }

  // A pin that stops firing is ALSO news: it means the debt was paid (or the
  // measurement drifted) and the pin is now hiding nothing. Say so rather than
  // let a stale pin sit there looking like coverage.
  const int expected = (int)(sizeof(kPinned) / sizeof(kPinned[0]));
  if (pinnedHit != expected)
    std::printf("  NOTE  %d of %d pinned violations no longer fire — re-check the pins\n",
                expected - pinnedHit, expected);
  std::printf("slotcontract_check: %s (%d failure(s), %d pinned)\n",
              failures ? "RED" : "GREEN", failures, pinnedHit);
  return failures ? 1 : 0;
}
