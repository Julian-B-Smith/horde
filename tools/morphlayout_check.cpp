/* morphlayout_check — the morph field's array layout is APPEND-ONLY (ADR-159).
   T1  the first 222 entries of morphIds are the frozen 2026-08-22 order, and
       the late per-osc rows (181/1181) come LAST.
   T2  a 222-entry layout-1 corner (saved 2026-08-22..08-31) lands each value on
       its own id — bendTime's slot reads back as id 107.
   T3  a 224-entry layout-1 corner (saved 2026-08-31..09-11 with 181 inside the
       prefix) is remapped: the value written at the legacy slot of 107 reads
       back as 107, and the legacy 181 slot reads back as 181.
   T4  a layout-2 array is identity (round trip).
   T5  CONTROL (must read wrong without the remap): the T3 array read as
       layout 2 puts bendTime's value on the wrong id — proves T3 exercised
       the remap and not a coincidence of equal values.
   Standalone; not wired into ./verify (human gate). Exit 1 on failure. */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"
extern "C" bool hypersaw_debug_apply(const clap_plugin_t *, const char *);
extern "C" const char *hypersaw_debug_cornervals(const clap_plugin_t *, int);
namespace {
#include "notefuzz_scaffold.inc"
const char *kFrozen = "1,1001,2,1002,3,1003,4,1004,5,1005,6,1006,7,1007,8,1008,9,1009,10,1010,12,1012,13,1013,14,1014,16,1016,17,1017,18,1018,19,1019,20,1020,21,1021,22,1022,23,1023,24,1024,25,1025,26,1026,27,1027,28,1028,29,1029,30,1030,31,1031,35,1035,36,1036,37,1037,39,1039,42,1042,43,1043,44,1044,45,1045,46,1046,47,1047,48,1048,49,1049,50,1050,51,1051,52,1052,53,1053,54,1054,55,1055,56,1056,65,1065,66,1066,67,1067,68,1068,69,1069,71,1071,72,1072,73,1073,74,1074,76,1076,77,1077,78,1078,79,1079,80,1080,81,1081,82,1082,83,1083,84,1084,85,1085,86,1086,87,1087,91,1091,92,1092,93,1093,94,1094,95,1095,104,1104,105,1105,129,1129,130,1130,131,1131,132,1132,150,1150,57,58,59,60,61,62,63,64,96,97,98,99,133,134,135,136,33,106,107,108,109,110,111,112,113,114,115,137,138,139,140,141,142,143,144,145,146,147,148,149,11,70,32,34,38,90,75,116,117,118,119,120,121,122,123,124,125,126,127,128";
struct Rig {
  const clap_plugin_t *p = nullptr;
  void boot() { auto *f = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    p = f->create_plugin(f, &kHost, "com.lifted-truck.hypersaw"); p->init(p); p->activate(p, kSR, 32, kBlock); }
  void kill() { p->deactivate(p); p->destroy(p); }
};
std::vector<std::string> split(const std::string &s) { std::vector<std::string> v; std::stringstream ss(s); std::string t; while (std::getline(ss, t, ',')) v.push_back(t); return v; }
// live order from the id-keyed corner JSON: {"1":v,"1001":v,...}
std::vector<std::string> liveOrder(const char *j) { std::vector<std::string> v; for (const char *c = j; (c = std::strchr(c, '"')); ) { const char *e = std::strchr(c + 1, '"'); v.push_back(std::string(c + 1, e)); c = e + 1; if (*c == ':') { c = std::strchr(c, ','); if (!c) break; } } return v; }
double valueOf(const char *j, const char *id) { std::string key = std::string("\"") + id + "\":"; const char *c = std::strstr(j, key.c_str()); return c ? std::atof(c + key.size()) : NAN; }
std::string cornersJson(int layout, const std::vector<double> &arr) { std::string s = "{\"schema\":3,\"params\":{}"; if (layout) s += ",\"morphLayout\":" + std::to_string(layout); s += ",\"morphCorners\":[";
  for (int k = 0; k < 4; k++) { s += k ? ",[" : "["; for (size_t i = 0; i < arr.size(); i++) { char b[32]; std::snprintf(b, sizeof b, i ? ",%.6g" : "%.6g", arr[i]); s += b; } s += "]"; } return s + "]}"; }
int fails = 0; void expect(bool ok, const char *w) { std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", w); if (!ok) fails++; }
}
int main() {
  Rig r; r.boot();
  const std::vector<std::string> frozen = split(kFrozen);
  const std::vector<std::string> live = liveOrder(hypersaw_debug_cornervals(r.p, 0));
  bool prefixOk = live.size() == frozen.size() + 2;
  for (size_t i = 0; prefixOk && i < frozen.size(); i++) prefixOk = live[i] == frozen[i];
  expect(prefixOk && live[frozen.size()] == "181" && live[frozen.size() + 1] == "1181", "T1 frozen 222-entry prefix, then 181/1181 last");
  auto idx = [&](const std::vector<std::string> &o, const char *id) { return (size_t)(std::find(o.begin(), o.end(), id) - o.begin()); };
  // T2: 222-entry layout-1 array, 120.5 at the frozen slot of 107
  { std::vector<double> a(222, 0.0); a[idx(frozen, "107")] = 120.5; a[idx(frozen, "4")] = 0.31;
    hypersaw_debug_apply(r.p, cornersJson(0, a).c_str()); const char *c = hypersaw_debug_cornervals(r.p, 0);
    expect(valueOf(c, "107") == 120.5 && valueOf(c, "4") == 0.31, "T2 222-entry layout-1 corner: bendTime slot reads as id 107"); }
  // legacy 224 order: frozen prefix with 181/1181 inserted right after 1150 (table order), then the tail
  std::vector<std::string> legacy; for (auto &s : frozen) { legacy.push_back(s); if (s == "1150") { legacy.push_back("181"); legacy.push_back("1181"); } }
  { std::vector<double> a(224, 0.0); a[idx(legacy, "107")] = 120.5; a[idx(legacy, "181")] = 7.25; a[idx(legacy, "115")] = 8.5;
    hypersaw_debug_apply(r.p, cornersJson(0, a).c_str()); const char *c = hypersaw_debug_cornervals(r.p, 0);
    expect(valueOf(c, "107") == 120.5 && valueOf(c, "181") == 7.25 && valueOf(c, "115") == 8.5, "T3 224-entry layout-1 (ADR-150 order) corner is remapped by id");
    hypersaw_debug_apply(r.p, cornersJson(2, a).c_str()); c = hypersaw_debug_cornervals(r.p, 0);
    expect(valueOf(c, "107") != 120.5 && valueOf(c, "109") == 120.5, "T5 control: the same array read as layout 2 lands bendTime's value on bendTau (109) — the two-slot shift the remap undoes"); }
  { std::vector<double> a(224, 0.0); a[idx(live, "107")] = 121.5; a[idx(live, "1181")] = -3.5;
    hypersaw_debug_apply(r.p, cornersJson(2, a).c_str()); const char *c = hypersaw_debug_cornervals(r.p, 0);
    expect(valueOf(c, "107") == 121.5 && valueOf(c, "1181") == -3.5, "T4 layout-2 array is identity"); }
  std::printf("morphlayout_check: %s\n", fails ? "FAIL" : "PASS"); r.kill(); hypersaw_entry_deinit(); return fails ? 1 : 0;
}
