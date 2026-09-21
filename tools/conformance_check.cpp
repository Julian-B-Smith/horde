/* conformance_check — FOUNDATIONS' note-lifecycle conformance suite, run
 * against OUR bookkeeping.  (Their R5 / DECISIONS #80; our ROADMAP entry
 * "FOUNDATIONS — fleet protocol acked; conformance run BLOCKED on repo
 * visibility".)
 *
 * WHY THIS FILE IS TRACKED AND THEIR HEADERS ARE NOT. HYPERSAW is a PUBLIC
 * repo; FOUNDATIONS is PRIVATE. Committing `note.h` / `note_conformance.h`
 * would publish a private sibling's source, so they are vendored to a
 * gitignored path (libs/vendor/foundations/, human ruling 2026-08-15) and this
 * whole target disappears when they are absent. The adapter below is ours and
 * carries all of the risk, so it is public. The consequence is stated rather
 * than hidden: public CI will never have their headers, so `./verify` reports
 * this gate as SKIPPED there — a locally-run measurement, not a CI block.
 *
 * WHAT IS UNDER TEST, AND WHAT IS DELIBERATELY NOT. The suite drives an adapter
 * over the consumer's own note bookkeeping. Ours is not a library type: identity
 * lives in the shell (`Plugin::tags[kPoly]`, hypersaw_clap.cpp) and the STEAL
 * POLICY lives in the engine (`swarm_core.h::alloc()`, ADR-083's three tiers),
 * because the tiers read envelope state the shell cannot see. So the adapter
 * spans both — and it must NOT bridge them by recomputing anything:
 *
 *   - notes arrive as REAL CLAP events through the REAL process() path;
 *   - a steal is observed as the NOTE_END the plugin emits to the host, which
 *     is precisely what a host sees, not a prediction of what it should emit;
 *   - `end()` calls the shipped `retireTag()`, the same function a steal and a
 *     mono retarget call.
 *
 * An adapter that computed "who should have been stolen" would be an oracle
 * checking its own copy of the rule under test (L0031, and the shape L0032
 * names): it would agree with the engine by construction and certify nothing.
 *
 * CALIBRATION LIVES IN ./verify, NOT HERE. A suite that has never rejected
 * anything is not a gate (their words and ours). The planted-defect run is
 * recorded in the trace for this change.
 * WIRED: ./verify full.
 */
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <clap/clap.h>

#include "../src/hypersaw_clap_entry.h"
#include "foundations/note.h"
#include "foundations/note_conformance.h"

namespace {

constexpr double kSR = 44100.0;
constexpr uint32_t kBlock = 64;

const clap_host_t kHost = {CLAP_VERSION_INIT, nullptr, "conformance_check", "-", "-", "1.0",
                           [](const clap_host_t *, const char *) -> const void * { return nullptr; },
                           [](const clap_host_t *) {}, [](const clap_host_t *) {},
                           [](const clap_host_t *) {}};

// Every NOTE_END the plugin pushes out during a block lands here. A steal is
// read from this list, never inferred.
std::vector<clap_event_note_t> g_ends;

/* THE END LEDGER — our own oracle, not theirs, and the reason this file is
   worth running even on a red.
   Their R-end-1 asserts identities come back "through end() or the steal";
   that encodes THEIR table's timing. Ours retires an identity at gate-off
   instead (the 2026-07-31 END-at-release redesign, hypersaw_clap.cpp:905 —
   emitting at env death made hosts wait on an invisible ~1.1 s tail). So we
   can fail their ENCODING while still satisfying the RULING it exists to
   protect: "a note's identity may only be discarded through a path that
   delivers its END."
   This ledger measures that property directly and timing-independently: every
   identity issued must be ENDed exactly once, whenever that happens. A leak is
   the stuck note; a double is host bookkeeping corruption. */
struct Ident
{
  int32_t note_id;
  int16_t channel, key;
  bool operator==(const Ident &o) const
  {
    return note_id == o.note_id && channel == o.channel && key == o.key;
  }
};
std::vector<Ident> g_issued;    // note-ons, in order
std::vector<Ident> g_ended;     // NOTE_ENDs observed, in order

bool outPush(const clap_output_events_t *, const clap_event_header_t *h)
{
  if (h->type == CLAP_EVENT_NOTE_END && h->space_id == CLAP_CORE_EVENT_SPACE_ID)
  {
    const auto *n = reinterpret_cast<const clap_event_note_t *>(h);
    g_ends.push_back(*n);
    g_ended.push_back(Ident{n->note_id, (int16_t)n->channel, (int16_t)n->key});
  }
  return true;
}

int countOf(const std::vector<Ident> &v, const Ident &x)
{
  int n = 0;
  for (const auto &e : v) if (e == x) n++;
  return n;
}

struct EvList
{
  clap_input_events_t list{};
  std::vector<clap_event_note_t> store;
  std::vector<clap_event_param_value_t> params;
  std::vector<const clap_event_header_t *> ptrs;
};
uint32_t evSize(const clap_input_events_t *l) { return (uint32_t)((EvList *)l->ctx)->ptrs.size(); }
const clap_event_header_t *evGet(const clap_input_events_t *l, uint32_t i)
{
  return ((EvList *)l->ctx)->ptrs[i];
}

/* The adapter. `Handle` is (slot, identity): a slot alone cannot distinguish two
   same-key instances that occupied the same slot in turn, and identity alone
   cannot find the row to retire — the same-key retrigger cases need both. */
struct ShellAdapter
{
  static constexpr std::size_t kMaxNotes = 16;   // hypersaw::kPoly, asserted below
  /* CARRY `port`. Their RefBag::take() matches on all four fields, so an
     identity returned with the wrong port is silently "not the note we issued".
     The first version reconstructed port from a tag read taken AFTER the
     note-off, by which point the slot may hold someone else — the returned
     identity then failed to match and R-end-1 went red about nothing. Our own
     ledger ignores port, so it stayed green: two oracles disagreeing because
     one of them was reading a field the other did not. */
  struct Handle
  {
    int slot = -1;
    int32_t note_id = -1;
    int16_t port = -1, channel = -1, key = -1;
  };
  struct OnResult
  {
    Handle handle;
    bool stole = false;
    foundations::NoteRef stolen{};
  };

