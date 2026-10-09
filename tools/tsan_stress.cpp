/*
 * tsan_stress — multi-thread stress harness for parameters, presets and state
 * (ADR-197 risk row 2; B448 Wave B item B2). Built and run under
 * -fsanitize=thread by tools/tsan_stress_check.py, which carries the wiring
 * declaration; this file is the harness only.
 *
 * WHAT RUNS. One real plugin instance, created through the CLAP entry
 * (hypersaw_entry_get_factory), activated, then two threads:
 *   AUDIO  start_processing, then process() every block with seeded host
 *          events (notes, CLAP_EVENT_PARAM_VALUE over every enumerated param),
 *          and params_flush between some blocks. CLAP allows params_flush on
 *          the audio thread while processing; calling it from the main thread
 *          while processing would break the contract, and a report caused by
 *          a contract the harness broke would be the harness's, not the plugin's.
 *   MAIN   the host's main thread AND the editor's bridge, interleaved by a
 *          seeded schedule: (a) parameter writes, (b) state_load of the
 *          tests/state_fixtures chunks and the editor's apply-state of the
 *          factory presets, (c) state_save and the editor's state read,
 *          (d) morph capture / corner apply / morph toggle and mode writes,
 *          plus the editor's polling reads (params, mod-live, spectrum,
 *          history service), the editor's PANIC, the host's params
 *          value/value_to_text reads, and a burst past the editor queue's
 *          capacity.
 * ONE main thread, not one per category: in a CLAP host every [main-thread]
 * call and every editor callback (macOS webview messages) run on the same
 * thread, and the editor's param queue is single-producer. Two producer
 * threads would manufacture reports no host can produce.
 *
 * WHY A SINGLE TU THAT INCLUDES THE SHELL SOURCE, rather than linking
 * HYPERSAW-impl the way the other probes do:
 *   1. TSan only sees accesses in instrumented code, and HYPERSAW_SANITIZE
 *      instruments host-side executables only, never the impl library
 *      (CMakeLists.txt, the HYPERSAW_SANITIZE block). Linked, the shell
 *      would be invisible and the harness would read clean for that reason.
 *   2. The editor bridge is a set of lambdas local to gui_create, which needs
 *      a webview to reach. Inside this TU the harness calls the SAME Plugin
 *      members those lambdas call (see mainOp). Every editor lambda is a
 *      one-line call of a Plugin member now (modSetDepth, setModWheel and
 *      modRemoveRoute were inline bodies once and were mirrored here), so a
 *      new lambda with a body of its own must be given a member, not a mirror.
 * The GUI object (hypersaw_gui.mm) is linked uninstrumented only to satisfy
 * gui_create's symbols; no editor is ever created.
 *
 * DETERMINISM. Both schedules are mulberry32 streams (forcecore::rngNext)
 * from --seed, bounded by iteration counts, never wall time. Thread
 * interleaving is the OS's, so the ORDER of the two threads' operations is not
 * reproducible, but the operation sequence of each thread is. All file reads
 * happen before any thread starts.
 *
 * MODES.  --plant   no plugin: a deliberate race on a plain int that the MAIN
 *                   thread WRITES and a second thread reads. The check requires
 *                   TSan to report it, and its classifier to file it as a
 *                   main-thread WRITE (the gate's failing class).
 *         --plant-read  the mirror: the second thread writes, the MAIN thread
 *                   only reads. Must be reported AND filed as a READ, so the
 *                   classifier is proven both ways.
 *         default   audio + main, every category.
 *         --control audio thread only: host events and params_flush. Nothing
 *                   else touches the instance while it processes, so any
 *                   report here is a harness defect or an audio-thread-only
 *                   race, and is reported as such by the check.
 * Exit 0 when the run completes; TSan itself sets the exit code on a report.
 */

#include "../src/hypersaw_clap.cpp"

// Named here, not inherited from the shell source: include_check holds every
// file to its own headers (MSVC does not reach them transitively).
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>

