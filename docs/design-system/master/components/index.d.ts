// horde house components: the reference bundle's API (vanilla mount functions, not React).
// Documentation only. In GUI 3 the engine supplies ownership, weights and source values.

/** A generated manifest key, e.g. "env1.attack" (B308 L2). Never a display name or a position. */
export type ManifestKey = string;
export type CornerIndex = 0 | 1 | 2 | 3;              // A ◆, B ▲, C ●, D ■
export type SourceKind = 'env' | 'lfo' | 'macro' | 'engine';
/** UNI, BI and INV add to the parameter; CTL (Control, ADR-266 r2) drives its position between lo and hi, ignoring its own value. */
export type Polarity = 'uni' | 'bi' | 'inv' | 'ctl';

export interface Source { id: string; name: string; kind: SourceKind; /** raw value in [0, 1] at time t (s) */ f(t: number): number }
export interface Mapping { param: ManifestKey; src: string; pol: Polarity; /** global depth 0..1, a magnitude (the polarity carries the sign) */ depth: number; /** corner-level depths A..D */ cd?: [number, number, number, number];
  /** Control only: the low and high ends the source sweeps, 0..1 */ lo?: number; hi?: number }

export interface SessionOptions { x?: number; /** measured downward */ y?: number; sources?: Source[]; mappings?: Mapping[]; keys?: boolean;
  /** the ACTUAL names of the four corner presets A..D; null = an empty corner */ corners?: [string | null, string | null, string | null, string | null] }
/** One morph cursor, one MAP state, one keyboard mode, shared by every control on a page. */
export declare class Session {
  constructor(opts?: SessionOptions);
  pad: { x: number; y: number };
  /** null = Auto (each parameter's owning corner) */
  pinned: CornerIndex | null;
  map: { on: boolean; src: string; momentary: boolean };
  /** keyboard mode: hotkeys and arrows act only while true */
  keys: boolean;
  weights(): [number, number, number, number];
  /** the parameter's Control mapping, if any (at most one) */
  ctlOf(key: ManifestKey): Mapping | null;
  /** where additive mappings start: the Control position, else the parameter's own blended value */
  origin(p: KnobProps): number;
  /** the output: origin + every UNI/BI/INV contribution, clamped once */
  output(p: KnobProps): number;
  /** a mapping's amount as text: "30%", or a Control's "30–80%" */
  amount(m: Mapping): string;
  /** UNI → BI → INV → CTL; CTL skipped while another mapping controls the parameter */
  cyclePol(m: Mapping): void;
  /** per-corner lock: a locked corner refuses edits and still morphs */
  locks: [boolean, boolean, boolean, boolean];
  toggleLock(i: CornerIndex): void;
  /** true when the corner's values differ from its saved preset */
  edited(i: CornerIndex): boolean;
  /** return one corner to its saved preset */
  revert(i: CornerIndex): void;
  /** each corner's share of the parameters it currently owns */
  census(): [number, number, number, number] | null;
  setPin(i: CornerIndex | null): void;
  setMap(on: boolean, momentary?: boolean): void;
  setSrc(id: string): void;
  /** the map keys (M, 1–9 under MAP, P, Backspace under MAP) and Escape act always, not only in keyboard mode (ADR-335 r1; GUI 3 sets it from its one constant) */
  mapKeysAlways: boolean;
  /** the layered Escape (ADR-338 r11): menu-like layers a host unshifts, then map, pin, keys; Panic is on no key (ADR-340 r12) */
  escapeLayers: { name: string; active(): boolean; cancel(): void }[];
  /** cancel the first active layer; its name, or null when nothing was open */
  escape(): string | null;
  start(): void;
}

export interface KnobProps {
  key: ManifestKey;
  name: string;
  /** accessible name: module path plus parameter, e.g. "Filter · Cutoff" */
  path?: string;
  /** the shell's formatter for this key's unit */
  fmt?: (v: number) => string;
  /** stored value per corner A..D (normalised 0..1) */
  vals?: [number, number, number, number];
  /** one global value instead of four corners */
  locked?: boolean;
  g?: number;
  def?: number;
  bipolar?: boolean;
  /** the classic box only (no size): 84 px instead of 68 */
  big?: boolean;
  /** B589, the Density ladder: a sized control draws the tight geometry, its label ABOVE on one line, its value on hover / drag /
      focus in the label's place (primary: always, under the face), and no readout cell; the corner line and the full focus caption
      go to the section's ReadoutSlot. Faces: primary 56, standard 48, compact 40 (the default inside a dense panel), utility 28
      (label inline, one ring). Omitted: the classic 68 px box with its 96 px two-line readout cell, unchanged. */
  size?: ControlSize;
  /** the tiny tier's label (ADR-338 r10: layout.json's abbr column, at most 4 characters); the full path stays in the title */
  abbr?: string;
  /** tiny knob: false draws no label line (the cell is the 28 px hit box only) */
  label?: boolean;
  /** tiny slider: the drawn track's width in px (TS1a 64, TS2a 76 by default) */
  width?: number;
}
/** 'tiny' is the tiny tier (ADR-338 r1/r3, drawn as ADR-340 r1/r2): a knob draws T2d, a 20 px face in a 28 px hit cell with a
    see-through editing-corner pip between ink end stops; a slider draws TS1a, a 20 px row. 'cell' (slider only) is TS2a, a 16 px
    table value cell. The tiny tier shows no value in the cell: the label line never changes (ADR-340 r4, H1). */
