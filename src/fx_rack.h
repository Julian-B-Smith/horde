/*
 * fx_rack.h — post-oscillator internal FX rack (ADR-054, increment 1).
 *
 * A fixed series of kRackSlots slots processed IN ORDER (slot 0 → 1 → …), each
 * slot running one user-selectable effect in place on the stereo bus. Order is
 * the slot index, so "reordering" = reassigning effect types to slots — the
 * simplest thing that makes FX order audible (the human's #1 requirement). A
 * true parallel/matrix grid and the real engine cores (filter/notch/time +
 * saturation) come in later increments; this increment is the routing/param/UX
 * skeleton with TRIVIAL placeholder effects to get the feel right.
 *
 * Parity contract (ADR-054): the default slot type is Off, and an all-Off rack
 * MUST be a bit-exact passthrough — processStereo touches no sample — so every
 * existing instrument golden stays green. That is why Off is `continue`, not a
 * gain of 1.0 (a multiply would still be exact, but not touching the buffer is
 * the unambiguous guarantee).
 *
 * Real-time rules (charter): allocation-free after construction, no locks, no
 * wall-clock. All state is preallocated per slot; setType/setAmount are plain
 * stores. Placeholder DSP is per-sample arithmetic only.
 */
#pragma once

#include <cmath>
#include <cstring>
#include <memory>
#include <vector>
#include <algorithm>

#include "notch_core.h"
#include "time_core.h"
#include "delay_core.h"

namespace hypersaw
{

/* ---- THE SLOT CONTRACT, DECLARED AS DATA -----------------------------------
 * Approved 2026-08-15 ("the rack owns dry/wet"). Six slot types currently mean
 * four different things by `amount`, with THREE different identity points and
 * two slots that cannot be bypassed at all — which is how Notch shipped
 * collapsing stereo to mono at a setting a patch author would read as "off".
 *
 * This table is the promise each slot makes. `slotcontract_check` reads it and
 * holds every slot to it, so a NEW slot type cannot quietly acquire the same
 * defect: it has to state its identity point and its side effects, and the gate
 * measures whether it told the truth.
 *
 * `identity_at < 0` means THE SLOT HAS NO IDENTITY POINT — it cannot be
 * bypassed at any setting. That is a defect the contract exists to remove, not
 * a property to be tolerated; it is recorded here so the gate can name it rather
 * than let it hide. Fixing it is the rack-side half (rack-owned `mix`), which
 * lands after this gate proves what is broken.
 */
struct SlotContract
{
  double identity_at;     // amount at which the slot is a bit-exact passthrough
  bool blends_dry;        // is dry+wet "less effect", or a DIFFERENT effect?
  bool changes_image;     // may legitimately alter stereo image / channel count
  bool changes_level;     // may legitimately alter broadband level
  int latency_samples;    // declared group delay of the wet path
};

// Indexed by FxType. Off is trivially an identity at every amount.
constexpr SlotContract kSlotContract[] = {
    /* Off    */ {0.0, true, false, false, 0},
    /* Drive  */ {0.0, true, false, false, 0},
    /* Filter */ {0.0, false, false, false, 0},   // dry+lowpass is a shelf, not a gentler lowpass
    /* Gain   */ {0.5, true, false, true, 0},     // 0.5 is unity; level IS its effect
    /* Comp   */ {-1.0, false, false, true, 0},   // 0.98 brickwall always on: NO identity point
    /* Comb   */ {0.0, true, false, false, 0},    // delayed wet: blending dry combs unless compensated
    /* Notch  */ {-1.0, false, true, true, 0},    // measured -5.4 dB and mono at amount 0
};

constexpr int kRackSlots = 4;

// Effect types. Off is the inert default; Drive/Filter/Gain are the increment-1
// placeholders (kept — order is audible with them); Comp and Comb (ADR-071) are
// REAL cores transcribed from the detune-lab prototype; Notch (added for the
// parallel-streams round) is the Track E1.2 swarm-herded notch cascade
// (notch_core.h), ported and oracle-covered (notch_check) but previously
// unreachable from the rack — this slot is what makes it selectable.
enum class FxType : int
{
  Off = 0,
  Drive = 1,   // dry/wet tanh — amount 0 = passthrough, 1 = tanh(4x)
  Filter = 2,  // one-pole LP  — amount 0 = open (passthrough), 1 = heavy
  Gain = 3,    // level        — amount 0.5 = unity, 0 = silence, 1 = +6 dB
  Comp = 4,    // lab comp+limiter: amount = strength; 0.98 brickwall always on
  Comb = 5,    // polyphonic per-note Karplus-Strong comb: amount = wet mix
  Echo = 7,    // tap-swarm delay (TimeCore mode 0); ADR-031's stability laws
               // live inside that core, Layer-0 guarded by L0-19
  Room = 8,    // FDN room swarm (TimeCore mode 1); L0-20/21
  Delay = 9,   // ADR-142: the STANDARD stereo delay (DelayCore) — textbook
               // feedback, sync, L/R offset, crossfeed/ping-pong, in-loop tone.
               // The A/B baseline for Echo/Room, and the module B73 judges them
               // against; its own oracle is delay_check, not a lab parity run.
  Notch = 6,   // swarm-herded notch cascade (NotchCore): amount -> core's own
               // dry/wet `mix` param; population/topology stay at core defaults
               // until the rack grows a per-slot param page for them.
};

// Comb bank size: one tuned line per sounding note, stolen oldest-first past 8.
constexpr int kCombLines = 8;

class FxRack
{
 private:
  /* THE PER-SLOT RECORD, declared up here rather than beside the members
     because B117's render helpers take it BY REFERENCE: a parameter type must
     be complete where the function is DECLARED, unlike a function body. It is
     the state a fading module carries with it (fx_rack's shadow), which is why
     it is one struct and not a scatter of parallel arrays. */
  struct Slot
  {
    // Rack-owned dry/wet. Defaults to 1 (fully wet) so every patch predating the
    // contract renders bit-identically; 0 is a universal, guaranteed bypass.
    double mix = 1.0;
    FxType type = FxType::Off;
    double amount = 0.5;
    // Second per-slot axis (2026-08-03). ADR-071 fixed the comb's resonance at
    // the lab default "until the rack grows per-slot param pages" — this is
    // that page, kept to ONE generic knob rather than a comb-specific param so
    // the next slot type that wants a second control costs no new ids. Only
    // Comb reads it today; 0.5 reproduces the previously hardcoded fb = 0.79
    // exactly, so the default is bit-inert.
    double tone = 0.5;
    double zL = 0, zR = 0;  // one-pole filter memory (Filter type)
  };
  struct Comb
  {
    std::vector<float> bufL, bufR;  // sized at setSampleRate (main thread)
    int key = -1, dly = 100, w = 0;
    long age = -1;
    double lpL = 0, lpR = 0;
    // Declick (2026-08-03). A line is retuned while it may still be RINGING,
    // and both the old code's memset and the delay-length jump are step
    // discontinuities in a signal that is audibly nonzero — the human's "tiny
    // amount of clicking". `g` gates this line's contribution so a retune can
    // be deferred until it is silent: retuning -> ramp g to 0 -> apply pendDly
    // at the bottom -> ramp back. Fading DOWN first is the whole point; a
    // ramp-up alone cannot hide a discontinuity that already happened.
    double g = 0;
    int pendDly = 0;
    bool retuning = false;
  };

