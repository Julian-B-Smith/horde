/*
 * h2_soak_check.mjs — the long-run soak of every enrolled horde 2 engine.
 * ROADMAP B448 (Blind-Spot Armor) phase 2, package P6, ruled in ADR-209. The proposal is
 * docs/strategy/blind-spot-armor-phase2.md: section 1 row R5, section 3 row P6. It is a
 * loop over the engine harness (h2/harness/README.md), read as it is written
 * (harness.mjs runStream), so a render of hours needs the memory of one of seconds.
 *
 * WIRED: ./verify full
 *
 * WHAT IT DOES. For each enrolled engine it renders long CELLS through the C++ engine
 * (h2_harness_render, never the JS one): a patch at chosen settings, a sample rate, and a
 * seeded stream of notes that never stops. As the stream arrives it holds, on BOTH
 * channels:
 *   - no output sample is non-finite, ever (the count is printed);
 *   - LEVEL: the level of every window late in the run stays within a band of the
 *     level early in the run;
 *   - DC: likewise the offset;
 *   - WRAP: at every power-of-two frame count the run passes (2^20 up; 2^31 in the
 *     192 kHz cell) there is no click and no step in level;
 *   - SILENCE: after the last key-off and the release, the output falls silent and
 *     stays so.
 * IT DOES NOT HOLD spectral centroid or pitch, which row R5 also names:
 * tools/patchspace/metrics.mjs has neither yet (package P9 adds them) and the proposal
 * forbids a second metrics library. Row R5 stays partial.
 *
 * THE LAST LINE IS THE VERDICT. Whatever happens, the last line this tool writes to
 * standard output is `h2_soak_check: GREEN (...)` or `h2_soak_check: RED (...)`, with
 * the simulated seconds and the windows judged, and a run that judged no window is
 * RED and exits non-zero. (--measure and --list are not gate runs and end in their
 * own line.)
 *
 * THE NOTE STREAM, AND WHY TWO WINDOWS CAN BE COMPARED. With notes coming and going the
 * level moves on purpose, so "the level late against the level early" means nothing
 * unless the two stretches hold the same music. So the stream is ONE PHRASE, drawn
 * from the printed seed, played again every PERIOD (8 or 16 s): a drone held for the
 * whole run, a long hold, two chords, a note struck four times (twice without a
 * release between), a note held over the period's boundary, and single notes
 * (makePhrase). A WINDOW is exactly one period. Every judged window therefore holds
 * the same notes at the same places at the same velocities, and what differs between
 * two windows is only what the engine does by itself: its per-note random draws, its
 * free-running phases and slow motion, and any drift. The first of these is the
 * measured spread the bands are built from; the last is what the gate is for.
 *   Period 0 is a WARM-UP, read for non-finite samples and not judged: it starts from
 *   silence and every later period starts from the one before.
 *   THE DRONE is the one voice that is never restarted. The other voices live a few
 *   seconds; whatever drifts inside a voice has the whole run to do it there.
 *   IT IS PROVEN ON EVERY RUN: control/soak-steady, a source that is as steady as one
 *   can be, is played the same phrase and must read no drift, no wrap fault, nothing.
 *
 * THE BUS, NOT THE OUTPUT. The engine ends in tanh, and a loud patch sits near its
 * rail, where a drift of a decibel in the engine moves the output by a small part of
 * one. So level, DC and the wrap measures are read on the PRE-LIMITER BUS
 * (atanh(output), h2/harness/README.md). That is only readable well under the rail,
 * so a cell whose patch is too loud (or silent) sets its own gain, and a bus that
 * peaks above busReadMax or touches the rail in a judged window is RED (bus-unread),
 * never quietly judged.
 *   WHY A LOWER GAIN LOSES NOTHING. h2/engine/engine.h's last two lines are
 *       L[i] = std::tanh(yl * s.gain * 1.6);   R[i] = std::tanh(yr * s.gain * 1.6);
 *   and gain is read nowhere else: tools/h2_fuzz_check.mjs holds that on every run,
 *   from the source and sample by sample (its header, READING ABOVE THE RAIL). So the
 *   bus is gain times something gain cannot reach: at a lower gain the engine's whole
 *   state is the same, sample for sample, and only the limiter is driven less hard.
 *   The limiter has no state, so it has nothing to drift. This file relies on the
 *   fuzz for that and does not repeat it.
 *   "Carried signal" and silence are read on the OUTPUT, which is what is heard.
 *
 * COUNTED, ON EVERY RUN: how many judged windows carried signal (output above
 * signalFloor on both channels). A judged window that did not is RED: a soak of
 * silence proves nothing. control/silent, an adapter that returns silence, is run
 * through the same cell and must be red for exactly that.
 *
 * THE RULES, one function each (RULES). Every measure is metrics.mjs's own (rms, dc,
 * peak, nonFinite, clicks) over a slot or a wrap window; this file adds slots up.
 *   non-finite  any non-finite output sample, anywhere in the stream. No band.
 *   LEVEL       per bus channel: the reference is the median level of the first
 *               REF_WINDOWS judged windows. Every later window is set against it.
 *               Within levelPass: pass. To levelFail: GREY. Beyond: RED.
 *   DC          the same on the window's mean, as a share of the reference level.
 *   WRAP        at each power of two the adapter's own frame counter passes: a CLICK
 *               (clicks() on the 4096 frames around it, less the largest of the same
 *               measure at the same place in up to WRAP_REFS earlier periods: what is
 *               there every period cancels) and a STEP (the level of the one period
 *               after it over the one period before it; any stretch one period long
 *               holds the whole phrase once, so a healthy engine reads 0 dB to within
 *               its window-to-window spread).
 *   SILENCE     after the last key-off and the engine's release law, the output is
 *               under silentFloor within settlePass (GREY to settleFail) and stays so
 *               to the end of the render.
 *   bus-unread, no-signal   above: the two ways a cell could show nothing.
 *
 * THE COUNTER WRAP. A 32-bit frame counter passes 2^31 after 3.1 hours at 192 kHz and
 * 12.4 hours at 48 kHz, so one cell runs at 192 kHz for four hours (composed/long-192:
 * 2,764,800,000 frames of churn, past 2^31 = 2,147,483,648 by 53 minutes; it does
 * NOT reach 2^32, which is 6.2 hours). The engine cannot be asked to start with an
 * aged counter, and this package may not edit it: FOR THE REAL ENGINE THE WRAP IS
 * REACHED ONLY BY THAT CELL, once every three nights. What proves the wrap RULE on
 * every run, in seconds, is two control adapters whose own counters start 51.3 s
 * short of 2^31 and of 2^32 and misbehave there (below).
 *
 * THE CONTROLS, on every run of every size (h2/harness/control_soak.h). The phrase is
 * played through seven planted adapters and two more, and the SAME reader and judge
 * read them. Every planted fault is on the RIGHT channel only:
 *   control/soak-steady   no fault: must read NOTHING (and must have a wrap judged);
 *   control/soak-level    the right bus doubles every 235 s: LEVEL drift on R;
 *   control/soak-dc       an offset that creeps up on the right bus: DC drift on R;
 *   control/soak-wrap31   a signed counter 51.3 s short of 2^31; the right bus steps
 *                         by x1.26 when it goes negative: WRAP step on R at 2^31, and
 *                         the LEVEL drift that step is;
 *   control/soak-wrap32   a 32-bit counter 51.3 s short of 2^32; a pilot tone reads
 *                         its phase from it and jumps: WRAP click on R at 2^32;
 *   control/soak-nan      a NaN on the right output from 70 s in, every 1.5 s:
 *                         non-finite, the exact count;
 *   control/soak-hang     a level that never stops after the last key-off: not-silent;
 *   control/silent        returns silence: no-signal;
 *   control/soak-steady at gain 400   the output on the limiter's rail: bus-unread.
 * For each, PLANTS works out in closed form what the plant must read as, sharing no
 * code with the reader or the judge. The run is RED unless each reads EXACTLY so: the
 * finding where it was planted, with its number, in the fail band, and no other
 * finding in any band on either channel. (The rail control alone may show more: a bus
 * that was not read can read as anything.)
 *   EACH RULE WAS REMOVED ONCE, in a scratch copy, and the run went red through that
 *   rule's own control (2026-10-12; the package's trace lists the mutations and
 *   their output). So was the right channel: a judge that reads only the left misses
 *   all six faults planted on the right.
 *   NOT ENORMOUS. The level plant drifts at 1.45 times the slowest rate the control
 *   cell can call red, the DC plant at 1.44 times, and the wrap step is 2.1 times its
 *   line (CONTROL REACH, printed). A level band twice as wide loses the level plant,
 *   and a mutation that doubles it turns the run red.
 *
 * WHAT A CELL CAN SEE (REACH, printed for every cell of every run). The slowest steady
 * drift a cell can find is one that moves its LAST window past the line, counted
 * from the middle of the reference windows: band / span. With this file's bands a
 * drift is RED from:
 *   the short cell (108 s)          level 45 dB an hour,   DC 0.60 of the level an hour
 *   a four-hour cell (14344 s)      level 0.34 dB an hour, DC 0.0045 of the level an hour
 *   a forty-minute cell (2344 s)    level 2.1 dB an hour,  DC 0.028 of the level an hour
 * and GREY from half of each. The DC figures are as loose as they are because of one
 * cell (THRESHOLDS: extreme-loud).
 * ANYTHING SLOWER IS NOT SEEN, and neither is a drift that returns to where it began
 * before the run ends, unless a window on the way crosses the line.
 *
 * THREE SIZES, ONE TOOL (--list prints every cell).
 *   short    node tools/h2_soak_check.mjs --bin <h2_harness_render>      (./verify full)
 *            composed/short (the oracle defaults, 48 kHz, 16 windows of 8 s) and
 *            composed/short-192 (every continuous key at its minimum, 192 kHz).
 *   long     ... --size long --night N        (the nightly job soak-h2-long)
 *            FOUR SIMULATED HOURS of ONE cell, chosen by N modulo 3: long-192 (192 kHz,
 *            past 2^31 frames), long-defaults (48 kHz), long-fold (two blades in
 *            series, 44.1 kHz). 899 judged windows of 16 s.
 *   extreme  ... --size extreme --night N     (the nightly job soak-h2-extreme)
 *            FORTY SIMULATED MINUTES of ONE cell, N modulo 3: the loudest chord the
 *            parameter fuzz has found; the densest ledger preset; every continuous
 *            key at its declared maximum. 149 judged windows.
 *   Each nightly job also runs the nine controls and the streaming row, and prints
 *   `KIT-GATE cases=<n>`, n being the WINDOWS JUDGED of the night's engine cell.
 *   --cell ENGINE/ID runs one cell by name, at --windows N if given (a heavy cell
 *   for four hours, by hand: --cell composed/extreme-max --windows 899).
 *   THE NIGHT NUMBER comes from the job's command line (days since 1970, from `date`),
 *   never from this file, which reads no clock: the same N gives the same run. Every
 *   cell of a rotation is reached within three nights.
 *
 * WHY NOT FOUR HOURS OF EVERYTHING, AND THE ARITHMETIC. See RUN TIME below the
 * thresholds: the numbers are measured there, beside their dates.
 *
 * DETERMINISM. mulberry32 streams from the printed seed, nothing else. No clock and no
 * environment variable is read in this file, so no verdict can depend on time taken,
 * and two runs print the same bytes on standard output. Standard error carries the
 * problems and, for a long cell, a line every 100 periods to show it is alive.
 *
 * WHAT IT DOES NOT COVER (each is stated here, none is hidden):
 *   - Spectral centroid and pitch: not held (see the top). A fault that keeps level
 *     and DC and moves the spectrum or the tuning passes this gate.
 *   - Any drift slower than the printed REACH, and any drift on a time scale longer
 *     than four hours.
 *   - THE WRAP ON THE REAL ENGINE between nights: only long-192 reaches 2^31 frames,
 *     one night in three. Nothing reaches 2^32. A counter that counts something other
 *     than output frames (oversampled steps, control ticks, notes) wraps elsewhere:
 *     every power of two from 2^20 is checked, which covers a counter at 2 or 4 times
 *     the frame rate, and no more.
 *   - A wrap click smaller than the engine's own edges: the click measure is relative
 *     to the median frame around it. The control's click is a tone as loud as two
 *     voices jumping in phase; a quieter one would not be found.
 *   - Four hours at the extreme settings. The maximum corner, the densest preset and
 *     the loud chord are soaked for forty minutes a night each third night, because
 *     four hours of one is about 5.7 hours of a hosted runner as assumed (RUN TIME). The corner
 *     with every toggle on is not soaked at all: it renders slower than real time.
 *   - Sustain 0 on the nightly. A held voice at sustain 0 is never freed, and without
 *     flush-to-zero (the harness sets none; the shell will, ADR-211) it computes on
 *     subnormal numbers for as long as its key is down. That is cheap on arm64 and of
 *     unknown cost on x86, so the cell that holds it for four hours (long-min) is run
 *     by hand, and the short local run holds it for a minute at 192 kHz.
 *   - CPU. This tool reads no clock. A voice that keeps costing after it has gone
 *     silent is invisible to it.
 *   - A tail that falls silent late and then stays silent is caught by the rule but
 *     planted by no control here (the parameter fuzz has one for its own rule).
 *   - One phrase per seed. Other note patterns (faster than a half step of the
 *     period, more than seven held, hostile events) are the parameter fuzz's and the
 *     legacy note fuzz's, not this file's.
 *   - The digest of a stream over 2^30 frames is not re-computed on the reading side
 *     (it costs 16 byte steps a frame); its frame count and structure are checked.
 */
