/*
 * auhost — a small command-line OFFLINE AudioUnit host: discovery, parameter
 * listing, deterministic note scripts, float32 WAV out, and wall-clock timing of
 * every render call. HYPERSAW, 2026-10-01, ROADMAP B381 stage 1 (the Serum 2
 * gauntlet; the plan for stages 2-4 is docs/design/serum-gauntlet-protocol.md).
 *
 * UNWIRED: a measurement tool, not a pass/fail gate — it prints, never judges,
 * and carries no threshold to weaken (the measure_cpu / measure_h2_scalpel
 * contract). It is outside tools/test_table_check.py's census by location.
 *
 * THE THIRD-PARTY LATCH. Stage 1 was ruled to load NOTHING but Apple's AUs and
 * horde's own (the human, 2026-10-01: the third-party load "may end up
 * benefitting from my oversight"). So instantiation refuses any component that
 * is not Apple's (manufacturer 'appl') or horde's ('Hsaw' 'LfTk'; the
 * manufacturer code alone is shared with other projects) unless
 * --allow-third-party is on the command line: the gate is in the code, not in a
 * reviewer's vigilance.
 * --list only reads the component registry; it never loads a plug-in binary.
 *
 * COMMANDS (run `auhost` with no arguments for the same text):
 *   auhost --list [--type aumu] [--mfr appl]
 *   auhost --params <type> <subtype> <mfr>
 *   auhost --render <type> <subtype> <mfr> --script s.json [options]
 *   auhost --horde --script s.json [options]       (h2 SCALPEL core, no AU)
 * options: --sr 48000  --block 128  --repeat 5  --warmup 8  --prime 1  --out x.wav
 *          --set <id|name>=<value> (repeatable; applied at sample 0)
 *          --seed 1 (horde only)  --allow-third-party (stage 2+, human-gated)
 *
 * SCRIPT FORMAT (JSON; times are SAMPLE frames "at", or seconds "sec" so one
 * script serves several sample rates):
 *   { "name": "c-major", "seconds": 3, "voices": 3,
 *     "events": [ {"at": 0, "program": 80}, {"at": 0, "on": 60, "vel": 100},
 *                 {"sec": 2.5, "off": 60}, {"at": 4800, "param": 0, "value": 0.5},
 *                 {"at": 0, "param": "Volume", "value": 0.5},
 *                 {"at": 0, "bend": 0.25}, {"at": 0, "cc": 1, "value": 64} ] }
 * "frames" may replace "seconds". "voices" is the count the per-voice figure
 * divides by; absent, it is the script's most simultaneously held notes.
 * Notes, bends, CCs and program changes reach the AU sample-accurately (the
 * offset argument of MusicDeviceMIDIEvent). Parameter changes are passed with
 * their in-block offset too, but most AUs apply a parameter at the block start:
 * the host cannot make an AU sample-accurate, only offer it the offset. In
 * --horde mode "param" is a core key (razor_core.h's set() table) and is
 * applied at the start of the block containing it.
 *
 * TIMING. Each repeat instantiates a FRESH unit (instantiation and Initialize
 * are not timed), replays the script from sample 0, and times every
 * AudioUnitRender call with steady_clock. The first --warmup blocks of every
 * repeat are discarded; the rest are pooled. Reported: per-block % of real time
 * (block time / block duration) as median, p90, p99 and max, and that median
 * divided by the voice count. The render thread asks for QOS_CLASS_USER_
 * INTERACTIVE, which biases Apple Silicon toward a performance core; macOS has
 * no way to pin a core, so the numbers are best-effort same-host, never
 * cross-machine. A calibration loop (1e8 dependent multiply-adds, B236's
 * pattern) is printed first so a slow machine can be told from a slow engine.
 *
 * DETERMINISM, stated rather than assumed. The HOST is deterministic: the same
 * script places the same events at the same sample offsets, every repeat starts
 * from a fresh instance at sample time 0, and the host's tempo callback reports
 * a fixed 120 BPM transport derived from the sample count (no wall clock). What
 * the host CANNOT make reproducible lives inside the plug-in: random unison or
 * oscillator start phases, noise generators seeded from the clock or a counter
 * that survives instantiation, background threads, sample loading that finishes
 * asynchronously after Initialize. So every repeat is compared with repeat 0
 * and the max |difference| and first differing frame are printed:
 * "bit-identical" is measured per run, never promised.
 * PRIMING (--prime, default 1): that many whole renders run first on fresh
 * instances and are thrown away, untimed. Measured 2026-10-01 on Apple's
 * DLSMusicDevice, chord.json: without priming, repeat 0 alone differed from
 * repeats 1..3 (max 5.0e-2 from 0.191 s on; repeats 1..3 agreed with each
 * other bit for bit); with --prime 1 all four were bit-identical. The first
 * instance in a process is not the same instrument as the next (state the
 * plug-in builds once per process). --prime 0 shows it. The --horde core is seeded (mulberry32, --seed) and is
 * deterministic by construction (razor_core.h's header).
 *
 * WHY THE --horde MODE IS A DIRECT CORE LOOP. Same-host comparable: the same
 * script reader, the same block loop, the same clock and statistics as the AU
 * path, with nothing between the clock and the core. The core holds 8 voices,
 * so a script with more simultaneous notes runs ceil(voices/8) cores, notes
 * dealt round-robin by note-on order, outputs summed. Its float instantiation
 * (render<float>) is the bench's own output type.
 *
 * Build: the `auhost` CMake target (macOS only), or directly:
 *   clang++ -std=c++20 -O3 -ffp-contract=off tools/auhost/auhost.cpp \
 *     -framework AudioToolbox -framework CoreFoundation -o auhost
 * (-ffp-contract=off is required: tools/h2_rules_check.py rule 2 — this file
 * compiles an h2 core, and the shipped build is the tested build.)
 */
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>
#include <strings.h>   // strcasecmp

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../../h2/cores/scalpel/razor_core.h"

