/*
 * control_steal.h — the steal fade's planted faults. ROADMAP B459 (ADR-216 D7).
 *
 * WHY THEY EXIST. Two independent critics each put a broken fade into the engine and
 * tools/h2_steal_fade_check.mjs stayed green (2026-10-13). The first deleted the ramp:
 * nothing judged that "the old sound ends through a fade". The second kept the ramp
 * for four samples and then cut the voice, and stopped the ramp short of zero: the
 * check read the first four samples and the last twenty-four, and nothing between.
 * A row that has never been seen to fail proves nothing, so each row of that check
 * that judges the fade's shape or the start has a plant here that it must read as
 * a fault, on every run.
 *
 * NOT engines, and never enrolled. The check plays a job through one of these
 * adapters, reads it with the SAME measure as the real engine, and is red unless
 * the plant reads as the fault it carries.
 *
 *   id                    the fault                                   the row that must see it
 *   control/steal-cut     NO RAMP: the old sound plays on at full     FADE
 *                         level for the fade's length, then is cut
 *   control/steal-cut5    A CUT INSIDE: the ramp is right for four    RAMP (and not JUMP, which
 *                         samples, then the output is nothing until   reads those four samples:
 *                         the fade's end                              that is the point)
 *   control/steal-short   A RAMP THAT STOPS SHORT: right until its    RAMP (and not FADE, whose
 *                         gain is kShortLeft/N, held there, then cut  ceiling it sits under)
 *   control/steal-step    A START THAT STEPS: the fade is right and   END (LATE reads it on time)
 *                         the notes start on time, at the OLD
 *                         envelope's level instead of from silence
 *   control/steal-early   A START THAT IS NOT LATE ENOUGH: the same   LATE
 *                         start one sample before the fade's end
 * The first three are the critics' mutants (the ramp's scaling lines removed; `left <
 * N - 4 ? 0 : ratio`; `left < 15 ? 1 : ratio`). The check's header gives what each
 * read on the mutant engine itself.
 *
 * HOW, WITHOUT TOUCHING THE ENGINE. No engine can be asked from outside to skip its
 * ramp, and a second engine build would need a CMake target of its own. So a plant
 * holds TWO shipped engines, A and B, and gives both the same seed and the same
 * calls, so they are the same engine sample for sample. At the first note-on that
 * A turns into a take (A's public voice view shows one more waiting voice), B is
 * NOT given that note, nor any other note-on that takes at the same instant. A
 * take draws nothing from the engine's shared random stream, so B goes on as if no
 * chord had been struck: the old sound, unfaded. Then, over the N samples A's fade
 * has left (k counts them from 0):
 *   cut     the output is B's; from sample N it is A's, and B is dropped.
 *   cut5    A's for k < 4, zero from there to N, then A's.
 *   short   A's while its gain (N - k) / N is at least kShortLeft / N; after that
 *           B's, scaled to kShortLeft / N on the pre-limiter bus; then A's.
 *   step    A's (the fade as it should be). At sample N, B's stealFade is set to 0
 *           and B is given the withheld note-ons: it restarts its voices, which
 *           still sound at full level, as the present law does. From there the
 *           output is B's and A is dropped.
 *   early   as step, one sample sooner (N - 1).
 * Only the FIRST take of a job is planted; later ones are the surviving engine's own.
 *
 * WHAT THEY ASSUME OF THE JOB, and what happens otherwise: stealFade is on (with it
 * off A never shows a waiting voice, nothing is planted, and the output is A's
 * whole: the check's control then reads clean and the check is red, as it should
 * be); every note of the planted chord waits (a note that finds a free voice is
 * given to B too, and is heard at once in both); and the pool is taken whole, so
 * that nothing but the old sound is in A's output during the fade (cut5 writes
 * zeros there).
 *
 * The pre-limiter bus is the composed adapter's (atanh of the output).
 */
#pragma once

#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "../engine/engine.h"
#include "harness.h"

namespace horde2::harness {

class StealPlantControl final : public Adapter {
 public:
  enum Plant { kCut, kCut5, kShort, kStep, kEarly };
  // cut5: the samples that are right before the cut. It is the check's kJumpSamples
  // on purpose: JUMP reads exactly these and must NOT see this plant.
  static constexpr int kCutAt = 4;
  // short: the ramp's gain stops at kShortLeft / N (the critic's mutant: 15 of 240)
  static constexpr int kShortLeft = 15;

