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
 * never the JS one) and judges EVERY sample of BOTH channels:
 *   - no output sample is non-finite, held or released (the count printed is R3's
 *     engine NaN count);
 *   - the pre-limiter bus level is read, however loud, and judged: a steady level
 *     against three bands, a level that keeps growing as a BLOW-UP. The engine ends
 *     in tanh, which would hide both, so the bus is read, not the output;
 *   - after the last key-off the output falls silent in a stated time, and stays so
 *     to the end of the render;
 *   - the engine refuses no key the table declares.
 * THE LEVEL BANDS CATCH A RUNAWAY, NOT A LEVEL REGRESSION: both hang off the one
 * loudest chord found, so an engine with every patch twice as loud still passes.
 *
 * THE LAST LINE IS THE VERDICT. Whatever happens, the last line this tool writes to
 * standard output is `h2_fuzz_check: GREEN (...)` or `h2_fuzz_check: RED (...)`,
 * with the counts, and a run that ends without having judged one case is RED. So a
 * caller can tell a pass from a run that never happened: it reads the last line.
 * (--measure and --replay are not gate runs and end in their own line.)
 *
 * THE CASES. Case i is a pure function of (table, seed, i, the run's size):
 *   static   a drawn patch, 1 to 4 notes at drawn velocities (0.1 to 1 each, a full
 *            chord included) held 0.25 to 1 s, then released.
 *   sweep    the same, with ONE key moved in steps during the held note: end to end,
 *            back, or between two drawn values; an option list steps through every
 *            option.
 *   stack    the same, with 2 to 8 keys changed in the SAME block, at 1 to 3 moments.
 * The rate (44.1, 48 or 96 kHz), the host block size and the host seed are drawn per
 * case. A PREAMBLE comes first, so that reaching every extreme does not depend on
 * luck: every continuous key at its minimum, then at its maximum, then one case per
 * option index, then gain at its own minimum, then the engine's NAMED cases, which
 * are played on every run whatever the seed. After it the cases rotate sweep, stack,
 * static, and the swept or stacked key rotates through the table: a ROUND is 3 x
 * (keys) cases, in which every key that can vary is swept once and leads a stacked
 * change once.
 *   A SECOND NOTE. One case in RESTRIKE_EVERY strikes a note after the first frame:
 *   either on top of the held ones, or after every held note has been released (a
 *   gap). Keys that act at a note-on (rotation sync, glide between notes) can only
 *   show there.
 *   GAIN STAYS AUDIBLE. gain scales the whole bus, so at its minimum, 0, a case can
 *   show nothing but a non-finite sample. Gain is therefore kept off its minimum
 *   except in the cases that test gain itself: its own preamble case, its sweeps and
 *   the stacked changes it leads. Everywhere else a draw that would land on the
 *   minimum is drawn inside the range instead, so every other key's extremes are
 *   judged with sound on the bus.
 *   LONG HOLDS AND FULL TAILS go by the case's place in its round plus the round's
 *   number, so over LONG_EVERY (or FULL_TAIL_EVERY) rounds every key's sweep, stack
 *   and static place gets one. That does not depend on the table's size, where a
 *   fixed stride could fall in step with the round and pick the same places for ever.
 *
 * WHAT A CASE COULD SHOW IS COUNTED, on every run, from the render itself:
 *   level, growth   only if the bus was above zero at some frame;
 *   silence         only if the tail was rendered to the end of the release AND the
 *                   output was above silentFloor in the last WINDOW_S before the
 *                   last key-off: a voice that simply kept going would then have
 *                   failed. "Judged for silence" counts those cases and no others.
 * control/silent, which can show nothing, is run through the same counts and must
 * read zero in both. And a case that does NOT test gain's minimum whose bus never
 * left zero is RED: with keys held and gain up, a bus of exact zeros is a case that
 * judged nothing.
 *
 * READING ABOVE THE RAIL. The harness recovers the bus as atanh(output). From about
 * 19 the output is exactly +-1 and the bus reads infinity: the rail. A loud chord
 * gets there without anything being wrong (the named case `loud-chord` reads 21.37),
 * so "it touched the rail" says nothing. When a case touches the rail, or peaks
 * above READ_ABOVE where atanh is already losing digits, the SAME case is rendered
 * again with every gain value it sets (the patch's and each change's during the
 * note) divided by READ_FACTOR = 2^40, and the reading is multiplied back. That
 * reading is the case's true level.
 *   WHY THAT IS VALID. h2/engine/engine.h's last two lines are
 *       L[i] = std::tanh(yl * s.gain * 1.6);   R[i] = std::tanh(yr * s.gain * 1.6);
 *   s.gain is written only by snap() and by the 16-sample glide `s += (t - s) * a`,
 *   which is linear in the values set. If nothing before those lines reads gain, the
 *   bus is the gain values times something gain cannot reach. 2^40 is a power of
 *   two, so the division, the glide and the multiplication back are exact in binary
 *   floating point; only tanh and atanh round.
 *   IT IS CHECKED IN EVERY CASE IT IS USED IN, at no extra render. The direct render
 *   and the reading describe the same frames, so each direct OUTPUT sample must be
 *   tanh(reading) to within readUlps units in the last place: three roundings, the
 *   engine's tanh, the harness's atanh and this file's tanh, each good to about one
 *   unit. A sample on the rail is held to the same rule (the reading must then be
 *   on the rail too, within those units). One sample outside it and the case is a
 *   MISREAD, red, named, and its level is not believed. control/fuzz-keyed is an
 *   adapter whose bus is multiplied by 8 above a level of 10: it must be reported as
 *   misread in every case that takes it there. The same comparison is made over the
 *   preamble cases under the rail (CONTROL linearity), and on control/tone, whose
 *   bus ignores gain, it must fail.
 *   THE SOURCE IS ALSO READ, as a second line: one more read of the gain member
 *   (`.gain` or `->gain`) anywhere under h2/engine/ stops the run. That guard is
 *   TEXTUAL and can be evaded (a pointer to member reads gain without either
 *   spelling); the sample-by-sample check above is the one that cannot.
 *   ITS LIMITS. A bus still on the rail at 1 / 2^40 of the gain is above 2e13: OFF
 *   SCALE, and called a blow-up. An engine that declares no gain key cannot be read
 *   this way: a case on its rail is counted as LEVEL UNREAD, printed, and red. A
 *   stage that only acts on samples the direct render has on the rail AND the
 *   reading puts above about 18 cannot be told from the limiter. The voice cap's
 *   fade passes samples through a float (adapter_composed.h); the harness does not
 *   expose the cap.
 *
 * LOUD VERSUS GROWING. A level that is finite and steady or periodic, however loud,
 * is a LEVEL: pass to prePass, grey to preFail, TOO LOUD above it. A level that keeps
 * growing is a BLOW-UP. The held note is cut into STRETCHES by the case's changes.
 *   WHICH CASES ARE PROBED. In the stretch that holds the case's peak, the bus peak
 *   of the last quarter is set against the quarter before it. A case is probed when
 *   that ratio is above riseProbe, WHATEVER its level, or when its level is above
 *   prePass.
 *   THE PROBE renders the case twice more, with that stretch made longer by
 *   max(PROBE_S, PROBE_SHARE of its own length) and by twice that. GROWTH is the
 *   smallest of three ratios: last quarter over the one before, at each length, and
 *   the long hold's last quarter over the short hold's. Steady to growPass, grey to
 *   growFail, a BLOW-UP above it. A level off scale in any of these is a blow-up.
 *   WHY THE SMALLEST OF THREE. One burst in one window (a beating chord), a linear
 *   attack and a level that has stopped rising each leave at least one ratio small.
 *   An exponential rise makes all three large. control/fuzz-burst is the plant that
 *   needs it: periodic bursts, which any one of the three ratios alone calls a
 *   blow-up in some case and the smallest never does.
 *   ITS REACH, printed on every run (GROWTH REACH) and exercised by two plants that
 *   START AT A HEALTHY LEVEL. For a level doubling every T seconds in a stretch of
 *   length s, held e longer: the trigger needs 2^(s / 4T) > riseProbe and the verdict
 *   needs 2^(min((s + e) / 4, e) / T) > growFail. With the numbers of this file:
 *     a plain hold (s up to 1 s, e = 0.5 s) decides T up to 0.16 s;
 *     a long hold of the PR-size run (4 s, e = 1.33 s) decides T up to 0.57 s;
 *     a long hold of the deep run (12 s, e = 4 s) decides T up to 1.7 s.
 *   One case in LONG_EVERY has a long hold. control/fuzz-blowup doubles every 40 ms
 *   and must be read in every case that holds its corner long enough to trigger.
 *   control/fuzz-slow doubles every 400 ms: no plain hold decides that, and neither
 *   does a long hold probed for only PROBE_S. It must be read in the PR-size run as
 *   well as the deep one, in the long holds whose stretch is 2.8 s or more. Nothing
 *   plants a fault between 0.57 s and the deep run's 1.7 s: the deep run's reach is
 *   the same rule at a longer hold, and is arithmetic, not a judged control. For
 *   each of the two plants the run prints the shortest time in the corner at which
 *   it was read and the longest at which it was not.
 *   ITS LIMITS. A rise slower than the reach above reads as a level, and is then
 *   judged as one (the soak, P6, holds for hours). A fault that grows and then STOPS
 *   under preFail is a level too: control/fuzz-plateau must read grey, never red,
 *   in both sizes. Only the stretch that holds the case's peak is looked at, so a
 *   fault growing in a quieter stretch (gain was turned down after it) is not probed.
 *   The probe holds the STATE the peak was in: a fault that only grows WHILE a key is
 *   being moved (during a sweep) is not followed.
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
 * from its source. A value counts as SENT only if it was in force while a frame
 * rendered: of several values sent for one key with no frame between them, the last.
 * (The mapped row b1on rewrites w, so a drawn w that b1on then overwrites was never
 * sent.) Counted that way, the run is RED if any key was never set, any continuous
 * key was never sent its declared minimum or its declared maximum, any option was
 * never sent, or any key that can vary was never swept or never led a stacked change.
 *   SENT IS NOT THE SAME AS HEARD, so each key is also put in one of three classes:
 *   HAS AN EFFECT   on a fixed sample of patches (the same on every run, whatever
 *                   the seed), moving ONLY this key between its extremes, or through
 *                   its options, changed the output. The patches strike a second note
 *                   after a gap. Only these keys are counted as reached.
 *   NOT READ        declared, and the engine's source never reads its member (a
 *                   textual test, checked against the effect test: a key it calls
 *                   unread that has an effect stops the run).
 *   NO EFFECT SEEN  read by the source, but no patch of the sample showed a
 *                   difference. Printed by name, not counted as reached, and RED:
 *                   its extremes were sent and nothing shows they were heard.
 *   control/tone, which ignores every key, must show no effect for any.
 *
 * THE CONTROLS, on every run (h2/harness/control_fuzz.h). The same cases go through
 * ten adapters that each carry one planted fault in one corner of the same table,
 * every fault on the RIGHT channel only:
 *   control/fuzz-blowup    a healthy level that doubles every 40 ms: BLOW-UP;
 *   control/fuzz-slow      the same every 400 ms: BLOW-UP, in a long hold;
 *   control/fuzz-plateau   doubles every 40 ms, then stops at 64 x gain: a level,
 *                          grey in its band, and NOT a blow-up;
 *   control/fuzz-burst     periodic bursts of 12 times the level: CLEAN, and at
 *                          least once only because the growth is the smallest ratio;
 *   control/fuzz-loud      a steady level of 4096 x gain, over 200 times the rail:
 *                          a level, AT that number, and NOT a blow-up;
 *   control/fuzz-keyed     a bus multiplied by 8 above 10: MISREAD;
 *   control/fuzz-nan       a NaN while a note is held: non-finite;
 *   control/fuzz-nan-tail  a NaN only after the release: non-finite;
 *   control/fuzz-hang      a level that never stops after the release: not-silent;
 *   control/fuzz-late      a level that stops 0.65 s after the release, so it is
 *                          silent at the end of the render and not before: not-silent.
 * For each, planted() works out from the case alone, in closed form, what the plant
 * must have rendered and what the rules of this header make of it. planted() shares
 * NO code with the judge (read, assess, growthOf, verdictOf): a change to the judge
 * moves the reading and not the prediction. The run is RED unless every case reads
 * exactly as predicted: the fault where it was planted, nothing where it was not, the
 * right KIND, and for a level the right number. The first failing case of each plant
 * must minimise to its corner (and gain, where the plant needs it loud) and nothing
 * else. The same cases through control/tone must report nothing. A key no table
 * declares must be reported. The key report must go red when one key's counts are
 * taken away.
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
 *            PR_ROUNDS rounds: every key swept and stacked that many times. Long
 *            holds are LONG_HOLD_S.
 *   deep     ... --deep                                                   (the nightly)
 *            DEEP_CASES cases, long holds of DEEP_LONG_HOLD_S, and the line
 *            `KIT-GATE cases=<n>`. RED unless every key's sweep, stacked change and
 *            static place had a full-length tail (it needs FULL_TAIL_EVERY rounds).
 *   --cases N sets the count (never below one full round: a run that cannot reach
 *   every key is refused, not judged). --seed N replaces the seed; it is printed.
 *   --measure prints the numbers the thresholds table is built from and judges nothing.
 *
 * RUN TIME, measured 2026-10-11 on the dev Mac (8 cores, 4 render processes; a
 * stopwatch held outside this tool, which reads no clock). PR-size, 1022 cases with
 * the effect test, the ten plants, the re-reads, the probes and the minimising: 70.7
 * and 71.8 s with the machine's load average between 3 and 5 (169 and 172 s of CPU),
 * and 84.9 and 89.4 s with it between 4 and 6 (196 and 217 s). The budget is two
 * minutes. A deep run of 2702 cases (8 rounds, 12 s long holds) took 235 s: 87 ms a
 * case, so the deep run's 20000 cases are about 29 minutes here, where the build
 * before this one needed 11 to 14. The ten plants read every case again and are
 * most of that. A hosted 4-core runner is ASSUMED four to six times slower (not
 * measured: no nightly has run), which makes the deep run about two to three hours
 * there. The count is one number in tools/nightly/jobs.json, beside its floor.
 *
 * DETERMINISM. mulberry32 streams from the printed seed, nothing else. No clock is
 * read anywhere in this file, so no verdict can depend on time taken; --procs changes
 * how many render processes run side by side and, in the output, only the first
 * line, which states it.
 *
 * WHAT IT DOES NOT COVER (each is printed or stated, none is hidden):
 *   - Engine keys with NO declared range (21 on 2026-10-11: bend, poly, grav, ...).
 *     The table lists them as fixed, so there is nothing to draw from. The run prints
 *     them as a HOLE every time. Declaring their ranges is the table owner's edit.
 *   - kCustom, the one string row: the table declares one value. It is set, never varied.
 *   - A level REGRESSION: see "THE LEVEL BANDS" at the top.
 *   - Growth slower than the printed reach, growth in a stretch that is not the
 *     case's loudest, and growth that needs a key to keep moving: see "ITS LIMITS"
 *     under LOUD VERSUS GROWING.
 *   - Silence is judged where the tail is rendered to the end of the release: every
 *     case whose release is short, and the full-tail cases whatever their release.
 *     The rest render SHORT_TAIL_S of release (finite and bounded are still judged)
 *     and are counted as not judged for silence, as is a case that was already under
 *     the floor when its keys came up.
 *   - Notes are plain: one extra note-on at most, no hostile note events, no note
 *     churn (the soak, P6), no rate-by-block matrix (the invariance matrix, P4).
 *   - The effect test says a key CAN change the output, on a small sample. It does
 *     not say the change is right, or that every option differs from every other.
 *   - THIS FILE'S WIRING CLAIM IS NOT CROSS-CHECKED. tools/test_table_check.py reads
 *     tools/*_check.cpp, tools/*_check.py and tools/labharness/*_check.mjs. It does
 *     not read tools/*.mjs, so it would not notice if the line above were false.
 */
