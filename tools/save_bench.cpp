/*
 * save_bench — how long does SAVING cost, and what does the GUI's poll add?
 *
 * B204. The human had to FORCE QUIT Ableton because saving a set containing
 * horde stalled. Two hypotheses were already killed on evidence (chunk size:
 * 9.5 KB; disk I/O in the save path: none). This measures the remaining
 * candidates as NUMBERS instead of impressions:
 *
 *   1. state_save through the CLAP factory, default and adversarial patch.
 *   2. presetMatches (B174's hzPresetDirty) against a real ~9.5 KB document,
 *      and its SCALING in document length — it is one std::string::find per
 *      parameter key, so it is O(keys x document) by construction and the
 *      question is only what the constant is.
 *   3. cornerMatches x4 (B122's hzMorphCornerDirty).
 *   4. The CONTROL: the same poll tick with 2 and 3 removed, i.e. the work the
 *      poll did before those landed. Without it the marginal cost of B174 is
 *      an impression, not a measurement.
 *   5. Whether anything in the save path grows with SESSION LENGTH: fill the
 *      undo ring to capacity and re-measure state_save and its blob size.
 *
 * DIAGNOSTIC, NOT A GATE — same standing as shell_bench / user_patch_bench:
 * it prints numbers and always exits 0. It asserts nothing, so there is
 * nothing here for ./verify to enforce.
 *
 * METHOD. Every figure is the MINIMUM of N repetitions (N printed beside it),
 * not the mean: the minimum is the closest estimate of the work itself, with
 * scheduler noise and page faults excluded rather than averaged in. A median
 * is printed alongside so a bimodal cost cannot hide behind a good minimum.
 * steady_clock, one timing call per repetition (not per batch), so nothing is
 * amortised away.
 */

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>   // std::atof / std::atoi — used below, so declared here (MSVC gate)
#include <cstring>
#include <string>
#include <vector>

#include <clap/clap.h>

#include "../src/hypersaw_clap_entry.h"
#include "../src/hypersaw_debug.h"

