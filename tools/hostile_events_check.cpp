/*
 * hostile_events_check — out-of-range host note, expression, velocity and
 * tempo values are handled at the event boundary, and the output stays finite
 * (B446 P1; velocity and tempo B455).
 * WIRED: ./verify full.
 * (Declared up here because test_table_check reads only a file's first 40 lines.)
 *
 * WHY. CLAP defines a note key as 0..127 (-1 a wildcard on events that match
 * existing notes) and TUNING as -120..+120 semitones, but nothing enforced
 * either: the voice frequency is 440·2^((key-69)/12) and the tune factor
 * 2^(semis/12), so an extreme key or tuning made the frequency infinite and the
 * output non-finite. The guards (src/input_guards.h) sit in handleEvent: an
 * out-of-range note key is DROPPED, a non-finite expression value is DROPPED,
 * TUNING is clamped to ±120 and PRESSURE to 0..1. process() then zeroes any
 * non-finite output sample as defence in depth.
 *
 * Rows:
 *   CONTROL  the hazard is real where the boundary does not stand: SwarmCore
 *            driven DIRECTLY with an infinite frequency, or an infinite tune,
 *            renders non-finite samples with Roundness and Saw Base up. If it
 *            renders finite, the detector below is blind and the check is RED.
 *            zeroNonFinite replaces exactly the non-finite samples and leaves
 *            finite ones bit-identical.
 *   KEY      through the real CLAP process(): NOTE_ON with keys 20000, 32767,
 *            -5, -1, 128, -32768 renders finite output and allocates no voice;
 *            a NOTE_OFF with an out-of-range key releases nothing; the -1
 *            wildcard NOTE_OFF still releases the held note; keys 0 and 127
 *            still strike.
 *   EXPR     on a held note: TUNING ±inf / NaN and PRESSURE NaN / inf are
 *            dropped (the render equals the no-expression render bit for bit);
 *            TUNING 1e6 / 1e300 / -1e6 render exactly what ±120 renders; an
 *            in-range TUNING still takes effect; key 127 at +120 stays finite.
 *   VEL      (B455) note velocity is checked where it enters: NaN, +inf and -inf
 *            take the path velocity 0 takes (a fresh key strikes nothing, a held
 *            key is released, bit for bit the velocity-0 render); 0 and a negative
 *            keep that meaning; 7 and 1e300 render exactly what velocity 1 renders
 *            and matrix source 14 reads 1; 0.6 strikes, reads 0.6 exactly and
 *            renders differently from velocity 1 (the row that shows velocity
 *            reaches the render at all, so the equalities above are not blind).
 *            SUB On is set for these rows (and shown to sound): the sub is a
 *            second reader of velocity.
 *   TEMPO    (B455) host tempo is checked where it enters, through BOTH doors (a
 *            transport event, and the block's transport), under the tempo-grid
 *            detune law: NaN, +inf, -inf, 0, -97, the 1 BPM floor and 1e-310 are
 *            ignored (the render equals the no-transport render bit for bit, on
 *            a different valid tempo too: the last one is kept); 97 takes effect
 *            and both doors agree (the not-blind row); 120 sent explicitly equals
 *            the default; 1e300 is taken as sent and renders finite.
 *            hostTempoUsable, the function both doors call, is tabled directly.
 *   Every render also checks the oscillator's voice phases are finite.
 *   LATCH    (B448 B1) every run above left the output guard a count of 0 (finite
 *            output cannot tell "stopped at the boundary" from "repaired at the
 *            end"); then hypersaw_test_guard_output (the shipped guardOutput) gets
 *            a planted NaN, +Inf, -Inf: zeroed, counted, latched, latch survives
 *            clean blocks, reset clears. Faulty versions: tools/nan_latch_check.
 *
 * The rows are shell-level on purpose (L0031: a check that builds the core
 * directly gives the shell's path zero coverage) — only CONTROL is core-direct,
 * because showing the unguarded hazard is exactly what the shell must not allow.
 */

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include <clap/clap.h>

