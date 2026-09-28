/*
 * h2_scalpel_render.mjs — the JS half of tools/h2_scalpel_parity_check.cpp
 * (ROADMAP B332 phase 1a; ADR-187 item 6). It renders every parity scenario
 * through the PROTECTED oracle reference/scalpel/prototype/razor-core.js in Node,
 * seeded, and streams the scenario scripts, the rendered samples (float64) and
 * the oracle's blade-event digests to stdout. The C++ check replays each script
 * through h2/cores/scalpel/razor_core.h and compares. Nothing is written to the
 * repo: the fixtures are regenerated at check time, every time, so the oracle
 * stays the live source of truth (the gen_goldens.mjs idiom).
 *
 * Run by the check itself (it spawns `node tools/h2_scalpel_render.mjs` from the
 * repo root). By hand:
 *   node tools/h2_scalpel_render.mjs --list               scenario names
 *   node tools/h2_scalpel_render.mjs --only 'Crushed' > local/s.bin
 *   build-release/h2_scalpel_parity_check local/s.bin     replay one stream file
 *   node tools/h2_scalpel_render.mjs --bench              JS CPU per voice (Layer-E)
 *
 * THE ORACLE IS NEVER EDITED. Two copies are loaded from its text at run time:
 *   - PRISTINE: `require`d as is. ITS samples are the ones streamed and
 *     compared (critic review 2026-09-28: the parity target is the untouched
 *     file, not a copy of it).
 *   - INSTRUMENTED: an in-memory SCRATCH copy with seven literal insertions that
 *     report blade events (tryE and scan BLEP corrections, blade-window entries)
 *     with the oversampled tick and member id. Each insertion must match exactly
 *     once; if the oracle text moves, this fails loudly instead of counting
 *     nothing. It is rendered alongside every scenario for its event digest
 *     only, and its samples must equal the pristine ones bit for bit (the NI
 *     line; the check's NONINV row), so the counters provably change no
 *     arithmetic on any scenario.
 * The oracle's git blob hash is streamed too (ORACLE), pinning the parity
 * target by content (ADR-187 item 3).
 *
 * SEEDING. Math.random is replaced by mulberry32(seed) around every oracle
 * instance (composed_engine_check.mjs's convention); the C++ consumes the same
 * stream in the same order.
 *
 * CHAOTIC EXCLUSIONS (ADR-065's evidence rule, inherited by ADR-187 item 6). A
 * scenario may be excluded from the parity verdict ONLY by listing it in
 * CHAOTIC below with its reason, and the exclusion is re-justified every run:
 * the oracle is re-rendered with its inputs perturbed by one ULP (every
 * note-on frequency nudged to the next double up — the size of a last-bit libm
 * disagreement, applied to every voice, since a phrase can release the first
 * note before the sensitive passage), and the JS-vs-JS divergence is sent to
 * the check, which refuses the exclusion unless the JS alone breaks the same
 * gate, comparably.
 *
 * Workers: the renders are independent, so they run on worker threads; the
 * stream is written in scenario order regardless of completion order.
 */
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';
import { availableParallelism } from 'node:os';
import { createHash } from 'node:crypto';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const ORACLE = join(root, 'reference/scalpel/prototype/razor-core.js');
const PRESETS = join(root, 'reference/scalpel/data/presets.json');
const require = createRequire(import.meta.url);

