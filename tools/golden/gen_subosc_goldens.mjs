/*
 * gen_subosc_goldens.mjs — SUB OSC parity goldens for tools/subosc_check.cpp.
 *
 * Slices `SubOscCore` live out of reference/subosc.html with extract_core.mjs
 * (ADR-003: the prototype IS the spec; a copied-out core silently diverges the
 * first time the reference moves — LIBRARY L0001), renders a scenario set at
 * BOTH 48 000 and 44 100 Hz, and writes raw interleaved f32 stereo plus a
 * manifest into build-golden/subosc/.
 *
 * ── NOTHING IS INVERTED HERE, AND THAT IS WORTH SAYING ──────────────────────
 * STATION's generator has to undo a monitoring `tanh` before it can compare
 * (gen_station_goldens.mjs's header). SUB OSC's lab core has no monitoring
 * stage at all — §2's chain ends at the tone filter and the lab's gain lives in
 * the audio graph, below the extractor's banner — so the golden is the core's
 * own output at full amplitude with the whole eps = 1e-6 budget intact. The one
 * rounding on the path is the f32 golden file itself (6e-8 relative, ~6 % of
 * the budget), and the C++ core stores f32 too, so both sides round once.
 *
 * ── THE MASTER PHASE IS AN INPUT, SO IT IS DUMPED, NOT DERIVED ──────────────
 * §2: "the master phase is an INPUT" — in the device the voice hands the sub
 * oscillator 1's phase; the lab fakes it with a saw at a lab-only ratio that
 * does not survive the port. So the sync scenarios STATE their source: a saw at
 * an absolute frequency written into the manifest to 17 digits, accumulated by
 * the identical recurrence on both sides and stored f32 (the type the shell's
 * phase buffer will be). Dumping the Hz rather than a ratio keeps `Math.pow`
 * out of the shared path: a 1-ulp disagreement there would move a sync reset
 * by a whole sample, which is a discontinuity, not a rounding error.
 *
 * ── THE MANIFEST IS THE SINGLE SOURCE OF TRUTH ──────────────────────────────
 * Each row dumps the WHOLE resulting parameter table as flat key=value tokens
 * (the lab's own `core.p`, in the lab's own order), not a scenario name the C++
 * mirrors. A mirrored scenario table in two languages drifts; a dumped state
 * cannot — and because SUB OSC's `setParam` throws on an unknown key (§7), a
 * parameter the lab grows and this port lacks is a loud failure on the next
 * run rather than a silent parity hole.
 *
 * Usage: node gen_subosc_goldens.mjs [--selfcheck]
 */

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { extractCore } from './extract_core.mjs';

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const LAB = join(root, 'reference/subosc.html');
const SubOscCore = extractCore(LAB, 'SubOscCore');

const BLOCK = 128;
const RATES = [48000, 44100];
const mtof = m => 440 * Math.pow(2, (m - 69) / 12);

