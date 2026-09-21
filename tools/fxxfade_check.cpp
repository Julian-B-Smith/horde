/*
 * fxxfade_check — B117 / ADR-163: the FX presence crossfade, held to its four
 * promises. combguard_check and fxxfade_plugin_check sit beside it.
 * WIRED: ./verify full.
 *
 * WHAT IS UNDER TEST. Module TYPE is stepped, so under both morph modes a
 * slot's module flips atomically (B49): the outgoing tail is cut, the incoming
 * module enters with no lead-in. `fxXfade = 1` instead keeps the outgoing
 * module rendering from its own state in a preallocated per-slot shadow while
 * an equal-power pair of gains hands over across `fxXfadeMs`. The toggle is an
 * instrument for a ruling, not a feature, so the promise that matters most is
 * that its DEFAULT changed nothing.
 *
 *   T1 the default is inert — through a real type flip, a plugin that never
 *      hears of ids 264/265 renders BIT-IDENTICALLY to one told fxXfade = 0,
 *      and the CONTROL (fxXfade = 1, same script) renders differently, so the
 *      comparison is demonstrably able to see a difference at all.
 *   T2 a tail is faded, not cut — Room flipped to Off with silence on the bus:
 *      per-block RMS decays over ~fxXfadeMs with no single-block drop past
 *      6 dB, while the CONTROL (the same flip at fxXfade = 0) drops past 20 dB
 *      in one block.
 *   T3 one switch, one fade — the picked corner's type written EVERY tick (what
 *      a morph delivers at the choke point) crosses over exactly ONCE, for the
 *      declared number of samples, and never re-fades while the write stays on
 *      one side. CONTROL: a genuine second flip does start a second epoch, so
 *      the epoch counter can count past one.
 *   T4 the singleton guard counts a shadow — a second Comb is refused while a
 *      Comb is fading out, and ALLOWED once the fade ends, which is what makes
 *      the refusal the shadow's doing rather than a permanent block.
 *
 * DECLARED LIMITS, because an undeclared one is how a probe rots into
 * decoration:
 *   - T1 says "the default is inert ACROSS INSTANCES", not "identical to the
 *     pre-B117 build" — this binary cannot render a build it is not. The
 *     repo-level proof of that is ./verify full: the parity chains (parity,
 *     time, filter, notch, …) are bit-exact goldens and all stay green.
 *   - T2 and T3 drive `FxRack` directly. The morph engine is out of this
 *     change's scope, and both properties live in the rack: T2 needs an exactly
 *     silent dry bus (an envelope tail would confound "the flip cut it"), T3
 *     needs to SEE fade epochs, which no CLAP readback exposes. What T3 proves
 *     about a blend sweep is the rack half — a re-write of the same type never
 *     re-arms; the shell half (the morph only calls applyParam when the value
 *     actually moves, hypersaw_clap.cpp morphCur) is a second, independent
 *     reason and is not measured here.
 *   - CALIBRATED against two plants, because a probe nobody has seen fail is a
 *     claim, not a measurement. (a) `setXfade` forced to false — 5 assertions
 *     go red, and the two that stayed green did so on SILENCE, which is why
 *     both now carry a `xf[flipAt] > 0` vacuity control. (b) the same-type
 *     re-arm guard removed from setType — the fade never ends (60 fading
 *     blocks instead of 13) and T3 goes red on both halves.
 *   - Echo <-> Room is NOT crossfaded and is not asserted here: those two are
 *     one TimeCore per slot in two modes and writing `mode` clears both its
 *     buffers, so that flip stays atomic by construction (fx_rack.h
 *     sharesCore). A shadow for it would need a second time engine per slot.
 */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"
#include "../src/fx_rack.h"

