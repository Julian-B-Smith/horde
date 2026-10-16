/*
 * h2_ab_page.mjs — the A/B page's behaviour (tools/h2_ab.mjs writes the page around it) and the probes that
 * hold it without a browser. ROADMAP B488 (0), ADR-220 item 2.
 *
 * pageMain(document, env) IS THE PAGE'S SCRIPT: its source is shipped as `(pageMain)(document, window)`. It is
 * a real function, not a string, so the probes below run it against stub objects with no evaluator
 * (tools/labharness/sandbox_check.mjs flags any `new Function`, vm, eval or non-literal import() under tools/).
 * `env` is everything it takes from the browser: AudioContext, fetch, crypto, requestAnimationFrame.
 *
 * PLAYBACK. Per row, both WAVs are fetched and decoded once, on that row's first Play. Play starts two looping
 * AudioBufferSourceNodes at ONE `when` on one AudioContext, each into its own GainNode. SWITCH NEVER SEEKS OR
 * RESTARTS: it ramps the two gains across each other over 10 ms, so the flip is click-free and at one sample.
 * The level match is the gain on B (it may exceed 1: B is raised to A). Why not media elements: two of them
 * kept in step by seeks cannot be made sample-tight, and a seek on a playing one restarted the sample.
 * FALLBACK. Opened from file:// the fetch fails; the page then plays through <audio> elements (volume only,
 * the silent one lined up with the audible one), says so on the page, and the switch is not sample-tight.
 * BLIND MODE. Turning it on draws, per row, whether X is A or B from crypto.getRandomValues; the listener marks a
 * preference per row; "reveal" shows the mapping and a tally. Nothing is stored.
 *
 * The probes at the foot are run by tools/h2_ab.mjs --self-test (control (d); ./verify fast and full).
 */

export const PAGE_CSS = `:root{color-scheme:light dark;--bg:#fafafa;--fg:#1c1c1c;--dim:#666;--line:#d4d4d4;--card:#fff;--acc:#1f6feb;--accfg:#fff;--hot:#c2410c}
@media (prefers-color-scheme:dark){:root{--bg:#161616;--fg:#e8e8e8;--dim:#9a9a9a;--line:#333;--card:#1f1f1f;--acc:#3b82f6;--accfg:#fff;--hot:#fb923c}}
[hidden]{display:none!important}
body{font:16px/1.45 system-ui,sans-serif;background:var(--bg);color:var(--fg);margin:0 auto;padding:1rem 1.25rem 4rem;max-width:46rem}
h1{font-size:1.3rem;margin:.2rem 0}h2{font-size:1.15rem;margin:2rem 0 .4rem;padding-bottom:.3rem;border-bottom:2px solid var(--line)}
code{font-family:ui-monospace,Menlo,monospace;font-size:.9em}.dim{color:var(--dim)}
table{border-collapse:collapse;width:100%;font-size:.85rem}td,th{padding:.15rem .5rem .15rem 0;text-align:left}
button{font:inherit;padding:.45rem 1.1rem;border:1px solid var(--line);background:transparent;color:var(--fg);border-radius:6px;cursor:pointer}
button:hover{border-color:var(--acc)}button.chosen{background:var(--acc);color:var(--accfg);border-color:var(--acc)}
.bar{display:flex;gap:1.2rem;align-items:center;flex-wrap:wrap;margin:.8rem 0}.keys{font-size:.9rem}
kbd{font:inherit;font-family:ui-monospace,Menlo,monospace;font-size:.85em;border:1px solid var(--line);border-bottom-width:2px;border-radius:4px;padding:0 .35rem;background:var(--card)}
.note{border-left:4px solid var(--hot);padding:.4rem .8rem;margin:.8rem 0;background:var(--card)}
#tally{font-weight:600}
.row{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:1rem 1.1rem;margin:.9rem 0;min-height:11rem;scroll-margin:1rem}
.row.focus{outline:3px solid var(--acc);outline-offset:2px}
.row .what{font-weight:600}.row .diff{font-size:.9rem;color:var(--dim);margin-top:.15rem}
.sides{display:flex;gap:.8rem;margin:.8rem 0}
.side{flex:1;text-align:center;font-size:2.2rem;font-weight:700;padding:.6rem .4rem;border:2px solid var(--line);border-radius:8px;color:var(--dim);transition:background .08s}
.side small{display:block;font-size:.8rem;font-weight:400}
.side.on{background:var(--acc);border-color:var(--acc);color:var(--accfg)}
.ctl{display:flex;gap:.6rem;align-items:center;flex-wrap:wrap}
.track{position:relative;height:.6rem;margin:1rem 0 .3rem;background:var(--line);border-radius:3px}
.ph{position:absolute;top:-.25rem;bottom:-.25rem;left:0;width:3px;background:var(--hot);border-radius:2px}
.row.playing .track{background:color-mix(in srgb,var(--acc) 35%,var(--line))}
.blind:not(.revealed) .dir{display:none}`;