 public:
  // Construction is main-thread: allocate at the default rate immediately so a
  // shell that never calls setSampleRate still has valid comb buffers (an empty
  // buffer would make `% len` divide by zero in process).
  FxRack() { setSampleRate(44100); }

  // Called from activate (main thread — allocation is allowed there, never in
  // process). Sizes each comb line to the lowest musical note (~20 Hz) at sr,
  // and derives the comp coefficients from their SECONDS constants (ADR-009 —
  // the lab's 0.3 / 0.0015 were per-sample at 44.1 kHz; expressed as time
  // constants they are sr-independent: atk 63.58 us, rel 15.11 ms).
  void setSampleRate(double sampleRate)
  {
    sr = sampleRate;
    const int len = (int)std::ceil(sr / 20.0) + 4;
    for (auto &c : combs)
    {
      c.bufL.assign(len, 0.0f);
      c.bufR.assign(len, 0.0f);
      c.key = -1;
      c.w = 0;
      c.lpL = c.lpR = 0;
      c.dly = 100;
      c.g = 0;
      c.pendDly = 0;
      c.retuning = false;
    }
    compAtk = 1.0 - std::exp(-1.0 / (6.3576e-5 * sr));
    compRel = 1.0 - std::exp(-1.0 / (1.5106e-2 * sr));
    compEnv = 0;
    // Notch (Track E1.2): one NotchCore per slot, (re)constructed here — main
    // thread only, never in processSlot (RT-safety: NotchCore's ctor allocates
    // nothing itself, but rebuilding it fresh at the new sr is still an
    // allocation-shaped operation and belongs where the comb buffers are sized).
    for (auto &n : notch) n = std::make_unique<NotchCore>(sr);
    /* TIME ENGINES (Echo/Room), same rule and same reason as Notch above:
       TimeCore's CONSTRUCTOR allocates (~3 s echo buffer + 12 room lines,
       ~1.75 MB each), so it is built here on the main thread and never touched
       by processSlot. Mode changes ARE audio-thread safe: setParam("mode")
       calls rebuild(false), which writes pre-existing arrays and allocates
       nothing. Unconditional rather than lazy because setType() runs on the
       AUDIO thread from param events, so lazy construction there would be
       exactly the allocation this note exists to prevent. */
    for (auto &t : timeFx) t = std::make_unique<TimeCore>(sr);
    // 2 MB of buffers per instance: heap, never a member by value (the oracle
    // segfaulted on two stack-allocated cores before this was float+heap).
    for (auto &d : delayFx) d = std::make_unique<DelayCore>(sr);
    // Declick constants in SECONDS, converted here (ADR-009). 6 ms is long
    // enough to be inaudible as a step and short enough that a retuned line is
    // back before the next note; 30 ms smooths the 1/activeLines normaliser,
    // which used to divide every ringing line's output by a bigger integer the
    // instant a new note claimed a line — a step of up to 6 dB, mid-ring.
    combRamp = 1.0 - std::exp(-1.0 / (6.0e-3 * sr));
    combNorm = 1.0 - std::exp(-1.0 / (3.0e-2 * sr));
    normSm = 1.0;
    // A fade length is derived from seconds at THIS rate (ADR-009), so a rate
    // change invalidates one in flight; drop it rather than let it run long.
    for (int k = 0; k < kRackSlots; k++) fadeLeft[k] = 0;
  }