import { spawn } from 'node:child_process';
import { existsSync, mkdtempSync, readdirSync, readFileSync, realpathSync, rmSync, writeFileSync } from 'node:fs';
import { availableParallelism, tmpdir } from 'node:os';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
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
 * 12000 cases of `composed` as this file makes them now (gain kept audible, a second
 * note in one case of four, long holds of 4 s), 8,252,855,692 samples, nothing
 * non-finite, nothing off scale. --measure probes EVERY case for growth; a normal run
 * probes the risers and the loud.
 *   level      In both runs the loudest case was the named `loud-chord`, 21.368. Of the
 *              11998 DRAWN cases, one and three peaked above 8 and none reached it.
 *   growth     worst 1.9480 (seed 20261011, case 4371) and 1.5919 (seed 46152, case 3439):
 *              what a healthy engine does when its loudest stretch is held longer.
 *   rise       172 and 166 of 6000 cases rose by more than riseProbe from the third
 *              quarter of the peak's stretch to the last (2.8 %): the cases a normal
 *              run probes. NOT MEASURED: growth over the deep run's 12 s holds.
 *   reading    a direct output sample against tanh(its reading through the divided
 *              gain): worst 1.750 and 1.749 units in the last place over 178 and 175
 *              cases compared in full, and 1.694 in the named chord. These three are
 *              from --measure --cases 400 at both seeds, run after the comparison was
 *              given its floor for subnormal samples (the two long runs divided by
 *              zero there and printed Infinity).
 *   silence    seconds from the release law's end to under silentFloor, worst 0.1955
 *              (seed 46152, case 1788) and 0.1950 (seed 20261011, case 1209).
 * THE VALUES BELOW WERE NOT MOVED TO FOLLOW THESE. The growth and settle lines were set
 * from an earlier build's worsts (1.6904 and 0.2022 s) at x1.5 and x3. The cases have
 * changed since, and the healthy worst growth is now 1.9480, so the growth lines stand
 * at x1.30 and x2.61 of it. Following the rule would RAISE them to 2.92 and 5.84, and
 * loosening a line is the human's ruling, not this file's. Each row states the margin
 * it really has.
 *
 * THE BANDS.
 *   level     the TRUE bus peak. At or under prePass: pass. To preFail: GREY. Above
 *             it: FAIL (too loud). The loudest chord found reads 21.368 at full gain,
 *             and that is the measured worst: every band sits above it. Both bands
 *             are multiples of that ONE chord, so they catch a runaway and not a
 *             level regression.
 *   growth    of a probed case, held longer. To growPass: steady. To growFail: GREY.
 *             Above it, or off scale: FAIL (blow-up). riseProbe is not a band: it
 *             decides which cases are probed at all.
 *   silence   under silentFloor from the release law's end + settlePass: pass. Under
 *             it only from + settleFail: GREY. Still above it then: FAIL.
 *   reading   one line: a direct output sample further than readUlps units in the
 *             last place from tanh(the reading through the divided gain) is a misread.
 * A non-finite sample and an unknown key have no band. One is red.
 *
 * WHY THESE MARGINS (argued, for the human to change). Level: x2 is 6 dB over the
 * loudest chord found and x4 is 12 dB. Growth: the rule was x1.5 and x3 over the
 * healthy worst (see above for where the lines stand now); the planted blow-up reads
 * 8.7 or more. The tail after a release is the engine's DC blocker ringing down
 * (measured: 60 dB per 0.14 s), so x1.5 on the time is about 44 dB more decay than
 * the worst case needed. Reading: three roundings of about one unit each are the
 * whole budget on a platform whose libm is good to a unit or two, and a stage that
 * depends on the level shows far outside it (control/fuzz-keyed: 114 units at the
 * least, at full gain, and millions at lower gains).
 */
