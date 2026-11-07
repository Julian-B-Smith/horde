// Build step (B624, not shipped): translates and levels the engine's bank once, and writes it as C++ the
// plug-in embeds. level_bank renders every preset, about two minutes, so it can't run when a host loads the
// plug-in; running it here also means the plug-in holds the same levelled states as the rendered previews.
//   gen_bank <data/bank/presets.json> <out.inc>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "horde_engine.h"

int main(int argc, char** argv) {
  if (argc != 3) { std::fprintf(stderr, "usage: gen_bank <presets.json> <out.inc>\n"); return 2; }
  std::ifstream in(argv[1]);
  std::stringstream text;
  text << in.rdbuf();
  std::vector<horde::BankPreset> bank;
  const std::size_t n = horde::translate_bank(text.str(), bank);
  if (n == 0 || n != bank.size()) { std::fprintf(stderr, "gen_bank: translated %zu presets\n", n); return 1; }
  horde::LevelReport report;
  if (!horde::level_bank(bank, report)) { std::fprintf(stderr, "gen_bank: level_bank refused\n"); return 1; }

  std::ofstream out(argv[2]);
  char number[64];
  for (std::size_t i = 0; i < bank.size(); ++i) {
    out << "static const PresetRoute kRoutes" << i << "[] = {";
    for (const auto& r : bank[i].routes) {
      std::snprintf(number, sizeof number, "%.17g", r.octaves);
      out << "{" << r.env << ", \"" << r.key << "\", " << number << "}, ";
    }
    out << "{0, nullptr, 0}};\n";
  }
  out << "static const EmbeddedPreset kBank[] = {\n";
  for (std::size_t i = 0; i < bank.size(); ++i) {
    // A raw string with a delimiter no state document contains, since a state is JSON and may quote anything.
    std::snprintf(number, sizeof number, "%.17g", bank[i].glide_s);
    out << "{R\"HORDEBANK(" << bank[i].name << ")HORDEBANK\", R\"HORDEBANK(" << bank[i].state << ")HORDEBANK\", kRoutes" << i << ", "
        << bank[i].routes.size() << ", " << number << ", " << (bank[i].glide_always ? "true" : "false") << "},\n";
  }
  out << "};\n";
  std::printf("gen_bank: %zu presets levelled, median %.2f LUFS\n", bank.size(), report.median_lufs);
  return out.good() ? 0 : 1;
}
