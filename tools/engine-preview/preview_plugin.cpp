// horde ENGINE PREVIEW (B624): a thin CLAP around the clean-room engine, so the human can play it in Live and we
// can see what it costs there. Throwaway on purpose: it isn't the B398 shell and owes none of ADR-186 §5's rules.
// Its window is GUI 3's page in a web view (preview_webview.mm, B628), or the small AppKit one (preview_gui.mm) when
// built with HORDE_PREVIEW_WEBVIEW=OFF. No filters, effects, modulation matrix or morph: the engine has none of them
// yet. Pitch bend and the bank's glide are played here, a stand-in for the pitch system (RC-3) until B504 (B632).
//
// Five choices worth keeping in mind before changing this file:
// - Every non-reserved engine row is a host parameter (317, plus Preset; B632, ADR-370), under the engine's own id.
//   Live gives a plug-in of more than 64 an empty panel that fills as controls in the plug-in's window are touched,
//   which the page's gestures do. Polyphony is the one voice control (the human, 2026-11-05).
// - A preset change builds a whole new engine on the main thread and hands it to the audio thread. load_state and
//   prepare aren't real-time (RT-1), and the swap keeps the audio thread free of them. Sounding notes stop at the swap.
// - The saved chunk is the preset's index and the rows that differ from what the preset loads, not the
//   engine's state text. It stays small (large chunks make hosts drop state silently), and it never reads the engine
//   from the main thread while audio runs.
// - Pitch (B632): a stand-in for the pitch system (RC-3), MIDI pitch bend at a fixed +/-2 semitones and each preset's
//   glide exactly as 10.3 states it for the phrases (the law below, after tests/wasm/item15b.mjs). A note's pitch is
//   taken over only once it glides or is bent; until then the engine plays it 12-TET itself, tuning expression included.
//   A moving pitch goes in at audio rate through IF-2's pitch stream, not as an event per sample, which would cut the
//   engine into one-sample segments for the whole glide.
// - GUI 3's bridge (bridge_core.h, B628) runs on the main thread. Page messages reach audio through the same lock-free
//   edit queue as the window's edits, each message closed by an acknowledgement entry, so the audio thread publishes
//   the last applied seq in order. Frames read the engine's RC-8 snapshot, which any thread may read.
#include <clap/clap.h>

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "bank.h"
#include "bridge_core.h"
#include "bridge_ext.h"
#include "flush_to_zero.h"
#include "horde_engine.h"
#include "preview_gui.h"

namespace {

// kPresetId (preview_gui.h) sits above the engine's id range: IF-11 keeps its ids below 0xB00000.
using preview::kPresetId;
constexpr int kMaxVoices = 64;
constexpr int kMaxRouteKeys = 8;
constexpr int kMaxTargets = 16;
constexpr int kMaxEvents = 4096;
constexpr int kEngineMaxBlock = 4096;   // the most prepare accepts (DT-2)
// The plug-in's own safety rule, not the engine's: a preset that loads asking for more voices than this gets this many,
// set through the engine's own Param path before the load completes, so every display reads the truth. Eight voices of
// a heavy patch can already overrun a block, and several bank presets ask for 16. A polyphony set by hand stands.
constexpr int kPolyphonyCap = 8;
// A power of two: edits waiting for the audio thread. A page `set` can carry every engine row at once (a preset morph,
// a load), about 330 entries, and a message goes in whole or waits, so this holds a dozen of those.
constexpr std::uint32_t kEditQueue = 4096;
constexpr int kNoteRing = 128;
constexpr std::uint64_t kScopeRing = 8192, kScopeWindow = 2048, kScopeStep = 16;   // a power of two; 128 points a frame          // recent note-ons, by note id, for the page's notes (a power of two)
constexpr const char* kDefaultPreset = "Trance jitter";   // one of the lighter patches in the 2026-11-05 worst-block run
#ifndef HORDE_PREVIEW_BUILD_LABEL
#define HORDE_PREVIEW_BUILD_LABEL "engine unknown, build 0"
#endif
// Which engine commit and which build this is, so the human can tell test builds apart in Live (window, log, description).
constexpr const char* kBuildLabel = HORDE_PREVIEW_BUILD_LABEL;

// Every manifest row (the shadow, the page and the saved state), and the host rows among them: since B632 every
// non-reserved row, in manifest order, under the engine's own id, so automation written today still finds its row when
// the manifest grows (ids are append-only, IF-11). Built once on the main thread at init, read-only after, so the audio
// thread reads it freely.
struct Rows {
  std::vector<const horde::ParamInfo*> all;   // the manifest, in its ascending id order
  std::vector<double> default_v;              // per manifest row: the manifest's defaults as v
  std::vector<int> host_of;                   // per manifest row: its host row, or -1 (reserved)
  std::vector<const horde::ParamInfo*> row;   // the host rows
  std::vector<int> at;                        // a host row's manifest index
  int count = 0;
  std::string hash, text;                     // the manifest hash (section 3) and the text it hashes
};

const Rows& rows() {
  static const Rows table = [] {
    Rows t;
    for (std::size_t i = 0; i < horde::manifest_size(); ++i) t.all.push_back(&horde::manifest_param(i));
    t.host_of.assign(t.all.size(), -1);
    for (std::size_t m = 0; m < t.all.size(); ++m) {
      if (t.all[m]->reserved) continue;   // not a host parameter in 1.0 (5.1): the engine refuses a Param to it
      t.host_of[m] = t.count++;
      t.at.push_back(int(m));
      t.row.push_back(t.all[m]);
    }
    auto probe = horde::make_engine();   // the manifest's defaults as v, which ParamInfo gives only in units
    for (const auto* p : t.all) t.default_v.push_back(probe->param_v(p->id));
    t.text = preview::bridge::manifest_text(t.all);
    t.hash = preview::bridge::sha256_hex(t.text);
    return t;
  }();
  return table;
}

// A row's manifest index by id, or -1: a binary search, as the manifest is in ascending id order (IF-11). Called on
// the audio thread, so no allocation.
int manifest_index(clap_id id) {
  const auto& all = rows().all;
  auto it = std::lower_bound(all.begin(), all.end(), id, [](const horde::ParamInfo* p, clap_id i) { return p->id < i; });
  return it != all.end() && (*it)->id == id ? int(it - all.begin()) : -1;
}

int default_preset() {
  for (int i = 0; i < kBankSize; ++i)
    if (!std::strcmp(kBank[i].name, kDefaultPreset)) return i;
  return 0;
}

// A stepped row's host value is its option index, so the panel steps; the engine always takes v (IF-11).
bool stepped(const horde::ParamInfo& p) { return p.law == horde::Law::Step && p.steps > 1; }
double host_to_v(const horde::ParamInfo& p, double host) { return stepped(p) ? host / double(p.steps - 1) : host; }
double v_to_host(const horde::ParamInfo& p, double v) { return stepped(p) ? std::round(v * double(p.steps - 1)) : v; }

// B.1's routes per note, as tests/harness/bank.h plays them, without its map: nothing here allocates once built.
struct Target { int env; int key; double scale; };
double octave_in_v(const horde::ParamInfo& p) {
  if (p.law == horde::Law::LogOffZone) return 0.99 / std::log2(p.hi / 0.004);
  return 1.0 / std::log2(p.hi / p.lo);
}

struct Slot {
  std::unique_ptr<horde::Engine> engine;
  int preset = 0;
  std::uint64_t generation = 0;   // counts builds, so the main thread can tell a slot built after a load was asked for
  Target targets[kMaxTargets];
  int n_targets = 0;
  std::uint32_t key_id[kMaxRouteKeys] = {};
  int n_keys = 0;
  double env_velocity[2] = {1, 1};
  double glide_s = 0;          // B.1's glide and glideAlways, played by the pitch stand-in (B632)
  bool glide_always = false;
  std::uint64_t ci = 29;
  std::uint64_t counter[kMaxVoices] = {};
  double last[kMaxVoices][kMaxRouteKeys] = {};
};

// One preset's cost in this session. Written by the audio thread alone, read by the main thread for the log, so
// relaxed atomics are enough: a torn snapshot would only make one log line briefly stale.
struct Stats {
  std::atomic<std::uint64_t> blocks{0}, overruns{0};
  std::atomic<double> seconds{0}, audio_seconds{0}, worst{0};
};

// One entry of the edit queue. Value: a parameter change, in the host's form for a host parameter (a stepped row's
// option index) and in v for any other engine row; the host hears of it only when it is a host parameter, inside its
// own gesture when `bracket` is set (a window edit, or a page set with no page gesture open). Begin and End: a page
// gesture on a host row. Ack: every page message ends with one, so the seq it carries is published only after all
// that message's entries are applied.
struct Edit {
  enum Kind : std::uint8_t { Value, Begin, End, Ack };
  Kind kind = Value;
  bool bracket = true;
  clap_id id = 0;
  double value = 0;
  std::int64_t seq = 0;
};

// What the page's session needs between frames. Main thread only.
// A LOAD the page is waiting for (GUI 3's device.js, B628): a `hello`, or a `preset`. The page adopts the first frame
// whose ack reaches the load's seq and that carries every engine row: those values become its state and its undo
// starts fresh. So while a load is pending, frames hold `ack` below its seq and carry no `params` or `presets`; the
// frame that completes it carries them all. A preset load completes only once the audio thread runs an engine built
// for that preset after the message came, so the page never adopts the old preset's values.
struct Load {
  bool pending = false;
  bool preset = false;         // wait for the engine swap, not only the ack
  std::int64_t seq = 0;        // the load's seq, the latest if two overlapped
  int index = -1;              // preset: the bank index asked for
  std::uint64_t after = 0;     // preset: slots built up to this generation are too early
};

struct Session {
  preview::bridge::Want want;
  std::vector<double> sent;      // per manifest row, the v last sent in `params` (NaN: never)
  int sent_preset = -1;
  bool sent_plugin = false;
  Load load;
  bool saw_hello = false;        // this page loads on hello: no resending until a frame lands (bridge_resend)
  bool hello_due = false;        // the hello reply rides with the next completed load
  preview::bridge::Mismatch mismatch;
  std::string events;            // gesture echoes waiting for the next frame, JSON objects joined by commas
  std::string state;
  bool state_due = false;
  std::vector<std::string> unknown, reserved, types;
  bool refused_due = false;
  std::vector<bool> gesture_open;   // per manifest row
  std::deque<std::vector<Edit>> waiting;   // whole messages the queue had no room for yet, in order
  std::unique_ptr<horde::GuiFrame> frame = std::make_unique<horde::GuiFrame>();   // 64 voices of members: tens of KB, never on the stack
  std::string out;               // the last frame's text, alive until the next
};

// The pitch stand-in's notes and bend (B632). Audio thread only; nothing here allocates once built.
constexpr int kPitchNotes = 64;     // notes followed, newest first: an older one past this is forgotten
constexpr int kPitchStreams = 16;   // notes gliding at once
constexpr int kBendChanges = 64;    // bend messages in one span
constexpr double kBendSemitones = 2;   // fixed, until B504's per-patch bend range
struct PitchNote {
  std::int32_t id = -1;
  std::int16_t key = -1, channel = -1;
  double tuning = 0;           // semitones, the note's tuning expression (IF-2: folded in once we set the pitch)
  double f = 0;                // its 12-TET pitch, as the engine computes it (Note::hz), so a takeover changes nothing
  double from = 0;             // where its glide starts; 0: it doesn't glide
  std::uint64_t at = 0;        // its note-on on the engine's sample clock
  bool held = false, gliding = false, taken = false, live = false;
};
struct BendChange { std::uint32_t sample; int channel; double semitones; };
struct Pitch {
  PitchNote notes[kPitchNotes];
  int next = 0, last = -1;     // the next slot to fill; the most recently started note
  double bend[16] = {};        // semitones per MIDI channel
  double bend_at_start[16] = {};
  BendChange changes[kBendChanges];
  int n_changes = 0;
  std::uint64_t span_clock = 0;   // the engine's sample clock at the span being built
  float stream[kPitchStreams][kEngineMaxBlock];
  horde::AudioStream streams[kPitchStreams];
  int n_streams = 0;
};

struct Plugin {
  clap_plugin_t plugin{};
  const clap_host_t* host = nullptr;
  const clap_host_params_t* host_params = nullptr;