const THRESHOLD_TABLE = [
  { name: 'prePass', value: 42.8, what: 'true pre-limiter bus peak, the top of the pass band',
    worst: 'measured worst 21.368 (the named case loud-chord, held 6 s)', margin: 'x2 (6 dB)', date: '2026-10-11' },
  { name: 'preFail', value: 85.5, what: 'true pre-limiter bus peak above which a steady level fails as too loud',
    worst: 'measured worst 21.368 (the same)', margin: 'x4 (12 dB)', date: '2026-10-11' },
  { name: 'riseProbe', value: 1.5, what: 'a case is probed for growth when, in the stretch that holds its peak, the last quarter is this many times the quarter before (or its level is above prePass)',
    worst: 'not a worst: 172 and 166 of 6000 healthy cases rose by more than it (2.8 %), and those are the cases probed', margin: 'none: a trigger, not a verdict', date: '2026-10-11' },
  { name: 'growPass', value: 2.54, what: 'growth of a probed case held longer, the top of the steady band',
    worst: 'measured worst 1.9480 (seed 20261011, case 4371, over 12000 cases, every one probed); the line is 1.5 x an EARLIER worst, 1.6904, and was not raised', margin: 'x1.30 as it stands (x1.5 would be 2.92)', date: '2026-10-11' },
  { name: 'growFail', value: 5.08, what: 'growth above which a probed case is a blow-up',
    worst: 'measured worst 1.9480 (the same)', margin: 'x2.61 as it stands (x3 would be 5.84)', date: '2026-10-11' },
  { name: 'offScale', value: 'the rail at gain / 2^40', what: 'a level the divided gain cannot bring off the rail (above 2e13) is a blow-up',
    worst: 'not this tool\'s: the adapter\'s limit of sight (h2/harness/README.md: 19 is under the rail, 19.2 on it) times READ_FACTOR', margin: 'none', date: '2026-10-11' },
  { name: 'readUlps', value: 8, what: 'the reading through the divided gain: the most a direct output sample may differ from tanh(reading), in units in the last place (2^-52 of the larger)',
    worst: 'measured worst 1.750 (seed 20261011, case 98, of 353 cases compared in full)', margin: 'x4.6', date: '2026-10-11' },
  { name: 'settlePass', value: 0.304, what: 'seconds after the release law\'s end by which the output is under silentFloor',
    worst: 'measured worst 0.1955 (seed 46152, case 1788); the line is 1.5 x an earlier worst, 0.2022', margin: 'x1.55 as it stands', date: '2026-10-11' },
  { name: 'settleFail', value: 0.607, what: 'seconds after the release law\'s end past which a tail above silentFloor fails',
    worst: 'measured worst 0.1955 (the same)', margin: 'x3.1 as it stands', date: '2026-10-11' },
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
const LONG_EVERY = 16;          // one place in this many has a long hold (see longHold)
const LONG_HOLD_S = 4, DEEP_LONG_HOLD_S = 12;             // a long hold, PR-size and deep
const RESTRIKE_EVERY = 4;       // one case in this many strikes a second note after the first frame
const MIN_SEGMENT = 256;        // frames between two moments of a case: a stretch has four quarters, and each needs frames
const WINDOW_S = 0.05;          // silence is asserted over this long a stretch, and "audible at the release" is read over it
const TAIL_CAP_S = 1.0;         // a tail longer than this is rendered in full only in the full-tail cases
const SHORT_TAIL_S = 0.25;      // otherwise this much release is rendered and silence is NOT asserted (counted)
const FULL_TAIL_EVERY = 8;      // one place in this many has a full-length tail (see fullTail)
const EDGE_END = 0.7;           // edge mode: a continuous key lands on an end with this probability (the gauntlet's 0.7)
const EDGE_SHARE = 2 / 3;       // of the drawn base patches, this share are edge mode; the rest are broad
const CHUNK_FRAMES = 1 << 20;   // jobs are grouped into render processes of about this many frames
const MINIMISE_PER_ENGINE = 3;  // failing cases minimised per adapter (the rest are listed, not minimised)
// Reading above the rail (see "READING ABOVE THE RAIL" in the header). A power of two, so
// dividing every gain value by it and multiplying the reading back are exact in binary.
const READ_FACTOR = 2 ** 40;
const READ_ABOVE = 8;           // a direct bus peak above this is re-read through the divided gain: atanh loses
                                // precision toward the rail (h2/harness/README.md: 4.5e-9 at 10, 0.13 at 18.5)
const PROBE_S = 0.5;            // the growth probe holds the loudest stretch at least this much longer, and twice that
const PROBE_SHARE = 1 / 3;      // ...or this share of the stretch's own length, if that is more
const LIN_FROM = 0.05, LIN_TO = 6, LIN_MIN = 4;   // the linearity control: which direct peaks it compares, and how many at least
const EFFECT_SEED = 0xEFFEC7;   // the effect test's patches: fixed, so a key's class does not move with --seed
const EFFECT_TRIES = 64;        // patches tried per key before it is called "no effect seen"
const EFFECT_HOLD_S = 0.12;     // each is held this long, with every note released at half and one struck at three quarters

/* ================================================================== what each engine declares
 * One entry per ENROLLED engine. An enrolled engine with no entry is red: the fuzz
 * cannot claim a key list it was not given.
 *   table     the declared parameter table: every key with its range, options and default
 *   mapped    rows of the table that are not engine keys, and what they send instead
 *   cell      sets after `snap` (the reference cell's: h2/harness/patches.json)
 *   keys      reads the engine's own key list out of its source, so a key the engine
 *             has and the table does not know is found, not assumed away
 *   notRead   keys the engine declares and its source never reads (textual)
 *   release   seconds from the last key-off to a freed voice, from the values a case
 *             gave the release key. seen(key) is every value the case set.
 *   gain      the key the output stage is linear in, and `limit`, that stage restated:
 *             output = limit(bus). Together they are what reading above the rail needs. */
const engineSource = () => { const dir = join(ROOT, 'h2/engine'); return readdirSync(dir).filter(n => n.endsWith('.h')).sort().map(f => readFileSync(join(dir, f), 'utf8')); };
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
    // A key row is {"key", &SP::member, ...} or {"key", &DP::member}; a READ of it is
    // `.member` or `->member` somewhere in h2/engine/*.h. No such text: not read. Textual, so
    // it can only err toward "read" (a short member name matches other things); the effect
    // test is what says whether a key the source does read can be heard.
    notRead() {
      const all = engineSource().join('\n');
      return [...all.matchAll(/\{"([^"]+)", &(?:SP|DP)::(\w+)(?:, (?:true|false))?\}/g)].filter(m => !new RegExp(`(\\.|->)${m[2]}\\b`).test(all)).map(m => m[1]);
    },
    // engine.h: in release `env -= env * rC`, rC = 1 - exp(-4 / (R ms as samples)), and the
    // voice is freed below 1e-4. From an envelope of at most 1 that is R * ln(1e4) / 4.
    // A case's LARGEST R is used: the engine glides toward a new R, so a release that
    // starts just after R was lowered still runs at nearly the old one.
    release: seen => Math.max(...seen('R')) / 1000 * Math.log(1e4) / 4,
    gain: 'gain',
    limit: Math.tanh,
    gainIsLast() {
      // Every read of the gain member in h2/engine/*.h (`.gain` or `->gain`, whatever the
      // struct is called there) must be one of the limiter's two lines. One more read
      // anywhere is a stage that depends on gain, and the reading must be argued again.
      // TEXTUAL: a read through a pointer to member has neither spelling. The check that
      // cannot be evaded is the sample-by-sample one in read() (`against`).
      const reads = engineSource().flatMap(s => s.split('\n')).filter(l => /(\.|->)gain\b/.test(l));
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
  const gainRow = rows.find(r => r.key === cfg.gain);
  if (cfg.gain && !(gainRow && gainRow.kind === 'c' && !gainRow.mapped && gainRow.def > gainRow.min && cfg.limit)) throw new Error(`h2_fuzz_check: ${cfg.table} has no continuous row "${cfg.gain}" with a default above its minimum (and a limit) to read the level through`);
  return { engine, cfg, rows, fuzz, byKey: Object.fromEntries(rows.map(r => [r.key, r])),
    unranged: (tree.unsampled || []).map(u => u.key),
    maxOpts: Math.max(...rows.filter(r => r.opts).map(r => r.opts.length)),
    defaults: Object.fromEntries(rows.map(r => [r.key, r.def])) };
}
/* The preamble, then ROUNDS of one sweep, one stack and one static case per key that can
   vary. The fewest cases that reach everything by construction is the preamble and one round. */
const roundLen = space => 3 * space.fuzz.length;
export const preambleCases = space => 2 + space.maxOpts + (space.cfg.gain ? 1 : 0) + (space.cfg.named || []).length;
export const minCases = space => preambleCases(space) + roundLen(space);
export const prCases = space => preambleCases(space) + PR_ROUNDS * roundLen(space);
/* Which places of which rounds get a full-length tail, and which a long hold. A place is a
   case's position in its round; adding the round's number walks the choice through every
   place, so after FULL_TAIL_EVERY (or LONG_EVERY) rounds each has had one, whatever the
   round's length. (A fixed stride of 8 over a round of 336 chose the same 42 places for
   ever.) The two never fall on the same case: 4 is not a multiple of 8. */
const fullTail = (place, round) => (place + round) % FULL_TAIL_EVERY === 0;
const longHold = (place, round) => (place + round) % LONG_EVERY === 4;

function interior(row, p) {
  const v = TAPERS[row.taper](row, p);
  // a rounding overshoot is put back ON the declared end, so every input stays in range
  return v < row.min ? row.min : v > row.max ? row.max : v;
}
/* One key's value from two uniform draws. broad: uniform in taper position. edge: a
   continuous key lands on a declared END with probability EDGE_END (either end, evenly).
   Enumerations and toggles are uniform over their options; a string keeps its default.
   offMin (gain, outside the cases that test gain): a draw that lands on the minimum is
   drawn inside the range instead, from the same two numbers. */
function drawValue(row, mode, u, v, offMin) {
  if (row.kind === 's') return row.def;
  if (row.kind !== 'c') return row.opts[Math.min(row.opts.length - 1, Math.floor(v * row.opts.length))];
  const x = mode === 'edge' && u < EDGE_END ? (v < 0.5 ? row.min : row.max) : interior(row, v);
  if (!offMin || x > row.min) return x;
  const y = interior(row, 2 * v % 1);
  return y > row.min ? y : row.def;
}
/* A whole patch: one substream per row (the gauntlet's scheme), so a new row in the
   table moves no other row's draw. */
function drawPatch(space, cs, mode, offMinKey) {
  const V = {};
  for (const row of space.rows) { const g = mulberry32(hash32(cs, 1, row.index)); const u = g(), v = g(); V[row.key] = drawValue(row, mode, u, v, row.key === offMinKey); }
  return V;
}

/* ================================================================== a case
 * { i, kind, lead, place, long, own, sr, block, seed, notes [[note, vel]], hold (frames),
 *   base { key: value }, events [{ at, sets [[key, value]], off?, on? }],
 *   tail 'auto' | 'full' | 'short' }
 * An event is a MOMENT of the held note: its sets are sent, then (off) every held note
 * is released, then (on: [note, vel]) one note is struck. The moments cut the hold into
 * STRETCHES. `own` marks the cases that test gain itself, where gain may sit at its
 * minimum. A case is a pure function of (space, seed, i, opt.longS). Everything but
 * the patch comes from one stream, drawn in a fixed order; the second note has its own
 * stream, so it moves no other draw. */
export function makeCase(space, seed, i, opt) {
  const cs = hash32(seed, 2, i), g = mulberry32(hash32(cs, 3)), pre = preambleCases(space);
  const sr = pick(g, SR_CHOICES), block = pick(g, BLOCK_CHOICES);
  const hold = Math.round(HOLD_MIN_S * Math.pow(2, HOLD_OCTAVES * g()) * sr);
  const count = 1 + Math.floor(g() * MAX_NOTES), notes = [];
  while (notes.length < count) { const n = NOTE_LO + Math.floor(g() * (NOTE_HI - NOTE_LO + 1)); if (!notes.some(x => x[0] === n)) notes.push([n, 0.1 + 0.9 * g()]); }
  const c = { i, kind: 'static', lead: null, place: null, long: false, own: false, sr, block, seed: hash32(cs, 4), notes, hold, base: null, events: [], tail: 'auto' };
  const mode = g() < EDGE_SHARE ? 'edge' : 'broad', gain = space.cfg.gain || null;

  if (i < pre) {
    // THE PREAMBLE: coverage by construction, so no key's extremes depend on luck.
    //   0  every continuous key at its declared minimum, every option list at its first;
    //      gain alone at its DEFAULT, so the other minimums are judged with sound on the bus
    //   1  every continuous key at its declared maximum, every option list at its last
    //   2+k  an edge patch with every enumeration at option k (mod its length)
    //   then, if the engine has a gain key, case 0 again with gain at its minimum too
    //   then the engine's NAMED cases, exactly as written, whatever the seed
    const gainMin = gain !== null && i === 2 + space.maxOpts, firstNamed = 2 + space.maxOpts + (gain ? 1 : 0);
    if (i >= firstNamed) { const named = space.cfg.named[i - firstNamed]; return Object.assign(decode(named.case).c, { i, kind: 'named', name: named.name }); }
    c.base = drawPatch(space, cs, 'edge', gain); c.own = gainMin;
    for (const r of space.rows) {
      if (r.kind === 's') continue;
      if (i === 0 || gainMin) c.base[r.key] = r.kind === 'c' ? r.min : r.opts[0];
      else if (i === 1) c.base[r.key] = r.kind === 'c' ? r.max : r.opts[r.opts.length - 1];
      else if (r.opts) c.base[r.key] = r.opts[(i - 2) % r.opts.length];
    }
    if (i === 0 && gain) c.base[gain] = space.byKey[gain].def;
    if (i === 1) c.tail = 'full';   // the longest release the table allows is judged for silence on every run
    return restrike(c, cs);
  }
  const j = i - pre, round = Math.floor(j / roundLen(space)), place = j % roundLen(space), lead = space.fuzz[Math.floor(place / 3)];
  c.kind = ['sweep', 'stack', 'static'][place % 3]; c.place = lead.key;
  // gain is free to reach its minimum only in the cases that are about gain
  const own = c.own = c.kind !== 'static' && lead.key === gain;
  c.base = drawPatch(space, cs, mode, own ? null : gain);
  if (fullTail(place, round)) c.tail = 'full';
  if (longHold(place, round)) { c.long = true; c.hold = Math.round(((opt && opt.longS) || LONG_HOLD_S) * sr); }
  if (c.kind === 'static') return restrike(c, cs);
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
    c.events = values.map((v, k) => ({ at: Math.round(c.hold * (k + 1) / (values.length + 1)), sets: [[lead.key, v]] }));
  } else {
    // several keys changed in the same block, at one to three moments
    const moments = 1 + Math.floor(g() * 3);
    for (let m = 0; m < moments; m++) {
      const n = 2 + Math.floor(g() * 7), rows = [lead];
      while (rows.length < n) { const r = pick(g, space.fuzz); if (!rows.includes(r)) rows.push(r); }
      c.events.push({ at: Math.round(c.hold * (m + 1) / (moments + 1)), sets: rows.map(r => [r.key, drawValue(r, 'edge', g(), g(), !own && r.key === gain)]) });
    }
  }
  return restrike(c, cs);
}
/* A second note, in one case of RESTRIKE_EVERY. ADDED: struck in the middle of a drawn
   stretch, over the notes already held. After a GAP: every held note is released in the
   middle of a drawn stretch, and the note is struck in the middle of a later one. A
   middle is always MIN_SEGMENT clear of its neighbours (checked: the last line). */
function restrike(c, cs) {
  const g = mulberry32(hash32(cs, 5)), take = Math.floor(g() * RESTRIKE_EVERY) === 0;
  const gap = g() < 0.5, u1 = g(), u2 = g(), vel = 0.1 + 0.9 * g();
  let note = NOTE_LO + Math.floor(g() * (NOTE_HI - NOTE_LO + 1));
  const edges = () => [0, ...c.events.map(e => e.at), c.hold];
  const middle = s => { const e = edges(); return Math.floor((e[s] + e[s + 1]) / 2); };
  const put = ev => { c.events.push(ev); c.events.sort((a, b) => a.at - b.at); return c.events.indexOf(ev); };
  if (take && gap) {
    const k = put({ at: middle(Math.floor(u1 * (c.events.length + 1))), sets: [], off: true });
    // the stretches after the release: from the one it opened to the last
    put({ at: middle(k + 1 + Math.floor(u2 * (c.events.length - k))), sets: [], on: [note, vel] });
  } else if (take) {
    while (c.notes.some(x => x[0] === note)) note = note < NOTE_HI ? note + 1 : NOTE_LO;   // a key not already down
    put({ at: middle(Math.floor(u1 * (c.events.length + 1))), sets: [], on: [note, vel] });
  }
  const e = edges();
  for (let k = 1; k < e.length; k++) if (e[k] - e[k - 1] < MIN_SEGMENT) throw new Error(`h2_fuzz_check: case ${c.i}: a stretch of ${e[k] - e[k - 1]} frames is under MIN_SEGMENT`);
  return c;
}

/* ================================================================== a case as a harness job */
/* The commands, and what the judge needs: the stretches of the held note, where the last
   keys came off, and from which frame the render must be silent (null when this case's
   tail is too short to say). */
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
  let held = c.notes.map(x => x[0]), at = 0;
  for (const [n, vel] of c.notes) cmds.push(['on', n, mtof(n), vel]);
  const stretches = [];
  for (const e of c.events) {
    cmds.push(['render', e.at - at]); stretches.push([at, e.at]); at = e.at;
    for (const [k, v] of e.sets) cmds.push(...setsOf(k, v));
    if (e.off) { for (const n of held) cmds.push(['off', n]); held = []; }
    if (e.on) { cmds.push(['on', e.on[0], mtof(e.on[0]), e.on[1]]); held.push(e.on[0]); }
  }
  cmds.push(['render', c.hold - at]); stretches.push([at, c.hold]);
  for (const n of held) cmds.push(['off', n]);
  const seen = key => [c.base[key], ...c.events.flatMap(e => e.sets.filter(s => s[0] === key).map(s => s[1]))];
  const release = cfg.release(seen), window = Math.round(WINDOW_S * c.sr);
  // silence is owed from the release law's end plus settlePass; settleFail is the later line
  const passAt = Math.ceil((release + THRESHOLDS.settlePass) * c.sr), failAt = Math.ceil((release + THRESHOLDS.settleFail) * c.sr);
  let asserted = c.tail === 'full' || (c.tail === 'auto' && failAt + window <= Math.round(TAIL_CAP_S * c.sr));
  // `shortTail` (frames) is the effect test's: its renders only need the release to begin
  let tail = asserted ? failAt + window : c.shortTail !== undefined ? c.shortTail : Math.round(SHORT_TAIL_S * c.sr);
  // --measure: no verdict; render well past the release law and time the decay instead
  if (opt && opt.measure) { asserted = false; tail = Math.ceil((release + opt.measure) * c.sr); }
  cmds.push(['render', tail]);
  return { cmds, frames: c.hold + tail, off: c.hold, window, stretches, release, releaseFrames: release * c.sr,
    silenceFrom: asserted ? c.hold + passAt : null, lateFrom: asserted ? c.hold + failAt : null,
    windows: c.windows || null, scale: c.scale || 1 };
}
const jobOf = (adapter, c, b) => ({ name: `case ${c.i}`, engine: adapter, sr: c.sr, seed: c.seed,
  block: typeof c.block === 'object' ? c.block.vary : c.block, vary: typeof c.block === 'object', pre: true, cmds: b.cmds });

/* ================================================================== reading one render
 * read() turns one render into FACTS and decides nothing; verdictOf() decides, once
 * the level has been read above the rail and a rising or loud case has been probed for
 * growth (assess, below). EVERY sample of BOTH channels is read, held and released.
 *
 * `b.scale` is what the bus must be multiplied by: 1 for a direct render, READ_FACTOR
 * for a replay whose gain values were divided by it. `against`, given with such a
 * replay, is the direct render's two OUTPUT channels: the reading is checked against
 * them sample by sample (see "IT IS CHECKED IN EVERY CASE IT IS USED IN"). */
