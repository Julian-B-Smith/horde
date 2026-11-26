#pragma once
// The slot table (ROADMAP B714; SPEC-ENGINE MO-28; the rack-slot contract's RS-2 and §12.11): which module each slot
// hosts and every key it exposes, built ONCE on the main thread from data alone (each module's pinned manifest, horde's
// law/mod overlay, its state-budget row and the shell's host-id ledgers) and read-only after, so any thread may read it.
//
// Nothing here names a module. A consumer links a generated registry (tools/rack/CMakeLists.txt, horde_rack_registry)
// whose entries carry the texts and a factory; the table is the same code for one module or eight. A row the table
// cannot use is REFUSED LOUDLY: the slot is left out, and the reason is in `refused` (published in hello.slots and the
// session log), never a silent skip. The refusals: an undeclared class (A1), a flip key with no switch method (A11), an
// address with no overlay entry or no host id, a module over kMaxKeys, a manifest whose module name is not the entry's.
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "rack/law.h"

namespace admission { class Module; }

namespace rack {

constexpr int kMaxSlots = 8;
constexpr int kMaxKeys = 64;   // per slot, switch excluded; a module over it is refused at the table's build

// What a consumer's generated registry hands the table: one module, as data.
struct ModuleSource {
  std::string key;             // modules.json key ("the budget row's key" and the plant names)
  std::string name;            // display name, the host parameters' prefix
  std::string pin;             // the 40-hex commit its tree is pinned at ("" for an in-repo test module)
  std::string manifest;        // the pinned manifest's text, as the module ships it
  std::string overlay;         // tools/rack/law/<key>.json
  std::string budgets;         // the state-budget table file's text
  std::string budget_key;      // its row's key in that table
  std::function<std::unique_ptr<admission::Module>()> make;
};

enum class Cls : std::uint8_t { Morph, Flip, Device };

struct KeyRow {
  std::string address;         // the manifest's, or io.inGain / io.outGain for a pair the slot supplies
  std::string label;           // label_by_type for the module's one type, else the address after its last '.'
  std::string unit;            // the manifest's ("" for none, and for an enum, whose labels name the options)
  std::string law_name;        // the overlay's word: lin, log, step, io
  Cls cls = Cls::Morph;
  KeyLaw law;
  bool mod = false;            // the overlay's mod flag (X114 (b))
  double def = 0;              // units
  std::vector<std::string> labels;   // a stepped key's options, from the manifest's `values`
  std::uint32_t host_id = 0;   // the ledger's
  bool slot_io = false;        // the slot supplies it (the module declares no io pair, RS-4.7); not a module parameter
};

struct FixedControl { std::string label, address; };   // RS-5.8: at most four, the manifest's fixedControls

struct SlotRow {
  int n = 0;                   // N in fx/N, from 1 in the table's order (MO-28)
  std::string key, name, module, pin;
  std::string module_version;  // sha256 of the pinned manifest text (MO-31's module_version)
  std::uint32_t budget_row = 0;        // the table's per-corner bytes
  std::string budget_status;           // ruled | provisional, as the table says
  std::uint32_t on_id = 0;             // fx/N.on's host id
  bool owns_io = false;                // the manifest declares io.inGain and io.outGain itself
  int io_in = -1, io_out = -1;         // indices into keys of the slot's I/O pair (owned or supplied)
  std::vector<KeyRow> keys;            // morph and flip keys in manifest order, then Device keys, then a supplied io pair
  std::vector<KeyRow> constants;       // flip keys with fewer than two values: held at their default, never exposed
  std::vector<FixedControl> fixed;
  std::function<std::unique_ptr<admission::Module>()> make;
};

// One frozen pre-B714 page key (critic finding 2): published in hello.preview until GUI 3's page reads hello.slots.
struct Alias {
  int slot = 0;                // index into slots
  int key = -1;                // index into that slot's keys; -1 is its switch
  std::string preview;         // the old page key
  bool plain = false;          // published in module units (lin over [min, max]), else in the host value's own form
};

// A frozen legacy chunk line (versions 5 and 6), translated by ids, never by module code.
struct LegacyLine {
  int version = 0;
  std::string prefix;
  std::vector<std::uint32_t> ids;      // positional: one value per id, in order
  std::string on_word;                 // indexed: "<prefix> <on_word> <0|1>" sets on_id
  std::uint32_t on_id = 0, id_base = 0;
  int count = 0;                       // indexed: "<prefix> <k> <value>" for k in [0, count), id id_base + k
  bool plain = false;                  // values in module units, else in each id's host form
};

struct SlotTable {
  std::vector<SlotRow> slots;
  std::vector<std::string> refused;    // whole-slot refusals, one line each
  std::vector<Alias> aliases;
  std::vector<LegacyLine> legacy;
  std::string ledger_hash;             // sha256 of the ledgers' texts, joined: hello.slots' `ledger`
  // A host id's slot and key (key -1: the switch), or false.
  bool find(std::uint32_t id, int& slot, int& key) const;
};

// Builds the table. `ledgers` are the shell's host-id ledger texts (host_ids.lock.json and, in a test build, its own).
SlotTable build_slot_table(const std::vector<ModuleSource>& modules, const std::vector<std::string>& ledgers);

std::string sha256_hex(const std::string& bytes);

// The consumer's registry (generated by horde_rack_registry): its modules and ledgers, in slot order.
const std::vector<ModuleSource>& registered_modules();
const std::vector<std::string>& registered_ledgers();
// The table of the registry, built once on first use (main thread first; read-only after).
const SlotTable& registered_table();

}  // namespace rack
