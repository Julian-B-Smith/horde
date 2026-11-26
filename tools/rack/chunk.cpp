// The rack's chunk sections and the frozen legacy reader (ROADMAP B714). See chunk.h for the layout.
#include "rack/chunk.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <sstream>

#include "rack/rack.h"

namespace rack {

namespace {

FxRecord* record_of(FxState& fx, int n) {
  for (auto& r : fx.slots)
    if (r.n == n) return &r;
  return nullptr;
}
// The legacy record of slot `slot` in the table, made on first use, with every corner alike.
FxRecord& legacy_record(FxState& fx, const SlotTable& t, int slot) {
  const SlotRow& row = t.slots[std::size_t(slot)];
  for (auto& r : fx.slots)
    if (r.module == row.module) return r;
  FxRecord r;
  r.n = row.n;
  r.module = row.module;
  r.version = row.module_version;
  fx.slots.push_back(std::move(r));
  return fx.slots.back();
}
void put_key(std::vector<FxKey>& list, const std::string& address, double units, double v) {
  for (auto& k : list)
    if (k.address == address) { k.units = units; k.v = v; return; }   // a later line for the same key replaces the earlier one
  list.push_back({address, units, v});
}
// One translated legacy value: a host id and its value, in that id's host form or (plain) in units.
void legacy_value(FxState& fx, const SlotTable& t, std::uint32_t id, double value, bool plain) {
  int slot = -1, key = -1;
  if (!t.find(id, slot, key)) return;   // a module this build does not host: nothing to migrate it into
  FxRecord& r = legacy_record(fx, t, slot);
  if (key < 0) {
    const bool on = std::lround(std::clamp(value, 0.0, 1.0)) != 0;
    for (bool& c : r.on) c = on;
    return;
  }
  const KeyRow& k = t.slots[std::size_t(slot)].keys[std::size_t(key)];
  const double units = plain ? clamp_units(k.law, value) : units_of_host(k.law, value);
  const double v = v_of_units(k.law, units);
  if (k.cls == Cls::Device) put_key(r.device, k.address, units, v);
  else
    for (auto& c : r.keys) put_key(c, k.address, units, v);
}

}  // namespace

FxState capture(const Rack& rack) {
  FxState fx;
  const SlotTable* t = rack.table();
  if (!t) return fx;
  for (int s = 0; s < rack.slots(); ++s) {
    const SlotRow& row = t->slots[std::size_t(s)];
    FxRecord r;
    r.n = row.n;
    r.module = row.module;
    r.version = row.module_version;
    const bool on = rack.on(s);
    bool differs = on;
    for (bool& c : r.on) c = on;
    for (std::size_t k = 0; k < row.keys.size(); ++k) {
      const double u = rack.units(s, int(k));
      if (u == row.keys[k].def) continue;   // exact: the same units reload to the same sound
      differs = true;
      const KeyLaw& law = row.keys[k].law;
      const double host = rack.host(s, int(k));
      const FxKey key{row.keys[k].address, u, stepped(law) ? host / double(law.n - 1) : host};
      if (row.keys[k].cls == Cls::Device) r.device.push_back(key);
      else
        for (auto& c : r.keys) c.push_back(key);
    }
#ifdef HORDE_RACK_PLANT_DROP_SLOT_STATE   // must-fail: a save forgets the first slot's record
    if (s == 0) continue;
#endif
    if (differs) fx.slots.push_back(std::move(r));
  }
  return fx;
}

void write_fx(const FxState& fx, std::string& text) {
  if (fx.empty()) return;
  char line[256];
  text += "fx 1\n";
  for (const FxRecord& r : fx.slots) {
    std::snprintf(line, sizeof line, "fx.slot %d %s %s %s\n", r.n, r.module.c_str(), r.version.c_str(), r.origin.empty() ? "-" : r.origin.c_str());
    text += line;
    for (int c = 0; c < 4; ++c) {
      std::snprintf(line, sizeof line, "fx.on %d %d %d\n", r.n, c, r.on[c] ? 1 : 0);
      text += line;
      for (const FxKey& k : r.keys[c]) {
        std::snprintf(line, sizeof line, "fx.key %d %d %s %.17g %.17g\n", r.n, c, k.address.c_str(), k.units, k.v);
        text += line;
      }
    }
    for (const FxKey& k : r.device) {
      std::snprintf(line, sizeof line, "fx.device %d %s %.17g %.17g\n", r.n, k.address.c_str(), k.units, k.v);
      text += line;
    }
  }
  text += "fx.end\nprefs 1\nprefs.end\n";
}

bool read_fx_line(std::string_view line_in, FxState& fx) {
  const bool fx_line = line_in.substr(0, 3) == "fx " || line_in.substr(0, 3) == "fx.";
  const bool prefs_line = line_in.substr(0, 6) == "prefs " || line_in.substr(0, 6) == "prefs.";
  if (!fx_line && !prefs_line) return false;
  const std::string line(line_in);
  char a[160] = "", b[160] = "", c[160] = "";
  int n = 0, corner = 0, on = 0;
  double u = 0, v = NAN;
  if (std::sscanf(line.c_str(), "fx.slot %d %159s %159s %159s", &n, a, b, c) == 4) {
    if (record_of(fx, n)) return true;   // a slot named twice: the first record stands
    FxRecord r;
    r.n = n;
    r.module = a;
    r.version = b;
    r.origin = c;
    fx.slots.push_back(std::move(r));
  } else if (std::sscanf(line.c_str(), "fx.on %d %d %d", &n, &corner, &on) == 3) {
    FxRecord* r = record_of(fx, n);
    if (r && corner >= 0 && corner < 4) {
      r->on[corner] = on != 0;
      r->bytes[corner] += line.size() + 1;
    }
  } else if (std::sscanf(line.c_str(), "fx.key %d %d %159s %lf %lf", &n, &corner, a, &u, &v) >= 4) {
    FxRecord* r = record_of(fx, n);
    if (r && corner >= 0 && corner < 4 && std::isfinite(u)) {
      put_key(r->keys[corner], a, u, v);
      r->bytes[corner] += line.size() + 1;
    }
  } else if (std::sscanf(line.c_str(), "fx.device %d %159s %lf %lf", &n, a, &u, &v) >= 3) {
    FxRecord* r = record_of(fx, n);
    if (r && std::isfinite(u)) put_key(r->device, a, u, v);
  }
  // `fx 1`, `fx.end`, `prefs 1`, `prefs.end` and any line of a later layout: consumed, nothing to hold in S1
  return true;
}

bool read_legacy_line(const SlotTable& t, int version, std::string_view line_in, FxState& fx) {
  for (const LegacyLine& l : t.legacy) {
    if (l.version > version) continue;   // a version-5 chunk can't hold version 6's lines
    if (line_in.substr(0, l.prefix.size() + 1) != l.prefix + " ") continue;
#ifdef HORDE_RACK_PLANT_IGNORE_LEGACY   // must-fail: the reader consumes the old lines and drops what they say
    return true;
#endif
    const std::string line(line_in.substr(l.prefix.size() + 1));
    std::istringstream in(line);
    if (!l.ids.empty()) {
      // Positional: exactly one finite value per id, or the line is no line at all.
      std::vector<double> v;
      std::string word;
      while (in >> word) {
        char* end = nullptr;
        const double x = std::strtod(word.c_str(), &end);
        if (end == word.c_str() || *end || !std::isfinite(x)) return true;
        v.push_back(x);
      }
      if (v.size() != l.ids.size()) return true;
      for (std::size_t i = 0; i < v.size(); ++i) legacy_value(fx, t, l.ids[i], v[i], l.plain);
      fx.migrated = true;
      return true;
    }
    std::string first;
    in >> first;
    if (!l.on_word.empty() && first == l.on_word) {
      int on = 0;
      if (in >> on) {
        legacy_value(fx, t, l.on_id, on != 0 ? 1 : 0, false);
        fx.migrated = true;
      }
      return true;
    }
    char* end = nullptr;
    const long k = std::strtol(first.c_str(), &end, 10);
    double x = 0;
    if (end == first.c_str() || *end || k < 0 || k >= l.count || !(in >> x) || !std::isfinite(x)) return true;
    legacy_value(fx, t, l.id_base + std::uint32_t(k), x, l.plain);
    fx.migrated = true;
    return true;
  }
  return false;
}

void apply(Rack& rack, const FxState& fx, std::vector<std::string>& report, const std::function<void()>& between) {
  const SlotTable* t = rack.table();
  if (!t) return;
  // The final state first, then one store per value. Never through a transient dry state: the audio thread reads the
  // switches and values while this runs (a host may restore state during playback), and a slot it saw off for one
  // block would restart cleared on the next, cutting the tail of a slot the load keeps on. So: a slot the load leaves
  // on takes its values first and its switch last; a slot it turns off takes its switch first. `between` is a test
  // seam, called after every store, where a test runs the audio thread.
  struct Final { bool on = false; std::vector<double> units, host; };
  std::vector<Final> fin(std::size_t(rack.slots()));
  for (int s = 0; s < rack.slots(); ++s) {
    const SlotRow& row = t->slots[std::size_t(s)];
    for (const KeyRow& k : row.keys) fin[std::size_t(s)].units.push_back(k.def), fin[std::size_t(s)].host.push_back(NAN);
  }
  for (const FxRecord& r : fx.slots) {
    int slot = -1;
    for (int s = 0; s < rack.slots() && slot < 0; ++s)
      if (t->slots[std::size_t(s)].module == r.module) slot = s;
    if (slot < 0) {
      report.push_back("fx[" + r.module + "]: this build hosts no such module; its record is dropped");
      continue;
    }
    const std::size_t budget = rack.effective_budget(slot);
    if (budget && r.bytes[0] > budget) {
      report.push_back("fx[" + r.module + "]: corner A holds " + std::to_string(r.bytes[0]) + " bytes, over its " + std::to_string(budget) +
                       "-byte budget (RS-6.12); the slot loads dry");
      continue;
    }
    if (!r.version.empty() && r.version != t->slots[std::size_t(slot)].module_version)
      report.push_back("fx[" + r.module + "]: saved by manifest " + r.version.substr(0, 12) + ", loaded by " +
                       t->slots[std::size_t(slot)].module_version.substr(0, 12) + "; its keys load by address");
    const SlotRow& row = t->slots[std::size_t(slot)];
    Final& f = fin[std::size_t(slot)];
    const auto key_of = [&](const std::string& address) {
      for (std::size_t k = 0; k < row.keys.size(); ++k)
        if (row.keys[k].address == address) return int(k);
      return -1;
    };
    // The units are what the module hears; the v, where the record has one, is the host value it read (a v the
    // reader could not read is NaN, and the rack derives one from the units).
    const auto take = [&](const FxKey& key, bool device) {
      const int k = key_of(key.address);
      if (k < 0 || (row.keys[std::size_t(k)].cls == Cls::Device) != device) return;
      const KeyLaw& law = row.keys[std::size_t(k)].law;
      f.units[std::size_t(k)] = key.units;
      f.host[std::size_t(k)] = stepped(law) ? key.v * double(law.n - 1) : key.v;
    };
    for (const FxKey& key : r.keys[0]) take(key, false);
    for (const FxKey& key : r.device) take(key, true);
    f.on = r.on[0];
  }
#ifdef HORDE_RACK_PLANT_APPLY_VIA_DRY   // must-fail: the load passes through the dry state, as the first S1 build did
  rack.set_dry();
  if (between) between();
#endif
  for (int s = 0; s < rack.slots(); ++s) {
    const Final& f = fin[std::size_t(s)];
    if (!f.on) {
      rack.set_on(s, false);
      if (between) between();
    }
    for (std::size_t k = 0; k < f.units.size(); ++k) {
      rack.set_units(s, int(k), f.units[k], f.host[k]);
      if (between) between();
    }
    if (f.on) {
      rack.set_on(s, true);
      if (between) between();
    }
  }
}

}  // namespace rack
