/* penv_check — ENV 2 (the pitch envelope, ADR-135) restarts its attack on
   EVERY strike, not only on the first key after silence (ADR-161; human
   2026-09-13: with one note held, "consecutive notes bring the pitch peak
   closer and closer to the destination until there's no longer a noticeable
   spike" — the shrinking spike was the first note's decay tail).
   T1  one note held, six further strikes: each strike's peak env2 >= 0.99.
   T2  CONTROL (must decay): between strikes env2 falls below 0.6 — the
       envelope is not merely pinned at 1, so T1's peaks are attacks.
   T3  CONTROL (must not fire): a held note with no strike never makes the
       envelope RISE — it may only decay.
   Standalone; not wired into ./verify (human gate). Exit 1 on failure. */
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
extern "C" void hypersaw_debug_penv(const clap_plugin_t *, double *, double *, double *);
extern "C" void hypersaw_debug_voices(const clap_plugin_t *, char *, uint32_t);
namespace {
#include "notefuzz_scaffold.inc"
struct Rig {
  const clap_plugin_t *p = nullptr; std::vector<float> L, R; clap_audio_buffer_t out{}; clap_process_t proc{}; float *ch[2]; long blocks = 0;
  void boot() { auto *f = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    p = f->create_plugin(f, &kHost, "com.lifted-truck.hypersaw"); p->init(p); p->activate(p, kSR, 32, kBlock); p->start_processing(p);
    L.assign(kBlock, 0); R.assign(kBlock, 0); ch[0] = L.data(); ch[1] = R.data(); out.data32 = ch; out.channel_count = 2;
    proc.frames_count = kBlock; proc.audio_outputs = &out; proc.audio_outputs_count = 1; proc.out_events = &kOut; }
  void step(EvList &e) { e.finalize(); proc.in_events = &e.list; p->process(p, &proc); blocks++; }
  void set(clap_id id, double v) { EvList e; e.params.push_back(mkParam(id, v)); step(e); }
  void note(int m, bool on, int id) { EvList e; e.notes.push_back(mkNote(on ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF, 0, (int16_t)m, id, on ? 0.8 : 0)); step(e); }
  double t() const { return blocks * (double)kBlock / kSR; }
  void kill() { p->stop_processing(p); p->deactivate(p); p->destroy(p); }
};
}
int main() {
  Rig r; r.boot(); r.set(161, 12);
  for (int i = 0; i < 20; i++) { EvList e; r.step(e); }
  int fails = 0; auto expect = [&](bool ok, const char *w) { std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", w); if (!ok) fails++; };
  auto watch = [&](double sec) { double pk = -1e9; int n = Math_blocks(sec);
    for (int i = 0; i < n; i++) { EvList e; r.step(e); double a, st, ps; hypersaw_debug_penv(r.p, &a, &st, &ps); pk = std::max(pk, a); } return pk; };
  r.note(60, true, 1); watch(0.5);
  double minStrike = 1e9, maxGap = -1e9;
  for (int k = 0; k < 6; k++) { r.note(64, true, 10 + k); const double pk = watch(0.12); minStrike = std::min(minStrike, pk);
    r.note(64, false, 10 + k); const double g = watch(0.24); maxGap = std::max(maxGap, g); std::printf("     strike %d: peak %.3f  gap peak %.3f\n", k + 1, pk, g); }
  char b[160];
  std::snprintf(b, sizeof b, "T1 every strike with a note held reaches the attack peak (min %.3f >= 0.99)", minStrike); expect(minStrike >= 0.99, b);
  std::snprintf(b, sizeof b, "T2 control: between strikes the envelope decays (max gap peak %.3f < 0.6)", maxGap); expect(maxGap < 0.6, b);
  // T3: with 60 still held and nothing struck, the envelope may only fall.
  double rise = 0, prev; { double st, ps; hypersaw_debug_penv(r.p, &prev, &st, &ps); }
  for (int i = 0; i < Math_blocks(0.5); i++) { EvList e; r.step(e); double a, st, ps; hypersaw_debug_penv(r.p, &a, &st, &ps); rise = std::max(rise, a - prev); prev = a; }
  std::snprintf(b, sizeof b, "T3 control: a held note alone never retriggers (largest per-block rise %.4f == 0)", rise); expect(rise <= 0.0, b);
  std::printf("penv_check: %s\n", fails ? "FAIL" : "PASS");
  r.kill(); hypersaw_entry_deinit(); return fails ? 1 : 0;
}
