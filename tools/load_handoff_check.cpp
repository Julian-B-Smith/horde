/*
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
 * Controls (the rows above are only meaningful if these hold):
 *   STOP-IDLE    the same load, never processing: must match.
 *   STOP-NOSTOP  processing throughout, no stop: must match. If this fails,
 *                STOP's verdict is confounded and is reported as such.
 *   HOOK-OFF     processing, morph OFF in the outgoing patch: must match.
 *   NONZERO      the chunk differs from the defaults in at least one
 *                parameter, and its routing cells differ from the reference's
 *                corner values in at least one cell — otherwise both rows
 *                would compare equal values and read green blind.
 *
 * WIRED: ./verify full
 */

#include <algorithm>
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
  std::vector<clap_event_param_value_t> evs;
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
}  // namespace

int main()
{
  const std::string chunk = makeChunk();
  stopRows(chunk);
  hookRows(chunk);
  std::printf("load_handoff_check: %s (%d failure(s))\n", g_fail ? "RED" : "GREEN", g_fail);
  return g_fail ? 1 : 0;
}
