/*
 * au_bench — a headless Audio Unit CPU bench. It measures an installed AU instrument under
 * the conditions horde's CPU tool uses for PF-1, PF-3 and PF-9 (specs/SPEC-ENGINE.md §9), so
 * a reference synth's cost reads in horde's unit: % of one core = render time ÷ audio
 * duration × 100. HYPERSAW, 2026-11-07, ROADMAP B638 (ADR-380 decision 4: the human's
 * reference reading of a heavy third-party patch on this same Mac).
 *
 * Not a ./verify gate: it needs real installed Audio Units and a quiet machine, and it measures, never judges (PF-3: CPU is reported, never a gate), so it runs by hand.
 *
 * WHERE THE CONDITIONS COME FROM. The test author's CPU tool,
 * cleanroom-tests:engine2/tests/cpu/main.cpp and pf3_chord.h beside it. Copied here, not
 * re-derived, so the two numbers mean the same thing:
 *   - the chord: the k-th note on key 48 + 3 (k mod 8), velocity 0.85 (MIDI 108 = round(0.85 x 127)),
 *     all on at sample 0 of the first block (inside the untimed span), held to the end, never released;
 *   - 0.25 s untimed, then 2 s timed: round(0.25 sr / B) and round(2 sr / B) blocks;
 *   - best of 5 repeats, interleaved across the cells (every cell once per repeat), a FRESH instance
 *     per repeat (the tool makes a fresh engine in every run_once);
 *   - the result: seconds of render per second of audio, as a share of one core, from the best repeat;
 *   - the worst block: the longest single block of the timed span as a share of its duration (its
 *     deadline), the smallest of the 5 repeats' values (PF-10's reading: one disturbance does not count);
 *   - the calibration loop (10^8 dependent multiply-adds), timed before every run; the build uses the
 *     tool's Release flags (-O3 -DNDEBUG, no -ffp-contract flag), so the loop compiles to the same code
 *     and its seconds compare across the two reports;
 *   - NOT PF-3's load guard (ADR-391 A2, the human 2026-11-10: "give the AU bench the same check"): the
 *     contamination check of tools/c2a_remainder instead. 30 s of the first cell's work first, so the M3's
 *     short boost is spent; then each run is bracketed by the calibration loop and counts only if both
 *     calibrations are within 2 % of the fastest after the warm-up AND the timed span's wall time is within
 *     2 % of the rendering thread's own CPU time; the first 5 clean runs of each cell are the best of 5. Up
 *     to 20 rounds, then REFUSE (exit 2). Load is printed every round, never gating. Two must-fail controls
 *     run after the measurement (a calibration and a run beside twice as many spinning threads as cores);
 *     if either passes, nothing is reported (exit 4). A plug-in that renders on its own worker threads
 *     fails the preemption test on every run (the render thread waits); the process CPU column shows it.
 * PF-3 itself fixes 128-sample blocks; 512 is measured too because a DAW user's buffer often is.
 *
 * WHAT DIFFERS, AND WHY.
 *   - MIDI has no note ids. The tool gives each of 16 or 64 notes its own id; on MIDI a repeated key on
 *     one channel would retrigger a voice, not add one. So the k-th note goes on MIDI channel
 *     floor(k / 8) (8 notes: all on channel 1; 64: channels 1 to 8). Whether the plug-in then SOUNDS
 *     them all is its polyphony's business; the host cannot count its voices, so it prints each larger
 *     cell's cost and RMS against the 8-note cell's (a cost ratio near 1 means the extra notes were not
 *     voiced) and any parameter whose name says polyphony or quality.
 *   - The clock is mach_absolute_time (the tool's steady_clock reads the same counter on macOS).
 *   - The plug-in may render on worker threads the wall clock does not see as cost, so beside PF-3's
 *     wall-clock share the bench prints the whole process's CPU time (getrusage, all threads) per
 *     second of audio over the timed span. If it is far above the wall share, the plug-in is spreading
 *     its work across cores and "% of one core" understates it.
 *   - In process (AudioComponentInstanceNew, the AUv2 default): Live hosts AUv2 in process, and an
 *     out-of-process instance adds an IPC round trip to every render that would be charged to the
 *     plug-in.
 *   - After Initialize and the preset, the main run loop runs for 0.2 s, untimed: a plug-in that
 *     finishes loading on the main queue would otherwise never get to, because this thread renders.
 *
 * MUST-FAIL CONTROLS (--controls), on Apple's DLSMusicDevice: a slowed calibration and a preempted run are
 * rejected by the contamination check (C1, ADR-391 A2);
 * the stand-in's share is finite and plausible; a planted "skip the render" reads near 0 % and is
 * flagged silent; missing, garbage and wrong-component presets are refused; an .adv carrying the
 * stand-in's state round-trips (a parameter set before saving reads back after loading); an .adv
 * holding a VST3 device, or a corrupt buffer, is refused. The controls test the pipeline, report no
 * reference reading and judge with margins of 10x, so they run one round with no warm-up and nothing required clean.
 *
 * SILENCE IS A FAILURE. A patch that renders silence (timed-span peak below -100 dBFS) or any NaN/inf
 * is a failed measurement, never a cheap one: the cell is marked and the exit status is 3.
 *
 * THE THIRD-PARTY LATCH (as tools/auhost's, B381): anything not Apple's or horde's is refused unless
 * --allow-third-party is given. The human asked for this load (B638); the flag says so on the command line.
 *
 * Build: tools/au-bench/build.sh (into build-au-bench/, gitignored). Usage: run with no arguments.
 */
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <pthread/qos.h>
#include <time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <zlib.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

#ifdef NDEBUG
constexpr bool kRelease = true;
#else
constexpr bool kRelease = false;
#endif

// ---- PF-3's protocol, as the CPU tool runs it ------------------------------------------------
constexpr int kRepeats = 5;
constexpr double kCleanTol = 1.02;       // ADR-391 A2: calibration within 2 % of the fastest; wall within 2 % of thread CPU
constexpr double kWarmupSeconds = 30;    // ADR-391 A1: the M3's boost is spent before the first counted run
constexpr int kMaxRounds = 20;           // then refuse
constexpr double kWarmSeconds = 0.25, kTimedSeconds = 2.0;
constexpr int kVelocity = 108;                         // round(0.85 x 127): PF-3's velocity 0.85
constexpr double kSilentPeak = 1e-5;                   // -100 dBFS
int chord_key(int k) { return 48 + 3 * (k % 8); }      // pf3_chord.h, round 60
int g_first_channel = 0;                               // --first-channel, 0-based: a diagnostic, default MIDI channel 1
int chord_channel(int k) { return (g_first_channel + k / 8) % 16; }   // MIDI's stand-in for a note id (see header)

