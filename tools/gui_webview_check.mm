/*
 * gui_webview_check — the plugin GUI's web view, on a real WKWebView (B446 W3a).
 *
 *   gui_webview_check [-v]       (macOS only; -v prints every row, not only failures)
 *
 * WHY. Three properties of the GUI's web view hold only at run time, inside the
 * browser engine, so no text scan can establish them:
 *   - the view shows its embedded page and nothing else (no navigation away,
 *     and only that page's main frame reaches the plugin's bridge);
 *   - the page's Content-Security-Policy (tools/embed_file.py) refuses script
 *     that was not embedded at build time, while every binding still works;
 *   - page script cannot read the clipboard by itself, and file names reach
 *     the dropdowns as text.
 *
 * HOW. It runs the shipped path end to end: the real plugin (CLAP factory),
 * gui_create, gui_set_parent into an off-screen NSView, so the web view under
 * test is the one HypersawGui builds, with no test hook. HOME points at a
 * scratch directory holding preset and corner files with hostile names, so the
 * per-user store is never touched. Scripts are run in the page with WebKit's
 * callAsyncJavaScript, an API call outside the page's policy.
 *
 * Rows:
 *   BOOT      the page loads under its policy with zero policy violations (a
 *             listener armed at document start records them), and a bridge
 *             call resolves. This is the "the GUI still works" floor.
 *   NAMES     hzPresetList returns typed arrays carrying every hostile file name
 *             exactly; the preset and corner dropdowns show each as text; no
 *             element was created from a name and no handler in one ran.
 *   CSP       an inline <script> added at run time does not run; neither does an
 *             inline handler arriving through markup.
 *   FRAME     a message posted to the bridge from a subframe is not delivered.
 *   CLIPBOARD script-initiated paste is not supported; hzPasteState is bound.
 *   NAV       location to about:blank and to file:, a link to a remote page and
 *             window.open all leave the page where it is.
 *   CONTROL   the same detectors against a plain choc web view with none of the
 *             above (no policy, no navigation lock, choc's default clipboard
 *             preferences), plus the pre-fix dropdown pattern replayed in it.
 *             Every detector must REGISTER there (each reads the opposite of
 *             its row above). A control that registers nothing means the
 *             detector is blind, and the check is RED.
 *
 * Quiet on green except its last line. Wall-clock waits (page load, a timer
 * tick) are bounded; a timeout is a FAIL, never a pass.
 * WIRED: ./verify full (macOS only)
 */

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include <clap/clap.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../src/hypersaw_clap_entry.h"
#include "../libs/choc/choc/gui/choc_WebView.h"

