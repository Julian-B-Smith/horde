/* main-layout-lab-parts.js — the Scalpel bench's morphing logo and Specimen, for docs/design/main-layout-lab.html only. horde GUI session, 2026-11-02.
   VERBATIM from docs/design/scalpel-interface-lab.html at 271f275 (E2 and E3), as ADR-333 asks: "copied precisely", not re-derived. Only this header's shims
   are new: they stand in for what the bench's own engine and page provide. Not a component: when a layout is ruled, the two go to the design master
   (ADR-316), and this file retires with the lab. performance.now inside is the bench's cost readout; it never reaches a drawing. */
(function () {
'use strict';
const SR = 44100, LOAD = { shed: false }, SHED_LOGO = 4, SHED_SPECIMEN = 12;
let frameDoc = null, xyAt = () => [0.5, 0.5];
// The bench's token names, read from the shell's tokens in the lab's frame.
const TOKEN = { '--text': '--ink', '--bg': '--bg', '--scr-tube': '--scr-ground', '--lgA': '--cA', '--lgB': '--cB', '--lgC': '--cC', '--lgD': '--cD' };
function TOK(name, fallback) {
  const root = frameDoc && frameDoc.documentElement, v = root ? getComputedStyle(root).getPropertyValue(TOKEN[name] || name).trim() : '';
  return v || fallback || '';
}
function RGBA(hex, a) { const m = /^#([0-9a-f]{6})$/i.exec(String(hex).trim()), v = m ? parseInt(m[1], 16) : 0; return 'rgba(' + (v >> 16 & 255) + ',' + (v >> 8 & 255) + ',' + (v & 255) + ',' + a + ')'; }
const qp = (k, d) => d;
const el = (tag, cls) => { const e = document.createElement(tag); if (cls) e.className = cls; return e; };
const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
function mulberry32(a) { return function () { a |= 0; a = a + 0x6D2B79F5 | 0; let t = Math.imul(a ^ a >>> 15, 1 | a); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; }; }
const LAB = { page: 'main' }, MORPH = { on: false, x: 0.5, y: 0.5 }, EFF = { detune: 28 }, AUD = { on: false, feed: null };
const MON = { clock: 0, rSm: 0.6, L: null, core: null };
const xyMarker = () => xyAt();
/* ======== E2, verbatim ======== */
const TD_TAU = 6.28318530718, TD_CYCLE = 8.0, TD_W1 = TD_TAU / TD_CYCLE;
/* gui2.html:6230: must stay fract(sin(a*127.1+b*311.7)*43758.5453) or relocation diverges */
const tdHash2 = (a, b) => { const x = Math.sin(a * 127.1 + b * 311.7) * 43758.5453; return x - Math.floor(x); };
/* td::Warp, as gui2.html:6233-6350 ports it (constant names and order of operations kept) */
function tdWarp(seed) {
  let a = (seed * 9301 + 49297) % 233280;
  const next = () => { a = (a * 9301 + 49297) % 233280; return a / 233280; };
  const ph1 = next() * TD_TAU, ph2 = next() * TD_TAU, ph3 = next() * TD_TAU;
  const P = { waveAmp: 26, waveFreq: 1.4, tremor: 3.5, smearAngle: 0, angleJitter: 12, waveJitter: 0.5, pxScale: 1 };
  const strokes = [], fields = [];
  return {
    params: P,
    addBlob: (amp, rad, r1, r2, cyc, bph) => strokes.push({ x0: 0, y0: 0, amp, rad, r1, r2, blob: true, cyc, bph }),
    addSwirl: (rad, sc, rot, cyc, bph) => fields.push({ rad, sc, rot, cyc, bph }),
    render(src, dst, W, H, t) {
      const d1 = ph1 + t * TD_W1 + 1.7 * Math.sin(t * TD_W1 * 2 + 0.5);
      const d2 = ph2 - t * TD_W1 + 1.3 * Math.sin(t * TD_W1 * 3 + 2.0);
      const aMod = 1 + 0.35 * Math.sin(t * TD_W1 * 2 + 1.0) * Math.sin(t * TD_W1);
      const wave = new Float32Array(W);
      for (let x = 0; x < W; x++) {
        const fx = x / W;
        wave[x] = P.waveAmp * aMod * Math.sin(fx * P.waveFreq * TD_TAU + d1) + P.waveAmp * 0.35 * Math.sin(fx * P.waveFreq * 5.1 + d2);
      }
      const sts = [];
      for (const st of strokes) {
        let amp = st.amp, x0 = st.x0, y0 = st.y0;
        if (st.blob) {
          const ph = t * TD_W1 * 4 * st.cyc + st.bph, env = 0.5 * (1 - Math.cos(ph));
          amp = st.amp * Math.pow(env, 1.6);
          const k = Math.floor(ph / TD_TAU);
          x0 = W * (0.15 + tdHash2(st.bph, k) * 0.7);
          y0 = H * (0.25 + tdHash2(st.bph + 9.7, k) * 0.5);
        }
        if (Math.abs(amp) <= 2 * P.pxScale) continue;
        const th = (P.smearAngle + (st.r1 * 2 - 1) * P.angleJitter) * TD_TAU / 360;
        sts.push({ amp, x0, y0, tanT: Math.tan(th), cosT: Math.cos(th), sinT: Math.sin(th), rad: st.rad + Math.abs(amp) * 0.25,
          wf: (0.11 / P.pxScale) * Math.pow(2.5, (st.r2 * 2 - 1) * P.waveJitter) });
      }
      const flds = [];
      for (const f of fields) {
        const ph = t * TD_W1 * 4 * f.cyc + f.bph, e = Math.pow(0.5 * (1 - Math.cos(ph)), 1.6);
        if (Math.abs(f.sc * e) <= 0.005 && Math.abs(f.rot * e) <= 0.005) continue;
        const k = Math.floor(ph / TD_TAU);
        flds.push({ x0: W * (0.15 + tdHash2(f.bph + 3.1, k) * 0.7), y0: H * (0.25 + tdHash2(f.bph + 13.9, k) * 0.5), rad: f.rad, sc: f.sc * e, rot: f.rot * e });
      }
      for (let y = 0; y < H; y++) {
        for (let x = 0; x < W; x++) {
          let sy = y + wave[x] + P.tremor * Math.sin(y * 0.055 + x * 0.01 + ph3 + t * TD_W1 * 2 + Math.sin(t * TD_W1) * 2);
          let sx = x;
          for (const f of flds) {
            const dx = x - f.x0, dy = y - f.y0;
            if (dx > f.rad * 3 || dx < -f.rad * 3 || dy > f.rad * 3 || dy < -f.rad * 3) continue;
            const env = Math.exp(-(dx * dx + dy * dy) / (2 * f.rad * f.rad)) - 0.0111;   // 0 at the 3σ box (load-bearing)
            if (env <= 0) continue;
            sx -= env * (f.sc * dx - f.rot * dy);
            sy -= env * (f.sc * dy + f.rot * dx);
          }
          for (const q of sts) {
            const dxr = x - (q.x0 + (y - q.y0) * q.tanT);
            if (dxr > q.rad * 3.5 || dxr < -q.rad * 3.5) continue;
            const env = Math.exp(-(dxr * dxr) / (2 * q.rad * q.rad)) - 0.0022;
            if (env <= 0) continue;
            const ad = Math.abs(q.amp), sg = q.amp > 0 ? 1 : -1, dl = (y - q.y0) * sg;
            if (dl > -40 * P.pxScale) {
              const rise = 1 - Math.exp(-Math.max(0, dl) / (ad * 0.5 + 10 * P.pxScale));
              const over = Math.max(0, dl - ad * 1.6);
              const fall = Math.exp(-(over * over) / (2 * (ad + 60 * P.pxScale) * (ad + 60 * P.pxScale)));
              const shift = q.amp * env * rise * fall + sg * env * Math.min(ad * 0.45, 70 * P.pxScale) * Math.sin(dl * q.wf) * rise * fall;
              sy -= shift * q.cosT; sx -= shift * q.sinT;
            }
          }
          if (sy < 0) sy = 0; if (sy > H - 1.01) sy = H - 1.01;
          if (sx < 0) sx = 0; if (sx > W - 1.01) sx = W - 1.01;
          const x0i = sx | 0, y0i = sy | 0, fx = sx - x0i, fy = sy - y0i;
          const i00 = (y0i * W + x0i) * 4, i10 = i00 + 4, i01 = i00 + W * 4, i11 = i01 + 4, di = (y * W + x) * 4;
          /* gui2's deliberate divergence from the header: alpha is sampled like the colours,
             so the mark keeps its transparency over the chassis */
          for (let ch = 0; ch < 4; ch++) {
            const top = src[i00 + ch] + (src[i10 + ch] - src[i00 + ch]) * fx, bot = src[i01 + ch] + (src[i11 + ch] - src[i01 + ch]) * fx;
            dst[di + ch] = (top + (bot - top) * fy + 0.5) | 0;
          }
        }
      }
    }
  };
}
/* the mark's CSS box and its 2× bitmap (gui2: "2x BITMAP, SAME CSS BOX"); everything else is
   gui2's 146×96 box scaled by LOGO_H/96, so the authored look holds at this size */
const LOGO_W = 91, LOGO_H = 60;                  // B323: was 70×46 (B315); the CSS box says why this size
/* B334 (human 2026-09-28: "the logo sits a little high in its frame"): where the undistorted mark's
   middle line is baked, as a fraction of the box. B315 baked it at 0.38 ("high in the box: the blobs
   pull downward hardest"), and measured over the warp's motion (64 moments) its ink's centre of mass
   (the fill and its edge) then sat 5.2 CSS px above the box's middle. The box IS its frame: it spans the frame's top padding to
   the body's top edge (B323's negative margins), so centring in the box centres it in the frame. 0.46
   puts the time-averaged centre of mass on the middle (checkB334 measures it, with 0.38 as its
   control); the warp's deepest pull touches the box's bottom row at 1 moment in 64. */
const LOGO_TEXT_Y = 0.46;
const LOGO = { cv: null, ctx: null, W: 2 * LOGO_W, H: 2 * LOGO_H, SRC: null, DST: null, ink: '', warp: null,
  baseWave: 0, baseTrem: 0, t: 0, clk: -1, n: 0, paints: 0, smD: 0.28, smX: 0.5, smY: 0.5, hue: 0, tint: '', edge: '', ms: 0, reduce: false,
  aux: null, textY: LOGO_TEXT_Y, style: 0, tag: null };
/* B334 — THE LOGO'S STYLES, auditioned by clicking it. Human, 2026-09-28: "Could we test a stylized
   drop shadow under the horde logo? Or perhaps a small inner shadow, giving the fill some depth within
   the stroke? ... I'm also considering adding a mild gradient to the fill. Maybe we can audition a few
   styles that I can cycle through by clicking the logo." Every style is a few canvas composites on the
   mark's own 182×120 bitmap after the warp (logoOutline), never per-pixel script, so its cost is a
   small fixed add to the warp's (checkB334 reports it per style), and a shed frame skips it with the
   warp (J2). Every colour is derived from the fill of the moment (LOGO.tint), so each style still
   shifts with the morph cursor and the drift.
     FLAT      — rounds 8-10's mark: the fill and its 120° edge.
     DROP      — a crisp offset shadow (1.5, 2 CSS px), in the fill's third triad colour (turned 240°,
                 the edge being 120°) pulled 35% toward the chassis ink: a printed, off-register look.
     INSET     — a small inner shadow inside the stroke: the fill's own colour, darkened, cast 1.2 px
                 down from the top inner edge with a 1 px blur, clipped to the fill.
     GRADIENT  — a mild vertical gradient on the fill: its colour lifted 24% toward white at the top of
                 the letters, lowered 20% toward black at their foot, the colour itself at the middle.
     INSET + GRADIENT — both.
     DROP + INSET + GRADIENT — B364 (human, 2026-09-29: "I really like the logo with the hard
                 colored drop shadow; I also like the inset+gradient mode. Could we compose these
                 two?"): all three passes, unchanged from their own styles above. Paint order (set
                 by logoOutline/logoTint, not by this entry) is: the fill uses GRADIENT's stops;
                 INSET composites inside the fill's own silhouette; DROP composites last, behind
                 everything already drawn (destination-over), so the crisp shadow sits BENEATH the
                 inset-and-gradient letters — never muddying the inset edge, since the two never
                 occupy the same pixel (the inset stays inside the glyph, the drop shows only
                 outside it). No colour adjustment was needed for either theme: DROP's shadow
                 colour (the fill turned 240°, pulled toward the chassis ink) and the gradient's
                 lift/lower both already derive from the fill of the moment (checkB334).
     SOFT DROP — the soft alternative to DROP (B334's brief: "crisp or soft"): a blurred shadow straight
                 down, the chassis ink at 38% on the light chassis; on the dark one, where a dark shadow
                 cannot show, the fill's own colour at 55%, which reads as a glow.
     B368 (human, 2026-09-29: "Is the logo gradient a hue gradient or a value gradient? It's hard to
     tell. I would be curious to also audition a hue gradient that's a bit more dramatic, and also at
     more of a diagonal."). GRADIENT above is a VALUE gradient (the fill lifted toward white, lowered
     toward black; its hue does not move). Two styles join the END of the cycle, the seven above
     unchanged:
     HUE ↘     — a HUE gradient over the whole mark's box on a diagonal, top-left to bottom-right, its
                 axis 35° below horizontal (LOGO_HUE_ANGLE), stretched so the box's top-left and
                 bottom-right corners are its two ends. The hue turns through 150° (LOGO_HUE_SPAN: from
                 75° before the fill's hue to 75° after it) in OKLCH, a perceptual space, holding ONE
                 lightness and the fill's chroma, so it reads as hue and not as value. The lightness is
                 the fill's OKLCH L, capped at 0.80 (LOGO_HUE_LMAX): MEASURED, a pale fill (the drift's
                 yellow, #e7f929, L 0.93) held at its own L left the cyan end only 0.033 of chroma
                 inside sRGB, a grey, and its hue turn read 146° of 150; at 0.80 every hue of the span
                 keeps real chroma. Where a hue still cannot hold the fill's chroma inside sRGB, the
                 chroma alone is lowered (logoOklchHex), never the lightness or the hue. Thirteen stops,
                 so the canvas's sRGB blend between them stays on the OKLCH path. It follows the fill of
                 the moment, so the morph and the drift turn the whole gradient.
     DROP + INSET + HUE ↘ — B364's composite with the hue gradient in place of the value gradient. */
const LOGO_STYLES = [
  { id: 'flat', name: 'FLAT' },
  { id: 'drop', name: 'DROP', drop: 'crisp' },
  { id: 'inset', name: 'INSET', inset: true },
  { id: 'gradient', name: 'GRADIENT', grad: true },
  { id: 'inset-gradient', name: 'INSET + GRADIENT', inset: true, grad: true },
  { id: 'drop-inset-gradient', name: 'DROP + INSET + GRADIENT', drop: 'crisp', inset: true, grad: true },
  { id: 'soft', name: 'SOFT DROP', drop: 'soft' },
  { id: 'hue-diag', name: 'HUE ↘', hue: true },                                       // B368
  { id: 'drop-inset-hue', name: 'DROP + INSET + HUE ↘', drop: 'crisp', inset: true, hue: true }   // B368
];
/* per viewer: this browser's storage, read once when the mark is made; ?logo=<id> overrides it for a
   link or a screenshot. Storage can throw (a file: page, a locked-down profile): the lab then just
   forgets, and the style is the default on the next load. B375 (the human, 2026-09-29: "I love the
   drop/inset/hue gradient logo; let's lock it in"): the DEFAULT, what a viewer with nothing stored (or
   an unknown id) sees, is DROP + INSET + HUE ↘, found by id so the cycle's order stays free; before
   B375 it was index 0, FLAT. The cycle order is unchanged, so a click from the default goes to FLAT. */
const LOGO_STYLE_KEY = 'scalpel-lab.logo-style';
const LOGO_DEFAULT = LOGO_STYLES.findIndex(s => s.id === 'drop-inset-hue');
function logoStyleLoad() {
  let id = qp('logo', '');
  if (!id) { try { id = localStorage.getItem(LOGO_STYLE_KEY) || ''; } catch (_) { id = ''; } }
  const i = LOGO_STYLES.findIndex(s => s.id === id);
  return i >= 0 ? i : LOGO_DEFAULT;
}
function logoStyleSave(i) { try { localStorage.setItem(LOGO_STYLE_KEY, LOGO_STYLES[i].id); return true; } catch (_) { return false; } }
/* the click: the next style, saved, painted now (even with the clock standing: a still keeps its
   motion frozen but shows the new style), and its name shown for a moment under the mark */
function logoCycle() {
  LOGO.style = (LOGO.style + 1) % LOGO_STYLES.length;
  logoStyleSave(LOGO.style);
  if (LOGO.reduce) logoBake(); else logoFrame(true);
  const t = LOGO.tag;
  if (t) { t.textContent = LOGO_STYLES[LOGO.style].name; t.classList.remove('show'); void t.offsetWidth; t.classList.add('show'); }
  if (LOGO.cv) LOGO.cv.title = logoTitle();
}
function logoTitle() {
  return 'horde — gui2\'s warping wordmark (B315): colour = the morph cursor (morph on) or a slow drift (off) · edge = that colour turned 120° (gui2 hue-test, B317) · ripple = R · wiggle = Detune' +
    '\nstyle: ' + LOGO_STYLES[LOGO.style].name + ' (' + (LOGO.style + 1) + ' of ' + LOGO_STYLES.length + ') — click for the next (B334); your choice is kept in this browser';
}
const LOGO_LG = ['--lgA', '--lgB', '--lgC', '--lgD'];
function logoMix(a, b, f) {
  const pa = /^#?(..)(..)(..)$/.exec(a), pb = /^#?(..)(..)(..)$/.exec(b);
  if (!pa || !pb) return a;
  const c = i => Math.round(parseInt(pa[i], 16) + (parseInt(pb[i], 16) - parseInt(pa[i], 16)) * f).toString(16).padStart(2, '0');
  return '#' + c(1) + c(2) + c(3);
}
function logoHueRot(hex, deg) {                    // gui2.html:6390-6406
  const m = /^#?([0-9a-f]{6})$/i.exec(hex); if (!m) return hex;
  const v = parseInt(m[1], 16);
  let r = (v >> 16 & 255) / 255, g = (v >> 8 & 255) / 255, b = (v & 255) / 255;
  const mx = Math.max(r, g, b), mn = Math.min(r, g, b), l = (mx + mn) / 2;
  let h = 0, sN = 0;
  if (mx !== mn) {
    const d = mx - mn;
    sN = l > 0.5 ? d / (2 - mx - mn) : d / (mx + mn);
    h = mx === r ? ((g - b) / d + (g < b ? 6 : 0)) : mx === g ? (b - r) / d + 2 : (r - g) / d + 4;
    h /= 6;
  }
  h = (h + deg / 360) % 1;
  const q = l < 0.5 ? l * (1 + sN) : l + sN - l * sN, pp = 2 * l - q;
  const f = t => { t = (t + 1) % 1; return t < 1 / 6 ? pp + (q - pp) * 6 * t : t < 1 / 2 ? q : t < 2 / 3 ? pp + (q - pp) * (2 / 3 - t) * 6 : pp; };
  const to2 = x => Math.round(x * 255).toString(16).padStart(2, '0');
  return '#' + to2(f(h + 1 / 3)) + to2(f(h)) + to2(f(h - 1 / 3));
}
/* one canvas for the lab's life: rebuild() re-parents it, so the warp, its bake and its clock
   survive every rebuild (a new canvas per rebuild would re-bake and restart the mark) */
function logoCanvas() {
  if (LOGO.cv) return LOGO.cv;
  const cv = el('canvas', 'logo');
  cv.width = LOGO.W; cv.height = LOGO.H;
  cv.setAttribute && cv.setAttribute('role', 'img'); cv.setAttribute && cv.setAttribute('aria-label', 'horde');
  LOGO.style = logoStyleLoad();
  cv.title = logoTitle();
  cv.addEventListener('click', logoCycle);
  LOGO.tag = el('span', 'logoname');
  LOGO.cv = cv;
  LOGO.ctx = cv.getContext ? cv.getContext('2d', { willReadFrequently: true }) : null;
  LOGO.reduce = !!(typeof matchMedia === 'function' && matchMedia('(prefers-reduced-motion: reduce)').matches);
  const K = (90 * LOGO.H / 192) / 200;             // gui2: OUR_FONT / TD_AUTHORED_FONT, at this bitmap
  const w = tdWarp(7);
  w.params.pxScale = K; w.params.waveAmp *= K; w.params.tremor *= K;
  LOGO.baseWave = w.params.waveAmp; LOGO.baseTrem = w.params.tremor;
  /* the human's tool session, verbatim (gui2.html:6486-6493): amp/rad scale by K */
  w.addBlob(117.67534376865139 * K, 47.87252228922112 * K, 0.7035143339442531, 0.3147373015901911, 2, 3.4454990687082927);
  w.addBlob(71.4473354749417 * K, 79.18600416618574 * K, 0.7303262990254228, 0.20458152460366485, 2, 1.1934938693140864);
  w.addSwirl(134.69669851752556 * K, -0.5233313135724312, -0.48917431962495467, 1, 4.827469642758654);
  w.addSwirl(108.93653939929409 * K, 0.532155537777295, -0.6532128975413898, 2, 0.717080792499773);
  LOGO.warp = w;
  logoBake();
  return cv;
}
/* bake the undistorted mark once per ink (gui2's bake(): the warp then reads a static source) */
function logoBake() {
  const W = LOGO.W, H = LOGO.H;
  if (!LOGO.ctx || typeof document === 'undefined' || !document.createElement) return;
  const mk = () => { const c = document.createElement('canvas'); c.width = W; c.height = H; return c; };
  if (!LOGO.aux) { const s = mk(), e = mk(), f = mk(), i = mk(), j = mk(), d = mk();   // i, j, d: B334's style layers
    LOGO.aux = { s, e, f, i, j, d, sx: s.getContext('2d', { willReadFrequently: true }), ex: e.getContext('2d'), fx: f.getContext('2d'),
      ix: i.getContext('2d'), jx: j.getContext('2d'), dx: d.getContext('2d') }; }
  const A = LOGO.aux, ink = TOK('--text', '#191521');
  if (!A.sx || !A.sx.getImageData) return;
  A.sx.clearRect(0, 0, W, H);
  A.sx.fillStyle = ink;
  A.sx.font = '700 ' + (90 * H / 192).toFixed(2) + 'px Helvetica,"Helvetica Neue",Arial,sans-serif';
  A.sx.textBaseline = 'middle';
  A.sx.fillText('horde', 16 * H / 192, H * LOGO.textY);   // B334: centred by its warped mass (LOGO_TEXT_Y), was 0.38
  LOGO.SRC = A.sx.getImageData(0, 0, W, H);
  LOGO.DST = LOGO.ctx.createImageData(W, H);
  LOGO.ink = ink;
  LOGO.ctx.putImageData(LOGO.SRC, 0, 0);
  logoTint(0); logoOutline();
}
/* colour (gui2.html:6505-6585's tint), from the morph when it is on */
function logoTint(dt) {
  const g = LOGO.ctx, W = LOGO.W, H = LOGO.H;
  let fill;
  if (!MORPH.on) {
    LOGO.hue = (LOGO.hue + dt * 0.02) % 1;
    const m = /^#([0-9a-f]{6})$/i.exec(logoHueRot(TOK(LOGO_LG[0], '#FF2E88'), LOGO.hue * 360));
    fill = TOK(LOGO_LG[0], '#FF2E88');
    if (m) {                                        // 18% toward its own luma (gui2, 2026-09-01)
      const v = parseInt(m[1], 16); let r = v >> 16 & 255, gg = v >> 8 & 255, b = v & 255;
      const l = 0.299 * r + 0.587 * gg + 0.114 * b;
      r = Math.round(r + (l - r) * 0.18); gg = Math.round(gg + (l - gg) * 0.18); b = Math.round(b + (l - b) * 0.18);
      fill = '#' + ((1 << 24) + (r << 16) + (gg << 8) + b).toString(16).slice(1);
    }
  } else {
    LOGO.smX += (MORPH.x - LOGO.smX) * 0.1; LOGO.smY += (MORPH.y - LOGO.smY) * 0.1;
    const top = logoMix(TOK(LOGO_LG[0], '#FF2E88'), TOK(LOGO_LG[1], '#00D5C8'), LOGO.smX);
    const bot = logoMix(TOK(LOGO_LG[2], '#FF2E88'), TOK(LOGO_LG[3], '#00D5C8'), LOGO.smX);
    fill = logoMix(top, bot, LOGO.smY);
  }
  LOGO.tint = fill;
  const st = LOGO_STYLES[LOGO.style];                 // B368: a style's fill is flat, the value gradient or the hue gradient
  g.globalCompositeOperation = 'source-in'; g.fillStyle = st.hue ? logoHueGradient(g, fill) : st.grad ? logoGradient(g, fill) : fill; g.fillRect(0, 0, W, H); g.globalCompositeOperation = 'source-over';
}
/* B334's GRADIENT: the fill of the moment, lifted at the top of the letters and lowered at their
   foot. The stops span the baked x-height band (textY ∓ 0.2 of the box), where the letters are; the
   warp moves ink beyond it, and beyond the stops the gradient holds its end colours. */
const LOGO_GRAD_UP = 0.24, LOGO_GRAD_DOWN = 0.14;
function logoGradient(g, fill) {
  const H = LOGO.H, gr = g.createLinearGradient(0, H * (LOGO.textY - 0.2), 0, H * (LOGO.textY + 0.2));
  gr.addColorStop(0, logoMix(fill, '#ffffff', LOGO_GRAD_UP));
  gr.addColorStop(0.5, fill);
  gr.addColorStop(1, logoMix(fill, '#000000', LOGO_GRAD_DOWN));
  return gr;
}
/* B368's HUE ↘ (the style block above says what and why). OKLab/OKLCH are Björn Ottosson's (2020)
   matrices, with the sRGB transfer curve. logoOklch reads a #rrggbb as [L, C, h°]; logoOklchHex writes
   one back, lowering C by bisection until the colour is inside sRGB (L and h held exactly). */
const LOGO_HUE_SPAN = 150, LOGO_HUE_ANGLE = 35, LOGO_HUE_STOPS = 13, LOGO_HUE_LMAX = 0.8;
const logoLin = c => (c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4));
const logoGam = c => (c <= 0.0031308 ? 12.92 * c : 1.055 * Math.pow(c, 1 / 2.4) - 0.055);
function logoOklch(hex) {
  const m = /^#?([0-9a-f]{6})$/i.exec(String(hex).trim()); if (!m) return [0, 0, 0];
  const v = parseInt(m[1], 16), r = logoLin((v >> 16 & 255) / 255), g = logoLin((v >> 8 & 255) / 255), b = logoLin((v & 255) / 255);
  const l = Math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b), mm = Math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b),
    s = Math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
  const L = 0.2104542553 * l + 0.7936177850 * mm - 0.0040720468 * s, A = 1.9779984951 * l - 2.4285922050 * mm + 0.4505937099 * s,
    B = 0.0259040371 * l + 0.7827717662 * mm - 0.8086757660 * s;
  return [L, Math.hypot(A, B), ((Math.atan2(B, A) * 180 / Math.PI) + 360) % 360];
}
function logoOklchRgb(L, C, h) {
  const a = C * Math.cos(h * Math.PI / 180), b = C * Math.sin(h * Math.PI / 180);
  const l = Math.pow(L + 0.3963377774 * a + 0.2158037573 * b, 3), m = Math.pow(L - 0.1055613458 * a - 0.0638541728 * b, 3), s = Math.pow(L - 0.0894841775 * a - 1.2914855480 * b, 3);
  return [4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s, -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s, -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s];
}
function logoOklchHex(L, C, h) {
  const inG = c => c.every(x => x >= -1e-6 && x <= 1 + 1e-6);
  let lo = 0, hi = C, rgb = logoOklchRgb(L, C, h);
  if (!inG(rgb)) { for (let i = 0; i < 24; i++) { const mid = (lo + hi) / 2; if (inG(logoOklchRgb(L, mid, h))) lo = mid; else hi = mid; } rgb = logoOklchRgb(L, lo, h); }
  return '#' + rgb.map(x => Math.round(clamp(logoGam(clamp(x, 0, 1)), 0, 1) * 255).toString(16).padStart(2, '0')).join('');
}
function logoHueGradient(g, fill) {
  const W = LOGO.W, H = LOGO.H, th = LOGO_HUE_ANGLE * Math.PI / 180, ux = Math.cos(th), uy = Math.sin(th);
  const half = (W / 2) * ux + (H / 2) * uy;          // the box's top-left and bottom-right corners project to ∓half
  const gr = g.createLinearGradient(W / 2 - ux * half, H / 2 - uy * half, W / 2 + ux * half, H / 2 + uy * half);
  const [L0, C, h] = logoOklch(fill), L = Math.min(L0, LOGO_HUE_LMAX);
  for (let i = 0; i < LOGO_HUE_STOPS; i++) { const f = i / (LOGO_HUE_STOPS - 1); gr.addColorStop(f, logoOklchHex(L, C, h + (f - 0.5) * LOGO_HUE_SPAN)); }
  return gr;
}
/* B334's shadows. Both use the canvas shadow (universal, unlike ctx.filter): the source is drawn a
   whole box-width off to the left and the shadow offset brings only the shadow back, so nothing
   but the shadow lands. Offsets and blurs are in bitmap px (2× the CSS px). */
