/*
 * h2_engine_parity_check — L0 parity of horde 2's composed engine
 * (h2/engine/engine.h) against its golden, the composed JS
 * (docs/design/scalpel-horde-engine.js over reference/scalpel/prototype/razor-core.js
 * and reference/swarmsaw.html's SwarmSynth). ROADMAP B385; design and plan:
 * docs/port/h2-engine.md; ADR-187 items 3 and 6.
 *
 * UNWIRED: the golden's Cross-mod ring (watch) rows are chaotic (ADR-065) and miss RMS
 * by chance under phase 1a's exclusion rule; wired once the lead rules (B385 report).
 *
 * WHAT IS CHECKED. tools/h2_engine_render.mjs renders every scenario through
 * the golden in Node, seeded with mulberry32, and streams the script, the
 * PRISTINE golden's float64 samples and the blade-event digest counted by an
 * instrumented scratch copy (whose samples must equal the pristine ones bit for
 * bit: NONINV). This binary replays each script through the C++ engine with the
 * same seed and demands, per scenario, ALL of:
 *   - RMS(C++ - JS) < 1e-6 over the whole stereo render;
 *   - max |C++ - JS| < 1e-6 on every sample (ruled 2026-09-30, B332);
 *   - identical blade events: every PolyBLEP correction and blade-window entry,
 *     keyed by oversampled tick, member and kind, through two order-sensitive
 *     hashes;
 * and, over the whole stream:
 *   - the BIT-EXACT FLOOR (ruled 2026-09-30, B332: its value set from the measured
 *     baseline with a stated margin): the MEAN share of samples bit-identical to
 *     the golden's, over the scenarios held to parity, is at least kFloor. PROPOSED
 *     form, for ratification: a per-scenario floor was measured and rejected,
 *     because one scenario's share moves by up to 57 points when an input moves
 *     one ULP (L0071's probe; the share is the time before the first libm
 *     disagreement reaches the output), while the mean moves by 1.3. The share
 *     depends on the platform's libm (L0066), so the floor is PINNED PER PLATFORM;
 *     a platform with no pin reports it and SKIPS it, never passes it.
 * Families: P/ the 83 bench presets x phrases; E/ the 12 B366 lab presets x
 * phrases; T/ phase 1a's blade rows; C/ the composed rows (the voice law, the
 * cap and cull, the first tick, gravity, the ensemble, D1-D3, M1-M3 at three
 * rates, the swarm's parameters, A2, os). The list is the render script's.
 *
 * THE TARGET IS PINNED BY CONTENT. Each golden file's git blob arrives in an
 * ORACLE record and must appear in h2/README.md as `<path>@<blob>`.
 *
 * EXCLUSIONS (ADR-065's evidence rule, as phase 1a applies it). A scenario the
 * script marks chaotic is exempt from max-abs (and the floor) ONLY: it must hold
 * RMS < 1e-6 and identical events, and the golden ALONE, inputs one ULP apart,
 * must miss max-abs by at least as much as the C++ does. The count is PINNED.
 *
 * MUST-FAIL CONTROLS (LIBRARY L0032). Compiled with H2_ENGINE_FAULTS, so real
 * faults can be planted in the engine itself; each must turn its row red:
 *   A2a the oracle's Math.round on a Quantized Cut spread (A2's half away from
 *       zero dropped)                 the Cut spread at -2.5
 *   A2b Rotate spread's sign dropped  a negative Rotate spread
 *   F2 the start-phase and modulator draws swapped   a free-running FM blade
 *   F3 the blade-entry BLEP skipped   a hard sync blade (samples AND events)
 *   F5 every event one tick late      samples bit-identical: events ALONE
 *   V1 the blade oracle's voice law   the repeated note (B310)
 *   T1 the swarm tick a sample late   a coupled chord
 *   L1 B325's look-ahead dropped      a lock-2 chord on a mono voice
 *   K1 M1 reverted (0.08 per tick)    K 1 at 48 kHz
 * DETECTION FLOOR (printed, not judged; 1e-3 must be red): the swarm's pitch
 * scaled by (1 + eps). DETERMINISM: the same script twice is bit-identical.
 * FMA CONTROL: h2_engine_fma_control is this source at -ffp-contract=fast, over
 * the full stream; exit 0 fired, 1 did not fire, 2 the run broke.
 *
 * Usage: h2_engine_parity_check [--only REGEX] [stream-file | -]
 *   With NO arguments (./verify, tools/sanitize_oracles.sh) it spawns
 *   `node tools/h2_engine_render.mjs` itself, from the repo root. The controls,
 *   the floor, the pins and the exclusion count run only on the full stream.
 */
