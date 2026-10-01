/*
 * gen_goldens_sr.mjs — the swarm core's goldens at a SECOND sample rate (48 kHz), beside the 44.1 kHz
 * set gen_goldens.mjs writes. HYPERSAW, 2026-09-30, ROADMAP B382 (the human's ruling on B379: "a 48 kHz
 * golden set"), in the M1 PR (composed-m1-ksm).
 *
 * WHY. Every legacy L0-1 golden is 44.1 kHz, so the C++ core (src/swarm_core.h and its h2 copy) was
 * never compared with the JS reference at any other rate. B379 found two differences there by hand
 * (docs/port/phase-1b.md: M1, the coupling smoother's per-tick 0.08, and M2, bipolar onset) and put the
 * hypothesis "M1 and M2 are the only 48 kHz differences" on four scenarios. This set tests it on the
 * whole L0-1 matrix: swarm48_check / h2_swarm48_check replay every golden on the C++ at 48 kHz and
 * report EVERY scenario beyond 1e-6.
 *
 * WHAT IS WRITTEN (build-golden/sr48000/, build artifacts, never committed, like the 44.1 set):
 *   <name>.p.f32   PLAIN: SwarmSynth exactly as reference/swarmsaw.html has it (extract_core.mjs), or
 *                  DynSynth for the dyn scenarios.
 *   <name>.m.f32   MIRRORED: SwarmSynth with every divergence the composed engine registers switched ON
 *                  (docs/design/scalpel-horde-engine.js SWARM_PATCHES, docs/port/divergences.json): the
 *                  engine's own patched class, Composed.SwarmSynth, never a copy. So "M1 and M2
 *                  accounted for" is the lab's law itself, and whatever still differs is a difference
 *                  no divergence explains. DynSynth (the dyn scenarios) is run by no engine patch (the
 *                  composed engine uses none of its coupling), but its smoother carries the SAME
 *                  per-tick literal (swarmdynamics.html controlTick, `s.Ksm += (Ktarget - s.Ksm) *
 *                  0.08`); its mirrored file has that one literal replaced by M1's coefficient for the
 *                  rate, the engine's own (Mirrored.ksmC), and nothing else. Measured 2026-09-30: with
 *                  it, every dyn scenario is within 1e-6 of the C++; without it, all but dyn-grid (K 0)
 *                  miss by 4e-5 to 9e-2. So M1 in DynSynth is a fact about the C++ reference, recorded
 *                  here and in the M1 ledger entry, and not an engine divergence.
 *   manifest.tsv   name, engine, mirrored file, seed, params (application order), notes, plain file,
 *                  and what the C++ must show against the MIRRORED file:
 *                    match           within 1e-6;
 *                    differ:<why>    beyond 1e-6: a difference already known, named, and not (yet)
 *                                    mirrored. A scenario whose divergence the engine does not
 *                                    register is `differ:<id> not registered`; the moment it is
 *                                    registered (M2 and M3 are separate PRs) it becomes `match`,
 *                                    with no edit here, so this file is right in every merge order.
 *   ksm.tsv        sr, the mirrored SwarmSynth's per-tick coupling coefficient (M1), 17 digits (a
 *                  double round-trips exactly): the C++ compares its own at each rate, bit for bit.
 * The matrix is gen_goldens.mjs's, imported (SCENARIOS, DYN_SCENARIOS, the protocol and
 * renderScenario), plus EXTRA: the scenarios the 44.1 set cannot carry because they differ there by
 * design (negative onset, law 3), each naming the divergence it needs.
 *
 *   node tools/golden/gen_goldens_sr.mjs               write build-golden/sr48000/
 *   node tools/golden/gen_goldens_sr.mjs --selfcheck   render every scenario twice, byte-compare
 * Deterministic: seeded streams only, no clock. Both references are read, never edited.
 */
import { mkdirSync, writeFileSync, readFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createRequire } from 'node:module';
import { extractCore, BANNERS } from './extract_core.mjs';
import { SCENARIOS, DYN_SCENARIOS, coreToDyn, SEEDS, MIDI, renderScenario, sha256 } from './gen_goldens.mjs';

const here = dirname(fileURLToPath(import.meta.url));
const repo = join(here, '..', '..');
const require = createRequire(import.meta.url);
const SR = 48000;
const outDir = join(repo, 'build-golden', `sr${SR}`);

/* the composed engine's patched SwarmSynth and the divergences it registers */
const RazorCore = require(join(repo, 'reference/scalpel/prototype/razor-core.js'));
const { makeComposedEngine, swarmSourceFromHtml } = require(join(repo, 'docs/design/scalpel-horde-engine.js'));
const Composed = makeComposedEngine(RazorCore, swarmSourceFromHtml(readFileSync(join(repo, 'reference/swarmsaw.html'), 'utf8')));
const PATCHES = Composed.swarmPatches, has = id => PATCHES.some(x => x.id === id);
class Mirrored extends Composed.SwarmSynth {
  constructor(sr) { super(sr); for (const x of PATCHES) this.p[x.flag] = 1; }
}
const SwarmSynth = extractCore(join(repo, 'reference/swarmsaw.html'), 'SwarmSynth');
const DynSynth = extractCore(join(repo, 'reference/swarmdynamics.html'), 'DynSynth');
/* DynSynth with M1's coefficient in its smoother (header): its DSP section sliced by the reference
   banners (extract_core.mjs's rule), the one literal replaced exactly once, the rate's coefficient the
   engine's own */
