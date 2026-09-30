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
 *   voice law (B310)  HORDE LAW WINS: poly note-on allocates by ADR-083's three
 *                    tiers (free, quietest releasing tail, oldest held), never
 *                    RazorCore's same-note reuse, so a repeated note's first
 *                    release keeps ringing. See noteOn() below.
 *   B315     BIPOLAR ROTATE SPREAD, A DIVERGENCE FROM THE ORACLE (recorded as ADR-184 A2 (1)).
 *                    The human (2026-09-27) asked for every spread to be bipolar.
 *                    RazorCore's spread law already reads a negative amount as
 *                    the mirror (every member offset is amount·pn(j), and pn is
 *                    centred on 0: razor-core.js:106-115, :127-135), EXCEPT
 *                    rotation: its render gates member rotation on
 *                    `t.rotSpread > 0.004` (and rotSpread2, :786, :791), so a
 *                    negative Rotate spread homes every member to 0 instead of
 *                    turning them the other way. Here the gate reads |x|: set()
 *                    hands the oracle the magnitude and keeps the sign, and
 *                    spread() negates rotOff/rotOff2 after the oracle's own law
 *                    has run. The result is exactly the oracle's formula,
 *                    rotSpread·2·pn(2), with the sign kept; at x >= 0 nothing
 *                    here runs, so every non-negative patch is bit-identical.
 *                    The sign switches at once (the magnitude is still the
 *                    oracle's smoothed one): members change direction, their
 *                    positions do not jump (rot is integrated). See set() and
 *                    spread() below.
 *   ADR-184 A2 (2)   EXACT CUT-SPREAD MIRROR UNDER QUANTIZE (B323): the snapped
 *                    spread rounds half away from zero (roundAway, spread()).
 *   B323     VOICE CAP AND CULL (graceful degradation under load): a host-set
 *                    cap on sounding voices; over it the quietest releasing tails
 *                    fade out (a linear 8 ms ramp, never a cut), by ADR-083's
 *                    tiers, never a held note. An input, not a clock read. See
 *                    cull() and render() below.
 *   B375     THE POLICY AT A FULL CAP (the human, 2026-09-29: "Blocking new voices
 *                    seems like a reasonable policy, though maybe we could include
 *                    a toggle to test"). When a note-on finds the cap full and no
 *                    releasing tail to take, `capPolicy` (an input, like the cap)
 *                    decides: 0 REFUSE, the DEFAULT: the note is dropped and every
 *                    held voice is left exactly as it was; 1 STEAL (the lab's test
 *                    toggle): the oldest held voice is released and faded out over
 *                    B323's 8 ms ramp, and the note takes a free slot; 2 REPLACE:
 *                    B323's own law as built (the oldest held voice is re-struck at
 *                    once), kept reachable, used by nothing. With no cap (0, every
 *                    offline render and every parity gate) none of this runs. Not a
 *                    divergence from the oracle: RazorCore has no cap, and the
 *                    composed engine's voice law is horde's (B310). See noteOn().
 *   B335     GRAVITY AND THE ENSEMBLE TIMING CORRECTION (human 2026-09-28: "Have we
 *                    ported gravity over from the original engine? Or the ensemble
 *                    voice lag correction behavior?"). Both SURVIVE in the accounting
 *                    (row 27/28 grav, basin; rows 70-74 onset & scatter) and neither
 *                    is in SwarmSynth, so both are composed here:
 *            GRAVITY (ADR-008, ADR-086 and its Amendment 1). REFERENCE: DynSynth,
 *                    reference/swarmdynamics.html. Its gravityStep (:291-321) and ratio
 *                    set (:214-215) are COPIED below, cited, because calling DynSynth
 *                    would change this factory's signature (every caller passes two
 *                    arguments); composed_engine_check's GRAV rows prove the copy against
 *                    DynSynth itself, exactly. Each held voice's swarm pitch f0cur is
 *                    pulled toward the nearest folded just ratio of every other held
 *                    voice inside the basin, on a FIXED TIME grid (256/44100 s, 279
 *                    samples at 48 kHz), the steps falling BETWEEN segments of audio
 *                    (render(), below). f0cur rides the played pitch multiplicatively, as
 *                    swarm_core.h keeps it (`s.f0cur *= ratio` on every glide step), and
 *                    a note-on resets it to ET (ADR-008: "the settle is per-chord").
 *                    At grav < 0.005 (DynSynth's own threshold) no step runs, no render is
 *                    segmented and the swarm reads the played pitch unchanged: bit-identical.
 *            ONSET SCATTER, TIMING CORRECTION, ATTACK SCATTER, PER-PARTIAL ENV, RELEASE
 *                    SCATTER (ADR-077, ADR-078, B149). THERE IS NO JS REFERENCE: these
 *                    exist only in the legacy C++, src/swarm_core.h, and THE C++ IS THE
 *                    REFERENCE HERE. The draws (initVoice :649-684, gaussT :2125, ensembleSeed
 *                    :1388, the persistent tOff and its reseed on a seed change, rebuild
 *                    :1483-1489) and the per-sample entry (renderSeg :1052, :1118-1143,
 *                    :1198-1204) are
 *                    TRANSCRIBED expression for expression. Checked C++ <-> JS, not against
 *                    a lab: tools/onset_ref_check.cpp renders swarm_core.h itself into
 *                    tools/labharness/onset_ref_cpp.json and composed_engine_check's ONS
 *                    rows compare this transcription with it (same seeds).
 *                    Two deliberate differences, both structural, both measured there:
 *                    (1) TIME IS COUNTED PER OUTPUT SAMPLE. swarm_core.h counts a member's
 *                    wait and steps its entry ramp once per SUB-sample (inside the ADR-075
 *                    oversampling loop), so at its 2x setting a 10 ms scatter waits 5 ms and
 *                    the ramp runs twice as fast; here the law is in seconds at every os
 *                    (ADR-009). At the C++'s 1x the two are the same samples.
 *                    (2) PER-PARTIAL ENV uses THIS engine's envelope law. ADR-078 defines
 *                    the per-voice ADSR as "the same arithmetic as the shared envelope, run
 *                    once per voice", and the shared envelope here is RazorCore's (linear
 *                    attack, exp(-4/T) decay and release: rows 12-20 and D11 are not
 *                    composed). So each member runs RazorCore's ADSR with its OWN attack
 *                    and release times, drawn by the C++ law (T·max(0.15, 1 + 0.6·scatter·g),
 *                    floored at 2 ms), and with nothing scattered every member IS the voice
 *                    envelope, bit for bit (ADR-078's "uniform when nothing is scattered").
 *                    The DRAWS and the TIMING are the C++'s exactly; the envelope SHAPE is
 *                    this engine's, and the check compares the laws (each member's time
 *                    scales by the same drawn factor in both engines), not the samples.
 *                    See armMembers() and memberStep() below.
 *   ADR-189  ANTI-ALIASING DIVERGENCES FROM THE ORACLE (B355; ratified 2026-09-29: "I ratify your
 *                    recommended aliasing fixes, and we should skip the ones you recommended
 *                    against"). B346 (tools/patchspace/alias_sources.mjs) found where the
 *                    engine's aliasing comes from; three of its cures are built here, each behind
 *                    its OWN flag in `d`, EVERY FLAG DEFAULT 0. With all three at 0 no line below
 *                    changes a sample: the engine is bit-identical to the one before B355
 *                    (composed_engine_check AA0 fingerprints it; docs/port/divergences.json is the
 *                    ADR-187 ledger and tools/labharness/divergence_ledger_check.mjs proves it).
 *            D1 `aaCarrier`  first-order ADAA on a blade CARRIER, REPLACING the scanner's
 *                    carrier-wrap PolyBLEPs (never stacked on them), NARROWED by ADR-189 A1 to
 *                    where it beats them: carrier shape not sine, AND (an FM mode, 1 or 2, OR
 *                    Band-limit off), AND not an S&H modulator (d1Takes()). Everywhere else the
 *                    carrier keeps its BLEPs. See voiceAA(), d1Takes() and scan().
 *            D2 `aaXin`      the PolyBLEP scanner tracks the xin phase input (feedback and
 *                    cross-member modulation push the carrier phase; the oracle's scan() does not
 *                    add it, so it BLEPs the wrong edges, razor-core.js :633). See scan().
 *            D3 `aaLoop`     the feedback / cross-mod loop keeps the ORACLE'S TAP (the PolyBLEP
 *                    output) and runs it through a one-pole loop filter at LOOP_FC (5 kHz), which
 *                    removes broad#828's rate-locked limit cycle (ADR-189 A1). See loopIn() and
 *                    bladeStep(). The oracle's own render loop is not touched: the xin it
 *                    computed is OVERWRITTEN in stepM before the blade step reads it.
 *            The oracle is called, never edited (reference/** is protected). D1 is the one place
 *            a subclass cannot reach: the carrier is computed inside the STATIC voice(), which
 *            out()/outSerial() call by name. So while a D1 render runs, RazorCore.voice is
 *            swapped for voiceAA and restored in a finally, the oracle's own idiom (it swaps
 *            RazorCore.mr/mn around every blade-2 evaluation, razor-core.js :267-276).
 *            JS-ONLY MECHANISM: the static voice() swap exists because a subclass cannot reach
 *            a static called by name. The C++ implements the carrier AA choice inline in
 *            voice() as per-instance state; the swap is not part of the divergence.
 *   B382     DIVERGENCES FROM SwarmSynth (the human, 2026-09-30, B379: the lab matches the C++ swarm
 *                    core, src/swarm_core.h, where SwarmSynth disagrees with it). Each is a TEXT PATCH of
 *                    SwarmSynth's controlTick()/noteOn(), registered in SWARM_PATCHES at the top of the
 *                    factory next to its own reasons, behind its own flag in `d` whose 0 is SwarmSynth's
 *                    expression, DEFAULT ON (ADR-187 A1: a default that changes what the listener hears
 *                    is one divergence per PR), ledgered in docs/port/divergences.json.
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
 * inertia, inertiaCurve, freqGlide, keepPhase, pivotMode, and (B335) grav, basin,
 * onsetScatter, onsetAlpha, attackScatter, voiceEnv, relScatter, and (B355, ADR-189)
 * aaCarrier, aaXin, aaLoop.
 *
 * DETERMINISM. The swarm draws only SwarmSynth's seeded mulberry32 streams
 * (rngG/rngS) and (B335) the ensemble stream, mulberry32 from
 * swarm_core.h's ensembleSeed(seed); RazorCore's Math.random must be seeded by
 * the host (the lab installs mulberry32 before every instance). No clock is read here.
 */

/* The factory takes RazorCore (the class) and the TEXT of swarmsaw.html's DSP
   section, so the same function builds the engine in Node, on the page and
   inside an AudioWorklet (toString() re-emits both). */
function makeComposedEngine(RazorCore, swarmSrc) {
  'use strict';
  /* B382, DIVERGENCES FROM SwarmSynth (ADR-187 §5, docs/port/divergences.json). SwarmSynth is called,
     never edited (reference/** is protected), and its controlTick()/noteOn() cannot be reached from
     outside: a change INSIDE them is made to the TEXT this factory evaluates. Each divergence
     registers itself below (SWARM_PATCHES.push, next to the code it concerns) as exact old -> new
     blocks of that text, the lift ledger's `patch` op (h2/cores/swarm/lift-ledger.json):
       id     its divergences.json id;
       flag   its engine key in `d`, copied into SwarmSynth.p by syncSwarm() with the rest of `keys`.
              The patched text reads it on every call, so 0 runs SwarmSynth's own expression, the
              very characters of the reference, and the lab can compare the two live;
       keys   every `d` key the patched text reads from p (the flag first);
       edits  [old, new] pairs. `old` must occur EXACTLY ONCE in the text, or the factory throws:
              a changed reference must be re-read, never patched by a near miss.
     SwarmSynth itself is built from the patched text just above the class (patchSwarm), after every
     registration has run. With no registration the text is swarmsaw.html's, character for character. */
  const SWARM_PATCHES = [];
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
    grav: 0, basin: 35,                                // B335 rows 27, 28 (basin in cents)
    onsetScatter: 0, onsetAlpha: 0.25, attackScatter: 0, voiceEnv: 0, relScatter: 0,   // B335 rows 70-74 (onsetScatter in ms)
    aaCarrier: 0, aaXin: 0, aaLoop: 0,                 // ADR-189 D1, D2, D3 (B355): OFF is the oracle's sound
    ksmPerRate: 1,                                     // B382 M1: ON, the C++'s per-rate coupling smoother
  };
  // p keys whose change makes SwarmSynth.setParam() rebuild x[] (swarmsaw.html:247)
  const REBUILD = ['n', 'dist', 'seed', 'law'];
  /* B323's cull fade, in SECONDS (ADR-009): a LINEAR gain ramp from 1 to 0 over CULL_FADE,
     applied every sample (1/(CULL_FADE·sr) a sample, 1/384 at 48 kHz), on top of RazorCore's own
     release. The lead's addendum (2026-09-28, after the human reported "more noise and clicks"):
     a cull must never produce a discontinuity. 8 ms sits in the 5-10 ms asked for: long enough
     that the metric check (composed_engine_check CULL) reads no new click on a pure sine, short
     enough to shed the load within three or four 128-sample blocks. The fading voice still costs
     its render until it is freed, and the load it measures counts that. */
  const CULL_FADE = 0.008;
  /* ADR-189 D3's loop filter, in Hz (ADR-009: a time constant, converted per internal sample in setOS),
     on the ORACLE'S OWN TAP (ADR-189 A1). MEASURED, not tuned by ear (B355 trace; the line-over-floor
     probe: a line at R/5, 2R/5 or R/3 of the internal rate against the same-width bands 0.02 R and
     0.04 R either side). broad#828's loop, as the oracle runs it, holds a line 13-19 dB over its floor
     at os 1, 2, 4 and 8 (a rate-locked limit cycle, B346); broad#540 another, 10-20 dB. A one-pole on the
     fed-back sample removes it: the worst line over os 1/2/4/8 and A1/E5 reads, on broad#828, 3 kHz
     +2.3, 4 kHz +1.3, 5 kHz +2.2, 6 kHz +3.4, 8 kHz +7.1; on broad#540, 4 kHz +1.6, 5 kHz +3.0. 5 kHz is
     the highest measured cutoff with every line within the +-3 dB scatter of a clean reference, so it
     changes least what the feedback sounds like. Rejected, measured: tapping this sample instead of
     the PolyBLEP output (it moves the cycle to R/3); halving or quartering the loop gain (the line
     stays); os (the line sits at the same fraction of the rate at every os). WHY A FILTER WORKS where
     a gain bound does not: the cycle is not a small-signal instability (broad#828's loop gain is ~0.18
     a tap) but CHATTER at a carrier wrap. The saw drops, the fed-back drop pulls the carrier's phase
     back across the wrap, it rises again, and a loop of a sample or two resolves that at the sample
     rate however small the gain. Through a low-pass the pull-back is slewed in SECONDS, so the carrier
     passes the wrap before the pull arrives when xin's jump x 2 pi fc stays under the carrier's own
     rate. A slower carrier under stronger feedback can still chatter, attenuated. */
  const LOOP_FC = 5000;
  /* B382 M1 `ksmPerRate` (ruled 2026-09-30, B379; ADR-009, B150). SwarmSynth smooths the coupling
     targets with a literal 0.08 PER 16-SAMPLE TICK (swarmsaw.html controlTick, `s.KsmS += (syncT -
     s.KsmS) * 0.08` and the same for KsmP), so its time constant is 4.35 ms at 44.1 kHz and 4.00 ms
     at 48 kHz: the ADR-009 class B150 removed from the C++. This is B150's law, src/swarm_core.h
     :389-391: the coefficient from the time constant in SECONDS, kKsmTauSeconds (:152, the value
     that gives the reference's 0.08 at 44.1 kHz), with 44.1 kHz special-cased to the literal 0.08,
     because the round trip is three ULP short of it (swarm_core.h :146-150) and the 44.1 kHz swarm
     must not move by a bit. Resolved once, at construction, as the C++ does. 48 kHz: 0.0737460642...
     Default ON (the human: the lab matches the C++); 0 is SwarmSynth's 0.08 at every rate. */
  SWARM_PATCHES.push({ id: 'M1', flag: 'ksmPerRate', keys: ['ksmPerRate'], edits: [
    ['    this.sr = sr;\n',
     '    this.sr = sr;\n    this.ksmC = sr === 44100 ? 0.08 : 1 - Math.exp(-(TICK / sr) / 0.004351220802760264);   // B382 M1\n'],
    ['    s.KsmS += (syncT - s.KsmS) * 0.08;\n    s.KsmP += (splayT - s.KsmP) * 0.08;\n',
     '    s.KsmS += (syncT - s.KsmS) * (p.ksmPerRate ? this.ksmC : 0.08);   // B382 M1\n' +
     '    s.KsmP += (splayT - s.KsmP) * (p.ksmPerRate ? this.ksmC : 0.08);\n'],
  ] });
  /* ADR-184 A2 (2): round half AWAY from zero, so −x rounds to exactly −(round x). Equal to
     Math.round everywhere except the negative halves (Math.round(−2.5) is −2). */
  const roundAway = x => (x < 0 ? -Math.round(-x) : Math.round(x));
  /* B335 gravity's ratio set, reference/swarmdynamics.html:214 (the C++ kRatios is the same 13). */
  const RATIOS = [1, 16 / 15, 9 / 8, 6 / 5, 5 / 4, 4 / 3, 7 / 5, 3 / 2, 8 / 5, 5 / 3, 16 / 9, 15 / 8, 2];
  /* B335 the ensemble stream's starting state, swarm_core.h ensembleSeed() (:1388):
     (int64)toInt32(seed)·2654435761 + 0x9E3779B8, truncated to uint32. Math.imul is the low 32
     bits of the product, and the sum stays far inside 2^53, so `>>> 0` is the C++ truncation. */
  const ensembleSeed = seed => (Math.imul(seed | 0, 2654435761 | 0) + 0x9E3779B8) >>> 0;

  /* ADR-189 D1 (B355): FIRST-ORDER ADAA OF A BLADE CARRIER. The oracle's voice (razor-core.js :173-238)
     samples the carrier, hot = wave(p.hot, frac(cp)), and the scanner BLEPs its wraps. ADAA-1
     replaces that sample with the carrier's MEAN over the phase it swept since the previous internal
     sample, (F(cp) - F(cp0)) / (cp - cp0), F the oracle's own periodic antiderivative (RazorCore.F,
     :41-49: every carrier shape but sine->saw has one, which is why shape 6 keeps the plain sample).
     The mean is a box filter over the step, so every discontinuity and corner of the carrier comes
     out band-limited to first order whatever pushed the phase (the cut rate, FM, xin, collision).
     Everything else is the oracle's: this runs voice() first, and only for a carrier on a REAL member
     state (ns.aaReal; the BLEP height probe hAt and the DC estimators run voice() on scratch states,
     which stay plain, as in B346's harness) does it recompute cp with voice()'s own expressions
     (:177-191, the same operations in the same order, so the plain hot here equals voice()'s bit for
     bit) and return voice()'s own blend, xin0 + g*depth*(hot - xin0) (:237), with hot replaced.
     - NEAR-ZERO STEP: |cp - cp0| < 1e-7 cycles (a reflect turnaround, a near-frozen carrier) takes
       the carrier at the midpoint of the step, the limit of the quotient; no division by ~0. The
       quotient's own rounding: F is O(1) with unit slope and cp is at most ~1.5e3 cycles, so F's
       error is ~1e-13 and the quotient's ~1e-6 at the threshold, far under any signal.
     - NO PREVIOUS PHASE: at blade entry (ns.inside was false), right after the flag turns on
       (aOk false) and for shapes without F (sine->saw) the plain sample is returned, as B346 did.
     - HALF A SAMPLE OF DELAY, NOT COMPENSATED. The mean over [cp0, cp] is centred half a step back,
       so the carrier CONTENT inside the blade lags by half an internal sample (5.2 us at os 2,
       10.4 us at os 1). Nothing else is delayed: the base wave, the blade gate and window, and the
       note itself are exactly where they were, so this is a fixed offset of the carrier's phase
       inside each blade of kk*dphi/2 cycles (half the carrier's per-sample advance), not a latency.
       It is left uncompensated because a centred mean needs the NEXT phase (a one-sample look-ahead,
       i.e. a real latency on the whole output) or an extrapolation that FM and xin break; and
       because B346 measured this very form (17 of 72 aliasing patch-notes helped, 0 worse). Its
       audible size is measured in the B355 report (the in-band change it makes on the 72). */
  const VOICE0 = RazorCore.voice;
  /* ADR-189 A1, THE NARROWED SCOPE (the human, after the critic: "narrow D1 as recommended"). D1 takes
     a blade's carrier only where ADAA beats the PolyBLEP it replaces:
       the carrier shape is NOT sine (0), and not sine->saw (6, which has no F), AND
       (the blade is an FM mode, 1 or 2, whose modulated phase the scanner's linear interpolation
        cannot follow, OR Band-limit is off, so there is no BLEP at all), AND
       the modulator is not S&H noise (mshape 7): its steps are phase JUMPS, which a mean over the
        step smears (the critic's 3 bench regressions were all S&H).
     A sync or ring carrier with Band-limit on keeps its wrap BLEPs: there the 2-point PolyBLEP (a
     triangle kernel) band-limits better than first-order ADAA (a box), measured -30.3 against -22.8 dB
     on a periodic sync saw (B355 trace). The mshape rule applies to FM blades only: a sync or ring
     blade has no modulator. Discrete per-blade parameters only, so no automation can chatter across
     a threshold. `p` is the blade's parameter view (s for blade 1, bx.g for blade 2: its mode, hot and
     mshape are blade 2's own, fillG2); `aa` is Band-limit, which the view does not carry. */
  const d1Takes = (p, aa) => {
    const md = p.mode, fm = md === 1 || md === 2;
    if (!(fm || md === 0 || md === 5) || p.hot === 0 || p.hot > 4) return false;
    if (fm && p.mshape === 7) return false;
    return fm || !aa;
  };
  let aaBand = 1;                                    // Band-limit (d.aa) for the render in progress (renderBlock)
  function voiceAA(p, phi, c, k, modX, ns, inp) {
    if (!ns.aaReal) return VOICE0(p, phi, c, k, modX, ns, inp);
    const was = ns.inside, y = VOICE0(p, phi, c, k, modX, ns, inp), md = p.mode;
    if (!ns.inside || !d1Takes(p, aaBand)) { ns.aOk = false; return y; }
    const w = p.w;
    let st = c - w * 0.5; st -= Math.floor(st);
    let e = phi - st; if (e < 0) e += 1;
    const kk = p.lock === 1 ? k / w : k;
    const er = p.mirror === 1 && e > w * 0.5 ? w - e : e;
    const hp = kk * er;
    let cp;
    if (md === 0 || md === 5) cp = hp + ns.xin + ns.cacc;
    else {
      const Ib = ns.ov > 0 ? p.I * (1 + 4 * p.colB * ns.ov) : p.I;
      cp = ns.xin + ns.cacc + (p.fmType === 1 ? hp + ns.acc
        : hp + Ib * 0.15915494309189535 * RazorCore.mod(p.mshape, md === 1 ? p.mEff * er : modX, ns.seed));
    }
    const cp0 = ns.acp, ok = was && ns.aOk;
    ns.acp = cp; ns.aOk = true;
    if (!ok) return y;
    const dcp = cp - cp0;
    let hot;
    if (Math.abs(dcp) < 1e-7) { const x = cp - 0.5 * dcp; hot = RazorCore.wave(p.hot, x - Math.floor(x)); }
    else hot = (RazorCore.F(p.hot, cp) - RazorCore.F(p.hot, cp0)) / dcp;
    const base = RazorCore.wave(p.base, phi), xin0 = inp === undefined ? base : inp;
    if (md === 5) hot = xin0 * hot;                    // ring: the carrier times the input (:200)
    return xin0 + ns.g * p.depth * (hot - xin0);
  }

  /* B382: SwarmSynth from its patched text (SWARM_PATCHES, top of this factory) */
  function patchSwarm(src) {
    for (const x of SWARM_PATCHES) for (const [a, b] of x.edits) {
      const i = src.indexOf(a);
      if (i < 0 || src.indexOf(a, i + 1) >= 0) throw new Error(`scalpel-horde-engine: ${x.id}'s anchor is not exactly once in swarmsaw.html's DSP section: ${a.trim()}`);
      src = src.slice(0, i) + b + src.slice(i + a.length);
    }
    return src;
  }
  const SwarmSynth = new Function('"use strict";\n' + patchSwarm(swarmSrc) + '\nreturn SwarmSynth;')();

  class ComposedEngine extends RazorCore {
    /* B382: the patched SwarmSynth and its registered divergences, for the checks (a SwarmSynth with no
       flag in its p is the reference's, bit for bit) */
    static get SwarmSynth() { return SwarmSynth; }
    static get swarmPatches() { return SWARM_PATCHES.map(x => ({ id: x.id, flag: x.flag, keys: x.keys.slice() })); }
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
      this.voiceCap = 0;                 // B323: the host's cap on sounding voices; 0 = off (cull())
      this.culled = 0;                   // tails culled so far (the lab's load meter shows it)
      this.capPolicy = 0;                // B375: at a full cap, 0 refuse (default), 1 steal with the fade, 2 replace (B323)
      this.refused = 0; this.stolen = 0; // B375: notes refused, held voices stolen, so far (the load meter)
      /* B335 gravity: the fixed-TIME grid (ADR-086 A1: exactly 256 at 44.1 kHz, 279 at 48 kHz),
         the samples owed to it (counted from the first render, as DynSynth's gravAccum is), and
         the readout (DynSynth's gravInfo, in fixed arrays: index into RATIOS, octave, cents) */
      this.gravGrid = Math.max(1, Math.round(sr * 256 / 44100));
      this.gravAccum = 0;
      this.gravN = 0; this.gravRatio = new Int32Array(32); this.gravOct = new Int32Array(32); this.gravErr = new Float64Array(32);
      this.gAct = [];
      /* B335 the ensemble timing state (swarm_core.h tOff/tRng/ensSeed): tOff PERSISTS ACROSS NOTES,
         which is the whole of ADR-077; it re-derives on a seed change only (B149) */
      this.tOff = new Float64Array(9); this.tRng = 0; this.ensSeed = 0; this.ensSeeded = false;
      /* B335 per-member gains ride RazorCore's pan gains (stepM): the render-call counter, the
         call whose pan gains are snapshotted, the snapshot, and this call's envelope times */
      this.rCall = 0; this.glCall = -1; this.glB = new Float64Array(9); this.grB = new Float64Array(9);
      this.pvLive = false; this.eA = 0; this.eR = 0; this.eDC = 0;
      this.voices.forEach((v, si) => {
        v.si = si; v.sn = 0; v.cull = false;
        v.gOn = false; v.gf0 = 0; v.gfb = 0; v.pv = false;
        v.m.forEach((m, i) => {
          m.v = v; m.i = i; m.j = 0; m.dph = 0;
          m.onsD = 0; m.onsD0 = 0; m.onsC = 0; m.relC = 0; m.aMul = 1; m.rMul = 1;
          m.onsE = 0; m.eS = 0; m.eE = 0; m.hold = false; m.pg = 1; m.eCall = -1; m.eAi = 0; m.eRc = 0;
          m.fu = 0; m.f1 = 0; m.f2 = 0;                // ADR-189 D3: the loop filter's state and its taps (bladeStep)
        });
        v.fx0 = 0;                                     // ADR-189 D3: member 0's sample before it steps (loopIn)
      });
      /* ADR-189: every blade state gets the same fields up front, so V8 keeps one shape per state
         (B346 measured a shape change as a cost of its own). aaReal marks the MEMBER states, the
         only ones D1 filters (scratch states serve the BLEP height probe and the DC estimators);
         acp/aOk are D1's previous carrier phase and its validity. */
      const aaState = (ns, real) => { ns.aaReal = real; ns.acp = 0; ns.aOk = false; };
      for (const ns of [this.sc, this.sc2, this.bxs.ns3, this.bxs.ns4]) aaState(ns, false);
      for (const v of this.voices) for (const m of v.m) for (const ns of [m.ns, m.ns2, m.bx.ns3, m.bx.ns4]) aaState(ns, true);
      this.aaWas = false;                              // D1 on during the previous render (renderBlock)
      /* the lab assigns c.post = fn; the viz message gains the swarm's state */
      let user = null;
      const wrap = msg => {
        if (msg && msg.t === 'viz' && msg.mem && this.src === 'horde') msg.horde = this.hordeViz();
        user(msg);
      };
      Object.defineProperty(this, 'post', { configurable: true, enumerable: true,
        get() { return user ? wrap : null; }, set(f) { user = f || null; } });
    }

    /* B315: Rotate spread is bipolar (header, "BIPOLAR ROTATE SPREAD"). The oracle is handed
       |x|, so its gate (razor-core.js:786, :791) opens for either sign; the sign waits here
       for spread(). The sign store is created lazily: RazorCore's constructor runs before
       this class's fields would exist. */
    set(m) {
      if (m && ('rotSpread' in m || 'rotSpread2' in m)) {
        const g = this.rotSign || (this.rotSign = { rotSpread: 1, rotSpread2: 1 });
        m = Object.assign({}, m);
        for (const k of ['rotSpread', 'rotSpread2']) if (k in m) { const x = +m[k]; g[k] = x < 0 ? -1 : 1; m[k] = Math.abs(x); }
      }
      super.set(m);
    }

    /* the oracle's spread law, then the sign it could not keep: rotOff = x·2·pn(2) with x signed.
       Blade 2 follows blade 1's offsets unless it owns its spreads (b2sp), as spreadMember does. */
    spread(v) {
      /* ADR-184 A2 (2): under Quantize the oracle snaps Cut spread with Math.round (razor-core.js
         kSpread :148), which rounds halves UP, so −2.5 snapped to −2 while +2.5 snapped to 3. The
         oracle is handed the spread already rounded half away from zero; an integer passes its
         Math.round unchanged, so every value but a negative half is bit-identical to the oracle.
         The smoothed values are put back at once: nothing else in spread() reads them. */
      const s = this.s, q = this.d.kq, k1 = s.kspread, k2 = s.kspread2;
      if (q) { s.kspread = roundAway(k1); s.kspread2 = roundAway(k2); }
      super.spread(v);
      if (q) { s.kspread = k1; s.kspread2 = k2; }
      const g = this.rotSign;
      if (!g || (g.rotSpread > 0 && g.rotSpread2 > 0)) return;
      const own = this.d.b2sp, N = this.d.N;
      for (let i = 0; i < N; i++) {
        const m = v.m[i];
        if (g.rotSpread < 0) m.rotOff = -m.rotOff;
        m.rotOff2 = own ? (g.rotSpread2 < 0 ? -m.rotOff2 : m.rotOff2) : m.rotOff;
      }
    }

    setOS(n) {
      super.setOS(n);
      this.loopA = 1 - Math.exp(-TAU * LOOP_FC / (this.sr * n));   // ADR-189 D3's one-pole, per internal sample
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
      for (const x of SWARM_PATCHES) for (const k of x.keys) p[k] = d[k];   // B382: the divergences' keys
      this.gCoefS = p.freqGlide > 0 ? 1 - Math.exp(-1 / (p.freqGlide * 0.25 * this.sr)) : 0;   // swarmsaw.html:655
    }

    /* B310, HORDE'S VOICE LAW (ADR-083, src/swarm_core.h alloc()). The human:
       "when I play the same note twice in a row, the release of the first note
       doesn't continue and instead the note gets stolen". RazorCore's poly
       noteOn (razor-core.js:368-379) first reuses ANY active voice holding the
       same note, releasing ones included, so a repeat cut the first release off.
       horde has no same-note reuse: a note-on takes
         1) a FREE slot (released and faded), oldest first;
         2) else a RELEASING tail, quietest first, age as the tiebreak;
         3) else (every slot gated) the oldest held voice.
       "Faded" maps onto RazorCore's own voice envelope `v.env` (the ADSR,
       razor-core.js:777-779, which scales the voice's output as v.env*v.vel*norm,
       :862), with horde's threshold: !gate && env < 1e-3, the same test on the
       same kind of field (swarm_core.h, Voice::env). RazorCore's `!active` is
       env < 1e-4 and is a subset of it. Ages are RazorCore's v.age (++this.age
       per start; 0 for a never-used slot), strict `<` so ties fall to pool order,
       as horde's loop does. The pool is RazorCore's (voices 0..d.poly-1).
       Every allocated slot is a NEW voice, so startVoice runs with fresh = true
       (the oracle's steal path did the same; only its same-note reuse was not).
       Mono/legato (d.polyMode) is RazorCore's path, untouched.
       NOTE-OFF IS INHERITED, deliberately: RazorCore releases every voice with
       that note that is STILL GATED (razor-core.js:433), which is horde's rule
       (SwarmCore::noteOff releases by key, every gated match, swarm_core.h
       noteOff; the shell releases by key, hypersaw_clap.cpp noteOffAll). So on
       A-on, A-off, A-on, A-off the second off finds the new, gated voice and the
       first tail is left to ring; a doubly-held A is released whole by one off,
       so a host that merges the two offs into one cannot strand a stuck note.
       Deterministic: no draw, no clock. */
    noteOn(note, freq, vel) {
      const d = this.d;
      if (d.polyMode) return super.noteOn(note, freq, vel);
      const pool = this.voices.slice(0, d.poly);
      /* B323: at the voice cap a note REPLACES a sounding voice instead of adding one, by the
         same tiers (a sounding tail first; only if none, the oldest held). Below the cap, or
         with no cap, `busy` is false and this is B310's law exactly. */
      const busy = this.voiceCap > 0 && this.liveCount() >= this.voiceCap;
      let v = this.tierPick(pool, busy ? x => !x.active : null);
      /* B375: a full cap and no tail to take, so only held voices sound. B323 re-struck the oldest
         held one at once (REPLACE, policy 2, below). REFUSE (0, the default) drops the note: no
         voice is touched. STEAL (1) releases the oldest held voice and hands it to the cull's 8 ms
         fade (it no longer counts as live, so the count stays at the cap), and the note takes a
         FREE slot (inactive: tier 1 over the free ones); with the pool full of sounding voices there
         is none, and then it is REPLACE, horde's own full-pool law (ADR-083 tier 3, at once). */
      if (!v && busy && this.capPolicy !== 2) {
        if (this.capPolicy !== 1) { this.refused++; return; }
        let h = null;
        for (const x of pool) if (x.active && x.gate && !x.cull && (!h || x.age < h.age)) h = x;
        const f = this.tierPick(pool, x => x.active);
        if (h && f) {
          h.gate = false; h.stage = 4;                  // released, so its own envelope falls under the ramp too
          h.cull = true; h.cullG = 1; this.stolen++;
          v = f;
        }
      }
      if (!v) for (const x of pool) if ((!busy || x.active) && (!v || x.age < v.age)) v = x;
      // the rest is RazorCore's noteOn tail (razor-core.js:376-379), unchanged
      this.startVoice(v, note, freq, vel, true, true);
      v.freq = v.freqT = freq;
      this.couple(v, this.s, d);
      this.spread(v);
    }

    /* ADR-083 tiers 1 and 2, shared by note-on (B310) and the cull (B323): the oldest FADED
       slot (not gated, env < 1e-3), else the QUIETEST releasing tail (age breaks ties, strict
       `<` so ties fall to pool order). Never a gated voice: tier 3 (the oldest held) belongs to
       note-on alone. `skip(x)` removes candidates (the cull skips silent and fading voices). */
    tierPick(pool, skip) {
      let v = null;
      for (const x of pool) if (!x.gate && x.env < 1e-3 && !(skip && skip(x)) && (!v || x.age < v.age)) v = x;
      if (v) return v;
      for (const x of pool)
        if (!x.gate && !(skip && skip(x)) && (!v || x.env < v.env || (x.env === v.env && x.age < v.age))) v = x;
      return v;
    }

    /* B323, GRACEFUL DEGRADATION UNDER LOAD. The human (B313, 2026-09-28): "I'm noticing the
       issue [overload] on many other patches now." Measured: six voices of a two-blade N 6
       patch cost ~150% of real time in the lab's worklet. `voiceCap` is an INPUT, set by the
       host (the lab's worklet shell measures its own render time and lowers or raises the cap;
       msg {t:'cap', n}); 0 is off. This module never reads a clock (SPEC §5.7): the same cap
       and note order give the same samples. While more voices sound than the cap allows, the
       cull FADES OUT the quietest releasing tails, by tiers 1 and 2 above; a gated (held)
       voice is never culled, so a held chord can exceed the cap and only note-on's tier 3
       (noteOn above) ever takes a held note, as it always has at a full pool. A culled tail is
       never cut: its gain ramps linearly to 0 over CULL_FADE (8 ms), per sample, and only then
       is the slot freed (render below). */
    liveCount() { let n = 0; for (const v of this.voices) if (v.active && !v.cull) n++; return n; }
    cull() {
      let live = this.liveCount();
      while (live > this.voiceCap) {
        const v = this.tierPick(this.voices, x => !x.active || x.cull);
        if (!v) break;                                  // only held voices are left: never culled
        v.cull = true; v.cullG = 1; live--; this.culled++;
      }
    }

    msg(o) {
      if (o && o.t === 'cap') { this.voiceCap = Math.max(0, Math.floor(+o.n || 0)); return; }
      if (o && o.t === 'capPolicy') { const n = Math.floor(+o.n || 0); this.capPolicy = n === 1 || n === 2 ? n : 0; return; }   // B375
      super.msg(o);
    }

    startVoice(v, note, freq, vel, fresh, retrig) {
      v.cull = false;                                  // a re-allocated slot is a new voice, never still fading
      super.startVoice(v, note, freq, vel, fresh, retrig);
      /* ADR-189: a fresh voice's loop starts from silence, as the oracle's does (y1 = y2 = 0,
         razor-core.js :401), so D3's filter and taps start at 0 too */
      if (fresh) for (const m of v.m) { m.fu = 0; m.f1 = 0; m.f2 = 0; }
      /* A non-fresh retrigger keeps the swarm running, as RazorCore keeps its phases
         (SwarmSynth would start a new swarm: an open question for the lead). B335: it keeps
         the ensemble's draws too (swarm_core.h retargetNote re-strikes through initVoice and
         draws again: the same open question), and each member's own envelope re-enters its
         attack from where it stands, as RazorCore's voice envelope does (stage 1). */
      if (!fresh) { if (retrig && v.pv) for (const m of v.m) m.eS = 1; return; }
      v.gOn = false;                                   // B335, ADR-008: a note-on resets to ET
      v.pv = false;
      if (this.src === 'horde') {
        this.syncSwarm();
        const S = this.sw.swarms[v.si];
        this.sw.alloc = () => S;                       // bind swarm ↔ voice for this one call
        this.sw.noteOn(note, freq * Math.pow(2, this.s.bend / 12));
        delete this.sw.alloc;
        v.sn = this.nBase;
        for (const m of v.m) { m.j = 0; m.phi = frac(S.phase[m.i] + this.origin); }
        this.armMembers(v);                            // B335 onset scatter / per-partial env
        /* B325: a PROVISIONAL first tick, so the member frequencies exist before RazorCore reads
           them. noteOn's couple() and spread(), and the first sample's Hz-unit cut rates (lock 2),
           Hz-unit modulators (mUnit) and per-cycle DC estimate, all read m.inc. Before this,
           couple() handed them the UNDETUNED pitch until its next 32-sample pass. That produced
           two artefacts that were the composition's own:
           - a DC estimate taken with the wrong cut rate: a slow offset of up to 2.5e-2 per note
             that took up to ~85 ms to correct, on lock-2 presets (Formant pluck, arpeggio);
           - a modulator phase offset that never recovered (Crunch horde, mUnit 1).
           The B325 audit found both in the neutral case, where the composed engine must equal
           the oracle. The tick is only a LOOK-AHEAD. The swarm is snapshotted first, and stepM
           restores it and takes the real first tick exactly where it always did, so the swarm's
           trajectory (and oracle O1) is unchanged, whatever happens before the first sample.
           A first try kept this tick and skipped the scheduled one. It froze the pitch of the
           FIRST of several note-ons that land on a mono voice in the same block (a chord on a
           mono preset), which the audit's neutral case caught at once. couple() re-takes the
           look-ahead when the pitch moved before the first sample, so m.inc follows it.
           Only when the first tick is due on the note's first sample: (v.sn & 15) === 0, always
           so in the worklet's 128-sample blocks. */
        v.tick0 = false;                               // a look-ahead left by a start this one replaces is void
        if ((v.sn & 15) === 0) this.lookAhead(v, S, true);
      } else {
        /* the oracle's start, handed over in the horde frame and mapped back:
           exact for its aligned 0 and seeded-draw starts (multiples of 2^-32) */
        for (const m of v.m) { m.j = 0; m.phi = frac(frac(m.phi - 0.5) + this.origin); }
      }
    }

    /* B335 THE ENSEMBLE STREAM, transcribed from src/swarm_core.h (the C++ is the reference; there is
       no JS one). rngT is mulberry32 (forcecore::rngNext, the form of SwarmSynth.rngS), gaussT the
       C++'s Box-Muller (:2125-2130, its truncated 2π literal included), ensSync its rebuild()
       rule (:1483-1489): the stream and the offsets re-derive when the SEED changes and never
       otherwise, because tOff is memory the ensemble accumulates across notes. Checked lazily at
       note-on rather than on the seed write: nothing draws from the stream in between. */
    rngT() {
      this.tRng |= 0; this.tRng = (this.tRng + 0x6D2B79F5) | 0;
      let t = Math.imul(this.tRng ^ (this.tRng >>> 15), 1 | this.tRng);
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    }
    gaussT() {
      let u = this.rngT(); if (u < 1e-9) u = 1e-9;
      const w = this.rngT();
      return Math.sqrt(-2 * Math.log(u)) * Math.cos(6.283185307 * w);
    }
    ensSync() {
      const sd = this.d.seed;
      if (!this.ensSeeded || this.ensSeed !== sd) {
        this.ensSeeded = true; this.ensSeed = sd;
        this.tRng = ensembleSeed(sd);
        this.tOff.fill(0);
      }
    }

    /* B335 ARM A FRESH NOTE'S MEMBERS: swarm_core.h initVoice :643-685, transcribed. Per member,
       in the C++'s draw order: an attack jitter and a release jitter (both drawn whenever either
       feature is on, even at scatter 0, so the stream's sequence is the C++'s), then, with onset
       scatter on, the Vorberg/Wing correction of the PERSISTENT offsets
           tOff_i <- tOff_i − alpha·(tOff_i − mean) + N(0, scatter ms)
       re-centred, converted to samples, and shifted so the earliest member starts at once (the
       note must not feel late, only internally spread). onsC/relC are the C++'s own one-pole
       coefficients from this engine's attack and release times (s.A, s.R: ms -> seconds): onsC IS
       the entry ramp below; relC is kept only so the check can compare the draw. aMul/rMul are the
       drawn TIME FACTORS, max(0.15, jitter), that the per-partial envelopes scale A and R by. */
    armMembers(v) {
      const d = this.d, N = d.N, sr = this.sr;
      v.pv = d.onsetScatter > 0 || d.voiceEnv > 0.5;
      if (!v.pv) return;
      this.ensSync();
      const As = this.s.A * 0.001, Rs = this.s.R * 0.001;
      for (let i = 0; i < N; i++) {
        const m = v.m[i];
        m.onsD = 0; m.onsE = 0; m.hold = false; m.pg = 1;
        const jit = 1 + this.gaussT() * d.attackScatter * 0.6;
        m.aMul = Math.max(0.15, jit);
        m.onsC = 1 - Math.exp(-1 / (Math.max(0.002, As * m.aMul) * sr));
        const rjit = 1 + this.gaussT() * d.relScatter * 0.6;
        m.rMul = Math.max(0.15, rjit);
        m.relC = 1 - Math.exp(-1 / (Math.max(0.002, Rs * m.rMul) * sr));
      }
      if (d.onsetScatter > 0) {
        const T = this.tOff;
        let mean = 0;
        for (let i = 0; i < N; i++) mean += T[i];
        mean /= (N > 0 ? N : 1);
        const sig = d.onsetScatter * 0.001;
        for (let i = 0; i < N; i++) T[i] += -d.onsetAlpha * (T[i] - mean) + this.gaussT() * sig;
        let m2 = 0;
        for (let i = 0; i < N; i++) m2 += T[i];
        m2 /= (N > 0 ? N : 1);
        for (let i = 0; i < N; i++) v.m[i].onsD = (T[i] - m2) * sr;
        let lo = v.m[0].onsD;
        for (let i = 1; i < N; i++) lo = Math.min(lo, v.m[i].onsD);
        for (let i = 0; i < N; i++) v.m[i].onsD -= lo;
      }
      /* the per-partial envelope (ADR-078 in this engine's law, header (2)): a member that waits
         enters from silence, as the C++'s does (onsE = 0); one that enters at once starts where
         the voice's envelope stands, RazorCore's fresh start (0 on a free slot, the level of a
         stolen one: razor-core.js startVoice `if (!v.active) v.env = 0`) */
      for (let i = 0; i < N; i++) { const m = v.m[i]; m.onsD0 = m.onsD; m.eS = 1; m.eE = m.onsD > 0 ? 0 : v.env; }
    }

    /* B335 ONE SAMPLE OF THE MEMBERS' ENTRY, for voice v: called once per output sample from
       member 0's first step, after the swarm's tick and before any member advances.
       swarm_core.h renderSeg, transcribed per member:
         - WAIT (:1052): while onsD > 0 the member counts down, contributes nothing and does NOT
           advance its phase: it has not started playing (its frozen phase still sits in the
           swarm's mean field, as it does in the C++, which ticks every member).
         - ENTRY RAMP, onset scatter alone (:1137-1142): onsE += (1 − onsE)·onsC on top of the voice
           envelope, so a late entry cannot click in at the level the voice has reached.
         - PER-PARTIAL ENV (:1118-1135, :1198-1204): every member runs RazorCore's ADSR (this
           call's A, D, S, R: razor-core.js render :740-742, :777-779) with its own drawn attack
           and release times; the voice envelope becomes BOOKKEEPING, the loudest member, so
           liveness, the voice law and the cull key off it unchanged (ADR-078's design), and each
           member's gain is its level over that loudest level.
       Both switches are read LIVE, as the C++ reads them per render. A gain of exactly 1 leaves
       the member's output untouched (x·1 is x), which is what makes the unscattered cases exact. */
    memberStep(v) {
      const d = this.d, N = d.N, ens = d.onsetScatter > 0, venv = d.voiceEnv > 0.5;
      if (!ens && !venv) { for (let i = 0; i < N; i++) { v.m[i].hold = false; v.m[i].pg = 1; } return; }
      let vMax = 0;
      const Sus = this.s.S, dC = this.eDC, sr = this.sr, call = this.rCall;
      for (let i = 0; i < N; i++) {
        const m = v.m[i];
        if (m.onsD > 0) { m.onsD -= 1; m.hold = true; m.pg = 0; continue; }
        m.hold = false;
        if (venv) {
          /* this member's attack increment and release coefficient, RazorCore's expressions over its
             own times (the C++'s max(0.002 s, T·factor)), once per render call as RazorCore's are */
          if (m.eCall !== call) {
            m.eCall = call;
            m.eAi = 1 / Math.max(1, Math.max(2, this.eA * m.aMul) * 0.001 * sr);
            m.eRc = 1 - Math.exp(-4 / Math.max(1, Math.max(2, this.eR * m.rMul) * 0.001 * sr));
          }
          if (!v.gate && m.eS !== 0) m.eS = 4;
          if (m.eS === 1) {
            m.eE += m.eAi;
            if (m.eE >= 1) { m.eE = 1; m.eS = 2; }
          } else if (m.eS === 2) m.eE += (Sus - m.eE) * dC;
          else if (m.eS === 4) {
            m.eE -= m.eE * m.eRc;
            if (m.eE < 1e-4) { m.eE = 0; m.eS = 0; }
          }
          if (m.eE > vMax) vMax = m.eE;
        } else {
          m.onsE += (1 - m.onsE) * m.onsC;
          m.pg = m.onsE;
        }
      }
      if (venv) {
        v.env = vMax;
        for (let i = 0; i < N; i++) { const m = v.m[i]; if (!m.hold) m.pg = vMax > 0 ? m.eE / vMax : 0; }
      }
    }

    /* B325: take (first) or re-take the look-ahead tick. v.pre0 holds the swarm as noteOn left
       it: every field a copy, typed arrays into buffers allocated once per voice. `phase` is
       left out on purpose. SwarmSynth.controlTick never writes it (renderSeg and noteOn do), so
       the tick cannot have moved it, and a caller may set start phases between note-on and the
       first sample (composed_engine_check's O3 coupling-law fixture does). */
    lookAhead(v, S, first) {
      if (first) {
        const o = v.pre0 || (v.pre0 = {});
        for (const k in S) { if (k === 'phase') continue; const x = S[k]; if (ArrayBuffer.isView(x)) { if (o[k]) o[k].set(x); else o[k] = x.slice(); } else o[k] = x; }
      } else this.unLookAhead(v, S);
      this.tickSwarm(v, S);
      v.tick0 = true;
    }
    unLookAhead(v, S) {
      const o = v.pre0;
      for (const k in o) { const x = o[k]; if (ArrayBuffer.isView(x)) S[k].set(x); else S[k] = x; }
      v.tick0 = false;
    }

    settle(v) { if (this.src === 'razor') super.settle(v); }

    /* RazorCore's couple() still supplies what is frame-invariant (v.r, each
       member's lead on the mean field); the member frequency is the swarm's. */
    couple(v, s, d) {
      super.couple(v, s, d);
      if (this.src !== 'horde') return;
      const S = this.sw.swarms[v.si], N = d.N;
      /* B325: the pitch moved since the look-ahead (a later note-on on a mono voice, a retune)
         and no sample has rendered yet: look again, so m.inc is the swarm's for the pitch now */
      if (v.tick0 && (S.fBase !== v.freq || S.f0 !== this.f0Of(v))) this.lookAhead(v, S, false);
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
      S.f0 = this.f0Of(v);                             // B335: the played pitch, or gravity's f0cur
      this.sw.controlTick(S);
    }

    /* B335 THE SWARM'S PITCH, f0cur (swarm_core.h Voice::f0cur; DynSynth's s.f0cur). Until gravity
       has moved this voice it IS the played pitch, the very expression the swarm read before B335,
       so every patch gravity never touches is bit-identical. Once moved, f0cur carries its offset
       and rides the played pitch MULTIPLICATIVELY, as the C++ does on every glide step
       (`s.f0cur *= ratio`, swarm_core.h:1731): a bend or glide keeps the settled interval. */
    f0Of(v) {
      const fb = v.freq * Math.pow(2, this.s.bend / 12);
      if (!v.gOn) return fb;
      if (fb !== v.gfb) { v.gf0 *= fb / v.gfb; v.gfb = fb; }
      return v.gf0;
    }

    /* B335 CONSONANCE GRAVITY (ADR-008): reference/swarmdynamics.html gravityStep, :291-321, copied
       expression for expression (the GRAV rows check it against DynSynth itself, exactly). Every
       pair of HELD voices, sorted by pitch, is pulled toward the nearest octave-folded just ratio
       when it lies inside the basin: each note moves err·3·grav·dt/2 cents, in opposite
       directions. Only gated voices pull (a release tail keeps its pitch), and only the horde
       swarm has an f0cur (the razor source is RazorCore's own swarm half). The readout is
       DynSynth's gravInfo in fixed arrays (gravN pairs: RATIOS index, octave, cents error). */
    gravityStep(dtB) {
      this.gravN = 0;
      const g = this.d.grav;
      if (g < 0.005 || this.src !== 'horde') return;
      const act = this.gAct;
      act.length = 0;
      for (const v of this.voices) if (v.gate) {
        if (!v.gOn) { v.gf0 = v.gfb = v.freq * Math.pow(2, this.s.bend / 12); }
        else this.f0Of(v);
        act.push(v);
      }
      if (act.length < 2) return;
      act.sort((a, b) => a.gf0 - b.gf0);
      const rate = g * 3; // full gravity: ~1/3 s settle
      for (let a = 0; a < act.length - 1; a++) {
        for (let b = a + 1; b < act.length; b++) {
          const lo = act[a], hi = act[b];
          let r = hi.gf0 / lo.gf0;
          const oct = Math.floor(Math.log2(r));
          const rf = r / Math.pow(2, oct);
          let bi = 0, be = 1e9;
          for (let i = 0; i < RATIOS.length; i++) {
            const e = Math.abs(1200 * Math.log2(rf / RATIOS[i]));
            if (e < be) { be = e; bi = i; }
          }
          const err = 1200 * Math.log2(rf / RATIOS[bi]); // + = interval sharp
          if (Math.abs(err) > this.d.basin) continue;
          const move = err * rate * dtB * 0.5; // cents to move each note
          hi.gf0 *= Math.pow(2, -move / 1200);
          lo.gf0 *= Math.pow(2, move / 1200);
          hi.gOn = true; lo.gOn = true;
          if (this.gravN < 32) { const k = this.gravN++; this.gravRatio[k] = bi; this.gravOct[k] = oct; this.gravErr[k] = err; }
        }
      }
    }

    /* RazorCore calls stepM once per member per oversampled step, members in
       order, so member 0's first step of each sample is where the swarm's
       16-sample tick belongs (before any member advances). */
    stepM(m, dphi, c, k, s) {
      if (this.src !== 'horde') return this.bladeStep(m, dphi, c, k, s);
      const v = m.v;
      if (m.j === 0) {
        const S = this.sw.swarms[v.si], i = m.i;
        /* B325: undo startVoice's look-ahead before the real first tick (see there) */
        if (i === 0) {
          if ((v.sn & 15) === 0) { if (v.tick0) this.unLookAhead(v, S); this.tickSwarm(v, S); }
          v.sn++;
          if (v.pv) this.memberStep(v);                // B335: waits, entry ramps, per-partial envs
        }
        // swarmsaw.html:665-672 (renderSeg), per member
        const glideOn = this.sw.p.freqGlide > 0;
        if (glideOn) S.fRun[i] += this.gCoefS * (S.eff[i] - S.fRun[i]);
        if (!(v.pv && m.hold)) {                       // B335: a waiting member's phase stands still
          const f = glideOn ? S.fRun[i] : S.eff[i];
          const dph = Math.max(0, f) / this.sr;
          let ph = S.phase[i] + dph;
          ph -= Math.floor(ph);
          S.phase[i] = ph;
          m.dph = Math.max(0, f) / (this.sr * this.os);
        }
      }
      if (++m.j >= this.os) m.j = 0;
      /* B335 THE MEMBER'S GAIN rides RazorCore's pan gains. Its render sums each member as
         `vl += y·gl[q]` right after this call returns (razor-core.js :839-860), with the DC estimate
         already taken off y, so scaling gl[q] here scales the member's DC-corrected output and
         nothing else. gl/gr are rebuilt at the top of every render call (:733-738): the first
         step of a call snapshots them, and every step of that call writes base·gain, 1 for a voice
         with no per-member state. Untouched unless some sounding voice has it (pvLive). */
      if (this.pvLive) {
        const q = m.i;
        if (this.glCall !== this.rCall) { this.glCall = this.rCall; this.glB.set(this.gl); this.grB.set(this.gr); }
        const g = v.pv ? m.pg : 1;
        this.gl[q] = this.glB[q] * g; this.gr[q] = this.grB[q] * g;
      }
      if (v.pv && m.hold) return 0;                    // not started: no output, no blade step
      return this.bladeStep(m, m.dph, c, k, s);
    }

    /* ADR-189: RazorCore's blade step (stepM, razor-core.js :655-723) with D3's loop input before
       it and the loop bookkeeping after it. With every flag 0 the only writes are to fields the
       oracle never reads (fu, f1, f2), so the samples are the oracle's own. */
    bladeStep(m, dphi, c, k, s) {
      const xOn = s.xm > 0.0005 || s.fb > 0.0005;     // the oracle's own switch (render, :809)
      if (xOn && this.d.aaLoop) this.loopIn(m, s);
      const y = super.stepM(m, dphi, c, k, s);
      /* D3's taps: THE ORACLE'S OWN TAP, y (the PolyBLEP output the render shifts into y1/y2, :840),
         through the loop filter (LOOP_FC). ADR-189 A1 dropped the tap "fix" (tapping this sample,
         m.prev, one internal sample earlier): it moved broad#828's cycle from R/5 to R/3 instead of
         curing it, and made broad#857 A5 6.2 dB worse. Run only while the loop is on, as the oracle
         shifts y1/y2, and whatever aaLoop says, so switching D3 on mid-note starts from a warm filter. */
      if (xOn) { m.fu += this.loopA * (y - m.fu); m.f2 = m.f1; m.f1 = m.fu; }
      return y;
    }

    /* ADR-189 D3: THE LOOP'S INPUT, the oracle's own formula (render :814,
         xin = 0.5*(xm*y1[next member] + fb*0.5*(y1 + y2))
       over D3's FILTERED taps (the same samples the oracle taps, low-passed). The oracle's
       cross-mod reads every member's y1 as it stood when the internal sample began (xb, :810); here
       the next member has not stepped yet, except round the ring (member 0, stepped first), whose
       sample is kept in v.fx0 before it steps. The render already wrote the oracle's xin into the
       four blade states; this overwrites it before the blade step reads it (stepM, dcEst and hAt all
       read ns.xin after this point). */
    loopIn(m, s) {
      const v = m.v, N = this.d.N, q = m.i, nb = q + 1 === N ? 0 : q + 1;
      if (q === 0) v.fx0 = m.fu;
      const xin = 0.5 * (s.xm * (nb === 0 ? v.fx0 : v.m[nb].fu) + s.fb * 0.5 * (m.f1 + m.f2));
      m.ns.xin = xin; m.ns2.xin = xin; m.bx.ns3.xin = xin; m.bx.ns4.xin = xin;
    }

    /* ADR-189 D1 and D2: RazorCore's PolyBLEP scanner (scan, razor-core.js :610-644) for one blade's
       carrier wraps (and crush steps). With both flags 0 it IS the oracle's.
       D1: a carrier's wraps get NO BLEP: voiceAA band-limits the carrier itself (replacing, never
       stacked; B346 measured stacking within 1 dB of replacing on 72 of 72, and the BLEP's height
       probe would measure the plain carrier, not what ADAA plays).
       D2: the carrier phase the scanner interpolates is voice()'s, cp = kk*e + [FM] + xin + cacc; the
       oracle's scan omits xin, so under feedback or cross-mod it BLEPs edges where the carrier has
       none and misses the real ones. Here the step's xin is added at BOTH ends, B346's blepXin
       exactly (5 of 72 helped, 0 worse). NOT the previous step's xin at o0, although the carrier did
       move by xin's change too (the oracle's collision idiom, cacc - cd and cacc, :633, would suggest
       it): the BLEP's height comes from hAt (:520-540), which probes the output across the edge
       with the CURRENT xin, so an edge placed on the interpolated xin is probed off the edge and gets
       a height near 0. Measured (B355 trace): that form moved a feedback sync blade's output by
       6e-7 at most, i.e. it corrected nothing. Carriers only: a crush step sits at an integer hold
       phase, which xin does not move (B346's harness shifted the crush scan too; this does not).
       The body below is the oracle's, copied; the marked line is the change. */
    scan(m, st, p0, dphi, c, k, s, dAcc, modX0, g, kB, nsB, modX1) {
      const d = this.d;
      if (!d.aaCarrier && !d.aaXin) return super.scan(m, st, p0, dphi, c, k, s, dAcc, modX0, g, kB, nsB, modX1);
      const carrier = g.mode === 0 || g.mode === 5 || RazorCore.isFM(g);
      if (d.aaCarrier && d1Takes(g, 1)) return;        // D1, where it takes this carrier (scan runs only with Band-limit on)
      if (!carrier || !d.aaXin) return super.scan(m, st, p0, dphi, c, k, s, dAcc, modX0, g, kB, nsB, modX1);
      const w = g.w, ns = nsB;
      let off = -1, step = 1;
      if (g.hot === 2 || g.hot === 4) { off = 0.5; step = 1; }
      else if (g.hot === 3) { off = 0; step = 0.5; }
      if (off < 0) return;
      let e0 = p0 - st; e0 -= Math.floor(e0);
      if (e0 >= w) return;
      const kk = g.lock === 1 ? kB / w : kB;
      const eEnd = Math.min(e0 + dphi, w), tmax = (eEnd - e0) / dphi;
      const hw = w * 0.5, refl = g.mirror === 1;
      const r0 = refl && e0 > hw ? w - e0 : e0, r1 = refl && eEnd > hw ? w - eEnd : eEnd;
      let o0 = 0, o1 = 0;
      if (RazorCore.isFM(g)) {
        if (g.fmType === 1) { o1 = ns.acc; o0 = o1 - dAcc; }
        else {
          const sc = 0.15915494309189535 * g.I;
          o0 = sc * RazorCore.mod(g.mshape, g.mode === 1 ? g.mEff * r0 : modX0, ns.seed);
          o1 = sc * RazorCore.mod(g.mshape, g.mode === 1 ? g.mEff * r1 : modX1, ns.seed);
        }
      }
      if (ns.cd) { o0 += ns.cacc - ns.cd; o1 += ns.cacc; }
      o0 += ns.xin; o1 += ns.xin;                      // D2: THE CHANGE (the xin input the carrier reads, :187)
      const cp0 = kk * r0 + o0, cp1 = kk * r1 + o0 + (o1 - o0) * tmax;
      const lo = Math.min(cp0, cp1), hi = Math.max(cp0, cp1);
      if (!(hi - lo > 1e-12 && hi - lo < 8)) return;
      let j = Math.floor((lo - off) / step) + 1;
      for (let q = 0; q < 6; q++, j++) {
        const pos = off + j * step; if (pos > hi) break;
        if (pos <= lo || pos === 0) continue;
        const tau = (pos - cp0) / (cp1 - cp0) * tmax;
        if (tau > 0 && tau <= 1) this.addE(m, p0 + tau * dphi, tau, c, k, s);
      }
    }

    /* B323: with no cull running this is ONE call of RazorCore's render, as before. While a
       culled tail fades, the block is rendered ONE SAMPLE per call (into two reused 1-sample
       buffers: nothing is allocated) and every fading voice's envelope is scaled after each
       sample, so its gain follows the ramp g(t) = 1 − t/CULL_FADE sample by sample. RazorCore's
       render keeps its counters in the instance (the 16-sample smoother, the 32-sample couple
       tick, the viz counter), so the calls step exactly as the whole block would; what a call
       re-reads at its start is the per-CALL setup (the pan law from the smoothed width, the
       envelope rates from A/D/R, razor-core.js:731-745), which differs from the whole block's only
       while those controls glide. The check measures the steady case bit-identical. Cost: the
       per-call setup is ~3% at one sample per call (measured, trace b323), for 8 ms. */
    /* B335 GRAVITY'S GRID (ADR-086 and Amendment 1, the DynSynth render loop, swarmdynamics.html
       :411-427): with gravity on, the block is rendered in SEGMENTS that end on the fixed-time
       grid, and gravity steps BETWEEN them, so its explicit-Euler trajectory depends on the
       cumulative sample count and never on the host's block size, and on time, not the rate.
       The count runs from the first render whether gravity is on or not (DynSynth's gravAccum
       does), so turning it on mid-note lands on the same grid. With gravity off the block is ONE
       renderBlock, exactly the pre-B335 render: RazorCore re-reads a few smoothed controls once
       per call (the pan law, the envelope rates), so segmenting a gravity-off block could move a
       sample while those glide; it is never done. */
    render(L, R) {
      if (this.voiceCap > 0) this.cull();
      const grid = this.gravGrid;
      if (!(this.d.grav >= 0.005) || this.src !== 'horde') {
        this.gravN = 0;
        this.renderBlock(L, R);
        this.gravAccum = (this.gravAccum + L.length) % grid;
      } else {
        for (let done = 0; done < L.length;) {
          const seg = Math.min(L.length - done, grid - this.gravAccum);
          this.renderBlock(L.subarray(done, done + seg), R.subarray(done, done + seg));
          this.gravAccum += seg; done += seg;
          if (this.gravAccum >= grid) { this.gravityStep(grid / this.sr); this.gravAccum = 0; }
        }
      }
      if (this.src !== 'horde') return;
      /* keep-phase snapshot of the newest sounding swarm (swarmsaw.html:714) */
      let lv = null;
      for (const v of this.voices) if (v.active && (!lv || v.age > lv.age)) lv = v;
      if (lv) this.sw.lastPhase.set(this.sw.swarms[lv.si].phase);
    }

    /* B335: the state RazorCore's render reads ONCE PER CALL, taken just before the call so that it
       is the very value that call reads (razor-core.js :740-742): the per-partial envelopes use it,
       and the render-call counter tells stepM that RazorCore has rebuilt its pan gains. */
    preCall() {
      this.rCall++;
      if (!this.pvLive) return;
      const s = this.s;
      this.eA = s.A; this.eR = s.R;
      this.eDC = 1 - Math.exp(-4 / Math.max(1, s.D * 0.001 * this.sr));
    }

    /* ADR-189 D1: the render with voiceAA in RazorCore.voice's place (header, ADR-189), restored
       whatever happens. A render that has just switched D1 on has no previous carrier phase on any
       state, so every state starts plain (aOk). */
    renderBlock(L, R) {
      const aa1 = !!this.d.aaCarrier;
      if (aa1 && !this.aaWas) for (const v of this.voices) for (const m of v.m) { m.ns.aOk = false; m.ns2.aOk = false; m.bx.ns3.aOk = false; m.bx.ns4.aOk = false; }
      this.aaWas = aa1;
      if (!aa1) return this.renderPlain(L, R);
      aaBand = this.d.aa ? 1 : 0;                      // render() copies d.aa into s.aa at its top (:728): the same value
      RazorCore.voice = voiceAA;
      try { this.renderPlain(L, R); } finally { RazorCore.voice = VOICE0; }
    }

    renderPlain(L, R) {
      let fading = false;
      this.pvLive = false;
      for (const v of this.voices) if (v.active) { if (v.cull) fading = true; if (v.pv) this.pvLive = true; }
      if (!fading) { this.preCall(); super.render(L, R); this.nBase += L.length; }
      else {
        const one = this.one || (this.one = [new Float32Array(1), new Float32Array(1)]);
        const step = 1 / (CULL_FADE * this.sr);
        for (let i = 0; i < L.length; i++) {
          this.preCall();
          super.render(one[0], one[1]);
          L[i] = one[0][0]; R[i] = one[1][0];            // R may BE L (a mono host): R last, as RazorCore writes
          this.nBase++;
          for (const v of this.voices) if (v.cull && v.active) {
            const g = v.cullG - step;
            if (g <= 0) { v.env = 0; v.active = false; v.cull = false; }   // freed at the ramp's end
            else {
              /* B335: a per-partial voice's envelope is its loudest member (memberStep), so the
                 ramp scales the members and the bookkeeping level follows them */
              const f = g / v.cullG;
              v.env *= f; v.cullG = g;
              if (v.pv) for (const m of v.m) m.eE *= f;
            }
          }
        }
      }
    }

    hordeViz() {
      let lv = null;
      for (const v of this.voices) if (v.active && (!lv || v.age > lv.age)) lv = v;
      if (!lv) return null;
      const S = this.sw.swarms[lv.si], N = this.d.N;
      const o = { R: S.R, psi: frac(S.psi / TAU), Kenv: S.Kenv, KsmS: S.KsmS, KsmP: S.KsmP, sigma: S.sigma,
        eff: Array.from(S.eff.subarray(0, N)), phase: Array.from(S.phase.subarray(0, N)) };
      /* B335, for the lab's next round: the swarm's pitch (gravity's f0cur once it has moved), the
         pairs gravity holds (DynSynth's gravInfo: ratio, octave, cents), and each member's onset
         (ms after the note-on, swarm_core.h's onsD0) and current gain */
      o.f0 = S.f0;
      o.grav = [];
      for (let k = 0; k < this.gravN; k++) o.grav.push({ ratio: RATIOS[this.gravRatio[k]], oct: this.gravOct[k], err: this.gravErr[k] });
      if (lv.pv) { o.onsetMs = lv.m.slice(0, N).map(m => m.onsD0 / this.sr * 1000); o.gain = lv.m.slice(0, N).map(m => m.pg); }
      return o;
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
