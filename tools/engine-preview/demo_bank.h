#pragma once
// The demo bank (B645): a few presets that use the morph's four corners, read from demo_bank.json by gen_bank, which
// embeds them after the factory bank, and by demo_render, which measures them. One reader for both, so what is
// measured is what the plug-in plays. Not real-time: it builds engines to turn each corner into a state text.
//
// What a reader would otherwise re-derive:
// - A corner is written as a difference: a base (a factory preset by name, or the engine's defaults) and the rows the
//   preset's `common` object and the corner's own object set, the corner's last. Each is turned into the engine's own
//   version-1 state text by loading the base, applying the rows as Param events and saving, so the engine checks every
//   value (PA-7) and the text is exactly what a load gives back.
// - Values are written in each row's unit, as SPEC-ENGINE 5.1 states it (Hz, seconds, dB, cents, a % row's 0 to 1); a
//   Step row by its option's name ("saw", "FM free", "on"), or by its number for a row of consecutive integers
//   (unison 7, octave -1). An unknown key, an option the row lacks, a value outside the range, a reserved or Device row
//   in a corner (they never morph, so only `common` may set a Device row) is an error, never a guess.
// - Every corner must state `out.level`, in `common` or its own object: the demo's level is set by measurement
//   (demo_render --level), and a base's level would otherwise differ between gen_bank, which reads the levelled factory
//   bank, and demo_render, which reads it unlevelled (level_bank moves only out.level).
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "horde_engine.h"

namespace preview::demo {

struct Mode { std::string key, mode; int count = 0; };   // the page's names (morph_core.h): mode, and count 0, 2 or 4
// B.1's blade-envelope route (horde_engine.h, BladeRoute), the stand-in for the modulation system the factory bank
// already plays (B632): ENV 2 or ENV 3 to a blade's cut, cutHz or width, `octaves` at full level. Per preset, not per
// corner, so it moves the key around whatever value the morph resolves.
struct Route { int env = 2; std::string key; double octaves = 0; };

struct Demo {
  std::string name, description;
  // demo_render's hints, never embedded: a single note for a lead or a bass (-1: none), and for a sound that dies away
  // while held (a pluck, a hit), the chord struck again every this many seconds (0: held once)
  int single_key = -1;
  double retrigger_s = 0;
  bool bass = false;                // demo_render's low-end ceiling leaves a bass preset out
  double glide_s = 0;               // B.1's glide, which the plug-in plays as 10.3's (B632)
  bool glide_always = false;
  std::vector<std::pair<std::string, double>> settings;   // D.2's settings by key, in v; morph.on is always among them
  std::vector<Mode> modes;
  std::vector<Route> routes;
  std::string corners[4];           // A to D: each the engine's version-1 state text
};

// Reads demo_bank.json. `factory` is the translated bank (levelled or not: every corner sets out.level). False, with
// `error` naming the preset, corner and key, on anything the file states that the engine or D.2 can't take.
bool read(std::string_view json, const std::vector<horde::BankPreset>& factory, std::vector<Demo>& out, std::string& error);

}  // namespace preview::demo