export function pageMain(document, env) {
  const $ = id => document.getElementById(id);
  const match = $('match'), blindBox = $('blind'), revealBtn = $('reveal'), tally = $('tally'), note = $('note');
  const rows = [...document.querySelectorAll('.row')].map((el, i) => {
    const q = s => el.querySelector(s);
    return { i, el, a: q('audio.a'), b: q('audio.b'), urls: { A: el.dataset.a, B: el.dataset.b }, gain: parseFloat(el.dataset.gain) || 1,
      pp: q('.pp'), sw: q('.sw'), side: [q('.side0'), q('.side1')], pref: [q('.pref0'), q('.pref1')], prefBox: q('.prefs'),
      ph: q('.ph'), t: q('.t'), st: q('.st'),
      real: 'A',           // the side that is audible: A or B
      map: null,           // blind: real side -> label, { A: 'X', B: 'Y' } or the reverse; null when not blind
      choice: null,        // blind: the label the listener preferred
      buf: null, loading: null, src: null, g: null, t0: 0, dur: 0, playing: false, starting: false };
  });
  let mode = 'wa';       // 'wa': Web Audio; 'el': <audio> elements (the fetch failed: file://)
  let ctx = null, current = null, token = 0, framing = false, revealed = false, focus = 0;
  const RAMP = 0.01;     // seconds: the crossfade of a Switch, a level change and a Stop
  const SIDES = ['A', 'B'];
  const other = s => (s === 'A' ? 'B' : 'A');

  /* ---- what is shown */
  // the real side behind the i-th block: blocks are in label order (A, B; or X, Y when blind)
  const sideAt = (r, i) => (r.map ? (r.map.A === (i === 0 ? 'X' : 'Y') ? 'A' : 'B') : (i === 0 ? 'A' : 'B'));
  const labelOf = (r, s) => (r.map && !revealed ? r.map[s] : s);
  function drawSides(r) {
    for (let i = 0; i < 2; i++) {
      const s = sideAt(r, i);
      r.side[i].textContent = r.map && revealed ? s + ' (heard as ' + r.map[s] + ')' : labelOf(r, s);
      r.side[i].classList.toggle('on', s === r.real);
    }
    r.prefBox.hidden = !r.map || revealed;
    for (let k = 0; k < 2; k++) r.pref[k].classList.toggle('chosen', r.choice === (k === 0 ? 'X' : 'Y'));
  }
  const dur = r => (mode === 'wa' ? r.dur : (isFinite(r.a.duration) ? r.a.duration : 0));
  const audible = r => (r.real === 'A' ? r.a : r.b);
  const pos = r => {
    if (mode === 'el') return audible(r).currentTime;
    const d = r.dur;
    return d ? Math.max(0, ctx.currentTime - r.t0) % d : 0;   // the context clock modulo the loop
  };
  function draw(r) {
    const d = dur(r), p = pos(r);
    r.ph.style.left = (d ? 100 * p / d : 0) + '%';
    r.t.textContent = p.toFixed(1) + ' / ' + d.toFixed(1) + ' s';
  }
  function frame() {
    if (!current) { framing = false; return; }
    draw(current);
    env.requestAnimationFrame(frame);
  }
  function setPlaying(r, on) {
    r.playing = on; r.starting = false;
    current = on ? r : (current === r ? null : current);
    r.el.classList.toggle('playing', on);
    r.pp.textContent = on ? 'Stop' : 'Play';
    if (on && !framing) { framing = true; env.requestAnimationFrame(frame); }
    if (!on) { r.ph.style.left = '0%'; }
  }

  /* ---- levels: the gain of a side. A is the reference; B carries the match (it may be above 1) */
  const level = (r, s) => (s !== r.real ? 0 : (s === 'B' && match.checked ? r.gain : 1));
  // Web Audio: every change of the gains is a 10 ms ramp from where the gain is now
  function ramp(r) {
    if (!r.g) return;
    const t = ctx.currentTime;
    for (const s of SIDES) {
      const p = r.g[s].gain;
      p.cancelScheduledValues(t); p.setValueAtTime(p.value, t); p.linearRampToValueAtTime(level(r, s), t + RAMP);
    }
  }
  // elements: volume cannot exceed 1, so the louder side is turned down instead of B up
  function levelsEl(r) {
    const g = match.checked ? r.gain : 1;
    let va = 1, vb = g;
    if (vb > 1) { va = 1 / vb; vb = 1; }
    r.a.volume = r.real === 'A' ? va : 0; r.b.volume = r.real === 'B' ? vb : 0;
  }
  const relevel = r => (mode === 'wa' ? ramp(r) : levelsEl(r));

  /* ---- Web Audio playback */
  function load(r) {
    if (!r.loading) {
      r.loading = Promise.all(SIDES.map(s => Promise.resolve().then(() => env.fetch(r.urls[s])).then(res => {
        if (!res.ok) throw new Error('HTTP ' + res.status);
        return res.arrayBuffer();
      }).then(ab => ctx.decodeAudioData(ab)))).then(bufs => { r.buf = { A: bufs[0], B: bufs[1] }; r.dur = bufs[0].duration; });
    }
    return r.loading;
  }
  function startWA(r) {
    const when = ctx.currentTime + 0.05;   // one start time for both: the same sample of the same clock
    r.src = {}; r.g = {};
    for (const s of SIDES) {
      const g = ctx.createGain(); g.gain.value = level(r, s); g.connect(ctx.destination);
      const n = ctx.createBufferSource(); n.buffer = r.buf[s]; n.loop = true; n.connect(g);   // the whole render, tail included
      r.g[s] = g; r.src[s] = n;
    }
    r.src.A.start(when); r.src.B.start(when);
    r.t0 = when;
    setPlaying(r, true);
  }
  function stopWA(r) {
    const t = ctx.currentTime;
    for (const s of SIDES) {
      const p = r.g[s].gain;
      p.cancelScheduledValues(t); p.setValueAtTime(p.value, t); p.linearRampToValueAtTime(0, t + RAMP);
      r.src[s].stop(t + RAMP + 0.005);
    }
    r.src = null; r.g = null;
  }

  /* ---- element playback (the fallback) */
  function playEl(r) {
    r.a.loop = true; r.b.loop = true;
    levelsEl(r);
    const au = audible(r);
    (au === r.a ? r.b : r.a).currentTime = au.currentTime;   // both paused: line the silent one up
    Promise.all([r.a.play(), r.b.play()]).catch(e => { r.st.textContent = String(e); });
    setPlaying(r, true);
  }
  // the silent element follows the audible one, never the other way round; a seek on the silent one cannot be heard
  function align(r) {
    const au = audible(r), hid = au === r.a ? r.b : r.a;
    if (!au.paused && Math.abs(hid.currentTime - au.currentTime) > 0.02) hid.currentTime = au.currentTime;
  }

  /* ---- play, stop, switch */
  function stop(r) {
    token++; r.starting = false; r.st.textContent = '';
    if (!r.playing) return;
    if (mode === 'wa') stopWA(r); else { r.a.pause(); r.b.pause(); }
    setPlaying(r, false);
  }
  function play(r) {
    rows.forEach(o => { if (o !== r && (o.playing || o.starting)) stop(o); });   // one row at a time
    const mine = ++token;
    if (mode === 'el') { playEl(r); return; }
    if (!ctx) { const AC = env.AudioContext || env.webkitAudioContext; ctx = new AC(); }
    if (ctx.state === 'suspended') ctx.resume();
    r.starting = true; r.st.textContent = 'loading...';
    load(r).then(() => { if (mine !== token) return; r.st.textContent = ''; startWA(r); }, () => {
      if (mine !== token) return;
      mode = 'el'; note.hidden = false; r.st.textContent = '';   // fetch failed (file://): the elements play instead
      playEl(r);
    });
  }
  const toggle = r => { if (r.playing || r.starting) stop(r); else play(r); };
  // SWITCH ONLY MOVES THE GAINS (or, in the fallback, the volumes): no seek, no start, no stop, no new node.
  const doSwitch = r => { r.real = other(r.real); drawSides(r); relevel(r); };

  /* ---- blind mode */
  function tallyText() {
    let a = 0, b = 0, n = 0;
    rows.forEach(r => { if (!r.choice) n++; else if (r.map.A === r.choice) a++; else b++; });
    return 'Preferred A in ' + a + ' row(s), preferred B in ' + b + ' row(s), no preference in ' + n + '.';
  }
  blindBox.onchange = () => {
    const on = blindBox.checked;
    revealed = false; tally.textContent = ''; revealBtn.hidden = !on;
    document.body.classList.toggle('blind', on); document.body.classList.remove('revealed');
    if (on) {
      const bits = new Uint8Array(rows.length);
      env.crypto.getRandomValues(bits);   // not the page's seeded RNG: the listener cannot guess it
      rows.forEach((r, i) => {
        r.map = (bits[i] & 1) ? { A: 'Y', B: 'X' } : { A: 'X', B: 'Y' };
        r.choice = null;
        r.real = r.map.A === 'X' ? 'A' : 'B';   // every row opens on X, whichever build that is
        relevel(r);
      });
    } else rows.forEach(r => { r.map = null; r.choice = null; });
    rows.forEach(drawSides);
    if (blindBox.blur) blindBox.blur();
  };
  revealBtn.onclick = () => { revealed = true; document.body.classList.add('revealed'); tally.textContent = tallyText(); rows.forEach(drawSides); };

  /* ---- wiring */
  const click = (el, fn) => { el.onclick = () => { fn(); if (el.blur) el.blur(); }; };   // a focused button would take Space
  function setFocus(i, scroll) {
    focus = Math.max(0, Math.min(rows.length - 1, i));
    rows.forEach((r, k) => r.el.classList.toggle('focus', k === focus));
    if (scroll && rows[focus].el.scrollIntoView) rows[focus].el.scrollIntoView({ block: 'nearest' });
  }
  rows.forEach(r => {
    click(r.pp, () => toggle(r));
    click(r.sw, () => doSwitch(r));
    r.pref.forEach((b, k) => click(b, () => { if (revealed) return; r.choice = k === 0 ? 'X' : 'Y'; drawSides(r); }));
    r.el.onclick = () => setFocus(r.i);
    r.a.ontimeupdate = r.b.ontimeupdate = () => { if (mode === 'el') align(r); };
    drawSides(r);
  });
  match.onchange = () => { rows.forEach(relevel); if (match.blur) match.blur(); };
  click($('stop'), () => rows.forEach(stop));
  document.addEventListener('keydown', e => {
    if (e.ctrlKey || e.metaKey || e.altKey) return;
    const k = e.key, r = rows[focus];
    if (k === ' ') { e.preventDefault(); toggle(r); }
    else if (k === 's' || k === 'S' || k === 'x' || k === 'X') doSwitch(r);
    else if (k === 'ArrowDown') { e.preventDefault(); setFocus(focus + 1, true); }
    else if (k === 'ArrowUp') { e.preventDefault(); setFocus(focus - 1, true); }
  });
  document.addEventListener('keyup', e => { if (e.key === ' ') e.preventDefault(); });
  setFocus(0);
}
export const PAGE_JS = `(${pageMain.toString()})(document, window);`;

