/*
 * hostile_events_check — out-of-range host note, expression, velocity and
 * tempo values are handled at the event boundary, and the output stays finite
 * (B446 P1; velocity and tempo B455; event size, sample rate, output buffers
 * and MIDI data bytes B455).
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
 *   VEL      (B455) note velocity is checked where it enters: a NOTE_ON with NaN,
 *            +inf or -inf is DROPPED (a fresh key strikes nothing; a held key
 *            stays held, bit for bit the untouched render); 0 and a negative keep
 *            their meaning, the release; 7 and 1e300 render what velocity 1 renders
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
 *   SIZE     (B455) an event is checked before it is read, through BOTH doors
 *            (process and params.flush): a NOTE_ON whose size says "a header
 *            only", with a whole valid note behind it, strikes nothing; so does
 *            a size of 0; an absent event (a null pointer from the list) is
 *            passed over and the valid note after it strikes. Each refusal is
 *            counted. The same header in a heap block of exactly its claimed
 *            size is refused too (the row a sanitizer build reads). A flush
 *            takes its valid value and refuses the two beside it; a flush with
 *            no list at all does nothing.
 *   RATE     (B455) the sample rate is checked at activate against the two
 *            bounds in input_guards.h (read from the header, never restated
 *            here): 0, a negative, NaN, the infinities, far past the upper
 *            bound, half the lower one, and one step outside either bound are
 *            refused (activate returns false) and leave the instance as it
 *            was; the bounds and the usual rates inside them activate and
 *            render a note, finite. An instance whose activation was refused
 *            and which is processed anyway renders exact silence, fresh or
 *            holding a sounding note, and plays again once an activation is
 *            taken. hostSampleRateUsable is tabled directly.
 *   OUT      (B455) a block with nowhere to render — a null channel, no 32-bit
 *            buffers, one channel, no bus — is refused (CLAP_PROCESS_ERROR),
 *            writes nothing, and the blocks after it render exactly what they
 *            render when no such block was sent. Its events are kept: a
 *            NOTE_OFF sent in it ends the note at the next block that renders.
 *            With no out-event list a note end stays pending and is sent when
 *            a list is back. A block processed while a load owns the state is
 *            silence, with or without an output list to write it to.
 *   MIDI     (B455) a MIDI data byte is 7 bits: CC1, channel pressure and
 *            pitch bend (wheel and member-channel) with the top bit set in a
 *            data byte, a CC number byte included, are dropped and counted;
 *            the in-range rows show each reaches its reader, and the controls
 *            show a byte that is not one of the message's is not asked.
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

#include <algorithm>
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
constexpr int kSrcWheel = 15, kSrcPressure = 16;   // CC1 and channel pressure (ADR-149)
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

/* The host. It offers the params extension only for request_flush, which the
   shell calls synchronously on a load's first enqueue: the seam the OUT rows
   use to process a block while a load owns the state. */
void (*g_onFlush)() = nullptr;   // run ONCE, on the next request_flush, then cleared
int g_noteEnds = 0;              // NOTE_END events the plugin pushed
void hp_rescan(const clap_host_t *, clap_param_rescan_flags) {}
void hp_clear(const clap_host_t *, clap_id, clap_param_clear_flags) {}
void hp_request_flush(const clap_host_t *)
{
  if (!g_onFlush) return;
  void (*f)() = g_onFlush;
  g_onFlush = nullptr;
  f();
}
const clap_host_params_t kHostParams = {hp_rescan, hp_clear, hp_request_flush};
const void *host_get_extension(const clap_host_t *, const char *id)
{
  return std::strcmp(id, CLAP_EXT_PARAMS) == 0 ? &kHostParams : nullptr;
}
void host_noop(const clap_host_t *) {}
const clap_host_t kHost = {CLAP_VERSION, nullptr, "hostile_events_check", "", "", "1.0",
                           host_get_extension, host_noop, host_noop, host_noop};
