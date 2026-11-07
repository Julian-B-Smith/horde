// Not run by ./verify: it needs the clean-room engine, which isn't in this public repo or in CI yet (B568). B642.
//
// The morph drop-out probe. Drives the built CLAP through its own interface, as smoke.cpp does, holds a chord for many
// seconds while the pad moves over four corners that differ (a source on in some corners and off in others, waves and
// blade modes, unison counts, Morphable values), and records three things per block: what process() cost against the
// block's time, the output, and what the plug-in's frame says changed (the engine's held values, the morph levels).
// Then it looks for drop-outs in the output and prints each with the blocks around it.
//   preview-morph-probe <horde-engine-preview.clap> [options]
//     --law blend|quantum|stepped|off   the morph's law, or the morph off (the must-read-none control)
//     --sr 44100|48000 --block 128|512  the host's rate and block
//     --seconds S                       how long the chord is held (default 24)
//     --unison N                        the largest unison count a corner asks for (default 9)
//     --unison-all N                    every corner's osc 1 unison at N (with --law off: corner A's sound at N)
//     --notes N                         how many notes the held chord has, 1 to 8 (default 4; the plug-in caps polyphony at 8)
//     --bare                            with --law off: the preset as it loads (Trance jitter), only its unison set
//     --plant-gap                       zero 20 ms in the middle of the hold after rendering (the must-fail control)
//     --flood                           with each position, the page's refused `set` of every morphing key (device.js)
//     --still                           the morph on but the pad never moves
//     --pad-rate HZ                     position messages per second (default 60, the page's pointer rate)
//     --path tour|lissajous|both        the pad's path (default both)
//     --no-want                         the page never asks for the morph member (no IF-10 reads on the audio thread)
//     --wav FILE --csv FILE             the output, and one row per block
//     --pair CLAP [--pair-law L] [--pair-unison-all N]
//                                       a second instance (another build, or the same one under law L) run block by
//                                       block beside the first, in alternating order: paired timings, and are the
//                                       outputs bit-identical? Drop-outs are then judged on the first.
// Drop-out: a 5 ms window whose RMS is more than 30 dB below the median of its neighbours within 200 ms, while the
// chord is held and the neighbours are louder than -80 dBFS. The controls: --plant-gap must read one, --law off none.
#include <clap/clap.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <time.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "bridge_ext.h"
#include "horde_engine.h"

