/*
 * gui_history_check — every control the GUI exposes must MARK THE HISTORY.
 * WIRED: ./verify fast.
 *
 *   node tools/labharness/gui_history_check.mjs [gui.html]   (default: src/gui/gui2.html)
 *
 * WIRED in ./verify's `fast` leg, beside the other node harnesses (which skip
 * when node is absent, the convention this follows). It landed UNWIRED for one
 * commit and the lead wired it on integration: the implementing agent's brief
 * carried a stale blanket rule ("do not edit ./verify") alongside ADR-180
 * section 1, which had already narrowed that gate to WEAKENING a check —
 * adding one is not gated. The agent obeyed the stricter line and said so,
 * which was the correct call from inside the brief; the contradiction was the
 * lead's to fix, and the boilerplate has been corrected so the next brief does
 * not reproduce it.
 *
 * THE BUG THIS EXISTS FOR (B191). The human, 2026-09-21: "I changed the shape
 * value and it didn't make a node." A history node exists only where a gesture
 * bracket CLOSES — the shell's `guiGesture` calls `undoMarkParam` on the END
 * and on nothing else (src/hypersaw_clap.cpp). gui2 opened and closed that
 * bracket from `pointerdown`/`pointerup` ON THE CONTROL ELEMENT, and that
 * signal does not exist for every control:
 *   * a <select> opens a NATIVE popup outside the document, so the release
 *     lands on the platform menu and `pointerup` need never reach the element;
 *   * a control changed from the KEYBOARD emits no pointer events at all;
 *   * a knob's own <input type=range> has `pointer-events:none`, so its
 *     pointer listeners were dead from the day the skin was added.
 * The sub's Wave is a <select>, which is what the human saw — but so are 69
 * other selects, 36 toggles and every knob's keyboard path. It is a CLASS.
 *
 * THE PROPERTY, and why it takes two gates. "Changing a parameter through its
 * own control produces exactly one history node" splits cleanly in two:
 *   (1) the control emits exactly one BALANCED gesture bracket around its
 *       value change, whatever the input modality — this file;
 *   (2) one closed bracket on that id produces exactly one node — undo_check
 *       (layer 4, `controlMarkChecks`), which drives the real shell.
 * Neither half alone is the property; both are wired.
 *
 * HOW THIS RUNS THE REAL CODE, and why not a browser. The wiring is EXECUTED,
 * not pattern-matched: the `/*GATE:BRACKET*(/` block and the `wireKnob`,
 * `mxWire`, `buildOscSelectors`, `buildSubTab` functions are lifted verbatim
 * out of the GUI and evaluated in a vm context whose `document` hands back
 * fake controls built from the page's OWN markup, over node's real
 * `EventTarget`. A static rule would have to guess which listener fires when,
 * which is exactly the reasoning that produced the bug. Headless Chrome was
 * the other candidate and was rejected for two reasons: it cannot drive a
 * NATIVE <select> popup either (the case under test), and ./verify must not
 * grow a browser dependency. What a browser would add over this — layout,
 * real popups — is precisely the part no automated harness can reach on any
 * platform, so it would buy nothing and cost a dependency.
 *
 * TOTALITY, so a new parameter is covered without anyone remembering. Every
 * `[data-p]` element in the page is enumerated and exercised; gui2's controls
 * are GENERATED from src/param_presentation.tsv (tools/gen_gui_controls.py,
 * whose currency ./verify gates), so a new table row becomes a new control and
 * a new control is picked up here on the next run. An element whose control
 * kind this file does not recognise is a FAILURE, not a skip — that is what
 * makes a newly invented control type meet the gate instead of slipping past.
 *
 * CALIBRATION (L0032/L0033). Two zeros are worthless without a one beside
 * them, so the run ends by wiring two PLANTED controls: the pre-B191 shape
 * (pointer-only brackets) and a control with no bracket at all. Each must be
 * reported RED by the same scenario runner that must report the real wiring
 * GREEN. A plant that fails to fire fails the run.
 *
 */
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, resolve, join } from 'node:path';
import vm from 'node:vm';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const guiPath = process.argv[2] ? resolve(process.argv[2]) : join(root, 'src/gui/gui2.html');
const html = readFileSync(guiPath, 'utf8');

