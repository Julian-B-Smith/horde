/*
 * presetstore_check — B129 acceptance (4). Standalone, unwired (human gate).
 *
 * Proves the four things the store must do and the one it must refuse:
 *   1. presetRoot() resolves per PLATFORM from the environment. All three
 *      mappings are asserted on whatever machine runs this, because
 *      presetRootFor() takes the platform as a value; an #ifdef'd helper
 *      would be testable only by owning three machines.
 *   2. First open installs every embedded factory file; the second open
 *      writes nothing.
 *   3. A user's EDIT of a factory file survives a newer bank, and so does a
 *      user's DELETION — the newer bank adds only genuinely new files.
 *   4. SAVE into the factory tier is refused, and no name can escape the
 *      store directory (L0036: pin the refusal or it reopens).
 *
 * Detector discipline (L0032): every count assertion has a case that must
 * read EXACTLY zero — an empty bank must write 0 files and create nothing at
 * all — paired with a case on the same corpus that must read NON-zero, so the
 * zero can actually fail. The edit-survival check compares bytes, not mtimes.
 *
 * Build: it is part of the normal CMake configure; run the binary directly.
 */
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "gui/preset_store.h"
#include "factory_bank.h"  // generated; kFactoryBank_count may legitimately be 0

namespace fs = std::filesystem;
using namespace hypersaw;

static int failures = 0;
static int checks = 0;

static void ok(bool cond, const std::string &what)
{
  checks++;
  if (cond) return;
  failures++;
  std::printf("  FAIL  %s\n", what.c_str());
}

static void eq(const std::string &got, const std::string &want, const std::string &what)
{
  checks++;
  if (got == want) return;
  failures++;
  std::printf("  FAIL  %s\n        got  %s\n        want %s\n", what.c_str(), got.c_str(),
              want.c_str());
}

static void eqN(std::size_t got, std::size_t want, const std::string &what)
{
  checks++;
  if (got == want) return;
  failures++;
  std::printf("  FAIL  %s (got %zu, want %zu)\n", what.c_str(), got, want);
}

static void setEnv(const char *k, const char *v)
{
#ifdef _WIN32
  _putenv_s(k, v ? v : "");
#else
  if (v)
    setenv(k, v, 1);
  else
    unsetenv(k);
#endif
}