const DYN_KSM = 's.Ksm += (Ktarget - s.Ksm) * 0.08;';
function dynMirrored(sr) {
  const html = readFileSync(join(repo, 'reference/swarmdynamics.html'), 'utf8');
  const src = html.slice(html.search(BANNERS.reference.start), html.search(BANNERS.reference.end));
  if (src.split(DYN_KSM).length !== 2) throw new Error(`gen_goldens_sr: DynSynth's smoother line is not exactly once in swarmdynamics.html: ${DYN_KSM}`);
  const k = new Mirrored(sr).ksmC;
  if (typeof k !== 'number') throw new Error('gen_goldens_sr: the engine registers no M1 coefficient (ksmC)');
  return new Function('KSM', '"use strict";\n' + src.replace(DYN_KSM, 's.Ksm += (Ktarget - s.Ksm) * KSM;') + '\nreturn DynSynth;')(k);
}
const DynMirrored = dynMirrored(SR);

/* the scenarios the 44.1 kHz set cannot hold: they differ from SwarmSynth at every rate, by a
   divergence the C++ has (ADR-056's bipolar onset, M2; ADR-022's tempo grid, law 3, M3) */
const EXTRA = [
  { name: 'onset-splay',      p: { K: 0.35, onset: -0.6, dissolve: 0.5 }, needs: 'M2' },
  { name: 'onset-splay-k0',   p: { K: 0, onset: -1, dissolve: 1.0, detune: 0.4 }, needs: 'M2' },
  { name: 'onset-sync-half',  p: { K: 0.35, onset: 0.5, dissolve: 0.5 } },            // phase-1b's table: rms 9.7e-4 at 48 kHz before M1
  { name: 'law3-grid',        p: { law: 3, bpm: 120, beatMult: 1, detune: 0.6, n: 9, K: 0 }, needs: 'M3' },
  { name: 'law3-grid-sync',   p: { law: 3, bpm: 140, beatMult: 2, detune: 0.3, n: 7, K: 0.4 }, needs: 'M3' },
];
/* what the C++ must show against the mirrored golden (header) */
const expectOf = sc => (sc.needs && !has(sc.needs) ? `differ:${sc.needs} not registered` : 'match');

const ALL = [
  ...SCENARIOS.map(sc => ({ ...sc, engine: 'saw' })),
  ...EXTRA.map(sc => ({ ...sc, engine: 'saw' })),
  ...DYN_SCENARIOS.map(sc => ({ ...sc, engine: 'dyn' })),
];
const selfcheck = process.argv.includes('--selfcheck');
if (!selfcheck) mkdirSync(outDir, { recursive: true });
let failures = 0;
const lines = [];
for (const sc of ALL) {
  const dyn = sc.engine === 'dyn', translate = dyn ? coreToDyn : null, notes = sc.notes || [MIDI];
  for (const seed of SEEDS) {
    const name = `${sc.name}.seed${seed}`;
    const Plain = dyn ? DynSynth : SwarmSynth, Mirror = dyn ? DynMirrored : Mirrored;
    const plain = renderScenario(Plain, seed, sc.p, notes, translate, SR);
    const mirrored = renderScenario(Mirror, seed, sc.p, notes, translate, SR);
    if (selfcheck) {
      const again = [renderScenario(Plain, seed, sc.p, notes, translate, SR), renderScenario(Mirror, seed, sc.p, notes, translate, SR)];
      const ok = sha256(plain) === sha256(again[0]) && sha256(mirrored) === sha256(again[1]);
      if (!ok) failures++;
      console.log(`${ok ? 'OK ' : 'DRIFT'} ${name} ${sha256(mirrored).slice(0, 12)}`);
      continue;
    }
    const pf = `${name}.p.f32`, mf = `${name}.m.f32`;
    writeFileSync(join(outDir, pf), Buffer.from(plain.buffer));
    writeFileSync(join(outDir, mf), Buffer.from(mirrored.buffer));
    const params = Object.entries({ seed, ...sc.p }).map(([k, v]) => `${k}=${v}`).join(',');
    lines.push([name, sc.engine, mf, seed, params, notes.join('+'), pf, expectOf(sc)].join('\t'));
  }
}
if (selfcheck) {
  if (failures) { console.error(`gen_goldens_sr: ${failures} scenario(s) NON-DETERMINISTIC`); process.exit(1); }
  console.log(`gen_goldens_sr: ${ALL.length * SEEDS.length} scenarios deterministic at ${SR} Hz (${PATCHES.map(x => x.id).join(', ') || 'no'} divergence(s) mirrored)`);
} else {
  writeFileSync(join(outDir, 'manifest.tsv'), lines.join('\n') + '\n');
  /* M1's coefficient per rate (the engine's patched constructor's ksmC; dynMirrored has proven it exists) */
  const rates = [22050, 32000, 44100, 48000, 88200, 96000, 192000];
  writeFileSync(join(outDir, 'ksm.tsv'), rates.map(r => `${r}\t${new Mirrored(r).ksmC.toPrecision(17)}`).join('\n') + '\n');
  console.log(`gen_goldens_sr: ${lines.length} goldens at ${SR} Hz -> build-golden/sr${SR}/ (mirrored: ${PATCHES.map(x => x.id).join(', ') || 'none'})`);
}