namespace {

bool callback_requested = false;
const void* host_extension(const clap_host_t*, const char*) { return nullptr; }
void host_noop(const clap_host_t*) {}
void host_callback(const clap_host_t*) { callback_requested = true; }
const clap_host_t kHost = {CLAP_VERSION, nullptr, "engine-preview morph probe", "", "", "1", host_extension, host_noop, host_noop, host_callback};

struct Events {
  std::vector<clap_event_note_t> notes;
  std::vector<const clap_event_header_t*> order;
  void note(bool on, std::uint32_t time, std::int32_t id, std::int16_t key, double velocity) {
    clap_event_note_t e{};
    e.header = {sizeof e, time, CLAP_CORE_EVENT_SPACE_ID, std::uint16_t(on ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF), 0};
    e.note_id = id; e.port_index = 0; e.channel = 0; e.key = key; e.velocity = velocity;
    notes.push_back(e);
  }
  void seal() {
    order.clear();
    for (auto& e : notes) order.push_back(&e.header);
  }
};
std::uint32_t events_size(const clap_input_events_t* l) { return std::uint32_t(static_cast<Events*>(l->ctx)->order.size()); }
const clap_event_header_t* events_get(const clap_input_events_t* l, std::uint32_t i) { return static_cast<Events*>(l->ctx)->order[i]; }
bool out_push(const clap_output_events_t*, const clap_event_header_t*) { return true; }

double thread_cpu() {
  timespec t;
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
  return double(t.tv_sec) + 1e-9 * double(t.tv_nsec);
}
double last_cpu = 0;   // the last process() call's thread CPU time: what it cost, whatever else the machine was doing

double process(const clap_plugin_t* plugin, Events& events, float* l, float* r, std::uint32_t frames) {
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
  const auto t0 = std::chrono::steady_clock::now();
  const double c0 = thread_cpu();
  plugin->process(plugin, &proc);
  last_cpu = thread_cpu() - c0;
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// v of a row from its value in the row's own terms: an option index or a count for a Step row, else v itself.
double v_of(const char* key, double x) {
  const horde::ParamInfo* p = horde::find_param(key);
  if (!p) { std::fprintf(stderr, "no row %s\n", key); std::exit(2); }
  if (p->law == horde::Law::Step && p->steps > 1) return (x - p->lo) / double(p->steps - 1);   // a choice's lo is 0, a count's its first
  return x;
}

struct Options {
  std::string law = "blend", path = "both", wav, csv;
  double sr = 44100, seconds = 24, pad_rate = 60;
  std::uint32_t block = 512;
  int unison = 9, unison_all = 0, notes = 4;
  bool plant = false, flood = false, still = false, want = true, bare = false;
};

// Four corners that differ in what the brief names: a source on in some and off in others (MO-26's fade, the switch
// wait, landing), waves and blade modes (Structural flips, PA-5), unison counts (PA-6) and Morphable values.
std::string corner_values(int c, int unison_max, int unison_all = 0) {
  struct Kv { const char* key; double x; };
  int u[4] = {7, 3, std::min(unison_max, 9), unison_max};
  if (unison_all > 0) u[0] = u[1] = u[2] = u[3] = unison_all;   // --unison-all: every corner's osc 1 at one count
  std::vector<Kv> kv;
  switch (c) {
    case 0: kv = {{"osc1.on", 1}, {"osc2.on", 0}, {"sub.on", 0}, {"osc1.baseWave", 2}, {"osc1.blade1.on", 1}, {"osc1.blade1.mode", 0},
                  {"osc1.detune", 0.14}, {"mix.osc1", 0.909}, {"osc2.unison", 5}, {"osc2.baseWave", 2}}; break;
    case 1: kv = {{"osc1.on", 1}, {"osc2.on", 1}, {"sub.on", 0}, {"osc1.baseWave", 4}, {"osc1.blade1.on", 1}, {"osc1.blade1.mode", 4},
                  {"osc1.detune", 0.5}, {"mix.osc1", 0.8}, {"osc2.unison", 5}, {"osc2.baseWave", 1}}; break;
    case 2: kv = {{"osc1.on", 0}, {"osc2.on", 1}, {"sub.on", 1}, {"osc1.baseWave", 1}, {"osc1.blade1.on", 1}, {"osc1.blade1.mode", 1},
                  {"osc1.detune", 0.3}, {"mix.osc1", 0.85}, {"osc2.unison", 9}, {"osc2.baseWave", 2}}; break;
    default: kv = {{"osc1.on", 1}, {"osc2.on", 0}, {"sub.on", 1}, {"osc1.baseWave", 3}, {"osc1.blade1.on", 0}, {"osc1.blade1.mode", 0},
                   {"osc1.detune", 0.9}, {"mix.osc1", 1.0}, {"osc2.unison", 3}, {"osc2.baseWave", 4}}; break;
  }
  std::string s = "{";
  char b[96];
  for (const Kv& e : kv) {
    std::snprintf(b, sizeof b, "\"%s\":%.17g,", e.key, v_of(e.key, e.x));
    s += b;
  }
  std::snprintf(b, sizeof b, "\"osc1.unison\":%.17g}", v_of("osc1.unison", u[c]));
  return s + b;
}

// The pad's path at time t (s from the chord's start): a tour that dwells at each corner (so a source switches fully
// off and lands back on), then a Lissajous figure, in turn.
void pad_at(const Options& o, double t, double& x, double& y) {
  static const double kCorner[5][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0}};
  const double tour = 6.0, liss = 6.0;
  double u = std::fmod(t, tour + liss);
  const bool touring = o.path == "tour" || (o.path == "both" && u < tour);
  if (o.path == "tour") u = std::fmod(t, tour);
  if (touring) {
    const double leg = u / 1.5;   // four legs of 1.5 s: 1.0 s moving, 0.5 s dwelling at the next corner
    const int k = std::min(3, int(leg));
    const double f = std::min(1.0, (leg - k) * 1.5);
    x = kCorner[k][0] + (kCorner[k + 1][0] - kCorner[k][0]) * f;
    y = kCorner[k][1] + (kCorner[k + 1][1] - kCorner[k][1]) * f;
    return;
  }
  const double v = o.path == "lissajous" ? t : u - tour;
  x = 0.5 + 0.5 * std::sin(2 * M_PI * 0.23 * v);
  y = 0.5 + 0.5 * std::sin(2 * M_PI * 0.31 * v + 0.7);
}

std::string between(const std::string& s, const std::string& open, char close, std::size_t from = 0) {
  const std::size_t at = s.find(open, from);
  if (at == std::string::npos) return "";
  const std::size_t start = at + open.size();
  int depth = 0;
  for (std::size_t i = start; i < s.size(); ++i) {
    if (s[i] == '{' || s[i] == '[') ++depth;
    if (s[i] == close && depth == 0) return s.substr(start, i - start);
    if (s[i] == '}' || s[i] == ']') --depth;
  }
  return "";
}

struct BlockRow {
  std::uint64_t start = 0;
  std::uint32_t frames = 0;
  double took = 0, cpu = 0, x = 0, y = 0;
  std::string levels, heard, texts, counters, dropped;
  int on = 0;   // the sources the engine holds on after this block, from the frame's heard switches (bit 0 osc 1 .. bit 3 noise)
};
const char* const kSwitches[4] = {"osc1.on", "osc2.on", "sub.on", "noise.on"};
std::string sources(int on) {
  static const char* const kName[4] = {"osc1", "osc2", "sub", "noise"};
  std::string s;
  for (int k = 0; k < 4; ++k)
    if (on >> k & 1) s += std::string(s.empty() ? "" : "+") + kName[k];
  return s.empty() ? "NONE" : s;
}

// One plug-in instance and what the run recorded from it. With --pair two run side by side in one process, block by
// block in alternating order, so their timings are paired under the same machine load.
struct Instance {
  const clap_plugin_t* plugin = nullptr;
  const preview_bridge_ext* bridge = nullptr;
  long long seq = 0;
  int on = 0;
  std::vector<float> L, R;
  std::vector<BlockRow> rows;
  std::string name;
  bool open(const char* path, const Options& o) {
    name = path;
    std::string binary = path;
    struct stat info;
    if (stat(path, &info) == 0 && S_ISDIR(info.st_mode)) {
      const std::string base = binary.substr(binary.rfind('/') + 1);
      binary += "/Contents/MacOS/" + base.substr(0, base.rfind('.'));
    }
    void* library = dlopen(binary.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!library) { std::printf("dlopen: %s\n", dlerror()); return false; }
    const auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(library, "clap_entry"));
    if (!entry || !entry->init(path)) return false;
    const auto* factory = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    plugin = factory->create_plugin(factory, &kHost, factory->get_plugin_descriptor(factory, 0)->id);
    if (!plugin || !plugin->init(plugin) || !plugin->activate(plugin, o.sr, 1, o.block) || !plugin->start_processing(plugin)) return false;
    bridge = static_cast<const preview_bridge_ext*>(plugin->get_extension(plugin, kPreviewBridgeExt));
    return bridge != nullptr;
  }
  void post(const std::string& body) { bridge->receive(plugin, ("{\"seq\":" + std::to_string(++seq) + "," + body + "}").c_str()); }
  void main_thread() { plugin->on_main_thread(plugin); }   // harmless when nothing asked: it builds only what is due
  void idle(int n, std::uint32_t block) {
    std::vector<float> silent(2 * block);
    for (int b = 0; b < n; ++b) {
      Events none;
      process(plugin, none, silent.data(), silent.data() + block, block);
      main_thread();
    }
  }
  void read_switches(const std::string& heard) {
    for (int k = 0; k < 4; ++k) {
      const std::size_t at = heard.find(std::string("\"") + kSwitches[k] + "\":");
      if (at != std::string::npos) on = std::strtod(heard.c_str() + at + std::strlen(kSwitches[k]) + 3, nullptr) > 0.5 ? on | 1 << k : on & ~(1 << k);
    }
  }
  // The morph settings and corners (or, for the control, corner A as plain edits), as the page sends them.
  void setup(const Options& o) {
    post("\"type\":\"hello\"");
    idle(4, o.block);
    bridge->frame(plugin);
    // The plug-in's default preset is the one the human played (Trance jitter); its polyphony is capped at 8 on load.
    if (o.law != "off") {
      for (int c = 0; c < 4; ++c) post("\"type\":\"corner\",\"index\":" + std::to_string(c) + ",\"values\":" + corner_values(c, o.unison, o.unison_all));
      const double law = o.law == "blend" ? 0 : o.law == "quantum" ? 0.5 : 1;
      char b[160];
      std::snprintf(b, sizeof b, "\"type\":\"morph\",\"settings\":{\"morph.law\":%g,\"morph.on\":1}", law);
      post(b);
    } else {
      // The control: the morph off, so the chord plays corner A's sound as plain edits, and the pad's moves change nothing.
      if (o.bare) {   // --bare: the preset as it loads, only osc 1's unison set (--unison-all)
        char b[96];
        std::snprintf(b, sizeof b, "{\"osc1.unison\":%.17g}", v_of("osc1.unison", o.unison_all > 0 ? o.unison_all : 1));
        post(std::string("\"type\":\"set\",\"values\":") + b);
      } else {
        post("\"type\":\"set\",\"values\":" + corner_values(0, o.unison, o.unison_all));
      }
    }
    post("\"type\":\"want\",\"osc\":[],\"members\":false,\"morph\":true");
    idle(8, o.block);
    // What the engine holds now: every row rides in the first morph frame after `want`; read the switches from it.
    read_switches(between(bridge->frame(plugin), "\"heard\":{", '}'));
    if (!o.want) {   // --no-want: the page never asks for the morph member, so the audio thread makes no IF-10 reads
      post("\"type\":\"want\",\"osc\":[],\"members\":false");
      idle(2, o.block);
      bridge->frame(plugin);
    }
  }
  void block(Events& e, std::uint64_t pos, std::uint32_t n) {
    BlockRow row;
    row.start = pos;
    row.frames = n;
    row.took = process(plugin, e, L.data() + pos, R.data() + pos, n);
    row.cpu = last_cpu;
    const std::string frame = bridge->frame(plugin);
    const std::size_t m = frame.find(",\"morph\":{");
    if (m != std::string::npos) {
      row.x = std::strtod(between(frame, "\"x\":", ',', m).c_str(), nullptr);
      row.y = std::strtod(between(frame, "\"y\":", ',', m).c_str(), nullptr);
      row.levels = between(frame, "\"levels\":[", ']', m);
      row.heard = between(frame, "\"heard\":{", '}', m);
      row.texts = between(frame, "\"texts\":[", ']', m);
      row.counters = between(frame, "\"counters\":{", '}', m);
      row.dropped = between(frame, "\"dropped\":", ',', m);
    }
    read_switches(row.heard);
    row.on = on;
    rows.push_back(row);
    main_thread();
  }
  void close() {
    // The plug-in's own figures for this run land in its session log at deactivate (point HOME elsewhere to keep it).
    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);
  }
};

