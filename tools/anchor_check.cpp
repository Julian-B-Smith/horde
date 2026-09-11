/* anchor_check — the wheel lane must not transpose held notes when its ANCHOR
   moves (2026-09-11, human: "playing several notes makes them glide to other
   notes at random intervals … chromatic quantise, bend law off").

   Mechanism found: the wheel lane quantises `lastNoteKey + wheel` and emits the
   offset back to EVERY voice (updateTuneAll). Its quantiser latches the last
   committed step in ABSOLUTE pitch, and the time gate refuses a new step until
   qTime has elapsed since the last COMMIT — so a second key struck inside the
   gate window kept the FIRST key's step: emitted = firstKey − secondKey, a
   whole-chord transposition held for the rest of the window. Chromatic, no
   wheel, no scale involved. (docs/design/bend-lab.html carries the same latch;
   the bench is single-note, so it never met an anchor change — L0031 again.)

   T1  no wheel input, keys struck inside the gate window: the lane emits 0 at
       every block — quant {chromatic, scale} × gate {continuous, free, sync}.
   T2  CONTROL (must read non-zero): the gate still gates the WHEEL. A fast
       wheel ramp under an 8 Hz gate commits fewer, later steps than the
       continuous path; a detector that could not see the gate would pass T1
       for the wrong reason.
   Standalone; not wired into ./verify (human gate). Exit 1 on any failure. */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"
extern "C" double hypersaw_debug_pitchbend(const clap_plugin_t *);
extern "C" int hypersaw_debug_lastnotekey(const clap_plugin_t *);
namespace {
#include "notefuzz_scaffold.inc"
struct Rig {
  const clap_plugin_t *p = nullptr; std::vector<float> L, R; clap_audio_buffer_t out{}; clap_process_t proc{}; float *ch[2];
  void boot() { auto *f = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    p = f->create_plugin(f, &kHost, "com.lifted-truck.hypersaw"); p->init(p); p->activate(p, kSR, 32, kBlock); p->start_processing(p);
    L.assign(kBlock, 0); R.assign(kBlock, 0); ch[0] = L.data(); ch[1] = R.data(); out.data32 = ch; out.channel_count = 2;
    proc.frames_count = kBlock; proc.audio_outputs = &out; proc.audio_outputs_count = 1; proc.out_events = &kOut; }
  void step(EvList &e) { e.finalize(); proc.in_events = &e.list; p->process(p, &proc); }
  void block() { EvList e; step(e); }
  void set(clap_id id, double v) { EvList e; e.params.push_back(mkParam(id, v)); step(e); }
  void note(int m, bool on, int id) { EvList e; e.notes.push_back(mkNote(on ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF, 0, (int16_t)m, id, on ? 0.8 : 0)); step(e); }
  double lane() const { return hypersaw_debug_pitchbend(p); }
  void kill() { p->stop_processing(p); p->deactivate(p); p->destroy(p); }
};
int fails = 0;
void expect(bool ok, const char *what) { std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", what); if (!ok) fails++; }
}
int main() {
  const char *qName[] = {"off", "chromatic", "scale (drag)", "scale", "scale (offset)"};
  const char *gName[] = {"continuous", "free 8 Hz", "sync /4"};
  // ---- T1: anchor moves, wheel at rest -> lane stays 0 ------------------------
  for (int q : {1, 3}) for (int g : {0, 1, 2}) {
    Rig r; r.boot(); r.set(114, q); r.set(146, g);
    for (int i = 0; i < 4; i++) r.block();   // let the lane commit its idle step first (the trap needs a prior commit)
    double worst = 0; int worstKey = 0; double worstT = 0; int blocks = 0;
    auto watch = [&](int n) { for (int i = 0; i < n; i++, blocks++) { r.block(); const double v = std::fabs(r.lane());
      if (v > worst) { worst = v; worstKey = hypersaw_debug_lastnotekey(r.p); worstT = blocks * (double)kBlock / kSR; } } };
    r.note(60, true, 1); r.note(64, true, 2); r.note(67, true, 3); watch(Math_blocks(0.5));   // a chord struck across 3 blocks: inside every gate window
    r.note(71, true, 4); watch(Math_blocks(0.05)); r.note(59, true, 5); watch(Math_blocks(0.5));   // two more inside one window
    r.note(60, false, 1); r.note(62, true, 6); watch(Math_blocks(0.3));
    char buf[160]; std::snprintf(buf, sizeof buf, "T1 quant=%-12s gate=%-10s  worst |lane| = %.3f st (anchor %d @ %.3fs)", qName[q], gName[g], worst, worstKey, worstT);
    expect(worst == 0.0, buf); r.kill();
  }
  // ---- T2 control: the gate still gates the WHEEL -----------------------------
  auto ramp = [&](int g) {
    Rig r; r.boot(); r.set(114, 1); r.set(146, g); r.set(147, 8);   // chromatic, 8 Hz -> 125 ms windows
    // The gate's clock restarts at the note-on commit, so time is counted from
    // the strike: idle blocks before the ramp are part of the window.
    int blocks = 0;   // every step is one block; the note block is block 0
    r.note(60, true, 1); blocks++; for (int i = 0; i < 4; i++) { r.block(); blocks++; }
    int flips = 0; double last = r.lane(), firstFlipT = -1; const int n = Math_blocks(0.10);   // +2 st in 100 ms
    for (int i = 1; i <= n + Math_blocks(0.4); i++) { r.set(38, 2.0 * std::min(i, n) / n); blocks++;
      const double v = r.lane(); if (v != last) { flips++; if (firstFlipT < 0) firstFlipT = blocks * (double)kBlock / kSR; last = v; } }
    r.kill(); std::printf("  T2 gate=%-10s flips=%d first commit @ %.3fs final=%+.1f st\n", gName[g], flips, firstFlipT, last);
    return std::pair<int, double>{flips, firstFlipT}; };
  const auto c = ramp(0), f = ramp(1);
  expect(c.first == 2 && f.first == 1, "T2 control: gated ramp commits ONE step where continuous commits two (gate alive)");
  expect(f.second >= 0.125 && c.second < 0.08, "T2 control: gated first commit waits out the window, continuous does not");
  std::printf("anchor_check: %s\n", fails ? "FAIL" : "PASS");
  hypersaw_entry_deinit(); return fails ? 1 : 0;
}
