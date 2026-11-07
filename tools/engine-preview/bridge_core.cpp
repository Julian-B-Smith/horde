// GUI 3's bridge, its pure half (B628). See bridge_core.h for what it owes and section 3 of
// docs/proposals/gui-on-engine.md for the schema both sides signed.
#include "bridge_core.h"

#include <CommonCrypto/CommonDigest.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "choc/text/choc_JSON.h"

namespace preview::bridge {

namespace {

const char* class_name(horde::ParamClass c) {
  switch (c) {
    case horde::ParamClass::Morphable: return "morphable";
    case horde::ParamClass::Structural: return "structural";
    case horde::ParamClass::Device: return "device";
  }
  return "?";
}

const char* law_name(horde::Law l) {
  switch (l) {
    case horde::Law::Lin: return "lin";
    case horde::Law::Log: return "log";
    case horde::Law::Pow: return "pow";
    case horde::Law::LinWraps: return "linwraps";
    case horde::Law::LogOffZone: return "logoffzone";
    case horde::Law::Db: return "db";
    case horde::Law::Step: return "step";
  }
  return "?";
}

const char* stage_name(horde::EnvStage s) {
  switch (s) {
    case horde::EnvStage::Idle: return "idle";
    case horde::EnvStage::Attack: return "attack";
    case horde::EnvStage::Hold: return "hold";
    case horde::EnvStage::Decay: return "decay";
    case horde::EnvStage::Sustain: return "sustain";
    case horde::EnvStage::Release: return "release";
  }
  return "idle";
}

std::string g17(double x) {
  char b[40];
  std::snprintf(b, sizeof b, "%.17g", x);
  return b;
}

// Snapshot numbers: shortest text that reads back to the same double, so a v the page sent comes back equal (device.js
// compares with ===). Non-finite has no JSON form; it never reaches here from the engine's frame, and reads as null.
void put_num(std::string& out, double x) {
  if (!std::isfinite(x)) { out += "null"; return; }
  char b[40];
  for (int digits = 15; digits <= 17; ++digits) {
    std::snprintf(b, sizeof b, "%.*g", digits, x);
    if (std::strtod(b, nullptr) == x) break;
  }
  out += b;
}
// A float32 from the GUI frame: seven digits hold all it has.
void put_float(std::string& out, float x) {
  if (!std::isfinite(x)) { out += "null"; return; }
  char b[24];
  std::snprintf(b, sizeof b, "%.7g", double(x));
  out += b;
}

void put_strings(std::string& out, const std::vector<std::string>& list) {
  out += '[';
  for (std::size_t i = 0; i < list.size(); ++i) {
    if (i) out += ',';
    out += quoted(list[i]);
  }
  out += ']';
}

bool number_of(const choc::value::ValueView& v, double& out) {
  if (!v.isInt() && !v.isFloat()) return false;
  out = v.get<double>();
  return std::isfinite(out);
}

}  // namespace

std::string quoted(std::string_view s) {
  std::string out = "\"";
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
    else if (c < 0x20) {
      char b[8];
      std::snprintf(b, sizeof b, "\\u%04x", c);
      out += b;
    } else {
      out += char(c);
    }
  }
  return out + "\"";
}

// ---- the manifest hash ------------------------------------------------------------------------------------------
std::string manifest_line(const horde::ParamInfo& p) {
  return std::string(p.key) + '\t' + class_name(p.cls) + '\t' + law_name(p.law) + '\t' + g17(p.lo) + '\t' + g17(p.hi) + '\t' +
         std::to_string(p.steps) + '\t' + g17(p.default_value) + '\n';
}

std::string manifest_text(const std::vector<const horde::ParamInfo*>& rows) {
  std::vector<const horde::ParamInfo*> kept;
  for (const auto* p : rows)
    if (!p->reserved) kept.push_back(p);
  // Bytewise, as the page's sort must be too: std::strcmp compares unsigned bytes, never the locale's collation.
  std::sort(kept.begin(), kept.end(), [](const auto* a, const auto* b) { return std::strcmp(a->key, b->key) < 0; });
  std::string text;
  for (const auto* p : kept) text += manifest_line(*p);
  return text;
}