function logoShadowOf(g, src, col, dx, dy, blur) {
  const W = LOGO.W;
  g.save(); g.shadowColor = col; g.shadowBlur = blur; g.shadowOffsetX = 2 * W + dx; g.shadowOffsetY = dy;
  g.drawImage(src, -2 * W, 0); g.restore();
}
function logoDarkChassis() { const c = /^#?([0-9a-f]{6})$/i.exec(TOK('--bg', '#F2EDE2')); if (!c) return false; const v = parseInt(c[1], 16); return (0.299 * (v >> 16 & 255) + 0.587 * (v >> 8 & 255) + 0.114 * (v & 255)) < 110; }
/* INSET: the inverse of the fill (opaque everywhere but the letters) casts its shadow down into
   them, and only what lands inside the fill is kept */
function logoInset(g) {
  const A = LOGO.aux, W = LOGO.W, H = LOGO.H;
  A.ix.globalCompositeOperation = 'source-over'; A.ix.clearRect(0, 0, W, H); A.ix.fillStyle = '#000'; A.ix.fillRect(0, 0, W, H);
  A.ix.globalCompositeOperation = 'destination-out'; A.ix.drawImage(A.f, 0, 0); A.ix.globalCompositeOperation = 'source-over';
  A.jx.clearRect(0, 0, W, H);
  logoShadowOf(A.jx, A.i, RGBA(logoMix(LOGO.tint, '#000000', 0.5), 0.8), 0.8, 2.4, 2);
  A.jx.globalCompositeOperation = 'destination-in'; A.jx.drawImage(A.f, 0, 0); A.jx.globalCompositeOperation = 'source-over';
  g.drawImage(A.j, 0, 0);
}
/* DROP and SOFT DROP: the composed mark (edge and fill) casts its shadow BEHIND itself */
function logoDropColour(soft) {
  if (!soft) return logoMix(logoHueRot(LOGO.tint, 240), TOK('--text', '#191521'), 0.35);
  return logoDarkChassis() ? RGBA(LOGO.tint, 0.55) : RGBA(TOK('--text', '#191521'), 0.38);
}
function logoDrop(g, soft) {
  const A = LOGO.aux, W = LOGO.W, H = LOGO.H;
  A.dx.clearRect(0, 0, W, H); A.dx.drawImage(LOGO.cv, 0, 0);
  g.save(); g.globalCompositeOperation = 'destination-over';
  if (soft) logoShadowOf(g, A.d, logoDropColour(true), 0, 3, 5); else logoShadowOf(g, A.d, logoDropColour(false), 3, 4, 0);
  g.restore();
}
/* THE EDGE (B317). Human, 2026-09-28: "please replace the black border with the version that
   shifts color". gui2 has two edges (gui2.html:6384-6420): its DEFAULT is the ink one, --lg-edge
   (#191521 on the light chassis, none on the dark: B315 ported that, and it is the black border
   the human means), and its HUE-TEST, the human's own audition of 2026-08-29, where the edge is
   the fill's colour turned 120° round the hue wheel (`hueRot(lastTint, 120)`, :6414; 90° was
   auditioned first and the human asked for 120). The fill already shifts (the morph cursor, or
   the ADR-150 drift), so the edge shifts with it and stays a third of the wheel away. Ported as
   gui2's code path runs it: the hue-test edge replaces the ink one in BOTH themes (gui2's
   `if (brandHueTest && lastTint) edge = …` overrides dark's `none` too). --lg-edge stays in the
   token set (copied from gui2) and is no longer read. Same eight stamps, fill on top. */
const LOGO_EDGE_TURN = 120;
function logoEdgeColour() { return LOGO.tint ? logoHueRot(LOGO.tint, LOGO_EDGE_TURN) : TOK('--lg-edge', 'none'); }
function logoOutline() {
  const edge = logoEdgeColour(), A = LOGO.aux, g = LOGO.ctx, W = LOGO.W, H = LOGO.H;
  LOGO.edge = edge;
  if (!edge || edge === 'none' || !A) return;
  A.fx.clearRect(0, 0, W, H); A.fx.drawImage(LOGO.cv, 0, 0);
  A.ex.clearRect(0, 0, W, H); A.ex.drawImage(LOGO.cv, 0, 0);
  A.ex.globalCompositeOperation = 'source-in'; A.ex.fillStyle = edge; A.ex.fillRect(0, 0, W, H); A.ex.globalCompositeOperation = 'source-over';
  g.clearRect(0, 0, W, H);
  const R = 2.6;                                    // gui2's EDGE_R on a 2× bitmap (~1.3 px on screen)
  for (const [x, y] of [[1, 0], [-1, 0], [0, 1], [0, -1], [0.7, 0.7], [-0.7, 0.7], [0.7, -0.7], [-0.7, -0.7]]) g.drawImage(A.e, x * R, y * R);
  g.drawImage(A.f, 0, 0);
  const st = LOGO_STYLES[LOGO.style];               // B334: the style's passes, on the composed mark
  if (st.inset && A.ix) logoInset(g);
  if (st.drop && A.dx) logoDrop(g, st.drop === 'soft');
}
/* one paint's worth: advance on the ENGINE clock, at most every other frame (gui2's 30 fps cap on
   a 60 Hz display), and nothing at all while the clock stands (a frozen still keeps its mark) */
function logoFrame(force) {
  if (!LOGO.ctx || !LOGO.SRC || !LOGO.cv.isConnected) return;
  if (TOK('--text', '#191521') !== LOGO.ink) { logoBake(); force = true; }
  if (LOGO.reduce) return;
  if (LOGO.clk < 0 || LOGO.clk > MON.clock) LOGO.clk = MON.clock;   // the first paint, or a reset clock (B323, the freeze: E3)
  const clk = MON.clock, dt = Math.min(0.25, Math.max(0, clk - LOGO.clk) / SR);
  if (!force && (dt <= 0 || (++LOGO.n % (LOAD.shed ? SHED_LOGO : 2)))) return;
  LOGO.clk = clk;
  const t0 = typeof performance === 'object' ? performance.now() : 0;
  LOGO.smD += (clamp((EFF.detune || 0) / 100, 0, 1) - LOGO.smD) * 0.06;
  const w = LOGO.warp;
  w.params.waveAmp = LOGO.baseWave * (0.82 + 0.64 * LOGO.smD);
  w.params.tremor = LOGO.baseTrem * (0.6 + 1.0 * Math.max(0, MON.rSm));
  LOGO.t += dt * 0.35;                              // gui2's SPEED
  w.render(LOGO.SRC.data, LOGO.DST.data, LOGO.W, LOGO.H, LOGO.t);
  LOGO.ctx.putImageData(LOGO.DST, 0, 0);
  logoTint(dt); logoOutline();
  LOGO.paints++;
  if (typeof performance === 'object') LOGO.ms = performance.now() - t0;   // cost only: never reaches the drawing
}

/* ======== E3, verbatim ======== */
const SPECIMEN_ZOOM = 1.25;
/* the pearl's PHYSICS state, one fresh copy per call: the live Specimen's, and checkB365's scratch ones.
   B365: `act` is how much the RIPPLE/QUIVER are still driven by a note (1 while sound plays, settling
   to exactly 0 in the silence after it); `legacy` true is the pre-B365 law (nothing ever settles),
   kept only as the check's control. B365 correction: `freezeIdle` true reproduces the MERGED-B365 bug
   (idle motion frozen at rest too), kept only as the check's must-fail control — never true live. */
function specimenState() {
  return { yaw: 0.55, pit: 0.18, yawT: 0.55, pitT: 0.18, dent: { x: 0, v: 0 }, poke: [0, 0, 1], time: 3.7,
    gates: {}, shySm: 0, kSm: undefined, frontIx: 0, rng: mulberry32(0xC0FFEE), act: 1, legacy: false, freezeIdle: false,
    fronts: [0, 1, 2, 3].map(() => ({ age: 100, az: 0, el: 0, wave: 9, hold: -1, heldAmt: 0 })) };
}
const SPECIMEN = Object.assign({ on: qp('specimen', '1') !== '0', cv: null, gl: null, u: {}, fail: '', ms: 0, draws: 0,
  xyLast: null, xyYaw: 0, xyPit: 0, center: true, mask: 0, fit: null, clk: -1, n: 0,
  bgTok: '', bg: [0.95, 0.93, 0.89], w: 0, h: 0, U: null }, specimenState());
function specimenCanvas() {
  if (!SPECIMEN.cv) SPECIMEN.cv = el('canvas', 'specimen');
  return SPECIMEN.cv;
}
/* the pad shows it only when the context is live and drawing, never on a promise */
function specimenLive() { return !!(SPECIMEN.on && SPECIMEN.gl && !SPECIMEN.fail && LAB.page === 'main'); }
const SPECIMEN_FS = `precision highp float;
uniform vec2 uRes;uniform float uTime;uniform float uK;uniform int uVariant;
uniform vec3 uPoke;uniform float uDent;uniform vec2 uOrbit;uniform float uThick;
uniform vec3 uPokeN[4];uniform float uStrikeN[4];uniform float uRWaveN[4];uniform float uHeldN[4];
uniform float uRAmp;uniform float uShy;
uniform vec3 uBg;uniform float uZoom;uniform vec2 uShift;uniform int uMask;
float smin(float a,float b,float k){float h=clamp(.5+.5*(b-a)/k,0.,1.);return mix(b,a,h)-k*h*(1.-h);}
float map(vec3 p){
 float t=uTime*.35;
 float qv=uShy*.016*sin(t*20.+p.y*9.+p.x*7.);
 p+=qv;
 vec3 c1=vec3(sin(t*.7)*.33,sin(t*.9+1.7)*.2,cos(t*.6)*.28);
 vec3 c2=vec3(cos(t*.5+3.)*.38,cos(t*.8)*.22,sin(t*.7+1.)*.3);
 vec3 c3=vec3(sin(t*.6+5.)*.28,cos(t*.5+2.)*.27,cos(t*.9+4.)*.22);
 float d=length(p-c1)-.6;
 d=smin(d,length(p-c2)-.48,uK);
 d=smin(d,length(p-c3)-.4,uK);
 vec3 q=p-uPoke;
 d+=uDent*exp(-dot(q,q)*16.);
 for(int i=0;i<4;i++){
  float rd2=distance(normalize(p),normalize(uPokeN[i]));
  if(uStrikeN[i]<3.5&&uRAmp>0.){
   float front=rd2*3.2-uStrikeN[i]*2.6;
   float env2=exp(-uStrikeN[i]*1.9)*exp(-front*front*2.2)*smoothstep(0.,.12,uStrikeN[i]);
   d+=.085*uRAmp*env2*sin(front*uRWaveN[i]);
  }
  if(uHeldN[i]>.001){
   d+=.05*uRAmp*uHeldN[i]*exp(-rd2*2.2)*sin(rd2*3.2*uRWaveN[i]-uTime*min(2.6*uRWaveN[i],26.));
  }
 }
 return d;
}
vec3 calcN(vec3 p){
 vec2 e=vec2(.0015,-.0015);
 return normalize(e.xyy*map(p+e.xyy)+e.yyx*map(p+e.yyx)+e.yxy*map(p+e.yxy)+e.xxx*map(p+e.xxx));
}
vec3 lobe(vec3 r,vec3 d,float p,vec3 c){return c*pow(max(dot(r,normalize(d)),0.),p);}
vec3 env(vec3 r){
 float up=smoothstep(-1.,1.,r.y);
 vec3 c=mix(vec3(.52,.52,.60),vec3(1.04,1.02,1.06),up);
 c=mix(c,vec3(.30,.29,.37),smoothstep(-.15,-1.,r.y));
 c+=lobe(r,vec3(.5,.9,.3),34.,vec3(1.5,1.48,1.52));
 c+=lobe(r,vec3(-.8,.5,.5),70.,vec3(.85,.86,.95));
 c+=lobe(r,vec3(.2,.1,-1.),50.,vec3(.5,.48,.55));
 if(uVariant==2){
  c+=lobe(r,vec3(.95,.05,-.35),18.,vec3(.9,.05,.45));
  c+=lobe(r,vec3(-.95,-.05,-.3),18.,vec3(.12,.6,.76));
 }
 return c;
}
vec3 film(float c){
 vec3 lam=vec3(650.,540.,450.);
 vec3 ph=12.566*1.35*uThick/lam*max(c,.05);
 return .55+.45*cos(ph);
}
void main(){
 vec2 sv=(gl_FragCoord.xy*2.-uRes)/uRes.y;
 vec2 uv=sv+uShift;
 float yaw=uOrbit.x,pit=clamp(uOrbit.y,-1.15,1.15);
 vec3 ro=vec3(cos(pit)*sin(yaw),sin(pit),cos(pit)*cos(yaw))*2.75;
 vec3 ww=normalize(-ro);
 vec3 uu=normalize(cross(vec3(0.,1.,0.),ww));
 vec3 vv=cross(ww,uu);
 vec3 rd=normalize(uv.x*uu+uv.y*vv+2.1*uZoom*ww);
 vec3 paperHi=uBg*1.035;vec3 paperLo=uBg*.93;
 vec3 bg=mix(paperLo,paperHi,smoothstep(-1.,.9,sv.y));
 vec3 col=bg;
 float t=0.;bool hit=false;
 for(int i=0;i<64;i++){
  vec3 p=ro+rd*t;
  float d=map(p);
  if(d<.0012*t+.0008){hit=true;break;}
  t+=d*.9;
  if(t>7.)break;
 }
 if(uMask==1){gl_FragColor=vec4(hit?1.:0.,0.,0.,1.);return;}
 if(hit){
  vec3 p=ro+rd*t;
  vec3 n=calcN(p);
  vec3 r=reflect(rd,n);
  float fr=pow(1.-max(dot(n,-rd),0.),5.);
  vec3 c=env(r);
  c*=mix(vec3(.86,.87,.93),vec3(1.),fr);
  if(uVariant==1){c*=mix(vec3(1.),film(dot(n,-rd)),.75);}
  c=c/(1.+c*.15);
  col=pow(max(c,vec3(0.)),vec3(.4545));
 } else if(rd.y<-.02){
  float tp=(-1.25-ro.y)/rd.y;
  if(tp>0.&&tp<12.){
   vec3 p=ro+rd*tp;
   float o1=clamp(map(p+vec3(0.,.28,0.))/.28,0.,1.);
   float o2=clamp(map(p+vec3(0.,.75,0.))/.75,0.,1.);
   float sh=clamp(o1*.6+o2*.4,0.,1.);
   float fade=smoothstep(4.2,1.1,length(p.xz));
   vec3 g=mix(paperLo*.80,paperHi*.985,sh);
   col=mix(bg,g,fade);
  }
 }
 gl_FragColor=vec4(col,1.);
}`;
/* gui2's chromeInit, minus its pointer handlers (the XY owns the pointer here) */
function specimenInit() {
  const S = SPECIMEN, cv = S.cv;
  let gl = null, err = '';
  try { gl = cv.getContext('webgl', { antialias: false }) || cv.getContext('experimental-webgl', { antialias: false }); }
  catch (e) { err = String(e && e.message || e); }
  if (!gl) { S.fail = 'WebGL context refused' + (err ? ' (' + err + ')' : ''); return; }
  const mk = (type, src) => { const sh = gl.createShader(type); gl.shaderSource(sh, src); gl.compileShader(sh); return gl.getShaderParameter(sh, gl.COMPILE_STATUS) ? sh : null; };
  const v = mk(gl.VERTEX_SHADER, 'attribute vec2 a;void main(){gl_Position=vec4(a,0.,1.);}'), f = mk(gl.FRAGMENT_SHADER, SPECIMEN_FS);
  if (!v || !f) { S.fail = 'shader compile failed'; return; }
  const prog = gl.createProgram();
  gl.attachShader(prog, v); gl.attachShader(prog, f); gl.linkProgram(prog);
  if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) { S.fail = 'shader link failed'; return; }
  gl.useProgram(prog);
  const buf = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, buf);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);
  const loc = gl.getAttribLocation(prog, 'a');
  gl.enableVertexAttribArray(loc); gl.vertexAttribPointer(loc, 2, gl.FLOAT, false, 0, 0);
  for (const n of ['uRes', 'uTime', 'uK', 'uVariant', 'uPoke', 'uDent', 'uOrbit', 'uThick', 'uPokeN', 'uStrikeN', 'uRWaveN', 'uHeldN', 'uRAmp', 'uShy', 'uBg', 'uZoom', 'uShift', 'uMask'])
    S.u[n] = gl.getUniformLocation(prog, n);
  S.gl = gl;
}
/* gui2's chromeNotePoke: a fresh note strikes within view, its wavelength by pitch (midi 36..96
   → 6..24), its spot drawn from the seeded stream and then world-locked */
