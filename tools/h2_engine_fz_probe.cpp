/*
 * h2_engine_fz_probe — what flush-to-zero does to the composed engine's
 * self-digest rows. ROADMAP B448 phase 2, package P16.
 *
 * HISTORY. It was written to give ADR-209 item 6 its number while the references
 * pinned the engine with flush-to-zero OFF: 8 of 543 rows changed with it on (PR
 * #1047). The human then ruled the mode on in the horde 2 shell (ADR-211 item 1),
 * and since 2026-10-11 the references pin the engine with it ON, set by the
 * self-digest tools over the whole replay (their header). So the probe now asks
 * the two questions that pin leaves:
 *   OFF     with the mode forced off, which rows differ from the pinned bits, and
 *           by how much? (The pin must depend on the mode: none is a failure.)
 *   SCOPES  does it matter WHERE the mode is switched on? Two narrower scopes are
 *           compared with the pinned one.
 *
 * This is a measurement, not a gate. Nothing in ./verify runs it and it is not a
 * CMake target; tools/h2_engine_fz_probe.py builds it twice, with the two
 * self-digest builds' own flags, and runs it. It reads the pinned references and
 * NEVER writes one: there is no re-pin path in this file.
 *
 * WHAT IS SET. The switch is h2/flush_to_zero.h's, the one copy the self-digest
 * tools use and the shell will: on arm64, bit 24 (FZ) of FPCR for the calling
 * thread, the whole register restored on the way out. The SET line prints the
 * register before, inside and after, and the SEMANTICS line prints what the bit
 * did to one subnormal RESULT and one subnormal INPUT on this CPU, so the meaning
 * of "on" is measured, not assumed. The probe itself is arm64 only (it reads
 * FPSR): on other architectures it refuses.
 *
 * THE SCOPES. Each row is replayed once per scope:
 *   thread  the mode on from before the engine is constructed to after the
 *           replay. THE PINNED SCOPE: what the self-digest tools do;
 *   off     the mode forced off throughout;
 *   render  the mode on inside Engine::render only, switched per render call;
 *   calls   the mode on inside every call a host block makes (render and the
 *           event calls: set, setString, snap, noteOn, noteOff, retune, panic,
 *           the cap setters), the engine constructed with it off.
 *
 * WHAT IS REUSED. The digest, the reference key, the reference reader and the
 * row-by-row verdict are tools/h2_engine_selfdigest_check.cpp's own, included
 * below with its main() renamed and never called, so a "changed row" here is
 * judged by the code the gate judges with. The replay is the hooked copy
 * (tools/h2_engine_hooked_replay.h), which BASELINE proves against the pins.
 *
 * THE ORDER OF PROOF, each a precondition of the next (LIBRARY L0032):
 *   SET        the bit reads back as set inside the switch and clear after it;
 *   DENORMAL   a decaying feedback path (y = y * 0.5 from 1, 1200 frames, the
 *              coefficient read from memory) rendered through the same switch
 *              and digested by the same digest gives DIFFERENT bytes on and off;
 *   ZERO       the same path stopped at 900 frames, above the subnormal range,
 *              gives IDENTICAL bytes on and off (the must-read-zero half);
 *   BASELINE   in the pinned scope, all rows equal the pinned reference for this
 *              build, platform, compiler and Node major. No reference for this
 *              key, or one differing row: the probe stops and reports no number.
 *   COUNT      per other scope, the rows whose digest differs from the same
 *              run's pinned-scope digest (which BASELINE showed equal to the
 *              pin). A digest says THAT a row changed, never by how much, so
 *              each such row's samples are also compared: how many differ, from
 *              which frame, and the largest difference. For `off`, no differing
 *              row at all is a FAILURE: the pin would not depend on the mode.
 *
 * FLAGS, context only. FPSR's cumulative bits are cleared before the pinned-scope
 * and the off replay and read after each: UFC (bit 3) is set when a result is
 * flushed, IDC (bit 7) when a subnormal input is. A row that raised neither with
 * the mode on ran no operation the bit changes (argued from the architecture's
 * definition of the two bits; the SEMANTICS line shows both being raised on this
 * CPU). UFC is also raised by an ordinary underflow to zero with the mode off, so
 * it over-counts.
 *
 * Usage: h2_engine_fz_probe --full-from FILE [--rows]
 *   --rows prints every differing row by name. Run from the repo root.
 * Exit: 0 measured; 1 a control or the baseline failed (no number); 2 the run
 *       broke (no stream, a cut stream, no reference, not arm64).
 */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define main h2_selfdigest_stock_main   // reused for its digest, key and verdict code; never called
