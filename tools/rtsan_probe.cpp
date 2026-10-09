/*
 * rtsan_probe — does the audio thread do anything a realtime thread must not?
 * (ADR-197 risk row 1; B448 Wave B item B3; ADR-199.) Built and run under
 * Clang's RealtimeSanitizer by tools/rtsan_check.py, which carries the wiring
 * declaration; this file is the probe only.
 *
 * WHAT IT ADDS TO rtsafety_probe. That probe counts calls to operator new, so it
 * sees one kind of offence (heap allocation through C++) on one input (parameter
 * ids 1..99 plus the SUB block). RealtimeSanitizer intercepts the libc layer
 * underneath: malloc/free from ANY caller (C++ new, std::string, std::vector,
 * std::function), mutex and condition-variable calls, file and socket I/O,
 * sleeps, and the first-use guard of a function-local static. It reports each
 * with the stack, so the finding names a src/ line rather than a count.
 *
 * WHAT RUNS. One real plugin instance, created through the CLAP entry,
 * activated, then a seeded schedule of blocks. Every call the HOST makes on the
 * audio thread runs inside a realtime scope:
 *   process(), params.flush(), start_processing(), stop_processing(), reset().
 * Everything the host or the editor does on the MAIN thread runs OUTSIDE it
 * and only STAGES work for the audio thread: guiSetParam / guiGesture (the
 * editor's main-to-audio queue), state_load, applyStateJson (a preset load),
 * the mod-matrix and morph editors, undo, activate/deactivate. The next
 * process() then drains what was staged inside the scope, which is the path a
 * state load or a GUI knob really takes onto the audio thread.
 * Paths driven inside the scope (the brief's list): note on/off including a
 * burst past the polyphony cap (voice stealing), note choke, note expressions;
 * EVERY enumerated parameter id swept across its range, then random; morph in
 * blend and quantum modes with the pad moving; the intent bus with its macros
 * and drag brackets; the SUB OSC block; both engines; MIDI CC1, channel
 * pressure, pitch bend on the manager and member channels; host transport and
 * tempo; the output-buffer-full refusal path; bypass (stop, idle, start,
 * reset) and reactivation at a new rate.
 *
 * WHY A SINGLE TU THAT INCLUDES THE SHELL SOURCE rather than linking
 * HYPERSAW-impl: the tsan_stress harness's reasoning. It also lets the probe
 * call the Plugin members the editor's bridge calls. RealtimeSanitizer's
 * interceptors work on uninstrumented code too, so this is a convenience here
 * and a necessity there. The GUI object is linked uninstrumented only to
 * satisfy gui_create's symbols; no editor is ever created.
 *
 * NO WARM-UP BLOCK, deliberately. A function-local static, a thread_local or a
 * lazily built table that initialises on the first call inside process() is a
 * real offence (the first block is the one a live set hears), so the probe
 * must not hide it behind a priming call.
 *
 * DETERMINISM. One mulberry32 stream (forcecore::rngNext) from --seed, bounded
 * by block counts, never wall time. All file reads happen before the first
 * scope. Output is counts only; the sanitizer writes its reports to stderr,
 * which rtsan_check.py captures whole.
 *
 * MODES.  --plant    no plugin: ONE malloc inside the same realtime scope the
 *                    plugin calls use. The check requires it to be REPORTED
 *                    before it believes any clean run (a runtime that cannot
 *                    start reads exactly like a clean plugin).
 *         --control  the same scope, empty. Must be clean.
 *         default    the full schedule.
 * Exit 0 when the run completes; with halt_on_error off (the check sets it)
 * RealtimeSanitizer reports every distinct stack and the exit code is the
 * probe's own.
 */

#include "../src/hypersaw_clap.cpp"

// Named here, not inherited from the shell source: include_check holds every
// file to its own headers (MSVC does not reach them transitively).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>

/* Not in the installed rtsan_interface.h (only __rtsan_disable/_enable are), but
   exported by the runtime — the same pair [[clang::nonblocking]] functions are
   instrumented with. Using the pair keeps the probe's scope identical to a real
   nonblocking function's, without annotating the shipped code (out of scope). */
extern "C"
{
  void __rtsan_realtime_enter(void);
  void __rtsan_realtime_exit(void);
}