function specimenNotePoke(midi, S) {
  S = S || SPECIMEN;                                                  // B365: checkB365 strikes a scratch state
  const az = S.yaw + ((midi % 12) / 12 - 0.5) * 2.4, el = Math.max(-0.9, Math.min(0.9, (Math.floor(midi / 12) - 5) * 0.25));
  S.poke = [Math.cos(el) * Math.sin(az), Math.sin(el), Math.cos(el) * Math.cos(az)];
  S.dent.v = 6.0;
  const wave = 6 + 18 * Math.max(0, Math.min(1, (midi - 36) / 60));
  let ix = S.fronts.findIndex(f => f.hold === midi);
  if (ix < 0) {
    let best = -1;
    for (let i = 0; i < 4; i++) if (S.fronts[i].hold < 0 && S.fronts[i].age > best) { best = S.fronts[i].age; ix = i; }
    if (ix < 0) { ix = S.frontIx; S.frontIx = (S.frontIx + 1) % 4; }
  }
  const f = S.fronts[ix], rebind = f.hold === midi;
  f.age = 0; f.wave = wave; f.hold = midi;
  if (!rebind) {
    f.az = S.yaw + (S.rng() * 2 - 1) * 0.9;
    f.el = Math.max(-0.85, Math.min(0.85, S.pit * 0.6 + (S.rng() * 2 - 1) * 0.5));
  }
}
/* gui2's chromeFrame, driven by the monitor instead of the bridge's viz snapshot: the voices'
   envelopes and gates (MON.core), the level of the last rendered strobe (MON.L), R (MON.rSm) and
   Detune. Draws only when the engine clock moved, the canvas resized, the theme changed, or
   `force` (a check); returns true when it drew. */
