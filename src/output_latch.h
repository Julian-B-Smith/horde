/*
 * output_latch.h — the output guard that LATCHES AND REPORTS (B448 B1, ADR-197,
 * docs/strategy/blind-spot-armor.md risk row 3).
 *
 * The last line before the host's bus used to zero a non-finite sample and say
 * nothing (B446, hypersaw::zeroNonFinite). A silent zero is the failure mode
 * the armor catalogue names "symptom clamping": the audio is repaired, the bug
 * that produced the NaN is invisible, and a DAW session can run for hours on a
 * plugin that is quietly replacing samples. The rule is that an output safety
 * guard is allowed only if it latches and reports and never clamps silently.
 *
 * WHAT THIS ADDS: counts and a latch, read from any thread. WHAT IT DOES NOT
 * CHANGE: the output. The repair is still zeroNonFinite (src/input_guards.h,
 * reused rather than re-implemented so there is one finite check in the tree),
 * which writes only non-finite samples, so every parity, golden and self-digest
 * stays bit-identical.
 *
 * REAL-TIME SAFE: relaxed std::atomic, no allocation, no lock. The counters are
 * touched only when a block actually had a hit, so the clean path costs the two
 * scans zeroNonFinite always did and nothing more. Relaxed ordering is enough:
 * the three fields are independent diagnostics, not a publication protocol.
 *
 * THE LATCH CLEARS ONLY BY reset(). Not on a clean block, not on activate: a
 * latch that heals itself reports nothing to a probe that looks a second late.
 *
 * Dependency-free (standard library plus input_guards.h) so the plugin and
 * tools/nan_latch_check.cpp compile the SAME code.
 */
#pragma once

#include <atomic>
#include <cstdint>

#include "input_guards.h"

namespace hypersaw
{

class NonFiniteLatch
{
 public:
  /* Zero every non-finite sample in both channels, count what was replaced, set
     the latch on the first event. Returns the samples replaced this call. */
  uint32_t guard(float *left, float *right, uint32_t n)
  {
    const uint32_t hits = zeroNonFinite(left, n) + zeroNonFinite(right, n);
    if (hits != 0)
    {
      samples_.fetch_add(hits, std::memory_order_relaxed);
      blocks_.fetch_add(1, std::memory_order_relaxed);
      latched_.store(true, std::memory_order_relaxed);
    }
    return hits;
  }

  /* Non-finite output samples replaced since the last reset. */
  uint64_t samples() const { return samples_.load(std::memory_order_relaxed); }
  /* Blocks that had at least one such sample since the last reset. */
  uint64_t blocks() const { return blocks_.load(std::memory_order_relaxed); }
  /* True from the first event until reset(). */
  bool latched() const { return latched_.load(std::memory_order_relaxed); }

  void reset()
  {
    samples_.store(0, std::memory_order_relaxed);
    blocks_.store(0, std::memory_order_relaxed);
    latched_.store(false, std::memory_order_relaxed);
  }

 private:
  std::atomic<uint64_t> samples_{0};
  std::atomic<uint64_t> blocks_{0};
  std::atomic<bool> latched_{false};
};

}  // namespace hypersaw
