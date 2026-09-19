/*
 * intent_check — the intent-bus resolver's oracle (B89 phase 2a, ADR-176).
 *
 * TWO KINDS OF CHECK, deliberately separated (L0031: a reference oracle
 * certifies AGREEMENT, and only over the surface the reference SPANS).
 *
 *   PARITY   Replay every fixture under build-golden/intent/ through
 *            IntentCore and compare weights / owner / final / clamped /
 *            intents / puck / commit against the prototype's own numbers at
 *            1e-6 absolute. Each fixture also carries a MUST-FAIL control: the
 *            same comparison against mutated inputs, which has to MISMATCH.
 *            A green run therefore proves both that the port agrees and that
 *            the comparison can tell disagreement apart from agreement.
 *
 *   INVARIANT The coupling blend (morphCoup as a shared seed, ADR-176
 *            decision 1) has NO analogue in the prototype, so there is nothing
 *            to be parity with. It is pinned by properties instead: coup = 0
 *            is bit-identical to no coupling, coup = 1 collapses every atom
 *            onto one owner, and the walk's cumulative law matches its own
 *            definition over a swept seed.
 *
 *   SHELL     Phase 2b's section S: the FLAG, the `intent=` chunk and the
 *            SHADOW, driven through the shipped plugin. Neither the prototype
 *            nor IntentCore has any of the three, so parity cannot reach them
 *            and a green parity run says nothing about them. Section S carries
 *            its own controls: a plant that must change the render, a chunk
 *            token whose removal must change the readback, a binding that must
 *            move the shadow.
 *
 * Standalone and UNWIRED: ./verify does not run this (adding a gate is the
 * human's decision, charter §Oracle discipline; ADR-171 is the wiring route).
 * The parity half links nothing but src/intent_core.h; section S links the
 * shell, the way polarity_check and paramclass_check do.
 *
 * Usage: intent_check [build-golden/intent]
 */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "../src/intent_core.h"
#include "statefix_common.h"   // section S drives the shipped plugin

using hypersaw::IntentCore;

static int failures = 0;

/* ------------------------------------------------------------------ fixture

   A READER FOR OUR GENERATOR'S OUTPUT, not a JSON parser. The fixtures are
   written by gen_intent_goldens.mjs as a FLAT object — every key is a
   dotted string, every value a number, a string, or an array of numbers, and
   nothing nests. That makes an exact-key scan (`"key":`) complete and
   unambiguous, and it is the reason the generator flattens in the first
   place: a general parser here would be a few hundred lines of untested code
   standing between the oracle and the thing it checks. */
struct Fixture
{
  std::string text;

  bool load(const char *path)
  {
    FILE *f = std::fopen(path, "rb");
    if (f == nullptr) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    text.resize((size_t)(n > 0 ? n : 0));
    const size_t got = n > 0 ? std::fread(&text[0], 1, (size_t)n, f) : 0;
    std::fclose(f);
    return got == text.size();
  }

  // Offset just past `"<key>":`, or npos.
  size_t at(const std::string &key) const
  {
    const std::string needle = "\"" + key + "\":";
    const size_t p = text.find(needle);
    return p == std::string::npos ? std::string::npos : p + needle.size();
  }

  bool has(const std::string &key) const { return at(key) != std::string::npos; }

  double num(const std::string &key, double fallback = 0.0) const
  {
    const size_t p = at(key);
    if (p == std::string::npos) return fallback;
    return std::strtod(text.c_str() + p, nullptr);
  }

  int integer(const std::string &key, int fallback = 0) const
  {
    return (int)std::lround(num(key, (double)fallback));
  }

  std::vector<double> arr(const std::string &key) const
  {
    std::vector<double> out;
    size_t p = at(key);
    if (p == std::string::npos) return out;
    while (p < text.size() && text[p] != '[') p++;
    if (p >= text.size()) return out;
    p++;
    const char *s = text.c_str();
    while (p < text.size() && text[p] != ']')
    {
      if (text[p] == ',' || text[p] == ' ' || text[p] == '\n') { p++; continue; }
      char *end = nullptr;
      out.push_back(std::strtod(s + p, &end));
      if (end == s + p) break;
      p = (size_t)(end - s);
    }
    return out;
  }

  std::string str(const std::string &key) const
  {
    size_t p = at(key);
    if (p == std::string::npos) return {};
    while (p < text.size() && text[p] != '"') p++;
    if (p >= text.size()) return {};
    p++;
    std::string out;
    while (p < text.size() && text[p] != '"')
    {
      if (text[p] == '\\' && p + 1 < text.size()) p++;
      out.push_back(text[p++]);
    }
    return out;
  }
};

/* ------------------------------------------------------------------- inputs

   One input set (a "b." or "c." block). Everything the resolver and the
   harness around it need for a full run. */
struct Inputs
{
  int nParams = 0, nIntents = 0, nAtoms = 0, ticks = 0;
  double dt = 0.016;
  int lfoRateParam = 0;
  double steepness = 8, coup = 0;
  int useDrawnSeeds = 0;
  uint32_t deviceSeed = 0;
  std::vector<double> seeds;
  double sharedSeed = 0;
  std::vector<double> atomOf;
  int homeAtom = 0, unisonAtom = 0;
  double morphX = 0, morphY = 0, morph2X = 0, morph2Y = 0;
  int switchTick = -1, dragTicks = 0;
  double dragX = 0, dragY = 0;
  int latch = 0;
  double puckX = 0.5, puckY = 0.5, puckVX = 0, puckVY = 0;
  std::vector<double> knob, padDriven;
  std::vector<double> base, rangeLo, rangeHi, bind, homeXs, homeYs, unisonReq;
  int gTarget = -1;
  double gRate = 0, gDepth = 0, gPhase0 = 0;
  int gArmor = 0;
  std::vector<double> cLfoTarget, cLfoDepth, cLfoScope;
  double cPhase0 = 0;
  int sweepIntent = -1, commitAt = -1;

  void read(const Fixture &fx, const char *pfx)
  {
    const std::string p(pfx);
    nParams = fx.integer(p + "nParams");
    nIntents = fx.integer(p + "nIntents");
    nAtoms = fx.integer(p + "nAtoms");
    ticks = fx.integer(p + "ticks");
    dt = fx.num(p + "dt");
    lfoRateParam = fx.integer(p + "lfoRateParam");
    steepness = fx.num(p + "steepness");
    coup = fx.num(p + "coup");
    useDrawnSeeds = fx.integer(p + "useDrawnSeeds");
    deviceSeed = (uint32_t)fx.num(p + "deviceSeed");
    seeds = fx.arr(p + "seeds");
    sharedSeed = fx.num(p + "sharedSeed");
    atomOf = fx.arr(p + "atomOf");
    homeAtom = fx.integer(p + "homeAtom");
    unisonAtom = fx.integer(p + "unisonAtom");
    morphX = fx.num(p + "morphX");
    morphY = fx.num(p + "morphY");
    morph2X = fx.num(p + "morph2X");
    morph2Y = fx.num(p + "morph2Y");
    switchTick = fx.integer(p + "switchTick", -1);
    dragTicks = fx.integer(p + "dragTicks");
    dragX = fx.num(p + "dragX");
    dragY = fx.num(p + "dragY");
    latch = fx.integer(p + "latch");
    puckX = fx.num(p + "puckX");
    puckY = fx.num(p + "puckY");
    puckVX = fx.num(p + "puckVX");
    puckVY = fx.num(p + "puckVY");
    knob = fx.arr(p + "knob");
    padDriven = fx.arr(p + "padDriven");
    base = fx.arr(p + "base");
    rangeLo = fx.arr(p + "rangeLo");
    rangeHi = fx.arr(p + "rangeHi");
    bind = fx.arr(p + "bind");
    homeXs = fx.arr(p + "homeXs");
    homeYs = fx.arr(p + "homeYs");
    unisonReq = fx.arr(p + "unisonReq");
    gTarget = fx.integer(p + "gTarget", -1);
    gRate = fx.num(p + "gRate");
    gDepth = fx.num(p + "gDepth");
    gPhase0 = fx.num(p + "gPhase0");
    gArmor = fx.integer(p + "gArmor");
    cLfoTarget = fx.arr(p + "cLfoTarget");
    cLfoDepth = fx.arr(p + "cLfoDepth");
    cLfoScope = fx.arr(p + "cLfoScope");
    cPhase0 = fx.num(p + "cPhase0");
    sweepIntent = fx.integer(p + "sweepIntent", -1);
    commitAt = fx.integer(p + "commitAt", -1);
  }
};

/* --------------------------------------------------------------- recording */
struct Run
{
  std::vector<double> weights, final_, intents, puck, baseAfter, macroAfter, unisonValue;
  std::vector<double> owner, clamped, homeOwner, unisonOwner;
};

/* The modulation SOURCES the prototype carries — one global LFO and one
   per-corner LFO sharing a phase — live here in the harness, NOT in
   IntentCore: the resolver's law is tiering and ownership, and a core that
   owned an oscillator would be the wrong seam for a shell that already has
   its own. The encodings mirror gen_intent_goldens.mjs. */
static constexpr int T_NONE = -1, T_MORPHX = -2, T_MORPHY = -3, T_INTENTX = -4;
static constexpr double kTwoPi = 6.283185307179586476925286766559;

static Run drive(const Inputs &in)
{
  const int N = in.nParams, I = in.nIntents;
  Run r;
  std::vector<double> base(in.base);
  std::vector<double> homeXs(in.homeXs), homeYs(in.homeYs);
  std::vector<double> knob(in.knob);
  std::vector<double> seeds(in.seeds);
  double sharedSeed = in.sharedSeed;

  // T9's direct half: when the fixture says the seeds came from a device seed,
  // REDRAW them here instead of trusting the recorded array, so the mulberry32
  // stream itself is under test and not merely transcribed.
  if (in.useDrawnSeeds != 0)
  {
    seeds.assign((size_t)in.nAtoms, 0.0);
    IntentCore::drawSeeds(in.deviceSeed, seeds.data(), in.nAtoms, &sharedSeed);
    for (int a = 0; a < in.nAtoms; a++)
      if (std::fabs(seeds[(size_t)a] - in.seeds[(size_t)a]) > 1e-12)
      {
        std::printf("      RNG DIVERGENCE at atom %d: %.17g vs %.17g\n",
                    a, seeds[(size_t)a], in.seeds[(size_t)a]);
        failures++;
      }
  }

  std::vector<int> atomOf((size_t)N), ownerAtom((size_t)in.nAtoms), ownerParam((size_t)N);
  for (int p = 0; p < N; p++) atomOf[(size_t)p] = (int)std::lround(in.atomOf[(size_t)p]);
  std::vector<int> padDriven((size_t)I), clamped((size_t)N), respect((size_t)N);
  for (int i = 0; i < I; i++) padDriven[(size_t)i] = (int)std::lround(in.padDriven[(size_t)i]);

  std::vector<double> intents((size_t)I), finals((size_t)N);
  std::vector<double> cornerMod((size_t)N), promoted((size_t)N), device((size_t)N);
  double w[IntentCore::kCorners];

  IntentCore::Puck puck;
  puck.x = in.puckX; puck.y = in.puckY; puck.vx = in.puckVX; puck.vy = in.puckVY;
  const IntentCore::Spring spring;
  double gPhase = in.gPhase0, cPhase = in.cPhase0;

  for (int t = 0; t < in.ticks; t++)
  {
    const bool second = in.switchTick >= 0 && t >= in.switchTick;
    const double mx = second ? in.morph2X : in.morphX;
    const double my = second ? in.morph2Y : in.morphY;
    const bool dragging = t < in.dragTicks;
    if (in.sweepIntent >= 0) knob[(size_t)in.sweepIntent] = std::sin(t * 0.13);

    // --- modulation sources (harness) ---
    gPhase = std::fmod(gPhase + in.gRate * in.dt, 1.0);
    const double gl = std::sin(gPhase * kTwoPi) * in.gDepth;

    // --- §4.1 / §4.2 / §4.3 ---
    const double ex = IntentCore::effectiveMorph(mx, in.gTarget == T_MORPHX ? gl * 0.5 : 0.0);
    const double ey = IntentCore::effectiveMorph(my, in.gTarget == T_MORPHY ? gl * 0.5 : 0.0);
    IntentCore::weights(ex, ey, in.steepness, w);
    IntentCore::resolveAtoms(seeds.data(), in.nAtoms, sharedSeed, in.coup, w, ownerAtom.data());
    IntentCore::mapOwners(ownerAtom.data(), atomOf.data(), N, ownerParam.data());
    const int homeOwner = ownerAtom[(size_t)in.homeAtom];
    const int unisonOwner = ownerAtom[(size_t)in.unisonAtom];

    // --- §4.4 ---
    IntentCore::padStep(puck, in.dt, dragging, in.dragX, in.dragY, in.latch != 0,
                        homeXs[(size_t)homeOwner], homeYs[(size_t)homeOwner], spring);
    for (int i = 0; i < I; i++) intents[(size_t)i] = knob[(size_t)i];
    intents[0] = IntentCore::padIntentX(puck, homeXs[(size_t)homeOwner])
               + (in.gTarget == T_INTENTX ? gl : 0.0);
    intents[1] = IntentCore::padIntentY(puck, homeYs[(size_t)homeOwner]);

    // The corner LFO's RATE is itself a morphable parameter, so §4.5's corner
    // tier is evaluated for it FIRST — base + intents clamped to the owner's
    // range, with no mod tiers (the prototype's pre-pass, which ADR-003 makes
    // the law).
    {
      const int c = ownerParam[(size_t)in.lfoRateParam];
      const int cp = IntentCore::cornerParamIdx(c, in.lfoRateParam, N);
      const double rate = IntentCore::evalCornerTier(
          base[(size_t)cp],
          IntentCore::intentContrib(intents.data(), in.bind.data(), c, in.lfoRateParam, I, N),
          0.0, in.rangeLo[(size_t)cp], in.rangeHi[(size_t)cp], nullptr);
      cPhase = std::fmod(cPhase + 0.1 * std::pow(120.0, rate) * in.dt, 1.0);
    }
    const double cl = std::sin(cPhase * kTwoPi);

    // --- §4.5 tier inputs ---
    for (int p = 0; p < N; p++)
    {
      const int oc = ownerParam[(size_t)p];
      // corner scope: ONLY the owning corner's routing, inside the clamp
      cornerMod[(size_t)p] =
          (in.cLfoScope[(size_t)oc] == 0 && (int)std::lround(in.cLfoTarget[(size_t)oc]) == p)
              ? cl * in.cLfoDepth[(size_t)oc] * 0.5 : 0.0;
      // promoted (global scope): every corner's routing, regardless of owner,
      // outside the clamp
      double prom = 0.0;
      for (int k = 0; k < IntentCore::kCorners; k++)
        if (in.cLfoScope[(size_t)k] != 0 && (int)std::lround(in.cLfoTarget[(size_t)k]) == p)
          prom += cl * in.cLfoDepth[(size_t)k] * 0.5;
      promoted[(size_t)p] = prom;
      device[(size_t)p] = (in.gTarget == p) ? gl : 0.0;
      respect[(size_t)p] = (in.gTarget == p && in.gArmor != 0) ? 1 : 0;
    }

    IntentCore::stepParams(N, I, intents.data(), ownerParam.data(), base.data(), in.bind.data(),
                           in.rangeLo.data(), in.rangeHi.data(), cornerMod.data(),
                           promoted.data(), device.data(), respect.data(),
                           finals.data(), clamped.data());

    // --- record ---
    for (int k = 0; k < IntentCore::kCorners; k++) r.weights.push_back(w[k]);
    for (int p = 0; p < N; p++) r.owner.push_back(ownerParam[(size_t)p]);
    for (int p = 0; p < N; p++) r.final_.push_back(finals[(size_t)p]);
    for (int p = 0; p < N; p++) r.clamped.push_back(clamped[(size_t)p]);
    for (int i = 0; i < I; i++) r.intents.push_back(intents[(size_t)i]);
    r.homeOwner.push_back(homeOwner);
    r.unisonOwner.push_back(unisonOwner);
    r.unisonValue.push_back(in.unisonReq[(size_t)unisonOwner]);
    r.puck.push_back(puck.x);
    r.puck.push_back(puck.y);

    if (t == in.commitAt)
      IntentCore::commit(N, I, intents.data(), ownerParam.data(), w, base.data(), in.bind.data(),
                         in.rangeLo.data(), in.rangeHi.data(), puck, homeXs.data(), homeYs.data(),
                         knob.data(), padDriven.data(), nullptr);
  }
  r.baseAfter = base;
  // Knob intents only — the pad-driven slots are an output of §4.4 and are
  // already compared per tick as `intents`.
  for (int i = 0; i < I; i++) if (padDriven[(size_t)i] == 0) r.macroAfter.push_back(knob[(size_t)i]);
  return r;
}