namespace
{
#include "notefuzz_scaffold.inc"

int failures = 0;
std::vector<std::string> report;
std::string ctlName;   // the CONTROL view's stand-in for the shell's preset name

void row(bool ok, const char *tag, const std::string &what)
{
  if (!ok) failures++;
  report.push_back(std::string(ok ? "PASS " : "FAIL ") + tag + "  " + what);
}

void spin(double seconds)
{
  NSDate *until = [NSDate dateWithTimeIntervalSinceNow:seconds];
  while ([until timeIntervalSinceNow] > 0)
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
}

/* Runs `body` as an async function in the page's main frame and returns what it
   returned (a string; callers JSON.stringify), or "" with `err` set. */
std::string run(WKWebView *wk, const std::string &body, std::string *err = nullptr, double timeout = 8.0)
{
  __block bool done = false;
  __block std::string out, e;
  [wk callAsyncJavaScript:[NSString stringWithUTF8String:body.c_str()]
                arguments:nil
                  inFrame:nil
           inContentWorld:WKContentWorld.pageWorld
        completionHandler:^(id result, NSError *error) {
          if (error)
            e = std::string([[error description] UTF8String] ?: "error");
          else if ([result isKindOfClass:[NSString class]])
            out = [(NSString *)result UTF8String] ?: "";
          else if (result)
            out = [[result description] UTF8String] ?: "";
          done = true;
        }];
  NSDate *until = [NSDate dateWithTimeIntervalSinceNow:timeout];
  while (!done && [until timeIntervalSinceNow] > 0)
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
  if (!done) e = "timed out";
  if (err) *err = e;
  return out;
}

bool waitFor(WKWebView *wk, const std::string &cond, double timeout)
{
  NSDate *until = [NSDate dateWithTimeIntervalSinceNow:timeout];
  while ([until timeIntervalSinceNow] > 0)
  {
    if (run(wk, "return String(!!(" + cond + "));", nullptr, 2.0) == "true") return true;
    spin(0.1);
  }
  return false;
}

/* The hostile file names. Each must come back exactly, as text. */
const std::vector<std::string> kNames = {"plain", "a<img src=x onerror=window.__pwnName=1>", "q\"uote",
                                         "amp&amp;"};

std::string jsNames()
{
  std::string s = "[";
  for (size_t i = 0; i < kNames.size(); i++)
  {
    s += i ? ",\"" : "\"";
    for (char c : kNames[i]) s += (c == '"' || c == '\\') ? std::string("\\") + c : std::string(1, c);
    s += "\"";
  }
  return s + "]";
}

const char *kArmScript =
    "window.__cspv = []; window.__armed = true;"
    "document.addEventListener('securitypolicyviolation',"
    "  e => window.__cspv.push(e.effectiveDirective + ' ' + e.blockedURI));";

/* ---- detectors, run identically against both views ---------------------- */

std::string injectScript(WKWebView *wk)
{
  return run(wk, "const s = document.createElement('script'); s.textContent = 'window.__pwnScript = 1';"
                 "document.head.appendChild(s); return String(window.__pwnScript);");
}

std::string injectHandler(WKWebView *wk)
{
  run(wk, "const d = document.createElement('div');"
          "d.innerHTML = '<img src=\"x\" onerror=\"window.__pwnImg = 1\">';"
          "document.body.appendChild(d); return '';");
  spin(0.5);
  return run(wk, "return String(window.__pwnImg);");
}

std::string framePost(WKWebView *wk, const std::string &tag)
{
  std::string e;
  run(wk, "const f = document.createElement('iframe'); document.body.appendChild(f);"
          "const h = f.contentWindow.webkit && f.contentWindow.webkit.messageHandlers.external;"
          "if (!h) return 'no-handler';"
          "h.postMessage(JSON.stringify({ id: 987654, fn: 'hzPresetSetName', params: ['" + tag + "'] }));"
          "return 'posted';", &e);
  spin(0.5);
  return run(wk, "return String(await window.hzPresetName());");
}

/* Can page script trigger a paste by itself? Run outside any user gesture (the
   timer outlives WebKit's 1 s gesture propagation), so a refused paste is
   refused silently, never by asking the user. The handler cancels the event and
   never calls getData, so the probe does not read the clipboard either way.
   (queryCommandSupported('paste') is no detector: it reads true both ways.) */
std::string scriptPaste(WKWebView *wk)
{
  return run(wk, "let fired = false; const h = e => { fired = true; e.preventDefault(); };"
                 "await new Promise(r => setTimeout(r, 1300));"
                 "const t = document.createElement('textarea'); document.body.appendChild(t); t.focus();"
                 "document.addEventListener('paste', h, true);"
                 "const ret = document.execCommand('paste');"
                 "document.removeEventListener('paste', h, true); t.remove();"
                 "return JSON.stringify({ ret, fired });");
}

/* The two preferences behind it, read back from the view through WebKit's SPI
   getters (the KVC setter keys choc uses have no matching getter). */
std::string clipboardPrefs(WKWebView *wk)
{
  id pr = wk.configuration.preferences;
  auto rd = [&](NSString *k) -> std::string {
    @try { return [[[pr valueForKey:k] description] UTF8String]; } @catch (NSException *) { return "?"; }
  };
  return "javaScriptCanAccessClipboard=" + rd(@"_javaScriptCanAccessClipboard") +
         " DOMPasteAllowed=" + rd(@"_domPasteAllowed");
}

/* "page" when our page is still the document, else what replaced it. */
std::string afterNav(WKWebView *wk, const std::string &attempt)
{
  run(wk, attempt + "; return '';");
  spin(0.8);
  return run(wk, "return document.getElementById('presetList') ? 'page'"
                 " : ('replaced:' + location.href + ' bridge=' + (typeof window.hzPresetName));");
}

}  // namespace

