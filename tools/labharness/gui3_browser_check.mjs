/*
 * gui3_browser_check.mjs — GUI 3 in a real browser: its self-checks pass, and its pictures still move after them (ROADMAP GB24, GUI-44).
 * WIRED: ./verify fast (beside gui3_seam_check). A missing browser prints a HOLE line there and does not fail verify (GUI-44, the human:
 * "fast, hole if absent"); a hole is visible on every run, never silent.
 *
 *   node tools/labharness/gui3_browser_check.mjs        the check and its control
 *   HORDE_CHROME=/path/to/chrome node ...                use that browser
 *
 * WHY (the human, 2026-11-05: "Everything is still frozen on all the osc views", the third report). GUI 3's ~83 self-checks run only when a page
 * loads in a browser, and no gate loads it in one: `gui3_seam_check` reads its text and runs its pure parts in node, and `lab_load_check`
 * runs a stub that executes no external script. So two things shipped that only a real browser shows:
 *   * the self-checks themselves, never run by `./verify`, only by hand;
 *   * a picture frozen AFTER the self-checks: they drive the pictures on a clock 1e7 ms ahead, every pace kept that as its last draw, and
 *     the real clock after them was never due again. Loading with `?fast` skips the checks, which is why hand tests saw motion.
 *
 * WHAT IT DOES, deterministically up to the browser's own frame timing (it measures change, never a duration):
 *   1. Serves docs/ read-only on a free port, and starts a headless Chrome on it over the DevTools protocol (no npm module: Node's own
 *      WebSocket and fetch). The window is 1280 x 900, so GUI 3's 980 x 720 window fits as the plugin shows it.
 *   2. Loads design/gui3.html WITHOUT ?fast and waits for the self-checks to finish (the status reads Ready. and at least one OK line).
 *      RED on any FAILED line, any uncaught exception, or no OK lines.
 *   3. Opens Osc 1 and takes two snapshots of every visible picture 1.5 s apart. RED unless the phase ring, the phase carpet and the
 *      voice map each changed: those three move every frame by design (GUI-42).
 *   4. THE CONTROL: the same load with a plant that stops the page's frame loop after its self-checks. The same measurement must read
 *      frozen, or the detector can't tell motion from stillness and the check says so instead of passing.
 * A missing browser is a visible HOLE (exit 3), never a pass.
 */
import { spawn, spawnSync } from 'node:child_process';
import { createServer } from 'node:http';
import { readFile, mkdtemp, rm } from 'node:fs/promises';
import { existsSync, readdirSync, readFileSync } from 'node:fs';
import { join, extname, resolve, dirname } from 'node:path';
import { tmpdir, homedir } from 'node:os';
import { fileURLToPath } from 'node:url';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../../docs');   // docs/, from tools/labharness/
const sleep = ms => new Promise(r => setTimeout(r, ms));
const MOVERS = ['Phase ring', 'Phase carpet', 'Voice map'];