/* ---------------------------------------------------------------- the probes: the page against stubs */
/* A stub DOM element: only what pageMain touches. `kids` maps a selector to a child. */
function stubEl(kids = {}, dataset = {}) {
  const classes = new Set();
  return { dataset, textContent: '', hidden: false, checked: false, onclick: null, onchange: null, style: {}, kids,
    classList: { add: c => classes.add(c), remove: c => classes.delete(c), contains: c => classes.has(c),
      toggle: (c, force) => { const on = force === undefined ? !classes.has(c) : force; if (on) classes.add(c); else classes.delete(c); return on; } },
    querySelector(sel) { return this.kids[sel]; }, scrollIntoView() {}, blur() {} };
}
function stubAudioEl(log, name) {
  let v = 1;
  const el = { name, paused: true, duration: 7, t: 0, loop: false, ontimeupdate: null,
    get currentTime() { return el.t; }, set currentTime(x) { el.t = x; log.push(`seek ${name} to ${x.toFixed(3)}`); },
    get volume() { return v; }, set volume(x) { v = x; },
    play() { el.paused = false; log.push(`play ${name}`); return Promise.resolve(); }, pause() { el.paused = true; log.push(`pause ${name}`); } };
  return el;
}
const flush = async () => { for (let i = 0; i < 8; i++) await new Promise(r => setImmediate(r)); };

