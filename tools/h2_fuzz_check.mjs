/*
 * h2_fuzz_check.mjs — the parameter fuzz of every enrolled horde 2 engine.
 * ROADMAP B448 (Blind-Spot Armor) phase 2, package P3, ruled in ADR-209 and ADR-211.
 * The proposal is docs/strategy/blind-spot-armor-phase2.md: section 1 rows R6 and R3,
 * section 3 row P3. It is a loop over the engine harness (h2/harness/README.md).
 *
 * WIRED: ./verify full
 *
 * WHAT IT DOES. For each enrolled engine it makes seeded cases from the engine's
 * declared parameter table, renders each through the C++ engine (h2_harness_render,
 * never the JS one) and judges EVERY sample:
 *   - no output sample is non-finite (the count printed is R3's engine NaN count);
 *   - the pre-limiter bus stays under a bound. The engine ends in tanh, which would
 *     hide a blow-up, so the bus is read, not the output. On the limiter's rail the
 *     bus reads infinity, which means "at least 19" and is reported as a BLOW-UP,
 *     not as a non-finite sample;
 *   - after the last key-off the output falls silent in a stated time;
 *   - the engine refuses no key the table declares.
 *
 * THE CASES. Case i is a pure function of (table, seed, i):
 *   static   a drawn patch, 1 to 4 notes held 0.25 to 1 s, then released.
 *   sweep    the same, with ONE key moved in steps during the held note: end to end,
 *            back, or between two drawn values; an option list steps through every
 *            option.
 *   stack    the same, with 2 to 8 keys changed in the SAME block, at 1 to 3 moments.
 * The rate (44.1, 48 or 96 kHz), the host block size and the host seed are drawn per
 * case. A PREAMBLE comes first, so that reaching every extreme does not depend on
 * luck: every continuous key at its minimum, then at its maximum, then one case per
 * option index. After it the cases rotate sweep, stack, static, and the swept or
 * stacked key rotates through the table, so every key that can vary is swept once
 * and leads a stacked change once in every round of 3 x (keys) cases.
 *
 * THE VELOCITY BUDGET. A chord's velocities share one note's budget: each is a drawn
 * 0.1..1 divided by the note count. The reason is measured. The bus sums voices, and
 * the adapter cannot see above the rail. With full velocities on every note, 12000
 * cases (2026-10-11, seeds 46152 and 20261011, 6000 each) gave worst bus peaks of
 * 17.19 and 9.15, with nothing on the rail and nothing non-finite. The 17.19 is four
 * notes with gain stepped from 0 to 1, and it minimises to eleven items (nine swarm
 * members at zero detune, blade 2 on, gain 1) that read 15.67. It is LOUD, NOT
 * GROWING. Measured: held for 10 s the minimised case puts 2 samples on the rail;
 * held for 40 s at one sixteenth of the gain it reads a steady 0.6 with a burst to
 * about 1.2 every 5.3 s (peak 1.27), which is about 10 and 20 at full gain. So a
 * stable chord can sit ON the rail, where this tool could not tell it from a
 * blow-up, and no margin fits between 17.19 and 19. With the budget the summed
 * level is about one note's, so the bound has room. A single note still plays the
 * whole velocity range. What full-velocity chords do to the limiter is a loudness
 * question (the measurement suite, P9), and is not judged here.
 *
 * THE SAMPLER, AND WHERE IT DIFFERS FROM THE GAUNTLET'S. The proposal says "the
 * existing sampler (tools/patchspace/space.mjs, dependency_tree.json)". This tool
 * draws from the SAME declared table (dependency_tree.json: every key's range, taper,
 * options and default) with the gauntlet's scheme (one substream per key; broad is
 * uniform in taper position, edge lands on an end with probability 0.7), but it does
 * not import space.mjs or gauntlet.mjs. Three reasons, each a fact about those files:
 *   1. They evaluate lab text, so their first import puts the entry script under the
 *      lab sandbox (tools/labharness/sandbox_guard.mjs). The sandbox refuses a script
 *      with no profile row and forbids child processes, and this tool must start the
 *      render binary. h2/harness/gen_patches.mjs restates one rule for the same reason.
 *   2. Their position-to-value map does not reach every declared end. Measured
 *      2026-10-11 by running the lab's fromPos shapes at positions 0 and 1 over the
 *      table: the two `circ` rows (c, c2: 0..1) give frac(1) = 0, so their maximum
 *      is never drawn, and dissolve (log, 0.05..7.94) gives 7.940000000000001, one
 *      step ABOVE its declared maximum. The other 71 rows land exactly. Here an end
 *      is the table's own number, never a computed one.
 *   3. The gauntlet resets every key the dependency tree calls inactive to its
 *      default (a fixpoint), because it measures what a patch SOUNDS like. A host
 *      can send any key at any time, so for robustness that reset is a narrowed
 *      range. Here every key keeps its draw; where the tree is right the audio is
 *      the same, and where the C++ engine differs from the tree the fuzz sees it.
 * The taper shapes are restated (TAPERS) and only place the interior draws. A taper
 * or a kind this file does not know stops the run.
 *
 * EVERY KEY, COUNTED. The table is checked against the engine's own key list, read
 * from its source. Then, counted from the commands that were actually sent, the run
 * is RED if any key was never set, any continuous key was never sent its declared
 * minimum or its declared maximum, any option was never sent, or any key that can
 * vary was never swept or never led a stacked change. Hits per key are printed.
 *
 * THE CONTROLS, on every run (h2/harness/control_fuzz.h). The same cases go through
 * three adapters that each carry one planted fault in one corner of the same table:
 * a level that grows without bound, a NaN, and a tone that never stops. For each the
 * run is RED unless the fault is found, the failing cases are EXACTLY the cases that
 * enter the corner (worked out from each case by entersCorner, so a miss and a false
 * alarm both show), and the minimised case is the corner and nothing else. The same
 * cases through control/tone must report nothing, and a key no table declares must
 * be reported.
 *
 * A FAILURE is minimised (delta debugging over what the case has beyond the defaults:
 * 1-minimal) and printed as one line that replays it:
 *   node tools/h2_fuzz_check.mjs --replay '<json>'
 * and as a KEEP object to paste into tools/h2_fuzz_kept.json. Kept cases are replayed
 * first on every run and must read clean. A render process that dies is charged to
 * the one case that kills it again when replayed alone.
 *
 * TWO SIZES.
 *   PR-size  node tools/h2_fuzz_check.mjs --bin <h2_harness_render>      (./verify full)
 *            PR_ROUNDS rounds: every key swept and stacked that many times.
 *   deep     ... --deep                                                   (the nightly)
 *            DEEP_CASES cases, and the line `KIT-GATE cases=<n>`.
 *   --cases N sets the count (never below one full round: a run that cannot reach
 *   every key is refused, not judged). --seed N replaces the seed; it is printed.
 *   --measure prints the numbers the thresholds table is built from and judges nothing.
 *
 * RUN TIME, measured 2026-10-11 on the dev Mac (8 cores, 4 render processes; a
 * stopwatch held outside this tool, which reads no clock). PR-size, 1020 cases with
 * the controls and the minimising: 27 s on a nearly idle machine (95 s of CPU). An
 * earlier build of this tool took 1 min 47 s over its 1020 cases while the machine's
 * load average was about 40. At 26.5 ms a case the deep run's 20000 cases are about
 * 9 minutes here. A hosted 4-core runner is ASSUMED four to six times slower (not
 * measured: no nightly has run), which makes the deep run 35 to 53 minutes there.
 * The count is one number in tools/nightly/jobs.json, beside its floor.
 *
 * DETERMINISM. mulberry32 streams from the printed seed, nothing else. No clock is
 * read anywhere in this file, so no verdict can depend on time taken; --procs changes
 * how many render processes run side by side and, in the output, only the first
 * line, which states it (checked 2026-10-11: one process against four, same seed).
 *
 * WHAT IT DOES NOT COVER (each is printed or stated, none is hidden):
 *   - Engine keys with NO declared range (21 on 2026-10-11: bend, poly, grav, ...).
 *     The table lists them as fixed, so there is nothing to draw from. The run prints
 *     them as a HOLE every time. Declaring their ranges is the table owner's edit.
 *   - kCustom, the one string row: the table declares one value. It is set, never varied.
 *   - gain scales the bus, so at gain 0 (its declared minimum) the bus is zero and
 *     only a non-finite value can show.
 *   - The bus bound belongs to THESE cases: 1 to 4 notes whose velocities add up to
 *     at most 1, in the cell's poly 8. It says nothing about how loud a full chord is.
 *   - Above the rail the adapter is blind, so a level of 20 and a level of 1e10 both
 *     read "blow-up". A way to tell them apart without an engine accessor exists
 *     (replay the case with its gain divided down: the engine's last line is linear in
 *     gain) and is NOT built: it changes what "on the rail" means, which is a ruling.
 *   - Silence is judged where the tail is rendered to the end of the release: every
 *     case whose release is short, and every FULL_TAIL_EVERY-th case whatever its
 *     release. The rest render 0.25 s of release (finite and bounded are still
 *     judged) and are counted as not judged for silence.
 *   - Notes are plain: no hostile note events, no note churn (the soak, P6), no
 *     rate-by-block matrix (the invariance matrix, P4).
 */
