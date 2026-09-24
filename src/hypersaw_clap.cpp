/*
 * hypersaw_clap.cpp — HYPERSAW CLAP plugin impl (Phase 2: SwarmCore wired in).
 *
 * The DSP is src/swarm_core.h — the parity-proven SAW core (L0-1) — untouched
 * here; this file is the CLAP adapter: note/param events in, audio out, state
 * save/load. Parameter IDs are frozen once shipped (host automation lanes and
 * saved sessions reference them); append new params, never renumber. Ranges
 * mirror the prototype UI (reference/swarmsaw.html) — notably dissolve is exposed in
 * SECONDS (the prototype knob is log10 s), driftDepth in cents.
 *
 * Real-time rules (charter): process() allocates nothing, no locks, no
 * wall-clock. setParam/rebuild are fixed-array math — safe on the audio
 * thread. params.flush is audio-thread while active per CLAP, main-thread
 * only when inactive, so touching the core there is race-free.
 */

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <atomic>
#include <string>
#include <filesystem>
#include <clap/clap.h>
#include <clapwrapper/vst3.h>
#include <algorithm>
#include <limits>   // quiet_NaN in the modsrc probe export (B171)
#include <vector>

#include "swarm_core.h"
#include "gui/hypersaw_gui.h"
#include "gui/preset_store.h"   // presetRoot(): the ONE store path (B129)
#include "spectra_core.h"
#include "subosc_core.h"   // B172: the SUB OSC engine block's core, one per voice
#include "glide_core.h"
#include "mod_core.h"
#include "morph_core.h"
#include "intent_core.h"   // B89 phase 2b: the resolver (ADR-176); shell owns the storage
#include "depends_graph.h"
#include "fx_rack.h"
#include "routing_core.h"
#include "undo_tree.h"
#include "hypersaw_clap_entry.h"
#include "hypersaw_debug.h"   // the probe surface's ONE prototype set; included
                              // HERE so a definition below that drifts from it
                              // is a compile error, not a silent link (audit H2)
#include "build_stamp.h"   // generated every build (CMake target)

namespace
{

static const char *s_features[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER,
                                   CLAP_PLUGIN_FEATURE_STEREO, nullptr};

/* ADR-114: the DEVICE is "horde"; HYPERSAW is the founding ENGINE inside it
   (the name the engine selector shows) and the repo's own name. The display
   string below is what a host puts in its browser, so it follows the device.
   THE ID DOES NOT AND MUST NOT. `com.lifted-truck.hypersaw` is how every host
   re-finds this plugin in an already-saved session; renaming it would orphan
   every project that has ever loaded the device — a rename is a new plugin as
   far as a DAW is concerned. The id is an identifier that happens to read like
   a name, which is exactly why it is tempting to "fix". */
static const clap_plugin_descriptor_t s_desc = {
    CLAP_VERSION_INIT,
    "com.lifted-truck.hypersaw",   // FROZEN — see above; not a display string
    "horde",
    "Lifted Truck",
    "https://github.com/Lifted-Truck/horde",
    "",
    "",
    "0.1.0",
    "Coupled-oscillator swarm synthesizer",
    &s_features[0]};

/* ---- parameter table (IDs frozen; append-only) ---- */

struct ParamDef
{
  clap_id id;
  const char *coreKey;  // SwarmCore setParam key
  const char *name;
  double minV, maxV, defV;
  bool stepped;
  const char *const *labels;  // for enum-ish stepped params, else nullptr
};

static const char *const kDistLabels[] = {"even spread", "JP-8000 curve", "gaussian (seeded)",
                                          "cauchy (seeded)", "golden (irrational)"};
static const char *const kLawLabels[] = {"cents-constant", "Hz-constant", "ERB-flat",
                                         "tempo-grid", "harmonic (series)",
                                         "stretch (inharmonic)"};
static const char *const kDriftModeLabels[] = {"walk (1/f)", "sine (per-voice)",
                                               "sample & hold"};
static const char *const kPanModeLabels[] = {"drift (per-voice)", "sweep (whole image)"};
static const char *const kPivotLabels[] = {"mean field", "root (fundamental)"};
static const char *const kPanLayoutLabels[] = {"pitch fan", "legacy (x-position)"};
static const char *const kSuperModeLabels[] = {"wide (clean)", "pulse (M/S)", "smear (allpass)"};
/* ADR-103. Three sources, genuinely distinct (human 2026-08-21: "I don't want
   always to replace the former option... I want it to be its own thing"):
   0 held — glide only while another key is DOWN (classic legato).
   1 last note (ringing) — glide while the previous note still SOUNDS (its tail
     counts); silence resets, so separated phrases start clean.
   2 always — glide from the last played note no matter what, silence included.
   History note: the pre-split option 1 behaved as ALWAYS (lastNoteF persisted
   across silence, human-ruled 2026-07-31), so stored glideMode=1 in schema<2
   patches migrates to 2 — same sound, new number. The NEW mode 1 is the option
   that never existed. */
static const char *const kGlideModeLabels[] = {"held note (legato)", "last note (ringing)",
                                               "always"};
static const char *const kOffOn[] = {"off", "on"};
/* B146 (human 2026-09-18, "Bass mono toggle ratified"): WHERE the ADR-035
   bass-mono stage sits relative to the FX rack.
     pre  — before the rack, which is where the stage has always run, so the
            default renders every existing patch to the bit;
     post — after the rack, the placement nothing downstream can undo. It is
            not redundant with `pre`: a LINEAR stereo-symmetric slot commutes
            with the side high-pass (measured on Comb — the reorder was dropped
            in 2026-08 for exactly that reason), but a NONLINEAR one does not.
            Drive is per-channel, so f(L)-f(R) makes side content out of MID,
            below the crossover, that a pre-only stage never saw;
     both — the two in series, at the cost of a second filter state. */
static const char *const kBassMonoPosLabels[] = {"pre (before FX)", "post (after FX)", "both"};
static const char *const kNoteNames[] = {"C", "C#", "D", "D#", "E", "F",
                                        "F#", "G", "G#", "A", "A#", "B"};
static const char *const kMorphArmLabels[] = {"live (owning corner)", "A", "B", "C", "D"};
static const char *const kMorphModeLabels[] = {"quantum (flip)", "blend"};
static const char *const kMpeLawLabels[] = {"instant", "follow bend law"};
static const char *const kQTimeModeLabels[] = {"continuous", "free (Hz)", "sync"};
static const char *const kNoteLinkLabels[] = {"own settings", "follow bend law"};
static const char *const kBendLawLabels[] = {"off (instant)", "constant time", "constant rate",
                                            "lag (one-pole)", "mass-spring"};
/* ADR-111. Value 2 keeps the behaviour every existing patch was saved with
   (drag), value 3 is the newcomer — append-only, so no stored patch changes
   sound. The note lane gets its own array: "drag" describes what the GLOBAL
   wheel lane does with a correction (it lands in pitchBend and transposes the
   whole field); a per-voice lane has no field to drag. */
static const char *const kBendQuantLabels[] = {"off", "chromatic", "scale (drag)", "scale", "scale (offset)"};
static const char *const kNoteQuantLabels[] = {"off", "chromatic", "scale"};
static const char *const kTopoLabels[] = {"mean-field", "ring", "two-cluster"};
static const char *const kPolesLabels[] = {"1 — classic", "2 — pair", "3 — triad", "4 — quad"};
// ADR-142: the Delay's time mode. Named for what each choice MEANS at the
// knob ("free ms" vs "tempo sync"), not "off/on" — a sync toggle labelled
// off/on reads as though it disables the delay.
static const char *const kDelaySyncLabels[] = {"free (ms)", "tempo sync"};
/* B171 LFO labels. The sync pair is NOT kDelaySyncLabels: the free branch of an
   LFO is a RATE in Hz, not a time in ms, and a label that names the wrong unit
   is the kind of lie a dropdown tells in one glance. Shape order is frozen —
   the value is a stepped parameter a patch stores, so entries are appended. */
/* "sample & hold" is spelled exactly as kDriftModeLabels spells it — the device
   already has a word for this shape, and a second spelling is a second thing a
   player has to learn is the same thing. */
static const char *const kLfoShapeLabels[] = {"sine",     "triangle", "saw up",
                                              "saw down", "square",   "sample & hold"};
static const char *const kLfoSyncLabels[] = {"free (Hz)", "tempo sync"};
static const char *const kLfoRetrigLabels[] = {"free-running", "retrig on note"};
static const char *const kFxTypeLabels[] = {"Off",  "Drive", "Filter", "Gain",
                                            "Comp", "Comb",  "Notch", "Echo", "Room", "Delay"};
/* B117 / ADR-163. "atomic" is what the rack has always done — module type is
   stepped, so it flips at a grid tick with the outgoing tail cut — and it is
   the default, so naming it is a description, not a new mode. */
static const char *const kFxXfadeLabels[] = {"atomic (flip)", "crossfade"};
// Display names for the gravity ratio readout (indices match core kRatios)
static const char *const kRatioNames[13] = {"1/1", "16/15", "9/8", "6/5", "5/4", "4/3", "7/5",
                                            "3/2", "8/5", "5/3", "16/9", "15/8", "2/1"};

/* ADR-115: the engine is SWARM SAW. Renamed SAW -> HYPERSAW (ADR-091), and now
   -> SWARM SAW, which returns it to the lineage its own prototype never left
   (reference/swarmsaw.html / SwarmSynth). With the device named horde, "HYPERSAW" now
   survives ONLY as the repo name and the frozen plugin id — it is off the
   product surface entirely. The VALUE and the state key are untouched, as in
   ADR-091: a label is not an identity, and every stored patch keeps loading. */
static const char *const kEngineLabels[] = {"SWARM SAW", "SPECTRA"};
static const char *const kWlawLabels[] = {"cents", "Hz"};
static const ParamDef kParams[] = {
    {1, "n", "Voices", 1, 32, 7, true, nullptr},
    {2, "dist", "Distribution", 0, 4, 1, true, kDistLabels},
    {3, "seed", "Seed", 0, 999999, 1234, true, nullptr},
    /* Musical rests again (ADR-156, 2026-09-10). The 2026-08-30 FLOOR defaults
       (detune 0, K -1) existed only so a unipolar macro route could sweep the
       whole knob from an XY pad; with the osc pads writing these params
       DIRECTLY, the floor was a lie under every patch — and ADR-152's macro
       suspension dropped the sound onto it whenever the morph was toggled.
       0.28 / 0 are the lab-authored defaults the cores already carry. */
    {4, "detune", "Detune", 0, 1, 0.28, false, nullptr},
    {5, "law", "Detune Law", 0, 5, 0, true, kLawLabels},
    {6, "K", "Coupling", -1, 1, 0, false, nullptr},   // ADR-156: the lab default, see detune
    {7, "onset", "Onset Lock", -1, 1, 0, false, nullptr},  // ADR-056: bipolar (<0 = splay onset)
    {8, "dissolve", "Dissolve (s)", 0.05, 7.94, 0.63, false, nullptr},
    {9, "driftDepth", "Drift Depth (c)", 0, 100, 0, false, nullptr},  // widened from the
    // prototype's 25c at human request (ADR-020); core takes any cents value
    {10, "driftRate", "Drift Rate", 0, 1, 0.4, false, nullptr},
    {11, "inertia", "Inertia", 0, 1, 0, false, nullptr},
    {12, "rtone", "Coherence -> Tone", -1, 1, 0, false, nullptr},
    {13, "normExp", "Density Comp", 0.5, 1, 0.75, false, nullptr},
    {14, "width", "Width", 0, 1.5, 0.8, false, nullptr},  // >1 = super-width (ADR-025)
    {15, "mono", "Mono Fold", 0, 1, 0, true, kOffOn},
    {16, "digital", "Digital", 0, 1, 1, false, nullptr},
    {17, "vol", "Volume", 0, 1, 0.4, false, nullptr},
    {18, "retrig", "Retrigger", 0, 1, 1, true, kOffOn},
    // ADR-021 envelope: defaults reproduce the reference AR bit-exactly
    {19, "attack", "Attack (s)", 0.001, 2.0, 0.003, false, nullptr},
    {20, "decay", "Decay (s)", 0.005, 4.0, 0.16, false, nullptr},
    {21, "sustain", "Sustain", 0, 1, 1.0, false, nullptr},
    {22, "release", "Release (s)", 0.005, 8.0, 0.16, false, nullptr},
    // Tempo-grid law (ADR-022): bpm is host-owned (transport), not a param
    {23, "beatMult", "Grid Cycles/Beat", 0.25, 8.0, 1.0, false, nullptr},
    // Dynamics surface (Phase 3 increment 2; engine per ADR-023)
    {24, "topo", "Topology", 0, 2, 0, true, kTopoLabels},
    {25, "reach", "Ring Reach", 1, 8, 5, true, nullptr},
    {26, "mu", "Cluster Link", 0, 1, 0.6, false, nullptr},
    {27, "alpha", "Phase Lag", -90, 90, 0, false, nullptr},
    {28, "poles", "Poles q", 1, 4, 1, true, kPolesLabels},
    {29, "grav", "Gravity", 0, 1, 0, false, nullptr},
    {30, "basin", "Basin (c)", 10, 50, 35, false, nullptr},
    {31, "absK", "Absolute Coupling", 0, 1, 0, true, kOffOn},
    // Voice mode (ADR-026): mono/glide/legato are SHELL note-routing plus the
    // core's glide param; octave is a pure shell transpose.
    {32, "voiceMono", "Mono", 0, 1, 0, true, kOffOn},
    {33, "glide", "Note Lag (s)", 0, 2.0, 0, false, nullptr},
    {34, "voiceLegato", "Legato", 0, 1, 1, true, kOffOn},
    {35, "octave", "Octave", -2, 2, 0, true, nullptr},
    // Transposition suite (ADR-027): all four combine into ONE live core tune
    // factor — the pitch knob bends sounding notes, not just new ones.
    {36, "semi", "Semitones", -12, 12, 0, true, nullptr},
    {37, "fineCents", "Fine (c)", -100, 100, 0, false, nullptr},
    {38, "pitchBend", "Pitch", -12, 12, 0, false, nullptr},
    {39, "scatter", "Phase Scatter", 0, 1, 0, false, nullptr},  // ADR-033
    // Output stage + pan order (ADR-035). bassMono/bassMonoHz are SHELL
    // post-processing (side-channel high-pass, M/S); panScatter is core.
    {40, "bassMono", "Bass Mono", 0, 1, 0, true, kOffOn},
    {41, "bassMonoHz", "Bass XOver (Hz)", 60, 500, 120, false, nullptr},
    {42, "panScatter", "Pan Scatter", 0, 1, 0, false, nullptr},
    // Phase 4 (ADR-037): engine select + SPECTRA surface. Shared knobs
    // (K/onset/dissolve/seed/vol/retrig) are mirrored into both cores by
    // applyParam; ids 44-51 are SPECTRA-only.
    {43, "engine", "Engine", 0, 1, 0, true, kEngineLabels},
    {44, "partials", "Partials", 1, 32, 12, true, nullptr},
    {45, "tilt", "Amp Tilt", 0.5, 2, 1, false, nullptr},
    {46, "stretch", "Stretch", 0, 1, 0, false, nullptr},
    {47, "cloud", "Cloud Voices", 1, 7, 5, true, nullptr},
    {48, "cwidth", "Cloud Width", 0, 1, 0.25, false, nullptr},
    {49, "wtilt", "Width Tilt", -1, 1, 0, false, nullptr},
    {50, "wlaw", "Width Law", 0, 1, 0, true, kWlawLabels},
    {51, "cascade", "Cascade", 0, 1, 0, false, nullptr},
    // SPECTRA sub-oscillator (ADR-042; SPECTRA-only, ids route to spectra core)
    {52, "subOn", "Sub Osc", 0, 1, 0, true, kOffOn},
    {53, "subVol", "Sub Level", 0, 1, 0, false, nullptr},
    {54, "subWave", "Sub Wave", 0, 1, 0, false, nullptr},
    {55, "subOct", "Sub Octave", -3, -1, -1, true, nullptr},
    // Two-cluster A/B balance (ADR-051): sweeps cluster B from synced (0) to
    // splayed (1); default 0 is bit-inert. Two-cluster topology only.
    {56, "balance", "A/B Balance", 0, 1, 0, false, nullptr},
    // Internal FX rack (ADR-054, increment 1): 4 series slots, each a type +
    // amount, processed in slot order. Default type Off = bit-exact passthrough
    // (the parity gate). coreKeys are unique non-core strings — used only as
    // state-blob keys; apply/readParam intercept these ids and route to `rack`.
    {57, "fx1type", "FX1 Type", 0, 9, 0, true, kFxTypeLabels},
    {58, "fx1amt", "FX1 Amount", 0, 1, 0.5, false, nullptr},
    {59, "fx2type", "FX2 Type", 0, 9, 0, true, kFxTypeLabels},
    {60, "fx2amt", "FX2 Amount", 0, 1, 0.5, false, nullptr},
    {61, "fx3type", "FX3 Type", 0, 9, 0, true, kFxTypeLabels},
    {62, "fx3amt", "FX3 Amount", 0, 1, 0.5, false, nullptr},
    {63, "fx4type", "FX4 Type", 0, 9, 0, true, kFxTypeLabels},
    {64, "fx4amt", "FX4 Amount", 0, 1, 0.5, false, nullptr},
    // SPECTRA ADSR (ADR-055; SPECTRA-only, ids route to the spectra core).
    // SEPARATE from the SAW ADSR (ids 19-22): the two references have
    // different reference AR constants (SAW 3 ms/160 ms, SPECTRA 4 ms/180 ms),
    // so each engine carries its own envelope defaulting to ITS reference —
    // the plugin default must be reference-exact, not just the golden harness.
    // Renumbered 57-60 → 65-68 on the merge with the FX rack (ADR-054 owns 57-64).
    {65, "sAttack", "S.Attack (s)", 0.001, 2.0, 0.004, false, nullptr},
    {66, "sDecay", "S.Decay (s)", 0.005, 4.0, 0.18, false, nullptr},
    {67, "sSustain", "S.Sustain", 0, 1, 1.0, false, nullptr},
    {68, "sRelease", "S.Release (s)", 0.005, 8.0, 0.18, false, nullptr},
    // SAW waveshape morph (ADR-058): 0 = saw, 1 = square. SAW-core key, routed
    // by the applyParam fallback; default 0 is bit-inert (spectra no-ops "shape").
    // Renamed from "Saw Shape" (human 2026-08-20): two unrelated things carried
    // that name — this ADR-058 square morph in The swarm, and the ADR-094
    // Saw shape SECTION. The key stays `shape` (state compat); only the face moved.
    {69, "shape", "Squareness", 0, 1, 0, false, nullptr},
    // ADR-072 batched param pass (task #18): the fold-campaign features.
    // Ids START AT 71: id 70 is a GHOST — the ADR-059 dev inertia-taper
    // exponent is intercepted by number in applyParam/readParam without a row
    // in this table, so "max id in the table + 1" is NOT the next free id.
    // (Found the hard way: toneTilt landed on 70 first and its writes were
    // silently swallowed by the taper hook — the functional smoke caught it.)
    // (ADR-060..070) made host-reachable. Ranges are the AUDITIONED lab ranges
    // (detune-lab sliders / fold ADRs), not invented. All defaults are the
    // core's bit-inert defaults, so an unautomated session sounds identical.
    // "toneTilt", not "tilt": id 45 already uses the key "tilt" for SPECTRA's
    // amp tilt, and applyParam mirrors unguarded ids into BOTH cores by key —
    // a new id named "tilt" would write both. The core carries the alias.
    {71, "toneTilt", "Tone Tilt", -1, 1, 0, false, nullptr},        // ADR-060
    {72, "hiTame", "Hi Tame", 0, 1, 0, false, nullptr},             // ADR-061
    {73, "driftMode", "Drift Mode", 0, 2, 0, true, kDriftModeLabels},  // ADR-062
    {74, "keepPhase", "Keep Phase", 0, 1, 0, true, kOffOn},         // ADR-062
    {75, "freqGlide", "Freq Glide (s)", 0, 0.1, 0, false, nullptr}, // ADR-063 seconds (ADR-009)
    {76, "panMotion", "Pan Motion", 0, 1, 0, false, nullptr},       // ADR-064
    {77, "panMode", "Pan Motion Mode", 0, 1, 0, true, kPanModeLabels},  // ADR-064
    {78, "motionCenter", "Centre Pin", 0, 1, 0, false, nullptr},    // ADR-064
    {79, "harmReach", "Harmonic Reach", 0.25, 4, 1, false, nullptr},  // ADR-065
    {80, "stretchB", "Stretch B", 0, 6, 0, false, nullptr},         // ADR-066
    {81, "spread", "Octave Spread", 1, 24, 1, false, nullptr},      // ADR-068
    {82, "anchor", "Root Anchor", 0, 1, 0, false, nullptr},         // ADR-068
    {83, "pivotMode", "Pivot", 0, 1, 0, true, kPivotLabels},        // ADR-069
    {84, "panLayout", "Pan Image", 0, 1, 0, true, kPanLayoutLabels},  // ADR-070
    {85, "panCurve", "Fan Curve", 0, 1, 0.5, false, nullptr},       // ADR-070
    {86, "panInvert", "Fan Invert", 0, 1, 0, true, kOffOn},         // ADR-070
    // ADR-074 super-width mode: active only at width > 1. Default 0 = mode F
    // (clean ITD+steepening) — a deliberate default-output change at width > 1
    // versus the old always-M/S behavior, per the human's ratified ship list.
    {87, "superMode", "Super-Width Mode", 0, 2, 0, true, kSuperModeLabels},
    // ADR-075: opt-in 2x oscillator oversampling. Default 0 keeps every
    // existing session and all 147 goldens bit-identical; on costs ~2.5x the
    // core's CPU (measured 2.5% -> 6.3% of one core at 8 notes x 16 voices).
    {88, "oversample", "Oversample 2x", 0, 1, 0, true, kOffOn},
    // ADR-076: poly glide reuses the existing Glide TIME knob (id 33), which
    // therefore stops being mono-only in the GUI gating.
    // ADR-102: no longer read by the DSP (poly glide is automatic — on whenever
    // a travel law is engaged). Declared for state compatibility; "(dev)" is the
    // established exemption label (gui_reach exempts it like inertiaCurve).
    {89, "polyGlide", "Poly Glide (dev)", 0, 1, 1, true, kOffOn},
    {90, "glideMode", "Glide From", 0, 2, 0, true, kGlideModeLabels},
    // ADR-077 ensemble onset timing. onsetScatter is the master switch (0 = off
    // = bit-exact); alpha is the mutual-correction gain that carries the serial
    // structure listeners judge (LIBRARY L0019).
    {91, "onsetScatter", "Onset Scatter (ms)", 0, 80, 0, false, nullptr},
    {92, "onsetAlpha", "Timing Correction", 0, 1.5, 0.25, false, nullptr},
    {93, "attackScatter", "Attack Scatter", 0, 1, 0, false, nullptr},
    // ADR-078 per-voice envelopes. Off = one shared envelope (reference path).
    {94, "voiceEnv", "Per-Partial Env", 0, 1, 0, true, kOffOn},
    {95, "relScatter", "Release Scatter", 0, 1, 0, false, nullptr},
    // Per-slot SECOND axis for the FX rack (2026-08-03) — ADR-071 deferred the
    // comb's resonance "until the rack grows per-slot param pages"; this is it.
    // Deliberately ONE generic knob per slot rather than a comb-specific param,
    // so the next slot type that wants a second control costs no new ids.
    // Comb reads it as resonance (fb = 0.6 + 0.38*tone); 0.5 reproduces the
    // previously hardcoded 0.79 exactly, so every existing state loads unchanged.
    {96, "fx1tone", "FX1 Tone", 0, 1, 0.5, false, nullptr},
    {97, "fx2tone", "FX2 Tone", 0, 1, 0.5, false, nullptr},
    {98, "fx3tone", "FX3 Tone", 0, 1, 0.5, false, nullptr},
    {99, "fx4tone", "FX4 Tone", 0, 1, 0.5, false, nullptr},
    // ADR-059 DEV tune-then-lock: inertia knob taper exponent (0.5 == the sqrt
    // default). Shell-owned; re-derives inertia from the stored knob. Removed
    // once the human locks a value. coreKey is a non-core state key.
    /* ADR-024 A1 (human 2026-08-22, by ear then checked by arithmetic): 2.5,
       not 0.5. With w = knob^curve and the musically useful w range ~0.02..0.3,
       curve 0.5 squeezes that range into knob 0.0004..0.09 — the bottom 9% —
       while 2.5 spreads it across knob 0.21..0.62. ADR-024's INTENT was to
       spread the useful range; the exponent went the wrong way. Hidden now
       that it has a settled value; it stays a parameter so a patch can still
       carry a different taper. */
    {70, "inertiaCurve", "Inertia Curve (dev)", 0.3, 5, 2.5, false, nullptr},
    // MASTER VOLUME (B24 mixer, 2026-08-07) — the first id above 99, allocated
    // under Amendment 1's stride-1000 scheme. Needed because Amendment 1 made
    // `vol` (17) per-oscillator: after that there was NO patch-level fader at
    // all. Default 1.0 = unity, and the render skips the multiply at exactly
    // 1.0, so every existing patch is bit-identical. GLOBAL (in kGlobalIds).
    {100, "masterVol", "Master Volume", 0, 1.5, 1.0, false, nullptr},
    // GLOBAL PITCH (human, 2026-08-07): patch-level transpose summed with each
    // oscillator's own. UI range is the honest playing range (+/-12 st); the
    // MOD MATRIX is intended to drive pitch harder (to +/-48, clamped) when it
    // folds into the shell — recorded in ROADMAP so the widened drive does not
    // become an invisible feature (L0023).
    {101, "gSemi", "Pitch", -12, 12, 0, true, nullptr},
    {102, "gFine", "Fine", -100, 100, 0, false, nullptr},
    {103, "gOct", "Master Octave", -2, 2, 0, true, nullptr},
    // MUTE / SOLO (B24 mixer remainder, 2026-08-09) — PARAMS, not GUI state,
    // because the human asked for automation to reach them. Per-oscillator, so
    // oscillator 2 is 1104/1105. Shell-owned: they gate the mix stage and never
    // enter SwarmCore, so the parity goldens cannot see them.
    // Defaults 0/0 mean every gain is exactly 1.0 and the render skips the
    // multiply, so an untouched patch stays bit-identical.
    {104, "oscMute", "Mute", 0, 1, 0, true, kOffOn},
    {105, "oscSolo", "Solo", 0, 1, 0, true, kOffOn},
    /* BEND TRAVEL LAW (id 106+, ADR pending; folded 2026-08-19). GLOBAL — the
       wheel bends the patch, so these are not per-oscillator. Ranges and defaults
       are the REFERENCE's, read from docs/design/bend-lab.html's own controls, so
       a value set here means what it meant on the bench glide_check's goldens were
       sliced from.
       `bendLaw` ships OFF: the core calls kConstRate its "ratified default", but
       that is the bench's default for AUDITIONING, and shipping it would change how
       every existing patch bends (human ruling 2026-08-19). */
    {106, "bendLaw", "Bend Law", 0, 4, 0, true, kBendLawLabels},
    {107, "bendTime", "Bend Time (ms)", 5, 1500, 120, false, nullptr},
    {108, "bendRate", "Bend Rate (st/s)", 0.5, 200, 24, false, nullptr},
    {109, "bendTau", "Bend Lag (ms)", 1, 2000, 60, false, nullptr},
    {110, "bendSpringF", "Spring (Hz)", 0.5, 20, 4, false, nullptr},
    {111, "bendDamp", "Damping", 0, 1, 0.6, false, nullptr},
    {112, "bendDistOver", "Distance Curve", 0, 2, 1, false, nullptr},
    // BEND LANE ONLY, and the core enforces it: a note has no home pitch to
    // spring back to, so retMul is meaningless on the note-pitch lane.
    {113, "bendReturn", "Return x", 0.2, 3, 1, false, nullptr},
    {114, "bendQuant", "Bend Quantise", 0, 4, 0, true, kBendQuantLabels},
    {115, "bendHyst", "Quantise Hyst (c)", 0, 50, 8, false, nullptr},
    /* GLOBAL SCALE (ids 116-128). THE MASK IS THE TRUTH, THE NAME IS UI — the
       standing ruling. Consumers store and transmit `{root, mask}` only, never a
       scale ID, which is what keeps `glide_core.h` free of a scale table: adding
       a named scale becomes a UI-table edit with no core change and no parity
       surface, and a hand-drawn set is first-class rather than a degraded mode.
       Hence twelve honest booleans instead of one packed 0..4095 integer, which
       no host could automate meaningfully and no user could read. The named-scale
       dropdown lives in the GUI and WRITES these thirteen; it is not a parameter.
       Global because four consumers are already visible — the bend quantiser, the
       note-pitch lane, the chord layer, any arp — and two modules disagreeing
       about the scale produce notes in neither key. */
    {116, "scaleRoot", "Scale Root", 0, 11, 0, true, kNoteNames},
    {117, "scaleDeg0", "Degree 1 (root)", 0, 1, 1, true, kOffOn},
    {118, "scaleDeg1", "Degree b2", 0, 1, 0, true, kOffOn},
    {119, "scaleDeg2", "Degree 2", 0, 1, 1, true, kOffOn},
    {120, "scaleDeg3", "Degree b3", 0, 1, 0, true, kOffOn},
    {121, "scaleDeg4", "Degree 3", 0, 1, 1, true, kOffOn},
    {122, "scaleDeg5", "Degree 4", 0, 1, 1, true, kOffOn},
    {123, "scaleDeg6", "Degree b5", 0, 1, 0, true, kOffOn},
    {124, "scaleDeg7", "Degree 5", 0, 1, 1, true, kOffOn},
    {125, "scaleDeg8", "Degree b6", 0, 1, 0, true, kOffOn},
    {126, "scaleDeg9", "Degree 6", 0, 1, 1, true, kOffOn},
    {127, "scaleDeg10", "Degree b7", 0, 1, 0, true, kOffOn},
    {128, "scaleDeg11", "Degree 7", 0, 1, 1, true, kOffOn},
    /* SAW SHAPE (glass) — ADR-094, the fifth detune-lab fold. PER-OSCILLATOR:
       each oscillator gets its own saw character, which is the point of having
       two. Both axes default to 0 and each stage is guarded, so these are a
       parity-safe superset like ADR-060..063 before them. */
    {129, "sawBase", "Saw Base", 0, 1, 0, false, nullptr},
    {130, "sawProfile", "Roundness Shape", 0, 1, 0, false, nullptr},
    {131, "round", "Roundness", 0, 1, 0, false, nullptr},
    /* B60/ADR-133: BIPOLAR. The maths was always bipolar --
       `rnd[i] = clamp(round * (1 + roundHi * (2*up - 1)))` has `2*up - 1`
       running -1..+1 across the spread -- so a negative roundHi skews roundness
       toward the LOW voices with no formula change at all. Only the declared
       lower bound stood in the way (human 2026-08-27: "the other direction
       skewing the roundness to the low end voices instead of the high end").
       Parity-safe as a superset by the ADR-056 pattern: the default is 0,
       `1 + 0*x == 1`, and no golden sets it, so every golden is untouched. */
    {132, "roundHi", "Round x Pitch", -1, 1, 0, false, nullptr},
    /* FX SLOT MIX (ids 133-136) — the rack-owned dry/wet of the approved slot
       contract. GLOBAL, like the rest of the rack. Defaults to 1 so every patch
       predating the contract is bit-identical; 0 is a guaranteed bypass for EVERY
       slot type, which is what retires "amount means four different things". */
    {133, "fx1mix", "FX1 Mix", 0, 1, 1, false, nullptr},
    {134, "fx2mix", "FX2 Mix", 0, 1, 1, false, nullptr},
    {135, "fx3mix", "FX3 Mix", 0, 1, 1, false, nullptr},
    {136, "fx4mix", "FX4 Mix", 0, 1, 1, false, nullptr},
    /* NOTE-PITCH TRAVEL LANE (ids 137-145). The bend law and the note law are the
       same five-law GlideCore; what differs is what they travel. ADR-026's `glide`
       (id 33) WAS this lane, hard-wired to one law — a one-pole in Hz
       (swarm_core.h:1238, `coef = 1 - exp(-dt / glide)`). The law system supersedes
       that knob rather than sitting beside it, which is why id 33 is re-labelled
       into this block instead of a `noteTau` being minted: id 33 already holds a
       lag time in SECONDS, and `serum-parity-reference.json` already stores
       0.89 in it. Minting a twin in MILLIseconds would have put a silent 1000x
       between a shipped preset and its meaning.
       `noteLawLink` ships FOLLOW (human 2026-08-20: "it should default to follow
       bend law because it's quite confusing otherwise" — two independent laws
       shaping one pitch is a UI with no single answer to "what will this note
       do", and a divergent note law would need its own visualiser to be legible
       at all). This REVERSES the 2026-08-19 ruling, which shipped own-settings so
       that a patch storing `glide` kept its portamento; that compatibility is now
       carried by a state migration instead — see applyStateJson. retMul is absent by the same rule as the bend
       block above: a note has no home pitch to spring back to. */
    {137, "noteLawLink", "Note Law", 0, 1, 1, true, kNoteLinkLabels},
    {138, "noteLaw", "Note Travel", 0, 4, 3, true, kBendLawLabels},
    {139, "noteTime", "Note Time (ms)", 5, 1500, 120, false, nullptr},
    {140, "noteRate", "Note Rate (st/s)", 0.5, 200, 24, false, nullptr},
    {141, "noteSpringF", "Note Spring (Hz)", 0.5, 20, 4, false, nullptr},
    {142, "noteDamp", "Note Damping", 0, 1, 0.6, false, nullptr},
    {143, "noteDistOver", "Note Distance Curve", 0, 2, 1, false, nullptr},
    {144, "noteQuant", "Note Quantise", 0, 2, 0, true, kNoteQuantLabels},
    {145, "noteHyst", "Note Quant Hyst (c)", 0, 50, 8, false, nullptr},
    /* QUANTISE STEP TIMING (146-148). The gate itself is the REFERENCE's `qTime`
       in milliseconds — it has been in bend-lab since 2026-08-07 and simply had
       never been ported. These three are the shell's musical face for it: a mode,
       a free rate, and a tempo division. The core never learns about tempo, the
       same way it never learned about scale NAMES — the shell resolves both to
       the one number the core reads. `sync` reuses kGridSteps (cycles per beat)
       rather than minting a division table, so its snapping and its names are
       already the ones the tempo grid uses. Mode ships CONTINUOUS, which is
       qTime = 0 — the path every existing golden was sliced from. */
    {146, "bendQTimeMode", "Step Timing", 0, 2, 0, true, kQTimeModeLabels},
    {147, "bendQTimeHz", "Step Rate (Hz)", 0.2, 50, 8, false, nullptr},
    {148, "bendQTimeSync", "Step Grid", 0.25, 8, 4, false, nullptr},
    /* ADR-097. Ships FOLLOW because that is what the reference does — bend-lab
       has never had a way to give per-note bend a DIFFERENT character from the
       wheel; it steps both with the same P. Inert at defaults all the same:
       `bendLaw` ships off, so following it is the instant write either way. The
       toggle exists because per-note bend is the one lane where a player may
       want the raw controller under their finger while the wheel keeps its
       character. */
    {149, "bendMpeLaw", "MPE Bend", 0, 1, 1, true, kMpeLawLabels},
    /* ADR-100 (human 2026-08-20: "add the ability to turn oscillators off and on
       instead of just volume"). PER-OSC, so the morph grid can hold "off in this
       corner, on in that one" per corner per oscillator. OFF hard-kills the
       core's voices (a tail outliving the switch contradicts the switch) and the
       render skip makes it cost NOTHING — which is the difference from vol 0,
       where ADR-099's skip already applies but held notes keep their envelopes
       frozen for resume. Off = not part of the patch right now. */
    {150, "enable", "Osc On", 0, 1, 1, true, kOffOn},
    /* QUANTUM MORPH (ids 151-158, ADR-104; global — the morph field is a patch
       property). Ranges are the LAB's own controls. morphOn ships OFF, so every
       existing patch and golden is untouched — the parity-safe-superset rule.
       Seed is a stepped param: the patchwork's IDENTITY, automatable like any
       other, reshuffled deterministically when it changes. */
    {151, "morphOn", "Morph", 0, 1, 0, true, kOffOn},
    /* ADR-115: the field STARTS AT CORNER A, not in the middle. w[0] = (1-x)(1-y),
       so (0,0) is 100% A. The centre was the worst possible default on two
       counts the human named as one ("the middle is the messiest place on the
       grid and the most confusing to edit"): every corner weighs 0.25 there, so
       the Gumbel draw scatters parameters across all four and the patch you hear
       is a patchwork of four sources; and because an UNARMED edit lands on
       whichever corner owns that parameter (ADR-109), edits at the centre
       scatter into four different corners too. At 100% A every parameter is
       owned by A, so the field behaves exactly like a plain patch until you
       choose to move — which is the right first experience of a feature this
       strange. */
    {152, "morphX", "Morph X", 0, 1, 0.0, false, nullptr},
    {153, "morphY", "Morph Y", 0, 1, 0.0, false, nullptr},
    {154, "morphTemp", "Temperature", 0.02, 4, 1, false, nullptr},
    {155, "morphCoup", "Coupling", 0, 1, 0.3, false, nullptr},
    {156, "morphSeed", "Morph Seed", 1, 9999, 1024, true, nullptr},
    {157, "morphMode", "Morph Mode", 0, 1, 0, true, kMorphModeLabels},
    /* ADR-112 A2: this is THE morph rate, not a flip de-clicker. morphStep
       runs every target through its coefficient in BOTH modes; the old label
       and the flip-only GUI gate hid a control that was always in the path —
       "there isn't a morph rate slider" (human, 2026-08-22): there was, it
       was just lying about its job. Max widened 0.5 -> 5 s: a performance
       morph time, not a smoothing constant. Stored patches (<= 0.5) keep
       their value; CLAP params carry plain values, so no renormalisation. */
    {158, "morphGlide", "Morph Glide (s)", 0, 5, 0.008, false, nullptr},
    /* CORNER EDITING (ADR-109, the human's 2026-08-19 model). `morphArm` is the
       four colour boxes: 0 = none armed, 1..4 = corner A..D. Global, and
       deliberately NOT morphable — an edit-routing mode that morphed would
       change where your edits land as you move the pad. */
    {159, "morphArm", "Edit Corner", 0, 4, 0, true, kMorphArmLabels},
    /* B38 (human 2026-08-24: "an optional per-voice gate that kills voices when
       they go below a chosen threshold"). NOT a new mechanism -- swarm_core has
       always retired a slot below a hard-coded 1e-4; this exposes the constant.
       In dB because that is the unit the trade is heard in, and because a
       linear readout of 0.0001 tells the player nothing.
       DEFAULT -80 IS LOAD-BEARING: it is exactly the shipped constant, which is
       the only reason this is a parity-safe superset and every golden is
       untouched. Raising it is AUDIBLE (-40 dB is clearly present in a quiet
       mix), so it is a CPU/quality trade the player makes deliberately -- the
       label says "cull" and the unit says dB for that reason.
       Global and non-morphable: a voice-lifecycle policy that morphed would
       change how long notes ring as you move the pad. */
    /* PER-SLOT TIME-ENGINE PARAMETERS (ADR-131). One block of 8 ids per slot at
       200 + slot*8, seven used and one spare, so a slot's page can grow without
       renumbering. GLOBAL, like every other rack id: the rack is post-mix and
       there is exactly one of it. Every row is `shown_when fxNtype=7|8` in the
       presentation table, so the controls appear only on a slot actually
       holding Echo or Room -- the same mechanism `topo`/`bendLaw` already use,
       and the reason a slot page can be type-specific without the GUI owning a
       second copy of what is live (ADR-108). */
    {200, "fx1size", "FX1 Size", 0, 1, 0.55, false, nullptr},
    {201, "fx1spread", "FX1 Spread", 0, 1, 0.6, false, nullptr},
    {202, "fx1taps", "FX1 Taps/Lines", 2, 12, 8, true, nullptr},
    {203, "fx1damp", "FX1 Damping", 0, 1, 0.4, false, nullptr},
    {204, "fx1noise", "FX1 Noise", 0, 1, 0.2, false, nullptr},
    {205, "fx1stereo", "FX1 Stereo", 0, 1, 0.7, false, nullptr},
    {206, "fx1dist", "FX1 Spacing", 0, 4, 1, true, kDistLabels},
    {208, "fx2size", "FX2 Size", 0, 1, 0.55, false, nullptr},
    {209, "fx2spread", "FX2 Spread", 0, 1, 0.6, false, nullptr},
    {210, "fx2taps", "FX2 Taps/Lines", 2, 12, 8, true, nullptr},
    {211, "fx2damp", "FX2 Damping", 0, 1, 0.4, false, nullptr},
    {212, "fx2noise", "FX2 Noise", 0, 1, 0.2, false, nullptr},
    {213, "fx2stereo", "FX2 Stereo", 0, 1, 0.7, false, nullptr},
    {214, "fx2dist", "FX2 Spacing", 0, 4, 1, true, kDistLabels},
    {216, "fx3size", "FX3 Size", 0, 1, 0.55, false, nullptr},
    {217, "fx3spread", "FX3 Spread", 0, 1, 0.6, false, nullptr},
    {218, "fx3taps", "FX3 Taps/Lines", 2, 12, 8, true, nullptr},
    {219, "fx3damp", "FX3 Damping", 0, 1, 0.4, false, nullptr},
    {220, "fx3noise", "FX3 Noise", 0, 1, 0.2, false, nullptr},
    {221, "fx3stereo", "FX3 Stereo", 0, 1, 0.7, false, nullptr},
    {222, "fx3dist", "FX3 Spacing", 0, 4, 1, true, kDistLabels},
    {224, "fx4size", "FX4 Size", 0, 1, 0.55, false, nullptr},
    {225, "fx4spread", "FX4 Spread", 0, 1, 0.6, false, nullptr},
    {226, "fx4taps", "FX4 Taps/Lines", 2, 12, 8, true, nullptr},
    {227, "fx4damp", "FX4 Damping", 0, 1, 0.4, false, nullptr},
    {228, "fx4noise", "FX4 Noise", 0, 1, 0.2, false, nullptr},
    {229, "fx4stereo", "FX4 Stereo", 0, 1, 0.7, false, nullptr},
    {230, "fx4dist", "FX4 Spacing", 0, 4, 1, true, kDistLabels},
    {160, "voiceCull", "Voice Cull", -80, -40, -80, false, nullptr},
    /* MOD MATRIX increment 2 (B69): the matrix reaches the audio path through
       ONE route — ENV 1 (the amp envelope's loudest-voice projection) to the
       ADR-027 tune sum. This knob is that route's depth, in semitones,
       bipolar so the envelope can dive as well as rise (the ADR-056/133
       superset pattern: default 0 = no route = byte-identical output).
       It is ALSO B64's pitch envelope in functional form — same ADSR as the
       amp envelope for now; a dedicated ENV 2 with its own times is the next
       increment, and this knob then becomes ENV 2's route without renaming. */
    {161, "modEnvPitch", "Env > Pitch", -48, 48, 0, false, nullptr},
    /* B64 completed (ADR-135): ENV 2, the dedicated pitch envelope. Its OWN
       times, computed in the shell at the mod grid — a mod SOURCE, not a copy
       of the core's amp envelope. Sustain defaults 0: a pitch envelope that
       returns to base pitch while the note holds is the musical default, and
       it is what makes ENV 2 audibly a different envelope from ENV 1.
       Route 0 (the Env > Pitch knob) now draws from ENV 2. ENV 1 (the amp
       projection, source slot 0) remains auto-included for future routes. */
    {162, "penvA", "P.Env Attack (s)", 0.001, 2.0, 0.003, false, nullptr},
    {163, "penvD", "P.Env Decay (s)", 0.005, 4.0, 0.16, false, nullptr},
    {164, "penvS", "P.Env Sustain", 0, 1, 0, false, nullptr},
    {165, "penvR", "P.Env Release (s)", 0.005, 8.0, 0.16, false, nullptr},
    /* ADR-137: eight MACROS + the XY assignment. A macro is a mod SOURCE
       (slots 2-9) with a knob on MAIN; the per-osc XY pad is a CONTROLLER of
       macros — its axes write the assigned macro params — per the human's
       ruling that the XY grids become macro controllers with variable
       assignments. All twelve stay OUT of the morph field (a corner that
       reassigned your controller mid-morph would be a trap, not a timbre) and
       OUT of the destination menu (macro-as-dest is fan-out, B70-adjacent,
       refused until ruled). Assignment defaults 0/1/2/3: osc 1's pad drives
       M1/M2, osc 2's M3/M4, and M5-8 start knob-only. */
    /* 0.5, not 0 (human 2026-08-30: "the default setting needs to center the
       main XY"): with floor defaults + 100% depth on the default routes,
       centred macros land detune at 0.5 and K at 0 — K's OLD default exactly,
       detune a touch wider than the old 0.28. The pad rests centred. */
    {166, "macro1", "Macro 1", 0, 1, 0.5, false, nullptr},
    {167, "macro2", "Macro 2", 0, 1, 0.5, false, nullptr},
    {168, "macro3", "Macro 3", 0, 1, 0, false, nullptr},
    {169, "macro4", "Macro 4", 0, 1, 0, false, nullptr},
    {170, "macro5", "Macro 5", 0, 1, 0, false, nullptr},
    {171, "macro6", "Macro 6", 0, 1, 0, false, nullptr},
    {172, "macro7", "Macro 7", 0, 1, 0, false, nullptr},
    {173, "macro8", "Macro 8", 0, 1, 0, false, nullptr},
    {174, "xyAsn0X", "XY1 X > Macro", 0, 8, 0, true, nullptr},
    {175, "xyAsn0Y", "XY1 Y > Macro", 0, 8, 1, true, nullptr},
    {176, "xyAsn1X", "XY2 X > Macro", 0, 8, 2, true, nullptr},
    {177, "xyAsn1Y", "XY2 Y > Macro", 0, 8, 3, true, nullptr},
    /* ADR-140: the CHROME-001 specimen is OFF by default — measured untenable
       ("jumpy, jaggy") in the VST on 2026-08-28. On = the reduced-cost render
       (low fixed resolution, fewer march steps, 20 Hz, idle-gated); off = the
       phase circle returns to MAIN. The native-GUI escape is B75. */
    {178, "specimen", "Specimen (CHROME-002)", 0, 1, 1, true, nullptr},
    /* ADR-150: MAIN's XY is its OWN pad (human: "the main XY needs to be its
       own XY separate from the OSC XYs; I didn't realize it wasn't yet") —
       its own assignment pair, not a view of the active osc's.

       WIDENED 0..8 -> 0..10 on 2026-09-19 (the human: "let's make pitch bend and
       mod wheel accessible to the MAIN XY as well as the macros"). The two new
       values are APPENDED, never inserted: 0..7 stay Macro 1..8 and 8 stays
       None, so every stored patch keeps its meaning bit for bit. 9 = Pitch
       Bend, 10 = Mod Wheel — both are written by the EDITOR through the paths
       those signals already own (param 38 for bend, hostIf.setModWheel for the
       wheel), so the shell learns the two values only here and in the decoder
       below.

       The host-facing NAME still reads "> Macro" although 9/10 are not macros:
       renaming a shipped parameter is a public-interface change and needs the
       human's gate, so it is left for that ruling rather than taken here. */
    {179, "mainAsnX", "Main X > Macro", 0, 10, 0, true, nullptr},
    {180, "mainAsnY", "Main Y > Macro", 0, 10, 1, true, nullptr},
    /* ADR-150: continuous per-osc pitch, in semitones — the transposition
       knobs (octave/semi) are stepped so the morph ARGMAX-jumps them; this
       one BLENDS. Per-osc (not in kGlobalIds), so morphInit auto-includes it
       and its twin — smooth pitch morphing for free. */
    {181, "oscPitch", "Pitch (cont.)", -24, 24, 0, false, nullptr},
    /* ADR-142 — the standard Delay's per-slot params: 232..263, four blocks of
       8, the same shape ADR-131 gave the time engines (slot = (id-232)/8, key
       = (id-232)%8), so a fifth slot or a ninth param is a table edit and never
       a switch to keep in step. Times are LINEAR ms here rather than log: the
       GUI's data-log10 owns the control curve (ADR's log-control rule), and the
       parameter the host automates stays in real milliseconds. */
    {232, "d1time", "D1 Time (ms)", 1, 2000, 375, false, nullptr},
    {233, "d1sync", "D1 Sync", 0, 1, 0, true, kDelaySyncLabels},
    {234, "d1beats", "D1 Beats", 0.0625, 8, 0.5, false, nullptr},
    {235, "d1offR", "D1 R Offset", 0.25, 2, 1, false, nullptr},
    {236, "d1fb", "D1 Feedback", 0, 1, 0.35, false, nullptr},
    {237, "d1cross", "D1 Crossfeed", 0, 1, 0, false, nullptr},
    {238, "d1damp", "D1 Damp", 0, 1, 0.35, false, nullptr},
    {239, "d1hp", "D1 Loop HP (Hz)", 0, 500, 60, false, nullptr},
    {240, "d2time", "D2 Time (ms)", 1, 2000, 375, false, nullptr},
    {241, "d2sync", "D2 Sync", 0, 1, 0, true, kDelaySyncLabels},
    {242, "d2beats", "D2 Beats", 0.0625, 8, 0.5, false, nullptr},
    {243, "d2offR", "D2 R Offset", 0.25, 2, 1, false, nullptr},
    {244, "d2fb", "D2 Feedback", 0, 1, 0.35, false, nullptr},
    {245, "d2cross", "D2 Crossfeed", 0, 1, 0, false, nullptr},
    {246, "d2damp", "D2 Damp", 0, 1, 0.35, false, nullptr},
    {247, "d2hp", "D2 Loop HP (Hz)", 0, 500, 60, false, nullptr},
    {248, "d3time", "D3 Time (ms)", 1, 2000, 375, false, nullptr},
    {249, "d3sync", "D3 Sync", 0, 1, 0, true, kDelaySyncLabels},
    {250, "d3beats", "D3 Beats", 0.0625, 8, 0.5, false, nullptr},
    {251, "d3offR", "D3 R Offset", 0.25, 2, 1, false, nullptr},
    {252, "d3fb", "D3 Feedback", 0, 1, 0.35, false, nullptr},
    {253, "d3cross", "D3 Crossfeed", 0, 1, 0, false, nullptr},
    {254, "d3damp", "D3 Damp", 0, 1, 0.35, false, nullptr},
    {255, "d3hp", "D3 Loop HP (Hz)", 0, 500, 60, false, nullptr},
    {256, "d4time", "D4 Time (ms)", 1, 2000, 375, false, nullptr},
    {257, "d4sync", "D4 Sync", 0, 1, 0, true, kDelaySyncLabels},
    {258, "d4beats", "D4 Beats", 0.0625, 8, 0.5, false, nullptr},
    {259, "d4offR", "D4 R Offset", 0.25, 2, 1, false, nullptr},
    {260, "d4fb", "D4 Feedback", 0, 1, 0.35, false, nullptr},
    {261, "d4cross", "D4 Crossfeed", 0, 1, 0, false, nullptr},
    {262, "d4damp", "D4 Damp", 0, 1, 0.35, false, nullptr},
    {263, "d4hp", "D4 Loop HP (Hz)", 0, 500, 60, false, nullptr},
    /* B117 / ADR-163 — the FX-presence ruling's INSTRUMENT, not a feature.
       Under both morph modes a slot's type flips atomically (B49), so a tail is
       cut and a module enters with no lead-in; the bounded pool (B95) fixes
       that structurally at 1.1. These two let the human HEAR the alternative
       first. Default 0 = atomic = today, to the bit. "(dev)" is the established
       label for a control that is not product surface (id 70 is the precedent);
       after the ruling the control is either buried — id kept so stored state
       still loads — or promoted to a setting. */
    // B117 RULED 2026-09-16 (ADR-163 A2): the toggle is BURIED — no control in any
    // GUI, ids kept so saved state loads, default flipped to crossfade so the
    // ruled behaviour is what a patch that never wrote the id gets.
    {264, "fxXfade", "FX Type Crossfade (dev)", 0, 1, 1, true, kFxXfadeLabels},
    {265, "fxXfadeMs", "FX Crossfade Time (dev)", 5, 500, 80, false, nullptr},
    /* B89 phase 2b / ADR-176 decision 6 — THE INTENT-BUS FLAG, the id the row
       below has been holding open. Default OFF, so every patch that never
       writes it renders exactly as it did: the flag IS the bit-identity claim,
       and parity_check / statefix_check / intent_check section S are where it
       is proven. "(dev)" is the established label for a control that is not
       product surface (id 70 is the precedent, 264/265 the recent one).
       It does NOTHING with morph off (plan R15): bindings live in corners, and
       with no field there is no owner — so the seam is narrowed to morph-on
       patches by construction rather than by a second guard. */
    {266, "intentBus", "Intent Bus (dev)", 0, 1, 0, true, kOffOn},
    /* B146 bass-mono placement, RATIFIED 2026-09-18. Id 267, not 266: 266 is
       reserved for the intent flag, and an id skipped on purpose is cheaper
       than an id claimed twice. Default 0 = pre = the stage's only placement
       until today, so a patch that never writes this renders bit-identically.
       Device class (kParamClassOverrides) — it selects where a stage runs, not
       a timbre a morph corner should hold. */
    {267, "bassMonoPos", "Bass Mono Position", 0, 2, 0, true, kBassMonoPosLabels},
    /* B89 phase 2d / SPEC-INTENT-BUS §5 (Latch) — the performance pad's spring
       defeat. A PARAMETER and not a shell toggle because it is a performance
       state a patch and a host both have to be able to hold: latched, the puck
       stays where the player left it, so the displacement — and therefore every
       intent riding it — survives the release. Default off = the spring, which
       is the pad's whole character (ADR-176 §5: the MAIN XY is the performance
       pad), so a patch that never writes this behaves exactly as it did.
       Device class: it is a property of the control surface, not a timbre a
       morph corner should own — and the pad's home already IS corner-owned,
       which is the part that flips. */
    {268, "intentLatch", "Pad Latch (dev)", 0, 1, 0, true, kOffOn},
    /* B171 — TWO LFOs AND TWO MORE ENVELOPES, as modulation SOURCES (mod slots
       18-21). Ids 269-288, contiguous and appended: the slot indices are frozen
       in the `modroutes` chunk, so sources only ever APPEND, and the ids follow
       the same rule for the same reason. ALL GLOBAL (kGlobalIds) and all Device
       class (kParamClassOverrides) — §3.2 puts a global mod source there, which
       is also what keeps them out of buildMorphOrder's frozen prefix.

       `lfoNBeats` MATCHES THE DELAY'S BEATS PARAM (234/242/250/258): the same
       0.0625..8 continuous range in BEATS, rendered as a knob with unit
       `/beat`. The brief asked for "a division list ... reuse its label table";
       there is no such table — d1beats is continuous and d1sync's labels are
       the free/sync pair — so matching the delay means matching what it IS.
       Default 1 = one beat = a 1/4 note (the delay's law is
       `seconds = beats * 60/bpm`, and bpm counts quarter notes).

       NOT MODULATION DESTINATIONS in this increment (see modAddRoute): an LFO
       modulating an LFO is a later feature with its own cycle rule. */
    {269, "lfo1Rate", "LFO 1 Rate (Hz)", 0.02, 40, 1, false, nullptr},
    {270, "lfo1Shape", "LFO 1 Shape", 0, 5, 0, true, kLfoShapeLabels},
    {271, "lfo1Sync", "LFO 1 Time Mode", 0, 1, 0, true, kLfoSyncLabels},
    {272, "lfo1Beats", "LFO 1 Beats", 0.0625, 8, 1, false, nullptr},
    {273, "lfo1Retrig", "LFO 1 Retrigger", 0, 1, 0, true, kLfoRetrigLabels},
    {274, "lfo1Phase", "LFO 1 Start Phase", 0, 1, 0, false, nullptr},
    {275, "lfo2Rate", "LFO 2 Rate (Hz)", 0.02, 40, 1, false, nullptr},
    {276, "lfo2Shape", "LFO 2 Shape", 0, 5, 0, true, kLfoShapeLabels},
    {277, "lfo2Sync", "LFO 2 Time Mode", 0, 1, 0, true, kLfoSyncLabels},
    {278, "lfo2Beats", "LFO 2 Beats", 0.0625, 8, 1, false, nullptr},
    {279, "lfo2Retrig", "LFO 2 Retrigger", 0, 1, 0, true, kLfoRetrigLabels},
    {280, "lfo2Phase", "LFO 2 Start Phase", 0, 1, 0, false, nullptr},
    /* ENV 3 and ENV 4 carry EXACTLY ENV 2's ranges, units and defaults (162-165)
       because they run EXACTLY ENV 2's law — advanceAdsr() is the one copy, and
       three envelopes reading three different ranges off one law would be three
       ways to describe the same contour. */
    {281, "env3A", "ENV 3 Attack (s)", 0.001, 2.0, 0.003, false, nullptr},
    {282, "env3D", "ENV 3 Decay (s)", 0.005, 4.0, 0.16, false, nullptr},
    {283, "env3S", "ENV 3 Sustain", 0, 1, 0, false, nullptr},
    {284, "env3R", "ENV 3 Release (s)", 0.005, 8.0, 0.16, false, nullptr},
    {285, "env4A", "ENV 4 Attack (s)", 0.001, 2.0, 0.003, false, nullptr},
    {286, "env4D", "ENV 4 Decay (s)", 0.005, 4.0, 0.16, false, nullptr},
    {287, "env4S", "ENV 4 Sustain", 0, 1, 0, false, nullptr},
    {288, "env4R", "ENV 4 Release (s)", 0.005, 8.0, 0.16, false, nullptr},
};

// THE DEFAULT OF A PARAMETER, DEFINED ONCE. Both CLAP (`clap_param_info.
// default_value`) and the GUI bridge ask here, so a host's "reset to default"
// and the GUI's double-click cannot disagree. They already could: oscillators
// above the first default to SILENT, and a GUI reading the default out of its
// own markup restored 0.4 to a parameter CLAP reports as 0.0. File scope
// deliberately — it depends on nothing but the row and the oscillator index.
static double defaultFor(const ParamDef &d, uint32_t osc)
{
  if (d.id == 150) return osc > 0 ? 0.0 : 1.0;   // osc 2 ships OFF (ADR-099 A1)
  /* The vol-0 twin default RETIRED (ADR-100 A3): with the enable switch as the
     off state, osc 2 shipping enable=0 AND vol=0 was two safeties on one door --
     the human clicked power ON, it worked, and heard nothing because the
     volume was still zero: "the power buttons don't work." One gate, the
     honest one. Old patches carry explicit vol values and are untouched. */
  return d.defV;
}
constexpr uint32_t kNumParams = sizeof(kParams) / sizeof(kParams[0]);

/* ---- ADR-082 multi-oscillator namespace (increment 1: mechanism only) -----
   id(P, osc k) = id(P, osc 0) + 100k.  Oscillator 0 keeps every id it has, so
   every existing session, automation lane and patch survives untouched. CLAP
   ids are APPEND-ONLY: this mapping is designed once or lived with forever.

   kNumOsc is 1 here ON PURPOSE. Increment 1 lands the id/state mechanism with
   the oscillator count unchanged, so params_count(), the id list and the saved
   state bytes are all bit-identical to before — which is exactly what makes
   the parity/state oracles a proof that the refactor is inert. Increment 2
   raises it to 2 (the ratified slot count) and adds the second core. */
// STRIDE 1000, NOT 100 (amendment, 2026-08-06 — see ADR-082 Amendment 1).
// The stride is also the CAPACITY of oscillator 0's block, and at stride 100
// that block was ids 1..99 with ZERO free slots: the instrument already had 99
// params, so it could never gain another one. A new param at id 100 is not
// merely cramped, it is UNREACHABLE — findParam computes osc = id/kOscStride,
// so 100 resolves to oscillator 1 / base 0 and is never found. 1000 leaves 900
// free slots and costs nothing to adopt today, because increment 1 shipped at
// kNumOsc == 1 and no id >= 100 has ever been exposed to a host.
// Chunk the extra oscillators' render through a fixed stack buffer. Small
// enough to be free on the stack, large enough that the loop overhead is
// irrelevant against a render of the same length.
// ---- BEND TRAVEL GRID (ADR-086 Amendment 1's construction, reused) ----------
// The bend glide advances on a fixed TIME grid, not a fixed sample count. The
// amendment exists because the first version of that idea (kGravGrid = 256
// SAMPLES) was a duration that shrank as the sample rate rose, so the trajectory
// tracked the rate; expressed in seconds it obeys ADR-009 like every other time
// constant. The value is exactly 16/44100 so the grid is EXACTLY 16 samples at
// 44.1 kHz — which is the rate `bend-lab.html` was benched at and therefore the
// rate glide_check's goldens encode. Any other value silently invalidates them.
constexpr double kBendGridSeconds = 16.0 / 44100.0;   // 0.363 ms

constexpr int kMixChunk = 256;
constexpr uint32_t kOscStride = 1000;
// B69 mod-matrix destination keys. Opaque to mod_core; the shell owns meaning.
// SYNTHETIC destinations live in high-bit space so they can never collide with
// a CLAP param id (kModDestPitch was 1, which is param "n" — a landmine found
// before it fired, moved in ADR-136). A generic destination IS its param id.
constexpr uint32_t kModDestSynthetic = 0x80000000u;
constexpr uint32_t kModDestPitch = kModDestSynthetic | 1;
constexpr uint32_t kMaxOsc = 2;   // ratified 2026-08-06; 2000-2999 stays free for a third
constexpr uint32_t kNumOsc = 2;   // ADR-082 increment 2: the ratified slot count
static_assert(kNumOsc >= 1 && kNumOsc <= kMaxOsc, "kNumOsc outside the ratified range");

/* GLOBAL params — one instance no matter how many oscillators exist. Everything
   NOT listed is per-oscillator. Itemised in ADR-082; the three judgement calls
   (amp env global; transpose per-osc; retrig/keepPhase per-osc) are recorded
   there rather than buried here, because a param in the wrong class is wrong
   permanently. */
constexpr clap_id kGlobalIds[] = {
    // A12 (human-ratified 2026-08-11): the amp envelope (19-22) and beatMult
    // (23) LEFT this list. Envelope, because a fast-attack oscillator layered
    // against a slow swell is a basic two-oscillator move and one shared
    // envelope makes the second oscillator a timbre-only layer. beatMult,
    // because it is a parameter OF the tempo-grid detune law and `detune`/`law`
    // are already per-oscillator — so an oscillator could pick the law but not
    // its own grid. bpm stays host-owned and global; beatMult is the per-source
    // ratio to it.
    15, 40, 41, 267,                             // output & image (267 = B146 placement)
    // NB: 14 "width" left this list 2026-08-07 (A12, human-ruled: "oscillators
    // will independently need their own width controls"). It is a SwarmCore
    // param, so each oscillator always had its own copy — global classification
    // just made oscillator 2's unreachable. mono (15) stays global pending the
    // rest of the A12 ruling.
    // NB: 17 "vol" is NOT here. It is the swarm's own output gain, computed
    // inside SwarmCore::render — so it is PER-OSCILLATOR, and it is what lets
    // two oscillators be balanced against each other. A patch-level master
    // volume, if wanted, is a separate new param (the stride-1000 amendment
    // leaves room for one).
                                 // amp envelope (voice-level, not per-osc)
    32, 33, 34, 38, 75, 89, 90, 11, 70,          // voice & glide behaviour
    57, 58, 59, 60, 61, 62, 63, 64, 96, 97, 98, 99,  // FX rack
    88,                                      // tempo grid, oversampling
    100, 101, 102, 103,                          // masterVol + global pitch
    106, 107, 108, 109, 110, 111, 112, 113, 114, 115,  // bend travel law (global: the wheel bends the patch)
    133, 134, 135, 136,                          // FX slot mix (rack-owned dry/wet)
    137, 138, 139, 140, 141, 142, 143, 144, 145,  // note travel law (global: it joins id 33, already here)
    146, 147, 148, 149,                          // quantise step timing + per-note bend law
    151, 152, 153, 154, 155, 156, 157, 158, 159,  // quantum morph + corner-edit arming
    116, 117, 118, 119, 120, 121, 122, 123, 124,     // global scale: root + twelve degrees
    125, 126, 127, 128,                          // (the mask is the truth; the name is UI)
    160,                                         // B38 voice-cull threshold (lifecycle policy)
    161,                                         // B69 mod route depth (Env > Pitch)
    162, 163, 164, 165,                          // ADR-135 ENV 2 (pitch envelope) ADSR
    166, 167, 168, 169, 170, 171, 172, 173,      // ADR-137 macros (mod sources 2-9)
    174, 175, 176, 177,                          // ADR-137 per-osc XY axis assignment
    178,                                         // ADR-140 specimen on/off (GUI-only)
    179, 180,                                    // ADR-150 MAIN pad's own assignment
    232, 233, 234, 235, 236, 237, 238, 239,      // ADR-142 Delay slot 1
    240, 241, 242, 243, 244, 245, 246, 247,      // ADR-142 Delay slot 2
    248, 249, 250, 251, 252, 253, 254, 255,      // ADR-142 Delay slot 3
    256, 257, 258, 259, 260, 261, 262, 263,      // ADR-142 Delay slot 4
    264, 265,                                    // B117 FX crossfade (dev) — the rack is ONE object
    266,                                         // B89 intent-bus flag (dev) — one resolver per device
    268,                                         // B89 pad latch — one performance pad per device
    269, 270, 271, 272, 273, 274,                // B171 LFO 1 (mod source slot 18)
    275, 276, 277, 278, 279, 280,                // B171 LFO 2 (mod source slot 19)
    281, 282, 283, 284,                          // B171 ENV 3 (mod source slot 20)
    285, 286, 287, 288,                          // B171 ENV 4 (mod source slot 21)
    // ADR-131 per-slot time-engine params: 200..231, four blocks of 8.
    200, 201, 202, 203, 204, 205, 206,
    208, 209, 210, 211, 212, 213, 214,
    216, 217, 218, 219, 220, 221, 222,
    224, 225, 226, 227, 228, 229, 230,
};
constexpr bool isGlobalId(clap_id id)
{
  for (clap_id g : kGlobalIds)
    if (g == id) return true;
  return false;
}
// How many of the 99 are per-oscillator — the size of each additional block.
inline uint32_t perOscParamCount()
{
  uint32_t n = 0;
  for (const auto &d : kParams)
    if (!isGlobalId(d.id)) n++;
  return n;
}

// Which oscillator an id addresses, and the osc-0 id it mirrors. Global ids
// always resolve to oscillator 0 — they have no counterpart in the higher
// blocks, which is why those slots are never allocated.
inline uint32_t oscOfId(clap_id id) { return (uint32_t)id / kOscStride; }
inline clap_id baseIdOf(clap_id id) { return (clap_id)((uint32_t)id % kOscStride); }

/* ---- ADR-088 ROUTING BLOCK — THE ID LAYOUT, WRITTEN DOWN ONCE -------------
   The crosspoint matrix's own parameter namespace, deliberately far above the
   instrument's 1..999 x kOscStride space and POSITIONAL rather than curated:
   an id is COMPUTED from the cell it names, never assigned by hand.

     coeff[row][to]    10000 + row*64 + to      -> 10000 .. 14095
     outAmount[to]     20000 + to               -> 20000 .. 20063
     slotInit[to]      21000 + to               -> 21000 .. 21063
     srcOut[src]       22000 + src              -> 22000 .. 22063   (B50 1c)

   THE ROW IS NOT THE MATRIX INDEX (ADR-088 amendment, 2026-09-18). Rows 0..7
   are reserved for SOURCES — `kRoutingMaxSrc`, eight of them: the two swarm
   oscillators, the sub, and five unclaimed — and slots begin at row
   `kRoutingMaxSrc`. routing_core.h still indexes `coeff[from][to]` with SOURCES
   first then `NSRC + slot`, so the two coordinate spaces diverge the moment a
   source is added or removed; `routingRowOfIndex` / `routingIndexOfRow` below
   are the ONLY translation, and everything that names a cell by id speaks ROWS.

   WHY THE RESERVED BLOCK, AND WHY IT COST A RENUMBERING. The original layout
   packed rows as `NSRC + slot`, which made the id space append-only for SLOTS
   and NOT for sources: raising NSRC 1 -> 2 moved every slot's row by 64 and
   re-pointed six live ids (PR #636 measured it — id 10065 went from Slot 1 ->
   Slot 2 to Src 2 -> Slot 2). Reserving the source block decouples the two, so
   this is the LAST time any routing id moves. Taken on the human's ruling
   2026-09-18, two days after the ids were released in dev builds and before any
   user patch existed: "Let's renumber now, I haven't built any new presets or
   saved any files that would lean on them." No migration shim exists because no
   tracked file carries a chunk written under the old layout — the factory bank
   is regenerated from this build and no fixture carries a routing.

   WHY POSITIONAL, AND WHY 64. Ids are append-only from this release, and the
   thing that grows here is the matrix's SHAPE, not a list of features: B23
   increment 3 adds per-oscillator sources and the FX rework adds up to ~13
   modules. A sequentially-assigned block would have to renumber every cell to
   widen by one slot; a positional one simply lights up ids that were always
   reserved for those coordinates. 64 is wide enough for the eight reserved
   source rows plus every slot routing_core.h's own `NSRC + NSLOT <= 32`
   static_assert can admit, so the layout cannot be outgrown before the
   crosspoint mask is — and the mask is the harder limit.

   PHASE 1 EXPOSES THE ACYCLIC SUBSET ONLY (B50 (f)). `edgeLive()` was widened
   by ADR-128 to accept every edge (cycle edges read zPrev, one sample late), so
   legality alone no longer names the forward graph. The predicate for "does
   this cell get a parameter today" is therefore `edgeLive && edgeForward` —
   BOTH from routing_core.h, because the shell owning its own copy of "which
   edges are live" is the exact duplication that header forbids. When feedback
   cells are exposed they take the ids this layout already reserves for them;
   nothing renumbers. */
/* B23 increment 3: TWO sources, one per swarm oscillator. The sub-oscillator is
   the third and takes row 2 when it lands — no id moves for it.
   B172 LANDED IT, and the promise held: raising this to 3 moved no routing id.
   Row 2 was already reserved (rows 0..7 are the source block), so the only new
   ids are the cells row 2 itself names — `routingIndexOfRow` already returned
   -1 for it and now returns 2, and every slot row keeps the row number it had.
   Row 2 is the SUB; its caption in the matrix well is "SUB". */
constexpr int kRoutingNSrc = 3;
constexpr int kRoutingMaxSrc = 8;              // reserved source ROWS (see above)
using RoutingMatrixT = hypersaw::RoutingMatrix<kRoutingNSrc, hypersaw::kRackSlots>;
constexpr int kRoutingNSlot = hypersaw::kRackSlots;
static_assert(kRoutingNSrc <= kRoutingMaxSrc, "sources must fit the reserved row block");

constexpr uint32_t kRoutingIdBase = 10000;     // every routing id is >= this
constexpr uint32_t kRoutingFromStride = 64;
static_assert(kRoutingMaxSrc + kRoutingNSlot <= (int)kRoutingFromStride,
              "the row space is 64 wide");
constexpr uint32_t kRoutingCoeffBase = 10000;
constexpr uint32_t kRoutingOutBase = 20000;
constexpr uint32_t kRoutingInitBase = 21000;
constexpr uint32_t kRoutingSrcOutBase = 22000;   // B50 phase 1c, the dry path

enum RoutingKind
{
  kRoutingCoeff = 0,
  kRoutingOut = 1,
  kRoutingInit = 2,
  // APPENDED, never inserted: the kind is written into the debug cell list and
  // read by routing_check, so renumbering the three above would silently
  // re-label every cell the oracle enumerates.
  kRoutingSrcOut = 3
};

/* THE ONLY TRANSLATION between the id's ROW and routing_core.h's matrix INDEX
   (ADR-088 amendment). A second copy of this arithmetic is the whole hazard the
   amendment exists to remove, so every site that holds one coordinate and needs
   the other calls these two — including the debug exports, which take rows
   because the oracle reads them off the id list.
   `routingIndexOfRow` returns -1 for a row that names no cell in THIS build: a
   reserved-but-unfilled source row (>= kRoutingNSrc, < kRoutingMaxSrc) or a row
   past the last slot. -1 is never a valid index, so a caller that forgets to
   check indexes out of bounds loudly rather than landing on cell 0. */
inline int routingRowOfIndex(int mi)
{
  return mi < kRoutingNSrc ? mi : kRoutingMaxSrc + (mi - kRoutingNSrc);
}
inline int routingIndexOfRow(int row)
{
  if (row < 0) return -1;
  if (row < kRoutingNSrc) return row;
  if (row >= kRoutingMaxSrc && row < kRoutingMaxSrc + kRoutingNSlot)
    return kRoutingNSrc + (row - kRoutingMaxSrc);
  return -1;
}

/* Id -> cell, in ROW coordinates. False for any id in the block that names no
   cell THIS build exposes: out of range, a reserved source row nothing fills
   yet, or a crosspoint that is not a live forward edge. Every reader goes
   through here, so "which cells exist" is one function. */
inline bool decodeRoutingId(clap_id id, int &kind, int &from, int &to)
{
  const uint32_t u = (uint32_t)id;
  if (u >= kRoutingOutBase && u < kRoutingOutBase + kRoutingFromStride)
  {
    kind = kRoutingOut;
    from = -1;
    to = (int)(u - kRoutingOutBase);
    return to < kRoutingNSlot;
  }
  if (u >= kRoutingInitBase && u < kRoutingInitBase + kRoutingFromStride)
  {
    kind = kRoutingInit;
    from = -1;
    to = (int)(u - kRoutingInitBase);
    return to < kRoutingNSlot;
  }
  /* The dry path is keyed on the SOURCE, so it fills `from` and leaves `to` at
     -1 — the mirror of outAmount/slotInit, which name a slot and leave `from`
     at -1. Reading either coordinate without checking the kind is therefore an
     out-of-range index, which is why every consumer switches on the kind. */
  if (u >= kRoutingSrcOutBase && u < kRoutingSrcOutBase + kRoutingFromStride)
  {
    kind = kRoutingSrcOut;
    from = (int)(u - kRoutingSrcOutBase);
    to = -1;
    return from < kRoutingNSrc;
  }
  if (u < kRoutingCoeffBase || u >= kRoutingCoeffBase + kRoutingFromStride * kRoutingFromStride)
    return false;
  const uint32_t off = u - kRoutingCoeffBase;
  kind = kRoutingCoeff;
  from = (int)(off / kRoutingFromStride);
  to = (int)(off % kRoutingFromStride);
  const int mi = routingIndexOfRow(from);
  if (mi < 0 || to >= kRoutingNSlot) return false;
  return RoutingMatrixT::edgeLive(mi, to) && RoutingMatrixT::edgeForward(mi, to);
}

// `from` is a ROW (see the layout comment) — the caller converts, not this.
inline clap_id routingCoeffId(int from, int to)
{
  return (clap_id)(kRoutingCoeffBase + (uint32_t)from * kRoutingFromStride + (uint32_t)to);
}

/* The routing parameter table. Built ONCE at load time — not lazily — because
   applyParam/readParam run on the audio thread when the param queue drains, and
   a function-local static would put its one allocating construction there.

   THE DEFAULTS ARE READ OFF A DEFAULT-CONSTRUCTED MATRIX, never retyped. The
   ctor IS setSerialChain (routing_core.h), so "the defaults reproduce today's
   series chain" is true by construction rather than by a table someone kept in
   step — which is what makes B50 (b)'s bit-identity claim structural. */
struct RoutingParamTable
{
  std::vector<std::string> names;   // storage: ParamDef holds const char* into these
  std::vector<std::string> keys;
  std::vector<ParamDef> defs;
};

static RoutingParamTable makeRoutingTable()
{
  RoutingParamTable r;
  const RoutingMatrixT def{};   // ctor == setSerialChain
  std::vector<clap_id> ids;
  /* Over ROWS, not matrix indices, so the ids come out ascending and the
     reserved source rows simply decode false — the table's membership test is
     decodeRoutingId and nothing else. */
  for (int f = 0; f < kRoutingMaxSrc + kRoutingNSlot; f++)
    for (int t = 0; t < kRoutingNSlot; t++)
    {
      int k = 0, ff = 0, tt = 0;
      const clap_id id = routingCoeffId(f, t);
      if (decodeRoutingId(id, k, ff, tt)) ids.push_back(id);
    }
  for (int t = 0; t < kRoutingNSlot; t++) ids.push_back((clap_id)(kRoutingOutBase + t));
  for (int t = 0; t < kRoutingNSlot; t++) ids.push_back((clap_id)(kRoutingInitBase + t));
  /* LAST, and that position is the contract: this order is the order the cells
     enter `morphIds`, so the dry path appends to the field AFTER the whole
     phase-1 routing block instead of displacing any of it (ADR-159). */
  for (int s = 0; s < kRoutingNSrc; s++) ids.push_back((clap_id)(kRoutingSrcOutBase + s));

  // Reserve before filling: ParamDef keeps raw pointers into these vectors, so
  // a reallocation mid-build would leave earlier rows pointing at freed storage.
  r.names.reserve(ids.size());
  r.keys.reserve(ids.size());
  r.defs.reserve(ids.size());
  char nb[64], kb[32];
  for (clap_id id : ids)
  {
    int kind = 0, from = 0, to = 0;
    decodeRoutingId(id, kind, from, to);
    double lo = 0, hi = 0, dv = 0;
    if (kind == kRoutingCoeff)
    {
      /* Row coordinates, so the SLOT number is `row - kRoutingMaxSrc` — which
         is what keeps `rt.c.m0.1` naming slot 1 -> slot 2 across the
         renumbering. The coreKeys of every pre-existing cell are unchanged;
         only their numeric ids moved. */
      const bool src = from < kRoutingMaxSrc;
      std::snprintf(nb, sizeof(nb), "Route %s%d > Slot%d", src ? "Src" : "Slot",
                    src ? from + 1 : from - kRoutingMaxSrc + 1, to + 1);
      std::snprintf(kb, sizeof(kb), "rt.c.%s%d.%d", src ? "s" : "m",
                    src ? from : from - kRoutingMaxSrc, to);
      // Bipolar and past unity: a crosspoint is a gain, so inversion and a
      // little make-up are both topology moves, and +-1 must sit INSIDE the
      // range rather than on its rail.
      lo = -2.0; hi = 2.0; dv = def.coeff[routingIndexOfRow(from)][to];
    }
    else if (kind == kRoutingOut)
    {
      std::snprintf(nb, sizeof(nb), "Out Slot%d", to + 1);
      std::snprintf(kb, sizeof(kb), "rt.out.%d", to);
      lo = 0.0; hi = 2.0; dv = def.outAmount[to];
    }
    else if (kind == kRoutingSrcOut)
    {
      std::snprintf(nb, sizeof(nb), "Out Src%d", from + 1);
      std::snprintf(kb, sizeof(kb), "rt.srcout.%d", from);
      // THE SAME RANGE THE OUT AMOUNTS ALREADY HAVE, deliberately: this cell
      // sits in the well's OUT column beside them and answers the same
      // question ("how much of this reaches the output"), so a second range
      // would make one column mean two things.
      lo = 0.0; hi = 2.0; dv = def.srcOut[from];
    }
    else
    {
      std::snprintf(nb, sizeof(nb), "Init Slot%d", to + 1);
      std::snprintf(kb, sizeof(kb), "rt.in.%d", to);
      lo = -1.0; hi = 1.0; dv = def.slotInit[to];
    }
    r.names.emplace_back(nb);
    r.keys.emplace_back(kb);
    // stepped = false on every row, and that IS the ADR-173 classification:
    // continuous -> morphable, so a corner holds a topology and the field
    // BLENDS the coefficients as values (ADR-125: argmax is for structure).
    r.defs.push_back(ParamDef{id, r.keys.back().c_str(), r.names.back().c_str(), lo, hi, dv,
                              false, nullptr});
  }
  return r;
}

static const RoutingParamTable g_routingTable = makeRoutingTable();

inline const ParamDef *findRoutingParam(clap_id id)
{
  for (const auto &d : g_routingTable.defs)
    if (d.id == id) return &d;
  return nullptr;
}
inline uint32_t routingParamCount() { return (uint32_t)g_routingTable.defs.size(); }

/* ---- ENGINE PARAMETER BLOCKS (B172) — THE SHARED MECHANISM ----------------
   ADR-088 reserved 3000..9999 for engines that are not the swarm. Each such
   engine takes a CONTIGUOUS block of a thousand ids, and every one of them is
   unreachable without the dispatch below: `findParam` derives the oscillator as
   `id / kOscStride`, so 4000 resolves to oscillator 4, fails `osc >= kNumOsc`
   and returns nullptr — silently, with every gate green (the same trap the
   routing block's comment records).

   THIS IS THE MECHANISM B162 REUSES. STATION's 3000-block lands as ONE MORE ROW
   in kEngineBlocks and touches nothing else: findParam, paramClassOf,
   applyParam, readParam, params_count and params_get_info all walk the table.
   Adding an engine must never mean editing the dispatch again — if it does,
   this abstraction failed and should be deleted rather than extended.

   THE BLOCK'S GATE. Every block names one stepped id as its `gateId`, default
   OFF. While that id reads 0 the engine renders nothing, allocates nothing and
   contributes nothing, so every patch written before the block existed is
   BIT-IDENTICAL with the block present — which is what makes the append safe
   and what subosc_check's control row proves rather than assumes. */
struct EngineBlock
{
  uint32_t base;             // first id; ids are base + i, positional, never curated
  uint32_t count;            // how many ids the block occupies
  const ParamDef *defs;      // `count` rows, ascending, defs[i].id == base + i
  clap_id gateId;            // the block's on/off; DEVICE class, default 0 (off)
  const char *module;        // CLAP module string for params_get_info
  /* THE STATE-KEY PREFIX, and it is not decoration. Both state paths are
     keyed on `coreKey`, and an engine's core keys are its own namespace —
     "wave", "level", "tone", "phase", "seed" all already exist in kParams. An
     unprefixed engine key would be found by the instrument's scan (or the
     reverse) and the wrong parameter would be restored, silently. The `o<k>.`
     twin convention is the same idea one namespace over. */
  const char *keyPrefix;
};

/* THE SUB OSC BLOCK (B172 / ADR-178). Ids 4000..4015: the fifteen core rows in
   `SubOscCore::Param` order — so id - 4000 IS the enum index, which is the whole
   reason the mapping needs no table — then the block's gate at 4015. One of the
   fifteen (4011) is RETIRED IN PLACE rather than removed; that is what keeps the
   identity true and every later id where the host already found it.

   RANGE, STEP AND DEFAULT ARE THE CORE TABLE'S. `SubOscCore::kParamTable`
   (subosc_core.h, SPEC-SUBOSC §7) is the only writer of those five numbers, and
   `kSubOscAgrees` below is a COMPILE-TIME proof that these rows still say what
   it says — count, min, max, stepped (== step != 0) and default, every row. The
   ParamDef shape cannot be generated from the table directly (it carries a
   host-facing name and an optional label array the core has no notion of, and
   the GUI generators parse these literals out of this file), so the rows are
   written out and the static_assert is what makes the second copy safe: a
   divergence is a build failure, not a silent lie. `stepped` is ADR-173's
   derivation evaluated at the one place that knows the step. */
static const char *const kSubWaveLabels[] = {"sine",  "triangle", "square", "saw",
                                             "pulse", "noise",    "bump"};
// B181 note 2: which held key the sub's own mono follows. "lowest" first, so
// it is the default by the table's own order.
static const char *const kSubBiasLabels[] = {"lowest", "highest", "last"};
constexpr uint32_t kSubOscIdBase = 4000;
constexpr clap_id kSubOscOnId = 4015;
static constexpr ParamDef kSubOscParams[] = {
    {4000, "wave", "SUB Wave", 0, 6, 3, true, kSubWaveLabels},
    {4001, "width", "SUB Width", 0.05, 0.95, 0.5, false, nullptr},
    {4002, "bumpAmt", "SUB Bump Amount", 0, 0.6, 0.35, false, nullptr},
    {4003, "bumpPhase", "SUB Bump Phase", -3.141592653589793, 3.141592653589793, -0.25, false,
     nullptr},
    {4004, "octave", "SUB Octave", -3, 0, -1, true, nullptr},   // B181 note 1: floor -2 -> -3
    {4005, "semis", "SUB Semitones", -12, 12, 0, true, nullptr},
    {4006, "fine", "SUB Fine", -100, 100, 0, false, nullptr},
    {4007, "level", "SUB Level", 0, 1, 0.8, false, nullptr},
    {4008, "phase", "SUB Start Phase", 0, 1, 0, false, nullptr},
    {4009, "keytrack", "SUB Keytrack", 0, 1, 1, true, kOffOn},
    {4010, "tone", "SUB Tone", 30, 20000, 20000, false, nullptr},
    /* RETIRED 2026-09-20 (B184) — WAS "SUB Hard Sync". The human struck the
       feature ("I can't imagine a scenario in which it would be useful"); it
       was never audible, because the shell never had a master phase to hand
       the core. THE ID IS RESERVED, NOT RECLAIMED: this block's map is
       positional, so deleting the row would slide 4012..4019 down and move the
       host automation lanes of parameters that DO sound. The same idiom as the
       retired pad aliases (174..177) and mod-matrix slots 10..13. It still
       reads, writes and round-trips through state so an old patch loads
       unchanged — the value simply reaches nothing. Do not reuse this id;
       sync for the SWARM oscillators is B185 and gets its own. */
    {4011, "sync", "SUB Sync (retired)", 0, 1, 0, true, kOffOn},
    {4012, "attack", "SUB Attack", 0.0005, 0.5, 0.005, false, nullptr},
    {4013, "release", "SUB Release", 0.002, 2, 0.08, false, nullptr},
    {4014, "seed", "SUB Seed", 0, 4294967295.0, 1, true, nullptr},
    /* THE GATE, and it is NOT one of the core's parameters — the core has no
       notion of being switched off, and giving it one would be a divergence
       from the parity reference. It is a shell row: default OFF, so the sub
       costs nothing and changes nothing until a player asks for it.
       "on", not "subOn": id 52 already carries that core key (the SPECTRA
       sub-oscillator, ADR-042) and two rows with one key is how a lookup
       keyed on the key finds the wrong one. The block's address prefix
       (`sub.`) is what disambiguates; see src/param_presentation.tsv. */
    {4015, "on", "SUB On", 0, 1, 0, true, kOffOn},
    /* ---- SHELL ROWS, ABOVE THE GATE (B181 notes 2 and 4) -------------------
       Everything from here up is the SHELL's, not the core's: voice assignment
       and glide are the shell's job for the swarm too (`voiceMono`,
       `voiceLegato`, the `glide` lane), and SubOscCore stays what its header
       says it is — a single-voice renderer that holds one note.

       They sit ABOVE the gate rather than interleaved because the block's id
       map is POSITIONAL below it: `id - 4000` IS SubOscCore::Param for every id
       under 4015, and subOscRowsAgreeWithCore proves it at compile time. A
       shell row inserted among them would break that identity silently.

       EVERY DEFAULT IS THE INERT ONE. mono off, glide 0 s, pitch mod 0 st —
       so a patch that says nothing about them renders exactly as it did before
       they existed, which subosc_check's 11g control measures rather than
       assumes. */
    {4016, "mono", "SUB Mono", 0, 1, 0, true, kOffOn},
    /* WHICH held key the mono sub follows. "lowest" is the human's own word
       and is therefore the default; the other two cost one line each at the
       pick site, so refusing them would have been a choice rather than a
       saving. */
    {4017, "bias", "SUB Mono Bias", 0, 2, 0, true, kSubBiasLabels},
    /* SECONDS, and converted per sample rate at the one place that knows it
       (renderSubSpan) — ADR-009: there is not a per-tick constant here. Range
       matched to the instrument's own `glide` lane (id 33) so the two knobs
       mean the same thing. 0 = off, which is a SNAP, not a very fast ramp. */
    {4018, "glide", "SUB Glide (s)", 0, 2.0, 0, false, nullptr},
    /* THE PITCH-ENVELOPE DESTINATION (note 4). `fine` already routes — it is
       continuous, it is in the block, and nothing refuses it — so the gap the
       human hit is RANGE: +/-100 cents is one semitone, which is not a pitch
       envelope. This is a DEDICATED offset rather than a widening of `fine`,
       so `fine` keeps its meaning and every stored value of it keeps its
       pitch. +/-48 st is the instrument's own pitch route's range (ADR-135's
       clamp in modStep), so the two pitch surfaces agree on what "full" is. */
    {4019, "pitchMod", "SUB Pitch Mod (st)", -48, 48, 0, false, nullptr},
};
constexpr uint32_t kSubOscParamCount = (uint32_t)(sizeof(kSubOscParams) / sizeof(kSubOscParams[0]));
// The first id above the gate; everything from here up is a shell row and has
// no SubOscCore::Param behind it. Named once, because three sites ask.
constexpr clap_id kSubShellIdBase = 4016;

/* THE PROOF, not the promise. Rows 0..kParamCount-1 against
   SubOscCore::kParamTable, the gate at kParamCount, and shell rows above it.
   Generalised for B181: the claim was "the block is the core table plus a
   gate"; it is now "the block OPENS with the core table, then the gate, then
   shell rows" — the same positional identity over the same span, with room
   above it. Weakening it would have meant dropping the per-row comparison;
   nothing there changed. */
constexpr bool subOscRowsAgreeWithCore()
{
  using Core = hypersaw::SubOscCore;
  if (kSubOscParamCount < (uint32_t)Core::kParamCount + 1) return false;
  for (uint32_t i = 0; i < kSubOscParamCount; i++)
    if (kSubOscParams[i].id != (clap_id)(kSubOscIdBase + i)) return false;
  for (int i = 0; i < Core::kParamCount; i++)
  {
    const ParamDef &d = kSubOscParams[i];
    const Core::ParamSpec &s = Core::kParamTable[i];
    if (d.minV != s.min || d.maxV != s.max || d.defV != s.def) return false;
    if (d.stepped != (s.step != 0)) return false;      // ADR-173's derivation
    const char *a = d.coreKey, *b = s.key;             // same address, same row
    while (*a && *a == *b) { a++; b++; }
    if (*a != 0 || *b != 0) return false;
  }
  return kSubOscParams[Core::kParamCount].id == kSubOscOnId &&
         kSubOscIdBase + (uint32_t)Core::kParamCount + 1 == (uint32_t)kSubShellIdBase;
}
static_assert(subOscRowsAgreeWithCore(),
              "kSubOscParams disagrees with SubOscCore::kParamTable — the core table is "
              "SPEC-SUBOSC §7's only writer of range/step/default");

constexpr uint32_t kEngineIdLo = 3000;    // ADR-088's reserved engine span, inclusive
constexpr uint32_t kEngineIdHi = 10000;   // exclusive; kRoutingIdBase takes over here
static_assert(kEngineIdHi == kRoutingIdBase, "the engine span must abut the routing block");
constexpr EngineBlock kEngineBlocks[] = {
    {kSubOscIdBase, kSubOscParamCount, kSubOscParams, kSubOscOnId, "SUB OSC", "sub."},
    /* STATION's 3000-block lands HERE (B162) and nowhere else. */
};
static_assert(kSubOscIdBase >= kEngineIdLo && kSubOscIdBase + kSubOscParamCount < kEngineIdHi,
              "the SUB OSC block must sit inside ADR-088's reserved engine span");

// The block an id belongs to, or nullptr. One membership test, every caller.
inline const EngineBlock *engineBlockOf(clap_id id)
{
  const uint32_t u = (uint32_t)id;
  if (u < kEngineIdLo || u >= kEngineIdHi) return nullptr;
  for (const auto &b : kEngineBlocks)
    if (u >= b.base && u < b.base + b.count) return &b;
  return nullptr;   // a reserved-but-unclaimed thousand: no parameter, loudly
}
inline const ParamDef *findEngineParam(clap_id id)
{
  const EngineBlock *b = engineBlockOf(id);
  return b ? &b->defs[(uint32_t)id - b->base] : nullptr;
}
/* `<prefix><coreKey>` -> id, or nullptr. THE ONLY decoder of an engine state
   key, called by both state paths and by the preset JSON path, so "which key
   names which id" is one function rather than four copies. */
inline const ParamDef *findEngineParamByKey(const std::string &key)
{
  for (const auto &b : kEngineBlocks)
  {
    const size_t n = std::strlen(b.keyPrefix);
    if (key.size() <= n || key.compare(0, n, b.keyPrefix) != 0) continue;
    for (uint32_t i = 0; i < b.count; i++)
      if (key.compare(n, std::string::npos, b.defs[i].coreKey) == 0) return &b.defs[i];
  }
  return nullptr;
}
inline uint32_t engineParamCount()
{
  uint32_t n = 0;
  for (const auto &b : kEngineBlocks) n += b.count;
  return n;
}
// Index -> row, in kEngineBlocks order. The enumeration order params_get_info
// hands a host, and the order paramsJson/defaultsJson emit.
inline const ParamDef *engineParamAt(uint32_t index, const char **moduleOut)
{
  for (const auto &b : kEngineBlocks)
  {
    if (index < b.count)
    {
      if (moduleOut) *moduleOut = b.module;
      return &b.defs[index];
    }
    index -= b.count;
  }
  return nullptr;
}

const ParamDef *findParam(clap_id id)
{
  /* ROUTING BLOCK FIRST, and this ordering is load-bearing. The line below
     derives the oscillator as `id / kOscStride`, so 10000 resolves to
     oscillator 10, fails `osc >= kNumOsc` and returns nullptr — every routing
     id would simply not exist, silently, with every gate green. routing_check's
     dispatch probe asserts this branch rather than trusting the reading. */
  if ((uint32_t)id >= kRoutingIdBase) return findRoutingParam(id);
  /* ENGINE BLOCKS SECOND, and for the SAME reason: `oscOfId(4000)` is 4, which
     fails `osc >= kNumOsc` below, so every SUB OSC id would simply not exist —
     silently. subosc_check's dispatch row asserts this branch. */
  if ((uint32_t)id >= kEngineIdLo) return findEngineParam(id);
  const uint32_t osc = oscOfId(id);
  if (osc == 0)
  {
    for (const auto &d : kParams)
      if (d.id == id) return &d;
    return nullptr;
  }
  if (osc >= kNumOsc) return nullptr;          // block exists only up to kNumOsc
  const clap_id base = baseIdOf(id);
  if (isGlobalId(base)) return nullptr;        // globals have no per-osc mirror
  for (const auto &d : kParams)
    if (d.id == base) return &d;
  return nullptr;
}

/* ---- PARAMETER CLASS (QM-4 §3.2 / ADR-152; B89 phase 1) --------------------
   Every parameter is morphable, structural, or device:

     morphable  lives in a corner, blends/flips per-parameter
     structural lives at device level with a per-corner REQUEST; resolves
                atomically, never blended (§8)
     device     not part of the morph at all — sources, not destinations

   THE CLASS IS DEFINITION, NOT STATE. It is never persisted, never reaches the
   host (no CLAP flag; phase 2 decides what a host sees), and never varies per
   instance. That is the whole reason it is DERIVED here rather than stored as
   a ninth column on 243 frozen rows:
     - a column means editing all 243 rows, and every one of those rows carries
       a frozen id, range and default — a diff that touches them all to add a
       field that no row's behaviour depends on is risk with no payment;
     - a column states the rule 243 times and therefore has 243 chances to
       disagree with it, with nowhere the rule itself is written down. §9's
       instruction is to classify by rule, so the rule is the artifact;
     - a derived class means the 244th parameter is classified the day it is
       appended, instead of silently inheriting whatever enum value is 0.
   The cost accepted in exchange: the exceptions are a lookup table, so an
   exception is invisible at the row it applies to. kParamClassOverrides is
   therefore ordered by id and every entry carries its reason.

   THE RULE, WRITTEN ONCE:
     1. an id in kParamClassOverrides takes the class stated there;
     2. otherwise STEPPED -> structural. A stepped value cannot be blended, so
        the morph already ARGMAX-jumps it (ADR-150's note on octave/semi) —
        "resolves atomically" is a description of shipped behaviour, not a new
        rule;
     3. otherwise CONTINUOUS -> morphable.
   §8's carve-out ("including continuous-valued ones that select structure") is
   exactly what the override table's structural entries are for: rule 2 already
   catches every STEPPED structure selector (Voices, Engine, FX type, Osc On,
   Mute/Solo, D*Sync), so only a continuous one needs naming.

   KEYED ON THE BASE ID. An oscillator and its +1000 twin share one ParamDef,
   so they share a class by construction — a class that differed between twins
   would be a property of the instance, not of the parameter.

   DELIBERATELY DOES NOT CONSULT morphIds. The morph field is built separately
   (morphInit), so `no morphIds member is device` is a real cross-check between
   two independently authored lists — paramclass_check asserts it. Derive the
   class FROM the field and that assertion certifies nothing (L0032). */
/* The numbers ARE the exported contract (hypersaw_debug_paramclass returns
   them); paramclass_check anchors one id per class so a reorder cannot pass
   silently. Display strings live at the reader, not here — the definition owes
   a class, not a caption. */
enum class ParamClass
{
  Morphable = 0,
  Structural = 1,
  Device = 2
};

struct ParamClassRule
{
  clap_id id;
  ParamClass cls;
  const char *reason;
};

/* THE EXCEPTIONS, and only the exceptions. Ordered by id. */
static const ParamClassRule kParamClassOverrides[] = {
    // (dev) vestigial: ADR-102 took it out of the DSP; it exists so stored
    // state still loads. Nothing reads it, so it is in no corner's gift.
    {89, ParamClass::Device, "(dev) not read by the DSP (ADR-102); state compat only"},
    // §3.2 names master volume as the device-class example.
    {100, ParamClass::Device, "master volume — §3.2's own device example"},
    // 151-158: the morph controls. The field must not morph its own position.
    {151, ParamClass::Device, "morph control — the field cannot morph itself"},
    {152, ParamClass::Device, "morph position — §3.2: a source, not a destination"},
    {153, ParamClass::Device, "morph position — §3.2: a source, not a destination"},
    {154, ParamClass::Device, "morph control — shapes the resolver itself"},
    {155, ParamClass::Device, "morph control — shapes the resolver itself"},
    {156, ParamClass::Device, "morph control — the field's flip topology"},
    {157, ParamClass::Device, "morph control — how the field resolves"},
    {158, ParamClass::Device, "morph control — the field's own rate"},
    // ADR-109: an edit-routing mode that morphed would change where your edits
    // land as you move the pad. Its row says so already.
    {159, ParamClass::Device, "drives the morph — corner-edit arming (ADR-109)"},
    // Continuous, but it is the voice-retirement threshold: §8's structure
    // selector (voice count), and its row already declares it non-morphable.
    {160, ParamClass::Structural, "selects VOICE COUNT (continuous §8 selector)"},
    // A mod ROUTE's depth. §6 tiers routings; a route is not a corner value.
    {161, ParamClass::Device, "mod-matrix route depth — tiered by §6, not a corner value"},
    // ENV 2 is a global modulation SOURCE; §3.2 puts global mod sources here.
    {162, ParamClass::Device, "ENV 2 is a global mod source (§3.2)"},
    {163, ParamClass::Device, "ENV 2 is a global mod source (§3.2)"},
    {164, ParamClass::Device, "ENV 2 is a global mod source (§3.2)"},
    {165, ParamClass::Device, "ENV 2 is a global mod source (§3.2)"},
    // The macros ARE the intents. §3.2: intent values are device; ADR-137 had
    // already ruled all twelve out of the morph field for the same reason.
    {166, ParamClass::Device, "macro = intent value (§3.2; ADR-137)"},
    {167, ParamClass::Device, "macro = intent value (§3.2; ADR-137)"},
    {168, ParamClass::Device, "macro = intent value (§3.2; ADR-137)"},
    {169, ParamClass::Device, "macro = intent value (§3.2; ADR-137)"},
    {170, ParamClass::Device, "macro = intent value (§3.2; ADR-137)"},
    {171, ParamClass::Device, "macro = intent value (§3.2; ADR-137)"},
    {172, ParamClass::Device, "macro = intent value (§3.2; ADR-137)"},
    {173, ParamClass::Device, "macro = intent value (§3.2; ADR-137)"},
    // Which macro a pad axis writes — intent plumbing, not a timbre.
    {174, ParamClass::Device, "intent assignment — which macro an axis writes (ADR-137)"},
    {175, ParamClass::Device, "intent assignment — which macro an axis writes (ADR-137)"},
    {176, ParamClass::Device, "intent assignment — which macro an axis writes (ADR-137)"},
    {177, ParamClass::Device, "intent assignment — which macro an axis writes (ADR-137)"},
    // GUI renderer on/off (ADR-140). No audio path at all.
    {178, ParamClass::Device, "GUI renderer toggle (ADR-140) — outside the audio path"},
    {179, ParamClass::Device, "intent assignment — MAIN pad's axes (ADR-150)"},
    {180, ParamClass::Device, "intent assignment — MAIN pad's axes (ADR-150)"},
    // (dev) B117/ADR-163: buried, ids kept so saved state loads.
    {264, ParamClass::Device, "(dev) buried rack policy (B117/ADR-163)"},
    {265, ParamClass::Device, "(dev) buried rack policy (B117/ADR-163)"},
    /* ADR-176 decision 6. Rule 2 would make it STRUCTURAL (it is stepped), and
       the ruling overrides that to DEVICE for the reason 151-158 are device: it
       shapes the RESOLVER, and a field that morphed its own resolver would
       change how it resolves as the pad moved. Device also keeps it out of
       morphIds, which is what paramclass_check's "no morphIds member is
       device" cross-check asserts — morphInit never appends it. */
    {266, ParamClass::Device, "(dev) intent-bus flag — shapes the resolver itself (ADR-176)"},
    {268, ParamClass::Device, "pad latch — the performance surface's spring, not a timbre (ADR-176 §5)"},
    /* B146. Rule 2 would make it STRUCTURAL (it is stepped, and it does change
       the graph), and the ruling overrides that to DEVICE: the placement is an
       output-stage policy of the instance, like master volume (id 100), not
       something a corner authors. Device means it is not in the morph field at
       all — so it never flips, argmax or otherwise, and paramclass_check's
       "no morphIds member is device" cross-check holds because morphInit never
       appends it. */
    {267, ParamClass::Device, "bass-mono placement — an output-stage policy (B146)"},
    /* B171, the ENV 2 rows above applied to twenty more: §3.2 puts a GLOBAL MOD
       SOURCE in the device class, and that is what all twenty are — the shape
       and timing of a modulator, not a value a corner authors. Device also
       keeps them out of morphIds, which is what paramclass_check's "no morphIds
       member is device" cross-check needs, and out of buildMorphOrder's frozen
       prefix (it skips globals). Rule 2 would call the stepped ones structural;
       the same override the delay's sync rows earn. */
    {269, ParamClass::Device, "LFO 1 is a global mod source (§3.2)"},
    {270, ParamClass::Device, "LFO 1 is a global mod source (§3.2)"},
    {271, ParamClass::Device, "LFO 1 is a global mod source (§3.2)"},
    {272, ParamClass::Device, "LFO 1 is a global mod source (§3.2)"},
    {273, ParamClass::Device, "LFO 1 is a global mod source (§3.2)"},
    {274, ParamClass::Device, "LFO 1 is a global mod source (§3.2)"},
    {275, ParamClass::Device, "LFO 2 is a global mod source (§3.2)"},
    {276, ParamClass::Device, "LFO 2 is a global mod source (§3.2)"},
    {277, ParamClass::Device, "LFO 2 is a global mod source (§3.2)"},
    {278, ParamClass::Device, "LFO 2 is a global mod source (§3.2)"},
    {279, ParamClass::Device, "LFO 2 is a global mod source (§3.2)"},
    {280, ParamClass::Device, "LFO 2 is a global mod source (§3.2)"},
    {281, ParamClass::Device, "ENV 3 is a global mod source (§3.2)"},
    {282, ParamClass::Device, "ENV 3 is a global mod source (§3.2)"},
    {283, ParamClass::Device, "ENV 3 is a global mod source (§3.2)"},
    {284, ParamClass::Device, "ENV 3 is a global mod source (§3.2)"},
    {285, ParamClass::Device, "ENV 4 is a global mod source (§3.2)"},
    {286, ParamClass::Device, "ENV 4 is a global mod source (§3.2)"},
    {287, ParamClass::Device, "ENV 4 is a global mod source (§3.2)"},
    {288, ParamClass::Device, "ENV 4 is a global mod source (§3.2)"},
};

/* The class of `id` and the one-line reason it has that class. False for an id
   that is not a parameter (including a global's non-existent +1000 twin). */
inline bool paramClassOf(clap_id id, ParamClass &cls, const char *&reason)
{
  const ParamDef *d = findParam(id);
  if (!d) return false;
  /* ROUTING BLOCK BEFORE `baseIdOf`, the SECOND place the id/kOscStride
     derivation would silently mis-resolve a routing id — and here it would not
     merely fail, it would ALIAS: 10064 (Slot1 > Slot1) reduces to base 64,
     which is a real instrument id and could carry an override. Classed by the
     rule rather than by the table: every cell is continuous, so ADR-173 rule 3
     makes it morphable, and that is what lets a corner hold a topology while
     the field blends the coefficients as VALUES (ADR-125). */
  if ((uint32_t)id >= kRoutingIdBase)
  {
    cls = ParamClass::Morphable;
    reason = "ADR-088 crosspoint: a continuous gain, blended never argmax'd";
    return true;
  }
  /* ENGINE BLOCKS, classed BY THE RULE and before `baseIdOf` — which would
     otherwise ALIAS: 4004 (SUB Octave) reduces to base 4, a real instrument id
     that could carry an override. The rule is ADR-173's, evaluated off the
     block's own `stepped` (which subOscRowsAgreeWithCore pins to the core
     table's step), with exactly ONE exception: the block's gate. */
  if (const EngineBlock *eb = engineBlockOf(id))
  {
    if (id == eb->gateId)
    {
      /* STRUCTURAL — rule 2, with NO override. This row read Device from B172
         until B203, on the argument that "switching an engine on is
         instance-level policy, like master volume", and the human overruled it
         2026-09-21: "Sub on/off is still exempt from morph and it ought to be
         wired in the way the other two oscs are." They are right, and the
         evidence is one id away: the OSCILLATORS' enable (150) is stepped,
         carries no override, is therefore Structural, and has been in the
         morph field since the field existed — with B48's ramp (see
         morphApplyOscEnable) as its special case. The old ruling made the
         instrument's two power switches answer to two different rules for no
         reason a player could hear.
         The gate is STILL not interpolated: like 150 it is applied as a LEVEL
         RAMP off the bilinear corner weight, and the stepped flip is deferred
         to the weight floor where the engine is already ~-60 dB. Membership is
         what changed here; how it resolves is morphApplyGateEnable's business.
         `subOn off is bit-inert` survives because every corner of a fresh
         instance holds the default (0), so the ramp weight is 0 everywhere
         until a corner authors otherwise. */
      cls = ParamClass::Structural;
      reason = "engine-block gate — a level ramp in the field, like osc enable 150 (B203)";
      return true;
    }
    cls = d->stepped ? ParamClass::Structural : ParamClass::Morphable;
    reason = d->stepped ? "engine block, stepped: cannot blend, resolves atomically"
                        : "engine block, continuous DSP value: blends inside its corner";
    return true;
  }
  const clap_id base = baseIdOf(id);
  for (const auto &r : kParamClassOverrides)
    if (r.id == base)
    {
      cls = r.cls;
      reason = r.reason;
      return true;
    }
  cls = d->stepped ? ParamClass::Structural : ParamClass::Morphable;
  reason = d->stepped ? "stepped: cannot blend, resolves atomically"
                      : "continuous DSP value: blends inside its corner";
  return true;
}

// Grid cycles/beat quantizes to musical (rational) divisions — the param
// stores the actual cycles-per-beat value (state stays forward-compatible),
// but applyParam snaps and value_to_text names the fraction.
static const double kGridSteps[] = {0.25, 1.0 / 3, 0.5, 2.0 / 3, 0.75, 1, 1.5, 2, 3, 4, 6, 8};
static const char *const kGridStepNames[] = {"1/4", "1/3", "1/2", "2/3", "3/4", "1",
                                             "3/2", "2",   "3",   "4",   "6",   "8"};
constexpr int kNumGridSteps = 12;

double snapGridStep(double v)
{
  double best = kGridSteps[0], bd = 1e9;
  for (double s : kGridSteps)
    if (std::fabs(v - s) < bd)
    {
      bd = std::fabs(v - s);
      best = s;
    }
  return best;
}

const char *gridStepName(double v)
{
  for (int i = 0; i < kNumGridSteps; i++)
    if (std::fabs(v - kGridSteps[i]) < 1e-6) return kGridStepNames[i];
  return nullptr;
}

struct Plugin
{
  clap_plugin_t plugin{};
  const clap_host_t *host = nullptr;
  const clap_host_params_t *hostParams = nullptr;
  // ADR-082 increment 2: N SAW cores. `core` stays a reference to oscillator 0
  // so the 52 existing call sites keep meaning exactly what they meant — this
  // change adds an oscillator, it does not rewrite the first one.
  hypersaw::SwarmCore cores[kMaxOsc] = {hypersaw::SwarmCore{44100.0},
                                        hypersaw::SwarmCore{44100.0}};
  hypersaw::SwarmCore &core = cores[0];

  /* FAN-OUT SEAM (2026-08-09). Every per-voice and lifecycle operation means
     "all oscillators", never "oscillator 0" — route them through these and
     never through `core`.

     The `core` alias exists so the multi-oscillator port (ADR-082) did not have
     to touch every legacy call site. That convenience is exactly what hid this:
     eight sites read as correct C++ and were correct with one oscillator, and
     with two they addressed half the instrument. PRESSURE fanned out while
     TUNING did not, so a bend split the pair mid-gesture; every allOff() —
     mono/poly toggle, engine switch, MIDI all-notes-off, reset, GUI panic —
     silenced oscillator 0 and left the rest ringing, which is a stuck note.

     This is L0028's shape: an operation whose intent is a ROLE ("every
     oscillator") written against an INSTANCE. Covered by tools/mpe_check.cpp;
     the alias itself is the root cause and its removal is queued behind a
     human gate, since `core` still has legitimately-oscillator-0 readers. */
  void allOffAll()
  {
    for (uint32_t k = 0; k < kNumOsc; k++) cores[k].allOff();
    subAllOff();
  }
  void noteOffAll(int key)
  {
    for (uint32_t k = 0; k < kNumOsc; k++) cores[k].noteOff(key);
    subNoteOff(key);
  }

  /* ---- SUB OSC, ONE CORE PER VOICE SLOT (B172 / ADR-178) -------------------
     subosc_core.h states the assumption in its header: SubOscCore holds exactly
     ONE note's state, so polyphony is N instances, not one instance played N
     times. The bank is indexed by OSCILLATOR 0's slot — the logical voice —
     which is the same index `tags`, `penv` and `slotOf` already use, so the sub
     needs no allocator of its own and cannot disagree with the swarm's about
     which voice a note is.

     THE KEY TABLE IS WHY. The swarm releases BY KEY (`SwarmCore::noteOff(key)`)
     and SubOscCore releases the one note it holds, so the shell has to remember
     which key each slot's sub is sounding. Set at every strike, cleared at
     every release and at allOff — a stale entry would release the wrong slot,
     which is the stuck-note shape the mono held-stack comment records.

     EVERY LIFECYCLE CALL FANS OUT THROUGH allOffAll / noteOffAll, which are the
     seam that comment names — a second hand-wired path for the same signal
     class is L0029's named failure, so there is exactly one. */
  hypersaw::SubOscCore subs[hypersaw::kPoly] = {
      hypersaw::SubOscCore{44100.0}, hypersaw::SubOscCore{44100.0}, hypersaw::SubOscCore{44100.0},
      hypersaw::SubOscCore{44100.0}, hypersaw::SubOscCore{44100.0}, hypersaw::SubOscCore{44100.0},
      hypersaw::SubOscCore{44100.0}, hypersaw::SubOscCore{44100.0}, hypersaw::SubOscCore{44100.0},
      hypersaw::SubOscCore{44100.0}, hypersaw::SubOscCore{44100.0}, hypersaw::SubOscCore{44100.0},
      hypersaw::SubOscCore{44100.0}, hypersaw::SubOscCore{44100.0}, hypersaw::SubOscCore{44100.0},
      hypersaw::SubOscCore{44100.0}};
  int subKey[hypersaw::kPoly] = {-1, -1, -1, -1, -1, -1, -1, -1,
                                 -1, -1, -1, -1, -1, -1, -1, -1};
  double subOn = 0;   // the block's gate (id 4015), STRUCTURAL since B203, ships off
  /* B203: the gate's morph-derived on-weight and the gain it is smoothed
     into, the sub's copy of B48's `oscOnW` / `oscGainSm` pair and carried by
     the SAME ~8 ms one-pole (gainSmoothCoef), so the instrument's power
     switches feel alike. 1.0 whenever morph is off, the gate is exempt, or
     every relevant corner agrees — which is what keeps an untouched patch
     bit-identical: multiplying a double by exactly 1.0 is exact. */
  double subOnW = 1.0, subOnGainSm = 1.0;

  /* ---- THE SUB'S OWN MONO, BIAS AND GLIDE (B181 note 2) -------------------
     The human: "Sub should have its own mono toggle, with 'lowest' as the MIDI
     bias and a simple glide knob."

     SHELL-SIDE, NOT IN THE CORE, and that is the same division the swarm
     already uses: `voiceMono` / `voiceLegato` / the `glide` lane are all the
     shell's, and SwarmCore does not know it is being played monophonically.
     SubOscCore likewise stays what its header declares — one note, one phase,
     one envelope — and the only thing this adds to it is a pitch INPUT.

     THE HELD STACK IS THE SUB'S OWN, fed by subNoteOn / subNoteOff, which
     every lifecycle path already funnels through (noteOffAll, allOffAll). Last
     entry = most recent, so `last` is the back, `lowest`/`highest` are a scan.
     Same 16-entry bound and same DROP-OLDEST rule as the swarm's heldStack
     (ADR-126) — including its stated cost, that an evicted key's later
     note-off matches nothing.

     ONE SOUNDING SLOT, AND IT IS SLOT 0. Mono means one note, so only slot 0
     is ever struck while `subMono` is on; the other fifteen are idle (their
     envelopes are at 0 and their filter states were cleared by subAllOff when
     the toggle moved), so they render exact silence into the sum. */
  double subMono = 0, subBias = 0, subGlide = 0;   // ids 4016 / 4017 / 4018
  double subPitchMod = 0;                          // id 4019 (note 4)
  int subHeld[hypersaw::kPoly];
  int subHeldCount = 0;
  int subStruckKey = -1;    // the key slot 0 was STRUCK at; the glide's origin
  // Semitones relative to subStruckKey. `cur` is what reaches the core, `to`
  // is where it is heading, `rate` is st/s — set at every retarget from the
  // remaining distance and the glide TIME, so a glide always takes `subGlide`
  // seconds whatever the interval.
  double subGlideCur = 0, subGlideTo = 0, subGlideRate = 0;

  int subBiasPick() const
  {
    if (subHeldCount <= 0) return -1;
    if (subBias >= 2) return subHeld[subHeldCount - 1];        // last
    int best = subHeld[0];
    for (int i = 1; i < subHeldCount; i++)
      if (subBias >= 1 ? subHeld[i] > best : subHeld[i] < best) best = subHeld[i];
    return best;                                               // highest / lowest
  }
  void subGlideAim(int key)
  {
    if (subStruckKey < 0) return;
    subGlideTo = (double)(key - subStruckKey);
    if (subGlide <= 0) { subGlideCur = subGlideTo; subGlideRate = 0; return; }
    const double span = subGlideTo - subGlideCur;
    subGlideRate = (span < 0 ? -span : span) / subGlide;   // st per second
  }
  // MONO NOTE-ON: push, then follow the bias. A sub already sounding GLIDES to
  // the new pick and is NOT re-struck — a re-strike would reset the phase and
  // the AR, which is audibly the opposite of a glide.
  void subMonoNoteOn(int key, double vel)
  {
    for (int i = 0; i < subHeldCount; i++)   // a key cannot be held twice
      if (subHeld[i] == key)
      {
        for (int j = i; j < subHeldCount - 1; j++) subHeld[j] = subHeld[j + 1];
        subHeldCount--;
        break;
      }
    if (subHeldCount < hypersaw::kPoly) subHeld[subHeldCount++] = key;
    else
    {
      for (int j = 0; j + 1 < hypersaw::kPoly; j++) subHeld[j] = subHeld[j + 1];
      subHeld[hypersaw::kPoly - 1] = key;
    }
    const int want = subBiasPick();
    if (want < 0) return;
    if (subStruckKey < 0)
    {
      subStruckKey = want;
      subGlideCur = subGlideTo = subGlideRate = 0;
      /* THE OVERLAP CLICK (B202; human 2026-09-21: "when the Sub Osc is set to
         mono, there's a click artifact when notes overlap"). This is mono's
         ONLY re-strike path — `subStruckKey` is cleared when the last key
         lifts, so the next press strikes instead of gliding — and `release`
         ships at 0.08 s, so in ordinary playing the previous note's tail is
         still running when it lands. noteOn REPLACES (subosc_core.h), and the
         phase reset underneath that live envelope was the click: 18.18x the
         signal's own slope, measured by subosc_check 11g.e before this line.
         Keeping the phase makes the strike continuous; `env > 0` is the tail,
         so a note that starts from silence still obeys the `phase` parameter
         exactly as before.

         WHY MONO AND NOT THE POLY PATH BESIDE IT — the structural difference
         the sub has from an oscillator. Poly hands the sub the SWARM
         allocator's slot, and alloc()'s tiers 1 and 2 read `env`, so a fresh
         note lands on an idle or quietest voice and a live tail is the case it
         is built to avoid. Mono has one slot and no such choice; the click is
         the price of that, and this is what pays it.

         WHAT THIS DOES NOT COVER, measured and named rather than implied: if
         the new note's VELOCITY differs from the tail's, `gain = level * vel`
         still steps — 11.91x at 1.0 -> 0.3, which 11g.e REPORTS and does not
         gate. Its size is the product of the INHERITED envelope level and the
         velocity change, and the inheritance is SPEC-SUBOSC §5.3's PROVISIONAL
         per-module AR, which open ruling R4 already expects the voice envelope
         to replace. Fixing it here means either cutting the tail (a bigger
         step) or the shell writing the core's `env`, which that member's own
         comment forbids — so it is left measured, visible and unqueued rather
         than paid for against a stage that is leaving. */
      subs[0].noteOn(want, vel, /*keepPhase=*/subs[0].env > 0);
      subKey[0] = want;
      return;
    }
    subGlideAim(want);
  }
  void subMonoNoteOff(int key)
  {
    int w = 0;
    for (int i = 0; i < subHeldCount; i++)
      if (subHeld[i] != key) subHeld[w++] = subHeld[i];
    subHeldCount = w;
    const int want = subBiasPick();
    if (want < 0)
    {
      subs[0].noteOff();
      subKey[0] = -1;
      subStruckKey = -1;         // the next press strikes rather than glides
      return;
    }
    subGlideAim(want);
  }
  void subNoteOn(int slot, int key, double vel)
  {
    if (subMono != 0) { subMonoNoteOn(key, vel); return; }
    if (slot < 0 || slot >= hypersaw::kPoly) return;
    subs[slot].noteOn(key, vel);
    subKey[slot] = key;
  }
  void subNoteOff(int key)
  {
    if (subMono != 0) { subMonoNoteOff(key); return; }
    for (int s = 0; s < hypersaw::kPoly; s++)
      if (subKey[s] == key) { subs[s].noteOff(); subKey[s] = -1; }
  }
  void subAllOff()
  {
    for (int s = 0; s < hypersaw::kPoly; s++) { subs[s].allOff(); subKey[s] = -1; }
    subHeldCount = 0;
    subStruckKey = -1;
    subGlideCur = subGlideTo = subGlideRate = 0;
    for (auto &c : subs) c.pitchOffsetSt = 0;
  }
  // One write per id, from the block's positional mapping: id - base IS the
  // core's enum index (see kSubOscParams). No switch, so a row added to the
  // core table cannot be forgotten here.
  void subSetParam(clap_id id, double v)
  {
    if (id == kSubOscOnId)
    {
      /* Switching the block OFF must SILENCE it, not freeze it: a frozen core
         resumes mid-note when the gate comes back, and "off" then means "off
         until you turn it on, at which point yesterday's note finishes". The
         same reasoning ADR-099 A1 applies to an oscillator's power switch. */
      if (v == 0 && subOn != 0) subAllOff();
      /* B203: ON RE-STRIKES WHAT IS HELD — ADR-100 Amendment 1's rule for an
         oscillator's enable, applied to the gate that now shares its field.
         Without it a corner that turns the sub ON is silent until the next
         fresh note, which is the "sometimes osc 2 doesn't work" report one
         engine over, and a gate in the morph field that a pad sweep cannot
         make audible is a member in name only. The OFF branch above killed
         whatever state there was, so this is a fresh strike and not a resumed
         envelope — deliberately, as it is there. The strike order is the SLOT
         order, not the press order, so in sub-mono the `last` bias picks the
         highest-numbered held slot rather than the most recent press; the
         oscillator re-strike has the same character and the alternative is a
         second record of press order that nothing else needs. */
      else if (v != 0 && subOn == 0)
      {
        /* THE RAMP RESUMES FROM THE WEIGHT, NOT FROM WHEREVER IT WAS LEFT.
           The row renders nothing while the gate is off, so `subOnGainSm` is
           stale — and its shipped value is 1.0, so a gate that flipped ON at
           the weight FLOOR (1e-3, where morphApplyGateEnable defers the flip
           to) would open at full level and fade DOWN to 0.001: a full-scale
           burst at exactly the pad position the ramp exists to make silent.
           With morph off `subOnW` is 1.0, so this assignment is 1.0 = 1.0 and
           the plain toggle behaves exactly as it always has. */
        subOnGainSm = subOnW;
        for (int i = 0; i < (int)hypersaw::kPoly; i++)
          if (tags[i].active) subNoteOn(i, tags[i].key, tags[i].vel);
      }
      subOn = v;
      return;
    }
    /* THE SHELL ROWS (B181), handled BEFORE the positional map — `id - 4000`
       for 4016 would be 16, one past SubOscCore::Param, and setParam would
       read off the end of the core's table. The order is the safety. */
    if ((uint32_t)id >= (uint32_t)kSubShellIdBase)
    {
      switch ((uint32_t)id)
      {
        case 4016:
          /* Same reasoning as the gate one branch up (ADR-099 A1): crossing
             the mono boundary must SILENCE, not freeze. Fifteen slots holding
             notes when mono comes on would keep sounding for ever (mono only
             ever releases slot 0), and slot 0's glide state would outlive its
             note when mono goes off. */
          if (v != subMono) subAllOff();
          subMono = v;
          return;
        case 4017: subBias = v; return;
        case 4018:
          subGlide = v;
          // Re-aim rather than re-time: a knob moved mid-glide changes how
          // long the REST of the journey takes, which is what a player means.
          if (subStruckKey >= 0) subGlideAim(subStruckKey + (int)subGlideTo);
          return;
        default: subPitchMod = v; return;   // 4019
      }
    }
    const int i = (int)((uint32_t)id - kSubOscIdBase);
    for (auto &c : subs) c.setParam((hypersaw::SubOscCore::Param)i, v);
  }
  double subGetParam(clap_id id) const
  {
    if (id == kSubOscOnId) return subOn;
    if ((uint32_t)id >= (uint32_t)kSubShellIdBase)
      switch ((uint32_t)id)
      {
        case 4016: return subMono;
        case 4017: return subBias;
        case 4018: return subGlide;
        default: return subPitchMod;   // 4019
      }
    return subs[0].param((hypersaw::SubOscCore::Param)((uint32_t)id - kSubOscIdBase));
  }
  /* ONE LOGICAL NOTE, N PHYSICAL VOICES — and the mapping is now CONSTRUCTED,
     not assumed. Every helper below used to apply oscillator 0's slot index to
     every core, on the strength of a comment ("note fan-out keeps slot indices
     aligned"). Nothing enforced it, and it is false: `alloc()`'s tiers 1 and 2
     read `s.env`, and the amp envelope is PER-OSCILLATOR (A12), so the moment
     two cores' envelopes differ their tails fade on different schedules, the
     same note lands on different slots, and a retarget gates the WRONG voice in
     core k while the real one is orphaned — gated, under a key whose note-off
     has already been and gone. It never releases. That is the human's
     intermittent stuck-note report, and FOUNDATIONS' 2026-08-11 brief §2 called
     the mechanism before it was measured.

     `slotOf[s][k]` is core k's slot for the logical voice that oscillator 0
     holds at slot s; `slotOf[s][0] == s` by definition. Recorded at note-on,
     which is the only place a core allocates. */
  int slotOf[hypersaw::kPoly][kNumOsc];
  void bindSlots(int slot0, uint32_t k, int slotK)
  {
    if (slot0 >= 0 && slot0 < (int)hypersaw::kPoly && k < kNumOsc) slotOf[slot0][k] = slotK;
  }
  void retargetAll(int slot, int key, double freq, bool keepPhase)
  {
    if (slot < 0) return;
    for (uint32_t k = 0; k < kNumOsc; k++)
      cores[k].retargetNote(slotOf[slot][k], key, freq, keepPhase);
    /* THE SUB RE-STRIKES ON A MONO RETARGET, and that is a NAMED DIVERGENCE
       from the swarm, not an accident. SubOscCore's only pitch input is
       `noteOn` (subosc_core.h:237) — there is no retune seam, and adding one to
       a parity-gated core is outside B172. So legato mono moves the sub to the
       new key by re-striking it: under `keepPhase` the swarm glides and the sub
       restarts its own AR, which is audible. Recorded rather than hidden
       because SPEC-SUBOSC §5.3/R4 already expects that AR to be STRUCK when the
       voice envelope is wired, and the divergence disappears with it — a
       retune seam added now would be built for an envelope that is leaving.
       The velocity is the SLOT'S, not 1.0: a retarget must not change loudness,
       which is what NoteTag::vel exists for (ADR-100 A1). */
    subNoteOn(slot, key, tags[slot].vel);
  }
  void setNoteExprAll(int slot, double v)
  {
    if (slot < 0) return;
    for (uint32_t k = 0; k < kNumOsc; k++) cores[k].setNoteExpr(slotOf[slot][k], v);
  }
  // Is this logical voice still keyed down? Per OSCILLATOR slot, because
  // slotOf is the only thing that says which physical voice is ours (a core's
  // own index i is NOT the logical slot — see the comment above).
  bool slotGated(int slot)
  {
    if (slot < 0 || slot >= (int)hypersaw::kPoly) return false;
    for (uint32_t k = 0; k < kNumOsc; k++)
      if (oscEnabled[k] && cores[k].voiceAt(slotOf[slot][k]).gate) return true;
    return false;
  }
  /* THE PER-NOTE PITCH COMPOSER (ADR-162). `noteTune` is ONE multiplier per
     voice, and there are now TWO independent per-note pitch offsets: the MPE
     bend lane and ENV 2. Either one calling setNoteExprAll directly would
     erase the other's contribution — the classic last-writer-wins bug, and it
     would present as "MPE bend works until you turn up the pitch envelope".
     So both write a COMPONENT here and only emitNoteExpr() reaches a core;
     `setNoteExprAll` has exactly one caller, which is what makes the rule
     checkable rather than remembered (L0029's one-routing-layer rule). */
  struct NoteExprParts { double bend = 0, penv = 0, emitted = 0; };
  NoteExprParts noteExpr[hypersaw::kPoly];
  void emitNoteExpr(int slot, bool force)
  {
    if (slot < 0 || slot >= (int)hypersaw::kPoly) return;
    NoteExprParts &ne = noteExpr[slot];
    const double v = ne.bend + ne.penv;
    if (!force && v == ne.emitted) return;
    ne.emitted = v;
    setNoteExprAll(slot, v);
  }
  // The bend lane's writes stay UNCONDITIONAL (force) — the historical
  // law-off path wrote every time, and byte-identity is the contract.
  void noteExprSetBend(int slot, double semis)
  {
    if (slot < 0 || slot >= (int)hypersaw::kPoly) return;
    noteExpr[slot].bend = semis;
    emitNoteExpr(slot, true);
  }
  void noteExprSetPenv(int slot, double semis)
  {
    if (slot < 0 || slot >= (int)hypersaw::kPoly) return;
    noteExpr[slot].penv = semis;
    emitNoteExpr(slot, false);
  }
  /* A fresh strike resets noteTune to 1.0 inside the core (ADR-036/038), so
     the composer's cache must return to the same zero — otherwise the cached
     `emitted` says "already written" about a value the core has thrown away,
     and a stale bend from the PREVIOUS note on this slot would be added back
     under the new note's pitch envelope. Called at every note-on, before
     seedNoteBend re-applies the channel's latched bend. */
  void resetNoteExpr(int slot)
  {
    if (slot < 0 || slot >= (int)hypersaw::kPoly) return;
    noteExpr[slot] = NoteExprParts{};
  }
  void setNotePressureAll(int slot, double v)
  {
    if (slot < 0) return;
    for (uint32_t k = 0; k < kNumOsc; k++) cores[k].setNotePressure(slotOf[slot][k], v);
  }
  // constructed state matches the reported default above
  /* RETIRED as a VOL zero (ADR-100 A3): silencing higher oscillators by vol
     dated from before the enable switch existed. With BOTH defaults at zero,
     the power button "worked" and produced nothing -- the volume was still
     down: "the power buttons don't work" (human 2026-08-21). The switch
     (enable=0, oscEnabled ships {1,0}) is now the ONE silent-by-default gate;
     vol keeps its musical default so switching ON is audible immediately. */
  struct SilenceHigherOscillators
  {
    explicit SilenceHigherOscillators(hypersaw::SwarmCore *) {}
  } silenceHigher{cores};
  hypersaw::SpectraCore spectra{44100.0};
  /* FORENSIC NOTE TRACE (FOUNDATIONS brief ask (c), 2026-08-11).
     The stuck-note bug took weeks because it could not be REPRODUCED, and no
     generator was ever going to reproduce it: a fuzzer emits the event stream
     it imagines, and ours deliberately excludes shapes no host can produce
     (notefuzz_check.cpp:14-17). So it can never model a stream the host
     actually delivered. Capture instead of simulate — then a field report
     becomes a replayable regression case instead of an anecdote.

     Written from the AUDIO THREAD: plain stores into a fixed array plus one
     release store. No allocation, no lock, no wall-clock — the charter and
     rtsafety_probe both forbid all three. Read from the GUI thread on panic;
     a torn read of a single record is acceptable here, because this is a
     diagnostic and making it exact would cost the audio thread something real.
     kTraceLen is a power of two so the index is a mask, not a modulo. */
  struct NoteTrace
  {
    uint64_t pos;      // absolute sample position: block steady time + offset
    uint16_t type;     // CLAP event type
    int16_t key;
    int32_t noteId;
    int16_t channel, port;
    float velocity;
  };
  static constexpr uint32_t kTraceLen = 512;   // a few seconds of dense play
  static_assert((kTraceLen & (kTraceLen - 1)) == 0, "kTraceLen must be a power of two");
  NoteTrace trace[kTraceLen] = {};
  std::atomic<uint64_t> traceWrite{0};         // total ever written; & (len-1) indexes
  uint64_t blockPos = 0;                       // steady time of the block in flight
  uint64_t tracePos = 0;                       // local monotonic sample count, never host-supplied

  /* HOST-MPE DETECTION. Live gates MPE behind a PER-DEVICE toggle the plugin
     cannot set and cannot read. With it off, an expressive device's stream
     arrives FLATTENED — every note on channel 0, no note expressions — and the
     result is retriggered blips where the player expects sustain. That cost two
     multi-round investigations here (2026-07-19 bend, 2026-08-12 Expressive
     Chords), and the human found it both times, not the oracle.

     We cannot turn the toggle on. We CAN notice its absence: notes arriving with
     zero note expressions AND never leaving channel 0 is the signature. Plain
     single-channel MIDI looks identical, which is why the hint is phrased as a
     possibility and never as an error — a diagnosis the user can dismiss beats a
     defect they cannot find. Relaxed stores; these are counters, not state. */
  std::atomic<uint32_t> sawNotes{0}, sawExprs{0}, sawNonZeroChan{0};
  std::string lastDumpPath;

  void recordNote(const clap_event_header_t *ev, const clap_event_note_t *n)
  {
    const uint64_t w = traceWrite.load(std::memory_order_relaxed);
    NoteTrace &r = trace[w & (kTraceLen - 1)];
    r.pos = blockPos + ev->time;
    r.type = (uint16_t)ev->type;
    r.key = (int16_t)n->key;
    r.noteId = n->note_id;
    r.channel = (int16_t)n->channel;
    r.port = (int16_t)n->port_index;
    r.velocity = (float)n->velocity;
    traceWrite.store(w + 1, std::memory_order_release);
  }

  /* Write the trace and the live voice tables to a file, and return its path.
     MAIN/GUI THREAD ONLY — this opens a file, which the audio thread may never
     do. It reads state the audio thread is concurrently writing and does not
     lock: a diagnostic that stalls the audio thread to describe it is worse
     than a diagnostic with one torn row.

     The path is derived at RUNTIME, never baked in — a machine-absolute path in
     a tracked file is both an identity leak and wrong on any other machine. */
/* Empty string = nothing to say. Deliberately silent until enough notes have
     arrived to be sure: a hint that fires on the first note would fire on every
     load, and a warning that is usually wrong gets ignored when it is right. */
  std::string hostHint() const
  {
    const uint32_t n = sawNotes.load(std::memory_order_relaxed);
    if (n < 24) return {};
    if (sawExprs.load(std::memory_order_relaxed) > 0) return {};
    if (sawNonZeroChan.load(std::memory_order_relaxed) > 0) return {};
    return "No note expressions received on any channel. If you are playing an "
           "MPE controller or an expressive device, MPE is probably OFF for this "
           "plugin in your host - per-note pitch and pressure will be flattened.";
  }

    std::string dumpForensics(const char *why)
  {
    namespace fs = std::filesystem;
    std::error_code ec;
    /* One store root for the whole plugin (B129): the dump lands beside the
       presets rather than in a second, differently-derived place. The old
       `$HOME/Library/Logs` branch was macOS-only and silently fell through to
       the temp dir everywhere else — which is still the fallback, but now only
       when the platform's home variable is genuinely unset. */
    fs::path dir = hypersaw::presetRoot();
    if (!dir.empty()) dir /= "logs";
    if (dir.empty()) dir = fs::temp_directory_path(ec) / "HYPERSAW";
    fs::create_directories(dir, ec);
    // Named by the trace counter, not by a clock: the charter bans wall-clock
    // reads in the core, and a monotonic counter also sorts correctly.
    const uint64_t w = traceWrite.load(std::memory_order_acquire);
    const fs::path out = dir / ("panic-" + std::to_string(w) + ".txt");
    std::FILE *f = std::fopen(out.string().c_str(), "w");
    if (!f) return {};

    std::fprintf(f, "HYPERSAW forensic dump\nreason: %s\nbuild: %s\nsample rate: %.1f\n",
                 why, HYPERSAW_BUILD_STAMP, sampleRate);
    std::fprintf(f, "engine: %s  mono: %d  legato: %d  monoSlot: %d  heldCount: %d\n",
                 spectraMode() ? "SPECTRA" : "SAW", (int)voiceMono, (int)voiceLegato,
                 monoSlot, heldCount);

    /* THE PATCH, because "the envelope sounds wrong" is unanswerable without it.
       The 2026-08-12 Expressive Chords report needed attack/decay/sustain/release
       to separate "the host sent short notes" from "our envelope mis-renders long
       ones", and the dump did not carry them — so the capture settled the note
       STREAM and left the sound unexplained. A forensic dump that records the
       input but not the configuration answers only half of any question. */
    std::fprintf(f, "\n-- patch (the params that shape what you hear) --\n");
    {
      static const int kWanted[] = {1, 4, 6, 8, 14, 17, 19, 20, 21, 22, 32, 34, 42, 94};
      for (int id : kWanted)
      {
        const ParamDef *d0 = findParam((clap_id)id);
        if (!d0) continue;
        std::fprintf(f, "  %-14s", d0->coreKey);
        for (uint32_t k = 0; k < kNumOsc; k++)
          std::fprintf(f, "  osc%u %-9.4f", k, cores[k].getParam(d0->coreKey));
        std::fprintf(f, "\n");
      }
    }

    std::fprintf(f, "\n-- held stack --\n");
    for (int i = 0; i < heldCount; i++) std::fprintf(f, "  [%d] key %d\n", i, heldStack[i].key);

    /* The voice tables per core, side by side with slotOf. This is the exact
       view that would have shown the stuck-note orphan at a glance: a gated
       voice in core 1 whose key appears in no held stack, at a slot the shell
       is not addressing. */
    std::fprintf(f, "\n-- voices (shell slot -> each core's own slot) --\n");
    for (int i = 0; i < hypersaw::kPoly; i++)
    {
      bool any = false;
      for (uint32_t k = 0; k < kNumOsc; k++)
        if (cores[k].voiceAt(i).gate || cores[k].voiceAt(i).env > 1e-4) any = true;
      if (!any && !tags[i].active) continue;
      std::fprintf(f, "  %2d:", i);
      for (uint32_t k = 0; k < kNumOsc; k++)
      {
        const auto &v = cores[k].voiceAt(slotOf[i][k]);
        std::fprintf(f, "  core%u[slot %d] midi %3d %s env %.4f |", k, slotOf[i][k],
                     v.midi, v.gate ? "GATED" : "  off", v.env);
      }
      std::fprintf(f, "  tag %s key %d note_id %d\n", tags[i].active ? "active" : "  --",
                   tags[i].key, tags[i].noteId);
    }

    std::fprintf(f, "\n-- last %u note events, oldest first (pos = absolute sample) --\n",
                 (unsigned)(w < kTraceLen ? w : kTraceLen));
    const uint64_t first = w > kTraceLen ? w - kTraceLen : 0;
    for (uint64_t n = first; n < w; n++)
    {
      const NoteTrace &r = trace[n & (kTraceLen - 1)];
      const char *t = r.type == CLAP_EVENT_NOTE_ON ? "ON   "
                    : r.type == CLAP_EVENT_NOTE_OFF ? "OFF  "
                    : r.type == CLAP_EVENT_NOTE_CHOKE ? "CHOKE" : "?????";
      std::fprintf(f, "  pos %10llu  %s key %3d  note_id %5d  ch %d  port %d  vel %.3f\n",
                   (unsigned long long)r.pos, t, r.key, r.noteId, r.channel, r.port, r.velocity);
    }
    std::fclose(f);
    return out.string();
  }

  /* Panic: capture, THEN clear. The ordering is the whole feature — a dump
     taken after the clear faithfully records a synth in perfect health and
     proves nothing, and panic is precisely the human's tell that the bug just
     happened. Extracted from the GUI lambda so the ordering is reachable from a
     headless oracle; when it lived inline it was guarded only by a comment,
     which trace_check recorded as a known coverage boundary rather than
     pretending to cover. */
  void panicWithDump()
  {
    lastDumpPath = dumpForensics("panic");
    /* RETIRE the outstanding notes; do not DISCARD them. This used to do
       `pendingEndCount = 0` and clear every tag directly, which destroyed every
       NOTE_END the host was owed — a host tracking `note_id`s was left holding
       identities that never end, and nothing downstream could recover them
       because the tag carrying the identity was already gone.

       Same class as L0022 (an END obligation destroyed rather than delivered),
       reached through a different door: there the host REFUSED the push and the
       tag was retired anyway; here the tag was dropped before a push was ever
       attempted. Found 2026-08-11 while answering FOUNDATIONS' question about
       which END cases their seam had not modeled — the question forced a read
       of this function and the defect was sitting in it.

       retireTag() moves each active tag into pendingEnds (respecting its cap)
       and clears `active`, so the blanket clear this replaced is redundant as
       well as wrong. emitNoteEnds then delivers them on following blocks, with
       the try_push retry L0022 installed. */
    for (int i = 0; i < hypersaw::kPoly; i++) retireTag(i);
    allOffAll();
    spectra.allOff();
    rack.reset();
    heldCount = 0;
    monoSlot = -1;
  }

  hypersaw::FxRack rack;  // ADR-054 internal FX rack (post-oscillator)
  /* B23 crosspoint topology over those slots (ADR-088). TWO sources as of
     increment 3 — one per swarm oscillator, each its own post-bass-mono
     buffer — and the default is still the old summed bus: setSerialChain gives
     every source coeff 1.0 into slot 0, so slot 0 gathers `osc0 + osc1` in the
     same float order renderSpan used to.
     `RoutingMatrixT`, not a second spelling of the template arguments: the id
     layout's `kRoutingNSrc` and the audio path's source count must be one
     number or the ids describe a matrix the audio thread does not have. */
  RoutingMatrixT routing;
  /* THE SOURCE BUFFERS for sources 1.. — source 0 is the output buffer itself,
     which oscillator 0 renders straight into as it always has. Every further
     source needs storage that survives from the span loop to the rack pass a
     whole block later, which is the only reason these exist.

     FIXED SIZE, NOT SIZED AT activate(). A buffer whose existence depends on
     activate() having run is the trap renderSpan's own comment records — a
     heap scratch sized there once made audible output conditional on it, and a
     restored instance silently lost oscillator 1 (state_check caught it).
     kSrcBufFrames is ~0.74 s at 44.1 kHz, past any host's block size; the two
     places that could exceed it REFUSE (plug_activate returns false, process
     returns CLAP_PROCESS_ERROR) rather than truncate, because a partial split
     is silently-wrong routing and an unchecked write is memory corruption.

     NOTE THE ALIAS THAT REMAINS. Source 0 still aliases the rack's output
     buffer, so processBlock's rule that the dry term INITIALISES the output
     (traces/2026-09-17-b50-dry-path.md) is still load-bearing — separate
     buffers removed the alias for sources 1.., not for source 0. Giving source
     0 its own buffer too would cost a per-block copy and change nothing an
     oracle can see, so it was not done. */
  static constexpr uint32_t kSrcBufFrames = 32768;
  float srcBufL[kRoutingNSrc - 1][kSrcBufFrames] = {{0}};
  float srcBufR[kRoutingNSrc - 1][kSrcBufFrames] = {{0}};
  static_assert((int)kNumOsc <= kRoutingNSrc, "every oscillator needs a routing source");
  double engineSel = 0;  // 0 SAW, 1 SPECTRA (ADR-037; shell dispatch)
  bool spectraMode() const { return engineSel != 0; }
  double sampleRate = 44100.0;

  // GUI -> audio param queue (producer: GUI main thread; consumer: process on
  // the audio thread, or flush on main when inactive — never concurrent per
  // the CLAP threading contract).
  struct ParamMsg
  {
    uint32_t id;
    double value;
    uint8_t kind;  // 0=value, 1=gesture begin, 2=gesture end, 3=load value (B125)
  };
  /* 2048, not 1024 and certainly not 256 (B110, 2026-09-10): a FULL preset
     applied through applyStateJson enqueues ~323 keys plus the osc-2 twins
     plus the migrators' own writes in ONE burst, and enqueueParam DROPS on
     overflow — so at 256 the tail of the table (osc 2's enable among it)
     silently never landed, on the exact path the GUI's preset LOAD uses. Found
     by B100's fixture generator, pinned by state_check's B110 assertion (RED
     at 256). DOUBLED BY B192, AND THE DOUBLING IS MEASURED, NOT ESTIMATED:
     initState writes every parameter's default through this same queue before
     the patch's own values follow, so one load is two bursts. Held at 1024 the
     whole change set turns state_check RED at "B100: unknown JSON header keys
     ignored, params still apply" — the tail of a burst silently dropped, which
     is B110's failure with a new cause. Peak in-flight depth over state_check's
     corpus, measured with a temporary counter in enqueueParam: 1471 of 2048.
     Static array, 32 KB, RT-safe.
     KNOWN AND PRE-EXISTING, so that a future reader does not mistake it for
     this change's doing: a rig that keeps `processing` true and loads
     repeatedly WITHOUT calling process() between loads never drains, so it
     saturates whatever this constant is (morphlayout_check reaches exactly
     kQCap at 1024 on main and at 2048 here). The shipping paths drain every
     block; raising the cap cannot fix a rig that never drains, and no rig
     assertion depends on the drops. */
  static constexpr uint32_t kQCap = 2048;
  ParamMsg queue[kQCap];
  std::atomic<uint32_t> qHead{0}, qTail{0};

  /* B84 / ADR-160 — UNDO HISTORY, main thread only. The tree, the pending
     MARK (a node is owed, and what to call it), and the monotone counter that
     orders nodes for display. `undoTick` is a counter, never a clock
     (SPEC 5.7). `undoRestoring` suppresses marking while a restore replays a
     snapshot through applyStateJson — a restore is navigation, not an edit,
     and without the guard every undo would create the node it just left. */
  hypersaw::UndoTree undo;
  std::string undoPendingLabel;
  bool undoPending = false;
  bool undoRestoring = false;
  uint64_t undoTick = 0;

  // Engine -> GUI viz feed: classic double buffer; writer alternates, reader
  // only ever copies the published side.
  hypersaw::VizSnapshot vizBuf[2];
  std::atomic<int> vizPublished{0};

  hypersaw::HypersawGui *gui = nullptr;
  // Spectrum feed: mono ring written on the audio thread (write-only, cheap);
  // the FFT runs on the GUI thread on demand — zero audio-thread analysis
  // cost, torn reads are cosmetic-only (visualizer).
  float specRing[4096] = {0};
  std::atomic<uint32_t> specPos{0};
  // Scope feed (2026-08-03): STEREO, unlike specRing's mono sum — the whole
  // point of a scope here is watching L against R (super-width's polarity
  // modes are invisible in a sum). Write-only on the audio thread.
  double outPeakViz = 0;   // peak since the last viz publish (see publishViz)
  /* B106: one ring PER OSCILLATOR, not one for whichever osc the viz followed.
     MAIN draws both waves, so both taps must exist at once. Fixed-size member
     arrays — preallocated by construction, so the audio-thread write below is
     still a ring store and nothing else. */
  float scopeL[kMaxOsc][2048] = {{0}}, scopeR[kMaxOsc][2048] = {{0}};
  std::atomic<uint32_t> scopePos[kMaxOsc] = {};
  uint32_t guiW = 980, guiH = 720;  // resizable (clamped in gui_adjust_size)
  std::atomic<bool> processing{false};
  // ADR-024: the inertia KNOB value (params/state domain). The core holds
  // sqrt(knob) — squaring the core value back is not bit-exact, and
  // state_check demands exact round-trips, so the knob domain gets this one
  // documented slot. Everything else stays core.p-authoritative.
  double inertiaKnob = 0;
  // ADR-059 tune-then-lock: taper exponent for the inertia knob. 0.5 == the
  // ADR-024 sqrt taper (bit-inert default). Higher = gentler onset just after 0
  // (the low-detune+retrigger steepness). DEV control — dial by ear, then the
  // chosen value gets hardcoded and this param + slider removed.
  double inertiaCurve = 2.5;   // ADR-024 A1; must match the ParamDef default
  // ADR-026 shell voice-mode state (audio-thread only)
  double voiceMono = 0, voiceLegato = 1;
  // ADR-082 classified transpose (35/36/37) PER-OSCILLATOR — "an octave down
  // replaces what a sub would do" — but increment 2 left this shell state as a
  // single copy, so editing osc 2's pitch was silently dropped and the GUI
  // poll snapped the control back (human report 2026-08-07). One copy per
  // oscillator; pitchBend stays global (the wheel bends the patch).
  double octaveA[kMaxOsc] = {0}, semiA[kMaxOsc] = {0}, fineCentsA[kMaxOsc] = {0};
  // B24: mute/solo targets, and the smoothed gain the mix actually applies.
  // Smoothed because a hard 1->0 on a ringing oscillator is a click; same
  // one-pole the master fader uses.
  double oscMute[kMaxOsc] = {0}, oscSolo[kMaxOsc] = {0};
  double oscGainSm[kMaxOsc] = {1.0, 1.0};
  /* B48: morph-derived osc on-weight. Partway between a corner with the osc
     ON and one with it OFF, the audible transition is this RAMP, not the
     stepped enable flip -- the flip still happens, but only at the weight
     floor where this gain has already faded the osc inaudible. 1.0 whenever
     the morph is off, the osc's enable is exempt, or every relevant corner
     agrees -- all of which keep oscGainTarget() on its old values exactly. */
  double oscOnW[kMaxOsc] = {1.0, 1.0};
  double oscPeakViz[kMaxOsc] = {0};   // per-oscillator meter, drained by publishViz

  // Mute wins over solo; any solo anywhere silences every non-soloed
  // oscillator. Computed from the targets, never stored, so the two params
  // remain the single source of truth (a cached "anySolo" flag is one more
  // thing to forget to update).
  /* Gate one oscillator's block and take its meter reading.
     `chunked` says whether this buffer is a slice of a larger render (the
     temp-chunk path): the smoothing coefficient is per-sample either way, so
     the only difference is that a chunked call must NOT reset the peak.
     Gain 1.0 with nothing to smooth skips the multiply entirely, which is what
     keeps an untouched patch bit-identical to a pre-mixer build. */
  void applyOscGainAndMeter(uint32_t k, float *bL, float *bR, int n, bool chunked)
  {
    const double target = oscGainTarget(k);
    const double c = gainSmoothCoef();
    double g = oscGainSm[k];
    double peak = chunked ? oscPeakViz[k] : 0.0;
    const bool settled = g == target;
    for (int i = 0; i < n; i++)
    {
      if (!settled)
      {
        g += (target - g) * c;
        if (std::fabs(g - target) < 1e-6) g = target;
      }
      if (g != 1.0)
      {
        bL[i] = (float)(bL[i] * g);
        bR[i] = (float)(bR[i] * g);
      }
      const double a = std::fabs((double)bL[i]) > std::fabs((double)bR[i])
                           ? std::fabs((double)bL[i]) : std::fabs((double)bR[i]);
      if (a > peak) peak = a;
    }
    oscGainSm[k] = g;
    oscPeakViz[k] = peak;
    /* ADR-100 A4: scope tap — this oscillator's own post-gain signal. B106
       dropped the `k == vizOsc` gate: MAIN draws every oscillator's waveform,
       so every oscillator has to be tapped, not just the followed one. Each
       osc writes only its OWN ring, so the per-osc cost is what it always was.
       Ring write only; RT-safe. */
    if (k < kMaxOsc)
    {
      uint32_t sw = scopePos[k].load(std::memory_order_relaxed);
      for (int i = 0; i < n; i++)
      { scopeL[k][(sw + i) & 2047] = bL[i]; scopeR[k][(sw + i) & 2047] = bR[i]; }
      scopePos[k].store(sw + (uint32_t)n, std::memory_order_release);
    }
  }

  // Same ~8 ms one-pole the master fader uses. Shared so the two faders in the
  // mixer cannot drift apart in feel, and so there is one place to change it.
  double gainSmoothCoef() const
  {
    return 1.0 - std::exp(-1.0 / (0.008 * sampleRate));
  }

  double oscGainTarget(uint32_t k) const
  {
    if (oscMute[k] != 0) return 0.0;
    bool anySolo = false;
    for (uint32_t i = 0; i < kNumOsc; i++)
      if (oscSolo[i] != 0) { anySolo = true; break; }
    if (anySolo && oscSolo[k] == 0) return 0.0;
    return oscOnW[k];   // B48: 1.0 except partway across an enable boundary
  }
  double pitchBend = 0, gSemi = 0, gFine = 0, gOct = 0;   // global transpose (101/102/103)
  int lastNoteKey = 69;   // quantise anchor for the GLOBAL wheel lane (A4 until a note arrives)

  /* BEND TRAVEL LAW (glide_core, folded 2026-08-19). `pitchBend` is now the
     SOUNDING bend; `bendTarget` is where the wheel asked it to go. With the law
     OFF they are the same value and the code below is a pass-through — kOff sets
     `x = target; vel = 0; y = target`, which is the property that lets this land
     in the audio path with parity provably unmoved (147/147 + subdiv + the
     sample-rate probe) BEFORE any law is exposed.
     The law params do not exist yet, so `bendActive()` is false everywhere today
     and the render takes exactly the path it took before this change. That is
     deliberate: wire first, prove inert, expose second. */
  hypersaw::GlideCore bendGlide{44100.0 / 16.0, /*bendLane=*/true};
  // The core calls kConstRate its "ratified default" — that is the BENCH's default
  // for auditioning. Shipping it would change how every existing patch bends, so
  // the PLUGIN ships kOff (human ruling 2026-08-19), matching the precedent that
  // oscillators above the first default to silent: a default must not rewrite a
  // sound that already exists.
  hypersaw::GlideCore::Params bendLaw = [] {
    hypersaw::GlideCore::Params q;
    q.model = hypersaw::GlideCore::kOff;
    return q;
  }();
  /* THE GLOBAL SCALE, held in ONE place rather than inside bend's law struct.
     Bend is the first consumer, not the owner: the note-pitch lane, the chord
     layer and any arp read the same {root, mask}, and two modules disagreeing
     about the scale produce notes in neither key.
     THIS IS ALSO THE SEAM. Today the root and mask come from thirteen CLAP
     params. A future provider — Tonality is the obvious one — would fill this
     same struct instead, and nothing downstream would need to change, because
     downstream only ever reads {root, mask}. That is exactly what the standing
     ruling bought: consumers transmit the mask, never a scale ID, so the thing
     that PRODUCES the mask is swappable. */
  /* Tet12 IS IN THE NAME ON PURPOSE (Tonality, HYPERSAW-002 §5, 2026-08-19).
     `root` 0-11 with twelve slots is not "a scale" — it is a 12-TET scale, and
     their Decision 6 keeps tuning behind a reduction boundary precisely so the
     assumption cannot leak by being unnamed. Their words: the cost is a rename
     today; the cost of not doing it is that in two years something reads
     `ScaleState` and assumes a generality it never had.
     Neither project supports anything beyond 12-TET, and neither is asking to.
     The ask was only that the name carry the assumption. */
  struct Tet12ScaleState
  {
    double root = 0;                                        // 0..11, C..B
    int mask[12] = {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1};    // C major, as shipped
  } scale;

  double bendTarget = 0;
  int bendAccum = 0;                     // samples owed to the bend grid
  int bendGridSamples() const
  {
    const int g = (int)std::lround(sampleRate * kBendGridSeconds);
    return g < 1 ? 1 : g;
  }
  /* The lane is active when ANYTHING in it processes the value — the law OR the
     quantiser. This asked only about the law until 2026-08-21, so with the law
     off (the shipped default) applyParam(38) instant-wrote the wheel PAST
     GlideCore::step(), and the quantiser lives inside step(): "bend quantize
     just quietly stopped working (it seems to quantize the initial note but no
     longer the bend part)" — the note lane steps its own GlideCore, the wheel
     path skipped its own. kOff + quant is cheap on the grid: step() is a
     pass-through that still quantises, which is exactly the published contract
     of kOff. */
  bool bendActive() const
  {
    return (int)bendLaw.model != hypersaw::GlideCore::kOff ||
           (int)bendLaw.quant != hypersaw::GlideCore::kQuantOff;
  }

  /* NOTE LANE (ADR-096). `noteLawOwn` ships LAG because that is precisely what
     id 33 has always done, so a plugin that loads an old patch travels exactly
     as it used to. The link is resolved HERE and pushed as a finished struct:
     resolving it in the core would put a shell-only concept (which of two
     parameter sets is live) inside the DSP. */
  hypersaw::GlideCore::Params noteLawOwn = [] {
    hypersaw::GlideCore::Params q;
    q.model = hypersaw::GlideCore::kLag;
    // tau tracks id 33, which ships 0 — GlideCore's own 60 ms default would make
    // the lane glide while the Note Lag slider read zero.
    q.tau = 0;
    return q;
  }();
  double noteLink = 1;   // 0 = own settings, 1 = follow bend law (shipped)

  /* Mode + rate/division -> the reference's qTime in MILLISECONDS. Kept in one
     place because both lanes read it: the bend lane directly, and the note lane
     through pushNoteLaw's copy of bendLaw. bpm is the host's (core.p.bpm), so a
     sync setting follows the session tempo without the core knowing what a beat
     is. Guarded against a zero/absent tempo — a host that never sends transport
     would otherwise divide by zero and hand the gate an infinity. */
  double resolveQTimeMs() const
  {
    const int mode = (int)qTimeMode;
    if (mode == 1) return 1000.0 / std::max(0.01, qTimeHz);
    if (mode == 2)
    {
      const double bpm = core.p.bpm > 1 ? core.p.bpm : 120.0;
      const double cyclesPerSec = (bpm / 60.0) * std::max(0.01, qTimeSync);
      return 1000.0 / cyclesPerSec;
    }
    return 0;   // continuous
  }
  double qTimeMode = 0, qTimeHz = 8, qTimeSync = 4;
  int oscEnabled[kMaxOsc] = {1, 0};   // ADR-100; osc2 ships OFF (ADR-099 A1)
  /* ADR-109 EXEMPT — the third member of the lab's authorship family (bias
     nudges a corner's share, pin hands one corner the field, exempt removes the
     parameter from the field entirely). Patch state, not parameters: 150-odd
     booleans as automation lanes would be noise. Indexed by morphIds position,
     so it rides the same append-only order the corners do. */
  std::vector<uint8_t> morphExempt;
  std::vector<uint8_t> morphGroupSplit;   // B222 scratch, by group lead: do the corners disagree?
  double morphArm = 0;
  bool morphFromField = false;   // ADR-109 re-entry guard, see the hook
  /* B125 (human 2026-09-14: a reverted corner "might become whatever the corner
     A preset is"): a STATE LOAD — preset, history restore, host chunk — is a
     whole-state replace whose corners arrive in the morph chunk. Its parameter
     writes are the loaded state's LIVE values and must never be routed into a
     corner as edits; before this flag they were, through the ADR-109 choke
     point, so with morph on the queued writes rewrote the armed / winning /
     weighted corners with the live values — with the pad parked on A, corner B
     became A. Set for the duration of a load's writes only. */
  bool loadingState = false;
  /* B222: set for the duration of an EDITOR value write's apply (queue kind
     0, which only guiSetParam produces, bar id 90's load migration). It is the
     one fact morph-on needs to tell the player's toggle from host automation,
     so the player's toggle can keep the patch while an existing session's
     automation plays back exactly as it always has. */
  bool editorWrite = false;
  /* ATOMIC GROUP (ADR-109 A1): indices in [first, last] share ONE corner
     decision, taken on the group's first index. Today the scale is the only
     group; the mechanism is general because the next one (a chord voicing, an
     FX slot's four params) will want it. */
  /* ATOMIC MORPH GROUPS (B49). Parameters that only mean something TOGETHER
     draw one corner between them, so the field can never assemble a state no
     corner authored. Was one hardcoded range (the scale, ADR-109 A1); now an
     explicit lead map, identity except where a group says otherwise -- so a
     second group is data, not another special case in the picker.
     Members are addressed by morphIds INDEX, not id, and a group's members
     need not be contiguous: an FX slot's type/amount/tone sit in three
     different id blocks. */
  /* Have the corners ever been AUTHORED, or are they still the seed?
     morphInit() runs once, lazily, from whichever morph path is touched first --
     in practice at startup, long before the player has edited anything. It seeds
     all four corners with parameter DEFAULTS, and its comment reasons that this
     is "silence-safe: every corner agrees". That is true of a FRESH instance,
     where live == default, and false the moment the player edits the patch: the
     first grid tick after morph is switched on then writes those stale defaults
     over their sound. Reported 2026-08-26: "switching morph on when you've
     edited the patch can be destructive; it replaces the sound with default
     inits."
     This flag is what lets morph-on distinguish "corners hold real content,
     leave them alone" from "corners are still the seed, adopt what is playing".
     Set by every path that gives a corner meaning: capture, an armed edit, an
     exempt write, a corner-preset apply, and a state chunk that carried
     corners. */
  bool morphCornersAuthored = false;
  std::vector<uint32_t> morphLead;   // index -> the index whose corner it follows
  size_t morphGroupLead(size_t i) const
  {
    return i < morphLead.size() ? (size_t)morphLead[i] : i;
  }
  // Every index sharing `i`'s lead — a group exempts and flips as one unit.
  void morphGroupRange(size_t i, size_t &lo, size_t &hi) const
  {
    const size_t lead = morphGroupLead(i);
    lo = hi = i;
    for (size_t j = 0; j < morphLead.size(); j++)
      if (morphGroupLead(j) == lead) { if (j < lo) lo = j; if (j > hi) hi = j; }
  }

  /* ================= QUANTUM MORPH (ADR-104) =================
     The SHELL owns which parameters morph and what a corner is; morph_core.h
     owns the math. The morphable set is v1-curated: every PER-OSC parameter
     (the twin-having set — timbre) for both oscillators, enables included
     ("they can just toggle on and gradually increase the volume as they move
     between corners" — human, 2026-08-20). Globals (bend law, scale, master,
     voice routing) stay patch-level; the ADR records that the set widens
     later, with the corner-editing model, rather than v1 guessing at it.
     Corner snapshots persist in the state chunk (a corner IS patch data);
     they are NOT parameters — 4 x ~100 automation lanes would be noise. */
  /* B69 increment 2. The matrix instance and the ONE destination this
     increment applies: a pitch offset in semitones joining updateTune's sum.
     Applied as an OFFSET beside the stored params — never written back into
     any param — so readback, state and automation all still see the base
     value; corrupting the base is the classic matrix mistake and the reason
     destinations are added one at a time. modPitchSm is slewed at the grid
     rate (~8 ms one-pole, the gainSmoothCoef feel) because env * depth at
     48 st moves fast enough to zipper otherwise. */
  /* B134 — THE SOURCE POLARITY TABLE. What each slot naturally emits, in the
     same slot order the source-slot comments in modStep() and gui2.html's
     MOD_SRC_NAMES use. It lives HERE rather than in ModCore because knowing
     that slot 17 is the pitch wheel is shell knowledge; the core is handed a
     flag array and stays framework-free.
       0    ENV 1        unipolar   (amp envelope)
       1    ENV 2        unipolar   (pitch envelope)
       2-9  Macros 1-8   unipolar
       10-13 XY aliases  BIPOLAR    (retired, ADR-156: they read 0 — declared
                                     so a saved route's halo does not lie
                                     about the span it would have had)
       14   Velocity     unipolar
       15   Mod wheel    unipolar
       16   Pressure     unipolar
       17   Pitch wheel  BIPOLAR
       18   LFO 1        BIPOLAR    (B171 — an LFO swings ±1 about base)
       19   LFO 2        BIPOLAR
       20   ENV 3        unipolar   (B171)
       21   ENV 4        unipolar
       22+  unassigned   unipolar   (the default a new source inherits)
     Installed through a member initializer, not a call in the factory: a
     construction path that forgot the call would give that instance a silently
     all-unipolar table, which is exactly the kind of init-order trap this file
     already carries scars from. */
  static hypersaw::ModCore makeModCore()
  {
    hypersaw::ModCore m;
    for (int i = 10; i <= 13; i++) m.srcPol[i] = hypersaw::ModCore::kSrcBipolar;
    m.srcPol[17] = hypersaw::ModCore::kSrcBipolar;
    m.srcPol[18] = hypersaw::ModCore::kSrcBipolar;   // B171 LFO 1
    m.srcPol[19] = hypersaw::ModCore::kSrcBipolar;   // B171 LFO 2
    return m;
  }
  hypersaw::ModCore mod = makeModCore();
  double modPitchSt = 0, modPitchSm = 0;
  /* ADR-136: generic destinations. For every param the matrix targets, the
     shell owns the BASE here — the value the player/host/morph authored — and
     writes base+offset through the normal apply path each mod tick. Readback
     reports base, so state, automation and the GUI never see the modulation.
     modFromMatrix is the re-entrancy guard (the morphFromField pattern): a
     matrix write must not route into morph corners or update its own base. */
  struct ModDest { clap_id id = 0; double base = 0, lastApplied = 1e300; bool active = false; };
  ModDest modDests[hypersaw::ModCore::kMaxRoutes];
  bool modFromMatrix = false;
  /* DEFAULT MAPPING (human 2026-08-29): a fresh instance ships with Macro 1
     driving BOTH oscillators' detune and Macro 2 driving both pull-Ks — so
     the default pads (M1/M2 on osc 1's pad) feel like the old hardwired XY
     from the first note. Load-is-a-load still governs: a saved set's chunk
     REPLACES these, absent keys clear them (ADR-138) — defaults are what you
     get before you have said anything, never what overrides what you said.
     Depths: detune 0.7 of range; K 1.0 (a unipolar macro can only push K up
     from base, so full depth is what makes the pad's reach musical). */
  /* modInstallDefaults() is gone (ADR-156): the four M1/M2 -> detune/K
     routes existed to give macro-driven osc pads a rest. The pads write the
     params directly now; a fresh instance has NO routes, which is what an
     empty matrix should honestly say. */
  ModDest *modDestFor(clap_id id, bool create)
  {
    for (auto &d : modDests) if (d.active && d.id == id) return &d;
    if (!create) return nullptr;
    for (auto &d : modDests)
      if (!d.active) { d.id = id; d.base = readParam(id); d.lastApplied = 1e300; d.active = true; return &d; }
    return nullptr;
  }
  /* ADR-135/162: ENV 2, a shell-side ADSR advanced at the mod grid — now one
     envelope PER NOTE SLOT, which is the per-note fan-out increment ADR-135
     named and ADR-161 deferred. One-pole approaches per stage (attack -> 1,
     decay -> sustain, release -> 0); the times (162-165) are shared, the
     STATE is not. Preallocated, advanced on the audio thread, no allocation.

     Stage transitions are per slot: `retrig` (set by every note-on for that
     slot — fresh strike, retarget, steal) restarts the attack FROM THE CURRENT
     LEVEL, so a re-strike mid-decay rises from where it is rather than from
     zero; losing the gate is the release. ADR-161's every-strike restart is
     subsumed — per note, every strike IS a fresh envelope, and a strike can no
     longer blip the notes already held (the bug the human reported). */
  struct PitchEnv
  {
    double level = 0;
    int stage = 0;       // 0 idle/release, 1 attack, 2 decay/sustain
    bool retrig = false; // set at note-on (audio thread), consumed once per tick
  };
  PitchEnv penv[hypersaw::kPoly];
  double env2A = 0.003, env2D = 0.16, env2S = 0.0, env2R = 0.16;
  /* B171 — ENV 3 and ENV 4, mod source slots 20 and 21. Same type, same law,
     same per-slot state as ENV 2; only the pitch-route projection is ENV 2's
     alone. They carry no retrig flag of their own: `penv[s].retrig` is set at
     note-on for the slot, and all three envelopes consume that ONE flag in
     modStep — a second and third flag set at the same three note-on sites is
     three chances to miss one (L0029's shape, one level down). */
  static constexpr int kExtraEnvs = 2;
  PitchEnv xenv[kExtraEnvs][hypersaw::kPoly];
  double xenvA[kExtraEnvs] = {0.003, 0.003}, xenvD[kExtraEnvs] = {0.16, 0.16},
         xenvS[kExtraEnvs] = {0.0, 0.0}, xenvR[kExtraEnvs] = {0.16, 0.16};
  /* ONE ADSR LAW, THREE ENVELOPES. Extracted from modStep's ENV 2 block
     verbatim — same branches, same order, same constants — because a second
     copy of an envelope is precisely the repo's named failure, and ENV 2 must
     stay BIT-identical (parity_check / state_check / bank_check prove it).
     `gated` is the slot's key state; `retrig` is consumed by the CALLER, once
     per tick, so the three envelopes that share the flag all see it. */
  static void advanceAdsr(PitchEnv &pe, bool gated, bool retrig, double dt, double A, double D,
                          double S, double R)
  {
    if (retrig) pe.stage = 1;
    if (!gated) pe.stage = 0;   // this slot's key is up: release
    // An idle slot at rest costs nothing: no exp(), no pow() in setNoteExpr.
    if (pe.stage != 0 || pe.level != 0.0)
    {
      double target, tau;
      if (pe.stage == 1) { target = 1.0; tau = A; }
      else if (pe.stage == 2) { target = S; tau = D; }
      else { target = 0.0; tau = R; }
      pe.level += (target - pe.level) * (1.0 - std::exp(-dt / std::max(1e-4, tau)));
      if (pe.stage == 1 && pe.level > 0.99) { pe.level = 1.0; pe.stage = 2; }
      // SNAP TO EXACTLY ZERO at the end of a release (the modPitchSm rule):
      // a one-pole only approaches 0, and "approaches" would leave a dead
      // slot's noteTune a hair off 1.0 forever — a permanent detune the
      // ear finds long before an oracle does.
      if (pe.stage == 0 && std::fabs(pe.level) < 1e-6) pe.level = 0.0;
    }
  }

  /* ---- B171: TWO LFOs, mod source slots 18 and 19 ------------------------
     A phase accumulator advanced at the mod grid (ADR-009: the rate is in Hz
     or in beats, never a per-tick constant), a shape read out of that phase,
     and — for S&H — one seeded mulberry32 stream per LFO. Preallocated, no
     allocation on the audio thread (rtsafety_probe).

     STEPPING, NOT SMOOTHING, AND DELIBERATELY SO. The generic destination path
     (ADR-136) applies `base + delta*span` with no filter, and this increment
     does NOT add one — the human's brief is the simple version. A square or
     S&H LFO into an audio-rate destination therefore steps at the 172 Hz mod
     tick (256/44100 s). That is a known, recorded limit, not an oversight. */
  static constexpr int kNumLfo = 2;
  struct Lfo
  {
    double phase = 0;        // [0,1), the cycle position
    double sh = 0;           // S&H's held value, bipolar
    uint32_t rng = 0;        // the mulberry32 stream state
    bool drawn = false;      // has S&H ever drawn? gates the state-chunk key
    bool restored = false;   // a chunk set us; activate() must not overwrite it
  };
  Lfo lfo[kNumLfo];
  bool lfoStruck = false;   // set by the envelope loop, read by the LFO loop
  double lfoRate[kNumLfo] = {1, 1}, lfoBeats[kNumLfo] = {1, 1}, lfoPhase0[kNumLfo] = {0, 0};
  int lfoShape[kNumLfo] = {0, 0}, lfoSync[kNumLfo] = {0, 0}, lfoRetrig[kNumLfo] = {0, 0};
  /* The patch seed XOR the LFO index — the index scaled by the golden-ratio
     word mulberry32 itself steps with. A bare `^ i` would hand LFO 1 the patch
     seed VERBATIM (the same stream every other consumer of that seed draws)
     and LFO 2 its immediate neighbour, and two adjacent mulberry32 seeds are
     not independent enough for two S&H lanes to sound uncorrelated. */
  uint32_t lfoSeed(int i) const
  {
    return (uint32_t)core.p.seed ^ (0x9E3779B9u * (uint32_t)(i + 1));
  }
  /* Re-seed and rewind. Called from activate() and from the `seed` param — the
     same place SwarmCore's own rebuild() re-rolls its streams, so "change the
     seed" means one thing across the device. */
  void lfoReseed()
  {
    for (int i = 0; i < kNumLfo; i++)
    {
      lfo[i].rng = lfoSeed(i);
      lfo[i].sh = 0;
      lfo[i].drawn = false;
      lfo[i].phase = lfoPhase0[i];
    }
  }
  /* Bipolar (±1) readout of `phase`. Every shape crosses zero AT phase 0 and
     has mean 0 over a cycle, so switching shape does not jump the destination
     to a different average — the property the oracle's shape rows pin. */
  static double lfoShapeAt(int shape, double ph, double sh)
  {
    switch (shape)
    {
      case 1:   // triangle: 0 -> +1 -> 0 -> -1 -> 0
        return ph < 0.25 ? 4.0 * ph : (ph < 0.75 ? 2.0 - 4.0 * ph : 4.0 * ph - 4.0);
      case 2: return 2.0 * ph - 1.0;          // saw up: monotone across the cycle
      case 3: return 1.0 - 2.0 * ph;          // saw down
      case 4: return ph < 0.5 ? 1.0 : -1.0;   // square: two-valued, exactly
      case 5: return sh;                      // S&H: piecewise-constant per wrap
      default:
      {
        // A LOCAL π, not M_PI — MSVC leaves M_PI undefined and portability_gate
        // fails the file that uses it (L0003, bit three times).
        constexpr double kPi = 3.141592653589793;
        return std::sin(2.0 * kPi * ph);
      }
    }
  }
  /* The GLOBAL projection of ENV 2 — mod source slot 1, what every route
     OTHER than the pitch route reads. Max over GATED slots, ENV 1's
     convention, so an ENV 2 -> filter patch keeps its meaning with one note
     and takes the loudest-shaped voice with a chord (ADR-162). Diagnostic
     companion: the stage of whichever slot won the max, -1 if none. */
  double env2 = 0;
  int env2Stage = -1;
  // ADR-137: macro values (mod source slots 2-9) and the XY axis assignment
  // [osc0 X, osc0 Y, osc1 X, osc1 Y], each an index into macroVal.
  double macroVal[8] = {0.5, 0.5, 0, 0, 0, 0, 0, 0};   // 1/2 match their 0.5 table default (paramscope sweep)
  int xyAsn[4] = {0, 1, 2, 3};
  int mainAsn[2] = {0, 1};                       // ADR-150: MAIN pad's own pair
  double pitchContA[kMaxOsc] = {0};              // ADR-150: continuous per-osc pitch (st)
  /* = 1, matching the table (paramscope's default-truth sweep caught this as
     a LIE: info said 1, readback said 0 — so fresh instances showed the
     specimen OFF all along, which is why the human kept asking to "default it
     on" after it was nominally defaulted. The member init and the table row
     are two copies of one fact; the sweep is what keeps them honest. */
  double specimenOn = 1;
  /* ADR-149: MIDI/MPE performance signals as matrix sources (slots 14-17).
     GLOBAL projections for now — per-note APPLICATION is B82's build; these
     make the wheel/pressure/velocity routable today. */
  double srcVel = 0;      // last note-on velocity, 0..1
  double srcWheel = 0;    // CC1, 0..1 (was previously DROPPED entirely)
  double srcPress = 0;    // latest pressure (channel AT or any note expression)
  double srcPitchW = 0;   // plain pitch wheel, -1..1 (bipolar source)
  /* RETIRED (ADR-162): env2Gate (the shared gate EDGE) and env2Retrig (the
     shared every-strike flag, ADR-161). Both were properties of ONE envelope
     shared by every voice; per slot the gate and the retrigger are properties
     of that slot — `penv[s].retrig` and `slotGated(s)`. The bug ADR-161 fixed
     (a held note's spike shrinking under each new strike) and the bug it left
     behind (a strike blipping the held notes) were both the sharing. */
  hypersaw::MorphCore morph;
  std::vector<clap_id> morphIds;          // id order = persistence order (stable)
  std::vector<double> morphCorner[4];     // snapshots, aligned to morphIds
  std::vector<double> morphCur;           // last applied value per morphIds slot
  /* ADR-183: per slot, the morphIds INDEX of the switch that gates this
     slot's source (its oscillator's enable, or its engine block's gateId), or
     -1 when the slot belongs to no switchable source. Sized and filled once in
     morphInit, so the blend reads it on the audio thread without allocating. */
  std::vector<int32_t> morphGateSlot;
  /* ADR-183 / critic S1 (PR #744): the distinct gate slots, and per gate slot
     whether its source was OFF when this tick began. Snapshotted at the top
     of each morph tick because the gate itself is re-committed INSIDE the
     slot loop, and its position in morphIds relative to the slots it gates
     differs per source. Sized in morphInit; the audio thread only writes. */
  std::vector<int32_t> morphGateSlotList;
  std::vector<uint8_t> morphSrcWasOff;
  // ADR-115: MUST match the ParamDef defaults for 152/153 (corner A = 0,0).
  // paramscope_check's default-truth sweep exists for exactly this pair going
  // out of step, and caught it the first time this changed.
  double morphX = 0.0, morphY = 0.0, morphTemp = 1, morphCoup = 0.3;
  double morphOn = 0, morphMode = 0, morphGlideS = 0.008;
  uint32_t morphSeed = 1024;
  int morphAccum = 0;
  /* ADR-176 decision 6 — the intent-bus flag (param 266). Kept here rather
     than beside the rack's flags because it gates ONE branch, at the top of
     morphStep, and reads as a morph control at the only site that consults
     it. Its meaning changes between phase 2b (shadow: resolve, apply nothing)
     and 2c (applied); harmless for a dev param that ships off. */
  double intentBusOn = 0;

  /* ADR-159 — THE PREFIX IS FROZEN. The per-osc block below is built by
     walking the param table, so a per-osc row added to the table lands INSIDE
     the prefix and shifts every entry after it: `oscPitch` (181, ADR-150,
     2026-08-31) did exactly that, and every corner/exempt array saved between
     ADR-104 A2 (the bend/note tail, 2026-08-21) and 2026-08-31 read its bend
     law two slots off — bendSpringF as bendRate, bendQuant as bendDistOver,
     bendQTimeHz as … — a spring-quantised 2 s step gate on patches that had
     none. Hidden until B110 let `morphOn` land on preset load. Per-osc rows
     added after the freeze are listed here and APPENDED after every earlier
     block, twin beside base, so append-only is true by construction again.
     `applyMorphChunk` remaps arrays saved under the 2026-08-31..09-11 layout. */
  static constexpr clap_id kMorphLateIds[] = {181};
  static constexpr size_t kMorphAdr150Size = 224;   // the only layout that ever had 181 in the prefix
  static bool isMorphLateId(clap_id id)
  {
    for (clap_id l : kMorphLateIds) if (l == id) return true;
    return false;
  }
  // One builder for both the live order and the 2026-08-31 legacy order.
  static std::vector<clap_id> buildMorphOrder(bool lateInPrefix)
  {
    std::vector<clap_id> ids;
    for (const auto &d : kParams)
    {
      if (isGlobalId(d.id)) continue;
      if (!lateInPrefix && isMorphLateId(d.id)) continue;
      ids.push_back(d.id);
      ids.push_back(d.id + 1000);    // the twin — each osc morphs its own
    }
    return ids;
  }

  void morphInit()
  {
    if (!morphIds.empty()) return;
    morphIds = buildMorphOrder(/*lateInPrefix=*/false);
    /* ADR-104 Amendment 1: the FX rack joins the field — slot type, amount,
       tone, mix. This is what makes "off in this corner, driven in that one"
       a rack story and not only an oscillator story, and it is the
       prerequisite for module-level exempt. morphIds is APPEND-ONLY (like
       param ids): v1 corner chunks fill the per-osc prefix in their original
       order and the FX tail takes defaults — an old patch loads with its
       corners intact and its rack unmorphed, which is exactly what it said
       when it was saved. */
    for (clap_id id : {57u, 58u, 59u, 60u, 61u, 62u, 63u, 64u,
                       96u, 97u, 98u, 99u, 133u, 134u, 135u, 136u})
      morphIds.push_back(id);
    /* ADR-104 A2 (human 2026-08-21): the BEND and NOTE-TRAVEL laws join the
       field -- a corner can hold "spring bend, scale-quantised" while another
       holds "instant, free". Appended AFTER the FX block: the morphIds order is
       append-only, so every stored corner chunk keeps its meaning. morphX/Y and
       the morph controls themselves (151-158) stay out by construction -- the
       field must not morph its own position. Law flips are already safe
       mid-flight: applyParam(106) resets the traveller to the sounding pitch. */
    for (clap_id id : {33u, 106u, 107u, 108u, 109u, 110u, 111u, 112u, 113u,
                       114u, 115u, 137u, 138u, 139u, 140u, 141u, 142u, 143u,
                       144u, 145u, 146u, 147u, 148u, 149u})
      morphIds.push_back(id);
    /* ADR-109 A1 — the globals a human scan found unreachable by right-click
       (2026-08-22). They were never in the field, so exempt had nothing to
       toggle and silently did nothing; "doesn't work" was the honest reading.
       Appended, never inserted: morphIds order is the corner chunk's order, so
       an existing patch keeps every value it stored.
       `inertia` and `inertiaCurve` are here because a corner that changes the
       swarm's weight changes its character more than most timbre knobs. */
    for (clap_id id : {11u, 70u, 32u, 34u, 38u, 90u, 75u})
      morphIds.push_back(id);
    /* THE SCALE IS ONE THING. Root + twelve degrees flip as a UNIT: a
       per-degree flip would assemble a chimera scale from two corners — C major
       and F# minor interleaved is not a scale, it is a bug with a musical
       name. The human said it exactly: "all the individual scale degrees would
       need to be included collectively, of course." */
    const size_t scaleFirst = morphIds.size();
    for (clap_id id = 116; id <= 128; id++) morphIds.push_back(id);
    const size_t scaleLast = morphIds.size() - 1;
    // ADR-159: the late per-osc rows, appended last (see kMorphLateIds).
    for (clap_id id : kMorphLateIds) { morphIds.push_back(id); morphIds.push_back(id + 1000); }
    /* ADR-088 crosspoints, APPENDED AFTER EVERYTHING (ADR-159's rule: never
       inserted — the corner chunk's order IS this order, so an insertion would
       silently re-read every stored corner against the wrong parameters).

       A corner may therefore hold a whole TOPOLOGY. Under BLEND (157 = 1) the
       cells interpolate as VALUES, cell by cell — a crosspoint is continuous
       and 0 already means "not connected", so connecting and disconnecting is
       one continuous motion (ADR-088's founding argument). Under QUANTUM (the
       default) the whole block draws ONE corner: the lead map below, not this
       append, is where that is decided.
       The comment that stood here until 2026-09-18 cited ADR-125 for "no
       argmax", which is the OPPOSITE of what ADR-125 rules — "ARGMAX over
       topology means every route coefficient draws the same corner ... all
       route ids point at one lead index" — and the identity lead map that
       matched the comment shipped as the default (B142, ADR-176 §3).
       Legality is enforced on the READ side, so a corner holding any table at
       all stays correct by construction — which is exactly why this is safe. */
    for (const auto &d : g_routingTable.defs) morphIds.push_back(d.id);

    /* B172 — THE ENGINE BLOCKS' MORPHABLE ROWS, APPENDED AFTER THE ROUTING
       BLOCK and therefore after everything (ADR-159's rule again: never
       inserted). Membership is a CLASS TEST and nothing else, so the field and
       the classifier cannot disagree about which engine rows a corner holds —
       the block's GATE is Device and is therefore absent by that same test,
       which is what keeps "the gate is not a corner value" a property of one
       rule rather than of two lists.
       STATION appends here too (B162), by adding its block to kEngineBlocks.
       Both appends bump the layout marker; see cornerJson.

       B195 — THE RULING B172 OWED (human 2026-09-21: "some Sub Osc parameters
       don't reach morph"). Until now the test was `== Morphable`, which under
       ADR-173's default excluded every STEPPED row of the block — all EIGHT of
       the sub's: wave, octave, semitones, keytrack, seed, mono, bias, and the
       retired `sync` (4011), which is Structural like the rest and joins with
       them because membership is the CLASS and not a list of rows someone
       judged interesting; it reaches nothing either way. The INSTRUMENT
       table's own hand-curated appends above have always included stepped rows
       (the bend and note-travel laws, the FX slot types), where a stepped
       member morphs ATOMICALLY: morphApplyTarget takes the winner's request in
       full and rounds it, so it snaps rather than interpolating through values
       no corner authored. That is the established meaning of a stepped corner
       value, so an engine block that excluded them was an asymmetry, not a
       policy. The rule is now: Morphable and Structural join the field, Device
       stays out. The gate leaves by exactly the reason it left before.

       WHY TWO PASSES OVER THE SAME TABLE, which looks redundant and is not.
       morphIds is APPEND-ONLY: a stored corner array is POSITIONAL, so the
       only safe way to admit new ids is at the tail. Widening the test in one
       pass would interleave the newly-admitted Structural rows with the
       Morphable ones IN BLOCK ORDER (4000 before 4001, …) and every slot after
       the first newly-admitted id would SHIFT — silently re-reading every
       corner ever saved against the wrong parameter. So: Morphable first, in
       exactly the order it had, then Structural after all of them. Do not
       collapse these into one loop. */
    for (const auto &b : kEngineBlocks)
      for (uint32_t i = 0; i < b.count; i++)
      {
        ParamClass cls = ParamClass::Device;
        const char *why = nullptr;
        if (b.defs[i].id == b.gateId) continue;   // B203's third pass — see below
        if (paramClassOf(b.defs[i].id, cls, why) && cls == ParamClass::Morphable)
          morphIds.push_back(b.defs[i].id);
      }
    for (const auto &b : kEngineBlocks)
      for (uint32_t i = 0; i < b.count; i++)
      {
        ParamClass cls = ParamClass::Device;
        const char *why = nullptr;
        if (b.defs[i].id == b.gateId) continue;   // B203's third pass — see below
        if (paramClassOf(b.defs[i].id, cls, why) && cls == ParamClass::Structural)
          morphIds.push_back(b.defs[i].id);
      }
    /* B203 — THE BLOCK'S GATE JOINS THE FIELD (human 2026-09-21: "Sub on/off
       is still exempt from morph and it ought to be wired in the way the other
       two oscs are"). paramClassOf now calls it Structural rather than Device
       and the reasoning lives there; this is the APPEND, and it is a THIRD
       pass for exactly the reason there are already two.

       The gate is Structural, so pass 2 would have taken it — IN BLOCK ORDER,
       which for the sub puts 4015 between 4014 (seed) and 4016 (mono) and
       SHIFTS every slot after it. morphIds is append-only and a stored corner
       array is positional, so that would silently re-read every corner ever
       saved against the wrong parameter. The two `continue`s above are what
       keep passes 1 and 2 producing the exact order they produced before this
       change; the gates land here, after all of them, at the tail.
       morphlayout_check T10b is the positional gate on that claim and T10d
       pins this ruling. */
    for (const auto &b : kEngineBlocks) morphIds.push_back(b.gateId);

    /* THE LEAD MAP. Identity, then the groups.
       FX SLOTS (B49, measured 2026-08-26): type and amount were drawn
       INDEPENDENTLY each grid tick, so a sweep between a Drive corner and a
       Gain corner spent its middle third at type=Drive with amount=0.10 --
       Drive at 0.10 is nearly passthrough, so the drive corner's character
       silently evaporated mid-blend, and the state existed in neither corner
       (3 of 9 sampled positions). `amount` is dimensionally different per type
       (Drive pre-gain, Gain 0.5-is-unity, Comp strength, Comb wet), so pairing
       it with another corner's type is not merely arbitrary, it is
       meaningless. Type + amount + tone now draw ONE corner per slot -- the
       same reasoning that already makes root + twelve scale degrees atomic. */
    morphLead.resize(morphIds.size());
    for (size_t i = 0; i < morphLead.size(); i++) morphLead[i] = (uint32_t)i;
    for (size_t i = scaleFirst; i <= scaleLast; i++) morphLead[i] = (uint32_t)scaleFirst;
    for (int slot = 0; slot < 4; slot++)
    {
      const clap_id ids[3] = {(clap_id)(57 + 2 * slot), (clap_id)(58 + 2 * slot),
                              (clap_id)(96 + slot)};
      size_t lead = morphIds.size();
      for (clap_id want : ids)
        for (size_t i = 0; i < morphIds.size(); i++)
          if (morphIds[i] == want && i < lead) lead = i;
      if (lead >= morphIds.size()) continue;
      for (clap_id want : ids)
        for (size_t i = 0; i < morphIds.size(); i++)
          if (morphIds[i] == want) morphLead[i] = (uint32_t)lead;
    }
    /* THE ROUTING BLOCK IS ONE THING (ADR-125, restated as the ruling in
       ADR-176 §3 after B142 found the shipped default contradicting it).
       Identity leads here meant every crosspoint, out amount, slot init and
       dry-path cell drew its OWN corner under quantum, so the live table was
       assembled from up to four corners at once: a topology no corner
       authored (ADR-124's chimera, now with feedback in front of it), and
       under ADR-175 a mixture of two acyclic tables can carry a CYCLE — which
       flips the whole FX pass to sample-by-sample, a processing mode no corner
       declared. Same mechanism as the scale and the FX slots: one lead index
       for every id `decodeRoutingId` names, so the block flips and exempts as
       a unit. BLEND is untouched by construction — morphStep's blend branch
       never consults the lead map, so cell-wise interpolation survives exactly
       as ADR-088 argued for it (routing_check 11 is that claim's gate). */
    {
      size_t routeLead = morphIds.size();
      int kind = 0, from = 0, to = 0;
      for (size_t i = 0; i < morphIds.size(); i++)
        if (decodeRoutingId(morphIds[i], kind, from, to)) { routeLead = i; break; }
      for (size_t i = routeLead; i < morphIds.size(); i++)
        if (decodeRoutingId(morphIds[i], kind, from, to)) morphLead[i] = (uint32_t)routeLead;
    }

    /* ADR-183: each slot's source gate, resolved to a slot index here so the
       blend never searches morphIds on the audio thread. */
    morphGateSlot.assign(morphIds.size(), -1);
    for (size_t i = 0; i < morphIds.size(); i++)
    {
      const clap_id gate = sourceGateOf(morphIds[i]);
      if (gate == kNoSourceGate) continue;
      for (size_t j = 0; j < morphIds.size(); j++)
        if (morphIds[j] == gate) { morphGateSlot[i] = (int32_t)j; break; }
    }
    morphGateSlotList.clear();
    for (int32_t g : morphGateSlot)
      if (g >= 0 && std::find(morphGateSlotList.begin(), morphGateSlotList.end(), g) ==
                        morphGateSlotList.end())
        morphGateSlotList.push_back(g);
    morphSrcWasOff.assign(morphIds.size(), 0);

    for (int k = 0; k < 4; k++) morphCorner[k].assign(morphIds.size(), 0.0);
    morphCur.assign(morphIds.size(), -1e30);
    morphExempt.assign(morphIds.size(), 0);
    morphGroupSplit.assign(morphIds.size(), 0);   // B222: sized here, never on the audio thread
    morph.reshuffle(morphSeed, (int)morphIds.size());
    // A fresh instance's corners all hold the DEFAULT patch, so switching morph
    // on before capturing anything is silence-safe: every corner agrees.
    for (size_t i = 0; i < morphIds.size(); i++)
    {
      const ParamDef *d = findParam(morphIds[i]);
      const double v = d ? defaultFor(*d, morphIds[i] / 1000) : 0.0;
      for (int k = 0; k < 4; k++) morphCorner[k][i] = v;
    }
    // B89 phase 2b: the resolver's tables, sized HERE and only here — this is
    // the main-thread, once-per-instance site (plug_activate calls morphInit),
    // which is what lets intentStep run on the audio thread allocation-free.
    intentInit();
  }

  /* ADR-109: exempt toggle + query, addressed by parameter id so the GUI needs
     no knowledge of morphIds ordering. Writing the live value into ALL FOUR
     corners on exempt is the recorded design lean: un-exempting is then
     seamless (no jump), and the corners honestly record what was playing. */
  bool morphToggleExempt(clap_id id)
  {
    morphInit();
    for (size_t i = 0; i < morphIds.size(); i++)
      if (morphIds[i] == id)
      {
        const bool on = !morphExempt[i];
        // A group exempts as a unit, for the same reason it flips as one.
        size_t lo, hi;
        morphGroupRange(i, lo, hi);
        for (size_t j = lo; j <= hi; j++)
        {
          if (morphGroupLead(j) != morphGroupLead(i)) continue;   // gaps: FX groups

          morphExempt[j] = on ? 1 : 0;
          if (on)
          {
            const double live = readParam(morphIds[j]);
            for (int k = 0; k < 4; k++) morphCorner[k][j] = live;
            morphCornersAuthored = true;
          }
        }
        return on;
      }
    return false;
  }
  /* ADR-110: which corner owns each parameter RIGHT NOW, for the GUI's colour
     coding. The same pickCorner the audio path uses and the same group lead, so
     the colours cannot disagree with what you hear — the lab's rule ("when they
     were two copies, any edit to one was a map that lied about the sound"),
     applied to a third consumer. Exempt parameters report -1: no corner owns
     them, and the GUI must not tint them as if one did. */
  std::string morphOwnersJson()
  {
    morphInit();
    const bool live = morphOn > 0.5;
    double w[4], lw[4];
    hypersaw::MorphCore::weights(morphX, morphY, w);
    hypersaw::MorphCore::logW(w, morphTemp, lw);
    std::string out = "{";
    char buf[40];
    bool first = true;
    for (size_t i = 0; i < morphIds.size(); i++)
    {
      /* -1 = no corner owns this right now (field off, or exempt). The key's
         PRESENCE is the membership answer, which the menu needs whether or not
         the field is running — so every id is emitted, always. */
      int k = (!live || (i < morphExempt.size() && morphExempt[i]))
                  ? -1
                  : morph.pickCorner((int)morphGroupLead(i), lw, morphCoup);
      /* -2 = HELD (B93, 2026-09-03): a corner won this parameter but its
         enabling condition is false in that corner, so morphStep holds the
         live value instead of applying the corner's (ADR-108). Painting the
         winner's colour here was a lie the human caught as "globals that act
         like corner params" — the knob wore corner B's colour while showing
         a value no corner owned. The GUI paints held distinctly. */
      if (k >= 0 && !depLiveInCorner(morphIds[i], k)) k = -2;
      std::snprintf(buf, sizeof(buf), "%s\"%u\":%d", first ? "" : ",",
                    (unsigned)morphIds[i], k);
      out += buf;
      first = false;
    }
    /* B89 phase 2d — THE INTENT BUS'S TWO GUI FACTS, riding the feed the GUI
       already fetches for ownership colours rather than a third bridge verb.
       Emitted ONLY with the flag on, so with it off this JSON is byte-for-byte
       what it was and the GUI's own paint is part of the bit-identity claim.
       The keys are NON-NUMERIC and cannot collide: every other key here is a
       decimal parameter id, and the consumer looks keys up by id.
         intentHome  [x, y, owningCorner] — the pad's home in CANVAS coords
                     (y down), which is the space §4.4 stores it in and the
                     space the GUI's pad paints in.
         intentNames the ten captions in stored-slot order (kIntentOrder), for
                     the macro labels; X and Y are slots 0 and 1. */
    if (intentBusOn > 0.5 && !intentOwnerAtom.empty())
    {
      const int ho = intentOwnerAtom[(size_t)intentHomeAtom];
      char hb[64];   // `buf` above is sized for one id:owner pair and no more
      std::snprintf(hb, sizeof(hb), ",\"intentHome\":[%.6g,%.6g,%d]", intentHomeX[ho],
                    intentHomeY[ho], ho);
      out += hb;
      out += ",\"intentNames\":[";
      for (int i = 0; i < kIntents; i++)
      {
        out += i ? ",\"" : "\"";
        out += jsonEscape(intentCaption(i));
        out += "\"";
      }
      out += "]";
    }
    return out + "}";
  }

  /* ADR-111: corner k's stored baseline, for the armed view. The GUI paints
     these INSTEAD of live values while a corner is armed — you are looking at
     what you are editing, not at what happens to be sounding. Same id-keyed
     shape as morphOwnersJson so the two consumers share their plumbing. */
  std::string morphCornerValsJson(int k)
  {
    morphInit();
    if (k < 0 || k > 3) return "{}";
    std::string out = "{";
    char buf[48];
    for (size_t i = 0; i < morphIds.size(); i++)
    {
      std::snprintf(buf, sizeof(buf), "%s\"%u\":%.10g", i ? "," : "",
                    (unsigned)morphIds[i], morphCorner[k][i]);
      out += buf;
    }
    return out + "}";
  }

  std::string morphExemptJson()
  {
    morphInit();
    std::string out = "{";
    char buf[32];
    bool first = true;
    for (size_t i = 0; i < morphIds.size(); i++)
      if (morphExempt[i])
      {
        std::snprintf(buf, sizeof(buf), "%s\"%u\":1", first ? "" : ",", (unsigned)morphIds[i]);
        out += buf;
        first = false;
      }
    return out + "}";
  }

  void morphCapture(int k)
  {
    if (k < 0 || k > 3) return;
    morphInit();
    for (size_t i = 0; i < morphIds.size(); i++)
    {
      const uint32_t id = morphIds[i];
      double v = readParam(id);
      /* ADR-152 — CAPTURE FLATTENS (QM-4 §7, brought forward): readParam
         reports BASE, but the sound being captured includes the macro
         family's live offsets — and those sources are suspended once the
         morph runs, so a base-only capture stores a corner that never
         sounds like what was authored. Bake the macro-family contribution
         (slots 2-13, mod.src already reflects any suspension) into the
         stored value, clamped to the dest's own range. Routes to stepped
         params are refused at add, so everything summed here is continuous. */
      if (const ParamDef *pd = findParam(id))
      {
        double macroOfs = 0;
        for (int r = 0; r < mod.nRoutes; r++)
        {
          const auto &q = mod.routes[r];
          if (q.active && q.dest == id && q.src >= 2 && q.src <= 13)
            macroOfs += mod.src[q.src] * q.depth;
        }
        v = std::max(pd->minV, std::min(pd->maxV, v + macroOfs * (pd->maxV - pd->minV)));
      }
      morphCorner[k][i] = v;
    }
    morphCornersAuthored = true;
    undoMarkCorner(k, "captured", nullptr); cornerName[k] = "";   // B84 / B122
  }

  /* One morph step, on the 256-sample gravity grid (heavier than the bend grid
     on purpose — a parameter field does not need 2.7 kHz updates, and the grid
     accumulator makes the result independent of host buffer subdivision, the
     ADR-086 rule). Stepped params take their winning corner's value outright;
     continuous params either flip (quantum) with a one-pole slew toward the
     winner, or blend (mode 1) across all four corners — at engine revision 2,
     across the corners where the parameter's source is ON (ADR-183,
     morphBlendTarget). */
  /* Is `id` live under corner k's stored settings? Linear scan over a table of
     ~55 rules, once per morphed parameter per 256-sample grid tick -- cheap
     enough to not need an index, and an index would be a second structure to
     keep in step with the generated one. */
  bool depLiveInCorner(clap_id id, int k) const
  {
    for (int r = 0; r < hypersaw::kNumDepRules; r++)
    {
      const hypersaw::DepRule &rule = hypersaw::kDepRules[r];
      if (rule.id != id) continue;
      for (int c = 0; c < rule.nConds; c++)
      {
        // find the condition parameter's value in THIS corner
        for (size_t j = 0; j < morphIds.size(); j++)
          if (morphIds[j] == rule.conds[c].id)
          {
            if (std::fabs(morphCorner[k][j] - rule.conds[c].value) < 0.5) return true;
            break;
          }
      }
      return false;   // rule exists and no condition matched
    }
    return true;      // no rule -> always live
  }

  /* ================= B222 — MORPH-ON KEEPS THE PATCH =======================
     The human, 2026-09-23: "turning on morph reverts the routing matrix to its
     initial state, which kills whatever patch you're working on."
     Measured (tools/undo_check.cpp layer 5): after ANY state load — the Init
     patch, a factory preset, a history restore — switching morph on reverted
     EVERY morphable edit made since, the routing matrix and ordinary
     parameters alike. The seed adoption in applyParam(151) is keyed on
     `morphCornersAuthored`, and every load sets that flag because every
     stateJson carries a morph chunk; so "the corners are still the seed" was
     false for any instance that had ever loaded anything, even with four
     identical corners, and the field wrote the load-time values back.

     THE RULE, per morph GROUP (a singleton, or an atomic group: the scale,
     each FX slot's triple, the whole routing block — ADR-176 §3):
       * all four corners hold the SAME values -> the field has no opinion
         about the group (it would pin one value wherever the puck sits), so
         the LIVE values are adopted into all four and morph-on changes
         nothing you can hear. Nothing authored is lost: four equal corners
         carry no morph information;
       * the corners DIFFER -> the group is the field's and plays the owning
         corner's values. The live edit is NOT kept, because keeping it would
         mean writing over a corner the player authored — the destructive
         direction the seed adoption was written to keep closed.
     Exempt members are skipped: they are live-only already.

     THE GRANULARITY IS THE FIELD'S OWN (critic NOTE, PR #732). A group is
     only a unit where the field treats it as one: quantum picks ONE corner
     per group lead, and so does blend for a STEPPED slot. Blend computes a
     CONTINUOUS slot on its own — each cell is its own weighted sum, the lead
     map is never consulted (morphStep) — so there a slot whose four corners
     agree adopts the live value even when a sibling in its group disagrees.
     Applying the group rule in blend would throw away a live routing cell for
     a disagreement in a DIFFERENT cell, which the field itself never couples.
     (Quantum keeps the group rule for the reason the group exists — ADR-176
     §3's ruling against ADR-124's chimera: under quantum what you hear is ONE
     corner's whole routing table, so a cell adopted into every corner while a
     sibling cell's corners differ would make each corner's table a mix of
     that corner's cells and the live one — a topology no corner was authored
     with. It is NOT a cycle guard today: the routing ids expose forward cells
     only, and ADR-175 calls the cycle hazard "latent while only acyclic cells
     are exposed". It becomes one if the B139 feedback cells are exposed.) The
     mode is read at the toggle; a later quantum<->blend switch re-reads the
     corners as they are.

     "SAME" MEANS SAME AT THE SAVED PRECISION (critic B1, PR #732). The host
     chunk and every preset write corners %.6g; a corner captured live keeps
     full precision. After an ordinary save and reopen, a cell nobody morphed
     can hold 0.123457 in one corner and 0.123456789 in another, and `!=`
     called that a morph and handed the whole routing block to the field —
     the human's defect again, through a session reload. So two corner values
     agree when |a - b| <= 5e-6 * max(|a|, |b|): %.6g keeps six significant
     digits, so rounding moves a value by at most half a unit in the sixth,
     which is at most 5e-6 of its magnitude, at every magnitude. A tolerance
     rather than comparing %.6g TEXT because this runs on the audio thread
     and snprintf does not belong there; relative rather than the absolute
     5e-7*max(1,|a|) the review suggested because that bound is too tight
     above 1 (1234.5678 saves as 1234.57, off by 2.2e-3 > 6.2e-4). Values
     closer than that are indistinguishable after any save anyway.
     EXCEPT A STEPPED SLOT, which compares EXACTLY (critic re-review, PR
     #732): its values are integers, %.6g stores every one up to 999999
     exactly, so two different stepped corner values were AUTHORED — the
     oscillator Seed (0..999999) holding 999996 and 999999 is two seeds, not
     one seed rounded, and the tolerance called them equal and wrote the live
     seed over both. The SUB seed (4014, up to ~4.29e9) exceeds what %.6g
     stores exactly, so after a reload its corners can differ by rounding
     alone; exact comparison then calls them split and hands the slot to the
     field — the SAFE direction (the corners keep what they hold; only the live
     edit to that one slot is not adopted), never an overwrite.
     Each corner is compared with corner 0 only; under the tolerance that makes
     "agree" hold pairwise within twice the bound, which is still far inside
     what any save can distinguish.

     REJECTED: clearing `morphCornersAuthored` on loads whose corners agree. It
     would reopen the seed adoption DURING a load, where 151 lands in the
     middle of the queued burst and would adopt half-loaded values.
     Audio thread: compares and writes into vectors morphInit pre-sized. */
  static bool cornerValuesAgree(double a, double b, bool stepped)
  {
    if (stepped) return a == b;
    return std::fabs(a - b) <= 5e-6 * std::max(std::fabs(a), std::fabs(b));
  }
  bool cornersAgreeAt(size_t i) const
  {
    const ParamDef *d = findParam(morphIds[i]);
    const bool stepped = d && d->stepped;
    const double v = morphCorner[0][i];
    return cornerValuesAgree(v, morphCorner[1][i], stepped) &&
           cornerValuesAgree(v, morphCorner[2][i], stepped) &&
           cornerValuesAgree(v, morphCorner[3][i], stepped);
  }
  void morphAdoptUncontested()
  {
    const size_t n = morphIds.size();
    if (morphGroupSplit.size() != n) return;   // morphInit sizes it; never allocate here
    std::fill(morphGroupSplit.begin(), morphGroupSplit.end(), 0);
    for (size_t i = 0; i < n; i++)
      if (!cornersAgreeAt(i)) morphGroupSplit[morphGroupLead(i)] = 1;
    const bool blend = (int)morphMode == 1;
    for (size_t i = 0; i < n; i++)
    {
      if (i < morphExempt.size() && morphExempt[i]) continue;    // live-only already
      const ParamDef *d = findParam(morphIds[i]);
      const bool perSlot = blend && d && !d->stepped;             // the field's own unit
      if (perSlot ? !cornersAgreeAt(i) : morphGroupSplit[morphGroupLead(i)] != 0)
        continue;                                                  // the field's
      const double live = readParam(morphIds[i]);
      for (int k = 0; k < 4; k++) morphCorner[k][i] = live;
    }
  }

  /* ADR-109 — where does an edit LAND? The human's model, implemented:
       armed (1..4)  -> that corner's baseline, and only that corner's.
       none armed    -> the corner that OWNS this parameter right now, so the
                        edit sticks instead of being overwritten at the next
                        grid tick (the v1 seam this closes).
     Returns true when the caller should ALSO apply the value live. Armed edits
     do not: you are editing a baseline that may not be the one sounding, and
     forcing it live would lie about which corner you just changed. */
  bool morphRouteEdit(clap_id id, double v)
  {
    if (morphOn <= 0.5 || loadingState) return true;   // B125: a load is not an edit
    size_t idx = morphIds.size();
    for (size_t i = 0; i < morphIds.size(); i++)
      if (morphIds[i] == id) { idx = i; break; }
    if (idx == morphIds.size()) return true;          // not morphed: normal edit
    if (idx < morphExempt.size() && morphExempt[idx]) return true;   // exempt: live only

    const int armed = (int)morphArm;
    if (armed >= 1 && armed <= 4)
    {
      morphCorner[armed - 1][idx] = v;
      morphCornersAuthored = true;
      return false;
    }
    double w[4], lw[4];
    hypersaw::MorphCore::weights(morphX, morphY, w);
    hypersaw::MorphCore::logW(w, morphTemp, lw);
    const ParamDef *d = findParam(id);
    if ((int)morphMode == 1 && d && !d->stepped)
    {
      /* BLEND MODE, continuous parameter, position mid-path — the human's own
         open question: "maybe it edits both in such a way that their average
         arrives at that point along the morph path?" Yes, and weighted: the
         delta is distributed across corners in proportion to their weight, so
         sum(w[k] * corner[k]) lands exactly on the edited value while the
         corners keep their relative identities. Distributing EVENLY would move
         a corner you are barely touching as much as the one under your cursor;
         proportional is the reading that respects where you are standing.
         ADR-183: "their average" is whatever average the forward blend takes,
         so the weights are morphLiveWeights' — at revision 2 an edit where
         the source is off in a weighted corner moves only the ON corners
         (an OFF corner's weight is 0), which is what makes it stick. */
      double e[4];
      const double *u = morphLiveWeights(idx, w, engineRevision() >= 2, e) ? e : w;
      double cur = 0, sumsq = 0;
      for (int k = 0; k < 4; k++) { cur += u[k] * morphCorner[k][idx]; sumsq += u[k] * u[k]; }
      if (sumsq > 1e-12)
      {
        const double scale = (v - cur) / sumsq;
        for (int k = 0; k < 4; k++) morphCorner[k][idx] += u[k] * scale;
      }
      return true;
    }
    // quantum: the corner that won this parameter owns the edit
    const int k = morph.pickCorner((int)morphGroupLead(idx), lw, morphCoup);
    morphCorner[k][idx] = v;
    return true;
  }

  /* B69 increment 2 — the matrix's control tick, on the same gravity grid as
     morphStep (the ADR-086 rule: grid accumulation makes the result
     independent of host buffer subdivision). ENV 1's global projection is the
     LOUDEST voice's envelope across both oscillators — the honest global
     reduction of a per-voice quantity, stated so nobody mistakes it for a
     per-note fan-out (that is a later increment, and the scope field on the
     route is already there for it). Inert by construction at depth 0: the
     route only exists once the knob has moved, evaluate() of an empty table
     is zero entries, and modPitchSm settles to exactly 0. */
  /* ADR-136 route management, called from the GUI bridge (main thread).
     Stepped destinations are refused — a zippered enum is not modulation, and
     the GUI mirrors the rule by not offering the menu item. Source is a slot
     index (0 = ENV 1, 1 = ENV 2). */
  bool modAddRoute(uint32_t srcSlot, clap_id destId)
  {
    const ParamDef *pd = findParam(destId);
    if (!pd || pd->stepped) return false;
    // No self-reference: the matrix's own controls (161-165) and the macros +
    // XY assignment (166-177, ADR-137). Macro-as-dest is fan-out — B70's
    // territory, refused until its cycle rule is ruled.
    if (destId >= 161 && destId <= 177) return false;
    /* B171, the same refusal one block along: the LFO and ENV 3/4 controls
       (269-288) are SOURCES, not destinations. "LFO modulates LFO" is a real
       feature and a later one — it needs the cycle rule B70 owes for
       macro-as-destination, and a matrix that can feed a modulator its own
       output without that rule is a matrix that can deadlock or run away.
       modDestOptions() in gui2.html mirrors this exclusion. */
    if (destId >= 269 && destId <= 288) return false;
    return mod.addRoute(srcSlot, destId, 0.25, hypersaw::ModCore::kGlobal);
  }
  /* ADR-141: re-aim a live route's SOURCE. The human's ruling moved the
     modulator choice out of the right-click menu and into the table, so this
     is the table's verb. The pitch route is refused: its source is ADR-135's
     contract (knob 161 IS that route's depth, ENV 2 IS its source), not a
     user choice. */
  bool modSetSource(int idx, uint32_t srcSlot)
  {
    if (idx < 0 || idx >= mod.nRoutes) return false;
    if (srcSlot >= (uint32_t)hypersaw::ModCore::kMaxSources) return false;
    if (mod.routes[idx].dest & kModDestSynthetic) return false;
    mod.routes[idx].src = srcSlot;
    return true;
  }
  /* B134: the route's polarity, the table's second verb. The pitch route is
     refused for the same reason modSetSource refuses it — knob 161 owns that
     route's whole meaning (ADR-135), and a polarity on it would be a second,
     invisible author of the semitone offset. */
  bool modSetPolarity(int idx, int pol)
  {
    if (idx < 0 || idx >= mod.nRoutes) return false;
    if (pol < hypersaw::ModCore::kAsIs || pol > hypersaw::ModCore::kInverted) return false;
    if (mod.routes[idx].dest & kModDestSynthetic) return false;
    mod.routes[idx].polarity = pol;
    return true;
  }
  /* The GUI's view of the table. `srcPol` rides each ROUTE rather than arriving
     as a second top-level array, so this stays a JSON ARRAY — every consumer
     (renderModRoutes, the halo pass, refreshModAdd's "n routed" tags) treats
     the parse as a list, and an object wrapper would be a shape change for
     four of them to pay for one. The GUI never guesses a source's polarity:
     the halo's reach depends on it, and a second copy of the table in JS is
     the duplicated-key-chain failure L0005 records. */
  std::string modRoutesJson()
  {
    std::string out = "[";
    char buf[160];
    for (int r = 0; r < mod.nRoutes; r++)
    {
      const auto &q = mod.routes[r];
      std::snprintf(buf, sizeof buf,
                    "%s{\"i\":%d,\"src\":%u,\"dest\":%u,\"depth\":%.6g,\"pol\":%d,\"srcPol\":%d}",
                    r ? "," : "", r, q.src, q.dest, q.depth, q.polarity,
                    q.src < (uint32_t)hypersaw::ModCore::kMaxSources ? mod.srcPol[q.src] : 0);
      out += buf;
    }
    return out + "]";
  }
  /* ADR-137: the live picture for the GUI's mod halos — base and the value the
     matrix last applied, per active destination. The GUI computes reach from
     the routes it already has; this reports only what it cannot know. */
  std::string modLiveJson() const
  {
    std::string out = "[";
    char buf[96];
    bool first = true;
    for (const auto &md : modDests)
    {
      if (!md.active) continue;
      // 1e300 is the "never applied yet" sentinel — a poll can land in the
      // sub-tick window between route-add and the first evaluate.
      const double now = md.lastApplied > 1e299 ? md.base : md.lastApplied;
      std::snprintf(buf, sizeof buf, "%s{\"id\":%u,\"base\":%.6g,\"now\":%.6g}",
                    first ? "" : ",", md.id, md.base, now);
      out += buf;
      first = false;
    }
    return out + "]";
  }
  int modPitchRouteIdx() const
  {
    for (int r = 0; r < mod.nRoutes; r++)
      if (mod.routes[r].dest == kModDestPitch) return r;
    return -1;
  }
  /* ADR-138: route persistence, keyed on B72's deterministic link identity.
     One line, generic routes only: `src:dest:depth[:pol];…`. The pitch route is
     param 161's and persists as that param — writing it here too would double
     it on load.

     B134 RETIRED THE (src, dest) MERGE. ADR-138 canonicalised by summing the
     depths of duplicate (src, dest) pairs, which the SUM law made
     indistinguishable from the un-merged form. Polarity breaks that identity:
     ENV1→detune at +0.5 bipolar and ENV1→detune at +0.5 as-is do not sum to
     one entry of any depth, so merging would silently discard one route's
     setting. One entry per ROUTE now — the loader already created one route
     per entry, so nothing on the read side changes, and B72 can still key on
     (src, dest) when it gets there.

     POLARITY IS OMITTED WHEN 0, deliberately: a patch with no polarity set
     serialises to exactly the bytes ADR-138 wrote, so every stored chunk,
     preset and fixture golden stays byte-identical instead of gaining a `:0`
     that would make "bit-inert" a claim about behaviour only. */
  /* ---- ADR-088 `routing` CHUNK (B50 (c)) ----------------------------------
     SPARSE BY DESIGN, and that is what keeps B50 (b) true. Only cells that
     DIFFER from the series default are emitted, so a patch nobody has rerouted
     produces an empty chunk, the key is omitted entirely, and the saved bytes
     of every existing preset and fixture are what they were. The precedent is
     ADR-138's `modroutes`, for the same reason.

     Keyed on the numeric id, not on the coreKey string: the id IS the cell's
     coordinates (see the id-layout comment), so a chunk stays readable across a
     matrix that grows, while a key list would have to be kept in step by hand.
     An id this build does not expose is skipped on load, which is how a patch
     saved by a wider future build stays loadable here. */
  std::string routingChunk() const
  {
    std::string out;
    char buf[64];
    for (const auto &d : g_routingTable.defs)
    {
      const double v = getRoutingParam(d.id);
      if (v == d.defV) continue;
      std::snprintf(buf, sizeof(buf), "%s%u:%.17g", out.empty() ? "" : ",", (unsigned)d.id, v);
      out += buf;
    }
    return out;
  }
  /* A load is a load: EVERY cell returns to its default first, so a patch
     without the key loads the series chain rather than inheriting whatever the
     previous patch was routed to. Written through applyParam so the presence
     bits, the morph hooks and the mod base all see the load exactly as they see
     any other write — one write path, no second one to drift. */
  void applyRoutingChunk(const std::string &chunk)
  {
    routingChunkCells(chunk, [&](clap_id id, double v) { applyParam(id, v); });
  }
  /* The chunk's meaning, once: every cell at its default, then the cells the
     chunk names. Two writers consume it — the host chunk applies directly
     (above), a history restore queues (applyStateJson, B222) — so the parse
     is shared and only the WRITE differs. */
  template <class Set> void routingChunkCells(const std::string &chunk, Set &&set) const
  {
    for (const auto &d : g_routingTable.defs) set(d.id, d.defV);
    size_t pos = 0;
    while (pos < chunk.size())
    {
      const size_t comma = chunk.find(',', pos);
      const std::string tok = chunk.substr(pos, comma == std::string::npos ? std::string::npos
                                                                           : comma - pos);
      pos = comma == std::string::npos ? chunk.size() : comma + 1;
      const size_t colon = tok.find(':');
      if (colon == std::string::npos) continue;
      const clap_id id = (clap_id)std::strtoul(tok.c_str(), nullptr, 10);
      if (!findRoutingParam(id)) continue;   // a cell this build does not expose
      set(id, std::atof(tok.c_str() + colon + 1));
    }
  }

  /* B149: the ADR-077/078 ensemble-timing state of oscillator `k`, as one
     line of the state chunk. EMITTED ONLY when the stream has left its seeded
     initial state — i.e. only for a patch that has actually played notes with
     onset scatter or per-voice envelopes on — so every other patch's bytes are
     exactly what they were before this key existed, and statefix_check /
     bank_check / state_check remain the regression proof rather than three
     fixtures to regenerate.

     Carries the seed it was derived under (see SwarmCore::setEnsembleTiming):
     that is what makes the restore independent of whether state_load applied
     the parameters before this key (idle) or queues them for after it
     (processing). %.17g throughout — a round-trip that loses a bit is a
     continuation that is no longer bit-identical, which is the whole point. */
  std::string ensembleChunk(uint32_t k) const
  {
    if (k >= kNumOsc || cores[k].ensembleIsInitial()) return {};
    const auto e = cores[k].ensembleTiming();
    char buf[48];
    std::snprintf(buf, sizeof buf, "%.17g;%u;", e.seed, (unsigned)e.rng);
    std::string out = buf;
    for (int i = 0; i < hypersaw::kMaxV; i++)
    {
      std::snprintf(buf, sizeof buf, "%s%.17g", i ? "," : "", e.off[i]);
      out += buf;
    }
    return out;
  }
  /* A malformed or short line leaves the missing offsets at 0 rather than
     refusing the load: the chunk is append-only and a future build may write
     more of them, and a patch that half-loads its timing history is still a
     patch that loads. */
  void applyEnsembleChunk(uint32_t k, const std::string &chunk)
  {
    if (k >= kNumOsc) return;
    const size_t s1 = chunk.find(';');
    if (s1 == std::string::npos) return;
    const size_t s2 = chunk.find(';', s1 + 1);
    if (s2 == std::string::npos) return;
    hypersaw::SwarmCore::EnsembleTiming e{};
    e.seed = std::atof(chunk.c_str());
    e.rng = (uint32_t)std::strtoul(chunk.c_str() + s1 + 1, nullptr, 10);
    size_t pos = s2 + 1;
    for (int i = 0; i < hypersaw::kMaxV && pos <= chunk.size(); i++)
    {
      const size_t comma = chunk.find(',', pos);
      e.off[i] = std::atof(chunk.c_str() + pos);
      if (comma == std::string::npos) break;
      pos = comma + 1;
    }
    cores[k].setEnsembleTiming(e);
  }

  /* B171 — the LFOs' stream state, one line of the chunk, B149's rule applied
     verbatim: EMITTED ONLY once a S&H stream has actually DRAWN. Every patch
     that has never run an S&H LFO therefore writes exactly the bytes it wrote
     before this key existed, which is what keeps state_check / statefix_check /
     bank_check the regression proof for this change rather than three fixtures
     to regenerate. Carries `phase;rng` per LFO, %.17g — a round trip that loses
     a bit is a continuation that is no longer bit-identical. */
  std::string lfoChunk() const
  {
    bool any = false;
    for (const auto &L : lfo) any = any || L.drawn;
    if (!any) return {};
    std::string out;
    char buf[64];
    for (int i = 0; i < kNumLfo; i++)
    {
      std::snprintf(buf, sizeof buf, "%s%.17g;%u", i ? "," : "", lfo[i].phase,
                    (unsigned)lfo[i].rng);
      out += buf;
    }
    return out;
  }
  /* A short or malformed line leaves the remaining LFOs at their seeded state
     rather than refusing the load — the chunk is append-only and a future build
     may write more lanes; a patch that half-loads its LFO phases is still a
     patch that loads. */
  void applyLfoChunk(const std::string &chunk)
  {
    size_t pos = 0;
    for (int i = 0; i < kNumLfo && pos < chunk.size(); i++)
    {
      const size_t semi = chunk.find(';', pos);
      if (semi == std::string::npos) return;
      lfo[i].phase = std::atof(chunk.c_str() + pos);
      lfo[i].rng = (uint32_t)std::strtoul(chunk.c_str() + semi + 1, nullptr, 10);
      lfo[i].drawn = true;
      lfo[i].restored = true;
      const size_t comma = chunk.find(',', semi + 1);
      if (comma == std::string::npos) return;
      pos = comma + 1;
    }
  }
  bool lfoRestoredPending() const
  {
    for (const auto &L : lfo) if (L.restored) return true;
    return false;
  }
  void lfoClearRestored() { for (auto &L : lfo) L.restored = false; }

  // `lossless`: see historyJson (B222) — the persisted chunk stays %.6g.
  std::string modRoutesChunk(bool lossless = false) const
  {
    std::string out;
    char buf[80];
    for (int r = 0; r < mod.nRoutes; r++)
    {
      const auto &q = mod.routes[r];
      if (q.dest & kModDestSynthetic) continue;
      if (q.polarity == hypersaw::ModCore::kAsIs)
        std::snprintf(buf, sizeof buf, lossless ? "%u:%u:%.17g;" : "%u:%u:%.6g;", q.src, q.dest,
                      q.depth);
      else
        std::snprintf(buf, sizeof buf, lossless ? "%u:%u:%.17g:%d;" : "%u:%u:%.6g:%d;", q.src,
                      q.dest, q.depth, q.polarity);
      out += buf;
    }
    return out;
  }
  void applyModRoutesChunk(const std::string &chunk)
  {
    // Existing generic routes are replaced wholesale (a load is a load); the
    // pitch route, if present, is untouched — it belongs to param 161.
    for (int r = mod.nRoutes - 1; r >= 0; r--)
      if (!(mod.routes[r].dest & kModDestSynthetic)) mod.removeRoute(r);
    size_t pos = 0;
    while (pos < chunk.size())
    {
      const size_t semi = chunk.find(';', pos);
      const std::string ent = chunk.substr(pos, semi == std::string::npos ? std::string::npos
                                                                          : semi - pos);
      pos = semi == std::string::npos ? chunk.size() : semi + 1;
      unsigned src = 0, dest = 0;
      double depth = 0;
      int pol = hypersaw::ModCore::kAsIs;   // B134: an ABSENT fourth field is as-is,
      // which is what makes every pre-B134 chunk load to exactly what it meant
      const int got = std::sscanf(ent.c_str(), "%u:%u:%lf:%d", &src, &dest, &depth, &pol);
      if (got < 3) continue;
      if (got < 4 || pol < hypersaw::ModCore::kAsIs || pol > hypersaw::ModCore::kInverted)
        pol = hypersaw::ModCore::kAsIs;     // a garbage field degrades, never poisons
      // Through the shipped refusal path — a chunk naming a stepped dest, the
      // matrix's own controls, or a bad source is dropped, never trusted.
      if (!modAddRoute(src, dest)) continue;
      mod.routes[mod.nRoutes - 1].depth = std::max(-1.0, std::min(1.0, depth));
      mod.routes[mod.nRoutes - 1].polarity = pol;
    }
  }
  int modAccum = 0;
  void modStep(int samples)
  {
    modAccum += samples;
    const int grid = (int)std::lround(sampleRate * hypersaw::kGravGridSeconds);
    if (modAccum < grid) return;
    const double dt = (double)modAccum / sampleRate;
    modAccum = 0;
    double envMax = 0;
    // ENV 1 (slot 0) stays the max amp envelope over every voice. The `anyGate`
    // companion retired with the SHARED ENV 2 (ADR-162) — the gate is a
    // property of a slot now, and slotGated() asks per slot.
    for (uint32_t k = 0; k < kNumOsc; k++)
      if (oscEnabled[k])
        for (int i = 0; i < (int)hypersaw::kPoly; i++)
        {
          const double e = cores[k].voiceAt(i).env;
          if (e > envMax) envMax = e;
        }
    mod.src[0] = envMax;
    /* ADR-135/162: ENV 2, one envelope PER NOTE SLOT. Stage machine per slot
       (strike -> attack from the current level, gate held -> decay to sustain,
       gate lost -> release), one-pole approaches. Time constants are the
       knobs' SECONDS converted per tick (ADR-009's rule — never hand-tuned
       per-tick constants). Two outputs, deliberately different:
         - PER VOICE, route 0 only: depth * penv[s] semitones, handed to the
           note-expression composer. This is the whole point — a strike shapes
           the note struck and leaves the held notes alone.
         - GLOBAL, every other route: the max over ALL slots, release tails
           included (mod.src[1]).
       Bit-identity at depth 0 is structural, not incidental: with the pitch
       knob at 0 `noteExprSetPenv` is handed exactly 0.0, the composer's sum is
       unchanged, and it never reaches a core. */
    {
      const int pr = modPitchRouteIdx();
      const double penvDepth = pr >= 0 ? mod.routes[pr].depth : 0.0;
      double gatedMax = 0;
      int gatedStage = -1;
      double xenvMax[kExtraEnvs] = {0, 0};   // B171: ENV 3/4's global projections
      /* B171: did ANY slot strike this tick? The LFOs' retrig mode reads this
         rather than a flag of its own set at the three note-on sites — one
         signal, distributed from one place (L0029), and it cannot drift out of
         step with the envelopes that share it. */
      bool anyStrike = false;
      for (int s = 0; s < (int)hypersaw::kPoly; s++)
      {
        PitchEnv &pe = penv[s];
        const bool gated = slotGated(s);
        /* B171: the retrig flag is consumed ONCE here and handed to all three
           envelopes — ENV 2 below, ENV 3/4 in the xenv loop that follows — so
           a strike restarts every envelope that slot owns. Clearing it before
           the xenv loop instead would have given ENV 3/4 a flag permanently
           false, which is the silent half of this kind of bug. */
        const bool strike = pe.retrig;
        pe.retrig = false;
        anyStrike = anyStrike || strike;
        advanceAdsr(pe, gated, strike, dt, env2A, env2D, env2S, env2R);
        for (int x = 0; x < kExtraEnvs; x++)
        {
          advanceAdsr(xenv[x][s], gated, strike, dt, xenvA[x], xenvD[x], xenvS[x], xenvR[x]);
          // ENV 3/4 project into the matrix the SAME way ENV 2's global lane
          // does — max over every slot, releasing tails included.
          if (xenv[x][s].level > xenvMax[x]) xenvMax[x] = xenv[x][s].level;
        }
        /* Lead ruling 2026-09-13 on the stream's open question: the global
           source counts EVERY slot, releasing ones included, so at last-key-up
           it releases over env2R exactly as the shared envelope did instead of
           snapping to 0 — "behaves as before" is the criterion's reason and it
           outranks its "gated" wording. */
        if (pe.level > gatedMax) { gatedMax = pe.level; gatedStage = pe.stage; }
        // OQ-30 bounding at APPLICATION, the same rule the global lane obeys.
        const double semis = std::max(-48.0, std::min(48.0, penvDepth * pe.level));
        noteExprSetPenv(s, semis);
      }
      env2 = gatedMax;
      env2Stage = gatedStage;
      mod.src[1] = env2;
      mod.src[20] = xenvMax[0];   // B171 ENV 3
      mod.src[21] = xenvMax[1];   // B171 ENV 4
      lfoStruck = anyStrike;
    }
    /* B171 — the two LFOs, slots 18 and 19. Phase advances by frequency * the
       MEASURED tick span (ADR-009: seconds in, per-tick out, never a hand-tuned
       constant), so the same patch at 44.1/48/96 kHz completes a cycle in the
       same number of SECONDS. Tempo mode divides the host's bpm by the beats
       knob, the delay's own law (`seconds = beats * 60/bpm`) read as a rate.
       S&H draws ONE new value per wrap from this LFO's seeded stream — a draw
       inside the tick loop would make the value depend on block size. */
    for (int i = 0; i < kNumLfo; i++)
    {
      Lfo &L = lfo[i];
      /* Retrig rewinds to the START PHASE knob, then this tick advances from
         there — the note is treated as having landed at the tick boundary,
         which is the only phase the control grid can represent. Free-running
         mode never rewinds: it takes its start phase once, at activate(). */
      if (lfoRetrig[i] != 0 && lfoStruck) L.phase = lfoPhase0[i];
      const double bpm = core.p.bpm > 1 ? core.p.bpm : 120.0;
      const double freq = lfoSync[i] != 0 ? (bpm / 60.0) / std::max(0.01, lfoBeats[i])
                                          : lfoRate[i];
      L.phase += freq * dt;
      if (L.phase >= 1.0)
      {
        // One draw per WRAP, however many cycles a long tick crossed: a tick
        // that spanned three cycles of a 40 Hz S&H still yields one value, and
        // that is honest — the source is read once per tick either way.
        L.phase -= std::floor(L.phase);
        L.sh = 2.0 * forcecore::rngNext(L.rng) - 1.0;
        L.drawn = true;
      }
      mod.src[18 + i] = lfoShapeAt(lfoShape[i], L.phase, L.sh);
    }
    // ADR-137: macros feed source slots 2-9 every tick. A macro with no route
    // is inert by the matrix's own law — no route, no evaluate output.
    /* ADR-152: while the morph is ON the whole macro FAMILY (macros 2-9 and
       the pad aliases 10-13) is suspended — sources read 0, so their routes
       contribute nothing and every dest releases to its base, which the morph
       field owns (base follows morph writes, the ADR-136 intercept). Without
       this the global pad/macro position is an invisible fifth author of
       every corner (QM-4 P1): corners whose identity lives in K/detune were
       flattened to wherever the pad happened to rest. Performance sources
       (ENV 1/2, velocity, wheel, pressure, pitch wheel) stay live — they are
       gestures, not layout. */
    const double macroLive = morphOn > 0.5 ? 0.0 : 1.0;
    for (int i = 0; i < 8; i++) mod.src[2 + i] = macroVal[i] * macroLive;
    /* Pad AXES as first-class sources (human 2026-08-29: "make X and Y for
       each separate XY grid accessible from the mod matrix"). Slots 10-13 =
       XY1 X, XY1 Y, XY2 X, XY2 Y — each an ALIAS through the assignment, so
       routing "XY1 X" means "whatever the pad's X drives", and re-aiming the
       pad re-aims every route riding it. The full nested system is STRATA
       (B77); this is the interim the human asked for. */
    /* RETIRED (ADR-156): slots 10-13 aliased the OSC pads' macros; those pads
       now write detune/K directly, so the alias has nothing to read. The
       slots stay reserved (route tables and MOD_SRC_NAMES index by slot) and
       read 0 — a saved route on them goes inert rather than mis-aiming.
       xyAsn (174-177) stays stored for session compatibility. */
    for (int i = 0; i < 4; i++) mod.src[10 + i] = 0.0;
    // ADR-149: MIDI/MPE performance signals, slots 14-17 (velocity, mod
    // wheel, pressure, pitch wheel). Global projections; B82 owns per-note.
    mod.src[14] = srcVel;
    mod.src[15] = srcWheel;
    mod.src[16] = srcPress;
    mod.src[17] = srcPitchW;
    uint32_t dests[hypersaw::ModCore::kMaxRoutes];
    double deltas[hypersaw::ModCore::kMaxRoutes];
    const int n = mod.evaluate(hypersaw::ModCore::kGlobal, dests, deltas, hypersaw::ModCore::kMaxRoutes);
    double pitch = 0;
    for (int i = 0; i < n; i++)
    {
      /* ADR-162: the pitch route is applied PER VOICE above (depth * that
         slot's own ENV 2, through the note-expression composer), so it no
         longer feeds the global lane — a shared offset is exactly what made a
         strike blip every held note. The lane itself stays: it is the generic
         "global semitone offset" seam, and with no contributor it settles to
         exactly 0 and releases updateTuneAll. */
      if (dests[i] == kModDestPitch) continue;
      if (dests[i] & kModDestSynthetic) continue;      // unknown synthetic: inert
      /* Generic param destination (ADR-136). depth*src is normalized; scale by
         the param's own range and clamp to its bounds — OQ-30's rule applied
         at the ruled place. Stepped params are refused at route-add, so
         everything arriving here is continuous. */
      const ParamDef *pd = findParam(dests[i]);
      if (!pd) continue;
      ModDest *md = modDestFor(dests[i], true);
      if (!md) continue;
      const double span = pd->maxV - pd->minV;
      double want = md->base + deltas[i] * span;
      want = std::max(pd->minV, std::min(pd->maxV, want));
      if (std::fabs(want - md->lastApplied) > 1e-9)
      {
        md->lastApplied = want;
        modFromMatrix = true;
        applyParam(dests[i], want);
        modFromMatrix = false;
      }
    }
    // A destination whose routes have all been removed releases back to base.
    for (auto &d2 : modDests)
    {
      if (!d2.active) continue;
      bool still = false;
      for (int r = 0; r < mod.nRoutes; r++)
        if (mod.routes[r].dest == d2.id) { still = true; break; }
      if (!still)
      {
        modFromMatrix = true;
        applyParam(d2.id, d2.base);
        modFromMatrix = false;
        d2.active = false;
      }
    }
    // OQ-30 bounding at APPLICATION, the ruled place: the route can ask for
    // anything; the destination clamps to its own declared range.
    pitch = std::max(-48.0, std::min(48.0, pitch));
    modPitchSt = pitch;
    const double c = 1.0 - std::exp(-dt / 0.008);
    modPitchSm += (modPitchSt - modPitchSm) * c;
    if (std::fabs(modPitchSm - modPitchSt) < 1e-6) modPitchSm = modPitchSt;
    static_assert(true, "");
    if (std::fabs(modPitchSm - modPitchApplied) > 1e-5)
    {
      modPitchApplied = modPitchSm;
      updateTuneAll();
    }
  }
  double modPitchApplied = 0;

  /* ---- ONE APPLICATION, TWO LAWS (B89 phase 2c) --------------------------
     Choosing the target and APPLYING it are different jobs, and since 2c there
     are two laws that choose (morphStep's Gumbel field and intentStep's
     SPEC-INTENT-BUS walk) and exactly one that applies. These three helpers
     are that one, extracted verbatim from morphStep rather than copied into
     the resolver: a second copy is the failure this codebase has already paid
     for (ADR-110 -- "when they were two copies, any edit to one was a map that
     lied about the sound"), and it would make B89's "no second write path"
     claim a promise about a copy instead of a property of the code. */

  /* THE WRITE. The 1e-9 deadband and the morphFromField guard are the field's
     contract with applyParam's ADR-109 choke point; nothing else in the shell
     may write a morphed slot. Returns true when it actually wrote, which is
     what lets a calibration door report its own anchor (L0033). */
  bool morphCommitSlot(size_t i, double next)
  {
    if (std::fabs(next - morphCur[i]) <= 1e-9) return false;
    morphCur[i] = next;
    morphFromField = true;
    applyParam(morphIds[i], next);
    morphFromField = false;
    return true;
  }

  /* TARGET -> VALUE. Stepped/structural takes the winner's request IN FULL
     (ADR-125); continuous is carried by the one-pole (`morphGlide`, id 158 --
     plan R16 keeps it for the resolver too, so the bus sets the destination
     and the shipped rate control still owns the journey). `morphCur < -1e29`
     is "never applied yet": the first tick lands on the target outright rather
     than gliding up from the sentinel. */
  bool morphApplyTarget(size_t i, const ParamDef &d, double target, double coef)
  {
    double next = d.stepped ? target
                            : (morphCur[i] < -1e29 ? target
                                                   : morphCur[i] + (target - morphCur[i]) * coef);
    if (d.stepped) next = std::round(next);
    return morphCommitSlot(i, next);
  }

  /* B203: THE ONE PLACE A BLOCK'S GATE IS MAPPED TO ITS RAMP. An engine's
     ramp weight is consumed by that engine's OWN renderer (renderSubSpan), so
     the weight is a member of the engine's state and this is the mapping;
     STATION's gate adds one line here and nothing else. An array indexed by
     block would buy nothing while the consumer is per-engine code anyway. */
  void setEngineGateRamp(clap_id id, double w)
  {
    if (id == kSubOscOnId) subOnW = w;
  }
  static bool isEngineGateId(clap_id id)
  {
    const EngineBlock *b = engineBlockOf(id);
    return b && b->gateId == id;
  }

  // B48/B203: an exempt enable is fully live, so its ramp must not linger.
  void morphExemptSlot(size_t i)
  {
    // THE GATE TEST FIRST, and not because of aliasing (4015 % 1000 is 15, not
    // 150) but because an engine id must never reach `baseIdOf` at all — the
    // rule paramClassOf states two screens up, kept true here too.
    if (isEngineGateId(morphIds[i])) { setEngineGateRamp(morphIds[i], 1.0); return; }
    if (baseIdOf(morphIds[i]) != 150) return;
    const uint32_t o = oscOfId(morphIds[i]);
    if (o < kMaxOsc) oscOnW[o] = 1.0;
  }

  /* B48 SPECIAL CASE -- osc on/off morphs as a LEVEL RAMP, not a pick (human
     2026-08-26). The stepped pick drew enable from one corner while vol came
     from another, and ADR-100's off transition hard-kills voices, so the
     boundary was a click and the partway state a chimera. Here the BILINEAR
     weight of the corners that hold the osc ON becomes a gain ramp (applied in
     applyOscGainAndMeter through the existing ~8 ms smoother), and the stepped
     flip is deferred to the weight floor, where the osc is already ~-60 dB:
     the kill/re-strike still runs, but inaudibly. Plain w[], not the Gumbel
     draw and not the resolver's sharpened weights -- the ramp is deterministic
     in the pad position, all three laws. At a pure corner the weight equals
     that corner's stored enable, so corners stay bit-identical. */
  bool morphApplyOscEnable(size_t i, const double *wBilinear)
  {
    const double onW = morphOnWeight(i, wBilinear);
    const uint32_t o = oscOfId(morphIds[i]);
    if (o < kMaxOsc) oscOnW[o] = onW;
    return morphCommitSlot(i, onW > kMorphOnFloor ? 1.0 : 0.0);
  }

  /* THE RAMP'S FLOOR (B48/B203): below this share of ON corners the source's
     stepped switch flips off (~-60 dB, so the kill and re-strike run
     inaudibly). Named because ADR-183's blend rule falls back to the plain
     blend at EXACTLY this floor — the parameter switch-over and the source's
     own kill are one threshold, so the switch-over happens only where the
     source is already off. */
  static constexpr double kMorphOnFloor = 1e-3;

  /* THE RAMP LAW, STATED ONCE (B203). The bilinear weight of the corners
     holding this switch ON, clamped. Extracted from morphApplyOscEnable rather
     than copied into its sibling below for ADR-110's reason: two copies of a
     law are two chances to edit one of them. */
  double morphOnWeight(size_t i, const double *wBilinear) const
  {
    double onW = 0;
    for (int k = 0; k < 4; k++) onW += wBilinear[k] * morphCorner[k][i];
    return onW < 0 ? 0 : (onW > 1 ? 1 : onW);
  }

  /* B203 — AN ENGINE BLOCK'S GATE, BY THE SAME LAW (human 2026-09-21: "Sub
     on/off is still exempt from morph and it ought to be wired in the way the
     other two oscs are"). Every property B48 claims is claimed here and for the
     same reasons: plain `w[]` and not the Gumbel draw or the resolver's
     sharpened weights, so the ramp is DETERMINISTIC IN THE PAD POSITION under
     all three laws; at a PURE CORNER the weight equals that corner's stored
     gate, so 0 and 1 come out exactly and the corner is bit-identical; and the
     stepped flip is deferred to the weight floor (1e-3, ~-60 dB) where the
     kill (subAllOff) and the re-strike still run but inaudibly.

     WHERE THE SUB DIFFERS FROM AN OSCILLATOR, and why the handling is the
     same anyway: an oscillator's ramp lands in applyOscGainAndMeter, which
     already existed for the mixer's mute/solo faders; the sub has no mixer
     strip of that kind, so renderSubSpan carries the ramp itself — the same
     one-pole (gainSmoothCoef) applied once per chunk OUTSIDE the sixteen-slot
     loop, because the gate is the ROW's switch and not each voice's. And the
     sub's kill is total (SubOscCore::allOff clears phase, envelope and filter
     state) where an oscillator's is a voice kill; that is exactly why the
     deferral to the weight floor matters more here, not less. */
  bool morphApplyGateEnable(size_t i, const double *wBilinear)
  {
    const double onW = morphOnWeight(i, wBilinear);
    setEngineGateRamp(morphIds[i], onW);
    return morphCommitSlot(i, onW > kMorphOnFloor ? 1.0 : 0.0);
  }

  /* ================= ADR-183 / B232 — THE OFF-CORNER BLEND RULE ===========
     The human, 2026-09-23: "if one morph corner has, say, Osc 2 turned off and
     the other has it on, instead of blending from corner 1's irrelevant Osc 2
     settings, the blend effectively treats corner 1 as if it has corner 2's
     osc 2 with the level at 0 … this is also how the Sub should work when
     blend is on." The on/off itself is B48/B203's level ramp and is untouched;
     this is about the source's CONTINUOUS parameters, which revision 1 blends
     over all four corners — so an off corner's settings, which cannot sound
     there, pulled the sound everywhere else (Osc 2 detune 0.1 off / 0.8 on
     read 0.45 at the midpoint).

     THE DECLARATION IS THE SOURCE'S GATE, not the depends graph. A slot's
     source is switched by exactly the id the level ramp already reads:
       - an ADR-088 engine block's `gateId` (the sub's 4015 over 4000..4019;
         STATION's block gets the rule by declaring its gate in kEngineBlocks);
       - an oscillator's enable (150 + k * kOscStride) over that oscillator's
         per-osc ids — ADR-082's global/per-osc classification, so a third
         swarm oscillator gets the rule from the stride.
     `param_presentation.tsv`'s `depends` was the other candidate and is the
     wrong one for three reasons: it also drives the GUI's shown_when (every
     oscillator control would hide when its oscillator is off — a GUI change
     nobody asked for); it drives ADR-108's hold on the PICK path, which this
     rule must leave alone and which is not revision-gated, so declaring the
     enable there would re-voice revision-1 patches under quantum; and its
     clause grammar ORs conditions, so `law=4` AND `enable=1` is not
     expressible without a grammar change (and gen_depends_header skips engine
     rows entirely).

     Returns kNoSourceGate for a slot with no switchable source (globals, the
     FX rack, routing cells) and for the gates themselves (their law is the
     ramp). An engine id must never reach baseIdOf — the order below is the
     rule morphExemptSlot states too. */
  static constexpr clap_id kNoSourceGate = CLAP_INVALID_ID;
  static clap_id sourceGateOf(clap_id id)
  {
    if (const EngineBlock *b = engineBlockOf(id)) return b->gateId == id ? kNoSourceGate : b->gateId;
    const uint32_t o = oscOfId(id);
    const clap_id base = baseIdOf(id);
    if (o >= kNumOsc || base == 150 || isGlobalId(base)) return kNoSourceGate;
    return (clap_id)(150 + o * kOscStride);
  }

  /* The BLEND's effective corner weights for continuous slot `i` at bilinear
     weights `w`: fills `e` and returns true when the off-corner rule applies,
     returns false when the weights are `w` itself. ONE statement of the law
     with two consumers — morphBlendTarget (the forward blend) and
     morphRouteEdit (its inverse, which must land a live edit ON the value the
     forward blend will then read, or the edit is overwritten at the next grid
     tick) — for ADR-110's reason: two copies of a law are two chances to edit
     one of them. `liveOnly` is `engineRevision() >= 2`.

     Revision 2: the weight of a corner whose source is OFF is dropped and the
     rest renormalised, `e = w g / sum(w g)` with g the corner's stored gate —
     the same `w * g` product morphOnWeight sums for the ramp, so the
     denominator IS the ramp's gain. Three cases keep `w`, each chosen so the
     rule changes a value only where it has something to say:
       - no gate, or the gate is EXEMPT (ADR-109: an exempt gate is live-only,
         so its corners' stored values say nothing about where it is off);
       - no OFF corner carries weight (`offW == 0`): the renormalised sum is
         the plain one mathematically, and taking the plain one makes it so
         bitwise — a pure ON corner, an all-ON field and a segment between two
         ON corners all read exactly what revision 1 reads;
       - the live weight is at or below kMorphOnFloor: the source's switch has
         flipped off there (morphApplyOscEnable / morphApplyGateEnable, same
         threshold, same comparison), so the value cannot sound and the plain
         blend keeps a pure OFF corner reading back its own stored value —
         corner bit-identity. The discontinuity at the floor lands where the
         ramp's gain is <= 1e-3 (-60 dB) and the source is being killed. */
  bool morphLiveWeights(size_t i, const double *w, bool liveOnly, double *e) const
  {
    if (!liveOnly || i >= morphGateSlot.size()) return false;
    const int32_t g = morphGateSlot[i];
    if (g < 0 || morphExempt[(size_t)g]) return false;
    double liveW = 0, offW = 0;
    for (int k = 0; k < 4; k++)
    {
      const double on = morphCorner[k][(size_t)g];
      liveW += w[k] * on;
      offW += w[k] * (1.0 - on);
    }
    if (offW <= 0 || liveW <= kMorphOnFloor) return false;
    for (int k = 0; k < 4; k++) e[k] = w[k] * morphCorner[k][(size_t)g] / liveW;
    return true;
  }

  /* The BLEND target. When the rule does not apply this is the plain
     four-corner sum by the SAME expression in the SAME order it always was,
     so a revision-1 patch renders bit-identically (the old law is selected,
     not deleted — ADR-183 §2).

     ONE MORE FALLBACK, forward only: every corner carrying weight holds the
     SAME value. Then both laws give that value mathematically, but not
     bitwise — off the grid the bilinear weights do not sum to exactly 1 in
     doubles, so `c * sum(w)` and `c * sum(e)` can land an ulp apart, and at
     (0.3, 0.7) two untouched osc-2 rows (1019, 1026) did exactly that
     (offcorner_check's off-grid control). Taking the plain sum keeps revision
     2 bit-identical to revision 1 wherever the rule has nothing to change,
     which is what lets "rev 2 differs only where the rule applies" be a
     bitwise statement about audio. NOT in morphLiveWeights, because the
     inverse (morphRouteEdit) must not take it: an edit of an uncontested slot
     makes it contested, and the forward then reads it through `e`, so the
     edit has to be distributed by `e` to land. */
  bool morphWeightedCornersAgree(size_t i, const double *w) const
  {
    int first = -1;
    for (int k = 0; k < 4; k++)
    {
      if (w[k] <= 0) continue;
      if (first < 0) first = k;
      else if (morphCorner[k][i] != morphCorner[first][i]) return false;
    }
    return true;
  }
  double morphBlendTarget(size_t i, const double *w, bool liveOnly) const
  {
    double e[4];
    const double *u =
        morphLiveWeights(i, w, liveOnly, e) && !morphWeightedCornersAgree(i, w) ? e : w;
    double target = 0;
    for (int k = 0; k < 4; k++) target += u[k] * morphCorner[k][i];
    return target;
  }

  void morphStep(int samples)
  {
    /* THE SEAM (B89 phase 2b, ADR-176 decision 6). ONE branch, taken only when
       a dev flag that ships OFF is on. With the flag off the instruction
       sequence past it is the one that shipped, which is why "bit-identical
       with the flag off" is a structural claim and not a measurement that
       happened to agree (parity_check 156/156 and statefix_check are the
       evidence; intent_check section S is the must-fail control that proves
       those two can see a difference at all).
       `morphOn > 0.5` is redundant at this call site and kept anyway: it is
       plan R15's rule — the flag does NOTHING with morph off, because bindings
       live in corners and with no field there is no owner — and a reader of
       this line should not have to go and find the caller to learn that. */
    if (intentBusOn > 0.5 && morphOn > 0.5) { intentStep(samples); return; }
    morphAccum += samples;
    const int grid = (int)std::lround(sampleRate * hypersaw::kGravGridSeconds);
    if (morphAccum < grid) return;
    const double dt = (double)morphAccum / sampleRate;
    morphAccum = 0;
    double w[4], lw[4];
    hypersaw::MorphCore::weights(morphX, morphY, w);
    hypersaw::MorphCore::logW(w, morphTemp, lw);
    const double coef = morphGlideS > 1e-4 ? 1 - std::exp(-dt / morphGlideS) : 1.0;
    // ADR-183: read ONCE per tick through the one read site B100 names.
    const bool offCornerRule = engineRevision() >= 2;
    /* ADR-183 / critic S1 (PR #744) — A SOURCE THAT WAS OFF LANDS, IT DOES NOT
       GLIDE. Below the floor the blend reads the plain sum, which near a pure
       OFF corner IS that corner's value; the one-pole then carried the slot
       from there toward the ON corners' value AFTER the source re-struck, so
       the OFF corner was heard on the way up (the critic measured the sub at
       -23 dB at the default glide and -13.5 dB at 0.5 s, osc 2 at about
       -9.5 dB, against the as-if patch). Whatever a slot held while its source
       was silent was never heard, so there is no audible position to glide
       FROM: on the tick its source comes back, a gated continuous slot takes
       its target outright — the "never applied yet" landing morphApplyTarget
       gives the -1e29 sentinel, written as a direct commit so a slot whose
       source stays off is not re-applied every tick. The state is read at the
       TOP of the tick, before the loop re-commits the gate. Blend only, like
       the rule itself; revision 1 never reads it. */
    const bool landRule = offCornerRule && (int)morphMode == 1;
    if (landRule)
      for (int32_t g : morphGateSlotList)
        morphSrcWasOff[(size_t)g] = readParam(morphIds[(size_t)g]) < 0.5 ? 1 : 0;
    for (size_t i = 0; i < morphIds.size(); i++)
    {
      const ParamDef *d = findParam(morphIds[i]);
      if (!d) continue;
      // ADR-109: an exempt parameter is not in the field at all — it holds
      // whatever it is set to, and no corner owns it.
      if (i < morphExempt.size() && morphExempt[i]) { morphExemptSlot(i); continue; }
      // B48's ramp and the exempt hold are the same two helpers the resolver
      // uses; `w` here is the plain bilinear weight, which is what that ramp
      // wants in both modes.
      if (isEngineGateId(morphIds[i])) { morphApplyGateEnable(i, w); continue; }
      if (baseIdOf(morphIds[i]) == 150) { morphApplyOscEnable(i, w); continue; }
      double target;
      if ((int)morphMode == 1 && !d->stepped)
      {
        target = morphBlendTarget(i, w, offCornerRule);
        const int32_t g = landRule && i < morphGateSlot.size() ? morphGateSlot[i] : -1;
        if (g >= 0 && morphSrcWasOff[(size_t)g]) { morphCommitSlot(i, target); continue; }
      }
      else
      {
        const int k = morph.pickCorner((int)morphGroupLead(i), lw, morphCoup);
        target = morphCorner[k][i];
        /* ADR-108 -- THE DERIVED MORPH HIERARCHY (the human's ask: "a graph of
           feature dependencies so we can automatically derive a morph
           hierarchy"). If this parameter's enabling condition is false IN THE
           CORNER THAT WON IT, the flip would be a no-op: the engine's own guard
           ignores the value, so the corner spent its identity on a parameter
           that cannot sound. Hold instead, and the flip lands on something
           audible. The condition is evaluated against the WINNING CORNER'S
           stored values, not the live ones -- the question is "would this
           matter if that corner were playing", which is the corner's own state
           to answer.
           Conservative by construction: no rule means always-live, so a
           parameter the graph does not describe morphs exactly as before. */
        if (!depLiveInCorner(morphIds[i], k)) target = morphCur[i] < -1e29
                                                           ? target
                                                           : morphCur[i];
      }
      morphApplyTarget(i, *d, target, coef);
    }
  }

  /* ================= B89 PHASE 2c — THE INTENT BUS, APPLIED ================
     ADR-176 (the owner law, the atoms, the ten intents, the flag) behind param
     266, which ships OFF. `intentStep` resolves SPEC-INTENT-BUS §4.5's
     `final[p]` into `intentResolved` and — since 2c — APPLIES it: that value
     is the target the shipped one-pole carries `morphCur` toward, written
     through morphApplyTarget, which is the same and only write path morphStep
     uses. 2b proved the two facts this rests on: the flag off costs nothing
     (structural — the branch is not taken), and the flag on with nothing bound
     is a pure corner read, so switching it on changes WHICH corner owns a slot
     and never what the owner's value means.

     WHY THE SHELL OWNS THE TABLES (plan R14). IntentCore is pure functions
     over caller-owned spans, so every array here is sized ONCE in morphInit,
     beside morphCorner[k].assign — the audio thread never allocates, and
     rtsafety_probe is the gate on that claim. The binding table alone is
     4 x 10 x N doubles (~78 KB at today's N); doubles, not floats, because a
     ten-term sum compared at 1e-6 absolute is uncomfortably close to float32.

     UNITS. `morphCorner` is RAW (a corner holds what the parameter holds).
     `bind` and `range` are NORMALISED, which is what lets a binding mean the
     same thing on a 0..1 knob and a 1..2000 ms time. The conversion happens
     here, at the boundary, with modStep's own span convention — so the corner
     chunk keeps every byte it had and no ADR-159 remap is in scope. */
  static constexpr int kIntents = 10;   // X, Y, M1..M8 (ADR-176 decision 4)
  /* THE STORED ORDER, append-only (ADR-176 decision 4 / plan R6). These are
     slot KEYS, not captions: a patch renames the captions (intentName below),
     and a rename must never move a binding. Written into the chunk so a future
     build that appends an eleventh slot can still read a ten-slot patch. */
  static constexpr const char *kIntentOrder[kIntents] = {"X",  "Y",  "M1", "M2", "M3",
                                                         "M4", "M5", "M6", "M7", "M8"};
  /* The default captions (ADR-176 Amendment 3 — the human's final eight). Patch
     state: the user renames them per patch, and the defaults are what a patch
     that never renamed anything gets, which is why they are never written into
     a chunk that is otherwise at its defaults. */
  static constexpr const char *kIntentDefaultName[kIntents] = {
      "X", "Y", "Space", "Timbre", "Motion", "Grit", "Time", "Character",
      "Brightness", "Pressure"};

  std::vector<double> intentRangeLo, intentRangeHi;   // [4 * N] normalised, corner-major
  std::vector<double> intentBind;                     // [4 * kIntents * N] normalised
  double intentHomeX[4] = {0.5, 0.5, 0.5, 0.5};       // §4.4 home, per corner
  double intentHomeY[4] = {0.5, 0.5, 0.5, 0.5};
  /* EMPTY MEANS DEFAULT. The array starts empty (a std::string member cannot
     carry kIntentDefaultName without a constructor), and morphInit is the site
     that fills it — but morphInit runs at ACTIVATE, so a GUI or an oracle that
     asked before then read ten blank captions. One accessor instead of one
     more initialisation site: "" is the absent caption, and the absent caption
     is the ADR-176 A3 default. It also means a rename sanitised down to
     nothing degrades to the default rather than to a blank knob. */
  std::string intentName[kIntents];
  const char *intentCaption(int i) const
  {
    if (i < 0 || i >= kIntents) return "";
    return intentName[i].empty() ? kIntentDefaultName[i] : intentName[i].c_str();
  }
  /* THE ATOM MAP (ADR-176 decision 2): atoms are LEAD GROUPS, not parameters —
     the distinct values of morphLead[], compacted, plus `home` as its own atom
     (plan R10). The scale is one atom of 13, each FX slot one of 3, and since
     B142 the whole routing block is one. Nothing here knows what a group
     means; morphInit builds the map and IntentCore just indexes it. */
  std::vector<int> intentAtomOf;        // [N] -> atom index
  int intentHomeAtom = 0;               // the `home` atom's index (the last one)
  int intentNAtoms = 0;
  std::vector<double> intentSeeds;      // [nAtoms], one per atom, from morphSeed
  double intentSharedSeed = 0;          // drawn AFTER them (ADR-176 Amendment 1)
  // Per-slot unit conversion, read off the ParamDef once (see UNITS above).
  std::vector<double> intentMinV, intentSpan;
  // The per-tick working set. Sized once; never resized on the audio thread.
  std::vector<double> intentBaseN;      // [4 * N] the corners, normalised
  std::vector<double> intentFinal;      // [N] §4.5 final, normalised
  std::vector<double> intentResolved;   // [N] the same value in RAW units — THE TARGET
  int intentWrote = 0;                 // slots the last apply wrote (a door's anchor, L0033)
  std::vector<int> intentOwnerAtom;     // [nAtoms]
  std::vector<int> intentOwnerParam;    // [N]
  std::vector<int> intentClamped;       // [N] §4.5's clamp indicator
  double intentValue[kIntents] = {0};   // the ten intents; X/Y are the pad's since 2d

  /* ---- §4.4 THE PERFORMANCE PAD (B89 phase 2d) --------------------------
     WHICH HOST PARAMETERS THE PAD READS, and it reads no others: the two
     macros ADR-150's assignment names — ids 179/180 hold the assignment, so
     the pad's x is `166 + mainAsn[0]` and its y is `166 + mainAsn[1]` (by
     default macros 1 and 2). Those two ids are the POINTER: they are what the
     host automates, what the GUI pad writes, and what a macro knob writes,
     and with the flag on they are an INPUT to the spring and nothing else.
     THE SHELL WRITES NEITHER — the puck below is separate state, so a spring
     return cannot fight the host for the parameter it is reading, and turning
     the flag on can never move a value the player's hand is on.

     THE PAD'S COORDINATE SPACE IS THE CANVAS'S: y grows DOWNWARD, because
     that is the space the prototype's `padPos` works in and therefore the
     space `home` is stored in (§4.4, and padIntentY's inversion assumes it).
     A macro knob reads UP. The two conventions meet at exactly one line, in
     intentStep, and nowhere else. */
  hypersaw::IntentCore::Puck intentPuck;
  /* DRAG IS A BRACKET, NOT A VALUE. A released pointer leaves the parameter
     exactly where it was, so no reading of the two macro values can tell
     "still held" from "let go" — only the gesture bracket can, which is why
     the latch lives here and is fed from drainQueue (the one place gesture
     messages already pass through on the audio thread, in order with the
     values they bracket). Per AXIS, because the two axes can name the same
     macro or none at all. */
  bool intentDrag[2] = {false, false};
  double intentLatchOn = 0;   // param 268; latched, the puck does not return

  /* Sized once, from morphInit, with morphIds and morphLead already built.
     Everything it writes is a DEFAULT: full range, no binding, centred home,
     the ADR-176 A3 captions — so an instance that never sees an `intent=`
     chunk resolves exactly as a plain corner read. */
  void intentInit()
  {
    const size_t n = morphIds.size();
    intentRangeLo.assign(4 * n, 0.0);
    intentRangeHi.assign(4 * n, 1.0);
    intentBind.assign((size_t)4 * kIntents * n, 0.0);
    for (int k = 0; k < 4; k++) { intentHomeX[k] = 0.5; intentHomeY[k] = 0.5; }
    for (int i = 0; i < kIntents; i++) intentName[i] = kIntentDefaultName[i];

    intentMinV.assign(n, 0.0);
    intentSpan.assign(n, 1.0);
    for (size_t i = 0; i < n; i++)
    {
      const ParamDef *d = findParam(morphIds[i]);
      if (!d) continue;
      intentMinV[i] = d->minV;
      // A zero span would make the normalisation a division by zero; no shipped
      // row has one, and a future one degrades to "already normalised".
      intentSpan[i] = (d->maxV - d->minV) > 1e-300 ? (d->maxV - d->minV) : 1.0;
    }

    intentAtomOf.assign(n, 0);
    {
      std::vector<int> compact(n, -1);   // morphIds index -> atom, for leads only
      int na = 0;
      for (size_t i = 0; i < n; i++)
      {
        const size_t lead = morphGroupLead(i);
        if (compact[lead] < 0) compact[lead] = na++;
        intentAtomOf[i] = compact[lead];
      }
      intentHomeAtom = na;
      intentNAtoms = na + 1;
    }
    intentSeeds.assign((size_t)intentNAtoms, 0.0);
    intentDrawSeeds();

    intentBaseN.assign(4 * n, 0.0);
    intentFinal.assign(n, 0.0);
    intentResolved.assign(n, 0.0);
    intentOwnerAtom.assign((size_t)intentNAtoms, 0);
    intentOwnerParam.assign(n, 0);
    intentClamped.assign(n, 0);
  }

  /* One seed per atom in ATOM-INDEX order, the shared seed appended last
     (ADR-176 Amendment 1): appending it leaves every per-atom draw
     bit-identical to a stream without coupling, so turning coupling on moves
     the blend and not the boundaries. Pure array writes over storage that
     already exists — the same RT-safety argument MorphCore::reshuffle makes at
     the id-156 site that calls this. */
  void intentDrawSeeds()
  {
    if (intentSeeds.empty()) return;
    hypersaw::IntentCore::drawSeeds(morphSeed, intentSeeds.data(), intentNAtoms,
                                    &intentSharedSeed);
  }

  /* ---- the `intent=` chunk ------------------------------------------------
     SPARSE, AND KEYED ON THE PARAMETER ID. Sparse for ADR-138/ADR-088's
     reason: a patch that has never bound anything writes NO KEY, so every
     stored preset, fixture and factory file keeps the bytes it had and
     statefix_check / bank_check stay the regression proof for this change
     rather than casualties of it. Keyed on the id rather than the morphIds
     INDEX because an index is a layout fact — ADR-159 is the scar — and an id
     this build does not expose is simply skipped, which is how a patch saved
     by a wider future build stays loadable here.

     GRAMMAR (one line, comma-separated tokens, colon-separated fields):
       L:1                      layout; first token, always present
       O:<k0>:...:<k9>          the stored slot order, append-only (kIntentOrder)
       N:<i>:<name>             a renamed intent slot (only when renamed)
       R:<id>:<k>:<lo>:<hi>     corner k's range for parameter id, normalised
       B:<id>:<k>:<i>:<v>       corner k's binding of intent i to parameter id
       H:<k>:<x>:<y>            corner k's pad home
     `base` is NOT here: it stays in the `morph=` corner chunk where it always
     was, which is the whole reason that chunk's bytes do not move. */
  static std::string intentSafeName(const std::string &in)
  {
    std::string out;
    for (char c : in)
    {
      // The three characters that ARE the grammar, plus anything that would
      // need escaping inside the JSON string this chunk also travels in. A
      // name is a caption, so degrading it beats inventing an escape layer.
      if (c == ',' || c == ':' || c == '"' || c == '\\' || (unsigned char)c < 0x20) continue;
      out += c;
      if (out.size() >= 24) break;
    }
    return out;
  }

  std::string intentChunk() const
  {
    if (morphIds.empty() || intentBind.empty()) return {};
    const size_t n = morphIds.size();
    std::string body;
    char buf[96];
    for (int i = 0; i < kIntents; i++)
      if (std::strcmp(intentCaption(i), kIntentDefaultName[i]) != 0)
        body += ",N:" + std::to_string(i) + ":" + intentSafeName(intentName[i]);
    for (int k = 0; k < 4; k++)
      for (size_t i = 0; i < n; i++)
      {
        const size_t cp = (size_t)k * n + i;
        if (intentRangeLo[cp] == 0.0 && intentRangeHi[cp] == 1.0) continue;
        std::snprintf(buf, sizeof buf, ",R:%u:%d:%.17g:%.17g", (unsigned)morphIds[i], k,
                      intentRangeLo[cp], intentRangeHi[cp]);
        body += buf;
      }
    for (int k = 0; k < 4; k++)
      for (int t = 0; t < kIntents; t++)
        for (size_t i = 0; i < n; i++)
        {
          const size_t bi = ((size_t)k * kIntents + t) * n + i;
          if (intentBind[bi] == 0.0) continue;
          std::snprintf(buf, sizeof buf, ",B:%u:%d:%d:%.17g", (unsigned)morphIds[i], k, t,
                        intentBind[bi]);
          body += buf;
        }
    for (int k = 0; k < 4; k++)
    {
      if (intentHomeX[k] == 0.5 && intentHomeY[k] == 0.5) continue;
      std::snprintf(buf, sizeof buf, ",H:%d:%.17g:%.17g", k, intentHomeX[k], intentHomeY[k]);
      body += buf;
    }
    if (body.empty()) return {};   // nothing has left its default: no key at all
    std::string out = "L:1,O";
    for (int i = 0; i < kIntents; i++) { out += ":"; out += kIntentOrder[i]; }
    return out + body;
  }

  /* A load is a load: EVERY table returns to its default first, so a patch
     with no `intent=` key loads unbound rather than inheriting whatever the
     previous patch bound. `O` is READ, not trusted-and-ignored: a slot key
     this build does not know ends the mapping for that patch's later slots,
     which is what append-only buys. */
  void applyIntentChunk(const std::string &chunk)
  {
    morphInit();
    if (intentBind.empty()) return;
    const size_t n = morphIds.size();
    std::fill(intentRangeLo.begin(), intentRangeLo.end(), 0.0);
    std::fill(intentRangeHi.begin(), intentRangeHi.end(), 1.0);
    std::fill(intentBind.begin(), intentBind.end(), 0.0);
    for (int k = 0; k < 4; k++) { intentHomeX[k] = 0.5; intentHomeY[k] = 0.5; }
    for (int i = 0; i < kIntents; i++) intentName[i] = kIntentDefaultName[i];
    if (chunk.empty()) return;

    // The patch's slot order, defaulting to ours; `stored[j]` is the slot index
    // THIS build gives the j-th slot the patch stored, or -1 for one we lack.
    int stored[kIntents];
    for (int i = 0; i < kIntents; i++) stored[i] = i;

    size_t pos = 0;
    while (pos < chunk.size())
    {
      const size_t comma = chunk.find(',', pos);
      const std::string tok =
          chunk.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
      pos = comma == std::string::npos ? chunk.size() : comma + 1;
      if (tok.size() < 2 || tok[1] != ':') continue;
      // Fields after the leading "X:".
      std::vector<std::string> f;
      {
        size_t q = 2;
        while (q <= tok.size())
        {
          const size_t c = tok.find(':', q);
          f.push_back(tok.substr(q, c == std::string::npos ? std::string::npos : c - q));
          if (c == std::string::npos) break;
          q = c + 1;
        }
      }
      switch (tok[0])
      {
        case 'L': break;   // the layout marker; only version 1 exists
        case 'O':
        {
          for (int j = 0; j < kIntents; j++)
          {
            stored[j] = -1;
            if ((size_t)j >= f.size()) continue;
            for (int i = 0; i < kIntents; i++)
              if (f[(size_t)j] == kIntentOrder[i]) { stored[j] = i; break; }
          }
          break;
        }
        case 'N':
        {
          if (f.size() < 2) break;
          const int j = std::atoi(f[0].c_str());
          if (j < 0 || j >= kIntents || stored[j] < 0) break;
          intentName[stored[j]] = intentSafeName(f[1]);
          break;
        }
        case 'R':
        {
          if (f.size() < 4) break;
          const clap_id id = (clap_id)std::strtoul(f[0].c_str(), nullptr, 10);
          const int k = std::atoi(f[1].c_str());
          const size_t i = intentSlotOf(id);
          if (k < 0 || k > 3 || i == SIZE_MAX) break;
          intentRangeLo[(size_t)k * n + i] = std::atof(f[2].c_str());
          intentRangeHi[(size_t)k * n + i] = std::atof(f[3].c_str());
          break;
        }
        case 'B':
        {
          if (f.size() < 4) break;
          const clap_id id = (clap_id)std::strtoul(f[0].c_str(), nullptr, 10);
          const int k = std::atoi(f[1].c_str());
          const int j = std::atoi(f[2].c_str());
          const size_t i = intentSlotOf(id);
          if (k < 0 || k > 3 || j < 0 || j >= kIntents || stored[j] < 0 || i == SIZE_MAX) break;
          intentBind[((size_t)k * kIntents + stored[j]) * n + i] = std::atof(f[3].c_str());
          break;
        }
        case 'H':
        {
          if (f.size() < 3) break;
          const int k = std::atoi(f[0].c_str());
          if (k < 0 || k > 3) break;
          intentHomeX[k] = hypersaw::IntentCore::clamp01(std::atof(f[1].c_str()));
          intentHomeY[k] = hypersaw::IntentCore::clamp01(std::atof(f[2].c_str()));
          break;
        }
        default: break;
      }
    }
  }

  size_t intentSlotOf(clap_id id) const
  {
    for (size_t i = 0; i < morphIds.size(); i++)
      if (morphIds[i] == id) return i;
    return SIZE_MAX;
  }

  /* The pad's pointer id for one axis, or -1 for "None" (assignment 8). One
     decoder, so the gesture latch and the spring's drag target cannot come to
     disagree about which parameter the pad is reading.

     9 (Pitch Bend) and 10 (Mod Wheel) also return -1, and that is the CONTRACT,
     not an oversight: the intent bus's pad pointer is a macro or nothing —
     SPEC-INTENT-BUS gives the pad an INTENT to displace, and neither bend nor
     the wheel is one. An axis aimed at them is invisible to the latch and to
     the spring, exactly like None, while the editor writes the signal through
     the path that signal already owns. */
  int intentPadId(int axis) const
  {
    const int a = mainAsn[axis];
    return (a >= 0 && a < 8) ? 166 + a : -1;
  }

  /* Fed from drainQueue for EVERY gesture, flag or no flag: with the flag off
     nothing reads these two bools, which is why doing it unconditionally costs
     nothing observable and keeps the branch out of the message loop.
     A knob on the same macro brackets the same way a pad drag does, and that
     is deliberate — the pointer is the PARAMETER, not the canvas, so grabbing
     Macro 1's knob moves the puck exactly as grabbing the pad's x does. */
  void intentNoteGesture(clap_id id, bool begin)
  {
    for (int a = 0; a < 2; a++)
      if (intentPadId(a) == (int)id) intentDrag[a] = begin;
  }

  /* ---- the seam ----------------------------------------------------------
     SPEC-INTENT-BUS §4.1-§4.5 over the shell's own field, on morphStep's own
     grid and accumulator (one accumulator, so there is no second one to drift
     out of step with the first). Since 2c the resolved value is APPLIED, by
     intentApply below and through morphApplyTarget — the write morphStep would
     have made, made once, from the other law. Flag-off bit-identity stays
     structural rather than measured: with the flag off this function is not
     entered at all.

     Deliberately NOT built here (each has its phase): the corner-scope
     modulation tier (plan R5 — the prototype's corner LFO has no shell
     counterpart yet) and the promoted/device mod tiers, which `modStep`
     already is. The pad spring and latch arrived in 2d and are below. */
  void intentStep(int samples)
  {
    morphAccum += samples;
    const int grid = (int)std::lround(sampleRate * hypersaw::kGravGridSeconds);
    if (morphAccum < grid) return;
    // dt BEFORE the reset: the glide coefficient is a function of the interval
    // that actually elapsed, not of the nominal grid (ADR-086/ADR-009).
    const double dt = (double)morphAccum / sampleRate;
    morphAccum = 0;
    const size_t n = morphIds.size();
    if (n == 0 || intentFinal.size() != n) return;
    using IC = hypersaw::IntentCore;

    /* §4.2. steepness = 1/morphTemp (ADR-176 decision 1): temp 1 is the
       spec's softest blend, temp 0.02 its hardest flip. The modSum argument is
       0 here because a device routing that targets MorphX/Y is plan R5's
       phase-3 tier; the call is written through effectiveMorph anyway so the
       site that gains it is already the right one. */
    double w[4];
    IC::weights(IC::effectiveMorph(morphX, 0.0), IC::effectiveMorph(morphY, 0.0),
                morphTemp > 1e-9 ? 1.0 / morphTemp : 1.0e9, w);
    IC::resolveAtoms(intentSeeds.data(), intentNAtoms, intentSharedSeed, morphCoup, w,
                     intentOwnerAtom.data());
    IC::mapOwners(intentOwnerAtom.data(), intentAtomOf.data(), (int)n,
                  intentOwnerParam.data());

    /* §4.4 THE PERFORMANCE PAD (B89 phase 2d). `home` is an ATOM (ADR-176
       decision 2 / plan R10), so its owner comes out of the SAME walk every
       parameter atom came out of two lines above — the home flips with the
       field because it is in the field, not because a rule here says so. */
    const int homeOwner = intentOwnerAtom[intentHomeAtom];
    const double homeX = intentHomeX[homeOwner], homeY = intentHomeY[homeOwner];
    /* The one line where the knob's convention (up = 1) meets the pad's
       (canvas y, down = 1). An unassigned axis has no pointer, so its drag
       target is the puck itself: the axis simply cannot be dragged. */
    const double dragX = intentPadId(0) >= 0 ? macroVal[mainAsn[0]] : intentPuck.x;
    const double dragY = intentPadId(1) >= 0 ? 1.0 - macroVal[mainAsn[1]] : intentPuck.y;
    const IC::Spring padSpring;   // the prototype's constants, in 1/s^2 and 1/s
    IC::padStep(intentPuck, dt, intentDrag[0] || intentDrag[1], dragX, dragY,
                intentLatchOn > 0.5, homeX, homeY, padSpring);

    /* THE TEN INTENTS. X and Y are the puck's DISPLACEMENT from the owning
       corner's home (+-0.5 of the pad's extent = full swing); M1..M8 are the
       eight macro knobs, read straight. The two macros the pad's axes name do
       DOUBLE DUTY — they are the pointer AND their own M-intent — because
       ADR-150 built the MAIN pad as a macro pad and 2d does not move it. */
    intentValue[0] = IC::padIntentX(intentPuck, homeX);
    intentValue[1] = IC::padIntentY(intentPuck, homeY);
    for (int i = 0; i < 8; i++) intentValue[2 + i] = macroVal[i];

    // The corners, normalised (see UNITS). Recomputed per tick because a corner
    // is editable while the field runs; the cost is one multiply-add per slot.
    for (int k = 0; k < 4; k++)
      for (size_t i = 0; i < n; i++)
        intentBaseN[(size_t)k * n + i] = (morphCorner[k][i] - intentMinV[i]) / intentSpan[i];

    IC::stepParams((int)n, kIntents, intentValue, intentOwnerParam.data(),
                   intentBaseN.data(), intentBind.data(), intentRangeLo.data(),
                   intentRangeHi.data(), nullptr, nullptr, nullptr, nullptr,
                   intentFinal.data(), intentClamped.data());
    for (size_t i = 0; i < n; i++)
      intentResolved[i] = intentMinV[i] + intentFinal[i] * intentSpan[i];
    intentApply(dt);
  }

  /* ---- the apply (B89 phase 2c) -----------------------------------------
     Every morphable slot, the resolver's `final[p]` as the TARGET, through the
     same three helpers morphStep applies with — so there is no second write
     path to keep in step and nothing here reaches applyParam except by the
     route the field has always taken (morphFromField set, ADR-109's choke
     point, the 1e-9 deadband).

     Three rules the resolver does NOT get to reinterpret, because they are the
     field's and the field is still what is sounding:
       - morphExempt (ADR-109): an exempt slot is not in the field, so nothing
         is written for it and no owner it may have been assigned is consulted.
       - the lead groups (ADR-176 decision 2): the atom map IS morphLead, built
         in intentInit, so a group flips as one by construction rather than by
         a rule repeated here.
       - ADR-108's hold, and WHICH RANGE CLAMPED THE VALUE. This is the hazard
         the plan names. The corner clamp belongs to the OWNER's range and is
         already applied, inside IntentCore::stepParams, to the owner's own
         base. The hold that follows replaces that value with the LIVE one and
         must NOT re-clamp it: the held value belongs to whatever corner last
         sounded, and squeezing it into the owner's range would be a value no
         corner ever authored — audible exactly when the owner's range is
         narrow, which is the case the range control exists for. Order is the
         whole rule: clamp with the owner's range FIRST, hold AFTER, no clamp
         on the way out. (intent_check T-G is the assertion; it fails against
         the inverted order.) */
  void intentApply(double dt)
  {
    const double coef = morphGlideS > 1e-4 ? 1 - std::exp(-dt / morphGlideS) : 1.0;
    // The B48 ramp is bilinear in the pad position under every law (see
    // morphApplyOscEnable) — NOT the sharpened weights the owner draw uses.
    double wBilinear[4];
    hypersaw::MorphCore::weights(morphX, morphY, wBilinear);
    intentWrote = 0;
    for (size_t i = 0; i < morphIds.size(); i++)
    {
      const ParamDef *d = findParam(morphIds[i]);
      if (!d) continue;
      if (i < morphExempt.size() && morphExempt[i]) { morphExemptSlot(i); continue; }
      if (isEngineGateId(morphIds[i]))
      {
        if (morphApplyGateEnable(i, wBilinear)) intentWrote++;
        continue;
      }
      if (baseIdOf(morphIds[i]) == 150)
      {
        if (morphApplyOscEnable(i, wBilinear)) intentWrote++;
        continue;
      }
      const int k = intentOwnerParam[i];
      double target = intentResolved[i];
      if (!depLiveInCorner(morphIds[i], k))
        target = morphCur[i] < -1e29 ? target : morphCur[i];
      if (morphApplyTarget(i, *d, target, coef)) intentWrote++;
    }
  }

  /* CALIBRATION ONLY (L0032) — 2b's must-fail control for "the flag off is
     bit-identical", carried into 2c with its meaning intact. A green flag-off
     render proves the comparison RAN; it does not prove the comparison could
     have failed. So this runs the resolver AND its apply once, at whatever the
     flag happens to be, and intent_check asserts the resulting render DIFFERS.
     Reached only from hypersaw_debug_intent_plant: no shell path, no GUI path
     and no host path calls it.
     In 2b this door owned a second write loop, because intentStep applied
     nothing. In 2c the apply IS intentStep's, so the door calls it and reports
     the count instead of writing again — which is how "no second write path"
     stays a property of the code rather than a claim in a comment. The count
     is the control's own anchor: a plant that silently wrote nothing would
     "not fire" for the wrong reason (L0033). */
  int intentPlantOnce()
  {
    morphInit();
    intentStep((int)std::lround(sampleRate * hypersaw::kGravGridSeconds));
    return intentWrote;
  }

  /* CALIBRATION ONLY — SPEC-INTENT-BUS §7 commit, reachable only from
     hypersaw_debug_intent_commit. The GUI verb and the pad it re-homes onto
     are 2d's; T6's invariant (after a commit the resolved sound is unchanged
     and every offset reads zero) has to be measured through the plugin NOW,
     and a test-only door is both cheaper and more honest than a half-built
     button. `forceCorner >= 0` is the must-fail control: baking into a corner
     that does not own the displacement must NOT leave the sound unchanged. */
  int intentCommit(int forceCorner)
  {
    morphInit();
    const size_t n = morphIds.size();
    if (n == 0 || intentFinal.size() != n) return -1;
    using IC = hypersaw::IntentCore;
    double w[4];
    IC::weights(IC::effectiveMorph(morphX, 0.0), IC::effectiveMorph(morphY, 0.0),
                morphTemp > 1e-9 ? 1.0 / morphTemp : 1.0e9, w);
    if (forceCorner >= 0 && forceCorner < 4)
      for (int k = 0; k < 4; k++) w[k] = (k == forceCorner) ? 1.0 : 0.0;
    // Commit bakes into the NORMALISED bases the resolver reads; intentStep
    // refreshes them from morphCorner every tick and this door runs between
    // ticks, so refresh here rather than trusting the last tick's copy.
    for (int k = 0; k < 4; k++)
      for (size_t i = 0; i < n; i++)
        intentBaseN[(size_t)k * n + i] = (morphCorner[k][i] - intentMinV[i]) / intentSpan[i];
    /* THE REAL PUCK since 2d. §7's re-home is what makes commit an identity:
       the displacement is baked into the dominant corner's bases AND that
       corner's home moves to where the puck is, so X and Y read zero on the
       next tick instead of re-applying the offset that was just baked. With
       the pad at rest the puck IS the home and this is the identity 2c wrote
       by hand; with the pad displaced it is the only version that holds. */
    const IC::Puck &pk = intentPuck;
    int dom = -1;
    IC::commit((int)n, kIntents, intentValue, intentOwnerParam.data(), w, intentBaseN.data(),
               intentBind.data(), intentRangeLo.data(), intentRangeHi.data(), pk, intentHomeX,
               intentHomeY, nullptr, nullptr, &dom);
    if (dom < 0 || dom > 3) return -1;
    // Back to RAW, for the slots commit actually touched — a round trip
    // through the normalisation is not free, so untouched slots keep the exact
    // bytes they were stored with.
    for (size_t i = 0; i < n; i++)
      if (intentOwnerParam[i] == dom)
        morphCorner[dom][i] = intentMinV[i] + intentBaseN[(size_t)dom * n + i] * intentSpan[i];
    morphCornersAuthored = true;
    /* Zero the KNOBS, not the derived copy: `intentValue` is recomputed from
       macroVal on every tick, so zeroing it alone would last exactly one tick.
       Eight writes, not ten: since 2d X and Y are PAD-DRIVEN (§7.3's exception
       — the re-home above already zeroes them), and the two macros the pad's
       axes name are zeroed here as the M-intents they also are. */
    for (int i = 0; i < 8; i++) macroVal[i] = 0.0;
    for (int i = 0; i < kIntents; i++) intentValue[i] = 0.0;
    return dom;
  }

  /* CALIBRATION ONLY — T5's must-fail control, and nothing else may call it.
     "Atoms are lead groups" is only testable if the test can BREAK the group:
     move one member onto a different atom and the chimera ADR-176 decision 2
     forbids must appear. The `home` atom is the one atom no parameter maps to
     and it carries its own independently drawn seed, so moving a member there
     is exactly "this member no longer follows its group" — with no table to
     resize and therefore nothing to allocate. NOT undoable: morphInit is
     once-per-instance, so the control runs on an instance of its own. */
  bool intentBreakAtom(int slot)
  {
    morphInit();
    if (slot < 0 || (size_t)slot >= intentAtomOf.size()) return false;
    intentAtomOf[(size_t)slot] = intentHomeAtom;
    return true;
  }

  double mpeBendLaw = 1;   // ADR-097: per-note bend follows the wheel by default

  void pushNoteLaw()
  {
    hypersaw::GlideCore::Params e = noteLink >= 0.5 ? bendLaw : noteLawOwn;
    // retMul is bend-only by construction (a note has no home pitch), so a
    // FOLLOWING note lane must not inherit the bend lane's return multiplier.
    e.retMul = 1.0;
    // Anchor mode is bend-only for the same reason: the note lane's `base` is
    // kLogFreqToMidi — a unit-alignment constant, not a note — so "admit the
    // anchor's class" would admit pitch class 0 forever. Strict is the honest
    // reading of "scale" for a lane whose anchor is not a pitch.
    if ((int)e.quant == hypersaw::GlideCore::kQuantScaleAnchor ||
        (int)e.quant == hypersaw::GlideCore::kQuantScaleOffset)
      e.quant = hypersaw::GlideCore::kQuantScale;
    e.scaleRoot = scale.root;
    for (int d = 0; d < 12; d++) e.scaleMask[d] = scale.mask[d];
    e.qTime = resolveQTimeMs();
    for (auto &c : cores) c.setNoteLaw(e);
  }
  // ADR-035 bass-mono output stage: ONE 2nd-order TPT SVF high-pass on the
  // SIDE channel (L = M + HP(S), R = M − HP(S)) — lows collapse to mid with
  // no crossover phase mismatch, the classic vinyl-elliptic routing.
  double bassMonoOn = 0, bassMonoHz = 120;
  // B146: 0 = pre (the historical placement), 1 = post, 2 = both. Held as the
  // stepped integer the label array indexes, so the render branch is a compare
  // and not a rounding decision taken once per block.
  int bassMonoPos = 0;
  double masterVol = 1.0, masterVolSm = 1.0;   // B24: target + smoothed
  // Which oscillator the visuals describe. GUI-owned (follows the OSC tab),
  // audio-thread-read. The visuals were hardwired to oscillator 0 — the
  // intermediary the human asked for is this one index.
  std::atomic<uint32_t> vizOsc{0};
  /* The bass-mono SVF's two integrator states — one PAIR PER PLACEMENT, and
     for `pre`, one pair PER SOURCE (B23 increment 3: pre runs on each source
     buffer separately, upstream of the matrix). Under `both` the two stages run
     in series over the same block, so a shared pair would have the post stage
     read the pre stage's history and neither filter would be the 2nd-order
     Butterworth it claims to be; two sources through one pair is the same fault
     in space instead of time. Plain members: preallocated, and the audio thread
     allocates nothing. */
  double bmIc1[kRoutingNSrc] = {0}, bmIc2[kRoutingNSrc] = {0};   // pre  — per source, before the rack
  double bmIc1Post = 0, bmIc2Post = 0;     // post — after the rack
  // EVERY pair, in one call: the two writers below both mean "forget all of it",
  // and enumerating them at each site is how one gets missed when a pair is added.
  void clearBassMonoState()
  {
    for (int s = 0; s < kRoutingNSrc; s++) bmIc1[s] = bmIc2[s] = 0;
    bmIc1Post = bmIc2Post = 0;
  }

  void updateTune(uint32_t k)
  {
    const double st = 12.0 * (octaveA[k] + gOct) + semiA[k] + gSemi + pitchBend +
                      (fineCentsA[k] + gFine) / 100.0 + modPitchSm    // B69: matrix offset
                      + pitchContA[k];   // ADR-150: the morphable continuous pitch
    const double factor = st == 0.0 ? 1.0 : std::pow(2.0, st / 12.0);
    cores[k].setParam("tune", factor);
    if (k == 0)
      spectra.setParam("tune", factor);  // ADR-057: SPECTRA rides osc 0's transpose (legacy path)
  }
  void updateTuneAll()
  {
    for (uint32_t k = 0; k < kNumOsc; k++) updateTune(k);
  }
  struct Held
  {
    int16_t key;
    double freq;
  };
  Held heldStack[16];
  int heldCount = 0;
  int monoSlot = -1;
  /* Identity-initialised: a slot that was never bound behaves exactly as the
     old code did rather than indexing on uninitialised memory. The map is a
     correction to an assumption, so its unset state must be that assumption. */
  struct InitSlotMap {
    explicit InitSlotMap(int (*m)[kNumOsc]) {
      for (uint32_t s = 0; s < hypersaw::kPoly; s++)
        for (uint32_t k = 0; k < kNumOsc; k++) m[s][k] = (int)s;
    }
  } initSlotMap{slotOf};

  // Host note identity per swarm slot, for CLAP NOTE_END: hosts use note-end
  // to retire per-note bookkeeping, and without it some (Live via the VST3
  // wrapper) withhold retriggering a pitch until they believe the previous
  // note ended — the 2026-07-18 "retrigger doesn't overlap" report.
  struct NoteTag
  {
    int32_t noteId = -1;
    int16_t port = -1, channel = -1, key = -1;
    bool active = false;
    float vel = 1.0f;   // ADR-100 A1: an enable-ON re-strike must not change loudness
  };
  NoteTag tags[hypersaw::kPoly];
  // RETIRED TAGS AWAITING NOTE_END (2026-07-31, the mono-poison bug). A mono
  // retarget — and a poly voice steal — OVERWRITES tags[slot] with the new
  // note, so the old note's identity is gone before emitNoteEnds could ever
  // end it. The wrapper's table then carries that note as sounding FOREVER:
  // Live withholds retriggering its key, the damage survives switching modes
  // (nothing re-ends it), and a fast arpeggiator "fixes" it by cycling every
  // key through a fresh on/off/END — the human's exact diagnostic. Every
  // overwrite of an active tag now queues the old identity here; emitNoteEnds
  // flushes the queue unconditionally each block.
  NoteTag pendingEnds[2 * hypersaw::kPoly];
  int pendingEndCount = 0;
  void retireTag(int slot)
  {
    if (!tags[slot].active) return;
    if (pendingEndCount < (int)(sizeof(pendingEnds) / sizeof(pendingEnds[0])))
      pendingEnds[pendingEndCount++] = tags[slot];
    tags[slot].active = false;
  }

  // ADR-038: latched per-channel MPE pitch bend, in semitones. MPE hosts
  // send member-channel bend BEFORE the note-on it modifies, so the latch —
  // not the event — is what a fresh strike must read. Channel index 0 is the
  // MPE manager / plain single-channel MIDI and is deliberately excluded:
  // member channels are 2-16 (indices 1-15), and applying the ±48 st MPE
  // range to a normal ±2 st bend wheel on channel 1 would be wildly wrong.
  double mpeBendSemis[16] = {0};

  /* PER-NOTE BEND INERTIA (ADR-097). bend-lab gives every sounding note its OWN
     inertia state stepped with the SAME params as the wheel — `nt.bend.step(
     nt.bendTgt, P, nt.midi)` — and the port applied per-note bend INSTANTLY at
     all three of its entry points instead. So a patch with a bend law shaped the
     wheel and left MPE snapping, which is the one case where character matters
     most: on an MPE controller the bend IS the performance.
     One traveller per note slot, not per channel: two notes on one channel can
     be at different bends mid-flight, and a channel-keyed lane would drag them
     together. `bendLane = true` because retMul — return-toward-rest — is exactly
     as meaningful here as on the wheel. */
  struct NoteBendLane
  {
    hypersaw::GlideCore g{44100.0 / 16, /*bendLane=*/true};   // the kBendGrid rate
    double target = 0;
    double emitted = 0;
    bool live = false;
  };
  NoteBendLane noteBend[hypersaw::kPoly];

  // Set a note's bend TARGET. With no law engaged this is the historical instant
  // write, byte-for-byte — the law-off path must not acquire a traveller.
  void setNoteBendTarget(int slot, double semis)
  {
    if (slot < 0 || slot >= hypersaw::kPoly) return;
    NoteBendLane &nb = noteBend[slot];
    nb.target = semis;
    if (!bendActive() || !mpeBendLaw)
    {
      nb.g.reset(semis);
      nb.emitted = semis;
      nb.live = false;
      noteExprSetBend(slot, semis);
      return;
    }
    nb.live = true;
  }

  // A fresh strike ARRIVES at its latched bend rather than travelling to it: the
  // note did not exist while the controller moved, so gliding in from zero would
  // invent a gesture the player never made. Mirrors the reference's reset().
  void seedNoteBend(int slot, double semis)
  {
    if (slot < 0 || slot >= hypersaw::kPoly) return;
    noteBend[slot].g.reset(semis);
    noteBend[slot].target = semis;
    noteBend[slot].emitted = semis;
    noteBend[slot].live = false;
    noteExprSetBend(slot, semis);
  }

  // One grid step for every travelling note. Called from the same boundary that
  // steps the wheel, so both lanes advance on one clock.
  void stepNoteBends()
  {
    for (int i = 0; i < hypersaw::kPoly; i++)
    {
      NoteBendLane &nb = noteBend[i];
      if (!nb.live || !tags[i].active) continue;
      const double v = nb.g.step(nb.target, bendLaw, (double)tags[i].key);
      if (v != nb.emitted) { nb.emitted = v; noteExprSetBend(i, v); }
    }
  }

  void emitNoteEnds(const clap_output_events_t *out, uint32_t time)
  {
    int kept = 0;
    for (int k = 0; k < pendingEndCount; k++)
    {
      clap_event_note_t ev{};
      ev.header.size = sizeof(ev);
      ev.header.time = time;
      ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      ev.header.type = CLAP_EVENT_NOTE_END;
      ev.note_id = pendingEnds[k].noteId;
      ev.port_index = pendingEnds[k].port;
      ev.channel = pendingEnds[k].channel;
      ev.key = pendingEnds[k].key;
      ev.velocity = 0;
      // KEEP IT IF THE PUSH IS REJECTED. try_push CAN fail — the host's output
      // buffer is finite and drainQueue floods it with param events whenever a
      // knob moves. Ignoring the return value silently DESTROYED the NOTE_END,
      // so Live never learned the note ended and withheld retriggering that
      // pitch: "stuck for longer than it should, most when I've recently
      // changed the K value" (human, 2026-08-03). Survivors are compacted and
      // retried next block.
      if (out->try_push(out, &ev.header)) continue;
      pendingEnds[kept++] = pendingEnds[k];
    }
    pendingEndCount = kept;
    for (int i = 0; i < hypersaw::kPoly; i++)
    {
      if (!tags[i].active) continue;
      // EMIT ON RELEASE, NOT ENV DEATH (2026-07-31 redesign, test round 1).
      // Live gates RETRIGGERING a pitch on receiving this note's END — the
      // 2026-07-18 finding that motivated emission. Emitting at env death made
      // the host wait on an invisible ~1.1 s tail: inconsistent minimum note
      // durations, laggy release, mono re-press blocked until the tail died.
      // gate==0 is the moment the musical note ended; the DSP tail keeps
      // sounding regardless (hosts do not gate our audio). The re-press guard
      // below still covers the one residual ordering hazard: an off and a
      // re-press of the SAME key landing in the same block.
      const bool dead = spectraMode() ? !spectra.voiceAt(i).gate : !core.voiceAt(i).gate;
      if (!dead) continue;
      // RE-PRESS GUARD (2026-07-31, the stuck-note bug): if this key+channel is
      // still HELD in another slot, do NOT end it yet. Hosts without real note
      // ids (Live via the VST3/AU wrappers sends note_id -1) match NOTE_END by
      // key+channel, so ending the DYING old instance of a re-pressed key
      // poisons the wrapper's bookkeeping for the NEW held instance — its
      // eventual note-off is swallowed and the gate sticks on forever. Fast
      // typing re-presses keys inside the previous release tail constantly
      // ("almost every note is getting stuck", poly + computer keyboard); a
      // piano roll never overlaps a key with its own tail, which is why it was
      // immune. Deferring is safe for id-matching hosts too: the END still
      // fires once the LAST instance of the key dies.
      bool keyStillHeld = false;
      for (int j = 0; j < hypersaw::kPoly; j++)
      {
        if (j == i || !tags[j].active) continue;
        if (tags[j].key != tags[i].key || tags[j].channel != tags[i].channel) continue;
        const bool jDead = spectraMode()
                               ? (!spectra.voiceAt(j).gate && spectra.voiceAt(j).env < 1e-4)
                               : (!core.voiceAt(j).gate && core.voiceAt(j).env < 1e-4);
        if (!jDead) { keyStillHeld = true; break; }
      }
      if (keyStillHeld) continue;
      clap_event_note_t ev{};
      ev.header.size = sizeof(ev);
      ev.header.time = time;
      ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      ev.header.type = CLAP_EVENT_NOTE_END;
      ev.note_id = tags[i].noteId;
      ev.port_index = tags[i].port;
      ev.channel = tags[i].channel;
      ev.key = tags[i].key;
      ev.velocity = 0;
      // Same rule: only retire the tag once the host has ACCEPTED the end.
      // A rejected push leaves the tag active so the next block tries again —
      // the note is resolved late rather than never.
      if (out->try_push(out, &ev.header)) tags[i].active = false;
    }
  }

  void enqueueParam(uint32_t id, double value, uint8_t kind)
  {
    const uint32_t head = qHead.load(std::memory_order_relaxed);
    if (head - qTail.load(std::memory_order_acquire) >= kQCap) return;  // drop on overflow
    queue[head % kQCap] = {id, value, kind};
    qHead.store(head + 1, std::memory_order_release);
    if (hostParams && hostParams->request_flush) hostParams->request_flush(host);
  }

  void drainQueue(const clap_output_events_t *out)
  {
    uint32_t tail = qTail.load(std::memory_order_relaxed);
    const uint32_t head = qHead.load(std::memory_order_acquire);
    while (tail != head)
    {
      const ParamMsg &m = queue[tail % kQCap];
      if (m.kind == 0 || m.kind == 3)   // 3 = a LOAD's value (B125): bypasses corner routing
      {
        loadingState = m.kind == 3;
        editorWrite = m.kind == 0;
        applyParam(m.id, m.value);
        loadingState = false;
        editorWrite = false;
        if (out)
        {
          clap_event_param_value_t ev{};
          ev.header.size = sizeof(ev);
          ev.header.time = 0;
          ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
          ev.header.type = CLAP_EVENT_PARAM_VALUE;
          ev.param_id = m.id;
          ev.cookie = nullptr;
          ev.note_id = -1;
          ev.port_index = -1;
          ev.channel = -1;
          ev.key = -1;
          ev.value = m.value;
          out->try_push(out, &ev.header);
        }
      }
      else
      {
        /* B89 phase 2d: the performance pad's drag bracket, read HERE because
           this is where gesture messages already arrive on the audio thread,
           in order with the values they bracket — a second path would be a
           second clock. Unconditional: with the intent flag off nothing reads
           the latch, so this cannot move a sample. */
        intentNoteGesture((clap_id)m.id, m.kind == 1);
        if (out)
        {
          clap_event_param_gesture_t ev{};
          ev.header.size = sizeof(ev);
          ev.header.time = 0;
          ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
          ev.header.type =
              m.kind == 1 ? CLAP_EVENT_PARAM_GESTURE_BEGIN : CLAP_EVENT_PARAM_GESTURE_END;
          ev.param_id = m.id;
          out->try_push(out, &ev.header);
        }
      }
      tail++;
    }
    qTail.store(tail, std::memory_order_release);
  }

  /* B106: the per-oscillator carpet block, addressed by INDEX. Filled last, at
     both publish exits, because the engine branches above wipe the snapshot
     (`v = VizSnapshot{}`) and a block written before them would be silently
     erased. In SPECTRA mode oscillator 0's pane carries the partial-0 cloud —
     the same phases the active block gets — because that is what is actually
     sounding; the swarm cores are not, so the other pane reads inactive rather
     than animating a swarm nobody can hear. */
  void fillOscPanes(hypersaw::VizSnapshot &v)
  {
    for (int k = 0; k < hypersaw::VizSnapshot::kVizOsc; k++)
    {
      v.oscOn[k] = (uint32_t)k < kNumOsc && oscEnabled[k] != 0;
      v.oscActive[k] = false;
      v.oscN[k] = 0;
      v.oscTopo[k] = 0;
      v.oscF0[k] = 0;
      if ((uint32_t)k >= kNumOsc) continue;
      if (spectraMode())
      {
        const auto *fs = k == 0 ? spectra.focus() : nullptr;
        if (!fs) continue;
        v.oscActive[k] = true;
        v.oscN[k] = (int)spectra.p.cloud;
        for (int i = 0; i < v.oscN[k] && i < 32; i++) v.oscPhase[k][i] = fs->phase[i];
        continue;
      }
      const auto *s = cores[k].focus();
      if (!s) continue;
      v.oscActive[k] = true;
      v.oscN[k] = (int)cores[k].p.n;
      v.oscTopo[k] = (int)cores[k].p.topo;
      v.oscF0[k] = s->f0cur * cores[k].p.tune;
      for (int i = 0; i < v.oscN[k] && i < 32; i++) v.oscPhase[k][i] = s->phase[i];
    }
  }

  void publishViz()
  {
    // THE INTERMEDIARY (human, 2026-08-07): every per-swarm visual reads the
    // oscillator the GUI is editing, not oscillator 0. Slot indices stay
    // aligned across cores because noteOn/noteOff fan out in order.
    const uint32_t vo = vizOsc.load(std::memory_order_relaxed);
    hypersaw::SwarmCore &vc = cores[vo < kNumOsc ? vo : 0];
    const int writeIdx = 1 - vizPublished.load(std::memory_order_relaxed);
    hypersaw::VizSnapshot &v = vizBuf[writeIdx];
    v.oscEnabled = oscEnabled[vo < kNumOsc ? vo : 0] != 0;
    // NOTE MONITOR — built here, BEFORE the engine branch, and from whichever
    // engine is sounding. It used to live inside the SAW-only path after the
    // SPECTRA early-return, so in SPECTRA mode there was no monitor AT ALL and
    // a stuck voice there was invisible by construction (human, 2026-08-03:
    // "a properly stuck note that isn't expressing on the notes tab").
    int nm = 0;
    for (int i = 0; i < hypersaw::kPoly && nm < 16; i++)
    {
      const int gate = spectraMode() ? spectra.voiceAt(i).gate : vc.voiceAt(i).gate;
      const double env = spectraMode() ? spectra.voiceAt(i).env : vc.voiceAt(i).env;
      // 1e-9, not 1e-4: the render skip-test also uses 1e-4, so a voice just
      // under it was invisible to the monitor while still being rendered. The
      // human hit a note that was audible and ABSENT from the tab (2026-08-03,
      // SAW, no FX), so the monitor must never be the thing that is silent.
      if (!gate && env < 1e-9) continue;
      v.nmMidi[nm] = spectraMode() ? spectra.voiceAt(i).midi : vc.voiceAt(i).midi;
      v.nmGate[nm] = gate;
      v.nmEnv[nm] = env;
      nm++;
    }
    v.nmCount = nm;
    // OUTPUT PEAK, published alongside the monitor. If sound continues while
    // this reads silence, the plugin is not the source — a question that has
    // cost real debugging time twice now and should be answerable at a glance.
    v.outPeak = outPeakViz;
    outPeakViz = 0;
    for (uint32_t k = 0; k < kNumOsc && k < 4; k++)
    {
      v.oscPeak[k] = oscPeakViz[k];
      oscPeakViz[k] = 0;
    }
    if (spectraMode())
    {
      // SPECTRA viz: partial-0's cloud drives the phase circle (v.R/psi/phase),
      // and the per-partial strip feed (v.partR/partAmp/partPhase) carries the
      // whole harmonic series — the cascade lock-front made visible.
      const auto *fs = spectra.focus();
      { const int keep = v.nmCount;
        int km[16]; int kg[16]; double ke[16];
        for (int i = 0; i < keep; i++) { km[i] = v.nmMidi[i]; kg[i] = v.nmGate[i]; ke[i] = v.nmEnv[i]; }
        v = hypersaw::VizSnapshot{};
        v.nmCount = keep;
        for (int i = 0; i < keep; i++) { v.nmMidi[i] = km[i]; v.nmGate[i] = kg[i]; v.nmEnv[i] = ke[i]; } }
      if (fs)
      {
        v.active = true;
        v.spectra = true;
        const int P = (int)spectra.p.partials, M = (int)spectra.p.cloud;
        v.partials = P;
        v.cloud = M;
        v.n = M;
        v.R = fs->R[0];
        v.psi = fs->psi[0];
        v.sigma = fs->sigma[0];
        v.KsmS = fs->KsmS[0];
        v.KsmP = fs->KsmP[0];
        for (int i = 0; i < M && i < 32; i++) v.phase[i] = fs->phase[i];
        for (int k = 0; k < P && k < 32; k++)
        {
          v.partR[k] = fs->R[k];
          v.partAmp[k] = spectra.partialAmp(k);
          for (int m = 0; m < M && m < 7; m++)
            v.partPhase[k * 7 + m] = fs->phase[k * hypersaw::SpectraCore::kMMax + m];
        }
      }
      fillOscPanes(v);
      vizPublished.store(writeIdx, std::memory_order_release);
      return;
    }
    const auto *s = vc.focus();
    if (!s)
    {
      v = hypersaw::VizSnapshot{};
    }
    else
    {
      v.active = true;
      v.n = (int)vc.p.n;
      v.centerIdx = vc.centerIndex();
      v.R = s->R;
      v.RN = s->RN;
      v.psi = s->psi;
      v.sigma = s->sigma;
      v.KsmS = s->KsmS;
      v.KsmP = s->KsmP;
      for (int i = 0; i < v.n && i < 32; i++) v.phase[i] = s->phase[i];
      // voice map: focus swarm's placement vs actual, plus its pan seats
      v.sampleRate = sampleRate;
      /* Voice map centre includes the oscillator's transpose (human 2026-08-21:
         "the voice map should normalize to whatever the offset is"). vf/eff are
         POST-tune (render multiplies f0cur * tune), so an untransposed centre
         drew the whole cloud off-axis by exactly oct+semi+fine. p.tune carries
         the full factor (incl. bend and global transpose), which keeps the map
         centred during bends too. */
      v.vmF0 = s->f0cur * vc.p.tune;
      for (int i = 0; i < v.n && i < 32; i++)
      {
        v.vmVf[i] = s->vf[i];
        v.vmEff[i] = s->eff[i];
        v.vmPan[i] = vc.panEffAt(i);
      }
      // Per-voice envelope shape (ADR-077/078 scatter made visible). Coefficients
      // are one-poles, so the time constant is the inverse of the derivation in
      // the core: c = 1 - exp(-1/(t*sr))  ->  t = -1/(sr*ln(1-c)). Published
      // from the coefficients the core is ACTUALLY using, so the display cannot
      // disagree with the sound.
      {
        const bool perVoice = vc.p.onsetScatter > 0 || vc.p.voiceEnv > 0.5;
        v.envCount = perVoice ? (v.n < 32 ? v.n : 32) : 0;
        const auto tOf = [&](double c) {
          return (c > 0 && c < 1) ? -1000.0 / (sampleRate * std::log(1.0 - c)) : 0.0;
        };
        for (int i = 0; i < v.envCount; i++)
        {
          v.envOnsetMs[i] = s->onsD0[i] / sampleRate * 1000.0;
          v.envAtkMs[i] = tOf(s->onsC[i]);
          v.envRelMs[i] = tOf(s->relC[i]);
        }
      }
      // dynamics layer
      v.topo = (int)vc.p.topo;
      v.poles = (int)vc.p.poles;
      v.RA = s->RA;
      v.RB = s->RB;
      v.RQ = s->RQ;
      v.gravCount = vc.gravCount < 4 ? vc.gravCount : 4;
      for (int i = 0; i < v.gravCount; i++)
      {
        v.gravRatio[i] = vc.gravPairs[i][0];
        v.gravOct[i] = vc.gravPairs[i][1];
        v.gravErr[i] = vc.gravErr[i];
      }
      // note monitor: every slot, gated or ringing
      // grid status (ADR-016/017): unit, occupied rungs, cause-AND-state lock
      v.gridActive = ((int)vc.p.law == 3);
      if (v.gridActive)
      {
        v.gridU = (vc.p.bpm / 60.0) * vc.p.beatMult;
        int rungCount = 0;
        double seen[32];
        for (int i = 0; i < v.n && i < 32; i++)
        {
          const double rung = std::round((s->vf[i] - s->f0cur * vc.p.tune) / v.gridU);
          bool dup = false;
          for (int j = 0; j < rungCount; j++)
            if (seen[j] == rung) dup = true;
          if (!dup && rungCount < 32) seen[rungCount++] = rung;
        }
        v.gridRungs = rungCount;
        const bool coupled = s->KsmS > 0.05;
        const bool coherent = s->R > 0.8 || s->RQ > 0.8 || (v.topo == 2 && s->RA > 0.8 && s->RB > 0.8);
        v.gridLockWarn = coupled && coherent;
      }
    }
    fillOscPanes(v);
    vizPublished.store(writeIdx, std::memory_order_release);
  }

  // GUI-thread spectrum: last 2048 ring samples, Hann, radix-2 FFT, then
  // 96 log-spaced bins 30 Hz..16 kHz normalized from a -80 dB floor.
  void computeSpectrum(float *out, int nBins)
  {
    constexpr int N = 2048;
    static thread_local double re[N], im[N];
    const uint32_t w = specPos.load(std::memory_order_acquire);
    for (int i = 0; i < N; i++)
    {
      const double hann = 0.5 - 0.5 * std::cos(2 * 3.141592653589793 * i / N);
      re[i] = (double)specRing[(w - N + i) & 4095] * hann;
      im[i] = 0;
    }
    // iterative radix-2
    for (int i = 1, j = 0; i < N; i++)
    {
      int bit = N >> 1;
      for (; j & bit; bit >>= 1) j ^= bit;
      j ^= bit;
      if (i < j)
      {
        std::swap(re[i], re[j]);
        std::swap(im[i], im[j]);
      }
    }
    for (int len = 2; len <= N; len <<= 1)
    {
      const double ang = -2 * 3.141592653589793 / len;
      const double wr = std::cos(ang), wi = std::sin(ang);
      for (int i = 0; i < N; i += len)
      {
        double cr = 1, ci = 0;
        for (int k = 0; k < len / 2; k++)
        {
          const double ur = re[i + k], ui = im[i + k];
          const double vr = re[i + k + len / 2] * cr - im[i + k + len / 2] * ci;
          const double vi = re[i + k + len / 2] * ci + im[i + k + len / 2] * cr;
          re[i + k] = ur + vr;
          im[i + k] = ui + vi;
          re[i + k + len / 2] = ur - vr;
          im[i + k + len / 2] = ui - vi;
          const double ncr = cr * wr - ci * wi;
          ci = cr * wi + ci * wr;
          cr = ncr;
        }
      }
    }
    const double binHz = sampleRate / N;
    for (int b = 0; b < nBins; b++)
    {
      const double f = 30.0 * std::pow(16000.0 / 30.0, (double)b / (nBins - 1));
      int bin = (int)(f / binHz);
      if (bin < 1) bin = 1;
      if (bin > N / 2 - 1) bin = N / 2 - 1;
      const double mag = std::hypot(re[bin], im[bin]) / (N / 4);
      const double db = 20 * std::log10(mag + 1e-9);
      double v = (db + 80.0) / 80.0;
      out[b] = (float)(v < 0 ? 0 : (v > 1 ? 1 : v));
    }
  }

  // Defaults for EVERY id, same shape and same loop as paramsJson so the two
  // cannot disagree about which ids exist. This is what makes the defaults
  // survive the GUI: they live in the shell and are served to whatever asks —
  // webview today, anything else later — rather than living in HTML attributes
  // that vanish with the markup.
  std::string defaultsJson() const
  {
    std::string out = "{";
    char buf[48];
    for (uint32_t k = 0; k < kNumOsc; k++)
      for (const auto &d : kParams)
      {
        if (k > 0 && isGlobalId(d.id)) continue;
        const clap_id id = (clap_id)(d.id + k * kOscStride);
        std::snprintf(buf, sizeof(buf), "%s\"%u\":%.6g", out.size() > 1 ? "," : "", id,
                      defaultFor(d, k));
        out += buf;
      }
    // ADR-088 routing cells. Their default IS the series chain, read off a
    // default-constructed matrix — so the pane's double-click-to-default and a
    // host's "reset to defaults" both restore 1 -> 2 -> 3 -> 4 and nothing else.
    for (const auto &d : g_routingTable.defs)
    {
      std::snprintf(buf, sizeof(buf), ",\"%u\":%.6g", (unsigned)d.id, d.defV);
      out += buf;
    }
    // B172 engine blocks — same loop as paramsJson so the two cannot disagree
    // about which ids exist.
    for (const auto &b : kEngineBlocks)
      for (uint32_t i = 0; i < b.count; i++)
      {
        std::snprintf(buf, sizeof(buf), ",\"%u\":%.6g", (unsigned)b.defs[i].id, b.defs[i].defV);
        out += buf;
      }
    out += "}";
    return out;
  }


  /* BEND CURVES FOR THE GUI. Computed HERE, by the shipped GlideCore, rather than
     by a JavaScript twin of the laws: a second implementation of a trajectory is
     a second thing to keep in step, and the whole point of the graph is to show
     what the instrument actually does. Runs on the GUI thread via the bridge —
     never the audio thread — so allocating a scratch core is fine.
     Both simulations are the bench's, so the picture in the plugin and the
     picture in bend-lab.html answer the same question the same way:
       · STEP    — +range held from 0.05 s to 0.65 s of a 1.4 s window, which is
                   where the laws visibly differ.
       · WOBBLE  — a sine on the wheel, measured at the FUNDAMENTAL. Rate limiting
                   is nonlinear, so this is the first harmonic rather than the
                   whole story — but it is the part heard as depth, and it is the
                   bill inertia charges for a slow bend. */
  /* ADR-101: one cycle of the EDITED oscillator's waveform, drawn by the same
     stage chain the render runs — the reference anchor functions, the ADR-058
     squareness morph — never by a JS twin (the bend graphs' rule, ADR "drawn by
     the engine"). Display honesty notes: the ideal saw stands in for the BLEPped
     one (band-limiting is inaudible to the eye at this size), and roundness is
     shown at its knob value — the per-voice roundHi pitch scaling varies by
     note, which a single static cycle cannot show. */
  /* ---- THE SUB'S CYCLE, DRAWN BY THE ENGINE (B181 note 3) -----------------
     The human: "Sub needs a visualizer to see the shape."

     PUBLISHED, NOT RE-DERIVED IN JS, and that choice is the whole design. The
     GUI already computes the LFO shape independently of the shell (B177) and
     this repo has a recorded scar for the same mistake (ADR-110: "when they
     were two copies, any edit to one was a map that lied about the sound"). A
     display that disagrees with the sound is a confident wrong answer, and
     the only structural cure is that there is nothing to disagree WITH — so
     this calls SubOscCore::shapeAt, which is the SAME function render() calls
     per sample, and the GUI draws the numbers it is handed.

     THE BAND-LIMITING IS REAL. `dph` is the core's own running phase
     increment, so the polyBLEP correction in the drawing is the correction in
     the sound: a pulse at a high note visibly rounds its edges, exactly as it
     does audibly. The bump's normaliser comes from the core's own bounded
     search, so the two-lobe silhouette is drawn at the amplitude it sounds at.

     Noise draws from a LOCAL stream seeded with `seed`, which is the first N
     draws a note-on would make (the core re-seeds per note — audit A3). It is
     a sample of the stream, not a second stream.

     Instance 0 is the reader, as subGetParam's is: the parameters are
     per-device, so all sixteen hold the same table. */
  std::string subWaveJson() const
  {
    using Core = hypersaw::SubOscCore;
    const Core &c = subs[0];
    constexpr int N = 256;
    const int wave = (int)c.param(Core::kWave);
    const double sr = c.sampleRate();
    const double dph = std::min(c.freqHz(), 0.49 * sr) / sr;
    const double w = c.param(Core::kWidth);
    const double bumpA = c.param(Core::kBumpAmt), bumpPhi = c.param(Core::kBumpPhase);
    uint32_t rng = (uint32_t)c.param(Core::kSeed);
    // The header needs its own buffer: it is ~40 characters wide and a char[32]
    // truncated it mid-key, which produced JSON the reader parsed as "no
    // points" rather than as an error. Caught by subosc_check's 11i rows on
    // their first run — the reason a display gets an oracle at all.
    char hdr[96], buf[32];
    std::snprintf(hdr, sizeof hdr, "{\"n\":%d,\"wave\":%d,\"hz\":%.4f,\"wave_pts\":[", N, wave,
                  c.freqHz());
    std::string out = hdr;
    for (int i = 0; i < N; i++)
    {
      // The start phase is where a note begins, so the drawing begins there
      // too — moving `phase` visibly rotates the cycle, which is what the
      // control does to the sound.
      double ph = c.param(Core::kPhase) + (double)i / N;
      ph -= std::floor(ph);
      const double v = Core::shapeAt(wave, ph, dph, w, bumpA, bumpPhi, c.bumpNorm(), rng);
      std::snprintf(buf, sizeof(buf), i ? ",%.5f" : "%.5f", v);
      out += buf;
    }
    return out + "]}";
  }

  /* B177 note (2026-09-21) — ONE CYCLE OF EACH LFO, AND THE SHELL DRAWS IT.
     B177 shipped the MOD page's LFO pictures with a JS transcription of
     `lfoShapeAt` in gui2.html: two copies of one law, nothing holding them
     together, so an edit to the switch above would have left the drawing
     confidently wrong. Same exposure ADR-110 records for the bend curve, and
     the same cure B181 note 3 applied to the SUB — publish the cycle from the
     function that MAKES THE SOUND (the mod tick calls this same `lfoShapeAt`
     at line ~3744) and delete the twin.

     N = 129 points at ph = i/128, which is exactly the grid the deleted JS
     drew on, so the picture is unchanged for every shape but S&H.

     S&H IS THE PART THAT GOT MORE HONEST. The GUI showed a fixed eight-value
     display list labelled "illustrative" because it could not reach the
     engine's stream. `sh` here is the FIRST EIGHT DRAWS of this LFO's own
     seeded stream — lfoSeed(i), the stream `lfoReseed` installs — advanced on
     a LOCAL copy of the state, so publishing a picture can never perturb the
     sound. One value per wrap is the tick loop's law, so eight values are
     eight cycles: the steps are the ones the patch seed will actually
     produce, not a stand-in for them.

     Free of the audio thread: called from the GUI bind, reads plain doubles. */
  std::string lfoCycleJson() const
  {
    constexpr int N = 128;        // segments; N+1 points, the GUI's old grid
    constexpr int kShSteps = 8;   // wraps shown for S&H, the old display list's length
    char buf[32];
    std::string out = "{\"lfo\":[";
    for (int i = 0; i < kNumLfo; i++)
    {
      if (i) out += ',';
      std::snprintf(buf, sizeof buf, "{\"shape\":%d,\"pts\":[", lfoShape[i]);
      out += buf;
      for (int k = 0; k <= N; k++)
      {
        const double v = lfoShapeAt(lfoShape[i], (double)k / N, lfo[i].sh);
        /* SEVEN decimals, not the five a 300-pixel canvas needs: lfoenv_check's
           E2 walks the live source against these points, so the transport's own
           quantisation is the floor of what that gate can resolve. At %.5f the
           residual was 5e-6 — the printf, not the law — and a tolerance written
           to absorb it would have absorbed a real divergence of the same size. */
        std::snprintf(buf, sizeof buf, k ? ",%.7f" : "%.7f", v);
        out += buf;
      }
      out += "],\"sh\":[";
      uint32_t rng = lfoSeed(i);          // a COPY: the live stream is untouched
      for (int k = 0; k < kShSteps; k++)
      {
        const double v = 2.0 * forcecore::rngNext(rng) - 1.0;
        std::snprintf(buf, sizeof buf, k ? ",%.7f" : "%.7f", v);
        out += buf;
      }
      out += "]}";
    }
    return out + "]}";
  }

  std::string shapeWaveJson()
  {
    const uint32_t vo = vizOsc.load(std::memory_order_relaxed);
    const hypersaw::Params &q = cores[vo < kNumOsc ? vo : 0].p;
    constexpr int N = 256;
    // rEff parameterised so the roundHi SILHOUETTES (below) run the same stage
    // chain at the per-voice extremes instead of a JS twin approximating them.
    auto stage = [&](double ph, double rEff) {
      double v = 2 * ph - 1;
      if (q.sawBase > 0.001)
      {
        const double f = std::max(0.0, std::min(4.0, q.sawBase * 4));
        const int i0 = std::min(3, (int)std::floor(f));
        const double fr = f - i0;
        const double b0 = i0 == 0 ? v : hypersaw::sawBaseAnchor(i0, ph);
        v = b0 * (1 - fr) + hypersaw::sawBaseAnchor(i0 + 1, ph) * fr;
      }
      if (rEff > 0.001)
      {
        const double f = std::max(0.0, std::min(4.0, q.sawProfile * 4));
        const int i0 = std::min(3, (int)std::floor(f));
        const double fr = f - i0;
        const double sh = hypersaw::sawShapeAnchor(i0, ph) * (1 - fr)
                        + hypersaw::sawShapeAnchor(i0 + 1, ph) * fr;
        v = v * (1 - rEff) + sh * rEff;
      }
      return v;
    };
    auto emit = [&](std::string &out, double rEff) {
      char buf[32];
      for (int i = 0; i < N; i++)
      {
        const double ph = (double)i / N;
        double v = stage(ph, rEff);
        if (q.shape > 0.001)                    // ADR-058: v = w - shape*w(ph+1/2)
          v -= q.shape * stage(ph >= 0.5 ? ph - 0.5 : ph + 0.5, rEff);
        std::snprintf(buf, sizeof(buf), i ? ",%.4f" : "%.4f", v);
        out += buf;
      }
    };
    std::string out = "{\"wave\":[";
    emit(out, q.round);
    // ROUND x PITCH silhouettes (human 2026-08-25): when roundHi spreads the
    // per-voice roundness, also send the shape at BOTH extremes of the spread
    // -- swarm_core.h:1524, rnd = clamp01(round*(1 + roundHi*(2*up - 1))), so
    // up=0 and up=1 give the lowest- and highest-pitch voices' shapes. Emitted
    // only when the spread is live, so the GUI keys on presence.
    if (q.round > 0.001 && std::fabs(q.roundHi) > 0.001)
    {
      const auto c01 = [](double x){ return std::max(0.0, std::min(1.0, x)); };
      out += "],\"lo\":["; emit(out, c01(q.round * (1 - q.roundHi)));
      out += "],\"hi\":["; emit(out, c01(q.round * (1 + q.roundHi)));
    }
    out += "]}";
    return out;
  }

  std::string bendCurveJson() const
  {
    const double cr = 1.0 / kBendGridSeconds;          // ticks per second
    const double A = 2.0;                              // +2 semitones, the bench's range
    hypersaw::GlideCore::Params lp = bendLaw;
    lp.scaleRoot = scale.root;
    for (int d = 0; d < 12; d++) lp.scaleMask[d] = scale.mask[d];

    const int N = (int)(1.4 * cr), t0 = (int)(0.05 * cr), t1 = (int)(0.65 * cr);
    constexpr int kPts = 240;                          // enough for a 316 px canvas
    std::string traj = "[", tgts = "[";
    double lag50 = -1, peak = 0, prevSgn = 0;
    int settleIdx = -1, rings = 0;
    hypersaw::GlideCore g(cr, true);
    g.reset(0);
    for (int i = 0; i < N; i++)
    {
      const double t = (i >= t0 && i < t1) ? A : 0.0;
      const double x = g.step(t, lp);
      if (i >= t0 && i < t1)
      {
        const double e = x - A;
        if (lag50 < 0 && std::fabs(x) >= 0.5 * std::fabs(A)) lag50 = (i - t0) / cr * 1000.0;
        if (std::fabs(x) > std::fabs(peak)) peak = x;
        if (std::fabs(e) > 0.05) settleIdx = i;
        if (std::fabs(e) > 0.02)
        {
          const double sg = e < 0 ? -1.0 : 1.0;
          if (prevSgn != 0 && sg != prevSgn) rings++;
          prevSgn = sg;
        }
      }
      if (i % (N / kPts + 1) == 0)
      {
        char b[40];
        std::snprintf(b, sizeof(b), "%s%.4g", traj.size() > 1 ? "," : "", x);
        traj += b;
        std::snprintf(b, sizeof(b), "%s%.4g", tgts.size() > 1 ? "," : "", t);
        tgts += b;
      }
    }
    traj += "]"; tgts += "]";
    const double over = std::max(0.0, (std::fabs(peak) - std::fabs(A)) * 100.0);
    const bool never = settleIdx >= t1 - 2;
    const double settle = never ? -1.0 : (settleIdx < 0 ? 0.0 : (settleIdx - t0 + 1) / cr * 1000.0);

    // vibrato cost: one-bin DFT at the wobble rate, target and actual
    const double f = 5.0, TAU = 6.283185307179586, WA = A * 0.5;
    const int NW = (int)(2.0 * cr), startW = (int)(1.0 * cr);
    double reX = 0, imX = 0, reT = 0, imT = 0;
    hypersaw::GlideCore gw(cr, true);
    gw.reset(0);
    for (int i = 0; i < NW; i++)
    {
      const double ph = TAU * f * i / cr;
      const double t = WA * std::sin(ph);
      const double x = gw.step(t, lp);
      if (i >= startW)
      {
        const double c = std::cos(ph), sn = std::sin(ph);
        reX += x * c; imX -= x * sn; reT += t * c; imT -= t * sn;
      }
    }
    const double magX = std::hypot(reX, imX), magT = std::hypot(reT, imT);
    double d = std::atan2(imX, reX) - std::atan2(imT, reT);
    while (d > 3.141592653589793) d -= TAU;
    while (d < -3.141592653589793) d += TAU;

    char out[256];
    std::snprintf(out, sizeof(out),
                  "{\"lag50\":%.4g,\"over\":%.4g,\"settle\":%.4g,\"rings\":%d,"
                  "\"depth\":%.4g,\"wlag\":%.4g,\"span\":%.4g,\"amp\":%.4g,",
                  lag50 < 0 ? 0.0 : lag50, over, settle, rings,
                  magT > 0 ? magX / magT * 100.0 : 0.0, -d / (TAU * f) * 1000.0, 1.4, A);
    return std::string(out) + "\"traj\":" + traj + ",\"tgt\":" + tgts + "}";
  }

  std::string paramsJson() const
  {
    // ADR-082: emit EVERY oscillator's block, not just oscillator 0. Without
    // this the GUI cannot see — let alone edit — the second oscillator, which
    // is the whole point of increment 2.
    //
    // It also lets the GUI DERIVE which params are global instead of carrying
    // a copy of kGlobalIds: a base id with no `+kOscStride` sibling in this
    // JSON is global. A hand-maintained second list would drift from this one
    // within a release, and the drift would be silent — the GUI would simply
    // edit the wrong oscillator.
    std::string out = "{";
    char buf[48];
    for (uint32_t k = 0; k < kNumOsc; k++)
      for (const auto &d : kParams)
      {
        if (k > 0 && isGlobalId(d.id)) continue;   // globals exist once
        const clap_id id = (clap_id)(d.id + k * kOscStride);
        std::snprintf(buf, sizeof(buf), "%s\"%u\":%.6g", out.size() > 1 ? "," : "", id,
                      readParam(id));
        out += buf;
      }
    /* ADR-088: the routing cells ride the SAME snapshot every other parameter
       does, so the matrix pane repaints on the existing poll and needs no bind
       of its own (ADR-143: one hzFrame per frame, and this is not on it). It is
       also how the pane learns WHICH cells exist — it decodes the ids rather
       than carrying a hand-typed list of crosspoints.
       Safe for learnOscLayout: these ids are all >= 10000, so they are outside
       both `i < OSC_STRIDE` (the globals test) and `floor(i/1000) == numOsc`
       (the oscillator count walk, which stops at 2). */
    for (const auto &d : g_routingTable.defs)
    {
      std::snprintf(buf, sizeof(buf), ",\"%u\":%.6g", (unsigned)d.id, readParam(d.id));
      out += buf;
    }
    /* B172 engine blocks, on the SAME snapshot for the same reason the routing
       cells are: one poll feeds every pane. Safe for learnOscLayout — these
       ids are >= 3000, so `i < OSC_STRIDE` (the globals test) is false and
       `floor(i/1000) == numOsc` never reaches them (the walk stops at 2). */
    for (const auto &b : kEngineBlocks)
      for (uint32_t i = 0; i < b.count; i++)
      {
        std::snprintf(buf, sizeof(buf), ",\"%u\":%.6g", (unsigned)b.defs[i].id,
                      readParam(b.defs[i].id));
        out += buf;
      }
    return out + "}";
  }

  /* Corner persistence (ADR-104): a corner IS patch data. Values ride in
     morphIds order (id-ascending by construction), which is stable for a given
     schema; adding params later lengthens the list, and the schema bump is the
     signal to re-derive. Absent section = no corners captured (fresh corners
     hold defaults). */
  /* One corner as JSON, and its inverse — the corner-preset surface (ADR-105).
     Same order contract as morphJson: morphIds order, append-only. */
  std::string cornerJson(int k)
  {
    if (k < 0 || k > 3) return "{}";
    morphInit();
    /* THE LAYOUT MARKER, BUMPED ONCE HERE AND AT THE OTHER THREE WRITERS.
       9 = B203: the engine blocks' GATES join the field, appended after both
       of B195's passes (morphInit's third pass), so the corner array grew by
       one per block and the order changed. Same reasoning as 8 below in every
       respect: a layout-8 array is shorter, maps 1:1, and the new slot holds
       its default — the bump NAMES the order, it does not migrate anything.
       8 = B195: the engine blocks' STRUCTURAL rows join the field, appended
       after their block's morphable ones (morphInit), so the corner array grew
       by eight and the order changed. A layout-7 array is shorter and maps 1:1
       (morphSlotMap), so the eight new slots simply hold their defaults —
       which is why the bump is a marker and not a migration, and why NOTHING
       stored moves. The bump is not needed to READ a layout-7 array correctly
       (morphSlotMap treats every layout >= 2 as a 1:1 prefix); it is taken
       because the marker's job is to NAME AN ORDER, and B175's cross-layout
       remap will have to ask which order an array was written in. The factory
       bank re-saves either way — its corner arrays are eight entries longer.
       7 = B181: the SUB block's two new MORPHABLE shell rows (sub.glide,
       sub.pitchMod) append after everything, so the corner array grew by two
       and the order changed.
       6 = B172: the SUB OSC engine block's morphable ids appended after the
       routing block. STATION (B162) appends into this SAME layout and will
       bump it again — the marker names an ORDER, and every append changes the
       order, so one bump per appending change is the rule, not one bump per
       engine family. 5 = the ADR-088-amendment routing renumbering (B23
       increment 3: source rows reserved, Src 2's cells new slot positions);
       4 = the Src→OUT dry-path cells appended after the routing block (B50
       phase 1c); 3 = the routing block (phase 1). */
    std::string out = "{\"morphLayout\":9,\"cornerPreset\":[";
    char buf[32];
    for (size_t i = 0; i < morphIds.size(); i++)
    {
      std::snprintf(buf, sizeof(buf), i ? ",%.6g" : "%.6g", morphCorner[k][i]);
      out += buf;
    }
    return out + "]}";
  }
  bool cornerApply(int k, const std::string &json)
  {
    if (k < 0 || k > 3) return false;
    morphInit();
    size_t cp = json.find("\"cornerPreset\"");
    if (cp == std::string::npos) return false;
    const char *c = std::strchr(json.c_str() + cp, '[');
    if (!c) return false;
    // ADR-159: a corner-preset FILE is the same positional array as a corner
    // chunk, so it takes the same remap (the human's corners/*.json, Aug 21-30).
    const std::vector<size_t> map = morphSlotMap(parseMorphLayout(json), countArray(c));
    resetCorner(k);   // B124
    c++;
    morphCornersAuthored = true;
    for (size_t j = 0; j < map.size(); j++)
    {
      if (map[j] != SIZE_MAX) morphCorner[k][map[j]] = std::atof(c);
      const char *nx = std::strchr(c, ',');
      const char *cl = std::strchr(c, ']');
      if (!nx || (cl && cl < nx)) break;
      c = nx + 1;
    }
    undoMarkCorner(k, "loaded", nullptr);   // B84 / B122: the GUI names it next
    return true;
  }

  // Both corner writes name the corner they touched; the tick tells two visits
  // to the same corner apart, so one label serves capture and apply alike.
  /* B122: corner preset NAMES live in the shell (human 2026-09-14: history
     "doesn't handle presets elegantly" — the name of what a corner holds was
     GUI-only, so an undo restored the values and left the dropdown lying).
     Stamped into the morph chunk, so a patch, a set and every history
     snapshot carry which preset each corner came from; "" = captured / none. */
  std::string cornerName[4];
  static const char *cornerLetter(int k) { static const char *L[] = {"A", "B", "C", "D"}; return L[k & 3]; }
  void undoMarkCorner(int k, const char *verb, const char *name)
  {
    char lb[96];
    if (name && *name) std::snprintf(lb, sizeof lb, "corner %s %s %s", cornerLetter(k), verb, name);
    else std::snprintf(lb, sizeof lb, "corner %s %s", cornerLetter(k), verb);
    undoMark(lb);
  }
  /* The GUI names the corner right after a load; the load's own mark is
     usually still pending (the snapshot waits for the next frame), so the
     label is amended in place and the node reads "corner B ← Squids", not
     "corner B loaded".

     USUALLY, NOT ALWAYS — and the else branch is B186's fix. The GUI names
     the corner in a SECOND webview round trip (gui2.html ~2956: `await
     hzMorphCornerApply` then `await hzMorphCornerName`), and hzFrame services
     the pending mark FIRST thing every frame (hypersaw_gui_common.h:487). A
     frame landing between those two awaits is not an exotic interleaving,
     just the one an `await` permits — and when it did, the load's node
     photographed the corner's new VALUES beside its OLD name, the name change
     was recorded nowhere, and the instrument stood on a node that did not
     hold its state. The first navigation away then reverted the dropdown to
     unnamed, which is the "history doesn't handle presets elegantly" shape
     B122 was supposed to have closed. A corner's name is patch state (B122 —
     it rides the morph chunk into every snapshot), so a name change with no
     pending mark to amend is an edit of its own and marks like one.
     UndoTree::push's dedup makes a rename to the same name cost nothing. */
  void setCornerName(int k, const std::string &name)
  {
    if (k < 0 || k > 3) return;
    cornerName[k] = name.substr(0, 60);
    const std::string label =
        cornerName[k].empty()
            ? std::string("corner ") + cornerLetter(k) + " captured"
            : std::string("corner ") + cornerLetter(k) + " \xe2\x86\x90 " + cornerName[k];   // "←" as UTF-8: labels show raw
    if (undoPending && undoPendingLabel.rfind("corner ", 0) == 0) undoPendingLabel = label;
    else undoMark(label.c_str());
  }
  std::string cornerNamesJson() const
  {
    std::string out = "[";
    for (int k = 0; k < 4; k++) { out += k ? ",\"" : "\""; out += jsonEscape(cornerName[k]); out += "\""; }
    return out + "]";
  }
  /* Would applying this corner-preset JSON change corner k? Runs the preset
     through the same layout remap the loader uses (ADR-159) and compares —
     the GUI's "edited since load" asterisk (human 2026-09-14). */
  bool cornerMatches(int k, const std::string &json)
  {
    if (k < 0 || k > 3) return false;
    morphInit();
    const size_t cp = json.find("\"cornerPreset\"");
    if (cp == std::string::npos) return false;
    const char *c = std::strchr(json.c_str() + cp, '[');
    if (!c) return false;
    const std::vector<size_t> map = morphSlotMap(parseMorphLayout(json), countArray(c));
    // B124: slots the preset does not carry load as defaults, so they must READ as defaults to match
    std::vector<char> carried(morphIds.size(), 0);
    for (size_t j = 0; j < map.size(); j++) if (map[j] != SIZE_MAX) carried[map[j]] = 1;
    for (size_t i = 0; i < morphIds.size(); i++)
      if (!carried[i] && std::fabs(morphCorner[k][i] - cornerSlotDefault(i)) > 1e-9) return false;
    c++;
    for (size_t j = 0; j < map.size(); j++)
    {
      if (map[j] != SIZE_MAX && std::fabs(morphCorner[k][map[j]] - std::atof(c)) > 1e-9) return false;
      const char *nx = std::strchr(c, ',');
      const char *cl = std::strchr(c, ']');
      if (!nx || (cl && cl < nx)) break;
      c = nx + 1;
    }
    return true;
  }

  /* ADR-105 A3: the LIVE settings as a corner preset, no capture required.
     "Requiring a corner to first be captured before the state can be saved is
     a little convoluted" (human 2026-08-21) -- the save serialises what is
     SOUNDING, and any corner can then load it. Same shape and order contract
     as cornerJson. */
  std::string liveCornerJson()
  {
    morphInit();
    std::string out = "{\"morphLayout\":9,\"cornerPreset\":[";   // ADR-159; 8 = B195, see cornerJson
    char buf[32];
    for (size_t i = 0; i < morphIds.size(); i++)
    {
      std::snprintf(buf, sizeof(buf), i ? ",%.6g" : "%.6g", readParam(morphIds[i]));
      out += buf;
    }
    return out + "]}";
  }

  /* ADR-112 A3: ONE parser for the morph chunk, called by BOTH state paths.
     The JSON preset path always carried the corners; the HOST session path
     (state_save/state_load) never did, so a DAW session restored every live
     param and silently dropped the field — all four corners lazily re-init
     to the restored live values, a degenerate field where the pad moves
     nothing ("sessions don't save the morph", human 2026-08-23). The writer
     (morphJson) and this parser stay adjacent twins on purpose: the JSON
     state-twins bug was two copies drifting apart. */
  /* B124 (human 2026-09-14: a corner "semi-permanently messed up until I
     reload the plugin"): a stored array SHORTER than the live order — every
     patch and corner preset saved before a later append (202-, 222-entry files
     beside 224-entry ones) — only overwrote the slots it carried, so the
     previous load's values survived in the rest: a pitch offset from one
     preset, or the whole bend/scale tail from a 202-entry file, outlived
     every later load. Before filling, every slot goes back to its default. */
  double cornerSlotDefault(size_t i) const
  {
    const ParamDef *d = findParam(morphIds[i]);
    return d ? defaultFor(*d, morphIds[i] / 1000) : 0.0;
  }
  void resetCorner(int k)
  {
    for (size_t i = 0; i < morphIds.size(); i++) morphCorner[k][i] = cornerSlotDefault(i);
  }
  /* ADR-159: where stored slot j lands in the live order. Layout 1 arrays of
     exactly kMorphAdr150Size were written with 181/1181 inside the prefix;
     every other layout-1 array (<= 222 entries) is the frozen prefix and maps
     1:1. Returns SIZE_MAX for a slot the live order does not carry. */
  std::vector<size_t> morphSlotMap(int layout, size_t storedLen) const
  {
    std::vector<size_t> map(storedLen);
    for (size_t j = 0; j < storedLen; j++) map[j] = j < morphIds.size() ? j : SIZE_MAX;
    if (layout >= 2 || storedLen != kMorphAdr150Size) return map;
    std::vector<clap_id> legacy = buildMorphOrder(/*lateInPrefix=*/true);
    // the legacy order is the live order with the late rows in the prefix and
    // nothing appended; its tail (FX, laws, globals, scale) follows the prefix
    // exactly as morphInit appends it, so rebuild it the same way.
    std::vector<clap_id> live = buildMorphOrder(false);
    const size_t livePrefix = live.size();
    for (size_t i = livePrefix; i < morphIds.size(); i++)
      if (!isMorphLateId(morphIds[i]) && !isMorphLateId(morphIds[i] >= 1000 ? morphIds[i] - 1000 : morphIds[i]))
        legacy.push_back(morphIds[i]);
    for (size_t j = 0; j < storedLen; j++)
    {
      map[j] = SIZE_MAX;
      if (j >= legacy.size()) continue;
      for (size_t i = 0; i < morphIds.size(); i++)
        if (morphIds[i] == legacy[j]) { map[j] = i; break; }
    }
    return map;
  }
  static int parseMorphLayout(const std::string &json)
  {
    const size_t lp = json.find("\"morphLayout\"");
    if (lp == std::string::npos) return 1;
    const size_t colon = json.find(':', lp);
    return colon == std::string::npos ? 1 : std::atoi(json.c_str() + colon + 1);
  }
  // Count the numbers in the bracketed array starting at `c` (which points at '[').
  static size_t countArray(const char *c)
  {
    const char *cl = std::strchr(c, ']');
    if (!cl || cl == c + 1) return 0;
    size_t n = 1;
    for (const char *q = c; q < cl; q++) n += (*q == ',');
    return n;
  }

  void applyMorphChunk(const std::string &json)
  {
    const int layout = parseMorphLayout(json);
    /* B122: corner names. Key present -> all four are set from it; absent (a
       patch from before names) -> all four cleared, so a stale name can never
       outlive the values it described. */
    for (int k = 0; k < 4; k++) cornerName[k].clear();
    {
      const size_t np = json.find("\"cornerNames\"");
      if (np != std::string::npos)
      {
        const char *c = std::strchr(json.c_str() + np + 13, '[');
        for (int k = 0; c && k < 4; k++)
        {
          const char *q = std::strchr(c + 1, '"');
          if (!q) break;
          std::string v;
          for (const char *e = q + 1; *e && *e != '"'; e++) { if (*e == '\\' && e[1]) { v += e[1]; e++; } else v += *e; }
          cornerName[k] = v.substr(0, 60);
          c = std::strchr(q + 1 + v.size() + (v.size() ? 0 : 0), '"');   // the closing quote
          if (!c) break;
          const char *cl = std::strchr(c, ']');
          const char *nx = std::strchr(c, ',');
          if (!nx || (cl && cl < nx)) break;
          c = nx;
        }
      }
    }
    {
      size_t ep = json.find("\"morphExempt\"");
      if (ep != std::string::npos)
      {
        morphInit();
        const char *c = std::strchr(json.c_str() + ep, '[');
        if (c)
        {
          const std::vector<size_t> map = morphSlotMap(layout, countArray(c));
          for (size_t i = 0; i < morphExempt.size(); i++) morphExempt[i] = 0;   // B124: uncarried slots = not exempt
          c++;
          for (size_t j = 0; j < map.size(); j++)
          {
            if (map[j] != SIZE_MAX) morphExempt[map[j]] = (uint8_t)(std::atoi(c) != 0);
            const char *nx = std::strchr(c, ',');
            const char *cl = std::strchr(c, ']');
            if (!nx || (cl && cl < nx)) break;
            c = nx + 1;
          }
        }
      }
    }
    /* ADR-104: corner snapshots. Simple bracketed-array scan of our own
       writer's output — four arrays in morphIds order. */
    {
      size_t mp = json.find("\"morphCorners\"");
      if (mp != std::string::npos)
      {
        morphInit();
        const char *c = json.c_str() + mp;
        for (int k = 0; k < 4; k++)
        {
          c = std::strchr(c, '[');
          if (!c) break;
          if (k == 0) { c = std::strchr(c + 1, '['); if (!c) break; }   // outer, then inner
          const std::vector<size_t> map = morphSlotMap(layout, countArray(c));   // ADR-159
          resetCorner(k);   // B124: what the file does not carry is the default, never the previous load
          c++;
          for (size_t j = 0; j < map.size(); j++)
          {
            if (map[j] != SIZE_MAX) morphCorner[k][map[j]] = std::atof(c);
            morphCornersAuthored = true;
            const char *nx = std::strchr(c, ',');
            const char *cl = std::strchr(c, ']');
            if (!nx || (cl && cl < nx)) { c = cl ? cl + 1 : c; break; }
            c = nx + 1;
          }
        }
      }
    }
  }

  // `lossless`: see historyJson (B222) — the persisted chunk stays %.6g.
  std::string morphJson(bool lossless = false)
  {
    if (morphIds.empty()) return "";
    // ADR-159: the array layout version. 2 = late per-osc rows appended last;
    // absent = 1 (pre-2026-09-11), where a 224-entry array is the ADR-150 order.
    std::string out = ",\"morphLayout\":9,\"cornerNames\":" + cornerNamesJson() + ",\"morphCorners\":[";
    char buf[32];
    const char *first = lossless ? "%.17g" : "%.6g", *rest = lossless ? ",%.17g" : ",%.6g";
    for (int k = 0; k < 4; k++)
    {
      out += k ? ",[" : "[";
      for (size_t i = 0; i < morphIds.size(); i++)
      {
        std::snprintf(buf, sizeof(buf), i ? rest : first, morphCorner[k][i]);
        out += buf;
      }
      out += "]";
    }
    out += "]";
    // ADR-109: the exempt set rides with the corners, same order contract.
    out += ",\"morphExempt\":[";
    for (size_t i = 0; i < morphExempt.size(); i++)
    {
      std::snprintf(buf, sizeof(buf), i ? ",%d" : "%d", (int)morphExempt[i]);
      out += buf;
    }
    out += "]";
    return out;
  }

  /* B100 STATE HEADER. Every blob — host chunk and JSON preset — carries
     {schema, engine_revision, build}. `schema` is the WIRE format (the chunk's
     version line, the preset's "schema"), `build` is provenance only (written,
     never read back), and `engine_revision` PINS DSP BEHAVIOUR PER PATCH: a
     sound-changing law lands behind a revision gate, so a patch saved under
     revision N keeps rendering with revision-N laws until the patch itself is
     opted forward (the u-he/Surge pattern — the opt-forward control is a
     patch-level act, never a build-level one). New instances start at the
     latest; a blob with no header predates the mechanism and is revision 1 BY
     DEFINITION — every session saved before 2026-09-10 renders with the laws
     it was saved under. kEngineRevision moves only with the ADR that adds a
     gated law. THE LAWS, BY THE REVISION THEY ARRIVED IN (ADR-183 §5: every
     law names its revision and keeps the old one selectable):
       2 — B232 / ADR-183, the off-corner blend rule (morphBlendTarget and its
           inverse in morphRouteEdit). Revision 1 is the plain four-corner
           bilinear blend, kept, not deleted.
     tools/offcorner_check.cpp renders both revisions of the same patch; the
     round-trip is tools/state_check.cpp and tools/statefix_check.cpp.
     A revision this build cannot honour clamps to the latest it knows: a
     value with no laws behind it is never stored, and the re-save then
     records what actually rendered. engineRevision() is the ONE read site —
     a future gated law consults it there, never a copy, so a law cannot fork
     on a stale snapshot. Atomic because the read site will be the audio
     thread and the write site is state_load on the main thread. */
  static constexpr int kEngineRevision = 2;   // ADR-183: the off-corner blend rule
  std::atomic<int> patchEngineRevision{kEngineRevision};
  int engineRevision() const { return patchEngineRevision.load(std::memory_order_relaxed); }
  void setEngineRevision(long rev)
  {
    const long r = std::max(1L, std::min((long)kEngineRevision, rev));
    patchEngineRevision.store((int)r, std::memory_order_relaxed);
  }

  /* B174: THE GLOBAL PRESET'S NAME, in the shell — the B122 corner mechanism
     one level up, for the same two reasons.

     (1) The PAGE cannot hold it. localStorage is unavailable under the
     plugin's opaque origin (ADR-105 A2), so a name the GUI remembers dies with
     the window: "I would like for the global preset to persist when you reload
     the GUI, like the corner presets do" (human 2026-09-20).

     (2) What is not shell state cannot ride a history snapshot, and that is
     the bug the human actually hit — loading a preset on a second branch
     appeared to switch the FIRST branch onto it. B186's gauntlet proved the
     STATE was never contaminated (PR #703: the literal scenario plus 120 seeds
     restore byte-identically); what moved was the DISPLAY, because the name
     lived only in the page and the page shows whatever was loaded last, from
     whichever branch you stand on. Storing it here is what makes the name
     travel with the node.

     "" = an unnamed patch: a fresh instance, one pasted in, or one saved
     before this key existed. */
  std::string presetName;
  void setPresetName(const std::string &n)
  {
    presetName.clear();
    /* The host chunk is LINE-based (state_save), so a newline inside a name
       would make the rest of it look like the next key=value line. Control
       characters are dropped rather than escaped — a preset name is a file
       name, and none of them can appear in one. */
    for (char c : n.substr(0, 60))
      if ((unsigned char)c >= 0x20) presetName += c;
  }

  /* One reader for "the number this JSON gives for `needle`, or `def` when it
     does not name the key". applyStateJson (what a load DOES) and
     presetMatches (whether a load would CHANGE anything) have to agree down to
     the absent-key rule, so they read through these four lines rather than
     through two copies of them. Returns whether the key was present. */
  static bool jsonNumber(const std::string &json, const std::string &needle, double def,
                         double &out)
  {
    size_t pos = json.find(needle);
    if (pos == std::string::npos) { out = def; return false; }
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) { out = def; return false; }
    out = std::atof(json.c_str() + pos + 1);
    return true;
  }

  // The string value of a top-level key, unescaped; "" when absent.
  static std::string jsonString(const std::string &json, const std::string &needle)
  {
    const size_t kp = json.find(needle);
    if (kp == std::string::npos) return "";
    const size_t colon = json.find(':', kp + needle.size());
    if (colon == std::string::npos) return "";
    const size_t q0 = json.find('"', colon + 1);
    if (q0 == std::string::npos) return "";
    std::string out;
    for (size_t i = q0 + 1; i < json.size() && json[i] != '"'; i++)
    {
      if (json[i] == '\\' && i + 1 < json.size()) i++;
      out += json[i];
    }
    return out;
  }

  /* Would applying this patch JSON change the instrument? The GUI's asterisk,
     and the global twin of cornerMatches — the same LAW, deliberately not a
     second opinion: dirty is the shell's own answer to one question, so it
     cannot disagree with a load the way a GUI-side "edited" flag can.

     The BODY cannot be shared with cornerMatches, whose every line is the
     morph slot array and its ADR-159 layout remap; a global patch is a
     parameter key set, so this walks the key sets applyStateJson walks. What
     IS shared is the rule that makes either of them honest: a key the preset
     does not carry loads as its DEFAULT (B181 note 6), so it must READ as its
     default to match — B124's uncarried-slot rule, one level up. */
  bool presetMatches(const std::string &json) const
  {
    if (json.find("\"params\"") == std::string::npos) return false;
    // Relative, because the parameter ranges here span 0..1 and 0..20000 alike
    // and one absolute epsilon cannot be right for both.
    auto same = [&](clap_id id, double want) {
      return std::fabs(readParam(id) - want) <= 1e-9 * std::max(1.0, std::fabs(want));
    };
    double v = 0;
    for (const auto &d : kParams)
    {
      if (d.id == 178) continue;   // ADR-147: applyStateJson skips it, so matching must too
      jsonNumber(json, "\"" + std::string(d.coreKey) + "\"", defaultFor(d, 0), v);
      if (!same(d.id, v)) return false;
    }
    for (const auto &b : kEngineBlocks)
      for (uint32_t i = 0; i < b.count; i++)
      {
        jsonNumber(json, "\"" + std::string(b.keyPrefix) + b.defs[i].coreKey + "\"",
                   defaultFor(b.defs[i], 0), v);
        if (!same(b.defs[i].id, v)) return false;
      }
    for (uint32_t k = 1; k < kNumOsc; k++)
      for (const auto &d : kParams)
      {
        if (isGlobalId(d.id)) continue;
        char nb[64];
        std::snprintf(nb, sizeof(nb), "\"o%u.%s\"", k, d.coreKey);
        jsonNumber(json, nb, defaultFor(d, k), v);
        if (!same((clap_id)(d.id + k * kOscStride), v)) return false;
      }
    return true;
  }

  std::string stateJson(bool lossless = false) const
  {
    // The debug dump IS the preset format (ROADMAP Phase 2 design position):
    // one schema, provenance included (SPEC §5.7). B100: the header is the
    // first three keys; "schema" stays 3 — the header adds keys, it does not
    // change what any existing key means, and every reader ignores keys it
    // does not know.
    std::string out = "{\"plugin\":\"HYPERSAW\",\"schema\":3";   // 2: ADR-103 glideMode split · 3: ADR-138 modRoutes
    char buf[96];
    std::snprintf(buf, sizeof(buf), ",\"engine_revision\":%d,\"build\":\"%s\",\"params\":{",
                  engineRevision(), HYPERSAW_BUILD_ID);
    out += buf;
    bool first = true;
    for (const auto &d : kParams)
    {
      std::snprintf(buf, sizeof(buf), "%s\"%s\":%.17g", first ? "" : ",", d.coreKey,
                    readParam(d.id));
      out += buf;
      first = false;
    }
    /* Higher oscillators in the JSON path, `o<k>.`-prefixed — the SAME
       convention state_save has used since ADR-082. The JSON path never had
       it, so a preset saved and loaded restored oscillator 1 and silently left
       oscillator 2 at whatever it was: "saving a patch doesn't seem to do
       anything, or at least loading doesn't" (human 2026-08-21) — measured:
       detune2 stayed 0.900 against a saved 0.222 while detune1 restored. */
    for (uint32_t k = 1; k < kNumOsc; k++)
      for (const auto &d : kParams)
      {
        if (isGlobalId(d.id)) continue;
        std::snprintf(buf, sizeof(buf), ",\"o%u.%s\":%.17g", k, d.coreKey,
                      readParam(d.id + k * 1000));
        out += buf;
      }
    /* B172 ENGINE BLOCKS, prefixed (see EngineBlock::keyPrefix). Emitted
       UNCONDITIONALLY, unlike `routing=`/`intent=`/`ens=`: those are chunks
       that mean "someone left the default", while these are ordinary
       parameters and a patch that omits a parameter is a patch that does not
       say what it is. The cost is paid once — every stored chunk gains 16
       keys, so the factory bank is re-saved in the same change and bank_check
       re-asserts bit-identical re-save from there. */
    for (const auto &b : kEngineBlocks)
      for (uint32_t i = 0; i < b.count; i++)
      {
        std::snprintf(buf, sizeof(buf), ",\"%s%s\":%.17g", b.keyPrefix, b.defs[i].coreKey,
                      readParam(b.defs[i].id));
        out += buf;
      }
    // const_cast confined to serialisation: morphJson touches no state, but
    // morphIds is lazily built and stateJson is const. Building eagerly at
    // construction would be cleaner; deferred to keep this diff reviewable.
    std::string tail = "}" + const_cast<Plugin *>(this)->morphJson(lossless);
    // ADR-138: routes in the preset too, same canonical chunk as state_save —
    // one serializer, two transports. Only when routes exist (see state_save).
    const std::string routes = modRoutesChunk(lossless);
    if (!routes.empty()) tail += ",\"modRoutes\":\"" + routes + "\"";
    // B89 phase 2b: the intent bus's bindings, ranges, homes and names. Same
    // rule and the same reason as modRoutes above — only when something has
    // left its default, so a patch that has never bound an intent writes no
    // key and its bytes are what they were.
    const std::string intent = intentChunk();
    if (!intent.empty()) tail += ",\"intent\":\"" + intent + "\"";
    // B174: which preset this patch came from. Emitted ONLY when non-empty —
    // the `modRoutes`/`intent` rule above — so an unnamed patch writes no key
    // and every chunk saved before this change is byte-for-byte what it was
    // (bank_check's re-save identity and the B100 fixture corpus are the proof).
    if (!presetName.empty()) tail += ",\"presetName\":\"" + jsonEscape(presetName) + "\"";
    return out + tail + "}";
  }

  /* ================= B192 — INITIALISE BEFORE EVERY LOAD ==================
     The human, 2026-09-21: "Let's make sure there's a factory Init patch and
     that everything initializes as the first step before a load."

     WHY THIS EXISTS WHEN B181 NOTE 6 ALREADY DEFAULTS ABSENT KEYS. "Every loop
     defaults its absent keys" is THREE loops in applyStateJson (the instrument
     table, the engine blocks, the osc-2 twins) that must each stay right
     forever, plus a fourth in state_load that did NOT (B183). "Reset to init,
     then apply" is ONE rule in one place, and it reaches what a key-by-key
     default structurally cannot: the morph corners, the corner names, the
     exempt set, the intent chunk, the generic routes, the preset name, the
     engine revision — and whatever the next module adds, for free.

     WHICH LANE. The same rule state_load has used since 2026-07-18: while the
     audio thread is in process() a direct write would race rebuild() against
     render(), so the defaults go through the param queue; idle, they are
     applied directly, because a host reads values back immediately after
     setState. Kind 3 = "load", so B125's rule still holds — a load's writes
     are not edits and are never routed into an armed corner.

     ONE NAMED EXCLUSION, id 178 (`specimen`): ADR-147 rules it NOT patch state
     and both load paths skip it on purpose. Resetting it here would make a
     load touch the one parameter whose whole contract is that it does not.

     `chunkOnlyState` IS NOT A SECOND RULE, IT IS THE TRANSPORT'S REACH. The
     host chunk carries the routing matrix (`routing=`); the preset JSON does
     NOT (B193 — stateJson emits kParams, the twins, the engine blocks, the
     morph chunk, modRoutes and intent, and no routing cells). Resetting the
     matrix on a preset load would therefore DELETE a topology the patch has no
     way to restore. So the flag is true only for the transport that can put it
     back, and it RETIRES the day B193 puts routing in stateJson. (B222: a
     HISTORY snapshot can put it back too — historyJson writes its own
     `routing` key and applyStateJson reads it on a history restore only — so
     a restore resets the matrix without this flag; a preset still cannot.)
     DELIBERATELY NOT RESET, and named rather than silently skipped (L0036):
     `ens=` and `lfo=` are RNG STREAM CONTINUATIONS, not patch values — they are
     emitted only once a stream has drawn, precisely so a patch that never ran
     one is byte-for-byte what it was. Resetting them is a separate question
     about stream identity across a load, and no row asks it yet. */
  void initState(bool chunkOnlyState)
  {
    const bool viaQueue = processing.load(std::memory_order_acquire);
    auto setDefault = [&](clap_id id, double v) {
      if (viaQueue)
      {
        enqueueParam(id, v, 3);
        return;
      }
      loadingState = true;
      applyParam(id, v);
      loadingState = false;
    };
    for (const auto &d : kParams)
    {
      if (d.id == 178) continue;   // ADR-147: specimen is not patch state
      setDefault(d.id, defaultFor(d, 0));
    }
    for (const auto &b : kEngineBlocks)
      for (uint32_t i = 0; i < b.count; i++) setDefault(b.defs[i].id, defaultFor(b.defs[i], 0));
    for (uint32_t k = 1; k < kNumOsc; k++)
      for (const auto &d : kParams)
      {
        if (isGlobalId(d.id)) continue;
        setDefault((clap_id)(d.id + k * kOscStride), defaultFor(d, k));
      }
    /* THE MORPH FIELD. A fresh instance's four corners hold exactly the
       per-slot defaults (morphInit), unnamed and unexempted, and
       `morphCornersAuthored` false — so this IS the init state, not an
       approximation of it. applyMorphChunk resets a corner it actually
       carries; what it could never do is reset a corner the patch says
       nothing about, which is the hole a patch written before corners
       existed falls straight through. */
    morphInit();
    for (int k = 0; k < 4; k++)
    {
      resetCorner(k);
      cornerName[k].clear();
    }
    for (auto &e : morphExempt) e = 0;
    morphCornersAuthored = false;
    // The chunks, each by the rule it already states for itself: an absent key
    // means the default, never "whatever the previous patch had".
    applyModRoutesChunk("");
    applyIntentChunk("");
    if (chunkOnlyState) applyRoutingChunk("");
    setPresetName("");
    setEngineRevision(1);
  }

  /* `nameFromLoader` is the name of the preset being loaded, or "" to take the
     patch's own (see the naming block at the end of this function).
     `historyRestore` is true ONLY from undoGoTo / undoStep: it is what lets a
     node's `routing` key be read (B222) without teaching the PRESET door a key
     and a behaviour nobody has ruled on (critic S4, PR #732). */
  bool applyStateJson(const std::string &json, const std::string &nameFromLoader = "",
                      bool historyRestore = false)
  {
    // Tolerant flat scan of our own schema: for each known coreKey, find
    // "key" and parse the number after the colon. Queued to the audio
    // thread — never applied directly from the GUI thread.
    if (json.find("\"params\"") == std::string::npos) return false;
    /* B192 — RESET TO INIT, THEN APPLY. The FIRST act of the load, before any
       chunk and before any parameter, so nothing the instance happened to hold
       can survive into a patch that does not name it. The per-key defaulting
       below is kept rather than deleted: it is also `presetMatches`' rule
       (B174 moved it into jsonNumber to answer "would a load change
       anything?"), and two independent statements of "absent means default"
       is the cheap kind of redundancy. */
    initState(/*chunkOnlyState=*/false);
    /* B222: a HISTORY snapshot carries the routing matrix (historyJson), and
       only a history restore reads it. A preset or GUI load ignores the key
       even if a file carries one, and leaves the matrix alone exactly as on
       main (`chunkOnlyState` above): which key a preset would use for routing,
       and whether a preset load should reset the matrix, is B193's question
       and the human's ruling, not a side effect of history. Queued with the
       parameters as kind 3, for initState's reason (a direct write while
       process() runs races the audio thread) and B125's (a load's writes are
       not edits, so none of them is routed into a corner). */
    if (historyRestore)
    {
      const size_t rk = json.find("\"routing\":\"");
      const size_t q0 = rk == std::string::npos ? rk : rk + 11;   // past `"routing":"`
      const size_t q1 = q0 == std::string::npos ? q0 : json.find('"', q0);
      if (q1 != std::string::npos)
        routingChunkCells(json.substr(q0, q1 - q0),
                          [&](clap_id id, double v) { enqueueParam(id, v, 3); });
    }
    /* ADR-138: a load is a load — generic routes are REPLACED by the preset's
       (or cleared, for a preset saved before routes existed; stale routes
       bleeding into a loaded patch would be state the preset never named).
       Applied directly on this thread, the same discipline as the GUI's own
       route edits; params below still go through the queue. */
    {
      std::string chunk;
      const size_t mp = json.find("\"modRoutes\"");
      if (mp != std::string::npos)
      {
        const size_t q0 = json.find('"', json.find(':', mp) + 1);
        const size_t q1 = q0 == std::string::npos ? std::string::npos : json.find('"', q0 + 1);
        if (q0 != std::string::npos && q1 != std::string::npos)
          chunk = json.substr(q0 + 1, q1 - q0 - 1);
      }
      applyModRoutesChunk(chunk);
    }
    /* B89 phase 2b, and a load is a load for the same reason: an ABSENT key
       means unbound (full ranges, zero bindings, centred homes, default
       names), never "whatever the previous patch had". */
    {
      std::string chunk;
      const size_t ip = json.find("\"intent\"");
      if (ip != std::string::npos)
      {
        const size_t q0 = json.find('"', json.find(':', ip) + 1);
        const size_t q1 = q0 == std::string::npos ? std::string::npos : json.find('"', q0 + 1);
        if (q0 != std::string::npos && q1 != std::string::npos)
          chunk = json.substr(q0 + 1, q1 - q0 - 1);
      }
      applyIntentChunk(chunk);
    }
    /* ---- A LOAD IS A LOAD, AND IT NOW MEANS THE SAME THING FOR PARAMETERS
       (B181 note 6, human 2026-09-20: "Sub currently seems to be ignored by
       loading presets").

       Until this change every parameter loop here read `if (pos == npos)
       continue;` — an ABSENT key was SKIPPED, so the parameter kept whatever
       the previous patch left in it. The two chunk paths three lines above
       have always said the opposite, in as many words: "an ABSENT key means
       unbound ... never 'whatever the previous patch had'." One function held
       both rules.

       The engine-block loop even defended the skip: "a patch written before
       the block existed simply says nothing about it, and the block ships off,
       so it stays inert". The hidden premise is that the block is off — and
       the moment a player switches it ON, an absent key means it stays on
       across every subsequent preset load, carrying its wave, its octave and
       its level with it. That is exactly what the human heard.

       So: a parameter ABSENT from a loaded patch is restored to its DEFAULT
       (defaultFor — the same single source CLAP's default_value and the GUI's
       double-click use), never left as it was. Instrument table, engine blocks
       and osc-2 twins alike, because "a load is a load" is not a property of
       one namespace.

       WHAT THIS COSTS, STATED: an old patch that omits a key now renders that
       key at its default rather than at the previous patch's value. That is a
       behaviour change for every pre-existing preset, and it is the POINT — it
       is what makes a load reproducible. The evidence is subosc_check's
       40-patch byte-identity row: loading each factory patch after deliberately
       disturbing every parameter must give byte-identical state to loading it
       on a fresh instance. The migrations below still run AFTER this and still
       win, so `enable`, `noteLawLink`/`glide` and the schema<2 glideMode
       rewrite are untouched — each is keyed on the JSON text, not on what the
       instance currently holds.

       `any` still means "the patch named at least one key we know", which is
       this function's return value and the host's success flag; a default
       restore is not evidence of that, so it deliberately does not set it. */
    // B174: the absent-key rule lives in jsonNumber now, because presetMatches
    // has to apply exactly this one to answer "would a load change anything?".
    auto valueOrDefault = [&](const std::string &needle, double def, double &out) {
      return jsonNumber(json, needle, def, out);
    };
    bool any = false;
    for (const auto &d : kParams)
    {
      if (d.id == 178) continue;   // ADR-147: specimen is not patch state (see state_load)
      double v = 0;
      any = valueOrDefault("\"" + std::string(d.coreKey) + "\"", defaultFor(d, 0), v) || any;
      enqueueParam(d.id, v, 3);   // B125: load kind
    }
    // B172 engine blocks, prefixed — same rule, and this is the loop whose
    // absent-key skip the human actually heard.
    for (const auto &b : kEngineBlocks)
      for (uint32_t i = 0; i < b.count; i++)
      {
        double v = 0;
        any = valueOrDefault("\"" + std::string(b.keyPrefix) + b.defs[i].coreKey + "\"",
                             defaultFor(b.defs[i], 0), v) ||
              any;
        enqueueParam(b.defs[i].id, v, 3);
      }
    // the twins, by the state_save convention
    for (uint32_t k = 1; k < kNumOsc; k++)
      for (const auto &d : kParams)
      {
        if (isGlobalId(d.id)) continue;
        char nb[64];
        std::snprintf(nb, sizeof(nb), "\"o%u.%s\"", k, d.coreKey);
        double v = 0;
        any = valueOrDefault(nb, defaultFor(d, k), v) || any;
        enqueueParam(d.id + k * 1000, v, 3);
      }
    /* PRE-NOTE-LANE PATCH MIGRATION. `noteLawLink` ships FOLLOW as of 2026-08-20,
       but a patch saved before the note lane existed carries no such key — it
       expressed its portamento purely as `glide` seconds, and restoring it into a
       FOLLOWing lane would hand it to `bendLaw`, which ships off, silently
       deleting the glide it was saved with. A patch that names `glide` but not
       `noteLawLink` predates the lane by definition, so it is restored to
       own-settings + lag, which is exactly what `glide` meant when it was saved.
       docs/presets/serum-parity-reference.json is one such patch ("glide":0.89). */
    if (json.find("\"noteLawLink\"") == std::string::npos &&
        json.find("\"glide\"") != std::string::npos)
    {
      enqueueParam(137, 0, 3);                                  // own settings
      enqueueParam(138, hypersaw::GlideCore::kLag, 3);          // lag, as it always was
    }
    applyMorphChunk(json);
    /* ADR-103: schema<2 patches saved glideMode=1 when that option behaved as
       ALWAYS (silence included) — the new mode 1 (ringing-gated) did not exist.
       Migrate the stored 1 to 2: same sound, new number. */
    {
      size_t sp = json.find("\"schema\"");
      long schema = 1;
      if (sp != std::string::npos)
      {
        sp = json.find(':', sp);
        if (sp != std::string::npos) schema = std::atol(json.c_str() + sp + 1);
      }
      size_t gm = json.find("\"glideMode\"");
      if (schema < 2 && gm != std::string::npos)
      {
        gm = json.find(':', gm);
        if (gm != std::string::npos && std::atof(json.c_str() + gm + 1) >= 0.5)
          enqueueParam(90, 2, 0);
      }
      // B100: the patch's engine revision, pinned from the header; a preset
      // without one (any schema-3-or-earlier file) is revision 1. Set
      // unconditionally — a load is a load, and the previous patch's
      // revision must not bleed into a header-less one.
      size_t er = json.find("\"engine_revision\"");
      long rev = 1;
      if (er != std::string::npos)
      {
        er = json.find(':', er);
        if (er != std::string::npos) rev = std::atol(json.c_str() + er + 1);
      }
      setEngineRevision(rev);
    }
    /* Pre-ADR-100 patches have no "enable" key and were saved when every
       oscillator always rendered — restore them that way, whatever the new
       defaults ship as. */
    if (json.find("\"enable\"") == std::string::npos)
    {
      enqueueParam(150, 1, 3);
      enqueueParam(1150, 1, 3);
    }
    /* B174 — THE NAME IS SET HERE, NOT BY A SECOND CALL AFTERWARDS.
       B122's setCornerName amends a mark that is still PENDING, and PR #703
       found the hole that leaves: let a GUI frame land between the load and
       the naming and undoService takes the snapshot first, so the node records
       the PREVIOUS name and the corner's identity is lost from history. There
       is no such window here — the loader's name is an ARGUMENT, so the state
       undoService will snapshot already carries it however late the pump
       arrives. An empty argument defers to the patch's own key, which is how a
       history restore (whose json IS a snapshot) gets its branch's name back;
       a patch with no key at all is unnamed, never "whatever was loaded
       before" — a load is a load, the same rule as the chunks above. */
    setPresetName(nameFromLoader.empty() ? jsonString(json, "\"presetName\"") : nameFromLoader);
    const std::string label =
        presetName.empty() ? std::string("load") : "load \xe2\x86\x90 " + presetName;   // "←" as UTF-8
    undoMark(label.c_str());   // B84: a preset load is ONE history node
    return any;
  }

  /* ================= B222 — WHAT A HISTORY NODE STORES =====================
     The preset JSON PLUS the routing matrix. The preset JSON has never carried
     routing (B193: stateJson emits kParams, the twins, the engine blocks, the
     morph chunk, modRoutes and intent — no routing cell), so a node restored
     everything except the FX matrix, which kept whatever it held. That alone
     made history lie about the matrix; with the morph field it made a restore
     depend on the road taken, which is what the human heard — a node with
     morph on drives the matrix from its corners the moment audio runs, and
     that matrix survived the return to any other node (undo_check layer 5,
     red on the pre-B222 build with every snapshot byte-identical).

     WHY A HISTORY-ONLY KEY AND NOT B193's LITERAL FIX (routing in stateJson).
     stateJson IS the preset file format and the GUI's save. Putting routing
     there changes what every preset saves and — through "a load is a load" —
     what every existing preset LOADS: today a preset load leaves the matrix
     alone (B192's `chunkOnlyState`), so a preset without the key would start
     resetting the player's matrix. Both are human gates. A history snapshot is
     main-thread memory that is never persisted, so its format is this
     function's to choose — and it is READ only by a history restore
     (applyStateJson's `historyRestore`), so no preset gains the key by
     accident: a preset carrying `routing` loads exactly as on main.

     `routing` is written ALWAYS here, even empty, because a node's promise is
     total: an empty chunk means "the series chain", and a restore must put the
     chain back rather than skip. It sits before "params" so it is the first
     `"routing":"` in the text whatever a preset or corner NAME contains
     (jsonEscape makes an embedded quote `\"`, which cannot match anyway).

     LOSSLESS, for the same reason and found the same way: the corner arrays
     and the mod-route depths are written %.6g in the persisted formats, so a
     node recorded with morph ON restored its corners ROUNDED — the field then
     played 1.27526 where the player had heard 1.275262652, and the node never
     sounded again the way it did when it was made. Measured first as audio
     (a node's render at record time against its render after any restore),
     now gated by layer 5's corner-readout row, which is %.10g and so sees a
     %.6g rounding the node's own writer cannot. The parameters were always
     %.17g; now everything in a node is. The persisted %.6g is untouched —
     changing it would re-save every preset. */
  std::string historyJson() const
  {
    std::string s = stateJson(/*lossless=*/true);
    const size_t at = s.find(",\"params\":{");
    s.insert(at == std::string::npos ? s.size() - 1 : at, ",\"routing\":\"" + routingChunk() + "\"");
    return s;
  }

  /* ================= UNDO HISTORY (B84 / ADR-160) =========================
     Main thread only. The audio thread is not read, not written, and not
     synchronised with beyond the two atomic loads undoService already needs in
     order to ask "has the queue drained?".

     A MARK says a node is owed and what to call it; it does NOT snapshot.
     Every GUI write reaches the engine through the param QUEUE, so a snapshot
     taken at the mark would photograph the state BEFORE the edit that mark
     names. undoService takes it at the first main-thread pump (hzFrame, or any
     undo bind) once qTail has caught qHead. Two properties fall out for free:
     marks arriving while nothing drains (transport stopped, editor closed)
     COLLAPSE into the single node the resume produces, and a mark whose
     snapshot equals the current node's is dropped by UndoTree::push.

     ADR-160 (3): host parameter events and automation never mark. They reach
     applyParam on the audio thread and nothing on that path calls undoMark —
     the exclusion is structural, not a filter that could be forgotten. */
  void undoMark(const char *label)
  {
    if (undoRestoring) return;   // replaying a snapshot is navigation, not an edit
    undoPending = true;
    undoPendingLabel.assign(label ? label : "");
  }

  // The gesture-END label: the parameter's display name plus which oscillator
  // it belongs to, so the two strips are told apart in the list.
  /* THE EDITOR'S GESTURE BRACKET, in one place. It was the body of
     hostIf.gesture until B89 phase 2d needed the same bracket reachable
     headlessly (the performance pad's drag is a bracket, not a value, so an
     oracle that cannot open one cannot test the spring at all). One function,
     two callers — the bridge verb and hypersaw_debug_gesture — rather than two
     copies of a two-line law (ADR-110's scar).
     B84 rides the ADR-121 latch rather than adding a second one: the END of a
     bracket is exactly "one drag = one undo step", and a morph-pad drag that
     rewrites 224 owners ends once. Nothing else about the latch changes. */
  void guiGesture(clap_id id, bool begin)
  {
    enqueueParam(id, 0, begin ? 1 : 2);
    if (!begin) undoMarkParam(id);
  }

  /* THE EDITOR'S VALUE WRITE, in one place for guiGesture's reason: the
     bridge's setParam and the headless `setmorph` verb (hypersaw_debug_undo)
     are two callers of this, never two copies of it, so an oracle drives the
     write the editor makes.
     B84 — the morph toggle gets its own label. Marked HERE and not in the
     morphOn branch of applyParam, which is the audio thread and is also where
     host automation lands: a mark there would both touch the RT path and give
     automation a node, and ADR-160 (3) forbids the second. This seam is
     main-thread and reachable only from the editor. */
  void guiSetParam(clap_id id, double v)
  {
    enqueueParam(id, v, 0);
    if (id == 151) undoMark(v > 0.5 ? "morph on" : "morph off");
  }

  void undoMarkParam(clap_id id)
  {
    const ParamDef *d = findParam(id);
    if (!d) return;
    /* B222: the morph toggle's bracket END must not rename the node its value
       write already named. gui2 brackets every control around its own value
       change (B191), so the order is begin, setParam, END — the END arrives
       LAST and, left alone, overwrote "morph on"/"morph off" with the bare
       display name "Morph". The history rail folds a chain of same-label nodes
       into one row, so on-then-off drew as "Morph ×2" and the ON had no entry
       of its own (human 2026-09-23). guiSetParam's label is the specific one;
       keep it. */
    if (id == 151 && undoPending && undoPendingLabel.rfind("morph o", 0) == 0) return;
    char lb[96];
    const uint32_t k = oscOfId(id);
    // B172: an engine-block id has no oscillator — `oscOfId(4001)` is 4 and
    // `baseIdOf(4001)` is 1, so without this the undo label would read
    // "SUB Width (osc 5)". The routing ids never reached here with a suffix
    // because every routing id is global by `isGlobalId`'s accident; this is
    // the honest test rather than the lucky one.
    if (engineBlockOf(id)) std::snprintf(lb, sizeof lb, "%s", d->name);
    else if (kNumOsc > 1 && !isGlobalId(baseIdOf(id)))
      std::snprintf(lb, sizeof lb, "%s (osc %u)", d->name, (unsigned)(k + 1));
    else
      std::snprintf(lb, sizeof lb, "%s", d->name);
    undoMark(lb);
  }

  void undoService()
  {
    /* The ROOT. Without it the first edit's node has no parent and Undo is
       dead until the second one — the history would begin one state too late,
       and that state is exactly the one a player wants back. Taken at the
       first pump with a drained queue; a mark already pending dedups against
       this same snapshot, so an editor opened onto a just-loaded patch shows
       one node, not two. */
    const bool seed = undo.size() == 0;
    if (!seed && !undoPending) return;
    if (qHead.load(std::memory_order_acquire) != qTail.load(std::memory_order_acquire)) return;
    const std::string snap = historyJson();
    if (seed) undo.push("start", snap, ++undoTick);
    if (undoPending)
    {
      undo.push(undoPendingLabel.c_str(), snap, ++undoTick);
      undoPending = false;
    }
  }

  bool undoGoTo(int i)
  {
    undoService();   // the edit in hand becomes a node BEFORE we walk away from it
    if (!undo.liveAt(i)) return false;
    undoRestoring = true;
    applyStateJson(undo.node(i).json, "", /*historyRestore=*/true);
    undoRestoring = false;
    undo.restore(i);
    undoPending = false;
    return true;
  }

  bool undoStep(int dir)
  {
    undoService();
    const int i = dir < 0 ? undo.undo() : undo.redo();
    if (i == hypersaw::UndoTree::kNone) return false;
    undoRestoring = true;
    applyStateJson(undo.node(i).json, "", /*historyRestore=*/true);
    undoRestoring = false;
    undoPending = false;
    return true;
  }

  std::string undoTreeJson()
  {
    undoService();
    char buf[192];
    std::snprintf(buf, sizeof buf, "{\"cur\":%d,\"cap\":%d,\"nodes\":[", undo.current(),
                  undo.capacity());
    std::string out = buf;
    bool first = true;
    for (int i = 0; i < undo.capacity(); i++)
    {
      if (!undo.liveAt(i)) continue;
      const auto &n = undo.node(i);
      std::snprintf(buf, sizeof buf, "%s{\"i\":%d,\"parent\":%d,\"tick\":%llu,\"label\":\"",
                    first ? "" : ",", i, n.parent, (unsigned long long)n.tick);
      out += buf;
      out += jsonEscape(n.label);
      out += "\"}";
      first = false;
    }
    return out + "]}";
  }

  // The labels are ours (parameter names, "corner 2"), but they cross into a
  // webview as JSON — escape rather than trust the table to stay quote-free.
  static std::string jsonEscape(const std::string &in)
  {
    std::string o;
    o.reserve(in.size() + 8);
    for (char c : in)
    {
      if (c == '"' || c == '\\') { o += '\\'; o += c; }
      else if ((unsigned char)c < 0x20) o += ' ';
      else o += c;
    }
    return o;
  }

  /* THE ONE WRITER of the matrix from the parameter side (ADR-088).
     The presence bit is DERIVED from the coefficient, never set separately:
     routing_core.h's header says "a coefficient of 0 *is* not connected", and
     that is only true if the two cannot disagree. Deriving it is also what
     keeps `isTerminal` honest — morph a slot's only outgoing coefficient to
     zero and that slot becomes an output, continuously, with no edge to add. */
  void setRoutingParam(clap_id id, double v)
  {
    int kind = 0, from = 0, to = 0;
    if (!decodeRoutingId(id, kind, from, to)) return;
    if (kind == kRoutingCoeff)
    {
      // ROW -> matrix index: decodeRoutingId speaks the id's coordinates, the
      // matrix speaks its own (ADR-088 amendment). The decode already proved
      // this row names a live cell, so the index is >= 0.
      const int mi = routingIndexOfRow(from);
      routing.coeff[mi][to] = v;
      if (v != 0.0) routing.inFrom[to] |= (1u << mi);
      else routing.inFrom[to] &= ~(1u << mi);
    }
    else if (kind == kRoutingOut) routing.outAmount[to] = v;
    else if (kind == kRoutingSrcOut) routing.srcOut[from] = v;
    else routing.slotInit[to] = v;
  }
  double getRoutingParam(clap_id id) const
  {
    int kind = 0, from = 0, to = 0;
    if (!decodeRoutingId(id, kind, from, to)) return 0.0;
    if (kind == kRoutingCoeff) return routing.coeff[routingIndexOfRow(from)][to];
    if (kind == kRoutingOut) return routing.outAmount[to];
    if (kind == kRoutingSrcOut) return routing.srcOut[from];
    return routing.slotInit[to];
  }

  void applyParam(clap_id id, double value)
  {
    if (const ParamDef *d = findParam(id))
    {
      double v = std::max(d->minV, std::min(d->maxV, value));
      if (id == 23) v = snapGridStep(v);  // rational beat increments only
      // Inertia knob taper (ADR-024): core w = sqrt(knob) spreads the useful
      // heavy range across the knob (measured: the raw map leaves w in
      // 0.02..0.3 a dead plateau at musical K). Core DSP untouched — the
      // taper lives here; readParam inverts it.
      if (id == 11)
      {
        inertiaKnob = v;
        // ADR-059: 0.5 uses sqrt EXACTLY (bit-identical to the ADR-024 default);
        // other exponents use pow. Default knob feel is unchanged.
        v = inertiaCurve == 0.5 ? std::sqrt(v) : std::pow(v, inertiaCurve);
      }
      if (id == 70)  // ADR-059 dev: inertia taper exponent; re-derive inertia now
      {
        inertiaCurve = v;
        core.setParam("inertia",
                      inertiaCurve == 0.5 ? std::sqrt(inertiaKnob) : std::pow(inertiaKnob, inertiaCurve));
        return;
      }
      const double applied = d->stepped ? std::round(v) : v;
      /* ADR-109: one choke point. Every parameter edit — GUI, host automation,
         preset load — passes here, so corner routing needs exactly one hook
         rather than a rule per call site. `morphFromField` guards re-entry:
         morphStep applies the field's own output through applyParam, and
         routing THAT would have the field endlessly rewriting its own corners. */
      if (!morphFromField && !modFromMatrix && morphOn > 0.5 && !morphRouteEdit(id, applied)) return;
      /* ADR-136: the base intercept. Any write that is NOT the matrix's own
         lands as the new BASE for a modulated destination; the offset is
         re-applied on the next mod tick rather than here, so a user drag under
         modulation feels like dragging the base. */
      if (!modFromMatrix)
        if (ModDest *md = modDestFor(id, false)) md->base = applied;
      /* ADR-088 routing block. Placed AFTER the morph/mod hooks above (a
         crosspoint is morphable, so a corner edit has to route like any other
         parameter) and BEFORE every `baseIdOf` test below, which would alias a
         routing id onto an instrument one. Handles and RETURNS: the fallthrough
         at the end of this function hands the id to cores[].setParam. */
      if ((uint32_t)id >= kRoutingIdBase) { setRoutingParam(id, applied); return; }
      /* ENGINE BLOCKS (B172), placed for the same two reasons the routing block
         is: after the morph/mod hooks (an engine row is morphable, so a corner
         edit routes like any other parameter) and before every `baseIdOf` test
         below, which would alias 4004 onto base 4. THE DISPATCH IS THE TABLE;
         only the last line is engine-specific, because each engine owns a
         different core — STATION adds a branch there and nothing else. */
      if (const EngineBlock *eb = engineBlockOf(id))
      {
        if (eb->base == kSubOscIdBase) subSetParam(id, applied);
        return;
      }
      if (id == 32)
      {
        if (applied != voiceMono)
        {
          allOffAll();
          heldCount = 0;
          monoSlot = -1;
        }
        voiceMono = applied;
        return;
      }
      if (id == 34)
      {
        voiceLegato = applied;
        return;
      }
      if (baseIdOf(id) == 104 || baseIdOf(id) == 105)
      {
        const uint32_t osc = oscOfId(id);
        if (osc < kNumOsc)
        {
          if (baseIdOf(id) == 104) oscMute[osc] = applied;
          else oscSolo[osc] = applied;
        }
        return;
      }
      if (baseIdOf(id) == 35 || baseIdOf(id) == 36 || baseIdOf(id) == 37)
      {
        const uint32_t osc = oscOfId(id);
        if (osc < kNumOsc)
        {
          const clap_id base = baseIdOf(id);
          if (base == 35) octaveA[osc] = applied;
          else if (base == 36) semiA[osc] = applied;
          else fineCentsA[osc] = applied;
          updateTune(osc);
        }
        return;
      }
      /* BEND LAW. Routed here rather than through a core setParam() because the
         law lives in the SHELL's GlideCore, not in an oscillator: bend is global.
         Switching the law resets the filter to the current sounding bend so a
         change of law cannot make the pitch jump — the state carries over, only
         the trajectory changes. */
      if (id >= 106 && id <= 115)
      {
        switch (id)
        {
          case 106:
            if ((int)applied != (int)bendLaw.model)
            {
              bendLaw.model = applied;
              bendGlide.reset(pitchBend);
              bendAccum = 0;
              // Leaving a law re-arrives instantly: with kOff the target IS the
              // value, so settle now rather than at the next grid boundary.
              if (!bendActive() && pitchBend != bendTarget)
              {
                pitchBend = bendTarget;
                updateTuneAll();
              }
              // ADR-097: and the per-note lanes with it. stepNoteBends() only
              // runs inside the bendActive() branch of process(), so a note left
              // travelling when the law is switched off would never be stepped
              // again and would hang at a partial bend forever.
              if (!bendActive())
                for (int i = 0; i < hypersaw::kPoly; i++)
                  if (noteBend[i].live) setNoteBendTarget(i, noteBend[i].target);
            }
            break;
          case 107: bendLaw.gtime = applied; break;
          case 108: bendLaw.rate = applied; break;
          case 109: bendLaw.tau = applied; break;
          case 110: bendLaw.springF = applied; break;
          case 111: bendLaw.damp = applied; break;
          case 112: bendLaw.distOver = applied; break;
          case 113: bendLaw.retMul = applied; break;
          case 114:
            bendLaw.quant = applied;
            // Same settle rule as leaving a law (case 106): if the lane just
            // went fully inactive, the sounding bend would otherwise be stuck
            // at the last QUANTISED step forever — nothing steps it again.
            if (!bendActive() && pitchBend != bendTarget)
            {
              pitchBend = bendTarget;
              bendGlide.reset(bendTarget);
              updateTuneAll();
            }
            break;
          case 115: bendLaw.qhyst = applied; break;
          default: break;
        }
        pushNoteLaw();   // a FOLLOWING note lane tracks every bend edit
        return;
      }
      /* GLOBAL SCALE -> the mask the quantiser actually reads. The second
         consumer has now arrived (the note lane, ADR-096), which is why this
         writes to `scale` and both lanes read it rather than either owning it —
         the surface was made global for exactly this. */
      if (id >= 133 && id <= 136) { rack.setMix((int)(id - 133), applied); return; }
      /* ADR-131: 200..231 is four blocks of 8. Arithmetic rather than 28 cases,
         so adding a slot or a param cannot fall out of step with the table. */
      if (id >= 200 && id <= 231)
      { rack.setTimeParam((int)((id - 200) / 8), (int)((id - 200) % 8), applied); return; }
      // ADR-142: the Delay's four blocks of 8 (see the param table's note).
      if (id >= 232 && id <= 263)
      { rack.setDelayParam((int)((id - 232) / 8), (int)((id - 232) % 8), applied); return; }
      if (id == 161)
      {
        /* The knob IS the pitch route's depth. The route is created on first
           non-zero depth and its depth tracks the knob thereafter — one knob,
           one route, no hidden state. Source slot 1 = ENV 2 (ADR-135).
           ADR-138: found BY DEST, never by index — "route 0" stopped being a
           safe name the moment routes persist (a restored generic route can
           sit at index 0), and it was already corruptible by removing the
           pitch route in the GUI and then automating this knob. */
        const int pr = modPitchRouteIdx();
        if (pr >= 0) mod.routes[pr].depth = applied;
        else if (applied != 0.0)
          mod.addRoute(1, kModDestPitch, applied, hypersaw::ModCore::kGlobal);
        return;
      }
      if (id >= 162 && id <= 165)
      {
        if (id == 162) env2A = applied;
        else if (id == 163) env2D = applied;
        else if (id == 164) env2S = applied;
        else env2R = applied;
        return;
      }
      /* B171: LFO 1/2 (269-280) and ENV 3/4 (281-288). Stored here rather than
         pushed to a core because these modulators are the SHELL's — the cores
         never see them; the matrix does. */
      if (id >= 269 && id <= 280)
      {
        const int i = (id - 269) / 6, f = (int)((id - 269) % 6);
        switch (f)
        {
          case 0: lfoRate[i] = applied; break;
          case 1: lfoShape[i] = (int)applied; break;
          case 2: lfoSync[i] = (int)applied; break;
          case 3: lfoBeats[i] = applied; break;
          case 4: lfoRetrig[i] = (int)applied; break;
          default: lfoPhase0[i] = applied; break;
        }
        return;
      }
      if (id >= 281 && id <= 288)
      {
        const int x = (id - 281) / 4, f = (int)((id - 281) % 4);
        switch (f)
        {
          case 0: xenvA[x] = applied; break;
          case 1: xenvD[x] = applied; break;
          case 2: xenvS[x] = applied; break;
          default: xenvR[x] = applied; break;
        }
        return;
      }
      if (id >= 166 && id <= 173) { macroVal[id - 166] = applied; return; }
      if (id >= 174 && id <= 177) { xyAsn[id - 174] = (int)applied; return; }
      if (id == 179 || id == 180)
      {
        /* Re-aiming an axis mid-gesture would strand its drag latch ON — the
           END arrives for the OLD id and matches nothing — and a stuck latch
           is a puck that never springs home again. Clearing both is the cure
           that needs no bookkeeping: the worst it costs is one interrupted
           drag, and the player is already holding the thing that will send the
           next BEGIN. */
        intentDrag[0] = intentDrag[1] = false;
        mainAsn[id - 179] = (int)applied;
        return;
      }
      if (baseIdOf(id) == 181)
      {
        const uint32_t osc = oscOfId(id);
        if (osc < kNumOsc) { pitchContA[osc] = applied; updateTune(osc); }
        return;
      }
      if (id == 178) { specimenOn = applied; return; }
      /* NOTE LANE (ADR-096). Mirrors the bend block above field-for-field, minus
         retMul. Note the absent tau: id 33 carries the note lag, in seconds, and
         the core converts at the use site — see the swarm_core comment. */
      // NOTE LAG (id 33) is the own-settings tau, in SECONDS. It keeps feeding
      // the core param (state, readback, and the lag arming check all read it)
      // AND now mirrors into the law the shell pushes, because the core no
      // longer converts at the use site — see the swarm_core comment.
      if (id == 33)
      {
        noteLawOwn.tau = applied * 1000.0;
        core.setParam("glide", applied);
        for (uint32_t k = 1; k < kNumOsc; k++) cores[k].setParam("glide", applied);
        pushNoteLaw();
        return;
      }
      if (baseIdOf(id) == 150)
      {
        const uint32_t osc = id / 1000;
        const bool on = applied >= 0.5;
        /* ADR-100 A3's blanket write is GONE (ADR-132, 2026-08-27). It used to
           copy an enable edit into ALL FOUR corners, and its reason was real
           when written: without it "the next grid tick reads the corner's
           stored enable and reverts it, and a power switch that snaps back
           reads as broken".

           ADR-109 made that obsolete and nobody removed it. `morphRouteEdit`
           now runs BEFORE this block and stores the edit itself in every path:
           armed, into the armed corner; unarmed pick-mode, into the corner that
           WON the parameter. Either way the grid tick reads back what was just
           written, so nothing reverts and no safety net is needed.

           What the net cost instead: it destroyed the feature ADR-100 exists
           for. Its own header promises "the morph grid can hold 'off in this
           corner, on in that one'" — and an unarmed toggle silently overwrote
           the three corners the player was not standing on. Reported
           2026-08-27 and reproduced: corners C and D, authored OFF and never
           touched, both read ON after one unarmed edit at corner B. */
        if ((oscEnabled[osc] != 0) != on)
        {
          oscEnabled[osc] = on ? 1 : 0;
          // Both transitions kill: OFF because the switch means silence NOW,
          // ON because voices frozen since the disable would otherwise resume
          // as zombies at whatever loudness they froze at.
          cores[osc].killAll();
          /* ADR-100 Amendment 1: enable-ON RE-STRIKES what is held. Without
             this, switching an oscillator on mid-chord produced nothing until
             the next fresh note -- "sometimes osc 2 doesn't work" (human,
             2026-08-21): they enabled it, played nothing new, heard nothing,
             and "broken" was a fair conclusion. Re-striking from the tags also
             makes MORPH-driven enable flips musical: the oscillator pops in
             WITH the held chord at the held velocities -- exactly the "toggle
             on as you move between corners" design. A fresh attack rather than
             a resumed envelope is intentional: the note is NEW on this
             oscillator; the OFF transition killed whatever state there was. */
          if (on)
            for (int i = 0; i < (int)hypersaw::kPoly; i++)
              if (tags[i].active)
              {
                const double f = 440.0 * std::pow(2.0, (tags[i].key - 69) / 12.0);
                const int sk = cores[osc].noteOn(tags[i].key, f);
                cores[osc].setNoteVelocity(sk, tags[i].vel);
                bindSlots(i, osc, sk);
              }
        }
        return;
      }
      if (id == 159) { morphArm = applied; return; }
      if (id >= 151 && id <= 158)
      {
        const bool morphWasOn = morphOn > 0.5;
        switch (id)
        {
          case 151: morphOn = applied;
                    // fill, not assign: this runs on the AUDIO thread and the
                    // vector is pre-sized at activate — no allocation here.
                    if (morphOn > 0.5) std::fill(morphCur.begin(), morphCur.end(), -1e30);
                    /* NON-DESTRUCTIVE MORPH-ON. If the corners are still the
                       seed morphInit() laid down at startup, adopt the LIVE
                       patch into all four rather than letting the first grid
                       tick write stale defaults over the player's sound
                       (reported 2026-08-26: "switching morph on when you've
                       edited the patch can be destructive; it replaces the
                       sound with default inits").
                       All four, not just one, so morphInit's silence-safe
                       property is preserved exactly: every corner agrees, so
                       the field is inert until something is captured. This is
                       the same lean already recorded at morphToggleExempt --
                       "the corners honestly record what was playing".
                       Guarded on `morphCornersAuthored` so a loaded preset's
                       corners are never clobbered: the destructive direction
                       has to stay closed in BOTH directions. */
                    if (morphOn > 0.5 && !morphCornersAuthored)
                      for (size_t i = 0; i < morphIds.size(); i++)
                      {
                        const double live = readParam(morphIds[i]);
                        for (int k2 = 0; k2 < 4; k2++) morphCorner[k2][i] = live;
                      }
                    /* B222: the same promise once the corners ARE authored —
                       which, before this, meant every instance that had ever
                       loaded anything, because every state load carries a
                       morph chunk and sets the flag above. The rule is
                       morphAdoptUncontested's; it runs on the EDITOR's
                       off -> on only, never a load's write, never automation. */
                    else if (morphOn > 0.5 && !morphWasOn && editorWrite && !loadingState)
                      morphAdoptUncontested();
                    // B48: morph off releases the on-weight ramp, else the
                    // last partway value would keep scaling a morph-free patch.
                    // B203: the engine gates' ramp is released with them — same
                    // sentence, one switch over.
                    if (morphOn <= 0.5)
                    {
                      for (uint32_t k2 = 0; k2 < kMaxOsc; k2++) oscOnW[k2] = 1.0;
                      for (const auto &b : kEngineBlocks) setEngineGateRamp(b.gateId, 1.0);
                    }
                    break;
          case 152: morphX = applied; break;
          case 153: morphY = applied; break;
          case 154: morphTemp = applied; break;
          case 155: morphCoup = applied; break;
          case 156:
            if ((uint32_t)applied != morphSeed)
            {
              morphSeed = (uint32_t)applied;
              // reshuffle is pure array writes — RT-safe; morphInit ran at activate
              morph.reshuffle(morphSeed, (int)morphIds.size());
              // ADR-176: the resolver's per-atom seeds come from the same
              // device seed, so they re-draw in the same breath. One seed, two
              // laws — a second site would be a second chance to forget.
              intentDrawSeeds();
            }
            break;
          case 157: morphMode = applied; break;
          case 158: morphGlideS = applied; break;
          default: break;
        }
        return;
      }
      if (id == 149)
      {
        mpeBendLaw = applied;
        // Turning the law OFF must land every travelling note NOW. Leaving them
        // mid-flight would strand each at whatever bend it happened to hold, and
        // nothing would ever step them again.
        if (!mpeBendLaw)
          for (int i = 0; i < hypersaw::kPoly; i++)
            if (noteBend[i].live) setNoteBendTarget(i, noteBend[i].target);
        return;
      }
      if (id >= 146 && id <= 148)
      {
        if (id == 146) qTimeMode = applied;
        else if (id == 147) qTimeHz = applied;
        else qTimeSync = snapGridStep(applied);   // musical divisions only
        bendLaw.qTime = resolveQTimeMs();
        pushNoteLaw();
        return;
      }
      if (id >= 137 && id <= 145)
      {
        switch (id)
        {
          case 137: noteLink = applied; break;
          case 138: noteLawOwn.model = applied; break;
          case 139: noteLawOwn.gtime = applied; break;
          case 140: noteLawOwn.rate = applied; break;
          case 141: noteLawOwn.springF = applied; break;
          case 142: noteLawOwn.damp = applied; break;
          case 143: noteLawOwn.distOver = applied; break;
          case 144: noteLawOwn.quant = applied; break;
          case 145: noteLawOwn.qhyst = applied; break;
          default: break;
        }
        pushNoteLaw();
        return;
      }
      if (id >= 116 && id <= 128)
      {
        if (id == 116) scale.root = applied;
        else scale.mask[id - 117] = applied >= 0.5 ? 1 : 0;
        pushNoteLaw();
        return;
      }
      if (id == 38)
      {
        // The wheel sets a TARGET. With the law off the glide is a pass-through,
        // so this stays the instant write it has always been — byte-identical,
        // not merely equivalent. With a law on, the render advances toward it on
        // the bend grid.
        bendTarget = applied;
        if (!bendActive())
        {
          pitchBend = applied;
          bendGlide.reset(applied);
          updateTuneAll();
        }
        return;
      }
      if (id == 40)
      {
        // Clean engage, both placements: a stage that has been idle holds the
        // history of whenever it was last switched off, and B146 gave the
        // output stage a second one to forget.
        if (applied != 0 && bassMonoOn == 0) clearBassMonoState();
        bassMonoOn = applied;
        return;
      }
      if (id == 41)
      {
        bassMonoHz = applied;
        return;
      }
      if (id == 267)
      {
        /* B146. Moving the stage is an engage for whichever placement was not
           running, and the same clean-engage rule applies — otherwise
           switching pre -> post -> pre resumes a filter from a block that is
           now minutes old. Cheaper and more honest to clear both than to
           reason about which one survives the move. */
        const int want = (int)std::lround(applied);
        if (want != bassMonoPos) clearBassMonoState();
        bassMonoPos = want;
        return;
      }
      if (id == 100)
      {
        masterVol = applied;   // smoothing happens in process()
        return;
      }
      if (id == 101) { gSemi = applied; updateTuneAll(); return; }
      if (id == 102) { gFine = applied; updateTuneAll(); return; }
      if (id == 103) { gOct = applied; updateTuneAll(); return; }
      if (id == 43)
      {
        if (applied != engineSel)
        {
          allOffAll();
          spectra.allOff();
          heldCount = 0;
          monoSlot = -1;
          for (auto &t : tags) t.active = false;
        }
        engineSel = applied;
        return;
      }
      if ((id >= 44 && id <= 55) || (id >= 65 && id <= 68))  // 65-68: SPECTRA ADSR (ADR-055)
      {
        spectra.setParam(d->coreKey, applied);
        return;
      }
      if (id >= 57 && id <= 64)  // ADR-054 FX rack: type/amount pairs → rack
      {
        const int slot = (int)(id - 57) / 2;
        if (((id - 57) & 1) == 0)
        {
          /* Instance caps are enforced HERE, the one choke point every type
             write passes (GUI, host automation, preset load, morph): a type
             that is already held to its cap elsewhere is REFUSED and the slot
             keeps its type — readback reports the rack, so host and GUI see
             the refusal rather than a phantom second Comb.
             B188: a LOAD gets `claimType`, which additionally resolves a fade
             SHADOW that is the only thing standing in the way — a load arrives
             as a whole patch in one drain, so a patch that MOVES Comb between
             slots was refused against the shadow its own first write armed and
             lost the module. A live edit keeps `typeAllowed`, because during
             ordinary play the shadow is still rendering and still writing the
             shared bank (fxxfade_check T4). Neither weakens the cap: a LIVE
             second instance is refused on both paths. */
          if (!(loadingState ? rack.claimType(slot, (int)applied)
                             : rack.typeAllowed(slot, (int)applied)))
            return;
          rack.setType(slot, (int)applied);
        }
        else rack.setAmount(slot, applied);
        return;
      }
      /* B117 / ADR-163. Written straight through to the rack, which owns the
         handover; nothing else in the shell knows about it. Both are patch
         scope (the rack is ONE object), hence data-fixed on their controls. */
      if (id == 264) { rack.setXfade(applied >= 0.5); return; }
      if (id == 265) { rack.setXfadeMs(applied); return; }
      // ADR-176 decision 6: a flag and nothing else. No state is rebuilt here —
      // the resolver's tables are sized in morphInit and are correct whether or
      // not the flag has ever been on, so toggling it cannot allocate.
      if (id == 266) { intentBusOn = applied; return; }
      // B89 phase 2d: the pad's spring defeat. Like 266, a flag and nothing
      // else — padStep reads it per tick, so there is no state to rebuild.
      if (id == 268) { intentLatchOn = applied; return; }
      if (id >= 96 && id <= 99)  // per-slot second axis (comb resonance today)
      {
        rack.setTone((int)(id - 96), applied);
        return;
      }
      // Width: the SAW core calls it "width", SPECTRA calls it "swidth" — same
      // stereo-spread control, so one slider (id 14) drives both.
      if (id == 14) spectra.setParam("swidth", applied);
      // ADR-082: ids in a higher block address that oscillator's core. Osc 0
      // keeps every id it had, so this line is unchanged for existing patches.
      const uint32_t osc = oscOfId(id);
      // A GLOBAL core param means "the same value in every oscillator", not
      // "oscillator 0's value". oscOfId() returns 0 for every global id, so
      // this line used to write the Attack knob into cores[0] and nowhere else
      // — measured: with attack at 1.5 s, oscillator 1 reached 90% at 0.955 s
      // while oscillator 2 sat at 0.007 s, its compiled-in default. Every
      // global core param behaved that way, so a two-oscillator patch was half
      // configured and the second half silently ignored the panel.
      // Third instance of the same shape (after the note/lifecycle fan-out and
      // pan motion): an operation whose intent is "every oscillator" written
      // against one. See L0028.
      if (isGlobalId(id))
        for (uint32_t k = 0; k < kNumOsc; k++) cores[k].setParam(d->coreKey, applied);
      else if (osc < kNumOsc)
        cores[osc].setParam(d->coreKey, applied);
      spectra.setParam(d->coreKey, applied);  // shared-name knobs mirror; unknown keys no-op
      /* B171: the device seed re-rolls the LFOs' S&H streams here, in the same
         breath SwarmCore's own rebuild() re-rolls its ensemble stream — "change
         the seed" has to mean one thing across the device, or the S&H lane is
         the one modulator a re-seed cannot move. Last in applyParam so the
         core's rebuild has already run. */
      if (baseIdOf(id) == 3) lfoReseed();
    }
  }

  double readParam(clap_id id) const
  {
    if (const ParamDef *d = findParam(id))
    {
      // Shell-domain params first; everything else reads the core through the
      // SAME key map setParam uses — no parallel chain to drift (the
      // 2026-07-18 state bug: dynamics params were missing from a duplicated
      // read chain, so get_value fell through to 0 and state saved lies).
      // ADR-088: read the MATRIX, not a shadow copy — the state chunk, the
      // host readback and the GUI all land on the same numbers the audio pass
      // multiplies by, so a readback cannot report a topology that is not live.
      if ((uint32_t)id >= kRoutingIdBase) return getRoutingParam(id);
      /* ENGINE BLOCKS (B172) — read the CORE, not a shadow copy, for the reason
         the routing branch above reads the matrix: a readback that could
         report a value the audio path does not hold is the `readParam` half of
         the 2026-07-18 state bug this function's header records. Before every
         `baseIdOf` test, which would alias. */
      if (const EngineBlock *eb = engineBlockOf(id))
        return eb->base == kSubOscIdBase ? subGetParam(id) : 0.0;
      if (d->id == 11) return inertiaKnob;  // ADR-024 knob domain
      if (d->id == 70) return inertiaCurve;  // ADR-059 dev taper exponent
      if (d->id == 32) return voiceMono;
      if (d->id == 34) return voiceLegato;
      if (d->id == 104) return oscMute[oscOfId(id) < kNumOsc ? oscOfId(id) : 0];
      if (d->id == 105) return oscSolo[oscOfId(id) < kNumOsc ? oscOfId(id) : 0];
      if (d->id == 35) return octaveA[oscOfId(id) < kNumOsc ? oscOfId(id) : 0];
      if (d->id == 36) return semiA[oscOfId(id) < kNumOsc ? oscOfId(id) : 0];
      if (d->id == 37) return fineCentsA[oscOfId(id) < kNumOsc ? oscOfId(id) : 0];
      // The wheel's TARGET is the parameter; `pitchBend` is where the glide has
      // currently reached. Reporting the sounding value would make a host read
      // back something the user never set, and would fight automation mid-glide.
      if (d->id == 38) return bendTarget;
      if (d->id >= 106 && d->id <= 115)
      {
        switch (d->id)
        {
          case 106: return bendLaw.model;
          case 107: return bendLaw.gtime;
          case 108: return bendLaw.rate;
          case 109: return bendLaw.tau;
          case 110: return bendLaw.springF;
          case 111: return bendLaw.damp;
          case 112: return bendLaw.distOver;
          case 113: return bendLaw.retMul;
          case 114: return bendLaw.quant;
          case 115: return bendLaw.qhyst;
          default: break;
        }
      }
      /* NOTE LANE readback. Its absence is why "follow bend law" would not stick:
         applyParam stored the choice in `noteLink`, but readParam fell through to
         the ParamDef default, so the host's very next getParams() echoed 0 back
         and the selector snapped to "own settings". A parameter the shell OWNS
         must be readable from where the shell keeps it — the write half alone is
         a value the host can never see. */
      /* oscOfId(id), NOT d->id/1000: findParam(1150) returns the BASE def, so
         d->id/1000 is always 0 and oscillator 2's readback mirrored oscillator
         1 forever -- "osc 2 says it's on, but it isn't; the only way to toggle
         it on is to turn osc 1 off first" (human 2026-08-21): with osc 1 off,
         the mirror finally showed off, so the toggle finally sent 1. The
         truth-sweep gate in paramscope_check now makes this class unshippable. */
      if (baseIdOf(d->id) == 150) return oscEnabled[oscOfId(id) < kNumOsc ? oscOfId(id) : 0];
      if (d->id == 159) return morphArm;
      if (d->id == 151) return morphOn;
      if (d->id == 152) return morphX;
      if (d->id == 153) return morphY;
      if (d->id == 154) return morphTemp;
      if (d->id == 155) return morphCoup;
      if (d->id == 156) return (double)morphSeed;
      if (d->id == 157) return morphMode;
      if (d->id == 158) return morphGlideS;
      if (d->id == 149) return mpeBendLaw;
      if (d->id == 146) return qTimeMode;
      if (d->id == 147) return qTimeHz;
      if (d->id == 148) return qTimeSync;
      if (d->id >= 137 && d->id <= 145)
      {
        switch (d->id)
        {
          case 137: return noteLink;
          case 138: return noteLawOwn.model;
          case 139: return noteLawOwn.gtime;
          case 140: return noteLawOwn.rate;
          case 141: return noteLawOwn.springF;
          case 142: return noteLawOwn.damp;
          case 143: return noteLawOwn.distOver;
          case 144: return noteLawOwn.quant;
          case 145: return noteLawOwn.qhyst;
          default: break;
        }
      }
      if (d->id >= 133 && d->id <= 136) return rack.getMix((int)(d->id - 133));
      if (d->id >= 200 && d->id <= 231)
        return rack.getTimeParam((int)((d->id - 200) / 8), (int)((d->id - 200) % 8));
      if (d->id >= 232 && d->id <= 263)
        return rack.getDelayParam((int)((d->id - 232) / 8), (int)((d->id - 232) % 8));
      if (d->id == 264) return rack.getXfade();      // B117: the rack owns both
      if (d->id == 265) return rack.getXfadeMs();    // (clamped there, so readback is the truth)
      if (d->id == 266) return intentBusOn;          // ADR-176: the shell owns it, so it reads back
      if (d->id == 268) return intentLatchOn;        // B89 2d, same shape
      if (d->id == 161)
      {
        const int pr = modPitchRouteIdx();
        return pr >= 0 ? mod.routes[pr].depth : 0.0;
      }
      if (const ModDest *md = const_cast<Plugin *>(this)->modDestFor(d->id, false))
        return md->base;
      if (d->id == 162) return env2A;
      if (d->id == 163) return env2D;
      if (d->id == 164) return env2S;
      if (d->id == 165) return env2R;
      if (d->id >= 269 && d->id <= 280)   // B171 LFO 1/2
      {
        const int i = (d->id - 269) / 6;
        switch ((d->id - 269) % 6)
        {
          case 0: return lfoRate[i];
          case 1: return lfoShape[i];
          case 2: return lfoSync[i];
          case 3: return lfoBeats[i];
          case 4: return lfoRetrig[i];
          default: return lfoPhase0[i];
        }
      }
      if (d->id >= 281 && d->id <= 288)   // B171 ENV 3/4
      {
        const int x = (d->id - 281) / 4;
        switch ((d->id - 281) % 4)
        {
          case 0: return xenvA[x];
          case 1: return xenvD[x];
          case 2: return xenvS[x];
          default: return xenvR[x];
        }
      }
      if (d->id >= 166 && d->id <= 173) return macroVal[d->id - 166];
      if (d->id >= 174 && d->id <= 177) return xyAsn[d->id - 174];
      if (d->id == 179 || d->id == 180) return mainAsn[d->id - 179];
      if (baseIdOf(d->id) == 181)
        return pitchContA[oscOfId(id) < kNumOsc ? oscOfId(id) : 0];
      if (d->id == 178) return specimenOn;
      if (d->id >= 116 && d->id <= 128)
        return d->id == 116 ? scale.root : (double)scale.mask[d->id - 117];
      if (d->id == 40) return bassMonoOn;
      if (d->id == 41) return bassMonoHz;
      if (d->id == 267) return (double)bassMonoPos;   // B146
      if (d->id == 100) return masterVol;
      if (d->id == 101) return gSemi;
      if (d->id == 102) return gFine;
      if (d->id == 103) return gOct;
      if (d->id == 43) return engineSel;
      if ((d->id >= 44 && d->id <= 55) || (d->id >= 65 && d->id <= 68))  // SPECTRA (44-55) + SPECTRA ADSR (ADR-055, 65-68)
        return const_cast<Plugin *>(this)->spectra.getParam(d->coreKey);
      if (d->id >= 57 && d->id <= 64)  // ADR-054 FX rack readback (state/get_value)
      {
        const int slot = (int)(d->id - 57) / 2;
        return ((d->id - 57) & 1) == 0 ? (double)rack.getType(slot) : rack.getAmount(slot);
      }
      if (d->id >= 96 && d->id <= 99) return rack.getTone((int)(d->id - 96));
      // ADR-082: read from the oscillator the id addresses. applyParam was
      // routed by oscillator and this was not, so state_save wrote every
      // `o<k>.` key by reading OSCILLATOR 0 — and state_check's
      // "every param round-trips exactly" passed anyway, because it compares
      // two reads through the same broken accessor. Only the audio comparison
      // caught it. Write path and read path must be routed together.
      const uint32_t osc = oscOfId(id);
      return osc < kNumOsc ? cores[osc].getParam(d->coreKey)
                           : core.getParam(d->coreKey);
    }
    return 0;
  }

  // Shared by NOTE_OFF, NOTE_CHOKE, and the MIDI 1.0 vel-0 convention below.
  void handleNoteOff(const clap_event_note_t *n)
  {
    if (n->key < 0)
    {
      allOffAll();
      spectra.allOff();
      heldCount = 0;
      return;
    }
    if (spectraMode())
    {
      spectra.noteOff(n->key);
      return;
    }
    if (voiceMono != 0)
    {
      // Remove EVERY entry for this key, not just the first. The old loop
      // `break`s on the first match, so a duplicated entry survived a note-off
      // and became a PHANTOM held key — see the note-on guard for how one got
      // in and why that hung the voice. With that guard in place duplicates
      // cannot occur, so this is an invariant restore rather than a second fix:
      // if one ever slips in (a 16-entry overflow drop, or a host sending an
      // off for a key we never saw an on for), a leftover entry is exactly what
      // hangs the voice. Order is preserved, so last-note priority is unchanged.
      {
        int w = 0;
        for (int i = 0; i < heldCount; i++)
          if (heldStack[i].key != n->key) heldStack[w++] = heldStack[i];
        heldCount = w;
      }
      /* THE SUB HEARS EVERY RELEASE, INCLUDING THE ONES THE SWARM SWALLOWS
         (B181 note 2). In swarm-mono a key released while it is NOT the
         sounding one produces no call at all below — `retargetAll` only runs
         for the sounding key — so the sub's own held stack would keep it for
         ever and `lowest` would chase a key nobody is holding.
         BIT-INERT FOR THE EXISTING PATH: outside sub-mono this releases at
         most the one slot whose subKey matches, and the retarget three lines
         down immediately re-strikes that same slot with `noteOn`, which
         REPLACES the note regardless (subosc_core.h:237). No sample is
         rendered between the two, so nothing changes for a patch with
         `sub.mono` off. */
      subNoteOff(n->key);
      if (monoSlot >= 0 && core.voiceAt(monoSlot).midi == n->key)
      {
        if (heldCount > 0)
        {
          const Held &top = heldStack[heldCount - 1];
          retargetAll(monoSlot, top.key, top.freq, voiceLegato != 0);
          tags[monoSlot].key = top.key;
        }
        else
        {
          noteOffAll(n->key);
        }
      }
    }
    else
    {
      noteOffAll(n->key);
    }
  }

  void handleEvent(const clap_event_header_t *ev)
  {
    if (ev->space_id != CLAP_CORE_EVENT_SPACE_ID) return;
    switch (ev->type)
    {
      case CLAP_EVENT_NOTE_ON:
      {
        auto *n = reinterpret_cast<const clap_event_note_t *>(ev);
        recordNote(ev, n);
        sawNotes.fetch_add(1, std::memory_order_relaxed);
        if (n->channel > 0) sawNonZeroChan.fetch_add(1, std::memory_order_relaxed);
        // MIDI 1.0: note-on velocity 0 IS a note-off, and the AU wrapper
        // forwards controller 0x90-vel-0 releases verbatim (ADR-038). This
        // synth ignores velocity, so without the remap such a release struck
        // a fresh full-gain voice that no note-off ever ends — the
        // 2026-07-18 "doesn't stop when you let go" hang.
        if (n->velocity <= 0.0)
        {
          handleNoteOff(n);
          break;
        }
        const double freq = 440.0 * std::pow(2.0, (n->key - 69) / 12.0);
        // ADR-071: note-context feed for the rack's per-note comb — common to
        // both engines (the comb resonates whatever is played, SAW or SPECTRA).
        rack.noteOn(n->key, freq);
        if (spectraMode())
        {
          // SPECTRA v1: plain poly (mono/glide are SAW-side features; ADR-037).
          // No MPE bend re-apply here — SpectraCore has no noteTune (ADR-038's
          // per-note pitch is SAW-side until the kernel unification).
          const int slot = spectra.noteOn(n->key, freq);
          // B172: the sub follows the note in BOTH engines. It is a routing
          // SOURCE beside them, not a feature of the swarm, so a sub that went
          // silent when the engine selector moved would be a source that exists
          // only in one mode — with nothing saying so.
          subNoteOn(slot, n->key, n->velocity);
          retireTag(slot);
          lastNoteKey = n->key;
          // ADR-162: this slot's pitch envelope restarts (from its current
          // level); the notes already held are untouched. The composer's cache
          // is cleared with it — the core just reset this voice's noteTune.
          penv[slot].retrig = true;
          resetNoteExpr(slot);
          tags[slot] = {n->note_id, n->port_index, n->channel, n->key, true, (float)n->velocity};
          srcVel = n->velocity;   // ADR-149: matrix source 14
          break;
        }
        int struck;
        if (voiceMono != 0)
        {
          // Glide/legato engage only when another key is still HELD (human
          // clarification 2026-07-18) — a ringing release tail alone gets a
          // fresh strike on a new slot, overlapping the tail naturally.
          // A mono held-stack is the set of keys currently DOWN, so a key
          // cannot appear in it twice. This used to push unconditionally, so a
          // duplicate NOTE_ON for an already-held key — which a computer
          // keyboard played fast produces and a piano roll never does — pushed a
          // second entry. The note-off path then removed only one of them and
          // saw heldCount > 0, so it RETARGETED the voice to the phantom key
          // instead of releasing it, and the note hung forever. That is the
          // human's 2026-07-26 report ("notes get stuck for longer than they
          // ought to when I play quickly ... hasn't happened with preprogrammed
          // MIDI in the piano roll") — it read as finite only because a later
          // press-and-release of the same key cleared the phantom.
          // Measured: mono+restrike went 0/25 seeds silent -> 25/25.
          // A re-press is therefore "move to the top" (last-note priority), and
          // `anotherHeld` is evaluated AFTER that removal, so re-pressing the
          // ONLY held key is a fresh strike rather than a retarget to itself.
          int dupAt = -1;
          for (int i = 0; i < heldCount; i++)
            if (heldStack[i].key == n->key) { dupAt = i; break; }
          if (dupAt >= 0)
          {
            for (int j = dupAt; j < heldCount - 1; j++) heldStack[j] = heldStack[j + 1];
            heldCount--;
          }
          const bool anotherHeld = heldCount > 0;
          /* ADR-126: DROP-OLDEST on overflow, ratified 2026-08-26. The old
             `if (heldCount < 16)` silently discarded the NEWEST key -- not a
             considered choice, just a bound written to be safe rather than
             musical, and the promise to change it (made to FOUNDATIONS
             2026-08-11) went unkept for a fortnight.
             Measured cost of the old behaviour: overflowing by ONE
             self-corrects, because the sounding note is tracked separately in
             `core.voiceAt(monoSlot).midi` and the release path only retargets
             when the released key IS the sounding one. Overflowing by TWO
             forgot the intermediate key entirely -- hold 40..55, press 70,
             press 71, release 71, and 55 sounds while 70 is still physically
             held. Drop-oldest keeps the fallback chain anchored to what the
             player most recently played, which is what last-note priority
             means.
             The cost we accepted in that answer: the evicted key's later
             note-off matches nothing. That key was NOT sounding (in mono only
             the top of the stack sounds), so all that is lost is its
             availability as a fallback after the newer keys release -- a much
             smaller harm than a key press that is silently forgotten. */
          if (heldCount < 16) heldStack[heldCount++] = {n->key, freq};
          else
          {
            for (int j = 0; j < 15; j++) heldStack[j] = heldStack[j + 1];
            heldStack[15] = {n->key, freq};
          }
          const bool voiceGated = monoSlot >= 0 && core.voiceAt(monoSlot).gate;
          if (anotherHeld && voiceGated)
          {
            const bool keep = voiceLegato != 0;
            retargetAll(monoSlot, n->key, freq, keep);
          }
          else
          {
            // MONO INVARIANT: at most ONE gated voice. Taking the fresh-strike
            // path while the previous mono voice is still GATED orphans it —
            // every release path keys off monoSlot's current midi, so once
            // monoSlot moves on, nothing can ever release the orphan.
            // Minimal repro found by notefuzz_check --minimal:
            //   on(61) on(61) on(60) off(61) off(60)  -> voice on 61 hangs.
            // The re-press sends the second on down this path, the on(60)
            // retargets monoSlot away, and the off(61) then finds
            // monoSlot.midi != 61 and does nothing at all.
            // Only a GATED voice is force-released here: a ringing RELEASE tail
            // has gate == 0, so the intended tail-overlap behaviour above is
            // untouched.
            if (monoSlot >= 0 && core.voiceAt(monoSlot).gate)
              noteOffAll(core.voiceAt(monoSlot).midi);
            monoSlot = core.noteOn(n->key, freq);
            core.setNoteVelocity(monoSlot, n->velocity);
            subNoteOn(monoSlot, n->key, n->velocity);   // B172
            bindSlots(monoSlot, 0, monoSlot);
            for (uint32_t k = 1; k < kNumOsc; k++)
            {
              const int sk = cores[k].noteOn(n->key, freq);
              cores[k].setNoteVelocity(sk, n->velocity);
              bindSlots(monoSlot, k, sk);   // sk may differ from monoSlot
            }
          }
          retireTag(monoSlot);
          lastNoteKey = n->key;
          // ADR-162: this monoSlot's pitch envelope restarts (from its current
          // level); the notes already held are untouched. The composer's cache
          // is cleared with it — the core just reset this voice's noteTune.
          penv[monoSlot].retrig = true;
          resetNoteExpr(monoSlot);
          tags[monoSlot] = {n->note_id, n->port_index, n->channel, n->key, true, (float)n->velocity};
          struck = monoSlot;
        }
        else
        {
          const int slot = core.noteOn(n->key, freq);
          core.setNoteVelocity(slot, n->velocity);
          subNoteOn(slot, n->key, n->velocity);   // B172
          bindSlots(slot, 0, slot);
          for (uint32_t k = 1; k < kNumOsc; k++)
          {
            const int sk = cores[k].noteOn(n->key, freq);
            cores[k].setNoteVelocity(sk, n->velocity);
            bindSlots(slot, k, sk);   // sk may differ from slot
          }
          retireTag(slot);
          lastNoteKey = n->key;
          // ADR-162: this slot's pitch envelope restarts (from its current
          // level); the notes already held are untouched. The composer's cache
          // is cleared with it — the core just reset this voice's noteTune.
          penv[slot].retrig = true;
          resetNoteExpr(slot);
          tags[slot] = {n->note_id, n->port_index, n->channel, n->key, true, (float)n->velocity};
          srcVel = n->velocity;   // ADR-149: matrix source 14
          struck = slot;
        }
        // ADR-038: a fresh strike resets noteTune (ADR-036), so re-apply the
        // channel's latched MPE bend — MPE hosts sent it before this note-on.
        if (n->channel >= 1 && n->channel < 16 && mpeBendSemis[n->channel] != 0.0)
          seedNoteBend(struck, mpeBendSemis[n->channel]);
        break;
      }
      case CLAP_EVENT_NOTE_OFF:
      case CLAP_EVENT_NOTE_CHOKE:
      {
        // Single note-off path (spectra dispatch lives inside handleNoteOff;
        // the vel-0 NOTE_ON remap above routes through the same code).
        recordNote(ev, reinterpret_cast<const clap_event_note_t *>(ev));
        handleNoteOff(reinterpret_cast<const clap_event_note_t *>(ev));
        break;
      }
      case CLAP_EVENT_NOTE_EXPRESSION:
        sawExprs.fetch_add(1, std::memory_order_relaxed);
      {
        // MPE per-note pitch (ADR-036): hosts deliver per-note bend as the
        // TUNING expression in relative semitones; CLAP wildcard matching
        // (-1) applies. Reaches the core through the ADR-027 live-tune seam.
        auto *x = reinterpret_cast<const clap_event_note_expression_t *>(ev);
        // ADR-084: PRESSURE -> per-voice gain (default mapping the human asked
        // for). Same tag-matching as TUNING; fan out to every oscillator, since
        // note fan-out keeps slot indices aligned.
        if (x->expression_id == CLAP_NOTE_EXPRESSION_PRESSURE)
        {
          for (int i = 0; i < hypersaw::kPoly; i++)
            if (tags[i].active &&
                (x->note_id == -1 || tags[i].noteId == x->note_id) &&
                (x->key == -1 || tags[i].key == x->key) &&
                (x->channel == -1 || tags[i].channel == x->channel))
            {
              setNotePressureAll(i, x->value);
            }
          srcPress = x->value;   // ADR-149: matrix source 16
          break;
        }
        if (x->expression_id != CLAP_NOTE_EXPRESSION_TUNING) break;
        for (int i = 0; i < hypersaw::kPoly; i++)
        {
          if (!tags[i].active) continue;
          const NoteTag &t = tags[i];
          if ((x->note_id == -1 || x->note_id == t.noteId) &&
              (x->port_index == -1 || x->port_index == t.port) &&
              (x->channel == -1 || x->channel == t.channel) &&
              (x->key == -1 || x->key == t.key))
            setNoteBendTarget(i, x->value);
        }
        break;
      }
      case CLAP_EVENT_MIDI:
      {
        // MPE member-channel pitch bend (ADR-038). Live (VST3, via the
        // wrapper's IMidiMapping params) and Logic (AU, raw MIDI) deliver
        // MPE bend as per-channel 0xE0 on rotating member channels 2-16 —
        // NOT as note expressions — at the MPE default range of ±48 st.
        // Channel 1 (index 0) is excluded: see mpeBendSemis.
        auto *m = reinterpret_cast<const clap_event_midi_t *>(ev);
        const int ch = m->data[0] & 0x0F;
        /* ADR-149: CC1 and channel pressure were DROPPED here until now — the
           handler read only 0xE0. They become matrix sources 15 and 16. */
        if ((m->data[0] & 0xF0) == 0xB0 && m->data[1] == 1)
        { srcWheel = m->data[2] / 127.0; break; }
        if ((m->data[0] & 0xF0) == 0xD0)
        { srcPress = m->data[1] / 127.0; break; }
        if ((m->data[0] & 0xF0) != 0xE0) break;
        const int v14 = (int)m->data[1] | ((int)m->data[2] << 7);
        if (ch == 0)
        {
          /* THE PLAIN PITCH WHEEL. Channel 0 is the MPE manager / ordinary
             single-channel MIDI, and until 2026-08-19 it was dropped entirely —
             the exclusion below rightly refused to read a ±2 st wheel at the
             ±48 st MPE range, but nothing else picked it up, so a wheel on a
             normal DAW track never reached the engine at all. Every bend-law
             session tested against the GUI's Pitch control (param 38) and
             passed, while the human's hand was on the wheel: "none of the bend
             laws actually make the pitch bend." Routed through applyParam(38)
             — the exact path the GUI control takes — so the bend law shapes
             wheel and slider identically, and with the law off it stays the
             same instant write it always was. ±2 st is the MIDI 1.0 default
             and the MPE manager-channel default; a bend-range param can widen
             it later without touching this site. */
          applyParam(38, (v14 - 8192) * (2.0 / 8192.0));
          srcPitchW = (v14 - 8192) / 8192.0;   // ADR-149: matrix source 17, bipolar
          break;
        }
        const double semis = (v14 - 8192) * (48.0 / 8192.0);
        mpeBendSemis[ch] = semis;
        for (int i = 0; i < hypersaw::kPoly; i++)
          if (tags[i].active && tags[i].channel == ch) setNoteBendTarget(i, semis);
        break;
      }
      case CLAP_EVENT_PARAM_VALUE:
      {
        auto *pv = reinterpret_cast<const clap_event_param_value_t *>(ev);
        applyParam(pv->param_id, pv->value);
        break;
      }
      case CLAP_EVENT_TRANSPORT:
      {
        auto *tr = reinterpret_cast<const clap_event_transport_t *>(ev);
        if (tr->flags & CLAP_TRANSPORT_HAS_TEMPO) core.p.bpm = tr->tempo;
        break;
      }
      default:
        break;
    }
  }

  /* ONE span of oscillator rendering, extracted so the bend grid can cut a block
     into grid-sized pieces without a second copy of this logic. Two copies of a
     mix stage is how they disagree — the same reason the generated GUI derives
     its controls instead of hand-placing them. */
  void renderSpan(float *outL, float *outR, uint32_t at, uint32_t count)
  {
    const int n = (int)count;
    if (oscEnabled[0] == 0)
    {
      // Osc 0 renders STRAIGHT into the output buffer, so its skip must do the
      // zeroing render() would have done. Meter to 0 for the same reason as
      // ADR-099: a dead oscillator must not hold its last peak.
      for (int i = 0; i < n; i++) { outL[at + i] = 0.0f; outR[at + i] = 0.0f; }
      oscPeakViz[0] = 0.0;
    }
    else
      core.render(outL + at, outR + at, n);
    // Oscillator 0 renders STRAIGHT into the output, so its mute/solo gain
    // and meter are applied in place afterwards rather than during a sum.
    if (oscEnabled[0] != 0) applyOscGainAndMeter(0, outL + at, outR + at, n, false);
    // Oscillators 1..N-1 render into a FIXED STACK buffer, in chunks, and
    // land in their own SOURCE buffer (B23 increment 3 — they used to sum into
    // oscillator 0's). At their default vol = 0 they contribute exact zeros
    // through a coefficient of exactly 1.0, so a patch that never touches them
    // is bit-identical to a one-oscillator build — which is what keeps the 147
    // parity goldens green.
    //
    // Stack, not a heap scratch. The first version sized a std::vector at
    // activate() and skipped the oscillator when the buffer was too small;
    // that made AUDIBLE OUTPUT conditional on activate() having run, so a
    // restored instance silently lost oscillator 1 (state_check caught it:
    // "restored instance renders bit-identical audio" went red). A chunk
    // loop over a fixed buffer cannot allocate, cannot depend on block
    // size, and cannot silently drop a voice.
    for (uint32_t k = 1; k < kNumOsc; k++)
    {
      /* ADR-099: a fully-silent oscillator is SKIPPED, not rendered-and-zeroed.
         Its contribution is exact zeros in both silent cases — the core's own
         `p.vol` multiplies inside render(), and a settled mute multiplies after
         it — so summing was pure identity and cost a full swarm anyway.
         Measured: osc2 at its default vol 0 cost the SAME as osc2 audible
         (5.68% vs 5.69% of a core), i.e. half the render bill of every
         single-oscillator patch bought nothing.
         The traded behaviour, stated: while skipped the core's envelopes and
         phases FREEZE, so raising the volume mid-held-note resumes the voice
         from where it paused instead of where it would have decayed to. That is
         the "muted layer costs nothing" contract every DAW mixer teaches.
         Deliberately NOT gain-smoothed-out mid-ramp: the skip waits for the
         smoother to SETTLE at 0, so a fade-out completes before the core stops.
         The meter is forced to 0 — a skipped oscillator must not hold its last
         peak on the mixer. */
      /* ADR-099 Amendment 1 (human ruling 2026-08-21): skip ONLY when the
         oscillator is switched OFF. The vol-0/settled-mute skip bought CPU by
         freezing the core, and a frozen core freezes its VISUALS — so a silent
         osc 2 half-rendered while a silent osc 1 (never skipped) kept moving:
         "currently it just looks like a mistake." It did. Volume is volume;
         OFF is the no-cost state — and osc 2 now SHIPS off, so the default
         patch keeps the cheap path through the honest switch instead of
         through a silently frozen core. */
      if (oscEnabled[k] == 0)
      {
        oscPeakViz[k] = 0.0;
        continue;
      }
      /* B23 increment 3: oscillator k IS routing source k, so its chunk is
         KEPT (written, not summed) into that source's block buffer instead of
         being added to oscillator 0's. The old sum is now the matrix's
         default — slot 0 gathers `1.0*src0 + 1.0*src1` in this same order, and
         the gather accumulates in float exactly as `outL[i] += tL[i]` did, so
         the summed result is bit-identical rather than merely equivalent.
         The buffers were zeroed for the whole block before the span loop, so a
         disabled or skipped oscillator leaves a SILENT source rather than a
         stale one. */
      float tL[kMixChunk], tR[kMixChunk];
      for (int off = 0; off < n; off += kMixChunk)
      {
        const int m = n - off < kMixChunk ? n - off : kMixChunk;
        cores[k].render(tL, tR, m);
        applyOscGainAndMeter(k, tL, tR, m, true);
        for (int i = 0; i < m; i++)
        {
          srcBufL[k - 1][at + off + i] = tL[i];
          srcBufR[k - 1][at + off + i] = tR[i];
        }
      }
    }
    renderSubSpan(at, count);
  }

  /* ---- SUB OSC INTO ROUTING SOURCE ROW 2 (B172) ----------------------------
     SOURCE 2, so the block buffer is srcBuf[1] (source 0 is the output buffer;
     the comment on srcBufL says why). Sixteen per-voice cores SUM into it,
     mono — SubOscCore writes the same sample to both channels by construction
     (SPEC-SUBOSC §2: pan and width are the voice's, not a sub's).

     OFF COSTS NOTHING AND CHANGES NOTHING. With the gate at 0 this returns
     before touching a buffer, so the source stays the zeros process() wrote at
     the top of the block, the matrix gathers exact 0.0f through a coefficient
     of exactly 1.0, and every pre-B172 patch renders bit-identically — which is
     what subosc_check's control row measures rather than assumes.

     THE ROW'S LEVEL LAW — HEADROOM, AND IT IS LOAD-BEARING (phase 1 finding 1).
     The core's tone stage is a TPT one-pole, and a TPT one-pole OVERSHOOTS on a
     step when its cutoff sits near Nyquist: at level 1 and tone 20 kHz the core
     peaks above unity for every edged shape. MEASURED on this build, worst over
     {44.1, 48, 96} kHz x MIDI 12..96 x width {0.05, 0.27, 0.5, 0.95}, bumpAmt
     at its 0.6 ceiling:
         sine 1.0025   triangle 0.9998   square 1.1221   saw 1.1208
         pulse 1.1883  noise 1.4012      bump 1.0029
     — worst 1.4012 (noise, 48 kHz), and SPEC-SUBOSC's phase-1 finding records
     1.425 for the same shape over a wider sweep. The DIVISOR IS THE LARGER of
     the two, so the bound holds under both measurements: a sub at level 1 into
     a unity path peaks at 1.4012/1.425 = 0.983 and CANNOT reach the rail. That
     is a property of the constant, not of the patch — no vigilance, no
     limiter, and nothing a mod route can undo.
     Why a constant and not a per-shape gain: a gain that changed with `wave`
     would make the waveform selector a level control too, and the shape
     selector is exactly where a player does not want one. The cost accepted is
     that sine and triangle are 3.07 dB below where they could sit; `level`
     (default 0.8) is the control that answers that, and it costs one turn. */
  static constexpr double kSubRowHeadroomPeak = 1.425;   // SPEC-SUBOSC phase 1 finding 1
  static constexpr double kSubRowHeadroom = 1.0 / kSubRowHeadroomPeak;   // 0.7018 = -3.07 dB
  void renderSubSpan(uint32_t at, uint32_t count)
  {
    if (subOn == 0) return;
    const int n = (int)count;
    float tL[kMixChunk], tR[kMixChunk];
    for (int off = 0; off < n; off += kMixChunk)
    {
      const int m = n - off < kMixChunk ? n - off : kMixChunk;
      /* ---- THE SUB'S PITCH, COMPOSED ONCE PER CHUNK (B181 notes 2 and 4) ---
         TWO contributors — the mono glide and the pitch-mod offset — and ONE
         writer, because two hands on the same quantity is the last-writer-wins
         bug the note-expression composer exists to prevent (L0029, ADR-162).

         THE GLIDE IS IN SECONDS, converted here and nowhere else (ADR-009):
         `dt` is this chunk's own duration at the running sample rate, so the
         journey takes `subGlide` seconds at 44.1, 48 and 96 kHz alike. The
         GRANULARITY is the chunk (kMixChunk = 256 samples, 5.8 ms at 44.1 kHz)
         because the core recomputes its phase increment once per render() call
         — a finer grid would mean changing a parity-gated loop, which this
         note does not buy.

         BIT-INERT AT THE DEFAULTS: with mono off and pitch mod 0, every
         instance is handed exactly 0.0 and §4's law reads `m + 0.0`, which is
         `m`. subosc_check's 11g row measures that rather than assuming it. */
      if (subMono != 0 && subGlideRate > 0 && subGlideCur != subGlideTo)
      {
        const double dt = (double)m / sampleRate;
        const double step = subGlideRate * dt;
        if (subGlideTo > subGlideCur)
          subGlideCur = subGlideCur + step > subGlideTo ? subGlideTo : subGlideCur + step;
        else
          subGlideCur = subGlideCur - step < subGlideTo ? subGlideTo : subGlideCur - step;
      }
      /* ---- THE GATE'S LEVEL RAMP (B203) -----------------------------------
         `subOnW` is the bilinear weight of the corners holding the gate ON
         (morphApplyGateEnable); this carries it through the same ~8 ms
         one-pole the oscillator enables use. ONCE PER CHUNK AND OUTSIDE THE
         SLOT LOOP: the gate is the ROW's switch, so advancing the smoother
         inside the sixteen-slot accumulation would run it sixteen times per
         sample and the ramp would be 16x too fast.
         BIT-INERT AT 1.0: `x * headroom * 1.0` is `x * headroom` exactly, so
         a patch that never morphs its gate renders sample-for-sample as it
         did before this existed — 11j.a measures that rather than assuming
         it. The stack array is the same shape as tL/tR: no allocation. */
      double gate[kMixChunk];
      {
        const double c = gainSmoothCoef();
        double g = subOnGainSm;
        for (int i = 0; i < m; i++)
        {
          if (g != subOnW)
          {
            g += (subOnW - g) * c;
            if (std::fabs(g - subOnW) < 1e-6) g = subOnW;
          }
          gate[i] = g;
        }
        subOnGainSm = g;
      }
      for (int s = 0; s < hypersaw::kPoly; s++)
      {
        subs[s].pitchOffsetSt =
            subPitchMod + (subMono != 0 && s == 0 ? subGlideCur : 0.0);
        /* HARD SYNC IS RETIRED, NOT DEFERRED (B184, human 2026-09-20).
           It used to be a recorded refusal: SPEC-SUBOSC §6 wanted oscillator
           1's fundamental phase per sample, SwarmCore published none, so the
           shell passed nullptr and the parameter was inert. The human struck
           the feature rather than pay for the source, so `render` no longer
           takes a master phase at all and id 4011 is a reserved, ignored slot
           (see kSubOscParams). subosc_check's 11e RETIREMENT pin measures that
           writing 4011 moves not one sample, with a control proving the
           comparison can fail — the absence is tested, not remembered (L0036).
           Sync for the SWARM oscillators is a separate row (B185). */
        subs[s].render(tL, tR, m);
        /* ACCUMULATE, never write. process() zeroes every source buffer for
           the whole block before the span loop, so `+=` over sixteen voices
           and however many spans the event list splits the block into is
           correct by that zeroing — the same contract the oscillators' `=`
           relies on from the other side. */
        for (int i = 0; i < m; i++)
        {
          srcBufL[1][at + off + i] += (float)(tL[i] * kSubRowHeadroom * gate[i]);
          srcBufR[1][at + off + i] += (float)(tR[i] * kSubRowHeadroom * gate[i]);
        }
      }
    }
  }

  /* ADR-035's bass-mono stage: ONE 2nd-order TPT SVF high-pass on the SIDE
     channel (L = M + HP(S), R = M − HP(S)). Extracted for B146, which runs it
     in TWO places, and extracted rather than copied for the reason renderSpan
     states: two copies of a mix stage is how they disagree.

     THE STATE IS THE CALLER'S. Both placements are the same filter and must
     never be the same filter INSTANCE — under `both` they run in series inside
     one block, so a shared pair would feed the post stage the pre stage's
     integrator history. Passing the state in makes that structural instead of
     remembered. */
  void bassMonoStage(float *L, float *R, uint32_t n, double &ic1, double &ic2) const
  {
    constexpr double kPi = 3.141592653589793;
    const double fc = std::min(bassMonoHz, 0.45 * sampleRate);
    const double g = std::tan(kPi * fc / sampleRate);
    const double k = 1.4142135623730951;  // Butterworth 2nd order
    const double a0 = 1.0 / (1.0 + g * (g + k));
    for (uint32_t i = 0; i < n; i++)
    {
      const double m = 0.5 * (L[i] + R[i]);
      const double sIn = 0.5 * (L[i] - R[i]);
      const double hp = (sIn - (g + k) * ic1 - ic2) * a0;
      const double v1 = g * hp;
      const double bp = v1 + ic1;
      ic1 = bp + v1;
      const double v2 = g * bp;
      ic2 = v2 + ic2 + v2;
      L[i] = (float)(m + hp);
      R[i] = (float)(m - hp);
    }
  }

  clap_process_status process(const clap_process_t *p)
  {
    // Host tempo drives the grid law (ADR-022); fallback stays at the last
    // known (or default 120) when the host provides none.
    if (p->transport && (p->transport->flags & CLAP_TRANSPORT_HAS_TEMPO))
      core.p.bpm = p->transport->tempo;

    drainQueue(p->out_events);

    float *outL = p->audio_outputs[0].data32[0];
    float *outR = p->audio_outputs[0].data32[1];
    const uint32_t nframes = p->frames_count;
    /* B23 increment 3: the source buffers are fixed-size, so a block past them
       is REFUSED rather than truncated — see their declaration. plug_activate
       refuses the same ceiling up front; this is the belt for a host that
       processes without activating, or that exceeds its own declared maximum. */
    if (nframes > kSrcBufFrames) return CLAP_PROCESS_ERROR;
    /* Sources 1.. start the block SILENT. Zeroing here rather than at each
       skip site is what makes "a disabled oscillator's source is silent" true
       for every path through the span loop at once — the SPECTRA branch, a
       switched-off oscillator, and a span the bend grid never reaches. */
    for (int s = 1; s < kRoutingNSrc; s++)
      for (uint32_t i = 0; i < nframes; i++) { srcBufL[s - 1][i] = 0.0f; srcBufR[s - 1][i] = 0.0f; }
    /* Absolute sample position for the forensic trace. NEVER derived from
       steady_time alone: the first real field dump (2026-08-12, Live via the
       VST3 wrapper) came back with every pos under 512 and NON-MONOTONIC —
       327, 146, 451, 17 — because the host reports steady_time as 0 every
       block, so `pos` was just the in-block offset and events from different
       blocks interleaved meaninglessly. The one column a replay depends on was
       the one that was wrong, and it was wrong in the only environment that
       matters. Count blocks locally and ALWAYS advance; use steady_time only
       as a bonus when the host supplies something plausible. */
    tracePos += nframes;
    blockPos = p->steady_time > 0 ? (uint64_t)p->steady_time : tracePos;
    const uint32_t nev = p->in_events->size(p->in_events);

    uint32_t frame = 0, evIndex = 0;
    while (frame < nframes)
    {
      uint32_t until = nframes;
      while (evIndex < nev)
      {
        const clap_event_header_t *ev = p->in_events->get(p->in_events, evIndex);
        if (ev->time > frame)
        {
          until = ev->time < nframes ? ev->time : nframes;
          break;
        }
        handleEvent(ev);
        ++evIndex;
      }
      /* BEND GRID. Subdividing is deliberately conditional: with no law engaged
         the render takes exactly the span it always took, so this fold cannot
         move a single sample of existing output — the parity claim is by
         CONSTRUCTION, not by measurement agreeing afterwards. When a law IS
         engaged the span is cut on the fixed grid and the tune factor is
         recomputed at each boundary, which is where the bench measured it. */
      if (morphOn > 0.5) morphStep((int)(until - frame));
      modStep((int)(until - frame));
      if (bendActive() && !spectraMode())
      {
        const int grid = bendGridSamples();
        while (frame < until)
        {
          const uint32_t take = (uint32_t)std::min<int>(grid - bendAccum, (int)(until - frame));
          renderSpan(outL, outR, frame, take);
          frame += take;
          bendAccum += (int)take;
          if (bendAccum >= grid)
          {
            bendAccum = 0;
            // The law carries a copy because glide_core owns its own Params;
            // the SOURCE is `scale`, so a provider that fills it reaches the
            // quantiser without glide_core learning anything new.
            bendLaw.scaleRoot = scale.root;
            for (int d = 0; d < 12; d++) bendLaw.scaleMask[d] = scale.mask[d];
            const double v = bendGlide.step(bendTarget, bendLaw, (double)lastNoteKey);
            if (v != pitchBend) { pitchBend = v; updateTuneAll(); }
            stepNoteBends();   // ADR-097: per-note bend rides the same clock
          }
        }
        continue;   // `frame` is already at `until`
      }
      if (spectraMode())
      {
        spectra.render(outL + frame, outR + frame, (int)(until - frame));
        // B172: source row 2 is engine-independent (see the note-on comment).
        renderSubSpan(frame, (uint32_t)(until - frame));
      }
      else
        renderSpan(outL, outR, frame, (uint32_t)(until - frame));
      frame = until;
    }

    // ADR-035 bass mono: runs BEFORE the spectrum feed so the visualizer
    // shows what actually leaves the plugin. B146 made the PLACEMENT a
    // parameter; `pre` (the default) is this call and nothing else, so the
    // shipped chain is the one it always was.
    // ONE STAGE PER SOURCE (B23 increment 3): `pre` means "before the matrix",
    // and after increment 3 there is a source per oscillator, so a single stage
    // over source 0 would leave every other source unfiltered. Each source
    // carries its own integrator pair for the reason the declaration states.
    if (bassMonoOn != 0 && bassMonoPos != 1)
    {
      bassMonoStage(outL, outR, nframes, bmIc1[0], bmIc2[0]);
      for (int s = 1; s < kRoutingNSrc; s++)
        bassMonoStage(srcBufL[s - 1], srcBufR[s - 1], nframes, bmIc1[s], bmIc2[s]);
    }

    // Internal FX rack (ADR-054), now driven THROUGH the B23 crosspoint matrix
    // (ADR-088) rather than as a hardcoded series. Post-oscillator,
    // post-bass-mono; runs before the spectrum feed so the visualizer reflects
    // post-FX output.
    //
    // BASS-MONO DEFAULTS UPSTREAM AND IS NOW MOVABLE (B146, ratified
    // 2026-09-18). The measurement that once argued against a forced reorder
    // still stands and is why `pre` is the DEFAULT: Comb at amount 0.9 scales
    // the sub-crossover channel difference by 2.2x whether bass-mono is on or
    // off — same ~11% residual either way — because it is a stereo-SYMMETRIC
    // filter, and a linear symmetric slot commutes with the side high-pass.
    // What that measurement did not cover is a NONLINEAR slot: Drive runs
    // per channel, so f(L)-f(R) manufactures side content out of the mid below
    // the crossover, which no upstream stage ever saw. So the reorder stayed
    // un-forced and became a choice — the player's, not taste exercised on
    // their behalf. routing_check's bass-mono probe is the oracle for both
    // halves (they commute with the rack bypassed; they do not with Drive).
    //
    // The default topology is setSerialChain(), which reproduces the old
    // `rack.processStereo` chain BIT-EXACTLY: every live edge carries a
    // coefficient of exactly 1.0, so each gather is `0.0f + 1.0*x` and the
    // terminal sum is `0.0f + 1.0*slot3` — both exact in float. That inertness
    // is what keeps the 147 goldens as this change's regression proof, and
    // routing_check asserts it against the real rack rather than trusting it.
    //
    // Fixed stack scratch + chunk loop, matching the oscillator sum above and
    // for the same reason: a heap buffer sized at activate() once made audible
    // output conditional on activate() having run.
    {
      /* ADR-142: the host's tempo, pushed once per block. The Delay's sync
         reads it as DATA — no core reads a clock (SPEC §5.7), and a host that
         never sends transport leaves the rack at its 120 default rather than
         at zero. */
      rack.setTempo(core.p.bpm);
      float sL[hypersaw::kRackSlots][kMixChunk], sR[hypersaw::kRackSlots][kMixChunk];
      float *slotL[hypersaw::kRackSlots], *slotR[hypersaw::kRackSlots];
      for (int t = 0; t < hypersaw::kRackSlots; t++) { slotL[t] = sL[t]; slotR[t] = sR[t]; }
      for (uint32_t off = 0; off < nframes; off += (uint32_t)kMixChunk)
      {
        const uint32_t left = nframes - off;
        const int m = (int)(left < (uint32_t)kMixChunk ? left : (uint32_t)kMixChunk);
        /* Source 0 is the output buffer (oscillator 0 renders into it);
           sources 1.. are their own. The ORDER is the matrix's own source
           order, which is what makes slot 0's default gather
           `0 + 1.0*osc0 + 1.0*osc1` — the same terms in the same order the
           pre-increment-3 renderSpan summed them in, and therefore the same
           float result rather than merely the same value. */
        const float *srcL[kRoutingNSrc];
        const float *srcR[kRoutingNSrc];
        srcL[0] = outL + off;
        srcR[0] = outR + off;
        for (int s = 1; s < kRoutingNSrc; s++)
        {
          srcL[s] = srcBufL[s - 1] + off;
          srcR[s] = srcBufR[s - 1] + off;
        }
        routing.processBlock(srcL, srcR, slotL, slotR, outL + off, outR + off, m,
                             [&](int slot, float *L, float *R, int n) {
                               rack.processSlot(slot, L, R, n);
                             });
      }
    }

    /* B146: the POST placement, immediately after the rack and before the
       master volume. Master volume is a scalar gain and commutes with a linear
       filter, so "after the rack" and "after the gain" are the same audio —
       this side of it keeps the stage inside the FX chain, where the ruling
       put it, rather than downstream of the instrument's output trim.
       Its own state pair: see bassMonoStage. */
    if (bassMonoOn != 0 && bassMonoPos != 0) bassMonoStage(outL, outR, nframes, bmIc1Post, bmIc2Post);

    // MASTER VOLUME (B24): last in the chain, before the visualizer feed so
    // the meters show what leaves the plugin. One-pole smoothed (~8 ms) with a
    // snap once within 1e-6 of target — the snap is load-bearing: it makes
    // unity EXACTLY 1.0, and the skip below keeps every pre-mixer patch
    // byte-identical rather than "identical up to a converging one-pole".
    {
      const double c = 1.0 - std::exp(-1.0 / (0.008 * sampleRate));
      for (uint32_t i = 0; i < nframes; i++)
      {
        masterVolSm += (masterVol - masterVolSm) * c;
        if (std::fabs(masterVolSm - masterVol) < 1e-6) masterVolSm = masterVol;
        if (masterVolSm != 1.0)
        {
          outL[i] = (float)(outL[i] * masterVolSm);
          outR[i] = (float)(outR[i] * masterVolSm);
        }
      }
    }

    publishViz();
    {
      uint32_t w = specPos.load(std::memory_order_relaxed);
      for (uint32_t i = 0; i < nframes; i++)
        specRing[(w + i) & 4095] = outL[i] + outR[i];
      specPos.store(w + nframes, std::memory_order_release);
      for (uint32_t i = 0; i < nframes; i++)
      {
        const double a = std::fabs((double)outL[i]) + std::fabs((double)outR[i]);
        if (a > outPeakViz) outPeakViz = a;
      }
      /* ADR-100 A4: the scope follows the VIZ oscillator, tapped per-osc in
         applyOscGainAndMeter — the master fill here is retired. It sat beside
         the per-osc viz panels and read as per-osc while showing the whole
         bus: "the osc 2 waveform viewer seems to be hooked up to osc 1". */
    }
    emitNoteEnds(p->out_events, nframes > 0 ? nframes - 1 : 0);

    if (spectraMode() ? (spectra.focus() != nullptr) : (core.focus() != nullptr))
      return CLAP_PROCESS_CONTINUE;
    return CLAP_PROCESS_SLEEP;
  }
};

Plugin *self(const clap_plugin_t *p) { return static_cast<Plugin *>(p->plugin_data); }

/* ---- lifecycle ---- */

bool plug_init(const clap_plugin_t *p)
{
  auto *pl = self(p);
  if (pl->host)
    pl->hostParams = static_cast<const clap_host_params_t *>(
        pl->host->get_extension(pl->host, CLAP_EXT_PARAMS));
  return true;
}

void plug_destroy(const clap_plugin_t *p)
{
#if defined(__APPLE__) || defined(_WIN32)
  delete self(p)->gui;
  self(p)->gui = nullptr;
#endif
  delete self(p);
}

bool plug_activate(const clap_plugin_t *p, double sr, uint32_t, uint32_t maxFrames)
{
  auto *pl = self(p);
  /* B23 increment 3: the per-source block buffers are fixed-size, so a host
     that declares a block past them is refused HERE, where a failed activation
     is the documented outcome, rather than discovered mid-block. The ceiling is
     ~0.74 s at 44.1 kHz; the argument was unused until this increment gave the
     shell something that depends on it. */
  if (maxFrames > Plugin::kSrcBufFrames) return false;
  pl->sampleRate = sr;
  // Recreate the core at the host rate, preserving params (constructor cost
  // is trivial; activate is main-thread and never concurrent with process).
  for (uint32_t k = 0; k < kNumOsc; k++)
  {
    /* B149: the ensemble-timing state is carried across the replacement for the
       same reason `p` is — activate() DESTROYS the core, and the host's order
       is setState() then activate(), so a restored timing history that is not
       carried here never survives to the first render. Measured: without this
       line the chunk round-trip passes its own assertion and changes nothing
       (tseed_check D3 read 0.000e+00 — a false green, the
       detector-shares-the-assumption trap). Read before, written after
       setParam("seed") — that call's rebuild() is exactly what re-rolls the
       stream. Rate-independent by construction: tOff is in SECONDS. */
    const auto ens = pl->cores[k].ensembleTiming();
    hypersaw::Params saved = pl->cores[k].p;
    pl->cores[k] = hypersaw::SwarmCore(sr);
    pl->cores[k].p = saved;
    pl->cores[k].setParam("seed", saved.seed);  // re-trigger rebuild() with saved state
    pl->cores[k].setEnsembleTiming(ens);
  }
  hypersaw::SpectraCore::SParams sp = pl->spectra.p;
  pl->spectra = hypersaw::SpectraCore(sr);
  pl->spectra.p = sp;
  pl->spectra.rebuild();
  pl->rack.setSampleRate(sr);  // ADR-071: size comb lines + derive comp coeffs at sr
  /* B172: the sub's sixteen voices follow the host rate. setSampleRate RECALCS
     (subosc_core.h) rather than replacing the object, so the parameters survive
     without the save/restore dance the swarm cores need — and every voice is
     silenced, because a rate change while a note is held would otherwise leave
     sixteen phases running at the old increment. */
  for (auto &c : pl->subs) c.setSampleRate(sr);
  pl->subAllOff();
  // The shell owns the note law (it resolves the link), so push it once here.
  // Without this the cores run GlideCore's OWN defaults until the first edit of
  // a note/bend/scale param -- including an empty scale mask, which the
  // quantiser would read as "no degree admitted".
  pl->pushNoteLaw();
  // ADR-104: morph tables are built HERE, on the main thread — morphInit
  // allocates, and applyParam(151) can arrive on the audio thread.
  pl->morphInit();
  /* B171: the LFOs take their start phase and their seeds here — a free-running
     LFO starts at its Start Phase knob once, at activate, and never rewinds.
     B149's trap applies verbatim: the host's order is setState() then
     activate(), so a phase/stream RESTORED from the chunk must survive this
     call. `restored` is that one-shot flag; without it the restore would pass
     its own round-trip assertion and change nothing. */
  if (pl->lfoRestoredPending())
    pl->lfoClearRestored();
  else
    pl->lfoReseed();
  return true;
}

void plug_deactivate(const clap_plugin_t *) {}
bool plug_start_processing(const clap_plugin_t *p)
{
  self(p)->processing.store(true, std::memory_order_release);
  return true;
}
void plug_stop_processing(const clap_plugin_t *p)
{
  self(p)->processing.store(false, std::memory_order_release);
}
void plug_reset(const clap_plugin_t *p)
{
  // The host-MPE counters describe the CURRENT note stream, so a reset clears
  // them: after a transport reset the evidence for "no expressions have arrived"
  // has to be re-earned, or the hint would report a stream that is over.
  self(p)->sawNotes.store(0, std::memory_order_relaxed);
  self(p)->sawExprs.store(0, std::memory_order_relaxed);
  self(p)->sawNonZeroChan.store(0, std::memory_order_relaxed);
  auto *pl = self(p);
  pl->allOffAll();
  for (double &b : pl->mpeBendSemis) b = 0.0;
}

clap_process_status plug_process(const clap_plugin_t *p, const clap_process_t *proc)
{
  return self(p)->process(proc);
}

/* ---- audio/note ports (unchanged from Phase 0) ---- */

uint32_t aports_count(const clap_plugin_t *, bool is_input) { return is_input ? 0 : 1; }

bool aports_get(const clap_plugin_t *, uint32_t index, bool is_input, clap_audio_port_info_t *info)
{
  if (is_input || index != 0) return false;
  info->id = 0;
  std::snprintf(info->name, sizeof(info->name), "%s", "Main Out");
  info->flags = CLAP_AUDIO_PORT_IS_MAIN;
  info->channel_count = 2;
  info->port_type = CLAP_PORT_STEREO;
  info->in_place_pair = CLAP_INVALID_ID;
  return true;
}

const clap_plugin_audio_ports_t s_audio_ports = {aports_count, aports_get};

uint32_t nports_count(const clap_plugin_t *, bool is_input) { return is_input ? 1 : 0; }

bool nports_get(const clap_plugin_t *, uint32_t index, bool is_input, clap_note_port_info_t *info)
{
  if (!is_input || index != 0) return false;
  info->id = 0;
  std::snprintf(info->name, sizeof(info->name), "%s", "Note In");
  info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
  info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
  return true;
}

const clap_plugin_note_ports_t s_note_ports = {nports_count, nports_get};

/* ---- params extension ---- */

// Osc 0 occupies indices [0, kNumParams) EXACTLY as before, so at kNumOsc == 1
// the enumeration a host sees is unchanged, index for index and id for id.
// Higher oscillators append their per-osc params after it.
uint32_t params_count(const clap_plugin_t *)
{
  // The ADR-088 routing block enumerates AFTER the oscillator blocks, so every
  // index a host already knows keeps its parameter. It has to be enumerated
  // explicitly: this loop walks kParams, and the routing table is deliberately
  // NOT in kParams (its ids are positional, not curated, and the presentation
  // registry is address-keyed over the instrument's params).
  /* B172: the engine blocks enumerate LAST, after the routing block, for the
     same reason routing enumerates after the oscillators — every index a host
     already knows keeps its parameter. */
  return kNumParams + (kNumOsc - 1) * perOscParamCount() + routingParamCount() +
         engineParamCount();
}

bool params_get_info(const clap_plugin_t *, uint32_t index, clap_param_info_t *info)
{
  uint32_t osc = 0;
  const ParamDef *dp = nullptr;
  const uint32_t oscEnd = kNumParams + (kNumOsc - 1) * perOscParamCount();
  const char *engineModule = nullptr;
  if (index >= oscEnd + routingParamCount())
  {
    // B172 engine blocks. osc stays 0, so `d.id + osc * kOscStride` below is
    // the engine id itself — the routing branch's own trick.
    dp = engineParamAt(index - oscEnd - routingParamCount(), &engineModule);
    if (!dp) return false;
  }
  else if (index >= oscEnd)
  {
    const uint32_t r = index - oscEnd;
    dp = &g_routingTable.defs[r];
    // osc stays 0, so `d.id + osc * kOscStride` below is the routing id itself.
  }
  else if (index < kNumParams) { dp = &kParams[index]; }
  else
  {
    uint32_t rest = index - kNumParams;
    const uint32_t per = perOscParamCount();
    if (per == 0) return false;
    osc = 1 + rest / per;
    if (osc >= kNumOsc) return false;
    uint32_t want = rest % per;
    for (const auto &d : kParams)
      if (!isGlobalId(d.id) && want-- == 0) { dp = &d; break; }
    if (!dp) return false;
  }
  const ParamDef &d = *dp;
  info->id = (clap_id)(d.id + osc * kOscStride);
  info->flags = CLAP_PARAM_IS_AUTOMATABLE;
  if (d.stepped) info->flags |= CLAP_PARAM_IS_STEPPED;
  info->cookie = nullptr;
  const bool isRouting = (uint32_t)d.id >= kRoutingIdBase;
  if (osc == 0)
    std::snprintf(info->name, sizeof(info->name), "%s", d.name);
  else
    std::snprintf(info->name, sizeof(info->name), "Osc%u %s", osc + 1, d.name);
  std::snprintf(info->module, sizeof(info->module), "%s",
                engineModule ? engineModule
                             : isRouting ? "Routing"
                                         : osc == 0 ? "" : (osc == 1 ? "Osc 2" : "Osc 3"));
  info->min_value = d.minV;
  info->max_value = d.maxV;
  // Oscillators above the first default to SILENT (vol = 0). Without this the
  // second oscillator would sound the instant kNumOsc rose, changing every
  // existing patch — a host "reset to defaults" must give silence too, not
  // just our constructor.
  info->default_value = defaultFor(d, osc);
  return true;
}

bool params_get_value(const clap_plugin_t *p, clap_id id, double *out)
{
  if (!findParam(id)) return false;
  *out = self(p)->readParam(id);
  return true;
}

bool params_value_to_text(const clap_plugin_t *, clap_id id, double value, char *out,
                          uint32_t cap)
{
  const ParamDef *d = findParam(id);
  if (!d) return false;
  if (d->labels)
  {
    const int idx = (int)std::round(value) - (int)d->minV;
    const int span = (int)(d->maxV - d->minV);
    if (idx >= 0 && idx <= span) std::snprintf(out, cap, "%s", d->labels[idx]);
    else std::snprintf(out, cap, "%d", (int)std::round(value));
  }
  else if (d->stepped)
  {
    std::snprintf(out, cap, "%d", (int)std::round(value));
  }
  else if (id == 8)  // dissolve: seconds
  {
    std::snprintf(out, cap, "%.2f s", value);
  }
  else if (id == 19 || id == 20 || id == 22)  // envelope times
  {
    if (value < 0.01) std::snprintf(out, cap, "%.1f ms", value * 1000);
    else std::snprintf(out, cap, "%.2f s", value);
  }
  else if (id == 33)  // glide seconds
  {
    if (value < 0.001) std::snprintf(out, cap, "off");
    else if (value < 0.01) std::snprintf(out, cap, "%.1f ms", value * 1000);
    else std::snprintf(out, cap, "%.2f s", value);
  }
  else if (baseIdOf(id) == 35)  // octave (any oscillator block)
  {
    std::snprintf(out, cap, "%+d oct", (int)std::round(value));
  }
  else if (baseIdOf(id) == 36)
  {
    std::snprintf(out, cap, "%+d st", (int)std::round(value));
  }
  else if (id == 27)
  {
    std::snprintf(out, cap, "%+.0f deg", value);
  }
  else if (baseIdOf(id) == 37)
  {
    std::snprintf(out, cap, "%+.1f c", value);
  }
  else if (id == 38)
  {
    std::snprintf(out, cap, "%+.2f st", value);
  }
  else if (id == 23)  // grid cycles/beat: named rational division
  {
    const char *name = gridStepName(snapGridStep(value));
    std::snprintf(out, cap, "%s/beat", name ? name : "?");
  }
  else if (id == 9)  // drift depth: cents
  {
    std::snprintf(out, cap, "%.1f c", value);
  }
  else if (id == 10)  // drift rate knob 0..1 -> walk speed 0.2..8.2 per second
  {
    std::snprintf(out, cap, "%.1f /s", 0.2 + value * 8);
  }
  else
  {
    std::snprintf(out, cap, "%.3f", value);
  }
  return true;
}

bool params_text_to_value(const clap_plugin_t *, clap_id id, const char *text, double *out)
{
  const ParamDef *d = findParam(id);
  if (!d) return false;
  if (d->labels)
  {
    const int span = (int)(d->maxV - d->minV);
    for (int i = 0; i <= span; i++)
      if (!std::strcmp(text, d->labels[i]))
      {
        *out = i;
        return true;
      }
  }
  *out = std::atof(text);
  return true;
}

void params_flush(const clap_plugin_t *p, const clap_input_events_t *in,
                  const clap_output_events_t *out)
{
  self(p)->drainQueue(out);
  const uint32_t nev = in->size(in);
  for (uint32_t i = 0; i < nev; i++) self(p)->handleEvent(in->get(in, i));
}

const clap_plugin_params_t s_params = {params_count, params_get_info, params_get_value,
                                       params_value_to_text, params_text_to_value, params_flush};

/* ---- state extension: versioned key=value text ---- */

/* ---- OSCILLATOR PRESETS (B20) -------------------------------------------
   The format and filtering live in src/osc_preset.h and are gated by
   tools/preset_check.cpp. The plugin-side wiring (bind read/write to
   readParam/applyParam with the +kOscStride offset) is NOT here yet, on
   purpose: it would have no caller until the osc-page GUI exists, and
   unreachable code rots quietly — it compiles forever while the surface it
   assumed drifts underneath it. It lands with the GUI that calls it, in the
   same change, so it is exercised the day it ships. */

bool state_save(const clap_plugin_t *p, const clap_ostream_t *stream)
{
  // ADR-082: oscillator 0's keys are UNCHANGED, so every existing patch keeps
  // loading bit-identically and state_check stays the regression proof. Higher
  // oscillators prefix `o<k>.`. At kNumOsc == 1 this emits exactly the old
  // bytes, header included — which is the point of increment 1.
  std::string blob = kNumOsc > 1 ? "hypersaw-state 2\n" : "hypersaw-state 1\n";
  char line[80];
  // B100 header: the version line above IS the chunk's schema; these two lines
  // complete {schema, engine_revision, build}. Plain key=value so every
  // pre-B100 build reads them as unknown keys and ignores them — the chunk
  // version does not move, and a session round-trips through an old build.
  std::snprintf(line, sizeof(line), "engine_revision=%d\nbuild=%s\n",
                self(p)->engineRevision(), HYPERSAW_BUILD_ID);
  blob += line;
  for (const auto &d : kParams)
  {
    std::snprintf(line, sizeof(line), "%s=%.17g\n", d.coreKey, self(p)->readParam(d.id));
    blob += line;
  }
  for (uint32_t k = 1; k < kNumOsc; k++)
    for (const auto &d : kParams)
    {
      if (isGlobalId(d.id)) continue;
      std::snprintf(line, sizeof(line), "o%u.%s=%.17g\n", k, d.coreKey,
                    self(p)->readParam((clap_id)(d.id + k * kOscStride)));
      blob += line;
    }
  // B172 engine blocks, prefixed and unconditional — see stateJson's comment
  // for why these are not emitted-only-when-dirty like `routing=` below.
  for (const auto &b : kEngineBlocks)
    for (uint32_t i = 0; i < b.count; i++)
    {
      std::snprintf(line, sizeof(line), "%s%s=%.17g\n", b.keyPrefix, b.defs[i].coreKey,
                    self(p)->readParam(b.defs[i].id));
      blob += line;
    }
  // ADR-112 A3: the morph field rides the session, not just the preset. The
  // fragment is opaque JSON on one line; the parser find()s its keys, so the
  // leading comma the fragment carries is harmless.
  blob += "morph=" + self(p)->morphJson() + "\n";
  // B174: which preset this session is sitting on — the corner names have
  // ridden the `morph=` line since B122 and the GLOBAL name had no such home,
  // so reopening a project showed the right patch under the wrong name (or
  // none). Emitted ONLY when non-empty, exactly as `routing=` below, so a
  // session saved before names is byte-identical. setPresetName strips control
  // characters, which is what keeps this one line one line.
  if (!self(p)->presetName.empty()) blob += "presetname=" + self(p)->presetName + "\n";
  // ADR-138: generic mod routes ride the session. Emitted ONLY when routes
  // exist, so a routeless patch's bytes are unchanged and every existing
  // state round-trip stays exactly what it was. Old builds ignore the key.
  {
    const std::string routes = self(p)->modRoutesChunk();
    if (!routes.empty()) blob += "modroutes=" + routes + "\n";
  }
  // ADR-088 (B50): the crosspoint topology, emitted ONLY when some cell has
  // left its default — a patch on the series chain writes no key at all and its
  // bytes are unchanged, which is what keeps state_check and the fixtures the
  // regression proof for this change rather than a casualty of it.
  {
    const std::string rt = self(p)->routingChunk();
    if (!rt.empty()) blob += "routing=" + rt + "\n";
  }
  // B89 phase 2b (ADR-176): the intent bus's corner-owned bindings, ranges and
  // homes plus the patch's intent names. Emitted ONLY when something has left
  // its default, exactly as `routing=` above — which is what keeps every
  // existing chunk, fixture and factory file byte-for-byte what it was.
  {
    const std::string it = self(p)->intentChunk();
    if (!it.empty()) blob += "intent=" + it + "\n";
  }
  // B149: the ADR-077/078 ensemble-timing state, per oscillator, and LAST in
  // the blob on purpose — state_load's idle path applies keys in file order, so
  // arriving after `seed` means the rebuild that re-rolls the stream has
  // already run. (The processing path reverses that order; the key carries its
  // own seed so both orders restore the same state.) Emitted only for a patch
  // that has actually drawn from the stream, so a chunk that had no key before
  // this change still has none.
  for (uint32_t k = 0; k < kNumOsc; k++)
  {
    const std::string ens = self(p)->ensembleChunk(k);
    if (ens.empty()) continue;
    blob += (k == 0 ? std::string("ens=") : "o" + std::to_string(k) + ".ens=") + ens + "\n";
  }
  /* B171: the LFO streams, AFTER `seed` for the same reason `ens=` is — the
     seed's applyParam re-rolls them (lfoReseed), so a restored phase written
     before it would be thrown away by the very key it followed. Emitted only
     for a patch whose S&H has drawn, so a chunk that had no key before this
     change still has none. */
  {
    const std::string lf = self(p)->lfoChunk();
    if (!lf.empty()) blob += "lfo=" + lf + "\n";
  }
  int64_t written = 0;
  while (written < (int64_t)blob.size())
  {
    const int64_t n =
        stream->write(stream, blob.data() + written, (uint64_t)(blob.size() - written));
    if (n <= 0) return false;
    written += n;
  }
  return true;
}

bool state_load(const clap_plugin_t *p, const clap_istream_t *stream)
{
  std::string blob;
  char buf[512];
  int64_t n;
  while ((n = stream->read(stream, buf, sizeof(buf))) > 0) blob.append(buf, (size_t)n);
  if (n < 0) return false;
  // Version 2 adds `o<k>.` keys; version 1 is still accepted and simply leaves
  // the higher oscillators at their defaults (i.e. silent) — forward and
  // backward compatible, which append-only ids buy us for free.
  const bool v1 = blob.rfind("hypersaw-state 1\n", 0) == 0;
  const bool v2 = blob.rfind("hypersaw-state 2\n", 0) == 0;
  if (!v1 && !v2) return false;
  size_t pos = blob.find('\n') + 1;
  auto *pl = self(p);
  /* B192 / B183 — RESET TO INIT, THEN APPLY. This REPLACES five hand-listed
     resets (modroutes, routing, intent, presetname, engine_revision), each of
     which stated "an absent key means the default" for its own chunk and none
     of which covered the PARAMETERS: the loop below only ever visits the keys
     the file names, so a parameter this chunk does not mention kept whatever
     the instance held. Harmless on a fresh restore and wrong in exactly B181
     note 6's way when a host re-uses an instance, which is the common case in
     a long session. `true` = this transport carries the routing matrix, so
     resetting it here is restorable; see initState. */
  pl->initState(/*chunkOnlyState=*/true);
  while (pos < blob.size())
  {
    const size_t eol = blob.find('\n', pos);
    const std::string line = blob.substr(pos, eol == std::string::npos ? std::string::npos
                                                                       : eol - pos);
    pos = eol == std::string::npos ? blob.size() : eol + 1;
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string key = line.substr(0, eq);
    if (key == "engine_revision")   // B100: the patch's pinned revision
    {
      pl->setEngineRevision(std::atol(line.c_str() + eq + 1));
      continue;
    }
    if (key == "build") continue;   // B100: provenance only, never read back
    if (key == "morph")   // ADR-112 A3: the field's chunk, shared parser
    {
      pl->applyMorphChunk(line.substr(eq + 1));
      continue;
    }
    if (key == "modroutes")   // ADR-138: generic routes, canonical (src,dest,depth)
    {
      pl->applyModRoutesChunk(line.substr(eq + 1));
      continue;
    }
    if (key == "routing")     // ADR-088: crosspoint cells that left their default
    {
      pl->applyRoutingChunk(line.substr(eq + 1));
      continue;
    }
    if (key == "intent")      // ADR-176: intent bindings/ranges/homes/names
    {
      pl->applyIntentChunk(line.substr(eq + 1));
      continue;
    }
    if (key == "presetname")  // B174: the global preset's name
    {
      pl->setPresetName(line.substr(eq + 1));
      continue;
    }
    /* ADR-147: the specimen's visibility is a GUI preference, NOT patch state,
       so a chunk cannot switch it off. The scar: ADR-140's few-hours-long
       off-by-default era wrote specimen=0 into the human's working set, and
       every reload re-hid the blob no matter what was installed — three
       debugging rounds ended at "please flip a checkbox", which is the
       software outsourcing its own defect. Whether a visualizer shows belongs
       to the machine and the moment, like the selected tab — it is still
       writable live (turn it off for a session if it bothers you) and still
       WRITTEN to state for forward compatibility; it is simply never read
       back. */
    if (key == "specimen") continue;
    const double val = std::atof(line.c_str() + eq + 1);
    // ADR-082: split an `o<k>.` prefix off the key and resolve it to that
    // oscillator's block. A prefix naming an oscillator this build does not
    // have falls through to the existing unknown-key path (ignored), which is
    // how a 2-osc patch stays loadable by a 1-osc build.
    uint32_t keyOsc = 0;
    if (key.size() > 2 && key[0] == 'o' && key.find('.') != std::string::npos)
    {
      const size_t dot = key.find('.');
      bool digits = dot > 1;
      for (size_t i = 1; i < dot && digits; i++) digits = key[i] >= '0' && key[i] <= '9';
      if (digits)
      {
        keyOsc = (uint32_t)std::atoi(key.c_str() + 1);
        key = key.substr(dot + 1);
      }
    }
    if (keyOsc >= kNumOsc && keyOsc != 0) continue;   // block this build lacks
    // B149: the one non-parameter key that takes the `o<k>.` prefix, so it is
    // read here rather than beside morph/routing above — the prefix split is
    // this loop's, and a second copy of it is a second thing to keep in step.
    if (key == "ens") { pl->applyEnsembleChunk(keyOsc, line.substr(eq + 1)); continue; }
    // B171: the LFO streams. Not prefixed (the LFOs are global), but read here
    // beside `ens` because both are non-parameter keys emitted after `seed`.
    if (key == "lfo") { pl->applyLfoChunk(line.substr(eq + 1)); continue; }
    /* B172 engine blocks. AFTER the `o<k>.` split (an engine key never carries
       one, and asking first would mean two prefix vocabularies) and BEFORE the
       kParams scans below, which is the ordering that matters: `sub.wave` must
       never fall through to a search that could match `wave`. Routed through
       the same two lanes — queue while processing, applyParam while idle. */
    if (keyOsc == 0)
      if (const ParamDef *ed = findEngineParamByKey(key))
      {
        if (pl->processing.load(std::memory_order_acquire)) pl->enqueueParam(ed->id, val, 3);
        else pl->applyParam(ed->id, val);
        continue;
      }
    const clap_id idOff = (clap_id)(keyOsc * kOscStride);
    // Thread safety (2026-07-18): state_load is main-thread and MAY run while
    // the audio thread is in process() — a direct setParam would race
    // rebuild() against render(). Idle: apply directly (hosts read values
    // back immediately after setState). Processing: route through the param
    // queue; the audio thread applies next block and drainQueue's outgoing
    // param events tell the host the new values.
    if (pl->processing.load(std::memory_order_acquire))
    {
      for (const auto &d : kParams)
        if (key == d.coreKey)
        {
          if (keyOsc && isGlobalId(d.id)) break;   // globals have no per-osc mirror
          pl->enqueueParam((clap_id)(d.id + idOff), val, 3);   // B125
          break;
        }
    }
    else
    {
      // Route through applyParam (not core.setParam) so layer mappings like
      // the ADR-024 inertia taper apply identically on both load paths.
      bool known = false;
      for (const auto &d : kParams)
        if (key == d.coreKey)
        {
          if (keyOsc && isGlobalId(d.id)) break;   // globals have no per-osc mirror
          pl->loadingState = true;                  // B125: a load is not an edit
          pl->applyParam((clap_id)(d.id + idOff), val);
          pl->loadingState = false;
          known = true;
          break;
        }
      if (!known) continue;  // unknown/future keys ignored (state_check pins this)
    }
  }
  pl->undoMark("host load");   // B84: main thread, per the CLAP state contract
  return true;
}

const clap_plugin_state_t s_state = {state_save, state_load};

/* ---- gui extension (macOS/cocoa + Windows/win32 via the seam; the win32
       backend is CI-compile-verified, runtime validation is a recorded
       residual — see the Phase 2 trace) ---- */
#if defined(__APPLE__) || defined(_WIN32)

#ifdef __APPLE__
#define HYPERSAW_WINDOW_API CLAP_WINDOW_API_COCOA
#else
#define HYPERSAW_WINDOW_API CLAP_WINDOW_API_WIN32
#endif

bool gui_is_api_supported(const clap_plugin_t *, const char *api, bool is_floating)
{
  return !is_floating && !std::strcmp(api, HYPERSAW_WINDOW_API);
}

bool gui_get_preferred_api(const clap_plugin_t *, const char **api, bool *is_floating)
{
  *api = HYPERSAW_WINDOW_API;
  *is_floating = false;
  return true;
}

/* Headless probe surface (2026-08-21). The JSON state path — the preset
   system's path — was reachable ONLY through webview lambdas local to
   gui_create, so it had no oracle and "loading doesn't seem to do anything"
   shipped unobserved. These are the same two calls the GUI buttons make,
   exported so a probe can make them without a webview. Not part of the CLAP
   surface; not for hosts. */
/* B89 phase 1 — the classification, readable from a tool. NO plugin handle:
   the class is definition, not instance state, so an export that took one
   would imply it could differ between instances. Returns the ParamClass as an
   int, or -1 if `id` is not a parameter. `keyOut`/`reasonOut` may be null. */
extern "C" int hypersaw_debug_paramclass(uint32_t id, const char **keyOut,
                                         const char **reasonOut)
{
  ParamClass cls;
  const char *reason = nullptr;
  if (!paramClassOf((clap_id)id, cls, reason)) return -1;
  if (keyOut) *keyOut = findParam((clap_id)id)->coreKey;
  if (reasonOut) *reasonOut = reason;
  return (int)cls;
}

extern "C" void hypersaw_debug_state(const clap_plugin_t *p, char *out, uint32_t cap)
{
  const std::string j = self(p)->stateJson();
  std::snprintf(out, cap, "%s", j.c_str());
}
extern "C" void hypersaw_debug_subwave(const clap_plugin_t *p, char *out, uint32_t cap)
{
  const std::string j = self(p)->subWaveJson();
  std::snprintf(out, cap, "%s", j.c_str());
}
extern "C" void hypersaw_debug_lfocycle(const clap_plugin_t *p, char *out, uint32_t cap)
{
  const std::string j = self(p)->lfoCycleJson();
  std::snprintf(out, cap, "%s", j.c_str());
}
extern "C" bool hypersaw_debug_exempt(const clap_plugin_t *p, uint32_t id) { return self(p)->morphToggleExempt((clap_id)id); }
/* The GUI bridge's other two corner verbs, headless — the same reason the
   exempt door above exists. B89 2c (e) has to prove capture still bakes and an
   armed edit still lands in the armed corner WITH THE RESOLVER RUNNING, and
   both gestures reach the shell only through hostIf, which no oracle can
   drive. `arm` is the parameter (159), so it needs no door of its own. */
extern "C" void hypersaw_debug_capture(const clap_plugin_t *p, int k) { self(p)->morphCapture(k); }
extern "C" const char *hypersaw_debug_cornervals(const clap_plugin_t *p, int k)
{ static std::string j; j = self(p)->morphCornerValsJson(k); return j.c_str(); }
extern "C" const char *hypersaw_debug_ownersjson(const clap_plugin_t *p)
{ static std::string j; j = self(p)->morphOwnersJson(); return j.c_str(); }
extern "C" const char *hypersaw_debug_exemptjson(const clap_plugin_t *p)
{ static std::string j; j = self(p)->morphExemptJson(); return j.c_str(); }
/* B134: the GUI's own view of the route table, headless. polarity_check needs
   the SHELL's answer — that slot 17 is declared bipolar and slot 2 unipolar —
   and the bridge that normally carries it is a webview no oracle can drive. */
extern "C" const char *hypersaw_debug_modroutes(const clap_plugin_t *p)
{ static std::string j; j = self(p)->modRoutesJson(); return j.c_str(); }
/* B171 — the LIVE source slot, not a readParam round-trip. The distinction is
   the whole value of the export: a probe that asked readParam(269) would learn
   what the RATE KNOB says, which is a fact about the parameter store, while
   what an oracle needs to know is what the matrix is actually handing the
   routes this tick (L0032 — a detector that shares its subject's accessor
   agrees with itself). Out-of-range returns NaN rather than 0 because 0 is a
   legitimate reading for an unassigned slot. */
extern "C" double hypersaw_debug_modsrc(const clap_plugin_t *p, int slot)
{
  if (slot < 0 || slot >= hypersaw::ModCore::kMaxSources)
    return std::numeric_limits<double>::quiet_NaN();
  return self(p)->mod.src[slot];
}
/* ADR-088 (B50) — a window onto the LIVE MATRIX, not onto readParam.
   Deliberately not `readParam`: a round-trip through one accessor agrees with
   itself (the state_check trap, L0032), so the round-trip probe would certify
   nothing. These read the very doubles `processBlock` multiplies by. */
/* `from` IS A ROW, matching the id list these are read beside (ADR-088
   amendment) — the oracle parses `id,kind,from,to;` and hands `from` straight
   back here, so a second coordinate space at this boundary would make every
   cell-by-cell comparison silently address the wrong cell. A reserved-but-
   unfilled source row maps to index -1 and reads as an absent cell. */
extern "C" double hypersaw_debug_routing(const clap_plugin_t *p, int from, int to)
{
  auto *pl = self(p);
  const int mi = routingIndexOfRow(from);
  if (mi < 0 || to < 0 || to >= kRoutingNSlot) return 0.0;
  return pl->routing.coeff[mi][to];
}
extern "C" bool hypersaw_debug_routing_on(const clap_plugin_t *p, int from, int to)
{
  auto *pl = self(p);
  const int mi = routingIndexOfRow(from);
  if (mi < 0 || to < 0 || to >= kRoutingNSlot) return false;
  return pl->routing.connected(mi, to);
}
extern "C" double hypersaw_debug_routing_out(const clap_plugin_t *p, int to)
{
  if (to < 0 || to >= kRoutingNSlot) return 0.0;
  return self(p)->routing.outAmount[to];
}
extern "C" double hypersaw_debug_routing_init(const clap_plugin_t *p, int to)
{
  if (to < 0 || to >= kRoutingNSlot) return 0.0;
  return self(p)->routing.slotInit[to];
}
/* The id list the oracle (and any future consumer) enumerates instead of
   re-deriving the layout: `id,kind,from,to;` per cell. Re-deriving it would be
   a second copy of decodeRoutingId, which is the one thing the id-layout
   comment asks nobody to make. */
/* B50 phase 1c: the dry path, read off the live matrix like every other cell. */
extern "C" double hypersaw_debug_routing_srcout(const clap_plugin_t *p, int from)
{
  if (from < 0 || from >= kRoutingNSrc) return 0.0;
  return self(p)->routing.srcOut[from];
}
extern "C" const char *hypersaw_debug_routing_ids(void)
{
  static std::string s;
  if (s.empty())
  {
    char b[48];
    for (const auto &d : g_routingTable.defs)
    {
      int kind = 0, from = 0, to = 0;
      decodeRoutingId(d.id, kind, from, to);
      std::snprintf(b, sizeof(b), "%u,%d,%d,%d;", (unsigned)d.id, kind, from, to);
      s += b;
    }
  }
  return s.c_str();
}
/* B89 phase 2c — THE RESOLVER'S OUTPUT, and the tables behind it. `final` is
   the TARGET intentApply hands to the glide, so these exports are the oracle's
   view of the value the engine is being carried toward — not a second
   evaluation of it, which would certify the copy.
   Addressed by morphIds SLOT INDEX, not by parameter id, because that is the
   index IntentCore works in and because `hypersaw_debug_cornervals` already
   publishes the slot -> id order — a second id lookup here would be a second
   copy of a mapping that already has one owner. Out-of-range reads are inert
   (0 / -1), never undefined. */
extern "C" double hypersaw_debug_intent_final(const clap_plugin_t *p, int slot)
{
  auto *pl = self(p);
  if (slot < 0 || (size_t)slot >= pl->intentResolved.size()) return 0.0;
  return pl->intentResolved[(size_t)slot];
}
extern "C" int hypersaw_debug_intent_owner(const clap_plugin_t *p, int slot)
{
  auto *pl = self(p);
  if (slot < 0 || (size_t)slot >= pl->intentOwnerParam.size()) return -1;
  return pl->intentOwnerParam[(size_t)slot];
}
extern "C" double hypersaw_debug_intent_bind(const clap_plugin_t *p, int corner, int intent,
                                             int slot)
{
  auto *pl = self(p);
  const size_t n = pl->morphIds.size();
  if (corner < 0 || corner > 3 || intent < 0 || intent >= Plugin::kIntents) return 0.0;
  if (slot < 0 || (size_t)slot >= n || pl->intentBind.empty()) return 0.0;
  return pl->intentBind[((size_t)corner * Plugin::kIntents + intent) * n + (size_t)slot];
}
extern "C" bool hypersaw_debug_intent_range(const clap_plugin_t *p, int corner, int slot,
                                            double *lo, double *hi)
{
  auto *pl = self(p);
  const size_t n = pl->morphIds.size();
  if (corner < 0 || corner > 3 || slot < 0 || (size_t)slot >= n || pl->intentRangeLo.empty())
    return false;
  const size_t cp = (size_t)corner * n + (size_t)slot;
  if (lo) *lo = pl->intentRangeLo[cp];
  if (hi) *hi = pl->intentRangeHi[cp];
  return true;
}
/* The ten CAPTIONS, in stored-slot order, as a JSON array — the same shape
   cornerNamesJson uses, so the GUI that eventually reads them parses one
   pattern and not two. The slot KEYS (kIntentOrder) are not published here:
   they are the chunk's business, and a consumer that needed both would be
   free to read the chunk. */
extern "C" const char *hypersaw_debug_intent_names(const clap_plugin_t *p)
{
  static std::string j;
  auto *pl = self(p);
  j = "[";
  for (int i = 0; i < Plugin::kIntents; i++)
  {
    j += i ? ",\"" : "\"";
    j += Plugin::jsonEscape(pl->intentCaption(i));
    j += "\"";
  }
  return (j += "]").c_str();
}
/* The homes, for the same oracle. `home` is an atom of its own (plan R10), so
   it is not addressable through the slot exports above. */
extern "C" bool hypersaw_debug_intent_home(const clap_plugin_t *p, int corner, double *x,
                                           double *y)
{
  if (corner < 0 || corner > 3) return false;
  if (x) *x = self(p)->intentHomeX[corner];
  if (y) *y = self(p)->intentHomeY[corner];
  return true;
}
/* The three CALIBRATION DOORS — controls, not features. Each has exactly one
   caller (tools/intent_check.cpp) and no shell, GUI or host path reaches any
   of them; see the comment on each Plugin:: member for what it buys and why a
   door is the honest way to buy it.
     plant       — 2b's "the flag off is bit-identical" control (returns the
                   number of slots the apply wrote, its own anchor).
     commit      — SPEC-INTENT-BUS §7, for T6. `forceCorner >= 0` bakes into a
                   corner that does not own the displacement: T6's must-fail
                   control. Returns the corner committed into, or -1.
     break_atom  — T5's must-fail control: break one member out of its lead
                   group and the forbidden chimera must appear. */
extern "C" int hypersaw_debug_intent_plant(const clap_plugin_t *p)
{
  return self(p)->intentPlantOnce();
}
extern "C" int hypersaw_debug_intent_commit(const clap_plugin_t *p, int forceCorner)
{
  return self(p)->intentCommit(forceCorner);
}
extern "C" bool hypersaw_debug_intent_break_atom(const clap_plugin_t *p, int slot)
{
  return self(p)->intentBreakAtom(slot);
}
/* NOT a calibration door: the EDITOR'S OWN gesture verb, made reachable
   headlessly — the same thing hypersaw_debug_capture is, and for the same
   reason. It calls Plugin::guiGesture, which is literally what hostIf.gesture
   calls, so an oracle that brackets a drag through this door exercises the
   shipped path (queue -> drainQueue -> the pad's latch) rather than a
   test-only shortcut. The pad's spring cannot be tested any other way: a
   released pointer leaves the parameter where it was, so the bracket is the
   only observable that says "still held". */
extern "C" void hypersaw_debug_gesture(const clap_plugin_t *p, uint32_t id, bool begin)
{
  self(p)->guiGesture((clap_id)id, begin);
}
/* The performance pad's puck, for the same oracle. The puck is device state
   with no parameter of its own by construction (it is the spring's output,
   not the player's input), so there is no get_value that can see it. */
extern "C" void hypersaw_debug_intent_puck(const clap_plugin_t *p, double *x, double *y)
{
  if (x) *x = self(p)->intentPuck.x;
  if (y) *y = self(p)->intentPuck.y;
}
/* Which corner owns `home` right now — the atom the pad's displacement is
   measured from (§4.4). Read off the last resolved walk, so it is the answer
   the audio thread used, not a second draw that could disagree with it. */
extern "C" int hypersaw_debug_intent_homeowner(const clap_plugin_t *p)
{
  auto *pl = self(p);
  if (pl->intentOwnerAtom.empty()) return -1;
  return pl->intentOwnerAtom[(size_t)pl->intentHomeAtom];
}
extern "C" bool hypersaw_debug_apply(const clap_plugin_t *p, const char *json)
{
  return self(p)->applyStateJson(json ? json : "");
}
/* B174: the GUI's LOAD button, whose one call both applies the patch and says
   which preset it is — the window-free path (see applyStateJson's naming
   block). A separate export rather than a third argument on the one above
   because that one's C ABI is held by a dozen tools. */
extern "C" bool hypersaw_debug_apply_named(const clap_plugin_t *p, const char *json,
                                           const char *name)
{
  return self(p)->applyStateJson(json ? json : "", name ? name : "");
}
/* B174: true iff applying `json` would leave the patch exactly as it is — the
   GUI's asterisk, headless. */
extern "C" bool hypersaw_debug_presetmatches(const clap_plugin_t *p, const char *json)
{
  return self(p)->presetMatches(json ? json : "");
}
/* B100: the patch's pinned engine revision. */
extern "C" int hypersaw_debug_engine_revision(const clap_plugin_t *p)
{
  return self(p)->engineRevision();
}
/* 2026-09-11 chord-transposition hunt: the wheel lane's EMITTED value (what
   updateTuneAll multiplies every voice by) and its anchor key. A probe reads
   these between blocks; nothing in the audio thread changes. */
extern "C" double hypersaw_debug_pitchbend(const clap_plugin_t *p) { return self(p)->pitchBend; }
extern "C" int hypersaw_debug_lastnotekey(const clap_plugin_t *p) { return self(p)->lastNoteKey; }
/* Per-voice pitch state of oscillator 0: "slot,midi,gate,f0,f0cur,glideActive;…"
   for every gated or ringing voice. Read between blocks by a probe. */
/* The note law as oscillator 0's core holds it (2026-09-11 stuck-glide hunt). */
extern "C" void hypersaw_debug_notelaw(const clap_plugin_t *p, char *out, uint32_t cap)
{
  const auto &q = self(p)->cores[0].p.noteLaw;
  std::snprintf(out, cap, "model=%g tau=%g gtime=%g rate=%g springF=%g damp=%g distOver=%g retMul=%g quant=%g qhyst=%g qTime=%g | link=%g bend.model=%g bend.springF=%g",
                q.model, q.tau, q.gtime, q.rate, q.springF, q.damp, q.distOver, q.retMul, q.quant, q.qhyst, q.qTime,
                self(p)->noteLink, self(p)->bendLaw.model, self(p)->bendLaw.springF);
}
extern "C" bool hypersaw_debug_cornerapply(const clap_plugin_t *p, int k, const char *json) { return self(p)->cornerApply(k, json ? json : ""); }
extern "C" const char *hypersaw_debug_cornernames(const clap_plugin_t *p) { static std::string j; j = self(p)->cornerNamesJson(); return j.c_str(); }
extern "C" void hypersaw_debug_cornername(const clap_plugin_t *p, int k, const char *n) { self(p)->setCornerName(k, n ? n : ""); }
extern "C" bool hypersaw_debug_cornermatches(const clap_plugin_t *p, int k, const char *json) { return self(p)->cornerMatches(k, json ? json : ""); }
/* B84: the undo tree without a webview. One export, op-dispatched, so the
   check drives exactly the calls the GUI binds drive — a second entry point
   would be a second implementation of the thing under test.
     service | tree | json <i> | restore <i> | undo | redo | mark <label-id>
     | live | setmorph <0|1>   (B222; hypersaw_debug.h's op list predates them)
   Everything returns a string because two of the ops return JSON; the numeric
   ops return a decimal. */
extern "C" const char *hypersaw_debug_undo(const clap_plugin_t *p, const char *op, int arg)
{
  static std::string r;
  auto *pl = self(p);
  const std::string o = op ? op : "";
  if (o == "service") { pl->undoService(); r = "1"; }
  else if (o == "tree") r = pl->undoTreeJson();
  else if (o == "json") r = pl->undo.liveAt(arg) ? pl->undo.node(arg).json : std::string();
  else if (o == "label") r = pl->undo.liveAt(arg) ? pl->undo.node(arg).label : std::string();
  else if (o == "parent") r = std::to_string(pl->undo.liveAt(arg) ? pl->undo.node(arg).parent : -1);
  else if (o == "size") r = std::to_string(pl->undo.size());
  else if (o == "current") r = std::to_string(pl->undo.current());
  else if (o == "restore") r = pl->undoGoTo(arg) ? "1" : "0";
  else if (o == "undo") r = pl->undoStep(-1) ? "1" : "0";
  else if (o == "redo") r = pl->undoStep(1) ? "1" : "0";
  else if (o == "mark") { char lb[32]; std::snprintf(lb, sizeof lb, "probe %d", arg); pl->undoMark(lb); r = "1"; }
  // B222: what a node taken NOW would store, and the editor's morph toggle
  // (the value half of the checkbox; the bracket is hypersaw_debug_gesture).
  else if (o == "live") r = pl->historyJson();
  else if (o == "setmorph") { pl->guiSetParam(151, arg ? 1.0 : 0.0); r = "1"; }
  else r = "?";
  return r.c_str();
}
/* ENV 2 (pitch envelope) and the smoothed matrix pitch offset, read between
   blocks — the 2026-09-13 "pitch peak shrinks on consecutive notes" report.
   Since ADR-162 `env2` is the GLOBAL PROJECTION (mod source slot 1: the max
   over gated slots) and `stage` is the stage of the slot that won that max,
   -1 when nothing is gated. `pitchSm` is the global pitch lane, which the
   pitch route no longer feeds — it reading 0 while a note bends IS the
   per-note evidence. Per-slot state: hypersaw_debug_penv_slot. */
extern "C" void hypersaw_debug_penv(const clap_plugin_t *p, double *env2, double *stage, double *pitchSm)
{
  *env2 = self(p)->env2; *stage = self(p)->env2Stage; *pitchSm = self(p)->modPitchSm;
}
/* ONE slot's pitch envelope and the semitone offset it is contributing to that
   voice's noteTune through the composer (ADR-162). Diagnostic only. */
extern "C" void hypersaw_debug_penv_slot(const clap_plugin_t *p, int slot, double *level,
                                         double *stage, double *semis)
{
  if (slot < 0 || slot >= hypersaw::kPoly) { *level = 0; *stage = -1; *semis = 0; return; }
  *level = self(p)->penv[slot].level;
  *stage = self(p)->penv[slot].stage;
  *semis = self(p)->noteExpr[slot].penv;
}
/* B130: the phase circle's own numbers, headless. R (order parameter: 1 =
   locked, ~0 = a cloud or an even splay lattice), RA/RB (the two-cluster
   orders) and n (swarm size) are read from the SAME place publishViz copies
   them from — `cores[osc].focus()` — so a factory-bank assertion and the GUI
   can never disagree about what a preset does. Reading the core directly
   rather than the published VizSnapshot is deliberate: the snapshot follows
   `vizOsc` (the oscillator the editor is looking at), and a probe that had to
   write vizOsc to name an oscillator would be mutating GUI state to measure.
   Zero voices sounding -> all four read 0, which is the honest "nothing to
   observe" rather than a stale last value. Not part of the CLAP surface. */
extern "C" void hypersaw_debug_viz(const clap_plugin_t *p, int osc, double *R, double *RA,
                                   double *RB, int *n, double *RN)
{
  auto *pl = self(p);
  const uint32_t o = (osc > 0 && (uint32_t)osc < kNumOsc) ? (uint32_t)osc : 0;
  const auto &core = pl->cores[o];
  const auto *s = core.focus();
  if (R) *R = s ? s->R : 0.0;
  if (RA) *RA = s ? s->RA : 0.0;
  if (RB) *RB = s ? s->RB : 0.0;
  if (n) *n = s ? (int)core.p.n : 0;
  // B131: the n-th order parameter — 1 for an even lattice (splay), ~1/sqrt(n)
  // for a cloud — the observable R cannot give, appended to the signature.
  if (RN) *RN = s ? s->RN : 0.0;
}
/* B131: the voice phases themselves (0..1), so a check can compute any
   statistic — gap uniformity separates an even lattice from a cloud where
   neither R nor RN can (RN is scattered by a lock's finite phase spread). */
extern "C" int hypersaw_debug_phases(const clap_plugin_t *p, int osc, double *out, int cap)
{
  auto *pl = self(p);
  const uint32_t o = (osc > 0 && (uint32_t)osc < kNumOsc) ? (uint32_t)osc : 0;
  const auto &core = pl->cores[o];
  const auto *s = core.focus();
  if (!s || !out) return 0;
  const int n = std::min(cap, (int)core.p.n);
  for (int i = 0; i < n; i++) out[i] = s->phase[i];
  return n;
}
extern "C" void hypersaw_debug_voices(const clap_plugin_t *p, char *out, uint32_t cap)
{
  auto &core = self(p)->cores[0]; uint32_t n = 0; out[0] = 0;
  for (int i = 0; i < hypersaw::kPoly; i++)
  {
    const auto &v = core.voiceAt(i);
    if (!v.gate && v.env < 1e-4) continue;
    // noteTune last (ADR-162): the per-note pitch multiplier is what carries
    // the pitch envelope now, so the voice table has to show it or the
    // per-note claim is unfalsifiable from outside. Appended, never inserted —
    // preset_probe prints this line positionally.
    n += (uint32_t)std::snprintf(out + n, cap > n ? cap - n : 0, "%d,%d,%d,%.3f,%.3f,%d,%.6f;", i, v.midi, v.gate, v.f0, v.f0cur, v.glideActive, v.noteTune);
    if (n >= cap) break;
  }
}

bool gui_create(const clap_plugin_t *p, const char *api, bool is_floating)
{
  if (!gui_is_api_supported(p, api, is_floating)) return false;
  auto *pl = self(p);
  if (pl->gui) return true;
  hypersaw::GuiHost hostIf;
  hostIf.getViz = [pl]() {
    return pl->vizBuf[pl->vizPublished.load(std::memory_order_acquire)];
  };
  hostIf.getSpectrum = [pl](float *out, int n) { pl->computeSpectrum(out, n); };
  // B106: one reader, two entry points — getScope resolves the ACTIVE
  // oscillator (the OSC pages' view, unchanged), getScopeFor names one.
  hostIf.getScopeFor = [pl](int osc, float *l, float *r, int n) {
    const uint32_t o = (uint32_t)osc < kNumOsc ? (uint32_t)osc : 0;
    const uint32_t w = pl->scopePos[o].load(std::memory_order_acquire);
    for (int i = 0; i < n; i++)
    { const uint32_t k = (w - (uint32_t)n + (uint32_t)i) & 2047;
      l[i] = pl->scopeL[o][k]; r[i] = pl->scopeR[o][k]; }
  };
  hostIf.getScope = [pl](float *l, float *r, int n) {
    const uint32_t o = pl->vizOsc.load(std::memory_order_relaxed);
    const uint32_t v = o < kNumOsc ? o : 0;
    const uint32_t w = pl->scopePos[v].load(std::memory_order_acquire);
    for (int i = 0; i < n; i++)
    { const uint32_t k = (w - (uint32_t)n + (uint32_t)i) & 2047;
      l[i] = pl->scopeL[v][k]; r[i] = pl->scopeR[v][k]; }
  };
  hostIf.getParamsJson = [pl]() { return pl->paramsJson(); };
  hostIf.getDefaultsJson = [pl]() { return pl->defaultsJson(); };
  hostIf.getBendCurveJson = [pl]() { return pl->bendCurveJson(); };
  hostIf.getShapeWaveJson = [pl]() { return pl->shapeWaveJson(); };
  hostIf.getSubWaveJson = [pl]() { return pl->subWaveJson(); };   // B181 note 3
  hostIf.getLfoCycleJson = [pl]() { return pl->lfoCycleJson(); }; // B177 note
  hostIf.morphCapture = [pl](uint32_t k) { pl->morphCapture((int)k); };
  hostIf.morphCornerJson = [pl](uint32_t k) { return pl->cornerJson((int)k); };
  hostIf.morphLiveJson = [pl]() { return pl->liveCornerJson(); };
  hostIf.morphToggleExempt = [pl](uint32_t id) { return pl->morphToggleExempt((clap_id)id); };
  hostIf.morphExemptJson = [pl]() { return pl->morphExemptJson(); };
  hostIf.morphOwnersJson = [pl]() { return pl->morphOwnersJson(); };
  hostIf.modRoutesJson = [pl]() { return pl->modRoutesJson(); };
  hostIf.modLiveJson = [pl]() { return pl->modLiveJson(); };
  hostIf.modAddRoute = [pl](uint32_t src, uint32_t dest) { return pl->modAddRoute(src, dest); };
  hostIf.modSetDepth = [pl](int i, double v) {
    // No index-0 special case: ADR-138 made knob 161 find its route BY DEST,
    // so the depth of whatever sits at index 0 is nobody's secret twin.
    if (i >= 0 && i < pl->mod.nRoutes) pl->mod.routes[i].depth = v;
  };
  hostIf.modSetSource = [pl](int i, uint32_t src) { return pl->modSetSource(i, src); };
  hostIf.modSetPolarity = [pl](int i, int pol) { return pl->modSetPolarity(i, pol); };
  hostIf.setModWheel = [pl](double v) { pl->srcWheel = v < 0 ? 0 : (v > 1 ? 1 : v); };
  hostIf.modRemoveRoute = [pl](int i) { pl->mod.removeRoute(i); };
  hostIf.morphCornerValsJson = [pl](int k) { return pl->morphCornerValsJson(k); };
  hostIf.morphCornerApply = [pl](uint32_t k, const std::string &j) { return pl->cornerApply((int)k, j); };
  hostIf.morphCornerNamesJson = [pl]() { return pl->cornerNamesJson(); };                      // B122
  hostIf.morphCornerSetName = [pl](int k, const std::string &n) { pl->setCornerName(k, n); };  // B122
  hostIf.morphCornerMatches = [pl](int k, const std::string &j) { return pl->cornerMatches(k, j); };   // B122
  hostIf.setParam = [pl](uint32_t id, double v) { pl->guiSetParam((clap_id)id, v); };
  hostIf.gesture = [pl](uint32_t id, bool begin) { pl->guiGesture((clap_id)id, begin); };
  // Stamp carries hash AND build time: a hash alone cannot distinguish "the
  // binary I just built" from "a binary built from the same commit last week",
  // which is precisely the stale-install question (L0020).
  // PANIC: kill everything a stuck note could be hiding in. There was no such
  // control at all before 2026-08-03, so a stuck voice meant deleting the
  // device. Clears both engines, every note tag (including the pending-END
  // queue), the mono held-stack, and the FX rack's tails.
  hostIf.panic = [pl]() { pl->panicWithDump(); };
  hostIf.getBuildId = []() { return std::string(HYPERSAW_BUILD_STAMP); };
  hostIf.getHostHint = [pl]() { return pl->hostHint(); };
  hostIf.setVizOsc = [pl](uint32_t k) { pl->vizOsc.store(k, std::memory_order_relaxed); };
  hostIf.getStateJson = [pl]() { return pl->stateJson(); };
  hostIf.applyStateJson = [pl](const std::string &s, const std::string &n) {
    return pl->applyStateJson(s, n);
  };
  hostIf.presetNameGet = [pl]() { return pl->presetName; };                              // B174
  hostIf.presetSetName = [pl](const std::string &n) { pl->setPresetName(n); };           // B174
  hostIf.presetMatches = [pl](const std::string &j) { return pl->presetMatches(j); };    // B174
  hostIf.undoService = [pl]() { pl->undoService(); };
  hostIf.undoTreeJson = [pl]() { return pl->undoTreeJson(); };
  hostIf.undoRestore = [pl](int i) { return pl->undoGoTo(i); };
  hostIf.undoStep = [pl](int dir) { return pl->undoStep(dir); };
  pl->gui = new hypersaw::HypersawGui(std::move(hostIf));
  return true;
}

void gui_destroy(const clap_plugin_t *p)
{
  auto *pl = self(p);
  delete pl->gui;
  pl->gui = nullptr;
}

bool gui_set_scale(const clap_plugin_t *, double) { return true; }

bool gui_get_size(const clap_plugin_t *p, uint32_t *w, uint32_t *h)
{
  *w = self(p)->guiW;
  *h = self(p)->guiH;
  return true;
}

bool gui_can_resize(const clap_plugin_t *) { return true; }

bool gui_get_resize_hints(const clap_plugin_t *, clap_gui_resize_hints_t *hints)
{
  hints->can_resize_horizontally = true;
  hints->can_resize_vertically = true;
  hints->preserve_aspect_ratio = false;
  hints->aspect_ratio_width = 0;
  hints->aspect_ratio_height = 0;
  return true;
}

bool gui_adjust_size(const clap_plugin_t *, uint32_t *w, uint32_t *h)
{
  *w = std::max(720u, std::min(1600u, *w));
  *h = std::max(440u, std::min(1000u, *h));
  return true;
}

bool gui_set_size(const clap_plugin_t *p, uint32_t w, uint32_t h)
{
  auto *pl = self(p);
  pl->guiW = w;
  pl->guiH = h;
  return true;  // the webview child autoresizes with the reparented view
}

bool gui_set_parent(const clap_plugin_t *p, const clap_window_t *window)
{
  auto *pl = self(p);
  if (!pl->gui || !window) return false;
#ifdef __APPLE__
  return pl->gui->attachToParent(window->cocoa);
#else
  return pl->gui->attachToParent(window->win32);
#endif
}

bool gui_set_transient(const clap_plugin_t *, const clap_window_t *) { return false; }
void gui_suggest_title(const clap_plugin_t *, const char *) {}
bool gui_show(const clap_plugin_t *) { return true; }
bool gui_hide(const clap_plugin_t *) { return true; }

const clap_plugin_gui_t s_gui = {gui_is_api_supported, gui_get_preferred_api, gui_create,
                                 gui_destroy,          gui_set_scale,         gui_get_size,
                                 gui_can_resize,       gui_get_resize_hints,  gui_adjust_size,
                                 gui_set_size,         gui_set_parent,        gui_set_transient,
                                 gui_suggest_title,    gui_show,              gui_hide};

#endif  // __APPLE__ || _WIN32

/* ---- clap-wrapper VST3 specifics (ADR-038) ----
 * Without this extension the VST3 wrapper advertises only PRESSURE through
 * INoteExpressionController (its CLAP_SUPPORTS_ALL_NOTE_EXPRESSIONS compile
 * flag defaults OFF and make_clapfirst_plugins never forwards it), so
 * note-expression-speaking hosts never send the per-note TUNING stream
 * ADR-036 listens for. PRESSURE is kept to match the wrapper's default. */
uint32_t v3spec_num_midi_channels(const clap_plugin *, uint32_t) { return 16; }
uint32_t v3spec_note_expressions(const clap_plugin *)
{
  return AS_VST3_NOTE_EXPRESSION_TUNING | AS_VST3_NOTE_EXPRESSION_PRESSURE;
}
const clap_plugin_as_vst3_t s_vst3_specifics = {v3spec_num_midi_channels, v3spec_note_expressions};

const void *plug_get_extension(const clap_plugin_t *, const char *id)
{
  if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &s_audio_ports;
  if (!std::strcmp(id, CLAP_EXT_NOTE_PORTS)) return &s_note_ports;
  if (!std::strcmp(id, CLAP_EXT_PARAMS)) return &s_params;
  if (!std::strcmp(id, CLAP_EXT_STATE)) return &s_state;
  if (!std::strcmp(id, CLAP_PLUGIN_AS_VST3)) return &s_vst3_specifics;
#if defined(__APPLE__) || defined(_WIN32)
  if (!std::strcmp(id, CLAP_EXT_GUI)) return &s_gui;
#endif
  return nullptr;
}

void plug_on_main_thread(const clap_plugin_t *) {}

/* ---- factory ---- */

uint32_t factory_get_plugin_count(const clap_plugin_factory *) { return 1; }

const clap_plugin_descriptor_t *factory_get_plugin_descriptor(const clap_plugin_factory *,
                                                              uint32_t index)
{
  return index == 0 ? &s_desc : nullptr;
}

const clap_plugin_t *factory_create_plugin(const clap_plugin_factory *, const clap_host_t *host,
                                           const char *plugin_id)
{
  if (std::strcmp(plugin_id, s_desc.id) != 0) return nullptr;
  auto *pl = new Plugin();
  // ADR-156: no default routes, no birth-time floor writes — the table's
  // defaults (detune 0.28, K 0) are the cores' own, so readback agrees by
  // construction (paramscope's default-lie sweep is the gate).
  pl->host = host;
  pl->plugin.desc = &s_desc;
  pl->plugin.plugin_data = pl;
  pl->plugin.init = plug_init;
  pl->plugin.destroy = plug_destroy;
  pl->plugin.activate = plug_activate;
  pl->plugin.deactivate = plug_deactivate;
  pl->plugin.start_processing = plug_start_processing;
  pl->plugin.stop_processing = plug_stop_processing;
  pl->plugin.reset = plug_reset;
  pl->plugin.process = plug_process;
  pl->plugin.get_extension = plug_get_extension;
  pl->plugin.on_main_thread = plug_on_main_thread;
  return &pl->plugin;
}

const clap_plugin_factory_t s_factory = {factory_get_plugin_count, factory_get_plugin_descriptor,
                                         factory_create_plugin};

}  // namespace

extern "C"
{
  const char *hypersaw_test_host_hint(const clap_plugin_t *p)
{
  static std::string held;
  held = self(p)->hostHint();
  return held.c_str();
}

const char *hypersaw_test_panic(const clap_plugin_t *p)
{
  static std::string held;
  self(p)->panicWithDump();
  held = self(p)->lastDumpPath;
  return held.empty() ? nullptr : held.c_str();
}

const char *hypersaw_test_dump_forensics(const clap_plugin_t *p, const char *why)
{
  static std::string held;
  held = self(p)->dumpForensics(why ? why : "test");
  return held.empty() ? nullptr : held.c_str();
}

/* ---- note-bookkeeping introspection, for the FOUNDATIONS conformance suite --
   These are READ-ONLY windows plus ONE shipped mutator (retireTag). They exist
   so an external suite can assert our tag tables without the adapter
   reimplementing any of the behaviour under test: the notes themselves still
   arrive as real CLAP events through the real process() path, and the steal
   decision still happens where it lives (swarm_core.h alloc()). An adapter that
   recomputed "who should have been stolen" would be an oracle checking its own
   copy of the rule (L0031). */

int hypersaw_test_poly(void) { return (int)hypersaw::kPoly; }

bool hypersaw_test_tag_at(const clap_plugin_t *p, int slot, int32_t *note_id, int16_t *port,
                          int16_t *channel, int16_t *key)
{
  if (slot < 0 || slot >= (int)hypersaw::kPoly) return false;
  const auto &t = self(p)->tags[slot];
  if (note_id) *note_id = t.noteId;
  if (port) *port = t.port;
  if (channel) *channel = t.channel;
  if (key) *key = t.key;
  return t.active;
}

/* Calls the SHIPPED retireTag() — the same function a steal and a mono retarget
   call — and reports the identity it took. Returns false when the slot held
   nothing, which is what makes the suite's no-double-END case meaningful: the
   second call must find an inactive tag and yield no identity. */
bool hypersaw_test_retire_slot(const clap_plugin_t *p, int slot, int32_t *note_id, int16_t *port,
                               int16_t *channel, int16_t *key)
{
  if (slot < 0 || slot >= (int)hypersaw::kPoly) return false;
  auto *s = self(p);
  const auto before = s->tags[slot];
  s->retireTag(slot);
  if (!before.active) return false;
  if (note_id) *note_id = before.noteId;
  if (port) *port = before.port;
  if (channel) *channel = before.channel;
  if (key) *key = before.key;
  return true;
}

/* Gate state of the logical voice at `slot`, read from oscillator 0's voice —
   `slotOf[slot][0] == slot` by definition. "Released" for the steal cases means
   gate == 0, which is exactly the predicate alloc()'s tiers read. */
bool hypersaw_test_slot_gated(const clap_plugin_t *p, int slot)
{
  if (slot < 0 || slot >= (int)hypersaw::kPoly) return false;
  return self(p)->core.voiceAt(slot).gate != 0;
}

bool hypersaw_test_mod_add(const clap_plugin_t *p, uint32_t srcSlot, uint32_t destId)
{
  return self(p)->modAddRoute(srcSlot, destId);
}
void hypersaw_test_mod_remove(const clap_plugin_t *p, int idx) { self(p)->mod.removeRoute(idx); }
double hypersaw_test_mod_applied(const clap_plugin_t *p, uint32_t destId)
{
  for (auto &d : self(p)->modDests)
    if (d.active && d.id == destId) return d.lastApplied;
  return -1e300;
}

bool hypersaw_entry_init(const char *) { return true; }
  void hypersaw_entry_deinit(void) {}
  const void *hypersaw_entry_get_factory(const char *factory_id)
  {
    if (!std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID)) return &s_factory;
    return nullptr;
  }
}