  double sample_rate = 44100;
  int max_block = kEngineMaxBlock;
  bool active = false;      // main thread
  int built_preset = -1;    // main thread
  bool apply_shadow = false;   // main thread: the next build keeps the shadow's values (a loaded state)
  bool force_build = false;    // main thread
  Slot* newest = nullptr;      // main thread: the slot built last, live or on its way; the frames read its engine
  std::uint64_t generations = 0;   // main thread: slots built so far
  // The live slot's generation << 8 | its preset, published by the audio thread at the swap (one word, never torn).
  std::atomic<std::uint64_t> live_tag{0};
  std::vector<std::vector<double>> preset_v;   // main thread: per preset, every row's v as it loads (empty: not read yet)

  std::atomic<int> wanted_preset{0};
  // v per manifest row: what the host, the page and the saved state read. Sized once at init.
  std::unique_ptr<std::atomic<double>[]> shadow;

  Slot* live = nullptr;                   // audio thread while active
  std::atomic<Slot*> incoming{nullptr};   // main to audio
  std::atomic<Slot*> retired{nullptr};    // audio to main, deleted there
  std::atomic<bool> rescan_pending{false};

  // One writer (the main thread) and one reader (whichever thread processes or flushes), so no lock on the audio path.
  Edit edits[kEditQueue] = {};
  std::atomic<std::uint32_t> edit_head{0}, edit_tail{0};
  std::atomic<std::int64_t> acked{0};     // the last page seq applied, audio to main (ADR-319's confirm)
  void* gui = nullptr;
  Session session;

  // The page's notes. The engine's GUI frame has no note or gate, so the audio thread keeps them from its own note-ons
  // and the engine's voice events (VO-10), one packed word per engine voice so the main thread can't read one torn.
  struct RecentNote { std::int32_t id = -1; std::int16_t key = -1; double velocity = 0; };
  RecentNote recent[kNoteRing];           // audio thread
  std::uint32_t recent_next = 0;          // audio thread
  std::atomic<std::uint64_t> voice_note[kMaxVoices] = {};
  std::atomic<bool> engine_silent{true};  // OU-4 as the audio thread last saw it
  // The output's peak since the last frame, tapped here: the engine's frame holds only its last processing call's
  // peaks, and with routes a call is one control interval long. The reader asks for a reset; the audio thread does it.
  // GN-4's tap (B632): the plug-in's own output, mono, into a ring the main thread copies the newest window from. One
  // writer; the reader takes kScopeWindow behind the write count, so the writer would need the rest of the ring
  // (most of 0.15 s) during one copy to tear it.
  float scope_ring[kScopeRing] = {};
  std::atomic<std::uint64_t> scope_written{0};
  float out_peak_accum = 0;               // audio thread
  std::atomic<float> out_peak{0};
  std::atomic<bool> out_peak_reset{false};

