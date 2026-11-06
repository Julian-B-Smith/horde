// Not run by ./verify: it needs the clean-room engine, which isn't in this public repo or in CI yet (B568). build.sh
// runs it.
//
// Writes the recorded frames GUI 3's ReplayTransport plays (docs/proposals/gui-on-engine.md section 7), so the page's
// side runs in CI while the engine can't: a fixed chord on the default preset, through the built plug-in's own bridge
// (the extension in bridge_ext.h), one snapshot every 1/30 s of audio. The replay file is a JSON array of exactly the
// objects the window would have passed hordeNativeSnapshot, in order, as gui3_browser_check reads it. Like GUI 3's
// page, the dump sends no hello: the first frame carries params, presets and Voices because nothing was sent before.
// The display file beside it holds, for a sample of v on every engine row, the engine's own reading of it: x in the
// row's unit by the engine's law, the unit, and a stepped row's option name. The engine has no display formatter, so
// these are what the page's display text is held to; it also names the engine commit, which the array has no room for.
//   frames_dump <horde-engine-preview.clap> <engine commit> <replay.json> <display.json>
// Deterministic: a fresh engine, fixed blocks, fixed notes, no clock read. The committed fixtures are
// tools/labharness/fixtures/gui3-replay.json and gui3-display.json; build.sh writes fresh ones and compares, and
// editing the committed ones is a human gate (the charter, like goldens).
#include <clap/clap.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "bridge_ext.h"
#include "horde_engine.h"
#include "manifest/manifest.h"   // the engine's own law (value_in_unit) and its rows' units and option names

namespace {

const void* host_extension(const clap_host_t*, const char*) { return nullptr; }
void host_noop(const clap_host_t*) {}
bool callback_requested = false;
void host_callback(const clap_host_t*) { callback_requested = true; }
const clap_host_t kHost = {CLAP_VERSION, nullptr, "engine-preview frames_dump", "", "", "1", host_extension, host_noop, host_noop, host_callback};

std::vector<clap_event_note_t> notes;
std::uint32_t events_size(const clap_input_events_t*) { return std::uint32_t(notes.size()); }
const clap_event_header_t* events_get(const clap_input_events_t*, std::uint32_t i) { return &notes[i].header; }
bool out_push(const clap_output_events_t*, const clap_event_header_t*) { return true; }

void block(const clap_plugin_t* plugin, std::uint32_t frames) {
  static float l[4096], r[4096];
  float* channels[2] = {l, r};
  clap_audio_buffer_t buffer{};
  buffer.data32 = channels;
  buffer.channel_count = 2;
  clap_input_events_t in{nullptr, events_size, events_get};
  clap_output_events_t out{nullptr, out_push};
  clap_process_t proc{};
  proc.frames_count = frames;
  proc.audio_outputs = &buffer;
  proc.audio_outputs_count = 1;
  proc.in_events = &in;
  proc.out_events = &out;
  plugin->process(plugin, &proc);
  notes.clear();
  if (callback_requested) {
    callback_requested = false;
    plugin->on_main_thread(plugin);
  }
}

void chord(bool on) {
  const std::int16_t keys[4] = {60, 64, 67, 71};   // smoke.cpp's chord, at its velocity
  for (int k = 0; k < 4; ++k) {
    clap_event_note_t e{};
    e.header = {sizeof e, 0, CLAP_CORE_EVENT_SPACE_ID, std::uint16_t(on ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF), 0};
    e.note_id = k + 1;
    e.port_index = 0;
    e.channel = 0;
    e.key = keys[k];
    e.velocity = on ? 0.85 : 0;
    notes.push_back(e);
  }
}

std::string num(double x) {
  if (!std::isfinite(x)) return "null";
  char b[40];
  std::snprintf(b, sizeof b, "%.9g", x);
  return b;
}

std::string quoted(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out + "\"";
}

// The build label holds a build number that counts up with every build, so its copies would make every dump differ:
// each is written as a fixed placeholder, and the display file names the engine commit instead.
std::string without_build_label(std::string frame) {
  const std::string tag = "\"build\":\"";
  for (std::size_t at = frame.find(tag); at != std::string::npos; at = frame.find(tag, at + tag.size())) {
    const std::size_t end = frame.find('"', at + tag.size());
    frame.replace(at + tag.size(), end - at - tag.size(), "(build label)");
  }
  return frame;
}

bool write_file(const char* path, const std::string& text) {
  std::FILE* f = std::fopen(path, "w");
  if (!f) return false;
  std::fwrite(text.data(), 1, text.size(), f);
  return std::fclose(f) == 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 5) { std::printf("usage: frames_dump <clap> <engine commit> <replay.json> <display.json>\n"); return 2; }
  char home[] = "/tmp/engine-preview-frames-XXXXXX";   // the session log goes under $HOME: never the human's
  if (!mkdtemp(home)) return 2;
  setenv("HOME", home, 1);

