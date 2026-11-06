// Not run by ./verify: it needs the clean-room engine, which isn't in this public repo or in CI yet (B568). build.sh runs it.
//
// Drives the built CLAP through its own interface, the way a host does, and holds its output against the engine's
// own offline render (horde_offline_render, written by the builder), bit for bit. The offline render shares none of
// the plug-in's code, so a pass says the shell plays the bank as the renders do: the levelled state, the routes and
// the note order all arrive intact. DT-2 is why bit-exact is the right bar: output doesn't depend on block splits.
//   smoke <horde-engine-preview.clap> [<plant.clap> [<plant-map.clap>]]
// Controls that must fail: a different preset's render must differ, and the plant build (no routes, and the retired
// Voices limit of 4 put back) must miss on a routed preset and sound 4 voices of a 12-note chord. The second plant
// shifts the bridge's key map by one row, and the full-row mapping audit must read it red. Without them, an
// always-equal comparison would read as a pass.
#include <clap/clap.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "bank.h"
#include "bridge_ext.h"
#include "flush_to_zero.h"
#include "horde_engine.h"

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

// Chooses the preset, lets the "main thread" build it, and plays the chord from the fresh engine's first sample.
std::vector<float> play(Loaded& p, int preset, const std::vector<std::uint32_t>& blocks) {
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
  const std::uint32_t n = params->count(plugin);
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

int find_preset(const char* name) {
  for (int i = 0; i < kBankSize; ++i)
    if (!std::strcmp(kBank[i].name, name)) return i;
  return -1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::printf("usage: smoke <clap> [<plant clap>]\n"); return 2; }
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
  check(int(n_params) == 1 + engine_rows, "every non-reserved engine row is a host parameter, plus Preset (" + std::to_string(1 + engine_rows) + ")");
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
    const int other = (int(was) + 7) % kBankSize;
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

  if (argc > 2) {
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

  std::printf("%s (%d failed)\n", failures ? "engine-preview smoke: RED" : "engine-preview smoke: GREEN", failures);
  return failures ? 1 : 0;
}