/* ------------------------------------------------------------- comparisons */
static const double kTol = 1e-6;

struct Diff
{
  const char *field = nullptr;
  int index = -1;
  double got = 0, want = 0;
  bool any() const { return field != nullptr; }
};

static void cmp(Diff &d, const char *field, const std::vector<double> &got,
                const std::vector<double> &want)
{
  if (d.any()) return;
  if (got.size() != want.size())
  {
    d = {field, -1, (double)got.size(), (double)want.size()};
    return;
  }
  for (size_t i = 0; i < got.size(); i++)
    if (!(std::fabs(got[i] - want[i]) <= kTol))
    {
      d = {field, (int)i, got[i], want[i]};
      return;
    }
}

static Diff compare(const Run &r, const Fixture &fx)
{
  Diff d;
  cmp(d, "weights", r.weights, fx.arr("e.weights"));
  cmp(d, "owner", r.owner, fx.arr("e.owner"));
  cmp(d, "homeOwner", r.homeOwner, fx.arr("e.homeOwner"));
  cmp(d, "unisonOwner", r.unisonOwner, fx.arr("e.unisonOwner"));
  cmp(d, "unisonValue", r.unisonValue, fx.arr("e.unisonValue"));
  cmp(d, "intents", r.intents, fx.arr("e.intents"));
  cmp(d, "puck", r.puck, fx.arr("e.puck"));
  cmp(d, "clamped", r.clamped, fx.arr("e.clamped"));
  cmp(d, "final", r.final_, fx.arr("e.final"));
  cmp(d, "baseAfter", r.baseAfter, fx.arr("e.baseAfter"));
  cmp(d, "macroAfter", r.macroAfter, fx.arr("e.macroAfter"));
  return d;
}

/* ------------------------------------------------------------- invariants */
// The coupling blend has no prototype analogue (ADR-176 decision 1 is newer
// than the lab), so it is pinned by property, never by parity.
static void check(bool ok, const char *what, const char *detail)
{
  std::printf("  %-4s %s  (%s)\n", ok ? "OK" : "FAIL", what, detail);
  if (!ok) failures++;
}

static void invariants()
{
  std::printf("\ninvariants (no prototype analogue — properties, not parity)\n");
  char d[256];
  double w[IntentCore::kCorners];
  IntentCore::weights(0.37, 0.61, 3.0, w);

  double seeds[16], shared = 0;
  IntentCore::drawSeeds(4242u, seeds, 16, &shared);

  {
    int a[16], b[16];
    IntentCore::resolveAtoms(seeds, 16, shared, 0.0, w, a);
    IntentCore::resolveAtoms(seeds, 16, 0.999, 0.0, w, b);  // shared seed ignored at coup 0
    bool same = true;
    for (int i = 0; i < 16; i++) if (a[i] != b[i]) same = false;
    std::snprintf(d, sizeof d, "coup=0, two different shared seeds");
    check(same, "coup 0 is bit-identical to no coupling", d);
  }
  {
    int a[16];
    IntentCore::resolveAtoms(seeds, 16, shared, 1.0, w, a);
    bool one = true;
    for (int i = 1; i < 16; i++) if (a[i] != a[0]) one = false;
    std::snprintf(d, sizeof d, "coup=1 -> every atom owned by %d", a[0]);
    check(one, "coup 1 collapses the field onto one atom boundary", d);
  }
  {
    // The walk must be monotone in the seed: sweeping a seed 0 -> 1 visits the
    // corners in A,B,C,D order and never goes back. This is the property that
    // makes ONE seed ONE editable boundary — the reason ADR-176 chose the walk
    // over Gumbel-max in the first place.
    int last = 0; bool monotone = true;
    for (int i = 0; i <= 10000; i++)
    {
      const int o = IntentCore::ownerOf(i / 10000.0, w);
      if (o < last) monotone = false;
      last = o;
    }
    std::snprintf(d, sizeof d, "10001 seeds swept, final owner %d", last);
    check(monotone && last == 3, "the walk is monotone in the seed (one boundary per atom)", d);
  }
  {
    // THE EXACT BOUNDARY. `ownerOf` uses a STRICT `<`, so a seed sitting
    // exactly on a cumulative weight belongs to the NEXT corner. No fixture
    // can reach this: the generator's seeds come from mulberry32 and never
    // land on a normalised partial sum, so flipping `<` to `<=` in the core
    // leaves all 32 parity fixtures green (measured, 2026-09-18). That is a
    // coverage boundary of the reference oracle, recorded rather than
    // papered over (L0033), and closed HERE where the weights can be
    // constructed by hand instead of drawn.
    double half[IntentCore::kCorners] = {0.5, 0.5, 0.0, 0.0};
    const bool onBoundary = IntentCore::ownerOf(0.5, half) == 1;
    const bool belowBoundary = IntentCore::ownerOf(0.5 - 1e-15, half) == 0;
    std::snprintf(d, sizeof d, "seed 0.5 on w={.5,.5,0,0} -> corner %d (strict <)",
                  IntentCore::ownerOf(0.5, half));
    check(onBoundary && belowBoundary, "a seed exactly on a boundary belongs to the NEXT corner", d);
  }
  {
    // A control that MUST read non-zero: a deliberately wrong walk (strict >
    // instead of <) has to disagree somewhere, or the sweep above proves
    // nothing about the comparison.
    bool disagrees = false;
    for (int i = 0; i <= 1000; i++)
    {
      const double s = i / 1000.0;
      double acc = 0; int wrong = 3;
      for (int k = 0; k < IntentCore::kCorners; k++) { acc += w[k]; if (s <= acc - 1e-3) { wrong = k; break; } }
      if (wrong != IntentCore::ownerOf(s, w)) disagrees = true;
    }
    check(disagrees, "calibration: a shifted walk boundary is detectable", "must read non-zero");
  }
  {
    // §4.2 sanity: the sharpened weights are a distribution, and an exact
    // corner is one-hot at every steepness.
    double c[IntentCore::kCorners];
    bool ok = true;
    const double steeps[4] = {1.0, 2.0, 8.0, 24.0};
    for (int si = 0; si < 4; si++)
    {
      const double k = steeps[si];
      IntentCore::weights(1.0, 0.0, k, c);
      if (!(std::fabs(c[1] - 1.0) < 1e-12 && std::fabs(c[0]) < 1e-12)) ok = false;
      IntentCore::weights(0.42, 0.73, k, c);
      double s = 0; for (int j = 0; j < 4; j++) s += c[j];
      if (std::fabs(s - 1.0) > 1e-12) ok = false;
    }
    check(ok, "weights normalise and a corner stays one-hot at every steepness", "k in {1,2,8,24}");
  }
}

/* ===================== SECTION S — THE SHELL (B89 phase 2b) ================
   Everything above this line is the RESOLVER's oracle: pure math against the
   prototype. Section S is the SEAM's oracle, and it drives the shipped plugin
   — the flag, the chunk, the shadow — because none of those three exist at the
   header's level, so a green parity run certifies nothing about them (L0031:
   a reference oracle covers only the surface the reference spans).

   S1  a patch that has bound nothing writes NO `intent=` key, in either
       transport, and reads back the documented defaults: range [0,1],
       bind 0, home {0.5, 0.5}, the ADR-176 A3 captions.
   S2  the chunk round-trips through the host transport, and a chunk carrying a
       rename, a range, a binding and a home arrives as all four. CONTROL: the
       same readback against a chunk with the binding token removed must
       DISAGREE, or S2 cannot tell a parsed chunk from an ignored one.
   S3  THE BIT-IDENTITY CONTROL. Flag off, the resolver's output is unused, so
       the render is what it always was — and this proves the comparison could
       have SEEN otherwise: the same patch with the shadow planted into the
       applied path (hypersaw_debug_intent_plant — exactly what 2c will wire)
       must render DIFFERENTLY. The plant asserts its own anchor (L0032): it
       reports how many slots it wrote, and a live parameter must have moved.
       S3e is the measured BOUNDARY of that control, recorded rather than
       retried (L0033) — see its own comment.
   S4  THE DEGENERATE-CASE IDENTITY that makes 2c safe. Flag on, no bindings,
       full ranges: the shadow is a PURE CORNER READ — at every sampled morph
       position, for every field slot, shadow[p] == the owning corner's stored
       value. Nothing the resolver adds can move a value until a binding does.
   S5  the same positions against morphStep's OWN law, reconstructed from the
       shipped owner query (hypersaw_debug_ownersjson) and the shipped corner
       values — never from a second copy of the law. Where the two owner laws
       agree the values are identical to 1e-12; where they disagree, that is
       the flip-map change ADR-176 decision 1 ratified, counted and printed
       rather than hidden. S5x is the one position where they CANNOT disagree.
   S6  CONTROL for S4/S5: set one binding and a non-zero intent, and the shadow
       must MOVE. Without it, S4's agreement is equally consistent with a
       resolver that never ran.
   S7  atoms are lead groups: the thirteen scale ids report ONE owner at every
       position. CONTROL: some other slot must sit on a different corner, or
       "they agree" is only "everything agrees".

   SECTION T (below, phase 2c) is the APPLY's oracle: SPEC-INTENT-BUS §12's
   numbered tests measured on the shipped instrument now that the resolved
   value reaches the engine. Section S keeps its 2b meaning unchanged and is
   re-run verbatim — with the apply live, every one of its assertions still
   holds, which is acceptance (b) and (c) of the 2c brief.                 */

extern "C" double hypersaw_debug_intent_final(const clap_plugin_t *, int);
extern "C" int hypersaw_debug_intent_owner(const clap_plugin_t *, int);
extern "C" double hypersaw_debug_intent_bind(const clap_plugin_t *, int, int, int);
extern "C" bool hypersaw_debug_intent_range(const clap_plugin_t *, int, int, double *, double *);
extern "C" bool hypersaw_debug_intent_home(const clap_plugin_t *, int, double *, double *);
extern "C" const char *hypersaw_debug_intent_names(const clap_plugin_t *);
extern "C" int hypersaw_debug_intent_plant(const clap_plugin_t *);
extern "C" int hypersaw_debug_intent_commit(const clap_plugin_t *, int);
extern "C" bool hypersaw_debug_intent_break_atom(const clap_plugin_t *, int);
extern "C" void hypersaw_debug_gesture(const clap_plugin_t *, uint32_t, bool);
extern "C" void hypersaw_debug_intent_puck(const clap_plugin_t *, double *, double *);
extern "C" int hypersaw_debug_intent_homeowner(const clap_plugin_t *);
extern "C" const char *hypersaw_debug_modroutes(const clap_plugin_t *);
extern "C" void hypersaw_debug_capture(const clap_plugin_t *, int);
extern "C" bool hypersaw_debug_exempt(const clap_plugin_t *, uint32_t);
extern "C" const char *hypersaw_debug_undo(const clap_plugin_t *, const char *, int);
extern "C" const char *hypersaw_debug_cornervals(const clap_plugin_t *, int);
extern "C" bool hypersaw_debug_cornerapply(const clap_plugin_t *, int, const char *);
extern "C" const char *hypersaw_debug_ownersjson(const clap_plugin_t *);

namespace shell
{

using namespace statefix;

constexpr clap_id kIntentFlag = 266;
constexpr clap_id kMorphOn = 151, kMorphX = 152, kMorphY = 153, kMorphSeed = 156;
constexpr clap_id kMacro1 = 166;
constexpr int kIntents = 10;
constexpr double kEps = 1e-12;
// The crosspoint namespace. Deliberately NOT authored by this oracle: a dense
// random coefficient table is a topology no corner would ever hold, and under
// ADR-175 a cyclic one flips the whole FX pass to sample-by-sample — a CPU
// cliff that would make this check's cost a property of a coincidence.
constexpr clap_id kRoutingIdBase = 10000;

void say(bool ok, const std::string &what)
{
  std::printf("  %-4s %s\n", ok ? "OK" : "FAIL", what.c_str());
  if (!ok) failures++;
}

/* ---- the field, read the way the GUI reads it -----------------------------
   `hypersaw_debug_cornervals` emits `{"<id>":<value>,...}` in morphIds order,
   which IS the slot order the intent exports are indexed by. Parsed here
   rather than added as a slot->id export, so that mapping keeps one owner. */
struct Field
{
  std::vector<clap_id> ids;         // slot -> parameter id
  std::vector<double> corner[4];    // slot -> corner k's stored value

