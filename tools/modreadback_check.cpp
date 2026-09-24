/*
 * modreadback_check — B241's oracle: a MODULATED parameter reads back, is
 * saved, captured, adopted and snapshotted at its BASE, never at wherever the
 * modulator happened to be. ADR-136 promised this in a comment ("readback
 * reports base, so state, automation and the GUI never see the modulation");
 * this check is what makes the promise a measured property.
 * WIRED: ./verify full.
 *
 * WHY IT EXISTS. The B233 playbook read (from code, not a run) that readParam
 * consulted the ADR-136 base only AFTER a long run of early returns — routing
 * cells, engine rows, and every shell-owned id — so any of those under a
 * route read back the live, modulated value. And every persistence path
 * (state_save, stateJson, a history node, corner capture, morph-on adoption,
 * the host's get_value) reads through readParam. The measurement confirmed it
 * on 2026-09-23 (traces/2026-09-23-b241-modulated-readback.md): 172 routable
 * rows read back modulated — every engine, osc 2 and routing row the matrix
 * accepts, and the shell-owned rows of osc 1 + global.
 *
 * WHAT IT GATES. Every section RENDERS (L0063): the modulation is applied by
 * the audio thread's mod tick, so an instance that never calls process() has
 * no modulated value to leak and would pass vacuously.
 *
 *   A  SUB FINE UNDER LFO 1, EVERY PERSISTENCE PATH. sub.fine at base 20
 *      cents, LFO 1 as a square (exactly +1 for the first half cycle) at the
 *      route's 0.25 depth, so the applied value is exactly 20 + 0.25*200 = 70.
 *      Each path must read 20: get_value, the host state chunk, stateJson, a
 *      history node, corner capture, morph-on adoption (both branches: the
 *      first-ever morph-on and B222's editor morph-on), and the base a fresh
 *      instance holds after loading the saved chunk.
 *      CONTROL (must MOVE): the applied value really is 70 — without it,
 *      "every path reads 20" is also satisfied by an LFO that never moved.
 *      CONTROL (must read zero): the same run with NO route reads 20 on every
 *      path, so a nonzero deviation is the route's and not the rig's.
 *      POSITIVE CONTROL: swarm detune (id 4), an instrument-table row the
 *      ADR-136 intercept always covered, reads base on every path.
 *   B  THE SWEEP. Every parameter the matrix accepts as a destination, one at
 *      a time: route LFO 1 to it, render until the applied value leaves the
 *      base, and assert get_value still reads the base. A table by id family
 *      is printed. This is what makes the class unshippable rather than the
 *      one row the report named: the defect was an ORDERING in readParam, and
 *      the next early return someone adds would reopen it for a family no
 *      example here names.
 *      And ADR-136's other half on the same rows: a host write made UNDER the
 *      route lands as the new base, reading back exactly what the same write
 *      reads with no route (so a row that quantises is held to its own law).
 *      Inertia (11, a tapered knob), Step Grid (148, snapped after the
 *      intercept) and Inertia Curve (70, returns before it) broke this.
 *      CONTROL: at least one row per family actually moved — a family whose
 *      rows never moved would pass by never being tested.
 *   C  ROUTING CELLS PERSIST THROUGH THEIR OWN CHUNK. The `routing=` chunk and
 *      the history node's `routing` key read the matrix directly, not through
 *      readParam, so they are asserted separately on a continuous cell.
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <clap/clap.h>

#include "../src/hypersaw_clap_entry.h"
#include "../src/hypersaw_debug.h"

namespace
{
#include "notefuzz_scaffold.inc"

int g_failures = 0;
void check(bool ok, const std::string &what, const std::string &detail = "")
{
  std::printf("  %s  %s%s%s\n", ok ? "ok  " : "FAIL", what.c_str(),
              detail.empty() ? "" : "  —  ", detail.c_str());
  if (!ok) g_failures++;
}
std::string num(double v)
{
  char b[64];
  std::snprintf(b, sizeof b, "%.10g", v);
  return b;
}
bool same(double a, double b) { return std::fabs(a - b) <= 1e-9 * std::max(1.0, std::fabs(b)); }

constexpr clap_id kSubFine = 4006, kSubOn = 4015, kDetune = 4, kMorphOn = 151;
constexpr clap_id kLfo1Rate = 269, kLfo1Shape = 270;
constexpr int kSlotLfo1 = 18;
constexpr double kSquare = 4;   // lfoShapeAt: +1 for phase < 0.5, exactly

/* ---- the host's side of the state extension -------------------------- */
struct OStream
{
  std::string buf;
  clap_ostream_t s{this, [](const clap_ostream_t *o, const void *d, uint64_t n) -> int64_t {
                     ((OStream *)o->ctx)->buf.append((const char *)d, (size_t)n);
                     return (int64_t)n;
                   }};
};
struct IStream
{
  std::string buf;
  size_t pos = 0;
  clap_istream_t s{this, [](const clap_istream_t *i, void *d, uint64_t n) -> int64_t {
                     auto *self = (IStream *)i->ctx;
                     const size_t k = std::min((size_t)n, self->buf.size() - self->pos);
                     std::memcpy(d, self->buf.data() + self->pos, k);
                     self->pos += k;
                     return (int64_t)k;
                   }};
};

