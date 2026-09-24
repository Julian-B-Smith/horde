/*
 * offcorner_check — B232 / ADR-183: in BLEND, a parameter whose source is OFF
 * in a corner takes no weight from that corner, and the rule is revision 2.
 * WIRED: ./verify full
 *
 *   offcorner_check [factory_dir]     (default docs/presets/factory; the Init
 *                                      row reads init/INIT - Init.json there)
 *
 * THE HUMAN'S CASE, AS A ROW THAT FAILS ON THE OLD BUILD. Corner A holds Osc 2
 * OFF with detune 0.1, corner B holds it ON with detune 0.8, puck at the
 * midpoint of A-B. Revision 1 (the plain four-corner blend) reads 0.45 there;
 * revision 2 must read 0.8 — "treats corner 1 as if it has corner 2's osc 2
 * with the level at 0". Built against a binary without the rule, a revision-2
 * blob clamps to revision 1 and the row reads 0.45 (L0059: the row is red on
 * the binary the change fixes, which is the only thing that makes it mean
 * anything). The CONTROL beside it is Osc 1's detune, whose source is ON in
 * every corner: both revisions must read the same bits.
 *
 * EVERY ROW RENDERS (L0063). A patch is built through the host path, captured
 * with the shell's own writer, re-stamped with the revision under test, loaded
 * into a FRESH instance through the preset door, and played. Parameter values
 * are read back AFTER rendering — the value the morph field wrote, as the
 * engine reports it — and the audio is hashed. Two audio rows carry the claim:
 *   - the RULE IS AUDIBLE: revision 1 and revision 2 of the same patch differ
 *     (the must-differ control that proves the hash can see a difference);
 *   - the RULE IS EXACTLY "AS IF": at revision 2 the patch renders bit-for-bit
 *     like the same patch with the OFF corner's settings replaced by the ON
 *     corner's — the human's sentence, as an identity.
 * "Revision 1 renders exactly as origin/main" is a CROSS-BUILD claim no single
 * binary can make about itself; the `hash` lines this prints for revision-1
 * renders are the anchor for that comparison (the trace records the run
 * against origin/main's build), and statefix_check's revision-1 fixture corpus
 * and bank_check's renders are the standing half.
 *
 * WHICH SOURCES. Swarm 2 (1150 over the 1000-block), Swarm 1 (150 over osc 1's
 * ids) and the Sub (gate 4015 over 4000..4019) — every source the shell has,
 * each through the one declaration the shell reads (sourceGateOf).
 *
 * WHAT IS PINNED UNCHANGED (L0036 — a deliberate absence needs a row):
 * stepped parameters (they take the pick path), exempt parameters, an exempt
 * GATE (its corners' stored values say nothing about where it is off), the
 * QUANTUM mode, and the intent-bus resolver (it resolves each parameter from
 * ONE owner corner — IntentCore::stepParams — so it has no blend to change).
 */

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"
#include "../src/hypersaw_debug.h"

namespace
{
#include "notefuzz_scaffold.inc"

int g_fail = 0;
void check(bool ok, const std::string &what)
{
  std::printf("%s %s\n", ok ? "OK  " : "FAIL", what.c_str());
  if (!ok) g_fail++;
}
std::string fmt(const char *f, double a, double b = 0, double c = 0)
{
  char buf[256];
  std::snprintf(buf, sizeof buf, f, a, b, c);
  return buf;
}

struct PV
{
  clap_id id;
  double v;
};

/* ---- the rig: one fresh instance per render ---------------------------- */
struct Rig
{
  const clap_plugin_t *p = nullptr;
  const clap_plugin_params_t *params = nullptr;
  std::vector<float> L, R;
  clap_audio_buffer_t out{};
  clap_process_t proc{};
  float *ch[2];
  uint64_t hash = 1469598103934665603ull;   // FNV-1a 64 over the rendered float bits

