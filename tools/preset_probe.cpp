/* preset_probe — load a saved preset JSON into the headless plugin and run the
   two symptoms reported 2026-09-11: (A) "a second note plays the FIRST note
   again": hold 60, strike 64, release 60, measure what the remaining voice
   sounds at; (B) chord: the wheel lane's emitted value per block. Works on any
   commit since 2026-08-21 (hypersaw_debug_apply); the per-voice / lane exports
   are plain externs (they were weak for the 2026-09-11 bisect; MSVC has no
   __attribute__, and the exports now exist on every branch that builds this). Usage:
   preset_probe [preset.json] */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"
extern "C" bool hypersaw_debug_apply(const clap_plugin_t *, const char *);
extern "C" const char *hypersaw_debug_cornervals(const clap_plugin_t *, int);
extern "C" double hypersaw_debug_pitchbend(const clap_plugin_t *);
extern "C" int hypersaw_debug_lastnotekey(const clap_plugin_t *);
extern "C" void hypersaw_debug_voices(const clap_plugin_t *, char *, uint32_t);
extern "C" void hypersaw_debug_notelaw(const clap_plugin_t *, char *, uint32_t);
namespace {
#include "notefuzz_scaffold.inc"
struct Rig {
  const clap_plugin_t *p = nullptr; std::vector<float> L, R; clap_audio_buffer_t out{}; clap_process_t proc{}; float *ch[2];
  std::vector<float> mono; long blocks = 0;
  void boot() { auto *f = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    p = f->create_plugin(f, &kHost, "com.lifted-truck.hypersaw"); p->init(p); p->activate(p, kSR, 32, kBlock); p->start_processing(p);
    L.assign(kBlock, 0); R.assign(kBlock, 0); ch[0] = L.data(); ch[1] = R.data(); out.data32 = ch; out.channel_count = 2;
    proc.frames_count = kBlock; proc.audio_outputs = &out; proc.audio_outputs_count = 1; proc.out_events = &kOut; }
  void step(EvList &e) { e.finalize(); proc.in_events = &e.list; p->process(p, &proc); blocks++; for (int j = 0; j < kBlock; j++) mono.push_back(0.5f * (L[j] + R[j])); }
  void run(double s) { for (int i = 0; i < Math_blocks(s); i++) { EvList e; step(e); } }
  void note(int m, bool on, int id) { EvList e; e.notes.push_back(mkNote(on ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF, 0, (int16_t)m, id, on ? 0.8 : 0)); step(e); }
  double t() const { return blocks * (double)kBlock / kSR; }
  void voices(const char *tag) { char b[512]; hypersaw_debug_voices(p, b, sizeof b); std::printf("    %-8s t=%.3f voices[slot,midi,gate,f0,f0cur,glide]: %s\n", tag, t(), b); }
  // autocorrelation pitch of the last `sec` seconds of mono, in MIDI (fractional)
  double pitchMidi(double sec) const { const int win = (int)(sec * kSR); if ((int)mono.size() < win) return 0; const size_t s0 = mono.size() - win;
    int lo = (int)(kSR / 2000), hi = (int)(kSR / 40); double best = -1; int bl = lo;
    for (int lag = lo; lag < hi; lag++) { double a = 0; for (int i = 0; i + lag < win; i++) a += mono[s0 + i] * mono[s0 + i + lag]; if (a > best) { best = a; bl = lag; } }
    // refine: prefer the shortest lag whose peak is within 5 % of the best (octave errors)
    for (int lag = lo; lag < bl; lag++) { double a = 0; for (int i = 0; i + lag < win; i++) a += mono[s0 + i] * mono[s0 + i + lag]; if (a > 0.95 * best) { bl = lag; break; } }
    return 69 + 12 * std::log2((kSR / bl) / 440.0); }
  void kill() { p->stop_processing(p); p->deactivate(p); p->destroy(p); }
};
}
int main(int argc, char **argv) {
  std::string json;
  if (argc > 1) { std::ifstream f(argv[1]); std::stringstream ss; ss << f.rdbuf(); json = ss.str(); }
  std::printf("preset: %s\n", argc > 1 ? argv[1] : "(fresh defaults)");
  int bad = 0;
  // ---- A: second note while the first is held --------------------------------
  { Rig r; r.boot(); if (!json.empty()) { if (!hypersaw_debug_apply(r.p, json.c_str())) std::printf("  apply FAILED\n"); r.run(0.2); }
    { const char *c = hypersaw_debug_cornervals(r.p, 0); int n = 0; for (const char *q = c; *q; q++) n += (*q == ','); std::printf("    live corner 0: %d entries; all: %s\n", n + 1, c); }
    { char b[400]; hypersaw_debug_notelaw(r.p, b, sizeof b); std::printf("    notelaw: %s\n", b); }
    r.note(60, true, 1); r.run(0.4); r.voices("held60");
    r.note(64, true, 2); r.run(0.05); r.voices("+64@50ms"); r.note(60, false, 1);
    for (int k = 1; k <= 4; k++) { r.run(0.25); char tag[24]; std::snprintf(tag, sizeof tag, "%.2fs", 0.05 + 0.25 * k); r.voices(tag);
      const double m = r.pitchMidi(0.2); std::printf("    A  heard %.2f s after striking 64: MIDI %.2f (%+.0f cents from E4)\n", 0.05 + 0.25 * k, m, (m - 64) * 100);
      if (k == 4 && std::fabs(m - 64) > 0.3) bad++; }
    r.note(64, true, 3); r.run(0.5); const double m = r.pitchMidi(0.2); std::printf("    A' 64 pressed AGAIN, 0.5 s later: MIDI %.2f\n", m); r.kill(); }
  // ---- B: chord, wheel lane readout ------------------------------------------
  { Rig r; r.boot(); if (!json.empty()) { hypersaw_debug_apply(r.p, json.c_str()); r.run(0.2); }
    double lastPb = -1e9; int lastKey = -1; int events = 0;
    auto lane = [&]() { const double pb = hypersaw_debug_pitchbend(r.p); const int k = hypersaw_debug_lastnotekey(r.p);
      if (pb != lastPb || k != lastKey) { if (events++ < 12) std::printf("    B  lane @%.3fs anchor=%d emitted=%+.3f st\n", r.t(), k, pb); if (std::fabs(pb) > 0.01) bad++; lastPb = pb; lastKey = k; } };
    r.note(60, true, 1); lane(); r.note(64, true, 2); lane(); r.note(67, true, 3); lane();
    for (int i = 0; i < Math_blocks(1.0); i++) { r.run(256.0 / kSR); lane(); }
    r.note(71, true, 4); lane(); for (int i = 0; i < Math_blocks(1.0); i++) { r.run(256.0 / kSR); lane(); }
    std::printf("    B  chord: pitch heard at 2 s (C E G B held): MIDI %.2f\n", r.pitchMidi(0.2)); r.kill(); }
  std::printf("preset_probe: %s\n", bad ? "SYMPTOM" : "clean");
  hypersaw_entry_deinit(); return bad ? 2 : 0;
}