namespace
{

const void *host_get_extension(const clap_host_t *, const char *) { return nullptr; }
void host_noop(const clap_host_t *) {}
const clap_host_t kHost = {CLAP_VERSION, nullptr, "save_bench", "", "", "1.0",
                           host_get_extension, host_noop, host_noop, host_noop};

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
bool oev_try_push(const clap_output_events_t *, const clap_event_header_t *) { return true; }
const clap_output_events_t kOut = {nullptr, oev_try_push};

/* Move every parameter off its default through the HOST's own list — the same
   disturbance state_check uses (tools/state_check.cpp disturbAll), so the
   adversarial patch here is the one the persistence oracle already trusts and
   not a hand-picked list that flatters the measurement. Excludes id 178 for
   the same ADR-147 reason: both load paths skip it. */
size_t disturbAll(const clap_plugin_t *p)
{
  auto *px = (const clap_plugin_params_t *)p->get_extension(p, CLAP_EXT_PARAMS);
  if (!px) return 0;
  EvList e;
  e.list.ctx = &e;
  e.list.size = ev_size;
  e.list.get = ev_get;
  size_t moved = 0;
  for (uint32_t i = 0, n = px->count(p); i < n; i++)
  {
    clap_param_info_t info{};
    if (!px->get_info(p, i, &info)) continue;
    if (info.id == 178) continue;
    double cur = info.default_value;
    px->get_value(p, info.id, &cur);
    const bool stepped = (info.flags & CLAP_PARAM_IS_STEPPED) != 0;
    const double want = stepped ? (cur + 1 <= info.max_value ? cur + 1 : cur - 1)
                                : (cur < 0.5 * (info.min_value + info.max_value)
                                       ? info.max_value - (info.max_value - info.min_value) / 3
                                       : info.min_value + (info.max_value - info.min_value) / 3);
    if (want < info.min_value || want > info.max_value || want == cur) continue;
    clap_event_param_value_t ev{};
    ev.header.size = sizeof(ev);
    ev.header.type = CLAP_EVENT_PARAM_VALUE;
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.param_id = info.id;
    ev.note_id = -1;
    ev.port_index = -1;
    ev.channel = -1;
    ev.key = -1;
    ev.value = want;
    e.evs.push_back(ev);
    moved++;
  }
  px->flush(p, &e.list, &kOut);
  return moved;
}

void nudge(const clap_plugin_t *p, clap_id id, double v)
{
  auto *px = (const clap_plugin_params_t *)p->get_extension(p, CLAP_EXT_PARAMS);
  if (!px) return;
  EvList e;
  e.list.ctx = &e;
  e.list.size = ev_size;
  e.list.get = ev_get;
  clap_event_param_value_t ev{};
  ev.header.size = sizeof(ev);
  ev.header.type = CLAP_EVENT_PARAM_VALUE;
  ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  ev.param_id = id;
  ev.note_id = -1;
  ev.port_index = -1;
  ev.channel = -1;
  ev.key = -1;
  ev.value = v;
  e.evs.push_back(ev);
  px->flush(p, &e.list, &kOut);
}

std::string saveBlob(const clap_plugin_t *p)
{
  auto *sx = (const clap_plugin_state_t *)p->get_extension(p, CLAP_EXT_STATE);
  OStr o;
  o.s.ctx = &o;
  o.s.write = ostr_write;
  sx->save(p, &o.s);
  return o.data;
}

std::string stateJson(const clap_plugin_t *p)
{
  std::vector<char> buf(1u << 20);
  hypersaw_debug_state(p, buf.data(), (uint32_t)buf.size());
  return std::string(buf.data());
}

/* min / median of `reps` timings of `fn`, in microseconds. The function is run
   once before timing starts so the first-call allocations (the blob's string
   growth, the JSON reserve) are not billed to every row. */
struct Timing
{
  double minUs = 0, medUs = 0;
  int reps = 0;
};
template <typename F>
Timing timeIt(int reps, F &&fn)
{
  fn();
  std::vector<double> us;
  us.reserve((size_t)reps);
  for (int i = 0; i < reps; i++)
  {
    const auto t0 = std::chrono::steady_clock::now();
    fn();
    const auto t1 = std::chrono::steady_clock::now();
    us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
  }
  std::sort(us.begin(), us.end());
  Timing t;
  t.minUs = us.front();
  t.medUs = us[us.size() / 2];
  t.reps = reps;
  return t;
}
void row(const char *what, const Timing &t)
{
  std::printf("  %-52s min %9.2f us   median %9.2f us   (n=%d)\n", what, t.minUs, t.medUs, t.reps);
}

/* How many keys does presetMatches walk? Exactly the keys stateJson emits
   inside its "params" object: kParams, then the o<k>. twins, then the engine
   blocks all append to the same object before the closing brace. Counting them
   from the document is what keeps this number from being a stale literal. */
size_t paramKeyCount(const std::string &json)
{
  const size_t at = json.find("\"params\":{");
  if (at == std::string::npos) return 0;
  size_t i = at + 10, depth = 1, keys = 0;
  bool inStr = false, isKey = true;
  for (; i < json.size() && depth > 0; i++)
  {
    const char c = json[i];
    if (inStr)
    {
      if (c == '\\') { i++; continue; }
      if (c == '"') { inStr = false; if (isKey) keys++; }
      continue;
    }
    if (c == '"') { inStr = true; continue; }
    if (c == '{') depth++;
    else if (c == '}') depth--;
    else if (c == ':') isKey = false;
    else if (c == ',') isKey = true;
  }
  return keys;
}

/* A longer document with the SAME keys: filler is prepended inside the object,
   so every std::string::find scans past it before it can hit its key. That is
   what makes the scaling visible — appending would change nothing, because
   find() stops at the first match. The filler contains no quote characters, so
   it can never be mistaken for a key. */
std::string padded(const std::string &json, size_t extra)
{
  if (extra == 0 || json.empty() || json[0] != '{') return json;
  return "{\"pad\":\"" + std::string(extra, 'x') + "\"," + json.substr(1);
}

const clap_plugin_t *makePlugin()
{
  auto *factory =
      (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_t *p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
  p->init(p);
  return p;
}

/* One scenario: a patch, its save cost, its dirty-test cost, and the control.
   `adversarial` also captures all four morph corners and names them, because
   the morph chunk is the part of the blob that is NOT a parameter table and
   the corner dirty tests have nothing to compare against without it. */
/* Where to drop the adversarial host chunk, so vst3_save_bench can setState it
   and measure the SAME patch through the wrapper. Empty = do not write: this
   bench does no file I/O unless asked for exactly that. */
const char *g_chunkOut = nullptr;

void scenario(const char *name, bool adversarial)
{
  const clap_plugin_t *p = makePlugin();
  size_t moved = 0;
  if (adversarial)
  {
    moved = disturbAll(p);
    for (int k = 0; k < 4; k++)
    {
      hypersaw_debug_capture(p, k);
      char nm[32];
      std::snprintf(nm, sizeof nm, "corner-%d", k);
      hypersaw_debug_cornername(p, k, nm);
    }
    // Generic mod routes, so modroutes= is present in the blob (ADR-138).
    for (uint32_t s = 0; s < 4; s++) hypersaw_test_mod_add(p, s, 9 + s);
  }

  const std::string blob = saveBlob(p);
  const std::string js = stateJson(p);
  const size_t keys = paramKeyCount(js);
  if (adversarial && g_chunkOut)
    if (std::FILE *f = std::fopen(g_chunkOut, "wb"))
    {
      std::fwrite(blob.data(), 1, blob.size(), f);
      std::fclose(f);
      std::printf("  adversarial chunk written to %s\n", g_chunkOut);
    }
  std::printf("\n[%s]%s\n", name, adversarial ? "  (every parameter off default, 4 corners captured, 4 mod routes)" : "");
  if (adversarial) std::printf("  parameters moved off default: %zu\n", moved);
  std::printf("  host chunk (state_save): %zu bytes   preset document (stateJson): %zu bytes   keys presetMatches walks: %zu\n",
              blob.size(), js.size(), keys);
  std::printf("  => presetMatches does %zu find()s over a %zu-byte document"
              " = up to %.2f M character comparisons per call\n",
              keys, js.size(), (double)keys * (double)js.size() / 1e6);

  row("state_save (CLAP clap_plugin_state.save)", timeIt(400, [&] { saveBlob(p); }));
  row("stateJson  (the preset document builder)", timeIt(400, [&] { stateJson(p); }));

  // MATCHING is the worst case on purpose: presetMatches returns the moment a
  // key disagrees, so a mismatching document measures the early exit, not the
  // work the GUI pays every poll while the asterisk is OFF (the common case).
  row("presetMatches(self)  -- hzPresetDirty, matching",
      timeIt(200, [&] { hypersaw_debug_presetmatches(p, js.c_str()); }));
  {
    /* A REAL mismatch, not a cosmetic one: the FIRST key presetMatches walks is
       the first key stateJson emitted, so appending "1" to its value is the
       earliest possible disagreement and measures the return-on-first-difference
       path. (An inserted unread key would match everywhere and measure the full
       walk again wearing a mismatch label — a control that shares the
       assumption it is testing certifies nothing.) */
    std::string other = js;
    const size_t k0 = other.find("\"params\":{") + 10;
    const size_t c0 = other.find(':', k0);
    const size_t e0 = other.find_first_of(",}", c0);
    if (c0 != std::string::npos && e0 != std::string::npos) other.insert(e0, "1");
    std::printf("  early-exit control: first key reads %s vs %s\n",
                js.substr(k0, e0 - k0).c_str(), other.substr(k0, e0 - k0 + 1).c_str());
    row("presetMatches(first key differs) -- the early exit",
        timeIt(200, [&] { hypersaw_debug_presetmatches(p, other.c_str()); }));
  }

  /* WHERE THE presetMatches MILLISECOND GOES. The scaling rows below show the
     find() scan is NOT the bulk of it, so the per-key constant is: build a
     std::string needle, find it, then std::atof the %.17g value. atof is
     strtod, which for 17 significant digits runs the slow correctly-rounded
     path; these two rows bill the needle-and-find half and the atof half
     separately, over the same key count presetMatches walks. */
  {
    std::vector<std::string> needles;
    needles.reserve(keys);
    for (size_t i = 0, at = js.find("\"params\":{") + 10; needles.size() < keys;)
    {
      const size_t q0 = js.find('"', at);
      if (q0 == std::string::npos) break;
      const size_t q1 = js.find('"', q0 + 1);
      if (q1 == std::string::npos) break;
      needles.push_back(js.substr(q0, q1 - q0 + 1));
      at = js.find_first_of(",}", q1);
      if (at == std::string::npos) break;
      at++;
      i++;
    }
    row("  ... needle build + find(), all keys, no atof", timeIt(200, [&] {
          volatile size_t acc = 0;
          for (const auto &n : needles) acc += js.find("\"" + std::string(n.c_str() + 1, n.size() - 2) + "\"");
        }));
    const std::string num = "0.33333333333333331";
    row("  ... std::atof alone, one per key", timeIt(200, [&] {
          volatile double acc = 0;
          for (size_t i = 0; i < keys; i++) acc += std::atof(num.c_str());
        }));
  }

  // Four corner dirty tests, the B122 half of the poll.
  std::string cj[4];
  for (int k = 0; k < 4; k++)
  {
    const char *v = hypersaw_debug_cornervals(p, k);
    cj[k] = v ? v : "";
  }
  row("cornerMatches x4 -- hzMorphCornerDirty, matching", timeIt(200, [&] {
        for (int k = 0; k < 4; k++) hypersaw_debug_cornermatches(p, k, cj[k].c_str());
      }));
  std::printf("  corner document: %zu bytes each\n", cj[0].size());

  /* THE POLL TICK, with and without the dirty tests. The residual is what
     syncFromEngine/syncCornerNames/syncPresetName still cost with B174's and
     B122's comparisons deleted: the corner NAME list, plus the parameter
     snapshot the poll has fetched since the GUI existed. stateJson stands in
     for paramsJson, which has no debug export; it is a strict UPPER bound on
     it (same ~400 readParam calls, then %.17g instead of %.6g, plus the keys
     and the morph chunk paramsJson does not emit), so the control OVERSTATES
     the residual and therefore UNDERSTATES B174's share. Stated rather than
     hidden: a proxy that flatters the hypothesis would be worthless here. */
  const Timing withDirty = timeIt(200, [&] {
    stateJson(p);
    hypersaw_debug_cornernames(p);
    hypersaw_debug_presetmatches(p, js.c_str());
    for (int k = 0; k < 4; k++) hypersaw_debug_cornermatches(p, k, cj[k].c_str());
  });
  const Timing noDirty = timeIt(200, [&] {
    stateJson(p);
    hypersaw_debug_cornernames(p);
  });
  row("POLL TICK shell-side, dirty tests ON  (2 per second)", withDirty);
  row("POLL TICK shell-side, dirty tests OFF (the control)", noDirty);
  std::printf("  => marginal cost of the dirty tests: %.2f us per tick, %.3f%% of one core at 2 Hz\n",
              withDirty.minUs - noDirty.minUs, (withDirty.minUs - noDirty.minUs) * 2.0 / 1e4);

  // SCALING in document length: same keys, longer document.
  std::printf("  presetMatches scaling in document length (same key set):\n");
  for (size_t extra : {(size_t)0, (size_t)10000, (size_t)40000, (size_t)160000})
  {
    const std::string big = padded(js, extra);
    char lbl[80];
    std::snprintf(lbl, sizeof lbl, "  document %7zu bytes", big.size());
    row(lbl, timeIt(100, [&] { hypersaw_debug_presetmatches(p, big.c_str()); }));
  }

  p->destroy(p);
}

/* Does the SAVE PATH grow with session length? B191 made every control mark
   history (each arrow-key press included), so the undo ring now fills far
   faster than it did. The claim under test is that the ring is not serialised
   into the host chunk and does not touch state_save at all. */
void sessionLength()
{
  const clap_plugin_t *p = makePlugin();
  disturbAll(p);
  const std::string before = saveBlob(p);
  const Timing t0 = timeIt(400, [&] { saveBlob(p); });
  const int sz0 = std::atoi(hypersaw_debug_undo(p, "size", 0));

  /* Fill the ring. TWO things are load-bearing and both were wrong in this
     bench's first draft, which measured an empty tree and reported it as full:
     undoMark only RAISES a pending flag (hypersaw_clap.cpp:6362) — the snapshot
     is taken by undoService, which the GUI calls once per frame — and
     UndoTree::push dedups an identical snapshot, so each mark must follow a
     real parameter move or the "full" ring holds one node. What is timed here
     is therefore the whole per-edit history cost: move, mark, service. */
  const Timing mark = timeIt(300, [&, n = 0]() mutable {
    nudge(p, 9, 10.0 + (double)((n++) % 40));
    hypersaw_debug_undo(p, "mark", n);
    hypersaw_debug_undo(p, "service", 0);
  });
  const int sz1 = std::atoi(hypersaw_debug_undo(p, "size", 0));

  const std::string after = saveBlob(p);
  const Timing t1 = timeIt(400, [&] { saveBlob(p); });
  const Timing tree = timeIt(100, [&] { hypersaw_debug_undo(p, "tree", 0); });

  std::printf("\n[session length — does the undo ring reach the save path?]\n");
  std::printf("  undo tree size: %d -> %d of %d slots\n", sz0, sz1,
              [] { return 200; }());
  std::printf("  host chunk: %zu bytes -> %zu bytes (parameter moves included)\n",
              before.size(), after.size());
  std::printf("  chunk contains the string \"undo\": %s\n",
              after.find("undo") == std::string::npos ? "NO" : "YES");
  row("state_save, empty-ish tree", t0);
  row("state_save, tree at capacity", t1);
  row("one history entry (param move + mark + service snapshot)", mark);
  row("undoTreeJson at capacity (GUI HIST page poll only)", tree);
  p->destroy(p);
}

/* THE CONTROL FOR vst3_save_bench. That tool does setState(chunk) then
   getState and prints both byte counts; if they disagree, the question is
   whether the wrapper lost something or whether OUR OWN load path did. Running
   the identical round trip through the CLAP factory answers it, and without
   this row a VST3-path finding would be a difference with no baseline —
   exactly the shape that gets attributed to the wrong layer. */
void clapRoundTrip(bool withCorners, bool activated)
{
  const clap_plugin_t *a = makePlugin();
  /* ACTIVATION IS A VARIABLE, NOT A DETAIL. A round trip run on an
     init-but-never-activated instance is a rig a host never presents, so a
     loss seen only there would be an artefact of the bench and not a defect.
     Both arms are run and printed for exactly that reason. */
  if (activated) a->activate(a, 44100.0, 32, 512);
  disturbAll(a);
  if (withCorners)
  {
    for (int k = 0; k < 4; k++) hypersaw_debug_capture(a, k);
    for (uint32_t s = 0; s < 4; s++) hypersaw_test_mod_add(a, s, 9 + s);
  }
  const std::string saved = saveBlob(a);

  const clap_plugin_t *b = makePlugin();
  if (activated) b->activate(b, 44100.0, 32, 512);
  auto *sx = (const clap_plugin_state_t *)b->get_extension(b, CLAP_EXT_STATE);
  struct IStr
  {
    clap_istream_t s;
    std::string data;
    size_t pos = 0;
  } in;
  in.data = saved;
  in.s.ctx = &in;
  in.s.read = [](const clap_istream_t *s, void *buf, uint64_t n) -> int64_t {
    auto *i = (IStr *)s->ctx;
    const size_t take = std::min((size_t)n, i->data.size() - i->pos);
    std::memcpy(buf, i->data.data() + i->pos, take);
    i->pos += take;
    return (int64_t)take;
  };
  const bool ok = sx->load(b, &in.s);
  const std::string again = saveBlob(b);

  std::printf("\n[CLAP round trip — corners %s, instance %s]\n",
              withCorners ? "CAPTURED" : "untouched", activated ? "ACTIVATED" : "init-only");
  std::printf("  load -> %s;  saved %zu bytes, re-saved %zu bytes, identical: %s\n",
              ok ? "true" : "FALSE", saved.size(), again.size(),
              saved == again ? "YES" : "NO");
  if (saved != again)
  {
    // Name the first line that differs rather than the fact that one does.
    size_t pa = 0, pb = 0;
    while (pa < saved.size() && pb < again.size())
    {
      const size_t ea = saved.find('\n', pa), eb = again.find('\n', pb);
      const std::string la = saved.substr(pa, ea - pa), lb = again.substr(pb, eb - pb);
      if (la != lb)
      {
        std::printf("  first differing line:  saved [%s]  re-saved [%s]\n", la.c_str(), lb.c_str());
        break;
      }
      pa = ea + 1;
      pb = eb + 1;
    }
  }
  a->destroy(a);
  b->destroy(b);
}

}   // namespace

int main(int argc, char **argv)
{
  if (argc >= 2) g_chunkOut = argv[1];   // optional: dump the adversarial chunk here
  hypersaw_entry_init("");
  std::printf("save_bench — B204 save-stall measurement. All figures: min of n, "
              "steady_clock, one call timed per repetition.\n");
  scenario("default patch", false);
  scenario("adversarial patch", true);
  sessionLength();
  /* Both arms, because the first draft ran only the second and read its loss
     as a general state-load defect. Parameters alone is the baseline; the
     corner captures and mod routes are the added variable. */
  clapRoundTrip(false, false);
  clapRoundTrip(true, false);
  clapRoundTrip(false, true);
  clapRoundTrip(true, true);
  std::printf("\nNOTE: every figure above is the CLAP factory path. The VST3 path "
              "(clap-wrapper ClapAsVst3::getState, libs/clap-wrapper/src/wrapasvst3.cpp:276) "
              "forwards straight to this same clap_plugin_state.save through a stream "
              "adapter; it is not timed here because driving it needs a VST3 host.\n");
  hypersaw_entry_deinit();
  return 0;
}
