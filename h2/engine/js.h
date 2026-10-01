/*
 * js.h — JavaScript number semantics and the repo's one RNG, for horde 2's
 * composed engine (h2/engine/, ROADMAP B385; design: docs/port/h2-engine.md).
 *
 * The engine's parity target is JavaScript (ADR-187 item 3), so wherever a std::
 * function "looks the same" as its JS counterpart but is not, the engine calls
 * these instead:
 *   - Math.round is ported as floor(x + 0.5), not std::round, which rounds
 *     negative halves away from zero (Math.round(-2.5) is -2, std::round(-2.5)
 *     is -3). floor(x + 0.5) itself differs from Math.round where the addition
 *     rounds: at 0.49999999999999994 (it gives 1, Math.round 0) and at odd
 *     integers from 2^52 up (it gives x + 1). No argument the engine rounds can
 *     be either: they are spreads, rotation offsets, rule indices, tempo-grid
 *     steps and the gravity grid, all small and computed, never those values.
 *   - Math.min/Math.max return NaN if either operand is NaN, and order ±0.
 *     std::min/max return an operand that depends on argument order.
 *   - A `switch` on a JS number is strict equality: a non-integral selector
 *     matches no case. A C++ int cast would truncate 1.5 into case 1.
 *   - JS truthiness of a number is "non-zero and not NaN". C++ reads NaN as true.
 *   - Math.pow(±1, ±Infinity) and Math.pow(x, NaN) are NaN in JS, 1 in C.
 *   - `x | 0` is ToInt32: truncate, then wrap modulo 2^32.
 * libm itself is NOT matched: V8 carries its own sin, exp, log and the rest, and
 * they can differ from the platform's in the last bit (LIBRARY L0066). The parity
 * check measures that; nothing here hides it.
 */
#pragma once

#include <climits>
#include <cmath>
#include <cstdint>

namespace horde2::engine {

namespace js {
inline double round(double x) { return std::floor(x + 0.5); }
inline double max(double a, double b) {
  if (std::isnan(a) || std::isnan(b)) return std::nan("");
  if (a > b) return a;
  if (b > a) return b;
  return std::signbit(a) ? b : a;   // equal: +0 over -0
}
inline double min(double a, double b) {
  if (std::isnan(a) || std::isnan(b)) return std::nan("");
  if (a < b) return a;
  if (b < a) return b;
  return std::signbit(a) ? a : b;   // equal: -0 over +0
}
inline double sign(double x) {
  if (std::isnan(x)) return x;
  if (x > 0) return 1;
  if (x < 0) return -1;
  return x;                          // ±0 keeps its sign
}
inline bool truthy(double x) { return x != 0 && !std::isnan(x); }
// switch selector: an integral value selects its case; anything else matches none
inline int sel(double x) {
  return (x == std::floor(x) && std::fabs(x) < 1e9) ? static_cast<int>(x) : INT_MIN;
}
inline double pow(double x, double y) {
  if (std::isnan(y)) return std::nan("");
  if (std::fabs(x) == 1 && std::isinf(y)) return std::nan("");
  return std::pow(x, y);
}
inline double frac(double x) { return x - std::floor(x); }
inline int32_t toInt32(double x) {
  if (!std::isfinite(x)) return 0;
  double m = std::fmod(std::trunc(x), 4294967296.0);
  if (m < 0) m += 4294967296.0;
  return static_cast<int32_t>(static_cast<uint32_t>(m));
}
}  // namespace js

// mulberry32, the repo's one RNG (SPEC §5.7). Unsigned 32-bit wraparound is
// exactly Math.imul and the `| 0` / `>>> 0` coercions of the JS form.
struct Mulberry32 {
  uint32_t a = 0;
  double next() {
    a = a + 0x6D2B79F5u;
    uint32_t t = (a ^ (a >> 15)) * (1u | a);
    t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
    return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
  }
};

// Blade-event sink for the parity check. kind: 1 edge/base BLEP (tryE), 2 carrier
// BLEP (scan), 3 blade-1 window entry, 4 blade-2 window entry. Two order-sensitive
// 32-bit hashes over (tick, member*8 + kind): two logs agree only if counts, times,
// members and kinds agree in order. The engine only writes through a pointer that
// is null outside the check, so the arithmetic never depends on it.
struct EventLog {
  uint64_t count[5] = {0, 0, 0, 0, 0};
  uint32_t h1 = 2166136261u, h2 = 0;
  void add(uint32_t tick, uint32_t id, uint32_t kind) {
    count[kind]++;
    const uint32_t w = id * 8u + kind;
    h1 = (h1 ^ tick) * 16777619u;
    h1 = (h1 ^ w) * 16777619u;
    h2 = h2 * 31u + tick;
    h2 = h2 * 31u + w;
  }
};

}  // namespace horde2::engine