  void read(const clap_plugin_t *p)
  {
    for (int k = 0; k < 4; k++)
    {
      const std::string j = hypersaw_debug_cornervals(p, k);
      corner[k].clear();
      if (k == 0) ids.clear();
      size_t pos = 0;
      while ((pos = j.find('"', pos)) != std::string::npos)
      {
        const size_t q1 = j.find('"', pos + 1);
        if (q1 == std::string::npos) break;
        const clap_id id = (clap_id)std::strtoul(j.c_str() + pos + 1, nullptr, 10);
        const size_t colon = j.find(':', q1);
        if (colon == std::string::npos) break;
        if (k == 0) ids.push_back(id);
        corner[k].push_back(std::strtod(j.c_str() + colon + 1, nullptr));
        pos = colon + 1;
      }
    }
  }
  size_t n() const { return ids.size(); }
  int slotOf(clap_id id) const
  {
    for (size_t i = 0; i < ids.size(); i++)
      if (ids[i] == id) return (int)i;
    return -1;
  }
};

/* morphStep's OWN owner map, id-keyed: -1 exempt / field off, -2 held
   (ADR-108), otherwise the corner its Gumbel law picked. READ, never
   recomputed — a second copy of pickCorner here would certify the copy. */
std::map<clap_id, int> shippedOwners(const clap_plugin_t *p)
{
  std::map<clap_id, int> out;
  const std::string j = hypersaw_debug_ownersjson(p);
  size_t pos = 0;
  while ((pos = j.find('"', pos)) != std::string::npos)
  {
    const size_t q1 = j.find('"', pos + 1);
    if (q1 == std::string::npos) break;
    const clap_id id = (clap_id)std::strtoul(j.c_str() + pos + 1, nullptr, 10);
    const size_t colon = j.find(':', q1);
    if (colon == std::string::npos) break;
    out[id] = std::atoi(j.c_str() + colon + 1);
    pos = colon + 1;
  }
  return out;
}

/* ---- a driver that can actually make intentStep run -----------------------
   The resolver runs on the gravity grid inside process(), so a flush cannot
   reach it: every sampled position below costs two blocks of silence. */
struct Live
{
  const clap_plugin_t *p;
  std::vector<float> L, R;
  float *chans[2];
  clap_audio_buffer_t ob{};
  clap_process_t proc{};

  explicit Live(const clap_plugin_t *pl) : p(pl), L(kBlock), R(kBlock)
  {
    chans[0] = L.data();
    chans[1] = R.data();
    ob.data32 = chans;
    ob.channel_count = 2;
    proc.frames_count = kBlock;
    proc.audio_outputs = &ob;
    proc.audio_outputs_count = 1;
    proc.out_events = &kOut;
    p->activate(p, kSampleRate, 32, 1024);
    p->start_processing(p);
  }
  ~Live()
  {
    p->stop_processing(p);
    p->deactivate(p);
  }
  void run(int blocks, EvList *ev = nullptr)
  {
    EvList none;
    for (int b = 0; b < blocks; b++)
    {
      proc.in_events = (b == 0 && ev != nullptr) ? &ev->list : &none.list;
      p->process(p, &proc);
    }
  }
};

/* ---- a patch with four genuinely different corners ------------------------
   Authored through the shipped corner-preset surface (cornerApply), so nothing
   here reaches past a door the GUI already opens. Values are rounded to six
   significant digits on purpose: `hypersaw_debug_cornervals` prints %.10g, and
   a reference read back through a 10-digit print cannot support a 1e-12
   comparison unless the value survives that print exactly. */
void authorCorners(const clap_plugin_t *p, const Field &f, double spread)
{
  auto *params = paramsOf(p);
  const uint32_t n = params->count(p);
  std::map<clap_id, std::pair<double, double>> range;
  std::map<clap_id, bool> stepped;
  for (uint32_t i = 0; i < n; i++)
  {
    clap_param_info_t info{};
    if (!params->get_info(p, i, &info)) continue;
    range[info.id] = std::pair<double, double>(info.min_value, info.max_value);
    stepped[info.id] = (info.flags & CLAP_PARAM_IS_STEPPED) != 0;
  }
  for (int k = 0; k < 4; k++)
  {
    std::string json = "{\"morphLayout\":5,\"cornerPreset\":[";
    char buf[48];
    for (size_t s = 0; s < f.n(); s++)
    {
      double v = f.corner[k][s];
      const auto it = range.find(f.ids[s]);
      if (it != range.end() && !stepped[f.ids[s]] && f.ids[s] < kRoutingIdBase)
      {
        const double frac = 0.30 + spread * (double)k;
        std::snprintf(buf, sizeof buf, "%.6g",
                      it->second.first + frac * (it->second.second - it->second.first));
        v = std::atof(buf);
      }
      std::snprintf(buf, sizeof buf, s ? ",%.17g" : "%.17g", v);
      json += buf;
    }
    json += "]}";
    if (!hypersaw_debug_cornerapply(p, k, json.c_str())) say(false, "corner apply refused");
  }
}

/* The sampled morph positions: the four EXACT corners first (both laws are
   one-hot there, so identity must be total), then a deterministic interior
   lattice. 200 in all, the number the acceptance names. */
std::vector<std::pair<double, double>> positions()
{
  std::vector<std::pair<double, double>> out;
  out.push_back(std::pair<double, double>(0, 0));
  out.push_back(std::pair<double, double>(1, 0));
  out.push_back(std::pair<double, double>(0, 1));
  out.push_back(std::pair<double, double>(1, 1));
  for (int i = 0; out.size() < 200; i++)
  {
    const int gx = i % 14, gy = (i / 14) % 14;
    out.push_back(std::pair<double, double>((gx + 0.5) / 14.0, (gy + 0.5) / 14.0));
  }
  return out;
}

const char *const kDefaultNames[kIntents] = {"X",      "Y",    "Space",      "Timbre",
                                             "Motion", "Grit", "Time",       "Character",
                                             "Brightness", "Pressure"};

/* ---- S1 + S2: the chunk ------------------------------------------------- */
void chunkSection()
{
  std::printf("\nS1/S2 — the intent= chunk (silent on defaults, parsed when present)\n");
  {
    const clap_plugin_t *p = makePlugin();
    const std::string chunk = saveChunk(p);
    const std::string json = saveJson(p);
    say(chunk.find("\nintent=") == std::string::npos,
        "S1a a patch that has bound nothing writes no `intent=` line");
    say(json.find("\"intent\"") == std::string::npos,
        "S1b ... and no \"intent\" key in the preset transport either");
    // Reading the field is what builds it (morphCornerValsJson calls morphInit),
    // and it must happen AFTER the two saves above or the corners would join
    // the very bytes S1a/S1b are asserting about.
    Field f0;
    f0.read(p);
    double lo = -1, hi = -1, hx = -1, hy = -1;
    bool defs = hypersaw_debug_intent_range(p, 2, 7, &lo, &hi) && lo == 0.0 && hi == 1.0;
    defs = defs && hypersaw_debug_intent_home(p, 3, &hx, &hy) && hx == 0.5 && hy == 0.5;
    defs = defs && hypersaw_debug_intent_bind(p, 1, 4, 9) == 0.0;
    say(defs, "S1c absent means range [0,1], bind 0, home {0.5,0.5}");
    const std::string names = hypersaw_debug_intent_names(p);
    bool ok = true;
    for (int i = 0; i < kIntents; i++)
      if (names.find(std::string("\"") + kDefaultNames[i] + "\"") == std::string::npos) ok = false;
    say(ok, std::string("S1d the ADR-176 A3 captions are the defaults: ") + names);
    p->destroy(p);
  }

  /* A chunk that leaves the defaults in all four ways. The ids are read off an
     instance rather than guessed: slot 3's id is whatever morphIds has there,
     and hard-coding it would be a second copy of that order. */
  const clap_plugin_t *probe = makePlugin();
  Field pf;
  pf.read(probe);
  const clap_id idA = pf.ids.at(3), idB = pf.ids.at(11);
  probe->destroy(probe);

  char tok[128];
  std::string body = "L:1,O:X:Y:M1:M2:M3:M4:M5:M6:M7:M8,N:4:Swell";
  std::snprintf(tok, sizeof tok, ",R:%u:1:0.25:0.75", (unsigned)idA);
  body += tok;
  std::snprintf(tok, sizeof tok, ",B:%u:1:4:0.125", (unsigned)idB);
  const std::string bindTok = tok;
  body += bindTok;
  body += ",H:2:0.25:0.75";

  struct Back
  {
    double lo = 0, hi = 0, bind = -1, hx = 0, hy = 0;
    std::string names, resavedLine;
  };
  auto readBack = [&](const std::string &chunkBody) {
    Back b;
    const clap_plugin_t *p = makePlugin();
    std::string blob = saveChunk(p);
    blob += "intent=" + chunkBody + "\n";
    if (loadChunk(p, blob))
    {
      Field g;
      g.read(p);
      hypersaw_debug_intent_range(p, 1, g.slotOf(idA), &b.lo, &b.hi);
      b.bind = hypersaw_debug_intent_bind(p, 1, 4, g.slotOf(idB));
      hypersaw_debug_intent_home(p, 2, &b.hx, &b.hy);
      b.names = hypersaw_debug_intent_names(p);
      const std::string re = saveChunk(p);
      const size_t at = re.find("\nintent=");
      if (at != std::string::npos)
        b.resavedLine = re.substr(at + 8, re.find('\n', at + 1) - at - 8);
    }
    p->destroy(p);
    return b;
  };

  const Back got = readBack(body);
  say(got.lo == 0.25 && got.hi == 0.75, "S2a a stored range arrives");
  say(got.bind == 0.125, "S2b a stored binding arrives");
  say(got.hx == 0.25 && got.hy == 0.75, "S2c a stored home arrives");
  say(got.names.find("\"Swell\"") != std::string::npos, "S2d a stored rename arrives");
  say(!got.resavedLine.empty() &&
          got.resavedLine.find(bindTok.substr(1)) != std::string::npos &&
          got.resavedLine.find("N:4:Swell") != std::string::npos &&
          got.resavedLine.find("H:2:0.25:0.75") != std::string::npos,
      "S2e the re-saved line carries the rename, the binding and the home");
  std::printf("       re-saved: intent=%s\n", got.resavedLine.c_str());

  // CONTROL: drop the binding token; the same readback must now report 0, and
  // the range token must still arrive — otherwise "0" would only mean the
  // whole chunk was ignored.
  std::string cut = body;
  cut.erase(cut.find(bindTok), bindTok.size());
  const Back ctl = readBack(cut);
  say(ctl.bind == 0.0 && ctl.lo == 0.25,
      "S2f CONTROL: with the binding token removed the readback reports 0, while the "
      "range token still arrives");
}

/* ---- S3: the bit-identity control --------------------------------------- */
void plantSection()
{
  std::printf("\nS3 — flag off is bit-identical, and the comparison can see otherwise\n");

  struct Leg
  {
    std::vector<float> audio;
    int wrote = 0;
    double moved = 0;
  };
  auto renderPatch = [](bool plant, bool morphOn) {
    Leg leg;
    const clap_plugin_t *p = makePlugin();
    Field f;
    f.read(p);
    authorCorners(p, f, 0.15);
    EvList ev;
    ev.push(kMorphOn, morphOn ? 1 : 0);
    ev.push(kMorphX, 0.37);
    ev.push(kMorphY, 0.61);
    ev.push(kIntentFlag, 0);   // THE FLAG IS OFF on every leg here
    paramsOf(p)->flush(p, &ev.list, &kOut);
    drain(p);
    const int probe = f.slotOf(4);   // detune: continuous, in the field, audible
    double before = 0, after = 0;
    paramsOf(p)->get_value(p, f.ids[probe], &before);
    if (plant) leg.wrote = hypersaw_debug_intent_plant(p);
    paramsOf(p)->get_value(p, f.ids[probe], &after);
    leg.moved = std::fabs(after - before);
    render(p, leg.audio);
    p->destroy(p);
    return leg;
  };

  const Leg a = renderPatch(false, false);
  const Leg b = renderPatch(false, false);
  const Leg c = renderPatch(true, false);
  say(a.audio == b.audio,
      "S3a the same patch at flag 0 renders bit-identically twice (the comparison's floor)");
  say(rms(a.audio) > 1e-6,
      "S3b ... and the render is not silence, so byte-equality means something");
  say(c.wrote > 0 && c.moved > 1e-9,
      "S3c the plant asserts its own ANCHOR: it wrote " + std::to_string(c.wrote) +
          " slots and moved detune by " + std::to_string(c.moved));
  say(a.audio != c.audio,
      "S3d CONTROL: the shadow planted into the applied path at flag 0 renders "
      "DIFFERENTLY — so `flag off is bit-identical` is a claim the comparison could "
      "have refuted");

  /* S3e — the same control with the MORPH ON, which is the configuration the
     bit-identity claim is actually about. A PREDICTION THAT MEASURED FALSE,
     recorded here because it is what 2c needs to know: the expectation was
     that morphStep, running at the head of the first block, would overwrite
     every field slot before a sample was rendered and make the plant
     inaudible. It does not — the plant is audible with the morph on too. So a
     writer placed BESIDE morphStep is not harmlessly overwritten; 2c's
     replacement (the early branch already built) is load-bearing, not
     stylistic. The first differing frame is printed so the next phase can see
     where the two paths part. */
  const Leg d = renderPatch(false, true);
  const Leg e = renderPatch(true, true);
  say(e.wrote > 0, "S3e1 the plant still wrote " + std::to_string(e.wrote) +
                       " slots with the morph on (the plant is not the thing that failed)");
  size_t firstDiff = d.audio.size();
  for (size_t i = 0; i < d.audio.size() && i < e.audio.size(); i++)
    if (d.audio[i] != e.audio[i]) { firstDiff = i; break; }
  say(d.audio != e.audio,
      "S3e2 CONTROL holds with the morph ON as well: the plant is audible from "
      "interleaved sample " + std::to_string(firstDiff) + " of " +
          std::to_string(d.audio.size()) +
          " — so a second writer beside morphStep would NOT be harmlessly "
          "overwritten, and 2c's replacement of that write is load-bearing");
}

/* ---- S4 / S5 / S6 / S7: the degenerate identity -------------------------- */
void identitySection()
{
  std::printf("\nS4/S5/S7 — flag on, no bindings: the shadow is a pure corner read\n");
  const uint32_t seeds[3] = {1024, 7, 4242};
  const double spreads[3] = {0.15, 0.22, 0.10};

  for (int patch = 0; patch < 3; patch++)
  {
    const clap_plugin_t *p = makePlugin();
    Field f;
    f.read(p);
    authorCorners(p, f, spreads[patch]);
    f.read(p);   // re-read: the corners are the authored ones now
    {
      EvList ev;
      ev.push(kMorphOn, 1);
      ev.push(kMorphSeed, (double)seeds[patch]);
      ev.push(kIntentFlag, 1);
      paramsOf(p)->flush(p, &ev.list, &kOut);
      drain(p);
    }
    Live live(p);
    const std::vector<std::pair<double, double>> pos = positions();

    double worstRead = 0, worstAgree = 0;
    size_t reads = 0, agree = 0, disagree = 0, noOwner = 0;
    size_t scaleChecked = 0, scaleSplit = 0, positionsWithASplit = 0;
    const int scale0 = f.slotOf(116);

    for (size_t q = 0; q < pos.size(); q++)
    {
      EvList ev;
      ev.push(kMorphX, pos[q].first);
      ev.push(kMorphY, pos[q].second);
      live.run(2, &ev);
      const std::map<clap_id, int> own = shippedOwners(p);

      for (size_t s = 0; s < f.n(); s++)
      {
        const int k = hypersaw_debug_intent_owner(p, (int)s);
        if (k < 0 || k > 3) { noOwner++; continue; }
        // S4: the resolver's output IS the owning corner's stored value.
        const double v = hypersaw_debug_intent_final(p, (int)s);
        const double d = std::fabs(v - f.corner[k][s]);
        if (d > worstRead) worstRead = d;
        reads++;
        // S5: against morphStep's own law, where the two laws agree.
        const std::map<clap_id, int>::const_iterator it = own.find(f.ids[s]);
        if (it == own.end() || it->second < 0) continue;   // exempt / held: morphStep holds
        if (it->second == k)
        {
          const double e = std::fabs(v - f.corner[it->second][s]);
          if (e > worstAgree) worstAgree = e;
          agree++;
        }
        else disagree++;
      }
      // S7: the scale is ONE atom, and something else is not on its corner.
      if (scale0 >= 0)
      {
        const int k0 = hypersaw_debug_intent_owner(p, scale0);
        for (int deg = 1; deg <= 12; deg++)
        {
          const int sd = f.slotOf((clap_id)(116 + deg));
          if (sd < 0) continue;
          scaleChecked++;
          if (hypersaw_debug_intent_owner(p, sd) != k0) scaleSplit++;
        }
        for (size_t s = 0; s < f.n(); s++)
          if (hypersaw_debug_intent_owner(p, (int)s) != k0) { positionsWithASplit++; break; }
      }
    }

    char msg[400];
    std::snprintf(msg, sizeof msg,
                  "S4 patch %d (seed %u): %zu slot-reads over %zu positions, worst "
                  "|shadow - corner[owner]| = %.3g (tol %.0e)",
                  patch + 1, seeds[patch], reads, pos.size(), worstRead, kEps);
    say(worstRead <= kEps && reads > 0, msg);
    std::snprintf(msg, sizeof msg,
                  "S5 patch %d: the two owner laws AGREE on %zu reads (worst diff %.3g) and "
                  "DISAGREE on %zu — the ADR-176 decision-1 flip-map change, counted not hidden",
                  patch + 1, agree, worstAgree, disagree);
    say(worstAgree <= kEps && agree > 0, msg);
    std::snprintf(msg, sizeof msg,
                  "S7 patch %d: the 13 scale ids share one owner at every position "
                  "(%zu checks, %zu splits); CONTROL: %zu positions had some slot elsewhere",
                  patch + 1, scaleChecked, scaleSplit, positionsWithASplit);
    say(scaleSplit == 0 && scaleChecked > 0 && positionsWithASplit > 0, msg);
    std::printf("       (%zu reads had no owner — the resolver reported out of range)\n", noOwner);
    p->destroy(p);
  }

  /* S5x — the one position where the two laws CANNOT disagree: at an exact
     corner every weight is one-hot, so the walk and the Gumbel draw both land
     on it, and the identity with morphStep's own output is therefore total. */
  {
    const clap_plugin_t *p = makePlugin();
    Field f;
    f.read(p);
    authorCorners(p, f, 0.15);
    f.read(p);
    EvList ev;
    ev.push(kMorphOn, 1);
    ev.push(kIntentFlag, 1);
    paramsOf(p)->flush(p, &ev.list, &kOut);
    drain(p);
    Live live(p);
    const double xs[4] = {0, 1, 0, 1}, ys[4] = {0, 0, 1, 1};
    size_t checked = 0, bad = 0;
    double worst = 0;
    for (int corner = 0; corner < 4; corner++)
    {
      EvList e2;
      e2.push(kMorphX, xs[corner]);
      e2.push(kMorphY, ys[corner]);
      live.run(2, &e2);
      const std::map<clap_id, int> own = shippedOwners(p);
      for (size_t s = 0; s < f.n(); s++)
      {
        const std::map<clap_id, int>::const_iterator it = own.find(f.ids[s]);
        if (it == own.end() || it->second < 0) continue;
        checked++;
        if (it->second != hypersaw_debug_intent_owner(p, (int)s)) bad++;
        const double d = std::fabs(hypersaw_debug_intent_final(p, (int)s) - f.corner[it->second][s]);
        if (d > worst) worst = d;
      }
    }
    char msg[256];
    std::snprintf(msg, sizeof msg,
                  "S5x at the four EXACT corners both laws are one-hot: %zu reads, %zu owner "
                  "disagreements, worst |shadow - morphStep target| = %.3g",
                  checked, bad, worst);
    say(bad == 0 && worst <= kEps && checked > 0, msg);
    p->destroy(p);
  }

  /* S6 — the control. A binding plus a non-zero intent must MOVE the shadow;
     without it S4's agreement is equally consistent with a resolver that never
     ran at all. The binding is authored through the chunk, which is the only
     author of a binding this phase has. */
  {
    const clap_plugin_t *p = makePlugin();
    Field f;
    f.read(p);
    authorCorners(p, f, 0.15);
    f.read(p);
    const int slot = 3;
    auto arm = [&](const clap_plugin_t *pl, Live &live) {
      EvList ev;
      ev.push(kMorphOn, 1);
      ev.push(kIntentFlag, 1);
      ev.push(kMacro1, 1.0);   // macro 1 = intent M1, and the MAIN pad's X axis
      ev.push(kMorphX, 0.37);
      ev.push(kMorphY, 0.61);
      live.run(2, &ev);
      live.run(2);
      (void)pl;
    };
    Live live(p);
    arm(p, live);
    const double before = hypersaw_debug_intent_final(p, slot);

    char tok[96];
    std::string body = "L:1,O:X:Y:M1:M2:M3:M4:M5:M6:M7:M8";
    for (int k = 0; k < 4; k++)   // every corner, so the owner cannot matter
    {
      std::snprintf(tok, sizeof tok, ",B:%u:%d:2:0.2", (unsigned)f.ids[slot], k);
      body += tok;
    }
    std::string blob = saveChunk(p);
    blob += "intent=" + body + "\n";
    loadChunk(p, blob);
    arm(p, live);
    const double after = hypersaw_debug_intent_final(p, slot);
    say(hypersaw_debug_intent_bind(p, 0, 2, slot) == 0.2,
        "S6a the binding reached the table through the chunk");
    say(std::fabs(after - before) > 1e-9,
        "S6b CONTROL: with a binding and a non-zero intent the shadow MOVES (by " +
            std::to_string(std::fabs(after - before)) +
            ") — S4's agreement is not agreement by inaction");
    p->destroy(p);
  }
}

/* ===================== SECTION T — THE SPEC'S TESTS, THROUGH THE PLUGIN ====
   SPEC-INTENT-BUS §12's numbered tests measured on the SHIPPED INSTRUMENT with
   the flag on — not on IntentCore, which the parity half above already covers.
   That distinction is the whole reason this section exists: §12 is about what
   the DEVICE does, and the device is the resolver PLUS the field's application
   (the glide, ADR-125's stepped branch, ADR-109's exempt, ADR-108's hold,
   ADR-176's lead-group atoms) — none of which the header can see, and all of
   which are where 2c's risk lives.

   Every test carries its must-fail control, and every control is MEASURED, not
   argued (L0032); where a test could pass by inaction it also carries its own
   anchor — the count of positions it actually got to look at (L0033).

     TC   the APPLIED path (acceptance (c)): with nothing bound, every slot
          lands EXACTLY on the resolved target or holds what it had, never on
          a third value — and with the morph OFF the flag does nothing (R15,
          rendered).
     T1   a corner that LOCKS a parameter (lo == hi) pins it under a full
          intent sweep while that corner owns it. CONTROL: widen that one
          range and the same sweep must move it.
     T4   one intent, bound in every corner, acting through TWO corners at once
          — each parameter by its OWN owner's depth. CONTROL: sharpen
          (temp 0.02, steepness 50) until ownership collapses to one corner,
          and the second corner's depth goes silent.
     T5   four corners requesting a different `n` (voices, structural): a full
          sweep yields one of the four and never a blend; the scale is ONE
          atom. CONTROL: break the lead map and the chimera ADR-176 decision 2
          forbids appears — a scale no corner authored.
     T6   commit bakes the displacement into the dominant corner: the resolved
          output is unchanged and every offset reads zero. CONTROL: commit into
          a corner that owns nothing at this position.
     T9   the owner map is a function of `morphSeed`: same seed identical,
          different seed different, the four exact corners unchanged either way
          (the 2026-09-10 amendment's reading, not the spec's original T9).
     T10  THE HUMAN'S TEST: one intent bound in two corners with DIFFERENT
          ranges on an envelope parameter — the shallow corner stays shallow
          while it owns. CONTROL: swap the two ranges and the roles swap.
     TE   the corner gestures with the flag on — an armed edit still writes
          morphCorner, capture still bakes, exempt still holds, and one gesture
          is still one ADR-160 history node.
     TG   THE HAZARD the plan names: the corner clamp must be the OWNER's
          range, and a value HELD under ADR-108 must not be re-clamped into it.
*/
namespace spec
{

using namespace statefix;

// The subjects, chosen for what they are rather than for what they are called.
constexpr clap_id kFx1Type = 57;    // stepped, FX1's atom
constexpr clap_id kFx1Amt = 58;     // continuous, in FX1's atom, NO dependency:
                                    // the closest thing the shipped set has to
                                    // the spec's `cutoff`, and a pinned value
                                    // can therefore only be the lock.
constexpr clap_id kFx1Tone = 96;    // continuous, DEPENDS on fx1type == 5 — the
                                    // ADR-108 hold TG needs.
constexpr clap_id kDetune = 4;      // continuous, its own atom
constexpr clap_id kDecay = 20;      // the envelope parameter T10 asks for
constexpr clap_id kVoices = 1;      // stepped + structural: T5's `n`
constexpr clap_id kBendQuant = 114; // the scale's enabling condition
constexpr clap_id kScaleRoot = 116; // ... and the 13-id atom it heads
constexpr clap_id kMorphArm = 159, kMorphGlide = 158, kMorphTemp = 154;
constexpr int kM1 = 2;              // intent index of macro 1 (kIntentOrder)

/* A patch under test: four authored corners, the flag on, the glide immediate.
   The glide at 0 is not a convenience — it is what makes the ASSERTIONS about
   the applied value assertions about the resolver's target rather than about
   how far a one-pole had got (R16 keeps the glide; this pins it out of the
   measurement). */
struct Rig
{
  const clap_plugin_t *p = nullptr;
  Field f;

