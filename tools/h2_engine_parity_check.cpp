/*
 * h2_engine_parity_check — L0 parity of horde 2's composed engine
 * (h2/engine/engine.h) against its golden, the composed JS
 * (docs/design/scalpel-horde-engine.js over reference/scalpel/prototype/razor-core.js
 * and reference/swarmsaw.html's SwarmSynth). ROADMAP B385; design and plan:
 * docs/port/h2-engine.md; ADR-187 items 3 and 6.
 *
 * UNWIRED: the Cross-mod ring (watch) rows are chaotic (ADR-065) and miss RMS by chance under
 * phase 1a's exclusion rule; the human rules on the ring criterion, then this is wired (B385).
 *
 * WHAT IS CHECKED. tools/h2_engine_render.mjs renders every scenario through
 * the golden in Node, seeded with mulberry32, and streams the script, the
 * PRISTINE golden's float64 samples, the blade-event digest counted by an
 * instrumented scratch copy (whose samples must equal the pristine ones bit for
 * bit: NONINV) and the golden's load readouts. This binary replays each script
 * through the C++ engine with the same seed and demands, per scenario, ALL of:
 *   - RMS(C++ - JS) < 1e-6 over the whole stereo render;
 *   - max |C++ - JS| < 1e-6 on every sample (ruled 2026-09-30, B332);
 *   - identical blade events: every PolyBLEP correction and blade-window entry,
 *     keyed by oversampled tick, member and kind, through two order-sensitive
 *     hashes;
 *   - identical load readouts (tails culled, notes refused, voices stolen);
 *   - no key the engine does not have, unless kKeyWhitelist names it;
 * and, over the whole stream:
 *   - the BIT-EXACT FLOOR (ruled 2026-09-30, B332: its value set from the measured
 *     baseline with a stated margin): the MEAN share of samples bit-identical to
 *     the golden's, over the scenarios held to parity, is at least the pinned
 *     floor. PROPOSED form, for ratification: a per-scenario floor was measured and
 *     rejected, because one scenario's share moves by up to 57 points when an input
 *     moves one ULP (L0071's probe; the share is the time before the first libm
 *     disagreement reaches the output), while the mean moves by 1.3. The share
 *     depends on both sides' libm (L0066), so the pin is keyed on platform,
 *     compiler and the golden's Node major; anything unpinned reports and SKIPS,
 *     never passes.
 * Families: P/ the 83 bench presets x phrases; E/ the 12 B366 lab presets x
 * phrases; T/ phase 1a's blade rows; C/ the composed rows. The list is the render
 * script's.
 *
 * THE TARGET IS PINNED BY CONTENT. Each golden file's git blob arrives in an
 * ORACLE record and must appear in h2/README.md as `<path>@<blob>`.
 *
 * EXCLUSIONS (ADR-065's evidence rule). The excluded rows are pinned BY NAME
 * (kChaotic). Each must hold identical events and readouts, and a strict 1e-6
 * max-abs bound over its first 128 frames (before the ring amplifies anything).
 * Then one of two rules:
 *   1a  (the default; phase 1a's, as briefed) max-abs alone is exempt: RMS < 1e-6,
 *       and the golden ALONE, inputs one ULP apart, misses max-abs by at least as
 *       much as the C++ does;
 *   b   (--chaotic-rule b; PROPOSED, a weakening of 1a that only the human can
 *       rule) RMS is exempt too: the golden's own one-ULP divergence must be at
 *       least K (--chaotic-k, default 10) times the C++ error, in RMS AND in max.
 * The run prints each row's ratios under both. Control X1 must turn every listed
 * row "NOT justified" under the selected rule.
 *
 * MUST-FAIL CONTROLS (LIBRARY L0032). Compiled with H2_ENGINE_FAULTS, so real
 * faults can be planted in the engine itself; each must turn its row red:
 *   A2a the oracle's Math.round on a Quantized Cut spread   the Cut spread at -2.5
 *   A2b Rotate spread's sign dropped                        a negative Rotate spread
 *   F2  the start-phase and modulator draws swapped         a free-running FM blade
 *   F3  the blade-entry BLEP skipped                        samples AND events
 *   F5  every event one tick late                           events ALONE
 *   V1  the blade oracle's voice law                        the repeated note (B310)
 *   T1  the swarm tick a sample late                        a coupled chord
 *   L1  B325's look-ahead dropped                           a lock-2 chord on a mono voice
 *   K1  M1 reverted (0.08 per tick)                         K 1 at 48 kHz
 *   C1  the cull a no-op                                    both cull rows
 *   C2  the cull takes the oldest voice, held or not        the mid-phrase cull
 *   X1  the ring's xm scaled by 1 + 1e-9                    every listed chaotic row
 * DETECTION FLOOR (printed, not judged; 1e-3 must be red): the swarm's pitch
 * scaled by (1 + eps).
 * DETERMINISM: three fixed scripts (the swarm with the ensemble and gravity; the
 * mid-phrase cull; D1-D3 with feedback at os 4) rendered at host blocks of 1, 37,
 * 128 and 512, twice in this process and once in a CHILD process (this binary
 * with --det-digest): every render of a script must have one digest.
 * FMA CONTROL: h2_engine_fma_control is this source at -ffp-contract=fast, over
 * the full stream; it fires only if parity misses AND (where the floor is pinned)
 * the mean bit-exact share falls under the floor. Exit 0 fired, 1 not, 2 broke.
 *
 * Usage: h2_engine_parity_check [--only REGEX] [--chaotic-rule 1a|b] [--chaotic-k K] [stream-file | -]
 *   With NO stream argument (./verify, tools/sanitize_oracles.sh) it spawns
 *   `node tools/h2_engine_render.mjs` itself, from the repo root. The controls,
 *   the floor, the pins and determinism run only on the full stream.
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
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
using h2engine_stream::Cmd;
using h2engine_stream::ReplayInfo;
using h2engine_stream::Scenario;

namespace {

constexpr double kRmsTol = 1e-6;
constexpr double kMaxTol = 1e-6;
constexpr double kFirstTol = 1e-6;   // the excluded rows' first 128 frames
constexpr size_t kFirstFrames = 128;
// The exclusions, by name: the list cannot change without an edit a reviewer sees.
const char* const kChaotic[] = {
  "P/Starting points / Cross-mod ring (watch) :: chord",
  "P/Starting points / Cross-mod ring (watch) :: repeat",
  "P/Starting points / Cross-mod ring (watch) :: arp",
};
// Keys the golden has and the engine deliberately does not; none today.
const char* const kKeyWhitelist[] = {""};

// The bit-exact floor, keyed on what the share depends on: the platform (its libm),
// the compiler, and the golden's Node major (V8's libm). darwin-arm64, Apple clang
// 16, Node 24 (2026-10-01, the 540 scenarios held to parity): mean 37.00% at the
// scripts' own inputs, 38.34% and 37.61% with every note-on frequency nudged 1 and
// 2 ULP; the floor sits 7 points under the lowest. The FMA-fused build reads 17.48%.
#if defined(__APPLE__) && defined(__aarch64__)
constexpr const char* kPlatform = "darwin-arm64";
#elif defined(__linux__) && defined(__x86_64__)
constexpr const char* kPlatform = "linux-x86_64";
#elif defined(_WIN32)
constexpr const char* kPlatform = "windows";
#else
constexpr const char* kPlatform = "other";
#endif
std::string compilerId() {
  char b[48];
#if defined(__apple_build_version__)
  std::snprintf(b, sizeof b, "appleclang-%d", __clang_major__);
#elif defined(__clang__)
  std::snprintf(b, sizeof b, "clang-%d", __clang_major__);
#elif defined(__GNUC__)
  std::snprintf(b, sizeof b, "gcc-%d", __GNUC__);
#elif defined(_MSC_VER)
  std::snprintf(b, sizeof b, "msvc-%d", _MSC_VER);
#else
  std::snprintf(b, sizeof b, "unknown");
#endif
  return b;
}
struct FloorPin { const char* platform; const char* compiler; int nodeMajor; double floor; };
const FloorPin kFloorPins[] = {
  {"darwin-arm64", "appleclang-16", 24, 0.30},
};
double pinnedFloor(int nodeMajor) {
  const std::string cc = compilerId();
  for (const FloorPin& p : kFloorPins)
    if (std::strcmp(p.platform, kPlatform) == 0 && cc == p.compiler && nodeMajor == p.nodeMajor) return p.floor;
  return -1;
}

struct Result { double rms = 0, max = 0, exact = 0, firstMax = 0; bool evOk = true, cntOk = true; };

Result compare(const Scenario& sc, const std::vector<double>& cpp, const EventLog& log, const ReplayInfo& info) {
  Result r;
  if (cpp.size() != sc.js.size()) { r.rms = r.max = r.firstMax = INFINITY; r.evOk = false; return r; }
  double e = 0;
  size_t same = 0;
  for (size_t i = 0; i < cpp.size(); i++) {
    const double d = std::fabs(cpp[i] - sc.js[i]);
    if (std::isnan(d)) { r.max = INFINITY; e = INFINITY; if (i < 2 * kFirstFrames) r.firstMax = INFINITY; continue; }
    e += d * d;
    if (d > r.max) r.max = d;
    if (i < 2 * kFirstFrames && d > r.firstMax) r.firstMax = d;
    if (std::memcmp(&cpp[i], &sc.js[i], sizeof(double)) == 0) same++;
  }
  r.rms = std::sqrt(e / static_cast<double>(cpp.size()));
  r.exact = cpp.empty() ? 1 : static_cast<double>(same) / static_cast<double>(cpp.size());
  for (int k = 1; k <= 4; k++) if (log.count[k] != sc.ev[k]) r.evOk = false;
  if (log.h1 != sc.h1 || log.h2 != sc.h2) r.evOk = false;
  if (sc.hasCnt) for (int k = 0; k < 3; k++) if (info.cnt[k] != sc.cnt[k]) r.cntOk = false;
  return r;
}
bool parityOk(const Result& r) { return r.rms < kRmsTol && r.max < kMaxTol && r.evOk && r.cntOk; }
bool isChaotic(const std::string& name) {
  for (const char* c : kChaotic) if (name == c) return true;
  return false;
}
// the exclusion rules (header)
struct Rule { bool b = false; double K = 10; };
bool justified(const Rule& rule, const Scenario& sc, const Result& r) {
  if (!r.evOk || !r.cntOk || !sc.hasSelf || !(r.firstMax < kFirstTol)) return false;
  if (rule.b) return sc.selfRms >= rule.K * r.rms && sc.selfMax >= rule.K * r.max;
  return r.rms < kRmsTol && sc.selfMax >= kMaxTol && sc.selfMax >= r.max;
}

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
bool whitelisted(const std::string& key) {
  for (const char* k : kKeyWhitelist) if (*k && key == k) return true;
  return false;
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
  {11, "C1 the cull a no-op", "C/CAP cull, the cap lowered mid-phrase"},
  {11, "C1 the cull a no-op", "C/CAP cull per-partial voices, the cap lowered mid-phrase"},
  {12, "C2 the cull takes the oldest voice, held or not", "C/CAP cull, the cap lowered mid-phrase"},
};

// ---- determinism: fixed scripts, several host block sizes, two processes ----
Cmd set(const char* k, double v) { Cmd c; c.op = "set"; c.key = k; c.a = v; return c; }
Cmd op3(const char* o, double a = 0, double b = 0, double c = 0) { Cmd x; x.op = o; x.a = a; x.b = b; x.c = c; return x; }
double mtof(int n) { return 440 * std::pow(2.0, (n - 69) / 12.0); }
std::vector<Scenario> detScripts() {
  std::vector<Scenario> out;
  auto start = [&](const char* name) { Scenario s; s.name = name; s.sr = 48000; s.seed = 0xD375; out.push_back(s); return &out.back(); };
  {
    Scenario* s = start("swarm, ensemble and gravity");
    for (const Cmd& c : {set("N", 5), set("K", 0.5), set("detune", 25), set("onsetScatter", 8), set("voiceEnv", 1),
                         set("attackScatter", 0.5), set("grav", 0.8), set("mode", 2), set("I", 2)}) s->cmds.push_back(c);
    s->cmds.push_back(op3("snap"));
    s->cmds.push_back(op3("on", 57, mtof(57), 0.8)); s->cmds.push_back(op3("on", 64, mtof(64) * 1.004, 0.8));
    s->cmds.push_back(op3("render", 128, 60)); s->cmds.push_back(op3("off", 57)); s->cmds.push_back(op3("off", 64));
    s->cmds.push_back(op3("render", 128, 30));
  }
  {
    Scenario* s = start("the mid-phrase cull");
    s->cmds.push_back(set("R", 1500)); s->cmds.push_back(set("voiceEnv", 1)); s->cmds.push_back(op3("snap"));
    for (int n : {45, 52, 57, 60}) s->cmds.push_back(op3("on", n, mtof(n), 0.85));
    s->cmds.push_back(op3("render", 128, 10));
    for (int n : {52, 57, 60}) s->cmds.push_back(op3("off", n));
    s->cmds.push_back(op3("render", 128, 3)); s->cmds.push_back(op3("cap", 1)); s->cmds.push_back(op3("render", 128, 30));
  }
  {
    Scenario* s = start("D1-D3 with feedback at os 4");
    for (const Cmd& c : {set("mode", 2), set("I", 2), set("fb", 0.4), set("xm", 0.3), set("aaCarrier", 1), set("aaXin", 1),
                         set("aaLoop", 1), set("os", 4), set("N", 4)}) s->cmds.push_back(c);
    s->cmds.push_back(op3("snap"));
    s->cmds.push_back(op3("on", 45, mtof(45), 0.9)); s->cmds.push_back(op3("on", 52, mtof(52), 0.9));
    s->cmds.push_back(op3("render", 128, 50)); s->cmds.push_back(op3("off", 45)); s->cmds.push_back(op3("off", 52));
    s->cmds.push_back(op3("render", 128, 20));
  }
  return out;
}
constexpr int kDetBlocks[] = {1, 37, 128, 512};
uint64_t fnv(const std::vector<double>& v) {
  uint64_t h = 1469598103934665603ull;
  const unsigned char* p = reinterpret_cast<const unsigned char*>(v.data());
  for (size_t i = 0; i < v.size() * sizeof(double); i++) { h ^= p[i]; h *= 1099511628211ull; }
  return h;
}
// While a culled voice fades, the golden renders ONE SAMPLE PER CALL, through a
// one-sample Float32Array, to the end of the host's block (scalpel-horde-engine.js
// renderPlain), so how many samples pass through float32 depends on where the
// host's blocks end. That script may differ across block sizes by float32
// rounding and nothing more: 2^-23 at the output's |x| <= 1.
constexpr double kFloatTailTol = 1.1920928955078125e-07;
// "<script index> <block> <digest>" per render; `outs` keeps the renders
std::map<std::string, uint64_t> detDigests(std::map<std::string, std::vector<double>>* outs = nullptr) {
  std::map<std::string, uint64_t> d;
  const std::vector<Scenario> sc = detScripts();
  std::vector<double> out;
  for (size_t i = 0; i < sc.size(); i++)
    for (int b : kDetBlocks) {
      h2engine_stream::replay(sc[i], out, 0, nullptr, 0, nullptr, b);
      const std::string k = std::to_string(i) + " " + std::to_string(b);
      d[k] = fnv(out);
      if (outs) (*outs)[k] = out;
    }
  return d;
}

}  // namespace

int main(int argc, char** argv) {
  std::string only, path;
  Rule rule;
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--det-digest") == 0) {   // the child half of the determinism rows
      for (const auto& kv : detDigests()) std::printf("%s %llu\n", kv.first.c_str(), static_cast<unsigned long long>(kv.second));
      return 0;
    }
    if (std::strcmp(argv[i], "--only") == 0 && i + 1 < argc) only = argv[++i];
    else if (std::strcmp(argv[i], "--chaotic-rule") == 0 && i + 1 < argc) rule.b = std::strcmp(argv[++i], "b") == 0;
    else if (std::strcmp(argv[i], "--chaotic-k") == 0 && i + 1 < argc) rule.K = std::strtod(argv[++i], nullptr);
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
  std::printf("RULE  the chaotic exclusions are judged under rule %s%s\n", rule.b ? "b (PROPOSED: RMS exempt too)" : "1a (max-abs exempt only)",
              rule.b ? (", K " + std::to_string(rule.K)).c_str() : "");

  int red = 0, pass = 0, miss = 0, excluded = 0, total = 0, niBad = 0, nodeMajor = -1;
  double worstRms = 0, worstMax = 0, sumExact = 0, minExact = 2;
  int heldN = 0;   // the scenarios held to parity (every one not listed chaotic): the floor's population
  std::string worstRmsName, worstMaxName, minExactName, nodeVersion;
  std::vector<std::string> oracles, listedNames;
  uint64_t evTotal[5] = {0, 0, 0, 0, 0};
  struct Fam { char tag; int n = 0, redN = 0; double rms = 0, max = 0, minEx = 2, sumEx = 0; };
  Fam fams[4] = {{'P'}, {'E'}, {'T'}, {'C'}};
  std::vector<Scenario> keep;
  std::vector<std::pair<double, std::string>> shares;
  std::vector<double> cpp;
  bool ended = false, infra = false;
  while (h2engine_stream::readLine(f, line)) {
    size_t p = 0;
    const std::string op = h2engine_stream::word(line, p);
    if (op == "NODE") { nodeVersion = h2engine_stream::word(line, p); nodeMajor = std::atoi(nodeVersion.c_str()); continue; }
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
    ReplayInfo info;
    h2engine_stream::replay(sc, cpp, 0, &log, 0, &info);
    const Result r = compare(sc, cpp, log, info);
    total++;
    if (sc.ni != 1) niBad++;
    if (!sc.hasCnt) { std::printf("FAIL  '%s' carries no CNT record\n", sc.name.c_str()); infra = true; }
    for (int k = 1; k <= 4; k++) evTotal[k] += log.count[k];
    Fam* fam = nullptr;
    for (Fam& x : fams) if (!sc.name.empty() && sc.name[0] == x.tag) fam = &x;
    if (info.unknownKeys && !whitelisted(info.firstUnknown)) {
      red++;
      std::printf("FAIL  '%s' sets %d key(s) the engine does not have (first: %s), and none is whitelisted\n", sc.name.c_str(),
                  info.unknownKeys, info.firstUnknown.c_str());
    }
    const bool ok = parityOk(r);
    if (!sc.excl.empty()) {
      listedNames.push_back(sc.name);
      const bool just = justified(rule, sc, r);
      if (just) excluded++; else { red++; if (fam) fam->redN++; }
      std::printf("%s  rms %.3e  max %.3e  first 128 frames max %.3e  ev %s  %s\n"
                  "        chaotic? the golden alone, inputs 1 ULP apart: rms %.3e max %.3e (ratio to the C++: rms %.0fx, max %.0fx)"
                  " -> exclusion %s under rule %s (%s)\n",
                  just ? "EXCL" : "FAIL", r.rms, r.max, r.firstMax, r.evOk && r.cntOk ? "ok " : "BAD", sc.name.c_str(), sc.selfRms,
                  sc.selfMax, sc.selfRms / r.rms, sc.selfMax / r.max, just ? "justified" : "NOT justified", rule.b ? "b" : "1a",
                  sc.excl.c_str());
    } else {
      if (ok) pass++; else { red++; miss++; if (fam) fam->redN++; }
      if (r.rms > worstRms) { worstRms = r.rms; worstRmsName = sc.name; }
      if (r.max > worstMax) { worstMax = r.max; worstMaxName = sc.name; }
      if (r.exact < minExact) { minExact = r.exact; minExactName = sc.name; }
      sumExact += r.exact; heldN++;
      shares.emplace_back(r.exact, sc.name);
      if (fam) { fam->n++; fam->rms = std::max(fam->rms, r.rms); fam->max = std::max(fam->max, r.max); fam->minEx = std::min(fam->minEx, r.exact); fam->sumEx += r.exact; }
      std::printf("%s  rms %.3e  max %.3e  exact %6.2f%%  ev %s %llu/%llu/%llu/%llu  %s\n", ok ? "PASS" : "FAIL", r.rms, r.max,
                  100 * r.exact, r.evOk && r.cntOk ? "ok " : "BAD", static_cast<unsigned long long>(log.count[1]),
                  static_cast<unsigned long long>(log.count[2]), static_cast<unsigned long long>(log.count[3]),
                  static_cast<unsigned long long>(log.count[4]), sc.name.c_str());
      if (!r.evOk)
        std::printf("        events: golden %llu/%llu/%llu/%llu hashes %u %u, C++ hashes %u %u\n", static_cast<unsigned long long>(sc.ev[1]),
                    static_cast<unsigned long long>(sc.ev[2]), static_cast<unsigned long long>(sc.ev[3]),
                    static_cast<unsigned long long>(sc.ev[4]), sc.h1, sc.h2, log.h1, log.h2);
      if (!r.cntOk)
        std::printf("        readouts (culled/refused/stolen): golden %.0f/%.0f/%.0f, C++ %.0f/%.0f/%.0f\n", sc.cnt[0], sc.cnt[1], sc.cnt[2],
                    info.cnt[0], info.cnt[1], info.cnt[2]);
    }
    bool want = !sc.excl.empty() || sc.name == "T/mode 0 :: chord";
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
  if (nodeMajor < 0) { std::printf("FAIL  no NODE record in the stream\n"); infra = true; }
  std::printf("%s  NONINV  the instrumented scratch golden rendered the pristine golden's samples bit for bit on %d of %d scenarios\n",
              niBad == 0 && total > 0 ? "PASS" : "FAIL", total - niBad, total);
  if (niBad) infra = true;
  if (infra) red++;

  const double meanExact = heldN > 0 ? sumExact / heldN : 0.0;
  const double floor = pinnedFloor(nodeMajor);
  std::printf("BITEXACT  mean share per family:");
  for (const Fam& x : fams) if (x.n) std::printf("  %c/ %.2f%%", x.tag, 100 * x.sumEx / x.n);
  std::printf("  (all held: %.2f%%; key %s / %s / node %d)\n", 100 * meanExact, kPlatform, compilerId().c_str(), nodeMajor);

#ifdef H2_ENGINE_FMA_CONTROL
  if (infra) {
    std::printf("h2_engine_fma_control: INFRASTRUCTURE FAILURE — the run broke (renderer, stream or instrumentation); "
                "this is NOT the control firing\n");
    return 2;
  }
  // both detectors must see the fused build: parity, and (where pinned) the floor
  const bool floorCaught = floor < 0 || meanExact < floor;
  const bool caught = miss > 0 && floorCaught;
  std::printf("h2_engine_fma_control: %s — the -ffp-contract=fast build of the same engine: %d of %d scenarios miss parity "
              "(worst rms %.3e, worst max %.3e); mean bit-exact %.2f%% %s\n",
              caught ? "FIRED" : "DID NOT FIRE", miss, total, worstRms, worstMax, 100 * meanExact,
              floor < 0 ? "(the floor is not pinned for this key)" : (floorCaught ? "is under the pinned floor" : "HOLDS the pinned floor: the floor did not see it"));
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
    // the exclusions, by name
    bool namesOk = listedNames.size() == sizeof kChaotic / sizeof kChaotic[0];
    for (const std::string& n : listedNames) if (!isChaotic(n)) namesOk = false;
    if (!namesOk) red++;
    std::printf("%s  PIN  the stream lists %zu chaotic scenarios; the pinned list is %zu names%s\n", namesOk ? "PASS" : "FAIL",
                listedNames.size(), sizeof kChaotic / sizeof kChaotic[0], namesOk ? ", and they match by name" : ": THEY DIFFER (re-pin deliberately)");
    for (const Control& c : kControls) {
      const Scenario* sc = nullptr;
      for (const Scenario& s : keep) if (s.name == c.scenario) sc = &s;
      if (!sc) { std::printf("FAIL  control %s: its scenario '%s' is not in the stream\n", c.name, c.scenario); red++; continue; }
      EventLog clog, flog;
      ReplayInfo ci, fi;
      std::vector<double> a, b;
      h2engine_stream::replay(*sc, a, 0, &clog, 0, &ci);
      h2engine_stream::replay(*sc, b, c.fault, &flog, 0, &fi);
      const Result clean = compare(*sc, a, clog, ci), r = compare(*sc, b, flog, fi);
      bool fired = parityOk(clean) && !parityOk(r);
      if (c.fault == 3) fired = fired && !r.evOk && (r.rms >= kRmsTol || r.max >= kMaxTol);   // both criteria must see it
      if (!fired) red++;
      std::printf("%s  control %s turns '%s' red: rms %.3e max %.3e, events %s, readouts %s (clean: rms %.3e, events %s)\n", fired ? "PASS" : "FAIL",
                  c.name, sc->name.c_str(), r.rms, r.max, r.evOk ? "agree" : "DISAGREE", r.cntOk ? "agree" : "DISAGREE", clean.rms,
                  clean.evOk ? "agree" : "DISAGREE");
    }
    // X1: the exclusion path's own must-fail control
    {
      int caught = 0, n = 0;
      std::string detail;
      for (const Scenario& s : keep) {
        if (s.excl.empty()) continue;
        n++;
        EventLog flog;
        ReplayInfo fi;
        std::vector<double> b;
        h2engine_stream::replay(s, b, 13, &flog, 0, &fi);
        const Result r = compare(s, b, flog, fi);
        const bool just = justified(rule, s, r);
        if (!just) caught++;
        char buf[200];
        std::snprintf(buf, sizeof buf, "\n        %s: rms %.3e max %.3e first-128 %.3e, ratios %.2fx / %.2fx -> %s", s.name.c_str(), r.rms, r.max,
                      r.firstMax, s.selfRms / r.rms, s.selfMax / r.max, just ? "STILL JUSTIFIED" : "not justified");
        detail += buf;
      }
      const bool fired = n > 0 && caught == n;
      if (!fired) red++;
      std::printf("%s  control X1 the ring's xm scaled by 1 + 1e-9 turns every listed chaotic row NOT justified (rule %s): %d of %d%s\n",
                  fired ? "PASS" : "FAIL", rule.b ? "b" : "1a", caught, n, detail.c_str());
    }
    const Scenario* mode0 = nullptr;
    for (const Scenario& s : keep) if (s.name == "T/mode 0 :: chord") mode0 = &s;
    if (mode0) {
      // F5: an EVENT-ONLY fault; the samples must stay bit-identical to the clean replay
      EventLog clog, flog;
      ReplayInfo ci, fi;
      std::vector<double> a, b;
      h2engine_stream::replay(*mode0, a, 0, &clog, 0, &ci);
      h2engine_stream::replay(*mode0, b, 5, &flog, 0, &fi);
      const Result r = compare(*mode0, b, flog, fi);
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
        ReplayInfo ei;
        std::vector<double> c;
        h2engine_stream::replay(*mode0, c, 0, &elog, eps, &ei);
        const Result r2 = compare(*mode0, c, elog, ei);
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
    } else {
      red++;
      std::printf("FAIL  'T/mode 0 :: chord' is not in the stream (F5 and the floor need it)\n");
    }
    // DETERMINISM: this process twice, then a child process, at four host block sizes
    {
      std::map<std::string, std::vector<double>> outs;
      const std::map<std::string, uint64_t> a = detDigests(&outs), b = detDigests();
      std::map<std::string, uint64_t> child;
      const std::string cmd = std::string("\"") + argv[0] + "\" --det-digest";
#ifdef _WIN32
      FILE* cf = popen(cmd.c_str(), "rb");
#else
      FILE* cf = popen(cmd.c_str(), "r");
#endif
      if (cf) {
        std::string l;
        while (h2engine_stream::readLine(cf, l)) {
          size_t q = 0;
          const std::string i = h2engine_stream::word(l, q), bs = h2engine_stream::word(l, q);
          child[i + " " + bs] = std::strtoull(h2engine_stream::word(l, q).c_str(), nullptr, 10);
        }
        if (pclose(cf) != 0) child.clear();
      }
      const std::vector<Scenario> sc = detScripts();
      for (size_t i = 0; i < sc.size(); i++) {
        bool same = !child.empty(), blockFree = true;
        const std::string refKey = std::to_string(i) + " 128";
        const uint64_t ref = a.at(refKey);
        double blockDelta = 0;   // the largest difference from the 128-sample render
        std::string row;
        for (int bs : kDetBlocks) {
          const std::string k = std::to_string(i) + " " + std::to_string(bs);
          if (a.at(k) != b.at(k) || !child.count(k) || child.at(k) != a.at(k)) same = false;
          if (a.at(k) != ref) blockFree = false;
          const std::vector<double>& o = outs.at(k);
          const std::vector<double>& r0 = outs.at(refKey);
          for (size_t j = 0; j < o.size() && j < r0.size(); j++) blockDelta = std::max(blockDelta, std::fabs(o[j] - r0[j]));
          char buf[48];
          std::snprintf(buf, sizeof buf, " %d:%016llx", bs, static_cast<unsigned long long>(a.at(k)));
          row += buf;
        }
        const bool floatTail = i == 1;   // the mid-phrase cull (kFloatTailTol)
        const bool ok = same && (blockFree || (floatTail && blockDelta <= kFloatTailTol));
        if (!ok) red++;
        char tail[160] = "";
        if (!blockFree)
          std::snprintf(tail, sizeof tail, ", and across host blocks of 1, 37, 128 and 512 within float32 rounding (max |diff| %.3e, bound %.3e)",
                        blockDelta, kFloatTailTol);
        std::printf("%s  DET  '%s': %s across two runs in this process and a child process%s;%s\n", ok ? "PASS" : "FAIL",
                    sc[i].name.c_str(), same ? "bit-identical" : "NOT identical",
                    blockFree ? ", and bit-identical at host blocks of 1, 37, 128 and 512" : tail, row.c_str());
      }
    }
  }
  for (const Fam& x : fams)
    if (x.n) std::printf("FAMILY %c/  %d scenarios, %d red; worst rms %.3e, worst max %.3e, lowest bit-exact share %.1f%%, mean %.2f%%\n", x.tag, x.n,
                         x.redN, x.rms, x.max, 100 * x.minEx, 100 * x.sumEx / x.n);
  std::sort(shares.begin(), shares.end());
  std::printf("BITEXACT  per scenario, lowest (a per-scenario share is not judged: a 1-ULP input change moves one by up to 57 points):");
  for (size_t i = 0; i < shares.size() && i < 5; i++) std::printf("\n        %6.2f%%  %s", 100 * shares[i].first, shares[i].second.c_str());
  std::printf("\n");
  if (fullStream && !infra) {
    if (floor < 0) {
      std::printf("SKIP  BITEXACT  no floor is pinned for %s / %s / node %d: mean share %.2f%% reported, NOT judged (never a pass)\n", kPlatform,
                  compilerId().c_str(), nodeMajor, 100 * meanExact);
    } else {
      const bool held = meanExact >= floor;
      if (!held) red++;
      std::printf("%s  BITEXACT  %s / %s / node %d: the mean bit-exact share %.2f%% %s the pinned floor %.0f%%\n", held ? "PASS" : "FAIL", kPlatform,
                  compilerId().c_str(), nodeMajor, 100 * meanExact, held ? "holds" : "is UNDER", 100 * floor);
    }
  }
  std::printf("events counted (C++ side): edge/base BLEPs %llu, carrier BLEPs %llu, blade-1 entries %llu, blade-2 entries %llu\n",
              static_cast<unsigned long long>(evTotal[1]), static_cast<unsigned long long>(evTotal[2]),
              static_cast<unsigned long long>(evTotal[3]), static_cast<unsigned long long>(evTotal[4]));
  std::printf("worst rms %.3e (%s); worst max-abs %.3e (%s); bit-exact samples: mean %.2f%%, lowest %.2f%% (%s)\n", worstRms, worstRmsName.c_str(),
              worstMax, worstMaxName.c_str(), 100 * meanExact, 100 * minExact, minExactName.c_str());
  std::printf("h2_engine_parity_check: %s — %d/%d scenarios at parity (rms < %.0e, max < %.0e, events and readouts identical), %d excluded as chaotic "
              "with evidence under rule %s, %d miss parity, %d red\n", red ? "RED" : "GREEN", pass, total, kRmsTol, kMaxTol, excluded,
              rule.b ? "b" : "1a", miss, red);
  return red ? 1 : 0;
#endif
}