// ---- the clock ---------------------------------------------------------------------------------
double ticks_to_s(uint64_t t) {
  static const mach_timebase_info_data_t tb = [] { mach_timebase_info_data_t x{}; mach_timebase_info(&x); return x; }();
  return double(t) * double(tb.numer) / double(tb.denom) * 1e-9;
}
double thread_cpu_seconds() {   // the calling thread alone: what preemption cannot inflate
  timespec ts{};
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
  return double(ts.tv_sec) + 1e-9 * double(ts.tv_nsec);
}
double cpu_seconds() {   // the whole process, every thread: user + system
  rusage r{};
  getrusage(RUSAGE_SELF, &r);
  return double(r.ru_utime.tv_sec + r.ru_stime.tv_sec) + 1e-6 * double(r.ru_utime.tv_usec + r.ru_stime.tv_usec);
}

// ---- the machine's load: printed every round, never gating since ADR-391 A2 -------------------------------
struct Reading { double load = INFINITY, other = INFINITY; std::string who; };
Reading read_load() {
  Reading r;
  double l[1] = {INFINITY};
  getloadavg(l, 1);
  r.load = l[0];
  r.other = 0;
  FILE* p = popen("ps -A -o %cpu=,pid=,comm=", "r");   // read-only: the process table
  if (!p) { r.other = INFINITY; return r; }
  const long self = long(getpid());
  char line[2048];
  while (std::fgets(line, sizeof line, p)) {
    double cpu = 0;
    long pid = 0;
    int used = 0;
    if (std::sscanf(line, "%lf %ld %n", &cpu, &pid, &used) < 2 || pid == self) continue;
    if (cpu / 100.0 > r.other) {
      r.other = cpu / 100.0;
      std::string name = line + used;
      while (!name.empty() && (name.back() == '\n' || name.back() == ' ')) name.pop_back();
      const auto slash = name.rfind('/');
      r.who = slash == std::string::npos ? name : name.substr(slash + 1);
    }
  }
  pclose(p);
  return r;
}
void print_reading(const char* what, const Reading& r) {
  std::printf("%s  one-minute load %.2f, busiest other process %.2f cores (%s): reported, not gating\n", what, r.load, r.other, r.who.c_str());
  std::fflush(stdout);
}
double calibration() {   // 10^8 dependent multiply-adds, seconds (the CPU tool's loop, verbatim)
  volatile double seed = 1.0000001;
  double x = seed;
  const uint64_t t0 = mach_absolute_time();
  for (int i = 0; i < 100000000; ++i) x = x * 0.9999999 + 1e-9;
  const uint64_t t1 = mach_absolute_time();
  seed = x;
  return ticks_to_s(t1 - t0);
}

// Twice as many spinning threads as cores, for the controls: whatever runs beside them is preempted.
struct Spinners {
  std::atomic<bool> stop{false};
  std::vector<std::thread> spin;
  unsigned n = 2 * std::max(1u, std::thread::hardware_concurrency());
  Spinners() {
    for (unsigned i = 0; i < n; ++i)
      spin.emplace_back([this] {
        pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
        volatile double x = 1;
        while (!stop.load(std::memory_order_relaxed)) x = x * 0.999 + 1;
      });
  }
  ~Spinners() { stop = true; for (auto& t : spin) t.join(); }
};
double calibration_beside_spinners() { Spinners s; return calibration(); }

// ---- small helpers -------------------------------------------------------------------------------
std::string fourcc_str(UInt32 v) {
  std::string s(4, ' ');
  for (int i = 0; i < 4; ++i) { const char c = char((v >> (24 - 8 * i)) & 0xff); s[size_t(i)] = std::isprint(static_cast<unsigned char>(c)) ? c : '?'; }
  return s;
}
UInt32 fourcc(const char* s) { return (UInt32(uint8_t(s[0])) << 24) | (UInt32(uint8_t(s[1])) << 16) | (UInt32(uint8_t(s[2])) << 8) | UInt32(uint8_t(s[3])); }
std::string cf_string(CFStringRef s) {
  if (!s) return "";
  char buf[1024];
  return CFStringGetCString(s, buf, sizeof buf, kCFStringEncodingUTF8) ? buf : "";
}
std::string squash(const std::string& s) {   // name matching: lower case, letters and digits only
  std::string o;
  for (char c : s) if (std::isalnum(static_cast<unsigned char>(c))) o += char(std::tolower(static_cast<unsigned char>(c)));
  return o;
}
bool read_file(const std::string& path, std::string& out, std::string& why) {
  std::ifstream f(path, std::ios::binary);
  if (!f) { why = "cannot open " + path; return false; }
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  if (out.empty()) { why = path + " is empty"; return false; }
  return true;
}
std::string lower_ext(const std::string& path) {
  std::string e = std::filesystem::path(path).extension().string();
  for (char& c : e) c = char(std::tolower(static_cast<unsigned char>(c)));
  return e;
}
double db(double x) { return x > 0 ? 20 * std::log10(x) : -INFINITY; }

// ---- gzip -----------------------------------------------------------------------------------------
bool gunzip(const std::string& in, std::string& out, std::string& why) {
  z_stream z{};
  if (inflateInit2(&z, 16 + MAX_WBITS) != Z_OK) { why = "zlib would not start"; return false; }
  z.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
  z.avail_in = uInt(in.size());
  char buf[65536];
  int st = Z_OK;
  while (st == Z_OK) {
    z.next_out = reinterpret_cast<Bytef*>(buf);
    z.avail_out = sizeof buf;
    st = inflate(&z, Z_NO_FLUSH);
    if (st != Z_OK && st != Z_STREAM_END) { inflateEnd(&z); why = "the file is not valid gzip (zlib " + std::to_string(st) + ")"; return false; }
    out.append(buf, sizeof buf - z.avail_out);
  }
  inflateEnd(&z);
  return true;
}
bool gzip(const std::string& in, std::string& out) {
  z_stream z{};
  if (deflateInit2(&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) return false;
  z.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
  z.avail_in = uInt(in.size());
  char buf[65536];
  int st = Z_OK;
  while (st == Z_OK) {
    z.next_out = reinterpret_cast<Bytef*>(buf);
    z.avail_out = sizeof buf;
    st = deflate(&z, Z_FINISH);
    out.append(buf, sizeof buf - z.avail_out);
  }
  deflateEnd(&z);
  return st == Z_STREAM_END;
}

// ---- presets ----------------------------------------------------------------------------------------
// A preset is the AU's ClassInfo dictionary. It comes from an .aupreset (that dictionary as a plist),
// or from a Live .adv device preset: gzipped XML in which <AuPluginDevice> ... <AuPluginInfo> names the
// component (ComponentType / ComponentSubType / ComponentManufacturer, decimal) and <AuPreset><Buffer>
// holds the same plist, hex-encoded (the layout Live writes, read from a Live 12 set on this Mac).
struct Preset { CFPropertyListRef dict = nullptr; std::string source; };

bool dict_matches(CFDictionaryRef d, const AudioComponentDescription& want, std::string& why) {
  const struct { const char* key; UInt32 want; } keys[] = {{"type", want.componentType}, {"subtype", want.componentSubType}, {"manufacturer", want.componentManufacturer}};
  for (const auto& k : keys) {
    CFStringRef ck = CFStringCreateWithCString(nullptr, k.key, kCFStringEncodingUTF8);
    const void* v = CFDictionaryGetValue(d, ck);
    CFRelease(ck);
    if (!v) continue;   // absent: the AU decides when it is set
    SInt64 n = 0;
    if (CFGetTypeID(v) != CFNumberGetTypeID() || !CFNumberGetValue(static_cast<CFNumberRef>(v), kCFNumberSInt64Type, &n)) { why = std::string("its '") + k.key + "' is not a number"; return false; }
    if (UInt32(n) != k.want) { why = std::string("it is for another component: its ") + k.key + " is '" + fourcc_str(UInt32(n)) + "', this one's is '" + fourcc_str(k.want) + "'"; return false; }
  }
  return true;
}

bool plist_dict(const std::string& bytes, const AudioComponentDescription& want, CFPropertyListRef& out, std::string& why) {
  CFDataRef data = CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(bytes.data()), CFIndex(bytes.size()));
  CFErrorRef err = nullptr;
  CFPropertyListRef pl = CFPropertyListCreateWithData(nullptr, data, kCFPropertyListImmutable, nullptr, &err);
  CFRelease(data);
  if (err) CFRelease(err);
  if (!pl) { why = "it is not a property list"; return false; }
  if (CFGetTypeID(pl) != CFDictionaryGetTypeID()) { CFRelease(pl); why = "its property list is not a dictionary"; return false; }
  if (!dict_matches(static_cast<CFDictionaryRef>(pl), want, why)) { CFRelease(pl); return false; }
  out = pl;
  return true;
}

