/*
 * hypersaw_gui.h — the GUI seam (ADR-019 ratification amendment).
 *
 * This header is the ONLY thing the plugin shell knows about the GUI, and it
 * contains no webview, platform, or CLAP types. Swapping the webview for a
 * native backend (iPlug2/ImGui, the recorded fallback) means reimplementing
 * hypersaw_gui.mm against this same interface — nothing else moves.
 *
 * - VizSnapshot: plain-struct visualization feed, engine -> GUI.
 * - GuiHost: transport-agnostic callbacks, GUI -> engine (param sets are
 *   QUEUED by the shell onto the audio thread, never applied directly).
 */
#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace hypersaw
{

struct VizSnapshot
{
  bool oscEnabled = true;   // ADR-100 A2: the viewed oscillator's switch
  bool active = false;
  int n = 0;
  int centerIdx = 0;
  double R = 0, RN = 0, psi = 0, sigma = 0, KsmS = 0, KsmP = 0;
  double phase[32] = {0};
  // dynamics layer (Phase 3)
  int topo = 0, poles = 1;
  double RA = 0, RB = 0, RQ = 0;
  int gravCount = 0;
  int gravRatio[4] = {0};
  int gravOct[4] = {0};
  double gravErr[4] = {0};
  bool gridActive = false, gridLockWarn = false;
  double gridU = 0;
  int gridRungs = 0;
  // SPECTRA layer (Phase 4): per-partial strip visualizer feed. partials/cloud
  // are the live P/M; partR/partAmp index by partial; partPhase is the cloud
  // phase grid, row-major [partial*cloud + voice] with cloud <= 7.
  bool spectra = false;
  int partials = 0, cloud = 0;
  double partR[32] = {0};
  double partAmp[32] = {0};
  double partPhase[32 * 7] = {0};
  // Voice map (2026-07-30, human request): the detune lab's pan x pitch view.
  // Per-voice data for the FOCUS swarm — target freq (vf, post-law placement),
  // actual freq (eff, coupling included; the gap between them IS the pull),
  // signed base pan. f0 anchors the pitch axis.
  double sampleRate = 44100;   // scope needs period in SAMPLES from f0
  double vmF0 = 0;
  double vmVf[32] = {0};
  double vmEff[32] = {0};
  double vmPan[32] = {0};
  // Note monitor: {midi, gate, env} for EVERY swarm slot, so a stuck note is
  // VISIBLE — a row still gated (or still loud) with all keys up names the
  // layer at fault: gated = a note-off never arrived (host/wrapper side);
  // ungated but loud = DSP tail (core side). Turns the human's stuck-note
  // reports from anecdote into a screenshot.
  double outPeak = 0;   // plugin output peak — answers "is it even us?"
  // Per-oscillator meters (B24). PRE-master, PRE-FX: they answer "is this
  // strip contributing?", which is the question a mixer strip asks — a
  // post-master reading would just be outPeak scaled, and would go dark
  // when the master fader was down even though the strip was working.
  double oscPeak[4] = {0};
  // Per-voice envelope shape for the envelope display (2026-08-03). Published
  // as the times the CORE actually gave each voice, not re-derived in JS: the
  // scatter draws from the core's seeded stream, so any JS reconstruction
  // would be a second implementation free to drift from the one you hear.
  int envCount = 0;
  double envOnsetMs[32] = {0};   // delay before this voice enters
  double envAtkMs[32] = {0};     // this voice's attack time
  double envRelMs[32] = {0};     // this voice's release time
  int nmCount = 0;
  int nmMidi[16] = {0};
  int nmGate[16] = {0};
  double nmEnv[16] = {0};
  /* B106 — PER-OSCILLATOR PANES. Everything above describes the ACTIVE
     oscillator (whichever setVizOsc last named) and still feeds the OSC pages
     unchanged. This block is addressed BY OSCILLATOR INDEX instead, because
     MAIN shows both at once and a pane labelled "OSC 2" must be fed by osc 2
     whatever the OSC page happens to be editing — a label resolved from
     "active" is a label that can lie (L0028: address the role, and LABEL the
     resolved instance). Carpet data only: phase grid, its count, the cluster
     topology that colours it, the switch, and the pane's own f0 for the
     waveform trigger. */
  static constexpr int kVizOsc = 2;   // mirrors the shell's kNumOsc
  bool oscActive[kVizOsc] = {false, false};
  bool oscOn[kVizOsc] = {true, true};
  int oscN[kVizOsc] = {0, 0};
  int oscTopo[kVizOsc] = {0, 0};
  double oscF0[kVizOsc] = {0, 0};
  double oscPhase[kVizOsc][32] = {{0}, {0}};
};

struct GuiHost
{
  std::function<VizSnapshot()> getViz;
  std::function<void(float *, int)> getSpectrum;  // log-spaced 0..1 bins
  std::function<void(float *, float *, int)> getScope;  // raw L/R tail, newest last
  // B106: the same tail for a NAMED oscillator, so MAIN can draw both waves.
  // getScope above stays the active-oscillator tap the OSC pages use.
  std::function<void(int, float *, float *, int)> getScopeFor;
  std::function<std::string()> getParamsJson;            // {"<id>":value,...}
  // Defaults live in the SHELL, not in GUI markup. A GUI that reads a default out
  // of its own HTML loses it the moment that GUI is replaced — and can already
  // disagree with the host: oscillators above the first default to SILENT, so
  // clap_param_info reports 0 for osc2 level while the markup says 0.4. Served
  // from the same defaultFor() that fills clap_param_info.default_value.
  std::function<std::string()> getDefaultsJson;          // {"<id>":default,...}
  // Bend trajectory + vibrato cost, computed by the SHIPPED GlideCore so the
  // graph cannot disagree with the instrument. A JS twin of the laws would be a
  // second implementation to keep in step, which is the thing the graph exists
  // to make visible in the first place.
  std::function<std::string()> getBendCurveJson;
  std::function<std::string()> getShapeWaveJson;   // ADR-101: engine-drawn cycle
  std::function<void(uint32_t)> morphCapture;      // ADR-104: snapshot -> corner k
  std::function<std::string(uint32_t)> morphCornerJson;      // ADR-105: corner -> preset
  std::function<std::string()> morphLiveJson;               // ADR-105 A3: live state -> preset
  std::function<bool(uint32_t)> morphToggleExempt;          // ADR-109
  // ADR-136: mod-matrix route surface (main thread; the GUI's MOD page)
  std::function<std::string()> modRoutesJson;
  std::function<std::string()> modLiveJson;   // ADR-137: per-dest base + applied, for the halos
  std::function<bool(uint32_t, uint32_t)> modAddRoute;      // (srcSlot, destParamId)
  std::function<void(int, double)> modSetDepth;             // (routeIndex, depth)
  std::function<bool(int, uint32_t)> modSetSource;          // ADR-141: (routeIndex, srcSlot)
  std::function<void(int)> modRemoveRoute;
  std::function<std::string()> morphExemptJson;
  std::function<std::string()> morphOwnersJson;             // ADR-110 colour coding
  std::function<std::string(int)> morphCornerValsJson;     // ADR-111 armed corner view
  std::function<bool(uint32_t, const std::string &)> morphCornerApply;
  std::function<void(double)> setModWheel;               // ADR-150: GUI mod wheel -> matrix source 15
  std::function<void(uint32_t, double)> setParam;        // by frozen CLAP id
  std::function<void(uint32_t, bool)> gesture;           // id, begin
  std::function<void(uint32_t)> setVizOsc;               // visuals follow the GUI's active osc
  std::function<void()> panic;                          // all-off, both engines + rack
  std::function<std::string()> getBuildId;               // git short hash, GUI corner
  std::function<std::string()> getHostHint;              // host-misconfiguration notice, or empty
  /* Hand keyboard focus back to the host's view. Live routes computer-keyboard
     notes to whichever view is first responder, so while our webview holds it
     Live sees the key DOWN and not the key UP — measured 2026-08-12 as note
     durations 64-115 ms with Live focused vs 137-518 ms with the plugin focused,
     zero overlap, releases batched onto identical sample positions. Nothing in
     the DSP can fix a note-off the host never sends. */
  std::function<void()> releaseKeyFocus;
  std::function<std::string()> getStateJson;             // full provenance dump
  std::function<bool(const std::string &)> applyStateJson;
};

class HypersawGui
{
 public:
  explicit HypersawGui(GuiHost host);
  ~HypersawGui();

  HypersawGui(const HypersawGui &) = delete;
  HypersawGui &operator=(const HypersawGui &) = delete;

  bool attachToParent(void *parentView);  // NSView* on macOS
  void getSize(uint32_t &width, uint32_t &height) const;

 private:
  struct Impl;
  Impl *impl;
};

}  // namespace hypersaw