  // Note feed for note-context slots (ADR-071 comb). Called from the shell's
  // note handlers on the audio thread — bounded work, no allocation (the
  // buffers exist since setSampleRate; clearing one line is a fixed memset).
  void noteOn(int key, double freq)
  {
    Comb *line = nullptr;
    for (auto &c : combs)
      if (c.key == key) { line = &c; break; }                 // retrigger same key
    if (!line)
      for (auto &c : combs)
        if (c.key < 0) { line = &c; break; }                  // free line
    if (!line)
    {
      line = &combs[0];                                       // steal oldest
      for (auto &c : combs)
        if (c.age < line->age) line = &c;
    }
    const int len = (int)line->bufL.size();
    const int newDly =
        std::max(2, std::min(len - 1, (int)std::lround(sr / std::max(20.0, freq))));
    line->key = key;
    line->age = ageCounter++;
    // The buffer is NOT cleared any more. This line is continuously fed from
    // the bus rather than impulse-excited, so whatever it holds is simply the
    // excitation for the new pitch — while wiping it mid-ring was an audible
    // step to zero. What remains discontinuous is the read-pointer jump, so a
    // still-sounding line rings down first and retunes at the bottom.
    if (line->g <= 0.002)
    {
      line->dly = newDly;   // silent already: nothing to click
      line->retuning = false;
    }
    else
    {
      line->pendDly = newDly;
      line->retuning = true;
    }
  }
  void noteOff(int key)
  {
    // The line keeps ringing (natural KS decay) and becomes steal-preferred by
    // age; no state change needed beyond forgetting the key binding on steal.
    (void)key;
  }

  // Comb declick times, in SECONDS (ADR-009). Defaults are set in
  // setSampleRate; 0 restores the pre-2026-08-03 instant behaviour, which is
  // how the oracle plants a known-bad case to prove its click detector can
  // actually see one. Not a test-only hook — it is the real constant, exposed.
  void setCombDeclick(double rampSec, double normSec)
  {
    combRamp = rampSec > 0 ? 1.0 - std::exp(-1.0 / (rampSec * sr)) : 1.0;
    combNorm = normSec > 0 ? 1.0 - std::exp(-1.0 / (normSec * sr)) : 1.0;
  }

  /* INSTANCE CAPS (2026-09-05). The Comb is a SINGLETON by construction: its
     eight KS lines are ONE bank owned by the rack (`combs[]`, fed by the
     shell's note events), and every Comb slot iterates that same bank — a
     second Comb slot writes each line twice per block and advances its write
     pointer twice, so the feedback compounds past unity and the tuning
     halves. The human loaded a second Comb by accident and it "blew up the
     audio". Everything else owns per-slot state and may repeat. Indexed by
     FxType; kRackSlots = unlimited. The B95 pool policy may lower others. */
  static constexpr int kSlotMaxInstances[10] = {
      kRackSlots, kRackSlots, kRackSlots, kRackSlots,   // Off, Drive, Filter, Gain
      kRackSlots, 1,          kRackSlots, kRackSlots,   // Comp, COMB, Notch, Echo
      kRackSlots, kRackSlots};                          // Room, Delay
  int typeOf(int slot) const { return (int)slots[slot].type; }
  bool typeAllowed(int slot, int type) const
  {
    if (type < 0 || type >= 10) return false;
    int held = 0;
    for (int k = 0; k < kRackSlots; k++)
    {
      if (k == slot) continue;
      /* B117: a module fading OUT still renders and still writes the
         rack-shared bank, so it HOLDS its type until the fade ends — else a
         slot could leave Comb and another claim it during the handover, which
         is the double-write the cap exists to prevent, merely delayed by
         fxXfadeMs. `else if` keeps each slot worth AT MOST ONE instance, so a
         shadow can never push a non-singleton past its kRackSlots sentinel.
         A slot's OWN shadow is not counted (the `continue` above): arming a
         new fade overwrites it, so it never coexists with that slot's
         incoming module. */
      if ((int)slots[k].type == type) held++;
      else if (fadeLeft[k] > 0 && (int)shadow[k].type == type) held++;
    }
    return held < kSlotMaxInstances[type];
  }

