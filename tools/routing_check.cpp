/* routing_check — the B23 crosspoint matrix (ADR-088) before it enters the
 * audio path. Same order the glide and swarmalator ports used: core + oracle
 * first, shell integration onto proven ground second.
 *
 * These are INVARIANT assertions, not parity against the routing lab. Deliberate:
 * the lab's scheme C carries toy effects (one-pole, delay, tanh), so a
 * sample-parity test would mostly measure those and would go red for reasons
 * that are not about routing. Every slot here is a trivial deterministic
 * stand-in, so what is measured is the topology and nothing else — which is
 * also what makes these transferable (L0030/L0031: an oracle that names no
 * internals outlives the implementation it was written against).
 */
#include <cmath>
#include <cstdio>
#include <vector>

#include <cstring>
#include "../src/routing_core.h"
#include "../src/fx_rack.h"
#include "../src/hypersaw_clap_entry.h"

/* B50 phase 1 — the shell's window onto the LIVE matrix. Declared here
   rather than in the entry header because these are debug exports, not part
   of the shipped surface, and the entry header is the shipped surface. */
extern "C" double hypersaw_debug_routing(const clap_plugin_t *, int from, int to);
extern "C" bool hypersaw_debug_routing_on(const clap_plugin_t *, int from, int to);
extern "C" double hypersaw_debug_routing_out(const clap_plugin_t *, int to);
extern "C" double hypersaw_debug_routing_init(const clap_plugin_t *, int to);
extern "C" const char *hypersaw_debug_routing_ids(void);

constexpr double kPi = 3.141592653589793;

static int failures = 0;
static void check(bool ok, const char *what, const char *detail)
{
  std::printf("%-6s %s  (%s)\n", ok ? "OK" : "FAIL", what, detail);
  if (!ok) failures++;
}

using Matrix = hypersaw::RoutingMatrix<2, 4>;

// Deterministic stand-in slots: distinguishable, order-sensitive, no state.
// Slot 1 is NONLINEAR on purpose — a linear chain would make serial and
// parallel indistinguishable, and the test would pass for the wrong reason.
static double slotProc(int slot, double x)
{
  switch (slot)
  {
    case 0: return x * 2.0;
    case 1: return x * x;          // nonlinear: order matters
    case 2: return x + 1.0;
    default: return x * 0.5;
  }
}

/* ---- B50 phase 1: the SHELL rig (assertions 8-12) -------------------------
   Assertions 1-7 above measure routing_core.h with a trivial stand-in slot, and
   that is deliberately all they can see. None of them can tell whether the
   shell ever reaches the core — the matrix ran in the audio path for three
   weeks with no parameter pointing at it and every one of them green (L0031:
   an oracle certifies agreement over the surface it spans, and this one spanned
   the core only). These five drive the SHIPPED parameter surface instead:
   parameters arrive as a host delivers them, and the read side is the live
   matrix, never a readback. */
static const clap_host_t kHost = {CLAP_VERSION_INIT, nullptr, "routing_check", "-", "-", "1.0",
                                  [](const clap_host_t *, const char *) -> const void * { return nullptr; },
                                  [](const clap_host_t *) {}, [](const clap_host_t *) {},
                                  [](const clap_host_t *) {}};
struct EvList
{
  clap_input_events_t in{};
  std::vector<const clap_event_header_t *> ev;
};
static uint32_t evSize(const clap_input_events_t *l) { return (uint32_t)((EvList *)l->ctx)->ev.size(); }
static const clap_event_header_t *evGet(const clap_input_events_t *l, uint32_t i)
{
  return ((EvList *)l->ctx)->ev[i];
}
static bool outPush(const clap_output_events_t *, const clap_event_header_t *) { return true; }

struct Cell { unsigned id; int kind, from, to; };

/* The cell list comes FROM the shell (hypersaw_debug_routing_ids). Re-deriving
   the id layout here would be a second copy of decodeRoutingId, and a probe
   that restates the implementation cannot disagree with it (L0032). */
static std::vector<Cell> parseCells(const char *s)
{
  std::vector<Cell> out;
  while (s && *s)
  {
    Cell c{};
    if (std::sscanf(s, "%u,%d,%d,%d;", &c.id, &c.kind, &c.from, &c.to) != 4) break;
    out.push_back(c);
    const char *semi = std::strchr(s, ';');
    if (!semi) break;
    s = semi + 1;
  }
  return out;
}