namespace {

// ---------------------------------------------------------------- tiny JSON
// Enough JSON for a script: objects, arrays, numbers, strings, true/false/null.
// No dependency (charter: never add one); errors name the byte offset.
struct Json {
  enum Kind { Null, Bool, Num, Str, Arr, Obj } kind = Null;
  double num = 0;
  bool b = false;
  std::string str;
  std::vector<Json> arr;
  std::vector<std::pair<std::string, Json>> obj;
  const Json* get(const char* k) const {
    for (const auto& kv : obj) if (kv.first == k) return &kv.second;
    return nullptr;
  }
};

struct JsonParser {
  const std::string& s;
  size_t p = 0;
  std::string err;
  explicit JsonParser(const std::string& text) : s(text) {}
  void ws() { while (p < s.size() && (s[p] == ' ' || s[p] == '\n' || s[p] == '\r' || s[p] == '\t')) p++; }
  bool fail(const char* what) { if (err.empty()) err = std::string(what) + " at byte " + std::to_string(p); return false; }
  bool lit(const char* w) { const size_t n = std::strlen(w); if (s.compare(p, n, w) != 0) return false; p += n; return true; }
  bool string(std::string& out) {
    if (p >= s.size() || s[p] != '"') return fail("expected a string");
    p++;
    while (p < s.size() && s[p] != '"') {
      if (s[p] == '\\' && p + 1 < s.size()) {
        const char e = s[++p];
        out.push_back(e == 'n' ? '\n' : e == 't' ? '\t' : e);   // \uXXXX is not needed by a script
      } else out.push_back(s[p]);
      p++;
    }
    if (p >= s.size()) return fail("unterminated string");
    p++;
    return true;
  }
  bool value(Json& v) {
    ws();
    if (p >= s.size()) return fail("unexpected end");
    const char c = s[p];
    if (c == '{') {
      v.kind = Json::Obj; p++; ws();
      if (p < s.size() && s[p] == '}') { p++; return true; }
      for (;;) {
        ws(); std::string k; if (!string(k)) return false;
        ws(); if (p >= s.size() || s[p] != ':') return fail("expected ':'");
        p++; Json x; if (!value(x)) return false;
        v.obj.emplace_back(k, std::move(x));
        ws(); if (p < s.size() && s[p] == ',') { p++; continue; }
        if (p < s.size() && s[p] == '}') { p++; return true; }
        return fail("expected ',' or '}'");
      }
    }
    if (c == '[') {
      v.kind = Json::Arr; p++; ws();
      if (p < s.size() && s[p] == ']') { p++; return true; }
      for (;;) {
        Json x; if (!value(x)) return false;
        v.arr.push_back(std::move(x));
        ws(); if (p < s.size() && s[p] == ',') { p++; continue; }
        if (p < s.size() && s[p] == ']') { p++; return true; }
        return fail("expected ',' or ']'");
      }
    }
    if (c == '"') { v.kind = Json::Str; return string(v.str); }
    if (lit("true")) { v.kind = Json::Bool; v.b = true; return true; }
    if (lit("false")) { v.kind = Json::Bool; return true; }
    if (lit("null")) return true;
    char* end = nullptr;
    v.num = std::strtod(s.c_str() + p, &end);
    if (end == s.c_str() + p) return fail("unexpected character");
    v.kind = Json::Num; p = static_cast<size_t>(end - s.c_str());
    return true;
  }
};

// ---------------------------------------------------------------- the script
enum class Ev { On, Off, Param, Bend, CC, Program };
struct Event {
  int64_t at = 0;   // sample frame at the render rate
  Ev type = Ev::On;
  int note = 0, vel = 100, num = 0;
  bool byName = false;
  uint32_t id = 0;
  std::string name;
  double value = 0;
};
struct Script {
  std::string name;
  int64_t frames = 0;
  int voices = 0;
  std::vector<Event> events;   // sorted by time, stable (file order breaks ties)
};

bool loadScript(const char* path, double sr, Script& sc, std::string& err) {
  FILE* f = std::fopen(path, "rb");
  if (!f) { err = std::string("cannot open ") + path; return false; }
  std::string text;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
  std::fclose(f);
  JsonParser jp(text);
  Json root;
  if (!jp.value(root) || root.kind != Json::Obj) { err = jp.err.empty() ? "script is not a JSON object" : jp.err; return false; }
  auto num = [](const Json* j, double d) { return j && j->kind == Json::Num ? j->num : d; };
  if (const Json* j = root.get("name")) sc.name = j->str;
  if (const Json* j = root.get("frames")) sc.frames = static_cast<int64_t>(j->num);
  else sc.frames = static_cast<int64_t>(std::llround(num(root.get("seconds"), 2.0) * sr));
  const Json* evs = root.get("events");
  if (!evs || evs->kind != Json::Arr) { err = "script has no \"events\" array"; return false; }
  for (const Json& e : evs->arr) {
    Event x;
    if (const Json* j = e.get("at")) x.at = static_cast<int64_t>(j->num);
    else x.at = static_cast<int64_t>(std::llround(num(e.get("sec"), 0) * sr));
    x.value = num(e.get("value"), 0);
    if (const Json* j = e.get("on")) { x.type = Ev::On; x.note = static_cast<int>(j->num); x.vel = static_cast<int>(num(e.get("vel"), 100)); }
    else if (const Json* j = e.get("off")) { x.type = Ev::Off; x.note = static_cast<int>(j->num); }
    else if (const Json* j = e.get("param")) {
      x.type = Ev::Param;
      if (j->kind == Json::Str) { x.byName = true; x.name = j->str; } else x.id = static_cast<uint32_t>(j->num);
    }
    else if (const Json* j = e.get("bend")) { x.type = Ev::Bend; x.value = j->num; }
    else if (const Json* j = e.get("cc")) { x.type = Ev::CC; x.num = static_cast<int>(j->num); }
    else if (const Json* j = e.get("program")) { x.type = Ev::Program; x.num = static_cast<int>(j->num); }
    else { err = "an event has none of on/off/param/bend/cc/program"; return false; }
    if (x.at < 0 || x.at >= sc.frames) { err = "an event lies outside the script's length"; return false; }
    sc.events.push_back(x);
  }
  std::stable_sort(sc.events.begin(), sc.events.end(), [](const Event& a, const Event& b) { return a.at < b.at; });
  int held = 0, most = 0;
  for (const Event& e : sc.events) {
    if (e.type == Ev::On) most = std::max(most, ++held);
    if (e.type == Ev::Off) held = std::max(0, held - 1);
  }
  sc.voices = static_cast<int>(num(root.get("voices"), most));
  if (sc.voices < 1) sc.voices = 1;
  return true;
}

// ---------------------------------------------------------------- helpers
OSType fourcc(const char* s) {
  char b[4] = {' ', ' ', ' ', ' '};   // 'dls ' is three letters and a space
  for (int i = 0; i < 4 && s[i]; i++) b[i] = s[i];
  return (OSType(uint8_t(b[0])) << 24) | (OSType(uint8_t(b[1])) << 16) | (OSType(uint8_t(b[2])) << 8) | OSType(uint8_t(b[3]));
}
std::string fourccStr(OSType t) {
  std::string s(4, ' ');
  for (int i = 0; i < 4; i++) { const char c = static_cast<char>((t >> (24 - 8 * i)) & 0xff); s[i] = (c >= 32 && c < 127) ? c : '?'; }
  return s;
}
std::string cfString(CFStringRef r) {
  if (!r) return std::string();
  char buf[512];
  if (CFStringGetCString(r, buf, sizeof buf, kCFStringEncodingUTF8)) return buf;
  return std::string();
}
// Apple's own and horde's own: the only components stage 1 may instantiate.
// horde is matched by subtype too: the 'LfTk' manufacturer code is shared by
// other projects' plug-ins, and "ours" means horde, not the code.
bool firstParty(const AudioComponentDescription& d) {
  return d.componentManufacturer == fourcc("appl") ||
         (d.componentManufacturer == fourcc("LfTk") && d.componentSubType == fourcc("Hsaw"));
}

bool writeWav(const char* path, const std::vector<float>& L, const std::vector<float>& R, double sr) {
  FILE* f = std::fopen(path, "wb");
  if (!f) return false;
  const uint32_t frames = static_cast<uint32_t>(L.size()), ch = 2, data = frames * ch * 4;
  auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };   // WAV is little-endian, as is every Mac this runs on
  auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
  std::fwrite("RIFF", 1, 4, f); u32(4 + (8 + 18) + (8 + 4) + (8 + data));
  std::fwrite("WAVE", 1, 4, f);
  std::fwrite("fmt ", 1, 4, f); u32(18); u16(3 /* IEEE float */); u16(ch); u32(static_cast<uint32_t>(sr));
  u32(static_cast<uint32_t>(sr) * ch * 4); u16(ch * 4); u16(32); u16(0);
  std::fwrite("fact", 1, 4, f); u32(4); u32(frames);   // required for non-PCM formats
  std::fwrite("data", 1, 4, f); u32(data);
  for (uint32_t i = 0; i < frames; i++) { std::fwrite(&L[i], 4, 1, f); std::fwrite(&R[i], 4, 1, f); }
  return std::fclose(f) == 0;
}