/* `key=value` from the state chunk, or `"key":value` from a JSON snapshot.
   NaN when absent — a missing key must never read as a match. */
double chunkValue(const std::string &chunk, const std::string &key)
{
  const std::string needle = "\n" + key + "=";
  const size_t at = chunk.find(needle);
  return at == std::string::npos ? NAN : std::strtod(chunk.c_str() + at + needle.size(), nullptr);
}
double jsonValue(const std::string &js, const std::string &key)
{
  const std::string needle = "\"" + key + "\":";
  const size_t at = js.find(needle);
  return at == std::string::npos ? NAN : std::strtod(js.c_str() + at + needle.size(), nullptr);
}

struct Rig
{
  const clap_plugin_t *p = nullptr;
  std::vector<float> L, R;
  float *ch[2];
  clap_audio_buffer_t out{};
  clap_process_t proc{};

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
  void tick(int n = 1) { for (int i = 0; i < n; i++) { EvList e; step(e); } }
  void set(clap_id id, double v) { EvList e; e.params.push_back(mkParam(id, v)); step(e); }
  void note(int m)
  {
    EvList e;
    e.notes.push_back(mkNote(CLAP_EVENT_NOTE_ON, 0, (int16_t)m, -1, 0.8));
    step(e);
  }
  double get(clap_id id) const
  {
    auto *ext = (const clap_plugin_params_t *)p->get_extension(p, CLAP_EXT_PARAMS);
    double v = NAN;
    return ext && ext->get_value(p, id, &v) ? v : NAN;
  }
  std::string save() const
  {
    auto *st = (const clap_plugin_state_t *)p->get_extension(p, CLAP_EXT_STATE);
    OStream o;
    o.buf.clear();
    st->save(p, &o.s);
    return "\n" + o.buf;   // leading newline: the first key is findable like the rest
  }
  void load(const std::string &chunk)
  {
    auto *st = (const clap_plugin_state_t *)p->get_extension(p, CLAP_EXT_STATE);
    IStream i;
    i.buf = chunk.substr(1);   // strip save()'s leading newline
    st->load(p, &i.s);
  }
  std::string json() const
  {
    std::vector<char> b(1 << 20);
    hypersaw_debug_state(p, b.data(), (uint32_t)b.size());
    return b.data();
  }
  std::string history() const { return hypersaw_debug_undo(p, "live", 0); }
  double corner(int k, clap_id id) const
  {
    return jsonValue(hypersaw_debug_cornervals(p, k), std::to_string(id));
  }
  double applied(clap_id id) const { return hypersaw_test_mod_applied(p, id); }
  void kill() { p->stop_processing(p); p->deactivate(p); p->destroy(p); }
};

/* A rig at the measurement point: LFO 1 a 1 Hz square, the destination at
   `base`, a held note so the sub is sounding, optionally the route, and a
   render of 20 mod ticks (~0.12 s) — inside the square's +1 half cycle, so
   the applied value is base + 0.25*span exactly. */
void prime(Rig &r, clap_id dest, double base, bool route)
{
  r.boot();
  r.set(kLfo1Rate, 1.0);
  r.set(kLfo1Shape, kSquare);
  r.set(kSubOn, 1);
  r.set(dest, base);
  r.note(48);
  if (route) check(hypersaw_test_mod_add(r.p, kSlotLfo1, dest), "  route LFO 1 -> " + std::to_string(dest) + " accepted");
  r.tick(20);
}

/* One path's reading, and its deviation from base. */
struct Row { std::string path; double v; };

void report(const char *title, double base, const std::vector<Row> &rows, bool mustEqual)
{
  std::printf("\n  %-44s %16s %12s\n", title, "value", "value-base");
  for (const auto &row : rows)
  {
    const bool ok = same(row.v, base);
    std::printf("    %-42s %16s %12s  %s\n", row.path.c_str(), num(row.v).c_str(),
                num(row.v - base).c_str(), ok ? "" : "<- NOT BASE");
    if (mustEqual) check(ok, row.path + " reads the base " + num(base), num(row.v));
  }
}