const ratio = (a, b) => (b > 0 ? a / b : a > 0 ? Infinity : 1);   // a ratio over nothing is Infinity if anything is there, 1 if not
const ULP = 2 ** -52;
export function read(r, b, opt, against) {
  if (r.crash) return { crash: r.crash, nonFinite: 0, preNaN: 0, rail: 0, prePeak: 0, peakAt: 0, stretch: 0, rise: 1, tailPeak: null, latePeak: null, samples: 0, win: null, canLevel: false, canSilence: false };
  if (r.L.length !== b.frames) throw new Error(`h2_fuzz_check: ${r.name}: rendered ${r.L.length} frames, asked for ${b.frames}`);
  if (!r.preL) throw new Error(`h2_fuzz_check: ${r.name}: the render carries no pre-limiter bus`);
  const n = b.frames, v = { nonFinite: 0, preNaN: 0, rail: 0, prePeak: 0, peakAt: 0, tailPeak: null, latePeak: null, samples: 2 * n, win: null,
    unknownKeys: r.summary.unknownKeys, firstUnknown: r.summary.firstUnknown, digest: r.summary.digest };
  for (const [y, p] of [[r.L, r.preL], [r.R, r.preR]]) for (let k = 0; k < n; k++) {
    if (!Number.isFinite(y[k])) { v.nonFinite++; continue; }
    const a = Math.abs(p[k]);
    if (a !== a) v.preNaN++;
    else if (a === Infinity) v.rail++;
    else if (a > v.prePeak) { v.prePeak = a; v.peakAt = k; }
  }
  // a bus that never left zero can show no level and no growth: counted, per case
  v.canLevel = v.rail > 0 || v.prePeak > 0;
  if (opt && opt.keep && (v.rail || v.prePeak > READ_ABOVE)) v.out = [r.L, r.R];
  v.prePeak *= b.scale;
  // the bus peak of a run of frames, both channels; frames on the rail read Infinity, a NaN drops out
  const peakOf = (from, to) => { let m = 0; for (const p of [r.preL, r.preR]) for (let k = from; k < to; k++) { const a = Math.abs(p[k]); if (a > m) m = a; } return m * b.scale; };
  // the stretch that holds the peak (the last, if the peak came after the release) and how
  // its last quarter stands to the quarter before: the growth probe's trigger
  v.stretch = b.stretches.length - 1;
  for (let s = 0; s < b.stretches.length; s++) if (v.peakAt >= b.stretches[s][0] && v.peakAt < b.stretches[s][1]) { v.stretch = s; break; }
  { const [from, to] = b.stretches[v.stretch], q = Math.floor((to - from) / 4); v.rise = ratio(peakOf(to - q, to), peakOf(to - 2 * q, to - q)); }
  if (b.windows) v.win = b.windows.map(([from, to]) => peakOf(from, to));
  {
    // was there sound when the keys came up? If not, a voice that never freed would be silent too
    let near = 0;
    for (const y of [r.L, r.R]) for (let k = Math.max(0, b.off - b.window); k < b.off; k++) { const a = Math.abs(y[k]); if (a > near) near = a; }
    v.canSilence = b.silenceFrom !== null && near > THRESHOLDS.silentFloor;
  }
  if (b.silenceFrom !== null) {
    // the WHOLE tail from the settle time to the last frame, both channels. A NaN compares
    // false and drops out of these two maxima; the loop above has already counted it.
    v.tailPeak = v.latePeak = 0;
    for (const y of [r.L, r.R]) for (let k = b.silenceFrom; k < n; k++) {
      const a = Math.abs(y[k]);
      if (a > v.tailPeak) v.tailPeak = a;
      if (k >= b.lateFrom && a > v.latePeak) v.latePeak = a;
    }
  }
  if (against) {
    // Every direct output sample against limit(this reading), in units in the last place of the
    // larger. Under about 2e-296 the divided bus is subnormal and no longer exact; its step,
    // scaled back, is the unit there (a decaying voice gets that small; a level never does).
    const floor = Number.MIN_VALUE * b.scale;
    v.ulps = 0; v.ulpAt = 0;
    for (const [y, p] of [[against[0], r.preL], [against[1], r.preR]]) for (let k = 0; k < n; k++) {
      const d = y[k], t = b.limit(p[k] * b.scale);
      if (!(Math.abs(d) <= 1) || t !== t || t === d) continue;   // a non-finite sample is counted elsewhere
      const u = Math.abs(t - d) / Math.max(Math.max(Math.abs(d), Math.abs(t)) * ULP, floor);
      if (u > v.ulps) { v.ulps = u; v.ulpAt = k; }
    }
  }
  if (opt && opt.measure) {
    // seconds after the release law's end at which the output was last above the floor (negative: it was already under)
    let last = b.off - 1;
    for (const y of [r.L, r.R]) for (let k = n - 1; k > last; k--) if (!(Math.abs(y[k]) <= THRESHOLDS.silentFloor)) { last = k; break; }
    v.decay = (last + 1 - b.off - b.releaseFrames) / r.sr;
  }
  return v;
}

/* ================================================================== the verdict
 * From the facts of a case (v), after assess() has filled in:
 *   v.level     the TRUE bus peak: the direct reading, or the reading through the
 *               divided gain scaled back (v.via says which; v.blowBy says what made
 *               a blow-up one)
 *   v.ulps      for a case read through the divided gain: how far the direct render
 *               is from that reading, at its worst sample
 *   v.offScale  the bus was on the rail even with the gain divided by READ_FACTOR
 *   v.unread    the bus was on the rail and the engine declares no gain key to divide
 *   v.growth    for a probed case only: how the level grew when held longer (growthOf)
 *
 *   crash        the render process died on this case.
 *   non-finite   an output sample that is not a finite number, or a bus sample that
 *                is NaN under a finite output. The count is risk row R3's engine NaN count.
 *   misread      the reading through the divided gain does not describe the direct
 *                render (above readUlps): the bus is not linear in gain here.
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
  if (v.ulps > T.readUlps) v.classes.push('misread');
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
   before it (index 0) of the peak's stretch held longer (short), and held twice that
   longer (long). The growth is the SMALLEST of three ratios: the last quarter over the
   one before it at each length, and the long hold's last quarter over the short hold's.
   All three must be large for it to be large, so one loud burst in one window, a linear
   attack and a level that has stopped rising all read small, and an exponential rise
   reads large at every length. control/fuzz-burst is read wrongly by any other choice. */
