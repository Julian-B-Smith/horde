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
 *   - the pre-limiter bus level is read, however loud, and judged: a steady level
 *     against three bands, a level that keeps growing as a BLOW-UP. The engine ends
 *     in tanh, which would hide both, so the bus is read, not the output;
 *   - after the last key-off the output falls silent in a stated time;
 *   - the engine refuses no key the table declares.
 *
 * THE CASES. Case i is a pure function of (table, seed, i):
 *   static   a drawn patch, 1 to 4 notes at drawn velocities (0.1 to 1 each, a full
 *            chord included) held 0.25 to 1 s, then released.
 *   sweep    the same, with ONE key moved in steps during the held note: end to end,
 *            back, or between two drawn values; an option list steps through every
 *            option.
 *   stack    the same, with 2 to 8 keys changed in the SAME block, at 1 to 3 moments.
 * The rate (44.1, 48 or 96 kHz), the host block size and the host seed are drawn per
 * case. A PREAMBLE comes first, so that reaching every extreme does not depend on
 * luck: every continuous key at its minimum, then at its maximum, then one case per
 * option index, then the engine's NAMED cases, which are played on every run whatever
 * the seed. After it the cases rotate sweep, stack, static, and the swept or stacked
 * key rotates through the table, so every key that can vary is swept once and leads
 * a stacked change once in every round of 3 x (keys) cases.
 *
 * READING ABOVE THE RAIL. The harness recovers the bus as atanh(output). From about
 * 19 the output is exactly +-1 and the bus reads infinity: the rail. A loud chord
 * gets there without anything being wrong (the named case `loud-chord` reads 19.27),
 * so "it touched the rail" says nothing. When a case touches the rail, or peaks
 * above READ_ABOVE where atanh is already losing digits, the SAME case is rendered
 * again with every gain value it sets (the patch's and each change's during the
 * note) divided by READ_FACTOR = 2^40, and the reading is multiplied back. That
 * reading is the case's true level.
 *   WHY THAT IS VALID. h2/engine/engine.h lines 1961-1962 are the engine's last:
 *       L[i] = std::tanh(yl * s.gain * 1.6);   R[i] = std::tanh(yr * s.gain * 1.6);
 *   They are the only reads of s.gain in the file. s.gain is written only by snap()
 *   and by the 16-sample glide `s += (t - s) * a` (line 1802), which is linear in the
 *   values set. So the bus is the gain values times something gain cannot reach. 2^40
 *   is a power of two, so the division, the glide and the multiplication back are
 *   exact in binary floating point; only tanh and atanh round, and far less at a
 *   small argument than at a large one.
 *   WHAT WOULD BREAK IT. Any stage before that line that reads gain: a compressor
 *   or a saturator keyed to it, a level-dependent voice cull, a gain-dependent
 *   denormal guard. And the voice cap's fade, which passes samples through a float
 *   (adapter_composed.h); the harness does not expose the cap.
 *   IT IS NOT ASSUMED. On every run the source is read again, and one more read of
 *   the gain member anywhere under h2/engine/ stops the run (ENGINES.gainIsLast).
 *   And on every run the drawn preamble cases that sit well under the
 *   rail are rendered both ways and compared sample by sample (CONTROL linearity). A
 *   difference above linTol is red: the method is then wrong and no level it read
 *   can be trusted. The same comparison on control/tone, whose bus ignores gain,
 *   must fail. The planted steady level is also read back against the number it
 *   was built with.
 *   ITS LIMITS. A bus still on the rail at 1 / 2^40 of the gain is above 2e13: OFF
 *   SCALE, and called a blow-up. An engine that declares no gain key cannot be read
 *   this way: a case on its rail is counted as LEVEL UNREAD, printed, and red.
 *
 * LOUD VERSUS GROWING. A level that is finite and steady or periodic, however loud,
 * is a LEVEL: pass to prePass, grey to preFail, TOO LOUD above it. A level that keeps
 * growing is a BLOW-UP. The rule: a case whose level is above prePass is rendered
 * twice more, with the stretch of the note that held its peak made PROBE_S (0.5 s)
 * longer and twice that longer, through the divided gain. In each the bus peak of
 * the last quarter of that stretch is set against the quarter before it. GROWTH is
 * the smallest of three ratios: last quarter over the one before, at each length,
 * and the long hold's last quarter over the short hold's. Steady to growPass, grey
 * to growFail, a BLOW-UP above it. A level off scale in any of these is a blow-up.
 *   WHY THE SMALLEST OF THREE. One burst in one window (a beating chord), a linear
 *   attack (it reads 1.33) and a level that has stopped rising each leave at least
 *   one ratio small. An exponential rise makes all three large.
 *   ITS LIMITS. A level under prePass is not probed, so a slow instability that has
 *   not got loud within the 1 s hold is unseen (the soak, P6, holds for hours). A
 *   rise slower than growFail per quarter of the held stretch (about 0.13 to 0.4 s)
 *   reads as a level, and is then judged as one. The probe holds the STATE the peak
 *   was in; a fault that needs a further change to show is not followed.
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
 * four adapters that each carry one planted fault in one corner of the same table:
 *   control/fuzz-blowup  a level that doubles every 40 ms: must read BLOW-UP, and at
 *                        least one case must be decided by the growth windows
 *                        (the rest go off scale while held);
 *   control/fuzz-loud    a steady level of 4096 x gain, over 200 times the rail:
 *                        must read as a level, AT that number, and NOT a blow-up;
 *   control/fuzz-nan     one NaN on one channel: must read non-finite;
 *   control/fuzz-hang    a tone that never stops: must read not-silent.
 * For each, planted() works out from the case alone what the plant must have
 * rendered, and the run is RED unless every case reads exactly that: the fault where
 * it was planted, nothing where it was not, and the right KIND. The first failing
 * case must minimise to the corner and nothing else. The same cases through
 * control/tone must report nothing. A key no table declares must be reported. The
 * key report must go red when one key's counts are taken away.
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
 * stopwatch held outside this tool, which reads no clock). PR-size, 1021 cases with
 * the controls, the re-reads, the probes and the minimising: 34 s and 43 s on two
 * runs with the machine's load average between 3 and 5 (112 and 147 s of CPU). At 33
 * to 42 ms a case the deep run's 20000 cases are 11 to 14 minutes here. A hosted
 * 4-core runner is ASSUMED four to six times slower (not measured: no nightly has
 * run), which makes the deep run 44 to 85 minutes there. The count is one number in
 * tools/nightly/jobs.json, beside its floor.
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
 *   - Silence is judged where the tail is rendered to the end of the release: every
 *     case whose release is short, and every FULL_TAIL_EVERY-th case whatever its
 *     release. The rest render 0.25 s of release (finite and bounded are still
 *     judged) and are counted as not judged for silence.
 *   - Notes are plain: no hostile note events, no note churn (the soak, P6), no
 *     rate-by-block matrix (the invariance matrix, P4). A hold is 1 s at most.
 *   - THIS FILE'S WIRING CLAIM IS NOT CROSS-CHECKED. tools/test_table_check.py reads
 *     tools/*_check.cpp, tools/*_check.py and tools/labharness/*_check.mjs. It does
 *     not read tools/*.mjs, so it would not notice if the line above were false.
 */