std::string sha256_hex(std::string_view bytes) {
  // CommonCrypto is part of the system on macOS, the only platform this plug-in builds for: no new dependency.
  unsigned char digest[CC_SHA256_DIGEST_LENGTH];
  CC_SHA256(bytes.data(), CC_LONG(bytes.size()), digest);
  static const char* const kHex = "0123456789abcdef";
  std::string out;
  for (unsigned char d : digest) {
    out += kHex[d >> 4];
    out += kHex[d & 15];
  }
  return out;
}

std::string manifest_hash(const std::vector<const horde::ParamInfo*>& rows) { return sha256_hex(manifest_text(rows)); }

KeyMap::KeyMap(std::vector<const horde::ParamInfo*> rows) : rows_(std::move(rows)) {
  by_key_.resize(rows_.size());
  for (std::size_t i = 0; i < rows_.size(); ++i) by_key_[i] = int(i);
  std::sort(by_key_.begin(), by_key_.end(), [&](int a, int b) { return std::strcmp(rows_[std::size_t(a)]->key, rows_[std::size_t(b)]->key) < 0; });
}

int KeyMap::find(std::string_view key) const {
  auto it = std::lower_bound(by_key_.begin(), by_key_.end(), key, [&](int i, std::string_view k) { return std::string_view(rows_[std::size_t(i)]->key) < k; });
  const int found = it != by_key_.end() && std::string_view(rows_[std::size_t(*it)]->key) == key ? *it : -1;
#ifdef HORDE_PREVIEW_PLANT_KEYMAP_SHIFT   // smoke.cpp's must-fail build for the mapping audit: every key lands one row off
  if (found >= 0) return (found + 1) % int(rows_.size());
#endif
  return found;
}

