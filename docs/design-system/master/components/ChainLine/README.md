# Chain line

Every tap of the chain on one line, so one picture answers "where is my headroom going?" The mixer lab's second view of gain staging (B225), drawn beside the Gain staging view (B544), reading the same stages and, since B547, setting the same gains.

**The dots are readings; the bars under the line are the controls.** A dot only shows the tap's measured peak hold (pointing at it names it); it never sets a gain. Each stage's gain is its own fixed bar in the gain lane below, in the dot's column. (B547 made the dots the gain handles; they move on their own, so it was unclear where a dragged one would end up. B549 / ADR-272 A1 replaced that.)

**Looks** A glass (`hd-viz`, 240 px tall, full width) with the gain lane under it. **x** is the chain in signal order, one slot per stage (the four sources side by side in the first column); **y** is each tap's **peak hold** in dBFS, −48 to +6 (gridlines at −6, −12, −24, −36, −48; below −48 a tap sits on the floor line). The **0 dBFS rail** is dashed in `alarm`, labelled, and is the only rail. Source lanes (OSC 1, OSC 2, SUB, NOISE) share the first column, side by side, each at its own level, and **fold into the next stage**; the line then runs through the filters, the engine, the make-up and the rack to the master. Links are `scr-meter` at half strength. A dot is:
- **solid** (`scr-meter`) where the engine feeds the tap;
- **hollow** (the glass ground, an `scr-meter` ring) where it is owed: nothing publishes a reading yet, so it sits on the floor line, never at a guess;
- **red** (`alarm` fill) where its clip lamp is latched over;
- wearing a 2 px `scr-marker` ring when it is the **first over** (the earliest latched sample; a tie goes to the most upstream stage), the same rule and the same stage Gain staging outlines;
- wearing a `scr-value` ring when it is the tap you are pointing at.

Column labels are short ("SOURCES", "F1 IN", "FX2 OUT", "MASTER") and read downwards when the slots are narrower than 40 px. A key under the line draws the same marks. A readout line names the pointed-at tap, its peak hold, fed or owed, and its lamp.

**It reads the same stages as Gain staging.** `ChainLine(el, { stages, sr, read, latch, fed?, lane?, gain?, onGain?, keys? })` takes `GainStaging.chain(mode)`'s list (or the consumer's own) and the same two callbacks, `read(id)` and `latch(id)`; the limiter's reduction stage (`kind: 'gr'`) is a reading in dB of reduction, not a level on this scale, so only Gain staging draws it. The two views advance by the same `dt` and are fed by one source, so they cannot disagree. The peak hold is Gain staging's own law (held 1.5 s, then 20 dB/s, per channel), repeated in this part because the other keeps its constants private: change them together.

**FX device (ADR-254)** The FX device has no sources, filters or make-up, so with `GainStaging.chain('fx')` the line **starts at the audio input** (`in`), runs through the four FX slots and the master. Nothing else changes.

**The gain lane (B549).** Under the glass, one **vertical offset bar** per stage that has a gain, in the same column as that stage's dot, so the eye links control to reading without either moving the other. A bar rises from a **0 dB baseline** for a boost and falls for a cut; its rail spans the stage's own gain range (`stage.gain`), its fill runs from the baseline to the handle (`value`), and the handle is the house one (`panel`, ink outline). The **dB value** rides beside the handle (above it for 0 or a boost, below it for a cut) on the well. All bars share **one dB scale**, the union of every stage's range with 0 (the instrument chain: −60 to +24 dB), so equal dB is equal height whatever a stage's own range; 0 dB and both scale ends are labelled in the gutter. The make-up's default +12.04 dB reads as a standing bar. A stage with no gain has an empty slot (the baseline runs on). The lane is the control look (ink outline, well), not a glass and not a meter colour.
- **Drag a bar up or down**, by the house drag scale (B449): 160 px is the stage's whole gain range, relative to where you grabbed (never a jump-to-click), **shift ×0.25**; **double-click** sets the stage's default; **↑ ↓** on a focused bar change the gain by 1 % of its range, **shift** 0.25 %, gated by `keys` (default on; `keys` off also takes the bars out of the tab order). Dragging a bar changes only that stage's gain and its fader.
- **A bar never moves by itself.** It reads the consumer's gain and moves only when you drag it or the gain moves (a Gain staging fader, a double-click); the levels animating do not touch it.
- **Hit width.** Every slot is at least 32 px wide (the house hit is 28), so a bar is never narrower than that. When the card is too narrow for 32 px slots (a phone: 20 slots need 691 px), **the line and its lane scroll sideways together** inside their box, so a dot and its bar stay in one column; the page does not scroll sideways. A line built without `gain`/`onGain` has no lane and shrinks to fit, as in B545.
- **Dots** (readings): point at a dot (or tap it) to name it; focus the line and use ← → to step along the chain. The readout also names the stage's gain.

