// The rack's descriptors for a page (ROADMAP B714). See descriptor.h.
#include "rack/descriptor.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace rack {

std::string key_name(int n, const std::string& address) { return "fx/" + std::to_string(n) + "/" + address; }
std::string switch_name(int n) { return "fx/" + std::to_string(n) + ".on"; }

std::string json_quoted(const std::string& s) {
  std::string out = "\"";
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
    else if (c < 0x20) {
      char b[8];
      std::snprintf(b, sizeof b, "\\u%04x", c);
      out += b;
    } else out += char(c);
  }
  return out + '"';
}

void json_number(std::string& out, double x) {
  if (!std::isfinite(x)) { out += "null"; return; }
  char b[40];
  for (int digits = 15; digits <= 17; ++digits) {
    std::snprintf(b, sizeof b, "%.*g", digits, x);
    if (std::strtod(b, nullptr) == x) break;
  }
  out += b;
}

namespace {
const char* cls_name(Cls c) { return c == Cls::Morph ? "morph" : c == Cls::Flip ? "flip" : "device"; }
void strings(std::string& out, const std::vector<std::string>& list) {
  out += '[';
  for (std::size_t i = 0; i < list.size(); ++i) out += (i ? "," : "") + json_quoted(list[i]);
  out += ']';
}
}  // namespace

std::string slots_json(const Rack& rack) {
  const SlotTable* t = rack.table();
  std::string out = "{\"ledger\":" + json_quoted(t ? t->ledger_hash : "") + ",\"refused\":";
  std::vector<std::string> refused = t ? t->refused : std::vector<std::string>{};
  refused.insert(refused.end(), rack.errors().begin(), rack.errors().end());
  strings(out, refused);
  out += ",\"slots\":[";
  for (int s = 0; t && s < rack.slots(); ++s) {
    const SlotRow& row = t->slots[std::size_t(s)];
    if (s) out += ',';
    out += "{\"slot\":" + std::to_string(row.n) + ",\"module\":" + json_quoted(row.module) + ",\"name\":" + json_quoted(row.name) +
           ",\"pin\":" + json_quoted(row.pin) + ",\"version\":" + json_quoted(row.module_version) + ",\"budget\":{\"bytes\":" +
           std::to_string(rack.effective_budget(s)) + ",\"status\":" + json_quoted(row.budget_status) + "},\"owns_io\":" +
           (row.owns_io ? "true" : "false") + ",\"usable\":" + (rack.usable(s) ? "true" : "false") + ",\"fixed\":[";
    for (std::size_t f = 0; f < row.fixed.size(); ++f)
      out += std::string(f ? "," : "") + "{\"label\":" + json_quoted(row.fixed[f].label) + ",\"key\":" + json_quoted(key_name(row.n, row.fixed[f].address)) + "}";
    out += "],\"on\":{\"key\":" + json_quoted(switch_name(row.n)) + ",\"id\":" + std::to_string(row.on_id) + "},\"keys\":[";
    for (std::size_t k = 0; k < row.keys.size(); ++k) {
      const KeyRow& key = row.keys[k];
      out += std::string(k ? "," : "") + "{\"key\":" + json_quoted(key_name(row.n, key.address)) + ",\"id\":" + std::to_string(key.host_id) +
             ",\"name\":" + json_quoted(row.name + " " + key.label) + ",\"cls\":\"" + cls_name(key.cls) + "\",\"law\":" + json_quoted(key.law_name) +
             ",\"mod\":" + (key.mod ? "true" : "false") + ",\"min\":";
      json_number(out, key.law.lo);
      out += ",\"max\":";
      json_number(out, key.law.hi);
      out += ",\"default\":";
      json_number(out, key.def);
      out += std::string(",\"stepped\":") + (stepped(key.law) ? "true" : "false");
      if (stepped(key.law)) {
        out += ",\"labels\":";
        strings(out, key.labels);
      }
      out += ",\"unit\":" + json_quoted(key.unit) + "}";
    }
    out += "]}";
  }
  return out + "]}";
}

std::string slot_meters_json(const Rack& rack) {
  const SlotTable* t = rack.table();
  std::string out = "[";
  for (int s = 0; t && s < rack.slots(); ++s) {
    const SlotMeters m = rack.meters(s);
    char b[200];
    std::snprintf(b, sizeof b, "%s{\"slot\":%d,\"in\":{\"peak\":%.7g,\"clip\":%s},\"out\":{\"peak\":%.7g,\"clip\":%s},\"nonfinite\":%llu}", s ? "," : "",
                  t->slots[std::size_t(s)].n, double(m.in_peak), m.in_clip ? "true" : "false", double(m.out_peak), m.out_clip ? "true" : "false",
                  (unsigned long long)m.nonfinite);
    out += b;
  }
  return out + "]";
}

}  // namespace rack
