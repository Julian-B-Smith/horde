#pragma once
// The levelled bank gen_bank writes, shared by the plug-in and its smoke test so both play the same states.
struct PresetRoute { int env; const char* key; double octaves; };
// glide_s and glide_always: B.1's `glide` (in seconds) and `glideAlways`, which the plug-in plays as 10.3's glide (B632).
struct EmbeddedPreset { const char* name; const char* state; const PresetRoute* routes; int n_routes; double glide_s; bool glide_always; };
#include "bank_embedded.inc"
inline constexpr int kBankSize = int(sizeof kBank / sizeof kBank[0]);
