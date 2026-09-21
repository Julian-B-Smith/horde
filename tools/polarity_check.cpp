/*
 * polarity_check — B134's oracle: a modulation route's POLARITY is a property
 * of the route, it maps the source before depth, and it survives the chunk.
 *
 * WIRED into `./verify full` (verify:235). The header used to read "STANDALONE
 * AND UNWIRED" under the pre-ADR-180 rule and was left behind when the gate was
 * wired — corrected here by B171, which extends section C.
 *
 * Five sections, each answering one acceptance clause of ROADMAP B134:
 *   A  the map itself, at the core — a bipolar source through a unipolar route
 *      reads 0..1, a unipolar source through a bipolar route reads ±1, inverted
 *      negates, and "others pass".
 *   B  THE CONTROL. as-is is BIT-identical to a build without the field: the
 *      reference is `depth * src[q.src]`, the exact expression evaluate() used
 *      before B134, compared with ==, not a tolerance. Calibrated in the same
 *      breath — the non-as-is cases MUST differ from that reference, or the
 *      comparison is blind and section A proved nothing (L0032; and the
 *      detector-shares-the-assumption trap this repo keeps re-finding).
 *   C  the shell's SOURCE POLARITY TABLE, read back the way the GUI reads it:
 *      slot 2 (a macro) unipolar, slot 17 (pitch wheel) and 10-13 (the retired
 *      aliases) bipolar, and — B171 — slots 18/19 (the LFOs) bipolar with slot
 *      20 (ENV 3) beside them as the unipolar control.
 *   D  the chunk: the fourth field round-trips, an ABSENT field is as-is, a
 *      polarity-free patch serialises to exactly its pre-B134 bytes, and two
 *      routes that share (src, dest) but differ in polarity both survive — the
 *      case that retired ADR-138's merge.
 *   E  end to end through the shipped path: Macro 1 (a unipolar source) into
 *      Detune on a BIPOLAR route reaches below the base, which no as-is route
 *      on that source can do.
 *
 * Reuses tools/statefix_common.h — one stub host, one loader per transport,
 * one render — rather than growing a third copy of the CLAP scaffold.
 * WIRED: ./verify full.
 */

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <cmath>

#include "statefix_common.h"
#include "../src/hypersaw_debug.h"
#include "../src/mod_core.h"

using hypersaw::ModCore;
using namespace statefix;