  explicit Rig(double spread = 0.15)
  {
    p = makePlugin();
    f.read(p);
    authorCorners(p, f, spread);
    f.read(p);
  }
  ~Rig() { if (p != nullptr) p->destroy(p); }
  Rig(const Rig &) = delete;
  Rig &operator=(const Rig &) = delete;

  int slot(clap_id id) const { return f.slotOf(id); }
  double live(clap_id id) const
  {
    double v = 0;
    paramsOf(p)->get_value(p, id, &v);
    return v;
  }
  int owner(clap_id id) const { return hypersaw_debug_intent_owner(p, f.slotOf(id)); }

  // Corner k's stored vector, written back verbatim — the door authorCorners
  // uses, so a test that wants one different value does not need a second one.
  void writeCorner(int k)
  {
    std::string json = "{\"morphLayout\":5,\"cornerPreset\":[";
    char buf[48];
    for (size_t s = 0; s < f.n(); s++)
    {
      std::snprintf(buf, sizeof buf, s ? ",%.17g" : "%.17g", f.corner[k][s]);
      json += buf;
    }
    json += "]}";
    if (!hypersaw_debug_cornerapply(p, k, json.c_str())) say(false, "corner apply refused");
  }
  void setCorner(int k, clap_id id, double v)
  {
    const int s = f.slotOf(id);
    if (s < 0) { say(false, "no field slot for a test subject"); return; }
    f.corner[k][(size_t)s] = v;
  }