/* B317: the XY's marker orbits the pearl with gui2's drag gains (E3's header). The baseline is
   dropped whenever the Specimen is not showing (specimenFrame's early returns), so a marker
   moved elsewhere (OSC's knobs, a preset) while it was hidden cannot land as one jolt on return.
   xyYaw / xyPit total the XY's share of the targets (the check reads them; nothing else does). */
function specimenFollowXY(S, cv) {
  const [u, v] = xyMarker(), W = cv.clientWidth, H = cv.clientHeight;
  if (S.xyLast && W > 0 && H > 0) {
    const y0 = S.yawT, p0 = S.pitT;
    S.yawT += (u - S.xyLast[0]) * W * 0.006;
    S.pitT = Math.max(-1.1, Math.min(1.1, S.pitT + (v - S.xyLast[1]) * H * 0.005));
    S.xyYaw += S.yawT - y0; S.xyPit += S.pitT - p0;
  }
  S.xyLast = [u, v];
}
/* B323 — KEEP IT CENTRED. Human, 2026-09-28: "sometimes it floats down low enough that it gets
   cut off by the bounding box; is there a way we can keep it centered to the space?" The pearl is
   three metaballs orbiting the origin (map(): centres c1..c3 swing up to 0.38, radii 0.6/0.48/0.4),
   so its silhouette wanders across the view, and at the XY's pitch limits (±1.1) it reached the pad's
   edge (trace b317). Every frame its projected bounds are computed on the CPU from the same three
   spheres, each grown by the surface's own excursions (quiver, fronts, held ripple and the smin bulge:
   SPEC_GROW, plus an outward dent while one rings), with gui2's camera exactly as the shader builds
   it. The view is then shifted so that box's centre sits at the pad's centre (uShift, an off-axis
   lens shift: the perspective is unchanged), and the focal length is reduced only if the box would
   not fit inside the pad with SPEC_MARGIN to spare (uZoom). Per-frame and exact, so it cannot lag
   and has nothing to ease. The sphere-to-screen extent is exact per axis: a sphere's x/z range is
   its xz-disc's tangent pair (x·z ± r·√(x²+z²−r²)) / (z²−r²). checkB323 sweeps the XY's reach and
   reads the drawn pearl back (uMask) against the pad's edges and centre. */