struct Rig
{
  const clap_plugin_t *p = nullptr;
  const clap_plugin_params_t *par = nullptr;
  std::vector<float> L, R;
  float *chans[2]{};
  clap_audio_buffer_t out{};
  clap_output_events_t outEv{nullptr, outPush};
  // The event list holds RAW POINTERS into `store`, so a reallocation dangles
  // them mid-flush. Reserved once, generously — the scar paramscope_check
  // records (a 64-slot reservation silently overflowed and produced a measured
  // number that was simply wrong).
  std::vector<clap_event_param_value_t> store;
  EvList evl;
  static constexpr int kBlk = 512;

  explicit Rig(const clap_plugin_factory_t *f)
  {
    p = f->create_plugin(f, &kHost, "com.lifted-truck.hypersaw");
    p->init(p);
    p->activate(p, 44100.0, 32, kBlk);
    p->start_processing(p);
    par = (const clap_plugin_params_t *)p->get_extension(p, CLAP_EXT_PARAMS);
    L.assign(kBlk, 0.0f);
    R.assign(kBlk, 0.0f);
    chans[0] = L.data(); chans[1] = R.data();
    out.data32 = chans; out.channel_count = 2;
    store.reserve(4096);
    evl.in.ctx = &evl; evl.in.size = evSize; evl.in.get = evGet;
  }
  ~Rig() { p->stop_processing(p); p->deactivate(p); p->destroy(p); }

  void queue(clap_id id, double v)
  {
    clap_event_param_value_t pv{};
    pv.header.size = sizeof(pv); pv.header.type = CLAP_EVENT_PARAM_VALUE;
    pv.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    pv.note_id = -1; pv.port_index = -1; pv.channel = -1; pv.key = -1;
    pv.param_id = id; pv.value = v;
    store.push_back(pv);
    evl.ev.push_back(&store.back().header);
  }
  // A host write, through the host's own door.
  void set(clap_id id, double v) { queue(id, v); flush(); }
  void flush() { par->flush(p, &evl.in, &outEv); evl.ev.clear(); }
  // Renders blocks so the morph field actually STEPS (morphStep runs inside
  // process(), on the gravity grid — flush alone never moves it).
  void render(int blocks)
  {
    clap_process_t pr{};
    pr.frames_count = kBlk;
    pr.audio_outputs = &out; pr.audio_outputs_count = 1;
    pr.in_events = &evl.in; pr.out_events = &outEv;
    for (int b = 0; b < blocks; b++) { p->process(p, &pr); evl.ev.clear(); }
  }
  bool info(clap_id id, clap_param_info_t &out2) const
  {
    const uint32_t n = par->count(p);
    for (uint32_t i = 0; i < n; i++)
      if (par->get_info(p, i, &out2) && out2.id == id) return true;
    return false;
  }
};

/* ---- string-backed CLAP streams, so a state round-trip needs no file ---- */
struct OStr { clap_ostream_t s; std::string data; };
static int64_t ostrWrite(const clap_ostream_t *s, const void *b, uint64_t n)
{
  ((OStr *)s)->data.append((const char *)b, (size_t)n);
  return (int64_t)n;
}
struct IStr { clap_istream_t s; std::string data; size_t pos = 0; };
static int64_t istrRead(const clap_istream_t *s, void *b, uint64_t n)
{
  auto *i = (IStr *)s;
  const size_t take = n < i->data.size() - i->pos ? (size_t)n : i->data.size() - i->pos;
  std::memcpy(b, i->data.data() + i->pos, take);
  i->pos += take;
  return (int64_t)take;
}

// The whole matrix, read off the live doubles — the comparison B50 (b) needs.
static bool matrixEqual(const clap_plugin_t *a, const clap_plugin_t *b,
                        const std::vector<Cell> &cells)
{
  for (const Cell &c : cells)
  {
    const double x = c.kind == 0 ? hypersaw_debug_routing(a, c.from, c.to)
                   : c.kind == 1 ? hypersaw_debug_routing_out(a, c.to)
                                 : hypersaw_debug_routing_init(a, c.to);
    const double y = c.kind == 0 ? hypersaw_debug_routing(b, c.from, c.to)
                   : c.kind == 1 ? hypersaw_debug_routing_out(b, c.to)
                                 : hypersaw_debug_routing_init(b, c.to);
    if (x != y) return false;
    if (c.kind == 0 && hypersaw_debug_routing_on(a, c.from, c.to)
                           != hypersaw_debug_routing_on(b, c.from, c.to))
      return false;   // the presence bit is part of the topology, not a detail
  }
  return true;
}

