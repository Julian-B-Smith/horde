/*
 * h2_engine_render.mjs — the JS half of tools/h2_engine_parity_check.cpp
 * (ROADMAP B385; design docs/port/h2-engine.md; ADR-187 items 3 and 6). It
 * renders every parity scenario through the GOLDEN, the composed engine
 * (docs/design/scalpel-horde-engine.js over reference/scalpel/prototype/razor-core.js
 * and reference/swarmsaw.html's SwarmSynth), seeded, and streams the scripts,
 * the rendered samples (float64) and the blade-event digests to stdout. The C++
 * check replays each script through h2/engine/engine.h and compares. Nothing is
 * written to the repo: the JS is rendered at check time, every time, so the
 * golden stays the live source of truth.
 *
 *   node tools/h2_engine_render.mjs --list                scenario names
 *   node tools/h2_engine_render.mjs --only 'GRAV' > s.bin  a subset, to a file
 *   build-release/h2_engine_parity_check s.bin            replay a stream file
 *   node tools/h2_engine_render.mjs --bench               JS CPU per voice (Layer-E)
 *
 * NO GOLDEN FILE IS EDITED. Two copies of the engine are built from their text:
 *   - PRISTINE: the files as they are. ITS samples are streamed and compared.
 *   - INSTRUMENTED: in-memory scratch copies with literal insertions that report
 *     blade events (the blade oracle's seven, tools/h2_scenarios.mjs, plus one in
 *     the composed engine's own BLEP scanner, the D2 path that bypasses the
 *     oracle's). Each insertion must match exactly once, or this fails loudly.
 *     Rendered alongside every scenario for its event digest only; its samples
 *     must equal the pristine ones bit for bit (the NI line, the NONINV row).
 * Every golden file's git blob is streamed (ORACLE records), pinning the target
 * by content; the check compares them with h2/README.md's pins.
 *
 * SEEDING. Math.random is replaced by mulberry32(seed) around every instance:
 * the composed engine's seeded wrapper (ADR-187 item 3). The C++ consumes the same
 * stream in the same order (Engine::seedRandom).
 *
 * CHAOTIC EXCLUSIONS (ADR-065's evidence rule, as phase 1a applies it). A
 * scenario leaves the max-abs verdict ONLY by being listed in CHAOTIC with its
 * reason. Every run re-renders it with every note-on frequency nudged one ULP up,
 * and the check refuses the exclusion unless the JS alone misses max-abs by at
 * least as much as the C++ does.
 */
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { Worker, isMainThread, parentPort } from 'node:worker_threads';
import { availableParallelism } from 'node:os';
import { createHash } from 'node:crypto';
import { SR, BLK, mtof, mulberry32, loadInstrumented, insertAll, notesFor, presetCmds, on, off, blocks, PHRASES, bladeRows, rowScenario, spliceAt } from './h2_scenarios.mjs';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const require = createRequire(import.meta.url);
// The golden, as five files (docs/port/h2-engine.md, "The golden").
const GOLDEN = [
  'docs/design/scalpel-horde-engine.js',
  'reference/scalpel/prototype/razor-core.js',
  'reference/swarmsaw.html',
  'reference/scalpel/data/presets.json',
  'docs/design/scalpel-interface-lab.html',
];
const at = rel => join(root, rel);

/* --------------------------------------------------------------- the golden */
function swarmSource() {
  const { swarmSourceFromHtml } = require(at(GOLDEN[0]));
  return swarmSourceFromHtml(readFileSync(at(GOLDEN[2]), 'utf8'));
}
function loadPristine() {
  const { makeComposedEngine } = require(at(GOLDEN[0]));
  return makeComposedEngine(require(at(GOLDEN[1])), swarmSource());
}
// The composed engine's own BLEP site (its scan(), ADR-189 D2), which calls addE
// directly and so is not seen by the oracle's insertions.
const ENGINE_INSTRUMENT = [
  ['if (tau > 0 && tau <= 1) this.addE(m, p0 + tau * dphi, tau, c, k, s);',
   'if (tau > 0 && tau <= 1) { RazorCore.__ev(2); this.addE(m, p0 + tau * dphi, tau, c, k, s); }'],
];
function loadInstrumentedEngine() {
  const RCi = loadInstrumented(at(GOLDEN[1]));
  const src = insertAll(readFileSync(at(GOLDEN[0]), 'utf8'), ENGINE_INSTRUMENT, 'scalpel-horde-engine.js');
  const mod = { exports: {} };
  new Function('module', src)(mod);
  return { E: mod.exports.makeComposedEngine(RCi, swarmSource()), RC: RCi };
}
// The lab's B366 envelope presets, read from the lab's own text (never copied):
// the ENV_PRESETS literal, evaluated alone.
function labPresets() {
  const html = readFileSync(at(GOLDEN[4]), 'utf8');
  const A = 'const ENV_CAT = ', B = '].map(p => Object.assign({ category: ENV_CAT, lab: true }, p));';
  const a = html.indexOf(A), b = html.indexOf(B, a);
  if (a < 0 || b < 0 || html.indexOf(A, a + 1) >= 0) throw new Error('scalpel-interface-lab.html: the ENV_PRESETS literal was not found exactly once');
  return new Function(html.slice(a, b) + '];\nreturn ENV_PRESETS.map(p => Object.assign({ category: ENV_CAT }, p));')();
}