let failures = 0;
const fail = (what) => { console.log(`FAIL ${what}`); failures++; };
const ok = (what) => console.log(`OK   ${what}`);
const check = (cond, what) => (cond ? ok(what) : fail(what));

/* ---------------- lifting the wiring out of the page ---------------- */

/* Exact anchors with uniqueness asserts, never index arithmetic on a
   substring (L0046): a marker pair for the top-level block, and for a function
   the `function NAME(` line down to the next line that is exactly `}`, which
   is what closes a top-level declaration in this file. */
function markedBlock(name) {
  const open = html.indexOf(`/*GATE:${name}`);
  const close = html.indexOf(`/*/GATE:${name}*/`);
  if (open < 0 || close < 0 || close < open)
    throw new Error(`gui_history_check: the GATE:${name} markers are missing from ${guiPath} —
  the gate's only anchor into the page. Restore them, or the wiring is untested.`);
  return html.slice(open, close);
}
function topLevelFn(name) {
  const lines = html.split('\n');
  const starts = lines.reduce((a, l, i) => (l.startsWith(`function ${name}(`) ? a.concat(i) : a), []);
  if (starts.length !== 1)
    throw new Error(`gui_history_check: expected exactly one top-level \`function ${name}(\`, found ${starts.length}`);
  for (let i = starts[0] + 1; i < lines.length; i++)
    if (lines[i] === '}') return lines.slice(starts[0], i + 1).join('\n');
  throw new Error(`gui_history_check: no top-level close for function ${name}`);
}

/* ---------------- the page's own controls, from its own markup ---------- */

// Script and style bodies first: both carry template literals that emit markup
// (`data-addr="${addr}"`), and a scan that read those would invent controls.
const markup = html.replace(/<script[\s\S]*?<\/script>/g, '').replace(/<style[\s\S]*?<\/style>/g, '');
const VOID = new Set(['input', 'br', 'img', 'hr', 'meta', 'link', 'source', 'col']);

function scanControls() {
  const out = [];
  const stack = [];
  const tagRe = /<(\/?)([a-zA-Z][\w-]*)([^>]*)>/g;
  let m;
  while ((m = tagRe.exec(markup))) {
    const [, slash, tagRaw, attrs] = m;
    const tag = tagRaw.toLowerCase();
    if (slash) {
      for (let i = stack.length - 1; i >= 0; i--)
        if (stack[i].tag === tag) { stack.length = i; break; }
      continue;
    }
    const attr = (n) => (attrs.match(new RegExp(`\\b${n}="([^"]*)"`)) || [])[1];
    const classes = (attr('class') || '').split(/\s+/).filter(Boolean);
    const p = attr('data-p');
    if (p !== undefined) {
      out.push({
        tag,
        type: attr('type') || null,
        classes,
        id: Number(p),
        fixed: /\bdata-fixed="/.test(attrs),
        inKnob: stack.some((s) => s.classes.includes('knob')),
      });
    }
    if (!VOID.has(tag) && !attrs.endsWith('/')) stack.push({ tag, classes });
  }
  return out;
}

/* ---------------- a DOM only as real as the wiring needs --------------- */

