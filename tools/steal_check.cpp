/* steal_check — WHICH voice dies when the pool is full.
 *
 * FOUNDATIONS asked (seam brief question 1) whether oldest-logical is an
 * acceptable steal rule at kPoly=16. Answering it exposed that NOTHING here
 * pinned steal priority at all: `notefuzz_check` proves no voice HANGS, and it
 * proves that whether the victim is the oldest, the newest, or picked at
 * random. A seam that changed steal order would have left all sixteen gates
 * green. This is that gap.
 *
 * WHAT IS PINNED — the three tiers of `alloc()` (swarm_core.h), in priority
 * order, stated as behaviour rather than as code:
 *   1. a FREE slot is used before anything is stolen;
 *   2. a RELEASING tail is taken before any held note is;
 *   3. only when every slot is gated does the OLDEST held note die.
 * Tier 3 is the one that must agree with a logical-steal rule. Tiers 1-2 read
 * envelope state and are engine-private — they are pinned here as behaviour so
 * that a library seam cannot silently override them from the outside.
 *
 * FREQUENCY LAYOUT IS LOAD-BEARING (L0032). Every measured note lives within a
 * SINGLE OCTAVE (MIDI 60..71), so no measured fundamental can be a harmonic of
 * another measured note — a harmonic is at least 2x, which is outside the
 * octave by construction. The mixer_check probe reported mute as 67% broken
 * because it separated two oscillators by an octave and read one's 2nd harmonic
 * as the other's fundamental; the fix there was a tritone, and the general form
 * is this: choose the layout so the contamination is IMPOSSIBLE, not unlikely.
 * Notes 72..75 exist to fill the pool and are deliberately NOT measured — 72 is
 * exactly one octave above 60 and would read 60's second harmonic.
 * WIRED: ./verify full.
 */
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"