#include <algorithm>
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

#define H2_ENGINE_FAULTS 1
#include "../h2/engine/engine.h"
#include "h2_engine_stream.h"

using horde2::engine::EventLog;
using h2engine_stream::Scenario;

namespace {

constexpr double kRmsTol = 1e-6;
constexpr double kMaxTol = 1e-6;
constexpr int kExpectedExclusions = 3;   // Cross-mod ring (watch) x {chord, repeat, arp}
// The bit-exact floor on the MEAN share over the scenarios held to parity, per
// platform (see header). A negative value: not pinned here, reported and skipped.
// darwin-arm64 (Apple clang, Node 24.10, 2026-10-01): 37.21% at the scripts'
// own inputs, 38.49% and 37.85% with every note-on frequency nudged 1 and 2 ULP
// (L0071's probe); the floor sits 7 points under the lowest.
#if defined(__APPLE__) && defined(__aarch64__)
constexpr const char* kPlatform = "darwin-arm64";
constexpr double kFloor = 0.30;
#elif defined(__linux__) && defined(__x86_64__)
constexpr const char* kPlatform = "linux-x86_64";
constexpr double kFloor = -1;
#else
constexpr const char* kPlatform = "unpinned";
constexpr double kFloor = -1;
#endif

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
  if (fn == "log2") return std::log2(x);
  if (fn == "pow") return horde2::engine::js::pow(x, y);
  if (fn == "pow2") return std::exp2(y);
  if (fn == "atan2") return std::atan2(x, y);
  if (fn == "asin") return std::asin(x);
  if (fn == "tan") return std::tan(x);
  if (fn == "tanh") return std::tanh(x);
  if (fn == "sqrt") return std::sqrt(x);
  return NAN;
}
bool sameSamples(const std::vector<double>& a, const std::vector<double>& b) {
  return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(double)) == 0;
}
std::string slurp(const char* path) {
  std::string s;
  FILE* f = std::fopen(path, "rb");
  if (!f) return s;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
  std::fclose(f);
  return s;
}

struct Control { int fault; const char* name; const char* scenario; };
const Control kControls[] = {
  {1, "A2a the oracle's Math.round on a Quantized Cut spread", "C/A2 Quantized Cut spread -2.5"},
  {10, "A2b Rotate spread's sign dropped", "C/A2 negative Rotate spread"},
  {2, "F2 start-phase and modulator draws swapped", "T/fm mode 2 type 0 :: chord"},
  {3, "F3 blade-entry BLEP skipped", "T/mode 0 :: chord"},
  {6, "V1 the blade oracle's voice law", "C/VL repeat, the first release rings"},
  {7, "T1 the swarm tick a sample late", "C/M1 K 1 at 48000"},
  {8, "L1 B325's look-ahead dropped", "C/FT chord on a mono voice in one block"},
  {9, "K1 M1 reverted (0.08 per tick)", "C/M1 K 1 at 48000"},
};

}  // namespace

int main(int argc, char** argv) {
  std::string only, path;
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--only") == 0 && i + 1 < argc) only = argv[++i];
    else path = argv[i];
  }
  const bool fullStream = only.empty() && path.empty();
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
    FILE* probe = std::fopen("tools/h2_engine_render.mjs", "rb");
    if (!probe) { std::fprintf(stderr, "h2_engine_parity_check: run from the repo root (tools/h2_engine_render.mjs not found)\n"); return 2; }
    std::fclose(probe);
    std::string cmd = "node tools/h2_engine_render.mjs";
    if (!only.empty()) cmd += " --only '" + only + "'";
