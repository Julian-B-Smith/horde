/*
 * flush_to_zero.h — flush-to-zero around one call, and the caller's floating-point
 * control word back afterwards. ROADMAP B448; ruled by the human in ADR-211 item 1:
 * the horde 2 shell runs with flush-to-zero on from its first build.
 *
 * ONE COPY, TWO USERS. The shell (B398, not built yet) and the tools that pin the
 * engine's bits (tools/h2_engine_selfdigest_check.cpp, tools/h2_engine_fz_probe.cpp)
 * both switch the mode through this file, so the build that is pinned and the
 * build that ships cannot set it differently (ADR-187 A1). It sits at the top of
 * h2/ because it is neither an engine nor a module: it makes and processes no
 * sound, and it includes nothing from the engine.
 *
 * WHAT THE MODE IS. Arithmetic that would produce a subnormal number produces
 * zero, and a subnormal operand is read as zero. A decaying tail then ends in
 * exact zeros instead of hundreds of subnormal samples, which some CPUs compute
 * many times slower than normal ones.
 *
 * WHAT IS SET, per architecture:
 *   arm64    bit 24 (FZ) of FPCR. MEASURED on the dev Mac (Apple clang 16,
 *            2026-10-11): it flushes subnormal RESULTS and subnormal INPUTS. An
 *            arm64 CPU without FEAT_AFP has no separate input bit; with FEAT_AFP
 *            the split only appears when FPCR.AH is set, which nothing here does.
 *   x86-64   bits 15 (FTZ, results) and 6 (DAZ, inputs) of MXCSR, both, because
 *            arm64's one bit does both jobs. WRITTEN FROM THE DOCUMENTATION AND
 *            NOT MEASURED ON AN x86 CPU. The only run it has had is as an x86-64
 *            binary under Rosetta 2 translation on the dev Mac (2026-10-11): the
 *            bits read back, results and inputs both flush, the word is restored.
 *            That is a translator, not a CPU. It is NOT expected to match arm64
 *            bit for bit: x86 decides that a result is tiny after rounding and
 *            arm64 before, so a result that rounds up to the smallest normal
 *            number is kept on x86 and flushed on arm64 (the two manuals; seen
 *            once, native arm64 against the Rosetta run, on 1.5 * DBL_MIN times
 *            the double nearest 2/3). The engine's bits are pinned per platform
 *            for that kind of reason.
 *   others   nothing: kFlushToZeroAvailable is false and the calls below run
 *            `f` unchanged. A caller that pins bits must check flushToZeroState().
 * Only those bits are changed, and on the way out the WHOLE word is written back
 * as it was found: rounding mode, exception masks and every other field are the
 * caller's. On x86-64 the same register also holds the sticky exception flags, so
 * flags raised inside the call are dropped with the restore, and a test that
 * compares the word before and after must read it immediately around the call
 * (any arithmetic in between may raise a flag).
 *
 * WHY A CALL AND NOT A GUARD OBJECT. The compiler does not know that writing the
 * control word changes arithmetic, so it may move a floating-point operation
 * across the write. underFlushToZero(f) calls `f` through a function that cannot
 * be inlined: everything `f` does runs between the two writes. A guard object in
 * the same function as the arithmetic gives no such promise.
 *
 * DID IT TAKE EFFECT? flushToZeroState() reads the bits back AND multiplies its
 * way into a subnormal, once as a result and once as an input, and reports all
 * three. A build where the write silently does nothing reads bits = false; a CPU
 * where the bits read back but arithmetic is unchanged reads results = false.
 *
 * Real-time safe: no allocation, no lock, no system call. Each switch is two
 * register writes (tools/h2_rt_lint_check.py scans this file with the rest of h2/).
 */
#pragma once

#include <cstdint>
#include <cstring>
#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

// Local to this file and undefined at its end: no macro leaves h2/ but HZ_CULL_ENV.
#if defined(_MSC_VER) && !defined(__clang__)
#define H2_FTZ_NOINLINE __declspec(noinline)
#else
#define H2_FTZ_NOINLINE __attribute__((noinline))
#endif

