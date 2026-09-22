/* trace_check — the forensic note trace (FOUNDATIONS brief ask (c)).
 *
 * WHY THIS EXISTS AT ALL. The stuck-note bug survived weeks because it could
 * not be REPRODUCED, and no generator was ever going to reproduce it: a fuzzer
 * emits the event stream it IMAGINES, and ours deliberately excludes shapes no
 * host can produce (notefuzz_check.cpp:14-17). It can therefore never model a
 * stream the host actually delivered. Capture beats simulate — a field report
 * becomes a replayable regression case instead of an anecdote.
 *
 * WHY IT DRIVES THE REAL PLUGIN. The dump is triggered through the same code
 * path the GUI panic button uses, via a test hook, rather than by rebuilding
 * the ring in the test. A test that reimplements the mechanism it checks spans
 * the wrong layer and would pass against a capture path that was never wired
 * (L0031-B3: a fidelity result about the wrong surface is not evidence).
 *
 * EVERY ASSERTION HAS A MUST-READ-NOTHING CONTROL (L0032). A dump is a text
 * file full of plausible-looking lines; "the key is in the file" is satisfied
 * by a file that mentions every key. So each positive assertion is paired with
 * a negative one that must NOT match, on a dump taken from a state where the
 * thing genuinely is absent.
 * WIRED: ./verify full.
 */
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <clap/clap.h>

#include "../src/hypersaw_clap_entry.h"

