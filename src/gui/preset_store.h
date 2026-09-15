/*
 * preset_store.h — WHERE the on-disk store lives, and the factory-bank
 * install. The single site that spells the per-user store path (B129 / B99(b)):
 * every presets/corners/prefs bind and the shell's forensic dump root derive
 * from presetRoot(), because four hand-built copies of one path is how the
 * Windows store silently no-opped — `$HOME/Library/...` simply does not exist
 * there, `fs::create_directories` failed, and the save bind returned false
 * into a UI that had nowhere to show it.
 *
 * Deliberately dependency-free: <filesystem> and the standard library only, no
 * choc and no hypersaw_gui.h. That is what lets the shell (hypersaw_clap.cpp,
 * built on every platform) and the GUI backends (macOS/Windows only) and a
 * headless check all include the same header without dragging a webview in.
 */
#pragma once

#include <algorithm>  // MSVC needs this for std::sort/std::find, libc++ leaks it
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace hypersaw
{

/* The platform is a VALUE, not an #ifdef at the call site. presetRootFor() is
   the pure kernel — (platform, $HOME, %APPDATA%) -> path — so one check run on
   one machine can assert all three mappings, which an #ifdef'd helper makes
   untestable except by owning three machines. */
enum class StorePlatform
{
  macOS,
  Windows,
  Other
};

inline constexpr StorePlatform kStorePlatform =
#if defined(__APPLE__)
    StorePlatform::macOS;
#elif defined(_WIN32)
    StorePlatform::Windows;
#else
    StorePlatform::Other;
#endif

/* Returns an EMPTY path when the platform's home variable is unset. Callers
   must treat empty as "no store available" and fall back (the shell dumps to
   the temp dir); silently rooting at "/" would write into the filesystem root. */
inline std::filesystem::path presetRootFor(StorePlatform p, const char *home, const char *appdata)
{
  namespace fs = std::filesystem;
  const auto nonEmpty = [](const char *s) { return s && *s; };
  switch (p)
  {
    case StorePlatform::macOS:
      if (!nonEmpty(home)) return {};
      return fs::path(home) / "Library" / "Application Support" / "LiftedTruck" / "HYPERSAW";
    case StorePlatform::Windows:
      if (!nonEmpty(appdata)) return {};
      return fs::path(appdata) / "LiftedTruck" / "HYPERSAW";
    case StorePlatform::Other:
      if (!nonEmpty(home)) return {};
      return fs::path(home) / ".local" / "share" / "LiftedTruck" / "HYPERSAW";
  }
  return {};
}

/* THE path helper. Read from the environment on every call rather than cached
   in a static: a check drives it with setenv/_putenv between cases, and the
   cost is one getenv per file operation on the GUI thread. */
inline std::filesystem::path presetRoot()
{
  return presetRootFor(kStorePlatform, std::getenv("HOME"), std::getenv("APPDATA"));
}

/* ---------------------------------------------------------------- kinds ---
   "presets" / "corners" / "prefs" are user-writable directories directly under
   the root. "factory" is the read-only shipped bank, which lives INSIDE
   presets/ as presets/factory/<category>/<name>.json — a subdirectory, so the
   non-recursive listing of presets/ never mixes the two tiers. */

inline bool isUserKind(const std::string &kind)
{
  return kind == "presets" || kind == "corners" || kind == "prefs";
}

inline bool isFactoryKind(const std::string &kind) { return kind == "factory"; }

inline std::filesystem::path kindDir(const std::filesystem::path &root, const std::string &kind)
{
  if (root.empty()) return {};
  if (isFactoryKind(kind)) return root / "presets" / "factory";
  if (isUserKind(kind)) return root / kind;
  return {};
}

/* A preset name becomes a filename, so it is sanitised rather than trusted.
   Factory names carry exactly one '/' (category/name); every other separator,
   "..", and anything outside [alnum space - _] is dropped. Dropping rather
   than rejecting means "a/../b" sanitises to "ab" — a real file name, never an
   escape — and an empty result is the refusal. */
inline std::string sanitiseName(const std::string &name, bool allowCategory)
{
  std::string out;
  int slashes = 0;
  for (char c : name)
  {
    const unsigned char u = (unsigned char)c;
    const bool ok = (u >= '0' && u <= '9') || (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') ||
                    c == ' ' || c == '-' || c == '_';
    if (ok)
      out += c;
    else if (allowCategory && c == '/' && slashes == 0 && !out.empty())
    {
      out += c;
      slashes++;
    }
  }
  // A trailing '/' would resolve to a directory, not a file.
  if (!out.empty() && out.back() == '/') out.pop_back();
  return out;
}

/* Resolve a (kind, name) pair to a file path, or an empty path if the request
   is not representable. The ONLY route from a JS-supplied name to a path. */
inline std::filesystem::path storeFile(const std::filesystem::path &root, const std::string &kind,
                                       const std::string &name)
{
  const auto dir = kindDir(root, kind);
  if (dir.empty()) return {};
  const std::string safe = sanitiseName(name, isFactoryKind(kind));
  if (safe.empty()) return {};
  return dir / (safe + ".json");
}

/* SAVE into the factory tier is refused (B129 acceptance 3): the bank is the
   plugin's, and a user's copy belongs in presets/. Pinned by presetstore_check
   so the refusal cannot be "helpfully" reopened (L0036). */
inline bool saveAllowed(const std::string &kind) { return isUserKind(kind); }

/* ------------------------------------------------------------- factory ---- */

struct FactoryFile
{
  const char *category;  // "" = directly under presets/factory/
  const char *name;      // without the .json extension
  const unsigned char *bytes;
  std::size_t size;
};

/* "<category>/<name>.json", or "<name>.json" for an uncategorised file. The
   relative path is the LEDGER KEY as well as the destination, so it is built
   once here. */
inline std::string factoryRel(const FactoryFile &f)
{
  std::string rel;
  if (f.category && *f.category) rel = std::string(f.category) + "/";
  return rel + f.name + ".json";
}

struct FactoryInstall
{
  std::size_t written = 0;  // files created by this call
  std::size_t kept = 0;     // present already, or deliberately deleted before
  bool stampWritten = false;
};

/* The stamp: line 1 is "version <id>", every later line is a relative path the
   plugin has EVER installed. The ledger is what makes a user's DELETION stick:
   a file that is absent but listed was deleted on purpose, and a newer bank
   must not resurrect it. Without the ledger, "install if absent" cannot tell
   "never shipped to you" from "you threw it away". */
inline bool readFactoryStamp(const std::filesystem::path &stamp, std::string &versionOut,
                             std::vector<std::string> &installedOut)
{
  std::ifstream f(stamp);
  if (!f) return false;
  std::string line;
  if (!std::getline(f, line)) return false;
  const std::string kPrefix = "version ";
  if (line.rfind(kPrefix, 0) != 0) return false;
  versionOut = line.substr(kPrefix.size());
  while (std::getline(f, line))
  {
    if (!line.empty() && line.back() == '\r') line.pop_back();  // a stamp copied from Windows
    if (!line.empty()) installedOut.push_back(line);
  }
  return true;
}

/* Copy every embedded factory file that the user has neither got nor deleted.
   Returns counts so a check can assert "the second open writes nothing" as a
   number rather than by guessing from mtimes.

   Runs on the GUI thread at bridge-install time: file I/O, never the audio
   thread. With zero embedded files it is a pure no-op — it does not even
   create the directory — which is the state while the bank itself is in
   flight, and is also the control case presetstore_check asserts must read 0. */
inline FactoryInstall installFactoryBank(const std::filesystem::path &root, const FactoryFile *files,
                                         std::size_t count, const char *version)
{
  namespace fs = std::filesystem;
  FactoryInstall r;
  if (root.empty() || files == nullptr || count == 0) return r;

  const fs::path dir = root / "presets" / "factory";
  const fs::path stamp = dir / "factory.version";
  std::string stampVersion;
  std::vector<std::string> installed;
  const bool haveStamp = readFactoryStamp(stamp, stampVersion, installed);
  // Same bank as last time: nothing can have changed, so touch nothing at all.
  if (haveStamp && version && stampVersion == version)
  {
    r.kept = count;
    return r;
  }

  std::error_code ec;
  for (std::size_t i = 0; i < count; i++)
  {
    const std::string rel = factoryRel(files[i]);
    const bool knownBefore = std::find(installed.begin(), installed.end(), rel) != installed.end();
    const fs::path dest = dir / rel;
    if (knownBefore || fs::exists(dest, ec))
    {
      r.kept++;
      if (!knownBefore) installed.push_back(rel);
      continue;
    }
    fs::create_directories(dest.parent_path(), ec);
    std::ofstream out(dest, std::ios::binary | std::ios::trunc);
    if (!out) continue;
    if (files[i].size > 0)
      out.write(reinterpret_cast<const char *>(files[i].bytes), (std::streamsize)files[i].size);
    if (!out.good()) continue;
    out.close();
    installed.push_back(rel);
    r.written++;
  }

  fs::create_directories(dir, ec);
  std::ofstream s(stamp, std::ios::trunc);
  if (s)
  {
    s << "version " << (version ? version : "") << "\n";
    std::sort(installed.begin(), installed.end());
    installed.erase(std::unique(installed.begin(), installed.end()), installed.end());
    for (const auto &rel : installed) s << rel << "\n";
    r.stampWritten = s.good();
  }
  return r;
}

/* Every installed factory preset as "<category>/<name>", sorted — the strings
   the GUI puts in its "factory" optgroup and hands straight back to
   hzPresetLoad("factory", ...). The store on disk is the truth, not the
   embedded array: a user's deletion must disappear from the list. */
inline std::vector<std::string> listFactory(const std::filesystem::path &root)
{
  namespace fs = std::filesystem;
  std::vector<std::string> out;
  const fs::path dir = kindDir(root, "factory");
  if (dir.empty()) return out;
  std::error_code ec;
  for (auto &e : fs::recursive_directory_iterator(dir, ec))
  {
    if (ec) break;
    if (!e.is_regular_file(ec) || e.path().extension() != ".json") continue;
    const fs::path rel = fs::relative(e.path(), dir, ec);
    if (ec || rel.empty()) continue;
    std::string s = rel.generic_string();
    s.erase(s.size() - 5);  // ".json"
    out.push_back(s);
  }
  std::sort(out.begin(), out.end());
  return out;
}

}  // namespace hypersaw
