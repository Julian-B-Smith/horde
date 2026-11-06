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
  std::vector<const clap_event_header_t*> order;
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

std::vector<float> offline(int preset) {
  std::string phrase = "{\"events\":[";
  char line[160];
  const char* comma = "";
  for (int k = 0; k < 4; ++k) {
    std::snprintf(line, sizeof line, "%s{\"sample\":0,\"time_s\":0,\"type\":\"note_on\",\"note_id\":%d,\"channel\":0,\"key\":%d,\"velocity\":%.17g}", comma, k + 1, kChord[k], kVelocity);
    phrase += line;
    comma = ",";
  }
  for (int k = 0; k < 4; ++k) {
    std::snprintf(line, sizeof line, ",{\"sample\":%u,\"time_s\":1.5,\"type\":\"note_off\",\"note_id\":%d,\"channel\":0,\"key\":%d}", kOff, k + 1, kChord[k]);
    phrase += line;
  }
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
  check(n_params <= 64, "at most 64 host parameters, so Live shows the panel without the plug-in's window");
  check(n_params == 48, "48 of them: 47 rows and Preset, with no separate Voices limit (\"One control: Polyphony\")");
  clap_param_info_t info;
  check(p.params->get_info(p.plugin, 0, &info) && !std::strcmp(info.name, "Preset"), "the first parameter is Preset");

  // ---- GUI 3's bridge (B628), driven through the plug-in's own extension, the path the window takes ----
  const auto* bridge = static_cast<const preview_bridge_ext*>(p.plugin->get_extension(p.plugin, kPreviewBridgeExt));
  check(bridge != nullptr, "the plug-in offers the bridge extension");
  if (!bridge) return 1;
  // A row the host never sees: the bridge must carry it to the engine on its own. The first continuous one outside
  // the host parameters.
  const horde::ParamInfo* page_row = nullptr;
  for (std::size_t i = 0; i < horde::manifest_size() && !page_row; ++i) {
    const horde::ParamInfo& row = horde::manifest_param(i);
    if (row.reserved || row.law != horde::Law::Lin) continue;
    bool host = false;
    for (std::uint32_t k = 0; k < n_params && !host; ++k) host = p.params->get_info(p.plugin, k, &info) && info.id == row.id;
    if (!host) page_row = &row;
  }
  check(page_row != nullptr, "the engine has a continuous row that isn't a host parameter");
  int engine_rows = 0;
  for (std::size_t i = 0; i < horde::manifest_size(); ++i) engine_rows += horde::manifest_param(i).reserved ? 0 : 1;
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
