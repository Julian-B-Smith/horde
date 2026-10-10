/*
 * h2_scalpel_parity_check — L0 parity of horde 2's SCALPEL blade engine
 * (h2/cores/scalpel/razor_core.h) against its oracle,
 * reference/scalpel/prototype/razor-core.js. ROADMAP B332 phase 1a; ADR-187
 * item 6 (parity strength), ADR-186 item 4 (copy-forward core).
 *
 * WIRED: ./verify full (after the SCALPEL oracle battery), with its FMA control.
 *
 * WHAT IS CHECKED. tools/h2_scalpel_render.mjs renders every scenario through
 * the protected oracle in Node, seeded with mulberry32, and streams the script,
 * the PRISTINE oracle's float64 samples and the blade-event digest counted by an
 * instrumented scratch copy (whose samples must equal the pristine ones bit for
 * bit on every scenario: the NONINV row). This binary replays each script
 * through the C++ core with the same seed and demands ALL of:
 *   - RMS(C++ - JS) < 1e-6 over the whole stereo render (L0-1's epsilon);
 *   - max |C++ - JS| < 1e-6 on every sample (the stated max-abs bound: no single
 *     sample may miss by more than the RMS budget, so a click cannot hide
 *     inside a good average);
 *   - identical blade-event counts AND times: every PolyBLEP correction (tryE,
 *     scan) and every blade-window entry, keyed by oversampled tick, member and
 *     kind, through two order-sensitive hashes (the core's EventLog reads and
 *     never writes).
 * Scenarios: all 83 bench presets x {held chord, repeated note, arpeggio, and
 * legato for the mono presets}, plus targeted rows (every blade mode, twins,
 * mirror, frames, rotation, the v1.1 interplay, DC modes, oversampling, the
 * laws and rules, voice management, retune, panic, a mid-note polyMode switch,
 * the modulator's 65536 wrap). The list is the render script's; this file only
 * replays it.
 *
 * EXCLUSIONS (ADR-065's evidence rule). A scenario the script marks chaotic is
 * exempt from the max-abs bound ONLY. It must still hold RMS < 1e-6 and
 * identical events, and the oracle ALONE, with its inputs perturbed by one ULP,
 * must break the max-abs bound by at least as much as the C++ misses it
 * (selfMax >= C++ max). The number of excluded scenarios is PINNED
 * (kExpectedExclusions): the list cannot grow, or shrink, silently.
 *
 * MUST-FAIL CONTROLS (L0032: a detector that shares the assumption it measures
 * confirms whatever you expect). This binary is compiled with
 * H2_SCALPEL_FAULTS, which lets it plant real port faults into the core and
 * demand each turns a named scenario red that is green without it:
 *   F1 std::round for Math.round        (spread snapped at a negative half)
 *   F2 the phase/modX draws swapped     (random start phases)
 *   F3 the blade-entry BLEP skipped      (a hard sync blade; must break BOTH the
 *                                         samples and the event digest)
 *   F5 every event one tick late          (samples bit-identical to clean: only
 *                                         the event criterion can catch it)
 * DETECTION FLOOR (documented, M3): the swarm's pitch is scaled by (1 + eps),
 * one non-hash constant, for eps = 1e-12 .. 1e-3, and the smallest eps that
 * turns the row red is printed. 1e-3 must be red (sanity); the floor is reported,
 * not judged — it states what size of arithmetic slip this gate can see.
 * F4, FMA contraction, cannot be planted at run time: it is the compiler flag
 * itself. It is the target h2_scalpel_fma_control, this same source built with
 * -ffp-contract=fast (plus -mfma on x86-64), run over the FULL stream. Its exit
 * 0 means "the contracted build missed parity on at least one row"; exit 1
 * means it passed (the control did not fire); exit 2 means the run itself broke
 * (renderer crash, truncated stream), which is never counted as firing.
 *
 * LIBM (Layer-E, printed, never judged). The stream carries probes of the
 * oracle's transcendental functions evaluated by V8; this binary evaluates the
 * same inputs with the platform libm (pow(2, x) as std::exp2, which is what
 * clang lowers the core's constant-base-2 sites to) and prints how often they
 * agree to the bit and the largest ULP gap (LIBRARY L0066's prediction).
 *
 * WHY IN full, NOT fast: Node renders ~390 scenarios twice (pristine and
 * instrumented), ~30 s on this Mac's worker threads — station_check's reason.
 *
 * Usage:  h2_scalpel_parity_check [--only REGEX] [--full-from FILE | stream-file | -]
 *   With no stream file it runs `node tools/h2_scalpel_render.mjs` itself, so
 *   it must be started from the repo root (./verify and sanitize_oracles.sh do).
 *   --full-from FILE (./verify): that renderer's full output, rendered once and fed to
 *   this check, its FMA control and the DROP control (tools/parity_floor_check.py
 *   --stream). A file cannot say it was rendered without --only, so the caller's flag
 *   declares it full, never the file. A plain stream-file or `-` is a SUBSET.
 *   The controls, the scenario floor and the exclusion pin run only on a full stream.
 *
 * SCENARIO FLOOR (ADR-206 item 1, B455 H1). A stream with scenarios dropped still read
 * green: the exclusion pin counts only the chaotic rows. The total is pinned at what the
 * renderer writes today (kMinScenarios, tools/scenario_floor.h); fewer is a failure that
 * names both numbers. The DROP control (a stream minus its last scenario, header and END
 * rewritten to agree) must read red BY the floor.
 */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define popen _popen
