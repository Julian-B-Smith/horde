# horde — decisions (append-only)

## ADR-001 · One kernel-agnostic engine — ACCEPTED
SAW mode is the per-partial swarm engine at P=1 with a saw kernel; SPECTRA is the same engine at P>1 with a sine kernel; wavetable kernel reserved (terrain-sibling crossover). Rationale: the three prototypes converged on identical state (phases, vf, couple, R/ψ/σ, slewed K) differing only in kernel and partial count. Consequence: Phase 4 must prove mode parity at P=1.

## ADR-002 · CLAP-native, VST3 via clap-wrapper — ACCEPTED
Carried from prior platform exploration (JUCE alternatives). Revisit only if a Phase 0 host-compat blocker appears.

## ADR-003 · JS prototypes are the reference implementation — ACCEPTED
swarmsaw/swarmspectra/swarmdynamics HTML cores are the spec-in-code. C++ correctness is defined as parity (L0-1) plus trajectory regressions, not as independent reimplementation. The prototypes' headless Node harnesses are the templates for `./verify`. Divergence requires an ADR here.

## ADR-004 · σ-normalized K with squared taper; absolute-K escape hatch — ACCEPTED
K_Hz = 4·K·|K|·σ_measured places the phase transition mid-knob at any detune/law/register (validated). σ floored at 0.05–0.08 Hz. Normalization collapses for identical oscillators, so an absolute-K advanced mode is required for strict-chimera experiments (L0-10 note).

## ADR-005 · Tempo-grid quantization is a detune law — ACCEPTED
Not a subsystem: voices are placed by the distribution, then Hz offsets snap to the nearest grid multiple. First implementation (forced ladder) ignored the detune knob and produced ±23 Hz spreads; quantize-to-grid fixed spread, exactness, and coherence simultaneously (L0-12). Unit is cycles-per-beat.

## ADR-006 · Additive renderer: oscillator bank vs iFFT — OPEN
Phase 0 spike. Bank is simpler and parity-friendly; iFFT scales past ~200 sines and fits control-rate coupling naturally. Decide on measured min-spec headroom at 128 partials × 5 voices × 4 notes.

## ADR-007 · Splay is assigned-slot, not emergent N-th-harmonic coupling — ACCEPTED
N-th-harmonic mean-field coupling rotates at N·f and cannot be held across a 16-sample control tick; it also admits full-sync as a spurious solution. Assigned slots anchored on the center voice (argmin |x|) are stable, tick-holdable, and avoid pitch sag. Splay authority is 3× sync (detune fights slot precision; at 1× the multiplication was only 2.4× baseline vs 9× at 3×). Detune under splay is reframed as "looseness."

## ADR-008 · Consonance gravity is an audio-engine force — ACCEPTED
Runs on per-note f0cur inside the engine (per-render-block pairwise pulls, basin-limited, octave-folded ratio snap), not as MIDI preprocessing. This is the substantive distinction from the Hermode/Pivotuner/Alt-tuner category (see PRIOR-ART) and what makes settling an audible physical event. Note-on resets to ET so the settle is per-chord.

## ADR-009 · Coupling slew coefficients are per-tick at 2756 ticks/s — ACCEPTED (recorded as a trap)
16-sample ticks at 44.1 kHz. τ ≈ 1/(coef × 2756). A 10× mental error here silently destroyed the cascade zipper once. All time-constant math in the C++ port must be expressed in seconds and converted, never hand-tuned per-tick.

## ADR-010 · Spin-up ratifications (2026-07-17) — ACCEPTED
Survey conducted via /spinup; answers in project.manifest.json (provisional pending final ratification). (a) Project/repo name is HYPERSAW; SWARM✱ branding stays open until the Phase 5 naming decision. (b) Architecture rung 2 — single thread + fresh-context verifier/subagents; earned by the parity oracle, fleet not earned (one engine per ADR-001). (c) Model pins per doctrine: lead/critic Opus, implementer Sonnet, verifier Haiku; judgment-bearing DSP/oracle work stays with the lead. (d) Tonality and terrain-sibling intake briefs deferred to Phase 3 / Phase 4 respectively; the default ratio set and stubbed kernel are the visibly-degraded placeholders. (e) Kit docs (SPEC/ACCEPTANCE/ROADMAP/PRIOR-ART/PARKED + this log) adopted as canonical; ACCEPTANCE.md numbers are the oracle contract.

## ADR-011 · swarmsaw_v2.html supersedes swarmsaw.html as the SAW reference — ACCEPTED
Per CHANGE-NOTE-splay-legibility.md (2026-07-17): v2 = v1 + R_N (Daido) order parameter in controlTick + splay-legibility visualization (dual meters, seat rings, formation polygon). Diff-verified additive; no audio-path changes. SPEC §5.6 and ACCEPTANCE L0-3 amended accordingly. R_N is parity-relevant for the C++ port (displayed AND asserted). swarmsaw.html retained for history until the human approves deletion; ADR-003's reference list should be read with this substitution.

## ADR-012 · Reference timeline consolidated; change notes archived under docs/ — ACCEPTED
The definitive timeline of the SAW reference, for any agent reconstructing it: (1) 2026-07-17 morning — `swarmsaw.html` v1 arrives with the spec kit; (2) mid-spin-up — `swarmsaw_v2.html` + the splay-legibility change note arrive (v2 = v1 + R_N readout + splay visualization; diff-verified additive), amendments folded into SPEC §5.6 / ACCEPTANCE L0-3 (ADR-011); (3) post-ratification cleanup — v1 deleted (human-approved), **v2 renamed to `swarmsaw.html`**, so the ADR-003 reference names hold again and both v1 and v2 remain recoverable in git history (v1 and the pre-rename v2 live in the initial commit). Policy from here: reference-implementation updates land as in-repo edits under version control with an ADR; externally-authored change notes are archived at `docs/change-notes/YYYY-MM-DD-<topic>.md`, folded into SPEC/ACCEPTANCE in the same change, never left as loose root files.

## ADR-013 · GUI is an early priority, not a Phase 5 deliverable — ACCEPTED
Human direction at ratification (2026-07-17): a GUI matching the prototypes' visual design as closely as possible is an early priority. ROADMAP amended: Phase 2 now includes GUI v1 (phase circle with dual R₁/R_N meters, seat rings, formation polygon, XY pad, live readouts — the SPEC §5.6 contract, styled from the prototype design language); Phase 5 keeps the full face (phase carpet, partial strips, mod-matrix UI, MPE surface). The prototypes' CSS/canvas code is the design reference — extract tokens (palette, meter treatments, layout) rather than reinventing. Consequence for Phase 0: the CLAP skeleton must pick the GUI stack early enough that Phase 2 can ship a real GUI; renderer spike (ADR-006) and GUI-stack choice are both Phase 0 exit criteria now.

## ADR-014 · Public repo: private siblings are aliased in tracked files — ACCEPTED
Pre-push sweep (2026-07-17) found one referenced sibling project whose repo is private; its name appeared in the kit docs. Human ruled scrub-before-push: all tracked-file references use a neutral alias ("terrain sibling"), the unpushed history was rewritten so the name never reaches the public remote, and the alias→name map lives in PRIVATE-NOTES.md (untracked, gitignored). Standing rule: never write a private sibling's real name into a tracked file; public siblings (e.g. Tonality) are named directly. The verify leak gate covers paths/identity; this alias rule covers project names — both are push-blocking.

## ADR-015 · Daido multi-pole coupling verified; ADR-007 partially amended — ACCEPTED
*(Arrived 2026-07-17 as an external ADR drop numbered 010; renumbered here — this log was already at ADR-014. Claims independently re-verified headless on the v2 dynamics core before merging: (a),(d),(e) reproduced exactly, (c) reproduced to three decimals, (b) reproduced qualitatively.)*
Measured in the dynamics lab (mean-field path, `poles q` select, q-harmonic mean field sin(ψ_q − qθ − α), tick-held):
(a) **q-cluster formation**: q=2 → R₂ = 0.97 across seeds with R₁ low; q=3 → R₃ = 0.97; clusters lock hard.
(b) **Spontaneous symmetry breaking**: cluster population splits are seed-dependent (measured 8/16, 12/12, 8/16, 14/10, 11/13 at identical parameters) — demographics are preset character.
(c) **Split is a timbre dimension**: 2f0 energy constant (~0.080 projection) across seeds while residual f0 tracks cluster imbalance (12/12 → 0.006; max imbalance → up to 0.064), i.e. the split ratio is a fundamental-vs-octave mix knob the seed rolls.
(d) **Bistability**: identical parameters, aligned start → full sync persists (R₁ = 0.99); scattered start → clusters. Two coexisting attractors, confirming hysteresis potential.
(e) **ADR-007 amendment**: the claim that q-harmonic mean-field coupling cannot be tick-held was WRONG — ψ_q and qθ both advance at ~q·f, so their difference drifts at q·detune (slow); tick-holding locks cleanly at q ≤ 4. ADR-007's *other* rationale stands: q-coupling admits full sync as a coexisting solution (that is now (d), a feature), so assigned-slot splay remains the correct mechanism where a deterministic multiplication outcome is required. Daido coupling ships alongside it as the emergent sibling.
Meter note: under q-coupling, sync shows R₁ AND R_q high; clusters show R₁ low + R_q high (unlike SWARMSAW's R_N, where N=7 magnification keeps sync-side R_N low).
Spec placement: `poles q` joins Layer 2 (coupling); generalizes the drawn-coupling-function direction (Daido harmonics ⊂ arbitrary Γ(Δθ)).

## ADR-016 · Coupling × beat-grid interaction is physics; the UI must disclose it — ACCEPTED
*(Arrived in the same drop as external ADR-011; renumbered. Suppression direction re-verified headless — locking a populated grid suppresses grid-rate envelope power; the ~80× magnitude is the design session's measurement and is harness-dependent (a coarser independent method gave ~5× at detune 0.6): treat the direction as blocking, the magnitude as reference.)*
Measured: with the tempo-grid law active and populated (3–7 rungs, exact multiples verified), raising K to lock (R = 0.97) suppresses envelope power at the grid rate ~80× (6.0e-4 → 7.7e-6); same under Daido q=2. Locked voices share one frequency → no gaps → no beats. This is the intended "K loosens/overrides the grid" negotiation, not a defect — but it is invisible without instrumentation and was reported as a regression in testing. Requirement: any surface exposing the tempo-grid law MUST show live grid status (unit, rungs occupied, and a lock warning when coherence is high) so the populated-but-overridden state is legible. Reference implementation: `beatinfo` readout in swarmdynamics.html. Hierarchy note for presets: grid-forward patches want K subcritical; document K ≲ 1×σ as the compatible range. Warning condition as originally stated here is superseded by ADR-017.

## ADR-017 · Amendment to ADR-016: lock warnings require cause AND state — ACCEPTED
*(Arrived in the same drop as external ADR-012; renumbered, internal reference updated from "ADR-011" to ADR-016. Falsifier scenario re-verified: retrigger zeroes phases, so R = 1.00 at note-on with Ksm = 0.000 by construction; the corrected gate is implemented verbatim in swarmdynamics.html drawBeatInfo.)*
ADR-016 specified the grid-suppression warning condition as coherence alone (R or R_q > ~0.8). Falsified in testing: with K = 0 and retriggered phases, R reads 1.00 with Ksm = 0.000 — the swarm is coherent from initial conditions, not from coupling, and no suppression is occurring (nothing holds the voices; the grid reasserts as they disperse). Corrected condition, verified both directions: warning fires iff coupling is engaged (Ksm > ~0.05 Hz) AND coherence is high (R, R_q, or both cluster orders > 0.8). General principle for all such displays, here and in the VST: order parameters measure state, not cause — any UI claim about what the coupling is *doing* must gate on the coupling being nonzero. Reference implementation updated in swarmdynamics.html; UI additionally reserves fixed height for the warning line so its appearance cannot reflow the layout.

## ADR-018 · ADR-006 closed: oscillator bank renderer (iFFT recorded as escape hatch) — ACCEPTED (ratified 2026-07-17)
Spike measured 2026-07-17 on Apple M3, single thread, Release -O3, 8 s renders (tools/renderer_bench.cpp): at the target load (128 partials × 5 voices × 4 notes = 2560 sinusoids) the bank runs 66× realtime, vDSP iFFT 216×; at the prototype ceiling (480 osc) 403× vs 947×. Both have order-of-magnitude headroom even derated ~4× for min-spec. With throughput not the binding constraint, the decision falls to fidelity: (1) **parity** — L0-1 demands ε=1e-6 equivalence with the JS reference's per-sample phase accumulators, which the bank preserves structurally and an iFFT/windowed-OLA pipeline cannot; (2) **control-rate semantics** — the bank honors the 16-sample tick; iFFT quantizes control to the hop (~23 ms at 4096/1024), smearing fast K slews and per-tick coupling. Decision: oscillator bank (table + linear interp per SPEC §6.6). iFFT remains the recorded escape hatch if partial budgets grow past ~1k sines per note or min-spec measurements (E-6 hardware envelope) fall short; revisit trigger written into ROADMAP Phase 4. Caveats: M3 is not min-spec; benchmark excludes control-tick coupling cost (identical for both paths).

## ADR-019 · GUI stack: embedded webview (choc), reusing the prototype design language directly — ACCEPTED (ratified 2026-07-17, with amendment)
ADR-013 requires the GUI to match the HTML/canvas prototypes as closely as possible; a webview can run the prototype's canvas/JS/CSS nearly verbatim instead of reimplementing it in native drawing code. Research (2026-07-17, scout report in traces): choc::ui::WebView (Tracktion; actively maintained; WKWebView/WebView2/WebKitGTK; permissive license) is the de-facto C++ plugin webview — JUCE 8's own WebView UI feature is built on it; CLAP itself added a draft webview extension in 1.2.7; Cmajor's CLAP exporter defaults to webview UIs; a Rust webview+CLAP stack reports 15k+ production users. Known risks, each with a mitigation owner: (1) keyboard-focus stealing in Live/Logic on macOS (documented JUCE-forum pattern) → implement key-forwarding in the platform view from day one; (2) webview-through-clap-wrapper-to-VST3 unconfirmed in the wild (VSTGUI/ImGui confirmed; a webview's NSView/HWND should pass through identically but this is inference) → Phase 0/1 smoke test before GUI work starts; (3) multi-instance memory behavior → measure with 8 instances during Phase 2. Fallback recorded: iPlug2 IGraphics (CLAP-native since 2024) or Dear ImGui, at the cost of a full visual reimplementation. Consequence: GUI v1 (Phase 2, ADR-013) is built as HTML/canvas assets shared with — and diffable against — the prototypes.
**Ratification amendment (human, 2026-07-17): build for swappability.** The webview is an implementation behind a seam, not an assumption the codebase absorbs: (1) engine↔GUI communication goes through a transport-agnostic message/parameter layer (state snapshots out, gestures in — no webview types outside the GUI module); (2) the CLAP gui-extension implementation lives in one module that owns all platform-view and webview specifics; (3) visualization data (R/R_q/σ/phases for the meters) is produced by the engine as plain structs, so a native fallback (iPlug2 IGraphics / ImGui) consumes the same feed. Swapping GUI systems must never touch the DSP core or the plugin shell.

## ADR-020 · Param-surface may exceed prototype UI ranges; display units are host-facing — ACCEPTED
Human request (2026-07-18, post-GUI test): drift depth widened 0–25c → 0–100c and drift rate displayed in physical units. Principle: the REFERENCE pins the DSP mapping (a cents value is a cents value at any magnitude — parity and goldens unaffected), while the prototype's UI ranges are ergonomic defaults the plugin may exceed deliberately, recorded here when it does. Drift rate keeps its 0–1 knob domain (state/parity compatibility) but value_to_text and the GUI display the actual random-walk speed 0.2–8.2 /s (rate = 0.2 + knob·8, the controlTick mapping). Caveat recorded: widening a param's range rescales NORMALIZED automation in VST3 hosts — acceptable pre-release, never again after 1.0 without a migration shim. Also: GUI window is now resizable (720×440–1600×1000, default 920×600, responsive layout); ADR-019 risk #1 downgraded — no keyboard stealing observed in Live with sliders/pad; the state textbox is the remaining watch item (only text-enterable field).

## ADR-021 · ADSR envelope as a parity-safe superset of the reference AR — ACCEPTED
Human request (2026-07-18): traditional ADSR control. The reference has a fixed AR (~3 ms attack, ~160 ms release, per-swarm one-pole; SPEC §6.5) sitting in the parity-critical render loop, so the design constraint is: DEFAULTS MUST REPRODUCE THE REFERENCE BIT-EXACTLY. Implementation: attack/decay/sustain/release params (defaults 3 ms / 160 ms / 1.0 / 160 ms); when sustain >= 1 the envelope takes the reference's exact code path (same expressions, same operands — decay never engages), so L0-1 goldens are the regression proof that defaults are unchanged; sustain < 1 enters an attack→decay phase machine that is new, deliberately divergent behavior. Time params are expressed in seconds and converted per-rate (ADR-009 discipline). Param IDs 19–22 appended (never renumbered); state schema gains keys additively (old states load with reference defaults). The SAW prototype is NOT extended — this is a plugin-surface superset per the ADR-020 principle, recorded here because it touches the DSP core.

## ADR-022 · Tempo-grid law ported into the SAW core from the DYNAMICS reference — ACCEPTED
The grid law (ADR-005) lives in swarmdynamics.html, not the SAW reference, so adding law 3 to SwarmCore is a cross-reference port: the snapping expression is taken verbatim from the DYNAMICS beatQ path (u = bpm/60 · cyclesPerBeat; cents placement, then round(df/u)·u on the Hz offset), with L0-12 as its oracle (the SAW goldens cannot cover it; laws 0–2 remain proven untouched by L0-1 staying green). bpm is host-owned — CLAP transport tempo, read per block and from transport events, never a user param (fallback 120 without a host tempo); cycles-per-beat is param 23. Drift applies after the snap: turning drift up deliberately loosens the grid (the L0-12 protocol runs at drift 0). Placement protocol note: the DYNAMICS reference has no distribution selector (even spread implicit) — L0-12's rung/spread numbers reproduce at dist=even, M=16, and the harness pins that protocol.

## ADR-023 · One unified core proven against BOTH references — ACCEPTED
Phase 3 structural finding: ADR-001's "one engine" held to the expression level — DynSynth's unipolar K taper 4K²σ IS the SAW sync branch at K ≥ 0, and the two references share σ measurement, slews, RNG, envelope, and render paths verbatim. The dynamics layer (topologies mean-field/ring/two-cluster, Sakaguchi α via Math.PI-precision radians, Daido poles, consonance gravity on f0cur, bimodal placement tied to two-cluster) was ported expression-for-expression into SwarmCore such that (a) SAW defaults leave every SAW expression bit-untouched (topo 0/α 0/poles 1/grav 0; sin(ψ−θ−0.0) ≡ sin(ψ−θ)), proven by the SAW golden set staying 30/30 with the identical worst case; (b) a DYN config (dist even, lpOut 0 — the output pole is the ONE structural difference between references, exposed as a param) reproduces DynSynth, proven by a new 21-scenario DYN golden set (7 scenarios × 3 seeds; 9 bit-exact, worst 4.3e-9 on the chaotic ring). Splay exists only on the mean-field path (SAW has no topologies; DYN has no splay). L0-8..11 green with one protocol note (L0-11's snapshot values reproduce as a 2–8 s observation window — the locked/drifting ring regions wander; reference-probed) and one probe correction (a 350 Hz "outside basin" test note actually folds inside the basin; 240 Hz is outside). ADR-015 anchor criteria (R_q ≥ 0.95 at q∈{2,3} across seeds; aligned-start bistability) now enforced in trajectory_check; formal ACCEPTANCE text proposed at the Phase 3 gate.

## ADR-024 · Inertia knob taper at the surface; XY pad two-way binding — ACCEPTED
Human report (2026-07-18): inertia feels binary. Measured landscape (R tail-mean over K × w): at K ≥ 0.85 the response is smooth, but at near-critical K (0.6–0.7) the FIRST epsilon of inertia drops the marginal lock a regime (0.60→0.45 at w=0.02) and then w ∈ 0.02..0.3 is a dead plateau (ω0 = 2π(8(1−w)+0.6) stays far above the beat rates there). Two separate phenomena: the initial step is reference physics (SPEC §5.3 keeps the hunting deliberately — softening it would be a DSP divergence, offered to the human as a reference-update question); the dead plateau is a knob-mapping defect fixed here parity-safely: the CLAP layer maps core w = sqrt(knob) (heavy region spread across the knob), the knob value keeps ONE documented shell-side slot (sqrt/square round-trips are not bit-exact and state_check demands exactness), and both state-load paths route through applyParam so the taper applies identically. Core DSP and all goldens untouched. Also fixed: the XY pad was one-way (pad→knobs); setControl and the slider handlers now update the pad crosshair, and the GUI polls params ~2×/s (drag-guarded) so host automation and device-panel moves reflect back into the GUI.

## ADR-025 · Super-width: width param extends to 1.5 with mid/side boost — ACCEPTED
Human request (2026-07-18): wider. The pan law tops out at hard L/R, so beyond-reference width is a master-bus mid/side side-boost (up to 2× at 1.5) applied before the soft clip, reachable only at width > 1 — the reference range is bit-untouched (pan-law width contribution clamps at 1 to prevent anti-phase pan angles). Verified: side energy ~4× at 1.5 vs 1.0, bounded output. Range extension carries the usual pre-release normalized-automation caveat (ADR-020).

## ADR-026 · Voice mode: mono/legato/glide + octave transpose — ACCEPTED
Human request (2026-07-18). Core superset (parity 51/51 unchanged): retargetNote(slot, midi, f, legatoKeepPhase) reuses a sounding voice (legato keeps phases/envelope; non-legato re-strikes in place but glides from the current pitch), glide is an exponential f0 approach (tau = glide seconds, per-tick coefficient per ADR-009) that moves f0cur multiplicatively so gravity offsets survive; the poly path never arms it. Shell owns the mono note-routing: held-note stack (16 deep), last-note priority with return-to-held on release, mode-switch panic-clears. Octave (−2..+2) is a pure shell transpose at note-on (frequency only; host note identity untouched). Params 32–35 (24–30 stay reserved for the dynamics surface); param 15 renamed "Mono Fold" to disambiguate. NOTE_END in mono legato carries the latest note identity — earlier notes in a legato chain get no individual END (known edge, recorded; pluginval clean).

## ADR-027 · Live transposition suite; glide gated on held keys — ACCEPTED
Human clarifications (2026-07-18). (a) Octave/semitone/fine-cents/pitch-knob (params 35–38) combine into ONE multiplicative core tune factor applied at law evaluation — so the pitch knob bends SOUNDING notes (the note-on-time octave transpose from ADR-026 is removed in favor of this; one mechanism). tune = 1.0 is bit-inert (parity 51/51 unchanged); gravity ratios are tune-invariant (common factor cancels pairwise). (b) The "octave toggle" the human actually meant is a UNISON octave layer — parked as a design-session question (PARKED #14: coupling-graph membership and σ-normalization under a 2:1 split are real physics questions; reference-first like clustered-pairs). (c) Mono glide/legato now engage ONLY when another key is physically held; a ringing release tail alone gets a fresh strike on a new slot, overlapping the tail naturally (the previous voice-live test conflated tail-ringing with held).

## ADR-028 · Dynamics surface exposed (params 24–31, meters, readouts); absolute-K semantics — ACCEPTED
Phase 3 increment 2. Params: Topology/Ring Reach/Cluster Link μ/Phase Lag α/Poles q/Gravity/Basin/Absolute-K (ids 24–31; labels for enums; α in degrees). GUI Dynamics cluster; meters gain the dyn readouts (R_q under poles, A/B cluster orders under two-cluster); gravity readout shows captured pairs as ratio-name + octave fold + live cents (the ADR-008 requirement); ADR-016/017 grid-status line implemented verbatim (unit, occupied rungs, cause-AND-state lock warning — KsmS > 0.05 AND coherence > 0.8 — on a fixed-height line). Absolute-K (ADR-004) semantics chosen: σ replaced by 1.0 Hz in the coupling targets (taper preserved) — the minimal bypass; guarded, default-inert, verified (identical oscillators lock from scatter, R = 1.0). En route, state_check went RED and caught a real bug: readParam's hand-maintained key chain lacked the new params (get_value → 0; state saved lies; the value-round-trip check passed because both sides lied identically — only the restored-audio-bit-identity requirement caught it). Fixed by construction: core.getParam reads through the SAME paramSlot map setParam uses; the shell's parallel chain is deleted (L0005).


*(ADR-029..032 arrived 2026-07-18 in design-session packet UPDATE-001, numbered 013-016 there; renumbered per the standing collision policy (ADR-012/L0001). The packet's 010-012 matched local 015-017 verbatim and were skipped. Internal references remapped.)*

## ADR-029 · Filter family: parameter-domain coupling; home spring exposed; allpass topology rejected for the notch swarm — ACCEPTED
The swarm force system applies to filter center frequencies at control rate (SPEC-FILTER.md): two engines (resonator bank, notch swarm) on one shared core, accepting external audio in the VST (effect line). Three bound decisions:
(a) **Home spring is a musical parameter.** Competing springs settle at err_final = h/(h+g)·err_initial — verified predictive in both engines (bank gravity 28.8¢→6.9¢ at h=1.5/g=4.8; notch capture residuals 7–10¢). The prototype's h=1.5/s is a compromise, not a constant; expose it, and park a capture-tightening mode (adaptive h release inside basins) for sub-cent snap.
(b) **Allpass-cascade phaser topology REJECTED by measurement** for the notch swarm: cascade notches form at cumulative-phase odd-π points, not stage centers — the swarm moved stage frequencies while actual nulls sat elsewhere (pseudo-depths scattering ±10 dB, some negative). True SVF notch sections (y = x − k·v1) null exactly at fc (~150 dB measured), which the engine's premise requires. Feedback retained (tanh-bounded); classic-phaser phase character traded for exactness.
(c) **State-domain filter coupling stays parked** behind the Stuart–Landau bridge ADR; this family is deliberately control-rate-only.

## ADR-030 · Time-domain family (tap swarm + FDN room swarm); FDN eigenstructure finding — ACCEPTED
The shared force core applies to delay times (SPEC-FILTER family, log2-seconds coordinate). Reference: swarmtime.html (TimeLab, echo/room modes). Verified: tap gather σ 0.328→0.066 and grid CV 0.720→0.055; rhythmic gravity onto {¼,⅓,½,⅔,¾,1,1½,2,3,4}×beat, in-basin 34.9→8.3¢; impulse echoes confirmed at all tap times; room→note collapse yields a 16–19 dB comb at k/L; sympathetic gravity (lengths → multiples of note period) 34.3→7.4¢ on captured lines; stable at regen 0.97 with drift+K.
(a) **FDN feedback matrix sign is load-bearing.** Householder I−(2/N)J gives the all-ones vector eigenvalue −1; injecting input along ones then resonates at odd/(2L) — the initial build probed 1/L and measured an anti-resonance (−1.4 dB). Negated Householder puts +1 on the ones channel → comb at k/L, which the room→note and sympathetic-tuning semantics require. Any future matrix/topology work must state which eigenchannel the input excites.
(b) **Equilibrium law err_final = h/(h+g)·err_0 now confirmed three-for-three to the decimal** (bank 6.9¢, echo 8.3¢, room 7.4¢). Promote from finding to design law; acceptance criteria may cite it directly.
(c) **Sympathetic-room capture is geometry-limited**: multiples of the note period are 100–390¢ apart for short lines, so fixed cents-basins capture few lines (3 of 8 at ±80¢). Park: adaptive basin (fraction of local inter-target spacing) and/or longer-line placement defaults for sympathetic presets; f0 reinforcement scales with capture count (measured +3.3 dB relative at 3/8).
(d) Time-domain drift = wow/flutter; moving taps chirp (Doppler) — continuous-interp "tape mode" is the prototype default; discrete between-repeat motion parked as a mode switch.

## ADR-031 · Effects consolidation; two stability laws from measured failures — ACCEPTED
SPEC-EFFECTS.md supersedes SPEC-FILTER.md, adding the time-domain family (tap swarm delay, FDN room swarm; ADR-030 findings incorporated). Two stability laws promoted from bug post-mortems to design law:
(a) **Feedback normalization assumes worst-case correlation.** N delay taps reading shared low-frequency content sum with gain N; a √N feedback norm gave the echo loop LF gain regen·√N — unstable above regen ≈ 0.35, i.e., at defaults. Measured runaway: buffer mean 0.04→0.42 over 10 s of a held note, followed by tanh-saturation squashing the audio to near-silence with slow recovery — the field-reported "plays unreliably / too quiet / buffer takes long to clear" symptom, diagnosed correctly by the user from the phenomenology. Fix: feedback paths /N, output paths /√N, verified flat over 12 s at regen up to 0.97.
(b) **DC-block inside every feedback loop; corner ≤ lowest-musical-comb/6.** The room's +1 eigenchannel passes DC at loop gain ≈ regen. A 15 Hz in-loop blocker fixed DC but compounded per pass and gutted a 26 Hz comb (16.3→2.6 dB); at 4 Hz the comb holds 13.5 dB and line DC stays ≤ 0.006. In-loop filters compound: their passband edges must clear musical content by a wide margin.
Both laws are Layer-0-guarded (L0-19/L0-21) so neither failure mode can silently return in the C++ port.

## ADR-032 · Focus-trap silence; interaction-layer failure modes are real failure modes — ACCEPTED
Field report: "suddenly no sound." DSP exonerated by 60 s randomized parameter fuzz (no NaN, RMS floor 0.04). Root cause was the interaction layer: sliders retain keyboard focus after use, and the note-key handler correctly ignores keys targeted at INPUT elements (so typing in seed/bpm fields doesn't trigger notes) — therefore adjusting any knob silently killed the entire musical keyboard until an incidental click elsewhere. Present in all six labs; never caught because on-screen key clicks incidentally restore focus.
Fixes (all labs): range inputs blur on pointerup; selects blur on change; Escape = panic (blur + all notes off). Belt-and-suspenders: NaN watchdogs added to both feedback engines (TimeLab, PhaserLab) — non-finite feedback state self-heals within one block instead of rendering permanent silence.
Laws for the VST: (a) any "keyboard input guarded by focus" design must actively release focus from momentary controls, and the guard's scope must be per-control-type, not blanket; (b) every feedback engine ships a finiteness watchdog — a NaN must never be able to silence the instrument for longer than one processing block; (c) diagnostic order is preserved as method: exonerate the DSP by fuzz before touching the UI, so fixes land in the layer that actually failed.

## ADR-033 · Absolute-K unit raised to 2.5 Hz; phase-scatter percentage — ACCEPTED
Human requests (2026-07-18). (a) Absolute-K's unit was my ADR-028 "minimal bypass" placeholder (1 Hz → max pull 4 Hz — measurably feeble next to σ-normalized patches at ~13 Hz). No reference constrains this mode, so the unit rises to 2.5 Hz (max pull 10 Hz at knob 1); existing absK patches get proportionally stronger (pre-release, acceptable). (b) Phase scatter (param 39): partial-random phase init generalizing the retrigger toggle — phase_i = rng·scatter when scatter > 0; at 0 the LEGACY retrig path runs bit-exactly (rng untouched → all goldens stand); at 1.0 the retrig-off stream is reproduced by value (rng·1.0). The retrigger checkbox dims when overridden. Purpose: everything between "tight aligned attack" and "full cloud" becomes a knob.

## ADR-034 · E0 unification ruling: one force core for the effects line; SwarmCore shares only what is genuinely shared — ACCEPTED
The Track E packet's sequencing note assumed the dynamics engine would carry its own position-force implementation to unify with. Inspection says otherwise: SwarmCore's dynamics are PHASE-domain Kuramoto (sin coupling on phases; frequency offsets come from static placement laws), while the effect labs run a POSITION-domain spring system (home/sync/splay/gravity/drift/inertia on log2 coordinates). These are mathematically distinct systems, both reference-frozen — rewriting either side's constants to "unify" them would be a parity-breaking spec change wearing a refactor's clothes.
What unification honestly means here, and what shipped as src/force_core.h:
(a) ONE shared module implements the labs' force system verbatim (they embed one identical force expression; per-engine deltas are frozen constants captured as a Profile — clamps, span floor, inertia ω₀ — plus the attractor set: harmonics n·f0, rhythmic RSET×beat, period multiples). All four E1–E3 engines will consume this module; the four-copies future the packet warned about is dead.
(b) SwarmCore adopts from it the ONE piece of arithmetic the two families genuinely share — mulberry32 — by delegation (parity 51/51 proves bit-neutrality). The drift walks remain separate on purpose: the labs bake depth into the walk unclamped, SwarmCore clamps to ±1 and scales at application — same family, different frozen constants, both reference-exact.
(c) Correctness contract: force_check reproduces the labs' population trajectories seed-for-seed (Node-extracted cores → checkpoint goldens; observed divergence ≤ 1.3e-14 log2 units, tolerance 1e-9) and enforces the population halves of L0-14/15/17/19/20. Protocol finding (L0002 class): the recorded acceptance numbers reproduce ONLY with drift off — TimeLab defaults drift to 30¢, but σ 0.755 / CV 0.009·0.055 / equilibrium ratios 0.238·0.217 all land exactly at driftDepth 0, so the reference measurements encoded a drift-off protocol nobody wrote down. Goldens pin it.

## ADR-035 · Output stage: bass mono (M/S elliptic) + pan scatter — ACCEPTED
Human requests (2026-07-18). (a) **Bass mono** (params 40/41): one 2nd-order Butterworth TPT-SVF high-pass on the SIDE channel — L = M + HP(S), R = M − HP(S). Lows collapse to mid with no crossover phase mismatch (the classic vinyl-elliptic routing; "responsible" = phase-coherent and mono-compatible by construction). Shell post-processing, runs before the spectrum feed so the visualizer shows what leaves the plugin. Measured: side −24 dB @ 30 Hz, −3 dB at corner, 0 dB @ 1 kHz (fc 120). (b) **Pan scatter** (param 42, core): the human's diagnosis was right — pan position is monotone in the detune offset (pan = x[i]·width), so spatial order == frequency order and detune sweeps march across the field in series. The param blends each voice's pan toward a SEEDED PERMUTATION of the legacy positions (Fisher–Yates on an independent stream derived from the seed — the phase/drift stream never moves differently). 0 = bit-exact legacy (parity 51/51 is the proof); 1 = fully scrambled order; between = partial decorrelation. Slider preserved per the request so series panning stays available as a choice.

## ADR-036 · MPE per-note pitch via CLAP note expressions — ACCEPTED
Human request (2026-07-18, "loop in MPE pitch bend sooner rather than later"). CLAP's NOTE_EXPRESSION/TUNING stream (relative semitones, wildcard note matching) maps to a per-swarm noteTune factor applied at the ADR-027 live-tune seam: f0c = f0cur · tune · noteTune. At 1.0 the multiply is bit-inert (IEEE x·1.0 == x — the same guarantee ADR-027 leans on; parity 51/51 green). Fresh strikes reset the factor; mono/legato retargets keep it (MPE bend streams continue across retargets). The note-ports extension already preferred CLAP_NOTE_DIALECT_CLAP, so hosts with MPE (Live via the wrapper) deliver expressions with no further surface. Full per-note MPE (pressure→K, slide→detune) stays Phase 5 with the mod matrix — this ships the pitch dimension early because it needs zero new routing concepts.

## ADR-037 · SPECTRA ships as its own parity-frozen core; the SPEC §2 "one engine at P=1" gate needs a ruling — ACCEPTED (a), 2026-07-18
Phase 4 reality check: SPEC §2 declares one engine ("per-partial swarm over a kernel"; SAW = P=1 + saw kernel), and the Phase 4 gate reads "SAW provably = SPECTRA at P=1". But the two REFERENCES are separate frozen programs whose arithmetic genuinely differs — sigma floor 0.05 vs 0.08, K-slew 0.08/(1+cascade·120) vs 0.08, fixed AR (4 ms/180 ms) vs ADSR, phase update max(0,f)/sr vs the SAW expression, sine-table kernel vs saw, gain norm /(ampSum·M^0.75), tanh on both channels. A literally shared voice path cannot be parity-exact with both references at once. Shipped now (unambiguous under any ruling): src/spectra_core.h as a verbatim port — parity RMS 0.0 on 9/9 scenarios, L0-6 zipper (monotone front; 7.21 s at cascade 1, 1.81 s at 0.5) and L0-7 interference gate (−15.06 dB, wmix engaged) green, wired into ./verify full via spectra_check. OPEN QUESTION for the human (gate interpretation): (a) reinterpret the P=1 gate as a MEASURED-equivalence check (e.g. locked-state spectra match within a stated tolerance) honoring both frozen references, or (b) amend SPEC to bless one unified voice path and re-measure one side's acceptance numbers (a spec change on protected docs). Shell integration (engine-select param, GUI mode gates, spectra viz) proceeds after the ruling — the core and its oracle don't depend on it.
RULING (human, 2026-07-18): option (a) now — the P=1 gate is a MEASURED equivalence check, implemented in spectra_check as a tick-for-tick R-trajectory comparison (at P=1 the two references' dynamics expressions coincide: same coupling/slew/Kenv forms, same rng stream formula, aligned initial phases — the kernel is the only difference, which is exactly SPEC §2's claim, proven at the dynamics level). FOLLOW-UP: attempt a shared voice path behind a build/runtime switch so the human can A/B the sound against the frozen cores; adoption only on that listening test.

## ADR-038 · MPE reaches the wrapped formats: VST3 tuning advertisement + MIDI-dialect member-channel bend — ACCEPTED
Field report (2026-07-18, post-ADR-036 merge): "the MPE bend isn't working yet" in Live (VST3). ADR-036's closing claim — "hosts with MPE (Live via the wrapper) deliver expressions with no further surface" — was wrong; this ADR corrects it. Investigation of the pinned clap-wrapper (v0.15.1):
(a) **The VST3 wrapper never advertised TUNING.** Its INoteExpressionController exposes only the expressions in `_expressionmap`, which defaults to PRESSURE-only unless the `CLAP_SUPPORTS_ALL_NOTE_EXPRESSIONS` compile flag is set (wrapasvst3.h:474) — a flag `make_clapfirst_plugins` never forwards (make_clapfirst.cmake:171 passes no `SUPPORTS_ALL_NOTE_EXPRESSIONS`), so it is unreachable from our build. A host that speaks VST3 note expressions therefore never sends tuning. The supported fix is the wrapper's own extension seam: the plugin now answers `CLAP_PLUGIN_AS_VST3` (clapwrapper/vst3.h) with `supportedNoteExpressions = TUNING|PRESSURE` (PRESSURE kept = wrapper default preserved) and `getNumMIDIChannels = 16`; the wrapper picks this up in `setupWrapperSpecifics` before building the advertisement and the process adapter. Incoming VST3 kTuningTypeID events were already converted correctly ((v−0.5)·240 semis, note-id matched — process.cpp:660) — only the advertisement was missing.
(b) **MPE can also arrive as rotating member channels + per-channel 0xE0, not note expressions.** The VST3 wrapper maps per-channel pitch-bend parameters (IMidiMapping, 16 channels) to `CLAP_EVENT_MIDI` 0xE0 events (vst3/process.cpp:321), and the AUv2 wrapper forwards raw 0xE0 the same way (auv2/process.cpp:551) — Logic's MPE path. The shell now handles `CLAP_EVENT_MIDI`: bend on member channels 2–16 (indices 1–15) at the MPE default ±48 st is latched per channel and applied through the same ADR-036 `setNoteExpr` seam to swarms whose VoiceTag channel matches; the latch is re-applied on note-on because MPE hosts send bend BEFORE the note it modifies and a fresh strike resets noteTune. Channel 1 (index 0 — MPE manager / plain single-channel MIDI) is deliberately excluded: ±48 applied to an ordinary ±2 bend wheel would be wildly wrong, and global bend already has param 38. Non-MPE hosts (everything on channel 1) therefore see zero behavior change.
Residuals, explicitly parked: MPE zone configuration (upper zone, RPN 0 bend-range negotiation) is not parsed — ±48 member default only; an "MPE bend range" param can join the Phase 5 MPE surface; manager-channel bend → param 38 mapping is untouched (bend wheel in non-MPE hosts still a GUI-only affair). Parity/state contract: noteTune stays 1.0 unless events arrive (latches init 0, `plug_reset` clears), so parity-frozen defaults are bit-inert — ./verify full green post-change (parity 51/51, state_check GREEN). CLAP-native hosts (Bitwig) keep the ADR-036 NOTE_EXPRESSION path unchanged. Human verification in Live with an MPE controller is the closing gate.

## ADR-039 · MIDI 1.0 vel-0 note-on IS a note-off; note-lifecycle fuzz oracle — ACCEPTED
*(Authored concurrently with ADR-038 in a parallel session; both claimed 038 — renumbered here at merge triage per the ADR-015 renumbering policy.)*
Field report (2026-07-18): "envelope doesn't always stop when you let go even when release is all the way down." Diagnosis by layered exoneration (the ADR-032 method): (1) primary hypothesis — that the ADR-021 sustain>=1 reference path ignored p.releaseS — REFUTED by reading (render() builds atk/rel from p.attackS/p.releaseS every call; the ADR-021 comment means defaults *coincide* with the reference constants, not that they are hardcoded) and by measurement (headless tail at release=0.005 s is 40 ms and scales with the knob; the 9.21τ truncation is the render loop's 1e-4 skip floor, by design). (2) Shell note routing (poly, mono held-stack, legato) exonerated by 120 seeded causal random note-stream scenarios through the real CLAP process() — all silent after all-keys-up. (3) The defect: the AUv2 clap-wrapper forwards a controller's 0x90-velocity-0 note-off verbatim as CLAP_EVENT_NOTE_ON velocity 0 (no MIDI 1.0 vel-0 remap upstream), and handleEvent ignored velocity — so a vel-0 release struck a FRESH full-gain voice that no note-off ever ends. Reproduced headlessly: 50/50 scenarios hang at sustain level (poly stacks hung voices, peak > 1.0). Controller-dependent, hence "doesn't always."
Decision: the shell remaps NOTE_ON velocity <= 0 to the shared note-off path (handleNoteOff — same mono/held-stack routing). Standard MIDI 1.0 semantics; this synth is velocity-insensitive, so no legitimate host intent is lost. Core untouched; parity 51/51 green (worst 4.262e-09). New oracle: tools/notefuzz_check (poly/mono/legato x normal/vel-0 releases, release at knob min, silence required 0.5 s after all-keys-up; timestamps drawn sorted per block — an unsorted generator emits acausal OFF-before-its-ON orders no host can produce and fakes hangs). NOT yet wired into ./verify (protected file — human gate): proposed addition to the full() chain. Residual noted for a follow-up: voice stealing and mono retarget overwrite tags[] without emitting NOTE_END for the displaced note (host-side bookkeeping leak; the wrapper's _activeNotes grows).


## ADR-040 · SPECTRA partial ceiling raised 24 → 32 — ACCEPTED
Human request (2026-07-18). The SpectraSynth reference caps partials at P_MAX=24; this raises kPMax to 32 — a RANGE EXTENSION in the same family as the ADR-020 drift-depth widening, not a change to the reference arithmetic. The port's amp tilt (1/k^t), gain normalization (/(ampSum·M^0.75)) and stretch law all consume P unchanged; the spectra goldens render at partials ≤ 12 so parity RMS 0.0 stands as the regression proof. VizSnapshot's per-partial arrays and the strip visualizer grew to 32 to match. Worst-case CPU 8 voices × 32 × 7 cloud = 1792 sine oscillators — inside the ADR-006 spike headroom. Param 44 max is now 32 (append-only id unchanged).

## ADR-041 · Post-Track-E1, the parity-to-prototype contract graduates to forward performance standards — ACCEPTED (scheduled)
Human direction (2026-07-18): "the prototypes were never the final intended form, just gesturing toward it. Once we're past the Track E1 integrations we can start making modifications to the reference path… come up with a new set of performance standards and run against those instead." Recorded as a scheduled transition, not immediate. UNTIL the E1 gate closes, the standing discipline holds unchanged: reference paths stay bit-frozen, additions use the superset-with-inert-defaults pattern (parity is the regression proof), and any prototype divergence needs an ADR. AT the E1 gate, a dedicated phase (ROADMAP "Phase F — reference-path liberation") opens: (a) author the successor acceptance standard — behavioral/perceptual targets and new golden references generated from the LIBERATED implementation, versioned, replacing the "== prototype" definition of correct; (b) migrate the L0 suites from parity-to-prototype onto the new references, one engine at a time, each migration its own ADR + gate; (c) retire or re-scope the protected prototype HTMLs (they become historical provenance, no longer the oracle). The bit-parity harness is NOT deleted — it is repointed. Nothing about this ADR changes today's gates; it fixes the sequencing so the graduation is deliberate, measured, and reversible, never a silent drift off the contract that has protected correctness so far.

## ADR-042 · SPECTRA per-voice uncoupled sub-oscillator — ACCEPTED (first beyond-prototype feature)
Human request (2026-07-18), and the first DELIBERATELY-ORIGINAL addition — no prototype implements it. It ships now (not deferred to Phase F) because it is a pure ADDITION under the superset-with-inert-defaults discipline, not a reference-path modification: at subOn=0 OR subVol=0 the render's inner loop is byte-identical to the ported reference (the partial sum l·g is held in a double, then added, exactly as before), so the spectra goldens hold at rms 0.0 — parity is the regression proof, same guarantee ADR-021/025/033/035 lean on. Design (human-specified): PER-VOICE (one sub per SPECTRA swarm), one octave down (f0/2), UNCOUPLED (its own phase accumulator, outside the Kuramoto graph), following the note envelope so it starts/stops with the voice, mono (centered — a sub belongs in the middle). Three params (SPECTRA-only, ids 52-54): subOn toggle, subVol (0..1, default 0), subWave — a smooth morph sine → triangle → smooth-square (tanh-rounded, gentler than a hard clip). Level constant subGain = subVol·0.6 (taste, sits under the output tanh). Verified: with A3 held, sub-off has 1.2 units at 110 Hz (negligible), sine-sub 6066, square-sub 7183 (plus harmonics) — the intended sub-octave. state_check GREEN proves the params round-trip and restored audio is bit-identical. This ADR also OPERATIONALIZES the ADR-041 framing: "beyond the prototype" additions are already routine and safe; only reference-path MODIFICATIONS wait for Phase F. The octave-locked monophonic sub the human mentioned as a later idea is parked (#17). **Addendum 2026-07-19:** a sub-octave selector (param 55, stepped -1/-2/-3; default -1 = f0/2 bit-exact) was added — subOct feeds subFreqMul = 2^subOct; measured -1→110 Hz, -2→55 Hz at A3.

## ADR-043 · Track E1 increment 1: resonator bank on the shared force core — ACCEPTED
First audio-processing effect engine (SPEC-EFFECTS §3). src/filter_core.h ports swarmfilter.html's FilterLab VERBATIM, consuming forcecore (ADR-034) for the population dynamics (filter frequencies under home/sync/splay/gravity) and adding only the filter-specific audio: polyBLEP saw exciter (EX_DET unison), N-band TPT SVF bandpass bank, collapse→Q routing (Qeff widens with population collapse), wet/dry, tanh. Correctness contract: L0-1-style audio parity vs the JS FilterLab — filter_check reproduces 11 scenarios at RMS 0.0 (bit-exact, first build; the E0 force-core parity was the load-bearing foundation), plus the audio half of L0-14 (collapse→Q lifts effective Q from base to 30.4 under K=+1). Wired into ./verify full (verify edit rides the PR — human gate). PROTOCOL: every parity scenario pins noise=0 — FilterLab's exciter mixes Math.random() dither when noise>0, which is non-deterministic; the noise mix is ported (runtime feature intact, seeded reproducibly) but the ORACLE avoids it. SCOPE (increment 1 of E1): this is the resonator bank engine + oracle only. Remaining E1: notch swarm (swarmphaser, L0-16/17), external-audio input path in the shell, and the effect-plugin shell target (E3 territory) — each its own increment. The population halves of L0-14/15/17 stay covered by force_check; filter_check owns the audio path.

## ADR-044 · Prior-art posture: Lem/Kuroscillator lineage acknowledged; novelty claims corrected — ACCEPTED
User-sourced (2026-07-19; triaged from PRIOR-ART-DELTA_1.md, now deleted). Lem & Orlarey (CMMR 2019) plus Lem's program (SMC 2019, SMC 2022, CCRMA PhD 2022) — with Collins (ICMC 2008) and Lambert (ICMC 2012) antecedents — establish RESEARCH prior art for Kuramoto audio synthesis with the mean-field O(N) reduction, coupled sine banks, and coupled trigger clocks for rhythm. Correction recorded: any "emergent trigger synchronization is unoccupied" framing is wrong at the research level — unoccupied as a PRODUCT only. PRIOR-ART.md §6 now carries the citation set, the precise novelty posture (mechanisms + shipping instrument architecture, not the base idea), and a defensive-publication recommendation to cite the lineage rather than claim an unqualified first.
TRIAGE DISCERNMENT (per explicit human instruction to be discerning): the delta's speculative items — an Arnold-tongue / Arnold-layer modulation surface, a sample-onset gravity attractor, and a built grain engine — were EXCLUDED from PRIOR-ART as not-in-project (zero references in SPEC/ROADMAP/DECISIONS; the Arnold surface is an undecided exploration from a parallel agent's experiments). PRIOR-ART must never claim features that do not ship. Mirollo–Strogatz pulse coupling parked (PARKED #18) as the candidate grain-trigger substrate if that engine is ever built.
NUMBERING: the packet proposed this as "ADR-018," which collides with the existing bank-renderer ADR-018 (2026-07-17). Renumbered to 044 per the external-packet renumbering policy (cf. ADR-015, renumbered from packet 010).

## ADR-045 · Coupling function (Γ) and topology (W) as first-class force-core objects — PROPOSED
*(Triaged 2026-07-19 from RECOMMENDATION-coupling-topology-axes.md, archived at docs/design/2026-07-19-coupling-topology-axes.md, source deleted. Status PROPOSED — captured in the log; IMPLEMENTATION is gated on human ratification + critic review, since the lift touches the force-core seam. Two ADR cross-references in the source packet were WRONG and are corrected here per the discernment discipline: the FDN mixing matrix is ADR-030 (not "ADR-014", which is the private-sibling alias rule), and the Daido validation is ADR-015 (not "ADR-010", which is the spin-up ratifications).)*

**Context.** Sakaguchi α, Daido poles (ADR-015), and the three topologies are scattered coordinates in one 2-D design space (Γ × W) but are currently hardcoded as inline branches in the dynamics path of the unified core (ADR-023). The FDN mixing matrix (ADR-030) and the parked drawn-distribution idea (PARKED #1, a *distribution*-object precedent) show the same "pluggable pure object" pattern. The parked list is growing along both axes with no shared seam to attach to.

**Decision (proposed).** Introduce two pure, seed-deterministic, parity-tested objects consumed by the force-core, peer to the existing distribution/law objects:
- **`CouplingFunction` (Γ)** as a Fourier coefficient vector {(K_n, φ_n)}, n=1..H (H=4 covers everything shipped). Mean-field evaluation via generalized order parameters Z_n = ⟨e^{inθ}⟩ (the core already computes Z_1 = R and, on the dynamics path, Z_q = R_q). Sakaguchi = {(K_1, α)}; Daido-q = {(K_q, 0)}; sync+cluster mixes {(K_1,0),(K_2,0)} are expressible and yield bistable landscapes — new behavior falling out of an object built anyway.
- **`Topology` (W)** selecting neighbor/weight structure; present set mean_field | ring(reach) | two_cluster(μ), with the parked zoo (small-world, scale-free, modular, spatial, adaptive) as future enum additions consuming one interface. Unifies with the FDN mixing-matrix constructor family (ADR-030's "which eigenchannel does the input excite" is a `Topology` property).

**Scope guard.** This is a *representation* change, not a behavior change. The lift must preserve bit-parity against the current dynamics reference — {mean_field, ring, two_cluster} × {sin, Sakaguchi-α, Daido-q} through the new objects must reproduce their existing L0 numbers exactly (ε=1e-6), added as an explicit Layer-0 guard so the abstraction can never silently alter shipped behavior. No new Γ or W behavior ships under this ADR. New points on either axis are separate ADRs, prototype-first per ADR-003, each with its own L0 rows.

**Consequences.** Converts the coupling/topology parked entries from per-feature builds into single-object additions. Blocks nothing in flight. Earned-next payoffs it enables (NOT committed here): drawn-Γ (a user-drawn curve whose Fourier transform IS the CouplingFunction vector — the phase carpet already visualizes the result) and spatial/distance-dependent W (topology as audible stereo space). Both prototype-first when proposed.

**Assessment (triage).** Technically sound and idiomatic to the repo — the Γ×W factorization is the same move that made distribution × detune-law orthogonal, applied to coupling; the parity-guard-on-a-representation-lift is exactly the right discipline. Recommended for ratification. Sequencing note: the source pitched this "as Phase 3 finishes"; Phase 3 is now CLOSED and the code is stable, so the lift is a clean standalone refactor whenever prioritized — it does not gate E1 or Phase F.

## ADR-046 · Track E1 increment 2: notch swarm on the shared force core — ACCEPTED
Second frequency effect engine (SPEC-EFFECTS §4). src/notch_core.h ports swarmphaser.html's PhaserLab VERBATIM, consuming forcecore (ADR-034) for the population (notch frequencies) and the same polyBLEP exciter as filter_core; the audio path is a feedback allpass→TRUE-NOTCH cascade (per-section SVF y = x − k·v1, exact null at fc — the allpass-cumulative-phase topology was rejected by measurement, ADR-029b) with a tanh'd feedback loop and a NaN watchdog (ADR-032). Constant Q (1/stageQ), not collapse→Q. Correctness: L0-1 audio parity vs the JS PhaserLab — notch_check reproduces 11 scenarios at RMS 0.0 (bit-exact, first build), plus the audio half of L0-16 (on-notch attenuation 158 dB deeper than a quarter-octave-off probe — matching the reference's ~150 dB nulls, the regression guard against the allpass defect). Wired into ./verify full (now SEVEN oracle chains). noise=0 parity protocol (Math.random dither). Parity setup note: PhaserLab runs a construction-time controlTick that notch_core omits — harmless because the generator applies 'seed' last, whose rebuild wipes that step in the reference too, so pre-render state matches. SCOPE (increment 2 of E1): the two frequency engines (resonator bank ADR-043, notch swarm) now exist as parity-proven cores. Remaining E1: external-audio input + an effect-plugin shell so they are auditionable (the "testing them out" step); then L0-18 family-stability long-runs.

## ADR-047 · SWARM-FX: the effects line as a standalone audio-effect plugin — ACCEPTED
Track E1 increment 3 (the "testing them out" step, human-approved 2026-07-19). The swarm effect engines need external audio to be auditioned; the instrument can't take audio input. Decision: a SEPARATE plugin target, SWARM-FX (CLAP/VST3/AUv2, AUv2 type aufx), with its own factory/entry (src/swarmfx_clap.cpp, swarmfx_entry.*) so the instrument shell is byte-untouched. It shares the parity-proven cores headers-only (filter_core.h, notch_core.h) via a new processExternal() path added to each — same controlTick + SVF bank/cascade as the reference render(), but the dry signal is the plugin input (mono-summed) instead of the exciter; render() stays the parity-frozen path (all 7 oracle chains still green). One unified 16-param surface (engine select Bank/Notch + shared force params + per-engine Q/feedback/mix) maps onto both cores; a MIDI note-in moves the gravity center (setNoteFreq, no exciter voice). RATIONALE for engine-select-in-one-plugin (vs one plugin per effect): the human framed these as experimental FX that may not survive — one shell hosts all of them, so adding an engine is a core + a dropdown entry and discarding one is a deletion. Deliberately minimal first cut: generic host-drawn params (no webview yet), block-start param application (sample-accurate scheduling deferred). Validated: pluginval strictness 10 SUCCESS, auval SUCCEEDED (aufx), installed; headless check confirms both engines process external audio (finite, non-silent, spectrally shaped). SCOPE: this is the effect-plugin SHELL + external-audio path. Remaining E1: L0-17 audio (tuned harmonic rejection) + L0-18 family-stability long-runs as oracle rows; a webview GUI for SWARM-FX; then E2 time engines slot into the same shell. E3 ("effects as sections inside the instrument") is a later, separate integration.

## ADR-048 · Swarmalator engine ingested + ported (experimental) — ACCEPTED
Human drop (2026-07-19): SPEC-SWARMALATOR.md + swarmalator.html, "a new engine to test out." The swarmalator couples audio phase θ and spatial angle ξ TO EACH OTHER (ring swarmalator, O'Keeffe-Hong-Strogatz 2017), so pan is a state variable of the same system that makes the tone — the stereo/spectral unification, on-thesis. Mean-field-reducible: two compound order parameters W± = <e^{i(ξ±θ)}> (two extra complex accumulators per tick, O(N)); K = ordinary Kuramoto sync (phase axis == SAW at J=0), J = the cross coupling. Ingested as spec-in-code per ADR-003; swarmalator.html + SPEC-SWARMALATOR.md added to protected paths (7th prototype). Ported src/swarmalator_core.h verbatim (stereo out — pan from ξ — unlike the mono effect cores; shares mulberry32 via forcecore per ADR-034; K-norm reuses ADR-004). Oracle: gen_swarmalator_goldens.mjs + swarmalator_check — stereo L0-1 parity RMS 0.0 on 9/9 scenarios (first build), plus the SPEC §5 acceptance anchors (sync R=0.966 vs spec 0.97; splay R=0.056; rainbow max(R±)=0.515 with R incoherent; sync+rainbow both raised; stability peak 0.927 NaN-clean). Wired into ./verify full (eight oracle chains). TRIAGE (L0009): the packet's ADR cross-refs were wrong and corrected on ingest — Daido tick-holding is ADR-015 (packet said "ADR-010"); the ingest/design ADR is this one, 048 (packet said "ADR-046", the notch swarm here); prior-art posture is ADR-044 (packet said "ADR-018"). STATUS: EXPERIMENTAL — the human is testing the effects line and this in parallel; may not survive, may be joined by other new engines. Under ADR-045 (PROPOSED) it is a point in the (Γ, W) space (spatial ring topology × two-term coupling), and it is the parked grain-swarm's spatial dynamics as a special case (J on position). NEXT: shell integration for auditioning (instrument engine-select, or its own target) after the human's first read of the effects + this. No shell wiring yet — core + oracle only, the parity foundation.

## ADR-049 · Track E2: time engines (tap-swarm delay + FDN room) + SWARM-FX integration — ACCEPTED
Human direction after the frequency effects underwhelmed ("Let's try the time engines"): delays/reverbs are intrinsically effect-shaped, so the time engines are the effects line's better bet. src/time_core.h ports swarmtime.html's TimeLab VERBATIM on forcecore (the population being DELAY TIMES, log2 seconds): ECHO mode = tap-swarm delay (N taps on one buffer, feedback of the tap SUM DC-blocked+damped; /N feedback norm per the worst-case-correlation stability law, not √N), ROOM mode = FDN room swarm (N lines, NEGATED Householder h·s−outs[i] so the input-excited ones-eigenchannel carries +1 → comb at k/L, ADR-030a; per-line damping LP + DC blocker). Gravity attractors: rhythmic ratios×beat (echo), period-multiples (room). NaN watchdog self-heals feedback state (ADR-032). Oracle: gen_time_goldens.mjs + time_check — L0-1 parity vs the JS TimeLab (both modes), worst RMS 5.6e-12 (NOT bit-exact like the non-feedback engines: transcendental-ulp of tanh accrues in the feedback loop, but 5.6e-12 << the 1e-6 eps — same class as parity_check's dyn-ring at 4.3e-9), plus L0-19 echo LF stability (12 s at regen 0.5/0.97, tail mean|amp| 0.073/0.075 <= 0.15 — the √N-runaway ref was 0.42+), L0-20 room resonance, L0-21 room 12 s at regen 0.95 bounded + DC-controlled. verify full = NINE oracle chains. TESTABLE NOW: both wired into SWARM-FX (ADR-047) as engines 2 (Tap Delay) / 3 (FDN Room) — the unified param surface reused (Center→size, Resonance→regen, Feedback→damp), MIDI note moves the rhythmic/period gravity center; pluginval 10 + auval SUCCEEDED, installed. Residual (noted): mono core (stereo delay/reverb is the obvious refinement if they survive); host-tempo sync for the rhythmic gravity (bpm fixed 120 for now); block-start params. This closes the E1/E2 effect-engine PORTS; remaining before an effects gate: human listening verdict on which survive, then stereo + GUI + host-tempo for the survivors.

## ADR-050 · Stereo effect path for the time engines; Foxfire prior-art correction — ACCEPTED
Human direction (2026-07-19) after preferring the time engines: "Let's definitely expand into stereo." Added time_core.h processExternalStereo() — mono-summed into the delay/room network (feedback logic mirrors processSample exactly), but each tap/line panned across the field by index × a new `stereo` param (constant power), so the swarm of delays/room-modes spreads L↔R; dry keeps the input's own stereo, wet is the decorrelated field. DELIBERATE DIVERGENCE from the mono reference (swarmtime.html outputs mono) — legitimate because the effects are EXPERIMENTAL (the human explicitly wants license to experiment) and the mono reference path (render()/processExternal()) stays byte-frozen so time_check still guards it (parity worst 5.6e-12 unchanged). SWARM-FX param 17 "Stereo Width" (append-only) → time.stereo; the time engines route to processExternalStereo. Measured: L/R correlation 1.0 at width 0 (mono) → 0.69 echo / 0.79 room at 0.8. pluginval 10 SUCCESS, installed. Stereo is currently time-engines-only (the survivors); frequency-engine stereo is deferred to the "reconsider them" work. Residuals: host-tempo sync (bpm fixed 120); block-start params; per-tap pan is by INDEX (rank), not by the swarm's live state — a spatial-by-delay-value pan is a candidate refinement.
PRIOR-ART CORRECTION (same session): the human surfaced Chiral Audio's Foxfire — a SHIPPING 16-voice Kuramoto chorus (coupled LFOs modulating delay taps). This triggers the falsifier the "no shipping product" market claim carried; PRIOR-ART §1/§6 corrected to retract the flat claim and sharpen it to "no product uses coupled-oscillator dynamics as the timbre/synthesis engine" (Foxfire is a modulation effect, not a phase-transition synth). ROADMAP gains a Kuramoto-chorus engine idea (coupled modulation LFOs on short delays — distinct from the force-herded tap-swarm delay), prototype-first, citing Foxfire.

## ADR-052 · Entangled-mods proposal: oracle model for net-new dynamical cores; Phase A accepted — ACCEPTED (direction), phases gate individually
Human design proposal (2026-07-19, `docs/proposals/2026-07-19-kuramoto-entangled-mods.md`, archived from root per ADR-011's no-loose-design-files rule). Four phases: A observable extraction (R/ψ/drift/R₂/lock-ratio/slip-events as a smoothed, unwrapped mod-source bus), B coherence-budget coupling (a conserved [0,2] budget makes G₂'s *effective K* — not its gain — anticorrelate with G₁'s R), C membership spinors (per-voice (a,b)∈ℂ² with equal-power two-path render, phase carried over so a migrated voice beats against its new ensemble; C.1 stochastic tunneling, C.2 Pareto blinking, C.3 coherent Rabi), D measurement bus (slip/note/transient events collapse spinors by Born rule, then slerp-relax back). Honest framing kept: classical coupled oscillators cannot literally entangle — these are *structural analogues* (shared-state correlation, conserved budgets, measurement-as-conditioning, population transfer), and that honesty stays in README/PRIOR-ART per living-README doctrine.

GROUNDING (L0009 — every repo-state claim checked before it shaped ROADMAP): §2's mean-field assumption HOLDS (swarm_core.h:706 is `KsmS·R·sin(ψ−θᵢ−α)`, R/ψ at 664-665 — no O(N²) migration needed). Phase A's sources are ~70% already computed each control tick (R, ψ, RN, RQ, RA, RB) but only as VIZ readouts, not a published bus with unwrap/smoothing/slip-events — the bus is the real new work. §8.3's open "control tick rate 1-4 kHz" is self-answered: `controlTick` already runs every 16 samples = 2756 Hz, in-band — reuse it, don't invent a rate. §8.5 "budget acts on K not gain" already matches the core's coupling/output separation. MAPPING: Phase A = the source layer of the Phase-5 mod matrix (ROADMAP:68-69) and the Kuramoto LFO (under active human prototyping); Phase B = the CROSS-COUPLED OSC2/OSC3 variant (ROADMAP:53, "swarm-of-swarms," PARKED #5); Phases C/D = genuinely net-new, no precedent.

DECISION — oracle model (extends, does not weaken, ADR-003). Net-new dynamical cores with no external prototype follow PROTOTYPE-FIRST: build an in-repo HTML lab for the audible mechanisms (esp. C's membership rendering / phase-carry-over beating — §8.2, a by-ear choice) so the human can A/B before porting; the proposal's analytic + statistical criteria (norm conservation <1e-9 over 10⁶ ticks, Born-rule frequency within binomial bounds, deterministic replay, slip-rate-peaks-near-K_c) become the C++ PORT oracle, added as Layer-0/Layer-E rows. Parity stays the primary contract wherever a prototype exists; analytic oracles SUPPLEMENT it for inherently-statistical mechanisms — they never replace parity to make a red gate pass. Human ratified prototype-first (design Q, 2026-07-19).

FLAG (unresolved, human's call — NOT decided here): §8.1 G₂ identity is ambiguous — "modal bank from the spectral resonator (gravity lens)" could mean HYPERSAW's own filter_core (E1.1) / consonance gravity (ADR-008), OR the terrain sibling (private). The proposal itself flags it cross-project ("writes stay home" → brief→response, not direct import); if it is the sibling, it is INTEGRATIONS.md territory and the sibling's real name never enters a tracked file. §8.2 (power vs amplitude render), §8.6 (drift gating by R) are design-session items. ACCEPTED NOW: Phase A into ROADMAP as the mod-matrix source layer (rides existing prototypes, gated prototype-first per this ADR); B/C/D remain forward/parked design items pointing at the archived proposal, each ratified and sized individually when reached — realistically ~6-7 gated phases, sequenced behind the Kuramoto LFO since A is its source layer.

## ADR-053 · Kuramoto LFO design accepted (routable mod primitive, rotor-first); prototypes are NON-GOLDEN — ACCEPTED (design), port gated on a golden rotor
Human design brief (2026-07-20, `docs/proposals/2026-07-20-kuramoto-lfo.md`, end-of-prototyping report for the modulation thread; the Kuramoto LFO the ROADMAP mod-matrix item has awaited). Four concept-test prototypes attached (archived non-golden at `docs/design/kuramoto-lfo-prototypes/`, see its NON-GOLDEN.md). DESIGN DECISION accepted: build the Kuramoto LFO as a **routable modulation primitive** (per-voice swarm-coordinate LFO population published to the mod bus), NOT a hardwired chorus — the chorus is one demo destination. Ship the **rotor** first (4 phase-coupled LFOs → morph/cutoff/chorus/saturation, bipolar K, shape selector, rotor viz), then add rate → depth → destination axes behind it, one at a time, each a routable extension with its own Layer-0 rows. This is the generator side of the ADR-052 mod matrix; ADR-052 Phase A (audio-swarm observables) is the emergent-source side — complementary, same bus.

GROUNDING (L0009 — prototype code read, packet claims corrected): the packet's §4 "consuming the existing force-core" is IMPRECISE and the imprecision is load-bearing. Two coupling domains, axis-dependent: the **rotor** (shipping face) is PHASE-domain Kuramoto (`R=√(cx²+cy²)/NV`, `psi=atan2`, bipolar `±K²·3`) — reuses SwarmCore's law, NOT force_core; the rate/depth/dest **axes** are POSITION-domain springs on log2(rate)/depth/destination — force_core's domain. This ANSWERS the ROADMAP's open "reuse force_core or a dedicated phase population?" question: BOTH, axis-dependent, and the audible spine is always phase (recommendation lesson (d): rate coupling was inaudible until phase coupling rode alongside it — the ear tracks phase coherence, not rate coherence). Bipolar-K → splay differentiation from Foxfire's unipolar macro confirmed in the rotor code; both prior-art finds (Lem/Kuroscillator, Foxfire) already in PRIOR-ART.md (posture ADR-044 here — the packet's "ADR-018/048" is wrong-for-repo).

NOT GOLDEN (human override): the packet self-describes the prototypes as "each a parity oracle / headless-verified / reproduce those numbers per ADR-003"; the human directly stated they are "not golden, merely tests of the concepts." The human's word governs — these are NOT ingest-and-port references. Port gate (ADR-003 / ADR-052 prototype-first): the chosen concept (rotor) must first be hardened into a golden reference (headless-extractable core + measured anchors + the multi-LFO-cycle mod-test rule) then ingested like every engine. No port, no code this ADR — design + archive only. ADR-number collision noted: the packet's 048–052 are the authoring kit's numbers and all collide here (this repo's 052 = entangled-mods, ADR-052); ignore the packet's numbers entirely.

CARRIED LESSONS (recommendation §5 — banked for WHEN the mod subsystem is built, not written to LIBRARY now: inherited-but-unverified-here per the write gate): (a) constructor-initialize every viz-read field (render thread outruns audio thread at startup); (b) place a modulated parameter's resting point inside the signal's active range, sweep symmetric in the perceptual domain (log for freq); (c) **measure modulation over ≥3–4 full LFO cycles, never a single block** — per-block RMS/correlation/centroid is invalid below audio rate (this will DICTATE the shape of the mod acceptance oracle); (d) couple the audible quantity alongside a one-step-removed swarm coordinate — the modulator-domain analogue of ADR-016. CORRECTED FINDING (§6, self-falsified): "thin voices to graduate comb-resonance PITCH" was a spectral-centroid/brightness shift misread as pitch by a zero-crossing proxy (2821→2522 Hz was timbre, not pitch — same class as L0002 and the squareness even/odd confound). Do NOT build that control; if graduating resonance pitch is wanted, modulate base tap spacing directly.

## ADR-051 · Two-cluster A/B balance knob; swarmdynamics reference updated — ACCEPTED
Human proposal (2026-07-19, PROPOSAL-cluster-balance.md + swarmdynamics_AB.html clone). Adds one control — A/B balance in [0,1] — to the two-cluster topology: cluster A keeps intra-coupling gain 1, cluster B is scaled by kB = 1 − 2·balance, so one knob sweeps symmetric (balance 0, kB=1) → B uncoupled (0.5, kB=0) → B splayed (1.0, kB=−1), reaching the broken-symmetry state (A coherent, B dissolved) that previously required hunting the (alpha, mu, detune) corner. INGEST (ADR-011/012 reference-update pattern): verified the clone is ADDITIVE before ingesting — extracted both DynSynth cores headless, balance=0 reproduces the current reference BIT-IDENTICALLY (worst |delta|=0), change confined to the two-cluster branch. The clone replaced swarmdynamics.html (old recoverable in git; protected-path edit is the human's proposal + ratify-by-merge). PORT: swarm_core.h two-cluster branch gains kB (default balance 0 = bit-inert); shell param 56 "A/B Balance" (append-only), GUI row gated two-cluster-only. ORACLE: new golden `dyn-cluster-balance` (balance 0.5) proves the port matches the reference on the NEW code path (parity now 54/54, worst 4.262e-09 unchanged); L0-23 (ACCEPTANCE) enforces the behavioral anchors (R_A pinned, R_B dissolves, split>=0.4/0.5, mu sets floor). PROPOSAL NOTES honored: K is UNIPOLAR here (4·K²·σ) so balance is the only cluster-splay axis (K-sign×balance corners collapse to two) — verified; the per-cluster-normalization refinement is DECLINED (the reference uses global norm; parity to the reference takes precedence — flagged for Phase F if the effect drifts). The optional bipolar-balance extension (kB>1, B over-coupled) is left parked. Addresses PARKED #16 (per-cluster two-cluster controls).

## ADR-054 · Internal FX rack with a routing grid; one rack, two shells; placeholder-first — ACCEPTED (architecture)
Human direction (2026-07-20): the instrument makes an extraordinary morph across one XY sweep (orchestral swell → organic texture → alien metallic) *when placed behind the right saturation + wet reverb*; bring that inside. GROUNDING: most of the DSP already exists — delay (time_core ECHO), reverb (time_core FDN ROOM, both human-approved, ADR-049/050), filter (filter_core), notch (notch_core), all on force_core, all hosted by the standalone SWARM-FX shell (ADR-047). This direction is therefore mostly the E3 INTEGRATION (effects as post-oscillator sections) plus ONE genuinely new engine (saturation — today only the master tanh, swarm_core.h:419) plus the mod-aware/experimental layer.

DECISIONS (ratified by the human's three answers):
1. **Routing GRID, not a fixed chain — prioritized ASAP.** Order of FX is expected to be particularly significant to this instrument, so the rack ships with reorderable/routable slots from the start, not a hardcoded sat→delay→reverb order. This is the near-term build target.
2. **One rack, two shells.** Everything packages inside the main synth (internal FX sections); the standalone SWARM-FX plugin then offers *all of it* — the same rack over shared cores (E3's "same cores, two shells," now the same *rack*), possibly with a **MIDI sidechain input** for anything that needs note context (e.g. consonance-gravity-centered engines whose attractor tracks incoming MIDI, as the time engines already do via setNoteFreq).
3. **Placeholder-first.** Build the rack + routing grid + the mod-destination wiring against TRIVIAL placeholder FX to get the architecture and feel right, THEN refine each slot / drop in the real cores (filter/notch/time) and the new saturation engine. Reduce-first: the routing/UX/param-plumbing is the risk, not the DSP (which is done); prove it with placeholders before investing in each engine.
4. **Saturation engine** is the one new DSP: `drive → shaper → tone → level`, where the pre-shaper DRIVE is the lever the squareness experiment (2026-07-20, scratchpad) already identified — the master tanh is barely engaged at normal volume, so a driven saturation section is what unlocks the aggressive/metallic end of the morph (and makes the K→square transition musically reachable). Not a generic waveshaper bolt-on; the drive is the point.
5. **Mod-aware / experimental FX deferred behind the mod matrix (human accepted the recommendation).** First: FX params as ordinary mod destinations (XY / Kuramoto LFO / velocity sweep drive/size/feedback — exactly the by-hand discovery, made internal). Later (needs the mod bus to exist, ADR-052 Phase A + ADR-053 rotor): FX driven by the synth's OWN emergent observables (reverb size ← R, delay feedback gated by slip, drive ← σ) and/or FX cores CROSS-COUPLED to the carrier swarm (the effect as part of the same dynamical system — the swarmalator-unification thesis one level out, and where the truly novel engines live). E3's "collapse/comb-regularity/in-basin-error as mod sources" is this layer.

SEQUENCING: the routing grid + placeholder rack is the near-term target (per (1)); the mod-aware layer is gated on the mod-matrix work, so the rotor-to-golden step (ADR-053, PR #55 spec) remains the highest-leverage unblock. No code this ADR — architecture + roadmap only; E3 expanded accordingly. STILL OPEN (not decided): global-bus vs per-voice FX placement (per-voice saturation-before-sum is a distinct, thicker sound — flagged for the build), and which experimental engines to attempt first.

## ADR-055 · SPECTRA ADSR as a parity-safe superset defaulting to SPECTRA's OWN reference AR — ACCEPTED
Human request (2026-07-20): the shell's Attack/Decay/Sustain/Release knobs (ids 19-22) route to the SAW core but were DEAD in SPECTRA mode — SpectraCore had no ADSR, running a hardcoded AR (spectra_core.h: `atk = 1-exp(-1/(0.004·sr))`, `rel = 1-exp(-1/(0.18·sr))`). This adds ADSR to SpectraCore, mirroring ADR-021's superset-with-inert-default exactly.

THE PARITY WRINKLE AND THE RULING. SPECTRA's reference (swarmspectra.html) has NO envelope — just a fixed AR whose constants (4 ms attack / 180 ms release) DIFFER from the SAW reference's (3 ms / 160 ms; ADR-021). So SPECTRA's ADSR must default to ITS OWN constants (attackS=0.004, releaseS=0.18, sustainL=1.0) for the current AR to reproduce bit-exactly. At sustainL >= 1 the render loop takes the reference's exact expressions ((1-env)·atk gated, (0-env)·rel released) with the SAME operands/doubles as the former hardcoded AR — so the spectra goldens are the regression proof, unchanged. sustainL < 1 enters the attack→decay machine (new, deliberately divergent), identical in shape to ADR-021. Prototype-first (ADR-003): CHECKED — swarmspectra.html has no envelope to mirror, and the ADSR SHAPE added is byte-identical to the already-ratified ADR-021 superset (not a new divergent envelope), so no new prototype is required; the reference AR is the base, the ADSR the accepted extension. This is a plugin-surface superset (ADR-020), recorded here because it touches the DSP core.

OWN PARAMS, NOT THE SHARED 19-22 (the load-bearing decision). SPECTRA gets its own ADSR param ids 65-68 (sAttack/sDecay/sSustain/sRelease, SPECTRA-only, append-only) — it does NOT read the shared shell ADSR (19-22, which stay SAW-only). Sharing 19-22 was REJECTED on a parity-integrity argument: the shell's `attack` param defaults to 0.003 (SAW's constant), and if it routed into SPECTRA it would push 0.003 into the core at plugin load, silently shifting SPECTRA's default AR off its 4 ms reference — and NO oracle would catch it, because spectra_check constructs SpectraCore DIRECTLY (bypassing the shell) and renders at the core's own SParams defaults, so the goldens would stay green while the shipped plugin diverged. Under the epistemic-discipline doctrine (a comfortable green that doesn't test the real path is the dangerous case), the fix must make BOTH the goldens and the shipped plugin reference-exact by construction. Own params with own reference defaults (0.004/0.18) do exactly that. This also follows the ADR-042 precedent (SPECTRA's sub-osc took its own ids 52-55) and honors ADR-037's framing: the two references are separate frozen programs with genuinely different envelope constants, so each engine carries its own envelope.

WIRING. SSwarm gains `inAttack` (re-armed per strike, unread on the reference-exact path); SParams gains attackS/decayS/sustainL/releaseS; paramSlot maps sAttack/sDecay/sSustain/sRelease (distinct names so shell id-19 never leaks into SPECTRA via the shared-name mirror). Shell: ParamDefs 65-68 with SPECTRA reference defaults, set/read routed to the spectra core alongside 44-55. State: unique coreKeys round-trip via the existing kParams loop; old saved states load with SPECTRA's reference defaults (additive schema). GUI: rows 65-68 added to the Envelope cluster, gated SPECTRA_ONLY (19-22 already SAW_ONLY) — the one cluster shows the engine-appropriate ADSR, same pattern as the sub-osc. Regression proof: spectra_check parity (goldens) + P=1 gate (env-independent R-trajectory) stay green; state_check proves the new params round-trip.

## ADR-056 · Bipolar onset lock (signed Kenv; onset<0 = splay burst) — ACCEPTED (superset, human-approved direct implementation)
Human request (2026-07-20): make onset lock bipolar, asking whether dissolve would still return it to neutral. Onset lock is a note-start coupling burst (Kenv, initVoice) that ADDS to sync and DECAYS over `dissolve` (controlTick) back to the steady K — but it was unipolar because Kenv = 8·onset² is always ≥0. Change: `Kenv = 8·onset·fabs(onset)` (signed quadratic), and route the sign — `syncT += max(0, Kenv)`, `splayT += max(0, −Kenv)·3` (the ×3 matches the steady splay gain). So onset>0 is the existing sync burst; onset<0 is a SPLAY burst (voices start spread, then settle). Param range widened 0..1 → −1..1 (shell id 7 + GUI).

PARITY (superset, bit-inert default): for onset ≥ 0 (the entire former range) fabs(onset)==onset so Kenv is byte-identical to 8·onset², and max(0,Kenv)==Kenv / max(0,−Kenv)==0 reduce the routing to the reference's exact `(max(0,km)+Kenv)` / `max(0,−km)·3` — verify full stays green (parity 54/54, all 9 chains). onset<0 is NEW C++-only behavior with no JS reference — same posture as ADR-025 super-width (reference range bit-untouched, the extension validated behaviorally). Divergence acknowledged per ADR-003; human approved direct implementation ("throw it in, we can revert" — the change is a signed fabs + one sign-routed term, trivially revertible).

ORACLE: L0-24 behavioral anchor in trajectory_check (ADR-056 block): splay-onset early R < sync-onset early R by a clear margin (got 0.92 — the burst direction is unmistakable); dissolve neutralizes (late |ΔR| < 0.06 at K=0.9). FINDING surfaced by the anchor (answers the human's dissolve question honestly): dissolve returns the COUPLING boost to neutral (Kenv→0), but near-critical K the splay-vs-sync initial condition can leave the swarm in different basins even after Kenv→0 — genuine path-dependence/multistability, NOT a failure. So "dissolve returns to neutral" holds for the coupling; the resulting R is path-dependent near criticality and guaranteed-convergent only at strong K. The anchor checks convergence only in the guaranteed regime. TUNING OPEN: the ×3 splay-onset gain is a first guess (perceptual symmetry with steady splay); adjustable by ear. ACCEPTANCE L0-24 added.

## ADR-057 · Transposition (octave/semi/fine/pitch) extended to SPECTRA — ACCEPTED (parity-safe superset)
Human sweep request (2026-07-20): "more routings from SAW can extend to SPECTRA, like the pitch controls." AUDIT of SAW_ONLY vs what spectra_core supports: the shared coupling knobs (K, onset, dissolve, seed, vol, retrig, width) already route to both; the clear gap is TRANSPOSITION — the shell's octave/semi/fine/pitch (ids 35-38) fold into one `tune` factor (updateTune) that was applied to `core` ONLY, and spectra_core had no `tune` param, so SPECTRA was pitch-locked to the played note. Fix: spectra_core gains `double tune = 1.0` (paramSlot key "tune"); partial freqs and the sub-osc multiply `s.f0 * p.tune`; updateTune now sets tune on BOTH cores; ids 35-38 leave SAW_ONLY (shown in both engines). PARITY: tune 1.0 is bit-inert (f0*1.0 == f0 in IEEE), so spectra_check stays GREEN (worst rms 0) and every golden is untouched — verify full 9/9 green. NOT a reference divergence in the dynamics sense (it's a frequency scale, always 1.0 in the swarmspectra.html reference). SWEEP VERDICT for the rest (recorded for the follow-ups): voice mode mono/glide/legato (32-34) → needs glide/retarget in spectra_core (bigger, deferred); MPE per-note bend → spectra_core needs per-voice noteTune (deferred); drift/rtone/scatter/panScatter → core additions, lower value; dynamics (topo/poles/α/μ/grav/absK), n, dist, law, digital, inertia, detune-law → structurally SAW-only (SPECTRA has no phase-swarm topology). Also this change ships a GUI fix: the phase-circle viz box's helper-text lines (gravline/beatinfo) were min-height and grew the box when text arrived, bumping the vizcol; now fixed heights (18/34px) lock the box.

## ADR-058 · SAW waveshape morph (saw ↔ band-limited square) — ACCEPTED (parity-safe superset)
Human request (2026-07-20): a parameter that reshapes the saw waves in real time — "other waves but not sine" — even short of a full wavetable, to hear how non-saw waves behave in the swarm as they transform. Implementation: one `shape` knob (0..1, id 69, SAW-only). The osc already computes a polyBLEP saw `v`; the morph subtracts a HALF-CYCLE-OFFSET polyBLEP saw: `v = saw(ph) − shape·saw(ph+½)`. At shape 0 → saw; shape 1 → `saw(ph) − saw(ph+½)` which is a SQUARE by construction; in between, a continuum of asymmetric pulse-like hybrids. Both saws carry the SAME polyBLEP correction, so the morph stays anti-aliased (a real quality win over waveshaping a band-limited saw, which would re-alias). PARITY: guarded `if (shape > 0)`, so shape 0 is the reference path untouched — parity 54/54, all 9 chains green, build clean. shape>0 is C++-only (no swarmsaw.html reference — same posture as ADR-025 super-width / ADR-056 bipolar onset); its correctness is the provable identity `saw−saw(ph+½)=square` plus a trajectory_check bounded/NaN-clean guard (L0/ADR-058 block) at shape 0.5 and 1.0. The ×16-voice cost of the second polyBLEP is paid only when shape>0. SCOPE (first pass, deliberately minimal): one saw↔square axis. FOLLOW-UPS (noted): a pulse-WIDTH knob (offset ≠ ½ → PWM/pulse family) and triangle (integrate the square) are cheap extensions of the same two-saw machinery. Revertible: delete the guarded block + the param.

## ADR-059 · Inertia knob taper: tune-then-lock dev slider — ACCEPTED (dev tuning aid, temporary)
Human report (2026-07-21, correcting the Phase-2 E-1 caveat): the inertia knob's response is steep just after 0 — a small move off 0 has an outsized effect — pronounced at low detune + retrigger-on (voices start synced + near-identical, so the underdamped momentum system rings coherently even at small w; the ADR-024 sqrt taper compressed the audible-hunt onset to the bottom of the knob). The response is a CONTINUOUS taper problem, not a bifurcation, so reshaping the knob→w map fixes it — but the perceptually-right curve is an ear call, not a derivable one. So: a TUNE-THEN-LOCK dev control. The inertia knob taper becomes `w = knob^curve` (id 70 "Inertia Curve (dev)", 0.3..5, default 0.5). At 0.5 it uses `sqrt` EXACTLY (special-cased — pow(x,0.5) is not bit-identical to sqrt(x)), so the default feel is unchanged; higher curve → smaller w at low knob → gentler onset. The human dials it by ear, reports the value where the response feels even, and that value gets HARDCODED (taper replaced, id 70 + slider removed). PARITY/SAFETY: the taper is SHELL-side (applyParam), not the core — parity_check (which sets core inertia directly) is unaffected regardless; verify full 9/9 green, state_check confirms the param round-trips, pluginval 10 SUCCESS. Pre-1.0 so the temporary id (70) is acceptable churn. Same tune-then-lock mechanism is offered for the K cloud→order edge (~0.6-0.8) if wanted. Trace: 2026-07-21-inertia-curve.md.

## ADR-060 · Tone tilt folded from the detune lab into the SAW reference + core — ACCEPTED (parity-safe superset)
First fold from the detune-lab audition (docs/design/detune-lab.html) into the real thing, chosen as the pipeline-primer (human: "safe superset first", 2026-07-23). Tone tilt is a bipolar per-voice one-pole filter: `tilt>0` darkens (LP), `tilt<0` thins/removes low end (HP), cutoff rising as √(f/f0) so higher voices lose more top; `tilt=0` is inert. REFERENCE-FIRST per ADR-003: transcribed into swarmsaw.html (the oracle) FIRST — new `tilt:0` param, per-voice `vlp`/`vlpc` state, the coeff loop after σ, the one-pole in the render voice loop, `vlp` reset on note-on — then mirrored bit-for-bit into swarm_core.h (param, Swarm arrays, ctor init vlpc=1, initVoice reset, controlTick coeff, render application after the ADR-058 shape block, paramSlot "tilt", `tiltHP` member). PARITY: two new golden scenarios (tilt-dark +0.6/K, tilt-thin −0.6) × 3 seeds; `parity_check` 60/60 within ε (the 6 new at **rms 0.000e+00** — C++ matches JS exactly), the pre-edit 54 unchanged (verified bit-identical vs origin/main before the C++ side), all 9 chains green, notefuzz green. The taper is `H = tilt>0 ? 2·200^(1−|t|) : 0.1·24^|t|` (smooth onset from bypass — no jump). NOT YET a CLAP param: this ADR folds the DSP + parity only; exposing `tilt` on the frozen CLAP surface (a public-interface change) is the next gated step. Trace: 2026-07-23-fold-tone-tilt.md.

## ADR-061 · Hi-tame equal-loudness roll-off folded into the SAW reference + core — ACCEPTED (parity-safe superset)
Second lab→core fold (after ADR-060 tone tilt), continuing the map's superset-first sequence. Hi-tame is a per-voice equal-loudness gain: each voice is scaled by `(f0/f)^hiTame`, turning the higher voices down so a tall detuned/harmonic stack isn't harsh up top; `hiTame=0` is inert (guarded — no multiply). REFERENCE-FIRST per ADR-003: swarmsaw.html edited first (new `hiTame:0` param, per-voice `hg` gain array init 1, the gain computed after the tilt coeff loop, applied `v *= hg[i]` after the tilt in the render voice loop), then mirrored bit-for-bit into swarm_core.h (param, `Swarm.hg`, ctor init 1, controlTick compute, render application after the tilt, paramSlot "hiTame"). No note-on reset — hg is recomputed each control tick, not stateful. PARITY: one new golden scenario (hi-tame, hiTame 1.0 / detune 0.6 / n 12) × 3 seeds; `parity_check` 60/60 → **63/63** within ε (the 3 new at **rms 0.000e+00**, C++ == JS exactly), the pre-edit 60 verified bit-identical vs origin/main (incl. with tilt active), all 9 chains + notefuzz green. NOT YET a CLAP param — DSP + parity only; the CLAP surface is one batched pass at the end of the fold (human cadence choice, 2026-07-23). Trace: 2026-07-23-fold-hi-tame.md.

## ADR-062 · Drift modes + keep-phase folded into the SAW reference + core — ACCEPTED (grouped parity-safe supersets)
Third fold (human cadence: group low-risk supersets, 2026-07-23). TWO inert-default features in one PR, each proven individually. (a) **Drift modes**: `driftMode` 0 walk (the original 1/f random walk) / 1 sine (per-voice decorrelated LFO) / 2 sample&hold (stepped random); mode 0 is bit-identical to the prior drift. (b) **Keep-phase**: `keepPhase=1` makes note-on continue from the last-sounding phases (a per-block `lastPhase` snapshot of the focus swarm) instead of retrig/random; 0 inert. REFERENCE-FIRST (ADR-003): swarmsaw.html edited (params, `driftPh`/`driftHoldT` state, `lastPhase`, the drift-mode branches, the note-on phase precedence `keepPhase ? lastPhase : (retrig ? 0 : rand)`, the render-tail snapshot), then mirrored into swarm_core.h — with keep-phase placed ABOVE the core-only `scatter` branch (ADR-033) to match swarmsaw's precedence, so both engines skip rngNext identically when keep-phase is on (the rngState must stay in lockstep for subsequent drift). PARITY: 3 new goldens (drift-sine, drift-sh, keep-phase-with-drift) × 3 seeds; `parity_check` 63/63 → **72/72** within ε — drift modes **rms 0.000e+00**, keep-phase ≤ 6e-18 (sub-ULP, one seed; the rngNext-skip stays exact). Inertness proven first vs origin/main (defaults / drift-walk / gauss+tilt bit-identical at the defaults). All 9 chains + notefuzz green. Harness note: the golden harness fires chords, not note SEQUENCES, so the keep-phase golden starts from an empty `lastPhase` (phase 0) — it exercises the branch + the rngState-skip, not the non-zero continuation (that is lab-verified). NOT YET CLAP params (batched at fold end). Trace: 2026-07-23-fold-drift-modes-keep-phase.md.

## ADR-063 · Opt-in frequency glide (de-zipper) folded into the SAW reference + core — ACCEPTED (parity-safe superset)
Fourth fold. The lab's frequency smoothing is ALWAYS-ON, which would have been a behavioural change to the reference (it shifts drift and slider-sweep transients and could move the trajectory anchors) rather than an inert superset — so it was excluded from the ADR-062 group and folded here as **OPT-IN** instead (human decision, 2026-07-23: "opt-in approved, for now"). `freqGlide` is a time constant in **SECONDS** per ADR-009 (seconds in, coefficients out), default **0 = off = bit-identical**. Two legs, mirroring the lab: (a) a control-rate one-pole on each voice's target frequency, coefficient `1 − exp(−dt/freqGlide)` with `dt = kTick/sr`; (b) a per-sample slew on the oscillator frequency at a quarter of that time constant, `1 − exp(−1/(freqGlide·0.25·sr))`, which removes the residual control-rate stair the coarse leg leaves. Both SNAP on note-on (`vfInit`), so a new note starts dead on pitch instead of gliding in from the previous one. REFERENCE-FIRST (ADR-003): swarmsaw.html edited (param, `vfSm`/`fRun`/`vfInit` state, the conditional smoothing in the frequency loop, the note-on snap, the per-sample leg in render), then mirrored bit-for-bit into swarm_core.h (+ paramSlot "freqGlide"). EFFICACY measured on the steppiest source (sample&hold drift at full rate): per-tick voice-frequency jump **9.26 Hz → 0.54 Hz (94 % smaller)**. PARITY: one new golden (freq-glide, freqGlide 0.006 against S&H drift) × 3 seeds; `parity_check` 72/72 → **75/75** within ε, the 3 new at **rms 0.000e+00** (C++ == JS exactly); inertness proven first vs origin/main (defaults / drift-walk / drift-sine / keep-phase+tilt all bit-identical at freqGlide=0); all 9 chains + notefuzz green. "For now" is recorded: if the always-on behaviour is later preferred, that is a separate ADR with re-measured trajectory anchors. NOT YET a CLAP param (batched at fold end). Trace: 2026-07-23-fold-freq-glide.md.

## ADR-064 · Pan motion + centre pin folded into the SAW reference + core — ACCEPTED (grouped parity-safe supersets)
Fifth fold; the two are grouped because they are coupled — the centre pin scales pan motion as well as drift. (a) **Pan motion**: `panMotion` depth + `panMode` (0 = independent per-voice LFO drift, 1 = one shared sweep of the whole image); slow LFOs displace each voice's signed base pan, recomputed once per block, then re-encoded to the constant-power L/R gains. (b) **Centre pin**: `motionCenter` scales BOTH drift and pan motion by each voice's normalised distance from the fundamental (`cdist = |vf − f0| / max`), so at 1 the fundamental is held still and the outer voices keep full motion — the human's "stable core, moving periphery". All three default 0 = inert. REFERENCE-FIRST (ADR-003): swarmsaw.html edited (params; signed base pan now stored in rebuild — bit-inert, panL/panR unchanged; per-voice `cdist`; the drift weight `mw = 1 − motionCenter·(1 − cdist)`, exactly 1.0 when off; the per-block pan-motion block selecting modulated gains), then mirrored into swarm_core.h. NAMING NOTE: the core's member is `panBase` (not `pan`) because `rebuild()` already has a local `double pan`; the core's base pan is captured POST-scatter (ADR-033), so pan motion rides on top of scatter. cdist uses the PREVIOUS tick's value (one-tick lag, as in the lab) — negligible since cdist is near-static. MEASURED: with the pin at 1 the centre voice's drift swing goes 8.06 Hz → **0.00 Hz** while the edge voice stays 9.98 → 9.98 Hz. PARITY: 3 new goldens (pan-drift, pan-sweep, centre-pin driving both legs) × 3 seeds; `parity_check` 75/75 → **84/84**, all 9 new at **rms 0.000e+00**; inertness proven first vs origin/main (defaults / drift-walk / wide+tilt / glide+S&H bit-identical). All 9 chains + notefuzz green. NOT YET CLAP params (batched at fold end). Trace: 2026-07-23-fold-pan-motion-centre-pin.md.

## ADR-065 · Harmonic detune law (law 4) + `harmReach` folded into the SAW reference + core — ACCEPTED (parity-safe superset), with a recorded DOMAIN LIMIT on the bit-parity contract
Sixth fold, and the first of the map's **new detune laws** (the supersets are done). The law is the one the human reverse-engineered from the NI demo (2026-07-23): pulling voices out one at a time at full spread walked them **down the harmonic series** — the spread pattern IS the harmonic series. So law 4 places voice `i` at `f = f0·(1 + detune·harmReach·i)`: the voice INDEX is the harmonic rung. At `detune·harmReach = 1` the stack is exactly f0, 2f0, 3f0 … (H1..Hn); below that it is a continuous unison→harmonic-series morph. It deliberately ignores `dist` and `anchor` — the geometry is the integer series, not a distribution — and `harmReach` decouples the top harmonic from the voice count (`harmReach` 1.833 with n=7 → top voice at H12), which is what makes the character adjustable without changing polyphony cost.

NAMING (a real collision, not a style choice): the core ALREADY had `reach = 5` — the RING-topology neighbourhood radius (ADR-030 lineage). The lab's knob is called "reach", so a literal transcription would have been a duplicate member with a clashing meaning in the same struct. Folded as **`harmReach`** in BOTH files; the lab keeps its label.

REFERENCE-FIRST per ADR-003: swarmsaw.html edited first (`harmReach:1` param, the `else if (p.law === 4)` branch ahead of the ERB branch), then mirrored bit-for-bit into swarm_core.h (`double harmReach = 1`, the same branch, paramSlot). Laws 0/1/2/3 take no new code path — inert by construction.

PARITY: 3 new golden scenarios (harm-series = full series; harm-partial = mid-morph; harm-reach = harmReach beyond n) × 3 seeds. `parity_check` 84/84 → **93/93** within ε, the 9 new at **rms 0.000e+00** (C++ == JS exactly); the pre-edit 84 unchanged; all 8 chains + notefuzz green.

**THE DOMAIN LIMIT (the load-bearing finding).** A fourth scenario — law 4 at full spread WITH strong coupling (K 0.6) — FAILED parity at **rms 9.736e-02**, ~5 orders of magnitude outside ε. Diagnosis, not assumption: divergence was bracketed and scales with coupling × spread (3.7e-19 → 4.4e-13 → 3.5e-07 → 9.7e-02), and then falsified directly — perturbing the **JS reference alone by 1 ULP** and re-rendering gave **rms 9.383e-02**, i.e. JS-vs-itself diverges as far as C++-vs-JS, growing over time (5.62e-2 in 0–0.5 s → 1.147e-1 in 1–2 s), while stable regimes under the same probe stayed at 1.869e-19 and exactly 0. So this is **chaotic amplification of the last bit, not a porting bug**: a harmonic stack under strong coupling has a positive Lyapunov exponent, and `Math.sin` and `std::sin` are not identically rounded, so sample-exact parity in that regime is impossible **in principle** — no implementation could pass it.

RULING: the golden was REMOVED (with an explanatory comment in `gen_goldens.mjs` pointing here) rather than kept red or given a loosened ε — never weaken a gate (CLAUDE.md oracle discipline). The regime is covered instead by a **behavioural anchor** in `trajectory_check` (ADR-065 block): bounded + NaN-clean uncoupled and coupled, and coupling measurably entrains the harmonic stack. The anchor uses **K 0.9, not 0.6** — probed, K 0.6 lifts late-window R by only 0.006 on an octave-plus stack (indistinguishable from chaotic jitter, an anchor that could flip sign on any legitimate refactor), while K 0.9 is the first value that actually entrains this geometry (R 0.24 → 0.70), so the claim is checked where it is true, with margin (gate: ΔR > 0.2).

CONSEQUENCE FOR THE CONTRACT: bit-parity (ACCEPTANCE L0-1, ε=1e-6 RMS) is a valid oracle **only in non-chaotic regimes**; where the dynamics amplify ULP differences, correctness must be defined behaviorally. ACCEPTANCE.md is a protected path, so this ADR records the limit and **flags the amendment for human approval** rather than making it. NOT YET a CLAP param (batched at fold end). Trace: 2026-07-23-fold-harmonic-law.md.

## ADR-066 · Stretch (inharmonic) detune law — law 5, NOT the lab's law 3 — folded into the SAW reference + core — ACCEPTED (parity-safe superset)
Seventh fold, second of the new detune laws. The stretch law places voices by cents as law 0 does, then stretches the OFFSET ITSELF by `(1 + stretchB·x²)`, so outer voices spread disproportionately: `rat = 2^(x·detune·100/1200) − 1; f = f0·(1 + rat·(1 + stretchB·x²))`. That is piano/bell inharmonicity — the metallic end of the law set, and the complement to ADR-065's harmonic law (which lands ON the integer series; this one deliberately misses it).

NUMBERING (the decision this ADR exists to record). The lab menus stretch as **law 3**, but law 3 in the core is the TEMPO-GRID law (ADR-005/ADR-022, ported from the DYNAMICS reference) and law 4 is now harmonic (ADR-065) — so a literal transcription would have silently overwritten the tempo grid, and `dyn-grid` (which selects law 3 via `coreToDyn`) would have started rendering an inharmonic stack while still called a grid. Stretch takes **law 5**. The lab keeps its own numbering: it is an audition instrument with its own menu, not a numbering authority, and forcing it to renumber would edit a file whose only job is to be played. The divergence is recorded here and in both law tables so the next transcription does not re-derive it wrong.

REFERENCE-FIRST per ADR-003: swarmsaw.html edited first (`stretchB:0` param, the `else if (p.law === 5)` branch ahead of the ERB else), then mirrored bit-for-bit into swarm_core.h (`double stretchB = 0`, same branch, paramSlot). Laws 0/1/2/3/4 take no new code path — inert by construction.

PARITY: 3 new golden scenarios (stretch-flat B=0; stretch-mild B=2; stretch-bell B=6 across 12 voices) × 3 seeds. `parity_check` 93/93 → **102/102** within ε, the 9 new at **rms 0.000e+00** (C++ == JS exactly); all 8 chains + notefuzz green.

THE B=0 IDENTITY, MEASURED NOT ASSERTED. `stretchB = 0` reduces the expression to `f0·(1 + (2^(…) − 1))`, which is algebraically law 0 — but `1 + (v − 1) == v` is NOT a general floating-point identity, so the claim was checked rather than reasoned: a temporary `ident-law0` scenario (law 0, same detune/n) was generated alongside `stretch-flat` and the renders byte-compared — **IDENTICAL across all three seeds** (sha256 ac88657551bd… for both). So `stretch-flat` doubles as a regression pin on that identity: if a future edit to the stretch branch perturbs the B=0 path, its golden moves. The temporary scenario was removed; the check is recorded here as the evidence for the comment in both law tables.

NOT YET a CLAP param — `stretchB` is unexposed and the `law` param still ranges 0..3 (`kLawLabels` has four entries), so neither harmonic nor stretch is reachable from the host yet. Exposing them is part of the one batched CLAP pass at fold end (human cadence, 2026-07-23), which is a public-interface change and therefore its own gate. Trace: 2026-07-23-fold-stretch-law.md.

## ADR-067 · Golden (irrational) distribution — dist 4 — folded into the SAW reference + core — ACCEPTED (parity-safe superset)
Eighth fold, last of the *cheap* detune-geometry items (octave spread + root-anchor remains, and it rewrites the placement block — sequenced separately). The golden distribution places voice i at `x = 2·(((i+1)·φ⁻¹) mod 1) − 1` with φ⁻¹ = 0.6180339887498949: low-discrepancy irrational spacing — voices never repeat a position and never cluster, "even-but-inharmonic". It fills the gap between the deterministic-but-regular dists (even/JP) and the seeded-random ones (gaussian/cauchy): structured like the former, non-repeating like the latter.

MECHANICS: the reference's cauchy branch was the trailing `else` (the menu caps at 3), so golden required converting it to an explicit `else if (dist === 3)` with golden as the new `else`. Bit-inert for dists 0–3 — no branch body changed. Golden consumes NO rngG draws; safe because `grng` is re-seeded at the top of every rebuild, so a zero-draw distribution cannot desync anything downstream. C++ mirror uses `std::fmod`, which equals JS `%` for positive operands (both are here). Per convention (unchanged since the tempo-grid law), the reference's browser menu is NOT extended — the engine code is the spec; the menu is incidental GUI.

PARITY, with the inertness proof made explicit this time: golden manifests were generated from BOTH the edited reference and origin/main's reference and diffed per-scenario — all 102 pre-existing renders **sha256-identical**, only the 6 new files added. Then `parity_check` 102/102 → **108/108** within ε, the 6 new (golden-dist wide/11-voice; golden-sync coupled K=0.8) at **rms 0.000e+00**; all chains + notefuzz green. The coupled scenario is safe under the ADR-065 domain limit — K 0.8 over a ±semitone-class spread is the same regime as the long-standing sync goldens, and it renders exactly.

NOT YET a CLAP param — `dist` id 4 currently ranges 0..3 (kDistLabels); widening to 0..4 joins the batched CLAP pass (task #18) alongside the law 0..5 widening. Trace: 2026-07-24-fold-golden-dist.md.

## ADR-068 · Octave spread + root anchor — the placement-block rewrite — folded into the SAW reference + core — ACCEPTED (parity-safe superset)
Ninth fold, and the one the fold map warned about: unlike every previous fold (a new guarded branch), `spread` and `anchor` thread through EVERY detune law — the placement block is rewritten, not extended. `dep = detune·spread` replaces `detune` in all six laws, and each voice's position becomes `x − anchor·xmin` (xmin = the lowest raw x, recomputed per rebuild). At spread 24 law 0 goes from ±1 st to ±2 oct; at anchor 1 the LOWEST voice lands exactly on the root and the ensemble fans upward — the NI-style unipolar image the whole detune-lab campaign started from.

SEMANTIC RULINGS (recorded because the lab could not decide them):
- **Harmonic law (4)** ignores x and therefore the anchor (it is inherently root-anchored), but spread DOES scale it — `1 + dep·harmReach·i` — matching the lab.
- **Stretch law (5)** uses the ANCHORED x in its `x²` term, matching the lab: the inharmonicity follows the shifted geometry, not the raw one.
- **Tempo-grid law (3)** exists only in the core (the lab and the SAW reference have no law 3), so the lab is silent on it. Ruling: dep and the anchored x apply uniformly there too — placement semantics stay singular across the law table. Bit-inert at defaults; a non-default spread/anchor on the grid is C++-reachable-only behavior (same posture as ADR-025 super-width) until a golden covers it.
- **rootWeight is OUT of scope**: the lab entangles it with anchor (`aw = anchor` gates it), but it is a GAIN feature, not placement; it stays lab-only pending its own fold.

INERTNESS — the load-bearing proof, done the ADR-067 way: goldens generated from the edited reference AND origin/main's reference, manifests diffed per scenario — all **108 pre-existing renders sha256-identical**. The IEEE identities doing the work (`detune·1.0 == detune`, `x − 0.0 == x`) hold exactly, including for negative and ±0 x.

PARITY: 3 new golden scenarios × 3 seeds — spread-octave (±1 oct, even), anchor-root (anchor 1 over SEEDED cauchy, so xmin varies per seed and the anchor path is exercised for real — its three seed hashes all differ, as designed), spread-anchor-ni (JP, spread 8, anchor 1 — the NI image). `parity_check` 108/108 → **117/117**, the 9 new at **rms 0.000e+00**; all chains + notefuzz green. Wide-spread scenarios stay UNCOUPLED (K 0): wide + strong K is the chaotic regime the ACCEPTANCE §L0-1 domain limit excludes (ADR-065); coupled-wide behaviour is anchor territory, not golden territory.

GEOMETRY, measured on the reference before the goldens: spread 24·detune 0.5 puts the bottom voice at exactly 0.5000·f0 (−1200 c); anchor 1 over cauchy pins the lowest voice at exactly 1.0000·f0 with every other voice above it. Interior voices sit off naive positions because density-comp (normExp 0.75) is active at defaults — the established engine layer, not this change.

NOT YET CLAP params — spread/anchor join the batched pass (task #18). Trace: 2026-07-24-fold-spread-anchor.md.

## ADR-069 · Root-pinned pacemaker (`pivotMode`) — the first approved coupling-law divergence — ACCEPTED (parity-safe superset)
Tenth fold; first of the human-approved divergences (2026-07-24), and unlike ADR-060..068 it touches the COUPLING LAW, not placement or tone. `pivotMode` 1 replaces the mean-field sync attractor with a pacemaker: every voice entrains to the FUNDAMENTAL — the voice nearest f0 (min |vf−f0|, recomputed each control tick: the centre voice in symmetric detune, the lowest in anchored/harmonic modes). `sin(root − self)` is zero for the root itself, so it never moves: the swarm folds ONTO the played pitch (pitch-stable collapse) instead of onto the drifting swarm mean. The splay anchor (c0) also re-anchors on the root, so K<0 splays the fan around the fundamental. Human verdict from the lab (2026-07-22, ROADMAP): "totally different, but a more musical sound in general at a lot of settings" — BOTH modes kept as a toggle.

KNOWN TRAIT, KEPT: the pacemaker term drops the R scaler (`KsmS·sin(...)` vs `KsmS·R·sin(...)`), so its onset just off K=0 is a touch stronger than mean-field's. The lab flagged it for possible re-taper at port; folding verbatim was chosen — re-tapering would diverge from the lab reference the human actually auditioned, and the taper can be revisited with a knob-curve later (ADR-059 precedent) without touching the DSP.

SCOPE RULING: in the core the pivot applies on the topo-0 / poles-1 path ONLY — the lab has neither ring/two-cluster topologies nor Daido poles, so pivot × those combinations is meaningless-by-construction and gated off (pivot is simply not read there). Sakaguchi alphaR rides along uniformly inside the pacemaker term (0 in the SAW reference → bit-equal, ADR-023 pattern).

REFERENCE-FIRST (ADR-003): swarmsaw.html edited first (`pivotMode:0` param; rootIdx scan; the branch in the sync term; c0 re-anchor), then mirrored into swarm_core.h (+ paramSlot). rootIdx is computed only when pivoting, identically in both.

PARITY: inertness by manifest diff — all **117 pre-existing renders sha256-identical** vs origin/main. 3 new goldens × 3 seeds, ALL COUPLED (pivot only acts through coupling): pivot-lock (K 0.8), pivot-splay (K −0.8), pivot-anchor (the musical combo with ADR-068 anchor 1 + spread 4, K 0.7). `parity_check` 117/117 → **126/126**, the 9 new at **rms 0.000e+00** — notable: the pacemaker under coupling renders EXACTLY, where mean-field at comparable drive can go chaotic (ADR-065); a pinned root leaves no drifting mean to amplify ULPs. Spreads kept modest by design to stay inside the L0-1 parity domain. Behavioural anchor (trajectory_check ADR-069 block): bounded/NaN-clean on both K signs; entrainment ΔR **0.672** vs a 0.2 gate. All chains + notefuzz green.

NOT YET a CLAP param — joins the batched pass (#18). Trace: 2026-07-24-fold-root-pivot.md.

## ADR-070 · Alternating pitch-ranked pan fan as the DEFAULT image — the one true default-changing divergence — ACCEPTED
Eleventh fold; second approved divergence (#17), and the only one in the campaign that CHANGES DEFAULT OUTPUT: the stereo image of every width-bearing patch. The new default (`panLayout` 0): voices ranked by PITCH (raw x; by index when harmonic — law 4's rung IS the pitch order), rank r stepping out from dead centre on alternating sides at distance (r/(n−1))^γ, γ = 6^(0.5−panCurve) (0.5 linear · <0.5 centre-focused · >0.5 side-focused), `panInvert` flipping the triangle. Rank 0 — the fundamental — sits EXACTLY centre: the root holds the middle and the ensemble widens as it climbs (the human's requested default image, 2026-07-21). The legacy x-proportional image is KEPT as `panLayout` 1 — same posture as ADR-069's toggle — because gaussian/cauchy under it scramble stereo order by design, which is its own character.

WHY pitch-ranked: the legacy image maps DISTRIBUTION position to FIELD position, so seeded dists (gaussian/cauchy/golden) place voices in scrambled stereo order — the complaint that started this (human, 2026-07-22: "the distributions don't order the pan voices correctly"). Ranking by pitch makes the image follow the EAR's order regardless of layout.

PARITY MECHANICS (three findings recorded):
1. **std::stable_sort, not std::sort** — JS Array.sort is stable (ES2019), and gaussian/cauchy CLAMP at ±1, so ties are reachable; an unstable sort would order ties differently and break parity silently on some seed eventually.
2. **rebuild triggers grew**: law/panLayout/panCurve/panInvert now rebuild (the fan ranks by pitch, so the image depends on them) — safe because rebuild is idempotent (x[] deterministic from seed).
3. **The DYN reference needed pinning** — caught by a RED, not by inspection: dyn scenarios replay core-side on SwarmCore, whose default image moved, while DynSynth (a separate frozen program) keeps the legacy image — all 8 dyn scenario groups failed at rms 2.5e-2..5.6e-2 until DYN_BASE pinned `panLayout: 1` (coreToDyn drops the key; dyn has no layout selector). The failure is the proof the pin is honest, not cosmetic.

DIVERGENCE SCOPE, measured not asserted (manifest diff vs origin/main): exactly the predicted set changed — all **34 SAW scenario groups** (width defaults 0.8; every one width-bearing) re-measured; all **8 dyn groups sha256-identical**; spectra untouched. THE LEGACY PIN: new scenario `pan-legacy` (= old tilt-thin params + panLayout 1) **hash-MATCHES the pre-ADR-070 tilt-thin render on all three seeds** — the old image is preserved bit-for-bit behind the toggle, so nothing auditioned before this fold is lost.

PARITY: `parity_check` 126/126 → **141/141** (5 new scenario groups: fan over gauss (per-seed hashes differ — ranking does real work), curve 0.15, invert, fan×harmonic, pan-legacy), all at **rms 0.000e+00**; trajectory anchors unaffected (R-based — pan never touches phase dynamics); all chains + notefuzz green.

MONO NOTE: constant-power panning means the mono fold (L+R) of a voice depends on its pan position, so the new default DOES alter mono content slightly — this was the lab's documented exception ("all default-inert except the deliberate new pan default") and is accepted with the divergence.

NOT YET CLAP params (panLayout/panCurve/panInvert join #18). Pre-1.0, no shipped sessions — the default flip costs nothing now; post-1.0 it would have been a breaking change (see the consolidation-review roadmap item for the general principle). Trace: 2026-07-24-fold-pan-fan.md.

## ADR-071 · FX rack increment 2: Comp and Comb — the rack's first REAL cores (E3; human-ruled destinations) — ACCEPTED
Implements the 2026-07-24 rulings: comp/limiter and the polyphonic KS comb land as RACK SLOTS (types 4/5 on the existing fx params 57–64), NOT core folds — the HPF precedent applied twice. First increment past ADR-054's placeholders.

**Comp (type 4).** The lab's comp + limiter as ONE dynamics slot: amount = comp strength; the 0.98 brickwall is always engaged while the slot is active ("optional" = the slot being Off — one amount knob per slot is the rack's grammar). Peak follower with fast-attack/slow-release; soft knee above 0.4; ratio 1 + 4·amount. **ADR-009 applied at transcription**: the lab's follower coefficients (0.3 / 0.0015) were PER-SAMPLE at 44.1 kHz — banned form. Converted to seconds (atk 63.58 µs, rel 15.11 ms) and re-derived per-rate in setSampleRate, so the follower is sr-independent; ctor defaults preserve the 44.1 kHz behavior if setSampleRate is never called.

**Comb (type 5).** The lab's per-note Karplus-Strong comb re-hosted BUS-side: the rack keeps kCombLines=8 tuned feedback lines; the SHELL feeds note events (`rack.noteOn` at the common note-on point, both engines — ADR-054's anticipated note-context sidechain, now real). Each line is fed the whole bus and resonates its own pitch out of the mix (sympathetic-string posture) — a deliberate ARCHITECTURAL divergence from the lab, where each swarm's comb ate only that swarm's own output; the polyphonic per-note-tuned character is preserved, the per-voice isolation is not (recorded honestly; if A/B against the lab says the isolation matters, the comb moves core-side as its own decision). amount = wet mix; resonance fixed at the lab default (fb 0.79, damp 0.5) until the rack grows per-slot param pages. Note-off leaves the line ringing (natural KS decay); reuse is steal-oldest. Allocation only in ctor/setSampleRate (main thread); note-on clearing is a bounded memset — RT-clean per the charter.

**Interface note.** fx type params 57/59/61/63 widened 0..3 → 0..5 + labels "Comp"/"Comb" — the direct implementation of the human's slot approvals; pre-1.0, ids unchanged, values append-only.

**EVIDENCE.** (1) All-Off passthrough intact: parity 141/141 untouched, all chains + notefuzz green (notefuzz drives the shell factory, so the new note feed is fuzzed). (2) Engaged paths probed headlessly through the REAL CLAP plugin (scratch harness, 4-note chord A3/C#4/E4/A4): Comp takes chord peak 0.840 → **0.523** with mild rms reduction (0.225 → 0.209) — compression engages above the knee and the brickwall holds; Comb lifts rms 0.105 → 0.401 (single note, mix 0.9) with tail decaying to **0.000000** after note-off — resonates, rings, dies; both NaN-clean and bounded. The first probe run caught its own blind spot: a single A3 peaks at 0.34 < the 0.4 knee, so Comp measured identical to Off — the chord drive is what makes the evidence real. (3) pluginval strictness 10 **SUCCESS** including its param-fuzz sweep over the widened type range. Trace: 2026-07-24-fx-comp-comb.md.

## ADR-072 · Batched CLAP param pass — fold-campaign features made host-reachable — ACCEPTED (public interface, human-ratified roster)
Task #18, the gate the fold campaign (ADR-060..070) parked its interface work behind. Human rulings (2026-07-29): expose everything EXCEPT `lpOut` (core-only oracle/structural switch); `toneTilt` rename approved ("whatever works in the medium term" — display names are free to change later, ids/keys are not); state-compat approach delegated. Widened: `law` 0..3→0..5 (harmonic ADR-065, stretch ADR-066), `dist` 0..3→0..4 (golden ADR-067). New ids **71..86** (toneTilt · hiTame · driftMode · keepPhase · freqGlide · panMotion · panMode · motionCenter · harmReach · stretchB · spread · anchor · pivotMode · panLayout · panCurve · panInvert), each at its audition-lab range, each defaulting to the core's bit-inert default.

GHOST ID 70: the ADR-059 dev inertia-taper exponent is intercepted by NUMBER in applyParam/readParam with no row in kParams, so the id space has a hole the table cannot show — "table max + 1" is NOT the next free id. toneTilt landed on 70 first and its writes were silently swallowed (the functional smoke caught it: 15/16 ACT, toneTilt DEAD). The block starts at 71 and the ghost is documented at the table. Successor rule: check applyParam/readParam for numeric intercepts before allocating ids.

KEY COLLISION RETIRED, NOT GUARDED: SwarmCore's tone tilt is exposed as `toneTilt` (core alias to `p.tilt`), never as `tilt` — id 45 owns that key for SPECTRA and unguarded ids mirror into both cores by name (PR #125's probe evidence).

STATE: state_save writes raw values by key, so saved sessions are immune to the enum widening BY CONSTRUCTION — no remap machinery added, none needed. Recorded residual: VST3 normalized automation lanes on law/dist recorded pre-widening re-scale (a lane at law 3 = normalized 1.0 replays as law 5); accepted — sessions are the protected object, enum automation lanes are rare, and the alternative (parallel params) permanently forks the surface.

EVIDENCE: paramfunc_smoke (new diagnostic) — all 16 ids ACT at extremes against a width/detune-hot baseline, keepPhase through a real retrigger, plus law 4 / dist 4 alone; paramleak_probe extended to both directions with firing positive controls — all 16 inert on the SPECTRA engine, id 45 still inert on SAW. `./verify full` green, all nine chains; the only core edit is the alias (parity-neutral, worst rms unchanged). Neither diagnostic is wired into ./verify (gate changes need a human; propose separately if wanted).

## ADR-073 · Even-voice symmetric pan fan — no centre seat at even n — ACCEPTED (default-output change, human-directed)
ADR-070 seats fan rank 0 at dead centre and steps out at d = r/(n−1). At EVEN voice counts that degenerates: n=2 gives one voice dead centre and one voice HARD side — a lopsided image at any width ("2 with any width is unlistenable", human 2026-07-30, which is also the protected-path gate approval). FORK ON PARITY: odd n unchanged (ADR-070's image is explicitly the odd case, root holding the middle); even n uses d = (floor(r/2) + 0.5)/(n/2), so pairs sit symmetrically about centre and no voice occupies it. Alternating sides, pitch ranking, panCurve and panInvert are untouched — only the distance law forks.

EVIDENCE. Inertness by sha256 over renders (stashed reference vs edited): 144 bit-identical, exactly 4 changed — cauchy-cloud/gauss-cloud (n=16), hi-tame/stretch-bell (n=12) — i.e. every changed render is genuinely even-n and no odd-n render moved. Parity 147/147 within ε after adding even-fan-2 / even-fan-4 goldens (n=2 rms 0.000e+00, n=4 rms 2.393e-17); goldens carry width EXPLICITLY because the fan is inert at width 0. Balance: pan-seat sum is exactly 0 for n = 2,4,6,8,12,16 with no centre-seat occupant. ./verify full green.

SCATTER'S ROLE in the new mode (human sketch: "scatter can offset them instead of what it does now") is NOT part of this ADR — panScatter still blends toward the legacy image as before. Recorded as the open follow-up: offset pairs together to preserve balance, or per-voice with a re-centre.

MEASUREMENT NOTE worth keeping: three separate false readings preceded the real proof — a manifest.tsv diff (that file has no hash column, so it reported 0 changed), and two scenario parsers that misidentified which scenarios were even-n. The valid detector is sha256 over the .f32 renders. L0016/L0017 again: calibrate on a case the detector MUST fire on.

## ADR-074 · Super-width becomes a 3-mode system — F (clean) default, A/D as documented character — ACCEPTED (revises ADR-025)
The width-lab characterization (docs/reports/2026-08-02-width-characterization.html) and the human's ratified ship list. Width > 1 now selects via `superMode` (id 87): **0 wide (F)** — seat steepening (fan exponent 1/(1+2(w−1))) + per-voice ITD (far channel delayed up to 0.6 ms·(w−1)·2·|pan|, ring 256 samples = 1.2 ms at 192 kHz) — measured the WIDEST of all six candidates (S/M −0.1 dB, corr 0.01) at zero cliffs; **1 pulse (A)** — the original ADR-025 M/S boost verbatim, negative cross-feed and all; **2 smear (D)** — allpass-side variant. A and D are DOCUMENTED CHARACTER: their polarity cross-feed (the up-cliff mechanism, PR #158) is the sound the human kept. C and E retired as subsumed by F. Default 0 is a deliberate default-output change at width > 1 versus the old always-A behavior.

L0021 DISCHARGED IN THE SAME CHANGE: waveshape_check gains three gates — F at width 1.5 MUST be clean (0 cliffs; got 0), A and D MUST cliff (pinned: 1,867 and 14,300 — if a future change silently linearizes a character mode, the pin fires and the change owns up). Width ≤ 1 bit-untouched: parity 147/147 after the fold, worst rms unchanged (4.262e-09, pre-existing dyn-ring).

Known limits, recorded: mode F costs ~1.3 dB more mono-fold loss than gains-only widening (Haas combing — the standard cost of every delay widener); ITD max is a fixed 0.6 ms audition value (a trim knob can come later without ids churn); mode D's allpass corner is fixed at 700 Hz likewise.

## ADR-075 · Opt-in 2x oscillator oversampling — ACCEPTED (superset, bounded claim)
Closes the HF-droop thread (measured 2026-07-31/08-03). The voice sum runs twice per output sample at half the phase step and feeds a 63-tap windowed-sinc halfband decimator (cutoff 0.235 of the 2x rate); everything after the sum — output pole, envelope, ITD head — stays at 1x. Param `oversample` (id 88), **default OFF and bit-exact**: the ADR-063 precedent, so all 147 goldens and every saved session are untouched.

MEASURED IN-CORE (single saw, bin-commensurate f0, droop vs the ideal 1/k law): 5 k −0.60 → −0.52 · 10 k −2.17 → **−1.23** · 15 k −4.50 → **−2.13** · 20 k −7.56 → −5.65. CPU at 8 notes x 16 voices: 2.5% → 6.3% of one core — comfortably inside the E-6 envelope (<50%).

BOUNDED CLAIM, and an honest gap: the JS spike predicted −0.83 dB at 15 kHz; the real core reaches −2.13. The residual is NOT the decimator — the likely source is additional in-core filtering the spike did not model (the R→tone output pole runs on the summed signal at 1x). An isolation measurement was attempted and abandoned when the harness kept mangling; recorded as OPEN. So the shipped claim is "roughly halves the droop through 15 kHz", NOT "flat to 15 kHz" as the spike suggested. 20 kHz stays down: at 0.91x Nyquist it lives inside any decimator's transition band, and 4x OS is the only way to clear it (~2x again the CPU).

L0021 DISCHARGED IN THE SAME CHANGE: waveshape_check gains (a) an OS-off determinism gate (worst |diff| = 0 across independent constructions — catches state bleed from the new decimator members) and (b) a recovery gate asserting 2x lifts the 15 kHz harmonic by >= 1.5 dB (measured +2.37). Harmonic LEVEL by Goertzel at bin-commensurate f0 is valid here; L0017's trap was sparse probes for ALIASING, where folded products miss the grid.

## ADR-076 · Poly glide — every new voice bends in from the last-played pitch — ACCEPTED (superset, opt-in)
Human request 2026-07-31 ("poly glide ... as long as it's trivial to implement", plus "an always-on glide that remembers the position of the last played note(s) and always begins with a bend"). It was in fact small: the core already had per-swarm glide machinery from the ADR-026 mono retarget, so poly glide is a core+shell change — seed the new voice's f0/f0cur from a remembered `lastNoteF`, set glideTarget, reuse the existing per-tick glide. The `glide` TIME knob (id 33) is shared and consequently stops being mono-gated in the GUI.

DELIBERATELY MEMORY-BASED, NOT VOICE-BASED: `lastNoteF` persists across silence, so the first note after a rest still bends in from wherever you last played — the human's "always begins with a bend", not merely legato-when-overlapping. Mono retargets refresh the memory too, so switching modes stays coherent.

`polyGlide` id 89, default 0 and bit-exact (ADR-063 precedent). MEASURED (A2 -> A3, glide 0.30 s): off -> the new note starts at 220.0 Hz; on with glide 0 -> 220.0 (the time knob still gates it); on with glide 0.30 -> starts 110.0, 50 ms 125.8, 300 ms 179.8, 1.5 s 219.2. parity 147/147, verify full green.

GLIDE SOURCE IS A MODE, not a behaviour (human, 2026-08-03: "memory-based glide is a function I want access to but I don't always want it"). `glideMode` id 90: **0 = held note (legato)** — bend only while another key is still HELD, which is exactly the rule the mono path has used since 2026-07-18, and the default; **1 = last note (memory)** — always bend from the last-played pitch, even after a rest. MEASURED (A2 then A3, glide 0.3 s; 220 = no bend, 110 = bends in): legato 110.0 overlapping / 220.0 after a rest; memory 110.0 / 110.0. The held check runs BEFORE alloc(), since alloc can steal a still-gated voice and asking afterwards would misreport.

DEFERRED, honestly: the chord case. `lastNoteF` is ONE frequency, so a chord's voices all bend from the same pitch rather than each from its own nearest predecessor — which the human's "last played note(s)" anticipated. That is a bigger design (voice-to-predecessor assignment), left as a follow-up rather than smuggled in half-done.

## ADR-077 · Ensemble onset timing folded into the core — Vorberg/Wing correction, not jitter — ACCEPTED (superset, increment 1)
The 2026-07-28 research headline (LIBRARY L0019) reaches the instrument. Voices enter at different times, and those times come from MUTUAL ERROR CORRECTION — `off_i <- off_i − alpha·(off_i − mean_off) + motorNoise_i` — because listeners judge ensemble togetherness from the SERIAL STRUCTURE of asynchrony, not its variance. Params: `onsetScatter` id 91 (motor-noise sigma in ms; **0 = off = bit-exact**, the master switch), `onsetAlpha` id 92 (correction gain, default 0.25 — the measured near-optimal for real quartets), `attackScatter` id 93.

MEASURED FROM THE FOLDED CORE, and it reproduces the research's regimes: alpha 0 → onset SD 202 ms, lag-1 **+0.985** (random walk, drifts without bound); alpha 0.25 → SD 35.8 ms, lag-1 **+0.679** (bounded WITH structure); alpha 1.0 → SD 26.4 ms, lag-1 **−0.072** (i.i.d. — precisely what conventional humanize produces); alpha 1.5 → lag-1 **−0.550** (over-correction, alternating early/late).

ARCHITECTURE NOTE, honestly: the lab gives every voice its own full ADSR; the core's envelope is PER-SWARM. Rather than rewrite the envelope architecture (large, parity-risky), this increment adds a per-voice ENTRY: a voice holds silent — and does not advance its phase, since it has not started playing — until its offset elapses, then fades in on its own attack coefficient so a late entry cannot click in at whatever level the shared envelope has reached. Per-voice release/decay shaping is NOT folded and remains lab-only; that is increment 2 and needs the envelope rework.

L0021 DISCHARGED: waveshape_check gains a STRUCTURE gate — lag-1 must fall monotonically with alpha (a0 > 0.9, a0 > a.25 > 0.4, a1 < 0.2). Deliberately not a variance test: a regression that silently turned this into per-note jitter would still "scatter onsets" and pass any variance check; only the ordering catches it. Parity 147/147 unchanged, `./verify full` green.

## ADR-078 · Per-voice envelopes — ACCEPTED (superset; ADR-077 increment 2)
Human, 2026-08-03: the ensemble timing sounded right across the board, and per-voice envelopes "would allow for some very interesting randomization and voice-state-based modulation down the line" — the second reason is the stronger one, since per-voice state is what a mod matrix wants as a SOURCE. `voiceEnv` (id 94) gives every voice its own ADSR; `relScatter` (id 95) spreads release times the way attackScatter spreads attacks, so a chord decays as players rather than as one gate. Default off = the shared-envelope reference path, bit-exact.

DESIGN: the shared `s.env` is NOT removed — it becomes pure BOOKKEEPING, tracking the loudest voice (`s.env = max(onsE)`). Every liveness test, voice-steal decision and NOTE_END emission keys off s.env, so all of that machinery keeps working unchanged while the AUDIO is enveloped per voice inside the loop. That is why this fold did not have to touch the note-lifecycle code at all — a rewrite there would have re-opened the stuck-note surface we spent three rounds closing.

BUG CAUGHT BY TEST, NOT INSPECTION: the first build computed the per-voice attack/release coefficients only inside the ADR-077 `onsetScatter > 0` branch, so enabling voiceEnv alone left every coefficient at 0 and the swarm rendered SILENCE. The probe found it immediately; the coefficient setup now runs whenever either feature is on, with only the Vorberg/Wing timing correction still gated on scatter.

MEASURED: voiceEnv on with scatter 0 — audible (peak 0.211) and per-voice spread exactly 0 (uniform, as it must be when nothing is scattered); scatter 0.8 — spread 0.237 at 150 ms into the release, i.e. voices genuinely releasing at different rates; both cases reach true silence (106 / 174 blocks). Gates pin all four properties. Parity 147/147, `./verify full` green.

FORWARD: per-voice envelope level is now a natural mod SOURCE (the human's stated motivation). Wiring it into the matrix belongs with that work, not here.

## ADR-079 · NOTE_END must survive a rejected push — ACCEPTED (the real stuck-note bug)
Four rounds of stuck-note work fixed real bugs and never reached this one. The human's clue closed it: "voices stall ... most when I've recently changed the K value", with release/attack/decay at minimum so the envelope was excluded by construction. Measured first: the CORE is innocent — tail after note-off is 46.4 ms at K 0.0 steady, K 0.9 steady, K 0.9→0.0, K 0.0→0.9, and at rtone ±1. Identical everywhere, exactly what a 5 ms release predicts.

THE BUG: `emitNoteEnds` called `out->try_push(...)` and IGNORED THE RETURN VALUE, then retired the tag (or cleared the pending queue) regardless. `try_push` can legitimately fail — the host's output-event buffer is finite, and `drainQueue` floods that same buffer with outgoing param events every time a knob moves. So sweeping K could crowd out a NOTE_END, which was then destroyed forever: Live never learned the note ended and withheld retriggering that pitch until something else cleared its table. That is precisely the reported signature, including why it correlated with knob movement and why an arpeggiator "fixed" it.

FIX: both emission sites now retire only on ACCEPTANCE. Rejected ends stay queued (the pending array is compacted in place) or leave their tag active, and the next block retries — the note resolves late rather than never.

ORACLE: endprobe gains a REJECTING HOST — try_push refuses the next 40 pushes, as a full buffer does. Proven discriminating by running it against the pre-fix code: **old 0 NOTE_END delivered, new 1**. A test that does not fail on the old code proves nothing, so that check was run deliberately.

LESSON (candidate for LIBRARY): an API that returns a success flag is telling you it can fail. Ignoring it converts a transient, recoverable condition into permanent silent data loss — and the loss is invisible precisely because the failing path is rare and load-dependent, so it presents as intermittent flakiness rather than a bug.

## ADR-080 · FX rack: a per-slot SECOND axis; comb resonance folded (2026-08-03) — ACCEPTED
Human, on the comb: *"Wasn't there a second slider on the comb in the lab?"* — correct. `detune-lab.html` has **comb mix** and **resonance**; only mix was folded. ADR-071 hardcoded `fb = 0.79` and said so explicitly: *"resonance fixed at the lab default until the rack grows per-slot param pages."* This is that page.

**One generic axis, not a comb param.** New ids **96–99** are `fx1tone..fx4tone` — a second knob on *every* slot, so the next slot type wanting a second control costs no new ids and no new concept. Only Comb reads it today; the others ignore it. The alternative — a comb-specific `combRes` id — would have to be repeated per type forever, and would sit dead in the param list whenever the slot is anything else.

**Bit-inert by construction.** Comb resonance uses the lab's own law, `fb = 0.6 + 0.38·tone`. Default `tone = 0.5` gives exactly `0.79`, the value ADR-071 hardcoded — so parity stays 147/147 and **every saved state loads unchanged** (a missing key reads the default, which is the old behaviour).

**Gated, because an inert default is exactly how a dead control hides.** Parity staying green proves only that 0.5 changes nothing; it says nothing about whether the knob is connected. The FX dropdowns shipped Comb *unreachable* for precisely that reason and no oracle saw it. `waveshape_check` T7 therefore measures ring time to −40 dB across the range: **99 / 190 / 772 ms at tone 0.1 / 0.5 / 0.9**, required strictly increasing.

**Interface note.** Ids 96–99 are additive and append-only, pre-1.0 — the same shape as ADR-071's widening of 57/59/61/63 and ADR-078's 94/95.

## ADR-081 · Envelope cluster owns per-voice envelopes + scatter; envelope display added (2026-08-03) — ACCEPTED
Human: *"the per voice envelope should go in the envelope tab. Maybe also the scatter controls, and we can add an envelope visualizer which also shows the variation from scatter?"*

**Moved** ids 91/92/93/94/95 (onset scatter, timing correction, attack scatter, per-voice envelopes, release scatter) from **Drift** to **Envelope**. The resulting split is cleaner than the one it replaces: **Drift = pitch variation, Envelope = amplitude/time variation**. Onset scatter was the debatable one — it is *timing*, not envelope shape — but it moves the moment each voice's envelope begins, so it belongs with the display that shows exactly that, and separating it from attack/release scatter would scatter one idea across two clusters.

**The display is fed by the ENGINE, not re-derived.** New viz fields `envOnsetMs/envAtkMs/envRelMs` publish, per voice, the times the core actually assigned — recovered from the one-pole coefficients it is using (`t = −1/(sr·ln(1−c))`). The scatter draws from the core's seeded RNG stream, so a JS reconstruction would be a **second implementation free to drift from the one you hear** — the display would eventually lie, and lie plausibly. Requires one new core array, `onsD0` (the initial onset delay, kept because `onsD` decrements to zero); viz-only bookkeeping, never read in the audio path, so parity is untouched.

**Why a visual at all:** the standing convention recorded the same day — lab visuals ship with the feature they explain. Horizontal offset between traces *is* onset scatter; differing slopes *are* attack/release scatter. A numeric readout accompanies it, so the effect is legible without playing a note.

## ADR-082 · Multi-oscillator architecture: param-id namespace, CPU budget, per-osc state — ACCEPTED (ratified 2026-08-06: **2 oscillator slots**)

**Why this blocks the interface renovation.** The layout lab's own Decision 2 says it: 2-3 full
oscillators is not a GUI change, it is N core instances with per-osc params, per-osc state,
multiplied CPU, and a param-id namespace. **CLAP ids are append-only**, so the namespace is
designed once or lived with forever. Every other renovation item (osc page, morph page,
per-parameter corner colouring) sits downstream of it.

**Measured context.** 99 params today, ids **1..99, densely packed with no gaps**
(`src/hypersaw_clap.cpp` `kParams[]`). Classifying them: roughly **70 are per-oscillator** —
the entire swarm surface (n, dist, seed, detune, law, K, onset, dissolve, drift, topo, reach,
mu, alpha, poles, grav, basin, the SPECTRA block 44-51, shape, tone, pan/spread/anchor 76-87,
the scatter/env block 91-95) — and roughly **29 are global**: output/image (width, mono, vol,
bassMono, bassMonoHz), voice & glide behaviour (voiceMono, glide, legato, polyGlide, glideMode,
freqGlide, pitchBend, inertia, inertiaCurve), the FX rack (57-64, 96-99), tempo grid
(beatMult), and oversample.

### Decision 1 - id scheme: a fixed +100 stride per oscillator

    id(P, osc k) = id(P, osc 0) + 100k

Oscillator 0 **keeps every id it has today**, so all existing sessions, automation lanes and
patches survive untouched. Oscillator 1 occupies 100-199, oscillator 2 occupies 200-299. Where
a global param's id would fall inside a block, the slot is simply never allocated - the gaps
are self-documenting evidence of which ids are global.

*Rejected - re-homing all per-osc params into clean blocks.* Correct-looking and impossible:
ids are append-only, so moving osc 0 breaks every saved session.

*Rejected - one param set plus an "edit target" selector.* Cheap in the GUI and wrong for a
plugin: automating oscillator 2's detune becomes impossible, because the lane's meaning depends
on a selector position. Per-osc automation is table stakes.

### Decision 2 - CPU budget, and a constraint that falls out of it

Current measured cost is **2.5% of one core at 1x, 6.3% at 2x oversampling** (ADR-075, on this
M3), against the **E-6 budget of 50% of one core on min-spec**. The voice loop scales linearly
with oscillator count, so:

| config | this M3 | x4 min-spec derate (the ADR-018 precedent) |
|---|---|---|
| 3 osc, 1x | ~7.5% | ~30% - inside |
| 2 osc, 2x OS | ~12.6% | ~50% - at the line |
| 3 osc, 2x OS | ~18.9% | **~75% - over budget** |

**So "3 oscillators" and "2x oversampling" cannot both be unconditional.** Proposed: ship 3
oscillator slots, keep oversampling opt-in and global, and **measure on real min-spec hardware
before increment 2** rather than trusting a derate factor borrowed from a different workload.
The derate is an estimate and this table is arithmetic on top of it - it is a reason to
measure, not a verdict.

### Decision 3 - per-osc state keys

State is `hypersaw-state 1` followed by `coreKey=value` lines. Proposal: per-osc keys for
k >= 1 gain an `o<k>.` prefix (`o1.detune=...`); **oscillator 0's keys are unchanged**, so every
existing patch loads bit-identically and `state_check` stays green as the regression proof.
Format bumps to `hypersaw-state 2` while still accepting `1` (absent `o1.`/`o2.` keys => those
oscillators at defaults, i.e. silent). "Copy osc 1 -> osc 2" becomes a key-prefix rewrite.

### Increments (the walking-skeleton order the layout lab recommends)

1. Id scheme + state format land with **N = 1 still**, so nothing audible changes and the
   parity/state oracles prove the refactor inert.
2. A second oscillator behind the existing `balance` param - thin, no GUI.
3. GUI: osc page with per-parameter corner colouring and glyphs (the standing convention).

### Ratification (human, 2026-08-06)

**Two oscillator slots**, on the reasoning that SAW alone does not need three. Ids therefore
occupy 1-99 (osc 0) and 100-199 (osc 1); the 200-299 block stays unallocated and available if a
third slot is ever wanted.

**CORRECTION — the sub/balance question was withdrawn, because it rested on a false premise.**
The ADR asked whether the sub-oscillator block (52-55) and `balance` (56) are "superseded by
real oscillators". Checking the code rather than the layout lab's prose:
- **`balance` (56) is not an oscillator mixer.** It is the two-cluster A/B *coupling* balance
  inside the SAW swarm — `kB = 1 - 2*balance` (ADR-051) — an intra-swarm parameter.
- **The sub (52-55) is SPECTRA-only**; it exists in `spectra_core.h` and `swarm_core.h` has no
  sub at all.

Neither is superseded by anything. Both are ordinary **per-oscillator** parameters and get
blocks like every other per-osc param (osc 1: 152-156). No deprecation, no deletion, no
decision required. The question came from the layout lab's shorthand ("ONE engine select + A/B
balance + sub") being read as an architecture description when it was a sketch.

**Oversampling stays GLOBAL** (param 88). With two slots the derated estimate is ~20% at 1x and
~50% at 2x — the latter sits exactly on the E-6 budget line, so making oversampling per-osc
would multiply the one number already at its limit for no expressive gain. The min-spec
measurement required before increment 2 stands.

### Amendment 1 (2026-08-06) — stride 100 → 1000, and `vol` is per-oscillator

Found while starting increment 2, before anything was built on the scheme. Both defects are
in the ratified text above; both were free to fix at that moment and would have been permanent
a week later.

**(a) The stride is also the CAPACITY of oscillator 0's block — and it was full.** With stride
100, osc 0 owns ids 1..99. The instrument has **99 params with zero free slots**. A new param
would need id 100, and `findParam` computes `osc = id / kOscStride`, so id 100 resolves to
oscillator 1 / base 0 and is **never found** — the param would not be cramped, it would be
silently unreachable. Stride 100 therefore capped the instrument at 99 parameters *forever*,
and it was already at the cap on the day it was ratified.

**Stride is now 1000**: osc 0 = 1..999 (900 free slots), osc 1 = 1000..1999, osc 2 =
2000..2999. Every existing id is unchanged. This costs nothing **only because increment 1
shipped at `kNumOsc = 1`** — no id ≥ 100 has ever been exposed to a host, so no session,
automation lane or patch can reference one. After the first 2-oscillator build ships, this
amendment becomes impossible.

**(b) `vol` (17) was misclassified as global.** It is the swarm's *own* output gain, computed
inside `SwarmCore::render` (`gain = p.vol * 0.9 / n^normExp`), so it belongs to the oscillator.
Leaving it global would give two oscillators one shared gain and **no way to balance them** —
which is precisely the control increment 2 exists to provide. It is now per-oscillator. A
patch-level master volume, if wanted, is a separate new param; the stride amendment leaves
room for one.

**How this was caught, since the process point generalises:** not by review of the ADR, but by
starting to build the increment it authorised and finding there was no mixer. The
`balance` (56) param that the original increment-2 sketch named as the place to hide a second
oscillator is the two-cluster *coupling* balance (ADR-051), not a mixer — a correction already
recorded in the ratification note. Following that thread is what exposed both defects. An ADR
reads as complete right up until you try to execute it.

**Verified:** `./verify full` GREEN at `kNumOsc = 1` (parity 147/147, worst 4.262e-09 —
unchanged), and calibrated at `kNumOsc = 2` where `state_check` passes in full, including the
version assertion and the `o1.` key round-trip.

### Per-osc / global split (the part that is baked in forever)

Everything not listed here is **per-oscillator**. GLOBAL params, by group:

| group | ids |
|---|---|
| output & image | 14 width, 15 mono, 17 vol, 40 bassMono, 41 bassMonoHz |
| amp envelope | 19 attack, 20 decay, 21 sustain, 22 release |
| voice & glide | 32 voiceMono, 33 glide, 34 voiceLegato, 38 pitchBend, 75 freqGlide, 89 polyGlide, 90 glideMode, 11 inertia, 70 inertiaCurve |
| FX rack | 57-64, 96-99 |
| global misc | 23 beatMult, 88 oversample |

**Three entries are judgement calls, flagged rather than buried** — if any is wrong it is wrong
permanently: (a) **amp envelope 19-22 global** — conventional (the *voice* has an amp envelope,
not each oscillator), and SPECTRA already has its own env at 65-68; (b) **transpose 35 octave /
36 semi / 37 fineCents per-osc** — this is much of the point of a second oscillator (an octave
down replaces what a sub would do), so they move into the per-osc class; (c) **18 retrig and 74
keepPhase per-osc** — phase behaviour is a property of the oscillator, not the note.

## ADR-083 · Voice steal: three-tier policy — releasing tails before held notes — ACCEPTED (deliberate divergence)

Human report (2026-08-08): a sustained note under a running arpeggio is eventually stolen.
Measured mechanism: a released tail occupies its slot until `env < 1e-3` (~1.1 s at the 0.16 s
release), so a 9-note/s arp keeps ~10 tails alive; when the 16-slot pool fills, the reference's
steal-oldest policy sacrifices the OLDEST voice — which is precisely the note being deliberately
held. Reproduced with a Goertzel probe at the held note's f0 (long windows + a no-arp control,
after a short-window first attempt measured the sustain's own 65 Hz beat nulls): stolen ~11 arp
notes in, power to 7% of the beating floor.

**Decision:** `alloc()` becomes three tiers — (1) free slot, oldest first (as the reference);
(2) **releasing tail, quietest first** (least audible loss; age as tiebreak) — an arp recycles
its own tails and never touches holds; (3) only when every slot is gated, oldest held note
(unavoidable, as the reference). The JS reference keeps its naive policy (protected prototype);
this is a recorded divergence in the overflow regime only. Goldens never overflow the pool, so
parity is the regression proof: **147/147, worst 4.262e-09, unchanged**.

**Verified:** the pre-fix probe run is the calibration (sustain stolen at ~1.2 s); post-fix the
sustain survives the entire 160-note arp with its f0 power never dropping below the no-arp
beating floor. Follow-up queued: fold the arp-sustain scenario into `notefuzz_check` as a
permanent gate.

## ADR-084 · Velocity and MPE pressure → per-voice volume, on by default — ACCEPTED (superset)

Human request (2026-08-08). The synth ignored velocity entirely (the AU-wrapper comment said as
much) and note expressions handled only TUNING. Now: **note-on velocity scales the voice's
gain** (linear, `vel` ∈ [0,1]) and **CLAP `PRESSURE` expressions drive a per-voice gain
target** smoothed over ~20 ms (ADR-009 seconds→coefficient; expression streams arrive at UI
rate and a raw multiply zippers). Pressure re-arms to 1.0 per note, so hosts that never send it
are untouched; MPE surfaces get strike-then-swell behaviour by default.

Superset discipline: `vel`/`press` default 1.0 exactly, the multiply is bit-inert, and the core
`noteOn` signature is unchanged (velocity applied via `setNoteVelocity` after allocation) — so
every golden and every pre-existing caller is byte-identical. **Parity 147/147, worst
4.262e-09, unchanged** is the regression proof. Calibrated through the real CLAP path: vel 0.5
→ rms ratio 0.503; pressure 0.3 mid-note → ratio 0.302 after the smoother settles.

Recorded residuals: SPECTRA ignores velocity still (its own increment); the mono/legato
retarget keeps the original strike's velocity (a retargeted note is the same phrase — revisit
if the ear disagrees); raw MIDI channel aftertouch (0xD0) relies on the wrapper translating to
PRESSURE expressions — verify in a DAW pass. Velocity CURVE (soft/hard) is a future param; the
stride amendment leaves room.

## NOTICE · Library standby (plugin-skeleton FOUNDATIONS) — RECORDED 2026-08-08

Relayed by the human from the library thread. A shared infrastructure library
("plugin-skeleton") is being founded; its FOUNDATIONS document defines contracts this project
will eventually consume (parameter registry, mod routing, scoped presets, event pipeline,
payload interfaces). A mediator agent will open a brief→response dialogue when this repo
becomes the active correspondent; until then **passive standby**: no refactoring toward the
library, no imagined interfaces, no speculative extraction — writes stay home, roadmap
continues as-is. Passive disciplines adopted: stable hierarchical param addresses (record any
rename old→new), no new singletons/global state in components plausibly per-voice or per-module
elsewhere, engine internals behind clean boundaries. Standby artifact:
`INTEGRATION-STANDBY.md` (friction list · component inventory · param+mod architecture
sketch), kept cheap and current — it becomes the first brief when the mediator calls. The
pre-existing donor manifest (`docs/integrations/corelib-insights.md`) predates this notice and
is documentation, not refactoring; the standby artifact references it rather than duplicating.

## ADR-085 — `mpe_check` is a blocking gate in `./verify full`

**Date:** 2026-08-09 · **Status:** accepted (human: *"Gate ratified."*)

**Context.** Eight sites in the CLAP event loop applied per-voice and lifecycle
operations to `core` (the oscillator-0 alias) where the intent was every
oscillator. Every existing gate passed: parity renders a SINGLE core, so a bend
that split the oscillator pair and an all-notes-off that left half the
instrument gated — a stuck note — were invisible to all fourteen other chains.

**Decision.** `tools/mpe_check.cpp` runs in `./verify full` as a blocking gate.
`./verify` is a protected path; this ADR records the human approval required to
edit it.

**Why this gate and not a state assertion.** It drives the real CLAP path
(factory → activate → events → process) and detects via emitted audio with a
single-bin Goertzel. There is no per-voice tuning getter, and adding one to
test with would test the accessor — the `state_check` trap, where a round-trip
through one broken accessor agreed with itself.

**Calibration is part of the decision.** Planted `allOff` → a frozen plateau
(rms 0.142 → 0.142 → 0.142, decay ×1.0); planted tuning and retarget → 49.8%
and 50.0% of the energy stranded at the old pitch. Verified at the DISPATCHER,
not only the probe: `./verify full` exits 1 with the bug planted and 0 restored.

**Consequence.** The fan-out fix is a helper per operation family, so every
future consumer (third oscillator, sub-oscillator, per-voice FX send) re-opens
the whole class until the L0029 routing layer lands. This gate is what makes
that debt visible rather than silent.

## ADR-086 — Consonance gravity integrates on a fixed grid (ACCEPTED)

**Date:** 2026-08-09 · **Status:** ACCEPTED 2026-08-10, ratified after an ear
check ("the two gravity buffers sound nearly identical"). Implemented same day.

### Implementation note — the accumulator alone was not the fix

The first implementation did exactly what this ADR described: accumulate
samples, then `while (accum >= grid) gravityStep(grid/sr)` at the top of
`render()`. The subdivision probe rejected it immediately — still 1.04.
Fixing the step SIZE is insufficient because it leaves the step PLACEMENT
wrong: in one whole call every step fires *before any audio is written*, while
chunked calls spread the same steps through the buffer. Same steps, different
placement, same divergence.

The working fix **segments the render**: `render()` is now a loop that calls a
private `renderSeg()` in pieces bounded by the grid, advancing gravity between
them. Invariance verified at 0.00 for chunk sizes 64…44100 including 333 and
127, which are not multiples of the grid.

### And it moved something it should not have

Segmenting also changed **pan motion** (ADR-064), which is a per-render-call
integrator too — nine SAW parity scenarios went red, against goldens whose
reference (`swarmsaw.html`) this ADR never touched. Pan motion is now hoisted
into `advancePanMotion()`, called once per outer call, so it keeps its
per-call rate exactly. **Gravity was ratified for a fixed grid; pan motion was
not**, and confining the change to what was approved is the whole reason the
hoist exists.

Pan motion therefore remains subdivision-dependent — measured 0.191 at chunk
333 — and that is now a declared, visible exclusion in `subdiv_check` rather
than an unknown. See ROADMAP § pan motion.

**Verified:** `./verify full` GREEN, 15 gates, parity **147/147 worst
4.262e-09**. Golden footprint exactly as predicted: **248 unchanged, 3 moved**
(`dyn-gravity` × 3 seeds).

**Original proposal follows.**

**Date:** 2026-08-09 · Touches a protected path (`swarmdynamics.html`) and
moves one golden.

### Context — a measured defect

`SwarmCore::render()` opens with `gravityStep((double)frames / sr)`. Gravity is
integrated **once per render call, with dt = the block length** — explicit Euler
on a nonlinear ODE (`move = err · rate · dt`, then `f0cur *= 2^(-move/1200)`,
with `err` recomputed from the current `f0cur` each call). One step of dt and
two of dt/2 therefore disagree, so output depends on how a buffer is
**subdivided**, not merely on its total length.

The JS reference has the identical shape: `swarmdynamics.html:405`,
`this.gravityStep(outL.length / sr)`.

**Why no gate caught it.** The golden generator renders a fixed-size buffer and
`parity_check` renders `kBlock` — both sides use the *same* subdivision, so
parity agrees with itself. The defect is invisible to the oracle by
construction, which is the same blind spot the multi-oscillator fan-out bug
exploited.

### Evidence

Bare `SwarmCore`, same seed and notes, 1 s:

| gravity | one whole call vs 256-frame chunks | vs 333-frame blocks |
|---|---|---|
| 0.00 | **0** | **0** |
| 0.50 | **1.028** | **1.029** |

**Correction to the first report of this bug.** That 1.03 was described as "a
different sound". It is not — it is **phase**, not tuning. Re-measured on
`dyn-gravity`'s own settings, the musically meaningful quantity is invariant:

| integration step | interval settles at | waveform rmsDiff / rms |
|---|---|---|
| 2048 samples | 701.931 ¢ | 0.128 |
| 512 (reference) | 701.927 ¢ | — |
| 256 | 701.926 ¢ | 0.022 |
| 128 | 701.926 ¢ | 0.033 |
| 16 | 701.926 ¢ | 0.043 |

Gravity settles to within **0.005 cents** of the same place at every step size
(just 3/2 is 701.955 ¢). What varies is the phase trajectory getting there.
So the bug is a **reproducibility** defect, not a tuning defect — real, but
smaller than first stated, and that distinction should survive into whatever
gets fixed.

### Options

1. **Do nothing; document it.** Cheapest. Leaves renders non-reproducible across
   host buffer sizes whenever `grav > 0.005`, and leaves oscillator 0 (rendered
   in one `n`-frame call) integrating differently from oscillators 1..N (256-frame
   chunks). Rejected: "the sound changes when you change your buffer size" is a
   defect users report and cannot work around.
2. **Fixed accumulator grid at 256 samples** — accumulate elapsed samples, step
   gravity by a constant `dt = 256/sr` whenever a full interval has elapsed.
   **Recommended.**
3. **Fine grid at the existing 16-sample control tick.** Most accurate, aligns
   with `controlTick`. Rejected on measured cost: gravity is O(gated²) per step,
   and with 10 held notes a 16-sample grid costs **+66% CPU** (2.09% → 3.48% of
   one core) to buy a settling difference of 0.001 cents.
4. **Align the shell instead** — render oscillator 0 in `kMixChunk` chunks like
   the others. Makes the oscillators agree with each other but leaves the whole
   instrument dependent on the residual partial chunk, so host buffer size still
   changes the sound. Fixes the symptom we noticed, not the defect.

### Measured cost of the recommendation

10 held notes, 2 s of audio:

| grid | CPU | vs today |
|---|---|---|
| 512 (today, typical) | 41.9 ms (2.09% of a core) | — |
| **256 (proposed)** | **42.7 ms (2.13%)** | **+2%** |
| 128 | 44.9 ms (2.25%) | +7% |
| 16 | 69.7 ms (3.48%) | +66% |

256 samples is 172 Hz — roughly 57 updates across gravity's ~1/3 s settle time,
which is ample for a slow restoring force.

### Consequences

- **One golden moves**: `dyn-gravity` is the *only* scenario in the 147 that
  engages gravity (`grav` defaults to 0 and `gravityStep` early-returns below
  0.005). The other 146 are bit-identical. An earlier note in ROADMAP implied a
  broad re-measurement; that was wrong.
- **Both references change together** — `swarmdynamics.html` (PROTECTED, the
  spec) and `swarm_core.h`. Parity stays green because both sides move; the
  audio for gravity patches changes in phase, not in tuning.
- Oscillator 0 and oscillators 1..N integrate identically afterwards, by
  construction rather than by matching their chunk sizes.
- Unblocks the Tonality gap-24 slice-1 ask: with a stable integrator, swapping
  the ratio table becomes one attributable change instead of two entangled ones.
- `grav = 0` patches are untouched at every step of this.

### What would falsify the recommendation

If a listening pass on `dyn-gravity` finds the phase-trajectory change audible
and *worse* — the settling numbers say the destination is identical, but they
say nothing about the character of the journey, and gravity's whole point is
that settling is an audible physical event (decelerating beating). That is an
ear question, not a numbers question, and it should be answered before this is
ratified.

## ADR-086 Amendment 1 — the gravity grid is a fixed TIME (ACCEPTED)

**Date:** 2026-08-10 · **Status:** ACCEPTED, ratified same day.

ADR-086 shipped `kGravGrid = 256` **samples**. A sample-rate invariance probe,
written within the hour, found that this is a duration that shrinks as the rate
rises — 5.81 ms at 44.1 kHz, 2.67 ms at 96 kHz — so Euler truncation error and
therefore gravity's trajectory tracked the sample rate. Settle time drifted
**+0.42% at 96 kHz, monotonically**. The original fix removed a dependence on
buffer size and left one on sample rate: relocated, not closed.

**No golden could ever have seen this** — goldens are generated at 44.1 kHz
only, so parity is silent about every other rate. This is L0031's thesis
demonstrated on the fix that produced L0031.

**Decision.** `kGravGridSeconds = 256.0 / 44100.0` (5.805 ms), with
`gravGridSamples() = lround(sr * kGravGridSeconds)`. The value is chosen so the
result is **exactly 256 at 44.1 kHz**: verified, **252 goldens unchanged, 0
moved**. After the amendment the settle time is 0.00% / +0.16% / 0.00% /
−0.03% across 44.1–96 kHz — no longer monotonic, the residual being integer
rounding of the grid (279 samples at 48 kHz against an ideal 278.6), which is
inherent and musically nil.

Brings gravity under ADR-009 like every other time constant in the engine.

## ADR-087 — `subdiv_check` is a blocking gate; pan motion is a declared exclusion

**Date:** 2026-08-10 · **Status:** accepted (human ratified).

**Decision 1.** `tools/subdiv_check.cpp` runs in `./verify full`. It asserts
that rendering does not depend on how a buffer is subdivided — **the property
no golden can test**, because the generator and `parity_check` both render at a
fixed block, so both sides share any subdivision bug and parity agrees with
itself. All 147 scenarios passed for the entire life of the gravity bug and
could not have failed. Calibrated: reverting the segmenting gives FAIL at 1.093.

**Decision 2.** Pan motion (ADR-064) remains a **per-render-call** integrator
and therefore subdivision-dependent (measured 0.191 at chunk 333). Ruled: leave
it. The audible stake is near zero — worst-case step is 0.014 on a ±1 pan range,
about 0.06 dB at 21 Hz, below click threshold — and the reference
(`swarmsaw.html`) is a stage that will be graduated rather than patched
piecemeal. `subdiv_check` reports it as **KNOWN**, printed loudly and named in
its summary line, because an undeclared exclusion is how a gate rots into
decoration.

`./verify full` now runs **16 gates**. Parity 147/147 worst 4.262e-09.

## ADR-088 — B23 routing topology: dense crosspoint matrix (ACCEPTED)

**Date:** 2026-08-10 · **Status:** topology ACCEPTED (human-ratified); the id
allocation in §4 **ACCEPTED 2026-08-10** on the specification — see there.
(Header said PROPOSED until 2026-08-18 while §4's own body already read
"Therefore §4 is ACCEPTED": the summary line was not updated when the section
under it was resolved. Caught by triaging the FOUNDATIONS thread that resolved
it. A header that contradicts its own body is the same failure as an exchange
whose frontmatter contradicts its tree — LIBRARY L0037.)

### Decision

HYPERSAW's audio routing is a **dense crosspoint matrix**: every rack slot may
be fed from any source and from any **earlier** slot, each crosspoint carrying a
continuous coefficient, each slot additionally carrying an **initial value**
(`out_i = in_i + Σ g_ki · m_k`, the canonical crosspoint form). A slot that no
later slot reads is an output.

### Why, on our own evidence

`docs/design/routing-lab.html` benched six topologies over identical slots and
sources, with audio, a cost model, and a morph analysis. Two things decided it:

1. **Topology morph.** Quantum morph is a headline feature and a corner
   interpolates *values*. In a dense table a crosspoint at 0 **is** "not
   connected", so connecting and disconnecting are one continuous motion. Every
   sparse scheme stores topology as discrete structure, so adding an edge,
   reordering a chain, or repatching a bus is a **hard cut**. C is the only
   scheme that is simultaneously serial-capable, single-instance, morphable and
   modulatable.
2. **The cost objection was measuring the wrong thing.** Round 1 rejected C at
   "120 params" by conflating patch state with automation ids. Split properly it
   is **88 patch-state values and 8 automation ids** at 4 oscillators × 8 slots.

**Acyclicity by construction, enforced on the read side.** A slot may only read
earlier slots, so one forward pass is always correct and no runtime cycle check
is needed — an audio thread cannot afford one. The legality test lives where
edges are *consumed*, never in the editor: preset load, morph corners and
automation are all writers, so a guard on the write path is bypassable by
construction. (The lab's scheme-D implementation proved this the hard way, and
the principle generalizes past routing to every structure a preset can carry.)

### What this decision is NOT

It is not a bet on the shared library. FOUNDATIONS was asked one doorframe
question and answered that §3.5's chain is "a default shape, not a
constitutional commitment", explicitly declining to choose a topology and asking
not to be cited as design input. Its only role here is **removing the retrofit
risk**: divergence between HYPERSAW's topology and any future library shape is
information, not debt.

### §4 — Id allocation (ACCEPTED 2026-08-10, on the specification)

**Resolved 2026-08-10 by reading the specification we vendor.** This section
originally opened *"CLAP ids are append-only, so this cannot be unmade."* That
sentence is wrong in **both** directions at once, and FOUNDATIONS caught it by
citing `libs/clap/include/clap/ext/params.h` — our own tree — so it could be
checked without taking their word. Verified verbatim:

- **`params.h:212`** — *"Stable parameter identifier, it must never change."*
  The id-stability half is **stronger** than claimed: append-only is **mandated
  by specification**, not a convention we adopted for safety. The block
  allocation needs no further permission from anyone.
- **`params.h:70-77` ("VI. Adding or removing parameters")** — the parameter
  **set** is explicitly revisable: `restart()` → deactivate → apply → `clear(host,
  param_id, CLAP_PARAM_CLEAR_ALL)` for any id gone or reused → `rescan(ALL)`.
  The "cannot be unmade" half is simply **false**.
- **`params.h:328`** — *"It can only be used while the plugin is deactivated."*
  There is no mid-session rescan to survive; the question we were both asking
  contained a false premise.

**The accurate claim, which is weaker and truer:** *ids are append-only by
specification; the parameter SET is revisable through a documented
deactivate/rescan cycle whose host-side automation behaviour is unmeasured.*

**What this opens.** "Params that exist only when their rack does" is a flow
CLAP documents, not a door we assumed shut. It is not adopted here — a static
block is simpler and the host behaviour is unmeasured — but it is a **live
option** for a future rack rather than a foreclosed one, and that is worth
knowing before a second rack exists.

The original framing cited FOUNDATIONS **ROADMAP open question #15** — *"can shipping
hosts survive `rescan(CLAP_PARAM_RESCAN_ALL)` mid-session with automation lanes
intact? Decides whether the shell needs a bounded macro/proxy surface (five
vendors' answer) or can expose dynamic params (CLAP's promise, unverified). Runs
before any shell phase relies on either answer."*

HYPERSAW holds a `clap_host_params_t *` and **has never called rescan**. Our
param list is static for an instance's life by assumption, never by measurement.
So the "permanent" framing is the CONSERVATIVE reading, not a verified one.

**What survives the uncertainty, and what does not:**

- **The block allocation is safe under BOTH answers** and is therefore still
  proposed: a clean block costs nothing if ids turn out to be revisable, and
  saves everything if they do not. Hygiene either way.
- **The justification does not survive**, and matters beyond this ADR: if hosts
  do survive a rescan, HYPERSAW gains an option it has been treating as
  foreclosed — params that exist only when their rack does, rather than a
  statically allocated block sized for the worst case. That is a different
  design, not a tidier version of this one.

**Therefore §4 is ACCEPTED**, on the specification rather than on the
measurement. The residual empirical question — what a host does to an automation
lane across the documented restart cycle — is narrowed, no longer blocking, and
carried separately (`offer-param-rescan-spike.md`, accepted by FOUNDATIONS with
two amendments: cases must run the legal cycle rather than an out-of-spec live
rescan, and the clap-wrapper VST3 row is to be protected ahead of extra hosts,
since most hosts meet us through the wrapper rather than through CLAP).

**Process note worth keeping.** We held §4 on a library open question that was
filed "pre-shell" — i.e. scoped against *their* phases, not against the fact
that it gated a consumer's current work. Their words: *"a phase gate that
ignores its correspondent's blocker is not a gate, it is a schedule."* The
lesson cuts both ways: we also sat on it rather than asking, and it surfaced
only because the human noticed the two threads were the same question.

### The proposal itself, unchanged

- **Topology is patch state, not CLAP params.** Crosspoint on/off is discrete
  and nothing needs to automate it; morph corners are internal snapshots and
  reach it without host ids. Only the continuous quantities get ids.
- **Routing gets its own stride block: `10000 + rack*1000 + local`.** Rack 0
  occupies 10000–10999. Rationale: ADR-082's amendment had to move once already
  because a flat space ran out on the day it was needed, and stride IS capacity.
  Oscillators own 0–2999 (`kOscStride` 1000, third slot reserved); 3000–9999
  stays unallocated so a future block does not have to squeeze.
- **Today's id cost is 8 per rack** — per-slot amount ×4, per-slot initial
  value ×4.

**Not implemented.** This ADR records the ruling; the increment that builds it
is scoped in ROADMAP.

### Amendment 1 — the dense table's justification is now BOTH morph laws (2026-08-27)

**Queued at ADR-125 and executed here**, with its own reason stated at the time:
*"or the next agent re-derives the wrong thing from a stale why."*

The original rationale rests entirely on continuity — *"a coefficient of 0 IS
'not connected', so connecting and disconnecting are one continuous motion"* —
which reads as though the dense table exists to serve a blended morph. **ADR-125
then ruled ARGMAX the default topology law**, under which topology does *not*
interpolate: the route coefficients draw one corner between them and the chain
order flips discretely.

That does not weaken the choice, and the amendment is the point: **the dense
table is the substrate for BOTH laws, and its value under the ruled default is
different from the one recorded.**

- Under **BLEND** (the retained option), continuity is the argument as written.
- Under **ARGMAX** (the default), the table earns its place for a different
  reason: a coefficient that can be *any* value is what lets a flip be **glided
  through zero** rather than cut, so even a discrete topology change has a
  continuous path available to it. A sparse edge list offers no such path — an
  edge either exists or does not, and there is nothing to ramp.

So the honest one-line justification is now: *dense because a coefficient of zero
is a legal, reachable, ramp-through-able state* — which serves continuity when
blending and de-clicking when flipping. The cost argument (88 patch-state values,
8 automation ids) is unchanged and still merely fails to disqualify it.

**Also unchanged and worth restating here, since ADR-128 widened the matrix:**
read-side legality is still what makes one forward pass correct. Backwards edges
are now legal but read `zPrev`, so the single-pass property the original ADR
bought is intact rather than traded away.

## ADR-089 — Gate changes are asymmetric: adding is delegated, weakening is not

**Date:** 2026-08-11 · **Status:** accepted. Human: *"I'm just going to have to
trust your judgment on those gates because I don't really have any insight into
them."*

### The problem with taking that at face value

The charter requires a human decision before "editing `./verify` or the gates it
runs". Its purpose is that **a weakened gate is invisible** — nothing goes red,
the suite still prints GREEN, and the loss is silent. A human who cannot inspect
the gate cannot supply that check, so accepting the delegation unchanged would
remove the safeguard rather than relocate it.

### The asymmetry the charter does not draw

Two operations are covered by one rule and they carry opposite risk:

| operation | effect on the suite | who decides |
|---|---|---|
| **adding** a gate | strictly stricter — can only turn green→red, never red→green | **agent may proceed**, with calibration recorded |
| **weakening** a threshold, **skipping**, **xfail**, **removing**, or **narrowing** a gate | strictly looser — can turn red→green silently | **always the human**, no exceptions, no delegation |

Adding a gate cannot hide a regression; it can only expose one or cost time.
Every failure mode the original rule protects against lives on the second row.

### Conditions on the delegated half

A gate may be added without asking only if all hold, and each is recorded in the
gate's own source so a later reader can audit without re-deriving:

1. **Calibrated** — proven RED on the defect it exists for and GREEN on restore.
   A gate never shown to fail is decoration.
2. **Threshold from measurement**, with the regression's magnitude on record.
   `samplerate_check`'s 0.3% sits between a measured 0.163% and 0.419%; a 1%
   tolerance would have passed its own defect.
3. **Vacuity control** — a case that must read exactly zero, or a distinctness
   check, so it cannot pass with the feature absent.
4. **Deterministic**, and cheap enough to state (`routing_check` 0.00 s,
   `samplerate_check` 0.02 s against a 3–4 minute suite).
5. **Exclusions declared loudly** in the gate's own output — `subdiv_check`
   prints pan motion as KNOWN and names it in its summary line.

### Applied

`samplerate_check` and `routing_check` are gated. `./verify full` runs **18**
gates.

### Not amended: the charter itself

`CLAUDE.md` above §Domain is the invariant harness layer and is explicitly not
edited per-project. This ADR records a project-level working agreement about how
that rule is applied here; it does not rewrite the rule. If the distinction is
worth having everywhere, it belongs upstream in the doctrine repo, not in this
file.

## ADR-090 — Per-oscillator pan is an IMAGE SHIFT, not a balance (ACCEPTED)

**Date:** 2026-08-11 · **Status:** ACCEPTED (human-ratified). Not yet
implemented; it is parity-affecting and scoped below.

### Decision

A per-oscillator pan control **offsets every voice's seat** in that
oscillator's stereo field. It is not a mix-stage balance.

### Why, and why the cheap option was rejected

The cheap option is a balance at the mix stage: `gL = min(1, 1-pan)`,
`gR = min(1, 1+pan)`. Exactly unity at centre, never boosts, zero parity risk,
shell-only. It is what a mixer does to a stereo source.

**But HYPERSAW's stereo image is GENERATED, not recorded.** Voices are *seated*
across the field by `panLayout` / `panScatter` / `panCurve`. Attenuating one
channel does not move that image — it **deletes the far side of the ensemble**.
Hard-panning a swarm under a balance law silences the voices seated opposite,
which is a different instrument, not a different position. Balance is the right
law for material that arrived stereo; this material did not.

### Consequences, honestly

- **Parity-affecting.** Seats live in `swarm_core.h`, and the reference
  (`swarmsaw.html`, PROTECTED) must move with it. Goldens that engage pan will
  re-measure.
- Composes with the existing seat laws rather than fighting them: a shift is
  applied to `panBase` before `panScatter`/`panCurve`, so the image's *shape*
  is preserved and only its centre moves.
- Edge clamping is the open sub-question: a shifted seat can exceed ±1. Clamp
  (voices pile at the edge, image compresses) or wrap (voices reappear on the
  far side, image tears). **Recommend clamp** — compression is a musical result,
  tearing is not.

### Scope

Its own increment, not folded into B23's. New per-oscillator param, seat offset
in the core, reference edit, goldens re-measured, and a probe proving the far
side survives a hard pan — which is exactly what a balance law would fail.


## ADR-091 — Engine family expansion: SAW renamed HYPERSAW; a hyperpop-oriented experimental engine track opens with the formant engine (ACCEPTED)

**Date:** 2026-08-17 · **Status:** ACCEPTED (human direction). Documentation,
ingestion and label rename done in this change; engine implementation is
roadmapped, not started.

### Decision

1. **The engine formerly named SAW is named HYPERSAW.** Label only: `kEngineLabels[0]`,
   the GUI's visible string, and the engine-name mentions in SPEC/ACCEPTANCE. The
   enum value (0) and the state key are untouched, so no saved patch changes.
2. **The instrument's sound-design space widens** from "the supersaw taken
   seriously as physics" to a *family* of experimental, responsive engines with
   dynamical characteristics, oriented toward the sounds of hyperpop. HYPERSAW and
   SPECTRA are the first two members; each new engine follows the same discipline —
   a browser prototype that is the oracle, a `SPEC-*.md`, seedable determinism,
   parity as correctness.
3. **The first new engine is the formant engine** (working name CANTO): FOF /
   pulsar grain synthesis where the fundamental is a firing rate, formants are
   grains, every continuous control is a mass on a spring, and one hidden register
   state R reshapes the whole engine as pitch descends. Ingested today as
   `horde_formant_pulsar_fof.html` (prototype, `FormantCore`) and `SPEC-FORMANT.md`
   (spec v0.1, moved from `HORDE_formant_engine_spec.md` into the `SPEC-*.md`
   convention so it is found where the other engine specs are found).

### Triage of the ingested prototype — what it is and is not yet

- **It has a separable core** (`class FormantCore` with a headless `render(out)`),
  which is what makes it a candidate oracle at all (ADR-003 shape).
- **Its register formulas match the spec exactly** (`bwk`, `reg.shift`, `tilt`,
  `tex`, `sub` — checked line for line).
- **It is NOT yet a valid oracle**, for one reason: masking uses `Math.random()`.
  The spec (§9) already requires a seedable RNG for parity; the prototype does not
  have it. Under this project's determinism invariant (mulberry32 streams only,
  SPEC §5.7) this is a blocker for golden generation, not a nit. **Fixing it is a
  spec-preserving edit** — a seeded stream substituted for `Math.random()` — and is
  the first roadmapped item.
- Both `performance.now()` reads are UI-side (event-train display, a 1.5 s meter
  window), not in the DSP path. Fine.
- It links a Google font (`<link href="https://fonts.googleapis.com/...">`). Not
  blocking for a root prototype; must be inlined or dropped before any lab copy
  (self-contained rule) or webview embedding (CSP).
- It does not load in `lab_load_check` (bare `devicePixelRatio` in a canvas `fit()`
  helper). Root prototypes are not swept today, so this gates nothing now; it is
  recorded so the lab copy fixes it on arrival.

### Consequences

- The prototype and spec are **protected paths** from this ADR on, with the same
  meaning as the other seven: an edit is a spec change. The one edit already
  sanctioned is the seeded-RNG substitution above.
- Corner colour, morph, mod-matrix and presentation-table work all assumed one or
  two engines; a family means the presentation table's `scope` prefixes and the
  engine selector's real-surface gating both grow. That is the design work, and it
  is on the ROADMAP rather than implied.

## ADR-095 — The FX rack owns dry/wet (ACCEPTED)

**Date:** 2026-08-19 · **Status:** ACCEPTED (human approved the contract 2026-08-15;
this is its rack-side half). The gate half landed first, deliberately.

### The rule

```
out = (mix == 0) ? in : lerp(in, wet, mix)
```

Applied by the RACK to every slot type. `mix == 0` is an early-out, so passthrough
is bit-identical **by construction** rather than by each slot's implementation
remembering to honour it. That is the property the old design lacked, and the
reason Notch shipped collapsing stereo to mono at a setting a patch author reads
as "off": there was no rule a new slot type could not break.

`mix == 1` runs the wet path untouched — no copy, no lerp — which is why every
patch predating the contract is bit-identical and parity did not move (156/156).

`amount` stops carrying bypass duty and becomes purely per-slot character. Gain's
0.5-is-unity and Comp's always-on brickwall stop being anomalies: they are simply
what those slots DO at `mix = 1`.

### Two pins retired, which is what paying a debt looks like

`slotcontract_check` pinned three violations when it landed. Two are now gone:
**Comp** and **Notch** had *no identity point at any amount* — they are bypassable
now, universally. **Comb's +8.8 dB at amount 0.5 remains pinned**: that is a fact
about the AMOUNT axis and bypass does not touch it.

The gate's assertion changed with the rule. It no longer hunts a per-slot
`identity_at`; it asserts the universal guarantee — **`mix = 0` is bit-exact for
every slot type** — which is the assertion a new slot cannot escape.

### The plant that did not fire, and why that mattered

The first plant removed the `mix <= 0` early-out for one slot type. The gate stayed
green — **correctly**. With `mix = 0` the blend path computes
`dry + (wet − dry)·0 = dry`, so the early-out is an OPTIMISATION and the lerp is
the guarantee. The plant tested the wrong thing.

Replanting against the actual guarantee — inverting the blend so `mix = 0` means
fully wet — turns the gate **RED on four slot types at once**: Drive 0.151,
Filter 0.192, Comb 0.438, Notch 0.229. Restored: green.

That distinction is worth keeping: a check can look untested when the plant, not
the check, is the thing that is broken.

### Surface

Four global params, ids 133-136, appended. Default **1** so nothing existing
changes. Placed in the FX rack group.

## ADR-094 — Saw shape (glass) folded from the detune lab (ACCEPTED)

**Date:** 2026-08-19 · **Status:** ACCEPTED (human: *"I approve of that plan"*).
**Spec change:** yes — `swarmsaw.html` is a protected reference and it changed;
the port changed identically in the same commit, so this is a fold, not a
divergence.

### What was missing

All four controls — `sawBase`, `sawProfile`, `round`, `roundHi` — existed **only**
in `docs/design/detune-lab.html`. Zero occurrences in the SAW reference, the core,
the shell or the presentation table. This was never a GUI omission: **the fold had
never happened.** `shape` (id 69, ADR-058) is a different axis entirely — saw to
band-limited square, not roundness.

The fifth fold in an established sequence: tone tilt (ADR-060), hi-tame (061),
drift modes + keep-phase (062), glide (063), and now this. Same terms as all four:
**parity-safe superset, inert at its defaults.**

### The feature

Two independent axes, applied in the reference's order — BLEP, then base, then
roundness:

- **BASE** (`sawBase`) crossfades the top-level saw through four
  sawtooth-flavoured variants: curved, fat, driven, aggressive. **Anchor 0 is the
  BLEP saw**, so the crossfade begins at exactly what the engine already produced.
- **ROUNDNESS** (`round`, `sawProfile`, `roundHi`) morphs the result toward one of
  five anchors — glass (parabola) · soft · hollow (triangle) · pure (sine) · reedy
  — per voice, optionally scaled by the voice's position in the spread so higher
  voices round more.

Both banks are carried over **verbatim**, placeholders and all: the lab's own
comment says the real profiles are still to be measured from synths, and a fold
moves code rather than improving it, or parity stops meaning anything.

### Placement, which parity alone would not have caught

The port carries a stage the reference does not — ADR-058's `shape`, a C++-only
superset. The new stages go **before** it, exactly where the reference puts them.
ADR-058 is the stage that yields position because it has no reference to be
faithful to. Ordering them the other way would have been invisible to parity until
someone set both non-zero, which is the kind of latency this project keeps paying
for.

### Inert first, then actually exercised

The fold moved **147/147 to 147/147** — bit-identical, by construction, because
`sawBase <= 0.001` skips the crossfade and `round <= 0.001` leaves every voice at
zero roundness.

**That green says nothing about the feature**, which is exactly the hole ADR-093
found in the glide goldens. Three scenarios added — `saw-base`, `saw-glass`,
`saw-round-hi` — taking parity to **156/156**, and both proved able to fail:

| plant | result |
|---|---|
| drop the `roundHi` pitch scaling | **FAIL** `saw-round-hi`, rms 3.591e-02 |
| swap the hollow anchor for glass | **FAIL** `saw-round-hi`, rms 8.329e-03 |
| restored | 156/156, worst 4.262e-09 |

### Surface

Four **per-oscillator** params, ids 129-132 appended (`params.h:212` — ids never
change). Per-oscillator because giving each oscillator its own saw character is
the point of having two. Presented as a *Saw shape* group on the OSC page.

## ADR-093 — Quantiser ties resolve toward the previous emitted step (ACCEPTED)

**Date:** 2026-08-19 · **Status:** ACCEPTED (human ruling in session, "go for it").
**Spec change:** yes — `docs/design/bend-lab.html` is a protected path and the
reference changed. The port changed identically in the same commit, so this is a
spec change, NOT a divergence.

### Two defects, one code path

**1. Ties resolved by loop order.** `if (d < bestD)` with a scan ascending from
`floor(semis) − 12` means the lower candidate arrives first, claims `bestD`, and
the equal-distance higher candidate fails `d < bestD`. Every tie resolved
**downward**, decided by iteration order rather than by any musical choice.

Tonality found this first, in their own quantiser, independently
(`HYPERSAW-002` §2). Their domain made it severe: `conform_to_scale` quantises
**integer MIDI notes**, so every out-of-scale pitch class is exactly equidistant
and all five accidentals in a major scale tie deterministically — a fixed
direction did not resolve a corner, it decided every accidental and sagged
chromatic lines flat. **Ours is narrower and we said so** after first overstating
it: our input is the glide's continuous output, so a tie is a knife edge one ULP
wide, reachable when the law is off (`x = target` bit-exactly) and rare otherwise.

**2. Chromatic mode disagreed between reference and port.** Chromatic never ran
the candidate loop at all — the reference used `Math.round`, the port used
`std::lround`, and those **differ on negatives**: `Math.round(-1.5) = -1` (half
toward +∞) versus `lround(-1.5) = -2` (half away from zero). A full semitone of
divergence in the shipped plugin, at every exact negative half-step, in a project
whose definition of correctness is 1e-6 parity with that reference.

### Decision

- **One candidate search for both modes.** Chromatic is "every pitch class
  admitted" running the same loop. There is no rounding function left to
  disagree about, which closes defect 2 by construction rather than by matching
  two library behaviours.
- **On an exact tie, prefer the candidate nearer the previous EMITTED step.** The
  output line is the melody a listener hears, so continuity is owed to the output,
  not the input (Tonality's ruling, adopted). Deterministic and replayable, where
  the existing hysteresis is continuity-in-time and depends on wobble history —
  both are kept because they do different jobs.
- **With nothing emitted yet, keep the lower candidate.** Stated as a choice
  rather than left to loop order; reachable only on the first quantised sample.

### Why the goldens never saw either defect

The standing gesture settles at **0.5**, which in C major is equidistant from
nothing. The whole tie path was unrendered, so 147/147 and `glide_check` were
silent about it — `L0031` exactly: a reference oracle certifies agreement over
the surface the reference RENDERS, and this surface was never rendered.

Two scenarios added, `glide-tie-scale` and `glide-tie-chrom`, landing on exact
ties of both signs. **`model = kOff` is load-bearing in both**: under any moving
law the output approaches asymptotically and never lands exactly on a midpoint,
so a tie scenario using one is a test that cannot fail. The first version used
`kConstTime` and a planted regression sailed straight through it.

### Proof the gate can fire

- Remove the tie-break → **RED**, `glide-tie-scale` rms 1.1547, `glide-tie-chrom`
  rms 0.4564.
- Put chromatic back on `lround` → **RED**, same two scenarios.
- Restored → **GREEN**, both at rms **0**.

`./verify full` green; parity 147/147 unchanged (worst 4.262e-09), the pre-existing
five glide goldens byte-identical.

### One thing this exposed and did not fix

The scenario list exists **twice** — `tools/golden/gen_glide_goldens.mjs` and
`tools/glide_check.cpp` — including the gesture. Adding a scenario to one produced
goldens the other never read, and the check stayed green while covering nothing.
Recorded as a known duplication; the fix is for the C++ to read the generator's
manifest rather than mirror its table.


## ADR-092 — WARP (distortion engine) ingested as a CANDIDATE; it is FX-C's prototype, not a fourth voice engine

**Date:** 2026-08-18 · **Status:** accepted · **Human:** *"just finished another
prototype engine. This one will require more tweaking before it's ready to become
the gold standard (just like the other one)."*

**Context.** `horde_distortion_engine.html` + its spec arrive one day after the FX
roadmap queued **FX-C, a morphing waveshaper with experimental hysteresis**, and
noted that SPEC-FORMANT §10's promised "distortion engine spec" did not exist yet.
It exists now, and it is that: a simplex-blended shaper (§4), three independent
memory mechanisms (§5), an all-pass phase network pre and dispersion post (§3, §6).

**Decision.**
1. **WARP is FX-C's prototype, not a fourth engine in the ADR-091 family.** The
   family (HYPERSAW · SPECTRA · CANTO) are *sources*; WARP is a *post-stage*. The
   spec says so itself (§10: "WARP is the shared post-stage; parameter surface
   should be identical regardless of source"), and that sentence is the strongest
   argument against filing it as a sibling: a stage every source hands off to is a
   slot type under the FX slot contract, which is exactly FX-C's slot.
2. **Ingested as a CANDIDATE, not a reference** — the same status CANTO holds, for
   the same reason. `SPEC-DISTORTION.md` becomes a protected path; the prototype
   does NOT yet join the seven spec-in-code references.
3. **The blocker is identical to CANTO's, at `horde_distortion_engine.html:161`:**
   `Math.random()` inside `WarpCore.render()`, driving the `walk` parameter's
   coefficient drift. Same seed + same note order must give identical output
   (CLAUDE.md invariant, SPEC §5.7). A prototype that cannot reproduce itself
   cannot be a parity reference, because there is nothing stable to be at parity
   WITH. One sanctioned edit outstanding: replace with a mulberry32 stream.

**Consequences.** WARP inherits FX-C's prerequisite — the FX slot contract must
exist first (rack owns dry/wet; per-slot `{identity_at, blends_dry, changes_image,
changes_level, latency_samples}`), and its `changes_image` is certainly true given
the all-pass network. Its hysteresis must declare its own settling and face the
feedback lab's edge-width scan before it is allowed on any feedback path. The
spec's own §10 already lists the work honestly — clicks from block-rate all-pass
coefficient recompute, no oversampling, LUT resolution at high fold order — and
that list is the queue, not a disclaimer.

**Rejected:** filing WARP as engine #4. It would give the family a member that
generates nothing, and would duplicate FX-C rather than fulfil it.

## ADR-096 — The note lane travels by the same five laws as the bend lane (ACCEPTED)

**Date:** 2026-08-19 · **Status:** ACCEPTED (human proposed the merge: "could we
increase the bendTau range and then merge them?").

### The problem

Two controls held the same physical quantity. `glide` (id 33, ADR-026/076) was a
lag time in SECONDS driving a hard-wired one-pole in HERTZ at `swarm_core.h:1238`;
`bendTau` (id 109) was a lag time in MILLIseconds feeding `GlideCore::kLag`. The
laws were identical — `glide_core.h:110` and the old site are the same
`x += (target - x) * (1 - exp(-dt/tau))` — so the note lane had one law where the
bend lane had five, and the human wanted all five on both.

### The ruling

The note lane runs `GlideCore`, the same struct the bend lane runs. Ids 137-145
mirror the bend block field-for-field minus `retMul` (a note has no home pitch to
spring back to — the same rule the bend block already states). `noteLawLink`
selects own-settings or follow-the-bend-law; the shell resolves the link and
pushes a finished struct, so no shell-only concept enters the DSP.

### Why id 33 survived instead of being superseded

The tempting move — mint `noteTau` in ms and retire id 33 — puts a silent 1000x
between a shipped preset and its meaning: `docs/presets/serum-parity-reference.json`
already stores `"glide":0.89`, and 0.89 read as milliseconds is not slow
portamento, it is no portamento. Id 33 therefore keeps its id, its key, its
SECONDS and its range, and the core converts at the use site. `bendTau` widened
1-400 -> 1-2000 ms instead, which is free because nothing has ever stored it —
it shipped hours earlier (bbdfd0c) and appears in no preset.

**A merge between two units is a migration, not a rename.** The cheap direction
is the one where the stored value never moves.

### Why the defaults are own-settings + lag

`noteLawLink` could not default to FOLLOW: `bendLaw` ships off (human ruling
2026-08-19), so a following note lane would travel instantly and portamento would
vanish from every patch that ever set glide. Own-settings + `noteLaw = lag` +
tau-from-id-33 reproduces the pre-ADR-096 behaviour exactly. Sync is the option,
not the base state.

### The divergence, stated plainly

Travel moved from the HERTZ domain to SEMITONES. The law is the same one-pole;
the domain is not, and Hz-linear travel audibly accelerates at the bottom of a
wide interval. **No gate can see this.** `trajectory_check`'s glide criterion is
"within 1c in 12 tau" — tau-relative, so both domains pass it identically; a
plant (tau x10) was run and the gate went RED, confirming it covers the lane's
TIMING and confirming that timing is all it covers. Parity is silent by
construction: the JS reference has no glide at all (156/156 unmoved, worst
4.262e-09). Curve SHAPE is therefore a human ear ruling — filed as NTR-3.

### Also landed

`shown_when` gained AND across comma-separated clauses. One key could not express
`noteLawLink=0,noteLaw=3`: gating on the link alone showed all seven law controls
at once, gating on the law alone showed them while the lane was following. This
is the same missing capability the chord layer will need for OR-across-keys.

## ADR-097 — Per-note (MPE) bend obeys the bend law (ACCEPTED)

**Date:** 2026-08-20 · **Status:** ACCEPTED (human: "I remember the lab having a
toggle to apply the same inertia settings to MPE pitch bend").

### The gap

`docs/design/bend-lab.html` gives every sounding note its own inertia state and
steps it with the SAME params as the wheel — `nt.bend.step(nt.bendTgt, P,
nt.midi)`, line 539. The port applied per-note bend **instantly** at all three of
its entry points: the ADR-038 note-on latch, `CLAP_NOTE_EXPRESSION_TUNING`, and
member-channel `0xE0`. So a patch with a bend law shaped the wheel and left MPE
snapping — in the one context where character matters most, because on an MPE
controller the bend IS the performance.

This is the third unported reference behaviour found in two days (the wheel,
`qTime`, this). They share a shape: the reference had it, a gate existed for the
neighbouring case, and nothing asked whether THIS transport was covered.

### The rule

One traveller per note SLOT, not per channel — two notes on one channel can be at
different bends mid-flight, and a channel-keyed lane would drag them together.
`bendLane = true`, because return-toward-rest is as meaningful per note as on the
wheel. Both lanes step on one clock, at the existing bend-grid boundary.

A fresh strike **arrives** at its latched bend instead of travelling to it: the
note did not exist while the controller moved, so gliding in from zero would
invent a gesture the player never made. That mirrors the reference's `reset()`.

### `bendMpeLaw` (id 149) ships FOLLOW

Because the reference has no way to give per-note bend a different character from
the wheel — it steps both with the same P. The toggle exists anyway for the one
real case: a player wanting the raw controller under their finger while the wheel
keeps its character. Inert at defaults regardless: `bendLaw` ships off, so
following it is the instant write either way.

### Two stranding paths, both closed

`stepNoteBends()` only runs inside the `bendActive()` branch of `process()`. So a
note left travelling when the law is switched off — or when `bendMpeLaw` is
turned off — would never be stepped again and would hang at a partial bend
forever. Both transitions now land every travelling note immediately.

### Evidence

`mpe_check`: early 659 Hz bin 0.00597 against settled 0.20031 — it travels AND
arrives. The late read is the must-arrive control: without it, a lane that simply
DROPPED the bend would pass the early check for the wrong reason. Plant (instant
application restored) reads early 0.32062 — already at the destination — and the
gate goes RED.

## ADR-098 — Control columns FLOW; they do not tile (ACCEPTED)

**Date:** 2026-08-20 · **Status:** ACCEPTED (human: "come up with a formatting
rule that doesn't allow so much empty space").

### The rule

> A control column is a **flow**, not a grid. Panels are laid out with CSS
> multi-column, so a panel's height is exactly its content's height and the next
> panel begins immediately beneath it. No panel's size may be a function of any
> other panel's size.

`.ctlcol { display:block; columns:240px; column-gap:10px }` with
`break-inside: avoid` on every direct child.

### Why the old layout could not avoid the waste

`.ctlcol` was `display:grid`, and a grid lays items out in **rows**. Every box in
a row is stretched to the height of the tallest box in that row. With Bend at ~25
control rows sitting beside Pitch at 3, Pitch was painted **498px tall around
136px of content**. Measured across the whole OSC control column: **37.2% empty**,
and none of it between panels where it would read as breathing room — all of it
inside them, which reads as a bug.

After: **1.9%**, and the column got SHORTER (952px → 764px) holding the same
controls, so it is denser and needs less scrolling at once.

### Why this is not a threshold to police

The defect class is removed **by construction**, not by vigilance: multi-column
flow has no row concept, so there is no mechanism left that can stretch a panel
to a neighbour's height. Nothing needs to check a percentage because nothing can
produce the percentage. (Doctrine: safety by construction, not by vigilance.)

That is deliberate, because a pixel-level layout gate is **not CI-able here** —
it needs a real layout engine, the same reason pluginval/auval live in the
human-paced Layer-E set rather than `./verify fast`. A rule enforced by structure
outranks one enforced by a check that cannot run.

### Amendment 1 (2026-08-20) — the rule is about TILING, not about `.ctlcol`

First application only converted `.ctlcol`. FX and MIX tile their clusters
straight into the page grid, and both carried `align-items:start` — which stops a
box being STRETCHED but does nothing about the ROW still being as tall as its
tallest member. On FX the ~560px "About the rack" note set row 1's height and
dropped "FX rack" into row 2, far below four short slot boxes. **Half the rule
fixes half the defect**, and the half it fixes is the one that is easy to see.

Stated properly, the rule is: *wherever clusters are tiled*, they flow. Both
pages now do.

Known consequence, not a defect: an unbreakable tall panel sets a floor on the
column height, so FX's four slots wrap 1-2-3 / 4 rather than sitting in a row.
Reading order is preserved (top-to-bottom, then right).

### The tradeoff, taken knowingly

Reading order becomes top-to-bottom per column rather than left-to-right. That is
the newspaper convention and what every masonry layout accepts. `break-inside:
avoid` applies to EVERY direct child, not just `.cluster` — the OSC column also
carries a bare `.note`, and flowed prose with no break rule splits across a
column boundary and reads as orphaned text.

## ADR-099 — A silent oscillator is skipped, not rendered-and-zeroed (ACCEPTED)

**Date:** 2026-08-20 · **Status:** ACCEPTED (human: "I am starting to notice a bit
of a processor choke... is there anywhere we can work on efficiency before we make
it even heavier?").

### Measure first, and what the measurements said

- `cpu_bench` (core alone): 1.81% of a core at 7 voices x 8 notes — the swarm
  math is NOT the choke.
- New `shell_bench` (the real plugin through the CLAP factory): heavy patch tops
  out at 5.9%. The DAW's ~40% is this figure times sample rate, buffer size,
  held tails and instance count — so every shell percent is a multiplier.
- The decisive row: **oscillator 2 at its shipped default vol 0 cost exactly the
  same as oscillator 2 audible** (5.68% vs 5.69%). Every single-oscillator patch
  paid double.
- `sample` profile: renderSeg 67%, libm transcendentals ~23% (sincos/atan2/exp2
  from controlTick's order-parameter and coupling loops).

### The rule

An oscillator k >= 1 whose output is provably exact zeros — its own `p.vol` is 0
(the gain multiplies INSIDE render), or its mixer gain has SETTLED at 0 — is
skipped entirely: no render, no chunked sum, meter forced to 0. Skipping an
exact-zero contribution is output-identical by construction.

Not gain-smoothed-out mid-ramp: the skip waits for the smoother to settle, so a
mute's fade-out completes before the core stops. Oscillator 0 is not skipped (it
renders straight into the output buffer, and silencing the primary is not a
pattern worth complexity).

### The traded behaviour, stated

While skipped, the core's envelopes and phases FREEZE. Raising the volume
mid-held-note resumes voices from where they paused rather than where they would
have decayed to. That is the "muted layer costs nothing" contract every DAW
mixer teaches, and it is the entire source of the win.

### Numbers

default patch 8 notes: 3.51 -> 1.82% (−48%) · 16 voices: 5.64 -> 3.78% (−33%) ·
osc2 audible: unchanged (5.67%) — the cost returns exactly when bought.

### Oracles

mixer_check gained the vol-path case (vol 0 -> 0.4 mid-note un-skips; the shipped
default IS the skip state, and the pre-existing solo-restore case cannot see this
path). Plant (sticky skip `< 0.5`) takes it and four solo/mute cases RED.
parity 156/156 unmoved; state, routing, mpe, rtsafety, notefuzz all GREEN.

### Left on the table, recorded not taken

controlTick's order-parameter sincos runs even at K = 0, where the coupling force
it feeds is multiplied by exactly zero (~15-20% of the remaining bill). Skipping
it is output-identical for AUDIO but stales R/psi, which the VIZ reads — so it
needs a design ruling on viz staleness, not a stealth optimisation. ROADMAP has
the entry.

## ADR-099 Amendment 1 — Skip only when OFF; volume is just volume (2026-08-21)

The vol-0/settled-mute skip bought CPU by freezing the core, and a frozen core
freezes its VISUALS: a silent osc 2 half-rendered (viz stopped, meter zeroed)
while a silent osc 1 — never skipped — kept moving. The human read it exactly:
"currently it just looks like a mistake." Human ruling: the skip only kicks in
when the oscillator is switched OFF (ADR-100's enable), never when it is merely
silent.

The CPU is kept a different way: **osc 2 now SHIPS off** (enable default 0 for
osc > 0), so the default patch takes the cheap path through the honest switch
instead of through a silently frozen core, and "Osc On" unchecked SAYS why the
oscillator is dark. Pre-ADR-100 patches (no "enable" key) migrate to
enabled-everywhere — they were saved when everything always rendered.

paramscope_check's setup now switches osc 2 on before raising its volume — the
check predates the switch; its assertions are unchanged.

Fresh-default cost after: 1.97% (was 1.82% under the vol-skip) — the difference
is the honest price of visuals that never lie.

## ADR-100 — Oscillators have an on/off switch, and OFF means gone (ACCEPTED)

**Date:** 2026-08-20 · **Status:** ACCEPTED (human: "add the ability to turn
oscillators off and on instead of just volume", with the morph design: "they can
just toggle on and gradually increase the volume as they move between corners").

### The rule

`enable` (base id 150, PER-OSC so each morph corner can hold its own answer per
oscillator). OFF hard-kills the core's voices — gate AND envelope to zero — and
joins the ADR-099 render skip, so a disabled oscillator costs nothing at all.
Osc 0 disabled zeroes its span by hand (it renders straight into the output
buffer; every other path relies on render() doing the zeroing).

OFF differs from vol 0 on purpose: vol 0 freezes envelopes for resume (a muted
layer); OFF kills them (the oscillator is not part of the patch right now). Both
transitions kill — OFF because the switch means silence now, ON because voices
frozen since the disable would resume as zombies at whatever loudness they froze.

### Measured (enable_probe, through the CLAP factory)

both on 4 held: rms 0.284 / 3.71% · osc2 off: 1.96% · both off: rms 0.00000 /
0.03%, tails KILLED · re-enable + new note: SOUNDS, old notes stay dead.

### The choke, diagnosed while here (user_patch_bench, the human's own patch)

Both oscillators, 16 voices, release 5 s, spring bend, comb + drive, 48 kHz /
64-sample blocks: idle 0.14%, ~1% per held note — and EIGHT SECONDS after
note-off the CPU had not moved (8.7% flat). The release envelope is a one-pole
with the knob as its TIME CONSTANT and voices die at -60 dB, which a 5 s release
reaches after ~7 tau ≈ 35 s. Every note burns full price for half a minute,
inaudible for most of it. "Each takes a long time to let up" is that arithmetic.
Changing the kill threshold or the curve is a SOUND change and a reference
change — parked on the ROADMAP as an open question, not smuggled in as an
optimisation. The debug-mode hypothesis was checked and cleared: Release, -O3,
-DNDEBUG, zero assert strings, installed binary byte-identical to the artifact.

### Also in this change

- `shown_when` gained its OR tier (`;` between groups) with its first real
  consumer: the Scale section serves BOTH quantisers, so it shows on
  `bendQuant=2;noteQuant=2`. The tonic selector was never missing — its gate and
  the dynamically-built picker's disagreed, so Scale showed while Root hid.
- COPY PATCH / PASTE in the tab bar over hzGetState/hzApplyState — the "send me
  the patch" loop the human asked for, and the seed of the preset system.

## ADR-100 Amendment 1 — Enable-ON re-strikes what is held (2026-08-21)

"Sometimes osc 2 doesn't work": switching an oscillator ON mid-chord produced
nothing until the next fresh note (the ON transition killed voices, correctly,
and nothing re-created them). The human enabled it, played nothing new, heard
nothing — "broken" was fair. Enable-ON now re-strikes every active tag on that
core at its held velocity (velocity joined NoteTag for exactly this). A fresh
attack rather than a resumed envelope is intentional: the note is NEW on this
oscillator. Side effect that is really the point: MORPH-driven enable flips
now pop the oscillator in WITH the held chord — the corner design working.
Probe: osc1 vol 0 + osc2 off = silence 0.00000; osc2 ON mid-hold = 0.069, no
fresh note. The second reported cause is the documented v1 seam: with morph ON,
corners captured with osc2 off will force it off every grid tick — the
corner-editing phase (and exempt) is the designed fix.

## ADR-101 — The saw-shape anchors are tabled; the functions stay the truth (ACCEPTED)

**Date:** 2026-08-20 · **Status:** ACCEPTED (human diagnosis, confirmed by bench:
"I think I figured out what's eating the most CPU — saw shape").

### The measurement that confirmed the human

16-voice baseline 3.71% of a core. sawBase 0.5 alone: **8.07%**. Roundness +
profile: **8.62%**. All four stages: **12.38%** — the section cost 2.3x the rest
of the synth combined. Cause: `std::pow(|r|, 1.4)` (and sin/tanh) per sample per
voice, two anchors per stage for the blend — ~11M pow/s at 128 oscillators.

### The rule

The reference anchor FUNCTIONS remain the single source of truth — the tables
are built FROM them at load (never on the audio thread) and the render loop
reads the tables. 16384 cells + linear interpolation; N+1 points so the last
cell interpolates toward f(1-) instead of wrapping +1 down to -1. Anchor
selection (index + blend fraction) hoisted to the segment head — it was
recomputed per sample per voice, clamp, floor and all. A zero-weight blend side
is skipped entirely.

### The divergence, measured not argued

Linear interpolation is an approximation, so this is an INTENTIONAL divergence
from the reference path and is recorded as such. Budget: smooth anchors ~2e-8;
the pow cusp at r=0 ~5e-7 for one sample-neighbourhood per cycle; the triangle's
corner lands exactly on a grid point at this power-of-two size. Measured against
the reference goldens: saw-base 1.007e-09, saw-glass 2.485e-09, saw-round-hi
9.707e-10 RMS — three orders of magnitude under the 1e-6 bar, and the fleet
worst is still the unrelated dyn-ring at 4.262e-09.

### After

sawBase 4.30% · roundness 4.23% · ALL FOUR **5.02%** (was 12.38) — the section
now costs +35% instead of +234%.

### Also in this change

- id 69 relabelled **Squareness** and moved into the Saw shape section (human:
  two unrelated things were both named "Saw Shape"). The key stays `shape` —
  state compatibility; only the face moved.
- The Saw shape section has an ENGINE-DRAWN one-cycle waveform (`hzGetShapeWave`
  → the same anchor functions + the ADR-058 morph; the bend graphs' "drawn by
  the engine, never a JS twin" rule). Display honesty: the ideal saw stands in
  for the BLEPped one, and roundness shows at its knob value — roundHi's
  per-note pitch scaling cannot appear in a static cycle.

### Process note

PR #365 was stacked on render-efficiency and merged into it AFTER that branch's
own PR had merged — the PR #328 trap, second occurrence. Recovered by PR #366
(the branch diff against main IS the lost content). Standing rule from here:
**no more stacked PRs**; a dependent change waits for its base to land on main.

## ADR-102 — The bend lane is active when ANYTHING processes it; poly glide is automatic; Bend lives on MAIN (ACCEPTED)

**Date:** 2026-08-21 · **Status:** ACCEPTED (four human reports/requests in one).

### The regression: quantise with the law off did nothing

`bendActive()` asked only about the LAW, so with the law off — the shipped
default — `applyParam(38)` instant-wrote the wheel past `GlideCore::step()`, and
the quantiser lives inside step(). The note lane quantised (it steps its own
GlideCore); the wheel's lane skipped its own. Fix: active = law engaged OR
quantiser engaged; kOff + quant on the grid is the published kOff contract
(pass-through that still quantises). Turning the quantiser off settles the lane,
same rule as leaving a law. Gate in mpe_check: law off, chromatic, wheel to
+1.37 st → must sound +1 st; both bins measured (the step bin is the must-arrive
control); plant reads the raw bin and goes RED.
The human's follow-up suggestion (gate bend-quantise on a global quantise) was
motivated by the bug reading as arbitrary; deferred — it works standalone now.

### Poly glide is not a choice

"On as long as mono isn't toggled and a bend law is set." The arming condition
drops `p.polyGlide` (declared for state compat, labelled (dev), permanently
gated off in the GUI via an impossible shown_when — voiceMono=2). Mono never
reaches the poly path, so the law is the only question left.

**The flag was load-bearing in a way nobody knew.** Removing it turned parity
RED (153/156, worst 8.4e-02 at dyn-gravity): the CORE's default noteLaw lambda
set kLag but kept GlideCore's own tau = 60 ms, so every bare core glided out of
the box — invisible for two days because the flag gated the arming. The shell's
twin lambda fixed this exact default on 2026-08-19; the core's did not. A
default that only a removed flag was hiding is a bug with a delay fuse.

### "Always glide" already existed

The requested toggle is glideMode = 1 (ADR-076, human-ruled 2026-07-31:
"remembers the last played note(s) and always begins with a bend"). It was
undiscoverable behind "last note (memory)"; relabelled "always (from last
note)". No new parameter.

### Bend returns to MAIN

The whole merged section (29 rows), per the human. The cluster is fully
generated, so the move is a page-column edit and a regen.

## ADR-103 — Glide From has three sources, and "always" is its own thing (ACCEPTED)

**Date:** 2026-08-21 · **Status:** ACCEPTED (human: "I want 'always' to be an
option available to mono... I don't want always to replace the former option of
'glide from last note', I want it to be its own thing").

### The split

0 **held note (legato)** — glide only while another key is down.
1 **last note (ringing)** — glide while the previous note still SOUNDS: its
  release tail counts, and true silence resets the phrase. Ringing is measured
  as the render measures audibility (env > 1e-3), so the glide source dies at
  the moment the ear loses the note. **This option never existed before.**
2 **always** — glide from the last played note regardless, silence included.

The selector is un-gated (visible in mono). Mono's fresh strikes flow through
the same core noteOn arming, so all three sources work in mono without a
separate mono path.

### The history correction the split forced

The pre-split option 1 ("last note (memory)") BEHAVED as always — lastNoteF
persisted across silence, human-ruled 2026-07-31. So the new value 2 is the old
value 1's sound, and stored patches migrate: schema<2 + glideMode=1 → 2 (state
schema bumped to 2 for exactly this). A patch keeps its sound; the number moves.

### Gate

trajectory_check, four criteria, each mid-flight (arrival cannot fail — the
arming-bug family's lesson): held/ringing snap after true silence, always
glides from silence, ringing glides while a tail still sounds. Plant (mode 1
forced to old always-behaviour) takes "after true silence, snaps" RED.

## ADR-104 — The quantum morph engine: flips, not crossfades (ACCEPTED, v1)

**Date:** 2026-08-21 · **Status:** ACCEPTED (human: "let's go ahead with the morph
port"). Reference: `docs/design/quantum-morph-lab.html` — bilinear corner weights,
per-parameter Gumbel-max corner assignment, temperature + coupling + (later) bias
in ONE score, mulberry32 streams. The Gumbel-max trick makes one seeded draw per
(param, corner) a committed, repeatable patchwork instead of a dice roll per
visit — argmax(log w/T + gumbel) IS a softmax sample.

### Division of labour

`morph_core.h` owns the math (weights, draws, the single scoring law — what you
hear and what a map paints must be one function or the map lies). The SHELL owns
which parameters morph, what a corner snapshot is, capture, persistence — the
glide_core/scale-table division.

### v1 scope, stated

- Morphable set: every PER-OSC parameter (the twin-having set), both
  oscillators, enables included (the human's corner design needs them). Globals
  stay patch-level; the set widens with the corner-editing phase, not by v1
  guessing.
- Corner snapshots are STATE, not parameters (4 x ~100 automation lanes would be
  noise); they ride the state chunk in morphIds order (id-ascending, stable).
- morphStep runs on the 256-sample gravity grid with the ADR-086 accumulator —
  buffer-subdivision independent, and a parameter field does not need 2.7 kHz.
- Stepped params take the winning corner outright; continuous params flip with a
  one-pole slew (`morphGlide`, seconds — ADR-009) or crossfade in blend mode.
- morphOn ships OFF: parity-safe superset, all 156 goldens untouched.
- A fresh instance's corners all hold the DEFAULT patch, so morph-on before any
  capture is silence-safe: every corner agrees.
- While morphOn, live edits to morphed params are overwritten at the next grid —
  the corner-editing model (recorded 2026-08-20) is the designed answer and the
  explicit NEXT PHASE, along with per-param bias/pin from the lab.

### RT discipline

morphInit allocates → it runs at activate (main thread). The audio-thread paths
(morphOn flip, seed reshuffle) are fill/array-writes only. Caught by reading the
code, not by rtsafety_probe — the probe never flips morph mid-run; extending it
to sweep stepped params is noted for the fine-tune pass.

### Amendment 1 (2026-08-21) — the FX rack joins the field

Slot type, amount, tone, mix (57-64, 96-99, 133-136) are appended to the
morphable set: "off in this corner, driven in that one" becomes a rack story,
and module-level EXEMPT (roadmapped the same day) has something to exempt.
**morphIds is append-only from here, like param ids** — a v1 corner chunk fills
the per-osc prefix in its original order and the FX tail takes defaults, so an
old patch loads with corners intact and rack unmorphed: exactly what it said
when saved.

### Gates

trajectory_check: same seed + position -> identical assignment on two cores
(128/128); a different seed changes the patchwork (83/128 moved); cold +
cornered -> the dominant corner owns everything (128/128). Shell inertness rides
state_check/parity (morphOn off).

## ADR-105 — Presets: the quick win is storage that already exists (ACCEPTED)

**Date:** 2026-08-21 · **Status:** ACCEPTED (human: "a save/load preset section
for a quick win... maybe also for individual corner presets").

Named presets over the EXISTING state JSON, stored in the webview's
localStorage (per-app-container in WKWebView — survives reloads and sessions).
COPY PATCH / PASTE stays the export/import path; the disk browser over
docs/presets is the roadmapped step this deliberately does not attempt.
Corner presets: the same pattern over one corner's snapshot
(hzMorphCornerJson/Apply, morphIds order — the append-only contract), so a
corner sound is a nameable, reusable thing independent of the patch it was
born in. SAVE…/load… live on each corner row next to CAPTURE.

Also in this change (ADR-100 A2): oscillator power toggles live ON the osc tab
bar (raw id 150+stride, never effId — each button addresses its own oscillator
regardless of which is being edited), and every per-swarm visual (phase circle,
carpet, voice map) shows an explicit OFF state when the viewed oscillator is
switched off — the "phase dots with the visuals off" were real voices fanned
into the disabled core for the enable-ON re-strike, frozen because render is
skipped. The visual follows the SWITCH, per the human's ruling.

The saw-shape globality report was investigated and NOT reproduced: the engine
routes 129/1129 independently (probe: osc1=0.25, osc2=0.75 simultaneously) and
the GUI sends 129 vs 1129 correctly per the selector (instrumented bridge).
Likeliest experiential cause: morph ON with corners captured when both
oscillators shared shape values — the field forces them back in lockstep.

## ADR-106 — The quantiser is anchored: base + offset, the reference's own words (ACCEPTED)

**Date:** 2026-08-21 · **Status:** ACCEPTED (human report: "notes are resolving
to a slightly different pitch after a bend than when they're initially played").

### The dropped argument

bend-lab's quantiser has carried a `base` parameter since it existed —
`quantise(p, base = 0)`, first line `semis = base + this.x` with the comment
"ABSOLUTE pitch, not the offset". The port dropped it. Consequences:
- The BEND lane quantised its OFFSET as if the offset were a pitch: any note
  whose class is off the offset-space grid resolved to a shifted pitch after a
  bend. The report, verbatim.
- The NOTE lane quantised in log2(f)*12 space, whose classes sit 0.376 st off
  MIDI's — every scale-quantised note landed a constant 37.6 cents off the grid
  (latent: noteQuant ships off).
- Every golden ran base = 0, where offset and pitch space coincide — L0031's
  blind spot, hit for the third time (the wheel, qTime, now base).

### The fix

`step(target, p, base = 0)` / `quantise(p, base)`: candidates, the qStep latch
and hysteresis all live in ABSOLUTE pitch (as the reference's do); only the
return converts back to lane units. Anchors: per-note MPE lanes pass their own
key; the GLOBAL wheel lane passes the last-struck key (one lane, many notes —
the global compromise; the per-note lanes are exact); the note lane passes
69 − 12·log2(440) to align its log-frequency space with MIDI classes.

### Proof

Two new goldens sliced live from the lab, anchored at F#3 (54) and 54.5 —
classes chosen to be maximally off the default major grid, so a port ignoring
base CANNOT pass. First run: RED at rms 0.5 exactly (the port emitting integer
offsets against the lab's half-offsets) — the scenarios' own must-fail control.
After threading: **rms = 0 on both** — bit-exact. All 21 pre-existing goldens
unmoved (base = 0 is byte-identical). A process note: the runner edit was first
applied with an assert-free regex replace that silently missed `, s.p);` — the
un-asserted-edit trap, again, caught by the golden staying red.

### Also in this change

The GUI1 gravity pitch-correction readout (ratio name + octave fold + live
cents) is ported to gui2 — the snapshot and marshal existed end-to-end; only
the display line was missing. Both swarm panels carry it, class-addressed.

## ADR-105 Amendment 1 — The JSON state path carries the twins (2026-08-21)

"Saving a patch doesn't seem to do anything, or at least loading doesn't."
Measured: the JSON path (the preset system's path, and the dev panel's before
it) saved and scanned BASE ids only — oscillator 2's params never round-tripped
(detune2 stayed at 0.900 against a saved 0.222 while detune1 restored exactly).
For the two-oscillator morph patches being built, a load restored half a patch.

Fix: `o<k>.`-prefixed twins in both directions — the SAME convention
`state_save`/`state_load` (ADR-082) have always used; the JSON path simply
never got it. Gate: state_check gained a JSON-path case driven through new
headless debug exports (`hypersaw_debug_state/apply` — the same two calls the
GUI preset buttons make, previously reachable only through webview lambdas,
which is WHY this path had no oracle and the bug shipped unobserved). Plant
(twin write removed): base case OK, twin case RED — fires on exactly the
missing half.

Process note, third occurrence: a bare `git checkout -- <file>` to clear a
plant destroyed the turn's uncommitted work again. New personal rule enforced
this turn: plants are removed by REVERSE REPLACE, never by checkout, and a
checkpoint commit lands before any plant cycle.

## ADR-105 A2 + ADR-100 A3 — Presets on disk; one gate on osc 2's silence (ACCEPTED)

**Date:** 2026-08-21 · **Status:** ACCEPTED (human: "the save/load functions
still aren't working in either interface. Also the power buttons on the
oscillators don't work").

### Presets: localStorage was quicksand

The plugin webview loads via setHTML — an OPAQUE ORIGIN, where localStorage
THROWS SecurityError — and the store's own try/catch swallowed it: presets
no-opped in the plugin while passing every lab test on localhost. The store is
now DISK: `~/Library/Application Support/LiftedTruck/HYPERSAW/{presets,corners}`
via hzPresetList/Save/Load/Delete (GUI-thread binds — filesystem there, never
in process()), names sanitised to filename-safe characters. The GUI prefers the
host store and falls back to localStorage only in the lab. This is also the
roadmapped "disk browser" landing earlier than planned, because the shortcut
proved not to exist.

### The power buttons: two safeties on one door

Osc 2 shipped enable=0 (ADR-099 A1) AND vol=0 (the ADR-082 era
SilenceHigherOscillators + the id-17 twin default). Clicking power ON worked —
and produced nothing, because the volume was still down. A switch that "works"
inaudibly is indistinguishable from a broken switch. The vol zeroing is retired
at BOTH sites (defaultFor and the constructor silencer); the enable switch is
the ONE silent-by-default gate, and switching ON is audible immediately.
enable_probe now proves the switch ALONE suffices (no explicit vol in setup);
mixer_check and paramscope_check setups flip the switch explicitly.

### Also

The morph-vs-switch interplay got its rule early (the exempt-lean design):
while morph is ON, a manual enable toggle writes itself into all four corners,
so the switch cannot be reverted by the field. Probe-proven; the full per-edit
routing remains the corner-editing phase.

### Process

The recurring python-heredoc corruption (unclosed-paren SyntaxErrors on
multi-line replaces) is now avoided by writing edit scripts through the Write
tool; heredocs carry only trivial one-liners.

## ADR-107 — The truth sweeps: every parameter's readback is gated (ACCEPTED)

**Date:** 2026-08-21 · **Status:** ACCEPTED (human mandate: "don't use a band aid
solution here — start from basics and build an efficient solution that will help
us avoid similar problems as we add other modules with different default
settings").

### The defect class, named

readParam(1150) indexed shell state with the BASE id — findParam(1150) returns
the base def, so `d->id / 1000` is always 0 — and oscillator 2's enable
readback mirrored oscillator 1 forever. The GUI, three patches deep, faithfully
displayed the lie: "osc 2 says it's on, but it isn't, and the only way to
toggle it on is to turn osc 1 off first" — with osc 1 off the mirror finally
showed off, so the toggle finally sent 1. The human's reproduction steps
encoded the aliasing exactly. Every neighbouring case (mute/solo/octave) used
oscOfId(id); this one did not, and a probe had "verified" the readback for the
wrong reason (after setting 1150=1, engine truth and the aliased readback
coincide — the detector shared the assumption).

### The mechanism, not the patch

Two sweeps in paramscope_check (a ./verify gate), enumerating the plugin's OWN
param_info so future modules are covered with nothing to remember:

1. **Default truth** — fresh instance: get_value(id) == declared default for
   EVERY id, bases and twins. The representation IS the engine at rest.
2. **Round-trip truth** — set every id to a distinct in-range value; the
   readback must return it. Any split between the apply chain and the read
   chain fires here. Exemptions are claims with reasons (23 and 148 snap to
   musical grids by design), never silences.

First run, against the live tree: THREE lies — the reported 1150 aliasing, a
polyGlide default mismatch (shell declared 1, core struct held 0) nobody had
noticed, and 148's by-design snap (exempted with its reason). After the three
one-line truths: 239 ids, zero lies.

### Also (the requested GUI moves)

Power rows hand-placed in BOTH mix strips (data-fixed raw ids, the strip
convention) — which also HIDES the OSC-page swarm-section toggles, because the
generator skips hand-placed ids. The GUI's hard-coded default fallbacks
(`k===0?1:0`, a third copy of the defaults) are deleted: SHELL_DEFAULTS
(defaultsJson, twins included) is the one source.

## ADR-108 — The feature-dependency graph: one declaration, three consumers (ACCEPTED)

**Date:** 2026-08-22 · **Status:** ACCEPTED (human: "create a graph of feature
dependencies so we can automatically derive a morph hierarchy — this will also
clear up some GUI issues where there are omnipresent fields which should only
show up on certain settings, like the advanced detune controls").

### The problem, stated precisely

THREE systems answered "does this parameter matter right now?" independently:
`shown_when` (GUI, a grammar grown ad hoc), the morph field (which happily
flipped a parameter whose enabling law was off — a no-op flip spending a
corner's identity on something that cannot sound), and the ENGINE's own guards,
which are the truth the other two approximated.

### The rule

A `depends` column in the presentation table — already the parameter registry,
so a second registry cannot drift from it — is the SINGLE declaration:

1. **GUI**: `gen_gui_controls` reads `depends` and derives `shown_when`. The two
   columns coexisted for exactly one commit, long enough to seed `depends`
   losslessly from the hand-written gates, so today's GUI regenerates
   byte-identically before a single dependency was corrected.
2. **MORPH (the derived hierarchy)**: `tools/gen_depends_header.py` emits
   `src/depends_graph.h`; `morphStep` holds a parameter whose enabling condition
   is false IN THE WINNING CORNER, so flips land on parameters that sound.
   Evaluated against the corner's STORED values — "would this matter if that
   corner were playing" is the corner's own state to answer. Conservative: no
   rule means always-live, so anything the graph does not describe morphs
   exactly as before.
3. **AUDIT**: `depends_check` (in `./verify fast`) keeps the declaration
   well-formed and the header current, and prints advisory drift where the
   engine mode-guards something the table does not describe. It deliberately
   does NOT claim a declared dependency is CORRECT — nothing can know
   `harmReach` belongs to law 4 except by reading the branch that uses it, and
   turning that judgement into a hard gate is how exemption lists start.

### The human's example, fixed at the root

`beatMult` (law 3), `harmReach` (law 4), `stretchB` (law 5) — read straight out
of the branches that guard them in `swarm_core.h` — now appear only under their
own law. They were omnipresent because nobody had written the gate; the graph
made the omission visible rather than requiring someone to notice it.

### `never`, and the gate's first catch

`depends_check` went RED on its first run against a hack of mine: retired
`polyGlide` was hidden with `voiceMono=2`, an impossible value. That is a gate
spelled as an out-of-range number — one range change away from becoming ALWAYS
TRUE with nobody touching it. The grammar now has an explicit `never`, honoured
by all four consumers. **A gate that catches its author's own shortcut on the
first run is worth more than one that passes.**

### Control

Plant (a dependency naming a key no parameter declares): RED, both on the typo
and on the resulting stale header. Restored: GREEN, 55 declared dependencies,
2 advisories (laws 0 and 1 take no extra parameters — correctly a note, not a
failure).

## ADR-109 — Corner editing: four boxes, and the authorship family (ACCEPTED)

**Date:** 2026-08-22 · **Status:** ACCEPTED (the human's model, recorded
2026-08-19 before the morph page existed, built now that the dependency graph
underpins it).

### The routing, as the human specified it

`morphArm` (id 159, global, deliberately NOT morphable — an edit-routing mode
that morphed would change where your edits land as you move the pad):

- **None armed** — an edit lands on the corner that OWNS that parameter right
  now, so it STICKS. This closes the v1 seam where live edits were overwritten
  at the next grid tick.
- **Armed A..D** — the edit writes that corner's baseline and is NOT applied
  live: you are authoring a corner that may not be the one sounding, and forcing
  it live would lie about which corner you just changed.

### The human's open question, answered

> *"how it behaves when it's in continuous mode, the parameter is continuous,
> and the current morph position is somewhere in the middle. Maybe it edits both
> in such a way that their average arrives at that point?"*

Yes, and **weighted**: the delta is distributed across corners in proportion to
their bilinear weight, so `sum(w[k] * corner[k])` lands exactly on the edited
value while the corners keep their relative identities. Distributing EVENLY
would move a corner you are barely touching as much as the one under your
cursor; proportional is the reading that respects where you are standing.

### EXEMPT — the third family member

Right-click any control to remove it from the field entirely (bias nudges a
corner's share, pin hands one corner the field, exempt removes the parameter).
On exempt the live value is written into ALL FOUR corners — the recorded design
lean — so un-exempting never jumps and the corners honestly record what was
playing. Exempt is patch STATE riding the chunk in morphIds order, not 150-odd
automation lanes.

### One choke point, one guard

Every edit — GUI, host automation, preset load — passes `applyParam`, so routing
needed exactly ONE hook rather than a rule per call site. `morphFromField`
guards re-entry: morphStep applies the field's own output through applyParam,
and routing that would have the field endlessly rewriting its own corners.

### Visible state, because the mode is invisible otherwise

The four boxes live on the tab bar (every page shares it — the human asked for
them on every page). The armed box glows; while any corner is armed every panel
border goes dashed, because "edits are not going where a performer would expect"
is exactly the state a UI must not let you forget. Exempt rows carry a ⊘ mark.

### Probe

`corner_probe`, through the CLAP factory: unarmed edit STICKS (0.610 survives
200 grid ticks), armed edit LANDS IN D (0.230 appears when the pad reaches D),
exempt HOLDS (0.420 survives the pad slamming to the opposite corner).

## ADR-109 A1 + ADR-024 A1 — The globals join the field; the inertia taper turns around (2026-08-22)

**Source:** a human scan of which controls right-click actually reached, plus
two design questions.

### Why those controls "didn't work"

Every parameter the scan named — freq glide, inertia, inertia curve, mono,
legato, pitch, glide from, scale + root — is in `kGlobalIds`, and `morphInit`
skips globals. They were never in the morph field, so `morphToggleExempt` had
nothing to toggle and returned false silently. "Doesn't work" was the honest
reading of a feature correctly doing nothing.

They are now appended to the field (APPENDED, never inserted: morphIds order is
the corner chunk's order, so every stored patch keeps its values).

### The scale is ONE thing

Root + twelve degrees flip as a UNIT. A per-degree flip would assemble a chimera
from two corners — C major and F# minor interleaved is not a scale, it is a bug
with a musical name. The human said it exactly: *"all the individual scale
degrees would need to be included collectively, of course."* Implemented as a
general ATOMIC GROUP (one corner decision taken on the group's lead index,
exempt toggling the whole span), because the next group — a chord voicing, an FX
slot's four params — will want the same mechanism.

### ADR-024 Amendment 1 — inertiaCurve 0.5 -> 2.5

The human asked to lock it "around 2.5" by ear. The arithmetic agrees, and shows
the original went the wrong way: with `w = knob^curve` and the musically useful
`w` range ~0.02..0.3, curve **0.5** squeezes that range into knob 0.0004..0.09 —
the bottom 9% of the sweep — while **2.5** spreads it across knob 0.21..0.62.
ADR-024's stated INTENT was to spread the useful range; the exponent was
inverted relative to its own goal. Now hidden (settled value), but still a
parameter so a patch can carry a different taper. Existing patches store their
own value and are untouched.

### freq glide is NOT gated to sample-and-hold — checked, not assumed

The human asked whether it should nest under S&H. It should not: `glideOn` is
`p.freqGlide > 0` at both sites (`swarm_core.h:791` and `:1373`) with NO
drift-mode condition, and the render substitutes the smoothed `fRun` for `eff`
whenever it is on — for every drift mode, and for any other jump in effective
frequency (a detune move, a law change, gravity). It is a general de-zipper that
is MOST AUDIBLE under S&H because that is where `eff` steps discontinuously,
which is why the `freq-glide` golden uses driftMode 2. Gating it would hide a
control that still does something, and would make the dependency graph assert
something the engine does not do — the one error `depends_check` structurally
cannot catch.

### The ⊘ that printed as text

`content: ' \2298'` had been written through a Python string that doubled the
backslash, so CSS saw an escaped backslash followed by literal `2298`. One
character.


## ADR-110 — the right-click becomes a MENU, and the morph gets colour (2026-08-22)

**Status:** accepted, shipped.

### The menu

ADR-109 shipped exempt as a BARE right-click. That made the *gesture* the
feature: there was exactly one thing right-click could ever mean, and the human
asked for a menu precisely because more items are coming ("I intend to grow the
right-click menu in the future").

`PARAM_MENU` in `src/gui/gui2.html` is an item registry. An entry declares
`show` / `label` / `hint` / `run`. Two consequences worth stating because they
are the reason for the shape:

- `show` decides whether an item **appears at all**, not whether it is greyed.
  An item that cannot apply here is absent. A disabled row that you can click
  and get nothing from is the same failure as ADR-109's silent exempt — a
  control that lies about being a control.
- `label` takes the context, so a toggle is ONE entry with two faces
  ("Exempt from morph" / "Return to morph field") rather than two entries
  racing to hide each other.

The header carries the parameter name **and its id** (`#75`), because the human
reports bugs by naming parameters and the id is what the code answers to.

**Reset-to-default was lifted out of the `dblclick` closure** into
`resetToDefault(el)` and both callers share it. Copying that body into the menu
would have been a second implementation of a default lookup — the exact drift
the comment inside it already warns about.

### Colour coding

Each row now carries a 3px stripe in the colour of the corner whose value it is
currently showing — the same four colours as the morph pad and the arm boxes, so
the pad's geography and the panel agree at a glance.

`morphOwnersJson()` answers it from **the engine's own `pickCorner`, with the
same `morphGroupLead`** the audio path uses. A GUI-side re-derivation would have
been a second implementation, i.e. a map that can lie about the sound.

Two contract details:

- **-1 means nobody owns this** (field off, or exempt). The GUI must not tint
  those; an unowned row is a row showing a value no corner is responsible for.
- **Every id is emitted, always — even with the field off.** The key's PRESENCE
  is the membership answer, and the menu needs membership to decide whether
  "Exempt" applies at all. An early `return "{}"` when morph was off (the first
  draft did exactly this) would have silently hidden the item whenever the field
  was off — a bug indistinguishable from the ADR-109 one it replaced.

### The oracle, and the check that was blind

`corner_probe` cases 6 and 7. Case 6 asserts the map is populated (>40 entries)
BEFORE reading anything out of it: every later assertion in that case is
satisfied by an empty map, so without the population control the whole case is a
check that cannot fail (L0024/L0032).

Case 6's remaining assertion — "a live parameter names a corner in 0..3" —
is still too weak, and this was demonstrated rather than argued. Planting a
defect that made `morphOwnersJson` always report corner A left case 6 **fully
green**; only case 7 went red. Case 7 asserts the claim the stripe actually
makes to the eye: author corner A = 0.11 and corner D = 0.88, then at each pad
corner require the reported owner AND the sounding value to agree. That binds
the colour to the sound, which is the only property worth having.

The lesson is L0039's: a range check inherits the coincidences of whatever
sample it was written against. `k == 0` is indistinguishable from a stale read,
a constant, and an uninitialised zero.

### Evidence

`corner_probe` 12/12 · plant → RED on case 7, green on case 6 · plant removed by
reverse-replace, `grep -c PLANT` = 0 · `./verify full` EXIT=0 · parity 156/156
(worst 4.262e-09 @ dyn-ring.seed42) · depends_check GREEN (57) · state_check
GREEN · paramscope_check GREEN.


## ADR-111 — the accidental mode gets a name, and arming gets a view (2026-08-22)

**Status:** accepted, shipped.

### The phenomenon, diagnosed

The human: *"When I change the notes in a chord, it now seems to quantize the
other notes (those already held down) into a new scale. Maybe this only happens
when I play a note that's out of scale. It sounds very cool, but it should be
its own mode."*

Every clause of that observation is confirmed by the code path:

1. `lastNoteKey` updates on EVERY note-on (`hypersaw_clap.cpp:2928`).
2. The global wheel lane quantises absolute pitch `lastNoteKey + x`
   (`hypersaw_clap.cpp:3259`) — the base-anchoring that ADR-10x installed so
   bends land on real scale tones.
3. A note whose class is OUT of the scale therefore produces a nonzero
   correction at rest: `q = nearestScaleTone(key) − key`.
4. That lands in `pitchBend`, which is applied through `tune` — a
   post-lane multiplier on every voice (`updateTuneAll`) — so the ENTIRE
   sounding field transposes by the newest note's correction, intervals
   intact. A uniformly transposed chord is the same intervals from a new
   root, which the ear reads as "re-quantised into a new scale."
5. In-scale notes correct by zero — hence "maybe only out-of-scale notes."
6. It sounds deliberate rather than glitchy because the retune commits on the
   step-time grid and sticks via hysteresis — the lane's own pacing, applied
   to a correction nobody designed.

### The ruling

The behaviour is kept BIT-EXACT and named **scale (drag)** — the newest note is
conformed onto the scale and drags the whole field with it. The expected
behaviour is the new **scale** (anchored): the anchor's own pitch class is
always admissible, so an out-of-scale anchor rests at exactly zero correction
and held notes never move; a travelling bend still lands only on admitted
pitches.

Value assignment is APPEND-ONLY on purpose: existing patches stored 2 and were
saved while sounding drag, so 2 keeps drag under the honest label and 3 is the
newcomer. Remapping 2 → anchored would have silently changed the sound of every
stored patch to "fix" a behaviour the human explicitly asked to keep.

### Where the mode lives, and where it must not

- `glide_core.h` gains `kQuantScaleAnchor = 3`. `kQuantScale` stays strict
  forever: it mirrors the reference, and the goldens cover it with
  out-of-scale bases (`glide-quant-root3` — base class 0 excluded by the
  root-3 mask), so changing its semantics would have gone RED. The anchored
  variant is a parity-safe superset the mono reference cannot even express —
  the phenomenon needs polyphony the lab does not have (L0031, fourth
  instance).
- Per-note MPE lanes pass each note's own key as base, so anchored is
  correct there with no extra work: an MPE bend can always return to its own
  note.
- The FOLLOWING note-glide lane clamps anchor → strict (`pushNoteLaw`,
  beside the retMul precedent): its base is `kLogFreqToMidi`, a
  unit-alignment constant, not a note — "admit the anchor's class" would
  admit pitch class 0 forever.
- The note lane's own label array split off (`kNoteQuantLabels`): "drag"
  describes what the GLOBAL lane does with a correction; a per-voice lane
  has no field to drag, so its value 2 stays plain "scale".

### The armed corner view

Arming a colour box now shows THAT corner's stored settings in place —
`morphCornerValsJson(k)` — instead of the live morph output: you look at what
you are editing. Rows the armed corner is currently voicing keep their solid
stripe; rows it owns on paper but is not voicing go dotted in the armed colour
(the human's "provisional ghost"). Both live-repaint sites (the poll and the
OSC-tab switch) go through one guarded `paintLive()`, so a third site cannot
quietly forget the rule. Exempt rows stay live — no corner owns them, so the
armed view has nothing truthful to show there.

### Evidence

`glidepath_probe`: drag moves a held C4 to ~247 Hz on an F#4 strike, anchored
holds ~262 Hz, at 44.1k AND 48k — the drag case doubling as the
must-discriminate control for the anchored one. `glide_check` invariants:
strict corrects an out-of-scale anchor (+1) while anchored rests at exactly 0,
and a travelling anchored bend emits 4 admitted steps, 0 alien — anchored is
neither strict nor secretly off. `corner_probe` 13/13 (case 8 binds the
armed view's data source to the values case 7 authored). Parity untouched:
glide 3.51308e-08 worst, same as before the change.


## ADR-112 — movable-do, a wheel on screen, and an honest preset selector (2026-08-22)

**Status:** accepted, shipped.

### scale (offset) — the played note is the tonic

`kQuantScaleOffset = 4`: the mask is applied RELATIVE TO THE ANCHOR — whatever
note you play is the tonic, and a bend walks the scale SHAPE from there.
`scaleRoot` is deliberately ignored (a root and a movable tonic cannot both be
in charge), and the presentation graph now says so: the Root row hides when
only offset is quantising while the twelve degree rows stay live — the
dependency column earning its keep. The anchor's class is admitted even if
degree 1 is switched off, because without that every strike would carry a rest
correction into the global lane and the mode would drag by accident — the
exact surprise ADR-111 just spent a mode naming.

Evidence: `glide_check` — base 61 under a C-major mask rests at zero and lands
a +2 bend on exactly 63 (the "re" of C-sharp major) while strict corrects the
rest (−1) and lands 62; one gesture separates strict/offset/off, so a mode
that collapsed into either neighbour goes RED. `mpe_check` — offset joins the
drag/anchor sweep and never moves the held C4.

### The stack, measured instead of asserted

The ADR-111 handoff flagged a hypothesis: note-lane quantise stacking with the
global drag correction. A scratch probe (not committed) measured it:

- **Plain poly: NO stack.** BND-5 by construction — the note lane cannot move
  a freshly played note, so a lone F#4 under drag sounds one correction
  (393.8 Hz — G, the tie toward the A4 boot anchor), law on or off.
- **Mono/legato: the stack is real.** C4 → legato F#4 under drag + lag +
  FOLLOW sounds **E4** (329.1 Hz): the glide landing is quantised per-voice
  (−1) and drag transposes globally (−1). Anchored lands F4 — the note-glide
  quantise alone, which is that feature doing its stated job.

The ruling on whether mono's double-correction is wanted — and whether the
note lane should get target-anchored semantics — is B30's, with these numbers
attached.

### The wheel

Param 38 IS the wheel: it sets `bendTarget` and rides the whole bend path —
law, quantise, every scale mode — which is exactly what the human could not
audition without hardware. The on-screen wheel springs back to the
**pre-gesture value**, not to zero: 38 lives in the morph field, so a corner
may legitimately hold a nonzero Pitch, and a wheel that springs to zero would
stomp it on every release.

### The preset selector

Starts on a disabled `Load…` placeholder. The first real preset must not look
loaded when nothing is; the placeholder can be departed from but never chosen,
and save/load re-select the real name explicitly.

### Evidence roll-up

`./verify full` EXIT=0 · parity 156/156 untouched · glide_check GREEN (offset
invariant discriminating) · mpe_check GREEN (drag/anchored/offset sweep, 40:1
bins) · rows BND-23/24/25, PRE-5.


### ADR-112 A1 — the scale picker follows the degrees, not the root (2026-08-22)

Human: "keep the scale selector for easy autofill and just drop the root
selector" in offset mode. The picker had inherited Root's gate (the 2026-08-20
fix that stopped it showing unconditionally); offset mode is the first place
the two gates DIVERGE — Root dead, degrees live — and the picker autofills the
degrees, so it follows their gate. The general rule both fixes share: a
control is gated with what it EDITS.


### ADR-112 A2 — the morph rate slider that already existed (2026-08-22)

Human: "there isn't a morph rate slider." There was — id 158, mislabeled
"Flip Glide" and GUI-gated to flip mode only, while `morphStep` runs EVERY
target through its coefficient in both modes. A control that affects blend
mode but hides in blend mode is the gating rule inverted: gate with what it
EDITS (A1's rule), and it edits the whole field. Relabeled "Morph Glide (s)",
gate widened to `morphOn=1`, max 0.5 → 5 s (a performance morph time, not a
de-clicker; stored patches keep their values, CLAP params are plain-valued).


### ADR-112 A3 — the morph field rides the session, not just the preset (2026-08-23)

Human: "I don't think sessions save the position of the morph XY." The
position was the one part that DID save — X/Y are ordinary params and both
state paths round-trip them (probed: 0.830/0.210 through host state AND the
JSON preset path). What sessions lost was everything else about the field:
`state_save`/`state_load` (the HOST path — what a DAW session uses) carried
no morph chunk at all, so the four corner snapshots and the exempt set
silently dropped. On reload every live param restored and the corners lazily
re-initialised FROM those live values — a degenerate field, all four corners
identical, where moving the pad changes nothing. "Sessions don't save the
morph" is exactly what that feels like from the chair.

Fix: ONE parser (`applyMorphChunk`), extracted from applyStateJson and called
by both paths; state_save appends the chunk as a single opaque
`morph=<json>` line. Old builds ignore the unknown key; new builds loading
old blobs simply have no line (lazy init, exactly the prior behaviour) —
append-only compatibility both directions, no version bump.

Oracle: state_check authors corner D to 0.777 while the live value ends at
0.3 — a split a lazy re-init cannot produce — through REAL process blocks
(the first draft authored via params_flush, which in a no-audio-thread
harness only enqueues: it authored nothing and could not fail for the right
reason — L0024's shape again). The blob-text assertion ("0.777" present)
pins the middle so a symmetric writer/parser no-op cannot pass. Plant
(remove the save line — the shipped bug) → RED on 4 assertions; reverse-
replaced, verify full EXIT=0.


### ADR-091 A1 — the formant click, diagnosed; and F2 built as a lab (2026-08-23)

Human, playing the formant prototype: "There is occasionally a little clicking
at the lower frequencies. Could we add a polyphonic mode? I also want to hear
controls that add detune width to the formants."

**The click is grain truncation, and it is register-dependent by construction.**
A FOF grain's envelope is `exp(-pi*bw*t)`; the reference ends it at
`dur = min(0.06, tex + ln(1000)/(pi*bw))` — the -60 dB point, OR a 60 ms cap,
whichever comes first. The register state narrows bandwidth as pitch falls
(`bwk = 2^-1.6R`), which LENGTHENS the decay, so low notes hit the cap and the
grain vanishes at a finite amplitude — a step. Every formant spawns on the same
period boundary, so all five steps align and sum into one click per period.
Computed, then confirmed live in the lab's own readout:

| f0 | R | F1 bw | decay needs | grain ends at |
|---|---|---|---|---|
| 220 Hz | 0.00 | 59.2 Hz | 39 ms | -62.4 dB (finishes; no click) |
| 55 Hz | 0.67 | 28.3 Hz | 79 ms | -46.3 dB (truncated) |
| 27.5 Hz | 1.00 | 19.5 Hz | 114 ms | **-32.0 dB (clicks)** |

With `bw scale` down the same mechanism reaches -13 dB. Fix: ramp the envelope
to zero over the last ~2 ms so truncation cannot step, whatever ended the grain.
`grain fade = 0` reproduces the reference exactly, so bug and fix are one A/B and
the readout names which regime you are in.

**Why a new file.** `horde_formant_pulsar_fof.html` is the ingested reference and
a protected path — it IS the spec, and polyphony is an architectural claim about
what the engine is, exactly the decision that belongs in a lab that can be A/B'd
and ratified rather than slipped into the reference. Same shape as glitch-lab vs
horde_decoherence_lab. This lands ROADMAP **F2**, which already asked for the
polyphonic-choir lab and posed three candidate coupling sites; `choir K`
implements the formant-mass one.

**Measured while building, not assumed:** two voices at 55 Hz and 440 Hz report
R 0.667 and R 0.000 — per-voice register confirmed. Stereo width verified by
L/R correlation with its own control: copies 1 → 1.000 (inert at the reference
setting), 4 copies with width 0 → 1.000 (so the decorrelation is the width
control, not merely having copies), width 1.0 → 0.9755. A poly trim was added
because the reference's mono drive stage pins the tanh on a chord (1 voice peak
0.84, 6 voices 1.00); it divides by sqrt(voice LIMIT), never the live count,
since scaling by the live count would duck held notes as new ones arrive.

**The lab-load gate earned its keep**: it caught a bare `devicePixelRatio`
(window-only; a ReferenceError in the headless check) that no amount of
in-browser testing would have surfaced.


### ADR-091 A2 — the polyphony crash was a grain budget, on two threads (2026-08-23)

Human: "Something keeps crashing when I try polyphony." Reproduced and measured
before touching anything; it was two compounding failures, one per thread.

**Audio thread — unbounded grain count.** Every grain costs a `sin()` per
SAMPLE, so cost is (total grains x sample rate). The cap was per-voice (500), so
the total scaled with voice count and with formant-detune copies:

| setting | live grains | events/s | biggest message |
|---|---|---|---|
| 1 voice, 1 copy | 125 | 1,313 | 12 |
| 6 voices, 1 copy | 1,249 | 10,403 | 84 |
| 8 voices, 5 copies | **3,221** | **24,600** | **572** |

3,221 grains x 44.1 kHz = 142M sin/s. The tell was the worklet's own message
rate falling from 187/s to 52/s — it was rendering at ~30% of real time.

**Main thread — an O(n) trim in a hot loop.** `while(len>4000) shift()` at
24.6k events/s moved ~98M array elements per second. That is the freeze the
human saw as a crash.

**Fixes.** A GLOBAL grain budget (900, divided by the voice limit) spent at
SPAWN: a voice out of budget stops emitting and its train thins. It never culls
a sounding grain — culling mid-flight is exactly the truncation discontinuity
the de-click (A1) exists to prevent, so enforcing the cap by splicing would have
shipped the click back as a crash fix. Events are bounded at the source (96 per
post: the train is a picture, not a log), and the UI trim is one `splice`
instead of N `shift`s.

**Verified end to end, with the page's own handler intact** (the first
measurement bypassed it, which would have measured the wrong thread): grains
874 at the worst setting, worklet at full real-time (181-188 msg/s) at every
configuration, main thread 60 fps with an 18 ms worst frame. Stress: 240
note-ons through 8 slots (232 steals) peaked at 866 grains and drained to
exactly 0 after all-off — no leak.


### ADR-091 A3 — the polyphony crash, properly: grains were transcendental (2026-08-23)

A2 bounded the grain COUNT and I reported it fixed. The human came back: "I'm
still seeing it; once polyphony tries to exceed 6, it kills audio." A2 was a
real fix to a real bug and it was not enough — it moved the wall instead of
removing it, because it never addressed what a grain COSTS.

**Each grain evaluated two transcendentals per sample** — `sin(2*pi*fc*t)` for
the carrier and `exp(-pi*bw*t)` for the decay. At A2's ~870-grain ceiling that
is ~77M library calls a second, so a machine also running a DAW still starved
and the audio died. Every one of those closed forms is the solution to a
recursion, so the recursion is what we run now: rotate a unit vector by w per
sample (its y IS sin(w*n)), multiply by exp(-pi*bw/sr) per sample, rotate at
pi/tex for the raised-cosine attack. **Verified equal to the closed form to
1e-14** over a full grain life at F1, F5, sub and pulsar — float64 rounding,
not an approximation.

Also fixed: `grains.splice(k,1)` ran INSIDE the per-sample loop, O(n) per
ending grain. Swap-with-last + pop is O(1), and the backward loop means the
element swapped in was already summed this sample.

**The meter that could not fire.** I first put a CPU meter in the worklet timing
render() against the block budget. It read 0% forever: `performance` is
UNDEFINED in AudioWorkletGlobalScope (probed: `typeof performance ===
"undefined"`). A meter that always reads zero is worse than none. The honest
signal lives on the main thread — when the worklet cannot keep up, the AUDIO
clock advances slower than the wall clock, and that ratio is exactly "is it
killing audio". The lab now prints it.

**With a must-fire control, because an "OK" that cannot say otherwise is
worthless.** Raising grain budget alone did NOT overload it (density is capped
by spawn rate and lifetime, not by the budget), so the control raises the grain
CAP too: budget 4000 + cap 300 ms + 5 copies at a high octave reached 3909
grains and the detector read **43%** — brackets the original crash at 3221
grains, and matches the human's report exactly. At defaults, poly 6/7/8 each
with three extra steals: 100%, ~1000 grains, drains to 0 on release.

**The lab-load gate caught what testing could not**: six stray backticks in
comments I added INSIDE the worklet's template literal, which terminated the
string. Then my own repair swept too broadly and replaced the template's own
delimiters — caught again, same gate, same run.


### ADR-091 A4 — polyphony reverted, and both my instruments were blind (2026-08-23)

Human: "Every time you've run a test, the audio has cut out after 6 voices even
though the output displays continue as normal... I'm not terribly impressed with
how this sounds as a polyphonic instrument anyway, so maybe we should just
revert to the monophony."

**The measurement failure is the finding.** Across three rounds I reported
healthy audio from two instruments, and the human — listening to the same runs —
heard the output cut out every time. Both instruments were structurally
incapable of seeing the failure they were pointed at:

- **AnalyserNode RMS taps the GRAPH, not the device.** It reports what the node
  computed and keeps reporting its last buffer when the worklet stalls. It is
  loudest exactly when the speakers have gone quiet. Every "rms 0.6, audible:
  true" line in A2/A3 is this.
- **ctx.currentTime vs wall clock is the audio DEVICE clock**, which keeps
  advancing through an underrun — the device clocks out silence perfectly on
  time. It only moved (43%) under a catastrophic 3909-grain overload, which is
  why the must-fire control "passed" while the real regime went unseen.

This is the trap the project memory already names: a probe that confirms the
expected answer for the wrong reason. I built the must-fire control for the
detector's *sensitivity* and never asked the prior question — whether the
quantity being measured is downstream of the fault at all. A control proves a
check can fire; it does not prove the check is pointed at the right thing.

The honest instrument here is a recording of the OUTPUT
(MediaStreamDestination, checked for gaps). Until that exists the lab shows no
audio-health number at all, because a number that argues against the user's
ears is worse than no number — it cost three rounds.

**The revert.** Polyphony is out; `lim` is pinned to 1 with the voice machinery
left standing, so restoring it is one line if the engine earns it. Kept, because
none of it depends on polyphony and all of it was asked for or fixes a reported
bug: formant detune width, the grain de-click (with its A/B and live readout),
the recursive grain (verified equal to the closed form to 1e-14 — a real
speedup that stands on its own), and seeded masking. Removed with the
polyphony: the voice strip, choir coupling, poly trim, grain budget control, and
both audio-health readouts.

**Standing question for the human, not answered here:** whether CANTO earns a
place in horde at all ("I'm not entirely convinced this one will make it into
Horde"). ROADMAP F2 is closed as *tried and reverted*, not as delivered.


### ADR-113 — GUI1 demoted; gui2 is the shipped interface (2026-08-23)

Human: "update the readme and now demote GUI 1."

The 2026-08-07 condition for the swap was "GUI2 swaps in when it reaches
parity." It has, on both readings: reach (`gui_reach` measures gui2 at 142/159
declared parameters against gui.html's 102/159) and scope — GUI1 predates the
second oscillator, the mixer, the FX rack and the morph engine and cannot show
any of them. It stopped being a smaller view of the same instrument and became
a view of an older one, which is what makes it legacy rather than an
alternative.

`HYPERSAW_GUI2` now defaults **ON**. GUI1 is kept building behind
`-DHYPERSAW_GUI2=OFF`, and that escape hatch was verified rather than asserted:
a clean configure with the flag off resolves `src/gui/gui.html` and compiles
green.

Worth stating because it is exactly the kind of thing a green gate hides:
`gui_reach` is an **either-GUI** check, so it stays green while gui2's 17-param
gap exists. The README now names that gap instead of leaving a reader to infer
coverage from a passing gate.

The README was stale on nearly every number (147/147 parity, 42 tests, 18 labs,
30 gates, "105 params", a 2026-08-15 date). Refreshed from a measured
`./verify full` in the same change: 156/156 parity, 124 tests, 22 labs, 31
gates, 159 declared params. Also added: the two-oscillator cost measurement, the
morph engine, the formant engine's reverted polyphony and open future, and a
Known Gaps entry recording that in-page WebAudio health readouts are not
trustworthy (ADR-091 A4) so no future session rebuilds one.


## ADR-114 — the device is horde; HYPERSAW is the engine (2026-08-23)

**Status:** accepted, shipped.

Human: "The device is going to be called horde instead of hypersaw." That closes
the question §Domain had been carrying as *"working product title horde; naming
open until Phase 5"* — it is no longer open, and no longer working.

The name now resolves to three different things, and keeping them distinct is
the whole content of this ADR:

- **horde** — the DEVICE. What a host shows in its browser, what the README
  leads with, what a user calls it. The CLAP descriptor's display string moved
  here.
- **HYPERSAW** — the founding ENGINE, and the label the engine selector shows.
  Unchanged; SPECTRA is its sibling in that same list. Also the repo's name,
  which is left alone (renaming a repo breaks every link ever shared to it, for
  no gain a redirect would not give).
- **`com.lifted-truck.hypersaw`** — the plugin ID, **frozen forever**. This is
  how every host re-finds the plugin in an already-saved session. Renaming it
  does not rename the plugin; it creates a *different* plugin and orphans every
  project that ever loaded the old one. It is an identifier that happens to read
  like a name, which is exactly why a future session will be tempted to tidy it
  — the comment at the descriptor says so in place.

The README was rewritten around the same decision, and around the human's
complaint that it was "mostly a graveyard of old concepts, nothing about the
current functionality or the morph grid or the roadmap": it now leads with what
the instrument does, gives the morph grid its own section (it is the feature
that makes horde a patch explorer rather than a synth with many knobs), carries
the five new screenshots, and summarises where the work is going. The
archaeology — the two-interface justification, the phase-by-phase history, the
retired house mark — is gone; ROADMAP.md and this log already hold it, and a
README repeating them is a second copy free to drift.


## ADR-115 — the field starts at corner A; the engine is SWARM SAW (2026-08-23)

**Status:** accepted, shipped.

### Morph X/Y default 0.5 -> 0.0

Human: "the default setting should probably put the morph grid at 100% corner A
instead of in the middle, since the middle is the messiest place on the grid and
the most confusing to edit."

Both halves of that are true, and they are the same fact seen from two sides.
`weights()` is bilinear, so at (0.5, 0.5) every corner weighs exactly 0.25 —
the point of MAXIMUM disagreement in the field. Two consequences:

- **What you hear** is a patchwork drawn from all four corners at once. Harmless
  at init, when the corners are identical copies of the defaults; maximally
  scrambled the moment you author them, which is the moment a new user first
  tries the feature.
- **What you edit** scatters the same way. An unarmed edit lands on whichever
  corner owns that parameter (ADR-109), so at the centre consecutive edits land
  in different corners with nothing on screen explaining why.

At (0, 0) every parameter is owned by A, so the field behaves exactly like a
plain patch until the user chooses to move. The strangeness becomes opt-in
rather than the first thing encountered.

**`paramscope_check` earned its keep here.** ADR-107's default-truth sweep went
RED immediately: the ParamDef said 0.0 while the member initialisers still said
0.5 — the same ParamDef/member split that bit ADR-024 A1. The gate named it as
"2 lie(s)" on a fresh instance before the change could ship.

### The engine is SWARM SAW

Human: "instead of hypersaw, the engine itself should be called swarm saw."

SAW -> HYPERSAW (ADR-091) -> **SWARM SAW**, which returns the engine to the
lineage its own prototype never left (`swarmsaw.html`, `SwarmSynth`,
`swarm_core.h` — none of which ever said HYPERSAW). Chosen in caps to sit beside
SPECTRA in the same selector.

With the device named horde (ADR-114), **"HYPERSAW" is now off the product
surface entirely** — it survives only as the repository name and the frozen
plugin id `com.lifted-truck.hypersaw`. Both stay: renaming a repo breaks every
link ever shared to it, and renaming the id orphans every saved session. As in
ADR-091, the parameter VALUE and the state key are untouched — a label is not an
identity, and every stored patch keeps loading.

Left alone deliberately: the log directory `~/Library/Logs/HYPERSAW` and the
`"plugin":"HYPERSAW"` marker in the state JSON. Both are identifiers rather than
display strings, and the second is written into every preset a user has already
saved.


## ADR-116 — the horde UI spec, ingested (2026-08-23)

**Status:** accepted as the design of record; **not implemented**.

`HORDE UI Spec.dc.html` arrived from a Claude Design session, written against
this repo's gui2. Filed at `docs/design-system/` as delivered — not transcribed
to markdown, because it is ~200 exact values (hexes, pixel counts, angles,
timings) and a normative document with two copies is one that will disagree with
itself.

### What it is

Format-neutral values in §1–7 (normative for any stack), a webview build in §8
(CSS verbatim, normative for gui2 today), and a native-port appendix in §9 for a
C++ painter that does not exist yet. It declares its own precedence: **where they
conflict, §1–7 win.** That single line settles every future argument about
whether the CSS or the design is authoritative.

### What it changes about the shipped interface

This is a **re-skin, not a restyle**:

- **Ground inverts.** Spec is cream (`#F2EDE2`); gui2 is near-black (`#0b0e13`).
  Every colour decision in the current file was made against a dark ground.
- **The corner palette moves** — A `#FFD702`, B `#FF2E88`, C `#00D5C8`,
  D `#A6F219`, each with a dark text pair and a 10% tint.
- **Semantic colours get one job each and may not be borrowed** — value/physics/
  meter/marker/celebrate/caution/alarm/dry-ghost. The rule with teeth: red
  appears at most twice on a page (PANIC + clip) "or it stops meaning danger".
- **Fonts become bundled TTF** (Archivo + Roboto Mono, offline). gui2 has **zero**
  `@font-face` rules today and rides system monospace.
- **Hard-shadow, flat-ink language** (1.5px offset, no blur) — a visual idiom the
  current interface does not use at all.

### THE TRAP TO FIX FIRST

gui2 **already defines the corner palette twice, and the two copies already
disagree**:

- `MCOLORS` (JS, line ~927): `#5ff2e0 #ffc24b #b18cff #ff7d9c` — drives the pad
  washes, the arm boxes, the per-row ownership stripes and the corner labels.
- `--cA…--cD` (CSS, line ~21): `#f2b134 #ff4d6d #4cc9f0 #7ae582` — **consumed by
  nothing** (0 references).

They are not the same colours. The CSS set is currently dead, which is the only
reason nobody has seen it: the first person to style something with `var(--cA)`
gets a colour that does not match the pad, and corner identity — which the spec
makes load-bearing across five surfaces — silently splits in two. **Any re-skin
must land on ONE palette source**, and the dead set must die rather than be
updated alongside. This is the same two-copies failure as the JSON state twins
and the corner-owner map, found before it could cost anything.

### What it specifies that does not exist yet

Mod halos and live dots (no modulators — B16/B23), the census bar, FX module
corner strips, the temperature wheel, the goniometer, the LFO rotor, the envelope
node editor, and the generative wordmark warp (§5 — which connects to the logo
work already parked). The spec is ahead of the instrument on purpose; it should
not be read as a list of things that are broken.

### Not delivered

The spec's last line points at page compositions living in a **Design Directions**
file, and the human also named `HORDE Design System.dc.html`. Neither arrived. So
we hold tokens and widget behaviour but not page layouts — the better half to
have first, since layouts can be re-derived from tokens and not the reverse.


## ADR-117 — the design system's second half, and where the two documents fight (2026-08-24)

**Status:** ingested; two questions escalated to the human.

`HORDE Design System.dc.html` arrived, completing the pair. Filed beside the UI
spec. Also delivered: `hp-support.js`, which the human flagged as possibly
needing reconnection — it is **byte-identical** to the `support.js` already
vendored (same SHA-256, and all three `.dc.html` files reference `./support.js`),
so there was nothing to reconnect and there is one copy, not two.

### The division of authority is clean, and self-declared

- **UI Spec** owns VALUES: "§1–7 … normative for both stacks."
- **Design System** owns COMPOSITIONS — MAIN, OSC, FX, MIX, bend+scale — and
  defers on values in its own header: "Values are normative in HORDE UI Spec."

So the layout half of B37 is unblocked, and a hex is never argued about twice.

### Value conflicts — settled by that rule, recorded so nobody "fixes" them backwards

| | UI Spec (wins) | Design System |
|---|---|---|
| window | 1180 × 820, min 1000 × 700 | 1180 × 780, min 1000 × 660 |
| shadow | 1.5px offset | 2px offset |
| meter fill | `#A6F219` | acid `#9BE514` |
| `well` | `#FFFFFF`, `well-alt` `#E9E3D4` | `well` `#E9E3D4` |

Corner colours, reassuringly, **agree** across both: A `#FFD702`, B `#FF2E88`,
C `#00D5C8`, D `#A6F219`.

### Two conflicts the precedence rule CANNOT settle — human ruling needed

These are not values, so "UI Spec wins" does not apply:

1. **Glyphs.** UI Spec is explicit twice — "no glyphs (one selector)" and
   "NO glyphs anywhere"; corner identity is colour + preset-name chip. The
   Design System gives every corner one (GLASS ◆ / GRIT ▲ / HOLLOW ● / BLOOM ■)
   and states "glyph always rides with colour". Worth deciding on the merits
   rather than by document precedence, because it is an **accessibility**
   question first: a glyph is how corner identity survives colour-blindness.
   The UI Spec is aware of this and answers it differently — its §8
   accessibility note assigns that job to the preset-name chips. So both
   documents solved the same problem; they just picked different solutions, and
   shipping both would be redundant rather than harmless.
2. **Re-theming.** UI Spec: "corner colours never re-theme." Design System:
   "corner identity becomes per-direction, not global." Only matters if a second
   visual direction ever ships — but it decides whether corner colour is a
   constant of the instrument or a property of a skin, which is a much larger
   claim than it looks.

### What the Design System settles that the repo had open

Not decisions we can adopt unilaterally — they are design proposals against
unruled engineering — but worth noting they exist:

- **The scale panel's two skins** (GLOBAL piano / OFFSET dominoes, one control,
  the skin *is* the mode indicator, root selector greys out in OFFSET). A direct
  answer to a UI question the repo has carried since the scale work.
- **Routing BLEND vs ARGMAX as a VIEW toggle**, both laws reading the same dense
  table — "switching is a view change, not a patch change". That reframes B23's
  unruled law as something a user can audition rather than something we must
  decide before building.
- **FX module anatomy** — one card every future module inherits, with the corner
  strip as the census landing site.
- **MASTER as the morph-exempt zone** with a permanent lock, matching the
  exemption feature already shipped (ADR-109).

It is explicit about what it does NOT settle, mirroring the repo's own open
list: routing law, module order semantics, per-module internals, bass-mono
ordering (B23).


## ADR-118 — re-skin increment 0: one palette, and a bridge to the canvas (2026-08-24)

**Status:** shipped. **Deliberately invisible.**

The human asked to start the re-skin. The first increment paints nothing: it
removes the two obstacles that would have made a recolour produce wrong results
for reasons nobody could see.

### 1. The corner palette existed twice, holding different colours

`--cA…--cD` in CSS said `#f2b134 #ff4d6d #4cc9f0 #7ae582`. `MCOLORS` in the
script said `#5ff2e0 #ffc24b #b18cff #ff7d9c`. The CSS set had **zero
references**, which is the only reason nobody ever saw it — the first
`var(--cA)` written during a re-skin would have split corner identity across the
five surfaces that carry it (pad washes, arm boxes, row ownership stripes,
corner labels, ghost rows). Now declared once; `MCOLORS` reads it.

### 2. A canvas cannot read a CSS variable

Every painted colour in this file is a string literal — 17 distinct ones across
35 paint sites, and three of those pairs are the same intended colour typed
twice: `#e8ecf4`/`#e8ecf2`, `#0b0e13`/`#0b0e14`, `#7f8899`/`#7c8698`. That is a
palette rotting by hand, and it is why "change :root and everything follows"
would have been false for exactly the surfaces the spec cares most about.

`TOK()` reads a custom property once and caches it, so the stylesheet can become
the source for painted colour too. It falls back to a literal when there is no
computed style, because the headless lab-load gate runs this script without a
layout engine and a bridge that throws there would take the file down.

### THE FINDING THAT MATTERS MOST FOR THE RE-SKIN

**`--cA` is byte-identical to `--pull`, and `--cB` to `--amber`.** Corner A and
the interface's accent colour are the same string; so are corner B and the
warning amber. Nothing in the file distinguishes "this is corner A" from "this
is the accent", so moving corner A to the spec's `#FFD702` by find-and-replace
would silently drag the accent with it — or, done via the token, would move
corner A while leaving accent-coloured things behind, and only some of the
surfaces would follow.

**Increment 1 must separate the two roles BEFORE changing either value.** This
is the same class of failure as the two palettes above: one string doing two
jobs is indistinguishable from one string doing one job, right up until you
change it.

### Verified as a no-op, after fixing the oracle

First attempt hashed every canvas and reported a difference — but four of them
(`xy`, `phase`, `spec`, `carpetC`) **animate**, so their pixels differ between
page loads regardless of any change. Hashing them across loads was measuring the
clock. Re-run against the five stable canvases plus computed styles:

- computed styles on 12 representative selectors: **no differences**
- `wavscope`, `gviz`, `morphPad`, `stripC`, `vmapC`: **byte-identical hashes** —
  `morphPad` being the one that paints corner colours
- `MCOLORS` still reads `#5ff2e0 #ffc24b #b18cff #ff7d9c`, now from the stylesheet


### ADR-118 A1 — increment 1a: corner colour and accent colour become distinguishable (2026-08-24)

Increment 0 found that `--cA` is byte-identical to `--pull` and `--cB` to
`--amber`, which made a recolour impossible to do correctly: nothing in the file
said whether a given `#5ff2e0` meant "corner A" or "the accent". This increment
classifies every one of them. Still deliberately invisible — values unchanged.

**A THIRD copy of the corner palette turned up.** The arm-box builder held its
own inline array `['#5ff2e0','#ffc24b','#b18cff','#ff7d9c']`. ADR-118 found two
copies; this is the third, and it is the one whose drift would have been most
visible — the arm boxes sit next to the pad they must agree with. Now `MCOLORS[k]`.

The rest are **not** corner identity, and saying so in the code is the point:

- two-cluster colouring in the phase circle and the carpet — a *data dimension*
  (which cluster a voice is in)
- L/R in the scope — a data dimension (channel)
- root-vs-other in the scale strip — *marker* semantics, which the new spec gives
  its own token (`marker #FFD702`)

All now read `TOK('--pull')` / `TOK('--amber')`, so when the re-skin moves corner
A to yellow these stay put — which is exactly the failure increment 0 predicted.

**Four ad-hoc token readers folded into TOK.** Draw paths were calling
`getComputedStyle(document.body).getPropertyValue('--pull')` — inside animation
loops. That is a forced style recalc per frame, and it was a hand-rolled TOK
without the cache. Correctness and cost, one edit.

**The lab-load gate caught a TDZ.** Pointing the arm boxes at `MCOLORS` created a
use ~300 lines before the declaration. The bridge is now hoisted to the top of
the script, where a palette belongs. Verified no-op afterwards: computed styles
unchanged on 7 selectors, the five non-animated canvases byte-identical, and the
arm boxes measurably painting `rgb(95,242,224) / (255,194,75) / (177,140,255) /
(255,125,156)` — the same four colours, now from the shared source.


### ADR-118 A2 — increment 1b: the canvas paints from tokens; and a correction (2026-08-24)

All **33 remaining canvas colour literals** now read from `TOK()`. Six colours
the stylesheet had never named got tokens (`--text-hi`, `--dim2`, `--grid`,
`--grid2`, `--hot`, `--bad`), and three drifted near-duplicates were absorbed
into the token they meant (`#0b0e14`→`--bg`, `#7c8698`→`--dim`,
`#e8ecf2`→`--text-hi`). The two surviving ad-hoc `getComputedStyle` readers went
the way of the four in A1. Values unchanged; `:root` is now the single source
for painted colour, which is what makes increment 1c a stylesheet edit.

### CORRECTION: the canvas-hash oracle proved less than I said it did

ADR-118 and A1 both reported "the five non-animated canvases byte-identical,
`morphPad` among them" and offered `morphPad` as the evidence that corner
colours were untouched. **`morphPad` is blank in the standalone page** — 38 400
transparent pixels — because it skips painting while its page is hidden. So were
`wavscope`, `stripC`, and three of the five `gviz` canvases. A blank canvas
matching a blank canvas proves nothing, and I presented it as the strongest
evidence in the set.

Measured this time before trusting it: of 17 canvases, **7 have content**
(`xy`, `phase`, `spec`, `carpetC`, two `gviz`, `vmapC`), **6 are blank**, and
four of the painted ones animate and cannot be compared across page loads at
all. That leaves exactly **two** genuinely load-bearing canvas comparisons
(`gviz` 542×144), not five.

What actually carries the no-op claim, in order of strength:

1. **`TOK()` resolves to the exact previous literals** — direct, not inferential.
2. **Computed styles** on representative selectors — real DOM, unchanged.
3. **The morph pad, forced to paint** by switching to its page, then sampled at
   its four corners: each corner's dominant-channel ordering matches its
   `MCOLORS` entry (`#275956` / `#5b4925` / `#413760` / `#5a323f` — the corner
   colours at wash alpha over the dark ground). This is the check `morphPad`'s
   hash was pretending to be.
4. `gviz` — two painted canvases, byte-identical.

Same lesson as ADR-091 A4, in a new costume: **ask whether the instrument can
see the thing before believing what it reports.** A hash that is stable because
nothing is drawn looks exactly like a hash that is stable because nothing
changed.


### ADR-118 A3 — increment 1c: the ground flips (2026-08-24)

The visible one. `:root` is now the spec's token set and gui2 is a **cream**
instrument: cream ground, white data wells, ink outlines, magenta as the
authored value. Because A1 and A2 spent two increments making every colour
token-driven, this landed as **one block plus twelve targeted edits** rather
than a hunt.

Legacy token NAMES were kept pointing at their new spec ROLES (`--pull` →
value, `--amber` → caution, `--text` → ink), so 56 `var()` sites and every
`TOK()` call inherited the new palette untouched. That was the whole bet of
increments 0–1b and it paid.

Twelve places needed a semantic decision rather than a value swap:

- **Data wells are WHITE, not the ground.** Five canvas fills were painting
  `--bg`; on a dark theme "page ground" and "well" were the same colour, so
  nothing distinguished them. §1 separates them, so they now paint `--well`.
- **The HOLD badge's text** was `--bg` (dark text on an amber chip). On cream
  that would have vanished; it is `--text` (ink).
- **M / S, the meter, and its clip state** went to their spec roles — alarm,
  celebrate, caution — and the meter lost its gradient, because §4 specifies
  flat fills.
- **The menu's shadow** was `0 8px 26px #000a`. The spec bans blur outright
  ("hard, no blur"), so it is `var(--shadow)` — a 1.5px offset.
- **MCOLORS' headless fallbacks** still held the OLD corner colours. They only
  fire where there is no computed style, so they would have been wrong exactly
  where nobody looks.

### A contrast audit caught a mapping error of mine

I mapped `--dim` → the spec's **hint** colour and left labels on it. §2 says
labels are **secondary**; only hints take hint. Measured on the rendered DOM:
labels were at **2.84:1**, below 3:1 for 10px text. Moved to `--t2`:
**9.06:1**. Canvas axis labels had the same fault (mapped to `disabled`
weight, 2.09:1) and are now 9.46:1.

Worth recording what the audit did *not* flag, so a later reader does not
"fix" it: `meter`/`marker`/`celebrate`/`corner A` all sit near 1.4:1 against
white, and that is correct — the spec never uses them as text, always as
**fills carrying an ink 1.5 border**. Hairlines at 1.33:1 are gridlines and are
meant to be faint. Hint text stays at 2.84:1 because §2 deliberately sets it
quieter than a label; that is the spec's call, and it is now the only text
below 3:1 rather than the default for every label on the page.

`./verify full` EXIT=0, parity 156/156 — the interface changed, the instrument
did not.


### ADR-118 A4 — increment 2: §4 widget states (2026-08-24)

The controls were essentially native — `input[type=range]` carried
`accent-color` and nothing else, so the browser drew every slider, toggle and
dropdown. §4 gives each a geometry and four states; this implements the ones
this interface actually has. Knob, fader, mod halo and census bar wait for the
widgets and features they describe, which is why they are absent rather than
approximated.

- **Slider** — track 6/r3/well-alt with an ink border, handle 13×13 r4 with the
  hard offset shadow, hover lifts to 2.5, active fills the handle with value
  and drops the shadow flat. A native range cannot paint its own fill, so §4's
  "fill from left" rides a `--pct` custom property set by the same function
  that already repaints readouts. It is computed from the SLIDER's min/max, not
  the parameter's, because on a log-scaled control the knob position is what
  the eye is measuring.
- **Toggle** — 32×17 r999, value fill and thumb right when on.
- **Enum / text** — pill geometry, ink border, secondary text, a drawn caret.
- **Chips** — radius 999 per §3.
- **Focus** — the spec's magenta ring, on every focusable thing.

### The §3 hit floor, and not faking it

§3 requires ≥28×28 for every control. Measured: **33 visible controls under
it** — toggles at 17, selects at 24, chips at 27, ranges at 18, colour swatches
at 14.

The naive fix inflates the widgets, which would break the spec in the other
direction: a toggle is 32×17 *by specification*, and the four `.armBox`
swatches are 14×14 because they are colour chips, not labels. So controls whose
size is incidental simply grow (`min-height:28px`), while the two whose size is
deliberate keep their painted face and extend a transparent hit area around it.

**Verified rather than assumed.** I claimed a pseudo-element takes clicks, then
tested it: hit-testing 5px and 10px outside the toggle's painted box lands on
the toggle, and a click there flips it. Final sweep over 188 controls:
**none fail the floor**, `.armBox` still measures 14×14 and the toggle 32×17.

`./verify full` EXIT=0, parity 156/156.


### ADR-118 A5 — increment 3: §6 visualizer semantics, and a regression A2 could not see (2026-08-24)

**The regression first, because it was already shipped.** A2 swept the canvas
for `#hex` literals and reported zero remaining. There were six more in
`rgba(...)` form, which that search structurally could not match — and four of
them were **trail-fade overlays** compositing toward `#0b0e14`. Painting a
translucent near-black rectangle each frame is how a trail decays on a dark
theme; on 1c's white wells it fades every trail toward BLACK instead of toward
the ground. The phase circle, the carpet, the SPECTRA strips and the voice map
all shipped that way in 1c.

The fix is not "use white": it is `TOKA(token, alpha)`, so a fade names the
**surface it fades into** rather than a colour that happens to match it today.
That is the same failure as `--cA` being byte-identical to `--pull` — a value
that is right by coincidence is indistinguishable from one that is right by
construction, until the coincidence ends.

Lesson for the remaining increments: **a colour search that only knows one
notation is not a colour search.** `rgba()`, `hsl()`, gradient stops and
`box-shadow` all carry colour past a `#`-shaped grep.

### §6 Kuramoto

The phase circle now says what the spec says it should:

- ring is **ink** 1.5 (was a grid tint)
- the voice web is a **hairline** — an in-well line, per §1
- the order vector is **physics violet**, 3px, with a ⌀7.5 head. It was magenta,
  which claims it is the user's authored value; it is the system's own state.
  This is the exact distinction §1 means by "one job each — never borrowed".
- **the R rim arc did not exist.** Now teal, round caps, from 12 o'clock CW at
  R·90°, so sync literally wraps the dial instead of being a printed number.

Also killed: a `0 0 6px` glow that survived every earlier pass. §3 bans blur
outright — "hard, no blur" — so it is the 1.5px offset.

Verified by driving the painter with a synthetic frame and reading the pixels
back: **334 magenta** (voices), **243 violet** (order vector), **146 teal**
(R arc), ink ring present, and the dominant colour is now the white well —
i.e. the fades composite toward the ground, which is the regression's own test.

`./verify full` EXIT=0, parity 156/156.


### ADR-118 A6 — increment 3b: the voice map, and caution stops being borrowed (2026-08-24)

**§1 was being violated in three places.** "SIGNAL (one job each — never
borrowed)" — and `--amber` (caution) was carrying the R channel in the scope and
the second cluster in both the phase circle and the carpet. The spec's own
argument about red ("appears at most twice on any page or it stops meaning
danger") applies to caution for identical reasons: a warning colour spent on
ordinary data is no longer a warning.

**A genuine gap in the spec, recorded rather than papered over.** §6 never
contemplates two-cluster topology or an L/R scope, so it names no colour for
"the second of two PEER series". `--ghost` is the closest unborrowed role it
does define ("dry/reference traces behind magenta wet") and is what these use
now — but ghost implies *secondary*, and the two clusters are peers, so this is
the least-wrong option rather than the right one. **A designer question, not an
implementation one.**

**§6 VOICE MAP.** The old drawing coloured *both* the target ring and the actual
dot by root-vs-other, so one colour carried two dimensions at once — you could
not tell "this is the target" from "this is the root" without counting. Now:

- **target** is an ink ring — structure, the pitch you asked for
- **actual** is a value-magenta dot with an ink hairline, so the **gap between
  ring and dot IS drift + glide + pull**, which is the whole reason the map has
  two marks
- **root** is a marker star on its own channel, ink-outlined because marker
  yellow is 1.4:1 against white unaided (§1 pairs every fill with an ink border)

Scope line weights moved to §6's range: 2.0 for the primary trace, 1.4 for the
one behind it.

Verified by driving the painter with a synthetic frame where actual deliberately
differs from target, then counting pixels: **182 ink** (target rings), **62
value** (actual dots), **19 marker** (root star) — all three roles present and
distinguishable.

**Not done, and deliberately:** the repo's `drawCarpet` is x = voice, y = phase.
§6's carpet is x = time, y = voice, hue = phase on a 5-step cycle. Those are
*different pictures*, not different paint, so forcing the 5-step hue onto this
one would encode phase twice (it is already the y axis) and destroy the cluster
distinction the colour currently carries. The real carpet lands with the page
compositions.

`./verify full` EXIT=0, parity 156/156.


### ADR-118 A7 — increment 3c: §6 spectrum (2026-08-24)

The spectrum was a smoothed magenta polyline. §6 asks for third-octave bars in
METER teal with magenta peak caps, an ink floor and a dashed 0 dBFS ceiling.

**The data was already most of the way there, which is worth recording.** The
engine emits a LOG axis — `f = 30 · (16000/30)^(b/(n−1))` — which is exactly the
spec's stated 30 Hz–16 kHz range. It is simply *finer* than a third octave: 256
points across 9.06 octaves is ~0.036 octaves per bin. So the bins are aggregated
into **28 third-octave bands**, taking the **max** within each band rather than
the mean, because a spectrum display that averages a peak away is hiding the one
thing it exists to show.

**Peak caps** rise instantly, hold 1.5 s, then fall — the asymmetry §4 already
specifies for meters, for the same reason (a peak indicator that falls as fast
as the signal cannot be read at frame rate).

**On "caps render red ABOVE the ceiling".** The engine clamps `v = (dB+80)/80`
to 1.0, so 0 dBFS *is* the top of the range and nothing can be drawn above it.
A cap that has REACHED the ceiling is therefore the clip condition, and that is
what turns alarm red. Faithful to the intent, honest about the data — the
alternative would be inventing headroom the engine does not report.

Verified by driving the painter with a synthetic spectrum containing one band
pinned at 0 dBFS: **5342 teal** (bars), **438 magenta** (peak caps), **15 alarm**
(the pinned band only), **660 ink** (floor + dashed ceiling). The alarm count
being small and non-zero is the point — it fires on the clipped band and nowhere
else.

`./verify full` EXIT=0, parity 156/156. §6 is now complete except the true phase
carpet, which is a different picture rather than different paint (A6).


## ADR-119 — vaporwave phosphor: the wells become tubes (2026-08-24)

**Status:** adopted and shipped, on the human's call ("I've decided I want to go
in a slightly different direction for the displays").

`HORDE Vaporwave Phosphor.dc.html` (filed at `docs/design-system/`) re-themes
the DATA WELLS only: "the chrome stays horde (cream, ink, stickers) — only the
data wells switch." White paper becomes a deep-violet phosphor tube (#180A26)
with 1px/3px scanlines at 0.22, a grid-violet for in-tube structure, and glowing
traces. **Semantics carry 1:1** — value/meter/physics/marker keep their meanings
and only re-tune for emission: value #FF5ECF, meter #59F6E8, physics #9D6CFF,
marker #FFB86B.

### Why this was cheap, and what that proves

The document predicted "a WELL-ONLY token swap: 5 screen tokens + a trace-
rendering mode, zero layout change" — and that is what it was, because
increments 0–3 made every canvas paint site token-driven. The screen set lives
beside the chrome set in `:root`; canvases read `--scr-*`, CSS controls keep the
chrome tokens, and no layout moved. The token bridge paid for itself in one
direction change.

### The rules this direction deliberately breaks, quarantined as specified

- **Glow.** §3 bans blur, and still does: every glow is an UNDERLAY at low alpha
  (8px 22% stroke under the scope beam, 30% discs under voice dots, a 12px 25%
  stroke under the R arc) — "the no-blur glow recipe, portable to any painter".
- **The sunset gradient.** The one gradient in the theme, quarantined to
  spectrum bars and mapped to LEVEL (violet floor → pink → amber sun), so it
  encodes something rather than decorating.

### Judgment calls the document did not spell out

- **gvizCtx became the tube in one place.** The envelope/scatter/bend canvases
  were TRANSPARENT over the cream panel; as data wells they are tubes now, and
  the shared clear is the single site that decides.
- **Scanlines on trail-fade canvases are scaled by the fade alpha** (0.22·α per
  frame), so the repeated overlay converges to ~0.22 instead of accumulating
  toward solid black. Full-clear canvases draw them at 0.22 directly; the morph
  pad bakes them into its cached field.
- **The pad cursor was about to become invisible.** It wore `--text-hi`, which
  1c re-pointed to ink — ink on a tube is black on near-black. It is the user's
  hand, so it is now screen-value pink.
- **Two-cluster and L/R "peer series"** (the §6 gap A6 recorded) render on
  screens as the etch recipe: dotted cyan for the reference/second series, per
  the document's own dry-ghost treatment. Still the least-wrong choice; the
  designer question stands.
- **Corner colours were NOT re-tuned** — the document does not touch them, so
  the open re-theming ruling (ADR-117) is not decided by accident. Corner
  washes on the morph pad now sit on tube and read as glow for free.

### Verified by pixels

Phase circle on a synthetic frame: tube ground dominant, 362 pink voice px (with
blooms), 433 violet vector px, 157 cyan arc px. Spectrum with one band pinned at
0 dBFS: all three sunset stops present (violet 1181 / pink 752 / amber 1365 —
level-mapped, not decorative), 427 cyan cap px, **15 alarm px on the pinned band
only**, and scanline rows measurably darker than their neighbours.

`./verify full` pending below; fast EXIT=0 at time of writing.


### ADR-114 A1 — session safety under the rename, proven rather than asserted (2026-08-24)

Human: "what will happen to existing projects that call hypersaw?" ADR-114
asserted the frozen id protects them; this verifies it per format, from source
and logs rather than intent.

**VST3 (what Ableton matches projects against).** clap-wrapper derives the
class ID as an RFC 4122 v5 UUID — SHA-1 over the DNS namespace plus
`clapdescr->id` (`wrapasvst3_entry.cpp:275`, `sha1.cpp:311`). The display name
is NOT an input to the hash. Reproduced independently:
`uuid5(NAMESPACE_DNS, "com.lifted-truck.hypersaw")` =
`F730E1CE-68C6-57DB-87D2-452E272BD28F` — byte-for-byte the CID Ableton's own
log recorded when restoring projects before the rename. The CID is a pure
function of the frozen id; the rename cannot move it, mathematically.

**AU.** Empirical: the log shows `Audio Unit v2: Created: HYPERSAW` at
2026-08-24T02:05 — an instance successfully created from the RENAMED binary,
so the component codes projects match on did not move either.

**CLAP.** Hosts match the id string itself, which is the thing that is frozen.

**What existing projects therefore experience:** they reload exactly as saved —
same engine, same state (the chunk format and even its `"plugin":"HYPERSAW"`
marker are untouched, ADR-115), same automation (parameter ids are append-only).
The only change is cosmetic: after a rescan the device label reads **horde**.
Track and clip names the user typed stay whatever they typed.

**The corollary this proof cuts both ways on:** since the CID is derived from
the id, changing `com.lifted-truck.hypersaw` by even one character would mint a
new CID and orphan every project ever saved. The comment at the descriptor
already says never to touch it; now the mechanism is on record too.


### ADR-119 A2 — bezels, the light-screen audition, and the instruments of coherence (2026-08-24)

Four interface notes from the human, and one sound claim measured first.

**"K range feels reduced" — measured: unchanged.** A/B probe through the real
plugin against a 2026-08-22 pre-change build: K→clarity identical to four
decimal places at every sweep point (`docs/research/2026-08-24-k-coherence-ab.md`).
The inertia taper was cleared analytically too — it moved every stored knob's
effective inertia DOWN, and inertia here is a lock-slowing mass-spring, so it
could only have helped. What changed in the same window was the coherence
INSTRUMENTS: invisible netting (fixed), a diminished readout (restored below),
blooms smearing alignment. The claim was treated as a sound bug until the
measurement said otherwise; the offer to re-run against a named patch stands.

**Bezels.** Every canvas now wears the card language — ink 1.5, radius 8 — which
is the congruity element the tubes were missing. No shadow: §3 reserves shadows
for interactive elements, never wells.

**The readout returns, fixed-format.** `R 0.534 · A — · B —` under both phase
circles (A/B fill in under two-cluster topology), and `gravity —` that NEVER
collapses to an empty line — every segment always present, absent values print
an em dash, tabular-nums so digits do not wiggle. The per-frame painter no
longer owns the readout; drawViz does, because it has topo/RA/RB.

**The waveform's hue rides R.** The wet beam mixes from value-pink toward
marker-amber as the smoothed order parameter rises — coherence becomes visible
in the waveform itself, exactly as asked.

**The light screen is an AUDITION.** `body.scr-light` re-tunes the six screen
tokens to a pale-LCD set (tube #F2EEFF, deeper traces, scanlines at 0.07), and
an SCR chip in the tab bar flips it live — clearing the token cache and
re-baking the pad field; trail canvases converge on their own. Session-only by
design. The values are mine to audition, not the designer's.

**A real bug the toggle exposed:** TOK read computed style from
`documentElement`, and body-level overrides never appear there — the toggle
flipped the class and every token kept its dark value. TOK reads `document.body`
now. The screen-theme mechanism would have silently half-worked forever
otherwise.

## ADR-123 — osc on/off morphs as a level ramp; the A3 corner-write gets its field guard (2026-08-26)

**Trigger.** Human: *"on blend, it should jump to on and gradually bring the
volume of the osc up to max instead of picking an on/off value from one patch
and a volume value from another."* Filed as B48; implemented same day.

**The two defects.** (1) `enable` (150/1150) is stepped, so pick-mode drew it
from one corner while `vol` came from another — the partway state was a
chimera neither corner contains. (2) ADR-100's OFF hard-kills voices by
design; correct for a human flipping a switch, a click generator when the
field flips it at a blend boundary.

**The design.** `morphStep` special-cases 150-ids: the bilinear corner weight
of the "on" corners becomes a per-osc **on-weight** (`oscOnW[]`), applied as
one more factor in `oscGainTarget()` — through the existing ~8 ms smoother and
its skip-if-1.0 guard, so the settled path is byte-identical to today. Plain
`w[]`, not the Gumbel draw: the ramp is deterministic in pad position, both
modes. The stepped flip (with its kill/re-strike) still happens, but only at
the 1e-3 weight floor, where the osc is already ~−60 dB — inaudible by
construction. Exempt enables and morph-off both pin the weight to 1.0.

**The latent bug this exposed, and its control.** ADR-100 A3 writes a switch
edit into all four corners so the field cannot revert it — but the handler ran
for the FIELD's own flips too, so the first boundary crossing overwrote every
corner's enable and the stored on/off boundary silently ceased to exist.
Guarded with `!morphFromField`. Proven by A/B: with the guard removed, a sweep
across the boundary and back reads corner A's enable as 1 (destroyed); with
it, 0 (survived).

**Evidence.** Osc2-solo sweep across the boundary: silence at the off corner,
−41 dB at first blend step, monotone rise to −14 dB at the on corner — no
step, no click window. Parity 156/156 (worst 4.262e-09) — the goldens never
engage the morph, and the settled-gain path is unchanged. Tests B48-1/B48-2.

**Rejected:** writing the ramp into `vol` (17/1017) — it would fight vol's own
morph target and corrupt corner authoring through the armed-edit router; a
derived multiplier composes instead.

## ADR-124 — atomic morph groups generalised; the FX slot becomes one of them (2026-08-26)

**Trigger.** Human: *"The same rule should probably apply to FX modules, but
we're already changing how they work so maybe we should start moving on that
soon."* Filed as B49.

**The measured defect.** Corner A = slot1 Drive @ amount 0.90, corner B =
slot1 Gain @ amount 0.10; sweep morph X. **3 of 9 sampled positions were
states neither corner holds** — the middle third read `type=Drive,
amount=0.10`. Drive at 0.10 is near-passthrough, so the drive corner's whole
character silently evaporated mid-blend while the type still claimed Drive.
Mechanism: type (57/59/61/63) and amount (58/60/62/64) are both in the field
and, in pick mode, are drawn **independently** each grid tick. `amount` is
dimensionally different per type (Drive pre-gain, Gain 0.5-is-unity, Comp
strength, Comb wet), so pairing it with another corner's type is not merely
arbitrary — it is meaningless. Mode 1 fails differently and no better: there
`amount` interpolates across incommensurable units.

**The fix reuses a mechanism rather than adding one.** ADR-109 A1 already
made root + twelve scale degrees draw one corner between them, for exactly
this reason ("C major and F# minor interleaved is not a scale"). That was one
hardcoded index range; it is now an explicit lead map — identity except where
a group says otherwise — so a second group is *data*. Members need not be
contiguous, which matters because a slot's type/amount/tone sit in three
different id blocks. Type + amount + tone now draw one corner per slot.

**What this does NOT do.** It does not ramp. A slot still appears and
disappears abruptly across an on/off boundary; B49's ramp (scale `mix` by the
on-weight, exactly ADR-123's shape with `fx_rack.h:272`'s guaranteed bypass as
the zero) and the type-swap ruling (dip-through-zero vs two instances) are
still open. Atomicity fixes *what* you hear being a real authored state;
the ramp fixes *when* it becomes audible.

**Evidence.** Chimera probe 3/9 → **0/9**. Parity **156/156** within ε=1e-6
(worst 4.262e-09) — groups only bind when the morph is on, and at a pure
corner every member drew that corner anyway. Scale-group non-regression shown
by A/B: the pre-change and post-change builds produce byte-identical probe
output. (That probe never successfully authored the scale corners — both read
the default major scale — so it proves non-regression, **not** atomicity;
recorded as such rather than counted as a passing check it is not.)

## ADR-125 — topology morphs by ARGMAX; BLEND stays an option (2026-08-26)

**Status: ACCEPTED — human ruling, made by ear on `docs/design/fx-morph-law-lab.html`.**

**The ruling, verbatim.** *"As much as part of me wants to go for the blend
anyway, it does create an untenable screechy feedback; maybe we could leave it
as an option but default to argmax. Argmax does in some ways better capture the
spirit of the quantum morph anyway."*

So: **route/topology coefficients draw ONE corner (ARGMAX/QUANTUM). BLEND is
retained as a selectable law, not as the default.** Module *parameters* are
unaffected — they continue to follow `morphMode` as they do today.

**Why this is the right shape and not merely the safe one.** The second half of
the ruling is the load-bearing half. ADR-104's whole premise is a *quantum*
morph: the field draws a corner per parameter rather than averaging corners, and
ADR-115 chose corner A over the centre as the opening position for the same
reason — *"the middle is the messiest place on the grid."* A blended topology is
an averaged structure, which is the one thing the quantum morph was built not to
do. BLEND was the odd law out on this substrate all along; the screech is what
made that audible.

**Implementation is an existing mechanism, not a new one.** ARGMAX over topology
means every route coefficient draws the same corner — which is exactly what
ADR-124's `morphLead` map does. When routing joins the morph field, all route
ids point at one lead index and the picker does the rest. No new code path.

**The option, when it is built.** A global `morphTopoLaw` (ARGMAX default,
BLEND opt-in). **Deliberately NOT added yet:** routing coefficients are not in
`morphIds` today, so the param would be a control that changes nothing — the
exact dead-control failure `gui_reach` exists to catch (L0023). It lands with
the routing-morph work, in the same change.

**One open thread this ruling does not close, recorded so it is not lost.** The
screech may be partly the *cycle delay* rather than the blend: BLEND's midpoint
is the only state that closes `drive→comb→drive`, and the lab's cycle carries a
one-render-quantum delay (2.9 ms at 44.1 kHz) because Web Audio requires one.
A 2.9 ms loop is intrinsically metallic. If so, that is direct evidence for
B50's feedback fork — **block-rate feedback is unacceptable for audio, per-sample
is required** — and it would be evidence gathered by ear rather than argued.
What would settle it: the same A/B with a one-sample loop delay. Not run;
the ruling above does not depend on it either way.

**Consequence for B50's migration.** Ported route edges become one atomic group
per patch, so a migrated patch's chain order flips discretely between corners
rather than smearing into a parallel blend. That was the open question B50 said
had to be answered *before* the migration was written. It is answered.

## ADR-126 — mono held-stack overflow drops the OLDEST key (2026-08-26)

**Status: ACCEPTED — human ratification.** *"I want to make sure to ratify the
decision to replace drop-newest with drop-oldest."*

**What was there.** `if (heldCount < 16) heldStack[heldCount++] = …` — on
overflow the *newest* key was silently discarded. Not a considered choice: a
bound written to be safe rather than musical.

**Measured cost of the old behaviour** (scratch probe, real plugin, mono+legato).
It does not hang or silence anything — the sounding note is tracked separately
in `core.voiceAt(monoSlot).midi`, and the release path only retargets when the
released key *is* the sounding one — and overflowing by exactly one
self-corrects. Overflowing by **two** forgets the intermediate key entirely:
hold 40…55, press 70, press 71, release 71 → **55 sounds, not 70, while 70 is
still physically held.** Drop-oldest keeps the fallback chain anchored to what
the player most recently played, which is what last-note priority means.

**The accepted cost**, stated in our own 2026-08-11 answer: the evicted key's
later note-off matches nothing. That key was not sounding — in mono only the
top of the stack sounds — so all that is lost is its availability as a fallback
after the newer keys release. *"A silent key press is a much larger harm than a
stale fallback."*

**This also settles the open cross-repo question.** FOUNDATIONS asked which of
our two contradicting answers governs (Aug-11: drop-oldest, *"and we will
change ours to match"*; Aug-25: keep drop-newest *"we would rather keep
parity"*). **The Aug-11 round governs.** The Aug-25 answer's parity argument
only worked because the Aug-11 promise had gone unkept for a fortnight — it was
defending an accident, not a design. They shipped drop-oldest and quote our own
Aug-11 reasoning in their header; we are now consistent with the record we
made.

**Reachability, so the scope is honest:** above 16 simultaneously held keys in
mono. Hands cannot; a sustained MIDI clip, an arp feeding mono, or stacked
chords can.

**Oracles.** `parity_check` 156/156 within ε=1e-6 (worst 4.262e-09) —
unchanged, because no golden holds 16 keys in mono. `notefuzz_check` GREEN,
0 hangs. Test row B51-1.

## ADR-125 Amendment 1 — the lab that produced the ruling had two feedback defects (2026-08-26)

**Raised by the human's observation**, not by a review: *"The argmax boundary
doesn't have a click as much as it seems almost like a struck plectrum. I
imagine this has to do with the interaction between the comb and the drive."*
Chasing that mechanism found two defects in the lab ADR-125 was ruled on.

**1. The plectrum is real, and the mechanism is the one the human named.**
Measured at the ARGMAX flip, transient peak over steady mean: **1.6 at comb
feedback 0, 1.7 at 0.45, 4.3 at 0.9.** It scales with feedback because a comb
with feedback IS Karplus-Strong — delay plus feedback plus damping is the
plucked-string algorithm — and the flip presents it with a discontinuity. An
impulse into Karplus-Strong is *by definition* a pluck, so "struck plectrum" is
not a metaphor; it is the algorithm doing its defining thing. The ~1.6 floor at
zero feedback is the topology step itself; the comb turns that step into a
pluck. **B50-1's falsifier asked whether the flip is a CLICK. It is not — so
the falsifier did not fire and ADR-125 stands on this point.**

**2. TWO in-loop filters were resonant, and one made the lab diverge.**
`BiquadFilterNode` defaults Q to 1.0, and the comb's damping lowpass was left at
that default — a lowpass at Q=1 peaks *above* unity near its corner, so the
in-loop damper was ADDING gain at ~4.2 kHz instead of removing it. At feedback
0.9 that put the comb's own loop gain over 1 and the output reached **4.3e4
under ARGMAX, which closes no cycle at all.** The DC blocker measured 1.0839 at
its corner for the same reason. Both are now Butterworth (Q = 1/√2, maximally
flat — can only remove energy). ADR-031(b) warns about precisely this class
(*"in-loop filters compound"*); a resonant damper is its sharpest form. The lab
also lacked ADR-031(c)'s watchdog, which a lab a human makes rulings on must
have — a hard-knee limiter now guards the output.

**The honest consequence for ADR-125, stated rather than buried.** A runaway at
~4.2 kHz is *screechy*. The human's first reason for rejecting BLEND was
*"untenable screechy feedback"* — and some of that screech may have been this
defect rather than BLEND itself, since a resonant in-loop damper degrades every
law and BLEND's cycle would amplify it worst. **ADR-125 is NOT withdrawn:** its
second reason — that a blended topology is an averaged structure, which is the
one thing a quantum morph exists not to do — is independent of any lab defect
and is the reason recorded as load-bearing. But the first reason is now
uncertain, and BLEND deserves a re-listen on the fixed lab before anyone treats
its rejection as settled on sonic grounds.

## ADR-127 — switching morph ON adopts the live patch instead of overwriting it (2026-08-26)

**Status: ACCEPTED.** Human report: *"switching morph on when you've edited the
patch can be destructive; it replaces the sound with default inits."*

**Reproduced, 5 of 5 edited parameters destroyed.** Edit voices→16, detune→0.75,
K→0.80, release→2.50, width→1.30; switch morph on; let the 5.8 ms grid tick.
Every one snaps back to its default.

**Mechanism.** `morphInit()` seeds all four corners with `defaultFor(...)` and
reasons that this is *"silence-safe: every corner agrees"*. That is true of a
**fresh instance**, where live == default. But `morphInit()` is guarded by
`if (!morphIds.empty()) return;` and is called from readParam and state paths —
so it runs at startup, long before any editing. The corners then hold stale
defaults, and the first grid tick after morph-on writes them over the player's
sound. The comment was correct about the case it was written for and silently
wrong about every other case.

**The fix.** When morph is switched on and the corners have never been
**authored**, adopt the live value into all four. All four, not one, so
morphInit's silence-safe property is preserved exactly — every corner still
agrees, so the field stays inert until something is captured. `morphCornersAuthored`
is set by every path that gives a corner meaning: capture, an armed edit, an
exempt write, a corner-preset apply, and a state chunk that carried corners.
That guard matters as much as the fix: a loaded preset's corners must never be
clobbered either, so the destructive direction is closed **both** ways.

**Why not the pop-up.** The human offered two designs — a warning recommending
"capture to the yellow corner", or normalising a corner to what is set. A
warning that says *"you are about to lose your sound"* is worse than not losing
it; the dialog exists only because the underlying behaviour is wrong. And the
adopt-live lean is already recorded in this codebase, at `morphToggleExempt`:
*"the corners honestly record what was playing."* This applies the same rule one
level up.

**Parity.** 156/156 within ε=1e-6, unchanged — `morphOn` ships off and no golden
enables it, so the new branch is unreachable from the goldens by construction.
Test rows B54-1 / B54-2.

## ADR-128 — audio feedback carries a ONE-SAMPLE delay, not a block-rate one (2026-08-26)

**Status: ACCEPTED — human ruling.** *"I'll take your per-sample rec."* Closes
B50's first fork.

**What was decided.** When the FX routing matrix gains feedback, a backwards
edge carries **one sample** of delay and the crosspoint sum runs inside the
sample loop. It does **not** carry a block-rate unit delay.

**Why block-rate was rejected**, in one number: at 128 samples / 44.1 kHz a
block-rate loop is **2.9 ms**, which is a flanger rather than a routing
primitive — and it **changes with the host's buffer size**, so the same patch
would sound different at 64 and 512 samples. That is a direct violation of our
own determinism rule (SPEC §5.7: same seed + note order → identical output);
buffer-size-dependent output is the same class of defect as a wall-clock read
in the core. One sample is 22.7 µs and buffer-independent.

**FOUNDATIONS' OQ-23 is not overruled — it does not apply.** Their ruling
(*"a cycle is legal, every feedback edge carries a unit delay at block rate"*,
`notice-oq23-ruled.md`) was made for the **modulation** graph, where control
rate is the natural rate and 2.9 ms is inaudible. Inheriting it for audio would
be a category error. **This divergence should be filed to them**, not because
they require it, but because their register is watching for exactly this: a
second consumer discovering that a doorframe's rate assumption does not
transfer across domains.

**Cost, stated so it is not discovered later.** Per-sample crosspoint summing is
O(live edges) per sample instead of per block. At today's six modules with a
handful of live edges that is negligible; it stops being negligible if B45's
roster lands and every crosspoint is opened, which is why the implementation
must sum over the **live** edge set, recomputed per block, rather than the full
N² table.

**Not implemented yet.** Routing coefficients are not in the audio path's
feedback form; this ruling constrains B50's build rather than describing shipped
behaviour.

## ADR-129 — the time engines reach the instrument: Echo and Room as rack slots (2026-08-27)

**Status: ACCEPTED.** Human: *"I also want to hear the reverb and the delays
ASAP."*

**They were already built.** `src/time_core.h` — the Track E2 port of
`swarmtime.html` — has been oracle-covered since it landed (`time_check`: L0-1
parity plus the L0-19/20/21 stability laws, all running in `./verify`), and was
reachable only from SWARM-FX, the standalone effect. It had **zero references**
in the plugin. This wires it in; it does not write a reverb.

**Two slot types, one core.** `Echo` (7) is the tap-swarm delay — N read taps on
one buffer. `Room` (8) is the FDN room swarm — N lines with a negated
Householder. They differ only by `mode`, which is why they are one core and two
enum values rather than two implementations.

**RT-safety follows the NotchCore precedent exactly.** `TimeCore`'s constructor
allocates (~3 s echo buffer + 12 room lines ≈ 1.75 MB each, ~7 MB for the rack),
so instances are built in `setSampleRate` on the main thread and never touched
by `processSlot`. Mode changes **are** audio-thread safe: `setParam("mode")`
calls `rebuild(false)`, which writes into pre-existing arrays and allocates
nothing — verified by reading it, not assumed. Allocation is unconditional
rather than lazy because `setType()` runs on the audio thread from param
events, so lazy construction there would be the exact allocation this rule
exists to prevent. An unused slot therefore holds memory it is not using; that
is the deliberate trade.

**`amount` → REGEN, not mix.** The slot's own `mix` already owns wet/dry under
the rack's universal contract, so mapping `amount` to mix would be a second
dry/wet fighting the first. Regen is the control that changes what the effect
*is* — slapback at 0.1, cavern at 0.9. The core's own `mix` is pinned fully wet
so the slot mix does all blending, which is what keeps `mix = 0` a bit-exact
passthrough.

**Measured.** Tail RMS with the note long gone (only an effect can be sounding):

| | regen 0.3 | 0.6 | 0.9 | 0.97 |
|---|---|---|---|---|
| Echo | −69.2 dB | −59.9 | −52.3 | — |
| Room | −102.7 dB | −95.5 | −62.7 | **−34.8** |

Both live, both monotonic in regen. **Usability note worth having early: Room's
useful range is the top third of the knob** — below regen ≈ 0.6 its tail is
near the noise floor, which is correct for a small room but makes most of the
knob's travel do very little. A per-type `amount` curve is the obvious fix and
belongs with the per-slot pages, not here.

**Contract preserved:** `Echo` selected at `mix = 0` differs from no Echo in
**0 of 88,064 samples**. Parity 156/156 unchanged — no golden selects a slot
type above 0.

**Append-only:** labels and the enum share numeric order, and the type params
widen 0..6 → 0..8. An existing patch stores the integer, so appending is the
only safe direction.

## ADR-130 — the bundles are named horde; a gate now catches option-list drift (2026-08-27)

**Status: ACCEPTED.** Human: *"the au is still called hypersaw, and the vst
isn't showing these yet"* — two unrelated defects reported together.

### 1. The VST was fine; the GUI could not reach the new values

ADR-129 widened the FX type params 0..6 → 0..8, but gui2's four type dropdowns
are **hand-written `<option>` lists**, outside the generator's markers, and they
still offered seven. Echo and Room shipped as slot types **no player could
select**.

`gui_reach` stayed green throughout, and correctly: param 57 *is* reachable. It
simply cannot reach all of its own **values** — a gap that gate was never
shaped to see.

**A new check in `presentation_check`** closes it: for any stepped param named
by a hand-written select, the option values must be exactly the declared range.
Generated controls are exempt, since the generator derives them and cannot
drift. **Calibrated against the real bug** — with Echo/Room removed from one
select it reports `param 57: select offers [0..6], declared range 0..8 —
MISSING [7, 8]` and fails; restored, it passes.

### 2. The AU name, and the lever that does not exist

The AU read *"Lifted Truck: HYPERSAW"* because the AU output name defaults to
the target's output name. **`AUV2_OUTPUT_NAME` is not a usable narrower lever**:
in the target-based configuration clap-wrapper overwrites it from the CLAP
target's `LIBRARY_OUTPUT_NAME` (`wrap_auv2.cmake:100-101`), so setting it is
inert — it was set, observed to do nothing, and removed rather than left as a
control that lies.

So `OUTPUT_NAME` → `horde`, which renames every format's bundle.

**Identity is untouched, which is what makes this safe:** the CLAP id
(`com.lifted-truck.hypersaw`) is frozen, the AU triple (`aumu`/`Hsaw`/`LfTk`) is
frozen and verified unchanged in the built plist, and the VST3 UID derives from
the CLAP id rather than the filename. A host re-finds the plugin by those, not
by its name.

**THE ONE REAL COST, and it needs a human action:** renaming produces *new*
bundles. `horde.vst3` and `horde.component` do not replace `HYPERSAW.vst3` and
`HYPERSAW.component` — both pairs are now installed, and both will appear in the
browser until the old pair is deleted by hand. Deleting installed plugins is not
something this session will do unasked.

`./install` was retargeted with them (12 references), so it maintains the new
pair from here.

**Verified:** AU plist name `Lifted Truck: horde`, subtype `Hsaw`, manufacturer
`LfTk`; both new bundles carry the Echo/Room labels; seal verified; `auval`
SUCCEEDED; parity 156/156; `./verify fast` exit 0.

## ADR-131 — the FX slots get real parameters (2026-08-27)

**Status: ACCEPTED (increment 1 of the FX page rebuild).** Human: *"Let's rebuild
the FX page so the FX can have actual parameters and visuals."*

**The gap.** A rack slot exposed four controls — type, amount, tone, mix —
standing in front of cores with far more. `TimeCore` alone has nineteen
parameters, of which the rack could reach exactly one (`amount` → regen). The
`docs/proposals/fx-slot-contract.md` note *"until the rack grows per-slot param
pages"* is what this begins.

**Twenty-eight parameters, at `200 + slot*8`.** Seven per slot — size, spread,
taps/lines, damping, noise, stereo, spacing — with one spare id per block so a
slot's page can grow without renumbering. Global, because the rack is post-mix
and there is exactly one of it (ADR-129's reasoning about note identity applies
to the whole rack, not just to scope).

**Dispatched by arithmetic, not by twenty-eight cases:** `slot = (id-200)/8`,
`key = (id-200)%8`. A block layout that the shell computes cannot fall out of
step with a table the shell also computes.

**Type-conditional by an existing mechanism.** Every row carries
`shown_when: fxNtype=7|8` *and the matching `depends`*, which is ADR-108's
pattern exactly: the GUI hides the control and the morph hierarchy holds it, from
one declaration, so the field can never write a parameter the interface has
hidden. Both columns are set deliberately — they are separate columns and
keeping them in step is a convention, not an enforcement.

**The change guard is load-bearing, not an optimisation.** `size`, `spread`,
`nb` and `dist` call `TimeCore::rebuild()` inside `setParam`, so writing them
every block would rebuild the delay swarm every block. The rack mirrors what the
shell asked for against what the core has been told and writes only differences.
The plain scalars go through the same path so there is one rule rather than two.

**Measured.** With slot 1 as Echo, each of the seven changes the render
(133,676–143,490 of 158,208 samples differ from baseline). With slot 1 **Off**,
the worst difference across all seven is **0 samples** — they are inert unless
the slot is a time engine. Readback matches TimeCore's own defaults.
Parity **156/156**.

**Not in this increment:** the visuals, and per-slot pages for the other six
types. `amount` still stands alone for Drive/Filter/Gain/Comp/Comb/Notch. The
FX page is now a page with real controls on it; it is not yet the designed page
the human asked for.

## ADR-132 — ADR-100 A3's blanket corner write is removed (2026-08-27)

**Status: ACCEPTED.** Human: *"if I edit the on/off of an oscillator in one
corner, it seems to copy that setting to every other corner."*

**Reproduced.** Author corner A osc2 ON and B/C/D OFF; then, **unarmed**, toggle
the power at corner B. Corners C and D — authored OFF and never touched — both
read ON afterwards.

**Cause.** ADR-100 A3 wrote every enable edit into all four corners. Its reason
was real when written: *"otherwise the next grid tick reads the corner's stored
enable and reverts it, and a power switch that snaps back reads as broken."*

**ADR-109 made that obsolete, and nobody removed the workaround.**
`morphRouteEdit` runs *before* this block and stores the edit in every path —
armed, into the armed corner; unarmed pick-mode, into the corner that **won**
the parameter (`morphCorner[k][idx] = v`). The grid tick therefore reads back
what was just written and cannot revert. The net was catching nothing.

**What it cost instead: the feature ADR-100 exists for.** That ADR's own header
promises *"the morph grid can hold 'off in this corner, on in that one' per
corner per oscillator"* — and its amendment quietly made that impossible for any
edit not made with a corner armed.

**Verified both directions.** After removal: C and D keep their authored OFF and
the edit lands only on the corner being stood on. And the symptom A3 guarded
against does **not** return — an unarmed toggle at pad positions 0.0, 0.5 and
1.0 holds through 3 s (~500 grid ticks) at each. Parity 156/156.

**The general shape, worth naming:** a workaround outlives the defect it was
written for, and the thing that removes the defect is a *different* ADR that
never knew the workaround existed. Neither ADR is wrong on its own terms. Only
reading them together shows the redundancy — which is an argument for checking a
workaround's stated premise when the surrounding mechanism changes, not for
writing fewer workarounds.

## ADR-133 — Round × Pitch is bipolar; and the morph lab joins the port-gap tracker (2026-08-27)

**Status: ACCEPTED.** Human: *"I think round x pitch should be bipolar, with the
other direction skewing the roundness to the low end voices instead of the high
end."*

### The change is a range, not a formula

`swarm_core.h` already computes
`rnd[i] = clamp(round · (1 + roundHi · (2·up − 1)))`, and `2·up − 1` runs
−1..+1 across the spread. A negative `roundHi` therefore skews roundness toward
the **low** voices with **no change to the maths at all** — the declared lower
bound of `0` was the only thing preventing it. Range becomes −1..1, default
unchanged at 0.

**Parity-safe as a superset**, by ADR-056's pattern (which widened the onset
lock to bipolar for the same reason): the default is 0, `1 + 0·x == 1`, and no
golden sets it. **156/156 unchanged.**

**Measured from the core's own `rnd[]`** rather than inferred from audio, since
the claim is about *which voices* get rounded:

| voice | roundHi −1 | 0 | +1 |
|---|---|---|---|
| 0 (low) | **1.000** | 0.500 | **0.000** |
| 3 | 0.533 | 0.500 | 0.467 |
| 7 (high) | **0.000** | 0.500 | **1.000** |

Tilt −1.000 at −1, +1.000 at +1, and 0.0000 spread at zero — exactly mirrored,
flat in the middle. The GUI knob picked up bipolar paint automatically, because
`paintFill`'s test keys on `lo < 0 && hi > 0`.

### The gate hole this work exposed

`morph_core.h`'s header names its reference plainly — *"ported from
docs/design/quantum-morph-lab.html. The lab is the reference"* — but **that pair
was never registered in `port_gap`**, so nothing ever compared them.

Registering it immediately reports four unmatched controls, and the first is a
feature the human asked to "reintroduce":

- **`timing`** — the lab's *Flip timing: Immediate / Next note*. Never ported.
  Not a regression: a port gap with no gate looking at it.
- **`glyphs`**, **`tint`**, **`editRoute`** — the campaign-3 audition items,
  which are open human rulings. The gate rediscovered them independently, which
  is corroboration rather than news.

**The lesson is about coverage, not care.** Every other reference/port pair was
registered; this one was not, and its absence was silent. A tracker that lists
what it checks does not tell you what it *doesn't* — so the honest question for
any such registry is "what is missing from this list", and nothing was asking it.

**`port_gap` is advisory, not a `./verify` gate.** That is a separate decision
and is left alone here; making it blocking would fail the build today on four
items that are legitimately open.

## ADR-134 — the mod matrix reaches the audio path: ENV 1 → pitch (2026-08-28)

**Status: ACCEPTED (B69 increment 2).** The matrix's first live route, and —
deliberately the same thing — **B64's pitch envelope in functional form.**

**One knob, one route.** Param 161 `Env > Pitch` (±48 st, default 0) is route
0's depth. The route exists once the knob moves; `evaluate()` of an empty table
is zero entries; the smoothed offset settles to exactly 0 — so at default the
feature is inert **by construction**, and measured: a depth-0 render is
bit-identical to a build that never touched it (0 of 52,736 samples differ).

**The offset joins ADR-027's tune sum as a fifth term.** It is applied beside
the stored parameters, never written back into any of them — readback, state,
and host automation all still see the base value. Corrupting the base is the
classic matrix mistake, and it is why destinations are added one at a time
rather than generically.

**ENV 1's global projection is the loudest voice's envelope** across enabled
oscillators — stated plainly so nobody mistakes it for per-note fan-out. The
route already carries the scope field (B34 vocabulary) for the per-note
increment.

**Grid + slew.** Evaluation on the 256-sample gravity grid (ADR-086: buffer-
subdivision-independent), with an ~8 ms one-pole on the applied offset —
env × 48 st moves fast enough to zipper otherwise. OQ-30's rule is honoured at
the ruled place: the route may ask for anything; the destination clamps to its
declared ±48.

**Measured** (single voice, no detune, zero-crossing pitch): depth +12 → attack
at 410 Hz vs 220 baseline (1.86×; the residue from 2.0 is the slew still rising
inside the window), decay converging back to base. Depth −12 mirrors (0.545×).
Parity 156/156.

**What this is NOT yet:** per-note (the global projection is honest but
shared), a dedicated envelope (ENV 2 with its own times is B64's completion —
this knob then becomes ENV 2's route without renaming), or the right-click
route surface (B69's interface ruling, next increment).

## ADR-135 — ENV 2: the dedicated pitch envelope; B64 completed (2026-08-28)

**Status: ACCEPTED (B69 increment 3).** ENV 2 is a shell-side ADSR advanced at
the mod grid — a mod *source* with its own times (162–165), not a copy of the
core's amp envelope. Route 0 (`Env > Pitch`) now draws from it; ENV 1 (the amp
projection, slot 0) stays auto-included for future routes.

**Sustain defaults 0, deliberately:** a pitch envelope that returns to base
pitch while the note holds is the musical default — and it is the measurable
difference between ENV 2 and ENV 1. Measured exactly that way: with the amp
envelope sustaining at 1 (note still sounding), depth +12 reads 390 Hz in the
attack window and **exactly 220.0 Hz in the held-sustain window** — the pitch
came back while the note did not. Under ENV 1 it would have stayed raised.
Depth 0 remains bit-identical to an untouched build (0 of 52,736 samples).

**Gate semantics, stated plainly:** ENV 2 gates on "any voice gated across
enabled oscillators" — attack restarts on the first gate after silence, release
begins when all keys are up. A global paraphrase; per-note ENV 2 is the fan-out
increment. Stage machine: one-pole approaches (attack → 1, decay → sustain,
release → 0), time constants in seconds per ADR-009.

**Why the right-click menu increment was reordered behind this:** a menu item
that creates routes nothing can apply is a dead control (L0023's class). Generic
destination application is the gate for that surface; ENV 2 needed nothing but
a source slot.

## ADR-136 — generic mod destinations, the right-click, and the MOD page (2026-08-28)

**Status: ACCEPTED (B69 increment 4).** The matrix opens to arbitrary
continuous parameters, and the human's interface ruling ships: right-click any
knob → *Send to mod matrix* (ENV 1 or ENV 2); routes live on the MOD page —
the tab goes live for the first time — with depth sliders and removal.

**The base/offset contract is the heart of it.** For every modulated
destination the shell owns the BASE — the value the player, host or morph
authored — and writes base+offset through the normal apply path each mod tick
under a re-entrancy guard (the morphFromField pattern). Every *other* write
updates base, so dragging a knob under modulation drags the base and the
offset rides on. **Readback reports base**, so state, automation and the GUI
never see the modulation. Measured: mid-modulation the applied value read
0.7911 while host readback held exactly 0.5500.

**Rules, each with its reason:** stepped destinations are refused at route-add
(a zippered enum is not modulation) and the menu mirrors the rule by absence;
the matrix's own controls (161–165) never appear in the menu — modulating your
own depth is B70's territory and arrives with its cycle rule, not by accident;
depth is ±1 of the destination's range, clamped to the param's own bounds
(OQ-30 at application); a destination whose routes are all removed releases
back to base. **kModDestPitch moved to high-bit synthetic space** — it was 1,
which is param "n": a collision found before it fired.

**Test hooks over reimplementation:** `hypersaw_test_mod_add/remove/applied`
call the same shell functions the GUI bridge calls, so the oracle exercises
the shipped path. Full cycle measured: add → applies at the attack → base
survives → stepped refused → removal releases. Depth 0 / no-route remains
byte-identical; parity 156/156.

**Known gap, stated:** routes added by right-click are NOT yet persisted in
the state chunk — the Env>Pitch route survives via param 161, generic routes
do not survive a session reload. Persistence is the next increment and the gap
is recorded in B69 rather than discovered by a user losing work.

## ADR-137 — eight macros, the XY pads become macro controllers, and the offset is drawn, never moved (2026-08-28)

**Status: ACCEPTED (B69 increment 5).** Three rulings from the human land
together because they are one system: (1) modulation on a control is shown as
an OFFSET around a stationary knob, per the UI Spec §4 the human provided
2026-08-23; (2) eight macro knobs on MAIN; (3) the XY grids stop writing
detune/pull-K and become macro controllers with variable assignments.

**The offset law.** The knob/handle NEVER moves under modulation — it holds
the BASE (which is also exactly what ADR-136 makes readback report, so the
picture and the contract are the same fact). On a knob the reach is the
`.kmod` arc plus the `.kmnow` live tick — the seam ADR-121 built and nothing
ever called; this increment is its first caller. On a slider it is a band
under the track plus a tick (Spec §4's slider mod band). Reach is computed
from the route table the GUI already holds; only base + applied cross the
bridge (`hzModLive`), because those are the two numbers the GUI cannot know.
Log-scaled controls draw the band through their own curve — the band sits
where the knob would sit, not where a linear ruler says.

**Macros.** Params 166-173, mod source slots 2-9, knobs on MAIN. A macro is a
param-driven source: it modulates with NO note sounding (measured: routed
Macro 1 at 0.8, depth 0.25 → applied 0.7500 = 0.55 + 0.25·0.8·range, gate
closed, readback still 0.5500). Macros stay OUT of the morph field (a corner
that moved your controller would be a trap, not a timbre) and OUT of the
destination menu — macro-as-dest is fan-out, B70-adjacent, refused until its
cycle rule is ruled.

**The XY pads.** Each oscillator's pad drives two ASSIGNED macros (params
174-177 hold the assignment; defaults give osc 1 the pad M1/M2, osc 2 M3/M4).
MAIN shows the ACTIVE oscillator's pad, per the human's "until we come up with
a better system". The pad's own selects follow the active osc and therefore
carry no data-p (one control, two addresses); the always-reachable static
copies live on SET as data-fixed rows, which is also what satisfies gui_reach
honestly. Routing stays in ONE place: the pad writes macros, macros reach
parameters only through the matrix.

**Menu shape:** ten flat source entries would double the right-click menu, so
"Send to mod matrix — Macro…" keeps the menu open and swaps its items for the
macro list in place (the first `keep:true` pmenu item).

**Verified:** macro_probe (drive gate-free · track · base survives · return ·
refuse macro/assign-as-dest · assignment readback); in-browser: pad writes the
assigned pair (166/167 → 170/167 after reassignment), SET row re-aims the MAIN
selects, halo angles exact (base 0.3 → 84°, +0.4 reach → 196°, now 0.55 →
tick 14°), slider band renders and clears, submenu shows 8, matrix's own knobs
show none. Parity 156/156 — macros at defaults are the parity-safe superset
again.

**Known gap (unchanged from ADR-136, now louder):** routes still do not
survive a session reload. With macros now the performance surface, persistence
is the next increment.

## ADR-138 — route persistence on B72's link identity; the pitch route found by dest (2026-08-28)

**Status: ACCEPTED (B69 increment 6).** Generic mod routes now survive the
session — the gap ADR-136 and ADR-137 carried openly is closed. One
serializer, two transports: a `modroutes=` line in the CLAP state chunk and a
`"modRoutes"` string in the JSON preset, both the same canonical form.

**The canonical form IS B72's identity, established at the boundary first.**
`src:dest:depth;…` with one entry per (src, dest), depths summed — the SUM law
already makes the merged and un-merged forms indistinguishable, so
serialization canonicalizes for free, and the (src, dest) key is exactly what
B72's morph interpolation will later key on. The pitch route is NOT in the
chunk: it is param 161's and persists as that param — writing it twice would
double it on load.

**Restore goes through the shipped refusal path** (applyModRoutesChunk calls
modAddRoute), so a chunk naming a stepped destination, the matrix's own
controls, or a bad source is dropped, never trusted. **A load is a load:**
generic routes are replaced wholesale, and a state saved before routes existed
clears them — stale routes bleeding into a loaded patch would be state the
preset never named. A routeless patch emits no line at all, so existing state
bytes are unchanged.

**The by-dest fix.** Param 161's handler assumed the pitch route was INDEX 0 —
false the moment a restored generic route lands first, and already corruptible
by removing the pitch route in the GUI then automating 161 (the knob would
have overwritten some other route's depth). Both the handler and readParam now
find the pitch route by destination key. Second landmine in this id space
found before it fired; measured in the probe: editing 161 after restore leaves
the generic depths untouched.

**Measured (routepersist_probe, 10/10):** round-trip modulates in a fresh
instance (applied 0.95 = base + 0.5·0.8·range, readback 0.55 intact);
duplicates canonicalize (`2:4:0.5;3:9:0.25;`); pitch knob restores as 12 st;
load-is-a-load clears. state_check, preset_check, mod_check, parity 156/156
all green.

## ADR-139 — CHROME-001: the specimen replaces MAIN's phase circle, and sound strikes it (2026-08-28)

**Status: ACCEPTED (B74 v1).** The human's liquid-chrome material study
(`Chrome 001.dc.html`, from their website) becomes MAIN's visualizer, replacing
the phase circle there — which "currently makes the main page feel a little
redundant" (human) because OSC shows the same circle; OSC keeps its copy.
Ported shader-verbatim minus the React shell, plus exactly ONE shader
addition (uRipple). Requested material: surfaceTension 0.1 · pearl · 500 nm.

**The sound mapping (v1, all from the snapshot the bridge already ships —
zero new plumbing):** a fresh note-on POKES the metal (the source's dent
spring IS a percussion response; azimuth from pitch class, elevation from
octave, so a run walks strikes around the specimen); the loudest envelope
blooms filmThickness 500→750 nm (a literal hue bloom per attack); R drives
surface tension 0.1→0.45 (a phase-locked swarm pulls the lobes into one mass
— the order parameter as metal deciding whether it is one object); outPeak
drives uRipple (held level shivers the surface unpoked). Gate edges are
latched per-midi so a held note strikes once. Pointer interaction kept:
drag orbits, click pokes. The source's self-throttle kept: slow frames drop
render scale, never rate; ~30 Hz cadence off the existing rAF loop; skipped
entirely when MAIN is hidden.

**Two traps recorded:** (1) the theme-switch blanket clear called
getContext('2d') on EVERY canvas, which LOCKS a canvas's context type — WebGL
on the specimen was dead on arrival until the clear learned to skip it
(measured: fresh canvas got GL, chromeC returned null). (2) In the same
change, internal ids stopped reaching the UI: the pitch route rendered as
"ENV 2 → #2147483649"; modDestLabel now names synthetic dests, strips the osc
stride with an "· osc N" tag, and clones labels to drop .sc/.u decorations —
and the MOD page's ±48 slider detection keys on dest, not index 0 (ADR-138's
restore order made the GUI's index assumption false exactly as the shell's).

**No WebGL = a quietly blank cell** (lab harness, exotic hosts); the phase
circle still exists one page away. Verified in-browser: GL live, A4 poked at
pitch-class-9's azimuth, spring mid-decay, center pixel lit chrome; labels
"Pitch" / "Detune · osc 2". Deferred, named: visualizer uniforms as synthetic
mod destinations (patchable material), morph-corner colors on the hot
variant's rim lobes, multi-poke slots for chords.

## ADR-140 — the specimen becomes a toggle, off by default; the webview verdict and B75 (2026-08-28)

**Status: ACCEPTED (B74 v2).** ADR-139's specimen was measured untenable in
the VST ("jumpy, jaggy" — human, 2026-08-28, hours after v1 shipped). Ruling
applied: performance is preserved by DEFAULT — param 178 ships 0, MAIN gets
its phase circle back, and CHROME-001 becomes a SET → Visualizers toggle whose
ON state is a reduced-cost render for the human to judge.

**The cost went where the bill was.** v1 rendered 468×418 device pixels (dpr
1.5) × 64 march steps × 30 Hz. v2: FIXED low internal resolution, CSS-upscaled
(204×182 measured at cluster width — soft chrome upscales beautifully; retina
doubling was most of the bill, so dpr is deliberately not in the product), 44
steps, 20 Hz, and an IDLE GATE — nothing sounding, nothing ringing, nobody
touching it → zero draws, the last frame standing as a product photograph.
Net ≈12× cheaper per second while active, free while idle. Two throttle bugs
fixed in passing: the 40 ms EMA threshold would have throttled every healthy
frame at 20 Hz cadence (nominal gap IS 50 ms → threshold 75), and a long gap
(hidden page, idle wake) fed the EMA as if it were a slow render and collapsed
the scale (measured 0.65 → 0.455 from pauses alone) — gaps > 250 ms are now
excluded from the EMA.

**The webview question, answered honestly:** yes, a native C++ GUI would beat
the webview here — direct Metal, no compositor contention, and ADR-019 built
the seam for exactly this swap (hypersaw_gui.h is the ONLY interface the shell
knows; a native backend reimplements hypersaw_gui.mm and nothing else moves).
But it is a weeks-scale build against a minutes-scale fix, so: v2 now, B75
files the native backend as its own investigation, and the human's judgement
of v2-on in the VST decides whether B75 is urgency or luxury.

**Also:** the wordmark's outline EDGE_R 1.4 → 1.0 (human: "same thickness as
the other details" — a dilation ring reads fatter than a stroked line of the
same nominal width; 1.0 matches the 1.5px hairlines by eye, and a smaller
radius closes the ring more easily, not less).

## ADR-141 — one send entry; the modulator is chosen in the table (2026-08-28)

**Status: ACCEPTED (B71).** The human's ruling: *"it should just be 'send to
mod matrix' and it sends it to a table and from there you can decide which
modulator to link it to (or x to release it)"*, plus a *"release all
modulators"* verb. Supersedes ADR-137's in-place macro submenu — that shape
assumed the source is chosen at send time, and ten sources made the menu the
clutter the right-click was meant to prevent.

**The menu is now two items, permanently.** "Send to mod matrix" on an
unrouted continuous parameter; "Release modulator(s)" on a routed one, never
both. Source count no longer touches menu length — B16's LFOs and B40's
bottom bar can add sources without the menu growing a pixel.

**No pending/unlinked state, deliberately.** A sent parameter becomes a REAL
route at ENV 1, depth 0.25, and "deciding which modulator" is a re-source of a
live row through the table's select. The rejected alternative is recorded
because it looks tidier than it is: a pending row would be a second route
representation needing its own persistence, refusal rules, halo semantics and
morph story — cost with no audible return.

**Two index traps closed.** (1) Release-all removes DESCENDING, because
removeRoute compacts the table: ascending removal deletes the wrong rows.
Measured in-browser — a dest routed at indices 0 and 2 removes as [2, 0] and
the unrelated route at index 1 survives intact. This is the same family as the
161 landmine (ADR-138); the third sighting. (2) modSetDepth's leftover
`if (i == 0)` comment claiming route 0 is knob 161's twin was deleted — false
since ADR-138 made 161 find its route by dest.

**The pitch route is not re-sourceable**: the shell refuses (synthetic dest)
and the row's select is disabled with the reason in its tooltip — ENV 2 is
ADR-135's contract, not a user choice. Stating the rule beats letting a click
fail silently.

**Layout, measured not assumed:** five controls across cannot fit the 240px
newspaper column ADR-098 gives this cluster (the destination label collapsed
to zero width and the row read as an anonymous slider). The row is two lines —
what the route IS above, what it DOES below — which also leaves the natural
seat for B70's depth-mod control. The MOD note stopped describing the
pre-ADR-138 "route 0" world.

## ADR-142 — the standard Delay: a module with no lab, whose oracle is its spec (2026-08-28)

**Status: ACCEPTED (B68 / B73 increment A).** Slot type 9, `src/delay_core.h`,
32 per-slot params (232–263, four blocks of 8 — ADR-131's arithmetic, not
cases). It is simultaneously the A/B baseline the human asked for in B68 and
the reference the swarm delays get judged against in B73.

**Why it exists, measured in `src/time_core.h` rather than asserted.** The
human's report was *"the feedback from their lab didn't really work very
well"*. Three compounding causes, all visible in the source:
1. `fbSig = wetRaw / n` is ADR-031's worst-case-correlation norm. At the
   default 8 taps the audible loop gain at regen 0.9 is ≈0.32 — repeats die
   in two or three generations and the edge-of-oscillation zone, the musical
   heart of a delay, is unreachable at any knob setting.
2. Every tap averages into ONE write head, so each generation re-smears
   through the whole swarm: a wash, not repeats.
3. `tanh` sits at unity in the loop and damping is always in circuit, so
   every pass ducks and dulls even when the patch asked for neither.

**THIS CORE HAS NO HTML LAB, DELIBERATELY**, and that is a real amendment to
ADR-003's spec-in-code rule for this one module. Writing a lab first would
have reproduced the rejected law; B73's roadmap entry is the standing license
to diverge. So the file IS the spec and `tools/delay_check.cpp` IS the
correctness definition — ten impulse-response invariants rather than parity
against a reference that would only enshrine the defect.

**Three bugs the oracle found in its own author's work, each fixed at the
source rather than papered over in the test:**
- **The limiter shaped the dry path.** `softLimit(in + loop*fb)` compressed a
  full-scale input entering a delay with feedback 0 — measured 0.976 where it
  should be 1.000. It now limits only the returning term, so the input path is
  arithmetically clean and past-unity feedback still parks at a ceiling (the
  fixed point of x = limit(1.08x) is ≈1.0; L0-D4 measures 1.0000 over 10 s).
- **Damping was always in circuit** — the exact complaint this module was
  written to answer, reproduced by its author. `damp = 0` and `loopHp = 0` now
  BYPASS their filters; "open at 18 kHz" is still a one-pole colouring every
  pass at 48 kHz, and it showed as a measurable error in the generation ratio.
  With true bypass the ratio is EXACTLY 0.5000 at feedback 0.5.
- **Nothing snapped the read head on load.** A knob move should glide (tape
  retime) but a preset arriving should not: repeats landed late and pitched
  until `snapTime()` existed. The rack calls it when a slot becomes a Delay.

**Two measurement traps recorded, because both would have passed a careless
test.** Peak amplitude is NOT loop gain — a fractional read splits an impulse
across two samples, so peaks read 0.4735 where the gain was exactly 0.5;
summed magnitude is invariant under that split. And summed magnitude is NOT
brightness — a one-pole lowpass has unity DC gain, so it preserves the sum
exactly (dry and damped both measured 0.6000); darkening shows in the PEAK
(0.6000 vs 0.0632). Pick the statistic the physics actually moves.

**Storage is float, on the heap.** Two double-buffered cores are 8 MB and
segfaulted the oracle on the stack. Float halves it, the loop state stays
double where the recursion lives, and the rack holds `unique_ptr`s as it
already does for TimeCore. rtsafety stays GREEN: allocation happens only at
construction.

**Deferred with reason:** M-S mode. The human listed "std/M-S/ping-pong";
crossfeed = 1 IS ping-pong, and M-S needs a mid/side seam the rack does not
have. Named here rather than silently dropped.

**The oracle is NOT yet in `./verify`** — adding a gate is a human decision
(charter). It builds and passes; the test row says `none` rather than naming
it, because a row claiming a gate that does not run is the false coverage the
table exists to prevent.

## ADR-143 — feeds are gated on visible consumers; the specimen was never the cost (2026-08-28)

**Status: ACCEPTED.** The human reported the interface *"still lagging as
badly as it was when the blob was visible"* and asked the right question: is
it still computing in the background? **It is not — measured, 0 GL draws with
the specimen off** (and 0 again with it on but nothing sounding, so ADR-140's
idle gate holds too). The lag was never the specimen. It was the frame loop.

**What was actually happening: 150 bridge round-trips per second, on every
page, forever.** `vizFrame` and `specFrame` ran every frame and `drawScope`
every second frame, none of them gated on whether anything they feed was on
screen. On FX, MOD, SET and MORPH — where NOTHING consumes them — the JS half
alone cost MORE than on MAIN (0.473 ms per vizFrame vs 0.200), because the
work still happens and the paint lands in canvases nobody can see. Over a
webview bind, where each call is marshalling plus a JS evaluation rather than
a function call, that traffic is the bill.

**The gate is derived from the DOM, never a hardcoded page list.** A feed runs
only when the visible page contains one of its consumers, so putting a
`canvas.spec` on a new page turns its feed back on by itself — the same
one-state-many-renderings discipline the phase circle and XY pads already use,
applied to the FETCH instead of the paint. `.meter` is in the viz set because
MIX's strip meters ride the viz snapshot even though MIX draws no circle.

**Cadence halved for the two heaviest feeds** (30 Hz, not 60), on the argument
this file already makes for the wordmark's warp: halving the rate halves the
cost outright, and neither a phase circle nor a smoothed spectrum reads
differently at 30. Viz and spec are given opposite frame parity so they never
land in the same frame.

**Measured, per second at 60 fps:**

| page | before | after |
|---|---|---|
| MAIN | 150 | 92 |
| OSC | 150 | 62 |
| MIX | 150 | 32 |
| FX · MOD · SET · MORPH | 150 | **2** |

**The mod halo's DOM sweep is now scoped to the visible page** — it queried
the whole document (~200 controls) and called getBoundingClientRect per hit,
a forced reflow 20 times a second over controls that were not on screen.

**I stepped in this handler's own documented trap.** `recomputeFeeds()` first
went in BEFORE the page reveal, where no page carries `.on` — so every feed
read false and OSC's phase circle, spectrum and voice map went dark (measured
`{viz:false, spec:false, scope:false}` on a page that needs all three). The
comment three lines above says "REVEAL FIRST, THEN PAINT" and records the
identical bug from 2026. Second victim, same line.

**This is a floor, not a ceiling.** MAIN still pays 92 calls/s. The structural
fix is one batched snapshot bind instead of three per-frame calls (filed as
B76), and under it B75's native backend. Whether 92 is tenable is the human's
judgement in the DAW, not mine from a localhost profile.

## ADR-144 — the specimen returns: adaptive resolution, and a full-resolution plate at rest (2026-08-28)

**Status: ACCEPTED (B74 v3).** With ADR-143 having found and removed the real
cost, the human asked for the specimen back and named its remaining fault:
*"it was fairly low resolution on the last pass."* It ships ON again, as its
own cluster **first in MAIN's left column** (their placement), and the
resolution stops being my guess.

**A fixed scale was the wrong SHAPE of answer.** ADR-140 pinned the render at
0.65× with retina deliberately excluded — a number chosen while hunting a
stall that turned out to be the frame loop, not the shader. The affordable
resolution is a property of the machine, so the scale now adapts in BOTH
directions between a floor of 0.5 and a ceiling of the display's own pixel
grid (capped at 2×; past that the samples land inside a pixel nobody can
resolve). The throttle that only ever stepped down now steps back up.

**A band, not a threshold.** Down at an EMA above 75 ms, up below 58 — a
single trip point makes the scale oscillate across it forever. Down is
immediate, because a stalling interface must recover now; up is patient, 40
sustained comfortable frames per step, so a brief calm cannot buy a resolution
the machine will not keep. Measured on this display: it climbs to the 2× ceiling
on its own and renders **628×560 device pixels into a 314 px canvas** — against
roughly 204 px on the last pass, about nine times the pixels.

**The plate.** ADR-140 called the idle frame "a product photograph"; this makes
it literal. When the specimen settles it draws ONE more frame at the ceiling
and then stops — measured, exactly 1 draw across 120 frames. Motion is where a
dropped pixel hides; stillness is where it shows, and stillness is the state
you look at longest. The one expensive frame per settle is the deliberate
trade, and it is bounded at one.

**The redundancy ruling still holds:** the specimen and MAIN's phase circle
never share the page. With the specimen on, the circle lives on OSC; with it
off, the circle takes MAIN's viz slot back. Markup defaults now match the
shell default (178 = 1) so a no-engine preview shows a state the plugin
actually has.

**Still open, honestly:** whether 2× at 20 Hz is affordable in the DAW is the
human's judgement, not a localhost profile's. If it is not, the adaptive
scale finds the floor by itself now instead of waiting for a session — which
is the difference between this pass and the last.

## ADR-145 — the CLAP was never installed, and the legacy bundles shadow the new ones (2026-08-29)

**Status: ACCEPTED.** The human said *"I think the old version is still
loading."* They were right, and the cause was two defects in `./install`, not
in the GUI.

**1. `./install` never copied the CLAP.** The build emits `horde.clap`; the
script installed only the VST3 and the AU. The only CLAP in `~/Library` was a
hand-placed `HYPERSAW.clap` from **Aug 25** — four days stale. On a CLAP-first
plugin (ADR-002) that means the format this project develops against was the
one format guaranteed to be old. The omission was invisible because nothing
ever compared what was built against what was installed for that format.

**2. Legacy bundles shadow the current ones.** `HYPERSAW.vst3` and
`HYPERSAW.component` (Aug 27) sit beside `horde.vst3` / `horde.component`, and
every one of them declares `com.lifted-truck.hypersaw` and the AU triple
`aumu/Hsaw/LfTk` — **frozen forever by ADR-002**, which is exactly what makes
the duplicate dangerous rather than harmless. A host may resolve to either.

**A correction to earlier claims, because it matters.** Several PRs and traces
this week ended with "installed; auval SUCCEEDED". With two components sharing
one AU triple, `auval` may have been validating the Aug 27 bundle rather than
the build just made. The line was reported in good faith and is not
trustworthy as written; what IS verified now is stated by embedded-string
comparison instead (L0042's method — identifiers do not survive compilation,
so check the artefact for strings only the new source contains):
`specimenBox` and `scaleMax` appear in all three installed `horde.*` bundles
and in none of the legacy ones.

**The script does not delete them, and will not.** Deleting a human's
installed plugins is not an agent's call, and a stale bundle may be kept
deliberately to open an old session. `./install` now names each legacy bundle
with its date and prints the exact `rm -rf` on every run until they are gone,
and finishes by listing what it actually installed with timestamps — so
"is the thing on disk the thing I just built" stops being a question anyone
has to ask.

**Filed as B78** — and the roadmap item this trap was supposedly already
tracked under (B58-4) turned out **not to exist in ROADMAP.md at all**. It
lived only in session memory. A citation to a roadmap entry that is not there
is the same failure class as a test row naming an oracle `./verify` does not
run: provenance that cannot be checked. The reference in the script was
corrected to the item that now genuinely exists.

## ADR-146 — the webview un-throttled at the source: B79 executed, not investigated (2026-08-29)

**Status: ACCEPTED.** The human: *"So can we please fix the issue?"* — after a
watchdog, a health line, and a filed investigation. Fair. This is the fix
itself, not another layer around it.

**The measured defects** (health line, in Ableton): `frame 69ms` — WKWebView's
occlusion heuristic decides a host's child view is "background" and throttles
rAF to ~14 fps while fully visible — and `dpr 1` — the WebContent process
never picks up the retina backing scale, so every canvas in the GUI renders
soft. Identical page in a browser: 6 ms, dpr 2. The second defect explains the
human's original "fairly low resolution" verdict better than anything in the
specimen's own code ever did.

**The fix:** three WebKit switches applied in `hypersaw_gui.mm` at webview
creation and again at attach (when the real window and its scale exist):
occlusion detection off, visibility-based process suppression off, and an
explicit device-scale override from the window's `backingScaleFactor`.

**How, and the tradeoff stated:** these are WebKit SPI, reached through KVC
(`setValue:forKey:@"windowOcclusionDetectionEnabled"` resolves to
`_setWindowOcclusionDetectionEnabled:` by KVC's `_set<Key>:` search rule) —
no private headers, and every call in @try. A WebKit rename degrades to a
silent no-op, never a crash, and the regression is then VISIBLE: the health
line reads `raf 69ms · dpr 1` again instead of `raf 16ms · dpr 2`. The JS
timer watchdog (previous commit) stays as the floor for exactly that world.
This is a locally-installed instrument; an App Store build would need this
revisited, and that constraint is recorded here rather than discovered.

**Verified here:** builds clean, parity 156/156, verify fast exit 0, auval
SUCCEEDED, installed binary carries both KVC key strings. **Verdict pending
where it counts:** the health line in Ableton after relaunch — `raf 16ms ·
dpr 2` is B79's written exit criterion, and the human's next screenshot is
the measurement.

## ADR-147 — the specimen's visibility is a GUI preference, not patch state (2026-08-29)

**Status: ACCEPTED.** Three debugging rounds in a row ended with "please flip
a checkbox", and the checkbox stayed unflipped while the human asked what
success is supposed to look like. That is the software outsourcing its own
defect. The defect: ADR-140's few-hours off-by-default era wrote `specimen=0`
into the human's working set, and every reload re-hid the blob regardless of
what was installed.

**The rule:** whether a visualizer shows belongs to the machine and the
moment — like the selected tab, and unlike anything that sounds. Param 178
stays writable live (turn it off for a session if it bothers you) and is
still WRITTEN to state for forward compatibility, but it is **never read
back**: both restore paths (the CLAP/VST3/AU chunk and the JSON preset) skip
it. An old set with 0 in the chunk loads with the specimen on; state_check,
preset_check and parity are all green because the param never touched sound.

If a persistent per-machine preference is ever wanted, it goes in the GUI's
app-support store beside the presets (the ADR-105 A2 mechanism), not in the
patch.

## ADR-148 — the per-voice seam: a note tap with a bit-identity contract (2026-08-30)

**Status: ACCEPTED (B81/B82 increment 1).** The structural move both FILT and
per-note modulation stand on: SwarmCore gains an optional NOTE TAP — when
installed, each note's contribution to a block reaches the tap as DOUBLE
buffers before the shared bus; the core then re-adds the (possibly filtered)
buffers with the direct path's exact float-store arithmetic, in the same note
order. `tap == nullptr` IS the original code path, pointer for pointer.

**The double scratch is load-bearing.** The direct path computes
`(float)((double)out + lp·g)`. Capturing `lp·g` pre-rounded to float would
change the double addition and silently break the identity — the proof would
have failed on T2 and someone would have "fixed" it with a tolerance. Raw
doubles all the way to the flush keep the arithmetic literally the same.

**What stays where:** the width/tanh tail runs after all notes in both modes —
filters are per-note, the bus character remains the bus's. Raw function
pointer + ctx, never std::function: audio thread, no allocation, no type
erasure (rtsafety stays GREEN). Frames > kTapMax (impossible at real rates)
falls back to the direct path rather than overrunning. SPECTRA is explicitly
DEFERRED: this increment covers SwarmCore; the SPECTRA seam lands when a
consumer needs it, not speculatively.

**Proven (voicetap_check, first full run green):** no-op tap bit-identical
across five feature configs (defaults, voice envelopes, oversampling+
decimator, super-width A, scatter+glide) rendered in deliberately odd chunk
sizes; a unit-coefficient one-pole — real filter code — also bit-identical;
a real LP audibly in-path (Σ|Δ| = 229); per-note buffers carry exactly the
gated notes. Parity 156/156 untouched (no plugin code installs a tap yet);
state, rtsafety green. The oracle is NOT yet a ./verify gate — human decision,
proposed with this ADR.

**Next increments ride this:** B81-2 (SVF pair + routing + filter envelope +
FILT page), B82 (per-note evaluate with per-voice source vectors), and the
human's parity re-baseline pre-authorization stands recorded in B81 for the
day a consumer changes the summed sound.

## ADR-149 — MIDI/MPE performance signals join the matrix (2026-08-30)

**Status: ACCEPTED.** Sources 14-17: **Velocity** (last note-on), **Mod
Wheel** (CC1), **Pressure** (channel aftertouch or any note-expression
pressure, whichever spoke last), **Pitch Wheel** (bipolar, −1..1 — the first
bipolar source, and the shape B87's bipolar-modulation want will generalize).
kMaxSources 16 → 24.

**Two of these were being DROPPED on the floor:** the MIDI handler read only
0xE0 — CC1 and channel pressure never reached anything. The wheel your hand
was on had no path into the instrument except the pitch lane.

**Scope, stated honestly:** these are GLOBAL projections (last-note velocity,
one pressure). TRUE per-note routing — this voice's velocity to this voice's
cutoff — is B82's build, now unblocked on both sides: ADR-148 gave it the
audio seam, this gives it the source vocabulary; what remains is per-note
evaluate + the dest whitelist.

**Measured through the shipped path** (midisrc_probe): a real CC1 event at
127 drove a routed destination to exactly depth × range (25.0 on the
100-cent drift lane at 0.25), base readback intact. mod/mpe/notefuzz/parity
all green; the pitch-wheel channel-0 path is untouched (the source capture
rides beside the existing applyParam(38) write).

## ADR-150 — 2026-08-31: MAIN gets its own hands — separate main XY, continuous pitch, morph mini, wheels

**Context.** The human's 2026-08-31 batch, all one theme: MAIN was borrowing
the OSC pages' controls instead of owning its own. The "main" XY pad was the
active osc's pad wearing a different frame; transposition only moved in
integer jumps so morphing between presets with different tunings stair-stepped;
pitch bend lived on the OSC page only and mod wheel had no GUI surface at all.

**Decisions.**
1. **The MAIN pad is its own instrument** (params 179 `mainAsnX` / 180
   `mainAsnY`, default macros 1/2 — the detune/K pair, centered at 0.5 so the
   default patch puts the puck mid-pad). It never re-aims when the edit osc
   changes. The OSC pad keeps its 174-177 per-osc assignment behavior. One
   pad controller (`wirePad`) drives both with different id sources — the
   pads-as-class discipline B85 asked for, landed here.
2. **Continuous per-osc pitch** (param 181 `oscPitch`, ±24 st, float). It sums
   into the same per-osc tuning term as transpose/octave but is NOT quantized
   and NOT in kGlobalIds, so it twins and morphs — the smooth blend the
   integer transpose knobs structurally cannot give. Transpose stays integer;
   this is a parallel lane, not a replacement.
3. **Morph mini on MAIN** (B85 first landing): a light second rendering of the
   morph field — bilinear MCOLORS ground + live puck at (152,153), drag writes
   through the same gesture path. The heavy baked-field pad remains the MORPH
   page's; the mini redraws only when the puck moves (`morphMiniDirty`).
4. **Wheels on MAIN**: the bend wheel widget re-mounts to MAIN's Wheels
   cluster (param 38, spring-back behavior unchanged) beside a new mod-wheel
   slider driving matrix source 15 via a new `setModWheel` host hook — the
   GUI wheel and a real CC1 land in the same `srcWheel`, so there is exactly
   one mod-wheel truth.
5. **Logo**: colored-stroke hue offset 90°→120° (90 produced no pink/purple
   anywhere on the field — the human auditioned it); with morph toggled OFF
   the mark now drifts hue medium-slowly (~50 s full cycle) instead of
   freezing on meaningless position colours.

**Also filed:** B88 (scale-quantize slider + chord topology — the human's
stochastic chord-cluster vision, recorded verbatim, decomposition deferred).

**Evidence.** parity 156/156 (181 defaults 0 → tuning term adds 0 → bit-clean),
paramscope/state/mod green, lab_load green, gen_gui_controls 198 controls.


## ADR-151 — 2026-08-31: MAIN polish round 2 — one field painter, self-gated mini, honest XY reach, eased tension

**Context.** The human's review of the ADR-150 landing: the mini morph was
"the wrong aesthetic" and went blank on a screen-colour change; the new
clusters had nested inside Presets (a markup slip — the close tag sat after
them); the pads silently drove four parameters with no way to see or refuse
it; the specimen snapped taut in a single frame on a strike and its held
waves read as jerky quivering on high notes.

**Decisions.**
1. **One morph-field painter.** The mini's hand-rolled bilinear blend is
   deleted; `paintMorphField()` is extracted from the big pad's `drawField`
   and both call it — the pads cannot drift apart again. The scheme-switch
   blank had the same root as the 2026-08-24 big-pad bug (`_padInvalidate`
   re-bakes; the mini had no hook), so `_padInvalidate` now wakes the mini too.
2. **The mini shows only while morph is ON** (param 151), via a
   `data-selfgate` marker that `applyGates` respects — its cluster loop was
   recomputing `hidden` for every cluster and silently clobbering any
   visibility another system set. Self-gating clusters are now first-class.
3. **Layout:** Presets closes before the XY/Morph/Wheels clusters (they were
   accidentally its children); Macros moves to the controls column — the
   viz column is visualizers only, per the human.
4. **Honest XY reach.** Every assignment select gains **None** (value 8;
   params 174-177/179/180 max 7→8; the C++ alias slots emit 0.0 for an
   unassigned axis instead of `& 7`-wrapping onto Macro 1). Each pad carries
   a live note listing what its axes actually touch: the assigned macro plus
   every matrix route riding it (and the pad-axis alias slots, for the osc
   pad) — rendered from MODROUTES, no second copy.
5. **Specimen:** uK is eased (tighten ~280 ms — the lead-up — relax ~500 ms)
   instead of stepping with R. The held-wave temporal rate saturates at 26
   rad/s: it used to scale with the note's spatial wavelength (2.6·W, ~10 Hz
   at W=24), under-sampled at frame rate — that WAS the "jerky quivering on
   the higher frequencies". Spatial wavelength still tracks the note.

**Evidence.** verify full exit 0 (range widening is parity-safe: defaults
unchanged, None emits the 0.0 an absent source always had); in-pane: gating
holds through applyGates both ways, corner pixel matches the field bake,
route note renders "x → Macro 1 → Detune". Installed 6ee53f5.


## ADR-152 — 2026-09-01: morph corner integrity — macro family suspends under morph; capture flattens (QM-4 ingested)

**Context.** The human: corner presets whose identity lives in K/detune were
being flattened because the global XY/macros write offsets on top of whatever
the morph resolves — the pad position is an invisible fifth author of every
corner (QM-4's P1, observed here before the spec named it). Worse, capture
stored BASE values (ADR-136 readback contract), so a corner authored with
macros displaced never contained the sound that was captured.

**Decisions.**
1. **The macro family (source slots 2-13: macros + pad aliases) is SUSPENDED
   while the morph is ON** — sources read 0, routes contribute nothing, every
   dest releases to base, and base follows the morph field (the ADR-136
   intercept at the applyParam choke point). Performance sources (ENV 1/2,
   velocity, wheel, pressure, pitch wheel) stay live: gestures, not layout.
   Non-destructive — no routes are removed; the GUI greys the suspended rows
   and the pad route notes say "(suspended: morph on)".
2. **Capture flattens** (QM-4 §7 brought forward): morphCapture bakes the
   macro family's live contribution (mod.src × depth × span, clamped to the
   dest's range) into the stored corner, so the corner IS the sound that was
   authored. Corners captured before this ADR store base-only — re-capture
   to upgrade them.
3. **QM-4 ingested** (`QM-4-intent-bus-spec.md` + `horde-intent-bus-prototype.html`,
   filed as B89): the real architecture — intents, corner-owned bindings,
   modulation tiers, atomic structural resolution. This ADR is explicitly the
   interim; B89 retires it. One sanctioned prototype edit outstanding: seed
   `reshuffle()` (unseeded Math.random, the CANTO/STATION blocker class).
   Both files enter the protected list (they ARE the reference).

**Rejected:** removing macro routes on morph-on (destructive, loses user
routing); corner-specific route tables now (that IS B89 §6.2 — building it
twice as an interim would be the complexity the reduce-never-invent tenet
exists to stop).

**Evidence.** verify full exit 0 (suspension is inert with morph off — the
shipped default — so every parity chain is untouched by construction);
md->base follows morph writes at hypersaw_clap.cpp:3150, so suspended routes
cannot fight the field. In-pane: route rows grey under morph, notes update.


## ADR-153 — 2026-09-01: SvfCore — the per-voice filter's core (B81 inc 2, slice 1)

**Context.** B81 increment 2 opens: the SVF pair that rides the ADR-148 tap.
First slice = the core + its spec, nothing else — the shell integration,
routing, envelope, and FILT page follow on the proven core.

**Decisions.**
1. **TPT (Zavalishin) state-variable form**, LP/BP/HP off one pass, chosen
   for stability under audio-rate cutoff motion — the whole point of this
   filter is to be swept by a per-voice envelope.
2. **Rest damping k = √2 (Q = 0.707, Butterworth)**, res → k floor 0.05.
   The oracle caught the first draft here: k = 2 measured −6.02 dB at fc —
   critical damping, not the classic synth lowpass. Measured, then fixed;
   the suite records both numbers.
3. **Bit-exact bypass** (voicetap discipline: off is provably absent) and
   **caller-owned parameter slew** — setParams is state-pure (T7 proves a
   redundant set is a no-op), so the shell's seconds-based tick coefficients
   (ADR-009) own all smoothing and the oracle can pin input/param pairs to
   exact outputs.
4. **Oracle-as-spec from birth** (`svf_check`, eight probes: bypass
   bit-exactness, DC law, −3 dB at fc at two sample rates, 12 dB/oct
   rolloff, bounded resonance, corner stability as bounded-AND-decaying
   windowed peaks, setParams purity). No reference lab exists on purpose
   (B81: the E1 labs are bus creatures). Registered in CMake, NOT wired
   into ./verify — the standing human ruling covers it with
   delay/strata/voicetap.
5. **Slot renumber recorded:** the filter env's matrix source slot is 18 —
   the entry's original "slot 14" predates ADR-149 taking 14-17.

**Evidence.** svf_check GREEN (0 failures) after two honest reds: the k-law
fix (design bug, caught by T3) and two probe fixes (T6's absolute floor was
aspirational against a 10 Hz / Q≈20 ring's physics; instantaneous tail
samples land on arbitrary ring phase — windowed peaks measure the envelope).
verify full untouched: the core is unwired, zero shipped-path changes.


## ADR-154 — 2026-09-05: autonomous notice-001 taken at intake — frozen identifiers reaffirmed, brand adoption deferred to /retrofit, cleanup sequenced

**Context.** autonomous (Decision 69) filed the inventory of Ableton sets a
param-ID cleanup would break and the sequencing ruling: rename first, two
identifiers never move, cleanup last with a migrator decision. Filed as a
notice (`ball: none`); relayed by the human 2026-09-05.

**Decisions.**
1. **Intake, not just receipt.** Every checkable claim was verified against
   our tree before commit and the cites are affirmed in the frontmatter:
   ADR-002 (frozen CLAP id → VST3 UID), ADR-114 (the device rename kept every
   identifier), CMakeLists.txt (`LfTk`/`Hsaw`/`aumu` exactly as stated).
2. **Reaffirmed as invariant:** the VST3 class ID and the AU triple
   `LfTk`/`aumu`/`Hsaw` do not move in ANY rename, brand included. This was
   already ADR-002's freeze; it is now also an upstream ruling with a
   28-set inventory behind it.
3. **Brand adoption ("Mind Lathe", `com.mind-lathe.<plugin>`) is DEFERRED to
   the next `/retrofit`** and gated on the synced CONVENTIONS §Audio plugins
   actually changing — at intake it still reads "Lifted Truck". Only the
   manufacturer NAME and bundle prefix change then; per (2) the binding
   identifiers do not, so shipped sets keep loading (the HYPERSAW→horde
   precedent).
4. **The cleanup is a compatibility event** (B98), opened only after the
   account rename and autonomous's K5 routines; migrate vs version vs accept
   is decided then, against the inventory.

**Evidence.** notice-001 read in full; CMakeLists.txt lines 128-143 and 164;
CONVENTIONS.md §Audio plugins as synced on 2026-09-05.


## ADR-155 — 2026-09-07: repo layout — reference implementations and specs get their folders; names standardised

**Context.** The human: "the spec sheets and labs should be put in organized
folders, and should probably have their names standardized if that wouldn't
be too destructive." Twenty-four tracked files sat at the root beside the
harness docs, in three naming styles (`SPEC-X.md`, `X-SPEC.md`, `SPEC_x.md`;
`horde_x.html`, `horde-x.html`, `x.html`).

**Decisions.**
1. **`reference/`** holds the spec-in-code prototypes — the files the parity
   oracles extract cores from (protected paths). Named for their ROLE, which
   is what distinguishes them from the design labs that already live in
   `docs/design/` with their own generated index. Basenames standardised to
   hyphens with the redundant `horde_`/`horde-` prefix dropped (the repo is
   horde): `decoherence-lab`, `distortion-engine`, `formant-pulsar-fof`,
   `intent-bus`. The `swarm*` names are untouched — 27 historical citations
   and the ADR-011/012 lineage hang off them.
2. **`specs/`** holds every spec plus ACCEPTANCE.md, all as `SPEC-<NAME>.md`:
   `STATION-SPEC` → `SPEC-STATION`, `QM-4-intent-bus-spec` → `SPEC-INTENT-BUS`
   (the QM-4 numbering stays in the document's own title), `SPEC_swarm_glitch_modules`
   → `SPEC-GLITCH-MODULES`. `PRIOR-ART.md` and `PARKED.md` (design records, not
   specs) go to `docs/`.
3. **History is not rewritten.** `traces/`, DECISIONS.md, `audits/`,
   `briefs/`, `integrations/` keep the paths as they were when written — a
   trace citing `swarmsaw.html` at the root is a true statement about that
   date. Every LIVE reference (47 files: tools, `./verify`'s structure list,
   CLAUDE.md's protected list, README, ROADMAP, LIBRARY, the specs' own
   cross-refs, core comments, the design labs' links, `index.html`) was
   rewritten in the same change, root-relative; files inside a target folder
   refer to siblings by bare name so relative links keep resolving.
4. **`git mv` throughout** — every file keeps its blame and log.

**Evidence.** `./verify full` exit 0 after the move (every parity chain found
its reference at the new path — the goldens regenerate from `reference/`);
lab_load 26/0; zero old-name hits remain in live files; zero double
prefixes; git reports 24 renames.


## ADR-156 — 2026-09-10: per-osc XY pads control their parameters directly; only MAIN's pad is a macro (reversal of ADR-137's osc-pad half)

**Context.** ADR-137 made every XY pad a macro controller; ADR-150 gave MAIN
its own; ADR-152 suspended the macro family under morph to protect corner
identity. The human's 2026-09-10 report is the composite failure: OSC 2's pad
does nothing by default, MAIN's pad drives detune and K for both oscillators,
and turning the morph on "often breaks the patch" because the workflow sets
K and detune via the pad — which under ADR-152 means the values were macro
offsets on floor defaults, and suspension dropped them to the floor.

**Decision (human).** Per-osc pads write their parameters directly (K and
detune for that oscillator) — they are ordinary parameter edits, so corners
capture them and the morph carries them. MAIN's pad remains the one macro
pad (assignable, ADR-150). The floor defaults and the default M1/M2 routes
that existed only to give macro-driven pads a rest are reversed. The human's
STRATA nested-macro system is deferred to a later release by their own call.

**Consequences.** Simpler than what it replaces (reduce, never invent): one
macro pad instead of three, no suspension-induced patch breakage on morph
toggle, and no floor defaults lying under musical rests. B104 carries the
build, including the oracle re-alignment and the pad-axis alias sources'
fate. ADR-137 stands for MAIN's pad and for the matrix vocabulary.


## ADR-091 / ADR-122 / ADR-152 — Amendment 2026-09-10: the three sanctioned RNG seeds are done

**What landed** (PR #531, Sonnet implementer under a lead brief): each
protected prototype's one sanctioned edit — `reference/intent-bus.html`
`reshuffle()`, `reference/formant-pulsar-fof.html` `FormantCore` masking
draw, `reference/station.html` Wave RAM `RND` — now draws from a mulberry32
stream (the byte-identical form the design labs carry) seeded from a
persisted `seed` field with a UI control. No other line in those files
changed; only explanatory comments still mention `Math.random`. Determinism
proven by a vm harness that extracts the shipped lines by number: same seed →
identical, different seed → different, in all three.

**What this does NOT change.** Candidate status for CANTO and STATION is a
human ruling, not a consequence of the seed — and both are parked for 1.0
regardless (§"1.0 — DEFINITION OF DONE"). The sanction is spent; the
protected-paths list in CLAUDE.md should drop the "one sanctioned edit
outstanding" clauses at the next touch.

**Two findings for the human.** (1) `specs/SPEC-INTENT-BUS.md` §12 T9 reads
"Reshuffle: flip points change" as unconditional; under a seeded reshuffle,
clicking again with an unchanged seed reproduces the topology by design — the
spec (protected) wants one sentence: "with an unchanged seed, Reshuffle
reproduces the topology; change the seed for a new one." (2) The lab-load
sandbox never covered `reference/` (its default sweep is `docs/design/` +
the GUIs), and two of the three prototypes fail it on missing browser globals
(`Option`, `devicePixelRatio`) — pre-existing, confirmed against pre-edit
files. Adding those globals to the sandbox stub and `reference/` to the
sweep is a gate edit: ROADMAP B108, human-gated.
## ADR-157 — 2026-09-10: state header and per-patch engine revision (ACCEPTED, from the B100 stream's draft)

Every state blob carries `{schema, engine_revision, build}`. `schema` is the
wire format (the chunk's version line; the preset's `"schema"`); `build` is
provenance only. **`engine_revision` pins DSP behaviour per patch:** a
sound-changing law lands behind a revision gate consulted at ONE site
(`Plugin::engineRevision()`); a patch keeps rendering under the revision it
was saved with until opted forward by a patch-level control (the u-he/Surge
pattern; the GUI control is a follow-up, the shell hook exists). A blob with
no header is revision 1 by definition — every session saved before
2026-09-10. A revision above the latest a build knows CLAMPS to the latest
(never store a value with no laws behind it; the re-save records what
rendered). `kEngineRevision` moves only with the ADR that introduces a gated
law; the parity-safe-superset discipline (new law behind a default-old
switch) is the revision-1 path by construction. Adding the header bumps
neither transport's schema.

**Corpus.** `tests/state_fixtures/` is append-only: a fixture turning red is a
finding, never a regen. `statefix_check` asserts load + 1 s render
bit-identical per fixture (`gen_state_fixtures` is manual — it writes tracked
files). Wiring `statefix_check` into `./verify full` after `state_check` is
recommended by the stream and is the human's gate decision (B100 row).

**Consequences.** Goldens v2 (1.1) become a second gated set beside the
rev-1 legacy-conformance goldens. The JSON preset path must stop truncating
full patches (B110) before its fixtures land — the stream refused to render
a golden that would have pinned the truncation, which is the corpus rule
working on its first day. Accepted as filed by the lead 2026-09-10; the
stream's three scope additions (a shared `statefix_common.h`, clamp-not-
preserve, two headless debug exports) are ratified here.

## ADR-158 — The quantiser starts over when its anchor moves (2026-09-11)

**Context.** The wheel lane (`bendGlide`, `src/hypersaw_clap.cpp` bend grid
tick) steps with `base = lastNoteKey` and its emitted offset multiplies
every voice. `GlideCore::quantise` keeps three pieces of state in ABSOLUTE
pitch — the committed step `qStep`, the hysteresis latch on it, and the time
gate's clock `qT`, which resets on COMMIT. None of them knew the anchor could
move. With Step Timing engaged (ids 146–148) a second key struck inside the
window could not commit its own step, so the lane emitted `oldStep − newKey`
and every held voice transposed by the interval between the two notes until
the window elapsed. Chromatic, wheel at rest, no scale involved — the
human's "randomly wandering S&H on pitch" when playing chords. Measured by
`tools/anchor_check.cpp` (worst 12 st). The reference (`docs/design/bend-lab.html`)
carries the identical latch and never meets a base change: it is a
single-note bench, so this is L0031's blind spot once more.

**Decision.** In `GlideCore::quantise`, a change of `base` re-arms the
quantiser exactly as `reset()` does (`qArmed = false`, gate open): the new
anchor commits immediately and the gate's clock restarts at the note-on. A
note-on is not a wheel gesture; the gate exists to turn a WHEEL move into a
glissando run, and it still does (anchor_check T2 is the must-read-non-zero
control). Implemented in the core, not the shell, because the latch's
coordinate system is the core's own and any future consumer that moves its
anchor (a per-note lane on slot reuse, an arp) inherits the rule.

**Parity.** Every golden runs `base` constant (bench: single note), so the
new branch never executes there — parity-safe superset, `./verify full`
green including the glide chain. Not a revision-gated law (ADR-157): no
session could have relied on a chord transposing by accident.

**Consequences.** `anchor_check` is standalone and unwired (human gate to
wire it beside `glide_check`). The lane's idle commit at A4 (lastNoteKey's
initial 69) before any note is harmless now: the first note re-arms it.

## ADR-159 — The morph field's array layout is frozen; late per-osc rows append last (2026-09-11)

**Context.** ADR-104 promised "morphIds is APPEND-ONLY (like param ids)" and
kept it for the FX, law, global and scale tails — each appended. The per-osc
PREFIX, however, is built by walking the param table, so a per-osc row added
to the table is inserted into the prefix. `oscPitch` (181, ADR-150,
2026-08-31) did that: 222 entries became 224 with the two new slots before
the tail, and every corner/exempt array saved between ADR-104 A2 (2026-08-21)
and 2026-08-31 has since been read two slots off in the tail — the bend and
note laws. `bendQTimeHz` at 8 landed on `bendQTime` via the slot shift and
a spring law acquired a 2-second chromatic step gate; the note lane parked
each new note at the previous pitch (the "second note plays the first note"
report) and the wheel lane transposed chords (B114 sharpened it). The
promise was a comment, not a test, and B110 unmasked it: `morphOn` used to
be truncated out of the preset load, so the misread corners never engaged.

**Decision.** (1) The prefix is frozen at the 2026-08-22 order: per-osc rows
added after it are listed in `kMorphLateIds` and APPENDED after every earlier
block, twin beside base. Adding a per-osc row now means adding it to that
list — a build-time act with a test behind it. (2) The writer stamps
`"morphLayout":2`. (3) The parser reads layout 1 (absent marker) arrays of
exactly 224 entries as the 2026-08-31 order and remaps them by id; every
other layout-1 array is the frozen prefix and maps 1:1 (shorter arrays fill
the prefix; the late slots take defaults). 224 is unambiguous because the
tail was complete (222) nine days before 181 arrived. (4) `morphlayout_check`
pins the frozen order and both remaps, with a control that reads the legacy
array as layout 2 and must land on the wrong id.

**Not done.** No re-save migration of the human's files: the remap runs on
every load, so the files stay as they are. Not a revision-gated law
(ADR-157): nothing rendered correctly under the misread.

**Consequences.** B110 was correct and stays; its side-effect was to reveal
this. The Ableton chunk path (`applyMorphChunk` is shared) heals the same
way. Lesson filed in REFLECTIONS: an append-only promise on a derived order
needs an oracle on the order, not a comment.