namespace
{
#include "notefuzz_scaffold.inc"

constexpr clap_id kIdXfade = 264, kIdXfadeMs = 265;
constexpr clap_id kIdFx1Type = 57, kIdFx1Amt = 58, kIdFx2Type = 59;

int fails = 0;
void ok(bool pass, const char *name, const char *detail)
{
  std::printf("%s %s (%s)\n", pass ? "OK  " : "FAIL", name, detail);
  if (!pass) fails++;
}

double rms(const float *L, const float *R, int n)
{
  double s = 0;
  for (int i = 0; i < n; i++) s += (double)L[i] * L[i] + (double)R[i] * R[i];
  return std::sqrt(s / (2.0 * n));
}
double db(double a, double b) { return 20.0 * std::log10((a + 1e-30) / (b + 1e-30)); }

/* ---- T1 + T4: the shipped plugin, through the CLAP factory ---------------
   The route from a param event to audio IS the thing under test for the
   guard, so these go through the factory rather than the rack. */
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
    p->init(p);
    p->activate(p, kSR, 32, kBlock);
    p->start_processing(p);
    L.assign(kBlock, 0);
    R.assign(kBlock, 0);
    ch[0] = L.data();
    ch[1] = R.data();
    out.data32 = ch;
    out.channel_count = 2;
    proc.frames_count = kBlock;
    proc.audio_outputs = &out;
    proc.audio_outputs_count = 1;
    proc.out_events = &kOut;
  }
  void step(EvList &e)
  {
    e.finalize();
    proc.in_events = &e.list;
    p->process(p, &proc);
  }
  void set(const std::vector<std::pair<clap_id, double>> &kv)
  {
    EvList e;
    for (auto &x : kv) e.params.push_back(mkParam(x.first, x.second));
    step(e);
  }
  void note(uint16_t type, int16_t key)
  {
    EvList e;
    e.notes.push_back(mkNote(type, 0, key, 1, 0.9));
    step(e);
  }
  void run(int blocks, std::vector<float> *cap)
  {
    for (int i = 0; i < blocks; i++)
    {
      EvList e;
      step(e);
      if (cap)
        for (int j = 0; j < kBlock; j++) { cap->push_back(L[j]); cap->push_back(R[j]); }
    }
  }
  double read(clap_id id)
  {
    auto *ext = (const clap_plugin_params_t *)p->get_extension(p, CLAP_EXT_PARAMS);
    double v = -1;
    if (ext) ext->get_value(p, id, &v);
    return v;
  }
  void kill()
  {
    p->stop_processing(p);
    p->deactivate(p);
    p->destroy(p);
  }
};

// One scripted render: FX1 = Delay under a held note, then the type flipped to
// Off mid-note. `mode` < 0 writes neither dev id at all.
std::vector<float> flipRender(int mode)
{
  Probe q;
  q.boot();
  if (mode >= 0) q.set({{kIdXfade, (double)mode}, {kIdXfadeMs, 80.0}});
  q.set({{kIdFx1Type, 9.0}, {kIdFx1Amt, 0.6}});
  std::vector<float> cap;
  q.note(CLAP_EVENT_NOTE_ON, 60);
  q.run(24, &cap);
  q.set({{kIdFx1Type, 0.0}});
  q.run(40, &cap);
  q.note(CLAP_EVENT_NOTE_OFF, 60);
  q.kill();
  return cap;
}

/* ---- T2 + T3: the rack itself -------------------------------------------
   See the declared limits at the top of this file for why. */
void feedNoise(hypersaw::FxRack &r, int blocks, uint32_t &seed)
{
  float L[kBlock], R[kBlock];
  for (int b = 0; b < blocks; b++)
  {
    for (int i = 0; i < kBlock; i++)
    {
      L[i] = (float)((double)(mrand(seed) % 20001) / 10000.0 - 1.0) * 0.3f;
      R[i] = (float)((double)(mrand(seed) % 20001) / 10000.0 - 1.0) * 0.3f;
    }
    r.processStereo(L, R, kBlock);
  }
}

// Room excited, then silence in, then the slot flipped to Off: per-block RMS of
// what comes out. Room rather than Delay because an FDN tail is DENSE — a plain
// delay's tail is silent between repeats, so "no single-block drop past 6 dB"
// would fire on the gaps rather than on the flip.
std::vector<double> tailAfterFlip(bool xfade, double ms, int blocksAfter)
{
  hypersaw::FxRack r;
  r.setSampleRate(kSR);
  r.setXfade(xfade);
  r.setXfadeMs(ms);
  r.setType(0, 8);        // Room
  r.setAmount(0, 0.8);    // regen
  uint32_t seed = 12345;
  feedNoise(r, 20, seed);

  float L[kBlock], R[kBlock];
  std::vector<double> out;
  // Two silent blocks first, so the measurement starts on the TAIL rather than
  // on the excitation still passing through.
  for (int b = 0; b < 2; b++)
  {
    std::fill(L, L + kBlock, 0.f);
    std::fill(R, R + kBlock, 0.f);
    r.processStereo(L, R, kBlock);
    out.push_back(rms(L, R, kBlock));
  }
  r.setType(0, 0);        // Off, mid-tail
  for (int b = 0; b < blocksAfter; b++)
  {
    std::fill(L, L + kBlock, 0.f);
    std::fill(R, R + kBlock, 0.f);
    r.processStereo(L, R, kBlock);
    out.push_back(rms(L, R, kBlock));
  }
  return out;
}