class FakeEl extends EventTarget {
  constructor({ tag = 'div', type = null, classes = [], id = null, fixed = false } = {}) {
    super();
    this.tagName = tag.toUpperCase();
    this.type = type === null ? undefined : type;
    this.dataset = {};
    if (id !== null) this.dataset.p = String(id);
    if (fixed) this.dataset.fixed = '1';
    this._classes = new Set(classes);
    this.children = [];
    this.value = '0.5';
    this.min = '0'; this.max = '1'; this.step = '0.01';
    this.checked = false;
    this.disabled = false;
    this.textContent = '';
    this.hidden = false;
  }
  get className() { return [...this._classes].join(' '); }
  set className(v) { this._classes = new Set(String(v).split(/\s+/).filter(Boolean)); }
  get classList() {
    const s = this._classes;
    return {
      contains: (c) => s.has(c),
      add: (c) => s.add(c),
      remove: (c) => s.delete(c),
      toggle: (c, on) => { const want = on === undefined ? !s.has(c) : !!on; want ? s.add(c) : s.delete(c); return want; },
    };
  }
  appendChild(c) { this.children.push(c); return c; }
  querySelector(sel) {
    if (sel === 'input[type=range]') return this.children.find((c) => c.tagName === 'INPUT' && c.type === 'range') || null;
    return null;
  }
  focus() {}
  setPointerCapture() {}
  releasePointerCapture() {}
  set innerHTML(_) { this.children = []; }
  get innerHTML() { return ''; }
}

function ev(type, props = {}) {
  const e = new Event(type, { bubbles: true, cancelable: true });
  return Object.assign(e, { clientY: 0, pointerId: 1, shiftKey: false, deltaY: -1, ...props });
}

/* ---------------- the recorder and the properties ---------------- */

/* THE LOG IS THE OBSERVATION. Every bridge verb the wiring reaches for is
   recorded in order; the properties below are statements about that order and
   nothing else, so they hold identically for a knob, a select and a plant. */
const log = [];
const bridge = {
  gesture: (id, begin) => log.push({ k: begin ? 'begin' : 'end', id }),
  setParam: (id, v) => log.push({ k: 'set', id, v }),
  setVizOsc: () => {},
  getParams: async () => '{}',
};

/* One scenario's verdict: the bracket is balanced, never nested, closes the id
   it opened, and every value written lands INSIDE it. A setParam outside a
   bracket is an edit the history cannot see — the bug, stated as a property. */
function verdict(entries, expectId, { expectBrackets = 1, expectSets = null } = {}) {
  const problems = [];
  let open = null, begins = 0, ends = 0, sets = 0, outside = 0;
  for (const e of entries) {
    if (e.k === 'begin') {
      begins++;
      if (open !== null) problems.push('nested begin');
      open = e.id;
    } else if (e.k === 'end') {
      ends++;
      if (open === null) problems.push('end with no begin');
      else if (open !== e.id) problems.push(`end closes id ${e.id}, begin opened ${open}`);
      open = null;
    } else if (e.k === 'set') {
      sets++;
      if (e.id !== expectId) problems.push(`setParam on id ${e.id}, expected ${expectId}`);
      if (open === null) outside++;
    }
  }
  if (open !== null) problems.push(`bracket left OPEN on id ${open} (no history node)`);
  if (begins !== expectBrackets) problems.push(`${begins} gesture begin(s), expected ${expectBrackets}`);
  if (ends !== expectBrackets) problems.push(`${ends} gesture end(s), expected ${expectBrackets}`);
  // A declared exclusion writes its parameter and marks nothing, so "outside a
  // bracket" is the REQUIREMENT there rather than the defect.
  if (outside && expectBrackets > 0) problems.push(`${outside} setParam(s) outside any bracket`);
  if (expectSets !== null && sets !== expectSets) problems.push(`${sets} setParam(s), expected ${expectSets}`);
  if (expectSets === null && expectBrackets > 0 && sets === 0) problems.push('no setParam at all');
  return problems;
}

let scenariosRun = 0;
function run(name, id, play, opts) {
  log.length = 0;
  scenariosRun++;
  play();
  const problems = verdict(log.slice(), id, opts);
  // Only failures are named: 800-odd green rows would bury them. The COUNT is
  // printed at the end, so a green run still says how much it looked at.
  if (problems.length) fail(`${name}: ${problems.join('; ')}`);
  return problems;
}

/* ---------------- the sandbox ---------------- */

const controls = scanControls();
const registry = { '[data-p]': [], '.knob': [], '.oscSel': [] };