namespace {
#include "notefuzz_scaffold.inc"

constexpr double kPi = 3.14159265358979323846;   // no M_PI under MSVC (L0003)

// Single-bin magnitude at `freq`. Reading ONE bin (not a band) is what keeps a
// neighbouring semitone out of the measurement.
double goertzel(const std::vector<float> &x, double freq)
{
  /* HANN-WINDOWED, and that is not a detail. Unwindowed, this measures a
     SILENCED note's bin sitting 15.6 Hz from a neighbour still sounding at full
     amplitude, and a rectangular window's sidelobes put ~5% of that neighbour
     into the victim's bin — which read as "the stolen note is 5% still there".
     Measuring a weak residue beside a strong signal is the case that has fooled
     this project repeatedly (L0016's absorbed spectral case). */
  const size_t N = x.size();
  const double w = 2.0 * kPi * freq / kSR;
  const double c = 2.0 * std::cos(w);
  double s1 = 0, s2 = 0, wsum = 0;
  for (size_t i = 0; i < N; i++)
  {
    const double h = 0.5 - 0.5 * std::cos(2.0 * kPi * (double)i / (double)(N - 1));
    wsum += h;
    const double s0 = h * (double)x[i] + c * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  const double re = s1 - s2 * std::cos(w), im = s2 * std::sin(w);
  return 2.0 * std::sqrt(re * re + im * im) / (wsum > 0 ? wsum : 1.0);
}

double midiHz(int m) { return 440.0 * std::pow(2.0, (m - 69) / 12.0); }

int failures = 0;
void check(bool ok, const char *what, const char *detail)
{
  std::printf("%-6s %s  (%s)\n", ok ? "OK" : "FAIL", what, detail);
  if (!ok) failures++;
}
}  // namespace

int main()
{
  hypersaw_entry_init("");
  auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_t *p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
  p->init(p);
  p->activate(p, kSR, 32, 2048);
  p->start_processing(p);

  std::vector<float> L(kBlock), R(kBlock);
  float *chans[2] = {L.data(), R.data()};
  clap_audio_buffer_t out{};
  out.data32 = chans;
  out.channel_count = 2;
  int64_t blkIdx = 0;

  auto run = [&](EvList &e, int blocks, std::vector<float> *cap) {
    e.finalize();
    for (int b = 0; b < blocks; b++)
    {
      clap_process_t proc{};
      proc.audio_inputs_count = 0;
      proc.audio_outputs_count = 1;
      proc.audio_outputs = &out;
      proc.frames_count = kBlock;
      proc.in_events = &e.list;
      proc.out_events = &kOut;
      proc.steady_time = blkIdx++ * kBlock;
      p->process(p, &proc);
      e.notes.clear();
      e.params.clear();
      e.finalize();
      if (cap)
        for (int i = 0; i < kBlock; i++) cap->push_back(0.5f * (L[i] + R[i]));
    }
  };
  auto capture = [&](double seconds) {
    std::vector<float> v;
    EvList e;
    run(e, Math_blocks(seconds), &v);
    return v;
  };
  auto idle = [&](int blocks) {
    EvList e;
    run(e, blocks, nullptr);
  };
  auto send = [&](uint16_t type, int key) {
    EvList e;
    e.notes.push_back(mkNote(type, 0, (int16_t)key, -1));
    run(e, 1, nullptr);
  };

  /* One saw per note, no spread: n=1, detune=0, K=0. A swarm smears each note
     across many partials, which would put energy from one note into another's
     bin no matter how the pitches are laid out. Sustain full and release short
     so "gated" and "gone" are unambiguous. */
  auto reset = [&]() {
    p->reset(p);
    EvList e;
    e.params.push_back(mkParam(1, 1));       // n = 1 oscillator per swarm
    e.params.push_back(mkParam(4, 0.0));     // detune 0
    e.params.push_back(mkParam(6, 0.0));     // K = 0 (no coupling drift)
    e.params.push_back(mkParam(19, 0.001));  // attack: reach full quickly
    e.params.push_back(mkParam(20, 0.001));  // decay
    e.params.push_back(mkParam(21, 1.0));    // sustain
    e.params.push_back(mkParam(22, 0.005));  // release: short
    e.params.push_back(mkParam(32, 0.0));    // poly
    run(e, 4, nullptr);
  };

  char d[240];
  const int kFill = 16;   // kPoly

  // ---- 1. TIER 3: with every slot gated, the OLDEST held note dies ---------
  {
    reset();
    for (int i = 0; i < kFill; i++) { send(CLAP_EVENT_NOTE_ON, 60 + i); idle(2); }
    const std::vector<float> before = capture(0.25);
    const double oldestBefore = goertzel(before, midiHz(60));

    /* The 17th note is ABOVE the measured octave, not below. MIDI 59 was the
       first choice and is wrong: its 2nd harmonic lands exactly on MIDI 71,
       which assertion 1 counts as a survivor — the probe would have been
       reading the intruder as proof the victim's neighbour lived. Harmonics
       only go up, so a note above the range cannot reach into it. */
    send(CLAP_EVENT_NOTE_ON, 76);
    idle(16);   // let the stolen voice finish dying BEFORE measuring
    const std::vector<float> after = capture(0.25);
    const double oldestAfter = goertzel(after, midiHz(60));

    /* THE FLOOR IS MEASURED, NOT GUESSED. A silenced bin never reads zero — its
       neighbours leak into it — so "how quiet is gone?" has to come from a
       render where the note genuinely never sounded. Play the same chord minus
       MIDI 60 and read f(60): whatever that is IS the floor, and the victim
       must land at it. A hand-picked 5% threshold gave 5.1% and would have been
       "marginal"; measuring the floor turns a judgement call into a number
       (L0024 — a result at its threshold means the detector is wrong). */
    reset();
    for (int i = 1; i < kFill; i++) { send(CLAP_EVENT_NOTE_ON, 60 + i); idle(2); }
    send(CLAP_EVENT_NOTE_ON, 76);
    idle(16);
    const std::vector<float> neverPlayed = capture(0.25);
    const double floor60 = goertzel(neverPlayed, midiHz(60));

    std::snprintf(d, sizeof(d), "oldest (midi 60) %.5f -> %.5f; leakage floor from a "
                                "render where 60 never sounded %.5f",
                  oldestBefore, oldestAfter, floor60);
    check(oldestBefore > 0.01 && oldestAfter < 3.0 * floor60 + 1e-4,
          "pool full: the OLDEST held note is the one stolen", d);

    // ...and every younger measurable note survives it. Only 61..71 are read:
    // 72..75 are contaminated by lower notes' harmonics by construction.
    int survivors = 0;
    double worst = 1e9;
    for (int m = 61; m <= 71; m++)
    {
      const double v = goertzel(after, midiHz(m));
      if (v > 0.01) survivors++;
      if (v < worst) worst = v;
    }
    std::snprintf(d, sizeof(d), "%d/11 of midi 61..71 still sounding, quietest %.5f", survivors, worst);
    check(survivors == 11, "and every YOUNGER held note survives the steal", d);
  }

  // ---- 2. TIER 2 BEATS TIER 3: a releasing tail dies before a held note ----
  {
    reset();
    for (int i = 0; i < kFill; i++) { send(CLAP_EVENT_NOTE_ON, 60 + i); idle(2); }
    // Release the OLDEST. It is now a decaying tail — and it is also exactly
    // who tier 3 would pick, so if tier 2 did not exist this test could not
    // tell the two apart. Release a YOUNGER note instead: now the tier-2 victim
    // (the tail) and the tier-3 victim (oldest = 60) are DIFFERENT notes, and
    // the assertion discriminates.
    /* A LONG release is load-bearing here. The first version left release at
       5 ms and idled 46 ms before stealing — by which point the released voice
       had faded below the free threshold, so its slot was tier-1 FREE, not
       tier-2 RELEASING. The assertion passed through the wrong tier and would
       have stayed green with tier 2 deleted. Stretch the release so the tail is
       genuinely still decaying at the moment of the steal. */
    {
      EvList e;
      e.params.push_back(mkParam(22, 0.8));   // release 800 ms
      run(e, 1, nullptr);
    }
    send(CLAP_EVENT_NOTE_OFF, 65);
    idle(8);        // ~46 ms in: audibly decaying, nowhere near free

    send(CLAP_EVENT_NOTE_ON, 76);            // 17th note arrives
    idle(16);
    const std::vector<float> after = capture(0.25);

    const double oldestStillThere = goertzel(after, midiHz(60));
    const double releasedGone = goertzel(after, midiHz(65));

    // Same measured floor, for the same reason: 0.005 was a guess, and it sits
    // BELOW the leakage from notes 64 and 66 — an unmeetable bar that would have
    // read as a broken tier 2. Render the cluster with 65 never sounding.
    reset();
    for (int i = 0; i < kFill; i++) { if (60 + i != 65) { send(CLAP_EVENT_NOTE_ON, 60 + i); idle(2); } }
    idle(16);
    const std::vector<float> without65 = capture(0.25);
    const double floor65 = goertzel(without65, midiHz(65));

    std::snprintf(d, sizeof(d), "oldest-held (60) %.5f survives; released (65) %.5f vs "
                                "leakage floor %.5f",
                  oldestStillThere, releasedGone, floor65);
    check(oldestStillThere > 0.01 && releasedGone < 3.0 * floor65 + 1e-4,
          "a RELEASING tail is stolen before any held note", d);
  }

  // ---- 3. TIER 1: a free slot is used before anything is stolen ------------
  {
    reset();
    for (int i = 0; i < 4; i++) { send(CLAP_EVENT_NOTE_ON, 60 + i); idle(2); }
    send(CLAP_EVENT_NOTE_ON, 64);            // pool has room: nothing may die
    const std::vector<float> after = capture(0.25);
    int alive = 0;
    for (int m = 60; m <= 64; m++) if (goertzel(after, midiHz(m)) > 0.01) alive++;
    std::snprintf(d, sizeof(d), "%d/5 notes sounding with 11 slots free", alive);
    check(alive == 5, "with free slots available, no note is stolen at all", d);
  }

  /* CALIBRATION — recorded because a green suite proves nothing on its own.
     Planting `alloc()`'s tier 3 to steal the NEWEST instead of the oldest
     (reverse the age comparison) makes assertion 1 FAIL; removing tier 2 so a
     releasing tail is never preferred makes assertion 2 FAIL while 1 and 3 stay
     green, which is what shows the two tiers are pinned SEPARATELY.
     Force the rebuild between plants — delete the impl object. Asserting a
     plant's anchor proves the SOURCE changed, never that the BINARY did
     (L0032, corrected 2026-08-11 after a plant reported the previous plant's
     failures verbatim). */
  p->stop_processing(p);
  p->deactivate(p);
  p->destroy(p);
  std::printf("steal_check: %s (%d failures)\n", failures ? "RED" : "GREEN", failures);
  return failures ? 1 : 0;
}
