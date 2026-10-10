/*
 * identity_store_check — the built plugin uses ITS OWN preset folder and leaves
 * the other identity's alone, shown by a run (B456).
 *
 *   identity_store_check real|test      (macOS only)
 *
 * WHY. The same sources build as the real plugin or as a side-by-side TEST
 * identity (CMake option HORDE_TEST_IDENTITY, src/plugin_identity.h). The test
 * build exists so unmerged code can run in a host next to the real plugin, and
 * it is only safe if it can never read or write the user's real preset folder.
 * tools/test_identity_check.py proves the two folder names differ in the
 * sources; this proves what a built binary does with a disk.
 *
 * HOW. HOME points at a scratch directory this check creates, so no real store
 * is reachable. The OTHER identity's folder is planted there first, holding a
 * preset, a prefs file and a factory stamp that names the current bank. Then
 * the shipped path runs end to end, with no test hook on the store: the real
 * plugin from its CLAP factory, its GUI on an off-screen WKWebView (opening it
 * installs the factory bank), and the page's own bridge calls (list, save,
 * load, delete), plus the forensic dump, which also writes under the store.
 *
 * Rows:
 *   ID      the factory's descriptor carries the expected CLAP id and name.
 *   OWN     the store appears at .../LiftedTruck/<this identity's folder>: the
 *           whole factory bank is installed there, a saved preset lands there
 *           and loads back, the dump lands there.
 *   OTHER   the planted folder is untouched: same files, same bytes, same
 *           modification times. Its preset is not listed, does not load and is
 *           not deleted through the bridge. Its stamp did not stop the install:
 *           had the plugin read that folder, it would have found "this bank is
 *           already installed" and written no factory file of its own.
 *   ONLY    under LiftedTruck there are exactly the two folders.
 *   CONTROL the same observations judged against the OTHER identity's table
 *           must fail ID and OWN. A check that passes for both identities is
 *           blind, and is RED.
 *
 * THE EXPECTED VALUES ARE THIS FILE'S OWN, not read from plugin_identity.h: a
 * check that learned the folder name from the code under test would agree with
 * whatever that code did. "Not read" is shown by behaviour (the three OTHER
 * observations that a read would change), not by tracing system calls.
 *
 * Quiet on green except its last line; -v prints every row. Waits are bounded;
 * a timeout is a FAIL.
 * WIRED: ./verify full (macOS only; `real`, on the default build)
 */

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include <clap/clap.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "../src/hypersaw_clap_entry.h"
#include "factory_bank.h"   // generated: kFactoryBank_count, kFactoryBank_version (the planted stamp names it)

namespace
{
namespace fs = std::filesystem;

struct Want
{
  const char *word, *clapId, *name, *folder;
};
// Independent copies of both identities (see the header). test_identity_check.py
// lists this file as one of the two allowed to name the test identity.
const Want kRealWant = {"real", "com.lifted-truck.hypersaw", "horde", "HYPERSAW"};
const Want kTestWant = {"test", "com.lifted-truck.hypersaw.test", "horde TEST", "HYPERSAW-TEST"};

const void *hostExt(const clap_host_t *, const char *) { return nullptr; }
void hostNoop(const clap_host_t *) {}
const clap_host_t kHost = {CLAP_VERSION, nullptr, "identity_store_check", "", "", "1.0",
                           hostExt, hostNoop, hostNoop, hostNoop};

/* Runs `body` as an async function in the page and returns its string result. */
std::string run(WKWebView *wk, const std::string &body, double timeout = 8.0)
{
  __block bool done = false;
  __block std::string out;
  [wk callAsyncJavaScript:[NSString stringWithUTF8String:body.c_str()]
                arguments:nil
                  inFrame:nil
           inContentWorld:WKContentWorld.pageWorld
        completionHandler:^(id result, NSError *error) {
          if (error)
            out = std::string("ERROR ") + ([[error description] UTF8String] ?: "");
          else if ([result isKindOfClass:[NSString class]])
            out = [(NSString *)result UTF8String] ?: "";
          done = true;
        }];
  NSDate *until = [NSDate dateWithTimeIntervalSinceNow:timeout];
  while (!done && [until timeIntervalSinceNow] > 0)
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
  return done ? out : "ERROR timed out";
}

/* Every file under `dir` as "relative path -> size, modification time, FNV-1a
   of the bytes". Bytes AND times: a rewrite with identical content is still a
   write. Empty when the directory does not exist. */
std::map<std::string, std::string> snapshot(const fs::path &dir)
{
  std::map<std::string, std::string> out;
  std::error_code ec;
  for (auto &e : fs::recursive_directory_iterator(dir, ec))
  {
    if (!e.is_regular_file(ec)) continue;
    std::ifstream f(e.path(), std::ios::binary);
    uint64_t h = 1469598103934665603ull;
    size_t n = 0;
    for (char c; f.get(c); n++) h = (h ^ (unsigned char)c) * 1099511628211ull;
    const auto t = fs::last_write_time(e.path(), ec).time_since_epoch().count();
    out[fs::relative(e.path(), dir, ec).generic_string()] =
        std::to_string(n) + " bytes, mtime " + std::to_string((long long)t) + ", fnv " + std::to_string(h);
  }
  return out;
}

struct Seen   // what the run observed; judged afterwards against a Want
{
  std::string clapId, name;
  std::vector<std::string> storeFolders;   // directory names under LiftedTruck after the run
  fs::path storeParent, dumpPath;
  std::string saveOk, loadBack, listed;
  bool booted = false;
};

int g_failures = 0;
std::vector<std::string> g_report;
void row(bool ok, const char *tag, const std::string &what)
{
  if (!ok) g_failures++;
  g_report.push_back(std::string(ok ? "PASS " : "FAIL ") + tag + "  " + what);
}

/* The identity rows, as booleans, so the CONTROL can judge the same run
   against the other table without printing a second set of rows. */
struct Verdict
{
  bool id, ownInstalled, ownSaved, ownDump;
};
Verdict judge(const Seen &s, const Want &w)
{
  const fs::path own = s.storeParent / w.folder;
  std::error_code ec;
  size_t factory = 0;
  for (auto &e : fs::recursive_directory_iterator(own / "presets" / "factory", ec))
    if (e.path().extension() == ".json") factory++;
  const std::string dump = s.dumpPath.generic_string(), logs = (own / "logs").generic_string() + "/";
  return {s.clapId == w.clapId && s.name == w.name, kFactoryBank_count > 0 && factory == kFactoryBank_count,
          fs::exists(own / "presets" / "identity-probe.json", ec), dump.rfind(logs, 0) == 0};
}
}  // namespace

