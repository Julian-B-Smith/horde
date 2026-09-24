/*
 * fxlab_check.mjs — the FX design lab's EQ and compressor cards tell the truth.
 * WIRED: ./verify fast (beside lab_load_check).
 *
 * WHY THIS EXISTS (B234). docs/design/fx-design-lab.html's EQ and COMP cards
 * run real DSP in the page and draw pictures of it: the EQ's response curve and
 * the compressor's transfer curve. The lab measures both claims at load —
 * eqSelfCheck() and compSelfCheck() — and prints the result in its audit line.
 * A result nobody opens the page to read is not a gate, so this file runs the
 * same self-checks headlessly and fails when the page would print an ERROR.
 *
 * WHAT IS ASSERTED (the page's own numbers, exported as globalThis.FXLAB_SELFCHECK):
 *   eq    — the drawn curve matches the measured sine response of the running
 *           filters within 0.01 dB (defaults + a stress set, every type and slope);
 *           MUST-FAIL CONTROL: the curve designed at 48 kHz against filters at
 *           44.1 kHz must miss by more than the tolerance.
 *   comp  — steady-state gain reduction for a sine at four known levels (below,
 *           in and above the knee, and through the sidechain HPF) matches the
 *           transfer curve within 0.05 dB, measured at the output AND on the meter;
 *           MUST-FAIL CONTROL 1: a hard-knee curve must miss; CONTROL 2 (B230):
 *           a second instance fed silence reads exactly 0 dB GR, and the same pair
 *           with the rack's shared-envelope law planted must read more than 1 dB.
 * A control that passes means the check cannot see the fault it exists for; the
 * page reports that as an error and so does this file.
 *
 * APPROACH. The lab's inline scripts run in a vm context with inert DOM stubs
 * (the lab_load_check idiom, reduced to what this page touches). Only the
 * self-check result is read; nothing is drawn.
 *
 * Usage: node tools/labharness/fxlab_check.mjs [lab.html]   (exit 1 on any error;
 *        the argument exists so a scratch copy with a planted fault can be run)
 */
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import vm from 'node:vm';

const root = join(dirname(fileURLToPath(import.meta.url)), '../..');
const file = process.argv[2] || join(root, 'docs/design/fx-design-lab.html');

// A value that can be called, constructed, indexed and coerced without
// throwing, so the only thing that can fail is the lab's own arithmetic.
function stub() {
  const f = function () {};
  return new Proxy(f, {
    get(t, p) {
      if (p === Symbol.iterator) return function* () {};
      if (p === Symbol.toPrimitive) return () => 0;
      if (p === 'length') return 0;
      if (p === 'then') return undefined;
      return stub();
    },
    set() { return true; }, has() { return true; },
    apply() { return stub(); }, construct() { return stub(); },
  });
}

const html = readFileSync(file, 'utf8');
const blocks = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]);
const sandbox = {
  document: stub(), location: stub(), navigator: stub(),
  requestAnimationFrame: () => 0, setTimeout: () => 0, addEventListener: () => {},
  console: { log() {}, warn() {}, error() {} }, Math, JSON,
  getComputedStyle: () => stub(), devicePixelRatio: 1,
};
sandbox.globalThis = sandbox; sandbox.window = sandbox; sandbox.self = sandbox;
const ctx = vm.createContext(sandbox);
let fail = 0;
try {
  blocks.forEach((b, i) => new vm.Script(b, { filename: `fx-design-lab.html#script${i + 1}` }).runInContext(ctx, { timeout: 60000 }));
} catch (e) {
  console.log(`FAIL  the lab threw at load: ${e && e.name}: ${e && e.message}`);
  process.exit(1);
}
const R = sandbox.FXLAB_SELFCHECK;
if (!R || !R.eq || !R.comp) {
  console.log('FAIL  globalThis.FXLAB_SELFCHECK is missing — the self-checks did not run');
  process.exit(1);
}
const { eq, comp } = R;
console.log(`eq    curve vs measured  max Δ ${eq.worst.toFixed(4)} dB over ${eq.pts} pts (tol 0.01)   `
  + `control 48 kHz curve Δ ${eq.ctl.toFixed(3)} dB → ${eq.ctlFails ? 'fails, as it must' : 'PASSED (blind)'}`);
for (const [f, L, sc, want, meas] of comp.rows)
  console.log(`comp  ${String(f).padStart(4)} Hz @ ${String(L).padStart(3)} dBFS${sc ? ' via SC HPF' : '           '}  curve GR ${want.toFixed(3)}  measured ${meas.toFixed(3)} dB`);
console.log(`comp  max Δ ${comp.worst.toFixed(4)} dB (tol 0.05)   control hard-knee curve Δ ${comp.ctl.toFixed(3)} dB → `
  + `${comp.ctlFails ? 'fails, as it must' : 'PASSED (blind)'}`);
console.log(`comp  silent 2nd instance GR ${comp.grSilent.toFixed(3)} dB   control B230 shared envelope GR `
  + `${comp.grShared.toFixed(2)} dB → ${comp.sharedFails ? 'fails, as it must' : 'PASSED (blind)'}`);
for (const e of R.errors) { fail++; console.log(`ERROR ${e}`); }
console.log(`\n${fail ? 'RED' : 'GREEN'} — fxlab_check: EQ curve and compressor law, ${fail} error(s)`);
process.exit(fail ? 1 : 0);
