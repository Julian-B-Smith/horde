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
 * Standalone and UNWIRED: ./verify does not run this (adding a gate is the
 * human's decision, charter §Oracle discipline; ADR-171 is the wiring route).
 * It links nothing but src/intent_core.h.
 *
 * Usage: intent_check [build-golden/intent]
 */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../src/intent_core.h"

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

/* ------------------------------------------------------------------- main */
int main(int argc, char **argv)
{
  const std::string dir = argc > 1 ? argv[1] : "build-golden/intent";
  Fixture man;
  if (!man.load((dir + "/intent-manifest.tsv").c_str()))
  {
    std::printf("intent_check: no manifest at %s/intent-manifest.tsv\n", dir.c_str());
    std::printf("  run: node tools/golden/gen_intent_goldens.mjs\n");
    return 1;
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

  std::printf("\n%d fixtures, %d failure(s)\n", cases, failures);
  if (cases < 24)
  {
    std::printf("FAIL: fewer than 24 fixtures — regenerate with gen_intent_goldens.mjs\n");
    failures++;
  }
  return failures == 0 ? 0 : 1;
}