/* ---------------------------------------------------------- the exclusions */
// The ONLY way out of the max-abs verdict (header). name -> why.
const XM_RING = 'cross-member modulation ring (xm 0.7): last-bit feedback amplification (ADR-065 class)';
export const CHAOTIC = {
  'P/Starting points / Cross-mod ring (watch) :: chord': XM_RING,
  'P/Starting points / Cross-mod ring (watch) :: repeat': XM_RING,
  'P/Starting points / Cross-mod ring (watch) :: arp': XM_RING,
};

/* --------------------------------------------------------------- scenarios */
const B1 = { N: 3, w: 0.3, k: 5.5, c: 0.6 };
const B2 = { ...B1, b2on: 1, mode2: 4, w2: 0.2, k2: 3, c2: 0.35 };
const patch = over => { const c = []; for (const [k, v] of Object.entries(over)) c.push(typeof v === 'string' ? ['sets', k, v] : ['set', k, v]); c.push(['snap']); return c; };
const onF = (n, f, vel = 0.85) => ['on', n, f, vel];
const rb = (size, k) => ['render', size, k];

// The composed rows: each feature the golden adds over the blade oracle, in
// isolation (docs/port/h2-engine.md, "Scenario families", 4).
function composedRows(presets) {
  const C = [];
  const row = (name, over, body, { sr = SR, seed = 0xB385, pre = [] } = {}) =>
    C.push({ name: 'C/' + name, sr, seed, cmds: [...pre, ...patch(over), ...body] });
  const preset = name => { const p = presets.find(x => x.name === name); if (!p) throw new Error('no bench preset ' + name); return p; };
  const chord = PHRASES.chord([45]), arp = PHRASES.arp([45]), repeat = PHRASES.repeat([45]);

  // B310: the voice law
  row('VL repeat, the first release rings', { ...B1, R: 400 }, repeat);
  row('VL tiers 2 and 3', { ...B1, poly: 3, R: 600 }, [on(45), on(52), on(57), blocks(10), off(45), blocks(5), off(52), blocks(5),
    on(60), blocks(10), on(64), blocks(10), on(67), blocks(20), off(57), off(60), off(64), off(67), blocks(30)]);
  row('VL tier 1, a faded slot', { ...B1, poly: 2, R: 5 }, [on(45), blocks(5), off(45), blocks(20), on(52), on(57), blocks(10), off(52), off(57), blocks(20)]);
  // B323 + B375: the cap, the cull and the policy at a full cap
  const capRow = (name, over, cap, pol, body) => row(name, over, body, { pre: [['cap', cap], ['capPolicy', pol]] });
  // The cull (B323): the cap is LOWERED MID-PHRASE while tails release (a cap set
  // before the notes makes these refuse rows instead). R is long, so each culled
  // tail is still sounding when its 8 ms ramp ends and the ramp frees it.
  // The HELD note is the OLDEST, so a cull that ignored the gate would take it first.
  row('CAP cull, the cap lowered mid-phrase', { ...B1, R: 1500 }, [on(45), on(52), on(57), on(60), blocks(10), off(52), off(57), off(60), blocks(3),
    ['cap', 1], blocks(30), on(64), blocks(10), ['cap', 0], on(67), blocks(10), off(45), off(64), off(67), blocks(30)]);
  capRow('CAP refuse', { ...B1, R: 300 }, 2, 0, [on(45), on(52), blocks(10), on(57), blocks(20), off(45), off(52), off(57), blocks(20)]);
  capRow('CAP steal', { ...B1, R: 300 }, 2, 1, [on(45), on(52), blocks(10), on(57), blocks(20), off(45), off(52), off(57), blocks(20)]);
  capRow('CAP replace', { ...B1, R: 300 }, 2, 2, [on(45), on(52), blocks(10), on(57), blocks(20), off(45), off(52), off(57), blocks(20)]);
  capRow('CAP steal with no free slot', { ...B1, poly: 2, R: 300 }, 2, 1, [on(45), on(52), blocks(10), on(57), blocks(20), off(52), off(57), blocks(20)]);
  capRow('CAP not binding', { ...B1, R: 300 }, 8, 0, chord);
  // the per-partial fade: a culled voice's members scale with it (each member's own envelope)
  row('CAP cull per-partial voices, the cap lowered mid-phrase', { ...B1, R: 1500, voiceEnv: 1, attackScatter: 0.5, relScatter: 0.5 },
    [on(45), on(52), on(57), blocks(15), off(45), off(52), blocks(3), ['cap', 1], blocks(30), off(57), blocks(30)]);
  // B325: the first tick, on and off the 16-sample grid
  row('FT lock 2, 37-sample blocks', { ...B1, lock: 2, kHz: 900 }, [on(45), rb(37, 3), on(52), rb(37, 5), on(57), rb(37, 40), off(45), off(52), off(57), rb(37, 30)]);
  row('FT Hz modulator, 100-sample blocks', { ...B1, mode: 2, mUnit: 1, mHz: 440 }, [on(45), rb(100, 3), on(52), rb(100, 40), off(45), off(52), rb(100, 20)]);
  row('FT chord on a mono voice in one block', { ...B1, polyMode: 1, lock: 2, kHz: 900 }, [on(45), on(52), on(57), blocks(40), off(57), off(52), off(45), blocks(20)]);
  row('FT retune before the first sample', { ...B1, lock: 2, kHz: 900 }, [on(45), ['re', 45, mtof(47)], blocks(40), off(45), blocks(20)]);
  // B335: gravity
  const G = { ...B1, grav: 1, R: 300 };
  row('GRAV a sharp fifth', G, [onF(57, mtof(57)), onF(64, mtof(64) * Math.pow(2, 15 / 1200)), blocks(120), off(57), off(64), blocks(20)]);
  row('GRAV a triad, K .35', { ...G, N: 5 }, [on(57), on(61), on(64), blocks(120), off(57), off(61), off(64), blocks(20)]);
  row('GRAV an octave-folded twelfth', G, [on(45), on(64), blocks(100), off(45), off(64), blocks(20)]);
  row('GRAV a pair outside the basin', { ...G, basin: 5 }, [on(57), on(60), blocks(80), off(57), off(60), blocks(20)]);
  row('GRAV switched on mid-note', { ...G, grav: 0 }, spliceAt([on(57), on(64), blocks(100), off(57), off(64), blocks(20)], [[30, ['set', 'grav', 0.8]]]));
  row('GRAV 100-sample blocks', G, [on(57), on(64), rb(100, 120), off(57), off(64), rb(100, 20)]);
  row('GRAV a bend while settled', G, spliceAt([on(57), on(64), blocks(100), off(57), off(64), blocks(20)], [[50, ['set', 'bend', 2]]]));
  row('GRAV at 44.1 kHz', G, [on(57), on(64), blocks(100), off(57), off(64), blocks(20)], { sr: 44100 });
  // B335: the ensemble
  const O = { ...B1, N: 5, onsetScatter: 12, A: 20, R: 250 };
  for (const a of [0, 0.25, 1]) row(`ONS onset scatter, alpha ${a}`, { ...O, onsetAlpha: a }, arp);
  row('ONS attack and release scatter', { ...O, onsetScatter: 8, attackScatter: 0.8, relScatter: 0.8, A: 30 }, chord);
  row('ONS per-partial envelopes', { ...O, onsetScatter: 0, voiceEnv: 1, attackScatter: 0.6, relScatter: 0.6, A: 40, R: 300 }, chord);
  row('ONS per-partial envelopes, arp', { ...O, onsetScatter: 0, voiceEnv: 1, attackScatter: 0.6, relScatter: 0.6, A: 40, R: 300 }, arp);
  row('ONS scatter and envelopes together', { ...O, voiceEnv: 1, attackScatter: 0.4, relScatter: 0.4 }, arp);
  row('ONS a seed change re-derives the offsets', { ...O, dist: 2 }, spliceAt(arp, [[40, ['set', 'seed', 999]]]));
  row('ONS mono retrigger with per-partial envelopes', { ...B1, N: 5, polyMode: 1, voiceEnv: 1, attackScatter: 0.5, A: 30 }, PHRASES.legato([45]));
  row('ONS at 44.1 kHz', O, arp, { sr: 44100 });
  row('ONS onset scatter switched off mid-note', O, spliceAt(chord, [[20, ['set', 'onsetScatter', 0]]]));
  row('ONS per-partial envelopes switched off mid-note', { ...O, onsetScatter: 0, voiceEnv: 1, attackScatter: 0.6, relScatter: 0.6 },
    spliceAt(chord, [[40, ['set', 'voiceEnv', 0]]]));
  // ADR-189 D1-D3, each alone and all on, over the bench presets in their scope
  const flagSets = [['D1', { aaCarrier: 1 }], ['D2', { aaXin: 1 }], ['D3', { aaLoop: 1 }], ['D1-D3', { aaCarrier: 1, aaXin: 1, aaLoop: 1 }]];
  for (const name of ['Zap bass', 'Frozen noise FM', 'Trance jitter', 'Feedback snarl', 'Cross-mod roar']) {
    const p = preset(name), root = notesFor(p.category);
    for (const [tag, fl] of flagSets) C.push({ name: `C/${tag} :: ${name}`, sr: SR, seed: 0xB355, cmds: [...presetCmds({ ...p.params, ...fl }), ...PHRASES.chord(root)] });
  }
  row('D1 a sync carrier with Band-limit off', { ...B1, aa: 0, aaCarrier: 1 }, chord);
  row('D1 a ring carrier with Band-limit off', { ...B1, mode: 5, aa: 0, aaCarrier: 1 }, chord);
  row('D1 an FM carrier, pitch FM', { ...B1, mode: 2, fmType: 1, I: 2, aaCarrier: 1 }, chord);
  row('D1 an S&H modulator (out of scope)', { ...B1, mode: 2, mshape: 7, I: 2.5, aaCarrier: 1 }, chord);
  row('D1 switched on mid-note', { ...B1, mode: 2, I: 2 }, spliceAt(chord, [[30, ['set', 'aaCarrier', 1]], [60, ['set', 'aaCarrier', 0]]]));
  row('D2 blade 2 carrier under feedback', { ...B2, mode2: 0, hot2: 2, fb: 0.3, aaXin: 1 }, chord);
  // D1 and D2 under the blade paths that move the carrier phase (critic L4)
  row('D1 width-locked FM carrier', { ...B1, mode: 2, I: 2, lock: 1, aaCarrier: 1 }, chord);
  row('D1 mirrored FM carrier', { ...B1, mode: 2, I: 2, mirror: 1, hard: 0.2, aaCarrier: 1 }, chord);
  row('D1 under collision', { ...B2, mode: 2, mode2: 2, I: 2, colK: 0.9, colB: 0.5, w: 0.5, w2: 0.5, aaCarrier: 1 }, chord);
  row('D1 on a serial input', { ...B2, aa: 0, mode2: 0, hot2: 2, b2mix: 0.7, aaCarrier: 1 }, chord);
  row('D1 serial twins', { ...B2, mode: 2, I: 2, b2mix: 0.6, mirror: 3, mirror2: 2, aaCarrier: 1 }, chord);
  row('D2 under phase FM', { ...B1, mode: 2, fmType: 0, I: 2, fb: 0.3, aaXin: 1 }, chord);
  row('D2 width-locked', { ...B1, lock: 1, fb: 0.3, aaXin: 1 }, chord);
  row('D2 mirrored', { ...B1, mirror: 1, hard: 0.2, fb: 0.3, aaXin: 1 }, chord);
  row('D2 under collision', { ...B2, mode2: 0, hot2: 2, colK: 0.9, w: 0.5, w2: 0.5, fb: 0.3, aaXin: 1 }, chord);
  row('D3 a cross-mod ring', { ...B1, N: 5, xm: 0.5, aaLoop: 1 }, chord);
  row('D3 switched on mid-note', { ...B1, N: 1, fb: 0.5 }, spliceAt(chord, [[30, ['set', 'aaLoop', 1]]]));
  // B382 M1-M3 at both rates
  for (const sr of [44100, 48000, 96000]) {
    row(`M1 K .35 at ${sr}`, { ...B1, N: 7, detune: 25 }, chord, { sr });
    row(`M1 K 1 at ${sr}`, { ...B1, N: 7, K: 1, detune: 40 }, chord, { sr });
    row(`M1 K -.6 at ${sr}`, { ...B1, N: 6, K: -0.6 }, chord, { sr });
  }
  for (const sr of [44100, 48000]) {
    row(`M2 onset +.5 at ${sr}`, { ...B1, N: 5, onset: 0.5, dissolve: 0.3, K: 0.1 }, arp, { sr });
    row(`M2 onset -.5 at ${sr}`, { ...B1, N: 5, onset: -0.5, dissolve: 0.3, K: 0.1 }, arp, { sr });
    row(`M3 law 3 at 120 bpm, ${sr}`, { ...B1, N: 5, 'h.law': 3, detune: 30, K: 0 }, chord, { sr });
    row(`M3 law 3 at 140 bpm x2, ${sr}`, { ...B1, N: 5, 'h.law': 3, bpm: 140, beatMult: 2, detune: 30, K: 0.2 }, chord, { sr });
  }
  // the swarm's own parameters
  for (const dist of [0, 1, 2, 3, 4]) row(`SW dist ${dist}`, { ...B1, N: 7, dist, seed: 77, detune: 30 }, chord);
  for (const law of [0, 1, 2, 4, 5]) row(`SW law ${law}`, { ...B1, N: 5, 'h.law': law, detune: 40, harmReach: 0.5, stretchB: 2 }, chord);
  for (const dm of [0, 1, 2]) row(`SW drift mode ${dm}`, { ...B1, N: 5, driftDepth: 25, driftMode: dm, 'h.driftRate': 0.6 }, chord);
  row('SW drift with the centre pin', { ...B1, N: 5, driftDepth: 25, motionCenter: 0.5 }, chord);
  for (const curve of [0.5, 2.5]) row(`SW inertia, curve ${curve}`, { ...B1, N: 5, inertia: 0.5, inertiaCurve: curve, K: 0.6 }, chord);
  row('SW frequency glide under drift and retune', { ...B1, N: 5, freqGlide: 0.05, driftDepth: 20 }, PHRASES.retune([45]));
  row('SW keep phase', { ...B1, N: 5, keepPhase: 1, phaseMode: 0 }, repeat);
  row('SW random start phases (retrig off)', { ...B1, N: 7, phaseMode: 0 }, arp);
  for (const K of [0.6, -0.6]) row(`SW pivot, K ${K}`, { ...B1, N: 5, pivotMode: 1, K, detune: 30 }, chord);
  row('SW anchor and spread', { ...B1, N: 5, anchor: 1, spread: 3, detune: 20 }, chord);
  row('SW a seed change mid-phrase', { ...B1, N: 5, dist: 2, detune: 30 }, spliceAt(arp, [[30, ['set', 'seed', 4242]]]));
  row('SW N changes mid-note', { ...B1, N: 3, K: 0.4 }, spliceAt(chord, [[30, ['set', 'N', 7]], [60, ['set', 'N', 2]]]));
  // ADR-184 A2
  row('A2 negative Rotate spread', { ...B1, N: 5, rotSpread: -0.4 }, chord);
  row('A2 blade 2 own negative Rotate spread', { ...B2, N: 5, b2sp: 1, rotSpread2: -0.3, rot2Follow: 0, rotRate2: 0.5 }, chord);
  row('A2 Rotate spread flips sign mid-note', { ...B1, N: 5, rotSpread: 0.4 }, spliceAt(chord, [[35, ['set', 'rotSpread', -0.4]]]));
  for (const ks of [-2.5, 2.5]) row(`A2 Quantized Cut spread ${ks}`, { ...B1, N: 6, kspread: ks, kq: 1 }, chord);
  row('A2 Quantized blade 2 Cut spread -1.5', { ...B2, N: 6, b2sp: 1, kspread2: -1.5, kq: 1 }, chord);
  // oversampling changed mid-note
  row('OS 2 -> 4 -> 1 mid-note', { ...B2 }, spliceAt(chord, [[30, ['set', 'os', 4]], [60, ['set', 'os', 1]]]));
  return C;
}

