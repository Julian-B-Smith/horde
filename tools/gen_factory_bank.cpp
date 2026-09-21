/*
 * gen_factory_bank — writes the docs/presets/factory tree from the bank held AS CODE
 * below (B130). Run it, commit what it wrote; the generated files ARE the bank.
 *
 *   gen_factory_bank <out-dir>     (the path is NOT guessed — pass it, the way
 *                                   statefix_check makes you pass the corpus)
 *
 * WHY A GENERATOR AND NOT HAND-WRITTEN JSON. A factory patch is a CURRENT-LAYOUT
 * blob: 325 parameter keys, four 224-entry corner arrays in morphIds order, the
 * B100 state header, the ADR-159 layout stamp and the ADR-152 flatten. Hand-typing
 * any of that would be hand-typing a derived order — exactly the promise-without-
 * an-oracle that ADR-159 was written about. Here the ONLY hand-authored data is a
 * table of {parameter id -> value}; every byte of every shipped file is produced by
 * the shell's own writers (`hypersaw_debug_state`, and for corner presets the
 * order and defaults the shell reports through `hypersaw_debug_cornervals`), so a
 * parameter-table change is absorbed by re-running this, not by 40 hand edits.
 *
 * The corner-preset array is built WITHOUT a second debug export: a fresh instance's
 * four corners hold exactly the per-slot defaults (morphInit, hypersaw_clap.cpp),
 * so `hypersaw_debug_cornervals(p, 0)` on a fresh plugin IS the shell's morphIds
 * order paired with the shell's defaults. The generator overwrites only the ids a
 * corner names and prints the array with the same "%.6g" the shell's own corner
 * writer uses, so a written corner file round-trips through cornerApply exactly.
 *
 * Every patch is authored through the HOST path — parameter events into
 * process() — not by poking state, so a value that a host could not reach could
 * not enter the bank either.
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <clap/clap.h>
#include <cstdlib>
#include "../src/hypersaw_clap_entry.h"
#include "../src/hypersaw_debug.h"

namespace fs = std::filesystem;

namespace
{
#include "notefuzz_scaffold.inc"

/* ---- the bank, as code ------------------------------------------------- */

struct PV
{
  clap_id id;
  double v;
};

/* A corner preset: a NAME (it ships as corners/<name>.json and every patch that
   uses it stamps that name into cornerNames) and the ids it moves off default. */
struct CornerDef
{
  const char *name;
  std::vector<PV> set;
};

struct PresetDef
{
  const char *category;
  const char *name;
  const char *teach;         // BANK.md line: what it shows, which knob first
  std::vector<PV> params;
  const char *corners[4];    // corner-preset names, or "" — empty row = no morph
};

/* The physics corners the morph exemplars are built from. These four are the
   coupling law in four postures, which is why they are corner PRESETS and not
   four inline parameter blobs: the same four files are what a player loads into
   a corner by hand.
     cloud       K 0 — nothing pulls. THREE voices, not sixteen.
     lock        K 1: one voice, phase-locked.
     splay       K -1: repulsion drives the phases onto an even lattice.
     two-cluster topo 2 with balance 1 — cluster A locked, cluster B splayed, the
                 one posture where RA and RB say something R cannot.

   WHY THE CLOUD IS THREE VOICES, and it is not a fudge. B130 asks the four
   corners' (R, RA, RB) signatures to differ pairwise by >= 0.4, and R alone
   cannot separate a cloud from a splay: both are incoherent, and measured, a
   K-0 16-voice cloud reads R 0.24 against the splay's 0.04. What separates them
   is the FINITE-SIZE FLOOR — an uncoupled swarm's order parameter is not 0, it
   is ~1/sqrt(n) (the resultant of n random phasors), while a splayed lattice is
   0 for every n. So the cloud corner is a SMALL cloud: n 3 puts its floor at
   ~0.51, mid-way between the splay's 0.04 and the lock's 0.97, and all six
   pairs clear 0.4 on a true K-0 cloud rather than on a partly-locked one.
   Corner-owned voice count is part of what quantum morph demonstrates anyway.
   Measured margins are thin (0.04 on cloud-vs-lock) — bank_check prints every
   number so a future engine change shows up as a moved measurement, not a
   mystery. The clean fix is a fifth observable: RN, the n-th order parameter,
   is 1 for an even lattice and ~1/sqrt(n) for a cloud, separates them outright,
   and is ALREADY in VizSnapshot — but B130 fixes hypersaw_debug_viz's signature
   at (R, RA, RB, n), so widening it is the lead's call, not this tool's. */