export function growthOf(short, long) {
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
   at a time and only the job that dies again is charged with the crash.
   opt.against[k] is the direct render's output for case k (see read). */
export async function evaluate(ctx, adapter, space, cases, opt) {
  const built = cases.map(c => build(space, c, opt)), batches = [];
  let cur = [], frames = 0;
  built.forEach((b, k) => {
    b.limit = space.cfg.limit;
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
    idx.forEach((k, m) => { verdicts[k] = read(results[m], built[k], opt, opt && opt.against ? opt.against[k] : null); });
  });
  ctx.renders += cases.length;
  return { verdicts, built };
}

/* ================================================================== reading the level, then the verdict
 * 1. DIRECT. Every case is rendered as it is.
 * 2. ABOVE THE RAIL. A case whose bus touched the rail, or peaked above READ_ABOVE, is
 *    replayed with EVERY gain value it sets (the patch's and each change's) divided
 *    by READ_FACTOR, and the reading is multiplied back. Its level is that reading,
 *    and the reading is checked against the direct render's samples (read: against).
 *    Still on the rail there: off scale. No gain key declared: unread.
 * 3. GROWTH. A case whose peak stretch rises by more than riseProbe from its third
 *    quarter to its last, or whose level is above prePass, is replayed twice more with
 *    that stretch made probeExtra() and twice probeExtra() longer (later moments move
 *    back by as much), and growthOf() reads the last two quarters of each.
 *    `opt.growAll` probes every case (--measure). */
const dividedGain = (space, c) => {
  const k = space.cfg.gain, f = v => v / READ_FACTOR;
  return Object.assign({}, c, { scale: READ_FACTOR, base: Object.assign({}, c.base, { [k]: f(c.base[k]) }),
    events: c.events.map(e => Object.assign({}, e, { sets: e.sets.map(([key, v]) => (key === k ? [key, f(v)] : [key, v])) })) });
};
const edgesOf = c => [0, ...c.events.map(e => e.at), c.hold];
export const probeExtra = (len, sr) => Math.max(Math.round(PROBE_S * sr), Math.round(len * PROBE_SHARE));
/* the case with stretch `s` made `extra` frames longer, and the two windows to read */
function heldLonger(c, s, extra) {
  const edges = edgesOf(c), from = edges[s], to = edges[s + 1] + extra, q = Math.floor((to - from) / 4);
  return Object.assign({}, c, { hold: c.hold + extra, tail: 'short', windows: [[to - 2 * q, to - q], [to - q, to]],
    events: c.events.map(e => Object.assign({}, e, { at: e.at >= edges[s + 1] ? e.at + extra : e.at })) });
}
export async function assess(ctx, adapter, space, cases, opt) {
  const gain = space.cfg.gain, T = THRESHOLDS;
  const ev = await evaluate(ctx, adapter, space, cases, Object.assign({}, opt, { keep: !!gain })), vs = ev.verdicts;
  for (const v of vs) { v.level = v.prePeak; v.via = 'direct'; }
  // ---- above the rail
  const high = vs.map((v, k) => k).filter(k => !vs[k].crash && (vs[k].rail || vs[k].prePeak > READ_ABOVE));
  if (high.length && !gain) for (const k of high) { if (vs[k].rail) vs[k].unread = true; }
  else if (high.length) {
    // the same frames (so the same tail plan), with the direct outputs to hold the reading to
    const red = (await evaluate(ctx, adapter, space, high.map(k => dividedGain(space, cases[k])), Object.assign({}, opt, { keep: false, against: high.map(k => vs[k].out) }))).verdicts;
    high.forEach((k, m) => {
      const v = vs[k], r = red[m];
      delete v.out;
      if (r.crash) { v.crash = r.crash; return; }
      v.nonFinite += r.nonFinite; v.preNaN += r.preNaN; v.ulps = r.ulps; v.ulpAt = r.ulpAt;
      if (r.rail) { v.offScale = true; v.blowBy = 'off scale'; return; }
      v.level = r.prePeak; v.peakAt = r.peakAt; v.stretch = r.stretch; v.rise = r.rise; v.via = 'divided gain';
    });
  }
  // ---- growth
  const probe = vs.map((v, k) => k).filter(k => !vs[k].crash && !vs[k].offScale && !vs[k].unread && ((opt && opt.growAll) || vs[k].level > T.prePass || vs[k].rise > T.riseProbe));
  if (probe.length) {
    const probes = [];
    for (const k of probe) {
      const [from, to] = ev.built[k].stretches[vs[k].stretch];
      vs[k].extra = probeExtra(to - from, cases[k].sr);
      for (const mult of [1, 2]) { const c = heldLonger(cases[k], vs[k].stretch, mult * vs[k].extra); probes.push(gain ? dividedGain(space, c) : c); }
    }
    const pr = (await evaluate(ctx, adapter, space, probes)).verdicts;
    probe.forEach((k, m) => {
      const v = vs[k], a = pr[2 * m], b = pr[2 * m + 1];
      if (a.crash || b.crash) { v.crash = a.crash || b.crash; return; }
      v.nonFinite += a.nonFinite + b.nonFinite; v.preNaN += a.preNaN + b.preNaN;
      v.win = [a.win, b.win];
      // a window on the rail even through the divided gain: the level left the scale while it was held
      if ([...a.win, ...b.win].includes(Infinity)) { v.growth = Infinity; v.offScale = true; v.blowBy = 'off scale when held longer'; return; }
      v.growth = growthOf(a.win, b.win);
      if (v.growth > T.growFail) v.blowBy = 'the growth windows';
    });
  }
  for (const v of vs) verdictOf(v);
  return ev;
}

/* ================================================================== what the run reached
 * Counted from the COMMANDS that were sent, not from the plan that made them: a
 * sampler that quietly narrowed a range would still say it drew the full one. And a
 * value counts only if it was IN FORCE while a frame rendered: of several values sent
 * for one key with no render between them, the last. */
export function coverage(space, cases, built, st) {
  st = st || Object.fromEntries(space.rows.map(r => [r.key, { sets: 0, min: 0, max: 0, sweeps: 0, stacks: 0,
    opts: r.opts ? new Map(r.opts.map(o => [o, 0])) : null }]));
  cases.forEach((c, n) => {
    const inForce = new Map(space.rows.map(r => [r.key, new Set()])), sent = new Map(space.rows.map(r => [r.key, 0])), pending = new Map();
    for (const cmd of built[n].cmds) {
      if ((cmd[0] === 'set' || cmd[0] === 'sets') && inForce.has(cmd[1])) { pending.set(cmd[1], cmd[2]); sent.set(cmd[1], sent.get(cmd[1]) + 1); }
      else if (cmd[0] === 'render' && cmd[1] > 0) { for (const [k, v] of pending) inForce.get(k).add(v); pending.clear(); }
    }
    // a mapped row is not a command; its own values are the case's, each in force for a stretch
    for (const r of space.rows) if (r.mapped) {
      const vals = [c.base[r.key], ...c.events.flatMap(e => e.sets.filter(s => s[0] === r.key).map(s => s[1]))];
      inForce.set(r.key, new Set(vals)); sent.set(r.key, vals.length);
    }
    for (const r of space.rows) {
      const s = st[r.key], vs = inForce.get(r.key);
      s.sets += sent.get(r.key);
      if (r.kind === 'c') { if (vs.has(r.min)) s.min++; if (vs.has(r.max)) s.max++; }
      if (r.opts) for (const o of vs) if (s.opts.has(o)) s.opts.set(o, s.opts.get(o) + 1);
    }
    if (c.kind === 'sweep') st[c.lead].sweeps++;
    if (c.kind === 'stack') st[c.lead].stacks++;
  });
  return st;
}

/* ================================================================== the effect test
 * Sent is not heard. For each key that can vary: a fixed patch is rendered once per
 * extreme of the key (both ends of a range, or every option) with NOTHING else changed.
 * If the output digests differ, the key has an effect. The patch: broad draws, every
 * TOGGLE on (a switch that is off hides the keys behind it), gain at its default, two
 * notes, every note released at half the hold and one struck at three quarters. A key
 * with no effect on one patch is tried on the next, up to `tries`; the patches are taken
 * in stages of 1, 1, 2, 4, 8, ... so that the many keys that show at once cost one
 * render each and the few that need a rare combination do not cost a process each.
 * The patches do not depend on --seed. Returns Map(key -> the number of the first patch
 * that showed an effect, or null). */
function effectCase(space, m) {
  const cs = hash32(EFFECT_SEED, 7, m), sr = 48000, hold = Math.round(EFFECT_HOLD_S * sr), gain = space.cfg.gain;
  const base = drawPatch(space, cs, 'broad', gain);
  for (const r of space.rows) if (r.kind === 't') base[r.key] = 1;
  if (gain) base[gain] = space.byKey[gain].def;
  return { i: `effect ${m}`, kind: 'effect', lead: null, place: null, long: false, own: false, sr, block: 128, seed: hash32(cs, 4), notes: [[48, 0.8], [67, 0.6]], hold, base, tail: 'short', shortTail: Math.round(hold / 4),
    events: [{ at: Math.round(hold / 2), sets: [], off: true }, { at: Math.round(3 * hold / 4), sets: [], on: [55, 0.8] }] };
}
export async function effects(ctx, adapter, space, rows, tries) {
  const seen = new Map(rows.map(r => [r.key, null]));
  for (let from = 0, to = 1; from < tries; from = to, to = Math.min(tries, 2 * to)) {
    const left = rows.filter(r => seen.get(r.key) === null);
    if (!left.length) break;
    const cases = [], owner = [];
    for (let m = from; m < to; m++) {
      const c0 = effectCase(space, m);
      for (const r of left) for (const x of r.kind === 'c' ? [r.min, r.max] : r.opts) { cases.push(Object.assign({}, c0, { base: Object.assign({}, c0.base, { [r.key]: x }) })); owner.push([r.key, m]); }
    }
    const vs = (await evaluate(ctx, adapter, space, cases)).verdicts, digests = new Map();
    vs.forEach((v, k) => { const id = owner[k].join(' '); if (!digests.has(id)) digests.set(id, new Set()); digests.get(id).add(v.crash ? `crash ${k}` : v.digest); });
    for (const r of left) for (let m = to - 1; m >= from; m--) if (digests.get(`${r.key} ${m}`).size > 1) seen.set(r.key, m);
  }
  return seen;
}

/* ================================================================== the planted faults
 * h2/harness/control_fuzz.h. The corners are written in the table of CONTROL_ENGINE,
 * and the plants are run over that engine's own cases. Every number here restates a
 * constant of that file (PLANT_AMP its kAmp, PLANT_CAP its kLevelCap, PLANT_HANG its
 * kHangAmp, doubleS its kDoubleS or kSlowDoubleS, and so on).
 *   expect  the reading the plant exists to draw, which must be drawn at least once
 *   also    keys besides the corner that the smallest failing case may keep
 *           (a level that is only a fault when it is loud keeps gain)            */
const CONTROL_ENGINE = 'composed';
const CLEAN = 'control/tone', SILENT = 'control/silent';
const PLANT_AMP = 0.5, PLANT_CAP = 1e30, PLANT_HANG = 1e-3;
const PLANTS = [
  { id: 'control/fuzz-blowup', kind: 'level', expect: 'blow-up', corner: { fb: 'max', K: 'min' }, doubleS: 0.04, what: 'a healthy level that doubles every 0.04 s' },
  { id: 'control/fuzz-slow', kind: 'level', expect: 'blow-up', corner: { stretchB: 'max' }, doubleS: 0.4, what: 'a healthy level that doubles every 0.4 s' },
  { id: 'control/fuzz-plateau', kind: 'level', expect: 'grey level', corner: { colB: 'max', width: 'min' }, doubleS: 0.04, top: 64, also: ['gain'], what: 'a level that doubles every 0.04 s and stops at 64 x gain' },
  { id: 'control/fuzz-burst', kind: 'level', expect: 'clean', corner: { inertia: 'max', harmReach: 'min' }, burst: { times: 12, everyS: 0.35, lenS: 0.02 }, what: 'bursts of 12 times the healthy level, 0.02 s in every 0.35 s' },
  { id: 'control/fuzz-loud', kind: 'level', expect: 'too-loud', corner: { xm: 'max', kRuleAmt: 'min' }, steady: 4096, what: 'a steady level of 4096 x gain' },
  { id: 'control/fuzz-keyed', kind: 'keyed', expect: 'misread', corner: { spread: 'max', onset: 'min' }, steady: 16, above: 10, times: 8, also: ['gain'], what: 'a level of 16 x gain, multiplied by 8 where that is above 10' },
  { id: 'control/fuzz-nan', kind: 'nan', expect: 'non-finite', corner: { detune: 'max', S: 'min', aa: 0 }, what: 'a NaN while a note is held' },
  { id: 'control/fuzz-nan-tail', kind: 'nan-tail', expect: 'non-finite', corner: { hard: 'max', depth: 'min' }, what: 'a NaN only after the release' },
  { id: 'control/fuzz-hang', kind: 'hang', expect: 'not-silent', corner: { R: 'min', A: 'max' }, what: 'a level that never stops after the release' },
  { id: 'control/fuzz-late', kind: 'hang', expect: 'not-silent', corner: { R: 'min', D: 'max' }, lateS: 0.65, what: 'a level that stops 0.65 s after the release' },
];
const NEAR = 1e-9;     // a planted number this close (relatively) to a line may read either side: counted, not judged
// The rail, as a plant's prediction sees it: the bus reads infinity from about 19.06, and where exactly
// depends on the last bit of tanh. A planted level between these two is not judged either way.
const RAIL_UNDER = 18, RAIL_OVER = 19.2;
const atEnd = (row, end, v) => (end === 'max' ? v >= row.max : end === 'min' ? v <= row.min : v === end);
const cornerText = (space, plant) => Object.entries(plant.corner).map(([k, e]) => `${k} = ${e === 'max' ? space.byKey[k].max : e === 'min' ? space.byKey[k].min : e}${typeof e === 'string' ? ` (its ${e})` : ''}`).join(', ');
/* The largest right-channel level, at gain 1, over corner frames [a, b) of a level plant. */
function lawMax(plant, sr, a, b) {
  if (plant.steady) return plant.steady;
  if (plant.burst) {
    const every = Math.round(plant.burst.everyS * sr), len = Math.round(plant.burst.lenS * sr);
    // a burst fills the last `len` frames of each period
    for (let start = Math.floor(a / every) * every + every - len; start < b; start += every) if (start + len > a) return PLANT_AMP * plant.burst.times;
    return PLANT_AMP;
  }
  return Math.min(plant.top || PLANT_CAP, PLANT_AMP * Math.pow(2, (b - 1) / (plant.doubleS * sr)));
}
/* What must this plant have rendered for this case, and what do the rules of the header
   make of it? Worked out from the case alone, in closed form, sharing NO code with the
   judge: the thresholds are read, and every rule is restated here from the header. So a
   change to read(), assess(), growthOf() or verdictOf() moves the reading and not this.
   Returns { classes, grey, level?, near, entered, cornerS }: `near` means a planted
   number sits on a line and the case is not judged; `entered` that the case took the
   plant into its corner at all; `cornerS` the longest it stayed there, in seconds. */
export function planted(space, plant, c, b) {
  const T = THRESHOLDS, gk = space.cfg.gain, cur = Object.assign({}, c.base), st = [], rel = [];
  const inCorner = () => Object.entries(plant.corner).every(([k, e]) => atEnd(space.byKey[k], e, cur[k]));
  // the stretches of the held note, and the releases
  // `quiet`: frames with no note held since the last release that found the corner (null: none has)
  let from = 0, held = c.notes.length, n0 = 0, quiet = null;
  for (const e of [...c.events, { at: c.hold, sets: [], off: true }]) {
    const corner = held > 0 && inCorner();
    st.push({ len: e.at - from, held: held > 0, corner, gain: cur[gk], n0 });
    n0 = corner ? n0 + (e.at - from) : 0;
    if (held === 0 && quiet !== null) quiet += e.at - from;
    for (const [k, v] of e.sets) cur[k] = v;
    if (e.off) { if (held > 0) { rel.push({ at: e.at, corner: inCorner() }); if (inCorner()) quiet = 0; } held = 0; }
    if (e.on) held++;
    if (!(held > 0 && inCorner())) n0 = 0;
    from = e.at;
  }
  const out = { classes: [], grey: [], near: false, entered: st.some(x => x.corner) || rel.some(x => x.corner) };
  out.cornerS = Math.max(0, ...st.filter(x => x.corner).map(x => (x.n0 + x.len) / c.sr));
  const close = (x, line) => Math.abs(x - line) <= NEAR * Math.abs(line);
  if (plant.kind === 'nan') { if (st.some(x => x.corner)) out.classes.push('non-finite'); return out; }
  if (plant.kind === 'nan-tail') { if (rel.some(x => x.corner)) out.classes.push('non-finite'); return out; }
  if (plant.kind === 'hang') {
    // a stuck level of PLANT_HANG x gain from the release that found the corner; `lateS` ends it
    if (quiet === null || b.silenceFrom === null) return out;
    const amp = Math.tanh(PLANT_HANG * cur[gk]);
    if (close(amp, T.silentFloor)) { out.near = true; return out; }
    if (!(amp > T.silentFloor)) return out;
    // `lateS` is counted in frames with no note held, so a gap before the last release uses some of it up
    const ends = plant.lateS ? c.hold + Math.max(0, Math.round(plant.lateS * c.sr) - quiet) : Infinity;
    if (ends > b.lateFrom) out.classes.push('not-silent'); else if (ends > b.silenceFrom) out.grey.push('tail');
    out.quietEnd = ends <= b.frames - 64;
    return out;
  }
  if (plant.kind === 'keyed') {
    // the direct bus is 8 times the reading wherever level x gain is above `above`: a misread
    const top = Math.max(0, ...st.filter(x => x.corner).map(x => plant.steady * x.gain));
    if (close(top, plant.above) || close(top, READ_ABOVE)) out.near = true;
    else if (top > plant.above) out.classes.push('misread');
    return out;
  }
  // ---- a level plant. The peak of each stretch: the law's largest value in it, times its gain.
  const peak = st.map(x => x.gain * (x.corner ? lawMax(plant, c.sr, x.n0, x.n0 + x.len) : x.held ? PLANT_AMP : 0));
  const level = Math.max(...peak), s = peak.indexOf(level), x = st[s];
  out.level = level;
  if (!x.corner || !(level > 0)) return out;   // the loudest stretch is a healthy one: steady, and far under every line
  const offScale = v => { if (v > RAIL_UNDER * READ_FACTOR && v < RAIL_OVER * READ_FACTOR) out.near = true; return v >= RAIL_OVER * READ_FACTOR; };
  if (offScale(level)) { out.classes.push('blow-up'); delete out.level; return out; }
  // a level this far up puts even the divided reading where atanh loses digits: the kind is still judged, the number is not
  if (level > READ_ABOVE * READ_FACTOR) delete out.level;
  // probed? the last quarter of that stretch over the quarter before, or a level above prePass
  const q0 = Math.floor(x.len / 4), end = x.n0 + x.len;
  const rise = lawMax(plant, c.sr, end - q0, end) / lawMax(plant, c.sr, end - 2 * q0, end - q0);
  if (close(rise, T.riseProbe) || close(level, T.prePass) || close(level, T.preFail)) out.near = true;
  let growing = false, greyGrowth = false;
  if (rise > T.riseProbe || level > T.prePass) {
    // held longer by e and by 2e: the last two quarters of each, then the three ratios, EACH of which must be large
    const e = Math.max(Math.round(PROBE_S * c.sr), Math.round(x.len * PROBE_SHARE));
    const w = [1, 2].map(m => { const to = end + m * e, q = Math.floor((x.len + m * e) / 4); return [lawMax(plant, c.sr, to - 2 * q, to - q) * x.gain, lawMax(plant, c.sr, to - q, to) * x.gain]; });
    if (w.flat().map(offScale).some(Boolean)) growing = true;
    else {
      const three = [w[0][1] / w[0][0], w[1][1] / w[1][0], w[1][1] / w[0][1]];
      if (three.some(r => close(r, T.growFail) || close(r, T.growPass))) out.near = true;
      growing = three.every(r => r > T.growFail);
      greyGrowth = !growing && three.every(r => r > T.growPass);
    }
  }
  if (growing) out.classes.push('blow-up');
  else if (level > T.preFail) out.classes.push('too-loud');
  else if (level > T.prePass) out.grey.push('level');
  if (greyGrowth) out.grey.push('growth');
  return out;
}

/* ================================================================== minimising a failure
 * What a case has beyond the defaults is a list of ITEMS: a key whose starting value is
 * not the default, one change made during the note, or a moment's release or second
 * note. Delta debugging (ddmin) drops items while the same class of failure remains,
 * and stops when no single item can be dropped: 1-minimal, which is the fewest
 * parameters away from default unless two items only matter together with a third.
 * Then the notes are cut to one if one is enough. Every candidate is a real render. */
function itemsOf(space, c) {
  const items = [];
  for (const r of space.rows) if (c.base[r.key] !== r.def) items.push({ base: r.key });
  c.events.forEach((e, n) => { e.sets.forEach((_, m) => items.push({ ev: n, set: m })); if (e.off || e.on) items.push({ ev: n, note: true }); });
  return items;
}
function withItems(space, c, items, tail, notes) {
  const base = Object.assign({}, space.defaults);
  for (const it of items) if (it.base !== undefined) base[it.base] = c.base[it.base];
  const events = c.events.map((e, n) => {
    const ev = { at: e.at, sets: e.sets.filter((_, m) => items.some(it => it.ev === n && it.set === m)) };
    if (items.some(it => it.ev === n && it.note)) { if (e.off) ev.off = true; if (e.on) ev.on = e.on; }
    return ev;
  }).filter(e => e.sets.length || e.off || e.on);
  return Object.assign({}, c, { base, events, tail, notes: notes || c.notes });
}
const CLASS_ORDER = ['crash', 'non-finite', 'misread', 'blow-up', 'too-loud', 'level-unread', 'not-silent', 'unknown-key'];
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

/* ================================================================== one line that replays a case
 * An event is [frame, sets] or, with a note moment, [frame, sets, { off: true }] or
 * [frame, sets, { on: [note, velocity] }] (both may be given). */
export function encode(space, adapter, c) {
  const sets = {};
  for (const r of space.rows) if (c.base[r.key] !== r.def) sets[r.key] = c.base[r.key];
  return { v: 1, engine: adapter, table: space.engine, sr: c.sr, block: c.block, seed: c.seed, notes: c.notes, hold: c.hold, tail: c.tail,
    sets, events: c.events.map(e => (e.off || e.on ? [e.at, e.sets, Object.assign({}, e.off ? { off: true } : {}, e.on ? { on: e.on } : {})] : [e.at, e.sets])) };
}
export function decode(o) {
  if (!o || o.v !== 1) throw new Error('h2_fuzz_check: not a version 1 replay line');
  const space = loadSpace(o.table);
  const c = { i: 'replay', kind: 'replay', lead: null, place: null, long: false, own: false, sr: o.sr, block: o.block, seed: o.seed >>> 0, notes: o.notes, hold: o.hold, tail: o.tail,
    base: Object.assign({}, space.defaults, o.sets), events: (o.events || []).map(([at, sets, note]) => Object.assign({ at, sets }, note || {})) };
  return { adapter: o.engine, space, c };
}
const replayLine = (space, adapter, c) => `node tools/h2_fuzz_check.mjs --replay '${JSON.stringify(encode(space, adapter, c))}'`;
const diffText = (space, c) => {
  const d = space.rows.filter(r => c.base[r.key] !== r.def).map(r => `${r.key} = ${c.base[r.key]}`);
  const e = c.events.map(ev => `at frame ${ev.at}: ${[...ev.sets.map(([k, v]) => `${k} = ${v}`), ev.off ? 'every note released' : '', ev.on ? `note ${ev.on[0]} struck` : ''].filter(Boolean).join(', ')}`);
  return `${d.length ? d.join(', ') : 'every key at its default'}${e.length ? '; then ' + e.join('; ') : ''}`;
};

/* ================================================================== tallies */
const SLAB = 256;                 // cases made, rendered and judged at a time, so a deep run's memory does not grow
const KEEP_FAILS = 40;            // failing cases held for the report (all are counted)
const MEASURE_EXTRA_S = 1.5;      // --measure renders this far past the release law
/* The slowest doubling time a stretch of `len` frames can decide (see "ITS REACH" in the
   header): the trigger must fire and all three ratios must pass growFail. Seconds. */
const reachOf = (len, sr) => { const e = probeExtra(len, sr); return Math.min(Math.min(Math.floor((len + e) / 4), e) / Math.log2(THRESHOLDS.growFail), Math.floor(len / 4) / Math.log2(THRESHOLDS.riseProbe)) / sr; };
const newTally = () => ({ cases: 0, kinds: { static: 0, sweep: 0, stack: 0, named: 0 }, samples: 0, nonFinite: 0, preNaN: 0,
  added: 0, gapped: 0, long: 0, railCases: 0, reread: 0, ulps: { v: 0, i: null }, offScale: 0, unread: 0, probed: 0, risers: 0,
  level: { v: 0, i: null, via: '', c: null }, growth: { v: -Infinity, i: null, win: null, extra: 0, sr: 1 }, rise: { v: 0, i: null },
  canLevel: 0, muteOwn: 0, mute: [], asserted: 0, judged: 0, longest: 0, longestJudged: 0, tail: { v: 0, i: null }, decay: { v: -Infinity, i: null },
  reach: { v: 0, i: null, s: 0 }, plainReach: 0, full: { sweep: new Set(), stack: new Set(), static: new Set() },
  named: [], grey: [], greyCount: 0, fails: [], failCount: 0 });
function tally(t, cases, built, verdicts) {
  cases.forEach((c, k) => {
    const v = verdicts[k], b = built[k];
    t.cases++; t.kinds[c.kind]++; t.samples += v.samples; t.nonFinite += v.nonFinite; t.preNaN += v.preNaN;
    if (c.events.some(e => e.off)) t.gapped++; else if (c.events.some(e => e.on)) t.added++;
    if (c.long) t.long++;
    if (v.rail) t.railCases++;
    if (v.via === 'divided gain' || v.offScale) { t.reread++; if (v.ulps > t.ulps.v) t.ulps = { v: v.ulps, i: c.i }; }
    if (v.offScale) t.offScale++;
    if (v.unread) t.unread++;
    if (!v.offScale && !v.unread && v.level > t.level.v) t.level = { v: v.level, i: c.i, via: v.via, c };
    if (v.rise > THRESHOLDS.riseProbe) t.risers++;
    if (v.rise < Infinity && v.rise > t.rise.v) t.rise = { v: v.rise, i: c.i };
    if (v.growth !== undefined) { t.probed++; if (v.growth > t.growth.v) t.growth = { v: v.growth, i: c.i, win: v.win, extra: v.extra, sr: c.sr }; }
    if (v.canLevel) t.canLevel++; else { if (c.own) t.muteOwn++; else if (t.mute.length < 8) t.mute.push(c.i); }
    for (const [from, to] of b.stretches) {
      const r = reachOf(to - from, c.sr);
      if (r > t.reach.v) t.reach = { v: r, i: c.i, s: (to - from) / c.sr };
      if (!c.long && c.kind !== 'named' && r > t.plainReach) t.plainReach = r;
    }
    if (b.release > t.longest) t.longest = b.release;
    if (b.silenceFrom !== null) t.asserted++;
    if (v.canSilence) {
      t.judged++;
      if (b.release > t.longestJudged) t.longestJudged = b.release;
      if (v.tailPeak > t.tail.v) t.tail = { v: v.tailPeak, i: c.i };
    }
    if (c.tail === 'full' && c.place !== null) t.full[c.kind].add(c.place);
    if (v.decay !== undefined && v.decay > t.decay.v) t.decay = { v: v.decay, i: c.i };
    if (c.kind === 'named') t.named.push({ c, v });
    if (v.grey.length && t.greyCount++ < KEEP_FAILS) t.grey.push({ c, v });
    if (v.classes.length && t.failCount++ < KEEP_FAILS) t.fails.push({ c, v });
  });
}
const secs = (frames, sr) => `${(frames / sr).toPrecision(3)} s`;
const winText = (w, extra, sr) => (w ? `${w[0].map(x => x.toPrecision(4)).join(' then ')} held ${secs(extra, sr)} longer, ${w[1].map(x => x.toPrecision(4)).join(' then ')} held ${secs(2 * extra, sr)} longer` : '');
const verdictText = (v, c) => (v.crash ? `the render process died on ${v.crash}` :
  [v.nonFinite ? `${v.nonFinite} non-finite output samples` : '', v.preNaN ? `${v.preNaN} bus samples unreadable` : '',
    v.offScale ? `bus level OFF SCALE: on the rail even at 1 / 2^40 of the gain (${v.blowBy})` : v.unread ? 'bus level UNREAD: on the rail, and no gain key to divide' :
      `bus level ${v.level.toPrecision(5)} (read: ${v.via})`,
    v.rail ? `${v.rail} samples on the rail in the direct render` : '',
    v.ulps !== undefined ? `the direct render is within ${v.ulps.toPrecision(3)} units in the last place of that reading${v.ulps > THRESHOLDS.readUlps ? ` -- MISREAD, first worst at frame ${v.ulpAt}` : ''}` : '',
    v.growth !== undefined ? `growth ${v.growth.toPrecision(4)} (last two quarters: ${winText(v.win, v.extra, c.sr)})` : '',
    v.latePeak !== null ? `tail peak ${v.tailPeak.toExponential(2)}, late ${v.latePeak.toExponential(2)}${v.canSilence ? '' : ' (already under the floor at the release: could not have failed)'}` : 'tail not judged',
    v.unknownKeys ? `${v.unknownKeys} unknown keys, first "${v.firstUnknown}"` : ''].filter(Boolean).join('; '));

/* ================================================================== the linearity control
 * The reading through the divided gain is checked in every case it is used in (read:
 * against). This is the same comparison where the WHOLE signal can be compared: cases
 * that sit under the rail are rendered both ways, and every direct output sample is
 * set against limit(reading). Returns the worst distance per case, in units in the
 * last place. */
async function linearity(ctx, adapter, space, seed, count, opt) {
  const rows = [];
  for (let i = 0; i < count; i++) {
    const c = Object.assign(makeCase(space, seed, i, opt), { tail: 'short' }), d = dividedGain(space, c);
    const bc = build(space, c), bd = Object.assign(build(space, d), { limit: space.cfg.limit });
    const [rc, rd] = await renderBatch([jobOf(adapter, c, bc), jobOf(adapter, d, bd)], ctx.bin);
    const direct = read(rc, bc), v = read(rd, bd, null, [rc.L, rc.R]);
    if (!direct.rail && direct.prePeak >= LIN_FROM && direct.prePeak <= LIN_TO) rows.push({ i, peak: direct.prePeak, ulps: v.ulps });
  }
  return rows;
}

/* ================================================================== the key report
 * `eff` is the effect test's result: { seen: Map(key -> patch number or null), notRead }. */
function keyReport(space, st, say, problems, eff) {
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
  say(`[${space.engine}] SENT  a value counts if it was in force while a frame rendered. ${cont.length - noMin.length} of ${cont.length} continuous keys were sent their declared minimum (fewest: ${lowMin.key} in ${st[lowMin.key].min} cases) ` +
    `and ${cont.length - noMax.length} of ${cont.length} their declared maximum (fewest: ${lowMax.key} in ${st[lowMax.key].max} cases); ` +
    `${optCount - noOpt.length} of ${optCount} options of ${lists.length} option lists were sent (fewest: ${lowOpt.key}=${lowOpt.o} in ${lowOpt.n} cases)`);
  const vary = space.fuzz.length;
  say(`[${space.engine}] CHANGES  swept during a held note: ${vary - noSweep.length} of ${vary} keys; in a stacked change as its lead key: ${vary - noStack.length} of ${vary}` +
    (strings.length ? `. NOT VARIED: ${strings.map(r => r.key).join(', ')} (a string row: the table declares one value, its default)` : ''));
  // ---- sent is not heard: the three classes
  const heard = space.fuzz.filter(r => eff.seen.get(r.key) !== null), unread = space.fuzz.filter(r => eff.notRead.includes(r.key));
  const noEffect = space.fuzz.filter(r => eff.seen.get(r.key) === null && !eff.notRead.includes(r.key));
  const wrongly = unread.filter(r => eff.seen.get(r.key) !== null);
  if (noEffect.length) problems.push(`[${space.engine}] KEYS: ${noEffect.length} were sent but never shown to have an effect, so their extremes are not reached: ${noEffect.map(r => r.key).join(', ')}`);
  if (wrongly.length) problems.push(`[${space.engine}] KEYS: the source reader calls ${wrongly.map(r => r.key).join(', ')} unread, yet moving it changed the output: the reader is wrong`);
  const deepest = heard.reduce((m, r) => Math.max(m, eff.seen.get(r.key)), 0);
  say(`[${space.engine}] EFFECT  ${heard.length} of ${vary} keys that can vary changed the output when moved ALONE between their extremes, on a fixed sample (${heard.filter(r => eff.seen.get(r.key) === 0).length} on its first patch, the last on patch ${deepest + 1} of at most ${EFFECT_TRIES})`);
  say(`[${space.engine}] NOT READ  declared, not read by the engine (its source never reads the member), and not counted as reached: ${unread.length ? unread.map(r => r.key).join(', ') : 'none'}`);
  say(`[${space.engine}] NO EFFECT SEEN  read by the source, but no patch of the sample showed a difference; not counted as reached: ${noEffect.length ? noEffect.map(r => r.key).join(', ') : 'none'}`);
  const ok = r => eff.seen.get(r.key) !== null;
  say(`[${space.engine}] REACHED  sent AND shown to have an effect: both ends of ${cont.filter(r => ok(r) && st[r.key].min && st[r.key].max).length} of ${cont.length} continuous keys; ` +
    `every option of ${lists.filter(r => ok(r) && [...st[r.key].opts.values()].every(n => n > 0)).length} of ${lists.length} option lists`);
  if (hole.length) say(`[${space.engine}] HOLE  ${hole.length} engine keys have NO declared range, so they were not fuzzed (the table lists them as fixed): ${hole.join(', ')}`);
  return { keys: space.rows.length, engineKeys: engineKeys.length, hole, heard: heard.length, noEffect: noEffect.map(r => r.key), unread: unread.map(r => r.key) };
}

/* ================================================================== the run */
/* Every way out of a gate run ends in the verdict line: see "THE LAST LINE IS THE VERDICT". */
const verdictLine = text => console.log(`h2_fuzz_check: ${text}`);
function usage(why) {
  console.error(`h2_fuzz_check: ${why}\nusage: node tools/h2_fuzz_check.mjs [--bin PATH] [--seed N] [--deep] [--cases N] [--procs N] [--measure] [--replay '<json>']`);
  verdictLine(`RED (not run: ${why}; 0 cases judged)`);
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
  const caseOpt = { longS: o.deep ? DEEP_LONG_HOLD_S : LONG_HOLD_S };

  if (o.replay) {
    const { adapter, space, c } = decode(JSON.parse(o.replay));
    const v = (await assess(ctx, adapter, space, [c])).verdicts[0];
    say(`REPLAY ${adapter}: ${v.classes.length ? v.classes.join(' + ').toUpperCase() : 'clean'}${v.grey.length ? ` (grey: ${v.grey.join(', ')})` : ''}; ${verdictText(v, c)}; ${diffText(space, c)}`);
    return v.classes.length ? 1 : 0;
  }

  const engines = enrolled(), table = listAdapters(o).map(a => a.id);
  for (const id of [...engines, CLEAN, SILENT, ...PLANTS.map(p => p.id)]) if (!table.includes(id)) throw new Error(`h2_fuzz_check: ${ctx.bin} has no adapter "${id}"; rebuild h2_harness_render`);
  if (!engines.length) throw new Error('h2_fuzz_check: no engine is enrolled; a fuzz of nothing must not read green');
  const spaces = Object.fromEntries(engines.map(e => [e, loadSpace(e)]));
  const cases = e => (o.cases !== null ? o.cases : o.deep ? Math.max(DEEP_CASES, prCases(spaces[e])) : prCases(spaces[e]));
  for (const e of engines) if (cases(e) < minCases(spaces[e])) usage(`${cases(e)} cases cannot reach every key of "${e}": its table needs at least ${minCases(spaces[e])}`);
  say(`h2_fuzz_check: seed 0x${o.seed.toString(16).toUpperCase()} (${o.seed}), ${o.deep ? 'deep' : o.cases !== null ? 'custom' : 'PR-size'} run, ${engines.map(e => `${cases(e)} cases of ${e}`).join(', ')}, long holds of ${caseOpt.longS} s, ${o.procs} render processes`);

  if (o.measure) {
    // A MEASUREMENT, NOT A VERDICT: the numbers the thresholds table is built from. The
    // growth probe runs on EVERY case here, so the worst growth of a healthy engine is known.
    for (const e of engines) {
      const space = spaces[e], t = newTally(), N = cases(e);
      for (let a = 0; a < N; a += SLAB) {
        const cs = Array.from({ length: Math.min(SLAB, N - a) }, (_, k) => makeCase(space, o.seed, a + k, caseOpt));
        const { verdicts, built } = await assess(ctx, e, space, cs, { measure: MEASURE_EXTRA_S, growAll: true });
        tally(t, cs, built, verdicts);
      }
      say(`MEASURED [${e}] ${t.cases} cases, ${t.samples} samples: true bus level, worst ${t.level.v.toPrecision(6)} (case ${t.level.i}, read: ${t.level.via}); on the rail in the direct render ${t.railCases} cases; read through the divided gain ${t.reread}, worst distance of a direct sample from its reading ${t.ulps.v.toPrecision(4)} units in the last place${t.ulps.i !== null ? ` (case ${t.ulps.i})` : ''}; off scale ${t.offScale}; unread ${t.unread}; non-finite samples ${t.nonFinite + t.preNaN}`);
      say(`MEASURED [${e}] seconds from the release law's end until the output stays at or under silentFloor (${T.silentFloor.toExponential(3)}), worst ${t.decay.v.toFixed(4)} (case ${t.decay.i})` +
        (t.decay.v >= MEASURE_EXTRA_S - 1e-9 ? ` -- NEVER within the ${MEASURE_EXTRA_S} s rendered` : ''));
      say(`MEASURED [${e}] growth when the loudest stretch is held longer, over all ${t.probed} cases probed: worst ${t.growth.v.toPrecision(5)} (case ${t.growth.i}; last two quarters: ${winText(t.growth.win, t.growth.extra, t.growth.sr)})`);
      say(`MEASURED [${e}] rise from the third quarter to the last of the peak's stretch: above riseProbe (${T.riseProbe}) in ${t.risers} of ${t.cases} cases; largest finite ${t.rise.v.toPrecision(4)} (case ${t.rise.i})`);
      if (space.cfg.gain) {
        const lin = await linearity(ctx, e, space, o.seed, Math.min(N, 200), caseOpt);
        const w = lin.reduce((m, r) => (m === null || r.ulps > m.ulps ? r : m), null);
        say(`MEASURED [${e}] the reading through the divided gain, ${lin.length} of the first ${Math.min(N, 200)} cases (direct peak ${LIN_FROM} to ${LIN_TO}), every sample: worst distance ${w ? w.ulps.toPrecision(4) : 'none'} units in the last place${w ? ` (case ${w.i}, peak ${w.peak.toPrecision(4)})` : ''}`);
      }
      if (t.level.c) {
        // the loudest case, cut down to what keeps it within 10 % of that level
        const m = await minimise(ctx, e, space, t.level.c, null, v => !v.crash && !v.offScale && v.level >= 0.9 * t.level.v);
        const vm = (await assess(ctx, e, space, [m.c])).verdicts[0];
        say(`MEASURED [${e}] the loudest case, minimised while it stays within 10 % (${m.minimised ? `${m.items} item(s), ${m.c.notes.length} note(s)` : 'as found'}): ${verdictText(vm, m.c)}; ${diffText(space, m.c)}\n    REPRO ${replayLine(space, e, m.c)}`);
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
    if (v.classes.length) problems.push(`KEPT ${k.id} fails again: ${v.classes.join(' + ')}; ${verdictText(v, c)}\n    REPRO ${replayLine(space, adapter, c)}`);
  }
  say(`KEPT  ${kept.length} kept case(s) replayed first (${KEPT})`);

  let judged = 0, greys = 0;
  for (const e of engines) {
    const space = spaces[e], N = cases(e), t = newTally(), withControls = e === CONTROL_ENGINE, vary = space.fuzz.length;
    const eff = { seen: await effects(ctx, e, space, space.fuzz, EFFECT_TRIES), notRead: space.cfg.notRead ? space.cfg.notRead() : [] };
    const plants = PLANTS.map(p => ({ p, due: 0, found: 0, entered: 0, near: 0, via: {}, wrong: [], first: null, seenMin: Infinity, blind: 0, blindMax: 0, bySmallest: 0, quiet: 0 }));
    const clean = { bad: [] }, mute = { cases: 0, level: 0, silence: 0 };
    let st = null;
    for (let a = 0; a < N; a += SLAB) {
      const cs = Array.from({ length: Math.min(SLAB, N - a) }, (_, k) => makeCase(space, o.seed, a + k, caseOpt));
      const { verdicts, built } = await assess(ctx, e, space, cs);
      tally(t, cs, built, verdicts);
      st = coverage(space, cs, built, st);
      if (!withControls) continue;
      for (const q of plants) {
        const r = await assess(ctx, q.p.id, space, cs), grey = q.p.expect.startsWith('grey ') ? q.p.expect.slice(5) : null;
        cs.forEach((c, k) => {
          const want = planted(space, q.p, c, r.built[k]), v = r.verdicts[k];
          if (want.entered) q.entered++;
          if (want.near) { q.near++; return; }
          const right = sameList(v.classes, want.classes) && sameList(v.grey, want.grey) &&
            (want.level === undefined || (v.blowBy !== 'off scale' && Math.abs(v.level - want.level) <= NEAR * want.level));
          if (grey ? want.grey.includes(grey) : want.classes.includes(q.p.expect)) q.due++;
          if (grey ? v.grey.includes(grey) : v.classes.includes(q.p.expect)) {
            q.found++; if (!q.first) q.first = { c, v };
            if (v.blowBy) q.via[v.blowBy] = (q.via[v.blowBy] || 0) + 1;
            if (want.cornerS < q.seenMin) q.seenMin = want.cornerS;
            if (want.quietEnd) q.quiet++;
          } else if (want.entered) { q.blind++; if (want.cornerS > q.blindMax) q.blindMax = want.cornerS; }
          // one ratio alone above growFail in a case that is not a blow-up: only the smallest of the three got it right
          if (v.win && !v.classes.length && Math.max(v.win[0][1] / v.win[0][0], v.win[1][1] / v.win[1][0], v.win[1][1] / v.win[0][1]) > T.growFail) q.bySmallest++;
          if (!right && q.wrong.length < 5) q.wrong.push(`case ${c.i}: planted ${[...want.classes, ...want.grey.map(g => 'grey ' + g)].join(' + ') || 'nothing'}${want.level !== undefined ? ` at level ${want.level.toPrecision(6)}` : ''}, read ${[...v.classes, ...v.grey.map(g => 'grey ' + g)].join(' + ') || 'clean'} (${verdictText(v, c)})`);
        });
      }
      const r = await assess(ctx, CLEAN, space, cs);
      cs.forEach((c, k) => { const v = r.verdicts[k]; if ((v.classes.length || v.grey.length) && clean.bad.length < 5) clean.bad.push(`case ${c.i} read ${[...v.classes, ...v.grey].join(' + ')}`); });
      if (a === 0) for (const v of (await assess(ctx, SILENT, space, cs)).verdicts) { mute.cases++; if (v.canLevel) mute.level++; if (v.canSilence) mute.silence++; }
    }
    judged += t.cases; greys += t.greyCount;

    // ---- the engine's own verdict
    say(`[${e}] ${t.cases} cases (${t.kinds.static} static, ${t.kinds.sweep} sweeps, ${t.kinds.stack} stacked changes, ${t.kinds.named} named), ${t.samples} samples judged; a second note struck over the held ones in ${t.added} and after a gap in ${t.gapped}; a long hold in ${t.long}`);
    say(`[${e}] NON-FINITE  ${t.nonFinite} output samples${t.preNaN ? ` and ${t.preNaN} unreadable bus samples` : ''} (the engine NaN count, risk row R3)`);
    say(`[${e}] LEVEL  true bus peak, worst ${t.level.v.toPrecision(5)} at case ${t.level.i} (read: ${t.level.via}); pass to ${T.prePass}, grey to ${T.preFail}, above it too loud: a runaway alarm, not a level check. ` +
      `On the rail in the direct render: ${t.railCases} case(s); read through the gain divided by 2^40: ${t.reread}, the direct render within ${t.ulps.v.toPrecision(3)} units in the last place of the reading at worst (allowed ${T.readUlps}); off scale: ${t.offScale}; level unread: ${t.unread}`);
    say(`[${e}] GROWTH  probed (the peak's stretch rose by more than ${T.riseProbe}, or the level was above ${T.prePass}): ${t.probed} case(s), ${t.risers} of them risers${t.probed ? `; worst growth ${t.growth.v.toPrecision(4)} at case ${t.growth.i}` : ''} (steady to ${T.growPass}, grey to ${T.growFail}, above it a blow-up)`);
    say(`[${e}] GROWTH REACH  the slowest doubling a hold of this run can decide: every ${t.reach.v.toPrecision(3)} s (case ${t.reach.i}, a stretch of ${t.reach.s.toPrecision(3)} s); without the long holds and the named cases: every ${t.plainReach.toPrecision(3)} s. Anything slower reads as a level`);
    say(`[${e}] COULD SHOW  level and growth: ${t.canLevel} of ${t.cases} cases. In the other ${t.cases - t.canLevel} the bus never left zero: ${t.muteOwn} that test gain at its minimum, where that is expected, and ${t.cases - t.canLevel - t.muteOwn} others${t.mute.length ? ` (cases ${t.mute.join(', ')}${t.cases - t.canLevel - t.muteOwn > t.mute.length ? ', ...' : ''})` : ''}. Silence: ${t.judged} (below)`);
    if (t.cases - t.canLevel - t.muteOwn) problems.push(`[${e}] ${t.cases - t.canLevel - t.muteOwn} case(s) that do not test gain's minimum rendered a bus of exact zeros with keys held, so they could show no level and no growth; first: case ${t.mute[0]}`);
    for (const x of t.named) say(`[${e}] NAMED ${x.c.name}  ${x.v.classes.length ? x.v.classes.join(' + ').toUpperCase() : x.v.grey.length ? 'grey ' + x.v.grey.join(', ') : 'pass'}: ${verdictText(x.v, x.c)}`);
    if (t.named.length !== (space.cfg.named || []).length) problems.push(`[${e}] ${(space.cfg.named || []).length} named case(s) are declared and ${t.named.length} were read`);
    say(`[${e}] SILENCE  judged in ${t.judged} of ${t.cases} cases: the tail was rendered to the end of the release AND there was sound when the keys came up. Not judged: ${t.asserted - t.judged} already under the floor at the release (they could not have failed), ${t.cases - t.asserted} with a tail cut short. ` +
      `Longest release judged ${t.longestJudged.toFixed(2)} s of ${t.longest.toFixed(2)} s; worst tail peak ${t.tail.v.toExponential(2)}${t.tail.i !== null ? ` at case ${t.tail.i}` : ''} (floor ${T.silentFloor.toExponential(2)})`);
    say(`[${e}] FULL TAILS  a full-length tail, whatever the release: the sweeps of ${t.full.sweep.size} of ${vary} keys, the stacked changes of ${t.full.stack.size}, and ${t.full.static.size} of the ${vary} static places${o.deep ? '' : ` (every one of them in the deep run, which is red otherwise)`}`);
    if (!t.judged) problems.push(`[${e}] silence was judged in no case`);
    if (t.longestJudged < t.longest) problems.push(`[${e}] the longest release in the run (${t.longest.toFixed(2)} s) was never judged for silence (longest judged ${t.longestJudged.toFixed(2)} s)`);
    if (o.deep) for (const kind of ['sweep', 'stack', 'static']) if (t.full[kind].size < vary) problems.push(`[${e}] DEEP: only ${t.full[kind].size} of ${vary} keys had a full-length tail on a ${kind} case; a deep run needs ${FULL_TAIL_EVERY} rounds (${preambleCases(space) + FULL_TAIL_EVERY * roundLen(space)} cases)`);
    const keyProblems = [], kr = keyReport(space, st, say, keyProblems, eff);
    problems.push(...keyProblems);
    if (!kr.heard) problems.push(`[${e}] CONTROL effect: no key at all changed the output: the effect test sees nothing`);
    {
      // the key report must be able to say no: the same counts with ONE key's taken away
      const lost = space.rows.find(r => r.kind === 'c').key, scratch = [];
      keyReport(space, Object.assign({}, st, { [lost]: { sets: 0, min: 0, max: 0, sweeps: 0, stacks: 0, opts: null } }), () => {}, scratch, eff);
      // what the report already said of the real counts is not this control's
      const added = scratch.filter(p => !keyProblems.includes(p)), named = added.length === 5 && added.every(p => p.endsWith(`: ${lost}`));
      if (!named) problems.push(`[${e}] CONTROL coverage: with the counts of "${lost}" removed the key report gave ${added.length} more problem(s), want the five that name it`);
      say(`[${e}] CONTROL coverage  the same counts with those of "${lost}" removed: ${named ? 'red five ways (never set, no minimum, no maximum, never swept, never stacked)' : 'NOT reported'}`);
    }
    {
      // ...and the effect test must be able to say no: control/tone ignores every key
      const deaf = await effects(ctx, CLEAN, space, space.fuzz.slice(0, 8), 1), heardBy = [...deaf].filter(([, m]) => m !== null).map(([k]) => k);
      if (heardBy.length) problems.push(`[${e}] CONTROL effect (must fail): ${CLEAN}, which ignores every key, showed an effect for ${heardBy.join(', ')}`);
      say(`[${e}] CONTROL effect (must fail)  ${CLEAN}, which ignores every key, tested the same way for ${deaf.size} keys: ${heardBy.length ? 'an effect REPORTED for ' + heardBy.join(', ') : 'no effect for any'}`);
    }
    {
      // ...and "in force" must mean it: a value overwritten before any frame renders was never sent
      const c0 = makeCase(space, o.seed, 0, caseOpt), b0 = build(space, c0), row = space.rows.find(r => r.kind === 'c' && !r.mapped && c0.base[r.key] !== r.max);
      b0.cmds.unshift(['set', row.key, row.max]);
      const got = coverage(space, [c0], [b0])[row.key];
      if (got.max !== 0 || got.sets < 2) problems.push(`[${e}] CONTROL in force: "${row.key}" sent at its maximum and overwritten before the first frame was counted as sent its maximum ${got.max} time(s) (must be 0) from ${got.sets} set command(s) (must be 2 or more)`);
      say(`[${e}] CONTROL in force  "${row.key}" sent at its maximum, then overwritten before the first frame: counted as sent its maximum ${got.max} time(s) (must be 0), from ${got.sets} set commands`);
    }
    if (space.cfg.gain) {
      // the reading through the divided gain must describe the direct render wherever both can be had in full
      const lin = await linearity(ctx, e, space, o.seed, preambleCases(space), caseOpt);
      const w = lin.reduce((m, r) => (m === null || r.ulps > m.ulps ? r : m), null);
      const ok = lin.length >= LIN_MIN && lin.every(r => r.ulps <= T.readUlps);
      if (!ok) problems.push(`[${e}] CONTROL linearity: ${lin.length < LIN_MIN ? `only ${lin.length} case(s) could be compared, want ${LIN_MIN}` : `case ${w.i} is ${w.ulps.toPrecision(4)} units in the last place from its reading, allowed ${T.readUlps}`}: the reading through the divided gain cannot be trusted`);
      say(`[${e}] CONTROL linearity  ${lin.length} preamble case(s) under the rail (direct peaks ${lin.map(r => r.peak.toPrecision(3)).join(', ')}) rendered again with gain / 2^40: every direct output sample against tanh(reading), worst ${w ? w.ulps.toPrecision(3) : 'none'} units in the last place${w ? ` (case ${w.i})` : ''}, allowed ${T.readUlps}${ok ? '' : ' -- FAILED'}`);
      // ...and the comparison must be able to say no: control/tone's bus ignores gain entirely
      const not = await linearity(ctx, CLEAN, space, o.seed, 3, caseOpt), caught = not.length > 0 && not.every(r => r.ulps > T.readUlps);
      if (!caught) problems.push(`[${e}] CONTROL linearity (must fail): ${CLEAN}, whose bus does not follow gain, was not reported as misread`);
      say(`[${e}] CONTROL linearity (must fail)  ${CLEAN}, whose bus ignores gain, compared the same way over ${not.length} case(s): ${caught ? `${Math.min(...not.map(r => r.ulps)).toExponential(2)} units or more, reported` : 'NOT reported'}`);
    } else say(`[${e}] CONTROL linearity  NOT RUN: this engine declares no gain key, so a level on the rail is counted as unread`);
    if (t.greyCount) {
      say(`[${e}] GREY  ${t.greyCount} case(s) in a grey band, for the human's ears (not red):`);
      for (const [n, g] of t.grey.entries()) {
        if (n >= MINIMISE_PER_ENGINE) { say(`    case ${g.c.i} (${g.v.grey.join(', ')}): ${verdictText(g.v, g.c)} (not minimised)`); continue; }
        const m = await minimise(ctx, e, space, g.c, g.v);
        say(`    case ${g.c.i} (${g.v.grey.join(', ')}): ${verdictText(g.v, g.c)}\n      ${m.minimised ? `minimised to ${m.items} item(s), ${m.c.notes.length} note(s)` : 'not minimised'}: ${diffText(space, m.c)}\n      REPRO ${replayLine(space, e, m.c)}`);
      }
    }
    if (t.failCount) {
      problems.push(`[${e}] ${t.failCount} failing case(s); first: case ${t.fails[0].c.i} ${t.fails[0].v.classes.join(' + ')}`);
      say(`[${e}] FAILED  ${t.failCount} case(s):`);
      for (const [n, f] of t.fails.entries()) {
        if (n >= MINIMISE_PER_ENGINE) { say(`    case ${f.c.i} ${f.v.classes.join(' + ')}: ${verdictText(f.v, f.c)} (not minimised)\n      REPRO ${replayLine(space, e, f.c)}`); continue; }
        const m = await minimise(ctx, e, space, f.c, f.v);
        say(`    case ${f.c.i} ${f.v.classes.join(' + ')}: ${verdictText(f.v, f.c)}\n      ${m.minimised ? `minimised to ${m.items} item(s), ${m.c.notes.length} note(s)` : 'NOT minimised (it fails only with its own tail)'}: ${diffText(space, m.c)}\n      REPRO ${replayLine(space, e, m.c)}`);
        say(`      KEEP  add to ${KEPT} "cases": ${JSON.stringify({ id: `${e}-seed${o.seed}-case${f.c.i}`, found: `seed ${o.seed}, case ${f.c.i}`, was: m.cls, case: encode(space, e, m.c) })}`);
      }
    }
    if (!withControls) continue;

    // ---- the controls: the same cases through the planted faults, the clean tone and the silent adapter
    for (const q of plants) {
      const where = cornerText(space, q.p), exp = q.p.expect, name = exp.toUpperCase();
      if (q.wrong.length) problems.push(`CONTROL ${q.p.id}: the reading does not match the plant: ${q.wrong.join('; ')}`);
      const exactly = `${q.wrong.length ? 'NOT ' : ''}exactly as planted${q.near ? ` (${q.near} case(s) with a planted number on a line, not judged)` : ''}`;
      if (exp === 'clean') {
        // a plant that must NOT be reported: every case clean, and the rule it exists for exercised
        if (!q.entered) problems.push(`CONTROL ${q.p.id}: no case entered its corner (${where}), so it tested nothing`);
        if (!q.bySmallest) problems.push(`CONTROL ${q.p.id}: in no case was one of the three growth ratios alone above growFail, so "the smallest of three" was not exercised`);
        say(`CONTROL ${q.p.id}  planted ${q.p.what} at ${where}: ${q.entered} of ${t.cases} cases entered the corner and every case read ${exactly}, none a blow-up; in ${q.bySmallest} of them one of the three growth ratios ALONE was above ${T.growFail}, and only the smallest of the three read them right`);
        continue;
      }
      if (!q.found) { problems.push(`CONTROL ${q.p.id}: the planted ${exp} at ${where} was NOT FOUND in ${t.cases} cases`); say(`CONTROL ${q.p.id}  NOT FOUND (${where})`); continue; }
      const m = await minimise(ctx, q.p.id, space, q.first.c, q.first.v);
      // the minimised case must be the corner (and what the plant needs besides) and nothing else
      const final = Object.assign({}, m.c.base), touched = new Set(space.rows.filter(r => m.c.base[r.key] !== r.def).map(r => r.key));
      for (const ev of m.c.events) for (const [k, v] of ev.sets) { final[k] = v; touched.add(k); }
      const want = Object.keys(q.p.corner), may = new Set([...want, ...(q.p.also || [])]);
      const exact = m.minimised && want.every(k => touched.has(k) && atEnd(space.byKey[k], q.p.corner[k], final[k])) && [...touched].every(k => may.has(k));
      if (!exact) problems.push(`CONTROL ${q.p.id}: minimised to "${diffText(space, m.c)}", which is not the corner (${where})`);
      const how = Object.entries(q.via).map(([k, n]) => `${n} by ${k}`).join(', ');
      say(`CONTROL ${q.p.id}  planted ${q.p.what} at ${where}: read as ${name} in ${q.found} of ${t.cases} cases, ${exactly}` + (how ? ` (${how})` : '') +
        `; first at case ${q.first.c.i} (within the first ${q.first.c.i + 1} cases)`);
      if (exp === 'blow-up') say(`    REACH: read after as little as ${q.seenMin.toPrecision(3)} s in the corner; ${q.blind} case(s) entered the corner and were NOT read as a blow-up, as planted (too short to trigger or to decide, or not the case's loudest stretch), the longest after ${q.blindMax.toPrecision(3)} s`);
      if (q.p.lateS) say(`    in ${q.quiet} of them the level had stopped more than 64 frames before the end of the render`);
      say(`    first: ${verdictText(q.first.v, q.first.c)}`);
      say(`    minimised to ${m.items} item(s), ${m.c.notes.length} note(s): ${diffText(space, m.c)}${exact ? ' -- the corner and nothing else' : ' -- NOT the corner'}\n    REPRO ${replayLine(space, q.p.id, m.c)}`);
      // each plant's own rule must have been exercised, or it ran on nothing
      if (exp === 'blow-up' && !q.via['the growth windows']) problems.push(`CONTROL ${q.p.id}: no case was read as growing by the growth windows (all ${q.found} were off scale), so the window rule was not exercised`);
      if (q.p.lateS && !q.quiet) problems.push(`CONTROL ${q.p.id}: in no case it was found in had the level stopped before the last 64 frames, so "the whole tail is judged" was not exercised`);
    }
    if (clean.bad.length) problems.push(`CONTROL ${CLEAN}: a clean adapter was reported: ${clean.bad.join('; ')}`);
    say(`CONTROL ${CLEAN}  must read clean over the same ${t.cases} cases: ${clean.bad.length ? 'REPORTED ' + clean.bad.join('; ') : 'nothing reported'}`);
    // the "could show" counts must be able to read zero: nothing can show on an adapter that only returns silence
    if (!mute.cases || mute.level || mute.silence) problems.push(`CONTROL ${SILENT}: of ${mute.cases} cases through an adapter that returns silence, ${mute.level} were counted as able to show a level and ${mute.silence} as judged for silence; both must be 0`);
    say(`CONTROL ${SILENT}  the first ${mute.cases} cases through an adapter that returns silence: counted as able to show a level ${mute.level}, as judged for silence ${mute.silence} (both must be 0)`);
    // an undeclared key must be reported, or "no unknown key" above proves nothing
    const c0 = makeCase(space, o.seed, 0, caseOpt), b0 = build(space, c0);
    b0.cmds.unshift(['set', 'h2fuzz.no-such-key', 1]);
    const vk = read((await renderBatch([jobOf(e, c0, b0)], ctx.bin))[0], b0);
    if (!vk.unknownKeys) problems.push(`CONTROL unknown-key: a key the engine does not have was not reported on "${e}"`);
    say(`CONTROL unknown-key  a key no table declares, sent to ${e}: ${vk.unknownKeys ? `reported ("${vk.firstUnknown}")` : 'NOT reported'}`);
  }
  if (!engines.includes(CONTROL_ENGINE)) problems.push(`the controls are written in the table of "${CONTROL_ENGINE}", which is not enrolled: no control ran`);
  if (!judged) problems.push('no case was judged: a run of nothing is not a pass');

  printThresholds(say);
  for (const p of problems) console.error(`h2_fuzz_check: RED  ${p}`);
  if (o.deep) say(`KIT-GATE cases=${judged}`);
  verdictLine(`${problems.length ? 'RED' : 'GREEN'} (${judged} cases judged over ${engines.length} enrolled engine(s): ${engines.join(', ')}; ${problems.length} problem(s); ${greys} grey result(s) for the human's ears; ${ctx.renders} renders with the controls, the re-reads, the probes and the minimising)`);
  return problems.length ? 1 : 0;
}

/* Run only as the entry script. The comparison is between two file URLs made the same
   way: a hand-built `file://${argv[1]}` differs from import.meta.url whenever the path
   holds a space (percent-encoded in a URL) or is reached through a symlink (Node
   resolves the entry's real path), and then nothing ran, nothing printed and the exit
   code was 0. */
if (process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href) {
  main(process.argv.slice(2)).then(code => { process.exitCode = code; }, e => {
    console.error(e.message || e);
    verdictLine(`RED (stopped before a verdict: ${String(e.message || e).split('\n')[0]}; 0 cases judged)`);
    process.exitCode = 2;
  });
}