static const clap_host_t kHost = {CLAP_VERSION_INIT, nullptr, "trace_check", "-", "-", "1.0",
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

static int failures = 0;
static void check(bool ok, const char *what, const char *detail)
{
  std::printf("%-6s %s  (%s)\n", ok ? "OK" : "FAIL", what, detail);
  if (!ok) failures++;
}

static std::string slurp(const char *path)
{
  if (!path) return {};
  std::FILE *f = std::fopen(path, "rb");
  if (!f) return {};
  std::string s;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
  std::fclose(f);
  return s;
}

// Count occurrences of `needle`, so "present" can be distinguished from
// "present the right number of times" — a ring that never wraps and a ring that
// wraps wrongly both contain the key.
static int countOf(const std::string &hay, const std::string &needle)
{
  int n = 0;
  for (size_t i = hay.find(needle); i != std::string::npos; i = hay.find(needle, i + 1)) n++;
  return n;
}

int main()
{
  const double SR = 44100.0;
  const int BLK = 512;
  hypersaw_entry_init("");
  auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_t *p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
  p->init(p);
  p->activate(p, SR, 32, 2048);
  p->start_processing(p);

  std::vector<float> L(BLK), R(BLK);
  float *chans[2] = {L.data(), R.data()};
  clap_audio_buffer_t out{};
  out.data32 = chans;
  out.channel_count = 2;
  EvList evl;
  evl.in.ctx = &evl;
  evl.in.size = evSize;
  evl.in.get = evGet;
  clap_output_events_t outEv{nullptr, outPush};

  // Notes are queued into a stable store: the event list holds raw pointers, so
  // a reallocating vector would dangle mid-block (the mixer_check trap).
  std::vector<clap_event_note_t> store;
  store.reserve(4096);
  int64_t blk = 0;

  auto note = [&](uint16_t type, int key, int noteId) {
    clap_event_note_t n{};
    n.header.size = sizeof(n);
    n.header.type = type;
    n.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    n.header.time = 0;
    n.note_id = noteId;
    n.port_index = 0;
    n.channel = 0;
    n.key = key;
    n.velocity = 0.8;
    store.push_back(n);
    evl.ev.push_back(&store.back().header);
  };
  auto run = [&](int blocks) {
    for (int b = 0; b < blocks; b++)
    {
      clap_process_t proc{};
      proc.audio_inputs_count = 0;
      proc.audio_outputs_count = 1;
      proc.audio_outputs = &out;
      proc.frames_count = BLK;
      proc.in_events = &evl.in;
      proc.out_events = &outEv;
      // 0 every block: exactly what the field host reports, and what broke the
      // first dump's timeline. A harness that supplies a tidy counter here tests
      // an environment nobody ships into.
      proc.steady_time = 0;
      blk++;
      p->process(p, &proc);
      evl.ev.clear();
    }
  };
  auto dump = [&](const char *why) { return slurp(hypersaw_test_dump_forensics(p, why)); };

  char d[256];

  // ---- 1. THE CONTROL: nothing played, nothing recorded --------------------
  // Runs FIRST, on a virgin plugin. If this dump already contains note rows or
  // a gated voice, every later assertion is meaningless — the file would be
  // reporting something other than what was played.
  {
    run(2);
    const std::string t = dump("control-empty");
    const int rows = countOf(t, "  pos ");
    const int gated = countOf(t, "GATED");
    std::snprintf(d, sizeof(d), "%d event rows, %d gated voices, %zu bytes", rows, gated, t.size());
    check(!t.empty() && rows == 0 && gated == 0,
          "a dump with nothing played records no events and no gated voice", d);
  }

  // ---- 2. the exact stream played comes back, in order ---------------------
  {
    const int keys[4] = {60, 64, 67, 72};
    for (int i = 0; i < 4; i++) { note(CLAP_EVENT_NOTE_ON, keys[i], 100 + i); run(1); }
    for (int i = 0; i < 4; i++) { note(CLAP_EVENT_NOTE_OFF, keys[i], 100 + i); run(1); }
    const std::string t = dump("stream");

    bool ordered = true;
    size_t at = 0;
    for (int i = 0; i < 4 && ordered; i++)
    {
      const std::string want = "ON    key  " + std::to_string(keys[i]);
      const size_t f = t.find(want, at);
      if (f == std::string::npos) ordered = false;
      else at = f;
    }
    const int ons = countOf(t, "ON    key"), offs = countOf(t, "OFF   key");
    // note_id is carried through verbatim — the field a replay needs most, and
    // the one most easily dropped by a struct copy that looks complete.
    const bool ids = countOf(t, "note_id   103") == 2;
    std::snprintf(d, sizeof(d), "%d ON, %d OFF, in-order=%s, note_id 103 seen %d times",
                  ons, offs, ordered ? "yes" : "no", countOf(t, "note_id   103"));
    check(ons == 4 && offs == 4 && ordered && ids,
          "every note event is captured, in order, with its note_id", d);
  }

  // ---- 3. a gated voice is VISIBLE, and its absence is too -----------------
  // This is the view that would have shown the stuck-note orphan at a glance.
  {
    note(CLAP_EVENT_NOTE_ON, 55, 200);
    run(4);
    const std::string held = dump("held");
    const int gatedWhileHeld = countOf(held, "GATED");

    note(CLAP_EVENT_NOTE_OFF, 55, 200);
    run(120);   // well past the release tail
    const std::string released = dump("released");
    const int gatedAfterRelease = countOf(released, "GATED");

    std::snprintf(d, sizeof(d), "gated while held %d, gated after release %d",
                  gatedWhileHeld, gatedAfterRelease);
    check(gatedWhileHeld > 0 && gatedAfterRelease == 0,
          "the voice table shows a gated voice while held and none after release", d);
  }

  // ---- 4. the ring WRAPS: newest kept, oldest dropped ----------------------
  // Without this, a ring that silently never wrapped — or one that wrapped and
  // kept the wrong half — would pass every assertion above.
  {
    const int N = 900;   // > kTraceLen (512)
    for (int i = 0; i < N; i++)
    {
      note(CLAP_EVENT_NOTE_ON, 36 + (i % 24), 1000 + i);
      note(CLAP_EVENT_NOTE_OFF, 36 + (i % 24), 1000 + i);
      if ((i % 8) == 7) run(1);
    }
    run(1);
    const std::string t = dump("wrap");
    const int rows = countOf(t, "  pos ");
    const bool newestKept = t.find("note_id  1899") != std::string::npos;
    const bool oldestDropped = t.find("note_id  1000") == std::string::npos;
    std::snprintf(d, sizeof(d), "%d rows (cap 512), newest note_id 1899 kept=%s, "
                                "oldest note_id 1000 dropped=%s",
                  rows, newestKept ? "yes" : "no", oldestDropped ? "yes" : "no");
    check(rows == 512 && newestKept && oldestDropped,
          "the ring keeps the most recent 512 events and drops the rest", d);
  }

  // ---- 4b. the timeline ADVANCES ACROSS BLOCKS ----------------------------
  /* The first real field dump had every pos under 512 and non-monotonic: the
     host reported steady_time as 0 every block, so pos was only the in-block
     offset and events from different blocks interleaved. The column a replay
     most depends on was the one that was broken, in the only environment that
     counts — and nothing here noticed, because this harness supplies a tidy
     increasing steady_time that the field does not. So: drive it the way the
     field does. */
  {
    blk = 0;   // steady_time will be forced to 0 below, mimicking the wrapper
    note(CLAP_EVENT_NOTE_ON, 40, 900);
    run(1);
    for (int i = 0; i < 6; i++) { note(CLAP_EVENT_NOTE_OFF, 40, 900); note(CLAP_EVENT_NOTE_ON, 40, 900); run(1); }
    const std::string t = dump("timeline");
    // Parse the pos column and require it to be non-decreasing AND to exceed
    // one block — either alone is satisfiable by the broken version.
    std::vector<unsigned long long> pos;
    for (size_t i = t.find("  pos "); i != std::string::npos; i = t.find("  pos ", i + 1))
      pos.push_back(std::strtoull(t.c_str() + i + 6, nullptr, 10));
    bool monotonic = true;
    for (size_t i = 1; i < pos.size(); i++) if (pos[i] < pos[i - 1]) monotonic = false;
    const unsigned long long span = pos.empty() ? 0 : pos.back() - pos.front();
    std::snprintf(d, sizeof(d), "%zu events, monotonic=%s, span %llu samples (>1 block = %d)",
                  pos.size(), monotonic ? "yes" : "no", span, BLK);
    check(pos.size() > 6 && monotonic && span > (unsigned long long)BLK,
          "the trace timeline advances across blocks, not just within one", d);
  }

  // ---- 4c. the host-MPE hint fires only on the flattened signature ---------
  /* Live gates MPE per device; with it off an expressive stream arrives
     flattened and sustained notes become blips. That cost two multi-round
     investigations, both ended by the human rather than by any gate. We cannot
     set the toggle; we can notice its absence. Three controls, because a
     warning that is usually wrong is worse than none: it must stay SILENT below
     the evidence threshold, silent when note expressions arrive, and silent when
     any note uses a member channel. */
  {
    auto fresh = [&]() { p->reset(p); run(2); };
    auto play = [&](int n, int channel, bool withExpr) {
      for (int i = 0; i < n; i++)
      {
        clap_event_note_t nt{};
        nt.header.size = sizeof(nt); nt.header.type = CLAP_EVENT_NOTE_ON;
        nt.header.space_id = CLAP_CORE_EVENT_SPACE_ID; nt.header.time = 0;
        nt.note_id = -1; nt.port_index = 0; nt.channel = (int16_t)channel;
        nt.key = (int16_t)(60 + (i % 5)); nt.velocity = 0.8;
        store.push_back(nt); evl.ev.push_back(&store.back().header);
        if (withExpr)
        {
          static std::vector<clap_event_note_expression_t> ex; ex.reserve(256);
          clap_event_note_expression_t e{};
          e.header.size = sizeof(e); e.header.type = CLAP_EVENT_NOTE_EXPRESSION;
          e.header.space_id = CLAP_CORE_EVENT_SPACE_ID; e.header.time = 0;
          e.expression_id = CLAP_NOTE_EXPRESSION_TUNING;
          e.note_id = -1; e.port_index = 0; e.channel = (int16_t)channel;
          e.key = (int16_t)(60 + (i % 5)); e.value = 0.0;
          ex.push_back(e); evl.ev.push_back(&ex.back().header);
        }
        run(1);
      }
    };
    fresh(); play(8, 0, false);
    const std::string few = hypersaw_test_host_hint(p);
    fresh(); play(30, 0, false);
    const std::string flat = hypersaw_test_host_hint(p);
    fresh(); play(30, 0, true);
    const std::string withE = hypersaw_test_host_hint(p);
    fresh(); play(30, 3, false);
    const std::string memberCh = hypersaw_test_host_hint(p);

    std::snprintf(d, sizeof(d), "8 notes:%s | 30 flat:%s | 30 with expr:%s | 30 on ch3:%s",
                  few.empty() ? "silent" : "FIRES", flat.empty() ? "SILENT" : "fires",
                  withE.empty() ? "silent" : "FIRES", memberCh.empty() ? "silent" : "FIRES");
    check(few.empty() && !flat.empty() && withE.empty() && memberCh.empty(),
          "the MPE-off hint fires ONLY on the flattened signature", d);

    // Leave a clean slate: this block gates 30 notes, and without clearing them
    // their tails inflate assertion 5's post-panic measurement. A test that
    // silently changes the next test's starting state is a shared-fixture bug.
    hypersaw_test_panic(p);
    run(8);
  }

  // ---- 5. panic CAPTURES BEFORE IT CLEARS ---------------------------------
  /* The ordering is the whole feature: a dump taken after the clear records a
     synth in perfect health. Previously untestable — it lived inline in the GUI
     lambda — and recorded here as a known boundary rather than pretended over.
     Now reachable, so the boundary is closed instead of documented.

     Both halves are asserted, because either alone is satisfiable by a bug:
     a dump showing the gated voice proves capture happened first, and silence
     afterwards proves the clear still happened at all. */
  {
    note(CLAP_EVENT_NOTE_ON, 48, 300);
    note(CLAP_EVENT_NOTE_ON, 52, 301);
    run(6);
    const std::string t = slurp(hypersaw_test_panic(p));
    const int gatedInDump = countOf(t, "GATED");

    /* Past the release tail, not immediately after. Panic RELEASES the voices
       (allOff), it does not hard-mute them — measured 0.287 two blocks later,
       which is a normal release, not a failed clear. Asserting instant silence
       would have been asserting a behaviour the synth does not have and should
       not: a panic that truncates the envelope would click. */
    run(120);
    double peak = 0;
    for (int i = 0; i < BLK; i++)
    {
      const double m = std::fabs((double)L[i]) > std::fabs((double)R[i])
                         ? std::fabs((double)L[i]) : std::fabs((double)R[i]);
      if (m > peak) peak = m;
    }
    std::snprintf(d, sizeof(d), "dump taken at panic shows %d gated voices; "
                                "post-panic peak %.2e", gatedInDump, peak);
    check(gatedInDump > 0 && peak < 1e-4,
          "panic captures the stuck state BEFORE clearing it", d);
  }

  /* CALIBRATION.
     FIRES — (A) dropping note_id in recordNote fails assertions 2 AND 4, since
     both read it back; (B) clamping the ring index instead of masking it
     (`w < kTraceLen ? w : kTraceLen-1`) fails ONLY assertion 4, reporting
     "oldest dropped=no", which is exactly what a clamp does.

     THE BUILD MUST BE FORCED. Plant B first reported A's failures verbatim: the
     impl object was stale, so it ran the previous binary. Delete the impl object
     (CMakeFiles/HYPERSAW-impl.dir/src/hypersaw_clap.cpp.o, under the build dir)
     between plants
     — asserting the plant's ANCHOR (which was done) proves the SOURCE changed,
     never that the BINARY did.

     BOUNDARY NOW CLOSED. This suite used to call dumpForensics() directly and
     never exercised panic's dump-before-clear ordering, which is the whole
     feature; that was recorded here as uncovered. Assertion 5 now drives the
     panic path itself (panicWithDump, extracted out of the GUI lambda for
     exactly this reason) and asserts BOTH halves — the dump sees the gated
     voice, and the synth is silent afterwards. Calibrated by swapping the two
     statements, which makes it FAIL at 0 gated voices. Recording a boundary is
     the honest move when it cannot be closed; it is not a substitute for
     closing it when it can. */
  p->stop_processing(p);
  p->deactivate(p);
  p->destroy(p);
  std::printf("trace_check: %s (%d failures)\n", failures ? "RED" : "GREEN", failures);
  return failures ? 1 : 0;
}