import { spawn } from 'node:child_process';
import { existsSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
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
 * 12000 cases of `composed` with full-velocity chords in scope, 7,986,889,844 samples,
 * nothing non-finite, nothing off scale. --measure probes EVERY case for growth and
 * compares up to 200 cases for linearity; a normal run does both only where it must.
 *   level      In both runs the loudest case was the named `loud-chord`, 19.2663 as
 *              first written. Minimised while it stays loud it reads 21.368, and that
 *              form is the named case now, read on every run. Of the 11998 DRAWN
 *              cases, three peaked above 8 and none reached 19.27.
 *   growth     worst 1.6904 (seed 46152, case 2307) and 1.6236 (case 3402): what a
 *              healthy engine does when its loudest stretch is held longer.
 *   linearity  worst difference 3.947e-15 and 3.973e-15 of the direct peak, over 135
 *              and 133 cases with direct peaks from 0.05 to 6.
 *   silence    seconds from the release law's end to under silentFloor, worst 0.2022
 *              (seed 46152, case 5425) and 0.1911 (case 1209).
 *
 * THE BANDS.
 *   level     the TRUE bus peak. At or under prePass: pass. To preFail: GREY. Above
 *             it: FAIL (too loud). The loudest chord found reads 21.368 at full gain,
 *             and that is the measured worst: every band sits above it.
 *   growth    of a case above prePass, held longer. To growPass: steady. To growFail:
 *             GREY. Above it, or off scale: FAIL (blow-up).
 *   silence   under silentFloor from the release law's end + settlePass: pass. Under
 *             it only from + settleFail: GREY. Still above it then: FAIL.
 *   linearity a control, with one line: above linTol the run is red.
 * A non-finite sample and an unknown key have no band. One is red.
 *
 * WHY THESE MARGINS (argued, for the human to change). Level: x2 is 6 dB over the
 * loudest chord found and x4 is 12 dB. Growth: the healthy worst is under a doubling
 * per quarter, x1.5 and x3 put the lines at 2.5 and 5.1 times per quarter, and the
 * planted blow-up reads 9 or more. The tail after a release is the engine's DC
 * blocker ringing down (measured: 60 dB per 0.14 s), so x1.5 on the time is about 44
 * dB more decay than the worst case needed. Linearity: x1000, the room the harness
 * selftest gives a round trip through tanh and atanh "because another platform's
 * libm may round differently"; a real dependence on gain would show at parts in
 * 100 or 1000, nine decades above the line.
 */
const THRESHOLD_TABLE = [
  { name: 'prePass', value: 42.8, what: 'true pre-limiter bus peak, the top of the pass band',
    worst: 'measured worst 21.368 (the named case loud-chord, held 6 s)', margin: 'x2 (6 dB)', date: '2026-10-11' },
  { name: 'preFail', value: 85.5, what: 'true pre-limiter bus peak above which a steady level fails as too loud',
    worst: 'measured worst 21.368 (the same)', margin: 'x4 (12 dB)', date: '2026-10-11' },
  { name: 'growPass', value: 2.54, what: 'growth of a loud case held longer, the top of the steady band',
    worst: 'measured worst 1.6904 (seed 46152, case 2307, over all 12000 cases)', margin: 'x1.5', date: '2026-10-11' },
  { name: 'growFail', value: 5.08, what: 'growth above which a loud case is a blow-up',
    worst: 'measured worst 1.6904 (the same)', margin: 'x3', date: '2026-10-11' },
  { name: 'offScale', value: 'the rail at gain / 2^40', what: 'a level the divided gain cannot bring off the rail (above 2e13) is a blow-up',
    worst: 'not this tool\'s: the adapter\'s limit of sight (h2/harness/README.md: 19 is under the rail, 19.2 on it) times READ_FACTOR', margin: 'none', date: '2026-10-11' },
  { name: 'linTol', value: 4e-12, what: 'the linearity control: largest difference between the direct bus and the one read through the divided gain, as a share of the direct peak',
    worst: 'measured worst 3.973e-15 (seed 20261011, case 186, of 268 cases compared)', margin: 'x1000', date: '2026-10-11' },
  { name: 'settlePass', value: 0.304, what: 'seconds after the release law\'s end by which the output is under silentFloor',
    worst: 'measured worst 0.2022 (seed 46152, case 5425)', margin: 'x1.5', date: '2026-10-11' },
  { name: 'settleFail', value: 0.607, what: 'seconds after the release law\'s end past which a tail above silentFloor fails',
    worst: 'measured worst 0.2022 (the same)', margin: 'x3', date: '2026-10-11' },
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
// Reading above the rail (see "READING ABOVE THE RAIL" in the header). A power of two, so
// dividing every gain value by it and multiplying the reading back are exact in binary.
const READ_FACTOR = 2 ** 40;
const READ_ABOVE = 8;           // a direct bus peak above this is re-read through the divided gain: atanh loses
                                // precision toward the rail (h2/harness/README.md: 4.5e-9 at 10, 0.13 at 18.5)
const PROBE_S = 0.5;            // the growth probe holds the loudest stretch this much longer, and twice this
const LIN_FROM = 0.05, LIN_TO = 6, LIN_MIN = 4;   // the linearity control: which direct peaks it compares, and how many at least

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
    // The key the output stage is linear in. h2/engine/engine.h lines 1961-1962:
    //     L[i] = std::tanh(yl * s.gain * 1.6);   R[i] = std::tanh(yr * s.gain * 1.6);
    // are the only reads of s.gain in the file, and s.gain is written only by snap() and by
    // the 16-sample glide `s += (t - s) * a` (line 1802), which is linear too.
    gain: 'gain',
    gainIsLast() {
      // Every read of the gain member in h2/engine/*.h (`.gain` or `->gain`, whatever the
      // struct is called there) must be one of the limiter's two lines. One more read
      // anywhere is a stage that depends on gain, and the reading must be argued again.
      const dir = join(ROOT, 'h2/engine'), reads = [];
      for (const f of readdirSync(dir).filter(n => n.endsWith('.h')).sort()) for (const l of readFileSync(join(dir, f), 'utf8').split('\n')) if (/(\.|->)gain\b/.test(l)) reads.push(l);
      const last = reads.filter(l => /^\s*[LR]\[i\] = std::tanh\(y[lr] \* s\.gain \* 1\.6\);\s*$/.test(l));
      if (reads.length !== 2 || last.length !== 2) throw new Error(`h2_fuzz_check: h2/engine/*.h read the gain member on ${reads.length} line(s), ${last.length} of them the limiter's two: the output stage changed, so the reading through the divided gain is no longer shown to be valid`);
    },
    // Cases that are played on every run, whatever the seed, after the preamble's own.
    named: [
      // The loudest thing this fuzz has found: nine swarm members at zero detune, blade 2 on,
      // gain stepped to 1 under four full-velocity notes. First seen 2026-10-11 as case 2479 of
      // seed 46152 in an earlier build (bus 17.19 in 0.8 s); this is that case minimised while it
      // stays loud. Steady and PERIODIC, not growing: a burst about every 5.3 s, so it is held
      // 6 s to include one. It is the measured worst the level bands are built from.
      { name: 'loud-chord', case: { v: 1, engine: 'composed', table: 'composed', sr: 44100, block: 128, seed: 1802188165,
        notes: [[93, 0.8262921872548759], [52, 0.48174431112129246], [66, 0.907497893483378], [76, 0.7283318708883598]], hold: 264600, tail: 'auto',
        sets: { base: 3, b2on: 1, mode2: 3, c2: 1, mirror2: 3, N: 9, detune: 0 },
        events: [[8878, [['gain', 1]]]] } },
    ],
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
  if (cfg.gainIsLast) cfg.gainIsLast();
  if (cfg.gain && !rows.some(r => r.key === cfg.gain && r.kind === 'c' && !r.mapped)) throw new Error(`h2_fuzz_check: ${cfg.table} has no continuous row "${cfg.gain}" to read the level through`);
  return { engine, cfg, rows, fuzz, byKey: Object.fromEntries(rows.map(r => [r.key, r])),
    unranged: (tree.unsampled || []).map(u => u.key),
    maxOpts: Math.max(...rows.filter(r => r.opts).map(r => r.opts.length)),
    defaults: Object.fromEntries(rows.map(r => [r.key, r.def])) };
}
/* The fewest cases that reach everything by construction: the preamble, then one sweep,
   one stack and one static case per key that can vary. */
export const preambleCases = space => 2 + space.maxOpts + (space.cfg.named || []).length;
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
  while (notes.length < count) { const n = NOTE_LO + Math.floor(g() * (NOTE_HI - NOTE_LO + 1)); if (!notes.some(x => x[0] === n)) notes.push([n, 0.1 + 0.9 * g()]); }
  const c = { i, kind: 'static', lead: null, sr, block, seed: hash32(cs, 4), notes, hold, base: null, events: [], tail: i % FULL_TAIL_EVERY === 1 ? 'full' : 'auto' };
  const mode = g() < EDGE_SHARE ? 'edge' : 'broad';

  if (i < pre) {
    // THE PREAMBLE: coverage by construction, so no key's extremes depend on luck.
    //   0  every continuous key at its declared minimum, every option list at its first
    //   1  every continuous key at its declared maximum, every option list at its last
    //   2+k  an edge patch with every enumeration at option k (mod its length)
    //   then the engine's NAMED cases, exactly as written, whatever the seed
    const named = (space.cfg.named || [])[i - 2 - space.maxOpts];
    if (i >= 2 + space.maxOpts) return Object.assign(decode(named.case).c, { i, kind: 'named', name: named.name, mode: 'named' });
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
    silenceFrom: asserted ? c.hold + passAt : null, lateFrom: asserted ? c.hold + failAt : null,
    windows: c.windows || null, scale: c.scale || 1 };
}
const jobOf = (adapter, c, b) => ({ name: `case ${c.i}`, engine: adapter, sr: c.sr, seed: c.seed,
  block: typeof c.block === 'object' ? c.block.vary : c.block, vary: typeof c.block === 'object', pre: true, cmds: b.cmds });

