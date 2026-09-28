/*
 * measure_h2_scalpel — CPU cost of horde 2's SCALPEL blade engine
 * (h2/cores/scalpel/razor_core.h) per voice, as a fraction of real time, against
 * the JS oracle on the same scripts. ROADMAP B332 phase 1a; ADR-187 item 8:
 * CPU is Layer-E, measured by hand on a named machine, never a gate.
 *
 * UNWIRED: a timing measurement, not a pass/fail gate (the measure_cpu /
 * measure_modsources contract) — it prints, never judges, and carries no
 * threshold to weaken. Outside tools/test_table_check.py's census by name.
 *
 * WHAT IT TIMES. `node tools/h2_scalpel_render.mjs --bench --emit` writes two
 * scripts — the heavy class the brief names (Crushed bells: N 6 plus two blades)
 * and a light one (Quarter sync: N 1, one blade), one voice held for 4 s at
 * 48 kHz, 2x oversampling (the oracle's default) — which this binary replays
 * through the core, best of five, and divides by the audio duration. The JS half
 * is `node tools/h2_scalpel_render.mjs --bench` (best of three after a JIT
 * warm-up). A calibration loop (1e8 dependent multiply-adds) is timed on both
 * sides so a slow machine can be told from a slow engine (B236's pattern).
 *
 * THIS IS THE RELEASE BUILD, NOT THE PARITY BUILD: -O3 with the compiler's
 * default floating-point contraction, a separate target from
 * h2_scalpel_parity_check (-O2 -ffp-contract=off). It also prints its own
 * distance from the oracle, so the reader sees what the shipped flags cost in
 * parity (the FMA fault, measured rather than assumed).
 *
 * Usage (repo root):  build-release/measure_h2_scalpel [stream-file]
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

#include "../h2/cores/scalpel/razor_core.h"
#include "h2_scalpel_stream.h"

int main(int argc, char** argv) {
  FILE* f = nullptr;
  bool piped = false;
  if (argc > 1) f = std::fopen(argv[1], "rb");
  else {
#ifdef _WIN32
    f = popen("node tools/h2_scalpel_render.mjs --bench --emit", "rb");
#else
    f = popen("node tools/h2_scalpel_render.mjs --bench --emit", "r");
#endif
    piped = true;
  }
  if (!f) { std::fprintf(stderr, "measure_h2_scalpel: cannot open the bench stream (run from the repo root)\n"); return 1; }

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
  h2stream::readLine(f, line);   // header
  std::vector<double> out;
  while (h2stream::readLine(f, line)) {
    if (line.rfind("SCN ", 0) != 0) { if (line.rfind("END", 0) == 0) break; continue; }
    h2stream::Scenario sc;
    if (!h2stream::readScenario(f, line, sc)) { std::fprintf(stderr, "measure_h2_scalpel: truncated stream\n"); return 1; }
    double audio = 0;
    for (const h2stream::Cmd& c : sc.cmds) if (c.op == "render") audio += c.a * c.b;
    audio /= sc.sr;
    double best = 1e30;
    for (int r = 0; r < 5; r++) {
      const auto t0 = std::chrono::steady_clock::now();
      h2stream::replay(sc, out);
      const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      if (s < best) best = s;
    }
    double e = 0, mx = 0;
    for (size_t i = 0; i < out.size() && i < sc.js.size(); i++) { const double d = std::fabs(out[i] - sc.js[i]); e += d * d; if (d > mx) mx = d; }
    std::printf("%-22s C++ %6.3f %% of real time per voice (%.4f s for %.2f s, best of 5); vs oracle rms %.3e max %.3e\n",
                sc.name.c_str(), 100 * best / audio, best, audio, std::sqrt(e / static_cast<double>(out.size())), mx);
  }
  if (piped) pclose(f); else std::fclose(f);
  return 0;
}
