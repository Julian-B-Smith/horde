/*
 * lab_load_check.mjs — does each lab HTML actually SURVIVE being loaded?
 *
 * WHY THIS EXISTS. Five times in this project a lab has shipped with a
 * setup-time call reaching forward to a `const`/`let` declared further down the
 * file (L0026). The throw is swallowed by the browser, the rest of the script
 * never runs, and the page still LOOKS fine — the mod lab's entire matrix was
 * missing for weeks that way. The fifth instance was written immediately after
 * documenting that the trap needs tooling rather than care, which is the whole
 * argument for this file.
 *
 * APPROACH. Not a static heuristic — those cry wolf, and a gate that cries wolf
 * trains you to skim it. Instead each <script> block is executed for real in a
 * vm context whose DOM/audio globals are universal proxies (every property is
 * another callable proxy, so no lab code can fail for lack of a canvas). What
 * is left is genuine JS: TDZ errors, typos, bad references. If it throws at
 * load, the page is broken in a browser too.
 *
 * Usage: node tools/labharness/lab_load_check.mjs [file.html ...]
 * Exit 1 if any lab throws.
 */
import { readFileSync, readdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, resolve, join, basename } from 'node:path';
import vm from 'node:vm';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const labDir = join(root, 'docs/design');
// B138 (2026-09-16): the reference prototypes were never in the sweep — the
// 2026-09-07 layout move put the spec-in-code labs under reference/ and this
// gate kept reading only docs/design, so fifteen labs loaded unchecked while
// it printed GREEN. reference/ and its one packet directory are swept now.
const refDirs = [join(root, 'reference'), join(root, 'reference/maw')];

// A value that can be called, constructed, indexed, iterated and coerced
// without ever throwing — so the ONLY errors that surface are the lab's own.
function stub() {
  const f = function () {};
  return new Proxy(f, {
    get(t, p) {
      if (p === Symbol.iterator) return function* () {};
      if (p === Symbol.toPrimitive) return () => 0;
      if (p === 'length') return 0;
      if (p === 'then') return undefined;          // never look thenable
      if (p === 'style' || p === 'dataset' || p === 'classList') return stub();
      return stub();
    },
    set() { return true; },
    has() { return true; },
    apply() { return stub(); },
    construct() { return stub(); },
  });
}

function checkFile(file) {
  const html = readFileSync(file, 'utf8');
  const blocks = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]);
  if (!blocks.length) return { file, skipped: 'no inline script' };
  const sandbox = {
    document: stub(), window: stub(), location: stub(),
    // navigator is a stub EXCEPT requestMIDIAccess, which must look like a
    // promise: the generic stub deliberately has no `then` (so an awaited stub
    // resolves instead of hanging), and `.then(...)` on it is a TypeError the
    // checker would pin on the lab (the MAW prototype, B138).
    navigator: new Proxy({ requestMIDIAccess: () => ({ then: () => ({ catch() {} }) }) },
                         { get: (t, p) => (p in t ? t[p] : stub()) }),
    AudioContext: function () { return stub(); },
    webkitAudioContext: function () { return stub(); },
    requestAnimationFrame: () => 0, cancelAnimationFrame: () => {},
    setTimeout: () => 0, setInterval: () => 0, clearTimeout: () => {},
    clearInterval: () => {}, addEventListener: () => {}, alert: () => {},
    console: { log() {}, warn() {}, error() {} },
    Math, JSON, Date, performance: { now: () => 0 },
    // Real-enough DOM constructors the labs construct directly. Omitting these
    // made the checker blame spectra-lab for the CHECKER's missing global —
    // exactly the cry-wolf failure this gate exists to avoid.
    Event: class { constructor(t) { this.type = t; } },
    CustomEvent: class { constructor(t, o) { this.type = t; this.detail = o && o.detail; } },
    KeyboardEvent: class { constructor(t) { this.type = t; } },
    MouseEvent: class { constructor(t) { this.type = t; } },
    Image: class {}, Blob: class {}, URL: { createObjectURL: () => '', revokeObjectURL() {} },
    // Four more the reference labs read at setup (B138): without them the
    // checker blamed five labs for its OWN missing globals — the cry-wolf case.
    Option: class { constructor(text, value) { this.text = text; this.value = value; } },
    getComputedStyle: () => stub(), devicePixelRatio: 1,
    matchMedia: () => ({ matches: false, addEventListener() {}, addListener() {} }),
  };
  sandbox.globalThis = sandbox; sandbox.self = sandbox;
  sandbox.window = sandbox;
  const ctx = vm.createContext(sandbox);
  for (let i = 0; i < blocks.length; i++) {
    try {
      new vm.Script(blocks[i], { filename: `${basename(file)}#script${i + 1}` })
        .runInContext(ctx, { timeout: 20000 });
    } catch (e) {
      // Report the lab's own failures. A stub can never satisfy every DOM
      // contract, so errors that are clearly the stub's fault are not the
      // lab's bug — but a ReferenceError always is.
      const msg = String(e && e.message || e);
      const stack = String(e && e.stack || '').split('\n').slice(0, 3).join(' | ');
      return { file, block: i + 1, kind: e && e.name, msg, stack };
    }
  }
  return { file, ok: true };
}

const args = process.argv.slice(2);
// Default sweep covers the design labs AND the shipping GUIs — gui2.html is
// built up cluster-by-cluster on a branch and must never load-fail silently.
const guiDir = join(root, 'src/gui');
const files = args.length ? args.map(a => resolve(a))
  : [...readdirSync(labDir).filter(f => f.endsWith('.html')).sort().map(f => join(labDir, f)),
     ...refDirs.flatMap(d => readdirSync(d).filter(f => f.endsWith('.html')).sort().map(f => join(d, f))),
     ...readdirSync(guiDir).filter(f => f.endsWith('.html')).sort().map(f => join(guiDir, f))];

let bad = 0, skipped = 0;
for (const f of files) {
  const r = checkFile(f);
  const name = basename(r.file);
  if (r.skipped) { skipped++; console.log(`SKIP  ${name}  (${r.skipped})`); }
  else if (r.ok) console.log(`OK    ${name}`);
  else { bad++;
    console.log(`FAIL  ${name}  script block ${r.block}: ${r.kind}: ${r.msg}`);
    console.log(`        ${r.stack}`); }
}
console.log(`\n${bad ? 'RED' : 'GREEN'} — ${files.length} labs loaded, ${bad} broken, ${skipped} skipped`);
process.exit(bad ? 1 : 0);