import { existsSync, readFileSync, realpathSync } from 'node:fs';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
import { ROOT, binPath, enrolled, jobFor, listAdapters, mtof, patch as referencePatch, patchCmds, patchesFor, run, runStream } from '../h2/harness/harness.mjs';
import { clicks, dc, nonFinite, peak, rms } from './patchspace/metrics.mjs';

/* ================================================================== THRESHOLDS
 * PROPOSED, not ratified (ADR-209 item 5). Every threshold this tool has is a row of
 * THRESHOLD_TABLE, and nothing else in the file is one. The human's rule (ADR-209 item
 * 5): "measured worst plus a stated margin, in three bands (pass, fail, and a grey band
 * that goes to the human's ears)".
 *
 * MEASURED 2026-10-11 and 12 on darwin-arm64 (Apple clang, the Release h2_harness_render),
 * ONE run of each cell at its nightly length, seed 46152, with
 *   node tools/h2_soak_check.mjs --measure --cell <cell>
 * and one more of extreme-loud at four hours (--windows 899), for the reason below.
 * --measure also reads the two wrap measures at 400 seeded places where nothing wraps,
 * so their spread on a healthy engine is known from about 406 places a cell, not nine.
 * Each number is the worst of the two channels; "level" and "DC" are the worst judged
 * window against the early reference, as the rules read them.
 *   cell            simulated         non-finite  level, dB  DC, of level  click, dB  step, dB  bus peak
 *   long-192        4 h, 899 windows  0           0.021      0.00028       +1.82      0.011     1.105
 *   long-defaults   4 h               0           0.193      0.00034       +1.07      0.226     2.576
 *   long-fold       4 h               0           0.220      0.00031       +1.00      0.209     2.306
 *   long-min        4 h               0           0.000      0.00000       +0.00      0.000     1.131
 *   extreme-max     40 min, 149       0           0.068      0.00041       +0.95      0.120     2.134
 *   extreme-dense   40 min            0           0.174      0.00033       +2.10      0.236     2.165
 *   extreme-loud    40 min            0           0.324      0.00384       +0.54      0.086     3.019
 *   extreme-loud    4 h, by hand      0           0.334      0.00451       +0.39      0.087     3.061
 *   short           128 s, 16         0           0.130      0.00045       +1.15      0.195     2.807
 *   short-192       48 s, 6           0           0.000      0.00000       +0.00      0.000     0.383
 * THE FOUR-HOUR ROWS ARE FOUR-HOUR MEASUREMENTS and the forty-minute rows are
 * forty-minute ones: no band here is a short measurement stretched. But each is ONE
 * run: one phrase, one host seed, one machine. 11,510,581,382 output samples in the
 * eight long runs, none non-finite, none on the rail.
 *   The largest click excess seen anywhere was +2.77 dB (long-fold's patch in a
 *   96 s trial, at a probe with three references); the clicks row is built on it.
 *   Settle: every cell was under -90 dBFS BEFORE its release law ended (the latest,
 *   extreme-max, under 0.0005 s before; the others 0.011 s or more), and long-min and
 *   short-192 had already decayed when the keys came up.
 *   long-192 is the first render of this engine past 2^31 frames. At 2^31 it read a
 *   click of -0.74 dB and a step of -0.002 dB: nothing.
 *   EXTREME-LOUD WAS RUN FOR FOUR HOURS TOO, because in forty minutes its last quarter
 *   sat 0.17 dB under its first, the one cell where the two differ by more than
 *   0.012 dB. IT IS NOT A DRIFT. Over four hours the level cycles by about 0.13 dB
 *   with a period near one hour (medians of twelve twenty-minute parts, L: -6.816
 *   -6.938 -6.875 -6.828 -6.939 -6.848 -6.840 -6.940 -6.839 -6.854 -6.940 -6.813) and
 *   the first and last quarters agree within 0.004 dB. The four reference windows
 *   happen to sit at the top of that cycle, which is where the 0.33 comes from.
 * NOT MEASURED: any second seed; any run longer than four hours.
 *
 * THE BANDS.
 *   level    |a judged window's bus level - the early reference|, per channel. At or
 *            under levelPass: pass. To levelFail: GREY. Above: RED.
 *   DC       the same on the bus mean, as a share of the reference level.
 *   click    the wrap window's click measure over the largest of its references'.
 *   step     |the period after a wrap over the period before it|, in dB.
 *   settle   under silentFloor from the release law's end + settlePass: pass. Under it
 *            only from + settleFail: GREY. Still above it then: RED.
 * A non-finite sample, a bus that was not read, a window with no signal and a key the
 * engine refuses have no band. One is red.
 *
 * WHY THESE MARGINS (argued, for the human to change). Level, DC and step: x2 for the
 * pass line and x4 for the fail line, on ONE run of each cell: a second seed is a
 * second draw from the same spread, and the worst of 899 windows is already far out
 * in it. The level and DC rows are both set by ONE cell, extreme-loud (nine members
 * at zero detune, the fuzz's loudest chord): without it the worsts are 0.220 dB and
 * 0.00045, and the bands would be 0.44 / 0.88 dB and 0.0009 / 0.0018. Whether that
 * one cell should have bands of its own is the human's to rule; one table has one. Click: dB over
 * the references, so the margin is added, +3 dB and +9 dB over the largest seen; the
 * planted click reads about 20. Settle: the parameter fuzz's own two lines for the
 * same law and the same floor (tools/h2_fuzz_check.mjs measured 0.1955 s over 12000
 * cases), so that one engine does not have two definitions of silent; nothing here
 * came near them.
 */
const THRESHOLD_TABLE = [
  { name: 'levelPass', value: 0.67, what: 'LEVEL drift, dB: a judged window\'s bus level against the early reference, the top of the pass band',
    worst: 'measured worst 0.334 (extreme-loud run for four simulated hours, window 311 of 899; 0.324 at its nightly forty minutes); the four-hour cells read 0.220, 0.193, 0.021 and 0.000', margin: 'x2', date: '2026-10-12' },
  { name: 'levelFail', value: 1.34, what: 'LEVEL drift, dB, above which a window fails',
    worst: 'measured worst 0.334 (the same)', margin: 'x4', date: '2026-10-12' },
  { name: 'dcPass', value: 0.009, what: 'DC drift: the change of a judged window\'s bus mean from the early reference, as a share of the reference level, the top of the pass band',
    worst: 'measured worst 0.00451 (extreme-loud run for four simulated hours, window 426 of 899; 0.00384 at its nightly forty minutes); every other cell read 0.00045 or less', margin: 'x2', date: '2026-10-12' },
  { name: 'dcFail', value: 0.018, what: 'DC drift above which a window fails',
    worst: 'measured worst 0.00451 (the same)', margin: 'x4', date: '2026-10-12' },
  { name: 'clickPass', value: 6, what: 'WRAP click, dB: the click measure of the 4096 frames around a power-of-two frame count over the largest of its reference windows\', the top of the pass band',
    worst: 'measured worst +2.77 (ledger/fold-over-sync, a 96 s trial, among 402 places); the nightly-length cells read +1.82 or less over about 406 places each', margin: '+3 dB', date: '2026-10-11' },
  { name: 'clickFail', value: 12, what: 'WRAP click, dB, above which the wrap fails',
    worst: 'measured worst +2.77 (the same)', margin: '+9 dB', date: '2026-10-11' },
  { name: 'stepPass', value: 0.47, what: 'WRAP step, dB: the level of the period after a power-of-two frame count over the level of the period before it, the top of the pass band',
    worst: 'measured worst 0.236 (extreme-dense, 40 simulated minutes, among 406 places); the four-hour cells read 0.226 or less', margin: 'x2', date: '2026-10-12' },
  { name: 'stepFail', value: 0.94, what: 'WRAP step, dB, above which the wrap fails',
    worst: 'measured worst 0.236 (the same)', margin: 'x4', date: '2026-10-12' },
  { name: 'settlePass', value: 0.304, what: 'seconds after the release law\'s end by which the output is under silentFloor',
    worst: 'measured worst here: silent under 0.0005 s BEFORE the law ended (extreme-max); the line is the parameter fuzz\'s, whose own measured worst is 0.1955', margin: 'the fuzz\'s: x1.55 of its worst', date: '2026-10-12' },
  { name: 'settleFail', value: 0.607, what: 'seconds after the release law\'s end past which a tail above silentFloor fails',
    worst: 'measured worst here: silent before the law ended (the same); the parameter fuzz\'s line', margin: 'the fuzz\'s: x3.1 of its worst', date: '2026-10-12' },
  { name: 'silentFloor', value: Math.pow(10, -90 / 20), what: 'the output level called silent: -90 dBFS peak',
    worst: 'not measured here: the line tools/patchspace/metrics.mjs silence() and the parameter fuzz already draw', margin: 'none', date: '2026-10-11' },
  { name: 'signalFloor', value: Math.pow(10, -60 / 20), what: 'a judged window carried signal if the output RMS of BOTH channels is above this: -60 dBFS',
    worst: 'the quietest judged window measured is -41.7 dBFS (short-192: blips of 10 ms at sustain 0)', margin: '18 dB under it', date: '2026-10-12' },
  { name: 'busReadMax', value: 8, what: 'a bus peak above this in a judged window is BUS UNREAD: atanh is losing digits there (h2/harness/README.md: 4.5e-9 at 10), and the cell\'s gain must come down',
    worst: 'the largest bus peak measured in any cell is 3.061 (extreme-loud, four hours)', margin: 'x2.6 over it; a limit of sight, not a band', date: '2026-10-12' },
];
export const THRESHOLDS = Object.fromEntries(THRESHOLD_TABLE.map(r => [r.name, r.value]));