const sandbox = {
  console,
  WeakMap, Set, Map, Event, EventTarget, Math, JSON, Number, String, Object, Array, Promise,
  bridge,
  lastParams: {},
  SHELL_DEFAULTS: {},
  DEFAULTS: {},
  xyDirty: false,
  numOsc: 2,
  editOsc: 0,
  oscPanel: 'swarm',
  OSC_STRIDE: 1000,
  SUB_ON_ID: 4015,
  effId: (id) => id,                       // scope re-aiming is gui_reach's subject, not this one
  ctlToParam: (el) => Number(el.value),
  displayValue: () => {},
  paintControl: () => {},
  paintSolo: () => {},
  specimenSync: () => {},
  applyGates: () => {},
  applyOscPanel: () => {},
  drawSubWave: () => {},
  mxRange: () => ({ lo: -1, hi: 1 }),
  mxEditLo: () => -1,
  mxDefault: () => 0,
  resetToDefault: () => {},                // re-entered through its own latch below
  document: {
    querySelectorAll: (sel) => registry[sel] || [],
    createElement: (tag) => new FakeEl({ tag }),
  },
};
vm.createContext(sandbox);

/* Build the fake controls BEFORE the page's top-level wiring runs, because
   that wiring is a `querySelectorAll(...).forEach` evaluated on load — the
   same ordering the browser has. */
const knobWrappers = new Map();
for (const d of controls) {
  const el = new FakeEl(d);
  d.el = el;
  registry['[data-p]'].push(el);
  if (d.inKnob) {
    const k = new FakeEl({ tag: 'span', classes: ['knob'] });
    k.appendChild(el);
    knobWrappers.set(el, k);
    registry['.knob'].push(k);
  }
}
const oscSelHost = new FakeEl({ tag: 'div', classes: ['oscSel'] });
registry['.oscSel'].push(oscSelHost);

const source = [
  markedBlock('BRACKET'),
  topLevelFn('wireKnob'),
  topLevelFn('mxWire'),
  topLevelFn('buildOscSelectors'),
  topLevelFn('buildSubTab'),
  'globalThis.__gate = { wireKnob, mxWire, buildOscSelectors, gestureFor, NO_HISTORY_IDS };',
].join('\n\n');

try {
  new vm.Script(source, { filename: 'gui2.html:wiring' }).runInContext(sandbox);
} catch (e) {
  console.log(`FAIL wiring: the lifted block did not evaluate — ${e.message}`);
  process.exit(1);
}
const gate = sandbox.__gate;
for (const k of registry['.knob']) gate.wireKnob(k);
const excluded = new Set([...gate.NO_HISTORY_IDS]);

/* ---------------- the scenarios, per control kind ---------------- */

/* Every kind is driven through the paths a player actually has. The FIRST in
   each list is the one the old wiring could not serve: a value change with no
   pointer events at all, which is the keyboard path for everything and the
   native-popup path for a <select>. */
function kindOf(d) {
  if (d.classes.includes('msbtn')) return 'msbtn';
  if (d.inKnob) return 'knob';
  if (d.tag === 'select') return 'select';
  if (d.tag === 'input' && d.type === 'range') return 'range';
  if (d.tag === 'input' && d.type === 'checkbox') return 'checkbox';
  return null;
}