export type ControlSize = 'primary' | 'standard' | 'compact' | 'utility' | 'tiny' | 'cell';
/** A sized slider (any `size`) draws 200 × 35 (vertical 40 × 200, a 22 × 8 cap, `--fader-h` / size-fader-h tall) with a value-only
    readout column; `utility` draws as compact. */
export interface SliderProps extends KnobProps { vert?: boolean }

export declare function Knob(el: HTMLElement, props: KnobProps, session: Session): { render(): void };
export interface ReadoutSlotProps { /** controls inside this element feed the slot (default: the slot's parent) */ scope?: HTMLElement;
  /** what the slot reads at rest */ summary?: string | (() => string) }
/** The section's one live readout (B589): the HOT control's (pointer, drag or keyboard focus, inside `scope`) name, value and the
    classic second line (corner glyph, letter, stored value and weight; the full focus caption under Option or MAP; "controlled by";
    LOCKED); otherwise the summary. aria-hidden: each control's own aria-valuetext carries the same text. */
export declare function ReadoutSlot(el: HTMLElement, session: Session, props?: ReadoutSlotProps): { render(): void };
export declare function Slider(el: HTMLElement, props: SliderProps, session: Session): { render(): void };
export interface MapControlProps { strip?: HTMLElement; frame?: HTMLElement }
export declare function MapControl(el: HTMLElement, session: Session, props?: MapControlProps): { render(): void };
export declare function ModInspector(el: HTMLElement, session: Session): void;
export interface XYPadProps { size?: number; /** only a grab on the puck moves it, relatively; an empty-pad click moves nothing (the morph editor's pad) */ relative?: boolean; /** show the optional census ring (off by default) */ census?: boolean; /** census ring dot count (12) */ dots?: number; /** dot diameter in px */ dot?: number }
export declare function XYPad(el: HTMLElement, session: Session, props?: XYPadProps): { census(on: boolean): void };
export declare function CornerPin(el: HTMLElement, session: Session): void;
export declare function CornerChips(el: HTMLElement, session: Session): void;
export declare function CornerInspector(el: HTMLElement, session: Session): void;
export type SchemeId = 'tube' | 'orchid' | 'frost' | 'ember' | 'dusk' | 'lumen' | 'paper';
export interface ScreenSchemeProps { /** null follows the chassis theme */ scheme?: SchemeId | null; list?: boolean; onchange?: (id: SchemeId | null) => void }
export declare function ScreenScheme(el: HTMLElement, target: HTMLElement, props?: ScreenSchemeProps): { set(id: SchemeId | null): void };
export interface GlideParams { law: 0 | 1 | 2 | 3 | 4; time: number; rate: number; lag: number; springF: number; damp: number; distOver: number; retMul: number;
  quant: 0 | 1 | 2; root: number; mask: number[]; hyst: number; qTime: number }
/** the bend lab's travel law at 2756 ticks a second: the law's path x and the emitted pitch y, plus lag50 / over / settle / rings */
export declare const Glide: { LAWS: string[]; SCALES: [string, number[]][]; simulate(p: GlideParams, from: number, to: number, seconds: number, lane: 'bend' | 'note'): any };
export declare function ScalePicker(el: HTMLElement, props?: { root?: number; mask?: number[]; skin?: 'global' | 'offset'; /** the GLOBAL visual */ visual?: 'circles' | 'piano' | 'ring' | 'steps'; /** show the visual switcher */ variants?: boolean; onChange?: (s: { root: number; mask: number[] }) => void }): { get(): { root: number; mask: number[] }; set(root: number, mask: number[]): void };
export interface StepperProps { name?: string; path?: string; /** named options (their index is the value) */ options?: string[];
  min?: number; max?: number; step?: number; value?: number; /** home: double-click returns here */ def?: number; fmt?(v: number): string;
  onInput?(v: number): void; say?(msg: string): void; /** TC2's table form (K4): no arrows, a dotted underline */ flat?: boolean;
  /** the dense 20 px face */ sm?: boolean; /** keyboard mode (arrows, PgUp/PgDn, Home/End, Enter/F2) */ keys?(): boolean }
