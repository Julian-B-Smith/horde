/*
 * h2_engine_selfdigest_check — the composed engine (h2/engine/engine.h) against
 * ITSELF: a C++-vs-C++ bit-identity digest of all the parity scenarios. ROADMAP
 * B441 phase 2 (the step before C1-C3); ADR-187 item 5.
 *
 * WIRED: ./verify full (both builds, after h2_engine_parity_check, on the stream it rendered).
 *
 * WHY. h2_engine_parity_check proves the C++ is within 1e-6 of the JS golden (rms
 * and max-abs) with a 30 % mean bit-exact floor. That is weaker than "the same bits
 * as today's C++": an optimisation that moves a sample by 1e-12 passes parity. B441's
 * output-neutral experiments (ADR-187 item 5: "output-neutral work ... must leave
 * every digest unchanged; it is NOT a divergence") need the stronger statement, and
 * this is it: every scenario's samples, events and readouts are bit-identical to the
 * pinned C++ render.
 *
 * TWO BUILDS OF THIS SOURCE, each against its own reference (the lead, 2026-10-04):
 *   parity   h2_engine_selfdigest_check: the parity check's flags (-O2
 *            -ffp-contract=off) with H2_ENGINE_FAULTS compiled in (fault 0), so the
 *            FAULT control can plant a fault. The build parity judges.
 *   product  h2_engine_selfdigest_product (H2_SELFDIGEST_PRODUCT): the release
 *            flags (-O3 -DNDEBUG -ffp-contract=off), no fault hooks. ADR-187 A1: the
 *            shipped build is the tested build, and C1-C3 target these flags. -O3
 *            and -O2 emit different libm call sites (tools/h2_libm_count.py), so
 *            they are digested apart. It runs the ULP control only (FAULT needs the
 *            hooks), and prints, as CONTEXT and never a verdict, how many scenarios
 *            its digests share with the committed parity reference.
 *
 * WHAT IS DIGESTED, per scenario: FNV-1a 64 over the raw bytes of every output
 * double (interleaved L/R, as the replay emits them), then the blade-event log (its
 * four counts as uint64, then its two order-sensitive 32-bit hashes, which fold
 * every event's tick, member and kind in order: js.h EventLog), then the three load
 * readouts (culled, refused, stolen) as raw doubles. The scripts are the parity
 * stream's (tools/h2_engine_render.mjs), replayed through h2_engine_stream.h exactly
 * as the parity check replays them, at the script's own host blocks. The JS samples
 * in the stream are not read. A digest is stable across process runs and host blocks
 * because the engine is (the parity check's DET rows); the one script that is
 * block-dependent by design (a culled voice's float32 tail) does not matter here,
 * since the blocks are the script's. The three chaotic ring rows are digested like
 * every other row: C++ against C++ is deterministic.
 * One line per scenario, `<16 hex>  <name>`, then `TOTAL <16 hex> <n>` (FNV-1a 64
 * over the lines, in stream order).
 *
 * THE REFERENCES are h2/engine/selfdigest.<build>.<platform>.<compiler>.node<major>.txt,
 * keyed like the parity check's bit-exact floor: the platform and compiler (libm,
 * codegen) and the golden's Node major (V8's libm computes the scripts' note
 * frequencies). On an unkeyed platform the digests are printed and NOT judged (a
 * SKIP, never a pass). A reference for this build and platform under another
 * compiler or Node major gets a LOUD stderr WARNING to re-pin, and stays a SKIP (the
 * floor's rule, L0072).
 *
 * THE REFERENCES ARE GOLDEN FIXTURES: A PROTECTED PATH (charter; ADR-187 item 7;
 * ruled by the lead 2026-10-04). A re-pin needs a recorded reason, stated in the PR
 * body and the trace, and the human reviews every re-pin through the PR. The reason
 * must be ONE of:
 *   (a) an output-changing divergence recorded in the ledger
 *       (docs/port/divergences.json; ADR-187 item 5);
 *   (b) a scenario-set change (rows added or changed in tools/h2_engine_render.mjs):
 *       the digest diff must touch ONLY those rows, which --repin lists (below) so a
 *       reviewer can confirm it;
 *   (c) a joint JS+C++ engine change landed under ADR-187 A2 item 3.
 * An "output-neutral" optimisation PR NEVER re-pins: a red here is the experiment
 * failing. It re-runs tools/h2_libm_count.py --write instead and commits its counts,
 * so a libm call that moved under the same bits is visible in its diff.
 *
 * RE-PIN (a human-visible act; both builds):
 *   node tools/h2_engine_render.mjs > /tmp/h2s
 *   build-release/h2_engine_selfdigest_check --full-from /tmp/h2s --repin
 *   build-release/h2_engine_selfdigest_product --full-from /tmp/h2s --repin
 * Each writes its build's reference from a full, clean stream, only when its
 * controls fired, and first prints every row whose digest differs from the old
 * reference (REPIN changed / added / removed), with the counts.
 *
 * MUST-FAIL CONTROLS (LIBRARY L0032), every run, judged by the same verdict code
 * as the reference comparison (against the reference where keyed, else against this
 * run's own clean digests):
 *   ULP    (both builds) one sample of kUlpScenario nudged by one ULP: exactly that
 *          scenario red, by name;
 *   FAULT  (parity build) fault 9 (K1, M1 reverted: the swarm's K smoother back to
 *          the constant 0.08 per tick) planted in the engine, every scenario
 *          re-rendered: at least kFaultMinRed scenarios red.
 *
 * KEYSELF (every run): the key's verdict on fabricated keys, the parity check's
 * FLOORKEY idiom. A made-up platform must read unkeyed with no warning, its ULP
 * control (judged against this run) must fire, and with every digest differing it
 * must exit 0 (a SKIP); on this platform another Node major must read unkeyed, and
 * warn wherever this platform has a reference. Nothing outside the binary (no
 * environment variable, no flag) can make the real key unkeyed.
 *
 * Usage: h2_engine_selfdigest_check|h2_engine_selfdigest_product [--full-from FILE|-] [--repin]
 *   With NO stream argument (tools/sanitize_oracles.sh runs every wired binary bare,
 *   the B384 trap) it spawns `node tools/h2_engine_render.mjs` itself, as
 *   h2_engine_parity_check does. --full-from FILE is that renderer's FULL output
 *   (./verify renders it once for the parity check, its FMA control and both builds
 *   of this check). A cut stream (no END record, a count that disagrees with the
 *   header, a renderer that exited non-zero) is an infrastructure failure, exit 2.
 *   Run from the repo root (the references are repo-relative).
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
#define popen _popen
#define pclose _pclose
#endif

#ifndef H2_SELFDIGEST_PRODUCT
#define H2_ENGINE_FAULTS 1
#endif
#include "../h2/engine/engine.h"
#include "h2_engine_stream.h"

using horde2::engine::EventLog;
using h2engine_stream::ReplayInfo;
using h2engine_stream::Scenario;

namespace {

#ifdef H2_SELFDIGEST_PRODUCT
constexpr const char* kBuild = "product";
constexpr const char* kTool = "h2_engine_selfdigest_product";
#else
constexpr const char* kBuild = "parity";
constexpr const char* kTool = "h2_engine_selfdigest_check";
#endif

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
std::string hex(uint64_t v) {
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(v));
  return b;
}

using Digests = std::vector<std::pair<std::string, uint64_t>>;   // stream order
using Ref = std::map<std::string, uint64_t>;
uint64_t total(const Digests& d) {
  Fnv f;
  for (const auto& [name, v] : d) { const std::string l = hex(v) + "  " + name + "\n"; f.bytes(l.data(), l.size()); }
  return f.h;
}

// THE verdict: the scenarios of `run` whose digest differs from `ref` (changed), is
// absent from it (added), or (the reverse) is pinned and was not rendered (removed).
// The controls and the re-pin listing go through this function too, so they test
// the code that judges.
struct Diff { std::vector<std::string> changed, added, removed; size_t size() const { return changed.size() + added.size() + removed.size(); } };
Diff verdict(const Digests& run, const Ref& ref) {
  Diff d;
  std::map<std::string, int> seen;
  for (const auto& [name, v] : run) {
    seen[name]++;
    const auto it = ref.find(name);
    if (it == ref.end()) d.added.push_back(name);
    else if (it->second != v) d.changed.push_back(name);
  }
  for (const auto& [name, v] : ref) if (!seen.count(name)) d.removed.push_back(name);
  return d;
}
void printDiff(const char* tag, const Diff& d) {
  for (const std::string& x : d.changed) std::printf("%s changed  %s\n", tag, x.c_str());
  for (const std::string& x : d.added) std::printf("%s added    %s  (not in the reference)\n", tag, x.c_str());
  for (const std::string& x : d.removed) std::printf("%s removed  %s  (pinned, not rendered)\n", tag, x.c_str());
}

std::string refKey(const std::string& platform, const std::string& compiler, int nodeMajor) {
  return platform + "." + compiler + ".node" + std::to_string(nodeMajor);
}
std::string refPath(const char* build, const std::string& key) { return std::string("h2/engine/selfdigest.") + build + "." + key + ".txt"; }

// The key's verdict, the one place the gate decides whether it judges. keyed: a
// reference exists for this build and key. warn: unkeyed, but this build HAS a
// reference on this platform under another compiler or Node major (`other` names
// it), i.e. an upgrade quietly turned the gate into a SKIP. Main and the KEYSELF
// self-test (which feeds it fabricated keys, as the parity check's FLOORKEY feeds
// floorVerdict) both decide through it, so the unkeyed path a Linux CI lane takes
// is exercised on every run here, never assumed.
struct KeyVerdict { bool keyed = false, warn = false; std::string key, file, other; };
KeyVerdict keyVerdict(const char* build, const std::string& platform, const std::string& compiler, int nodeMajor) {
  KeyVerdict v;
  v.key = refKey(platform, compiler, nodeMajor);
  v.file = refPath(build, v.key);
  v.keyed = std::filesystem::exists(v.file);
  if (!v.keyed) {
    std::error_code ec;
    const std::string prefix = std::string("selfdigest.") + build + "." + platform + ".";
    for (const auto& e : std::filesystem::directory_iterator("h2/engine", ec)) {
      const std::string fn = e.path().filename().string();
      if (fn.rfind(prefix, 0) == 0) v.other = fn;
    }
    v.warn = !v.other.empty();
  }
  return v;
}
// The exit a judged run ends with. Unkeyed, a differing digest cannot be red (it
// is not compared); a control that did not fire is red everywhere.
int exitFor(bool keyed, size_t differ, int red) { return red || (keyed && differ) ? 1 : 0; }

// Reads a reference file: `<16 hex>  <name>` lines; `#` lines and TOTAL are context.
// 0 absent, 1 read, -1 malformed.
int readRef(const std::string& path, Ref& ref, std::vector<std::string>& oracles, uint64_t& tot) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return 0;
  std::string line;
  while (h2engine_stream::readLine(f, line)) {
    if (line.rfind("# ORACLE ", 0) == 0) { oracles.push_back(line.substr(9)); continue; }
    if (line.empty() || line[0] == '#') continue;
    if (line.rfind("TOTAL ", 0) == 0) { tot = std::strtoull(line.c_str() + 6, nullptr, 16); continue; }
    if (line.size() < 19 || line.compare(16, 2, "  ") != 0) { std::fclose(f); return -1; }
    ref[line.substr(18)] = std::strtoull(line.substr(0, 16).c_str(), nullptr, 16);
  }
  std::fclose(f);
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  std::string path;
  bool repin = false;
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--full-from") == 0 && i + 1 < argc) path = argv[++i];
    else if (std::strcmp(argv[i], "--repin") == 0) repin = true;
    else { std::fprintf(stderr, "%s: unknown argument '%s' (usage: --full-from FILE|- [--repin])\n", kTool, argv[i]); return 2; }
  }
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
    // NO ARGUMENTS (tools/sanitize_oracles.sh runs every wired binary bare: the
    // B384 trap): render the stream here, as h2_engine_parity_check does.
    FILE* probe = std::fopen("tools/h2_engine_render.mjs", "rb");
    if (!probe) { std::fprintf(stderr, "%s: run from the repo root (tools/h2_engine_render.mjs not found)\n", kTool); return 2; }
    std::fclose(probe);
#ifdef _WIN32
    f = popen("node tools/h2_engine_render.mjs", "rb");
#else
    f = popen("node tools/h2_engine_render.mjs", "r");
#endif
    piped = true;
    path = "node tools/h2_engine_render.mjs";
  }
  if (!f) { std::fprintf(stderr, "%s: cannot open '%s'\n", kTool, path.c_str()); return 2; }

  std::string line;
  if (!h2engine_stream::readLine(f, line) || line.rfind("H2ENGINE 1", 0) != 0) {
    std::fprintf(stderr, "%s: bad stream header '%s'\n", kTool, line.c_str());
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
    if (op != "SCN") { std::fprintf(stderr, "%s: unexpected stream line '%s'\n", kTool, line.c_str()); infra = true; break; }
    Scenario sc;
    if (!h2engine_stream::readScenario(f, line, sc)) { std::fprintf(stderr, "%s: truncated stream in '%s'\n", kTool, sc.name.c_str()); infra = true; break; }
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
  if (piped) {
    const int st = pclose(f);
    if (st != 0) { std::printf("FAIL  STREAM  the golden's renderer exited with status %d\n", st); infra = true; }
  } else if (f != stdin) {
    std::fclose(f);
  }
  const int n = static_cast<int>(run.size());
  if (!ended || endN != headerN || n != headerN) {
    std::printf("FAIL  STREAM  not a full stream: header %ld, END %s, %d scenarios read\n", headerN, ended ? std::to_string(endN).c_str() : "ABSENT", n);
    infra = true;
  }
  if (nodeMajor < 0) { std::printf("FAIL  STREAM  no NODE record\n"); infra = true; }
  if (ulpOut.size() < 2) { std::printf("FAIL  STREAM  the ULP control's row '%s' is not in the stream\n", kUlpScenario); infra = true; }
  const uint64_t tot = total(run);
  std::printf("TOTAL %s %d (%s build)\n", hex(tot).c_str(), n, kBuild);
  if (infra) { std::printf("%s: INFRASTRUCTURE FAILURE — the stream is not a full render; nothing judged\n", kTool); return 2; }

  // The reference for this build and key, else this run's own digests (the controls still run).
  const KeyVerdict kv = keyVerdict(kBuild, kPlatform, compilerId(), nodeMajor);
  const std::string& key = kv.key;
  const std::string& refFile = kv.file;
  const bool keyed = kv.keyed;
  Ref ref, self;
  for (const auto& [name, v] : run) self[name] = v;
  std::vector<std::string> refOracles;
  uint64_t refTot = 0;
  if (keyed && readRef(refFile, ref, refOracles, refTot) != 1) { std::printf("FAIL  the reference %s is malformed\n", refFile.c_str()); return 2; }
  int red = 0;

  // ULP: one sample one ULP up, mid-render; exactly that row red, by name. Judged
  // as the CHANGE from this run's own verdict against `judgeBy`, so a red reference
  // can neither hide nor fake it. KEYSELF reruns it the unkeyed way (against self).
  const size_t ulpAt = ulpOut.size() / 2;
  auto ulpControl = [&](const Ref& judgeBy, size_t& newly, std::string& first) {
    Digests nudged = run;
    std::vector<double> o = ulpOut;
    o[ulpAt] = std::nextafter(o[ulpAt], INFINITY);
    for (auto& [name, v] : nudged) if (name == kUlpScenario) v = digest(o, ulpLog, ulpInfo);
    const Diff r = verdict(nudged, judgeBy), base = verdict(run, judgeBy);
    std::vector<std::string> added;
    for (const std::string& x : r.changed) { bool was = false; for (const std::string& y : base.changed) was = was || x == y; if (!was) added.push_back(x); }
    newly = added.size();
    first = added.empty() ? "" : added[0];
    return added.size() == 1 && added[0] == kUlpScenario && r.added.size() == base.added.size() && r.removed.size() == base.removed.size();
  };
  {
    size_t newly = 0;
    std::string first;
    const bool fired = ulpControl(keyed ? ref : self, newly, first);
    if (!fired) red++;
    std::printf("%s  control ULP  sample %zu of '%s' nudged one ULP: %zu row(s) newly red%s%s\n", fired ? "PASS" : "FAIL", ulpAt, kUlpScenario,
                newly, first.empty() ? "" : ", first: ", first.c_str());
  }
  // KEYSELF: the key's verdict on fabricated keys (the parity check's FLOORKEY
  // idiom), so the unkeyed path a Linux CI lane takes (digests printed, controls
  // judged against this run, the comparison SKIPPED, exit 0) and the upgrade
  // WARNING are exercised on every run, not assumed. No environment variable or
  // flag reaches the real key: the gate cannot be switched off from outside.
  {
    const KeyVerdict unkeyed = keyVerdict(kBuild, "selftest-unkeyed", compilerId(), nodeMajor),
                     otherNode = keyVerdict(kBuild, kPlatform, compilerId(), nodeMajor + 1);
    size_t newly = 0;
    std::string first;
    const bool ulpUnkeyed = ulpControl(self, newly, first);   // the unkeyed path judges its controls against this run
    const bool exits = exitFor(false, static_cast<size_t>(n), 0) == 0 && exitFor(false, 0, 1) == 1 && exitFor(true, 1, 0) == 1 && exitFor(true, 0, 0) == 0;
    const bool warnOk = !keyed || otherNode.warn;   // a platform that has a reference must warn on a Node upgrade
    const bool ok = !unkeyed.keyed && !unkeyed.warn && !otherNode.keyed && warnOk && ulpUnkeyed && exits;
    if (!ok) red++;
    std::printf("%s  KEYSELF  an unkeyed platform %s, its ULP control %s, and with every digest differing it exits %d (SKIP); "
                "on %s another Node major %s; keyed, one differing row exits %d\n",
                ok ? "PASS" : "FAIL", unkeyed.keyed ? "IS JUDGED" : (unkeyed.warn ? "prints and WARNS (wrong)" : "prints, no warning"),
                ulpUnkeyed ? "fires" : "DOES NOT FIRE", exitFor(false, static_cast<size_t>(n), 0), kPlatform,
                otherNode.keyed ? "IS JUDGED" : (otherNode.warn ? "prints and warns" : (keyed ? "DOES NOT WARN" : "prints (no reference on this platform)")),
                exitFor(true, 1, 0));
  }
#ifndef H2_SELFDIGEST_PRODUCT
  // FAULT: an engine constant changed (K1), every script re-rendered.
  {
    Digests faulted;
    for (const Scenario& sc : scripts) {
      std::vector<double> out;
      EventLog log;
      ReplayInfo info;
      h2engine_stream::replay(sc, out, kFaultId, &log, 0, &info);
      faulted.emplace_back(sc.name, digest(out, log, info));
    }
    const Diff r = verdict(faulted, self);   // against this run: the fault's own effect
    const bool fired = static_cast<int>(r.size()) >= kFaultMinRed;
    if (!fired) red++;
    std::printf("%s  control FAULT  K1 (fault %d, M1 reverted to 0.08 per tick) turns %zu of %d rows red (need >= %d)%s%s\n", fired ? "PASS" : "FAIL",
                kFaultId, r.size(), n, kFaultMinRed, r.changed.empty() ? "" : ", first: ", r.changed.empty() ? "" : r.changed[0].c_str());
  }
#else
  // CONTEXT, never a verdict: the product build against the committed PARITY
  // reference. -O3 may legitimately differ from -O2; the count is what a reader needs.
  {
    Ref par;
    std::vector<std::string> po;
    uint64_t pt = 0;
    if (readRef(refPath("parity", key), par, po, pt) == 1) {
      const Diff d = verdict(run, par);
      std::printf("CONTEXT  product vs parity (not judged): %d of %d scenarios share their digest with %s; %zu differ%s%s\n",
                  n - static_cast<int>(d.size()), n, refPath("parity", key).c_str(), d.size(),
                  d.changed.empty() ? "" : ", first: ", d.changed.empty() ? "" : d.changed[0].c_str());
    } else {
      std::printf("CONTEXT  product vs parity (not judged): no parity reference for %s\n", key.c_str());
    }
  }
#endif

  if (repin) {
    if (red) { std::printf("%s: REFUSING to re-pin: a control did not fire\n", kTool); return 1; }
    // What the re-pin changes, row by row: case (b) must touch only the rows it adds or changes.
    if (keyed) {
      const Diff d = verdict(run, ref);
      printDiff("REPIN", d);
      std::printf("REPIN  %s: %zu changed, %zu added, %zu removed, %d unchanged\n", refFile.c_str(), d.changed.size(), d.added.size(), d.removed.size(),
                  n - static_cast<int>(d.changed.size() + d.added.size()));
    } else {
      std::printf("REPIN  %s: no previous reference (every row is new)\n", refFile.c_str());
    }
    FILE* w = std::fopen(refFile.c_str(), "wb");
    if (!w) { std::printf("%s: cannot write %s\n", kTool, refFile.c_str()); return 2; }
    std::fprintf(w, "# %s reference (tools/h2_engine_selfdigest_check.cpp): %s build, key %s\n", kTool, kBuild, key.c_str());
    std::fprintf(w, "# FNV-1a 64 of each scenario's C++ samples, events and readouts. A golden fixture (protected): re-pin only\n"
                    "# for a reason stated in the PR and the trace (the tool header's cases a-c); an output-neutral PR never re-pins.\n");
    for (const std::string& o : oracles) std::fprintf(w, "# ORACLE %s\n", o.c_str());
    for (const auto& [name, v] : run) std::fprintf(w, "%s  %s\n", hex(v).c_str(), name.c_str());
    std::fprintf(w, "TOTAL %s %d\n", hex(tot).c_str(), n);
    std::fclose(w);
    std::printf("RE-PINNED  %s  (%d scenarios, TOTAL %s). A golden fixture: state the reason (a, b or c) in the PR and the trace.\n", refFile.c_str(), n,
                hex(tot).c_str());
    std::fprintf(stderr, "%s: RE-PINNED %s — commit it only with its stated reason (tool header, cases a-c)\n", kTool, refFile.c_str());
    return 0;
  }

  if (!keyed) {
    // a reference for this build and platform under another compiler or Node major: warn loudly
    if (kv.warn) {
      char w[320];
      std::snprintf(w, sizeof w, "WARNING: self-digest (%s) not judged — key %s has no reference, but this platform has h2/engine/%s; re-pin deliberately (L0072)\n",
                    kBuild, key.c_str(), kv.other.c_str());
      std::printf("%s", w);
      std::fprintf(stderr, "%s", w);
    }
    std::printf("SKIP  SELFDIGEST  no %s reference for %s: %d digests printed, NOT judged (never a pass)\n", kBuild, key.c_str(), n);
    std::printf("%s: %s — unkeyed (%s), controls %s\n", kTool, red ? "RED" : "SKIP", key.c_str(), red ? "FAILED" : "fired");
    return exitFor(false, static_cast<size_t>(n), red);
  }
  const Diff r = verdict(run, ref);
  printDiff("FAIL  SELFDIGEST", r);
  if (r.size()) {
    red++;
    if (refOracles != oracles) std::printf("NOTE  the stream's ORACLE pins differ from the reference's: the golden (the scripts' source) moved\n");
  }
  std::printf("%s  SELFDIGEST  %d of %d scenarios bit-identical to %s (TOTAL %s, reference %s)\n", r.size() ? "FAIL" : "PASS",
              n - static_cast<int>(r.changed.size() + r.added.size()), n, refFile.c_str(), hex(tot).c_str(), hex(refTot).c_str());
  std::printf("%s: %s — %s build, %d scenarios against %s; %zu differ; %d red\n", kTool, red ? "RED" : "GREEN", kBuild, n, key.c_str(), r.size(), red);
  return exitFor(true, r.size(), red);
}