  void push(const std::vector<std::pair<clap_id, double>> &kv)
  {
    EvList ev;
    for (size_t i = 0; i < kv.size(); i++) ev.push(kv[i].first, kv[i].second);
    paramsOf(p)->flush(p, &ev.list, &kOut);
    drain(p);
  }
  /* Bindings, ranges and homes have exactly one author in phase 2 — the chunk
     (2b's ruling: the oracle uses the shipped door rather than a write surface
     built for it). A load restores the params saved in the blob, so anything
     pushed before the load survives it. */
  void intentChunk(const std::string &body)
  {
    std::string blob = saveChunk(p);
    if (!body.empty()) blob += "intent=L:1,O:X:Y:M1:M2:M3:M4:M5:M6:M7:M8" + body + "\n";
    if (!loadChunk(p, blob)) say(false, "chunk load refused");
  }
  // The standing configuration of every test here: field on, resolver on,
  // glide immediate, a named seed.
  void arm(uint32_t seed, double temp = 1.0)
  {
    push({{kMorphOn, 1}, {kIntentFlag, 1}, {kMorphGlide, 0}, {kMorphTemp, temp},
          {kMorphSeed, (double)seed}});
  }
};

std::string bindAllCorners(clap_id id, int intent, double depth)
{
  std::string out;
  char tok[96];
  for (int k = 0; k < 4; k++)
  {
    std::snprintf(tok, sizeof tok, ",B:%u:%d:%d:%.17g", (unsigned)id, k, intent, depth);
    out += tok;
  }
  return out;
}
std::string rangeTok(clap_id id, int corner, double lo, double hi)
{
  char tok[96];
  std::snprintf(tok, sizeof tok, ",R:%u:%d:%.17g:%.17g", (unsigned)id, corner, lo, hi);
  return tok;
}

/* ---- TC: the APPLIED path, and the flag with the morph off --------------
   S4 (above, 2b) proves the resolver's OUTPUT is the owning corner's stored
   value. TC is the other half, and the half 2c adds: that the engine is
   carried to that value, through applyParam, and to nothing else.

   The contract is a disjunction, because the field's own rules are part of it:
   after a grid tick a slot holds EITHER the resolved target (exact here — the
   glide is pinned immediate, so what is measured is the target and not how far
   a one-pole had got) or exactly what it held before (ADR-108's hold, and the
   1e-9 deadband every field writer goes through). Anything else is a
   MISMATCHING id, and the question TC answers is whose it is.

   WHOSE IT IS, measured rather than argued. Three id families read back a
   value the field never gave them, and all three are the READ path's, not the
   resolver's:
     - `beatMult` (23) and the step grid (148): applyParam SNAPS them to
       rational beat increments. The destination owns its own law (ADR-088's
       rule, shipped long before this).
     - ids 44-55 / 65-68: readParam routes them to the SHARED `spectra` core
       while applyParam writes that core for EVERY oscillator's copy, so osc
       1's readback reports whatever osc 2 last applied.
     - `toneTilt` (71): the per-osc read collides with `tilt` (45) in the core
       key map — 1071 reads back a value outside its own declared range.
   So TC does not assert a number it would have to fix to go green. It asserts
   the SET of mismatching ids under the resolver is a SUBSET of the set the
   SHIPPED field produces on the same patch: the resolver introduces no
   mismatch of its own. Both sets are printed. (ADR candidates, out of this
   brief's scope — the read path is not 2c's to change.) */

struct Buckets
{
  size_t reads = 0, exact = 0, held = 0, mismatch = 0;
  double worstExact = 0, worstOwner = 0;
  std::map<clap_id, size_t> ids;   // mismatching id -> how often
};

std::string idList(const std::map<clap_id, size_t> &m)
{
  std::string out;
  for (std::map<clap_id, size_t>::const_iterator it = m.begin(); it != m.end(); ++it)
  {
    char b[32];
    std::snprintf(b, sizeof b, "%s%u(x%zu)", out.empty() ? "" : " ", (unsigned)it->first,
                  it->second);
    out += b;
  }
  return out.empty() ? "none" : out;
}

void tc()
{
  std::printf("\nTC — the applied path: the engine is carried to the resolved value\n");
  const uint32_t seeds[3] = {1024, 7, 4242};
  const double spreads[3] = {0.15, 0.22, 0.10};
  std::map<clap_id, size_t> onIds;
  for (int patch = 0; patch < 3; patch++)
  {
    Rig rig(spreads[patch]);
    rig.push({{kMorphOn, 1}, {kIntentFlag, 1}, {kMorphGlide, 0}, {kMorphTemp, 1},
              {kMorphSeed, (double)seeds[patch]}});
    // Zero intents, explicitly: two macros default to 0.5, and the acceptance
    // names the zero-intent case even though no binding exists to carry them.
    {
      std::vector<std::pair<clap_id, double>> zero;
      for (int i = 0; i < 8; i++)
        zero.push_back(std::pair<clap_id, double>((clap_id)(166 + i), 0.0));
      rig.push(zero);
    }
    Live live(rig.p);
    const std::vector<std::pair<double, double>> pos = positions();
    std::vector<double> prev(rig.f.n(), 0.0);
    for (size_t s = 0; s < rig.f.n(); s++) prev[s] = rig.live(rig.f.ids[s]);

    Buckets b;
    for (size_t q = 0; q < pos.size(); q++)
    {
      EvList ev;
      ev.push(kMorphX, pos[q].first);
      ev.push(kMorphY, pos[q].second);
      live.run(2, &ev);
      for (size_t s = 0; s < rig.f.n(); s++)
      {
        const double resolved = hypersaw_debug_intent_final(rig.p, (int)s);
        const double v = rig.live(rig.f.ids[s]);
        const int k = hypersaw_debug_intent_owner(rig.p, (int)s);
        const double d = std::fabs(v - resolved);
        b.reads++;
        if (d <= kEps)
        {
          b.exact++;
          b.worstExact = std::max(b.worstExact, d);
          // ... and the target is still the OWNER's corner value, so the
          // applied value is the owner's and not merely self-consistent.
          if (k >= 0 && k < 4)
            b.worstOwner = std::max(b.worstOwner, std::fabs(v - rig.f.corner[k][s]));
        }
        else if (v == prev[s] || d <= 1e-9) b.held++;
        else { b.mismatch++; b.ids[rig.f.ids[s]]++; onIds[rig.f.ids[s]]++; }
        prev[s] = v;
      }
    }
    char msg[520];
    std::snprintf(msg, sizeof msg,
                  "TC patch %d (seed %u): %zu slot-reads over %zu positions — %zu landed "
                  "EXACTLY on the resolved target (worst %.3g; worst |applied - corner[owner]| "
                  "= %.3g), %zu held (ADR-108 / the 1e-9 deadband), %zu read back something "
                  "else",
                  patch + 1, seeds[patch], b.reads, pos.size(), b.exact, b.worstExact,
                  b.worstOwner, b.held, b.mismatch);
    say(b.exact > b.reads / 2 && b.worstOwner <= kEps, msg);
  }
  std::printf("       ids that read back something else, WITH the resolver: %s\n",
              idList(onIds).c_str());

  /* ATTRIBUTION: the same measurement under the SHIPPED field, flag OFF,
     against the shipped owner query. Its only job is to say whether those ids
     belong to the resolver or to the read path — and it is the reason TC can
     assert something true instead of excluding ids by name. */
  std::map<clap_id, size_t> offIds;
  size_t offReads = 0;
  for (int patch = 0; patch < 3; patch++)
  {
    Rig rig(spreads[patch]);
    rig.push({{kMorphOn, 1}, {kIntentFlag, 0}, {kMorphGlide, 0}, {kMorphTemp, 1},
              {kMorphSeed, (double)seeds[patch]}});
    Live live(rig.p);
    const std::vector<std::pair<double, double>> pos = positions();
    std::vector<double> prev(rig.f.n(), 0.0);
    for (size_t s = 0; s < rig.f.n(); s++) prev[s] = rig.live(rig.f.ids[s]);
    for (size_t q = 0; q < pos.size(); q++)
    {
      EvList ev;
      ev.push(kMorphX, pos[q].first);
      ev.push(kMorphY, pos[q].second);
      live.run(2, &ev);
      const std::map<clap_id, int> own = shippedOwners(rig.p);
      for (size_t s = 0; s < rig.f.n(); s++)
      {
        /* SYMMETRIC with the leg above, including the held slots: ownersjson
           reports -2 for a slot ADR-108 is holding, and the expectation for
           one of those is "unchanged", so a CHANGE is the mismatch. Skipping
           them would have made the comparison unfair in the resolver's
           disfavour — it is how `beatMult` first looked like the resolver's
           doing when it is the snap in applyParam, which both laws meet. */
        const std::map<clap_id, int>::const_iterator it = own.find(rig.f.ids[s]);
        const double v = rig.live(rig.f.ids[s]);
        if (it != own.end())
        {
          offReads++;
          const bool wrong = it->second >= 0
                                 ? std::fabs(v - rig.f.corner[it->second][s]) > 1e-9
                                 : true;
          if (wrong && v != prev[s]) offIds[rig.f.ids[s]]++;
        }
        prev[s] = v;
      }
    }
  }
  std::printf("       ids that read back something else, SHIPPED field: %s\n",
              idList(offIds).c_str());
  std::string extra;
  for (std::map<clap_id, size_t>::const_iterator it = onIds.begin(); it != onIds.end(); ++it)
    if (offIds.find(it->first) == offIds.end())
    {
      char b[16];
      std::snprintf(b, sizeof b, "%s%u", extra.empty() ? "" : " ", (unsigned)it->first);
      extra += b;
    }
  char msg[440];
  std::snprintf(msg, sizeof msg,
                "TC attribution: every id that reads back something else under the RESOLVER "
                "does so under the SHIPPED field too (%zu reads) — the resolver introduces "
                "none of its own (extras: %s)",
                offReads, extra.empty() ? "none" : extra.c_str());
  say(extra.empty(), msg);

  /* R15 — with the morph OFF the flag does nothing, because bindings live in
     corners and with no field there is no owner. Rendered, not reasoned: the
     seam's condition is `intentBusOn && morphOn`, and this measures that the
     second half of that `&&` is load-bearing. S3's plant is the control that
     says this comparison can fail at all. */
  auto renderAt = [](double flag) {
    const clap_plugin_t *p = makePlugin();
    Field f;
    f.read(p);
    authorCorners(p, f, 0.15);
    EvList ev;
    ev.push(kMorphOn, 0);
    ev.push(kIntentFlag, flag);
    ev.push(kMorphX, 0.37);
    ev.push(kMorphY, 0.61);
    paramsOf(p)->flush(p, &ev.list, &kOut);
    drain(p);
    std::vector<float> audio;
    render(p, audio);
    p->destroy(p);
    return audio;
  };
  const std::vector<float> off = renderAt(0);
  const std::vector<float> on = renderAt(1);
  std::snprintf(msg, sizeof msg,
                "TC R15: with the MORPH off, flag 1 renders bit-identically to flag 0 (%zu "
                "samples, rms %.4g — not silence, so the equality means something)",
                on.size(), rms(on));
  say(off == on && rms(on) > 1e-6, msg);
}

/* ---- T1: a locked range pins its parameter while that corner owns it ----- */
void t1()
{
  std::printf("\nT1 — a corner that locks a parameter (lo == hi) pins it while it owns it\n");
  const double lock = 0.42;   // normalised AND raw: fx1amt is a 0..1 control

  struct Leg { size_t owned = 0; double worstOff = 0; double maxMove = 0; };
  auto leg = [&](bool widen) {
    Leg r;
    Rig rig;
    std::string body = widen ? rangeTok(kFx1Amt, 0, 0.0, 1.0)
                             : rangeTok(kFx1Amt, 0, lock, lock);
    body += bindAllCorners(kFx1Amt, kM1, 0.5);
    rig.push({{kMorphOn, 1}, {kIntentFlag, 1}, {kMorphGlide, 0}, {kMorphTemp, 1},
              {kMorphSeed, 1024}});
    rig.intentChunk(body);
    rig.arm(1024);
    Live live(rig.p);
    const std::vector<std::pair<double, double>> pos = positions();
    for (size_t q = 0; q < pos.size(); q++)
    {
      double v[2] = {0, 0};
      int own = -1;
      for (int m = 0; m < 2; m++)
      {
        EvList ev;
        ev.push(kMorphX, pos[q].first);
        ev.push(kMorphY, pos[q].second);
        ev.push(kMacro1, m == 0 ? 0.0 : 1.0);
        live.run(2, &ev);
        own = rig.owner(kFx1Amt);
        v[m] = rig.live(kFx1Amt);
      }
      if (own != 0) continue;   // corner A is the one holding the lock
      r.owned++;
      for (int m = 0; m < 2; m++)
        r.worstOff = std::max(r.worstOff, std::fabs(v[m] - lock));
      r.maxMove = std::max(r.maxMove, std::fabs(v[1] - v[0]));
    }
    return r;
  };

  const Leg base = leg(false);
  const Leg ctl = leg(true);
  char msg[400];
  std::snprintf(msg, sizeof msg,
                "T1 corner A locks fx1amt at %.2f: over %zu positions A owned it, a full macro "
                "sweep moved it by at most %.3g and it never left the lock (worst %.3g)",
                lock, base.owned, base.maxMove, base.worstOff);
  say(base.owned > 0 && base.maxMove <= 1e-12 && base.worstOff <= 1e-12, msg);
  std::snprintf(msg, sizeof msg,
                "T1 CONTROL: with A's range widened to [0,1] the same sweep at the same "
                "positions MOVES it (by up to %.3g over %zu owned positions)",
                ctl.maxMove, ctl.owned);
  say(ctl.owned > 0 && ctl.maxMove > 1e-6, msg);
}

/* ---- T10: two corners, two ranges, one intent ---------------------------- */
void t10()
{
  std::printf("\nT10 — the same intent through two corners with different ranges\n");
  struct Leg { size_t nA = 0, nB = 0; double maxA = 0, maxB = 0; };
  auto leg = [&](bool swapRanges) {
    Leg r;
    Rig rig;
    const double shallowHi = 0.2;
    std::string body = rangeTok(kDecay, swapRanges ? 1 : 0, 0.0, shallowHi);
    body += rangeTok(kDecay, swapRanges ? 0 : 1, 0.0, 1.0);
    body += bindAllCorners(kDecay, kM1, 0.6);
    rig.push({{kMorphOn, 1}, {kIntentFlag, 1}, {kMorphGlide, 0}, {kMorphTemp, 1},
              {kMorphSeed, 7}});
    rig.intentChunk(body);
    rig.arm(7);
    Live live(rig.p);
    const std::vector<std::pair<double, double>> pos = positions();
    for (size_t q = 0; q < pos.size(); q++)
    {
      EvList ev;
      ev.push(kMorphX, pos[q].first);
      ev.push(kMorphY, pos[q].second);
      ev.push(kMacro1, 1.0);   // the intent pushed all the way up
      live.run(2, &ev);
      const int own = rig.owner(kDecay);
      const double v = rig.live(kDecay);
      if (own == 0) { r.nA++; r.maxA = std::max(r.maxA, v); }
      if (own == 1) { r.nB++; r.maxB = std::max(r.maxB, v); }
    }
    return r;
  };

  // The shallow corner's ceiling in RAW units: decay is 0.005..4 s, so a
  // normalised hi of 0.2 is 0.804 s. Read off the parameter, never retyped.
  double lo = 0, hi = 0;
  {
    const clap_plugin_t *probe = makePlugin();
    auto *params = paramsOf(probe);
    const uint32_t n = params->count(probe);
    for (uint32_t i = 0; i < n; i++)
    {
      clap_param_info_t info{};
      if (params->get_info(probe, i, &info) && info.id == kDecay)
      { lo = info.min_value; hi = info.max_value; }
    }
    probe->destroy(probe);
  }
  const double ceiling = lo + 0.2 * (hi - lo);

  const Leg base = leg(false);
  const Leg ctl = leg(true);
  char msg[420];
  std::snprintf(msg, sizeof msg,
                "T10 corner A's decay range is shallow [0,0.2]: with the intent at full, A "
                "stayed under %.4f s across %zu positions (max %.4f s) while B reached %.4f s "
                "across %zu",
                ceiling, base.nA, base.maxA, base.maxB, base.nB);
  say(base.nA > 0 && base.nB > 0 && base.maxA <= ceiling + 1e-9 && base.maxB > ceiling + 1e-3,
      msg);
  std::snprintf(msg, sizeof msg,
                "T10 CONTROL: swap the two ranges and the roles swap — A now reaches %.4f s "
                "and B is the one capped at %.4f s",
                ctl.maxA, ctl.maxB);
  say(ctl.nA > 0 && ctl.nB > 0 && ctl.maxB <= ceiling + 1e-9 && ctl.maxA > ceiling + 1e-3, msg);
}

/* ---- T4: one intent acting through two corners at once ------------------- */
void t4()
{
  std::printf("\nT4 — one intent, two corners, simultaneously (steepness 1 = temp 1)\n");
  const double depth[4] = {0.10, 0.18, 0.26, 0.34};

  struct Leg
  {
    int ownD = -1, ownE = -1;
    double moveD = 0, moveE = 0, wantD = 0, wantE = 0;
  };
  /* The prediction is computed from the CORNER VALUE the plugin reports, not
     from a second copy of §4.5: base + depth, clamped by the corner tier and
     then by clamp01, scaled back to raw. Anything else would be this oracle
     grading its own arithmetic. */
  auto predict = [](const Rig &rig, clap_id id, int corner, double d, double minV, double span) {
    const double baseN = (rig.f.corner[corner][(size_t)rig.f.slotOf(id)] - minV) / span;
    const double up = std::min(1.0, std::max(0.0, baseN + d));
    return (up - baseN) * span;
  };
  auto bounds = [](clap_id id, double &minV, double &span) {
    const clap_plugin_t *probe = makePlugin();
    auto *params = paramsOf(probe);
    const uint32_t n = params->count(probe);
    minV = 0;
    span = 1;
    for (uint32_t i = 0; i < n; i++)
    {
      clap_param_info_t info{};
      if (params->get_info(probe, i, &info) && info.id == id)
      { minV = info.min_value; span = info.max_value - info.min_value; }
    }
    probe->destroy(probe);
  };
  double dMin = 0, dSpan = 1, eMin = 0, eSpan = 1;
  bounds(kDetune, dMin, dSpan);
  bounds(kDecay, eMin, eSpan);

  auto leg = [&](double x, double y, double temp, uint32_t seed) {
    Leg r;
    Rig rig;
    std::string body;
    char tok[96];
    for (int k = 0; k < 4; k++)
    {
      std::snprintf(tok, sizeof tok, ",B:%u:%d:%d:%.17g", (unsigned)kDetune, k, kM1, depth[k]);
      body += tok;
      std::snprintf(tok, sizeof tok, ",B:%u:%d:%d:%.17g", (unsigned)kDecay, k, kM1, depth[k]);
      body += tok;
    }
    rig.push({{kMorphOn, 1}, {kIntentFlag, 1}, {kMorphGlide, 0}, {kMorphTemp, temp},
              {kMorphSeed, (double)seed}});
    rig.intentChunk(body);
    rig.arm(seed, temp);
    Live live(rig.p);
    double v0[2] = {0, 0}, v1[2] = {0, 0};
    for (int m = 0; m < 2; m++)
    {
      EvList ev;
      ev.push(kMorphX, x);
      ev.push(kMorphY, y);
      ev.push(kMacro1, m == 0 ? 0.0 : 1.0);
      live.run(2, &ev);
      (m == 0 ? v0 : v1)[0] = rig.live(kDetune);
      (m == 0 ? v0 : v1)[1] = rig.live(kDecay);
      r.ownD = rig.owner(kDetune);
      r.ownE = rig.owner(kDecay);
    }
    r.moveD = v1[0] - v0[0];
    r.moveE = v1[1] - v0[1];
    if (r.ownD >= 0) r.wantD = predict(rig, kDetune, r.ownD, depth[r.ownD], dMin, dSpan);
    if (r.ownE >= 0) r.wantE = predict(rig, kDecay, r.ownE, depth[r.ownE], eMin, eSpan);
    return r;
  };

  /* The acceptance's configuration: the EXACT centre at temp 1. Note that the
     centre is where the four weights are tied at EVERY steepness, so it cannot
     also host the control — the control moves a hair off centre (and the same
     off-centre position is measured at temp 1 too, so the collapse is
     attributable to the steepness and not to the move). */
  Leg mid;
  uint32_t used = 0;
  const uint32_t seeds[6] = {1024, 7, 4242, 99, 31337, 5};
  for (int i = 0; i < 6; i++)
  {
    mid = leg(0.5, 0.5, 1.0, seeds[i]);
    used = seeds[i];
    if (mid.ownD != mid.ownE) break;
  }
  char msg[440];
  std::snprintf(msg, sizeof msg,
                "T4 at (0.5,0.5), temp 1, seed %u: detune is owned by corner %d and decay by "
                "corner %d, and ONE macro moved both — detune by %.6g (corner %d's depth "
                "predicts %.6g) and decay by %.6g (corner %d's predicts %.6g)",
                used, mid.ownD, mid.ownE, mid.moveD, mid.ownD, mid.wantD, mid.moveE, mid.ownE,
                mid.wantE);
  say(mid.ownD != mid.ownE && std::fabs(mid.moveD - mid.wantD) <= 1e-9 &&
          std::fabs(mid.moveE - mid.wantE) <= 1e-9 && std::fabs(mid.moveD) > 1e-9 &&
          std::fabs(mid.moveE) > 1e-9,
      msg);

  const Leg off = leg(0.35, 0.45, 1.0, used);
  const Leg sharp = leg(0.35, 0.45, 0.02, used);
  std::snprintf(msg, sizeof msg,
                "T4 CONTROL: at (0.35,0.45) temp 1 still splits (corners %d/%d); at temp 0.02 "
                "(steepness 50) ownership COLLAPSES to corner %d for both, and the second "
                "corner's depth goes silent — detune moves %.6g against corner %d's %.6g",
                off.ownD, off.ownE, sharp.ownD, sharp.moveD, sharp.ownD, sharp.wantD);
  say(sharp.ownD == sharp.ownE && off.ownD != off.ownE &&
          std::fabs(sharp.moveD - sharp.wantD) <= 1e-9 &&
          std::fabs(sharp.moveE - sharp.wantE) <= 1e-9,
      msg);
}

/* ---- T5: structural requests, and the atom that must not split ----------- */
void t5()
{
  std::printf("\nT5 — four corners requesting a different `n`, and the scale as one atom\n");
  const double voices[4] = {3, 7, 12, 19};

  struct Leg
  {
    size_t positions = 0, outside = 0, distinct = 0, noCornerMatch = 0, split = 0;
  };
  auto leg = [&](bool breakAtom) {
    Leg r;
    Rig rig;
    for (int k = 0; k < 4; k++)
    {
      rig.setCorner(k, kVoices, voices[k]);
      // The scale is ADR-108-gated on bendQuant; without this the degrees are
      // HELD in every corner and the atom test would pass by never applying.
      rig.setCorner(k, kBendQuant, 2);
      rig.setCorner(k, kScaleRoot, (double)(k * 3));
      for (int deg = 0; deg < 12; deg++)
        rig.setCorner(k, (clap_id)(117 + deg), ((deg + k) % 3 == 0) ? 0 : 1);
      rig.writeCorner(k);
    }
    rig.f.read(rig.p);
    rig.arm(4242);
    if (breakAtom && !hypersaw_debug_intent_break_atom(rig.p, rig.slot((clap_id)118)))
      say(false, "break_atom refused");
    Live live(rig.p);
    const std::vector<std::pair<double, double>> pos = positions();
    bool seen[4] = {false, false, false, false};
    for (size_t q = 0; q < pos.size(); q++)
    {
      EvList ev;
      ev.push(kMorphX, pos[q].first);
      ev.push(kMorphY, pos[q].second);
      live.run(2, &ev);
      r.positions++;
      const double n = rig.live(kVoices);
      bool hit = false;
      for (int k = 0; k < 4; k++)
        if (std::fabs(n - voices[k]) < 1e-9) { hit = true; seen[k] = true; }
      if (!hit) r.outside++;
      // The 13-id scale, read LIVE and compared to the four stored vectors: a
      // chimera is a live scale that matches no single corner.
      bool matched = false;
      for (int k = 0; k < 4; k++)
      {
        bool all = true;
        for (int d = 0; d < 13; d++)
        {
          const clap_id id = (clap_id)(116 + d);
          if (std::fabs(rig.live(id) - rig.f.corner[k][(size_t)rig.slot(id)]) > 1e-9)
          { all = false; break; }
        }
        if (all) { matched = true; break; }
      }
      if (!matched) r.noCornerMatch++;
      const int k0 = rig.owner(kScaleRoot);
      for (int d = 1; d < 13; d++)
        if (rig.owner((clap_id)(116 + d)) != k0) { r.split++; break; }
    }
    for (int k = 0; k < 4; k++) r.distinct += seen[k] ? 1 : 0;
    return r;
  };

  const Leg base = leg(false);
  char msg[440];
  std::snprintf(msg, sizeof msg,
                "T5 `n` over %zu positions: %zu values outside the four requested {3,7,12,19} "
                "and %zu of the four actually visited — a structural request is taken in full, "
                "never blended",
                base.positions, base.outside, base.distinct);
  say(base.outside == 0 && base.distinct >= 2, msg);
  std::snprintf(msg, sizeof msg,
                "T5 the 13-id scale is ONE atom: %zu owner splits and %zu positions where the "
                "live scale matched no corner",
                base.split, base.noCornerMatch);
  say(base.split == 0 && base.noCornerMatch == 0, msg);

  const Leg ctl = leg(true);
  std::snprintf(msg, sizeof msg,
                "T5 CONTROL: with degree 2 broken off the lead map the chimera appears — %zu "
                "positions split the atom's owner and %zu produced a live scale NO corner "
                "authored",
                ctl.split, ctl.noCornerMatch);
  say(ctl.split > 0 && ctl.noCornerMatch > 0, msg);
}

/* ---- T6: commit ---------------------------------------------------------- */
void t6()
{
  std::printf("\nT6 — commit bakes the displacement and leaves the sound where it was\n");
  struct Leg
  {
    int dom = -2;
    double worst = 0, macroSum = 0, worstOffset = 0;
    size_t slots = 0;
  };
  /* At an EXACT corner every weight is one-hot, so that corner owns every atom
     and is the dominant one — which is what makes "the resolved output is
     unchanged" a total claim rather than one about the subset it owns. */
  auto leg = [&](int forceCorner) {
    Leg r;
    Rig rig;
    std::string body = bindAllCorners(kDetune, kM1, 0.2);
    body += bindAllCorners(kDecay, kM1, 0.2);
    body += bindAllCorners(kFx1Amt, kM1, 0.2);
    rig.push({{kMorphOn, 1}, {kIntentFlag, 1}, {kMorphGlide, 0}, {kMorphTemp, 1},
              {kMorphSeed, 1024}});
    rig.intentChunk(body);
    rig.arm(1024);
    Live live(rig.p);
    {
      EvList ev;
      ev.push(kMorphX, 1.0);
      ev.push(kMorphY, 1.0);
      ev.push(kMacro1, 0.7);
      live.run(2, &ev);
    }
    std::vector<double> before(rig.f.n(), 0.0);
    for (size_t s = 0; s < rig.f.n(); s++)
      before[s] = hypersaw_debug_intent_final(rig.p, (int)s);

    r.dom = hypersaw_debug_intent_commit(rig.p, forceCorner);
    live.run(2);
    for (size_t s = 0; s < rig.f.n(); s++)
    {
      r.worst = std::max(r.worst, std::fabs(hypersaw_debug_intent_final(rig.p, (int)s) - before[s]));
      r.slots++;
    }
    for (int i = 0; i < 8; i++) r.macroSum += std::fabs(rig.live((clap_id)(166 + i)));
    // "every offset reads zero" — measured as the resolved value having become
    // the corner's own stored base, which is what a baked offset means.
    rig.f.read(rig.p);
    const int own = rig.owner(kDetune);
    if (own >= 0)
    {
      const clap_id ids[3] = {kDetune, kDecay, kFx1Amt};
      for (int i = 0; i < 3; i++)
      {
        const int s = rig.slot(ids[i]);
        r.worstOffset = std::max(r.worstOffset, std::fabs(hypersaw_debug_intent_final(rig.p, s) -
                                                          rig.f.corner[own][(size_t)s]));
      }
    }
    return r;
  };

  const Leg base = leg(-1);
  char msg[440];
  std::snprintf(msg, sizeof msg,
                "T6 commit into the dominant corner (%d) at the (1,1) corner: %zu slots, worst "
                "|resolved after - before| = %.3g, the eight intent knobs now sum to %.3g, and "
                "the bound parameters sit exactly on their corner's base (worst %.3g)",
                base.dom, base.slots, base.worst, base.macroSum, base.worstOffset);
  say(base.dom == 3 && base.worst <= kEps && base.macroSum == 0.0 && base.worstOffset <= kEps,
      msg);

  const Leg ctl = leg(0);
  std::snprintf(msg, sizeof msg,
                "T6 CONTROL: commit into corner 0, which owns nothing at (1,1) — the "
                "displacement is not baked, the knobs are zeroed anyway, and the resolved "
                "output MOVES (worst %.3g)",
                ctl.worst);
  say(ctl.dom == 0 && ctl.worst > 1e-6, msg);
}

/* ---- T9: the owner map is a function of the seed ------------------------- */
void t9()
{
  std::printf("\nT9 — the owner map is a function of morphSeed (2026-09-10 amendment)\n");
  auto sweep = [](uint32_t seed, std::vector<int> &all, std::vector<int> &corners) {
    Rig rig;
    rig.arm(seed);
    Live live(rig.p);
    const std::vector<std::pair<double, double>> pos = positions();
    all.clear();
    corners.clear();
    for (size_t q = 0; q < pos.size(); q++)
    {
      EvList ev;
      ev.push(kMorphX, pos[q].first);
      ev.push(kMorphY, pos[q].second);
      live.run(2, &ev);
      for (size_t s = 0; s < rig.f.n(); s++)
      {
        const int o = hypersaw_debug_intent_owner(rig.p, (int)s);
        all.push_back(o);
        if (q < 4) corners.push_back(o);   // positions() opens with the four exact corners
      }
    }
  };
  std::vector<int> a1, a2, b1, ca1, ca2, cb1;
  sweep(1024, a1, ca1);
  sweep(1024, a2, ca2);
  sweep(4242, b1, cb1);
  size_t diffSame = 0, diffSeed = 0, diffCorners = 0;
  for (size_t i = 0; i < a1.size() && i < a2.size(); i++) diffSame += a1[i] != a2[i];
  for (size_t i = 0; i < a1.size() && i < b1.size(); i++) diffSeed += a1[i] != b1[i];
  for (size_t i = 0; i < ca1.size() && i < cb1.size(); i++) diffCorners += ca1[i] != cb1[i];
  char msg[400];
  std::snprintf(msg, sizeof msg,
                "T9 same seed, two instances: %zu owner disagreements over %zu reads",
                diffSame, a1.size());
  say(diffSame == 0 && !a1.empty(), msg);
  std::snprintf(msg, sizeof msg,
                "T9 CONTROL: a different seed disagrees on %zu of %zu reads — the map is the "
                "seed's, not the position's alone",
                diffSeed, a1.size());
  say(diffSeed > 0, msg);
  std::snprintf(msg, sizeof msg,
                "T9 the four EXACT corners are unchanged either way: %zu disagreements over "
                "%zu reads (one-hot weights leave the seed nothing to decide)",
                diffCorners, ca1.size());
  say(diffCorners == 0 && !ca1.empty(), msg);
}

/* ---- TE: the corner gestures, with the flag on --------------------------- */
void te()
{
  std::printf("\nTE — the corner gestures and the ADR-160 marks, with the resolver running\n");
  {
    Rig rig;
    rig.arm(1024);
    Live live(rig.p);
    EvList ev;
    ev.push(kMorphX, 0.37);
    ev.push(kMorphY, 0.61);
    live.run(2, &ev);
    // ARMED: the edit belongs to the armed corner and to no other.
    rig.push({{kMorphArm, 1}});
    rig.push({{kDetune, 0.123456}});
    Field g;
    g.read(rig.p);
    const int s = g.slotOf(kDetune);
    const bool onlyArmed = std::fabs(g.corner[0][(size_t)s] - 0.123456) < 1e-9 &&
                           std::fabs(g.corner[1][(size_t)s] - 0.123456) > 1e-9;
    say(onlyArmed, "TE1 an ARMED edit still writes exactly the armed corner's baseline");

    /* UNARMED, MEASURED over a sweep rather than at one position, because at
       one position the two laws agree by coincidence about a quarter of the
       time. morphRouteEdit routes an unarmed edit with morph.pickCorner — the
       SHIPPED Gumbel law — which under the flag is not necessarily the corner
       the resolver says owns the slot; where they disagree the edit lands in a
       corner that is not sounding and the next grid tick overwrites it. The
       assertion below is the mechanism (an unarmed edit still lands in SOME
       corner baseline, which is ADR-109's contract); the disagreement count is
       a phase-2c FINDING for the lead, not something this brief rules on. */
    rig.push({{kMorphArm, 0}});
    const std::vector<std::pair<double, double>> pos = positions();
    size_t edits = 0, landedSomewhere = 0, disagreed = 0;
    for (size_t q = 4; q < 28 && q < pos.size(); q++)
    {
      EvList e2;
      e2.push(kMorphX, pos[q].first);
      e2.push(kMorphY, pos[q].second);
      live.run(2, &e2);
      const int own = rig.owner(kDetune);
      const double v = 0.10 + 0.01 * (double)q;
      rig.push({{kDetune, v}});
      Field h;
      h.read(rig.p);
      int landed = -1;
      for (int k = 0; k < 4; k++)
        if (std::fabs(h.corner[k][(size_t)s] - v) < 1e-9) landed = k;
      edits++;
      if (landed >= 0) landedSomewhere++;
      if (landed != own) disagreed++;
    }
    std::printf("       FINDING: over %zu unarmed edits the corner morphRouteEdit chose "
                "disagreed with the RESOLVER's owner %zu times — morphRouteEdit still asks "
                "morph.pickCorner, so those edits are overwritten at the next tick (out of "
                "this brief's scope; ADR candidate)\n",
                edits, disagreed);
    say(edits > 0 && landedSomewhere == edits,
        "TE2 an UNARMED edit still lands in a corner baseline (WHICH corner is the finding "
        "above, not this assertion)");
  }
  {
    // CAPTURE still bakes what is sounding.
    Rig rig;
    rig.arm(7);
    Live live(rig.p);
    EvList ev;
    ev.push(kMorphX, 0.25);
    ev.push(kMorphY, 0.8);
    live.run(2, &ev);
    Field before;
    before.read(rig.p);
    const int s = before.slotOf(kDetune);
    const double liveV = rig.live(kDetune);
    /* Capture into a corner that does NOT already hold the live value —
       otherwise "the corner now holds it" is true before the gesture and the
       assertion measures nothing (L0033). The owning corner is exactly the one
       that would make it vacuous, so the target is chosen, not assumed. */
    int target = -1;
    for (int k = 0; k < 4 && target < 0; k++)
      if (std::fabs(before.corner[k][(size_t)s] - liveV) > 1e-9) target = k;
    const bool differed = target >= 0;
    if (target < 0) target = 2;
    hypersaw_debug_capture(rig.p, target);
    Field after;
    after.read(rig.p);
    char msg[320];
    std::snprintf(msg, sizeof msg,
                  "TE3 CAPTURE still bakes with the flag on: corner %d's detune was %.6g, the "
                  "live value %.6g, and after the capture the corner reads %.6g (anchor: the "
                  "two differed beforehand = %s)",
                  target, before.corner[target][(size_t)s], liveV,
                  after.corner[target][(size_t)s], differed ? "yes" : "NO");
    say(differed && std::fabs(after.corner[target][(size_t)s] - liveV) < 1e-9, msg);
  }
  {
    // EXEMPT still holds: the resolver must not write an exempt slot.
    Rig rig;
    rig.arm(1024);
    Live live(rig.p);
    auto sweepMove = [&]() {
      const std::vector<std::pair<double, double>> pos = positions();
      double lo = 1e30, hi = -1e30;
      for (size_t q = 0; q < 24; q++)
      {
        EvList ev;
        ev.push(kMorphX, pos[q].first);
        ev.push(kMorphY, pos[q].second);
        live.run(2, &ev);
        const double v = rig.live(kDetune);
        lo = std::min(lo, v);
        hi = std::max(hi, v);
      }
      return hi - lo;
    };
    const double moving = sweepMove();
    hypersaw_debug_exempt(rig.p, kDetune);
    const double exempt = sweepMove();
    Field g;
    g.read(rig.p);
    const int s = g.slotOf(kDetune);
    const bool allFour = std::fabs(g.corner[0][(size_t)s] - g.corner[3][(size_t)s]) < 1e-9;
    char msg[320];
    std::snprintf(msg, sizeof msg,
                  "TE4 EXEMPT still holds under the resolver: detune swung %.4g across the "
                  "sweep before the toggle and %.4g after, and all four corners took the live "
                  "value (%s)",
                  moving, exempt, allFour ? "yes" : "NO");
    say(moving > 1e-6 && exempt == 0.0 && allFour, msg);
  }
  {
    // ONE GESTURE, ONE NODE (ADR-160). undo_check owns the general law; this
    // asserts it is not disturbed by the flag.
    Rig rig;
    rig.arm(1024);
    Live live(rig.p);
    live.run(2);
    hypersaw_debug_undo(rig.p, "service", 0);
    const int base = std::atoi(hypersaw_debug_undo(rig.p, "size", 0));
    hypersaw_debug_capture(rig.p, 1);
    hypersaw_debug_undo(rig.p, "service", 0);
    const int one = std::atoi(hypersaw_debug_undo(rig.p, "size", 0));
    hypersaw_debug_undo(rig.p, "service", 0);
    const int still = std::atoi(hypersaw_debug_undo(rig.p, "size", 0));
    char msg[300];
    std::snprintf(msg, sizeof msg,
                  "TE5 one gesture is still one history node with the flag on: %d -> %d after a "
                  "capture, and a second service adds none (%d)",
                  base, one, still);
    say(base >= 1 && one == base + 1 && still == one, msg);
  }
}

/* ---- TG: the hazard — whose range clamps, and what a hold must not do ---- */
void tg()
{
  std::printf("\nTG — the corner clamp is the OWNER's range, and a held value is not "
              "re-clamped\n");
  /* fx1tone (96) is live only while fx1type (57) == 5, and the two share one
     atom (B49's FX group), so the dependency is always evaluated against the
     corner that also supplies the type — which is exactly ADR-108's rule. */
  const double narrowLo = 0.9, narrowHi = 1.0;

  struct Leg { double atB = 0, atA = 0; bool haveRange = false; double rlo = 0, rhi = 0; };
  auto leg = [&](bool depLiveInA) {
    Leg r;
    Rig rig;
    for (int k = 0; k < 4; k++)
    {
      rig.setCorner(k, kFx1Type, (k == 1 || (k == 0 && depLiveInA)) ? 5 : 0);
      rig.writeCorner(k);
    }
    rig.f.read(rig.p);
    rig.push({{kMorphOn, 1}, {kIntentFlag, 1}, {kMorphGlide, 0}, {kMorphTemp, 1},
              {kMorphSeed, 1024}});
    rig.intentChunk(rangeTok(kFx1Tone, 0, narrowLo, narrowHi));
    rig.arm(1024);
    r.haveRange = hypersaw_debug_intent_range(rig.p, 0, rig.slot(kFx1Tone), &r.rlo, &r.rhi);
    Live live(rig.p);
    {   // corner B (1,0): the tone is live there and B's range is the default
      EvList ev;
      ev.push(kMorphX, 1.0);
      ev.push(kMorphY, 0.0);
      live.run(2, &ev);
      r.atB = rig.live(kFx1Tone);
    }
    {   // corner A (0,0): A owns it, and A's range is the narrow one
      EvList ev;
      ev.push(kMorphX, 0.0);
      ev.push(kMorphY, 0.0);
      live.run(2, &ev);
      r.atA = rig.live(kFx1Tone);
    }
    return r;
  };

  const Leg held = leg(false);
  const Leg liveA = leg(true);
  char msg[460];
  std::snprintf(msg, sizeof msg,
                "TG1 the clamp is the OWNER's range: at corner B (range [0,1]) fx1tone reads "
                "%.6g; at corner A, whose range is [%.2f,%.2f] and where the dependency IS "
                "live, it reads %.6g — A's floor, not B's value",
                liveA.atB, liveA.rlo, liveA.rhi, liveA.atA);
  say(liveA.haveRange && liveA.rlo == narrowLo && std::fabs(liveA.atA - narrowLo) < 1e-9 &&
          std::fabs(liveA.atB - narrowLo) > 1e-3,
      msg);
  std::snprintf(msg, sizeof msg,
                "TG2 THE HAZARD: with the dependency FALSE in corner A, the ADR-108 hold keeps "
                "corner B's %.6g instead of being re-clamped into A's [%.2f,%.2f] — the wrong "
                "order would read %.2f, and the two answers differ by %.3g",
                held.atB, held.rlo, held.rhi, narrowLo, std::fabs(held.atB - narrowLo));
  say(std::fabs(held.atA - held.atB) < 1e-9 && std::fabs(held.atA - narrowLo) > 1e-3, msg);
}

/* ---- T8: the performance pad, through the plugin (B89 phase 2d) ----------
   The four t8-* GOLDENS above already hold IntentCore::padStep to the
   prototype's own trajectory at 1e-6 (release/return, latch, retarget, the
   stiff drag spring), so what is left for the instrument is everything the
   header cannot see: that the MAIN pad's two host parameters are the pointer,
   that the gesture BRACKET is what says "held", that `home` comes off the same
   walk as every other atom and flips with the field, and that the displacement
   reaches the engine through the bindings.

   THE PAD'S TWO HOST PARAMETERS are the macros ADR-150's assignment names —
   ids 179/180 hold the assignment, so by default the pointer is macro 1 (166,
   x) and macro 2 (167, y). The shell writes NEITHER: the puck is separate
   state, which is why a spring return cannot fight a host automating the
   parameter it reads. T8 therefore drives the pad by writing those two ids,
   bracketed by hypersaw_debug_gesture — the editor's own verb, not a
   test-only door.

   Y IS FLIPPED ONCE, in intentStep: a knob reads up (1 = top) and the pad's
   coordinate space is the canvas's (y down), which is the space §4.4 stores
   `home` in. So knob 0.45 is puck y 0.55, and every number below is written
   in the pad's space.

   THE SETTLING TIME IS DERIVED, never typed: the resting spring is
   k = 90 1/s^2, d = 11 1/s (IntentCore::Spring — read from the core here, so a
   constant that moved would move this budget with it). zeta = d/(2*sqrt(k))
   = 0.580 < 1, so the return is underdamped and its envelope is
   exp(-(d/2) t)/sqrt(1-zeta^2). Worst case the puck is a full pad width from
   home, so the intent starts at |X| = 2 before §4.4's +-1 clamp, and settling
   to 1e-3 needs t = ln(2/(1e-3*sqrt(1-zeta^2)))/(d/2). */
void t8()
{
  std::printf("\nT8 — the performance pad: the spring, the bracket, and the home that flips\n");
  const IntentCore::Spring sp;
  const double zeta = sp.dRest / (2.0 * std::sqrt(sp.kRest));
  const double settleS = std::log(2.0 / (1e-3 * std::sqrt(1.0 - zeta * zeta))) / (sp.dRest / 2.0);
  // The control rate is morphStep's grid, and one Live block IS one tick.
  const int settleTicks = (int)std::ceil(settleS * kSampleRate / (double)kBlock);
  // The dragging spring is stiffer (k=600, d=40); same derivation, to 1e-4.
  const double dragS = std::log(1.0 / (1e-4 * std::sqrt(1.0 - 0.8165 * 0.8165))) / (sp.dDrag / 2.0);
  const int dragTicks = (int)std::ceil(dragS * kSampleRate / (double)kBlock);
  std::printf("       derived from k=%.0f d=%.0f: zeta %.3f, settling to |X|<=1e-3 in %.3f s "
              "(%d ticks); the drag spring converges in %.3f s (%d ticks)\n",
              sp.kRest, sp.dRest, zeta, settleS, settleTicks, dragS, dragTicks);

  constexpr clap_id kPadX = 166, kPadY = 167;   // the default MAIN assignment
  const double kHome0X = 0.30, kHome0Y = 0.70;   // corner A's home, pad space
  const double kHome3X = 0.70, kHome3Y = 0.80;   // corner D's, for the retarget
  const double kBaseAmt = 0.40, kBaseDet = 0.30, kDepth = 0.5;
  // Dragged to pad (0.55, 0.55): X = (0.55-0.30)*2 = 0.5, Y = (0.70-0.55)*2 = 0.30.
  const double kDragPadX = 0.55, kDragPadY = 0.55;
  const double wantX = (kDragPadX - kHome0X) * 2.0, wantY = (kHome0Y - kDragPadY) * 2.0;

  /* One rig, parked on corner A. An EXACT corner is one-hot under both laws,
     so every atom — `home` among them — is corner A's, and nothing below
     depends on the seed. */
  auto build = [&](Rig &rig) {
    rig.setCorner(0, kFx1Amt, kBaseAmt);
    rig.setCorner(0, kDetune, kBaseDet);
    rig.writeCorner(0);
    rig.f.read(rig.p);
    rig.push({{kMorphOn, 1}, {kIntentFlag, 1}, {kMorphGlide, 0}, {kMorphTemp, 1},
              {kMorphSeed, 1024}, {kPadX, 0.5}, {kPadY, 0.5}});
    char tok[128];
    std::string body;
    std::snprintf(tok, sizeof tok, ",B:%u:0:0:%.17g", (unsigned)kFx1Amt, kDepth);
    body += tok;
    std::snprintf(tok, sizeof tok, ",B:%u:0:1:%.17g", (unsigned)kDetune, kDepth);
    body += tok;
    std::snprintf(tok, sizeof tok, ",H:0:%.17g:%.17g", kHome0X, kHome0Y);
    body += tok;
    std::snprintf(tok, sizeof tok, ",H:3:%.17g:%.17g", kHome3X, kHome3Y);
    body += tok;
    rig.intentChunk(body);
    rig.arm(1024);
  };
  // The pointer, written to the two host parameters the pad reads, inside a
  // gesture bracket — the shipped path a GUI drag takes, end to end.
  auto grab = [&](Rig &rig, Live &live, double padX, double padY, int ticks) {
    hypersaw_debug_gesture(rig.p, kPadX, true);
    hypersaw_debug_gesture(rig.p, kPadY, true);
    EvList ev;
    ev.push(kPadX, padX);
    ev.push(kPadY, 1.0 - padY);   // knob space -> pad space, once
    live.run(ticks, &ev);
  };
  auto release = [&](Rig &rig) {
    hypersaw_debug_gesture(rig.p, kPadX, false);
    hypersaw_debug_gesture(rig.p, kPadY, false);
  };
  auto puck = [&](const clap_plugin_t *p, double *x, double *y) {
    hypersaw_debug_intent_puck(p, x, y);
  };

  char msg[520];

  /* T8a — latch OFF. Drag, hold, release, and the puck returns to the home
     that is current; X and Y go to zero and the two bound parameters go back
     to their corner's base. The drag half is the anchor (L0033): a run where
     the puck never moved would satisfy "returns to home" by never leaving. */
  {
    Rig rig;
    build(rig);
    Live live(rig.p);
    live.run(settleTicks);                       // settle onto A's home first
    double rx = 0, ry = 0;
    puck(rig.p, &rx, &ry);
    const double restAmt = rig.live(kFx1Amt), restDet = rig.live(kDetune);

    grab(rig, live, kDragPadX, kDragPadY, dragTicks);
    double dx = 0, dy = 0;
    puck(rig.p, &dx, &dy);
    const double heldAmt = rig.live(kFx1Amt), heldDet = rig.live(kDetune);

    release(rig);
    live.run(settleTicks);
    double bx = 0, by = 0;
    puck(rig.p, &bx, &by);
    const double backAmt = rig.live(kFx1Amt), backDet = rig.live(kDetune);

    std::snprintf(msg, sizeof msg,
                  "T8a at rest the puck sits on corner A's home (%.4f, %.4f) vs the stored "
                  "(%.2f, %.2f), and the two bound parameters read their base (%.6g / %.6g "
                  "vs %.2f / %.2f)",
                  rx, ry, kHome0X, kHome0Y, restAmt, restDet, kBaseAmt, kBaseDet);
    say(std::fabs(rx - kHome0X) < 1e-3 && std::fabs(ry - kHome0Y) < 1e-3 &&
            std::fabs(restAmt - kBaseAmt) < 1e-3 && std::fabs(restDet - kBaseDet) < 1e-3,
        msg);

    std::snprintf(msg, sizeof msg,
                  "T8b HELD, the puck is at the pointer (%.4f, %.4f vs %.2f, %.2f) and §4.4's "
                  "mapping reaches the engine: X = %.4f -> fx1amt %.6g (predicted %.6g), "
                  "Y = %.4f -> detune %.6g (predicted %.6g)",
                  dx, dy, kDragPadX, kDragPadY, wantX, heldAmt, kBaseAmt + kDepth * wantX, wantY,
                  heldDet, kBaseDet + kDepth * wantY);
    say(std::fabs(dx - kDragPadX) < 1e-3 && std::fabs(dy - kDragPadY) < 1e-3 &&
            std::fabs(heldAmt - (kBaseAmt + kDepth * wantX)) < 2e-3 &&
            std::fabs(heldDet - (kBaseDet + kDepth * wantY)) < 2e-3,
        msg);

    std::snprintf(msg, sizeof msg,
                  "T8c RELEASED, within the derived settling time (%.3f s) the puck is back on "
                  "home (%.6f, %.6f), so X and Y are %.2e / %.2e and the parameters are back at "
                  "base (%.6g / %.6g); anchor: the drag had moved them by %.3g / %.3g",
                  settleS, bx, by, std::fabs((bx - kHome0X) * 2), std::fabs((kHome0Y - by) * 2),
                  backAmt, backDet, std::fabs(heldAmt - restAmt), std::fabs(heldDet - restDet));
    say(std::fabs((bx - kHome0X) * 2) <= 1e-3 && std::fabs((kHome0Y - by) * 2) <= 1e-3 &&
            std::fabs(backAmt - kBaseAmt) < 1e-3 && std::fabs(backDet - kBaseDet) < 1e-3 &&
            std::fabs(heldAmt - restAmt) > 0.1 && std::fabs(heldDet - restDet) > 0.05,
        msg);
  }

  /* T8d — THE HOME OWNER FLIPS MID-RETURN. The morph jumps from corner A to
     corner D while the puck is on its way home, so the TARGET jumps by 0.4 in
     x. The puck's own position must not: it is an integrator's state, and a
     retarget is a change of force, not of position. Sampled every tick, so a
     one-tick jump cannot hide between reads. */
  {
    Rig rig;
    build(rig);
    Live live(rig.p);
    live.run(settleTicks);
    grab(rig, live, kDragPadX, kDragPadY, dragTicks);
    release(rig);
    const int ownerBefore = hypersaw_debug_intent_homeowner(rig.p);

    double px = 0, py = 0, worstStep = 0, worstDrag = 0;
    puck(rig.p, &px, &py);
    // The drag phase's own biggest step, measured on the SAME rig, as the
    // scale a "continuous" claim is judged against rather than a round number.
    {
      double ax = px, ay = py;
      Rig scratch;
      build(scratch);
      Live sl(scratch.p);
      sl.run(settleTicks);
      hypersaw_debug_gesture(scratch.p, kPadX, true);
      hypersaw_debug_gesture(scratch.p, kPadY, true);
      EvList ev;
      ev.push(kPadX, kDragPadX);
      ev.push(kPadY, 1.0 - kDragPadY);
      puck(scratch.p, &ax, &ay);
      for (int t = 0; t < dragTicks; t++)
      {
        sl.run(1, t == 0 ? &ev : nullptr);
        double nx = 0, ny = 0;
        puck(scratch.p, &nx, &ny);
        const double s = std::max(std::fabs(nx - ax), std::fabs(ny - ay));
        if (s > worstDrag) worstDrag = s;
        ax = nx; ay = ny;
      }
    }

    const int switchTick = 12;
    for (int t = 0; t < settleTicks; t++)
    {
      if (t == switchTick)
      {
        EvList ev;
        ev.push(kMorphX, 1.0);
        ev.push(kMorphY, 1.0);
        live.run(1, &ev);
      }
      else live.run(1);
      double nx = 0, ny = 0;
      puck(rig.p, &nx, &ny);
      const double s = std::max(std::fabs(nx - px), std::fabs(ny - py));
      if (s > worstStep) worstStep = s;
      px = nx; py = ny;
    }
    const int ownerAfter = hypersaw_debug_intent_homeowner(rig.p);
    double hx = 0, hy = 0;
    hypersaw_debug_intent_home(rig.p, ownerAfter < 0 ? 0 : ownerAfter, &hx, &hy);

    std::snprintf(msg, sizeof msg,
                  "T8d the home owner flips mid-return (corner %d -> %d, home (%.2f,%.2f) -> "
                  "(%.2f,%.2f): the TARGET jumps 0.40 in x) and the puck's own position stays "
                  "continuous — worst single-tick step %.4g, against the drag phase's own "
                  "%.4g — ending on the NEW home (%.6f, %.6f)",
                  ownerBefore, ownerAfter, kHome0X, kHome0Y, hx, hy, worstStep, worstDrag, px, py);
    say(ownerBefore == 0 && ownerAfter == 3 && worstStep <= worstDrag &&
            std::fabs(px - kHome3X) < 1e-3 && std::fabs(py - kHome3Y) < 1e-3,
        msg);
  }

  /* T8e — THE MUST-FAIL CONTROL. Latch on, the same drag and the same release:
     the puck must NOT return. Without it T8c's "settles to zero" is equally
     consistent with a puck that never left home in the first place. */
  {
    Rig rig;
    build(rig);
    rig.push({{(clap_id)268, 1.0}});
    Live live(rig.p);
    live.run(settleTicks);
    grab(rig, live, kDragPadX, kDragPadY, dragTicks);
    release(rig);
    live.run(settleTicks);
    double lx = 0, ly = 0;
    puck(rig.p, &lx, &ly);
    const double heldAmt = rig.live(kFx1Amt);
    std::snprintf(msg, sizeof msg,
                  "T8e CONTROL: with the latch (id 268) ON the released puck does NOT return — "
                  "it sits at (%.6f, %.6f), %.3g from corner A's home, so X is still %.4f and "
                  "fx1amt still reads %.6g rather than its base %.2f",
                  lx, ly, std::fabs(lx - kHome0X), (lx - kHome0X) * 2, heldAmt, kBaseAmt);
    say(std::fabs(lx - kDragPadX) < 1e-6 && std::fabs(ly - kDragPadY) < 1e-6 &&
            std::fabs(lx - kHome0X) > 0.1 && std::fabs(heldAmt - kBaseAmt) > 0.1,
        msg);
  }
}

/* ---- TS: ADR-152's macro suspension, pinned under the flag ---------------
   Acceptance (e) of the 2d brief: a macro drives intents OR routes, never
   both. The suspension is ADR-152's and predates the bus (modStep multiplies
   the macro family by zero while the morph is on), but 2d is the increment
   that gives a macro a SECOND job, so "exactly 0" stops being a fact about
   one feature and becomes the boundary between two. Pinned here, with the
   route proven live in the other half of the same rig — a route that was
   never wired would report "contributes 0" just as loudly (L0032).

   MEASURED ON THE RENDER, not on get_value. A routed destination's readback is
   the modulation BASE by construction (ADR-136's intercept: readParam returns
   `md->base` for any id the matrix owns), so get_value cannot see a route's
   contribution at all — it reported "no movement" for a route that was
   demonstrably in the table. Rendering asks the only question that matters
   anyway: does moving this macro change what comes out. */
void ts()
{
  std::printf("\nTS — a macro route contributes exactly 0 while the flag and the morph are on\n");
  /* THE CORNERS ARE LEFT AT THEIR DEFAULTS, which is why this does not use
     Rig. `authorCorners` sets every continuous slot to 0.30..0.75 of its
     range, and for `attack` (0.001..2 s) that is 0.60..1.50 s — longer than
     render()'s ~1 s window, so a morph-ON render of an authored patch has an
     amplitude envelope that never opens and measures rms EXACTLY 0. This
     test's own anchor (rms > 1e-4) caught it. Unauthored corners hold the
     per-slot defaults (morphInit), so the field applies the shipped default
     patch — audible, and the only thing that differs between the two renders
     compared below is macro 1.

     The destination is DETUNE and the morph sits at (0.37, 0.61): both are
     S3's choices, made there for the same reason — detune is continuous, in
     the field, and audibly changes the swarm. An FX amount would have been the
     tidier subject and is the trap: fx1type ships at 0, so a route to fx1amt
     moves a number nothing reads, and the CONTROL reported "no movement" for a
     route that was demonstrably in the table. */
  auto leg = [&](bool morphOn, double macro, std::vector<float> &out) {
    const clap_plugin_t *p = makePlugin();
    auto push = [&](const std::vector<std::pair<clap_id, double>> &kv) {
      EvList ev;
      for (size_t i = 0; i < kv.size(); i++) ev.push(kv[i].first, kv[i].second);
      paramsOf(p)->flush(p, &ev.list, &kOut);
      drain(p);
    };
    push({{kMorphOn, morphOn ? 1.0 : 0.0}, {kIntentFlag, 1}, {kMorphGlide, 0},
          {kMorphTemp, 1}, {kMorphSeed, 1024}, {kMorphX, 0.37}, {kMorphY, 0.61},
          {kMacro1, macro}});
    // A generic route macro 1 (source slot 2) -> detune at full depth, authored
    // through the shipped `modroutes=` chunk — the only author of a route this
    // oracle has, and the same one a saved patch uses.
    char tok[64];
    std::snprintf(tok, sizeof tok, "2:%u:1;", (unsigned)kDetune);
    std::string blob = saveChunk(p);
    blob += std::string("modroutes=") + tok + "\n";
    if (!loadChunk(p, blob)) say(false, "modroutes chunk load refused");
    push({{kMorphOn, morphOn ? 1.0 : 0.0}, {kIntentFlag, 1}, {kMacro1, macro}});
    render(p, out);
    p->destroy(p);
  };
  std::vector<float> onZero, onFull, offZero, offFull;
  leg(true, 0.0, onZero);
  leg(true, 1.0, onFull);
  leg(false, 0.0, offZero);
  leg(false, 1.0, offFull);
  auto worst = [](const std::vector<float> &a, const std::vector<float> &b) {
    double w = 0;
    const size_t n = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; i++)
    {
      const double d = std::fabs((double)a[i] - (double)b[i]);
      if (d > w) w = d;
    }
    return w;
  };
  char msg[460];
  std::snprintf(msg, sizeof msg,
                "TS1 morph ON + flag ON: a full-depth macro-1 route to detune, with macro 1 at 0 "
                "and at 1, renders BIT-IDENTICALLY (worst sample diff %.3g over %zu samples; rms "
                "%.5g, so the equality is not silence) — the route contributes exactly 0",
                worst(onZero, onFull), onZero.size(), rms(onZero));
  say(worst(onZero, onFull) == 0.0 && rms(onZero) > 1e-4, msg);
  std::snprintf(msg, sizeof msg,
                "TS2 CONTROL: the same route with the morph OFF changes the render (worst sample "
                "diff %.4g, rms %.5g vs %.5g) — TS1 is ADR-152's suspension, not an unwired route",
                worst(offZero, offFull), rms(offZero), rms(offFull));
  say(worst(offZero, offFull) > 1e-4, msg);
}

void section()
{
  std::printf("\n========= SECTION T — SPEC-INTENT-BUS §12, through the plugin =========\n");
  tc();
  t1();
  t4();
  t5();
  t6();
  t8();
  t9();
  t10();
  te();
  tg();
  ts();
}

}   // namespace spec