import { spawn } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { availableParallelism, tmpdir } from 'node:os';
import { join } from 'node:path';
import { ROOT, binPath, enrolled, jobText, listAdapters, mtof, parseOutput } from '../h2/harness/harness.mjs';

/* ================================================================== THRESHOLDS
 * PROPOSED, not ratified (ADR-209 item 5). Every threshold this tool has is a row of
 * THRESHOLD_TABLE, and nothing else in the file is one. The human's rule (ADR-209
 * item 5): "measured worst plus a stated margin, in three bands (pass, fail, and a
 * grey band that goes to the human's ears)".
 *
 * MEASURED 2026-10-11 on darwin-arm64 (Apple clang, the Release h2_harness_render) with
 *   node tools/h2_fuzz_check.mjs --measure --cases 6000                    (seed 46152)
 *   node tools/h2_fuzz_check.mjs --measure --cases 6000 --seed 20261011
 * 12000 cases of `composed`, 7,968,665,758 samples, nothing on the rail, nothing
 * non-finite. Bus peak, worst: 4.62679 (case 3615) and 4.79601 (case 5024). Seconds
 * from the release law's end to under silentFloor, worst: 0.1808 (case 5425) and
 * 0.1823 (case 2343). The larger of each pair is the measured worst below.
 *
 * THE THREE BANDS.
 *   level     bus peak at or under prePass: pass. Above it and under the rail: GREY.
 *             On the rail: FAIL. The rail is not a number of this tool's: it is where
 *             the adapter stops seeing (h2/harness/README.md: 19 is under it, 19.2 on it).
 *   silence   under silentFloor from the release law's end + settlePass: pass. Under
 *             it only from + settleFail: GREY. Still above it then: FAIL.
 * A non-finite sample and an unknown key have no band. One is red.
 *
 * WHY THESE MARGINS (argued, for the human to change). Level x2 is 6 dB: the two
 * samples of 6000 differ by 4 %, and 9.6 still sits 6 dB under the rail. The tail after
 * a release is the engine's DC blocker ringing down (measured: 60 dB per 0.14 s), so
 * x1.5 on the time is about 40 dB more decay than the worst case needed, and x3 is
 * later than that decay can last from any level under the rail.
 */
const THRESHOLD_TABLE = [
  { name: 'prePass', value: 9.6, what: 'pre-limiter bus peak, the top of the pass band',
    worst: 'measured worst 4.79601 (seed 20261011, case 5024)', margin: 'x2 (6 dB)', date: '2026-10-11' },
  { name: 'preFail', value: 'the rail', what: 'the bus reads infinity: at least 19',
    worst: 'not this tool\'s: the adapter\'s limit of sight, h2/harness/README.md', margin: 'none', date: '2026-10-10' },
  { name: 'settlePass', value: 0.274, what: 'seconds after the release law\'s end by which the output is under silentFloor',
    worst: 'measured worst 0.1823 (seed 20261011, case 2343)', margin: 'x1.5', date: '2026-10-11' },
  { name: 'settleFail', value: 0.547, what: 'seconds after the release law\'s end past which a tail above silentFloor fails',
    worst: 'measured worst 0.1823 (the same)', margin: 'x3', date: '2026-10-11' },
  { name: 'silentFloor', value: Math.pow(10, -90 / 20), what: 'the output level called silent: -90 dBFS peak',
    worst: 'not measured here: the line tools/patchspace/metrics.mjs silence() already draws (-90 dBFS); the two settle rows are measured against it', margin: 'none', date: '2026-10-11' },
];
export const THRESHOLDS = Object.fromEntries(THRESHOLD_TABLE.filter(r => typeof r.value === 'number').map(r => [r.name, r.value]));

/* ================================================================== the run's shape
 * None of these is a tolerance: they say how many cases, how long and how often. */
const SEED = 0xB448;            // the default seed of both sizes; --seed replaces it and is printed
const PR_ROUNDS = 3;            // the PR-size run: every key swept and stacked this many times
const DEEP_CASES = 20000;       // the nightly's default; see "RUN TIME" in the header
const KEPT = 'tools/h2_fuzz_kept.json';
const SR_CHOICES = [44100, 48000, 48000, 48000, 96000];   // weights by repetition
const BLOCK_CHOICES = [32, 64, 128, 128, 512, { vary: 256 }];
const NOTE_LO = 24, NOTE_HI = 96;                         // C1..C7
const MAX_NOTES = 4;
const HOLD_MIN_S = 0.25, HOLD_OCTAVES = 2;                // hold = 0.25 * 2^(2u): 0.25..1 s
const MIN_SEGMENT = 512;        // frames between two changes: control_fuzz.h's blow-up needs 512 to reach the rail
const WINDOW_S = 0.05;          // silence is asserted over this long a stretch
const TAIL_CAP_S = 1.0;         // a tail longer than this is rendered in full only in every FULL_TAIL_EVERY-th case
const SHORT_TAIL_S = 0.25;      // otherwise this much release is rendered and silence is NOT asserted (counted)
const FULL_TAIL_EVERY = 8;      // case 1 (every continuous key at its maximum) is one of them
const EDGE_END = 0.7;           // edge mode: a continuous key lands on an end with this probability (the gauntlet's 0.7)
const EDGE_SHARE = 2 / 3;       // of the drawn base patches, this share are edge mode; the rest are broad
const CHUNK_FRAMES = 1 << 20;   // jobs are grouped into render processes of about this many frames
const MINIMISE_PER_ENGINE = 3;  // failing cases minimised per adapter (the rest are listed, not minimised)

/* ================================================================== what each engine declares
 * One entry per ENROLLED engine. An enrolled engine with no entry is red: the fuzz
 * cannot claim a key list it was not given.
 *   table     the declared parameter table: every key with its range, options and default
 *   mapped    rows of the table that are not engine keys, and what they send instead
 *   cell      sets after `snap` (the reference cell's: h2/harness/patches.json)
 *   keys      reads the engine's own key list out of its source, so a key the engine
 *             has and the table does not know is found, not assumed away
 *   release   seconds from the last key-off to a freed voice, from the values a case
 *             gave the release key. seen(key) is every value the case set.            */
