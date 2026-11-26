import './sandbox_guard.mjs';
/*
 * fx_routing_check.mjs — the FX routing ladder keeps its wires separated, and its wire model keeps GUI-75's laws (GB41).
 * WIRED: ./verify fast (wired in the PR that creates it, GUI-75, under ADR-180 §1).
 *
 * WHY THIS EXISTS. The human asked for wires "tidy and rational and separated" (2026-11-25) and ruled the ladder from
 * docs/design/fx-routing-lab.html (GUI-75). The lab proved the layout with a self-check on its own page; GB41's
 * acceptance asks the MASTER to hold the same measures as its own check, so a later edit to the bundle that lets two
 * wires share a port, run along each other or bury a chip turns CI red instead of reaching the human's eye.
 *
 * WHAT IT RUNS. The GUI-75 block of the VENDORED bundle (docs/design/arp-face/bundle.js, the copy the shell loads;
 * design_chain_check holds it equal to the master's), evaluated alone: FxWires is pure (no DOM, no clock), and
 * FxWires.ladder is the geometry the view draws, so this measures what is on screen, not a re-derivation of it.
 *
 * WHAT IS ASSERTED, on the master preview's sample patch (ten boxes, sixteen wires, two of them return wires), at every
 * corner and at Heard for three pad positions, with Prevent feedback off and on, at host widths 333, 600, 693 and 960:
 *   geometry  (and on the shell's crowded house default: six inputs into one module, then seeded Randomize draws)
 *             no port shared by two wires; no stretch two wires run along together; every forward point above the row
 *             and every return point below it; no level chip on another; distinct ports at least 3 px apart; no arrowhead over a neighbouring head or cord
 *   model     the one writer: a level of 0 (or less, or null) removes the wire at that corner only and keeps its other
 *             corners; a wire absent at every corner leaves the model; Heard is never written; a level over 1 is held at
 *             1; can() refuses a self wire, a wire from Out and a wire into an input
 *   heard     the wire exists where the dominant corner holds it, and its level blends over the corners that hold it
 *   feedback  with Prevent feedback on, a loop's back edge (echo -> weir at C) is held and a return wire that closes no
 *             loop is not; off, nothing is held
 *   randomize a seeded draw is repeatable, forward only, at 100 %, and leaves the other corners alone; a copy matches
 * Must-fail controls, each run every time (a control that reads clean means the instrument is blind, and the check is red):
 *   a pitch squeezed to 6 px must clash chips; every wire forced to one start port must count shared ports; a wire moved
 *   to the wrong side of the row must count band points; a duplicated segment must count an overlap; a level of 0.001
 *   must keep the wire (the removal is about 0, not about small); two seeds must draw different corners.
 */
import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const SRC = readFileSync(resolve(ROOT, 'docs/design/arp-face/bundle.js'), 'utf8');
const START = SRC.indexOf('/* ======== GUI-75: FX routing');
const END_MARK = 'H.FxWires = FxWires; H.FxRouting = FxRouting;\n})();';
const END = SRC.indexOf(END_MARK, START);
const bad = [], notes = [];
const ok = (c, m) => { if (!c) bad.push(m); };
if (START < 0 || END < 0) { console.error('fx_routing_check: RED — the GUI-75 block is not in the vendored bundle'); process.exit(1); }
const win = { Horde: {} };
new Function('window', SRC.slice(START, END + END_MARK.length))(win);
const W = win.Horde.FxWires;

// ---- the master preview's sample patch (components/FxRouting/preview.html) ----
const M = (id, name, short, style) => ({ id, kind: 'mod', name, short, style });
const NODES = [{ id: 'osc1', kind: 'in', name: 'Osc 1', short: 'O1', style: 'lane' }, { id: 'osc2', kind: 'in', name: 'Osc 2', short: 'O2', style: 'lane' },
  { id: 'sub', kind: 'in', name: 'Sub', short: 'Su', style: 'lane2' }, { id: 'noise', kind: 'in', name: 'Noise', short: 'Nz', style: 'lane2' },
  M('bulwark', 'Bulwark', 'Bw', 'bulwark'), M('weir', 'Weir', 'Wr', 'filter'), M('sluice', 'Sluice', 'Sl', 'sluice'), M('echo', 'Echo', 'Ec', 'echo'),
  M('scape', 'Scape', 'Sc', 'scape'), { id: 'out', kind: 'out', name: 'Out', short: 'Out' }];
const all = v => [v, v, v, v];
const WIRES = [{ u: 'osc1', v: 'weir', lv: [1, .8, 1, .6] }, { u: 'osc2', v: 'weir', lv: [.7, .7, null, .5] }, { u: 'osc2', v: 'sluice', lv: [null, null, .6, .4] },
  { u: 'sub', v: 'bulwark', lv: all(.8) }, { u: 'noise', v: 'echo', lv: [.35, .5, null, .2] }, { u: 'bulwark', v: 'out', lv: all(1) }, { u: 'weir', v: 'echo', lv: [.4, .25, .6, .3] },
  { u: 'weir', v: 'scape', lv: [.5, .6, .3, .8] }, { u: 'weir', v: 'out', lv: [.8, .9, .7, .6] }, { u: 'sluice', v: 'scape', lv: [null, null, .5, .45] }, { u: 'sluice', v: 'out', lv: [null, null, .5, .5] },
  { u: 'echo', v: 'scape', lv: [.5, .4, .3, .5] }, { u: 'echo', v: 'out', lv: all(.6) }, { u: 'scape', v: 'out', lv: all(1) },
  { u: 'echo', v: 'weir', lv: [null, null, .2, .15] }, { u: 'scape', v: 'echo', lv: [null, .15, null, null] }];