/* Every path the brief names, for one destination. Each capture/adoption runs
   on its own instance, so no path's side effect (corners authored, morph on,
   a base rewritten by the field) can colour another's reading. */
std::vector<Row> readAllPaths(clap_id dest, const std::string &key, double base, bool route)
{
  std::vector<Row> rows;
  std::string chunk;
  {
    Rig r;
    prime(r, dest, base, route);
    rows.push_back({"params_get_value (host readback)", r.get(dest)});
    chunk = r.save();
    rows.push_back({"state_save chunk", chunkValue(chunk, key)});
    rows.push_back({"stateJson (preset save)", jsonValue(r.json(), key)});
    rows.push_back({"history node (undo 'live')", jsonValue(r.history(), key)});
    hypersaw_debug_capture(r.p, 2);
    rows.push_back({"corner capture (corner C)", r.corner(2, dest)});
    r.kill();
  }
  {
    // A fresh instance loads the saved chunk, the route is then removed, and
    // what is left is the BASE the chunk carried — the damage a user meets on
    // reopening the project, as the engine reports it after rendering.
    Rig r;
    r.boot();
    r.load(chunk);
    r.tick(4);
    if (route) hypersaw_test_mod_remove(r.p, 0);
    r.tick(4);
    rows.push_back({"reload chunk, drop route: base", r.get(dest)});
    r.kill();
  }
  {
    // Morph-on, FIRST ever (corners never authored): every corner adopts the
    // live readback (the 2026-08-26 fix). Host write of 151, no editor.
    Rig r;
    prime(r, dest, base, route);
    r.set(kMorphOn, 1);
    rows.push_back({"morph-on adoption, first (corner A)", r.corner(0, dest)});
    r.tick(20);
    r.set(kMorphOn, 0);
    if (route) hypersaw_test_mod_remove(r.p, 0);
    r.tick(4);
    rows.push_back({"  ...then morph off, drop route: base", r.get(dest)});
    r.kill();
  }
  {
    // Morph-on through the EDITOR with corners already authored — B222's
    // morphAdoptUncontested. The corners are authored BEFORE the route exists
    // (captured at base, so every corner agrees and the row is uncontested);
    // the route is then added and the editor's bracket turns morph on.
    Rig r;
    r.boot();
    r.set(kLfo1Rate, 1.0);
    r.set(kLfo1Shape, kSquare);
    r.set(kSubOn, 1);
    r.set(dest, base);
    r.note(48);
    for (int k = 0; k < 4; k++) hypersaw_debug_capture(r.p, k);
    if (route) hypersaw_test_mod_add(r.p, kSlotLfo1, dest);
    r.tick(20);
    hypersaw_debug_gesture(r.p, kMorphOn, true);
    hypersaw_debug_undo(r.p, "setmorph", 1);
    hypersaw_debug_gesture(r.p, kMorphOn, false);
    r.tick(2);
    rows.push_back({"morph-on adoption, B222 editor (corner A)", r.corner(0, dest)});
    r.kill();
  }
  return rows;
}

void sectionPaths()
{
  std::printf("\n-- A. sub.fine under LFO 1: every persistence path reads the base --\n");
  const double base = 20.0, want = 20.0 + 0.25 * 200.0;
  {
    Rig r;
    prime(r, kSubFine, base, true);
    check(same(r.applied(kSubFine), want),
          "control (must MOVE): LFO 1 drives sub.fine to base + 0.25*span", num(r.applied(kSubFine)));
    r.kill();
  }
  report("sub.fine, ROUTED (base 20, applied 70)", base, readAllPaths(kSubFine, "sub.fine", base, true), true);
  report("sub.fine, NO ROUTE (must-read-zero)", base, readAllPaths(kSubFine, "sub.fine", base, false), true);

  // POSITIVE CONTROL: swarm detune, covered by the ADR-136 intercept since
  // 2026-08; depth 0.25 of its span moves it well clear of the base.
  const double dBase = 0.3;
  {
    Rig r;
    prime(r, kDetune, dBase, true);
    check(!same(r.applied(kDetune), dBase), "control (must MOVE): LFO 1 drives detune off its base",
          num(r.applied(kDetune)));
    r.kill();
  }
  report("detune (id 4), ROUTED — positive control", dBase, readAllPaths(kDetune, "detune", dBase, true), true);
}