/** The house stepper (ADR-340 r5): drag the value (6 px a step, Shift 24, re-anchored at the ends), click the arrows, double-click
    for the default, Option-click to type (Enter/Tab commit, Escape cancels, a click away commits; out of range clamps and says so). */
export declare function Stepper(el: HTMLElement, props: StepperProps): { get(): number; set(v: number): void; open(): void; paint(): void };
export declare function PlainSlider(el: HTMLElement, props: { name: string; path?: string; min: number; max: number; value: number; def?: number; log?: boolean; fmt(v: number): string; onInput?(v: number): void }): { get(): number };
/** the visualizer painters: each draws one frame into a canvas with the screen roles only */
export declare const Viz: { kuramoto: KuramotoRingPainter; scope: Function; spectrum: Function; carpet: Function; carpetV: Function; blade: Function; envelope: Function; rotor: Function; voiceMap: Function; gonio: Function; bend: Function };
/** largest-remainder apportionment: integer counts summing exactly to n */
export declare function apportion(share: number[], n: number): number[];

// ===== Modulation: the mod matrix family (ADR-266 r1–r4) =====
/** The ONE live source strip (the matrix's and the MAP control's): kind colour, name, number, live meter; a click selects. */
export declare function SourceStrip(el: HTMLElement, session: Session, props?: { counts?: boolean; label?: string; onPick?(id: string): void }): { el: HTMLElement; sync(): void; render(): void };
/** Sources × destinations: depth fill, the live "now" lane, a dashed ring on a modulated depth; CTL cells show the swept band. */
export declare function ModMatrixGrid(el: HTMLElement, session: Session, props?: object): void;
/** One route: source, the four-state polarity pill, destination, depth (or a CTL low/high range), authored → now, remove; depth routes indented. */
export declare function RouteRow(el: HTMLElement, session: Session, m: Mapping, props?: object): void;
export declare function RouteList(el: HTMLElement, session: Session, props?: object): void;
/** "What is moving it": per destination, each source's share now, and the sum. */
export declare function ModLedger(el: HTMLElement, session: Session, props?: object): void;
/** A screen: per destination, the reach band, start line and now trace. */
export declare function TraceLanes(el: HTMLElement, session: Session, props?: object): void;
/** A screen: the routes in use as compact wires (width = depth, brightness = source now). */
export declare function CompactWires(el: HTMLElement, session: Session, props?: object): void;
/** A hosted module's macros: visible, read-only, never cross-mapped (ADR-266 r3). */
export declare function HostedMacroRows(el: HTMLElement, session: Session, props?: { module?: string; macros?: { name: string; f(t: number): number; targets: string[] }[] }): void;
/** The matrix arithmetic shared by the views: push, now, raw, reach, ctlPos, depthNow … */
export declare const Matrix: Record<string, unknown>;

// ===== Modulators (ADR-266 r6–r7) =====
export interface BreakPoint { /** cycles (LFO) or seconds (envelope) */ t: number; v: number;
  /** the segment's bend in the ENGINE's unit (SPEC-ENGINE C.12): u in -2.5..+2.5, c = 4u; 0 straight, +1 the decay/release default,
      +2.30 the bank exponential. From the lab's c: u = (ln 100 / 2)·c. */ u: number }
type Keys = boolean | (() => boolean);
export interface ModCardHeaderProps { name: string; kind: 'lfo' | 'env' | 'macro' | 'engine'; kindLabel?: string; scope?: string;
  /** true/false shows the retrigger switch (ADR-266 r7); null leaves it out */ retrig: boolean | null; retrigHint?: string; onRetrig?(on: boolean): void; onCommit?(s: { retrig: boolean }): void }
export interface RateSyncState { sync: 0 | 1; hz: number; beats: number; /** effective Hz */ rate: number; name: string }
export interface RateSyncProps { name?: string; path?: string; sync?: 0 | 1; hz?: number; beats?: number; bpm?: number | (() => number);
  min?: number; max?: number; unit?: string; defHz?: number; defBeats?: number; keys?: Keys; onChange?(s: RateSyncState): void; onCommit?(s: RateSyncState): void }
