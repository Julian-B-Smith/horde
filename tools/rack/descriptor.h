#pragma once
// What the rack tells a page (ROADMAP B714): the hello reply's `slots` descriptor and the frames' slot meters, both
// generated from the slot table alone. Names are SPEC-ENGINE MO-28's: a module key fx/N/<address>, a switch fx/N.on.
// Main thread; JSON text.
#include <string>

#include "rack/rack.h"

namespace rack {

// {ledger, refused: [..], slots: [{slot, module, name, pin, version, budget: {bytes, status}, owns_io,
//   fixed: [{label, key}], on: {key, id}, keys: [{key, id, name, cls, law, mod, min, max, default, stepped, labels?,
//   unit}]}]}. min, max and default are in the module's units; a page's v for a key is its place by `law` (law.h).
// budget.bytes is RS-6.12's per-corner figure: the table's row, or the module's own where that is smaller.
std::string slots_json(const Rack& rack);
// [{slot, in: {peak, clip}, out: {peak, clip}, nonfinite, tailLost}], one per slot, the observe member's meters.slots.
std::string slot_meters_json(const Rack& rack);
// The host parameter's name for a slot's switch (key -1) or key: the ledger's frozen name where it has one, else
// "<module name> on" / "<module name> <label>".
std::string param_name(const SlotRow& row, int key);
// A key's option texts as the host shows them: the ledger's frozen ones where it has them, else the manifest's.
const std::vector<std::string>& host_labels(const KeyRow& key);
// MO-28's names.
std::string key_name(int n, const std::string& address);
std::string switch_name(int n);
std::string json_quoted(const std::string& s);
void json_number(std::string& out, double x);   // the shortest text that reads back to the same double

}  // namespace rack
