// UNWIRED: needs the clean-room engine, which isn't in this public repo or in CI yet (B568). build.sh runs it.
//
// Drives the built CLAP through its own interface, the way a host does, and holds its output against the engine's
// own offline render (horde_offline_render, written by the builder), bit for bit. The offline render shares none of
// the plug-in's code, so a pass says the shell plays the bank as the renders do: the levelled state, the routes and
// the note order all arrive intact. DT-2 is why bit-exact is the right bar: output doesn't depend on block splits.
//   smoke <horde-engine-preview.clap> [<plant.clap>]
// Controls that must fail: a different preset's render must differ, and the plant build (no routes) must miss on a
// routed preset. Without them, an always-equal comparison would read as a pass.
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
  clap_param_info_t info;
  check(p.params->get_info(p.plugin, 0, &info) && !std::strcmp(info.name, "Preset"), "the first parameter is Preset");

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
    check(peak(heard) > 1e-3, std::string(kBank[preset].name) + ": it makes sound");
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
    const clap_id voices_id = 0xB00002;
    Events set;
    set.param(voices_id, 3);
    float l[64], r[64];
    process(p.plugin, set, l, r, 64);
    (void)play(p, presets[1], {128});
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
    double preset = -1, voices = -1;
    p.params->get_value(p.plugin, 0xB00001, &preset);
    p.params->get_value(p.plugin, voices_id, &voices);
    check(int(preset) == presets[1] && int(voices) == 3, "the preset and Voices come back");
    Events restore;
    restore.param(voices_id, 4);
    process(p.plugin, restore, l, r, 64);
  }

  // The log the human's Live sessions leave for the lead.
  p.plugin->on_main_thread(p.plugin);
  p.plugin->stop_processing(p.plugin);
  p.plugin->deactivate(p.plugin);
  const std::string log_dir = std::string(home) + "/Library/Logs/horde-engine-preview";
  const std::string list = "ls " + log_dir + "/session-*.tsv > /dev/null 2>&1";
  check(std::system(list.c_str()) == 0, "the session log is written");
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
    plant.plugin->stop_processing(plant.plugin);
    plant.plugin->deactivate(plant.plugin);
    plant.plugin->destroy(plant.plugin);
  } else {
    check(false, "the plant build was not given, so its control did not run");
  }

  std::printf("%s (%d failed)\n", failures ? "engine-preview smoke: RED" : "engine-preview smoke: GREEN", failures);
  return failures ? 1 : 0;
}
