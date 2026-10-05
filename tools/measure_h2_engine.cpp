/*
 * measure_h2_engine — CPU cost of horde 2's composed engine (h2/engine/engine.h).
 * ROADMAP B385 checkpoint 4 (the replay against the golden) and B441 phase 1(A)
 * (the frozen protocol, the ledger and the cost attribution: docs/port/cpu-ledger.md,
 * which is the protocol's text; this file is its implementation). ADR-187 item 8:
 * CPU is Layer-E, measured by hand on a named machine, never a gate.
 *
 * UNWIRED: a timing measurement, not a pass/fail gate (the measure_* contract): it
 * prints, never judges, and carries no threshold to weaken. The load guard below
 * refuses to MEASURE on a busy machine; it never turns a figure into a verdict.
 *
 * MODES (run from the repo root; the release build, -O3 -ffp-contract=off, h2 rule 7):
 *   build-release/measure_h2_engine [stream-file]
 *       Checkpoint 4. `node tools/h2_engine_render.mjs --bench --emit` writes three
 *       scripts (one voice held 4 s at 48 kHz: Crushed bells, Glass horde pad,
 *       Quarter sync); each is replayed best of five and divided by the audio
 *       duration, with its distance from the golden.
 *   build-release/measure_h2_engine --ledger [--sr 44100|48000] [--presets <file>] [--reps R] [--force] [--also <preset>]...
 *       B441's frozen protocol (docs/port/cpu-ledger.md, "Protocol"): the ledger
 *       presets x 1/8/16 voices, interleaved best of R (default 5), the calibration
 *       loop each repeat, the load guard before each repeat. One `LEDGER {json}` row
 *       per cell on stdout. --also appends a bank preset's cells after the frozen
 *       seven, measured in the same interleave under the same protocol (B441 C3:
 *       one preset per specialised kernel); the frozen cells are unchanged by it.
 *   THE LOAD GUARD (B441-3): before every repeat, outside the timed region, the
 *       1-minute load average must be <= 3.0 AND no other process may use >= 50 % of
 *       a core (a `ps` snapshot). Either failing waits 20 s and re-checks, for up to
 *       10 min, then exit 3. B441-3 added the per-process half: a Chrome tab, another
 *       session's Node run and a sibling repo's mutation test each burned a full core
 *       on 2026-10-04 while the load average could still read under 3. Every LEDGER
 *       row records the top 3 foreign processes (basename, %CPU) as its best repeat began.
 *   --sr R (any mode but the checkpoint-4 replay, which runs at its stream's own
 *       rate): the cell's sample rate. Protocol B441-2: 44.1 kHz is the reference
 *       (the lead's E-6 ruling) and the default; 48 kHz, B441-1's rate, is secondary.
 *   build-release/measure_h2_engine --sweep [--presets <file>] [--force]
 *       Every bank preset at one voice, best of 2, 1 s timed: a coarse ranking used
 *       to choose the ledger's light-to-heavy picks. Not a ledger figure.
 *   build-release/measure_h2_engine --hold <preset|defaults> <voices> <seconds>
 *       Renders the protocol's cell for that long and prints nothing else: a target
 *       for an external sampling profiler (`sample <pid>`).
 *   build-release/measure_h2_engine_stages --stages [--presets <file>] [--reps R] [--force]
 *       The cost attribution. Only the measure_h2_engine_stages target (the same
 *       file built with H2_ENGINE_STAGES) has it: each run skips one engine stage
 *       (engine.h, enum Stage) and the stage's cost is the full time minus that.
 *
 * PRESETS come from the parity stream itself (`node tools/h2_engine_render.mjs
 * --only '^P/.* :: chord$'`): each P row is presetCmds(params) followed by its
 * phrase, so the commands before the first note-on are exactly the preset as the
 * parity check plays it (with render-goldens' gain 0.35). "defaults" is the engine
 * with no preset (the oracle's defaults: N 5, a sync blade, os 2).
 *
 * THE CELL (frozen; the doc's protocol section explains each choice):
 *   44.1 kHz (--sr; 48 kHz secondary), 128-sample blocks, the preset's own oversampling (no bank preset sets
 *   os, so every one runs at the engine's 2), poly 8. V voices: V notes at keys
 *   48 + 3k (k = 0..V-1, B378's chord), velocity 0.85, all on before the first block.
 *   16 voices are two engines of 8 (kVoices is 8, engine.h), the second on keys
 *   72.. and seeded one higher. 0.25 s rendered untimed (the onset, the first DC
 *   estimates), then 2.0 s timed. Every block's first sample goes into a volatile
 *   sink so the optimiser cannot drop the render.
 * CALIBRATION: 1e8 dependent multiply-adds (checkpoint 4's loop, so its figures
 * compare), timed every repeat, best kept. A row's `ratio` is the cell's best
 * seconds per audio second over the calibration's best seconds: the machine-speed-
 * normalised figure later runs are compared on.
 */
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#else
#include <stdlib.h>   // getloadavg
#include <unistd.h>   // getpid
#endif