const SPEC_GROW = 0.12, SPEC_MARGIN = 0.1;
function specimenFit(yaw, pit, time, dent, aspect) {
  const t = time * 0.35, sn = Math.sin, cs = Math.cos;
  const balls = [[sn(t * 0.7) * 0.33, sn(t * 0.9 + 1.7) * 0.2, cs(t * 0.6) * 0.28, 0.6],
    [cs(t * 0.5 + 3) * 0.38, cs(t * 0.8) * 0.22, sn(t * 0.7 + 1) * 0.3, 0.48],
    [sn(t * 0.6 + 5) * 0.28, cs(t * 0.5 + 2) * 0.27, cs(t * 0.9 + 4) * 0.22, 0.4]];
  const p = Math.max(-1.15, Math.min(1.15, pit));                    // the shader's own clamp
  const ro = [cs(p) * sn(yaw) * 2.75, sn(p) * 2.75, cs(p) * cs(yaw) * 2.75];
  const ww = [-ro[0] / 2.75, -ro[1] / 2.75, -ro[2] / 2.75];
  const ul = Math.hypot(ww[2], ww[0]) || 1, uu = [ww[2] / ul, 0, -ww[0] / ul];   // normalize(cross(up, ww))
  const vv = [ww[1] * uu[2] - ww[2] * uu[1], ww[2] * uu[0] - ww[0] * uu[2], ww[0] * uu[1] - ww[1] * uu[0]];
  const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  const f = 2.1 * SPECIMEN_ZOOM, grow = SPEC_GROW + Math.max(0, -dent);
  let x0 = Infinity, x1 = -Infinity, y0 = Infinity, y1 = -Infinity;
  const span = (a, z, r) => { const q = Math.sqrt(Math.max(0, a * a + z * z - r * r)), den = Math.max(1e-6, z * z - r * r); return [f * (a * z - r * q) / den, f * (a * z + r * q) / den]; };
  for (const b of balls) {
    const d = [b[0] - ro[0], b[1] - ro[1], b[2] - ro[2]], r = b[3] + grow, z = dot(d, ww);
    const [a, c] = span(dot(d, uu), z, r), [e, g] = span(dot(d, vv), z, r);
    x0 = Math.min(x0, a); x1 = Math.max(x1, c); y0 = Math.min(y0, e); y1 = Math.max(y1, g);
  }
  const hx = (x1 - x0) / 2, hy = (y1 - y0) / 2;
  const k = Math.min(1, (1 - SPEC_MARGIN) / hy, (aspect - SPEC_MARGIN) / hx);
  return { shift: [k * (x0 + x1) / 2, k * (y0 + y1) / 2], zoom: SPECIMEN_ZOOM * k, k };
}
/* B365 — THE PEARL SETTLES WHEN THE SOUND ENDS, BUT ONLY THE NOTE-DRIVEN PART OF IT. Human,
   2026-09-29: "the specimen always keeps a ripple going after all notes have ended." MEASURED FIRST
   (trace b365; headless Chrome, audio on, A3 held 1 s and released): 4 s after the key-off the
   monitor still held note 57 gated, the front bound to it read held 1.00 (the shader's travelling
   "held" ripple at full amplitude) and the quiver read 0.35-0.45. TWO CAUSES, both fixed here:
   (1) A STALE FEED (L0064: a view is driven by what was rendered). The pearl read the MONITOR
       (MON.core, MON.L), the lab's second engine instance, which HOLDS its note by design (the cycle
       view and the ring draw it) and never hears a key-off. Its gate stayed up, so the held ripple
       ran for ever and its level kept the quiver alive. While the audio runs the pearl now reads what
       the AUDIO rendered: every SPEC_WIN blocks the worklet shell posts the peak of the samples it
       rendered and its engine's voices (feedOf), and specimenSource() hands that feed over. With the
       audio off nothing is heard, and the pearl follows the monitor's held note as every view does:
       unchanged.
   (2) A TIME-DRIVEN ANIMATION, gui2's metaballs orbiting on uTime and the studio turntable turning on
       dt — B365 AS FIRST MERGED gated this on `act` too, which the human then corrected (same day):
       "I still want the specimen to shift around when notes aren't playing, I just don't want the
       note-dependent ripples to trigger unless there's a note pressed." So `act` gates ONLY the
       note-driven terms — the strike/held ripple (rAmp) and the quiver (shy) below — never the drift
       or the turntable, which run on real `dt` unconditionally, exactly as they did before B365 (the
       IDLE motion). `act` is 1 while sound plays (a voice alive, or a window's peak at or over
       SPEC_QUIET, −90 dBFS); in the silence after it, act decays with τ SPEC_SETTLE and snaps to
       exactly 0 under 1e-3 (1.04 s), and the ripple/quiver amplitudes (scaled by act) follow it to
       exactly 0 — the fronts still age on real time. Because the idle motion never stops, the frame
       IS redrawn at rest now (gui2's old idle law, restored: see specimenFrame, which dropped the
       "nothing changed" skip B365 added).
   S.freezeIdle (default false, never set on the live SPECIMEN) reproduces the MERGED-B365 bug for
   the check only: with it, the drift/turntable use act-scaled time too, so they freeze exactly as
   B365-as-merged did, without a second copy of this function.
   WHILE SOUND PLAYS NOTHING CHANGES: act is exactly 1, every scaled term is x·1 = x, and the step is
   the pre-B365 arithmetic bit for bit (checkB365 compares the two laws on one feed). */