  /* ---- B117 / ADR-163: FX PRESENCE CROSSFADE ------------------------------
   * A DEV TOGGLE, an instrument for a ruling — not a feature. Module TYPE is
   * stepped, so under both morph modes a slot's module flips atomically (B49):
   * a tail cut, an entrance with no lead-in. With `fxXfade = 1` the OUTGOING
   * module keeps rendering from its own state in a preallocated per-slot
   * SHADOW while an equal-power fade hands over to the incoming one. The
   * bounded pool (B95) is the structural answer at 1.1; this previews it so
   * the human can HEAR both before ruling.
   *
   * Default ON since the B117 ruling (2026-09-16, ADR-163 A2): the toggle is
   * buried, crossfade at 80 ms is the shipped behaviour, and fxxfade_check T1
   * proves an instance that never writes id 264 renders exactly like one told
   * fxXfade = 1. The atomic path stays reachable (a saved fxXfade = 0 loads),
   * which is what keeps the pre-B117 render as the T1 control.
   */
  void setXfade(bool on) { xfadeOn = on; }
  void setXfadeMs(double ms) { xfadeMs = ms < 5.0 ? 5.0 : (ms > 500.0 ? 500.0 : ms); }
  double getXfade() const { return xfadeOn ? 1.0 : 0.0; }
  double getXfadeMs() const { return xfadeMs; }
  // Readback for the oracle and the guard: is this slot mid-handover, and what
  // is the outgoing module while it is?
  bool fading(int slot) const
  {
    return slot >= 0 && slot < kRackSlots && fadeLeft[slot] > 0;
  }
  int shadowTypeOf(int slot) const
  {
    return (slot < 0 || slot >= kRackSlots || fadeLeft[slot] <= 0) ? 0
                                                                  : (int)shadow[slot].type;
  }

  void setType(int slot, int type)
  {
    if (slot < 0 || slot >= kRackSlots) return;
    const FxType prev = slots[slot].type;
    /* ARM BEFORE THE STORE, so the shadow captures the OUTGOING slot — its
       amount/tone/mix and its one-pole memory — rather than the incoming one.
       A type change during a running fade OVERWRITES the shadow, which is
       ADR-163's "drop the current shadow and start a new fade from the
       then-outgoing module" and is also what keeps "at most two modules per
       slot" true by construction rather than by counting.
       A redundant write of the SAME type must not re-arm (a blend morph writes
       the picked corner's type every tick, and re-arming there would re-fade
       forever — fxxfade_check T3). */
    if ((FxType)type != prev)
    {
      if (xfadeOn && !sharesCore(prev, (FxType)type))
      {
        shadow[slot] = slots[slot];
        fadeLen[slot] = (int)std::lround(xfadeMs * 1e-3 * sr);
        if (fadeLen[slot] < 1) fadeLen[slot] = 1;
        fadeLeft[slot] = fadeLen[slot];
      }
      else fadeLeft[slot] = 0;   // atomic: the default, and the shared-core pair
    }
    slots[slot].type = (FxType)type;
    /* ADR-142: selecting Delay is a LOAD, not a knob move — snap the read head
       to the patch's time instead of gliding to it from whatever the slot held
       before. Without this the first repeats after a type change arrive late
       and pitched (measured as an oracle failure before snapTime existed). */
    if ((FxType)type == FxType::Delay && prev != FxType::Delay && delayFx[slot])
    {
      delayFx[slot]->p = delaySet[slot];
      delayFx[slot]->reset();
    }
  }
  void setTone(int slot, double tone)
  {
    if (slot < 0 || slot >= kRackSlots) return;
    slots[slot].tone = tone < 0 ? 0 : (tone > 1 ? 1 : tone);
  }
  double getTone(int slot) const
  {
    return (slot < 0 || slot >= kRackSlots) ? 0.5 : slots[slot].tone;
  }
  /* ADR-131 — per-slot time-engine parameters. The shell writes these; the
     mirrored state lives with the other private members. The change guard in
     processSlot is what makes them safe: size/spread/nb/dist call
     TimeCore::rebuild() inside setParam, so writing them every block would
     rebuild the delay swarm every block — audible as a stutter, and pointless
     work. The scalars go through the same path so there is one rule, not two. */
  void setTimeParam(int slot, int key, double v)
  {
    if (slot < 0 || slot >= kRackSlots || key < 0 || key > 6) return;
    auto &t = timeSet[slot];
    if (key == 0) t.size = v;
    else if (key == 1) t.spread = v;
    else if (key == 2) t.taps = v;
    else if (key == 3) t.damp = v;
    else if (key == 4) t.noise = v;
    else if (key == 5) t.stereo = v;
    else t.dist = v;
  }
  /* ADR-142: the Delay's per-slot params, keyed 0..7 in declaration order —
     the same arithmetic-not-cases shape ADR-131 chose for the time engines, so
     adding a param is a table edit at both ends and never a switch to keep in
     step. */
  void setDelayParam(int slot, int key, double v)
  {
    if (slot < 0 || slot >= kRackSlots || key < 0 || key > 7) return;
    auto &d = delaySet[slot];
    switch (key)
    {
      case 0: d.timeMs = v; break;
      case 1: d.sync = v; break;
      case 2: d.timeBeats = v; break;
      case 3: d.offsetR = v; break;
      case 4: d.feedback = v; break;
      case 5: d.crossfeed = v; break;
      case 6: d.damp = v; break;
      default: d.loopHp = v; break;
    }
  }
  double getDelayParam(int slot, int key) const
  {
    if (slot < 0 || slot >= kRackSlots || key < 0 || key > 7) return 0.0;
    const auto &d = delaySet[slot];
    switch (key)
    {
      case 0: return d.timeMs;
      case 1: return d.sync;
      case 2: return d.timeBeats;
      case 3: return d.offsetR;
      case 4: return d.feedback;
      case 5: return d.crossfeed;
      case 6: return d.damp;
      default: return d.loopHp;
    }
  }
  // Host tempo for sync. Data pushed in, never a clock read in a core.
  void setTempo(double bpm)
  {
    rackTempo = bpm > 1 ? bpm : 120.0;
    for (auto &d : delayFx) d->setTempo(rackTempo);
  }
  double getTimeParam(int slot, int key) const
  {
    if (slot < 0 || slot >= kRackSlots || key < 0 || key > 6) return 0.0;
    const auto &t = timeSet[slot];
    if (key == 0) return t.size;
    if (key == 1) return t.spread;
    if (key == 2) return t.taps;
    if (key == 3) return t.damp;
    if (key == 4) return t.noise;
    if (key == 5) return t.stereo;
    return t.dist;
  }