export interface TempoDivisionsProps { beats?: number; bpm?: number | (() => number); path?: string; onChange?(beats: number): void; onCommit?(beats: number): void }
export interface LfoShapeEditorProps { name?: string; pts?: BreakPoint[]; /** 0 DRAWN, 1 S&H, 2 RANDOM */ kind?: 0 | 1 | 2; /** NONE LINEAR COSINE CUBIC SLEW */ smooth?: 0 | 1 | 2 | 3 | 4;
  slewMs?: number; seed?: number; phase?: number; /** acc + phase, unwrapped, cycles */ position?(): number | null; cycleSec?(): number; rateLabel?(): string; keys?: Keys; aspect?: string;
  onChange?(s: object): void; /** once per gesture */ onCommit?(s: object): void }
export interface EnvelopeEditorProps { name?: string; adsr?: { a: number; d: number; s: number; r: number }; pts?: BreakPoint[]; /** -1 none (one-shot) */ sus?: number;
  voices?(): { t: number; tOff: number | null; v0?: number }[]; keys?: Keys; aspect?: string; onChange?(s: object): void; onCommit?(s: object): void }
export interface CurvePlotProps { T: number | (() => number); lines: { f(t: number): number; role?: string; glow?: boolean; dash?: number[]; a?: number; lw?: number }[] | (() => object[]);
  marks?: { t: number; label: string }[] | (() => object[]); rings?: { t: number; v: number }[] | (() => object[]); caption?: string | (() => string); xFmt?(t: number): string; label?: string; aspect?: string }
export interface OrbitalMapping { b: number; /** 0 x, 1 y, 2 speed, 3 near, 4 angle */ q: number; dest: string; mode: 'cont' | 'sh'; sync?: 0 | 1; hz?: number; beats?: number }
export interface OrbitalCardProps { field: object; maps: OrbitalMapping[]; read(i: number): { src: number; out: number; tick: number | boolean }; bpm?: number | (() => number);
  name?: string; retrig?: boolean; onRetrig?(on: boolean): void; onChange?(what: string, v?: unknown): void; onCommit?(what: string, v?: unknown): void; keys?: Keys }
export interface MidiTrackersProps { cfg?: { mode: 0 | 1; orderN: number; sinceT: number }; read?(): { values: Record<string, number>; fired: number }; keys?: Keys; onChange?(cfg: object): void; onCommit?(cfg: object): void }
export interface LaneRow { name: string; kind: 'level' | 'step' | 'event'; bip?: boolean; key?: string; role?: string }
export interface GateLanesProps { rows?: LaneRow[]; points?: number; label?: string; aspect?: string }
export interface EventLanesProps { act?: (0 | 1)[]; inputs?: [string, string][]; shIn?: string; seed?: number; onInput?(id: string): void; onSeed?(s: number): void; onChange?(act: number[]): void; onCommit?(act: number[]): void }
export interface SmoothingLanesProps { mode?: number; seed?: number | (() => number); position?(): number; cycleSec?(): number; slewMs?: number | (() => number); onPick?(mode: number): void; onCommit?(mode: number): void }
export interface ShClockCardProps { /** 0 CYCLE (ruled), 1 GLOBAL, 2 KURO */ clock?: 0 | 1 | 2; bind?: number; sync?: 0 | 1; hz?: number; beats?: number; bpm?: number | (() => number); retrig?: boolean; keys?: Keys;
  onRetrig?(on: boolean): void; onChange?(s: object): void; onCommit?(s: object): void }
export interface OwnershipTierProps { rows: { id: string; kind: 'lfo' | 'env' | 'macro' | 'engine'; tier: 'baked' | 'derived' | 'mod'; from: string }[]; onChange?(rows: object[]): void; onCommit?(rows: object[]): void; onRefuse?(row: object): void }
/** Horde.ModLaw: segCurve(x, u), uFromLab(c), fmtU(u), U_MAX, U_BANK, U_ONEPOLE, U_DEF {attack 0, decay 1, release 1}, shapeAt, envAt, envTauAt, adsrPts, DIVS, nearestDiv, divName, m32at, drawV, randAt, slewAt, randPage, randValue, lfoSeedOf …
    Horde.ModSlider(el, props): the PlainSlider row driven by the house drag rule (160 px full range).
    Horde.Synth: Orbital, SampleHold, Tracker, ModBench, PHRASE (seeded preview stand-ins). */

