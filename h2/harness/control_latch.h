/*
 * control_latch.h — the cut latch's planted faults and its probe. ROADMAP B487
 * (ADR-216 D9). THE FAULT BUILD ONLY: this file compiles to nothing unless
 * H2_ENGINE_FAULTS is defined, which it is in h2_harness_render_faults (CMakeLists.txt)
 * and in no shipped or harness build. The shipped harness, h2_harness_render, never
 * lists these adapters.
 *
 * WHY THEY EXIST. tools/h2_cut_latch_check.mjs judges that with `cutLatch` 1 a quantized
 * cut reaches each blade only at that blade's next cycle start (engine.h latchCut,
 * latchStep). A row that has never been seen to fail proves nothing (LIBRARY L0032;
 * control_spread.h and control_steal.h are the precedents), so every way the latch can
 * be wrong has a plant here that the check must read as that fault on every run. And
 * ADR-216 A4 item 15 found that a probe of the engine's FIELDS cannot see a blade FED
 * something else; so the probe here reads what each blade is fed, through the fault
 * build's tap (Engine::cutTap: the cut stepBlade receives, and bx.k, on every step).
 *
 * WHY A FAULT BUILD, AND NOT THE SHIPPED ENGINE THROUGH ITS PUBLIC VIEW (control_spread.h's
 * way). No outside caller can make the latch apply a step late or early, on one blade, or
 * never, and the cut a blade is fed (the member override) is private. Every plant is a
 * fault number in engine.h (15 to 21), compiled only under H2_ENGINE_FAULTS, where the
 * shipped build folds each site to `false`. The check shows on every run that this
 * build at fault 0 renders what the shipped harness renders, bit for bit.
 *
 *   id                        the engine, at fault
 *   control/latch-f0          0: nothing planted (the engine itself, in this build)
 *   control/latch-f15         15: THE LATCH IGNORED: a pending cut is taken on every step,
 *                             so at the first step after the tick (the present law)
 *   control/latch-f16         16: ONE STEP LATE: taken on the step after the cycle start
 *   control/latch-f17         17: ONE STEP EARLY: taken on the step before it
 *   control/latch-f18         18: BLADE 1 ONLY: blade 2's cut is set at the tick, at once
 *   control/latch-f19         19: BLADE 2 ONLY: blade 1's cut is set at the tick, at once
 *   control/latch-f20         20: NEVER RELEASED: the cut stays where the note set it
 *   control/latch-f21         21: FED STALE: the fields latch as designed, but each blade is
 *                             fed the law's latest cut (what an engine that latched its
 *                             fields and fed the blades from elsewhere would do)
 *   control/latch-probe       THE PROBE (not a sound), at fault 0; and
 *   control/latch-probe-f<n>  the same probe on the engine at fault n (15 to 21)
 *
 * THE PROBE. Two engines on one job, rendered one sample at a time: A with the job's
 * settings (at the probe's fault), B the same with `cutLatch` forced to 0 and no fault.
 * Each has a tap that receives, on every member step, the cut each blade is FED and where
 * that step's phase stands in each blade's window, from the raw phase, increment and
 * window (engine.h tapCut, apart from the latch's own arithmetic). A step STARTS A CYCLE
 * on a blade when its place in that blade's window is under one step's increment
 * (e1 < dphi): the window's start was passed on this step, and the carrier phase there
 * is kk·e1, restarted from 0. (In exact arithmetic that is the blade law's own entry
 * test, e1 < e0, which the latch uses: the two agree by construction, and the probe is
 * a reading of the PROPERTY, that a fed cut never changes while the carrier is
 * mid-cycle, not an independent detector of some other event. What makes it able to
 * fail is that it reads what the blade is fed, and the plants below.) A cut a blade is fed "changes" when it differs, bit for
 * bit, from what that member's blade was fed on its previous step; a voice that starts
 * a note (its `age` moves) starts its members afresh, so a note's first cut is no change.
 * Readings, per blade b (0 is blade 1, 1 is blade 2), each a running count since prepare():
 *   slot 0 + b   A's changes ON a cycle-start step
 *   slot 2 + b   A's changes on any OTHER step (0 for a latch that is right)
 *   slot 4 + b   B's changes (the law's flips, as the present law feeds them)
 *   slot 6 + b   B's changes on a step that is not a cycle start
 *   slot 8 + b   cycle-start steps of A at which A is fed another cut than B on that same
 *                step (0 for a latch that is right: at a cycle start the latched cut IS the
 *                law's latest, which is what B is fed)
 *   slot 10 + b  A's cycle-start steps compared
 *   slot 12 + b  the longest run of consecutive steps of one member on which A and B are
 *                fed different cuts on that blade (the latch's worst delay, in steps)
 *   slot 14      the largest difference between A's and B's spread slots (rv) and phases
 *                over every sounding member, after every sample (0: one walk)
 *   slot 15      members compared, summed over samples (0 would mean nothing was compared)
 * At sample n the left channel carries slot n mod 64 (0 beyond slot 15) and the right
 * channel the slot number, so the check reads the last 64 samples. The two engines walk
 * the same spread only on a job where nothing but the law draws from the host stream (no
 * noise blade): slot 14 says whether they did.
 *
 * The pre-limiter bus of the plants is the composed adapter's (atanh of the output); the
 * probe's samples are readings and are returned as they are.
 */
