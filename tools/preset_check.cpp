/*
 * preset_check — the oscillator preset tier (B20).
 *
 * The claim under test is not "it round-trips" but the sharper one that makes
 * the tier worth having: a preset is SLOT-AGNOSTIC. Saved from one oscillator
 * it must load into another and produce the same parameter values, because
 * "copy oscillator 1 to 2" and "load this osc preset into slot 2" are the same
 * operation. A format that quietly embedded its origin slot would pass a naive
 * round-trip and fail the moment anyone used it.
 *
 * Also pinned: global params never travel. An oscillator preset that carried
 * the FX rack or the master image would silently redecorate the patch it was
 * dropped into — a data-loss bug wearing the costume of a feature.
 * WIRED: ./verify full.
 */
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <functional>
#include "../src/osc_preset.h"

using namespace hypersaw::oscpreset;

static int failures = 0;
static void check(bool ok, const char *what, const char *detail = "")
{
  std::printf("%s   %s%s%s\n", ok ? "OK  " : "FAIL", what, *detail ? "  " : "", detail);
  if (!ok) failures++;
}

// A stand-in parameter surface: per-slot values, plus a global set that must
// never move between slots.
struct Surface
{
  std::vector<std::string> perOsc{"n", "detune", "K", "seed", "vol", "shape"};
  std::vector<std::string> globals{"width", "vol_master", "fx1type"};
  std::map<std::string, double> slot[2];
  std::map<std::string, double> global;

  bool isGlobal(const std::string &k) const
  {
    for (const auto &g : globals) if (g == k) return true;
    return false;
  }
  void forEach(int, const std::function<void(const char *, bool)> &) {}
};

int main()
{
  Surface s;
  // oscillator 0 gets a distinctive patch; oscillator 1 stays at defaults
  s.slot[0] = {{"n", 9}, {"detune", 0.61}, {"K", -0.4}, {"seed", 4242}, {"vol", 0.33}, {"shape", 0.8}};
  s.slot[1] = {{"n", 7}, {"detune", 0.28}, {"K", 0.0},  {"seed", 1234}, {"vol", 0.0},  {"shape", 0.0}};
  s.global = {{"width", 0.8}, {"vol_master", 0.4}, {"fx1type", 3}};

  auto forEach = [&](auto cb) {
    for (const auto &k : s.perOsc) cb(k.c_str(), false);
    for (const auto &k : s.globals) cb(k.c_str(), true);
  };

  // --- save from oscillator 0 ------------------------------------------------
  const std::string blob = save(forEach, [&](const char *k) {
    auto it = s.slot[0].find(k);
    return it != s.slot[0].end() ? it->second : (s.global.count(k) ? s.global[k] : 0.0);
  });
  check(blob.rfind(header(), 0) == 0, "preset is versioned");

  {
    bool anyGlobal = false;
    for (const auto &g : s.globals)
      if (blob.find("\n" + g + "=") != std::string::npos || blob.rfind(g + "=", 0) == 0)
        anyGlobal = true;
    check(!anyGlobal, "no global param travels in an oscillator preset");
  }
  check(blob.find("o0.") == std::string::npos && blob.find("o1.") == std::string::npos,
        "preset keys are UNPREFIXED (slot-agnostic on disk)");

  // --- load into oscillator 1 — the operation that matters -------------------
  const int applied = load(blob, [&](const char *k, double v) {
    if (s.isGlobal(k)) return false;
    for (const auto &pk : s.perOsc)
      if (pk == k) { s.slot[1][k] = v; return true; }
    return false;
  });
  char b[96];
  std::snprintf(b, sizeof(b), "(%d keys)", applied);
  check(applied == (int)s.perOsc.size(), "every per-osc key applied", b);

  bool same = true;
  for (const auto &k : s.perOsc)
    if (s.slot[1][k] != s.slot[0][k]) { same = false;
      std::printf("     mismatch %s: %.17g vs %.17g\n", k.c_str(), s.slot[1][k], s.slot[0][k]); }
  check(same, "oscillator 1 now matches oscillator 0 exactly");

  bool globalsIntact = s.global["width"] == 0.8 && s.global["vol_master"] == 0.4 &&
                       s.global["fx1type"] == 3;
  check(globalsIntact, "globals untouched by the load");

  // --- rejections ------------------------------------------------------------
  check(load("hypersaw-state 2\nn=9\n", [](const char *, double) { return true; }) == -1,
        "a PATCH blob is rejected, not silently half-applied");
  check(load("", [](const char *, double) { return true; }) == -1, "empty input rejected");
  {
    // unknown/future keys are skipped, not fatal — the same forward-compatibility
    // the patch loader already promises
    const int n = load(std::string(header()) + "n=5\nnotAThing=1\ndetune=0.4\n",
                       [&](const char *k, double) {
                         return std::strcmp(k, "n") == 0 || std::strcmp(k, "detune") == 0;
                       });
    std::snprintf(b, sizeof(b), "(applied %d of 3)", n);
    check(n == 2, "unknown keys skipped, known ones still applied", b);
  }

  std::printf("preset_check: %s (%d failure%s)\n", failures ? "RED" : "GREEN", failures,
              failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
