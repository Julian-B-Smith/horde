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

#include "swarm_core.h"
#include "gui/hypersaw_gui.h"
#include "spectra_core.h"
#include "glide_core.h"
#include "mod_core.h"
#include "morph_core.h"
#include "depends_graph.h"
#include "fx_rack.h"
#include "routing_core.h"
#include "hypersaw_clap_entry.h"
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
static const char *const kFxTypeLabels[] = {"Off",  "Drive", "Filter", "Gain",
                                            "Comp", "Comb",  "Notch", "Echo", "Room", "Delay"};
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
    {6, "K", "Pull K", -1, 1, 0, false, nullptr},   // ADR-156: the lab default, see detune
    {7, "onset", "Onset Lock", -1, 1, 0, false, nullptr},  // ADR-056: bipolar (<0 = splay onset)
    {8, "dissolve", "Dissolve (s)", 0.05, 7.94, 0.63, false, nullptr},
    {9, "driftDepth", "Drift Depth (c)", 0, 100, 0, false, nullptr},  // widened from the
    // prototype's 25c at human request (ADR-020); core takes any cents value
    {10, "driftRate", "Drift Rate", 0, 1, 0.4, false, nullptr},
    {11, "inertia", "Inertia", 0, 1, 0, false, nullptr},
    {12, "rtone", "R->Tone", -1, 1, 0, false, nullptr},
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
    {31, "absK", "Absolute K", 0, 1, 0, true, kOffOn},
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
       its own assignment pair, not a view of the active osc's. */
    {179, "mainAsnX", "Main X > Macro", 0, 8, 0, true, nullptr},
    {180, "mainAsnY", "Main Y > Macro", 0, 8, 1, true, nullptr},
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
    15, 40, 41,                                  // output & image
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

