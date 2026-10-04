/*
 * h2_engine_selfdigest_check — the composed engine (h2/engine/engine.h) against
 * ITSELF: a C++-vs-C++ bit-identity digest of all the parity scenarios. ROADMAP
 * B441 phase 2 (the step before C1-C3); ADR-187 item 5.
 *
 * WIRED: ./verify full (after h2_engine_parity_check, on the stream it rendered).
 *
 * WHY. h2_engine_parity_check proves the C++ is within 1e-6 of the JS golden (rms
 * and max-abs) with a 30 % mean bit-exact floor. That is weaker than "the same bits
 * as today's C++": an optimisation that moves a sample by 1e-12 passes parity. B441's
 * output-neutral experiments (ADR-187 item 5: "output-neutral work ... must leave
 * every digest unchanged; it is NOT a divergence") need the stronger statement, and
 * this is it: every scenario's samples, events and readouts are bit-identical to the
 * pinned C++ render.
 *
 * WHAT IS DIGESTED, per scenario: FNV-1a 64 over the raw bytes of every output
 * double (interleaved L/R, as the replay emits them), then the blade-event log (its
 * four counts as uint64, then its two order-sensitive 32-bit hashes, which fold
 * every event's tick, member and kind in order: js.h EventLog), then the three load
 * readouts (culled, refused, stolen) as raw doubles. The scripts are the parity
 * stream's (tools/h2_engine_render.mjs), replayed through h2_engine_stream.h exactly
 * as the parity check replays them, at the script's own host blocks; the build is the
 * parity build (-O2 -ffp-contract=off, H2_ENGINE_FAULTS compiled in, fault 0). The JS
 * samples in the stream are not read. A digest is stable across process runs and
 * host blocks because the engine is (the parity check's DET rows); the one script
 * that is block-dependent by design (a culled voice's float32 tail) does not matter
 * here, since the blocks are the script's. The three chaotic ring rows are digested
 * like every other row: C++ against C++ is deterministic.
 * One line per scenario, `<16 hex>  <name>`, then `TOTAL <16 hex> <n>` (FNV-1a 64
 * over the lines, in stream order).
 *
 * THE REFERENCE is h2/engine/selfdigest.<platform>.<compiler>.node<major>.txt, keyed
 * like the parity check's bit-exact floor: the platform and compiler (libm, codegen)
 * and the golden's Node major (V8's libm computes the scripts' note frequencies).
 * On an unkeyed platform the digests are printed and NOT judged (a SKIP, never a
 * pass). A reference for this platform under another compiler or Node major gets a
 * LOUD stderr WARNING to re-pin, and stays a SKIP (the floor's rule, L0072).
 *
 * RE-PIN (a human-visible act):
 *   node tools/h2_engine_render.mjs > /tmp/h2s && \
 *     build-release/h2_engine_selfdigest_check --full-from /tmp/h2s --repin
 * writes this platform's reference from a full, clean stream, and only when both
 * controls fired. Re-pinning is allowed ONLY for an output-changing divergence
 * recorded in the ledger (docs/port/divergences.json; ADR-187 items 5 and 7). An
 * "output-neutral" PR NEVER re-pins: a red here is the experiment failing.
 * An output-neutral PR also re-runs tools/h2_libm_count.py and commits its counts,
 * so a libm call that changed under the same bits is visible in its diff.
 *
 * MUST-FAIL CONTROLS (LIBRARY L0032), every run, judged by the same verdict code
 * as the reference comparison (against the reference where keyed, else against this
 * run's own clean digests):
 *   ULP    one sample of kUlpScenario nudged by one ULP: exactly that scenario red,
 *          by name;
 *   FAULT  fault 9 (K1, M1 reverted: the swarm's K smoother back to the constant
 *          0.08 per tick) planted in the engine, every scenario re-rendered: at
 *          least kFaultMinRed scenarios red.
 *
 * Usage: h2_engine_selfdigest_check --full-from FILE|- [--repin]
 *   FILE is tools/h2_engine_render.mjs's FULL output (./verify renders it once for
 *   the parity check, its FMA control and this check). A cut file (no END record, or
 *   a count that disagrees with the header) is an infrastructure failure, exit 2.
 * Exit: 0 green (or an unkeyed SKIP), 1 red, 2 the run broke.
 */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define H2_ENGINE_FAULTS 1
#include "../h2/engine/engine.h"
#include "h2_engine_stream.h"

using horde2::engine::EventLog;
using h2engine_stream::ReplayInfo;
using h2engine_stream::Scenario;

