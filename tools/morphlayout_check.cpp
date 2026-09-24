/* morphlayout_check — the morph field's array layout is APPEND-ONLY (ADR-159).
   WIRED: ./verify full, beside the other state oracles (argv[1] = tests/morph_order.txt).
   The header claimed "not wired into ./verify" until 2026-09-20 and had been
   wired since B124 — the B190 class of stale relationship claim. Moved to the
   top by B240, whose T13 paragraph pushed it past test_table_check's 40 lines.
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
   T6-T9 the corner-preset FILE path, corner names, the short-array reset and
       the "a load with morph already on must not route into corners" rule.
   T10 (B195) every host-visible id in ADR-088's engine span (3000..9999) is in
       the morph field iff its class is not Device — driven from the shell's own
       parameter enumeration, so a new engine block inherits the coverage.
   T10b the Structural engine ids form one contiguous run at the TAIL of the
       order (B195/B203's layout-9 shape). RELABELLED BY B240: it was headed
       "no previously stored slot moved", which it cannot see — a Morphable row
       inserted before that run passes it. T13 is the gate on that claim.
   T10c CONTROL: the T10 scan run against a membership set that lies about one
       id must report exactly one violation.
   T11 a corner HOLDS a sub wave, and morphing between a sine corner and a bump
       corner keeps SUB Wave on an authored value at every pad position (a
       stepped member resolves atomically), calibrated by the same sweep
       driving the block's continuous SUB Level strictly between the corners.
   T12 (B203) the block's GATE is a corner value and it morphs as B48's LEVEL
       RAMP: the AUDIO moves continuously across a pad sweep while the gate's
       own VALUE only ever reads 0 or 1 (each half is the other's control), and
       a PURE CORNER is exact — bit-identical to every corner agreeing at the
       ON end, exactly silent at the OFF end.
   T13 (B240) THE WHOLE ORDER IS FROZEN. tests/morph_order.txt (argv[1]) holds
       every slot of the layout-9 order; it must be an EXACT PREFIX of the live
       order, so an insertion, removal or reorder anywhere fails and an append
       at the end passes. The marker must equal the fixture's `layout` when
       nothing is appended, and exceed it by exactly one when something is
       (one appending change, one bump; a second append waits for the first to
       be frozen into the file). No id may occupy two slots. T13e is the
       control: planted insertion, removal and swap must each be reported at
       the slot where they happen, and a planted append must not be.
   Exit 1 on failure. */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <clap/clap.h>