/* The page, its stub DOM and a stub browser. plant: { skewStart, noLoop } plant a fault in the stub audio graph;
   fetchOk false makes every fetch fail, as it does from file://. */
function stubPage({ rows = 2, plant = {}, fetchOk = true }) {
  const ops = [], sources = [], gains = [], audioLog = [], raf = [], handlers = {};
  const T = { ops, sources, gains, audioLog, now: 0, fetched: [], decoded: 0 };
  class Param {
    constructor(g) { this.g = g; this.v = 0; }
    get value() { return this.v; }
    set value(x) { this.v = x; ops.push({ op: 'value=', g: this.g, to: x }); }
    cancelScheduledValues(t) { ops.push({ op: 'cancelScheduledValues', g: this.g, t }); }
    setValueAtTime(x, t) { this.v = x; ops.push({ op: 'setValueAtTime', g: this.g, to: x, t }); }
    linearRampToValueAtTime(x, t) { this.v = x; ops.push({ op: 'linearRampToValueAtTime', g: this.g, to: x, t }); }
    setTargetAtTime(x, t, tau) { this.v = x; ops.push({ op: 'setTargetAtTime', g: this.g, to: x, t, tau }); }
  }
  class Ctx {
    constructor() { this.state = 'running'; this.destination = {}; T.ctx = this; }
    get currentTime() { return T.now; }
    resume() { return Promise.resolve(); }
    decodeAudioData() { T.decoded++; return Promise.resolve({ duration: 7 }); }
    createGain() { const idx = gains.length; const g = { idx, gain: new Param(idx), connect() {} }; gains.push(g); ops.push({ op: 'createGain' }); return g; }
    createBufferSource() {
      const n = { idx: sources.length, buffer: null, _loop: false, startWhen: undefined, stopWhen: undefined, connect() {},
        get loop() { return this._loop; }, set loop(x) { if (!plant.noLoop) this._loop = x; },
        start(when) { n.startWhen = when + (plant.skewStart && n.idx === 1 ? 0.01 : 0); ops.push({ op: 'start', n: n.idx, when: n.startWhen }); },
        stop(when) { n.stopWhen = when; ops.push({ op: 'stop', n: n.idx, when }); } };
      sources.push(n); ops.push({ op: 'createBufferSource' });
      return n;
    }
  }
  const rowEls = [];
  for (let i = 0; i < rows; i++) {
    const kids = { 'audio.a': stubAudioEl(audioLog, 'A' + i), 'audio.b': stubAudioEl(audioLog, 'B' + i) };
    for (const k of ['.pp', '.sw', '.side0', '.side1', '.pref0', '.pref1', '.prefs', '.ph', '.t', '.st']) kids[k] = stubEl();
    rowEls.push(Object.assign(stubEl(kids, { gain: '1.5', a: `wav/p${i}__A.wav`, b: `wav/p${i}__B.wav` }), { k: kids }));
  }
  const ids = {};
  for (const id of ['match', 'blind', 'reveal', 'tally', 'note', 'stop']) ids[id] = stubEl();
  ids.match.checked = true; ids.reveal.hidden = true; ids.note.hidden = true;
  T.ids = ids; T.rows = rowEls;
  T.doc = { getElementById: id => ids[id], querySelectorAll: () => rowEls, body: stubEl(), addEventListener: (type, fn) => { handlers[type] = fn; } };
  T.key = (key) => { const e = { key, prevented: false, preventDefault() { e.prevented = true; } }; handlers.keydown(e); return e; };
  T.env = { AudioContext: Ctx, crypto: { getRandomValues: a => { for (let i = 0; i < a.length; i++) a[i] = i % 2 === 0 ? 1 : 0; return a; } },
    fetch: url => { T.fetched.push(url); return fetchOk ? Promise.resolve({ ok: true, arrayBuffer: () => Promise.resolve(new ArrayBuffer(8)) }) : Promise.reject(new TypeError('Failed to fetch')); },
    requestAnimationFrame: fn => { raf.push(fn); } };
  T.tick = () => { const fns = raf.splice(0); fns.forEach(f => f()); };
  return T;
}
const RAMP_OPS = new Set(['cancelScheduledValues', 'setValueAtTime', 'linearRampToValueAtTime']);

