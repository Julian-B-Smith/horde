/*
 * WIRED: ./verify full
 *
 * load_handoff_check — a host state load stays the loaded patch, whatever the
 * processing state does around it (B448 B2, deterministic companion to
 * tools/tsan_stress_check.py).
 *
 * Single-threaded and deterministic on purpose: both rows are ORDERING
 * properties, which ThreadSanitizer cannot see. `processing` is an atomic, so
 * a check-then-act on it is race-free to TSan however wrong the order is.
 *
 * Rows (every comparison is against a REFERENCE: the same chunk loaded into a
 * fresh instance that is not processing, read back through the CLAP params
 * extension or the corner-values probe):
 *   STOP     processing is true when the load begins and the host stops
 *            processing part-way through it. The stop is placed exactly
 *            where it can land in a real host — after the load has decided to
 *            queue its init defaults — by doing it from the host's
 *            request_flush callback, which the shell calls on its first
 *            enqueue. After the host restarts and renders, every parameter
 *            must equal the reference.
 *   HOOK     processing is true and morph is ON in the outgoing patch; the
 *            loaded chunk carries routing cells. After the load and two
 *            blocks, all four corners must equal the reference's (a load is
 *            not an edit, B125).
 *   Q-*      a queued load equals an idle load: mod routes, the whole saved
 *            state (Q-ROUTES-CTL / Q-STATE-CTL see a difference that is there).
 *   S-*      SUPERSEDE: two queued loads inside one block leave the second,
 *            whole; the first's corners are not what stands.
 *   O-*      an overflowing load: refused entries are counted, its morph
 *            field still lands (the marker's slot is reserved).
 *   X-*      a queued load carrying intent / lfo / ens / engine_revision lines
 *            equals an idle one; without them it differs.
 *   P-*      two and three back-to-back host loads in one block: the last
 *            stands whole, nothing refused.
 *   E-*      events of a block owned by a direct load: the block is silent,
 *            the note and a value no queued entry names are replayed, a value
 *            the queue also names keeps the queued one (E-ORDER), none lost.
 *   F-*      a flush during a direct load does not drain; the next one does;
 *            a flush outside a load drains (control).
 *   R-*      a host reset during an owned block ends the deferred note that
 *            came before it; with no reset the note holds (control).
 *   D-*      deferred events with an impossible size (near 2^32, 0, a note
 *            shorter than a note) are refused and counted, nothing copied; a
 *            valid one beside them is kept and replayed.
 * Controls (the rows above are only meaningful if these hold):
 *   STOP-IDLE    the same load, never processing: must match.
 *   STOP-NOSTOP  processing throughout, no stop: must match. If this fails,
 *                STOP's verdict is confounded and is reported as such.
 *   HOOK-OFF     processing, morph OFF in the outgoing patch: must match.
 *   NONZERO      the chunk differs from the defaults in at least one
 *                parameter, and its routing cells differ from the reference's
 *                corner values in at least one cell — otherwise both rows
 *                would compare equal values and read green blind.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <clap/clap.h>

#include "../src/hypersaw_clap_entry.h"
#include "../src/hypersaw_debug.h"

namespace
{
constexpr double kSR = 44100.0;
constexpr uint32_t kBlock = 256;
constexpr clap_id kMorphOn = 151;
constexpr clap_id kRoutingIdBase = 10000;   // src/hypersaw_clap.cpp: every routing id is >= this

int g_fail = 0;
void row(bool ok, const char *tag, const std::string &what)
{
  std::printf("%s  %-11s %s\n", ok ? "PASS" : "FAIL", tag, what.c_str());
  if (!ok) g_fail++;
}

/* The host. request_flush is the seam STOP uses: the shell calls it on every
   enqueue, synchronously, on the loading thread. */