long long xml_int(const std::string& xml, size_t from, size_t to, const char* tag) {   // <Tag Value="123" />, or LLONG_MIN
  const std::string open = std::string("<") + tag + " Value=\"";
  const size_t p = xml.find(open, from);
  if (p == std::string::npos || p >= to) return LLONG_MIN;
  return std::strtoll(xml.c_str() + p + open.size(), nullptr, 10);
}

bool adv_buffer(const std::string& xml, const AudioComponentDescription& want, std::string& plist, std::string& why) {
  std::vector<std::pair<size_t, size_t>> devs;
  for (size_t p = 0; (p = xml.find("<AuPluginDevice", p)) != std::string::npos;) {
    const size_t e = xml.find("</AuPluginDevice>", p);
    if (e == std::string::npos) { why = "an <AuPluginDevice> is never closed"; return false; }
    devs.push_back({p, e});
    p = e;
  }
  if (devs.empty()) {
    if (xml.find("<Vst3PluginInfo") != std::string::npos) why = "it holds a VST3 device, not an Audio Unit: save the preset with the plug-in loaded as an AU, or export an .aupreset";
    else if (xml.find("<VstPluginInfo") != std::string::npos) why = "it holds a VST device, not an Audio Unit: save the preset with the plug-in loaded as an AU, or export an .aupreset";
    else why = "it holds no Audio Unit device";
    return false;
  }
  int found = 0;
  size_t at = 0, end = 0;
  for (const auto& [p, e] : devs) {
    if (UInt32(xml_int(xml, p, e, "ComponentType")) == want.componentType && UInt32(xml_int(xml, p, e, "ComponentSubType")) == want.componentSubType &&
        UInt32(xml_int(xml, p, e, "ComponentManufacturer")) == want.componentManufacturer) { ++found; at = p; end = e; }
  }
  if (found == 0) { why = "none of its " + std::to_string(devs.size()) + " Audio Unit device(s) is this component"; return false; }
  if (found > 1) { why = "it holds " + std::to_string(found) + " instances of this component; which one is meant is ambiguous"; return false; }
  const size_t b = xml.find("<Buffer>", at), be = xml.find("</Buffer>", at);
  if (b == std::string::npos || be == std::string::npos || b > end || be > end || be < b) { why = "its Audio Unit device has no state <Buffer>"; return false; }
  std::string hex;
  for (size_t i = b + 8; i < be; ++i) {
    const char c = xml[i];
    if (std::isspace(static_cast<unsigned char>(c))) continue;
    if (!std::isxdigit(static_cast<unsigned char>(c))) { why = "its state <Buffer> is not hex"; return false; }
    hex += c;
  }
  if (hex.empty() || hex.size() % 2) { why = "its state <Buffer> is empty or has an odd number of hex digits"; return false; }
  plist.clear();
  for (size_t i = 0; i < hex.size(); i += 2) plist += char(std::strtol(hex.substr(i, 2).c_str(), nullptr, 16));
  return true;
}

bool load_preset(const std::string& path, const AudioComponentDescription& want, Preset& out, std::string& why) {
  const std::string ext = lower_ext(path);
  if (ext != ".aupreset" && ext != ".adv") { why = path + ": not a supported preset format (.aupreset or a Live .adv)"; return false; }
  std::string bytes, plist;
  if (!read_file(path, bytes, why)) return false;
  if (ext == ".adv") {
    std::string xml;
    if (bytes.size() >= 2 && uint8_t(bytes[0]) == 0x1f && uint8_t(bytes[1]) == 0x8b) { if (!gunzip(bytes, xml, why)) { why = path + ": " + why; return false; } }
    else xml = bytes;   // an uncompressed .adv: Live writes gzip, but the XML is the same
    if (!adv_buffer(xml, want, plist, why)) { why = path + ": " + why; return false; }
  } else plist = bytes;
  if (!plist_dict(plist, want, out.dict, why)) { why = path + ": " + why; return false; }
  out.source = path + " (" + ext.substr(1) + ")";
  return true;
}

// ---- the unit ------------------------------------------------------------------------------------------
struct HostTime { double sr; int64_t pos; };
OSStatus beat_and_tempo(void* user, Float64* beat, Float64* tempo) {   // a fixed 120 BPM transport from the sample count: no wall clock
  const HostTime* h = static_cast<const HostTime*>(user);
  if (beat) *beat = double(h->pos) / h->sr * 2.0;
  if (tempo) *tempo = 120.0;
  return noErr;
}

bool first_party(const AudioComponentDescription& d) {
  return d.componentManufacturer == fourcc("appl") || (d.componentManufacturer == fourcc("LfTk") && d.componentSubType == fourcc("Hsaw"));
}

struct Unit {
  AudioUnit u = nullptr;
  bool initialized = false;
  ~Unit() { if (u) { if (initialized) AudioUnitUninitialize(u); AudioComponentInstanceDispose(u); } }
};

bool apply_preset(AudioUnit u, const Preset& p, std::string& why) {
  if (!p.dict) return true;
  const OSStatus st = AudioUnitSetProperty(u, kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global, 0, &p.dict, sizeof(CFPropertyListRef));
  if (st != noErr) { why = "the plug-in refused the preset (ClassInfo, OSStatus " + std::to_string(int(st)) + ")"; return false; }
  return true;
}