namespace
{

int g_failures = 0;
void check(bool ok, const std::string &what)
{
  std::printf("%s %s\n", ok ? "OK  " : "FAIL", what.c_str());
  if (!ok) g_failures++;
}

/* A one-route table, evaluated the way the shell evaluates it. Returns the
   delta the combination law produced for the single destination. */
double evalOne(int pol, int sourcePol, double v, double depth)
{
  ModCore m;
  m.srcPol[0] = sourcePol;
  m.addRoute(0, 42, depth, ModCore::kGlobal, pol);
  m.src[0] = v;
  uint32_t dests[ModCore::kMaxRoutes];
  double deltas[ModCore::kMaxRoutes];
  const int n = m.evaluate(ModCore::kGlobal, dests, deltas, ModCore::kMaxRoutes);
  return n == 1 ? deltas[0] : 1e300;
}

/* The shell's JSON is a flat array of objects; this reads one integer field of
   the entry at `idx`. A tolerant scan, deliberately — the point is to read what
   the GUI reads, not to acquire a JSON parser. */
int jsonIntAt(const std::string &j, int idx, const char *key)
{
  size_t pos = 0;
  for (int i = 0; i <= idx; i++)
  {
    pos = j.find("{\"i\":", pos);
    if (pos == std::string::npos) return -999;
    if (i < idx) pos++;
  }
  const std::string needle = std::string("\"") + key + "\":";
  const size_t at = j.find(needle, pos);
  if (at == std::string::npos) return -999;
  return std::atoi(j.c_str() + at + needle.size());
}

std::string chunkLine(const std::string &blob, const char *key)
{
  const std::string needle = std::string("\n") + key + "=";
  const size_t at = blob.find(needle);
  if (at == std::string::npos) return "";
  const size_t eol = blob.find('\n', at + needle.size());
  return blob.substr(at + needle.size(),
                     eol == std::string::npos ? std::string::npos : eol - at - needle.size());
}

/* A fresh instance carrying exactly `routes` and nothing else said. */
std::string roundTripChunk(const std::string &routes)
{
  const clap_plugin_t *p = makePlugin();
  loadChunk(p, "hypersaw-state 2\nmodroutes=" + routes + "\n");
  const std::string saved = chunkLine(saveChunk(p), "modroutes");
  p->destroy(p);
  return saved;
}

/* ---- A: the map ---- */
void sectionMap()
{
  std::printf("\n-- A. the polarity map, at the core --\n");

  // A bipolar source through a UNIPOLAR route reads 0..1.
  bool span01 = true, ends01 = true;
  for (int i = 0; i <= 200; i++)
  {
    const double v = -1.0 + i / 100.0;
    const double d = evalOne(ModCore::kUnipolar, ModCore::kSrcBipolar, v, 1.0);
    if (d < 0.0 || d > 1.0) span01 = false;
  }
  ends01 &= evalOne(ModCore::kUnipolar, ModCore::kSrcBipolar, -1.0, 1.0) == 0.0;
  ends01 &= evalOne(ModCore::kUnipolar, ModCore::kSrcBipolar, 0.0, 1.0) == 0.5;
  ends01 &= evalOne(ModCore::kUnipolar, ModCore::kSrcBipolar, 1.0, 1.0) == 1.0;
  check(span01, "bipolar source -> unipolar route stays within 0..1 (201 points)");
  check(ends01, "bipolar source -> unipolar route: -1 -> 0, 0 -> 0.5, +1 -> 1 exactly");

  // A unipolar source through a BIPOLAR route reads +-1.
  bool spanPm = true, endsPm = true;
  for (int i = 0; i <= 100; i++)
  {
    const double v = i / 100.0;
    const double d = evalOne(ModCore::kBipolar, ModCore::kSrcUnipolar, v, 1.0);
    if (d < -1.0 || d > 1.0) spanPm = false;
  }
  endsPm &= evalOne(ModCore::kBipolar, ModCore::kSrcUnipolar, 0.0, 1.0) == -1.0;
  endsPm &= evalOne(ModCore::kBipolar, ModCore::kSrcUnipolar, 0.5, 1.0) == 0.0;
  endsPm &= evalOne(ModCore::kBipolar, ModCore::kSrcUnipolar, 1.0, 1.0) == 1.0;
  check(spanPm, "unipolar source -> bipolar route stays within -1..1 (101 points)");
  check(endsPm, "unipolar source -> bipolar route: 0 -> -1, 0.5 -> 0, 1 -> +1 exactly");

  // Inverted negates, whatever the source is.
  bool inv = true;
  for (int i = -100; i <= 100; i++)
  {
    const double v = i / 100.0;
    inv &= evalOne(ModCore::kInverted, ModCore::kSrcUnipolar, v, 1.0) == -v;
    inv &= evalOne(ModCore::kInverted, ModCore::kSrcBipolar, v, 1.0) == -v;
  }
  check(inv, "inverted negates exactly, for either source polarity (201 points)");

  // "others pass": asking for the polarity a source already has is the identity.
  bool pass = true;
  for (int i = 0; i <= 100; i++)
  {
    const double v = i / 100.0;
    pass &= evalOne(ModCore::kUnipolar, ModCore::kSrcUnipolar, v, 1.0) == v;
    const double b = -1.0 + i / 50.0;
    pass &= evalOne(ModCore::kBipolar, ModCore::kSrcBipolar, b, 1.0) == b;
  }
  check(pass, "a source already of the asked polarity passes through unchanged");

  // The map runs BEFORE depth, and the SUM law still holds over mixed routes —
  // the case ADR-138's (src,dest) merge could not have represented.
  {
    ModCore m;
    m.srcPol[0] = ModCore::kSrcUnipolar;
    m.addRoute(0, 42, 0.25, ModCore::kGlobal, ModCore::kAsIs);
    m.addRoute(0, 42, 0.25, ModCore::kGlobal, ModCore::kBipolar);
    m.src[0] = 0.0;
    uint32_t dests[ModCore::kMaxRoutes];
    double deltas[ModCore::kMaxRoutes];
    const int n = m.evaluate(ModCore::kGlobal, dests, deltas, ModCore::kMaxRoutes);
    check(n == 1 && deltas[0] == -0.25,
          "two routes on one (src,dest) with different polarity sum as 0 + (-0.25)");
  }
}

/* ---- B: the control, and its calibration ---- */
void sectionControl()
{
  std::printf("\n-- B. as-is is bit-identical to a build without the field --\n");
  const double depths[] = {-1.0, -0.37, 0.0, 0.25, 1.0};
  bool identical = true, differs = false;
  for (double depth : depths)
    for (int sp = 0; sp <= 1; sp++)
      for (int i = -100; i <= 100; i++)
      {
        const double v = i / 100.0;
        // The pre-B134 expression, verbatim: `q.depth * src[q.src]`.
        const double before = depth * v;
        if (evalOne(ModCore::kAsIs, sp, v, depth) != before) identical = false;
        // CALIBRATION: if nothing ever differs from `before`, the equality
        // above is vacuous — polarity would simply not be reaching evaluate().
        if (evalOne(ModCore::kBipolar, ModCore::kSrcUnipolar, v, depth) != before) differs = true;
      }
  check(identical, "pol 0 reproduces depth*src bit-for-bit (2010 points, == not epsilon)");
  check(differs, "calibration: a non-as-is route DOES differ from that reference");
}

/* ---- C: the shell's source polarity table ---- */
void sectionTable()
{
  std::printf("\n-- C. the source polarity table, read back the GUI's way --\n");
  const clap_plugin_t *p = makePlugin();
  // Macro 1 (slot 2), a retired XY alias (slot 10) and the pitch wheel (17),
  // all onto Detune. Route order is add order, so the indices are 0,1,2.
  check(hypersaw_test_mod_add(p, 2, 4), "route added on slot 2 (Macro 1)");
  check(hypersaw_test_mod_add(p, 10, 4), "route added on slot 10 (retired XY1 X)");
  check(hypersaw_test_mod_add(p, 17, 4), "route added on slot 17 (Pitch Wheel)");
  const std::string j = hypersaw_debug_modroutes(p);
  std::printf("     %s\n", j.c_str());
  check(jsonIntAt(j, 0, "srcPol") == ModCore::kSrcUnipolar, "Macro 1 declared unipolar");
  check(jsonIntAt(j, 1, "srcPol") == ModCore::kSrcBipolar, "retired XY alias declared bipolar");
  check(jsonIntAt(j, 2, "srcPol") == ModCore::kSrcBipolar, "Pitch Wheel declared bipolar");
  check(jsonIntAt(j, 0, "pol") == ModCore::kAsIs, "a fresh route is as-is");
  // B171: the two new BIPOLAR slots. An LFO declared unipolar by accident would
  // make an as-is route swing 0..1 about base instead of ±1 — audible, but only
  // as "the LFO feels off-centre", which is exactly what a table gets wrong
  // silently. ENV 3 (slot 20) is the unipolar control beside them.
  check(hypersaw_test_mod_add(p, 18, 4), "route added on slot 18 (LFO 1)");
  check(hypersaw_test_mod_add(p, 19, 4), "route added on slot 19 (LFO 2)");
  check(hypersaw_test_mod_add(p, 20, 4), "route added on slot 20 (ENV 3)");
  const std::string j2 = hypersaw_debug_modroutes(p);
  check(jsonIntAt(j2, 3, "srcPol") == ModCore::kSrcBipolar, "LFO 1 declared bipolar");
  check(jsonIntAt(j2, 4, "srcPol") == ModCore::kSrcBipolar, "LFO 2 declared bipolar");
  check(jsonIntAt(j2, 5, "srcPol") == ModCore::kSrcUnipolar,
        "control: ENV 3 beside them is unipolar (the table is not all-bipolar)");
  p->destroy(p);
}

/* ---- D: the chunk ---- */
void sectionChunk()
{
  std::printf("\n-- D. the modroutes chunk --\n");

  const std::string bare = roundTripChunk("2:4:0.5;");
  std::printf("     absent-field round trip: %s\n", bare.c_str());
  check(bare == "2:4:0.5;",
        "a polarity-free patch serialises to exactly its pre-B134 bytes (no :0)");

  const std::string pol2 = roundTripChunk("2:4:0.5:2;");
  std::printf("     fourth-field round trip: %s\n", pol2.c_str());
  check(pol2 == "2:4:0.5:2;", "the fourth field round-trips");

  const std::string all = roundTripChunk("2:4:0.5:1;2:6:0.5:2;2:7:0.5:3;");
  check(all == "2:4:0.5:1;2:6:0.5:2;2:7:0.5:3;", "all three non-default polarities round-trip");

  // The case that retired the (src,dest) merge: same source, same destination,
  // different polarity. A merge would have collapsed these into one entry and
  // silently discarded a setting.
  const std::string twin = roundTripChunk("2:4:0.25;2:4:0.25:2;");
  std::printf("     duplicate (src,dest) round trip: %s\n", twin.c_str());
  check(twin == "2:4:0.25;2:4:0.25:2;",
        "two routes sharing (src,dest) with different polarity both survive");

  // A garbage fourth field degrades to as-is; the route still loads.
  const std::string junk = roundTripChunk("2:4:0.5:9;");
  check(junk == "2:4:0.5;", "an out-of-range fourth field degrades to as-is, route kept");

  // The preset transport carries it too — one serializer, two transports.
  {
    const clap_plugin_t *p = makePlugin();
    loadChunk(p, "hypersaw-state 2\nmodroutes=2:4:0.5:3;\n");
    const std::string js = saveJson(p);
    check(js.find("\"modRoutes\":\"2:4:0.5:3;\"") != std::string::npos,
          "the preset JSON carries the fourth field");
    p->destroy(p);

    const clap_plugin_t *q = makePlugin();
    loadJson(q, "{\"plugin\":\"HYPERSAW\",\"schema\":3,\"params\":{},"
                "\"modRoutes\":\"2:4:0.5:3;\"}");
    check(jsonIntAt(hypersaw_debug_modroutes(q), 0, "pol") == ModCore::kInverted,
          "a preset load restores the polarity");
    q->destroy(q);
  }
}

/* ---- E: end to end, through the shipped apply path ---- */
void sectionApplied()
{
  std::printf("\n-- E. applied value: Macro 1 -> Detune, bipolar --\n");
  // Detune (param 4) is continuous over 0..1 with default 0.28, so a bipolar
  // route at depth 0.25 must reach 0.28 +- 0.25 as Macro 1 sweeps 0 -> 1. An
  // as-is route on the same unipolar source can only push UP from the base.
  struct Case { const char *routes; double macro; double want; const char *what; };
  const Case cases[] = {
      {"2:4:0.25:2;", 0.0, 0.03, "bipolar, Macro 1 at 0 -> base - depth (below the base)"},
      {"2:4:0.25:2;", 1.0, 0.53, "bipolar, Macro 1 at 1 -> base + depth"},
      {"2:4:0.25;",   0.0, 0.28, "as-is,   Macro 1 at 0 -> base (control: cannot go below)"},
      {"2:4:0.25;",   1.0, 0.53, "as-is,   Macro 1 at 1 -> base + depth"},
      {"2:4:0.25:3;", 1.0, 0.03, "inverted, Macro 1 at 1 -> base - depth"},
  };
  for (const Case &c : cases)
  {
    const clap_plugin_t *p = makePlugin();
    char json[256];
    std::snprintf(json, sizeof json,
                  "{\"plugin\":\"HYPERSAW\",\"schema\":3,\"params\":{\"macro1\":%.17g},"
                  "\"modRoutes\":\"%s\"}",
                  c.macro, c.routes);
    loadJson(p, json);
    std::vector<float> audio;
    render(p, audio);
    const double got = hypersaw_test_mod_applied(p, 4);
    std::printf("     applied %.6f (want %.6f)  %s\n", got, c.want, c.what);
    check(std::fabs(got - c.want) < 1e-9, c.what);
    p->destroy(p);
  }
}

}  // namespace

int main()
{
  std::printf("polarity_check — B134 per-route modulation polarity\n");
  sectionMap();
  sectionControl();
  sectionTable();
  sectionChunk();
  sectionApplied();
  std::printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED", g_failures,
              g_failures == 1 ? "" : "s");
  return g_failures ? 1 : 0;
}
