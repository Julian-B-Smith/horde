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
import { existsSync, readdirSync } from 'node:fs';
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

/* THE BRIDGE'S PAGE SIDE (B628, docs/proposals/gui-on-engine.md section 7). The page loads on GUI3_REPLAY, the frames the plug-in's
   bridge would hand it, through ReplayTransport, which runs the same decode as the plug-in's NativeTransport. Until the lead's
   recorded fixture exists (tools/labharness/fixtures/gui3-replay.json, used when present), the frames are SYNTHETIC, written here:
   three notes taking turns, a preset bank and Voices 6. Asserted: every message carries a seq; the first `want` asks Main's two
   oscillators with members and Osc 1's asks [1]; the morph switch and a rack row are dimmed and an engine row isn't; the bank's
   names reach the preset bar and its next button sends `preset`; Voices reads the frame's 6; and the feed's gates change as frames
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
  Object.assign(fr[0], { params: { 'voice.mode': 0 }, presets: { names: ['Glass horde pad', 'Crushed bells', 'Undertone cathedral'], index: 0 }, plugin: { voices: 6 } });
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
  return { seqs: sent().every(m => typeof m.seq === 'number'), want0, want1, gates: [...gates], barName, pre,
    morphOff: document.getElementById('morphOn').disabled, rackInert: !!rackEl && rackEl.classList.contains('inert') && +getComputedStyle(rackEl).opacity < 1,
    engLive: !!engEl && !engEl.classList.contains('inert'), ringA, ringB, voices: (document.querySelector('#voicesHost output') || {}).textContent, notes: document.querySelectorAll('.inert-note').length };
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
function replayVerdict(r, rc, frames, red) {
  if (!r.ready) { red.push('replay: the page never said Ready on GUI3_REPLAY'); return; }
  r.thrown.forEach(t => red.push('replay, uncaught: ' + String(t).split('\n')[0]));
  r.csp.forEach(v => red.push('replay, blocked by the page policy: ' + v));
  const p = r.p, names = (frames[0].presets || {}).names || [];
  if (!p.seqs) red.push('replay: a message went out without a seq');
  if (!p.want0 || JSON.stringify(p.want0.osc) !== '[1,2]' || !p.want0.members || p.want0.cycles) red.push('replay: Main’s want is ' + JSON.stringify(p.want0) + ', not osc [1,2] with members and no cycles');
  if (!p.want1 || JSON.stringify(p.want1.osc) !== '[1]') red.push('replay: Osc 1’s want is ' + JSON.stringify(p.want1));
  if (!p.morphOff || !p.rackInert || !p.engLive || !p.notes) red.push(`replay: inert state wrong (morph switch disabled ${p.morphOff}, a rack row dimmed ${p.rackInert}, osc1.detune live ${p.engLive}, notes ${p.notes})`);
  if (names.length && (!p.barName.startsWith(names[0]) || !p.pre || p.pre.index !== 1)) red.push(`replay: the bank didn't reach the preset bar ("${p.barName}"), or next sent ${JSON.stringify(p.pre)}`);
  if (frames[0].plugin && p.voices !== String(frames[0].plugin.voices)) red.push('replay: Voices reads ' + p.voices + ', not ' + frames[0].plugin.voices);
  const recordedPhases = new Set(frames.flatMap(f => (((f.observe || {}).voices || [])[0] || { osc: [] }).osc.filter(Boolean).map(c => (c.phases || []).map(x => +(+x).toFixed(5)).join())));
  if (!p.ringA || !p.ringA.engine || !recordedPhases.has(p.ringA.phase.join())) red.push('replay: Osc 1\u2019s ring isn\u2019t drawing a recorded frame\u2019s members: ' + JSON.stringify(p.ringA));
  if (p.ringA && p.ringB && p.ringA.phase.join() === p.ringB.phase.join()) red.push('replay: Osc 1\u2019s ring held still while frames played');
  if (rc.p && (!rc.p.ringA || !rc.p.ringB || rc.p.ringA.phase.join() !== rc.p.ringB.phase.join())) red.push('CONTROL: with the replay frozen, Osc 1\u2019s ring still changed, so a moving ring proves nothing');
  if (p.gates.length < 2) red.push('replay: the feed’s gates never changed as frames played (' + p.gates.join(' | ') + ')');
  if (!rc.ready || !rc.p || rc.p.gates.length !== 1) red.push('CONTROL: with the replay frozen the feed showed ' + (rc.p ? rc.p.gates.length : 'no') + ' sets of gates, not one, so a moving feed proves nothing');
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
  const c = await run(chrome, base + '#osc1', FREEZE), seen = MOVERS.filter(n => c.pics.some(p => p.name.startsWith(n) && p.moved));
  if (seen.length) red.push('CONTROL: with frames stopped the detector still read motion in ' + seen.join(', ') + ', so it can’t tell stillness');
  const recorded = existsSync(FIXTURE), frames = recorded ? JSON.parse(await readFile(FIXTURE, 'utf8')) : syntheticFrames();
  embedded = embedPage();
  const eb = base.replace('/design/gui3.html', EMBEDDED);
  replayVerdict(await runReplay(chrome, eb, frames, false), await runReplay(chrome, eb, frames, true), frames, red);
  console.log(red.length ? 'gui3_browser_check: RED\n  ' + red.join('\n  ')
    : `gui3_browser_check: GREEN -- ${r.st.ok} self-checks pass in a real browser, ${MOVERS.join(', ')} move after them, and with frames stopped they read frozen (control); ` +
      `the plug-in's one-file page runs under its policy with no violation, its bridge side holds on ${recorded ? 'the recorded' : 'SYNTHETIC'} frames, and a frozen replay reads frozen (control)`);
} finally { srv.close(); }
process.exit(red.length ? 1 : 0);