/* ---- B: the sweep ------------------------------------------------------ */
const char *familyOf(clap_id id)
{
  if (id >= 10000) return "routing (>=10000)";
  if (id >= 3000) return "engine (3000-9999)";
  if (id >= 1000) return "osc 2 (1000-2999)";
  return "osc 1 + global (<1000)";
}

void sectionSweep()
{
  std::printf("\n-- B. the sweep: every routable destination reads its base under LFO 1 --\n");
  Rig r;
  r.boot();
  r.set(kLfo1Rate, 1.0);
  r.set(kLfo1Shape, kSquare);
  auto *ext = (const clap_plugin_params_t *)r.p->get_extension(r.p, CLAP_EXT_PARAMS);
  struct Fam { int routable = 0, moved = 0, leaked = 0, baseLost = 0; std::string ex; };
  std::map<std::string, Fam> fams;
  std::vector<std::string> leaks, writes;
  const uint32_t n = ext->count(r.p);
  for (uint32_t i = 0; i < n; i++)
  {
    clap_param_info_t info{};
    if (!ext->get_info(r.p, i, &info)) continue;
    const clap_id id = info.id;
    const double b0 = r.get(id);
    if (!hypersaw_test_mod_add(r.p, kSlotLfo1, id)) continue;   // stepped / refused
    Fam &f = fams[familyOf(id)];
    f.routable++;
    // Render until the matrix has pushed the destination off its base: the
    // square is +1 then -1, so a base pinned at one bound still moves within
    // one cycle (172 ticks at 1 Hz).
    bool moved = false;
    double seen = b0, ap = b0;
    for (int t = 0; t < 200 && !moved; t++)
    {
      r.tick();
      ap = r.applied(id);
      if (ap > -1e299 && !same(ap, b0)) { moved = true; seen = r.get(id); }
    }
    /* ADR-136's other half, the "drag under modulation" law: a host write to
       a destination the matrix is driving lands as its new BASE, and the base
       is what reads back. The write is a tenth of the span away from b0
       (toward the middle), so it is a real move for every row. The EXPECTED
       reading is the same write made with no route (`wrote` below): a row
       that quantises (23 snaps to a beat grid) reads its quantised value
       either way, and that is the row's law, not a lost write. */
    double wrote = NAN, gotW = NAN;
    if (moved)
    {
      const double span = info.max_value - info.min_value;
      const double w = b0 + (b0 < info.min_value + 0.5 * span ? 0.1 : -0.1) * span;
      r.set(id, w);
      r.tick(2);
      gotW = r.get(id);
      hypersaw_test_mod_remove(r.p, 0);
      r.tick(2);
      r.set(id, w);
      r.tick(1);
      wrote = r.get(id);
    }
    else
    {
      hypersaw_test_mod_remove(r.p, 0);
      r.tick(2);
    }
    // Put the row back, so the next row is swept on the default patch.
    r.set(id, b0);
    r.tick(1);
    if (!moved) continue;
    f.moved++;
    if (!same(gotW, wrote))
    {
      f.baseLost++;
      char line[256];
      std::snprintf(line, sizeof line, "%6u %-28s same write: no route reads %-12s under the route reads %s", (unsigned)id,
                    info.name, num(wrote).c_str(), num(gotW).c_str());
      writes.push_back(line);
    }
    if (!same(seen, b0))
    {
      f.leaked++;
      char line[256];
      std::snprintf(line, sizeof line, "%6u %-28s base %-12s applied %-12s get_value %s", (unsigned)id,
                    info.name, num(b0).c_str(), num(ap).c_str(), num(seen).c_str());
      leaks.push_back(line);
      if (f.ex.size() < 60) f.ex += (f.ex.empty() ? "" : " ") + std::to_string(id);
    }
  }
  std::printf("\n    %-24s %9s %6s %7s %9s  %s\n", "family", "routable", "moved", "LEAKED", "WRITELOST",
              "leaked ids (first few)");
  int leakedTotal = 0, lostTotal = 0;
  for (const auto &kv : fams)
  {
    std::printf("    %-24s %9d %6d %7d %9d  %s\n", kv.first.c_str(), kv.second.routable, kv.second.moved,
                kv.second.leaked, kv.second.baseLost, kv.second.ex.c_str());
    leakedTotal += kv.second.leaked;
    lostTotal += kv.second.baseLost;
    check(kv.second.moved > 0, "control: " + kv.first + " has rows the LFO actually moved");
  }
  if (!leaks.empty())
  {
    std::printf("\n    every leaking row:\n");
    for (const auto &l : leaks) std::printf("    %s\n", l.c_str());
  }
  if (!writes.empty())
  {
    std::printf("\n    every row whose write under modulation did not read back:\n");
    for (const auto &l : writes) std::printf("    %s\n", l.c_str());
  }
  check(leakedTotal == 0, "no routable destination reads back its modulated value",
        std::to_string(leakedTotal) + " leaked");
  check(lostTotal == 0, "a write under modulation reads back as the new base, on every row",
        std::to_string(lostTotal) + " lost");
  r.kill();
}