#include "../src/hypersaw_clap_entry.h"
#include "../src/hypersaw_debug.h"
#include "../src/input_guards.h"
#include "../src/swarm_core.h"

namespace
{
constexpr double kSR = 44100.0;
constexpr uint32_t kBlock = 256;
constexpr int kBlocks = 52;   // ~0.3 s: the NaN appears within the first block
constexpr clap_id kSawBase = 129, kRound = 131;
constexpr clap_id kLaw = 5;        // Detune Law; 3 = tempo-grid
constexpr clap_id kSubOn = 4015;   // SUB On, the SUB OSC source's gate (NOT id 52: that is SPECTRA's sub)
constexpr int kSrcVelocity = 14;           // matrix source slot: last note-on velocity
const double kInf = std::numeric_limits<double>::infinity();
const double kNaN = std::numeric_limits<double>::quiet_NaN();
const float kNaNf = std::numeric_limits<float>::quiet_NaN();
const float kInff = std::numeric_limits<float>::infinity();

int g_fail = 0;
void row(bool ok, const char *tag, const std::string &what)
{
  std::printf("%s  %-7s %s\n", ok ? "PASS" : "FAIL", tag, what.c_str());
  if (!ok) g_fail++;
}

const void *host_get_extension(const clap_host_t *, const char *) { return nullptr; }
void host_noop(const clap_host_t *) {}
const clap_host_t kHost = {CLAP_VERSION, nullptr, "hostile_events_check", "", "", "1.0",
                           host_get_extension, host_noop, host_noop, host_noop};
bool oev_try_push(const clap_output_events_t *, const clap_event_header_t *) { return true; }
const clap_output_events_t kOut = {nullptr, oev_try_push};

/* One block's events, in delivery order (all at time 0, which is legal: CLAP
   requires only non-decreasing times). A union-sized slot per event keeps the
   headers stable while the vector grows. */
struct Ev
{
  union
  {
    clap_event_header_t h;
    clap_event_note_t n;
    clap_event_note_expression_t x;
    clap_event_param_value_t p;
    clap_event_transport_t t;
  };
};
struct EvList
{
  clap_input_events_t list{};
  std::vector<Ev> evs;
  EvList()
  {
    list.ctx = this;
    list.size = [](const clap_input_events_t *l) -> uint32_t { return (uint32_t)((EvList *)l->ctx)->evs.size(); };
    list.get = [](const clap_input_events_t *l, uint32_t i) -> const clap_event_header_t * {
      return &((EvList *)l->ctx)->evs[i].h;
    };
  }
  void note(uint16_t type, int key, double velocity = 1.0)
  {
    Ev e;
    std::memset(&e, 0, sizeof e);   // a union: zero every byte, not just the first member
    e.n.header = {sizeof(clap_event_note_t), 0, CLAP_CORE_EVENT_SPACE_ID, type, 0};
    e.n.note_id = -1;
    e.n.port_index = 0;
    e.n.channel = 0;
    e.n.key = (int16_t)key;
    e.n.velocity = velocity;
    evs.push_back(e);
  }
  void expr(clap_note_expression id, int key, double v)
  {
    Ev e;
    std::memset(&e, 0, sizeof e);   // a union: zero every byte, not just the first member
    e.x.header = {sizeof(clap_event_note_expression_t), 0, CLAP_CORE_EVENT_SPACE_ID,
                  CLAP_EVENT_NOTE_EXPRESSION, 0};
    e.x.expression_id = id;
    e.x.note_id = -1;
    e.x.port_index = -1;
    e.x.channel = -1;
    e.x.key = (int16_t)key;
    e.x.value = v;
    evs.push_back(e);
  }
  void tempo(double bpm)
  {
    Ev e;
    std::memset(&e, 0, sizeof e);   // a union: zero every byte, not just the first member
    e.t.header = {sizeof(clap_event_transport_t), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_TRANSPORT, 0};
    e.t.flags = CLAP_TRANSPORT_HAS_TEMPO;
    e.t.tempo = bpm;
    evs.push_back(e);
  }
  void param(clap_id id, double v)
  {
    Ev e;
    std::memset(&e, 0, sizeof e);   // a union: zero every byte, not just the first member
    e.p.header = {sizeof(clap_event_param_value_t), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
    e.p.param_id = id;
    e.p.note_id = -1;
    e.p.port_index = -1;
    e.p.channel = -1;
    e.p.key = -1;
    e.p.value = v;
    evs.push_back(e);
  }
};

struct Render
{
  std::vector<float> out;   // interleaved L/R
  bool finite = true;       // every output sample and every voice phase
  int voices = 0;           // active note tags after the events
  bool gated60 = false;     // is any slot keyed to 60 still gated
  double srcVel = 0;        // matrix source 14 after block 1
};

/* What a run adds to the common setup. `blockTempo` is the SECOND door a tempo
   comes through: the block's own transport (clap_process.transport), which a
   host fills every block, here from block 1 on. The first door is a transport
   event (EvList::tempo). */
struct RunOpt
{
  std::vector<std::pair<clap_id, double>> params;   // applied with the setup block
  bool hasBlockTempo = false;
  double blockTempo = 0;
};

/* B448 B1: what the shell's last-line output guard saw, summed over every run().
   The event-boundary guards are meant to leave it NOTHING to catch — finite
   output alone cannot tell "stopped upstream" from "repaired at the end". */
uint64_t g_guardSamples = 0;
bool g_guardLatched = false;

/* A fresh instance with Roundness and Saw Base at 0.5 — the condition under
   which the voice phase indexes the anchor tables — then `first` delivered in
   block 0 and `second` in block 1, then silence to kBlocks. */
Render run(const EvList &first, const EvList &second, const RunOpt &opt = {})
{
  auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_t *p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
  p->init(p);
  p->activate(p, kSR, 32, kBlock);
  p->start_processing(p);
  std::vector<float> L(kBlock), R(kBlock);
  float *chans[2] = {L.data(), R.data()};
  clap_audio_buffer_t ob{};
  ob.data32 = chans;
  ob.channel_count = 2;
  clap_process_t proc{};
  proc.frames_count = kBlock;
  proc.audio_outputs = &ob;
  proc.audio_outputs_count = 1;
  proc.out_events = &kOut;

  EvList setup, b0 = first, b1 = second, none;
  setup.param(kRound, 0.5);
  setup.param(kSawBase, 0.5);
  for (const auto &kv : opt.params) setup.param(kv.first, kv.second);
  clap_event_transport_t tr{};
  tr.header = {sizeof(clap_event_transport_t), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_TRANSPORT, 0};
  tr.flags = CLAP_TRANSPORT_HAS_TEMPO;
  tr.tempo = opt.blockTempo;
  b0.list.ctx = &b0;
  b1.list.ctx = &b1;
  Render r;
  double ph[64];
  for (int blk = -1; blk < kBlocks; blk++)
  {
    proc.in_events = blk == -1 ? &setup.list : blk == 0 ? &b0.list : blk == 1 ? &b1.list : &none.list;
    proc.transport = opt.hasBlockTempo && blk >= 1 ? &tr : nullptr;
    p->process(p, &proc);
    if (blk < 0) continue;
    for (uint32_t i = 0; i < kBlock; i++)
    {
      r.out.push_back(L[i]);
      r.out.push_back(R[i]);
      if (!std::isfinite(L[i]) || !std::isfinite(R[i])) r.finite = false;
    }
    const int n = hypersaw_debug_phases(p, 0, ph, 64);
    for (int i = 0; i < n; i++)
      if (!std::isfinite(ph[i])) r.finite = false;
    if (blk == 1) r.srcVel = hypersaw_debug_modsrc(p, kSrcVelocity);
    if (blk == 1)
      for (int s = 0; s < hypersaw_test_poly(); s++)
      {
        int16_t key = 0;
        if (hypersaw_test_tag_at(p, s, nullptr, nullptr, nullptr, &key))
        {
          r.voices++;
          if (key == 60 && hypersaw_test_slot_gated(p, s)) r.gated60 = true;
        }
      }
  }
  g_guardSamples += hypersaw_test_nonfinite_samples(p);
  g_guardLatched = g_guardLatched || hypersaw_test_nonfinite_latched(p);
  p->stop_processing(p);
  p->deactivate(p);
  p->destroy(p);
  return r;
}

bool coreRendersNonFinite(double freq, double tuneSemis)
{
  hypersaw::SwarmCore c(kSR);
  c.setParam("round", 0.5);
  c.setParam("sawBase", 0.5);
  const int slot = c.noteOn(60, freq);
  if (tuneSemis != 0) c.setNoteExpr(slot, tuneSemis);
  std::vector<float> L(kBlock), R(kBlock);
  for (int b = 0; b < 8; b++)
  {
    c.render(L.data(), R.data(), (int)kBlock);
    for (uint32_t i = 0; i < kBlock; i++)
      if (!std::isfinite(L[i]) || !std::isfinite(R[i])) return true;
  }
  return false;
}

std::string num(double v)
{
  char b[32];
  std::snprintf(b, sizeof b, "%g", v);
  return b;
}
}  // namespace

int main()
{
  /* ---- CONTROL -------------------------------------------------------------- */
  {
    const bool a = coreRendersNonFinite(kInf, 0);
    const bool b = coreRendersNonFinite(440.0 * std::pow(2.0, (20000 - 69) / 12.0), 0);
    const bool c = coreRendersNonFinite(261.63, kInf);
    row(a, "CONTROL", "SwarmCore direct, frequency +inf: renders non-finite (the hazard is real)");
    row(b, "CONTROL", "SwarmCore direct, the frequency key 20000 maps to: renders non-finite");
    row(c, "CONTROL", "SwarmCore direct, tune +inf semitones: renders non-finite");
    row(!coreRendersNonFinite(261.63, 120.0), "CONTROL",
        "SwarmCore direct, tune +120 (the clamp's edge): renders finite");
    if (!(a && b && c))
    {
      std::printf("hostile_events_check: RED — a control did not trip, so the finiteness rows are blind\n");
      return 1;
    }
    float buf[6] = {0.25f, std::numeric_limits<float>::quiet_NaN(), -0.5f,
                    std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 1e-30f};
    const uint32_t hits = hypersaw::zeroNonFinite(buf, 6);
    row(hits == 3 && buf[0] == 0.25f && buf[1] == 0.0f && buf[2] == -0.5f && buf[3] == 0.0f &&
            buf[4] == 0.0f && buf[5] == 1e-30f,
        "CONTROL", "zeroNonFinite replaces exactly the 3 non-finite samples, finite ones untouched");
  }

  const EvList empty;
  EvList hold60;
  hold60.note(CLAP_EVENT_NOTE_ON, 60);
  const Render base = run(hold60, empty);
  row(base.finite && base.voices == 1 && base.gated60, "KEY", "baseline: key 60 strikes one voice, output finite");

  /* ---- KEY ------------------------------------------------------------------ */
  for (int key : {20000, 32767, -5, -1, 128, -32768})
  {
    EvList on;
    on.note(CLAP_EVENT_NOTE_ON, key);
    const Render r = run(on, empty);
    row(r.finite && r.voices == 0, "KEY",
        "NOTE_ON key " + std::to_string(key) + ": output finite, no voice allocated (voices " +
            std::to_string(r.voices) + ", finite " + (r.finite ? "yes" : "NO") + ")");
  }
  for (int key : {20000, -5})
  {
    EvList off;
    off.note(CLAP_EVENT_NOTE_OFF, key);
    const Render r = run(hold60, off);
    row(r.finite && r.gated60, "KEY", "NOTE_OFF key " + std::to_string(key) + ": ignored, key 60 still held");
  }
  {
    EvList off;
    off.note(CLAP_EVENT_NOTE_OFF, -1);
    const Render r = run(hold60, off);
    row(r.finite && !r.gated60, "KEY", "NOTE_OFF key -1 (CLAP wildcard): still releases key 60");
  }
  for (int key : {0, 127})
  {
    EvList on;
    on.note(CLAP_EVENT_NOTE_ON, key);
    const Render r = run(on, empty);
    row(r.finite && r.voices == 1, "KEY", "NOTE_ON key " + std::to_string(key) + " (valid edge): strikes, finite");
  }

  /* ---- EXPR ----------------------------------------------------------------- */
  auto withExpr = [&](clap_note_expression id, int key, double v, int onKey = 60) {
    EvList on, x;
    on.note(CLAP_EVENT_NOTE_ON, onKey);
    x.expr(id, key, v);
    return run(on, x);
  };
  for (double v : {kInf, -kInf, kNaN})
  {
    const Render r = withExpr(CLAP_NOTE_EXPRESSION_TUNING, 60, v);
    row(r.finite && r.out == base.out, "EXPR", "TUNING " + num(v) + ": dropped (render == no-expression render)");
  }
  for (double v : {kNaN, kInf})
  {
    const Render r = withExpr(CLAP_NOTE_EXPRESSION_PRESSURE, 60, v);
    row(r.finite && r.out == base.out, "EXPR", "PRESSURE " + num(v) + ": dropped (render == no-expression render)");
  }
  {
    const Render up = withExpr(CLAP_NOTE_EXPRESSION_TUNING, 60, 120.0);
    const Render dn = withExpr(CLAP_NOTE_EXPRESSION_TUNING, 60, -120.0);
    for (double v : {1e6, 1e300})
    {
      const Render r = withExpr(CLAP_NOTE_EXPRESSION_TUNING, 60, v);
      row(r.finite && r.out == up.out, "EXPR", "TUNING " + num(v) + ": renders exactly what +120 renders");
    }
    const Render r = withExpr(CLAP_NOTE_EXPRESSION_TUNING, 60, -1e6);
    row(r.finite && r.out == dn.out, "EXPR", "TUNING -1e+06: renders exactly what -120 renders");
    const Render mid = withExpr(CLAP_NOTE_EXPRESSION_TUNING, 60, 7.25);
    row(mid.finite && mid.out != base.out && mid.out != up.out, "EXPR",
        "TUNING 7.25 (in range): takes effect, unclamped");
    const Render top = withExpr(CLAP_NOTE_EXPRESSION_TUNING, 127, 120.0, 127);
    row(top.finite, "EXPR", "key 127 at TUNING +120 (the extreme valid input): output finite");
  }

  /* ---- VEL (B455) ----------------------------------------------------------- */
  {
    RunOpt sub;
    sub.params = {{kSubOn, 1.0}};   // the Sub Osc reads velocity too
    auto strike = [&](double vel) {
      EvList on;
      on.note(CLAP_EVENT_NOTE_ON, 60, vel);
      return run(on, empty, sub);
    };
    auto onHeld = [&](double vel) {   // a second NOTE_ON for the key block 0 struck
      EvList on;
      on.note(CLAP_EVENT_NOTE_ON, 60, vel);
      return run(hold60, on, sub);
    };
    const Render full = strike(1.0), silent = run(empty, empty, sub), held0 = onHeld(0.0);
    row(full.finite && full.voices == 1 && full.srcVel == 1.0 && full.out != silent.out, "VEL",
        "baseline: velocity 1 strikes one voice, source 14 reads 1, the render is not silence");
    row(full.out != base.out, "VEL",
        "baseline: SUB On changes the velocity-1 render (the sub is sounding in these rows)");
    row(held0.finite && !held0.gated60 && held0.out != full.out, "VEL",
        "baseline: velocity 0 on a held key releases it (the meaning this site already gives it)");
    const Render part = strike(0.6);
    row(part.finite && part.voices == 1 && part.srcVel == 0.6 && part.out != full.out && part.out != silent.out,
        "VEL", "velocity 0.6 (in range): strikes, source 14 reads 0.6 exactly, render != the velocity-1 render");
    for (double v : {kNaN, kInf, -kInf, 0.0, -0.5})
    {
      const Render a = strike(v), b = onHeld(v);
      row(a.finite && a.voices == 0 && a.srcVel == 0.0 && a.out == silent.out, "VEL",
          "velocity " + num(v) + " on a fresh key: strikes nothing, source 14 stays 0, the render is silence");
      row(b.finite && !b.gated60 && b.out == held0.out, "VEL",
          "velocity " + num(v) + " on a held key: render == the velocity-0 render");
    }
    for (double v : {7.0, 1e300})
    {
      const Render r = strike(v);
      row(r.finite && r.voices == 1 && r.srcVel == 1.0 && r.out == full.out, "VEL",
          "velocity " + num(v) + ": renders exactly what velocity 1 renders, source 14 reads 1");
    }
  }

  /* ---- TEMPO (B455) --------------------------------------------------------- */
  {
    for (double v : {kNaN, kInf, -kInf, 0.0, -97.0, 0.5, 1.0, 1e-310})
      row(!hypersaw::hostTempoUsable(v), "TEMPO", "hostTempoUsable(" + num(v) + ") is false");
    for (double v : {1.5, 20.0, 97.0, 120.0, 999.0, 1e300})
      row(hypersaw::hostTempoUsable(v), "TEMPO", "hostTempoUsable(" + num(v) + ") is true");

    /* The tempo-grid detune law is what reads the tempo in the oscillator, so
       it is on for every render here. `firstBpm`, when given, is a valid tempo
       delivered by event in block 0, ahead of the tempo under test. */
    RunOpt grid;
    grid.params = {{kLaw, 3.0}};
    auto block0 = [&](double firstBpm) {
      EvList on;
      if (firstBpm != 0) on.tempo(firstBpm);
      on.note(CLAP_EVENT_NOTE_ON, 60);
      return on;
    };
    auto byEvent = [&](double bpm, double firstBpm = 0) {
      EvList t;
      t.tempo(bpm);
      return run(block0(firstBpm), t, grid);
    };
    auto byBlock = [&](double bpm, double firstBpm = 0) {
      RunOpt o = grid;
      o.hasBlockTempo = true;
      o.blockTempo = bpm;
      return run(block0(firstBpm), empty, o);
    };
    const Render none = run(block0(0), empty, grid);     // no transport at all: the 120 default
    const Render only97 = run(block0(97.0), empty, grid);   // 97 in block 0, nothing after
    const Render e97 = byEvent(97.0), b97 = byBlock(97.0);
    row(none.finite && none.voices == 1 && none.out != base.out, "TEMPO",
        "baseline: the tempo-grid law renders finite, and differently from the default law");
    row(e97.finite && e97.out != none.out && only97.finite && only97.out != none.out, "TEMPO",
        "tempo 97 by event: takes effect (render != the no-transport render)");
    row(b97.finite && b97.out == e97.out, "TEMPO",
        "tempo 97 by the block's transport: the same render as by event");
    row(byEvent(120.0).out == none.out && byBlock(120.0).out == none.out, "TEMPO",
        "tempo 120 sent by either door: render == the no-transport render (120 is the default)");
    for (double v : {kNaN, kInf, -kInf, 0.0, -97.0, 1.0, 1e-310})
    {
      const Render e = byEvent(v), b = byBlock(v);
      row(e.finite && e.out == none.out, "TEMPO",
          "tempo " + num(v) + " by event: ignored (render == the no-transport render)");
      row(b.finite && b.out == none.out, "TEMPO",
          "tempo " + num(v) + " by the block's transport: ignored (render == the no-transport render)");
      const Render ek = byEvent(v, 97.0), bk = byBlock(v, 97.0);
      row(ek.finite && bk.finite && ek.out == only97.out && bk.out == only97.out, "TEMPO",
          "tempo " + num(v) + " after 97, either door: 97 is kept (render == the 97-only render)");
    }
    {
      const Render e = byEvent(1e300), b = byBlock(1e300);
      row(e.finite && b.finite && e.out == b.out && e.out != none.out, "TEMPO",
          "tempo 1e+300, either door: taken as sent (no upper bound is defined), renders finite");
    }
  }

  /* ---- LATCH (B448 B1) ------------------------------------------------------ */
  row(g_guardSamples == 0 && !g_guardLatched, "LATCH",
      "every hostile run above: the output guard replaced 0 samples and never latched "
      "(the boundary guards stop it upstream) — got " + std::to_string(g_guardSamples));
  {
    /* The only way to put a non-finite sample on the bus now is to plant one: run
       the shell's own guard (Plugin::guardOutput, the call process() ends with)
       on a buffer, then keep rendering through the real process(). */
    auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    const clap_plugin_t *p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
    p->init(p);
    p->activate(p, kSR, 32, kBlock);
    p->start_processing(p);
    row(hypersaw_test_nonfinite_samples(p) == 0 && hypersaw_test_nonfinite_blocks(p) == 0 &&
            !hypersaw_test_nonfinite_latched(p),
        "LATCH", "a fresh plugin reads 0 / 0 / unlatched");
    float l[8] = {0.5f, kNaNf, -0.25f, 0, 0, 0, 0, 0}, r[8] = {0, 0, 0, kInff, -kInff, 0, 0, 0.125f};
    const uint32_t hit = hypersaw_test_guard_output(p, l, r, 8);
    row(hit == 3 && l[1] == 0.0f && r[3] == 0.0f && r[4] == 0.0f && l[0] == 0.5f && l[2] == -0.25f &&
            r[7] == 0.125f,
        "LATCH", "the shell's guard zeroes planted NaN, +Inf, -Inf and touches nothing else");
    row(hypersaw_test_nonfinite_samples(p) == 3 && hypersaw_test_nonfinite_blocks(p) == 1 &&
            hypersaw_test_nonfinite_latched(p),
        "LATCH", "...and the shell COUNTED them (3 samples, 1 block) and latched");
    std::vector<float> L(kBlock), R(kBlock);
    float *chans[2] = {L.data(), R.data()};
    clap_audio_buffer_t ob{};
    ob.data32 = chans;
    ob.channel_count = 2;
    clap_process_t proc{};
    proc.frames_count = kBlock;
    proc.audio_outputs = &ob;
    proc.audio_outputs_count = 1;
    proc.out_events = &kOut;
    EvList none;
    proc.in_events = &none.list;
    for (int i = 0; i < 4; i++) p->process(p, &proc);
    row(hypersaw_test_nonfinite_latched(p) && hypersaw_test_nonfinite_samples(p) == 3, "LATCH",
        "four clean process() blocks later: still latched, count unchanged");
    hypersaw_test_nonfinite_reset(p);
    row(!hypersaw_test_nonfinite_latched(p) && hypersaw_test_nonfinite_samples(p) == 0 &&
            hypersaw_test_nonfinite_blocks(p) == 0,
        "LATCH", "reset clears the latch and the counts");
    p->stop_processing(p);
    p->deactivate(p);
    p->destroy(p);
  }

  if (g_fail)
  {
    std::printf("hostile_events_check: RED — %d row(s) failed\n", g_fail);
    return 1;
  }
  std::printf("hostile_events_check: GREEN\n");
  return 0;
}