struct Opts {
  double sr = 48000;
  int block = 128, repeat = 5, warmup = 8, prime = 1;   // prime: header, DETERMINISM
  uint32_t seed = 1;
  const char* script = nullptr;
  const char* out = nullptr;
  bool thirdParty = false;
  std::vector<std::pair<std::string, double>> sets;
};

double pct(std::vector<double> v, double q) {
  if (v.empty()) return NAN;
  std::sort(v.begin(), v.end());
  const double i = q * static_cast<double>(v.size() - 1);
  const size_t a = static_cast<size_t>(std::floor(i)), b = std::min(v.size() - 1, a + 1);
  return v[a] + (v[b] - v[a]) * (i - static_cast<double>(a));
}

void calibration() {
  volatile double sink = 0;
  double x = 0;
  const auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < 100000000; i++) x = x * 1.0000001 + 1e-9;
  sink = x;
  std::printf("calibration: 1e8 dependent multiply-adds in %.1f ms (x=%.3f)\n",
              std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(), static_cast<double>(sink));
}

// One repeat against repeat 0: the largest |difference| and the first frame
// that differs at all (-1: none). Where it starts says more than how big it is
// (a reverb tail, a note's onset, the first block).
struct Diff { double max = 0; int64_t first = -1; };
Diff compare(const std::vector<float>& L0, const std::vector<float>& R0, const std::vector<float>& L, const std::vector<float>& R) {
  Diff d;
  for (size_t i = 0; i < L.size(); i++) {
    const double e = std::max(std::fabs(double(L[i]) - L0[i]), std::fabs(double(R[i]) - R0[i]));
    if (e > 0 && d.first < 0) d.first = static_cast<int64_t>(i);
    d.max = std::max(d.max, e);
  }
  return d;
}