/* ================================================================== reading one render
 * read() turns one render into FACTS and decides nothing; verdictOf() decides, once
 * the level has been read above the rail and a loud case has been probed for growth
 * (assess, below). EVERY sample of both channels is read.
 *
 * `b.scale` is what the bus must be multiplied by: 1 for a direct render, READ_FACTOR
 * for a replay whose gain values were divided by it. */
const MEASURE_FLOORS = [0, 1e-9, 1e-6, Math.pow(10, -90 / 20), 1e-3];
export function read(r, b, opt) {
  if (r.crash) return { crash: r.crash, nonFinite: 0, preNaN: 0, rail: 0, prePeak: 0, peakAt: 0, tailPeak: null, latePeak: null, samples: 0, win: null };
  if (r.L.length !== b.frames) throw new Error(`h2_fuzz_check: ${r.name}: rendered ${r.L.length} frames, asked for ${b.frames}`);
  if (!r.preL) throw new Error(`h2_fuzz_check: ${r.name}: the render carries no pre-limiter bus`);
  const n = b.frames, v = { nonFinite: 0, preNaN: 0, rail: 0, prePeak: 0, peakAt: 0, tailPeak: null, latePeak: null, samples: 2 * n, win: null,
    unknownKeys: r.summary.unknownKeys, firstUnknown: r.summary.firstUnknown };
  for (const [y, p] of [[r.L, r.preL], [r.R, r.preR]]) for (let k = 0; k < n; k++) {
    if (!Number.isFinite(y[k])) { v.nonFinite++; continue; }
    const a = Math.abs(p[k]);
    if (a !== a) v.preNaN++;
    else if (a === Infinity) v.rail++;
    else if (a > v.prePeak) { v.prePeak = a; v.peakAt = k; }
  }
  v.prePeak *= b.scale;
  if (b.windows) {
    // the bus peak inside each window; a window that touches the rail reads Infinity
    v.win = b.windows.map(([from, to]) => {
      let m = 0;
      for (const p of [r.preL, r.preR]) for (let k = from; k < to; k++) { const a = Math.abs(p[k]); if (a > m) m = a; }
      return m * b.scale;
    });
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
  }
  return v;
}