/* Web Audio: Play starts both sources at one `when`, looping; Switch issues gain ramps only, complementary;
   a level toggle likewise; Stop and starting another row stop the sources; the keys; the playhead; blind mode.
   plant: { skewStart, noLoop, afterSwitch(T) } plants the fault a control must read red. */
export async function pageWebAudioProbe(plant = {}) {
  const T = stubPage({ rows: 2, plant });
  pageMain(T.doc, T.env);
  const problems = [], [r0, r1] = T.rows, bad = s => problems.push(s);
  r0.k['.pp'].onclick(); await flush();
  const starts = T.ops.filter(o => o.op === 'start');
  if (T.sources.length !== 2 || starts.length !== 2) bad(`Play made ${T.sources.length} sources and started ${starts.length}`);
  else {
    if (starts[0].when !== starts[1].when) bad(`the two sources start at different times (${starts[0].when} and ${starts[1].when})`);
    if (!T.sources.every(s => s.loop === true)) bad('a source is not looping');
  }
  if (T.fetched.length !== 2 || T.decoded !== 2) bad(`Play fetched ${T.fetched.length} files and decoded ${T.decoded} (want 2 and 2)`);
  if (gains01(T) !== '1,0') bad(`the gains at the start are ${gains01(T)} (A audible: 1,0)`);
  // the playhead: the context clock modulo the loop (7 s)
  T.now = 0.05 + 9.4; T.tick();
  if (r0.k['.t'].textContent !== '2.4 / 7.0 s' || r0.k['.ph'].style.left.slice(0, 5) !== '34.28') bad(`the playhead at 9.4 s into a 7 s loop reads "${r0.k['.t'].textContent}" at ${r0.k['.ph'].style.left}`);

  // Switch
  let mark = T.ops.length;
  r0.k['.sw'].onclick();
  if (plant.afterSwitch) plant.afterSwitch(T);
  let sw = T.ops.slice(mark);
  const other = sw.filter(o => !RAMP_OPS.has(o.op));
  if (other.length) bad(`Switch issued ${other.map(o => o.op).join(', ')}, not only gain ramps`);
  const ramps = sw.filter(o => o.op === 'linearRampToValueAtTime');
  if (ramps.length !== 2) bad(`Switch issued ${ramps.length} gain ramps (want 2)`);
  else {
    const a = ramps.find(o => o.g === 0), b = ramps.find(o => o.g === 1);
    if (!a || !b || a.to !== 0 || !(b.to > 0)) bad(`the ramps are not complementary: A to ${a && a.to}, B to ${b && b.to} (A to 0, B to its level)`);
    else if (a.t !== b.t || Math.abs(a.t - T.now - 0.01) > 1e-9) bad(`the ramps end at ${a.t} and ${b.t}, not together 10 ms from now (${T.now})`);
  }
  const onSide = r => (r.k['.side0'].classList.contains('on') ? 'A' : r.k['.side1'].classList.contains('on') ? 'B' : '?');
  if (onSide(r0) !== 'B') bad(`after Switch the highlighted side is ${onSide(r0)}, not B`);
  // the level match, live: a ramp to B's level 1, then back
  mark = T.ops.length; T.ids.match.checked = false; T.ids.match.onchange();
  sw = T.ops.slice(mark);
  const lv = sw.filter(o => o.op === 'linearRampToValueAtTime' && o.g === 1);
  if (sw.some(o => !RAMP_OPS.has(o.op)) || lv.length !== 1 || lv[0].to !== 1) bad('the match toggle is not a single ramp of B to 1');
  T.ids.match.checked = true; T.ids.match.onchange();
  r0.k['.sw'].onclick();
  // another row stops this one; Stop stops both sources
  const first = T.sources.slice(0, 2);
  r1.k['.pp'].onclick(); await flush();
  if (!first.every(s => s.stopWhen !== undefined)) bad('starting another row did not stop the first row\'s sources');
  const second = T.sources.slice(2);
  const st2 = second.map(s => s.startWhen);
  if (second.length !== 2 || st2[0] !== st2[1]) bad(`the second row's sources start at ${st2.join(' and ')}`);
  r1.k['.pp'].onclick();
  if (!second.every(s => s.stopWhen !== undefined) || r1.k['.pp'].textContent !== 'Play') bad('Stop did not stop both sources');
  // the keys: Space plays the focused row, S switches, the arrows move the focus, Space on the next row plays that
  T.key(' '); await flush();
  if (r0.k['.pp'].textContent !== 'Stop') bad('Space did not play the focused row');
  mark = T.ops.length; T.key('s');
  if (T.ops.slice(mark).some(o => !RAMP_OPS.has(o.op))) bad('the S key did more than ramp the gains');
  if (!T.key('ArrowDown').prevented || !r1.classList.contains('focus') || r0.classList.contains('focus')) bad('ArrowDown did not move the focus to the next row');
  T.key(' '); await flush();
  if (r0.k['.pp'].textContent !== 'Play' || r1.k['.pp'].textContent !== 'Stop') bad('Space on the second row did not hand playback over');
  T.key(' ');

  // blind mode: a per-row permutation of {A, B} onto {X, Y}, hidden until reveal, then shown; a tally
  T.ids.blind.checked = true; T.ids.blind.onchange();
  const labels = r => [r.k['.side0'].textContent, r.k['.side1'].textContent];
  for (const r of T.rows) if (labels(r).slice().sort().join('') !== 'XY') bad(`blind labels are ${labels(r).join(',')}, not X and Y`);
  if (!T.doc.body.classList.contains('blind') || T.ids.reveal.hidden || r0.k['.prefs'].hidden) bad('blind mode did not show its controls');
  for (const r of T.rows) if (onSide(r) !== 'A' && onSide(r) !== 'B') bad('no side is highlighted in blind mode');
  if (T.rows.some(r => !r.k['.side0'].classList.contains('on'))) bad('a blind row did not open on X');
  // the stub draws bits 1, 0: row 0 has X = B, row 1 has X = A
  r0.k['.sw'].onclick(); r0.k['.sw'].onclick();
  r0.k['.pref0'].onclick(); r1.k['.pref0'].onclick();   // "prefer X" on both
  T.ids.reveal.onclick();
  const rev = r => r.k['.side0'].textContent + ' | ' + r.k['.side1'].textContent;
  if (rev(r0) !== 'B (heard as X) | A (heard as Y)' || rev(r1) !== 'A (heard as X) | B (heard as Y)') bad(`reveal shows "${rev(r0)}" and "${rev(r1)}"`);
  if (T.ids.tally.textContent !== 'Preferred A in 1 row(s), preferred B in 1 row(s), no preference in 0.') bad(`the tally reads "${T.ids.tally.textContent}"`);
  T.ids.blind.checked = false; T.ids.blind.onchange();
  if (labels(r0).join(',') !== 'A,B' || T.doc.body.classList.contains('blind')) bad('blind mode off did not restore A and B');
  return { problems, text: `Play: ${starts.length} sources at one start time, looping; Switch: ${ramps.length} complementary ramps and no other call; blind: ${labels(r0).join('/')} restored` };
}
const gains01 = T => T.gains.slice(0, 2).map(g => g.gain.v).join(',');

