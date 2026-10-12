/*
 * flush_to_zero.h — flush-to-zero and round-to-nearest around one call, and the
 * caller's floating-point control word back afterwards. ROADMAP B448. Two
 * rulings by the human: the horde 2 shell runs with flush-to-zero on from its
 * first build (ADR-211 item 1), and this switch forces round-to-nearest inside
 * its scope and restores the host's setting afterwards (2026-10-12, option A of
 * docs/proposals/h2-flush-to-zero.md section 8; the lead records it as ADR-213).
 *
 * ONE COPY, EVERY USER. The shell (B398, not built yet) and every tool that
 * replays the engine (through tools/h2_engine_stream.h's replay, which the parity
 * check, its FMA control, both self-digest tools and the probes all use) switch
 * the mode through this file, so the build that is judged, the build that is
 * pinned and the build that ships cannot set it differently (ADR-211 item 1). It
 * sits at the top of h2/ because it is neither an engine nor a module: it makes
 * and processes no sound, and it includes nothing from the engine.
 *
 * WHAT THE SWITCH OWNS, AND WHAT IT LEAVES ALONE. Inside its scope it sets two
 * fields of the calling thread's control word:
 *   the flush-to-zero bits   on;
 *   the rounding field       round to nearest, ties to even, whatever the caller
 *                            had.
 * Every other field goes into the scope exactly as the caller had it:
 *   arm64 (FPCR)    the six trap enables (IOE, DZE, OFE, UFE, IXE, IDE),
 *                   default-NaN (DN), the two half-precision bits (AHP, and
 *                   FZ16, the flush bit for half-precision numbers, which the
 *                   engine's doubles never are), and FEAT_AFP's three (AH, FIZ,
 *                   NEP) on a CPU that has them;
 *   x86-64 (MXCSR)  the six exception masks and the six sticky exception flags.
 * On the way out the WHOLE word is written back as it was found, the two owned
 * fields included. On x86-64 the same register holds the sticky exception flags,
 * so flags raised inside the call are dropped with the restore, and a test that
 * compares the word before and after must read it immediately around the call
 * (any arithmetic in between may raise a flag).
 *
 * WHAT FLUSH-TO-ZERO IS. Arithmetic that would produce a subnormal number
 * produces zero, and a subnormal operand is read as zero. A decaying tail then
 * ends in exact zeros instead of hundreds of subnormal samples, which some CPUs
 * compute many times slower than normal ones.
 *
 * WHAT IS WRITTEN, per architecture:
 *   arm64    bit 24 (FZ) of FPCR set, and bits 23 and 22 (RMode) cleared.
 *            MEASURED on the dev Mac (Apple clang 16; FZ 2026-10-11, RMode
 *            2026-10-12). FZ flushes subnormal RESULTS and subnormal INPUTS. An
 *            arm64 CPU without FEAT_AFP has no separate input bit; with FEAT_AFP
 *            the split only appears when FPCR.AH is set, which nothing here does.
 *            Only GCC and Clang spell the register access below. Microsoft's
 *            compiler on arm64 (_M_ARM64) has no inline assembly; it reaches
 *            the register through intrinsics (_ReadStatusReg and
 *            _WriteStatusReg, as its documentation is remembered here, unchecked).
 *            That branch is NOT written, because nothing here can compile or
 *            run it; that target stops at the #error below.
 *   x86-64   bits 15 (FTZ, results) and 6 (DAZ, inputs) of MXCSR set, both,
 *            because arm64's one bit does both jobs; and bits 14 and 13 (RC, the
 *            rounding control) cleared. WRITTEN FROM THE DOCUMENTATION. What it
 *            has and has not been run on is kept in ONE place:
 *            docs/proposals/h2-flush-to-zero.md, section 7. It is NOT
 *            expected to match arm64 bit for bit: x86 decides that a result is
 *            tiny after rounding and arm64 before, so a result that rounds up to
 *            the smallest normal number is kept on x86 and flushed on arm64. The
 *            engine's bits are pinned per platform for that kind of reason.
 *            The older x87 control word has a rounding field of its own, which
 *            is NOT touched: on x86-64 a double is computed in SSE registers,
 *            under MXCSR (argued from the calling convention, not measured).
 *            ARM64EC is kept OUT of this branch on purpose: Microsoft's compiler
 *            defines _M_X64 there too, and what its emulated MXCSR does to
 *            native arm64 arithmetic is not known here.
 *   others   a compile error. A target this file does not know (32-bit x86,
 *            arm64 under Microsoft's compiler, ARM64EC, anything else) must not
 *            build a switch that silently does nothing.
 * A cleared rounding field is round-to-nearest, ties to even, on both.
 *
 * WHY THE ROUNDING FIELD IS OWNED. The pinned bits need round-to-nearest.
 * Measured 2026-10-11, while this switch still left the field to the host: a
 * process entered rounding toward zero differed on all 543 self-digest rows and
 * left the parity bound on 484 scenarios. A host's rounding mode would have
 * changed the sound. Measured 2026-10-12 with the field owned: every replay
 * entered rounding toward zero, 543 of 543 rows bit-identical to the pinned
 * references, in both builds (docs/proposals/h2-flush-to-zero.md, section 8).
 *
 * THE CALLER'S ROUNDING MODE IS REPORTED, NOT JUDGED. underFlushToZero returns
 * the mode the caller arrived with, decoded from the word the switch had read
 * anyway: no second register read, no allocation, no lock, no call. A shell
 * counts the entries where it is not Rounding::nearest, so a host that calls in
 * oddly is corrected AND seen; the counter is the shell's. An entry that leaves
 * by a throw returns nothing. A scope nested in another reports nearest, which
 * is what the outer one set.
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
 * DID IT TAKE EFFECT? flushToZeroState() reports what is IN FORCE, never what
 * was asked for. Flush-to-zero: the bits read back, AND a multiply into a
 * subnormal once as a result and once as an input. Rounding: the field read
 * back, AND four additions that only round-to-nearest, ties to even, gets
 * right. A build where the write silently does nothing reads bits = false; a CPU
 * where the fields read back but arithmetic is unchanged reads results = false
 * or roundsToNearest = false.
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

// The four directions a rounding field can hold: what a caller arrived with
// (underFlushToZero's return) and what is in force now (FlushToZeroState).
enum class Rounding : uint8_t { nearest, towardZero, upward, downward };

// The ONE copy of the bit-twiddling. Every public call below goes through
// runWith, and so does the tools-only forced-off call at the end; the tools-only
// host beside it is the one other writer of the word, with these same two calls.
namespace flush_detail {

// kRoundingCode is the rounding field's four codes, in order. kOtherFields is
// for the tools-only host at the end of this file: fields the switch does NOT own.
#if defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
inline constexpr uint64_t kFlushBits = 1ull << 24;   // FPCR.FZ
inline constexpr int kRoundingShift = 22;            // FPCR.RMode, bits 23:22
// 00 nearest (ties to even), 01 toward plus infinity, 10 toward minus infinity, 11 toward zero
inline constexpr Rounding kRoundingCode[4] = {Rounding::nearest, Rounding::upward, Rounding::downward, Rounding::towardZero};
// AHP, DN, FZ16, IDE, the five trap enables IXE to IOE, and FEAT_AFP's NEP, AH, FIZ
inline constexpr uint64_t kOtherFields = (1ull << 26) | (1ull << 25) | (1ull << 19) | (1ull << 15) | (0x1full << 8) | 0x7ull;
inline uint64_t word() noexcept { uint64_t v; __asm__ volatile("mrs %0, fpcr" : "=r"(v)); return v; }
inline void setWord(uint64_t v) noexcept { __asm__ volatile("msr fpcr, %0" : : "r"(v)); }
#elif (defined(__x86_64__) || defined(_M_X64)) && !defined(_M_ARM64EC)
// This whole branch is WRITTEN FROM THE DOCUMENTATION (header: "what is written").
inline constexpr uint64_t kFlushBits = (1ull << 15) | (1ull << 6);   // MXCSR.FTZ | MXCSR.DAZ
inline constexpr int kRoundingShift = 13;                            // MXCSR.RC, bits 14:13
// 00 nearest (ties to even), 01 toward minus infinity, 10 toward plus infinity, 11 toward zero
inline constexpr Rounding kRoundingCode[4] = {Rounding::nearest, Rounding::downward, Rounding::upward, Rounding::towardZero};
// The six sticky exception flags. NOT the six masks: they arrive set, and
// clearing one unmasks an exception that the next inexact result would raise.
inline constexpr uint64_t kOtherFields = 0x3full;
inline uint64_t word() noexcept { return _mm_getcsr(); }
inline void setWord(uint64_t v) noexcept { _mm_setcsr(static_cast<unsigned int>(v)); }
#else
#error "h2/flush_to_zero.h: no flush-to-zero switch is written for this target (known: arm64 under GCC or Clang, x86-64). Add and measure one; never build without it (ADR-211 item 1)."
#endif
inline constexpr uint64_t kRoundingField = 3ull << kRoundingShift;
constexpr Rounding roundingOf(uint64_t w) noexcept { return kRoundingCode[(w >> kRoundingShift) & 3]; }

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

// THE switch: every public call and the tools' forced-off call go through it.
// The word written is the caller's, with the flush bits as asked and the
// rounding field cleared (round-to-nearest, ties to even, on both
// architectures); no other bit of it differs (header: "what the switch owns").
// Returns the rounding mode the caller arrived with, from the word already read.
template <class F>
inline Rounding runWith(bool on, F&& f) {
  const Restore restore(word());
  setWord((on ? (restore.saved | kFlushBits) : (restore.saved & ~kFlushBits)) & ~kRoundingField);
  wall(f);
  return roundingOf(restore.saved);
}

}  // namespace flush_detail

// The calling thread's floating-point control word, read only (FPCR on arm64,
// MXCSR on x86-64). For a caller that proves its word came back.
inline uint64_t fpControlWord() noexcept { return flush_detail::word(); }

// Runs f() with flush-to-zero ON and rounding to nearest, then puts the caller's
// control word back. The shell wraps each entry that runs engine code in this;
// scopes may nest. Returns the rounding mode the caller ARRIVED with (header:
// "the caller's rounding mode is reported"): the shell counts the entries where
// that is not Rounding::nearest.
template <class F>
inline Rounding underFlushToZero(F&& f) { return flush_detail::runWith(true, f); }

// What is IN FORCE right now on the calling thread: the fields as read back, and
// what arithmetic actually does. on() and off() are about flush-to-zero alone
// and both strict, so a half-set mode (x86 with FTZ and no DAZ, or bits that
// read back and change nothing) is neither. nearest() is as strict about
// rounding: the field AND the arithmetic. A default-constructed state (nothing
// read yet) is neither on() nor nearest().
struct FlushToZeroState {
  bool bits = false;      // every flush-to-zero bit reads back set
  bool results = false;   // a subnormal RESULT came out as zero
  bool inputs = false;    // a subnormal INPUT was read as zero
  Rounding roundingField = Rounding::nearest;   // the rounding field as read back
  bool roundsToNearest = false;                 // four additions came out as only nearest, ties to even, gives them
  bool on() const { return bits && results && inputs; }
  bool off() const { return !bits && !results && !inputs; }
  bool nearest() const { return roundingField == Rounding::nearest && roundsToNearest; }
  // The mode the engine's pinned bits were taken in: what a shell entry must
  // read inside the switch.
  bool engineMode() const { return on() && nearest(); }
};

// Never inlined: its arithmetic must run where it is called, inside the scope
// being asked about. The operands are volatile so nothing is folded at compile
// time, and the products are judged by their BITS: with the mode on, a
// floating-point compare would read a surviving subnormal as zero and report a
// flush that did not happen. The four sums are ordinary numbers next to 1, whose
// ULP is 2^-52, so a compare reads them truly with the mode on or off:
//   1 + 3/4 ULP             the next double when rounding to nearest or upward, else 1;
//   1 + 1/4 ULP             1, unless rounding upward;
//   1 + 1/2 ULP             a tie: 1, the even neighbour, under ties-to-even;
//   (1 + 1 ULP) + 1/2 ULP   a tie whose even neighbour is the upper one, 1 + 2 ULP.
// Round-to-nearest, ties to even, is the one mode that gives all four.
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
  volatile double one = 1.0, next = 0x1.0000000000001p+0, nextEven = 0x1.0000000000002p+0;
  volatile double quarterUlp = 0x1p-54, halfUlp = 0x1p-53, threeQuarterUlp = 0x1.8p-53;
  const double up = one + threeQuarterUlp;
  const double stay = one + quarterUlp;
  const double tieDown = one + halfUlp;
  const double tieUp = next + halfUlp;
  const uint64_t w = flush_detail::word();
  FlushToZeroState s;
  s.bits = (w & flush_detail::kFlushBits) == flush_detail::kFlushBits;
  s.results = (rb << 1) == 0;   // either zero, whatever its sign
  s.inputs = (ib << 1) == 0;
  s.roundingField = flush_detail::roundingOf(w);
  s.roundsToNearest = up == next && stay == one && tieDown == one && tieUp == nextEven;
  return s;
}

// TOOLS ONLY. Nothing a shell ships calls anything in this namespace: it exists
// so the tools' controls can show that "on" is not a reading that is always
// true, and can play a host that enters with the word in a known state, without
// a second copy of the bit-twiddling above. (A host's ROUNDING mode needs
// nothing from here: the tools set it with <cfenv>'s fesetround, so the control
// that judges the rounding field does not lean on this file's own constants.)
namespace flush_tools_only {

// What the switch DECLARES it owns, read only. The tools measure which bits
// really move and hold the answer to these.
inline constexpr uint64_t kFlushBits = flush_detail::kFlushBits;
inline constexpr uint64_t kRoundingField = flush_detail::kRoundingField;

// Runs f() with flush-to-zero forced OFF through the same switch (so rounding
// to nearest all the same), then puts the caller's control word back.
template <class F>
inline Rounding withoutFlushToZero(F&& f) { return flush_detail::runWith(false, f); }

// Runs f() as a host whose fields the switch does NOT own
// (flush_detail::kOtherFields) are each the opposite of what this thread came
// with, then puts the word back. With the thread as it came, that shows every
// one of those bits both set and clear, so a switch that wrote one of them
// either way would show. NO FLOATING-POINT ARITHMETIC inside f, only reads of
// the word and calls to the switch: on arm64 the trap enables are among the
// bits, and under them an inexact or underflowing operation stops the process.
// A bit the CPU does not implement stays zero. The word is put back by a plain
// write and not by flush_detail::Restore, so that a control watching a switch
// whose restore is broken still gets the traps off again and lives to report
// it; f must therefore not throw.
template <class F>
inline void asHostWithOtherFieldsFlipped(F&& f) {
  const uint64_t came = flush_detail::word();
  flush_detail::setWord(came ^ flush_detail::kOtherFields);
  flush_detail::wall(f);
  flush_detail::setWord(came);
}

}  // namespace flush_tools_only

}  // namespace horde2

#undef H2_FTZ_NOINLINE