void section()
{
  std::printf("\n=============== SECTION S — the shell seam (B89 phase 2b) ==============\n");
  chunkSection();
  plantSection();
  identitySection();
  spec::section();
}

}   // namespace shell

/* ------------------------------------------------------------------- main */
int main(int argc, char **argv)
{
  const std::string dir = argc > 1 ? argv[1] : "build-golden/intent";
  /* A missing manifest is still RED — an unasserted parity half is a promise,
     not evidence — but it no longer RETURNS, because section S asserts things
     the goldens have nothing to do with and skipping them would make one
     missing file silently shrink the oracle. */
  Fixture man;
  const bool haveGoldens = man.load((dir + "/intent-manifest.tsv").c_str());
  if (!haveGoldens)
  {
    std::printf("FAIL no manifest at %s/intent-manifest.tsv\n", dir.c_str());
    std::printf("  run: node tools/golden/gen_intent_goldens.mjs\n");
    failures++;
  }

  std::printf("intent_check — parity vs reference/intent-bus.html (tol %.0e absolute)\n", kTol);
  int cases = 0;
  size_t pos = 0;
  while (pos < man.text.size())
  {
    const size_t nl = man.text.find('\n', pos);
    const std::string line = man.text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
    pos = nl == std::string::npos ? man.text.size() : nl + 1;
    const size_t tab = line.find('\t');
    if (tab == std::string::npos) continue;
    const std::string name = line.substr(0, tab);

    Fixture fx;
    if (!fx.load((dir + "/" + name + ".json").c_str()))
    {
      std::printf("  FAIL %-22s fixture missing\n", name.c_str());
      failures++;
      continue;
    }
    cases++;

    Inputs b, c;
    b.read(fx, "b.");
    c.read(fx, "c.");
    const Diff db = compare(drive(b), fx);
    const Diff dc = compare(drive(c), fx);

    // Base must MATCH; the control must MISMATCH. Both halves, every case.
    const bool ok = !db.any() && dc.any();
    std::printf("  %-4s %-22s %s\n", ok ? "OK" : "FAIL", name.c_str(), fx.str("proves").c_str());
    if (db.any())
      std::printf("       base MISMATCH on %s[%d]: got %.17g want %.17g\n",
                  db.field, db.index, db.got, db.want);
    if (!dc.any())
      std::printf("       CONTROL DID NOT FIRE — \"%s\" reproduced the base expectations,\n"
                  "       so this case cannot tell agreement from coincidence\n",
                  fx.str("control").c_str());
    else
      std::printf("       control fires on %s[%d] (%s)\n", dc.field, dc.index,
                  fx.str("control").c_str());
    if (!ok) failures++;
  }

  invariants();
  shell::section();

  std::printf("\n%d fixtures, %d failure(s)\n", cases, failures);
  if (haveGoldens && cases < 24)
  {
    std::printf("FAIL: fewer than 24 fixtures — regenerate with gen_intent_goldens.mjs\n");
    failures++;
  }
  return failures == 0 ? 0 : 1;
}
