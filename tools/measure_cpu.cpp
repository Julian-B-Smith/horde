/*
 * measure_cpu — CPU per voice count for docs/MEASUREMENTS.md (ROADMAP B103).
 *
 * The SHIPPED plugin through the CLAP factory (cpu_bench times SwarmCore
 * alone and missed the shell's cost — see shell_bench.cpp's header), against
 * the E-6 envelope: 44.1 kHz, 128-sample buffer. Eight held notes (keys
 * 48 + 3k, the shell_bench chord), swarm size n = 1 / 4 / 8 / 16 / 32 per
 * note (param 1 "Voices", and its osc-2 twin 1001 under the ADR-082 stride),
 * one oscillator and two (osc 2 needs BOTH enable 1150 and volume 1017 —
 * ADR-100 ships it OFF, so a volume write alone still measures one). 5 s of
 * audio per cell after a 200-block warm-up; each cell is timed three times
 * and the MINIMUM is reported (the others are scheduler noise, not the
 * plugin). Reports ms of CPU per second of audio and % of one core.
 *
 * The RENDER is deterministic (seeded streams, fixed note order, no clock
 * inside process()); only the stopwatch around it is wall-clock, which is
 * what makes this a measurement rather than a gate. Release build only — a
 * -O0 number is meaningless (global CLAUDE.md, "Test in Release").
 * Standalone, registered in CMake beside svf_check, NOT in ./verify.
 */
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>
#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"
#include "build_stamp.h"   // generated every build (CMake target)

namespace
{
#include "notefuzz_scaffold.inc"

constexpr double kRate = 44100.0;
constexpr uint32_t kBlk = 128;     // the E-6 envelope's buffer
constexpr double kSeconds = 5.0;
constexpr int kNotes = 8;
constexpr int kReps = 3;

struct Cell { double msPerS, pct; double readN2, readEn2, readVol2; };

Cell timeCell(const clap_plugin_factory_t *factory, int n, bool twoOsc)
{
  double best = 1e9; double rn = -1, re = -1, rv = -1;
  for (int rep = 0; rep < kReps; rep++)
  {
    const clap_plugin_t *p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
    p->init(p); p->activate(p, kRate, 32, kBlk); p->start_processing(p);
    std::vector<float> L(kBlk), R(kBlk);
    float *chans[2] = {L.data(), R.data()};
    clap_audio_buffer_t out{}; out.data32 = chans; out.channel_count = 2;
    clap_process_t proc{}; proc.frames_count = kBlk; proc.audio_outputs = &out;
    proc.audio_outputs_count = 1; proc.out_events = &kOut;
    auto once = [&](EvList &e) { e.finalize(); proc.in_events = &e.list; p->process(p, &proc); };
    {
      EvList e;
      e.params.push_back(mkParam(1, n));
      e.params.push_back(mkParam(1001, n));
      e.params.push_back(mkParam(1150, twoOsc ? 1 : 0));
      e.params.push_back(mkParam(1017, twoOsc ? 0.4 : 0));
      once(e);
    }
    // Read back what the shell holds, so a wrong id assumption shows in the
    // table instead of silently timing the default (L0032).
    auto *ext = (const clap_plugin_params_t *)p->get_extension(p, CLAP_EXT_PARAMS);
    if (ext) { ext->get_value(p, 1001, &rn); ext->get_value(p, 1150, &re); ext->get_value(p, 1017, &rv); }
    for (int k = 0; k < kNotes; k++)
    { EvList e; e.notes.push_back(mkNote(CLAP_EVENT_NOTE_ON, 0, (int16_t)(48 + k * 3), k + 1)); once(e); }
    for (int b = 0; b < 200; b++) { EvList e; once(e); }   // warm-up, untimed
    const int NB = (int)(kSeconds * kRate / kBlk);
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < NB; b++) { EvList e; once(e); }
    const auto t1 = std::chrono::steady_clock::now();
    const double cpu = std::chrono::duration<double>(t1 - t0).count();
    const double audio = NB * (double)kBlk / kRate;
    best = std::min(best, cpu / audio);
    p->stop_processing(p); p->deactivate(p); p->destroy(p);
  }
  return {best * 1000.0, best * 100.0, rn, re, rv};
}
}  // namespace

int main()
{
  hypersaw_entry_init("");
  auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const int ns[] = {1, 4, 8, 16, 32};
  std::printf("| voices per note (n) | 1 osc: ms CPU / s audio | 1 osc: %% of a core | 2 osc: ms CPU / s audio | 2 osc: %% of a core | readback (osc2 n / enable / vol) |\n");
  std::printf("|---|---|---|---|---|---|\n");
  for (int n : ns)
  {
    const Cell a = timeCell(factory, n, false);
    const Cell b = timeCell(factory, n, true);
    std::printf("| %d | %.1f | %.2f%% | %.1f | %.2f%% | %.0f / %.0f / %.1f |\n",
                n, a.msPerS, a.pct, b.msPerS, b.pct, b.readN2, b.readEn2, b.readVol2);
  }
  std::printf("\nBuild %s. %d held notes (keys 48+3k), %.0f Hz, %u-sample blocks, %.0f s per cell, min of %d runs.\n",
              HYPERSAW_BUILD_STAMP, kNotes, kRate, kBlk, kSeconds, kReps);
  hypersaw_entry_deinit();
  return 0;
}