// The shared report: one line per run, the same for the AU and --horde paths.
// `blockSec` holds the post-warm-up block times of every repeat.
void report(const char* who, const Script& sc, const Opts& o, const std::vector<double>& blockSec,
            const std::vector<Diff>& diffs) {
  const double dur = o.block / o.sr;
  std::vector<double> p;
  p.reserve(blockSec.size());
  for (double s : blockSec) p.push_back(100 * s / dur);
  const double med = pct(p, 0.5);
  std::printf("%s | script %s | sr %.0f block %d | voices %d | repeats %d (warm-up %d blocks each, %d primed) | %zu blocks timed\n",
              who, sc.name.c_str(), o.sr, o.block, sc.voices, o.repeat, o.warmup, o.prime, p.size());
  std::printf("  %% of real time per block: median %.3f  p90 %.3f  p99 %.3f  max %.3f  | per voice (median/%d): %.4f\n",
              med, pct(p, 0.9), pct(p, 0.99), pct(p, 1.0), sc.voices, med / sc.voices);
  if (diffs.empty()) { std::printf("  determinism: one repeat, nothing to compare\n"); return; }
  double worst = 0;
  for (const Diff& d : diffs) worst = std::max(worst, d.max);
  std::printf("  determinism: repeats 1..%d vs repeat 0, max |diff| %.3e%s\n", o.repeat - 1, worst,
              worst == 0 ? " (bit-identical)" : " (NOT bit-identical)");
  if (worst > 0)
    for (size_t r = 0; r < diffs.size(); r++)
      std::printf("    repeat %zu: max |diff| %.3e, first differing frame %lld (%.4f s)\n", r + 1, diffs[r].max,
                  static_cast<long long>(diffs[r].first), diffs[r].first < 0 ? 0.0 : diffs[r].first / o.sr);
}

// ---------------------------------------------------------------- the AU side
struct HostTime { double sr = 48000; int64_t pos = 0; };
// A fixed 120 BPM, playing transport derived from the sample count: plug-ins
// that sync LFOs or arps to the host read a deterministic clock, never a wall one.
OSStatus beatAndTempo(void* user, Float64* beat, Float64* tempo) {
  const HostTime* h = static_cast<const HostTime*>(user);
  if (beat) *beat = static_cast<double>(h->pos) / h->sr * 2.0;
  if (tempo) *tempo = 120.0;
  return noErr;
}

bool findComponent(const char* t, const char* s, const char* m, AudioComponent& comp) {
  AudioComponentDescription d{fourcc(t), fourcc(s), fourcc(m), 0, 0};
  comp = AudioComponentFindNext(nullptr, &d);
  if (!comp) { std::fprintf(stderr, "auhost: no component '%s' '%s' '%s'\n", t, s, m); return false; }
  return true;
}