int main(int argc, char **argv)
{
  bool verbose = false;
  const Want *want = nullptr, *other = nullptr;
  for (int i = 1; i < argc; i++)
  {
    const std::string a = argv[i];
    if (a == "-v") verbose = true;
    else if (a == "real") { want = &kRealWant; other = &kTestWant; }
    else if (a == "test") { want = &kTestWant; other = &kRealWant; }
  }
  if (!want)
  {
    std::fprintf(stderr, "usage: identity_store_check real|test [-v]\n");
    return 2;
  }
  @autoreleasepool
  {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];

    // Scratch HOME. presetRoot() reads HOME on every call (preset_store.h), so
    // from here on no real per-user store is reachable from this process.
    std::string tmpl = (fs::temp_directory_path() / "identity_store_check.XXXXXX").string();
    if (!mkdtemp(tmpl.data())) { std::printf("FAIL setup: mkdtemp\n"); return 1; }
    const fs::path home = tmpl;
    setenv("HOME", tmpl.c_str(), 1);
    Seen s;
    s.storeParent = home / "Library" / "Application Support" / "LiftedTruck";

    // Plant the OTHER identity's folder, then record it.
    const fs::path planted = s.storeParent / other->folder;
    fs::create_directories(planted / "presets" / "factory");
    fs::create_directories(planted / "prefs");
    std::ofstream(planted / "presets" / "OTHER-SENTINEL.json") << "{\"sentinel\":true}";
    std::ofstream(planted / "prefs" / "gui.json") << "{\"schemeIx\":3,\"modeIx\":1}";
    std::ofstream(planted / "presets" / "factory" / "factory.version") << "version " << kFactoryBank_version << "\n";
    const auto before = snapshot(planted);

    /* ---------------- the shipped plugin ---------------- */
    hypersaw_entry_init("");
    auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    const clap_plugin_descriptor_t *d = factory->get_plugin_descriptor(factory, 0);
    s.clapId = d->id;
    s.name = d->name;
    const clap_plugin_t *p = factory->create_plugin(factory, &kHost, d->id);
    p->init(p);
    auto *gui = (const clap_plugin_gui_t *)p->get_extension(p, CLAP_EXT_GUI);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 980, 720)
                                                styleMask:NSWindowStyleMaskBorderless
                                                  backing:NSBackingStoreBuffered
                                                    defer:YES];
    clap_window_t cw{};
    cw.api = CLAP_WINDOW_API_COCOA;
    cw.cocoa = (__bridge void *)win.contentView;
    const bool created = gui && gui->create(p, CLAP_WINDOW_API_COCOA, false) && gui->set_parent(p, &cw);
    WKWebView *wk = created ? (WKWebView *)win.contentView.subviews.firstObject : nil;
    if (wk && [wk isKindOfClass:[WKWebView class]])
    {
      NSDate *until = [NSDate dateWithTimeIntervalSinceNow:15.0];
      while (!s.booted && [until timeIntervalSinceNow] > 0)
        s.booted = run(wk, "return String(document.readyState === 'complete'"
                           " && typeof window.hzPresetList === 'function');", 2.0) == "true";
    }
    row(s.booted, "BOOT", s.booted ? "the plugin GUI loaded and its store bridge is bound"
                                   : "the plugin GUI did not load within 15 s");
    if (s.booted)
    {
      s.saveOk = run(wk, "return String(await window.hzPresetSave('presets', 'identity-probe', '{\"probe\":1}'));");
      s.loadBack = run(wk, "return String(await window.hzPresetLoad('presets', 'identity-probe'));");
      s.listed = run(wk, "const o = await window.hzPresetList();"
                         "return JSON.stringify({ own: o.presets.includes('identity-probe'),"
                         " sentinel: o.presets.includes('OTHER-SENTINEL'), factory: o.factory.length });");
      const std::string otherLoad = run(wk, "return String(await window.hzPresetLoad('presets', 'OTHER-SENTINEL'));");
      const std::string otherDel = run(wk, "return String(await window.hzPresetDelete('presets', 'OTHER-SENTINEL'));");
      row(otherLoad.empty(), "OTHER", "the planted preset does not load through the bridge: '" + otherLoad + "'");
      row(otherDel == "false", "OTHER", "the planted preset is not deleted through the bridge: " + otherDel);
    }
    if (const char *dump = hypersaw_test_dump_forensics(p, "identity_store_check")) s.dumpPath = dump;
    if (gui) gui->destroy(p);
    p->destroy(p);

    /* ---------------- what is on the disk now ---------------- */
    std::error_code ec;
    for (auto &e : fs::directory_iterator(s.storeParent, ec))
      if (e.is_directory(ec)) s.storeFolders.push_back(e.path().filename().string());
    std::sort(s.storeFolders.begin(), s.storeFolders.end());
    const auto after = snapshot(planted);

    const Verdict v = judge(s, *want);
    const std::string ownDir = (s.storeParent / want->folder).generic_string().substr(home.generic_string().size());
    row(v.id, "ID", "descriptor is '" + s.clapId + "' / '" + s.name + "', expected '" + want->clapId + "' / '" +
                        want->name + "'");
    row(v.ownInstalled, "OWN", "all " + std::to_string(kFactoryBank_count) + " factory presets are installed under $HOME" +
                                   ownDir + " (the planted stamp in the other folder did not stop it)");
    row(v.ownSaved && s.saveOk == "true" && s.loadBack == "{\"probe\":1}", "OWN",
        "a preset saved through the bridge is a file there and loads back: save=" + s.saveOk + " load='" + s.loadBack + "'");
    row(v.ownDump, "OWN", "the forensic dump landed in its logs/: $HOME" +
                              s.dumpPath.generic_string().substr(std::min(home.generic_string().size(),
                                                                          s.dumpPath.generic_string().size())));
    row(s.listed == "{\"own\":true,\"sentinel\":false,\"factory\":" + std::to_string(kFactoryBank_count) + "}", "OTHER",
        "the listing shows this store's preset and bank and not the planted preset: " + s.listed);
    row(!before.empty() && before == after, "OTHER",
        "the planted " + std::string(other->folder) + " folder is unchanged: " + std::to_string(before.size()) +
            " files, same bytes and modification times");
    std::string folders;
    for (auto &f : s.storeFolders) folders += (folders.empty() ? "" : ", ") + f;
    std::vector<std::string> expectFolders = {want->folder, other->folder};
    std::sort(expectFolders.begin(), expectFolders.end());
    row(s.storeFolders == expectFolders, "ONLY", "folders under LiftedTruck after the run: " + folders);

    const Verdict c = judge(s, *other);
    row(!c.id && !c.ownInstalled && !c.ownSaved && !c.ownDump, "CONTROL",
        std::string("judged as the ") + other->word + " identity, the same run fails ID and all three OWN rows");

    // Only ever the directory mkdtemp made above.
    if (home.filename().string().rfind("identity_store_check.", 0) == 0) fs::remove_all(home, ec);
  }
  for (auto &l : g_report)
    if (verbose || l.rfind("FAIL", 0) == 0) std::printf("%s\n", l.c_str());
  if (g_failures)
  {
    std::printf("identity_store_check: FAILED (%d of %zu rows) for the %s identity\n", g_failures, g_report.size(),
                want->word);
    return 1;
  }
  std::printf("identity_store_check: GREEN (%zu rows: the %s build's store is LiftedTruck/%s; the planted %s folder "
              "was neither read nor written; control fired)\n",
              g_report.size(), want->word, want->folder, other->folder);
  return 0;
}