const std::vector<CornerDef> kCorners = {
    {"MISC - Cloud", {{1, 16}, {4, 0.55}, {6, 0.0}, {8, 0.35}, {39, 1.0}, {14, 1.0}}},   // B131: a FULL cloud; RN now tells it from the splay
    {"MISC - Lock", {{1, 16}, {4, 0.30}, {6, 1.0}, {8, 0.35}, {39, 1.0}, {14, 1.0}}},
    {"MISC - Splay", {{1, 16}, {4, 0.30}, {6, -1.0}, {8, 0.35}, {39, 1.0}, {14, 1.0}}},
    {"MISC - Two Cluster",
     {{1, 16}, {4, 0.30}, {6, 1.0}, {8, 0.35}, {24, 2}, {26, 0.6}, {56, 1.0}, {39, 1.0}, {14, 1.0}}},
};

/* Shorthand for the morph-exemplar corner row. */
#define QM_CORNERS {"MISC - Cloud", "MISC - Lock", "MISC - Splay", "MISC - Two Cluster"}
#define NO_CORNERS {"", "", "", ""}

const std::vector<PresetDef> kBank = {
    /* ---- init ----------------------------------------------------------
       B192 (human, 2026-09-21): "let's make sure there's a factory Init
       patch". The bank shipped 40 patches and four corner presets, and not one
       of them was the place a player goes to start over — the only way back to
       defaults was to close the instrument and open a new one.

       ITS PARAMETER TABLE IS EMPTY, AND THAT IS THE WHOLE POINT. The generator
       boots a fresh instance and saves what the shell itself holds, so this
       file is the shell's OWN defaults written by the shell's own writer. A
       hand-listed "init" table would be a second statement of every default,
       free to drift from the first one silently — the exact failure ADR-159
       was written about for the corner order. Re-run the generator after any
       default changes and this patch follows for nothing.

       It is not redundant with initState (hypersaw_clap.cpp): initState is the
       first act of a LOAD and is not reachable by a player, this is a patch a
       player can load. Loading it exercises initState too, so the two agree by
       construction rather than by care. */
    {"init", "INIT - Init",
     "The instrument as it opens: every parameter at its default, no morph corners authored, nothing routed. Load it to start over.",
     {},
     NO_CORNERS},

    /* ---- lead ---------------------------------------------------------- */
    {"lead", "LD - Hyper Lead",
     "The founding supersaw with the coupling switched on — Pull K is the knob: at 0 it is a detuned stack, at 1 it collapses into one voice.",
     {{1, 16}, {2, 1}, {4, 0.42}, {6, 0.18}, {8, 0.9}, {14, 1.1}, {17, 0.4}, {19, 0.004},
      {20, 0.35}, {21, 0.85}, {22, 0.25}, {39, 0.6}, {69, 0.18}, {71, 0.15}, {100, 0.9}},
     NO_CORNERS},
    {"lead", "LD - Knife Fifth",
     "Poles q = 2 locks the swarm into two antiphase groups, so the stack sounds a hollow fifth above itself — raise Poles q to hear the split.",
     {{1, 12}, {4, 0.5}, {6, 0.85}, {8, 0.5}, {28, 2}, {14, 0.9}, {19, 0.002}, {20, 0.3},
      {21, 0.8}, {22, 0.2}, {69, 0.35}, {71, 0.3}, {100, 0.85}},
     NO_CORNERS},
    {"lead", "LD - Glass Reed",
     "Saw Base and Roundness round the edge off every voice before they couple — sweep Roundness and the swarm goes from reed to glass.",
     {{1, 10}, {4, 0.33}, {6, 0.4}, {8, 0.7}, {14, 0.95}, {19, 0.006}, {20, 0.4}, {21, 0.9},
      {22, 0.3}, {129, 0.7}, {130, 0.45}, {131, 0.6}, {132, 0.4}, {100, 0.9}},
     NO_CORNERS},
    {"lead", "LD - Hoover",
     "Wide detune, slow Dissolve and a Drive slot: the swarm is still arguing while the note is loud. Dissolve is the knob — it sets how long the argument lasts.",
     {{1, 20}, {4, 0.72}, {6, 0.55}, {8, 3.2}, {9, 18}, {14, 1.2}, {19, 0.003}, {20, 0.6},
      {21, 0.9}, {22, 0.35}, {57, 1}, {58, 0.55}, {133, 0.8}, {100, 0.8}},
     NO_CORNERS},
    {"lead", "LD - Talk Lead",
     "Two fixed notches cut a vowel into the swarm — move FX1 Amount to move the vowel.",
     {{1, 14}, {4, 0.38}, {6, 0.3}, {8, 0.8}, {14, 1.0}, {19, 0.004}, {20, 0.35}, {21, 0.85},
      {22, 0.25}, {57, 6}, {58, 0.42}, {96, 0.65}, {133, 0.9}, {100, 0.9}},
     NO_CORNERS},

    /* ---- bass ---------------------------------------------------------- */
    {"bass", "BS - Sub Anchor",
     "Near-total coupling at the bottom of the keyboard: four voices that agree read as one fat sine-ish saw. Drop Pull K and the sub smears.",
     {{1, 4}, {4, 0.09}, {6, 0.92}, {8, 0.25}, {14, 0.2}, {19, 0.002}, {20, 0.25}, {21, 0.95},
      {22, 0.12}, {35, -1}, {40, 1}, {41, 140}, {100, 0.95}},
     NO_CORNERS},
    {"bass", "BS - Growl Bass",
     "A comb in the rack feeds the swarm's own beating back at it — FX1 Tone is the resonance.",
     {{1, 8}, {4, 0.3}, {6, 0.35}, {8, 0.6}, {14, 0.5}, {19, 0.002}, {20, 0.3}, {21, 0.9},
      {22, 0.14}, {35, -1}, {57, 5}, {58, 0.45}, {96, 0.72}, {133, 0.55}, {100, 0.85}},
     NO_CORNERS},
    {"bass", "BS - Reese",
     "Two voices, no coupling, wide: the classic beating bass is what this engine does when you tell it NOT to agree. Pull K is the anti-knob here.",
     {{1, 2}, {4, 0.26}, {6, 0.0}, {8, 4.0}, {14, 1.25}, {19, 0.002}, {20, 0.4}, {21, 0.95},
      {22, 0.18}, {35, -1}, {40, 1}, {100, 0.9}},
     NO_CORNERS},
    {"bass", "BS - Tight Stack",
     "Fast Dissolve: the swarm locks before the attack transient is over, so the note starts wide and lands narrow. Dissolve is the knob.",
     {{1, 10}, {4, 0.45}, {6, 1.0}, {8, 0.12}, {14, 0.4}, {19, 0.001}, {20, 0.18}, {21, 0.7},
      {22, 0.1}, {35, -1}, {69, 0.5}, {100, 0.95}},
     NO_CORNERS},
    {"bass", "BS - Wobble Frame",
     "Drift Depth walks every voice around its seat while the coupling pulls them back — the wobble is the fight. Drift Depth first.",
     {{1, 12}, {4, 0.2}, {6, 0.45}, {8, 1.4}, {9, 55}, {10, 0.22}, {14, 0.7}, {19, 0.003},
      {20, 0.4}, {21, 0.9}, {22, 0.2}, {35, -1}, {100, 0.85}},
     NO_CORNERS},

    /* ---- pad ----------------------------------------------------------- */
    {"pad", "PD - Slow Consensus",
     "A seven-second Dissolve: the pad is literally the swarm reaching agreement in real time. Play a long note and do not touch anything.",
     {{1, 24}, {4, 0.5}, {6, 0.45}, {8, 7.0}, {14, 1.15}, {19, 1.2}, {20, 2.0}, {21, 0.9},
      {22, 2.5}, {39, 0.8}, {100, 0.8}},
     NO_CORNERS},
    {"pad", "PD - Drift Choir",
     "Per-voice 1/f drift under a weak pull — voices wander, never far. Drift Rate sets how nervous the choir is.",
     {{1, 20}, {4, 0.44}, {6, 0.22}, {8, 3.0}, {9, 42}, {10, 0.18}, {14, 1.2}, {19, 0.8},
      {20, 1.5}, {21, 0.95}, {22, 2.0}, {39, 0.9}, {100, 0.8}},
     NO_CORNERS},
    {"pad", "PD - Glacier",
     "The Room slot behind a coupled pad: the swarm agrees, the room disagrees. FX1 Size is the knob.",
     {{1, 18}, {4, 0.36}, {6, 0.5}, {8, 4.0}, {14, 1.0}, {19, 1.5}, {20, 2.5}, {21, 0.9},
      {22, 3.0}, {57, 8}, {58, 0.6}, {200, 0.85}, {203, 0.35}, {133, 0.55}, {100, 0.8}},
     NO_CORNERS},
    {"pad", "PD - Breath Field",
     "Onset Scatter gives every voice its own entry time and Timing Correction lets them listen to each other — an ensemble breath, not a chord. Onset Scatter first.",
     {{1, 16}, {4, 0.4}, {6, 0.3}, {8, 2.5}, {14, 1.1}, {19, 0.6}, {20, 1.4}, {21, 0.9},
      {22, 1.8}, {91, 55}, {92, 0.4}, {93, 0.6}, {94, 1}, {95, 0.5}, {100, 0.8}},
     NO_CORNERS},
    {"pad", "PD - Stasis",
     "Inertia makes the swarm heavy: the pull is there, the voices barely move. Inertia is the knob — at 0 the same patch snaps shut.",
     {{1, 22}, {4, 0.48}, {6, 0.8}, {8, 2.0}, {11, 0.85}, {14, 1.1}, {19, 1.0}, {20, 2.0},
      {21, 0.95}, {22, 2.4}, {39, 0.7}, {100, 0.8}},
     NO_CORNERS},

    /* ---- pluck --------------------------------------------------------- */
    {"pluck", "PL - Porcelain",
     "Short envelope over a fast lock: you hear the swarm collapse inside the decay. Shorten Dissolve to hear it collapse sooner.",
     {{1, 12}, {4, 0.4}, {6, 0.9}, {8, 0.2}, {14, 0.8}, {19, 0.001}, {20, 0.22}, {21, 0.0},
      {22, 0.18}, {69, 0.2}, {100, 0.9}},
     NO_CORNERS},
    {"pluck", "PL - Koto Swarm",
     "Harmonic detune law: the voices are placed on the harmonic series instead of in cents, so the pluck rings as one string. Detune Law is the knob.",
     {{1, 9}, {4, 0.55}, {5, 4}, {6, 0.6}, {8, 0.3}, {14, 0.85}, {19, 0.001}, {20, 0.3},
      {21, 0.0}, {22, 0.25}, {79, 1.5}, {100, 0.9}},
     NO_CORNERS},
    {"pluck", "PL - Spark",
     "Tiny swarm, total coupling, 60 ms: the shortest way to hear what a lock sounds like. Raise Voices to blunt it.",
     {{1, 5}, {4, 0.3}, {6, 1.0}, {8, 0.08}, {14, 0.5}, {19, 0.001}, {20, 0.09}, {21, 0.0},
      {22, 0.07}, {71, 0.5}, {100, 0.95}},
     NO_CORNERS},
    {"pluck", "PL - Bell Lattice",
     "Poles q = 3 splits the swarm three ways and Stretch pushes the partials sharp — an inharmonic bell built out of coupling. Poles q first.",
     {{1, 15}, {4, 0.6}, {5, 5}, {6, 0.8}, {8, 0.45}, {28, 3}, {80, 2.5}, {14, 1.0},
      {19, 0.001}, {20, 0.9}, {21, 0.0}, {22, 0.8}, {100, 0.9}},
     NO_CORNERS},

    /* ---- keys ---------------------------------------------------------- */
    {"keys", "KY - Paper Rhodes",
     "Weak pull, dark tilt, a compressor on the end — the swarm supplies the tine's beat. Tone Tilt is the knob.",
     {{1, 8}, {4, 0.22}, {6, 0.25}, {8, 1.1}, {14, 0.6}, {19, 0.002}, {20, 0.9}, {21, 0.25},
      {22, 0.5}, {71, -0.55}, {72, 0.35}, {57, 4}, {58, 0.45}, {133, 0.7}, {100, 0.9}},
     NO_CORNERS},
    {"keys", "KY - Hymnal",
     "Consonance gravity on, held chords settle into just intonation while you hold them. Play a triad and wait — Gravity is the knob.",
     {{1, 7}, {4, 0.25}, {6, 0.4}, {8, 1.0}, {29, 0.7}, {30, 40}, {14, 0.9}, {19, 0.02},
      {20, 1.2}, {21, 0.8}, {22, 0.9}, {100, 0.85}},
     NO_CORNERS},
    {"keys", "KY - Clav Swarm",
     "Squareness at 1 with a tight lock: a square-ish clav whose body is 10 voices agreeing. Squareness is the knob.",
     {{1, 10}, {4, 0.34}, {6, 0.75}, {8, 0.25}, {14, 0.55}, {19, 0.001}, {20, 0.25},
      {21, 0.15}, {22, 0.2}, {69, 1.0}, {71, 0.35}, {100, 0.9}},
     NO_CORNERS},
    {"keys", "KY - Music Box",
     "Two octaves up, fast lock, long tail — the coupling is what keeps it from sounding like a detuned toy. Master Octave first.",
     {{1, 7}, {4, 0.28}, {6, 0.95}, {8, 0.15}, {14, 0.75}, {19, 0.001}, {20, 1.1}, {21, 0.0},
      {22, 1.0}, {103, 1}, {100, 0.85}},
     NO_CORNERS},

    /* ---- fx: one per module the rack ships today ------------------------ */
    {"fx", "FX - Drive Stack",
     "FX type Drive. A locked swarm has no beating left to hide clipping, so drive is where the lock becomes audible. FX1 Amount is the knob.",
     {{1, 12}, {4, 0.4}, {6, 0.7}, {8, 0.5}, {14, 0.9}, {20, 0.5}, {21, 0.85}, {22, 0.3},
      {57, 1}, {58, 0.7}, {133, 1.0}, {100, 0.7}},
     NO_CORNERS},
    {"fx", "FX - Filter Sweep",
     "FX type Filter. FX1 Amount is the cutoff — sweep it under a wide swarm and the detune spread becomes a moving formant.",
     {{1, 16}, {4, 0.55}, {6, 0.2}, {8, 1.5}, {14, 1.1}, {20, 0.6}, {21, 0.9}, {22, 0.35},
      {57, 2}, {58, 0.45}, {133, 1.0}, {100, 0.85}},
     NO_CORNERS},
    {"fx", "FX - Gain Trim",
     "FX type Gain. The rack's honest fader — 0.5 is unity. Use it to match a loud patch to a quiet one without touching Volume.",
     {{1, 10}, {4, 0.35}, {6, 0.5}, {8, 0.8}, {14, 0.9}, {20, 0.4}, {21, 0.85}, {22, 0.25},
      {57, 3}, {58, 0.62}, {133, 1.0}, {100, 0.85}},
     NO_CORNERS},
    {"fx", "FX - Comp Glue",
     "FX type Comp. A swarm's loudness moves as it locks; the compressor is what makes that a shape instead of a jump. FX1 Amount is the strength.",
     {{1, 14}, {4, 0.45}, {6, 0.6}, {8, 1.8}, {14, 1.0}, {20, 0.7}, {21, 0.9}, {22, 0.4},
      {57, 4}, {58, 0.6}, {133, 1.0}, {100, 0.85}},
     NO_CORNERS},
    {"fx", "FX - Comb Throat",
     "FX type Comb. FX1 Tone is the feedback — the comb's own resonance sits on top of the swarm's beating.",
     {{1, 12}, {4, 0.3}, {6, 0.4}, {8, 0.9}, {14, 0.8}, {20, 0.5}, {21, 0.85}, {22, 0.3},
      {57, 5}, {58, 0.5}, {96, 0.66}, {133, 0.6}, {100, 0.8}},
     NO_CORNERS},
    {"fx", "FX - Notch Phase",
     "FX type Notch. A static notch pair against a moving swarm — the phasing you hear is the swarm walking through the notch. FX1 Amount moves the notch.",
     {{1, 16}, {4, 0.5}, {6, 0.15}, {8, 2.0}, {14, 1.1}, {20, 0.6}, {21, 0.9}, {22, 0.35},
      {57, 6}, {58, 0.5}, {96, 0.55}, {133, 0.85}, {100, 0.85}},
     NO_CORNERS},
    {"fx", "FX - Echo Canyon",
     "FX type Echo. FX1 Size sets the space; the repeats are of a swarm that has already locked, so they stay legible.",
     {{1, 12}, {4, 0.38}, {6, 0.65}, {8, 0.7}, {14, 1.0}, {20, 0.45}, {21, 0.8}, {22, 0.3},
      {57, 7}, {58, 0.55}, {200, 0.6}, {203, 0.4}, {133, 0.5}, {100, 0.8}},
     NO_CORNERS},
    {"fx", "FX - Room Chamber",
     "FX type Room. FX1 Damping is the knob — a bright room re-excites the detune spread, a dark one buries it.",
     {{1, 14}, {4, 0.42}, {6, 0.5}, {8, 1.2}, {14, 1.05}, {19, 0.3}, {20, 1.0}, {21, 0.9},
      {22, 1.2}, {57, 8}, {58, 0.5}, {200, 0.7}, {203, 0.5}, {133, 0.45}, {100, 0.8}},
     NO_CORNERS},
    {"fx", "FX - Delay Dotted",
     "FX type Delay. Tempo-synced at a dotted eighth — D1 Beats is the knob, D1 Feedback the tail.",
     {{1, 12}, {4, 0.4}, {6, 0.55}, {8, 0.6}, {14, 1.0}, {20, 0.4}, {21, 0.8}, {22, 0.28},
      {57, 9}, {58, 0.5}, {233, 1}, {234, 0.75}, {236, 0.42}, {238, 0.4}, {133, 0.45},
      {100, 0.8}},
     NO_CORNERS},

    /* ---- morph --------------------------------------------------------- */
    {"morph", "MO - Quantum Morph",
     "THE morph demo: four corners that are four different physics — A a three-voice uncoupled cloud, B a sixteen-voice hard lock, C a repulsive splay, D two clusters. Drag the morph pad; in quantum mode each parameter flips to one corner's value, so the patch between corners is a patchwork, not an average.",
     {{1, 16}, {4, 0.30}, {8, 0.35}, {14, 1.0}, {19, 0.004}, {20, 0.8}, {21, 0.9}, {22, 0.4},
      {39, 1.0}, {100, 0.8}, {151, 1}, {152, 0.0}, {153, 0.0}, {154, 1.0}, {155, 0.3},
      {156, 1024}, {157, 0}, {158, 0.05}},
     QM_CORNERS},
    {"morph", "MO - Blend Drift",
     "The same four physics corners in BLEND mode with a 1.5 s morph glide — the average instead of the patchwork. Morph Mode is the knob: flip it to quantum and the same drag sounds completely different.",
     {{1, 16}, {4, 0.30}, {8, 0.35}, {14, 1.0}, {19, 0.004}, {20, 0.8}, {21, 0.9}, {22, 0.4},
      {39, 1.0}, {100, 0.8}, {151, 1}, {152, 0.0}, {153, 0.0}, {154, 1.0}, {155, 0.3},
      {156, 1024}, {157, 1}, {158, 1.5}},
     QM_CORNERS},
    {"morph", "MO - Two State",
     "One axis, two states: cloud on the left, lock on the right, top and bottom the same. Drag Morph X only — this is the cloud-to-lock sweep as a performance gesture.",
     {{1, 16}, {4, 0.30}, {8, 0.35}, {14, 1.0}, {19, 0.004}, {20, 0.8}, {21, 0.9}, {22, 0.4},
      {39, 1.0}, {100, 0.8}, {151, 1}, {152, 0.0}, {153, 0.0}, {154, 1.0}, {155, 0.3},
      {156, 1024}, {157, 1}, {158, 0.6}},
     {"MISC - Cloud", "MISC - Lock", "MISC - Cloud", "MISC - Lock"}},

    /* ---- demo: the coupling laws, each one on its own ------------------- */
    {"demo", "MISC - Cloud To Lock Sweep",
     "Pull K, and nothing else. At K 0 sixteen voices ignore each other and the order parameter sits down on its 1/sqrt(n) floor; at K 1 they are one voice inside a second. This is the whole thesis of the instrument in one knob.",
     {{1, 16}, {2, 0}, {4, 0.30}, {6, 0.0}, {8, 0.35}, {14, 1.0}, {17, 0.4}, {19, 0.002},
      {20, 2.0}, {21, 1.0}, {22, 0.3}, {39, 1.0}, {100, 0.85}},
     NO_CORNERS},
    {"demo", "MISC - Consonance Gravity Chord",
     "Gravity on, basin wide. Hold C4 and G4 — your keyboard's fifth is TEMPERED, 1.96 cents narrow of 3/2, and over about a second the two notes pull each other onto the just ratio. Gravity is the knob; at 0 the fifth stays tempered.",
     {{1, 3}, {4, 0.12}, {6, 0.3}, {8, 0.5}, {29, 1.0}, {30, 45}, {14, 0.6}, {19, 0.005},
      {20, 3.0}, {21, 1.0}, {22, 0.4}, {33, 0}, {100, 0.85}},
     NO_CORNERS},
    {"demo", "MISC - Splay Interference",
     "Pull K at -1: coupling reversed. The voices REPEL until they sit on an even phase lattice, gap 1/n, and the order parameter falls to zero — a texture you cannot get by detuning. Pull K is the knob; walk it back to 0 and the lattice dissolves.",
     {{1, 16}, {2, 0}, {4, 0.30}, {6, -1.0}, {8, 0.35}, {14, 1.0}, {17, 0.4}, {19, 0.002},
      {20, 2.0}, {21, 1.0}, {22, 0.3}, {39, 1.0}, {100, 0.85}},
     NO_CORNERS},
    {"demo", "MISC - Onset Scatter Ensemble",
     "Nobody starts together. Onset Scatter spreads the entries over 80 ms and Timing Correction decides whether the players pull back into line. Timing Correction is the knob — at 0 they never converge.",
     {{1, 16}, {4, 0.35}, {6, 0.3}, {8, 1.0}, {14, 1.0}, {19, 0.05}, {20, 1.5}, {21, 0.9},
      {22, 0.8}, {91, 80}, {92, 0.8}, {93, 0.7}, {94, 1}, {95, 0.6}, {100, 0.85}},
     NO_CORNERS},
    {"demo", "MISC - Tempo Grid Lattice",
     "Detune Law = tempo grid: the voices are not spread in cents, they are spread on rungs of the host tempo, so the swarm beats IN TIME. Grid Cycles/Beat is the knob.",
     {{1, 12}, {4, 0.6}, {5, 3}, {6, 0.25}, {8, 1.2}, {23, 2.0}, {14, 1.0}, {19, 0.004},
      {20, 1.2}, {21, 0.9}, {22, 0.5}, {100, 0.85}},
     NO_CORNERS},
};