bool oev_try_push(const clap_output_events_t *, const clap_event_header_t *e)
{
  if (e->type == CLAP_EVENT_NOTE_END) g_noteEnds++;
  return true;
}
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
    clap_event_midi_t m;
  };
};
struct EvList
{
  clap_input_events_t list{};
  std::vector<Ev> evs;
  /* What the list hands the plugin for entry i, when it is not &evs[i]: an
     absent event (null), or a header that lives in a block of its own. */
  struct Instead { bool set = false; const clap_event_header_t *p = nullptr; };
  std::vector<Instead> instead;
  EvList()
  {
    list.ctx = this;
    list.size = [](const clap_input_events_t *l) -> uint32_t { return (uint32_t)((EvList *)l->ctx)->evs.size(); };
    list.get = [](const clap_input_events_t *l, uint32_t i) -> const clap_event_header_t * {
      auto *s = (EvList *)l->ctx;
      if (i < s->instead.size() && s->instead[i].set) return s->instead[i].p;
      return &s->evs[i].h;
    };
  }
  void handBack(const clap_event_header_t *p)   // entry: this pointer (may be null), not a slot
  {
    Ev e;
    std::memset(&e, 0, sizeof e);
    evs.push_back(e);
    instead.resize(evs.size());
    instead.back() = {true, p};
  }
  void note(uint16_t type, int key, double velocity = 1.0, int channel = 0)
  {
    Ev e;
    std::memset(&e, 0, sizeof e);   // a union: zero every byte, not just the first member
    e.n.header = {sizeof(clap_event_note_t), 0, CLAP_CORE_EVENT_SPACE_ID, type, 0};
    e.n.note_id = -1;
    e.n.port_index = 0;
    e.n.channel = (int16_t)channel;
    e.n.key = (int16_t)key;
    e.n.velocity = velocity;
    evs.push_back(e);
  }
  void midi(int status, int d1, int d2)
  {
    Ev e;
    std::memset(&e, 0, sizeof e);
    e.m.header = {sizeof(clap_event_midi_t), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI, 0};
    e.m.port_index = 0;
    e.m.data[0] = (uint8_t)status;
    e.m.data[1] = (uint8_t)d1;
    e.m.data[2] = (uint8_t)d2;
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
  double srcWheel = 0, srcPress = 0;   // matrix sources 15 and 16 after block 1
  uint32_t refused = 0;     // host events the shell did not take, over the whole run
  bool activated = false;   // the activation at opt.sampleRate
  bool firstRefused = false;   // opt.refusedRate: activate returned false
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
  bool hasRate = false;         // activate at sampleRate, not kSR
  double sampleRate = 0;
  bool hasRefusedRate = false;  // activate at refusedRate FIRST (it must return false), then at sampleRate
  double refusedRate = 0;
};

/* B448 B1: what the shell's last-line output guard saw, summed over every run().
   The event-boundary guards are meant to leave it NOTHING to catch — finite
   output alone cannot tell "stopped upstream" from "repaired at the end". */
uint64_t g_guardSamples = 0;
bool g_guardLatched = false;

// Host events the shell did not take (absent, undersized, or no room to defer).
uint32_t refusedEvents(const clap_plugin_t *p)
{
  uint32_t n = 0;
  hypersaw_debug_handoff_stats(p, nullptr, nullptr, nullptr, &n, nullptr);
  return n;
}

/* A fresh instance with Roundness and Saw Base at 0.5 — the condition under
   which the voice phase indexes the anchor tables — then `first` delivered in
   block 0 and `second` in block 1, then silence to kBlocks. */
Render run(const EvList &first, const EvList &second, const RunOpt &opt = {})
{
  auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_t *p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
  p->init(p);
  Render r;
  if (opt.hasRefusedRate) r.firstRefused = !p->activate(p, opt.refusedRate, 32, kBlock);
  r.activated = p->activate(p, opt.hasRate ? opt.sampleRate : kSR, 32, kBlock);
  if (!r.activated)
  {
    p->destroy(p);
    return r;
  }
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
    if (blk == 1) r.srcWheel = hypersaw_debug_modsrc(p, kSrcWheel);
    if (blk == 1) r.srcPress = hypersaw_debug_modsrc(p, kSrcPressure);
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
  r.refused = refusedEvents(p);
  p->stop_processing(p);
  p->deactivate(p);
  p->destroy(p);
  return r;
}

// What a row fills a buffer with before a block, to see whether the block wrote it.
const float kMark = 0.5f;

/* One instance driven block by block, for the rows that need more than run()'s
   fixed schedule: its own activation, blocks with other buffers or no event
   lists, and reads between blocks. The output guard's counters join the LATCH
   row's sum when it goes. */
struct Rig
{
  const clap_plugin_t *p = nullptr;
  bool activated = false;
  std::vector<float> L = std::vector<float>(kBlock), R = std::vector<float>(kBlock);
  float *chans[2] = {nullptr, nullptr};
  clap_audio_buffer_t ob{};
  clap_process_t proc{};
  std::vector<float> out;   // every block rendered through block(), interleaved
  bool finite = true;
  EvList none;