  void boot()
  {
    auto *f = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    p = f->create_plugin(f, &kHost, "com.lifted-truck.hypersaw");
    p->init(p);
    p->activate(p, kSR, 32, kBlock);
    p->start_processing(p);
    params = (const clap_plugin_params_t *)p->get_extension(p, CLAP_EXT_PARAMS);
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
    for (int i = 0; i < kBlock; i++)
      for (float s : {L[(size_t)i], R[(size_t)i]})
      {
        uint32_t u;
        std::memcpy(&u, &s, 4);
        for (int b = 0; b < 4; b++) { hash ^= (u >> (8 * b)) & 0xFF; hash *= 1099511628211ull; }
      }
  }
  void run(double sec)
  {
    for (int i = 0; i < Math_blocks(sec); i++) { EvList e; step(e); }
  }
  void send(const std::vector<PV> &pv)
  {
    EvList e;
    for (const auto &q : pv) e.params.push_back(mkParam(q.id, q.v));
    step(e);
  }
  void noteOn(int16_t key)
  {
    EvList e;
    e.notes.push_back(mkNote(CLAP_EVENT_NOTE_ON, 0, key, 1));
    step(e);
  }
  double get(clap_id id) const
  {
    double v = NAN;
    params->get_value(p, id, &v);
    return v;
  }
  void kill()
  {
    p->stop_processing(p);
    p->deactivate(p);
    p->destroy(p);
  }
};

/* The shell's morphIds order paired with its per-slot defaults, off a FRESH
   instance (whose four corners hold exactly the defaults — morphInit). The
   writer emits the pairs IN morphIds order; that order is the corner file's
   contract, so parsing positionally is safe. Same reading gen_factory_bank
   makes. */
std::vector<PV> cornerOrder()
{
  Rig r;
  r.boot();
  const std::string j = hypersaw_debug_cornervals(r.p, 0);
  std::vector<PV> out;
  for (size_t i = 0; i < j.size();)
  {
    const size_t q0 = j.find('"', i);
    if (q0 == std::string::npos) break;
    const size_t q1 = j.find('"', q0 + 1);
    if (q1 == std::string::npos) break;
    const size_t colon = j.find(':', q1);
    if (colon == std::string::npos) break;
    out.push_back({(clap_id)std::strtoul(j.c_str() + q0 + 1, nullptr, 10),
                   std::strtod(j.c_str() + colon + 1, nullptr)});
    i = colon + 1;
  }
  r.kill();
  return out;
}
const std::vector<PV> &order()
{
  static const std::vector<PV> o = cornerOrder();
  return o;
}
int slotOf(clap_id id)
{
  for (size_t i = 0; i < order().size(); i++)
    if (order()[i].id == id) return (int)i;
  return -1;
}

// A corner preset as the shell's own cornerJson prints it (layout 9, "%.6g").
std::string cornerJson(const std::vector<PV> &set)
{
  std::vector<double> vals;
  for (const auto &s : order()) vals.push_back(s.v);
  for (const auto &s : set)
  {
    const int k = slotOf(s.id);
    if (k < 0) { check(false, "setup: id " + std::to_string(s.id) + " is in the morph field"); continue; }
    vals[(size_t)k] = s.v;
  }
  std::string out = "{\"morphLayout\":9,\"cornerPreset\":[";
  char buf[32];
  for (size_t i = 0; i < vals.size(); i++)
  {
    std::snprintf(buf, sizeof buf, i ? ",%.6g" : "%.6g", vals[i]);
    out += buf;
  }
  return out + "]}";
}

/* ---- the patch ---------------------------------------------------------- */
struct Patch
{
  std::vector<PV> live;            // the patch's own values, sent before the field
  std::vector<PV> corner[4];       // per-corner overrides of the defaults
  double x = 0.5, y = 0.0;         // the puck
  int mode = 1;                    // 157: 1 = blend, 0 = quantum
  std::vector<clap_id> exempt;     // ADR-109 exemptions, written into the saved field
};

/* Build the patch through the host path and capture it with the shell's own
   writer; the result is a revision-2 blob (a fresh instance is the latest). */
std::string capture(const Patch &pt)
{
  Rig r;
  r.boot();
  r.send(pt.live);
  r.run(0.05);
  // Setup rows print only when they fail: they are preconditions, not claims.
  for (int k = 0; k < 4; k++)
    if (!hypersaw_debug_cornerapply(r.p, k, cornerJson(pt.corner[k]).c_str()))
      check(false, "setup: corner " + std::to_string(k) + " applies");
  // Glide 0 (coef 1): the field lands on its target in one grid tick, so a
  // read after rendering is the target itself, not a point on a slew.
  r.send({{157, (double)pt.mode}, {158, 0.0}, {152, pt.x}, {153, pt.y}});
  r.send({{151, 1.0}});
  r.run(0.1);
  static char state[1 << 18];
  hypersaw_debug_state(r.p, state, sizeof state);
  r.kill();
  std::string j = state;
  if (!pt.exempt.empty())
  {
    // The exempt set rides the morph chunk positionally (ADR-109 / B124).
    const std::string key = "\"morphExempt\":[";
    const size_t a = j.find(key);
    const size_t b = a == std::string::npos ? a : j.find(']', a);
    if (a == std::string::npos || b == std::string::npos)
      check(false, "setup: the blob carries morphExempt");
    if (a != std::string::npos && b != std::string::npos)
    {
      std::vector<int> ex(order().size(), 0);
      for (clap_id id : pt.exempt)
        if (slotOf(id) >= 0) ex[(size_t)slotOf(id)] = 1;
      std::string arr;
      for (size_t i = 0; i < ex.size(); i++) arr += (i ? "," : "") + std::to_string(ex[i]);
      j = j.substr(0, a + key.size()) + arr + j.substr(b);
    }
  }
  return j;
}

// The same blob, stamped with revision `rev`. The anchor is asserted: a blob
// the replacement missed would silently be tested at the wrong revision.
std::string atRevision(const std::string &j, int rev)
{
  const std::string key = "\"engine_revision\":";
  const size_t a = j.find(key);
  if (a == std::string::npos) { check(false, "setup: the blob carries engine_revision"); return j; }
  size_t b = a + key.size();
  while (b < j.size() && (std::isdigit((unsigned char)j[b]) || j[b] == '-')) b++;
  return j.substr(0, a + key.size()) + std::to_string(rev) + j.substr(b);
}

struct Heard
{
  int rev = -1;
  uint64_t hash = 0;
  std::map<clap_id, double> v;   // every morph slot's id, read after rendering
};

/* Load `json` into a fresh instance, optionally send host params after the
   load, strike a note and render; read every morph id back afterwards. */
Heard hear(const std::string &json, const std::vector<PV> &after = {})
{
  Rig r;
  r.boot();
  const bool ok = hypersaw_debug_apply(r.p, json.c_str());
  if (!ok) check(false, "setup: the patch loads");
  r.run(0.05);
  if (!after.empty()) { r.send(after); r.run(0.05); }
  r.hash = 1469598103934665603ull;   // hash only what the note sounds
  r.noteOn(48);
  r.run(0.3);
  Heard h;
  h.rev = hypersaw_debug_engine_revision(r.p);
  h.hash = r.hash;
  for (const auto &s : order()) h.v[s.id] = r.get(s.id);
  r.kill();
  return h;
}

bool same(double a, double b) { return std::memcmp(&a, &b, sizeof a) == 0; }
std::string hex(uint64_t h)
{
  char b[20];
  std::snprintf(b, sizeof b, "%016llx", (unsigned long long)h);
  return b;
}

/* The differing ids between two hearings — "rev 2 differs ONLY where the rule
   applies" is this set equalling the predicted one. */
std::set<clap_id> diff(const Heard &a, const Heard &b)
{
  std::set<clap_id> d;
  for (const auto &kv : a.v)
    if (!same(kv.second, b.v.at(kv.first))) d.insert(kv.first);
  return d;
}
std::string list(const std::set<clap_id> &s)
{
  std::string o = "{";
  for (clap_id id : s) o += (o.size() > 1 ? "," : "") + std::to_string(id);
  return o + "}";
}

/* ---- the patches under test ---------------------------------------------
   Corners A (0) and C (2) hold the source OFF, B (1) and D (3) hold it ON, so
   the puck's X is the whole story and Y is inert. */
constexpr double kDetA = 0.1, kDetB = 0.8;

Patch osc2Patch()
{
  Patch p;
  // Osc 2 audible in the patch; osc 1 on everywhere (its detune is the control).
  p.live = {{1150, 1.0}, {1017, 0.5}};
  for (int k = 0; k < 4; k++)
  {
    const bool on = (k & 1) != 0;
    p.corner[k] = {{1150, on ? 1.0 : 0.0},
                   {1004, on ? kDetB : kDetA},   // Osc 2 detune: the human's case
                   {1001, on ? 9.0 : 3.0},       // Osc 2 voices: STEPPED, pick path
                   {4, on ? 0.6 : 0.2}};         // Osc 1 detune: source ON everywhere
  }
  return p;
}

Patch subPatch()
{
  Patch p;
  p.live = {{4015, 1.0}};
  for (int k = 0; k < 4; k++)
  {
    const bool on = (k & 1) != 0;
    p.corner[k] = {{4015, on ? 1.0 : 0.0},
                   {4007, on ? 0.9 : 0.2},        // SUB Level
                   {4010, on ? 8000.0 : 500.0},   // SUB Tone
                   {4000, on ? 3.0 : 0.0}};       // SUB Wave: STEPPED
  }
  return p;
}

Patch osc1Patch()
{
  Patch p;
  p.live = {{1150, 1.0}};
  for (int k = 0; k < 4; k++)
  {
    const bool on = (k & 1) != 0;
    p.corner[k] = {{150, on ? 1.0 : 0.0}, {4, on ? kDetB : kDetA}, {1150, 1.0}};
  }
  return p;
}

/* The human's sentence as a patch: each OFF corner (A, C) holds its ON
   neighbour's (B, D) values for `ids` — the source's CONTINUOUS parameters.
   Stepped ones are left alone on purpose: they take the pick path, which the
   rule does not touch, so copying them would change what the pick draws and
   test something else. */
Patch asIf(Patch p, const std::vector<clap_id> &ids)
{
  for (int k = 0; k < 4; k += 2)
    for (auto &q : p.corner[k])
      if (std::find(ids.begin(), ids.end(), q.id) != ids.end())
        for (const auto &on : p.corner[k + 1])
          if (on.id == q.id) q.v = on.v;
  return p;
}

}  // namespace