struct ParamInfo { AudioUnitParameterID id; std::string name; float min, max, def; uint32_t unit; };
std::vector<ParamInfo> params(AudioUnit u) {
  std::vector<ParamInfo> out;
  UInt32 size = 0;
  if (AudioUnitGetPropertyInfo(u, kAudioUnitProperty_ParameterList, kAudioUnitScope_Global, 0, &size, nullptr) != noErr) return out;
  std::vector<AudioUnitParameterID> ids(size / sizeof(AudioUnitParameterID));
  if (ids.empty() || AudioUnitGetProperty(u, kAudioUnitProperty_ParameterList, kAudioUnitScope_Global, 0, ids.data(), &size) != noErr) return out;
  for (AudioUnitParameterID id : ids) {
    AudioUnitParameterInfo info{};
    UInt32 sz = sizeof info;
    if (AudioUnitGetProperty(u, kAudioUnitProperty_ParameterInfo, kAudioUnitScope_Global, id, &info, &sz) != noErr) continue;
    std::string name = info.name;
    if ((info.flags & kAudioUnitParameterFlag_HasCFNameString) && info.cfNameString) {
      name = cfString(info.cfNameString);
      if (info.flags & kAudioUnitParameterFlag_CFNameRelease) CFRelease(info.cfNameString);
    }
    out.push_back({id, name, info.minValue, info.maxValue, info.defaultValue, static_cast<uint32_t>(info.unit)});
  }
  return out;
}

// Instantiates, formats (float32 non-interleaved stereo at sr), and initialises.
AudioUnit openUnit(AudioComponent comp, const Opts& o, HostTime* ht) {
  AudioComponentDescription d{};
  AudioComponentGetDescription(comp, &d);
  if (!firstParty(d) && !o.thirdParty) {
    std::fprintf(stderr, "auhost: REFUSED — '%s' '%s' is not Apple's or horde's, and --allow-third-party was not given "
                         "(B381: a third-party load is a human gate)\n", fourccStr(d.componentSubType).c_str(),
                 fourccStr(d.componentManufacturer).c_str());
    return nullptr;
  }
  AudioUnit u = nullptr;
  if (AudioComponentInstanceNew(comp, &u) != noErr || !u) { std::fprintf(stderr, "auhost: instantiation failed\n"); return nullptr; }
  AudioStreamBasicDescription f{};
  f.mSampleRate = o.sr; f.mFormatID = kAudioFormatLinearPCM;
  f.mFormatFlags = static_cast<AudioFormatFlags>(kAudioFormatFlagsNativeFloatPacked) | static_cast<AudioFormatFlags>(kAudioFormatFlagIsNonInterleaved);
  f.mBytesPerPacket = 4; f.mFramesPerPacket = 1; f.mBytesPerFrame = 4; f.mChannelsPerFrame = 2; f.mBitsPerChannel = 32;
  OSStatus st = AudioUnitSetProperty(u, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &f, sizeof f);
  if (st != noErr) { std::fprintf(stderr, "auhost: stream format (%.0f Hz, float32 stereo) refused: %d\n", o.sr, static_cast<int>(st)); AudioComponentInstanceDispose(u); return nullptr; }
  UInt32 maxFrames = static_cast<UInt32>(o.block);
  AudioUnitSetProperty(u, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &maxFrames, sizeof maxFrames);
  if (ht) {
    HostCallbackInfo cb{};
    cb.hostUserData = ht; cb.beatAndTempoProc = beatAndTempo;
    AudioUnitSetProperty(u, kAudioUnitProperty_HostCallbacks, kAudioUnitScope_Global, 0, &cb, sizeof cb);   // optional for the AU
  }
  if ((st = AudioUnitInitialize(u)) != noErr) { std::fprintf(stderr, "auhost: AudioUnitInitialize failed: %d\n", static_cast<int>(st)); AudioComponentInstanceDispose(u); return nullptr; }
  return u;
}

// A name -> id resolver over the unit's global parameters (exact, then case-insensitive).
bool resolve(const std::vector<ParamInfo>& ps, const std::string& key, AudioUnitParameterID& id) {
  char* end = nullptr;
  const unsigned long v = std::strtoul(key.c_str(), &end, 10);
  if (!key.empty() && *end == 0) { id = static_cast<AudioUnitParameterID>(v); return true; }
  for (const ParamInfo& p : ps) if (p.name == key) { id = p.id; return true; }
  for (const ParamInfo& p : ps) if (strcasecmp(p.name.c_str(), key.c_str()) == 0) { id = p.id; return true; }
  return false;
}