int main(int argc, char **argv)
{
  const bool verbose = argc > 1 && std::string(argv[1]) == "-v";
  @autoreleasepool
  {
    namespace fs = std::filesystem;
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];

    // Scratch store: HOME is read on every store call (preset_store.h presetRoot).
    char tmpl[] = "/tmp/gui_webview_check.XXXXXX";
    if (!mkdtemp(tmpl)) { std::printf("FAIL setup: mkdtemp\n"); return 1; }
    const fs::path home = tmpl;
    setenv("HOME", tmpl, 1);
    const fs::path store = home / "Library" / "Application Support" / "LiftedTruck" / "HYPERSAW";
    for (const char *kind : {"presets", "corners"})
    {
      fs::create_directories(store / kind);
      for (const auto &n : kNames) std::ofstream(store / kind / (n + ".json")) << "{}";
    }

    /* ---------------- the shipped GUI ---------------- */
    hypersaw_entry_init("");
    auto *factory = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    const clap_plugin_t *p = factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
    p->init(p);
    auto *gui = (const clap_plugin_gui_t *)p->get_extension(p, CLAP_EXT_GUI);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 980, 720)
                                                styleMask:NSWindowStyleMaskBorderless
                                                  backing:NSBackingStoreBuffered
                                                    defer:YES];
    NSView *parent = win.contentView;
    clap_window_t cw{};
    cw.api = CLAP_WINDOW_API_COCOA;
    cw.cocoa = (__bridge void *)parent;
    const bool created = gui && gui->create(p, CLAP_WINDOW_API_COCOA, false) && gui->set_parent(p, &cw);
    WKWebView *wk = created ? (WKWebView *)parent.subviews.firstObject : nil;
    if (!wk || ![wk isKindOfClass:[WKWebView class]])
    {
      std::printf("FAIL setup: the plugin GUI did not produce a WKWebView\n");
      return 1;
    }
    /* Armed at document start of the page's first load. Safe to add here: the
       load cannot reach document start until the navigation policy round trip
       is served by this thread's run loop, which has not spun yet. BOOT checks
       `__armed`, so a late arming is a FAIL rather than a blind pass. */
    WKUserScript *arm = [[WKUserScript alloc] initWithSource:[NSString stringWithUTF8String:kArmScript]
                                               injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                                            forMainFrameOnly:YES];
    [wk.configuration.userContentController addUserScript:arm];

    const bool booted = waitFor(wk, "document.readyState === 'complete' && window.__armed"
                                    " && typeof window.hzGetBuild === 'function'"
                                    " && document.getElementById('presetList')", 15.0);
    row(booted, "BOOT", booted ? "page loaded, violation listener armed at document start"
                               : "page did not load (or the listener was not armed) within 15 s");
    if (booted)
    {
      std::string e;
      const std::string build = run(wk, "return String(await window.hzGetBuild());", &e);
      row(e.empty() && !build.empty(), "BOOT", "hzGetBuild resolves under the policy: '" + build + "'" + e);
      spin(1.0);   // let the poll, the dropdown fills and the first frames run
      const std::string v = run(wk, "return JSON.stringify(window.__cspv);");
      row(v == "[]", "BOOT", "policy violations while the page runs: " + v);

      // NAMES
      const std::string names = jsNames();
      const std::string listed = run(wk,
          "const o = await window.hzPresetList(); const want = " + names + ";"
          "const has = a => Array.isArray(a) && want.every(n => a.includes(n));"
          "return String(typeof o === 'object' && has(o.presets) && has(o.corners));");
      row(listed == "true", "NAMES", "hzPresetList returns typed arrays with every hostile name exact: " + listed);
      const bool filled = waitFor(wk, "[...document.querySelectorAll('#mcorners select')].length === 4"
                                      " && [...document.querySelectorAll('#mcorners select')].every(s => s.options.length > "
                                      + std::to_string(kNames.size()) + ")", 5.0);
      const std::string shown = run(wk,
          "const want = " + names + ";"
          "const texts = s => [...s.options].map(o => o.text);"
          "const sels = [...document.querySelectorAll('#mcorners select'), document.getElementById('presetList')];"
          "const ok = sels.every(s => want.every(n => texts(s).includes(n)));"
          "const imgs = document.querySelectorAll('select img, option *').length;"
          "return JSON.stringify({ ok, imgs, pwn: String(window.__pwnName) });");
      row(filled && shown == "{\"ok\":true,\"imgs\":0,\"pwn\":\"undefined\"}", "NAMES",
          "every dropdown shows every name as text, nothing built from one: " + shown);

      // CSP
      const std::string s1 = injectScript(wk);
      row(s1 == "undefined", "CSP", "an inline <script> added at run time does not run (__pwnScript=" + s1 + ")");
      const std::string s2 = injectHandler(wk);
      row(s2 == "undefined", "CSP", "an inline handler arriving through markup does not run (__pwnImg=" + s2 + ")");

      // FRAME
      const std::string f = framePost(wk, "FRAMEPOST");
      row(f != "FRAMEPOST", "FRAME", "a subframe's bridge message is not delivered (preset name now '" + f + "')");

      // CLIPBOARD
      const std::string c = scriptPaste(wk);
      row(c == "{\"ret\":false,\"fired\":false}", "CLIPBOARD", "page script cannot paste by itself: " + c);
      const std::string cp = clipboardPrefs(wk);
      row(cp == "javaScriptCanAccessClipboard=0 DOMPasteAllowed=0", "CLIPBOARD", cp);
      const std::string ps = run(wk, "return typeof window.hzPasteState;");
      row(ps == "function", "CLIPBOARD", "PASTE's native path hzPasteState is bound: " + ps);

      // NAV (last: a failure here replaces the page)
      for (const char *attempt : {"location.href = 'about:blank'", "location.href = 'file:///'",
                                  "const a = document.createElement('a'); a.href = 'https://example.invalid/';"
                                  " document.body.appendChild(a); a.click()",
                                  "window.__w = window.open('about:blank')"})
      {
        const std::string r = afterNav(wk, attempt);
        row(r == "page", "NAV", std::string(attempt) + "  -> " + r);
      }
      const std::string w = run(wk, "return String(window.__w);");
      row(w == "null", "NAV", "window.open returned " + w);
    }
    gui->destroy(p);
    p->destroy(p);
    [win release];

    /* ---------------- CONTROL: a plain choc web view ---------------- */
    choc::ui::WebView::Options opts;
    opts.webviewIsReady = [](choc::ui::WebView &w) {
      w.bind("hzPresetName", [](const choc::value::ValueView &) -> choc::value::Value {
        return choc::value::createString(ctlName);
      });
      w.bind("hzPresetSetName", [](const choc::value::ValueView &a) -> choc::value::Value {
        if (a.isArray() && a.size() > 0) ctlName = std::string(a[0].getWithDefault<std::string_view>(""));
        return {};
      });
      w.setHTML("<!doctype html><html><head><meta charset=\"utf-8\"></head><body>"
                "<select id=\"presetList\"></select><script>window.__page = 1;</script></body></html>");
    };
    auto ctl = std::make_unique<choc::ui::WebView>(opts);
    WKWebView *cw2 = (__bridge WKWebView *)ctl->getViewHandle();
    const bool cBoot = cw2 && waitFor(cw2, "document.readyState === 'complete' && window.__page === 1"
                                           " && typeof window.hzPresetName === 'function'", 15.0);
    row(cBoot, "CONTROL", cBoot ? "plain choc view loaded" : "plain choc view did not load within 15 s");
    if (cBoot)
    {
      const std::string s1 = injectScript(cw2);
      row(s1 == "1", "CONTROL", "CSP script detector registers (__pwnScript=" + s1 + ")");
      const std::string s2 = injectHandler(cw2);
      row(s2 == "1", "CONTROL", "CSP handler detector registers (__pwnImg=" + s2 + ")");
      const std::string f = framePost(cw2, "FRAMEPOST");
      row(f == "FRAMEPOST", "CONTROL", "FRAME detector registers (name '" + f + "')");
      const std::string c = scriptPaste(cw2);
      row(c == "{\"ret\":true,\"fired\":true}", "CONTROL", "CLIPBOARD detector registers: " + c);
      const std::string cp = clipboardPrefs(cw2);
      row(cp == "javaScriptCanAccessClipboard=1 DOMPasteAllowed=1", "CONTROL", "preference read-back registers: " + cp);
      // The pre-fix pattern: names concatenated into JSON natively, options
      // built from markup in the page. Both must visibly fail to round-trip.
      const std::string old = run(cw2,
          "const want = " + jsNames() + ";"
          "const raw = '{\"corners\":[' + want.map(n => '\"' + n + '\"').join(',') + ']}';"
          "let parsed = true; try { JSON.parse(raw); } catch (_) { parsed = false; }"
          "const s = document.createElement('select');"
          "s.innerHTML = '<option value=\"\">custom</option>' + want.map(n => `<option>${n}</option>`).join('');"
          "const texts = [...s.options].map(o => o.text);"
          "return JSON.stringify({ parsed, roundTrip: want.every(n => texts.includes(n)) });");
      row(old == "{\"parsed\":false,\"roundTrip\":false}", "CONTROL",
          "NAMES detector registers on the pre-fix pattern: " + old);
      const std::string r = afterNav(cw2, "location.href = 'about:blank'");
      row(r.rfind("replaced:", 0) == 0 && r.find("bridge=function") != std::string::npos, "CONTROL",
          "NAV detector registers: " + r);
    }
    ctl.reset();
    hypersaw_entry_deinit();
    std::error_code ec;
    fs::remove_all(home, ec);
  }

  if (failures || verbose)
    for (const auto &l : report) std::printf("%s\n", l.c_str());
  std::printf("gui_webview_check: %s (%zu rows, %d failed)\n", failures ? "FAIL" : "OK", report.size(), failures);
  return failures ? 1 : 0;
}