/* ================================================================== the verdict
 * From the facts of a case (v), after assess() has filled in:
 *   v.level     the TRUE bus peak: the direct reading, or the reading through the
 *               divided gain scaled back (v.via says which; v.blowBy says what made
 *               a blow-up one)
 *   v.offScale  the bus was on the rail even with the gain divided by READ_FACTOR
 *   v.unread    the bus was on the rail and the engine declares no gain key to divide
 *   v.growth    for a loud case only: how the level grew when held longer (growthOf)
 *
 *   crash        the render process died on this case.
 *   non-finite   an output sample that is not a finite number, or a bus sample that
 *                is NaN under a finite output. The count is risk row R3's engine NaN count.
 *   blow-up      a level that KEEPS GROWING (growth above growFail), or one that is
 *                off scale: above the rail even at 1 / READ_FACTOR of the gain.
 *   too-loud     a level that is finite and steady or periodic, above preFail.
 *   level-unread on the rail with no way to read above it.
 *   not-silent   a sample above silentFloor at or after the release law's end plus settleFail.
 *   unknown-key  the engine refused a key the table declares.
 * GREY (reported with its replay line, never red): a steady level in (prePass,
 * preFail]; growth in (growPass, growFail]; a tail above silentFloor after
 * settlePass that is under it by settleFail. */
export function verdictOf(v) {
  const T = THRESHOLDS;
  v.classes = []; v.grey = [];
  if (v.crash) { v.classes.push('crash'); return v; }
  if (v.nonFinite || v.preNaN) v.classes.push('non-finite');
  const growing = v.offScale || (v.growth !== undefined && v.growth > T.growFail);
  if (growing) v.classes.push('blow-up');
  else if (v.unread) v.classes.push('level-unread');
  else if (v.level > T.preFail) v.classes.push('too-loud');
  else if (v.level > T.prePass) v.grey.push('level');
  if (!growing && v.growth !== undefined && v.growth > T.growPass) v.grey.push('growth');
  if (v.latePeak !== null && v.latePeak > T.silentFloor) v.classes.push('not-silent');
  else if (v.tailPeak !== null && v.tailPeak > T.silentFloor) v.grey.push('tail');
  if (v.unknownKeys) v.classes.push('unknown-key');
  return v;
}
/* "Keeps growing", from four window peaks: the last quarter (index 1) and the quarter
   before it (index 0) of the loudest stretch held PROBE_S longer (short), and held
   twice that longer (long). The growth is the SMALLEST of three ratios: the last
   quarter over the one before it at each length, and the long hold's last quarter
   over the short hold's. All three must be large for it to be large, so one loud
   burst in one window (a beating chord), a linear attack (it reads 1.33) and a level
   that has stopped rising all read small, and an exponential rise reads large at
   every length. A ratio over nothing is Infinity if anything is there, 1 if not. */
