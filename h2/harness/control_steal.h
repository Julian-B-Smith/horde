/*
 * control_steal.h — the steal fade's planted fault. ROADMAP B459 (ADR-216 D7); added
 * after an independent critic deleted the fade's ramp from the engine and
 * tools/h2_steal_fade_check.mjs stayed green (2026-10-13): nothing judged that
 * "the old sound ends through a fade".
 *
 * NOT an engine, and never enrolled. tools/h2_steal_fade_check.mjs plays its FADE
 * job through this adapter on every run, reads it with the SAME measure as the
 * real engine, and is red unless this one reads as the fault it carries.
 *
 * THE FAULT: A STEAL WITH NO RAMP. With stealFade on, when a chord takes sounding
 * voices, the old sound must fade to nothing over kStealFade and the notes start
 * when it ends. The plant renders what an engine WITHOUT the ramp renders: the old
 * sound plays on at full level for the fade's length, is cut in one sample, and the
 * notes start. It is the critic's mutant (the ramp's three scaling lines removed).
 *
 * HOW, WITHOUT TOUCHING THE ENGINE. No engine can be asked from outside to skip its
 * ramp, and a second engine build would need a CMake target of its own. So the
 * plant holds TWO shipped engines, A and B, and gives both the same seed and the
 * same calls, so they are the same engine sample for sample. At the first note-on
 * that A turns into a take (A's public voice view shows one more waiting voice),
 * B is NOT given that note, nor any other note-on that takes at the same instant.
 * A take draws nothing from the engine's shared random stream, so B goes on as if
 * no chord had been struck: the old sound, unfaded. For as many samples as A's
 * fade has left the output is B's; from the sample A's fade ends the output is
 * A's, where the old sound is gone and the notes start from silence. B is then
 * dropped. Only the FIRST take of a job is planted; later ones are A's own.
 *
 * WHAT IT ASSUMES OF THE JOB, and what happens otherwise: stealFade is on (with it
 * off A never shows a waiting voice, nothing is planted, and the output is A's
 * whole: the check's control then reads clean and the check is red, as it should
 * be); and every note of the planted chord waits (a note that finds a free voice
 * is given to B too, and is heard at once in both).
 *
 * The pre-limiter bus is the composed adapter's (atanh of the output).
 */
#pragma once

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "../engine/engine.h"
#include "harness.h"

namespace horde2::harness {

class StealCutControl final : public Adapter {
 public:
  void prepare(double sampleRate, uint32_t seed) override {
    a = std::make_unique<horde2::engine::Engine>(sampleRate);
    b = std::make_unique<horde2::engine::Engine>(sampleRate);
    a->seedRandom(seed);
    b->seedRandom(seed);
    phase = kMirror;
    cutLeft = 0;
  }
  bool set(const char* key, double value) override { if (phase != kDone) b->set(key, value); return a->set(key, value); }
  bool setString(const char* key, const char* value) override { if (phase != kDone) b->setString(key, value); return a->setString(key, value); }
  void snap() override { a->snap(); if (phase != kDone) b->snap(); }
  void noteOn(int note, double freq, double vel) override {
    const int before = waiting();
    a->noteOn(note, freq, vel);
    if (phase == kDone) return;
    if (waiting() > before) phase = kArmed;   // A took a sounding voice: B never hears of this note
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
    // kArmed: the output is B's (the old sound, unfaded) for as long as A's fade has
    // left, then A's. The count is read at the first render after the take.
    if (cutLeft == 0) cutLeft = fadeLeft();
    const int m = n < cutLeft ? n : cutLeft;
    a->render(tl.data(), tr.data(), m);
    b->render(L, R, m);
    cutLeft -= m;
    if (cutLeft > 0) return;
    phase = kDone;
    b.reset();
    if (m < n) a->render(L + m, R + m, n - m);
  }
  double preLimiter(double out) const override { return std::atanh(out); }

 private:
  enum Phase { kMirror, kArmed, kDone };
  int waiting() const { int k = 0; for (int i = 0; i < horde2::engine::Engine::kVoices; i++) if (a->voice(i).wait) k++; return k; }
  // the longest fade A is running under a waiting note; 1 if none is left to read
  int fadeLeft() const {
    int k = 1;
    for (int i = 0; i < horde2::engine::Engine::kVoices; i++) if (a->voice(i).wait && a->voice(i).sfLeft > k) k = a->voice(i).sfLeft;
    return k;
  }
  std::unique_ptr<horde2::engine::Engine> a, b;
  std::vector<double> tl, tr;
  Phase phase = kMirror;
  int cutLeft = 0;
};

}  // namespace horde2::harness