// ----------------------------------------------------------------- scenarios --
// Every row is a named departure from §7's defaults (wave = saw, octave = -1,
// level 0.8, tone wide open, sync off, seed 1), so the `default` row certifies
// the table itself and each other row certifies exactly what it names.
// `p` holds §7 ADDRESS KEYS: they go through the lab's setParam, which clamps
// and throws, so a typo here fails at generation rather than silently.
export const SCENARIOS = [
  // §7's default patch on a low note — the one row that certifies the defaults.
  { name: 'default', note: 36, secs: 0.5, p: {} },

  // §3's seven shapes, all on the same low note so the rows are comparable.
  { name: 'sine', note: 36, secs: 0.5, p: { wave: 0 } },
  { name: 'tri', note: 36, secs: 0.5, p: { wave: 1 } },     // NAIVE (limit L1)
  { name: 'square', note: 36, secs: 0.5, p: { wave: 2 } },
  { name: 'saw', note: 36, secs: 0.5, p: { wave: 3 } },
  { name: 'pulse', note: 36, secs: 0.5, p: { wave: 4, width: 0.27 } },
  { name: 'noise', note: 36, secs: 0.5, p: { wave: 5 } },
  { name: 'bump', note: 36, secs: 0.5, p: { wave: 6 } },

  // A HIGH note on the polyBLEP shapes: the correction is largest where dph is
  // largest, so this is where a mis-ported BLEP shows up first.
  { name: 'pulse-hi', note: 84, secs: 0.5, p: { wave: 4, width: 0.05, octave: 0 } },

  // BUMP at three (a, phi) pairs including §7's defaults: the peak search
  // (R7) runs per parameter set, so each pair exercises a different bracket.
  { name: 'bump-def', note: 24, secs: 0.5, p: { wave: 6, bumpAmt: 0.35, bumpPhase: -0.25 } },
  { name: 'bump-max', note: 24, secs: 0.5, p: { wave: 6, bumpAmt: 0.6, bumpPhase: 1.7 } },
  { name: 'bump-lo', note: 24, secs: 0.5, p: { wave: 6, bumpAmt: 0.12, bumpPhase: -3.0 } },

  // §4's pitch law: each offset on its own, then all three at once.
  { name: 'oct1', note: 48, secs: 0.5, p: { octave: -1 } },
  { name: 'oct2', note: 48, secs: 0.5, p: { octave: -2 } },
  { name: 'semis', note: 48, secs: 0.5, p: { semis: -7 } },
  { name: 'fine', note: 48, secs: 0.5, p: { fine: 37.5 } },
  { name: 'pitch-all', note: 60, secs: 0.5, p: { octave: -2, semis: 11, fine: -100 } },
  // Keytrack OFF pins to C2 (§4) — the note is deliberately three octaves away
  // from C2 so a port that quietly kept keytracking is not merely a little off.
  { name: 'keytrack-off', note: 84, secs: 0.5, p: { keytrack: 0 } },

  // §5.1's tone at two settings, on a shape with something to filter.
  { name: 'tone-200', note: 36, secs: 0.5, p: { wave: 3, tone: 200 } },
  { name: 'tone-2k', note: 36, secs: 0.5, p: { wave: 4, width: 0.3, tone: 2000 } },

  // §6 hard sync, against a STATED source: a saw at 41 Hz (the note here is
  // MIDI 36 at octave -1 = 32.70 Hz, so the master wraps faster than the
  // oscillator and the reset lands at irregular phases).
  { name: 'sync-pulse', note: 36, secs: 0.5, master: 41, p: { wave: 4, width: 0.4, sync: 1, phase: 0.3 } },
  // Sync ON but with the start phase at 0 and a master BELOW the oscillator, so
  // the reset is rare — the other half of §6's convention.
  { name: 'sync-slow', note: 48, secs: 0.5, master: 7.5, p: { wave: 3, sync: 1 } },
  // The must-not-fire case, and it is a parity row rather than a comment: the
  // same master with sync OFF must reproduce the unsynced shape exactly.
  { name: 'sync-off', note: 36, secs: 0.5, master: 41, p: { wave: 4, width: 0.4, sync: 0, phase: 0.3 } },

  // §5.3's PROVISIONAL envelope at non-default A/R, with the note-off inside
  // the render so the release ramp and the filter tail are both in the golden.
  { name: 'env-ar', note: 36, secs: 1.2, offAt: 0.45, p: { attack: 0.2, release: 0.6 } },
  // A short attack against a long release on the noise shape: the envelope and
  // the RNG stream advance together, so a mis-ordered pair shows up here.
  { name: 'env-noise', note: 36, secs: 0.9, offAt: 0.2, p: { wave: 5, attack: 0.0005, release: 0.5, tone: 900 } },

  // §5.2 level and velocity: both are plain multipliers, and a port that
  // applied velocity after the filter instead of before would still sound right
  // and would fail here.
  { name: 'level-vel', note: 36, secs: 0.5, vel: 0.4, p: { level: 0.33, tone: 600 } },

  // §10.2's seeded noise, TWICE with different seeds. The bit-identity of two
  // instances at the SAME seed is subosc_check's determinism row; what these
  // two gate is that the stream is a function of the seed at all, and that the
  // port's mulberry32 is the lab's to the last bit over 24 000 draws.
  { name: 'noise-s1', note: 36, secs: 0.5, p: { wave: 5, seed: 1 } },
  { name: 'noise-s2', note: 36, secs: 0.5, p: { wave: 5, seed: 3735928559 } },
];