/* ---- C: routing cells through their own chunk -------------------------- */
void sectionRoutingChunk()
{
  std::printf("\n-- C. a modulated routing cell: `routing=` and the history node read the base --\n");
  Rig probe;
  probe.boot();
  const std::string ids = hypersaw_debug_routing_ids();
  // The first continuous routing cell the matrix accepts as a destination.
  clap_id cell = 0;
  for (size_t pos = 0; pos < ids.size() && !cell;)
  {
    const size_t q = ids.find_first_of("0123456789", pos);
    if (q == std::string::npos) break;
    char *end = nullptr;
    const unsigned long v = std::strtoul(ids.c_str() + q, &end, 10);
    pos = (size_t)(end - ids.c_str());
    if (v >= 10000 && hypersaw_test_mod_add(probe.p, kSlotLfo1, (uint32_t)v)) cell = (clap_id)v;
  }
  probe.kill();
  check(cell != 0, "a continuous routing cell is routable", std::to_string(cell));
  if (!cell) return;

  Rig r;
  r.boot();
  r.set(kLfo1Rate, 1.0);
  r.set(kLfo1Shape, kSquare);
  auto *ext = (const clap_plugin_params_t *)r.p->get_extension(r.p, CLAP_EXT_PARAMS);
  clap_param_info_t info{};
  for (uint32_t i = 0; i < ext->count(r.p); i++)
    if (ext->get_info(r.p, i, &info) && info.id == cell) break;
  /* A base OFF the default AND a routed value off the default, so neither
     reading can be "absent from the sparse chunk, therefore default" by luck.
     0.6 of the span: the route's +0.25 lands at 0.85, inside the range. */
  const double base = info.min_value + 0.6 * (info.max_value - info.min_value);
  check(!same(base, info.default_value), "the chosen base is not the cell's default", num(base));
  r.set(cell, base);
  hypersaw_test_mod_add(r.p, kSlotLfo1, cell);
  r.tick(20);
  check(!same(r.applied(cell), base), "control (must MOVE): the cell is off its base", num(r.applied(cell)));
  /* The `routing` chunk's value for `cell`, searched ONLY inside the chunk's
     own text (a whole-document find can land on the same digits in the morph
     fragment), and read as the default when the sparse chunk omits it —
     omission IS the chunk's way of saying "default". */
  auto cellIn = [&](const std::string &chunkText) {
    const std::string key = std::to_string(cell) + ":";
    size_t at = 0;
    while ((at = chunkText.find(key, at)) != std::string::npos)
    {
      if (at == 0 || chunkText[at - 1] == ',') return std::strtod(chunkText.c_str() + at + key.size(), nullptr);
      at += key.size();
    }
    return info.default_value;
  };
  const std::string chunk = r.save();
  const size_t rt = chunk.find("\nrouting=");
  const std::string rtText =
      rt == std::string::npos ? "" : chunk.substr(rt + 9, chunk.find('\n', rt + 1) - (rt + 9));
  const std::string hist = r.history();
  const size_t hr = hist.find("\"routing\":\"");
  const std::string hrText =
      hr == std::string::npos ? "" : hist.substr(hr + 11, hist.find('"', hr + 11) - (hr + 11));
  check(hr != std::string::npos, "the history node carries a `routing` key");
  const double inChunk = cellIn(rtText), inHist = cellIn(hrText);
  report(("routing cell " + std::to_string(cell) + ", ROUTED").c_str(), base,
         {{"get_value", r.get(cell)}, {"state_save `routing=`", inChunk}, {"history node `routing`", inHist}},
         true);
  r.kill();
}
}  // namespace

int main()
{
  std::printf("modreadback_check — B241: modulated parameters persist at their BASE\n");
  sectionPaths();
  sectionSweep();
  sectionRoutingChunk();
  std::printf("\n%s  (%d failure%s)\n", g_failures ? "FAIL" : "PASS", g_failures,
              g_failures == 1 ? "" : "s");
  return g_failures ? 1 : 0;
}
