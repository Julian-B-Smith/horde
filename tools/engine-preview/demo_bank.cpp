// The demo bank's reader (B645). See demo_bank.h for the file's rules and why each one holds.
#include "demo_bank.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "choc/text/choc_JSON.h"
#include "manifest/manifest.h"   // the engine's own option names (param_text) and its law's inverse (v_of_value)
#include "morph_core.h"

namespace preview::demo {

namespace {

using choc::value::ValueView;

bool number_of(const ValueView& v, double& out) {
  if (!v.isInt() && !v.isFloat()) return false;
  out = v.get<double>();
  return std::isfinite(out);
}

std::vector<std::string> options_of(const horde::ParamInfo& p) {
  std::vector<std::string> out;
  const std::size_t index = horde::param_index(p.id);
  const char* all = index == horde::kNoParam ? nullptr : horde::param_text(index).options;
  if (!all) return out;
  std::string part;
  for (const char* c = all; *c; ++c) {
    if (*c == '|') { out.push_back(part); part.clear(); }
    else part += *c;
  }
  out.push_back(part);
  return out;
}

// One row's value as the file writes it -> v. A Step row by option name or by its number (index = number - lo: a row of
// consecutive integers gives its first number as lo, any other gives 0, horde_engine.h); a dB row also takes "-inf",
// its true off (v = 0); every other row a number in its unit, inside its range.
bool v_of(const horde::ParamInfo& p, const ValueView& value, double& v, std::string& why) {
  if (p.law == horde::Law::Step) {
    double index = -1;
    if (value.isString()) {
      const std::string name(value.getString());
      const auto names = options_of(p);
      for (std::size_t i = 0; i < names.size(); ++i)
        if (names[i] == name) index = double(i);
      if (index < 0) { why = "no option \"" + name + "\""; return false; }
    } else {
      double x;
      if (!number_of(value, x) || x != std::floor(x)) { why = "a Step row takes an option name or a whole number"; return false; }
      index = x - p.lo;
      if (index < 0 || index > double(p.steps) - 1) { why = "outside the row's numbers"; return false; }
    }
    v = p.steps > 1 ? index / double(p.steps - 1) : 0;
    return true;
  }
  if (p.law == horde::Law::Db && value.isString() && value.getString() == "-inf") { v = 0; return true; }
  double x;
  if (!number_of(value, x)) { why = "not a number"; return false; }
  const double lo = p.law == horde::Law::LogOffZone ? 0 : p.lo;
  if (p.law != horde::Law::LinWraps && (x < lo || x > p.hi)) { why = "outside " + std::to_string(lo) + " to " + std::to_string(p.hi); return false; }
  v = horde::v_of_value(p, x);
  if (!std::isfinite(v) || v < 0 || v > 1) { why = "no v for it"; return false; }
  return true;
}

// D.2's settings as the file names them: the rows' keys, a value in the row's unit; law by name, count as 2 or 4.
bool setting_v(int i, const ValueView& value, double& v, std::string& why) {
  const horde::ParamInfo& row = morph::setting_row(i);
  const std::string key = row.key;
  double index = -1;
  if (key == "morph.on" || key == "morph.audible") { why = "set by the loader, not the file"; return false; }
  if (key == "morph.law") {
    const char* names[] = {"Blend", "Quantum", "Stepped"};
    for (int k = 0; k < 3; ++k)
      if (value.isString() && value.getString() == names[k]) index = k;
    if (index < 0) { why = "Blend, Quantum or Stepped"; return false; }
  } else if (key == "morph.count") {
    double n;
    if (number_of(value, n) && (n == 2 || n == 4)) index = n == 2 ? 0 : 1;
    else { why = "2 or 4"; return false; }
  } else if (row.law == horde::Law::Step) {
    double x;
    if (value.isBool()) x = value.getBool() ? 1 : 0;
    else if (!number_of(value, x) || x != std::floor(x)) { why = "a whole number"; return false; }
    index = x - row.lo;
    if (index < 0 || index > double(row.steps) - 1) { why = "outside the row"; return false; }
  }
  if (index >= 0) {
    v = index / double(row.steps - 1);
    return true;
  }
  double x;
  if (!number_of(value, x) || x < row.lo || x > row.hi) { why = "outside the row"; return false; }
  v = horde::v_of_value(row, x);
  return std::isfinite(v) && v >= 0 && v <= 1;
}

std::uint64_t refusals(const horde::Counters& c) {
  return c.refused_nonfinite + c.clamped + c.refused_unknown_option + c.refused_param_id + c.refused_ratios;
}

// The rows of one object onto the engine: Param events, and a ratios text through set_ratios.
bool apply(horde::Engine& e, const ValueView& rows, bool corner, std::string& where, std::string& error) {
  if (!rows.isObject()) { error = where + ": not an object"; return false; }
  std::vector<horde::Event> events;
  for (std::uint32_t i = 0; i < rows.size(); ++i) {
    const auto member = rows.getObjectMemberAt(i);
    const std::string key = member.name;
    if (key == "osc1.ratios" || key == "osc2.ratios") {
      if (!member.value.isString() || !e.set_ratios(key == "osc1.ratios" ? 0 : 1, member.value.getString())) {
        error = where + ": " + key + " is not a ratios text the engine takes";
        return false;
      }
      continue;
    }
    const horde::ParamInfo* p = horde::find_param(key);
    if (!p) { error = where + ": no row " + key; return false; }
    if (p->reserved) { error = where + ": " + key + " is reserved"; return false; }
    if (corner && p->cls == horde::ParamClass::Device) { error = where + ": " + key + " is a Device row, which never morphs: set it in common"; return false; }
    std::string why;
    horde::Event ev;
    ev.type = horde::EventType::Param;
    ev.param_id = p->id;
    if (!v_of(*p, member.value, ev.value, why)) { error = where + ": " + key + ": " + why; return false; }
    events.push_back(ev);
  }
  const std::uint64_t before = refusals(e.counters());
  e.flush(events.data(), events.size());
  if (refusals(e.counters()) != before) { error = where + ": the engine refused or clamped a value"; return false; }
  return true;
}

}  // namespace

bool read(std::string_view json, const std::vector<horde::BankPreset>& factory, std::vector<Demo>& out, std::string& error) {
  out.clear();
  choc::value::Value doc;
  try {
    doc = choc::json::parse(json);
  } catch (const std::exception& x) {
    error = std::string("demo_bank.json does not parse: ") + x.what();
    return false;
  }
  if (!doc.isObject() || !doc.hasObjectMember("presets") || !doc["presets"].isArray()) { error = "no presets array"; return false; }
  const ValueView presets = doc["presets"];
  for (std::uint32_t n = 0; n < presets.size(); ++n) {
    const ValueView p = presets[n];
    Demo d;
    if (!p.isObject() || !p.hasObjectMember("name") || !p["name"].isString()) { error = "preset " + std::to_string(n) + ": no name"; return false; }
    d.name = std::string(p["name"].getString());
    for (const Demo& other : out)
      if (other.name == d.name) { error = d.name + ": named twice"; return false; }
    for (const auto& f : factory)
      if (f.name == d.name) { error = d.name + ": a factory preset has this name"; return false; }
    if (p.hasObjectMember("description") && p["description"].isString()) d.description = std::string(p["description"].getString());
    double x;
    if (p.hasObjectMember("render") && p["render"].isObject()) {
      const ValueView r = p["render"];
      if (r.hasObjectMember("single") && number_of(r["single"], x)) d.single_key = int(x);
      if (r.hasObjectMember("retrigger") && number_of(r["retrigger"], x)) d.retrigger_s = x;
      if (r.hasObjectMember("bass") && r["bass"].isBool()) d.bass = r["bass"].getBool();
    }
    if (p.hasObjectMember("glide") && number_of(p["glide"], x)) d.glide_s = x;
    if (p.hasObjectMember("glideAlways") && p["glideAlways"].isBool()) d.glide_always = p["glideAlways"].getBool();

    // D.2: the morph on, and every setting the file names; the rest stay at D.2's defaults.
    d.settings.emplace_back("morph.on", 1.0);
    if (p.hasObjectMember("morph")) {
      const ValueView m = p["morph"];
      if (!m.isObject()) { error = d.name + ": morph is not an object"; return false; }
      for (std::uint32_t i = 0; i < m.size(); ++i) {
        const auto member = m.getObjectMemberAt(i);
        const int s = morph::find_setting(member.name);
        std::string why;
        double v = 0;
        if (s < 0) { error = d.name + ": no morph setting " + std::string(member.name); return false; }
        if (!setting_v(s, member.value, v, why)) { error = d.name + ": " + std::string(member.name) + ": " + why; return false; }
        d.settings.emplace_back(member.name, v);
      }
    }
    if (p.hasObjectMember("modes")) {
      const ValueView m = p["modes"];
      for (std::uint32_t i = 0; m.isObject() && i < m.size(); ++i) {
        const auto member = m.getObjectMemberAt(i);
        const std::string key = member.name;
        horde::KeyMode mode;
        if (!member.value.isString() || !morph::mode_of(member.value.getString(), mode)) { error = d.name + ": " + key + ": follow, quantum or exempt"; return false; }
        const horde::ParamInfo* row = horde::find_param(key);
        if (key != "osc1.ratios" && key != "osc2.ratios" && (!row || row->reserved || row->cls == horde::ParamClass::Device)) {
          error = d.name + ": " + key + " takes no mode";
          return false;
        }
        d.modes.push_back({key, std::string(member.value.getString()), 0});
      }
    }

    // B.1's routes, only in the forms B.1 makes (horde_engine.h): ENV 2 or 3 to a blade's cut, cutHz or width.
    if (p.hasObjectMember("routes")) {
      const ValueView rs = p["routes"];
      for (std::uint32_t i = 0; rs.isArray() && i < rs.size(); ++i) {
        const ValueView r = rs[i];
        Route route;
        double env = 0;
        if (!r.isObject() || !r.hasObjectMember("env") || !r.hasObjectMember("key") || !r.hasObjectMember("octaves") || !number_of(r["env"], env) || (env != 2 && env != 3) || !r["key"].isString() || !number_of(r["octaves"], route.octaves)) {
          error = d.name + ": route " + std::to_string(i) + " is not {env 2 or 3, key, octaves}";
          return false;
        }
        route.env = int(env);
        route.key = std::string(r["key"].getString());
        const auto ends = [&](const std::string& tail) { return route.key.size() > tail.size() && route.key.compare(route.key.size() - tail.size(), tail.size(), tail) == 0; };
        const bool blade = route.key.find(".blade1.") != std::string::npos || route.key.find(".blade2.") != std::string::npos;
        if (!horde::find_param(route.key) || !blade || !(ends(".cut") || ends(".cutHz") || ends(".width"))) {
          error = d.name + ": route to " + route.key + ": B.1 routes reach only a blade's cut, cutHz or width";
          return false;
        }
        d.routes.push_back(route);
      }
    }

    const horde::BankPreset* base = nullptr;
    if (p.hasObjectMember("base") && p["base"].isString()) {
      for (const auto& f : factory)
        if (f.name == p["base"].getString()) base = &f;
      if (!base) { error = d.name + ": no factory preset " + std::string(p["base"].getString()); return false; }
    }
    if (!p.hasObjectMember("corners") || !p["corners"].isArray() || p["corners"].size() != 4) { error = d.name + ": corners is not four objects"; return false; }
    const bool level_in_common = p.hasObjectMember("common") && p["common"].isObject() && p["common"].hasObjectMember("out.level");
    for (int c = 0; c < 4; ++c) {
      std::string where = d.name + " corner " + "ABCD"[c];
      const ValueView corner = p["corners"][std::uint32_t(c)];
      if (!level_in_common && !(corner.isObject() && corner.hasObjectMember("out.level"))) { error = where + ": no out.level (demo_bank.h)"; return false; }
      auto e = horde::make_engine();
      if (!e || !e->prepare(44100, 64)) { error = "no engine"; return false; }
      if (base) {
        const horde::LoadReport r = e->load_state(base->state);
        if (!r.ok) { error = where + ": the base doesn't load: " + r.error; return false; }
      }
      if (p.hasObjectMember("common")) {
        std::string common = d.name + " common";
        if (!apply(*e, p["common"], false, common, error)) return false;
      }
      if (!apply(*e, corner, true, where, error)) return false;
      d.corners[c] = e->save_state();
    }
    out.push_back(std::move(d));
  }
  return true;
}

}  // namespace preview::demo
