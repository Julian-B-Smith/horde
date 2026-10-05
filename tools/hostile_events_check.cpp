/*
 * hostile_events_check — out-of-range host note and expression values are
 * handled at the event boundary, and the output stays finite (B446 P1).
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
 *   Every render also checks the oscillator's voice phases are finite.
 *
 * The rows are shell-level on purpose (L0031: a check that builds the core
 * directly gives the shell's path zero coverage) — only CONTROL is core-direct,
 * because showing the unguarded hazard is exactly what the shell must not allow.
 * WIRED: ./verify full.
 */

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
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
const double kInf = std::numeric_limits<double>::infinity();
const double kNaN = std::numeric_limits<double>::quiet_NaN();

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
  union { clap_event_header_t h; clap_event_note_t n; clap_event_note_expression_t x; clap_event_param_value_t p; };
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
  void note(uint16_t type, int key)
  {
    Ev e;
    std::memset(&e, 0, sizeof e);   // a union: zero every byte, not just the first member
    e.n.header = {sizeof(clap_event_note_t), 0, CLAP_CORE_EVENT_SPACE_ID, type, 0};
    e.n.note_id = -1;
    e.n.port_index = 0;
    e.n.channel = 0;
    e.n.key = (int16_t)key;
    e.n.velocity = 1.0;
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
};

/* A fresh instance with Roundness and Saw Base at 0.5 — the condition under
   which the voice phase indexes the anchor tables — then `first` delivered in
   block 0 and `second` in block 1, then silence to kBlocks. */
Render run(const EvList &first, const EvList &second)
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
  b0.list.ctx = &b0;
  b1.list.ctx = &b1;
  Render r;
  double ph[64];
  for (int blk = -1; blk < kBlocks; blk++)
  {
    proc.in_events = blk == -1 ? &setup.list : blk == 0 ? &b0.list : blk == 1 ? &b1.list : &none.list;
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

  if (g_fail)
  {
    std::printf("hostile_events_check: RED — %d row(s) failed\n", g_fail);
    return 1;
  }
  std::printf("hostile_events_check: GREEN\n");
  return 0;
}