#ifdef _WIN32
    f = popen(cmd.c_str(), "rb");
#else
    f = popen(cmd.c_str(), "r");
#endif
    piped = true;
  }
  if (!f) { std::fprintf(stderr, "h2_engine_parity_check: cannot open the golden's stream\n"); return 2; }

  std::string line;
  if (!h2engine_stream::readLine(f, line) || line.rfind("H2ENGINE 1", 0) != 0) {
    std::fprintf(stderr, "h2_engine_parity_check: bad stream header '%s'\n", line.c_str());
    return 2;
  }

  int red = 0, pass = 0, miss = 0, excluded = 0, listed = 0, total = 0, niBad = 0;
  double worstRms = 0, worstMax = 0, sumExact = 0, minExact = 2;
  int heldN = 0;   // the scenarios held to parity (every one not listed chaotic): the floor's population
  std::string worstRmsName, worstMaxName, minExactName;
  std::vector<std::string> oracles;
  uint64_t evTotal[5] = {0, 0, 0, 0, 0};
  // per family: count, red, worst rms, worst max, min exact share
  struct Fam { char tag; int n = 0, redN = 0; double rms = 0, max = 0, minEx = 2; };
  Fam fams[4] = {{'P'}, {'E'}, {'T'}, {'C'}};
  std::vector<Scenario> keep;
  std::vector<std::pair<double, std::string>> shares;
  std::vector<double> cpp;
  bool ended = false, infra = false;
  while (h2engine_stream::readLine(f, line)) {
    size_t p = 0;
    const std::string op = h2engine_stream::word(line, p);
    if (op == "LIBM") {
      const std::string fn = h2engine_stream::word(line, p);
      const size_t n = std::strtoull(h2engine_stream::word(line, p).c_str(), nullptr, 10);
      std::vector<double> v(n * 3);
      if (std::fread(v.data(), sizeof(double), n * 3, f) != n * 3) { std::fprintf(stderr, "h2_engine_parity_check: truncated LIBM record\n"); infra = true; break; }
      size_t same = 0; double worst = 0;
      for (size_t i = 0; i < n; i++) {
        const double u = ulps(libmEval(fn, v[3 * i], v[3 * i + 1]), v[3 * i + 2]);
        if (u == 0) same++;
        if (u > worst) worst = u;
      }
      std::printf("LIBM  %-6s platform libm == V8 on %6.2f%% of %zu probes; worst gap %.0f ULP%s\n", fn.c_str(),
                  n ? 100.0 * static_cast<double>(same) / static_cast<double>(n) : 0.0, n, worst,
                  fn == "pow2" ? "  (V8 Math.pow(2,x) vs std::exp2, as clang compiles the engine's base-2 sites)" : "");
      continue;
    }
    if (op == "ORACLE") { oracles.push_back(h2engine_stream::rest(line, p)); std::printf("ORACLE %s\n", oracles.back().c_str()); continue; }
    if (op == "END") { ended = true; break; }
    if (op != "SCN") { std::fprintf(stderr, "h2_engine_parity_check: unexpected stream line '%s'\n", line.c_str()); infra = true; break; }
    Scenario sc;
    if (!h2engine_stream::readScenario(f, line, sc)) { std::fprintf(stderr, "h2_engine_parity_check: truncated stream in '%s'\n", sc.name.c_str()); infra = true; break; }
    EventLog log;
    h2engine_stream::replay(sc, cpp, 0, &log);
    const Result r = compare(sc, cpp, log);
    total++;
    if (sc.ni != 1) niBad++;
    for (int k = 1; k <= 4; k++) evTotal[k] += log.count[k];
    Fam* fam = nullptr;
    for (Fam& x : fams) if (!sc.name.empty() && sc.name[0] == x.tag) fam = &x;
    const bool ok = parityOk(r);
    if (!sc.excl.empty()) {
      listed++;
      const bool justified = r.rms < kRmsTol && r.evOk && sc.hasSelf && sc.selfMax >= kMaxTol && sc.selfMax >= r.max;
      if (justified) excluded++; else { red++; if (fam) fam->redN++; }
      std::printf("%s  rms %.3e  max %.3e  exact %5.1f%%  ev %s  %s\n        chaotic? the golden alone, inputs 1 ULP apart: rms %.3e max %.3e -> exclusion %s (%s)\n",
                  justified ? "EXCL" : "FAIL", r.rms, r.max, 100 * r.exact, r.evOk ? "ok " : "BAD", sc.name.c_str(), sc.selfRms,
                  sc.selfMax, justified ? "justified (rms and events held; max-abs exempt)" : "NOT justified", sc.excl.c_str());
    } else {
      if (ok) pass++; else { red++; miss++; if (fam) fam->redN++; }
      if (r.rms > worstRms) { worstRms = r.rms; worstRmsName = sc.name; }
      if (r.max > worstMax) { worstMax = r.max; worstMaxName = sc.name; }
      if (r.exact < minExact) { minExact = r.exact; minExactName = sc.name; }
      sumExact += r.exact; heldN++;
      shares.emplace_back(r.exact, sc.name);
      if (fam) { fam->n++; fam->rms = std::max(fam->rms, r.rms); fam->max = std::max(fam->max, r.max); fam->minEx = std::min(fam->minEx, r.exact); }
      std::printf("%s  rms %.3e  max %.3e  exact %6.2f%%  ev %s %llu/%llu/%llu/%llu  %s\n", ok ? "PASS" : "FAIL", r.rms, r.max,
                  100 * r.exact, r.evOk ? "ok " : "BAD", static_cast<unsigned long long>(log.count[1]),
                  static_cast<unsigned long long>(log.count[2]), static_cast<unsigned long long>(log.count[3]),
                  static_cast<unsigned long long>(log.count[4]), sc.name.c_str());
      if (!r.evOk)
        std::printf("        events: golden %llu/%llu/%llu/%llu hashes %u %u, C++ hashes %u %u\n", static_cast<unsigned long long>(sc.ev[1]),
                    static_cast<unsigned long long>(sc.ev[2]), static_cast<unsigned long long>(sc.ev[3]),
                    static_cast<unsigned long long>(sc.ev[4]), sc.h1, sc.h2, log.h1, log.h2);
    }
    bool want = false;
    for (const Control& c : kControls) if (sc.name == c.scenario) want = true;
    if (want) keep.push_back(std::move(sc));
    std::fflush(stdout);
  }
  if (piped) {
    const int st = pclose(f);
    if (st != 0) { std::printf("FAIL  the golden's renderer exited with status %d\n", st); infra = true; }
  } else if (f != stdin) {
    std::fclose(f);
  }
  if (!ended) { std::printf("FAIL  the stream ended early (no END record)\n"); infra = true; }
  if (total == 0) { std::printf("FAIL  no scenarios\n"); infra = true; }
  if (oracles.size() != 5) { std::printf("FAIL  %zu ORACLE records in the stream (want the golden's 5 files)\n", oracles.size()); infra = true; }
  std::printf("%s  NONINV  the instrumented scratch golden rendered the pristine golden's samples bit for bit on %d of %d scenarios\n",
              niBad == 0 && total > 0 ? "PASS" : "FAIL", total - niBad, total);
  if (niBad) infra = true;
  if (infra) red++;