export function growthOf(short, long) {
  const ratio = (a, b) => (b > 0 ? a / b : a > 0 ? Infinity : 1);
  return Math.min(ratio(short[1], short[0]), ratio(long[1], long[0]), ratio(long[1], short[1]));
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
/* Renders `cases` on one adapter and reads each; returns the facts in case order. A
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
    idx.forEach((k, m) => { verdicts[k] = read(results[m], built[k], opt); });
  });
  ctx.renders += cases.length;
  return { verdicts, built };
}

/* ================================================================== reading the level, then the verdict
 * 1. DIRECT. Every case is rendered as it is.
 * 2. ABOVE THE RAIL. A case whose bus touched the rail, or peaked above READ_ABOVE, is
 *    replayed with EVERY gain value it sets (the patch's and each change's) divided
 *    by READ_FACTOR, and the reading is multiplied back. Its level is that reading.
 *    Still on the rail there: off scale. No gain key declared: unread.
 * 3. GROWTH. A case whose level is above prePass is replayed twice more, with the
 *    stretch that held its peak made PROBE_S and 2 * PROBE_S longer (later changes
 *    move back by as much), and growthOf() reads the last two quarters of each.
 *    `opt.growAll` probes every case (--measure). */
const dividedGain = (space, c) => {
  const k = space.cfg.gain, f = v => v / READ_FACTOR;
  return Object.assign({}, c, { scale: READ_FACTOR, base: Object.assign({}, c.base, { [k]: f(c.base[k]) }),
    events: c.events.map(e => ({ at: e.at, sets: e.sets.map(([key, v]) => (key === k ? [key, f(v)] : [key, v])) })) });
};
/* the case with the stretch around frame `at` made `extra` frames longer, and the two windows to read */
function heldLonger(c, at, extra) {
  const edges = [0, ...c.events.map(e => e.at), c.hold];
  let s = edges.length - 2;
  for (let k = 0; k < edges.length - 1; k++) if (at >= edges[k] && at < edges[k + 1]) { s = k; break; }
  const from = edges[s], to = edges[s + 1] + extra, q = Math.floor((to - from) / 4);
  return Object.assign({}, c, { hold: c.hold + extra, tail: 'short', windows: [[to - 2 * q, to - q], [to - q, to]],
    events: c.events.map(e => ({ at: e.at >= edges[s + 1] ? e.at + extra : e.at, sets: e.sets })) });
}
export async function assess(ctx, adapter, space, cases, opt) {
  const ev = await evaluate(ctx, adapter, space, cases, opt), vs = ev.verdicts, gain = space.cfg.gain;
  for (const v of vs) { v.level = v.prePeak; v.via = 'direct'; }
  // ---- above the rail
  const high = vs.map((v, k) => k).filter(k => !vs[k].crash && (vs[k].rail || vs[k].prePeak > READ_ABOVE));
  if (high.length && !gain) for (const k of high) { if (vs[k].rail) vs[k].unread = true; }
  else if (high.length) {
    const red = (await evaluate(ctx, adapter, space, high.map(k => dividedGain(space, cases[k])))).verdicts;
    high.forEach((k, m) => {
      const v = vs[k], r = red[m];
      if (r.crash) { v.crash = r.crash; return; }
      v.nonFinite += r.nonFinite; v.preNaN += r.preNaN;
      if (r.rail) { v.offScale = true; v.blowBy = 'off scale'; return; }
      v.level = r.prePeak; v.peakAt = r.peakAt; v.via = 'divided gain';
    });
  }
  // ---- growth
  const loud = vs.map((v, k) => k).filter(k => !vs[k].crash && !vs[k].offScale && !vs[k].unread && ((opt && opt.growAll) || vs[k].level > THRESHOLDS.prePass));
  if (loud.length) {
    const probes = [];
    for (const k of loud) for (const mult of [1, 2]) {
      const c = heldLonger(cases[k], vs[k].peakAt, Math.round(mult * PROBE_S * cases[k].sr));
      probes.push(gain ? dividedGain(space, c) : c);
    }
    const pr = (await evaluate(ctx, adapter, space, probes)).verdicts;
    loud.forEach((k, m) => {
      const v = vs[k], a = pr[2 * m], b = pr[2 * m + 1];
      if (a.crash || b.crash) { v.crash = a.crash || b.crash; return; }
      v.nonFinite += a.nonFinite + b.nonFinite; v.preNaN += a.preNaN + b.preNaN;
      v.win = [a.win, b.win];
      v.growth = growthOf(a.win, b.win);
      // a window on the rail even through the divided gain: the level left the scale while it was held
      if ([...a.win, ...b.win].includes(Infinity)) { v.offScale = true; v.blowBy = 'off scale when held longer'; }
      else if (v.growth > THRESHOLDS.growFail) v.blowBy = 'the growth windows';
    });
  }
  for (const v of vs) verdictOf(v);
  return ev;
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
 * and the plants are run over that engine's own cases. `start`, `doubleS`, `cap` and
 * `level` restate that file's kStart, kDoubleS, kLevelCap and kLoudLevel. */
const CONTROL_ENGINE = 'composed';
const CLEAN = 'control/tone';
const PLANTS = [
  { id: 'control/fuzz-blowup', kind: 'growing', expect: 'blow-up', corner: { fb: 'max', K: 'min' }, start: 4096, doubleS: 0.04, cap: 1e30 },
  { id: 'control/fuzz-loud', kind: 'steady', expect: 'too-loud', corner: { xm: 'max', kRuleAmt: 'min' }, level: 4096 },
  { id: 'control/fuzz-nan', kind: 'nan', expect: 'non-finite', corner: { detune: 'max', S: 'min', aa: 0 } },
  { id: 'control/fuzz-hang', kind: 'hang', expect: 'not-silent', corner: { R: 'min', A: 'max' } },
];
const NEAR = 1e-9;   // a planted level this close (relatively) to a band's edge may read either side: counted, not judged
const atEnd = (row, end, v) => (end === 'max' ? v >= row.max : end === 'min' ? v <= row.min : v === end);
const cornerText = (space, plant) => Object.entries(plant.corner).map(([k, e]) => `${k} = ${e === 'max' ? space.byKey[k].max : e === 'min' ? space.byKey[k].min : e}${typeof e === 'string' ? ` (its ${e})` : ''}`).join(', ');
/* What must this plant have rendered for this case? Worked out from the case alone, by
   walking its changes: for each stretch of the held note, is the plant in its corner,
   and at what gain. So the fuzz's reading of a plant is checked BOTH ways and in KIND:
   every case that enters the corner must read as the plant's fault, no other case may
   read as anything, and a level plant must read at the level it was built to have.
   Returns { classes, grey, level?, near? }. */
export function planted(space, plant, c, b) {
  const cur = Object.assign({}, c.base), stretches = [], T = THRESHOLDS;
  const at = () => Object.entries(plant.corner).every(([k, e]) => atEnd(space.byKey[k], e, cur[k]));
  let from = 0;
  for (const e of [...c.events, { at: c.hold, sets: [] }]) {
    if (e.at > from) stretches.push({ len: e.at - from, corner: at(), gain: cur[space.cfg.gain] });
    for (const [k, v] of e.sets) cur[k] = v;
    from = e.at;
  }
  const clean = { classes: [], grey: [] };
  if (plant.kind === 'nan') return stretches.some(x => x.corner) ? { classes: ['non-finite'], grey: [] } : clean;
  // a hang shows only where the tail is long enough for silence to be owed
  if (plant.kind === 'hang') return at() && b.silenceFrom !== null ? { classes: ['not-silent'], grey: [] } : clean;
  // a level plant: the level on a frame is the plant's own times the gain of that stretch
  let level = 0, n = 0;
  for (const x of stretches) {
    if (!x.corner) { n = 0; continue; }
    // control_fuzz.h counts the frames of one visit to the corner; the loudest frame of a stretch is its last
    const own = plant.kind === 'steady' ? plant.level : Math.min(plant.cap, plant.start * Math.pow(2, (n + x.len - 1) / (plant.doubleS * c.sr)));
    level = Math.max(level, own * x.gain);
    n += x.len;
  }
  const near = [T.prePass, T.preFail].some(edge => Math.abs(level - edge) <= NEAR * edge);
  if (level <= T.prePass) return Object.assign(clean, { near });
  // a growing level above the pass line is probed, and must read as growing however loud it is
  if (plant.kind === 'growing') return { classes: ['blow-up'], grey: [], near };
  return { classes: level > T.preFail ? ['too-loud'] : [], grey: level > T.preFail ? [] : ['level'], level, near };
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
const CLASS_ORDER = ['crash', 'non-finite', 'blow-up', 'too-loud', 'level-unread', 'not-silent', 'unknown-key'];
export async function minimise(ctx, adapter, space, c, v0, keep) {
  // what must remain: `keep` if given, else the first failing class, or for a grey case the same grey reading
  const cls = keep ? 'a reading' : CLASS_ORDER.find(k => v0.classes.includes(k)) || `grey ${v0.grey[0]}`;
  const same = keep || (v => (cls.startsWith('grey ') ? !v.classes.length && v.grey.includes(v0.grey[0]) : v.classes.includes(cls)));
  // silence needs the whole tail; every other reading shows without it
  const tail = cls === 'not-silent' || cls === 'grey tail' ? 'full' : 'short';
  const fails = async (lists, notes) => (await assess(ctx, adapter, space, lists.map(items => withItems(space, c, items, tail, notes)))).verdicts.map(same);
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
const newTally = () => ({ cases: 0, kinds: { static: 0, sweep: 0, stack: 0, named: 0 }, samples: 0, nonFinite: 0, preNaN: 0,
  railCases: 0, reread: 0, offScale: 0, unread: 0, probed: 0, level: { v: 0, i: null, via: '', c: null }, growth: { v: -Infinity, i: null, win: null },
  asserted: 0, longest: 0, longestAsserted: 0, tail: { v: 0, i: null }, decay: { v: -Infinity, i: null },
  named: [], grey: [], greyCount: 0, fails: [], failCount: 0 });
function tally(t, cases, built, verdicts) {
  cases.forEach((c, k) => {
    const v = verdicts[k], b = built[k];
    t.cases++; t.kinds[c.kind]++; t.samples += v.samples; t.nonFinite += v.nonFinite; t.preNaN += v.preNaN;
    if (v.rail) t.railCases++;
    if (v.via === 'divided gain') t.reread++;
    if (v.offScale) t.offScale++;
    if (v.unread) t.unread++;
    if (!v.offScale && !v.unread && v.level > t.level.v) t.level = { v: v.level, i: c.i, via: v.via, c };
    if (v.growth !== undefined) { t.probed++; if (v.growth > t.growth.v) t.growth = { v: v.growth, i: c.i, win: v.win }; }
    if (b.release > t.longest) t.longest = b.release;
    if (b.silenceFrom !== null) {
      t.asserted++;
      if (b.release > t.longestAsserted) t.longestAsserted = b.release;
      if (v.tailPeak > t.tail.v) t.tail = { v: v.tailPeak, i: c.i };
    }
    if (v.decay && v.decay[FLOOR_INDEX] > t.decay.v) t.decay = { v: v.decay[FLOOR_INDEX], i: c.i };
    if (c.kind === 'named') t.named.push({ c, v });
    if (v.grey.length && t.greyCount++ < KEEP_FAILS) t.grey.push({ c, v });
    if (v.classes.length && t.failCount++ < KEEP_FAILS) t.fails.push({ c, v });
  });
}
const winText = w => (w ? `${w[0].map(x => x.toPrecision(4)).join(' then ')} held ${PROBE_S} s longer, ${w[1].map(x => x.toPrecision(4)).join(' then ')} held ${2 * PROBE_S} s longer` : '');
const verdictText = v => (v.crash ? `the render process died on ${v.crash}` :
  [v.nonFinite ? `${v.nonFinite} non-finite output samples` : '', v.preNaN ? `${v.preNaN} bus samples unreadable` : '',
    v.offScale ? `bus level OFF SCALE: on the rail even at 1 / 2^40 of the gain (${v.blowBy})` : v.unread ? 'bus level UNREAD: on the rail, and no gain key to divide' :
      `bus level ${v.level.toPrecision(5)} (read: ${v.via})`,
    v.rail ? `${v.rail} samples on the rail in the direct render` : '',
    v.growth !== undefined ? `growth ${v.growth.toPrecision(4)} (last two quarters: ${winText(v.win)})` : '',
    v.latePeak !== null ? `tail peak ${v.tailPeak.toExponential(2)}, late ${v.latePeak.toExponential(2)}` : 'tail not judged',
    v.unknownKeys ? `${v.unknownKeys} unknown keys, first "${v.firstUnknown}"` : ''].filter(Boolean).join('; '));

/* ================================================================== the linearity control
 * The reading through the divided gain is only as good as the claim that the bus is
 * linear in gain. So on every run: cases that sit well UNDER the rail, where the bus
 * can be read directly, are rendered both ways and compared sample by sample. The
 * difference is given as a share of the direct peak. */
async function linearity(ctx, adapter, space, seed, count) {
  const rows = [];
  for (let i = 0; i < count; i++) {
    const c = Object.assign(makeCase(space, seed, i), { tail: 'short' }), d = dividedGain(space, c);
    const bc = build(space, c), bd = build(space, d);
    const [rc, rd] = await renderBatch([jobOf(adapter, c, bc), jobOf(adapter, d, bd)], ctx.bin);
    let peak = 0, worst = 0, usable = true;
    for (const [x, y] of [[rc.preL, rd.preL], [rc.preR, rd.preR]]) for (let k = 0; k < x.length; k++) {
      const a = Math.abs(x[k]), e = Math.abs(y[k] * READ_FACTOR - x[k]);
      if (!(a < Infinity) || !(e < Infinity)) { usable = false; break; }
      if (a > peak) peak = a;
      if (e > worst) worst = e;
    }
    if (usable && peak >= LIN_FROM && peak <= LIN_TO) rows.push({ i, peak, err: worst / peak });
  }
  return rows;
}

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
  const T = THRESHOLDS, sameList = (x, y) => x.length === y.length && x.every(k => y.includes(k));

  if (o.replay) {
    const { adapter, space, c } = decode(JSON.parse(o.replay));
    const v = (await assess(ctx, adapter, space, [c])).verdicts[0];
    say(`REPLAY ${adapter}: ${v.classes.length ? v.classes.join(' + ').toUpperCase() : 'clean'}${v.grey.length ? ` (grey: ${v.grey.join(', ')})` : ''}; ${verdictText(v)}; ${diffText(space, c)}`);
    return v.classes.length ? 1 : 0;
  }

  const engines = enrolled(), table = listAdapters(o).map(a => a.id);
  for (const id of [...engines, CLEAN, ...PLANTS.map(p => p.id)]) if (!table.includes(id)) throw new Error(`h2_fuzz_check: ${ctx.bin} has no adapter "${id}"; rebuild h2_harness_render`);
  if (!engines.length) throw new Error('h2_fuzz_check: no engine is enrolled; a fuzz of nothing must not read green');
  const spaces = Object.fromEntries(engines.map(e => [e, loadSpace(e)]));
  const cases = e => (o.cases !== null ? o.cases : o.deep ? Math.max(DEEP_CASES, prCases(spaces[e])) : prCases(spaces[e]));
  for (const e of engines) if (cases(e) < minCases(spaces[e])) usage(`${cases(e)} cases cannot reach every key of "${e}": its table needs at least ${minCases(spaces[e])}`);
  say(`h2_fuzz_check: seed 0x${o.seed.toString(16).toUpperCase()} (${o.seed}), ${o.deep ? 'deep' : o.cases !== null ? 'custom' : 'PR-size'} run, ${engines.map(e => `${cases(e)} cases of ${e}`).join(', ')}, ${o.procs} render processes`);

  if (o.measure) {
    // A MEASUREMENT, NOT A VERDICT: the numbers the thresholds table is built from. The
    // growth probe runs on EVERY case here, so the worst growth of a healthy engine is known.
    for (const e of engines) {
      const space = spaces[e], t = newTally(), N = cases(e);
      for (let a = 0; a < N; a += SLAB) {
        const cs = Array.from({ length: Math.min(SLAB, N - a) }, (_, k) => makeCase(space, o.seed, a + k));
        const { verdicts, built } = await assess(ctx, e, space, cs, { measure: MEASURE_EXTRA_S, growAll: true });
        tally(t, cs, built, verdicts);
      }
      say(`MEASURED [${e}] ${t.cases} cases, ${t.samples} samples: true bus level, worst ${t.level.v.toPrecision(6)} (case ${t.level.i}, read: ${t.level.via}); on the rail in the direct render ${t.railCases} cases; read through the divided gain ${t.reread}; off scale ${t.offScale}; unread ${t.unread}; non-finite samples ${t.nonFinite + t.preNaN}`);
      say(`MEASURED [${e}] seconds from the release law's end until the output stays at or under silentFloor (${T.silentFloor.toExponential(3)}), worst ${t.decay.v.toFixed(4)} (case ${t.decay.i})` +
        (t.decay.v >= MEASURE_EXTRA_S - 1e-9 ? ` -- NEVER within the ${MEASURE_EXTRA_S} s rendered` : ''));
      say(`MEASURED [${e}] growth when the loudest stretch is held longer, over all ${t.probed} cases probed: worst ${t.growth.v.toPrecision(5)} (case ${t.growth.i}; last two quarters: ${winText(t.growth.win)})`);
      if (space.cfg.gain) {
        const lin = await linearity(ctx, e, space, o.seed, Math.min(N, 200));
        const w = lin.reduce((m, r) => (m === null || r.err > m.err ? r : m), null);
        say(`MEASURED [${e}] linearity in gain, ${lin.length} of the first ${Math.min(N, 200)} cases (direct peak ${LIN_FROM} to ${LIN_TO}): worst difference ${w ? w.err.toExponential(3) : 'none'} of the peak${w ? ` (case ${w.i}, peak ${w.peak.toPrecision(4)})` : ''}`);
      }
      if (t.level.c) {
        // the loudest case, cut down to what keeps it within 10 % of that level
        const m = await minimise(ctx, e, space, t.level.c, null, v => !v.crash && !v.offScale && v.level >= 0.9 * t.level.v);
        const vm = (await assess(ctx, e, space, [m.c])).verdicts[0];
        say(`MEASURED [${e}] the loudest case, minimised while it stays within 10 % (${m.minimised ? `${m.items} item(s), ${m.c.notes.length} note(s)` : 'as found'}): ${verdictText(vm)}; ${diffText(space, m.c)}\n    REPRO ${replayLine(space, e, m.c)}`);
      }
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
    const v = (await assess(ctx, adapter, space, [c])).verdicts[0];
    if (v.classes.length) problems.push(`KEPT ${k.id} fails again: ${v.classes.join(' + ')}; ${verdictText(v)}\n    REPRO ${replayLine(space, adapter, c)}`);
  }
  say(`KEPT  ${kept.length} kept case(s) replayed first (${KEPT})`);

  let judged = 0;
  for (const e of engines) {
    const space = spaces[e], N = cases(e), t = newTally(), withControls = e === CONTROL_ENGINE;
    const plants = PLANTS.map(p => ({ p, predicted: 0, found: 0, other: 0, near: 0, via: {}, wrong: [], first: null })), clean = { bad: [] };
    let st = null;
    for (let a = 0; a < N; a += SLAB) {
      const cs = Array.from({ length: Math.min(SLAB, N - a) }, (_, k) => makeCase(space, o.seed, a + k));
      const { verdicts, built } = await assess(ctx, e, space, cs);
      tally(t, cs, built, verdicts);
      st = coverage(space, cs, built, st);
      if (!withControls) continue;
      for (const q of plants) {
        const r = await assess(ctx, q.p.id, space, cs);
        cs.forEach((c, k) => {
          const want = planted(space, q.p, c, r.built[k]), v = r.verdicts[k];
          if (want.near) { q.near++; return; }
          const right = sameList(v.classes, want.classes) && sameList(v.grey, want.grey) &&
            (want.level === undefined || (!v.offScale && Math.abs(v.level - want.level) <= NEAR * want.level));
          if (want.classes.length) q.predicted++; else if (want.grey.length) q.other++;
          if (v.classes.includes(q.p.expect)) { q.found++; if (v.blowBy) q.via[v.blowBy] = (q.via[v.blowBy] || 0) + 1; if (!q.first) q.first = { c, v }; }
          if (!right && q.wrong.length < 5) q.wrong.push(`case ${c.i}: planted ${[...want.classes, ...want.grey.map(g => 'grey ' + g)].join(' + ') || 'nothing'}${want.level !== undefined ? ` at level ${want.level.toPrecision(6)}` : ''}, read ${[...v.classes, ...v.grey.map(g => 'grey ' + g)].join(' + ') || 'clean'} (${verdictText(v)})`);
        });
      }
      const r = await assess(ctx, CLEAN, space, cs);
      cs.forEach((c, k) => { const v = r.verdicts[k]; if ((v.classes.length || v.grey.length) && clean.bad.length < 5) clean.bad.push(`case ${c.i} read ${[...v.classes, ...v.grey].join(' + ')}`); });
    }
    judged += t.cases;

    // ---- the engine's own verdict
    say(`[${e}] ${t.cases} cases (${t.kinds.static} static, ${t.kinds.sweep} sweeps, ${t.kinds.stack} stacked changes, ${t.kinds.named} named), ${t.samples} samples judged`);
    say(`[${e}] NON-FINITE  ${t.nonFinite} output samples${t.preNaN ? ` and ${t.preNaN} unreadable bus samples` : ''} (the engine NaN count, risk row R3)`);
    say(`[${e}] LEVEL  true bus peak, worst ${t.level.v.toPrecision(5)} at case ${t.level.i} (read: ${t.level.via}); pass to ${T.prePass}, grey to ${T.preFail}, above it too loud. ` +
      `On the rail in the direct render: ${t.railCases} case(s); read through the gain divided by 2^40: ${t.reread}; off scale: ${t.offScale}; level unread: ${t.unread}`);
    say(`[${e}] GROWTH  held longer and probed (level above ${T.prePass}): ${t.probed} case(s)${t.probed ? `; worst growth ${t.growth.v.toPrecision(4)} at case ${t.growth.i}` : ''} (steady to ${T.growPass}, grey to ${T.growFail}, above it a blow-up)`);
    for (const x of t.named) say(`[${e}] NAMED ${x.c.name}  ${x.v.classes.length ? x.v.classes.join(' + ').toUpperCase() : x.v.grey.length ? 'grey ' + x.v.grey.join(', ') : 'pass'}: ${verdictText(x.v)}`);
    if (t.named.length !== (space.cfg.named || []).length) problems.push(`[${e}] ${(space.cfg.named || []).length} named case(s) are declared and ${t.named.length} were read`);
    say(`[${e}] SILENCE  judged in ${t.asserted} of ${t.cases} cases (the rest have a tail cut short of the release law, counted not judged); longest release judged ${t.longestAsserted.toFixed(2)} s of ${t.longest.toFixed(2)} s; worst tail peak ${t.tail.v.toExponential(2)}${t.tail.i !== null ? ` at case ${t.tail.i}` : ''} (floor ${T.silentFloor.toExponential(2)})`);
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
    if (space.cfg.gain) {
      // the reading through the divided gain must agree with the direct one where both can be had
      const lin = await linearity(ctx, e, space, o.seed, 2 + space.maxOpts);
      const w = lin.reduce((m, r) => (m === null || r.err > m.err ? r : m), null);
      const ok = lin.length >= LIN_MIN && lin.every(r => r.err <= T.linTol);
      if (!ok) problems.push(`[${e}] CONTROL linearity: ${lin.length < LIN_MIN ? `only ${lin.length} case(s) could be compared, want ${LIN_MIN}` : `case ${w.i} differs by ${w.err.toExponential(3)} of its peak, allowed ${T.linTol}`}: the reading through the divided gain cannot be trusted`);
      say(`[${e}] CONTROL linearity  ${lin.length} preamble case(s) under the rail (direct peaks ${lin.map(r => r.peak.toPrecision(3)).join(', ')}) rendered again with gain / 2^40 and scaled back, every sample compared: worst difference ${w ? w.err.toExponential(2) : 'none'} of the peak${w ? ` (case ${w.i})` : ''}, allowed ${T.linTol}${ok ? '' : ' -- FAILED'}`);
      // ...and the comparison must be able to say no: control/tone's bus ignores gain entirely
      const not = await linearity(ctx, CLEAN, space, o.seed, 3), caught = not.length > 0 && not.every(r => r.err > T.linTol);
      if (!caught) problems.push(`[${e}] CONTROL linearity (must fail): ${CLEAN}, whose bus does not follow gain, was not reported as non-linear`);
      say(`[${e}] CONTROL linearity (must fail)  ${CLEAN}, whose bus ignores gain, compared the same way over ${not.length} case(s): ${caught ? `differs by ${Math.min(...not.map(r => r.err)).toExponential(2)} of the peak or more, reported` : 'NOT reported'}`);
    } else say(`[${e}] CONTROL linearity  NOT RUN: this engine declares no gain key, so a level on the rail is counted as unread`);
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
      const where = cornerText(space, q.p), what = q.p.kind === 'growing' ? `a level from ${q.p.start} x gain that doubles every ${q.p.doubleS} s` : q.p.kind === 'steady' ? `a steady level of ${q.p.level} x gain` : q.p.kind === 'nan' ? 'a NaN' : 'a tone that never stops';
      if (q.wrong.length) problems.push(`CONTROL ${q.p.id}: the reading does not match the plant: ${q.wrong.join('; ')}`);
      if (!q.found) { problems.push(`CONTROL ${q.p.id}: the planted ${q.p.expect} at ${where} was NOT FOUND in ${t.cases} cases`); say(`CONTROL ${q.p.id}  NOT FOUND (${where})`); continue; }
      const m = await minimise(ctx, q.p.id, space, q.first.c, q.first.v);
      // the minimised case must be the corner and nothing else
      const final = Object.assign({}, m.c.base), touched = new Set(space.rows.filter(r => m.c.base[r.key] !== r.def).map(r => r.key));
      for (const ev of m.c.events) for (const [k, v] of ev.sets) { final[k] = v; touched.add(k); }
      const want = Object.keys(q.p.corner), exact = m.minimised && touched.size === want.length && want.every(k => touched.has(k) && atEnd(space.byKey[k], q.p.corner[k], final[k]));
      if (!exact) problems.push(`CONTROL ${q.p.id}: minimised to "${diffText(space, m.c)}", which is not the corner (${where})`);
      const how = Object.entries(q.via).map(([k, n]) => `${n} by ${k}`).join(', ');
      say(`CONTROL ${q.p.id}  planted ${what} at ${where}: read as ${q.p.expect.toUpperCase()} in ${q.found} of ${t.cases} cases, ${q.wrong.length ? 'NOT ' : ''}exactly the ${q.predicted} it was planted in` +
        (q.p.kind === 'growing' ? ` (${how})` : '') + (q.other ? `; ${q.other} more read grey, as planted at their gain` : '') + (q.near ? `; ${q.near} within ${NEAR} of a band's edge, not judged` : '') +
        `; first at case ${q.first.c.i} (within the first ${q.first.c.i + 1} cases)`);
      say(`    first: ${verdictText(q.first.v)}`);
      say(`    minimised to ${m.items} item(s), ${m.c.notes.length} note(s): ${diffText(space, m.c)}${exact ? ' -- the corner and nothing else' : ' -- NOT the corner'}\n    REPRO ${replayLine(space, q.p.id, m.c)}`);
      // the growth windows must decide at least one live case, or that rule ran on nothing
      if (q.p.kind === 'growing' && !q.via['the growth windows']) problems.push(`CONTROL ${q.p.id}: no case was read as growing by the growth windows (all ${q.found} were off scale), so the window rule was not exercised`);
    }
    if (clean.bad.length) problems.push(`CONTROL ${CLEAN}: a clean adapter was reported: ${clean.bad.join('; ')}`);
    say(`CONTROL ${CLEAN}  must read clean over the same ${t.cases} cases: ${clean.bad.length ? 'REPORTED ' + clean.bad.join('; ') : 'nothing reported'}`);
    // an undeclared key must be reported, or "no unknown key" above proves nothing
    const c0 = makeCase(space, o.seed, 0), b0 = build(space, c0);
    b0.cmds.unshift(['set', 'h2fuzz.no-such-key', 1]);
    const vk = read((await renderBatch([jobOf(e, c0, b0)], ctx.bin))[0], b0);
    if (!vk.unknownKeys) problems.push(`CONTROL unknown-key: a key the engine does not have was not reported on "${e}"`);
    say(`CONTROL unknown-key  a key no table declares, sent to ${e}: ${vk.unknownKeys ? `reported ("${vk.firstUnknown}")` : 'NOT reported'}`);
  }
  if (!engines.includes(CONTROL_ENGINE)) problems.push(`the controls are written in the table of "${CONTROL_ENGINE}", which is not enrolled: no control ran`);

  printThresholds(say);
  for (const p of problems) console.error(`h2_fuzz_check: RED  ${p}`);
  say(`h2_fuzz_check: ${problems.length ? 'FAILED' : 'GREEN'} (${judged} cases over ${engines.length} enrolled engine(s): ${engines.join(', ')}; ${ctx.renders} renders with the controls, the re-reads, the probes and the minimising; ${problems.length} problem(s))`);
  if (o.deep) say(`KIT-GATE cases=${judged}`);
  return problems.length ? 1 : 0;
}

if (import.meta.url === `file://${process.argv[1]}`) {
  main(process.argv.slice(2)).then(code => { process.exitCode = code; }, e => { console.error(e.message || e); process.exitCode = 2; });
}
