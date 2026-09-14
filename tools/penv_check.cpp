/* penv_check — ENV 2, the pitch envelope, is PER NOTE (ADR-162; human
   2026-09-13 "Pitch envelope should be per-note"). It began as the ADR-161
   oracle for a shared envelope that restarted on every strike, which fixed the
   reported bug ("consecutive notes bring the pitch peak closer and closer to
   the destination until there's no longer a noticeable spike" — the shrinking
   spike was the first note's decay tail) and left a second one behind: with
   ONE envelope, a strike blipped every note already held.

   T1  one note held, six further strikes: each strike's peak env2 >= 0.99.
   T2  CONTROL (must decay): between strikes env2 falls below 0.6 — the
       envelope is not merely pinned at 1, so T1's peaks are attacks.
   T3  CONTROL (must not fire): a held note with no strike never makes the
       envelope RISE — it may only decay.
   T4  PER-NOTE: hold 60 until its envelope has settled, then strike 64. 64's
       noteTune rises to the peak (2.0 at depth +12) while 60's stays 1.0.
       Under the shared envelope 60's noteTune rose to the peak WITH it — the
       calibration point, and the 1e-3 band is three orders off that 1.0 blip.
   T5  PER-NOTE RELEASE: release 64 with 60 still held. 64's envelope releases
       to ~0; 60's neither rises nor moves its noteTune.
   T6  SOURCE SLOT + MUST-READ-ZERO CONTROL: with the pitch depth at 0 the
       global source (slot 1, what an ENV 2 -> filter route reads) still peaks
       on every strike, AND no voice's noteTune moves off exactly 1.0 — the
       per-note lane writes nothing at all when the route is off, which is what
       makes depth 0 bit-identical.
   env2/stage here are the GLOBAL PROJECTION (mod source slot 1 = max over
   gated slots); hypersaw_debug_penv_slot reads one slot's own envelope.
   Standalone; not wired into ./verify (human gate). Exit 1 on failure. */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"
extern "C" bool hypersaw_debug_apply(const clap_plugin_t *, const char *);
extern "C" void hypersaw_debug_penv(const clap_plugin_t *, double *, double *, double *);
extern "C" void hypersaw_debug_penv_slot(const clap_plugin_t *, int, double *, double *, double *);
extern "C" void hypersaw_debug_voices(const clap_plugin_t *, char *, uint32_t);
namespace {
#include "notefuzz_scaffold.inc"
// One row of hypersaw_debug_voices: slot,midi,gate,f0,f0cur,glide,noteTune.
struct VoiceRow { int slot = -1, midi = -1, gate = 0, glide = 0; double f0 = 0, f0cur = 0, tune = 1; };
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
  std::vector<VoiceRow> voices() const {
    char b[512]; hypersaw_debug_voices(p, b, sizeof b); std::vector<VoiceRow> v;
    for (const char *s = b; *s;) { VoiceRow r;
      if (std::sscanf(s, "%d,%d,%d,%lf,%lf,%d,%lf;", &r.slot, &r.midi, &r.gate, &r.f0, &r.f0cur, &r.glide, &r.tune) == 7) v.push_back(r);
      const char *nx = std::strchr(s, ';'); if (!nx) break; s = nx + 1; }
    return v; }
  // -1 when that key has no sounding voice (a released tail eventually leaves
  // the table) — callers must distinguish "gone" from "at rest".
  double tuneOf(int midi) const { for (const auto &v : voices()) if (v.midi == midi) return v.tune; return -1; }
  int slotOf(int midi) const { for (const auto &v : voices()) if (v.midi == midi) return v.slot; return -1; }
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
  char b[200];
  std::snprintf(b, sizeof b, "T1 every strike with a note held reaches the attack peak (min %.3f >= 0.99)", minStrike); expect(minStrike >= 0.99, b);
  std::snprintf(b, sizeof b, "T2 control: between strikes the envelope decays (max gap peak %.3f < 0.6)", maxGap); expect(maxGap < 0.6, b);
  // T3: with 60 still held and nothing struck, the envelope may only fall.
  double rise = 0, prev; { double st, ps; hypersaw_debug_penv(r.p, &prev, &st, &ps); }
  for (int i = 0; i < Math_blocks(0.5); i++) { EvList e; r.step(e); double a, st, ps; hypersaw_debug_penv(r.p, &a, &st, &ps); rise = std::max(rise, a - prev); prev = a; }
  std::snprintf(b, sizeof b, "T3 control: a held note alone never retriggers (largest per-block rise %.4f == 0)", rise); expect(rise <= 0.0, b);