#include <cstdlib>
#include "../src/hypersaw_clap_entry.h"
#include "../src/hypersaw_debug.h"
namespace {
#include "notefuzz_scaffold.inc"
const char *kFrozen = "1,1001,2,1002,3,1003,4,1004,5,1005,6,1006,7,1007,8,1008,9,1009,10,1010,12,1012,13,1013,14,1014,16,1016,17,1017,18,1018,19,1019,20,1020,21,1021,22,1022,23,1023,24,1024,25,1025,26,1026,27,1027,28,1028,29,1029,30,1030,31,1031,35,1035,36,1036,37,1037,39,1039,42,1042,43,1043,44,1044,45,1045,46,1046,47,1047,48,1048,49,1049,50,1050,51,1051,52,1052,53,1053,54,1054,55,1055,56,1056,65,1065,66,1066,67,1067,68,1068,69,1069,71,1071,72,1072,73,1073,74,1074,76,1076,77,1077,78,1078,79,1079,80,1080,81,1081,82,1082,83,1083,84,1084,85,1085,86,1086,87,1087,91,1091,92,1092,93,1093,94,1094,95,1095,104,1104,105,1105,129,1129,130,1130,131,1131,132,1132,150,1150,57,58,59,60,61,62,63,64,96,97,98,99,133,134,135,136,33,106,107,108,109,110,111,112,113,114,115,137,138,139,140,141,142,143,144,145,146,147,148,149,11,70,32,34,38,90,75,116,117,118,119,120,121,122,123,124,125,126,127,128";
struct Rig {
  const clap_plugin_t *p = nullptr; std::vector<float> L, R; clap_audio_buffer_t out{}; clap_process_t proc{}; float *ch[2];
  void boot() { auto *f = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    p = f->create_plugin(f, &kHost, "com.lifted-truck.hypersaw"); p->init(p); p->activate(p, kSR, 32, kBlock); p->start_processing(p);
    L.assign(kBlock, 0); R.assign(kBlock, 0); ch[0] = L.data(); ch[1] = R.data(); out.data32 = ch; out.channel_count = 2;
    proc.frames_count = kBlock; proc.audio_outputs = &out; proc.audio_outputs_count = 1; proc.out_events = &kOut; }
  void run(int blocks) { for (int i = 0; i < blocks; i++) { EvList e; e.finalize(); proc.in_events = &e.list; p->process(p, &proc); } }
  /* T12 (B203) needs the AUDIO and not a parameter read: the gate's VALUE
     snaps by design — that is the whole point of B48's law — and the ramp
     lives in the gain, so a value-only reading could not tell a ramp from a
     snap. Left channel, appended block by block. */
  std::vector<float> capture(int blocks) { std::vector<float> a; for (int i = 0; i < blocks; i++) { EvList e; e.finalize(); proc.in_events = &e.list; p->process(p, &proc); a.insert(a.end(), L.begin(), L.end()); } return a; }
  void kill() { p->stop_processing(p); p->deactivate(p); p->destroy(p); }
};
std::vector<std::string> split(const std::string &s) { std::vector<std::string> v; std::stringstream ss(s); std::string t; while (std::getline(ss, t, ',')) v.push_back(t); return v; }
// live order from the id-keyed corner JSON: {"1":v,"1001":v,...}
std::vector<std::string> liveOrder(const char *j) { std::vector<std::string> v; for (const char *c = j; (c = std::strchr(c, '"')); ) { const char *e = std::strchr(c + 1, '"'); v.push_back(std::string(c + 1, e)); c = e + 1; if (*c == ':') { c = std::strchr(c, ','); if (!c) break; } } return v; }
double valueOf(const char *j, const char *id) { std::string key = std::string("\"") + id + "\":"; const char *c = std::strstr(j, key.c_str()); return c ? std::atof(c + key.size()) : NAN; }
std::string cornersJson(int layout, const std::vector<double> &arr) { std::string s = "{\"schema\":3,\"params\":{}"; if (layout) s += ",\"morphLayout\":" + std::to_string(layout); s += ",\"morphCorners\":[";
  for (int k = 0; k < 4; k++) { s += k ? ",[" : "["; for (size_t i = 0; i < arr.size(); i++) { char b[32]; std::snprintf(b, sizeof b, i ? ",%.6g" : "%.6g", arr[i]); s += b; } s += "]"; } return s + "]}"; }
int fails = 0; void expect(bool ok, const char *w) { std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", w); if (!ok) fails++; }

/* ---- T13 (B240): the frozen whole-order fixture ------------------------- */
struct OrderFixture { int layout = -1; std::vector<std::string> ids; };
// `#` comment lines, one `layout N` line, then one id per line. A file that
// cannot be read comes back empty and T13a fails on it — never skipped.
OrderFixture readOrderFixture(const char *path)
{
  OrderFixture f;
  FILE *fp = std::fopen(path, "r");
  if (!fp) return f;
  char line[512];
  while (std::fgets(line, sizeof line, fp))
  {
    std::string s(line);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    if (s.empty() || s[0] == '#') continue;
    if (s.rfind("layout ", 0) == 0) { f.layout = std::atoi(s.c_str() + 7); continue; }
    f.ids.push_back(s);
  }
  std::fclose(fp);
  return f;
}
/* The first slot at which `order` stops carrying `frozen` as its PREFIX, or
   SIZE_MAX when every frozen slot is where it was. One function for the live
   run and for T13e's planted mutations, so the control exercises the exact
   comparator the gate uses. */
size_t prefixBreak(const std::vector<std::string> &frozen, const std::vector<std::string> &order)
{
  for (size_t i = 0; i < frozen.size(); i++)
    if (i >= order.size() || order[i] != frozen[i]) return i;
  return SIZE_MAX;
}
// The marker rule (see the header): equal when nothing is appended, exactly
// one past the fixture when an append is pending, and nothing else.
bool markerOk(size_t liveLen, size_t frozenLen, int marker, int frozenLayout)
{
  if (liveLen == frozenLen) return marker == frozenLayout;
  if (liveLen > frozenLen) return marker == frozenLayout + 1;
  return false;
}
}
int main(int argc, char **argv) {
  Rig r; r.boot();
  const std::vector<std::string> frozen = split(kFrozen);
  const std::vector<std::string> live = liveOrder(hypersaw_debug_cornervals(r.p, 0));
  /* T1 is a PREFIX clause, not a length pin (human ruling 2026-09-17, B50 phase 1):
     the invariant ADR-159 states is the ORDER of what was already stored — the
     frozen 222, then 181/1181 — never the field's total length. Anything after
     that prefix must be a later append-only block (today: the routing cells,
     ids >= 10000), so a stray insertion still fails here. */
  bool prefixOk = live.size() >= frozen.size() + 2;
  for (size_t i = 0; prefixOk && i < frozen.size(); i++) prefixOk = live[i] == frozen[i];
  /* PIN MOVED BY B172, and it admits TWO bands now, not one. Until B172 the
     only appended block was the routing cells (ids >= 10000), so "everything
     after the prefix is >= 10000" was an accurate statement of the invariant.
     ADR-088 also reserves 3000..9999 for ENGINE blocks, and the SUB OSC's
     morphable rows (4000..4014) append after the routing block — STATION's
     3000-block lands in the same band (B162), which is why the bound is the
     band and not the one engine. The invariant is unchanged and still has
     teeth: a stray INSERTION lands inside the frozen prefix and fails above,
     and an appended id below 3000 (an instrument row that skipped
     buildMorphOrder) still fails here. */
  bool tailOk = true;
  for (size_t i = frozen.size() + 2; i < live.size(); i++)
  {
    const int id = std::atoi(live[i].c_str());
    tailOk = tailOk && id >= 3000;   // engine blocks 3000..9999, routing >= 10000
  }
  expect(prefixOk && live[frozen.size()] == "181" && live[frozen.size() + 1] == "1181", "T1 frozen 222-entry prefix, then 181/1181");
  expect(tailOk, "T1b everything after the ADR-159 prefix is an appended block (routing ids >= 10000 or an ADR-088 engine block, 3000..9999)");
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
  // T6: the corner-preset FILE path (corners/*.json) takes the same remaps.
  auto cornerFile = [&](int layout, const std::vector<double> &arr) { std::string s = "{"; if (layout) s += "\"morphLayout\":" + std::to_string(layout) + ","; s += "\"cornerPreset\":[";
    for (size_t i = 0; i < arr.size(); i++) { char b[32]; std::snprintf(b, sizeof b, i ? ",%.6g" : "%.6g", arr[i]); s += b; } return s + "]}"; };
  { std::vector<double> a(224, 0.0); a[idx(legacy, "107")] = 122.5; a[idx(legacy, "181")] = 6.5;
    hypersaw_debug_cornerapply(r.p, 1, cornerFile(0, a).c_str()); const char *c = hypersaw_debug_cornervals(r.p, 1);
    expect(valueOf(c, "107") == 122.5 && valueOf(c, "181") == 6.5, "T6a corner-preset file, 224-entry layout-1: remapped by id");
    std::vector<double> b(222, 0.0); b[idx(frozen, "107")] = 123.5;
    hypersaw_debug_cornerapply(r.p, 2, cornerFile(0, b).c_str()); c = hypersaw_debug_cornervals(r.p, 2);
    expect(valueOf(c, "107") == 123.5, "T6b corner-preset file, 222-entry layout-1: frozen prefix 1:1");
    hypersaw_debug_cornerapply(r.p, 3, cornerFile(2, a).c_str()); c = hypersaw_debug_cornervals(r.p, 3);
    expect(valueOf(c, "109") == 122.5, "T6c control: the 224 array read as layout 2 lands on 109"); }
  // T7 (B122): corner names ride the morph chunk and round-trip through a
  // patch; a patch without the key clears them; "matches" tells edited from clean.
  { hypersaw_debug_cornername(r.p, 1, "Squids \"the\" one"); hypersaw_debug_cornername(r.p, 3, "Deviltrap");
    std::string names = hypersaw_debug_cornernames(r.p);
    expect(names == "[\"\",\"Squids \\\"the\\\" one\",\"\",\"Deviltrap\"]", "T7a names set and escaped");
    // round-trip: a patch that carries the chunk restores the names
    std::vector<double> a(224, 0.0); a[idx(live, "107")] = 121.5;
    std::string patch = cornersJson(2, a); patch.insert(patch.size() - 1, ",\"cornerNames\":[\"x\",\"y \\\"q\\\"\",\"\",\"z\"]");
    hypersaw_debug_apply(r.p, patch.c_str());
    names = hypersaw_debug_cornernames(r.p);
    expect(names == "[\"x\",\"y \\\"q\\\"\",\"\",\"z\"]", "T7b names parsed back from a patch (escapes intact)");
    hypersaw_debug_apply(r.p, cornersJson(2, a).c_str());
    names = hypersaw_debug_cornernames(r.p);
    expect(names == "[\"\",\"\",\"\",\"\"]", "T7c a patch without the key clears every name");
    // matches: the corner-preset file that produced corner 0 matches; a nudged one does not
    std::vector<double> b(224, 0.0); b[idx(live, "107")] = 121.5;
    hypersaw_debug_cornerapply(r.p, 0, cornerFile(2, b).c_str());
    expect(hypersaw_debug_cornermatches(r.p, 0, cornerFile(2, b).c_str()), "T7d a corner freshly loaded from a preset MATCHES it");
    b[idx(live, "4")] = 0.31;
    expect(!hypersaw_debug_cornermatches(r.p, 0, cornerFile(2, b).c_str()), "T7e control: a preset differing in one value does NOT match (the asterisk fires)"); }
  // T8 (the 2026-09-14 "corner semi-permanently messed up" report): a corner
  // array SHORTER than the live layout must reset the slots it does not carry
  // to their defaults — it used to leave the previous load's values there, so
  // a pitch offset (181/1181) from one preset outlived every later load.
  { std::vector<double> a(224, 0.0); a[idx(live, "181")] = 7.25; a[idx(live, "1181")] = -3.0; a[idx(live, "107")] = 120;
    hypersaw_debug_cornerapply(r.p, 1, cornerFile(2, a).c_str());
    const char *c = hypersaw_debug_cornervals(r.p, 1);
    expect(valueOf(c, "181") == 7.25, "T8 anchor: a 224-entry corner sets oscPitch to 7.25");
    std::vector<double> b(222, 0.0); b[idx(frozen, "107")] = 121;
    hypersaw_debug_cornerapply(r.p, 1, cornerFile(0, b).c_str()); c = hypersaw_debug_cornervals(r.p, 1);
    expect(valueOf(c, "107") == 121 && valueOf(c, "181") == 0.0 && valueOf(c, "1181") == 0.0, "T8 a 222-entry corner preset RESETS the late slots (oscPitch back to its default 0)");
    // same through the patch path: a 224 patch with a pitch offset, then a 222-entry patch
    std::vector<double> pa(224, 0.0); pa[idx(live, "1181")] = 5.5; hypersaw_debug_apply(r.p, cornersJson(2, pa).c_str());
    std::vector<double> pb(222, 0.0); hypersaw_debug_apply(r.p, cornersJson(0, pb).c_str()); c = hypersaw_debug_cornervals(r.p, 0);
    expect(valueOf(c, "1181") == 0.0, "T8 a 222-entry PATCH resets the late corner slots too"); }
  /* T9 (B125, the "reverted corner becomes corner A" report): with morph
     ALREADY ON from the previous state (the DAW case — a fresh instance has
     morph off at drain time and never routes), a LOAD's parameter writes must
     not be routed into corners. Sequence: load patch 1 (morph on, corner B
     armed, pad on A) and drain; load patch 2 with different corners and
     drain; corner B must be patch 2's, not patch 2's live values (= its
     corner A). Control: quantum mode with the pad on B, nothing armed, where
     the winning corner was the victim. Both legs FAIL with the bypass line
     disabled — run as the control before this landed. */
  { auto patch = [&](double armed, double x, double y, int mode, double base) {
      std::vector<std::vector<double>> cs(4, std::vector<double>(224, 0.0));
      for (int k = 0; k < 4; k++) { cs[k][idx(live, "107")] = base + 10 * k; cs[k][idx(live, "4")] = 0.1 * (k + 1); }
      std::string s = "{\"schema\":3,\"params\":{\"morphOn\":1,\"morphArm\":" + std::to_string((int)armed) + ",\"morphX\":" + std::to_string(x) + ",\"morphY\":" + std::to_string(y) + ",\"morphMode\":" + std::to_string(mode) + ",\"bendTime\":" + std::to_string(base) + "},\"morphLayout\":2,\"morphCorners\":[";
      for (int k = 0; k < 4; k++) { s += k ? ",[" : "["; for (size_t i = 0; i < 224; i++) { char b[32]; std::snprintf(b, sizeof b, i ? ",%.6g" : "%.6g", cs[k][i]); s += b; } s += "]"; }
      return s + "]}"; };
    hypersaw_debug_apply(r.p, patch(2, 0, 0, 1, 100).c_str()); r.run(8);   // morph on, B armed, pad on A — the prior state
    hypersaw_debug_apply(r.p, patch(2, 0, 0, 1, 300).c_str()); r.run(8);   // the load under test
    const char *c = hypersaw_debug_cornervals(r.p, 1);
    expect(valueOf(c, "107") == 310, "T9 a load with morph already on and corner B armed leaves corner B as the patch says (310, not the live 300)");
    hypersaw_debug_apply(r.p, patch(0, 1, 0, 0, 500).c_str()); r.run(8);   // quantum, pad on B, nothing armed
    hypersaw_debug_apply(r.p, patch(0, 1, 0, 0, 700).c_str()); r.run(8);
    c = hypersaw_debug_cornervals(r.p, 1);
    expect(valueOf(c, "107") == 710, "T9 control: quantum mode, pad on B — the winning corner is not overwritten by the load's live value"); }

  /* ---- T10 / T11 (B195): the ENGINE BLOCKS' membership of the field -------
     Driven off the SHELL'S OWN ENUMERATION — every host-visible parameter in
     ADR-088's reserved engine span (3000..9999) — and never off a list written
     here. That is what makes STATION inherit this coverage the day its block
     joins kEngineBlocks: the span is the contract, the block is an accident of
     which engines exist today. */
  auto inField = [&](uint32_t id) {
    return std::find(live.begin(), live.end(), std::to_string(id)) != live.end();
  };
  {
    auto *px = (const clap_plugin_params_t *)r.p->get_extension(r.p, CLAP_EXT_PARAMS);
    int seen = 0, morphable = 0, structural = 0, device = 0, bad = 0;
    std::string report;
    for (uint32_t i = 0, n = px ? px->count(r.p) : 0; i < n; i++)
    {
      clap_param_info_t info{};
      if (!px->get_info(r.p, i, &info)) continue;
      if (info.id < 3000 || info.id >= 10000) continue;
      const char *key = nullptr, *why = nullptr;
      const int cls = hypersaw_debug_paramclass(info.id, &key, &why);   // 0 morphable, 1 structural, 2 device
      seen++;
      if (cls == 0) morphable++; else if (cls == 1) structural++; else if (cls == 2) device++;
      /* THE RULE (B195): Morphable AND Structural are corner values; Device is
         not. A stepped/structural member morphs ATOMICALLY, which is what the
         instrument table's own curated appends have always meant by including
         the bend laws and the FX slot types. */
      const bool want = cls == 0 || cls == 1;
      if (inField(info.id) != want)
      {
        bad++;
        report += std::string("\n       id ") + std::to_string(info.id) + " (" + (key ? key : "?") +
                  ", class " + std::to_string(cls) + ") " + (want ? "MISSING from" : "PRESENT in") +
                  " morphIds";
      }
    }
    /* THE ANCHOR, AND WHY ITS THIRD CLAUSE LEFT (B203). It read `device > 0`
       until the human overruled B172's gate ruling: the block's GATE was the
       ONLY Device id in ADR-088's whole engine span, and it is Structural now,
       so the clause asserts something that is no longer true of the product.
       Kept as a PRINTED count rather than deleted, so a future engine block
       that does class a row Device is visible here the day it lands; T10's
       Device clause is then vacuous and says so out loud rather than reading
       as coverage it does not have. The scan's teeth are T10c's control, which
       plants a lie and requires exactly one violation to be reported. */
    char m[190];
    std::snprintf(m, sizeof m,
                  "T10 anchor: the engine span is populated and spans the classes it has "
                  "(%d ids: %d morphable, %d structural, %d device — device is 0 since B203)",
                  seen, morphable, structural, device);
    expect(seen > 0 && morphable > 0 && structural > 0, m);
    if (bad) std::printf("     T10 violations:%s\n", report.c_str());
    expect(bad == 0, "T10 every non-Device engine-block id is in the morph field and every Device one "
                     "is not — by the class and not by a list (the block's GATE is Structural "
                     "since B203 and is therefore IN)");
    /* T10b THE STRUCTURAL ENGINE IDS ARE ONE RUN AT THE TAIL. They joined in a
       SECOND pass over kEngineBlocks, after every Morphable one, precisely so
       they land at the tail; widening the class test in place would have
       interleaved them in block order (4000 before 4001).
       WHAT THIS DOES NOT SEE (B240 — it was labelled "no previously stored
       slot moved"): a Morphable row landing BEFORE the run — a second block's
       pass-1 rows, or a new SUB row — leaves the run contiguous at the tail
       and passes here while shifting every stored slot after it. T13's
       whole-order fixture is the gate on that. Note also that any append
       after the gate (Plugin::kMorphTailIds) ends the run's tail position, so
       this row turns red at the first real append until its pin is ruled. */
    {
      size_t firstStructural = live.size();
      int atTail = 0;
      for (uint32_t i = 0, n = px ? px->count(r.p) : 0; i < n; i++)
      {
        clap_param_info_t info{};
        if (!px->get_info(r.p, i, &info)) continue;
        if (info.id < 3000 || info.id >= 10000) continue;
        const char *key = nullptr, *why = nullptr;
        if (hypersaw_debug_paramclass(info.id, &key, &why) != 1) continue;
        const size_t at = idx(live, std::to_string(info.id).c_str());
        if (at < live.size()) { firstStructural = std::min(firstStructural, at); atTail++; }
      }
      char m2[200];
      std::snprintf(m2, sizeof m2,
                    "T10b the %d Structural engine ids are one contiguous run at the TAIL (first at "
                    "slot %zu of %zu) — the layout-9 shape; insertion is T13's to see",
                    atTail, firstStructural, live.size());
      expect(atTail == structural && atTail > 0 && firstStructural + (size_t)atTail == live.size(), m2);
    }
    /* T10c THE CONTROL (L0032). The same scan, run against a membership set
       that LIES about ONE id, must REPORT the violation — otherwise a scan
       that silently examined nothing would pass exactly as loudly. */
    std::vector<std::string> lying = live;
    std::string dropped;
    for (uint32_t i = 0, n = px ? px->count(r.p) : 0; i < n && dropped.empty(); i++)
    {
      clap_param_info_t info{};
      if (!px->get_info(r.p, i, &info)) continue;
      if (info.id < 3000 || info.id >= 10000) continue;
      const char *key = nullptr, *why = nullptr;
      if (hypersaw_debug_paramclass(info.id, &key, &why) == 2) continue;
      const auto at = std::find(lying.begin(), lying.end(), std::to_string(info.id));
      if (at == lying.end()) continue;
      dropped = *at;
      lying.erase(at);
    }
    int badLying = 0;
    for (uint32_t i = 0, n = px ? px->count(r.p) : 0; i < n; i++)
    {
      clap_param_info_t info{};
      if (!px->get_info(r.p, i, &info)) continue;
      if (info.id < 3000 || info.id >= 10000) continue;
      const char *key = nullptr, *why = nullptr;
      const int cls = hypersaw_debug_paramclass(info.id, &key, &why);
      const bool want = cls == 0 || cls == 1;
      const bool has = std::find(lying.begin(), lying.end(), std::to_string(info.id)) != lying.end();
      if (has != want) badLying++;
    }
    const std::string cmsg = "T10c control: the same scan against a membership set missing id " +
                             (dropped.empty() ? std::string("<none found>") : dropped) +
                             " reports exactly one violation";
    expect(!dropped.empty() && badLying == 1, cmsg.c_str());
  }

  /* T11 — A CORNER CAN HOLD A SUB WAVE, AND THE MORPH SNAPS.
     The human's report was that the sub's stepped rows never reached the
     field; T10 says they are members, T11 says membership MEANS something.
     Fresh rig, because the sweep drives every corner slot and the rigs above
     are full of deliberately wrong arrays. */
  {
    Rig w; w.boot();
    const std::vector<std::string> wo = liveOrder(hypersaw_debug_cornervals(w.p, 0));
    const size_t iWave = idx(wo, "4000"), iLevel = idx(wo, "4007");
    expect(iWave < wo.size(),
           "T11a the sub's WAVE (id 4000, stepped) is a corner slot at all — RED before B195");
    if (iWave < wo.size() && iLevel < wo.size())
    {
      // A fresh instance's corners hold the per-slot DEFAULTS, so start from
      // what the shell itself reports and move only the two slots under test.
      std::vector<double> base(wo.size(), 0.0);
      { const char *c = hypersaw_debug_cornervals(w.p, 0);
        for (size_t i = 0; i < wo.size(); i++) base[i] = valueOf(c, wo[i].c_str()); }
      auto cornerArr = [&](double wave, double level) {
        std::vector<double> a = base; a[iWave] = wave; a[iLevel] = level; return a; };
      auto cornerFile2 = [&](const std::vector<double> &arr) {
        std::string s = "{\"morphLayout\":9,\"cornerPreset\":[";
        for (size_t i = 0; i < arr.size(); i++) { char b[32]; std::snprintf(b, sizeof b, i ? ",%.6g" : "%.6g", arr[i]); s += b; }
        return s + "]}"; };
      // x = 0 corners hold wave "sine" (0) at level 0.1; x = 1 corners hold
      // "bump" (6) at level 0.9. Both rows of the pad, so a Gumbel draw at any
      // y can only ever land on one of the two authored waves.
      hypersaw_debug_cornerapply(w.p, 0, cornerFile2(cornerArr(0, 0.1)).c_str());
      hypersaw_debug_cornerapply(w.p, 2, cornerFile2(cornerArr(0, 0.1)).c_str());
      hypersaw_debug_cornerapply(w.p, 1, cornerFile2(cornerArr(6, 0.9)).c_str());
      hypersaw_debug_cornerapply(w.p, 3, cornerFile2(cornerArr(6, 0.9)).c_str());
      auto *px = (const clap_plugin_params_t *)w.p->get_extension(w.p, CLAP_EXT_PARAMS);
      auto send = [&](std::initializer_list<std::pair<clap_id, double>> kv) {
        EvList e;
        for (const auto &q : kv) e.params.push_back(mkParam(q.first, q.second));
        e.finalize(); w.proc.in_events = &e.list; w.p->process(w.p, &w.proc); };
      // BLEND (morphMode 1) is the hard case on purpose: it is the one mode
      // that interpolates at all, and morphStep's blend branch is guarded by
      // `!d->stepped` — so a stepped member must still snap here.
      send({{157, 1}, {151, 1}, {153, 0.5}, {158, 0}});
      w.run(40);
      bool waveOk = true, sawSine = false, sawBump = false, sawBlend = false;
      double strayWave = -1;
      for (int stepI = 0; stepI <= 20; stepI++)
      {
        send({{152, stepI / 20.0}});
        w.run(20);
        double wv = -1, lv = -1;
        px->get_value(w.p, 4000, &wv); px->get_value(w.p, 4007, &lv);
        if (wv == 0) sawSine = true; else if (wv == 6) sawBump = true;
        else { waveOk = false; strayWave = wv; }
        if (lv > 0.1 + 1e-6 && lv < 0.9 - 1e-6) sawBlend = true;
      }
      char m[200];
      std::snprintf(m, sizeof m,
                    "T11b morphing between a sine corner and a bump corner keeps SUB Wave on an "
                    "authored value at all 21 pad positions (stray %.6g)", strayWave);
      expect(waveOk && sawSine && sawBump, m);
      expect(sawBlend,
             "T11c CALIBRATION: the SAME sweep drives the block's CONTINUOUS SUB Level strictly "
             "between the two corners' values — so the sweep moves and T11b's detector could see "
             "an interpolated wave if there were one");
    }
    w.kill();
  }

  /* T12 — B203: THE BLOCK'S GATE IS A CORNER VALUE, AND IT RAMPS.
     The human, 2026-09-21: "Sub on/off is still exempt from morph and it ought
     to be wired in the way the other two oscs are." B48 is the way the other
     two are wired: the bilinear weight of the corners holding the switch ON
     becomes a GAIN RAMP through the ~8 ms smoother, and the stepped flip is
     deferred to the weight floor where the source is already ~-60 dB. So this
     asserts BOTH halves and each is the other's control — the AUDIO must move
     continuously while the PARAMETER must only ever read 0 or 1. A row that
     only read the parameter could not tell a ramp from a snap, and a row that
     only read the audio could not tell the deferral from an interpolated gate
     (which would be a value no corner authored).
     Both swarm oscillators are OFF IN EVERY CORNER, so what the sweep measures
     is the sub's row and nothing else. */
  {
    std::vector<std::string> go;
    std::vector<double> base;
    size_t iGate = 0;
    { Rig t; t.boot();
      go = liveOrder(hypersaw_debug_cornervals(t.p, 0));
      base.assign(go.size(), 0.0);
      const char *c = hypersaw_debug_cornervals(t.p, 0);
      for (size_t i = 0; i < go.size(); i++) base[i] = valueOf(c, go[i].c_str());
      t.kill(); }
    iGate = idx(go, "4015");
    expect(iGate < go.size(),
           "T12a the SUB block's GATE (id 4015) is a corner slot at all — RED before B203");
    if (iGate < go.size())
    {
      base[idx(go, "150")] = 0;
      base[idx(go, "1150")] = 0;
      base[idx(go, "4007")] = 1.0;   // sub level: the row well clear of the floor
      auto file = [&](const std::vector<double> &a) {
        std::string s = "{\"morphLayout\":9,\"cornerPreset\":[";
        for (size_t i = 0; i < a.size(); i++) { char b[32]; std::snprintf(b, sizeof b, i ? ",%.6g" : "%.6g", a[i]); s += b; }
        return s + "]}"; };
      auto author = [&](Rig &w, const double g4[4]) {
        for (int k = 0; k < 4; k++)
        { std::vector<double> a = base; a[iGate] = g4[k];
          hypersaw_debug_cornerapply(w.p, k, file(a).c_str()); } };
      // Morph on, no morph glide (158 = 0) so each pad position is settled by
      // the time it is read, pad parked at x, y = 0, one held note.
      auto start = [&](Rig &w, double x) {
        EvList e;
        e.params.push_back(mkParam(151, 1));
        e.params.push_back(mkParam(158, 0));
        e.params.push_back(mkParam(152, x));
        e.params.push_back(mkParam(153, 0));
        e.notes.push_back(mkNote(CLAP_EVENT_NOTE_ON, 0, 45, 1));
        e.finalize(); w.proc.in_events = &e.list; w.p->process(w.p, &w.proc); };
      auto rmsOf = [](const std::vector<float> &a) {
        double s = 0; for (float v : a) s += (double)v * v;
        return std::sqrt(s / (a.size() ? a.size() : 1)); };

      // ---- T12b/c: the sweep, in ONE instance -----------------------------
      const double edge[4] = {0, 1, 0, 1};   // corners 0/2 hold it OFF, 1/3 ON
      std::vector<double> level;
      bool valueSnaps = true;
      double strayGate = -1;
      { Rig w; w.boot(); author(w, edge); start(w, 0.0); w.run(40);
        auto *px = (const clap_plugin_params_t *)w.p->get_extension(w.p, CLAP_EXT_PARAMS);
        for (int s = 0; s <= 20; s++)
        { EvList e; e.params.push_back(mkParam(152, s / 20.0)); e.finalize();
          w.proc.in_events = &e.list; w.p->process(w.p, &w.proc);
          w.run(24);                                   // settle the ~8 ms ramp
          level.push_back(rmsOf(w.capture(8)));
          double gv = -1; px->get_value(w.p, 4015, &gv);
          if (gv != 0 && gv != 1) { valueSnaps = false; strayGate = gv; } }
        w.kill(); }
      const double lo = level.front(), hi = level.back(), span = hi - lo;
      double worst = 0;
      int between = 0;
      for (size_t i = 1; i < level.size(); i++)
        worst = std::max(worst, std::fabs(level[i] - level[i - 1]));
      for (double v : level)
        if (v > lo + span * 0.05 && v < hi - span * 0.05) between++;
      char m3[240];
      std::snprintf(m3, sizeof m3,
                    "T12b the gate RAMPS across the pad: over 21 positions the sub's level "
                    "rises %.4f -> %.4f with %d strictly between and no adjacent step above "
                    "25%% of the span (worst %.1f%%)",
                    lo, hi, between, span > 0 ? worst / span * 100 : 100.0);
      expect(span > 1e-3 && between >= 12 && worst <= span * 0.25, m3);
      char m4[200];
      std::snprintf(m4, sizeof m4,
                    "T12c CONTROL/paired: the gate's own VALUE only ever reads 0 or 1 across "
                    "that same sweep — the flip is deferred, never interpolated (stray %.6g)",
                    strayGate);
      expect(valueSnaps, m4);

      // ---- T12d: a pure corner is exact -----------------------------------
      /* B48's claim, restated for the gate: "at a pure corner the weight
         equals that corner's stored enable, so corners stay bit-identical".
         Two instances that take the IDENTICAL morph code path and differ only
         in WHERE the weight 1.0 came from — one from a pure corner, one from
         all four corners agreeing. Anything but an exact 1.0 at the corner
         puts a ramp on one and not the other. The OFF half is the same claim
         at the other end: a pure OFF corner is exactly 0, so the row is
         exactly silent, not merely quiet. */
      const double allOn[4] = {1, 1, 1, 1};
      std::vector<float> pure, agree, off;
      { Rig w; w.boot(); author(w, edge); start(w, 1.0); w.run(60); pure = w.capture(16); w.kill(); }
      { Rig w; w.boot(); author(w, allOn); start(w, 1.0); w.run(60); agree = w.capture(16); w.kill(); }
      { Rig w; w.boot(); author(w, edge); start(w, 0.0); w.run(60); off = w.capture(16); w.kill(); }
      size_t diff = pure.size() != agree.size() ? 0 : pure.size();
      for (size_t i = 0; i < pure.size() && i < agree.size(); i++)
        if (pure[i] != agree[i]) { diff = i; break; }
      double offPeak = 0;
      for (float v : off) offPeak = std::max(offPeak, std::fabs((double)v));
      double onRms = 0; for (float v : agree) onRms += (double)v * v;
      onRms = std::sqrt(onRms / (agree.size() ? agree.size() : 1));
      char m5[260];
      std::snprintf(m5, sizeof m5,
                    "T12d a PURE CORNER is exact: parked on the ON corner the render is "
                    "BIT-IDENTICAL to the same patch with all four corners holding the gate ON "
                    "(%zu samples, first difference %s), and the OFF corner is exactly silent "
                    "(peak %.3e); anchor: the ON render is not silence (rms %.4f)",
                    pure.size(), diff == pure.size() ? "none" : "SOME", offPeak, onRms);
      expect(diff == pure.size() && offPeak == 0.0 && onRms > 1e-3, m5);
    }
  }

  /* T13 — B240: THE WHOLE ORDER IS FROZEN, NOT ONLY ITS FIRST 224 SLOTS.
     T1 pins the ADR-159 prefix and T10b a shape; neither sees a row landing
     between the routing block and the SUB's gate, which is exactly where a
     second engine block's pass-1 rows used to go. The fixture is every slot
     of layout 9, read from a file so an insertion is a visible one-line diff
     in review. A fresh rig, so nothing the rows above loaded can matter. */
  {
    const char *fixPath = argc > 1 ? argv[1] : "tests/morph_order.txt";
    const OrderFixture fx = readOrderFixture(fixPath);
    Rig t; t.boot();
    const std::vector<std::string> order = liveOrder(hypersaw_debug_cornervals(t.p, 0));
    static char st[1 << 18];
    hypersaw_debug_state(t.p, st, sizeof st);
    const char *mk = std::strstr(st, "\"morphLayout\":");
    const int marker = mk ? std::atoi(mk + 14) : -1;
    t.kill();

    char m[240];
    std::snprintf(m, sizeof m, "T13a the fixture %s is readable: layout %d, %zu slots", fixPath,
                  fx.layout, fx.ids.size());
    expect(fx.layout > 0 && !fx.ids.empty(), m);

    const size_t brk = prefixBreak(fx.ids, order);
    if (brk == SIZE_MAX)
      std::snprintf(m, sizeof m,
                    "T13b every one of the %zu frozen slots holds its layout-%d id (live order %zu "
                    "slots: %zu appended at the tail)",
                    fx.ids.size(), fx.layout, order.size(), order.size() - fx.ids.size());
    else
      std::snprintf(m, sizeof m,
                    "T13b slot %zu MOVED: the fixture says %s, the live order says %s — an insertion, "
                    "removal or reorder; a new member goes in Plugin::kMorphTailIds",
                    brk, fx.ids[brk].c_str(), brk < order.size() ? order[brk].c_str() : "<end>");
    expect(!fx.ids.empty() && brk == SIZE_MAX, m);

    std::snprintf(m, sizeof m,
                  "T13c the marker (%d) names this order: fixture layout %d, %zu unfrozen tail "
                  "slot(s) — equal with none, exactly one bump with some",
                  marker, fx.layout, order.size() > fx.ids.size() ? order.size() - fx.ids.size() : 0);
    expect(markerOk(order.size(), fx.ids.size(), marker, fx.layout), m);

    std::vector<std::string> sorted = order;
    std::sort(sorted.begin(), sorted.end());
    const auto dup = std::adjacent_find(sorted.begin(), sorted.end());
    std::snprintf(m, sizeof m, "T13d no id holds two slots (duplicate: %s)",
                  dup == sorted.end() ? "none" : dup->c_str());
    expect(dup == sorted.end(), m);

    /* T13e THE CONTROL (L0032): the comparator T13b trusts must report each
       kind of move AT the slot it happens, and must not report an append.
       Planted on the fixture itself, mid-field (slot 240 sits inside the
       routing block, the region nothing before B240 could see). */
    if (fx.ids.size() > 250)
    {
      const size_t at = 240;
      std::vector<std::string> ins = fx.ids, rem = fx.ids, swp = fx.ids, app = fx.ids;
      ins.insert(ins.begin() + (long)at, "99999");
      rem.erase(rem.begin() + (long)at);
      std::swap(swp[at], swp[at + 1]);
      app.push_back("99999");
      const bool insOk = prefixBreak(fx.ids, ins) == at, remOk = prefixBreak(fx.ids, rem) == at,
                 swpOk = swp[at] != swp[at + 1] && prefixBreak(fx.ids, swp) == at,
                 appOk = prefixBreak(fx.ids, app) == SIZE_MAX;
      const bool mkCtl = !markerOk(fx.ids.size() + 1, fx.ids.size(), fx.layout, fx.layout) &&
                         markerOk(fx.ids.size() + 1, fx.ids.size(), fx.layout + 1, fx.layout) &&
                         !markerOk(fx.ids.size(), fx.ids.size(), fx.layout + 1, fx.layout) &&
                         !markerOk(fx.ids.size() + 1, fx.ids.size(), fx.layout + 2, fx.layout);
      std::snprintf(m, sizeof m,
                    "T13e CONTROL: planted insertion %s, removal %s, swap %s at slot %zu; planted "
                    "append %s; marker rule rejects an unbumped append and a double bump %s",
                    insOk ? "caught" : "MISSED", remOk ? "caught" : "MISSED",
                    swpOk ? "caught" : "MISSED", at, appOk ? "admitted" : "REJECTED",
                    mkCtl ? "yes" : "NO");
      expect(insOk && remOk && swpOk && appOk && mkCtl, m);
    }
    else
      expect(false, "T13e CONTROL: the fixture is too short to plant mid-field (<= 250 slots)");
  }

  std::printf("morphlayout_check: %s\n", fails ? "FAIL" : "PASS"); r.kill(); hypersaw_entry_deinit(); return fails ? 1 : 0;
}
