/*
 * adapter_composed.h — the harness adapter for horde 2's composed engine
 * (h2/engine/, `horde2::engine::Engine`). ROADMAP B448 phase 2, P2 (ADR-209).
 *
 * It goes through the engine's PUBLIC interface only: Engine(sampleRate),
 * seedRandom, set, setString, snap, noteOn, noteOff, panic, render. Nothing in
 * h2/engine/ is edited or re-declared, and no test hook is switched on
 * (H2_ENGINE_FAULTS, H2_ENGINE_STAGES and H2_ENGINE_KERNELS stay undefined), so
 * this is the engine as it ships.
 *
 * THE PRE-LIMITER BUS, AND ITS LIMITS. The engine has no accessor for the value
 * that enters its output limiter. Its last line is
 *     L[i] = std::tanh(yl * s.gain * 1.6)          (engine.h, the end of renderCallK)
 * and tanh is one-to-one below its rail, so preLimiter() returns atanh(out): the
 * limiter's input, AFTER the gain and the DC blocker. What that can and cannot say:
 *   - Below the rail it is the input to within rounding. The error grows toward
 *     the rail, because near 1 a double has few bits left for the distance from 1.
 *     Measured 2026-10-10 on darwin-arm64 (`h2_harness_render --pre-roundtrip`):
 *     1.3e-15 at |pre| = 3, 6e-13 at 6, 4.5e-9 at 10, 8e-5 at 15, 0.13 at 18.5.
 *     h2/harness/selftest.mjs re-measures the first three on every run.
 *   - On the rail (|out| = 1 exactly; measured: 19 is below it, 19.2 is on it) the
 *     input is lost and this returns +-infinity. Read that as "at least 19", never
 *     as a number. A NaN output stays NaN.
 *   - While a culled voice fades (the voice cap; renderCalls), the engine passes
 *     each output sample through a float, as the JS it mirrors does. A float has
 *     24 bits, so there the rail is reached from about |pre| = 9 and the error is
 *     larger by the same ratio (ARGUED from the format, not measured: the harness
 *     does not expose the voice cap, so no harness render takes that path).
 *     A steal's fade (ADR-216 D7, `stealFade` 1) takes the same one call per sample
 *     and harness renders DO take it (tools/h2_steal_fade_check.mjs), but its output
 *     is not passed through a float: only a culled voice's fade is.
 * So a bound of a few units on the pre-limiter bus is tested exactly enough, and
 * a blow-up reads as infinity, which no bound passes. A tool that needs the bus
 * beyond that (its true size when it is far above 19, or it before the DC
 * blocker) needs an engine accessor, which this package was told not to add.
 *
 * NOT EXPOSED, because no caller needs them yet: retune, the voice cap and its
 * policy, the load readouts (culled, refused, stolen) and the per-voice state.
 */
#pragma once

#include <cmath>
#include <cstdint>
#include <memory>

#include "../engine/engine.h"
#include "harness.h"

namespace horde2::harness {

class ComposedAdapter final : public Adapter {
 public:
  // The engine is about 114 KB and its members point into it (it is not copyable),
  // so each prepare() builds a new one on the heap.
  void prepare(double sampleRate, uint32_t seed) override {
    e = std::make_unique<horde2::engine::Engine>(sampleRate);
    e->seedRandom(seed);
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
  std::unique_ptr<horde2::engine::Engine> e;
};

}  // namespace horde2::harness