// Instantiates in process, formats float32 non-interleaved stereo at sr, initialises, applies the preset,
// then lets the main run loop drain for 0.2 s (untimed).
bool open_unit(AudioComponent comp, double sr, int block, HostTime* ht, const Preset& preset, Unit& out, std::string& why) {
  OSStatus st = AudioComponentInstanceNew(comp, &out.u);
  if (st != noErr || !out.u) { why = "instantiation failed (OSStatus " + std::to_string(int(st)) + ")"; out.u = nullptr; return false; }
  AudioStreamBasicDescription f{};
  f.mSampleRate = sr; f.mFormatID = kAudioFormatLinearPCM;
  f.mFormatFlags = AudioFormatFlags(kAudioFormatFlagsNativeFloatPacked) | AudioFormatFlags(kAudioFormatFlagIsNonInterleaved);
  f.mBytesPerPacket = 4; f.mFramesPerPacket = 1; f.mBytesPerFrame = 4; f.mChannelsPerFrame = 2; f.mBitsPerChannel = 32;
  if ((st = AudioUnitSetProperty(out.u, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &f, sizeof f)) != noErr) {
    why = "the plug-in refused float32 stereo at " + std::to_string(int(sr)) + " Hz (OSStatus " + std::to_string(int(st)) + ")"; return false;
  }
  UInt32 maxFrames = UInt32(block);
  AudioUnitSetProperty(out.u, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &maxFrames, sizeof maxFrames);
  if (ht) {
    HostCallbackInfo cb{};
    cb.hostUserData = ht; cb.beatAndTempoProc = beat_and_tempo;
    AudioUnitSetProperty(out.u, kAudioUnitProperty_HostCallbacks, kAudioUnitScope_Global, 0, &cb, sizeof cb);   // optional for the AU
  }
  if ((st = AudioUnitInitialize(out.u)) != noErr) { why = "AudioUnitInitialize failed (OSStatus " + std::to_string(int(st)) + ")"; return false; }
  out.initialized = true;
  if (!apply_preset(out.u, preset, why)) return false;
  CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.2, false);
  return true;
}

struct Param { AudioUnitParameterID id; std::string name; float min, max, def; };
std::vector<Param> params(AudioUnit u) {
  std::vector<Param> out;
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
      name = cf_string(info.cfNameString);
      if (info.flags & kAudioUnitParameterFlag_CFNameRelease) CFRelease(info.cfNameString);
    }
    out.push_back({id, name, info.minValue, info.maxValue, info.defaultValue});
  }
  return out;
}

// ---- a cell and one repeat of it --------------------------------------------------------------------------
struct Cell {
  double sr = 44100;
  int block = 128, voices = 8;
  bool skip_render = false;                    // the planted control: time an empty span instead of the render
  std::vector<double> times, worst, process;   // per run: share of a core, worst block share, process CPU share
  std::vector<double> span_wall, span_cpu, cal_before, cal_after;   // per run: the contamination check's readings (ADR-391 A2)
  std::vector<size_t> use;                     // the runs that count: the first 5 clean ones (all of them when not strict)
  double pick(const std::vector<double>& v, bool lo) const {
    double r = NAN;
    for (size_t i : use) if (i < v.size()) r = std::isnan(r) ? v[i] : lo ? std::min(r, v[i]) : std::max(r, v[i]);
    return r;
  }
  double peak = 0, sumsq = 0;
  uint64_t frames = 0;
  bool nonfinite = false;
  std::string error;
  double best() const { return pick(times, true); }
  double spread() const { return pick(times, false) / best(); }
  double worst_block() const { return pick(worst, true); }
  double process_best() const { return pick(process, true); }
  bool clean(size_t i, double fastest) const {
    return std::max(cal_before[i], cal_after[i]) <= kCleanTol * fastest && span_wall[i] <= kCleanTol * span_cpu[i];
  }
  double rms() const { return frames ? std::sqrt(sumsq / double(frames)) : 0; }
  bool silent() const { return !times.empty() && peak < kSilentPeak; }
  bool failed() const { return !error.empty() || nonfinite || silent() || use.empty(); }
};

bool run_once(Cell& c, AudioComponent comp, const Preset& preset) {
  const int B = c.block;
  HostTime ht{c.sr, 0};
  Unit unit;
  if (!open_unit(comp, c.sr, B, &ht, preset, unit, c.error)) return false;
  AudioUnit u = unit.u;
  std::vector<float> l(static_cast<size_t>(B)), r(static_cast<size_t>(B));
  auto* abl = static_cast<AudioBufferList*>(std::calloc(1, offsetof(AudioBufferList, mBuffers) + 2 * sizeof(AudioBuffer)));
  const int warm = int(std::lround(kWarmSeconds * c.sr / B)), timed = int(std::lround(kTimedSeconds * c.sr / B));
  for (int k = 0; k < c.voices; ++k) MusicDeviceMIDIEvent(u, UInt32(0x90 | chord_channel(k)), UInt32(chord_key(k)), kVelocity, 0);   // held before the first block
  double seconds = 0, longest = 0, cpu0 = 0, tcpu0 = 0;
  uint64_t span0 = 0;
  for (int b = 0; b < warm + timed; ++b) {
    std::fill(l.begin(), l.end(), 0.f);
    std::fill(r.begin(), r.end(), 0.f);
    abl->mNumberBuffers = 2;
    abl->mBuffers[0] = {1, UInt32(B * 4), l.data()};
    abl->mBuffers[1] = {1, UInt32(B * 4), r.data()};
    AudioUnitRenderActionFlags flags = 0;
    AudioTimeStamp ts{};
    ts.mSampleTime = Float64(int64_t(b) * B);
    ts.mFlags = kAudioTimeStampSampleTimeValid;
    ht.pos = int64_t(b) * B;
    if (b == warm) { cpu0 = cpu_seconds(); tcpu0 = thread_cpu_seconds(); span0 = mach_absolute_time(); }
    const uint64_t t0 = mach_absolute_time();
    const OSStatus st = c.skip_render ? noErr : AudioUnitRender(u, &flags, &ts, 0, UInt32(B), abl);
    const uint64_t t1 = mach_absolute_time();
    if (st != noErr) { c.error = "AudioUnitRender failed (OSStatus " + std::to_string(int(st)) + ", block " + std::to_string(b) + ")"; std::free(abl); return false; }
    if (b < warm) continue;
    const double took = ticks_to_s(t1 - t0);
    seconds += took;
    longest = std::max(longest, took);
    // The AU may hand back its own buffers (allowed when it renders in place): read through the list.
    for (UInt32 ch = 0; ch < abl->mNumberBuffers && ch < 2; ++ch) {
      const float* p = static_cast<const float*>(abl->mBuffers[ch].mData);
      for (int i = 0; i < B; ++i) {
        const double x = p[i];
        if (!std::isfinite(x)) { c.nonfinite = true; continue; }
        c.peak = std::max(c.peak, std::fabs(x));
        c.sumsq += x * x;
      }
      c.frames += uint64_t(B);
    }
  }
  const uint64_t span1 = mach_absolute_time();
  const double tcpu1 = thread_cpu_seconds(), cpu1 = cpu_seconds();
  std::free(abl);
  const double audio = double(timed) * B / c.sr;
  c.times.push_back(seconds / audio);
  c.worst.push_back(longest / (double(B) / c.sr));
  c.process.push_back((cpu1 - cpu0) / audio);
  c.span_wall.push_back(ticks_to_s(span1 - span0));
  c.span_cpu.push_back(tcpu1 - tcpu0);
  return true;
}