  void setMix(int slot, double v)
  {
    if (slot < 0 || slot >= kRackSlots) return;
    slots[slot].mix = v < 0 ? 0 : (v > 1 ? 1 : v);
  }
  double getMix(int slot) const { return (slot < 0 || slot >= kRackSlots) ? 1.0 : slots[slot].mix; }
  void setAmount(int slot, double amount)
  {
    if (slot < 0 || slot >= kRackSlots) return;
    slots[slot].amount = amount < 0 ? 0 : (amount > 1 ? 1 : amount);
  }

  // Readback for state_save / host get_value (the shell keys these by id).
  int getType(int slot) const
  {
    return (slot < 0 || slot >= kRackSlots) ? 0 : (int)slots[slot].type;
  }
  double getAmount(int slot) const
  {
    return (slot < 0 || slot >= kRackSlots) ? 0 : slots[slot].amount;
  }

  // Reset filter memory (e.g. on transport discontinuity). Parity-neutral.
  void reset()
  {
    for (auto &s : slots) { s.zL = 0; s.zR = 0; }
    for (int k = 0; k < kRackSlots; k++) fadeLeft[k] = 0;   // B117: no fade survives a reset
  }

  // Process the stereo bus in place, slot 0 → kRackSlots-1. All-Off is a
  // no-op (bit-exact passthrough — the parity gate).
  /* The whole rack, slot 0 → 1 → … in order. Unchanged behaviour: it is now a
     loop over processSlot(), which is the same sequence written once. */
  void processStereo(float *L, float *R, int n)
  {
    for (int i = 0; i < kRackSlots; i++) processSlot(i, L, R, n);
  }

  /* ONE slot, in place (B23 increment 2a). The crosspoint matrix routes each
     slot its own input and keeps its output, so it cannot use the fixed
     sequence above — but the per-slot DSP must stay literally the same code, or
     the routing change and an effect change would land together and neither
     could be attributed. Extracted mechanically: the switch body below is
     untouched, only its wrapper moved. The 147 parity goldens are the proof. */
  /* THE RACK OWNS DRY/WET (approved 2026-08-15). One rule for every slot type:
   *     out = (mix == 0) ? in : lerp(in, wet, mix)
   * `mix == 0` is an EARLY-OUT, so passthrough is bit-identical BY CONSTRUCTION
   * rather than by each slot's implementation remembering to honour it. That is
   * the property the old design lacked, and the reason Notch could ship collapsing
   * stereo to mono at a setting a patch author reads as "off": there was no rule
   * a new slot type could not break.
   *
   * `mix == 1` runs the wet path untouched — no copy, no lerp — so every patch
   * that predates the contract is bit-identical and the parity goldens cannot move.
   *
   * `amount` stops carrying bypass duty and becomes purely per-slot character.
   * Gain's 0.5-is-unity and Comp's always-on brickwall stop being anomalies: they
   * are simply what those slots DO at mix = 1. */
  void processSlot(int idx, float *L, float *R, int n)
  {
    if (idx < 0 || idx >= kRackSlots) return;
    // B117: the ONLY new branch on the default path, and it is never taken
    // while fxXfade = 0 — so the pre-B117 render is reached by the same code
    // it always was, sample for sample.
    if (fadeLeft[idx] > 0) { renderCrossfade(idx, L, R, n); return; }
    renderSlot(idx, slots[idx], L, R, n);
  }

  void processSlotWet(int idx, float *L, float *R, int n)
  {
    if (idx < 0 || idx >= kRackSlots) return;
    renderWet(idx, slots[idx], L, R, n);
  }

