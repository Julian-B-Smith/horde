// The blade engine's archive renders (B706, ADR-406 ruling 5): every factory preset on the engine's four phrases,
// through the engine's own offline entry, so a rebuild from the `blade-engine-final` tags can be checked bit for bit.
//   archive_render <data/bank/presets.json> <out dir> <phrase file>...
// Each phrase file is one object of data/phrases/phrases.json (render.sh splits them). For each preset, in bank order:
//   - translate_bank, then level_bank over the whole bank: the levelled states the plug-in embeds (gen_bank.cpp does
//     the same two calls), so the renders sound at the level the human hears;
//   - horde_offline_render at 44.1 kHz, the preset's blade-envelope routes passed as its routes, the phrase's length
//     plus a 1.5 s tail;
//   - a 32-bit float WAV, the entry's output unchanged (no dither, no normalising), so a hash of the file is a hash of
//     the engine's samples.
// What it does not play: a gliding preset's glide. The phrases carry no pitch runs, and writing them is item 15b's job,
// not this one's; the plug-in plays glide (10.3), so these renders differ from the plug-in on the bank's gliding presets.
// They pin the engine's output for a rebuild check; the installed bundles are the binary of record for listening.
// Deterministic: fixed events, no clock read, one engine per render (the entry builds its own).
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "horde_engine.h"

namespace {

std::string slurp(const char* path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream s;
  s << in.rdbuf();
  return s.str();
}

// A file name from a preset name: lower case letters and digits, every other run of characters one '-'.
std::string slug(const std::string& name) {
  std::string out;
  for (char c : name) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (std::isalnum(u)) out += static_cast<char>(std::tolower(u));
    else if (!out.empty() && out.back() != '-') out += '-';
  }
  while (!out.empty() && out.back() == '-') out.pop_back();
  return out;
}

// The phrase's id and length, read from its own text: a flat search is enough for phrases.json's fixed layout, whose
// "id" and "length_samples" members appear once each per phrase object, before its events.
bool phrase_header(const std::string& text, std::string& id, long& length) {
  const auto i = text.find("\"id\"");
  const auto l = text.find("\"length_samples\"");
  if (i == std::string::npos || l == std::string::npos) return false;
  const auto q1 = text.find('"', text.find(':', i) + 1), q2 = text.find('"', q1 + 1);
  id = text.substr(q1 + 1, q2 - q1 - 1);
  length = std::strtol(text.c_str() + text.find(':', l) + 1, nullptr, 10);
  return !id.empty() && length > 0;
}

std::string routes_json(const std::vector<horde::BladeRoute>& routes) {
  std::string out = "[";
  char number[64];
  for (std::size_t i = 0; i < routes.size(); ++i) {
    std::snprintf(number, sizeof number, "%.17g", routes[i].octaves);
    out += std::string(i ? "," : "") + "{\"env\":" + std::to_string(routes[i].env) + ",\"key\":\"" + routes[i].key + "\",\"octaves\":" + number + "}";
  }
  return out + "]";
}

void put32(std::FILE* f, std::uint32_t v) { std::fwrite(&v, 4, 1, f); }
void put16(std::FILE* f, std::uint16_t v) { std::fwrite(&v, 2, 1, f); }

// WAVE_FORMAT_IEEE_FLOAT (3), stereo, 32-bit, interleaved as the entry writes it. Little-endian host (Apple Silicon).
bool write_wav(const std::string& path, const std::vector<float>& lr, std::uint32_t rate) {
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  const std::uint32_t bytes = static_cast<std::uint32_t>(lr.size() * sizeof(float));
  std::fwrite("RIFF", 1, 4, f); put32(f, 36 + bytes); std::fwrite("WAVE", 1, 4, f);
  std::fwrite("fmt ", 1, 4, f); put32(f, 16); put16(f, 3); put16(f, 2); put32(f, rate); put32(f, rate * 8); put16(f, 8); put16(f, 32);
  std::fwrite("data", 1, 4, f); put32(f, bytes);
  std::fwrite(lr.data(), sizeof(float), lr.size(), f);
  return std::fclose(f) == 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) { std::fprintf(stderr, "usage: archive_render <presets.json> <out dir> <phrase file>...\n"); return 2; }
  std::vector<horde::BankPreset> bank;
  const std::size_t n = horde::translate_bank(slurp(argv[1]), bank);
  if (n == 0 || n != bank.size()) { std::fprintf(stderr, "archive_render: translated %zu presets\n", n); return 1; }
  horde::LevelReport report;
  if (!horde::level_bank(bank, report)) { std::fprintf(stderr, "archive_render: level_bank refused\n"); return 1; }
  const std::string dir = argv[2];
  constexpr std::uint32_t kRate = 44100;
  constexpr long kTail = 66150;   // 1.5 s at 44.1 kHz
  int written = 0;
  for (int p = 3; p < argc; ++p) {
    const std::string phrase = slurp(argv[p]);
    std::string id;
    long length = 0;
    if (!phrase_header(phrase, id, length)) { std::fprintf(stderr, "archive_render: %s is not a phrase\n", argv[p]); return 1; }
    const std::uint32_t frames = static_cast<std::uint32_t>(length + kTail);
    for (std::size_t i = 0; i < bank.size(); ++i) {
      std::vector<float> out(std::size_t(frames) * 2);
      const std::string routes = routes_json(bank[i].routes);
      const int code = horde_offline_render(bank[i].state.c_str(), phrase.c_str(), routes.c_str(), kRate, out.data(), frames);
      if (code != 0) { std::fprintf(stderr, "archive_render: %s on %s refused (%d)\n", bank[i].name.c_str(), id.c_str(), code); return 1; }
      char index[8];
      std::snprintf(index, sizeof index, "%02zu", i + 1);
      const std::string file = dir + "/bank-" + index + "-" + slug(bank[i].name) + "-" + id + ".wav";
      if (!write_wav(file, out, kRate)) { std::fprintf(stderr, "archive_render: cannot write %s\n", file.c_str()); return 1; }
      ++written;
    }
  }
  std::printf("archive_render: %d files, %zu presets, median %.2f LUFS\n", written, bank.size(), report.median_lufs);
  return 0;
}