function findChrome() {
  if (process.env.HORDE_CHROME && existsSync(process.env.HORDE_CHROME)) return process.env.HORDE_CHROME;
  const cache = join(homedir(), 'Library/Caches/ms-playwright');
  if (existsSync(cache)) for (const d of readdirSync(cache).filter(n => /^chromium_headless_shell-/.test(n)).sort().reverse()) {
    const p = join(cache, d, 'chrome-headless-shell-mac-arm64/chrome-headless-shell'); if (existsSync(p)) return p;
  }
  return null;
}
const TYPES = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.svg': 'image/svg+xml', '.png': 'image/png', '.woff2': 'font/woff2' };
const EMBEDDED = '/__embedded.html';   // the plug-in's page: tools/gui3_inline.py's output under embed_file's policy, held in memory
let embedded = null;
function serve() {
  const srv = createServer(async (req, res) => {
    if (new URL(req.url, 'http://x').pathname === EMBEDDED && embedded) { res.writeHead(200, { 'content-type': 'text/html' }); res.end(embedded); return; }
    const p = join(ROOT, decodeURIComponent(new URL(req.url, 'http://x').pathname));
    if (!p.startsWith(ROOT)) { res.writeHead(403); res.end(); return; }   // read-only, and never outside docs/
    try { const b = await readFile(p); res.writeHead(200, { 'content-type': TYPES[extname(p)] || 'application/octet-stream' }); res.end(b); } catch { res.writeHead(404); res.end(); }
  });
  return new Promise(r => srv.listen(0, '127.0.0.1', () => r(srv)));
}
async function cdp(chrome, dir) {
  const proc = spawn(chrome, ['--headless', '--remote-debugging-port=0', '--user-data-dir=' + dir, '--window-size=1280,900', 'about:blank'], { stdio: ['ignore', 'ignore', 'pipe'] });
  let port = null; proc.stderr.on('data', d => { const m = /DevTools listening on ws:\/\/127\.0\.0\.1:(\d+)\//.exec(String(d)); if (m) port = +m[1]; });
  for (let i = 0; i < 80 && !port; i++) await sleep(100);
  if (!port) { proc.kill(); throw new Error('the browser never opened its DevTools port'); }
  const tab = (await (await fetch(`http://127.0.0.1:${port}/json`)).json()).find(t => t.type === 'page');
  const ws = new WebSocket(tab.webSocketDebuggerUrl); await new Promise((r, j) => { ws.onopen = r; ws.onerror = j; });
  let id = 0; const pend = {}, events = [];
  ws.onmessage = m => { const d = JSON.parse(m.data); if (d.id && pend[d.id]) { pend[d.id](d); delete pend[d.id]; } else if (d.method) events.push(d); };
  const call = (method, params = {}) => new Promise(r => { const i = ++id; pend[i] = r; ws.send(JSON.stringify({ id: i, method, params })); });
  const evalIn = async expr => { const r = await call('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true }); if (r.result.exceptionDetails) throw new Error(r.result.exceptionDetails.text + ' ' + (r.result.exceptionDetails.exception || {}).description); return r.result.result.value; };
  await call('Runtime.enable'); await call('Page.enable');
  await call('Emulation.setDeviceMetricsOverride', { width: 1280, height: 900, deviceScaleFactor: 1, mobile: false });
  return { proc, ws, call, evalIn, events };
}
const MEASURE = `(async () => {
  const go = x => [...document.querySelectorAll('button,[role=tab]')].find(e => e.textContent.trim() === x)?.click();
  go('Osc'); await new Promise(r => setTimeout(r, 300));
  [...document.querySelectorAll('#pnav button,#pnav [role=tab]')].find(e => /Osc 1$/.test(e.textContent.trim()))?.click();
  await new Promise(r => setTimeout(r, 1200));
  const cvs = [...document.querySelectorAll('#page-osc1 canvas')].filter(c => c.clientWidth && c.offsetParent), a = cvs.map(c => c.toDataURL());
  await new Promise(r => setTimeout(r, 1500));
  return cvs.map((c, i) => ({ name: c.getAttribute('aria-label') || '', moved: c.toDataURL() !== a[i] }));
})()`;
const READ = `(() => { const t = document.body.innerText.split('\\n'); return { ready: /Ready/.test((document.getElementById('status') || {}).textContent || ''), ok: t.filter(l => /^OK ·/.test(l)).length, failed: t.filter(l => /^FAILED/.test(l)).map(l => l.slice(0, 200)) }; })()`;
/* the control's plant: once the page has said Ready (its self-checks are done), frames stop; a detector that still reads motion can't see stillness */
const FREEZE = `(() => { const raf = window.requestAnimationFrame.bind(window); window.requestAnimationFrame = cb => raf(t => { const s = document.getElementById('status'); if (s && /Ready/.test(s.textContent) && document.body.innerText.includes('OK ·')) return; cb(t); }); })();`;

/* GUI-61 r3 (the human: a drag past the XY's edges "highlights text in the surrounding boxes", in the plug-in's WebKit view). Real mouse drags
   through CDP: a knob on Osc 1 and the Morph page's pad are each dragged onto a paragraph. HEADLESS CHROME CAN'T SHOW THE BUG: its own pointer
   capture already keeps those drags from selecting, with or without the guard (a plant proved it, 2026-11-07), and no WebKit runs here. So the
   check holds the MECHANISM: midway through each drag the page carries hd-nosel and the paragraph computes user-select none, and both clear on
   release. The control: a drag across the paragraph itself holds no capture, so no class, and it selects text (the paragraph is selectable). */
const SEL_SETUP = `(() => { const vis = e => !!e && e.getClientRects().length > 0 && getComputedStyle(e).visibility !== 'hidden', c = e => { const r = e.getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2, l: r.left, r: r.right }; };
  const text = [...document.querySelectorAll('p, .hd-hint, .hd-note')].find(e => vis(e) && e.textContent.trim().length > 60 && e.getBoundingClientRect().top > 0 && e.getBoundingClientRect().bottom < innerHeight);
  const knob = [...document.querySelectorAll('.page:not([hidden]) [data-key] [role="slider"]')].find(e => vis(e) && e.getBoundingClientRect().bottom < innerHeight);
  const line = e => { const r = e.getClientRects()[0]; return { x: (r.left + r.right) / 2, y: (r.top + r.bottom) / 2, l: r.left, r: r.right }; };   // a wrapped paragraph's box centre can fall between its lines
  getSelection().removeAllRanges(); return { text: text && line(text), knob: knob && c(knob) }; })()`;
async function mouseDrag(b, from, to, probe) {
  from = { x: Math.round(from.x), y: Math.round(from.y) }; to = { x: Math.round(to.x), y: Math.round(to.y) };   // whole pixels: a press at x.5 selects nothing in headless Chrome, which would blind the control
  await b.call('Input.dispatchMouseEvent', { type: 'mouseMoved', x: from.x, y: from.y });
  await b.call('Input.dispatchMouseEvent', { type: 'mousePressed', x: from.x, y: from.y, button: 'left', buttons: 1, clickCount: 1 });
  for (let i = 1; i <= 12; i++) await b.call('Input.dispatchMouseEvent', { type: 'mouseMoved', x: from.x + (to.x - from.x) * i / 12, y: from.y + (to.y - from.y) * i / 12, button: 'left', buttons: 1 });
  const mid = await b.evalIn(NOSEL);
  await b.call('Input.dispatchMouseEvent', { type: 'mouseReleased', x: to.x, y: to.y, button: 'left', buttons: 0, clickCount: 1 });
  // read after the release: headless Chrome reports only a caret while the button is down, and the dragged range once it's up
  await sleep(80); const got = await b.evalIn('getSelection().toString()'), after = await b.evalIn(NOSEL); await b.evalIn('getSelection().removeAllRanges()');
  return { sel: got.trim().length, held: mid.cls && mid.us === 'none', cleared: !after.cls && after.us !== 'none' };
}
const NOSEL = `(() => { const t = [...document.querySelectorAll('p, .hd-hint, .hd-note')].find(e => e.getClientRects().length && e.textContent.trim().length > 60); return { cls: document.documentElement.classList.contains('hd-nosel'), us: t ? getComputedStyle(t).webkitUserSelect || getComputedStyle(t).userSelect : null }; })()`;
async function selectProbe(b) {
  const at = await b.evalIn(SEL_SETUP); if (!at.text || !at.knob) return { missing: !at.text ? 'a paragraph' : 'a knob' };
  const control = await mouseDrag(b, { x: at.text.l + 3, y: at.text.y }, { x: at.text.r - 3, y: at.text.y });
  const at2 = await b.evalIn(SEL_SETUP), knob = await mouseDrag(b, at2.knob, at2.text);   // measured again: a drag can focus, and so scroll, the page
  await b.evalIn(`[...document.querySelectorAll('button,[role=tab]')].find(e => e.textContent.trim() === 'Morph')?.click()`); await sleep(400);
  const pad = await b.evalIn(`(() => { const e = document.querySelector('#morphPad .hm-qp-layer'); if (!e) return null; const r = e.getBoundingClientRect(); return { x: r.left + r.width * .3, y: r.top + r.height * .5 }; })()`);
  const t2 = (await b.evalIn(SEL_SETUP)).text, padSel = pad && t2 ? await mouseDrag(b, pad, t2) : null;
  return { knob, control, pad: padSel };
}

/* The probe loads the page fresh with ?fast, as the plug-in loads it (no self-checks). After the self-checks have run, a plain drag across a paragraph
   in headless Chrome leaves only a caret, so the control would fail there for a reason that isn't the guard's. */
async function runSelect(chrome, url) {
  const dir = await mkdtemp(join(tmpdir(), 'gui3-select-')), b = await cdp(chrome, dir);
  try {
    await b.call('Page.navigate', { url });
    let ready = false; for (let i = 0; i < 60 && !ready; i++) { await sleep(500); try { ready = await b.evalIn(`/Ready/.test((document.getElementById('status') || {}).textContent || '')`); } catch {} }
    // GUI-54's splash covers the window for its first 2.5 s and takes the first press, so the drags wait for it to go
    for (let i = 0; i < 40 && ready && await b.evalIn(`!!document.querySelector('.hd-splash')`); i++) await sleep(200);
    return ready ? await selectProbe(b) : { missing: 'loaded page' };
  } finally { b.ws.close(); b.proc.kill(); await sleep(200); await rm(dir, { recursive: true, force: true }); }
}

async function run(chrome, url, plant) {
  const dir = await mkdtemp(join(tmpdir(), 'gui3-browser-')), b = await cdp(chrome, dir);
  try {
    if (plant) await b.call('Page.addScriptToEvaluateOnNewDocument', { source: plant });
    await b.call('Page.navigate', { url });
    let st = null; for (let i = 0; i < 180; i++) { await sleep(500); try { st = await b.evalIn(READ); } catch { continue; } if (st.ready && (st.ok || st.failed.length)) break; }
    const thrown = b.events.filter(e => e.method === 'Runtime.exceptionThrown').map(e => (e.params.exceptionDetails.exception || {}).description || e.params.exceptionDetails.text);
    return { st, thrown, pics: await b.evalIn(MEASURE) };
  } finally { b.ws.close(); b.proc.kill(); await sleep(200); await rm(dir, { recursive: true, force: true }); }
}

/* GUI-54 (the human): the Mindlathe logo over the window when it first opens, fading on the first press or after 2.5 s. Real mouse through CDP, and the
   splash's life timestamped by a MutationObserver installed before the page loads (added, starts fading, removed), so the timings hold under load.
   - press: the art is painted (cyan and magenta pixels, under the plug-in page's policy too: no image loads, the bytes are decoded), the splash covers
     the window, a real click on the Mix tab under it changes no page and starts the fade, and the splash goes. CONTROL: the same click once it has
     gone opens Mix, so the first click was swallowed, not missed.
   - timer: with no input the fade starts 2 to 4 s after the splash is SEEN (the observer stamps "added" when its callback runs, after the shell's
     first script, so a 2.5 s timer reads about 2.4 s). CONTROL: it is still up, not fading, at 1.2 s.
   - reduce: under prefers-reduced-motion a press removes it at once, with no fade.
   - plug-in: hello's answer with first_open false takes it away before any fade; with first_open true (the CONTROL) it fades on its timer. */
const SPLOG = `window.__splash = []; new MutationObserver(ms => { const t = performance.now(), is = n => n && n.classList && n.classList.contains('hd-splash');
  for (const m of ms) { m.addedNodes.forEach(n => { if (is(n)) window.__splash.push(['add', t]); }); m.removedNodes.forEach(n => { if (is(n)) window.__splash.push(['remove', t]); });
    if (m.type === 'attributes' && is(m.target) && m.target.classList.contains('out')) window.__splash.push(['out', t]); } }).observe(document, { subtree: true, childList: true, attributes: true, attributeFilter: ['class'] });`;
const SPLASH = `(() => { const s = document.querySelector('.hd-splash'), c = s && s.querySelector('canvas'), w = document.getElementById('win'); if (!s) return { up: false, log: window.__splash };
  const R = s.getBoundingClientRect(), B = w.getBoundingClientRect(), W = { left: B.left + w.clientLeft, top: B.top + w.clientTop, width: w.clientWidth, height: w.clientHeight },   // the window's inside: its border stays
    covers = Math.abs(R.left - W.left) < 1 && Math.abs(R.top - W.top) < 1 && Math.abs(R.width - W.width) < 1 && Math.abs(R.height - W.height) < 1;
  let op = 0, cy = 0, mg = 0; if (c && c.width) { const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data; for (let i = 3; i < d.length; i += 28) if (d[i] > 200) { op++; const r = d[i - 3], g = d[i - 2], b = d[i - 1]; if (b > 200 && g > 170 && r < 170) cy++; if (r > 200 && b > 200 && g < 150) mg++; } }
  return { up: true, out: s.classList.contains('out'), covers, op, cy, mg, log: window.__splash }; })()`;
const PAGE = `(document.querySelector('.page:not([hidden])') || {}).id || ''`, MIXAT = `(() => { const e = [...document.querySelectorAll('button,[role=tab]')].find(e => e.textContent.trim() === 'Mix'); if (!e) return null; const r = e.getBoundingClientRect(); return { x: Math.round(r.left + r.width / 2), y: Math.round(r.top + r.height / 2) }; })()`;
const span = (log, a, b) => { const x = log.find(e => e[0] === a), y = log.find(e => e[0] === b); return x && y ? Math.round(y[1] - x[1]) : null; };
async function click(b, at) { await b.call('Input.dispatchMouseEvent', { type: 'mouseMoved', x: at.x, y: at.y }); await b.call('Input.dispatchMouseEvent', { type: 'mousePressed', x: at.x, y: at.y, button: 'left', buttons: 1, clickCount: 1 }); await b.call('Input.dispatchMouseEvent', { type: 'mouseReleased', x: at.x, y: at.y, button: 'left', buttons: 0, clickCount: 1 }); }
async function runSplash(chrome, url, mode, hello) {
  const dir = await mkdtemp(join(tmpdir(), 'gui3-splash-')), b = await cdp(chrome, dir);
  try {
    if (mode === 'reduce') await b.call('Emulation.setEmulatedMedia', { features: [{ name: 'prefers-reduced-motion', value: 'reduce' }] });
    await b.call('Page.addScriptToEvaluateOnNewDocument', { source: SPLOG + (hello ? hello : '') });
    await b.call('Page.navigate', { url });
    let s = null; for (let i = 0; i < 120; i++) { await sleep(50); try { s = await b.evalIn(SPLASH); } catch { continue; } if (s.up || (s.log || []).some(e => e[0] === 'remove')) break; }
    for (let i = 0; i < 30 && s && s.up && !s.op && !s.out; i++) { await sleep(50); s = await b.evalIn(SPLASH); }   // the art decodes after it appears; a splash that never paints is still pressed
    const first = s || { up: false };
    if (mode === 'press' || mode === 'reduce') {
      const p0 = await b.evalIn(PAGE), at = await b.evalIn(MIXAT); if (!at || !first.up) return { first };
      await click(b, at); await sleep(60); const mid = await b.evalIn(SPLASH), p1 = await b.evalIn(PAGE);
      for (let i = 0; i < 120 && (await b.evalIn(SPLASH)).up; i++) await sleep(50);
      // the control: the tab measured again (the page can shift while it loads, and a stale point misses), and the switch polled for, not slept on
      // (a fixed 300 ms read red under load on main, the lead's report 2026-11-08: the splash had gone, the page hadn't switched yet)
      const gone = !(await b.evalIn(SPLASH)).up, at2 = await b.evalIn(MIXAT) || at; await click(b, at2);
      let p2 = p0; for (let i = 0; i < 60 && p2 === p0; i++) { await sleep(50); p2 = await b.evalIn(PAGE); }
      return { first, mid, swallowed: p1 === p0, gone, control: p2 !== p0 && /mix/i.test(p2), log: (await b.evalIn(SPLASH)).log };
    }
    for (let i = 0; i < 160 && !(await b.evalIn(`(window.__splash || []).some(e => e[0] === 'remove')`)); i++) await sleep(50);
    return { first, log: await b.evalIn('window.__splash'), csp: await b.evalIn('window.__csp || []').catch(() => []) };
  } finally { b.ws.close(); b.proc.kill(); await sleep(200); await rm(dir, { recursive: true, force: true }); }
}
function splashVerdict(sp, red) {
  const P = 'splash (GUI-54): ', f = sp.press.first;
  if (!f.up) red.push(P + 'no splash over the lab page at load');
  else {
    if (!f.covers) red.push(P + 'it does not cover the window');
    if (f.op < 1000 || f.cy < 50 || f.mg < 50) red.push(P + `the art is not painted (${f.op} opaque samples, ${f.cy} cyan, ${f.mg} magenta)`);
    if (!sp.press.swallowed) red.push(P + 'a click on the Mix tab under it changed the page: the first click must do nothing else');
    if (!sp.press.mid || !(sp.press.mid.out || !sp.press.mid.up)) red.push(P + 'a click did not start the fade');
    if (!sp.press.gone) red.push(P + 'it was still up 2 s after a click');
    if (!sp.press.control) red.push('CONTROL: ' + P + 'the same click on Mix once it had gone did not open Mix, so the swallow proves nothing');
  }
  const o = span(sp.timer.log, 'add', 'out'), still = sp.timer.log.filter(e => e[0] === 'out' || e[0] === 'remove').every(e => e[1] - sp.timer.log[0][1] > 1200);
  if (o === null || o < 2000 || o > 4000) red.push(P + `with no input the fade started ${o === null ? 'never' : o + ' ms'} after it appeared, not at 2.5 s`);
  if (!still) red.push('CONTROL: ' + P + 'it was gone or fading before 1.2 s with no input');
  const rl = sp.reduce.log || []; if (!sp.reduce.first.up || rl.some(e => e[0] === 'out') || !sp.reduce.gone) red.push(P + 'under reduced motion a press ' + (rl.some(e => e[0] === 'out') ? 'faded it instead of removing it at once' : 'did not remove it'));
  const n = sp.notFirst.log || [], y = sp.first.log || [], nr = span(n, 'add', 'remove');
  if (n.some(e => e[0] === 'out') || nr === null || nr > 2000) red.push(P + `on the plug-in, hello's first_open false ${nr === null ? 'never took it away' : 'took it away only after ' + (n.some(e => e[0] === 'out') ? 'a fade' : nr + ' ms')}`);
  const yo = span(y, 'add', 'out'); if (yo === null || yo < 2000) red.push('CONTROL: ' + P + `on the plug-in with first_open true it ${yo === null ? 'never faded' : 'faded at ' + yo + ' ms'}, not on its timer`);
  if (sp.first.first.up && (sp.first.first.op < 1000 || sp.first.first.mg < 50)) red.push(P + 'under the plug-in page’s policy the art is not painted');
  (sp.first.csp || []).concat(sp.notFirst.csp || []).forEach(v => red.push(P + 'blocked by the page policy: ' + v));
  return `${f.op} opaque samples`;
}

/* THE BRIDGE'S PAGE SIDE (B628, docs/proposals/gui-on-engine.md section 7). The page loads on GUI3_REPLAY, the frames the plug-in's
   bridge would hand it, through ReplayTransport, which runs the same decode as the plug-in's NativeTransport. Until the lead's
   recorded fixture exists (tools/labharness/fixtures/gui3-replay.json, used when present), the frames are SYNTHETIC, written here:
   three notes taking turns and a preset bank. Asserted: every message carries a seq; the first `want` asks Main's two
   oscillators with members and Osc 1's asks [1], and every want asks the morph; a rack row is dimmed and an engine row isn't;
   the morph switch is live (the plug-in plays the morph, GUI-59): a click sends morph.on, the Morph page shows what a
   frame's morph member reports, the frame's position holds (the pad follows host automation), and the Morph mode control reads
   the law a frame reports, Blend and back to Quantum; the bank's
   names reach the preset bar and its next button sends `preset`; and the feed's gates change as frames
   play. The control: the same load with the replay frozen must show ONE set of gates, or the detector can't see frames advance. */
/* The plug-in build's two steps (B628): gui3_inline.py, then embed_file.py's policy. Python, as the build is; a failure is red. */
function embedPage() {
  const tools = resolve(dirname(fileURLToPath(import.meta.url)), '..');
  const r = spawnSync('python3', ['-c', 'import sys; sys.path.insert(0, sys.argv[1]); import gui3_inline, embed_file; sys.stdout.write(embed_file.with_csp(gui3_inline.inline()))', tools], { encoding: 'utf8', maxBuffer: 1 << 26 });
  if (r.status !== 0) throw new Error('building the embedded page failed: ' + (r.stderr || '').trim().split('\n').slice(-1)[0]);
  return r.stdout;
}
const FIXTURE = join(dirname(fileURLToPath(import.meta.url)), 'fixtures/gui3-replay.json');
function syntheticFrames() {
  const keys = [60, 64, 67], fr = [];
  for (let i = 0; i < 30; i++) {
    const k = keys[Math.floor(i / 10)], ph = Array.from({ length: 8 }, (_, m) => (i * 0.05 + m / 8) % 1);   // θ in cycles
    fr.push({ observe: { sounding: true, meters: { osc1: { peak: 0.2, clip: false }, out: { peak: 0.25, clip: false } },
      voices: [{ level: 0.5, env: [1, 0, 0, 0], notes: { key: k, vel: 0.8, gate: true }, osc: [{ R: 0.6, psi: i * 0.1, frame: 0, phases: ph, levels: ph.map(() => 1), blades: [{ pos: 0.3, width: 0.1 }, null] }, null] }] } });
  }
  Object.assign(fr[0], { params: { 'voice.mode': 0 }, presets: { names: ['Glass horde pad', 'Crushed bells', 'Undertone cathedral'], index: 0 }, plugin: { build: 'synthetic' } });
  return fr;
}
const PROBE = `(async () => {
  const B = window.GUI3_BRIDGE, wait = ms => new Promise(r => setTimeout(r, ms)), sent = () => B.transport.sent, wants = () => sent().filter(m => m.type === 'want');
  const want0 = wants().slice(-1)[0] || null;
  const rack = window.GUI3_DATA.params.find(r => r.target === 'rack' && document.querySelector('[data-key="' + r.key + '"]'));
  const rackEl = rack && document.querySelector('[data-key="' + rack.key + '"]'), engEl = document.querySelector('[data-key="osc1.detune"]');
  const gates = new Set(); for (let i = 0; i < 14; i++) { const f = B.dev.feed(); if (f) gates.add(f.gates.join()); await wait(100); }
  const bar = document.querySelector('#presetHost .pb-name'), barName = bar ? bar.textContent : '';
  document.querySelectorAll('#presetHost .pb-step')[1]?.click(); await wait(50);
  const pre = sent().filter(m => m.type === 'preset').slice(-1)[0] || null;
  B.selectPage('osc1'); await wait(400); const want1 = wants().slice(-1)[0] || null;
  const ring = () => { const w = B.pics['osc1/osc1'] && B.pics['osc1/osc1'].swarm(); return w ? { engine: !!w.engine, phase: w.phase.map(x => +x.toFixed(5)), w1: w.blades[0] && +w.blades[0].w1.toFixed(5) } : null; };
  const ringA = ring(); await wait(450); const ringB = ring();
  const sw = document.getElementById('morphOn'), n0 = sent().length; sw.click(); await wait(80);
  const morphSent = sent().slice(n0).some(m => m.type === 'morph' && m.settings && m.settings['morph.on'] === 1);
  B.transport.push({ ack: sent().slice(-1)[0].seq, morph: { x: .3, y: .6, dropped: 1, unknown: 0, levels: [1, .25, 0, null], counters: { refusedWaypoints: 0, refusedLocks: 0, droppedInputs: 0, droppedEvents: 0 } } });
  B.selectPage('morph'); await wait(300); const mline = (document.getElementById('morphBridge') || {}).textContent || '', hostPos = [B.dev.morph().x, B.dev.morph().y].join();
  // The lead's guard (2026-11-07, a report the human could not recreate): the Morph mode control reads the law the plug-in reports, both ways
  const lawNow = () => { const e = [...document.querySelectorAll('[data-key="morph.law"] [role="radio"][aria-checked="true"]')].find(x => x.offsetParent); return e ? e.title : null; };
  const lawFrame = v => B.transport.push({ ack: sent().slice(-1)[0].seq, morph: { x: .3, y: .6, dropped: 1, unknown: 0, settings: { 'morph.law': v } } });
  const law0 = lawNow(); lawFrame(0); await wait(250); const lawBlend = lawNow(); lawFrame(.5); await wait(250); const lawQuantum = lawNow();
  sw.click(); await wait(80);
  return { seqs: sent().every(m => typeof m.seq === 'number'), want0, want1, gates: [...gates], barName, pre, morphSent, mline, hostPos, wantsMorph: wants().every(m => m.morph === true),
    law0, lawBlend, lawQuantum,
    morphOff: document.getElementById('morphOn').disabled, rackInert: !!rackEl && rackEl.classList.contains('inert') && +getComputedStyle(rackEl).opacity < 1,
    engLive: !!engEl && !engEl.classList.contains('inert'), ringA, ringB, notes: document.querySelectorAll('.inert-note').length };
})()`;
async function runReplay(chrome, url, frames, frozen) {
  const dir = await mkdtemp(join(tmpdir(), 'gui3-replay-')), b = await cdp(chrome, dir);
  try {
    await b.call('Page.addScriptToEvaluateOnNewDocument', { source: 'window.GUI3_REPLAY = ' + JSON.stringify(frames) + ';' + (frozen ? 'window.GUI3_REPLAY_FROZEN = true;' : '') +
      'window.__csp = []; document.addEventListener("securitypolicyviolation", e => window.__csp.push(e.violatedDirective + " " + (e.blockedURI || "inline")));' });
    await b.call('Page.navigate', { url });
    let ready = false; for (let i = 0; i < 60 && !ready; i++) { await sleep(500); try { ready = await b.evalIn(`/Ready/.test((document.getElementById('status') || {}).textContent || '') && !!window.GUI3_BRIDGE`); } catch {} }
    const thrown = b.events.filter(e => e.method === 'Runtime.exceptionThrown').map(e => (e.params.exceptionDetails.exception || {}).description || e.params.exceptionDetails.text);
    return { ready, thrown, p: ready ? await b.evalIn(PROBE) : null, csp: await b.evalIn('window.__csp || []').catch(() => ['unreadable']) };
  } finally { b.ws.close(); b.proc.kill(); await sleep(200); await rm(dir, { recursive: true, force: true }); }
}
/* THE PLUG-IN'S MODE (B628): the page loaded as the bridge loads it, with `hordeNativeSend` bound before it, and a stand-in bridge
   that answers `hello` with the first frame, its build and no mismatch. Asserted: the page is in plug-in mode (the lab's header,
   description and status are gone and the device sits at the view's top-left); hello went out first with shell-data's hash;
   the build label reads the bridge's build; a mismatch the bridge reports shows as a banner naming a row, and goes when cleared. */
const NATIVE = (frames, more) => `window.__sent = []; window.hordeNativeSend = t => { const m = JSON.parse(t); window.__sent.push(m);
  if (m.type === 'hello') setTimeout(() => window.hordeNativeSnapshot(Object.assign({}, ${JSON.stringify(frames[0])}, { ack: m.seq, hello: Object.assign({ build: 'stand-in', manifest_hash: m.manifest_hash }, ${JSON.stringify(more || {})}), mismatch: null, plugin: { build: 'stand-in' } })), 0); };`;
const NPROBE = `(async () => {
  const wait = ms => new Promise(r => setTimeout(r, ms)), R = e => e ? e.getBoundingClientRect() : null, vis = e => !!e && getComputedStyle(e).display !== 'none' && e.getClientRects().length > 0;
  const win = R(document.getElementById('win')), first = window.__sent[0] || null;
  const plugin = document.documentElement.classList.contains('plugin'), chrome = ['bar', 'sizeBar', 'status'].filter(id => vis(document.getElementById(id)));
  const build = (document.getElementById('buildLabel') || {}).textContent || '', bar = (document.querySelector('#presetHost .pb-name') || {}).textContent || '';
  window.hordeNativeSnapshot({ mismatch: { expected: window.GUI3_DATA.manifest_hash, got: 'ffff', keys: ['osc1.octave'] } }); await wait(150);
  const b = document.querySelector('.bridge-banner'), shown = vis(b) && /osc1\\.octave/.test(b.textContent);
  window.hordeNativeSnapshot({ mismatch: null }); await wait(150);
  return { plugin, chrome, top: win && Math.round(win.top), left: win && Math.round(win.left), w: win && Math.round(win.width), h: win && Math.round(win.height),
    first, hash: window.GUI3_DATA.manifest_hash, build, bar, shown, gone: !vis(b) };
})()`;
/* GUI-73 (the human, build 13: "the main page having to scroll for the XYs"). In the plug-in the bridge dims what this engine can't act
   on, and the line saying so stood on the page, so Main's stage lost 30 px and the XY ran 29 px past the window. Asserted, in plug-in
   mode on the stand-in bridge: Main's page area doesn't scroll; the morph pad and the XY are square and wholly inside it; the note shows,
   in the header beside the page's tabs, meeting no tab, the preset bar, the colours or the switches; and no note is left on a page.
   THE CONTROL: the same load with the 30 px line planted back on Main must read red, or the measure can't see a cut XY. */
const FIT = `(async () => {
  const wait = ms => new Promise(r => setTimeout(r, ms)), R = e => e ? e.getBoundingClientRect() : null, $ = id => document.getElementById(id);
  [...document.querySelectorAll('#pnav .pn-g')].find(b => b.textContent.trim() === 'Main')?.click(); await wait(500);
  const pages = $('pages'), pr = R(pages), inside = r => !!r && r.width > 0 && r.top >= pr.top - .5 && r.bottom <= pr.bottom + .5;
  const sq = e => { const r = R(e); return r && r.width > 0 ? { w: Math.round(r.width), h: Math.round(r.height), square: Math.abs(r.width - r.height) < 1, inside: inside(r) } : null; };
  const note = $('inertNote'), nr = note && !note.hidden ? R(note) : null, nav = R($('pnav'));
  const meet = (a, b) => a.left < b.right - .5 && b.left < a.right - .5 && a.top < b.bottom - .5 && b.top < a.bottom - .5;
  const others = [...document.querySelectorAll('#pnav .pn-g, #pnav .pn-p, .logo, .switches')].concat([$('presetHost'), $('colourHost')]).filter(Boolean);
  return { scroll: pages.scrollHeight - pages.clientHeight, pad: sq($('mainPad')), xy: sq($('mainXY')),
    note: nr ? { text: note.textContent, inHeader: !!nav && nr.top >= nav.top - .5 && nr.bottom <= nav.bottom + .5, clash: others.filter(e => meet(R(e), nr)).map(e => (e.id || e.className || e.tagName).toString().slice(0, 24)) } : null,
    onPages: pages.querySelectorAll('.inert-note').length };
})()`;
function fitVerdict(f, control, red) {
  if (!f) { red.push('Main fit (GUI-73): the plug-in-mode load never got far enough to measure Main'); return; }
  const bad = [];
  if (f.scroll > 0) bad.push(`its page area scrolls by ${f.scroll} px`);
  [['the morph pad', f.pad], ['the XY', f.xy]].forEach(([n, q]) => { if (!q) bad.push(n + ' is missing'); else if (!q.square || !q.inside) bad.push(`${n} is ${q.w} x ${q.h}${q.inside ? '' : ', not wholly inside the page'}`); });
  if (!f.note) bad.push('the dimmed-controls note is missing from the header');
  else { if (!f.note.inHeader) bad.push('the note is outside the header'); if (f.note.clash.length) bad.push('the note meets ' + f.note.clash.join(', ')); }
  if (f.onPages) bad.push(f.onPages + ' note(s) still on a page');
  if (bad.length) red.push('Main fit (GUI-73), in plug-in mode: ' + bad.join('; '));
  if (!control || !control.fit || !(control.fit.scroll > 0 || (control.fit.xy && !control.fit.xy.inside))) red.push('CONTROL: with a 30 px line planted back on Main, the fit check still read ' + (control && control.fit ? 'no scroll and the XY inside' : 'nothing') + ', so it can\u2019t see a cut XY');
}
/* GUI-74 (the human, 2026-11-25: B, "A plus a test strip", the filter in its own card before Scape). On the test plug-in's bridge
   (B711) the plug-in's own filter and Scape arrive as `preview.*` keys with descriptors in hello.preview, and the FX page draws a
   test strip, a Test filter card and Scape's card bound to them. Asserted, on a stand-in bridge that publishes the descriptors (Scape's
   keys read from shell-data's rack rows, so every rack control has one) and echoes each set:
     * the strip holds Scape on, Space, Decay, Tone, Mix, Era and the filter's six; the filter's card sits before Scape's; Scape's card
       has no rack control left and nothing dimmed;
     * a real mouse drag on Scape's Decay sends one gesture begin, sets on preview.scape.decay only, then one end;
     * a frame's value (host automation) repaints the knob;
     * PREVIEW-ONLY: after the drag, History's Undo is still off.
   CONTROLS: the same load with no hello.preview draws no strip and no filter card and leaves Scape's rack card dimmed; and a drag on
   a live engine knob (the first one showing on the Osc group's first page) turns Undo on, or the Undo reading proves nothing. */
const PV_DESC = (() => {
  const keys = [...new Set((readFileSync(join(ROOT, 'design/gui3/shell-data.js'), 'utf8').match(/"fx\.scape\.(verb|io)\.[a-zA-Z]+"/g) || []).map(k => k.slice(1, -1).replace(/^fx\.scape\.(verb|io)\./, '')))];
  const rows = [['on', 'Filter on', ['off', 'on']], ['mode', 'Filter mode', ['LP', 'BP', 'HP', 'Notch', 'Peak']], ['cutoff', 'Filter cutoff'], ['resonance', 'Filter resonance'], ['ring', 'Filter ring'], ['mix', 'Filter mix']]
    .map(([k, n, l]) => Object.assign({ key: 'preview.filter.' + k, name: n, min: 0, max: l ? l.length - 1 : 1, default: 0, stepped: !!l, unit: '' }, l ? { labels: l } : {}));
  rows.push({ key: 'preview.scape.on', name: 'Scape on', min: 0, max: 1, default: 0, stepped: true, labels: ['off', 'on'], unit: '' });
  keys.forEach(k => rows.push(k === 'era' || k === 'delayMotion' ? { key: 'preview.scape.' + k, name: 'Scape ' + k, min: 0, max: 1, default: 0, stepped: true, labels: k === 'era' ? ['clean', 'vintage'] : ['crossfade', 'glide'], unit: '' }
    : { key: 'preview.scape.' + k, name: 'Scape ' + k, min: k === 'decay' ? 0.2 : 0, max: k === 'decay' ? 12 : 1, default: k === 'decay' ? 2.2 : 0.5, stepped: false, unit: k === 'decay' ? 's' : '' }));
  return { value: 'v = (host - min) / (max - min)', rows };
})();
const NATIVE_PV = (frames, pv) => `window.__sent = []; window.hordeNativeSend = t => { const m = JSON.parse(t); window.__sent.push(m);
  const PV = ${JSON.stringify(pv)};
  if (m.type === 'hello') setTimeout(() => { const f = Object.assign({}, ${JSON.stringify(frames[0])}); if (PV) f.params = Object.assign({}, f.params || {}, Object.fromEntries(PV.rows.map(d => [d.key, (d.default - d.min) / (d.max - d.min)])));
    window.hordeNativeSnapshot(Object.assign(f, { ack: m.seq, hello: Object.assign({ build: 'stand-in', manifest_hash: m.manifest_hash, first_open: false }, PV ? { preview: PV } : {}), mismatch: null, plugin: { build: 'stand-in' } })); }, 0);
  if (m.type === 'set' && PV) { const pv = {}; for (const k in m.values) if (k.startsWith('preview.')) pv[k] = m.values[k]; if (Object.keys(pv).length) setTimeout(() => window.hordeNativeSnapshot({ ack: m.seq, params: pv }), 0); } };`;
const PV_OPEN = `(async () => {
  const wait = ms => new Promise(r => setTimeout(r, ms)), go = x => [...document.querySelectorAll('#pnav .pn-g')].find(b => b.textContent.trim() === x)?.click();
  go('FX'); await wait(900);
  const fx = document.getElementById('page-fx'), strip = fx.querySelector('.preview-strip'), sv = e => e ? [...e.querySelectorAll('[data-preview]')].map(x => x.dataset.preview) : [];
  const fCard = fx.querySelector('section[aria-label="FX · Test filter"]'), sCard = [...fx.querySelectorAll('section')].find(x => x.getAttribute('aria-label') === 'FX · Scape' || (x.querySelector('[data-key^="fx.scape."]') && !x.closest('.preview-sec')));
  const kn = sCard && sCard.querySelector('[data-preview="preview.scape.decay"]'); if (kn) kn.scrollIntoView({ block: 'center' }); await wait(200);
  const r = kn && kn.getBoundingClientRect();
  return { strip: sv(strip), filterCard: sv(fCard), before: !!(fCard && sCard && (fCard.compareDocumentPosition(sCard) & Node.DOCUMENT_POSITION_FOLLOWING)),
    scapeBound: sv(sCard).length, scapeRack: sCard ? sCard.querySelectorAll('[data-key^="fx.scape."]').length : -1, scapeInert: sCard ? sCard.querySelectorAll('.inert').length : -1,
    knob: r ? { x: Math.round(r.left + r.width / 2), y: Math.round(r.top + r.height / 2) } : null, mark: window.__sent.length };
})()`;
const PV_AFTER = mark => `(async () => {
  const wait = ms => new Promise(r => setTimeout(r, ms)), sent = window.__sent.slice(${mark});
  const seq = sent.filter(m => m.type === 'gesture' || m.type === 'set').map(m => m.type === 'gesture' ? m.phase + ':' + m.key : 'set:' + Object.keys(m.values).join(','));
  const last = window.__sent[window.__sent.length - 1].seq; window.hordeNativeSnapshot({ ack: last, params: { 'preview.scape.decay': 0.25 } }); await wait(300);
  const k = document.querySelector('#page-fx section[aria-label="FX · Scape"] [data-preview="preview.scape.decay"] [role="slider"]');
  const undo = () => { const b = [...document.querySelectorAll('.hist-bar button')].find(x => x.textContent.trim() === 'Undo'); return b ? !b.disabled : null; };
  [...document.querySelectorAll('#pnav .pn-g')].find(b => b.textContent.trim() === 'History')?.click(); await wait(400);
  return { seq, shown: k ? k.getAttribute('aria-valuetext') : null, undo: undo() };
})()`;
async function runPreview(chrome, url, frames, pv, engineDrag) {
  const dir = await mkdtemp(join(tmpdir(), 'gui3-preview-')), b = await cdp(chrome, dir);
  try {
    await b.call('Page.addScriptToEvaluateOnNewDocument', { source: NATIVE_PV(frames, pv) });
    await b.call('Page.navigate', { url });
    let ready = false; for (let i = 0; i < 60 && !ready; i++) { await sleep(500); try { ready = await b.evalIn(`!!(window.__sent || []).length && !!document.querySelector('#presetHost .pb-name')`); } catch {} }
    if (!ready) return { ready };
    await sleep(800);
    let at;
    if (engineDrag) {   // the control: a drag on an engine knob must turn Undo on
      await b.evalIn(`[...document.querySelectorAll('#pnav .pn-g')].find(b => b.textContent.trim() === 'Osc')?.click()`); await sleep(600);
      at = await b.evalIn(`(() => { const e = [...document.querySelectorAll('.page:not([hidden]) .ctl[data-w="knob"] [role="slider"]')].find(x => x.getClientRects().length && !x.closest('.inert, .idle')); if (!e) return null; e.scrollIntoView({ block: 'center' }); const r = e.getBoundingClientRect(); return { knob: { x: Math.round(r.left + r.width / 2), y: Math.round(r.top + r.height / 2) }, key: e.closest('[data-key]').dataset.key, mark: window.__sent.length }; })()`);   // the first live engine knob on the Osc group's first page
    } else at = await b.evalIn(PV_OPEN);
    if (at && at.knob) {
      const { x, y } = at.knob;
      await b.call('Input.dispatchMouseEvent', { type: 'mouseMoved', x, y });
      await b.call('Input.dispatchMouseEvent', { type: 'mousePressed', x, y, button: 'left', buttons: 1, clickCount: 1 });
      for (let i = 1; i <= 10; i++) await b.call('Input.dispatchMouseEvent', { type: 'mouseMoved', x, y: y - i * 4, button: 'left', buttons: 1 });
      await b.call('Input.dispatchMouseEvent', { type: 'mouseReleased', x, y: y - 40, button: 'left', buttons: 0, clickCount: 1 });
      await sleep(300);
    }
    return { ready, at, after: at ? await b.evalIn(PV_AFTER(at.mark)) : null };
  } finally { b.ws.close(); b.proc.kill(); await sleep(200); await rm(dir, { recursive: true, force: true }); }
}
function previewVerdict(r, none, eng, red) {
  const pre = 'Scape panel (GUI-74), on the bridge: ', want = ['preview.scape.on', 'preview.scape.size', 'preview.scape.decay', 'preview.scape.damp', 'preview.scape.mix', 'preview.scape.era', ...['on', 'mode', 'cutoff', 'resonance', 'ring', 'mix'].map(k => 'preview.filter.' + k)];
  if (!r.ready || !r.at) { red.push(pre + 'the page never got far enough to look'); return; }
  const a = r.at, bad = [], scapeKeys = PV_DESC.rows.filter(d => d.key.startsWith('preview.scape.') && d.key !== 'preview.scape.on').length;
  const miss = want.filter(k => !a.strip.includes(k)); if (miss.length) bad.push('the strip lacks ' + miss.join(', '));
  if (a.filterCard.length < 5) bad.push(`the Test filter card holds ${a.filterCard.length} controls, not 5`);
  if (!a.before) bad.push('the Test filter card is not before Scape’s');
  if (a.scapeBound !== scapeKeys || a.scapeRack || a.scapeInert) bad.push(`Scape’s card binds ${a.scapeBound} of ${scapeKeys}, with ${a.scapeRack} rack control(s) left and ${a.scapeInert} dimmed`);
  const q = r.after ? r.after.seq : [], sets = q.filter(x => x.startsWith('set:'));
  if (!a.knob) bad.push('no Decay knob to drag');
  else if (q[0] !== 'begin:preview.scape.decay' || q[q.length - 1] !== 'end:preview.scape.decay' || !sets.length || sets.some(x => x !== 'set:preview.scape.decay') || q.filter(x => x.startsWith('begin')).length !== 1) bad.push('a drag on Decay sent ' + JSON.stringify(q) + ', not one begin, its sets, one end');
  if (!r.after || !/3\.15/.test(r.after.shown || '')) bad.push('a frame’s Decay of 0.25 (3.15 s) shows as ' + JSON.stringify(r.after && r.after.shown));
  if (!r.after || r.after.undo !== false) bad.push('after the drag Undo reads ' + (r.after ? r.after.undo : 'nothing') + ', so a preview edit reached the patch’s history');
  if (bad.length) red.push(pre + bad.join('; '));
  if (!none.ready || !none.at || none.at.strip.length || none.at.filterCard.length || !none.at.scapeRack || !none.at.scapeInert) red.push('CONTROL: with no hello.preview the FX page still drew ' + JSON.stringify(none.at && { strip: none.at.strip.length, filter: none.at.filterCard.length, rack: none.at.scapeRack, inert: none.at.scapeInert }) + ', so the panel’s reading proves nothing');
  if (!eng.ready || !eng.after || eng.after.undo !== true) red.push('CONTROL: a drag on the engine knob ' + (eng.at && eng.at.key) + ' left Undo ' + (eng.after ? eng.after.undo : 'unread') + ', so "Undo stays off" proves nothing');
}
async function runNative(chrome, url, frames, noFrames, plantCss) {
  const dir = await mkdtemp(join(tmpdir(), 'gui3-native-')), b = await cdp(chrome, dir);
  try {
    // noFrames: a window that draws no frames (offscreen, or not yet attached: the lead's first window test). requestAnimationFrame never calls back.
    await b.call('Page.addScriptToEvaluateOnNewDocument', { source: (noFrames ? 'window.requestAnimationFrame = () => 0;' : '') + NATIVE(frames) + 'window.__csp = []; document.addEventListener("securitypolicyviolation", e => window.__csp.push(e.violatedDirective + " " + (e.blockedURI || "inline")));' +
      (plantCss ? `document.addEventListener('DOMContentLoaded', () => { const s = document.createElement('style'); s.textContent = ${JSON.stringify(plantCss)}; document.head.append(s); });` : '') });
    await b.call('Page.navigate', { url });
    let ready = false; for (let i = 0; i < 60 && !ready; i++) { await sleep(500); try { ready = await b.evalIn(noFrames ? `!!(window.__sent || []).length && !!document.querySelector('#presetHost .pb-name')` : `/Ready/.test((document.getElementById('status') || {}).textContent || '') && !!(window.__sent || []).length`); } catch {} }
    const thrown = b.events.filter(e => e.method === 'Runtime.exceptionThrown').map(e => (e.params.exceptionDetails.exception || {}).description || e.params.exceptionDetails.text);
    return { ready, thrown, n: ready ? await b.evalIn(NPROBE) : null, fit: ready && !noFrames ? await b.evalIn(FIT) : null, csp: await b.evalIn('window.__csp || []').catch(() => ['unreadable']) };
  } finally { b.ws.close(); b.proc.kill(); await sleep(200); await rm(dir, { recursive: true, force: true }); }
}
function nativeVerdict(r, red, frames, noFrames) {
  const first = frames.find(f => f.presets) || {}, names = (first.presets || {}).names || [], at = (first.presets || {}).index || 0;
  if (noFrames) {
    if (!r.ready) { red.push('plug-in mode with no frames: the page never sent hello'); return; }
    if (names.length && !r.n.bar.startsWith(names[at])) red.push(`plug-in mode with no frames: the preset bar reads "${r.n.bar}", not "${names[at]}" (the bank paints only on a frame)`);
    if (r.n.build !== 'Build stand-in') red.push('plug-in mode with no frames: the build label reads "' + r.n.build + '"');
    return;
  }
  if (names.length && r.ready && !r.n.bar.startsWith(names[at])) red.push(`plug-in mode: the preset bar reads "${r.n.bar}", not "${names[at]}"`);
  if (!r.ready) { red.push('plug-in mode: the page never said Ready with hordeNativeSend bound'); return; }
  r.thrown.forEach(t => red.push('plug-in mode, uncaught: ' + String(t).split('\\n')[0]));
  r.csp.forEach(v => red.push('plug-in mode, blocked by the page policy: ' + v));
  const n = r.n;
  if (!n.plugin || n.chrome.length) red.push('plug-in mode: the lab chrome still shows (' + (n.plugin ? n.chrome.join(', ') : 'no plugin class') + ')');
  if (n.top > 1 || n.left > 1) red.push(`plug-in mode: the device starts at ${n.left}, ${n.top}, not the view's top-left`);
  if (!n.first || n.first.type !== 'hello' || n.first.manifest_hash !== n.hash) red.push('plug-in mode: the first message was ' + JSON.stringify(n.first) + ', not hello with shell-data\\u2019s hash');
  if (n.build !== 'Build stand-in') red.push('plug-in mode: the build label reads "' + n.build + '"');
  if (!n.shown || !n.gone) red.push(`plug-in mode: a reported mismatch ${n.shown ? '' : 'never showed a banner naming its row'}${!n.shown || n.gone ? '' : ' stayed after it was cleared'}`);
}
function replayVerdict(r, rc, frames, red) {
  if (!r.ready) { red.push('replay: the page never said Ready on GUI3_REPLAY'); return; }
  r.thrown.forEach(t => red.push('replay, uncaught: ' + String(t).split('\n')[0]));
  r.csp.forEach(v => red.push('replay, blocked by the page policy: ' + v));
  // What the recording holds decides what is asserted: the lead's frames carry the engine as it is (preset 1, sparse source meters,
  // only sounding voices), the synthetic ones carry everything. Nothing is asserted that the frames can't show.
  const p = r.p, first = frames.find(f => f.presets) || {}, names = (first.presets || {}).names || [], at = (first.presets || {}).index || 0;
  const oscFrames = frames.map(f => ((((f.observe || {}).voices || [])[0] || {}).osc || [])[0]).filter(c => c && c.phases && c.phases.length);
  const noteKeys = new Set(frames.map(f => (((f.observe || {}).voices || []).map(v => v.notes && v.notes.gate ? v.notes.key : '').join())));
  if (!p.seqs) red.push('replay: a message went out without a seq');
  if (!p.want0 || JSON.stringify(p.want0.osc) !== '[1,2]' || !p.want0.members || p.want0.cycles) red.push('replay: Main’s want is ' + JSON.stringify(p.want0) + ', not osc [1,2] with members and no cycles');
  if (!p.want1 || JSON.stringify(p.want1.osc) !== '[1]') red.push('replay: Osc 1’s want is ' + JSON.stringify(p.want1));
  if (p.morphOff || !p.rackInert || !p.engLive || !p.notes) red.push(`replay: inert state wrong (morph switch disabled ${p.morphOff}, a rack row dimmed ${p.rackInert}, osc1.detune live ${p.engLive}, notes ${p.notes})`);
  if (!p.morphSent || !p.wantsMorph) red.push(`replay: the plug-in's morph isn't driven (a click on the switch sent morph.on ${p.morphSent}, every want asks the morph ${p.wantsMorph})`);
  if (p.hostPos !== '0.3,0.6') red.push('replay: a frame\u2019s position (host automation) didn\u2019t hold: the morph reads ' + p.hostPos + ', so the pad pushed its own back');
  if (p.law0 !== 'Quantum' || p.lawBlend !== 'Blend' || p.lawQuantum !== 'Quantum') red.push(`replay: the Morph mode control doesn\u2019t read the plug-in\u2019s law: ${p.law0} at the start (D.2's Quantum), ${p.lawBlend} after a frame reporting Blend, ${p.lawQuantum} after one reporting Quantum`);
  if (!/Osc 2 25 %/.test(p.mline) || !/Refused edits 1\b/.test(p.mline)) red.push('replay: the Morph page doesn\u2019t show the frame\u2019s morph report: "' + p.mline + '"');
  if (names.length && (!p.barName.startsWith(names[at]) || !p.pre || p.pre.index !== (at + 1) % names.length)) red.push(`replay: the preset bar reads "${p.barName}", not "${names[at]}" (index ${at}), or next sent ${JSON.stringify(p.pre)}`);
  const recordedPhases = new Set(frames.flatMap(f => (((f.observe || {}).voices || [])[0] || { osc: [] }).osc.filter(Boolean).map(c => (c.phases || []).map(x => +(+x).toFixed(5)).join())));
  if (oscFrames.length && (!p.ringA || !p.ringA.engine || !recordedPhases.has(p.ringA.phase.join()))) red.push('replay: Osc 1\u2019s ring isn\u2019t drawing a recorded frame\u2019s members: ' + JSON.stringify(p.ringA));
  if (new Set(oscFrames.map(c => c.phases.join())).size > 1 && p.ringA && p.ringB && p.ringA.phase.join() === p.ringB.phase.join()) red.push('replay: Osc 1\u2019s ring held still while frames played');
  if (oscFrames.length && rc.p && (!rc.p.ringA || !rc.p.ringB || rc.p.ringA.phase.join() !== rc.p.ringB.phase.join())) red.push('CONTROL: with the replay frozen, Osc 1\u2019s ring still changed, so a moving ring proves nothing');
  if (noteKeys.size > 1 && p.gates.length < 2) red.push('replay: the feed’s gates never changed as frames played (' + p.gates.join(' | ') + ')');
  if (!rc.ready || !rc.p || (noteKeys.size > 1 && rc.p.gates.length > 1)) red.push('CONTROL: with the replay frozen the feed showed ' + (rc.p ? rc.p.gates.length : 'no') + ' sets of gates, not one, so a moving feed proves nothing');
}

const chrome = findChrome();
if (!chrome) { console.log('gui3_browser_check: HOLE -- no headless Chrome found (set HORDE_CHROME); nothing was checked'); process.exit(3); }
const srv = await serve(), base = `http://127.0.0.1:${srv.address().port}/design/gui3.html`;
let red = [];
try {
  const r = await run(chrome, base + '#osc1');
  if (!r.st || !r.st.ready) red.push('the page never finished its self-checks');
  if (r.st && !r.st.ok) red.push('no self-check printed OK');
  if (r.st) r.st.failed.forEach(f => red.push('self-check: ' + f));
  r.thrown.forEach(t => red.push('uncaught: ' + String(t).split('\n')[0]));
  const still = MOVERS.filter(n => !r.pics.some(p => p.name.startsWith(n) && p.moved));
  if (still.length) red.push('frozen after the self-checks: ' + still.join(', '));
  r.sel = await runSelect(chrome, base + '?fast#osc1');
  if (!r.sel || r.sel.missing) red.push('text selection (GUI-61 r3): the probe found no ' + (r.sel ? r.sel.missing : 'page') + ' to drag onto');
  else {
    [['a knob', r.sel.knob], ['the Morph pad', r.sel.pad]].forEach(([n, d]) => {
      if (!d) red.push(`text selection (GUI-61 r3): ${n} or a paragraph beside it was not found`);
      else if (d.sel || !d.held || !d.cleared) red.push(`text selection (GUI-61 r3): ${n} dragged onto a paragraph ${d.held ? '' : 'left the page selectable mid-drag, '}${d.cleared ? '' : 'left it unselectable after release, '}${d.sel ? 'selected ' + d.sel + ' characters' : ''}`.replace(/, $/, ''));
    });
    if (!r.sel.control || !r.sel.control.sel || r.sel.control.held) red.push('CONTROL: a drag across the paragraph itself ' + (r.sel.control && r.sel.control.held ? 'set hd-nosel' : 'selected nothing') + ', so the guard\u2019s reading proves nothing');
  }
  const c = await run(chrome, base + '#osc1', FREEZE), seen = MOVERS.filter(n => c.pics.some(p => p.name.startsWith(n) && p.moved));
  if (seen.length) red.push('CONTROL: with frames stopped the detector still read motion in ' + seen.join(', ') + ', so it can’t tell stillness');
  const recorded = existsSync(FIXTURE), frames = recorded ? JSON.parse(await readFile(FIXTURE, 'utf8')) : syntheticFrames();
  embedded = embedPage();
  const eb = base.replace('/design/gui3.html', EMBEDDED);
  replayVerdict(await runReplay(chrome, eb, frames, false), await runReplay(chrome, eb, frames, true), frames, red);
  const nat = await runNative(chrome, eb, frames); nativeVerdict(nat, red, frames);
  const fitPlant = await runNative(chrome, eb, frames, false, '#page-main::before { content: ""; display: block; flex: 0 0 30px; height: 30px; }'); fitVerdict(nat.fit, fitPlant, red);
  previewVerdict(await runPreview(chrome, eb, frames, PV_DESC), await runPreview(chrome, eb, frames, null), await runPreview(chrome, eb, frames, PV_DESC, true), red);
  nativeVerdict(await runNative(chrome, eb, frames, true), red, frames, true);
  const csp = 'window.__csp = []; document.addEventListener("securitypolicyviolation", e => window.__csp.push(e.violatedDirective + " " + (e.blockedURI || "inline")));';
  const sp = { press: await runSplash(chrome, base + '?fast#osc1', 'press'), timer: await runSplash(chrome, base + '?fast#osc1', 'timer'), reduce: await runSplash(chrome, base + '?fast#osc1', 'reduce'),
    notFirst: await runSplash(chrome, eb, 'plugin', NATIVE(frames, { first_open: false }) + csp), first: await runSplash(chrome, eb, 'plugin', NATIVE(frames, { first_open: true }) + csp) };
  splashVerdict(sp, red);
  console.log(red.length ? 'gui3_browser_check: RED\n  ' + red.join('\n  ')
    : `gui3_browser_check: GREEN -- ${r.st.ok} self-checks pass in a real browser, ${MOVERS.join(', ')} move after them, and with frames stopped they read frozen (control); ` +
      `a knob and the Morph pad dragged onto a paragraph make the page unselectable mid-drag and selectable again on release, where a drag across the paragraph sets nothing and selects ${r.sel.control.sel} characters (control; headless Chrome can't show the WebKit bug itself); ` +
      `the Mindlathe splash covers the window painted (under the plug-in's policy too), swallows the first click and fades, fades on its own at 2.5 s, goes at once under reduced motion, and goes at once when hello says it isn't the first opening (controls: a later click opens Mix, it's still up at 1.2 s, and first_open true keeps it); ` +
      `the plug-in's one-file page runs under its policy with no violation, its bridge side holds on ${recorded ? 'the recorded' : 'SYNTHETIC'} frames, and a frozen replay reads frozen (control); ` +
      `in plug-in mode the ${nat.n.w} x ${nat.n.h} device fills the view's top-left with no lab chrome, says hello with its hash, shows and clears a mismatch, and paints its bank and build with no frames drawn; Main fits the window there with no scroll, its pad ${nat.fit.pad.w} px and XY ${nat.fit.xy.w} px square and inside, the dimmed-controls note in the header meeting nothing (control: a 30 px line planted back on Main reads red); on the bridge's preview keys the FX page draws the test strip, the Test filter card before Scape's and Scape's card bound to them, a Decay drag is one gesture of preview sets, a frame repaints it and Undo stays off (controls: no hello.preview draws none of it, and an engine drag turns Undo on)`);
} finally { srv.close(); }
process.exit(red.length ? 1 : 0);
