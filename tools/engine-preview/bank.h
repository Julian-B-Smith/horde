#pragma once
// The levelled bank gen_bank writes, shared by the plug-in and its smoke test so both play the same states.
struct PresetRoute { int env; const char* key; double octaves; };
// B645's demo bank, after the factory presets: four corners and the morph's settings and modes. A setting is D.2's key
// and its v (morph_core.h); a mode the page's names (`modes` message).
struct DemoSetting { const char* key; double v; };
struct DemoMode { const char* key; const char* mode; int count; };
struct EmbeddedMorph {
  const char* description;
  const char* const* corners;   // A to D, each the engine's version-1 state text; corners[0] is the preset's own state
  const DemoSetting* settings; int n_settings;
  const DemoMode* modes; int n_modes;
};
// glide_s and glide_always: B.1's `glide` (in seconds) and `glideAlways`, which the plug-in plays as 10.3's glide (B632).
// morph: a demo's corners and settings (B645), null for a factory preset, which fills all four corners with one sound.
struct EmbeddedPreset {
  const char* name; const char* state; const PresetRoute* routes; int n_routes; double glide_s; bool glide_always;
  const EmbeddedMorph* morph = nullptr;
};
#include "bank_embedded.inc"
// Every preset the plug-in offers: the factory bank's kFactorySize (indices 0 on), then the demos.
inline constexpr int kBankSize = int(sizeof kBank / sizeof kBank[0]);