  const clap_plugin_t *p = nullptr;
  std::vector<float> L, R;
  clap_audio_buffer_t out{};
  float *chans[2]{};
  clap_output_events_t outEv{nullptr, outPush};
  int64_t blk = 0;

  explicit ShellAdapter(const clap_plugin_t *plug) : p(plug), L(kBlock), R(kBlock)
  {
    chans[0] = L.data();
    chans[1] = R.data();
    out.data32 = chans;
    out.channel_count = 2;
  }

  void run(EvList &e)
  {
    e.list.ctx = &e;
    e.list.size = evSize;
    e.list.get = evGet;
    e.ptrs.clear();
    for (auto &n : e.store) e.ptrs.push_back(&n.header);
    for (auto &q : e.params) e.ptrs.push_back(&q.header);
    g_ends.clear();
    clap_process_t proc{};
    proc.audio_inputs_count = 0;
    proc.audio_outputs_count = 1;
    proc.audio_outputs = &out;
    proc.frames_count = kBlock;
    proc.in_events = &e.list;
    proc.out_events = &outEv;
    proc.steady_time = blk++ * kBlock;
    p->process(p, &proc);
  }

  /* Short envelopes, exactly as steal_check does. Their model has no TIME: end()
     frees the row instantly. Ours cannot — a released note keeps its slot until
     the envelope decays, which is what a release stage IS. At default release a
     tail holds a slot for ~1.1 s, so their Case 2 fill (which asserts it never
     steals) lands on voices the previous case only just released. Shortening the
     release makes "gated" and "gone" unambiguous; it configures the instrument
     for the test rather than changing what is measured. */
  void setParam(uint32_t id, double v)
  {
    EvList e;
    clap_event_param_value_t ev{};
    ev.header.size = sizeof(ev);
    ev.header.type = CLAP_EVENT_PARAM_VALUE;
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.time = 0;
    ev.param_id = id;
    ev.value = v;
    ev.note_id = -1;
    ev.port_index = -1;
    ev.channel = -1;
    ev.key = -1;
    e.params.push_back(ev);
    run(e);
  }

  /* quiesce() — their optional between-case hook (R-fill ruling, 2026-08-15).
     We offer it because we are exactly the consumer it was added for: our voices
     outlive their identities, so without it a case is measured against tails the
     previous case left ringing.
     It drains TIME ONLY. It deliberately does NOT force notes off: a note still
     gated between cases is one the suite has not released, and silencing it
     behind the suite's back would corrupt the very state the hook exists to
     clean. Quiescing means "let outstanding tails finish", never "panic". */
  void quiesce() { drain(64); }

