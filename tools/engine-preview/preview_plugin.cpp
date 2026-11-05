// horde ENGINE PREVIEW (B624): a thin CLAP around the clean-room engine, so the human can play it in Live and we
// can see what it costs there. Throwaway on purpose: it isn't the B398 shell and owes none of ADR-186 §5's rules.
// A small AppKit window (preview_gui.mm), and no filters, effects, modulation matrix, glide or pitch bend: the engine
// has none of them yet.
//
// Three choices worth keeping in mind before changing this file:
// - At most 64 host parameters. Live gives a plug-in with up to 64 a panel of sliders automatically; above that the
//   panel opens empty and is filled by touching controls in the plug-in's window, which this plug-in doesn't have.
// - A preset change builds a whole new engine on the main thread and hands it to the audio thread. load_state and
//   prepare aren't real-time (RT-1), and the swap keeps the audio thread free of them. Sounding notes stop at the swap.
// - The saved chunk is the preset's index and the exposed values, not the engine's state text. It stays small (large
//   chunks make hosts drop state silently), and it never reads the engine from the main thread while audio runs.
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
#include <memory>
#include <string>
#include <vector>

#include "bank.h"
#include "flush_to_zero.h"
#include "horde_engine.h"
#include "preview_gui.h"

namespace {

// kPresetId and kVoicesId (preview_gui.h) sit above the engine's id range: IF-11 keeps its ids below 0xB00000.
using preview::kMaxVoicesParam;
using preview::kPresetId;
using preview::kVoicesId;
constexpr int kMaxRows = 62;   // + Preset and Voices = Live's 64
constexpr int kMaxVoices = 64;
constexpr int kMaxRouteKeys = 8;
constexpr int kMaxTargets = 16;
constexpr int kMaxEvents = 4096;
constexpr int kEngineMaxBlock = 4096;   // the most prepare accepts (DT-2)
constexpr int kDefaultVoices = 4;       // starts light: 8 voices of a heavy patch can overrun every block
constexpr std::uint32_t kEditQueue = 1024;   // a power of two: the window's edits waiting for the audio thread
constexpr const char* kDefaultPreset = "Trance jitter";   // one of the lighter patches in the 2026-11-05 worst-block run
constexpr const char* kStateMagic = "horde-engine-preview 1";

// The rows on the panel. A key the engine doesn't have is skipped, and the smoke test prints how many stood.
const char* const kExposedKeys[] = {
    "voice.mode", "voice.gravity", "env1.attack", "env1.decay", "env1.sustain", "env1.release",
    "mix.osc1", "mix.osc2", "mix.sub", "mix.noise", "out.level", "voice.quality", "quality",
    "osc1.on", "osc1.unison", "osc1.detune", "osc1.coupling", "osc1.driftDepth", "osc1.width", "osc1.crossMod", "osc1.feedback",
    "osc1.blade1.on", "osc1.blade1.mode", "osc1.blade1.wave", "osc1.blade1.cut", "osc1.blade1.position", "osc1.blade1.width",
    "osc1.blade1.edges", "osc1.blade1.depth", "osc1.blade1.fmDepth", "osc1.blade1.modRate",
    "osc1.blade2.on", "osc1.blade2.mode", "osc1.blade2.wave", "osc1.blade2.cut", "osc1.blade2.position", "osc1.blade2.width",
    "osc1.blade2.edges", "osc1.blade2.depth", "osc1.blade2.fmDepth", "osc1.blade2.modRate",
    "osc2.on", "osc2.unison", "osc2.detune", "osc2.coupling", "sub.on", "sub.drive", "noise.on",
};

struct Rows {
  const horde::ParamInfo* row[kMaxRows] = {};
  double default_v[kMaxRows] = {};
  int count = 0;
};

const Rows& rows() {
  static const Rows table = [] {
    Rows t;
    for (const char* key : kExposedKeys) {
      const horde::ParamInfo* p = horde::find_param(key);
      if (p && !p->reserved && t.count < kMaxRows) t.row[t.count++] = p;
    }
    auto probe = horde::make_engine();   // the manifest's defaults as v, which ParamInfo gives only in units
    for (int i = 0; i < t.count; ++i) t.default_v[i] = probe->param_v(t.row[i]->id);
    return t;
  }();
  return table;
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
  Target targets[kMaxTargets];
  int n_targets = 0;
  std::uint32_t key_id[kMaxRouteKeys] = {};
  int n_keys = 0;
  double env_velocity[2] = {1, 1};
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

  std::atomic<int> wanted_preset{0};
  std::atomic<int> voices{kDefaultVoices};
  std::atomic<double> shadow[kMaxRows];   // what the host reads: v per exposed row

  Slot* live = nullptr;                   // audio thread while active
  std::atomic<Slot*> incoming{nullptr};   // main to audio
  std::atomic<Slot*> retired{nullptr};    // audio to main, deleted there
  std::atomic<bool> rescan_pending{false};

  // One writer (the main thread) and one reader (whichever thread processes or flushes), so no lock on the audio path.
  struct Edit { clap_id id; double value; };
  Edit edits[kEditQueue] = {};
  std::atomic<std::uint32_t> edit_head{0}, edit_tail{0};
  void* gui = nullptr;

  std::vector<horde::Event> events;
  std::int32_t next_note_id = 1 << 24;   // above any id a host's own counter reaches in a session
  std::atomic<std::uint64_t> nonfinite{0};
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
  std::fprintf(f, "# sample_rate %.0f, last block %u frames, voices %d, non-finite samples zeroed %llu\n", s.sample_rate, s.last_frames.load(std::memory_order_relaxed),
               s.voices.load(std::memory_order_relaxed), (unsigned long long)s.nonfinite.load(std::memory_order_relaxed));
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

Slot* build_slot(Plugin& s, int preset, bool keep_shadow) {
  auto slot = std::make_unique<Slot>();
  slot->preset = preset;
  slot->engine = horde::make_engine();
  if (!slot->engine || !slot->engine->prepare(s.sample_rate, s.max_block)) return nullptr;
  if (!slot->engine->load_state(kBank[preset].state).ok) return nullptr;
  slot->ci = std::uint64_t(std::max(1L, std::lround(s.sample_rate / 1500)));   // PA-3

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

  std::vector<horde::Event> setup;
  const Rows& t = rows();
  for (int i = 0; i < t.count; ++i) {
    if (keep_shadow) {
      horde::Event e;
      e.type = horde::EventType::Param;
      e.param_id = t.row[i]->id;
      e.value = s.shadow[i].load(std::memory_order_relaxed);
      setup.push_back(e);
    } else {
      s.shadow[i].store(slot->engine->param_v(t.row[i]->id), std::memory_order_relaxed);
    }
  }
  horde::Event cap;
  cap.type = horde::EventType::VoiceCap;
  cap.integer = s.voices.load(std::memory_order_relaxed);
  cap.flag = true;   // a full cap takes the oldest note, so a chord over the cap still plays its newest notes
  setup.push_back(cap);
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
  horde::Event e;
  if (id == kVoicesId) {
    const int count = std::clamp(int(std::lround(value)), 1, kMaxVoicesParam);
    s.voices.store(count, std::memory_order_relaxed);
    e.type = horde::EventType::VoiceCap;
    e.integer = count;
    e.flag = true;
  } else {
    const Rows& t = rows();
    int i = 0;
    while (i < t.count && t.row[i]->id != id) ++i;
    if (i == t.count) return;
    const double v = std::clamp(host_to_v(*t.row[i], value), 0.0, 1.0);
    s.shadow[i].store(v, std::memory_order_relaxed);
    e.type = horde::EventType::Param;
    e.param_id = id;
    e.value = v;
  }
  if (s.events.size() < kMaxEvents) s.events.push_back(e);
}

// The window's edits become parameter changes here, and the host hears of them, so its automation and panel keep up.
void drain_edits(Plugin& s, const clap_output_events_t* out) {
  std::uint32_t tail = s.edit_tail.load(std::memory_order_relaxed);
  const std::uint32_t head = s.edit_head.load(std::memory_order_acquire);
  for (; tail != head; ++tail) {
    const Plugin::Edit& edit = s.edits[tail % kEditQueue];
    set_param(s, edit.id, edit.value);
    if (!out) continue;
    clap_event_param_gesture_t gesture{};
    gesture.header = {sizeof gesture, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_GESTURE_BEGIN, 0};
    gesture.param_id = edit.id;
    out->try_push(out, &gesture.header);
    clap_event_param_value_t value{};
    value.header = {sizeof value, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
    value.param_id = edit.id;
    value.note_id = -1;
    value.port_index = -1;
    value.channel = -1;
    value.key = -1;
    value.value = edit.value;
    out->try_push(out, &value.header);
    gesture.header.type = CLAP_EVENT_PARAM_GESTURE_END;
    out->try_push(out, &gesture.header);
  }
  s.edit_tail.store(tail, std::memory_order_release);
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
      if (s.events.size() < kMaxEvents) s.events.push_back(e);
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
      } else if (status == 0x80 || status == 0x90) {
        e.type = horde::EventType::NoteOff;
        e.key = std::int16_t(d1);
      } else if (status == 0xB0 && d1 == 64) {
        e.type = horde::EventType::Pedal;
        e.value = d2;
      } else if (status == 0xB0 && d1 == 120) {
        e.type = horde::EventType::AllSoundOff;
      } else if (status == 0xB0 && d1 == 123) {
        e.type = horde::EventType::AllNotesOff;
      } else if (status == 0xD0) {
        e.type = horde::EventType::ChannelPressure;
        e.value = d1 / 127.0;
      } else {
        return;   // pitch bend included: the pitch system that would read it isn't built
      }
      if (s.events.size() < kMaxEvents) s.events.push_back(e);
    }
  }
  for (std::size_t i = before; i < s.events.size(); ++i) s.events[i].sample = sample;
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
    engine.process(s.events.data(), s.events.size(), nullptr, 0, left + pos, right + pos, int(span));
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
    want_callback = true;
  }
  if (proc->audio_outputs_count == 0 || proc->frames_count == 0) return CLAP_PROCESS_CONTINUE;
  float* left = proc->audio_outputs[0].data32[0];
  float* right = proc->audio_outputs[0].data32[1];
  const std::uint32_t frames = proc->frames_count;
  if (!s.live) {
    std::fill_n(left, frames, 0.0f);
    std::fill_n(right, frames, 0.0f);
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
  for (std::uint32_t i = 0; i < frames; ++i) {
    if (!std::isfinite(left[i])) { left[i] = 0; ++bad; }
    if (!std::isfinite(right[i])) { right[i] = 0; ++bad; }
  }
  if (bad) s.nonfinite.fetch_add(bad, std::memory_order_relaxed);

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

void plugin_on_main_thread(const clap_plugin_t* p) {
  Plugin& s = *self(p);
  delete s.retired.exchange(nullptr, std::memory_order_acq_rel);
  const int wanted = s.wanted_preset.load(std::memory_order_relaxed);
  bool rescan = s.rescan_pending.exchange(false);
  if (s.active && (s.force_build || wanted != s.built_preset) && !s.incoming.load(std::memory_order_acquire)) {
    if (Slot* fresh = build_slot(s, wanted, s.apply_shadow)) {
      s.incoming.store(fresh, std::memory_order_release);
      s.built_preset = wanted;
      s.apply_shadow = false;
      s.force_build = false;
      rescan = true;
    }
  }
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
  delete s.live;
  s.live = nullptr;
  delete s.incoming.exchange(nullptr);
  delete s.retired.exchange(nullptr);
}

bool plugin_init(const clap_plugin_t* p) {
  Plugin& s = *self(p);
  s.host_params = static_cast<const clap_host_params_t*>(s.host->get_extension(s.host, CLAP_EXT_PARAMS));
  const Rows& t = rows();
  for (int i = 0; i < t.count; ++i) s.shadow[i].store(t.default_v[i], std::memory_order_relaxed);
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
  std::fill_n(s.live->counter, kMaxVoices, std::uint64_t(0));
  for (auto& voice : s.live->last) std::fill_n(voice, kMaxRouteKeys, 0.0);
}

// ---- params ----------------------------------------------------------------------------------------------------
std::uint32_t params_count(const clap_plugin_t*) { return std::uint32_t(2 + rows().count); }

bool params_get_info(const clap_plugin_t*, std::uint32_t index, clap_param_info_t* info) {
  if (index >= std::uint32_t(2 + rows().count)) return false;
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
  if (index == 1) {
    info->id = kVoicesId;
    info->flags |= CLAP_PARAM_IS_STEPPED;
    std::snprintf(info->name, sizeof info->name, "Voices");
    info->min_value = 1;
    info->max_value = kMaxVoicesParam;
    info->default_value = kDefaultVoices;
    return true;
  }
  const Rows& t = rows();
  const int i = int(index) - 2;
  const horde::ParamInfo& row = *t.row[i];
  info->id = row.id;
  if (stepped(row)) info->flags |= CLAP_PARAM_IS_STEPPED;
  std::snprintf(info->name, sizeof info->name, "%s", row.key);
  info->min_value = 0;
  info->max_value = stepped(row) ? double(row.steps - 1) : 1.0;
  info->default_value = v_to_host(row, t.default_v[i]);
  return true;
}

bool params_get_value(const clap_plugin_t* p, clap_id id, double* out) {
  Plugin& s = *self(p);
  if (id == kPresetId) { *out = s.wanted_preset.load(std::memory_order_relaxed); return true; }
  if (id == kVoicesId) { *out = s.voices.load(std::memory_order_relaxed); return true; }
  const Rows& t = rows();
  for (int i = 0; i < t.count; ++i) {
    if (t.row[i]->id == id) { *out = v_to_host(*t.row[i], s.shadow[i].load(std::memory_order_relaxed)); return true; }
  }
  return false;
}

bool params_value_to_text(const clap_plugin_t*, clap_id id, double value, char* out, std::uint32_t size) {
  if (id == kPresetId) {
    std::snprintf(out, size, "%s", kBank[std::clamp(int(std::lround(value)), 0, kBankSize - 1)].name);
    return true;
  }
  if (id == kVoicesId) { std::snprintf(out, size, "%d", int(std::lround(value))); return true; }
  const Rows& t = rows();
  for (int i = 0; i < t.count; ++i) {
    if (t.row[i]->id != id) continue;
    const horde::ParamInfo& row = *t.row[i];
    if (stepped(row)) std::snprintf(out, size, "%d", int(std::lround(value)));
    else {
      const double x = horde::value_of(row, value);
      std::snprintf(out, size, "%.3g", std::fabs(x) < 1e-9 ? 0.0 : x);   // a law's rounding shouldn't read as -2e-15
    }
    return true;
  }
  return false;
}

bool params_text_to_value(const clap_plugin_t*, clap_id id, const char* text, double* out) {
  if (id == kPresetId) {
    for (int i = 0; i < kBankSize; ++i) {
      if (!std::strcmp(kBank[i].name, text)) { *out = i; return true; }
    }
    return false;
  }
  if (id == kVoicesId) { *out = std::atof(text); return true; }
  return false;   // a continuous row's text is in its units, and no inverse law is exposed; the slider still works
}

void params_flush(const clap_plugin_t* p, const clap_input_events_t* in, const clap_output_events_t* out) {
  Plugin& s = *self(p);
  s.events.clear();
  const bool edits_waiting = s.edit_tail.load(std::memory_order_relaxed) != s.edit_head.load(std::memory_order_acquire);
  drain_edits(s, out);
  const std::uint32_t n = in ? in->size(in) : 0;
  for (std::uint32_t i = 0; i < n; ++i) translate(s, in->get(in, i), 0);
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
bool state_save(const clap_plugin_t* p, const clap_ostream_t* stream) {
  Plugin& s = *self(p);
  std::string text = std::string(kStateMagic) + "\n";
  text += "preset " + std::to_string(s.wanted_preset.load(std::memory_order_relaxed)) + "\n";
  text += "voices " + std::to_string(s.voices.load(std::memory_order_relaxed)) + "\n";
  const Rows& t = rows();
  char line[64];
  for (int i = 0; i < t.count; ++i) {
    std::snprintf(line, sizeof line, "%u %.17g\n", t.row[i]->id, s.shadow[i].load(std::memory_order_relaxed));
    text += line;
  }
  for (std::size_t done = 0; done < text.size();) {
    const std::int64_t wrote = stream->write(stream, text.data() + done, text.size() - done);
    if (wrote <= 0) return false;
    done += std::size_t(wrote);
  }
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
  if (text.rfind(kStateMagic, 0) != 0) return false;   // no promise across builds: an unknown chunk leaves the default
  const Rows& t = rows();
  std::size_t at = text.find('\n');
  while (at != std::string::npos && at + 1 < text.size()) {
    const std::size_t end = text.find('\n', at + 1);
    const std::string line = text.substr(at + 1, end == std::string::npos ? std::string::npos : end - at - 1);
    int preset = 0, count = 0;
    unsigned id = 0;
    double v = 0;
    if (std::sscanf(line.c_str(), "preset %d", &preset) == 1) s.wanted_preset.store(std::clamp(preset, 0, kBankSize - 1), std::memory_order_relaxed);
    else if (std::sscanf(line.c_str(), "voices %d", &count) == 1) s.voices.store(std::clamp(count, 1, kMaxVoicesParam), std::memory_order_relaxed);
    else if (std::sscanf(line.c_str(), "%u %lf", &id, &v) == 2) {
      for (int i = 0; i < t.count; ++i) {
        if (t.row[i]->id == id) s.shadow[i].store(std::clamp(v, 0.0, 1.0), std::memory_order_relaxed);
      }
    }
    at = end;
  }
  s.apply_shadow = true;
  s.force_build = true;
  if (s.active) s.host->request_callback(s.host);
  return true;
}
const clap_plugin_state_t kState = {state_save, state_load};

const void* plugin_get_extension(const clap_plugin_t*, const char* id) {
  if (!std::strcmp(id, CLAP_EXT_PARAMS)) return &kParams;
  if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &kAudioPorts;
  if (!std::strcmp(id, CLAP_EXT_NOTE_PORTS)) return &kNotePorts;
  if (!std::strcmp(id, CLAP_EXT_STATE)) return &kState;
  if (!std::strcmp(id, CLAP_EXT_GUI)) return preview::gui_extension();
  return nullptr;
}

// ---- factory ---------------------------------------------------------------------------------------------------
const char* const kFeatures[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER, CLAP_PLUGIN_FEATURE_STEREO, nullptr};
const clap_plugin_descriptor_t kDescriptor = {
    CLAP_VERSION, "com.mind-lathe.horde.engine-preview", "horde ENGINE PREVIEW", "Mind Lathe", "", "", "", "0.0.1",
    "A throwaway test build of horde's new engine: sound and CPU only.", kFeatures};

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
int voices(void* plugin) { return static_cast<Plugin*>(plugin)->voices.load(std::memory_order_relaxed); }
double row_host_value(void* plugin, int row) {
  return v_to_host(*rows().row[row], static_cast<Plugin*>(plugin)->shadow[row].load(std::memory_order_relaxed));
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
  s.edits[head % kEditQueue] = {id, host_value};
  s.edit_head.store(head + 1, std::memory_order_release);
  // Live stops calling process on a silent track in some setups, and a flush still delivers the edit.
  if (s.host_params && s.host_params->request_flush) s.host_params->request_flush(s.host);
}
void*& gui_slot(void* plugin) { return static_cast<Plugin*>(plugin)->gui; }
}  // namespace preview

extern "C" {
bool preview_entry_init(const char*) { return true; }
void preview_entry_deinit(void) {}
const void* preview_entry_get_factory(const char* factory_id) { return std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) ? nullptr : &kFactory; }
}