export declare function ModCardHeader(el: HTMLElement, props: ModCardHeaderProps): unknown;
export declare function RateSync(el: HTMLElement, props?: RateSyncProps): unknown;
export declare function TempoDivisions(el: HTMLElement, props?: TempoDivisionsProps): unknown;
export declare function LfoShapeEditor(el: HTMLElement, props?: LfoShapeEditorProps): unknown;
export declare function EnvelopeEditor(el: HTMLElement, props?: EnvelopeEditorProps): unknown;
export declare function CurvePlot(el: HTMLElement, props: CurvePlotProps): unknown;
export declare function OrbitalCard(el: HTMLElement, props: OrbitalCardProps): unknown;
export declare function MidiTrackers(el: HTMLElement, props?: MidiTrackersProps): unknown;
export declare function GateLanes(el: HTMLElement, props?: GateLanesProps): unknown;
export declare function EventLanes(el: HTMLElement, props?: EventLanesProps): unknown;
export declare function SmoothingLanes(el: HTMLElement, props?: SmoothingLanesProps): unknown;
export declare function ShClockCard(el: HTMLElement, props?: ShClockCardProps): unknown;
export declare function OwnershipTier(el: HTMLElement, props: OwnershipTierProps): unknown;

// ===== Morph editor (ADR-266 r9–r11) =====
/** Quantum regions and boundary drag, layered on a core XYPad element (pass XYPad { relative: true }). */
export declare function QuantumPad(padEl: HTMLElement, session: Session, props?: { key?: ManifestKey }): void;
export declare function QuantumBanner(el: HTMLElement, session: Session): void;
export declare function LockBar(el: HTMLElement, session: Session, props?: object): void;
export declare function MorphScaleBar(el: HTMLElement, session: Session, props?: object): void;
/** The heard value along a chord across the pad (two 28 px handles on the pad aim it). */
export declare function CrossSection(el: HTMLElement, session: Session, props?: { pad?: HTMLElement }): void;
/** The grid: section headers, row-kind chips, cells as SLIDES / SNAPS / RAMPS; edits live, latched per gesture, one undo step each. */
export declare function MorphGrid(el: HTMLElement, session: Session, props?: object): unknown;
export declare function FindPane(el: HTMLElement, session: Session, grid: unknown): void;
export declare function MorphInspector(el: HTMLElement, session: Session): void;
export declare function KindLegend(el: HTMLElement, session: Session, props?: object): void;
export declare function MorphMode(el: HTMLElement, session: Session): void;
/** STEPPED as rate plus sync (H.RateSync), with glide. */
/** props.size 'compact' (B589): dense pill and stepper faces, 52 px slider columns, a 7 : 1 scope */
export declare function SteppedControl(el: HTMLElement, session: Session, props?: { glide?: boolean; size?: 'compact' }): void;
/** ONE field-wide cohesion control. */
export declare function Cohesion(el: HTMLElement, session: Session, props?: object): void;
/** Shared morph state (session.morph) and the labelled engine stand-in. */
export declare const Morph: Record<string, unknown>;

// ===== FX chain (ADR-266 r5) =====
export interface FxState { lanes: { id: string; name: string; short: string; style: 'lane' | 'lane2' }[]; nodes: { id: string; name: string; short: string; style: string; asleep: boolean; cost?: number; tag?: string }[];
  out: { id: string; name: string; short: string }; cables: { u: string; v: string; wf: number; wb: number; loop: boolean; held: boolean }[]; rules?: Record<string, 'banned' | string> }
export declare function FxRack(el: HTMLElement, get: () => FxState | null, session?: Session): { render(force?: boolean): void; renders: number };
export declare function FxMatrix(el: HTMLElement, get: () => FxState | null, session?: Session): { render(force?: boolean): void; renders: number };
export declare function FxPanel(el: HTMLElement, get: () => FxState | null, session?: Session): { render(force?: boolean): void; renders: number };
/** The cable encoding shared by the three views: weight, the dashed one-sample-late backward cable, the loop halo, wait dots; module styles (a cream card plus a screen and a cable colour: fxs-<module>-ground|ink|mid|cable, ADR-273). */
export declare const FxCables: { MIN: number; STYLES: Record<string, unknown>; /** each module's display style and whether it is still a stand-in (ADR-273) */ SCREENS: Record<string, { name: string; standIn: boolean }>; styleOf(id: string): unknown; width(w: number): number; alpha(w: number): number; legend(el: HTMLElement, props: object): void; [k: string]: unknown };
/** The previews' deterministic stand-in for the chain's morph (never the engine). */
export declare class FxSample { tau: number[]; strategy: 'a' | 'b'; target(w4: number[]): unknown; settle(): void; step(w4: number[], dt: number): void; land(): void; state(): FxState }

