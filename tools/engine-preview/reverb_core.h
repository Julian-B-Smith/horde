#pragma once
// The test-only post-engine reverb's pure half (B710, ADR-408): host parameter ids and the saved state's lines. No Scape
// here, so the smoke test (which never links Scape) reads the same ids and counts the plug-in plays. The DSP, and every
// range, default and unit (which are Scape's, read from its own table, never copied here), are in post_reverb.h.
//
// Not the engine's and not in its manifest: Scape's reverb sits after the engine's stereo output (after B660's filter)
// so the human can hear the first FX module in Live now, ahead of the rack (RC-5, B398). Off by default, and off it
// touches nothing, so every preset, bit-identity test and recorded frame is what it was.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace preview::reverb {

// The next free block after B660's filter (0xA00010 to 0xA00015): "Scape on", then every Scape parameter in Scape's own
// order. Scape's parameter index is its stable id (append-only: its manifest's "id"), so host id = kFirstParamId + that
// id, and a new Scape parameter would only ever add ids at the end. Above every engine id (IF-11 keeps them below
// 0xB00000) and clear of Preset's 0xB00001; never reused.
constexpr std::uint32_t kOnId = 0xA00016;
constexpr std::uint32_t kFirstParamId = kOnId + 1;
// Scape's parameter count at scape.pin. post_reverb.h holds this to Scape's own scape::kParamCount at compile time, so a
// pin move that adds a parameter stops the build until this block is re-laid (a re-test, as the pin's move is).
constexpr int kScapeParams = 29;
constexpr int kHostParams = 1 + kScapeParams;
constexpr std::uint32_t id_of(int k) { return kFirstParamId + std::uint32_t(k); }   // k: Scape's parameter id
constexpr bool is_reverb_id(std::uint32_t id) { return id >= kOnId && id < kOnId + std::uint32_t(kHostParams); }
constexpr int param_of(std::uint32_t id) { return int(id - kFirstParamId); }   // -1 is "Scape on"

// What the chunk saves: whether it is on, and each Scape parameter that differs from Scape's default, by Scape's stable
// id. Only differences, so an untouched reverb saves nothing at all, and the defaults stay Scape's (never written here).
struct State {
  bool on = false;
  std::vector<std::pair<int, double>> values;   // (Scape id, value in Scape's own units)
  bool operator==(const State&) const = default;
  bool is_default() const { return !on && values.empty(); }
};

// Lines "scape on 1" and "scape <id> <value>", written only when something differs from the defaults.
inline void write_lines(const State& s, std::string& text) {
  if (s.on) text += "scape on 1\n";
  for (const auto& [k, v] : s.values) {
    char line[64];
    std::snprintf(line, sizeof line, "scape %d %.17g\n", k, v);
    text += line;
  }
}
// `line` is one line without its newline. False when it isn't a scape line. A scape line naming an id outside Scape's
// range or carrying a non-finite value is read as no line at all (true, the state untouched); a later line for the same
// id replaces the earlier one.
inline bool read_line(std::string_view line, State& s) {
  if (line.substr(0, 6) != "scape ") return false;
  const std::string copy(line);
  int k = -1, on = 0;
  double v = 0;
  if (std::sscanf(copy.c_str(), "scape on %d", &on) == 1) { s.on = on != 0; return true; }
  if (std::sscanf(copy.c_str(), "scape %d %lf", &k, &v) != 2 || k < 0 || k >= kScapeParams || !std::isfinite(v)) return true;
  for (auto& [id, value] : s.values)
    if (id == k) { value = v; return true; }
  s.values.emplace_back(k, v);
  return true;
}

}  // namespace preview::reverb