int main(int argc, char **argv)
{
  const std::string factory = argc > 1 ? argv[1] : "docs/presets/factory";
  std::printf("offcorner_check — B232 / ADR-183: blend ignores a corner where the source is OFF\n");
  if (order().empty()) { std::printf("FAIL the shell reported an empty morph order\n"); return 1; }

  /* ---- 1. the revision itself ------------------------------------------ */
  {
    Rig r;
    r.boot();
    check(hypersaw_debug_engine_revision(r.p) == 2, "revision: a fresh instance is revision 2");
    r.kill();
    std::ifstream f(factory + "/init/INIT - Init.json", std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string init = ss.str();
    Rig q;
    q.boot();
    const bool ok = !init.empty() && hypersaw_debug_apply(q.p, init.c_str());
    q.run(0.05);
    check(ok && hypersaw_debug_engine_revision(q.p) == 2,
          "revision: the factory Init patch loads as revision 2");
    q.kill();
  }

  /* ---- 2. Swarm 2 — the human's case ----------------------------------- */
  const Patch P = osc2Patch();
  const std::string pj = capture(P);
  const Heard h1 = hear(atRevision(pj, 1)), h2 = hear(atRevision(pj, 2));
  Patch G = P;   // the same patch off the grid — see the off-grid control
  G.x = 0.3;
  G.y = 0.4;
  const std::string gj = capture(G);
  std::printf("hash rev1 osc2-midpoint %s\n", hex(h1.hash).c_str());
  check(h1.rev == 1 && h2.rev == 2, "swarm 2: the stamped revisions load as stamped (1, 2)");
  check(same(h2.v.at(1004), kDetB),
        fmt("swarm 2: rev 2 midpoint detune reads the ON corner's %.6g (read %.17g)", kDetB, h2.v.at(1004)));
  const double plain = 0.5 * kDetA + 0.5 * kDetB;
  check(std::fabs(h1.v.at(1004) - plain) < 1e-12,
        fmt("swarm 2: rev 1 midpoint detune is the plain blend %.6g (read %.17g)", plain, h1.v.at(1004)));
  check(same(h1.v.at(4), h2.v.at(4)) && std::fabs(h2.v.at(4) - 0.4) < 1e-12,
        fmt("CONTROL swarm 1 detune (source ON in every corner) is bit-identical across "
            "revisions (rev1 %.17g, rev2 %.17g)", h1.v.at(4), h2.v.at(4)));
  check(same(h1.v.at(1001), h2.v.at(1001)),
        fmt("stepped: osc 2 voices takes the pick path, unchanged (rev1 %g, rev2 %g)",
            h1.v.at(1001), h2.v.at(1001)));
  check(diff(h1, h2) == std::set<clap_id>{1004},
        "swarm 2: rev 2 differs from rev 1 ONLY in the gated continuous slot — " +
            list(diff(h1, h2)) + " want {1004}");
  {
    /* THE CONTROL OFF THE GRID. At the midpoint the bilinear weights sum to
       exactly 1, so "renormalise over the ON corners" and "don't" agree
       bitwise for an all-ON source whatever the code does. At (0.3, 0.4) they
       need not (0.42 + 0.18 + 0.28 + 0.12 in doubles), so this is a position
       where an all-ON parameter wrongly sent down the renormalising path would
       show up in the last bit. */
    const Heard g1 = hear(atRevision(gj, 1)), g2 = hear(atRevision(gj, 2));
    check(same(g1.v.at(4), g2.v.at(4)) && diff(g1, g2) == std::set<clap_id>{1004},
          fmt("CONTROL off the grid (0.3, 0.4): swarm 1 detune bit-identical (%.17g / %.17g)",
              g1.v.at(4), g2.v.at(4)) + ", differing set " + list(diff(g1, g2)) + " want {1004}");
  }
  check(h1.hash != h2.hash, "audio: the rule is audible (rev 1 and rev 2 renders differ) — "
                            "the must-differ control for the identity below");
  {
    const Heard h2as = hear(atRevision(capture(asIf(P, {1004})), 2));
    check(h2as.hash == h2.hash,
          "audio: rev 2 renders bit-identically to the same patch with the OFF corner "
          "holding the ON corner's osc 2 (" + hex(h2.hash) + " vs " + hex(h2as.hash) + ")");
  }

  /* ---- 3. the Sub (4015 over 4000..4019) ------------------------------- */
  {
    const Patch S = subPatch();
    const std::string sj = capture(S);
    const Heard s1 = hear(atRevision(sj, 1)), s2 = hear(atRevision(sj, 2));
    std::printf("hash rev1 sub-midpoint %s\n", hex(s1.hash).c_str());
    check(same(s2.v.at(4007), 0.9) && same(s2.v.at(4010), 8000.0),
          fmt("sub: rev 2 midpoint level/tone read the ON corner's 0.9 / 8000 (read %.17g / %.17g)",
              s2.v.at(4007), s2.v.at(4010)));
    check(std::fabs(s1.v.at(4007) - 0.55) < 1e-12 && std::fabs(s1.v.at(4010) - 4250) < 1e-9,
          fmt("sub: rev 1 reads the plain blend 0.55 / 4250 (read %.17g / %.17g)", s1.v.at(4007),
              s1.v.at(4010)));
    check(same(s1.v.at(4000), s2.v.at(4000)), "stepped: SUB Wave unchanged across revisions");
    check(diff(s1, s2) == std::set<clap_id>{4007, 4010},
          "sub: rev 2 differs ONLY in the gated continuous slots — " + list(diff(s1, s2)) +
              " want {4007,4010}");
    const Heard s2as = hear(atRevision(capture(asIf(S, {4007, 4010})), 2));
    check(s1.hash != s2.hash && s2as.hash == s2.hash,
          "audio: sub rule audible, and rev 2 == the patch with the OFF corner holding the ON "
          "corner's sub");
  }

  /* ---- 4. Swarm 1 (150 over osc 1's ids) ------------------------------- */
  {
    const std::string oj = capture(osc1Patch());
    const Heard o1 = hear(atRevision(oj, 1)), o2 = hear(atRevision(oj, 2));
    check(same(o2.v.at(4), kDetB) && std::fabs(o1.v.at(4) - plain) < 1e-12,
          fmt("swarm 1: rev 2 reads %.6g, rev 1 the plain %.6g", kDetB, plain) +
              fmt(" (read %.17g / %.17g)", o2.v.at(4), o1.v.at(4)));
  }

  /* ---- 5. edges --------------------------------------------------------- */
  {
    // A pure OFF corner reads back its own stored value; a pure ON corner its own.
    Patch a = P;
    a.x = 0.0;
    const Heard e0 = hear(atRevision(capture(a), 2));
    a.x = 1.0;
    const Heard e1 = hear(atRevision(capture(a), 2));
    check(same(e0.v.at(1004), kDetA) && same(e1.v.at(1004), kDetB),
          fmt("edge: pure OFF corner reads its stored %.6g, pure ON corner its %.6g "
              "(corner bit-identity; read %.17g", kDetA, kDetB, e0.v.at(1004)) +
              fmt(" / %.17g)", e1.v.at(1004)));

    /* THE FLOOR (kMorphOnFloor = 1e-3, the ramp's own). Below it the plain
       blend is read AND the source is switched off — the enable reading 0 is
       what makes the switch-over inaudible, so it is asserted beside it. */
    a.x = 5e-4;
    const std::string below = capture(a);
    const Heard b1 = hear(atRevision(below, 1)), b2 = hear(atRevision(below, 2));
    check(same(b1.v.at(1004), b2.v.at(1004)) && b2.v.at(1150) == 0.0,
          fmt("edge: live weight 5e-4 <= floor 1e-3 -> plain blend (rev1 %.17g, rev2 %.17g) with "
              "osc 2 switched OFF (enable %g)", b1.v.at(1004), b2.v.at(1004), b2.v.at(1150)));
    a.x = 2e-3;
    const Heard b3 = hear(atRevision(capture(a), 2));
    check(same(b3.v.at(1004), kDetB) && b3.v.at(1150) == 1.0,
          fmt("edge: live weight 2e-3 > floor -> the ON corner's %.6g (read %.17g), osc 2 ON (%g)",
              kDetB, b3.v.at(1004), b3.v.at(1150)));
  }
  {
    Patch x = P;
    x.exempt = {1004};
    const std::string xj = capture(x);
    const Heard x1 = hear(atRevision(xj, 1)), x2 = hear(atRevision(xj, 2));
    check(same(x1.v.at(1004), x2.v.at(1004)) && x1.hash == x2.hash,
          fmt("unchanged: an EXEMPT parameter is live-only in both revisions (%.17g / %.17g)",
              x1.v.at(1004), x2.v.at(1004)));
    x.exempt = {1150};
    const std::string gj = capture(x);
    const Heard g1 = hear(atRevision(gj, 1)), g2 = hear(atRevision(gj, 2));
    check(same(g1.v.at(1004), g2.v.at(1004)) && g1.hash == g2.hash,
          fmt("unchanged: an EXEMPT gate says nothing about its corners -> plain blend in both "
              "(%.17g / %.17g)", g1.v.at(1004), g2.v.at(1004)));
  }
  {
    Patch q = P;
    q.mode = 0;
    const std::string qj = capture(q);
    const Heard q1 = hear(atRevision(qj, 1)), q2 = hear(atRevision(qj, 2));
    check(diff(q1, q2).empty() && q1.hash == q2.hash,
          "unchanged: QUANTUM (the pick path, ADR-108's hold) renders bit-identically in both "
          "revisions");
    // The intent bus (266, ships OFF) resolves each slot from ONE owner corner
    // (IntentCore::stepParams) — no blend, so nothing for the rule to change.
    const Heard i1 = hear(atRevision(pj, 1), {{266, 1.0}}), i2 = hear(atRevision(pj, 2), {{266, 1.0}});
    check(diff(i1, i2).empty() && i1.hash == i2.hash && i1.hash != h1.hash,
          "unchanged: the intent-bus resolver renders bit-identically in both revisions (and "
          "differently from the flag-off blend, so the flag was on)");
  }

  /* ---- 6. the inverse: a live edit STICKS under the law that reads it ---- */
  for (int rev : {1, 2})
    for (const std::string *where : {&pj, &gj})
      /* morphRouteEdit distributes an unarmed blend edit so the FORWARD blend
         lands on it. Were the inverse left on the old weights, revision 2
         would read the ON corner's shifted value (0.85 at the midpoint), not
         the edit. 1019 (osc 2 attack) is the UNCONTESTED case: all four
         corners agree, so the forward reads it by the plain sum until the
         edit makes it contested — and the edit must still land. */
      for (const PV &ed : {PV{1004, 0.5}, PV{1019, 0.05}})
      {
        const Heard e = hear(atRevision(*where, rev), {ed});
        check(std::fabs(e.v.at(ed.id) - ed.v) < 1e-9,
              fmt("edit: rev %g, id %g, ", rev, ed.id) +
                  (where == &pj ? "midpoint" : "off-grid (0.3, 0.4)") +
                  fmt(" — a live edit sticks (want %.6g, read %.17g)", ed.v, e.v.at(ed.id)));
      }
  {
    Rig r;
    r.boot();
    hypersaw_debug_apply(r.p, atRevision(pj, 2).c_str());
    r.run(0.05);
    r.send({{1004, 0.5}});
    r.run(0.05);
    const std::string c0 = hypersaw_debug_cornervals(r.p, 0);
    const std::string key = "\"1004\":";
    const size_t at = c0.find(key);
    const double a0 = at == std::string::npos ? NAN : std::strtod(c0.c_str() + at + key.size(), nullptr);
    check(std::fabs(a0 - kDetA) < 1e-6,
          fmt("edit: rev 2 leaves the OFF corner's stored detune alone (%.6g, read %.6g)", kDetA, a0));
    r.kill();
  }

  std::printf("offcorner_check: %s (%d failure%s)\n", g_fail ? "RED" : "GREEN", g_fail,
              g_fail == 1 ? "" : "s");
  return g_fail ? 1 : 0;
}
