/* preset_pitch_check — after a preset load, a newly struck voice sounds at ITS OWN pitch.
   WIRED: ./verify full, beside morphlayout_check. Added under ADR-180 §1 (B454 item 10, ADR-206 item 5).

   Replaces tools/preset_probe.cpp (ADR-159 / B115, 2026-09-11), which bisected the report "a second
   note plays the first note again" by autocorrelating the audio. That verdict was a FALSE POSITIVE on
   today's tree: one integer lag at E4 is ~13 cents and the estimator biases short, so it read +36
   cents against a 30-cent limit while the plugin's own voice row showed 329.628 Hz. The gate sat
   inside its estimator's error and had no must-read-clean control. This one reads the plugin's voice
   table (hypersaw_debug_voices) instead: no audio, no estimator, nothing to bias.

   WHAT IS ASSERTED. After the load and after every step of each scenario below, once every glide the
   preset configures has FINISHED (waited out on the plugin's own glideActive flag, capped at
   kSettleCapSec — never a hard-coded wait a slow glide would outlast), EVERY gated voice's f0 is within
   kPitchTolCents of the equal-tempered pitch of its own MIDI note, and a gated voice for the note just
   struck exists. f0 is the note's pitch BEFORE tune, the pitch envelope (noteTune), vibrato, drift and
   the wheel — none of those enter f0 (swarm_core.h render: f0c = f0cur * tune * noteTune) — so a preset
   with intentional pitch modulation is compared against the plugin's own TARGET pitch for the voice,
   which is what f0 is, and this is why no voice is skipped. f0cur (f0 plus consonance-gravity offsets)
   is asserted too when the preset's Gravity is off; with gravity on it differs from f0 by design and is
   printed as information, not judged.
   Scenarios (each on a fresh plugin, so they cannot hide one another): the 2026-09-11 sequence (hold
   60, strike 64, release 60, press 64 again); a wide leap up and one down; a growing chord with the top
   released; a restrike after the voices released and again after silence (the glide-from-last-note
   modes). Whether the preset is mono, legato or gliding is the PRESET'S choice; the scenarios do not
   set it, so a legato pair happens exactly when the preset asked for one.
   What this does NOT see: the heard pitch (a stuck pitch lane downstream of f0), and any voice
   routing the voice table does not list. The wheel lane stays at zero on a chord is anchor_check's.
   The preset load / remap path is morphlayout_check's.

   CONTROLS (bank mode; a gate that cannot read red proves nothing):
   C1 CLEAN       defaults, the 2026-09-11 sequence: zero red, and enough readings that zero is not vacuous.
   C2 PLANT (reading layer)  the same sequence with the comparator handed the PREVIOUS note's pitch for
                  the newest voice. This is NOT a plugin fault: it proves the comparator and the voice
                  selection turn red on a parked f0, and nothing about the plugin.
   C3 PLANT (real) a 1.5 s constant-time note law, 60 held, 64 struck, read at once: the plugin's own
                  voice table then really does show the new voice's f0 parked at the OLD note (the
                  ADR-159 symptom's shape), so it must read red and be nearer 60 than 64; the same
                  plant read after the glide settled must read clean. This reaches the real voice table
                  through parameters only. It does NOT reproduce the layout-shift fault itself — that
                  needs the pre-ADR-159 shell, which cannot be built without editing src/.
   Usage: preset_pitch_check [<bank-dir>]    every .json under each category dir (not corners/); fewer
                                             than kMinBank is a failure. No argument = docs/presets/factory
                                             relative to the cwd, because tools/sanitize_oracles.sh runs
                                             every oracle it parses from ./verify with NO arguments, from
                                             the repo root; a usage exit there would read as a sanitizer fail.
          preset_pitch_check <preset.json>   one file, verbose, no controls
   HOME is pointed at a scratch dir: nothing here may touch the user's real preset store.
   Exit 0 clean, 1 red, 2 usage or infrastructure. */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"
