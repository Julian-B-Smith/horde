/*
 * gen_goldens.mjs — L0-1 golden generator (SAW core, Phase 1).
 *
 * Renders the parity scenario matrix on the JS reference (SwarmSynth,
 * extracted live from reference/swarmsaw.html — see extract_core.mjs) and writes
 * Float32LE stereo-interleaved renders + a manifest to build-golden/.
 * Binaries are build artifacts, never committed (ROADMAP Phase 1); this
 * generator is the checked-in source of truth for what a golden IS.
 *
 * Modes:
 *   node gen_goldens.mjs              — write goldens + manifest
 *   node gen_goldens.mjs --selfcheck  — render every scenario twice and
 *                                       byte-compare (determinism gate for
 *                                       ./verify full; exits nonzero on drift)
 *
 * Scenario matrix: {3 seeds} x {param vectors covering each SAW subsystem}
 * per ACCEPTANCE L0-1. A3 = 220 Hz (midi 57), 4 s renders, 44.1 kHz,
 * note-off at 3 s so release tails are covered.
 */

import { mkdirSync, writeFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import { extractCore } from './extract_core.mjs';

const here = dirname(fileURLToPath(import.meta.url));
const repo = join(here, '..', '..');
const outDir = join(repo, 'build-golden');

const SR = 44100;
const SECONDS = 4;
const NOTE_OFF_AT = 3;
const BLOCK = 1024;
const MIDI = 57; // A3 = 220 Hz
const SEEDS = [1234, 777, 42];

// DYN scenarios (Phase 3, ADR-023): params are CORE keys (what the C++ side
// replays); coreToDyn() translates for the JS reference. lpOut=0 selects the
// no-output-pole config (the one structural difference between references).
// All dyn scenarios pin the DYN defaults that differ from SAW defaults
// explicitly (n, detune, retrig) so the core replay is self-contained.
// panLayout 1 (ADR-070): the DYN reference is a separate frozen program whose
// pan image is the legacy x-proportional one — when the CORE's default moved to
// the fan, these scenarios had to say so explicitly (coreToDyn drops the key).
const DYN_BASE = { lpOut: 0, n: 24, detune: 0.2, retrig: 0, dist: 0, panLayout: 1 };  // dyn is always even-spread
const DYN_SCENARIOS = [
  { name: 'dyn-meanfield-alpha', p: { ...DYN_BASE, K: 0.9, alpha: 60 } },
  { name: 'dyn-ring',            p: { ...DYN_BASE, topo: 1, reach: 5, alpha: 80, K: 0.9 } },
  { name: 'dyn-twocluster',      p: { ...DYN_BASE, topo: 2, mu: 0.6, alpha: 78, K: 1.0, n: 32, detune: 0.02 } },
  { name: 'dyn-cluster-balance', p: { ...DYN_BASE, topo: 2, mu: 0.3, K: 1.0, balance: 0.5, n: 24, detune: 0.3 } },  // ADR-051: exercises the kB path
  { name: 'dyn-poles2',          p: { ...DYN_BASE, poles: 2, K: 1.0 } },
  { name: 'dyn-poles3',          p: { ...DYN_BASE, poles: 3, K: 1.0 } },
  { name: 'dyn-gravity',         p: { ...DYN_BASE, grav: 0.7, basin: 35, K: 0.3 }, notes: [57, 64] },
  { name: 'dyn-grid',            p: { ...DYN_BASE, law: 3, bpm: 120, beatMult: 1, detune: 0.6, n: 16, K: 0 } },
];

function coreToDyn(k, v) {
  if (k === 'dist') return null;  // dyn has no distribution selector
  if (k === 'width') return ['swidth', v];
  if (k === 'law') return v === 3 ? ['beatQ', 1] : null;
  if (k === 'lpOut') return null;
  if (k === 'panLayout') return null;  // ADR-070: dyn has no layout selector
  return [k, v];
}

// Param vectors covering each SAW subsystem (L0-1). Keys are SwarmSynth.p
// members; anything unspecified stays at the reference default.
const SCENARIOS = [
  { name: 'defaults',      p: {} },
  { name: 'sync-lock',     p: { K: 1.0 } },
  { name: 'sync-shimmer',  p: { K: 0.7 } },
  { name: 'splay-jp7',     p: { K: -1.0, dist: 1, n: 7 } },
  { name: 'inertia-hunt',  p: { K: 0.85, inertia: 0.5 } },
  { name: 'rtone-full',    p: { K: 1.0, rtone: 1.0 } },
  { name: 'scatter-drift', p: { retrig: 0, driftDepth: 0.5 } },
  { name: 'onset-dissolve',p: { K: 0.9, onset: 1.0, dissolve: 2.0 } },
  // Seeded distributions — these (with scatter-drift) are the scenarios where
  // the seed axis actually varies the render: JP/even + retrig + no drift are
  // seed-independent by design (SPEC Layer 1: seed governs Gaussian/Cauchy
  // draws, un-retriggered scatter, drift streams).
  { name: 'gauss-cloud',   p: { dist: 2, detune: 0.4, n: 16 } },
  { name: 'cauchy-cloud',  p: { dist: 3, detune: 0.3, n: 16, retrig: 0 } },
  // Tone tilt (ADR-060) — LP (darken) + HP (thin) branches, per-voice pitch-tracked.
  { name: 'tilt-dark',     p: { tilt: 0.6, K: 0.3 } },
  { name: 'tilt-thin',     p: { tilt: -0.6, detune: 0.5, n: 7 } },
  // Hi-tame (ADR-061) — per-voice equal-loudness roll-off across a wide stack.
  { name: 'hi-tame',       p: { hiTame: 1.0, detune: 0.6, n: 12 } },
  /* Saw shape / glass (ADR-094) — the fifth detune-lab fold. Three scenarios so
     each axis is exercised separately and then together: the base crossfade
     alone, the roundness morph alone, and roundness scaled by voice pitch.
     Without these the feature would be inert-and-green, which says nothing —
     exactly the hole ADR-093 found in the glide goldens. */
  { name: 'saw-base',      p: { sawBase: 0.55, detune: 0.5, n: 7 } },
  { name: 'saw-glass',     p: { round: 0.8, sawProfile: 0.0, detune: 0.5, n: 7 } },
  { name: 'saw-round-hi',  p: { round: 0.7, roundHi: 0.9, sawProfile: 0.6, detune: 0.6, n: 9 } },
  // Drift modes + keep-phase (ADR-062). keep-phase also exercises the rngNext-skip
  // in note-on (it must leave rngState identical across engines → same drift).
  { name: 'drift-sine',    p: { driftDepth: 15, driftRate: 0.5, driftMode: 1, detune: 0.4, n: 7 } },
  { name: 'drift-sh',      p: { driftDepth: 15, driftRate: 0.5, driftMode: 2, detune: 0.4, n: 7 } },
  { name: 'keep-phase',    p: { keepPhase: 1, retrig: 0, driftDepth: 12, detune: 0.4, n: 7 } },
  // Frequency glide (ADR-063) — opt-in de-zipper; exercised against S&H drift,
  // the steppiest source, so the smoothing is genuinely under load.
  { name: 'freq-glide',    p: { freqGlide: 0.006, driftDepth: 40, driftMode: 2, driftRate: 0.9, detune: 0.5, n: 7 } },
  // Pan motion + centre pin (ADR-064). The centre-pin scenario drives BOTH legs
  // (drift weighting and pan-motion weighting) so cdist is exercised end to end.
  { name: 'pan-drift',     p: { panMotion: 0.6, detune: 0.5, n: 7, width: 0.9 } },
  { name: 'pan-sweep',     p: { panMotion: 0.6, panMode: 1, detune: 0.5, n: 7, width: 0.9 } },
  { name: 'centre-pin',    p: { panMotion: 0.6, motionCenter: 1, driftDepth: 30, driftMode: 1, detune: 0.5, n: 7, width: 0.9 } },
  // Harmonic law (ADR-065, law 4 — law 3 is the tempo grid). Full series, the
  // mid-morph, and a fractional reach.
  { name: 'harm-series',   p: { law: 4, detune: 1, n: 7 } },
  { name: 'harm-partial',  p: { law: 4, detune: 0.5, n: 7 } },
  { name: 'harm-reach',    p: { law: 4, detune: 1, harmReach: 1.8333333333333333, n: 7 } },
  // NOTE: a coupled harmonic scenario (law 4 + strong K) is deliberately ABSENT.
  // At full harmonic spread the swarm is CHAOTIC: a 1-ULP perturbation of the JS
  // reference alone diverges to rms ~9.4e-2 (measured), i.e. as far as C++ vs JS.
  // Sample-exact parity is impossible there in principle (Math.sin and std::sin
  // are not identically rounded), so that path is covered by a behavioural anchor
  // in trajectory_check, not by a golden. See ADR-065.
  // Stretch law (ADR-066, law 5). stretch-flat pins the algebraic identity
  // B = 0 ≡ law 0; the other two cover mild and full bell inharmonicity.
  // Pan image (ADR-070 — THE default-changing divergence: the alternating
  // pitch-ranked fan replaced the x-proportional image as default, so every
  // width-bearing SAW golden re-measured at that merge; dyn/spectra untouched).
  // pan-legacy pins the OLD image: same params as tilt-thin + panLayout 1 — it
  // must hash-equal the pre-ADR-070 tilt-thin render (checked at fold time).
  { name: 'pan-fan-gauss',  p: { dist: 2, detune: 0.5, n: 9, width: 0.9 } },
  { name: 'pan-fan-curve',  p: { panCurve: 0.15, detune: 0.5, n: 9, width: 0.9 } },
  { name: 'pan-fan-invert', p: { panInvert: 1, detune: 0.5, n: 9, width: 0.9 } },
  { name: 'pan-fan-harm',   p: { law: 4, detune: 0.8, n: 7, width: 0.9 } },
  { name: 'pan-legacy',     p: { panLayout: 1, tilt: -0.6, detune: 0.5, n: 7 } },
  // Root-pinned pacemaker (ADR-069). Pivot only acts THROUGH coupling, so these
  // are coupled by necessity — spreads kept modest to stay inside the parity
  // domain (ACCEPTANCE L0-1 limit, ADR-065). Sync collapse, splay anchored on
  // the root, and the musical combo with the ADR-068 root-anchored geometry.
  { name: 'pivot-lock',       p: { pivotMode: 1, K: 0.8, detune: 0.4, n: 9 } },
  { name: 'pivot-splay',      p: { pivotMode: 1, K: -0.8, detune: 0.3, n: 7 } },
  { name: 'pivot-anchor',     p: { pivotMode: 1, anchor: 1, spread: 4, detune: 0.5, K: 0.7, n: 7 } },
  // Octave spread + root anchor (ADR-068). spread multiplies detune (dep 12 =
  // ±1 oct on law 0); anchor 1 pins the lowest voice to the root (unipolar fan).
  // anchor-root uses seeded cauchy so xmin varies per seed and is exercised for
  // real. Wide-spread scenarios stay UNCOUPLED (K 0) — wide + strong K is the
  // chaotic regime excluded by the ACCEPTANCE L0-1 domain limit (ADR-065).
  { name: 'spread-octave',    p: { spread: 24, detune: 0.5, n: 9 } },
  { name: 'anchor-root',      p: { anchor: 1, dist: 3, detune: 0.4, n: 9 } },
  { name: 'spread-anchor-ni', p: { spread: 8, detune: 0.6, anchor: 1, n: 7 } },
  // Golden distribution (ADR-067, dist 4). Deterministic (no rng draws), so like
  // even/JP the seed axis is inert — covered across width + a coupled variant.
  { name: 'golden-dist',   p: { dist: 4, detune: 0.5, n: 11, width: 0.9 } },
  { name: 'golden-sync',   p: { dist: 4, detune: 0.35, n: 9, K: 0.8 } },
  { name: 'stretch-flat',  p: { law: 5, detune: 0.3, n: 9 } },
  { name: 'stretch-mild',  p: { law: 5, stretchB: 2, detune: 0.3, n: 9 } },
  { name: 'stretch-bell',  p: { law: 5, stretchB: 6, detune: 0.5, n: 12 } },
  // ADR-073 even-voice fan: n=2 is the degenerate case the old law broke
  // (centre + hard side); n=4 covers the multi-pair path. Width is EXPLICIT —
  // the fan is inert at width 0, so a golden without width proves nothing.
  { name: 'even-fan-2',    p: { n: 2, width: 1.0, detune: 0.4 } },
  { name: 'even-fan-4',    p: { n: 4, width: 1.0, detune: 0.4, K: 0.4 } },
];

const mtof = (m) => 440 * Math.pow(2, (m - 69) / 12);

function renderScenario(Engine, seed, params, notes = [MIDI], translate = null) {
  const s = new Engine(SR);
  for (const [k, v] of Object.entries({ seed, ...params })) {
    const kv = translate ? translate(k, v) : [k, v];
    if (kv) s.setParam(kv[0], kv[1]);
  }
  for (const m of notes) s.noteOn(m, mtof(m));
  const total = SECONDS * SR;
  const L = new Float32Array(BLOCK), R = new Float32Array(BLOCK);
  const out = new Float32Array(total * 2);
  let noteOffDone = false;
  for (let off = 0; off < total; off += BLOCK) {
    if (!noteOffDone && off >= NOTE_OFF_AT * SR) {
      for (const m of notes) s.noteOff(m);
      noteOffDone = true;
    }
    s.render(L, R);
    const n = Math.min(BLOCK, total - off);
    for (let i = 0; i < n; i++) {
      out[(off + i) * 2] = L[i];
      out[(off + i) * 2 + 1] = R[i];
    }
  }
  return out;
}

function sha256(f32) {
  return createHash('sha256').update(Buffer.from(f32.buffer, f32.byteOffset, f32.byteLength)).digest('hex');
}

const selfcheck = process.argv.includes('--selfcheck');
const SwarmSynth = extractCore(join(repo, 'reference/swarmsaw.html'), 'SwarmSynth');
const DynSynth = extractCore(join(repo, 'reference/swarmdynamics.html'), 'DynSynth');

const manifest = { generated_by: 'tools/golden/gen_goldens.mjs', reference: 'reference/swarmsaw.html',
                   sr: SR, seconds: SECONDS, note: MIDI, note_off_at: NOTE_OFF_AT, entries: [] };
let failures = 0;

if (!selfcheck) mkdirSync(outDir, { recursive: true });

const ALL = [
  ...SCENARIOS.map(sc => ({ ...sc, engine: 'saw' })),
  ...DYN_SCENARIOS.map(sc => ({ ...sc, engine: 'dyn' })),
];
for (const scenario of ALL) {
  const Engine = scenario.engine === 'dyn' ? DynSynth : SwarmSynth;
  const translate = scenario.engine === 'dyn' ? coreToDyn : null;
  const notes = scenario.notes || [MIDI];
  for (const seed of SEEDS) {
    const name = `${scenario.name}.seed${seed}`;
    const a = renderScenario(Engine, seed, scenario.p, notes, translate);
    const hashA = sha256(a);
    if (selfcheck) {
      const hashB = sha256(renderScenario(Engine, seed, scenario.p, notes, translate));
      const ok = hashA === hashB;
      if (!ok) failures++;
      console.log(`${ok ? 'OK ' : 'DRIFT'} ${name} ${hashA.slice(0, 12)}`);
    } else {
      const file = `${name}.f32`;
      writeFileSync(join(outDir, file), Buffer.from(a.buffer));
      let peak = 0, sum = 0;
      for (let i = 0; i < a.length; i++) { const v = Math.abs(a[i]); if (v > peak) peak = v; sum += a[i] * a[i]; }
      manifest.entries.push({ name, engine: scenario.engine, file, seed, params: scenario.p,
                              notes, sha256: hashA,
                              peak: +peak.toFixed(6), rms: +Math.sqrt(sum / a.length).toFixed(6) });
      console.log(`wrote ${file} peak=${peak.toFixed(3)}`);
    }
  }
}

if (selfcheck) {
  if (failures) { console.error(`gen_goldens: ${failures} scenario(s) NON-DETERMINISTIC`); process.exit(1); }
  console.log(`gen_goldens: ${SCENARIOS.length * SEEDS.length} scenarios deterministic`);
} else {
  writeFileSync(join(outDir, 'manifest.json'), JSON.stringify(manifest, null, 2));
  // TSV twin of the manifest for the C++ parity harness: params serialized in
  // APPLICATION ORDER (seed first, then scenario keys in object order) — the
  // C++ side must call setParam in this exact order to reproduce rebuild()
  // RNG draws.
  const tsv = manifest.entries.map(e =>
    [e.name, e.engine, e.file, e.seed,
     Object.entries({ seed: e.seed, ...e.params }).map(([k, v]) => `${k}=${v}`).join(','),
     e.notes.join('+')].join('\t')
  ).join('\n') + '\n';
  writeFileSync(join(outDir, 'manifest.tsv'), tsv);
  console.log(`manifest: ${manifest.entries.length} goldens -> build-golden/`);
}