// ADR-391 A2. strict (a measurement): 30 s of the first cell's work, then rounds of every cell, each run bracketed by the
// calibration loop, until every cell has 5 clean runs (true) or 20 rounds pass (false: refuse). Not strict (the controls):
// one round, no warm-up, every run counts. `fastest` is the run's fastest calibration after the warm-up.
bool measure(std::vector<Cell>& cells, AudioComponent comp, const Preset& preset, bool strict, std::vector<double>& calib, double& fastest) {
  pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);   // ask for a performance core; the calibration confirms it
  if (strict && !cells.empty()) {
    const uint64_t w0 = mach_absolute_time();
    while (ticks_to_s(mach_absolute_time() - w0) < kWarmupSeconds) { Cell scratch = cells[0]; scratch.times.clear(); run_once(scratch, comp, preset); }
    std::printf("WARM-UP  %.0f s of the first cell before the first counted run (ADR-391 A1); not counted\n", kWarmupSeconds);
  }
  for (int round = 0; round < (strict ? kMaxRounds : 1); ++round) {
    int rej_cal = 0, rej_pre = 0;
    for (Cell& c : cells) {
      if (!c.error.empty()) continue;
      const double cb = calibration();
      if (!run_once(c, comp, preset)) { std::printf("FAIL  %.0f Hz, %d-sample blocks, %d notes: %s\n", c.sr, c.block, c.voices, c.error.c_str()); continue; }
      const double ca = calibration();
      c.cal_before.push_back(cb); c.cal_after.push_back(ca);
      calib.push_back(cb); calib.push_back(ca);
    }
    fastest = calib.empty() ? NAN : *std::min_element(calib.begin(), calib.end());
    int least = kRepeats;
    for (Cell& c : cells) {
      if (!c.error.empty()) continue;
      c.use.clear();
      for (size_t i = 0; i < c.times.size(); ++i)
        if (!strict || (c.clean(i, fastest) && int(c.use.size()) < kRepeats)) c.use.push_back(i);
      least = std::min(least, int(c.use.size()));
      if (!c.times.empty()) {
        const size_t i = c.times.size() - 1;
        rej_cal += std::max(c.cal_before[i], c.cal_after[i]) > kCleanTol * fastest;
        rej_pre += c.span_wall[i] > kCleanTol * c.span_cpu[i];
      }
    }
    if (!strict) return true;
    const Reading r = read_load();
    std::printf("round %d: fewest clean runs of any cell %d of %d; this round rejected %d for calibration, %d for preemption (load %.2f, busiest other process %.2f cores, %s: not gating)\n",
                round + 1, least, kRepeats, rej_cal, rej_pre, r.load, r.other, r.who.c_str());
    std::fflush(stdout);
    if (least >= kRepeats) return true;
  }
  return false;
}

// The two must-fail controls after a strict measurement (ADR-391 A2): false means the check is blind.
bool contamination_controls(const std::vector<Cell>& cells, AudioComponent comp, const Preset& preset, double fastest, std::string& out) {
  char line[512];
  const double slowed = calibration_beside_spinners();
  Cell pre = cells.empty() ? Cell{} : cells[0];
  pre.times.clear(); pre.span_wall.clear(); pre.span_cpu.clear();
  { Spinners s; run_once(pre, comp, preset); }
  const bool c1 = slowed > kCleanTol * fastest;
  const bool c2 = !pre.span_wall.empty() && pre.span_wall[0] > kCleanTol * pre.span_cpu[0];
  std::snprintf(line, sizeof line, "CONTROL (1) calibration beside %u spinning threads: x%.3f of the fastest: %s\nCONTROL (2) a run beside them: wall/thread CPU %.3f: %s\n", Spinners{}.n,
                slowed / fastest, c1 ? "REJECTED, as it must be" : "PASSED: the calibration test is blind", pre.span_wall.empty() ? NAN : pre.span_wall[0] / pre.span_cpu[0],
                c2 ? "REJECTED, as it must be" : "PASSED: the preemption test is blind");
  out += line;
  return c1 && c2;
}

// ---- finding the component ----------------------------------------------------------------------------------
struct Found { AudioComponent comp = nullptr; AudioComponentDescription desc{}; std::string name; UInt32 version = 0; };

int find_by_name(const std::string& query, Found& out) {   // 0 found; 1 none; 2 ambiguous (printed)
  AudioComponentDescription d{kAudioUnitType_MusicDevice, 0, 0, 0, 0};
  std::vector<Found> hits;
  for (AudioComponent c = nullptr; (c = AudioComponentFindNext(c, &d)) != nullptr;) {
    CFStringRef n = nullptr;
    AudioComponentCopyName(c, &n);
    Found f;
    f.comp = c; f.name = cf_string(n);
    if (n) CFRelease(n);
    AudioComponentGetDescription(c, &f.desc);
    AudioComponentGetVersion(c, &f.version);
    const std::string q = squash(query), full = squash(f.name);
    const auto colon = f.name.find(':');
    const std::string product = squash(colon == std::string::npos ? f.name : f.name.substr(colon + 1));
    if (product == q || full == q) { out = f; return 0; }   // an exact name wins over a substring
    if (full.find(q) != std::string::npos) hits.push_back(f);
  }
  if (hits.empty()) return 1;
  if (hits.size() > 1) {
    std::printf("'%s' matches %zu instruments; name one exactly:\n", query.c_str(), hits.size());
    for (const auto& h : hits) std::printf("  %s\n", h.name.c_str());
    return 2;
  }
  out = hits[0];
  return 0;
}

bool find_desc(UInt32 type, UInt32 sub, UInt32 mfr, Found& out) {
  AudioComponentDescription d{type, sub, mfr, 0, 0};
  out.comp = AudioComponentFindNext(nullptr, &d);
  if (!out.comp) return false;
  CFStringRef n = nullptr;
  AudioComponentCopyName(out.comp, &n);
  out.name = cf_string(n);
  if (n) CFRelease(n);
  AudioComponentGetDescription(out.comp, &out.desc);
  AudioComponentGetVersion(out.comp, &out.version);
  return true;
}

std::string describe(const Found& f) {
  char v[32];
  std::snprintf(v, sizeof v, "v%u.%u.%u", f.version >> 16, (f.version >> 8) & 0xff, f.version & 0xff);
  return f.name + " (" + fourcc_str(f.desc.componentType) + " " + fourcc_str(f.desc.componentSubType) + " " + fourcc_str(f.desc.componentManufacturer) + " " + v + ")";
}