// ---- messages ---------------------------------------------------------------------------------------------------
Message parse_message(std::string_view json, const KeyMap& keys) {
  Message m;
  choc::value::Value doc;
  try {
    doc = choc::json::parse(json);
  } catch (const std::exception& e) {
    m.error = std::string("not JSON: ") + e.what();
    return m;
  }
  if (!doc.isObject() || !doc.hasObjectMember("type") || !doc["type"].isString()) {
    m.error = "no type";
    return m;
  }
  m.type = std::string(doc["type"].getString());
  double seq = 0;
  // Without a numeric seq there is nothing to acknowledge, so the message can't be answered; it is refused whole.
  if (!doc.hasObjectMember("seq") || !number_of(doc["seq"], seq) || seq != std::floor(seq)) {
    m.error = "no numeric seq";
    return m;
  }
  m.seq = std::int64_t(seq);

  if (m.type == "set") {
    m.kind = Kind::Set;
    if (doc.hasObjectMember("values") && doc["values"].isObject()) {
      const auto values = doc["values"];
      for (std::uint32_t i = 0; i < values.size(); ++i) {
        const auto member = values.getObjectMemberAt(i);
        const int row = keys.find(member.name);
        double v = 0;
        if (row < 0) m.unknown.emplace_back(member.name);
        else if (keys.row(row).reserved) m.reserved.emplace_back(member.name);
        else if (number_of(member.value, v)) m.values.push_back({row, v});
        else m.unknown.emplace_back(member.name);   // a value that isn't a finite number is as unusable as an unknown key
      }
    }
  } else if (m.type == "gesture") {
    m.kind = Kind::Gesture;
    if (doc.hasObjectMember("key") && doc["key"].isString()) m.gesture_key = std::string(doc["key"].getString());
    const std::string phase = doc.hasObjectMember("phase") && doc["phase"].isString() ? std::string(doc["phase"].getString()) : "";
    m.gesture_begin = phase == "begin";
    const int row = keys.find(m.gesture_key);
    // A gesture on a key the engine lacks (a rack or midi row, or a typo) reaches neither the engine nor the host.
    if (row >= 0 && !keys.row(row).reserved && (phase == "begin" || phase == "end")) m.gesture_row = row;
  } else if (m.type == "want") {
    m.kind = Kind::Want;
    if (doc.hasObjectMember("osc") && doc["osc"].isArray()) {
      m.want.osc[0] = m.want.osc[1] = false;
      const auto osc = doc["osc"];
      for (std::uint32_t i = 0; i < osc.size(); ++i) {
        double n = 0;
        if (number_of(osc[i], n) && (n == 1 || n == 2)) m.want.osc[int(n) - 1] = true;
      }
    }
    if (doc.hasObjectMember("members") && doc["members"].isBool()) m.want.members = doc["members"].getBool();
    if (doc.hasObjectMember("cycles") && doc["cycles"].isBool()) m.want.cycles = doc["cycles"].getBool();
    if (doc.hasObjectMember("summed") && doc["summed"].isBool()) m.want.summed = doc["summed"].getBool();
    if (doc.hasObjectMember("morph") && doc["morph"].isBool()) m.want.morph = doc["morph"].getBool();
  } else if (m.type == "morph") {
    // D.2's settings by key, in v, as the page's device.js sends them: only the keys that changed. A key D.2 lacks
    // (morph.x and morph.y included, until round 61 makes them rows) is reported, never guessed at.
    m.kind = Kind::Morph;
    if (doc.hasObjectMember("settings") && doc["settings"].isObject()) {
      const auto settings = doc["settings"];
      for (std::uint32_t i = 0; i < settings.size(); ++i) {
        const auto member = settings.getObjectMemberAt(i);
        const int at = morph::find_setting(member.name);
        double v = 0;
        if (at >= 0 && number_of(member.value, v)) m.settings.emplace_back(at, std::clamp(v, 0.0, 1.0));
        else m.unknown.emplace_back(member.name);
      }
    }
  } else if (m.type == "position") {
    m.kind = Kind::Position;
    double x = 0, y = 0;
    if (doc.hasObjectMember("x") && doc.hasObjectMember("y") && number_of(doc["x"], x) && number_of(doc["y"], y)) {
      m.has_position = true;
      m.x = std::clamp(x, 0.0, 1.0);   // PA-7's rule for host values: a value outside [0, 1] is clamped to it
      m.y = std::clamp(y, 0.0, 1.0);
    }
  } else if (m.type == "modes") {
    // The whole menu, every time (device.js sendModes): a key it doesn't name is Follow at As the morph.
    m.kind = Kind::Modes;
    const auto key_of = [&](std::string_view key, bool& reserved) {
      reserved = false;
      if (key == "osc1.ratios") return kRatiosKey[0];
      if (key == "osc2.ratios") return kRatiosKey[1];
      const int row = keys.find(key);
      if (row >= 0 && keys.row(row).reserved) reserved = true;
      return row;
    };
    const auto entry = [&](std::string_view key) -> Message::KeyMode* {
      bool reserved = false;
      const int k = key_of(key, reserved);
      if (k == -1 || reserved) {
        (reserved ? m.reserved : m.unknown).emplace_back(key);
        return nullptr;
      }
      for (auto& e : m.modes)
        if (e.key == k) return &e;
      m.modes.push_back({k, {}});
      return &m.modes.back();
    };
    if (doc.hasObjectMember("modes") && doc["modes"].isObject()) {
      const auto modes = doc["modes"];
      for (std::uint32_t i = 0; i < modes.size(); ++i) {
        const auto member = modes.getObjectMemberAt(i);
        horde::KeyMode mode;
        if (!member.value.isString() || !morph::mode_of(member.value.getString(), mode)) {
          m.unknown.emplace_back(member.name);   // a mode the menu doesn't have
          continue;
        }
        if (Message::KeyMode* e = entry(member.name)) e->choice.mode = mode;
      }
    }
    if (doc.hasObjectMember("counts") && doc["counts"].isObject()) {
      const auto counts = doc["counts"];
      for (std::uint32_t i = 0; i < counts.size(); ++i) {
        const auto member = counts.getObjectMemberAt(i);
        double n = 0;
        if (!number_of(member.value, n) || (n != 0 && n != 2 && n != 4)) {
          m.unknown.emplace_back(member.name);
          continue;
        }
        if (Message::KeyMode* e = entry(member.name)) e->choice.count = n == 2 ? horde::KeyCount::Two : n == 4 ? horde::KeyCount::Four : horde::KeyCount::AsMorph;
      }
    }
  } else if (m.type == "corner") {
    // Values written into one corner: the seam a corner edit needs (IF-12's corners are four arrays the morph system
    // holds; the page keeps its own today and sends none). Keys as `set`'s, by the same rules.
    m.kind = Kind::Corner;
    double n = -1;
    if (doc.hasObjectMember("index") && number_of(doc["index"], n) && n == std::floor(n) && n >= 0 && n <= 3) m.index = int(n);
    if (doc.hasObjectMember("values") && doc["values"].isObject()) {
      const auto values = doc["values"];
      for (std::uint32_t i = 0; i < values.size(); ++i) {
        const auto member = values.getObjectMemberAt(i);
        const int row = keys.find(member.name);
        double v = 0;
        if (row < 0) m.unknown.emplace_back(member.name);
        else if (keys.row(row).reserved) m.reserved.emplace_back(member.name);
        else if (number_of(member.value, v)) m.values.push_back({row, std::clamp(v, 0.0, 1.0)});
        else m.unknown.emplace_back(member.name);
      }
    }
  } else if (m.type == "save") {
    m.kind = Kind::Save;   // MO-25: what is heard now, into corner `index`
    double n = -1;
    if (doc.hasObjectMember("index") && number_of(doc["index"], n) && n == std::floor(n) && n >= 0 && n <= 3) m.index = int(n);
  } else if (m.type == "preset") {
    m.kind = Kind::Preset;
    double n = 0;
    if (doc.hasObjectMember("index") && number_of(doc["index"], n)) m.index = int(n);
  } else if (m.type == "plugin") {
    m.kind = Kind::Plugin;
    double n = 0;
    if (doc.hasObjectMember("voices") && number_of(doc["voices"], n)) m.voices = int(n);
  } else if (m.type == "hello") {
    m.kind = Kind::Hello;
    if (doc.hasObjectMember("manifest_hash") && doc["manifest_hash"].isString()) {
      m.has_hash = true;
      m.page_hash = std::string(doc["manifest_hash"].getString());
    }
    if (doc.hasObjectMember("manifest") && doc["manifest"].isString()) m.page_manifest = std::string(doc["manifest"].getString());
  } else {
    // The stub throws on these when nothing stands behind them; here they are acknowledged and dropped (section 3).
    m.kind = Kind::Held;
    m.unknown_type = !(m.type == "rack" || m.type == "midi" || m.type == "clear");
  }
  return m;
}