  // Let released voices actually finish, so a freed slot is free. Bounded: a
  // drain that could spin forever would turn a lifecycle bug into a hang.
  void drain(int blocks)
  {
    for (int i = 0; i < blocks; i++) { EvList e; run(e); }
  }

  void send(uint16_t type, const foundations::NoteRef &n)
  {
    if (type == CLAP_EVENT_NOTE_ON)
      g_issued.push_back(Ident{n.note_id, n.channel, n.key});
    EvList e;
    clap_event_note_t ev{};
    ev.header.size = sizeof(ev);
    ev.header.type = type;
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.time = 0;
    ev.note_id = n.note_id;
    ev.port_index = n.port < 0 ? 0 : n.port;
    ev.channel = n.channel < 0 ? 0 : n.channel;
    ev.key = n.key;
    ev.velocity = 0.8;
    e.store.push_back(ev);
    run(e);
  }

  // Which slot currently holds this exact identity? -1 if none.
  int slotOfIdentity(const foundations::NoteRef &n) const
  {
    for (int s = 0; s < (int)kMaxNotes; s++)
    {
      int32_t id;
      int16_t port, ch, key;
      if (!hypersaw_test_tag_at(p, s, &id, &port, &ch, &key)) continue;
      if (id == n.note_id && ch == n.channel && key == n.key) return s;
    }
    return -1;
  }

  /* A red here is ambiguous until you know whether the SHELL or the ADAPTER
     produced it, and that question has cost this project real time before. Set
     HYPERSAW_CONFORMANCE_DEBUG=1 to print every adapter operation with the slot
     and the ENDs the plugin actually emitted, so the two can be told apart. */
  static bool debug()
  {
    static const bool on = std::getenv("HYPERSAW_CONFORMANCE_DEBUG") != nullptr;
    return on;
  }

  OnResult noteOn(const foundations::NoteRef &n)
  {
    send(CLAP_EVENT_NOTE_ON, n);
    OnResult r;
    // A steal announces itself as a NOTE_END for some OTHER identity, emitted
    // in the same block. Read, not predicted.
    for (const auto &e : g_ends)
    {
      if (e.note_id == n.note_id && e.channel == n.channel && e.key == n.key) continue;
      r.stole = true;
      r.stolen = foundations::NoteRef{e.note_id, (int16_t)e.port_index, (int16_t)e.channel,
                                      (int16_t)e.key};
      break;
    }
    const int slot = slotOfIdentity(n);
    r.handle = Handle{slot, n.note_id, n.port < 0 ? (int16_t)0 : n.port, n.channel, n.key};
    if (debug())
    {
      std::printf("[dbg] noteOn  key %3d id %3d -> slot %2d | ends emitted: %zu",
                  n.key, n.note_id, slot, g_ends.size());
      for (const auto &e : g_ends) std::printf("  END(key %d id %d)", e.key, e.note_id);
      std::printf("%s\n", r.stole ? "  => STOLE" : "");
    }
    return r;
  }

  /* Wildcard release. The count is the number of voices that actually stopped
     being gated because of this note-off — measured across the event, not
     computed from a copy of the matching rule. A shell that matched the wrong
     rows would report the wrong count here, which is the point. */
  std::size_t release(const foundations::NoteRef &q)
  {
    bool before[kMaxNotes];
    for (int s = 0; s < (int)kMaxNotes; s++) before[s] = hypersaw_test_slot_gated(p, s);
    send(CLAP_EVENT_NOTE_OFF, q);
    std::size_t n = 0;
    for (int s = 0; s < (int)kMaxNotes; s++)
      if (before[s] && !hypersaw_test_slot_gated(p, s)) n++;
    if (debug()) std::printf("[dbg] release key %3d ch %d id %3d -> %zu voice(s) ungated\n",
                             q.key, q.channel, q.note_id, n);
    return n;
  }

