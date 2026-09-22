/*
 * combguard_check — a second Comb slot must be IMPOSSIBLE, not merely loud.
 *
 * The Comb's KS bank is rack-owned and shared (fx_rack.h kSlotMaxInstances);
 * two Comb slots double-write every line per block and the feedback
 * compounds past unity — the human's "blew up the audio" (2026-09-05).
 * This drives the SHIPPED rack through the CLAP factory (the route from a
 * param event to audio is the thing under test), and holds three promises:
 *   T1 refusal: with FX1 = Comb, setting FX2 = Comb leaves FX2 reading Off.
 *   T2 identity: the audio after the refused write is BIT-IDENTICAL to a
 *      single-Comb render — the refusal is absent, not merely quiet.
 *   T3 bounded: a held note through the guarded rack never exceeds a sane
 *      peak (the control for what the guard is guarding against).
 *   T4 order-independence: FX2 = Comb first, then FX1 = Comb refused too.
 * WIRED: ./verify full.
 */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"

namespace
{
#include "notefuzz_scaffold.inc"
struct Probe
{
  const clap_plugin_t *p = nullptr;
  std::vector<float> L, R;
  clap_audio_buffer_t out{};
  clap_process_t proc{};
  float *ch[2];
  void boot()
  {
    auto *f = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    p = f->create_plugin(f, &kHost, "com.lifted-truck.hypersaw");
    p->init(p); p->activate(p, kSR, 32, kBlock); p->start_processing(p);
    L.assign(kBlock, 0); R.assign(kBlock, 0); ch[0] = L.data(); ch[1] = R.data();
    out.data32 = ch; out.channel_count = 2;
    proc.frames_count = kBlock; proc.audio_outputs = &out; proc.audio_outputs_count = 1; proc.out_events = &kOut;
  }
  void step(EvList &e) { e.finalize(); proc.in_events = &e.list; p->process(p, &proc); }
  void set(const std::vector<std::pair<clap_id, double>> &kv)
  { EvList e; for (auto &x : kv) e.params.push_back(mkParam(x.first, x.second)); step(e); }
  double read(clap_id id)
  { auto *ext = (const clap_plugin_params_t *)p->get_extension(p, CLAP_EXT_PARAMS); double v = -1; if (ext) ext->get_value(p, id, &v); return v; }
  // render: note on, N blocks, capture, note off
  std::vector<float> render(int blocks, double &peak)
  {
    std::vector<float> cap; peak = 0;
    { EvList e; e.notes.push_back(mkNote(CLAP_EVENT_NOTE_ON, 0, 60, 1, 0.9)); step(e); }
    for (int i = 0; i < blocks; i++)
    {
      EvList q; step(q);
      for (int j = 0; j < kBlock; j++) { cap.push_back(L[j]); cap.push_back(R[j]); peak = std::max(peak, (double)std::fabs(L[j])); }
    }
    { EvList e; e.notes.push_back(mkNote(CLAP_EVENT_NOTE_OFF, 0, 60, 1, 0)); step(e); }
    return cap;
  }
  void kill() { p->stop_processing(p); p->deactivate(p); p->destroy(p); }
};
int fails = 0;
void ok(bool pass, const char *name, const char *detail)
{ std::printf("%s %s (%s)\n", pass ? "OK  " : "FAIL", name, detail); if (!pass) fails++; }
}  // namespace

int main()
{
  const int kBlocks = (int)(2.0 * kSR) / kBlock;
  // reference: one Comb in FX1, amount 0.6
  Probe ref; ref.boot(); ref.set({{57, 5}, {58, 0.6}});
  /* AGE-MATCHING, not padding: set() PROCESSES a block, and the refusal arm
     below needs two set() calls to attempt its second Comb. Without this empty
     second block the reference enters render() one block YOUNGER, and one block
     is enough for FX1's amount smoother to move — so T2's bit-identity claim
     was comparing two different points on the smoother ramp and failed on a
     difference the guard has nothing to do with. Found when the check was first
     run for wiring (B159): T2 was the only red row, and it went green the
     moment the two arms were the same age. */
  ref.set({});
  double refPeak = 0; auto refA = ref.render(kBlocks, refPeak); ref.kill();

  // T1 + T2: FX1 = Comb, then FX2 = Comb attempted
  Probe a; a.boot(); a.set({{57, 5}, {58, 0.6}});
  a.set({{59, 5}, {60, 0.6}});
  const double t2 = a.read(59);
  char d[96]; std::snprintf(d, sizeof d, "FX2 reads %.0f (want 0 = Off)", t2);
  ok(t2 == 0.0, "T1 second Comb refused, slot keeps its type", d);
  double aPeak = 0; auto aA = a.render(kBlocks, aPeak); a.kill();
  /* Report WHERE and BY HOW MUCH on divergence: "diverged" alone cannot tell a
     leaked Comb (gross, growing) from a smoother-phase artifact (tiny, early),
     and that ambiguity is what made the original red row hard to read. */
  double worst = 0; size_t firstDiff = aA.size();
  for (size_t i = 0; i < aA.size() && i < refA.size(); i++)
  {
    const double d = std::fabs((double)aA[i] - (double)refA[i]);
    if (d > 0 && i < firstDiff) firstDiff = i;
    worst = std::max(worst, d);
  }
  const bool identical = (aA == refA);
  if (identical) std::snprintf(d, sizeof d, "identical to the bit (%zu samples)", aA.size());
  else std::snprintf(d, sizeof d, "diverged: first at sample %zu, worst |delta| %.3e", firstDiff, worst);
  ok(identical, "T2 audio after the refusal is bit-identical to one Comb", d);
  std::snprintf(d, sizeof d, "peak %.3f (ref %.3f)", aPeak, refPeak);
  ok(aPeak < 4.0 && std::isfinite(aPeak), "T3 bounded", d);

  // T4: order independence
  Probe b; b.boot(); b.set({{59, 5}, {60, 0.6}}); b.set({{57, 5}, {58, 0.6}});
  std::snprintf(d, sizeof d, "FX1 reads %.0f (want 0), FX2 reads %.0f (want 5)", b.read(57), b.read(59));
  ok(b.read(57) == 0.0 && b.read(59) == 5.0, "T4 refusal is order-independent", d);
  // and a NON-singleton type may repeat (the cap is per type, not a blanket)
  b.set({{57, 1}, {61, 1}});
  std::snprintf(d, sizeof d, "FX1 %.0f FX3 %.0f (Drive twice is allowed)", b.read(57), b.read(61));
  ok(b.read(57) == 1.0 && b.read(61) == 1.0, "T5 non-singleton types still repeat", d);
  b.kill();
  hypersaw_entry_deinit();
  if (fails) { std::printf("combguard_check: RED (%d failure(s))\n", fails); return 1; }
  std::printf("combguard_check: GREEN (0 failures)\n");
  return 0;
}