#include "../src/hypersaw_debug.h"
namespace fs = std::filesystem;
namespace {
#include "notefuzz_scaffold.inc"

constexpr double kPitchTolCents = 1.0;   // the human-approved bar (ADR-206 item 5); the voice row prints f0 to 3 decimals (0.05 cent at the lowest key)
constexpr double kSettleCapSec = 30.0;   // longest glide waited out; a glide still running at the cap is red, not skipped
constexpr double kGravOff = 0.005;       // swarm_core.h gravityStep: gravity below this returns untouched
constexpr clap_id kGravId = 29, kVoiceMono = 32, kVoiceLegato = 34, kGlideMode = 90, kNoteLink = 137, kNoteLaw = 138, kNoteTime = 139;   // src/hypersaw_clap.cpp param table
constexpr int kMinBank = 30;             // bank_check's floor: an emptied or mis-pointed directory must not pass vacuously
constexpr int kSettleTail = 3;           // blocks run after the last glide flag clears, so the final tick has landed

double etHz(int midi) { return 440.0 * std::pow(2.0, (midi - 69) / 12.0); }
double cents(double a, double b) { return 1200.0 * std::log2(a / b); }
// NOT `fabs(d) > tol`: a NaN or an f0 of 0 (log2 of <=0) must read red, and every comparison with NaN is false.
bool within(double centsOff) { return std::fabs(centsOff) <= kPitchTolCents; }

struct Voice { int slot = 0, midi = 0, gate = 0, glide = 0; double f0 = 0, f0cur = 0, noteTune = 1; };
enum class Fault { None, ParkedAtPrevious };
int g_parseBad = 0;   // voice-table records that did not parse: counted, never silently dropped

struct Rig {
  const clap_plugin_t *p = nullptr; std::vector<float> L, R; clap_audio_buffer_t out{}; clap_process_t proc{}; float *ch[2];
  Fault fault = Fault::None; int lastStruck = -1, prevStruck = -1, injected = 0;
  void boot() { auto *f = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    p = f->create_plugin(f, &kHost, "com.lifted-truck.hypersaw"); p->init(p); p->activate(p, kSR, 32, kBlock); p->start_processing(p);
    L.assign(kBlock, 0); R.assign(kBlock, 0); ch[0] = L.data(); ch[1] = R.data(); out.data32 = ch; out.channel_count = 2;
    proc.frames_count = kBlock; proc.audio_outputs = &out; proc.audio_outputs_count = 1; proc.out_events = &kOut; }
  void step(EvList &e) { e.finalize(); proc.in_events = &e.list; p->process(p, &proc); }
  void block() { EvList e; step(e); }
  void run(double s) { for (int i = 0; i < Math_blocks(s); i++) block(); }
  void set(clap_id id, double v) { EvList e; e.params.push_back(mkParam(id, v)); step(e); }
  void note(int m, bool on, int id) { EvList e; e.notes.push_back(mkNote(on ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF, 0, (int16_t)m, id, on ? 0.8 : 0)); step(e);
    if (on) { prevStruck = lastStruck; lastStruck = m; } }
  double param(clap_id id) const { double v = 0; auto *px = (const clap_plugin_params_t *)p->get_extension(p, CLAP_EXT_PARAMS); if (px) px->get_value(p, id, &v); return v; }
  std::vector<Voice> voices() {
    char b[4096]; hypersaw_debug_voices(p, b, sizeof b); std::vector<Voice> vs; int want = 0;
    for (const char *q = b; *q; q++) want += (*q == ';');
    for (const char *q = b; *q;) { Voice v; int n = 0;
      if (std::sscanf(q, "%d,%d,%d,%lf,%lf,%d,%lf;%n", &v.slot, &v.midi, &v.gate, &v.f0, &v.f0cur, &v.glide, &v.noteTune, &n) < 7 || n == 0) break;
      vs.push_back(v); q += n; }
    if ((int)vs.size() != want) g_parseBad++;
    if (fault == Fault::ParkedAtPrevious && prevStruck >= 0)   // the reading-layer plant (C2): the newest voice reads as the previous note
      for (auto &v : vs) if (v.gate && v.midi == lastStruck) { v.f0 = v.f0cur = etHz(prevStruck); injected++; }
    return vs; }
  bool anyGlide() { for (const auto &v : voices()) if (v.gate && v.glide) return true; return false; }
  // Wait out every glide on the plugin's own flag. Returns seconds waited, or a negative number if the cap was hit.
  double settle() { const int cap = Math_blocks(kSettleCapSec); int n = 0;
    while (n < cap && anyGlide()) { block(); n++; }
    const bool capped = n >= cap && anyGlide();
    for (int i = 0; i < kSettleTail; i++) block();
    return capped ? -1.0 : n * (double)kBlock / kSR; }
  void kill() { p->stop_processing(p); p->deactivate(p); p->destroy(p); }
};

struct Ctx {
  std::string preset; bool gravOff = true, verbose = false, quiet = false;
  int readings = 0, red = 0; double worst = 0, maxSettle = 0;
};

// One reading of the voice table. Returns the number of red findings in it.
int judge(Rig &r, Ctx &c, const char *step, int want) {
  int bad = 0; bool found = false; const auto vs = r.voices();
  for (const auto &v : vs) {
    if (!v.gate) continue;
    const double e = etHz(v.midi), d0 = cents(v.f0, e), dc = cents(v.f0cur, e);
    const bool ok0 = within(d0), okc = !c.gravOff || within(dc);
    c.readings++; if (v.midi == want) found = true;
    if (std::isfinite(d0)) c.worst = std::max(c.worst, std::fabs(d0));
    if (c.verbose) std::printf("      %-26s slot %2d note %3d  f0 %10.3f vs %10.3f  (%+.4f c)  f0cur %+.4f c%s  glide %d  noteTune %.4f\n", step, v.slot, v.midi, v.f0, e, d0, dc, c.gravOff ? "" : " (gravity on: info)", v.glide, v.noteTune);
    if (ok0 && okc) continue;
    bad++; if (!c.quiet) std::printf("  RED  %s | %s | note %d slot %d: f0 %.3f Hz, its note is %.3f Hz (%+.2f c); f0cur %+.2f c%s; glideActive %d\n", c.preset.c_str(), step, v.midi, v.slot, v.f0, e, d0, dc, c.gravOff ? "" : " (not judged: gravity on)", v.glide);
  }
  if (!found) { bad++; if (!c.quiet) std::printf("  RED  %s | %s | no gated voice for note %d (gated voices: %d)\n", c.preset.c_str(), step, want, (int)std::count_if(vs.begin(), vs.end(), [](const Voice &v) { return v.gate != 0; })); }
  c.red += bad; return bad;
}
// Wait out the glides, then read. A glide still running at the cap is itself a finding.
void settleJudge(Rig &r, Ctx &c, const char *step, int want) {
  const double s = r.settle();
  if (s < 0) { c.red++; if (!c.quiet) std::printf("  RED  %s | %s | a glide was still running after %.0f s\n", c.preset.c_str(), step, kSettleCapSec); }
  else c.maxSettle = std::max(c.maxSettle, s);
  judge(r, c, step, want);
}

// ---- scenarios: each runs on a fresh plugin ---------------------------------
void sPair(Rig &r, Ctx &c) {   // the 2026-09-11 report: hold 60, strike 64, release 60, press 64 again
  r.note(60, true, 1); r.run(0.4); settleJudge(r, c, "held 60", 60);
  r.note(64, true, 2); settleJudge(r, c, "strike 64 over held 60", 64);
  r.note(60, false, 1); r.run(0.05); settleJudge(r, c, "release 60", 64);
  r.note(64, true, 3); settleJudge(r, c, "press 64 again", 64);
}
void sWideUp(Rig &r, Ctx &c) {
  r.note(36, true, 1); r.run(0.3); r.note(96, true, 2); settleJudge(r, c, "strike 96 over held 36", 96);
  r.note(36, false, 1); r.run(0.05); settleJudge(r, c, "release 36", 96);
}
void sWideDown(Rig &r, Ctx &c) {
  r.note(96, true, 1); r.run(0.3); r.note(36, true, 2); settleJudge(r, c, "strike 36 over held 96", 36);
  r.note(96, false, 1); r.run(0.05); settleJudge(r, c, "release 96", 36);
}
void sChord(Rig &r, Ctx &c) {   // the top key released last: a mono preset must fall back to the key below it
  r.note(60, true, 1); settleJudge(r, c, "chord: 60", 60);
  r.note(64, true, 2); settleJudge(r, c, "chord: +64", 64);
  r.note(67, true, 3); settleJudge(r, c, "chord: +67", 67);
  r.note(71, true, 4); settleJudge(r, c, "chord: +71", 71);
  r.note(71, false, 4); r.run(0.05); settleJudge(r, c, "chord: release 71", 67);
}
void sRestrike(Rig &r, Ctx &c) {   // after the keys are up: the release-tail and silence glide sources (glideMode 1, 2)
  r.note(64, true, 1); r.run(0.3); r.note(64, false, 1); r.run(0.02);
  r.note(67, true, 2); settleJudge(r, c, "restrike 67 in the tail", 67);
  r.note(67, false, 2); r.run(2.0);
  r.note(60, true, 3); settleJudge(r, c, "strike 60 after silence", 60);
  r.note(60, false, 3); r.run(0.02); r.note(60, true, 4); settleJudge(r, c, "repress 60", 60);
}
using Scenario = void (*)(Rig &, Ctx &);
struct Named { const char *name; Scenario run; };
const Named kScenarios[] = {{"pair", sPair}, {"wide-up", sWideUp}, {"wide-down", sWideDown}, {"chord", sChord}, {"restrike", sRestrike}};

std::string readAll(const fs::path &f) { std::ifstream in(f, std::ios::binary); std::stringstream ss; ss << in.rdbuf(); return ss.str(); }

struct PresetResult { int red = 0, readings = 0; double worst = 0, maxSettle = 0; };
// Voice-mode overlays, set AFTER the load. The factory bank is entirely poly with glide 0 (every file stores voiceMono 0,
// glide 0, glideMode 0, noteLawLink 1 over an off bend law), so "as authored" never exercises a glide, a retarget or a
// legato pair: the paths where a new voice is NOT born at its note. The overlays impose them on every preset, and the
// scenarios are unchanged. A preset that DOES store a mode is covered by "as authored" as well.
struct SetParam { clap_id id; double v; };
struct Overlay { const char *name; std::vector<SetParam> sets; };
const Overlay kOverlays[] = {
  {"as authored", {}},
  {"mono legato", {{kVoiceMono, 1}, {kVoiceLegato, 1}}},
  {"mono restrike", {{kVoiceMono, 1}, {kVoiceLegato, 0}}},
  {"poly glide 400 ms, from always", {{kNoteLink, 0}, {kNoteLaw, 1}, {kNoteTime, 400}, {kGlideMode, 2}}},
  {"mono legato glide 400 ms", {{kVoiceMono, 1}, {kVoiceLegato, 1}, {kNoteLink, 0}, {kNoteLaw, 1}, {kNoteTime, 400}}},
};

// One preset through every overlay x scenario, each on a fresh plugin with the preset applied.
PresetResult runPreset(const std::string &label, const std::string &json, bool verbose) {
  Ctx c; c.verbose = verbose;
  for (const auto &o : kOverlays) for (const auto &s : kScenarios) {
    c.preset = label + " [" + o.name + "]";
    Rig r; r.boot();
    if (!hypersaw_debug_apply(r.p, json.c_str())) { std::printf("  RED  %s | apply FAILED\n", c.preset.c_str()); r.kill(); c.red++; continue; }
    r.run(0.2);   // the apply is queued: let it land before reading any parameter
    c.gravOff = r.param(kGravId) < kGravOff;
    for (const auto &sp : o.sets) r.set(sp.id, sp.v);
    r.run(0.05);
    if (verbose) std::printf("    overlay '%s', scenario %s (gravity %s)\n", o.name, s.name, c.gravOff ? "off: f0cur judged" : "on: f0cur is info");
    s.run(r, c); r.kill();
  }
  return {c.red, c.readings, c.worst, c.maxSettle};
}

// ---- controls ------------------------------------------------------------------
bool report(bool ok, const std::string &what) { std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", what.c_str()); return ok; }
std::string fmt(const char *f, double a = 0, double b = 0, double c = 0, double d = 0) { char s[400]; std::snprintf(s, sizeof s, f, a, b, c, d); return s; }

bool controls() {
  bool ok = true;
  { Ctx c; c.preset = "C1"; c.quiet = true; Rig r; r.boot(); r.run(0.2); sPair(r, c); r.kill();   // C1: defaults, nothing planted
    ok &= report(c.red == 0 && c.readings >= 4, fmt("C1 clean: defaults, the 2026-09-11 sequence: %.0f red over %.0f readings (must be 0 over >= 4)", c.red, c.readings)); }
  { Ctx c; c.preset = "C2"; c.quiet = true; Rig r; r.boot(); r.fault = Fault::ParkedAtPrevious; r.run(0.2); sPair(r, c); const int inj = r.injected; r.kill();   // C2: reading-layer plant
    ok &= report(c.red >= 2 && inj >= 2, fmt("C2 plant (reading layer, newest voice read as the PREVIOUS note): %.0f red from %.0f injected readings (must be >= 2 of each)", c.red, inj)); }
  { Ctx c; c.preset = "C3"; c.quiet = true; Rig r; r.boot();   // C3: a real, slow note glide, read before and after it lands
    r.set(kNoteLink, 0); r.set(kNoteLaw, 1); r.set(kNoteTime, 1500); r.run(0.1);   // own note law, constant time, 1500 ms
    r.note(60, true, 1); r.settle(); r.note(64, true, 2);
    double f0 = 0; for (const auto &v : r.voices()) if (v.gate && v.midi == 64) f0 = v.f0;
    const int redNow = judge(r, c, "C3 at once", 64);
    const double toOld = std::fabs(cents(f0, etHz(60))), toNew = std::fabs(cents(f0, etHz(64)));
    ok &= report(redNow >= 1 && toOld < toNew, fmt("C3 plant (real slow glide), read at once: new voice f0 %.3f Hz is %.1f c from the OLD note, %.1f c from its own (must be red and nearer the old)", f0, toOld, toNew));
    const double waited = r.settle(); const int redLater = judge(r, c, "C3 settled", 64);
    ok &= report(waited > 0 && redLater == 0, fmt("C3 clean (same plant, glide finished after %.2f s): %.0f red (must be 0, and the settle must not have hit the cap)", waited, redLater));
    r.kill(); }
  return ok;
}

void setEnv(const char *k, const char *v) {
#ifdef _WIN32
  _putenv_s(k, v);
#else
  setenv(k, v, 1);
#endif
}
// The plugin writes forensic dumps under $HOME's preset store (PANIC, hypersaw_test_dump_forensics). This tool presses neither,
// but "never touch the user's real store" is not left to that fact: HOME and APPDATA are redirected before any plugin exists.
struct ScratchHome {
  fs::path dir = fs::temp_directory_path() / "hypersaw_preset_pitch_check";
  ScratchHome() { std::error_code ec; fs::remove_all(dir, ec); fs::create_directories(dir / "home", ec); fs::create_directories(dir / "appdata", ec);
    setEnv("HOME", (dir / "home").string().c_str()); setEnv("APPDATA", (dir / "appdata").string().c_str()); }
  ~ScratchHome() { std::error_code ec; fs::remove_all(dir, ec); }
};
}  // namespace

int main(int argc, char **argv) {
  if (argc > 2) { std::fprintf(stderr, "usage: preset_pitch_check [<bank-dir> | <preset.json>]\n"); return 2; }
  ScratchHome home; const fs::path arg = argc == 2 ? argv[1] : "docs/presets/factory";std::vector<fs::path> files; const bool bank = fs::is_directory(arg);
  if (bank) {   // every category dir, discovered; corners/ holds positional arrays, not patches (bank_check's one exclusion)
    for (const auto &dir : fs::directory_iterator(arg)) {
      if (!dir.is_directory() || dir.path().filename() == "corners") continue;
      for (const auto &e : fs::directory_iterator(dir.path())) if (e.path().extension() == ".json") files.push_back(e.path()); }
    std::sort(files.begin(), files.end());
  } else if (fs::is_regular_file(arg)) files.push_back(arg);
  else { std::fprintf(stderr, "preset_pitch_check: %s is neither a directory nor a file\n", arg.string().c_str()); return 2; }
  std::printf("preset_pitch_check: %zu preset(s); a gated voice's f0 must sit within %.1f cent of its note, glides waited out (cap %.0f s)\n", files.size(), kPitchTolCents, kSettleCapSec);
  int rc = 0, redPresets = 0; long readings = 0; double worst = 0, slowest = 0;
  if (bank && (int)files.size() < kMinBank) { std::printf("  FAIL  bank holds %zu presets, floor %d\n", files.size(), kMinBank); rc = 1; }
  for (const auto &f : files) {
    const std::string label = bank ? f.parent_path().filename().string() + "/" + f.stem().string() : f.filename().string();
    const std::string blob = readAll(f);
    if (blob.empty()) { std::printf("  RED  %s | file empty or unreadable\n", label.c_str()); redPresets++; continue; }
    const PresetResult pr = runPreset(label, blob, !bank);
    readings += pr.readings; worst = std::max(worst, pr.worst); slowest = std::max(slowest, pr.maxSettle);
    if (pr.red || pr.readings == 0) { redPresets++; if (!pr.red) std::printf("  RED  %s | no gated voice was ever read\n", label.c_str()); }
    std::printf("  %s  %-40s %4d readings, worst |f0| %.4f c, slowest settle %.2f s\n", pr.red || pr.readings == 0 ? "RED " : "ok  ", label.c_str(), pr.readings, pr.worst, pr.maxSettle);
  }
  if (g_parseBad) { std::printf("  RED  %d voice-table record(s) did not parse\n", g_parseBad); rc = 1; }
  if (redPresets) rc = 1;
  std::printf("preset_pitch_check: %zu presets, %ld readings, %d red; worst |f0| error %.4f c (bar %.1f), slowest settle %.2f s\n", files.size(), readings, redPresets, worst, kPitchTolCents, slowest);
  if (bank) { if (!controls()) rc = 1; } else std::printf("  (controls run in bank mode only)\n");
  std::printf("preset_pitch_check: %s\n", rc ? "RED" : "clean");
  hypersaw_entry_deinit(); return rc;
}