function buildScenarios() {
  const presets = JSON.parse(readFileSync(at(GOLDEN[3]), 'utf8')).presets;
  const out = [];
  const phrasesOf = pr => pr.params.polyMode ? ['chord', 'repeat', 'arp', 'legato'] : ['chord', 'repeat', 'arp'];
  for (const pr of presets) {
    const root = notesFor(pr.category);
    for (const ph of phrasesOf(pr)) out.push({ name: `P/${pr.category} / ${pr.name} :: ${ph}`, sr: SR, seed: 0xC0FFEE, cmds: [...presetCmds(pr.params), ...PHRASES[ph](root)] });
  }
  for (const pr of labPresets()) {
    const root = notesFor(pr.category);
    for (const ph of phrasesOf(pr)) out.push({ name: `E/${pr.name} :: ${ph}`, sr: SR, seed: 0xB366, cmds: [...presetCmds(pr.params), ...PHRASES[ph](root)] });
  }
  for (const t of bladeRows()) out.push(rowScenario(t, 0xB332));
  out.push(...composedRows(presets));
  // --nudge K (L0071's probe, for the bit-exact floor): every note-on and retune
  // frequency IN THE SCRIPT moved K doubles up, so both sides render the nudged
  // inputs and only the share of bit-identical samples can move
  const k = Number(arg('--nudge', 0));
  if (k > 0) for (const sc of out) for (const cm of sc.cmds) if (cm[0] === 'on' || cm[0] === 're') for (let i = 0; i < k; i++) cm[2] = nextUp(cm[2]);
  return out;
}