static std::string slurp(const fs::path &p)
{
  std::ifstream f(p, std::ios::binary);
  if (!f) return "<MISSING>";
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

static void writeFile(const fs::path &p, const std::string &s)
{
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  f << s;
}

// generic_string() so the expectations below read the same on every platform.
static std::string g(const fs::path &p) { return p.generic_string(); }

int main()
{
  std::printf("presetstore_check — B129 store path + factory install\n");

  /* ---- 1. the path helper, all three platforms, from the environment ----
     The stand-in home directories deliberately do NOT look like a real user
     directory on any platform: the leak gate in ./verify greps tracked files
     for machine-absolute paths and cannot tell a fixture from an identity
     leak — and it is right not to try, so fixtures stay obviously synthetic. */
  const char *kHome = "/stand-in-home";
  const char *kAppData = "Q:/stand-in-roaming";
  eq(g(presetRootFor(StorePlatform::macOS, kHome, nullptr)),
     "/stand-in-home/Library/Application Support/LiftedTruck/HYPERSAW", "macOS root");
  eq(g(presetRootFor(StorePlatform::Windows, nullptr, kAppData)),
     "Q:/stand-in-roaming/LiftedTruck/HYPERSAW", "Windows root");
  eq(g(presetRootFor(StorePlatform::Other, kHome, nullptr)),
     "/stand-in-home/.local/share/LiftedTruck/HYPERSAW", "Linux root");
  // The must-read-empty control: no home variable => no root, never "/".
  ok(presetRootFor(StorePlatform::macOS, nullptr, kAppData).empty(), "macOS: no HOME => empty");
  ok(presetRootFor(StorePlatform::macOS, "", nullptr).empty(), "macOS: empty HOME => empty");
  ok(presetRootFor(StorePlatform::Windows, kHome, nullptr).empty(),
     "Windows: no APPDATA => empty (HOME must not stand in)");
  ok(presetRootFor(StorePlatform::Other, nullptr, nullptr).empty(), "Linux: no HOME => empty");

  const fs::path tmp = fs::temp_directory_path() / "hypersaw_presetstore_check";
  std::error_code ec;
  fs::remove_all(tmp, ec);
  fs::create_directories(tmp, ec);

  // presetRoot() itself, driven through the real environment on THIS platform.
  {
    const std::string home = g(tmp / "home");
    setEnv("HOME", home.c_str());
    setEnv("APPDATA", g(tmp / "appdata").c_str());
    const fs::path want = presetRootFor(kStorePlatform, home.c_str(), g(tmp / "appdata").c_str());
    eq(g(presetRoot()), g(want), "presetRoot() reads the environment");
    ok(!want.empty(), "the build platform's root is non-empty with env set");
  }

  /* ---- 2. the EMPTY bank is a total no-op (the must-read-zero control) ---- */
  {
    const fs::path root = tmp / "empty";
    const FactoryFile none[] = {{"", "", nullptr, 0}};
    auto r = installFactoryBank(root, none, 0, "v-empty");
    eqN(r.written, 0, "empty bank writes 0 files");
    ok(!r.stampWritten, "empty bank writes no stamp");
    ok(!fs::exists(root), "empty bank creates NO directory at all");
    // Paired non-zero on the same root, so the zero above can actually fail.
    const std::string body = "{\"a\":1}";
    const FactoryFile one[] = {{"lead", "probe", (const unsigned char *)body.data(), body.size()}};
    auto r2 = installFactoryBank(root, one, 1, "v-empty");
    eqN(r2.written, 1, "the SAME root with one file writes 1 (the zero can fail)");
    ok(fs::exists(root / "presets" / "factory" / "lead" / "probe.json"), "that file landed");
  }

  /* ---- 3. install semantics on a synthetic bank ---- */
  const std::string a1 = "{\"n\":\"alpha\"}";
  const std::string b1 = "{\"n\":\"beta\"}";
  const std::string c1 = "{\"n\":\"gamma\"}";
  const std::string d1 = "{\"n\":\"delta\"}";
  const FactoryFile bankV1[] = {
      {"lead", "alpha", (const unsigned char *)a1.data(), a1.size()},
      {"bass", "beta", (const unsigned char *)b1.data(), b1.size()},
      {"", "gamma", (const unsigned char *)c1.data(), c1.size()},
  };
  const FactoryFile bankV2[] = {
      {"lead", "alpha", (const unsigned char *)a1.data(), a1.size()},
      {"bass", "beta", (const unsigned char *)b1.data(), b1.size()},
      {"", "gamma", (const unsigned char *)c1.data(), c1.size()},
      {"pad", "delta", (const unsigned char *)d1.data(), d1.size()},
  };

  const fs::path root = tmp / "store";
  const fs::path fdir = root / "presets" / "factory";
  {
    auto r = installFactoryBank(root, bankV1, 3, "v1");
    eqN(r.written, 3, "first open installs every embedded file");
    ok(r.stampWritten, "first open writes factory.version");
    eq(slurp(fdir / "lead" / "alpha.json"), a1, "alpha content");
    eq(slurp(fdir / "bass" / "beta.json"), b1, "beta content");
    eq(slurp(fdir / "gamma.json"), c1, "uncategorised file lands at the factory root");
  }
  {
    auto r = installFactoryBank(root, bankV1, 3, "v1");
    eqN(r.written, 0, "second open writes nothing");
    ok(!r.stampWritten, "second open does not even rewrite the stamp");
  }

  /* ---- 4. a user's EDIT survives a newer bank; only new files are added ---- */
  const std::string edited = "{\"n\":\"alpha\",\"mine\":true}";
  writeFile(fdir / "lead" / "alpha.json", edited);
  {
    auto r = installFactoryBank(root, bankV2, 4, "v2");
    eqN(r.written, 1, "a newer bank adds ONLY the new file");
    eq(slurp(fdir / "lead" / "alpha.json"), edited, "the user's edit survives the re-install");
    eq(slurp(fdir / "pad" / "delta.json"), d1, "the new file arrived");
  }

  /* ---- 5. a user's DELETION survives a newer bank ---- */
  fs::remove(fdir / "bass" / "beta.json", ec);
  {
    // v3 is the same four files under a new id: nothing new, and the deleted
    // one must NOT come back — the ledger is what tells "thrown away" from
    // "never shipped to you".
    auto r = installFactoryBank(root, bankV2, 4, "v3");
    eqN(r.written, 0, "a newer bank resurrects nothing the user deleted");
    ok(!fs::exists(fdir / "bass" / "beta.json"), "the deleted factory file stays deleted");
  }

  /* ---- 6. listFactory reflects the DISK, not the embedded array ---- */
  {
    auto names = listFactory(root);
    const std::vector<std::string> want = {"gamma", "lead/alpha", "pad/delta"};
    eqN(names.size(), want.size(), "listFactory count after one deletion");
    for (std::size_t i = 0; i < want.size() && i < names.size(); i++)
      eq(names[i], want[i], "listFactory entry " + std::to_string(i));
    ok(std::find(names.begin(), names.end(), std::string("bass/beta")) == names.end(),
       "the deleted preset is absent from the list");
    ok(std::find(names.begin(), names.end(), std::string("factory")) == names.end(),
       "factory.version is not listed as a preset");
  }

  /* ---- 7. the refusals (L0036) ---- */
  {
    ok(!saveAllowed("factory"), "SAVE into the factory tier is refused");
    ok(saveAllowed("presets") && saveAllowed("corners") && saveAllowed("prefs"),
       "the three user tiers still accept a save");
    ok(!saveAllowed("logs") && !saveAllowed(""), "an unknown tier is refused");

    // No name reaches outside the store. Sanitising DROPS the characters, so
    // an escape attempt becomes an ordinary (ugly) file name inside the dir.
    for (const char *evil : {"../../../etc/passwd", "..", "/etc/passwd", "a/../../b",
                             "..\\..\\windows", "."})
    {
      const auto p = storeFile(root, "presets", evil);
      const std::string s = g(p);
      ok(p.empty() || s.rfind(g(root / "presets") + "/", 0) == 0,
         std::string("name cannot escape the store: ") + evil);
      ok(s.find("..") == std::string::npos, std::string("no '..' survives: ") + evil);
    }
    ok(storeFile(root, "presets", "!!!").empty(), "an all-illegal name is refused outright");
    ok(storeFile(root, "nope", "x").empty(), "an unknown kind resolves to nothing");
    ok(storeFile(fs::path(), "presets", "x").empty(), "an empty root resolves to nothing");
    eq(g(storeFile(root, "factory", "lead/alpha")), g(fdir / "lead" / "alpha.json"),
       "a factory name keeps its one category separator");
    eq(g(storeFile(root, "factory", "a/b/c")), g(fdir / "a" / "bc.json"),
       "a second separator is dropped, not honoured");
  }

  /* ---- 8. the REAL embedded bank (0 files while B130 is in flight) ---- */
  {
    const fs::path r2 = tmp / "real";
    auto r = installFactoryBank(r2, kFactoryBank, kFactoryBank_count, kFactoryBank_version);
    eqN(r.written, kFactoryBank_count, "the embedded bank installs every file it carries");
    auto again = installFactoryBank(r2, kFactoryBank, kFactoryBank_count, kFactoryBank_version);
    eqN(again.written, 0, "and installs nothing the second time");
    eqN(listFactory(r2).size(), kFactoryBank_count, "every embedded file is listed");
    std::printf("  embedded bank: %zu file(s), version %s\n", (std::size_t)kFactoryBank_count,
                kFactoryBank_version);
    if (kFactoryBank_count == 0)
      std::printf("  NOTE: the bank is empty (B130 in flight) — cases 2-7 carry the semantics.\n");
  }

  fs::remove_all(tmp, ec);
  std::printf("%s — %d checks, %d failure(s)\n", failures ? "FAIL" : "PASS", checks, failures);
  return failures ? 1 : 0;
}