#pragma once

#ifdef H2_ENGINE_FAULTS

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>

#include "../engine/engine.h"
#include "harness.h"

namespace horde2::harness {

class LatchPlantControl final : public Adapter {
 public:
  explicit LatchPlantControl(int fault) : fault(fault) {}
  void prepare(double sampleRate, uint32_t seed) override {
    e = std::make_unique<horde2::engine::Engine>(sampleRate);
    e->seedRandom(seed);
    e->fault = fault;
  }
  bool set(const char* key, double value) override { return e->set(key, value); }
  bool setString(const char* key, const char* value) override { return e->setString(key, value); }
  void snap() override { e->snap(); }
  void noteOn(int note, double freq, double vel) override { e->noteOn(note, freq, vel); }
  void noteOff(int note) override { e->noteOff(note); }
  void panic() override { e->panic(); }
  void render(double* L, double* R, int n) override { e->render(L, R, n); }
  double preLimiter(double out) const override { return std::atanh(out); }

 private:
  const int fault;
  std::unique_ptr<horde2::engine::Engine> e;
};

class LatchProbeControl final : public Adapter {
 public:
  static constexpr int kSlots = 64;
  explicit LatchProbeControl(int fault) : fault(fault) {}
  void prepare(double sampleRate, uint32_t seed) override {
    for (int i = 0; i < 2; i++) {
      e[i] = std::make_unique<horde2::engine::Engine>(sampleRate);
      e[i]->seedRandom(seed);
      tap[i] = Tap{};
      e[i]->cutTap = &tap[i];
    }
    e[0]->fault = fault;
    for (double& x : r) x = 0;
    for (auto& v : runs) for (auto& m : v) for (int& x : m) x = 0;
    for (double& a : age) a = -1;
    pos = 0;
  }
  bool set(const char* key, double value) override {
    const bool known = e[0]->set(key, value);
    e[1]->set(key, std::strcmp(key, "cutLatch") == 0 ? 0 : value);
    return known;
  }
  bool setString(const char* key, const char* value) override { const bool k = e[0]->setString(key, value); e[1]->setString(key, value); return k; }
  void snap() override { for (auto& x : e) x->snap(); }
  void noteOn(int note, double freq, double vel) override { for (auto& x : e) x->noteOn(note, freq, vel); }
  void noteOff(int note) override { for (auto& x : e) x->noteOff(note); }
  void panic() override { for (auto& x : e) x->panic(); }
  void render(double* L, double* R, int n) override {
    double x, y;
    for (int i = 0; i < n; i++, pos++) {
      for (int k = 0; k < 2; k++) { tap[k].n = 0; e[k]->render(&x, &y, 1); }
      score();
      const int s = static_cast<int>(pos % kSlots);
      L[i] = s < kRead ? r[s] : 0;
      R[i] = s;
    }
  }
  double preLimiter(double out) const override { return out; }

