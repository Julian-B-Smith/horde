/*
 * notefuzz_check — note-lifecycle oracle at the CLAP layer (ADR-038).
 *
 * Links the plugin impl statically (like state_check) and drives process()
 * with seeded random note on/off streams — poly, mono, mono+legato — plus
 * the MIDI 1.0 vel-0 convention (releases delivered as NOTE_ON velocity 0,
 * which is what the AU wrapper forwards for a controller's 0x90-vel-0
 * note-off). Release sits at its knob minimum; after every held key is
 * released the output must decay to silence. A voice still audible 0.5 s
 * after all-keys-up is a hang — the 2026-07-18 "doesn't stop when you let
 * go" report.
 *
 * Harness trap (learned the hard way): events are delivered time-sorted
 * within a block, so per-block timestamps must be drawn sorted BEFORE
 * assigning on/off actions — otherwise the generator emits an OFF that
 * precedes its own ON and fakes a hang no host can produce.
 * WIRED: ./verify full.
 */
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
#include <clap/clap.h>

#include "../src/hypersaw_clap_entry.h"

namespace
{
const void *host_get_extension(const clap_host_t *, const char *) { return nullptr; }
void host_noop(const clap_host_t *) {}
const clap_host_t kHost = {CLAP_VERSION, nullptr, "notefuzz_check", "", "", "1.0",
                           host_get_extension, host_noop, host_noop, host_noop};

bool oev_try_push(const clap_output_events_t *, const clap_event_header_t *) { return true; }
const clap_output_events_t kOut = {nullptr, oev_try_push};

struct EvList
{
  clap_input_events_t list;
  std::vector<clap_event_note_t> notes;
  std::vector<clap_event_param_value_t> params;
  std::vector<const clap_event_header_t *> order;  // time-sorted view
  void finalize()
  {
    order.clear();
    for (auto &n : notes) order.push_back(&n.header);
    for (auto &p : params) order.push_back(&p.header);
    std::stable_sort(order.begin(), order.end(),
                     [](const clap_event_header_t *a, const clap_event_header_t *b) {
                       return a->time < b->time;
                     });
    list.ctx = this;
    list.size = [](const clap_input_events_t *l) -> uint32_t {
      return (uint32_t)((EvList *)l->ctx)->order.size();
    };
    list.get = [](const clap_input_events_t *l, uint32_t i) -> const clap_event_header_t * {
      return ((EvList *)l->ctx)->order[i];
    };
  }
};

clap_event_note_t mkNote(uint16_t type, uint32_t time, int16_t key, int32_t noteId,
                         double velocity = 1.0)
{
  clap_event_note_t ev{};
  ev.header.size = sizeof(ev);
  ev.header.time = time;
  ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  ev.header.type = type;
  ev.note_id = noteId;
  ev.port_index = 0;
  ev.channel = 0;
  ev.key = key;
  ev.velocity = velocity;
  return ev;
}

clap_event_param_value_t mkParam(clap_id id, double value)
{
  clap_event_param_value_t ev{};
  ev.header.size = sizeof(ev);
  ev.header.time = 0;
  ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  ev.header.type = CLAP_EVENT_PARAM_VALUE;
  ev.param_id = id;
  ev.cookie = nullptr;
  ev.note_id = -1;
  ev.port_index = -1;
  ev.channel = -1;
  ev.key = -1;
  ev.value = value;
  return ev;
}

// mulberry32 — deterministic, same family the core uses
uint32_t mrand(uint32_t &s)
{
  s += 0x6D2B79F5u;
  uint32_t t = s;
  t = (t ^ (t >> 15)) * (t | 1u);
  t ^= t + (t ^ (t >> 7)) * (t | 61u);
  return t ^ (t >> 14);
}

constexpr int kBlock = 256;
constexpr double kSR = 44100.0;

// Peak |output| in [allOff + 0.5 s, allOff + 1.5 s]. With release at the
// 5 ms knob minimum the audible tail is ~46 ms, so anything above the floor
// in that window is a hung voice.
// `restrike` allows a NOTE_ON for a key already held, and `liveTiming` stamps
// every event in a block at frame 0. Both model a COMPUTER KEYBOARD played
// fast, which is where the human saw notes hang (2026-07-26: "notes get stuck
// for longer than they ought to when I play quickly ... hasn't happened with
// preprogrammed MIDI in the piano roll"). A piano roll emits a clean off before
// the next on for a key and stamps events sample-accurately; live input does
// neither, and the original fuzz modelled only the piano-roll shape — it
// explicitly `continue`d past duplicate keys and gave every event a distinct
// sorted time. Those two skips were the whole blind spot.
struct Result { double hangPeak; double releaseMs; };
Result runScenario(uint32_t seed, bool mono, bool legato, bool vel0off,
                   bool restrike, bool liveTiming, int twoOsc = 0)
{
  auto *factory =
      (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_t *p =
      factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
  p->init(p);
  p->activate(p, kSR, 32, kBlock);
  p->start_processing(p);

  std::vector<float> L(kBlock), R(kBlock);
  float *chans[2] = {L.data(), R.data()};
  clap_audio_buffer_t out{};
  out.data32 = chans;
  out.channel_count = 2;
  clap_process_t proc{};
  proc.frames_count = kBlock;
  proc.audio_outputs = &out;
  proc.audio_outputs_count = 1;
  proc.out_events = &kOut;

  uint32_t rng = seed;
  std::vector<int16_t> held;
  int32_t nextId = 1;

  auto process = [&](EvList &evs) {
    evs.finalize();
    proc.in_events = &evs.list;
    p->process(p, &proc);
  };
  auto pushOff = [&](EvList &evs, uint32_t t, int16_t key) {
    if (vel0off)
      evs.notes.push_back(mkNote(CLAP_EVENT_NOTE_ON, t, key, -1, 0.0));
    else
      evs.notes.push_back(mkNote(CLAP_EVENT_NOTE_OFF, t, key, -1));
  };

  {  // configure: release at knob min, sustain 1 (defaults elsewhere)
    EvList evs;
    evs.params.push_back(mkParam(22, 0.005));
    evs.params.push_back(mkParam(21, 1.0));
    /* FOUNDATIONS brief 2026-08-11 §1, and they were right: without this the
       plugin's own constructor leaves oscillator 2 at vol 0, and the hang gate
       reads RENDERED AUDIO — so a voice stuck in oscillator 2 renders exactly
       nothing and this oracle was structurally incapable of failing on the
       entire class of hang that needs two oscillators to exist. Every green run
       before today said nothing whatsoever about oscillator 2.

       `vol` is per-oscillator by our own A12 ruling, so raising it needs id
       1017 — the base id 17 would only re-set oscillator 1.

       The envelopes are set DIFFERENT on purpose. Slot alignment across cores
       is a convention ("note fan-out keeps slot indices aligned",
       hypersaw_clap.cpp:1421-1423), not a construction, and alloc()'s tiers 1
       and 2 read `s.env` — so identical envelopes make the two cores allocate
       in lockstep and the convention holds for free. Divergent envelopes are
       the condition under which it can break. Release stays SHORT on both:
       a long tail on oscillator 2 would decay past the 0.5 s window and report
       a hang that is just a slow release. */
    if (twoOsc)
    {
      evs.params.push_back(mkParam(1017, 0.4));    // osc2 audible at all
      if (twoOsc == 2)
      {
        evs.params.push_back(mkParam(1019, 0.15));   // attack  (osc1: default)
        evs.params.push_back(mkParam(1020, 0.40));   // decay
        evs.params.push_back(mkParam(1021, 0.60));   // sustain (osc1: 1.0)
        evs.params.push_back(mkParam(1022, 0.020));  // release — short, like osc1
      }
      else
      {
        // CONTROL (twoOsc == 1): oscillator 2 equally AUDIBLE, envelope MATCHED
        // to oscillator 1. If the hang were an artefact of raising the volume,
        // of the mode, or of the fuzz stream, it would appear here too. This
        // must read clean or the divergent result proves nothing about
        // divergence (L0032 — a probe with no must-read-clean control confirms
        // whatever you expected).
        evs.params.push_back(mkParam(1021, 1.0));
        evs.params.push_back(mkParam(1022, 0.005));
      }
    }
    if (mono) evs.params.push_back(mkParam(32, 1.0));
    evs.params.push_back(mkParam(34, legato ? 1.0 : 0.0));
    process(evs);
  }

  const int fuzzBlocks = 400;  // ~2.3 s of dense playing
  for (int b = 0; b < fuzzBlocks; b++)
  {
    EvList evs;
    const int nEv = (int)(mrand(rng) % 4);  // 0..3 events per block
    uint32_t times[4];
    for (int e = 0; e < nEv; e++) times[e] = liveTiming ? 0 : (mrand(rng) % kBlock);
    std::sort(times, times + nEv);  // causal delivery — see header comment
    for (int e = 0; e < nEv; e++)
    {
      const uint32_t t = times[e];
      const bool doOff = !held.empty() && (mrand(rng) & 1u);
      if (doOff)
      {
        const size_t idx = mrand(rng) % held.size();
        const int16_t key = held[idx];
        held.erase(held.begin() + idx);
        pushOff(evs, t, key);
      }
      else
      {
        const int16_t key = (int16_t)(48 + (mrand(rng) % 25));
        const bool dup = std::find(held.begin(), held.end(), key) != held.end();
        // In restrike mode a duplicate ON is DELIVERED but not re-pushed, so the
        // scenario still sends exactly one OFF per key — the on,on,off shape a
        // fast re-press produces when the host coalesces or drops the middle off.
        if (dup && !restrike) continue;
        if (!dup) held.push_back(key);
        evs.notes.push_back(mkNote(CLAP_EVENT_NOTE_ON, t, key, nextId++));
      }
    }
    process(evs);
  }

  {  // release every held key
    EvList evs;
    uint32_t t = 0;
    for (int16_t key : held) pushOff(evs, t++, key);
    held.clear();
    process(evs);
  }

  // TWO measurements now. `hangPeak` is the original permanent-hang gate.
  // `releaseMs` is how long after the last note-off the output stays above the
  // floor — a note held 300 ms too long is inaudible to the hang gate but is
  // exactly what the human reported, so a finite over-hold needs its own number.
  const int skipBlocks = (int)(0.5 * kSR) / kBlock;
  const int measureBlocks = (int)(1.0 * kSR) / kBlock;
  double peak = 0;
  int lastLoudBlock = -1;
  EvList empty;
  for (int b = 0; b < skipBlocks + measureBlocks; b++)
  {
    process(empty);
    for (int i = 0; i < kBlock; i++)
    {
      const double a = std::fabs((double)L[i]) + std::fabs((double)R[i]);
      if (a > 1e-5) lastLoudBlock = b;
      if (b >= skipBlocks && a > peak) peak = a;
    }
  }
  const double releaseMs = (lastLoudBlock + 1) * (double)kBlock / kSR * 1000.0;

  p->stop_processing(p);
  p->deactivate(p);
  p->destroy(p);
  return {peak, releaseMs};
}
}  // namespace

namespace {
// MINIMAL REPRO SEARCH. Brute-forces every short event sequence over two keys
// and reports the SHORTEST that leaves the voice sounding. Random fuzzing finds
// that a hang exists; this finds what the hang IS, which is the part you can
// reason about. Kept in the oracle rather than thrown away because the next
// note-handling regression will want it too.
double runSeq(const std::vector<int> &seq, bool mono, bool legato)
{
  auto *factory =
      (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_t *p =
      factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
  p->init(p);
  p->activate(p, kSR, 32, kBlock);
  p->start_processing(p);
  std::vector<float> L(kBlock), R(kBlock);
  float *chans[2] = {L.data(), R.data()};
  clap_audio_buffer_t out{};
  out.data32 = chans; out.channel_count = 2;
  clap_process_t proc{};
  proc.frames_count = kBlock; proc.audio_outputs = &out;
  proc.audio_outputs_count = 1; proc.out_events = &kOut;
  auto process = [&](EvList &evs) { evs.finalize(); proc.in_events = &evs.list; p->process(p, &proc); };
  { EvList e;
    e.params.push_back(mkParam(22, 0.005)); e.params.push_back(mkParam(21, 1.0));
    if (mono) e.params.push_back(mkParam(32, 1.0));
    e.params.push_back(mkParam(34, legato ? 1.0 : 0.0));
    process(e); }
  int id = 1;
  for (int code : seq)
  {
    EvList e;
    const int16_t key = (int16_t)(60 + (code & 1));
    if (code < 2) e.notes.push_back(mkNote(CLAP_EVENT_NOTE_ON, 0, key, id++));
    else          e.notes.push_back(mkNote(CLAP_EVENT_NOTE_OFF, 0, key, -1));
    process(e);
  }
  double peak = 0; EvList empty;
  const int nb = (int)(1.0 * kSR) / kBlock;
  for (int b = 0; b < nb; b++)
  {
    process(empty);
    if (b < nb / 2) continue;
    for (int i = 0; i < kBlock; i++)
      peak = std::max(peak, std::fabs((double)L[i]) + std::fabs((double)R[i]));
  }
  p->stop_processing(p); p->deactivate(p); p->destroy(p);
  return peak;
}
const char *codeName(int c)
{ return c == 0 ? "on(60)" : c == 1 ? "on(61)" : c == 2 ? "off(60)" : "off(61)"; }

// A sequence is only a bug if every key that went down came back up.
bool balanced(const std::vector<int> &seq)
{
  int d[2] = {0, 0};
  for (int c : seq) { if (c < 2) d[c] = 1; else d[c - 2] = 0; }
  return d[0] == 0 && d[1] == 0;
}
void minimalSearch(bool mono, bool legato)
{
  std::vector<int> seq;
  for (int len = 2; len <= 6; len++)
  {
    seq.assign(len, 0);
    const long total = 1L << (2 * len);
    for (long n = 0; n < total; n++)
    {
      for (int i = 0; i < len; i++) seq[i] = (int)((n >> (2 * i)) & 3);
      if (!balanced(seq)) continue;
      if (runSeq(seq, mono, legato) > 1e-5)
      {
        std::printf("  MINIMAL HANG (mono=%d legato=%d, len %d): ", (int)mono, (int)legato, len);
        for (int c : seq) std::printf("%s ", codeName(c));
        std::printf("\n");
        return;
      }
    }
  }
  std::printf("  no hang up to length 6 (mono=%d legato=%d)\n", (int)mono, (int)legato);
}
}  // namespace

int main(int argc, char **argv)
{
  hypersaw_entry_init("");
  if (argc > 1 && std::string(argv[1]) == "--minimal")
  {
    std::printf("minimal-repro search over 2 keys, sequences up to length 6\n");
    minimalSearch(true, false);
    minimalSearch(true, true);
    minimalSearch(false, false);
    return 0;
  }
  const int nSeeds = argc > 1 ? std::atoi(argv[1]) : 15;
  int failures = 0;
  struct Mode { const char *name; bool mono, legato, vel0off, restrike, liveTiming; int twoOsc; };
  const Mode modes[] = {
      // existing coverage — unchanged, so the gate cannot weaken
      {"poly", false, false, false, false, false, 0},
      {"mono", true, false, false, false, false, 0},
      {"mono+legato", true, true, false, false, false, 0},
      {"poly+vel0off", false, false, true, false, false, 0},
      {"mono+vel0off", true, false, true, false, false, 0},
      // new: the computer-keyboard shapes
      {"poly+restrike", false, false, false, true, false, 0},
      {"mono+restrike", true, false, false, true, false, 0},
      {"mono+legato+restrike", true, true, false, true, false, 0},
      {"poly+live", false, false, false, false, true, 0},
      {"mono+live", true, false, false, false, true, 0},
      {"poly+restrike+live", false, false, false, true, true, 0},
      {"mono+legato+restrike+live", true, true, false, true, true, 0},
      // The must-read-clean control: oscillator 2 equally audible, envelope
      // MATCHED. Isolates envelope divergence as the cause rather than volume.
      {"poly+2osc-same", false, false, false, false, false, 1},
      {"mono+2osc-same", true, false, false, false, false, 1},
      {"mono+legato+2osc-same", true, true, false, false, false, 1},
      // TWO-OSCILLATOR coverage (FOUNDATIONS brief §1). Same shapes, but with
      // oscillator 2 audible and its envelope diverged from oscillator 1's, so
      // a hang that lives only in the second core can actually be heard. These
      // ADD; nothing above was relaxed to make room.
      {"poly+2osc", false, false, false, false, false, 2},
      {"mono+2osc", true, false, false, false, false, 2},
      {"mono+legato+2osc", true, true, false, false, false, 2},
      {"mono+legato+restrike+2osc", true, true, false, true, false, 2},
      {"mono+legato+restrike+live+2osc", true, true, false, true, true, 2},
      {"poly+restrike+live+2osc", false, false, false, true, true, 2},
  };
  for (const auto &m : modes)
  {
    int modeFail = 0;
    double worstRelease = 0;
    for (int s = 0; s < nSeeds; s++)
    {
      const Result r = runScenario(1000 + (uint32_t)s, m.mono, m.legato, m.vel0off,
                                   m.restrike, m.liveTiming, m.twoOsc);
      if (r.releaseMs > worstRelease) worstRelease = r.releaseMs;
      if (r.hangPeak > 1e-5)
      {
        failures++;
        modeFail++;
        if (modeFail <= 3)
          std::printf("FAIL hang mode=%s seed=%d peak=%.6g\n", m.name, 1000 + s, r.hangPeak);
      }
    }
    std::printf("%s %s: %d/%d silent after all-keys-up, worst tail %.0f ms\n",
                modeFail ? "FAIL" : "OK  ", m.name, nSeeds - modeFail, nSeeds, worstRelease);
  }
  // ---- ADR-083 arp-sustain steal gate (B27) --------------------------------
  // A sustained note under a 9-note/s arpeggio must keep sounding: release
  // tails occupy slots for ~1.1 s, and the pre-ADR-083 steal-oldest policy
  // sacrificed the held note ~11 arp notes in. Oracle: Goertzel power at the
  // held note's own f0 in LONG windows (short windows at 65 Hz measure the
  // sustain's own beat nulls — that detector once convicted the wrong code),
  // against a no-arp control run for the beating floor.
  {
    auto goertzelRun = [](bool withArp) -> double {
      auto *factory =
          (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
      const clap_plugin_t *p =
          factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
      p->init(p);
      p->activate(p, kSR, 32, 256);
      p->start_processing(p);
      std::vector<float> L(256), R(256);
      float *chans[2] = {L.data(), R.data()};
      clap_audio_buffer_t out{};
      out.data32 = chans;
      out.channel_count = 2;
      const int SUS = 36;
      const double f0 = 440.0 * std::pow(2.0, (SUS - 69) / 12.0);
      int lastKey = -1, step = 0, nextId = 1;
      auto block = [&](std::vector<clap_event_note_t> notes) {
        EvList ev;
        ev.notes = std::move(notes);
        ev.finalize();
        clap_process_t proc{};
        proc.frames_count = 256;
        proc.audio_outputs = &out;
        proc.audio_outputs_count = 1;
        proc.in_events = &ev.list;
        proc.out_events = &kOut;
        p->process(p, &proc);
      };
      auto window = [&](int nblk) -> double {
        const double w = 2 * 3.14159265358979323846 * f0 / kSR, cw = 2 * std::cos(w);
        double s1 = 0, s2 = 0;
        long n = 0;
        for (int b = 0; b < nblk; b++)
        {
          std::vector<clap_event_note_t> evs;
          if (withArp && b % 18 == 0)
          {
            if (lastKey >= 0)
              evs.push_back(mkNote(CLAP_EVENT_NOTE_OFF, 0, (int16_t)lastKey, -1, 0.8));
            lastKey = 60 + (step % 8) * 2;
            evs.push_back(mkNote(CLAP_EVENT_NOTE_ON, 0, (int16_t)lastKey, nextId++, 0.8));
            step++;
          }
          block(std::move(evs));
          for (int i = 0; i < 256; i++)
          {
            const double s0 = L[i] + cw * s1 - s2;
            s2 = s1;
            s1 = s0;
            n++;
          }
        }
        return std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - cw * s1 * s2)) / n;
      };
      block({mkNote(CLAP_EVENT_NOTE_ON, 0, (int16_t)SUS, nextId++, 0.8)});
      window(40);
      double minP = 1e18;
      for (int seg = 0; seg < 12; seg++)
      {
        const double pw = window(40);
        if (pw < minP) minP = pw;
      }
      p->stop_processing(p);
      p->deactivate(p);
      p->destroy(p);
      return minP;
    };
    const double floorP = goertzelRun(false);
    const double arpP = goertzelRun(true);
    const bool ok = arpP > floorP * 0.3;
    std::printf("%s arp-sustain (ADR-083): held-f0 min %.3g vs no-arp floor %.3g\n",
                ok ? "OK  " : "FAIL", arpP, floorP);
    if (!ok) failures++;
  }

  std::printf(failures ? "notefuzz_check: RED (%d hangs)\n" : "notefuzz_check: GREEN (0 hangs)\n",
              failures);
  return failures ? 1 : 0;
}
