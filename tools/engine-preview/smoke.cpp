// Not run by ./verify: it needs the clean-room engine, which isn't in this public repo or in CI yet (B568). build.sh runs it.
//
// Drives the built CLAP through its own interface, the way a host does, and holds its output against the engine's
// own offline render (horde_offline_render, written by the builder), bit for bit. The offline render shares none of
// the plug-in's code, so a pass says the shell plays the bank as the renders do: the levelled state, the routes and
// the note order all arrive intact. DT-2 is why bit-exact is the right bar: output doesn't depend on block splits.
//   smoke <horde-engine-preview.clap> [<plant.clap> [<plant-map.clap> [<plant-morph.clap>]]]
// Controls that must fail: a different preset's render must differ, and the plant build (no routes, and the retired
// Voices limit of 4 put back) must miss on a routed preset and sound 4 voices of a 12-note chord. The second plant
// shifts the bridge's key map by one row, and the full-row mapping audit must read it red. Without them, an
// always-equal comparison would read as a pass.
#include <clap/clap.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "bank.h"
#include "bridge_ext.h"
#include "filter_core.h"
#include "flush_to_zero.h"
#include "horde_engine.h"
#include "morph_core.h"

namespace {

int failures = 0;
void check(bool ok, const std::string& what) {
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
  if (!ok) ++failures;
}

bool callback_requested = false;
const void* host_extension(const clap_host_t*, const char*) { return nullptr; }
void host_noop(const clap_host_t*) {}
void host_callback(const clap_host_t*) { callback_requested = true; }
const clap_host_t kHost = {CLAP_VERSION, nullptr, "engine-preview smoke", "", "", "1", host_extension, host_noop, host_noop, host_callback};

struct Events {
  std::vector<clap_event_note_t> notes;
  std::vector<clap_event_param_value_t> params;
  std::vector<clap_event_midi_t> midis;
  std::vector<const clap_event_header_t*> order;
  void midi(std::uint32_t time, std::uint8_t status, std::uint8_t d1, std::uint8_t d2) {
    clap_event_midi_t e{};
    e.header = {sizeof e, time, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI, 0};
    e.port_index = 0;
    e.data[0] = status; e.data[1] = d1; e.data[2] = d2;
    midis.push_back(e);
  }
  void note(bool on, std::uint32_t time, std::int32_t id, std::int16_t key, double velocity) {
    clap_event_note_t e{};
    e.header = {sizeof e, time, CLAP_CORE_EVENT_SPACE_ID, std::uint16_t(on ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF), 0};
    e.note_id = id; e.port_index = 0; e.channel = 0; e.key = key; e.velocity = velocity;
    notes.push_back(e);
  }
  void param(clap_id id, double value) {
    clap_event_param_value_t e{};
    e.header = {sizeof e, 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0};
    e.param_id = id; e.note_id = -1; e.port_index = -1; e.channel = -1; e.key = -1; e.value = value;
    params.push_back(e);
  }
  void seal() {
    order.clear();
    for (auto& e : params) order.push_back(&e.header);
    for (auto& e : notes) order.push_back(&e.header);
    for (auto& e : midis) order.push_back(&e.header);
    // A host's list is in time order; stable, so events at one time keep the order they were added in.
    std::stable_sort(order.begin(), order.end(), [](const clap_event_header_t* a, const clap_event_header_t* b) { return a->time < b->time; });
  }
};
std::uint32_t events_size(const clap_input_events_t* l) { return std::uint32_t(static_cast<Events*>(l->ctx)->order.size()); }
const clap_event_header_t* events_get(const clap_input_events_t* l, std::uint32_t i) { return static_cast<Events*>(l->ctx)->order[i]; }
bool out_push(const clap_output_events_t*, const clap_event_header_t*) { return true; }

struct Loaded {
  void* library = nullptr;
  const clap_plugin_t* plugin = nullptr;
  const clap_plugin_params_t* params = nullptr;
  const clap_plugin_state_t* state = nullptr;
  bool open(const char* path) {
    // On macOS a built .clap is a bundle directory, and dlopen wants the binary inside it. The plant is a bare file.
    std::string binary = path;
    struct stat info;
    if (stat(path, &info) == 0 && S_ISDIR(info.st_mode)) {
      const std::string name = binary.substr(binary.rfind('/') + 1);
      binary += "/Contents/MacOS/" + name.substr(0, name.rfind('.'));
    }
    library = dlopen(binary.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!library) { std::printf("dlopen: %s\n", dlerror()); return false; }
    const auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(library, "clap_entry"));
    if (!entry || !entry->init(path)) return false;
    const auto* factory = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (!factory || factory->get_plugin_count(factory) != 1) return false;
    plugin = factory->create_plugin(factory, &kHost, factory->get_plugin_descriptor(factory, 0)->id);
    if (!plugin || !plugin->init(plugin)) return false;
    params = static_cast<const clap_plugin_params_t*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
    state = static_cast<const clap_plugin_state_t*>(plugin->get_extension(plugin, CLAP_EXT_STATE));
    return params && state && plugin->activate(plugin, 44100, 1, 512) && plugin->start_processing(plugin);
  }
};

void process(const clap_plugin_t* plugin, Events& events, float* l, float* r, std::uint32_t frames) {
  events.seal();
  clap_input_events_t in{&events, events_size, events_get};
  clap_output_events_t out{nullptr, out_push};
  float* channels[2] = {l, r};
  clap_audio_buffer_t buffer{};
  buffer.data32 = channels;
  buffer.channel_count = 2;
  clap_process_t proc{};
  proc.frames_count = frames;
  proc.audio_outputs = &buffer;
  proc.audio_outputs_count = 1;
  proc.in_events = &in;
  proc.out_events = &out;
  plugin->process(plugin, &proc);
}

constexpr std::int16_t kChord[4] = {60, 64, 67, 71};
constexpr double kVelocity = 0.85;
constexpr std::uint32_t kFrames = 44100 * 2, kOff = 44100 * 3 / 2;   // 2 s, released at 1.5 s

// Chooses the preset and lets the "main thread" build it, so the next block is a fresh engine's first.
void fresh(Loaded& p, int preset) {
  // Through another preset first: only a change builds an engine, and the offline render starts at a fresh engine's
  // clock 0, so replaying on the engine already loaded would differ for the wrong reason (its clock has moved on).
  float silent[2][64];
  const int steps[2] = {preset == 0 ? 1 : 0, preset};
  for (int step = 0; step < 2; ++step) {
    Events choose;
    choose.param(0xB00001, steps[step]);
    process(p.plugin, choose, silent[0], silent[1], 64);
    if (callback_requested) { callback_requested = false; p.plugin->on_main_thread(p.plugin); }
    if (step == 0) {   // take the first engine, so the second build isn't waiting on it
      Events none;
      process(p.plugin, none, silent[0], silent[1], 64);
      if (callback_requested) { callback_requested = false; p.plugin->on_main_thread(p.plugin); }
    }
  }
}

// Plays the chord from the fresh engine's first sample.
std::vector<float> play(Loaded& p, int preset, const std::vector<std::uint32_t>& blocks) {
  fresh(p, preset);
  std::vector<float> l(kFrames), r(kFrames);
  std::uint32_t pos = 0;
  std::size_t b = 0;
  while (pos < kFrames) {
    std::uint32_t n = blocks[b++ % blocks.size()];
    if (n > kFrames - pos) n = kFrames - pos;
    Events e;
    if (pos == 0) for (int k = 0; k < 4; ++k) e.note(true, 0, k + 1, kChord[k], kVelocity);
    if (kOff >= pos && kOff < pos + n) for (int k = 0; k < 4; ++k) e.note(false, kOff - pos, k + 1, kChord[k], 0);
    process(p.plugin, e, l.data() + pos, r.data() + pos, n);
    pos += n;
  }
  if (callback_requested) { callback_requested = false; p.plugin->on_main_thread(p.plugin); }
  std::vector<float> interleaved(2 * kFrames);
  for (std::uint32_t i = 0; i < kFrames; ++i) { interleaved[2 * i] = l[i]; interleaved[2 * i + 1] = r[i]; }
  return interleaved;
}

// ---- 10.3's glide, written again here from tests/wasm/item15b.mjs, not taken from the plug-in --------------------
// A gliding note moves from `from` to f along ln f(t) = ln f + (ln from - ln f) e^(-3t/T). The plug-in carries a moving
// pitch through IF-2's float stream, so the reference rounds each run value through float the same way, and a run ends
// at the first sample whose float value is the note's own float pitch, where one event at f exactly takes over (the
// render harness ends its double runs within 1e-9 of ln f, which a float can't resolve). The law is unchanged.
struct RefNote { long at = 0; double f = 0, from = 0; };   // from 0: on pitch
double ref_twelve_tet(int key) { return 440 * std::pow(2.0, (key - 69) / 12.0); }
double ref_law(const RefNote& n, long sample, double rate, double glide_s) {
  if (!n.from) return n.f;
  const double t = double(sample - n.at) / rate;
  return std::exp(std::log(n.f) + (std::log(n.from) - std::log(n.f)) * std::exp(-3 * t / glide_s));
}
// The pitch a note's voice reads at `sample`, as the plug-in feeds it: the float-rounded law, then f exactly.
double ref_played(const RefNote& n, long sample, double rate, double glide_s) {
  if (!n.from) return n.f;
  for (long k = n.at; k <= sample; ++k)
    if (float(ref_law(n, k, rate, glide_s)) == float(n.f)) return n.f;
  return double(float(ref_law(n, sample, rate, glide_s)));
}

// The smoke chord's notes as 10.3 glides them: all four start at sample 0, each after the first gliding from the one
// before it (held, so it glides; round 49), the first on pitch.
std::vector<RefNote> chord_glides(int preset) {
  std::vector<RefNote> notes;
  for (int k = 0; k < 4; ++k) {
    RefNote n;
    n.f = ref_twelve_tet(kChord[k]);
    if (k > 0 && kBank[preset].glide_s > 0) {
      const double from = ref_law(notes.back(), 0, 44100, kBank[preset].glide_s);
      if (from != n.f) n.from = from;
    }
    notes.push_back(n);
  }
  return notes;
}

std::vector<float> offline(int preset) {
  struct Timed { long sample; std::string text; };
  std::vector<Timed> events;
  char line[200];
  for (int k = 0; k < 4; ++k) {
    std::snprintf(line, sizeof line, "{\"sample\":0,\"time_s\":0,\"type\":\"note_on\",\"note_id\":%d,\"channel\":0,\"key\":%d,\"velocity\":%.17g}", k + 1, kChord[k], kVelocity);
    events.push_back({0, line});
  }
  const std::vector<RefNote> glides = chord_glides(preset);
  for (int k = 0; k < 4; ++k) {
    const RefNote& n = glides[std::size_t(k)];
    if (!n.from) continue;
    std::string run = "{\"sample\":0,\"time_s\":0,\"type\":\"note_pitch\",\"note_id\":" + std::to_string(k + 1) + ",\"rate\":44100,\"hz_per_sample\":[";
    long end = 0;
    for (; end < long(kFrames); ++end) {
      const float hz = float(ref_law(n, end, 44100, kBank[preset].glide_s));
      if (hz == float(n.f)) break;
      std::snprintf(line, sizeof line, "%s%.17g", end ? "," : "", double(hz));
      run += line;
    }
    events.push_back({0, run + "]}"});
    std::snprintf(line, sizeof line, "{\"sample\":%ld,\"time_s\":%.17g,\"type\":\"note_pitch\",\"note_id\":%d,\"hz\":%.17g}", end, end / 44100.0, k + 1, n.f);
    if (end < long(kFrames)) events.push_back({end, line});
  }
  for (int k = 0; k < 4; ++k) {
    std::snprintf(line, sizeof line, "{\"sample\":%u,\"time_s\":1.5,\"type\":\"note_off\",\"note_id\":%d,\"channel\":0,\"key\":%d}", kOff, k + 1, kChord[k]);
    events.push_back({long(kOff), line});
  }
  std::stable_sort(events.begin(), events.end(), [](const Timed& a, const Timed& b) { return a.sample < b.sample; });
  std::string phrase = "{\"events\":[";
  for (std::size_t i = 0; i < events.size(); ++i) phrase += (i ? "," : "") + events[i].text;
  phrase += "]}";
  std::string routes = "[";
  for (int r = 0; r < kBank[preset].n_routes; ++r) {
    std::snprintf(line, sizeof line, "%s{\"env\":%d,\"key\":\"%s\",\"octaves\":%.17g}", r ? "," : "", kBank[preset].routes[r].env, kBank[preset].routes[r].key, kBank[preset].routes[r].octaves);
    routes += line;
  }
  routes += "]";
  std::vector<float> out(2 * kFrames);
  int rc = -1;
  // The plug-in runs under flush-to-zero (RT-4), so the reference must too, or tails could differ in subnormals.
  horde2::underFlushToZero([&] { rc = horde_offline_render(kBank[preset].state, phrase.c_str(), routes.c_str(), 44100, out.data(), kFrames); });
  if (rc != 0) { std::printf("offline render refused: %d\n", rc); ++failures; }
  return out;
}

std::size_t first_difference(const std::vector<float>& a, const std::vector<float>& b) {
  for (std::size_t i = 0; i < a.size(); ++i)
    if (a[i] != b[i]) return i;
  return a.size();
}

double peak(const std::vector<float>& a) {
  double m = 0;
  for (float x : a) m = std::max(m, double(std::fabs(x)));
  return m;
}

// The `ack` a snapshot carries: section 3's first member, so it is always where the bridge writes it.
long long ack_of(const std::string& frame) {
  const std::string tag = "{\"ack\":";
  return frame.rfind(tag, 0) == 0 ? std::atoll(frame.c_str() + tag.size()) : -1;
}
bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }
// How many rows a snapshot's `params` carries (0 when it has none): keys are quoted and values are numbers.
int params_rows(const std::string& frame) {
  const std::size_t at = frame.find("\"params\":{");
  if (at == std::string::npos) return 0;
  const std::size_t end = frame.find('}', at);
  int n = 0;
  for (std::size_t i = frame.find("\":", at + 10); i < end; i = frame.find("\":", i + 2)) ++n;
  return n;
}
// A row's v in a snapshot's `params`, or NaN.
double param_in(const std::string& frame, const char* key) {
  const std::size_t at = frame.find("\"params\":{");
  if (at == std::string::npos) return NAN;
  const std::size_t k = frame.find(std::string("\"") + key + "\":", at);
  return k == std::string::npos ? NAN : std::strtod(frame.c_str() + k + std::strlen(key) + 3, nullptr);
}

void one_block(const clap_plugin_t* plugin) {
  float l[256], r[256];
  Events none;
  process(plugin, none, l, r, 256);
}

// How many voices a snapshot's `observe` lists: the sounding ones (section 3).
int voices_in(const std::string& frame) {
  int n = 0;
  for (std::size_t i = frame.find("{\"level\":"); i != std::string::npos; i = frame.find("{\"level\":", i + 1)) ++n;
  return n;
}