  std::string binary = argv[1];
  const std::string name = binary.substr(binary.rfind('/') + 1);
  binary += "/Contents/MacOS/" + name.substr(0, name.rfind('.'));
  void* library = dlopen(binary.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!library) { std::printf("dlopen: %s\n", dlerror()); return 1; }
  const auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(library, "clap_entry"));
  entry->init(argv[1]);
  const auto* factory = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
  const clap_plugin_t* plugin = factory->create_plugin(factory, &kHost, factory->get_plugin_descriptor(factory, 0)->id);
  if (!plugin || !plugin->init(plugin) || !plugin->activate(plugin, 44100, 1, 512) || !plugin->start_processing(plugin)) return 1;
  const auto* bridge = static_cast<const preview_bridge_ext*>(plugin->get_extension(plugin, kPreviewBridgeExt));
  if (!bridge) { std::printf("the plug-in has no bridge extension\n"); return 1; }

  // The page's opening, as GUI 3's Main page sends it: both oscillators, with members, so the fixture has phases.
  bridge->receive(plugin, "{\"type\":\"want\",\"seq\":1,\"osc\":[1,2],\"members\":true,\"cycles\":false}");

  constexpr std::uint32_t kFrameSamples = 1470;   // 1/30 s at 44.1 kHz
  constexpr std::uint32_t kBlock = 490;           // three blocks a frame
  constexpr int kFrames = 30, kOffFrame = 15;     // 1 s: the chord held half of it, then its release
  std::vector<std::string> frames;
  for (int f = 0; f < kFrames; ++f) {
    for (std::uint32_t b = 0; b < kFrameSamples / kBlock; ++b) {
      if (b == 0 && f == 0) chord(true);
      if (b == 0 && f == kOffFrame) chord(false);
      block(plugin, kBlock);
    }
    frames.push_back(without_build_label(bridge->frame(plugin)));
  }

  std::string replay = "[\n";
  for (std::size_t f = 0; f < frames.size(); ++f) replay += frames[f] + (f + 1 < frames.size() ? ",\n" : "\n");
  replay += "]\n";

  std::string out = "{\n\"format\": \"horde-engine-preview-display/1\",\n";
  out += "\"engine\": " + quoted(argv[2]) + ",\n";
  out += "\"note\": \"Recorded by tools/engine-preview/frames_dump.cpp (B628), with gui3-replay.json beside it: " + std::to_string(kFrames) +
         " frames of 1470 samples at 44.1 kHz, chord 60 64 67 71 at velocity 0.85 on the default preset from frame 0, released at frame " +
         std::to_string(kOffFrame) + ". Editing either file is a human gate.\",\n";
  out += "\"rows\": [\n";
  const double kSamples[] = {0, 0.25, 0.5, 0.75, 1};
  bool first = true;
  for (std::size_t i = 0; i < horde::manifest_size(); ++i) {
    const horde::ParamInfo& p = horde::manifest_param(i);
    if (p.reserved) continue;
    const horde::ParamText& text = horde::param_text(horde::param_index(p.id));
    std::vector<std::string> options;
    if (text.options) {
      std::string all = text.options, part;
      for (char c : all) {
        if (c == '|') { options.push_back(part); part.clear(); }
        else part += c;
      }
      options.push_back(part);
    }
    out += std::string(first ? "" : ",\n") + "{\"key\": " + quoted(p.key) + ", \"unit\": " + quoted(text.unit ? text.unit : "") + ", \"samples\": [";
    first = false;
    for (int k = 0; k < 5; ++k) {
      const double v = kSamples[k];
      const double x = horde::value_in_unit(p, v);
      out += std::string(k ? ", " : "") + "{\"v\": " + num(v) + ", \"x\": " + num(x);
      if (std::isinf(x)) out += ", \"x_is\": " + quoted(x < 0 ? "-inf" : "inf");
      if (p.law == horde::Law::Step) {
        const int index = int(std::lround(x));
        // A stepped row with named options shows the name; one of consecutive integers shows the number (5.1).
        out += ", \"option\": " + (options.empty() ? quoted(std::to_string(int(std::lround(p.lo)) + index)) : quoted(options[std::size_t(index)]));
      }
      out += "}";
    }
    out += "]}";
  }
  out += "\n]\n}\n";

  if (!write_file(argv[3], replay) || !write_file(argv[4], out)) return 1;
  std::printf("frames_dump: %d frames, %zu bytes, to %s; display samples, %zu bytes, to %s\n", kFrames, replay.size(), argv[3], out.size(), argv[4]);
  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);
  plugin->destroy(plugin);
  return 0;
}