// ===== The phase ring and the gain-staging view (B544) =====
/** What Viz.kuramoto draws: the SCALPEL ring. Coherence is computed from `phase`, so needle, readout and R bar share one value and one ink (B323). */
export interface KuramotoRingState {
  /** each member's phase in cycles */ phase: number[];
  /** per member: blade 1 and blade 2 windows as start and width in cycles; a width of 0 is off (rim arcs, and the fill while inside) */ blades?: { st1: number; w1: number; st2: number; w2: number }[];
  /** per member: the RMS that member i+1's push adds to member i (arrow weight); null = cross-mod off; below 1e-6 draws as the dotted "no effect" hairline */ cross?: number[] | null;
  /** the XY point, 0..1 across the whole canvas: x = Detune, y = Coupling (0.5 = K 0); omit to draw the ring alone */ xy?: { x: number; y: number };
  /** true while the pointer holds the point (a thicker puck) */ drag?: boolean }
/** Horde.Viz.kuramoto(canvas, state) returns the Coherence it drew. */
export type KuramotoRingPainter = (cv: HTMLCanvasElement, s: KuramotoRingState) => number;
export interface RingXYState { /** Detune, 0..1 */ x: number; /** Coupling, 0..1 (0.5 = K 0) */ y: number; /** Cross-mod, 0..1: the third axis, shift + horizontal, relative */ sx: number }
/** The ring's gesture (B295): a click lands x and y at the clicked point, the drag follows, alt is fine (1/4), shift drags sx relative and lands nothing. Writes `st` in place. */
export declare function RingXY(cv: HTMLCanvasElement, st: RingXYState, on?: (st: RingXYState, info: { drag: boolean; shift: boolean; hover: boolean }) => void): { dragging(): boolean };
export interface GainRange { min: number; max: number; def: number; /** decimals shown, default 1 */ dec?: number; /** a value at or below this reads "−∞ dB" */ floor?: number; /** also name the factor (the make-up's ×4.00) */ x?: boolean; name?: string }
export interface GainStage { id: string; /** the card it sits in; adjacent stages with one group share a card */ group: string; name: string; /** null = no gain control (the limiter's reduction) */ gain: GainRange | null;
  /** 'level' has a meter pair and a clip lamp; 'gr' is the limiter's reduction, a one-bar read-out that grows down */ kind: 'level' | 'gr'; title: string }
export interface GainStagingProps { /** the chain in signal order */ stages: GainStage[]; /** sample rate, to turn a latch's sample index into seconds */ sr?: number;
  /** the stage's peaks since the previous frame in dBFS ({ l, r }), or { gr } dB of reduction; undefined draws as silent */ read(id: string): { l?: number; r?: number; gr?: number } | undefined;
  /** the latch, kept on the audio side: whether one sample went over 0 dBFS, at which sample, and the highest level */ latch(id: string): { on: boolean; at?: number; max?: number };
  clear(id: string): void; onGain?(id: string, db: number): void; /** initial gains by stage id, in dB (default: each stage's own default) */ gains?: Record<string, number>; keys?: Keys }
/** The gain-staging view (ADR-230 (3), A1; mixer lab B225): a gain control, a Meter-drawn level pair with peak hold and a latching clip lamp, and the first-over marker at every stage.
    step(dt, quiet) advances the ballistics by dt seconds (quiet skips painting); the component reads no clock. */
export declare function GainStaging(el: HTMLElement, props: GainStagingProps): { step(dt: number, quiet?: boolean): void; render(): void; first(): string | null; stages(s: GainStage[]): void; gain(id: string): number | null; setGain(id: string, db: number): void };
export declare namespace GainStaging { /** The reference's chain: 'instrument' (sources, filters, engine, make-up, FX slots, master) or 'fx' (audio in, FX slots, master: ADR-254) */ function chain(mode: 'instrument' | 'fx'): GainStage[] }
/** Horde.Synth.ring(seed, n): the ring preview's seeded swarm: phase[], blades[], step(dt, K, spread), cross(xm). */

// ===== The chain line and the blend maps (B545) =====
export interface ChainLineProps { /** the chain in signal order: GainStaging.chain's list, or the consumer's; 'gr' stages are not drawn (a reduction, not a level) */ stages: GainStage[]; sr?: number;
  /** the same callback Gain staging takes: peaks since the previous frame in dBFS, or undefined when the tap has no reading (hollow, on the floor line) */ read(id: string): { l?: number; r?: number; gr?: number } | undefined;
  /** the same latch Gain staging takes */ latch(id: string): { on: boolean; at?: number; max?: number };
  /** the engine publishes this tap (solid); default: read(id) returned a reading. A tap that is not fed is hollow. */ fed?(id: string): boolean;
  /** stages that run in parallel and fold into the next one (default: group 'SOURCES') */ lane?(stage: GainStage): boolean;
  /** B547, B549: the consumer's ONE gain model (the one Gain staging's faders also read and write): the stage's current gain in dB, read on every paint. With onGain it gives every stage that has a `gain` range a vertical offset bar in the gain lane under the line. The dots are readings and never set a gain. */ gain?(id: string): number | null | undefined;
  /** B549: called on every gain change from a bar (drag: 160 px = the stage's whole range, shift x0.25; double-click: the stage's default; up/down on a focused bar: 1 %, shift 0.25 %). The consumer writes it to the model and calls GainStaging's setGain so the fader shows it. A stage whose `gain` is null gets an empty slot. Without gain/onGain there is no lane. */ onGain?(id: string, db: number): void;
  /** gates the bars' up/down keys (and their tab stops) like Gain staging's faders; default true */ keys?: Keys }