// ---- the report ----------------------------------------------------------------------------------------------
std::string table(const std::vector<Cell>& cells, const std::vector<double>& calib) {
  std::string o;
  char line[512];
  o += "| rate | block | notes | mean % of one core (best of 5) | spread (worst/best) | worst block, % of its deadline | process CPU, all threads (% of one core) | peak dBFS | RMS dBFS | status |\n";
  o += "|---|---|---|---|---|---|---|---|---|---|\n";
  for (const Cell& c : cells) {
    const std::string status = !c.error.empty() ? "FAILED: " + c.error : c.nonfinite ? "FAILED: NaN or inf in the output" : c.silent() ? "FAILED: silent (a failed measurement, not a cheap one)" : "ok";
    std::snprintf(line, sizeof line, "| %.1f kHz | %d | %d | %.2f | %.3f | %.1f | %.2f | %.1f | %.1f | %s |\n", c.sr / 1000, c.block, c.voices, 100 * c.best(), c.spread(),
                  100 * c.worst_block(), 100 * c.process_best(), db(c.peak), db(c.rms()), status.c_str());
    o += line;
  }
  if (!calib.empty()) {
    const double lo = *std::min_element(calib.begin(), calib.end()), hi = *std::max_element(calib.begin(), calib.end());
    std::snprintf(line, sizeof line, "\nCalibration (10^8 dependent multiply-adds, before and after every run, after the warm-up): %.3f to %.3f s over %zu readings, a spread of %.1f %%; only runs within 2 %% of the fastest count (ADR-391 A2).\n", lo, hi,
                  calib.size(), 100 * (hi / lo - 1));
    o += line;
  }
  // What the plug-in's polyphony allowed: each larger cell against the 8-note cell at the same rate and block.
  std::string poly;
  for (const Cell& c : cells) {
    if (c.voices == 8 || c.failed()) continue;
    for (const Cell& b : cells)
      if (b.voices == 8 && b.sr == c.sr && b.block == c.block && !b.failed()) {
        std::snprintf(line, sizeof line, "- %.1f kHz, %d: %d notes cost %.2f x the 8-note cell (%.2f x if every note were voiced at the same cost), RMS %+.1f dB.\n", c.sr / 1000, c.block, c.voices,
                      c.best() / b.best(), double(c.voices) / 8, db(c.rms()) - db(b.rms()));
        poly += line;
      }
  }
  if (!poly.empty()) o += "\nPolyphony, as the host can see it (it cannot count the plug-in's voices; a cost ratio near 1 means the extra notes were not voiced):\n" + poly;
  return o;
}

void usage() {
  std::puts(
      "au_bench — measure an installed AU instrument under PF-3's conditions (B638)\n"
      "  au_bench --list                                     list the instruments (aumu); loads no plug-in\n"
      "  au_bench --name <name> (--preset <file> | --default) [options]\n"
      "        --preset <file>      .aupreset, or a Live .adv device preset holding the AU\n"
      "        --default            measure the plug-in's default patch (no preset); must be said explicitly\n"
      "        --sr 44100,48000     sample rates      --block 128,512   block sizes\n"
      "        --voices 8,16,64     note counts (PF-1's 8; PF-9's 16 and 64; 0 is the no-notes control)\n"
      "        --first-channel 1    the MIDI channel of notes 1 to 8 (a diagnostic: does the plug-in hear other channels?)\n"
      "        --report <file.md>   also write the table there (keep third-party results outside the repo)\n"
      "        --allow-third-party  required for anything not Apple's or horde's (B381's latch)\n"
      "  au_bench --controls                                 the must-fail controls on Apple's DLSMusicDevice\n"
      "exit: 0 measured; 1 refused (component, preset, arguments); 2 too few clean runs after 20 rounds (ADR-391 A2); 3 a cell failed or was silent; 4 a contamination control was blind");
}

std::vector<double> parse_list(const char* s) {
  std::vector<double> v;
  for (const char* p = s; *p;) {
    char* e = nullptr;
    const double x = std::strtod(p, &e);
    if (e == p) return {};
    v.push_back(x);
    p = *e == ',' ? e + 1 : e;
    if (*e && *e != ',') return {};
  }
  return v;
}

int cmd_list() {
  AudioComponentDescription d{kAudioUnitType_MusicDevice, 0, 0, 0, 0};
  int n = 0;
  for (AudioComponent c = nullptr; (c = AudioComponentFindNext(c, &d)) != nullptr; ++n) {
    Found f;
    f.comp = c;
    CFStringRef nm = nullptr;
    AudioComponentCopyName(c, &nm);
    f.name = cf_string(nm);
    if (nm) CFRelease(nm);
    AudioComponentGetDescription(c, &f.desc);
    AudioComponentGetVersion(c, &f.version);
    std::printf("%s\n", describe(f).c_str());
  }
  std::printf("%d instrument(s)\n", n);
  return 0;
}

// The probe instance: the unit's own report (latency, tail, polyphony and quality parameters), and the
// preset applied once before anything is timed, so a preset the plug-in refuses stops the run here.
bool probe(const Found& f, double sr, int block, const Preset& preset, std::string& out) {
  Unit unit;
  std::string why;
  if (!open_unit(f.comp, sr, block, nullptr, preset, unit, why)) { out = "REFUSED  " + why + "\n"; return false; }
  char line[512];
  Float64 latency = 0, tail = 0;
  UInt32 sz = sizeof latency;
  const bool hasLat = AudioUnitGetProperty(unit.u, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &latency, &sz) == noErr;
  sz = sizeof tail;
  const bool hasTail = AudioUnitGetProperty(unit.u, kAudioUnitProperty_TailTime, kAudioUnitScope_Global, 0, &tail, &sz) == noErr;
  std::snprintf(line, sizeof line, "reported latency %s, tail %s\n", hasLat ? (std::to_string(latency) + " s (" + std::to_string(int(std::lround(latency * sr))) + " samples at " + std::to_string(int(sr)) + " Hz)").c_str() : "not reported",
                hasTail ? (std::to_string(tail) + " s").c_str() : "not reported");
  out += line;
  const auto ps = params(unit.u);
  int shown = 0;
  for (const Param& p : ps) {
    const std::string n = squash(p.name);
    if (n.find("poly") == std::string::npos && n.find("voice") == std::string::npos && n.find("oversampl") == std::string::npos && n.find("quality") == std::string::npos) continue;
    AudioUnitParameterValue v = 0;
    AudioUnitGetParameter(unit.u, p.id, kAudioUnitScope_Global, 0, &v);
    std::snprintf(line, sizeof line, "  parameter %u '%s' = %g (range %g to %g)\n", p.id, p.name.c_str(), double(v), double(p.min), double(p.max));
    out += line;
    ++shown;
  }
  std::snprintf(line, sizeof line, "%zu global parameters; %d name polyphony, voices, oversampling or quality%s\n", ps.size(), shown,
                shown ? " (listed above)" : ": the plug-in exposes no such setting as a parameter (it may live in the patch)");
  out += line;
  return true;
}

