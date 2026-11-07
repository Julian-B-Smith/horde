// The morph system's pure half (B636). See morph_core.h for what it owes and why D.2 is transcribed here.
#include "morph_core.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace preview::morph {

namespace {

using horde::Law;
using horde::ParamClass;
using horde::ParamInfo;
using horde::Rate;

ParamInfo row(const char* key, double lo, double hi, double def, Law law, std::uint32_t steps = 0) {
  return ParamInfo{0, key, lo, hi, def, law, ParamClass::Device, Rate::Control, false, steps, false};
}

// SPEC-ENGINE D.2, row by row. A Step row's default is its option index (horde_engine.h's convention). The page's
// shell-data.js carries the same rows; its v defaults (0.738 for temperature, 0.04 for glide, 0.10232 for the seed) are
// these rows' defaults run through the law, which bridge_test holds.
const ParamInfo kRows[kSettings] = {
    row("morph.on", 0, 1, 0, Law::Step, 2),                  // off, on
    row("morph.audible", 0, 3, 0, Law::Step, 4),             // A, B, C, D
    row("morph.law", 0, 2, 1, Law::Step, 3),                 // Blend, Quantum, Stepped: Quantum
    row("morph.count", 0, 1, 1, Law::Step, 2),               // Two, Four: Four
    row("morph.temperature", 0.02, 4, 1, Law::Log),
    row("morph.cohesion", 0, 1, 0.3, Law::Lin),
    row("morph.seed", 1, 9999, 1023, Law::Step, 9999),       // 1 to 9,999: the default 1024 is option 1023
    row("morph.stepRate", 0.25, 32, 4, Law::Log),
    row("morph.stepSync", 0, 1, 0, Law::Step, 2),
    row("morph.stepDivision", 0, 21, 3, Law::Step, 22),      // RC-7.4's table: 3 is 1/16
    row("morph.stepTriplet", 0, 1, 0, Law::Step, 2),
    row("morph.stepDotted", 0, 1, 0, Law::Step, 2),
    row("morph.glide", 0, 5, 0.008, Law::Pow),
};

std::string num(double x) {
  char b[40];
  for (int digits = 15; digits <= 17; ++digits) {
    std::snprintf(b, sizeof b, "%.*g", digits, x);
    if (std::strtod(b, nullptr) == x) break;
  }
  return b;
}

// A ratios text on one line: quotes and backslashes escaped, control bytes as \u00XX. Everything else is the text's own
// UTF-8, so a load gets back exactly the bytes saved.
std::string escaped(std::string_view s) {
  std::string out = "\"";
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
    else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; }
    else out += char(c);
  }
  return out + '"';
}
bool unescaped(std::string_view s, std::string& out) {
  if (s.size() < 2 || s.front() != '"' || s.back() != '"') return false;
  out.clear();
  for (std::size_t i = 1; i + 1 < s.size(); ++i) {
    if (s[i] != '\\') { out += s[i]; continue; }
    if (++i >= s.size() - 1) return false;   // a backslash right before the closing quote escapes nothing
    if (s[i] == 'u' && i + 4 < s.size() - 1) {
      out += char(std::strtol(std::string(s.substr(i + 1, 4)).c_str(), nullptr, 16));
      i += 4;
    } else {
      out += s[i];
    }
  }
  return true;
}

}  // namespace

const ParamInfo& setting_row(int i) { return kRows[i]; }

int find_setting(std::string_view key) {
  for (int i = 0; i < kSettings; ++i)
    if (key == kRows[i].key) return i;
  return -1;
}

double default_v(int i) {
  const ParamInfo& p = kRows[i];
  switch (p.law) {
    case Law::Step: return p.default_value / double(p.steps - 1);
    case Law::Log: return std::log(p.default_value / p.lo) / std::log(p.hi / p.lo);
    case Law::Pow: return std::sqrt((p.default_value - p.lo) / (p.hi - p.lo));
    default: return (p.default_value - p.lo) / (p.hi - p.lo);
  }
}

horde::MorphSettings decode(const double v[kSettings]) {
  const auto at = [&](int i) { return horde::value_of(kRows[i], v[i]); };   // a Step row: its option index
  horde::MorphSettings s;
  s.on = at(kOn) >= 1;
  s.audible = int(at(kAudible));
  s.law = horde::MorphLaw(int(at(kLaw)));   // the option order is the enum's: Blend, Quantum, Stepped
  s.count = at(kCount) >= 1 ? 4 : 2;
  s.temperature = at(kTemperature);
  s.cohesion = at(kCohesion);
  s.seed = int(at(kSeed)) + int(kRows[kSeed].lo);
  s.step_rate = at(kStepRate);
  s.step_sync = at(kStepSync) >= 1;
  s.step_division = int(at(kStepDivision));
  s.step_triplet = at(kStepTriplet) >= 1;
  s.step_dotted = at(kStepDotted) >= 1;
  s.glide = at(kGlide);
  return s;
}

bool mode_of(std::string_view name, horde::KeyMode& out) {
  if (name == "follow") out = horde::KeyMode::Follow;
  else if (name == "quantum") out = horde::KeyMode::Quantum;
  else if (name == "exempt") out = horde::KeyMode::Exempt;
  else return false;
  return true;
}

