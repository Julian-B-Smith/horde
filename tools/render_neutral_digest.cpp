/*
 * render_neutral_digest — one FNV-1a digest line per idle/offline render case,
 * so a change that claims "render-neutral" is MEASURED against a saved
 * baseline instead of argued (B448 B2: the main-thread/audio-thread handoff).
 *
 * A measurement tool, not a gate: it prints digests and exits 0. Neutrality is
 * the DIFF of two runs (before and after a change), which no single run can
 * judge. Cases:
 *   fixture  every tests/state_fixtures/*.txt chunk, loaded into a fresh idle
 *            instance through state_load, rendered by statefix_common's
 *            render() (A3 held, 172 blocks of 256 at 44.1 kHz). The golden
 *            .f32's own digest is printed beside it: equal digests are the
 *            sanity row that this tool renders what statefix_check renders.
 *   preset   every docs/presets/factory/**.json, applied idle through the
 *            editor's preset path (hypersaw_debug_apply + flush), same render.
 *
 *     render_neutral_digest <repo-root>
 */

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include "statefix_common.h"

namespace fs = std::filesystem;
using namespace statefix;

namespace
{
/* FNV-1a 64 over the raw bytes: any bit that moves moves the digest, which is
   the point — this compares renders for identity, not for closeness. */
uint64_t fnv1a(const void *data, size_t n)
{
  uint64_t h = 1469598103934665603ull;
  const unsigned char *b = (const unsigned char *)data;
  for (size_t i = 0; i < n; i++)
  {
    h ^= b[i];
    h *= 1099511628211ull;
  }
  return h;
}

std::vector<std::string> sortedFiles(const fs::path &dir, const char *ext)
{
  std::vector<std::string> out;
  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator(); ++it)
    if (it->is_regular_file() && it->path().extension() == ext) out.push_back(it->path().string());
  std::sort(out.begin(), out.end());   // readdir order is not stable across filesystems
  return out;
}

std::string rel(const std::string &path, const fs::path &root)
{
  return fs::relative(path, root).generic_string();
}
}  // namespace

int main(int argc, char **argv)
{
  const fs::path root = argc > 1 ? fs::path(argv[1]) : fs::path(".");
  std::vector<float> out;
  int cases = 0;

  for (const auto &f : sortedFiles(root / "tests" / "state_fixtures", ".txt"))
  {
    std::string blob, gold;
    if (!readFile(f, blob)) continue;
    const clap_plugin_t *p = makePlugin();
    const bool ok = loadChunk(p, blob);
    render(p, out);
    p->destroy(p);
    const bool haveGold = readFile(goldenFor(f), gold);
    std::printf("fixture %016llx  golden %016llx  load=%d  %s\n",
                (unsigned long long)fnv1a(out.data(), out.size() * sizeof(float)),
                haveGold ? (unsigned long long)fnv1a(gold.data(), gold.size()) : 0ull, ok ? 1 : 0,
                rel(f, root).c_str());
    cases++;
  }

  for (const auto &f : sortedFiles(root / "docs" / "presets" / "factory", ".json"))
  {
    std::string json;
    if (!readFile(f, json)) continue;
    const clap_plugin_t *p = makePlugin();
    const bool ok = loadJson(p, json);
    render(p, out);
    p->destroy(p);
    std::printf("preset  %016llx  load=%d  %s\n",
                (unsigned long long)fnv1a(out.data(), out.size() * sizeof(float)), ok ? 1 : 0,
                rel(f, root).c_str());
    cases++;
  }

  std::printf("render_neutral_digest: %d case(s)\n", cases);
  return cases > 0 ? 0 : 1;
}
