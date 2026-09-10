/*
 * statefix_check — the state-fixture corpus loads and renders bit-identical,
 * forever (B100 item 4).
 *
 *   statefix_check <dir>      (default: tests/state_fixtures next to the repo
 *                              root is NOT guessed — pass the path)
 *
 * For every fixture in <dir> (.txt = host chunk, .json = preset): a fresh
 * instance loads it, renders 1 s of A3 through the CLAP process path, and the
 * float32 stream must equal the stored golden byte for byte. A fixture without
 * a golden, a golden without a fixture, or an empty corpus is RED — an
 * unasserted fixture is a promise, not evidence.
 *
 * CALIBRATION (L0032): a comparison that cannot fail proves nothing, so every
 * fixture is also loaded with a planted masterVol change and that render must
 * DIFFER from the golden. If the plant reads identical, the comparison is
 * blind (or the golden is silence) and the check is RED.
 *
 * Bit-identity is asserted on THIS toolchain. Whether it holds across
 * compilers is measured, not assumed — see the corpus README.
 */

#include <algorithm>
#include <filesystem>
#include "statefix_common.h"

namespace fs = std::filesystem;
using namespace statefix;

namespace
{

int g_failures = 0;
void check(bool ok, const std::string &what)
{
  std::printf("%s %s\n", ok ? "OK  " : "FAIL", what.c_str());
  if (!ok) g_failures++;
}

bool loadAny(const clap_plugin_t *p, const std::string &name, const std::string &blob)
{
  return isChunk(name) ? loadChunk(p, blob) : loadJson(p, blob);
}

/* The fixture with masterVol moved. Chunk: a later line wins, so append.
   JSON: the shell's find() takes the FIRST occurrence, so insert at the head
   of the params object. */
std::string planted(const std::string &name, const std::string &blob)
{
  const char *key = "masterVol";
  double cur = -1;
  const std::string needle = isChunk(name) ? std::string("\n") + key + "=" : std::string("\"") + key + "\":";
  const size_t at = blob.find(needle);
  if (at != std::string::npos) cur = std::atof(blob.c_str() + at + needle.size());
  const double v = cur > 0.6 ? 0.3 : 0.9;
  char buf[64];
  if (isChunk(name))
  {
    std::snprintf(buf, sizeof buf, "%s=%.17g\n", key, v);
    return blob + buf;
  }
  std::snprintf(buf, sizeof buf, "\"%s\":%.17g,", key, v);
  const size_t po = blob.find("\"params\":{");
  return po == std::string::npos ? blob : std::string(blob).insert(po + 10, buf);
}

void report(const std::vector<float> &got, const std::string &goldBytes)
{
  const size_t n = std::min(got.size(), goldBytes.size() / sizeof(float));
  const float *want = (const float *)goldBytes.data();
  size_t first = n;
  double maxd = 0;
  for (size_t i = 0; i < n; i++)
  {
    if (got[i] != want[i] && first == n) first = i;
    maxd = std::max(maxd, (double)std::fabs(got[i] - want[i]));
  }
  if (got.size() * sizeof(float) != goldBytes.size())
    std::printf("     length: got %zu floats, golden %zu\n", got.size(), goldBytes.size() / sizeof(float));
  if (first < n)
    std::printf("     first difference at frame %zu (%s), max |diff| %.3g\n", first / 2,
                first % 2 ? "R" : "L", maxd);
}

}  // namespace

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    std::fprintf(stderr, "usage: statefix_check <fixture-dir>\n");
    return 2;
  }
  const fs::path dir = argv[1];
  if (!fs::is_directory(dir))
  {
    std::fprintf(stderr, "statefix_check: %s is not a directory\n", dir.string().c_str());
    return 2;
  }
  hypersaw_entry_init("");

  std::vector<std::string> names;
  for (const auto &e : fs::directory_iterator(dir))
  {
    const std::string n = e.path().filename().string();
    if (isChunk(n) || isJson(n)) names.push_back(n);
    else if (n.size() > 4 && n.compare(n.size() - 4, 4, ".f32") == 0)
    {
      const std::string stem = n.substr(0, n.size() - 4);
      check(fs::exists(dir / (stem + ".txt")) || fs::exists(dir / (stem + ".json")),
            "golden has its fixture: " + n);
    }
  }
  std::sort(names.begin(), names.end());   // directory order is not a contract
  check(!names.empty(), "corpus is not empty");

  for (const std::string &name : names)
  {
    const std::string path = (dir / name).string();
    std::string blob, gold;
    if (!readFile(path, blob)) { check(false, "readable: " + name); continue; }
    if (!readFile(goldenFor(path), gold)) { check(false, "golden present: " + name); continue; }

    const clap_plugin_t *p = makePlugin();
    const bool loaded = loadAny(p, name, blob);
    check(loaded, "loads: " + name);
    std::vector<float> got;
    render(p, got);
    p->destroy(p);
    const bool same = got.size() * sizeof(float) == gold.size() &&
                      std::memcmp(got.data(), gold.data(), gold.size()) == 0;
    check(same, "renders bit-identical to golden: " + name);
    if (!same) report(got, gold);

    // Calibration: the same comparison must be ABLE to fail on this fixture.
    const clap_plugin_t *q = makePlugin();
    const bool pl = loadAny(q, name, planted(name, blob));
    std::vector<float> alt;
    render(q, alt);
    q->destroy(q);
    const bool differs = alt.size() * sizeof(float) != gold.size() ||
                         std::memcmp(alt.data(), gold.data(), gold.size()) != 0;
    check(pl && differs, "control: planted masterVol change renders differently: " + name);
  }

  hypersaw_entry_deinit();
  std::printf("statefix_check: %s (%zu fixture%s, %d failure%s)\n", g_failures ? "RED" : "GREEN",
              names.size(), names.size() == 1 ? "" : "s", g_failures, g_failures == 1 ? "" : "s");
  return g_failures ? 1 : 0;
}
