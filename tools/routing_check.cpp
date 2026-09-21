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
 * WIRED: ./verify full.
 */
#include <cmath>
#include <cstdio>
#include <string>    // MSVC: std::string is not reachable transitively (CI build-windows, PR #607)
#include <vector>

#include <cstring>
#include <algorithm>
#include "../src/routing_core.h"
#include "../src/fx_rack.h"
#include "../src/hypersaw_clap_entry.h"
/* B50 phase 1 (routing_*) and B142 (ownersjson): the shell's window onto the
   LIVE matrix, and the GUI's own ownership report. */
#include "../src/hypersaw_debug.h"

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

/* ONE reader for a cell's live value, whatever kind it is — assertion 8 and
   matrixEqual both go through here. Two copies of this switch is how a new
   kind gets read as 0 by one of them and correctly by the other, which reads
   as a shell bug and is not one: when kRoutingSrcOut (kind 3) was added, the
   copy inside assertion 8 fell through to the slotInit reader with to = -1 and
   reported "wrote 0.73 got 0" while the parameter had in fact arrived. */
static double cellValue(const clap_plugin_t *p, const Cell &c)
{
  switch (c.kind)
  {
    case 0: return hypersaw_debug_routing(p, c.from, c.to);
    case 1: return hypersaw_debug_routing_out(p, c.to);
    case 3: return hypersaw_debug_routing_srcout(p, c.from);
    default: return hypersaw_debug_routing_init(p, c.to);
  }
}

/* The id layout's ROW coordinates (ADR-088 amendment, 2026-09-18): rows 0..7
   are reserved for SOURCES and the slots begin at row 8, so a `from` read off
   the cell list is NOT routing_core.h's matrix index. These two let an
   assertion NAME a cell it wants to drive; the cell LIST and every membership
   test still come from the shell, so this is not a second decodeRoutingId. */
constexpr int kSlotRow0 = 8;
static unsigned coeffId(int row, int to) { return 10000u + (unsigned)row * 64u + (unsigned)to; }

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

/* How many SOURCES this build exposes, read off the shell's own cell list: one
   `srcOut` cell (kind 3) per source, by construction of makeRoutingTable. Never
   a literal — increment 3 exists because the source count grows, and an oracle
   carrying its own copy of it would keep passing after the shell moved on. */
static int srcCount(const std::vector<Cell> &cells)
{
  int n = 0;
  for (const Cell &c : cells)
    if (c.kind == 3) n++;
  return n;
}

/* `{"11":2,"12":-1,…}` — id -> owning corner, -1 unowned, -2 held (ADR-110).
   Same shape-follows-the-shell rule as parseCells: the key set is whatever the
   field contains, never a list restated here. */
struct Owner { unsigned id; int k; };
static std::vector<Owner> parseOwners(const char *s)
{
  std::vector<Owner> out;
  while (s && *s)
  {
    const char *q = std::strchr(s, '"');
    if (!q) break;
    Owner o{};
    if (std::sscanf(q, "\"%u\":%d", &o.id, &o.k) != 2) break;
    out.push_back(o);
    const char *comma = std::strchr(q, ',');   // values carry none; the last ends in '}'
    if (!comma) break;
    s = comma + 1;
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
    notes.reserve(64);
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
  /* A NOTE, through the same door — the matrix has nothing to route until the
     instrument sounds. Stored here rather than on the caller's stack for the
     reason `store` carries: `evl.ev` holds raw pointers, so the event must
     outlive the process() call that reads it, and the vector is reserved once
     so no push can move what is already pointed at. */
  std::vector<clap_event_note_t> notes;
  void noteOn(int key)
  {
    clap_event_note_t n{};
    n.header.size = sizeof(n); n.header.type = CLAP_EVENT_NOTE_ON;
    n.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    n.note_id = (int32_t)notes.size() + 1; n.port_index = 0; n.channel = 0;
    n.key = key; n.velocity = 1.0;
    notes.push_back(n);
    evl.ev.push_back(&notes.back().header);
  }
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
    if (cellValue(a, c) != cellValue(b, c)) return false;
    if (c.kind == 0 && hypersaw_debug_routing_on(a, c.from, c.to)
                           != hypersaw_debug_routing_on(b, c.from, c.to))
      return false;   // the presence bit is part of the topology, not a detail
  }
  return true;
}

/* ---- B139: a stand-in slot that can DELAY --------------------------------
   Assertions 17-19 are about loop TIME — the one-sample delay a cycle edge
   carries, and the 5 ms a real loop is made of — and a memoryless gain cannot
   express either. Still a stand-in and not an effect: a gain and a ring, so
   what is measured stays the routing rule (L0030/L0031).
   The scalar ring holds doubles and the block ring floats, on purpose: that is
   the actual asymmetry between the two paths, and hiding it behind one type
   would let assertion 17 pass on a similarity it invented. */
struct LoopSlots
{
  static constexpr int kN = 4;
  int d[kN] = {0, 0, 0, 0};             // delay, samples
  double g[kN] = {1.0, 1.0, 1.0, 1.0};  // gain
  std::vector<double> zs[kN];
  std::vector<float> zl[kN], zr[kN];
  int ws[kN] = {0, 0, 0, 0}, wb[kN] = {0, 0, 0, 0};

  void arm()
  {
    for (int t = 0; t < kN; t++)
    {
      const int len = d[t] > 0 ? d[t] : 1;
      zs[t].assign((size_t)len, 0.0);
      zl[t].assign((size_t)len, 0.0f);
      zr[t].assign((size_t)len, 0.0f);
      ws[t] = wb[t] = 0;
    }
  }

  double scalar(int t, double x)
  {
    if (d[t] <= 0) return g[t] * x;
    const double y = zs[t][(size_t)ws[t]];
    zs[t][(size_t)ws[t]] = x;
    ws[t] = (ws[t] + 1) % d[t];
    return g[t] * y;
  }