  foundations::NoteRef end(Handle h)
  {
    const foundations::NoteRef none{-1, -1, -1, -1};
    if (h.slot < 0) return none;
    // Retire only if the slot STILL holds this identity: a slot reused by a
    // later note must not be retired by a stale handle, and a second end() on a
    // retired handle must yield nothing (their no-double-END case).
    int32_t id;
    int16_t port, ch, key;
    const bool active = hypersaw_test_tag_at(p, h.slot, &id, &port, &ch, &key);
    const bool mine = active && id == h.note_id && ch == h.channel && key == h.key;
    if (debug())
      std::printf("[dbg] end    slot %2d want id %3d key %3d | slot holds %s id %3d key %3d -> %s\n",
                  h.slot, h.note_id, h.key, active ? "ACTIVE" : "empty ", id, key,
                  mine ? "retire" : "NOTHING");
    if (!mine) return none;
    /* GATE THE VOICE OFF FIRST, and this is not a detail. Retiring a tag while
       its voice is still GATED creates a sounding voice with no identity — an
       ORPHAN, which is exactly the mono-poison condition retireTag exists to
       prevent. The shell itself never reaches that state: it calls retireTag
       only immediately before overwriting the tag with a new note. Calling it
       bare from a test hook would manufacture a state the product cannot
       produce, and then measure it. So `end()` retires the identity AND ends
       the note, which is what their model means by end(). */
    const foundations::NoteRef self{h.note_id, h.port, h.channel, h.key};
    send(CLAP_EVENT_NOTE_OFF, self);
    drain(24);   // ~35 ms at 64-sample blocks; release is set to 5 ms below
    // The note-off may itself have caused the shell to emit the END and clear
    // the tag (we end at gate-off, hypersaw_clap.cpp:905). If so, the identity
    // has already left through the shell's own path — report it, do not retire
    // a second time.
    if (!hypersaw_test_tag_at(p, h.slot, &id, &port, &ch, &key) || id != h.note_id ||
        ch != h.channel || key != h.key)
      return foundations::NoteRef{h.note_id, h.port, h.channel, h.key};
    int32_t rid;
    int16_t rport, rch, rkey;
    if (!hypersaw_test_retire_slot(p, h.slot, &rid, &rport, &rch, &rkey)) return none;
    return foundations::NoteRef{rid, rport, rch, rkey};
  }

  bool live(Handle h) const
  {
    if (debug()) std::printf("[dbg] live?  slot %2d id %3d key %3d -> ", h.slot, h.note_id, h.key);
    if (h.slot < 0) { if (debug()) std::printf("no (slot -1)\n"); return false; }
    int32_t id;
    int16_t port, ch, key;
    const bool act = hypersaw_test_tag_at(p, h.slot, &id, &port, &ch, &key);
    const bool r = act && id == h.note_id && ch == h.channel && key == h.key;
    if (debug())
      std::printf("%s (slot %s, holds id %d key %d)\n", r ? "YES" : "no",
                  act ? "ACTIVE" : "empty", id, key);
    return r;
  }
};

}  // namespace