int cmdList(const char* type, const char* mfr) {
  AudioComponentDescription d{type ? fourcc(type) : 0, 0, mfr ? fourcc(mfr) : 0, 0, 0};
  AudioComponent c = nullptr;
  int n = 0;
  while ((c = AudioComponentFindNext(c, &d)) != nullptr) {
    AudioComponentDescription cd{};
    AudioComponentGetDescription(c, &cd);
    CFStringRef name = nullptr;
    AudioComponentCopyName(c, &name);
    UInt32 ver = 0;
    AudioComponentGetVersion(c, &ver);
    std::printf("%s %s %s  v%u.%u.%u  %s\n", fourccStr(cd.componentType).c_str(), fourccStr(cd.componentSubType).c_str(),
                fourccStr(cd.componentManufacturer).c_str(), ver >> 16, (ver >> 8) & 0xff, ver & 0xff, cfString(name).c_str());
    if (name) CFRelease(name);
    n++;
  }
  std::printf("%d component(s)\n", n);
  return 0;
}

int cmdParams(const char* t, const char* s, const char* m, const Opts& o) {
  AudioComponent comp;
  if (!findComponent(t, s, m, comp)) return 1;
  AudioUnit u = openUnit(comp, o, nullptr);
  if (!u) return 1;
  const auto ps = params(u);
  for (const ParamInfo& p : ps)
    std::printf("%10u  %-40s  [%g .. %g] default %g  unit %u\n", p.id, p.name.c_str(), p.min, p.max, p.def, p.unit);
  std::printf("%zu global parameter(s)\n", ps.size());
  AudioUnitUninitialize(u);
  AudioComponentInstanceDispose(u);
  return 0;
}

int cmdRender(const char* t, const char* s, const char* m, const Opts& o) {
  // An effect pulls an input bus this host does not feed (AudioUnitRender returns
  // kAudioUnitErr_NoConnection, -10876, measured on Apple's delay). Refuse plainly.
  if (fourcc(t) != kAudioUnitType_MusicDevice) { std::fprintf(stderr, "auhost: --render drives music devices ('aumu') only\n"); return 2; }
  AudioComponent comp;
  if (!findComponent(t, s, m, comp)) return 1;
  Script sc;
  std::string err;
  if (!loadScript(o.script, o.sr, sc, err)) { std::fprintf(stderr, "auhost: %s\n", err.c_str()); return 1; }
  CFStringRef cfName = nullptr;
  AudioComponentCopyName(comp, &cfName);
  const std::string who = std::string(t) + "/" + s + "/" + m + " (" + cfString(cfName) + ")";
  if (cfName) CFRelease(cfName);

  const int nb = static_cast<int>((sc.frames + o.block - 1) / o.block);
  std::vector<float> L0, R0, L, R;
  std::vector<double> timed;
  std::vector<Diff> diffs;
  std::vector<float> bl(o.block), br(o.block);
  auto* abl = static_cast<AudioBufferList*>(std::calloc(1, offsetof(AudioBufferList, mBuffers) + 2 * sizeof(AudioBuffer)));
  for (int r = -o.prime; r < o.repeat; r++) {   // r < 0: a priming render, neither timed nor kept
    HostTime ht{o.sr, 0};
    AudioUnit u = openUnit(comp, o, &ht);
    if (!u) { std::free(abl); return 1; }
    const auto ps = params(u);
    // Resolve every by-name reference before the clock starts: a typo is an error, never a silent no-op.
    std::vector<AudioUnitParameterID> ids(sc.events.size());
    for (size_t i = 0; i < sc.events.size(); i++) {
      const Event& e = sc.events[i];
      if (e.type != Ev::Param) continue;
      if (!e.byName) { ids[i] = e.id; continue; }
      if (!resolve(ps, e.name, ids[i])) { std::fprintf(stderr, "auhost: no parameter named '%s' (see --params)\n", e.name.c_str()); std::free(abl); return 1; }
    }
    for (const auto& kv : o.sets) {
      AudioUnitParameterID id;
      if (!resolve(ps, kv.first, id)) { std::fprintf(stderr, "auhost: --set: no parameter '%s'\n", kv.first.c_str()); std::free(abl); return 1; }
      AudioUnitSetParameter(u, id, kAudioUnitScope_Global, 0, static_cast<AudioUnitParameterValue>(kv.second), 0);
    }
    L.assign(static_cast<size_t>(sc.frames), 0); R.assign(static_cast<size_t>(sc.frames), 0);
    size_t ei = 0;
    for (int b = 0; b < nb; b++) {
      const int64_t t0 = static_cast<int64_t>(b) * o.block;
      for (; ei < sc.events.size() && sc.events[ei].at < t0 + o.block; ei++) {
        const Event& e = sc.events[ei];
        const UInt32 off = static_cast<UInt32>(e.at - t0);
        switch (e.type) {
          case Ev::On: MusicDeviceMIDIEvent(u, 0x90, e.note & 127, e.vel & 127, off); break;
          case Ev::Off: MusicDeviceMIDIEvent(u, 0x80, e.note & 127, 0, off); break;
          case Ev::CC: MusicDeviceMIDIEvent(u, 0xB0, e.num & 127, static_cast<UInt32>(e.value) & 127, off); break;
          case Ev::Program: MusicDeviceMIDIEvent(u, 0xC0, e.num & 127, 0, off); break;
          case Ev::Bend: {
            const int v = std::clamp(static_cast<int>(std::lround(8192 + e.value * 8192)), 0, 16383);
            MusicDeviceMIDIEvent(u, 0xE0, v & 127, v >> 7, off);
            break;
          }
          case Ev::Param: AudioUnitSetParameter(u, ids[ei], kAudioUnitScope_Global, 0, static_cast<AudioUnitParameterValue>(e.value), off); break;
        }
      }
      abl->mNumberBuffers = 2;
      abl->mBuffers[0] = {1, static_cast<UInt32>(o.block * 4), bl.data()};
      abl->mBuffers[1] = {1, static_cast<UInt32>(o.block * 4), br.data()};
      AudioUnitRenderActionFlags flags = 0;
      AudioTimeStamp ts{};
      ts.mSampleTime = static_cast<Float64>(t0);
      ts.mFlags = kAudioTimeStampSampleTimeValid;
      ht.pos = t0;
      const auto c0 = std::chrono::steady_clock::now();
      const OSStatus st = AudioUnitRender(u, &flags, &ts, 0, static_cast<UInt32>(o.block), abl);
      const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - c0).count();
      if (st != noErr) { std::fprintf(stderr, "auhost: AudioUnitRender failed: %d (block %d)\n", static_cast<int>(st), b); std::free(abl); return 1; }
      if (r >= 0 && b >= o.warmup) timed.push_back(sec);
      // The AU may hand back its own buffers (allowed when it renders in place): read through the list.
      const float* pl = static_cast<const float*>(abl->mBuffers[0].mData);
      const float* pr = static_cast<const float*>(abl->mBuffers[abl->mNumberBuffers > 1 ? 1 : 0].mData);
      for (int i = 0; i < o.block && t0 + i < sc.frames; i++) { L[t0 + i] = pl[i]; R[t0 + i] = pr[i]; }
    }
    AudioUnitUninitialize(u);
    AudioComponentInstanceDispose(u);
    if (r == 0) { L0 = L; R0 = R; }
    else if (r > 0) diffs.push_back(compare(L0, R0, L, R));
  }
  std::free(abl);
  report(who.c_str(), sc, o, timed, diffs);
  if (o.out) {
    if (!writeWav(o.out, L0, R0, o.sr)) { std::fprintf(stderr, "auhost: cannot write %s\n", o.out); return 1; }
    std::printf("  wrote %s (repeat 0, float32 stereo)\n", o.out);
  }
  return 0;
}

