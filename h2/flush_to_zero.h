/*
 * flush_to_zero.h — flush-to-zero around one call, and the caller's floating-point
 * control word back afterwards. ROADMAP B448; ruled by the human in ADR-211 item 1:
 * the horde 2 shell runs with flush-to-zero on from its first build.
 *
 * ONE COPY, EVERY USER. The shell (B398, not built yet) and every tool that
 * replays the engine (through tools/h2_engine_stream.h's replay, which the parity
 * check, its FMA control, both self-digest tools and the probes all use) switch
 * the mode through this file, so the build that is judged, the build that is
 * pinned and the build that ships cannot set it differently (ADR-211 item 1). It
 * sits at the top of h2/ because it is neither an engine nor a module: it makes
 * and processes no sound, and it includes nothing from the engine.
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
 *            Only GCC and Clang spell the register access below. Microsoft's
 *            compiler on arm64 (_M_ARM64) has no inline assembly; it reaches
 *            the register through intrinsics (_ReadStatusReg and
 *            _WriteStatusReg, as its documentation is remembered here, unchecked).
 *            That branch is NOT written, because nothing here can compile or
 *            run it; that target stops at the #error below.
 *   x86-64   bits 15 (FTZ, results) and 6 (DAZ, inputs) of MXCSR, both, because
 *            arm64's one bit does both jobs. WRITTEN FROM THE DOCUMENTATION. What
 *            it has and has not been run on is kept in ONE place:
 *            docs/proposals/h2-flush-to-zero.md, section 7. It is NOT
 *            expected to match arm64 bit for bit: x86 decides that a result is
 *            tiny after rounding and arm64 before, so a result that rounds up to
 *            the smallest normal number is kept on x86 and flushed on arm64. The
 *            engine's bits are pinned per platform for that kind of reason.
 *            ARM64EC is kept OUT of this branch on purpose: Microsoft's compiler
 *            defines _M_X64 there too, and what its emulated MXCSR does to
 *            native arm64 arithmetic is not known here.
 *   others   a compile error. A target this file does not know (32-bit x86,
 *            arm64 under Microsoft's compiler, ARM64EC, anything else) must not
 *            build a switch that silently does nothing.
 * Only those bits are changed, and on the way out the WHOLE word is written back
 * as it was found: rounding mode, exception masks and every other field are the
 * caller's. On x86-64 the same register also holds the sticky exception flags, so
 * flags raised inside the call are dropped with the restore, and a test that
 * compares the word before and after must read it immediately around the call
 * (any arithmetic in between may raise a flag).
 *
 * THE ROUNDING MODE IS NOT SET HERE, and the pinned bits need round-to-nearest.
 * A host that calls in under another rounding mode gets flush-to-zero and NOT the
 * pinned bits (measured 2026-10-11: entered rounding toward zero, all 543
 * self-digest rows differ). Whether this switch should also force
 * round-to-nearest, or the shell should refuse such a host, is the human's open
 * choice: docs/proposals/h2-flush-to-zero.md, section 8. Until it is made, the
 * tools prove the mode they ran in (tools/h2_engine_stream.h, `control ROUND`).
 *
 * THE WORD COMES BACK ON EVERY WAY OUT. The restore is a destructor
 * (flush_detail::Restore), so it runs when `f` returns and when `f` throws. It
 * is one register write: no allocation, no lock, no call. (A longjmp across the
 * scope runs no destructor and is not covered; nothing in h2/ makes one.)
 *
 * WHY A CALL, WITH THE GUARD ONLY FOR THE RESTORE. The compiler does not know
 * that writing the control word changes arithmetic, so it may move a
 * floating-point operation across the write. underFlushToZero(f) calls `f`
 * through a function that cannot be inlined: everything `f` does runs between
 * the two writes. A guard object in the same function as the arithmetic gives no
 * such promise, which is why no guard is offered to callers.
 *
 * DID IT TAKE EFFECT? flushToZeroState() reads the bits back AND multiplies its
 * way into a subnormal, once as a result and once as an input, and reports all
 * three. A build where the write silently does nothing reads bits = false; a CPU
 * where the bits read back but arithmetic is unchanged reads results = false.
 *
 * Real-time safe: no allocation, no lock, no system call. Each switch is two
 * register writes (tools/h2_rt_lint_check.py scans this file with the rest of
 * h2/, and tools/h2_engine_rtsan_probe.cpp runs the switch inside
 * RealtimeSanitizer's realtime scope around every engine call).
 */