// ---- the mismatch banner ----------------------------------------------------------------------------------------
namespace {
// key -> line, from a manifest text.
std::vector<std::pair<std::string, std::string>> lines_by_key(std::string_view text) {
  std::vector<std::pair<std::string, std::string>> out;
  std::size_t at = 0;
  while (at < text.size()) {
    std::size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    const std::string_view line = text.substr(at, end - at);
    const std::size_t tab = line.find('\t');
    out.emplace_back(std::string(line.substr(0, tab)), std::string(line));
    at = end + 1;
  }
  std::sort(out.begin(), out.end());
  return out;
}
}  // namespace

Mismatch check_hello(const Message& hello, const std::string& engine_hash, const std::string& engine_text) {
  Mismatch m;
  if (!hello.has_hash || hello.page_hash == engine_hash) return m;   // a page that sends no hash isn't checked
  m.any = true;
  m.expected = engine_hash;
  m.got = hello.page_hash;
  if (hello.page_manifest.empty()) return m;
  const auto ours = lines_by_key(engine_text), theirs = lines_by_key(hello.page_manifest);
  std::size_t i = 0, j = 0;
  while (i < ours.size() || j < theirs.size()) {
    if (j == theirs.size() || (i < ours.size() && ours[i].first < theirs[j].first)) m.keys.push_back(ours[i++].first);
    else if (i == ours.size() || theirs[j].first < ours[i].first) m.keys.push_back(theirs[j++].first);
    else {
      if (ours[i].second != theirs[j].second) m.keys.push_back(ours[i].first);
      ++i, ++j;
    }
  }
  return m;
}

// ---- the saved state --------------------------------------------------------------------------------------------
const char* const kStateMagic1 = "horde-engine-preview 1";
const char* const kStateMagic2 = "horde-engine-preview 2";
const char* const kStateMagic3 = "horde-engine-preview 3";