/* ------------------------------------------------------------------ rng */
function mulberry32(a) {
  return function () {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/* --------------------------------------------------------- the two oracles */
// Each insertion: [literal text in the oracle, replacement]. Kinds: 1 edge/base
// BLEP (tryE), 2 carrier BLEP (scan), 3 blade-1 window entry, 4 blade-2 entry.
const INSTRUMENT = [
  ['for (let i = 0; i < n; i++){', 'for (let i = 0; i < n; i++, this.__si++){'],
  ['for (let j = 0; j < os; j++){', 'for (let j = 0; j < os; j++){ RazorCore.__tick = this.__si*os + j;'],
  ['let y = this.stepM(mm,', 'RazorCore.__id = v.__vi*9 + q; let y = this.stepM(mm,'],
  ['if (dd > 0 && dd <= dphi) this.addE(m, E, dd/dphi, c, k, s);',
   'if (dd > 0 && dd <= dphi){ RazorCore.__ev(1); this.addE(m, E, dd/dphi, c, k, s); }'],
  ['if (tau > 0 && tau <= 1) this.addE(m, p0 + tau*dphi, tau, c, k, s);',
   'if (tau > 0 && tau <= 1){ RazorCore.__ev(2); this.addE(m, p0 + tau*dphi, tau, c, k, s); }'],
  ['const dAcc = on ? RazorCore.fmStep(', 'if (on && e1 < e0) RazorCore.__ev(3); const dAcc = on ? RazorCore.fmStep('],
  ['dAcc2 = RazorCore.fmStep(g, bx.ns3, a1, a1 < a0,', 'if (a1 < a0) RazorCore.__ev(4); dAcc2 = RazorCore.fmStep(g, bx.ns3, a1, a1 < a0,'],
];
function loadInstrumented() {
  let src = readFileSync(ORACLE, 'utf8');
  for (const [from, to] of INSTRUMENT) {
    const n = src.split(from).length - 1;
    if (n !== 1) throw new Error(`instrumentation anchor matched ${n} times (want 1): ${from}`);
    src = src.replace(from, to);
  }
  const mod = { exports: {} };
  new Function('module', src)(mod);   // an in-memory scratch copy; the file is untouched
  const RC = mod.exports;
  RC.__ev = kind => {
    const L = RC.__log;
    L.count[kind]++;
    const w = (RC.__id * 8 + kind) >>> 0, t = RC.__tick >>> 0;
    L.h1 = Math.imul(L.h1 ^ t, 16777619) >>> 0; L.h1 = Math.imul(L.h1 ^ w, 16777619) >>> 0;
    L.h2 = (Math.imul(L.h2, 31) + t) >>> 0; L.h2 = (Math.imul(L.h2, 31) + w) >>> 0;
  };
  return RC;
}

/* --------------------------------------------------------------- scenarios */
const SR = 48000, BLK = 128;
const mtof = n => 440 * Math.pow(2, (n - 69) / 12);
// render-goldens.js's register per category (the packet's own choice)
const NOTES = { 'Growls': [33], 'FM sines': [60, 64, 67], 'Movement': [48, 55], 'Leads': [62], 'Pads': [48, 55, 60, 64], 'Interplay': [45], 'Oddities': [52, 59] };
const notesFor = cat => { for (const k in NOTES) if (cat.endsWith(k)) return NOTES[k]; return [45]; };

// The ONLY way out of the parity verdict (see header). Each entry: name -> why.
// Every scenario not listed here is held to parity.
// The ONE entry today: the bench's own "(watch)" patch. Cross-member modulation
// at xm 0.7 feeds each member's phase from its neighbour's last output round the
// ring, a feedback loop that amplifies a last-bit difference; measured
// 2026-09-28, the oracle against itself with inputs one ULP apart diverges to
// max 1e-3..1e-2, 80x or more further than the C++ does (max 1.5e-6..2.6e-5), while
// the blade events still agree exactly. The check re-measures this every run.
const XM_RING = 'cross-member modulation ring (xm 0.7): last-bit feedback amplification (ADR-065 class)';
export const CHAOTIC = {
  'P/Starting points / Cross-mod ring (watch) :: chord': XM_RING,
  'P/Starting points / Cross-mod ring (watch) :: repeat': XM_RING,
  'P/Starting points / Cross-mod ring (watch) :: arp': XM_RING,
};

function presetCmds(params) {
  const cmds = [];
  for (const [k, v] of Object.entries(params)) cmds.push(typeof v === 'string' ? ['sets', k, v] : ['set', k, v]);
  cmds.push(['set', 'gain', 0.35]);   // render-goldens.js's gain override
  cmds.push(['snap']);
  return cmds;
}
const on = (n, vel = 0.85) => ['on', n, mtof(n), vel];
const off = n => ['off', n];
const blocks = k => ['render', BLK, k];

const PHRASES = {
  // a held chord, released, with its tail
  chord: root => {
    const ns = root.length >= 2 ? root : [root[0], root[0] + 7, root[0] + 12];
    return [...ns.map(n => on(n)), blocks(70), ...ns.map(off), blocks(40)];
  },
  // one key: struck, released, struck again, struck AGAIN while held (the
  // oracle's same-note reuse path, fresh = false), released
  repeat: root => { const n = root[0]; return [on(n), blocks(25), off(n), blocks(10), on(n, 0.6), blocks(25), on(n, 1), blocks(25), off(n), blocks(25)]; },
  // eight overlapping notes up the scale: fills the six-voice pool and steals
  arp: root => {
    const n0 = root[0], iv = [0, 3, 7, 10, 12, 15, 19, 22], c = [];
    iv.forEach((d, i) => { c.push(on(n0 + d, 0.5 + 0.06 * i), blocks(10)); if (i >= 2) c.push(off(n0 + iv[i - 2])); });
    c.push(off(n0 + iv[6]), off(n0 + iv[7]), blocks(30));
    return c;
  },
  // mono presets: overlapping keys glide, a release falls back to the held key
  legato: root => {
    const a = root[0], b = a + 5, c = a + 12;
    return [on(a), blocks(25), on(b), blocks(25), off(b), blocks(20), on(c), blocks(15), off(a), off(c), blocks(25)];
  },
  // struck, released until the voice goes inactive (a short R), struck again:
  // the freed slot is reused as a FRESH voice
  restrike: root => { const n = root[0]; return [on(n), blocks(20), off(n), blocks(30), on(n, 0.7), blocks(20), off(n), blocks(20)]; },
  // the bench's retune message ('re') on two held notes
  retune: root => {
    const a = root[0], b = a + 7;
    return [on(a), on(b), blocks(30), ['re', a, mtof(a + 2)], blocks(30), ['re', b, mtof(b - 1)], blocks(20), off(a), off(b), blocks(30)];
  },
  // the bench's panic message mid-chord, then a fresh strike
  panic: root => {
    const ns = [root[0], root[0] + 7, root[0] + 12];
    return [...ns.map(n => on(n)), blocks(30), ['panic'], blocks(30), on(root[0]), blocks(30), off(root[0]), blocks(20)];
  },
  // long enough for a fast modulator's phase to pass the 65536 wrap
  long: root => {
    const ns = [root[0], root[0] + 7, root[0] + 12];
    return [...ns.map(n => on(n)), blocks(250), ...ns.map(off), blocks(20)];
  },
};

// Targeted rows: every blade mode, twin, mirror, frame, rotation, the v1.1
// interplay, DC modes, oversampling and the laws, each isolated on a small patch.
function targeted() {
  const T = [];
  const add = (name, over, phrase = 'chord', root = [45], extra = []) => T.push({ name: 'T/' + name, over, phrase, root, extra });
  const B1 = { N: 3, w: 0.3, k: 5.5, c: 0.6 };
  for (const mode of [0, 1, 2, 3, 4, 5, 6]) add(`mode ${mode}`, { ...B1, mode });
  for (const hot of [0, 1, 2, 3, 4, 6]) add(`hot ${hot}`, { ...B1, mode: 0, hot, morph: 0.8 });
  for (const base of [0, 1, 2, 3, 4]) add(`base ${base}`, { ...B1, base });
  for (const [mode, fmType] of [[1, 0], [1, 1], [2, 0], [2, 1]]) add(`fm mode ${mode} type ${fmType}`, { ...B1, mode, fmType, I: 3, m: 2.3 });
  for (const mshape of [0, 1, 2, 3, 4, 5, 6, 7]) add(`fm mshape ${mshape}`, { ...B1, mode: 2, mshape, I: 2.5 });
  add('fm Hz units', { ...B1, mode: 2, mUnit: 1, mHz: 440 });
  add('lock width', { ...B1, lock: 1 });
  add('lock Hz', { ...B1, lock: 2, kHz: 900 });
  for (const mirror of [1, 2, 3]) add(`mirror ${mirror}`, { ...B1, mirror, hard: 0.3 });
  add('frame swarm', { ...B1, N: 5, frame: 1 });
  add('hard edges', { ...B1, hard: 0.7 });
  add('crush hard (BLEP steps)', { ...B1, mode: 6, hard: 0 });
  add('crush slew', { ...B1, mode: 6, hard: 0.4 });
  add('noise S&H', { ...B1, mode: 3, N: 5 });
  // rotation
  add('rotate +', { ...B1, rotRate: 0.35 });
  add('rotate -', { ...B1, rotRate: -1.2 });
  add('rotate spread', { ...B1, N: 5, rotSpread: 0.4 });
  add('rotate free clock', { ...B1, rotRate: 0.6, rotSync: 0 });
  add('rotate then home', { ...B1, rotRate: 0.9, rotSpread: 0.3 }, 'chord', [45], [[40, ['set', 'rotRate', 0]], [40, ['set', 'rotSpread', 0]]]);
  // blade 2
  const B2 = { ...B1, b2on: 1, mode2: 4, w2: 0.2, k2: 3, c2: 0.35 };
  for (const mode2 of [0, 1, 2, 3, 5, 6]) add(`b2 mode ${mode2}`, { ...B2, mode2 });
  add('b2 own fm', { ...B2, mode2: 2, b2fm: 1, fmType2: 1, mshape2: 5, I2: 3, m2: 1.7 });
  add('b2 sine-to-saw own mod', { ...B2, mode2: 0, hot2: 6, b2fm: 1, mode: 2, mshape: 6, mshape2: 6, morph2: 0.9 });
  add('b2 twin mirror', { ...B2, mirror2: 3, hard2: 0.2 });
  add('b2 inverted twin', { ...B2, mirror2: 2, mirror: 3 });
  add('b2 frame', { ...B2, N: 5, frame2: 1 });
  add('b2 own clock', { ...B2, N: 5, rot2Follow: 0, rotRate2: 0.7, rotRate: 0.2, b2sp: 1, rotSpread2: 0.3 });
  add('b2 own spreads', { ...B2, N: 6, law: 1, b2sp: 1, bspread2: 0.2, kspread2: 3, wspread2: 0.3, dspread2: 0.2, ispread2: 0.3, mspread2: 0.4, kRule2: 2 });
  add('b2 envelope', { ...B2, b2env: 1, benvK2: 0.6, benvW2: 0.4, benvD2: 120 });
  add('b2 Hz lock', { ...B2, lock2: 2, kHz2: 1200 });
  // v1.1 interplay
  for (const [b2mix, b2order] of [[0.5, 0], [0.5, 1], [1, 0], [1, 1]]) add(`serial l${b2mix} o${b2order}`, { ...B2, b2mix, b2order });
  add('serial twins', { ...B2, b2mix: 0.6, mirror: 3, mirror2: 2 });
  add('serial crush over sync', { ...B2, b2mix: 1, mode2: 6, hard2: 0.3 });
  add('collision pitch', { ...B2, colK: 0.9, w: 0.5, w2: 0.5 });
  add('collision bite', { ...B2, colB: 0.9, w: 0.5, w2: 0.5, mode: 2 });
  add('collision upper b1', { ...B2, colK: 0.9, colB: 0.5, b2order: 1, w: 0.5, w2: 0.5 });
  add('interplay sweep', { ...B2, b2mix: 0 }, 'chord', [45], [[30, ['set', 'b2mix', 0.8]], [60, ['set', 'b2mix', 0.2]]]);
  // DC and oversampling
  for (const dcMode of [0, 1, 2]) add(`dc ${dcMode}`, { ...B1, dcMode, c: 0.2 });
  for (const os of [1, 2, 4]) add(`os ${os}`, { ...B2, os });
  add('aa off', { ...B1, aa: 0 });
  // swarm laws and cut rules
  const SPR = { N: 6, bspread: 0.15, kspread: 4, wspread: 0.3, dspread: 0.2, mspread: 0.3, ispread: 0.2, rotSpread: 0.2 };
  for (const law of [0, 1, 2, 3, 4]) add(`law ${law}`, { ...B1, ...SPR, law });
  for (const kRule of [1, 2, 3, 4, 5, 6, 7, 8]) add(`rule ${kRule}`, { ...B1, N: 6, kRule, kRuleAmt: 0.8 });
  add('rule 9 custom', { ...B1, N: 7, kRule: 9, kCustom: '1; 7/6 ,7/4 21/8 bogus 0 -3 3/0' });
  add('rule snapped', { ...B1, N: 6, kRule: 4, kq: 1 });
  add('spread snapped', { ...B1, N: 6, kspread: 5, kq: 1 });
  // Math.round's negative half: out of the UI's 0..24 range, reached only by a
  // negative Cut spread. It is the one input where std::round and JS disagree.
  add('spread snapped -half', { ...B1, N: 2, kspread: -2.5, kq: 1 });
  add('blade envelope', { ...B1, benvK: 0.7, benvW: 0.5, benvD: 90, benvVel: 1 });
  add('cross-mod', { ...B1, N: 5, xm: 0.4 });
  add('feedback', { ...B1, N: 1, fb: 0.5 });
  add('splay K<0', { ...B1, N: 6, K: -0.8 });
  add('strong lock', { ...B1, N: 7, K: 1, detune: 40 });
  add('cycles coupling', { ...B1, N: 5, cScale: 1 });
  add('phase random', { ...B1, N: 5, phaseMode: 0 });
  add('phase zero', { ...B1, N: 5, phaseMode: 1 });
  add('pan order', { ...B1, N: 7, panOrder: 1, width: 1 });
  add('bend', { ...B1, bend: 3 });
  // parameter motion: the smoothers and the per-sample glides
  add('sweep k w c', { ...B1 }, 'chord', [45], [[20, ['set', 'k', 11]], [40, ['set', 'w', 0.6]], [60, ['set', 'c', 0.1]], [70, ['set', 'depth', 0.4]]]);
  add('sweep swarm', { ...B1, N: 5 }, 'chord', [45], [[20, ['set', 'K', -0.3]], [40, ['set', 'detune', 45]], [60, ['set', 'N', 3]]]);
  // voice management
  add('sr 44100', { ...B2 }, 'arp');
  add('poly 2 steals', { ...B1, poly: 2 }, 'arp');
  add('mono retrig', { ...B1, polyMode: 1 }, 'legato');
  add('legato glide always', { ...B1, polyMode: 2, glideAlways: 1, glide: 120 }, 'legato');
  // Rework rows (critic M2, 2026-09-28): paths the first 111 rows never reached.
  add('restrike after full release', { ...B1, R: 5 }, 'restrike');
  add('blade 1 off (w 0)', { ...B1, w: 0 });
  add('blade 2 off (w2 0)', { ...B2, w2: 0 });
  add('collision swept off', { ...B2, colK: 0.9, colB: 0.5, w: 0.5, w2: 0.5 }, 'chord', [45], [[40, ['set', 'colK', 0]], [40, ['set', 'colB', 0]]]);
  add('b2 own clock at rest', { ...B2, N: 5, rot2Follow: 0, rotRate2: 0, rotRate: 0.3 });
  add('b2 own clock homing', { ...B2, N: 5, rot2Follow: 0, rotRate2: 0.8, rotRate: 0.2, b2sp: 1, rotSpread2: 0.3 }, 'chord', [45],
      [[35, ['set', 'rotRate2', 0]], [35, ['set', 'rotSpread2', 0]]]);
  add('mono glide 1 ms', { ...B1, polyMode: 2, glide: 1 }, 'legato');
  add('settle skipped (K 0)', { ...B1, N: 5, K: 0 });
  add('gate mode 6 collision', { ...B2, mode: 6, mode2: 6, colK: 0.9, colB: 0.5, w: 0.5, w2: 0.5 });
  add('b2 lock 1 FM collision', { ...B2, lock2: 1, mode2: 2, b2fm: 1, fmType2: 1, I2: 2, colK: 0.9, colB: 0.6, w: 0.5, w2: 0.5 });
  add('retune message', { ...B1 }, 'retune');
  add('panic message', { ...B1 }, 'panic');
  add('polyMode switch mid-note', { ...B1 }, 'chord', [45], [[30, ['set', 'polyMode', 2]], [50, on(57)]]);
  add('modX wrap', { ...B2, mode: 2, m: 64, I: 1, b2fm: 1, mode2: 2, m2: 64, I2: 1 }, 'long', [100]);
  return T;
}

function buildScenarios() {
  const presets = JSON.parse(readFileSync(PRESETS, 'utf8')).presets;
  const out = [];
  for (const pr of presets) {
    const tag = `${pr.category} / ${pr.name}`, root = notesFor(pr.category);
    const phrases = pr.params.polyMode ? ['chord', 'repeat', 'arp', 'legato'] : ['chord', 'repeat', 'arp'];
    for (const ph of phrases) out.push({ name: `P/${tag} :: ${ph}`, sr: SR, seed: 0xC0FFEE, cmds: [...presetCmds(pr.params), ...PHRASES[ph](root)] });
  }
  for (const t of targeted()) {
    const cmds = [];
    for (const [k, v] of Object.entries(t.over)) cmds.push(typeof v === 'string' ? ['sets', k, v] : ['set', k, v]);
    cmds.push(['snap']);
    let body = PHRASES[t.phrase](t.root);
    if (t.extra.length) {
      // splice parameter moves in at block offsets (the render commands are split)
      const flat = [];
      for (const c of body) if (c[0] === 'render') for (let i = 0; i < c[2]; i++) flat.push(['render', BLK, 1]); else flat.push(c);
      let blk = 0; const res = [];
      for (const c of flat) {
        if (c[0] === 'render') { for (const [at, cmd] of t.extra) if (at === blk) res.push(cmd); blk++; }
        res.push(c);
      }
      body = res;
    }
    out.push({ name: `${t.name} :: ${t.phrase}`, sr: t.name === 'T/sr 44100' ? 44100 : SR, seed: 0xB332, cmds: [...cmds, ...body] });
  }
  return out;
}

/* ----------------------------------------------------------------- render */
function nextUp(x) {   // the next double above x (x > 0): a one-ULP perturbation
  const f = new Float64Array([x]), b = new BigUint64Array(f.buffer); b[0] += 1n; return f[0];
}
function renderWith(RC, sc, { instrument = false, perturb = false } = {}) {
  const SAVED = Math.random;
  Math.random = mulberry32(sc.seed >>> 0);
  const c = new RC(sc.sr);
  if (instrument) { c.__si = 0; c.voices.forEach((v, i) => { v.__vi = i; }); RC.__log = { count: [0, 0, 0, 0, 0], h1: 2166136261, h2: 0 }; }
  let total = 0;
  for (const cm of sc.cmds) if (cm[0] === 'render') total += cm[1] * cm[2];
  const buf = new Float64Array(total * 2);
  let pos = 0;
  for (const cm of sc.cmds) {
    switch (cm[0]) {
      case 'set': c.set({ [cm[1]]: cm[2] }); break;
      case 'sets': c.set({ [cm[1]]: cm[2] }); break;
      case 'snap': Object.assign(c.s, c.t); break;
      case 'on': c.noteOn(cm[1], perturb ? nextUp(cm[2]) : cm[2], cm[3]); break;
      case 'off': c.noteOff(cm[1]); break;
      case 're': c.msg({ t: 're', note: cm[1], freq: perturb ? nextUp(cm[2]) : cm[2] }); break;   // the bench's retune message
      case 'panic': c.msg({ t: 'panic' }); break;
      case 'render': {
        const L = new Float64Array(cm[1]), R = new Float64Array(cm[1]);
        for (let b = 0; b < cm[2]; b++) { c.render(L, R); for (let i = 0; i < cm[1]; i++) { buf[pos++] = L[i]; buf[pos++] = R[i]; } }
        break;
      }
      default: throw new Error('bad command ' + cm[0]);
    }
  }
  Math.random = SAVED;
  return { buf, log: instrument ? RC.__log : null };
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
// The samples streamed are the PRISTINE oracle's; the instrumented copy is
// rendered alongside only for its event digest, and its samples must equal the
// pristine ones bit for bit (NI 1) on every scenario, or the counters are not
// provably non-invasive.
function job(RCi, RCp, i, sc) {
  const { buf } = renderWith(RCp, sc);
  const ins = renderWith(RCi, sc, { instrument: true }), log = ins.log;
  let same = ins.buf.length === buf.length;
  for (let k = 0; same && k < buf.length; k++) if (!Object.is(buf[k], ins.buf[k])) same = false;
  let head = scriptText(i, sc);
  head += `EV ${log.count[1]} ${log.count[2]} ${log.count[3]} ${log.count[4]} ${log.h1} ${log.h2}\nNI ${same ? 1 : 0}\n`;
  if (CHAOTIC[sc.name]) {
    const p = renderWith(RCp, sc, { perturb: true });
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
  const RCi = loadInstrumented(), RCp = require(ORACLE);
  const all = buildScenarios();
  parentPort.on('message', i => {
    if (i < 0) { process.exit(0); }
    const r = job(RCi, RCp, i, all[i]);
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
  await write(`H2SCALPEL 1 ${idx.length}\n`);

  // libm probes (Layer-E, printed by the check, never judged): V8's value of
  // each transcendental the oracle calls, on inputs in the ranges it calls them
  // with, so the check can report how often the platform libm agrees to the bit.
  {
    const r = mulberry32(0x11B3), U = (a, b) => a + (b - a) * r(), n = 20000;
    const probes = {
      sin: () => { const x = r() < 0.5 ? U(-10, 10) : Math.floor(U(0, 70000)) * 127.1 + 311.7; return [x, 0, Math.sin(x)]; },   // phases; hash() arguments
      cos: () => { const x = U(-10, 10); return [x, 0, Math.cos(x)]; },
      exp: () => { const x = U(-60, 5); return [x, 0, Math.exp(x)]; },
      log: () => { const x = r() < 0.5 ? U(1e-9, 1) : U(1, 10); return [x, 0, Math.log(x)]; },
      pow: () => { const b = U(0.01, 4), e = U(-8, 8); return [b, e, Math.pow(b, e)]; },
      // Math.pow(2, x): the core's constant-base-2 sites (spreads, envelopes,
      // cents) which clang lowers to exp2 (non-integral x) or ldexp; the check
      // evaluates this row with std::exp2, i.e. as the core is compiled.
      pow2: () => { const e = U(-8, 8); return [2, e, Math.pow(2, e)]; },
      atan2: () => { const y = U(-1, 1), x = U(-1, 1); return [y, x, Math.atan2(y, x)]; },
      asin: () => { const x = U(0, 0.995); return [x, 0, Math.asin(x)]; },
      tanh: () => { const x = U(-4, 4); return [x, 0, Math.tanh(x)]; },
      sqrt: () => { const x = U(0, 10); return [x, 0, Math.sqrt(x)]; },
      hypot: () => { const x = U(-1, 1), y = U(-1, 1); return [x, y, Math.hypot(x, y)]; },
    };
    for (const [fn, gen] of Object.entries(probes)) {
      const v = new Float64Array(n * 3);
      for (let i = 0; i < n; i++) v.set(gen(), 3 * i);
      await write(`LIBM ${fn} ${n}\n`);
      await write(Buffer.from(v.buffer));
    }
  }

  // The parity target, pinned by content: the oracle file's git blob hash
  // (ADR-187 item 3). The check prints it; h2/README.md's status row pins it and
  // tools/h2_rules_check.py fails when the two disagree.
  {
    const bytes = readFileSync(ORACLE);
    const blob = createHash('sha1').update(`blob ${bytes.length}\0`).update(bytes).digest('hex');
    await write(`ORACLE reference/scalpel/prototype/razor-core.js ${blob}\n`);
  }

  const W = Math.max(1, Math.min(Number(arg('--workers', availableParallelism() - 1)), idx.length));
  const done = new Map();
  let next = 0, emit = 0, wake = null;
  const workers = [];
  const feed = w => { if (next < idx.length) w.postMessage(idx[next++]); else w.postMessage(-1); };
  // Completion is "every scenario WRITTEN", never "every worker exited": the
  // workers finish long before a slow reader has drained the stream, and an END
  // written early truncates the check (the first version did exactly that).
  let failed = null;
  for (let k = 0; k < W; k++) {
    const w = new Worker(fileURLToPath(import.meta.url), { argv });
    w.on('message', m => { done.set(m.i, m); feed(w); if (wake) { const f = wake; wake = null; f(); } });
    w.on('error', e => { failed = e; if (wake) { const f = wake; wake = null; f(); } });
    workers.push(w); feed(w);
  }
  while (emit < idx.length) {
    if (failed) throw failed;
    const i = idx[emit];
    if (!done.has(i)) { await new Promise(r => { wake = r; }); continue; }
    const m = done.get(i); done.delete(i);
    await write(m.head);
    await write(Buffer.from(m.buf.buffer, m.buf.byteOffset, m.buf.byteLength));
    emit++;
    if (emit % 25 === 0) process.stderr.write(`h2_scalpel_render: ${emit}/${idx.length} scenarios streamed\n`);
  }
  await write(`END ${idx.length}\n`);
  process.stderr.write(`h2_scalpel_render: ${idx.length} scenarios, ${W} workers, ${(Number(process.hrtime.bigint() - t0) / 1e9).toFixed(1)} s\n`);
}

/* ------------------------------------------------------------ Layer-E: CPU */
// JS cost per voice as a fraction of real time, on the classes the brief names:
// the heavy one (Crushed bells: N 6 plus two blades) and a light one (Quarter
// sync: N 1, one blade). One voice held for BENCH_S seconds at 48 kHz, 2x
// oversampling (the oracle's default). A calibration loop of known work is timed
// alongside so a reader can tell a slow machine from a slow engine (B236). The
// C++ half is tools/measure_h2_scalpel.cpp, which replays the same scripts.
function benchScenarios() {
  const presets = JSON.parse(readFileSync(PRESETS, 'utf8')).presets;
  const BENCH_S = 4, nb = Math.round(BENCH_S * SR / BLK);
  return ['Crushed bells', 'Quarter sync'].map(name => {
    const pr = presets.find(p => p.name === name);
    return { name: `BENCH/${name}`, sr: SR, seed: 0xB332, cmds: [...presetCmds(pr.params), on(57), blocks(nb)] };
  });
}
function bench() {
  const RC = require(ORACLE), sc = benchScenarios();
  if (argv.includes('--emit')) {   // the scripts, for the C++ half
    process.stdout.write(`H2SCALPEL 1 ${sc.length}\n`);
    for (const [i, s] of sc.entries()) {
      const { buf } = renderWith(RC, s);
      process.stdout.write(scriptText(i, s) + `EV 0 0 0 0 0 0\nDATA ${buf.length / 2}\n`);
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
    renderWith(RC, s);   // warm-up (JIT)
    const best = [];
    for (let r = 0; r < 3; r++) { const t = process.hrtime.bigint(); renderWith(RC, s); best.push(Number(process.hrtime.bigint() - t) / 1e9); }
    const secs = Math.min(...best), audio = s.cmds.filter(c => c[0] === 'render').reduce((a, c) => a + c[1] * c[2], 0) / s.sr;
    console.log(`${s.name.padEnd(22)} JS  ${(100 * secs / audio).toFixed(2)} % of real time per voice (${secs.toFixed(3)} s for ${audio.toFixed(2)} s, best of 3)`);
  }
}