const ENGINES = {
  composed: {
    table: 'tools/patchspace/dependency_tree.json',
    // The lab's switch for blade 1 (dependency_tree.json: "lab mapping: off => w 0"). Off sends
    // w 0; on sends the case's own w. h2/harness/gen_patches.mjs reads the row the same way: not an engine key.
    mapped: { b1on: (v, base) => [['w', v ? base.w : 0]] },
    cell: [['poly', 8]],
    keys() {
      const src = readFileSync(join(ROOT, 'h2/engine/engine.h'), 'utf8');
      const t = [...src.matchAll(/\{"([^"]+)", &SP::\w+, (?:true|false)\}/g)].map(m => m[1]);
      const d = [...src.matchAll(/\{"([^"]+)", &DP::\w+\}/g)].map(m => m[1]);
      const kT = /static constexpr int kTKeys = (\d+);/.exec(src);
      // a source that changed shape must stop the run, never be read short
      if (!kT || t.length !== Number(kT[1])) throw new Error(`h2_fuzz_check: h2/engine/engine.h: read ${t.length} target keys, kTKeys says ${kT ? kT[1] : '?'}`);
      if (d.length < 30) throw new Error(`h2_fuzz_check: h2/engine/engine.h: read only ${d.length} discrete keys; the key table changed shape`);
      for (const special of ['"os"', '"kCustom"']) if (!src.includes(`std::strcmp(key, ${special})`)) throw new Error(`h2_fuzz_check: h2/engine/engine.h no longer handles ${special} where set() did`);
      return [...t, ...d, 'os', 'kCustom'];
    },
    // engine.h: in release `env -= env * rC`, rC = 1 - exp(-4 / (R ms as samples)), and the
    // voice is freed below 1e-4. From an envelope of at most 1 that is R * ln(1e4) / 4.
    // A case's LARGEST R is used: the engine glides toward a new R, so a release that
    // starts just after R was lowered still runs at nearly the old one.
    release: seen => Math.max(...seen('R')) / 1000 * Math.log(1e4) / 4,
  },
};

/* ================================================================== seeded streams
 * mulberry32 and hash32 are the repo's own (tools/patchspace/space.mjs and
 * gen_dependency_tree.mjs), restated because those files evaluate lab text under the
 * lab sandbox, which forbids the child process this tool needs (harness.mjs says the
 * same of itself). */
export function mulberry32(a) {
  return function () {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
export function hash32(...xs) { let h = 0x811C9DC5; for (const x of xs) { h ^= x >>> 0; h = Math.imul(h, 0x01000193) >>> 0; h ^= h >>> 13; } return h >>> 0; }
const pick = (g, list) => list[Math.min(list.length - 1, Math.floor(g() * list.length))];

/* ================================================================== the declared space */
/* Taper position (0..1) -> a value strictly inside the range. The shapes are the lab's
   fromPos (docs/design/scalpel-interface-lab.html), restated: they only decide where
   the INTERIOR draws fall. The two ends are never computed: an end is the table's own
   number (see drawValue), so no rounding can move it. A taper this list does not have
   stops the run. */
const TAPERS = {
  lin: (r, p) => r.min + (r.max - r.min) * p,
  int: (r, p) => Math.round(r.min + (r.max - r.min) * p),
  log: (r, p) => r.min * Math.pow(r.max / r.min, p),
  pow: (r, p) => r.min + (r.max - r.min) * p * p,
  bpow: (r, p) => { const q = 2 * p - 1; return Math.sign(q) * r.max * q * q; },
  wlog: (r, p) => (p < 0.05 ? r.min : 0.005 * Math.pow(200, (p - 0.05) / 0.95)),
  circ: (r, p) => r.min + (r.max - r.min) * p,
};
const TAPER_NEEDS = { log: r => r.min > 0, bpow: r => r.min === -r.max, wlog: r => r.min === 0 && r.max === 1 };

export function loadSpace(engine) {
  const cfg = ENGINES[engine];
  if (!cfg) throw new Error(`h2_fuzz_check: enrolled engine "${engine}" has no entry in ENGINES (its declared parameter table)`);
  const tree = JSON.parse(readFileSync(join(ROOT, cfg.table), 'utf8'));
  const bad = (key, why) => { throw new Error(`h2_fuzz_check: ${cfg.table} "${key}": ${why}`); };
  const rows = Object.entries(tree.params).map(([key, p], index) => {
    const r = p.range || {}, row = { key, index, kind: p.kind, def: r.default, mapped: !!cfg.mapped[key] };
    if (p.kind === 'c') {
      Object.assign(row, { min: r.min, max: r.max, taper: r.taper });
      if (typeof r.min !== 'number' || typeof r.max !== 'number' || !(r.min < r.max)) bad(key, 'a continuous row with no min < max');
      if (!TAPERS[r.taper]) bad(key, `taper "${r.taper}" is not one this tool can draw`);
      if (TAPER_NEEDS[r.taper] && !TAPER_NEEDS[r.taper](r)) bad(key, `taper "${r.taper}" does not fit the range ${r.min}..${r.max}`);
      if (!(r.default >= r.min && r.default <= r.max)) bad(key, `default ${r.default} is outside ${r.min}..${r.max}`);
    } else if (p.kind === 'e') {
      row.opts = r.options;
      if (!Array.isArray(r.options) || r.options.length < 2 || !r.options.includes(r.default)) bad(key, 'an enumeration needs two or more options that include its default');
    } else if (p.kind === 't') {
      row.opts = [0, 1];
      if (r.default !== 0 && r.default !== 1) bad(key, 'a toggle needs a default of 0 or 1');
    } else if (p.kind === 's') {
      if (typeof r.default !== 'string') bad(key, 'a string row needs a string default');
    } else bad(key, `kind "${p.kind}" is not one this tool can draw`);
    return row;
  });
  const fuzz = rows.filter(r => r.kind !== 's');
  return { engine, cfg, rows, fuzz, byKey: Object.fromEntries(rows.map(r => [r.key, r])),
    unranged: (tree.unsampled || []).map(u => u.key),
    maxOpts: Math.max(...rows.filter(r => r.opts).map(r => r.opts.length)),
    defaults: Object.fromEntries(rows.map(r => [r.key, r.def])) };
}
/* The fewest cases that reach everything by construction: the preamble, then one sweep,
   one stack and one static case per key that can vary. */
export const preambleCases = space => 2 + space.maxOpts;
export const minCases = space => preambleCases(space) + 3 * space.fuzz.length;
export const prCases = space => preambleCases(space) + PR_ROUNDS * 3 * space.fuzz.length;

function interior(row, p) {
  const v = TAPERS[row.taper](row, p);
  // a rounding overshoot is put back ON the declared end, so every input stays in range
  return v < row.min ? row.min : v > row.max ? row.max : v;
}
/* One key's value from two uniform draws. broad: uniform in taper position. edge: a
   continuous key lands on a declared END with probability EDGE_END (either end, evenly).
   Enumerations and toggles are uniform over their options; a string keeps its default. */
function drawValue(row, mode, u, v) {
  if (row.kind === 's') return row.def;
  if (row.kind === 'c') return mode === 'edge' && u < EDGE_END ? (v < 0.5 ? row.min : row.max) : interior(row, v);
  return row.opts[Math.min(row.opts.length - 1, Math.floor(v * row.opts.length))];
}
/* A whole patch: one substream per row (the gauntlet's scheme), so a new row in the
   table moves no other row's draw. */
function drawPatch(space, cs, mode) {
  const V = {};
  for (const row of space.rows) { const g = mulberry32(hash32(cs, 1, row.index)); const u = g(), v = g(); V[row.key] = drawValue(row, mode, u, v); }
  return V;
}

/* ================================================================== a case
 * { i, kind, lead, sr, block, seed, notes [[note, vel]], hold (frames), base { key: value },
 *   events [{ at, sets [[key, value]] }], tail 'auto' | 'full' | 'short' }
 * A case is a pure function of (space, seed, i). Everything but the patch comes from
 * one stream, drawn in a fixed order. */
export function makeCase(space, seed, i) {
  const cs = hash32(seed, 2, i), g = mulberry32(hash32(cs, 3)), pre = preambleCases(space);
  const sr = pick(g, SR_CHOICES), block = pick(g, BLOCK_CHOICES);
  const hold = Math.round(HOLD_MIN_S * Math.pow(2, HOLD_OCTAVES * g()) * sr);
  const count = 1 + Math.floor(g() * MAX_NOTES), notes = [];
  // A chord's velocities SHARE one note's budget (each is a drawn 0.1..1 divided by the
  // note count). The bus sums voices and the rail is fixed, so without this a loud patch
  // under four full-velocity notes sits against the rail; see "THE VELOCITY BUDGET" above.
  while (notes.length < count) { const n = NOTE_LO + Math.floor(g() * (NOTE_HI - NOTE_LO + 1)); if (!notes.some(x => x[0] === n)) notes.push([n, (0.1 + 0.9 * g()) / count]); }
  const c = { i, kind: 'static', lead: null, sr, block, seed: hash32(cs, 4), notes, hold, base: null, events: [], tail: i % FULL_TAIL_EVERY === 1 ? 'full' : 'auto' };
  const mode = g() < EDGE_SHARE ? 'edge' : 'broad';

  if (i < pre) {
    // THE PREAMBLE: coverage by construction, so no key's extremes depend on luck.
    //   0  every continuous key at its declared minimum, every option list at its first
    //   1  every continuous key at its declared maximum, every option list at its last
    //   2+k  an edge patch with every enumeration at option k (mod its length)
    c.base = drawPatch(space, cs, 'edge'); c.mode = 'preamble';
    for (const r of space.rows) {
      if (r.kind === 's') continue;
      if (i === 0) c.base[r.key] = r.kind === 'c' ? r.min : r.opts[0];
      else if (i === 1) c.base[r.key] = r.kind === 'c' ? r.max : r.opts[r.opts.length - 1];
      else if (r.opts) c.base[r.key] = r.opts[(i - 2) % r.opts.length];
    }
    return c;
  }
  c.base = drawPatch(space, cs, mode); c.mode = mode;
  const j = i - pre, lead = space.fuzz[Math.floor(j / 3) % space.fuzz.length];
  c.kind = ['sweep', 'stack', 'static'][j % 3];
  if (c.kind === 'static') return c;
  c.lead = lead.key;
  if (c.kind === 'sweep') {
    // one key moved during the held note, in steps; the base value plays first
    let values;
    if (lead.kind === 'c') {
      const steps = 3 + Math.floor(g() * 6), how = Math.floor(g() * 3), pa = g(), pb = g();
      const a = how === 0 ? lead.min : how === 1 ? lead.max : interior(lead, pa);
      const b = how === 0 ? lead.max : how === 1 ? lead.min : interior(lead, pb);
      values = Array.from({ length: steps }, (_, k) => (k === 0 ? a : k === steps - 1 ? b : a + (b - a) * k / (steps - 1)));
      if (lead.taper === 'int') values = values.map(Math.round);
    } else {
      // every option once, starting anywhere (a toggle: both states, twice)
      const off = Math.floor(g() * lead.opts.length), turns = lead.kind === 't' ? 2 : 1;
      values = Array.from({ length: lead.opts.length * turns }, (_, k) => lead.opts[(off + k) % lead.opts.length]);
    }
    c.events = values.map((v, k) => ({ at: Math.round(hold * (k + 1) / (values.length + 1)), sets: [[lead.key, v]] }));
  } else {
    // several keys changed in the same block, at one to three moments
    const moments = 1 + Math.floor(g() * 3);
    for (let m = 0; m < moments; m++) {
      const n = 2 + Math.floor(g() * 7), rows = [lead];
      while (rows.length < n) { const r = pick(g, space.fuzz); if (!rows.includes(r)) rows.push(r); }
      c.events.push({ at: Math.round(hold * (m + 1) / (moments + 1)), sets: rows.map(r => [r.key, drawValue(r, 'edge', g(), g())]) });
    }
  }
  let last = 0;
  for (const e of [...c.events, { at: hold }]) { if (e.at - last < MIN_SEGMENT) throw new Error(`h2_fuzz_check: case ${i}: a segment of ${e.at - last} frames is under MIN_SEGMENT`); last = e.at; }
  return c;
}

/* ================================================================== a case as a harness job */
/* The commands, and what the judge needs: where the keys came off and from which frame
   the render must be silent (null when this case's tail is too short to say). */
export function build(space, c, opt) {
  const cfg = space.cfg, cmds = [];
  const setsOf = (key, value) => {
    const row = space.byKey[key];
    if (!row) throw new Error(`h2_fuzz_check: case ${c.i}: "${key}" is not in ${cfg.table}`);
    if (row.mapped) return cfg.mapped[key](value, c.base).map(([k, v]) => ['set', k, v]);
    return [[row.kind === 's' ? 'sets' : 'set', key, value]];
  };
  // every row of the table is sent, in table order; a mapped row goes last because it rewrites another row's key
  for (const row of space.rows) if (!row.mapped) cmds.push(...setsOf(row.key, c.base[row.key]));
  for (const row of space.rows) if (row.mapped) cmds.push(...setsOf(row.key, c.base[row.key]));
  cmds.push(['snap'], ...cfg.cell.map(([k, v]) => ['set', k, v]));
  for (const [n, vel] of c.notes) cmds.push(['on', n, mtof(n), vel]);
  let at = 0;
  for (const e of c.events) { cmds.push(['render', e.at - at]); at = e.at; for (const [k, v] of e.sets) cmds.push(...setsOf(k, v)); }
  cmds.push(['render', c.hold - at]);
  for (const [n] of c.notes) cmds.push(['off', n]);
  const seen = key => [c.base[key], ...c.events.flatMap(e => e.sets.filter(s => s[0] === key).map(s => s[1]))];
  const release = cfg.release(seen), window = Math.round(WINDOW_S * c.sr);
  // silence is owed from the release law's end plus settlePass; settleFail is the later line
  const passAt = Math.ceil((release + THRESHOLDS.settlePass) * c.sr), failAt = Math.ceil((release + THRESHOLDS.settleFail) * c.sr);
  let asserted = c.tail === 'full' || (c.tail === 'auto' && failAt + window <= Math.round(TAIL_CAP_S * c.sr));
  let tail = asserted ? failAt + window : Math.round(SHORT_TAIL_S * c.sr);
  // --measure: no verdict; render well past the release law and time the decay instead
  if (opt && opt.measure) { asserted = false; tail = Math.ceil((release + opt.measure) * c.sr); }
  cmds.push(['render', tail]);
  return { cmds, frames: c.hold + tail, off: c.hold, release, releaseFrames: release * c.sr,
    silenceFrom: asserted ? c.hold + passAt : null, lateFrom: asserted ? c.hold + failAt : null };
}
const jobOf = (adapter, c, b) => ({ name: `case ${c.i}`, engine: adapter, sr: c.sr, seed: c.seed,
  block: typeof c.block === 'object' ? c.block.vary : c.block, vary: typeof c.block === 'object', pre: true, cmds: b.cmds });

/* ================================================================== the judge
 * One render -> what was wrong with it. EVERY sample of both channels is read.
 *   non-finite   an output sample that is not a finite number, or a bus sample that
 *                is NaN under a finite output (an output beyond the limiter's range).
 *                The count is the engine NaN count of risk row R3.
 *   blow-up      the pre-limiter bus on the rail: it reads infinity under an output
 *                of exactly +-1, which means "at least 19" (h2/harness/README.md).
 *   not-silent   a sample above silentFloor at or after the release law's end plus
 *                settleFail.
 *   unknown-key  the engine refused a key the table declares.
 * GREY (reported with its replay line, never red): a finite bus peak above prePass,
 * or a tail that is above silentFloor after settlePass and under it by settleFail. */
const MEASURE_FLOORS = [0, 1e-9, 1e-6, Math.pow(10, -90 / 20), 1e-3];
export function judge(r, b, opt) {
  if (r.crash) return { classes: ['crash'], grey: [], crash: r.crash, nonFinite: 0, preNaN: 0, rail: 0, prePeak: 0, tailPeak: null, latePeak: null, samples: 0 };
  if (r.L.length !== b.frames) throw new Error(`h2_fuzz_check: ${r.name}: rendered ${r.L.length} frames, asked for ${b.frames}`);
  if (!r.preL) throw new Error(`h2_fuzz_check: ${r.name}: the render carries no pre-limiter bus`);
  const n = b.frames, v = { classes: [], grey: [], nonFinite: 0, preNaN: 0, rail: 0, prePeak: 0, tailPeak: null, latePeak: null, samples: 2 * n,
    unknownKeys: r.summary.unknownKeys, firstUnknown: r.summary.firstUnknown };
  for (const [y, p] of [[r.L, r.preL], [r.R, r.preR]]) for (let k = 0; k < n; k++) {
    if (!Number.isFinite(y[k])) { v.nonFinite++; continue; }
    const a = Math.abs(p[k]);
    if (a !== a) v.preNaN++;
    else if (a === Infinity) v.rail++;
    else if (a > v.prePeak) v.prePeak = a;
  }
  if (b.silenceFrom !== null) {
    // a NaN compares false and drops out of these two maxima; the loop above has already counted it
    v.tailPeak = v.latePeak = 0;
    for (const y of [r.L, r.R]) for (let k = b.silenceFrom; k < n; k++) {
      const a = Math.abs(y[k]);
      if (a > v.tailPeak) v.tailPeak = a;
      if (k >= b.lateFrom && a > v.latePeak) v.latePeak = a;
    }
  }
  if (opt && opt.measure) {
    // seconds after the release law's end at which the output was last above each floor (negative: it was already under)
    v.decay = MEASURE_FLOORS.map(f => { let last = b.off - 1; for (const y of [r.L, r.R]) for (let k = n - 1; k > last; k--) if (!(Math.abs(y[k]) <= f)) { last = k; break; } return (last + 1 - b.off - b.releaseFrames) / r.sr; });
    v.endPeak = Math.max(Math.abs(r.L[n - 1]), Math.abs(r.R[n - 1]));
  }
  const T = THRESHOLDS;
  if (v.nonFinite || v.preNaN) v.classes.push('non-finite');
  if (v.rail) v.classes.push('blow-up');
  else if (v.prePeak > T.prePass) v.grey.push('level');
  if (v.latePeak !== null && v.latePeak > T.silentFloor) v.classes.push('not-silent');
  else if (v.tailPeak !== null && v.tailPeak > T.silentFloor) v.grey.push('tail');
  if (v.unknownKeys) v.classes.push('unknown-key');
  return v;
}

/* ================================================================== rendering, several processes at once
 * harness.mjs's run() is synchronous; this is the same exchange (its jobText and
 * parseOutput) through spawn, so a few render processes work side by side. The
 * verdicts are filed by case index, so the number of processes changes how long the
 * run takes and nothing else. */
function renderBatch(jobs, bin) {
  return new Promise((resolve, reject) => {
    const dir = mkdtempSync(join(tmpdir(), 'h2fuzz-'));
    const done = (err, val) => { rmSync(dir, { recursive: true, force: true }); if (err) reject(err); else resolve(val); };
    const jf = join(dir, 'jobs.txt'), of = join(dir, 'out.bin');
    writeFileSync(jf, jobText(jobs));
    const p = spawn(bin, ['--jobs', jf, '--out', of], { stdio: ['ignore', 'ignore', 'pipe'] });
    let err = '';
    p.stderr.on('data', d => { err += d; });
    p.on('error', e => done(new Error(`h2_fuzz_check: could not start ${bin}: ${e.message}`)));
    p.on('close', (code, signal) => {
      // killed by a signal: the ENGINE fell over, which is a finding about a case, not a broken request
      if (code === null) return done(Object.assign(new Error(`h2_harness_render died on ${signal}`), { crash: signal }));
      if (code !== 0) return done(new Error(`h2_fuzz_check: h2_harness_render exited ${code}: ${err.trim()}`));
      try { done(null, parseOutput(readFileSync(of))); } catch (e) { done(e); }
    });
  });
}
async function mapPool(items, procs, fn) {
  const out = new Array(items.length);
  let next = 0;
  await Promise.all(Array.from({ length: Math.max(1, Math.min(procs, items.length)) }, async () => {
    for (;;) { const k = next++; if (k >= items.length) return; out[k] = await fn(items[k], k); }
  }));
  return out;
}
/* Renders and judges `cases` on one adapter; returns the verdicts in case order. A
   process that dies takes its whole batch with it, so the batch is replayed one job
   at a time and only the job that dies again is charged with the crash. */
export async function evaluate(ctx, adapter, space, cases, opt) {
  const built = cases.map(c => build(space, c, opt)), batches = [];
  let cur = [], frames = 0;
  built.forEach((b, k) => {
    if (cur.length && frames + b.frames > CHUNK_FRAMES) { batches.push(cur); cur = []; frames = 0; }
    cur.push(k); frames += b.frames;
  });
  if (cur.length) batches.push(cur);
  const verdicts = new Array(cases.length);
  await mapPool(batches, ctx.procs, async idx => {
    const jobs = idx.map(k => jobOf(adapter, cases[k], built[k]));
    let results;
    try { results = await renderBatch(jobs, ctx.bin); } catch (e) {
      if (!e.crash) throw e;
      results = [];
      for (const j of jobs) {
        try { results.push((await renderBatch([j], ctx.bin))[0]); } catch (e1) { if (!e1.crash) throw e1; results.push({ crash: e1.crash, name: j.name }); }
      }
    }
    idx.forEach((k, m) => { verdicts[k] = judge(results[m], built[k], opt); });
  });
  ctx.renders += cases.length;
  return { verdicts, built };
}

/* ================================================================== what the run reached
 * Counted from the COMMANDS that were sent, not from the plan that made them: a
 * sampler that quietly narrowed a range would still say it drew the full one. */
export function coverage(space, cases, built, st) {
  st = st || Object.fromEntries(space.rows.map(r => [r.key, { sets: 0, away: 0, min: 0, max: 0, sweeps: 0, stacks: 0,
    opts: r.opts ? new Map(r.opts.map(o => [o, 0])) : null }]));
  cases.forEach((c, n) => {
    const vals = new Map(space.rows.map(r => [r.key, []]));
    for (const cmd of built[n].cmds) if ((cmd[0] === 'set' || cmd[0] === 'sets') && vals.has(cmd[1])) vals.get(cmd[1]).push(cmd[2]);
    // a mapped row is not a command; its own values are the case's
    for (const r of space.rows) if (r.mapped) vals.set(r.key, [c.base[r.key], ...c.events.flatMap(e => e.sets.filter(s => s[0] === r.key).map(s => s[1]))]);
    for (const r of space.rows) {
      const s = st[r.key], vs = vals.get(r.key);
      s.sets += vs.length;
      if (vs.some(x => x !== r.def)) s.away++;
      if (r.kind === 'c') { if (vs.includes(r.min)) s.min++; if (vs.includes(r.max)) s.max++; }
      if (r.opts) for (const o of new Set(vs)) if (s.opts.has(o)) s.opts.set(o, s.opts.get(o) + 1);
    }
    if (c.kind === 'sweep') st[c.lead].sweeps++;
    if (c.kind === 'stack') st[c.lead].stacks++;
  });
  return st;
}

/* ================================================================== the planted faults
 * h2/harness/control_fuzz.h. The corners are written in the table of CONTROL_ENGINE,
 * and the plants are run over that engine's own cases. `when` is when the corner
 * counts: while a note is held, or at the moment the keys come off. */
const CONTROL_ENGINE = 'composed';
const CLEAN = 'control/tone';
const PLANTS = [
  { id: 'control/fuzz-blowup', expect: 'blow-up', when: 'held', corner: { fb: 'max', K: 'min' } },
  { id: 'control/fuzz-nan', expect: 'non-finite', when: 'held', corner: { detune: 'max', S: 'min', aa: 0 } },
  { id: 'control/fuzz-hang', expect: 'not-silent', when: 'release', corner: { R: 'min', A: 'max' } },
];
const atEnd = (row, end, v) => (end === 'max' ? v >= row.max : end === 'min' ? v <= row.min : v === end);
const cornerText = (space, plant) => Object.entries(plant.corner).map(([k, e]) => `${k} = ${e === 'max' ? space.byKey[k].max : e === 'min' ? space.byKey[k].min : e}${typeof e === 'string' ? ` (its ${e})` : ''}`).join(', ');
/* Does this case put the plant in its corner? Worked out from the case alone, by
   walking its changes, so the fuzz's verdict on a plant is checked both ways: every
   case that enters the corner must fail, and no other case may. */
export function entersCorner(space, plant, c, b) {
  const cur = Object.assign({}, c.base);
  const at = () => Object.entries(plant.corner).every(([k, e]) => atEnd(space.byKey[k], e, cur[k]));
  let held = at();
  for (const e of c.events) { for (const [k, v] of e.sets) cur[k] = v; if (at()) held = true; }
  // a hang shows only where the tail is long enough for silence to be owed
  return plant.when === 'held' ? held : at() && b.silenceFrom !== null;
}

/* ================================================================== minimising a failure
 * What a case has beyond the defaults is a list of ITEMS: a key whose starting value is
 * not the default, or one change made during the note. Delta debugging (ddmin) drops
 * items while the same class of failure remains, and stops when no single item can
 * be dropped: 1-minimal, which is the fewest parameters away from default unless two
 * items only matter together with a third. Then the notes are cut to one if one is
 * enough. Every candidate is a real render. */
function itemsOf(space, c) {
  const items = [];
  for (const r of space.rows) if (c.base[r.key] !== r.def) items.push({ base: r.key });
  c.events.forEach((e, n) => e.sets.forEach((_, m) => items.push({ ev: n, set: m })));
  return items;
}
function withItems(space, c, items, tail, notes) {
  const base = Object.assign({}, space.defaults);
  for (const it of items) if (it.base !== undefined) base[it.base] = c.base[it.base];
  const events = c.events.map((e, n) => ({ at: e.at, sets: e.sets.filter((_, m) => items.some(it => it.ev === n && it.set === m)) })).filter(e => e.sets.length);
  return Object.assign({}, c, { base, events, tail, notes: notes || c.notes });
}
const CLASS_ORDER = ['crash', 'non-finite', 'blow-up', 'not-silent', 'unknown-key'];
export async function minimise(ctx, adapter, space, c, v0) {
  // what must remain: the first failing class, or for a grey case the same grey reading
  const cls = CLASS_ORDER.find(k => v0.classes.includes(k)) || `grey ${v0.grey[0]}`;
  const same = v => (cls.startsWith('grey ') ? !v.classes.length && v.grey.includes(v0.grey[0]) : v.classes.includes(cls));
  // silence needs the whole tail; every other reading shows without it
  const tail = cls === 'not-silent' || cls === 'grey tail' ? 'full' : 'short';
  const fails = async (lists, notes) => (await evaluate(ctx, adapter, space, lists.map(items => withItems(space, c, items, tail, notes)))).verdicts.map(same);
  let items = itemsOf(space, c);
  const [whole, none] = await fails([items, []]);
  if (!whole) return { c, cls, minimised: false };   // it fails only with its own tail plan: keep it as found
  if (none) items = [];
  for (let n = 2; items.length >= 2;) {
    const size = Math.ceil(items.length / n), parts = [];
    for (let k = 0; k < items.length; k += size) parts.push(items.slice(k, k + size));
    const cands = parts.length > 2 ? [...parts, ...parts.map((_, k) => parts.filter((__, m) => m !== k).flat())] : parts;
    const hit = (await fails(cands)).indexOf(true);
    if (hit >= 0) { items = cands[hit]; n = hit < parts.length ? 2 : Math.max(n - 1, 2); }
    else if (size === 1) break;
    else n = Math.min(items.length, n * 2);
  }
  let notes = c.notes;
  if (notes.length > 1) {
    const one = notes.map(x => [x]), res = [];
    for (const x of one) res.push((await fails([items], x))[0]);
    if (res.includes(true)) notes = one[res.indexOf(true)];
  }
  return { c: withItems(space, c, items, tail, notes), cls, minimised: true, items: items.length };
}

/* ================================================================== one line that replays a case */
export function encode(space, adapter, c) {
  const sets = {};
  for (const r of space.rows) if (c.base[r.key] !== r.def) sets[r.key] = c.base[r.key];
  return { v: 1, engine: adapter, table: space.engine, sr: c.sr, block: c.block, seed: c.seed, notes: c.notes, hold: c.hold, tail: c.tail,
    sets, events: c.events.map(e => [e.at, e.sets]) };
}
export function decode(o) {
  if (!o || o.v !== 1) throw new Error('h2_fuzz_check: not a version 1 replay line');
  const space = loadSpace(o.table);
  const c = { i: 'replay', kind: 'replay', lead: null, sr: o.sr, block: o.block, seed: o.seed >>> 0, notes: o.notes, hold: o.hold, tail: o.tail,
    base: Object.assign({}, space.defaults, o.sets), events: (o.events || []).map(([at, sets]) => ({ at, sets })) };
  return { adapter: o.engine, space, c };
}
const replayLine = (space, adapter, c) => `node tools/h2_fuzz_check.mjs --replay '${JSON.stringify(encode(space, adapter, c))}'`;
const diffText = (space, c) => {
  const d = space.rows.filter(r => c.base[r.key] !== r.def).map(r => `${r.key} = ${c.base[r.key]}`);
  const e = c.events.map(ev => `at frame ${ev.at}: ${ev.sets.map(([k, v]) => `${k} = ${v}`).join(', ')}`);
  return `${d.length ? d.join(', ') : 'every key at its default'}${e.length ? '; then ' + e.join('; ') : ''}`;
};

/* ================================================================== tallies */
const SLAB = 256;                 // cases made, rendered and judged at a time, so a deep run's memory does not grow
const KEEP_FAILS = 40;            // failing cases held for the report (all are counted)
const MEASURE_EXTRA_S = 1.5;      // --measure renders this far past the release law
const FLOOR_INDEX = MEASURE_FLOORS.indexOf(THRESHOLDS.silentFloor);
const newTally = () => ({ cases: 0, kinds: { static: 0, sweep: 0, stack: 0 }, samples: 0, nonFinite: 0, preNaN: 0, railCases: 0,
  pre: { v: 0, i: null }, asserted: 0, longest: 0, longestAsserted: 0, tail: { v: 0, i: null }, decay: { v: -Infinity, i: null },
  grey: [], greyCount: 0, fails: [], failCount: 0 });
function tally(t, cases, built, verdicts) {
  cases.forEach((c, k) => {
    const v = verdicts[k], b = built[k];
    t.cases++; t.kinds[c.kind]++; t.samples += v.samples; t.nonFinite += v.nonFinite; t.preNaN += v.preNaN;
    if (v.rail) t.railCases++;
    if (v.prePeak > t.pre.v) t.pre = { v: v.prePeak, i: c.i };
    if (b.release > t.longest) t.longest = b.release;
    if (b.silenceFrom !== null) {
      t.asserted++;
      if (b.release > t.longestAsserted) t.longestAsserted = b.release;
      if (v.tailPeak > t.tail.v) t.tail = { v: v.tailPeak, i: c.i };
    }
    if (v.decay && v.decay[FLOOR_INDEX] > t.decay.v) t.decay = { v: v.decay[FLOOR_INDEX], i: c.i };
    if (v.grey.length && t.greyCount++ < KEEP_FAILS) t.grey.push({ c, v });
    if (v.classes.length && t.failCount++ < KEEP_FAILS) t.fails.push({ c, v });
  });
}
const verdictText = v => (v.crash ? `the render process died on ${v.crash}` :
  [v.nonFinite ? `${v.nonFinite} non-finite output samples` : '', v.preNaN ? `${v.preNaN} bus samples unreadable` : '',
    v.rail ? `${v.rail} bus samples on the rail (at least 19)` : '', `bus peak ${v.prePeak.toPrecision(4)}`,
    v.latePeak !== null ? `tail peak ${v.tailPeak.toExponential(2)}, late ${v.latePeak.toExponential(2)}` : 'tail not judged',
    v.unknownKeys ? `${v.unknownKeys} unknown keys, first "${v.firstUnknown}"` : ''].filter(Boolean).join('; '));

/* ================================================================== the key report */
function keyReport(space, st, say, problems) {
  const cfg = space.cfg, engineKeys = cfg.keys(), have = new Set(engineKeys);
  const table = new Set(space.rows.filter(r => !r.mapped).map(r => r.key)), unranged = new Set(space.unranged);
  // the engine's own key list against the table: nothing assumed on either side
  const ghost = [...table].filter(k => !have.has(k)), unknown = engineKeys.filter(k => !table.has(k) && !unranged.has(k));
  const hole = engineKeys.filter(k => !table.has(k));
  if (ghost.length) problems.push(`[${space.engine}] the table declares keys the engine does not have: ${ghost.join(', ')}`);
  if (unknown.length) problems.push(`[${space.engine}] the engine has keys the table does not know at all: ${unknown.join(', ')}`);
  const never = [], noMin = [], noMax = [], noOpt = [], noSweep = [], noStack = [];
  for (const r of space.rows) {
    const s = st[r.key];
    if (!s.sets) never.push(r.key);
    if (r.kind === 'c') { if (!s.min) noMin.push(r.key); if (!s.max) noMax.push(r.key); }
    if (r.opts) for (const [o, n] of s.opts) if (!n) noOpt.push(`${r.key}=${o}`);
    if (r.kind !== 's') { if (!s.sweeps) noSweep.push(r.key); if (!s.stacks) noStack.push(r.key); }
  }
  for (const [list, what] of [[never, 'were never set'], [noMin, 'were never sent their declared minimum'], [noMax, 'were never sent their declared maximum'],
    [noOpt, 'are options never sent'], [noSweep, 'were never swept during a note'], [noStack, 'were never in a stacked change']])
    if (list.length) problems.push(`[${space.engine}] KEYS: ${list.length} ${what}: ${list.join(', ')}`);
  const cont = space.rows.filter(r => r.kind === 'c'), lists = space.rows.filter(r => r.opts), strings = space.rows.filter(r => r.kind === 's');
  const mapped = space.rows.filter(r => r.mapped);
  say(`[${space.engine}] KEYS  ${space.rows.length} rows in ${cfg.table}: ${table.size} engine keys${mapped.length ? ` and ${mapped.length} mapped (${mapped.map(r => r.key).join(', ')})` : ''}. ` +
    `Set at least once: ${space.rows.length - never.length} of ${space.rows.length}. Hits per key (set commands sent):`);
  const cells = space.rows.map(r => `${r.key}=${st[r.key].sets}`);
  for (let k = 0; k < cells.length; k += 8) say('    ' + cells.slice(k, k + 8).join('  '));
  const low = [...space.rows].sort((a, b) => st[a.key].sets - st[b.key].sets || a.index - b.index).slice(0, 5);
  say(`[${space.engine}] KEYS  lowest five: ${low.map(r => `${r.key} ${st[r.key].sets}`).join(', ')}`);
  const fewest = (rows, f) => rows.reduce((m, r) => (m === null || f(r) < f(m) ? r : m), null);
  const lowMin = fewest(cont, r => st[r.key].min), lowMax = fewest(cont, r => st[r.key].max);
  const optCount = lists.reduce((n, r) => n + r.opts.length, 0);
  let lowOpt = null;
  for (const r of lists) for (const [o, n] of st[r.key].opts) if (!lowOpt || n < lowOpt.n) lowOpt = { key: r.key, o, n };
  say(`[${space.engine}] EXTREMES  ${cont.length - noMin.length} of ${cont.length} continuous keys were sent their declared minimum (fewest: ${lowMin.key} in ${st[lowMin.key].min} cases) ` +
    `and ${cont.length - noMax.length} of ${cont.length} their declared maximum (fewest: ${lowMax.key} in ${st[lowMax.key].max} cases); ` +
    `${optCount - noOpt.length} of ${optCount} options of ${lists.length} option lists were sent (fewest: ${lowOpt.key}=${lowOpt.o} in ${lowOpt.n} cases)`);
  const vary = space.fuzz.length;
  say(`[${space.engine}] CHANGES  swept during a held note: ${vary - noSweep.length} of ${vary} keys; in a stacked change as its lead key: ${vary - noStack.length} of ${vary}` +
    (strings.length ? `. NOT VARIED: ${strings.map(r => r.key).join(', ')} (a string row: the table declares one value, its default)` : ''));
  if (hole.length) say(`[${space.engine}] HOLE  ${hole.length} engine keys have NO declared range, so they were not fuzzed (the table lists them as fixed): ${hole.join(', ')}`);
  return { keys: space.rows.length, engineKeys: engineKeys.length, hole };
}

/* ================================================================== the run */
function usage(why) {
  console.error(`h2_fuzz_check: ${why}\nusage: node tools/h2_fuzz_check.mjs [--bin PATH] [--seed N] [--deep] [--cases N] [--procs N] [--measure] [--replay '<json>']`);
  process.exit(2);
}
function parseArgs(argv) {
  const o = { bin: undefined, seed: SEED, cases: null, deep: false, procs: Math.max(1, Math.min(4, availableParallelism())), measure: false, replay: null };
  for (let k = 0; k < argv.length; k++) {
    const a = argv[k], next = () => { if (k + 1 >= argv.length) usage(`${a} needs a value`); return argv[++k]; };
    const whole = () => { const n = Number(next()); if (!Number.isInteger(n) || n < 0) usage(`${a} needs a whole number`); return n; };
    if (a === '--bin') o.bin = next();
    else if (a === '--seed') o.seed = whole() >>> 0;
    else if (a === '--cases') o.cases = whole();
    else if (a === '--deep') o.deep = true;
    else if (a === '--procs') o.procs = Math.max(1, whole());
    else if (a === '--measure') o.measure = true;
    else if (a === '--replay') o.replay = next();
    else usage(`unknown argument ${a}`);
  }
  return o;
}
function printThresholds(say) {
  say('THRESHOLDS  PROPOSED, not ratified (ADR-209 item 5):');
  for (const r of THRESHOLD_TABLE) say(`    ${r.name} = ${r.value}  (${r.what}; ${r.worst}; margin ${r.margin}; ${r.date})`);
}

export async function main(argv) {
  const o = parseArgs(argv), say = s => console.log(s), problems = [];
  const ctx = { bin: binPath(o), procs: o.procs, renders: 0 };
  if (!existsSync(ctx.bin)) throw new Error(`h2_fuzz_check: ${ctx.bin} is not built (the CMake target h2_harness_render)`);

  if (o.replay) {
    const { adapter, space, c } = decode(JSON.parse(o.replay));
    const { verdicts } = await evaluate(ctx, adapter, space, [c]);
    say(`REPLAY ${adapter}: ${verdicts[0].classes.length ? verdicts[0].classes.join(' + ').toUpperCase() : 'clean'}${verdicts[0].grey.length ? ` (grey: ${verdicts[0].grey.join(', ')})` : ''}; ${verdictText(verdicts[0])}; ${diffText(space, c)}`);
    return verdicts[0].classes.length ? 1 : 0;
  }

  const engines = enrolled(), table = listAdapters(o).map(a => a.id);
  for (const id of [...engines, CLEAN, ...PLANTS.map(p => p.id)]) if (!table.includes(id)) throw new Error(`h2_fuzz_check: ${ctx.bin} has no adapter "${id}"; rebuild h2_harness_render`);
  if (!engines.length) throw new Error('h2_fuzz_check: no engine is enrolled; a fuzz of nothing must not read green');
  const spaces = Object.fromEntries(engines.map(e => [e, loadSpace(e)]));
  const cases = e => (o.cases !== null ? o.cases : o.deep ? Math.max(DEEP_CASES, prCases(spaces[e])) : prCases(spaces[e]));
  for (const e of engines) if (cases(e) < minCases(spaces[e])) usage(`${cases(e)} cases cannot reach every key of "${e}": its table needs at least ${minCases(spaces[e])}`);
  say(`h2_fuzz_check: seed 0x${o.seed.toString(16).toUpperCase()} (${o.seed}), ${o.deep ? 'deep' : o.cases !== null ? 'custom' : 'PR-size'} run, ${engines.map(e => `${cases(e)} cases of ${e}`).join(', ')}, ${o.procs} render processes`);

  if (o.measure) {
    // A MEASUREMENT, NOT A VERDICT: the numbers the thresholds table is built from.
    for (const e of engines) {
      const t = newTally(), N = cases(e);
      for (let a = 0; a < N; a += SLAB) {
        const cs = Array.from({ length: Math.min(SLAB, N - a) }, (_, k) => makeCase(spaces[e], o.seed, a + k));
        const { verdicts, built } = await evaluate(ctx, e, spaces[e], cs, { measure: MEASURE_EXTRA_S });
        tally(t, cs, built, verdicts);
      }
      say(`MEASURED [${e}] ${t.cases} cases, ${t.samples} samples: pre-limiter bus peak, worst ${t.pre.v.toPrecision(6)} (case ${t.pre.i}); cases on the rail ${t.railCases}; non-finite samples ${t.nonFinite + t.preNaN}`);
      say(`MEASURED [${e}] seconds from the release law's end until the output stays at or under silentFloor (${THRESHOLDS.silentFloor.toExponential(3)}), worst ${t.decay.v.toFixed(4)} (case ${t.decay.i})` +
        (t.decay.v >= MEASURE_EXTRA_S - 1e-9 ? ` -- NEVER within the ${MEASURE_EXTRA_S} s rendered` : ''));
    }
    say('h2_fuzz_check: measured, no verdict');
    return 0;
  }

  // ---- the kept cases first: each failed once and must stay fixed
  const kept = JSON.parse(readFileSync(join(ROOT, KEPT), 'utf8')).cases;
  if (!Array.isArray(kept)) throw new Error(`h2_fuzz_check: ${KEPT} has no "cases" list`);
  for (const k of kept) {
    const { adapter, space, c } = decode(k.case);
    if (!engines.includes(adapter)) { problems.push(`KEPT ${k.id}: "${adapter}" is not an enrolled engine`); continue; }
    const v = (await evaluate(ctx, adapter, space, [c])).verdicts[0];
    if (v.classes.length) problems.push(`KEPT ${k.id} fails again: ${v.classes.join(' + ')}; ${verdictText(v)}\n    REPRO ${replayLine(space, adapter, c)}`);
  }
  say(`KEPT  ${kept.length} kept case(s) replayed first (${KEPT})`);

  let judged = 0;
  for (const e of engines) {
    const space = spaces[e], N = cases(e), t = newTally(), withControls = e === CONTROL_ENGINE;
    const plants = PLANTS.map(p => ({ p, predicted: 0, found: 0, wrong: [], first: null })), clean = { bad: [] };
    let st = null;
    for (let a = 0; a < N; a += SLAB) {
      const cs = Array.from({ length: Math.min(SLAB, N - a) }, (_, k) => makeCase(space, o.seed, a + k));
      const { verdicts, built } = await evaluate(ctx, e, space, cs);
      tally(t, cs, built, verdicts);
      st = coverage(space, cs, built, st);
      if (!withControls) continue;
      for (const q of plants) {
        const r = await evaluate(ctx, q.p.id, space, cs);
        cs.forEach((c, k) => {
          const want = entersCorner(space, q.p, c, r.built[k]), v = r.verdicts[k];
          const right = want ? v.classes.length === 1 && v.classes[0] === q.p.expect : !v.classes.length && !v.grey.length;
          if (want) q.predicted++;
          if (v.classes.includes(q.p.expect)) { q.found++; if (!q.first) q.first = { c, v }; }
          if (!right && q.wrong.length < 5) q.wrong.push(`case ${c.i} ${want ? 'enters the corner' : 'is outside the corner'} and read ${v.classes.join(' + ') || 'clean'}${v.grey.length ? ' grey ' + v.grey.join(',') : ''}`);
        });
      }
      const r = await evaluate(ctx, CLEAN, space, cs);
      cs.forEach((c, k) => { const v = r.verdicts[k]; if ((v.classes.length || v.grey.length) && clean.bad.length < 5) clean.bad.push(`case ${c.i} read ${[...v.classes, ...v.grey].join(' + ')}`); });
    }
    judged += t.cases;

    // ---- the engine's own verdict
    say(`[${e}] ${t.cases} cases (${t.kinds.static} static, ${t.kinds.sweep} sweeps, ${t.kinds.stack} stacked changes), ${t.samples} samples judged`);
    say(`[${e}] NON-FINITE  ${t.nonFinite} output samples${t.preNaN ? ` and ${t.preNaN} unreadable bus samples` : ''} (the engine NaN count, risk row R3)`);
    say(`[${e}] LEVEL  pre-limiter bus peak, worst ${t.pre.v.toPrecision(5)} at case ${t.pre.i} (pass to ${THRESHOLDS.prePass}, grey above it, FAIL on the rail); cases on the rail: ${t.railCases}`);
    say(`[${e}] SILENCE  judged in ${t.asserted} of ${t.cases} cases (the rest have a tail cut short of the release law, counted not judged); longest release judged ${t.longestAsserted.toFixed(2)} s of ${t.longest.toFixed(2)} s; worst tail peak ${t.tail.v.toExponential(2)}${t.tail.i !== null ? ` at case ${t.tail.i}` : ''} (floor ${THRESHOLDS.silentFloor.toExponential(2)})`);
    if (!t.asserted) problems.push(`[${e}] silence was judged in no case`);
    if (t.longestAsserted < t.longest) problems.push(`[${e}] the longest release in the run (${t.longest.toFixed(2)} s) was never judged for silence (longest judged ${t.longestAsserted.toFixed(2)} s)`);
    keyReport(space, st, say, problems);
    {
      // the key report must be able to say no: the same counts with ONE key's taken away
      const lost = space.rows.find(r => r.kind === 'c').key, scratch = [];
      keyReport(space, Object.assign({}, st, { [lost]: { sets: 0, away: 0, min: 0, max: 0, sweeps: 0, stacks: 0, opts: null } }), () => {}, scratch);
      const named = scratch.length === 5 && scratch.every(p => p.endsWith(`: ${lost}`));
      if (!named) problems.push(`[${e}] CONTROL coverage: with the counts of "${lost}" removed the key report gave ${scratch.length} problem(s), want the five that name it`);
      say(`[${e}] CONTROL coverage  the same counts with those of "${lost}" removed: ${named ? 'red five ways (never set, no minimum, no maximum, never swept, never stacked)' : 'NOT reported'}`);
    }
    if (t.greyCount) {
      say(`[${e}] GREY  ${t.greyCount} case(s) in a grey band, for the human's ears (not red):`);
      for (const [n, g] of t.grey.entries()) {
        if (n >= MINIMISE_PER_ENGINE) { say(`    case ${g.c.i} (${g.v.grey.join(', ')}): ${verdictText(g.v)} (not minimised)`); continue; }
        const m = await minimise(ctx, e, space, g.c, g.v);
        say(`    case ${g.c.i} (${g.v.grey.join(', ')}): ${verdictText(g.v)}\n      ${m.minimised ? `minimised to ${m.items} item(s), ${m.c.notes.length} note(s)` : 'not minimised'}: ${diffText(space, m.c)}\n      REPRO ${replayLine(space, e, m.c)}`);
      }
    }
    if (t.failCount) {
      problems.push(`[${e}] ${t.failCount} failing case(s); first: case ${t.fails[0].c.i} ${t.fails[0].v.classes.join(' + ')}`);
      say(`[${e}] FAILED  ${t.failCount} case(s):`);
      for (const [n, f] of t.fails.entries()) {
        if (n >= MINIMISE_PER_ENGINE) { say(`    case ${f.c.i} ${f.v.classes.join(' + ')}: ${verdictText(f.v)} (not minimised)\n      REPRO ${replayLine(space, e, f.c)}`); continue; }
        const m = await minimise(ctx, e, space, f.c, f.v);
        say(`    case ${f.c.i} ${f.v.classes.join(' + ')}: ${verdictText(f.v)}\n      ${m.minimised ? `minimised to ${m.items} item(s), ${m.c.notes.length} note(s)` : 'NOT minimised (it fails only with its own tail)'}: ${diffText(space, m.c)}\n      REPRO ${replayLine(space, e, m.c)}`);
        say(`      KEEP  add to ${KEPT} "cases": ${JSON.stringify({ id: `${e}-seed${o.seed}-case${f.c.i}`, found: `seed ${o.seed}, case ${f.c.i}`, was: m.cls, case: encode(space, e, m.c) })}`);
      }
    }
    if (!withControls) continue;

    // ---- the controls: the same cases through the planted faults and the clean tone
    for (const q of plants) {
      const where = cornerText(space, q.p);
      if (q.wrong.length) problems.push(`CONTROL ${q.p.id}: the verdict does not match the corner: ${q.wrong.join('; ')}`);
      if (!q.found) { problems.push(`CONTROL ${q.p.id}: the planted ${q.p.expect} at ${where} was NOT FOUND in ${t.cases} cases`); say(`CONTROL ${q.p.id}  NOT FOUND (${where})`); continue; }
      const m = await minimise(ctx, q.p.id, space, q.first.c, q.first.v);
      // the minimised case must be the corner and nothing else
      const final = Object.assign({}, m.c.base), touched = new Set(space.rows.filter(r => m.c.base[r.key] !== r.def).map(r => r.key));
      for (const ev of m.c.events) for (const [k, v] of ev.sets) { final[k] = v; touched.add(k); }
      const want = Object.keys(q.p.corner), exact = m.minimised && touched.size === want.length && want.every(k => touched.has(k) && atEnd(space.byKey[k], q.p.corner[k], final[k]));
      if (!exact) problems.push(`CONTROL ${q.p.id}: minimised to "${diffText(space, m.c)}", which is not the corner (${where})`);
      say(`CONTROL ${q.p.id}  planted ${q.p.expect} at ${where}: FOUND in ${q.found} of ${t.cases} cases, ${q.wrong.length ? 'NOT ' : ''}exactly the ${q.predicted} that enter the corner; first at case ${q.first.c.i} (within the first ${q.first.c.i + 1} cases)`);
      say(`    minimised to ${m.items} item(s), ${m.c.notes.length} note(s): ${diffText(space, m.c)}${exact ? ' -- the corner and nothing else' : ' -- NOT the corner'}\n    REPRO ${replayLine(space, q.p.id, m.c)}`);
    }
    if (clean.bad.length) problems.push(`CONTROL ${CLEAN}: a clean adapter was reported: ${clean.bad.join('; ')}`);
    say(`CONTROL ${CLEAN}  must read clean over the same ${t.cases} cases: ${clean.bad.length ? 'REPORTED ' + clean.bad.join('; ') : 'nothing reported'}`);
    // an undeclared key must be reported, or "no unknown key" above proves nothing
    const c0 = makeCase(space, o.seed, 0), b0 = build(space, c0);
    b0.cmds.unshift(['set', 'h2fuzz.no-such-key', 1]);
    const vk = judge((await renderBatch([jobOf(e, c0, b0)], ctx.bin))[0], b0);
    if (!vk.classes.includes('unknown-key')) problems.push(`CONTROL unknown-key: a key the engine does not have was not reported on "${e}"`);
    say(`CONTROL unknown-key  a key no table declares, sent to ${e}: ${vk.classes.includes('unknown-key') ? `reported ("${vk.firstUnknown}")` : 'NOT reported'}`);
  }
  if (!engines.includes(CONTROL_ENGINE)) problems.push(`the controls are written in the table of "${CONTROL_ENGINE}", which is not enrolled: no control ran`);

  printThresholds(say);
  for (const p of problems) console.error(`h2_fuzz_check: RED  ${p}`);
  say(`h2_fuzz_check: ${problems.length ? 'FAILED' : 'GREEN'} (${judged} cases over ${engines.length} enrolled engine(s): ${engines.join(', ')}; ${ctx.renders} renders with the controls and minimising; ${problems.length} problem(s))`);
  if (o.deep) say(`KIT-GATE cases=${judged}`);
  return problems.length ? 1 : 0;
}

if (import.meta.url === `file://${process.argv[1]}`) {
  main(process.argv.slice(2)).then(code => { process.exitCode = code; }, e => { console.error(e.message || e); process.exitCode = 2; });
}