  explicit StealPlantControl(Plant p) : plant(p) {}
  void prepare(double sampleRate, uint32_t seed) override {
    a = std::make_unique<horde2::engine::Engine>(sampleRate);
    b = std::make_unique<horde2::engine::Engine>(sampleRate);
    a->seedRandom(seed);
    b->seedRandom(seed);
    phase = kMirror;
    total = done = 0;
    held.clear();
  }
  bool set(const char* key, double value) override { if (phase != kDone) b->set(key, value); return a->set(key, value); }
  bool setString(const char* key, const char* value) override { if (phase != kDone) b->setString(key, value); return a->setString(key, value); }
  void snap() override { a->snap(); if (phase != kDone) b->snap(); }
  void noteOn(int note, double freq, double vel) override {
    if (phase == kDone) { a->noteOn(note, freq, vel); return; }
    const int before = waiting();
    a->noteOn(note, freq, vel);
    if (waiting() > before) { phase = kArmed; held.push_back({note, freq, vel}); }   // A took a sounding voice: B is not told (step, early: not yet)
    else b->noteOn(note, freq, vel);
  }
  void noteOff(int note) override { a->noteOff(note); if (phase != kDone) b->noteOff(note); }
  void panic() override { a->panic(); if (phase != kDone) b->panic(); }
  void render(double* L, double* R, int n) override {
    if (phase == kDone) { a->render(L, R, n); return; }
    if (static_cast<int>(tl.size()) < n) { tl.resize(n); tr.resize(n); }
    if (phase == kMirror) {   // B keeps step with A, unheard
      a->render(L, R, n);
      b->render(tl.data(), tr.data(), n);
      return;
    }
    // kArmed: the planted stretch. Its length is read at the first render after the take.
    if (total == 0) total = fadeLeft();
    const int end = total - (plant == kEarly ? 1 : 0);
    const int m = n < end - done ? n : end - done;
    a->render(L, R, m);
    b->render(tl.data(), tr.data(), m);
    for (int i = 0; i < m; i++) {
      const int k = done + i;
      if (plant == kCut) { L[i] = tl[i]; R[i] = tr[i]; }
      else if (plant == kCut5) { if (k >= kCutAt) L[i] = R[i] = 0; }
      else if (plant == kShort && total - k < kShortLeft) {
        const double g = static_cast<double>(kShortLeft) / total;
        L[i] = std::tanh(std::atanh(tl[i]) * g);
        R[i] = std::tanh(std::atanh(tr[i]) * g);
      }
    }
    done += m;
    if (done < end) return;
    phase = kDone;
    if (plant == kStep || plant == kEarly) {
      // B starts the chord now, by the present law: its voices still sound at full level
      b->set("stealFade", 0);
      for (const Held& h : held) b->noteOn(h.note, h.freq, h.vel);
      a = std::move(b);
    }
    b.reset();
    if (m < n) a->render(L + m, R + m, n - m);
  }
  double preLimiter(double out) const override { return std::atanh(out); }

 private:
  enum Phase { kMirror, kArmed, kDone };
  struct Held { int note; double freq, vel; };
  int waiting() const { int k = 0; for (int i = 0; i < horde2::engine::Engine::kVoices; i++) if (a->voice(i).wait) k++; return k; }
  // the longest fade A is running under a waiting note; 1 if none is left to read
  int fadeLeft() const {
    int k = 1;
    for (int i = 0; i < horde2::engine::Engine::kVoices; i++) if (a->voice(i).wait && a->voice(i).sfLeft > k) k = a->voice(i).sfLeft;
    return k;
  }
  const Plant plant;
  std::unique_ptr<horde2::engine::Engine> a, b;   // after kDone: `a` is the one engine left
  std::vector<double> tl, tr;
  std::vector<Held> held;   // the planted chord's note-ons (a control adapter: not a real-time path)
  Phase phase = kMirror;
  int total = 0, done = 0;   // the planted stretch in samples, and how many are rendered
};

}  // namespace horde2::harness