int main()
{
  hypersaw_entry_init("");
  auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_t *p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
  p->init(p);
  p->activate(p, kSR, 32, 2048);
  p->start_processing(p);

  if (hypersaw_test_poly() != (int)ShellAdapter::kMaxNotes)
  {
    std::printf("FAIL  adapter kMaxNotes %zu != shell kPoly %d — fix the adapter, not this check\n",
                ShellAdapter::kMaxNotes, hypersaw_test_poly());
    return 1;
  }

  ShellAdapter a(p);
  // Same configuration steal_check uses, and for the same reason: a short
  // release makes "released" and "gone" distinguishable within a case.
  a.setParam(19, 0.001);   // attack
  a.setParam(20, 0.001);   // decay
  a.setParam(21, 1.0);     // sustain full
  a.setParam(22, 0.005);   // release short
  a.drain(8);

  const foundations::ConformanceReport rep = foundations::runNoteConformance(a);

  /* Drain before judging the ledger. retireTag() QUEUES an identity; the END
     goes out on a following block, so reading the ledger the instant the suite
     returns would count every still-queued END as a leak — a residue measured
     beside the event that produces it is not a residue (L0016). */
  for (int i = 0; i < 400; i++)
  {
    EvList e;
    a.run(e);
  }

  int leaked = 0, doubled = 0;
  std::vector<Ident> seen;
  for (const auto &id : g_issued)
  {
    if (countOf(seen, id)) continue;   // multiplicity handled once per identity
    seen.push_back(id);
    const int on = countOf(g_issued, id), off = countOf(g_ended, id);
    if (off < on)
    {
      leaked += on - off;
      std::printf("FAIL   LEDGER leak: id %d ch %d key %d issued %d, ENDed %d\n", id.note_id,
                  id.channel, id.key, on, off);
    }
    else if (off > on)
    {
      doubled += off - on;
      std::printf("FAIL   LEDGER double-END: id %d ch %d key %d issued %d, ENDed %d\n", id.note_id,
                  id.channel, id.key, on, off);
    }
  }
  const bool ledgerOk = leaked == 0 && doubled == 0;
  std::printf("%-6s LEDGER (ours, timing-independent): every identity issued was ENDed exactly "
              "once — %zu note-ons, %zu ENDs\n",
              ledgerOk ? "ok" : "FAIL", g_issued.size(), g_ended.size());

  p->stop_processing(p);
  p->deactivate(p);
  p->destroy(p);

  /* THE GATE PINS BOTH SETS, and pins them separately, because FOUNDATIONS'
     reclassification (their R8) split one verdict into two kinds:
       kRuled           — a decision every consumer must satisfy.
       kLibraryDefault  — how NoteTable happens to satisfy it; a consumer may
                          legitimately differ, reported as DIVERGE.
     Divergences are EXPECTED for us and are not failures: we retire an identity
     at gate-off, which they ruled CONFORMING (R8) — the rule constrains the
     PATH, not the MOMENT.
     The one ruled failure is pinned with its reason rather than tolerated
     silently, and it is filed with them: their Case 2 asserts the fill never
     steals, which presumes end() frees a slot instantly. A real voice holds its
     slot until its envelope decays, so with a full pool the fill MUST steal.
     Their own fixture cannot see it — a table has no envelope.
     Drift in either direction exits non-zero: a new failure is a regression, and
     a pinned one turning green means this record is stale. */
  /* NO expected ruled failures. R-end-1 became a pass when FOUNDATIONS removed
     the fill's no-steal assertion (their fill ruling, 2026-08-15) — it had been
     a PRECONDITION ("the pool starts empty") smuggled into a case about identity
     accounting. This array is deliberately left empty rather than deleted: the
     drift check below turns any new ruled failure into a red naming it. */
  static const char *const kExpectedRuledFail[] = {nullptr};
  /* R-end-1d dropped off this list when FOUNDATIONS fixed a handle-array
     truncation in Case 1 (their DECISIONS #88): a four-element array was losing
     every handle past the fourth, so twelve notes were issued and never ended.
     Our fill then had to steal them. With the leak gone our tailed shell finds a
     free row for every note — so voice residency was never the obstacle here,
     which is the opposite of what we concluded and worth remembering. */
  static const char *const kExpectedDiverge[] = {"R-steal-1d", "R-steal-2", "R-retrig-1d"};
  // 0 — the array holds only a nullptr placeholder (see above).
  constexpr int kNRuled = 0;
  constexpr int kNDiv = (int)(sizeof(kExpectedDiverge) / sizeof(kExpectedDiverge[0]));

  const auto listed = [](const char *name, const char *const *set, int n) {
    for (int i = 0; i < n; i++)
      if (!std::strncmp(name, set[i], std::strlen(set[i]))) return true;
    return false;
  };
  const auto present = [](const char *want, const char *const *got, int n) {
    for (int i = 0; i < n; i++)
      if (got[i] && !std::strncmp(got[i], want, std::strlen(want))) return true;
    return false;
  };

  int drift = 0;
  for (int i = 0; i < rep.failed && i < foundations::ConformanceReport::kMaxFail; i++)
    if (!listed(rep.failed_names[i], kExpectedRuledFail, kNRuled))
    { std::printf("FAIL   UNEXPECTED ruled failure (regression): %s\n", rep.failed_names[i]); drift++; }
  for (int i = 0; i < rep.diverged && i < foundations::ConformanceReport::kMaxFail; i++)
    if (!listed(rep.diverged_names[i], kExpectedDiverge, kNDiv))
    { std::printf("FAIL   UNEXPECTED divergence: %s\n", rep.diverged_names[i]); drift++; }
  for (int j = 0; j < kNRuled; j++)
    if (!present(kExpectedRuledFail[j], rep.failed_names, rep.failed))
    { std::printf("FAIL   %s no longer fails — update the pin and the ROADMAP entry\n",
                  kExpectedRuledFail[j]); drift++; }
  for (int j = 0; j < kNDiv; j++)
    if (!present(kExpectedDiverge[j], rep.diverged_names, rep.diverged))
    { std::printf("FAIL   %s no longer diverges — update the pin and the ROADMAP entry\n",
                  kExpectedDiverge[j]); drift++; }

  rep.summarize();
  std::printf("conformance_check: %s — %d passed, %d ruled failure(s) [%d pinned], "
              "%d divergence(s) [%d pinned], ledger %s\n",
              (drift || !ledgerOk) ? "RED" : "GREEN", rep.passed, rep.failed, kNRuled,
              rep.diverged, kNDiv, ledgerOk ? "GREEN" : "RED");
  return (drift || !ledgerOk) ? 1 : 0;
}