namespace stress
{
constexpr double kSR = 44100.0;
constexpr uint32_t kBlock = 128;

struct Rng
{
  uint32_t s;
  double next() { return forcecore::rngNext(s); }
  uint32_t below(uint32_t n) { return n ? (uint32_t)(next() * n) % n : 0; }
};

std::string readFile(const std::string &path)
{
  std::string out;
  if (FILE *f = std::fopen(path.c_str(), "rb"))
  {
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
  }
  return out;
}

bool endsWith(const std::string &s, const char *suf)
{
  const size_t n = std::strlen(suf);
  return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}

/* Sorted so the corpus order, and therefore the schedule, does not depend on
   readdir's order. */
void collect(const std::string &dir, const char *suffix, std::vector<std::string> &out)
{
  DIR *d = opendir(dir.c_str());
  if (!d) return;
  std::vector<std::string> names;
  while (dirent *e = readdir(d))
    if (e->d_name[0] != '.') names.push_back(e->d_name);
  closedir(d);
  std::sort(names.begin(), names.end());
  for (const auto &n : names)
  {
    const std::string p = dir + "/" + n;
    struct stat st;
    if (stat(p.c_str(), &st) != 0) continue;
    if (S_ISDIR(st.st_mode)) collect(p, suffix, out);
    else if (endsWith(n, suffix)) out.push_back(readFile(p));
  }
}

// ---- CLAP host side ----
const void *hostGetExtension(const clap_host_t *, const char *) { return nullptr; }
void hostNoop(const clap_host_t *) {}
const clap_host_t kHost = {CLAP_VERSION, nullptr, "tsan_stress", "", "", "1.0",
                           hostGetExtension, hostNoop, hostNoop, hostNoop};
bool outPush(const clap_output_events_t *, const clap_event_header_t *) { return true; }
const clap_output_events_t kOut = {nullptr, outPush};

struct Ev
{
  union { clap_event_header_t h; clap_event_note_t n; clap_event_param_value_t p; };
};
struct EvList
{
  clap_input_events_t list{};
  std::vector<Ev> evs;
  EvList()
  {
    evs.reserve(64);
    list.ctx = this;
    list.size = [](const clap_input_events_t *l) -> uint32_t { return (uint32_t)((EvList *)l->ctx)->evs.size(); };
    list.get = [](const clap_input_events_t *l, uint32_t i) -> const clap_event_header_t * {
      return &((EvList *)l->ctx)->evs[i].h;
    };
  }
  void note(uint16_t type, int key, uint32_t t)
  {
    Ev e;
    std::memset(&e, 0, sizeof e);
    e.n.header = {sizeof(clap_event_note_t), t, CLAP_CORE_EVENT_SPACE_ID, type, 0};
    e.n.note_id = -1;
    e.n.key = (int16_t)key;
    e.n.velocity = 0.8;
    evs.push_back(e);
  }
  void param(clap_id id, double v, uint32_t t)
  {
    Ev e;
    std::memset(&e, 0, sizeof e);
    e.p.header = {sizeof(clap_event_param_value_t), t, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
    e.p.param_id = id;
    e.p.note_id = -1;
    e.p.port_index = -1;
    e.p.channel = -1;
    e.p.key = -1;
    e.p.value = v;
    evs.push_back(e);
  }
};

struct ParamInfo
{
  clap_id id;
  double minV, maxV;
  bool stepped;
};

double pick(Rng &r, const ParamInfo &pi)
{
  const double v = pi.minV + r.next() * (pi.maxV - pi.minV);
  return pi.stepped ? std::floor(v + 0.5) : v;
}

struct IStream
{
  clap_istream_t s{};
  const std::string *data;
  size_t pos = 0;
  explicit IStream(const std::string &d) : data(&d)
  {
    s.ctx = this;
    s.read = [](const clap_istream_t *st, void *buf, uint64_t size) -> int64_t {
      auto *self = (IStream *)st->ctx;
      const size_t n = std::min<size_t>((size_t)size, self->data->size() - self->pos);
      std::memcpy(buf, self->data->data() + self->pos, n);
      self->pos += n;
      return (int64_t)n;
    };
  }
};
struct OStream
{
  clap_ostream_t s{};
  std::string data;
  OStream()
  {
    s.ctx = this;
    s.write = [](const clap_ostream_t *st, const void *buf, uint64_t size) -> int64_t {
      ((OStream *)st->ctx)->data.append((const char *)buf, (size_t)size);
      return (int64_t)size;
    };
  }
};

struct Ctx
{
  const clap_plugin_t *plug = nullptr;
  const clap_plugin_params_t *params = nullptr;
  const clap_plugin_state_t *state = nullptr;
  Plugin *pl = nullptr;
  std::vector<ParamInfo> info;
  std::vector<std::string> chunks;    // tests/state_fixtures/*.txt  (host state_load)
  std::vector<std::string> presets;   // docs/presets/factory/**.json (editor apply-state)
  uint32_t seed = 1, blocks = 3000, ops = 400;
  bool control = false;
  std::atomic<bool> audioStarted{false}, mainDone{false};
  std::atomic<uint64_t> blocksRun{0};
};

/* The audio thread: the host's render callback, nothing more. Every block
   carries seeded host events; one block in eight is followed by a
   params_flush carrying one more param event, which is the flush a host makes
   on the audio thread while processing. */
void audioLoop(Ctx &c)
{
  c.plug->start_processing(c.plug);
  c.audioStarted.store(true, std::memory_order_release);
  Rng r{c.seed ^ 0xA0D10u};
  std::vector<float> L(kBlock), R(kBlock);
  float *chans[2] = {L.data(), R.data()};
  clap_audio_buffer_t ob{};
  ob.data32 = chans;
  ob.channel_count = 2;
  clap_process_t proc{};
  proc.frames_count = kBlock;
  proc.audio_outputs = &ob;
  proc.audio_outputs_count = 1;
  proc.out_events = &kOut;
  std::vector<int> held;
  EvList ev, fl;
  for (uint64_t blk = 0;; blk++)
  {
    // Bounded by iterations: at least `blocks`, and in the full run also until
    // the main schedule has finished, so every main op overlaps rendering.
    if (blk >= c.blocks && (c.control || c.mainDone.load(std::memory_order_acquire))) break;
    ev.evs.clear();
    // Integer draws, not `next() < p`: these are schedule odds, not tolerances,
    // and an inline float compare would read as one to tolerance_registry_check.
    if (held.size() < 6 && r.below(10) < 3)
    {
      const int key = 36 + (int)r.below(48);
      ev.note(CLAP_EVENT_NOTE_ON, key, 0);
      held.push_back(key);
    }
    if (!held.empty() && r.below(5) == 0)
    {
      const size_t k = r.below((uint32_t)held.size());
      ev.note(CLAP_EVENT_NOTE_OFF, held[k], kBlock / 2);
      held.erase(held.begin() + (long)k);
    }
    for (uint32_t n = r.below(4), t = 0; n > 0; n--, t += kBlock / 4)
    {
      const ParamInfo &pi = c.info[r.below((uint32_t)c.info.size())];
      ev.param(pi.id, pick(r, pi), t);
    }
    proc.in_events = &ev.list;
    c.plug->process(c.plug, &proc);
    if (r.below(8) == 0)
    {
      fl.evs.clear();
      const ParamInfo &pi = c.info[r.below((uint32_t)c.info.size())];
      fl.param(pi.id, pick(r, pi), 0);
      c.params->flush(c.plug, &fl.list, &kOut);
    }
    c.blocksRun.store(blk + 1, std::memory_order_relaxed);
  }
  c.plug->stop_processing(c.plug);
}

/* One editor/host main-thread operation. The editor cases call the Plugin
   member the corresponding gui_create lambda calls (hostIf.<name> noted on
   each), so what is exercised is the shipped code path, not a model of it. */
void mainOp(Ctx &c, Rng &r)
{
  Plugin *pl = c.pl;
  const ParamInfo &pi = c.info[r.below((uint32_t)c.info.size())];
  switch (r.below(13))
  {
  case 10:  // hostIf.panic: the editor's PANIC button (also writes a forensic dump under $HOME's store)
    pl->panicWithDump();
    break;
  case 11:  // the host's own main-thread reads: params.value and params.value_to_text
  {
    double v = 0;
    char text[64];
    if (c.params->get_value(c.plug, pi.id, &v)) c.params->value_to_text(c.plug, pi.id, v, text, sizeof text);
    break;
  }
  case 12:  // enqueue overflow: one burst past the editor queue's capacity (it drops on overflow)
    for (uint32_t k = 0; k < Plugin::kQCap + 64; k++) pl->guiSetParam(pi.id, pick(r, pi));
    break;
  case 0:   // (a) hostIf.setParam
    pl->guiSetParam(pi.id, pick(r, pi));
    break;
  case 1:   // (a) hostIf.gesture around hostIf.setParam: one drag
    pl->guiGesture(pi.id, true);
    for (int k = 0; k < 4; k++) pl->guiSetParam(pi.id, pick(r, pi));
    pl->guiGesture(pi.id, false);
    break;
  case 2:   // (a) the modulation matrix editor
    switch (r.below(6))
    {
    case 0: pl->modAddRoute(r.below(24), pi.id); break;                       // hostIf.modAddRoute
    case 1: pl->modSetDepth((int)r.below(8), r.next() * 2 - 1); break;       // hostIf.modSetDepth
    case 2: pl->modSetSource((int)r.below(8), r.below(24)); break;            // hostIf.modSetSource
    case 3: pl->modSetPolarity((int)r.below(8), (int)r.below(2)); break;      // hostIf.modSetPolarity
    case 4: pl->modRemoveRoute((int)r.below(8)); break;                       // hostIf.modRemoveRoute
    default: pl->setModWheel(r.next()); break;                                // hostIf.setModWheel
    }
    break;
  case 3:   // (b) host state_load of a fixture chunk
    if (!c.chunks.empty())
    {
      IStream is(c.chunks[r.below((uint32_t)c.chunks.size())]);
      c.state->load(c.plug, &is.s);
    }
    break;
  case 4:   // (b) hostIf.applyStateJson: the editor's preset LOAD
    if (!c.presets.empty())
      pl->applyStateJson(c.presets[r.below((uint32_t)c.presets.size())], "stress");
    break;
  case 5:   // (c) host state_save
  {
    OStream os;
    c.state->save(c.plug, &os.s);
    break;
  }
  case 6:   // (c) hostIf.getStateJson + hostIf.presetMatches: the editor's save and asterisk
  {
    const std::string j = pl->stateJson();
    pl->presetMatches(j);
    break;
  }
  case 7:   // (d) morph: capture, corner apply, toggle, mode, position, exempt, name
  {
    const int k = (int)r.below(4);
    switch (r.below(7))
    {
    case 0: pl->morphCapture(k); break;                                // hostIf.morphCapture
    case 1: pl->cornerApply(k, pl->cornerJson((k + 1) & 3)); break;    // hostIf.morphCornerApply / morphCornerJson
    case 2: pl->guiSetParam(151, r.below(2)); break;                   // morph on/off
    case 3: pl->guiSetParam(157, r.below(2)); break;                   // quantum <-> blend
    case 4: pl->guiSetParam(152, r.next()); pl->guiSetParam(153, r.next()); break;   // pad position
    case 5: pl->morphToggleExempt(pi.id); break;                       // hostIf.morphToggleExempt
    default: pl->setCornerName(k, "corner"); break;                    // hostIf.morphCornerSetName
    }
    break;
  }
  case 8:   // editor polling: the reads its timer makes
  {
    float spec[64];
    (void)pl->paramsJson();          // hostIf.getParamsJson
    (void)pl->modLiveJson();         // hostIf.modLiveJson
    pl->computeSpectrum(spec, 64);   // hostIf.getSpectrum
    (void)pl->liveCornerJson();      // hostIf.morphLiveJson
    (void)pl->morphOwnersJson();     // hostIf.morphOwnersJson
    pl->undoService();               // hostIf.undoService
    break;
  }
  default:  // (b) history navigation replays a snapshot through applyStateJson
    if (r.below(2)) pl->undoStep(r.below(2) ? -1 : 1);   // hostIf.undoStep
    else pl->undoGoTo((int)r.below(16));                 // hostIf.undoRestore
    break;
  }
}
}  // namespace stress

int main(int argc, char **argv)
{
  using namespace stress;
  Ctx c;
  std::string root = ".";
  for (int i = 1; i < argc; i++)
  {
    const std::string a = argv[i];
    auto val = [&](uint32_t &dst) { if (i + 1 < argc) dst = (uint32_t)std::strtoul(argv[++i], nullptr, 10); };
    if (a == "--plant" || a == "--plant-read")
    {
      /* The detector's own controls: one unsynchronised int, no plugin
         involved. The check requires TSan to REPORT these before it believes
         any clean run — a sanitizer that is absent, or a runtime that cannot
         start on this OS, would otherwise read clean — and requires its
         classifier to put each on the right side of the WRITE rule. */
      static int planted = 0;
      static volatile int sink = 0;
      const bool mainWrites = a == "--plant";
      std::thread t([mainWrites] {
        for (int k = 0; k < 1000; k++)
        {
          if (mainWrites) sink = sink + planted;   // the other thread only READS
          else planted = k;                        // the other thread WRITES
        }
      });
      for (int k = 0; k < 1000; k++)
      {
        if (mainWrites) planted = k;               // the main thread WRITES
        else sink = sink + planted;                // the main thread only READS
      }
      t.join();
      std::printf("tsan_stress: plant done (%d)\n", planted + sink * 0);
      return 0;
    }
    if (a == "--control") c.control = true;
    else if (a == "--seed") val(c.seed);
    else if (a == "--blocks") val(c.blocks);
    else if (a == "--ops") val(c.ops);
    else if (a == "--root" && i + 1 < argc) root = argv[++i];
    else { std::fprintf(stderr, "usage: tsan_stress [--plant | --plant-read | --control] [--seed N] [--blocks N] [--ops N] [--root DIR]\n"); return 2; }
  }
  collect(root + "/tests/state_fixtures", ".txt", c.chunks);
  /* One more chunk, built here: the first fixture plus every load line the
     corpus does not carry — intent bindings, ranges and a home, both LFO
     streams, both oscillators' ensemble timing — so the stress run's loads
     reach those writes. (The fixture corpus is golden-bound and append-only;
     a line appended in memory changes no fixture.) */
  if (!c.chunks.empty())
  {
    std::string x = c.chunks[0];
    if (!x.empty() && x.back() != '\n') x += '\n';
    x += "intent=L:1,B:4:0:2:0.5,R:4:1:0.2:0.8,H:2:0.25:0.75\n";
    x += "lfo=0.25;12345,0.75;67890\n";
    x += "ens=0.5;4242;0.001,0.002,0.003\n";
    x += "o1.ens=0.25;99;0.004,0.005\n";
    c.chunks.push_back(x);
  }
  collect(root + "/docs/presets/factory", ".json", c.presets);
  if (!c.control && (c.chunks.empty() || c.presets.empty()))
  {
    // An empty corpus would silently skip (b): a stress run that loads nothing reads clean.
    std::fprintf(stderr, "tsan_stress: no fixtures under %s (chunks %zu, presets %zu)\n", root.c_str(),
                 c.chunks.size(), c.presets.size());
    return 2;
  }

  auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  c.plug = factory->create_plugin(factory, &kHost, factory->get_plugin_descriptor(factory, 0)->id);
  if (!c.plug || !c.plug->init(c.plug)) { std::fprintf(stderr, "tsan_stress: create failed\n"); return 2; }
  c.params = (const clap_plugin_params_t *)c.plug->get_extension(c.plug, CLAP_EXT_PARAMS);
  c.state = (const clap_plugin_state_t *)c.plug->get_extension(c.plug, CLAP_EXT_STATE);
  c.pl = self(c.plug);
  for (uint32_t i = 0, n = c.params->count(c.plug); i < n; i++)
  {
    clap_param_info_t inf{};
    if (c.params->get_info(c.plug, i, &inf))
      c.info.push_back({inf.id, inf.min_value, inf.max_value, (inf.flags & CLAP_PARAM_IS_STEPPED) != 0});
  }
  c.plug->activate(c.plug, kSR, 1, kBlock);
  {
    /* The intent bus ON (param 266), so the audio thread's intentStep reads
       the tables the loads write; set before either thread starts (a flush
       on the main thread is legal while not processing). The audio thread's
       seeded host events may switch it later, as a host could. */
    EvList on;
    on.param(266, 1, 0);
    c.params->flush(c.plug, &on.list, &kOut);
  }

  std::thread audio([&c] { audioLoop(c); });
  while (!c.audioStarted.load(std::memory_order_acquire)) std::this_thread::yield();
  uint32_t done = 0;
  if (!c.control)
  {
    Rng r{c.seed ^ 0x3A1Bu};
    for (; done < c.ops; done++) mainOp(c, r);
  }
  c.mainDone.store(true, std::memory_order_release);
  audio.join();

  c.plug->deactivate(c.plug);
  c.plug->destroy(c.plug);
  std::printf("tsan_stress: %s seed=%u params=%zu chunks=%zu presets=%zu blocks=%llu main_ops=%u\n",
              c.control ? "control" : "full", c.seed, c.info.size(), c.chunks.size(), c.presets.size(),
              (unsigned long long)c.blocksRun.load(), done);
  return 0;
}