function scenarios(d) {
  const el = d.el;
  const k = knobWrappers.get(el);
  const input = () => el.dispatchEvent(ev('input'));
  const change = () => el.dispatchEvent(ev('change'));
  const down = (t = el) => t.dispatchEvent(ev('pointerdown'));
  const up = (t = el) => t.dispatchEvent(ev('pointerup'));
  const cancel = (t = el) => t.dispatchEvent(ev('pointercancel'));
  switch (kindOf(d)) {
    case 'knob':
      return [
        ['keyboard (arrow on the focused input, no pointer at all)', () => input(), { expectSets: 1 }],
        ['skin drag (the input itself never sees a pointer)', () => { down(k); input(); input(); input(); up(k); }, { expectSets: 3 }],
        ['drag stolen by the platform (pointercancel)', () => { down(k); input(); cancel(k); }, { expectSets: 1 }],
      ];
    case 'range':
      return [
        ['keyboard (arrow key, no pointer at all)', () => input(), { expectSets: 1 }],
        ['drag', () => { down(); input(); input(); up(); }, { expectSets: 2 }],
        ['drag stolen by the platform (pointercancel)', () => { down(); input(); cancel(); }, { expectSets: 1 }],
      ];
    case 'select':
      return [
        ['keyboard / type-ahead (no pointer events)', () => input(), { expectSets: 1 }],
        ['NATIVE POPUP: the release lands on the menu, not the element', () => { down(); input(); }, { expectSets: 1 }],
        ['popup that does return the release', () => { down(); input(); up(); }, { expectSets: 1 }],
      ];
    case 'checkbox':
      return [
        ['keyboard (space, no pointer events)', () => change(), { expectSets: 1 }],
        ['click', () => { down(); change(); up(); }, { expectSets: 1 }],
        ['click whose release is lost', () => { down(); change(); }, { expectSets: 1 }],
      ];
    case 'msbtn':
      return [['click (a div with no value event — the click IS the gesture)', () => el.dispatchEvent(ev('click')), { expectSets: 1 }]];
    default:
      return null;
  }
}

/* ---------------- the run ---------------- */

// Never the absolute path: this repo is public and a CI log must not print a
// machine's directory layout.
const shown = guiPath.startsWith(root) ? guiPath.slice(root.length + 1) : guiPath.split('/').pop();
console.log(`gui_history_check: ${shown} — ${controls.length} controls`);

const byKind = {};
let unknown = 0;
for (const d of controls) {
  const kind = kindOf(d);
  if (!kind) {
    unknown++;
    fail(`control ${d.id} <${d.tag} type=${d.type} class="${d.classes.join(' ')}"> is a control kind this gate does not know — a new kind must be given its scenarios here, never skipped`);
    continue;
  }
  byKind[kind] = (byKind[kind] || 0) + 1;
  const isExcluded = excluded.has(d.id);
  for (const [name, play, opts] of scenarios(d)) {
    // A DECLARED EXCLUSION IS AN ASSERTION, NOT AN ABSENCE: the control must
    // still write the parameter, and must produce NO bracket whatsoever.
    run(`id ${d.id} ${kind} — ${name}`, d.id, play,
        isExcluded ? { ...opts, expectBrackets: 0 } : opts);
  }
}
check(unknown === 0, `every control is a known kind (${Object.entries(byKind).map(([k, n]) => `${k}:${n}`).join(' ')})`);
for (const id of excluded)
  check(controls.some((d) => d.id === id),
        `declared exclusion ${id} is a control that EXISTS (a stale exclusion hides a real gap)`);

/* The two hand-placed power buttons: `sub.on` is the presentation table's
   `widget = none` case and this button is its ONLY control, and each
   oscillator's tab-bar power button is a SECOND door onto `osc.enable`. Both
   wrote the parameter with no bracket at all before B191. */
gate.buildOscSelectors();
const powers = oscSelHost.children.filter((c) => c._classes.has('oscPow'));
check(powers.length === 3, `the tab bar builds its power buttons (${powers.length}: two oscillators + SUB)`);
powers.forEach((pw, i) => {
  const id = pw.dataset.panel === 'sub' ? 4015 : 150 + Number(pw.dataset.osc) * sandbox.OSC_STRIDE;
  run(`power button ${pw.dataset.panel === 'sub' ? 'SUB' : 'osc ' + (i + 1)} (id ${id}) — click`,
      id, () => pw.dispatchEvent(ev('click')), { expectSets: 1 });
});

