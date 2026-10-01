/*
 * measure_h2_engine — CPU cost of horde 2's composed engine (h2/engine/engine.h)
 * per voice, as a fraction of real time, against the golden (the composed JS) on
 * the same scripts. ROADMAP B385 checkpoint 4; ADR-187 item 8: CPU is Layer-E,
 * measured by hand on a named machine, never a gate.
 *
 * UNWIRED: a timing measurement, not a pass/fail gate (the measure_* contract): it
 * prints, never judges, and carries no threshold to weaken.
 *
 * WHAT IT TIMES. `node tools/h2_engine_render.mjs --bench --emit` writes three
 * scripts, one voice held for 4 s at 48 kHz at the patch's own oversampling: the
 * heavy class (Crushed bells, Glass horde pad) and a light one (Quarter sync).
 * This binary replays each through the engine, best of five, and divides by the
 * audio duration. The JS half is `node tools/h2_engine_render.mjs --bench` (best
 * of three after a JIT warm-up); the blade port alone is measure_h2_scalpel. A
 * calibration loop (1e8 dependent multiply-adds) is timed on both sides so a slow
 * machine can be told from a slow engine (B236's pattern). It also prints its
 * distance from the golden: this is the shipped flag set (-O3 -ffp-contract=off,
 * h2 rule 7), so it should read as the parity build does.
 *
 * Usage (repo root):  build-release/measure_h2_engine [stream-file]
 */
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

#include "../h2/engine/engine.h"
#include "h2_engine_stream.h"

int main(int argc, char** argv) {
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

  volatile double sink = 0;
  {
    double x = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100000000; i++) x = x * 1.0000001 + 1e-9;
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    sink = x;
    std::printf("calibration: 1e8 dependent multiply-adds in %.1f ms (x=%.3f)\n", ms, static_cast<double>(sink));
  }
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
      const auto t0 = std::chrono::steady_clock::now();
      h2engine_stream::replay(sc, out);
      const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
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
