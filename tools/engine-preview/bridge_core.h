#pragma once
// GUI 3's bridge to the clean-room engine, its pure half (B628; the seam is signed in docs/proposals/gui-on-engine.md
// section 3). No AppKit, no web view and no plug-in here, so bridge_test runs it headless: page messages in (JSON
// text), what to apply out, and snapshots out as JSON text. Everything here runs on the main thread, never on audio:
// it parses and allocates freely.
//
// Two things a reader would otherwise re-derive:
// - Keys, never ids. The page names rows by key; KeyMap turns a key into the engine's manifest row. A key the manifest
//   lacks, or a reserved row's, is reported back and never becomes an event (section 3, rule 1).
// - The engine's manifest has no target column: every row in it is the engine's. So "the engine's rows" here are the
//   manifest's non-reserved rows, and anything the page owns elsewhere (rack, midi, resolver) is a key the manifest
//   lacks, which is exactly how rule 2 keeps those off the engine.
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "filter_core.h"
#include "horde_engine.h"
#include "morph_core.h"

namespace preview::bridge {

// ---- the manifest hash (section 3) ------------------------------------------------------------------------------
// One row's line: key, class, law, lo, hi, steps, default, tab-joined, ending in a newline; numbers as C's %.17g.
// class: morphable | structural | device. law: lin | log | pow | linwraps | logoffzone | db | step.
std::string manifest_line(const horde::ParamInfo& p);
// The non-reserved rows' lines, sorted by key bytewise, concatenated: the text both sides hash.
std::string manifest_text(const std::vector<const horde::ParamInfo*>& rows);
std::string sha256_hex(std::string_view bytes);   // lowercase hex
std::string manifest_hash(const std::vector<const horde::ParamInfo*>& rows);

// The engine's rows by key (every manifest row, reserved ones included, so a reserved key reads as refused rather
// than unknown).
class KeyMap {
 public:
  explicit KeyMap(std::vector<const horde::ParamInfo*> rows);
  // The row's index in the list given, or -1.
  int find(std::string_view key) const;
  const horde::ParamInfo& row(int index) const { return *rows_[std::size_t(index)]; }
  int size() const { return int(rows_.size()); }
  const std::vector<const horde::ParamInfo*>& rows() const { return rows_; }