/* The routing matrix cell: a span with no value event, wired by its own
   function. Covered here rather than declared out, because "every control that
   moves a parameter" is the whole property. */
{
  const cell = new FakeEl({ tag: 'span', classes: ['mxc'], id: 3001 });
  gate.mxWire(cell, 3001);
  run('matrix cell 3001 — drag', 3001, () => {
    cell.dispatchEvent(ev('pointerdown'));
    cell.dispatchEvent(ev('pointermove', { clientY: -20 }));
    cell.dispatchEvent(ev('pointerup'));
  }, { expectSets: 1 });
  run('matrix cell 3001 — wheel', 3001, () => cell.dispatchEvent(ev('wheel')), { expectSets: 1 });
  run('matrix cell 3001 — double click resets', 3001, () => cell.dispatchEvent(ev('dblclick')), { expectSets: 1 });
}

/* ---------------- CALIBRATION: the plants (L0032/L0033) ---------------- */

/* Both zeros above are only evidence if this runner can see a broken control
   at all. These two are what the page looked like before B191 and what an
   unbracketed control looks like; each must be reported RED by the SAME
   verdict function, or the gate is a counter wired to nothing. */
function plantPointerOnly(el, id) {   // the pre-B191 wiring, verbatim in shape
  el.addEventListener(el.type === 'checkbox' ? 'change' : 'input', () => bridge.setParam(id, 1));
  el.addEventListener('pointerdown', () => bridge.gesture(id, true));
  el.addEventListener('pointerup', () => bridge.gesture(id, false));
}
function plantNoBracket(el, id) {     // the pre-B191 mute/solo and power buttons
  el.addEventListener('click', () => bridge.setParam(id, 1));
}

function mustBeRed(name, play, id, opts) {
  log.length = 0;
  play();
  const problems = verdict(log.slice(), id, opts);
  check(problems.length > 0, `PLANT ${name} is caught (${problems.join('; ') || 'NOTHING — the gate is blind'})`);
}
{
  const sel = new FakeEl({ tag: 'select', id: 4000 });
  plantPointerOnly(sel, 4000);
  mustBeRed('pointer-only <select>, keyboard path', () => sel.dispatchEvent(ev('input')), 4000, { expectSets: 1 });
  mustBeRed('pointer-only <select>, native popup (no pointerup)',
            () => { sel.dispatchEvent(ev('pointerdown')); sel.dispatchEvent(ev('input')); }, 4000, { expectSets: 1 });

  const btn = new FakeEl({ tag: 'div', classes: ['msbtn'], id: 104 });
  plantNoBracket(btn, 104);
  mustBeRed('control with no bracket at all', () => btn.dispatchEvent(ev('click')), 104, { expectSets: 1 });
}

/* WIDGET-KIND TOTALITY. Every widget the presentation table names must be a
   kind this gate exercises, so a new control type cannot enter the table
   without meeting a scenario list. The exclusions are named, not missing. */
const KIND_EXCLUSIONS = {
  none: 'hand-placed: `sub.sync` is retired and reaches nothing (B184); `sub.on` is the SUB tab power button, exercised above',
  enum: 'xyAsn0X..xyAsn1Y (174-177) were retired by ADR-156 and have no gui2 control',
};
const TSV_TO_KIND = { knob: 'knob', select: 'select', toggle: 'checkbox' };
const tsv = readFileSync(join(root, 'src/param_presentation.tsv'), 'utf8')
  .split('\n').filter((l) => l && !l.startsWith('#')).slice(1)
  .map((l) => l.split('\t'));
const widgets = [...new Set(tsv.map((r) => r[5]).filter(Boolean))].sort();
for (const w of widgets) {
  if (KIND_EXCLUSIONS[w]) { ok(`table widget \`${w}\` EXCLUDED — ${KIND_EXCLUSIONS[w]}`); continue; }
  const kind = TSV_TO_KIND[w];
  check(kind && byKind[kind] > 0,
        `table widget \`${w}\` is exercised by this gate (${kind ? byKind[kind] || 0 : 'no kind mapped'} controls)`);
}

console.log(failures
  ? `gui_history_check: RED (${failures} failure(s) over ${scenariosRun} scenarios)`
  : `gui_history_check: GREEN (${scenariosRun} scenarios over ${controls.length} controls + `
    + `${powers.length} power buttons + 1 matrix cell — every one marks the history exactly once)`);
process.exit(failures ? 1 : 0);
