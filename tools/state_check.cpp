/*
 * state_check — session-persistence oracle at the CLAP layer.
 *
 * Links the plugin impl statically (clap-first idiom makes the factory a
 * plain symbol) and drives it with a stub host: set every parameter to a
 * non-default value through the params extension, save state, build a FRESH
 * instance, load, and require (1) every get_value round-trips exactly and
 * (2) the restored instance renders bit-identical audio for the same note —
 * i.e. sessions restore *sound*, not just numbers. Also checks forward
 * compatibility: a state blob missing keys (old session) loads with
 * defaults intact, and unknown keys are ignored.
 *
 * The wrapper layers (VST3/AU setState paths) are covered by pluginval and
 * auval; this pins OUR side of the contract in ./verify full.
 */

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <clap/clap.h>

#include "../src/hypersaw_clap_entry.h"

namespace
{

extern "C" void hypersaw_debug_state(const clap_plugin_t *, char *, uint32_t);
extern "C" bool hypersaw_debug_apply(const clap_plugin_t *, const char *);
extern "C" bool hypersaw_debug_exempt(const clap_plugin_t *, uint32_t);
extern "C" const char *hypersaw_debug_cornervals(const clap_plugin_t *, int);
extern "C" const char *hypersaw_debug_exemptjson(const clap_plugin_t *);
extern "C" int hypersaw_debug_engine_revision(const clap_plugin_t *);

int g_failures = 0;
void check(bool ok, const char *what)
{
  std::printf("%s %s\n", ok ? "OK  " : "FAIL", what);
  if (!ok) g_failures++;
}

/* ---- stub host ---- */
const void *host_get_extension(const clap_host_t *, const char *) { return nullptr; }
void host_noop(const clap_host_t *) {}
const clap_host_t kHost = {CLAP_VERSION, nullptr, "state_check", "", "", "1.0",
                           host_get_extension, host_noop, host_noop, host_noop};

/* ---- string-backed streams ---- */
struct OStr
{
  clap_ostream_t s;
  std::string data;
};
int64_t ostr_write(const clap_ostream_t *s, const void *buf, uint64_t n)
{
  auto *o = (OStr *)s;
  o->data.append((const char *)buf, (size_t)n);
  return (int64_t)n;
}
struct IStr
{
  clap_istream_t s;
  std::string data;
  size_t pos = 0;
};
int64_t istr_read(const clap_istream_t *s, void *buf, uint64_t n)
{
  auto *i = (IStr *)s;
  const size_t take = std::min((size_t)n, i->data.size() - i->pos);
  std::memcpy(buf, i->data.data() + i->pos, take);
  i->pos += take;
  return (int64_t)take;
}

/* ---- single-event input list for params.flush ---- */
struct EvList
{
  clap_input_events_t list;
  std::vector<clap_event_param_value_t> evs;
};
uint32_t ev_size(const clap_input_events_t *l) { return (uint32_t)((EvList *)l)->evs.size(); }
const clap_event_header_t *ev_get(const clap_input_events_t *l, uint32_t i)
{
  return &((EvList *)l)->evs[i].header;
}
uint32_t oev_try_push_count = 0;
bool oev_try_push(const clap_output_events_t *, const clap_event_header_t *)
{
  oev_try_push_count++;
  return true;
}
const clap_output_events_t kOut = {nullptr, oev_try_push};

const clap_plugin_t *makePlugin()
{
  auto *factory =
      (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_t *p =
      factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
  p->init(p);
  return p;
}

uint64_t renderHash(const clap_plugin_t * /*unused*/, const clap_plugin_t *p)
{
  // 1 s of a held A3 through the CLAP process path, FNV over the f32 stream.
  p->activate(p, 44100.0, 32, 1024);
  p->start_processing(p);
  std::vector<float> L(256), R(256);
  float *chans[2] = {L.data(), R.data()};
  clap_audio_buffer_t out{};
  out.data32 = chans;
  out.channel_count = 2;

  EvList in;
  clap_event_note_t on{};
  on.header.size = sizeof(on);
  on.header.type = CLAP_EVENT_NOTE_ON;
  on.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  on.note_id = -1;
  on.port_index = 0;
  on.channel = 0;
  on.key = 57;
  on.velocity = 1.0;
  // reuse EvList storage: param_value sized >= note event? No — use separate list.
  struct NoteList
  {
    clap_input_events_t list;
    clap_event_note_t ev;
    bool sent = false;
  } notes;
  notes.ev = on;
  notes.list.ctx = &notes;
  notes.list.size = [](const clap_input_events_t *l) -> uint32_t {
    return ((NoteList *)l->ctx)->sent ? 0u : 1u;
  };
  notes.list.get = [](const clap_input_events_t *l, uint32_t) -> const clap_event_header_t * {
    return &((NoteList *)l->ctx)->ev.header;
  };
  EvList empty;
  empty.list.ctx = &empty;
  empty.list.size = ev_size;
  empty.list.get = ev_get;

  clap_process_t proc{};
  proc.frames_count = 256;
  proc.audio_outputs = &out;
  proc.audio_outputs_count = 1;
  proc.out_events = &kOut;

  uint64_t h = 1469598103934665603ull;
  for (int block = 0; block < 172; block++)  // ~1 s
  {
    proc.in_events = block == 0 ? &notes.list : &empty.list;
    p->process(p, &proc);
    if (block == 0) notes.sent = true;
    for (int i = 0; i < 256; i++)
    {
      uint32_t a, b;
      std::memcpy(&a, &L[i], 4);
      std::memcpy(&b, &R[i], 4);
      h = (h ^ a) * 1099511628211ull;
      h = (h ^ b) * 1099511628211ull;
    }
  }
  p->stop_processing(p);
  p->deactivate(p);
  return h;
}

}  // namespace

int main()
{
  hypersaw_entry_init("");

  const clap_plugin_t *a = makePlugin();
  auto *params = (const clap_plugin_params_t *)a->get_extension(a, CLAP_EXT_PARAMS);
  auto *state = (const clap_plugin_state_t *)a->get_extension(a, CLAP_EXT_STATE);
  check(params && state, "params + state extensions present");

  // Set EVERY param to a distinct non-default value inside its range via
  // the params extension (the host path), stepped params to valid steps.
  const uint32_t n = params->count(a);
  std::vector<double> want(n);
  EvList evs;
  evs.list.ctx = &evs;
  evs.list.size = ev_size;
  evs.list.get = ev_get;
  for (uint32_t i = 0; i < n; i++)
  {
    clap_param_info_t info{};
    params->get_info(a, i, &info);
    double v;
    if (info.flags & CLAP_PARAM_IS_STEPPED)
      v = info.min_value + std::floor((info.max_value - info.min_value) * 0.5);
    else
      v = info.min_value + (info.max_value - info.min_value) * (0.3 + 0.02 * i);
    want[i] = v;
    clap_event_param_value_t ev{};
    ev.header.size = sizeof(ev);
    ev.header.type = CLAP_EVENT_PARAM_VALUE;
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.param_id = info.id;
    ev.note_id = -1;
    ev.port_index = -1;
    ev.channel = -1;
    ev.key = -1;
    ev.value = v;
    evs.evs.push_back(ev);
  }
  params->flush(a, &evs.list, &kOut);

  // The plugin may legally quantize (grid steps); capture what it SAYS it
  // holds — that is what must persist.
  std::vector<double> held(n);
  for (uint32_t i = 0; i < n; i++)
  {
    clap_param_info_t info{};
    params->get_info(a, i, &info);
    params->get_value(a, info.id, &held[i]);
  }

  OStr saved;
  saved.s.ctx = &saved;
  saved.s.write = ostr_write;
  check(state->save(a, &saved.s), "state save succeeds");
  // ADR-082: version 2 adds the `o<k>.` per-oscillator key namespace. Both are
  // valid — a 1-oscillator build still emits version 1, and version 1 blobs
  // remain loadable — so the gate accepts either rather than pinning the one
  // that happened to exist first. WIDENED WITH HUMAN RATIFICATION 2026-08-06
  // (recorded in ROADMAP); it is still a gate, not a formality: an unversioned
  // or unknown-version blob must fail.
  check(saved.data.rfind("hypersaw-state 1\n", 0) == 0 ||
            saved.data.rfind("hypersaw-state 2\n", 0) == 0,
        "state blob is versioned (1 or 2)");
  // B100: the header rides every blob. engine_revision pins the patch's DSP
  // laws, build is provenance. Pinned in TEXT (not via the parser) so a
  // symmetric writer/parser no-op cannot pass.
  check(saved.data.find("\nengine_revision=1\n") != std::string::npos,
        "B100: host chunk carries engine_revision=1");
  check(saved.data.find("\nbuild=") != std::string::npos, "B100: host chunk carries build");

  // Fresh instance, load, compare every value exactly.
  const clap_plugin_t *b = makePlugin();
  auto *paramsB = (const clap_plugin_params_t *)b->get_extension(b, CLAP_EXT_PARAMS);
  auto *stateB = (const clap_plugin_state_t *)b->get_extension(b, CLAP_EXT_STATE);
  IStr in;
  in.s.ctx = &in;
  in.s.read = istr_read;
  in.data = saved.data;
  check(stateB->load(b, &in.s), "state load succeeds on fresh instance");
  bool allEqual = true;
  for (uint32_t i = 0; i < n; i++)
  {
    clap_param_info_t info{};
    paramsB->get_info(b, i, &info);
    double got = 0;
    paramsB->get_value(b, info.id, &got);
    // ADR-147: the specimen's visibility is a GUI preference, NOT patch
    // state — the chunk writes it for forward compatibility but the restore
    // path never reads it back, BY RULING. Asserting its round-trip would be
    // asserting the superseded contract.
    if (info.id == 178) continue;
    if (got != held[i])
    {
      std::printf("     mismatch id %u: held %.17g restored %.17g\n", info.id, held[i], got);
      allEqual = false;
    }
  }
  check(allEqual, "every param value round-trips exactly");

  // Restored instance must SOUND identical: same note, bit-identical render.
  const uint64_t ha = renderHash(nullptr, a);
  const uint64_t hb = renderHash(nullptr, b);
  check(ha == hb, "restored instance renders bit-identical audio");

  // Old-session compatibility: a blob missing keys keeps defaults; unknown
  // keys are ignored.
  const clap_plugin_t *c = makePlugin();
  auto *stateC = (const clap_plugin_state_t *)c->get_extension(c, CLAP_EXT_STATE);
  auto *paramsC = (const clap_plugin_params_t *)c->get_extension(c, CLAP_EXT_PARAMS);
  IStr partial;
  partial.s.ctx = &partial;
  partial.s.read = istr_read;
  partial.data = "hypersaw-state 1\nK=0.5\nfutureParam=42\n";
  check(stateC->load(c, &partial.s), "partial/forward blob loads");
  double kv = 0, det = 0;
  paramsC->get_value(c, 6, &kv);
  paramsC->get_value(c, 4, &det);
  // det 0.28 again (ADR-156, 2026-09-10): the 2026-08-30 floor defaults were
  // reversed when the osc pads went back to writing detune/K directly.
  check(kv == 0.5 && det == 0.28, "missing keys keep defaults; unknown keys ignored");

  a->destroy(a);
  /* THE JSON STATE PATH (2026-08-21) — the preset system's path, previously
     covered by NOTHING: it lived behind webview lambdas and shipped a bug where
     oscillator 2's params never round-tripped (base ids only, no twins). This
     drives the same two calls the GUI preset buttons make, via the headless
     debug exports. Twin restore is the load-bearing half — it is the half that
     was missing. */
  {
    auto *jfac = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    const clap_plugin_t *j = jfac->create_plugin(jfac, &kHost, "com.lifted-truck.hypersaw");
    j->init(j); j->activate(j, 44100.0, 32, 128); j->start_processing(j);
    auto *jp = (const clap_plugin_params_t *)j->get_extension(j, CLAP_EXT_PARAMS);
    auto setTwo = [&](double v1, double v2) {
      EvList e2;
      e2.list.ctx = &e2; e2.list.size = ev_size; e2.list.get = ev_get;
      for (auto pr : {std::pair<clap_id, double>{4, v1}, {1004, v2}})
      {
        clap_event_param_value_t ev{};
        ev.header.size = sizeof(ev);
        ev.header.type = CLAP_EVENT_PARAM_VALUE;
        ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        ev.param_id = pr.first; ev.note_id = -1; ev.port_index = -1;
        ev.channel = -1; ev.key = -1; ev.value = pr.second;
        e2.evs.push_back(ev);
      }
      jp->flush(j, &e2.list, &kOut);
    };
    auto drain = [&]() {
      // the JSON apply is QUEUED; a flush with no events drains it, exactly as
      // the host's request_flush would
      EvList none;
      none.list.ctx = &none; none.list.size = ev_size; none.list.get = ev_get;
      for (int i = 0; i < 4; i++) jp->flush(j, &none.list, &kOut);
    };
    setTwo(0.111, 0.222);
    static char snap[65536];
    hypersaw_debug_state(j, snap, sizeof snap);
    setTwo(0.9, 0.9);
    const bool applied = hypersaw_debug_apply(j, snap);
    drain();
    double a = -1, bb = -1;
    jp->get_value(j, 4, &a); jp->get_value(j, 1004, &bb);
    check(applied && std::fabs(a - 0.111) < 1e-9, "JSON state: base param round-trips");
    check(std::fabs(bb - 0.222) < 1e-9, "JSON state: oscillator 2 TWIN round-trips");
    /* B110 (found by B100's fixture generator, 2026-09-10): a FULL preset has
       ~323 keys plus the twins and the migrators' own writes, and applyStateJson
       pushes every one through the param queue — which DROPS on overflow. With
       the queue at 256 the tail of the table vanished silently, including
       osc 2's enable. Late-table ids are the canaries: masterVol (100), the
       osc-2 enable twin (1150) and the osc-2 continuous pitch (1181). Set
       them, snapshot the FULL state, scramble them, apply the snapshot, and
       they must all come back. Calibrated: with kQCap = 256 this reads RED. */
    auto setIds = [&](std::initializer_list<std::pair<clap_id, double>> kv) {
      EvList e3;
      e3.list.ctx = &e3; e3.list.size = ev_size; e3.list.get = ev_get;
      for (auto pr : kv)
      {
        clap_event_param_value_t ev{};
        ev.header.size = sizeof(ev);
        ev.header.type = CLAP_EVENT_PARAM_VALUE;
        ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        ev.param_id = pr.first; ev.note_id = -1; ev.port_index = -1;
        ev.channel = -1; ev.key = -1; ev.value = pr.second;
        e3.evs.push_back(ev);
      }
      jp->flush(j, &e3.list, &kOut);
    };
    setIds({{100, 0.333}, {1150, 0}, {1181, 7.5}});
    static char full[65536];
    hypersaw_debug_state(j, full, sizeof full);
    setIds({{100, 1.0}, {1150, 1}, {1181, 0}});
    const bool fullOk = hypersaw_debug_apply(j, full);
    drain();
    double mv = -1, en2 = -1, op2 = -1;
    jp->get_value(j, 100, &mv); jp->get_value(j, 1150, &en2); jp->get_value(j, 1181, &op2);
    char bd[200]; std::snprintf(bd, sizeof bd, "B110: a FULL preset applies to the last key — no queue truncation (masterVol %.3f want 0.333, enable2 %.0f want 0, pitch2 %.2f want 7.5)", mv, en2, op2);
    check(fullOk && std::fabs(mv - 0.333) < 1e-9 && en2 == 0.0 && std::fabs(op2 - 7.5) < 1e-9, bd);
    /* B100 on the preset path. The header is asserted in TEXT on the dump;
       the revision is read back through the debug export, which is the same
       accessor a gated law will consult. With kEngineRevision == 1 every case
       reads 1 — these are pinned NOW so the revision-2 bump cannot skip them,
       and the clamp case bites today: a preset from a future build must load
       and must not store a value this build has no laws for. */
    check(std::strstr(snap, "\"engine_revision\":1") != nullptr &&
              std::strstr(snap, "\"build\":\"") != nullptr,
          "B100: JSON preset carries engine_revision + build");
    const bool hl = hypersaw_debug_apply(
        j, "{\"plugin\":\"HYPERSAW\",\"schema\":3,\"params\":{\"detune\":0.5}}");
    check(hl && hypersaw_debug_engine_revision(j) == 1, "B100: header-less JSON loads as revision 1");
    const bool st = hypersaw_debug_apply(
        j, "{\"plugin\":\"HYPERSAW\",\"schema\":3,\"engine_revision\":1,\"build\":\"abc123+\","
           "\"future_header\":7,\"params\":{\"detune\":0.4}}");
    drain();
    double dj = -1;
    jp->get_value(j, 4, &dj);
    check(st && hypersaw_debug_engine_revision(j) == 1, "B100: revision-1 JSON round-trips its revision");
    check(std::fabs(dj - 0.4) < 1e-9, "B100: unknown JSON header keys ignored, params still apply");
    const bool fu = hypersaw_debug_apply(
        j, "{\"plugin\":\"HYPERSAW\",\"schema\":3,\"engine_revision\":99,\"params\":{\"detune\":0.4}}");
    check(fu && hypersaw_debug_engine_revision(j) == 1,
          "B100: future JSON revision loads, clamps to latest");
    j->stop_processing(j); j->deactivate(j); j->destroy(j);
  }

  {
    /* B100 on the host-chunk path — same four cases, the loader that every
       DAW session goes through. The re-save case is the one that matters for
       the 28 sets that already bind (ADR-154): a pinned revision must survive
       a load+save cycle, or the header is decoration. */
    const clap_plugin_t *r = makePlugin();
    auto *rs = (const clap_plugin_state_t *)r->get_extension(r, CLAP_EXT_STATE);
    auto *rp = (const clap_plugin_params_t *)r->get_extension(r, CLAP_EXT_PARAMS);
    check(hypersaw_debug_engine_revision(r) == 1, "B100: fresh instance is the latest revision (1)");
    auto loadText = [&](const char *text) {
      IStr s;
      s.s.ctx = &s; s.s.read = istr_read; s.data = text;
      return rs->load(r, &s.s);
    };
    check(loadText("hypersaw-state 2\nK=0.4\n") && hypersaw_debug_engine_revision(r) == 1,
          "B100: header-less chunk loads as revision 1");
    check(loadText("hypersaw-state 2\nengine_revision=1\nbuild=abc123+\nK=0.4\n") &&
              hypersaw_debug_engine_revision(r) == 1,
          "B100: revision-1 chunk round-trips its revision");
    OStr re;
    re.s.ctx = &re; re.s.write = ostr_write;
    check(rs->save(r, &re.s) && re.data.find("\nengine_revision=1\n") != std::string::npos,
          "B100: re-save keeps the pinned revision");
    double kv2 = 0;
    const bool uk = loadText("hypersaw-state 2\nengine_revision=1\nbuild=zzz\nfuture_header=7\nK=0.45\n");
    rp->get_value(r, 6, &kv2);
    check(uk && kv2 == 0.45, "B100: unknown chunk header keys ignored, params still apply");
    check(loadText("hypersaw-state 2\nengine_revision=99\nK=0.4\n") &&
              hypersaw_debug_engine_revision(r) == 1,
          "B100: future chunk revision loads, clamps to latest");
    r->destroy(r);
  }

  {
    /* ADR-112 A3: the HOST path must round-trip the morph FIELD — corners and
       exempt — not merely the params. This is exactly the case that shipped
       broken: every param restored, the field silently degenerate (all four
       corners lazily re-init to the restored live values). Author corner D to
       0.777 while the LIVE value ends at 0.3 — a value a lazy re-init CANNOT
       produce — save via the state extension, load into a FRESH instance, and
       require the authored corner and the exempt flag back.
       Authoring drives real PROCESS blocks (corner_probe's proven path):
       params_flush in this harness only enqueues host->engine edits, and with
       no audio thread the queue never drains — the first draft of this case
       authored nothing and could not fail for the right reason. */
    const clap_plugin_t *m = makePlugin();
    m->activate(m, 44100.0, 32, 256);
    m->start_processing(m);
    auto *ms = (const clap_plugin_state_t *)m->get_extension(m, CLAP_EXT_STATE);
    EvList ev2;
    ev2.list.ctx = &ev2; ev2.list.size = ev_size; ev2.list.get = ev_get;
    std::vector<float> mL(256), mR(256);
    float *mch[2] = {mL.data(), mR.data()};
    clap_audio_buffer_t mout{};
    mout.data32 = mch; mout.channel_count = 2;
    auto pumpM = [&](const clap_plugin_t *pp) {
      clap_process_t pr{};
      pr.frames_count = 256; pr.audio_outputs = &mout; pr.audio_outputs_count = 1;
      pr.in_events = &ev2.list; pr.out_events = &kOut;
      for (int i = 0; i < 4; i++) { pp->process(pp, &pr); ev2.evs.clear(); }
    };
    auto push = [&](clap_id id, double v) {
      clap_event_param_value_t e{};
      e.header.size = sizeof(e); e.header.type = CLAP_EVENT_PARAM_VALUE;
      e.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      e.param_id = id; e.value = v;
      e.note_id = -1; e.port_index = -1; e.channel = -1; e.key = -1;
      ev2.evs.push_back(e);
    };
    push(151, 1); pumpM(m);                 // morph on
    push(159, 4); pumpM(m);                 // arm corner D
    push(4, 0.777); pumpM(m);               // author D's detune
    push(159, 0); pumpM(m);                 // release the arm
    push(4, 0.3); pumpM(m);                 // live detune 0.3 (lands on the owner, not D)
    hypersaw_debug_exempt(m, 11);           // exempt inertia
    OStr ms_out;
    ms_out.s.ctx = &ms_out; ms_out.s.write = ostr_write;
    check(ms->save(m, &ms_out.s), "morph-field state saves");
    check(ms_out.data.find("morph=") != std::string::npos, "host blob carries the morph chunk");
    // The middle is pinned in TEXT: the writer must have serialised the
    // authored value, so a symmetric writer/parser no-op cannot pass.
    check(ms_out.data.find("0.777") != std::string::npos, "authored value reaches the blob");
    m->stop_processing(m); m->deactivate(m); m->destroy(m);

    const clap_plugin_t *m2 = makePlugin();
    auto *ms2 = (const clap_plugin_state_t *)m2->get_extension(m2, CLAP_EXT_STATE);
    IStr ms_in;
    ms_in.s.ctx = &ms_in; ms_in.s.read = istr_read; ms_in.data = ms_out.data;
    check(ms2->load(m2, &ms_in.s), "morph-field state loads on fresh instance");
    const std::string cd = hypersaw_debug_cornervals(m2, 3);
    const size_t at = cd.find("\"4\":");
    const double d = at == std::string::npos ? -99 : std::atof(cd.c_str() + at + 4);
    check(std::fabs(d - 0.777) < 1e-6, "authored corner D survives the session");
    const std::string ex = hypersaw_debug_exemptjson(m2);
    check(ex.find("\"11\"") != std::string::npos, "exempt set survives the session");
    m2->destroy(m2);
  }

  b->destroy(b);
  c->destroy(c);
  hypersaw_entry_deinit();

  std::printf("state_check: %s (%d failure%s)\n", g_failures ? "RED" : "GREEN", g_failures,
              g_failures == 1 ? "" : "s");
  return g_failures ? 1 : 0;
}