// Sends one message with the next seq and runs blocks and frames until it is acknowledged (a load needs its engine
// swap); returns the acknowledging frame, or "" if it never came.
std::string send_until_acked(const clap_plugin_t* plugin, const preview_bridge_ext* bridge, long long& seq, const std::string& body) {
  const long long mine = ++seq;
  bridge->receive(plugin, ("{\"seq\":" + std::to_string(mine) + "," + body + "}").c_str());
  for (int round = 0; round < 6; ++round) {
    one_block(plugin);
    const std::string frame = bridge->frame(plugin);
    if (ack_of(frame) >= mine) return frame;
  }
  return "";
}

// v for a stepped row of consecutive integers (polyphony): the option holding `count`.
double v_of_count(const horde::ParamInfo& row, int count) { return (count - row.lo) / double(row.steps - 1); }

// Holds a chord of `n` notes, reads how many voices the frame says sound, then releases it and lets the tail end.
int voices_for_chord(const clap_plugin_t* plugin, const preview_bridge_ext* bridge, int n) {
  float l[512], r[512];
  Events on;
  for (int k = 0; k < n; ++k) on.note(true, 0, 3000 + k, std::int16_t(48 + k), 0.8);
  process(plugin, on, l, r, 512);
  for (int b = 0; b < 8; ++b) {
    Events none;
    process(plugin, none, l, r, 512);
  }
  const int sounding = voices_in(bridge->frame(plugin));
  Events off;
  for (int k = 0; k < n; ++k) off.note(false, 0, 3000 + k, std::int16_t(48 + k), 0);
  process(plugin, off, l, r, 512);
  for (int b = 0; b < 2000; ++b) {
    Events none;
    process(plugin, none, l, r, 512);
  }
  return sounding;
}

// The full-row mapping audit: every non-reserved row set through the bridge to a v unlike its current one must read
// back that exact v in the engine and in the next frame's params. A stepped row goes to its last option and its first
// (first then last when it already sits on the last). Returns the keys that failed.
std::vector<std::string> audit_rows(const clap_plugin_t* plugin, const preview_bridge_ext* bridge, long long& seq, int& passed) {
  std::vector<std::string> failed;
  passed = 0;
  bridge->frame(plugin);   // whatever was pending goes now, so each frame below carries only the row just set
  for (std::size_t i = 0; i < horde::manifest_size(); ++i) {
    const horde::ParamInfo& row = horde::manifest_param(i);
    if (row.reserved) continue;
    const double now = bridge->engine_v(plugin, row.id);
    std::vector<double> targets;
    if (row.law == horde::Law::Step && row.steps > 1) targets = now == 1.0 ? std::vector<double>{0.0, 1.0} : std::vector<double>{1.0, 0.0};
    else targets = {now == 0.375 ? 0.625 : 0.375};
    bool ok = true;
    for (double v : targets) {
      char body[200];
      std::snprintf(body, sizeof body, "\"type\":\"set\",\"values\":{\"%s\":%.17g}", row.key, v);
      const std::string frame = send_until_acked(plugin, bridge, seq, body);
      ok = ok && bridge->engine_v(plugin, row.id) == v && param_in(frame, row.key) == v;
    }
    if (ok) ++passed;
    else failed.emplace_back(row.key);
  }
  return failed;
}

// ---- pitch (B632) -------------------------------------------------------------------------------------------------
// A note's pitch input (IF-2) after the last block, or NaN when no voice sounds it.
double played_of(const clap_plugin_t* plugin, const preview_bridge_ext* bridge, std::int32_t id, double* sounding = nullptr) {
  double played = NAN, heard = NAN;
  if (!bridge->note_pitch(plugin, id, &played, &heard)) return NAN;
  if (sounding) *sounding = heard;
  return played;
}

void block_of(const clap_plugin_t* plugin, Events& e, std::uint32_t frames) {
  static float l[4096], r[4096];
  process(plugin, e, l, r, frames);
}

void release_and_wait(const clap_plugin_t* plugin, const std::vector<std::pair<std::int32_t, std::int16_t>>& notes) {
  Events off;
  for (const auto& [id, key] : notes) off.note(false, 0, id, key, 0);
  block_of(plugin, off, 512);
  for (int b = 0; b < 2000; ++b) {
    Events none;
    block_of(plugin, none, 512);
  }
}

// One note held, bent fully up on its MIDI channel, then back to the centre. Returns the pitch input after the bend
// (NaN if the note isn't sounding) and, through `centred`, after the return.
double bent_pitch(const clap_plugin_t* plugin, const preview_bridge_ext* bridge, double* centred, double* sounding_bent) {
  Events on;
  on.note(true, 0, 7001, 60, 0.8);
  block_of(plugin, on, 256);
  Events up;
  up.midi(10, 0xE0, 0x7F, 0x7F);   // 16383: +2 semitones exactly
  block_of(plugin, up, 256);
  const double bent = played_of(plugin, bridge, 7001, sounding_bent);
  Events centre;
  centre.midi(0, 0xE0, 0x00, 0x40);   // 8192
  block_of(plugin, centre, 256);
  *centred = played_of(plugin, bridge, 7001);
  release_and_wait(plugin, {{7001, 60}});
  return bent;
}

// A: key 60 at sample 0; B: key 64 at 500, A held; C: key 67 at 1000, A and B held. C's pitch input is read after
// every one-sample block for `n` samples, from its note-on. The preset must have just been loaded, so no earlier note
// is there to glide from.
std::vector<double> glide_run(const clap_plugin_t* plugin, const preview_bridge_ext* bridge, int n) {
  Events a;
  a.note(true, 0, 8001, 60, 0.8);
  block_of(plugin, a, 500);
  Events b;
  b.note(true, 0, 8002, 64, 0.8);
  block_of(plugin, b, 500);
  std::vector<double> seen;
  for (int k = 0; k < n; ++k) {
    Events e;
    if (k == 0) e.note(true, 0, 8003, 67, 0.8);
    block_of(plugin, e, 1);
    seen.push_back(played_of(plugin, bridge, 8003));
  }
  release_and_wait(plugin, {{8001, 60}, {8002, 64}, {8003, 67}});
  return seen;
}

// A chord of three on one sample, nothing held: notes 2 and 3 read after every one-sample block for `n` samples.
void chord_run(const clap_plugin_t* plugin, const preview_bridge_ext* bridge, int n, std::vector<double> seen[3]) {
  for (int k = 0; k < n; ++k) {
    Events e;
    if (k == 0)
      for (int j = 0; j < 3; ++j) e.note(true, 0, 8101 + j, std::int16_t(60 + 4 * j - (j == 2 ? 1 : 0)), 0.8);   // 60, 64, 67
    block_of(plugin, e, 1);
    for (int j = 0; j < 3; ++j) seen[j].push_back(played_of(plugin, bridge, 8101 + j));
  }
  release_and_wait(plugin, {{8101, 60}, {8102, 64}, {8103, 67}});
}

// The first sample where `seen` and `want` differ, or -1.
long first_miss(const std::vector<double>& seen, const std::vector<double>& want) {
  for (std::size_t k = 0; k < seen.size(); ++k)
    if (!(seen[k] == want[k])) return long(k);
  return -1;
}

// ---- every row a host parameter (B632) ---------------------------------------------------------------------------
// Each host parameter but Preset set by a host param value event, to its last option and its first for a stepped row
// or to a value unlike its current one, must read back in the engine, in the host's readback and in the next frame.
// `shift` sends each event to the id `shift` places further on, the must-fail control. Returns the keys that failed.
std::vector<std::string> audit_host(const clap_plugin_t* plugin, const clap_plugin_params_t* params, const preview_bridge_ext* bridge, int shift, int& passed) {
  std::vector<std::string> failed;
  passed = 0;
  bridge->frame(plugin);
  // The engine rows only: Preset is first, and morph.x and morph.y (B639) reach the resolver, never the engine, so their
  // own check holds them (host_xy). The test-only filter's six (B660) come after them and have a check of their own.
  const std::uint32_t n = params->count(plugin) - 2 - preview::filter::kParams;
  for (std::uint32_t i = 1; i < n; ++i) {
    clap_param_info_t info{}, target{};
    params->get_info(plugin, i, &info);
    params->get_info(plugin, 1 + (i - 1 + std::uint32_t(shift)) % (n - 1), &target);
    const bool stepped = (info.flags & CLAP_PARAM_IS_STEPPED) != 0;
    double now = 0;
    params->get_value(plugin, info.id, &now);
    std::vector<double> values;
    if (stepped) values = now == info.max_value ? std::vector<double>{0.0, info.max_value} : std::vector<double>{info.max_value, 0.0};
    else values = {now == 0.375 ? 0.625 : 0.375};
    bool ok = true;
    for (double host : values) {
      Events e;
      e.param(target.id, host);
      block_of(plugin, e, 256);
      const double v = stepped ? host / info.max_value : host;
      double back = -1;
      params->get_value(plugin, info.id, &back);
      ok = ok && bridge->engine_v(plugin, info.id) == v && back == host && param_in(bridge->frame(plugin), info.name) == v;
    }
    if (ok) ++passed;
    else failed.emplace_back(info.name);
  }
  return failed;
}

// ---- the output tap (GN-4, B632) -----------------------------------------------------------------------------------
// Asks for the summed output, plays a chord and holds the frame's window to the output the host received: the newest
// 2048 samples, (L + R) / 2 in float, every 16th. Returns the first sample that differs (-1: none); `loud` says
// whether the output it was held to had sound.
long scope_miss(const clap_plugin_t* plugin, const preview_bridge_ext* bridge, long long& seq, bool* loud, int* points) {
  (void)send_until_acked(plugin, bridge, seq, "\"type\":\"want\",\"osc\":[1],\"members\":false,\"cycles\":false,\"summed\":true");
  std::vector<float> heard;
  float l[512], r[512];
  for (int b = 0; b < 8; ++b) {
    Events e;
    if (b == 0)
      for (int k = 0; k < 4; ++k) e.note(true, 0, 9001 + k, kChord[k], kVelocity);
    process(plugin, e, l, r, 512);
    for (int i = 0; i < 512; ++i) heard.push_back(0.5f * (l[i] + r[i]));
  }
  const std::string frame = bridge->frame(plugin);
  release_and_wait(plugin, {{9001, kChord[0]}, {9002, kChord[1]}, {9003, kChord[2]}, {9004, kChord[3]}});
  (void)send_until_acked(plugin, bridge, seq, "\"type\":\"want\",\"osc\":[1,2],\"members\":false,\"cycles\":false");
  std::vector<double> got;
  const std::size_t at = frame.find("\"summed\":{\"rate\":");
  const std::size_t list = at == std::string::npos ? std::string::npos : frame.find("\"samples\":[", at);
  if (list != std::string::npos) {
    const char* c = frame.c_str() + list + 11;
    while (*c && *c != ']') {
      char* end = nullptr;
      got.push_back(std::strtod(c, &end));
      c = *end == ',' ? end + 1 : end;
    }
  }
  *points = int(got.size());
  *loud = false;
  for (std::size_t k = 0; k < 128; ++k) {
    const float want = heard[heard.size() - 2048 + 16 * k];
    *loud = *loud || want != 0;
    if (k >= got.size() || float(got[k]) != want) return long(k);
  }
  return -1;
}

