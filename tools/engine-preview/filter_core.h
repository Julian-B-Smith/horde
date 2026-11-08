#pragma once
// The test-only post-engine filter's pure half (B660): host parameter ids, defaults, the value laws and the saved state's
// line. No Weir here, so the bridge test and the smoke test (which never link Weir) can read the same numbers the
// plug-in plays. The DSP is post_filter.h.
//
// Not the engine's and not in its manifest: the filter sits after the engine's output so the human can hear Weir's
// SVF 12 in Live now, ahead of the engine growing a filter slot. Off by default, and off it touches nothing, so every
// preset, bit-identity test and recorded frame is what it was.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

namespace preview::filter {

// The plug-in's own block beside kPresetId (0xB00001) and the morph's 0xA00001/2: above every engine id (IF-11 keeps
// its ids below 0xB00000), clear of the morph's pair, and never reused.
constexpr std::uint32_t kIdBase = 0xA00010;
enum Param : int { kOn = 0, kMode, kCutoff, kResonance, kRing, kMix, kParams };
constexpr std::uint32_t id_of(int p) { return kIdBase + std::uint32_t(p); }
constexpr bool is_filter_id(std::uint32_t id) { return id >= kIdBase && id < kIdBase + kParams; }
constexpr int param_of(std::uint32_t id) { return int(id - kIdBase); }

constexpr int kModeCount = 5;   // LP, BP, HP, Notch, Peak: Weir's modes 1 to 5 (0 is bypass, which Filter on covers)
inline const char* mode_name(int mode) {
  static const char* const names[kModeCount] = {"LP", "BP", "HP", "Notch", "Peak"};
  return names[std::clamp(mode, 0, kModeCount - 1)];
}

// The cutoff host value is a position x in 0..1 on a log law, because CLAP has no log flag and Live's slider is
// linear in the value: 20 Hz at 0, 20 kHz at 1 (1000x, so x = log(f / 20) / log(1000)).
constexpr double kCutoffLoHz = 20.0, kCutoffRatio = 1000.0;
inline double cutoff_hz(double x) { return kCutoffLoHz * std::pow(kCutoffRatio, std::clamp(x, 0.0, 1.0)); }

// Host values in the host's own form: on 0 or 1, mode 0 to 4, the rest 0 to 1 (the cutoff's a position).
struct State {
  double v[kParams] = {0.0, 0.0, 0.56628, 0.0, 0.0, 1.0};   // 0.56628 is 1 kHz; mix 1 is fully filtered
  bool operator==(const State&) const = default;
  bool is_default() const { return *this == State{}; }
};
inline double default_value(int p) { return State{}.v[p]; }
inline double max_value(int p) { return p == kOn ? 1.0 : p == kMode ? double(kModeCount - 1) : 1.0; }
inline bool stepped(int p) { return p == kOn || p == kMode; }
inline double clamped(int p, double v) {
  v = std::clamp(v, 0.0, max_value(p));
  return stepped(p) ? std::round(v) : v;
}
inline const char* param_name(int p) {
  static const char* const names[kParams] = {"Filter on", "Filter mode", "Filter cutoff", "Filter resonance", "Filter ring", "Filter mix"};
  return names[p];
}

// The saved chunk's one line, "filter <on> <mode> <cutoff x> <res> <ring> <mix>", written only when something differs
// from the defaults, so an untouched patch saves what it always did.
inline void write_line(const State& s, std::string& text) {
  if (s.is_default()) return;
  char line[160];
  std::snprintf(line, sizeof line, "filter %.17g %.17g %.17g %.17g %.17g %.17g\n", s.v[0], s.v[1], s.v[2], s.v[3], s.v[4], s.v[5]);
  text += line;
}
// `line` is one line without its newline. False when it isn't a filter line; a filter line with a non-finite value is
// read as no filter line at all (true, the state untouched).
inline bool read_line(std::string_view line, State& s) {
  if (line.substr(0, 7) != "filter ") return false;
  const std::string copy(line);
  double v[kParams];
  if (std::sscanf(copy.c_str(), "filter %lf %lf %lf %lf %lf %lf", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != kParams) return true;
  for (double x : v)
    if (!std::isfinite(x)) return true;
  for (int p = 0; p < kParams; ++p) s.v[p] = clamped(p, v[p]);
  return true;
}

}  // namespace preview::filter
