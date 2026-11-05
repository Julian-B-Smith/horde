#pragma once
// The levelled bank gen_bank writes, shared by the plug-in and its smoke test so both play the same states.
struct PresetRoute { int env; const char* key; double octaves; };
struct EmbeddedPreset { const char* name; const char* state; const PresetRoute* routes; int n_routes; };
#include "bank_embedded.inc"
inline constexpr int kBankSize = int(sizeof kBank / sizeof kBank[0]);