// ---- the morph (B636) ---------------------------------------------------------------------------------------------
// One message with the next seq, applied at the next block (the edit queue's drain). No wait: the caller runs blocks.
void post(const clap_plugin_t* plugin, const preview_bridge_ext* bridge, long long& seq, const std::string& body) {
  bridge->receive(plugin, ("{\"seq\":" + std::to_string(++seq) + "," + body + "}").c_str());
}
void blocks_of(const clap_plugin_t* plugin, int n, std::uint32_t frames = 512) {
  for (int b = 0; b < n; ++b) {
    float l[512], r[512];
    Events none;
    process(plugin, none, l, r, frames);
  }
}
// The morph back to D.2's defaults, applied by the next block (fresh() runs blocks before it builds its engine).
void morph_reset(const clap_plugin_t* plugin, const preview_bridge_ext* bridge, long long& seq) {
  post(plugin, bridge, seq, "\"type\":\"morph\",\"settings\":{\"morph.on\":0,\"morph.audible\":0,\"morph.law\":0.5,\"morph.glide\":0.04}");
  post(plugin, bridge, seq, "\"type\":\"position\",\"x\":0,\"y\":0");
  post(plugin, bridge, seq, "\"type\":\"modes\",\"modes\":{},\"counts\":{}");
  post(plugin, bridge, seq, "\"type\":\"waypoints\",\"key\":\"osc1.detune\",\"points\":[]");
  post(plugin, bridge, seq, "\"type\":\"lock\",\"key\":\"osc1.baseWave\",\"offsets\":null");
  post(plugin, bridge, seq, "\"type\":\"waypointLaw\",\"law\":\"tps\",\"clamp\":true");
}
std::string num17(double v) {
  char b[40];
  std::snprintf(b, sizeof b, "%.17g", v);
  return b;
}
// The test's corners: osc1.detune (Morphable, lin), out.level (Morphable, dB) and osc1.baseWave (Structural, in
// osc 1's kernel group) apart in every corner, everything else the preset's.
struct Corners { double detune[4], level[4], wave[4]; };
Corners test_corners() {
  const horde::ParamInfo* wave = horde::find_param("osc1.baseWave");
  const double n = wave && wave->steps > 1 ? double(wave->steps - 1) : 1;
  Corners c{};
  for (int k = 0; k < 4; ++k) {
    // Dyadic values and (below) dyadic positions: every product and sum of MO-9's blend is exact in any order, so the
    // engine's value is held to it with ==, no tolerance.
    c.detune[k] = 0.125 + 0.25 * k;
    c.level[k] = 0.5 + 0.0625 * ((k + 1) % 4);
    c.wave[k] = std::fmod(double(k + 1), n + 1) / n;
  }
  return c;
}
std::string corner_values(const Corners& c, int k) {
  return "{\"osc1.detune\":" + num17(c.detune[k]) + ",\"out.level\":" + num17(c.level[k]) + ",\"osc1.baseWave\":" + num17(c.wave[k]) + "}";
}
void post_corners(const clap_plugin_t* plugin, const preview_bridge_ext* bridge, long long& seq, const Corners& c) {
  for (int k = 0; k < 4; ++k) post(plugin, bridge, seq, "\"type\":\"corner\",\"index\":" + std::to_string(k) + ",\"values\":" + corner_values(c, k));
}
// After the morph's messages (or a reference's sets) at a fresh engine's first block: silent blocks while it settles,
// then the chord, 0.5 s, released at 0.3 s. Both sides of a comparison run exactly this, so their clocks agree.
std::vector<float> settle_and_play(const clap_plugin_t* plugin) {
  blocks_of(plugin, 4);
  constexpr std::uint32_t n = 22050, off = 13230;
  std::vector<float> out(2 * n);
  std::vector<float> l(n), r(n);
  for (std::uint32_t pos = 0; pos < n; pos += 128) {
    Events e;
    if (pos == 0) for (int k = 0; k < 4; ++k) e.note(true, 0, 9000 + k, kChord[k], kVelocity);
    if (off >= pos && off < pos + 128) for (int k = 0; k < 4; ++k) e.note(false, off - pos, 9000 + k, kChord[k], 0);
    process(plugin, e, l.data() + pos, r.data() + pos, std::min<std::uint32_t>(128, n - pos));
  }
  for (std::uint32_t i = 0; i < n; ++i) out[2 * i] = l[i], out[2 * i + 1] = r[i];
  blocks_of(plugin, 200);   // the tail ends, so the next comparison starts from silence
  return out;
}
// The corner `k` as the plug-in plays it with the morph off: the preset, then plain sets of that corner's values.
std::vector<float> corner_reference(Loaded& p, const preview_bridge_ext* bridge, long long& seq, int preset, const Corners& c, int k) {
  morph_reset(p.plugin, bridge, seq);
  fresh(p, preset);
  post(p.plugin, bridge, seq, "\"type\":\"set\",\"values\":" + corner_values(c, k));
  return settle_and_play(p.plugin);
}
// The morph on, at corner `k` exactly, under the law given as its v (0 Blend, 0.5 Quantum), with no glide.
std::vector<float> corner_morphed(Loaded& p, const preview_bridge_ext* bridge, long long& seq, int preset, const Corners& c, int k, double law) {
  morph_reset(p.plugin, bridge, seq);
  fresh(p, preset);
  post_corners(p.plugin, bridge, seq, c);
  post(p.plugin, bridge, seq, "\"type\":\"morph\",\"settings\":{\"morph.glide\":0,\"morph.law\":" + num17(law) + ",\"morph.on\":1}");
  post(p.plugin, bridge, seq, "\"type\":\"position\",\"x\":" + std::to_string(k & 1) + ",\"y\":" + std::to_string(k >> 1));
  return settle_and_play(p.plugin);
}
// MO-9 at count 4 for a lin key: the blend of its four corner values at MO-4's bilinear weights.
double mo9(const double v[4], double x, double y) {
  const double w[4] = {(1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y};
  return w[0] * v[0] + w[1] * v[1] + w[2] * v[2] + w[3] * v[3];
}
// Sets up the test corners under Blend, no glide, and moves the pad to (x, y); returns the engine's osc1.detune.
double blend_at(Loaded& p, const preview_bridge_ext* bridge, long long& seq, int preset, double x, double y) {
  morph_reset(p.plugin, bridge, seq);
  fresh(p, preset);
  post_corners(p.plugin, bridge, seq, test_corners());
  post(p.plugin, bridge, seq, "\"type\":\"morph\",\"settings\":{\"morph.glide\":0,\"morph.law\":0,\"morph.on\":1}");
  post(p.plugin, bridge, seq, "\"type\":\"position\",\"x\":" + num17(x) + ",\"y\":" + num17(y));
  blocks_of(p.plugin, 1, 256);
  return bridge->engine_v(p.plugin, horde::find_param("osc1.detune")->id);
}
// The `morph` member of a snapshot, from its key on (it is the snapshot's last member).
std::string morph_of(const std::string& frame) {
  const std::size_t at = frame.find(",\"morph\":{");
  return at == std::string::npos ? "" : frame.substr(at);
}
// A number after `"key":` in `text` from `from` on, or NaN.
double number_after(const std::string& text, const std::string& key, std::size_t from = 0) {
  const std::size_t at = text.find("\"" + key + "\":", from);
  return at == std::string::npos ? NAN : std::strtod(text.c_str() + at + key.size() + 3, nullptr);
}
// Corner `k`'s object in a morph member's `corners`, or "".
std::string corner_in(const std::string& morph, int k) {
  std::size_t at = morph.find("\"corners\":[");
  if (at == std::string::npos) return "";
  at += 11;   // past `"corners":[`
  for (int i = 0; i <= k; ++i) {
    const std::size_t open = morph.find('{', at), close = morph.find('}', open);
    if (open == std::string::npos || close == std::string::npos) return "";
    if (i == k) return morph.substr(open, close - open + 1);
    at = close + 1;
  }
  return "";
}
// Saves the plug-in's state into `text`; loads `text` back and lets the main thread build what it loaded.
bool save_to(Loaded& p, std::string& text) {
  struct Buffer { std::string* text; } b{&text};
  text.clear();
  clap_ostream_t os{&b, [](const clap_ostream_t* s, const void* data, std::uint64_t size) -> std::int64_t {
    static_cast<Buffer*>(s->ctx)->text->append(static_cast<const char*>(data), size);
    return std::int64_t(size);
  }};
  return p.state->save(p.plugin, &os);
}
bool load_from(Loaded& p, const std::string& text) {
  struct Buffer { const std::string* text; std::size_t read = 0; } b{&text};
  clap_istream_t is{&b, [](const clap_istream_t* s, void* data, std::uint64_t size) -> std::int64_t {
    auto* buf = static_cast<Buffer*>(s->ctx);
    const std::size_t n = std::min<std::size_t>(size, buf->text->size() - buf->read);
    std::memcpy(data, buf->text->data() + buf->read, n);
    buf->read += n;
    return std::int64_t(n);
  }};
  if (!p.state->load(p.plugin, &is)) return false;
  blocks_of(p.plugin, 1, 64);
  if (callback_requested) { callback_requested = false; p.plugin->on_main_thread(p.plugin); }
  blocks_of(p.plugin, 2, 256);   // takes the loaded state's engine, and its resolver resolves
  return true;
}
// A four-corner patch through the state: set up at (0.25, 0.75) under Blend, saved, another preset played, loaded back.
// Returns the engine's osc1.detune after the load (the corners, the settings and the position all decide it); `before`
// is what it read before the save, `chunk` the saved text. With `pins` (B639) osc1.detune also holds a waypoint at the
// pad's centre and the law is TIN with Clamp off, and osc1.baseWave a region lock: version 4's state.
constexpr double kPin = 0.9375;   // dyadic, like the corners
double through_state(Loaded& p, const preview_bridge_ext* bridge, long long& seq, int preset, int other, double* before, std::string* chunk,
                     bool pins = false) {
  *before = blend_at(p, bridge, seq, preset, 0.25, 0.75);
  post(p.plugin, bridge, seq, "\"type\":\"modes\",\"modes\":{\"osc1.baseWave\":\"quantum\"},\"counts\":{\"out.level\":2}");
  if (pins) {
    post(p.plugin, bridge, seq, "\"type\":\"waypoints\",\"key\":\"osc1.detune\",\"points\":[{\"x\":0.5,\"y\":0.5,\"v\":" + num17(kPin) + "}]");
    post(p.plugin, bridge, seq, "\"type\":\"waypointLaw\",\"law\":\"tin\",\"clamp\":false");
    post(p.plugin, bridge, seq, "\"type\":\"lock\",\"key\":\"osc1.baseWave\",\"offsets\":[0.5,-0.25,0.125]");
  }
  blocks_of(p.plugin, 1, 256);
  if (pins) *before = bridge->engine_v(p.plugin, horde::find_param("osc1.detune")->id);
  if (!save_to(p, *chunk)) return NAN;
  morph_reset(p.plugin, bridge, seq);
  fresh(p, other);
  blocks_of(p.plugin, 1, 256);
  if (!load_from(p, *chunk)) return NAN;
  return bridge->engine_v(p.plugin, horde::find_param("osc1.detune")->id);
}

// The id of the host parameter named `name` (morph.x, morph.y), or CLAP_INVALID_ID.
clap_id host_id(const Loaded& p, const char* name) {
  clap_param_info_t info{};
  for (std::uint32_t i = 0; i < p.params->count(p.plugin); ++i)
    if (p.params->get_info(p.plugin, i, &info) && !std::strcmp(info.name, name)) return info.id;
  return CLAP_INVALID_ID;
}
// The test corners under Blend, no glide, the pad left at A, then morph.x and morph.y set as host parameter events, the
// way automation arrives (B639); returns the engine's osc1.detune after one block.
double host_xy(Loaded& p, const preview_bridge_ext* bridge, long long& seq, int preset, double x, double y) {
  morph_reset(p.plugin, bridge, seq);
  fresh(p, preset);
  post_corners(p.plugin, bridge, seq, test_corners());
  post(p.plugin, bridge, seq, "\"type\":\"morph\",\"settings\":{\"morph.glide\":0,\"morph.law\":0,\"morph.on\":1}");
  blocks_of(p.plugin, 1, 256);
  float l[256], r[256];
  Events e;
  e.param(host_id(p, "morph.x"), x);
  e.param(host_id(p, "morph.y"), y);
  process(p.plugin, e, l, r, 256);
  return bridge->engine_v(p.plugin, horde::find_param("osc1.detune")->id);
}
// The test corners under Blend at the pad's centre with the waypoint list `points` (JSON) on osc1.detune; its value.
double pinned(Loaded& p, const preview_bridge_ext* bridge, long long& seq, int preset, const std::string& points) {
  morph_reset(p.plugin, bridge, seq);
  fresh(p, preset);
  post_corners(p.plugin, bridge, seq, test_corners());
  post(p.plugin, bridge, seq, "\"type\":\"morph\",\"settings\":{\"morph.glide\":0,\"morph.law\":0,\"morph.on\":1}");
  post(p.plugin, bridge, seq, "\"type\":\"waypoints\",\"key\":\"osc1.detune\",\"points\":" + points);
  post(p.plugin, bridge, seq, "\"type\":\"position\",\"x\":0.5,\"y\":0.5");
  blocks_of(p.plugin, 1, 256);
  return bridge->engine_v(p.plugin, horde::find_param("osc1.detune")->id);
}

// ---- each overrun on its own line in the session log (B642) -----------------------------------------------------------
// With every sounding block counted as an overrun (the extension's overrun_at, 0), a chord held while the pad moves and
// then rests must leave one line per overrun the totals count, in time order, each with its block size, the morph's
// state (moving while the pad moves, still once it rests) and both unison counts as the engine holds them. The plant
// that counts overruns but never records them must read as missing lines. Each run gets its own HOME, so its log is
// the only one there.
struct OverrunLines { int lines = 0, summary = -1, moving = 0, still = 0; bool frames_ok = true, unison_ok = true, ordered = true; };
int count_of(const char* key, double v) {
  const horde::ParamInfo* row = horde::find_param(key);
  return row ? int(std::lround(row->lo + std::round(v * double(row->steps > 1 ? row->steps - 1 : 0)))) : -1;
}
OverrunLines overrun_lines(const char* clap, int preset) {
  OverrunLines out;
  const std::string before = std::getenv("HOME") ? std::getenv("HOME") : "";
  char home[] = "/tmp/engine-preview-overruns-XXXXXX";
  if (!mkdtemp(home)) return out;
  setenv("HOME", home, 1);
  mkdir((std::string(home) + "/Library").c_str(), 0755);
  Loaded q;
  const preview_bridge_ext* bridge = nullptr;
  int unison[2] = {-1, -1};
  if (q.open(clap) && (bridge = static_cast<const preview_bridge_ext*>(q.plugin->get_extension(q.plugin, kPreviewBridgeExt)))) {
    bridge->overrun_at(q.plugin, 0);
    long long seq = 0;
    morph_reset(q.plugin, bridge, seq);
    fresh(q, preset);
    post_corners(q.plugin, bridge, seq, test_corners());
    post(q.plugin, bridge, seq, "\"type\":\"morph\",\"settings\":{\"morph.glide\":0,\"morph.law\":0,\"morph.on\":1}");
    blocks_of(q.plugin, 4);   // silent, so skipped and never counted
    float l[512], r[512];
    for (int b = 0; b < 40; ++b) {
      if (b > 0 && b < 20) post(q.plugin, bridge, seq, "\"type\":\"position\",\"x\":" + num17(b / 20.0) + ",\"y\":0.5");
      Events e;
      if (b == 0) for (int k = 0; k < 4; ++k) e.note(true, 0, 9300 + k, kChord[k], kVelocity);
      process(q.plugin, e, l, r, 512);
      if (callback_requested) { callback_requested = false; q.plugin->on_main_thread(q.plugin); }
    }
    for (int k = 0; k < 2; ++k) {
      const char* key = k == 0 ? "osc1.unison" : "osc2.unison";
      unison[k] = count_of(key, bridge->engine_v(q.plugin, horde::find_param(key)->id));
    }
    q.plugin->stop_processing(q.plugin);
    q.plugin->deactivate(q.plugin);   // writes the session log
    q.plugin->destroy(q.plugin);
    const std::string list = "ls " + std::string(home) + "/Library/Logs/horde-engine-preview/session-*.tsv 2>/dev/null";
    std::string path;
    if (FILE* ls = popen(list.c_str(), "r")) {
      char line[1024];
      if (std::fgets(line, sizeof line, ls)) path = line;
      pclose(ls);
    }
    while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) path.pop_back();
    std::ifstream f(path);
    std::string line;
    bool in_lines = false;
    double last = -1;
    while (std::getline(f, line)) {
      std::vector<std::string> col;
      std::stringstream cells(line);
      for (std::string c; std::getline(cells, c, '\t');) col.push_back(c);
      if (!in_lines && col.size() == 5 && col[0] == kBank[preset].name) out.summary = std::atoi(col[4].c_str());
      if (line.rfind("at_s\t", 0) == 0) { in_lines = true; continue; }
      if (!in_lines || col.size() != 7) continue;
      ++out.lines;
      const double at = std::atof(col[0].c_str());
      out.ordered = out.ordered && at > last;
      last = at;
      out.frames_ok = out.frames_ok && col[1] == kBank[preset].name && col[2] == "512";
      out.moving += col[4] == "moving";
      out.still += col[4] == "still";
      out.unison_ok = out.unison_ok && std::atoi(col[5].c_str()) == unison[0] && std::atoi(col[6].c_str()) == unison[1];
    }
  }
  setenv("HOME", before.c_str(), 1);
  return out;
}

