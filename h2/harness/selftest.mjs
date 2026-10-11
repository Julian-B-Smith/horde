/*
 * selftest.mjs — the behavioural half of enrolment: every enrolled adapter really
 * drives its engine, and the harness's own readings can be trusted. ROADMAP B448
 * phase 2, P2 (ADR-209). The static half is tools/engine_enrolment_check.py.
 *
 * NOT RUN BY ./verify YET. It needs the built render tool, and this package could
 * add neither a CMake target nor a compile line of its own (h2/harness/README.md,
 * "Wiring the selftest", says why and gives both ways to wire it). Until then it is
 * run by hand:
 *   node h2/harness/selftest.mjs --bin <path to h2_harness_render>
 *
 * It prints one PASS or FAIL line per row and exits 1 on any FAIL. The rows:
 *   LIST     the built binary's engine rows are exactly the registry's enrolled ids.
 *   FRESH    patches.json is what gen_patches.mjs generates from its sources today.
 *   patch    each reference patch of each enrolled engine, one note of the cell held
 *            for kSeconds: no unknown key, no non-finite sample, the pre-limiter bus
 *            maps back onto the output, and the patch's `expect` holds: `loud` is
 *            metrics.mjs's own silence rule read false AND at least LOUD_FLOOR_DB;
 *            `silent` is every sample exactly zero.
 *   NO-NOTE  MUST-READ-ZERO on the real engine: each loud patch with no note
 *            played renders exact zeros. Without it "loud" could be something the
 *            probe makes rather than the notes.
 *   CONTROLS (the judge must be able to say no):
 *     silent-adapter   the same loud job through control/silent, an adapter that
 *                      returns silence, must FAIL the must-read-loud judge. This is
 *                      the package's named must-fail control.
 *     loud-as-silent   a loud render must fail the must-read-silent judge.
 *     tone             control/tone has a level known in closed form: left reads
 *                      20 log10(0.5 / sqrt 2) dBFS, right is exactly half the left
 *                      sample by sample (a swapped or misaligned channel shows),
 *                      and after the release it is exact zeros.
 *     blocks           two rows, because either alone proves nothing. control/tone
 *                      is block-independent by construction, so its digest must be
 *                      the same at host blocks 1, 7, 128 and varying (a note lands
 *                      on its frame at any block size). control/block writes each
 *                      render call's length into its frames, so its output must
 *                      CHANGE with the block size, and must show exactly the cuts
 *                      the job language promises: without it the first row would
 *                      pass with the block size ignored. The engines' own
 *                      invariance is package P4's to judge, not this file's.
 *     digest           the same job twice gives the same digest, and different
 *                      patches give different ones (a constant digest shows).
 *     pre-limiter      the composed adapter's recovery read back at known values:
 *                      0 exactly, 3, 6 and 10 within a stated error, and the rail as
 *                      infinity; the tone control, which has no limiter, unchanged.
 *     refusal          a job for an adapter that does not exist, and a job with a
 *                      command the language does not have, stop the run. A job that
 *                      is quietly not run would read as a job that passed.
 *
 * THE NUMBERS BELOW ARE THIS FILE'S THRESHOLDS (they are outside
 * docs/armor/tolerances.json, whose scan does not read .mjs files yet):
 *   LOUD_FLOOR_DB  -60 dBFS RMS. Measured 2026-10-10 on darwin-arm64: the quietest
 *                  loud patch (ledger/breathing-pad, a 1.5 s attack) read -28.0 dBFS
 *                  in this file's window, so the floor sits 32 dB under the quietest
 *                  patch and 30 dB over the -90 dBFS line metrics.mjs calls silence.
 *   TONE_TOL_DB    0.001 dB: the window holds a whole number of the tone's cycles.
 *   PRE_ROUNDTRIP  the measured errors (1.3e-15, 6e-13, 4.5e-9) with three decades
 *                  of room, because another platform's libm may round differently.
 *   PRE_MAP_TOL    1e-12: tanh(atanh(y)) differs from y by a few rounding steps.
 * No clock, no randomness: the same binary gives the same rows.
 */