std::string write_state(const SavedState& s) {
  // Version 2 whenever the morph holds nothing a load wouldn't rebuild: an untouched patch's chunk is unchanged.
  std::string text = std::string(s.morph.empty() ? kStateMagic2 : kStateMagic3) + "\n";
  text += "preset " + std::to_string(s.preset) + "\n";
  for (const auto& [id, v] : s.values) {
    text += std::to_string(id) + ' ';
    put_num(text, v);   // the shortest that reads back exactly: a state reloads to the same v
    text += '\n';
  }
  morph::write_lines(s.morph, text);
  return text;
}

bool read_state(std::string_view text, SavedState& out) {
  const std::size_t first = text.find('\n');
  const std::string_view magic = text.substr(0, first);
  // No promise across builds: an unknown chunk leaves the default.
  if (magic != kStateMagic1 && magic != kStateMagic2 && magic != kStateMagic3) return false;
  SavedState s;
  std::size_t at = first;
  while (at != std::string_view::npos && at + 1 < text.size()) {
    const std::size_t end = text.find('\n', at + 1);
    const std::string line(text.substr(at + 1, end == std::string_view::npos ? std::string_view::npos : end - at - 1));
    int n = 0;
    unsigned id = 0;
    double v = 0;
    if (morph::read_line(line, s.morph)) {}   // first: "morph corner 1 4097 0.5" would otherwise not read as a row only by luck
    else if (std::sscanf(line.c_str(), "preset %d", &n) == 1) s.preset = n;
    else if (std::sscanf(line.c_str(), "voices %d", &n) == 1) {}   // the retired Voices limit: read, ignored (no `continue`: `at` must move)
    else if (std::sscanf(line.c_str(), "%u %lf", &id, &v) == 2 && std::isfinite(v)) s.values.emplace_back(id, v);
    at = end;
  }
  out = std::move(s);
  return true;
}

std::vector<std::pair<std::uint32_t, double>> differences(const std::vector<std::uint32_t>& ids, const std::vector<double>& current,
                                                          const std::vector<double>& loaded) {
  std::vector<std::pair<std::uint32_t, double>> out;
  for (std::size_t i = 0; i < ids.size() && i < current.size() && i < loaded.size(); ++i)
    if (current[i] != loaded[i]) out.emplace_back(ids[i], current[i]);   // exact: the same v reloads to the same sound
  return out;
}

// ---- the polyphony cap -------------------------------------------------------------------------------------------
double capped_count_v(const horde::ParamInfo& row, double v, int cap) {
  if (row.steps < 2) return v;
  const double span = double(row.steps - 1);
  return row.lo + std::round(v * span) > cap ? (cap - row.lo) / span : v;
}