const SPEC_QUIET = Math.pow(10, -90 / 20), SPEC_SETTLE = 0.15, SPEC_WIN = 16;
const SPEC_SILENT = { t: 'vz', peak: 0, act: 0, env: 0, gates: [], lst: 0, lenv: 0 };
/* the largest |sample| over a block's channels, folded into `pk`. Pure; the worklet shell runs it too */
function blockPeak(chans, pk) {
  for (const ch of chans) for (let i = 0; i < ch.length; i++) { const a = ch[i] < 0 ? -ch[i] : ch[i]; if (a > pk) pk = a; }
  return pk;
}
/* one window of what an engine rendered, for the Specimen: the peak of its samples, the voices alive,
   the loudest voice envelope and the gated notes; and (B366) the NEWEST voice's envelope stage and level,
   which the master envelope's stage indicator follows (E4). Pure; serialized into the worklet shell
   (workletSource), and called on the ScriptProcessor fallback and on the monitor (monFeed). */
function feedOf(c, peak) {
  let act = 0, env = 0, lv = null;
  const gates = [];
  for (const v of c.voices) if (v.active) { act++; if (v.env > env) env = v.env; if (v.gate) gates.push(v.note); if (!lv || v.age > lv.age) lv = v; }
  return { t: 'vz', peak, act, env, gates, lst: lv ? lv.stage : 0, lenv: lv ? lv.env : 0 };
}
function feedIn(m) { if (m && m.t === 'vz') AUD.feed = m; }
/* the monitor's feed: its voices, and the left channel of its last strobe (the pre-B365 reading) */
function monFeed() {
  let peak = 0;
  if (MON.L) for (let i = 0; i < MON.L.length; i++) { const a = Math.abs(MON.L[i]); if (a > peak) peak = a; }
  return MON.core ? feedOf(MON.core, peak) : Object.assign({}, SPEC_SILENT, { peak });
}
/* what drives the pearl: the audio's rendered windows while it runs (silent until its first one
   arrives), else the monitor */
