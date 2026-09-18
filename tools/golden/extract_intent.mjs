// Slice the intent-bus prototype's model live out of the protected HTML, so
// reference/intent-bus.html stays the single source of truth and there is no
// forked copy to drift. Same discipline as extract_core.mjs / extract_glide.mjs;
// the marker pair differs because the intent lab uses `/* ---------- model
// ---------- */` ... `/* ---------- audio ---------- */` rather than the
// oscillator labs' `/* ===== DSP: ... */` banners.
//
// TWO SLICES, deliberately.
//
//   extractModel()  the model block. DOM-free and self-contained: the corner
//                   tables, mulberry32 + reshuffle, weights(), pick(), and
//                   resolve(). Evaluates under `new Function` with no stubs.
//
//   extractCommit() the `commit` button's click handler (SPEC-INTENT-BUS §7).
//                   It lives in the UI block and is NOT DOM-free: it ends with
//                   `S.editing = dom; buildEditor();`. This is option (a) of
//                   docs/proposals/b89-phase2-intent-resolver.md §2 — extract
//                   the handler body as a second slice with its ONE UI call
//                   (buildEditor) stubbed to a no-op, rather than option (b)
//                   (reimplement §7 in the generator), because (b) would make
//                   T6 spec-parity instead of oracle-parity and the commit
//                   law — argmax ties, bake-then-clamp, which intents get
//                   zeroed — is exactly the kind of detail a reimplementation
//                   gets subtly right and the prototype gets actually right.
//                   `S.editing = dom` is model state and is KEPT.
//
// Everything is located by CONTENT, never by line index: a magic number in an
// extractor is a delayed break (the lesson extract_glide.mjs carries).
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const SRC = 'reference/intent-bus.html';

const MODEL_OPEN = '/* ---------- model ---------- */';
const MODEL_CLOSE = '/* ---------- audio ---------- */';
const COMMIT_OPEN = "document.getElementById('commit').onclick=()=>{";

function script() {
  const html = readFileSync(join(root, SRC), 'utf8');
  const m = html.match(/<script>([\s\S]*?)<\/script>/);
  if (!m) throw new Error(`${SRC}: no <script> block`);
  return m[1];
}

function modelSource() {
  const src = script();
  const a = src.indexOf(MODEL_OPEN);
  const b = src.indexOf(MODEL_CLOSE);
  if (a < 0 || b < 0 || b < a)
    throw new Error(`${SRC} shape changed: model markers not found (${a}, ${b})`);
  return src.slice(a, b);
}

// The handler body, brace-matched from its opening line so an edit inside it
// cannot silently truncate the slice.
function commitSource() {
  const src = script();
  const at = src.indexOf(COMMIT_OPEN);
  if (at < 0) throw new Error(`${SRC} shape changed: commit handler not found`);
  const bodyFrom = at + COMMIT_OPEN.length;
  let depth = 1, end = -1;
  for (let i = bodyFrom; i < src.length; i++) {
    const ch = src[i];
    if (ch === '{') depth++;
    else if (ch === '}') { depth--; if (depth === 0) { end = i; break; } }
  }
  if (end < 0) throw new Error(`${SRC} shape changed: commit handler never closes`);
  const body = src.slice(bodyFrom, end);
  if (!body.includes('buildEditor()'))
    throw new Error(`${SRC} shape changed: commit no longer calls buildEditor — re-check the stub list`);
  return body;
}

// One evaluation = one pristine model. Cases MUTATE the corner tables and S,
// so every case re-extracts rather than trying to undo its own edits; that is
// also what makes --selfcheck mean something.
export function loadModel() {
  const src = modelSource();
  // `buildEditor` is the commit slice's only UI reach; a no-op here keeps the
  // handler's model effects (bake, re-home, zero macros, S.editing) intact.
  const boot = `
    function buildEditor(){ /* UI stub — see extract_intent.mjs */ }
    function __commit__(){ ${commitSource()} }
    return { P, INT, CN, corners, S, weights, pick, reshuffle, resolve,
             commit: __commit__, mulberry32 };`;
  const api = new Function(src + boot)();
  for (const k of ['resolve', 'weights', 'pick', 'reshuffle', 'commit', 'mulberry32'])
    if (typeof api[k] !== 'function') throw new Error(`${SRC}: ${k} did not evaluate`);
  if (!Array.isArray(api.P) || api.P.length === 0) throw new Error(`${SRC}: P did not evaluate`);
  return api;
}