// ---- MO-26's fade, through the plug-in and, independently, offline (B639) -------------------------------------------
// Osc 1 on in corner A only, under Blend, the pad walking from A to B over kFadeBlocks blocks while one note sounds. The
// offline side is a fresh engine and resolver of the same build, driven as the clean room's own shell drives them (item
// 35's shell_of): the preset, its polyphony capped as the plug-in caps it, the same corners, settings and positions at
// the same samples, one control interval at a time, the resolver's events after the note at a shared sample.
constexpr int kFadeBlocks = 40;
constexpr std::uint32_t kFadeBlock = 512;
double fade_x(int b) { return double(b) / kFadeBlocks; }
struct Fade { std::vector<float> out; std::vector<double> levels; };
double first_level(const std::string& morph) {
  const std::size_t at = morph.find("\"levels\":[");
  return at == std::string::npos ? NAN : std::strtod(morph.c_str() + at + 10, nullptr);
}
Fade fade_plugin(Loaded& p, const preview_bridge_ext* bridge, long long& seq, int preset) {
  morph_reset(p.plugin, bridge, seq);
  fresh(p, preset);
  for (int k = 1; k < 4; ++k) post(p.plugin, bridge, seq, "\"type\":\"corner\",\"index\":" + std::to_string(k) + ",\"values\":{\"osc1.on\":0}");
  post(p.plugin, bridge, seq, "\"type\":\"morph\",\"settings\":{\"morph.law\":0,\"morph.on\":1}");
  post(p.plugin, bridge, seq, "\"type\":\"want\",\"osc\":[],\"members\":false,\"morph\":true");
  Fade f;
  f.out.resize(2 * kFadeBlocks * kFadeBlock);
  std::vector<float> l(kFadeBlock), r(kFadeBlock);
  for (int b = 0; b < kFadeBlocks; ++b) {
    if (b > 0) post(p.plugin, bridge, seq, "\"type\":\"position\",\"x\":" + num17(fade_x(b)) + ",\"y\":0");
    Events e;
    if (b == 0) e.note(true, 0, 9200, 60, 0.8);
    process(p.plugin, e, l.data(), r.data(), kFadeBlock);
    for (std::uint32_t i = 0; i < kFadeBlock; ++i) f.out[2 * (b * kFadeBlock + i)] = l[i], f.out[2 * (b * kFadeBlock + i) + 1] = r[i];
    f.levels.push_back(first_level(morph_of(bridge->frame(p.plugin))));
  }
  Events off;
  off.note(false, 0, 9200, 60, 0);
  process(p.plugin, off, l.data(), r.data(), kFadeBlock);
  blocks_of(p.plugin, 200);
  post(p.plugin, bridge, seq, "\"type\":\"want\",\"osc\":[1,2],\"members\":false");
  morph_reset(p.plugin, bridge, seq);
  blocks_of(p.plugin, 1, 256);
  return f;
}
std::vector<float> fade_offline(int preset) {
  std::vector<float> out(2 * kFadeBlocks * kFadeBlock);
  auto e = horde::make_engine();
  auto res = horde::make_morph_resolver();
  if (!e || !res || !e->prepare(44100, int(kFadeBlock)) || !e->load_state(kBank[preset].state).ok || !res->prepare(44100)) return {};
  const horde::ParamInfo* poly = horde::find_param("voice.polyphony");
  if (poly->lo + std::round(e->param_v(poly->id) * double(poly->steps - 1)) > 8) {   // the plug-in's own cap (preview_plugin.cpp)
    horde::Event cap;
    cap.type = horde::EventType::Param;
    cap.param_id = poly->id;
    cap.value = v_of_count(*poly, 8);
    e->flush(&cap, 1);
  }
  std::vector<double> by_id(horde::manifest_id_span(), 0.0);
  for (std::size_t i = 0; i < horde::manifest_size(); ++i) by_id[horde::manifest_param(i).id] = e->param_v(horde::manifest_param(i).id);
  const std::string t0 = e->ratios(0), t1 = e->ratios(1);
  res->set_corner(0, by_id.data(), t0.c_str(), t1.c_str(), 0);
  by_id[horde::find_param("osc1.on")->id] = 0;
  for (int k = 1; k < 4; ++k) res->set_corner(k, by_id.data(), t0.c_str(), t1.c_str(), 0);
  double v[preview::morph::kSettings];
  for (int i = 0; i < preview::morph::kSettings; ++i) v[i] = preview::morph::default_v(i);
  v[preview::morph::kLaw] = 0;
  v[preview::morph::kOn] = 1;
  res->set_settings(preview::morph::decode(v), 0);
  res->set_position(0, 0, 0);
  const std::uint64_t ci = 29;   // PA-3 at 44.1 kHz: round(44100 / 1500)
  std::vector<horde::MorphEvent> got(4096);
  std::vector<horde::Event> events;
  std::uint64_t clock = 0;
  horde2::underFlushToZero([&] {
    for (int b = 0; b < kFadeBlocks; ++b) {
      if (b > 0) res->set_position(fade_x(b), 0, clock);
      for (std::uint32_t pos = 0; pos < kFadeBlock;) {
        const std::uint32_t span = std::uint32_t(std::min<std::uint64_t>(kFadeBlock - pos, ci - clock % ci));
        events.clear();
        if (b == 0 && pos == 0) {
          horde::Event on;
          on.type = horde::EventType::NoteOn;
          on.note_id = 9200, on.channel = 0, on.key = 60, on.value = 0.8;
          events.push_back(on);
        }
        const std::size_t n = res->process(span, got.data(), got.size());
        for (std::size_t i = 0; i < n; ++i) {
          const horde::MorphEvent& m = got[i];
          horde::Event ev;
          ev.sample = std::uint32_t(m.sample - clock);
          if (m.kind == horde::MorphEvent::Kind::Param) ev.type = horde::EventType::Param, ev.param_id = m.param_id, ev.value = m.value, ev.landing = m.landing;
          else if (m.kind == horde::MorphEvent::Kind::Level) ev.type = horde::EventType::MorphLevel, ev.integer = m.source, ev.value = m.value;
          else if (m.kind == horde::MorphEvent::Kind::Ratios) { e->set_ratios(m.osc, m.text); continue; }   // at the span's start: its boundary
          else continue;
          events.push_back(ev);
        }
        float l[kFadeBlock], r[kFadeBlock];
        e->process(events.data(), events.size(), nullptr, 0, l, r, int(span));
        for (std::uint32_t i = 0; i < span; ++i) {
          const std::size_t at = std::size_t(b) * kFadeBlock + pos + i;
          out[2 * at] = std::isfinite(l[i]) ? l[i] : 0.0f;
          out[2 * at + 1] = std::isfinite(r[i]) ? r[i] : 0.0f;
        }
        pos += span;
        clock += span;
      }
    }
  });
  return out;
}

int find_preset(const char* name) {
  for (int i = 0; i < kBankSize; ++i)
    if (!std::strcmp(kBank[i].name, name)) return i;
  return -1;
}

// ---- B645's demo bank ------------------------------------------------------------------------------------------------
// One corner object of a morph member ("{\"key\":v,...}") as key and v pairs: keys are quoted and hold no comma.
std::vector<std::pair<std::string, double>> pairs_of(const std::string& object) {
  std::vector<std::pair<std::string, double>> out;
  for (std::size_t at = object.find('"'); at != std::string::npos;) {
    const std::size_t close = object.find('"', at + 1);
    if (close == std::string::npos) break;
    out.emplace_back(object.substr(at + 1, close - at - 1), std::strtod(object.c_str() + close + 2, nullptr));
    const std::size_t comma = object.find(',', close);
    at = comma == std::string::npos ? comma : object.find('"', comma);
  }
  return out;
}
// A demo loaded through the page's preset message, read in the frame that completes the load, where the page adopts
// every corner (device.js, adopt): corner A's row count, how many rows of each other corner differ from A's, whether
// the morph is on, and the four corner objects as sent.
struct DemoLoad { int rows_a = 0, apart[4] = {}; bool on = false; std::string corner[4]; };
DemoLoad demo_load(Loaded& p, const preview_bridge_ext* bridge, long long& seq, int demo) {
  post(p.plugin, bridge, seq, "\"type\":\"want\",\"osc\":[],\"members\":false,\"morph\":true");
  const std::string m = morph_of(send_until_acked(p.plugin, bridge, seq, "\"type\":\"preset\",\"index\":" + std::to_string(demo)));
  DemoLoad out;
  for (int k = 0; k < 4; ++k) out.corner[k] = corner_in(m, k);
  const auto a = pairs_of(out.corner[0]);
  out.rows_a = int(a.size());
  for (int k = 1; k < 4; ++k)
    for (const auto& [key, v] : pairs_of(out.corner[k]))
      for (const auto& [ka, va] : a)
        if (ka == key && va != v) ++out.apart[k];
  out.on = number_after(m, "morph.on") == 1;
  return out;
}
// The pad to (x, y) as host automation moves it (morph.x and morph.y, B639).
void pad_to(Loaded& p, double x, double y) {
  float l[256], r[256];
  Events e;
  e.param(host_id(p, "morph.x"), x);
  e.param(host_id(p, "morph.y"), y);
  process(p.plugin, e, l, r, 256);
}
// A factory preset after a demo, the pad at the centre with the demo's morph still on (a load keeps the settings): the
// chord from the fresh engine's first sample, through the host's Preset as play() loads it.
std::vector<float> factory_after_demo(Loaded& p, const preview_bridge_ext* bridge, long long& seq, int demo, int factory) {
  (void)demo_load(p, bridge, seq, demo);
  pad_to(p, 0.5, 0.5);
  return play(p, factory, {128});
}

// The demo bank's checks (B645), each instance its own. A demo loads its four corners (the control: a build that never
// loads corners B to D reads every corner as A), its morph comes back through a saved chunk, and a factory preset loaded
// after it still plays bit for bit as the engine's offline render (the control: a load that fills corner A only keeps
// the demo's other corners, and the pad between them is heard).
void demo_checks(const char* clap, const char* plant, const char* plant_map, int factory, int engine_rows) {
  int first = -1, demos = 0;
  bool placed = true;
  for (int i = 0; i < kBankSize; ++i) {
    const bool demo = kBank[i].morph != nullptr;
    if (demo && first < 0) first = i;
    demos += demo;
    placed = placed && demo == (i >= kFactorySize) && (!demo || !std::strncmp(kBank[i].name, "Demo (morph): ", 14));
  }
  check(first == kFactorySize && demos > 0 && placed, "the demo bank follows the factory bank: " + std::to_string(kFactorySize) + " factory presets, then " +
                                                          std::to_string(demos) + " named \"Demo (morph): ...\"");
  if (first < 0) return;
  Loaded p;
  check(p.open(clap), "a fresh instance for the demo bank loads");
  const auto* bridge = static_cast<const preview_bridge_ext*>(p.plugin->get_extension(p.plugin, kPreviewBridgeExt));
  if (!bridge) return;
  long long seq = 0;
  (void)send_until_acked(p.plugin, bridge, seq, "\"type\":\"hello\"");
  bool all = true;
  std::string detail;
  DemoLoad first_load;
  for (int d = first; d < kBankSize; ++d) {
    const DemoLoad l = demo_load(p, bridge, seq, d);
    if (d == first) first_load = l;
    const bool ok = l.rows_a == engine_rows && l.apart[1] > 0 && l.apart[2] > 0 && l.apart[3] > 0 && l.on;
    all = all && ok;
    detail += std::string(d == first ? "" : ", ") + std::to_string(l.apart[1]) + "/" + std::to_string(l.apart[2]) + "/" + std::to_string(l.apart[3]) + (ok ? "" : " (!)");
  }
  check(all, "every demo loads with the morph on and corners B, C and D apart from A, in the frame that completes the load (rows apart: " + detail + ")");

  // Through a saved chunk: the demo, another preset, the chunk back; a hello then resends every corner.
  (void)demo_load(p, bridge, seq, first);
  std::string chunk;
  check(save_to(p, chunk), "a demo saves");
  std::printf("     the first demo's chunk: %zu bytes\n", chunk.size());
  fresh(p, factory);
  blocks_of(p.plugin, 1, 256);
  check(load_from(p, chunk), "and loads");
  const std::string back = morph_of(send_until_acked(p.plugin, bridge, seq, "\"type\":\"hello\""));
  bool same = true;
  for (int k = 0; k < 4; ++k) same = same && !first_load.corner[k].empty() && corner_in(back, k) == first_load.corner[k];
  check(same && number_after(back, "morph.on") == 1, "its four corners and the morph come back from the chunk exactly, through another preset");

  const std::vector<float> reference = offline(factory);
  const std::vector<float> heard = factory_after_demo(p, bridge, seq, first, factory);
  const std::size_t at = first_difference(heard, reference);
  check(at == heard.size() && peak(heard) > 0, std::string(kBank[factory].name) + " after a demo, the pad at the centre: bit-identical to the engine's offline render" +
                                                   (at == heard.size() ? "" : " (first difference at value " + std::to_string(at) + ")"));
  p.plugin->stop_processing(p.plugin);
  p.plugin->deactivate(p.plugin);
  p.plugin->destroy(p.plugin);

  if (plant) {
    Loaded q;
    check(q.open(plant), "the plant build loads for the demo bank");
    const auto* qb = static_cast<const preview_bridge_ext*>(q.plugin->get_extension(q.plugin, kPreviewBridgeExt));
    if (qb) {
      long long qs = 0;
      (void)send_until_acked(q.plugin, qb, qs, "\"type\":\"hello\"");
      const DemoLoad l = demo_load(q, qb, qs, first);
      check(l.rows_a == engine_rows && l.apart[1] == 0 && l.apart[2] == 0 && l.apart[3] == 0,
            "control: a loader that fills all four corners with A reads every corner as A");
    }
    q.plugin->stop_processing(q.plugin);
    q.plugin->deactivate(q.plugin);
    q.plugin->destroy(q.plugin);
  }
  if (plant_map) {
    Loaded q;
    check(q.open(plant_map), "the shifted-key-map plant loads for the demo bank");
    const auto* qb = static_cast<const preview_bridge_ext*>(q.plugin->get_extension(q.plugin, kPreviewBridgeExt));
    if (qb) {
      long long qs = 0;
      (void)send_until_acked(q.plugin, qb, qs, "\"type\":\"hello\"");
      const std::vector<float> kept = factory_after_demo(q, qb, qs, first, factory);
      check(first_difference(kept, reference) != kept.size(), "control: a load that fills corner A only keeps the demo's other corners, and misses the offline render");
    }
    q.plugin->stop_processing(q.plugin);
    q.plugin->deactivate(q.plugin);
    q.plugin->destroy(q.plugin);
  }
}

}  // namespace

// ---- B660's test-only filter ----------------------------------------------------------------------------------------
// Weir's SVF 12 after the engine, six host parameters, off by default. `ref` is the plant built without any of it (the
// build as it was before the filter), so "off is bit-identical" is held to the real thing, not to the filter's own word,
// and the same plant is the no-op filter that the filter-on checks must tell from the real one.
namespace flt = preview::filter;

