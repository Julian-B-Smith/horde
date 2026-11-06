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
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "horde_engine.h"

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
};

enum class Kind {
  Set,       // engine values: `values` holds the rows to send
  Gesture,   // RC-11.3's begin or end
  Want,      // the parts of RC-8 the page draws
  Preset,    // the bank preset to load
  Plugin,    // the test plug-in's own Voices setting
  Hello,     // the page has loaded: its manifest hash, for the mismatch banner
  Held,      // morph, position, modes, rack, midi, clear, or a type nobody knows: acknowledged and dropped
  Invalid,   // unreadable, or no numeric seq: nothing to acknowledge
};

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
  int index = -1;                      // Preset
  int voices = -1;                     // Plugin: an old page's Voices, which the plug-in holds and drops
  bool has_hash = false;               // Hello
  std::string page_hash, page_manifest;   // Hello: the page's hash, and its manifest text if it sent one
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
struct SavedState {
  int preset = 0;
  std::vector<std::pair<std::uint32_t, double>> values;   // id, v
};
extern const char* const kStateMagic1;
extern const char* const kStateMagic2;
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
struct FrameInputs {
  const horde::GuiFrame* frame = nullptr;   // the engine's latest RC-8 frame, or null before its first
  bool engine_silent = true;                // OU-4, as the audio thread last saw it
  const NoteSlot* notes = nullptr;          // horde::kMaxVoices of them, by engine voice index
  int unison[2] = {horde::kMaxMembers, horde::kMaxMembers};   // members to send per oscillator
  bool out_tapped = false;                  // the plug-in's own output peak since the last frame, when it has one
  float out_peak = 0;
};
std::string observe_json(const FrameInputs& in, const Want& want);

// One snapshot's pieces. `ack` and `observe` ride every frame; the rest only when set (section 3).
struct Pieces {
  std::int64_t ack = 0;
  std::string observe;                                     // observe_json's output
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
