/*
 * gui3_browser_check (PROTOTYPE) — GUI 3 in a real browser: its self-checks pass, and its pictures still move after them.
 * A proposal, not a gate (docs/design/gui3/browser-gate-proposal.md). It lives here, outside tools/, until the human picks where it runs: an
 * unwired check under tools/ counts as weakening the gate set (weakening_check), which is the human's to approve. On a ruling it moves to
 * tools/labharness/gui3_browser_check.mjs and is wired in the same PR.
 *
 *   node docs/design/gui3/browser-check-prototype.mjs       the check and its control
 *   HORDE_CHROME=/path/to/chrome node ...                  use that browser
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
import { spawn } from 'node:child_process';
import { createServer } from 'node:http';
import { readFile, mkdtemp, rm } from 'node:fs/promises';
import { existsSync, readdirSync } from 'node:fs';
import { join, extname, resolve, dirname } from 'node:path';
import { tmpdir, homedir } from 'node:os';
import { fileURLToPath } from 'node:url';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../..');   // docs/, from docs/design/gui3/
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
function serve() {
  const srv = createServer(async (req, res) => {
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
  console.log(red.length ? 'gui3_browser_check: RED\n  ' + red.join('\n  ')
    : `gui3_browser_check: GREEN -- ${r.st.ok} self-checks pass in a real browser, ${MOVERS.join(', ')} move after them, and with frames stopped they read frozen (control)`);
} finally { srv.close(); }
process.exit(red.length ? 1 : 0);