  /* T4/T5 — the ADR-162 claim. Depth is +12 st, so a voice at the envelope
     peak reads noteTune 2.0 and a voice at rest reads exactly 1.0.

     MEASURE THE TOTAL OFFSET, NOT ONE LANE. The first draft of T4 asserted the
     held note's noteTune stayed 1.0 and PASSED against the shared envelope —
     vacuously, because the shared blip was applied through the GLOBAL pitch
     lane (modPitchSm -> updateTuneAll), where noteTune cannot see it. A probe
     that reads only the lane you built is a detector sharing its subject's
     assumption (L0032). What the player hears is the sum, so that is what is
     asserted: pitchSm + 12*log2(noteTune). Calibrated — against the
     shared-envelope build T4-total and T4-global both FAIL at +12 st. */
  auto semisOf = [&](int midi) { const double t = r.tuneOf(midi);
    if (t <= 0) return -1e9;   // no sounding voice for that key
    double a, st, ps; hypersaw_debug_penv(r.p, &a, &st, &ps);
    return ps + 12.0 * std::log2(t); };
  watch(1.5);   // let 60's own envelope settle (decay tau 0.16 s, sustain 0)
  const double t60Settled = semisOf(60);
  std::snprintf(b, sizeof b, "T4a the held note has settled back to base pitch (%.6f st, |.| < 1e-3)", t60Settled);
  expect(std::fabs(t60Settled) < 1e-3, b);
  r.note(64, true, 100);
  double t64Peak = -1e9, t64Tune = -1e9, t60Dev = 0, globalPeak = 0;
  // A "0.000000 deviation" that came from never FINDING the held voice reads
  // exactly like a pass (L0032). Count the observations and assert them.
  int seen60 = 0;
  for (int i = 0; i < Math_blocks(0.12); i++) { EvList e; r.step(e);
    double a, st, ps; hypersaw_debug_penv(r.p, &a, &st, &ps);
    globalPeak = std::max(globalPeak, std::fabs(ps));
    const double sa = semisOf(64), sh = semisOf(60), ta = r.tuneOf(64);
    if (ta > 0) t64Tune = std::max(t64Tune, ta);
    if (sa > -1e8) t64Peak = std::max(t64Peak, sa);
    if (sh > -1e8) { seen60++; t60Dev = std::max(t60Dev, std::fabs(sh)); } }
  std::snprintf(b, sizeof b, "T4 anchor: the held voice was actually observed (%d blocks of %d)", seen60, Math_blocks(0.12));
  expect(seen60 == Math_blocks(0.12), b);
  std::snprintf(b, sizeof b, "T4 the STRUCK note bends to the peak (%.4f st >= 11.9 at depth +12)", t64Peak);
  expect(t64Peak >= 11.9, b);
  std::snprintf(b, sizeof b, "T4 and it rides the PER-NOTE lane (its own noteTune %.4f >= 1.99)", t64Tune);
  expect(t64Tune >= 1.99, b);
  std::snprintf(b, sizeof b, "T4 the HELD note does not blip (max total offset %.6f st < 1e-3; shared env gave +12)", t60Dev);
  expect(t60Dev < 1e-3, b);
  std::snprintf(b, sizeof b, "T4 route 0 left the GLOBAL lane (max |modPitchSm| %.3e st == 0)", globalPeak);
  expect(globalPeak == 0.0, b);