/* ----------------------------------------------------------------- render */
function nextUp(x) {   // the next double above x (x > 0): a one-ULP perturbation
  const f = new Float64Array([x]), b = new BigUint64Array(f.buffer); b[0] += 1n; return f[0];
}
function renderWith(E, sc, { instrument = null, perturb = false } = {}) {
  const SAVED = Math.random;
  Math.random = mulberry32(sc.seed >>> 0);
  try {
    const c = new E(sc.sr);
    if (instrument) { c.__si = 0; c.voices.forEach((v, i) => { v.__vi = i; }); instrument.__log = { count: [0, 0, 0, 0, 0], h1: 2166136261, h2: 0 }; }
    let total = 0;
    for (const cm of sc.cmds) if (cm[0] === 'render') total += cm[1] * cm[2];
    const buf = new Float64Array(total * 2);
    let pos = 0;
    for (const cm of sc.cmds) {
      switch (cm[0]) {
        case 'set': case 'sets': c.set({ [cm[1]]: cm[2] }); break;
        case 'snap': Object.assign(c.s, c.t); break;
        case 'on': c.noteOn(cm[1], perturb ? nextUp(cm[2]) : cm[2], cm[3]); break;
        case 'off': c.noteOff(cm[1]); break;
        case 're': c.msg({ t: 're', note: cm[1], freq: perturb ? nextUp(cm[2]) : cm[2] }); break;
        case 'panic': c.msg({ t: 'panic' }); break;
        case 'cap': c.msg({ t: 'cap', n: cm[1] }); break;
        case 'capPolicy': c.msg({ t: 'capPolicy', n: cm[1] }); break;
        case 'render': {
          const L = new Float64Array(cm[1]), R = new Float64Array(cm[1]);
          for (let b = 0; b < cm[2]; b++) { c.render(L, R); for (let i = 0; i < cm[1]; i++) { buf[pos++] = L[i]; buf[pos++] = R[i]; } }
          break;
        }
        default: throw new Error('bad command ' + cm[0]);
      }
    }
    return { buf, log: instrument ? instrument.__log : null, counts: [c.culled, c.refused, c.stolen] };
  } finally {
    Math.random = SAVED;
  }
}
function scriptText(i, sc) {
  const L = [`SCN ${i} ${sc.name}`, `SR ${sc.sr}`, `SEED ${sc.seed >>> 0}`];
  for (const cm of sc.cmds) L.push(cm[0] === 'sets' ? `sets ${cm[1]} ${cm[2]}` : cm.join(' '));
  return L.join('\n') + '\n';
}
function diff(a, b) {
  let e = 0, mx = 0;
  for (let i = 0; i < a.length; i++) { const d = Math.abs(a[i] - b[i]); e += d * d; if (d > mx || d !== d) mx = d !== d ? Infinity : d; }
  return { rms: Math.sqrt(e / a.length), max: mx };
}
function job(Ep, Ei, i, sc) {
  const { buf, counts } = renderWith(Ep, sc);
  const ins = renderWith(Ei.E, sc, { instrument: Ei.RC }), log = ins.log;
  let same = ins.buf.length === buf.length;
  for (let k = 0; same && k < buf.length; k++) if (!Object.is(buf[k], ins.buf[k])) same = false;
  let head = scriptText(i, sc);
  head += `EV ${log.count[1]} ${log.count[2]} ${log.count[3]} ${log.count[4]} ${log.h1} ${log.h2}\nNI ${same ? 1 : 0}\n`;
  head += `CNT ${counts.join(' ')}\n`;   // the load readouts: tails culled, notes refused, voices stolen
  if (CHAOTIC[sc.name]) {
    const p = renderWith(Ep, sc, { perturb: true });
    const d = diff(buf, p.buf);
    head += `EXCL ${CHAOTIC[sc.name]}\nSELF ${d.rms} ${d.max}\n`;
  }
  head += `DATA ${buf.length / 2}\n`;
  return { head, buf };
}

