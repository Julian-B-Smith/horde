# b295-scalpel-lab-round4 — the ring XY snaps to the click; the phase carpet and voice map return

- **Queue item:** B295. The row is carried in records PR #779 (branch `lead-records-104`), and I read it verbatim from `origin/lead-records-104:ROADMAP.md`. The human, 2026-09-27: "please make the XY point on the phase circle snap to where the mouse clicks instead of staying put and dragging. Also can we add back in the phase carpet and voice map?"
- **Why:** Round 4 of B271's lab, `docs/design/scalpel-interface-lab.html`. It answers exactly those two asks. The ring's absolute behaviour reverses B293/B294's "a grab moves nothing" for the ring only. The waveform XY stays relative, because its x axis is phase.
- **What changed:** one file, the lab (`lab-review` meta `B271 + B293–B295 · 2026-09-27`), plus `docs/design/index.html` regenerated.
  1. **The ring is absolute** (`absGrab` / `absStep` in section I2; `RXY.abs`).
     - **The model.** Each ↔/↕ key is `hand + off[k]` in taper position, clamped, or wrapped when circular. The hand is the pointer across the canvas's content box. An inverted second reads `1 − hand`.
     - **A plain click** sets `off = 0` and lands every ↔/↕ key at the clicked point, through the one writer (`writeMany`).
     - **Alt** puts ¾ of each step into `off`, so the value moves ¼ as far as the hand from where alt goes down. Releasing alt keeps `off` and does not jump the value back.
     - **The ⇧ slot** stays relative (round 3's step). While ⇧ is down, `off` absorbs the ↔ motion, so the ↔ keys hold. A ⇧-click lands nothing.
     - **Blade roles** keep round 3's targeting (the arc under the pointer, or every blade from inside the circle). The click lands the value on each target.
     - **Hit mapping.** The ring's hit mapping now uses the painter's content box (`clientLeft`/`clientWidth` inside the 1.5 px border), so a click lands on the pixel where the marker is then drawn.
     - **Page text.** The hover readout, the mapping row's note, the I3 header, and open questions 25–27 are updated. Question 20 is marked superseded.
  2. **The phase carpet and the voice map**, ported from `src/gui/gui2.html` (`paintCarpet` :5966, `drawVmap` :6037, `star` :6107; the carpet is gui.html's port of `reference/swarmdynamics.html` drawCarpet) and fed from the SCALPEL oracle.
     - **Carpet fields.** Each member's phase `v.m[i].phi` on the monitor's live voice, the field the oracle's viz post publishes as `mem.phi` (`reference/scalpel/prototype/razor-core.js:802,809`). Each row's blade windows come from `cycleData()` (the viz feed's `mem.c/w/c2/w2`, the ring's arcs).
     - **Carpet time axis.** It is the monitor's stroboscope frames (`strobeLen`, factored out of `monStep`): whole note periods of rendered audio, never the clock. The trail is the last 16 frames at 0.75^age, which is gui2's wash per frame.
     - **Carpet adaptation.**
       - Rows = members, with phase across the strip.
       - Each row carries that member's own blade windows as bands.
       - Members are violet, magenta while inside a blade, as on the ring.
     - **Voice map fields.**
       - **Pan** from the gains the instance applies, `c.gl[i]` / `c.gr[i]` (`razor-core.js:648-654`), which include Stereo. The viz post carries no pan per member.
       - **Target** from couple()'s law, `detune·(2i/(N−1) − 1)` (`razor-core.js:420-421`), recomputed from `s.detune` and `d.N`. No field holds it.
       - **Actual** from the heard increment, `m.inc·v.gr` (`:427`, `:739/755`), against `v.freq·2^(bend/12)`.
       - The root star, the stepped zoom and its hysteresis are gui2's.
     - **Placement.** Side by side (a `.vtwin` strip).
       - **OSC:** under the spectrum, 128×76 each. The spectrum goes 132 → 92 and the ring keeps 250. The strip folds away while the ring's mapping row is open; with both open, the flex column squeezed the ring.
       - **MAIN:** under the cycle view in the left Visualizers cell, 123×52 each. The cycle view keeps its 150 px, and its caption is one line shorter.
- **Evidence consulted:**
  - ROADMAP B293, B294 and B295 (`lead-records-104`).
  - The B294 trace.
  - The lab, sections C, D, F, I2, I3, H and K.
  - `src/gui/gui2.html:1662-1680` and `:5947-6118`.
  - `src/gui/gui.html:1007-1030`.
  - `reference/swarmdynamics.html:679-693`.
  - `reference/scalpel/prototype/razor-core.js:62-65, 316-449, 640-660, 780-817`.
- **Verified vs entailed:**
  - **VERIFIED, in-page self-check 30/30, 9 of them controls that must fail.** I read it from headless Chrome via DevTools on the committed lab. Round 3's "ring: the grab alone moves nothing" is replaced. Ring checks:
    - **A click lands the value at the clicked point:** click (91, 70) on 260×250 → Detune 35.0000, Coupling 0.4400. The oracle's `t` agrees, and the marker sits at 91.00, 70.00.
    - **The drag follows and alt = ¼:** Detune 49.0385, Coupling 0.6400, marker 127.50, 45.00. The hand ends at 135: alt left the value 7.5 px behind.
    - **⇧ stays relative:** a ⇧-click moved nothing. Cross-mod 0.0100, Coupling 0.4000 and Detune 23.8462 match expected, with no jump when ⇧ is let go.
    - **CONTROL:** with `abs` off, the same click does not land (the marker stays 49.2 px from the click).
    - **Remap:** Width + inverted Stereo lands w and w2 at 0.1 of the taper and Stereo at 0.900.
    - Round 3's modulation check and its control are unchanged.
  - **VERIFIED, carpet checks:**
    - **Rows:** 5/5 at N 5, and the live carpet's rows equal the monitor's N.
    - **Lock → splay:** the newest r goes 0.999 at Coupling +1 → 0.029 at −1, the ring check's thresholds.
    - **CONTROL, must read zero:** at Detune 0 and Coupling 0, the drift against member 0 is 6.7e-16. At Detune 20 it is 0.346 cycle.
  - **VERIFIED, voice-map checks:**
    - **Dots:** 5/5, and the live map's dots equal the monitor's N.
    - **Law:** targets equal the law, and actuals sit on them at Coupling 0.
    - **Pan:** equals `RazorCore.PANS[N−1][i]` × Stereo.
    - **Tracks Detune:** the y spread goes 14.00 → 56.00 px at Detune 10 → 40 c on one zoom.
    - **CONTROL, must read zero:** at Detune 0, the largest |cents| is 0.
  - **VERIFIED, real mouse** (scratch CDP harness, `Input.dispatchMouseEvent`):
    - A click at 0.75, 0.25 of the ring gave Detune 75.189 c and Coupling 0.5040, with the oracle's `t` equal and the marker at 198.5, 61.5 of 264×248, under the click.
    - A drag to 0.3, 0.7 ended at Detune 29.848 and Coupling −0.4032, and the marker followed.
    - An alt-drag of +0.1 W gave 52.519 c (0.5 + ¼·0.1).
    - A ⇧-drag held Detune 14 and Coupling 0.35, and moved Cross-mod to 0.0102.
    - A waveform click moved nothing (still relative).
  - **Frame cost** (headless, `performance.now` in the harness only), in ms per frame:

    | Painter | ms |
    |---|---|
    | carpet (N 5) | 0.030 |
    | carpet (N 9) | 0.111 |
    | voice map | 0.013 |
    | ring | 0.024 |
    | spectrum | 0.18 |
    | all canvases | 0.58 |
    | monStep | 0.74 |

  - **`lab_load_check`:** GREEN (53 labs, 0 broken).
  - **Tier audit:** 93 rows at their tier, 8 promoted, 0 demoted, 0 missing.
  - **Layout:** no clipped text node on OSC (light and dark) or on MAIN (light; dark was checked by eye). The Advanced button stays inside the cell with the mapping row open.
  - **Not verified:** nobody has used the absolute ring by hand or listened to it. The carpet's and voice map's legibility in the plugin's real sizes is a lab judgement.
- **Alternatives rejected:**
  - **Absolute through a clamped relative accumulator.** It drifts off the hand after the pointer passes an edge.
  - **On alt release, snapping back under the pointer.** It undoes the fine adjustment.
  - **Landing the plane on a ⇧-click.** A ⇧-drag for Cross-mod would jump Detune to wherever you clicked.
  - **Feeding the carpet from the oracle's viz posts.** They arrive every floor(sr/30) samples, not whole periods, so the trail walked 0.67 cycle per post at A2. That is aliasing. The first build did this, and the zoomed shot showed it.
  - **A SPECTRUM / CARPET / VOICE MAP toggle.** It would hide the carpet or the voice map while the hand is on the ring that drives them. It is offered in the page as the alternative.
  - **gui2's value ink for the members.** In this lab magenta is the authored value.
- **Verify:** `./verify fast`, exit 0, git c211c6b (`.harness/last-verify.json`, 2026-09-27T03:01:36Z). This trace and the index regeneration are committed on top and re-verified before the push.
- **Screenshots** (scratch, not committed; `scratchpad/b295/shots/`):
  - 00: before (OSC light/dark, MAIN light).
  - 05–08: after (OSC light/dark, MAIN light/dark).
  - 09: the ring's mapping row open.
  - 10–12: ring click-snap (before; after, light and dark).
  - 13: mid-drag with the readout.
  - 14–17: carpet and voice map, locked and splayed, light and dark.
  - 18–19: drifting (Coupling 0, Detune 40).
  - 20–21: MAIN's Visualizers, light and dark.
- **Open questions:**
  1. On alt release, the drag keeps the fine offset (as built) rather than snapping back. Is that the right feel?
  2. When a blade role lands on both blades, they land on one position, and their relation is lost. Is that acceptable?
  3. The carpet's orientation (rows = members, phase across) differs from gui2's (columns = voices). Should the plugin follow the lab, or should the lab follow gui2?
  4. Members are violet here and magenta (value ink) in gui2. Which should the plugin use?
  5. OSC shows everything at once, with the strip folded while the ring's mapping row is open. The alternative is a toggle. Which does the human prefer?
  6. Round 3's open questions stand: the ⇧ slot, MAIN's ring placement, the arrow scale, and feedback.
  7. The in-page self-check is still not gated by `./verify`, as in B271/B293/B294.
