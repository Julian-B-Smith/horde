#pragma once
// The morph system's pure half for the test plug-in (B636): D.2's settings as the page sends them, MO-11's per-key
// choices, and the morph's lines in the saved chunk. No plug-in, no threads, so bridge_test runs it headless. The
// resolver itself is the clean room's (IF-12, make_morph_resolver); preview_plugin.cpp hosts it on the audio thread.
//
// Three things a reader would otherwise re-derive:
// - D.2 has no lockfile yet. IF-11 says the morph's host parameters come from a morph manifest appended to the
//   lockfile; none is committed, so nothing here is a host parameter and the rows below are transcribed from
//   SPEC-ENGINE D.2 (ranges, tapers, steps, defaults). They reach the resolver only through the page's `morph` message.
//   When that lockfile lands, these rows come from it and the ones it marks become host parameters.
// - The position is not in the table below. Round 61 makes `morph.x` and `morph.y` D.2 rows and host parameters
//   (IF-11); the plug-in exposes them as its own two host parameters (preview_plugin.cpp, kMorphXId and kMorphYId), and
//   every position, the host's or the page's `position` message, goes through one function there (set_position).
// - Generic on purpose. Nothing here or in the plug-in knows which events the resolver emits (Param, Ratios, SettingSet
//   today; round 61's levels later): the plug-in forwards whatever comes out, so the 1.0 resolver is a rebuild.
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "horde_engine.h"

namespace preview::morph {

// ---- D.2's settings ---------------------------------------------------------------------------------------------
// In the order below. Values cross the page in v, as every row does (IF-11): a Step row in steps of 1/(n - 1).
enum Setting : int {
  kOn, kAudible, kLaw, kCount, kTemperature, kCohesion, kSeed, kStepRate, kStepSync, kStepDivision, kStepTriplet,
  kStepDotted, kGlide, kSettings
};
// The row: key, law, lo, hi, steps and default in ParamInfo's own conventions (a Step row's default is its option
// index; a Step row of consecutive integers gives its first and last numbers, so the seed's lo is 1).
const horde::ParamInfo& setting_row(int i);
int find_setting(std::string_view key);   // -1 when D.2 has no such key
double default_v(int i);                  // the row's default as v
// v per setting -> the resolver's struct, in the rows' units (horde::value_of's law).
horde::MorphSettings decode(const double v[kSettings]);

// ---- MO-11's per-key choice ---------------------------------------------------------------------------------------
// The page's names: "follow" (or the key absent: Follow), "quantum", "exempt"; a count of 2 or 4 (absent or 0: As the
// morph).
struct KeyChoice {
  horde::KeyMode mode = horde::KeyMode::Follow;
  horde::KeyCount count = horde::KeyCount::AsMorph;
  bool operator==(const KeyChoice& o) const { return mode == o.mode && count == o.count; }
  bool operator!=(const KeyChoice& o) const { return !(*this == o); }
};
bool mode_of(std::string_view name, horde::KeyMode& out);   // false: not a mode the page names
const char* mode_name(horde::KeyMode m);
int count_number(horde::KeyCount c);                         // 0 (As the morph), 2 or 4

// ---- the morph's lines in the saved chunk -------------------------------------------------------------------------
// Everything is stored as a difference from what loads without it, so a patch that never touched the morph saves no
// morph line at all and keeps writing the version-2 chunk unchanged:
// - settings that differ from D.2's defaults, by key;
// - the position when it isn't (0, 0), D.2's default (ADR-373 item 9: a fresh patch sits on corner A);
// - each corner's rows that differ from the preset's own values, by id. While the morph is off, the audible corner IS
//   the plug-in's current sound (D.2: it is the corner that sounds), whose rows the chunk already holds, so that one
//   corner is not written: a load rebuilds it from the rows;
// - each corner's ratios text that differs from the preset's;
// - each key's mode and count that isn't Follow and As the morph;
// - each key's waypoints (MO-21, in stored order), the waypoint law and its clamp when not TPS with Clamp on, and each
//   key's region lock (MO-24), B639.
struct State {
  double x = 0, y = 0;
  std::vector<std::pair<int, double>> settings;   // D.2 index, v
  struct CornerValue { int corner; std::uint32_t id; double v; };
  std::vector<CornerValue> corners;
  struct CornerText { int corner, osc; std::string text; };
  std::vector<CornerText> texts;
  struct Mode { std::uint32_t id; KeyChoice choice; };   // id: a manifest id or horde::kRatiosKeyId
  std::vector<Mode> modes;
  struct Pins { std::uint32_t id; std::vector<horde::Waypoint> points; };   // MO-21: one key's list, in stored order
  std::vector<Pins> waypoints;
  horde::WaypointLaw law = horde::WaypointLaw::Tps;   // MO-21's defaults: TPS·Δ with Clamp to set values on
  bool clamp = true;
  struct Lock { std::uint32_t id; double offsets[3]; };   // MO-24: δ[B], δ[C], δ[D]
  std::vector<Lock> locks;
  bool empty() const {
    return x == 0 && y == 0 && settings.empty() && corners.empty() && texts.empty() && modes.empty() && waypoints.empty() &&
           law == horde::WaypointLaw::Tps && clamp && locks.empty();
  }
};
// Appends the morph's lines (each starting "morph ") to a chunk's text; reads one line back. `line` is one line without
// its newline; false when it isn't a morph line (the caller reads it as its own).
void write_lines(const State& s, std::string& text);
bool read_line(std::string_view line, State& s);

}  // namespace preview::morph