/* ------------------------------------------------------------------ main */
const argv = process.argv.slice(2);
const arg = (k, dflt) => { const i = argv.indexOf(k); return i >= 0 ? argv[i + 1] : dflt; };

if (!isMainThread) {
  const Ep = loadPristine(), Ei = loadInstrumentedEngine();
  const all = buildScenarios();
  parentPort.on('message', i => {
    if (i < 0) { process.exit(0); }
    const r = job(Ep, Ei, i, all[i]);
    parentPort.postMessage({ i, head: r.head, buf: r.buf }, [r.buf.buffer]);
  });
} else if (argv.includes('--list')) {
  buildScenarios().forEach((s, i) => console.log(`${i}\t${s.name}`));
} else if (argv.includes('--bench')) {
  bench();
} else {
  await main();
}

async function main() {
  const t0 = process.hrtime.bigint();
  const all = buildScenarios();
  const only = arg('--only', null), re = only ? new RegExp(only) : null;
  const idx = all.map((_, i) => i).filter(i => !re || re.test(all[i].name));
  const out = process.stdout;
  const write = data => new Promise(res => { if (out.write(data)) res(); else out.once('drain', res); });
  await write(`H2ENGINE 1 ${idx.length}\n`);
  await write(`NODE ${process.versions.node}\n`);   // the libm on the golden's side (the floor's pin)

  // libm probes (Layer-E, printed by the check, never judged): V8's value of each
  // transcendental the engine calls, on inputs in the ranges it calls them with.
  {
    const r = mulberry32(0x11B3), U = (a, b) => a + (b - a) * r(), n = 20000;
    const probes = {
      sin: () => { const x = r() < 0.5 ? U(-10, 10) : Math.floor(U(0, 70000)) * 127.1 + 311.7; return [x, 0, Math.sin(x)]; },   // phases; hash() arguments
      cos: () => { const x = U(-10, 10); return [x, 0, Math.cos(x)]; },
      exp: () => { const x = U(-60, 5); return [x, 0, Math.exp(x)]; },
      log: () => { const x = r() < 0.5 ? U(1e-9, 1) : U(1, 10); return [x, 0, Math.log(x)]; },
      log2: () => { const x = U(1, 4); return [x, 0, Math.log2(x)]; },   // gravity's interval
      pow: () => { const b = U(0.01, 4), e = U(-8, 8); return [b, e, Math.pow(b, e)]; },
      pow2: () => { const e = U(-8, 8); return [2, e, Math.pow(2, e)]; },   // compiled as exp2 (the check says so)
      atan2: () => { const y = U(-1, 1), x = U(-1, 1); return [y, x, Math.atan2(y, x)]; },
      asin: () => { const x = U(0, 0.995); return [x, 0, Math.asin(x)]; },
      tan: () => { const x = U(-1.5, 1.5); return [x, 0, Math.tan(x)]; },   // placement, dist 3
      tanh: () => { const x = U(-4, 4); return [x, 0, Math.tanh(x)]; },
      sqrt: () => { const x = U(0, 10); return [x, 0, Math.sqrt(x)]; },
    };
    for (const [fn, gen] of Object.entries(probes)) {
      const v = new Float64Array(n * 3);
      for (let i = 0; i < n; i++) v.set(gen(), 3 * i);
      await write(`LIBM ${fn} ${n}\n`);
      await write(Buffer.from(v.buffer));
    }
  }
  // the golden, pinned by content (git blob ids)
  for (const rel of GOLDEN) {
    const bytes = readFileSync(at(rel));
    const blob = createHash('sha1').update(`blob ${bytes.length}\0`).update(bytes).digest('hex');
    await write(`ORACLE ${rel} ${blob}\n`);
  }

  const W = Math.max(1, Math.min(Number(arg('--workers', availableParallelism() - 1)), idx.length));
  const done = new Map();
  let next = 0, emit = 0, wake = null, failed = null;
  const feed = w => { if (next < idx.length) w.postMessage(idx[next++]); else w.postMessage(-1); };
  // Completion is "every scenario WRITTEN", never "every worker exited": an END
  // written before a slow reader drained the stream would truncate the check.
  for (let k = 0; k < W; k++) {
    const w = new Worker(fileURLToPath(import.meta.url), { argv });
    w.on('message', m => { done.set(m.i, m); feed(w); if (wake) { const f = wake; wake = null; f(); } });
    w.on('error', e => { failed = e; if (wake) { const f = wake; wake = null; f(); } });
    feed(w);
  }
  while (emit < idx.length) {
    if (failed) throw failed;
    const i = idx[emit];
    if (!done.has(i)) { await new Promise(r => { wake = r; }); continue; }
    const m = done.get(i); done.delete(i);
    await write(m.head);
    await write(Buffer.from(m.buf.buffer, m.buf.byteOffset, m.buf.byteLength));
    emit++;
    if (emit % 50 === 0) process.stderr.write(`h2_engine_render: ${emit}/${idx.length} scenarios streamed\n`);
  }
  await write(`END ${idx.length}\n`);
  process.stderr.write(`h2_engine_render: ${idx.length} scenarios, ${W} workers, ${(Number(process.hrtime.bigint() - t0) / 1e9).toFixed(1)} s\n`);
}