  explicit Rig(double rate = kSR, uint32_t maxFrames = kBlock)
  {
    auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
    p->init(p);
    activated = p->activate(p, rate, 32, maxFrames);
    plainBuffers();
    proc.frames_count = kBlock;
    proc.out_events = &kOut;
  }
  Rig(const Rig &) = delete;
  Rig &operator=(const Rig &) = delete;
  ~Rig()
  {
    g_guardSamples += hypersaw_test_nonfinite_samples(p);
    g_guardLatched = g_guardLatched || hypersaw_test_nonfinite_latched(p);
    p->stop_processing(p);
    p->deactivate(p);
    p->destroy(p);
  }
  void plainBuffers()
  {
    chans[0] = L.data();
    chans[1] = R.data();
    ob.data32 = chans;
    ob.channel_count = 2;
    proc.audio_outputs = &ob;
    proc.audio_outputs_count = 1;
  }
  void mark(float v)
  {
    std::fill(L.begin(), L.end(), v);
    std::fill(R.begin(), R.end(), v);
  }
  bool holds(float v) const
  {
    for (uint32_t i = 0; i < kBlock; i++)
      if (L[i] != v || R[i] != v) return false;
    return true;
  }
  // One block as the fields stand; a rendered block's samples join `out`.
  clap_process_status block(const EvList &ev, bool keep = true)
  {
    proc.in_events = &ev.list;
    const clap_process_status st = p->process(p, &proc);
    for (uint32_t i = 0; keep && i < kBlock; i++)
    {
      out.push_back(L[i]);
      out.push_back(R[i]);
      if (!std::isfinite(L[i]) || !std::isfinite(R[i])) finite = false;
    }
    return st;
  }
  bool gated(int key) const
  {
    for (int s = 0; s < hypersaw_test_poly(); s++)
    {
      int16_t k = 0;
      if (hypersaw_test_tag_at(p, s, nullptr, nullptr, nullptr, &k) && k == key && hypersaw_test_slot_gated(p, s))
        return true;
    }
    return false;
  }
  bool sounds() const
  {
    for (float v : out)
      if (v != 0.0f) return true;
    return false;
  }
};

// The block the OWNED rows process from inside a load (the request_flush seam).
Rig *g_ownedRig = nullptr;
int g_ownedShape = 0;   // 0 plain buffers, 1 no output list (count 1), 2 no 32-bit buffers
bool g_ownedSilent = false;
void ownedBlock()
{
  Rig &r = *g_ownedRig;
  r.p->start_processing(r.p);
  EvList on;
  on.note(CLAP_EVENT_NOTE_ON, 60);
  r.plainBuffers();
  r.mark(kMark);
  if (g_ownedShape == 1) r.proc.audio_outputs = nullptr;
  if (g_ownedShape == 2) r.ob.data32 = nullptr;
  r.block(on, /*keep=*/false);
  g_ownedSilent = g_ownedShape == 0 ? r.holds(0.0f) : r.holds(kMark);   // zeroed, or (nowhere to write) untouched
  r.plainBuffers();
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
    /* Not finite: the NOTE_ON is dropped. `full` is also the render of key 60
       struck in block 0 and left alone, so equality with it on a held key means
       the event did nothing at all: no release (held0 differs from full, the
       row above), no second strike. */
    for (double v : {kNaN, kInf, -kInf})
    {
      const Render a = strike(v), b = onHeld(v);
      row(a.finite && a.voices == 0 && a.srcVel == 0.0 && a.out == silent.out, "VEL",
          "velocity " + num(v) + " on a fresh key: dropped (no voice, source 14 stays 0, the render is silence)");
      row(b.finite && b.gated60 && b.voices == 1 && b.srcVel == 1.0 && b.out == full.out, "VEL",
          "velocity " + num(v) + " on a held key: dropped (key 60 still held, render == the untouched held render)");
    }
    for (double v : {0.0, -0.5})
    {
      const Render a = strike(v), b = onHeld(v);
      row(a.finite && a.voices == 0 && a.srcVel == 0.0 && a.out == silent.out, "VEL",
          "velocity " + num(v) + " on a fresh key: strikes nothing, source 14 stays 0, the render is silence");
      row(b.finite && !b.gated60 && b.out == held0.out, "VEL",
          "velocity " + num(v) + " on a held key: releases it (render == the velocity-0 render)");
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

  /* ---- SIZE (B455) ---------------------------------------------------------- */
  {
    const Render silent = run(empty, empty);
    row(base.refused == 0 && base.out != silent.out, "SIZE",
        "baseline: a whole NOTE_ON is taken (0 refused) and the render is not silence");
    /* A header that claims `size` bytes, with a WHOLE valid note-on for key 60
       behind it in the slot: read past its claim, it would strike. */
    auto claimed = [&](uint32_t size) {
      EvList on;
      on.note(CLAP_EVENT_NOTE_ON, 60);
      on.evs.back().h.size = size;
      return run(on, empty);
    };
    const uint32_t headerOnly = (uint32_t)sizeof(clap_event_header_t);
    for (uint32_t size : {headerOnly, headerOnly + 4u, 0u, 1u})
    {
      const Render r = claimed(size);
      row(r.finite && r.voices == 0 && r.refused == 1 && r.out == silent.out, "SIZE",
          "NOTE_ON claiming " + std::to_string(size) + " byte(s) (a note is " +
              std::to_string(sizeof(clap_event_note_t)) + "): refused and counted (" + std::to_string(r.refused) +
              "), no voice, the render is silence");
    }
    {
      const Render r = claimed((uint32_t)sizeof(Ev));   // a LARGER claim is legal: the struct is all that is read
      row(r.finite && r.voices == 1 && r.refused == 0 && r.out == base.out, "SIZE",
          "NOTE_ON claiming more than a note: taken, render == the baseline");
    }
    {
      EvList on;
      on.handBack(nullptr);
      on.note(CLAP_EVENT_NOTE_ON, 60);
      on.handBack(nullptr);
      const Render r = run(on, empty);
      row(r.finite && r.voices == 1 && r.refused == 2 && r.out == base.out, "SIZE",
          "two absent events around a valid NOTE_ON: passed over and counted (" + std::to_string(r.refused) +
              "), the note strikes, render == the baseline");
    }
    {
      /* The header alone, in a block of exactly the bytes it claims. Nothing
         past them exists, which is what an address-sanitized build checks. */
      std::vector<unsigned char> exact(sizeof(clap_event_header_t));
      clap_event_header_t h = {(uint32_t)sizeof(clap_event_header_t), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_NOTE_ON, 0};
      std::memcpy(exact.data(), &h, sizeof h);
      EvList on;
      on.handBack((const clap_event_header_t *)exact.data());
      const Render r = run(on, empty);
      row(r.finite && r.voices == 0 && r.refused == 1 && r.out == silent.out, "SIZE",
          "a NOTE_ON header in a block of exactly its claimed size: refused and counted, nothing past it read");
    }
    /* The other door: params.flush, on an activated instance that is not processing. */
    {
      auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
      const clap_plugin_t *p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
      p->init(p);
      p->activate(p, kSR, 32, kBlock);
      auto *params = (const clap_plugin_params_t *)p->get_extension(p, CLAP_EXT_PARAMS);
      double round0 = 0, saw0 = 0, round1 = 0, saw1 = 0;
      params->get_value(p, kRound, &round0);
      params->get_value(p, kSawBase, &saw0);
      const double sent = 0.25, unsent = 0.75;
      EvList ev;
      ev.handBack(nullptr);
      ev.param(kRound, unsent);
      ev.evs.back().h.size = headerOnly;   // a value event that claims a header only
      ev.param(kSawBase, sent);
      params->flush(p, &ev.list, &kOut);
      params->get_value(p, kRound, &round1);
      params->get_value(p, kSawBase, &saw1);
      const uint32_t refused = refusedEvents(p);
      row(refused == 2 && round1 == round0 && round1 != unsent && saw1 == sent && saw0 != sent, "SIZE",
          "flush with an absent event, an undersized value and a valid value: " + std::to_string(refused) +
              " refused, the undersized one left its parameter at " + num(round1) + ", the valid one landed (" +
              num(saw1) + ")");
      params->flush(p, nullptr, &kOut);
      row(refusedEvents(p) == refused, "SIZE", "flush with no event list: nothing read, nothing counted");
      p->deactivate(p);
      p->destroy(p);
    }
  }

  /* ---- RATE (B455) ---------------------------------------------------------- */
  {
    /* Every rate here is derived from the two bounds in input_guards.h, so the
       rows follow the bounds and pin no number of their own. */
    const double lo = hypersaw::kHostSampleRateMin, hi = hypersaw::kHostSampleRateMax;
    const double under = std::nextafter(lo, 0.0), over = std::nextafter(hi, kInf);
    const std::vector<double> refusedRates = {0.0, -kSR, kNaN, kInf, -kInf, hi * 1000, lo / 2, 5e-324, under, over};
    std::vector<double> takenRates = {lo, hi};
    for (double usual : {22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0, 384000.0})
      if (hypersaw::hostSampleRateUsable(usual) && usual != lo && usual != hi) takenRates.push_back(usual);
    auto rate = [&](double v) {   // %g prints the two neighbours as the bounds themselves
      return v == under ? "one step under " + num(lo) : v == over ? "one step over " + num(hi) : num(v);
    };
    row(lo > 0.0 && lo < kSR && kSR < hi && takenRates.size() > 2, "RATE",
        "the bounds are " + num(lo) + " and " + num(hi) + " Hz (input_guards.h); " + num(kSR) + " is inside them");
    for (double v : refusedRates)
      row(!hypersaw::hostSampleRateUsable(v), "RATE", "hostSampleRateUsable(" + rate(v) + ") is false");
    for (double v : takenRates)
      row(hypersaw::hostSampleRateUsable(v), "RATE", "hostSampleRateUsable(" + rate(v) + ") is true");
    for (double v : refusedRates)
    {
      RunOpt o;
      o.hasRefusedRate = true;
      o.refusedRate = v;
      const Render r = run(hold60, empty, o);
      row(r.firstRefused && r.activated && r.finite && r.out == base.out, "RATE",
          "activate at " + rate(v) + ": refused (" + (r.firstRefused ? "false" : "TRUE") +
              "), and the render after a taken activation == the render with no refusal");
      RunOpt only;
      only.hasRate = true;
      only.sampleRate = v;
      const Render alone = run(hold60, empty, only);
      row(!alone.activated, "RATE", "activate at " + rate(v) + " alone: the instance is not activated");
    }
    const Render silent = run(empty, empty);
    for (double v : takenRates)
    {
      RunOpt o;
      o.hasRate = true;
      o.sampleRate = v;
      const Render r = run(hold60, empty, o), quiet = run(empty, empty, o);
      row(r.activated && r.finite && r.voices == 1 && r.out != quiet.out && quiet.out == silent.out, "RATE",
          "activate at " + rate(v) + ": taken, key 60 strikes one voice, the render is finite and not silence");
    }

    /* A REFUSED ACTIVATION, THEN process() ANYWAY (a host or wrapper that does
       not read activate's result). The cores were not rebuilt for the rate the
       host is running at, so the blocks are silence: the buffers are written
       with zeros, whatever is sent. `soundFirst`: the instance was activated
       properly and holds a sounding note when the refused activation comes. */
    auto refusedThenProcess = [&](double badRate, uint32_t maxFrames, bool soundFirst, const std::string &what) {
      Rig g(soundFirst ? kSR : badRate, soundFirst ? kBlock : maxFrames);
      EvList on;
      on.note(CLAP_EVENT_NOTE_ON, 60);
      bool sounded = true, refused = !g.activated;
      if (soundFirst)
      {
        g.p->start_processing(g.p);
        g.block(on);
        g.block(g.none);
        sounded = g.sounds();
        g.p->stop_processing(g.p);
        g.p->deactivate(g.p);
        refused = !g.p->activate(g.p, badRate, 32, maxFrames);
      }
      g.p->start_processing(g.p);
      bool zeroed = true;
      int errors = 0;
      for (int b = 0; b < 6; b++)
      {
        g.mark(kMark);
        if (g.block(b == 0 ? on : g.none, /*keep=*/false) == CLAP_PROCESS_ERROR) errors++;
        zeroed = zeroed && g.holds(0.0f);
      }
      g.p->stop_processing(g.p);
      // ...and a taken activation afterwards plays again.
      const bool taken = g.p->activate(g.p, kSR, 32, kBlock);
      g.p->start_processing(g.p);
      g.out.clear();
      g.block(on);
      g.block(g.none);
      row(refused && sounded && zeroed && errors == 6 && taken && g.sounds() && g.finite, "RATE",
          what + ": refused, then 6 blocks processed anyway are exact silence (" + std::to_string(errors) +
              " refused with CLAP_PROCESS_ERROR); a taken activation afterwards renders a note, finite");
    };
    for (double v : {0.0, kNaN, under, over})
    {
      refusedThenProcess(v, kBlock, false, "a fresh instance, activate at " + rate(v));
      refusedThenProcess(v, kBlock, true, "an instance holding a sounding note, re-activated at " + rate(v));
    }
    refusedThenProcess(kSR, 1u << 30, true, "an instance holding a sounding note, re-activated with a block size past the ceiling");
  }

  /* ---- OUT (B455) ----------------------------------------------------------- */
  {
    /* Six blocks with nowhere to render, between a note-on block and three
       plain blocks; `withBad` false leaves them out. The plain blocks must
       come out the same either way. */
    struct Seen { std::vector<float> out; int errors = 0, badBlocks = 0; bool untouched = true, finite = true, okStatus = true; };
    auto session = [&](bool withBad) {
      Seen sn;
      auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
      const clap_plugin_t *p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
      p->init(p);
      p->activate(p, kSR, 32, kBlock);
      p->start_processing(p);
      const float mark = 0.5f;
      std::vector<float> L(kBlock), R(kBlock);
      float *chans[2] = {L.data(), R.data()};
      clap_audio_buffer_t ob{};
      clap_process_t proc{};
      proc.frames_count = kBlock;
      proc.out_events = &kOut;
      EvList on, none;
      on.note(CLAP_EVENT_NOTE_ON, 60);
      auto good = [&](const EvList &ev) {
        chans[0] = L.data();
        chans[1] = R.data();
        ob.data32 = chans;
        ob.channel_count = 2;
        proc.audio_outputs = &ob;
        proc.audio_outputs_count = 1;
        proc.in_events = &ev.list;
        if (p->process(p, &proc) == CLAP_PROCESS_ERROR) sn.okStatus = false;
        for (uint32_t i = 0; i < kBlock; i++)
        {
          sn.out.push_back(L[i]);
          sn.out.push_back(R[i]);
          if (!std::isfinite(L[i]) || !std::isfinite(R[i])) sn.finite = false;
        }
      };
      good(on);
      for (int bad = 0; withBad && bad < 6; bad++)
      {
        std::fill(L.begin(), L.end(), mark);
        std::fill(R.begin(), R.end(), mark);
        chans[0] = L.data();
        chans[1] = R.data();
        ob.data32 = chans;
        ob.channel_count = 2;
        proc.audio_outputs = &ob;
        proc.audio_outputs_count = 1;
        proc.in_events = &none.list;
        if (bad == 0) chans[1] = nullptr;             // a null channel pointer
        if (bad == 1) chans[0] = nullptr;
        if (bad == 2) ob.data32 = nullptr;            // no 32-bit buffers
        if (bad == 3) ob.channel_count = 1;           // one channel of the two the port declares
        if (bad == 4) proc.audio_outputs_count = 0;   // no output bus
        if (bad == 5) proc.audio_outputs = nullptr;
        sn.badBlocks++;
        if (p->process(p, &proc) == CLAP_PROCESS_ERROR) sn.errors++;
        for (uint32_t i = 0; i < kBlock; i++)
          if (L[i] != mark || R[i] != mark) sn.untouched = false;
      }
      for (int b = 0; b < 3; b++) good(none);
      g_guardSamples += hypersaw_test_nonfinite_samples(p);
      g_guardLatched = g_guardLatched || hypersaw_test_nonfinite_latched(p);
      p->stop_processing(p);
      p->deactivate(p);
      p->destroy(p);
      return sn;
    };
    const Seen plain = session(false), bad = session(true);
    bool sounds = false;
    for (float v : plain.out) sounds = sounds || v != 0.0f;
    row(plain.okStatus && plain.finite && sounds, "OUT", "baseline: four plain blocks render a held note, finite");
    row(bad.badBlocks == 6 && bad.errors == 6, "OUT",
        "a null channel (either), no 32-bit buffers, one channel, no bus (count 0, or a null list): " +
            std::to_string(bad.errors) + " of " + std::to_string(bad.badBlocks) + " refused with CLAP_PROCESS_ERROR");
    row(bad.untouched, "OUT", "a refused block writes nothing to the buffers it was handed");
    row(bad.okStatus && bad.finite && bad.out == plain.out, "OUT",
        "the plain blocks around the refused ones render exactly what they render without them");
  }

  /* ---- OUT, continued: a refused block keeps its events; no out-event list;
          an owned block with nowhere to write (B455) ----------------------- */
  {
    EvList on, off;
    on.note(CLAP_EVENT_NOTE_ON, 60);
    off.note(CLAP_EVENT_NOTE_OFF, 60);

    /* The NOTE_OFF rides a block refused for its buffers. It must end the note
       at the next block that renders, exactly as if it had been sent there. */
    {
      Rig ref;
      ref.p->start_processing(ref.p);
      ref.block(on);
      ref.block(off);
      const bool refGated = ref.gated(60);
      for (int b = 0; b < 3; b++) ref.block(ref.none);

      Rig g;
      g.p->start_processing(g.p);
      g.block(on);
      const bool heldBefore = g.gated(60);
      g.chans[1] = nullptr;
      const clap_process_status st = g.block(off, /*keep=*/false);
      const bool heldInside = g.gated(60);
      g.plainBuffers();
      g.block(g.none);
      const bool heldAfter = g.gated(60);
      for (int b = 0; b < 3; b++) g.block(g.none);
      row(st == CLAP_PROCESS_ERROR && heldBefore && heldInside && !heldAfter && !refGated && refusedEvents(g.p) == 0 &&
              g.finite && g.out == ref.out,
          "OUT", std::string("a NOTE_OFF sent in a refused block: the note is held through it (") +
                     (heldInside ? "yes" : "NO") + "), released at the next block that renders (" +
                     (heldAfter ? "NO" : "yes") + "), 0 events refused, and the render == the render with the " +
                     "NOTE_OFF sent in that next block");
    }

    /* No out-event list. A note end has nowhere to go, so it stays pending and
       is sent when a list is there again; nothing else differs. Two places
       hold an owed end: the note's own tag (a note struck and released), and
       the retired-tag list (`steal`: one key more than there are voices, so a
       sounding note's tag is retired while its end is still owed). */
    {
      auto session = [&](bool steal, bool withoutList, int *endsWhileAbsent, int *endsAfter) {
        Rig g;
        g.p->start_processing(g.p);
        g_noteEnds = 0;
        if (withoutList) g.proc.out_events = nullptr;
        EvList many;
        for (int k = 0; k <= hypersaw_test_poly(); k++) many.note(CLAP_EVENT_NOTE_ON, 36 + k);
        g.block(steal ? many : on);
        g.block(steal ? g.none : off);
        for (int b = 0; b < 4; b++) g.block(g.none);
        *endsWhileAbsent = g_noteEnds;
        g.proc.out_events = &kOut;
        for (int b = 0; b < 4; b++) g.block(g.none);
        *endsAfter = g_noteEnds;
        return std::make_pair(g.out, g.finite);
      };
      for (bool steal : {false, true})
      {
        int a0 = 0, a1 = 0, b0 = 0, b1 = 0;
        const auto with = session(steal, false, &a0, &a1), without = session(steal, true, &b0, &b1);
        const std::string what = steal ? "one key more than there are voices" : "a note struck and released";
        row(a0 == 1 && a1 == 1 && with.second, "OUT", "baseline, " + what + ": with an out-event list, one NOTE_END (" +
                                                          std::to_string(a1) + ")");
        row(b0 == 0 && b1 == 1 && without.second && without.first == with.first, "OUT",
            what + ", six blocks with NO out-event list: " + std::to_string(b0) + " NOTE_END sent meanwhile, " +
                std::to_string(b1) + " once a list is back, and the render == the baseline");
      }
    }

    /* A block processed while a load owns the state is silence. Its buffers
       are asked for one by one, the list included. */
    {
      static char json[1 << 17];
      {
        Rig src;
        hypersaw_debug_state(src.p, json, sizeof json);
      }
      static const char *const kShape[] = {"plain buffers", "no output list (count 1)", "no 32-bit buffers"};
      for (int shape = 0; shape < 3; shape++)
      {
        Rig g;
        g_ownedRig = &g;
        g_ownedShape = shape;
        g_ownedSilent = false;
        g_onFlush = ownedBlock;
        const bool applied = hypersaw_debug_apply(g.p, json);
        const bool ran = g_onFlush == nullptr;   // the seam fired: the block ran inside the load
        g_onFlush = nullptr;
        for (int b = 0; b < 4; b++) g.block(g.none);
        row(applied && ran && g_ownedSilent && g.finite && g.sounds() && refusedEvents(g.p) == 0, "OUT",
            std::string("a block inside a load, ") + kShape[shape] + ": " +
                (shape == 0 ? "written with zeros" : "nothing written, nothing read") +
                "; its NOTE_ON plays in the blocks after the load");
      }
    }
  }

  /* ---- MIDI (B455) ---------------------------------------------------------- */
  {
    auto sent = [&](int status, int d1, int d2, int noteChannel = 0) {
      EvList on, m;
      on.note(CLAP_EVENT_NOTE_ON, 60, 1.0, noteChannel);
      m.midi(status, d1, d2);
      return run(on, m);
    };
    // In range: each message reaches its reader, so the rows below are not blind.
    const Render w7f = sent(0xB0, 1, 0x7F), w40 = sent(0xB0, 1, 0x40);
    row(w7f.finite && w7f.refused == 0 && w7f.srcWheel > base.srcWheel && w40.srcWheel > base.srcWheel &&
            w40.srcWheel < w7f.srcWheel,
        "MIDI", "CC1 at 0x40 and 0x7F: taken, source 15 reads " + num(w40.srcWheel) + " and " + num(w7f.srcWheel));
    const Render p7f = sent(0xD0, 0x7F, 0), p40 = sent(0xD0, 0x40, 0);
    row(p7f.finite && p7f.refused == 0 && p7f.srcPress > base.srcPress && p40.srcPress > base.srcPress &&
            p40.srcPress < p7f.srcPress,
        "MIDI", "channel pressure at 0x40 and 0x7F: taken, source 16 reads " + num(p40.srcPress) + " and " + num(p7f.srcPress));
    const Render b7f = sent(0xE0, 0x7F, 0x7F);
    row(b7f.finite && b7f.refused == 0 && b7f.out != base.out, "MIDI",
        "pitch wheel at its top (0x7F 0x7F): taken (render != the baseline)");
    EvList onCh1;
    onCh1.note(CLAP_EVENT_NOTE_ON, 60, 1.0, 1);
    const Render noBend = run(onCh1, empty), m7f = sent(0xE1, 0x7F, 0x7F, 1);
    row(m7f.finite && m7f.refused == 0 && m7f.out != noBend.out, "MIDI",
        "member-channel bend at its top (channel 2, 0x7F 0x7F): taken (render != the render with no bend)");

    // A data byte with the top bit set: the message is dropped and counted.
    struct Bad { int status, d1, d2; const char *what; };
    for (const Bad &b : {Bad{0xB0, 1, 0xFF, "CC1 with value byte 0xFF"}, Bad{0xB0, 1, 0xC0, "CC1 with value byte 0xC0"},
                         Bad{0xB0, 0x81, 0x7F, "a CC whose NUMBER byte is 0x81 (value 0x7F)"},
                         Bad{0xD0, 0xFF, 0, "channel pressure 0xFF"}, Bad{0xD0, 0xC0, 0, "channel pressure 0xC0"}})
    {
      const Render r = sent(b.status, b.d1, b.d2);
      row(r.finite && r.refused == 1 && r.srcWheel == base.srcWheel && r.srcPress == base.srcPress && r.out == base.out,
          "MIDI", std::string(b.what) + ": dropped and counted (" + std::to_string(r.refused) +
                      "), sources 15 and 16 unmoved, render == the baseline");
    }
    for (const Bad &b : {Bad{0xE0, 0xFF, 0xFF, "pitch wheel 0xFF 0xFF"}, Bad{0xE0, 0xFF, 0x7F, "pitch wheel 0xFF 0x7F"},
                         Bad{0xE0, 0x7F, 0xFF, "pitch wheel 0x7F 0xFF"}})
    {
      const Render r = sent(b.status, b.d1, b.d2);
      row(r.finite && r.refused == 1 && r.out == base.out, "MIDI",
          std::string(b.what) + ": dropped and counted, render == the baseline (no bend)");
    }
    {
      const Render r = sent(0xE1, 0xFF, 0xFF, 1);
      row(r.finite && r.refused == 1 && r.out == noBend.out, "MIDI",
          "member-channel bend 0xFF 0xFF: dropped and counted, render == the render with no bend");
    }
    // Controls: what is NOT a data byte of a message read here is not asked.
    {
      const Render third = sent(0xD0, 0x7F, 0xFF), other = sent(0xB0, 2, 0x7F), note = sent(0x90, 0xFF, 0xFF);
      row(third.refused == 0 && third.srcPress == p7f.srcPress, "MIDI",
          "channel pressure 0x7F with a third byte of 0xFF: taken (the message has one data byte)");
      row(other.refused == 0 && other.srcWheel == base.srcWheel && other.out == base.out, "MIDI",
          "a well-formed CC other than CC1: read by nothing, and not counted");
      row(note.refused == 0 && note.out == base.out, "MIDI",
          "a status this handler does not read (0x90): passed over, not counted");
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