// A morph delivers the PICKED CORNER'S value every grid tick, so the rack sees
// the same type written over and over with one genuine switch in the middle.
// `second` plants a real second flip — the control that proves the counter can
// reach two.
void sweepEpochs(bool second, int &epochs, int &fadingBlocks)
{
  hypersaw::FxRack r;
  r.setSampleRate(kSR);
  r.setXfade(true);
  r.setXfadeMs(80);
  uint32_t seed = 999;
  // PRIME OUTSIDE THE COUNT. Off -> Delay is itself a handover (a module
  // entering fades in — that is half the point), so the slot is settled into
  // Delay before the sweep starts; otherwise the sweep's first tick would be
  // counted as one of the epochs the sweep is supposed to produce.
  r.setType(0, 9);
  feedNoise(r, 20, seed);

  float L[kBlock], R[kBlock];
  epochs = 0;
  fadingBlocks = 0;
  bool was = false;
  for (int b = 0; b < 60; b++)
  {
    const int want = b < 20 ? 9 : (second && b >= 40 ? 1 : 0);
    r.setType(0, want);   // written EVERY tick, as the field does
    for (int i = 0; i < kBlock; i++)
    {
      L[i] = (float)((double)(mrand(seed) % 20001) / 10000.0 - 1.0) * 0.3f;
      R[i] = (float)((double)(mrand(seed) % 20001) / 10000.0 - 1.0) * 0.3f;
    }
    r.processStereo(L, R, kBlock);
    if (r.fading(0))
    {
      fadingBlocks++;
      if (!was) epochs++;
    }
    was = r.fading(0);
  }
}
}  // namespace

