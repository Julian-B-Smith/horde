// The slot table's build (ROADMAP B714). See slot_table.h for what it owes; every rule here is read from data.
#include "rack/slot_table.h"

#include <CommonCrypto/CommonDigest.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <tuple>
#include <utility>

#include "choc/text/choc_JSON.h"

namespace rack {

namespace {

using choc::value::ValueView;

bool number(const ValueView& v, double& out) {
  if (!v.isInt() && !v.isFloat()) return false;
  out = v.get<double>();
  return std::isfinite(out);
}
std::string text(const ValueView& obj, const char* member) {
  return obj.isObject() && obj.hasObjectMember(member) && obj[member].isString() ? std::string(obj[member].getString()) : std::string();
}
bool flag(const ValueView& obj, const char* member) { return obj.isObject() && obj.hasObjectMember(member) && obj[member].isBool() && obj[member].getBool(); }
std::uint32_t hex_id(const ValueView& obj, const char* member) {
  const std::string s = text(obj, member);
  return s.empty() ? 0 : std::uint32_t(std::strtoul(s.c_str(), nullptr, 0));
}

// One host-id ledger entry: (module, key) or (module, the switch) -> id.
struct LedgerKey {
  std::string module, key;
  bool sw = false;
  bool operator<(const LedgerKey& o) const { return std::tie(module, key, sw) < std::tie(o.module, o.key, o.sw); }
};

// The four switch methods a manifest may declare (ADR-309). The slot itself builds none of them in S1: a module
// switches its own flip keys by crossfade or silently, as every module hosted at S1 does. `gate` (the slot's duck, swap and un-duck,
// RS-4.3) and `seeded-jump` (whose seed the slot would have to own, critic finding 18) are refused until built.
bool switch_supported(const std::string& method, std::string& why) {
  if (method == "crossfade" || method == "silent") return true;
  why = method.empty() ? "declares no switch method (A11)" : "declares switch method \"" + method + "\", which the slot does not build yet (RS-4.3)";
  return false;
}

bool law_of(const std::string& word, Law& out) {
  if (word == "lin") out = Law::Lin;
  else if (word == "log") out = Law::Log;
  else if (word == "step") out = Law::Step;
  else if (word == "io" || word == "io-cut") out = Law::Io;
  else return false;
  return true;
}

}  // namespace

std::string sha256_hex(const std::string& bytes) {
  unsigned char d[CC_SHA256_DIGEST_LENGTH];
  CC_SHA256(bytes.data(), CC_LONG(bytes.size()), d);
  static const char* const hex = "0123456789abcdef";
  std::string out;
  for (unsigned char c : d) { out += hex[c >> 4]; out += hex[c & 15]; }
  return out;
}

bool SlotTable::find(std::uint32_t id, int& slot, int& key) const {
  for (std::size_t s = 0; s < slots.size(); ++s) {
    if (slots[s].on_id == id) { slot = int(s); key = -1; return true; }
    for (std::size_t k = 0; k < slots[s].keys.size(); ++k)
      if (slots[s].keys[k].host_id == id) { slot = int(s); key = int(k); return true; }
  }
  return false;
}

SlotTable build_slot_table(const std::vector<ModuleSource>& modules, const std::vector<std::string>& ledgers) {
  SlotTable t;
  std::map<LedgerKey, std::pair<std::uint32_t, bool>> ids;   // -> (id, slot_io)
  std::set<std::uint32_t> seen;
  std::vector<choc::value::Value> alias_docs;
  std::string joined;
  for (const std::string& text_in : ledgers) {
    joined += text_in;
    choc::value::Value doc;
    try { doc = choc::json::parse(text_in); } catch (const std::exception& e) { t.refused.push_back(std::string("a ledger is not JSON: ") + e.what()); continue; }
    if (doc.hasObjectMember("ids") && doc["ids"].isArray())
      for (std::uint32_t i = 0; i < doc["ids"].size(); ++i) {
        const auto e = doc["ids"][i];
        const std::uint32_t id = hex_id(e, "id");
        if (!id || !seen.insert(id).second) { t.refused.push_back("ledger id " + text(e, "id") + " is zero or taken twice: the entry is ignored"); continue; }
        ids[{text(e, "module"), text(e, "key"), flag(e, "switch")}] = {id, flag(e, "slot_io")};
      }
    if (doc.hasObjectMember("aliases") && doc["aliases"].isArray())
      for (std::uint32_t i = 0; i < doc["aliases"].size(); ++i) alias_docs.push_back(choc::value::Value(doc["aliases"][i]));
    if (doc.hasObjectMember("legacy") && doc["legacy"].isArray())
      for (std::uint32_t i = 0; i < doc["legacy"].size(); ++i) {
        const auto e = doc["legacy"][i];
        LegacyLine l;
        double n = 0;
        if (e.hasObjectMember("version") && number(e["version"], n)) l.version = int(n);
        l.prefix = text(e, "prefix");
        if (e.hasObjectMember("ids") && e["ids"].isArray())
          for (std::uint32_t k = 0; k < e["ids"].size(); ++k) l.ids.push_back(std::uint32_t(std::strtoul(std::string(e["ids"][k].getString()).c_str(), nullptr, 0)));
        l.on_word = text(e, "on_word");
        l.on_id = hex_id(e, "on_id");
        l.id_base = hex_id(e, "id_base");
        if (e.hasObjectMember("count") && number(e["count"], n)) l.count = int(n);
        l.plain = text(e, "form") == "plain";
        if (!l.prefix.empty()) t.legacy.push_back(std::move(l));
      }
  }
  t.ledger_hash = sha256_hex(joined);

  for (const ModuleSource& src : modules) {
    const auto refuse = [&](const std::string& why) { t.refused.push_back(src.key + ": " + why + "; the slot is left out"); };
    if (int(t.slots.size()) >= kMaxSlots) { refuse("past kMaxSlots (" + std::to_string(kMaxSlots) + ")"); continue; }
    choc::value::Value man, over, budgets;
    try {
      man = choc::json::parse(src.manifest);
      over = choc::json::parse(src.overlay);
      budgets = choc::json::parse(src.budgets);
    } catch (const std::exception& e) {
      refuse(std::string("its manifest, overlay or budget table is not JSON: ") + e.what());
      continue;
    }
    SlotRow row;
    row.n = int(t.slots.size()) + 1;
    row.key = src.key;
    row.name = src.name;
    row.pin = src.pin;
    row.module = text(man, "module");
    row.module_version = sha256_hex(src.manifest);
    row.make = src.make;
    if (row.module.empty() || text(over, "module") != row.module) { refuse("the manifest's module \"" + row.module + "\" is not its overlay's \"" + text(over, "module") + "\""); continue; }
    {
      const auto b = budgets.hasObjectMember("modules") ? budgets["modules"] : budgets;
      double bytes = 0;
      if (!b.isObject() || !b.hasObjectMember(src.budget_key.c_str()) || !number(b[src.budget_key.c_str()]["bytes"], bytes) || bytes <= 0) {
        refuse("no state-budget row \"" + src.budget_key + "\"");
        continue;
      }
      row.budget_row = std::uint32_t(bytes);
      row.budget_status = text(b[src.budget_key.c_str()], "status");
    }
    const auto sw = ids.find({row.module, "", true});
    if (sw == ids.end()) { refuse("no host id for its switch in the ledger (append one at next_free)"); continue; }
    row.on_id = sw->second.first;

    const auto params = man["params"];
    const auto okeys = over.hasObjectMember("keys") ? over["keys"] : choc::value::Value();
    std::string why;
    bool in = false, out = false;
    // label_by_type names a key per type: the module's one type is its constant flip key's only value
    std::string the_type;
    for (std::uint32_t i = 0; params.isArray() && i < params.size(); ++i) {
      const auto p = params[i];
      if (text(p, "class") == "flip" && p.hasObjectMember("values") && p["values"].isArray() && p["values"].size() == 1) the_type = std::string(p["values"][0].getString());
    }
    std::set<std::string> addresses;
    for (std::uint32_t i = 0; params.isArray() && i < params.size() && why.empty(); ++i) {
      const auto p = params[i];
      KeyRow k;
      k.address = text(p, "address");
      if (k.address.empty() || k.address.find('/') != std::string::npos || !addresses.insert(k.address).second) {
        why = "address \"" + k.address + "\" is empty, holds a '/', or is declared twice (RS-2.9)";
        break;
      }
      const std::string cls = text(p, "class");
      if (cls == "morph") k.cls = Cls::Morph;
      else if (cls == "flip") k.cls = Cls::Flip;
      else if (cls == "device") k.cls = Cls::Device;
      else { why = k.address + " declares no class (A1)"; break; }
      double lo = 0, hi = 0, def = 0;
      if (!number(p["min"], lo) || !number(p["max"], hi) || !number(p["default"], def)) { why = k.address + " lacks a finite min, max or default (RS-2.1)"; break; }
      k.unit = text(p, "unit");
      if (p.hasObjectMember("values") && p["values"].isArray())
        for (std::uint32_t v = 0; v < p["values"].size(); ++v) k.labels.emplace_back(p["values"][v].getString());
      double steps = 0;
      if (p.hasObjectMember("steps")) number(p["steps"], steps);
      const int n = !k.labels.empty() ? int(k.labels.size()) : int(steps);
      std::string word;
      bool overlay_mod = false;
      if (p.hasObjectMember("law")) word = text(p, "law");   // the module's own, once FOUNDATIONS names the field
      else if (okeys.isObject() && okeys.hasObjectMember(k.address.c_str())) {
        word = text(okeys[k.address.c_str()], "law");
        overlay_mod = flag(okeys[k.address.c_str()], "mod");
      }
      if (word.empty()) { why = k.address + " has no law: neither its manifest nor tools/rack/law/" + src.key + ".json gives one"; break; }
      k.mod = p.hasObjectMember("mod") ? flag(p, "mod") : overlay_mod;
      k.law_name = word;
      if (!law_of(word, k.law.law)) { why = k.address + "'s law \"" + word + "\" is none of lin, log, step, io"; break; }
      k.law.lo = lo;
      k.law.hi = hi;
      k.def = def;
      if (k.cls == Cls::Flip || k.law.law == Law::Step) {
        // A flip key with fewer than two values is a constant (ADR-412's reading, generic): held at its default, kept
        // out of the table, the host ids and the page. MO-28's n >= 2 would otherwise refuse the module's whole table.
        if (n < 2) {
          k.law.n = 0;
          row.constants.push_back(std::move(k));
          continue;
        }
        k.law.n = n;
        if (k.law.law != Law::Step) { k.law.law = Law::Step; k.law_name = "step"; }
        if (k.cls == Cls::Flip) {
          const auto s = p.hasObjectMember("switch") ? p["switch"] : choc::value::Value();
          if (!switch_supported(text(s, "method"), why)) { why = k.address + " " + why; break; }
        }
      }
      if (k.law.law == Law::Log && !(k.law.lo > 0 && k.law.hi > 0)) { why = k.address + " is log over a range that reaches 0"; break; }
      k.label = k.address.substr(k.address.rfind('.') + 1);
      if (!the_type.empty() && p.hasObjectMember("label_by_type") && p["label_by_type"].hasObjectMember(the_type.c_str()))
        k.label = std::string(p["label_by_type"][the_type.c_str()].getString());
      const auto id = ids.find({row.module, k.address, false});
      if (id == ids.end()) { why = "no host id for " + k.address + " in the ledger (append it at next_free)"; break; }
      k.host_id = id->second.first;
      in = in || k.address == "io.inGain";
      out = out || k.address == "io.outGain";
      row.keys.push_back(std::move(k));
    }
    if (!why.empty()) { refuse(why); continue; }
    // The overlay names exactly the manifest's addresses: an extra entry is a typo or a stale key.
    for (std::uint32_t i = 0; okeys.isObject() && i < okeys.size(); ++i)
      if (!addresses.count(std::string(okeys.getObjectMemberAt(i).name))) why = "its overlay names " + std::string(okeys.getObjectMemberAt(i).name) + ", which its manifest lacks";
    if (!why.empty()) { refuse(why); continue; }
    // RS-4.7: a module that declares both io keys owns them; otherwise the slot supplies an identical pair (RS-4.6:
    // +-24 dB, lin in dB, unity at 0 dB), Device class, outside every corner, each with its ledger id.
    row.owns_io = in && out;
    if (!row.owns_io) {
      for (const char* address : {"io.inGain", "io.outGain"}) {
        const auto id = ids.find({row.module, address, false});
        if (id == ids.end() || !id->second.second) { why = std::string("declares no io pair, and the ledger has no slot_io id for ") + address; break; }
        KeyRow k;
        k.address = address;
        k.label = k.address.substr(3);
        k.unit = "dB";
        k.law_name = "io";
        k.cls = Cls::Device;
        k.law = {Law::Io, -24, 24, 0};
        k.def = 0;
        k.host_id = id->second.first;
        k.slot_io = true;
        row.keys.push_back(std::move(k));
      }
      if (!why.empty()) { refuse(why); continue; }
    }
    for (std::size_t k = 0; k < row.keys.size(); ++k) {
      if (row.keys[k].address == "io.inGain") row.io_in = int(k);
      if (row.keys[k].address == "io.outGain") row.io_out = int(k);
    }
    if (int(row.keys.size()) > kMaxKeys) { refuse(std::to_string(row.keys.size()) + " keys, past kMaxKeys (" + std::to_string(kMaxKeys) + ")"); continue; }
#ifndef HORDE_RACK_PLANT_NO_CONST_CAP   // must-fail: the table takes more constants than a slot can hold
    if (int(row.constants.size()) > kMaxKeys) { refuse(std::to_string(row.constants.size()) + " constant keys, past kMaxKeys (" + std::to_string(kMaxKeys) + ")"); continue; }
#endif
    if (man.hasObjectMember("fixedControls") && man["fixedControls"].isArray())
      for (std::uint32_t i = 0; i < man["fixedControls"].size() && i < 4; ++i) {
        const auto f = man["fixedControls"][i];
        if (addresses.count(text(f, "key"))) row.fixed.push_back({text(f, "label"), text(f, "key")});
      }
    t.slots.push_back(std::move(row));
  }

  for (const auto& a : alias_docs) {
    Alias al;
    al.preview = text(a, "preview");
    al.plain = text(a, "form") == "plain";
    al.name = text(a, "name");
    if (a.hasObjectMember("labels") && a["labels"].isArray())
      for (std::uint32_t i = 0; i < a["labels"].size(); ++i) al.labels.emplace_back(a["labels"][i].getString());
    bool found = false;
    for (std::size_t s = 0; s < t.slots.size() && !found; ++s) {
      if (t.slots[s].module != text(a, "module")) continue;
      al.slot = int(s);
      if (flag(a, "switch")) { al.key = -1; found = true; }
      for (std::size_t k = 0; k < t.slots[s].keys.size() && !found; ++k)
        if (t.slots[s].keys[k].address == text(a, "key")) { al.key = int(k); found = true; }
    }
    if (!found || al.preview.empty()) continue;   // a module this build doesn't host has no alias here
    SlotRow& row = t.slots[std::size_t(al.slot)];
    if (al.key < 0) {
      if (!al.name.empty()) row.on_host_name = al.name;
    } else {
      KeyRow& k = row.keys[std::size_t(al.key)];
      if (!al.name.empty()) k.host_name = al.name;
      // An alias can rename a key's options, never add or drop one: a count that differs is a ledger at odds with the
      // manifest, refused loudly (the host would otherwise read the manifest's labels under the alias's name).
      if (!al.labels.empty() && al.labels.size() != k.labels.size()) {
        t.refused.push_back("alias " + al.preview + ": " + std::to_string(al.labels.size()) + " labels for " + k.address + "'s " +
                            std::to_string(k.labels.size()) + " options; the alias is ignored");
        continue;
      }
      if (!al.labels.empty()) k.host_labels = al.labels;
    }
    t.aliases.push_back(al);
  }
  return t;
}


}  // namespace rack