#include "h2_engine_selfdigest_check.cpp"
#undef main
#include "h2_engine_hooked_replay.h"

namespace {

// The control register and the switch are h2/flush_to_zero.h's (included by the
// self-digest source above). Only the status register is read here, for FLAGS.
#if defined(__aarch64__)
constexpr uint64_t kFZ = horde2::kFlushToZeroBits;       // FPCR.FZ
constexpr uint64_t kUFC = 1ull << 3, kIDC = 1ull << 7;   // FPSR: underflow, input denormal (cumulative)
inline uint64_t fpsrGet() { uint64_t v; __asm__ volatile("mrs %0, fpsr" : "=r"(v)); return v; }
inline void fpsrSet(uint64_t v) { __asm__ volatile("msr fpsr, %0" : : "r"(v)); }
#else
constexpr uint64_t kFZ = 0, kUFC = 0, kIDC = 0;   // not arm64: main refuses before anything is measured
inline uint64_t fpsrGet() { return 0; }
inline void fpsrSet(uint64_t) {}
#endif
inline uint64_t fpcrGet() { return horde2::fpControlWord(); }

// A call the compiler cannot inline: the hook's own wall where the mode is already
// set outside (the header's switch has one of its own; see its "why a call").
template <class F>
__attribute__((noinline)) void callOut(F&& f) { f(); }

/* THE switch: h2/flush_to_zero.h's two calls behind one name. Every scope and both
   controls go through it, so the DENORMAL control proves the switch the counts
   depend on, which is the switch the self-digest tools pin with. */
template <class F>
void underFz(bool on, F&& f) {
  if (on) horde2::underFlushToZero(f);
  else horde2::withoutFlushToZero(f);
}

// kThread is the pinned scope (the self-digest tools'); the others are compared with it.
enum Scope { kThread = 0, kOff, kRenderOnly, kCalls, kScopes };
const char* const kScopeName[kScopes] = {"thread", "off", "render", "calls"};

uint64_t replayDigest(const Scenario& sc, Scope scope, std::vector<double>& out, uint64_t* fpsr = nullptr) {
  EventLog log;
  ReplayInfo info;
  auto run = [&] {
    h2engine_hooked::replay(sc, out, &log, &info, [&](h2engine_hooked::Call kind, auto&& call) {
      const bool on = scope == kCalls || (scope == kRenderOnly && kind == h2engine_hooked::kRender);
      if (scope == kThread) callOut(call);   // already on, set once outside
      else underFz(on, call);
    });
  };
  if (fpsr) fpsrSet(0);
  if (scope == kThread) underFz(true, run);
  else run();
  if (fpsr) *fpsr = fpsrGet();
  return digest(out, log, info);
}

// The control's signal: one pole with no input, decaying from 1. `frames` 1200 runs
// through the whole subnormal range (2^-1023 .. 2^-1074); 900 stays above it.
uint64_t decayDigest(bool on, int frames, std::vector<double>* keep = nullptr) {
  static volatile double coeff = 0.5;   // read from memory: nothing here is folded at compile time
  std::vector<double> out(static_cast<size_t>(frames) * 2);
  double* const o = out.data();
  underFz(on, [&] {
    double y = 1;
    for (int i = 0; i < frames; i++) { o[2 * i] = y; o[2 * i + 1] = -y; y = y * coeff; }
  });
  if (keep) *keep = out;
  return digest(out, EventLog(), ReplayInfo());
}

}  // namespace