 private:
  static constexpr int kRead = 16, kSteps = 8 * horde2::engine::kMembers * 8;   // os up to 8
  struct Step { int voice, member; double k[2]; bool b2; bool start[2]; };
  struct Tap final : horde2::engine::Engine::CutTap {
    Step s[kSteps];
    int n = 0;
    void step(int voice, int member, double k1, double k2, bool b2, double e1, double a1, double dphi) override {
      if (n >= kSteps) return;
      s[n++] = Step{voice, member, {k1, k2}, b2, {e1 < dphi, b2 && a1 < dphi}};
    }
  };
  // one sample's steps of both engines, in the same order (the same voices sound in both)
  void score() {
    using horde2::engine::Engine;
    for (int v = 0; v < Engine::kVoices; v++) {   // a voice that starts a note starts its members afresh
      const double a = e[0]->voice(v).age;
      if (a != age[v]) { age[v] = a; for (int q = 0; q < horde2::engine::kMembers; q++) for (int b = 0; b < 2; b++) { seen[0][v][q][b] = seen[1][v][q][b] = false; runs[v][q][b] = 0; } }
    }
    const int n = tap[0].n < tap[1].n ? tap[0].n : tap[1].n;
    if (tap[0].n != tap[1].n) r[14] = INFINITY;   // not even the same steps
    for (int i = 0; i < n; i++) {
      const Step& A = tap[0].s[i];
      const Step& B = tap[1].s[i];
      if (A.voice != B.voice || A.member != B.member) { r[14] = INFINITY; continue; }
      for (int b = 0; b < 2; b++) {
        if (b == 1 && !A.b2) continue;
        const int v = A.voice, q = A.member;
        double& pa = last[0][v][q][b];
        double& pb = last[1][v][q][b];
        if (seen[0][v][q][b] && A.k[b] != pa) r[A.start[b] ? 0 + b : 2 + b]++;
        if (seen[1][v][q][b] && B.k[b] != pb) { r[4 + b]++; if (!B.start[b]) r[6 + b]++; }
        if (A.start[b]) { r[10 + b]++; if (A.k[b] != B.k[b]) r[8 + b]++; }
        int& run = runs[v][q][b];
        run = A.k[b] != B.k[b] ? run + 1 : 0;
        if (run > r[12 + b]) r[12 + b] = run;
        pa = A.k[b]; pb = B.k[b];
        seen[0][v][q][b] = seen[1][v][q][b] = true;
      }
    }
    // the walk: A and B's spread slots and phases after this sample
    for (int v = 0; v < Engine::kVoices; v++) {
      const horde2::engine::Voice& a = e[0]->voice(v);
      const horde2::engine::Voice& b = e[1]->voice(v);
      if (a.active != b.active) r[14] = INFINITY;
      if (!a.active || !b.active) continue;
      for (int q = 0; q < horde2::engine::kMembers; q++) {
        r[15]++;
        for (int j = 0; j < 14; j++) { const double d = std::fabs(a.m[q].rv[j] - b.m[q].rv[j]); if (!(d <= r[14])) r[14] = d; }
        const double d = std::fabs(a.m[q].phi - b.m[q].phi);
        if (!(d <= r[14])) r[14] = d;
      }
    }
  }
  const int fault;
  std::unique_ptr<horde2::engine::Engine> e[2];
  Tap tap[2];
  double r[kRead] = {};
  double age[horde2::engine::Engine::kVoices] = {};
  double last[2][horde2::engine::Engine::kVoices][horde2::engine::kMembers][2] = {};
  bool seen[2][horde2::engine::Engine::kVoices][horde2::engine::kMembers][2] = {};
  int runs[horde2::engine::Engine::kVoices][horde2::engine::kMembers][2] = {};
  uint64_t pos = 0;
};

}  // namespace horde2::harness

#endif  // H2_ENGINE_FAULTS