 private:
  /* THE SAME DSP, ADDRESSED BY SLOT RECORD RATHER THAN BY INDEX (B117). `idx`
     still selects the per-slot CORES (notch/time/delay), because those are
     what the outgoing module must keep rendering from; `s` carries the state a
     shadow copies. The switch body below is untouched — only `slots[idx]`
     became the parameter `s`, which is what lets a shadow render without a
     second copy of any of it. */
  void renderSlot(int idx, Slot &s, float *L, float *R, int n)
  {
    const double mix = s.mix;
    if (mix <= 0.0) return;                                  // guaranteed bypass
    if (mix >= 1.0) { renderWet(idx, s, L, R, n); return; }  // today's path, exactly

    /* Partial blend. Chunked over a fixed stack buffer: no allocation on the audio
     * thread. Only reachable once a patch sets an intermediate mix, so nothing that
     * exists today takes this branch. */
    constexpr int kFxBlend = 256;
    float dl[kFxBlend], dr[kFxBlend];
    for (int off = 0; off < n; off += kFxBlend)
    {
      const int m = n - off < kFxBlend ? n - off : kFxBlend;
      std::memcpy(dl, L + off, (size_t)m * sizeof(float));
      std::memcpy(dr, R + off, (size_t)m * sizeof(float));
      renderWet(idx, s, L + off, R + off, m);
      for (int i = 0; i < m; i++)
      {
        L[off + i] = (float)(dl[i] + (L[off + i] - dl[i]) * mix);
        R[off + i] = (float)(dr[i] + (R[off + i] - dr[i]) * mix);
      }
    }
  }

  /* THE HANDOVER. Both modules see the SAME dry input — the slot's input bus —
     so the outgoing one keeps feeding its own tail while the incoming one
     builds its own from scratch, and an equal-power pair of gains hands over
     across fadeLen samples. Chunked over fixed stack buffers, so at most two
     modules run for at most fxXfadeMs and nothing allocates (the RT rule).
     Comb needs nothing special here: its wet is already gated by the per-line
     declick ramp `g`, so a slot LEAVING Comb fades through that gate and the
     equal-power pair on top of it. */
  void renderCrossfade(int idx, float *L, float *R, int n)
  {
    constexpr int kFxBlend = 256;
    constexpr double kHalfPi = 1.5707963267948966;   // no M_PI: undefined under MSVC
    float aL[kFxBlend], aR[kFxBlend];
    const int len = fadeLen[idx] < 1 ? 1 : fadeLen[idx];
    const int done0 = len - fadeLeft[idx];
    for (int off = 0; off < n; off += kFxBlend)
    {
      const int m = n - off < kFxBlend ? n - off : kFxBlend;
      std::memcpy(aL, L + off, (size_t)m * sizeof(float));
      std::memcpy(aR, R + off, (size_t)m * sizeof(float));
      renderSlot(idx, shadow[idx], aL, aR, m);           // outgoing, from its own state
      renderSlot(idx, slots[idx], L + off, R + off, m);  // incoming
      for (int i = 0; i < m; i++)
      {
        const int done = done0 + off + i;
        const double th = done >= len ? kHalfPi : kHalfPi * ((double)done / (double)len);
        L[off + i] = (float)(aL[i] * std::cos(th) + L[off + i] * std::sin(th));
        R[off + i] = (float)(aR[i] * std::cos(th) + R[off + i] * std::sin(th));
      }
    }
    fadeLeft[idx] = fadeLeft[idx] > n ? fadeLeft[idx] - n : 0;
  }

  /* THE ONE PAIR OF TYPES THAT CANNOT HAND OVER. Echo and Room are ONE
     TimeCore per slot in two modes, and writing `mode` CLEARS both of that
     core's buffers (time_core.h setParam) — so there is no outgoing state left
     to fade, and running both modes on one core would re-clear it every block.
     That flip stays atomic, exactly as it is today. Every other pair owns
     disjoint state: Filter's one-pole lives in the Slot the shadow copies,
     Notch / Delay / the time engines each have their own per-slot core, and
     Comb's bank is rack-shared but only one module of a slot can be Comb. */
  static bool sharesCore(FxType a, FxType b)
  {
    const bool ta = (a == FxType::Echo || a == FxType::Room);
    const bool tb = (b == FxType::Echo || b == FxType::Room);
    return ta && tb;
  }