namespace horde2 {

#if defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
inline constexpr bool kFlushToZeroAvailable = true;
inline constexpr uint64_t kFlushToZeroBits = 1ull << 24;   // FPCR.FZ
inline uint64_t fpControlWord() noexcept { uint64_t v; __asm__ volatile("mrs %0, fpcr" : "=r"(v)); return v; }
inline void setFpControlWord(uint64_t v) noexcept { __asm__ volatile("msr fpcr, %0" : : "r"(v)); }
#elif defined(__x86_64__) || defined(_M_X64)
inline constexpr bool kFlushToZeroAvailable = true;
inline constexpr uint64_t kFlushToZeroBits = (1ull << 15) | (1ull << 6);   // MXCSR.FTZ | MXCSR.DAZ; unmeasured (header)
inline uint64_t fpControlWord() noexcept { return _mm_getcsr(); }
inline void setFpControlWord(uint64_t v) noexcept { _mm_setcsr(static_cast<unsigned int>(v)); }
#else
inline constexpr bool kFlushToZeroAvailable = false;
inline constexpr uint64_t kFlushToZeroBits = 0;
inline uint64_t fpControlWord() noexcept { return 0; }
inline void setFpControlWord(uint64_t) noexcept {}
#endif

namespace flush_detail {
// The wall (header: "why a call"). Never inlined, so `f` runs between the writes.
template <class F>
H2_FTZ_NOINLINE void wall(F&& f) { f(); }

// THE switch: both public calls and every control go through it.
template <class F>
inline void runWith(bool on, F&& f) {
  const uint64_t saved = fpControlWord();
  setFpControlWord(on ? (saved | kFlushToZeroBits) : (saved & ~kFlushToZeroBits));
  wall(f);
  setFpControlWord(saved);   // the whole word, bit for bit: the host may have entered with the mode on or off
}
}  // namespace flush_detail

// Runs f() with flush-to-zero ON, then puts the caller's control word back. The
// shell wraps each entry that runs engine code in this; scopes may nest.
template <class F>
inline void underFlushToZero(F&& f) { flush_detail::runWith(true, f); }

// Runs f() with flush-to-zero forced OFF, then puts the caller's control word
// back. For tests: the forced-off control, and a stub host that enters with the
// mode already on.
template <class F>
inline void withoutFlushToZero(F&& f) { flush_detail::runWith(false, f); }

// What the mode is RIGHT NOW on the calling thread: the bits as read back, and
// what arithmetic actually does. on() and off() are both strict, so a half-set
// mode (x86 with FTZ and no DAZ, or bits that read back and change nothing) is
// neither.
struct FlushToZeroState {
  bool bits = false;      // every bit of kFlushToZeroBits reads back set
  bool results = false;   // a subnormal RESULT came out as zero
  bool inputs = false;    // a subnormal INPUT was read as zero
  bool on() const { return bits && results && inputs; }
  bool off() const { return !bits && !results && !inputs; }
};

// Never inlined: its two multiplies must run where it is called, inside the scope
// being asked about. The operands are volatile so nothing is folded at compile
// time, and the products are judged by their BITS: with the mode on, a
// floating-point compare would read a surviving subnormal as zero and report a
// flush that did not happen.
H2_FTZ_NOINLINE inline FlushToZeroState flushToZeroState() noexcept {
  volatile double minNormal = 2.2250738585072014e-308;   // DBL_MIN
  volatile double half = 0.5;
  volatile double subnormal = 5.5626846462680035e-309;   // DBL_MIN / 4, stored by the compiler, never computed here
  volatile double big = 1e300;
  const double res = minNormal * half;   // off: 1.1e-308, a subnormal. on: zero
  const double inp = subnormal * big;    // off: 5.6e-9. on: zero
  uint64_t rb, ib;
  std::memcpy(&rb, &res, sizeof rb);
  std::memcpy(&ib, &inp, sizeof ib);
  FlushToZeroState s;
  s.bits = kFlushToZeroAvailable && (fpControlWord() & kFlushToZeroBits) == kFlushToZeroBits;
  s.results = (rb << 1) == 0;   // either zero, whatever its sign
  s.inputs = (ib << 1) == 0;
  return s;
}

}  // namespace horde2

#undef H2_FTZ_NOINLINE
