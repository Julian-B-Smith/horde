#pragma once
// The rack's part of a saved chunk (ROADMAP B714; SPEC-ENGINE MO-31; the rack-slot contract's §6.2): the patch's `fx`
// section, and the frozen reader of the pre-B714 module lines. Pure text, main thread, no audio.
//
// THE LAYOUT (MO-31 leaves it "the lead's to state"; this is the statement, version 1). Lines, after the shell's own:
//   fx 1                                     the section and its version
//   fx.slot <N> <module> <version> <origin>  slot N's record fx[<module>]: the manifest's sha256 (module_version) and
//                                            the preset it came from ("-": none, every S1 record)
//   fx.on <N> <c> <0|1>                      per corner c (0-3 = A-D): the slot's switch (MO-29)
//   fx.key <N> <c> <address> <units> <v>     per corner: a morph or flip key's value, where it differs from the
//                                            table's default (MO-28): in the module's units at %.17g, what the module
//                                            hears, and its v by the key's law, what the host and the resolver read
//                                            (host -> units -> v needn't round-trip bit for bit, so both are kept)
//   fx.device <N> <address> <units> <v>      once per patch, outside every corner: a Device key's value where it
//                                            differs from its default (MO-31's device settings, §6.2's device[<module>])
//   fx.end
//   prefs 1                                  the DEVICE PREFERENCES, not the patch's: device.fxFeedback and the slot
//   prefs.end                                pick land here from S3 and S5 (critic finding 14); empty in S1, reserved
//                                            so the format does not churn
// A slot at MO-31's dry state (off at every corner, every key at its default) writes no record, so a chunk with no
// record writes no `fx` at all and keeps the version it always had. Until S5 the four corners are written identical
// (the rack holds one value per key) and the reader takes corner A. Never saved: tails, glides, meters, latches.
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "rack/slot_table.h"

namespace rack {

class Rack;

// One key's value: in the module's units, and its v (NaN where a reader had none: derived from the units).
struct FxKey {
  std::string address;
  double units = 0, v = 0;
};

struct FxRecord {
  int n = 0;                   // the slot as the writer numbered it; the reader matches records to slots by module
  std::string module, version, origin = "-";
  bool on[4] = {};
  std::vector<FxKey> keys[4];
  std::vector<FxKey> device;
  std::size_t bytes[4] = {};   // as read: each corner's lines, for RS-6.12's budget guard
};

struct FxState {
  std::vector<FxRecord> slots;
  bool migrated = false;       // filled from version 5 or 6 lines, not from an `fx` section
  bool empty() const { return slots.empty(); }
};

// What the rack holds now, as records (slots at the dry state left out).
FxState capture(const Rack& rack);
// Writes the two sections (nothing at all when `fx` is empty).
void write_fx(const FxState& fx, std::string& text);
// One line without its newline. True when it was an fx or prefs line (consumed, read or not).
bool read_fx_line(std::string_view line, FxState& fx);
// A version 5 or 6 chunk's module line, through the table's frozen legacy entries (data, never module code). True
// when the line was one (consumed, whether it held a usable value or not).
bool read_legacy_line(const SlotTable& table, int version, std::string_view line, FxState& fx);
// Loads `fx` into the rack: the dry state first, then each record whose module the rack hosts, from corner A. A
// record over its slot's budget (RS-6.12) or for a module this build does not host is refused, its slot left dry, and
// a line says so in `report`.
void apply(Rack& rack, const FxState& fx, std::vector<std::string>& report);

}  // namespace rack