  const int slot64 = r.slotOf(64), slot60 = r.slotOf(60);
  double l60Pre, s60Pre, x60Pre; hypersaw_debug_penv_slot(r.p, slot60, &l60Pre, &s60Pre, &x60Pre);
  r.note(64, false, 100);
  double l64 = 1, lv, sv, xv, rise60 = -1e9, dev60 = 0, prev60 = l60Pre;
  int seen60b = 0;
  for (int i = 0; i < Math_blocks(0.8); i++) { EvList e; r.step(e);
    hypersaw_debug_penv_slot(r.p, slot64, &l64, &sv, &xv);
    hypersaw_debug_penv_slot(r.p, slot60, &lv, &sv, &xv);
    rise60 = std::max(rise60, lv - prev60); prev60 = lv;
    const double h = semisOf(60); if (h > -1e8) { seen60b++; dev60 = std::max(dev60, std::fabs(h)); } }
  std::snprintf(b, sizeof b, "T5 anchor: the held voice was actually observed (%d blocks of %d)", seen60b, Math_blocks(0.8));
  expect(seen60b == Math_blocks(0.8), b);
  std::snprintf(b, sizeof b, "T5 the RELEASED note's own envelope releases (slot %d level %.5f < 0.01)", slot64, l64);
  expect(slot64 >= 0 && l64 < 0.01, b);
  std::snprintf(b, sizeof b, "T5 control: the still-held note is untouched (largest rise %.2e <= 0, max total offset %.6f st < 1e-3)", rise60, dev60);
  expect(slot60 >= 0 && rise60 <= 0.0 && dev60 < 1e-3, b);

  /* T6 — the OTHER half of ENV 2: the global source slot every non-pitch route
     reads. Depth 0 means the per-note lane must write NOTHING (the reason
     depth 0 is bit-identical), so noteTune reading exactly 1.0 is the
     must-read-zero control, not a bonus assertion. */
  r.note(60, false, 1); watch(1.2); r.set(161, 0); watch(0.2);
  double srcMin = 1e9, tuneDev = 0; int seenT6 = 0;
  for (int k = 0; k < 3; k++) {
    r.note(67, true, 200 + k); double pk = -1e9;
    for (int i = 0; i < Math_blocks(0.12); i++) { EvList e; r.step(e);
      double a, st, ps; hypersaw_debug_penv(r.p, &a, &st, &ps); pk = std::max(pk, a);
      for (const auto &v : r.voices()) { seenT6++; tuneDev = std::max(tuneDev, std::fabs(v.tune - 1.0)); } }
    r.note(67, false, 200 + k); watch(0.3); srcMin = std::min(srcMin, pk);
    std::printf("     depth-0 strike %d: source slot 1 peak %.3f\n", k + 1, pk); }
  std::snprintf(b, sizeof b, "T6 mod source slot 1 still peaks on every strike (min %.3f >= 0.99)", srcMin);
  expect(srcMin >= 0.99, b);
  std::snprintf(b, sizeof b, "T6 control: at depth 0 no voice's noteTune moves (max |noteTune-1| %.3e == 0 over %d voice reads)", tuneDev, seenT6);
  expect(tuneDev == 0.0 && seenT6 > 0, b);

  /* T7 — the global source RELEASES at last-key-up (lead ruling 2026-09-13):
     it must decay over env2R, never snap to 0 in one block. Sustain is raised
     first so the leg cannot pass vacuously: at the default sustain 0 a settled
     note already reads 0 and a snap is invisible. */
  r.set(164, 0.6); r.set(165, 0.4);
  r.note(60, true, 300); watch(0.6);
  double before; { double st, ps; hypersaw_debug_penv(r.p, &before, &st, &ps); }
  r.note(60, false, 300);
  double after0, maxDrop = 0, prevL = before; int rises = 0;
  { double st, ps; hypersaw_debug_penv(r.p, &after0, &st, &ps); }
  for (int i = 0; i < Math_blocks(0.3); i++) { EvList e; r.step(e); double a, st, ps; hypersaw_debug_penv(r.p, &a, &st, &ps);
    maxDrop = std::max(maxDrop, prevL - a); if (a > prevL + 1e-12) rises++; prevL = a; }
  std::snprintf(b, sizeof b, "T7 anchor: the note was sustaining above 0 before key-up (%.3f >= 0.5)", before); expect(before >= 0.5, b);
  std::snprintf(b, sizeof b, "T7 source slot 1 releases over env2R at last-key-up (largest one-block drop %.4f < 0.1 of %.3f; still %.3f after 0.3 s of a 0.4 s release)", maxDrop, before, prevL);
  expect(maxDrop < 0.1 * before && prevL > 0.05, b);
  std::snprintf(b, sizeof b, "T7 control: the release never rises (%d rises == 0)", rises); expect(rises == 0, b);

  std::printf("penv_check: %s\n", fails ? "FAIL" : "PASS");
  r.kill(); hypersaw_entry_deinit(); return fails ? 1 : 0;
}