const fresh = () => new W({ nodes: NODES, wires: WIRES });

// ---- the crowded patch: the shell's house default (six inputs into the first module, the modules in order, the last to Out), then seeded Randomize draws at B to D ----
const mulberry = seed => () => { seed |= 0; seed = seed + 0x6D2B79F5 | 0; let t = Math.imul(seed ^ seed >>> 15, 1 | seed); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; };
const crowded = () => { const ins = ['osc1', 'osc2', 'sub', 'noise', 'flt1', 'flt2'], mods = ['echo', 'scape', 'shriek', 'bulwark'], ids = mods.concat(['out']);
  const m = new W({ nodes: ins.map(id => ({ id, kind: 'in', name: id, short: id, style: 'lane' })).concat(mods.map(id => M(id, id, id, id)), [{ id: 'out', kind: 'out', name: 'Out', short: 'Out' }]),
    wires: ins.map(u => ({ u, v: 'echo', lv: all(1) })).concat(mods.map((u, i) => ({ u, v: ids[i + 1], lv: all(1) }))) });
  [1, 2, 3].forEach(c => m.drawCorner(c, mulberry(40 + c), c / 3)); return m; };

// ---- geometry, as drawn ----
const PADS = [[.3, .35], [.8, .2], [.5, .9]];
let views = 0, worstGap = Infinity, maxCross = 0;
for (const [pname, model] of [['sample', fresh()], ['crowded', crowded()]]) for (const width of [333, 600, 693, 960]) for (const prevent of [false, true]) {
  const shows = [0, 1, 2, 3].map(c => ({ c, wt: W.weights(.3, .35), tag: 'ABCD'[c] })).concat(PADS.map(p => ({ c: W.HEARD, wt: W.weights(p[0], p[1]), tag: 'Heard@' + p })));
  for (const s of shows) {
    const L = W.ladder(model, model.shown(s.c, s.wt, prevent), width), m = W.measure(L.lay, L.P), where = `${pname} ${s.tag} at ${width} px, prevent ${prevent ? 'on' : 'off'}`;
    views++; worstGap = Math.min(worstGap, m.minGap); maxCross = Math.max(maxCross, m.cross);
    ok(m.shared === 0, `${where}: ${m.shared} shared port(s)`);
    ok(m.overlap === 0, `${where}: ${m.overlap} overlapping stretch(es)`);
    ok(m.band === 0, `${where}: ${m.band} point(s) on the wrong side of the row`);
    ok(L.clash === 0, `${where}: ${L.clash} level chip(s) on another`);
    ok(m.minGap >= 3, `${where}: two ports ${m.minGap.toFixed(2)} px apart (< 3)`);
    ok(W.heads(L.lay) === 0, `${where}: ${W.heads(L.lay)} arrowhead(s) cover a neighbouring head or cord`);
    ok(L.width >= NODES.length * W.MIN_COL + 16, `${where}: the ladder is ${L.width} px, under its minimum`);
  }
}

// ---- controls on the instruments (each must read dirty) ----
const model = fresh();
{ const m = crowded(), L = W.ladder(m, m.shown(0, W.weights(.3, .35), false), 600); L.lay.wires.forEach(x => { x.head = 2.5 + 2 * x.L; });
  ok(W.heads(L.lay) > 0, 'CONTROL: full-size arrowheads on the crowded patch covered nothing (the head instrument is blind)'); }
{ const L = W.ladder(model, model.shown(3, W.weights(.3, .35), false), 600, { pitch: 6 });
  ok(L.clash > 0, 'CONTROL: a 6 px pitch placed every chip clear (the clash instrument is blind)'); }
{ const L = W.ladder(model, model.shown(0, W.weights(.3, .35), false), 600), P = L.P.map(Q => [L.P[0][0]].concat(Q.slice(1)));
  ok(W.measure(L.lay, P).shared > 0, 'CONTROL: one start port for every wire counted no shared port'); }
{ const L = W.ladder(model, model.shown(0, W.weights(.3, .35), false), 600), x = L.lay.wires.find(w => w.dir === 'f');
  x.pts = x.pts.map(p => [p[0], -p[1]]); ok(W.measure(L.lay, L.P).band > 0, 'CONTROL: a forward wire moved below the row counted no band point'); }
{ const L = W.ladder(model, model.shown(0, W.weights(.3, .35), false), 600), P = L.P.concat([L.P[0].slice()]);
  ok(W.measure(L.lay, P).overlap > 0, 'CONTROL: a duplicated wire counted no overlap'); }

