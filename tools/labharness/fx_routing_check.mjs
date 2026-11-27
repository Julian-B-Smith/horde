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
 *   GUI-76   the voice boundary refuses a module into a filter; ADR-414's "either order, no loop" refuses the second filter direction at a
 *            corner; a return on the summed side is late (z⁻¹) and Filter 2 into Filter 1 is not; a bypassed slot on the graph passes its wires on (GUI-76 A2); the Mix console's flow (fromGraph) draws
 *            the graph live with the pad, without return wires
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
// GUI-76 r7: the Mix console's flow block (ChainConsole), found around its `var flow = {` and evaluated alone, so flow.fromGraph is held to the graph too
const FI = SRC.indexOf('  var flow = {'), FA = SRC.lastIndexOf('(function (H) {', FI), FZ = SRC.indexOf('})(window.Horde);', FI) + '})(window.Horde);'.length;
if (FI < 0 || FA < 0 || FZ < FI) { console.error('fx_routing_check: RED — the console flow block is not in the vendored bundle'); process.exit(1); }
new Function('window', SRC.slice(FA, FZ))(win);
const FLOW = win.Horde.ChainConsole.flow;

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
// ---- GUI-76: one graph from the sources to Out (the master preview's), filters per voice; modules carry their console slot ----
const GNODES = [{ id: 'osc1', kind: 'in', name: 'Osc 1', short: 'O1', style: 'lane' }, { id: 'osc2', kind: 'in', name: 'Osc 2', short: 'O2', style: 'lane' },
  { id: 'sub', kind: 'in', name: 'Sub', short: 'Su', style: 'lane2' }, { id: 'noise', kind: 'in', name: 'Noise', short: 'Nz', style: 'lane2' },
  { id: 'f1', kind: 'mod', name: 'Filter 1', short: 'F1', style: 'filter', voice: true, io: ['f1i', 'f1o'] }, { id: 'f2', kind: 'mod', name: 'Filter 2', short: 'F2', style: 'filter', voice: true, io: ['f2i', 'f2o'] },
  Object.assign(M('echo', 'Echo', 'Ec', 'echo'), { io: ['x1i', 'x1o'] }), Object.assign(M('scape', 'Scape', 'Sc', 'scape'), { io: ['x2i', 'x2o'] }),
  Object.assign(M('shriek', 'Shriek', 'Sh', 'shriek'), { io: ['x3i', 'x3o'] }), Object.assign(M('bulwark', 'Bulwark', 'Bw', 'bulwark'), { io: ['x4i', 'x4o'] }), { id: 'out', kind: 'out', name: 'Out', short: 'Out' }];
const GWIRES = [{ u: 'osc1', v: 'f1', lv: [1, 1, 1, null] }, { u: 'osc2', v: 'f1', lv: [.8, .8, .8, .8] }, { u: 'noise', v: 'f2', lv: [.5, .5, .5, .5] }, { u: 'osc1', v: 'shriek', lv: [null, null, null, .9] },
  { u: 'osc2', v: 'shriek', lv: [.4, null, .4, null] }, { u: 'noise', v: 'echo', lv: [null, null, .6, null] }, { u: 'f1', v: 'f2', lv: [null, 1, null, null] }, { u: 'f2', v: 'f1', lv: [null, null, .7, null] },
  { u: 'f1', v: 'echo', lv: [1, null, 1, .8] }, { u: 'f1', v: 'out', lv: [.5, null, .5, .5] }, { u: 'f2', v: 'echo', lv: [null, 1, null, null] }, { u: 'f2', v: 'scape', lv: [.6, .6, null, .6] },
  { u: 'echo', v: 'scape', lv: all(.7) }, { u: 'echo', v: 'out', lv: all(.5) }, { u: 'scape', v: 'out', lv: all(1) }, { u: 'shriek', v: 'out', lv: [.8, null, .8, .8] },
  { u: 'scape', v: 'echo', lv: [null, .15, null, null] }, { u: 'sub', v: 'bulwark', lv: all(.8) }, { u: 'sub', v: 'out', lv: [null, .6, null, null] }, { u: 'bulwark', v: 'out', lv: all(1) }];