namespace rtprobe
{
constexpr double kSR = 44100.0;
constexpr uint32_t kMaxBlock = 4096;

/* THE realtime scope. Every audio-thread call goes through this one function,
   and so does the plant, so "the plant was reported" proves THIS wrapper arms
   the sanitizer, not some other call site. */
template <class F> void inRealtime(F &&f)
{
  __rtsan_realtime_enter();
  f();
  __rtsan_realtime_exit();
}

struct Rng
{
  uint32_t s;
  double next() { return forcecore::rngNext(s); }
  uint32_t below(uint32_t n) { return n ? (uint32_t)(next() * n) % n : 0; }
  /* Integer odds, not `next() < p`: these are schedule odds, not tolerances,
     and an inline float compare would read as one to tolerance_registry_check. */
  bool oneIn(uint32_t n) { return below(n) == 0; }
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
/* A host that offers the params extension, as every real one does: the shell's
   enqueueParam calls request_flush on it. Main thread only, so it is not in
   the scope, but without it the shell's hostParams branch would never run. */
void hostRescan(const clap_host_t *, clap_param_rescan_flags) {}
void hostClear(const clap_host_t *, clap_id, clap_param_clear_flags) {}
void hostRequestFlush(const clap_host_t *) {}
const clap_host_params_t kHostParams = {hostRescan, hostClear, hostRequestFlush};
const void *hostGetExtension(const clap_host_t *, const char *id)
{
  return std::strcmp(id, CLAP_EXT_PARAMS) == 0 ? &kHostParams : nullptr;
}
void hostNoop(const clap_host_t *) {}
const clap_host_t kHost = {CLAP_VERSION, nullptr, "rtsan_probe", "", "", "1.0",
                           hostGetExtension, hostNoop, hostNoop, hostNoop};
bool outPush(const clap_output_events_t *, const clap_event_header_t *) { return true; }
/* A host whose output buffer is full: emitNoteEnds and drainQueue must cope with
   a refused push (L0022's path), and coping must not allocate either. */
bool outRefuse(const clap_output_events_t *, const clap_event_header_t *) { return false; }
const clap_output_events_t kOut = {nullptr, outPush};
const clap_output_events_t kOutFull = {nullptr, outRefuse};

struct Ev
{
  union
  {
    clap_event_header_t h;
    clap_event_note_t n;
    clap_event_note_expression_t x;
    clap_event_param_value_t p;
    clap_event_midi_t m;
    clap_event_transport_t t;
  };
};
struct EvList
{
  clap_input_events_t list{};
  std::vector<Ev> evs;
  EvList()
  {
    evs.reserve(256);
    list.ctx = this;
    list.size = [](const clap_input_events_t *l) -> uint32_t { return (uint32_t)((EvList *)l->ctx)->evs.size(); };
    list.get = [](const clap_input_events_t *l, uint32_t i) -> const clap_event_header_t * {
      return &((EvList *)l->ctx)->evs[i].h;
    };
  }
  /* The core loop assumes non-decreasing time; a host guarantees it. */
  void sortByTime()
  {
    std::stable_sort(evs.begin(), evs.end(), [](const Ev &a, const Ev &b) { return a.h.time < b.h.time; });
  }
  Ev &add(uint16_t type, uint32_t size, uint32_t t)
  {
    Ev e;
    std::memset(&e, 0, sizeof e);
    e.h = {size, t, CLAP_CORE_EVENT_SPACE_ID, type, 0};
    evs.push_back(e);
    return evs.back();
  }
  void note(uint16_t type, int key, uint32_t t, int ch = 0, int id = -1, double vel = 0.8)
  {
    Ev &e = add(type, sizeof(clap_event_note_t), t);
    e.n.note_id = id;
    e.n.key = (int16_t)key;
    e.n.channel = (int16_t)ch;
    e.n.velocity = vel;
  }
  void expr(int which, double v, uint32_t t, int ch = -1)
  {
    Ev &e = add(CLAP_EVENT_NOTE_EXPRESSION, sizeof(clap_event_note_expression_t), t);
    e.x.expression_id = which;
    e.x.note_id = -1; e.x.port_index = -1; e.x.channel = (int16_t)ch; e.x.key = -1;
    e.x.value = v;
  }
  void midi(uint8_t b0, uint8_t b1, uint8_t b2, uint32_t t)
  {
    Ev &e = add(CLAP_EVENT_MIDI, sizeof(clap_event_midi_t), t);
    e.m.data[0] = b0; e.m.data[1] = b1; e.m.data[2] = b2;
  }
  void tempo(double bpm, uint32_t t)
  {
    Ev &e = add(CLAP_EVENT_TRANSPORT, sizeof(clap_event_transport_t), t);
    e.t.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_IS_PLAYING;
    e.t.tempo = bpm;
  }
  void param(clap_id id, double v, uint32_t t)
  {
    Ev &e = add(CLAP_EVENT_PARAM_VALUE, sizeof(clap_event_param_value_t), t);
    e.p.param_id = id;
    e.p.note_id = -1; e.p.port_index = -1; e.p.channel = -1; e.p.key = -1;
    e.p.value = v;
  }
};

struct ParamInfo
{
  clap_id id;
  double minV, maxV;
  bool stepped;
};

struct IStream
{
  clap_istream_t s{};
  const std::string *data;
  size_t pos = 0;
  explicit IStream(const std::string &d) : data(&d)
  {
    s.ctx = this;
    s.read = [](const clap_istream_t *st, void *buf, uint64_t size) -> int64_t {
      auto *self_ = (IStream *)st->ctx;
      const size_t n = std::min<size_t>((size_t)size, self_->data->size() - self_->pos);
      std::memcpy(buf, self_->data->data() + self_->pos, n);
      self_->pos += n;
      return (int64_t)n;
    };
  }
};

/* What the run actually exercised. Printed so a green result states its own
   coverage: a probe that reports clean over a schedule that skipped half the
   paths is the false green rtsafety_probe's header warns about. */
struct Tally
{
  uint64_t blocks = 0, flushes = 0, noteOns = 0, noteOffs = 0, chokes = 0, exprs = 0, midis = 0,
           tempos = 0, paramEvs = 0, bursts = 0, bypasses = 0, reactivations = 0, refused = 0;
  uint64_t guiSets = 0, gestures = 0, stateLoads = 0, presetLoads = 0, modEdits = 0, morphOps = 0,
           undoOps = 0;
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
  std::vector<uint32_t> touched;      // how many times each info[] id was written by a host event
  uint32_t seed = 1, blocks = 1200;
  std::vector<int> held;
  double sr = kSR;
  Tally t;
};

double pick(Rng &r, const ParamInfo &pi)
{
  /* Endpoints are over-weighted on purpose: a stepped param's top choice and a
     continuous param's extremes are where a table lookup or a lazily built
     per-choice structure first runs. */
  const uint32_t d = r.below(8);
  const double v = d == 0 ? pi.minV : d == 1 ? pi.maxV : pi.minV + r.next() * (pi.maxV - pi.minV);
  return pi.stepped ? std::floor(v + 0.5) : v;
}

size_t idxOf(const Ctx &c, clap_id id)
{
  for (size_t i = 0; i < c.info.size(); i++)
    if (c.info[i].id == id) return i;
  return 0;
}

void hostParam(Ctx &c, EvList &ev, size_t i, double v, uint32_t t)
{
  ev.param(c.info[i].id, v, t);
  c.touched[i]++;
  c.t.paramEvs++;
}

/* ---- the audio thread: every call here is inside the realtime scope ---- */

void processBlock(Ctx &c, EvList &ev, uint32_t frames, bool refuseOut, double bpm)
{
  static std::vector<float> L(kMaxBlock), R(kMaxBlock);
  float *chans[2] = {L.data(), R.data()};
  clap_audio_buffer_t ob{};
  ob.data32 = chans;
  ob.channel_count = 2;
  clap_event_transport_t tr{};
  tr.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_IS_PLAYING;
  tr.tempo = bpm;
  clap_process_t proc{};
  proc.frames_count = frames;
  proc.audio_outputs = &ob;
  proc.audio_outputs_count = 1;
  proc.in_events = &ev.list;
  proc.out_events = refuseOut ? &kOutFull : &kOut;
  proc.transport = &tr;
  proc.steady_time = (int64_t)(c.t.blocks * 128);
  ev.sortByTime();
  inRealtime([&] { c.plug->process(c.plug, &proc); });
  c.t.blocks++;
  if (refuseOut) c.t.refused++;
}

void flushParams(Ctx &c, EvList &ev, bool refuseOut)
{
  ev.sortByTime();
  inRealtime([&] { c.params->flush(c.plug, &ev.list, refuseOut ? &kOutFull : &kOut); });
  c.t.flushes++;
}

/* A host bypass: the plugin stops being driven, then is woken and reset (CLAP
   reset is an audio-thread call). A real host does this on every transport
   stop, which makes it as common as a note. */
void bypassCycle(Ctx &c)
{
  inRealtime([&] { c.plug->stop_processing(c.plug); });
  inRealtime([&] { c.plug->start_processing(c.plug); });
  inRealtime([&] { c.plug->reset(c.plug); });
  c.held.clear();
  c.t.bypasses++;
}

/* Reactivation at a (possibly new) rate: stop on the audio thread, deactivate
   and activate on the MAIN thread (outside the scope; activate rebuilds the
   cores and allocates, which is why it is not allowed on the audio thread),
   then start again. */
void reactivate(Ctx &c, double sr)
{
  inRealtime([&] { c.plug->stop_processing(c.plug); });
  c.plug->deactivate(c.plug);
  c.sr = sr;
  c.plug->activate(c.plug, sr, 1, kMaxBlock);
  inRealtime([&] { c.plug->start_processing(c.plug); });
  c.held.clear();
  c.t.reactivations++;
}

/* ---- the main thread: OUTSIDE the scope, staging work for the next block ---- */

/* One host/editor main-thread operation. The editor cases call the Plugin member
   the corresponding gui_create lambda calls (as tools/tsan_stress.cpp does), so
   what is staged is the shipped code path. */
void mainOp(Ctx &c, Rng &r)
{
  Plugin *pl = c.pl;
  const ParamInfo &pi = c.info[r.below((uint32_t)c.info.size())];
  switch (r.below(9))
  {
  case 0:   // hostIf.setParam: the editor's knob, onto the main-to-audio queue
    pl->guiSetParam(pi.id, pick(r, pi));
    c.t.guiSets++;
    break;
  case 1:   // one drag: gesture begin, values, gesture end
    pl->guiGesture(pi.id, true);
    for (int k = 0; k < 4; k++) pl->guiSetParam(pi.id, pick(r, pi));
    pl->guiGesture(pi.id, false);
    c.t.gestures++;
    break;
  case 2:   // the modulation matrix editor
    switch (r.below(5))
    {
    case 0: pl->modAddRoute(r.below(24), pi.id); break;
    case 1: pl->modSetDepth((int)r.below(8), r.next() * 2 - 1); break;
    case 2: pl->modSetSource((int)r.below(8), r.below(24)); break;
    case 3: pl->modSetPolarity((int)r.below(8), (int)r.below(2)); break;
    default: pl->modRemoveRoute((int)r.below(8)); break;
    }
    c.t.modEdits++;
    break;
  case 3:   // host state_load of a fixture chunk
    if (!c.chunks.empty())
    {
      IStream is(c.chunks[r.below((uint32_t)c.chunks.size())]);
      c.state->load(c.plug, &is.s);
      c.t.stateLoads++;
    }
    break;
  case 4:   // the editor's preset LOAD
    if (!c.presets.empty())
    {
      pl->applyStateJson(c.presets[r.below((uint32_t)c.presets.size())], "rtsan");
      c.t.presetLoads++;
    }
    break;
  case 5:   // morph: capture, corner apply, toggle, mode, pad position
  {
    const int k = (int)r.below(4);
    switch (r.below(5))
    {
    case 0: pl->morphCapture(k); break;
    case 1: pl->cornerApply(k, pl->cornerJson((k + 1) & 3)); break;
    case 2: pl->guiSetParam(151, r.below(2)); break;
    case 3: pl->guiSetParam(157, r.below(2)); break;
    default: pl->guiSetParam(152, r.next()); pl->guiSetParam(153, r.next()); break;
    }
    c.t.morphOps++;
    break;
  }
  case 6:   // history navigation replays a snapshot through applyStateJson
    if (r.oneIn(2)) pl->undoStep(r.oneIn(2) ? -1 : 1);
    else pl->undoGoTo((int)r.below(16));
    c.t.undoOps++;
    break;
  case 7:   // the intent macros and the pad's drag bracket
    pl->guiGesture(166 + r.below(8), true);
    pl->guiSetParam(166 + r.below(8), r.next());
    pl->guiGesture(166 + r.below(8), false);
    c.t.gestures++;
    break;
  default:  // editor polling reads a snapshot the audio thread just published
    (void)pl->paramsJson();
    (void)pl->modLiveJson();
    break;
  }
}

/* ---- scenes: a preamble of param writes, then the random per-block schedule ---- */

struct Write { clap_id id; double v; };
struct Scene
{
  const char *name;
  std::vector<Write> pre;
  bool padMoves, macrosMove;
};

std::vector<Scene> scenes()
{
  // 43 engine, 151 morph on, 157 morph mode (1 = quantum), 266 intent bus,
  // 268 pad latch, 4015 SUB OSC gate (rtsafety_probe's note: gated OFF by default).
  return {
      {"saw", {{43, 0}}, false, false},
      {"saw+sub", {{43, 0}, {4015, 1}}, false, false},
      {"saw+morph-blend", {{43, 0}, {4015, 1}, {151, 1}, {157, 0}}, true, false},
      {"saw+morph-quantum", {{43, 0}, {151, 1}, {157, 1}}, true, false},
      {"saw+intent", {{43, 0}, {151, 1}, {157, 0}, {266, 1}, {268, 1}}, true, true},
      {"spectra", {{43, 1}, {4015, 1}, {151, 0}, {266, 0}}, false, false},
      {"spectra+morph-quantum", {{43, 1}, {151, 1}, {157, 1}}, true, false},
  };
}

void hostEvents(Ctx &c, Rng &r, EvList &ev, uint32_t frames, const Scene &sc, bool burst)
{
  const uint32_t half = frames / 2;
  if (burst)
  {
    // Past the polyphony cap in ONE block: every slot taken, then stolen from.
    for (int k = 0; k < hypersaw::kPoly + 8; k++)
    {
      const int key = 30 + 2 * k;
      ev.note(CLAP_EVENT_NOTE_ON, key, 0, 0, 5000 + k);
      c.held.push_back(key);
      c.t.noteOns++;
    }
    c.t.bursts++;
  }
  if (r.oneIn(3) && c.held.size() < 64)
  {
    const int key = r.oneIn(20) ? (r.oneIn(2) ? 0 : 127) : 24 + (int)r.below(84);
    const int ch = r.oneIn(4) ? (int)r.below(16) : 0;
    ev.note(CLAP_EVENT_NOTE_ON, key, r.below(frames), ch, r.oneIn(2) ? -1 : (int)r.below(1000),
            r.oneIn(8) ? 0.0 : 0.2 + 0.8 * r.next());
    c.held.push_back(key);
    c.t.noteOns++;
  }
  if (!c.held.empty() && r.oneIn(3))
  {
    const size_t k = r.below((uint32_t)c.held.size());
    ev.note(CLAP_EVENT_NOTE_OFF, r.oneIn(15) ? -1 : c.held[k], half + r.below(frames - half), 0, -1, 0.0);
    c.held.erase(c.held.begin() + (long)k);
    c.t.noteOffs++;
  }
  if (!c.held.empty() && r.oneIn(30))
  {
    ev.note(CLAP_EVENT_NOTE_CHOKE, c.held[r.below((uint32_t)c.held.size())], r.below(frames));
    c.t.chokes++;
  }
  if (r.oneIn(6))
  {
    ev.expr(CLAP_NOTE_EXPRESSION_TUNING, r.next() * 24 - 12, r.below(frames));
    c.t.exprs++;
  }
  if (r.oneIn(8))
  {
    ev.expr(CLAP_NOTE_EXPRESSION_PRESSURE, r.next(), r.below(frames));
    c.t.exprs++;
  }
  if (r.oneIn(4))
  {
    const uint32_t t = r.below(frames);
    const uint8_t ch = r.oneIn(2) ? 0 : (uint8_t)(1 + r.below(15));   // 0 = plain wheel, else MPE member
    switch (r.below(6))
    {
    case 0: case 1: ev.midi((uint8_t)(0xE0 | ch), (uint8_t)r.below(128), (uint8_t)r.below(128), t); break;
    case 2: ev.midi((uint8_t)(0xB0 | ch), 1, (uint8_t)r.below(128), t); break;          // CC1, mod wheel
    case 3: ev.midi((uint8_t)(0xD0 | ch), (uint8_t)r.below(128), 0, t); break;          // channel pressure
    case 4: ev.midi((uint8_t)(0xB0 | ch), r.oneIn(2) ? 64 : 7, (uint8_t)r.below(128), t); break;   // unhandled CCs
    default: ev.midi((uint8_t)(0xC0 | ch), (uint8_t)r.below(128), 0, t); break;         // program change
    }
    c.t.midis++;
  }
  if (r.oneIn(10))
  {
    ev.tempo(40.0 + r.next() * 200.0, r.below(frames));
    c.t.tempos++;
  }
  for (uint32_t n = r.below(5); n > 0; n--)
  {
    const size_t i = r.below((uint32_t)c.info.size());
    hostParam(c, ev, i, pick(r, c.info[i]), r.below(frames));
  }
  if (sc.padMoves)
  {
    hostParam(c, ev, idxOf(c, 152), r.next(), r.below(frames));
    hostParam(c, ev, idxOf(c, 153), r.next(), r.below(frames));
  }
  if (sc.macrosMove)
    for (clap_id id = 166; id < 174; id++) hostParam(c, ev, idxOf(c, id), r.next(), r.below(frames));
}

/* Every enumerated parameter id, written through the host's event path at five
   points across its range (min, quarter, mid, three-quarter, max), eight ids a
   block. The ids-1..99 reach of rtsafety_probe was the point of failure here:
   the engine blocks, the routing matrix and the 4000-range sit above it. */
void sweepAll(Ctx &c, EvList &ev)
{
  const double fr[5] = {0.0, 0.25, 0.5, 0.75, 1.0};
  const uint32_t frames = 512;
  size_t i = 0;
  while (i < c.info.size())
  {
    ev.evs.clear();
    if (i == 0) ev.note(CLAP_EVENT_NOTE_ON, 48, 0);
    for (int k = 0; k < 8 && i < c.info.size(); k++, i++)
      for (int s = 0; s < 5; s++)
      {
        const ParamInfo &pi = c.info[i];
        const double v = pi.minV + fr[s] * (pi.maxV - pi.minV);
        hostParam(c, ev, i, pi.stepped ? std::floor(v + 0.5) : v, (uint32_t)(s * (frames / 5) + k));
      }
    processBlock(c, ev, frames, false, 120.0);
  }
}

void runSchedule(Ctx &c)
{
  Rng r{c.seed ^ 0x5EEDu};
  EvList ev, fl;
  const uint32_t sizes[] = {1, 16, 33, 64, 127, 128, 256, 512, 1024, 2048, kMaxBlock};
  const std::vector<Scene> sc = scenes();
  const uint32_t perScene = std::max<uint32_t>(1, c.blocks / (uint32_t)sc.size());
  const double rates[] = {48000.0, 96000.0, kSR};
  uint32_t reacts = 0;

  // Both engines get the exhaustive sweep, then every scene its random blocks.
  for (int pass = 0; pass < 2; pass++)
  {
    ev.evs.clear();
    ev.param(43, pass, 0);
    ev.param(4015, 1, 0);
    processBlock(c, ev, 64, false, 120.0);
    sweepAll(c, ev);
  }
  for (size_t s = 0; s < sc.size(); s++)
  {
    for (uint32_t b = 0; b < perScene; b++)
    {
      ev.evs.clear();
      const uint32_t frames = sizes[r.below((uint32_t)(sizeof sizes / sizeof sizes[0]))];
      if (b == 0)
        for (const Write &w : sc[s].pre) ev.param(w.id, w.v, 0);
      hostEvents(c, r, ev, frames, sc[s], b == 0 || r.oneIn(60));
      // Staged work lands on the audio thread HERE: the drain at the top of the next process().
      if (r.oneIn(5)) mainOp(c, r);
      processBlock(c, ev, frames, r.oneIn(8), 60.0 + (double)((b / 17) % 15) * 10.0);
      if (r.oneIn(9))
      {
        fl.evs.clear();
        for (uint32_t n = 1 + r.below(3); n > 0; n--)
        {
          const size_t i = r.below((uint32_t)c.info.size());
          hostParam(c, fl, i, pick(r, c.info[i]), 0);
        }
        if (r.oneIn(3)) fl.note(CLAP_EVENT_NOTE_ON, 40 + (int)r.below(40), 0);
        flushParams(c, fl, r.oneIn(8));
      }
      if (b % 61 == 60) bypassCycle(c);
      if (b % 157 == 156) reactivate(c, rates[reacts++ % 3]);
    }
  }
}

int usage()
{
  std::fprintf(stderr, "usage: rtsan_probe [--plant|--control] [--seed N] [--blocks N] [--root DIR]\n");
  return 2;
}
}  // namespace rtprobe

int main(int argc, char **argv)
{
  using namespace rtprobe;
  Ctx c;
  std::string root = ".";
  bool plant = false, control = false;
  for (int i = 1; i < argc; i++)
  {
    const std::string a = argv[i];
    auto val = [&](uint32_t &dst) { if (i + 1 < argc) dst = (uint32_t)std::strtoul(argv[++i], nullptr, 10); };
    if (a == "--plant") plant = true;
    else if (a == "--control") control = true;
    else if (a == "--seed") val(c.seed);
    else if (a == "--blocks") val(c.blocks);
    else if (a == "--root" && i + 1 < argc) root = argv[++i];
    else return usage();
  }
  if (plant || control)
  {
    /* The detector's own proof, no plugin. ONE malloc through the very wrapper
       the plugin calls use (volatile sink: an elided allocation would be a
       plant that cannot fire). The control is the same scope, empty. */
    static void *volatile sink;
    if (plant) inRealtime([] { sink = std::malloc(64); });
    else inRealtime([] {});
    std::free(sink);
    std::printf("rtsan_probe: %s done\n", plant ? "plant" : "control");
    return 0;
  }

  collect(root + "/tests/state_fixtures", ".txt", c.chunks);
  collect(root + "/docs/presets/factory", ".json", c.presets);
  if (c.chunks.empty() || c.presets.empty())
  {
    // An empty corpus would silently skip the state-load drain: a probe that loads nothing reads clean.
    std::fprintf(stderr, "rtsan_probe: no fixtures under %s (chunks %zu, presets %zu)\n", root.c_str(),
                 c.chunks.size(), c.presets.size());
    return 2;
  }

  auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  c.plug = factory->create_plugin(factory, &kHost, factory->get_plugin_descriptor(factory, 0)->id);
  if (!c.plug || !c.plug->init(c.plug)) { std::fprintf(stderr, "rtsan_probe: create failed\n"); return 2; }
  c.params = (const clap_plugin_params_t *)c.plug->get_extension(c.plug, CLAP_EXT_PARAMS);
  c.state = (const clap_plugin_state_t *)c.plug->get_extension(c.plug, CLAP_EXT_STATE);
  c.pl = self(c.plug);
  for (uint32_t i = 0, n = c.params->count(c.plug); i < n; i++)
  {
    clap_param_info_t inf{};
    if (c.params->get_info(c.plug, i, &inf))
      c.info.push_back({inf.id, inf.min_value, inf.max_value, (inf.flags & CLAP_PARAM_IS_STEPPED) != 0});
  }
  c.touched.assign(c.info.size(), 0);
  c.plug->activate(c.plug, kSR, 1, kMaxBlock);
  inRealtime([&] { c.plug->start_processing(c.plug); });

  runSchedule(c);

  inRealtime([&] { c.plug->stop_processing(c.plug); });
  c.plug->deactivate(c.plug);
  c.plug->destroy(c.plug);

  size_t untouched = 0;
  for (uint32_t n : c.touched) untouched += n == 0;
  const Tally &t = c.t;
  std::printf("rtsan_probe: full seed=%u params=%zu (untouched %zu) chunks=%zu presets=%zu\n", c.seed,
              c.info.size(), untouched, c.chunks.size(), c.presets.size());
  std::printf("rtsan_probe: process=%llu flush=%llu refused-out=%llu bypass=%llu reactivate=%llu\n",
              (unsigned long long)t.blocks, (unsigned long long)t.flushes, (unsigned long long)t.refused,
              (unsigned long long)t.bypasses, (unsigned long long)t.reactivations);
  std::printf("rtsan_probe: noteOn=%llu (steal bursts %llu) noteOff=%llu choke=%llu expr=%llu midi=%llu "
              "tempo=%llu paramEv=%llu\n",
              (unsigned long long)t.noteOns, (unsigned long long)t.bursts, (unsigned long long)t.noteOffs,
              (unsigned long long)t.chokes, (unsigned long long)t.exprs, (unsigned long long)t.midis,
              (unsigned long long)t.tempos, (unsigned long long)t.paramEvs);
  std::printf("rtsan_probe: main-thread staging: guiSet=%llu gesture=%llu stateLoad=%llu presetLoad=%llu "
              "mod=%llu morph=%llu undo=%llu\n",
              (unsigned long long)t.guiSets, (unsigned long long)t.gestures, (unsigned long long)t.stateLoads,
              (unsigned long long)t.presetLoads, (unsigned long long)t.modEdits, (unsigned long long)t.morphOps,
              (unsigned long long)t.undoOps);
  return 0;
}
