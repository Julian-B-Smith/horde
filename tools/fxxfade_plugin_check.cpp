/* fxxfade_plugin_check — the B117 crossfade through the PLUGIN's own parameter
   path (params 264/265, FX1 type 57), which fxxfade_check's audible legs do not
   exercise (they drive FxRack directly). Room tail ringing after a short note,
   FX1 flipped to Off. T1 atomic: silence in the very next block. T2 crossfade
   500 ms: still ringing at 100 ms, gone by 600 ms. 2026-09-14, after the human
   could not hear the toggle in the DAW — the mechanism was fine, the test
   was not; this pins the plugin path so that question never reopens.
   Standalone, unwired (human gate). Exit 1 on failure.
   UNWIRED: standing human ruling on gate scope, stated in this header and pre-dating the ADR-179 §4 inversion; not revisited in the wiring PR (B159). */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"
namespace {
#include "notefuzz_scaffold.inc"
struct Rig {
  const clap_plugin_t *p = nullptr; std::vector<float> L, R; clap_audio_buffer_t out{}; clap_process_t proc{}; float *ch[2];
  void boot() { auto *f = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    p = f->create_plugin(f, &kHost, "com.lifted-truck.hypersaw"); p->init(p); p->activate(p, kSR, 32, kBlock); p->start_processing(p);
    L.assign(kBlock, 0); R.assign(kBlock, 0); ch[0] = L.data(); ch[1] = R.data(); out.data32 = ch; out.channel_count = 2;
    proc.frames_count = kBlock; proc.audio_outputs = &out; proc.audio_outputs_count = 1; proc.out_events = &kOut; }
  double step(EvList &e) { e.finalize(); proc.in_events = &e.list; p->process(p, &proc); double a = 0; for (int j = 0; j < kBlock; j++) a += 0.5 * (L[j] * L[j] + R[j] * R[j]); return std::sqrt(a / kBlock); }
  double block() { EvList e; return step(e); }
  void set(clap_id id, double v) { EvList e; e.params.push_back(mkParam(id, v)); step(e); }
  void note(int m, bool on) { EvList e; e.notes.push_back(mkNote(on ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF, 0, (int16_t)m, 1, on ? 0.9 : 0)); step(e); }
  void kill() { p->stop_processing(p); p->deactivate(p); p->destroy(p); }
};
}
int main() {
  int fails = 0; double at100[2] = {0, 0}, at600[2] = {0, 0}, first[2] = {0, 0};
  for (int xf = 0; xf < 2; xf++) {
    Rig r; r.boot();
    r.set(264, xf); r.set(265, 500); r.set(57, 8); r.set(58, 0.85); r.set(133, 1.0); r.set(22, 0.05);   // Room, regen .85, wet, short amp release
    for (int i = 0; i < 10; i++) r.block();
    r.note(60, true); for (int i = 0; i < Math_blocks(0.25); i++) r.block(); r.note(60, false);
    double last = 0; for (int i = 0; i < Math_blocks(0.6); i++) last = r.block();   // amp env gone; only the room tail rings
    std::printf("%s: tail before flip rms=%.5f (%.1f dBFS)\n", xf ? "CROSSFADE 500 ms" : "ATOMIC          ", last, 20 * std::log10(last + 1e-12));
    r.set(57, 0);   // FX1 -> Off, the flip
    std::printf("   after flip (ms:dB) ");
    for (int i = 0; i < Math_blocks(0.7); i++) { const double v = r.block(); if (i == 0) first[xf] = v; if (i == Math_blocks(0.1)) at100[xf] = v; if (i == Math_blocks(0.6)) at600[xf] = v;
      if (i % 8 == 0) std::printf("%d:%.0f ", (int)(i * 256000.0 / kSR), 20 * std::log10(v + 1e-12)); }
    std::printf("\n"); r.kill();
  }
  auto expect = [&](bool ok, const char *w) { std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", w); if (!ok) fails++; };
  expect(first[0] == 0.0, "T1 atomic: the block after the flip is digital silence");
  expect(first[1] > 0.5 * at100[1] && at100[1] > 1e-4, "T2 crossfade: still ringing 100 ms after the flip through the plugin's param path");
  expect(at600[1] == 0.0, "T2 crossfade: gone by 600 ms (500 ms fade + shadow retired)");
  expect(at100[0] == 0.0 && at100[1] > at100[0], "T3 control: the two modes differ where they must");
  std::printf("fxxfade_plugin_check: %s\n", fails ? "FAIL" : "PASS");
  hypersaw_entry_deinit(); return fails ? 1 : 0;
}