import { readFileSync, realpathSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
import { nonFinite, peak, rms, silence } from '../../tools/patchspace/metrics.mjs';
import { build as buildPatches, OUT as PATCHES_OUT } from './gen_patches.mjs';
import { ROOT, binPath, enrolled, jobFor, listAdapters, loadPatches, mtof, patchesFor, run } from './harness.mjs';

const LOUD_FLOOR_DB = -60, TONE_TOL_DB = 0.001, PRE_MAP_TOL = 1e-12;
const PRE_ROUNDTRIP = [[3, 1e-12], [6, 1e-10], [10, 1e-6]];
const SR = 48000, kSeconds = 0.3, kWindowFrom = 0.15;   // the second half: past the note's first transient
const TONE_NOTE = 69, TONE_HOLD = 0.25;                  // 440 Hz for 0.25 s: exactly 110 cycles

const dbOf = x => 20 * Math.log10(x);
const cut = (r, a, z) => ({ L: r.L.subarray(Math.round(a * r.sr), Math.round(z * r.sr)), R: r.R.subarray(Math.round(a * r.sr), Math.round(z * r.sr)) });

/* The two judges. Every verdict in this file, real or control, goes through them. */
export function readsLoud(r) {
  const w = cut(r, kWindowFrom, kSeconds), level = dbOf(Math.max(rms(w.L), rms(w.R)));
  if (nonFinite(r.L, r.R)) return { ok: false, why: `${nonFinite(r.L, r.R)} non-finite samples` };
  if (silence(w.L, w.R).silent) return { ok: false, why: `silent by metrics.mjs's rule (${level.toFixed(1)} dBFS RMS)` };
  if (!(level >= LOUD_FLOOR_DB)) return { ok: false, why: `${level.toFixed(1)} dBFS RMS is under the ${LOUD_FLOOR_DB} dBFS floor` };
  return { ok: true, why: `${level.toFixed(1)} dBFS RMS`, level };
}
export function readsSilent(r) {
  const p = Math.max(peak(r.L), peak(r.R));
  if (nonFinite(r.L, r.R)) return { ok: false, why: `${nonFinite(r.L, r.R)} non-finite samples` };
  return p === 0 ? { ok: true, why: 'every sample is exactly 0' } : { ok: false, why: `peak ${p.toExponential(2)}, want exact zeros` };
}

function main(argv) {
  const opt = { bin: argv.includes('--bin') ? argv[argv.indexOf('--bin') + 1] : undefined };
  let rows = 0, failed = 0;
  const row = (ok, name, text) => { rows++; if (!ok) failed++; console.log(`${ok ? 'PASS' : 'FAIL'}  ${name}  ${text}`); };

  // ---- the table and the patch file
  const table = listAdapters(opt), ids = enrolled();
  const built = table.filter(a => !a.control).map(a => a.id).sort();
  row(JSON.stringify(built) === JSON.stringify([...ids].sort()) && ['control/silent', 'control/tone', 'control/block'].every(c => table.some(a => a.id === c && a.control)),
    'LIST', `the binary's engine rows [${built.join(', ')}] against the registry's enrolled ids [${ids.join(', ')}], with the three controls present`);
  row(buildPatches() === readFileSync(join(ROOT, PATCHES_OUT), 'utf8'), 'FRESH', `${PATCHES_OUT} is what gen_patches.mjs generates today`);

  // ---- every reference patch of every enrolled engine, in ONE process
  const cell = loadPatches().cell, note = [cell.notes[0]];
  const jobs = [], plan = [];
  const add = (kind, p, req) => { plan.push({ kind, p }); jobs.push(jobFor(Object.assign({ patch: p, sr: SR, seed: 1, seconds: kSeconds, notes: note }, req))); };
  for (const e of ids) {
    const mine = patchesFor(e), loud = mine.filter(p => p.expect === 'loud');
    for (const p of mine) add('patch', p, { pre: true });
    for (const p of loud) add('no-note', p, { notes: [], seconds: 0.05 });
    if (loud.length) {
      add('silent-adapter', loud[0], { engine: 'control/silent' });
      add('repeat', loud[0], { pre: true });   // the first patch job again: its digest must not move
    }
  }
  const out = run(jobs, opt);
  const digests = new Map(), firstLoud = new Map();
  out.forEach((r, i) => {
    const { kind, p } = plan[i];
    if (kind === 'patch') {
      const v = p.expect === 'loud' ? readsLoud(r) : readsSilent(r);
      let worst = 0, lost = 0, unread = 0;
      for (const [y, pre] of [[r.L, r.preL], [r.R, r.preR]]) for (let k = 0; k < y.length; k++) {
        if (Math.abs(y[k]) >= 1) { lost++; continue; }   // on the rail: the input is not recoverable, by contract
        const d = Math.abs(Math.tanh(pre[k]) - y[k]);
        if (Number.isNaN(d)) unread++;                     // counted apart: a NaN compares false and would hide in a max
        else if (d > worst) worst = d;
      }
      const keys = r.summary.unknownKeys === 0, map = worst <= PRE_MAP_TOL && unread === 0;
      row(v.ok && keys && map, `${p.engine} ${p.id}`, `expect ${p.expect}: ${v.why}; ${keys ? 'no unknown key' : `${r.summary.unknownKeys} unknown keys, first "${r.summary.firstUnknown}"`}; ` +
        `pre-limiter maps back within ${worst.toExponential(1)}${unread ? `, ${unread} samples NOT readable` : ''}${lost ? ` (${lost} samples on the rail)` : ''}`);
      if (p.expect === 'loud') { digests.set(p.id, r.summary.digest); if (!firstLoud.has(p.engine)) firstLoud.set(p.engine, r); }
    } else if (kind === 'no-note') {
      const v = readsSilent(r);
      row(v.ok, `${p.engine} NO-NOTE ${p.id}`, `must read zero with no note played: ${v.why}`);
    } else if (kind === 'silent-adapter') {
      const v = readsLoud(r), back = readsLoud(firstLoud.get(p.engine)), wrong = readsSilent(firstLoud.get(p.engine));
      row(!v.ok && back.ok, `CONTROL silent-adapter (${p.engine} ${p.id})`,
        `an adapter that returns silence must FAIL must-read-loud: ${v.ok ? 'it PASSED' : 'failed, ' + v.why}; the real adapter on the same job: ${back.why}`);
      row(!wrong.ok, `CONTROL loud-as-silent (${p.engine} ${p.id})`, `a loud render must FAIL must-read-silent: ${wrong.ok ? 'it PASSED' : 'failed, ' + wrong.why}`);
    } else {
      const same = r.summary.digest === digests.get(p.id), distinct = new Set(digests.values()).size === digests.size;
      row(same && distinct, `CONTROL digest (${p.engine})`, `the same job twice: ${same ? 'one digest' : 'TWO digests'} (${r.summary.digest}); ` +
        `${digests.size} loud patches: ${distinct ? 'all digests differ' : 'two share a digest'}`);
    }
  });

  // ---- the tone control: a known level, aligned channels, exact silence after the release
  const toneCmds = [['on', TONE_NOTE, mtof(TONE_NOTE), 1], ['render', Math.round(TONE_HOLD * SR)], ['off', TONE_NOTE], ['render', Math.round(0.05 * SR)]];
  const toneJob = block => jobFor({ engine: 'control/tone', name: `tone block=${JSON.stringify(block)}`, sr: SR, seed: 1, block, cmds: toneCmds });
  const tones = run([toneJob(128), toneJob(1), toneJob(7), toneJob({ vary: 64 })], opt), t = tones[0];
  const held = cut(t, 0, TONE_HOLD), tail = cut(t, TONE_HOLD, TONE_HOLD + 0.05);
  const want = dbOf(0.5 / Math.SQRT2), got = dbOf(rms(held.L));
  let aligned = true;
  for (let k = 0; k < t.L.length; k++) if (t.R[k] !== t.L[k] / 2) aligned = false;
  const tailZero = Math.max(peak(tail.L), peak(tail.R)) === 0;
  row(Math.abs(got - want) <= TONE_TOL_DB && aligned && tailZero, 'CONTROL tone',
    `left reads ${got.toFixed(4)} dBFS RMS, closed form ${want.toFixed(4)}; right is ${aligned ? 'exactly' : 'NOT'} half the left on every frame; after the release: ${tailZero ? 'exact zeros' : 'NOT zero'}`);
  const blockDigests = tones.map(r => r.summary.digest);
  row(new Set(blockDigests).size === 1, 'CONTROL blocks (independent)', `control/tone at host blocks 128, 1, 7 and varying 1..64: ${new Set(blockDigests).size === 1 ? 'one digest' : 'digests differ: ' + blockDigests.join(' ')}`);
  // ...and the block size must really reach the adapter, or the row above (and any
  // block-invariance result built on the harness) would pass with blocks ignored.
  // control/block writes each call's length (left) and index (right) into its frames.
  const blockJob = block => jobFor({ engine: 'control/block', name: `block ${JSON.stringify(block)}`, sr: SR, seed: 1, block, cmds: [['render', 60], ['render', 40]] });
  const [b128, b7, bVary] = run([blockJob(128), blockJob(7), blockJob({ vary: 16 })], opt);
  // two renders of 60 and 40 frames: each is cut on its own, and its last block shortened to fit
  const want7 = [...Array(56).fill(7), ...Array(4).fill(4), ...Array(35).fill(7), ...Array(5).fill(5)];
  const want128 = [...Array(60).fill(60), ...Array(40).fill(40)];
  const eq = (a, w) => a.length === w.length && w.every((x, k) => a[k] === x);
  const sizes = new Set(bVary.L);
  let runsOk = true;   // every call's run of frames is as long as the length it wrote
  for (let k = 0; k < bVary.L.length;) { const n = bVary.L[k]; for (let q = 0; q < n; q++) if (bVary.L[k + q] !== n || bVary.R[k + q] !== bVary.R[k]) runsOk = false; k += n; }
  const inRange = [...sizes].every(n => n >= 1 && n <= 16);
  row(eq(b7.L, want7) && eq(b128.L, want128) && b7.summary.digest !== b128.summary.digest && sizes.size > 1 && inRange && runsOk, 'CONTROL blocks (dependent)',
    `control/block must CHANGE with the block size: block 7 cut 60+40 frames as ${eq(b7.L, want7) ? '8x7,4 then 5x7,5' : 'SOMETHING ELSE'}; block 128 as ${eq(b128.L, want128) ? '60 then 40' : 'SOMETHING ELSE'}; ` +
    `digests ${b7.summary.digest !== b128.summary.digest ? 'differ' : 'are THE SAME'}; varying 1..16 drew ${sizes.size} sizes, ${inRange ? 'all in range' : 'OUT OF RANGE'}, runs ${runsOk ? 'consistent' : 'INCONSISTENT'}`);

  // ---- the pre-limiter recovery at known values
  const probe = (id, xs) => {
    const r = spawnSync(binPath(opt), ['--pre-roundtrip', id, ...xs.map(String)], { encoding: 'utf8' });
    // C spells the rail "inf" / "-inf" (and "nan"), which Number() does not read
    const num = s => (/^-?inf/i.test(s) ? (s[0] === '-' ? -Infinity : Infinity) : Number(s));
    return r.status === 0 ? r.stdout.trim().split('\n').map(l => l.split(' ').map(num)) : null;
  };
  for (const id of ids.filter(e => e === 'composed')) {
    const p = probe(id, [0, ...PRE_ROUNDTRIP.map(a => a[0]), 25, -25]);
    const errs = p ? PRE_ROUNDTRIP.map(([x, tol], k) => ({ x, e: Math.abs(p[k + 1][2] - x), tol })) : [];
    const ok = !!p && p[0][2] === 0 && errs.every(a => a.e <= a.tol) && p[p.length - 2][2] === Infinity && p[p.length - 1][2] === -Infinity;
    row(ok, `CONTROL pre-limiter (${id})`, p ? `0 reads ${p[0][2]}; ${errs.map(a => `${a.x} within ${a.e.toExponential(1)} (allowed ${a.tol})`).join(', ')}; +-25, on the rail, read ${p[p.length - 2][2]} and ${p[p.length - 1][2]}` : 'the probe did not run');
  }
  const pt = probe('control/tone', [3]);
  row(!!pt && pt[0][2] === pt[0][1], 'CONTROL pre-limiter (control/tone)', pt ? `no limiter: ${pt[0][1]} comes back as ${pt[0][2]}` : 'the probe did not run');

  // ---- a job that cannot run must stop the run
  const refuses = job => { try { run([job], opt); return false; } catch { return true; } };
  const ghost = refuses(jobFor({ engine: 'no-such-engine', name: 'ghost', cmds: [['render', 16]] }));
  const typo = refuses(jobFor({ engine: 'control/tone', name: 'typo', cmds: [['rendr', 16]] }));
  row(ghost && typo, 'CONTROL refusal', `a job for an unknown adapter ${ghost ? 'stops the run' : 'RAN'}; a job with an unknown command ${typo ? 'stops the run' : 'RAN'}`);

  console.log(`h2 harness selftest: ${failed ? 'FAILED' : 'GREEN'} (${rows} rows, ${failed} failed; ${ids.length} enrolled engine(s): ${ids.join(', ')})`);
  return failed ? 1 : 0;
}

// Run only as the entry script. Two file URLs made the same way are compared: a hand-built
// `file://${argv[1]}` differs from import.meta.url whenever the path holds a space or is
// reached through a symlink, and then this file ran nothing and exited 0.
if (process.argv[1] && import.meta.url === pathToFileURL(realpathSync(process.argv[1])).href) process.exitCode = main(process.argv.slice(2));