  void renderWet(int idx, Slot &s, float *L, float *R, int n)
  {
    if (idx < 0 || idx >= kRackSlots) return;
    {
      switch (s.type)
      {
        case FxType::Off:
          break;  // bit-exact passthrough — never touch the buffer
        case FxType::Drive:
        {
          const double w = s.amount;                 // dry/wet
          const double pre = 1.0 + s.amount * 3.0;    // up to 4x into tanh
          for (int i = 0; i < n; i++)
          {
            L[i] = (float)((1 - w) * L[i] + w * std::tanh(L[i] * pre));
            R[i] = (float)((1 - w) * R[i] + w * std::tanh(R[i] * pre));
          }
          break;
        }
        case FxType::Filter:
        {
          // one-pole LP: coef 1 = passthrough, → 0 = heavy. amount 0 → open.
          const double coef = 1.0 - s.amount * 0.99;
          for (int i = 0; i < n; i++)
          {
            s.zL += coef * (L[i] - s.zL);
            s.zR += coef * (R[i] - s.zR);
            L[i] = (float)s.zL;
            R[i] = (float)s.zR;
          }
          break;
        }
        case FxType::Gain:
        {
          const double g = s.amount * 2.0;  // 0.5 → unity
          for (int i = 0; i < n; i++) { L[i] = (float)(L[i] * g); R[i] = (float)(R[i] * g); }
          break;
        }
        case FxType::Comp:
        {
          // Lab comp + limiter as ONE dynamics slot (ADR-071): amount = comp
          // strength (lab's `comp` knob); the 0.98 brickwall is always engaged
          // while the slot is active — "optional" is the slot being Off. Peak
          // follower with fast attack / slow release (seconds-derived coeffs,
          // set in setSampleRate); soft knee above 0.4, ratio 1 + 4*amount.
          const double amt = s.amount;
          for (int i = 0; i < n; i++)
          {
            double l = L[i], r = R[i];
            const double a = std::max(std::fabs(l), std::fabs(r));
            compEnv += (a > compEnv ? compAtk : compRel) * (a - compEnv);
            if (amt > 0.005 && compEnv > 0.4)
            {
              const double gr = (0.4 + (compEnv - 0.4) / (1 + amt * 4)) / compEnv;
              l *= gr;
              r *= gr;
            }
            const double pk = std::max(std::fabs(l), std::fabs(r));
            if (pk > 0.98) { const double lg = 0.98 / pk; l *= lg; r *= lg; }
            L[i] = (float)l;
            R[i] = (float)r;
          }
          break;
        }
        case FxType::Echo:
        case FxType::Room:
        {
          /* Track E2's time engines, reached at last. One core, two modes:
             ECHO is a tap-swarm delay, ROOM an FDN room swarm. Both carry
             ADR-031's stability laws INSIDE the core -- feedback normalised /N
             on worst-case correlation, DC blocked in every loop -- which is why
             a swarm of delays survives high regen without the LF runaway that
             ruling was written after.

             `amount` -> REGEN, deliberately. The slot's own `mix` already owns
             wet/dry under the rack contract, so mapping amount to mix would be
             a second dry/wet fighting the first. Regen is what changes the
             effect's identity: slapback at 0.1, cavern at 0.9. The core's mix
             is pinned fully wet so the slot mix does all blending -- which is
             what keeps mix = 0 a bit-exact passthrough. */
          auto &t = *timeFx[idx];
          const double wantMode = (s.type == FxType::Room) ? 1.0 : 0.0;
          if (t.p.mode != wantMode) t.setParam("mode", wantMode);   // no alloc
          /* Change-guarded: four of these rebuild the swarm inside setParam. */
          {
            auto &w = timeSet[idx]; auto &a = timeApplied[idx];
            if (w.size   != a.size)   { t.setParam("size", w.size);     a.size   = w.size; }
            if (w.spread != a.spread) { t.setParam("spread", w.spread); a.spread = w.spread; }
            if (w.taps   != a.taps)   { t.setParam("nb", w.taps);       a.taps   = w.taps; }
            if (w.dist   != a.dist)   { t.setParam("dist", w.dist);     a.dist   = w.dist; }
            if (w.damp   != a.damp)   { t.setParam("damp", w.damp);     a.damp   = w.damp; }
            if (w.noise  != a.noise)  { t.setParam("noise", w.noise);   a.noise  = w.noise; }
            if (w.stereo != a.stereo) { t.setParam("stereo", w.stereo); a.stereo = w.stereo; }
          }
          t.setParam("regen", s.amount);
          t.setParam("mix", 1.0);
          t.setParam("vol", 1.0);
          t.processExternalStereo(L, R, L, R, n);
          break;
        }
        case FxType::Comb:
        {
          // Polyphonic per-note Karplus-Strong comb (ADR-071): one tuned
          // feedback line per sounding note (fed by the shell's note events),
          // each resonating its own pitch out of the shared bus — the lab's
          // per-voice comb re-hosted bus-side, sympathetic-string style.
          // amount = wet mix; resonance fixed at the lab default (fb = 0.79,
          // damp 0.5) until the rack grows per-slot param pages.
          const double mix = s.amount;
          if (mix <= 0.001) break;
          int act = 0;
          for (auto &c : combs)
            if (c.key >= 0) act++;
          if (!act) break;
          // detune-lab law: fb = 0.6 + 0.38*resonance (res 0.5 -> 0.79, the
          // value ADR-071 hardcoded, so the default stays bit-identical).
          const double normTarget = 1.0 / act, fb = 0.6 + 0.38 * s.tone, damp = 0.5;
          for (int i = 0; i < n; i++)
          {
            const double l = L[i], r = R[i];
            double wl = 0, wr = 0;
            for (auto &c : combs)
            {
              if (c.key < 0) continue;
              const int len = (int)c.bufL.size();
              const int rd = (c.w - c.dly + len) % len;
              double dl = c.bufL[rd], dr = c.bufR[rd];
              c.lpL += (1 - damp) * (dl - c.lpL);
              dl = c.lpL;
              c.lpR += (1 - damp) * (dr - c.lpR);
              dr = c.lpR;
              // Feedback is taken pre-gain: the line keeps resonating while it
              // is faded out, so a retuned line returns already sounding
              // instead of restarting from nothing.
              c.bufL[c.w] = (float)(l + fb * dl);
              c.bufR[c.w] = (float)(r + fb * dr);
              c.w = (c.w + 1) % len;
              c.g += ((c.retuning ? 0.0 : 1.0) - c.g) * combRamp;
              if (c.retuning && c.g < 0.003)
              {
                // Retune at the BOTTOM of the ramp, and clear here rather than
                // in noteOn. Gating the output alone is not enough: the
                // read-pointer jump is a step in `dl`, which is written back
                // into the buffer ungated, so it recirculates every dly samples
                // and re-emerges once the ramp is back up — a click that is
                // merely LATE. Clearing while muted lets the line rebuild from
                // the bus, which is continuous, so no step exists to recirculate.
                c.dly = c.pendDly;
                c.retuning = false;
                std::memset(c.bufL.data(), 0, sizeof(float) * c.bufL.size());
                std::memset(c.bufR.data(), 0, sizeof(float) * c.bufR.size());
                c.w = 0;
                c.lpL = c.lpR = 0;
              }
              wl += dl * c.g;
              wr += dr * c.g;
            }
            normSm += (normTarget - normSm) * combNorm;
            L[i] = (float)(l * (1 - mix) + wl * normSm * mix);
            R[i] = (float)(r * (1 - mix) + wr * normSm * mix);
          }
          break;
        }
        case FxType::Delay:
        {
          /* `amount` -> FEEDBACK, for the same reason Echo maps it to regen:
             the slot's `mix` already owns wet/dry under the rack contract, and
             feedback is what changes the effect's identity (slapback at 0.05,
             runaway at 1.0). The per-slot page owns everything else. Feedback
             reaches 1.08 at amount 1 — past unity ON PURPOSE, bounded by the
             core's soft ceiling (L0-D4), which is the performance move a
             /N-normalised swarm delay cannot offer at any setting. */
          auto &d = *delayFx[idx];
          const bool retimed = d.p.timeMs != delaySet[idx].timeMs
                            || d.p.sync != delaySet[idx].sync
                            || d.p.timeBeats != delaySet[idx].timeBeats
                            || d.p.offsetR != delaySet[idx].offsetR;
          d.p = delaySet[idx];
          d.p.feedback = s.amount * 1.08;
          (void)retimed;   // a knob move GLIDES (tape retime) — snapping here
                           // would defeat the feature; snapTime is for loads,
                           // and the shell calls it when a slot is selected.
          d.processStereo(L, R, n);
          break;
        }
        case FxType::Notch:
        {
          // Swarm-herded notch cascade (notch_core.h), driven as a bus effect
          // via processExternal — NOT render(), which is the self-generating
          // exciter path used by the standalone NotchCore oracle. `amount`
          // drives the core's own dry/wet `mix` (cheap: `mix` is not one of
          // the keys that trigger rebuild() in NotchCore::setParam, so this is
          // a plain per-block store, not a rebuild). In place is safe: for
          // each sample processExternal reads inL/inR into `dry` BEFORE it
          // writes outL/outR, so L==inL and R==inR aliasing does not read a
          // value this same call already overwrote.
          notch[idx]->setParam("mix", s.amount);
          notch[idx]->processExternal(L, R, L, R, n);
          break;
        }
      }
    }
  }