/** The chain line (mixer lab, "Gain staging — every tap on one line"): x the chain in signal order, y each tap's peak hold in dBFS (-48 to +6), the 0 dBFS rail dashed in alarm; solid dot = fed, hollow = owed, red = latched over, marker ring = first over. The dots are read-only.
    step(dt, quiet) advances the peak holds (1.5 s, then 20 dB/s, Gain staging's law); the component reads no clock.
    B549: with gain/onGain a gain lane sits under the line, one vertical offset bar per stage from 0 dB in its dot's column, on one dB scale (the union of the stage ranges); slots are at least 32 px, so a narrow card scrolls the line and the lane together. */
export declare function ChainLine(el: HTMLElement, props: ChainLineProps): { step(dt: number, quiet?: boolean): void; render(): void; first(): string | null; probe(id?: string): string | null | void; stages(s: GainStage[]): void };
/** Horde.Viz.chainLine(canvas, state): the painter. taps: id, label, col, db (null = no reading), fed, over, first, probe; one slot per tap in order, so tap k's dot is at the centre of slot k (the lane puts its bar there); links: [from, to] tap indices. Records the hit list (id, x, y) and the pointing radius on canvas.__line. */
export type BlendLaw = 'tps' | 'tind' | 'tin' | 'idw';
export interface BlendRow { name: string; min: number; max: number; /** value text; default is the lab's decimals by range */ fmt?(v: number): string;
  /** the four corner values A, B, C, D (A top left, B top right, C bottom left, D bottom right of the pad) */ corners: [number, number, number, number];
  /** pins [x, y, value] in pad units 0..1 and STORED ORDER (it breaks the triangulation's ties and fixes the TPS solve's rounding); at most 8 */ pins?: [number, number, number][] }
export interface BlendMapsProps { row: BlendRow; /** the setting's laws, B272: ['tps', 'tind'] (default); the first is the default law */ laws?: BlendLaw[]; /** the setting (default: the first of laws) */ law?: BlendLaw;
  /** CLAMP TO SET VALUES (default true) */ clamp?: boolean; /** extra laws drawn only as labelled comparisons, never selectable (a preview passes ['tin', 'idw']) */ compare?: BlendLaw[]; maps?: 'value' | 'diff';
  onChange?(info: { type: 'law' | 'clamp' | 'move' | 'value' | 'add' | 'delete'; law: BlendLaw; clamp: boolean; pins: [number, number, number][]; selected: number }): void }
/** The blend maps (morph editor lab, Waypoints pane, B235 / B268 / B272): one row's value across the pad per law with contours, pins, corners, TIN's dashed triangulation, the clamp to the set values and a cross-section. */
export declare function BlendMaps(el: HTMLElement, props: BlendMapsProps): { law(): BlendLaw; setLaw(l: BlendLaw): void; clamp(): boolean; setClamp(b: boolean): void; row(): BlendRow; setRow(r: BlendRow): void; line(a: [number, number], b: [number, number]): void;
  /** a law's share of the pad clipped at the range and outside the set values, and the peak past them as a fraction of the range */ stats(law?: BlendLaw): { range: number; set: number; peak: number }; value(law: BlendLaw, x: number, y: number): number; render(): void };
export declare namespace BlendMaps { /** the pure law functions the pictures run: makeSurface(corners, pins, law), surfAt(surface, x, y, min, max, clamp) -> { v, raw, clip }, triangulate, weights */
  const math: { surface(c: number[], pins: number[][], law: BlendLaw): { base(x: number, y: number): number; raw(x: number, y: number): number; tri: number[][] | null; P: number[][]; lo: number; hi: number; n: number; law: BlendLaw; terms: number };
    at(s: unknown, x: number, y: number, min: number, max: number, clamp: boolean): { v: number; raw: number; clip: number }; triangulate(P: number[][]): number[][]; weights(x: number, y: number): number[]; laws: BlendLaw[]; name: Record<BlendLaw, string> } }