#include "../h2/engine/engine.h"
#include "h2_engine_stream.h"

namespace {

using Engine = horde2::engine::Engine;
using Clock = std::chrono::steady_clock;

// ---- the frozen protocol's constants (docs/port/cpu-ledger.md, "Protocol") ----
// The cell's rate, set once from --sr before any engine is built. B441-2: 44.1 kHz is
// the reference (E-6), 48 kHz (B441-1's only rate) the secondary.
double gSR = 44100;
constexpr int kBlock = 128;
constexpr double kWarmSeconds = 0.25, kTimedSeconds = 2.0;
constexpr uint32_t kSeed = 0xB385;   // checkpoint 4's bench seed
constexpr double kLoadMax = 3.0;     // 1-minute load average above which no repeat starts
constexpr int kLoadWaits = 30;       // x 20 s: wait up to 10 min for the machine to settle
constexpr double kForeignMax = 50;   // B441-3: % of one core any OTHER process may use as a repeat starts
constexpr double kMinSpecFactor = 1.5;   // M3 -> min-spec, an ASSUMPTION (module-1.0-bar.md appendix)
constexpr double kSlice = 34;            // the sources' slice of min-spec, % (same appendix)
// The ledger's presets, light to heavy (the doc says why each; --sweep ranked the bank).
// "defaults" is no preset.
const char* const kLedger[] = {"Quarter sync", "defaults", "Harmonic stack", "Fold over sync",
                               "Crushed bells", "Breathing pad", "Glass horde pad"};
const int kVoiceCounts[] = {1, 8, 16};

double sinkAcc = 0;
volatile double sink = 0;

double calibrate() {
  double x = 0;
  const auto t0 = Clock::now();
  for (int i = 0; i < 100000000; i++) x = x * 1.0000001 + 1e-9;
  const double s = std::chrono::duration<double>(Clock::now() - t0).count();
  sink = x;
  return s;
}

double load1() {
#ifdef _WIN32
  return 0;
#else
  double l[3] = {0, 0, 0};
  return getloadavg(l, 3) > 0 ? l[0] : 0;
#endif
}

// One other process in the guard's snapshot. `name` is the executable's basename, so a
// row never carries a machine path (ps prints the full path on macOS).
struct Proc { double pcpu; std::string name; };

// Every process but this one, highest %CPU first: `ps -A -o pcpu=,pid=,comm=`. macOS's
// pcpu is a decaying average over roughly the last minute, so a process that just
// stopped still reads warm for a while; the guard then waits it out, which is the
// intent. Called only between repeats, never inside the timed region.
std::vector<Proc> foreignProcs() {
  std::vector<Proc> out;
#ifndef _WIN32
  FILE* f = popen("ps -A -o pcpu=,pid=,comm= 2>/dev/null", "r");
  if (!f) return out;
  const long self = static_cast<long>(getpid());
  char buf[4096];
  while (std::fgets(buf, sizeof buf, f)) {
    double pc = 0; long pid = 0; int n = 0;
    if (std::sscanf(buf, " %lf %ld %n", &pc, &pid, &n) < 2 || pid == self) continue;
    std::string name(buf + n);
    while (!name.empty() && (name.back() == '\n' || name.back() == '\r' || name.back() == ' ')) name.pop_back();
    const size_t slash = name.rfind('/');
    if (slash != std::string::npos) name.erase(0, slash + 1);
    out.push_back({pc, name});
  }
  pclose(f);
  std::stable_sort(out.begin(), out.end(), [](const Proc& x, const Proc& y) { return x.pcpu > y.pcpu; });
#endif
  return out;
}

// What the guard saw when it let a repeat start.
struct GuardSnap { double load = 0; std::vector<Proc> top; };   // top: at most 3

// The load guard (B441-3): wait (never measure) while the 1-minute load is over kLoadMax
// or any other process uses >= kForeignMax % of a core. Returns false if it gave up;
// `waits` counts the 20 s waits. --force skips the waiting, never the snapshot.
bool loadGuard(bool force, GuardSnap& g, int& waits) {
  for (int w = 0;; w++) {
    g.load = load1();
    std::vector<Proc> ps = foreignProcs();
    if (ps.size() > 3) ps.resize(3);
    g.top = ps;
    const bool busyLoad = g.load > kLoadMax, busyProc = !ps.empty() && ps[0].pcpu >= kForeignMax;
    if (force || (!busyLoad && !busyProc)) return true;
    if (w >= kLoadWaits) {
      std::fprintf(stderr, "measure_h2_engine: still busy after %d waits (load %.2f; top process '%s' %.1f %%), not measuring\n",
                   kLoadWaits, g.load, ps.empty() ? "-" : ps[0].name.c_str(), ps.empty() ? 0.0 : ps[0].pcpu);
      return false;
    }
    if (busyProc)
      std::fprintf(stderr, "measure_h2_engine: '%s' uses %.1f %% >= %.0f %% of a core, waiting 20 s (%d/%d)\n",
                   ps[0].name.c_str(), ps[0].pcpu, kForeignMax, w + 1, kLoadWaits);
    else
      std::fprintf(stderr, "measure_h2_engine: load %.2f > %.1f, waiting 20 s (%d/%d)\n", g.load, kLoadMax, w + 1, kLoadWaits);
    waits++;
    std::this_thread::sleep_for(std::chrono::seconds(20));
  }
}

// The snapshot as a JSON array, names escaped (a process name is free text).
std::string topJson(const std::vector<Proc>& top) {
  std::string s = "[";
  for (size_t i = 0; i < top.size(); i++) {
    if (i) s += ",";
    s += "{\"comm\":\"";
    for (unsigned char ch : top[i].name) {
      if (ch == '"' || ch == '\\') { s += '\\'; s += static_cast<char>(ch); }
      else if (ch < 0x20) s += ' ';
      else s += static_cast<char>(ch);
    }
    char b[32];
    std::snprintf(b, sizeof b, "\",\"pcpu\":%.1f}", top[i].pcpu);
    s += b;
  }
  return s + "]";
}
std::string topText(const std::vector<Proc>& top) {
  std::string s;
  for (const Proc& p : top) {
    char b[32];
    std::snprintf(b, sizeof b, " %.1f%%", p.pcpu);
    s += (s.empty() ? "" : ", ") + p.name + b;
  }
  return s.empty() ? "-" : s;
}

struct Patch { std::string name; std::vector<h2engine_stream::Cmd> cmds; };

// --presets <file>: that stream saved beforehand. The protocol uses it, because rendering
// the 83 goldens takes Node ~6 s on 7 workers, which lifts the load average the guard
// reads right before the first repeat.
std::string gPresetFile;

// Every bank preset from the parity stream (header): the commands before the first note-on.
bool loadPresets(std::vector<Patch>& out) {
  const bool piped = gPresetFile.empty();
  FILE* f = piped ? popen("node tools/h2_engine_render.mjs --only '^P/.* :: chord$' 2>/dev/null", "r")
                  : std::fopen(gPresetFile.c_str(), "rb");
  if (!f) return false;
  auto done = [&] { if (piped) pclose(f); else std::fclose(f); };
  std::string line;
  h2engine_stream::readLine(f, line);
  while (h2engine_stream::readLine(f, line)) {
    if (line.rfind("SCN ", 0) != 0) { if (line.rfind("END", 0) == 0) break; continue; }
    h2engine_stream::Scenario sc;
    if (!h2engine_stream::readScenario(f, line, sc)) { done(); return false; }
    Patch p;
    // "P/<category> / <name> :: chord" -> <name>
    const size_t a = sc.name.rfind(" / "), b = sc.name.rfind(" :: ");
    p.name = sc.name.substr(a + 3, b - a - 3);
    for (const auto& c : sc.cmds) { if (c.op == "on") break; p.cmds.push_back(c); }
    out.push_back(p);
  }
  done();
  Patch d; d.name = "defaults";
  out.push_back(d);
  return true;
}
const Patch* findPatch(const std::vector<Patch>& ps, const std::string& name) {
  for (const Patch& p : ps) if (p.name == name) return &p;
  return nullptr;
}
bool isMono(const Patch& p) {
  for (const auto& c : p.cmds) if (c.op == "set" && c.key == "polyMode" && c.a != 0) return true;
  return false;
}

struct Cell {
  const Patch* patch;
  int voices;
  double os;          // > 0 overrides the preset's oversampling (the stage table's os-1 row)
  unsigned stageOff;  // engine stages skipped (the stages build only)
  double best = 1e30;
  GuardSnap atBest;   // B441-3: the guard's snapshot at the start of the best repeat
};

// One run of the protocol's cell; returns the timed seconds.
double runCell(const Cell& c, double timedSeconds = kTimedSeconds, horde2::engine::EventLog* log = nullptr) {
  const int engines = (c.voices + 7) / 8;
  Engine* e[2] = {nullptr, nullptr};
  for (int k = 0; k < engines; k++) {
    e[k] = new Engine(gSR);   // ~114 KB: the heap
    e[k]->seedRandom(kSeed + k);
    for (const auto& m : c.patch->cmds) {
      if (m.op == "set") e[k]->set(m.key.c_str(), m.a);
      else if (m.op == "sets") e[k]->setString(m.key.c_str(), m.str.c_str());
      else if (m.op == "snap") e[k]->snap();
    }
    e[k]->set("poly", 8);
    if (c.os > 0) e[k]->set("os", c.os);
#ifdef H2_ENGINE_STAGES
    e[k]->stageOff = c.stageOff;
#endif
    e[k]->events = log;
  }
  for (int v = 0; v < c.voices; v++) {
    const int note = 48 + 3 * v;
    e[v / 8]->noteOn(note, 440 * std::pow(2, (note - 69) / 12.0), 0.85);
  }
  double L[kBlock], R[kBlock];
  const int warm = static_cast<int>(std::lround(kWarmSeconds * gSR / kBlock));
  const int timed = static_cast<int>(std::lround(timedSeconds * gSR / kBlock));
  for (int b = 0; b < warm; b++) for (int k = 0; k < engines; k++) e[k]->render(L, R, kBlock);
  const auto t0 = Clock::now();
  for (int b = 0; b < timed; b++) for (int k = 0; k < engines; k++) { e[k]->render(L, R, kBlock); sinkAcc += L[0]; }
  const double s = std::chrono::duration<double>(Clock::now() - t0).count();
  sink = sinkAcc;
  for (int k = 0; k < engines; k++) delete e[k];
  return s;
}
double audioSeconds(double timedSeconds = kTimedSeconds) { return std::lround(timedSeconds * gSR / kBlock) * kBlock / gSR; }

std::string gitHead() {
  // --dirty: a row measured on an uncommitted tree says so
  FILE* f = popen("git describe --always --dirty --abbrev=7 2>/dev/null", "r");
  if (!f) return "?";
  char buf[64] = {0};
  if (!std::fgets(buf, sizeof buf, f)) buf[0] = 0;
  pclose(f);
  std::string s(buf);
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
  return s.empty() ? "?" : s;
}

// Interleaved best-of-`reps` over `cells`, with the calibration and the load guard
// before each repeat. Returns false if the guard gave up.
bool measure(std::vector<Cell>& cells, int reps, bool force, double& calBest, double& loadLo, double& loadHi,
             int& waits, double timedSeconds = kTimedSeconds) {
  calBest = 1e30; loadLo = 1e30; loadHi = 0; waits = 0;
  for (int r = 0; r < reps; r++) {
    GuardSnap g;
    if (!loadGuard(force, g, waits)) return false;
    const double l = g.load;
    loadLo = std::min(loadLo, l); loadHi = std::max(loadHi, l);
    calBest = std::min(calBest, calibrate());
    for (Cell& c : cells) {
      const double t = runCell(c, timedSeconds);
      if (t < c.best) { c.best = t; c.atBest = g; }
    }
    const double le = load1();
    loadLo = std::min(loadLo, le); loadHi = std::max(loadHi, le);
    std::fprintf(stderr, "measure_h2_engine: repeat %d/%d done (load %.2f -> %.2f; top foreign at its start: %s)\n", r + 1, reps,
                 l, le, topText(g.top).c_str());
  }
  return true;
}

int argInt(int argc, char** argv, const char* key, int dflt) {
  for (int i = 1; i + 1 < argc; i++) if (std::strcmp(argv[i], key) == 0) return std::atoi(argv[i + 1]);
  return dflt;
}
bool hasArg(int argc, char** argv, const char* key) {
  for (int i = 1; i < argc; i++) if (std::strcmp(argv[i], key) == 0) return true;
  return false;
}

int ledger(int argc, char** argv) {
  std::vector<Patch> ps;
  if (!loadPresets(ps)) { std::fprintf(stderr, "measure_h2_engine: cannot read the presets (run from the repo root)\n"); return 1; }
  std::vector<Cell> cells;
  for (const char* n : kLedger) {
    const Patch* p = findPatch(ps, n);
    if (!p) { std::fprintf(stderr, "measure_h2_engine: no preset '%s'\n", n); return 1; }
    for (int v : kVoiceCounts) cells.push_back({p, v, 0, 0});
  }
  for (int i = 1; i + 1 < argc; i++) {
    if (std::strcmp(argv[i], "--also") != 0) continue;
    const Patch* p = findPatch(ps, argv[i + 1]);
    if (!p) { std::fprintf(stderr, "measure_h2_engine: no preset '%s'\n", argv[i + 1]); return 1; }
    for (int v : kVoiceCounts) cells.push_back({p, v, 0, 0});
  }
  double cal, lo, hi;
  int waits = 0;
  if (!measure(cells, argInt(argc, argv, "--reps", 5), hasArg(argc, argv, "--force"), cal, lo, hi, waits)) return 3;
  const std::string head = gitHead();
  const double audio = audioSeconds();
  std::printf("calibration: best %.1f ms; load average %.2f..%.2f; guard waits %d; head %s; %.0f Hz\n", cal * 1000, lo, hi,
              waits, head.c_str(), gSR);
  std::printf("%-18s %3s  %9s  %9s  %8s  %13s  %s\n", "preset", "V", "% M3 core", "per voice", "ratio", "min-spec*", "vs 34 % slice (8 voices only)");
  for (const Cell& c : cells) {
    const double pct = 100 * c.best / audio, ratio = (c.best / audio) / cal, ms = pct * kMinSpecFactor;
    std::printf("%-18s %3d  %8.2f%%  %8.3f%%  %8.4f  %12.1f%%  %s\n", c.patch->name.c_str(), c.voices, pct, pct / c.voices, ratio, ms,
                c.voices == 8 ? (ms > kSlice ? "OVER" : "under") : "");
  }
  for (const Cell& c : cells) {
    const double pct = 100 * c.best / audio, ratio = (c.best / audio) / cal, ms = pct * kMinSpecFactor;
    std::printf("LEDGER {\"protocol\":\"B441-3\",\"head\":\"%s\",\"preset\":\"%s\",\"voices\":%d,\"os\":\"preset\",\"sr\":%.0f,"
                "\"block\":%d,\"timed_s\":%.4f,\"best_s\":%.6f,\"pct_m3\":%.4f,\"pct_m3_per_voice\":%.4f,\"cal_ms\":%.2f,"
                "\"ratio\":%.6f,\"minspec_pct_assumed_x1.5\":%.3f,\"slice_pct\":%.0f,\"over_slice\":%s,\"load_lo\":%.2f,\"load_hi\":%.2f,"
                "\"guard_waits\":%d,\"foreign_top3\":%s}\n",
                head.c_str(), c.patch->name.c_str(), c.voices, gSR, kBlock, audio, c.best, pct, pct / c.voices, cal * 1000, ratio, ms,
                kSlice, c.voices == 8 ? (ms > kSlice ? "true" : "false") : "null", lo, hi, waits, topJson(c.atBest.top).c_str());
  }
  return 0;
}

int sweep(int argc, char** argv) {
  std::vector<Patch> ps;
  if (!loadPresets(ps)) { std::fprintf(stderr, "measure_h2_engine: cannot read the presets\n"); return 1; }
  std::vector<Cell> cells;
  for (const Patch& p : ps) cells.push_back({&p, 1, 0, 0});
  double cal, lo, hi;
  int waits = 0;
  if (!measure(cells, 2, hasArg(argc, argv, "--force"), cal, lo, hi, waits, 1.0)) return 3;
  std::sort(cells.begin(), cells.end(), [](const Cell& a, const Cell& b) { return a.best > b.best; });
  std::printf("sweep (coarse, NOT a ledger figure): one voice, best of 2, 1 s timed; calibration %.1f ms; load %.2f..%.2f\n", cal * 1000, lo, hi);
  for (const Cell& c : cells)
    std::printf("SWEEP %7.3f %%  %s%s\n", 100 * c.best / audioSeconds(1.0), c.patch->name.c_str(), isMono(*c.patch) ? "  (mono)" : "");
  return 0;
}

int hold(int argc, char** argv) {
  if (argc < 5) { std::fprintf(stderr, "usage: measure_h2_engine --hold <preset|defaults> <voices> <seconds>\n"); return 1; }
  std::vector<Patch> ps;
  if (!loadPresets(ps)) return 1;
  const Patch* p = findPatch(ps, argv[2]);
  if (!p) { std::fprintf(stderr, "measure_h2_engine: no preset '%s'\n", argv[2]); return 1; }
  const Cell c{p, std::atoi(argv[3]), 0, 0};
  const double s = runCell(c, std::atof(argv[4]));
  std::printf("hold: %s, %d voices, %.1f s audio in %.3f s\n", argv[2], c.voices, std::atof(argv[4]), s);
  return 0;
}

#ifdef H2_ENGINE_STAGES
int stages(int argc, char** argv) {
  std::vector<Patch> ps;
  if (!loadPresets(ps)) return 1;
  struct St { const char* name; unsigned bit; };
  const St kSt[] = {{"swarm control tick (/16)", Engine::kStageTick}, {"swarm phase advance", Engine::kStagePhase},
                    {"blade evaluation (out)", Engine::kStageBlade}, {"PolyBLEP (edges + hAt probes)", Engine::kStageBlep},
                    {"DC estimate refresh", Engine::kStageDc}, {"os-rate output biquads", Engine::kStageDecim},
                    {"DC blocker + tanh", Engine::kStageOut}, {"couple + spread (/32)", Engine::kStageCouple}};
  unsigned all = 0;
  for (const St& s : kSt) all |= s.bit;
  const char* const heavy[] = {"Glass horde pad", "Crushed bells"};
  const int vs[] = {1, 8};
  std::vector<Cell> cells;
  for (const char* n : heavy) {
    const Patch* p = findPatch(ps, n);
    if (!p) return 1;
    for (int v : vs) {
      cells.push_back({p, v, 0, 0});
      for (const St& s : kSt) cells.push_back({p, v, 0, s.bit});
      cells.push_back({p, v, 0, all});
      cells.push_back({p, v, 1, 0});   // os 1, nothing skipped: what oversampling costs
    }
  }
  double cal, lo, hi;
  int waits = 0;
  if (!measure(cells, argInt(argc, argv, "--reps", 5), hasArg(argc, argv, "--force"), cal, lo, hi, waits)) return 3;
  std::printf("stages: calibration %.1f ms; load %.2f..%.2f; head %s; best of %d, interleaved\n", cal * 1000, lo, hi,
              gitHead().c_str(), argInt(argc, argv, "--reps", 5));
  const double audio = audioSeconds();
  for (size_t i = 0; i < cells.size();) {
    const Cell& full = cells[i];
    const double T = full.best;
    std::printf("\n%s, %d voice(s): full %.3f %% of an M3 core (%.3f %% per voice)\n", full.patch->name.c_str(), full.voices,
                100 * T / audio, 100 * T / audio / full.voices);
    double sum = 0;
    for (size_t s = 0; s < sizeof kSt / sizeof kSt[0]; s++) {
      const double d = T - cells[i + 1 + s].best;
      sum += d;
      std::printf("  %-32s  without %.3f %%   share %6.1f %%\n", kSt[s].name, 100 * cells[i + 1 + s].best / audio, 100 * d / T);
      std::printf("STAGE {\"preset\":\"%s\",\"voices\":%d,\"stage\":\"%s\",\"full_s\":%.6f,\"without_s\":%.6f,\"share\":%.4f}\n",
                  full.patch->name.c_str(), full.voices, kSt[s].name, T, cells[i + 1 + s].best, d / T);
    }
    const double rest = cells[i + 9].best;
    std::printf("  %-32s  %.3f %%   share %6.1f %%\n", "remainder (all eight skipped)", 100 * rest / audio, 100 * rest / T);
    std::printf("  %-32s  %6.1f %%  (additivity check: 100 %% if the stages do not interact)\n", "sum of shares + remainder",
                100 * (sum + rest) / T);
    std::printf("  %-32s  %.3f %%   (os 2 -> 1 saves %.1f %%)\n", "os 1, nothing skipped", 100 * cells[i + 10].best / audio,
                100 * (T - cells[i + 10].best) / T);
    i += 11;
  }
  // Op counts, untimed: BLEP edges (each one costs two full blade evaluations in hAt)
  // per member-step, one voice, the timed window only.
  std::printf("\nop counts (one voice, %.1f s, untimed): BLEP edges per member-step at the preset's os\n", kTimedSeconds);
  for (const char* n : heavy) {
    const Patch* p = findPatch(ps, n);
    horde2::engine::EventLog log;
    runCell({p, 1, 0, 0}, kTimedSeconds, &log);
    double N = 5, os = 2;
    for (const auto& c : p->cmds) if (c.op == "set" && c.key == "N") N = c.a; else if (c.op == "set" && c.key == "os") os = c.a;
    const double steps = N * os * (kWarmSeconds + kTimedSeconds) * gSR;
    std::printf("  %-16s  tryE edges %llu, scan edges %llu, blade-1 entries %llu, blade-2 entries %llu over %.0f member-steps"
                " (warm-up included): %.4f BLEP edges per member-step\n", n,
                static_cast<unsigned long long>(log.count[1]), static_cast<unsigned long long>(log.count[2]),
                static_cast<unsigned long long>(log.count[3]), static_cast<unsigned long long>(log.count[4]), steps,
                (log.count[1] + log.count[2]) / steps);
  }
  return 0;
}
#endif

// Checkpoint 4: replay the bench stream, against the golden.
int replayBench(int argc, char** argv) {
  FILE* f = nullptr;
  bool piped = false;
  if (argc > 1) f = std::fopen(argv[1], "rb");
  else {
#ifdef _WIN32
    f = popen("node tools/h2_engine_render.mjs --bench --emit", "rb");
#else
    f = popen("node tools/h2_engine_render.mjs --bench --emit", "r");
#endif
    piped = true;
  }
  if (!f) { std::fprintf(stderr, "measure_h2_engine: cannot open the bench stream (run from the repo root)\n"); return 1; }
  const double cal = calibrate();
  std::printf("calibration: 1e8 dependent multiply-adds in %.1f ms (x=%.3f)\n", cal * 1000, static_cast<double>(sink));
  std::string line;
  h2engine_stream::readLine(f, line);   // header
  std::vector<double> out;
  while (h2engine_stream::readLine(f, line)) {
    if (line.rfind("SCN ", 0) != 0) { if (line.rfind("END", 0) == 0) break; continue; }
    h2engine_stream::Scenario sc;
    if (!h2engine_stream::readScenario(f, line, sc)) { std::fprintf(stderr, "measure_h2_engine: truncated stream\n"); return 1; }
    double audio = 0;
    for (const h2engine_stream::Cmd& c : sc.cmds) if (c.op == "render") audio += c.a * c.b;
    audio /= sc.sr;
    double best = 1e30;
    for (int r = 0; r < 5; r++) {
      const auto t0 = Clock::now();
      h2engine_stream::replay(sc, out);
      const double s = std::chrono::duration<double>(Clock::now() - t0).count();
      if (s < best) best = s;
    }
    double e = 0, mx = 0;
    for (size_t i = 0; i < out.size() && i < sc.js.size(); i++) { const double d = std::fabs(out[i] - sc.js[i]); e += d * d; if (d > mx) mx = d; }
    std::printf("%-24s C++ %6.3f %% of real time per voice (%.4f s for %.2f s, best of 5); vs the golden rms %.3e max %.3e\n",
                sc.name.c_str(), 100 * best / audio, best, audio, std::sqrt(e / static_cast<double>(out.size())), mx);
  }
  if (piped) pclose(f); else std::fclose(f);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  for (int i = 1; i + 1 < argc; i++) if (std::strcmp(argv[i], "--presets") == 0) gPresetFile = argv[i + 1];
  for (int i = 1; i + 1 < argc; i++) if (std::strcmp(argv[i], "--sr") == 0) gSR = std::atof(argv[i + 1]);
  if (!(gSR >= 8000 && gSR <= 384000)) { std::fprintf(stderr, "measure_h2_engine: --sr wants a rate in Hz (44100, 48000)\n"); return 1; }
  if (hasArg(argc, argv, "--ledger")) return ledger(argc, argv);
  if (hasArg(argc, argv, "--sweep")) return sweep(argc, argv);
  if (argc > 1 && std::strcmp(argv[1], "--hold") == 0) return hold(argc, argv);
  if (hasArg(argc, argv, "--stages")) {
#ifdef H2_ENGINE_STAGES
    return stages(argc, argv);
#else
    std::fprintf(stderr, "measure_h2_engine: --stages needs the measure_h2_engine_stages build (H2_ENGINE_STAGES)\n");
    return 1;
#endif
  }
  return replayBench(argc, argv);
}