 private:
  std::vector<const horde::ParamInfo*> rows_;
  std::vector<int> by_key_;   // indices into rows_, sorted by key, for a binary search
};

// ---- messages from the page -------------------------------------------------------------------------------------
struct Want {
  bool osc[2] = {true, true};   // which oscillators' parts the page draws (want.osc names them 1 and 2)
  bool members = false;         // members' phases and levels
  bool cycles = false;          // GN-41's member cycles: the engine has none yet, so this slice never sends them
  bool summed = false;          // GN-4's summed output, the plug-in's own tap (B632): observe.summed
  bool morph = false;           // the morph system's state and what it resolved (B636): the snapshot's `morph`
};

enum class Kind {
  Set,       // engine values: `values` holds the rows to send
  Gesture,   // RC-11.3's begin or end
  Want,      // the parts of RC-8 the page draws
  Preset,    // the bank preset to load
  Plugin,    // the test plug-in's own Voices setting
  Hello,     // the page has loaded: its manifest hash, for the mismatch banner
  Morph,     // D.2's settings, by key (B636): `settings`
  Position,  // the pad's position: `x`, `y`
  Modes,     // MO-11's mode and count of every key, whole: `modes` (a key it doesn't name is Follow, As the morph)
  Corner,    // values written into one corner (the bridge's seam for a corner edit): `index`, `values`
  Save,      // MO-25's Save current sound into corner `index`
  Waypoints, // MO-21: one key's whole list of waypoints, `key` and `points` (B639)
  WaypointLaw,   // MO-21: the patch's waypoint law and its clamp, `law` and `clamp`
  Lock,      // MO-24: one key's region lock, `key` and `offsets`, or none (`has_offsets` false)
  Held,      // rack, midi, clear, or a type nobody knows: acknowledged and dropped
  Invalid,   // unreadable, or no numeric seq: nothing to acknowledge
};
// A `modes` entry's key: a KeyMap index, or one of these for the two `oscN.ratios` texts (MO-14 item 3).
constexpr int kRatiosKey[2] = {-2, -3};

struct Message {
  Kind kind = Kind::Invalid;
  std::string type;                    // as the page named it
  std::int64_t seq = 0;
  std::string error;                   // Invalid only
  struct Value { int row; double v; };  // row: a KeyMap index
  std::vector<Value> values;           // Set
  std::vector<std::string> unknown;    // keys the manifest lacks: reported, never sent
  std::vector<std::string> reserved;   // reserved rows' keys: reported, never sent
  bool unknown_type = false;           // Held because nobody knows the type
  int gesture_row = -1;                // Gesture on an engine row, else -1 (then it reaches neither engine nor host)
  bool gesture_begin = false;
  std::string gesture_key;
  Want want;                           // Want
  int index = -1;                      // Preset; Corner and Save: the corner, 0 to 3 (A to D), else -1
  int voices = -1;                     // Plugin: an old page's Voices, which the plug-in holds and drops
  bool has_hash = false;               // Hello
  std::string page_hash, page_manifest;   // Hello: the page's hash, and its manifest text if it sent one
  std::vector<std::pair<int, double>> settings;   // Morph: D.2 index (morph_core.h), v clamped to [0, 1]
  bool has_position = false;           // Position: both finite numbers, clamped to [0, 1]
  double x = 0, y = 0;
  struct KeyMode { int key; morph::KeyChoice choice; };   // Modes: key (a KeyMap index or kRatiosKey)
  std::vector<KeyMode> modes;
  int key = -1;                        // Waypoints, Lock: a KeyMap index or kRatiosKey; -1 for none
  std::vector<horde::Waypoint> points; // Waypoints: as sent, in order (the resolver clamps and judges them, MO-21)
  bool readable = true;                // Waypoints, WaypointLaw, Lock: false when its payload didn't read; nothing acts
  horde::WaypointLaw law = horde::WaypointLaw::Tps;
  bool clamp = true;
  bool has_offsets = false;            // Lock: false is "no lock" (the page's null)
  double offsets[3] = {};
};

// Parses one message. Never throws; an unreadable one comes back Invalid with `error` set.
Message parse_message(std::string_view json, const KeyMap& keys);

// ---- the mismatch banner ----------------------------------------------------------------------------------------
struct Mismatch {
  bool any = false;
  std::string expected, got;     // the engine's hash, the page's
  std::vector<std::string> keys; // rows whose line differs, or that one side lacks; empty unless the page sent its text
};
Mismatch check_hello(const Message& hello, const std::string& engine_hash, const std::string& engine_text);

// ---- the saved state ("current settings only", section 6 ruling 2) ----------------------------------------------
// The existing magic-line text, version 2: the preset and every engine row whose v differs from what that preset
// loads, by id. Version 1 (every exposed row, absolute) reads the same way: each line is a value to apply. A `voices`
// line, which both versions wrote until the human ruled "One control: Polyphony" (2026-11-05), is read and ignored.
// Version 3 (B636) adds the morph system's lines (morph_core.h, State) after the rows; version 4 (B639) adds MO-21's
// waypoints and law and MO-24's locks to them. A morph line is written only where the morph holds something a load
// wouldn't rebuild by itself, and the chunk says version 4 only when it holds a morph line; otherwise it is version 2,
// byte for byte as before, so an untouched patch saves exactly what it did. Versions 1 and 2 load with the morph at its
// defaults, version 3 with no waypoints or locks. Version 5 (B660) adds the test-only post-engine filter's line
// (filter_core.h); the chunk says version 5 only when the filter differs from its defaults (off), so every other chunk
// is byte for byte what it was, and a chunk with no filter line loads with the filter off.
struct SavedState {
  int preset = 0;
  std::vector<std::pair<std::uint32_t, double>> values;   // id, v
  morph::State morph;
  filter::State filter;
};
extern const char* const kStateMagic1;
extern const char* const kStateMagic2;
extern const char* const kStateMagic3;
extern const char* const kStateMagic4;
extern const char* const kStateMagic5;
std::string write_state(const SavedState& s);
bool read_state(std::string_view text, SavedState& out);   // false: not ours, and nothing is applied
// The rows of `current` that differ from `loaded` (both indexed like `ids`), as (id, v).
std::vector<std::pair<std::uint32_t, double>> differences(const std::vector<std::uint32_t>& ids, const std::vector<double>& current,
                                                          const std::vector<double>& loaded);

// ---- the polyphony cap (the plug-in's own rule; preview_plugin.cpp kPolyphonyCap) --------------------------------
// v for a stepped row of consecutive integers (voice.polyphony) as a load leaves it: `v` unchanged when its count is at
// most `cap`, else the option holding `cap`.
double capped_count_v(const horde::ParamInfo& row, double v, int cap);

// ---- snapshots to the page --------------------------------------------------------------------------------------
// A voice's note, from the plug-in's own note events (the engine's GUI frame has no note or gate).
struct NoteSlot {
  bool active = false;
  int key = -1;
  double velocity = 0;
  bool gate = false;
};

// What one `observe` is built from. Section 3 pins its decoded shape:
//   {sounding, voices: [{level, env: [4 stages], notes: {key, vel, gate}, osc: [{R, psi, frame, phases, levels,
//    blades: [{pos, width}]}]}], meters: {osc1, osc2, sub, noise, out: {peak, clip}}, latches: {clip, nonFinite},
//    counters: {IF-9's}}
// plus, only when want.summed asks (B632; section 3 has no shape for it, so this is the simplest): summed: {rate,
// samples: [..]}, the plug-in's own output, mono ((L + R) / 2), the newest window, every 16th sample.
struct FrameInputs {
  const horde::GuiFrame* frame = nullptr;   // the engine's latest RC-8 frame, or null before its first
  bool engine_silent = true;                // OU-4, as the audio thread last saw it
  const NoteSlot* notes = nullptr;          // horde::kMaxVoices of them, by engine voice index
  int unison[2] = {horde::kMaxMembers, horde::kMaxMembers};   // members to send per oscillator
  bool out_tapped = false;                  // the plug-in's own output peak since the last frame, when it has one
  float out_peak = 0;
  const float* scope = nullptr;             // GN-4: the summed output's window, when `want.summed` asked and there is one
  int scope_n = 0;
  double scope_rate = 0;                    // samples per second of that window
};
std::string observe_json(const FrameInputs& in, const Want& want);

// The snapshot's `morph` member (B636), only when `want.morph` asks; section 3 has no shape for it, so this is the
// simplest that carries RC-11.1's state and what the resolver resolved, in the frame-and-want pattern of the rest:
//   {x, y, dropped, unknown,             every frame: the position, the edits of morphing keys refused while on, and
//                                        resolver events of a kind this build doesn't forward (never silent)
//    settings: {key: v},                 D.2's thirteen, only when they changed
//    modes: {key: "quantum"|"exempt"},   whole, only when changed; counts: {key: 2|4}, likewise
//    corners: [{key: v}, x4],            each corner's rows that changed since last sent (all of them after a load)
//    ratios: [[osc1, osc2], x4],         the corners' texts, only when changed
//    heard: {key: v},                    the engine's stored value of each row that changed since last sent: what the
//                                        morph (or an edit) left there; a blending group's keys hold its heaviest
//                                        setting's (IF-4)
//    texts: [osc1, osc2],                the ratios texts the engine heard last, only when changed
//    sets: [{group, voice, settings: [{id, target, heard}]}]   IF-10's heard setting set of every group that holds one,
//                                        for the lowest sounding voice; every frame while one does (a 1.0 resolver
//                                        sends no sets, ADR-377, so in 1.0 it stays empty)
//    levels: [osc1, osc2, sub, noise],   IF-10's heard morph level of each source (MO-26), every frame (B639)
//    counters: {refusedWaypoints, refusedLocks, droppedInputs, droppedEvents},   the resolver's, every frame
//    waypoints: {key: [[x, y, v], ...]}, waypointLaw: {law, clamp}, locks: {key: [dB, dC, dD]}   only when changed
//   }
struct HeardGroup {
  const char* group = nullptr;
  int voice = -1;
  int n = 0;
  int id[horde::kMaxHeardSettings] = {};
  double target[horde::kMaxHeardSettings] = {}, heard[horde::kMaxHeardSettings] = {};
};
struct MorphFrame {
  double x = 0, y = 0;
  std::uint64_t dropped = 0, unknown = 0;
  const double* settings = nullptr;                                   // morph::kSettings values, or null: unchanged
  const std::vector<std::pair<std::string, morph::KeyChoice>>* modes = nullptr;   // non-default keys, or null
  const std::vector<std::pair<std::string, double>>* corners[4] = {};   // per corner: changed rows, or null
  const std::string (*ratios)[2] = nullptr;                           // four corners' texts, or null
  const std::vector<std::pair<std::string, double>>* heard = nullptr;
  const std::string* texts = nullptr;                                 // two heard texts, or null
  const std::vector<HeardGroup>* sets = nullptr;
  const double* levels = nullptr;                                     // four, or null
  const horde::ResolverCounters* counters = nullptr;
  const std::vector<std::pair<std::string, std::vector<horde::Waypoint>>>* waypoints = nullptr;   // keys holding any
  const horde::WaypointLaw* law = nullptr;                            // with `clamp`, or null: unchanged
  bool clamp = true;
  const std::vector<std::pair<std::string, std::array<double, 3>>>* locks = nullptr;
};
std::string morph_json(const MorphFrame& m);

// One snapshot's pieces. `ack` and `observe` ride every frame; the rest only when set (section 3).
struct Pieces {
  std::int64_t ack = 0;
  std::string observe;                                     // observe_json's output
  std::string morph;                                       // morph_json's output, or empty: no `morph` member
  const std::vector<std::pair<std::string, double>>* params = nullptr;   // key, v
  std::string events;                                      // a JSON array, or empty
  const std::vector<std::string>* preset_names = nullptr;
  int preset_index = -1;
  const std::string* plugin_build = nullptr;               // `plugin`: the build label
  const std::string* state = nullptr;
  const std::string* build = nullptr;                      // the hello reply's build label
  const std::string* engine_hash = nullptr;                // the hello reply's manifest hash
  const Mismatch* mismatch = nullptr;
  const std::vector<std::string>* refused_unknown = nullptr;
  const std::vector<std::string>* refused_reserved = nullptr;
  const std::vector<std::string>* refused_types = nullptr;
};
std::string snapshot_json(const Pieces& p);

std::string quoted(std::string_view s);   // a JSON string literal

}  // namespace preview::bridge