void set_filter(Loaded& p, double on, double mode, double cutoff, double res, double ring, double mix) {
  float l[64], r[64];
  Events e;
  const double v[flt::kParams] = {on, mode, cutoff, res, ring, mix};
  for (int f = 0; f < flt::kParams; ++f) e.param(flt::id_of(f), v[f]);
  process(p.plugin, e, l, r, 64);
}
// The high band: the second difference's energy, a crude high-pass; the left channel of `play`'s interleaved output.
double high_band(const std::vector<float>& x) {
  double e = 0;
  for (std::size_t i = 2; i < x.size() / 2; ++i) {
    const double d = double(x[2 * i]) - 2.0 * double(x[2 * i - 2]) + double(x[2 * i - 4]);
    e += d * d;
  }
  return e;
}
double rms_of(const std::vector<float>& x) {
  double e = 0;
  for (float v : x) e += double(v) * double(v);
  return std::sqrt(e / double(x.size()));
}
void filter_checks(const char* clap, const char* nofilter) {
  Loaded p, ref;
  check(p.open(clap), "filter: the plug-in loads");
  if (!nofilter) { check(false, "the no-filter plant was not given, so the filter's bit-identity control did not run"); return; }
  check(ref.open(nofilter), "filter: the build without the filter loads");
  if (!p.plugin || !ref.plugin) return;

  // The parameters: six, after the morph's pair, at 0xA00010 on, off by default; the plant has none.
  const std::uint32_t n = p.params->count(p.plugin);
  bool listed = n > std::uint32_t(flt::kParams) && ref.params->count(ref.plugin) == n - flt::kParams;
  for (int f = 0; f < flt::kParams && listed; ++f) {
    clap_param_info_t info{};
    listed = p.params->get_info(p.plugin, n - flt::kParams + std::uint32_t(f), &info) && info.id == 0xA00010 + std::uint32_t(f) &&
             !std::strcmp(info.name, flt::param_name(f)) && info.default_value == flt::default_value(f) && info.min_value == 0 &&
             info.max_value == flt::max_value(f) && (info.flags & CLAP_PARAM_IS_STEPPED) == (flt::stepped(f) ? CLAP_PARAM_IS_STEPPED : 0u);
  }
  check(listed, "the filter's six parameters follow the morph's, at 0xA00010 to 0xA00015, Filter on defaulting to off; the build without has none");
  double on_now = -1;
  check(p.params->get_value(p.plugin, 0xA00010, &on_now) && on_now == 0, "control: Filter on reads 0 (off) on a fresh instance");
  char text[64];
  check(p.params->value_to_text(p.plugin, 0xA00012, 1.0 / 3.0, text, sizeof text) && !std::strcmp(text, "200 Hz") &&
            p.params->value_to_text(p.plugin, 0xA00011, 2, text, sizeof text) && !std::strcmp(text, "HP"),
        "the cutoff reads in Hz on its log law (one third is 200 Hz) and the mode by name");

  // Off is the build as it was, bit for bit, on every preset the plug-in offers: the factory bank and the demo bank.
  const auto t0 = std::chrono::steady_clock::now();
  int identical = 0, loud = 0, first_bad = -1;
  for (int i = 0; i < kBankSize; ++i) {
    const std::vector<float> a = play(p, i, {128}), b = play(ref, i, {128});
    const bool same = a == b;
    identical += same;
    loud += peak(a) > 0;
    if (!same && first_bad < 0) first_bad = i;
  }
  const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  std::printf("     filter off, %d presets played twice in %.0f s\n", kBankSize, took);
  check(identical == kBankSize, "filter off: every preset (" + std::to_string(kBankSize) + ", factory and demo) is bit-identical to the build without the filter" +
                                    (first_bad >= 0 ? " (first difference: " + std::string(kBank[first_bad].name) + ")" : ""));
  check(loud >= kBankSize / 2, "control: most of them make sound, so identical is not the silence of two dead builds (" + std::to_string(loud) + ")");

  // The brightest of the smoke presets (as the plant renders it), through LP at 200 Hz.
  int bright = -1;
  double best = 0;
  for (const char* name : {"Trance jitter", "Glass horde pad", "Crunch horde", "Wobble jaw"}) {
    const int i = find_preset(name);
    if (i < 0) continue;
    const std::vector<float> a = play(ref, i, {128});
    const double ratio = high_band(a) / (rms_of(a) * rms_of(a) + 1e-30);
    if (ratio > best) { best = ratio; bright = i; }
  }
  check(bright >= 0, "a bright preset to filter");
  if (bright < 0) return;
  const std::vector<float> dry = play(ref, bright, {128});
  set_filter(p, 1, 0, 1.0 / 3.0, 0, 0, 1);
  set_filter(ref, 1, 0, 1.0 / 3.0, 0, 0, 1);   // the plant has no such parameter: what a no-op filter does with it
  const std::vector<float> wet = play(p, bright, {128});
  const std::vector<float> noop = play(ref, bright, {128});
  const double drop = high_band(wet) / high_band(dry), noop_drop = high_band(noop) / high_band(dry);
  std::printf("     %s through LP at 200 Hz: high band x%.5f, level x%.3f; the no-op filter: high band x%.3f\n", kBank[bright].name, drop,
              rms_of(wet) / rms_of(dry), noop_drop);
  check(drop < 0.01 && rms_of(wet) > 1e-3 * rms_of(dry), std::string("filter on: LP at 200 Hz takes ") + kBank[bright].name + "'s high band down by more than 20 dB and is not a mute");
  check(noop_drop > 0.9 && !(noop_drop < 0.01), "control: a no-op filter (the build without) reads the same drop as 1, so the check above fails it");

  // Mix 0 is the dry signal, bit for bit, with the filter on: the other way to read the same control.
  set_filter(p, 1, 0, 1.0 / 3.0, 0, 0, 0);
  check(play(p, bright, {128}) == dry, "control: filter on with Mix 0 is bit-identical to the dry render");

  // No NaN, no subnormal out of the real plug-in, every mode at full resonance and Ring (the pure-DSP corners are in bridge_test).
  bool clean = true;
  for (int mode = 0; mode < flt::kModeCount; ++mode) {
    set_filter(p, 1, mode, 0.4, 1, 1, 1);
    const std::vector<float> a = play(p, bright, {512});
    for (float v : a) clean = clean && std::isfinite(v) && std::fpclassify(v) != FP_SUBNORMAL;
    clean = clean && peak(a) > 0;
  }
  check(clean, "filter on: every mode at full Resonance and Ring is finite, never subnormal, and sounds");

  // The state: what was set saves, loads into a new instance and plays the same; a default filter saves no line.
  set_filter(p, 1, 3, 0.45, 0.6, 0.25, 0.75);
  std::string chunk;
  check(save_to(p, chunk) && chunk.find("\nfilter 1 3 ") != std::string::npos && chunk.rfind("horde-engine-preview 5", 0) == 0, "state: the filter's values are in the saved chunk");
  Loaded q;
  check(q.open(clap), "filter: a second instance loads");
  std::string plain;
  check(save_to(q, plain) && plain.find("filter") == std::string::npos && plain.rfind("horde-engine-preview 2", 0) == 0,
        "control: an untouched instance saves no filter line and the chunk it always did");
  if (q.plugin && load_from(q, chunk)) {
    const double want[flt::kParams] = {1, 3, 0.45, 0.6, 0.25, 0.75};
    bool same = true;
    for (int f = 0; f < flt::kParams; ++f) {
      double got = -1;
      same = same && q.params->get_value(q.plugin, flt::id_of(f), &got) && got == want[f];
    }
    check(same, "state: a new instance loads the six values back exactly");
    // Off, a new engine (so nothing of the last note is still sounding), then on: the cleared filter the new instance starts with.
    set_filter(p, 0, 3, 0.45, 0.6, 0.25, 0.75);
    fresh(p, bright);
    set_filter(p, 1, 3, 0.45, 0.6, 0.25, 0.75);
    check(play(p, bright, {128}) == play(q, bright, {128}), "state: and it plays what the saved instance plays, bit for bit");
    check(load_from(q, plain) && q.params->get_value(q.plugin, 0xA00010, &on_now) && on_now == 0, "state: an older chunk, with no filter line, loads with the filter off");
  } else {
    check(false, "the saved chunk did not load");
  }

  // Off again after on: nothing stale reaches the output.
  set_filter(p, 0, 3, 0.45, 0.6, 0.25, 0.75);
  check(play(p, bright, {128}) == dry, "filter turned off again: bit-identical to the build without the filter once more");

  for (Loaded* l : {&p, &ref, &q}) {
    if (!l->plugin) continue;
    l->plugin->stop_processing(l->plugin);
    l->plugin->deactivate(l->plugin);
    l->plugin->destroy(l->plugin);
  }
}