  std::vector<horde::Event> events;
  Pitch pitch;                            // audio thread
  std::int32_t next_note_id = 1 << 24;   // above any id a host's own counter reaches in a session
  std::atomic<std::uint64_t> nonfinite{0};
  std::atomic<std::uint64_t> idle_blocks{0};   // blocks skipped while the engine was silent (OU-4)
  std::atomic<std::uint32_t> last_frames{0};
  std::uint64_t frames_since_callback = 0;
  Stats stats[kBankSize];
  std::string log_path;
};

Plugin* self(const clap_plugin_t* p) { return static_cast<Plugin*>(p->plugin_data); }

void write_stats(Plugin& s) {
  if (s.log_path.empty()) {
    const char* home = std::getenv("HOME");
    if (!home) return;
    std::string dir = std::string(home) + "/Library/Logs";
    mkdir(dir.c_str(), 0755);
    dir += "/horde-engine-preview";
    mkdir(dir.c_str(), 0755);
    char stamp[32];
    const std::time_t now = std::time(nullptr);
    std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", std::localtime(&now));
    s.log_path = dir + "/session-" + stamp + "-" + std::to_string(getpid()) + "-" + std::to_string(reinterpret_cast<std::uintptr_t>(&s) % 100000) + ".tsv";
  }
  const std::string temp = s.log_path + ".tmp";
  std::FILE* f = std::fopen(temp.c_str(), "w");
  if (!f) return;
  std::fprintf(f, "# %s, sample_rate %.0f, last block %u frames, non-finite samples zeroed %llu, idle blocks skipped %llu\n", kBuildLabel, s.sample_rate,
               s.last_frames.load(std::memory_order_relaxed),
               (unsigned long long)s.nonfinite.load(std::memory_order_relaxed), (unsigned long long)s.idle_blocks.load(std::memory_order_relaxed));
  std::fprintf(f, "preset\tsounding_blocks\tmean_pct_of_block\tworst_pct_of_block\toverruns\n");
  for (int i = 0; i < kBankSize; ++i) {
    const Stats& st = s.stats[i];
    const std::uint64_t blocks = st.blocks.load(std::memory_order_relaxed);
    if (!blocks) continue;
    const double audio = st.audio_seconds.load(std::memory_order_relaxed);
    std::fprintf(f, "%s\t%llu\t%.2f\t%.2f\t%llu\n", kBank[i].name, (unsigned long long)blocks, audio > 0 ? 100 * st.seconds.load(std::memory_order_relaxed) / audio : 0.0,
                 100 * st.worst.load(std::memory_order_relaxed), (unsigned long long)st.overruns.load(std::memory_order_relaxed));
  }
  std::fclose(f);
  std::rename(temp.c_str(), s.log_path.c_str());
}

// kPolyphonyCap applied to one preset's values as it loads (manifest order), in v: the row is a Step of consecutive
// integers, so its value is lo + the option index (horde_engine.h, ParamInfo::lo).
void cap_polyphony(std::vector<double>& values) {
  static const horde::ParamInfo* row = horde::find_param("voice.polyphony");
  if (!row || row->steps < 2) return;
  const int m = manifest_index(row->id);
  if (m < 0 || std::size_t(m) >= values.size()) return;
  values[std::size_t(m)] = preview::bridge::capped_count_v(*row, values[std::size_t(m)], kPolyphonyCap);
}

Slot* build_slot(Plugin& s, int preset, bool keep_shadow) {
  auto slot = std::make_unique<Slot>();
  slot->preset = preset;
  slot->generation = ++s.generations;
  slot->engine = horde::make_engine();
  if (!slot->engine || !slot->engine->prepare(s.sample_rate, s.max_block)) return nullptr;
  if (!slot->engine->load_state(kBank[preset].state).ok) return nullptr;
  slot->ci = std::uint64_t(std::max(1L, std::lround(s.sample_rate / 1500)));   // PA-3
  slot->glide_s = kBank[preset].glide_s;
  slot->glide_always = kBank[preset].glide_always;

#ifndef HORDE_PREVIEW_PLANT_NO_ROUTES   // the smoke test's must-fail build: without routes it must miss the reference
  for (int r = 0; r < kBank[preset].n_routes && slot->n_targets < kMaxTargets; ++r) {
    const PresetRoute& route = kBank[preset].routes[r];
    const horde::ParamInfo* p = horde::find_param(route.key);
    if (!p || route.octaves == 0 || (route.env != 2 && route.env != 3)) continue;
    int key = 0;
    while (key < slot->n_keys && slot->key_id[key] != p->id) ++key;
    if (key == slot->n_keys) {
      if (slot->n_keys == kMaxRouteKeys) continue;
      slot->key_id[slot->n_keys++] = p->id;
    }
    slot->targets[slot->n_targets++] = {route.env, key, route.octaves * octave_in_v(*p)};
  }
#endif
  for (int k = 0; k < 2; ++k) {
    const horde::ParamInfo* p = horde::find_param(k == 0 ? "env2.velocity" : "env3.velocity");
    if (p) slot->env_velocity[k] = horde::value_of(*p, slot->engine->param_v(p->id));
  }

  // What this preset loads, as the plug-in loads it (polyphony capped), before any value of ours: the saved state keeps
  // only the rows that differ from it. `actual` is what the engine holds now, so only rows that differ are sent.
  const Rows& t = rows();
  std::vector<double> actual(t.all.size());
  for (std::size_t m = 0; m < t.all.size(); ++m) actual[m] = slot->engine->param_v(t.all[m]->id);
  std::vector<double>& loaded = s.preset_v[std::size_t(preset)];
  loaded = actual;
  cap_polyphony(loaded);

  std::vector<horde::Event> setup;
  setup.reserve(t.all.size() + 1);
  for (std::size_t m = 0; m < t.all.size(); ++m) {
    if (!keep_shadow) s.shadow[m].store(loaded[m], std::memory_order_relaxed);
    if (t.all[m]->reserved) continue;   // not a host parameter: the engine refuses a Param to it (5.1, PA-7)
    horde::Event e;
    e.type = horde::EventType::Param;
    e.param_id = t.all[m]->id;
    e.value = keep_shadow ? s.shadow[m].load(std::memory_order_relaxed) : loaded[m];
    if (e.value != actual[m]) setup.push_back(e);
  }
#ifdef HORDE_PREVIEW_PLANT_VOICE_CAP   // smoke.cpp's must-fail build: the retired Voices limit of 4, put back
  horde::Event cap;
  cap.type = horde::EventType::VoiceCap;
  cap.integer = 4;
  cap.flag = true;
  setup.push_back(cap);
#endif
  slot->engine->flush(setup.data(), setup.size());
  return slot.release();
}

void play_routes(Plugin& s, Slot& slot) {
  horde::Engine& e = *slot.engine;
  horde::VoiceView v;
  const int n = std::min(e.voice_count(), kMaxVoices);
  for (int i = 0; i < n; ++i) {
    if (!e.observe_voice(i, v) || !v.sounding) continue;
    if (slot.counter[i] != v.note_counter) {   // a fresh start clears the offsets (PA-2)
      slot.counter[i] = v.note_counter;
      std::fill_n(slot.last[i], kMaxRouteKeys, 0.0);
    }
    double sum[kMaxRouteKeys] = {};
    for (int t = 0; t < slot.n_targets; ++t) {
      const Target& target = slot.targets[t];
      const double row = slot.env_velocity[target.env - 2];
      sum[target.key] += target.scale * v.env_levels[target.env - 1] * (1 - row + row * v.velocity);
    }
    for (int k = 0; k < slot.n_keys; ++k) {
      if (sum[k] == slot.last[i][k] || s.events.size() >= kMaxEvents) continue;
      slot.last[i][k] = sum[k];
      horde::Event ev;
      ev.type = horde::EventType::VoiceOffset;
      ev.note_id = v.note_id;
      ev.param_id = slot.key_id[k];
      ev.value = sum[k];
      s.events.push_back(ev);
    }
  }
}

void set_param(Plugin& s, clap_id id, double value) {
  if (id == kPresetId) {
    s.wanted_preset.store(std::clamp(int(std::lround(value)), 0, kBankSize - 1), std::memory_order_relaxed);
    return;
  }
  const int m = manifest_index(id);
  if (m < 0 || rows().all[std::size_t(m)]->reserved) return;
  const int host = rows().host_of[std::size_t(m)];
  // A host parameter's value is in the host's form; any other engine row can only come from the page, in v.
  const double v = std::clamp(host >= 0 ? host_to_v(*rows().row[host], value) : value, 0.0, 1.0);
  s.shadow[m].store(v, std::memory_order_relaxed);
  horde::Event e;
  e.type = horde::EventType::Param;
  e.param_id = id;
  e.value = v;
  if (s.events.size() < kMaxEvents) s.events.push_back(e);
}

bool host_parameter(clap_id id) {
  if (id == kPresetId) return true;
  const int m = manifest_index(id);
  return m >= 0 && rows().host_of[std::size_t(m)] >= 0;
}

void push_gesture(const clap_output_events_t* out, clap_id id, bool begin) {
  clap_event_param_gesture_t gesture{};
  gesture.header = {sizeof gesture, 0, CLAP_CORE_EVENT_SPACE_ID, std::uint16_t(begin ? CLAP_EVENT_PARAM_GESTURE_BEGIN : CLAP_EVENT_PARAM_GESTURE_END), 0};
  gesture.param_id = id;
  out->try_push(out, &gesture.header);
}

// The window's and the page's edits become parameter changes here, and the host hears of those that are its
// parameters, so its automation and panel keep up.
void drain_edits(Plugin& s, const clap_output_events_t* out) {
  std::uint32_t tail = s.edit_tail.load(std::memory_order_relaxed);
  const std::uint32_t head = s.edit_head.load(std::memory_order_acquire);
  for (; tail != head; ++tail) {
    const Edit& edit = s.edits[tail % kEditQueue];
    if (edit.kind == Edit::Ack) {
      // Release: the main thread that reads this seq then reads every shadow value written before it.
      s.acked.store(edit.seq, std::memory_order_release);
      continue;
    }
    if (edit.kind == Edit::Begin || edit.kind == Edit::End) {
      if (out) push_gesture(out, edit.id, edit.kind == Edit::Begin);
      continue;
    }
    set_param(s, edit.id, edit.value);
    if (!out || !host_parameter(edit.id)) continue;
    if (edit.bracket) push_gesture(out, edit.id, true);
    clap_event_param_value_t value{};
    value.header = {sizeof value, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
    value.param_id = edit.id;
    value.note_id = -1;
    value.port_index = -1;
    value.channel = -1;
    value.key = -1;
    value.value = edit.value;
    out->try_push(out, &value.header);
    if (edit.bracket) push_gesture(out, edit.id, false);
  }
  s.edit_tail.store(tail, std::memory_order_release);
}

// The engine's voice events (VO-10) give each voice its note; the note's key and velocity come from our own note-ons.
// Packed into one word per voice: bit 0 a note, bit 1 its gate, bits 8-15 the key, bits 16-31 the velocity x 65535.
std::uint64_t pack_note(const Plugin& s, std::int32_t note_id, bool gate) {
  for (const auto& r : s.recent) {
    if (r.id != note_id || note_id < 0) continue;
    const std::uint64_t vel = std::uint64_t(std::lround(std::clamp(r.velocity, 0.0, 1.0) * 65535));
    return 1u | (gate ? 2u : 0u) | (std::uint64_t(std::uint8_t(r.key)) << 8) | (vel << 16);
  }
  return 0;   // a note we never saw start: no key to show
}

void pitch_forget_note(Pitch& p, std::int32_t id);

void follow_voices(Plugin& s, horde::Engine& engine) {
  horde::OutEvent buffer[64];
  for (;;) {
    const std::size_t n = engine.drain_output_events(buffer, 64);
    for (std::size_t k = 0; k < n; ++k) {
      const horde::OutEvent& e = buffer[k];
      if (e.voice < 0 || e.voice >= kMaxVoices) continue;
      std::atomic<std::uint64_t>& slot = s.voice_note[e.voice];
      switch (e.type) {
        case horde::OutType::VoiceStarted: slot.store(pack_note(s, e.note_id, true), std::memory_order_relaxed); break;
        case horde::OutType::VoiceTaken:
          pitch_forget_note(s.pitch, e.old_note_id);
          slot.store(pack_note(s, e.new_note_id, true), std::memory_order_relaxed);
          break;
        case horde::OutType::NoteChanged: slot.store(pack_note(s, e.new_note_id, true), std::memory_order_relaxed); break;
        case horde::OutType::VoiceReleased: slot.store(slot.load(std::memory_order_relaxed) & ~std::uint64_t(2), std::memory_order_relaxed); break;
        case horde::OutType::VoiceFreed:
          slot.store(0, std::memory_order_relaxed);
          pitch_forget_note(s.pitch, e.note_id);
          break;
        default: break;
      }
    }
    if (n < 64) break;   // bounded: the engine's own queue is finite, and each pass empties up to 64 of it
  }
}

void forget_voices(Plugin& s) {
  for (auto& v : s.voice_note) v.store(0, std::memory_order_relaxed);
}

void remember_note(Plugin& s, const horde::Event& on) {
  s.recent[s.recent_next++ % kNoteRing] = {on.note_id, on.key, on.value};
}

// ---- the pitch stand-in (B632) ---------------------------------------------------------------------------------
// 10.3's glide, as tests/wasm/item15b.mjs writes it for the renders: a note that glides moves from `from` to its own
// pitch f along ln f(t) = ln f + (ln from - ln f) e^(-3t/T), t seconds after its note-on, T the preset's glide (95 %
// done at t = T). Unbent: the bend multiplies the result.
double glide_hz(const PitchNote& n, std::uint64_t clock, double rate, double glide_s) {
  if (!n.from) return n.f;
  const double t = double(clock - n.at) / rate;
  return std::exp(std::log(n.f) + (std::log(n.from) - std::log(n.f)) * std::exp(-3 * t / glide_s));
}
double bend_factor(double semitones) { return std::pow(2.0, semitones / 12.0); }
double twelve_tet(int key, double tuning) { return 440 * std::pow(2.0, (key - 69) / 12.0 + tuning / 12); }   // the engine's Note::hz

void forget_pitch(Pitch& p) {
  for (auto& n : p.notes) n = PitchNote{};
  p.next = 0;
  p.last = -1;
}

PitchNote* pitch_note(Pitch& p, std::int32_t id) {
  if (id < 0) return nullptr;
  for (auto& n : p.notes)
    if (n.live && n.id == id) return &n;
  return nullptr;
}

void push_pitch(Plugin& s, std::int32_t id, double hz) {
  horde::Event e;
  e.type = horde::EventType::NotePitch;
  e.note_id = id;
  e.value = hz;
  if (s.events.size() < kMaxEvents) s.events.push_back(e);
}

// A note-on: whether it glides (10.3: when a note is held, or always with glideAlways; from the most recently started
// note, a chord's earlier note included, at the pitch that note sounds at this one's first sample), and if it glides
// or a bend stands on its channel, its pitch from its first sample (IF-2), so it never sounds 12-TET first.
void pitch_note_on(Plugin& s, const horde::Event& on, std::uint32_t sample) {
  Pitch& p = s.pitch;
  const Slot* slot = s.live;
  const std::uint64_t at = p.span_clock + sample;
  bool any_held = false;
  for (const auto& n : p.notes) any_held = any_held || (n.live && n.held);
  const PitchNote* src = p.last >= 0 && p.notes[p.last].live ? &p.notes[p.last] : nullptr;
  PitchNote& n = p.notes[p.next];
  const int index = p.next;
  p.next = (p.next + 1) % kPitchNotes;
  PitchNote fresh;
  fresh.id = on.note_id;
  fresh.key = on.key;
  fresh.channel = on.channel;
  fresh.f = twelve_tet(on.key, 0);
  fresh.at = at;
  fresh.held = fresh.live = true;
  if (slot && slot->glide_s > 0 && src && (any_held || slot->glide_always)) {
    const double from = glide_hz(*src, at, s.sample_rate, slot->glide_s);
    if (from != fresh.f) {
      fresh.from = from;
      fresh.gliding = fresh.taken = true;
    }
  }
  n = fresh;
  p.last = index;
  const int ch = std::clamp<int>(on.channel, 0, 15);
  if (n.gliding) {
    // The glide's first pitch as an event at the note-on too, not only in the stream: the engine sets up a voice from
    // its pitch at the start (IF-2: "a glide begins at its start pitch"), and the stream reaches it one sample later.
    // The same float value the stream carries there, so the two never disagree.
    push_pitch(s, n.id, double(float(glide_hz(n, at, s.sample_rate, slot->glide_s) * bend_factor(p.bend[ch]))));
  } else if (p.bend[ch] != 0) {
    n.taken = true;
    push_pitch(s, n.id, n.f * bend_factor(p.bend[ch]));
  }
}

void pitch_note_off(Pitch& p, const horde::Event& off) {
  for (auto& n : p.notes) {
    if (!n.live || !n.held) continue;
    const bool match = off.note_id >= 0 ? n.id == off.note_id
                                        : (off.channel < 0 || off.channel == n.channel) && (off.key < 0 || off.key == n.key);
    if (match) n.held = false;
  }
}

void pitch_bend(Plugin& s, int channel, double semitones, std::uint32_t sample) {
  Pitch& p = s.pitch;
  p.bend[channel] = semitones;
  if (p.n_changes < kBendChanges) p.changes[p.n_changes++] = {sample, channel, semitones};
  for (auto& n : p.notes) {
    if (!n.live || n.channel != channel || n.gliding) continue;   // a gliding note's stream reads the bend sample by sample
    n.taken = true;
    push_pitch(s, n.id, n.f * bend_factor(semitones));
  }
}

void pitch_tuning(Plugin& s, std::int32_t id, double semitones) {
  PitchNote* n = pitch_note(s.pitch, id);
  if (!n) return;
  n->tuning = semitones;
  n->f = twelve_tet(n->key, semitones);
  if (n->taken && !n->gliding) push_pitch(s, n->id, n->f * bend_factor(s.pitch.bend[std::clamp<int>(n->channel, 0, 15)]));
}

// Events stay in sample order, which the engine reads them in: a glide's last pitch goes in at its own sample.
void insert_pitch(Plugin& s, std::uint32_t sample, std::int32_t id, double hz) {
  if (s.events.size() >= kMaxEvents) return;
  horde::Event e;
  e.type = horde::EventType::NotePitch;
  e.note_id = id;
  e.value = hz;
  e.sample = sample;
  auto at = std::upper_bound(s.events.begin(), s.events.end(), sample, [](std::uint32_t v, const horde::Event& x) { return v < x.sample; });
  s.events.insert(at, e);   // within the capacity reserved at activation: no allocation
}

// Each gliding note's pitch for the span, one value per sample (IF-2's audio-rate input, float32). NaN where it gives
// none: before the note starts, and from the sample its glide arrives, where one exact NotePitch at f takes over. It
// arrives when its float value equals the note's own float pitch: the stream can't carry anything finer, so this is
// item15b.mjs's end of a run (within 1e-9 of ln f) at the resolution the stream has, and it needs no threshold.
void pitch_streams(Plugin& s, std::uint32_t span) {
  Pitch& p = s.pitch;
  const Slot* slot = s.live;
  p.n_streams = 0;
  for (auto& n : p.notes) {
    if (!n.live || !n.gliding || !slot || p.n_streams == kPitchStreams) continue;
    float* out = p.stream[p.n_streams];
    const int ch = std::clamp<int>(n.channel, 0, 15);
    double semitones = p.bend_at_start[ch];
    int change = 0;
    for (std::uint32_t k = 0; k < span; ++k) {
      while (change < p.n_changes && p.changes[change].sample <= k) {
        if (p.changes[change].channel == ch) semitones = p.changes[change].semitones;
        ++change;
      }
      const std::uint64_t clock = p.span_clock + k;
      if (!n.gliding || clock < n.at) { out[k] = NAN; continue; }
      const double factor = bend_factor(semitones);
      const float value = float(glide_hz(n, clock, s.sample_rate, slot->glide_s) * factor);
      if (value == float(n.f * factor)) {
        n.gliding = false;
        insert_pitch(s, k, n.id, n.f * factor);
        out[k] = NAN;
        continue;
      }
      out[k] = value;
    }
    p.streams[p.n_streams++] = {n.id, horde::StreamTarget::VoicePitchHz, 0, out};
  }
  std::copy(std::begin(p.bend), std::end(p.bend), std::begin(p.bend_at_start));
  p.n_changes = 0;
}

void pitch_forget_note(Pitch& p, std::int32_t id) {
  for (auto& n : p.notes)
    if (n.live && n.id == id) n.live = false;
}

// One CLAP event to engine events, at `sample` in the span being built.
void translate(Plugin& s, const clap_event_header_t* h, std::uint32_t sample) {
  const std::size_t before = s.events.size();
  if (h->space_id == CLAP_CORE_EVENT_SPACE_ID) {
    if (h->type == CLAP_EVENT_NOTE_ON || h->type == CLAP_EVENT_NOTE_OFF || h->type == CLAP_EVENT_NOTE_CHOKE) {
      const auto* n = reinterpret_cast<const clap_event_note_t*>(h);
      horde::Event e;
      e.type = h->type == CLAP_EVENT_NOTE_ON ? horde::EventType::NoteOn : horde::EventType::NoteOff;
      e.note_id = n->note_id;
      // VST3 and AU hosts often send -1. A route's offset addressed to -1 would reach every voice (PA-2), so a
      // note-on always gets an id of its own; its note-off still finds it by channel and key (VO-1).
      if (h->type == CLAP_EVENT_NOTE_ON && e.note_id < 0) e.note_id = s.next_note_id++;
      e.channel = n->channel;
      e.key = n->key;
      e.value = h->type == CLAP_EVENT_NOTE_CHOKE ? 0 : n->velocity;
      if (h->type == CLAP_EVENT_NOTE_ON) remember_note(s, e);
      if (s.events.size() < kMaxEvents) s.events.push_back(e);
      if (h->type == CLAP_EVENT_NOTE_ON) pitch_note_on(s, e, sample);
      else pitch_note_off(s.pitch, e);
    } else if (h->type == CLAP_EVENT_NOTE_EXPRESSION) {
      const auto* x = reinterpret_cast<const clap_event_note_expression_t*>(h);
      horde::Event e;
      e.type = horde::EventType::Expression;
      e.note_id = x->note_id;
      e.channel = x->channel;
      e.key = x->key;
      e.value = x->value;
      switch (x->expression_id) {
        case CLAP_NOTE_EXPRESSION_VOLUME: e.expression = horde::ExpressionKind::Volume; break;
        case CLAP_NOTE_EXPRESSION_PAN: e.expression = horde::ExpressionKind::Pan; break;
        case CLAP_NOTE_EXPRESSION_TUNING: e.expression = horde::ExpressionKind::Tuning; break;
        case CLAP_NOTE_EXPRESSION_VIBRATO: e.expression = horde::ExpressionKind::Vibrato; break;
        case CLAP_NOTE_EXPRESSION_EXPRESSION: e.expression = horde::ExpressionKind::Expression; break;
        case CLAP_NOTE_EXPRESSION_BRIGHTNESS: e.expression = horde::ExpressionKind::Brightness; break;
        default: e.expression = horde::ExpressionKind::Pressure; break;
      }
      if (s.events.size() < kMaxEvents) s.events.push_back(e);
      if (e.expression == horde::ExpressionKind::Tuning) pitch_tuning(s, e.note_id, e.value);
    } else if (h->type == CLAP_EVENT_PARAM_VALUE) {
      const auto* p = reinterpret_cast<const clap_event_param_value_t*>(h);
      set_param(s, p->param_id, p->value);
    } else if (h->type == CLAP_EVENT_MIDI) {
      const auto* m = reinterpret_cast<const clap_event_midi_t*>(h);
      const int status = m->data[0] & 0xF0, channel = m->data[0] & 0x0F, d1 = m->data[1], d2 = m->data[2];
      horde::Event e;
      e.channel = std::int16_t(channel);
      if (status == 0x90 && d2 > 0) {
        e.type = horde::EventType::NoteOn;
        e.note_id = s.next_note_id++;
        e.key = std::int16_t(d1);
        e.value = d2 / 127.0;
        remember_note(s, e);
        if (s.events.size() < kMaxEvents) s.events.push_back(e);
        pitch_note_on(s, e, sample);   // its pitch, if any, after it: same sample, either order (IF-2)
        for (std::size_t i = before; i < s.events.size(); ++i) s.events[i].sample = sample;
        return;
      } else if (status == 0x80 || status == 0x90) {
        e.type = horde::EventType::NoteOff;
        e.key = std::int16_t(d1);
        pitch_note_off(s.pitch, e);
      } else if (status == 0xB0 && d1 == 64) {
        e.type = horde::EventType::Pedal;
        e.value = d2;
      } else if (status == 0xB0 && d1 == 120) {
        e.type = horde::EventType::AllSoundOff;
        for (auto& n : s.pitch.notes) n.held = false;
      } else if (status == 0xB0 && d1 == 123) {
        e.type = horde::EventType::AllNotesOff;
        for (auto& n : s.pitch.notes) n.held = false;
      } else if (status == 0xD0) {
        e.type = horde::EventType::ChannelPressure;
        e.value = d1 / 127.0;
      } else if (status == 0xE0) {
#ifndef HORDE_PREVIEW_PLANT_DROP_BEND   // smoke.cpp's must-fail build: a bend that never arrives
        // 14 bits, centre 8192, at a fixed +/-2 semitones: the top and bottom values reach exactly +2 and -2.
        const int value = (d1 & 0x7F) | ((d2 & 0x7F) << 7);
        pitch_bend(s, channel, kBendSemitones * (value - 8192) / (value >= 8192 ? 8191.0 : 8192.0), sample);
#endif
        for (std::size_t i = before; i < s.events.size(); ++i) s.events[i].sample = sample;
        return;
      } else {
        return;
      }
      if (s.events.size() < kMaxEvents) s.events.push_back(e);
    }
  }
  for (std::size_t i = before; i < s.events.size(); ++i) s.events[i].sample = sample;
}

// GN-4: the block just sent to the host, as the scope draws it. Silence writes its zeros too, or the scope would hold
// the last loud window.
void tap(Plugin& s, const float* left, const float* right, std::uint32_t frames) {
  const std::uint64_t written = s.scope_written.load(std::memory_order_relaxed);
#if defined(HORDE_PREVIEW_PLANT_FROZEN_TAP)   // smoke.cpp's must-fail build: the tap stops after its first block
  if (written > 0) return;
#endif
  for (std::uint32_t i = 0; i < frames; ++i) {
#if defined(HORDE_PREVIEW_PLANT_SILENT_TAP)   // smoke.cpp's must-fail build: the tap hears silence while sound plays
    s.scope_ring[(written + i) & (kScopeRing - 1)] = 0;
#else
    s.scope_ring[(written + i) & (kScopeRing - 1)] = 0.5f * (left[i] + right[i]);
#endif
  }
  s.scope_written.store(written + frames, std::memory_order_release);
}

void render(Plugin& s, const clap_process_t* proc, float* left, float* right, std::uint32_t frames) {
  Slot& slot = *s.live;
  horde::Engine& engine = *slot.engine;
  const clap_input_events_t* in = proc->in_events;
  const std::uint32_t n_in = in ? in->size(in) : 0;
  std::uint32_t next = 0;
  bool transport_due = proc->transport != nullptr;
  for (std::uint32_t pos = 0; pos < frames;) {
    std::uint32_t span = std::min(frames - pos, std::uint32_t(s.max_block));
    const std::uint64_t clock = engine.sample_clock();
    const bool routed = slot.n_targets > 0;
    if (routed) span = std::min(span, std::uint32_t(slot.ci - clock % slot.ci));
    s.events.clear();
    s.pitch.span_clock = clock;
    if (transport_due) {
      transport_due = false;
      horde::Event e;
      e.type = horde::EventType::Transport;
      const clap_event_transport_t& t = *proc->transport;
      if (t.flags & CLAP_TRANSPORT_HAS_TEMPO) e.transport.tempo_bpm = t.tempo;
      if (t.flags & CLAP_TRANSPORT_HAS_TIME_SIGNATURE) {
        e.transport.ts_num = t.tsig_num;
        e.transport.ts_den = t.tsig_denom;
      }
      if (t.flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE) {
        e.transport.position_quarters = double(t.song_pos_beats) / double(CLAP_BEATTIME_FACTOR);
        e.transport.loop_start_quarters = double(t.loop_start_beats) / double(CLAP_BEATTIME_FACTOR);
        e.transport.loop_end_quarters = double(t.loop_end_beats) / double(CLAP_BEATTIME_FACTOR);
      }
      e.transport.playing = (t.flags & CLAP_TRANSPORT_IS_PLAYING) != 0;
      e.transport.looping = (t.flags & CLAP_TRANSPORT_IS_LOOP_ACTIVE) != 0;
      s.events.push_back(e);
    }
    if (pos == 0) drain_edits(s, proc->out_events);
    // The span's first sample: its events, then the routes (PA-3's order, as the render harness plays them).
    while (next < n_in) {
      const clap_event_header_t* h = in->get(in, next);
      if (h->time > pos) break;
      translate(s, h, 0);
      ++next;
    }
    if (routed && clock % slot.ci == 0) play_routes(s, slot);
    while (next < n_in) {
      const clap_event_header_t* h = in->get(in, next);
      if (h->time >= pos + span) break;
      translate(s, h, h->time - pos);
      ++next;
    }
    pitch_streams(s, span);
    engine.process(s.events.data(), s.events.size(), s.pitch.n_streams ? s.pitch.streams : nullptr, std::size_t(s.pitch.n_streams), left + pos, right + pos, int(span));
    pos += span;
  }
}

clap_process_status plugin_process(const clap_plugin_t* p, const clap_process_t* proc) {
  Plugin& s = *self(p);
  bool want_callback = false;
  if (s.incoming.load(std::memory_order_acquire) && !s.retired.load(std::memory_order_acquire)) {
    Slot* fresh = s.incoming.exchange(nullptr, std::memory_order_acq_rel);
    if (s.live) s.retired.store(s.live, std::memory_order_release);
    s.live = fresh;
    s.live_tag.store(fresh->generation << 8 | std::uint64_t(fresh->preset), std::memory_order_release);
    forget_voices(s);   // a new engine starts with no voices; the old one's notes stop at the swap
    forget_pitch(s.pitch);   // and no note to glide from: a render on a fresh engine starts the same way
    want_callback = true;
  }
  if (proc->audio_outputs_count == 0 || proc->frames_count == 0) return CLAP_PROCESS_CONTINUE;
  // OU-4: a silent engine outputs exact zero and only its clock moves, so the shell may stop calling it. Measured on
  // 1ad270a, a silent process() still cost 7 to 13 % of a core per instance (the human heard it as an ambient load);
  // skip() costs about 0.1 to 0.4 %. Any event or window edit wakes the engine the same block.
  float* left = proc->audio_outputs[0].data32[0];
  float* right = proc->audio_outputs[0].data32[1];
  const std::uint32_t frames = proc->frames_count;
  if (!s.live) {
    std::fill_n(left, frames, 0.0f);
    std::fill_n(right, frames, 0.0f);
    return CLAP_PROCESS_CONTINUE;
  }

  const bool quiet_in = (!proc->in_events || proc->in_events->size(proc->in_events) == 0) &&
                        s.edit_tail.load(std::memory_order_relaxed) == s.edit_head.load(std::memory_order_acquire);
  if (quiet_in && s.live->engine->silent()) {
    std::fill_n(left, frames, 0.0f);
    std::fill_n(right, frames, 0.0f);
    s.live->engine->skip(frames);
    tap(s, left, right, frames);
    s.engine_silent.store(true, std::memory_order_relaxed);
    if (s.out_peak_reset.exchange(false, std::memory_order_relaxed)) {   // silence reads as silence, not the last loud frame
      s.out_peak_accum = 0;
      s.out_peak.store(0, std::memory_order_relaxed);
    }
    s.idle_blocks.fetch_add(1, std::memory_order_relaxed);
    s.frames_since_callback += frames;
    if (want_callback || s.frames_since_callback >= std::uint64_t(2 * s.sample_rate)) {
      s.frames_since_callback = 0;
      s.host->request_callback(s.host);
    }
    return CLAP_PROCESS_CONTINUE;
  }
  const int preset = s.live->preset;
  const bool was_silent = s.live->engine->silent();
  const auto t0 = std::chrono::steady_clock::now();
  horde2::underFlushToZero([&] { render(s, proc, left, right, frames); });
  const auto t1 = std::chrono::steady_clock::now();

  // Output guard (Blind-Spot Armor: latch and report, never clamp silently): a bad sample must not reach the host,
  // and the session log shows the count.
  std::uint64_t bad = 0;
  if (s.out_peak_reset.exchange(false, std::memory_order_relaxed)) s.out_peak_accum = 0;
  float peak = s.out_peak_accum;
  for (std::uint32_t i = 0; i < frames; ++i) {
    if (!std::isfinite(left[i])) { left[i] = 0; ++bad; }
    if (!std::isfinite(right[i])) { right[i] = 0; ++bad; }
    peak = std::max({peak, std::fabs(left[i]), std::fabs(right[i])});
  }
  if (bad) s.nonfinite.fetch_add(bad, std::memory_order_relaxed);
  s.out_peak_accum = peak;
  s.out_peak.store(peak, std::memory_order_relaxed);
  tap(s, left, right, frames);
  // After the timed span: the page's notes and its silence flag cost a few loads, and aren't the engine's cost.
  follow_voices(s, *s.live->engine);
  s.engine_silent.store(s.live->engine->silent(), std::memory_order_relaxed);

  // Only blocks with sound count: an idle device's near-zero blocks would hide what a note costs.
  if (!was_silent || !s.live->engine->silent()) {
    Stats& st = s.stats[preset];
    const double took = std::chrono::duration<double>(t1 - t0).count();
    const double audio = double(frames) / s.sample_rate;
    st.blocks.store(st.blocks.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
    st.seconds.store(st.seconds.load(std::memory_order_relaxed) + took, std::memory_order_relaxed);
    st.audio_seconds.store(st.audio_seconds.load(std::memory_order_relaxed) + audio, std::memory_order_relaxed);
    if (took / audio > st.worst.load(std::memory_order_relaxed)) st.worst.store(took / audio, std::memory_order_relaxed);
    if (took >= audio) st.overruns.store(st.overruns.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
  }
  s.last_frames.store(frames, std::memory_order_relaxed);

  s.frames_since_callback += frames;
  if (s.wanted_preset.load(std::memory_order_relaxed) != preset || s.frames_since_callback >= std::uint64_t(2 * s.sample_rate)) want_callback = true;
  if (want_callback) {
    s.frames_since_callback = 0;
    s.host->request_callback(s.host);   // thread-safe by CLAP's contract, and the only way to reach the main thread
  }
  return CLAP_PROCESS_CONTINUE;
}

// Builds the wanted preset's engine and hands it to the audio thread, if one is due and none is waiting. Main thread:
// on_main_thread, and a frame that is waiting on a preset load.
bool build_wanted(Plugin& s) {
  delete s.retired.exchange(nullptr, std::memory_order_acq_rel);
  const int wanted = s.wanted_preset.load(std::memory_order_relaxed);
  if (!s.active || !(s.force_build || wanted != s.built_preset) || s.incoming.load(std::memory_order_acquire)) return false;
  Slot* fresh = build_slot(s, wanted, s.apply_shadow);
  if (!fresh) return false;
  s.incoming.store(fresh, std::memory_order_release);
  s.newest = fresh;
  s.built_preset = wanted;
  s.apply_shadow = false;
  s.force_build = false;
  return true;
}

void plugin_on_main_thread(const clap_plugin_t* p) {
  Plugin& s = *self(p);
  bool rescan = s.rescan_pending.exchange(false);
  if (build_wanted(s)) rescan = true;
  if (rescan && s.host_params && s.host_params->rescan) s.host_params->rescan(s.host, CLAP_PARAM_RESCAN_VALUES);
  write_stats(s);
}

bool plugin_activate(const clap_plugin_t* p, double sample_rate, std::uint32_t, std::uint32_t max_frames) {
  Plugin& s = *self(p);
  s.sample_rate = sample_rate;
  s.max_block = max_frames ? int(std::min<std::uint32_t>(max_frames, kEngineMaxBlock)) : kEngineMaxBlock;
  s.events.reserve(kMaxEvents + 1);
  const int wanted = s.wanted_preset.load(std::memory_order_relaxed);
  s.live = build_slot(s, wanted, s.apply_shadow);
  if (!s.live) return false;
  s.newest = s.live;
  s.live_tag.store(s.live->generation << 8 | std::uint64_t(s.live->preset), std::memory_order_release);
  s.built_preset = wanted;
  s.apply_shadow = false;
  s.force_build = false;
  s.active = true;
  s.rescan_pending.store(true);
  s.host->request_callback(s.host);
  return true;
}

void plugin_deactivate(const clap_plugin_t* p) {
  Plugin& s = *self(p);
  s.active = false;
  write_stats(s);
  s.newest = nullptr;
  s.live_tag.store(0, std::memory_order_release);
  delete s.live;
  s.live = nullptr;
  delete s.incoming.exchange(nullptr);
  delete s.retired.exchange(nullptr);
}

bool plugin_init(const clap_plugin_t* p) {
  Plugin& s = *self(p);
  s.host_params = static_cast<const clap_host_params_t*>(s.host->get_extension(s.host, CLAP_EXT_PARAMS));
  const Rows& t = rows();
  s.shadow = std::make_unique<std::atomic<double>[]>(t.all.size());
  for (std::size_t m = 0; m < t.all.size(); ++m) s.shadow[m].store(t.default_v[m], std::memory_order_relaxed);
  s.preset_v.resize(std::size_t(kBankSize));
  s.session.sent.assign(t.all.size(), NAN);
  s.session.gesture_open.assign(t.all.size(), false);
  s.wanted_preset.store(default_preset(), std::memory_order_relaxed);
  return true;
}

void plugin_destroy(const clap_plugin_t* p) {
  Plugin* s = self(p);
  delete s;
}

bool plugin_start_processing(const clap_plugin_t*) { return true; }
void plugin_stop_processing(const clap_plugin_t*) {}
void plugin_reset(const clap_plugin_t* p) {
  Plugin& s = *self(p);
  if (!s.live) return;
  s.live->engine->reset();
  forget_voices(s);
  forget_pitch(s.pitch);
  std::fill_n(s.live->counter, kMaxVoices, std::uint64_t(0));
  for (auto& voice : s.live->last) std::fill_n(voice, kMaxRouteKeys, 0.0);
}

// ---- params ----------------------------------------------------------------------------------------------------
std::uint32_t params_count(const clap_plugin_t*) { return std::uint32_t(1 + rows().count); }

bool params_get_info(const clap_plugin_t*, std::uint32_t index, clap_param_info_t* info) {
  if (index >= std::uint32_t(1 + rows().count)) return false;
  std::memset(info, 0, sizeof *info);
  info->flags = CLAP_PARAM_IS_AUTOMATABLE;
  if (index == 0) {
    info->id = kPresetId;
    info->flags |= CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM;
    std::snprintf(info->name, sizeof info->name, "Preset");
    info->min_value = 0;
    info->max_value = kBankSize - 1;
    info->default_value = default_preset();
    return true;
  }
  const Rows& t = rows();
  const int i = int(index) - 1;
  const horde::ParamInfo& row = *t.row[i];
  info->id = row.id;
  if (stepped(row)) info->flags |= CLAP_PARAM_IS_STEPPED;
  std::snprintf(info->name, sizeof info->name, "%s", row.key);
  info->min_value = 0;
  info->max_value = stepped(row) ? double(row.steps - 1) : 1.0;
  info->default_value = v_to_host(row, t.default_v[std::size_t(t.at[i])]);
  return true;
}

bool params_get_value(const clap_plugin_t* p, clap_id id, double* out) {
  Plugin& s = *self(p);
  if (id == kPresetId) { *out = s.wanted_preset.load(std::memory_order_relaxed); return true; }
  const int m = manifest_index(id);
  if (m < 0 || rows().host_of[std::size_t(m)] < 0) return false;
  *out = v_to_host(*rows().all[std::size_t(m)], s.shadow[m].load(std::memory_order_relaxed));
  return true;
}

bool params_value_to_text(const clap_plugin_t*, clap_id id, double value, char* out, std::uint32_t size) {
  if (id == kPresetId) {
    std::snprintf(out, size, "%s", kBank[std::clamp(int(std::lround(value)), 0, kBankSize - 1)].name);
    return true;
  }
  const int m = manifest_index(id);
  if (m < 0 || rows().host_of[std::size_t(m)] < 0) return false;
  const horde::ParamInfo& row = *rows().all[std::size_t(m)];
  if (stepped(row)) std::snprintf(out, size, "%d", int(std::lround(value)));
  else std::snprintf(out, size, "%.3g", std::round(horde::value_of(row, value) * 1e9) / 1e9 + 0.0);   // nine places first: no -2e-15 on a panel
  return true;
}

bool params_text_to_value(const clap_plugin_t*, clap_id id, const char* text, double* out) {
  if (id == kPresetId) {
    for (int i = 0; i < kBankSize; ++i) {
      if (!std::strcmp(kBank[i].name, text)) { *out = i; return true; }
    }
    return false;
  }
  return false;   // a continuous row's text is in its units, and no inverse law is exposed; the slider still works
}

void params_flush(const clap_plugin_t* p, const clap_input_events_t* in, const clap_output_events_t* out) {
  Plugin& s = *self(p);
  s.events.clear();
  const bool edits_waiting = s.edit_tail.load(std::memory_order_relaxed) != s.edit_head.load(std::memory_order_acquire);
  drain_edits(s, out);
  const std::uint32_t n = in ? in->size(in) : 0;
  if (s.live) s.pitch.span_clock = s.live->engine->sample_clock();
  for (std::uint32_t i = 0; i < n; ++i) translate(s, in->get(in, i), 0);
  // A flush has no samples: any bend it carried is simply the bend from now on.
  std::copy(std::begin(s.pitch.bend), std::end(s.pitch.bend), std::begin(s.pitch.bend_at_start));
  s.pitch.n_changes = 0;
  if (s.live) s.live->engine->flush(s.events.data(), s.events.size());
  else s.apply_shadow = n > 0 || edits_waiting || s.apply_shadow;   // inactive: the next build must keep these values
  if (s.wanted_preset.load(std::memory_order_relaxed) != s.built_preset) s.host->request_callback(s.host);
}

const clap_plugin_params_t kParams = {params_count, params_get_info, params_get_value, params_value_to_text, params_text_to_value, params_flush};

// ---- ports -----------------------------------------------------------------------------------------------------
std::uint32_t audio_count(const clap_plugin_t*, bool is_input) { return is_input ? 0 : 1; }
bool audio_get(const clap_plugin_t*, std::uint32_t index, bool is_input, clap_audio_port_info_t* info) {
  if (is_input || index != 0) return false;
  std::memset(info, 0, sizeof *info);
  info->id = 0;
  std::snprintf(info->name, sizeof info->name, "Output");
  info->flags = CLAP_AUDIO_PORT_IS_MAIN;
  info->channel_count = 2;
  info->port_type = CLAP_PORT_STEREO;
  info->in_place_pair = CLAP_INVALID_ID;
  return true;
}
const clap_plugin_audio_ports_t kAudioPorts = {audio_count, audio_get};

std::uint32_t note_count(const clap_plugin_t*, bool is_input) { return is_input ? 1 : 0; }
bool note_get(const clap_plugin_t*, std::uint32_t index, bool is_input, clap_note_port_info_t* info) {
  if (!is_input || index != 0) return false;
  std::memset(info, 0, sizeof *info);
  info->id = 0;
  info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
  info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
  std::snprintf(info->name, sizeof info->name, "Notes");
  return true;
}
const clap_plugin_note_ports_t kNotePorts = {note_count, note_get};

// ---- state -----------------------------------------------------------------------------------------------------
// Every row's v as `preset` loads, which the saved state is written against. Read when a build loads the preset; a
// save before any build loads it here, off the audio thread (load_state isn't real-time, RT-1).
const std::vector<double>& preset_values(Plugin& s, int preset) {
  std::vector<double>& loaded = s.preset_v[std::size_t(preset)];
  if (!loaded.empty()) return loaded;
  const Rows& t = rows();
  auto engine = horde::make_engine();
  if (engine && engine->prepare(44100, 64) && engine->load_state(kBank[preset].state).ok) {
    for (const auto* row : t.all) loaded.push_back(engine->param_v(row->id));
    cap_polyphony(loaded);   // as build_slot loads it
  } else {
    loaded = t.default_v;   // a preset that won't load is never built either; its defaults stand in
  }
  return loaded;
}

bool state_save(const clap_plugin_t* p, const clap_ostream_t* stream) {
  Plugin& s = *self(p);
  const Rows& t = rows();
  preview::bridge::SavedState saved;
  saved.preset = s.wanted_preset.load(std::memory_order_relaxed);
  std::vector<std::uint32_t> ids;
  std::vector<double> current;
  for (std::size_t m = 0; m < t.all.size(); ++m) {
    if (t.all[m]->reserved) continue;
    ids.push_back(t.all[m]->id);
    current.push_back(s.shadow[m].load(std::memory_order_relaxed));
  }
  const std::vector<double>& all_loaded = preset_values(s, saved.preset);
  std::vector<double> loaded;
  for (std::size_t m = 0; m < t.all.size(); ++m)
    if (!t.all[m]->reserved) loaded.push_back(all_loaded[m]);
  saved.values = preview::bridge::differences(ids, current, loaded);
  const std::string text = preview::bridge::write_state(saved);
  for (std::size_t done = 0; done < text.size();) {
    const std::int64_t wrote = stream->write(stream, text.data() + done, text.size() - done);
    if (wrote <= 0) return false;
    done += std::size_t(wrote);
  }
  s.session.state = text;   // the page hears of a save once, never per frame (section 3)
  s.session.state_due = true;
  return true;
}

bool state_load(const clap_plugin_t* p, const clap_istream_t* stream) {
  Plugin& s = *self(p);
  std::string text;
  char buffer[4096];
  for (;;) {
    const std::int64_t got = stream->read(stream, buffer, sizeof buffer);
    if (got < 0) return false;
    if (got == 0) break;
    text.append(buffer, std::size_t(got));
    if (text.size() > 1 << 20) return false;
  }
  preview::bridge::SavedState saved;
  if (!preview::bridge::read_state(text, saved)) return false;
  const int preset = std::clamp(saved.preset, 0, kBankSize - 1);
  s.wanted_preset.store(preset, std::memory_order_relaxed);
  // The preset's own values first, then the rows the state says differ: a row it doesn't name is the preset's.
  const Rows& t = rows();
  const std::vector<double>& loaded = preset_values(s, preset);
  for (std::size_t m = 0; m < t.all.size(); ++m) s.shadow[m].store(loaded[m], std::memory_order_relaxed);
  for (const auto& [id, v] : saved.values) {
    const int m = manifest_index(id);
    if (m >= 0 && !t.all[std::size_t(m)]->reserved) s.shadow[m].store(std::clamp(v, 0.0, 1.0), std::memory_order_relaxed);
  }
  s.apply_shadow = true;
  s.force_build = true;
  s.session.state = text;
  s.session.state_due = true;
  if (s.active) s.host->request_callback(s.host);
  return true;
}
const clap_plugin_state_t kState = {state_save, state_load};

// ---- GUI 3's bridge (B628) -------------------------------------------------------------------------------------
const preview::bridge::KeyMap& keymap() {
  static const preview::bridge::KeyMap map(rows().all);
  return map;
}

const std::vector<std::string>& preset_names() {
  static const std::vector<std::string> names = [] {
    std::vector<std::string> out;
    for (int i = 0; i < kBankSize; ++i) out.emplace_back(kBank[i].name);
    return out;
  }();
  return names;
}

// Moves waiting messages into the edit queue, whole and in order: a message split across two blocks could be heard
// half-applied, and its ack would no longer mean "all of it".
void pump(Plugin& s) {
  Session& ss = s.session;
  bool moved = false;
  while (!ss.waiting.empty()) {
    const std::vector<Edit>& batch = ss.waiting.front();
    const std::uint32_t head = s.edit_head.load(std::memory_order_relaxed);
    if (kEditQueue - (head - s.edit_tail.load(std::memory_order_acquire)) < batch.size()) break;
    for (std::size_t k = 0; k < batch.size(); ++k) s.edits[(head + std::uint32_t(k)) % kEditQueue] = batch[k];
    s.edit_head.store(head + std::uint32_t(batch.size()), std::memory_order_release);
    ss.waiting.pop_front();
    moved = true;
  }
  // Live stops calling process on a silent track in some setups, and a flush still delivers the edits.
  if (moved && s.host_params && s.host_params->request_flush) s.host_params->request_flush(s.host);
}

void note_refused(std::vector<std::string>& list, const std::string& what, bool& due) {
  if (list.size() >= 64 || std::find(list.begin(), list.end(), what) != list.end()) return;   // a report, not a log
  list.push_back(what);
  due = true;
}

// Everything the page reads only on change goes again in the next frame that may carry it.
void resend_all(Plugin& s) {
  Session& ss = s.session;
  std::fill(ss.sent.begin(), ss.sent.end(), NAN);
  ss.sent_preset = -1;
  ss.sent_plugin = false;
}

// A new page in a new window: it knows nothing, and hasn't said hello.
void open_session(Plugin& s) {
  Session& ss = s.session;
  ss.want = preview::bridge::Want{};
  ss.load = Load{};
  ss.saw_hello = ss.hello_due = false;
  std::fill(ss.gesture_open.begin(), ss.gesture_open.end(), false);
  resend_all(s);
}

// Whether the pending load completes in this frame, `acked` read with acquire. A preset load may build its engine here.
bool load_completes(Plugin& s, std::int64_t acked) {
  Load& load = s.session.load;
  if (!load.pending || acked < load.seq) return false;
  if (!load.preset) return true;
  if (!s.active) {
    // No engine to wait for: the shadow takes the preset's own values, which the next activation builds from.
    const std::vector<double>& loaded = preset_values(s, load.index);
    for (std::size_t m = 0; m < loaded.size(); ++m) s.shadow[m].store(loaded[m], std::memory_order_relaxed);
    s.apply_shadow = true;
    return true;
  }
  build_wanted(s);   // the audio thread takes it at its next block
  const std::uint64_t tag = s.live_tag.load(std::memory_order_acquire);
  return (tag >> 8) > load.after && int(tag & 0xFF) == load.index;
}

void bridge_receive(Plugin& s, std::string_view json) {
  using namespace preview::bridge;
  Session& ss = s.session;
  const Message m = parse_message(json, keymap());
  if (m.kind == Kind::Invalid) {
    note_refused(ss.types, "(unreadable: " + m.error + ")", ss.refused_due);
    return;   // no seq to acknowledge
  }
  for (const auto& key : m.unknown) note_refused(ss.unknown, key, ss.refused_due);
  for (const auto& key : m.reserved) note_refused(ss.reserved, key, ss.refused_due);
  const Rows& t = rows();
  std::vector<Edit> batch;
  switch (m.kind) {
    case Kind::Set:
      for (const auto& value : m.values) {
        const horde::ParamInfo& row = keymap().row(value.row);
        const int at = manifest_index(row.id);
        const int host = t.host_of[std::size_t(at)];
        Edit e;
        e.id = row.id;
        e.value = host >= 0 ? v_to_host(row, std::clamp(value.v, 0.0, 1.0)) : value.v;
        // Inside a page gesture the host already has its begin; outside one, each change is its own.
        e.bracket = !ss.gesture_open[std::size_t(at)];
        batch.push_back(e);
      }
      break;
    case Kind::Gesture:
      if (m.gesture_row >= 0) {
        const horde::ParamInfo& row = keymap().row(m.gesture_row);
        const int at = manifest_index(row.id);
        ss.gesture_open[std::size_t(at)] = m.gesture_begin;
        if (t.host_of[std::size_t(at)] >= 0) {
          Edit e;
          e.kind = m.gesture_begin ? Edit::Begin : Edit::End;
          e.id = row.id;
          batch.push_back(e);
        }
        if (!ss.events.empty()) ss.events += ',';
        ss.events += "{\"type\":\"gesture\",\"key\":" + quoted(m.gesture_key) + ",\"phase\":\"" + (m.gesture_begin ? "begin" : "end") + "\"}";
      }
      break;
    case Kind::Want: ss.want = m.want; break;
    case Kind::Preset:
      if (m.index >= 0) {
        const int index = std::clamp(m.index, 0, kBankSize - 1);
        // Loading the preset already playing still loads it: its own values replace any edits.
        if (index == s.wanted_preset.load(std::memory_order_relaxed)) s.force_build = true;
        ss.load = Load{true, true, m.seq, index, s.generations};
        Edit e;
        e.id = kPresetId;
        e.value = index;
        batch.push_back(e);
      }
      break;
    case Kind::Plugin:
      break;   // an old page's Voices: acknowledged and held, since Polyphony is the one voice control
    case Kind::Hello:
      ss.saw_hello = true;
      ss.hello_due = true;
      ss.mismatch = check_hello(m, t.hash, t.text);
      // A hello during a preset load still waits for that preset's engine; it only moves the seq to answer.
      if (ss.load.pending && ss.load.preset) ss.load.seq = m.seq;
      else ss.load = Load{true, false, m.seq, -1, 0};
      break;
    case Kind::Held:
      if (m.unknown_type) note_refused(ss.types, m.type, ss.refused_due);
      break;
    case Kind::Invalid: break;
  }
  Edit ack;
  ack.kind = Edit::Ack;
  ack.seq = m.seq;
  batch.push_back(ack);
  ss.waiting.push_back(std::move(batch));
  pump(s);
}

const std::string& bridge_frame(Plugin& s) {
  using namespace preview::bridge;
  pump(s);
  Session& ss = s.session;
  const Rows& t = rows();
  Pieces p;
  // The ack first, with acquire: every shadow value read below is then at least as new as the seq it confirms, so the
  // page never reads an older value under a newer ack and takes it for the engine's own change (device.js, stale).
  const std::int64_t acked = s.acked.load(std::memory_order_acquire);
  const bool completes = load_completes(s, acked);
  const bool holding = ss.load.pending && !completes;
  p.ack = holding ? std::min(acked, ss.load.seq - 1) : acked;
  if (completes) {
    resend_all(s);   // every row, the presets and `plugin`, in this one frame
    ss.load.pending = false;
  }

  FrameInputs in;
  if (s.newest && s.newest->engine->read_gui_snapshot(*ss.frame)) in.frame = ss.frame.get();
  in.engine_silent = s.engine_silent.load(std::memory_order_relaxed);
  NoteSlot notes[kMaxVoices];
  for (int i = 0; i < kMaxVoices; ++i) {
    const std::uint64_t w = s.voice_note[i].load(std::memory_order_relaxed);
    // Four places undo the 16-bit packing: a note-on of 0.85 reads back 0.85, not 0.8500038.
    const double velocity = std::round(double((w >> 16) & 0xFFFF) / 65535.0 * 1e4) / 1e4;
    notes[i] = {(w & 1) != 0, int((w >> 8) & 0xFF), velocity, (w & 2) != 0};
  }
  in.notes = notes;
  for (int o = 0; o < 2; ++o) {
    const int m = keymap().find(o == 0 ? "osc1.unison" : "osc2.unison");
    if (m < 0) continue;
    const horde::ParamInfo& row = keymap().row(m);
    const double v = s.shadow[manifest_index(row.id)].load(std::memory_order_relaxed);
    in.unison[o] = int(std::lround(row.lo + std::round(v * double(row.steps > 1 ? row.steps - 1 : 0))));
  }
  float scope[kScopeWindow / kScopeStep];
  if (ss.want.summed) {
    // Acquire: every sample counted in `written` is in the ring. The newest window, every kScopeStep-th sample.
    const std::uint64_t written = s.scope_written.load(std::memory_order_acquire);
    if (written >= kScopeWindow) {
      for (std::uint64_t k = 0; k < kScopeWindow / kScopeStep; ++k) scope[k] = s.scope_ring[(written - kScopeWindow + k * kScopeStep) & (kScopeRing - 1)];
      in.scope = scope;
      in.scope_n = int(kScopeWindow / kScopeStep);
      in.scope_rate = s.sample_rate / double(kScopeStep);
    }
  }
  in.out_tapped = true;
  in.out_peak = s.out_peak.load(std::memory_order_relaxed);
  s.out_peak_reset.store(true, std::memory_order_relaxed);
  p.observe = observe_json(in, ss.want);

  std::vector<std::pair<std::string, double>> changed;
  for (std::size_t m = 0; m < t.all.size() && !holding; ++m) {
    if (t.all[m]->reserved) continue;
    const double v = s.shadow[m].load(std::memory_order_relaxed);
    if (v == ss.sent[m]) continue;   // NaN never equals: a row never sent is sent
    ss.sent[m] = v;
    changed.emplace_back(t.all[m]->key, v);
  }
  if (!changed.empty()) p.params = &changed;
  const int preset = s.wanted_preset.load(std::memory_order_relaxed);
  if (preset != ss.sent_preset && !holding) {
    p.preset_names = &preset_names();
    p.preset_index = preset;
    ss.sent_preset = preset;
  }
  static const std::string build = kBuildLabel;
  if (!ss.sent_plugin && !holding) {
    p.plugin_build = &build;   // GUI 3's page keeps `plugin`; the label rides there as well as in the hello reply
    ss.sent_plugin = true;
  }
  if (ss.state_due) p.state = &ss.state;
  if (ss.hello_due && completes) {
    p.build = &build;
    p.engine_hash = &t.hash;
    p.mismatch = &ss.mismatch;
  }
  if (ss.refused_due) {
    p.refused_unknown = &ss.unknown;
    p.refused_reserved = &ss.reserved;
    p.refused_types = &ss.types;
  }
  if (!ss.events.empty()) p.events = "[" + ss.events + "]";
  ss.out = snapshot_json(p);
  ss.events.clear();
  if (completes) ss.hello_due = false;
  ss.refused_due = ss.state_due = false;
  return ss.out;
}

void ext_receive(const clap_plugin_t* p, const char* json) { bridge_receive(*self(p), json ? json : ""); }
const char* ext_frame(const clap_plugin_t* p) { return bridge_frame(*self(p)).c_str(); }
double ext_engine_v(const clap_plugin_t* p, std::uint32_t id) {
  Plugin& s = *self(p);
  return s.live ? s.live->engine->param_v(id) : NAN;
}
bool ext_note_pitch(const clap_plugin_t* p, std::int32_t note_id, double* played, double* sounding) {
  Plugin& s = *self(p);
  if (!s.live) return false;
  static horde::VoiceView view;   // tens of KB; tests only, on one thread
  const int n = std::min(s.live->engine->voice_count(), kMaxVoices);
  for (int i = 0; i < n; ++i) {
    if (!s.live->engine->observe_voice(i, view) || !view.sounding || view.note_id != note_id) continue;
    *played = view.played_pitch;
    *sounding = view.sounding_pitch;
    return true;
  }
  return false;
}
const preview_bridge_ext kBridgeExt = {ext_receive, ext_frame, ext_engine_v, ext_note_pitch};

const void* plugin_get_extension(const clap_plugin_t*, const char* id) {
  if (!std::strcmp(id, CLAP_EXT_PARAMS)) return &kParams;
  if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &kAudioPorts;
  if (!std::strcmp(id, CLAP_EXT_NOTE_PORTS)) return &kNotePorts;
  if (!std::strcmp(id, CLAP_EXT_STATE)) return &kState;
  if (!std::strcmp(id, CLAP_EXT_GUI)) return preview::gui_extension();
  if (!std::strcmp(id, kPreviewBridgeExt)) return &kBridgeExt;
  return nullptr;
}

// ---- factory ---------------------------------------------------------------------------------------------------
const char* const kFeatures[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER, CLAP_PLUGIN_FEATURE_STEREO, nullptr};
const clap_plugin_descriptor_t kDescriptor = {
    // The brand is one word, "Mindlathe", in every host (autonomous doctrine CONVENTIONS, Decision 80); the AU already said so.
    CLAP_VERSION, "com.mindlathe.horde-engine-preview", "horde ENGINE PREVIEW", "Mindlathe", "", "", "", "0.0.1",
    "A throwaway test build of horde's new engine: sound and CPU only. " HORDE_PREVIEW_BUILD_LABEL, kFeatures};

std::uint32_t factory_count(const clap_plugin_factory_t*) { return 1; }
const clap_plugin_descriptor_t* factory_descriptor(const clap_plugin_factory_t*, std::uint32_t index) { return index == 0 ? &kDescriptor : nullptr; }
const clap_plugin_t* factory_create(const clap_plugin_factory_t*, const clap_host_t* host, const char* id) {
  if (std::strcmp(id, kDescriptor.id)) return nullptr;
  auto* s = new Plugin;
  s->host = host;
  s->plugin.desc = &kDescriptor;
  s->plugin.plugin_data = s;
  s->plugin.init = plugin_init;
  s->plugin.destroy = plugin_destroy;
  s->plugin.activate = plugin_activate;
  s->plugin.deactivate = plugin_deactivate;
  s->plugin.start_processing = plugin_start_processing;
  s->plugin.stop_processing = plugin_stop_processing;
  s->plugin.reset = plugin_reset;
  s->plugin.process = plugin_process;
  s->plugin.get_extension = plugin_get_extension;
  s->plugin.on_main_thread = plugin_on_main_thread;
  return &s->plugin;
}
const clap_plugin_factory_t kFactory = {factory_count, factory_descriptor, factory_create};

}  // namespace

namespace preview {
int bank_size() { return kBankSize; }
const char* preset_name(int index) { return kBank[index].name; }
int row_count() { return rows().count; }
clap_id row_id(int row) { return rows().row[row]->id; }
const char* row_key(int row) { return rows().row[row]->key; }
int row_steps(int row) { return stepped(*rows().row[row]) ? int(rows().row[row]->steps) : 0; }
int preset(void* plugin) { return static_cast<Plugin*>(plugin)->wanted_preset.load(std::memory_order_relaxed); }
double row_host_value(void* plugin, int row) {
  return v_to_host(*rows().row[row], static_cast<Plugin*>(plugin)->shadow[rows().at[row]].load(std::memory_order_relaxed));
}
void row_text(int row, double host_value, char* out, std::size_t size) {
  params_value_to_text(nullptr, rows().row[row]->id, host_value, out, std::uint32_t(size));
}
void stats(void* plugin, int preset_index, double* mean_pct, double* worst_pct, unsigned long long* overruns) {
  const Stats& st = static_cast<Plugin*>(plugin)->stats[preset_index];
  const double audio = st.audio_seconds.load(std::memory_order_relaxed);
  *mean_pct = audio > 0 ? 100 * st.seconds.load(std::memory_order_relaxed) / audio : 0;
  *worst_pct = 100 * st.worst.load(std::memory_order_relaxed);
  *overruns = st.overruns.load(std::memory_order_relaxed);
}
void edit(void* plugin, clap_id id, double host_value) {
  Plugin& s = *static_cast<Plugin*>(plugin);
  const std::uint32_t head = s.edit_head.load(std::memory_order_relaxed);
  if (head - s.edit_tail.load(std::memory_order_acquire) >= kEditQueue) return;   // full: a slider's next move will do
  Edit e;
  e.id = id;
  e.value = host_value;
  s.edits[head % kEditQueue] = e;
  s.edit_head.store(head + 1, std::memory_order_release);
  // Live stops calling process on a silent track in some setups, and a flush still delivers the edit.
  if (s.host_params && s.host_params->request_flush) s.host_params->request_flush(s.host);
}
namespace {
const horde::ParamInfo& polyphony_row() { return *horde::find_param("voice.polyphony"); }
}  // namespace
int polyphony_lo() { return int(polyphony_row().lo); }
int polyphony_hi() { return int(polyphony_row().hi); }
int polyphony(void* plugin) {
  const horde::ParamInfo& row = polyphony_row();
  const double v = static_cast<Plugin*>(plugin)->shadow[manifest_index(row.id)].load(std::memory_order_relaxed);
  return int(row.lo + std::round(v * double(row.steps - 1)));
}
void set_polyphony(void* plugin, int voices) {
  const horde::ParamInfo& row = polyphony_row();
  edit(plugin, row.id, (std::clamp(voices, polyphony_lo(), polyphony_hi()) - row.lo) / double(row.steps - 1));   // in v: not a host parameter
}
void*& gui_slot(void* plugin) { return static_cast<Plugin*>(plugin)->gui; }
void bridge_receive(void* plugin, const char* json) { ::bridge_receive(*static_cast<Plugin*>(plugin), json ? json : ""); }
const char* bridge_frame(void* plugin) { return ::bridge_frame(*static_cast<Plugin*>(plugin)).c_str(); }
// Only for a page that never says hello: one that does gets the full set only when it asks, in the frame that answers
// its load, so nothing here can make it adopt values it didn't ask for.
void bridge_resend(void* plugin) {
  Plugin& s = *static_cast<Plugin*>(plugin);
  if (!s.session.saw_hello) resend_all(s);
}
void bridge_open(void* plugin) { open_session(*static_cast<Plugin*>(plugin)); }
const char* build_label() { return kBuildLabel; }
}  // namespace preview

extern "C" {
bool preview_entry_init(const char*) { return true; }
void preview_entry_deinit(void) {}
const void* preview_entry_get_factory(const char* factory_id) { return std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) ? nullptr : &kFactory; }
}