/* The fallback (the fetch failed, as from file://): the note shows, the elements play looping, and Switch moves
   only the volumes: no seek, play or pause (a seek on a playing element is asynchronous and landed at a stale
   time in a browser run: A 1.23 s, B 0.45 s). The silent element is lined up with the audible one, never the
   reverse. afterSwitch(audioLog elements) plants a fault. */
export async function pageFallbackProbe(afterSwitch) {
  const T = stubPage({ rows: 1, fetchOk: false });
  pageMain(T.doc, T.env);
  const r = T.rows[0], A = r.k['audio.a'], B = r.k['audio.b'], problems = [];
  r.k['.pp'].onclick(); await flush();
  if (T.ids.note.hidden) problems.push('the fallback note was not shown');
  if (A.paused || B.paused || !A.loop || !B.loop) problems.push('the elements are not playing in a loop');
  const advance = dt => { for (const el of [A, B]) if (!el.paused) el.t += dt; };   // the clock, not a seek
  advance(0.74); A.ontimeupdate();
  B.t += 0.1;                          // the silent one drifts ahead
  A.ontimeupdate();                    // the page may line the SILENT one up
  const drifted = Math.abs(A.t - B.t);
  advance(0.5);
  const mark = T.audioLog.length;
  r.k['.sw'].onclick();
  if (afterSwitch) afterSwitch(A, B);
  const ops = T.audioLog.slice(mark);
  if (ops.length) problems.push(`Switch issued ${ops.length} media call(s): ${ops.join('; ')}`);
  if (Math.abs(A.t - B.t) > 0.02) problems.push(`after Switch B is at ${B.t.toFixed(3)} s and A at ${A.t.toFixed(3)} s`);
  if (!(B.volume > 0 && A.volume === 0)) problems.push(`after Switch the volumes are A ${A.volume}, B ${B.volume} (B should be the audible one)`);
  if (drifted > 0.02) problems.push(`the silent element was left ${drifted.toFixed(3)} s out of step`);
  if (A.paused || B.paused) problems.push('Switch left an element paused');
  return { problems, text: `note shown; A ${A.t.toFixed(3)} s, B ${B.t.toFixed(3)} s after Switch; ${ops.length} media calls on Switch` };
}
