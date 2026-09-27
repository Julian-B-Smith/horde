/*
 * scalpel-horde-engine.js — B298: the COMPOSED engine. horde's swarm drives the
 * member trajectories; SCALPEL's blades are evaluated on them. HYPERSAW,
 * 2026-09-27, dispatched by the horde lead on ROADMAP B298 (records PR #783,
 * branch lead-records-107). The human: "Why am I seeing onset lock, dissolve,
 * and drift grayed out? And is there a reason inertia doesn't survive? ... we no
 * longer have the detune laws ... Can they be reintroduced?"
 *
 * WHY THIS EXISTS. The SCALPEL interface lab's sound was the SCALPEL oracle
 * alone (reference/scalpel/prototype/razor-core.js, RazorCore). That oracle has
 * no onset lock, dissolve, horde drift, inertia or detune laws, and it plays
 * SCALPEL's coupling law where docs/scalpel/ACCOUNTING.md row 6 says "Horde law
 * wins". So the lab greyed those controls and could mislead about what horde
 * will sound like. This module is the engine the lab can swap to.
 *
 * BOTH REFERENCES ARE CALLED, NEVER EDITED, NEVER COPIED WHOLESALE.
 *   - SCALPEL: ComposedEngine EXTENDS RazorCore. Every blade, spread, rule,
 *     cross-mod, feedback, frame, DC and band-limit path is RazorCore's own
 *     method, inherited unchanged (ACCOUNTING §1.5, "SCALPEL's law wins only
 *     where horde has nothing").
 *   - horde: a SwarmSynth instance (reference/swarmsaw.html, its DSP section
 *     evaluated from the HTML at load, the extract_core.mjs route) owns one
 *     swarm per RazorCore voice. Its own noteOn() and controlTick() run the
 *     coupling law, bipolar K with seats, onset lock and dissolve, seeded drift,
 *     inertia and every detune law (swarmsaw.html:352-366, :380-547).
 *   - COPIED, the minimum, cited: SwarmSynth's per-sample phase advance and
 *     frequency-glide leg (swarmsaw.html:661-672, eight lines), because
 *     renderSeg() also renders the saw and gates the swarm on ITS OWN envelope,
 *     and here the voice's envelope is RazorCore's; and the shell's inertia
 *     taper (src/hypersaw_clap.cpp:7307-7313, one line), which lives in the
 *     plugin shell, not in either reference.
 *
 * THE COMPOSITION, ROW BY ROW (docs/scalpel/ACCOUNTING.md):
 *   row 1  N/n       horde id; the member count is RazorCore's d.N (1..9: the
 *                    oracle hardcodes 9 members, §1.8) and SwarmSynth's p.n.
 *   row 4  detune    MERGES, horde law: SCALPEL's `detune` d cents IS the horde
 *                    knob d/100 ("Identical at dist 0, law 0", §1.5). Read
 *                    unsmoothed (row 4 smoothing U).
 *   row 5  law       SURVIVES as `h.law` (the key collides with SCALPEL's spread
 *                    law, §1.8.4): 0 cents, 1 Hz, 2 ERB, 4 harmonic (harmReach),
 *                    5 stretch (stretchB); spread and anchor thread every law.
 *   row 6  K         MERGES, HORDE LAW WINS: 4K|K|·σ Hz pull with seats at K<0
 *                    (§1.6.1). RazorCore's keff/couple law is not in the sound;
 *                    its couple() still runs, for the frame-invariant r and
 *                    lead only, and its m.inc is overwritten (couple(), below).
 *   rows 7/8 onset, dissolve  SURVIVE (SwarmSynth's Kenv, :355, :387).
 *   rows 9/10/56/60 drift     SURVIVE: driftDepth (cents), `h.driftRate` (key
 *                    collision with SCALPEL's spread-drift rate), driftMode,
 *                    motionCenter; the per-swarm seeded stream (:361, :401).
 *   row 16 retrig    MERGES with phaseMode: random = retrig off (horde's seeded
 *                    draw), aligned = retrig on (horde phase 0). *settled* is a
 *                    NEW row whose sequencing with onset is open (Q B4), so it
 *                    is NOT voiced here: phaseMode 2 plays as retrig on, the
 *                    horde default (§1.7 "settle off (retrig governs)").
 *   row 29 absK/cScale  NOT VOICED: SwarmSynth has no absK (C++ only,
 *                    swarm_core.h:1886-1889). cScale is ignored.
 *   G6/G7 inertia, inertiaCurve  SURVIVE: `inertia` is the KNOB and the core
 *                    receives knob^curve (sqrt exactly at 0.5), the shell's
 *                    taper (hypersaw_clap.cpp:7307-7313; default curve 2.5,
 *                    :2426). SwarmSynth itself has no curve.
 *   G9 freqGlide     SURVIVES (SwarmSynth's per-member glide).
 *   G3 glide         MERGES: RazorCore's per-sample lag on v.freq IS horde's lag
 *                    law (τ = T/3000); the swarm reads it as f0 on its 16-sample
 *                    tick, which is where horde steps it.
 *   §1.6.6 phase origin  horde's saw has its jump at 0, SCALPEL's at ½, so
 *                    φ_SCALPEL = frac(φ_horde + ½). The swarm keeps horde's phase
 *                    state and start; every blade formula reads φ_SCALPEL.
 * NOT COMPOSED THIS ROUND (stated, not hidden): the output stage and voice
 * (rows 12, 13, 15, 17-20, 66 and D11: vol/normExp, width, the tanh, ADSR, the
 * pan image) stay RazorCore's. SwarmSynth's per-member amplitude terms (hiTame,
 * tone tilt, :458-470) are inert at their defaults and are not composed.
 *
 * TWO PHASE INTEGRATORS, ON PURPOSE. The swarm's φ_horde advances with
 * SwarmSynth's own arithmetic, so its trajectories are SwarmSynth's bit for bit
 * (oracle O1). The blade phase φ_SCALPEL (m.phi) is started at
 * frac(φ_horde + ½) and advanced by RazorCore's own stepM() with the SAME
 * increments, so the blade path is RazorCore's bit for bit (oracle O2). The two
 * integrate identical increments and differ only by rounding; the check
 * measures that bound (tools/labharness/composed_engine_check.mjs).
 *
 * SWARM SOURCES. `src` selects who drives the members: 'horde' (the product) or
 * 'razor' (RazorCore's own swarm half: its couple(), settle() and increments).
 * The razor source is the O2 fixture and the must-fail control for O1; every
 * source hands the engine HORDE-FRAME start phases and the engine maps them
 * through `origin` (½; 0 is the broken mapping the O2 control plants).
 *
 * THE API IS RazorCore's (the lab swaps one line: CORE = ComposedEngine).
 * Instances: set/msg/noteOn/noteOff/render/setOS, s/t/d, voices[].m[] (phi in
 * the SCALPEL frame, inc in Hz), post({t:'viz', mem}) with a `horde` field
 * added. Statics: inherited (voice, wave, out, fmStep, g2, fillG2, PANS); mr/mn
 * forward to RazorCore's, because RazorCore.voice() reads RazorCore.mr by name.
 * toString() returns a SELF-CONTAINED bundle that defines RazorCore as the
 * composed class, so the lab's AudioWorklet builder (CORE.toString() + ... +
 * `new RazorCore(sampleRate)`) runs it unchanged.
 * New keys live in `d` (horde rows are unsmoothed), so the lab's key filter
 * (keys of t ∪ d) passes them: dist, seed, h.law, harmReach, stretchB, spread,
 * anchor, onset, dissolve, driftDepth, h.driftRate, driftMode, motionCenter,
 * inertia, inertiaCurve, freqGlide, keepPhase, pivotMode.
 *
 * DETERMINISM. The swarm draws only SwarmSynth's seeded mulberry32 streams
 * (rngG/rngS); RazorCore's Math.random must be seeded by the host (the lab
 * installs mulberry32 before every instance). No clock is read here.
 */