function specimenSource() { return AUD.on ? (AUD.feed || SPEC_SILENT) : monFeed(); }
function specSounding(src) { return src.act > 0 || src.peak >= SPEC_QUIET; }
/* one step of the pearl's physics: gui2's chromeFrame (strikes, the dent's spring, the turntable,
   the eased orbit, fronts, held ripples, quiver, bloom, tautness), on `S` (the live state or a
   check's), over dt seconds, from a feed. Returns the uniforms the shader is handed; no GL here. */
function specimenStep(S, dt, src, R) {
  const envMax = src.env, peak = src.peak, gatedNow = {};
  for (const n of src.gates) gatedNow[n] = true;
  for (const m in gatedNow) if (!S.gates[m]) specimenNotePoke(+m, S);   // a fresh attack strikes
  for (const m in S.gates) if (!gatedNow[m]) for (const f of S.fronts) if (f.hold === +m) f.hold = -1;
  S.gates = gatedNow;
  /* B365: the settle (the block above). legacy is the pre-B365 law: nothing ever settles */
  if (S.legacy || specSounding(src)) S.act = 1;
  else { S.act *= Math.exp(-dt / SPEC_SETTLE); if (S.act < 1e-3) S.act = 0; }
  /* B365 correction: dtI drives the IDLE motion (drift, turntable, tautness) — real dt, unconditionally,
     restoring gui2's pre-B365 law. freezeIdle (the check's control only) reproduces the merged-B365 bug
     by gating dtI on act too, the same way act now gates ONLY rAmp/shy below. */
  const act = S.act, dtI = S.freezeIdle ? dt * act : dt;
  const w = 9, z = 0.82, d = S.dent;                                  // the damped spring (w 9, ζ 0.82)
  d.v += (-w * w * d.x - 2 * z * w * d.v) * dt; d.x += d.v * dt;
  if (Math.abs(d.x) < 0.0005 && Math.abs(d.v) < 0.002) { d.x = 0; d.v = 0; }
  S.yawT += dtI * 0.03;                                               // slow studio turntable
  const ease = Math.min(1, dt * 7);
  S.yaw += (S.yawT - S.yaw) * ease; S.pit += (S.pitT - S.pit) * ease;
  S.time += dtI;
  const kT = Math.max(0.12, 0.45 - 0.3 * R);                          // coherence pulls taut, eased
  if (S.kSm === undefined) S.kSm = kT;
  S.kSm += (kT - S.kSm) * (kT < S.kSm ? 1 - Math.exp(-dtI / 0.28) : 1 - Math.exp(-dtI / 0.5));
  const pokes = new Float32Array(12), ages = new Float32Array(4), waves = new Float32Array(4), helds = new Float32Array(4);
  for (let i = 0; i < 4; i++) {
    const f = S.fronts[i];
    f.age = Math.min(10, f.age + dt);
    const hT = f.hold >= 0 ? 1 : 0;
    f.heldAmt += (hT - f.heldAmt) * (hT > f.heldAmt ? 1 - Math.exp(-dt / 0.25) : 1 - Math.exp(-dt / 0.45));
    pokes[i * 3] = Math.cos(f.el) * Math.sin(f.az); pokes[i * 3 + 1] = Math.sin(f.el); pokes[i * 3 + 2] = Math.cos(f.el) * Math.cos(f.az);
    ages[i] = f.age; waves[i] = f.wave; helds[i] = f.heldAmt;
  }
  const shyT = Math.min(1, peak * 1.5) * 0.6;                         // quiver, slewed
  S.shySm += (shyT - S.shySm) * (shyT > S.shySm ? 1 - Math.exp(-dt / 0.25) : 1 - Math.exp(-dt / 0.6));
  return { act, time: S.time, k: S.kSm, poke: S.poke, dent: Math.max(-0.35, Math.min(0.35, d.x * 0.28)), orbit: [S.yaw, S.pit],
    thick: 480 + 250 * Math.min(1, envMax),                            // the pearl blooms on attack
    pokes, ages, waves, helds,
    rAmp: Math.min(1, 0.25 + envMax * 0.5 + clamp((EFF.detune || 0) / 100, 0, 1) * 0.35) * act,   // a wide swarm strikes harder water
    shy: S.shySm * act };
}
/* B365, corrected: the pearl's NOTE-DRIVEN energy only — the strike and held ripples at their
   amplitude (each front's strike envelope as the shader forms it, exp(−1.9·age)·smoothstep(0, .12,
   age) while age < 3.5, and its held amount over the shader's .001 gate), the quiver, and the ringing
   dent (a note's poke; damped on real dt already, so it settles on its own). Exactly 0 once every
   note-driven term has settled — NOT a measure of the idle motion (drift/turntable), which never
   settles and must not read into this: U.act itself is deliberately excluded (it used to be summed
   here, which is why the merged-B365 bug read as "still moving" — it was reading the settle timer,
   not the picture). */