// ---- the model's laws ----
{ const m = fresh(), w = () => m.find('osc1', 'weir');
  ok(m.setLevel('osc1', 'weir', 0, 0) && w() && w().lv[0] === null && w().lv[1] === .8, 'a level of 0 at A did not remove the wire at A only');
  ok(m.setLevel('osc1', 'weir', 1, -.2) && w().lv[1] === null, 'a negative level did not remove the wire at B');
  m.setLevel('osc1', 'weir', 2, null); m.setLevel('osc1', 'weir', 3, 0);
  ok(!w(), 'a wire absent at every corner stayed in the model');
  ok(!m.setLevel('osc1', 'weir', W.HEARD, .5) && !w(), 'Heard was written');
  ok(m.setLevel('osc1', 'weir', 2, W.DEFAULT) && w().lv[2] === 1 && w().lv.filter(x => x !== null).length === 1, 'a make at C did not land at 100 % at C only');
  m.setLevel('osc1', 'weir', 2, 1.7); ok(w().lv[2] === 1, 'a level over 1 was not held at 1');
  m.setLevel('osc1', 'weir', 2, .001); ok(w() && w().lv[2] === .001, 'CONTROL: a level of 0.001 removed the wire (removal must be about 0, not small)');
  ok(m.can('weir', 'weir') && m.can('out', 'weir') && m.can('weir', 'osc2') && m.can('weir', 'echo') === '', 'can() refused or allowed the wrong wires');
  ok(!m.setLevel('weir', 'osc2', 0, .5) && !m.find('weir', 'osc2'), 'a wire into an input was written'); }
{ const m = fresh(), w = m.find('osc2', 'weir');   // lv [.7, .7, null, .5]
  ok(m.heard(w, W.weights(.1, .1)) !== null && Math.abs(m.heard(w, W.weights(.1, .1)) - .7) < 1e-9 + .03, 'Heard near A is not about A\'s level');
  ok(m.heard(w, W.weights(.1, .9)) === null, 'Heard near C keeps a wire C does not hold');
  const wt = W.weights(.6, .3), e = (wt[0] * .7 + wt[1] * .7 + wt[3] * .5) / (wt[0] + wt[1] + wt[3]);
  ok(Math.abs(m.heard(w, wt) - e) < 1e-12, 'Heard does not blend over the corners that hold the wire'); }
{ const m = fresh(), held = (c, p) => m.shown(c, W.weights(.3, .35), p).filter(x => x.held).map(x => x.w.u + '>' + x.w.v).join(',');
  ok(held(2, true) === 'echo>weir', `prevent on at C holds "${held(2, true)}", not echo>weir`);
  ok(held(2, false) === '', 'prevent off held a wire');
  ok(held(1, true) === 'scape>echo', `prevent on at B holds "${held(1, true)}", not scape>echo`);
  m.setLevel('echo', 'scape', 1, null);   // the loop's forward half is gone at B, so the return closes nothing
  ok(held(1, true) === '', 'CONTROL: a return wire that closes no loop was held'); }

// ---- Randomize's draw and copy (carried over from FxPatch) ----
{ const rng = seed => () => { seed |= 0; seed = seed + 0x6D2B79F5 | 0; let t = Math.imul(seed ^ seed >>> 15, 1 | seed); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; };   // mulberry32
  const snap = (m, c) => m.wires.filter(w => w.lv[c] !== null).map(w => w.u + '>' + w.v + ':' + w.lv[c]).sort().join(',');
  const a = fresh(), b = fresh(), d = fresh(), keepB = snap(a, 1);
  const n = a.drawCorner(0, rng(7), .6); b.drawCorner(0, rng(7), .6); d.drawCorner(0, rng(8), .6);
  ok(n > 0 && snap(a, 0) === snap(b, 0), 'a seeded draw is not repeatable');
  ok(snap(a, 0) !== snap(d, 0), 'CONTROL: two seeds drew the same corner (the draw ignores its generator)');
  ok(a.wires.filter(w => w.lv[0] !== null).every(w => !a.isReturn(w.u, w.v) && w.lv[0] === 1), 'a draw made a return wire or a level other than 100 %');
  ok(snap(a, 1) === keepB, 'a draw at A changed corner B');
  ok(a.copyCorner(0, 2) > 0 && snap(a, 2) === snap(a, 0), 'a copy from A to C does not match A');
  ok(a.copyCorner(1, 1) === 0, 'a copy onto its own corner changed something'); }

if (bad.length) { console.error('fx_routing_check: RED\n  ' + bad.join('\n  ')); process.exit(1); }
console.log(`fx_routing_check: GREEN — ${views} ladder views (2 patches × 4 corners and Heard at 3 pads × prevent off/on × 4 widths): 0 shared ports, 0 overlaps, 0 band points, 0 chip clashes, closest ports ${worstGap.toFixed(2)} px, crossings up to ${maxCross} (interleaving spans); the one writer, Heard, held back edges, Randomize's draw and copy hold; 8 controls fired`);