struct HostState
{
  const clap_plugin_t *plug = nullptr;
  bool stopOnFlush = false, stopped = false;
  void (*onFlush)() = nullptr;   // run ONCE, on the first request_flush, then cleared
} g_host;
void hpRescan(const clap_host_t *, clap_param_rescan_flags) {}
void hpClear(const clap_host_t *, clap_id, clap_param_clear_flags) {}
void hpRequestFlush(const clap_host_t *)
{
  if (g_host.stopOnFlush && !g_host.stopped && g_host.plug)
  {
    g_host.stopped = true;
    g_host.plug->stop_processing(g_host.plug);
  }
  if (g_host.onFlush)
  {
    void (*f)() = g_host.onFlush;
    g_host.onFlush = nullptr;
    f();
  }
}
const clap_host_params_t kHostParams = {hpRescan, hpClear, hpRequestFlush};
const void *hostGetExtension(const clap_host_t *, const char *id)
{
  return std::strcmp(id, CLAP_EXT_PARAMS) == 0 ? &kHostParams : nullptr;
}
void hostNoop(const clap_host_t *) {}
const clap_host_t kHost = {CLAP_VERSION, nullptr, "load_handoff_check", "", "", "1.0",
                           hostGetExtension, hostNoop, hostNoop, hostNoop};
bool outPush(const clap_output_events_t *, const clap_event_header_t *) { return true; }
const clap_output_events_t kOut = {nullptr, outPush};

struct EvList
{
  clap_input_events_t list{};
  std::vector<clap_event_param_value_t> evs;   // a note-on rides in a param-sized slot (it is smaller)
  EvList()
  {
    list.ctx = this;
    list.size = [](const clap_input_events_t *l) -> uint32_t { return (uint32_t)((EvList *)l->ctx)->evs.size(); };
    list.get = [](const clap_input_events_t *l, uint32_t i) -> const clap_event_header_t * {
      return &((EvList *)l->ctx)->evs[i].header;
    };
  }
  void param(clap_id id, double v)
  {
    clap_event_param_value_t e;
    std::memset(&e, 0, sizeof e);
    e.header = {sizeof e, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
    e.param_id = id;
    e.note_id = -1;
    e.port_index = -1;
    e.channel = -1;
    e.key = -1;
    e.value = v;
    evs.push_back(e);
  }
  void noteOn(int16_t key)
  {
    clap_event_param_value_t slot;
    std::memset(&slot, 0, sizeof slot);
    clap_event_note_t n;
    std::memset(&n, 0, sizeof n);
    n.header = {sizeof n, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_NOTE_ON, 0};
    n.note_id = -1;
    n.port_index = 0;
    n.channel = 0;
    n.key = key;
    n.velocity = 1.0;
    static_assert(sizeof n <= sizeof slot, "a note event fits a param event's slot");
    std::memcpy(&slot, &n, sizeof n);
    evs.push_back(slot);
  }
};

struct Inst
{
  const clap_plugin_t *p = nullptr;
  const clap_plugin_params_t *params = nullptr;
  const clap_plugin_state_t *state = nullptr;
  std::vector<float> L = std::vector<float>(kBlock), R = std::vector<float>(kBlock);