// ---- the must-fail controls ------------------------------------------------------------------------------------
int controls() {
  int failures = 0;
  auto verdict = [&](bool ok, const std::string& what) { std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what.c_str()); failures += !ok; };
  print_reading("machine load", read_load());

  Found dls;
  if (!find_desc(kAudioUnitType_MusicDevice, fourcc("dls "), fourcc("appl"), dls)) { verdict(false, "Apple's DLSMusicDevice is not installed: no stand-in"); return 1; }
  const Preset none;

  // C2 / C3: the stand-in, and the planted skip.
  std::vector<Cell> cells(2);
  cells[1].skip_render = true;
  std::vector<double> calib;
  double fastest = NAN;
  measure(cells, dls.comp, none, false, calib, fastest);
  std::fputs(table(cells, calib).c_str(), stdout);
  // C1 (ADR-391 A2): the contamination check's own plants, on the stand-in: a slowed calibration and a preempted run.
  {
    std::string ctl;
    const bool fired = contamination_controls(cells, dls.comp, none, fastest, ctl);
    std::fputs(ctl.c_str(), stdout);
    verdict(fired, "C1 the contamination check rejects a calibration and a run beside twice as many spinning threads as cores");
  }
  const Cell &real = cells[0], &skip = cells[1];
  verdict(real.error.empty() && std::isfinite(real.best()) && real.best() > 1e-5 && real.best() < 1.0 && !real.silent() && !real.nonfinite,
          "C2 the stand-in (DLSMusicDevice, 8 notes, 44.1 kHz, 128): " + std::to_string(100 * real.best()) + " % of a core, finite and plausible, not silent");
  verdict(skip.silent() && skip.best() < real.best() / 10,
          "C3 the planted skip-the-render: " + std::to_string(100 * skip.best()) + " % of a core (under a tenth of the stand-in) and flagged silent");

  // C4 to C9: presets.
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / ("au_bench_controls_" + std::to_string(getpid()));
  fs::create_directories(dir);
  auto write = [&](const char* name, const std::string& bytes) {
    const fs::path p = dir / name;
    std::ofstream(p, std::ios::binary) << bytes;
    return p.string();
  };
  Preset p;
  std::string why;
  verdict(!load_preset((dir / "missing.aupreset").string(), dls.desc, p, why), "C4 a missing preset file is refused: " + why);
  verdict(!load_preset(write("garbage.aupreset", "this is not a property list\x01\x02"), dls.desc, p, why), "C5 a garbage .aupreset is refused: " + why);
  verdict(!load_preset(write("patch.SerumPreset", "x"), dls.desc, p, why), "C5b an unsupported format is refused: " + why);

  // The stand-in's own state, with one parameter moved off its default, so a round trip is visible.
  std::string state;
  AudioUnitParameterID moved = 0;
  float target = 0;
  bool haveMoved = false;
  {
    Unit unit;
    if (!open_unit(dls.comp, 44100, 128, nullptr, none, unit, why)) { verdict(false, "the stand-in would not open: " + why); return 1; }
    for (const Param& q : params(unit.u))
      if (q.max > q.min) {
        target = q.min + 0.37f * (q.max - q.min);
        if (std::fabs(target - q.def) < 1e-3f * (q.max - q.min)) target = q.min + 0.61f * (q.max - q.min);
        moved = q.id;
        haveMoved = AudioUnitSetParameter(unit.u, q.id, kAudioUnitScope_Global, 0, target, 0) == noErr;
        break;
      }
    CFPropertyListRef ci = nullptr;
    UInt32 sz = sizeof ci;
    if (AudioUnitGetProperty(unit.u, kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global, 0, &ci, &sz) != noErr || !ci) { verdict(false, "the stand-in gave no ClassInfo"); return 1; }
    CFDataRef xml = CFPropertyListCreateData(nullptr, ci, kCFPropertyListXMLFormat_v1_0, 0, nullptr);
    state.assign(reinterpret_cast<const char*>(CFDataGetBytePtr(xml)), size_t(CFDataGetLength(xml)));
    CFRelease(xml);
    CFRelease(ci);
  }
  // C6: the same state claiming another component.
  {
    std::string other = state;
    const std::string key = "<key>subtype</key>";
    const size_t k = other.find(key);
    const size_t v0 = other.find("<integer>", k), v1 = other.find("</integer>", k);
    if (k != std::string::npos && v0 != std::string::npos && v1 != std::string::npos) other.replace(v0 + 9, v1 - v0 - 9, std::to_string(fourcc("xxxx")));
    verdict(k != std::string::npos && !load_preset(write("other.aupreset", other), dls.desc, p, why), "C6 a preset for another component is refused: " + why);
  }
  auto adv = [&](const std::string& plist, const char* deviceXml) {
    std::string hex;
    static const char* digits = "0123456789ABCDEF";
    for (size_t i = 0; i < plist.size(); ++i) {
      if (i % 40 == 0) hex += "\n\t\t\t\t";
      hex += digits[uint8_t(plist[i]) >> 4];
      hex += digits[uint8_t(plist[i]) & 15];
    }
    char head[512];
    std::snprintf(head, sizeof head, deviceXml, dls.desc.componentType, dls.desc.componentSubType, dls.desc.componentManufacturer);
    std::string xml = std::string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<Ableton MajorVersion=\"5\">\n") + head + hex +
                      "\n\t\t\t</Buffer></AuPreset></Preset></AuPluginInfo></PluginDesc>\n\t</AuPluginDevice>\n</Ableton>\n";
    std::string gz;
    gzip(xml, gz);
    return gz;
  };
  const char* auDevice =
      "\t<AuPluginDevice Id=\"0\">\n\t\t<PluginDesc><AuPluginInfo Id=\"0\">\n\t\t\t<ComponentType Value=\"%u\" />\n\t\t\t<ComponentSubType Value=\"%u\" />\n"
      "\t\t\t<ComponentManufacturer Value=\"%u\" />\n\t\t\t<Preset><AuPreset Id=\"0\"><Buffer>";
  // C7: the round trip through a gzipped .adv.
  {
    Preset rt;
    const bool loaded = load_preset(write("standin.adv", adv(state, auDevice)), dls.desc, rt, why);
    bool back = false;
    std::string detail = loaded ? "" : why;
    if (loaded) {
      Unit unit;
      if (open_unit(dls.comp, 44100, 128, nullptr, rt, unit, why)) {
        AudioUnitParameterValue v = -1;
        AudioUnitGetParameter(unit.u, moved, kAudioUnitScope_Global, 0, &v);
        back = !haveMoved || std::fabs(v - target) < 1e-4f;
        detail = haveMoved ? "parameter " + std::to_string(moved) + " set to " + std::to_string(target) + " before saving reads " + std::to_string(v) + " after loading" : "loaded (the stand-in has no movable parameter to check)";
      } else detail = why;
      CFRelease(rt.dict);
    }
    // The must-read-different half: an instance given no preset reads its default, so C7's match came from the file.
    if (haveMoved) {
      Unit plain;
      AudioUnitParameterValue v = target;
      if (open_unit(dls.comp, 44100, 128, nullptr, none, plain, why)) AudioUnitGetParameter(plain.u, moved, kAudioUnitScope_Global, 0, &v);
      back = back && std::fabs(v - target) >= 1e-4f;
      detail += "; with no preset it reads " + std::to_string(v);
    }
    verdict(loaded && back, "C7 an .adv carrying the stand-in's state round-trips: " + detail);
  }
  // C8: an .adv holding a VST3 device. C9: a corrupt buffer.
  {
    std::string xml = "<?xml version=\"1.0\"?>\n<Ableton><PluginDevice Id=\"0\"><PluginDesc><Vst3PluginInfo Id=\"0\"><Name Value=\"x\" /></Vst3PluginInfo></PluginDesc></PluginDevice></Ableton>\n", gz;
    gzip(xml, gz);
    verdict(!load_preset(write("vst3.adv", gz), dls.desc, p, why), "C8 an .adv holding a VST3 device is refused: " + why);
    std::string bad = adv(state, auDevice), raw;
    gunzip(bad, raw, why);
    const size_t b = raw.find("<Buffer>");
    raw[b + 20] = 'Z';
    gz.clear();
    gzip(raw, gz);
    verdict(!load_preset(write("corrupt.adv", gz), dls.desc, p, why), "C9 an .adv whose buffer is corrupt is refused: " + why);
  }
  fs::remove_all(dir);
  std::printf("%s  %d control(s) failed\n", failures ? "CONTROLS FAILED" : "CONTROLS PASSED", failures);
  return failures ? 3 : 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (!kRelease) { std::puts("REFUSED  PF-3 measures a Release build only; this is not one (NDEBUG unset)"); return 1; }
  if (argc < 2) { usage(); return 1; }
  std::string name, presetPath, reportPath;
  bool useDefault = false, thirdParty = false;
  std::vector<double> rates{44100, 48000}, blocks{128, 512}, voices{8, 16, 64};
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&](const char* what) -> const char* {
      if (i + 1 >= argc) { std::printf("REFUSED  %s needs a value\n", what); std::exit(1); }
      return argv[++i];
    };
    if (a == "--list") return cmd_list();
    if (a == "--controls") return controls();
    if (a == "--name") name = next("--name");
    else if (a == "--preset") presetPath = next("--preset");
    else if (a == "--default") useDefault = true;
    else if (a == "--report") reportPath = next("--report");
    else if (a == "--allow-third-party") thirdParty = true;
    else if (a == "--sr") rates = parse_list(next("--sr"));
    else if (a == "--block") blocks = parse_list(next("--block"));
    else if (a == "--voices") voices = parse_list(next("--voices"));
    else if (a == "--first-channel") {
      g_first_channel = std::atoi(next("--first-channel")) - 1;
      if (g_first_channel < 0 || g_first_channel > 15) { std::puts("REFUSED  --first-channel: 1 to 16"); return 1; }
    }
    else { std::printf("REFUSED  unknown argument '%s'\n", a.c_str()); usage(); return 1; }
  }
  if (name.empty()) { std::puts("REFUSED  --name is required (see --list)"); return 1; }
  if (presetPath.empty() == !useDefault) { std::puts("REFUSED  give exactly one of --preset <file> and --default: a missing preset is never rendered at defaults"); return 1; }
  if (rates.empty() || blocks.empty() || voices.empty()) { std::puts("REFUSED  --sr, --block and --voices take comma-separated numbers"); return 1; }
  for (double v : voices) if (v < 0 || v > 128) { std::puts("REFUSED  --voices: 0 to 128 notes (0: the no-notes control)"); return 1; }
  for (double b : blocks) if (b < 16 || b > 4096) { std::puts("REFUSED  --block: 16 to 4096 samples"); return 1; }

  Found f;
  const int fr = find_by_name(name, f);
  if (fr == 1) { std::printf("REFUSED  no instrument (aumu) named like '%s' (see --list)\n", name.c_str()); return 1; }
  if (fr == 2) return 1;
  if (!first_party(f.desc) && !thirdParty) {
    std::printf("REFUSED  %s is not Apple's or horde's, and --allow-third-party was not given (B381: a third-party load is a human gate)\n", describe(f).c_str());
    return 1;
  }
  Preset preset;
  std::string why;
  if (!presetPath.empty() && !load_preset(presetPath, f.desc, preset, why)) { std::printf("REFUSED  the preset: %s\n", why.c_str()); return 1; }

  std::string out = "# AU bench (B638): " + describe(f) + "\n\n";
  out += "Instantiated in process (AudioComponentInstanceNew). Preset: " + (preset.dict ? preset.source : std::string("none: the plug-in's DEFAULT PATCH (--default), not a reference")) + ".\n";
  out += "PF-3's protocol: the k-th note on key 48 + 3 (k mod 8), velocity 108/127 (0.85), on at sample 0 and held; notes 9 and up on MIDI channels 2 and up; "
         "0.25 s untimed, then 2 s timed; 30 s warm-up, then the first 5 clean runs of each cell (ADR-391 A2: calibration within 2 % of the fastest, wall within 2 % of thread CPU), interleaved, a fresh instance each.\n\n";
  std::string report;
  if (!probe(f, rates[0], int(blocks[0]), preset, report)) { std::fputs((out + report).c_str(), stdout); return 1; }
  out += report + "\n";
  std::fputs(out.c_str(), stdout);
  std::fflush(stdout);

  std::vector<Cell> cells;
  for (double sr : rates)
    for (double b : blocks)
      for (double v : voices) {
        Cell c;
        c.sr = sr; c.block = int(b); c.voices = int(v);
        cells.push_back(c);
      }
  std::vector<double> calib;
  double fastest = NAN;
  if (!measure(cells, f.comp, preset, true, calib, fastest)) {
    std::printf("REFUSED  fewer than %d clean runs of some cell after %d rounds (ADR-391 A2); nothing is reported\n", kRepeats, kMaxRounds);
    if (preset.dict) CFRelease(preset.dict);
    return 2;
  }
  std::string ctl;
  const bool fired = contamination_controls(cells, f.comp, preset, fastest, ctl);
  std::fputs(ctl.c_str(), stdout);
  if (!fired) {
    std::puts("REFUSED  a contamination control did not fire (ADR-391 A2); nothing is reported");
    if (preset.dict) CFRelease(preset.dict);
    return 4;
  }
  out += ctl;
  const std::string t = table(cells, calib);
  std::fputs(("\n" + t).c_str(), stdout);
  if (!reportPath.empty()) {
    std::ofstream r(reportPath);
    if (!r) { std::printf("FAIL  cannot write %s\n", reportPath.c_str()); return 1; }
    r << out << t;
    std::printf("report written to %s\n", reportPath.c_str());
  }
  if (preset.dict) CFRelease(preset.dict);
  bool anyFailed = false;
  for (const Cell& c : cells) anyFailed |= c.failed();
  return anyFailed ? 3 : 0;
}