declare global { interface Window { Horde: { Session: typeof Session; Knob: typeof Knob; Slider: typeof Slider; MapControl: typeof MapControl; ModInspector: typeof ModInspector; XYPad: typeof XYPad; CornerPin: typeof CornerPin; CornerChips: typeof CornerChips; CornerInspector: typeof CornerInspector; ScreenScheme: typeof ScreenScheme; [component: string]: unknown } } }

// ===== Density and hierarchy (B589: the human's notes on the GUI 3 shell, 2026-10-30) =====
export interface PageRef { id: string; label: string; /** an oscillator page's power, read from the manifest: draws the tab's dot */ on?: boolean }
export interface PageGroup { id: string; label: string; /** omitted: a one-page group (the group itself) */ pages?: PageRef[] }
export interface PageNavProps { groups: PageGroup[]; value?: { group: string; page?: string | null };
  /** fired on a user's pick; a group restores the page it was last on */ onChange?(group: string, page: string): void;
  /** the page area's element id, for the page tabs' aria-controls */ panel?: string; label?: string; keys?: boolean | (() => boolean) }
/** The two-level page bar: groups (ink-inverted pills) over the selected group's pages (underlined tabs); two role="tablist" rows,
    focusable only in keyboard mode (roving; Left/Right move and select, Down/Up between the rows). */
export declare function PageNav(el: HTMLElement, props: PageNavProps): { set(group: string, page?: string): void; get(): { group: string; page: string }; refresh(): void;
  /** update a page tab's on-dot (an oscillator switched elsewhere) */ mark(group: string, page: string, on: boolean): void };
export interface SectionHeaderProps { name: string; /** accessible prefix, "Osc 1 · Blade 2" */ path?: string; /** omit: no power toggle */ on?: boolean; onToggle?(on: boolean): void;
  /** repeated sections (blades 1, 2) */ num?: number | string; /** the identity rule's pattern (ink, never a colour): default 'solid' */ rule?: 'solid' | 'dash' | 'dot' | 'none';
  /** the section's main enum, as the manifest lists its options */ modes?: { options: string[]; value?: number; label?: string; onPick?(i: number): void };
  /** with S, the header carries a ReadoutSlot reading the controls inside `section` */ S?: Session; section?: HTMLElement; scope?: HTMLElement;
  summary?: string | (() => string); /** an element placed before the slot (a small action) */ end?: HTMLElement; keys?: boolean | (() => boolean) }
/** The one-line section header: identity rule, on toggle, number, name, mode pills, readout slot. Off dims the section's body. */
export declare function SectionHeader(el: HTMLElement, props: SectionHeaderProps): { on(b?: boolean): boolean | void; mode(i?: number): number | null | void; el: HTMLElement };
export interface ChannelStripDef { id: string; name: string; path?: string; /** the level's morph slider props (vertical and compact are set for you) */ fader: KnobProps;
  /** present: the strip has an on toggle */ on?: boolean; mute?: boolean; solo?: boolean; /** the output: no on, mute or solo; never silenced by a solo */ master?: boolean }
export interface ChannelStripProps { strips: ChannelStripDef[]; label?: string;
  /** peaks since the previous frame, dBFS, published by the engine */ read?(id: string): { l?: number; r?: number } | undefined;
  /** the clip latch, which lives on the audio side */ latch?(id: string): { on: boolean; max?: number } | undefined; clear?(id: string): void;
  onToggle?(id: string, what: 'on' | 'mute' | 'solo', value: boolean): void; /** px, default size-fader-h */ faderH?: number; keys?: boolean | (() => boolean) }
/** The mixer as a console: strips side by side, each on + name, peak readout + latching CLIP lamp, the fader beside its meter pair,
    the fader's value, mute and solo; the master at the right. step(dt) advances the bars (0.11 s fall) and holds (1.5 s, then 20 dB/s). */
export declare function ChannelStrip(el: HTMLElement, session: Session, props: ChannelStripProps): { step(dt: number): void; render(): void; refresh(): void;
  set(id: string, what: 'on' | 'mute' | 'solo', value: boolean): void; state(): { id: string; on: boolean; mute: boolean; solo: boolean; peak: number }[] };
/** compact pills as a radiogroup (Section header's modes, a blade's mirror) */
export declare function pillGroup(host: HTMLElement, labels: string[], value: number, label: string, onPick?: (i: number) => void, props?: { keys?: boolean | (() => boolean) }): { set(i: number): void; get(): number; buttons: HTMLButtonElement[] };