  Inst()
  {
    auto *f = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    p = f->create_plugin(f, &kHost, f->get_plugin_descriptor(f, 0)->id);
    g_host.plug = p;
    p->init(p);
    params = (const clap_plugin_params_t *)p->get_extension(p, CLAP_EXT_PARAMS);
    state = (const clap_plugin_state_t *)p->get_extension(p, CLAP_EXT_STATE);
    p->activate(p, kSR, 1, kBlock);
  }
  ~Inst()
  {
    p->deactivate(p);
    p->destroy(p);
  }
  void block(const EvList *ev = nullptr)
  {
    static EvList none;
    float *chans[2] = {L.data(), R.data()};
    clap_audio_buffer_t ob{};
    ob.data32 = chans;
    ob.channel_count = 2;
    clap_process_t proc{};
    proc.frames_count = kBlock;
    proc.audio_outputs = &ob;
    proc.audio_outputs_count = 1;
    proc.out_events = &kOut;
    proc.in_events = ev ? &ev->list : &none.list;
    p->process(p, &proc);
  }
  /* Idle-only: a params_flush on the main thread is legal while not processing. */
  void flushIdle(const EvList &ev) { params->flush(p, &ev.list, &kOut); }
  std::map<clap_id, double> values() const
  {
    std::map<clap_id, double> out;
    for (uint32_t i = 0, n = params->count(p); i < n; i++)
    {
      clap_param_info_t inf{};
      double v = 0;
      if (params->get_info(p, i, &inf) && params->get_value(p, inf.id, &v)) out[inf.id] = v;
    }
    return out;
  }
  std::string save() const
  {
    std::string blob;
    clap_ostream_t os{};
    os.ctx = &blob;
    os.write = [](const clap_ostream_t *s, const void *b, uint64_t n) -> int64_t {
      ((std::string *)s->ctx)->append((const char *)b, (size_t)n);
      return (int64_t)n;
    };
    state->save(p, &os);
    return blob;
  }
  bool load(const std::string &blob)
  {
    struct In { const std::string *d; size_t pos; } in{&blob, 0};
    clap_istream_t is{};
    is.ctx = &in;
    is.read = [](const clap_istream_t *s, void *b, uint64_t n) -> int64_t {
      auto *c = (In *)s->ctx;
      const size_t k = std::min<size_t>((size_t)n, c->d->size() - c->pos);
      std::memcpy(b, c->d->data() + c->pos, k);
      c->pos += k;
      return (int64_t)k;
    };
    return state->load(p, &is);
  }
  std::map<std::string, double> corners() const
  {
    std::map<std::string, double> out;
    for (int k = 0; k < 4; k++)
    {
      const std::string j = hypersaw_debug_cornervals(p, k);
      size_t pos = 0;
      while ((pos = j.find('"', pos)) != std::string::npos)
      {
        const size_t e = j.find('"', pos + 1);
        const std::string id = j.substr(pos + 1, e - pos - 1);
        out[std::to_string(k) + ":" + id] = std::strtod(j.c_str() + e + 2, nullptr);
        pos = e + 1;
      }
    }
    return out;
  }
};

/* Exact comparison on purpose: the reference is the same chunk through the
   same parser, so any difference at all is the ordering under test. */
template <class K>
std::string diff(const std::map<K, double> &got, const std::map<K, double> &want, int *count)
{
  std::string out;
  int n = 0;
  for (const auto &kv : want)
  {
    auto it = got.find(kv.first);
    if (it != got.end() && it->second == kv.second) continue;
    if (n++ < 4)
    {
      char b[96];
      std::snprintf(b, sizeof b, " %s=%.6g(want %.6g)", ([&] {
                      if constexpr (std::is_same_v<K, std::string>) return kv.first;
                      else return std::to_string(kv.first);
                    })().c_str(), it == got.end() ? -1.0 : it->second, kv.second);
      out += b;
    }
  }
  *count = n;
  return out;
}

/* The loaded patch: every parameter moved off its default (deterministically,
   to 37% of its range), routing cells included, morph left OFF in the chunk
   itself so the only morph state in play is the OUTGOING patch's. Built by
   the shell's own state_save, so the chunk is one the host would hold. */
std::string makeChunk()
{
  Inst src;
  EvList ev;
  for (uint32_t i = 0, n = src.params->count(src.p); i < n; i++)
  {
    clap_param_info_t inf{};
    if (!src.params->get_info(src.p, i, &inf)) continue;
    if (inf.id == kMorphOn || inf.id == 159 || inf.id == 178) continue;   // morph off, unarmed; 178 is not patch state
    double v = inf.min_value + 0.37 * (inf.max_value - inf.min_value);
    if (inf.flags & CLAP_PARAM_IS_STEPPED) v = (double)(int64_t)(v + 0.5);
    if (v == inf.default_value) v = inf.max_value;
    ev.param(inf.id, v);
  }
  src.flushIdle(ev);
  return src.save();
}

std::map<clap_id, double> defaultsNow()
{
  Inst d;
  return d.values();
}

void stopRows(const std::string &chunk)
{
  std::map<clap_id, double> ref;
  { Inst r; r.load(chunk); ref = r.values(); }
  int nz = 0;
  diff(defaultsNow(), ref, &nz);
  row(nz > 0, "NONZERO", std::to_string(nz) + " parameter(s) of the chunk differ from the defaults");

  int n = 0;
  std::string d;
  { Inst s; s.load(chunk); d = diff(s.values(), ref, &n); }
  row(n == 0, "STOP-IDLE", std::to_string(n) + " mismatch(es) loading while not processing" + d);

  {
    Inst s;
    s.p->start_processing(s.p);
    s.block();
    s.load(chunk);
    s.block();
    s.block();
    d = diff(s.values(), ref, &n);
  }
  row(n == 0, "STOP-NOSTOP", std::to_string(n) + " mismatch(es) loading while processing, no stop" + d);

  {
    Inst s;
    s.p->start_processing(s.p);
    s.block();
    g_host.stopOnFlush = true;
    g_host.stopped = false;
    s.load(chunk);
    g_host.stopOnFlush = false;
    const bool stopped = g_host.stopped;
    s.p->start_processing(s.p);
    s.block();
    s.block();
    d = diff(s.values(), ref, &n);
    row(stopped, "STOP-SEAM", stopped ? "the stop landed inside the load (first enqueue)"
                                      : "the load never enqueued, so the stop never landed: STOP is not testing anything");
  }
  row(n == 0, "STOP", std::to_string(n) + " parameter(s) differ after a stop mid-load" + d);
}

void hookRows(const std::string &chunk)
{
  auto refFor = [&](bool morphOn) {
    Inst r;
    EvList on;
    on.param(kMorphOn, morphOn ? 1 : 0);
    r.flushIdle(on);
    r.load(chunk);
    return std::make_pair(r.corners(), r.values());
  };
  auto scenario = [&](bool morphOn) {
    Inst s;
    EvList on;
    on.param(kMorphOn, morphOn ? 1 : 0);
    s.flushIdle(on);
    s.p->start_processing(s.p);
    s.block();
    s.load(chunk);
    s.block();
    s.block();
    return s.corners();
  };
  const auto refOn = refFor(true), refOff = refFor(false);

  // NONZERO: some loaded routing cell differs from what the reference's corners hold
  // for it, or writing the cell into a corner would change nothing and HOOK is blind.
  int cells = 0, differing = 0;
  for (const auto &kv : refOn.first)
  {
    const clap_id id = (clap_id)std::strtoul(kv.first.c_str() + 2, nullptr, 10);
    if (id < kRoutingIdBase) continue;
    cells++;
    auto it = refOn.second.find(id);
    if (it != refOn.second.end() && it->second != kv.second) differing++;
  }
  row(differing > 0, "NONZERO", std::to_string(differing) + " of " + std::to_string(cells) +
                                    " routing corner slot(s) differ from the loaded cell");

  int n = 0;
  std::string d = diff(scenario(false), refOff.first, &n);
  row(n == 0, "HOOK-OFF", std::to_string(n) + " corner slot(s) differ, outgoing morph OFF" + d);
  d = diff(scenario(true), refOn.first, &n);
  row(n == 0, "HOOK", std::to_string(n) + " corner slot(s) written by the load, outgoing morph ON" + d);
}

/* ---- B448 B2 rework: the rest of the handoff contract ------------------- */

struct Stats
{
  uint32_t depth = 0, dropped = 0, deferred = 0, deferDropped = 0, cap = 0;
};
Stats stats(const clap_plugin_t *p)
{
  Stats s;
  hypersaw_debug_handoff_stats(p, &s.depth, &s.dropped, &s.deferred, &s.deferDropped, &s.cap);
  return s;
}

/* A patch that touches every part a load writes: every parameter at `frac`
   of its range (as makeChunk), two mod routes, and corner B captured from
   that sound, so the morph field differs from the defaults too. */
std::string makeRichChunk(double frac)
{
  Inst src;
  EvList ev;
  for (uint32_t i = 0, n = src.params->count(src.p); i < n; i++)
  {
    clap_param_info_t inf{};
    if (!src.params->get_info(src.p, i, &inf)) continue;
    if (inf.id == kMorphOn || inf.id == 159 || inf.id == 178) continue;
    double v = inf.min_value + frac * (inf.max_value - inf.min_value);
    if (inf.flags & CLAP_PARAM_IS_STEPPED) v = (double)(int64_t)(v + 0.5);
    if (v == inf.default_value) v = inf.max_value;
    ev.param(inf.id, v);
  }
  src.flushIdle(ev);
  hypersaw_test_mod_add(src.p, 18, 4);    // LFO 1 -> detune
  hypersaw_test_mod_add(src.p, 2, 11);    // Macro 1 -> inertia
  hypersaw_debug_capture(src.p, 1);
  return src.save();
}

// The chunk with every line starting `key=` removed.
std::string withoutKey(const std::string &chunk, const char *key)
{
  std::string out, k = std::string("\n") + key + "=";
  size_t pos = 0;
  while (pos < chunk.size())
  {
    const size_t eol = chunk.find('\n', pos);
    const size_t end = eol == std::string::npos ? chunk.size() : eol + 1;
    if (pos == 0 || chunk.compare(pos - 1, k.size(), k) != 0) out.append(chunk, pos, end - pos);
    pos = end;
  }
  return out;
}

struct Seen
{
  std::string state, routes;
  std::map<std::string, double> corners;
};
Seen seen(Inst &i) { return {i.save(), hypersaw_debug_modroutes(i.p), i.corners()}; }

// The reference: loaded while NOT processing (direct), then three blocks.
Seen idleLoad(const std::string &chunk)
{
  Inst r;
  r.load(chunk);
  r.p->start_processing(r.p);
  for (int b = 0; b < 3; b++) r.block();
  return seen(r);
}
// Loaded while processing (queued); `chunks` in order, no block between them.
Seen queuedLoad(const std::vector<std::string> &chunks)
{
  Inst s;
  s.p->start_processing(s.p);
  for (const auto &c : chunks) s.load(c);
  for (int b = 0; b < 3; b++) s.block();
  return seen(s);
}

void queuedRows(const std::string &a, const std::string &b)
{
  const Seen ref = idleLoad(a), q = queuedLoad({a});
  row(!ref.routes.empty() && ref.routes != "[]", "Q-NONZERO", "the chunk's mod routes load: " + ref.routes);
  row(q.routes == ref.routes, "Q-ROUTES", "a queued load's mod routes equal an idle load's");
  row(q.state == ref.state, "Q-STATE", "a queued load's whole saved state equals an idle load's (" +
                                           std::to_string(ref.state.size()) + " bytes)");
  // Sense checks: the same comparisons must SEE a difference that is there.
  const Seen noRoutes = queuedLoad({withoutKey(a, "modroutes")});
  row(noRoutes.routes != ref.routes, "Q-ROUTES-CTL", "control: the chunk without its modroutes line differs");
  row(queuedLoad({b}).state != ref.state, "Q-STATE-CTL", "control: a different chunk's state differs");

  // SUPERSESSION: two loads within one block; the last one stands, whole.
  const Seen refB = idleLoad(b), ab = queuedLoad({a, b});
  row(refB.corners != ref.corners, "S-NONZERO", "the two chunks' morph fields differ");
  row(ab.state == refB.state && ab.corners == refB.corners, "SUPERSEDE",
      "two queued loads in one block leave the second load, whole (state and corners)");
  int n = 0;
  diff(ab.corners, ref.corners, &n);
  row(n > 0, "SUPERSEDE-CTL", "control: the first load's corners are not what stands (" + std::to_string(n) +
                                  " slot(s) differ)");
}

/* OVERFLOW: the queue is filled to within `room` entries before a queued
   load; parameter writes beyond the room are refused and counted, and the
   load's morph field still lands (its marker has a reserved slot). */
void overflowRows(const std::string &a)
{
  const Seen ref = idleLoad(a);
  Inst s;
  s.p->start_processing(s.p);
  const uint32_t room = 64;
  for (uint32_t k = 0; s.p && k < 4096; k++)
  {
    if (stats(s.p).depth >= stats(s.p).cap - room) break;
    hypersaw_debug_gesture(s.p, 4, (k & 1) == 0);
  }
  const Stats before = stats(s.p);
  s.load(a);
  const Stats after = stats(s.p);
  for (int b = 0; b < 3; b++) s.block();
  const auto corners = s.corners();
  Inst fresh;
  int nz = 0;
  diff(fresh.corners(), ref.corners, &nz);
  row(nz > 0, "O-NONZERO", "the chunk's corners differ from a fresh instance's in " + std::to_string(nz) + " slot(s)");
  row(after.dropped > before.dropped, "O-COUNTED", std::to_string(after.dropped - before.dropped) +
                                                      " entr(ies) refused and counted");
  int n = 0;
  const std::string d = diff(corners, ref.corners, &n);
  row(n == 0, "O-MARKER", "the overflowed load's morph field still landed" + d);
}

/* EVENTS DURING AN OWNED BLOCK: the host starts processing while a direct
   load runs (from inside the load, at its first request_flush) and sends a
   note-on and a parameter value in that block. The block is silent; both
   events are replayed at the next block; none is lost. */
Inst *g_inst = nullptr;
double g_silentPeak = -1;
bool g_sendNote = true;
void silentBlock()
{
  g_inst->p->start_processing(g_inst->p);
  EvList ev;
  if (g_sendNote) ev.noteOn(57);
  ev.param(100, 0.123);   // the load also writes masterVol: its queued value must win (E-ORDER)
  ev.param(178, 0);       // no load writes specimen: the deferred value must land (E-PARAM)
  g_inst->block(&ev);
  g_silentPeak = 0;
  for (uint32_t i = 0; i < kBlock; i++)
    g_silentPeak = std::max(g_silentPeak, (double)std::fabs(g_inst->L[i]) + std::fabs(g_inst->R[i]));
}
double peakAfter(Inst &s, int blocks)
{
  double pk = 0;
  for (int b = 0; b < blocks; b++)
  {
    s.block();
    for (uint32_t i = 0; i < kBlock; i++) pk = std::max(pk, (double)std::fabs(s.L[i]) + std::fabs(s.R[i]));
  }
  return pk;
}
void silentRows(const std::string &json)
{
  double spec = -1;
  auto run = [&](bool note, double &silent, double &after, double &vol, Stats &st) {
    Inst s;
    g_inst = &s;
    g_sendNote = note;
    g_silentPeak = -1;
    g_host.onFlush = silentBlock;
    hypersaw_debug_apply(s.p, json.c_str());
    g_host.onFlush = nullptr;
    silent = g_silentPeak;
    after = peakAfter(s, 8);
    s.params->get_value(s.p, 100, &vol);
    if (note) s.params->get_value(s.p, 178, &spec);
    st = stats(s.p);
  };
  double silent = 0, after = 0, vol = 0, cs = 0, ca = 0, cv = 0;
  Stats st, cst;
  run(true, silent, after, vol, st);
  run(false, cs, ca, cv, cst);
  char b[200];
  std::snprintf(b, sizeof b, "the owned block ran and was silent (peak %.3g)", silent);
  row(silent == 0.0, "E-SILENT", b);
  std::snprintf(b, sizeof b, "the note-on was replayed: peak %.3g after, against %.3g with no note (control)", after, ca);
  // Exact, not a threshold: the patch is silent without a note (the control
  // reads exactly 0), so any sound at all after the block is the replayed note.
  row(after > 0.0 && ca == 0.0, "E-NOTE", b);
  std::snprintf(b, sizeof b, "a deferred value no queued entry names was replayed: specimen %.6g (want 0)", spec);
  row(spec == 0.0, "E-PARAM", b);
  std::snprintf(b, sizeof b, "a deferred host value does not overwrite the queued entry for the same id: "
                             "masterVol %.6g (want the load's 1, not 0.123)", vol);
  row(vol == 1.0, "E-ORDER", b);
  row(st.deferred == 0 && st.deferDropped == 0, "E-NONE-LOST",
      std::to_string(st.deferred) + " byte(s) still waiting, " + std::to_string(st.deferDropped) + " dropped");
}

/* A FLUSH DURING A DIRECT LOAD does not drain: the host calls params.flush
   (not processing, so legal on the main thread) from inside the load. The
   control makes the same flush outside a load, where it must drain. */
uint32_t g_depthIn = 0, g_depthOut = 0, g_deferredIn = 0;
void flushInside()
{
  g_depthIn = stats(g_inst->p).depth;
  EvList ev;
  ev.param(178, 0);   // specimen: no load writes it, so its replay is visible
  g_inst->params->flush(g_inst->p, &ev.list, &kOut);
  g_depthOut = stats(g_inst->p).depth;
  g_deferredIn = stats(g_inst->p).deferred;
}
void flushRows(const std::string &json)
{
  Inst s;
  g_inst = &s;
  g_host.onFlush = flushInside;
  hypersaw_debug_apply(s.p, json.c_str());
  g_host.onFlush = nullptr;
  row(g_depthIn > 0 && g_depthOut == g_depthIn && g_deferredIn > 0, "F-NODRAIN",
      "a flush inside a direct load drained " + std::to_string(g_depthIn - g_depthOut) + " of " +
          std::to_string(g_depthIn) + " queued entr(ies) and kept its event");
  EvList none;
  s.flushIdle(none);
  double spec = -1;
  s.params->get_value(s.p, 178, &spec);
  const Stats after = stats(s.p);
  char b[160];
  std::snprintf(b, sizeof b, "after the load, the next flush drained (depth %u) and replayed the event (specimen %.6g, want 0)",
                after.depth, spec);
  row(after.depth == 0 && after.deferred == 0 && spec == 0.0, "F-LATER", b);
  // Control: the same kind of flush, no load around it, drains.
  Inst c;
  hypersaw_debug_gesture(c.p, 4, true);
  const uint32_t d0 = stats(c.p).depth;
  c.flushIdle(none);
  row(d0 > 0 && stats(c.p).depth == 0, "F-CTL", "control: a flush outside a load drains (" + std::to_string(d0) + " -> 0)");
}

/* THE REST OF A LOAD: intent tables, LFO streams, ensemble timing (both
   oscillators) and the engine revision ride the same staged adoption as the
   morph field on a queued load. The chunk carries every one of them. */
std::string withExtras(const std::string &chunk)
{
  std::string c = chunk;
  if (!c.empty() && c.back() != '\n') c += '\n';
  c += "intent=L:1,B:4:0:2:0.5,R:4:1:0.2:0.8,H:2:0.25:0.75\n";
  c += "lfo=0.25;12345,0.75;67890\n";
  c += "ens=0.5;4242;0.001,0.002,0.003\n";
  c += "o1.ens=0.25;99;0.004,0.005\n";
  c += "engine_revision=1\n";
  return c;
}
void extrasRows(const std::string &a)
{
  const std::string x = withExtras(a);
  const Seen ref = idleLoad(x), q = queuedLoad({x});
  const bool carries = ref.state.find("\nintent=") != std::string::npos &&
                       ref.state.find("\nlfo=") != std::string::npos &&
                       ref.state.find("\nens=") != std::string::npos &&
                       ref.state.find("\no1.ens=") != std::string::npos &&
                       ref.state.find("\nengine_revision=1") != std::string::npos;
  row(carries, "X-NONZERO", "an idle load of the chunk re-saves its intent, lfo, ens, o1.ens and engine_revision=1 lines");
  row(q.state == ref.state, "X-STATE", "a queued load's whole state equals an idle load's, extras included");
  row(queuedLoad({a}).state != ref.state, "X-CTL", "control: the same chunk without the extras differs");
}

/* PEAK-SIZE SUPERSESSION: back-to-back host loads inside one block, each a
   full batch. The last one stands, whole, and nothing is refused. Three loads
   of this chunk pass the old 2048-entry queue; they must fit this one. */
void peakRows(const std::string &a, const std::string &b)
{
  const std::string xa = withExtras(a), xb = withExtras(b);
  const Seen refA = idleLoad(xa), refB = idleLoad(xb);
  for (int n : {2, 3})
  {
    Inst s;
    s.p->start_processing(s.p);
    std::vector<std::string> seq;
    for (int k = 0; k < n; k++) seq.push_back(k % 2 == n % 2 ? xa : xb);   // ... ending with xb
    uint32_t perLoad = 0;
    for (size_t k = 0; k < seq.size(); k++)
    {
      s.load(seq[k]);
      if (k == 0) perLoad = stats(s.p).depth;
    }
    const Stats st = stats(s.p);
    for (int bl = 0; bl < 3; bl++) s.block();
    const Seen got = seen(s);
    char d[200];
    std::snprintf(d, sizeof d, "%d loads in one block (%u entries each, %u queued of %u): the last stands whole, %u refused",
                  n, perLoad, st.depth, st.cap, st.dropped);
    row(st.dropped == 0 && got.state == refB.state && got.corners == refB.corners,
        n == 2 ? "P-TWO" : "P-THREE", d);
  }
  row(refA.state != refB.state, "P-CTL", "control: the loads differ, so the last one is distinguishable");
}

/* A HOST RESET DURING AN OWNED BLOCK: the block's note-on is deferred, then
   the host resets (also deferred, in order). Replayed, the note must be
   ended by the reset that came after it. Control: no reset, the note holds. */
bool g_withReset = false;
void ownedBlockThenReset()
{
  silentBlock();
  if (g_withReset) g_inst->p->reset(g_inst->p);
}
bool anyGated(const clap_plugin_t *p)
{
  for (int slot = 0; slot < 64; slot++)
    if (hypersaw_test_slot_gated(p, slot)) return true;
  return false;
}
void resetRows(const std::string &json)
{
  bool gated[2] = {false, false};
  for (int withReset = 0; withReset < 2; withReset++)
  {
    Inst s;
    g_inst = &s;
    g_sendNote = true;
    g_withReset = withReset != 0;
    g_host.onFlush = ownedBlockThenReset;
    hypersaw_debug_apply(s.p, json.c_str());
    g_host.onFlush = nullptr;
    s.block();
    gated[withReset] = anyGated(s.p);
  }
  row(!gated[1], "R-RESET", "a reset deferred AFTER a deferred note-on ends it (no voice gated after replay)");
  row(gated[0], "R-CTL", "control: the same note with no reset is still gated");
}

/* DEFERRED EVENTS ARE COPIED ONLY WHEN THEIR SIZE IS POSSIBLE: a size near
   2^32, a size of 0, and a note event shorter than a note event are each
   refused and counted, nothing is copied, and the instance keeps running.
   Control: a valid event of the same flush is kept. Fed through a flush made
   inside a direct load, which defers everything it is given. */
struct RawList
{
  clap_input_events_t list{};
  std::vector<const clap_event_header_t *> evs;
  RawList()
  {
    list.ctx = this;
    list.size = [](const clap_input_events_t *l) -> uint32_t { return (uint32_t)((RawList *)l->ctx)->evs.size(); };
    list.get = [](const clap_input_events_t *l, uint32_t i) { return ((RawList *)l->ctx)->evs[i]; };
  }
};
struct SizeCase
{
  const char *tag;
  uint32_t size;
  uint16_t type;
  bool kept;
};
const SizeCase kSizes[] = {
    {"D-HUGE", 0xFFFFFFFFu, CLAP_EVENT_PARAM_VALUE, false},
    {"D-ZERO", 0, CLAP_EVENT_PARAM_VALUE, false},
    {"D-SHORT", (uint32_t)sizeof(clap_event_header_t), CLAP_EVENT_NOTE_ON, false},
    {"D-VALID", (uint32_t)sizeof(clap_event_param_value_t), CLAP_EVENT_PARAM_VALUE, true},
};
Stats g_sizeBefore[4], g_sizeAfter[4];
void flushOddSizes()
{
  for (int i = 0; i < 4; i++)
  {
    clap_event_param_value_t ev;   // the backing storage is always a whole param event
    std::memset(&ev, 0, sizeof ev);
    ev.header = {kSizes[i].size, 0, CLAP_CORE_EVENT_SPACE_ID, kSizes[i].type, 0};
    ev.param_id = 178;
    ev.note_id = -1;
    ev.port_index = -1;
    ev.channel = -1;
    ev.key = -1;
    ev.value = 0;
    RawList l;
    l.evs.push_back(&ev.header);
    g_sizeBefore[i] = stats(g_inst->p);
    g_inst->params->flush(g_inst->p, &l.list, &kOut);
    g_sizeAfter[i] = stats(g_inst->p);
  }
}
void sizeRows(const std::string &json)
{
  Inst s;
  g_inst = &s;
  g_host.onFlush = flushOddSizes;
  hypersaw_debug_apply(s.p, json.c_str());
  g_host.onFlush = nullptr;
  for (int i = 0; i < 4; i++)
  {
    const uint32_t refused = g_sizeAfter[i].deferDropped - g_sizeBefore[i].deferDropped;
    const uint32_t copied = g_sizeAfter[i].deferred - g_sizeBefore[i].deferred;
    char d[160];
    std::snprintf(d, sizeof d, "size %u, type %u: %s (refused %u, copied %u byte(s))", kSizes[i].size,
                  kSizes[i].type, kSizes[i].kept ? "kept" : "refused and counted", refused, copied);
    row(kSizes[i].kept ? (refused == 0 && copied == kSizes[i].size) : (refused == 1 && copied == 0), kSizes[i].tag, d);
  }
  EvList none;
  s.flushIdle(none);   // replays what was kept; the instance is still sound
  double spec = -1;
  s.params->get_value(s.p, 178, &spec);
  row(spec == 0.0, "D-REPLAY", "the kept event replayed after the load (specimen 0)");
}

std::string defaultJson()
{
  Inst d;
  static char buf[1 << 17];
  hypersaw_debug_state(d.p, buf, sizeof buf);
  return buf;
}
}  // namespace

int main()
{
  const std::string chunk = makeChunk();
  stopRows(chunk);
  hookRows(chunk);
  const std::string a = makeRichChunk(0.37), b = makeRichChunk(0.63);
  queuedRows(a, b);
  overflowRows(a);
  extrasRows(a);
  peakRows(a, b);
  const std::string json = defaultJson();
  silentRows(json);
  flushRows(json);
  resetRows(json);
  sizeRows(json);
  std::printf("load_handoff_check: %s (%d failure(s))\n", g_fail ? "RED" : "GREEN", g_fail);
  return g_fail ? 1 : 0;
}