const ParamDef *findParam(clap_id id)
{
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
  }
  void noteOffAll(int key)
  {
    for (uint32_t k = 0; k < kNumOsc; k++) cores[k].noteOff(key);
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
  }
  void setNoteExprAll(int slot, double v)
  {
    if (slot < 0) return;
    for (uint32_t k = 0; k < kNumOsc; k++) cores[k].setNoteExpr(slotOf[slot][k], v);
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
    fs::path dir;
#ifdef __APPLE__
    if (const char *home = std::getenv("HOME")) dir = fs::path(home) / "Library" / "Logs" / "HYPERSAW";
#endif
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
  // B23 crosspoint topology over those slots (ADR-088). One source for now —
  // the summed, post-bass-mono bus — so this increment is purely "the matrix is
  // in the audio path and inert". Per-oscillator sources are a later increment
  // and carry their own decision, because sources upstream of bass-mono is
  // exactly the ordering question this increment declined to force.
  hypersaw::RoutingMatrix<1, hypersaw::kRackSlots> routing;
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
    uint8_t kind;  // 0=value, 1=gesture begin, 2=gesture end
  };
  static constexpr uint32_t kQCap = 256;
  ParamMsg queue[kQCap];
  std::atomic<uint32_t> qHead{0}, qTail{0};

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
  float scopeL[2048] = {0}, scopeR[2048] = {0};
  std::atomic<uint32_t> scopePos{0};
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
    // ADR-100 A4: scope tap — this oscillator's own post-gain signal, when it
    // is the one the viz follows. Ring write only; RT-safe.
    if (k == vizOsc.load(std::memory_order_relaxed))
    {
      uint32_t sw = scopePos.load(std::memory_order_relaxed);
      for (int i = 0; i < n; i++)
      { scopeL[(sw + i) & 2047] = bL[i]; scopeR[(sw + i) & 2047] = bR[i]; }
      scopePos.store(sw + (uint32_t)n, std::memory_order_release);
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
  double morphArm = 0;
  bool morphFromField = false;   // ADR-109 re-entry guard, see the hook
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
  hypersaw::ModCore mod;
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
  /* ADR-135: ENV 2, a shell-side ADSR advanced at the mod grid. One-pole
     approaches per stage (attack -> 1, decay -> sustain, release -> 0), gated
     by "any voice gated across enabled oscillators" — a global paraphrase,
     stated plainly: per-note ENV 2 is the per-note fan-out increment, not
     this one. env2Gate tracks the edge so attack restarts on the first new
     gate after silence, matching how a player reads a monophonic envelope. */
  double env2 = 0, env2A = 0.003, env2D = 0.16, env2S = 0.0, env2R = 0.16;
  int env2Stage = 0;   // 0 idle, 1 attack, 2 decay/sustain
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
  bool env2Gate = false;
  hypersaw::MorphCore morph;
  std::vector<clap_id> morphIds;          // id order = persistence order (stable)
  std::vector<double> morphCorner[4];     // snapshots, aligned to morphIds
  std::vector<double> morphCur;           // last applied value per morphIds slot
  // ADR-115: MUST match the ParamDef defaults for 152/153 (corner A = 0,0).
  // paramscope_check's default-truth sweep exists for exactly this pair going
  // out of step, and caught it the first time this changed.
  double morphX = 0.0, morphY = 0.0, morphTemp = 1, morphCoup = 0.3;
  double morphOn = 0, morphMode = 0, morphGlideS = 0.008;
  uint32_t morphSeed = 1024;
  int morphAccum = 0;

  void morphInit()
  {
    if (!morphIds.empty()) return;
    for (const auto &d : kParams)
    {
      if (isGlobalId(d.id)) continue;
      morphIds.push_back(d.id);
      morphIds.push_back(d.id + 1000);    // the twin — each osc morphs its own
    }
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

    for (int k = 0; k < 4; k++) morphCorner[k].assign(morphIds.size(), 0.0);
    morphCur.assign(morphIds.size(), -1e30);
    morphExempt.assign(morphIds.size(), 0);
    morph.reshuffle(morphSeed, (int)morphIds.size());
    // A fresh instance's corners all hold the DEFAULT patch, so switching morph
    // on before capturing anything is silence-safe: every corner agrees.
    for (size_t i = 0; i < morphIds.size(); i++)
    {
      const ParamDef *d = findParam(morphIds[i]);
      const double v = d ? defaultFor(*d, morphIds[i] / 1000) : 0.0;
      for (int k = 0; k < 4; k++) morphCorner[k][i] = v;
    }
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
  }

  /* One morph step, on the 256-sample gravity grid (heavier than the bend grid
     on purpose — a parameter field does not need 2.7 kHz updates, and the grid
     accumulator makes the result independent of host buffer subdivision, the
     ADR-086 rule). Stepped params take their winning corner's value outright;
     continuous params either flip (quantum) with a one-pole slew toward the
     winner, or blend (mode 1) across all four corners. */
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
    if (morphOn <= 0.5) return true;
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
         proportional is the reading that respects where you are standing. */
      double cur = 0, sumsq = 0;
      for (int k = 0; k < 4; k++) { cur += w[k] * morphCorner[k][idx]; sumsq += w[k] * w[k]; }
      if (sumsq > 1e-12)
      {
        const double scale = (v - cur) / sumsq;
        for (int k = 0; k < 4; k++) morphCorner[k][idx] += w[k] * scale;
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
  std::string modRoutesJson()
  {
    std::string out = "[";
    char buf[128];
    for (int r = 0; r < mod.nRoutes; r++)
    {
      const auto &q = mod.routes[r];
      std::snprintf(buf, sizeof buf, "%s{\"i\":%d,\"src\":%u,\"dest\":%u,\"depth\":%.6g}",
                    r ? "," : "", r, q.src, q.dest, q.depth);
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
     One line, generic routes only: `src:dest:depth;…`. The pitch route is
     param 161's and persists as that param — writing it here too would double
     it on load. Serialization CANONICALIZES: one entry per (src, dest) with
     summed depth, which the SUM law already makes indistinguishable from the
     un-merged form — this is the identity B72's morph interpolation will key
     on, established at the serialization boundary first. */
  std::string modRoutesChunk() const
  {
    uint32_t ks[hypersaw::ModCore::kMaxRoutes], kd[hypersaw::ModCore::kMaxRoutes];
    double dep[hypersaw::ModCore::kMaxRoutes];
    int n = 0;
    for (int r = 0; r < mod.nRoutes; r++)
    {
      const auto &q = mod.routes[r];
      if (q.dest & kModDestSynthetic) continue;
      int j = 0;
      while (j < n && !(ks[j] == q.src && kd[j] == q.dest)) j++;
      if (j == n) { ks[n] = q.src; kd[n] = q.dest; dep[n] = q.depth; n++; }
      else dep[j] += q.depth;
    }
    std::string out;
    char buf[64];
    for (int j = 0; j < n; j++)
    {
      std::snprintf(buf, sizeof buf, "%u:%u:%.6g;", ks[j], kd[j], dep[j]);
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
      if (std::sscanf(ent.c_str(), "%u:%u:%lf", &src, &dest, &depth) != 3) continue;
      // Through the shipped refusal path — a chunk naming a stepped dest, the
      // matrix's own controls, or a bad source is dropped, never trusted.
      if (!modAddRoute(src, dest)) continue;
      mod.routes[mod.nRoutes - 1].depth = std::max(-1.0, std::min(1.0, depth));
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
    bool anyGate = false;
    for (uint32_t k = 0; k < kNumOsc; k++)
      if (oscEnabled[k])
        for (int i = 0; i < (int)hypersaw::kPoly; i++)
        {
          const auto &v = cores[k].voiceAt(i);
          if (v.env > envMax) envMax = v.env;
          if (v.gate) anyGate = true;
        }
    mod.src[0] = envMax;
    /* ADR-135: ENV 2. Stage machine on the gate edge, one-pole approaches per
       stage. Time constants are the knobs' SECONDS converted per tick
       (ADR-009's rule — never hand-tuned per-tick constants). */
    {
      if (anyGate && !env2Gate) { env2Stage = 1; }             // fresh gate: attack
      if (!anyGate) env2Stage = 0;                              // all keys up: release
      env2Gate = anyGate;
      double target, tau;
      if (env2Stage == 1) { target = 1.0; tau = env2A; }
      else if (env2Stage == 2) { target = env2S; tau = env2D; }
      else { target = 0.0; tau = env2R; }
      env2 += (target - env2) * (1.0 - std::exp(-dt / std::max(1e-4, tau)));
      if (env2Stage == 1 && env2 > 0.99) { env2 = 1.0; env2Stage = 2; }
      mod.src[1] = env2;
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
      if (dests[i] == kModDestPitch) { pitch = deltas[i]; continue; }
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

  void morphStep(int samples)
  {
    morphAccum += samples;
    const int grid = (int)std::lround(sampleRate * hypersaw::kGravGridSeconds);
    if (morphAccum < grid) return;
    const double dt = (double)morphAccum / sampleRate;
    morphAccum = 0;
    double w[4], lw[4];
    hypersaw::MorphCore::weights(morphX, morphY, w);
    hypersaw::MorphCore::logW(w, morphTemp, lw);
    const double coef = morphGlideS > 1e-4 ? 1 - std::exp(-dt / morphGlideS) : 1.0;
    for (size_t i = 0; i < morphIds.size(); i++)
    {
      const ParamDef *d = findParam(morphIds[i]);
      if (!d) continue;
      // ADR-109: an exempt parameter is not in the field at all — it holds
      // whatever it is set to, and no corner owns it.
      if (i < morphExempt.size() && morphExempt[i])
      {
        // B48: an exempt enable is fully live, so its ramp must not linger.
        if (baseIdOf(morphIds[i]) == 150)
        {
          const uint32_t o = oscOfId(morphIds[i]);
          if (o < kMaxOsc) oscOnW[o] = 1.0;
        }
        continue;
      }
      double target;
      /* B48 SPECIAL CASE — osc on/off morphs as a LEVEL RAMP, not a pick
         (human 2026-08-26). The stepped pick drew enable from one corner while
         vol came from another, and ADR-100's off transition hard-kills voices,
         so the boundary was a click and the partway state a chimera. Here the
         BILINEAR weight of the corners that hold the osc ON becomes a gain
         ramp (applied in applyOscGainAndMeter through the existing ~8 ms
         smoother), and the stepped flip is deferred to the weight floor,
         where the osc is already ~-60 dB: the kill/re-strike still runs, but
         inaudibly. Plain w[], not the Gumbel draw -- the ramp is deterministic
         in the pad position, both modes. At a pure corner the weight equals
         that corner's stored enable, so corners stay bit-identical. */
      if (baseIdOf(morphIds[i]) == 150)
      {
        double onW = 0;
        for (int k = 0; k < 4; k++) onW += w[k] * morphCorner[k][i];
        onW = onW < 0 ? 0 : (onW > 1 ? 1 : onW);
        const uint32_t o = oscOfId(morphIds[i]);
        if (o < kMaxOsc) oscOnW[o] = onW;
        const double next = onW > 1e-3 ? 1.0 : 0.0;
        if (std::fabs(next - morphCur[i]) > 1e-9)
        {
          morphCur[i] = next;
          morphFromField = true;
          applyParam(morphIds[i], next);
          morphFromField = false;
        }
        continue;
      }
      if ((int)morphMode == 1 && !d->stepped)
      {
        target = 0;
        for (int k = 0; k < 4; k++) target += w[k] * morphCorner[k][i];
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
      double next = d->stepped ? target
                               : (morphCur[i] < -1e29 ? target
                                                      : morphCur[i] + (target - morphCur[i]) * coef);
      if (d->stepped) next = std::round(next);
      if (std::fabs(next - morphCur[i]) > 1e-9)
      {
        morphCur[i] = next;
        morphFromField = true;
        applyParam(morphIds[i], next);
        morphFromField = false;
      }
    }
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
  double masterVol = 1.0, masterVolSm = 1.0;   // B24: target + smoothed
  // Which oscillator the visuals describe. GUI-owned (follows the OSC tab),
  // audio-thread-read. The visuals were hardwired to oscillator 0 — the
  // intermediary the human asked for is this one index.
  std::atomic<uint32_t> vizOsc{0};
  double bmIc1 = 0, bmIc2 = 0;

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
      setNoteExprAll(slot, semis);
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
    setNoteExprAll(slot, semis);
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
      if (v != nb.emitted) { nb.emitted = v; setNoteExprAll(i, v); }
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
      if (m.kind == 0)
      {
        applyParam(m.id, m.value);
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
      else if (out)
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
      tail++;
    }
    qTail.store(tail, std::memory_order_release);
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
    std::string out = "{\"cornerPreset\":[";
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
    c++;
    morphCornersAuthored = true;
    for (size_t i = 0; i < morphIds.size(); i++)
    {
      morphCorner[k][i] = std::atof(c);
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
    std::string out = "{\"cornerPreset\":[";
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
  void applyMorphChunk(const std::string &json)
  {
    {
      size_t ep = json.find("\"morphExempt\"");
      if (ep != std::string::npos)
      {
        morphInit();
        const char *c = std::strchr(json.c_str() + ep, '[');
        if (c)
        {
          c++;
          for (size_t i = 0; i < morphExempt.size(); i++)
          {
            morphExempt[i] = (uint8_t)(std::atoi(c) != 0);
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
          c++;
          for (size_t i = 0; i < morphIds.size(); i++)
          {
            morphCorner[k][i] = std::atof(c);
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

  std::string morphJson()
  {
    if (morphIds.empty()) return "";
    std::string out = ",\"morphCorners\":[";
    char buf[32];
    for (int k = 0; k < 4; k++)
    {
      out += k ? ",[" : "[";
      for (size_t i = 0; i < morphIds.size(); i++)
      {
        std::snprintf(buf, sizeof(buf), i ? ",%.6g" : "%.6g", morphCorner[k][i]);
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
     gated law; no revision-2 law exists yet, so the only observable today is
     the round-trip (tools/state_check.cpp, tools/statefix_check.cpp).
     A revision this build cannot honour clamps to the latest it knows: a
     value with no laws behind it is never stored, and the re-save then
     records what actually rendered. engineRevision() is the ONE read site —
     a future gated law consults it there, never a copy, so a law cannot fork
     on a stale snapshot. Atomic because the read site will be the audio
     thread and the write site is state_load on the main thread. */
  static constexpr int kEngineRevision = 1;
  std::atomic<int> patchEngineRevision{kEngineRevision};
  int engineRevision() const { return patchEngineRevision.load(std::memory_order_relaxed); }
  void setEngineRevision(long rev)
  {
    const long r = std::max(1L, std::min((long)kEngineRevision, rev));
    patchEngineRevision.store((int)r, std::memory_order_relaxed);
  }

  std::string stateJson() const
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
    // const_cast confined to serialisation: morphJson touches no state, but
    // morphIds is lazily built and stateJson is const. Building eagerly at
    // construction would be cleaner; deferred to keep this diff reviewable.
    std::string tail = "}" + const_cast<Plugin *>(this)->morphJson();
    // ADR-138: routes in the preset too, same canonical chunk as state_save —
    // one serializer, two transports. Only when routes exist (see state_save).
    const std::string routes = modRoutesChunk();
    if (!routes.empty()) tail += ",\"modRoutes\":\"" + routes + "\"";
    return out + tail + "}";
  }

  bool applyStateJson(const std::string &json)
  {
    // Tolerant flat scan of our own schema: for each known coreKey, find
    // "key" and parse the number after the colon. Queued to the audio
    // thread — never applied directly from the GUI thread.
    if (json.find("\"params\"") == std::string::npos) return false;
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
    bool any = false;
    for (const auto &d : kParams)
    {
      if (d.id == 178) continue;   // ADR-147: specimen is not patch state (see state_load)
      const std::string needle = "\"" + std::string(d.coreKey) + "\"";
      size_t pos = json.find(needle);
      if (pos == std::string::npos) continue;
      pos = json.find(':', pos + needle.size());
      if (pos == std::string::npos) continue;
      enqueueParam(d.id, std::atof(json.c_str() + pos + 1), 0);
      any = true;
    }
    // the twins, by the state_save convention
    for (uint32_t k = 1; k < kNumOsc; k++)
      for (const auto &d : kParams)
      {
        if (isGlobalId(d.id)) continue;
        char nb[64];
        std::snprintf(nb, sizeof(nb), "\"o%u.%s\"", k, d.coreKey);
        size_t pos = json.find(nb);
        if (pos == std::string::npos) continue;
        pos = json.find(':', pos + std::strlen(nb));
        if (pos == std::string::npos) continue;
        enqueueParam(d.id + k * 1000, std::atof(json.c_str() + pos + 1), 0);
        any = true;
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
      enqueueParam(137, 0, 0);                                  // own settings
      enqueueParam(138, hypersaw::GlideCore::kLag, 0);          // lag, as it always was
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
      enqueueParam(150, 1, 0);
      enqueueParam(1150, 1, 0);
    }
    return any;
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
      if (id >= 166 && id <= 173) { macroVal[id - 166] = applied; return; }
      if (id >= 174 && id <= 177) { xyAsn[id - 174] = (int)applied; return; }
      if (id == 179 || id == 180) { mainAsn[id - 179] = (int)applied; return; }
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
                    // B48: morph off releases the on-weight ramp, else the
                    // last partway value would keep scaling a morph-free patch.
                    if (morphOn <= 0.5)
                      for (uint32_t k2 = 0; k2 < kMaxOsc; k2++) oscOnW[k2] = 1.0;
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
        if (applied != 0 && bassMonoOn == 0) bmIc1 = bmIc2 = 0;  // clean engage
        bassMonoOn = applied;
        return;
      }
      if (id == 41)
      {
        bassMonoHz = applied;
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
             the refusal rather than a phantom second Comb. */
          if (!rack.typeAllowed(slot, (int)applied)) return;
          rack.setType(slot, (int)applied);
        }
        else rack.setAmount(slot, applied);
        return;
      }
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
          retireTag(slot);
          lastNoteKey = n->key;
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
          tags[monoSlot] = {n->note_id, n->port_index, n->channel, n->key, true, (float)n->velocity};
          struck = monoSlot;
        }
        else
        {
          const int slot = core.noteOn(n->key, freq);
          core.setNoteVelocity(slot, n->velocity);
          bindSlots(slot, 0, slot);
          for (uint32_t k = 1; k < kNumOsc; k++)
          {
            const int sk = cores[k].noteOn(n->key, freq);
            cores[k].setNoteVelocity(sk, n->velocity);
            bindSlots(slot, k, sk);   // sk may differ from slot
          }
          retireTag(slot);
          lastNoteKey = n->key;
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
    // sum. At their default vol = 0 they add exact zeros, so a patch that
    // never touches them is bit-identical to a one-oscillator build — which
    // is what keeps the 147 parity goldens green.
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
      float tL[kMixChunk], tR[kMixChunk];
      for (int off = 0; off < n; off += kMixChunk)
      {
        const int m = n - off < kMixChunk ? n - off : kMixChunk;
        cores[k].render(tL, tR, m);
        applyOscGainAndMeter(k, tL, tR, m, true);
        for (int i = 0; i < m; i++)
        {
          outL[at + off + i] += tL[i];
          outR[at + off + i] += tR[i];
        }
      }
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
        spectra.render(outL + frame, outR + frame, (int)(until - frame));
      else
        renderSpan(outL, outR, frame, (uint32_t)(until - frame));
      frame = until;
    }

    // ADR-035 bass mono: runs BEFORE the spectrum feed so the visualizer
    // shows what actually leaves the plugin.
    if (bassMonoOn != 0)
    {
      constexpr double kPi = 3.141592653589793;
      const double fc = std::min(bassMonoHz, 0.45 * sampleRate);
      const double g = std::tan(kPi * fc / sampleRate);
      const double k = 1.4142135623730951;  // Butterworth 2nd order
      const double a0 = 1.0 / (1.0 + g * (g + k));
      for (uint32_t i = 0; i < nframes; i++)
      {
        const double m = 0.5 * (outL[i] + outR[i]);
        const double sIn = 0.5 * (outL[i] - outR[i]);
        const double hp = (sIn - (g + k) * bmIc1 - bmIc2) * a0;
        const double v1 = g * hp;
        const double bp = v1 + bmIc1;
        bmIc1 = bp + v1;
        const double v2 = g * bp;
        bmIc2 = v2 + bmIc2 + v2;
        outL[i] = (float)(m + hp);
        outR[i] = (float)(m - hp);
      }
    }

    // Internal FX rack (ADR-054), now driven THROUGH the B23 crosspoint matrix
    // (ADR-088) rather than as a hardcoded series. Post-oscillator,
    // post-bass-mono; runs before the spectrum feed so the visualizer reflects
    // post-FX output.
    //
    // Bass-mono stays UPSTREAM of the rack. The reorder was considered and
    // dropped: the argument for moving it was that a decorrelating slot
    // downstream could undo the mono guarantee, and measurement refuted it —
    // Comb at amount 0.9 scales the sub-crossover channel difference by 2.2x
    // whether bass-mono is on or off, leaving the same ~11% residual either
    // way, because it is a stereo-SYMMETRIC filter. No current slot type
    // decorrelates, so there is no correctness case, and an audible reorder
    // with no oracle behind it is not one to make on taste.
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
        const float *srcL[1] = {outL + off};
        const float *srcR[1] = {outR + off};
        routing.processBlock(srcL, srcR, slotL, slotR, outL + off, outR + off, m,
                             [&](int slot, float *L, float *R, int n) {
                               rack.processSlot(slot, L, R, n);
                             });
      }
    }

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

bool plug_activate(const clap_plugin_t *p, double sr, uint32_t, uint32_t)
{
  auto *pl = self(p);
  pl->sampleRate = sr;
  // Recreate the core at the host rate, preserving params (constructor cost
  // is trivial; activate is main-thread and never concurrent with process).
  for (uint32_t k = 0; k < kNumOsc; k++)
  {
    hypersaw::Params saved = pl->cores[k].p;
    pl->cores[k] = hypersaw::SwarmCore(sr);
    pl->cores[k].p = saved;
    pl->cores[k].setParam("seed", saved.seed);  // re-trigger rebuild() with saved state
  }
  hypersaw::SpectraCore::SParams sp = pl->spectra.p;
  pl->spectra = hypersaw::SpectraCore(sr);
  pl->spectra.p = sp;
  pl->spectra.rebuild();
  pl->rack.setSampleRate(sr);  // ADR-071: size comb lines + derive comp coeffs at sr
  // The shell owns the note law (it resolves the link), so push it once here.
  // Without this the cores run GlideCore's OWN defaults until the first edit of
  // a note/bend/scale param -- including an empty scale mask, which the
  // quantiser would read as "no degree admitted".
  pl->pushNoteLaw();
  // ADR-104: morph tables are built HERE, on the main thread — morphInit
  // allocates, and applyParam(151) can arrive on the audio thread.
  pl->morphInit();
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
  return kNumParams + (kNumOsc - 1) * perOscParamCount();
}

bool params_get_info(const clap_plugin_t *, uint32_t index, clap_param_info_t *info)
{
  uint32_t osc = 0;
  const ParamDef *dp = nullptr;
  if (index < kNumParams) { dp = &kParams[index]; }
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
  if (osc == 0)
    std::snprintf(info->name, sizeof(info->name), "%s", d.name);
  else
    std::snprintf(info->name, sizeof(info->name), "Osc%u %s", osc + 1, d.name);
  std::snprintf(info->module, sizeof(info->module), "%s",
                osc == 0 ? "" : (osc == 1 ? "Osc 2" : "Osc 3"));
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
  // ADR-112 A3: the morph field rides the session, not just the preset. The
  // fragment is opaque JSON on one line; the parser find()s its keys, so the
  // leading comma the fragment carries is harmless.
  blob += "morph=" + self(p)->morphJson() + "\n";
  // ADR-138: generic mod routes ride the session. Emitted ONLY when routes
  // exist, so a routeless patch's bytes are unchanged and every existing
  // state round-trip stays exactly what it was. Old builds ignore the key.
  {
    const std::string routes = self(p)->modRoutesChunk();
    if (!routes.empty()) blob += "modroutes=" + routes + "\n";
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
  // ADR-138: a load is a load — clear generic routes up front, so a state
  // saved before routes existed (no `modroutes=` key) loads route-free
  // instead of inheriting whatever the previous patch had. A present key
  // then replaces this empty set.
  pl->applyModRoutesChunk("");
  // B100: a chunk without the header predates it and is revision 1 by
  // definition; a present `engine_revision=` line below overrides this.
  pl->setEngineRevision(1);
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
          pl->enqueueParam((clap_id)(d.id + idOff), val, 0);
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
          pl->applyParam((clap_id)(d.id + idOff), val);
          known = true;
          break;
        }
      if (!known) continue;  // unknown/future keys ignored (state_check pins this)
    }
  }
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
extern "C" void hypersaw_debug_state(const clap_plugin_t *p, char *out, uint32_t cap)
{
  const std::string j = self(p)->stateJson();
  std::snprintf(out, cap, "%s", j.c_str());
}
extern "C" void hypersaw_debug_panic(const clap_plugin_t *p) { self(p)->panicWithDump(); }
extern "C" bool hypersaw_debug_exempt(const clap_plugin_t *p, uint32_t id) { return self(p)->morphToggleExempt((clap_id)id); }
extern "C" const char *hypersaw_debug_cornervals(const clap_plugin_t *p, int k)
{ static std::string j; j = self(p)->morphCornerValsJson(k); return j.c_str(); }
extern "C" const char *hypersaw_debug_ownersjson(const clap_plugin_t *p)
{ static std::string j; j = self(p)->morphOwnersJson(); return j.c_str(); }
extern "C" const char *hypersaw_debug_exemptjson(const clap_plugin_t *p)
{ static std::string j; j = self(p)->morphExemptJson(); return j.c_str(); }
extern "C" bool hypersaw_debug_apply(const clap_plugin_t *p, const char *json)
{
  return self(p)->applyStateJson(json ? json : "");
}
/* B100: the patch's pinned engine revision, and the opt-forward hook the
   patch-level GUI control will bind (Plugin::setEngineRevision — the GUI
   wiring is a follow-up; this is the shell side only). */
extern "C" int hypersaw_debug_engine_revision(const clap_plugin_t *p)
{
  return self(p)->engineRevision();
}
extern "C" void hypersaw_debug_set_engine_revision(const clap_plugin_t *p, int rev)
{
  self(p)->setEngineRevision(rev);
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
  hostIf.getScope = [pl](float *l, float *r, int n) {
    const uint32_t w = pl->scopePos.load(std::memory_order_acquire);
    for (int i = 0; i < n; i++)
    { const uint32_t k = (w - (uint32_t)n + (uint32_t)i) & 2047;
      l[i] = pl->scopeL[k]; r[i] = pl->scopeR[k]; }
  };
  hostIf.getParamsJson = [pl]() { return pl->paramsJson(); };
  hostIf.getDefaultsJson = [pl]() { return pl->defaultsJson(); };
  hostIf.getBendCurveJson = [pl]() { return pl->bendCurveJson(); };
  hostIf.getShapeWaveJson = [pl]() { return pl->shapeWaveJson(); };
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
  hostIf.setModWheel = [pl](double v) { pl->srcWheel = v < 0 ? 0 : (v > 1 ? 1 : v); };
  hostIf.modRemoveRoute = [pl](int i) { pl->mod.removeRoute(i); };
  hostIf.morphCornerValsJson = [pl](int k) { return pl->morphCornerValsJson(k); };
  hostIf.morphCornerApply = [pl](uint32_t k, const std::string &j) { return pl->cornerApply((int)k, j); };
  hostIf.setParam = [pl](uint32_t id, double v) { pl->enqueueParam(id, v, 0); };
  hostIf.gesture = [pl](uint32_t id, bool begin) { pl->enqueueParam(id, 0, begin ? 1 : 2); };
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
  hostIf.applyStateJson = [pl](const std::string &s) { return pl->applyStateJson(s); };
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