/* ------------------------------------------------------------ Layer-E: CPU */
// The golden's cost per voice as a fraction of real time, on the heavy class
// (Crushed bells, Glass horde pad) and a light one (Quarter sync): one voice held
// for 4 s at 48 kHz. A calibration loop is timed alongside (B236). --emit writes
// the scripts for the C++ half (tools/auhost --horde, or any replay of the stream).
function benchScenarios() {
  const presets = JSON.parse(readFileSync(at(GOLDEN[3]), 'utf8')).presets;
  const nb = Math.round(4 * SR / BLK);
  return ['Crushed bells', 'Glass horde pad', 'Quarter sync'].map(name => {
    const pr = presets.find(p => p.name === name);
    return { name: `BENCH/${name}`, sr: SR, seed: 0xB385, cmds: [...presetCmds(pr.params), on(57), blocks(nb)] };
  });
}
function bench() {
  const E = loadPristine(), sc = benchScenarios();
  if (argv.includes('--emit')) {
    process.stdout.write(`H2ENGINE 1 ${sc.length}\n`);
    for (const [i, s] of sc.entries()) {
      const { buf } = renderWith(E, s);
      process.stdout.write(scriptText(i, s) + `EV 0 0 0 0 0 0\nNI 1\nDATA ${buf.length / 2}\n`);
      process.stdout.write(Buffer.from(buf.buffer));
    }
    process.stdout.write(`END ${sc.length}\n`);
    return;
  }
  let x = 0; const c0 = process.hrtime.bigint();
  for (let i = 0; i < 1e8; i++) x = x * 1.0000001 + 1e-9;
  const cal = Number(process.hrtime.bigint() - c0) / 1e6;
  console.log(`calibration: 1e8 dependent multiply-adds in ${cal.toFixed(1)} ms (x=${x.toFixed(3)})`);
  for (const s of sc) {
    renderWith(E, s);   // warm-up (JIT)
    const best = [];
    for (let r = 0; r < 3; r++) { const t = process.hrtime.bigint(); renderWith(E, s); best.push(Number(process.hrtime.bigint() - t) / 1e9); }
    const secs = Math.min(...best), audio = s.cmds.filter(c => c[0] === 'render').reduce((a, c) => a + c[1] * c[2], 0) / s.sr;
    console.log(`${s.name.padEnd(24)} JS  ${(100 * secs / audio).toFixed(2)} % of real time per voice (${secs.toFixed(3)} s for ${audio.toFixed(2)} s, best of 3)`);
  }
}
