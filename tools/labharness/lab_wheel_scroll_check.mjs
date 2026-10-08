/*
 * lab_wheel_scroll_check — scrolling never turns a lab control, and clicking a
 * pill never throws its panel back to the top.
 * WIRED: ./verify fast.
 *
 *   node tools/labharness/lab_wheel_scroll_check.mjs [lab.html ...]
 *
 * THE BUGS THIS EXISTS FOR (B301). The human, 2026-09-27: "Can we make it so
 * scrolling in the lab no longer turns knobs? It makes navigation a real
 * headache. Also clicking button parameters resets the position of the control
 * window."
 *   * WHEEL. Six labs turned a control on a plain wheel event and called
 *     preventDefault, so a trackpad scroll that crossed a knob rewrote the patch
 *     and the page stopped scrolling under the pointer. Now every lab wheel
 *     listener goes through one idiom, `onAltWheel`: a plain wheel is not
 *     touched (no value change, no preventDefault); ⌥/alt + wheel adjusts.
 *   * SCROLL. The SCALPEL lab's `rebuild()` (and the compact lab's
 *     `renderFrame`) clear the frame and build it again, so the Controls
 *     column's scroller is a NEW element after every pill click, and a new
 *     element starts at scrollTop 0. Both now rebuild through `keepScroll`.
 *
 * THREE LAYERS.
 *   1. STATIC TOTALITY over every tracked lab under docs/design: a wheel
 *      listener may be registered ONLY inside `onAltWheel`, every lab that
 *      defines it must be in LABS below (the inventory), and each copy of
 *      `onAltWheel` / `keepScroll` must be byte-identical to the SCALPEL lab's
 *      (labs are single-file, so the idiom is copied, and a copy that drifts is
 *      how the fix erodes one lab at a time).
 *   2. EXECUTED WHEEL. Each inventoried lab's scripts run for real in a vm over
 *      a small fake DOM (parsed from the lab's own markup; a real listener
 *      registry; everything it does not model is a universal stub, the
 *      lab_load_check idiom). Every element that registered a wheel listener
 *      gets a PLAIN wheel at a grid of points: no preventDefault, and the lab's
 *      state (LABS[..].state, plus every form value) must not change. Then the
 *      CONTROL: an ALT wheel at the same points must change the state for at
 *      least one element of every wheel site, or the plain-wheel zero proves
 *      nothing (a lab whose wheel code never ran would pass it).
 *   3. EXECUTED SCROLL. For labs with a rebuilt scroller (LABS[..].scroll), set
 *      each scroller's scrollTop, click every element with a click listener
 *      inside it, and require the scroller at the same place in the tree to
 *      hold the same offset afterwards.
 *
 * WHAT THE FAKE DOM CANNOT SEE, and was checked in a real browser instead.
 * It has no layout, so it never clamps a scrollTop and never moves the page;
 * a click that legitimately shortens a panel (collapsing a tier) keeps its
 * offset here but is clamped in a browser, which is correct. The page-level
 * scroll and the collapse case were measured once in headless Chrome for the
 * B301 inventory (trace 2026-09-27-b301-labs-wheel-scroll); a browser is not a
 * dependency ./verify may grow (gui_history_check's reasoning).
 *
 * CALIBRATION (L0032/L0033). Every run also loads two PLANTED copies of the
 * SCALPEL lab in memory: one whose `onAltWheel` has lost its alt guard, one
 * whose `keepScroll` no longer restores. Each must be reported RED by the same
 * runner that reports the real labs GREEN; a plant that does not fire fails
 * the run.
 */
// B446 W3b: runs this harness under Node's permission model (see sandbox_guard.mjs).
import './sandbox_guard.mjs';
import { readFileSync, readdirSync, statSync, existsSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, resolve, join, relative, basename } from 'node:path';
import vm from 'node:vm';
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const labDir = join(root, 'docs/design');

/* THE INVENTORY (B301). Every lab that adjusts a value on the wheel. `state` is
   an expression, evaluated in the lab's own global scope, whose JSON is the
   lab's parameter state; `scroll` is the selector of a scroller the lab
   rebuilds (keepScroll's `sel`); `variants` are extra query strings to load
   (the SCALPEL cycle view only takes the wheel in skin C); `oracle` loads the
   lab's <script src> files; `skip` names boot-time self-checks not to run. */
const LABS = {
  'scalpel-interface-lab.html': { state: 'P', scroll: '.scroll', variants: ['', '?skin=c'], oracle: true, skip: ['runChecks'] },
  'compact-lab.html': { state: 'STORE.v', scroll: '.ctlscroll' },
  'edge-correction-lab.html': { state: 'S' },
  'envelope-hierarchy-lab.html': { state: 'S' },
  'filter-lab.html': { state: 'S' },
  'fx-design-lab.html': { state: 'MODULES.map(m => m.P)' },
  'morph-editor-lab.html': { state: 'S' },
  'station-page-lab.html': { state: 'S' },
};
const CANON = 'scalpel-interface-lab.html';

// Lines are buffered, not printed: each lab runs in its own worker thread and
// the main thread prints the buffers in inventory order.
let failures = 0;
const OUT = [];
const fail = (what) => { OUT.push(`FAIL ${what}`); failures++; };
const ok = (what) => OUT.push(`OK   ${what}`);

/* ---------------------------------------------------------------------------
   1. STATIC
   ------------------------------------------------------------------------- */