const char* mode_name(horde::KeyMode m) {
  switch (m) {
    case horde::KeyMode::Follow: return "follow";
    case horde::KeyMode::Quantum: return "quantum";
    case horde::KeyMode::Exempt: return "exempt";
  }
  return "follow";
}

int count_number(horde::KeyCount c) { return c == horde::KeyCount::Two ? 2 : c == horde::KeyCount::Four ? 4 : 0; }

void write_lines(const State& s, std::string& text) {
  for (const auto& [i, v] : s.settings) text += std::string("morph setting ") + kRows[i].key + ' ' + num(v) + '\n';
  if (s.x != 0 || s.y != 0) text += "morph position " + num(s.x) + ' ' + num(s.y) + '\n';
  for (const auto& c : s.corners) text += "morph corner " + std::to_string(c.corner) + ' ' + std::to_string(c.id) + ' ' + num(c.v) + '\n';
  for (const auto& t : s.texts) text += "morph ratios " + std::to_string(t.corner) + ' ' + std::to_string(t.osc) + ' ' + escaped(t.text) + '\n';
  for (const auto& m : s.modes)
    text += "morph mode " + std::to_string(m.id) + ' ' + mode_name(m.choice.mode) + ' ' + std::to_string(count_number(m.choice.count)) + '\n';
  for (const auto& p : s.waypoints) {
    text += "morph waypoints " + std::to_string(p.id) + ' ' + std::to_string(p.points.size());
    for (const auto& w : p.points) text += ' ' + num(w.x) + ' ' + num(w.y) + ' ' + num(w.v);
    text += '\n';
  }
  if (s.law != horde::WaypointLaw::Tps || !s.clamp)
    text += std::string("morph waypointlaw ") + (s.law == horde::WaypointLaw::Tin ? "tin" : "tps") + ' ' + (s.clamp ? '1' : '0') + '\n';
  for (const auto& l : s.locks)
    text += "morph lock " + std::to_string(l.id) + ' ' + num(l.offsets[0]) + ' ' + num(l.offsets[1]) + ' ' + num(l.offsets[2]) + '\n';
}

bool read_line(std::string_view line, State& s) {
  if (line.rfind("morph ", 0) != 0) return false;
  const std::string l(line);
  char key[64] = {}, mode[16] = {};
  int corner = 0, osc = 0, count = 0, used = 0;
  unsigned id = 0;
  double a = 0, b = 0, c3 = 0;
  if (std::sscanf(l.c_str(), "morph setting %63s %lf", key, &a) == 2) {
    const int i = find_setting(key);
    if (i >= 0 && std::isfinite(a)) s.settings.emplace_back(i, std::fmin(1.0, std::fmax(0.0, a)));
  } else if (std::sscanf(l.c_str(), "morph position %lf %lf", &a, &b) == 2) {
    if (std::isfinite(a) && std::isfinite(b)) s.x = std::fmin(1.0, std::fmax(0.0, a)), s.y = std::fmin(1.0, std::fmax(0.0, b));
  } else if (std::sscanf(l.c_str(), "morph corner %d %u %lf", &corner, &id, &a) == 3) {
    if (corner >= 0 && corner < 4 && std::isfinite(a)) s.corners.push_back({corner, id, a});
  } else if (std::sscanf(l.c_str(), "morph ratios %d %d %n", &corner, &osc, &used) == 2 && used > 0) {
    std::string text;
    if (corner >= 0 && corner < 4 && (osc == 0 || osc == 1) && unescaped(std::string_view(l).substr(std::size_t(used)), text))
      s.texts.push_back({corner, osc, text});
  } else if (std::sscanf(l.c_str(), "morph waypoints %u %d %n", &id, &count, &used) == 2 && used > 0) {
    // n triples follow; a line whose numbers don't all read is dropped whole, as MO-21 refuses a list whole
    State::Pins p{id, {}};
    const char* at = l.c_str() + used;
    bool ok = count >= 0 && count <= 1024;
    for (int i = 0; ok && i < count; ++i) {
      horde::Waypoint w;
      int n = 0;
      ok = std::sscanf(at, "%lf %lf %lf %n", &w.x, &w.y, &w.v, &n) == 3 && std::isfinite(w.x) && std::isfinite(w.y) && std::isfinite(w.v);
      at += n;
      p.points.push_back(w);
    }
    if (ok && !p.points.empty()) s.waypoints.push_back(std::move(p));
  } else if (std::sscanf(l.c_str(), "morph waypointlaw %15s %d", mode, &count) == 2) {
    s.law = std::string_view(mode) == "tin" ? horde::WaypointLaw::Tin : horde::WaypointLaw::Tps;
    s.clamp = count != 0;
  } else if (std::sscanf(l.c_str(), "morph lock %u %lf %lf %lf", &id, &a, &b, &c3) == 4) {
    if (std::isfinite(a) && std::isfinite(b) && std::isfinite(c3)) s.locks.push_back({id, {a, b, c3}});
  } else if (std::sscanf(l.c_str(), "morph mode %u %15s %d", &id, mode, &count) == 3) {
    KeyChoice c;
    if (mode_of(mode, c.mode)) {
      c.count = count == 2 ? horde::KeyCount::Two : count == 4 ? horde::KeyCount::Four : horde::KeyCount::AsMorph;
      s.modes.push_back({id, c});
    }
  }
  return true;   // a morph line this build can't read is still the morph's: never a row
}

}  // namespace preview::morph