int main()
{
  char d[240];

  // ---- T1: the default changed nothing ------------------------------------
  const std::vector<float> untouched = flipRender(-1);
  const std::vector<float> atomic = flipRender(0);
  const std::vector<float> crossfaded = flipRender(1);
  std::snprintf(d, sizeof d, "%zu samples compared", untouched.size());
  // B117 ruled 2026-09-16: the default IS crossfade, so "inert default" now
  // means an instance that never heard of the id renders like fxXfade = 1.
  ok(!untouched.empty() && untouched == crossfaded,
     "T1 fxXfade=1 is bit-identical to never writing the param (buried default)", d);
  ok(crossfaded != atomic,
     "T1 CONTROL fxXfade=1 renders differently (the comparison can see a difference)",
     crossfaded != atomic ? "diverged, as it must" : "identical — T1 proves nothing");

  // ---- T2: a tail faded, not cut ------------------------------------------
  const int kAfter = 24;                       // 24 x 256 = 139 ms > 80 ms fade
  const std::vector<double> xf = tailAfterFlip(true, 80.0, kAfter);
  const std::vector<double> at = tailAfterFlip(false, 80.0, kAfter);
  const int flipAt = 2;                        // index of the first post-flip block
  const double xfFlipDrop = -db(xf[flipAt], xf[flipAt - 1]);
  const double atomicDrop = -db(at[flipAt], at[flipAt - 1]);
  std::snprintf(d, sizeof d, "%.2f dB at the flip (limit 6)", xfFlipDrop);
  ok(xfFlipDrop < 6.0, "T2 crossfade: no single-block drop past 6 dB at the flip", d);
  std::snprintf(d, sizeof d, "atomic drops %.1f dB in one block (want > 20)", atomicDrop);
  ok(atomicDrop > 20.0, "T2 CONTROL atomic cuts the tail (the defect is real)", d);
  /* SMOOTH ACROSS THE FADE, not only at its first block — the same 6 dB bar,
     held over the first three quarters of the handover. Not the whole of it:
     an equal-power curve ENDS at zero, so its last blocks are steep in dB by
     construction (cos falls from 0.10 to 0 over the last 5.8 ms), and a bound
     that ignored that would be a bound on arithmetic, not on audibility. */
  const int fadeBlocks = (int)(0.75 * 0.080 * kSR / kBlock);   // 75% of the 80 ms fade
  double worstInFade = 0;
  for (int i = flipAt + 1; i <= flipAt + fadeBlocks && i < (int)xf.size(); i++)
  {
    const double drop = -db(xf[i], xf[i - 1]);
    if (drop > worstInFade) worstInFade = drop;
  }
  /* `xf[flipAt] > 0` is the VACUITY CONTROL, and it is not decoration: with the
     crossfade planted out, every post-flip block is exactly zero and "no drop
     past 6 dB" passes at 0.00 dB — a smoothness bound satisfied by silence.
     Same guard on the span assertion below, for the same reason. */
  std::snprintf(d, sizeof d, "worst %.2f dB over the first %d blocks of the fade "
                             "(limit 6; flip block rms %.3g)",
                worstInFade, fadeBlocks, xf[flipAt]);
  ok(worstInFade < 6.0 && xf[flipAt] > 1e-9,
     "T2 crossfade: smooth across the fade, not only at its first block", d);
  // The fade SPANS ~fxXfadeMs: still substantially present at 60 ms, and
  // DIGITALLY SILENT once it is over (Off is a bit-exact passthrough, and the
  // bus is silence) — so "it is over" is an exact zero, not a threshold.
  const int b60 = flipAt + (int)(0.060 * kSR / kBlock);
  std::snprintf(d, sizeof d, "at 60 ms %.1f dB relative to the flip; at 139 ms rms %.3g",
                db(xf[b60], xf[flipAt]), xf[xf.size() - 1]);
  ok(db(xf[b60], xf[flipAt]) > -20.0 && xf[flipAt] > 1e-9 && xf[xf.size() - 1] == 0.0,
     "T2 the fade spans about fxXfadeMs, then it is over", d);

  // ---- T3: one switch, one fade -------------------------------------------
  int ep = 0, fb = 0;
  sweepEpochs(false, ep, fb);
  // Blocks in which the fade is still running: fadeLeft is decremented AFTER a
  // block renders, so a fade of `len` samples spans (len-1)/kBlock + 1 blocks
  // of which the last leaves fadeLeft at 0 — i.e. (len-1)/kBlock blocks report
  // "still fading" afterwards. Derived, not typed, so fxXfadeMs can move.
  const int fadeLen = (int)std::lround(0.080 * kSR);
  const int wantBlocks = (fadeLen - 1) / kBlock;
  std::snprintf(d, sizeof d, "%d epoch(s), %d fading block(s) (want 1 and %d)", ep, fb, wantBlocks);
  ok(ep == 1 && fb == wantBlocks, "T3 a re-written type crosses over exactly once", d);
  int ep2 = 0, fb2 = 0;
  sweepEpochs(true, ep2, fb2);
  std::snprintf(d, sizeof d, "%d epochs with a planted second flip (want 2)", ep2);
  ok(ep2 == 2, "T3 CONTROL a genuine second flip does start a second fade", d);

  // ---- T4: the guard counts a fading shadow -------------------------------
  Probe c;
  c.boot();
  c.set({{kIdXfade, 1.0}, {kIdXfadeMs, 200.0}});
  c.set({{kIdFx1Type, 5.0}, {kIdFx1Amt, 0.6}});   // FX1 = Comb
  c.note(CLAP_EVENT_NOTE_ON, 60);
  c.run(4, nullptr);
  c.set({{kIdFx1Type, 0.0}});                     // leave Comb — a shadow is armed
  c.set({{kIdFx2Type, 5.0}});                     // a second Comb, mid-fade
  const double duringFade = c.read(kIdFx2Type);
  std::snprintf(d, sizeof d, "FX2 reads %.0f during the fade (want 0 = refused)", duringFade);
  ok(duringFade == 0.0, "T4 a second Comb is refused while a Comb shadow is fading", d);
  c.run(60, nullptr);                             // 60 x 256 = 348 ms > 200 ms
  c.set({{kIdFx2Type, 5.0}});
  const double afterFade = c.read(kIdFx2Type);
  std::snprintf(d, sizeof d, "FX2 reads %.0f after the fade (want 5 = allowed)", afterFade);
  ok(afterFade == 5.0, "T4 CONTROL the refusal ends WITH the fade, not forever", d);
  c.note(CLAP_EVENT_NOTE_OFF, 60);
  c.kill();

  hypersaw_entry_deinit();
  if (fails)
  {
    std::printf("fxxfade_check: RED (%d failure(s))\n", fails);
    return 1;
  }
  std::printf("fxxfade_check: GREEN (0 failures)\n");
  return 0;
}