namespace {

// The ULP control's row: a plain blade chord every family-T reader knows.
const char* const kUlpScenario = "T/mode 0 :: chord";
// The FAULT control: K1 (parity check's kControls) reverts M1, a constant in the
// swarm's K smoother; it reaches every scenario whose coupling is ever smoothed.
// Measured 2026-10-04 at main a2dd06e: 474 of 543 rows red. The bar sits far
// under that and far over one, so "many" is a number, not a feeling.
constexpr int kFaultId = 9;
constexpr int kFaultMinRed = 100;

#if defined(__APPLE__) && defined(__aarch64__)
constexpr const char* kPlatform = "darwin-arm64";
#elif defined(__linux__) && defined(__x86_64__)
constexpr const char* kPlatform = "linux-x86_64";
#elif defined(_WIN32)
constexpr const char* kPlatform = "windows";
#else
constexpr const char* kPlatform = "other";
#endif
// The same spelling as h2_engine_parity_check's compilerId(), so both keys agree.
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

struct Fnv {
  uint64_t h = 1469598103934665603ull;
  void bytes(const void* p, size_t n) {
    const unsigned char* c = static_cast<const unsigned char*>(p);
    for (size_t i = 0; i < n; i++) { h ^= c[i]; h *= 1099511628211ull; }
  }
};
uint64_t digest(const std::vector<double>& out, const EventLog& log, const ReplayInfo& info) {
  Fnv f;
  f.bytes(out.data(), out.size() * sizeof(double));
  f.bytes(&log.count[1], 4 * sizeof(uint64_t));
  f.bytes(&log.h1, sizeof log.h1);
  f.bytes(&log.h2, sizeof log.h2);
  f.bytes(info.cnt, sizeof info.cnt);
  return f.h;
}
uint64_t render(const Scenario& sc, int fault, std::vector<double>* keep = nullptr) {
  std::vector<double> out;
  EventLog log;
  ReplayInfo info;
  h2engine_stream::replay(sc, out, fault, &log, 0, &info);
  if (keep) *keep = out;
  return digest(out, log, info);
}
std::string hex(uint64_t v) {
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(v));
  return b;
}

using Digests = std::vector<std::pair<std::string, uint64_t>>;   // stream order
uint64_t total(const Digests& d) {
  Fnv f;
  for (const auto& [name, v] : d) { const std::string l = hex(v) + "  " + name + "\n"; f.bytes(l.data(), l.size()); }
  return f.h;
}

// THE verdict: the scenarios of `run` whose digest differs from `ref`, is absent
// from it, or (the reverse) is pinned and was not rendered. The controls go
// through this function too, so they test the code that judges.
std::vector<std::string> verdict(const Digests& run, const std::map<std::string, uint64_t>& ref) {
  std::vector<std::string> red;
  std::map<std::string, int> seen;
  for (const auto& [name, v] : run) {
    seen[name]++;
    const auto it = ref.find(name);
    if (it == ref.end()) red.push_back(name + "  (not in the reference)");
    else if (it->second != v) red.push_back(name);
  }
  for (const auto& [name, v] : ref) if (!seen.count(name)) red.push_back(name + "  (pinned, not rendered)");
  return red;
}

std::string refKey(int nodeMajor) {
  return std::string(kPlatform) + "." + compilerId() + ".node" + std::to_string(nodeMajor);
}
std::string refPath(int nodeMajor) { return "h2/engine/selfdigest." + refKey(nodeMajor) + ".txt"; }

