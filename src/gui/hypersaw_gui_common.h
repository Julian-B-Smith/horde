/*
 * hypersaw_gui_common.h — platform-independent half of the webview backend:
 * the JS bridge bindings and the viz JSON serializer. Included ONLY by the
 * per-platform backends (hypersaw_gui.mm, hypersaw_gui_win.cpp); the plugin
 * shell still sees nothing but hypersaw_gui.h (the ADR-019 seam).
 */
#pragma once

#include "hypersaw_gui.h"

#include "../../libs/choc/choc/gui/choc_WebView.h"
#include "gui_html.h"
#include <filesystem>
#include <fstream>
#include <sstream>  // generated: kGuiHtml_data / kGuiHtml_size

#include <cstdio>
#include <functional>
#include <memory>

namespace hypersaw::detail
{

constexpr uint32_t kGuiWidth = 980;
constexpr uint32_t kGuiHeight = 720;

/* `withOscPanes` gates the B106 per-oscillator block. ADR-143's rule is that a
   feed is carried only where a consumer for it is on screen, and MAIN's carpets
   are the only consumer — so the pages that draw one oscillator keep exactly
   the bytes they had. Default true: the per-feed fallback (dev server, lab
   harness) serves whoever asks and has no want-bits to trim with. */
inline choc::value::Value vizToValue(const VizSnapshot &v, bool withOscPanes = true)
{
  // Structured choc::value crosses the bridge as a native JS object — no JSON
  // string on the C++ side, no JSON.parse per frame on the JS side. (The
  // former string path also hid a truncation bug — 2026-07-18.)
  auto obj = choc::value::createObject("Viz");
  obj.addMember("active", v.active);
  obj.addMember("oscEnabled", v.oscEnabled);
  obj.addMember("n", (int32_t)v.n);
  obj.addMember("centerIdx", (int32_t)v.centerIdx);
  obj.addMember("R", v.R);
  obj.addMember("RN", v.RN);
  obj.addMember("psi", v.psi);
  obj.addMember("sigma", v.sigma);
  obj.addMember("KsmS", v.KsmS);
  obj.addMember("KsmP", v.KsmP);
  obj.addMember("topo", (int32_t)v.topo);
  obj.addMember("poles", (int32_t)v.poles);
  obj.addMember("RA", v.RA);
  obj.addMember("RB", v.RB);
  obj.addMember("RQ", v.RQ);
  obj.addMember("gridActive", v.gridActive);
  obj.addMember("gridLockWarn", v.gridLockWarn);
  obj.addMember("gridU", v.gridU);
  obj.addMember("gridRungs", (int32_t)v.gridRungs);
  auto grav = choc::value::createEmptyArray();
  for (int i = 0; i < v.gravCount; i++)
  {
    auto pair = choc::value::createEmptyArray();
    pair.addArrayElement((int32_t)v.gravRatio[i]);
    pair.addArrayElement((int32_t)v.gravOct[i]);
    pair.addArrayElement(v.gravErr[i]);
    grav.addArrayElement(pair);
  }
  obj.addMember("grav", grav);
  auto phase = choc::value::createEmptyArray();
  for (int i = 0; i < v.n && i < 32; i++) phase.addArrayElement(v.phase[i]);
  obj.addMember("phase", phase);
  // voice map + note monitor (SAW mode; empty arrays in SPECTRA mode)
  obj.addMember("sr", v.sampleRate);
  obj.addMember("vmF0", v.vmF0);
  auto vmVf = choc::value::createEmptyArray();
  auto vmEff = choc::value::createEmptyArray();
  auto vmPan = choc::value::createEmptyArray();
  if (!v.spectra)
    for (int i = 0; i < v.n && i < 32; i++)
    {
      vmVf.addArrayElement(v.vmVf[i]);
      vmEff.addArrayElement(v.vmEff[i]);
      vmPan.addArrayElement(v.vmPan[i]);
    }
  obj.addMember("vmVf", vmVf);
  obj.addMember("vmEff", vmEff);
  obj.addMember("vmPan", vmPan);
  auto notes = choc::value::createEmptyArray();
  for (int i = 0; i < v.nmCount; i++)
  {
    auto row = choc::value::createEmptyArray();
    row.addArrayElement((int32_t)v.nmMidi[i]);
    row.addArrayElement((int32_t)v.nmGate[i]);
    row.addArrayElement(v.nmEnv[i]);
    notes.addArrayElement(row);
  }
  obj.addMember("outPeak", v.outPeak);
  // Per-oscillator meters (B24). Serialised as an array so the strip count
  // is data, not a shape the GUI hardcodes — a third oscillator needs no
  // change here.
  auto peaks = choc::value::createEmptyArray();
  for (double pk : v.oscPeak) peaks.addArrayElement(pk);
  obj.addMember("oscPeak", peaks);
  auto eOn = choc::value::createEmptyArray();
  auto eAt = choc::value::createEmptyArray();
  auto eRe = choc::value::createEmptyArray();
  for (int i = 0; i < v.envCount && i < 32; i++)
  {
    eOn.addArrayElement(v.envOnsetMs[i]);
    eAt.addArrayElement(v.envAtkMs[i]);
    eRe.addArrayElement(v.envRelMs[i]);
  }
  obj.addMember("envOnset", eOn);
  obj.addMember("envAtk", eAt);
  obj.addMember("envRel", eRe);
  obj.addMember("notes", notes);
  // SPECTRA per-partial strip feed (empty in SAW mode — v.spectra gates it).
  obj.addMember("spectra", v.spectra);
  if (v.spectra)
  {
    obj.addMember("partials", (int32_t)v.partials);
    obj.addMember("cloud", (int32_t)v.cloud);
    auto pr = choc::value::createEmptyArray();
    auto pa = choc::value::createEmptyArray();
    auto pp = choc::value::createEmptyArray();
    for (int k = 0; k < v.partials && k < 32; k++)
    {
      pr.addArrayElement(v.partR[k]);
      pa.addArrayElement(v.partAmp[k]);
      for (int m = 0; m < v.cloud && m < 7; m++) pp.addArrayElement(v.partPhase[k * 7 + m]);
    }
    obj.addMember("partR", pr);
    obj.addMember("partAmp", pa);
    obj.addMember("partPhase", pp);  // flat [partial*cloud + voice]
  }
  /* B106 — the per-oscillator pane feed, indexed by OSCILLATOR. MAIN draws a
     carpet per oscillator and labels them OSC 1 / OSC 2, so this cannot be
     "the active one and the other one" — a label resolved from "active" is a
     label that can lie. ~2x32 numbers per frame, carried only where MAIN's
     carpets are on screen (see `withOscPanes`). */
  if (!withOscPanes) return obj;
  auto oscs = choc::value::createEmptyArray();
  for (int k = 0; k < VizSnapshot::kVizOsc; k++)
  {
    auto o = choc::value::createObject("VizOsc");
    o.addMember("active", v.oscActive[k]);
    o.addMember("on", v.oscOn[k]);
    o.addMember("n", (int32_t)v.oscN[k]);
    o.addMember("topo", (int32_t)v.oscTopo[k]);
    o.addMember("f0", v.oscF0[k]);
    auto ph = choc::value::createEmptyArray();
    for (int i = 0; i < v.oscN[k] && i < 32; i++) ph.addArrayElement(v.oscPhase[k][i]);
    o.addMember("phase", ph);
    oscs.addArrayElement(o);
  }
  obj.addMember("oscs", oscs);
  return obj;
}

/* Every binding and the page load run from installBridge, which choc calls
   through Options::webviewIsReady. On macOS that callback fires synchronously
   inside the WebView constructor, so this is the old straight-line code in a
   different coat. On Windows the WebView2 controller is created ASYNCHRONOUSLY
   on the host's message loop, and until it exists choc's bind()/setHTML()
   return false and register NOTHING — no queue, no retry. The first native
   Windows load (Live 12.3, 2026-09-09) showed exactly that: audio fine, GUI a
   black rectangle, every bind and the setHTML dropped before the controller
   existed. The per-backend binding (hzGrabKeys) goes through platformBinds for
   the same reason; binding after makeWebView returns is the trap. */
inline void installBridge(choc::ui::WebView &web, GuiHost &host)
{
  web.bind("hzGetViz", [&host](const choc::value::ValueView &) -> choc::value::Value {
    return vizToValue(host.getViz());
  });
  web.bind("hzPanic", [&host](const choc::value::ValueView &) -> choc::value::Value {
    if (host.panic) host.panic();
    return choc::value::createInt32(1);
  });
  web.bind("hzGetBuild", [&host](const choc::value::ValueView &) -> choc::value::Value {
    return choc::value::createString(host.getBuildId ? host.getBuildId() : std::string("?"));
  });
  web.bind("hzReleaseKeyFocus", [&host](const choc::value::ValueView &) -> choc::value::Value {
    if (host.releaseKeyFocus) host.releaseKeyFocus();
    return choc::value::createInt32(1);
  });
  web.bind("hzGetHostHint", [&host](const choc::value::ValueView &) -> choc::value::Value {
    // Empty, not "?": absent hint means NOTHING TO SAY. A placeholder here would
    // render as a permanent warning badge on every load.
    return choc::value::createString(host.getHostHint ? host.getHostHint() : std::string());
  });
  web.bind("hzGetParams", [&host](const choc::value::ValueView &) -> choc::value::Value {
    return choc::value::createString(host.getParamsJson());
  });
  web.bind("hzGetBendCurve", [&host](const choc::value::ValueView &) -> choc::value::Value {
    return choc::value::createString(host.getBendCurveJson ? host.getBendCurveJson()
                                                           : std::string("{}"));
  });
  /* DISK PRESET STORE (ADR-105 A2). localStorage was the quick win and it
     quick-lost: the webview loads via setHTML — an OPAQUE ORIGIN — where
     localStorage throws SecurityError, and the store's own try/catch swallowed
     it: presets no-opped in the plugin while working on localhost. Files in
     app support instead (the global CLAUDE.md plugin-state rule: a stable
     app-support folder, never state-chunk bloat). These binds run on the GUI
     thread — filesystem and allocation are fine here, never in process(). */
  web.bind("hzPresetList", [](const choc::value::ValueView &) -> choc::value::Value {
    namespace fs = std::filesystem;
    auto listDir = [](const fs::path &d) {
      std::string out = "[";
      bool first = true;
      std::error_code ec;
      for (auto &e : fs::directory_iterator(d, ec))
        if (e.path().extension() == ".json")
        {
          out += (first ? "\"" : ",\"") + e.path().stem().string() + "\"";
          first = false;
        }
      return out + "]";
    };
    const fs::path base = fs::path(std::getenv("HOME") ? std::getenv("HOME") : "")
                          / "Library/Application Support/LiftedTruck/HYPERSAW";
    return choc::value::createString("{\"presets\":" + listDir(base / "presets") +
                                     ",\"corners\":" + listDir(base / "corners") + "}");
  });
  web.bind("hzPresetSave", [](const choc::value::ValueView &args) -> choc::value::Value {
    namespace fs = std::filesystem;
    if (!args.isArray() || args.size() < 3) return choc::value::createBool(false);
    std::string kind = args[0].getWithDefault<std::string>("");
    std::string name = args[1].getWithDefault<std::string>("");
    std::string json = args[2].getWithDefault<std::string>("");
    // sanitise: the name becomes a filename
    std::string safe;
    for (char c : name)
      if (std::isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_') safe += c;
    // "prefs": machine-local GUI settings (scheme/mode). The scheme chip was
    // "session-only on purpose — an audition" until 2026-08-29, when the human
    // reported the non-persistence as a bug: the audition graduated.
    if (safe.empty() || (kind != "presets" && kind != "corners" && kind != "prefs") || json.empty())
      return choc::value::createBool(false);
    const fs::path dir = fs::path(std::getenv("HOME") ? std::getenv("HOME") : "")
                         / "Library/Application Support/LiftedTruck/HYPERSAW" / kind;
    std::error_code ec;
    fs::create_directories(dir, ec);
    std::ofstream f(dir / (safe + ".json"), std::ios::trunc);
    if (!f) return choc::value::createBool(false);
    f << json;
    return choc::value::createBool(f.good());
  });
  web.bind("hzPresetLoad", [](const choc::value::ValueView &args) -> choc::value::Value {
    namespace fs = std::filesystem;
    if (!args.isArray() || args.size() < 2) return choc::value::createString("");
    std::string kind = args[0].getWithDefault<std::string>("");
    std::string name = args[1].getWithDefault<std::string>("");
    const fs::path fp = fs::path(std::getenv("HOME") ? std::getenv("HOME") : "")
                        / "Library/Application Support/LiftedTruck/HYPERSAW" / kind
                        / (name + ".json");
    std::ifstream f(fp);
    if (!f) return choc::value::createString("");
    std::ostringstream ss;
    ss << f.rdbuf();
    return choc::value::createString(ss.str());
  });
  web.bind("hzPresetDelete", [](const choc::value::ValueView &args) -> choc::value::Value {
    namespace fs = std::filesystem;
    if (!args.isArray() || args.size() < 2) return choc::value::createBool(false);
    std::string kind = args[0].getWithDefault<std::string>("");
    std::string name = args[1].getWithDefault<std::string>("");
    const fs::path fp = fs::path(std::getenv("HOME") ? std::getenv("HOME") : "")
                        / "Library/Application Support/LiftedTruck/HYPERSAW" / kind
                        / (name + ".json");
    std::error_code ec;
    return choc::value::createBool(fs::remove(fp, ec));
  });
  web.bind("hzMorphToggleExempt", [&host](const choc::value::ValueView &args) -> choc::value::Value {
    bool on = false;
    if (host.morphToggleExempt && args.isArray() && args.size() >= 1)
      on = host.morphToggleExempt((uint32_t)args[0].getWithDefault<int64_t>(0));
    return choc::value::createBool(on);
  });
  web.bind("hzMorphCornerVals", [&host](const choc::value::ValueView &args) -> choc::value::Value {
    int k = -1;
    if (args.isArray() && args.size() >= 1) k = (int)args[0].getWithDefault<int64_t>(-1);
    return choc::value::createString(host.morphCornerValsJson ? host.morphCornerValsJson(k) : std::string("{}"));
  });
  web.bind("hzModRoutes", [&host](const choc::value::ValueView &) -> choc::value::Value {
    return choc::value::createString(host.modRoutesJson ? host.modRoutesJson() : std::string("[]"));
  });
  web.bind("hzModLive", [&host](const choc::value::ValueView &) -> choc::value::Value {
    return choc::value::createString(host.modLiveJson ? host.modLiveJson() : std::string("[]"));
  });
  web.bind("hzModAdd", [&host](const choc::value::ValueView &args) -> choc::value::Value {
    bool ok = false;
    if (host.modAddRoute && args.isArray() && args.size() >= 2)
      ok = host.modAddRoute((uint32_t)args[0].getWithDefault<int64_t>(0),
                            (uint32_t)args[1].getWithDefault<int64_t>(0));
    return choc::value::createBool(ok);
  });
  web.bind("hzModDepth", [&host](const choc::value::ValueView &args) -> choc::value::Value {
    if (host.modSetDepth && args.isArray() && args.size() >= 2)
      host.modSetDepth((int)args[0].getWithDefault<int64_t>(-1),
                       args[1].getWithDefault<double>(0.0));
    return choc::value::createBool(true);
  });
  web.bind("hzModWheel", [&host](const choc::value::ValueView &args) -> choc::value::Value {
    if (host.setModWheel && args.isArray() && args.size() >= 1)
      host.setModWheel(args[0].getWithDefault<double>(0.0));
    return choc::value::createBool(true);
  });
  web.bind("hzModSource", [&host](const choc::value::ValueView &args) -> choc::value::Value {
    bool ok = false;
    if (host.modSetSource && args.isArray() && args.size() >= 2)
      ok = host.modSetSource((int)args[0].getWithDefault<int64_t>(-1),
                             (uint32_t)args[1].getWithDefault<int64_t>(0));
    return choc::value::createBool(ok);
  });
  web.bind("hzModRemove", [&host](const choc::value::ValueView &args) -> choc::value::Value {
    if (host.modRemoveRoute && args.isArray() && args.size() >= 1)
      host.modRemoveRoute((int)args[0].getWithDefault<int64_t>(-1));
    return choc::value::createBool(true);
  });
  web.bind("hzMorphOwners", [&host](const choc::value::ValueView &) -> choc::value::Value {
    return choc::value::createString(host.morphOwnersJson ? host.morphOwnersJson() : std::string("{}"));
  });
  web.bind("hzMorphExemptJson", [&host](const choc::value::ValueView &) -> choc::value::Value {
    return choc::value::createString(host.morphExemptJson ? host.morphExemptJson() : std::string("{}"));
  });
  web.bind("hzMorphLiveJson", [&host](const choc::value::ValueView &) -> choc::value::Value {
    return choc::value::createString(host.morphLiveJson ? host.morphLiveJson() : std::string("{}"));
  });
  web.bind("hzMorphCornerJson", [&host](const choc::value::ValueView &args) -> choc::value::Value {
    if (host.morphCornerJson && args.isArray() && args.size() >= 1)
      return choc::value::createString(host.morphCornerJson((uint32_t)args[0].getWithDefault<int64_t>(0)));
    return choc::value::createString("{}");
  });
  web.bind("hzMorphCornerApply", [&host](const choc::value::ValueView &args) -> choc::value::Value {
    bool ok = false;
    if (host.morphCornerApply && args.isArray() && args.size() >= 2)
      ok = host.morphCornerApply((uint32_t)args[0].getWithDefault<int64_t>(0),
                                 args[1].getWithDefault<std::string>(""));
    return choc::value::createBool(ok);
  });
  web.bind("hzMorphCapture", [&host](const choc::value::ValueView &args) -> choc::value::Value {
    if (host.morphCapture && args.isArray() && args.size() >= 1)
      host.morphCapture((uint32_t)args[0].getWithDefault<int64_t>(0));
    return {};
  });
  web.bind("hzGetShapeWave", [&host](const choc::value::ValueView &) -> choc::value::Value {
    return choc::value::createString(host.getShapeWaveJson ? host.getShapeWaveJson()
                                                           : std::string("{}"));
  });
  web.bind("hzGetDefaults", [&host](const choc::value::ValueView &) -> choc::value::Value {
    return choc::value::createString(host.getDefaultsJson ? host.getDefaultsJson()
                                                          : std::string("{}"));
  });
  web.bind("hzSetParam", [&host](const choc::value::ValueView &args) -> choc::value::Value {
    if (args.isArray() && args.size() >= 2)
      host.setParam((uint32_t)args[0].getWithDefault<int64_t>(0),
                    args[1].getWithDefault<double>(0.0));
    return {};
  });
  // The visuals follow the GUI's active oscillator. THIS BIND WAS MISSING until
  // 2026-08-08: the GuiHost member and the plugin-side assignment both landed,
  // the build was green, and window.hzSetVizOsc simply did not exist — so every
  // tab click threw and the viz stayed pinned to oscillator 0. A callback with
  // no bind is invisible to the compiler.
  web.bind("hzSetVizOsc", [&host](const choc::value::ValueView &args) -> choc::value::Value {
    if (host.setVizOsc && args.isArray() && args.size() >= 1)
      host.setVizOsc((uint32_t)args[0].getWithDefault<int64_t>(0));
    return {};
  });
  web.bind("hzGesture", [&host](const choc::value::ValueView &args) -> choc::value::Value {
    if (args.isArray() && args.size() >= 2)
      host.gesture((uint32_t)args[0].getWithDefault<int64_t>(0),
                   args[1].getWithDefault<bool>(false));
    return {};
  });
  // 256 bins, not 96 (human 2026-08-08: "choppy and ugly"). The analyzer is a
  // 2048-point FFT — ~1024 usable magnitudes — so 96 log bins were throwing
  // away most of what had already been computed; the cost of more bins is the
  // array marshal, not the transform.
  web.bind("hzGetSpec", [&host](const choc::value::ValueView &) -> choc::value::Value {
    constexpr int kBins = 256;
    float bins[kBins];
    host.getSpectrum(bins, kBins);
    auto arr = choc::value::createEmptyArray();
    for (int i = 0; i < kBins; i++) arr.addArrayElement(bins[i]);
    return arr;
  });
  web.bind("hzGetScope", [&host](const choc::value::ValueView &) -> choc::value::Value {
    // 1536, not 512: at D2 one period is ~604 samples, so a 512-sample window
    // cannot even hold one — there was nothing for a trigger to lock onto.
    constexpr int kN = 1536;
    float l[kN], r[kN];
    if (host.getScope) host.getScope(l, r, kN);
    else { for (int i = 0; i < kN; i++) { l[i] = 0; r[i] = 0; } }
    auto L = choc::value::createEmptyArray(), R = choc::value::createEmptyArray();
    for (int i = 0; i < kN; i++) { L.addArrayElement(l[i]); R.addArrayElement(r[i]); }
    auto obj = choc::value::createObject("Scope");
    obj.addMember("l", L); obj.addMember("r", R);
    return obj;
  });
  /* B76: ONE round-trip per GUI frame tick instead of three. Measured with
     the QUIET instrument (2026-08-29): a fully silenced page ran raf 16ms
     while the loud page ran 44ms — the per-frame cost was ours, and the BIND
     is the expensive unit (marshal + main-thread hop + reply eval), not the
     native work inside it. So the three per-feed fetches collapse into one
     call whose payload the ADR-143 consumer gate trims per page. The per-feed
     binds above STAY — they are the fallback for a page served outside the
     plugin (dev server, lab harness) and the seam other callers already use.
     `want` bits: 1 viz · 2 spec · 4 scope · 8 per-osc scope · 16 per-osc
     carpets. The last two are B106's: each has its OWN bit because each has
     its own DOM-derived consumer (MAIN's `canvas.wavosc` / `canvas.carpetM`),
     and folding them into one would make either page pay for the other's
     feed. */
  /* B76 part 2 — THE TOKENS, NOT THE CALLS. Collapsing three calls into one
     (part 1) moved the DAW's raf number not at all (44 ms before and after),
     which killed the call-count theory and convicted the payload: every bind
     reply is an evaluateJavaScript whose cost is per-TOKEN, and the scope was
     ~3,072 number literals per frame. A base64 string of packed int16 is ONE
     token carrying the same samples — the JS side decodes it in microseconds
     with a DataView. Int16 is not an audio path: 96 dB of display dynamic
     range on a ~300 px canvas, quantization invisible by construction. The
     spectrum rides the same way as uint8 (the GUI smooths it anyway). */
  web.bind("hzFrame", [&host](const choc::value::ValueView &args) -> choc::value::Value {
    static const char *kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    auto b64 = [](const unsigned char *d, size_t n) {
      std::string o;
      o.reserve(((n + 2) / 3) * 4);
      for (size_t i = 0; i < n; i += 3)
      {
        const unsigned a = d[i], b = i + 1 < n ? d[i + 1] : 0, c = i + 2 < n ? d[i + 2] : 0;
        o += kB64[a >> 2];
        o += kB64[((a & 3) << 4) | (b >> 4)];
        o += i + 1 < n ? kB64[((b & 15) << 2) | (c >> 6)] : '=';
        o += i + 2 < n ? kB64[c & 63] : '=';
      }
      return o;
    };
    const int want = args.isArray() && args.size() >= 1
                         ? (int)args[0].getWithDefault<int64_t>(7) : 7;
    auto obj = choc::value::createObject("Frame");
    if (want & 1) obj.addMember("viz", vizToValue(host.getViz(), (want & 16) != 0));
    if (want & 2)
    {
      constexpr int kBins = 256;
      float bins[kBins];
      host.getSpectrum(bins, kBins);
      unsigned char q[kBins];
      for (int i = 0; i < kBins; i++)
      {
        const float v = bins[i] < 0 ? 0.0f : (bins[i] > 1 ? 1.0f : bins[i]);
        q[i] = (unsigned char)(v * 255.0f + 0.5f);
      }
      obj.addMember("spec8", b64(q, kBins));
    }
    /* B106 — TWO scope shapes, never both on one page. Bit 4 is the ACTIVE
       oscillator's block (what the OSC pages ask for, byte-for-byte what they
       asked for before). Bit 8 is one block PER OSCILLATOR, which is what MAIN
       asks for INSTEAD of bit 4 — so MAIN's frame grows by exactly one
       oscillator's worth of samples and no page ever carries three blocks. */
    if (want & 12)
    {
      constexpr int kN = 1536;   // matches hzGetScope, same reason (D2 period)
      // little-endian int16, L block then R block — the JS DataView mirrors this
      auto pack = [&](auto &&fill) {
        float l[kN], r[kN];
        for (int i = 0; i < kN; i++) { l[i] = 0; r[i] = 0; }
        fill(l, r, kN);
        unsigned char pk[kN * 4];
        auto put = [&](int idx, float v) {
          const float c = v < -1 ? -1.0f : (v > 1 ? 1.0f : v);
          const int16_t q = (int16_t)(c * 32767.0f);
          pk[idx * 2] = (unsigned char)(q & 0xFF);
          pk[idx * 2 + 1] = (unsigned char)((q >> 8) & 0xFF);
        };
        for (int i = 0; i < kN; i++) put(i, l[i]);
        for (int i = 0; i < kN; i++) put(kN + i, r[i]);
        return b64(pk, sizeof pk);
      };
      if (want & 4)
        obj.addMember("scope16", pack([&](float *l, float *r, int n) {
          if (host.getScope) host.getScope(l, r, n);
        }));
      if (want & 8)
      {
        auto arr = choc::value::createEmptyArray();
        for (int k = 0; k < VizSnapshot::kVizOsc; k++)
          arr.addArrayElement(pack([&](float *l, float *r, int n) {
            if (host.getScopeFor) host.getScopeFor(k, l, r, n);
          }));
        obj.addMember("oscScope16", arr);
      }
      obj.addMember("scopeN", (int32_t)kN);
    }
    return obj;
  });
  web.bind("hzGetState", [&host](const choc::value::ValueView &) -> choc::value::Value {
    return choc::value::createString(host.getStateJson());
  });
  web.bind("hzApplyState", [&host](const choc::value::ValueView &args) -> choc::value::Value {
    bool ok = false;
    if (args.isArray() && args.size() >= 1)
      ok = host.applyStateJson(std::string(args[0].getWithDefault<std::string_view>("")));
    return choc::value::createBool(ok);
  });

}

inline std::unique_ptr<choc::ui::WebView>
makeWebView(GuiHost &host, std::function<void(choc::ui::WebView &)> platformBinds)
{
  choc::ui::WebView::Options opts;
  opts.enableDebugMode = false;
  opts.acceptsFirstMouseClick = true;  // click-through focus in hosts
  // Bindings before the page: they install document-created scripts, so the
  // page must be navigated to AFTER they exist (the order the old code had).
  opts.webviewIsReady = [&host, platformBinds](choc::ui::WebView &w) {
    installBridge(w, host);
    if (platformBinds) platformBinds(w);
    w.setHTML(std::string(reinterpret_cast<const char *>(kGuiHtml_data), kGuiHtml_size));
  };
  return std::make_unique<choc::ui::WebView>(opts);
}

}  // namespace hypersaw::detail
