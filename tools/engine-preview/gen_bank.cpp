// Build step (B624, not shipped): translates and levels the engine's bank once, and writes it as C++ the
// plug-in embeds. level_bank renders every preset, about two minutes, so it can't run when a host loads the
// plug-in; running it here also means the plug-in holds the same levelled states as the rendered previews.
// Since B645 it appends the demo bank (demo_bank.json, read by demo_bank.cpp) after the factory presets, each with its
// four corners and its morph: the factory entries are written exactly as before, so their indices and states don't move.
//   gen_bank <data/bank/presets.json> <demo_bank.json> <out.inc>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "demo_bank.h"
#include "horde_engine.h"

int main(int argc, char** argv) {
  if (argc != 4) { std::fprintf(stderr, "usage: gen_bank <presets.json> <demo_bank.json> <out.inc>\n"); return 2; }
  std::ifstream in(argv[1]);
  std::stringstream text;
  text << in.rdbuf();
  std::vector<horde::BankPreset> bank;
  const std::size_t n = horde::translate_bank(text.str(), bank);
  if (n == 0 || n != bank.size()) { std::fprintf(stderr, "gen_bank: translated %zu presets\n", n); return 1; }
  horde::LevelReport report;
  if (!horde::level_bank(bank, report)) { std::fprintf(stderr, "gen_bank: level_bank refused\n"); return 1; }

  std::ifstream demo_in(argv[2]);
  std::stringstream demo_text;
  demo_text << demo_in.rdbuf();
  std::vector<preview::demo::Demo> demos;
  std::string error;
  if (!preview::demo::read(demo_text.str(), bank, demos, error)) { std::fprintf(stderr, "gen_bank: %s\n", error.c_str()); return 1; }

  std::ofstream out(argv[3]);
  char number[64];
  for (std::size_t i = 0; i < bank.size(); ++i) {
    out << "static const PresetRoute kRoutes" << i << "[] = {";
    for (const auto& r : bank[i].routes) {
      std::snprintf(number, sizeof number, "%.17g", r.octaves);
      out << "{" << r.env << ", \"" << r.key << "\", " << number << "}, ";
    }
    out << "{0, nullptr, 0}};\n";
  }
  // The demos' morph data (B645): every corner's state, D.2's settings in v and the keys' modes, by key.
  for (std::size_t d = 0; d < demos.size(); ++d) {
    out << "static const DemoSetting kDemoSettings" << d << "[] = {";
    for (const auto& [key, v] : demos[d].settings) {
      std::snprintf(number, sizeof number, "%.17g", v);
      out << "{\"" << key << "\", " << number << "}, ";
    }
    out << "{nullptr, 0}};\n";
    out << "static const DemoMode kDemoModes" << d << "[] = {";
    for (const auto& m : demos[d].modes) out << "{\"" << m.key << "\", \"" << m.mode << "\", " << m.count << "}, ";
    out << "{nullptr, nullptr, 0}};\n";
    out << "static const PresetRoute kDemoRoutes" << d << "[] = {";
    for (const auto& r : demos[d].routes) {
      std::snprintf(number, sizeof number, "%.17g", r.octaves);
      out << "{" << r.env << ", \"" << r.key << "\", " << number << "}, ";
    }
    out << "{0, nullptr, 0}};\n";
    out << "static const char* const kDemoCorners" << d << "[4] = {";
    for (int c = 0; c < 4; ++c) out << "R\"HORDEBANK(" << demos[d].corners[c] << ")HORDEBANK\", ";
    out << "};\n";
    out << "static const EmbeddedMorph kDemoMorph" << d << " = {R\"HORDEBANK(" << demos[d].description << ")HORDEBANK\", kDemoCorners" << d
        << ", kDemoSettings" << d << ", " << demos[d].settings.size() << ", kDemoModes" << d << ", " << demos[d].modes.size() << "};\n";
  }
  out << "static const EmbeddedPreset kBank[] = {\n";
  for (std::size_t i = 0; i < bank.size(); ++i) {
    // A raw string with a delimiter no state document contains, since a state is JSON and may quote anything.
    std::snprintf(number, sizeof number, "%.17g", bank[i].glide_s);
    out << "{R\"HORDEBANK(" << bank[i].name << ")HORDEBANK\", R\"HORDEBANK(" << bank[i].state << ")HORDEBANK\", kRoutes" << i << ", "
        << bank[i].routes.size() << ", " << number << ", " << (bank[i].glide_always ? "true" : "false") << "},\n";
  }
  for (std::size_t d = 0; d < demos.size(); ++d) {
    // A demo plays corner A's state as its own (what the saved chunk is written against); its corners ride beside it.
    // Its name says which section it is in: every list that shows the bank (the page, the host's Preset, the window)
    // shows one flat list of names.
    std::snprintf(number, sizeof number, "%.17g", demos[d].glide_s);
    out << "{R\"HORDEBANK(Demo (morph): " << demos[d].name << ")HORDEBANK\", R\"HORDEBANK(" << demos[d].corners[0] << ")HORDEBANK\", kDemoRoutes" << d << ", " << demos[d].routes.size() << ", " << number << ", "
        << (demos[d].glide_always ? "true" : "false") << ", &kDemoMorph" << d << "},\n";
  }
  out << "};\n";
  out << "inline constexpr int kFactorySize = " << bank.size() << ";\n";
  std::printf("gen_bank: %zu presets levelled, median %.2f LUFS; %zu demos\n", bank.size(), report.median_lufs, demos.size());
  return out.good() ? 0 : 1;
}