#define pclose _pclose
#endif

#define H2_SCALPEL_FAULTS 1
#include "../h2/cores/scalpel/razor_core.h"
#include "h2_scalpel_stream.h"
#include "scenario_floor.h"

using horde2::scalpel::EventLog;
using h2stream::Scenario;

namespace {

constexpr double kRmsTol = 1e-6;          // ACCEPTANCE L0-1's epsilon (ADR-187 item 6)
constexpr double kMaxTol = 1e-6;          // the stated max-abs bound (header)
constexpr int kExpectedExclusions = 3;    // Cross-mod ring (watch) x {chord, repeat, arp}

struct Result { double rms = 0, max = 0, exact = 0; bool evOk = true; };

Result compare(const Scenario& sc, const std::vector<double>& cpp, const EventLog& log) {
  Result r;
  if (cpp.size() != sc.js.size()) { r.rms = r.max = INFINITY; r.evOk = false; return r; }
  double e = 0;
  size_t same = 0;
  for (size_t i = 0; i < cpp.size(); i++) {
    const double d = std::fabs(cpp[i] - sc.js[i]);
    if (std::isnan(d)) { r.max = INFINITY; e = INFINITY; continue; }
    e += d * d;
    if (d > r.max) r.max = d;
    if (std::memcmp(&cpp[i], &sc.js[i], sizeof(double)) == 0) same++;
  }
  r.rms = std::sqrt(e / static_cast<double>(cpp.size()));
  r.exact = cpp.empty() ? 1 : static_cast<double>(same) / static_cast<double>(cpp.size());
  for (int k = 1; k <= 4; k++) if (log.count[k] != sc.ev[k]) r.evOk = false;
  if (log.h1 != sc.h1 || log.h2 != sc.h2) r.evOk = false;
  return r;
}
bool parityOk(const Result& r) { return r.rms < kRmsTol && r.max < kMaxTol && r.evOk; }

// ULP distance between two finite doubles of the same sign (else a large number).
double ulps(double a, double b) {
  if (std::memcmp(&a, &b, sizeof a) == 0) return 0;
  if (!std::isfinite(a) || !std::isfinite(b) || std::signbit(a) != std::signbit(b)) return 1e300;
  int64_t ia, ib;
  std::memcpy(&ia, &a, sizeof a); std::memcpy(&ib, &b, sizeof b);
  return static_cast<double>(ia > ib ? ia - ib : ib - ia);
}
double libmEval(const std::string& fn, double x, double y) {
  if (fn == "sin") return std::sin(x);
  if (fn == "cos") return std::cos(x);
  if (fn == "exp") return std::exp(x);
  if (fn == "log") return std::log(x);
  if (fn == "pow") return horde2::scalpel::js::pow(x, y);
  if (fn == "pow2") return std::exp2(y);
  if (fn == "atan2") return std::atan2(x, y);
  if (fn == "asin") return std::asin(x);
  if (fn == "tanh") return std::tanh(x);
  if (fn == "sqrt") return std::sqrt(x);
  if (fn == "hypot") return std::hypot(x, y);
  return NAN;
}
bool sameSamples(const std::vector<double>& a, const std::vector<double>& b) {
  return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(double)) == 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::string only, path;
  bool fullFrom = false;   // --full-from FILE: the caller declares FILE the full render
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--only") == 0 && i + 1 < argc) only = argv[++i];
    else if (std::strcmp(argv[i], "--full-from") == 0 && i + 1 < argc) { path = argv[++i]; fullFrom = true; }
    else path = argv[i];
  }
  if (fullFrom && !only.empty()) {
    std::fprintf(stderr, "h2_scalpel_parity_check: --full-from takes a file, and no --only\n");
    return 2;
  }
  const bool fullStream = only.empty() && (path.empty() || fullFrom);
  FILE* f = nullptr;
  bool piped = false;
  if (path == "-") {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    f = stdin;
  } else if (!path.empty()) {
    f = std::fopen(path.c_str(), "rb");
  } else {
    FILE* probe = std::fopen("tools/h2_scalpel_render.mjs", "rb");
    if (!probe) { std::fprintf(stderr, "h2_scalpel_parity_check: run from the repo root (tools/h2_scalpel_render.mjs not found)\n"); return 2; }
    std::fclose(probe);
    std::string cmd = "node tools/h2_scalpel_render.mjs";
    if (!only.empty()) cmd += " --only '" + only + "'";
#ifdef _WIN32
    f = popen(cmd.c_str(), "rb");
#else
    f = popen(cmd.c_str(), "r");
#endif
    piped = true;
  }
  if (!f) { std::fprintf(stderr, "h2_scalpel_parity_check: cannot open the oracle stream\n"); return 2; }

  std::string line;
  if (!h2stream::readLine(f, line) || line.rfind("H2SCALPEL 1", 0) != 0) {
    std::fprintf(stderr, "h2_scalpel_parity_check: bad stream header '%s'\n", line.c_str());
    return 2;
  }

  int red = 0, pass = 0, miss = 0, excluded = 0, listed = 0, total = 0, niBad = 0;
  double worstRms = 0, worstMax = 0, sumExact = 0;
  std::string worstRmsName, worstMaxName, oracle;
  uint64_t evTotal[5] = {0, 0, 0, 0, 0};
  const char* kControls[] = {"T/spread snapped -half :: chord", "T/phase random :: chord", "T/mode 0 :: chord"};
  std::vector<Scenario> keep;   // the scenarios the controls replay
  std::vector<double> cpp;
  bool ended = false, infra = false;
  while (h2stream::readLine(f, line)) {
    size_t p = 0;
    const std::string op = h2stream::word(line, p);
    if (op == "LIBM") {
      const std::string fn = h2stream::word(line, p);
      const size_t n = std::strtoull(h2stream::word(line, p).c_str(), nullptr, 10);
      std::vector<double> v(n * 3);
      if (std::fread(v.data(), sizeof(double), n * 3, f) != n * 3) { std::fprintf(stderr, "h2_scalpel_parity_check: truncated LIBM record\n"); infra = true; break; }
      size_t same = 0; double worst = 0;
      for (size_t i = 0; i < n; i++) {
        const double u = ulps(libmEval(fn, v[3 * i], v[3 * i + 1]), v[3 * i + 2]);
        if (u == 0) same++;
        if (u > worst) worst = u;
      }
      std::printf("LIBM  %-6s platform libm == V8 on %6.2f%% of %zu probes; worst gap %.0f ULP%s\n", fn.c_str(),
                  n ? 100.0 * static_cast<double>(same) / static_cast<double>(n) : 0.0, n, worst,
                  fn == "pow2" ? "  (V8 Math.pow(2,x) vs std::exp2, as clang compiles the core's base-2 sites)" : "");
      continue;
    }
    if (op == "ORACLE") { oracle = h2stream::rest(line, p); std::printf("ORACLE %s\n", oracle.c_str()); continue; }
    if (op == "END") { ended = true; break; }
    if (op != "SCN") { std::fprintf(stderr, "h2_scalpel_parity_check: unexpected stream line '%s'\n", line.c_str()); infra = true; break; }
    Scenario sc;
    if (!h2stream::readScenario(f, line, sc)) { std::fprintf(stderr, "h2_scalpel_parity_check: truncated stream in '%s'\n", sc.name.c_str()); infra = true; break; }
    EventLog log;
    h2stream::replay(sc, cpp, 0, &log);
    const Result r = compare(sc, cpp, log);
    total++;
    if (sc.ni != 1) niBad++;
    for (int k = 1; k <= 4; k++) evTotal[k] += log.count[k];
    const bool ok = parityOk(r);
    if (!sc.excl.empty()) {
      listed++;
      // exempt from max-abs ONLY: RMS and events still hold, and the JS alone
      // must miss the max bound by at least as much as the C++ does
      const bool justified = r.rms < kRmsTol && r.evOk && sc.hasSelf && sc.selfMax >= kMaxTol && sc.selfMax >= r.max;
      if (justified) excluded++; else { red++; miss++; }
      std::printf("%s  rms %.3e  max %.3e  exact %5.1f%%  ev %s  %s\n        chaotic? the JS alone, inputs 1 ULP apart: rms %.3e max %.3e -> exclusion %s (%s)\n",
                  justified ? "EXCL" : "FAIL", r.rms, r.max, 100 * r.exact, r.evOk ? "ok " : "BAD", sc.name.c_str(), sc.selfRms,
                  sc.selfMax, justified ? "justified (rms and events held; max-abs exempt)" : "NOT justified", sc.excl.c_str());
    } else {
      if (ok) pass++; else { red++; miss++; }
      if (r.rms > worstRms) { worstRms = r.rms; worstRmsName = sc.name; }
      if (r.max > worstMax) { worstMax = r.max; worstMaxName = sc.name; }
      sumExact += r.exact;
      std::printf("%s  rms %.3e  max %.3e  exact %5.1f%%  ev %s %llu/%llu/%llu/%llu  %s\n", ok ? "PASS" : "FAIL", r.rms, r.max,
                  100 * r.exact, r.evOk ? "ok " : "BAD", static_cast<unsigned long long>(log.count[1]),
                  static_cast<unsigned long long>(log.count[2]), static_cast<unsigned long long>(log.count[3]),
                  static_cast<unsigned long long>(log.count[4]), sc.name.c_str());
      if (!r.evOk)
        std::printf("        events: JS %llu/%llu/%llu/%llu hashes %u %u, C++ hashes %u %u\n", static_cast<unsigned long long>(sc.ev[1]),
                    static_cast<unsigned long long>(sc.ev[2]), static_cast<unsigned long long>(sc.ev[3]),
                    static_cast<unsigned long long>(sc.ev[4]), sc.h1, sc.h2, log.h1, log.h2);
    }
    for (const char* k : kControls) if (sc.name == k) keep.push_back(std::move(sc));
    std::fflush(stdout);
  }
  if (piped) {
    const int st = pclose(f);
    if (st != 0) { std::printf("FAIL  the oracle renderer exited with status %d\n", st); infra = true; }
  } else if (f != stdin) {
    std::fclose(f);
  }
  if (!ended) { std::printf("FAIL  the stream ended early (no END record)\n"); infra = true; }
  if (total == 0) { std::printf("FAIL  no scenarios\n"); infra = true; }
  if (oracle.empty()) { std::printf("FAIL  no ORACLE record in the stream\n"); infra = true; }
  std::printf("%s  NONINV  the instrumented scratch oracle rendered the pristine oracle's samples bit for bit on %d of %d scenarios\n",
              niBad == 0 && total > 0 ? "PASS" : "FAIL", total - niBad, total);
  if (niBad) infra = true;
  // The floor under the corpus (ADR-206 item 1, B455 H1): pinned at the count the renderer writes
  // today; raise it in the same PR that adds a scenario, never lower it without a recorded
  // decision. A short stream is infrastructure (the FMA control reports exit 2, never "fired").
  // Full streams only: a --only or plain-file subset is short by design.
  constexpr int kMinScenarios = 386;
  if (fullStream && !scenarioFloorHolds("h2_scalpel_parity", total, kMinScenarios)) infra = true;
  if (infra) red++;