 private:
  Slot slots[kRackSlots];
  /* B117: the OUTGOING module's record, preallocated per slot — never
     constructed on the audio thread. `fadeLeft > 0` is the only "is there a
     shadow" flag there is; at fxXfade = 0 it is always zero. */
  Slot shadow[kRackSlots];
  int fadeLeft[kRackSlots] = {0, 0, 0, 0};   // samples of handover remaining
  int fadeLen[kRackSlots] = {0, 0, 0, 0};    // samples the current handover spans
  bool xfadeOn = true;   // B117 ruled 2026-09-16 (ADR-163 A2): crossfade IS the behaviour                      // ADR-163 dev toggle; OFF is today
  double xfadeMs = 80;
  Comb combs[kCombLines];
  // One NotchCore per slot (main-thread constructed, see setSampleRate). A
  // slot never in Notch mode still owns a live core doing nothing — cheap
  // (a handful of doubles) and it keeps processSlot allocation-free forever,
  // rather than lazily constructing on first use from the audio thread.
  std::unique_ptr<NotchCore> notch[kRackSlots];
  std::unique_ptr<TimeCore> timeFx[kRackSlots];
  struct TimeSet { double size = 0.55, spread = 0.6, taps = 8, damp = 0.4,
                   noise = 0.2, stereo = 0.7, dist = 1; };
  TimeSet timeSet[kRackSlots];      // what the shell asked for
  TimeSet timeApplied[kRackSlots];  // what the core has been told
  /* ADR-142: the Delay's eight per-slot params. No change-guard twin like
     TimeSet's: DelayCore's setters rebuild nothing (its buffers are fixed and
     its coefficients are recomputed per block anyway), so the shell's values
     are simply copied in. A guard here would be ceremony protecting nothing. */
  std::unique_ptr<DelayCore> delayFx[kRackSlots];
  DelayCore::Params delaySet[kRackSlots];
  double rackTempo = 120.0;
  double sr = 44100;
  // Comp state — coefficients derived from SECONDS constants in setSampleRate
  // (ADR-009); these defaults are the 44.1 kHz values so a shell that never
  // calls setSampleRate still behaves.
  double compEnv = 0, compAtk = 0.3, compRel = 0.0015;
  // Comb declick state — 44.1 kHz defaults, re-derived in setSampleRate.
  double combRamp = 3.772e-3, combNorm = 7.556e-4, normSm = 1.0;
  long ageCounter = 0;
};

}  // namespace hypersaw