int main(int argc, char** argv) {
  if (argc < 2) { std::printf("usage: smoke <clap> [<plant> <plant-map> <plant-morph> <plant-nofilter>]\n"); return 2; }
  // The session log goes under $HOME, so point it at a scratch folder: a test mustn't write to the human's Logs.
  char home[] = "/tmp/engine-preview-smoke-XXXXXX";
  if (!mkdtemp(home)) return 2;
  setenv("HOME", home, 1);
  std::string logs = std::string(home) + "/Library";
  mkdir(logs.c_str(), 0755);

  Loaded p;
  check(p.open(argv[1]), "the plug-in loads and activates through its CLAP interface");
  if (failures) return 1;

  const std::uint32_t n_params = p.params->count(p.plugin);
  std::printf("     %u host parameters\n", n_params);
  int engine_rows = 0;
  for (std::size_t i = 0; i < horde::manifest_size(); ++i) engine_rows += horde::manifest_param(i).reserved ? 0 : 1;
  // B632 (ADR-370): every engine row is a host parameter, past Live's 64, whose panel then fills from the window.
  // B639 (IF-11): and the morph's position, morph.x and morph.y, last.
  // B660: and the test-only filter's six after them.
  check(int(n_params) == 1 + engine_rows + 2 + preview::filter::kParams, "every non-reserved engine row is a host parameter, plus Preset, morph.x and morph.y, and the test-only filter's six (" + std::to_string(1 + engine_rows + 2 + preview::filter::kParams) + ")");
  {
    clap_param_info_t xi{}, yi{};
    check(p.params->get_info(p.plugin, std::uint32_t(1 + engine_rows), &xi) && p.params->get_info(p.plugin, std::uint32_t(2 + engine_rows), &yi) &&
              !std::strcmp(xi.name, "morph.x") && !std::strcmp(yi.name, "morph.y") && xi.min_value == 0 && xi.max_value == 1 && xi.default_value == 0 &&
              yi.default_value == 0 && !(xi.flags & CLAP_PARAM_IS_STEPPED),
          "morph.x and morph.y follow the engine rows, 0 to 1, default 0, continuous (D.2)");
  }
  {
    bool same = true;
    clap_param_info_t row_info{};
    std::uint32_t k = 1;
    for (std::size_t i = 0; i < horde::manifest_size(); ++i) {
      const horde::ParamInfo& row = horde::manifest_param(i);
      if (row.reserved) continue;
      same = same && p.params->get_info(p.plugin, k++, &row_info) && row_info.id == row.id && !std::strcmp(row_info.name, row.key);
    }
    check(same, "each host parameter carries its engine row's own id and key, so automation survives the manifest growing");
  }
  clap_param_info_t info;
  check(p.params->get_info(p.plugin, 0, &info) && !std::strcmp(info.name, "Preset"), "the first parameter is Preset");

  // ---- GUI 3's bridge (B628), driven through the plug-in's own extension, the path the window takes ----
  const auto* bridge = static_cast<const preview_bridge_ext*>(p.plugin->get_extension(p.plugin, kPreviewBridgeExt));
  check(bridge != nullptr, "the plug-in offers the bridge extension");
  if (!bridge) return 1;
  // A row the page sets: the first continuous one. (Since B632 every row is also a host parameter.)
  const horde::ParamInfo* page_row = nullptr;
  for (std::size_t i = 0; i < horde::manifest_size() && !page_row; ++i) {
    const horde::ParamInfo& row = horde::manifest_param(i);
    if (!row.reserved && row.law == horde::Law::Lin) page_row = &row;
  }
  check(page_row != nullptr, "the engine has a continuous row to set from the page");
  {
    // GUI 3's page adopts the frame that acknowledges its hello and carries every engine row: a LOAD. Until the audio
    // path has applied the hello, no frame may claim it, or carry the full set the page would take for the answer.
    bridge->receive(p.plugin, "{\"type\":\"hello\",\"seq\":1}");
    const std::string early = bridge->frame(p.plugin);
    check(ack_of(early) < 1 && params_rows(early) == 0 && !has(early, "\"hello\""), "control: before the hello is applied, no frame answers it");
    one_block(p.plugin);
    std::string frame = bridge->frame(p.plugin);
    check(ack_of(frame) == 1 && params_rows(frame) == engine_rows, "the frame that acknowledges hello carries every engine row (" + std::to_string(engine_rows) + ")");
    check(has(frame, "\"hello\":{\"build\":\"engine ") && has(frame, "\"presets\":{\"names\":[") && has(frame, "\"params\":{") &&
              has(frame, "\"plugin\":{\"build\":\"engine ") && !has(frame, "\"voices\":4"),
          "after hello the snapshot carries the build label, the presets and every engine row, and no Voices");
    const std::size_t at = frame.find("\"manifest_hash\":\"");
    const std::string engine_hash = at == std::string::npos ? "" : frame.substr(at + 17, 64);
    std::printf("     engine manifest hash %s\n", engine_hash.c_str());
    bridge->receive(p.plugin, ("{\"type\":\"hello\",\"seq\":2,\"manifest_hash\":\"" + engine_hash + "\"}").c_str());
    one_block(p.plugin);
    check(has(bridge->frame(p.plugin), "\"mismatch\":null"), "a page with the engine's manifest hash gets no banner");
    bridge->receive(p.plugin, "{\"type\":\"hello\",\"seq\":3,\"manifest_hash\":\"0000\"}");
    one_block(p.plugin);
    check(has(bridge->frame(p.plugin), "\"mismatch\":{\"expected\":\"" + engine_hash + "\",\"got\":\"0000\""),
          "control: a page with another hash gets the mismatch, and the plug-in keeps playing");
    frame = bridge->frame(p.plugin);
    check(!has(frame, "\"params\"") && !has(frame, "\"presets\""), "params and presets ride only when something changed");
  }
  if (page_row) {
    const double before = bridge->engine_v(p.plugin, page_row->id);
    const double wanted = before == 0.25 ? 0.75 : 0.25;
    char message[160];
    std::snprintf(message, sizeof message, "{\"type\":\"set\",\"seq\":501,\"values\":{\"%s\":%.17g}}", page_row->key, wanted);
    bridge->receive(p.plugin, message);
    // Must-fail by construction: until a block runs, the audio path hasn't applied it, and both checks read otherwise.
    check(bridge->engine_v(p.plugin, page_row->id) == before && ack_of(bridge->frame(p.plugin)) < 501,
          "control: before a block runs, the set has neither reached the engine nor been acknowledged");
    one_block(p.plugin);
    check(bridge->engine_v(p.plugin, page_row->id) == wanted, std::string("a set through the bridge reaches the engine: ") + page_row->key + " reads back its v");
    const std::string frame = bridge->frame(p.plugin);
    check(ack_of(frame) == 501, "and its seq comes back as ack");
    std::snprintf(message, sizeof message, "\"%s\":%.17g", page_row->key, wanted);
    check(has(frame, message), "and the snapshot's params carry the row the page set");
  }
  {
    // Every type the seam names is acknowledged, each in its turn: held ones included, never an error.
    const char* const kTypes[] = {"set", "gesture", "morph", "position", "modes", "rack", "midi", "clear", "want", "preset", "plugin", "hello"};
    long long seq = 600;
    bool all = true;
    std::string missed;
    for (const char* type : kTypes) {
      std::string message = "{\"type\":\"" + std::string(type) + "\",\"seq\":" + std::to_string(++seq);
      if (!std::strcmp(type, "preset")) message += ",\"index\":" + std::to_string(find_preset("Trance jitter"));
      if (!std::strcmp(type, "plugin")) message += ",\"voices\":4";
      if (!std::strcmp(type, "want")) message += ",\"osc\":[1,2],\"members\":false";
      bridge->receive(p.plugin, (message + "}").c_str());
      // A preset is a load: its ack waits for the new engine, which a frame builds and the next block takes.
      bool acked = false;
      for (int round = 0; round < 4 && !acked; ++round) {
        one_block(p.plugin);
        acked = ack_of(bridge->frame(p.plugin)) == seq;
      }
      if (!acked) { all = false; missed += std::string(" ") + type; }
    }
    check(all, "every message type is acknowledged with its own seq" + (missed.empty() ? "" : " (missed:" + missed + ")"));
    bridge->receive(p.plugin, "{\"type\":\"set\",\"values\":{}}");
    one_block(p.plugin);
    check(ack_of(bridge->frame(p.plugin)) == seq, "control: a message with no seq moves no ack");
    bridge->receive(p.plugin, "{\"type\":\"set\",\"seq\":700,\"values\":{\"no.such.key\":0.5}}");
    one_block(p.plugin);
    const std::string frame = bridge->frame(p.plugin);
    check(ack_of(frame) == 700 && has(frame, "\"refused\":{\"unknown\":[\"no.such.key\"]"), "a key the manifest lacks is acknowledged and reported back");
  }
  if (page_row) {
    // A preset LOAD: acknowledged only once its engine plays, in a frame carrying every row of that engine.
    double was = -1;
    p.params->get_value(p.plugin, 0xB00001, &was);
    const int other = (int(was) + 7) % kFactorySize;
    bridge->receive(p.plugin, ("{\"type\":\"preset\",\"seq\":1000,\"index\":" + std::to_string(other) + "}").c_str());
    one_block(p.plugin);   // the audio thread applies the message: Preset moves, and the old path would ack here
    double now = -1;
    p.params->get_value(p.plugin, 0xB00001, &now);
    const std::string applied = bridge->frame(p.plugin);   // builds the new engine; the audio thread hasn't taken it yet
    check(int(now) == other && ack_of(applied) < 1000 && params_rows(applied) == 0,
          "control: the message applied but its engine not yet live, no frame acknowledges the load or carries rows");
    one_block(p.plugin);   // the swap
    const std::string loaded = bridge->frame(p.plugin);
    check(ack_of(loaded) == 1000 && params_rows(loaded) == engine_rows && has(loaded, "\"index\":" + std::to_string(other) + "}"),
          "once the new engine plays, one frame acknowledges the preset with every row and its index");
    check(param_in(loaded, page_row->key) == bridge->engine_v(p.plugin, page_row->id), "and its rows are the new engine's own values");
    check(params_rows(bridge->frame(p.plugin)) == 0, "the next frame carries no rows: nothing changed");

    // Loading the preset that is already playing still loads it: an edit is replaced by the preset's own value.
    const double preset_v = bridge->engine_v(p.plugin, page_row->id);
    const double edited = preset_v == 0.125 ? 0.625 : 0.125;
    char message[160];
    std::snprintf(message, sizeof message, "{\"type\":\"set\",\"seq\":1001,\"values\":{\"%s\":%.17g}}", page_row->key, edited);
    bridge->receive(p.plugin, message);
    one_block(p.plugin);
    check(bridge->engine_v(p.plugin, page_row->id) == edited, "control: the edit is in the engine before the reload");
    bridge->receive(p.plugin, ("{\"type\":\"preset\",\"seq\":1002,\"index\":" + std::to_string(other) + "}").c_str());
    std::string reload;
    for (int round = 0; round < 4 && ack_of(reload) != 1002; ++round) {
      one_block(p.plugin);
      reload = bridge->frame(p.plugin);
    }
    check(ack_of(reload) == 1002 && params_rows(reload) == engine_rows && param_in(reload, page_row->key) == preset_v &&
              bridge->engine_v(p.plugin, page_row->id) == preset_v,
          "reloading the playing preset brings back its own values, in the engine and in the frame");
  }

  {
    float l[128], r[128];
    Events none;
    process(p.plugin, none, l, r, 128);
    bool zero = true;
    for (int i = 0; i < 128; ++i) zero = zero && l[i] == 0 && r[i] == 0;
    check(zero, "no notes, exact silence (OU-4)");
  }

  int routed = -1;
  for (int i = 0; i < kBankSize && routed < 0; ++i) {
    for (int r = 0; r < kBank[i].n_routes; ++r) {
      if (kBank[i].routes[r].octaves != 0) { routed = i; break; }
    }
  }
  check(routed >= 0, "the bank has a preset with blade-envelope routes");
  std::printf("     routed preset: %s\n", routed >= 0 ? kBank[routed].name : "none");

  std::vector<int> presets = {find_preset("Trance jitter"), find_preset("Glass horde pad"), find_preset("Crunch horde"), find_preset("Wobble jaw"), routed};
  for (int preset : presets) {
    if (preset < 0) { check(false, "a named preset is missing from the bank"); continue; }
    const auto reference = offline(preset);
    const auto heard = play(p, preset, {128});
    const std::size_t at = first_difference(heard, reference);
    check(at == heard.size(), std::string(kBank[preset].name) + ": bit-identical to the engine's offline render, blocks of 128" + (at == heard.size() ? "" : " (first difference at value " + std::to_string(at) + ")"));
    check(peak(heard) > 0, std::string(kBank[preset].name) + ": it makes sound (silence is exact zero, OU-4)");
    const auto uneven = play(p, preset, {64, 97, 1, 256, 33});
    check(first_difference(uneven, reference) == uneven.size(), std::string(kBank[preset].name) + ": the same under uneven blocks (DT-2)");
  }

  // Must-fail: the comparison can see a wrong sound.
  {
    const int a = presets[0], b = presets[1];
    const auto heard = play(p, a, {128});
    const auto other = offline(b);
    check(first_difference(heard, other) != heard.size(), "control: a different preset's render reads as different");
  }

  // State: a saved chunk brings back the preset and a moved value, and stays small.
  {
    Events set;
    float l[64], r[64];
    (void)play(p, presets[1], {128});
    // A row only the page can set rides in the saved state too ("current settings only", every engine row).
    const double page_value = 0.375;
    if (page_row) {
      char message[160];
      std::snprintf(message, sizeof message, "{\"type\":\"set\",\"seq\":800,\"values\":{\"%s\":%.17g}}", page_row->key, page_value);
      bridge->receive(p.plugin, message);
      process(p.plugin, set, l, r, 64);
    }
    struct Buffer { std::string text; std::size_t read = 0; } buffer;
    clap_ostream_t os{&buffer, [](const clap_ostream_t* s, const void* data, std::uint64_t size) -> std::int64_t {
      static_cast<Buffer*>(s->ctx)->text.append(static_cast<const char*>(data), size);
      return std::int64_t(size);
    }};
    check(p.state->save(p.plugin, &os), "state saves");
    std::printf("     state chunk %zu bytes\n", buffer.text.size());
    check(buffer.text.size() < 8192, "the state chunk stays small");
    (void)play(p, presets[0], {128});
    clap_istream_t is{&buffer, [](const clap_istream_t* s, void* data, std::uint64_t size) -> std::int64_t {
      auto* b = static_cast<Buffer*>(s->ctx);
      const std::size_t n = std::min<std::size_t>(size, b->text.size() - b->read);
      std::memcpy(data, b->text.data() + b->read, n);
      b->read += n;
      return std::int64_t(n);
    }};
    check(p.state->load(p.plugin, &is), "state loads");
    check(!has(buffer.text, "voices"), "the saved chunk has no Voices line");
    double preset = -1;
    p.params->get_value(p.plugin, 0xB00001, &preset);
    check(int(preset) == presets[1], "the preset comes back");
    if (page_row) {
      Events none;
      process(p.plugin, none, l, r, 64);   // asks for the main thread, which builds the loaded state's engine
      if (callback_requested) { callback_requested = false; p.plugin->on_main_thread(p.plugin); }
      process(p.plugin, none, l, r, 64);   // takes it
      check(bridge->engine_v(p.plugin, page_row->id) == page_value, "a row set only from the page comes back from the saved state");
    }
    check(has(bridge->frame(p.plugin), "\"state\":\"horde-engine-preview 2"), "the page hears of a load once, with the state");
    check(!has(bridge->frame(p.plugin), "\"state\""), "and not in the next frame");
  }

  // ---- Polyphony is the one voice control (the human, 2026-11-05) ----
  const horde::ParamInfo* polyphony = horde::find_param("voice.polyphony");
  check(polyphony != nullptr, "the engine has voice.polyphony");
  long long seq = 2000;
  if (polyphony && page_row) {
    // Each preset's own polyphony, read from a probe engine of the same build, not through the plug-in. Every bank
    // preset asks for 16 on 1ad270a, so the cap's other side (8 or fewer kept) is held in bridge_test, on the rule.
    int above = -1, counted = 0;
    for (int i = 0; i < kBankSize; ++i) {
      auto probe = horde::make_engine();
      if (!probe || !probe->prepare(44100, 64) || !probe->load_state(kBank[i].state).ok) continue;
      const double count = polyphony->lo + std::round(probe->param_v(polyphony->id) * double(polyphony->steps - 1));
      if (count > 8) {
        ++counted;
        if (above < 0) above = i;
      }
    }
    std::printf("     %d of %d bank presets ask for more than 8 voices\n", counted, kBankSize);
    const double eight = v_of_count(*polyphony, 8);
    check(above >= 0, "the bank has a preset above 8 voices");
    if (above >= 0) {
      const std::string frame = send_until_acked(p.plugin, bridge, seq, "\"type\":\"preset\",\"index\":" + std::to_string(above));
      check(bridge->engine_v(p.plugin, polyphony->id) == eight && param_in(frame, "voice.polyphony") == eight,
            std::string(kBank[above].name) + ": a preset above 8 voices loads at 8, in the engine and in the frame that completes the load");
    }
    {
      // A polyphony chosen by hand and saved stands through a load, below the cap and above it.
      for (int count : {6, 12}) {
        char chunk[160];
        std::snprintf(chunk, sizeof chunk, "horde-engine-preview 2\npreset %d\n%u %.17g\n", above, polyphony->id, v_of_count(*polyphony, count));
        struct Buffer { std::string text; std::size_t read = 0; } buffer{chunk};
        clap_istream_t is{&buffer, [](const clap_istream_t* s, void* data, std::uint64_t size) -> std::int64_t {
          auto* b = static_cast<Buffer*>(s->ctx);
          const std::size_t n = std::min<std::size_t>(size, b->text.size() - b->read);
          std::memcpy(data, b->text.data() + b->read, n);
          b->read += n;
          return std::int64_t(n);
        }};
        p.state->load(p.plugin, &is);
        one_block(p.plugin);
        if (callback_requested) { callback_requested = false; p.plugin->on_main_thread(p.plugin); }
        one_block(p.plugin);
        check(bridge->engine_v(p.plugin, polyphony->id) == v_of_count(*polyphony, count),
              "a saved polyphony of " + std::to_string(count) + ", set by hand, stands through the load");
      }
    }
    const double twelve = v_of_count(*polyphony, 12);
    char body[160];
    std::snprintf(body, sizeof body, "\"type\":\"set\",\"values\":{\"voice.polyphony\":%.17g}", twelve);
    (void)send_until_acked(p.plugin, bridge, seq, body);
    check(bridge->engine_v(p.plugin, polyphony->id) == twelve, "polyphony set to 12 by hand stands in the engine");
    const int sounding = voices_for_chord(p.plugin, bridge, 12);
    std::printf("     a held 12-note chord at polyphony 12: %d voices sounding\n", sounding);
    check(sounding == 12, "and a held 12-note chord sounds 12 voices");
    (void)send_until_acked(p.plugin, bridge, seq, "\"type\":\"plugin\",\"voices\":4");
    check(bridge->engine_v(p.plugin, polyphony->id) == twelve, "an old page's Voices message is acknowledged and changes nothing");
  }

  // ---- The full-row mapping audit ----
  {
    int passed = 0;
    const std::vector<std::string> failed = audit_rows(p.plugin, bridge, seq, passed);
    std::string list;
    for (const auto& key : failed) list += " " + key;
    std::printf("     mapping audit: %d of %d rows read back exactly%s%s\n", passed, engine_rows, failed.empty() ? "" : "; failed:", list.c_str());
    check(failed.empty() && passed == engine_rows, "every engine row set through the bridge reads back its exact v in the engine and the next frame");
    (void)send_until_acked(p.plugin, bridge, seq, "\"type\":\"preset\",\"index\":" + std::to_string(presets[0]));   // a load puts every row back
  }

  // ---- Every row automatable through the host (B632) ----
  {
    int passed = 0;
    const std::vector<std::string> failed = audit_host(p.plugin, p.params, bridge, 0, passed);
    std::string list;
    for (const auto& key : failed) list += " " + key;
    std::printf("     host automation: %d of %d rows read back exactly%s%s\n", passed, engine_rows, failed.empty() ? "" : "; failed:", list.c_str());
    check(failed.empty() && passed == engine_rows, "every engine row set by a host param event reads back in the engine, the host and the frame");
    int shifted_passed = 0;
    const std::vector<std::string> shifted = audit_host(p.plugin, p.params, bridge, 1, shifted_passed);
    check(!shifted.empty(), "control: with each event sent to the next id, the host audit fails " + std::to_string(shifted.size()) + " rows");
    (void)send_until_acked(p.plugin, bridge, seq, "\"type\":\"preset\",\"index\":" + std::to_string(presets[0]));
  }

  // ---- Pitch bend and the bank's glide (B632) ----
  const int glider = find_preset("Trance jitter");
  const double glide_s = glider >= 0 ? kBank[glider].glide_s : 0;
  check(glider >= 0 && glide_s > 0, "Trance jitter carries a glide time from B.1 (" + std::to_string(glide_s) + " s)");
  {
    (void)send_until_acked(p.plugin, bridge, seq, "\"type\":\"preset\",\"index\":" + std::to_string(glider));   // a fresh engine: nothing to glide from
    double centred = NAN, sounding = NAN;
    const double bent = bent_pitch(p.plugin, bridge, &centred, &sounding);
    const double f60 = ref_twelve_tet(60);
    std::printf("     bend: pitch input %.6f Hz bent, %.6f Hz centred; sounding pitch %.6f Hz bent\n", bent, centred, sounding);
    check(bent == f60 * std::pow(2.0, 2.0 / 12.0), "a full MIDI pitch bend reaches the note's pitch input at +2 semitones exactly");
    check(centred == f60, "and the centre brings it back to its 12-TET pitch exactly");
  }
  if (glider >= 0) {
    (void)send_until_acked(p.plugin, bridge, seq, "\"type\":\"preset\",\"index\":" + std::to_string(glider));
    constexpr int kSamples = 3000;
    const std::vector<double> seen = glide_run(p.plugin, bridge, kSamples);
    const double rate = 44100;
    const RefNote a{0, ref_twelve_tet(60), 0};
    const RefNote b{500, ref_twelve_tet(64), ref_law(a, 500, rate, glide_s)};
    const RefNote c{1000, ref_twelve_tet(67), ref_law(b, 1000, rate, glide_s)};
    const RefNote wrong{1000, ref_twelve_tet(67), ref_law(a, 1000, rate, glide_s)};   // from the first note, not the latest
    std::vector<double> want, linear, from_wrong;
    for (int k = 0; k < kSamples; ++k) {
      want.push_back(ref_played(c, 1000 + k, rate, glide_s));
      from_wrong.push_back(ref_played(wrong, 1000 + k, rate, glide_s));
      const double x = std::min(1.0, (k / rate) / glide_s);
      linear.push_back(double(float(c.from + (c.f - c.from) * x)));
    }
    const long miss = first_miss(seen, want);
    std::printf("     glide: C (67) from B (64, gliding from 60) over %d samples: first difference %ld; %.6f Hz -> %.6f Hz\n", kSamples, miss, seen.front(), seen.back());
    check(miss < 0, "a note glides by 10.3's law, sample for sample, from the most recently started note");
    check(first_miss(seen, linear) >= 0, "control: a linear glide reads as different");
    check(first_miss(seen, from_wrong) >= 0, "control: a glide from the wrong note (the first, not the latest) reads as different");

    (void)send_until_acked(p.plugin, bridge, seq, "\"type\":\"preset\",\"index\":" + std::to_string(glider));
    std::vector<double> chord[3];
    chord_run(p.plugin, bridge, 2000, chord);
    const RefNote n1{0, ref_twelve_tet(60), 0};
    const RefNote n2{0, ref_twelve_tet(64), ref_law(n1, 0, rate, glide_s)};
    const RefNote n3{0, ref_twelve_tet(67), ref_law(n2, 0, rate, glide_s)};
    std::vector<double> want1, want2, want3;
    for (int k = 0; k < 2000; ++k) {
      want1.push_back(ref_played(n1, k, rate, glide_s));
      want2.push_back(ref_played(n2, k, rate, glide_s));
      want3.push_back(ref_played(n3, k, rate, glide_s));
    }
    check(first_miss(chord[0], want1) < 0 && first_miss(chord[1], want2) < 0 && first_miss(chord[2], want3) < 0,
          "a chord: its first note on pitch, each later note gliding from the one before it (round 49)");
  }

  // ---- The output tap (GN-4, B632) ----
  {
    bool loud = false;
    int points = 0;
    const long miss = scope_miss(p.plugin, bridge, seq, &loud, &points);
    std::printf("     scope: %d points; first difference from the output %ld\n", points, miss);
    check(points == 128 && miss < 0 && loud, "the scope's window is the plug-in's own output, sample for sample, while a chord sounds");
  }

  // Silence is skipped (OU-4): a released chord's tail ends, and the blocks after it must be counted as skipped.
  {
    (void)play(p, presets[0], {512});
    float l[512], r[512];
    bool zero = true;
    for (int b = 0; b < 2000; ++b) {
      Events none;
      process(p.plugin, none, l, r, 512);
      for (int i = 0; i < 512 && b > 1900; ++i) zero = zero && l[i] == 0 && r[i] == 0;
    }
    check(zero, "after the tail, the output is exact zero");
    check(has(bridge->frame(p.plugin), "\"observe\":{\"sounding\":false,\"voices\":[]"), "a frame during silence claims no sound");
  }
  // Control: the same frame reads sound while a chord plays, or the check above couldn't fail.
  {
    float l[512], r[512];
    Events chord;
    for (int k = 0; k < 4; ++k) chord.note(true, 0, 900 + k, kChord[k], kVelocity);
    process(p.plugin, chord, l, r, 512);
    for (int b = 0; b < 8; ++b) {
      Events none;
      process(p.plugin, none, l, r, 512);
    }
    bridge->receive(p.plugin, "{\"type\":\"want\",\"seq\":900,\"osc\":[1],\"members\":true}");
    Events none;
    process(p.plugin, none, l, r, 512);
    const std::string frame = bridge->frame(p.plugin);
    check(has(frame, "\"observe\":{\"sounding\":true,\"voices\":[{"), "control: a frame while a chord plays claims sound");
    check(has(frame, "\"notes\":{\"key\":60,\"vel\":0.85") && has(frame, "\"gate\":true"), "its voices carry their notes from the plug-in's own events");
    check(has(frame, "\"phases\":[") && has(frame, "null]}"), "members ride when `want` asks, and an oscillator it didn't name is null");
    Events off;
    for (int k = 0; k < 4; ++k) off.note(false, 0, 900 + k, kChord[k], 0);
    process(p.plugin, off, l, r, 512);
    for (int b = 0; b < 2000; ++b) {
      Events quiet;
      process(p.plugin, quiet, l, r, 512);
    }
    check(has(bridge->frame(p.plugin), "\"sounding\":false"), "and once its tail has ended, no sound again");
  }
  // ---- The morph (B636): GUI 3's Morph page drives the clean room's own resolver, whose events reach the engine ----
  if (horde::find_param("osc1.detune") && horde::find_param("osc1.baseWave") && horde::find_param("out.level")) {
    long long mseq = 30000;
    const Corners corners = test_corners();
    const int preset = presets[0];
    // The fade's offline side plays no routes and no glide (one note), so it takes a smoke preset without routes.
    int fade_preset = -1;
    for (int i : presets)
      if (fade_preset < 0 && i >= 0 && kBank[i].n_routes == 0) fade_preset = i;

    // Morph off is the plug-in as it was: corners B to D and the position are silent and frozen (D.2, ADR-319).
    {
      morph_reset(p.plugin, bridge, mseq);
      fresh(p, preset);
      post_corners(p.plugin, bridge, mseq, corners);   // corner A is the sound while off: its edit is an edit of the sound
      post(p.plugin, bridge, mseq, "\"type\":\"position\",\"x\":1,\"y\":1");
      post(p.plugin, bridge, mseq, "\"type\":\"morph\",\"settings\":{\"morph.law\":0,\"morph.glide\":0}");
      // So the reference is corner A's test values set as plain edits.
      const auto off = settle_and_play(p.plugin);
      const auto plain = corner_reference(p, bridge, mseq, preset, corners, 0);
      check(first_difference(off, plain) == off.size() && peak(off) > 0,
            "morph off: with B to D edited and the pad at D, the sound is corner A's as plain edits, bit for bit");
      const auto on = corner_morphed(p, bridge, mseq, preset, corners, 3, 0.5);
      check(first_difference(on, plain) != on.size(), "control: the morph on at D reads as different");
    }

    // At each exact corner, the morphed output is that corner played with the morph off (SB-10), under Quantum and Blend.
    for (const double law : {0.5, 0.0}) {
      const char* name = law == 0 ? "Blend" : "Quantum";
      std::vector<float> reference[4];
      for (int k = 0; k < 4; ++k) reference[k] = corner_reference(p, bridge, mseq, preset, corners, k);
      bool exact = true;
      std::string misses;
      for (int k = 0; k < 4; ++k) {
        const auto morphed = corner_morphed(p, bridge, mseq, preset, corners, k, law);
        const std::size_t at = first_difference(morphed, reference[k]);
        if (at != morphed.size() || peak(morphed) == 0) { exact = false; misses += " " + std::string(1, char('A' + k)) + "@" + std::to_string(at); }
        if (k == 0) check(first_difference(morphed, reference[1]) != morphed.size(), std::string("control: under ") + name + ", corner A's render differs from corner B's");
      }
      check(exact, std::string("under ") + name + ", each exact corner sounds as that corner with the morph off, bit for bit" + misses);
    }

    // MO-9: under Blend the engine's value of a Morphable key is the blend of its corners at the pad's weights.
    {
      const double got = blend_at(p, bridge, mseq, preset, 0.25, 0.75), want = mo9(corners.detune, 0.25, 0.75);
      std::printf("     osc1.detune at (0.25, 0.75): engine %.17g, MO-9 %.17g\n", got, want);
      check(got == want, "moving the pad sets a Morphable key to MO-9's blend of its corners (osc1.detune at (0.25, 0.75))");
      check(got != mo9(corners.detune, 0.75, 0.25), "control: the blend at the transposed position reads as different");
      post(p.plugin, bridge, mseq, "\"type\":\"position\",\"x\":0.5,\"y\":0.25");
      blocks_of(p.plugin, 1, 256);
      const double moved = bridge->engine_v(p.plugin, horde::find_param("osc1.detune")->id);
      check(moved == mo9(corners.detune, 0.5, 0.25) && moved != got, "and moving it again follows the pad (0.5, 0.25)");
    }

    // MO-26 (B639): a source on in corner A only fades as the pad walks to B. The frame's heard level falls, and the
    // output is bit-identical to an engine and resolver driven offline by the same inputs (fade_offline).
    if (fade_preset >= 0) {
      const Fade heard = fade_plugin(p, bridge, mseq, fade_preset);
      const std::vector<float> offline_fade = fade_offline(fade_preset);
      const double start = heard.levels.front(), middle = heard.levels[kFadeBlocks / 2], end = heard.levels.back();
      std::printf("     osc 1's heard level as the pad walks A to B: %.6f, %.6f at the middle, %.6f at the end (%s)\n", start, middle, end, kBank[fade_preset].name);
      check(start > middle && middle > end, "a source on in corner A only fades as the pad moves away: the frame's heard level falls (IF-10, MO-26)");
      const std::size_t at = first_difference(heard.out, offline_fade);
      check(!offline_fade.empty() && at == heard.out.size() && peak(heard.out) > 0,
            "and the output is bit-identical to the engine and resolver driven offline by the same inputs" + (at == heard.out.size() ? std::string() : " (first difference at value " + std::to_string(at) + ")"));
      check(first_difference(heard.out, fade_plugin(p, bridge, mseq, presets[1] == fade_preset ? presets[0] : presets[1]).out) != heard.out.size(),
            "control: another preset's fade reads as different");
    } else {
      check(false, "none of the smoke presets is free of routes: the fade's offline reference has no preset to play");
    }

    // B639: morph.x and morph.y are host parameters (IF-11); host events move the resolution as the page's pad does.
    {
      const clap_id x = host_id(p, "morph.x"), y = host_id(p, "morph.y");
      const double got = host_xy(p, bridge, mseq, preset, 0.25, 0.75);
      double hx = NAN, hy = NAN;
      p.params->get_value(p.plugin, x, &hx);
      p.params->get_value(p.plugin, y, &hy);
      check(x != CLAP_INVALID_ID && y != CLAP_INVALID_ID && got == mo9(corners.detune, 0.25, 0.75) && hx == 0.25 && hy == 0.75,
            "morph.x and morph.y set as host parameters move the resolution to MO-9's blend there, and read back");
      check(got != corners.detune[0], "control: the resolution left corner A");
    }

    // MO-21 to MO-23 (B639): a waypoint bends the blend at its position.
    {
      const double plain = pinned(p, bridge, mseq, preset, "[]");
      const double bent = pinned(p, bridge, mseq, preset, "[{\"x\":0.5,\"y\":0.5,\"v\":" + num17(kPin) + "}]");
      std::printf("     osc1.detune at the centre: %.17g plain, %.17g with a waypoint of %.17g there\n", plain, bent, kPin);
      check(plain == mo9(corners.detune, 0.5, 0.5), "control: with no waypoints the centre is MO-9's blend");
      check(bent != plain && std::fabs(bent - kPin) < std::fabs(plain - kPin), "a waypoint changes the blend at its position, toward its value");
      std::string nine = "[";
      for (int i = 0; i < 9; ++i) nine += std::string(i ? "," : "") + "{\"x\":" + num17(0.1 + 0.08 * i) + ",\"y\":0.5,\"v\":0.5}";
      const double refused = pinned(p, bridge, mseq, preset, nine + "]");
      post(p.plugin, bridge, mseq, "\"type\":\"want\",\"osc\":[],\"members\":false,\"morph\":true");
      const std::string m = morph_of(send_until_acked(p.plugin, bridge, mseq, "\"type\":\"hello\""));
      check(refused == plain && number_after(m, "refusedWaypoints") >= 1, "a list of nine is refused whole (MO-21): the blend stays, and the frame counts it");
      post(p.plugin, bridge, mseq, "\"type\":\"want\",\"osc\":[1,2],\"members\":false");
    }

    // Reported, never gating: what the morph costs here. While it is on, spans are cut at every control interval (as
    // for a routed preset), so this is the price of that cut plus the resolver, on this machine under its load now.
    {
      const auto cost = [&](bool on) {
        morph_reset(p.plugin, bridge, mseq);
        fresh(p, preset);
        if (on) {
          post_corners(p.plugin, bridge, mseq, corners);
          post(p.plugin, bridge, mseq, "\"type\":\"morph\",\"settings\":{\"morph.law\":0.5,\"morph.on\":1}");
          post(p.plugin, bridge, mseq, "\"type\":\"position\",\"x\":0.4,\"y\":0.4");
        }
        blocks_of(p.plugin, 4);
        const auto t0 = std::chrono::steady_clock::now();
        (void)settle_and_play(p.plugin);
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      };
      const double off = cost(false), on = cost(true);
      std::printf("     cost of the chord and its tail, morph off %.3f s, on and still (Quantum) %.3f s: %+.0f %%\n", off, on, 100 * (on / off - 1));
    }

    // The page's morph messages reach the morph system, are acknowledged, and come back in the frame's `morph`.
    {
      (void)blend_at(p, bridge, mseq, preset, 0.25, 0.75);
      post(p.plugin, bridge, mseq, "\"type\":\"want\",\"osc\":[],\"members\":false,\"morph\":true");
      std::string frame = send_until_acked(p.plugin, bridge, mseq, "\"type\":\"hello\"");   // a LOAD: everything again
      std::string m = morph_of(frame);
      check(number_after(m, "x") == 0.25 && number_after(m, "y") == 0.75 && number_after(m, "morph.on") == 1 && number_after(m, "morph.law") == 0,
            "with want.morph the frame carries the position and D.2's settings as the morph system holds them");
      check(number_after(corner_in(m, 2), "osc1.detune") == corners.detune[2] && number_after(m, "osc1.detune", m.find("\"heard\":")) == bridge->engine_v(p.plugin, horde::find_param("osc1.detune")->id),
            "and the corners, and what the engine holds now (`heard`)");
      check(!has(morph_of(bridge->frame(p.plugin)), "\"settings\":{"), "the settings ride only when they changed");

      const long long modes_seq = mseq + 1;
      post(p.plugin, bridge, mseq, "\"type\":\"modes\",\"modes\":{\"osc1.baseWave\":\"quantum\"},\"counts\":{\"out.level\":2}");
      check(ack_of(bridge->frame(p.plugin)) < modes_seq, "control: before a block runs, the modes message isn't acknowledged");
      blocks_of(p.plugin, 1, 256);
      frame = bridge->frame(p.plugin);
      m = morph_of(frame);
      check(ack_of(frame) >= modes_seq && has(m, "\"modes\":{\"osc1.baseWave\":\"quantum\"}") && has(m, "\"counts\":{\"out.level\":2}"),
            "modes: acknowledged, and the frame carries each key's mode and count");

      const double before = bridge->engine_v(p.plugin, horde::find_param("osc1.detune")->id);
      post(p.plugin, bridge, mseq, "\"type\":\"corner\",\"index\":3,\"values\":{\"osc1.detune\":0.0625}");
      blocks_of(p.plugin, 1, 256);
      double edited[4] = {corners.detune[0], corners.detune[1], corners.detune[2], 0.0625};
      const double after = bridge->engine_v(p.plugin, horde::find_param("osc1.detune")->id);
      check(after != before && after == mo9(edited, 0.25, 0.75) && number_after(corner_in(morph_of(bridge->frame(p.plugin)), 3), "osc1.detune") == 0.0625,
            "corner: an edit of corner D reaches the resolver (the blend moves) and the frame");

      const std::uint64_t dropped_before = std::uint64_t(number_after(morph_of(bridge->frame(p.plugin)), "dropped"));
      post(p.plugin, bridge, mseq, "\"type\":\"set\",\"values\":{\"osc1.detune\":0.99}");
      blocks_of(p.plugin, 1, 256);
      check(bridge->engine_v(p.plugin, horde::find_param("osc1.detune")->id) == after &&
                std::uint64_t(number_after(morph_of(bridge->frame(p.plugin)), "dropped")) == dropped_before + 1,
            "while the morph is on, an edit of a key it resolves is refused and counted: the resolver is its only source (IF-12)");
      const horde::ParamInfo* poly = horde::find_param("voice.polyphony");
      post(p.plugin, bridge, mseq, "\"type\":\"set\",\"values\":{\"voice.polyphony\":" + num17(v_of_count(*poly, 6)) + "}");
      blocks_of(p.plugin, 1, 256);
      check(bridge->engine_v(p.plugin, poly->id) == v_of_count(*poly, 6), "control: a Device key (voice.polyphony) still takes the edit");

      post(p.plugin, bridge, mseq, "\"type\":\"save\",\"index\":1");
      check(!has(corner_in(morph_of(bridge->frame(p.plugin)), 1), "osc1.detune"), "control: before a block runs, corner B hasn't changed");
      blocks_of(p.plugin, 1, 256);
      check(number_after(corner_in(morph_of(bridge->frame(p.plugin)), 1), "osc1.detune") == after,
            "save: Save current sound writes what the engine holds into corner B (MO-25)");

      post(p.plugin, bridge, mseq, "\"type\":\"morph\",\"settings\":{\"morph.on\":0}");
      blocks_of(p.plugin, 1, 256);
      frame = bridge->frame(p.plugin);
      check(bridge->engine_v(p.plugin, horde::find_param("osc1.detune")->id) == corners.detune[0], "morph off hands over to the audible corner A (MO-20)");
      post(p.plugin, bridge, mseq, "\"type\":\"morph\",\"settings\":{\"morph.audible\":0.6666666666666666}");
      blocks_of(p.plugin, 1, 256);
      frame = bridge->frame(p.plugin);
      check(bridge->engine_v(p.plugin, horde::find_param("osc1.detune")->id) == corners.detune[2] && param_in(frame, "osc1.detune") == corners.detune[2],
            "and choosing corner C to hear while off plays C's, which the page hears as the sound's value");
      post(p.plugin, bridge, mseq, "\"type\":\"want\",\"osc\":[1,2],\"members\":false");
      blocks_of(p.plugin, 1, 256);
      check(!has(bridge->frame(p.plugin), ",\"morph\":"), "control: without want.morph the frame carries no morph member");
    }

    // State: the corners, the position, the settings and the modes save with the plug-in and load back.
    {
      double before = NAN;
      std::string chunk;
      const double after = through_state(p, bridge, mseq, preset, presets[1], &before, &chunk);
      std::printf("     a four-corner state chunk (3 keys apart per corner, the morph on, 2 modes): %zu bytes\n", chunk.size());
      check(has(chunk, "horde-engine-preview 4\n") && has(chunk, "morph position 0.25 0.75\n"), "the chunk carries the morph (version 4)");
      check(before == mo9(corners.detune, 0.25, 0.75), "control: before the save the engine holds the blend");
      check(after == before, "after another preset and a load, the engine holds the same blend: corners, settings and position came back");
      // A frame carries only what changed since the page last heard it; a hello is a LOAD and resends everything.
      post(p.plugin, bridge, mseq, "\"type\":\"want\",\"osc\":[],\"members\":false,\"morph\":true");
      const std::string m = morph_of(send_until_acked(p.plugin, bridge, mseq, "\"type\":\"hello\""));
      check(number_after(m, "x") == 0.25 && number_after(m, "y") == 0.75 && has(m, "\"modes\":{\"osc1.baseWave\":\"quantum\"}") &&
                number_after(corner_in(m, 2), "osc1.detune") == corners.detune[2],
            "and the frame shows the loaded corners, position and modes");
      check(chunk.size() < 8192, "the four-corner chunk stays small");
      std::string untouched;
      morph_reset(p.plugin, bridge, mseq);
      fresh(p, preset);
      blocks_of(p.plugin, 1, 256);
      check(save_to(p, untouched) && has(untouched, "horde-engine-preview 2\n") && !has(untouched, "morph"), "a patch that never touched the morph saves the version-2 chunk");

      // Version 4: waypoints, their law and a lock save and load back (B639).
      double pinned_before = NAN;
      std::string pinned_chunk;
      const double pinned_after = through_state(p, bridge, mseq, preset, presets[1], &pinned_before, &pinned_chunk, true);
      std::printf("     with a waypoint, the TIN law and a lock: %zu bytes\n", pinned_chunk.size());
      check(has(pinned_chunk, "horde-engine-preview 4\n") && has(pinned_chunk, "morph waypoints ") && has(pinned_chunk, "morph waypointlaw tin 0\n") &&
                has(pinned_chunk, "morph lock "),
            "the version-4 chunk carries the waypoints, the law and the lock");
      check(pinned_before != mo9(corners.detune, 0.25, 0.75), "control: before the save the waypoint bends the blend at the pad");
      check(pinned_after == pinned_before, "after another preset and a load the engine holds the same bent blend: the waypoints and law came back");
      post(p.plugin, bridge, mseq, "\"type\":\"want\",\"osc\":[],\"members\":false,\"morph\":true");
      const std::string pm = morph_of(send_until_acked(p.plugin, bridge, mseq, "\"type\":\"hello\""));
      check(has(pm, "\"waypoints\":{\"osc1.detune\":[[0.5,0.5,0.9375]]}") && has(pm, "\"waypointLaw\":{\"law\":\"tin\",\"clamp\":false}") &&
                has(pm, "\"locks\":{\"osc1.baseWave\":[0.5,-0.25,0.125]}"),
            "and the frame shows them");
      post(p.plugin, bridge, mseq, "\"type\":\"want\",\"osc\":[1,2],\"members\":false");
      post(p.plugin, bridge, mseq, "\"type\":\"want\",\"osc\":[1,2],\"members\":false");
      morph_reset(p.plugin, bridge, mseq);
      blocks_of(p.plugin, 1, 256);
    }
  } else {
    check(false, "the engine lacks osc1.detune, osc1.baseWave or out.level: the morph's checks did not run");
  }

  // The log the human's Live sessions leave for the lead.
  p.plugin->on_main_thread(p.plugin);
  p.plugin->stop_processing(p.plugin);
  p.plugin->deactivate(p.plugin);
  const std::string log_dir = std::string(home) + "/Library/Logs/horde-engine-preview";
  const std::string list = "ls " + log_dir + "/session-*.tsv > /dev/null 2>&1";
  check(std::system(list.c_str()) == 0, "the session log is written");
  // Must read above zero: the silent blocks above were skipped. The control is the plant, which never goes silent
  // in this test because it is only played, then destroyed.
  const std::string idle = "grep -q -E 'idle blocks skipped [1-9]' " + log_dir + "/session-*.tsv";
  check(std::system(idle.c_str()) == 0, "silent blocks are skipped, not rendered (OU-4), and the log counts them");
  const std::string show = "cat " + log_dir + "/session-*.tsv | head -8";
  std::fflush(stdout);
  (void)std::system(show.c_str());
  p.plugin->destroy(p.plugin);

  // B642: each overrun on its own line, with the per-preset totals kept as they were.
  {
    const OverrunLines o = overrun_lines(argv[1], presets[0]);
    check(o.summary > 0 && o.lines == o.summary,
          "every overrun the totals count has its own line in the session log (" + std::to_string(o.lines) + " of " + std::to_string(o.summary) + ")");
    check(o.ordered && o.frames_ok && o.unison_ok && o.moving > 0 && o.still > 0,
          "each line holds its time (in order), the preset, the block size, the morph's state (" + std::to_string(o.moving) + " moving, " +
              std::to_string(o.still) + " still) and both unison counts as the engine holds them");
  }

  demo_checks(argv[1], argc > 2 ? argv[2] : nullptr, argc > 3 ? argv[3] : nullptr, presets[1], engine_rows);

  if (argc > 2) {
    const OverrunLines dropped = overrun_lines(argv[2], presets[0]);
    check(dropped.summary > 0 && dropped.lines != dropped.summary, "control: a build that counts overruns but never records them reads as missing lines");
    Loaded plant;
    check(plant.open(argv[2]), "the plant build loads");
    const auto heard = play(plant, routed, {128});
    const auto reference = offline(routed);
    check(first_difference(heard, reference) != heard.size(), "control: a build with no routes misses the routed preset's render");
    const auto* plant_bridge = static_cast<const preview_bridge_ext*>(plant.plugin->get_extension(plant.plugin, kPreviewBridgeExt));
    if (plant_bridge && polyphony) {
      long long plant_seq = 0;
      char body[160];
      std::snprintf(body, sizeof body, "\"type\":\"set\",\"values\":{\"voice.polyphony\":%.17g}", v_of_count(*polyphony, 12));
      (void)send_until_acked(plant.plugin, plant_bridge, plant_seq, body);
      const int sounding = voices_for_chord(plant.plugin, plant_bridge, 12);
      check(sounding != 12, "control: with the old Voices limit of 4 put back, the 12-note chord sounds " + std::to_string(sounding) + ", not 12");
      double centred = NAN, heard = NAN;
      const double bent = bent_pitch(plant.plugin, plant_bridge, &centred, &heard);
      check(!(bent == ref_twelve_tet(60) * std::pow(2.0, 2.0 / 12.0)), "control: with bends dropped, the note's pitch never reaches +2 semitones");
      bool loud = false;
      int points = 0;
      check(scope_miss(plant.plugin, plant_bridge, plant_seq, &loud, &points) >= 0 && loud, "control: a frozen tap misses the output while a chord sounds");
      double before = NAN;
      std::string chunk;
      const double after = through_state(plant, plant_bridge, plant_seq, presets[0], presets[1], &before, &chunk);
      check(before == mo9(test_corners().detune, 0.25, 0.75) && after != before,
            "control: a chunk that forgets the morph loads without it (the plant's morph worked before the save)");
      int fade_preset = -1;
      for (int i : presets)
        if (fade_preset < 0 && i >= 0 && kBank[i].n_routes == 0) fade_preset = i;
      if (fade_preset >= 0) {
        const Fade dropped = fade_plugin(plant, plant_bridge, plant_seq, fade_preset);
        const std::vector<float> offline_fade = fade_offline(fade_preset);
        check(!(dropped.levels.back() < dropped.levels.front()) && first_difference(dropped.out, offline_fade) != dropped.out.size(),
              "control: with the resolver's levels dropped, no level falls and the output misses the offline fade");
      }
    }
    plant.plugin->stop_processing(plant.plugin);
    plant.plugin->deactivate(plant.plugin);
    plant.plugin->destroy(plant.plugin);
  } else {
    check(false, "the plant build was not given, so its control did not run");
  }
  if (argc > 3) {
    Loaded shifted;
    check(shifted.open(argv[3]), "the shifted-key-map plant loads");
    const auto* shifted_bridge = static_cast<const preview_bridge_ext*>(shifted.plugin->get_extension(shifted.plugin, kPreviewBridgeExt));
    if (shifted_bridge) {
      long long shifted_seq = 0;
      bool loud = false;
      int points = 0;
      // Before the audit below, which leaves this plant's rows scrambled and may silence it.
      check(scope_miss(shifted.plugin, shifted_bridge, shifted_seq, &loud, &points) >= 0 && loud,
            "control: a tap that hears silence while a chord sounds misses the output");
      int passed = 0;
      const std::vector<std::string> failed = audit_rows(shifted.plugin, shifted_bridge, shifted_seq, passed);
      check(!failed.empty(), "control: with the key map one row off, the mapping audit fails " + std::to_string(failed.size()) + " rows");
    }
    shifted.plugin->stop_processing(shifted.plugin);
    shifted.plugin->deactivate(shifted.plugin);
    shifted.plugin->destroy(shifted.plugin);
  } else {
    check(false, "the shifted-key-map plant was not given, so the audit's control did not run");
  }
  if (argc > 4) {
    Loaded still;
    check(still.open(argv[4]), "the dropped-position plant loads");
    const auto* still_bridge = static_cast<const preview_bridge_ext*>(still.plugin->get_extension(still.plugin, kPreviewBridgeExt));
    if (still_bridge) {
      long long still_seq = 0;
      const double got = blend_at(still, still_bridge, still_seq, presets[0], 0.25, 0.75);
      check(got == test_corners().detune[0] && got != mo9(test_corners().detune, 0.25, 0.75),
            "control: with position messages dropped, the pad moves nothing and the key stays at corner A's value");
      const double hosted = host_xy(still, still_bridge, still_seq, presets[0], 0.25, 0.75);
      check(hosted == test_corners().detune[0], "control: and morph.x and morph.y set by the host move nothing either");
    }
    still.plugin->stop_processing(still.plugin);
    still.plugin->deactivate(still.plugin);
    still.plugin->destroy(still.plugin);
  } else {
    check(false, "the dropped-position plant was not given, so the pad's control did not run");
  }

  filter_checks(argv[1], argc > 5 ? argv[5] : nullptr);

  std::printf("%s (%d failed)\n", failures ? "engine-preview smoke: RED" : "engine-preview smoke: GREEN", failures);
  return failures ? 1 : 0;
}
