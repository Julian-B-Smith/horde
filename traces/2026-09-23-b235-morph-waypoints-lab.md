# b235-morph-waypoints-lab — waypoints between the morph corners, as a design lab

- **Queue item:** B235 (row carried in PR #736, branch `lead-records-84`; human 2026-09-23: "would it make sense to be able to add custom curves to blend parameters? … you move the morph point to a position and then go down to the parameter list and set the parameter to where you want it at that point and the curves are calculated from there. I suppose there would have to be a limit.").
- **Why:** B235 asks a design question before any build. This change extends B211's `docs/design/morph-editor-lab.html`, and nothing else. A continuous row may carry values pinned at pad positions ("pins"), and blend passes through them. The lab answers the six questions with working prototypes and measured numbers. No change to `src/`, `specs/`, `reference/`, `docs/design/index.html` or ROADMAP. The `lab-review` meta now reads `B211 + B235 · 2026-09-23`.
- **What the page does:**
  - **Block W (pure, seeded, no DOM).** Four laws:
    - TIN: piecewise-linear over a Delaunay triangulation of the values.
    - TIN·Δ: the same triangulation, over the residual against today's bilinear.
    - IDW·Δ: inverse-distance over the residual.
    - TPS·Δ: a thin-plate spline over the residual.
  - **Guarantees.** A corner guard makes the four corners read, never computed. There is a range clamp that always flags, and an optional "clamp to set values" (the hull clamp). B232's off-corner base is available as a toggle. A surface cache is keyed on everything a surface depends on.
  - **Painting.** The pad paints the selected row's surface under the chosen law, with:
    - contours;
    - a dashed triangulation, whose lines are the creases;
    - an alarm hatch where the range clamp acted, and a caution hatch outside the set values;
    - ◆ pins, ▫ global stations with ×n, and a dashed ring when the puck is on a pin.
  - **Four minis.** They paint the selected row under each law, each with its own measured stats: clip area, excursion past the set values, the reach of the selected pin, and "a pin HERE at what you hear moves the rest by".
  - **The row.** It shows a ◆n count that also jumps the puck through the row's pins. It shows ◇ when the pins are dormant.
  - **The inspector.** It shows:
    - the count against both limits;
    - the HERE control, which is live only while ARMED;
    - refusals, each with its reason;
    - the pin list with JUMP and ✕.
  - **Gestures.** Drag a pin to move it; click one to jump the puck onto it. Delete/Backspace removes the pin under the puck. Exempting a row removes its pins.
  - **Answers.** A panel answers the six questions with the numbers computed, not typed.
  - **Deep links:** `?law=`, `?clamp=0`, `?b232=1`, `?arm=1`, `?pin=`, `?view=wp`.
- **Evidence consulted:**
  - The B211 trace `traces/2026-09-22-b211-morph-editor-lab.md`.
  - ROADMAP B232 and B235, read verbatim from `origin/lead-records-84`.
  - `src/hypersaw_clap.cpp`:
    - morphStep ~4182-4245 (the blend branch `morphMode == 1 && !d->stepped`, and the pick/hold path);
    - morphJson / applyMorphChunk ~6035-6140 and cornerJson's layout-marker history ~5806-5833 (morphLayout 9);
    - the `intent=` chunk comment ~4413 (sparse, keyed on id, the ADR-159 scar);
    - the B100 engine_revision block ~6147.
  - `src/swarm_core.h:136` (kGravGridSeconds = 256/44100).
  - DECISIONS ADR-109: the weighted unarmed edit, and exempt writing the live value into all four corners.
  - `tools/labharness/lab_load_check.mjs`; `tools/gen_lab_index.py` (the lab-review meta).
- **Verified vs entailed:**
  - **VERIFIED (the lab's own self-check).** It runs at load and was also run headless in node on the final file. 24 seeded fixtures (mulberry32 seed 235, 1-8 pins, one pin on a hull edge in every fourth fixture) × 4 laws, 17×17 samples. All six properties hold:
    - P1: no pin reproduces today's blend bit for bit.
    - P2: corners are bit-identical under every law.
    - P3: every law passes through every pin (1e-9).
    - P4: TIN stays inside its hull.
    - P5: under every Δ law, the first pin set at the heard value changes nothing.
    - P6: the planted overshoot is flagged on 18 samples and clamped exactly at the bound.
  - **Controls, all 4 correct.** Each re-runs the same check function the properties use:
    - M1: a law that ignores its pins misses 100 pins.
    - M2: a sample planted 1% past the hull is seen.
    - M6: the overshoot flag reads 0 under TIN, and 0 under clamped TPS·Δ.
    - M3 (a measurement): without the corner guard, TPS·Δ lands 1.2e-14 off a corner.
  - **MEASURED (fixture table, raw laws before either clamp):**

    | | past set values | fixtures past range | Nth pin at heard value moves pad | jump while dragging a pin | mean reach |
    |---|---|---|---|---|---|
    | TIN | 0% | 0/24 | 33% | 56% | 44% |
    | TIN·Δ | 0.7% | 0/24 | 29% | 54% | 44% |
    | IDW·Δ | 1.5% | 0/24 | 11% | 6e-7 | 65% |
    | TPS·Δ | 52% | 10/24 | 1e-14 | 4e-8 | 78% |

    "Jump while dragging" is what survives bisecting the worst drag step 24 times. It is ~0 for a law that is continuous in its pin positions. It is not ~0 for a mesh that flips an edge.
  - **VERIFIED (scratch gesture harness).** All three script blocks were run under lab_load_check's stub DOM. 18 real assertions pass:
    - a pin at a corner is refused;
    - creating a pin does not move the corners, and the heard value equals the pin at the pin;
    - an edit on a pin (or within the snap radius of one) changes that pin; it does not add one;
    - a new pin snaps to another row's station;
    - refusals: stepped rows, gates, QUANTUM, the row cap (8) with its stated reason, and B232's silent spot;
    - delete works; exempting removes the row's pins;
    - with pins on the page, corner A's heard value stays bit-identical;
    - the chunk string is well formed.

    A 19th line, "B232 reads ON corners", passed only through a fallback clause, because the spot was silent. It is **not** evidence. B232's behaviour is shown in screenshot 05 instead.
  - `lab_load_check`: OK. B211's parity is still 1092/1092.
  - **ENTAILED, not measured:**
    - the cost model (terms per tick, from the operation counts; no timing — the lab reads no clock);
    - the state sizes (≈ 3.3 KB of corner text, 64 pins ≈ 1.9 KB, from a JS approximation of %.6g).
- **Recommendation (for the human, not decided here):**
  - **Law: TPS·Δ with "clamp to set values" on.** It is the only law where one more pin at what you hear is a true no-op (1e-14): the thin-plate spline is the unique least-bending interpolant. Dragging a pin is continuous. It is exact at pins and corners, and a patch with no pins is today's law.
    - Its overshoot (52% of span unclamped) becomes a flat top under the hull clamp.
    - Against it: global reach (78%), and a solve per edit.
    - Runner-up: TIN·Δ, if the human prefers locality; it carries the 54% drag jump.
  - **Limit: 8 per parameter, 64 per patch.** These are stated as judgement, not forced by cost:
    - worst case 320 kernel terms per tick at the caps, against 724 multiply-adds for today's blend;
    - 2172 terms with no patch cap;
    - about 1.9 KB of state.
    - What binds is legibility and a fixed preallocated pool.
- **Alternatives rejected:**
  - TIN over raw values as the answer, even though the brief framed it as bounded. The measured drag jump and the whole-pad reshape on the first pin rule it out.
  - A super-triangle Bowyer–Watson. It loses hull triangles for pins near an edge; the lab seeds with the square instead.
  - Wall-clock timing for the cost answer: banned in this lab, so operation counts are used.
  - Staging pin edits the way corner edits are staged: a pin is heard AT the puck, so there is no surprise to stage against.
- **Verify:** `./verify fast`, exit 0, git d506357 (`.harness/last-verify.json`, 2026-09-24T03:47:13Z). This trace is committed on top; the final hash is in the PR report.
- **Screenshots** (headless Chrome at 2×, scratch, not committed):
  - 01: light theme, TPS·Δ clamped, planted overshoot.
  - 02: dark theme, unclamped, alarm hatch.
  - 03: light theme, armed on ◆1, with the inspector.
  - 04: dark theme, QUANTUM, dormant ◇ pins.
  - 05: light theme, B232 on, osc 2 Detune.
  - 06: dark theme, the global stations.
- **Open questions:**
  1. **Law:** TPS·Δ + hull clamp (smooth, additive, global), or TIN·Δ (local, creased, jumps mid-drag)? Once shipped, the law is behaviour; changing it later is a B100 revision.
  2. **Limits 8 / 64** are legibility judgements. Is a per-patch cap wanted at all? Cost would allow about 1,400 pins.
  3. **Exempt removes pins** (ADR-109's write extended to them) versus keeping them dormant.
  4. **The unarmed ADR-109 weighted edit on a pinned row, off a pin.** Should it target the law's output rather than the bilinear sum? Near pins the corners' leverage goes toward 0.
  5. **State keyed on param id with morphLayout left at 9** (the `intent=` precedent), not an index-keyed array with a bump to 10. The brief mentions the layout marker; this lab argues it is not needed.
  6. **B232's lab mapping is by SECTION** (osc 1 → 150, osc 2 → 1150, sub → 4015). The shell would route it through the depends graph.
  7. **The lab index is not regenerated.** `docs/design/index.html` is out of scope; the lead runs `tools/gen_lab_index.py`.
  8. **Quantum:** pins stay dormant. Is the edited quantum boundary (the human's own analogy) the right sibling to spec next?