**One gain model (B547, kept).** The line holds no gain of its own. `ChainLine(el, { …, gain(id), onGain(id, db) })`: `gain(id)` returns the stage's current gain in dB, `onGain(id, db)` is called on every change from a bar, and the ranges and defaults are the stage list's own (`stage.gain: { min, max, def }`, the same ones Gain staging's faders use, so both views move one value). The consumer owns the value, writes it where the audio side reads it, and calls Gain staging's `setGain(id, db)` so its fader shows it; Gain staging's own `onGain` writes the same value and the bar reads it on the next paint, so a fader moves its bar and a bar moves its fader.

**The consumer provides** the stage list; the gain model (above); per frame each stage's published peaks (`read(id)`: `{ l, r }` in dBFS, or `undefined` when the tap has no reading); the latch (`latch(id)`: `{ on, at, max }`, `at` a sample index); and optionally `fed(id)` (default: `read(id)` returned a reading) and `lane(stage)` (default: the `SOURCES` group); to show the gain lane, `gain(id)` and `onGain(id, db)`. The call returns `{ step(dt, quiet), render(), first(), probe(id), stages(list) }`; the component reads no clock (the peak holds run on the `dt` passed to `step`).

**Fed by** the same taps as Gain staging: RC-2 and RC-8, each source meter's and the output meter's peak and latch from the engine's snapshot (IF-6); the filters, the rack and the master stage supply their own. A tap whose reading is not yet published is hollow: that is the lab's FED and OWED badges, drawn on the dot instead of in a table.

**Voice-bound or audio-capable** Audio-capable: it reads levels, not notes, so the FX device uses it as it is.

**Screens** drawn in a glass, so it follows the Screen scheme: the grid, links, dots, labels and rings are the screen roles (`scr-ground`, `scr-meter`, `scr-grid`, `scr-value`, `scr-marker`), and the rail and the over dots are `alarm`. Scanlines and the scheme's alpha apply as in every glass.

**Reduced motion** nothing in it moves by itself: the peak holds advance by the `dt` the caller passes, and with reduced motion the preview runs the chain to 0.8 s (the first strike's peak held) and redraws only when a gain or the feed changes, exactly as Gain staging's does; a bar has no motion of its own in any case.

**Fits** the glass fills its column at a fixed height and draws at the display's pixel ratio (capped at 2); it repaints when its box or the theme changes. Slots narrower than 40 px turn their labels downwards. With the lane, slots never go below 32 px: on a narrow card the line and the lane scroll together (see the gain lane).

**Placeholders**
- The y range. The lab's −48 to +6 is kept; Gain staging's bars run −66 to +6. Decided by: the mixer panel's ruling (B402, RC-2 "deferred"), with the stage ranges.
- Which taps are owed. The component draws whatever `fed`/`read` say. The lab's FED / PART / OWED table was written against an older shell; which of RC-2's taps the build publishes today is the consumer's to report. Decided by: the engine's snapshot contract (IF-6).
- The lane's dB scale. It is the union of the stages' gain ranges with 0, so it widens or narrows when the mixer panel's ranges are ruled (most are placeholders, see Gain staging). Decided by: the mixer panel's ruling (B402).
- The limiter's reduction is not drawn on the line (it is a reduction, not a level). Decided by: a ruling if the human wants a bracket from LIM IN down to the output.

**Sources** `docs/design/mixer-lab.html`: the cluster "Gain staging — every tap on one line" :360-368; `drawStaging` :991-1031 (axis, rail, links, dots, rings, labels); `STAGES` :461-477 (the chain in topological order, the lane columns); `originStage` :1130 (first over). ADR-230 (3) and A1; ADR-254 and B435 (the FX device); the Gain staging and Meter components.

The preview draws SAMPLE levels generated in the preview (a seeded hot chord, the same one Gain staging's preview uses, run through the stages with the gains the controls hold), never the engine; Gain staging sits under the line in the preview and shares its engine, so clearing a lamp in either place clears it in both, and dragging any bar moves that stage's fader and its level. **Sources + output fed** is a preview state that leaves the other taps without a reading.