// -------------------------------------------------------------------- render --
// The master phase, when a scenario states one: a saw at an absolute frequency,
// accumulated in double and STORED f32 — the same two operations the C++ side
// performs, in the same order, so the two arrays are bit-identical and a sync
// reset lands on the same sample in both.
function masterPhase(hz, sr, n) {
  const a = new Float32Array(n);
  const d = hz / sr;
  let ph = 0;
  for (let i = 0; i < n; i++) { ph += d; ph -= Math.floor(ph); a[i] = ph; }
  return a;
}

function build(sc, sr) {
  const c = new SubOscCore(sr);
  for (const k in sc.p) c.setParam(k, sc.p[k]);
  c.noteOn(sc.note, sc.vel === undefined ? 1 : sc.vel);
  return c;
}

function renderScenario(sc, sr) {
  const c = build(sc, sr);
  const total = Math.round(sr * sc.secs);
  const offSample = sc.offAt === undefined ? -1 : Math.round(sr * sc.offAt);
  const master = sc.master === undefined ? null : masterPhase(sc.master, sr, total);
  const out = new Float32Array(total * 2);
  const L = new Float32Array(BLOCK), R = new Float32Array(BLOCK);
  for (let off = 0; off < total;) {
    let k = Math.min(BLOCK, total - off);
    if (offSample > off && offSample < off + k) k = offSample - off;  // split exactly on the note-off
    if (off === offSample) c.noteOff();
    c.render(L, R, k, master === null ? undefined : master.subarray(off, off + k));
    for (let i = 0; i < k; i++) { out[(off + i) * 2] = L[i]; out[(off + i) * 2 + 1] = R[i]; }
    off += k;
  }
  return out;
}

// The WHOLE resulting parameter table, flat, in the lab's own key order.
const num = v => Number(v).toPrecision(17);
const dumpParams = c => Object.keys(c.p).map(k => `${k}=${num(c.p[k])}`);

// --------------------------------------------------------------------- main --
const outDir = join(root, 'build-golden', 'subosc');
const selfcheck = process.argv.includes('--selfcheck');
if (!selfcheck) mkdirSync(outDir, { recursive: true });

let fail = 0;
const manifest = [];
for (const sr of RATES)
  for (const sc of SCENARIOS) {
    const name = `${sc.name}-${sr / 100}`;
    const audio = renderScenario(sc, sr);
    if (selfcheck) {
      const again = renderScenario(sc, sr);
      let same = again.length === audio.length;
      if (same) for (let i = 0; i < audio.length; i++) if (audio[i] !== again[i]) { same = false; break; }
      console.log(`${same ? 'OK  ' : 'FAIL'} ${name}`);
      if (!same) fail++;
      continue;
    }
    writeFileSync(join(outDir, `${name}.f32`), Buffer.from(audio.buffer));
    const meta = [`@sr=${sr}`, `@secs=${num(sc.secs)}`, `@block=${BLOCK}`,
                  `@note=${sc.note}:${num(sc.vel === undefined ? 1 : sc.vel)}`];
    if (sc.master !== undefined) meta.push(`@master=${num(sc.master)}`);
    if (sc.offAt !== undefined) meta.push(`@off=${Math.round(sr * sc.offAt)}`);
    manifest.push([name, [...meta, ...dumpParams(build(sc, sr))].join(' ')].join('\t'));
    console.log(`wrote ${name}.f32`);
  }

if (selfcheck) {
  console.log(fail ? `selfcheck: ${fail} NON-DETERMINISTIC` : 'selfcheck: renders are deterministic');
  process.exit(fail ? 1 : 0);
}
const labHash = createHash('sha256').update(readFileSync(LAB)).digest('hex').slice(0, 16);
writeFileSync(join(outDir, 'subosc-manifest.tsv'),
  `#lab\treference/subosc.html\tsha256:${labHash}\n` + manifest.join('\n') + '\n');
console.log(`lab sha256:${labHash}  ${manifest.length} goldens`);