/* The factory takes RazorCore (the class) and the TEXT of swarmsaw.html's DSP
   section, so the same function builds the engine in Node, on the page and
   inside an AudioWorklet (toString() re-emits both). */
function makeComposedEngine(RazorCore, swarmSrc) {
  'use strict';
  const SwarmSynth = new Function('"use strict";\n' + swarmSrc + '\nreturn SwarmSynth;')();
  const TAU = 6.283185307179586;
  const frac = x => x - Math.floor(x);
  const HORDE_D = {
    dist: 0,             // row 2; 0 (even) is row 4's translation of a SCALPEL preset. horde's own default is 1 (JP)
    seed: 1234,          // row 3
    'h.law': 0,          // row 5 (key collides with SCALPEL's spread `law`)
    harmReach: 1, stretchB: 0, spread: 1, anchor: 0,   // rows 61-64, the laws' sub-parameters
    onset: 0, dissolve: 0.63,                          // rows 7, 8 (dissolve in seconds)
    driftDepth: 0, 'h.driftRate': 0.4, driftMode: 0, motionCenter: 0,   // rows 9, 10, 56, 60
    inertia: 0, inertiaCurve: 2.5,                     // G6 (the knob), G7
    freqGlide: 0, keepPhase: 0, pivotMode: 0,          // G9, row 57, row 65
  };
  // p keys whose change makes SwarmSynth.setParam() rebuild x[] (swarmsaw.html:247)
  const REBUILD = ['n', 'dist', 'seed', 'law'];

  class ComposedEngine extends RazorCore {
    static get mr() { return RazorCore.mr; }
    static set mr(x) { RazorCore.mr = x; }
    static get mn() { return RazorCore.mn; }
    static set mn(x) { RazorCore.mn = x; }
    static toString() {
      return RazorCore.toString() + '\nRazorCore.PANS = ' + JSON.stringify(RazorCore.PANS) + ';\n' +
        makeComposedEngine.toString() + '\nRazorCore = makeComposedEngine(RazorCore, ' + JSON.stringify(swarmSrc) + ');\n';
    }

    constructor(sr) {
      super(sr);
      Object.assign(this.d, HORDE_D);
      this.src = 'horde';
      this.origin = 0.5;                 // §1.6.6
      this.sw = new SwarmSynth(sr);
      this.nBase = 0;                    // engine sample count: SwarmSynth's global tick is (count & 15)
      this.gCoefS = 0;
      this.voices.forEach((v, si) => {
        v.si = si; v.sn = 0;
        v.m.forEach((m, i) => { m.v = v; m.i = i; m.j = 0; m.dph = 0; });
      });
      /* the lab assigns c.post = fn; the viz message gains the swarm's state */
      let user = null;
      const wrap = msg => {
        if (msg && msg.t === 'viz' && msg.mem && this.src === 'horde') msg.horde = this.hordeViz();
        user(msg);
      };
      Object.defineProperty(this, 'post', { configurable: true, enumerable: true,
        get() { return user ? wrap : null; }, set(f) { user = f || null; } });
    }

    setOS(n) {
      super.setOS(n);
      if (this.voices) for (const v of this.voices) for (const m of v.m) m.j = 0;
    }

    /* horde params into SwarmSynth.p; x[] is rebuilt only when a rebuild key moved */
    syncSwarm() {
      const p = this.sw.p, d = this.d, t = this.t;
      const want = { n: d.N, dist: d.dist, seed: d.seed, law: d['h.law'] };
      let re = false;
      for (const k of REBUILD) if (p[k] !== want[k]) { p[k] = want[k]; re = true; }
      if (re) this.sw.rebuild();
      p.detune = t.detune / 100;                       // row 4: d cents ≡ knob d/100
      p.K = t.K;                                       // row 6, horde's own smoother (KsmS) applies
      p.onset = d.onset; p.dissolve = d.dissolve;
      p.driftDepth = d.driftDepth; p.driftRate = d['h.driftRate']; p.driftMode = d.driftMode;
      p.motionCenter = d.motionCenter;
      const c = d.inertiaCurve, k = d.inertia;         // hypersaw_clap.cpp:7307-7313
      p.inertia = c === 0.5 ? Math.sqrt(k) : Math.pow(k, c);
      p.freqGlide = d.freqGlide; p.keepPhase = d.keepPhase; p.pivotMode = d.pivotMode;
      p.harmReach = d.harmReach; p.stretchB = d.stretchB; p.spread = d.spread; p.anchor = d.anchor;
      p.retrig = d.phaseMode === 0 ? 0 : 1;            // row 16; settled (2) is not voiced
      this.gCoefS = p.freqGlide > 0 ? 1 - Math.exp(-1 / (p.freqGlide * 0.25 * this.sr)) : 0;   // swarmsaw.html:655
    }

    startVoice(v, note, freq, vel, fresh, retrig) {
      super.startVoice(v, note, freq, vel, fresh, retrig);
      /* A non-fresh retrigger keeps the swarm running, as RazorCore keeps its phases
         (SwarmSynth would start a new swarm: an open question for the lead). */
      if (!fresh) return;
      if (this.src === 'horde') {
        this.syncSwarm();
        const S = this.sw.swarms[v.si];
        this.sw.alloc = () => S;                       // bind swarm ↔ voice for this one call
        this.sw.noteOn(note, freq * Math.pow(2, this.s.bend / 12));
        delete this.sw.alloc;
        v.sn = this.nBase;
        for (const m of v.m) { m.j = 0; m.phi = frac(S.phase[m.i] + this.origin); }
      } else {
        /* the oracle's start, handed over in the horde frame and mapped back:
           exact for its aligned 0 and seeded-draw starts (multiples of 2^-32) */
        for (const m of v.m) { m.j = 0; m.phi = frac(frac(m.phi - 0.5) + this.origin); }
      }
    }

    settle(v) { if (this.src === 'razor') super.settle(v); }

    /* RazorCore's couple() still supplies what is frame-invariant (v.r, each
       member's lead on the mean field); the member frequency is the swarm's. */
    couple(v, s, d) {
      super.couple(v, s, d);
      if (this.src !== 'horde') return;
      const S = this.sw.swarms[v.si], N = d.N;
      if (!S.vfInit) {                                 // no swarm tick yet this note: the pitch
        const f = v.freq * Math.pow(2, s.bend / 12);
        for (let i = 0; i < N; i++) v.m[i].inc = f;
        return;
      }
      v.fc = S.fBase;                                  // v.gr = v.freq/v.fc then carries glide between ticks
      const glideOn = this.sw.p.freqGlide > 0;
      for (let i = 0; i < N; i++) v.m[i].inc = Math.max(0, glideOn ? S.fRun[i] : S.eff[i]);
    }

    tickSwarm(v, S) {
      this.syncSwarm();
      S.fBase = v.freq;
      S.f0 = v.freq * Math.pow(2, this.s.bend / 12);
      this.sw.controlTick(S);
    }

    /* RazorCore calls stepM once per member per oversampled step, members in
       order, so member 0's first step of each sample is where the swarm's
       16-sample tick belongs (before any member advances). */
    stepM(m, dphi, c, k, s) {
      if (this.src !== 'horde') return super.stepM(m, dphi, c, k, s);
      if (m.j === 0) {
        const v = m.v, S = this.sw.swarms[v.si], i = m.i;
        if (i === 0) { if ((v.sn & 15) === 0) this.tickSwarm(v, S); v.sn++; }
        // swarmsaw.html:665-672 (renderSeg), per member
        const glideOn = this.sw.p.freqGlide > 0;
        if (glideOn) S.fRun[i] += this.gCoefS * (S.eff[i] - S.fRun[i]);
        const f = glideOn ? S.fRun[i] : S.eff[i];
        const dph = Math.max(0, f) / this.sr;
        let ph = S.phase[i] + dph;
        ph -= Math.floor(ph);
        S.phase[i] = ph;
        m.dph = Math.max(0, f) / (this.sr * this.os);
      }
      if (++m.j >= this.os) m.j = 0;
      return super.stepM(m, m.dph, c, k, s);
    }

    render(L, R) {
      super.render(L, R);
      this.nBase += L.length;
      if (this.src !== 'horde') return;
      /* keep-phase snapshot of the newest sounding swarm (swarmsaw.html:714) */
      let lv = null;
      for (const v of this.voices) if (v.active && (!lv || v.age > lv.age)) lv = v;
      if (lv) this.sw.lastPhase.set(this.sw.swarms[lv.si].phase);
    }

    hordeViz() {
      let lv = null;
      for (const v of this.voices) if (v.active && (!lv || v.age > lv.age)) lv = v;
      if (!lv) return null;
      const S = this.sw.swarms[lv.si], N = this.d.N;
      return { R: S.R, psi: frac(S.psi / TAU), Kenv: S.Kenv, KsmS: S.KsmS, KsmP: S.KsmP, sigma: S.sigma,
        eff: Array.from(S.eff.subarray(0, N)), phase: Array.from(S.phase.subarray(0, N)) };
    }
  }
  return ComposedEngine;
}

/* swarmsaw.html's DSP section, sliced by its banners: the rule of
   tools/golden/extract_core.mjs (BANNERS.reference), repeated here because the
   page and the worklet cannot import a Node module. */
function swarmSourceFromHtml(html) {
  const a = html.search(/\/\* =+ DSP:/), b = html.search(/\/\* =+ Audio graph/);
  if (a < 0 || b <= a) throw new Error('swarmsaw.html: DSP:/Audio graph banners not found');
  const src = html.slice(a, b);
  if (src.indexOf('class SwarmSynth') < 0) throw new Error('swarmsaw.html: SwarmSynth not in the DSP section');
  return src;
}

if (typeof module !== 'undefined') module.exports = { makeComposedEngine, swarmSourceFromHtml };