// ---- snapshots --------------------------------------------------------------------------------------------------
std::string observe_json(const FrameInputs& in, const Want& want) {
  const horde::GuiFrame* f = in.frame;
  // Section 3 rule 5: silence carries the last frame and "nothing sounding". While the engine is silent the plug-in
  // skips it, and the last frame it published may still show the tail's final voices.
  bool sounding = false;
  if (f && !in.engine_silent)
    for (const auto& v : f->voice) sounding = sounding || v.sounding;

  std::string out = "{\"sounding\":";
  out += sounding ? "true" : "false";
  out += ",\"voices\":[";
  bool first_voice = true;
  for (int i = 0; f && sounding && i < horde::kMaxVoices; ++i) {
    const horde::GuiVoice& v = f->voice[i];
    if (!v.sounding) continue;
    if (!first_voice) out += ',';
    first_voice = false;
    out += "{\"level\":";
    put_float(out, v.level);
    // RC-8 carries one stage per voice (the governing envelope's); the others aren't in the frame, and read as null.
    out += ",\"env\":[\"";
    out += stage_name(v.stage);
    out += "\",null,null,null],\"notes\":";
    const NoteSlot* n = in.notes ? &in.notes[i] : nullptr;
    if (n && n->active) {
      out += "{\"key\":" + std::to_string(n->key) + ",\"vel\":";
      put_num(out, n->velocity);
      out += ",\"gate\":";
      out += n->gate ? "true" : "false";
      out += '}';
    } else {
      out += "null";
    }
    out += ",\"osc\":[";
    for (int o = 0; o < 2; ++o) {
      if (o) out += ',';
      if (!want.osc[o]) { out += "null"; continue; }
      out += "{\"R\":";
      put_float(out, v.R[o]);
      out += ",\"psi\":";
      put_float(out, v.psi[o]);
      out += ",\"frame\":";
      put_float(out, v.frame[o]);
      if (want.members) {
        const int n_members = std::clamp(in.unison[o], 0, horde::kMaxMembers);
        out += ",\"phases\":[";
        for (int k = 0; k < n_members; ++k) {
          if (k) out += ',';
          put_float(out, v.member_phase[o][k]);
        }
        out += "],\"levels\":[";
        for (int k = 0; k < n_members; ++k) {
          if (k) out += ',';
          put_float(out, v.member_level[o][k]);
        }
        out += ']';
      }
      out += ",\"blades\":[";
      for (int b = 0; b < 2; ++b) {
        if (b) out += ',';
        out += "{\"pos\":";
        put_float(out, v.window_position[o][b]);
        out += ",\"width\":";
        put_float(out, v.window_width[o][b]);
        out += '}';
      }
      out += "]}";
    }
    out += "]}";
  }
  out += "],\"meters\":{";
  static const char* const kSources[4] = {"osc1", "osc2", "sub", "noise"};
  for (int s = 0; s < 4; ++s) {
    out += quoted(kSources[s]) + ":{\"peak\":";
    put_float(out, f ? f->meters.source[s].peak : 0.0f);
    out += ",\"clip\":";
    out += f && f->meters.source[s].clip ? "true" : "false";
    out += "},";
  }
  out += "\"out\":{\"peak\":";
  put_float(out, in.out_tapped ? in.out_peak : (f ? f->meters.output.peak : 0.0f));
  out += ",\"clip\":";
  out += f && f->meters.output.clip ? "true" : "false";
  out += "}},\"latches\":{\"clip\":";
  out += f && f->clip_latch ? "true" : "false";
  out += ",\"nonFinite\":";
  out += f && f->nonfinite_latch ? "true" : "false";
  // IF-9's operation counters, under the stub's names (engine-stub.js observe), so the page reads them unchanged.
  const horde::Counters c = f ? f->counters : horde::Counters{};
  out += "},\"counters\":{\"memberSamples\":" + std::to_string(c.member_samples) + ",\"edgesCorrected\":" + std::to_string(c.edges_corrected) +
         ",\"edgesLeftOver\":" + std::to_string(c.edges_left_over) + ",\"envelopeSteps\":" + std::to_string(c.envelope_steps) +
         ",\"voicesSounding\":" + std::to_string(sounding ? c.voices_sounding : 0u) + "}";
  if (want.summed && in.scope) {
    out += ",\"summed\":{\"rate\":";
    put_num(out, in.scope_rate);
    out += ",\"samples\":[";
    char b[24];
    for (int k = 0; k < in.scope_n; ++k) {
      // Nine digits: a float reads back exactly, so a test can hold the window to the output sample for sample.
      std::snprintf(b, sizeof b, "%s%.9g", k ? "," : "", std::isfinite(in.scope[k]) ? double(in.scope[k]) : 0.0);
      out += b;
    }
    out += "]}";
  }
  return out + "}";
}