// Any way a page can listen to the wheel. `onwheel =` included: a property
// handler would dodge an addEventListener-only scan.
const WHEEL_REG = /addEventListener\(\s*['"](?:mouse)?wheel['"]|\bon(?:mouse)?wheel\s*=/g;
function fnText(src, name) {
  const a = src.indexOf('\nfunction ' + name + '(');
  if (a < 0) return null;
  const b = src.indexOf('\n}\n', a);
  return b < 0 ? null : src.slice(a + 1, b + 2);
}
function labFiles(dir) {
  const out = [];
  for (const f of readdirSync(dir).sort()) {
    const p = join(dir, f);
    if (statSync(p).isDirectory()) out.push(...labFiles(p));
    else if (f.endsWith('.html')) out.push(p);
  }
  return out;
}
function staticChecks() {
  const canonSrc = readFileSync(join(labDir, CANON), 'utf8');
  const canon = { onAltWheel: fnText(canonSrc, 'onAltWheel'), keepScroll: fnText(canonSrc, 'keepScroll') };
  if (!canon.onAltWheel || !/if \(!e\.altKey\) return;/.test(canon.onAltWheel)) fail(`static: ${CANON} has no onAltWheel with its alt guard — the canonical copy is gone`);
  let n = 0;
  for (const file of labFiles(labDir)) {
    const name = relative(labDir, file), src = readFileSync(file, 'utf8');
    const regs = [...src.matchAll(WHEEL_REG)].length;
    const oaw = fnText(src, 'onAltWheel'), ks = fnText(src, 'keepScroll');
    const inIdiom = oaw ? [...oaw.matchAll(WHEEL_REG)].length : 0;
    if (regs > inIdiom) fail(`static: ${name} registers ${regs - inIdiom} wheel listener(s) outside onAltWheel — route it through the idiom (B301)`);
    if (oaw && !LABS[name]) fail(`static: ${name} defines onAltWheel but is not in LABS — add it to the inventory so its wheel is exercised`);
    if (LABS[name] && !oaw) fail(`static: ${name} is in LABS but has no onAltWheel`);
    if (oaw && canon.onAltWheel && oaw !== canon.onAltWheel) fail(`static: ${name}'s onAltWheel differs from ${CANON}'s — the copies must stay identical`);
    if (ks && canon.keepScroll && ks !== canon.keepScroll) fail(`static: ${name}'s keepScroll differs from ${CANON}'s — the copies must stay identical`);
    if (LABS[name] && LABS[name].scroll && !ks) fail(`static: ${name} rebuilds a scroller (LABS) but has no keepScroll`);
    if (oaw) n++;
  }
  ok(`static: ${n} labs route every wheel listener through an identical onAltWheel`);
}

/* ---------------------------------------------------------------------------
   2/3. THE FAKE DOM
   ------------------------------------------------------------------------- */
// The lab_load_check stub: callable, constructible, iterable, coercible to 0.
// ONE shared instance, not a fresh Proxy per access: it carries no state, and the
// labs paint through it thousands of times per rebuild (a canvas context is a
// stub), so allocating one per call made the SCALPEL lab's boot take ~100 s.
const STUB = new Proxy(function () {}, {
  get(t, p) {
    if (p === Symbol.iterator) return function* () {};
    if (p === Symbol.toPrimitive) return () => 0;
    if (p === 'length') return 0;
    if (p === 'then') return undefined;
    return STUB;
  },
  set() { return true; }, has() { return true; },
  apply() { return STUB; }, construct() { return STUB; },
});
const stub = () => STUB;

class Ev {
  constructor(type, o) {
    Object.assign(this, { bubbles: false, cancelable: false, altKey: false, shiftKey: false, ctrlKey: false,
      metaKey: false, deltaX: 0, deltaY: 0, deltaMode: 0, clientX: 0, clientY: 0, offsetX: 0, offsetY: 0,
      button: 0, buttons: 0, pointerId: 1, key: '', detail: null }, o || {});
    this.type = type; this.defaultPrevented = false; this.target = null; this.currentTarget = null;
    this._stop = false; this._stopNow = false;
  }
  preventDefault() { if (this.cancelable) this.defaultPrevented = true; }
  stopPropagation() { this._stop = true; }
  stopImmediatePropagation() { this._stop = this._stopNow = true; }
}

const VOID = new Set(['area', 'base', 'br', 'col', 'embed', 'hr', 'img', 'input', 'link', 'meta', 'source', 'track', 'wbr']);
const RAW = new Set(['script', 'style', 'textarea', 'title']);
const camel = s => s.replace(/-([a-z])/g, (_, c) => c.toUpperCase());
const decode = s => s.replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&quot;/g, '"').replace(/&#39;/g, "'").replace(/&nbsp;/g, ' ').replace(/&amp;/g, '&');

function makeDoc(labPath, search) {
  const reg = { wheel: [], click: [] };          // every element that listened, by type
  // The SOURCE site of each wheel listener: the frame that called onAltWheel
  // (e.g. "wireEq (fx-design-lab.html:2310)"). The control is judged per source
  // site, not per element: the FX lab's big EQ canvas is the mini one's code in a
  // panel that is closed at load, so its painter never ran and nothing on it can
  // be hit, but the site is proven by the mini canvas.
  const wheelSite = new WeakMap();
  const siteFromStack = () => {
    const fr = String(new Error().stack).split('\n').map(l => l.trim()).filter(l => l.startsWith('at '));
    const i = fr.findIndex(l => /\bonAltWheel\b/.test(l));
    const f = fr[i >= 0 ? i + 1 : 2] || '?';
    return f.replace(/^at /, '').replace(/:\d+\)?$/, ')').replace(/\(.*[\\/]/, '(');
  };
  const unsupported = new Set();
  const wrap = t => {
    const px = new Proxy(t, {
      get(tg, p, rc) {
        if (p in tg) return Reflect.get(tg, p, rc);
        if (typeof p === 'symbol') return undefined;
        return stub();
      },
    });
    t._px = px;
    return px;
  };

  class Node {
    constructor(tag, type) {
      this.nodeType = type || 1; this.tagName = tag.toUpperCase(); this.nodeName = this.tagName;
      this.localName = tag.toLowerCase();
      this._nodes = []; this.parentNode = null; this._ls = Object.create(null); this._data = '';
      this.className = ''; this.id = ''; this.dataset = {}; this._attrs = Object.create(null);
      this.style = { setProperty(k, v) { this[k] = v; }, getPropertyValue(k) { return this[k] || ''; }, removeProperty(k) { delete this[k]; } };
      this.value = ''; this.checked = false; this.disabled = false; this.title = ''; this.type = '';
      this.scrollTop = 0; this.scrollLeft = 0; this.hidden = false; this.tabIndex = 0; this.selectedIndex = 0;
      this.ownerDocument = null; this.namespaceURI = 'http://www.w3.org/1999/xhtml';
      if (this.localName === 'canvas') { this.width = 300; this.height = 150; }
      this.onclick = null; this.oninput = null; this.onchange = null;
    }
    get parentElement() { return this.parentNode && this.parentNode.nodeType === 1 ? this.parentNode : null; }
    get childNodes() { return this._nodes.slice(); }
    get children() { return this._nodes.filter(n => n.nodeType === 1); }
    get childElementCount() { return this.children.length; }
    get firstChild() { return this._nodes[0] || null; }
    get lastChild() { return this._nodes[this._nodes.length - 1] || null; }
    get firstElementChild() { return this.children[0] || null; }
    get lastElementChild() { const c = this.children; return c[c.length - 1] || null; }
    _sib(d, el) {
      const p = this.parentNode; if (!p) return null;
      const list = el ? p.children : p._nodes; const i = list.indexOf(this._px);
      return list[i + d] || null;
    }
    get nextSibling() { return this._sib(1, false); }
    get previousSibling() { return this._sib(-1, false); }
    get nextElementSibling() { return this._sib(1, true); }
    get previousElementSibling() { return this._sib(-1, true); }
    get isConnected() { let n = this._px; while (n.parentNode) n = n.parentNode; return n === DOC.documentElement; }
    get offsetParent() { return this.parentNode; }
    get options() { return this.children.filter(c => c.localName === 'option'); }
    get classList() {
      const el = this;
      const get = () => String(el.className || '').split(/\s+/).filter(Boolean);
      const set = a => { el.className = a.join(' '); };
      return {
        add(...c) { const a = get(); for (const x of c) if (a.indexOf(x) < 0) a.push(x); set(a); },
        remove(...c) { set(get().filter(x => c.indexOf(x) < 0)); },
        toggle(c, f) { const a = get(), has = a.indexOf(c) >= 0, want = f === undefined ? !has : !!f; if (want && !has) a.push(c); if (!want && has) a.splice(a.indexOf(c), 1); set(a); return want; },
        contains(c) { return get().indexOf(c) >= 0; },
        replace(a0, b0) { set(get().map(x => (x === a0 ? b0 : x))); },
        get length() { return get().length; },
        [Symbol.iterator]() { return get()[Symbol.iterator](); },
      };
    }
    // text
    get textContent() { return this.nodeType === 3 ? this._data : this._nodes.map(n => n.textContent).join(''); }
    set textContent(v) { if (this.nodeType === 3) { this._data = String(v); return; } this._clear(); if (v !== '' && v !== null && v !== undefined) this._add(textNode(String(v))); }
    get innerText() { return this.textContent; }
    set innerText(v) { this.textContent = v; }
    get data() { return this._data; }
    set data(v) { this._data = String(v); }
    get nodeValue() { return this.nodeType === 3 ? this._data : null; }
    get innerHTML() { return this._nodes.map(serialize).join(''); }
    set innerHTML(v) { this._clear(); for (const n of parseHTML(String(v))) this._add(n); }
    get outerHTML() { return serialize(this._px); }
    insertAdjacentHTML(pos, html) {
      const nodes = parseHTML(String(html));
      if (pos === 'beforeend') for (const n of nodes) this._add(n);
      else if (pos === 'afterbegin') for (const n of nodes.reverse()) this._add(n, 0);
      else if (this.parentNode) { const p = this.parentNode, i = p._nodes.indexOf(this._px) + (pos === 'afterend' ? 1 : 0); nodes.forEach((n, k) => p._add(n, i + k)); }
    }
    // tree
    _clear() { for (const n of this._nodes) n.parentNode = null; this._nodes = []; }
    _add(n, at) {
      if (n.nodeType === 11) { const kids = n._nodes.slice(); n._clear(); kids.forEach((k, i) => this._add(k, at === undefined ? undefined : at + i)); return n; }
      if (n.parentNode) n.parentNode.removeChild(n);
      n.parentNode = this._px;
      if (at === undefined || at >= this._nodes.length) this._nodes.push(n); else this._nodes.splice(at, 0, n);
      return n;
    }
    appendChild(n) { return this._add(n); }
    append(...ns) { for (const n of ns) this._add(typeof n === 'object' ? n : textNode(String(n))); }
    prepend(...ns) { ns.forEach((n, i) => this._add(typeof n === 'object' ? n : textNode(String(n)), i)); }
    insertBefore(n, ref) { if (!ref) return this._add(n); if (n.parentNode) n.parentNode.removeChild(n); return this._add(n, this._nodes.indexOf(ref)); }
    removeChild(n) { const i = this._nodes.indexOf(n); if (i >= 0) { this._nodes.splice(i, 1); n.parentNode = null; } return n; }
    replaceChild(n, old) { const i = this._nodes.indexOf(old); if (i < 0) return old; this.removeChild(old); this._add(n, i); return old; }
    replaceChildren(...ns) { this._clear(); this.append(...ns); }
    replaceWith(...ns) { const p = this.parentNode; if (!p) return; const i = p._nodes.indexOf(this._px); p.removeChild(this._px); ns.forEach((n, k) => p._add(typeof n === 'object' ? n : textNode(String(n)), i + k)); }
    remove() { if (this.parentNode) this.parentNode.removeChild(this._px); }
    before(...ns) { const p = this.parentNode; if (!p) return; const i = p._nodes.indexOf(this._px); ns.forEach((n, k) => p._add(n, i + k)); }
    after(...ns) { const p = this.parentNode; if (!p) return; const i = p._nodes.indexOf(this._px) + 1; ns.forEach((n, k) => p._add(n, i + k)); }
    contains(n) { while (n) { if (n === this._px) return true; n = n.parentNode; } return false; }
    cloneNode(deep) {
      const c = this.nodeType === 3 ? textNode(this._data) : element(this.localName);
      for (const k of ['className', 'id', 'value', 'checked', 'disabled', 'title', 'type', '_data']) c[k] = this[k];
      Object.assign(c.dataset, this.dataset); Object.assign(c._attrs, this._attrs);
      if (deep) for (const n of this._nodes) c._add(n.cloneNode(true));
      return c;
    }
    // attributes
    setAttribute(k, v) {
      v = String(v);
      if (k === 'class') this.className = v; else if (k === 'id') this.id = v;
      else if (k.startsWith('data-')) this.dataset[camel(k.slice(5))] = v;
      else if (k === 'value' || k === 'title' || k === 'type') this[k] = v;
      else if (k === 'checked' || k === 'disabled' || k === 'hidden') this[k] = true;
      else this._attrs[k] = v;
    }
    getAttribute(k) {
      if (k === 'class') return this.className || null; if (k === 'id') return this.id || null;
      if (k.startsWith('data-')) { const v = this.dataset[camel(k.slice(5))]; return v === undefined ? null : String(v); }
      if (k === 'value' || k === 'title' || k === 'type') return this[k] === '' ? (k in this._attrs ? this._attrs[k] : null) : String(this[k]);
      if (k === 'checked' || k === 'disabled' || k === 'hidden') return this[k] ? '' : null;
      return k in this._attrs ? this._attrs[k] : null;
    }
    hasAttribute(k) { return this.getAttribute(k) !== null; }
    removeAttribute(k) {
      if (k === 'class') this.className = ''; else if (k === 'id') this.id = '';
      else if (k.startsWith('data-')) delete this.dataset[camel(k.slice(5))];
      else if (k === 'checked' || k === 'disabled' || k === 'hidden') this[k] = false;
      else delete this._attrs[k];
    }
    toggleAttribute(k, f) { const on = f === undefined ? !this.hasAttribute(k) : !!f; if (on) this.setAttribute(k, ''); else this.removeAttribute(k); return on; }
    // selectors
    matches(sel) { return matchSel(this._px, sel); }
    closest(sel) { let n = this._px; while (n && n.nodeType === 1) { if (matchSel(n, sel)) return n; n = n.parentNode; } return null; }
    querySelectorAll(sel) { const out = []; walk(this._px, n => { if (matchSel(n, sel)) out.push(n); }); return out; }
    querySelector(sel) { return this.querySelectorAll(sel)[0] || null; }
    getElementsByClassName(c) { return this.querySelectorAll('.' + c.trim().split(/\s+/).join('.')); }
    getElementsByTagName(t) { return this.querySelectorAll(t); }
    // events
    addEventListener(type, f, o) {
      if (typeof f !== 'function' && !(f && f.handleEvent)) return;
      (this._ls[type] = this._ls[type] || []).push(f);
      if (reg[type]) reg[type].push(this._px);
      if (type === 'wheel') wheelSite.set(this._px, siteFromStack());
    }
    removeEventListener(type, f) { const l = this._ls[type]; if (l) { const i = l.indexOf(f); if (i >= 0) l.splice(i, 1); } }
    dispatchEvent(ev) {
      ev.target = this._px;
      let n = this._px;
      while (n) {
        ev.currentTarget = n;
        for (const f of (n._ls[ev.type] || []).slice()) { typeof f === 'function' ? f.call(n, ev) : f.handleEvent(ev); if (ev._stopNow) break; }
        const h = n['on' + ev.type]; if (typeof h === 'function' && Object.prototype.hasOwnProperty.call(n, 'on' + ev.type)) h.call(n, ev);
        if (ev._stop || !ev.bubbles) break;
        n = n.parentNode;
      }
      return !ev.defaultPrevented;
    }
    click() { this.dispatchEvent(new Ev('click', { bubbles: true, cancelable: true })); }
    focus() {} blur() {} scrollIntoView() {} scrollTo() {} setPointerCapture() {} releasePointerCapture() {}
    hasPointerCapture() { return false; } select() {} animate() { return stub(); }
    // geometry: no layout, so a fixed box; a canvas measures what it draws, so the
    // pointer maps 1:1 onto its bitmap (hit-tests work in bitmap pixels).
    getBoundingClientRect() {
      const w = this.localName === 'canvas' ? +this.width || 300 : 100, h = this.localName === 'canvas' ? +this.height || 150 : 24;
      return { x: 0, y: 0, left: 0, top: 0, right: w, bottom: h, width: w, height: h };
    }
    getClientRects() { return [this.getBoundingClientRect()]; }
    get clientWidth() { return this.getBoundingClientRect().width; }
    get clientHeight() { return this.getBoundingClientRect().height; }
    get offsetWidth() { return this.clientWidth; }
    get offsetHeight() { return this.clientHeight; }
    get scrollWidth() { return this.clientWidth; }
    get scrollHeight() { return this.clientHeight; }
    get offsetTop() { return 0; }
    get offsetLeft() { return 0; }
    getContext() { return stub(); }
    toDataURL() { return ''; }
  }
  const element = tag => wrap(new Node(tag, 1));
  const textNode = s => { const t = wrap(new Node('#text', 3)); t._data = s; return t; };
  function walk(n, f) { for (const c of n._nodes) if (c.nodeType === 1) { f(c); walk(c, f); } }
  function serialize(n) {
    if (n.nodeType === 3) return n._data.replace(/&/g, '&amp;').replace(/</g, '&lt;');
    const a = (n.className ? ` class="${n.className}"` : '') + (n.id ? ` id="${n.id}"` : '');
    return `<${n.localName}${a}>${VOID.has(n.localName) ? '' : n._nodes.map(serialize).join('') + `</${n.localName}>`}`;
  }
  // A forgiving HTML parser: tags, attributes, text, comments, raw-text elements.
  function parseHTML(html) {
    const top = [], stack = [];
    const add = n => { if (stack.length) stack[stack.length - 1]._add(n); else top.push(n); };
    const re = /<!--[\s\S]*?-->|<!doctype[^>]*>|<\/([a-zA-Z][\w-]*)\s*>|<([a-zA-Z][\w-]*)((?:\s+[^\s=>\/]+(?:\s*=\s*(?:"[^"]*"|'[^']*'|[^\s>]+))?)*)\s*(\/?)>|([^<]+|<)/gi;
    let m;
    while ((m = re.exec(html))) {
      if (m[5] !== undefined) { add(textNode(decode(m[5]))); continue; }
      if (m[1]) { const t = m[1].toLowerCase(); for (let i = stack.length - 1; i >= 0; i--) if (stack[i].localName === t) { stack.length = i; break; } continue; }
      if (!m[2]) continue;
      const tag = m[2].toLowerCase(), e = element(tag);
      for (const a of (m[3] || '').matchAll(/([^\s=>\/]+)(?:\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))?/g))
        e.setAttribute(a[1].toLowerCase(), decode(a[2] ?? a[3] ?? a[4] ?? ''));
      add(e);
      if (RAW.has(tag)) {
        const end = html.toLowerCase().indexOf('</' + tag, re.lastIndex);
        const body = html.slice(re.lastIndex, end < 0 ? html.length : end);
        if (body) e._add(textNode(tag === 'textarea' || tag === 'title' ? decode(body) : body));
        if (tag === 'textarea') e.value = decode(body);
        re.lastIndex = end < 0 ? html.length : html.indexOf('>', end) + 1;
      } else if (!VOID.has(tag) && !m[4]) stack.push(e);
    }
    return top;
  }
  // Selectors: lists, descendant/child combinators, tag/#id/.class/[attr(op)v].
  // Anything else (pseudo-classes) is recorded and matches nothing.
  const selCache = new Map();
  function parseSel(sel) {
    if (selCache.has(sel)) return selCache.get(sel);
    const list = [];
    for (const part of sel.split(/,(?![^\[]*\])/)) {
      const toks = part.trim().replace(/\s*>\s*/g, ' > ').split(/\s+(?![^\[]*\])/).filter(Boolean);
      const chain = [];
      let comb = ' ';
      for (const t of toks) {
        if (t === '>') { comb = '>'; continue; }
        const c = { comb, tag: null, id: null, cls: [], attrs: [], bad: false };
        for (const q of t.matchAll(/(^[a-zA-Z*][\w-]*)|#([\w-]+)|\.([\w-]+)|\[([\w-]+)(?:([~^$*|]?=)(?:"([^"]*)"|'([^']*)'|([^\]]*)))?\]|(:[\w-]+(?:\([^)]*\))?)/g)) {
          if (q[1]) c.tag = q[1] === '*' ? null : q[1].toLowerCase();
          else if (q[2]) c.id = q[2]; else if (q[3]) c.cls.push(q[3]);
          else if (q[4]) c.attrs.push([q[4], q[5], q[6] ?? q[7] ?? q[8]]);
          else if (q[9]) { c.bad = true; unsupported.add(sel); }
        }
        chain.push(c); comb = ' ';
      }
      list.push(chain);
    }
    selCache.set(sel, list);
    return list;
  }
  function matchOne(n, c) {
    if (c.bad || n.nodeType !== 1) return false;
    if (c.tag && n.localName !== c.tag) return false;
    if (c.id && n.id !== c.id) return false;
    if (c.cls.length) { const k = String(n.className || '').split(/\s+/); if (!c.cls.every(x => k.indexOf(x) >= 0)) return false; }
    for (const [a, op, v] of c.attrs) {
      const got = n.getAttribute(a);
      if (got === null) return false;
      if (op === '=' && got !== v) return false;
      if (op === '~=' && got.split(/\s+/).indexOf(v) < 0) return false;
      if (op === '^=' && !got.startsWith(v)) return false;
      if (op === '$=' && !got.endsWith(v)) return false;
      if (op === '*=' && got.indexOf(v) < 0) return false;
    }
    return true;
  }
  function matchChain(n, chain, i) {
    if (!matchOne(n, chain[i])) return false;
    if (i === 0) return true;
    let p = n.parentNode;
    if (chain[i].comb === '>') return !!p && matchChain(p, chain, i - 1);
    for (; p; p = p.parentNode) if (matchChain(p, chain, i - 1)) return true;
    return false;
  }
  function matchSel(n, sel) { return parseSel(String(sel)).some(ch => ch.length && matchChain(n, ch, ch.length - 1)); }

  // The document, from the lab's own markup.
  const src = readFileSync(labPath, 'utf8');
  const DOC = {};
  const html = element('html'), head = element('head'), body = element('body');
  html._add(head); html._add(body);
  const bm = src.match(/<body[^>]*>([\s\S]*)<\/body>/i);
  for (const n of parseHTML(bm ? bm[1] : src)) body._add(n);
  const hm = src.match(/<head[^>]*>([\s\S]*?)<\/head>/i);
  if (hm) for (const n of parseHTML(hm[1])) head._add(n);
  Object.assign(DOC, {
    documentElement: html, head, body, readyState: 'complete', visibilityState: 'visible', hidden: false,
    createElement: t => element(String(t)), createElementNS: (_, t) => element(String(t)),
    createTextNode: s => textNode(String(s)), createDocumentFragment: () => wrap(new Node('#fragment', 11)),
    getElementById: id => { let r = null; walk(html, n => { if (!r && n.id === id) r = n; }); return r; },
    querySelector: s => html.querySelector(s), querySelectorAll: s => html.querySelectorAll(s),
    getElementsByClassName: c => html.getElementsByClassName(c), getElementsByTagName: t => html.getElementsByTagName(t),
    addEventListener() {}, removeEventListener() {}, activeElement: body, fonts: { ready: Promise.resolve(), load: () => Promise.resolve() },
  });
  const docPx = new Proxy(DOC, { get: (t, p) => (p in t ? t[p] : typeof p === 'symbol' ? undefined : stub()) });
  return { doc: docPx, src, reg, wheelSite, unsupported, Ev, labPath, search };
}

/* The page's <script> elements in order, skipping HTML comments: prose inside a
   comment that mentions "the <script src> below" is not a script (the SCALPEL
   lab's header says exactly that). */
function scriptsOf(src) {
  const out = [];
  const re = /<!--[\s\S]*?-->|<script\b([^>]*)>([\s\S]*?)<\/script>/gi;
  for (const m of src.matchAll(re)) if (m[0].startsWith('<script')) out.push(m);
  return out;
}

/* Run a lab (or a planted variant's source) under the fake DOM. Scripts run in
   document order: inline blocks and repo-local <script src> files. */
async function loadLab(labPath, search, srcOverride, spec) {
  spec = spec || {};
  const D = makeDoc(labPath, search);
  const store = Object.create(null);
  const storage = { getItem: k => (k in store ? store[k] : null), setItem: (k, v) => { store[k] = String(v); }, removeItem: k => { delete store[k]; }, clear() {} };
  const sandbox = {
    document: D.doc, console: { log() {}, warn() {}, error() {}, info() {}, debug() {} },
    location: { search, href: 'http://localhost/' + relative(root, labPath) + search, pathname: '/' + relative(root, labPath), hash: '', origin: 'http://localhost' },
    navigator: new Proxy({ requestMIDIAccess: () => ({ then: () => ({ catch() {} }) }), userAgent: 'node' }, { get: (t, p) => (p in t ? t[p] : stub()) }),
    history: { replaceState() {}, pushState() {} }, localStorage: storage, sessionStorage: storage,
    AudioContext: function () { return stub(); }, webkitAudioContext: function () { return stub(); },
    OfflineAudioContext: function () { return stub(); }, AudioWorkletNode: function () { return stub(); },
    Worker: function () { return stub(); }, ResizeObserver: class { observe() {} unobserve() {} disconnect() {} },
    MutationObserver: class { observe() {} disconnect() {} }, IntersectionObserver: class { observe() {} disconnect() {} },
    requestAnimationFrame: () => 0, cancelAnimationFrame: () => {},
    // Timers never run, as in lab_load_check: in these labs they carry the
    // load-time self-checks and the paint loop, not the controls (the filter
    // lab's deferred fidelity programme alone takes ~110 s in the vm). An
    // inventoried lab that ever builds a control in a timer shows up here as a
    // blind control (no wheel element ran), not as a silent pass.
    setTimeout: () => 0,
    clearTimeout: () => {}, setInterval: () => 0, clearInterval: () => {},
    addEventListener: () => {}, removeEventListener: () => {}, alert: () => {}, confirm: () => true, prompt: () => null,
    performance: { now: () => 0 }, devicePixelRatio: 1, innerWidth: 1400, innerHeight: 900, scrollX: 0, scrollY: 0,
    scrollTo() {}, getComputedStyle: () => stub(), matchMedia: () => ({ matches: false, addEventListener() {}, addListener() {} }),
    getSelection: () => stub(), queueMicrotask, structuredClone, TextEncoder, TextDecoder, URLSearchParams,
    URL: Object.assign(function (u, b) { return new URL(u, b); }, { createObjectURL: () => '', revokeObjectURL() {} }),
    Event: class extends Ev {}, CustomEvent: class extends Ev {}, KeyboardEvent: class extends Ev {}, MouseEvent: class extends Ev {},
    PointerEvent: class extends Ev {}, WheelEvent: class extends Ev {}, InputEvent: class extends Ev {},
    Image: class {}, Blob: class {}, FileReader: class {}, Option: class { constructor(t, v) { this.text = t; this.value = v; } },
  };
  sandbox.globalThis = sandbox; sandbox.self = sandbox; sandbox.window = sandbox;
  const ctx = vm.createContext(sandbox);
  const src = srcOverride !== undefined ? srcOverride : D.src;
  const errors = [];
  for (const m of scriptsOf(src)) {
    const attrs = m[1], s = attrs.match(/\bsrc\s*=\s*["']([^"']+)["']/), ty = attrs.match(/\btype\s*=\s*["']([^"']+)["']/);
    if (ty && !/javascript/i.test(ty[1])) continue;
    // <script src> (the oracles: razor-core.js, MAW's core) loads only where
    // LABS asks: the SCALPEL cycle view hit-tests blade windows whose geometry
    // comes from the oracle's defaults, so without it that site is blind.
    // lineOffset: a stack frame (the wheel SITE) then names the lab's own line
    let code = m[2], name = basename(labPath), lineOffset = src.slice(0, m.index + m[0].indexOf('>') + 1).split('\n').length - 1;
    if (s) {
      if (!spec.oracle) continue;
      // a scratch copy of a lab (the must-fail proofs) lives outside docs/design,
      // so its relative src is resolved against docs/design when it misses
      let p = resolve(dirname(labPath), s[1]);
      if (!existsSync(p)) p = resolve(labDir, s[1]);
      if (!existsSync(p)) continue;
      code = readFileSync(p, 'utf8'); name = relative(root, p); lineOffset = 0;
    }
    const tq = Date.now();
    try { new vm.Script(code, { filename: name, lineOffset }).runInContext(ctx, { timeout: 30000 }); }
    catch (e) { errors.push(`${name}: ${e && e.name}: ${e && e.message}`); }
    if (process.env.LWS_VERBOSE) console.log(`     script ${name} ${Date.now() - tq} ms`);
  }
  const tt = Date.now();
  // LABS[..].skip: global functions the async boot would call that are the lab's
  // OWN load-time gate (the SCALPEL self-checks render the oracle, ~50 s in the
  // vm). Replaced before the boot's first await resolves, so they never run.
  for (const f of spec.skip || []) { try { vm.runInContext(`${f} = function () {};`, ctx); } catch (_) {} }
  for (let i = 0; i < 20; i++) await new Promise(r => setImmediate(r));   // let async boots settle
  if (process.env.LWS_VERBOSE) console.log(`     timers/async ${Date.now() - tt} ms`);
  const evalIn = code => vm.runInContext(code, ctx, { timeout: 30000 });
  return { D, ctx, evalIn, errors };
}

// A lab's parameter state, as a string: its declared state plus every form value.
function snapshotter(L, stateExpr) {
  return () => {
    let s;
    try {
      s = L.evalIn(`(() => { const seen = new WeakSet(); return JSON.stringify(${stateExpr}, (k, x) => {
        if (x && typeof x === 'object') { if (seen.has(x)) return undefined; seen.add(x);
          if (ArrayBuffer.isView(x)) return Array.from(x.slice(0, 256)); }
        return typeof x === 'function' ? undefined : x; }); })()`);
    } catch (e) { s = 'STATE-ERR ' + e.message; }
    const vals = [];
    const walkV = n => { for (const c of n._nodes) if (c.nodeType === 1) { if (/^(input|select|textarea)$/.test(c.localName)) vals.push(c.value + (c.checked ? '*' : '')); walkV(c); } };
    walkV(L.D.doc.documentElement);
    return s + '|' + vals.join(',');
  };
}

/* 2. THE WHEEL, per lab (variant). Returns counts; the caller judges. `sites`
   accumulates across a lab's variants. */
function wheelRun(L, snap, sites) {
  const res = { els: 0, plainPD: 0, plainChg: 0, altChg: 0 };
  const seen = new Set();
  for (const t of L.D.reg.wheel) {
    if (seen.has(t) || !t.isConnected) continue;
    seen.add(t); res.els++;
    const r = t.getBoundingClientRect();
    const site = L.D.wheelSite.get(t) || '?';
    const s = sites.get(site) || { els: 0, altChg: 0 }; sites.set(site, s); s.els++;
    let changed = false;
    // a canvas hit-tests (a pin, a node, a blade window lives somewhere on it), so it
    // gets a grid; a knob or a cell is one target, so its centre is enough
    const [gx, gy] = t.localName === 'canvas' ? [16, 10] : [1, 1];
    for (let i = 0; i < gx; i++) for (let j = 0; j < gy; j++) for (const dy of [-100, 100]) {
      const at = { clientX: r.left + r.width * (i + 0.5) / gx, clientY: r.top + r.height * (j + 0.5) / gy, deltaY: dy, bubbles: true, cancelable: true };
      const s0 = snap();
      const plain = new L.D.Ev('wheel', at);
      t.dispatchEvent(plain);
      if (plain.defaultPrevented) res.plainPD++;
      if (snap() !== s0) res.plainChg++;
      if (!changed) {
        const alt = new L.D.Ev('wheel', Object.assign({ altKey: true }, at));
        t.dispatchEvent(alt);
        if (snap() !== s0) changed = true;
      }
    }
    if (changed) { s.altChg++; res.altChg++; }
  }
  return res;
}

/* 3. THE SCROLL: every clickable inside every rebuilt scroller keeps its offset. */
function scrollRun(L, sel) {
  const res = { scrollers: 0, clicks: 0, moved: [] };
  const scrollers = () => L.D.doc.documentElement.querySelectorAll(sel);
  const clickables = sc => { const out = []; const w = n => { for (const c of n._nodes) if (c.nodeType === 1) { if ((c._ls.click || []).length || (typeof c.onclick === 'function' && Object.prototype.hasOwnProperty.call(c, 'onclick'))) out.push(c); w(c); } }; w(sc); return out; };
  const n0 = scrollers().length;
  res.scrollers = n0;
  for (let i = 0; i < n0; i++) {
    const count = clickables(scrollers()[i] || { _nodes: [] }).length;
    for (let j = 0; j < count; j++) {
      const sc = scrollers()[i]; if (!sc) break;
      const b = clickables(sc)[j]; if (!b) break;
      const OFF = 137;
      sc.scrollTop = OFF;
      const label = (b.textContent || b.title || b.className || b.localName).trim().replace(/\s+/g, ' ').slice(0, 24);
      b.click(); res.clicks++;
      const after = scrollers()[i];
      if (!after) { res.moved.push(`${sel}[${i}] "${label}": the scroller is gone`); continue; }
      if (after.scrollTop !== OFF) res.moved.push(`${sel}[${i}] "${label}": scrollTop ${OFF} -> ${after.scrollTop}${after !== sc ? ' (rebuilt)' : ''}`);
    }
  }
  return res;
}

async function checkLab(name, spec, opts) {
  opts = opts || {};
  const labPath = opts.path || join(labDir, name);
  const verdict = { wheelRed: false, scrollRed: false };
  const tag = opts.tag || name;
  const sites = new Map();
  for (const variant of spec.variants || ['']) {
    const L = await loadLab(labPath, variant, opts.src, spec);
    const where = `${tag}${variant ? ' ' + variant : ''}`;
    if (L.errors.length) { (opts.quiet ? () => {} : fail)(`${where}: the lab threw under the fake DOM — ${L.errors[0]}`); verdict.wheelRed = verdict.scrollRed = true; continue; }
    const snap = snapshotter(L, spec.state);
    const s0 = snap();
    if (/^STATE-ERR/.test(s0)) { (opts.quiet ? () => {} : fail)(`${where}: state expression \`${spec.state}\` did not evaluate — ${s0}`); verdict.wheelRed = true; continue; }
    const w = wheelRun(L, snap, sites);
    const wheelOk = w.plainPD === 0 && w.plainChg === 0;
    if (!wheelOk) verdict.wheelRed = true;
    const wmsg = `${where}: plain wheel over ${w.els} element(s): ${w.plainChg} value change(s), ${w.plainPD} preventDefault(s); alt wheel moved ${w.altChg}/${w.els}`;
    if (!opts.quiet) (wheelOk ? ok : fail)(wmsg);
    if (spec.scroll && !variant) {
      const sr = scrollRun(L, spec.scroll);
      const scrollOk = sr.scrollers > 0 && sr.clicks > 0 && sr.moved.length === 0;
      if (!scrollOk) verdict.scrollRed = true;
      const smsg = `${where}: ${sr.clicks} click(s) inside ${sr.scrollers} "${spec.scroll}" scroller(s), ${sr.moved.length} moved` + (sr.clicks ? '' : ' (nothing clicked: blind)')
        + (sr.moved.length ? ' — ' + sr.moved.slice(0, 3).join('; ') : '');
      if (!opts.quiet) (scrollOk ? ok : fail)(smsg);
    }
    if (process.env.LWS_VERBOSE && L.D.unsupported.size) console.log(`     (unsupported selectors in ${where}: ${[...L.D.unsupported].slice(0, 8).join(' | ')})`);
  }
  // THE CONTROL, per source site over all variants: every place the lab calls
  // onAltWheel must be shown to adjust with alt, or its plain-wheel zero is vacuous.
  // TOTALITY: every onAltWheel call in the source must have run in some variant,
  // or a wheel site hides in a view no variant opens (add a variant for it).
  const labSrc = opts.src !== undefined ? opts.src : readFileSync(labPath, 'utf8');
  const callLines = labSrc.split('\n').map((l, i) => (/\bonAltWheel\(/.test(l) && !/function onAltWheel\(/.test(l) ? i + 1 : 0)).filter(Boolean);
  const ranLines = new Set([...sites.keys()].map(k => +((k.match(/:(\d+)\)$/) || [])[1])));
  const unrun = callLines.filter(n => !ranLines.has(n));
  const dead = [...sites].filter(([, s]) => !s.altChg).map(([k]) => k);
  const ctlOk = sites.size > 0 && dead.length === 0 && unrun.length === 0;
  if (!ctlOk) verdict.wheelRed = true;
  if (!opts.quiet) (ctlOk ? ok : fail)(`${tag}: control — alt wheel adjusts at ${sites.size - dead.length}/${callLines.length} onAltWheel call site(s)`
    + ` (${[...sites.keys()].map(k => k.replace(/ \(.*:(\d+)\)$/, ':$1')).join(', ')})`
    + (sites.size ? '' : ' — no wheel listener ran: blind') + (dead.length ? `; NO alt change from ${dead.join(', ')}` : '')
    + (unrun.length ? `; call site(s) at line ${unrun.join(', ')} never ran in any variant` : ''));
  if (process.env.LWS_VERBOSE) for (const [k, v] of sites) console.log(`     site ${k}: ${v.altChg}/${v.els}`);
  return verdict;
}

/* Calibration: two planted faults in the canonical lab, in memory. Each needle
   must be in the lab (else the plant is re-aimed, never silently skipped). */
const PLANTS = [
  { what: 'wheel without its alt guard', needle: '    if (!e.altKey) return;\n', key: 'wheelRed' },
  { what: 'keepScroll that never restores', needle: '  all().forEach((e, i) => { if (was[i] > 0) e.scrollTop = was[i]; });\n', key: 'scrollRed' },
];

/* EACH LOAD IS INDEPENDENT, so every lab and every plant runs in its own worker
   thread (node's built-in worker_threads, morph_editor_check's pattern — no
   dependency): ~12 s in sequence, ~3-4 s in parallel. Only buffered lines and a
   verdict cross the thread boundary. */
function inWorker(job) {
  return new Promise(res => {
    const w = new Worker(new URL(import.meta.url), { workerData: job });
    w.once('message', res);
    w.once('error', e => res({ out: [`FAIL ${job.name || job.plant}: worker threw: ${e && e.message}`], failures: 1, verdict: {} }));
  });
}

if (!isMainThread) {
  let verdict;
  if (workerData.plant !== undefined) {
    const P = PLANTS[workerData.plant], src = readFileSync(join(labDir, CANON), 'utf8');
    verdict = await checkLab(CANON, { ...LABS[CANON], variants: [''] }, { src: src.split(P.needle).join(''), quiet: true, tag: 'PLANT' });
  } else verdict = await checkLab(workerData.name, LABS[workerData.name], {});
  parentPort.postMessage({ out: OUT, failures, verdict });
} else {
  const args = process.argv.slice(2);
  if (args.length) {
    // ad-hoc: check the named files (e.g. a scratch copy) as their inventory entry
    for (const a of args) {
      const name = basename(a), spec = LABS[name];
      if (!spec) { fail(`${name}: not in the inventory (LABS)`); continue; }
      await checkLab(name, spec, { path: resolve(a) });
    }
  } else {
    staticChecks();
    const canonSrc = readFileSync(join(labDir, CANON), 'utf8');
    const aimed = PLANTS.map(P => canonSrc.indexOf(P.needle) >= 0);
    const jobs = [...Object.keys(LABS).map(name => inWorker({ name })),
      ...PLANTS.map((P, i) => (aimed[i] ? inWorker({ plant: i }) : Promise.resolve(null)))];
    const res = await Promise.all(jobs);
    const nl = Object.keys(LABS).length;
    for (const r of res.slice(0, nl)) { OUT.push(...r.out); failures += r.failures; }
    PLANTS.forEach((P, i) => {
      const r = res[nl + i];
      if (!aimed[i]) fail(`plant "${P.what}": its needle is not in ${CANON} — re-aim the plant`);
      else if (r.verdict[P.key]) ok(`plant: ${P.what} — caught`);
      else fail(`plant: ${P.what} — NOT caught (the check is blind to it)`);
    });
  }
  for (const l of OUT) console.log(l);
  console.log(`lab_wheel_scroll_check: ${failures ? 'RED' : 'GREEN'} — ${failures} failure(s)${args.length ? '' : `; ${Object.keys(LABS).length} labs, ${PLANTS.length} planted faults`}`);
  process.exit(failures ? 1 : 0);
}