// Reads a reference file: `<16 hex>  <name>` lines; `#` lines and TOTAL are context.
bool readRef(const std::string& path, std::map<std::string, uint64_t>& ref, std::vector<std::string>& oracles, uint64_t& tot) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  std::string line;
  while (h2engine_stream::readLine(f, line)) {
    if (line.rfind("# ORACLE ", 0) == 0) { oracles.push_back(line.substr(9)); continue; }
    if (line.empty() || line[0] == '#') continue;
    if (line.rfind("TOTAL ", 0) == 0) { tot = std::strtoull(line.c_str() + 6, nullptr, 16); continue; }
    if (line.size() < 19 || line.compare(16, 2, "  ") != 0) { std::fclose(f); return false; }
    ref[line.substr(18)] = std::strtoull(line.substr(0, 16).c_str(), nullptr, 16);
  }
  std::fclose(f);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  std::string path;
  bool repin = false;
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--full-from") == 0 && i + 1 < argc) path = argv[++i];
    else if (std::strcmp(argv[i], "--repin") == 0) repin = true;
    else { std::fprintf(stderr, "h2_engine_selfdigest_check: unknown argument '%s' (usage: --full-from FILE|- [--repin])\n", argv[i]); return 2; }
  }
  if (path.empty()) { std::fprintf(stderr, "h2_engine_selfdigest_check: --full-from FILE|- is required\n"); return 2; }
  FILE* f = nullptr;
  if (path == "-") {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    f = stdin;
  } else {
    f = std::fopen(path.c_str(), "rb");
  }
  if (!f) { std::fprintf(stderr, "h2_engine_selfdigest_check: cannot open '%s'\n", path.c_str()); return 2; }

  std::string line;
  if (!h2engine_stream::readLine(f, line) || line.rfind("H2ENGINE 1", 0) != 0) {
    std::fprintf(stderr, "h2_engine_selfdigest_check: bad stream header '%s'\n", line.c_str());
    return 2;
  }
  const long headerN = std::strtol(line.c_str() + 10, nullptr, 10);
  long endN = -1;
  int nodeMajor = -1;
  bool ended = false, infra = false;
  std::vector<std::string> oracles;
  std::vector<Scenario> scripts;   // the scripts only (the JS samples are dropped): the FAULT pass replays them
  std::vector<double> ulpOut;      // the ULP control's row, kept
  EventLog ulpLog;
  ReplayInfo ulpInfo;
  Digests run;
  while (h2engine_stream::readLine(f, line)) {
    size_t p = 0;
    const std::string op = h2engine_stream::word(line, p);
    if (op == "NODE") { nodeMajor = std::atoi(h2engine_stream::word(line, p).c_str()); continue; }
    if (op == "LIBM") {   // the parity check's libm probes: skipped here
      h2engine_stream::word(line, p);
      const size_t n = std::strtoull(h2engine_stream::word(line, p).c_str(), nullptr, 10);
      std::vector<double> v(n * 3);
      if (std::fread(v.data(), sizeof(double), n * 3, f) != n * 3) { infra = true; break; }
      continue;
    }
    if (op == "ORACLE") { oracles.push_back(h2engine_stream::rest(line, p)); continue; }
    if (op == "END") { ended = true; endN = std::strtol(h2engine_stream::word(line, p).c_str(), nullptr, 10); break; }
    if (op != "SCN") { std::fprintf(stderr, "h2_engine_selfdigest_check: unexpected stream line '%s'\n", line.c_str()); infra = true; break; }
    Scenario sc;
    if (!h2engine_stream::readScenario(f, line, sc)) { std::fprintf(stderr, "h2_engine_selfdigest_check: truncated stream in '%s'\n", sc.name.c_str()); infra = true; break; }
    sc.js.clear();
    sc.js.shrink_to_fit();
    std::vector<double> out;
    EventLog log;
    ReplayInfo info;
    h2engine_stream::replay(sc, out, 0, &log, 0, &info);
    run.emplace_back(sc.name, digest(out, log, info));
    if (sc.name == kUlpScenario) { ulpOut = out; ulpLog = log; ulpInfo = info; }
    std::printf("%s  %s\n", hex(run.back().second).c_str(), sc.name.c_str());
    scripts.push_back(std::move(sc));
  }
  if (f != stdin) std::fclose(f);
  const int n = static_cast<int>(run.size());
  if (!ended || endN != headerN || n != headerN) {
    std::printf("FAIL  STREAM  not a full stream: header %ld, END %s, %d scenarios read\n", headerN, ended ? std::to_string(endN).c_str() : "ABSENT", n);
    infra = true;
  }
  if (nodeMajor < 0) { std::printf("FAIL  STREAM  no NODE record\n"); infra = true; }
  if (ulpOut.size() < 2) { std::printf("FAIL  STREAM  the ULP control's row '%s' is not in the stream\n", kUlpScenario); infra = true; }
  const uint64_t tot = total(run);
  std::printf("TOTAL %s %d\n", hex(tot).c_str(), n);
  if (infra) { std::printf("h2_engine_selfdigest_check: INFRASTRUCTURE FAILURE — the stream is not a full render; nothing judged\n"); return 2; }

  // The reference for this key, else this run's own digests (the controls still run).
  const std::string key = refKey(nodeMajor), refFile = refPath(nodeMajor);
  std::map<std::string, uint64_t> ref, self;
  for (const auto& [name, v] : run) self[name] = v;
  std::vector<std::string> refOracles;
  uint64_t refTot = 0;
  const bool keyed = readRef(refFile, ref, refOracles, refTot);
  if (!keyed && std::filesystem::exists(refFile)) { std::printf("FAIL  the reference %s is malformed\n", refFile.c_str()); return 2; }
  const std::map<std::string, uint64_t>& judgeBy = keyed ? ref : self;
  int red = 0;

  // ULP: one sample one ULP up, mid-render; exactly that row red, by name.
  {
    Digests nudged = run;
    std::vector<double> o = ulpOut;
    const size_t at = o.size() / 2;
    o[at] = std::nextafter(o[at], INFINITY);
    for (auto& [name, v] : nudged) if (name == kUlpScenario) v = digest(o, ulpLog, ulpInfo);
    const std::vector<std::string> r = verdict(nudged, judgeBy), base = verdict(run, judgeBy);
    // judged as the CHANGE from this run's own verdict, so a red reference cannot hide or fake it
    std::vector<std::string> added;
    for (const std::string& x : r) { bool was = false; for (const std::string& y : base) was = was || x == y; if (!was) added.push_back(x); }
    const bool fired = added.size() == 1 && added[0] == kUlpScenario;
    if (!fired) red++;
    std::printf("%s  control ULP  sample %zu of '%s' nudged one ULP: %zu row(s) newly red%s%s\n", fired ? "PASS" : "FAIL", at, kUlpScenario,
                added.size(), added.empty() ? "" : ", first: ", added.empty() ? "" : added[0].c_str());
  }
  // FAULT: an engine constant changed (K1), every script re-rendered.
  {
    Digests faulted;
    for (const Scenario& sc : scripts) faulted.emplace_back(sc.name, render(sc, kFaultId));
    const std::vector<std::string> r = verdict(faulted, self);   // against this run: the fault's own effect
    const bool fired = static_cast<int>(r.size()) >= kFaultMinRed;
    if (!fired) red++;
    std::printf("%s  control FAULT  K1 (fault %d, M1 reverted to 0.08 per tick) turns %zu of %d rows red (need >= %d)%s%s\n", fired ? "PASS" : "FAIL",
                kFaultId, r.size(), n, kFaultMinRed, r.empty() ? "" : ", first: ", r.empty() ? "" : r[0].c_str());
  }

  if (repin) {
    if (red) { std::printf("h2_engine_selfdigest_check: REFUSING to re-pin: a control did not fire\n"); return 1; }
    FILE* w = std::fopen(refFile.c_str(), "wb");
    if (!w) { std::printf("h2_engine_selfdigest_check: cannot write %s\n", refFile.c_str()); return 2; }
    std::fprintf(w, "# h2_engine_selfdigest_check reference (tools/h2_engine_selfdigest_check.cpp): key %s\n", key.c_str());
    std::fprintf(w, "# FNV-1a 64 of each scenario's C++ samples, events and readouts. Re-pin ONLY for an output-changing\n"
                    "# divergence recorded in docs/port/divergences.json (ADR-187 items 5 and 7); an output-neutral PR never re-pins.\n");
    for (const std::string& o : oracles) std::fprintf(w, "# ORACLE %s\n", o.c_str());
    for (const auto& [name, v] : run) std::fprintf(w, "%s  %s\n", hex(v).c_str(), name.c_str());
    std::fprintf(w, "TOTAL %s %d\n", hex(tot).c_str(), n);
    std::fclose(w);
    std::printf("RE-PINNED  %s  (%d scenarios, TOTAL %s). Allowed ONLY for an output-changing divergence in the ledger.\n", refFile.c_str(), n, hex(tot).c_str());
    std::fprintf(stderr, "h2_engine_selfdigest_check: RE-PINNED %s — commit it only with its divergence record (ADR-187 item 5)\n", refFile.c_str());
    return 0;
  }

  if (!keyed) {
    // a reference for this platform under another compiler or Node major: warn loudly
    std::string other;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator("h2/engine", ec)) {
      const std::string fn = e.path().filename().string();
      if (fn.rfind(std::string("selfdigest.") + kPlatform + ".", 0) == 0) other = fn;
    }
    if (!other.empty()) {
      char w[320];
      std::snprintf(w, sizeof w, "WARNING: self-digest not judged — key %s has no reference, but this platform has h2/engine/%s; re-pin deliberately (L0072)\n",
                    key.c_str(), other.c_str());
      std::printf("%s", w);
      std::fprintf(stderr, "%s", w);
    }
    std::printf("SKIP  SELFDIGEST  no reference for %s: %d digests printed, NOT judged (never a pass)\n", key.c_str(), n);
    std::printf("h2_engine_selfdigest_check: %s — unkeyed (%s), controls %s\n", red ? "RED" : "SKIP", key.c_str(), red ? "FAILED" : "fired");
    return red ? 1 : 0;
  }
  const std::vector<std::string> r = verdict(run, ref);
  for (const std::string& x : r) std::printf("FAIL  SELFDIGEST  %s\n", x.c_str());
  if (!r.empty()) {
    red++;
    if (refOracles != oracles) std::printf("NOTE  the stream's ORACLE pins differ from the reference's: the golden (the scripts' source) moved\n");
  }
  std::printf("%s  SELFDIGEST  %d of %d scenarios bit-identical to %s (TOTAL %s, reference %s)\n", r.empty() ? "PASS" : "FAIL", n - static_cast<int>(r.size()),
              n, refFile.c_str(), hex(tot).c_str(), hex(refTot).c_str());
  std::printf("h2_engine_selfdigest_check: %s — %d scenarios against %s; %zu differ; %d red\n", red ? "RED" : "GREEN", n, key.c_str(), r.size(), red);
  return red ? 1 : 0;
}