/* ================================================================== RUN TIME, AND WHAT THE NIGHTLY CAN AFFORD
 * MEASURED 2026-10-11 and 12 on the dev Mac (M3, 8 cores), with a stopwatch held outside
 * this tool, which reads no clock. CPU seconds are user + system, the render tool and
 * this reader together. TWO SETS OF FIGURES, because the machine was loaded by other
 * work while the long runs were made (load average 8 to 180) and quiet afterwards.
 *   HOW LOADED: under those loads the render tool took 4.5 to 5.6 CPU s for 20 s of the
 *   ledger's own 8-voice defaults cell. Quiet (load 2.4) it took 2.17 s, which is
 *   exactly docs/port/cpu-ledger.md's figure for it (10.84 % of a core).
 *
 *   the short run     14.0 s of wall and 31.0 CPU s, quiet, at four processes (29.0 s
 *                     and 52.8 under a load of 13). The budget is a minute. Two runs
 *                     gave the same bytes on standard output.
 *
 *                     AT FULL LENGTH, LOADED                QUIET, 9 windows (162 s)
 *   cell              simulated   CPU s   x real time   |   CPU s   x real time   so a full cell is
 *   long-192          14402 s     2567    5.6           |   14.5    11.2          about 21 min of CPU
 *   long-defaults     14402 s     2372    6.1           |   11.7    13.9          17 min
 *   long-fold         14402 s     1918    7.5           |   11.7    13.9          17 min
 *   long-min          14401 s      804    17.9          |   not re-measured
 *   extreme-max        2413 s     1954    1.23          |   82.2    2.10          19 min
 *   extreme-dense      2407 s     1935    1.24          |   75.7    2.21          18 min
 *   extreme-loud       2402 s      693    3.5           |   28.1    5.8           7 min
 *   extreme-loud, 4 h 14402 s     3863    3.7           |   (by hand, not a nightly cell)
 * The quiet column is a short render scaled up, not a full run. Of long-192's CPU about
 * a quarter is this reader and the pipe (88 GB crosses it).
 *
 * THE NIGHTLY JOBS. Each is one cell, the nine controls and the streaming row (about
 * 20 CPU s) and the build of the render tool. BOTH ENTRIES WERE RUN AS THEIR JOB LINES
 * STAND, through tools/nightly/run.py with a job list holding the one entry, on
 * 2026-10-12 (night 20738) with the machine quiet (load 3 to 4):
 *   soak-h2-long      long-fold, four simulated hours: GREEN, KIT-GATE cases=899 at a
 *                     floor of 899; 1072 CPU s, 1014 s of wall with the build
 *   soak-h2-extreme   extreme-max, forty minutes: GREEN, KIT-GATE cases=149 at a floor
 *                     of 149; 1341 CPU s, 1390 s of wall with the build
 *                     (and extreme-loud, before the rotation's order was changed:
 *                     GREEN, 611 CPU s, 593 s of wall)
 * So a whole long job is about 18 minutes of this machine quiet, and the dearest
 * extreme job about 22: a little over the scaled figures above.
 *   ASSUMED, NOT MEASURED (no nightly has run): a hosted runner's core takes THREE
 *   TIMES this machine's quiet CPU time for this work. The parameter fuzz's header
 *   assumes four to six for its four processes side by side; a soak cell is one
 *   render process and this reader.
 *   soak-h2-long      52 to 65 minutes (long-defaults and long-fold; long-192)
 *   soak-h2-extreme   31 to 67 minutes (extreme-loud to extreme-max)
 * At five times instead of three: 86 to 108 and 51 to 112 minutes. Every job is well
 * inside two hours at three; at five, long-192 and extreme-max are inside it by
 * about ten minutes. long-192 cannot be made shorter: it has to pass 2^31 frames in
 * one render. If its first night runs over, it needs a longer limit, not a shorter
 * run; an extreme cell can lose windows (its floor in jobs.json with them).
 *   BOTH JOBS TOGETHER ARE 96 TO 121 MINUTES A NIGHT at three times (the pairs are
 *   long-192 with extreme-loud, long-defaults with extreme-dense, long-fold with
 *   extreme-max), 0.5 to 0.7 core-hours of this machine, quiet. tools/nightly/run.py
 *   runs every Linux entry one after another inside the draft workflow's ONE job of
 *   120 minutes, and the parameter fuzz's entry alone expects two to three hours
 *   there (its header). Each soak job fits a two-hour job of its own; the night as a
 *   whole does not fit the draft's one. The workflow is the human's to change.
 *
 * WHY NOT FOUR HOURS AT THE EXTREME SETTINGS, which is what the proposal's row asks.
 * The heavy cells render at about twice real time on this machine when it is quiet
 * (extreme-max: 2.10). Four hours of one is 1.9 core-hours here and, at the assumed
 * three times, 5.7 hours of a hosted runner: no two-hour job holds it, and the
 * proposal's "about 6 core-hours a night" is the size of one such cell there. So the
 * nightly holds FOUR HOURS where four hours can be afforded (three cells, among them
 * the 192 kHz one) and FORTY MINUTES at the extremes, and says so. The full four
 * hours of a heavy cell is one command on the dev Mac:
 *     node tools/h2_soak_check.mjs --cell composed/extreme-max --windows 899
 * The corner with every toggle on is in no cell: at eight voices it rendered at a
 * fifth of real time here (59 s of wall for 10 s, under load).
 */

/* ================================================================== the run's shape
 * None of these is a tolerance: they say what is played, for how long, and where the
 * judge looks. */
const SEED = 0xB448;            // the phrase's seed, both sizes; --seed replaces it and is printed
const STEPS = 32;               // a period is this many steps; a note event falls on a step or a half step
// The phrase in two densities. maxHeld is the most phrase notes held at once: with the drone the
// full phrase holds 7 of the pool's 8, so no HELD voice is ever stolen and the drone lives the
// whole run. The thin phrase is the same parts with smaller chords, for the cells where a full
// one cannot be afforded (192 kHz costs four times the samples).
const PHRASES = { full: { chords: [3, 4], singles: 6, maxHeld: 6 }, thin: { chords: [2, 2], singles: 2, maxHeld: 3 } };
const NOTE_LO = 45, NOTE_HI = 88, DRONE_LO = 33, DRONE_HI = 44;   // A2..E6, and the drone A1..G#2
const REF_WINDOWS = 4;          // the early reference is the median of this many judged windows (fewer in a run under 12)
const WRAP_FROM = 20;           // every power of two from 2^20 frames up that the run passes is a wrap position
const WRAP_HALF = 2048;         // the click window is this many frames each side of a wrap position: 15 of clicks()'s 512-sample frames
const WRAP_REFS = 3;            // reference windows: the same place this many periods earlier (as many as there are)
const TAIL_EXTRA_S = 0.75;      // the tail is rendered this far past the release law's end + settleFail
const DIGEST_UP_TO = 2 ** 30;   // streams up to this many frames are also digested on the reading side (harness.mjs)
const MEASURE_PROBES = 400;     // --measure reads the wrap measures at this many extra places (see layoutOf)
const PROGRESS_EVERY = 100;     // a cell tells standard error where it is every this many periods
const MAX_PROCS = 4;            // render processes side by side; changes how long a run takes and nothing else

/* ================================================================== seeded streams
 * mulberry32 and hash32 are the repo's own (tools/patchspace/space.mjs), restated as
 * tools/h2_fuzz_check.mjs restates them and for its reason: those files evaluate lab
 * text under the lab sandbox, which forbids the child process this tool needs. */