#ifdef H2_ENGINE_FMA_CONTROL
  if (infra) {
    std::printf("h2_engine_fma_control: INFRASTRUCTURE FAILURE — the run broke (renderer, stream or instrumentation); "
                "this is NOT the control firing\n");
    return 2;
  }
  const bool caught = miss > 0;
  std::printf("h2_engine_fma_control: %s — the -ffp-contract=fast build of the same engine %s the parity gate "
              "(%d of %d scenarios miss parity; worst rms %.3e, worst max %.3e; mean bit-exact %.2f%%)\n",
              caught ? "FIRED" : "DID NOT FIRE", caught ? "fails" : "PASSES", miss, total, worstRms, worstMax,
              heldN > 0 ? 100 * sumExact / heldN : 0.0);
  return caught ? 0 : 1;
#else
  if (fullStream && !infra) {
    // the target, pinned by content
    const std::string readme = slurp("h2/README.md");
    for (const std::string& o : oracles) {
      const size_t sp = o.find(' ');
      const std::string pin = o.substr(0, sp) + "@" + o.substr(sp + 1);
      const bool pinned = !readme.empty() && readme.find(pin) != std::string::npos;
      if (!pinned) red++;
      std::printf("%s  PIN  %s %s\n", pinned ? "PASS" : "FAIL", pin.c_str(),
                  pinned ? "is h2/README.md's pin" : "is NOT pinned in h2/README.md: the golden moved, re-pin it on purpose (ADR-187 item 3)");
    }
    if (listed != kExpectedExclusions) {
      red++;
      std::printf("FAIL  PIN  %d scenarios are marked chaotic; the pinned count is %d (the exclusion list changed: re-pin it deliberately)\n",
                  listed, kExpectedExclusions);
    } else {
      std::printf("PASS  PIN  the exclusion list holds its pinned %d scenarios\n", kExpectedExclusions);
    }
    for (const Control& c : kControls) {
      const Scenario* sc = nullptr;
      for (const Scenario& s : keep) if (s.name == c.scenario) sc = &s;
      if (!sc) { std::printf("FAIL  control %s: its scenario '%s' is not in the stream\n", c.name, c.scenario); red++; continue; }
      EventLog clog, flog;
      std::vector<double> a, b;
      h2engine_stream::replay(*sc, a, 0, &clog);
      h2engine_stream::replay(*sc, b, c.fault, &flog);
      const Result clean = compare(*sc, a, clog), r = compare(*sc, b, flog);
      bool fired = parityOk(clean) && !parityOk(r);
      if (c.fault == 3) fired = fired && !r.evOk && (r.rms >= kRmsTol || r.max >= kMaxTol);   // both criteria must see it
      if (!fired) red++;
      std::printf("%s  control %s turns '%s' red: rms %.3e max %.3e, events %s (clean: rms %.3e, events %s)\n", fired ? "PASS" : "FAIL",
                  c.name, sc->name.c_str(), r.rms, r.max, r.evOk ? "agree" : "DISAGREE", clean.rms, clean.evOk ? "agree" : "DISAGREE");
    }
    const Scenario* mode0 = nullptr;
    for (const Scenario& s : keep) if (s.name == "T/mode 0 :: chord") mode0 = &s;
    if (mode0) {
      // F5: an EVENT-ONLY fault; the samples must stay bit-identical to the clean replay
      EventLog clog, flog;
      std::vector<double> a, b;
      h2engine_stream::replay(*mode0, a, 0, &clog);
      h2engine_stream::replay(*mode0, b, 5, &flog);
      const Result r = compare(*mode0, b, flog);
      const bool fired = sameSamples(a, b) && !r.evOk && r.rms < kRmsTol && r.max < kMaxTol;
      if (!fired) red++;
      std::printf("%s  control F5 every event one tick late turns '%s' red on events alone: samples %s, events %s\n",
                  fired ? "PASS" : "FAIL", mode0->name.c_str(), sameSamples(a, b) ? "bit-identical to clean" : "CHANGED",
                  r.evOk ? "agree" : "DISAGREE");
      // DETECTION FLOOR: the swarm's pitch scaled by (1 + eps)
      std::string ladder;
      double floorEps = 0;
      bool topRed = false;
      for (int ex = -12; ex <= -3; ex++) {
        const double eps = std::pow(10.0, ex);
        EventLog elog;
        std::vector<double> c;
        h2engine_stream::replay(*mode0, c, 0, &elog, eps);
        const Result r2 = compare(*mode0, c, elog);
        const bool isRed = !parityOk(r2);
        if (isRed && floorEps == 0) floorEps = eps;
        if (!isRed) floorEps = 0;
        if (ex == -3) topRed = isRed;
        char buf[160];
        std::snprintf(buf, sizeof buf, "\n        eps 1e%d: rms %.3e max %.3e events %s -> %s", ex, r2.rms, r2.max, r2.evOk ? "agree" : "DISAGREE", isRed ? "RED" : "green");
        ladder += buf;
      }
      if (!topRed) red++;
      std::printf("%s  FLOOR  a relative pitch error eps on the swarm (each tick's f0) on '%s'; detection floor = %s%s\n",
                  topRed ? "PASS" : "FAIL", mode0->name.c_str(),
                  floorEps > 0 ? (std::string("1e") + std::to_string(static_cast<int>(std::lround(std::log10(floorEps))))).c_str() : "none within 1e-12..1e-3",
                  ladder.c_str());
      std::vector<double> d1, d2;
      h2engine_stream::replay(*mode0, d1);
      h2engine_stream::replay(*mode0, d2);
      const bool same = sameSamples(d1, d2);
      if (!same) red++;
      std::printf("%s  DET  the engine replays '%s' bit-identically\n", same ? "PASS" : "FAIL", mode0->name.c_str());
    } else {
      red++;
      std::printf("FAIL  'T/mode 0 :: chord' is not in the stream (F5, the floor and determinism need it)\n");
    }
  }
  for (const Fam& x : fams)
    if (x.n) std::printf("FAMILY %c/  %d scenarios, %d red; worst rms %.3e, worst max %.3e, lowest bit-exact share %.1f%%\n", x.tag, x.n, x.redN, x.rms, x.max, 100 * x.minEx);
  // the bit-exact floor, on the mean share (header)
  const double meanExact = heldN > 0 ? sumExact / heldN : 0.0;
  std::sort(shares.begin(), shares.end());
  std::printf("BITEXACT  per scenario, lowest (a per-scenario share is not judged: a 1-ULP input change moves one by up to 57 points):");
  for (size_t i = 0; i < shares.size() && i < 5; i++) std::printf("\n        %6.2f%%  %s", 100 * shares[i].first, shares[i].second.c_str());
  std::printf("\n");
  if (fullStream && !infra) {
    if (kFloor < 0) {
      std::printf("SKIP  BITEXACT  platform %s has no pinned floor: mean share %.2f%% reported, NOT judged (never a pass)\n", kPlatform, 100 * meanExact);
    } else {
      const bool held = meanExact >= kFloor;
      if (!held) red++;
      std::printf("%s  BITEXACT  platform %s: the mean bit-exact share %.2f%% %s the pinned floor %.0f%%\n", held ? "PASS" : "FAIL", kPlatform,
                  100 * meanExact, held ? "holds" : "is UNDER", 100 * kFloor);
    }
  }
  std::printf("events counted (C++ side): edge/base BLEPs %llu, carrier BLEPs %llu, blade-1 entries %llu, blade-2 entries %llu\n",
              static_cast<unsigned long long>(evTotal[1]), static_cast<unsigned long long>(evTotal[2]),
              static_cast<unsigned long long>(evTotal[3]), static_cast<unsigned long long>(evTotal[4]));
  std::printf("worst rms %.3e (%s); worst max-abs %.3e (%s); bit-exact samples: mean %.2f%%, lowest %.2f%% (%s)\n", worstRms, worstRmsName.c_str(),
              worstMax, worstMaxName.c_str(), 100 * meanExact, 100 * minExact, minExactName.c_str());
  std::printf("h2_engine_parity_check: %s — %d/%d scenarios at parity (rms < %.0e, max < %.0e, events identical), %d excluded as chaotic with evidence, "
              "%d miss parity, %d red\n", red ? "RED" : "GREEN", pass, total, kRmsTol, kMaxTol, excluded, miss, red);
  return red ? 1 : 0;
#endif
}