const graph = () => new W({ nodes: GNODES, wires: GWIRES });

// ---- the crowded patch: the shell's house default (six inputs into the first module, the modules in order, the last to Out), then seeded Randomize draws at B to D ----
const mulberry = seed => () => { seed |= 0; seed = seed + 0x6D2B79F5 | 0; let t = Math.imul(seed ^ seed >>> 15, 1 | seed); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; };
const crowded = () => { const ins = ['osc1', 'osc2', 'sub', 'noise', 'flt1', 'flt2'], mods = ['echo', 'scape', 'shriek', 'bulwark'], ids = mods.concat(['out']);
  const m = new W({ nodes: ins.map(id => ({ id, kind: 'in', name: id, short: id, style: 'lane' })).concat(mods.map(id => M(id, id, id, id)), [{ id: 'out', kind: 'out', name: 'Out', short: 'Out' }]),
    wires: ins.map(u => ({ u, v: 'echo', lv: all(1) })).concat(mods.map((u, i) => ({ u, v: ids[i + 1], lv: all(1) }))) });
  [1, 2, 3].forEach(c => m.drawCorner(c, mulberry(40 + c), c / 3)); return m; };

// ---- geometry, as drawn ----
const PADS = [[.3, .35], [.8, .2], [.5, .9]];
let views = 0, worstGap = Infinity, maxCross = 0;
for (const [pname, model] of [['sample', fresh()], ['crowded', crowded()], ['graph', graph()]]) for (const width of [333, 600, 693, 960]) for (const prevent of [false, true]) {
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

// ---- GUI-76 and ADR-414: the voice boundary, either order with no loop, late wires, the console's flow from the graph ----
{ const g = graph(), at = c => W.weights(...[[0, 0], [1, 0], [0, 1], [1, 1]][c]);
  ok(g.boundary() === 5 && fresh().boundary() === -1, `the boundary sits after Filter 2 (${g.boundary()}), and a row with no per-voice node marked draws none`);
  ok(g.can('echo', 'f1') !== '' && g.can('scape', 'f2') !== '' && !g.setLevel('echo', 'f1', 0, .5), 'the voice boundary: a module into a filter is refused and never written');
  ok(g.can('f2', 'shriek') === '' && g.can('osc1', 'f2') === '' && g.can('f1', 'f2') === '', 'CONTROL: a filter into a module, a source into a filter and Filter 1 into Filter 2 are allowed');
  ok(g.canAt('f2', 'f1', 1) !== '' && !g.setLevel('f2', 'f1', 1, .5) && g.find('f2', 'f1').lv[1] === null, 'either order, no loop: Filter 2 into Filter 1 at B, where Filter 1 feeds Filter 2, is refused');
  ok(g.canAt('f1', 'f2', 2) !== '' && g.canAt('f2', 'f1', 0) === '' && g.setLevel('f2', 'f1', 0, .5), 'CONTROL: at A, where neither runs into the other, Filter 2 into Filter 1 is made; at C Filter 1 into Filter 2 is refused');
  ok(g.late('scape', 'echo') && !g.late('f2', 'f1') && !g.late('f1', 'out'), 'a return on the summed side is late (z⁻¹); Filter 2 into Filter 1 is not, nor a forward wire');
  const Lc = W.ladder(g, g.shown(2, at(2), false), 960), xf = Lc.lay.wires.find(x => x.w.u === 'f2' && x.w.v === 'f1');
  ok(xf && xf.dir === 'r' && !xf.late && !/z/.test(xf.chip.txt), 'Filter 2 into Filter 1 runs below the row with no z⁻¹ (' + (xf && xf.chip.txt) + ')');
  const E = (m, c) => FLOW.fromGraph(m, at(c)).map(e => e.u + '>' + e.v);
  const h = graph(), eA = E(h, 0), eB = E(h, 1), eC = E(h, 2);   // a fresh graph: the controls above changed g
  ok(['osc2>x3i', 'x3i>x3o', 'f1o>x1i', 'f1i>f1o', 'x2o>lim', 'sub>x4i', 'f1o>lim', 'lim>vol'].every(k => eA.includes(k)), 'the console draws the graph at A: Osc 2 into SHRIEK IN, F1 OUT into x1 IN, Scape to LIM IN, Filter 1 dry to LIM IN');
  ok(eB.includes('f1o>f2i') && eC.includes('f2o>f1i') && !eA.includes('f1o>f2i') && !eA.includes('f2o>f1i'), 'live with the pad: B runs F1 → F2, C runs F2 → F1; CONTROL: A runs neither');
  ok(!eB.some(k => k === 'x2o>x1i'), 'a return wire (Scape → Echo at B) is left out of the console picture');
  [0, 1, 2, 3].forEach(c => h.setLevel('osc2', 'shriek', c, null)); ok(!E(h, 0).includes('osc2>x3i'), 'CONTROL: Osc 2 → Shriek removed at every corner leaves the picture');
  const F = FLOW.make(); F.graph = () => [{ u: 'osc1', v: 'lim' }]; ok(FLOW.edges(F).length === 1 && Object.keys(FLOW.feeds(F)).length === 0 && FLOW.edges(FLOW.make()).some(e => e.u === 'mk'), 'a flow with a graph draws only the graph; CONTROL: a plain flow keeps X13\'s edges'); }
// GUI-76 A2: GUI-14's bypass rule on the graph. Every wire into a bypassed slot runs on to every place it fed, its own edge goes, and its strips read the one stage
// feeding it (none when two do). CONTROL: nothing bypassed leaves the graph's edges as they are.
{ const G = [['x1o', 'x2i'], ['f1o', 'x2i'], ['x2i', 'x2o'], ['x2o', 'x3i'], ['x2o', 'lim'], ['x3i', 'x3o'], ['x3o', 'lim'], ['lim', 'vol']].map(q => ({ u: q[0], v: q[1] }));
  const F = FLOW.make(); F.graph = () => G.slice(); const ks = () => FLOW.edges(F).map(e => e.u + '>' + e.v).sort().join();
  ok(ks() === G.map(e => e.u + '>' + e.v).sort().join(), 'CONTROL: a graph with nothing bypassed was rewired');
  F.byp = { 2: true }; const E = ks();
  ok(['x1o>x3i', 'f1o>x3i', 'x1o>lim', 'f1o>lim', 'x3i>x3o'].every(k => E.split(',').includes(k)) && !/x2[io]/.test(E), `a bypassed slot 2 did not run its inputs on to its outputs: ${E}`);
  ok(Object.keys(FLOW.feeds(F)).length === 0, 'a bypassed slot fed by two stages read one of them');
  F.graph = () => G.filter(e => e.u !== 'f1o'); ok(FLOW.feeds(F)[2] === 'x1o', 'a bypassed slot fed by one stage did not read it'); }

if (bad.length) { console.error('fx_routing_check: RED\n  ' + bad.join('\n  ')); process.exit(1); }
console.log(`fx_routing_check: GREEN — ${views} ladder views (3 patches × 4 corners and Heard at 3 pads × prevent off/on × 4 widths): 0 shared ports, 0 overlaps, 0 band points, 0 chip clashes, closest ports ${worstGap.toFixed(2)} px, crossings up to ${maxCross} (interleaving spans); the one writer, Heard, held back edges, Randomize's draw and copy, the voice boundary, either order with no loop and the console's flow from the graph and its bypass hold; 14 controls fired`);