// ---------------------------------------------------------------- the horde core
int cmdHorde(const Opts& o) {
  using horde2::scalpel::RazorCore;
  Script sc;
  std::string err;
  if (!loadScript(o.script, o.sr, sc, err)) { std::fprintf(stderr, "auhost: %s\n", err.c_str()); return 1; }
  const int nCores = (sc.voices + RazorCore::kVoices - 1) / RazorCore::kVoices;
  const int nb = static_cast<int>((sc.frames + o.block - 1) / o.block);
  std::vector<float> L0, R0, L, R, bl(static_cast<size_t>(nCores) * o.block), br(bl.size());
  std::vector<double> timed;
  std::vector<Diff> diffs;
  for (int r = -o.prime; r < o.repeat; r++) {   // r < 0: a priming render, neither timed nor kept
    std::vector<std::unique_ptr<RazorCore>> cores;
    for (int c = 0; c < nCores; c++) {
      cores.emplace_back(new RazorCore(o.sr));   // large (8 voices x 9 members): heap, never stack
      cores[c]->seedRandom(o.seed + static_cast<uint32_t>(c));
      cores[c]->set("poly", RazorCore::kVoices);
      for (const auto& kv : o.sets)
        if (!cores[c]->set(kv.first.c_str(), kv.second)) { std::fprintf(stderr, "auhost: --set: no core key '%s'\n", kv.first.c_str()); return 1; }
      cores[c]->snap();
    }
    std::map<int, int> owner;   // note -> core it was dealt to
    int dealt = 0;
    L.assign(static_cast<size_t>(sc.frames), 0); R.assign(static_cast<size_t>(sc.frames), 0);
    size_t ei = 0;
    for (int b = 0; b < nb; b++) {
      const int64_t t0 = static_cast<int64_t>(b) * o.block;
      for (; ei < sc.events.size() && sc.events[ei].at < t0 + o.block; ei++) {
        const Event& e = sc.events[ei];
        if (e.type == Ev::On) {
          const int c = dealt++ % nCores;
          owner[e.note] = c;
          cores[c]->noteOn(e.note, 440.0 * std::pow(2.0, (e.note - 69) / 12.0), e.vel / 127.0);
        } else if (e.type == Ev::Off) {
          const auto it = owner.find(e.note);
          if (it != owner.end()) cores[it->second]->noteOff(e.note);
        } else if (e.type == Ev::Param) {
          if (!e.byName) { std::fprintf(stderr, "auhost: --horde params are core keys by name, not ids\n"); return 1; }
          for (auto& c : cores) if (!c->set(e.name.c_str(), e.value)) { std::fprintf(stderr, "auhost: no core key '%s'\n", e.name.c_str()); return 1; }
        }
        // bend / cc / program have no meaning for the bare core: ignored, as stated in the header.
      }
      // render() overwrites its buffers, so the clock covers the cores and nothing
      // else; summing the cores into the take happens after it stops (as the AU
      // path copies out after AudioUnitRender returns).
      const auto c0 = std::chrono::steady_clock::now();
      for (int c = 0; c < nCores; c++) cores[c]->render(bl.data() + c * o.block, br.data() + c * o.block, o.block);
      const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - c0).count();
      if (r >= 0 && b >= o.warmup) timed.push_back(sec);
      for (int c = 0; c < nCores; c++)
        for (int i = 0; i < o.block && t0 + i < sc.frames; i++) { L[t0 + i] += bl[c * o.block + i]; R[t0 + i] += br[c * o.block + i]; }
    }
    if (r == 0) { L0 = L; R0 = R; }
    else if (r > 0) diffs.push_back(compare(L0, R0, L, R));
  }
  char who[96];
  std::snprintf(who, sizeof who, "horde h2 SCALPEL core (%d core%s x %d voices, seed %u)", nCores, nCores > 1 ? "s" : "",
                RazorCore::kVoices, o.seed);
  report(who, sc, o, timed, diffs);
  if (o.out) {
    if (!writeWav(o.out, L0, R0, o.sr)) { std::fprintf(stderr, "auhost: cannot write %s\n", o.out); return 1; }
    std::printf("  wrote %s (repeat 0, float32 stereo)\n", o.out);
  }
  return 0;
}