void timing(const Options& o, const Instance& in, std::uint64_t lead, std::uint64_t hold) {
  const double budget = double(o.block) / o.sr;
  for (int kind = 0; kind < 2; ++kind) {
    std::vector<double> share;
    int over = 0, over_half = 0;
    for (const auto& r : in.rows) {
      if (r.start < lead || r.start >= lead + hold) continue;
      const double s = (kind ? r.cpu : r.took) / (double(r.frames) / o.sr);
      share.push_back(s);
      over += s >= 1;
      over_half += s >= 0.5;   // blocks that took half their time or more
    }
    std::vector<double> sorted = share;
    std::sort(sorted.begin(), sorted.end());
    double mean = 0;
    for (double s : share) mean += s;
    mean /= std::max<std::size_t>(1, share.size());
    auto pct = [&](double q) { return sorted.empty() ? 0.0 : sorted[std::min(sorted.size() - 1, std::size_t(q * double(sorted.size())))]; };
    std::printf("%s (process(), held span, %zu blocks of %u at %.0f Hz, budget %.2f ms): mean %.1f %%, p50 %.1f %%, p99 %.1f %%, worst %.1f %%, >=50 %% %d, overruns %d\n",
                kind ? "thread CPU" : "wall      ", share.size(), o.block, o.sr, 1e3 * budget, 100 * mean, 100 * pct(0.5), 100 * pct(0.99), 100 * (sorted.empty() ? 0 : sorted.back()), over_half, over);
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: preview-morph-probe <clap> [options]\n"); return 2; }
  Options o;
  std::string pair, pair_law;
  int pair_unison = -1;
  for (int i = 2; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--law") o.law = next();
    else if (a == "--sr") o.sr = std::atof(next().c_str());
    else if (a == "--block") o.block = std::uint32_t(std::atoi(next().c_str()));
    else if (a == "--seconds") o.seconds = std::atof(next().c_str());
    else if (a == "--unison") o.unison = std::atoi(next().c_str());
    else if (a == "--unison-all") o.unison_all = std::atoi(next().c_str());
    else if (a == "--plant-gap") o.plant = true;
    else if (a == "--bare") o.bare = true;
    else if (a == "--notes") o.notes = std::clamp(std::atoi(next().c_str()), 1, 8);
    else if (a == "--flood") o.flood = true;
    else if (a == "--still") o.still = true;
    else if (a == "--no-want") o.want = false;
    else if (a == "--pad-rate") o.pad_rate = std::atof(next().c_str());
    else if (a == "--path") o.path = next();
    else if (a == "--wav") o.wav = next();
    else if (a == "--csv") o.csv = next();
    else if (a == "--pair") pair = next();
    else if (a == "--pair-law") pair_law = next();
    else if (a == "--pair-unison-all") pair_unison = std::atoi(next().c_str());
    else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
  }
  std::vector<Instance> runs(pair.empty() ? 1 : 2);
  if (!runs[0].open(argv[1], o) || (runs.size() > 1 && !runs[1].open(pair.c_str(), o))) return 1;
  Options second = o;   // --pair-law: the second instance under another law (off: the same chord with the morph off)
  if (!pair_law.empty()) second.law = pair_law;
  if (pair_unison >= 0) second.unison_all = pair_unison;
  runs[0].setup(o);
  if (runs.size() > 1) runs[1].setup(second);

  const std::uint64_t lead = std::uint64_t(0.5 * o.sr), hold = std::uint64_t(o.seconds * o.sr), tail = std::uint64_t(1.5 * o.sr);
  const std::uint64_t total = lead + hold + tail;
  for (auto& in : runs) in.L.assign(total, 0.0f), in.R.assign(total, 0.0f);
  static const std::int16_t kChord[8] = {60, 64, 67, 71, 48, 55, 74, 79};   // --notes takes the first N
  const bool morph = o.law != "off";
  const double pad_every = o.sr / o.pad_rate;
  double next_pad = double(lead);
  double px = -1, py = -1;
  std::string flood;
  if (o.flood) {   // device.js's send(): every morphing row, as `set`, which the bridge refuses while the morph is on
    flood = "\"type\":\"set\",\"values\":{";
    for (std::size_t i = 0; i < horde::manifest_size(); ++i) {
      const horde::ParamInfo& p = horde::manifest_param(i);
      if (p.reserved || p.cls == horde::ParamClass::Device) continue;
      char b[96];
      std::snprintf(b, sizeof b, "\"%s\":%.6f,", p.key, 0.5);
      flood += b;
    }
    flood.back() = '}';
  }
  std::size_t b = 0;
  for (std::uint64_t pos = 0; pos < total; ++b) {
    const std::uint32_t n = std::uint32_t(std::min<std::uint64_t>(o.block, total - pos));
    // The page's messages land before the block that follows them, as its 60 Hz pointer moves arrive between callbacks.
    while (!o.still && morph && next_pad < double(pos + n) && pos >= lead && pos < lead + hold) {
      double x, y;
      pad_at(o, (next_pad - double(lead)) / o.sr, x, y);
      if (x != px || y != py) {
        char m[128];
        std::snprintf(m, sizeof m, "\"type\":\"position\",\"x\":%.9f,\"y\":%.9f", x, y);
        for (auto& in : runs) {
          in.post(m);
          if (o.flood) in.post(flood);
        }
        px = x, py = y;
      }
      next_pad += pad_every;
    }
    for (std::size_t k = 0; k < runs.size(); ++k) {
      Instance& in = runs[(k + b) % runs.size()];   // alternating which goes first, so neither always meets a warm cache
      Events e;
      if (pos <= lead && lead < pos + n) for (int j = 0; j < o.notes; ++j) e.note(true, std::uint32_t(lead - pos), 100 + j, kChord[j], 0.85);
      if (pos <= lead + hold && lead + hold < pos + n) for (int j = 0; j < o.notes; ++j) e.note(false, std::uint32_t(lead + hold - pos), 100 + j, kChord[j], 0);
      in.block(e, pos, n);
    }
    pos += n;
  }
  for (auto& in : runs) in.close();

  if (runs.size() > 1) {
    std::uint64_t first_diff = total;
    for (std::uint64_t i = 0; i < total && first_diff == total; ++i)
      if (runs[0].L[i] != runs[1].L[i] || runs[0].R[i] != runs[1].R[i]) first_diff = i;
    if (first_diff == total) std::printf("pair: the two outputs are bit-identical over all %llu samples\n", (unsigned long long)total);
    else std::printf("pair: the outputs first differ at sample %llu (%.3f s)\n", (unsigned long long)first_diff, double(first_diff) / o.sr);
    for (auto& in : runs) {
      std::printf("[%s]\n", in.name.substr(in.name.rfind('/') + 1).c_str());
      timing(o, in, lead, hold);
    }
    double a = 0, c = 0;
    for (std::size_t k = 0; k < runs[0].rows.size(); ++k)
      if (runs[0].rows[k].start >= lead && runs[0].rows[k].start < lead + hold) a += runs[0].rows[k].cpu, c += runs[1].rows[k].cpu;
    std::printf("pair: held-span thread CPU, second / first = %.3f\n", c / a);
  }
  Instance& in = runs[0];
  std::vector<float>& L = in.L;
  std::vector<float>& R = in.R;
  const std::vector<BlockRow>& rows = in.rows;

  // ---- the must-fail control: a planted gap ----
  const std::uint64_t gap_at = lead + hold / 2, gap_n = std::uint64_t(0.020 * o.sr);
  if (o.plant)
    for (std::uint64_t i = gap_at; i < gap_at + gap_n; ++i) L[i] = R[i] = 0;

  if (runs.size() == 1) timing(o, in, lead, hold);

  // ---- drop-outs: 5 ms windows more than 30 dB under the median of their neighbours within 200 ms ----
  const std::uint64_t W = std::uint64_t(std::lround(0.005 * o.sr));
  const std::size_t nw = std::size_t(total / W);
  std::vector<double> rms(nw);
  for (std::size_t w = 0; w < nw; ++w) {
    double acc = 0;
    for (std::uint64_t i = w * W; i < (w + 1) * W; ++i) acc += 0.5 * (double(L[i]) * L[i] + double(R[i]) * R[i]);
    rms[w] = std::sqrt(acc / double(W));
  }
  const int reach = 40;   // 200 ms
  const double drop = std::pow(10.0, -30.0 / 20), floor = std::pow(10.0, -80.0 / 20);
  const std::size_t first = std::size_t((lead + std::uint64_t(0.2 * o.sr)) / W), last = std::size_t((lead + hold) / W);
  std::vector<std::pair<std::size_t, std::size_t>> found;   // [first window, last window]
  double min_ratio_db = 0;
  // A drop starts at a window more than 30 dB under the median of its neighbours (within 200 ms, louder than -80 dBFS)
  // and lasts while the level stays under that level, so a long silence reads as one drop-out of its full length.
  bool in_drop = false;
  double quiet_level = 0;
  for (std::size_t w = first; w < last && w < nw; ++w) {
    if (in_drop) {
      if (rms[w] < quiet_level) { found.back().second = w; continue; }
      in_drop = false;
    }
    std::vector<double> nb;
    for (int d = -reach; d <= reach; ++d) {
      if (std::abs(d) < 3) continue;
      const long j = long(w) + d;
      if (j >= long(first) && j < long(last)) nb.push_back(rms[std::size_t(j)]);
    }
    if (nb.empty()) continue;
    std::nth_element(nb.begin(), nb.begin() + long(nb.size() / 2), nb.end());
    const double ref = nb[nb.size() / 2];
    if (ref < floor) continue;
    const double ratio = rms[w] / ref;
    min_ratio_db = std::min(min_ratio_db, 20 * std::log10(std::max(ratio, 1e-12)));
    if (ratio < drop) {
      found.emplace_back(w, w);
      in_drop = true;
      quiet_level = ref * drop;
    }
  }
  std::printf("drop-outs (5 ms windows > 30 dB under their 200 ms neighbours, chord held): %zu; deepest window %.1f dB under its neighbours\n", found.size(), min_ratio_db);
  for (const auto& [a, b] : found) {
    const std::uint64_t s0 = a * W, s1 = (b + 1) * W;
    int on_then = -1;
    for (const auto& r : rows)
      if (r.start <= s0 + W && s0 + W < r.start + r.frames) on_then = r.on;
    std::printf("  drop-out at sample %llu (%.3f s) to %llu, %.1f ms; sources on: %s\n", (unsigned long long)s0, double(s0) / o.sr, (unsigned long long)s1,
                1e3 * double(s1 - s0) / o.sr, on_then < 0 ? "?" : sources(on_then).c_str());
    for (const auto& r : rows) {
      const bool near_start = r.start + r.frames + 3 * o.block >= s0 && r.start <= s0 + 2 * o.block;
      const bool near_end = r.start + r.frames + 3 * o.block >= s1 && r.start <= s1 + 2 * o.block;
      if (!near_start && !near_end) continue;
        std::printf("    block @%llu cpu %4.0f %% pad (%.3f, %.3f) on %s levels [%s] heard {%.300s}%s\n", (unsigned long long)r.start, 100 * r.cpu / (double(r.frames) / o.sr), r.x, r.y,
                  sources(r.on).c_str(), r.levels.c_str(), r.heard.c_str(), r.texts.empty() ? "" : " texts changed");
    }
  }
  if (!rows.empty()) std::printf("last frame: dropped %s, counters {%s}\n", rows.back().dropped.c_str(), rows.back().counters.c_str());

  if (!o.csv.empty()) {
    std::ofstream f(o.csv);
    f << "start\tframes\tshare\tcpu_share\tx\ty\tlevels\tstructural_heard\n";
    for (const auto& r : rows) {
      // Only the Structural keys the frame says the engine changed: the flips (PA-5), switches and unison counts.
      std::string flips;
      std::size_t at = 0;
      while ((at = r.heard.find('"', at)) != std::string::npos) {
        const std::size_t end = r.heard.find('"', at + 1);
        const std::string key = r.heard.substr(at + 1, end - at - 1);
        const horde::ParamInfo* p = horde::find_param(key);
        const std::size_t colon = end + 1;
        const double v = std::strtod(r.heard.c_str() + colon + 1, nullptr);
        if (p && p->cls == horde::ParamClass::Structural) flips += key + "=" + std::to_string(v) + " ";
        at = r.heard.find(',', end);
        if (at == std::string::npos) break;
      }
      f << r.start << '\t' << r.frames << '\t' << r.took / (double(r.frames) / o.sr) << '\t' << r.cpu / (double(r.frames) / o.sr) << '\t' << r.x << '\t' << r.y << '\t' << r.levels << '\t' << flips << '\n';
    }
  }
  if (!o.wav.empty()) {
    std::ofstream f(o.wav, std::ios::binary);
    auto u32 = [&](std::uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
    auto u16 = [&](std::uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
    const std::uint32_t data = std::uint32_t(total * 8);
    f.write("RIFF", 4); u32(36 + data); f.write("WAVE", 4);
    f.write("fmt ", 4); u32(16); u16(3); u16(2); u32(std::uint32_t(o.sr)); u32(std::uint32_t(o.sr) * 8); u16(8); u16(32);
    f.write("data", 4); u32(data);
    for (std::uint64_t i = 0; i < total; ++i) { f.write(reinterpret_cast<const char*>(&L[i]), 4); f.write(reinterpret_cast<const char*>(&R[i]), 4); }
  }
  return 0;
}
