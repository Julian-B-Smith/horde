/* pitch_probe — does a NEW note sound at its MIDI pitch? (2026-09-11 report:
   "every new note picks a random note in a specific scale even when the
   scale is set to chromatic"). Drives the shipped plugin through the CLAP
   factory, renders one note at a time, measures the fundamental by
   autocorrelation over the last 0.5 s, and prints played vs heard. Diagnostic. */
#include <cmath>
#include <cstdio>
#include <vector>
#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"
namespace {
#include "notefuzz_scaffold.inc"
struct Probe {
  const clap_plugin_t *p = nullptr; std::vector<float> L, R; clap_audio_buffer_t out{}; clap_process_t proc{}; float *ch[2];
  void boot() { auto *f = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    p = f->create_plugin(f, &kHost, "com.lifted-truck.hypersaw"); p->init(p); p->activate(p, kSR, 32, kBlock); p->start_processing(p);
    L.assign(kBlock, 0); R.assign(kBlock, 0); ch[0] = L.data(); ch[1] = R.data(); out.data32 = ch; out.channel_count = 2;
    proc.frames_count = kBlock; proc.audio_outputs = &out; proc.audio_outputs_count = 1; proc.out_events = &kOut; }
  void step(EvList &e) { e.finalize(); proc.in_events = &e.list; p->process(p, &proc); }
  void set(const std::vector<std::pair<clap_id, double>> &kv) { EvList e; for (auto &x : kv) e.params.push_back(mkParam(x.first, x.second)); step(e); }
  double heardMidi(int midi, double seconds = 1.5) {
    { EvList e; e.notes.push_back(mkNote(CLAP_EVENT_NOTE_ON, 0, (int16_t)midi, 1, 0.9)); step(e); }
    const int blocks = (int)(seconds * kSR) / kBlock; std::vector<float> mono;
    for (int i = 0; i < blocks; i++) { EvList q; step(q); if (i >= blocks * 2 / 3) for (int j = 0; j < kBlock; j++) mono.push_back(0.5f * (L[j] + R[j])); }
    { EvList e; e.notes.push_back(mkNote(CLAP_EVENT_NOTE_OFF, 0, (int16_t)midi, 1, 0)); step(e); }
    for (int i = 0; i < (int)(0.6 * kSR) / kBlock; i++) { EvList q; step(q); }   // let the release die
    // autocorrelation peak between 30 Hz and 4 kHz
    const int n = (int)mono.size(); int lo = (int)(kSR / 4000), hi = (int)(kSR / 30); double best = -1; int bl = lo;
    for (int lag = lo; lag < hi && lag < n / 2; lag++) { double s = 0; for (int i = 0; i + lag < n; i++) s += mono[i] * mono[i + lag]; if (s > best) { best = s; bl = lag; } }
    // refine around the first strong peak (avoid octave-below aliasing): take the smallest lag within 5% of best
    double thr = best * 0.95; int first = bl; for (int lag = lo; lag <= bl; lag++) { double s = 0; for (int i = 0; i + lag < n; i++) s += mono[i] * mono[i + lag]; if (s >= thr) { first = lag; break; } }
    const double hz = kSR / (double)first; return 69.0 + 12.0 * std::log2(hz / 440.0);
  }
  void kill() { p->stop_processing(p); p->deactivate(p); p->destroy(p); }
};
void run(const char *label, const std::vector<std::pair<clap_id, double>> &setup) {
  Probe pr; pr.boot(); pr.set(setup);
  std::printf("%-58s", label);
  for (int m : {60, 61, 63, 66, 68, 70}) { const double h = pr.heardMidi(m); std::printf(" %d->%.1f%s", m, h, std::fabs(h - m) > 0.35 ? "!" : ""); }
  std::printf("\n"); pr.kill();
}
}  // namespace
int main() {
  std::vector<std::pair<clap_id, double>> chromatic; for (clap_id d = 117; d <= 128; d++) chromatic.push_back({d, 1});
  std::printf("pitch_probe — played -> heard (MIDI); '!' = off by more than a third of a semitone\n");
  run("defaults", {});
  run("bendQuant=scale(3), link=follow (default)", {{114, 3}});
  auto c1 = chromatic; c1.insert(c1.begin(), {114, 3});
  run("bendQuant=scale(3), then Scale section -> chromatic", c1);
  auto c2 = chromatic; c2.insert(c2.begin(), {114, 3}); c2.push_back({116, 0});   // touch root too
  run("... chromatic + scaleRoot re-set (does a root write push?)", c2);
  run("bendQuant=scale(drag)(2), default scale", {{114, 2}});
  run("bendQuant=chromatic(1)", {{114, 1}});
  run("noteLink=own(0), noteQuant=scale(2), default scale", {{137, 0}, {144, 2}});
  auto c3 = chromatic; c3.insert(c3.begin(), {137, 0}); c3.insert(c3.begin(), {144, 2});
  run("noteLink=own, noteQuant=scale, Scale -> chromatic", c3);
  auto c4 = chromatic; c4.insert(c4.begin(), {114, 2});
  run("bendQuant=scale(drag)(2), Scale -> chromatic (morph OFF)", c4);
  // morph ON with un-authored corners: every corner holds the DEFAULT scale
  // (C major); the field re-applies it. Scale set to chromatic AFTER morph on.
  auto c5 = chromatic; c5.insert(c5.begin(), {114, 2}); c5.insert(c5.begin(), {151, 1});
  run("MORPH ON, bendQuant=scale(drag), then Scale -> chromatic", c5);
  auto c6 = chromatic; c6.insert(c6.begin(), {114, 3}); c6.insert(c6.begin(), {151, 1});
  run("MORPH ON, bendQuant=scale(3 anchored), Scale -> chromatic", c6);
  auto c7 = chromatic; c7.insert(c7.begin(), {114, 1}); c7.insert(c7.begin(), {151, 1});
  run("MORPH ON, bendQuant=chromatic(1), Scale -> chromatic", c7);
  hypersaw_entry_deinit(); return 0;
}