void usage() {
  std::fprintf(stderr,
               "auhost — offline AudioUnit host (ROADMAP B381)\n"
               "  auhost --list [--type aumu] [--mfr appl]\n"
               "  auhost --params <type> <subtype> <mfr>\n"
               "  auhost --render <type> <subtype> <mfr> --script s.json [options]\n"
               "  auhost --horde --script s.json [options]\n"
               "options: --sr 48000 --block 128 --repeat 5 --warmup 8 --out x.wav\n"
               "         --prime 1 (discarded renders first) --set <id|name>=<value> (repeatable)\n"
               "         --seed 1 (horde) --allow-third-party\n");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { usage(); return 2; }
  Opts o;
  const char* cmd = argv[1];
  std::vector<const char*> pos;
  const char *type = nullptr, *mfr = nullptr;
  for (int i = 2; i < argc; i++) {
    const std::string a = argv[i];
    auto next = [&]() -> const char* { if (i + 1 >= argc) { std::fprintf(stderr, "auhost: %s needs a value\n", a.c_str()); std::exit(2); } return argv[++i]; };
    if (a == "--sr") o.sr = std::strtod(next(), nullptr);
    else if (a == "--block") o.block = std::atoi(next());
    else if (a == "--repeat") o.repeat = std::max(1, std::atoi(next()));
    else if (a == "--warmup") o.warmup = std::max(0, std::atoi(next()));
    else if (a == "--seed") o.seed = static_cast<uint32_t>(std::strtoul(next(), nullptr, 10));
    else if (a == "--script") o.script = next();
    else if (a == "--out") o.out = next();
    else if (a == "--type") type = next();
    else if (a == "--mfr") mfr = next();
    else if (a == "--prime") o.prime = std::max(0, std::atoi(next()));
    else if (a == "--allow-third-party") o.thirdParty = true;
    else if (a == "--set") {
      const std::string kv = next();
      const size_t eq = kv.rfind('=');
      if (eq == std::string::npos) { std::fprintf(stderr, "auhost: --set wants name=value\n"); return 2; }
      o.sets.emplace_back(kv.substr(0, eq), std::strtod(kv.c_str() + eq + 1, nullptr));
    }
    else pos.push_back(argv[i]);
  }
  if (o.block < 1 || o.block > 8192 || !(o.sr >= 8000)) { std::fprintf(stderr, "auhost: bad --block or --sr\n"); return 2; }
  pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);   // bias to a performance core (header: TIMING)
  const std::string c = cmd;
  if (c == "--list") return cmdList(type, mfr);
  if (c == "--params" && pos.size() == 3) return cmdParams(pos[0], pos[1], pos[2], o);
  if (c == "--render" && pos.size() == 3 && o.script) { calibration(); return cmdRender(pos[0], pos[1], pos[2], o); }
  if (c == "--horde" && pos.empty() && o.script) { calibration(); return cmdHorde(o); }
  usage();
  return 2;
}