#pragma once

#include <cstdint>
#include <cstring>
#if (defined(__x86_64__) || defined(_M_X64)) && !defined(_M_ARM64EC)
#include <xmmintrin.h>
#endif

// Local to this file and undefined at its end: no macro leaves h2/ but HZ_CULL_ENV.
#if defined(_MSC_VER) && !defined(__clang__)
#define H2_FTZ_NOINLINE __declspec(noinline)
#else
#define H2_FTZ_NOINLINE __attribute__((noinline))
#endif

namespace horde2 {

// The ONE copy of the bit-twiddling. Nothing outside this file writes the word:
// the public calls below, and the tools-only one at the end, all go through
// runWith.
namespace flush_detail {

#if defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
inline constexpr uint64_t kBits = 1ull << 24;   // FPCR.FZ
inline uint64_t word() noexcept { uint64_t v; __asm__ volatile("mrs %0, fpcr" : "=r"(v)); return v; }
inline void setWord(uint64_t v) noexcept { __asm__ volatile("msr fpcr, %0" : : "r"(v)); }
#elif (defined(__x86_64__) || defined(_M_X64)) && !defined(_M_ARM64EC)
inline constexpr uint64_t kBits = (1ull << 15) | (1ull << 6);   // MXCSR.FTZ | MXCSR.DAZ
inline uint64_t word() noexcept { return _mm_getcsr(); }
inline void setWord(uint64_t v) noexcept { _mm_setcsr(static_cast<unsigned int>(v)); }
#else
#error "h2/flush_to_zero.h: no flush-to-zero switch is written for this target (known: arm64 under GCC or Clang, x86-64). Add and measure one; never build without it (ADR-211 item 1)."
#endif

// The wall (header: "why a call"). Never inlined, so `f` runs between the writes.
template <class F>
H2_FTZ_NOINLINE void wall(F&& f) { f(); }

// Writes the saved word back when it goes out of scope: on return, and on a
// throw through `f` (header: "the word comes back on every way out"). The whole
// word, bit for bit: the host may have entered with the mode on or off.
struct Restore {
  const uint64_t saved;
  explicit Restore(uint64_t w) noexcept : saved(w) {}
  ~Restore() { setWord(saved); }
  Restore(const Restore&) = delete;
  Restore& operator=(const Restore&) = delete;
};

// THE switch: every public call and every control goes through it.
template <class F>
inline void runWith(bool on, F&& f) {
  const Restore restore(word());
  setWord(on ? (restore.saved | kBits) : (restore.saved & ~kBits));
  wall(f);
}

}  // namespace flush_detail

// The calling thread's floating-point control word, read only (FPCR on arm64,
// MXCSR on x86-64). For a caller that proves its word came back.
inline uint64_t fpControlWord() noexcept { return flush_detail::word(); }

// Runs f() with flush-to-zero ON, then puts the caller's control word back. The
// shell wraps each entry that runs engine code in this; scopes may nest.
template <class F>
inline void underFlushToZero(F&& f) { flush_detail::runWith(true, f); }

// What the mode is RIGHT NOW on the calling thread: the bits as read back, and
// what arithmetic actually does. on() and off() are both strict, so a half-set
// mode (x86 with FTZ and no DAZ, or bits that read back and change nothing) is
// neither.
struct FlushToZeroState {
  bool bits = false;      // every flush-to-zero bit reads back set
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
  s.bits = (flush_detail::word() & flush_detail::kBits) == flush_detail::kBits;
  s.results = (rb << 1) == 0;   // either zero, whatever its sign
  s.inputs = (ib << 1) == 0;
  return s;
}

// TOOLS ONLY. Nothing a shell ships calls anything in this namespace: it exists
// so the tools' controls can show that "on" is not a reading that is always
// true, and can play a host that enters with the mode in a known state, without
// a second copy of the bit-twiddling above.
namespace flush_tools_only {

// Runs f() with flush-to-zero forced OFF, then puts the caller's control word back.
template <class F>
inline void withoutFlushToZero(F&& f) { flush_detail::runWith(false, f); }

}  // namespace flush_tools_only

}  // namespace horde2

#undef H2_FTZ_NOINLINE