#ifdef H2_SCALPEL_FMA_CONTROL
  // Inverted verdict (see header). Only a genuine parity miss on a complete,
  // well-formed stream counts as firing; a broken run is its own exit code.
  if (infra) {
    std::printf("h2_scalpel_fma_control: INFRASTRUCTURE FAILURE — the run broke (renderer, stream or instrumentation); "
                "this is NOT the control firing\n");
    return 2;
  }
  const bool caught = miss > 0;
  std::printf("h2_scalpel_fma_control: %s — the -ffp-contract=fast build of the same core %s the parity gate "
              "(%d of %d scenarios miss parity; worst rms %.3e, worst max %.3e; mean bit-exact %.2f%%)\n",
              caught ? "FIRED" : "DID NOT FIRE", caught ? "fails" : "PASSES", miss, total, worstRms, worstMax,
              total - excluded > 0 ? 100 * sumExact / (total - excluded) : 0.0);
  return caught ? 0 : 1;
#else
  const char* kFaultName[] = {"", "F1 std::round for Math.round", "F2 phase/modX draws swapped", "F3 blade-entry BLEP skipped"};
  if (fullStream && !infra) {
    if (listed != kExpectedExclusions) {
      red++;
      std::printf("FAIL  PIN  %d scenarios are marked chaotic; the pinned count is %d (the exclusion list changed: re-pin it deliberately)\n",
                  listed, kExpectedExclusions);
    } else {
      std::printf("PASS  PIN  the exclusion list holds its pinned %d scenarios\n", kExpectedExclusions);
    }
    const Scenario* mode0 = nullptr;
    for (const Scenario& s : keep) if (s.name == kControls[2]) mode0 = &s;
    for (int fault = 1; fault <= 3; fault++) {
      const Scenario* sc = nullptr;
      for (const Scenario& s : keep) if (s.name == kControls[fault - 1]) sc = &s;
      if (!sc) { std::printf("FAIL  control %s: its scenario '%s' is not in the stream\n", kFaultName[fault], kControls[fault - 1]); red++; continue; }
      EventLog clog, flog;
      std::vector<double> a, b;
      h2stream::replay(*sc, a, 0, &clog);
      h2stream::replay(*sc, b, fault, &flog);
      const Result clean = compare(*sc, a, clog), r = compare(*sc, b, flog);
      bool fired = parityOk(clean) && !parityOk(r);
      if (fault == 3) fired = fired && !r.evOk && (r.rms >= kRmsTol || r.max >= kMaxTol);   // both criteria must see it
      if (!fired) red++;
      std::printf("%s  control %s turns '%s' red: rms %.3e max %.3e, events %s (clean: rms %.3e, events %s)\n", fired ? "PASS" : "FAIL",
                  kFaultName[fault], sc->name.c_str(), r.rms, r.max, r.evOk ? "agree" : "DISAGREE", clean.rms, clean.evOk ? "agree" : "DISAGREE");
    }
    if (mode0) {
      // F5: an EVENT-ONLY fault. The samples must stay bit-identical to the clean
      // replay, so only the event criterion can turn this row red.
      EventLog clog, flog;
      std::vector<double> a, b;
      h2stream::replay(*mode0, a, 0, &clog);
      h2stream::replay(*mode0, b, 5, &flog);
      const Result r = compare(*mode0, b, flog);
      const bool fired = sameSamples(a, b) && !r.evOk && r.rms < kRmsTol && r.max < kMaxTol;
      if (!fired) red++;
      std::printf("%s  control F5 every event one tick late turns '%s' red on events alone: samples %s, events %s\n",
                  fired ? "PASS" : "FAIL", mode0->name.c_str(), sameSamples(a, b) ? "bit-identical to clean" : "CHANGED",
                  r.evOk ? "agree" : "DISAGREE");
      // DETECTION FLOOR: the swarm's pitch scaled by (1 + eps).
      std::string ladder;
      double floor = 0;
      bool topRed = false;
      for (int ex = -12; ex <= -3; ex++) {
        const double eps = std::pow(10.0, ex);
        EventLog elog;
        std::vector<double> c;
        h2stream::replay(*mode0, c, 0, &elog, eps);
        const Result r2 = compare(*mode0, c, elog);
        const bool isRed = !parityOk(r2);
        if (isRed && floor == 0) floor = eps;
        if (!isRed) floor = 0;   // the floor is the smallest eps from which EVERY larger one is red
        if (ex == -3) topRed = isRed;
        char buf[160];
        std::snprintf(buf, sizeof buf, "\n        eps 1e%d: rms %.3e max %.3e events %s -> %s", ex, r2.rms, r2.max, r2.evOk ? "agree" : "DISAGREE", isRed ? "RED" : "green");
        ladder += buf;
      }
      if (!topRed) red++;
      std::printf("%s  FLOOR  a relative pitch error eps on the swarm (couple()'s f, one non-hash constant) on '%s'; "
                  "detection floor = %s%s\n", topRed ? "PASS" : "FAIL", mode0->name.c_str(),
                  floor > 0 ? (std::string("1e") + std::to_string(static_cast<int>(std::lround(std::log10(floor))))).c_str() : "none within 1e-12..1e-3",
                  ladder.c_str());
    }
    if (!keep.empty()) {   // C++ determinism: the same script twice is bit-identical
      std::vector<double> a, b;
      h2stream::replay(keep[0], a);
      h2stream::replay(keep[0], b);
      const bool same = sameSamples(a, b);
      if (!same) red++;
      std::printf("%s  DET  the C++ core replays '%s' bit-identically\n", same ? "PASS" : "FAIL", keep[0].name.c_str());
    }
  }
  std::printf("events counted (C++ side): edge/base BLEPs %llu, carrier BLEPs %llu, blade-1 entries %llu, blade-2 entries %llu\n",
              static_cast<unsigned long long>(evTotal[1]), static_cast<unsigned long long>(evTotal[2]),
              static_cast<unsigned long long>(evTotal[3]), static_cast<unsigned long long>(evTotal[4]));
  std::printf("worst rms %.3e (%s); worst max-abs %.3e (%s); mean bit-exact samples %.2f%%\n", worstRms, worstRmsName.c_str(),
              worstMax, worstMaxName.c_str(), total - excluded > 0 ? 100 * sumExact / (total - excluded) : 0.0);
  std::printf("h2_scalpel_parity_check: %s — %d/%d scenarios at parity (rms < %.0e, max < %.0e, events identical), %d excluded as chaotic with evidence, %d red\n",
              red ? "RED" : "GREEN", pass, total, kRmsTol, kMaxTol, excluded, red);
  return red ? 1 : 0;
#endif
}