std::string morph_json(const MorphFrame& m) {
  std::string out = "{\"x\":";
  put_num(out, m.x);
  out += ",\"y\":";
  put_num(out, m.y);
  out += ",\"dropped\":" + std::to_string(m.dropped) + ",\"unknown\":" + std::to_string(m.unknown);
  const auto pairs = [&](const std::vector<std::pair<std::string, double>>& list) {
    out += '{';
    for (std::size_t i = 0; i < list.size(); ++i) {
      if (i) out += ',';
      out += quoted(list[i].first) + ':';
      put_num(out, list[i].second);
    }
    out += '}';
  };
  if (m.settings) {
    out += ",\"settings\":{";
    for (int i = 0; i < morph::kSettings; ++i) {
      if (i) out += ',';
      out += quoted(morph::setting_row(i).key) + ':';
      put_num(out, m.settings[i]);
    }
    out += '}';
  }
  if (m.modes) {
    std::string modes, counts;
    for (const auto& [key, choice] : *m.modes) {
      if (choice.mode != horde::KeyMode::Follow) modes += (modes.empty() ? "" : ",") + quoted(key) + ":\"" + morph::mode_name(choice.mode) + '"';
      if (choice.count != horde::KeyCount::AsMorph) counts += (counts.empty() ? "" : ",") + quoted(key) + ':' + std::to_string(morph::count_number(choice.count));
    }
    out += ",\"modes\":{" + modes + "},\"counts\":{" + counts + '}';
  }
  if (m.corners[0] || m.corners[1] || m.corners[2] || m.corners[3]) {
    static const std::vector<std::pair<std::string, double>> kNone;
    out += ",\"corners\":[";
    for (int c = 0; c < 4; ++c) {
      if (c) out += ',';
      pairs(m.corners[c] ? *m.corners[c] : kNone);
    }
    out += ']';
  }
  if (m.ratios) {
    out += ",\"ratios\":[";
    for (int c = 0; c < 4; ++c) out += std::string(c ? "," : "") + '[' + quoted(m.ratios[c][0]) + ',' + quoted(m.ratios[c][1]) + ']';
    out += ']';
  }
  if (m.heard) {
    out += ",\"heard\":";
    pairs(*m.heard);
  }
  if (m.texts) out += ",\"texts\":[" + quoted(m.texts[0]) + ',' + quoted(m.texts[1]) + ']';
  if (m.sets) {
    out += ",\"sets\":[";
    for (std::size_t g = 0; g < m.sets->size(); ++g) {
      const HeardGroup& h = (*m.sets)[g];
      out += std::string(g ? "," : "") + "{\"group\":" + quoted(h.group ? h.group : "") + ",\"voice\":" + std::to_string(h.voice) + ",\"settings\":[";
      for (int k = 0; k < h.n; ++k) {
        out += std::string(k ? "," : "") + "{\"id\":" + std::to_string(h.id[k]) + ",\"target\":";
        put_num(out, h.target[k]);
        out += ",\"heard\":";
        put_num(out, h.heard[k]);
        out += '}';
      }
      out += "]}";
    }
    out += ']';
  }
  return out + '}';
}

std::string snapshot_json(const Pieces& p) {
  std::string out = "{\"ack\":" + std::to_string(p.ack) + ",\"observe\":" + (p.observe.empty() ? std::string("null") : p.observe);
  if (p.params) {
    out += ",\"params\":{";
    bool first = true;
    for (const auto& [key, v] : *p.params) {
      if (!first) out += ',';
      first = false;
      out += quoted(key) + ':';
      put_num(out, v);
    }
    out += '}';
  }
  if (!p.events.empty()) out += ",\"events\":" + p.events;
  if (p.preset_names) {
    out += ",\"presets\":{\"names\":";
    put_strings(out, *p.preset_names);
    out += ",\"index\":" + std::to_string(p.preset_index) + '}';
  }
  if (p.plugin_build) out += ",\"plugin\":{\"build\":" + quoted(*p.plugin_build) + '}';
  if (p.state) out += ",\"state\":" + quoted(*p.state);
  if (p.build || p.engine_hash) {
    out += ",\"hello\":{\"build\":" + quoted(p.build ? *p.build : "") + ",\"manifest_hash\":" + quoted(p.engine_hash ? *p.engine_hash : "") + '}';
  }
  if (p.mismatch) {
    if (p.mismatch->any) {
      out += ",\"mismatch\":{\"expected\":" + quoted(p.mismatch->expected) + ",\"got\":" + quoted(p.mismatch->got) + ",\"keys\":";
      put_strings(out, p.mismatch->keys);
      out += '}';
    } else {
      out += ",\"mismatch\":null";   // after a hello that matched: the page clears any banner
    }
  }
  if (p.refused_unknown || p.refused_reserved || p.refused_types) {
    static const std::vector<std::string> kNone;
    out += ",\"refused\":{\"unknown\":";
    put_strings(out, p.refused_unknown ? *p.refused_unknown : kNone);
    out += ",\"reserved\":";
    put_strings(out, p.refused_reserved ? *p.refused_reserved : kNone);
    out += ",\"types\":";
    put_strings(out, p.refused_types ? *p.refused_types : kNone);
    out += '}';
  }
  if (!p.morph.empty()) out += ",\"morph\":" + p.morph;   // last, and only when asked: every other member is unchanged
  return out + '}';
}

}  // namespace preview::bridge