export function mulberry32(a) {
  return function () {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
export function hash32(...xs) { let h = 0x811C9DC5; for (const x of xs) { h ^= x >>> 0; h = Math.imul(h, 0x01000193) >>> 0; h ^= h >>> 13; } return h >>> 0; }

/* ================================================================== the note stream
 * ONE PHRASE, played again every period. Times are half steps (a period is 2 x STEPS
 * of them), so the same phrase is played at every sample rate and period length.
 *   the drone      one low note struck on the first frame and held to the end of the
 *                  churn: the one voice that is never restarted, so whatever drifts in
 *                  a voice has the whole run to do it
 *   a long hold    one note held for three quarters of every period
 *   two chords     each struck and released together (three and four notes in the
 *                  full phrase, two and two in the thin one)
 *   a repeated note  struck four times half a step apart, the last two WITHOUT a
 *                  release between them (two voices on one key), then one key-off
 *                  for both
 *   over the boundary  one note struck near the end of a period and released early in
 *                  the next, so a period does not start from the same silence
 *   singles        notes at drawn places, of drawn lengths
 * Pitches and velocities are drawn; no two parts share a key, so a key-off releases
 * only its own note. The phrase never holds more than its maxHeld notes at once (a
 * single that would is dropped, and the count printed). */
export function makePhrase(seed, density) {
  const D = PHRASES[density];
  if (!D) throw new Error(`h2_soak_check: no phrase density "${density}"`);
  const g = mulberry32(hash32(seed, 0x50A4)), H = 2 * STEPS, used = new Set(), ev = [];
  const fresh = (lo, hi) => { for (;;) { const n = lo + Math.floor(g() * (hi - lo + 1)); if (!used.has(n)) { used.add(n); return n; } } };
  const vel = () => 0.35 + 0.65 * g();
  const drone = [fresh(DRONE_LO, DRONE_HI), vel()];
  const held = new Array(H).fill(0);   // phrase notes held during each half step
  const span = (a, b, n) => { for (let h = a; h !== b; h = (h + 1) % H) held[h] += n; };
  const on = (h, note, v) => ev.push({ h, on: [note, v] }), off = (h, note) => ev.push({ h, off: note });
  const part = (a, b, notes) => { for (const n of notes) on(a, n, vel()); for (const n of notes) off(b, n); span(a, b, notes.length); };
  const kinds = { 'long hold': 0, chord: 0, 'repeated note': 0, 'over the boundary': 0, single: 0 };
  part(2, 50, [fresh(NOTE_LO, NOTE_HI)]); kinds['long hold']++;
  part(8, 20, Array.from({ length: D.chords[0] }, () => fresh(NOTE_LO, NOTE_HI))); kinds.chord++;
  part(36, 46, Array.from({ length: D.chords[1] }, () => fresh(NOTE_LO, NOTE_HI))); kinds.chord++;
  const rep = fresh(NOTE_LO, NOTE_HI);
  on(22, rep, vel()); off(23, rep); on(24, rep, vel()); off(25, rep); on(26, rep, vel()); on(27, rep, vel()); off(30, rep);
  span(22, 23, 1); span(24, 25, 1); span(26, 27, 1); span(27, 30, 2); kinds['repeated note']++;
  part(58, 4, [fresh(NOTE_LO, NOTE_HI)]); kinds['over the boundary']++;
  let dropped = 0;
  for (let k = 0; k < D.singles; k++) {
    const a = Math.floor(g() * H), len = 1 + Math.floor(g() * 8), b = (a + len) % H, n = fresh(NOTE_LO, NOTE_HI), v = vel();
    let room = true;
    for (let h = a; h !== b; h = (h + 1) % H) if (held[h] + 1 > D.maxHeld) room = false;
    if (!room) { dropped++; continue; }
    on(a, n, v); off(b, n); span(a, b, 1); kinds.single++;
  }
  // within a half step the releases go first, so a voice is free before the next note asks for one
  const order = ev.map((e, k) => [e, k]).sort((x, y) => x[0].h - y[0].h || (x[0].off === undefined) - (y[0].off === undefined) || x[1] - y[1]).map(x => x[0]);
  return { seed, density, drone, events: order, notes: [...used].filter(n => n !== drone[0]), kinds, dropped, maxHeld: Math.max(...held) };
}
export const phraseText = p => `${p.density}: drone ${p.drone[0]}; per period ${p.events.filter(e => e.on).length} note-ons and ${p.events.filter(e => e.off !== undefined).length} key-offs: ` +
  Object.entries(p.kinds).map(([k, n]) => `${n} ${k}${n === 1 ? '' : 's'}`).join(', ') + `; at most ${p.maxHeld} held at once beside the drone` + (p.dropped ? `; ${p.dropped} single(s) dropped for room` : '');

/* ================================================================== a cell's layout and its script
 * A CELL is one continuous render: { id, engine (adapter id), sr, block, hostSeed,
 * periodS, windows, head (commands before the first note), releaseS (the release law),
 * counterStart }. Its stream is
 *   period 0            the warm-up: played and read for non-finite samples, not judged
 *   periods 1..windows  the judged windows, one period each
 *   the tail            every key released, then silence must return
 * counterStart is where the adapter's own frame counter stands on the first frame: 0
 * for an engine (no engine can be pre-aged from outside), and just short of 2^31 or
 * 2^32 for the two pre-aged control adapters. */
export function layoutOf(cell, T, probes) {
  const periodFrames = cell.periodS * cell.sr, half = periodFrames / (2 * STEPS), slot = periodFrames / STEPS;
  if (!Number.isInteger(half)) throw new Error(`h2_soak_check: cell ${cell.id}: a period of ${cell.periodS} s at ${cell.sr} Hz is not a whole number of half steps`);
  if (!(cell.windows >= 2)) throw new Error(`h2_soak_check: cell ${cell.id}: ${cell.windows} judged window(s) cannot show a drift; at least 2`);
  const churnFrames = (cell.windows + 1) * periodFrames;
  const tailFrames = Math.ceil((cell.releaseS + T.settleFail + TAIL_EXTRA_S) * cell.sr);
  const wrapHalf = WRAP_HALF;
  // every power of two the adapter's own counter passes during the churn, as a frame of this stream
  const wraps = [];
  for (let k = WRAP_FROM; k <= 52; k++) { const w = 2 ** k - cell.counterStart; if (w > 0 && w + wrapHalf <= churnFrames) wraps.push({ power: k, at: w }); }
  // --measure only: the same two measures at seeded places where nothing wraps, so that
  // their spread on a healthy engine is known from hundreds of places, not a dozen
  for (let i = 0; i < (probes || 0) && cell.windows >= 6; i++) {
    const g = mulberry32(hash32(0xC11C, i));
    wraps.push({ power: null, at: (3 + Math.floor(g() * (cell.windows - 4))) * periodFrames + Math.floor(g() * periodFrames) });
  }
  return { sr: cell.sr, periodFrames, half, slot, windows: cell.windows, churnFrames, tailFrames, totalFrames: churnFrames + tailFrames, wrapHalf, wraps,
    releaseFrames: Math.ceil(cell.releaseS * cell.sr), refWindows: Math.max(1, Math.min(REF_WINDOWS, Math.floor(cell.windows / 3))) };
}
export function soakCmds(cell, phrase, lay) {
  const cmds = [...cell.head, ['on', phrase.drone[0], mtof(phrase.drone[0]), phrase.drone[1]]];
  for (let p = 0; p <= lay.windows; p++) {
    let at = 0;
    for (const e of phrase.events) {
      if (e.h * lay.half > at) { cmds.push(['render', e.h * lay.half - at]); at = e.h * lay.half; }
      cmds.push(e.on ? ['on', e.on[0], mtof(e.on[0]), e.on[1]] : ['off', e.off]);
    }
    cmds.push(['render', lay.periodFrames - at]);
  }
  for (const n of [phrase.drone[0], ...phrase.notes]) cmds.push(['off', n]);
  cmds.push(['render', lay.tailFrames]);
  return cmds;
}
export const jobOf = (cell, phrase, lay) => ({ name: `soak ${cell.id}`, engine: cell.engine, sr: cell.sr, seed: cell.hostSeed >>> 0, block: cell.block, vary: false, pre: true, cmds: soakCmds(cell, phrase, lay) });

/* ================================================================== reading the stream
 * The reader is handed each chunk as it arrives and keeps NUMBERS, never the render:
 * its buffers are one SLOT of each channel (a step of the period; a quarter second
 * to a second) and the short windows around the wrap positions. Every measure is
 * tools/patchspace/metrics.mjs's (rms, dc, peak, nonFinite, clicks) over a slot or a
 * wrap window; this file only adds slots up into windows. The proposal forbids a
 * second metrics library (section 5).
 *
 * WHAT IS READ WHERE. Non-finite samples: the output, every frame of the stream.
 * Level, DC and the wrap measures: the PRE-LIMITER BUS (the header says why).
 * "Carried signal" and silence: the OUTPUT. A non-finite sample is counted, then
 * read as zero by the other measures, as metrics.mjs's analyse() does. */
const dB = x => (x > 0 ? 20 * Math.log10(x) : -Infinity);
const median = xs => { const s = [...xs].sort((a, b) => a - b), n = s.length; return n % 2 ? s[(n - 1) / 2] : (s[n / 2 - 1] + s[n / 2]) / 2; };
function zeroNonFinite(x) { for (let i = 0; i < x.length; i++) if (!Number.isFinite(x[i])) x[i] = 0; }

export function makeReader(lay, T) {
  const S = lay.slot, P = lay.periodFrames;
  const b = { L: new Float64Array(S), R: new Float64Array(S), pL: new Float64Array(S), pR: new Float64Array(S) };
  const newAcc = () => ({ slots: 0, ms: [0, 0], mean: [0, 0], outMs: [0, 0], busPeak: 0, nonFinite: 0, rail: 0 });
  const facts = { frames: 0, windows: [], slotMs: [[], []], nonFinite: 0, firstNonFinite: null, wraps: [], lastLoud: -1 };
  let fill = 0, acc = newAcc();

  // the windows around the wrap positions, and the same places in earlier periods
  const caps = [];
  for (const w of lay.wraps) {
    const group = { power: w.power, at: w.at, parts: [] };
    for (let k = 0; k <= WRAP_REFS; k++) {
      const start = w.at - k * P - lay.wrapHalf;
      if (start < P) break;   // the warm-up period is not a reference, and a wrap inside it is not judged
      const part = { group, k, start, end: start + 2 * lay.wrapHalf, pL: null, pR: null, m: null };
      group.parts.push(part); caps.push(part);
    }
    facts.wraps.push(group);
  }
  caps.sort((x, y) => x.start - y.start || x.end - y.end);
  let capFrom = 0;

  const measure = part => {
    const out = { click: [0, 0] };
    [part.pL, part.pR].forEach((x, ch) => { zeroNonFinite(x); out.click[ch] = clicks(x).worstDb; });
    part.m = out; part.pL = part.pR = null;   // the samples are let go; the numbers stay
  };

  const closeSlot = () => {
    const nfOut = nonFinite(b.L, b.R), nfBus = nonFinite(b.pL, b.pR);
    if (nfOut || nfBus) { for (const x of [b.L, b.R, b.pL, b.pR]) zeroNonFinite(x); }
    // an output sample ON the rail is finite and its bus reads infinity: that is the difference
    acc.nonFinite += nfOut; acc.rail += Math.max(0, nfBus - nfOut);
    const d = dc(b.pL, b.pR), rL = rms(b.pL), rR = rms(b.pR), oL = rms(b.L), oR = rms(b.R);
    acc.ms[0] += rL * rL; acc.ms[1] += rR * rR; acc.mean[0] += d.dcL; acc.mean[1] += d.dcR;
    acc.outMs[0] += oL * oL; acc.outMs[1] += oR * oR;
    acc.busPeak = Math.max(acc.busPeak, peak(b.pL), peak(b.pR));
    facts.slotMs[0].push(rL * rL); facts.slotMs[1].push(rR * rR);   // two numbers a slot: 115200 for four hours of half-second slots
    acc.slots++; fill = 0;
    if (acc.slots === STEPS) {
      const n = acc.slots;
      facts.windows.push({ ms: acc.ms.map(x => x / n), level: acc.ms.map(x => dB(Math.sqrt(x / n))), dc: acc.mean.map(x => x / n),
        out: acc.outMs.map(x => Math.sqrt(x / n)), busPeak: acc.busPeak, nonFinite: acc.nonFinite, rail: acc.rail });
      acc = newAcc();
    }
  };

  const push = c => {
    if (!c.preL) throw new Error('h2_soak_check: a soak job must ask for the pre-limiter bus');
    const n = c.frames, at = c.at;
    if (at !== facts.frames) throw new Error(`h2_soak_check: chunk at frame ${at}, expected ${facts.frames}`);
    const nf = nonFinite(c.L, c.R);
    if (nf) {
      facts.nonFinite += nf;
      if (facts.firstNonFinite === null) for (let i = 0; i < n; i++) if (!Number.isFinite(c.L[i]) || !Number.isFinite(c.R[i])) { facts.firstNonFinite = at + i; break; }
    }
    // the wrap windows this chunk overlaps
    while (capFrom < caps.length && caps[capFrom].end <= at) capFrom++;
    for (let k = capFrom; k < caps.length && caps[k].start < at + n; k++) {
      const part = caps[k];
      if (part.m || part.end <= at) continue;
      if (!part.pL) { part.pL = new Float64Array(2 * lay.wrapHalf); part.pR = new Float64Array(2 * lay.wrapHalf); }
      const a = Math.max(at, part.start), z = Math.min(at + n, part.end);
      part.pL.set(c.preL.subarray(a - at, z - at), a - part.start); part.pR.set(c.preR.subarray(a - at, z - at), a - part.start);
      if (z === part.end) measure(part);
    }
    // the churn, slot by slot
    let i = 0;
    while (i < n && at + i < lay.churnFrames) {
      const take = Math.min(S - fill, n - i, lay.churnFrames - (at + i));
      b.L.set(c.L.subarray(i, i + take), fill); b.R.set(c.R.subarray(i, i + take), fill);
      b.pL.set(c.preL.subarray(i, i + take), fill); b.pR.set(c.preR.subarray(i, i + take), fill);
      fill += take; i += take;
      if (fill === S) closeSlot();
    }
    // the tail: the last frame that is not silent on either channel (a NaN is not silent)
    for (let j = n - 1; j >= i; j--) if (!(Math.abs(c.L[j]) <= T.silentFloor) || !(Math.abs(c.R[j]) <= T.silentFloor)) { facts.lastLoud = at + j; break; }
    facts.frames += n;
  };
  return { push, facts };
}

/* ================================================================== the judge
 * One function per rule, each over the reader's numbers and nothing else. A finding
 * is { kind, ch ('L', 'R' or null), band ('fail' or 'grey'), text }. */
const CH = ['L', 'R'];
const judgedOf = facts => facts.windows.slice(1);   // window 0 is the warm-up
const fmt = (x, d = 3) => (Number.isFinite(x) ? (x >= 0 ? '+' : '') + x.toFixed(d) : String(x));
const bandOf = (x, pass, fail) => (x > fail ? 'fail' : x > pass ? 'grey' : null);

function ruleNonFinite(facts, lay) {
  return facts.nonFinite ? [{ kind: 'non-finite', ch: null, band: 'fail', count: facts.nonFinite,
    text: `${facts.nonFinite} non-finite output sample(s), the first at frame ${facts.firstNonFinite} (${(facts.firstNonFinite / lay.sr).toFixed(3)} s)` }] : [];
}
function ruleBus(facts, lay, T) {
  const w = judgedOf(facts), rail = w.reduce((a, x) => a + x.rail, 0), pk = Math.max(...w.map(x => x.busPeak));
  if (rail) return [{ kind: 'bus-unread', ch: null, band: 'fail', text: `${rail} sample(s) on the limiter's rail: the bus was not read there` }];
  return pk > T.busReadMax ? [{ kind: 'bus-unread', ch: null, band: 'fail', text: `bus peak ${pk.toFixed(3)} is above ${T.busReadMax}: too close to the rail to read` }] : [];
}
function ruleSignal(facts, lay, T) {
  const quiet = judgedOf(facts).filter(x => !(Math.min(x.out[0], x.out[1]) > T.signalFloor)).length;
  return quiet ? [{ kind: 'no-signal', ch: null, band: 'fail', text: `${quiet} of ${lay.windows} judged windows carried no signal on one channel or both: a soak of silence proves nothing` }] : [];
}
/* LEVEL: every judged window after the reference windows, against the median of the
   reference windows, per bus channel. */
export function levelOf(facts, lay, ch) {
  const w = judgedOf(facts), ref = median(w.slice(0, lay.refWindows).map(x => x.level[ch]));
  let worst = 0, at = -1;
  w.forEach((x, k) => { if (k >= lay.refWindows && Math.abs(x.level[ch] - ref) > Math.abs(worst)) { worst = x.level[ch] - ref; at = k + 1; } });
  return { ref, worst, at, last: w[w.length - 1].level[ch] - ref };
}
function ruleLevel(facts, lay, T) {
  return CH.flatMap((name, ch) => {
    const v = levelOf(facts, lay, ch), bd = bandOf(Math.abs(v.worst), T.levelPass, T.levelFail);
    return bd ? [{ kind: 'level-drift', ch: name, band: bd, value: v.last,
      text: `LEVEL drift ${fmt(v.worst)} dB in window ${v.at} of ${lay.windows} (${fmt(v.last)} dB in the last), against ${v.ref.toFixed(3)} dB early in the run` }] : [];
  });
}
/* DC: the same, on the window's bus mean, as a share of the reference level. */
export function dcOf(facts, lay, ch) {
  const w = judgedOf(facts), early = w.slice(0, lay.refWindows), ref = median(early.map(x => x.dc[ch])), lvl = Math.sqrt(median(early.map(x => x.ms[ch])));
  let worst = 0, at = -1;
  w.forEach((x, k) => { const d = (x.dc[ch] - ref) / lvl; if (k >= lay.refWindows && Math.abs(d) > Math.abs(worst)) { worst = d; at = k + 1; } });
  return { ref, lvl, worst, at, last: (w[w.length - 1].dc[ch] - ref) / lvl };
}
function ruleDc(facts, lay, T) {
  return CH.flatMap((name, ch) => {
    const v = dcOf(facts, lay, ch), bd = bandOf(Math.abs(v.worst), T.dcPass, T.dcFail);
    return bd ? [{ kind: 'dc-drift', ch: name, band: bd, value: v.last, abs: v.last * v.lvl,
      text: `DC drift ${fmt(v.worst, 5)} of the level in window ${v.at} of ${lay.windows} (${fmt(v.last, 5)} in the last), from a mean of ${v.ref.toExponential(2)} early in the run` }] : [];
  });
}
/* WRAP: two measures at each power-of-two frame count the adapter's counter passes.
     click  metrics.mjs clicks() (its worst frame, in dB over the window's median frame)
            on the 4096 frames around the wrap, minus the largest of the same measure
            at the same place in the periods before: what is there every period (a
            note-on, a sync edge) cancels, and what is there once does not.
     step   the level of the ONE PERIOD after the slot that holds the wrap over the
            level of the one period before it. Any stretch exactly one period long
            holds the whole phrase once, wherever it starts, so the two are the same
            notes and a healthy engine reads 0 dB to within its window-to-window
            spread (the LEVEL rule's own noise).
   A wrap is judged only where both can be: one period of reference before it, not
   counting the warm-up, and one period of churn after it. */
export function wrapOf(facts, lay, group, ch) {
  const here = group.parts[0], refs = group.parts.slice(1), s = Math.floor(group.at / lay.slot), ms = facts.slotMs[ch];
  if (!here || !refs.length || s - STEPS < STEPS || s + STEPS >= ms.length) return null;
  let before = 0, after = 0;
  for (let k = 1; k <= STEPS; k++) { before += ms[s - k]; after += ms[s + k]; }
  // silence either side is no step; sound on one side only is an infinite one
  const step = before > 0 && after > 0 ? dB(Math.sqrt(after / before)) : before === after ? 0 : after > 0 ? Infinity : -Infinity;
  return { click: here.m.click[ch] - Math.max(...refs.map(p => p.m.click[ch])), step, refs: refs.length };
}
function ruleWrap(facts, lay, T) {
  const out = [];
  for (const g of facts.wraps) CH.forEach((name, ch) => {
    const v = wrapOf(facts, lay, g, ch);
    if (!v) return;
    const where = `at 2^${g.power} on the counter (frame ${g.at} of the stream, ${(g.at / lay.sr).toFixed(3)} s)`;
    const c = bandOf(v.click, T.clickPass, T.clickFail), s = bandOf(Math.abs(v.step), T.stepPass, T.stepFail);
    if (c) out.push({ kind: 'wrap', ch: name, band: c, how: 'click', power: g.power, value: v.click, text: `WRAP click ${where}: ${fmt(v.click, 2)} dB over its ${v.refs} reference window(s)` });
    if (s) out.push({ kind: 'wrap', ch: name, band: s, how: 'step', power: g.power, value: v.step, text: `WRAP level step ${where}: ${fmt(v.step)} dB from the period before it to the period after` });
  });
  return out;
}
/* SILENCE: after the last key-off and the release law, the output is under
   silentFloor within settlePass (grey to settleFail) and stays there to the end. */
export function settleOf(facts, lay) { return (facts.lastLoud + 1 - (lay.churnFrames + lay.releaseFrames)) / lay.sr; }
function ruleSilence(facts, lay, T) {
  const s = settleOf(facts, lay), bd = bandOf(s, T.settlePass, T.settleFail);
  return bd ? [{ kind: 'not-silent', ch: null, band: bd, text: `the output was last above -90 dBFS ${fmt(s)} s after the release law's end (${((facts.lastLoud + 1 - lay.churnFrames) / lay.sr).toFixed(3)} s after the last key-off)` }] : [];
}
export const RULES = [ruleNonFinite, ruleBus, ruleSignal, ruleLevel, ruleDc, ruleWrap, ruleSilence];
export function judge(facts, lay, T) {
  if (facts.frames !== lay.totalFrames || facts.windows.length !== lay.windows + 1) throw new Error(`h2_soak_check: read ${facts.frames} frames and ${facts.windows.length} windows; the cell is ${lay.totalFrames} and ${lay.windows + 1}`);
  return RULES.flatMap(rule => rule(facts, lay, T));
}

/* what one cell read, as lines: the same lines for an engine cell and for a control */
export function factLines(facts, lay, T) {
  const w = judgedOf(facts), out = [];
  const outDb = w.flatMap(x => x.out.map(dB)), carried = w.filter(x => Math.min(x.out[0], x.out[1]) > T.signalFloor).length;
  out.push(`non-finite ${facts.nonFinite} of ${2 * facts.frames} output samples; on the rail ${w.reduce((a, x) => a + x.rail, 0)}; bus peak ${Math.max(...w.map(x => x.busPeak)).toFixed(3)}`);
  out.push(`signal    ${carried} of ${lay.windows} judged windows carried signal (output RMS ${Math.min(...outDb).toFixed(1)} to ${Math.max(...outDb).toFixed(1)} dBFS)`);
  out.push('LEVEL     ' + CH.map((name, ch) => { const v = levelOf(facts, lay, ch); return `${name} ${v.ref.toFixed(3)} dB early, worst ${fmt(v.worst)} dB (${v.at < 0 ? "no window differs" : "window " + v.at}), last ${fmt(v.last)} dB`; }).join('; '));
  out.push('DC        ' + CH.map((name, ch) => { const v = dcOf(facts, lay, ch); return `${name} mean ${v.ref.toExponential(2)} early, worst ${fmt(v.worst, 5)} of the level (${v.at < 0 ? "no window differs" : "window " + v.at}), last ${fmt(v.last, 5)}`; }).join('; '));
  const real = facts.wraps.filter(g => g.power !== null), judgedWraps = real.filter(g => wrapOf(facts, lay, g, 0)), unjudged = real.filter(g => !wrapOf(facts, lay, g, 0));
  for (const g of judgedWraps) out.push(`WRAP      2^${g.power} at ${(g.at / lay.sr).toFixed(3)} s: ` + CH.map((name, ch) => { const v = wrapOf(facts, lay, g, ch); return `${name} click ${fmt(v.click, 2)} dB, step ${fmt(v.step)} dB`; }).join('; ') + ` (${wrapOf(facts, lay, g, 0).refs} reference window(s))`);
  out.push(`WRAP      ${judgedWraps.length} power(s) of two judged${judgedWraps.length ? '' : ' (none)'}` + (unjudged.length ? `; passed but not judged: ${unjudged.map(g => `2^${g.power}`).join(', ')} (no whole period of reference before it after the warm-up, or no whole period after it)` : ''));
  out.push(facts.lastLoud < 0 ? 'SILENCE   the tail was never above -90 dBFS (every voice had already decayed when the keys came up)' : `SILENCE   last above -90 dBFS ${fmt(settleOf(facts, lay))} s after the release law's end`);
  return out;
}

/* ================================================================== what each engine declares, and its cells
 * One entry per ENROLLED engine. An enrolled engine with no entry is red: a soak
 * cannot claim an engine it was given no cells for.
 *   table     the declared parameter table (for the release key's default)
 *   release   seconds from the last key-off to a freed voice, from the patch's sets
 *   cells     { id, size, patch (a reference patch of h2/harness/patches.json), sets
 *             (laid over it, before snap), sr, periodS, windows, density (the phrase) }
 *             size 'short' is the local run of ./verify full; 'long' and 'extreme'
 *             are the two nightly rotations (the header's NIGHTLY section); 'local'
 *             is run by hand with --cell and belongs to no rotation.
 * A CELL WHOSE PATCH IS TOO LOUD OR SILENT SETS ITS OWN GAIN, low enough that the bus
 * stays readable (under busReadMax) and high enough to be heard. The header says why
 * that loses nothing (THE BUS, NOT THE OUTPUT). */
/* ADR-221 A1: every cell on a ledger patch that took the patch's level writes gain 0.35, the level its
   absolute readings were calibrated at (SILENCE's -90 dBFS line, the signal check, busReadMax's margin).
   ADR-221 lowered the engine's default, and so the regenerated ledger patches, to 0.0875: 12 dB less
   level against those absolute lines. A cell that already sets its own gain keeps it. */
const A1_GAIN = { gain: 0.35 };
const P16 = 16, FOUR_HOURS = 4 * 3600 / P16 - 1, FORTY_MIN = 40 * 60 / P16 - 1;   // judged windows: 899 and 149, after one warm-up period
const ENGINES = {
  composed: {
    table: 'tools/patchspace/dependency_tree.json',
    // h2/engine/engine.h: in release `env -= env * rC`, rC = 1 - exp(-4 / (R ms as samples)), and the voice
    // is freed below 1e-4: from an envelope of at most 1 that is R * ln(1e4) / 4 (the parameter fuzz's law).
    release: v => v.R / 1000 * Math.log(1e4) / 4,
    cells: [
      // SHORT: the oracle defaults at 48 kHz, and the 192 kHz path on the cheapest patch there is
      { id: 'short', size: 'short', patch: 'ledger/defaults', sets: { ...A1_GAIN }, sr: 48000, periodS: 8, windows: 16, density: 'full' },
      { id: 'short-192', size: 'short', patch: 'corner/continuous-min', sets: { gain: 0.35 }, sr: 192000, periodS: 8, windows: 6, density: 'thin' },
      // LONG: four simulated hours each. 192 kHz, past 2^31 frames; the oracle defaults at
      // 48 kHz; two blades in series at 44.1 kHz.
      { id: 'long-192', size: 'long', patch: 'ledger/quarter-sync', sets: { ...A1_GAIN }, sr: 192000, periodS: P16, windows: FOUR_HOURS, density: 'thin' },
      { id: 'long-defaults', size: 'long', patch: 'ledger/defaults', sets: { ...A1_GAIN }, sr: 48000, periodS: P16, windows: FOUR_HOURS, density: 'full' },
      { id: 'long-fold', size: 'long', patch: 'ledger/fold-over-sync', sets: { ...A1_GAIN }, sr: 44100, periodS: P16, windows: FOUR_HOURS, density: 'full' },
      // LOCAL: run by hand (--cell), in no rotation. Four hours of every continuous key at its
      // declared minimum: sustain 0, so every held voice decays and is never freed. Without
      // flush-to-zero (the harness sets none) those voices compute on subnormal numbers for as
      // long as their keys are down. On arm64 that costs little; what it costs on an x86 runner
      // is not measured and can be tens of times, so this cell is kept off the nightly.
      { id: 'long-min', size: 'local', patch: 'corner/continuous-min', sets: { gain: 0.35 }, sr: 48000, periodS: P16, windows: FOUR_HOURS, density: 'full' },
      // EXTREME: forty simulated minutes each, because four hours of these cannot be afforded
      // nightly (RUN TIME, above). The loudest chord the parameter fuzz has found (its named case
      // `loud-chord`: nine members at zero detune, blade 2 on), at the fuzz's own rate; the
      // densest ledger preset; every continuous key at its declared maximum. IN THIS ORDER, so
      // that the night of the 192 kHz cell, the dearest long one, carries the cheapest of these.
      { id: 'extreme-loud', size: 'extreme', patch: 'ledger/defaults', sets: { base: 3, b2on: 1, mode2: 3, c2: 1, mirror2: 3, N: 9, detune: 0, gain: 0.1 }, sr: 44100, periodS: P16, windows: FORTY_MIN, density: 'full' },
      { id: 'extreme-dense', size: 'extreme', patch: 'ledger/glass-horde-pad', sets: { ...A1_GAIN }, sr: 48000, periodS: P16, windows: FORTY_MIN, density: 'full' },
      { id: 'extreme-max', size: 'extreme', patch: 'corner/continuous-max', sets: { gain: 0.35 }, sr: 48000, periodS: P16, windows: FORTY_MIN, density: 'full' },
    ],
  },
};
const HOST_SEED = 0x50AC;   // every cell's host random stream (the engine's own draws)

export function engineCell(engine, spec, windows) {
  const cfg = ENGINES[engine], p = referencePatch(spec.patch);
  if (p.engine !== engine) throw new Error(`h2_soak_check: cell ${spec.id}: patch ${spec.patch} is not a patch of "${engine}"`);
  const tree = JSON.parse(readFileSync(join(ROOT, cfg.table), 'utf8'));
  const values = Object.fromEntries(Object.entries(tree.params).map(([k, q]) => [k, q.range && q.range.default]));
  for (const [k, v] of p.sets) values[k] = v;
  Object.assign(values, spec.sets);
  if (!(values.gain > 0)) throw new Error(`h2_soak_check: cell ${spec.id}: gain ${values.gain} is not audible; a soak of silence proves nothing`);
  return { id: `${engine}/${spec.id}`, engine, size: spec.size, sr: spec.sr, block: 128, hostSeed: HOST_SEED, periodS: spec.periodS, windows: windows || spec.windows, density: spec.density,
    head: patchCmds(p, spec.sets), releaseS: cfg.release(values), counterStart: 0, control: false,
    text: `${spec.patch}${Object.keys(spec.sets).length ? ' with ' + Object.entries(spec.sets).map(([k, v]) => `${k} ${v}`).join(', ') : ''}, the ${spec.density} phrase` };
}

/* ================================================================== running cells
 * Each cell is one render process, read as it is written. Several run side by side;
 * results are filed by cell, so the number of processes changes how long a run takes
 * and nothing else. A process that dies is a finding about that cell ('crash'). */
async function mapPool(items, procs, fn) {
  const out = new Array(items.length);
  let next = 0;
  await Promise.all(Array.from({ length: Math.max(1, Math.min(procs, items.length)) }, async () => {
    for (;;) { const k = next++; if (k >= items.length) return; out[k] = await fn(items[k], k); }
  }));
  return out;
}
export async function runCell(ctx, cell) {
  const T = ctx.T, lay = layoutOf(cell, T, ctx.probes), reader = makeReader(lay, T);
  let summary = null, crash = null, told = 0;
  // a long cell says where it is, on standard error only: a night's log shows it is alive
  const push = c => { reader.push(c); const k = reader.facts.windows.length; if (k >= told + PROGRESS_EVERY) { told = k - k % PROGRESS_EVERY; console.error(`h2_soak_check: ${cell.id}: ${k} of ${lay.windows + 1} periods read`); } };
  try {
    const done = await runStream([jobOf(cell, ctx.phrases[cell.density], lay)], push, { bin: ctx.bin, digest: lay.totalFrames <= DIGEST_UP_TO });
    summary = done[0].summary;
  } catch (e) {
    if (!e.crash) throw e;   // a refused request or a broken stream stops the run; only a dead process is a finding
    crash = e.crash;
  }
  ctx.renders++;
  const findings = crash ? [{ kind: 'crash', ch: null, band: 'fail', text: `the render process died on ${crash} after ${reader.facts.frames} frames (${(reader.facts.frames / cell.sr).toFixed(1)} s)` }] : judge(reader.facts, lay, T);
  if (summary && summary.unknownKeys && !cell.control) findings.push({ kind: 'unknown-key', ch: null, band: 'fail', text: `the engine refused ${summary.unknownKeys} key(s), the first "${summary.firstUnknown}"` });
  return { cell, lay, facts: reader.facts, summary, findings, crash };
}
const cellLine = (cell, lay) => `${cell.id}  ${cell.text}; ${cell.sr} Hz, block ${cell.block}, period ${cell.periodS} s x ${lay.windows + 1} (1 warm-up + ${lay.windows} judged) = ${(lay.churnFrames / cell.sr)} s, tail ${(lay.tailFrames / cell.sr).toFixed(2)} s (release law ${cell.releaseS.toFixed(3)} s); ${lay.totalFrames} frames`;


/* ================================================================== what a run can see
 * The slowest steady drift a cell can find: one that moves the LAST judged window
 * past the line, measured from the middle of the reference windows. From the bands
 * and the cell's length alone; printed for every cell of every run. */
export function reachOf(lay, cell, T) {
  const spanS = (lay.windows - (lay.refWindows + 1) / 2) * cell.periodS;
  return { spanS, levelFail: T.levelFail / spanS * 3600, levelGrey: T.levelPass / spanS * 3600, dcFail: T.dcFail / spanS * 3600, dcGrey: T.dcPass / spanS * 3600 };
}
const reachLine = (cell, lay, T) => { const r = reachOf(lay, cell, T); return `REACH ${cell.id}  over ${r.spanS} s: a steady level drift faster than ${r.levelFail.toPrecision(3)} dB an hour is red (grey from ${r.levelGrey.toPrecision(3)}); a steady DC creep faster than ${r.dcFail.toPrecision(3)} of the level an hour is red (grey from ${r.dcGrey.toPrecision(3)}). Anything slower is not seen.`; };

/* ================================================================== the planted faults
 * h2/harness/control_soak.h, restated. expect() works out from these constants and the
 * cell's layout, in closed form, what each plant must read as. It shares NO code with
 * the reader or the judge: a change to either moves the reading and not the prediction.
 *   level   the steady adapter repeats itself each period, so a window k periods later
 *           is the same samples times 2^(k x period / doubleS): the last window against
 *           the median of the reference windows (their middle, as the level rises) is
 *           20 log10(2) x (windows - (ref + 1) / 2) x period / doubleS dB.
 *   dc      the window's mean is the offset at its middle: the same span x dcPerS x gain.
 *   wrap31  the level after the wrap is wrapStep times the level before: a step of
 *           20 log10(wrapStep) dB, and the same drift in every window after it.
 *   wrap32  a click and nothing else: the pilot's level is the same before and after.
 *   nan     one NaN at nanFromS and every nanEveryS after it while a key is held, and
 *           the drone is held to the end of the churn. */
const PLANT = { gain: 0.35, releaseS: 0.06, freeBelow: 1e-4, levelDoubleS: 235, dcPerS: 4e-5, ageS: 51.3, wrapStep: 1.26, nanFromS: 70, nanEveryS: 1.5, hangAmp: 1e-3 };
const CONTROL_CELL = { sr: 48000, periodS: 8, windows: 12, density: 'full' };
const PLANT_TOL_DB = 0.03;      // a planted level, in dB: the drone is the one part that does not repeat exactly
const PLANT_TOL_DC = 0.05;      // a planted offset, relatively. The reading also carries the phrase's own window-to-window mean
                                // (a window is not a whole number of the low notes' cycles): the steady control reads up to
                                // 2.7e-4 of the level, which is 1 % of this plant
const spanOf = lay => lay.windows - (lay.refWindows + 1) / 2;   // periods from the reference's middle to the last window
const PLANTS = [
  { id: 'control/soak-steady', what: 'no fault: the same churn through a steady source', counter: () => 0, expect: () => [], may: [] },
  { id: 'control/soak-level', what: `the right bus doubles every ${PLANT.levelDoubleS} s`, counter: () => 0, may: [],
    expect: lay => [{ kind: 'level-drift', ch: 'R', value: 20 * Math.log10(2) * spanOf(lay) * CONTROL_CELL.periodS / PLANT.levelDoubleS, tol: PLANT_TOL_DB }] },
  { id: 'control/soak-dc', what: `the right bus gains an offset of ${PLANT.dcPerS} x gain a second`, counter: () => 0, may: [],
    expect: lay => [{ kind: 'dc-drift', ch: 'R', abs: PLANT.dcPerS * PLANT.gain * spanOf(lay) * CONTROL_CELL.periodS, rel: PLANT_TOL_DC }] },
  { id: 'control/soak-wrap31', what: `a signed 32-bit frame counter that starts ${PLANT.ageS} s short of 2^31; the right bus steps by x${PLANT.wrapStep} when it goes negative`,
    counter: sr => 2 ** 31 - Math.round(PLANT.ageS * sr), may: [{ kind: 'wrap', ch: 'R', how: 'click', power: 31 }],
    expect: () => [{ kind: 'wrap', ch: 'R', how: 'step', power: 31, value: 20 * Math.log10(PLANT.wrapStep), tol: PLANT_TOL_DB }, { kind: 'level-drift', ch: 'R', value: 20 * Math.log10(PLANT.wrapStep), tol: PLANT_TOL_DB }] },
  { id: 'control/soak-wrap32', what: `a 32-bit frame counter that starts ${PLANT.ageS} s short of 2^32; a pilot tone on the right bus reads its phase from it and jumps at the wrap`,
    counter: sr => 2 ** 32 - Math.round(PLANT.ageS * sr), may: [], expect: () => [{ kind: 'wrap', ch: 'R', how: 'click', power: 32 }] },
  { id: 'control/soak-nan', what: `a NaN on the right output at ${PLANT.nanFromS} s and every ${PLANT.nanEveryS} s after it, while a key is held`, counter: () => 0, may: [],
    expect: lay => [{ kind: 'non-finite', ch: null, count: Math.ceil((lay.churnFrames - Math.round(PLANT.nanFromS * lay.sr)) / Math.round(PLANT.nanEveryS * lay.sr)) }] },
  { id: 'control/soak-hang', what: `after the last key-off the right bus keeps a level of ${PLANT.hangAmp} x gain and never stops`, counter: () => 0, may: [], expect: () => [{ kind: 'not-silent', ch: null }] },
  // Two more, on adapters that already exist. A soak of silence must be red, and so must a
  // cell whose gain puts the bus on the limiter's rail: both are runs that could show nothing.
  { id: 'control/silent', what: 'an adapter that returns silence whatever is played (harness.h)', counter: () => 0, may: [], expect: () => [{ kind: 'no-signal', ch: null }] },
  { id: 'control/soak-steady', label: 'control/soak-steady at gain 400', gain: 400, what: 'the steady source with its gain set far too high: the output sits on the limiter\'s rail', counter: () => 0, anyElse: true, may: [],
    expect: () => [{ kind: 'bus-unread', ch: null }] },
];
const controlCell = plant => ({ id: plant.label || plant.id, engine: plant.id, size: 'control', sr: CONTROL_CELL.sr, block: 128, hostSeed: HOST_SEED, periodS: CONTROL_CELL.periodS, windows: CONTROL_CELL.windows, density: CONTROL_CELL.density,
  head: [['set', 'gain', plant.gain || PLANT.gain]], releaseS: PLANT.releaseS * Math.log(1 / PLANT.freeBelow), counterStart: plant.counter(CONTROL_CELL.sr), control: true, text: plant.what });
/* Does a finding answer an expectation? Kind, channel, the wrap's power and measure, and
   the number where one was predicted. */
function answers(f, e) {
  if (f.kind !== e.kind || f.ch !== e.ch || (e.how && f.how !== e.how) || (e.power && f.power !== e.power)) return false;
  if (e.count !== undefined) return f.count === e.count;
  if (e.value !== undefined) return Math.abs(f.value - e.value) <= e.tol;
  if (e.abs !== undefined) return Math.abs(f.abs - e.abs) <= e.rel * e.abs;
  return true;
}
const expectText = e => `${e.kind}${e.how ? ' (' + e.how + ')' : ''}${e.ch ? ' on ' + e.ch : ''}` + (e.count !== undefined ? `, ${e.count} sample(s)` : e.value !== undefined ? `, ${fmt(e.value)} dB` : e.abs !== undefined ? `, an offset of ${e.abs.toExponential(3)}` : '');
/* A plant is read RIGHT when every expected finding is there, in the fail band, with
   its number, and nothing else is reported in any band (but what the plant may also show). */
export function controlVerdict(plant, r) {
  const want = plant.expect(r.lay), bad = [], used = new Set();
  for (const e of want) {
    const k = r.findings.findIndex((f, i) => !used.has(i) && f.band === 'fail' && answers(f, e));
    if (k < 0) bad.push(`the planted ${expectText(e)} was NOT FOUND`); else used.add(k);
  }
  r.findings.forEach((f, i) => { if (!used.has(i) && !plant.anyElse && !plant.may.some(e => answers(f, e))) bad.push(`reported what was not planted: ${f.band} ${f.kind}${f.ch ? ' on ' + f.ch : ''}: ${f.text}`); });
  return { want, bad };
}

/* ================================================================== the streaming read against the whole-file read
 * harness.mjs's runStream must hand over the samples run() returns: the same frames,
 * the same bits (a NaN included), the same summary digest, and its own digest of what
 * it read. Two jobs in one stream, one with the pre-limiter bus and varying blocks,
 * one without. A row of every run, because every other row depends on it. */
export async function streamRow(ctx, engine) {
  const p = patchesFor(engine).find(q => q.expect === 'loud');
  if (!p) throw new Error(`h2_soak_check: "${engine}" has no reference patch that is expected to be loud`);
  const jobs = [jobFor({ patch: p, sr: 48000, seconds: 1.5, hold: 1.0, pre: true, block: { vary: 96 }, name: 'stream-row with the bus' }), jobFor({ patch: p, sr: 44100, seconds: 0.8, block: 64, name: 'stream-row without' })];
  const whole = run(jobs, { bin: ctx.bin }), got = jobs.map(() => ({ L: [], R: [], pL: [], pR: [] }));
  let chunks = 0;
  const done = await runStream(jobs, c => { chunks++; const g = got[c.job]; g.L.push(c.L.slice()); g.R.push(c.R.slice()); if (c.preL) { g.pL.push(c.preL.slice()); g.pR.push(c.preR.slice()); } }, { bin: ctx.bin, digest: true });
  ctx.renders += 2;
  const sameBits = (parts, ref) => {   // bit for bit, so a NaN equals a NaN and -0 is not 0
    let at = 0;
    for (const part of parts) { const a = new BigUint64Array(part.buffer, part.byteOffset, part.length), b = new BigUint64Array(ref.buffer, ref.byteOffset + at * 8, part.length); for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false; at += part.length; }
    return at === ref.length;
  };
  let frames = 0;
  const bad = [];
  jobs.forEach((job, k) => {
    const w = whole[k], g = got[k], d = done[k];
    frames += w.L.length;
    if (!sameBits(g.L, w.L) || !sameBits(g.R, w.R)) bad.push(`job ${k}: the output samples differ`);
    if (w.preL ? !(sameBits(g.pL, w.preL) && sameBits(g.pR, w.preR)) : g.pL.length > 0) bad.push(`job ${k}: the pre-limiter bus differs`);
    if (d.frames !== w.L.length || d.summary.digest !== w.summary.digest || d.digestRead !== w.summary.digest) bad.push(`job ${k}: ${d.frames} frames, digest ${d.summary.digest}, read as ${d.digestRead}; the whole-file read has ${w.L.length} and ${w.summary.digest}`);
  });
  return { bad, frames, chunks, digests: whole.map(w => w.summary.digest) };
}

/* ================================================================== the run */
/* Every way out of a gate run ends in the verdict line: see "THE LAST LINE IS THE VERDICT". */
const verdictLine = text => console.log(`h2_soak_check: ${text}`);
const SIZES = ['short', 'long', 'extreme'];
function usage(why) {
  console.error(`h2_soak_check: ${why}\nusage: node tools/h2_soak_check.mjs [--bin PATH] [--seed N] [--size short|long|extreme --night N] [--cell ENGINE/ID] [--windows N] [--measure] [--list]`);
  verdictLine(`RED (not run: ${why}; 0 simulated seconds and 0 windows judged)`);
  process.exit(2);
}
function parseArgs(argv) {
  const o = { bin: undefined, seed: SEED, size: 'short', night: null, cell: null, windows: null, measure: false, list: false };
  for (let k = 0; k < argv.length; k++) {
    const a = argv[k], next = () => { if (k + 1 >= argv.length) usage(`${a} needs a value`); return argv[++k]; };
    const whole = () => { const n = Number(next()); if (!Number.isInteger(n) || n < 0) usage(`${a} needs a whole number`); return n; };
    if (a === '--bin') o.bin = next();
    else if (a === '--seed') o.seed = whole() >>> 0;
    else if (a === '--size') { o.size = next(); if (!SIZES.includes(o.size)) usage(`--size is one of ${SIZES.join(', ')}`); }
    else if (a === '--night') o.night = whole();
    else if (a === '--cell') o.cell = next();
    else if (a === '--windows') o.windows = whole();
    else if (a === '--measure') o.measure = true;
    else if (a === '--list') o.list = true;
    else usage(`unknown argument ${a}`);
  }
  if (o.size !== 'short' && o.night === null && !o.cell) usage(`--size ${o.size} is a rotation: it needs --night N (any whole number; the cell is N modulo the rotation's length)`);
  if (o.windows !== null && !o.cell) usage('--windows needs --cell');
  return o;
}
function printThresholds(say) {
  say('THRESHOLDS  PROPOSED, not ratified (ADR-209 item 5):');
  for (const r of THRESHOLD_TABLE) say(`    ${r.name} = ${typeof r.value === 'number' ? Number(r.value.toPrecision(6)) : r.value}  (${r.what}; ${r.worst}; margin ${r.margin}; ${r.date})`);
}

export async function main(argv) {
  const o = parseArgs(argv), say = s => console.log(s), problems = [], T = THRESHOLDS;
  const ctx = { bin: binPath(o), T, renders: 0, probes: o.measure ? MEASURE_PROBES : 0, phrases: Object.fromEntries(Object.keys(PHRASES).map(d => [d, makePhrase(o.seed, d)])) };
  const engines = enrolled();
  if (!engines.length) throw new Error('h2_soak_check: no engine is enrolled; a soak of nothing must not read green');
  for (const e of engines) {
    if (!ENGINES[e]) throw new Error(`h2_soak_check: enrolled engine "${e}" has no entry in ENGINES (its cells)`);
    for (const size of SIZES) if (!ENGINES[e].cells.some(c => c.size === size)) throw new Error(`h2_soak_check: enrolled engine "${e}" has no ${size} cell`);
  }
  if (o.list) {
    for (const e of engines) for (const c of ENGINES[e].cells) { const cell = engineCell(e, c); say(`${c.size.padEnd(8)}${cellLine(cell, layoutOf(cell, T))}`); }
    return 0;
  }
  if (!existsSync(ctx.bin)) throw new Error(`h2_soak_check: ${ctx.bin} is not built (the CMake target h2_harness_render)`);
  const table = listAdapters(o).map(a => a.id);
  for (const id of [...engines, ...PLANTS.map(p => p.id)]) if (!table.includes(id)) throw new Error(`h2_soak_check: ${ctx.bin} has no adapter "${id}"; rebuild h2_harness_render`);

  // which cells: one named cell, or every enrolled engine's cells of the size (for a rotation, the night's one)
  let cells;
  if (o.cell) {
    const [e, id] = o.cell.split('/'), spec = ENGINES[e] && engines.includes(e) && ENGINES[e].cells.find(c => c.id === id);
    if (!spec) usage(`no cell "${o.cell}" (see --list)`);
    cells = [engineCell(e, spec, o.windows)];
  } else {
    cells = engines.flatMap(e => {
      const mine = ENGINES[e].cells.filter(c => c.size === o.size);
      return (o.size === 'short' ? mine : [mine[o.night % mine.length]]).map(c => engineCell(e, c));
    });
  }
  const what = o.cell ? `the cell ${o.cell}${o.windows !== null ? ` at ${o.windows} windows` : ''}` : o.size === 'short' ? 'the short local run' : `the nightly ${o.size} rotation, night ${o.night}: ${cells.map(c => c.id).join(', ')}`;
  say(`h2_soak_check: seed 0x${o.seed.toString(16).toUpperCase()} (${o.seed}), ${what}${o.measure ? ' -- A MEASUREMENT, not a gate run' : ''}`);
  for (const d of Object.keys(PHRASES)) say(`PHRASE ${phraseText(ctx.phrases[d])}`);

  if (o.measure) {
    // A MEASUREMENT, NOT A VERDICT: the numbers the thresholds table is built from.
    for (const cell of cells) {
      const r = await runCell(ctx, cell);
      say(`CELL ${cellLine(cell, r.lay)}`);
      if (r.crash) { say(`    CRASH ${r.findings[0].text}`); continue; }
      for (const l of factLines(r.facts, r.lay, T)) say(`    ${l}`);
      const w = r.facts.windows.slice(1), wr = r.facts.wraps.map(g => [0, 1].map(ch => wrapOf(r.facts, r.lay, g, ch))).filter(x => x[0]);
      CH.forEach((name, ch) => {
        const lv = w.map(x => x.level[ch]), q = Math.max(1, Math.floor(w.length / 4));
        say(`    MEASURED ${name}: level ${Math.min(...lv).toFixed(3)} to ${Math.max(...lv).toFixed(3)} dB; first quarter's median ${median(lv.slice(0, q)).toFixed(3)}, last quarter's ${median(lv.slice(-q)).toFixed(3)}` +
          (wr.length ? `; over ${wr.length} place(s), the wraps and the probes: largest click ${fmt(Math.max(...wr.map(x => x[ch].click)), 2)} dB, largest step ${fmt(wr.map(x => x[ch].step).reduce((a, b) => (Math.abs(b) > Math.abs(a) ? b : a), 0))} dB` : ''));
        // the shape of the run: a trend shows as a slope across the parts, a wander does not
        const parts = Math.min(12, w.length), part = k => w.slice(Math.floor(k * w.length / parts), Math.floor((k + 1) * w.length / parts));
        say(`    IN ${parts} PARTS ${name}: level medians ${Array.from({ length: parts }, (_, k) => median(part(k).map(x => x.level[ch])).toFixed(3)).join(' ')}`);
        say(`    IN ${parts} PARTS ${name}: mean, as a share of the early level ${Array.from({ length: parts }, (_, k) => (median(part(k).map(x => x.dc[ch])) / Math.sqrt(w[0].ms[ch])).toFixed(5)).join(' ')}`);
      });
      for (const f of r.findings) say(`    WOULD REPORT ${f.band} ${f.kind}${f.ch ? ' on ' + f.ch : ''}: ${f.text}`);
      say(`    ${reachLine(cell, r.lay, T)}`);
    }
    say('h2_soak_check: MEASURED (not a gate run; nothing was judged)');
    return 0;
  }

  // the streaming read, then every cell and every plant side by side
  for (const e of engines) {
    const s = await streamRow(ctx, e);
    for (const b of s.bad) problems.push(`STREAM ${e}: ${b}`);
    say(`STREAM ${e}  the streaming read against the whole-file read, 2 jobs, ${s.frames} frames in ${s.chunks} chunks: ${s.bad.length ? 'DIFFERS' : 'the same samples bit for bit, the same digests (' + s.digests.join(', ') + '), and the read side digests to the same'}`);
  }
  const tasks = [...cells, ...PLANTS.map(controlCell)];
  const results = await mapPool(tasks, MAX_PROCS, cell => runCell(ctx, cell));
  let seconds = 0, windows = 0, greys = 0;
  for (const r of results.slice(0, cells.length)) {
    say(`CELL ${cellLine(r.cell, r.lay)}`);
    if (!r.crash) for (const l of factLines(r.facts, r.lay, T)) say(`    ${l}`);
    if (r.summary && r.summary.digest) say(`    digest ${r.summary.digest}${r.lay.totalFrames <= DIGEST_UP_TO ? ' (and the samples read digest to the same)' : ' (not re-computed on the reading side: the stream is over 2^30 frames)'}`);
    for (const f of r.findings) {
      say(`    ${f.band === 'fail' ? 'RED ' : 'GREY'} ${f.kind}${f.ch ? ' on ' + f.ch : ''}: ${f.text}`);
      if (f.band === 'fail') problems.push(`${r.cell.id}: ${f.kind}${f.ch ? ' on ' + f.ch : ''}: ${f.text}`); else greys++;
    }
    if (!r.crash) { seconds += r.lay.windows * r.cell.periodS; windows += r.lay.windows; }
    say(reachLine(r.cell, r.lay, T));
  }
  // the controls: each plant must read exactly as it is built
  results.slice(cells.length).forEach((r, k) => {
    const plant = PLANTS[k], v = controlVerdict(plant, r), name = plant.label || plant.id;
    for (const b of v.bad) problems.push(`CONTROL ${name}: ${b}`);
    const found = r.findings.map(f => f.text).join('; ');
    say(`CONTROL ${name}  ${plant.what}. Must read as ${v.want.length ? v.want.map(expectText).join(' and ') : 'nothing at all'} over ${r.lay.windows * r.cell.periodS} s: ${v.bad.length ? 'WRONG -- ' + v.bad.join('; ') : 'read exactly so'}${found ? '. Read: ' + found : ''}`);
    if (name === 'control/soak-steady') {
      const judgedWraps = r.facts.wraps.filter(g => wrapOf(r.facts, r.lay, g, 0)).length;
      if (!judgedWraps) problems.push(`CONTROL ${plant.id}: no wrap position was judged, so "reads no wrap fault" was not exercised`);
      for (const l of factLines(r.facts, r.lay, T)) say(`    ${l}`);
    }
  });
  const cc = controlCell(PLANTS[0]), cl = layoutOf(cc, T), cr = reachOf(cl, cc, T);
  // how far over the line each drift plant is: a control found only because it is enormous proves little
  const dcRead = (results[cells.length + PLANTS.findIndex(p => p.id === 'control/soak-dc')].findings.find(f => f.kind === 'dc-drift' && f.ch === 'R') || {}).value;
  say(`CONTROL REACH  the control cell (${cr.spanS} s) sees a level drift from ${cr.levelFail.toPrecision(3)} dB an hour and a DC creep from ${cr.dcFail.toPrecision(3)} of the level an hour. Planted: a level drift of ${(20 * Math.log10(2) * 3600 / PLANT.levelDoubleS).toPrecision(3)} dB an hour, ${(20 * Math.log10(2) * 3600 / PLANT.levelDoubleS / cr.levelFail).toFixed(2)} times that reach; a DC creep read as ${dcRead === undefined ? 'nothing' : (dcRead / cr.spanS * 3600).toPrecision(3) + ' of the level an hour, ' + (dcRead / T.dcFail).toFixed(2) + ' times that reach'}; a wrap step of ${(20 * Math.log10(PLANT.wrapStep)).toFixed(3)} dB, ${(20 * Math.log10(PLANT.wrapStep) / T.stepFail).toFixed(2)} times its line.`);
  if (!windows) problems.push('no window was judged: a run of nothing is not a pass');

  printThresholds(say);
  for (const p of problems) console.error(`h2_soak_check: RED  ${p}`);
  if (o.size !== 'short' && !o.cell) say(`KIT-GATE cases=${windows}`);
  verdictLine(`${problems.length ? 'RED' : 'GREEN'} (${seconds} simulated seconds and ${windows} windows judged over ${engines.length} enrolled engine(s): ${engines.join(', ')}; ${cells.length} cell(s): ${cells.map(c => c.id).join(', ')}; ${problems.length} problem(s); ${greys} grey result(s) for the human's ears; ${ctx.renders} renders with the ${PLANTS.length} controls and the streaming row)`);
  return problems.length || !windows ? 1 : 0;
}

/* Run only as the entry script. The comparison is between two file URLs made the same
   way (tools/h2_fuzz_check.mjs says what goes wrong otherwise: a path with a space or a
   symlink, and then nothing ran, nothing printed and the exit code was 0). */
if (process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href) {
  main(process.argv.slice(2)).then(code => { process.exitCode = code; }, e => {
    console.error(e.message || e);
    verdictLine(`RED (stopped before a verdict: ${String(e.message || e).split('\n')[0]}; 0 simulated seconds and 0 windows judged)`);
    process.exitCode = 2;
  });
}
