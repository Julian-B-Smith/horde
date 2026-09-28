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
 * the float64 samples and the oracle's blade-event digest. This binary replays
 * each script through the C++ core with the same seed and demands ALL of:
 *   - RMS(C++ - JS) < 1e-6 over the whole stereo render (L0-1's epsilon);
 *   - max |C++ - JS| < 1e-6 on every sample (the stated max-abs bound: no single
 *     sample may miss by more than the RMS budget, so a click cannot hide
 *     inside a good average);
 *   - identical blade-event counts AND times: every PolyBLEP correction (tryE,
 *     scan) and every blade-window entry, keyed by oversampled tick, member and
 *     kind, through two order-sensitive hashes. The JS side is counted by a
 *     scratch copy of the oracle (the render script's header), the C++ side by
 *     the core's EventLog, which reads and never writes.
 * Scenarios: all 83 bench presets x {held chord, repeated note, arpeggio, and
 * legato for the mono presets}, plus targeted rows for every blade mode, twins,
 * mirror, frames, rotation, the v1.1 interplay (serial lambda, collision pitch
 * and bite), DC modes, oversampling, the swarm laws, the cut rules and voice
 * management. The list is the render script's; this file only replays it.
 *
 * EXCLUSIONS (ADR-065's evidence rule). A scenario the script marks chaotic is
 * accepted as excluded ONLY if the oracle ALONE, with its inputs perturbed by
 * one ULP, breaks the same gate (its SELF rms or max over the bound) and, when
 * the C++ diverges too, diverges comparably (JS self-divergence at least a
 * tenth of the C++ one). Otherwise the exclusion itself is RED. Excluded rows
 * still print their C++-vs-JS numbers and event agreement.
 *
 * MUST-FAIL CONTROLS (L0032: a detector that shares the assumption it measures
 * confirms whatever you expect). This binary is compiled with
 * H2_SCALPEL_FAULTS, which lets it plant three real port faults into the core
 * and demand each turns a named scenario red that is green without it:
 *   F1 std::round for Math.round        (spread snapped at a negative half)
 *   F2 the phase/modX draws swapped     (random start phases)
 *   F3 the blade-entry BLEP skipped      (a hard sync blade)
 * The fourth fault, FMA contraction, cannot be planted at run time: it is the
 * compiler flag itself. It is the separate target h2_scalpel_fma_control, this
 * same source built with -ffp-contract=fast (plus -mfma on x86-64, where
 * contraction has no instruction to use otherwise), whose exit 0 means "the
 * contracted build was caught" — so a sanitizer run that executes every wired
 * binary reads it correctly.
 *
 * LIBM (Layer-E, printed, never judged). The stream carries probes of the
 * oracle's transcendental functions evaluated by V8; this binary evaluates the
 * same inputs with the platform libm and prints how often they agree to the
 * bit and the largest ULP gap (LIBRARY L0066's prediction, measured).
 *
 * WHY IN full, NOT fast: Node renders ~370 scenarios (~15 s on this Mac's
 * worker threads) — station_check's reason. Deterministic, no model calls.
 *
 * Usage:  h2_scalpel_parity_check [--only REGEX] [stream-file | -]
 *   With no stream file it runs `node tools/h2_scalpel_render.mjs` itself, so
 *   it must be started from the repo root (./verify and sanitize_oracles.sh do).
 *   The must-fail controls run only on a full, unfiltered stream.
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

using horde2::scalpel::EventLog;
using h2stream::Scenario;

namespace {

constexpr double kRmsTol = 1e-6;   // ACCEPTANCE L0-1's epsilon (ADR-187 item 6)
constexpr double kMaxTol = 1e-6;   // the stated max-abs bound (header)

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
  if (fn == "atan2") return std::atan2(x, y);
  if (fn == "asin") return std::asin(x);
  if (fn == "tanh") return std::tanh(x);
  if (fn == "sqrt") return std::sqrt(x);
  if (fn == "hypot") return std::hypot(x, y);
  return NAN;
}

}  // namespace

int main(int argc, char** argv) {
  std::string only, path;
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--only") == 0 && i + 1 < argc) only = argv[++i];
    else path = argv[i];
  }
#ifdef H2_SCALPEL_FMA_CONTROL
  if (only.empty() && path.empty()) only = "^T/";   // the targeted rows reach every arithmetic path, in ~5 s
#endif
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
    if (!probe) { std::fprintf(stderr, "h2_scalpel_parity_check: run from the repo root (tools/h2_scalpel_render.mjs not found)\n"); return 1; }
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
  if (!f) { std::fprintf(stderr, "h2_scalpel_parity_check: cannot open the oracle stream\n"); return 1; }

  std::string line;
  if (!h2stream::readLine(f, line) || line.rfind("H2SCALPEL 1", 0) != 0) {
    std::fprintf(stderr, "h2_scalpel_parity_check: bad stream header '%s'\n", line.c_str());
    return 1;
  }

  int red = 0, pass = 0, excluded = 0, total = 0;
  double worstRms = 0, worstMax = 0, sumExact = 0;
  std::string worstRmsName, worstMaxName;
  uint64_t evTotal[5] = {0, 0, 0, 0, 0};
  int nonInvOk = -1;
  const char* kControls[] = {"T/spread snapped -half :: chord", "T/phase random :: chord", "T/mode 0 :: chord"};
  std::vector<Scenario> keep;   // the scenarios the controls replay
  std::vector<double> cpp;
  bool ended = false;
  while (h2stream::readLine(f, line)) {
    size_t p = 0;
    const std::string op = h2stream::word(line, p);
    if (op == "LIBM") {
      const std::string fn = h2stream::word(line, p);
      const size_t n = std::strtoull(h2stream::word(line, p).c_str(), nullptr, 10);
      std::vector<double> v(n * 3);
      if (std::fread(v.data(), sizeof(double), n * 3, f) != n * 3) { std::fprintf(stderr, "h2_scalpel_parity_check: truncated LIBM record\n"); return 1; }
      size_t same = 0; double worst = 0;
      for (size_t i = 0; i < n; i++) {
        const double u = ulps(libmEval(fn, v[3 * i], v[3 * i + 1]), v[3 * i + 2]);
        if (u == 0) same++;
        if (u > worst) worst = u;
      }
      std::printf("LIBM  %-6s platform libm == V8 on %6.2f%% of %zu probes; worst gap %.0f ULP\n", fn.c_str(),
                  n ? 100.0 * static_cast<double>(same) / static_cast<double>(n) : 0.0, n, worst);
      continue;
    }
    if (op == "NONINV") {
      nonInvOk = std::atoi(h2stream::word(line, p).c_str());
      const std::string n = h2stream::word(line, p), s = h2stream::word(line, p);
      std::printf("%s  NONINV  the instrumented scratch oracle renders the pristine oracle's samples bit for bit (%s scenarios, %s samples)\n",
                  nonInvOk == 1 ? "PASS" : "FAIL", n.c_str(), s.c_str());
      if (nonInvOk != 1) red++;
      continue;
    }
    if (op == "END") { ended = true; break; }
    if (op != "SCN") { std::fprintf(stderr, "h2_scalpel_parity_check: unexpected stream line '%s'\n", line.c_str()); return 1; }
    Scenario sc;
    if (!h2stream::readScenario(f, line, sc)) { std::fprintf(stderr, "h2_scalpel_parity_check: truncated stream in '%s'\n", sc.name.c_str()); return 1; }
    EventLog log;
    h2stream::replay(sc, cpp, 0, &log);
    const Result r = compare(sc, cpp, log);
    total++;
    for (int k = 1; k <= 4; k++) evTotal[k] += log.count[k];
    const bool ok = parityOk(r);
    if (!sc.excl.empty()) {
      const bool jsBreaks = sc.hasSelf && (sc.selfRms >= kRmsTol || sc.selfMax >= kMaxTol);
      const bool comparable = ok || sc.selfRms >= 0.1 * r.rms;
      const bool justified = jsBreaks && comparable;
      if (justified) excluded++; else red++;
      std::printf("%s  rms %.3e  max %.3e  exact %5.1f%%  ev %s  %s\n        chaotic? the JS alone, inputs 1 ULP apart: rms %.3e max %.3e -> exclusion %s (%s)\n",
                  justified ? "EXCL" : "FAIL", r.rms, r.max, 100 * r.exact, r.evOk ? "ok " : "BAD", sc.name.c_str(), sc.selfRms,
                  sc.selfMax, justified ? "justified" : "NOT justified", sc.excl.c_str());
    } else {
      if (ok) pass++; else red++;
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
    if (st != 0) { std::printf("FAIL  the oracle renderer exited with status %d\n", st); red++; }
  } else if (f != stdin) {
    std::fclose(f);
  }
  if (!ended) { std::printf("FAIL  the stream ended early (no END record)\n"); red++; }
  if (nonInvOk < 0) { std::printf("FAIL  no non-invasiveness record in the stream\n"); red++; }
  if (total == 0) { std::printf("FAIL  no scenarios\n"); red++; }

#ifdef H2_SCALPEL_FMA_CONTROL
  // Inverted verdict: this build contracts a*b+c into fused multiply-adds, a
  // port fault (V8 never fuses). Exit 0 iff the parity gate caught it.
  const bool caught = red > 0;
  std::printf("h2_scalpel_fma_control: %s — the -ffp-contract=fast build of the same core %s the parity gate "
              "(%d of %d scenarios red; worst rms %.3e, worst max %.3e; mean bit-exact %.2f%%)\n",
              caught ? "FIRED" : "DID NOT FIRE", caught ? "fails" : "PASSES", red, total, worstRms, worstMax,
              total - excluded > 0 ? 100 * sumExact / (total - excluded) : 0.0);
  return caught ? 0 : 1;
#else
  const char* kFaultName[] = {"", "F1 std::round for Math.round", "F2 phase/modX draws swapped", "F3 blade-entry BLEP skipped"};
  if (only.empty() && path.empty()) {
    for (int fault = 1; fault <= 3; fault++) {
      const Scenario* sc = nullptr;
      for (const Scenario& s : keep) if (s.name == kControls[fault - 1]) sc = &s;
      if (!sc) { std::printf("FAIL  control %s: its scenario '%s' is not in the stream\n", kFaultName[fault], kControls[fault - 1]); red++; continue; }
      EventLog clog, flog;
      std::vector<double> a, b;
      h2stream::replay(*sc, a, 0, &clog);
      h2stream::replay(*sc, b, fault, &flog);
      const Result clean = compare(*sc, a, clog), r = compare(*sc, b, flog);
      const bool fired = parityOk(clean) && !parityOk(r);
      if (!fired) red++;
      std::printf("%s  control %s turns '%s' red: rms %.3e max %.3e, events %s (clean: rms %.3e, events %s)\n", fired ? "PASS" : "FAIL",
                  kFaultName[fault], sc->name.c_str(), r.rms, r.max, r.evOk ? "agree" : "DISAGREE", clean.rms, clean.evOk ? "agree" : "DISAGREE");
    }
    if (!keep.empty()) {   // C++ determinism: the same script twice is bit-identical
      std::vector<double> a, b;
      h2stream::replay(keep[0], a);
      h2stream::replay(keep[0], b);
      const bool same = a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(double)) == 0;
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