/* ---- rig --------------------------------------------------------------- */

struct Rig
{
  const clap_plugin_t *p = nullptr;
  std::vector<float> L, R;
  clap_audio_buffer_t out{};
  clap_process_t proc{};
  float *ch[2];

  void boot()
  {
    auto *f = (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
    p = f->create_plugin(f, &kHost, "com.lifted-truck.hypersaw");
    p->init(p);
    p->activate(p, kSR, 32, kBlock);
    p->start_processing(p);
    L.assign(kBlock, 0);
    R.assign(kBlock, 0);
    ch[0] = L.data();
    ch[1] = R.data();
    out.data32 = ch;
    out.channel_count = 2;
    proc.frames_count = kBlock;
    proc.audio_outputs = &out;
    proc.audio_outputs_count = 1;
    proc.out_events = &kOut;
  }
  void step(EvList &e)
  {
    e.finalize();
    proc.in_events = &e.list;
    p->process(p, &proc);
  }
  void run(double sec)
  {
    for (int i = 0; i < Math_blocks(sec); i++) { EvList e; step(e); }
  }
  void send(const std::vector<PV> &pv)
  {
    EvList e;
    for (const auto &q : pv) e.params.push_back(mkParam(q.id, q.v));
    step(e);
  }
  void kill()
  {
    p->stop_processing(p);
    p->deactivate(p);
    p->destroy(p);
  }
};

/* ---- corner-preset authoring ------------------------------------------- */

/* The shell's morphIds order, paired with the shell's per-slot defaults, read
   off a FRESH instance (whose corners hold exactly those defaults — morphInit).
   Parsing `{"id":value,...}` positionally is safe because the writer emits the
   pairs IN morphIds order; that order is the corner file's contract. */
std::vector<PV> cornerOrder()
{
  Rig r;
  r.boot();
  const std::string j = hypersaw_debug_cornervals(r.p, 0);
  std::vector<PV> out;
  for (size_t i = 0; i < j.size();)
  {
    const size_t q0 = j.find('"', i);
    if (q0 == std::string::npos) break;
    const size_t q1 = j.find('"', q0 + 1);
    if (q1 == std::string::npos) break;
    const size_t colon = j.find(':', q1);
    if (colon == std::string::npos) break;
    out.push_back({(clap_id)std::strtoul(j.c_str() + q0 + 1, nullptr, 10),
                   std::strtod(j.c_str() + colon + 1, nullptr)});
    i = colon + 1;
  }
  r.kill();
  return out;
}

/* One corner preset as the shell's own cornerJson() would print it: layout 2,
   a positional array in morphIds order, "%.6g" per slot. */
std::string cornerPresetJson(const std::vector<PV> &order, const CornerDef &c, std::string &err)
{
  std::vector<double> vals;
  vals.reserve(order.size());
  for (const auto &slot : order) vals.push_back(slot.v);
  for (const auto &s : c.set)
  {
    bool hit = false;
    for (size_t i = 0; i < order.size(); i++)
      if (order[i].id == s.id) { vals[i] = s.v; hit = true; }
    if (!hit)
      err += std::string("corner '") + c.name + "': id " + std::to_string(s.id) +
             " is not in the morph field (global or non-morphable) — put it in the patch\n";
  }
  // 8 since B195 (the engine blocks' STRUCTURAL rows join the field, appended
  // after their block's morphable ones); the shell's cornerJson carries the
  // full ladder and is the only place the reasons are written out.
  std::string out = "{\"morphLayout\":8,\"cornerPreset\":[";
  char buf[32];
  for (size_t i = 0; i < vals.size(); i++)
  {
    std::snprintf(buf, sizeof buf, i ? ",%.6g" : "%.6g", vals[i]);
    out += buf;
  }
  return out + "]}";
}

const CornerDef *findCorner(const char *name)
{
  for (const auto &c : kCorners)
    if (!std::strcmp(c.name, name)) return &c;
  return nullptr;
}

bool writeText(const fs::path &path, const std::string &s)
{
  fs::create_directories(path.parent_path());
  FILE *f = std::fopen(path.string().c_str(), "wb");
  if (!f) return false;
  const bool ok = std::fwrite(s.data(), 1, s.size(), f) == s.size();
  std::fclose(f);
  return ok;
}

}  // namespace

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    std::fprintf(stderr, "usage: gen_factory_bank <out-dir>   (e.g. docs/presets/factory)\n");
    return 2;
  }
  const fs::path root = argv[1];

  // ---- corner presets -------------------------------------------------
  const std::vector<PV> order = cornerOrder();
  if (order.empty())
  {
    std::fprintf(stderr, "gen_factory_bank: the shell reported an empty morph order\n");
    return 1;
  }
  std::string err;
  std::vector<std::pair<std::string, std::string>> cornerFiles;   // name -> json
  for (const auto &c : kCorners)
  {
    const std::string j = cornerPresetJson(order, c, err);
    if (!writeText(root / "corners" / (std::string(c.name) + ".json"), j))
    { std::fprintf(stderr, "gen_factory_bank: cannot write corner '%s'\n", c.name); return 1; }
    cornerFiles.emplace_back(c.name, j);
  }
  if (!err.empty()) { std::fputs(err.c_str(), stderr); return 1; }
  std::printf("corners: %zu written (%zu slots each)\n", kCorners.size(), order.size());

  // ---- patches --------------------------------------------------------
  static char state[1 << 18];
  int written = 0;
  for (const auto &d : kBank)
  {
    Rig r;
    r.boot();
    /* morphOn LAST: with the field on, an unarmed parameter edit is routed to
       whichever corner owns it (ADR-109), so the patch's own live values have
       to be in place before the field is switched on. */
    std::vector<PV> live, late;
    for (const auto &q : d.params) (q.id == 151 ? late : live).push_back(q);
    r.send(live);
    r.run(0.05);

    bool anyCorner = false;
    for (int k = 0; k < 4; k++)
    {
      if (!d.corners[k] || !*d.corners[k]) continue;
      anyCorner = true;
      const CornerDef *c = findCorner(d.corners[k]);
      if (!c)
      { std::fprintf(stderr, "gen_factory_bank: %s names unknown corner '%s'\n", d.name, d.corners[k]); return 1; }
      std::string ignore;
      if (!hypersaw_debug_cornerapply(r.p, k, cornerPresetJson(order, *c, ignore).c_str()))
      { std::fprintf(stderr, "gen_factory_bank: cornerApply failed for %s corner %d\n", d.name, k); return 1; }
      hypersaw_debug_cornername(r.p, k, c->name);
    }
    if (!late.empty()) { r.send(late); }
    // Let the morph field resolve before the snapshot: with the field on, the
    // live values ARE the field's answer, and a patch saved before it resolved
    // would ship the pre-morph values under a morphOn flag.
    r.run(anyCorner || !late.empty() ? 0.5 : 0.1);

    hypersaw_debug_state(r.p, state, sizeof state);
    const fs::path out = root / d.category / (std::string(d.name) + ".json");
    if (!writeText(out, state))
    { std::fprintf(stderr, "gen_factory_bank: cannot write %s\n", out.string().c_str()); return 1; }
    r.kill();
    written++;
  }
  std::printf("patches: %d written\n", written);

  // ---- BANK.md --------------------------------------------------------
  /* Generated from the same table, so a teaching line and the patch it
     describes cannot drift apart (B103: the line is part of the product). */
  std::string md =
      "# The factory bank\n\n"
      "Generated by `tools/gen_factory_bank.cpp` — edit the table there, re-run it,\n"
      "commit what it writes. Every file is a current-layout patch captured through\n"
      "the shell's own state writer, so it carries the state header, `morphLayout 2`,\n"
      "`cornerNames` and the ADR-152 flatten and is self-contained.\n\n"
      "Each line says what the patch demonstrates and which knob to touch first.\n"
      "`tools/bank_check.cpp` asserts the bank loads, re-saves identically, makes\n"
      "sound, and that the named exemplars still do what their line claims.\n\n";
  // "init" comes first because it is where a player starts over, and it comes
  // first for FREE: kBank lists it first, and the order below is first-seen.
  // (This comment used to say "adding a category here is what makes it appear
  // in BANK.md at all" — true of the hand-listed array it sat above, and false
  // the moment that array went away.)
  /* CATEGORIES ARE DISCOVERED FROM kBank, NEVER HAND-LISTED (B192, 2026-09-21).
     `bank_check` carried exactly this array while CMake globs the whole tree, so
     the Init patch shipped, embedded and installed with ZERO check rows running
     on it. That hole was fixed in the check; leaving the same array here would
     have left the same defect with a smaller blast radius — an undocumented
     patch rather than an unchecked one — which is precisely how a class of bug
     survives its own fix. First-seen order, so BANK.md stays stable for a given
     table instead of reordering on a container's whim. */
  std::vector<std::string> cats;
  for (const auto &d : kBank)
    if (std::find(cats.begin(), cats.end(), d.category) == cats.end())
      cats.emplace_back(d.category);
  for (const std::string &catOwned : cats)
  {
    const char *cat = catOwned.c_str();
    md += std::string("## ") + cat + "\n\n";
    for (const auto &d : kBank)
    {
      if (std::strcmp(d.category, cat)) continue;
      md += std::string("- **") + d.name + "** — " + d.teach + "\n";
    }
    md += "\n";
  }
  md += "## corners/\n\nCorner presets the morph patches load into corners A–D. A patch\n"
        "stamps the name it loaded into `cornerNames`, so a corner always says where it\n"
        "came from.\n\n";
  for (const auto &c : kCorners) md += std::string("- **") + c.name + "**\n";
  md += "\n";
  if (!writeText(root / "BANK.md", md))
  { std::fprintf(stderr, "gen_factory_bank: cannot write BANK.md\n"); return 1; }

  hypersaw_entry_deinit();
  std::printf("gen_factory_bank: %d patches + %zu corners + BANK.md -> %s\n", written,
              kCorners.size(), root.string().c_str());
  return 0;
}