int main()
{
  char d[192];

  // ---- 1. the default topology IS a serial chain ---------------------------
  // Computed independently rather than by running the matrix, or the test would
  // be the implementation restated.
  {
    Matrix m;
    m.setSerialChain();
    const double src[2] = {0.3, 0.0};
    const double got = m.process(src, slotProc);
    const double want = slotProc(3, slotProc(2, slotProc(1, slotProc(0, 0.3))));
    std::snprintf(d, sizeof(d), "got %.12g, hand-computed chain %.12g", got, want);
    check(std::fabs(got - want) < 1e-12, "default topology is exactly a serial chain", d);
  }

  // ---- 2. sources sum at a slot -------------------------------------------
  {
    Matrix m;
    m.setSerialChain();
    const double a[2] = {0.3, 0.0}, b[2] = {0.0, 0.4}, both[2] = {0.3, 0.4};
    // slot 0 is linear (x*2), so its input sum is observable through the chain
    // only if the whole chain were linear — it is not. Assert at the slot input
    // instead, via a matrix whose only terminal is slot 0.
    Matrix m0;
    m0.setSerialChain();
    m0.inFrom[1] = 0; m0.outAmount[3] = 0; m0.outAmount[0] = 1.0;
    const double ga = m0.process(a, slotProc), gb = m0.process(b, slotProc),
                 gboth = m0.process(both, slotProc);
    std::snprintf(d, sizeof(d), "f(a)=%.6g f(b)=%.6g f(a+b)=%.6g", ga, gb, gboth);
    check(std::fabs(gboth - (ga + gb)) < 1e-12, "both sources sum into one slot", d);
  }

  // ---- 3. a backwards edge is LEGAL and carries ONE SAMPLE of delay --------
  // REPLACED 2026-08-27. The invariant here used to be "an illegal backwards
  // edge changes nothing", which encoded the forward-only contract. ADR-128
  // (human ruling) widened that contract: cycles are legal and a backwards edge
  // reads the previous SAMPLE. The old assertion is not weakened, it is
  // obsolete — it tested a rule that no longer exists — and these three take
  // its place, pinning the rule that replaced it. Edges are still set directly
  // on the model, which is the route a preset load, morph corner or automation
  // takes; no editor is involved and none can be relied on.
  {
    Matrix clean;
    clean.setSerialChain();
    const double src[2] = {0.3, 0.2};

    // (a) INERTNESS. With no backwards edge the engine must be byte-identical
    // to the forward-only one, sample after sample — this is what keeps every
    // golden green, and it is the property the whole change rests on.
    Matrix fwd = clean;
    bool driftedFwd = false;
    double firstFwd = 0;
    for (int i = 0; i < 64; i++)
    {
      const double y = fwd.process(src, slotProc);
      if (i == 0) firstFwd = y;
      else if (y != firstFwd) driftedFwd = true;
    }
    std::snprintf(d, sizeof(d), "64 samples, all %.12g, zPrev never read", firstFwd);
    check(!driftedFwd, "no backwards edge: the engine is stationary and unchanged", d);

    // (b) A backwards edge is now legal and DOES change the output — but not on
    // the first sample, because it reads a zPrev that is still zero. That
    // one-sample lag IS the ruling, so it is what gets asserted.
    Matrix fb = clean;
    fb.inFrom[0] |= (1u << (2 + 3));         // slot 3 -> slot 0, backwards
    fb.coeff[2 + 3][0] = 0.5;
    const double s0 = fb.process(src, slotProc);
    const double s1 = fb.process(src, slotProc);
    std::snprintf(d, sizeof(d),
                  "sample0 %.12g == forward-only %.12g; sample1 %.12g differs",
                  s0, firstFwd, s1);
    check(s0 == firstFwd && s1 != s0,
          "a backwards edge is inert for exactly one sample, then live", d);

    // (c) A SELF edge is the same rule at its tightest: slot 1 reading slot 1
    // must be delayed, not an infinite regress within one pass.
    Matrix self;
    self.setSerialChain();
    self.inFrom[1] |= (1u << (2 + 1));       // slot 1 -> slot 1
    self.coeff[2 + 1][1] = 0.5;
    const double q0 = self.process(src, slotProc);
    const double q1 = self.process(src, slotProc);
    std::snprintf(d, sizeof(d), "self-edge sample0 %.12g, sample1 %.12g", q0, q1);
    check(q0 == firstFwd && q1 != q0, "a self edge is delayed, not a regress", d);

    // and the control that the old block also carried: a FORWARD edge in the
    // same shape must change the result immediately, or the delay logic is
    // simply eating everything.
    Matrix legal = clean;
    legal.inFrom[3] |= (1u << (2 + 0));      // slot 0 -> slot 3, forwards
    legal.coeff[2 + 0][3] = 1.0;
    const double legalOut = legal.process(src, slotProc);
    std::snprintf(d, sizeof(d), "legal slot0->slot3 gives %.12g vs %.12g", legalOut, firstFwd);
    check(legalOut != firstFwd, "a forward edge in the same shape changes it at once", d);
  }

  // ---- 4. terminal detection follows the edges -----------------------------
  {
    Matrix m;
    m.setSerialChain();
    bool ok = m.isTerminal(3) && !m.isTerminal(0) && !m.isTerminal(1) && !m.isTerminal(2);
    // now cut slot 3's input: slot 2 becomes a terminal too
    m.inFrom[3] = 0;
    const bool bothTerminal = m.isTerminal(2) && m.isTerminal(3);
    std::snprintf(d, sizeof(d), "chain: only slot3 terminal = %s; after cutting slot3's input, "
                                "slots 2 and 3 both terminal = %s",
                  ok ? "yes" : "no", bothTerminal ? "yes" : "no");
    check(ok && bothTerminal, "a slot nobody reads is an output", d);
  }

  // ---- 5. serial and parallel genuinely differ ----------------------------
  // If they did not, the matrix would be expressing one topology with two
  // spellings and every assertion above would be vacuous.
  {
    Matrix ser;
    ser.setSerialChain();
    ser.inFrom[2] = 0; ser.inFrom[3] = 0;
    ser.outAmount[0] = 0; ser.outAmount[1] = 1.0; ser.outAmount[3] = 0;   // src->0->1->out

    Matrix par = ser;
    par.inFrom[1] = 0;                                  // slot 1 reads the SOURCE instead
    par.inFrom[1] |= 1u; par.coeff[0][1] = 1.0;
    par.outAmount[0] = 1.0; par.outAmount[1] = 1.0;     // both terminals

    const double src[2] = {0.3, 0.0};
    const double s = ser.process(src, slotProc), p = par.process(src, slotProc);
    std::snprintf(d, sizeof(d), "serial %.6g vs parallel %.6g", s, p);
    check(std::fabs(s - p) > 1e-9, "serial and parallel are not the same topology", d);
  }

  // ---- 6. the initial value is an independent input ------------------------
  {
    Matrix m;
    m.setSerialChain();
    const double src[2] = {0.0, 0.0};
    const double silent = m.process(src, slotProc);
    m.slotInit[0] = 0.25;
    const double offset = m.process(src, slotProc);
    std::snprintf(d, sizeof(d), "silent sources give %.6g; slotInit 0.25 gives %.6g", silent, offset);
    check(silent != offset, "slotInit drives a slot with no source connected", d);
  }

  // ---- 7. the block pass over the REAL rack is bit-exactly the old chain ----
  /* This section deliberately breaks the FX-agnostic rule above, and the reason
     is worth stating: the claim under test is not "routing works", it is "the
     matrix replaced `rack.processStereo` WITHOUT CHANGING A SAMPLE", and that
     claim is about the real rack or it is about nothing.

     It needs its own assertion because the 147 parity goldens CANNOT see this.
     They render SwarmCore directly; the plugin mix stage — bass-mono, the rack,
     master volume — is downstream of everything they cover. Treating a green
     parity run as evidence for a mix-stage refactor would be assuming exactly
     the coverage that does not exist (L0031: parity certifies agreement with
     the reference, over the surface the reference actually spans). */
  {
    hypersaw::RoutingMatrix<1, hypersaw::kRackSlots> m;   // ctor = serial chain
    hypersaw::FxRack direct, viaMatrix;

    // Every slot ACTIVE and distinct — an all-Off rack would make both paths
    // trivially equal by touching no samples, and the test would pass without
    // the chain ever being exercised.
    const int types[hypersaw::kRackSlots] = {1, 2, 4, 5};
    for (int i = 0; i < hypersaw::kRackSlots; i++)
    {
      direct.setType(i, types[i]);      viaMatrix.setType(i, types[i]);
      direct.setAmount(i, 0.4 + 0.1 * i); viaMatrix.setAmount(i, 0.4 + 0.1 * i);
    }

    const int N = 1024;
    std::vector<float> aL(N), aR(N), bL(N), bR(N);
    for (int i = 0; i < N; i++)
    {
      const double t = (double)i / 44100.0;
      // kPi, not M_PI: M_PI is a POSIX extension, not standard C++, and MSVC
      // does not define it without _USE_MATH_DEFINES. The Windows leg caught
      // this; the rest of the tree already uses a local constant for exactly
      // this reason, so match it rather than add a platform define.
      aL[i] = bL[i] = (float)(0.3 * std::sin(2 * kPi * 220 * t));
      aR[i] = bR[i] = (float)(0.3 * std::sin(2 * kPi * 331 * t));
    }

    direct.processStereo(aL.data(), aR.data(), N);

    // Same chunking the shell uses, so the comparison covers the chunk seam and
    // not just one big block — slot state has to survive it.
    float sL[hypersaw::kRackSlots][256], sR[hypersaw::kRackSlots][256];
    float *pL[hypersaw::kRackSlots], *pR[hypersaw::kRackSlots];
    for (int t = 0; t < hypersaw::kRackSlots; t++) { pL[t] = sL[t]; pR[t] = sR[t]; }
    for (int off = 0; off < N; off += 256)
    {
      const int n = N - off < 256 ? N - off : 256;
      const float *srcL[1] = {bL.data() + off};
      const float *srcR[1] = {bR.data() + off};
      m.processBlock(srcL, srcR, pL, pR, bL.data() + off, bR.data() + off, n,
                     [&](int slot, float *L, float *R, int k) {
                       viaMatrix.processSlot(slot, L, R, k);
                     });
    }

    int diff = 0;
    double energy = 0;
    for (int i = 0; i < N; i++)
    {
      if (aL[i] != bL[i] || aR[i] != bR[i]) diff++;
      energy += (double)aL[i] * aL[i] + (double)aR[i] * aR[i];
    }
    std::snprintf(d, sizeof(d), "%d/%d samples differ; reference energy %.4g "
                                "(0 would mean the rack did nothing)", diff, N, energy);
    check(diff == 0 && energy > 1e-3,
          "serial-chain block pass == rack.processStereo, sample for sample", d);

    /* CALIBRATED, and the useful result is the plant that did NOT fire.
       Fires (1023/1024 samples): a gather coefficient off by 1e-6; zeroing the
       output buffer BEFORE the slots gather, which is the aliasing hazard
       processBlock's comment warns about (the shell passes the mix bus as both
       source and destination, so an early zero destroys the input).
       NO-OP: removing the `isTerminal` filter from the terminal sum. It changes
       nothing HERE because setSerialChain leaves outAmount = 0 on every
       non-terminal, so summing them adds zeros. This assertion therefore does
       not cover terminal detection at all — assertion 4 does, and that division
       is recorded rather than left for someone to assume the other way. */
  }


  /* ======================================================================
     B50 PHASE 1 — the matrix as a PARAMETER SURFACE (assertions 8-12).
     ====================================================================== */
  auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const std::vector<Cell> cells = parseCells(hypersaw_debug_routing_ids());

  // ---- 8. every cell round-trips host parameter -> LIVE MATRIX -------------
  /* The claim is NOT "readParam returns what I wrote" — that is one accessor
     agreeing with itself, which is the exact trap state_check records. It is
     "a host write reaches the matrix", so the read side is the live double the
     audio pass multiplies by. */
  {
    Rig r(factory);
    int bad = 0, seen = 0;
    unsigned badId = 0;
    double badWrote = 0, badGot = 0;
    for (const Cell &c : cells)
    {
      clap_param_info_t inf{};
      if (!r.info(c.id, inf)) { if (!bad) badId = c.id; bad++; continue; }
      // A DISTINCT value per cell: writing the same number everywhere would
      // pass on a matrix that silently ignored `from` and `to`.
      const double want = inf.min_value
                          + (inf.max_value - inf.min_value) * (0.13 + 0.047 * (seen % 13));
      r.set(c.id, want);
      const double got = c.kind == 0 ? hypersaw_debug_routing(r.p, c.from, c.to)
                       : c.kind == 1 ? hypersaw_debug_routing_out(r.p, c.to)
                                     : hypersaw_debug_routing_init(r.p, c.to);
      if (std::fabs(got - want) > 1e-9)
      {
        if (!bad) { badId = c.id; badWrote = want; badGot = got; }
        bad++;
      }
      seen++;
    }
    std::snprintf(d, sizeof(d), "%zu cells enumerated, %d wrong (first id %u: wrote %.6g got %.6g)",
                  cells.size(), bad, badId, badWrote, badGot);
    check(bad == 0 && !cells.empty(), "every routing cell round-trips host param -> live matrix", d);
  }

  // ---- 9. ids >= 10000 are DISPATCHED, not derived -------------------------
  /* The trap B50 names: findParam derives the oscillator as id/kOscStride, so
     10000 resolves to oscillator 10, fails `osc >= kNumOsc`, and every routing
     id silently does not exist — with every other gate green. Asserted with a
     probe rather than by reading the branch, and paired with two must-fail
     controls: an id INSIDE the block that names no cell must stay unknown, and
     id 0 (what 10000 aliases to under `% kOscStride`) must stay unknown too. A
     probe that only asserted "10000 exists" would also pass on a shell that
     accepted every integer. */
  /* `get_value`, NOT `get_info`, and that distinction is this assertion's whole
     content. params_get_info walks the routing table BY INDEX and never calls
     findParam, so it keeps reporting every routing id even with the dispatch
     branch deleted — measured: removing the branch left this assertion green
     while three others went red, which is a probe testing the wrong door.
     params_get_value's first line is `if (!findParam(id)) return false;`, so it
     is the id-resolution path and the only honest witness here. */
  {
    Rig r(factory);
    double v = 0;
    const bool live = r.par->get_value(r.p, 10000, &v);
    // 10000 + 63*64 + 63: inside the block, past every cell this build exposes.
    double junk = 0;
    const bool ghost = r.par->get_value(r.p, 14095, &junk);
    // and the other half of the same trap: 10000 % kOscStride is 0, so if the
    // block were resolved by the derivation instead of dispatched, id 0 is what
    // it would alias onto. It must stay unknown.
    double z = 0;
    const bool zero = r.par->get_value(r.p, 0, &z);
    std::snprintf(d, sizeof(d),
                  "get_value(10000)=%s (the osc-10 derivation would say no); "
                  "get_value(14095)=%s (control, must be no); "
                  "get_value(0)=%s (control, must be no)",
                  live ? "yes" : "no", ghost ? "yes" : "no", zero ? "yes" : "no");
    check(live && !ghost && !zero,
          "routing ids dispatch before the id/kOscStride derivation", d);
  }

  // ---- 10. the DEFAULT is today's series chain, told or untold -------------
  /* B50 (b). A fresh instance and one explicitly TOLD every routing parameter
     its own default must hold the same matrix — that is what makes "the
     defaults reproduce the existing chain" measured rather than a reading of
     setSerialChain. Two controls in the same block: one cell moved off its
     default must make the comparison fail (or the comparison sees nothing),
     and the chain itself is spelled out independently. */
  {
    Rig fresh(factory), told(factory);
    for (const Cell &c : cells)
    {
      clap_param_info_t inf{};
      if (!told.info(c.id, inf)) continue;
      told.set(c.id, inf.default_value);
    }
    const bool same = matrixEqual(fresh.p, told.p, cells);

    clap_param_info_t one{};
    told.info(cells[0].id, one);
    told.set(cells[0].id, one.default_value + 0.5);
    const bool differs = !matrixEqual(fresh.p, told.p, cells);

    bool chain = hypersaw_debug_routing(fresh.p, 0, 0) == 1.0
                 && hypersaw_debug_routing_out(fresh.p, 3) == 1.0
                 && hypersaw_debug_routing_out(fresh.p, 0) == 0.0
                 && hypersaw_debug_routing_out(fresh.p, 1) == 0.0
                 && hypersaw_debug_routing_out(fresh.p, 2) == 0.0;
    for (int t = 1; t < 4; t++)          // slot t-1 -> slot t at unity
      chain = chain && hypersaw_debug_routing(fresh.p, 1 + (t - 1), t) == 1.0;
    std::snprintf(d, sizeof(d),
                  "told==fresh %s; control (one cell moved) differs %s; "
                  "src->1->2->3->4->out at unity %s",
                  same ? "yes" : "no", differs ? "yes" : "no", chain ? "yes" : "no");
    check(same && differs && chain, "routing defaults ARE today's series chain", d);
  }

  // ---- 11. a morph between two topologies BLENDS, never argmaxes -----------
  /* ADR-125: structure by argmax, coefficients as VALUES. Two corners hold
     different crosspoint tables; halfway between them the coefficient must be
     an intermediate number, not either corner's. A stepped (argmax'd) parameter
     would sit on one of the two — which is the failure this pins.
     The corners are authored through the SHIPPED path (arm a corner, edit the
     parameter: ADR-109), not by writing morphCorner directly, so what is under
     test is the field a player actually drives.
     MUST-FAIL CONTROL: at a pure corner the value must be that corner's
     exactly. Without it "blended" would also be satisfied by a shell that
     ignored the corners and returned something arbitrary. */
  {
    Rig r(factory);
    const clap_id cell = 10000 + 0 * 64 + 2;   // source -> slot 3: 0 in the chain,
                                               // so both corner values are authored
    const double A = 0.25, B = 1.75;
    r.set(151, 1);      // morph on
    r.set(157, 1);      // blend mode, not the quantum flip
    r.set(158, 0);      // no morph glide: the field lands, it does not creep
    r.set(159, 1); r.set(cell, A);    // arm corner A, author it
    r.set(159, 2); r.set(cell, B);    // arm corner B, author it
    r.set(159, 0);                    // disarm: edits go live again
    r.set(152, 0.0); r.set(153, 0.0); r.render(8);
    const double atA = hypersaw_debug_routing(r.p, 0, 2);
    r.set(152, 1.0); r.set(153, 0.0); r.render(8);
    const double atB = hypersaw_debug_routing(r.p, 0, 2);
    r.set(152, 0.5); r.set(153, 0.0); r.render(8);
    const double mid = hypersaw_debug_routing(r.p, 0, 2);
    const double lo = std::min(atA, atB), hi = std::max(atA, atB);
    const bool blended = mid > lo + 1e-6 && mid < hi - 1e-6;
    const bool corners = std::fabs(atA - A) < 1e-6 && std::fabs(atB - B) < 1e-6;
    std::snprintf(d, sizeof(d),
                  "corner A %.6g (authored %.6g), corner B %.6g (authored %.6g), "
                  "halfway %.6g strictly between = %s",
                  atA, A, atB, B, mid, blended ? "yes" : "no");
    check(corners && blended, "a morph blends crosspoint coefficients as values", d);
  }

  // ---- 12. the `routing` chunk carries the topology, and only when it must --
  /* B50 (c), in three claims that have to hold together:
       · a rerouted matrix survives save -> load into a FRESH instance;
       · a chunk saved on the series chain names no `routing` key at all — this
         is what keeps every existing preset, fixture and golden byte-identical,
         and it is the claim B50 (b) actually rests on;
       · a load with no key returns a rerouted instance TO the chain, rather
         than leaving it holding the previous patch's topology. That third one
         is the ADR-138 scar restated: the bug is not in what a chunk says, it
         is in what a chunk's SILENCE fails to undo. */
  {
    Rig a(factory);
    auto *stA = (const clap_plugin_state_t *)a.p->get_extension(a.p, CLAP_EXT_STATE);

    OStr plain{{nullptr, ostrWrite}, {}};
    stA->save(a.p, &plain.s);
    const bool quietOnChain = plain.data.find("\nrouting=") == std::string::npos;

    // Reroute: source straight into slot 4, slot 1 muted out of the chain.
    a.set(10000 + 0 * 64 + 3, 0.75);
    a.set(10000 + 0 * 64 + 0, 0.0);
    a.set(20000 + 0, 0.5);
    a.set(21000 + 2, -0.25);
    OStr routed{{nullptr, ostrWrite}, {}};
    stA->save(a.p, &routed.s);
    const bool speaksWhenRerouted = routed.data.find("\nrouting=") != std::string::npos;

    Rig b(factory);
    auto *stB = (const clap_plugin_state_t *)b.p->get_extension(b.p, CLAP_EXT_STATE);
    IStr in{{nullptr, istrRead}, routed.data, 0};
    stB->load(b.p, &in.s);
    const bool carried = matrixEqual(a.p, b.p, cells);

    // The silence test: b is rerouted now; loading the CHAIN chunk must undo it.
    IStr back{{nullptr, istrRead}, plain.data, 0};
    stB->load(b.p, &back.s);
    Rig c(factory);
    const bool restored = matrixEqual(b.p, c.p, cells);

    std::snprintf(d, sizeof(d),
                  "chain chunk names routing= %s (must be no); rerouted chunk does %s; "
                  "round-trip carried %s; keyless load returned to the chain %s",
                  quietOnChain ? "no" : "yes", speaksWhenRerouted ? "yes" : "no",
                  carried ? "yes" : "no", restored ? "yes" : "no");
    check(quietOnChain && speaksWhenRerouted && carried && restored,
          "the routing chunk persists a topology and stays silent on the default", d);
  }

  // ---- 13. a slot fed by nothing is silent (reachability) ------------------
  /* The read-side rule made audible: cut every edge into slot 0 and what it
     contributes downstream must be exactly what a zero input produces — not
     "small". Measured on the core with the trivial stand-ins, so what is read
     is topology and not an effect.
     CONTROL: the same chain with its feed intact must differ, or this passes on
     a matrix that is silent for any reason at all (L0032). */
  {
    Matrix cutm;
    cutm.setSerialChain();
    cutm.inFrom[0] = 0;                 // slot 0 reads nothing
    cutm.coeff[0][0] = 0.0;
    const double src[2] = {0.3, 0.2};
    const double cut = cutm.process(src, slotProc);

    Matrix livem;
    livem.setSerialChain();
    const double fed = livem.process(src, slotProc);

    const double want = slotProc(3, slotProc(2, slotProc(1, slotProc(0, 0.0))));
    std::snprintf(d, sizeof(d),
                  "unfed chain %.12g == hand-computed zero-input %.12g; fed %.12g (control)",
                  cut, want, fed);
    check(std::fabs(cut - want) < 1e-12 && std::fabs(fed - cut) > 1e-9,
          "a slot fed by nothing contributes nothing", d);
  }

  /* CALIBRATION, recorded because a green suite proves nothing on its own.
     Removing the read-side acyclicity guard (`edgeLive` -> always true) makes
     assertion 3 FAIL, 1 -> 5. So that guard is load-bearing and this catches
     its loss.
     The lab's OTHER bug — a terminal test that skips the legality check — was
     planted too and is a NO-OP here: `isTerminal` loops `t > slot`, so illegal
     destinations are excluded by the loop bound rather than by the check. The
     bug is not expressible against this shape. Recorded rather than counted as
     a second successful calibration, which is what it would look like from the
     outside.
     NB: these plants must be run with the object file deleted. CMake did not
     track `src/routing_core.h` as a dependency of this target, and the first
     calibration pass read a stale binary and reported two identical failures
     that were one failure twice.

     ---- B50 PHASE 1 CALIBRATION (assertions 8-13, 2026-09-17) --------------
     Four plants in the SHELL, each fired by the assertion written for it:

       1. findParam's routing dispatch deleted (ids fall through to the
          `id / kOscStride` derivation, resolve to oscillator 10 and vanish)
          -> 8, 9, 10 and 11 RED.
       2. setRoutingParam writes `coeff[0][to]` instead of `coeff[from][to]`
          -> 8 RED (6/18 cells), 10 RED, 11 RED.
       3. the routing ParamDefs declared `stepped` (so the morph ARGMAXes them
          instead of blending) -> 11 RED, and 8 RED because the apply path
          rounds. This is the ADR-125 claim's own plant.
       4. applyRoutingChunk's reset-to-default loop removed -> 12 RED on
          exactly one of its four clauses ("keyless load returned to the chain
          no"), the others untouched.

     THE PLANT THAT TAUGHT SOMETHING. Plant 1 was run first against an
     assertion 9 that asked `params_get_info`, and 9 stayed GREEN while three
     others went red — because get_info walks the routing table BY INDEX and
     never calls findParam. The probe was testing a door the bug does not go
     through. It now asks `params_get_value`, whose first line IS the findParam
     call, and plant 1 fires it. Recorded because a green assertion next to red
     ones is the most expensive kind of pass: it looks like coverage.

     Also confirmed the hard way and worth restating: the failed build during
     that pass left the PREVIOUS binary in place and it ran and reported, so the
     numbers on screen described a plant that was no longer in the source
     (L0032's stale-object case). Read the compile result before the assertions,
     every time. */
  std::printf("routing_check: %s (%d failures)\n", failures ? "RED" : "GREEN", failures);
  return failures ? 1 : 0;
}