function specimenEnergy(U) {
  let e = U.shy + Math.abs(U.dent);
  for (let i = 0; i < 4; i++) {
    const a = U.ages[i], s = Math.min(1, a / 0.12), strike = a < 3.5 ? Math.exp(-a * 1.9) * s * s * (3 - 2 * s) : 0;
    e += U.rAmp * (strike + (U.helds[i] > 0.001 ? U.helds[i] : 0));
  }
  return e;
}
function specimenFrame(force) {
  const S = SPECIMEN;
  if (!S.on || S.fail || LAB.page !== 'main') { S.xyLast = null; return false; }
  const cv = specimenCanvas();
  if (!cv.isConnected || !cv.offsetParent) { S.xyLast = null; return false; }   // create-on-show
  if (!S.gl) { specimenInit(); if (!S.gl) return false; }
  specimenFollowXY(S, cv);                                            // every frame, drawn or not
  /* B323 — THE FREEZE. Human, 2026-09-28: "it freezes when audio turns on until I turn it off and on
     again". Measured (trace b323): audio on re-strikes the monitor with its clock at 0 (monStrike
     'audio', B299), while S.clk still held the old clock, so clk − S.clk was negative, dt read 0 and
     the pearl drew nothing until the new clock passed the old one: as long as the page had run
     before, 0.55× that at 60 Hz. The logo froze the same way. A clock that went BACKWARDS is a new
     clock: re-anchor to it, so the next frame's dt is a real one. */
  if (S.clk > MON.clock) S.clk = MON.clock;
  const clk = MON.clock, dt = S.clk < 0 ? 0.033 : Math.min(0.05, Math.max(0, clk - S.clk) / SR);
  const scale = Math.min((typeof devicePixelRatio === 'number' && devicePixelRatio) || 1, 2);
  const pw = Math.max(2, Math.round(cv.clientWidth * scale)), ph = Math.max(2, Math.round(cv.clientHeight * scale));
  const tok = TOK('--scr-tube', '#f2ede2'), resized = pw !== S.w || ph !== S.h, themed = tok !== S.bgTok;
  if (!force && !resized && !themed && dt <= 0) return false;
  /* B365 correction: "at rest, nothing is drawn" no longer holds — the drift and the turntable keep
     moving whether or not a note sounds (the idle motion), so the image never stops changing on its
     own. B365-as-merged's skip here (S.act === 0 && ...) is gone: it was skipping frames the corrected
     idle law now needs to draw. gui2's own throttle (the shed below) is what bounds the idle cost, same
     as it always did pre-B365. */
  const src = specimenSource();
  if (!force && !resized && !themed && (++S.n % (LOAD.shed ? SHED_SPECIMEN : 3))) return false;   // gui2's ~20 Hz; ~5 Hz shedding (J2)
  const t0 = typeof performance === 'object' ? performance.now() : 0;
  S.clk = clk;
  const gl = S.gl;
  if (resized) { S.w = pw; S.h = ph; cv.width = pw; cv.height = ph; gl.viewport(0, 0, pw, ph); }
  const U = specimenStep(S, dt, src, Math.max(0, MON.rSm));
  S.U = U;
  gl.uniform2f(S.u.uRes, pw, ph);
  gl.uniform1f(S.u.uTime, U.time);
  gl.uniform1f(S.u.uK, U.k);
  gl.uniform1i(S.u.uVariant, 1);                                      // pearl
  gl.uniform3f(S.u.uPoke, U.poke[0], U.poke[1], U.poke[2]);
  gl.uniform1f(S.u.uDent, U.dent);
  gl.uniform2f(S.u.uOrbit, U.orbit[0], U.orbit[1]);
  gl.uniform1f(S.u.uThick, U.thick);
  gl.uniform3fv(S.u.uPokeN, U.pokes); gl.uniform1fv(S.u.uStrikeN, U.ages); gl.uniform1fv(S.u.uRWaveN, U.waves); gl.uniform1fv(S.u.uHeldN, U.helds);
  gl.uniform1f(S.u.uRAmp, U.rAmp);
  gl.uniform1f(S.u.uShy, U.shy);
  if (themed) {                                                        // the ground is the SCREEN colour
    S.bgTok = tok;
    const hx = /^#([0-9a-f]{6})$/i.exec(tok.trim());
    if (hx) { const v = parseInt(hx[1], 16); S.bg = [(v >> 16 & 255) / 255, (v >> 8 & 255) / 255, (v & 255) / 255]; }
  }
  gl.uniform3f(S.u.uBg, S.bg[0], S.bg[1], S.bg[2]);
  /* B323: centred on its projected bounds (specimenFit); S.center false is B317's fixed camera (the check's control) */
  const fit = S.center ? specimenFit(S.yaw, S.pit, S.time, U.dent, pw / ph) : { shift: [0, 0], zoom: SPECIMEN_ZOOM, k: 1 };
  S.fit = fit;
  gl.uniform1f(S.u.uZoom, fit.zoom);
  gl.uniform2f(S.u.uShift, fit.shift[0], fit.shift[1]);
  gl.uniform1i(S.u.uMask, S.mask ? 1 : 0);
  gl.drawArrays(gl.TRIANGLES, 0, 3);
  S.draws++;
  if (typeof performance === 'object') S.ms = performance.now() - t0;   // cost only (GPU work is asynchronous)
  return true;
}
/* ======== the lab's handle ======== */
window.MainLabParts = {
  attach(doc, xy) { frameDoc = doc; xyAt = xy; },
  MORPH, MON, AUD, LOGO, SPECIMEN, logoCanvas, logoFrame, specimenCanvas, specimenFrame,
  tick() { MON.clock += SR / 60; logoFrame(); specimenFrame(); }
};
})();
