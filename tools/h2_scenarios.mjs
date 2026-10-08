/*
 * h2_scenarios.mjs — the scenario language and the blade rows shared by horde 2's
 * two parity renderers: tools/h2_scalpel_render.mjs (razor_core.h against the
 * blade oracle, B332 phase 1a) and tools/h2_engine_render.mjs (the composed engine
 * against the composed JS, B385). One copy, so the blade rows the composed engine
 * re-runs are exactly the ones phase 1a proved the blade port on.
 *
 * A scenario is { name, sr, seed, cmds }. Commands:
 *   ['set', key, number] | ['sets', key, string] | ['snap'] | ['on', note, freq, vel]
 *   | ['off', note] | ['re', note, freq] | ['panic'] | ['render', blockSize, count]
 * and, for the composed engine only, ['cap', n] | ['capPolicy', n].
 * Also the instrumented scratch copy of the blade oracle both renderers count
 * blade events with (loadInstrumented): the oracle file is never edited.
 */
import './labharness/sandbox_guard.mjs';   // FIRST import: lab code runs under the permission model (B446 W3c)
import { readFileSync } from 'node:fs';

export const SR = 48000, BLK = 128;
export const mtof = n => 440 * Math.pow(2, (n - 69) / 12);

export function mulberry32(a) {
  return function () {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/* ------------------------------------------- the instrumented blade oracle */
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
// Applies [from, to] insertions to a source text; each anchor must match exactly
// once, so a moved oracle fails loudly instead of counting nothing.
export function insertAll(src, list, what) {
  for (const [from, to] of list) {
    const n = src.split(from).length - 1;
    if (n !== 1) throw new Error(`${what}: instrumentation anchor matched ${n} times (want 1): ${from}`);
    src = src.replace(from, to);
  }
  return src;
}
export function loadInstrumented(oraclePath) {
  const src = insertAll(readFileSync(oraclePath, 'utf8'), INSTRUMENT, 'razor-core.js');
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

/* --------------------------------------------------------------- phrases */
// render-goldens.js's register per category (the packet's own choice)
const NOTES = { 'Growls': [33], 'FM sines': [60, 64, 67], 'Movement': [48, 55], 'Leads': [62], 'Pads': [48, 55, 60, 64], 'Interplay': [45], 'Oddities': [52, 59] };
export const notesFor = cat => { for (const k in NOTES) if (cat.endsWith(k)) return NOTES[k]; return [45]; };

export function presetCmds(params) {
  const cmds = [];
  for (const [k, v] of Object.entries(params)) cmds.push(typeof v === 'string' ? ['sets', k, v] : ['set', k, v]);
  cmds.push(['set', 'gain', 0.35]);   // render-goldens.js's gain override
  cmds.push(['snap']);
  return cmds;
}
export const on = (n, vel = 0.85) => ['on', n, mtof(n), vel];
export const off = n => ['off', n];
export const blocks = k => ['render', BLK, k];

export const PHRASES = {
  // a held chord, released, with its tail
  chord: root => {
    const ns = root.length >= 2 ? root : [root[0], root[0] + 7, root[0] + 12];
    return [...ns.map(n => on(n)), blocks(70), ...ns.map(off), blocks(40)];
  },
  // one key: struck, released, struck again, struck AGAIN while held (the blade
  // oracle's same-note reuse path; the composed engine takes a new voice), released
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

// Splices parameter moves into a phrase at block offsets (the render commands are split).
export function spliceAt(body, extra) {
  if (!extra.length) return body;
  const flat = [];
  for (const c of body) if (c[0] === 'render') for (let i = 0; i < c[2]; i++) flat.push(['render', c[1], 1]); else flat.push(c);
  let blk = 0; const res = [];
  for (const c of flat) {
    if (c[0] === 'render') { for (const [at, cmd] of extra) if (at === blk) res.push(cmd); blk++; }
    res.push(c);
  }
  return res;
}

/* --------------------------------------------------------- the blade rows */
// Every blade mode, twin, mirror, frame, rotation, the v1.1 interplay, DC modes,
// oversampling and the laws, each isolated on a small patch (B332 phase 1a, with
// the critic's M2 rework rows). Each: { name, over, phrase, root, extra }.
export function bladeRows() {
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

// A blade row as a scenario: its patch, snapped, then its phrase with its moves.
export function rowScenario(t, seed) {
  const cmds = [];
  for (const [k, v] of Object.entries(t.over)) cmds.push(typeof v === 'string' ? ['sets', k, v] : ['set', k, v]);
  cmds.push(['snap']);
  const body = spliceAt(PHRASES[t.phrase](t.root), t.extra);
  return { name: `${t.name} :: ${t.phrase}`, sr: t.name === 'T/sr 44100' ? 44100 : SR, seed, cmds: [...cmds, ...body] };
}