int main(int argc, char** argv) {
  std::string path;
  bool rows = false;
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--full-from") == 0 && i + 1 < argc) path = argv[++i];
    else if (std::strcmp(argv[i], "--rows") == 0) rows = true;
    else { std::fprintf(stderr, "usage: h2_engine_fz_probe --full-from FILE [--rows]\n"); return 2; }
  }
  if (!kFZ) { std::printf("h2_engine_fz_probe: not arm64: this probe sets FPCR.FZ and nothing else; NOTHING MEASURED\n"); return 2; }
  int red = 0;

  // SET: the bit must read back, and must be gone afterwards.
  const uint64_t before = fpcrGet();
  uint64_t inside = 0;
  underFz(true, [&] { inside = fpcrGet(); });
  const uint64_t after = fpcrGet();
  const bool setOk = !(before & kFZ) && (inside & kFZ) && after == before;
  if (!setOk) red++;
  std::printf("%s  SET  FPCR bit 24 (FZ): 0x%llx before, 0x%llx inside the scope, 0x%llx after\n", setOk ? "PASS" : "FAIL",
              static_cast<unsigned long long>(before), static_cast<unsigned long long>(inside), static_cast<unsigned long long>(after));

  // SEMANTICS (printed, not judged): what the bit does to a subnormal result and to a subnormal input here.
  {
    static volatile double tiny = 2.2250738585072014e-308, half = 0.5, big = 1e300;   // DBL_MIN
    static volatile uint64_t subBits = 0x0004000000000000ull;                          // DBL_MIN / 4, a subnormal, by its bits
    double res[2], inp[2];
    uint64_t fr[2], fi[2];
    for (int on = 0; on < 2; on++) {
      underFz(on != 0, [&] {
        fpsrSet(0);
        res[on] = tiny * half;
        fr[on] = fpsrGet();
        double sub;
        const uint64_t b = subBits;
        std::memcpy(&sub, &b, sizeof sub);
        fpsrSet(0);
        inp[on] = sub * big;
        fi[on] = fpsrGet();
      });
    }
    std::printf("SEMANTICS  a subnormal RESULT (DBL_MIN * 0.5): %.17g off, %.17g on (UFC %s); a subnormal INPUT (DBL_MIN/4 * 1e300): %.17g off, %.17g on (IDC %s)\n",
                res[0], res[1], fr[1] & kUFC ? "raised" : "not raised", inp[0], inp[1], fi[1] & kIDC ? "raised" : "not raised");
  }

  // DENORMAL and ZERO: the switch and the digest can see a difference, and see none where there is none.
  {
    std::vector<double> off, on;
    const uint64_t dOff = decayDigest(false, 1200, &off), dOn = decayDigest(true, 1200, &on);
    size_t differ = 0, first = 0;
    for (size_t i = 0; i < off.size(); i++) if (std::memcmp(&off[i], &on[i], sizeof(double)) != 0 && !differ++) first = i / 2;
    const bool fired = dOff != dOn && differ > 0;
    if (!fired) red++;
    std::printf("%s  control DENORMAL  y = y * 0.5 from 1, 1200 frames: %zu of %zu samples differ with FZ on, first at frame %zu; digest %s off, %s on\n",
                fired ? "PASS" : "FAIL", differ, off.size(), first, hex(dOff).c_str(), hex(dOn).c_str());
    const uint64_t zOff = decayDigest(false, 900), zOn = decayDigest(true, 900);
    if (zOff != zOn) red++;
    std::printf("%s  control ZERO  the same path stopped at 900 frames (never subnormal): digest %s off, %s on (must be equal)\n", zOff == zOn ? "PASS" : "FAIL",
                hex(zOff).c_str(), hex(zOn).c_str());
  }
  if (red) { std::printf("h2_engine_fz_probe: a control FAILED (%s build); NOTHING MEASURED\n", kBuild); return 1; }

  FILE* f = path.empty() ? nullptr : std::fopen(path.c_str(), "rb");
  if (!f) { std::fprintf(stderr, "h2_engine_fz_probe: cannot open the stream '%s'\n", path.c_str()); return 2; }
  Digests run[kScopes];
  // Per scope: how far the changed rows moved. A digest says THAT a row changed, never by how much.
  struct Moved { std::string name; size_t samples = 0, total = 0, firstFrame = 0; double maxAbs = 0; bool sizeDiffers = false; };
  std::vector<Moved> moved[kScopes];
  int flagOn = 0, flagOff = 0;
  const h2engine_hooked::Stream s = h2engine_hooked::walk(f, [&](const Scenario& sc) {
    uint64_t fpsrOff = 0, fpsrOn = 0;
    std::vector<double> pinned, other;
    const uint64_t dPinned = replayDigest(sc, kThread, pinned, &fpsrOn);
    run[kThread].emplace_back(sc.name, dPinned);
    for (int k = kOff; k < kScopes; k++) {
      const uint64_t d = replayDigest(sc, static_cast<Scope>(k), other, k == kOff ? &fpsrOff : nullptr);
      run[k].emplace_back(sc.name, d);
      if (d == dPinned) continue;
      Moved m;
      m.name = sc.name;
      m.total = pinned.size();
      m.sizeDiffers = other.size() != pinned.size();
      for (size_t i = 0; i < pinned.size() && i < other.size(); i++) {
        if (std::memcmp(&pinned[i], &other[i], sizeof(double)) == 0) continue;
        if (!m.samples++) m.firstFrame = i / 2;
        const double a = std::fabs(other[i] - pinned[i]);
        if (a > m.maxAbs || a != a) m.maxAbs = a;
      }
      moved[k].push_back(m);
    }
    if (fpsrOff & kUFC) flagOff++;
    if (fpsrOn & (kUFC | kIDC)) flagOn++;
  });
  std::fclose(f);
  const int n = static_cast<int>(run[kThread].size());
  if (!s.full()) {
    std::printf("h2_engine_fz_probe: the stream is not a full render (header %ld, END %s, %d read); NOTHING MEASURED\n", s.headerN,
                s.ended ? std::to_string(s.endN).c_str() : "ABSENT", s.read);
    return 2;
  }

  // BASELINE: the pinned scope (FZ on over the whole replay) must reproduce every pinned row, or the counts below compare against nothing.
  const KeyVerdict kv = keyVerdict(kBuild, kPlatform, compilerId(), s.nodeMajor);
  Ref ref, self;
  std::vector<std::string> refOracles;
  uint64_t refTot = 0;
  if (!kv.keyed || readRef(kv.file, ref, refOracles, refTot) != 1) {
    std::printf("h2_engine_fz_probe: no readable %s reference for %s, so the baseline cannot be proven; NOTHING MEASURED\n", kBuild, kv.key.c_str());
    return 2;
  }
  const Diff base = verdict(run[kThread], ref);
  const bool baseOk = base.size() == 0 && n == static_cast<int>(ref.size());
  printDiff("FAIL  BASELINE", base);
  std::printf("%s  BASELINE  %s build, FZ on over the whole replay (the pinned scope): %d of %d rows bit-identical to %s (%zu pinned rows)\n",
              baseOk ? "PASS" : "FAIL", kBuild, n - static_cast<int>(base.changed.size() + base.added.size()), n, kv.file.c_str(), ref.size());
  if (!baseOk) { std::printf("h2_engine_fz_probe: the baseline FAILED (%s build); NOTHING MEASURED\n", kBuild); return 1; }

  for (const auto& [name, v] : run[kThread]) self[name] = v;
  size_t offDiffer = 0;
  for (int sc = kOff; sc < kScopes; sc++) {
    const Diff d = verdict(run[sc], self);
    double worst = 0;
    size_t silent = 0;   // rows whose samples are equal: only the events or the readouts moved
    for (const Moved& m : moved[sc]) {
      if (m.maxAbs > worst || m.maxAbs != m.maxAbs) worst = m.maxAbs;
      if (!m.samples && !m.sizeDiffers) silent++;
      if (rows) std::printf("CHANGED  %-6s  %zu of %zu samples differ, first at frame %zu, largest |difference| %.3g  %s\n", kScopeName[sc], m.samples, m.total,
                            m.firstFrame, m.maxAbs, m.name.c_str());
    }
    std::printf("COUNT  %s build, %s: %zu of %d rows differ from the pinned bits; largest |difference| in any sample %.3g; %zu differ in events or readouts only\n",
                kBuild, sc == kOff ? "FZ forced OFF" : (sc == kRenderOnly ? "FZ on, scope render" : "FZ on, scope calls "), d.size(), n, worst, silent);
    if (sc == kOff) offDiffer = d.size();
    if (d.size() != moved[sc].size()) { std::printf("FAIL  the verdict's count (%zu) and the per-row comparison (%zu) disagree\n", d.size(), moved[sc].size()); red++; }
  }
  if (red) { std::printf("h2_engine_fz_probe: the counts are INCONSISTENT (%s build); NOTHING MEASURED\n", kBuild); return 1; }
  // The pin must depend on the mode: with it forced off, at least one pinned row has to differ.
  if (!offDiffer) {
    std::printf("FAIL  control OFF  %s build: with FZ forced off NO row differs from the pinned bits, so the pin does not depend on the mode\n", kBuild);
    return 1;
  }
  std::printf("FLAGS  %s build (context): %d of %d rows raised UFC or IDC with FZ on (the pinned scope); %d of %d raised UFC with FZ off\n", kBuild, flagOn, n,
              flagOff, n);
  std::printf("h2_engine_fz_probe: MEASURED — %s build, %d rows, key %s\n", kBuild, n, kv.key.c_str());
  return 0;
}