  void block(int t, float *L, float *R, int n)
  {
    for (int i = 0; i < n; i++)
    {
      if (d[t] <= 0) { L[i] = (float)(g[t] * L[i]); R[i] = (float)(g[t] * R[i]); continue; }
      const float yl = zl[t][(size_t)wb[t]], yr = zr[t][(size_t)wb[t]];
      zl[t][(size_t)wb[t]] = L[i];
      zr[t][(size_t)wb[t]] = R[i];
      wb[t] = (wb[t] + 1) % d[t];
      L[i] = (float)(g[t] * yl); R[i] = (float)(g[t] * yr);
    }
  }
};

/* The one cycle topology all three assertions share: the default serial chain
   (src -> 0 -> 1 -> 2 -> 3 -> out) plus ONE backwards edge, slot 2 -> slot 1.
   The loop is therefore slot 1 -> slot 2 -> slot 1, one sample per trip plus
   whatever delay the stand-in carries. */
static Matrix cycleMatrix(double loopGain)
{
  Matrix m;                            // ctor == setSerialChain
  m.inFrom[1] |= (1u << (2 + 2));      // slot 2 -> slot 1, backwards
  m.coeff[2 + 2][1] = loopGain;
  return m;
}

/* One block render at a chosen block size, through the shipped block path. */
static void renderBlock(Matrix m, LoopSlots &sl, const std::vector<float> &inL,
                        const std::vector<float> &inR, int bs,
                        std::vector<float> &outL, std::vector<float> &outR)
{
  const int N = (int)inL.size();
  outL.assign((size_t)N, 0.0f);
  outR.assign((size_t)N, 0.0f);
  m.resetFeedback();
  sl.arm();
  float sL[4][256], sR[4][256];
  float *pL[4], *pR[4];
  for (int t = 0; t < 4; t++) { pL[t] = sL[t]; pR[t] = sR[t]; }
  for (int off = 0; off < N; off += bs)
  {
    const int n = N - off < bs ? N - off : bs;
    // Source 1 is silent in every topology below — it is passed anyway so the
    // two-source matrix is driven exactly as the shell drives its one-source
    // one, and so an edge that read the wrong source index would show.
    static const float kSilent[256] = {0.0f};
    const float *srcL[2] = {inL.data() + off, kSilent};
    const float *srcR[2] = {inR.data() + off, kSilent};
    m.processBlock(srcL, srcR, pL, pR, outL.data() + off, outR.data() + off, n,
                   [&](int slot, float *L, float *R, int k) { sl.block(slot, L, R, k); });
  }
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
      const double got = cellValue(r.p, c);
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

    /* EVERY source feeds slot 1 at unity: with two sources that IS the old
       summed bus (B23 increment 3), and the source count is read off the cell
       list rather than written here. The slot rows start at kSlotRow0, not at
       the source count — `from` is a ROW (ADR-088 amendment). */
    const int nSrc = srcCount(cells);
    bool chain = nSrc > 0
                 && hypersaw_debug_routing_out(fresh.p, 3) == 1.0
                 && hypersaw_debug_routing_out(fresh.p, 0) == 0.0
                 && hypersaw_debug_routing_out(fresh.p, 1) == 0.0
                 && hypersaw_debug_routing_out(fresh.p, 2) == 0.0;
    for (int s = 0; s < nSrc; s++)       // every source -> slot 1 at unity
      chain = chain && hypersaw_debug_routing(fresh.p, s, 0) == 1.0;
    for (int t = 1; t < 4; t++)          // slot t-1 -> slot t at unity
      chain = chain && hypersaw_debug_routing(fresh.p, kSlotRow0 + (t - 1), t) == 1.0;
    std::snprintf(d, sizeof(d),
                  "told==fresh %s; control (one cell moved) differs %s; "
                  "%d src->1->2->3->4->out at unity %s",
                  same ? "yes" : "no", differs ? "yes" : "no", nSrc, chain ? "yes" : "no");
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

  /* ======================================================================
     B50 PHASE 1c — THE DRY PATH (assertions 14-16).
     The brief numbers these (16)-(18); they are blocks 14-16 here, because the
     file's own blocks run 1-13. Recorded so the two numberings are not read as
     three missing assertions.
     ====================================================================== */

  // ---- 14. Src->OUT = 1 with the rack cut out IS the dry input -------------
  /* The claim phase 1c exists for: a fully bypassed rack must be expressible as
     an EDGE. Measured on the BLOCK path over the real rack, with `outL/outR`
     ALIASING the source buffers exactly as the shell passes them — that
     aliasing is why the dry term had to initialise the output instead of being
     added to a zeroed one, and a test that used separate buffers would not see
     the difference.
     TWO MUST-FAIL CONTROLS, because "the output equals the input" is also what
     a matrix that never touched the buffer would produce:
       · srcOut 0 in the SAME cut shape must render SILENCE, not the input —
         that is what proves the buffer is written rather than left alone;
       · srcOut 0.5 must render exactly half, so the coefficient is a gain and
         not a presence flag. */
  {
    const int N = 1024;
    std::vector<float> refL(N), refR(N);
    for (int i = 0; i < N; i++)
    {
      const double t = (double)i / 44100.0;
      refL[i] = (float)(0.3 * std::sin(2 * kPi * 220 * t));
      refR[i] = (float)(0.3 * std::sin(2 * kPi * 331 * t));
    }

    // Every slot ACTIVE, so "the rack contributed nothing" is a fact about the
    // ROUTING and not about an idle rack that could not have contributed.
    hypersaw::FxRack rack;
    const int types[hypersaw::kRackSlots] = {1, 2, 4, 5};
    for (int i = 0; i < hypersaw::kRackSlots; i++)
    { rack.setType(i, types[i]); rack.setAmount(i, 0.4 + 0.1 * i); }

    // The rack cut out of the graph entirely: no crosspoint, no out amount, no
    // slot init. The ONLY route to the output is the dry path.
    auto cutMatrix = [](double dry) {
      hypersaw::RoutingMatrix<1, hypersaw::kRackSlots> m;
      for (int t = 0; t < hypersaw::kRackSlots; t++)
      {
        m.inFrom[t] = 0; m.outAmount[t] = 0; m.slotInit[t] = 0;
        for (int f = 0; f < 1 + hypersaw::kRackSlots; f++) m.coeff[f][t] = 0;
      }
      m.srcOut[0] = dry;
      return m;
    };
    auto render = [&](double dry, std::vector<float> &L, std::vector<float> &R) {
      auto m = cutMatrix(dry);
      float sL[hypersaw::kRackSlots][256], sR[hypersaw::kRackSlots][256];
      float *pL[hypersaw::kRackSlots], *pR[hypersaw::kRackSlots];
      for (int t = 0; t < hypersaw::kRackSlots; t++) { pL[t] = sL[t]; pR[t] = sR[t]; }
      for (int off = 0; off < N; off += 256)
      {
        const int n = N - off < 256 ? N - off : 256;
        const float *srcL[1] = {L.data() + off};
        const float *srcR[1] = {R.data() + off};
        // out ALIASES src, the shell's own call shape (the mix bus is both).
        m.processBlock(srcL, srcR, pL, pR, L.data() + off, R.data() + off, n,
                       [&](int slot, float *l, float *r, int k) { rack.processSlot(slot, l, r, k); });
      }
    };

    std::vector<float> oneL = refL, oneR = refR;      // srcOut = 1
    std::vector<float> zeroL = refL, zeroR = refR;    // srcOut = 0  (control)
    std::vector<float> halfL = refL, halfR = refR;    // srcOut = 0.5 (control)
    render(1.0, oneL, oneR);
    render(0.0, zeroL, zeroR);
    render(0.5, halfL, halfR);

    int diff = 0, quiet = 0, halfBad = 0;
    double energy = 0;
    for (int i = 0; i < N; i++)
    {
      if (oneL[i] != refL[i] || oneR[i] != refR[i]) diff++;
      if (zeroL[i] == 0.0f && zeroR[i] == 0.0f) quiet++;
      if (halfL[i] != (float)(0.5 * refL[i]) || halfR[i] != (float)(0.5 * refR[i])) halfBad++;
      energy += (double)refL[i] * refL[i] + (double)refR[i] * refR[i];
    }
    std::snprintf(d, sizeof(d),
                  "dry=1: %d/%d samples differ from the input (energy %.4g); "
                  "control dry=0 silent on %d/%d; control dry=0.5 exact on %d/%d",
                  diff, N, energy, quiet, N, N - halfBad, N);
    check(diff == 0 && energy > 1e-3 && quiet == N && halfBad == 0,
          "Src->OUT alone renders the dry input, sample for sample", d);
  }

  // ---- 15. the dry path is INERT at its default ---------------------------
  /* B50 (b) extended to phase 1c: srcOut defaults to 0, so the terminal sum
     gains a term that contributes nothing and every existing golden, fixture
     and parity chain still renders what it rendered. Spelled out by hand, like
     assertion 1, rather than by running a second matrix — a comparison of the
     implementation against itself would pass with the term wired backwards.
     MUST-FAIL CONTROL: a non-zero srcOut must move the output by EXACTLY
     srcOut x src, so the term is present and is a gain on the source, not on
     something else that happens to be near it. */
  {
    Matrix m;
    m.setSerialChain();
    const double src[2] = {0.3, 0.2};
    const double got = m.process(src, slotProc);
    const double want = slotProc(3, slotProc(2, slotProc(1, slotProc(0, 0.3 + 0.2))));

    Matrix wet;
    wet.setSerialChain();
    wet.srcOut[0] = 0.25; wet.srcOut[1] = -0.5;
    const double dry = wet.process(src, slotProc);
    const double wantDry = want + 0.25 * 0.3 + (-0.5) * 0.2;

    /* and the third clause: setSerialChain must RESET a dry path someone set,
       or "the chain is the default" stops being true for a reused matrix. */
    wet.setSerialChain();
    const bool reset = wet.srcOut[0] == 0.0 && wet.srcOut[1] == 0.0;

    std::snprintf(d, sizeof(d),
                  "default %.12g == hand-computed chain %.12g; control srcOut "
                  "(0.25,-0.5) gives %.12g, closed form %.12g; setSerialChain resets %s",
                  got, want, dry, wantDry, reset ? "yes" : "no");
    check(got == want && std::fabs(dry - wantDry) < 1e-12 && dry != got && reset,
          "the dry path is inert at 0 and exactly srcOut x src when set", d);
  }

  // ---- 16. the dry path rides the `routing` chunk -------------------------
  /* Same three claims assertion 12 makes, asked of the NEW cell specifically:
     a chunk saved on the chain must still name no `routing` key (so phase 1c
     costs no existing preset a byte), a non-zero dry path must survive
     save -> load into a fresh instance, and a chunk whose SILENCE omits the
     cell must return it to 0 rather than leaving the previous patch's dry
     path in place. The last is the ADR-138 scar, which is about what a chunk
     fails to undo and not about what it says. */
  {
    Rig a(factory);
    auto *stA = (const clap_plugin_state_t *)a.p->get_extension(a.p, CLAP_EXT_STATE);
    OStr plain{{nullptr, ostrWrite}, {}};
    stA->save(a.p, &plain.s);
    const bool quietOnChain = plain.data.find("\nrouting=") == std::string::npos;

    a.set(22000 + 0, 0.625);
    const bool live = hypersaw_debug_routing_srcout(a.p, 0) == 0.625;
    OStr wet{{nullptr, ostrWrite}, {}};
    stA->save(a.p, &wet.s);
    const bool named = wet.data.find("22000:") != std::string::npos;

    Rig b(factory);
    auto *stB = (const clap_plugin_state_t *)b.p->get_extension(b.p, CLAP_EXT_STATE);
    IStr in{{nullptr, istrRead}, wet.data, 0};
    stB->load(b.p, &in.s);
    const bool carried = hypersaw_debug_routing_srcout(b.p, 0) == 0.625;

    IStr back{{nullptr, istrRead}, plain.data, 0};
    stB->load(b.p, &back.s);
    const bool restored = hypersaw_debug_routing_srcout(b.p, 0) == 0.0;

    std::snprintf(d, sizeof(d),
                  "host write reached the matrix %s; chain chunk names routing= %s "
                  "(must be no); wet chunk names 22000 %s; round-trip carried %s; "
                  "keyless load returned it to 0 %s",
                  live ? "yes" : "no", quietOnChain ? "no" : "yes", named ? "yes" : "no",
                  carried ? "yes" : "no", restored ? "yes" : "no");
    check(live && quietOnChain && named && carried && restored,
          "the dry path persists in the routing chunk and defaults to 0", d);
  }

  /* ======================================================================
     B139 — THE BLOCK PATH HONOURS THE ONE-SAMPLE DELAY (assertions 17-19).
     The brief numbers these (19)-(21); they are blocks 17-19 here, continuing
     the offset the phase-1c note above records.

     WHY THEY EXIST. `process()` has read `zPrev` per sample since ADR-128, but
     `processBlock` — the path the shell actually calls — gathered a whole block
     per slot, so a backwards edge read the previous BLOCK. Nothing was red:
     phase 1 exposes only the acyclic cell subset, so the disagreement was
     latent and would have surfaced as "the loop sounds different in the plugin
     than in the oracle" at the first exposed feedback cell. These three are the
     gate on phase 2.
     ====================================================================== */

  // ---- 17. one cycle edge: both paths agree and the BLOCK SIZE does not
  //          enter the answer -----------------------------------------------
  /* Two legs, because the two claims have different evidence:
     (a) block-size invariance is compared float-path-against-float-path, so it
         owes nothing to exact arithmetic and is asked on a real sine;
     (b) scalar-against-block is a double path against a float one, so it is
         asked where both types are EXACT — an impulse through power-of-two
         coefficients, where every value in the loop is 2^-k. Any other input
         would measure float-vs-double accumulation and report it as a routing
         disagreement. The claim under test is which SAMPLE each edge reads.
     Two controls: the loop must be audible at all (or every block size agrees
     on an answer the cycle never touched), and the cycle BRANCH with a zero
     coefficient must render the serial chain bit-exactly (or the per-sample
     regime is a second engine rather than a routing rule). */
  {
    const int N = 1024;
    const int sizes[4] = {1, 7, 64, 256};
    std::vector<float> sigL(N), sigR(N);
    for (int i = 0; i < N; i++)
    {
      const double t = (double)i / 44100.0;
      sigL[i] = (float)(0.3 * std::sin(2 * kPi * 220 * t));
      sigR[i] = (float)(0.25 * std::sin(2 * kPi * 331 * t));
    }

    LoopSlots sl;                    // memoryless: the loop time IS the edge
    std::vector<float> refL, refR, gotL, gotR;
    renderBlock(cycleMatrix(0.5), sl, sigL, sigR, sizes[0], refL, refR);
    int sizeDiff = 0;
    for (int k = 1; k < 4; k++)
    {
      renderBlock(cycleMatrix(0.5), sl, sigL, sigR, sizes[k], gotL, gotR);
      for (int i = 0; i < N; i++)
        if (gotL[i] != refL[i] || gotR[i] != refR[i]) sizeDiff++;
    }

    std::vector<float> plainL, plainR;
    renderBlock(Matrix(), sl, sigL, sigR, 64, plainL, plainR);   // no cycle edge
    int audible = 0;
    for (int i = 0; i < N; i++) if (plainL[i] != refL[i]) audible++;

    std::vector<float> zgL, zgR;
    renderBlock(cycleMatrix(0.0), sl, sigL, sigR, 64, zgL, zgR);
    bool branchSame = true;
    for (int i = 0; i < N; i++)
      if (zgL[i] != plainL[i] || zgR[i] != plainR[i]) branchSame = false;

    std::vector<float> impL(N, 0.0f), impR(N, 0.0f), scal(N);
    impL[0] = 1.0f; impR[0] = 1.0f;
    Matrix ms = cycleMatrix(0.5);
    ms.resetFeedback();
    LoopSlots ss;
    ss.arm();
    for (int i = 0; i < N; i++)
    {
      const double src[2] = {(double)impL[i], 0.0};
      scal[i] = (float)ms.process(src, [&](int t, double x) { return ss.scalar(t, x); });
    }
    int pathDiff = 0;
    for (int k = 0; k < 4; k++)
    {
      renderBlock(cycleMatrix(0.5), sl, impL, impR, sizes[k], gotL, gotR);
      for (int i = 0; i < N; i++)
        if (gotL[i] != scal[i] || gotR[i] != scal[i]) pathDiff++;
    }

    std::snprintf(d, sizeof(d),
                  "block 1/7/64/256 disagree on %d samples; scalar vs block %d; "
                  "loop audible on %d (control); zero-gain cycle == chain %s",
                  sizeDiff, pathDiff, audible, branchSame ? "yes" : "no");
    check(sizeDiff == 0 && pathDiff == 0 && audible > 0 && branchSame,
          "a cycle edge reads one SAMPLE late in the block path too", d);
  }

  // ---- 18. an unstable loop cannot SELF-START -----------------------------
  /* Silence in, exact silence out, at a loop gain of 1.2 — the cheapest test
     that the feedback path adds nothing of its own (an uninitialised carry, a
     denormal seeded by the gather, a slot buffer read before it is written).
     "Exact" is the whole assertion: at gain 1.2 anything non-zero is amplified
     without bound, so an approximate version of this would pass on a defect one
     second from full scale.
     MUST-READ-NONZERO CONTROL: the same loop fed one impulse must run away, or
     "silent" is a statement about a loop that is not connected (L0032). */
  {
    const int N = 4096;
    std::vector<float> quiet((size_t)N, 0.0f), oL, oR;
    LoopSlots sl;
    renderBlock(cycleMatrix(1.2), sl, quiet, quiet, 64, oL, oR);
    int loud = 0;
    for (int i = 0; i < N; i++) if (oL[i] != 0.0f || oR[i] != 0.0f) loud++;

    const int M = 256;
    std::vector<float> impL((size_t)M, 0.0f), impR((size_t)M, 0.0f), gL, gR;
    impL[0] = 1.0f; impR[0] = 1.0f;
    renderBlock(cycleMatrix(1.2), sl, impL, impR, 64, gL, gR);
    double peak = 0;
    for (int i = 0; i < M; i++) peak = std::max(peak, (double)std::fabs(gL[i]));

    std::snprintf(d, sizeof(d),
                  "gain 1.2, 4096 samples of silence: %d non-zero output samples; "
                  "control impulse into the same loop reaches %.3g", loud, peak);
    check(loud == 0 && peak > 1e6, "an unstable loop cannot self-start from silence", d);
  }

  // ---- 19. a 5 ms loop at 0.6 decays by 100 dB inside 240 ms ---------------
  /* The Maw packet's own stability criterion, asked of the routing loop rather
     than of an effect: the delay is a stand-in ring, so what is measured is the
     feedback path and not a filter. 220 samples at 44.1 kHz plus the edge's own
     sample is a 5.01 ms trip, 0.6 per trip, ~48 trips in 240 ms.
     MUST-NOT-DECAY CONTROL: the same loop at gain 1.0 must still be at full
     scale in the same window, or "decayed" is what this measurement says about
     a loop that never circulated. */
  {
    const double sr = 44100.0;
    const int D = 220;                        // 4.99 ms; the trip is D + 1
    const int N = (int)(0.250 * sr);          // 240 ms + a 10 ms measurement window
    const int mark = (int)(0.240 * sr);
    std::vector<float> impL((size_t)N, 0.0f), impR((size_t)N, 0.0f), oL, oR, hL, hR;
    impL[0] = 1.0f; impR[0] = 1.0f;

    LoopSlots decay;
    decay.d[2] = D;                           // the delay sits INSIDE the loop
    renderBlock(cycleMatrix(0.6), decay, impL, impR, 64, oL, oR);
    double peak = 0, tail = 0;
    for (int i = 0; i < N; i++)
    {
      const double a = std::fabs((double)oL[i]);
      if (a > peak) peak = a;
      if (i >= mark && a > tail) tail = a;
    }
    const double decayDb = 20.0 * std::log10((tail > 0 ? tail : 1e-300) / peak);

    LoopSlots hold;
    hold.d[2] = D;
    renderBlock(cycleMatrix(1.0), hold, impL, impR, 64, hL, hR);
    double held = 0;
    for (int i = mark; i < N; i++) held = std::max(held, (double)std::fabs(hL[i]));
    const double holdDb = 20.0 * std::log10(held > 0 ? held : 1e-300);

    std::snprintf(d, sizeof(d),
                  "peak %.4g; tail after 240 ms %.4g (%.1f dB, need <= -100); "
                  "control gain 1.0 holds at %.1f dB", peak, tail, decayDb, holdDb);
    check(peak > 0.5 && decayDb <= -100.0 && holdDb > -1.0,
          "a 5 ms loop at 0.6 decays 100 dB inside 240 ms", d);
  }

  /* ======================================================================
     B142 — THE ROUTING BLOCK IS ONE ATOM UNDER QUANTUM (assertions 20-21).
     The brief numbers these (22)-(23); they are blocks 20-21 here, continuing
     the offset the phase-1c note above records.

     WHY THEY EXIST. Assertion 11 above pins the BLEND law and sets `157 = 1`
     to do it, so the shipped default — quantum, 157 = 0 — had no assertion at
     all. Under it `morphInit` left `morphLead` at identity for the routing
     block, so every crosspoint drew its own corner: a live table assembled
     from up to four corners, which is a topology none of them authored and,
     under ADR-175, can carry a cycle two acyclic corners do not. ADR-176 §3
     rules the block ONE atomic group under quantum; these two are its gate.

     ONE SWEEP, TWO CLAIMS. Both read the same 200 pad positions — the owner
     REPORT (20) and the live MATRIX (21) — deliberately: if they were two
     sweeps, a disagreement between what the field reports and what it
     multiplies by would read as two green assertions.
     ====================================================================== */
  {
    /* Distinct per (corner, cell) BY CONSTRUCTION: corners are 0.2 apart and a
       whole table spans at most 0.09, so no two corners can hold the same
       value for any cell and "which corner is this table?" has one answer.
       Inside every kind's range (coeff ±2, out/srcout 0..2, init ±1). */
    auto authored = [](int corner, size_t cell) {
      return 0.1 + 0.2 * (double)corner + 0.005 * (double)cell;
    };
    const size_t nCells = cells.size();
    std::vector<std::vector<double>> table(4, std::vector<double>(nCells, 0.0));
    for (int k = 0; k < 4; k++)
      for (size_t c = 0; c < nCells; c++) table[(size_t)k][c] = authored(k, c);

    /* Which corner's table is this, or none? -1 for a chimera. 1e-9, not bit
       equality: the morph glide computes `a + (b - a) * coef` and at coef 1
       that is not exactly `b` — while 1e-9 is eight orders below the 0.2 that
       separates two corners, so nothing a chimera could do hides under it. */
    auto matchCorner = [&](const std::vector<double> &live) {
      for (int k = 0; k < 4; k++)
      {
        bool all = true;
        for (size_t c = 0; c < nCells && all; c++)
          all = std::fabs(live[c] - table[(size_t)k][c]) < 1e-9;
        if (all) return k;
      }
      return -1;
    };

    Rig r(factory);
    r.set(151, 1);      // morph on
    r.set(158, 0);      // no morph glide: the field lands, it does not creep
    // 157 is left at its DEFAULT (0 = quantum), and 154/155 (temperature,
    // coupling) too: the shipped field is the thing under test.
    for (int k = 0; k < 4; k++)
    {
      r.set(159, (double)(k + 1));   // arm corner k, author its whole table
      for (size_t c = 0; c < nCells; c++) r.set(cells[c].id, authored(k, c));
    }
    r.set(159, 0);      // disarm: edits go live again

    int splitPositions = 0;      // routing cells disagreeing — must stay 0
    int ctrlSplitPositions = 0;  // identity-lead params disagreeing — control
    int unmatched = 0;           // live tables matching no corner — must stay 0
    bool blockCorner[4] = {false, false, false, false};
    int worstSplit = 1;
    std::vector<double> live(nCells, 0.0);
    for (int i = 0; i < 200; i++)
    {
      const double x = (double)(i % 20) / 19.0, y = (double)(i / 20) / 9.0;
      r.set(152, x); r.set(153, y); r.render(2);

      /* Membership comes from the SHELL's cell list, never from a second copy
         of "an id >= 10000 is a routing id" (L0032). */
      bool seen[4] = {false, false, false, false};
      bool ctrlSeen[4] = {false, false, false, false};
      for (const Owner &o : parseOwners(hypersaw_debug_ownersjson(r.p)))
      {
        if (o.k < 0 || o.k > 3) continue;   // -1 not owned, -2 held
        bool isCell = false;
        for (const Cell &c : cells) if (c.id == o.id) { isCell = true; break; }
        (isCell ? seen : ctrlSeen)[o.k] = true;
      }
      int nOwners = 0, nCtrl = 0;
      for (int k = 0; k < 4; k++)
      {
        if (seen[k]) { nOwners++; blockCorner[k] = true; }
        if (ctrlSeen[k]) nCtrl++;
      }
      if (nOwners > 1) splitPositions++;
      if (nOwners > worstSplit) worstSplit = nOwners;
      if (nCtrl > 1) ctrlSplitPositions++;

      for (size_t c = 0; c < nCells; c++) live[c] = cellValue(r.p, cells[c]);
      if (matchCorner(live) < 0) unmatched++;
    }
    int blockCorners = 0;
    for (int k = 0; k < 4; k++) if (blockCorner[k]) blockCorners++;

    /* MUST-FAIL CONTROLS, both halves (L0032).
       (a) The identity-lead parameters — the same picker, the same report, the
           same 200 positions — must SPLIT somewhere, or "the block agrees" is
           a statement about a picker that returns one corner for everything
           and the fix is unproven. Identity leads are what the routing block
           had before this change, so this control IS the planted map, read off
           the parameters that still carry it.
       (b) The block must take at least two DIFFERENT corners across the pad,
           or the cells agree only because nothing ever flips. */
    std::snprintf(d, sizeof(d),
                  "200 positions, 4 different corner tables: cells split at %d "
                  "(worst %d owners); control (identity-lead params, same picker) "
                  "splits at %d; block takes %d corners across the pad",
                  splitPositions, worstSplit, ctrlSplitPositions, blockCorners);
    check(splitPositions == 0 && ctrlSplitPositions > 0 && blockCorners >= 2,
          "under quantum every routing cell reports ONE owner", d);

    // ---- 21. and the live matrix is that corner's table, not a chimera ----
    /* The owner report is a claim; the doubles `processBlock` multiplies by are
       the fact (the reason `hypersaw_debug_routing` exists at all). One owner
       per cell with a matrix that still mixed corners would be a lie the GUI
       would paint confidently.
       DETECTOR CALIBRATION, both directions: `matchCorner` must HIT a real
       corner table and MISS a hand-built chimera — corner 0's table with one
       cell taken from corner 1, which is precisely the shape the identity lead
       map produced. Without the miss half, "every position matched" would also
       be true of a predicate that matches anything. */
    std::vector<double> chimera = table[0];
    chimera[0] = table[1][0];
    const bool detectorHits = matchCorner(table[2]) == 2;
    const bool detectorMisses = matchCorner(chimera) < 0;
    std::snprintf(d, sizeof(d),
                  "%d of 200 live tables matched no corner (must be 0); detector hits a "
                  "real corner table %s, misses a one-cell chimera %s (control)",
                  unmatched, detectorHits ? "yes" : "no", detectorMisses ? "yes" : "no");
    check(unmatched == 0 && detectorHits && detectorMisses,
          "under quantum the live matrix IS one corner's table", d);
  }

  /* ---- 22. B146: bass-mono's PLACEMENT relative to the rack ---------------
     `bassMonoPos` moves the ADR-035 stage from before the rack (pre, the only
     placement there has ever been) to after it (post), or runs both. The
     ruling rests on one claim about the rack, and this measures it in both
     directions rather than asserting the comfortable half:

       A. WITH THE RACK BYPASSED the two placements are BIT-IDENTICAL. Nothing
          sits between the two points, so this is the honest statement of "the
          reorder is free when the chain is empty" — and it is the arm that
          fails if post silently does nothing at all.
       B. …which is why A needs a control that MUST read non-zero: `both` runs
          a SECOND stage over the same samples, so it must differ from `pre` in
          exactly the configuration where pre and post agree. Without this,
          A also passes on a parameter wired to nothing (L0032: the control
          that must read zero and the corruption that must not, in one probe).
       C. WITH A NONLINEAR SLOT BETWEEN THEM the placements DIFFER. This is the
          whole reason post exists: a stereo-symmetric LINEAR slot commutes
          with the side high-pass (measured on Comb, 2026-08: the same ~11%
          residual either way), but Drive runs per channel, so f(L)-f(R) makes
          side content out of the MID below the crossover that an upstream
          stage never saw and cannot remove.
       D. AND A CONTROL FOR THE COMPARATOR ITSELF: with bass mono OFF, pre and
          post must be bit-identical even with Drive engaged. That is what
          proves a difference in C is the STAGE MOVING and not two instances
          of the instrument disagreeing — a difference every arm here would
          otherwise report as a feature.

     WHAT THIS DOES NOT COVER, named rather than left to be assumed. Arm B asks
     only that `both` DIFFERS from `pre`, not that it is two correct stages in
     series. A plant that gave the post stage the PRE stage's filter state —
     the exact bug the two state pairs exist to prevent — was run and did NOT
     fire: `both` still differs, `post` alone still starts from a clean state,
     so every clause here reads the same (calibration pass, 2026-09-18).
     Recorded as a coverage boundary rather than retried until something fired
     (L0033). Distinguishing them needs the side-band slope (24 vs 48 dB/oct),
     and the only way to measure it from here is a second copy of the filter
     inside the oracle — which is the duplication this file exists to refuse.
     The defence is structural instead: bassMonoStage takes its state BY
     REFERENCE and the two call sites pass different members. */
  {
    /* One block of settled audio for a given placement, with or without a
       nonlinear slot between the two points. A LOW note (key 24, ~32.7 Hz)
       with the default width: bass mono can only be observed where there is
       side content BELOW the crossover, and a probe that renders none would
       report "identical" for every arm and call it a pass. */
    auto capture = [&](int pos, bool on, bool drive, std::vector<float> &L, std::vector<float> &R) {
      Rig r(factory);
      r.set(40, on ? 1 : 0);      // bass mono
      r.set(41, 120);             // crossover, at its default
      r.set(267, pos);            // B146 placement
      r.set(57, drive ? 1 : 0);   // FX slot 1 type: Drive or Off
      r.set(58, 0.9);             // …driven hard, so the nonlinearity is real
      r.set(133, 1);              // slot 1 fully wet
      r.render(2);                // let the param writes settle before the note
      r.noteOn(24);
      r.render(24);
      L = r.L; R = r.R;
    };
    auto worst = [](const std::vector<float> &a, const std::vector<float> &b) {
      double m = 0;
      for (size_t i = 0; i < a.size() && i < b.size(); i++)
        m = std::fmax(m, std::fabs((double)a[i] - (double)b[i]));
      return m;
    };
    std::vector<float> preL, preR, postL, postR, bothL, bothR;
    std::vector<float> dpreL, dpreR, dpostL, dpostR, opreL, opreR, opostL, opostR;
    capture(0, true, false, preL, preR);      // A: pre,  rack bypassed
    capture(1, true, false, postL, postR);    // A: post, rack bypassed
    capture(2, true, false, bothL, bothR);    // B: both, rack bypassed  (control)
    capture(0, true, true, dpreL, dpreR);     // C: pre,  Drive between
    capture(1, true, true, dpostL, dpostR);   // C: post, Drive between
    capture(0, false, true, opreL, opreR);    // D: stage off, pre       (control)
    capture(1, false, true, opostL, opostR);  // D: stage off, post      (control)

    // The anchor: every comparison below is vacuous on silence.
    double energy = 0;
    double side = 0;   // and vacuous again if the render is MONO to begin with
    for (size_t i = 0; i < preL.size(); i++)
    {
      energy += (double)preL[i] * preL[i] + (double)preR[i] * preR[i];
      side = std::fmax(side, std::fabs((double)preL[i] - (double)preR[i]));
    }
    const double bypassDiff = std::fmax(worst(preL, postL), worst(preR, postR));
    const double bothDiff = std::fmax(worst(preL, bothL), worst(preR, bothR));
    const double driveDiff = std::fmax(worst(dpreL, dpostL), worst(dpreR, dpostR));
    const double offDiff = std::fmax(worst(opreL, opostL), worst(opreR, opostR));
    std::snprintf(d, sizeof(d),
                  "energy %.4g, peak L-R %.4g; bypassed pre vs post %.3g (must be 0); "
                  "control both vs pre %.3g (must be > 0); Drive pre vs post %.3g "
                  "(must be > 0); control stage-off pre vs post %.3g (must be 0)",
                  energy, side, bypassDiff, bothDiff, driveDiff, offDiff);
    check(energy > 1e-3 && side > 1e-4 && bypassDiff == 0.0 && bothDiff > 1e-6
              && driveDiff > 1e-6 && offDiff == 0.0,
          "bassMonoPos: pre == post with the rack bypassed, != with Drive between", d);
  }

  /* ---- B23 INCREMENT 3: THE SECOND SOURCE (assertions 23-24) --------------
     The shell now hands the matrix one buffer PER OSCILLATOR instead of the
     summed bus. Both assertions are asked of the shipped audio path, because
     the core has been generic in NSRC since day one and every core-level
     assertion above was green while the shell still summed — the exact
     coverage gap L0031 describes. */

  /* ---- 23. two sources feeding DIFFERENT slots keep their signals apart ---
     osc 1 -> slot 1 only, osc 2 -> slot 3 only, and slot 1 is DRIVE at 0.9.
     The nonlinearity is the whole measurement: if osc 2's samples reached slot
     1's gather, the slot would compute f(a+b) where the topology says f(a), and
     f(a+b) - f(a) - f(b) + f(0) is large for a saturator and identically zero
     for any linear slot. A linear slot would pass this test whether the sources
     were split or summed, which is why one is not used.

     FOUR RENDERS, not two. f(0) need not be 0 (a shaper may carry DC), so the
     superposition residual is taken as
         both - only1 - only2 + neither
     which cancels the constant path exactly. `neither` doubles as the anchor:
     every clause here is vacuous on silence.

     MUST-FAIL CONTROL, the same four renders with one cell changed: open
     osc 2 -> slot 1 as well, and the residual must go large. Without it
     "the residual is small" is also what a probe measuring nothing reports. */
  {
    auto capture = [&](bool o1, bool o2, double leak,
                       std::vector<float> &L, std::vector<float> &R) {
      Rig r(factory);
      r.set(150, o1 ? 1 : 0);        // osc 1 on/off
      r.set(1150, o2 ? 1 : 0);       // osc 2 on/off (the +1000 twin)
      r.set(1017, 0.4);              // osc 2 ships silent — give it a voice
      r.set(1036, 7);                // …a fifth up, so the two tones are distinct
      r.set(57, 1); r.set(58, 0.9); r.set(133, 1);   // slot 1 = Drive, hard, fully wet
      // The topology: src1 -> slot 1 -> OUT and src2 -> slot 3 -> OUT, with the
      // serial chain between the slots cut so the two paths never meet.
      r.set(coeffId(0, 0), 1.0);                     // osc 1 -> slot 1
      r.set(coeffId(1, 0), leak);                    // osc 2 -> slot 1 (0, or the control)
      r.set(coeffId(1, 2), 1.0);                     // osc 2 -> slot 3
      r.set(coeffId(kSlotRow0 + 0, 1), 0.0);         // slot 1 -> slot 2, cut
      r.set(coeffId(kSlotRow0 + 1, 2), 0.0);         // slot 2 -> slot 3, cut
      r.set(coeffId(kSlotRow0 + 2, 3), 0.0);         // slot 3 -> slot 4, cut
      r.set(20000 + 0, 1.0);                         // slot 1 out
      r.set(20000 + 2, 1.0);                         // slot 3 out
      r.set(20000 + 3, 0.0);                         // slot 4 out, off
      r.render(2);
      r.noteOn(36);
      r.render(24);
      L = r.L; R = r.R;
    };
    auto residual = [&](double leak) {
      std::vector<float> bL, bR, o1L, o1R, o2L, o2R, nL, nR;
      capture(true, true, leak, bL, bR);
      capture(true, false, leak, o1L, o1R);
      capture(false, true, leak, o2L, o2R);
      capture(false, false, leak, nL, nR);
      double m = 0, e = 0;
      for (size_t i = 0; i < bL.size(); i++)
      {
        m = std::fmax(m, std::fabs((double)bL[i] - o1L[i] - o2L[i] + nL[i]));
        m = std::fmax(m, std::fabs((double)bR[i] - o1R[i] - o2R[i] + nR[i]));
        e += (double)o2L[i] * o2L[i] + (double)o2R[i] * o2R[i];
      }
      return std::pair<double, double>{m, e};
    };
    const auto split = residual(0.0);
    const auto leaked = residual(1.0);
    std::snprintf(d, sizeof(d),
                  "osc2-alone energy %.4g (anchor); separate paths residual %.3g "
                  "(must be ~0); control (osc 2 ALSO into the drive) %.3g (must be > 0)",
                  split.second, split.first, leaked.first);
    check(split.second > 1e-3 && split.first < 1e-6 && leaked.first > 1e-3,
          "two sources feeding different slots keep their signals separate", d);
  }

  /* ---- 24. the default topology renders the OLD SUMMED BUS ----------------
     B23 (c): every existing patch must render bit-identically up to float
     summation order. With the rack at its default (every slot Off, so each is a
     bit-exact passthrough) the output IS slot 1's gather, and the claim is
     exact, not approximate: `0.0f + 1.0*osc1 + 1.0*osc2` rounds once per add,
     which is what `outL[i] += tL[i]` did before increment 3. Asked as
     `both[i] == (float)(only1[i] + only2[i])` for every sample — EQUALITY, no
     tolerance, because a tolerance here would hide exactly the re-ordering the
     acceptance criterion is about.
     MUST-FAIL CONTROL: osc 2's crosspoint at 0.5 in the COMBINED render only —
     the parts stay at unity, so the sum they predict is the unity sum and the
     equality must break. The first draft moved the gain in all three renders
     and the control did not fire: at 0.5 everywhere, `both` and `only2` scale
     together and the identity still holds exactly. Recorded rather than quietly
     re-rolled (L0033) — the degenerate form measured the crosspoint's linearity,
     which is assertion 8's job, not the summation order. */
  {
    auto capture = [&](bool o1, bool o2, double srcGain,
                       std::vector<float> &L, std::vector<float> &R) {
      Rig r(factory);
      r.set(150, o1 ? 1 : 0);
      r.set(1150, o2 ? 1 : 0);
      r.set(1017, 0.4);
      r.set(1036, 7);
      r.set(coeffId(1, 0), srcGain);   // osc 2 -> slot 1: 1.0 default, 0.5 control
      r.render(2);
      r.noteOn(36);
      r.render(24);
      L = r.L; R = r.R;
    };
    std::vector<float> o1L, o1R, o2L, o2R;
    capture(true, false, 1.0, o1L, o1R);      // the parts, both at unity
    capture(false, true, 1.0, o2L, o2R);
    auto mismatches = [&](double srcGain) {
      std::vector<float> bL, bR;
      capture(true, true, srcGain, bL, bR);
      int bad = 0;
      for (size_t i = 0; i < bL.size(); i++)
      {
        if (bL[i] != (float)((double)o1L[i] + (double)o2L[i])) bad++;
        if (bR[i] != (float)((double)o1R[i] + (double)o2R[i])) bad++;
      }
      return bad;
    };
    const int sum = mismatches(1.0);
    const int ctl = mismatches(0.5);
    double e = 0;
    for (size_t i = 0; i < o2L.size(); i++) e += (double)o2L[i] * o2L[i] + (double)o2R[i] * o2R[i];
    std::snprintf(d, sizeof(d),
                  "osc2-alone energy %.4g (anchor); default: %d of %d samples differ from "
                  "the float sum (must be 0); control (osc 2 crosspoint 0.5) %d (must be > 0)",
                  e, sum, 2 * Rig::kBlk, ctl);
    check(e > 1e-3 && sum == 0 && ctl > 0,
          "the two-source default renders the old summed bus, sample for sample", d);
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
