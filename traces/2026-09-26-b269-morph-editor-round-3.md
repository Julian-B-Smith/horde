# b269-morph-editor-round-3 — the puck stays put, a hover preview, editable QUANTUM boundaries with locks, Cohesion, STEPPED, and the audits wired

- **Queue item:** B269. I read the row verbatim from `origin/lead-records-94:ROADMAP.md`, carried in the lead's records PR. The human, 2026-09-26, after merging B268: "Great so far".
- **Why:** The human reviewed the waypoints round and asked for four things:
  1. The heard position stays still while pins are edited.
  2. A preview of the value under the cursor.
  3. QUANTUM boundaries that can be edited and then survive reseeding until unlocked.
  4. A stepped blend.

  The human also approved turning the in-page audits into a wired gate: "Go for it". B268's trace had asked for that (open question 1). The lead proposed renaming the morph's Coupling to Cohesion.
- **What changed:**
  - `docs/design/morph-editor-lab.html`. The `lab-review` meta is now `B211 + B235 + B268 + B269 · 2026-09-26`.
  - `tools/labharness/morph_editor_check.mjs` (new).
  - `./verify`: one block in `fast()`, beside `fxlab_check`.

  The lab changes:
  - **The puck stays put.**
    - The pad's gestures are now functions of a state: `padDown`, `padMove`, `padUp`, `padDbl`, `padCtx` and `padWheel`. `wirePad` only translates events into these calls, and the round-3 audit calls them directly on a scratch copy.
    - A click on empty pad moves nothing. Only a drag of the puck moves it, with a grab offset so the puck never jumps to the pointer.
    - These gestures all leave the heard position alone: a pin drag, a value drag, the wheel, a click on a pin (B268 jumped the puck onto it), a double-click, a right-click, and the inspector's ARM + HERE (B268 snapped the puck to a shared station).
    - JUMP in the pin list and the row's ◆n count still move the puck. They are explicit "take me there" commands.
    - **Double-click value:** a new pin takes the value the heard law already gives at the clicked spot. It does not take the puck's value, and it does not take a stepped value. Under TPS·Δ that pin changes nothing until its value is moved (B235's Nth-pin property), so adding a pin never surprises the ear. Pinning the puck's value instead would bend the row as soon as the pin appeared.
  - **Hover preview.**
    - `hoverInfo` is `resolveRow` at the pointer, in the page's own state (mode, law, clamp, locks, step, a staged edit).
    - It is printed at the pointer. The readout shows the value, plus either the law or the owning corner, plus the boundary a press would grab.
    - It is also ticked on the colour scale (▼), and the scale caption repeats it.
  - **QUANTUM boundaries, editable, with locks.**
    - **Regions.** A row that picks (every structural row, and a continuous row under QUANTUM) paints its corners' regions. Colour marks the corner, alpha tracks the value, held cells are ticked, and each region is labelled with its corner and value. The regions come from `pickFor`, which falls through to the unchanged `pickCorner` for any row without a lock.
    - **Gesture: drag a boundary.** Press within 5 px of a colour edge and drag. The boundary between the two leading corners follows the pointer: β_k − β_m = log w_m(p) − log w_k(p), with β_k + β_m kept.
    - **Lock snapshot.** Locking stores β = T·((1−c)·g + c·g_shared) − β_A. That has the same argmax, so locking moves nothing. After that the row scores log w + β, which the seed, Temperature and Cohesion do not reach.
    - **Where the lock shows:**
      - a chip on the row (one click unlocks);
      - a bar over the pad with UNLOCK or LOCK AS IS;
      - an inspector section with region shares, the stored β, the buttons, and the exempt and B232 notes;
      - a ⟳ reseed button next to Seed, drawn by mulberry32 from the current seed.
    - **Units** (scale, FX slot, routing) lock as one, keyed by the unit lead.
  - **Proposed state:** a sparse `morphLocks` key, `L:1,Q:<lead param id>:<βB>:<βC>:<βD>`, next to `morphWaypoints`. It is keyed by parameter id, so morphLayout stays 9. It is the "authored bias" term that `src/morph_core.h:4` names as part of the one score; the port does not have that term.
  - **Lock and exempt.** Exempt resolves before any pick, so a lock is dormant while its row is exempt, and it is kept. A lock holds WHERE, not WHAT, so ADR-109's live write does not make it a no-op the way it does a pin.
  - **Lock and B232.** B232 is read for picks behind the existing switch: where the source is audible, an OFF corner cannot win its parameters. That corner's region is masked at resolve time, and the lock keeps its offset.
  - **Cohesion.** The lab's player-facing label, the resolver row, the id-155 absent row and the internal state field (`S.cohesion`) are renamed. The shell's `morphCoup` key and `pickCorner`'s `coup` argument are unchanged: one is state, the other is a line-for-line port. A one-line note under the slider says what Cohesion does.
  - **STEPPED**, proposed as mode 2 of id 157:
    - A global STEP slider (0..1), with n = round(1/STEP).
    - Reading (a) snaps the pad POSITION to an n×n grid; the puck shows its grid point.
    - Reading (b) snaps each row's blended VALUE to n steps of its own range.
    - A/B pills switch between the two readings. The answers panel states how they differ.
    - Pins stay editable and are heard through the step.
    - A structural row's lock still acts. A continuous row's lock is dormant, as in BLEND.
    - Gates keep ramping on the true position, because a stepped level would click.
    - The law maps and the cross-section stay unstepped.
  - **The round-3 audit** is a fourth audit line on the page and is exposed as `__morphEditorAudit.b269`.
  - **New deep links:** `?mode=stepped&step=&read=`, `?temp=`, `?cohesion=`, `?seed=`, `?lock=`, `?reseed=`, `?qdrag=x0,y0,x1,y1` (left mid-drag), and `?hover=x,y`.
- **Evidence consulted:**
  - ROADMAP B235, B268 and B269 (`origin/lead-records-94`).
  - `traces/2026-09-26-b268-morph-waypoints-ux.md`.
  - `src/morph_core.h` (the one score at :84, gShared, and the "authored bias" at :4).
  - `tools/labharness/lab_load_check.mjs`, `station_check.mjs` and `gui_history_check.mjs` (stub DOM, anchored plants, the WIRED idiom).
  - `tools/test_table_check.py` (rule 5).
  - `tools/gen_lab_index.py`, `tools/serve_labs.py`, and `./verify`.
- **Verified vs entailed:**
  - **VERIFIED: the in-page audits,** read back by `morph_editor_check` under the stub DOM on c0498e6:
    - field audit 1092/1092;
    - waypoint audit 6/6 properties and 4/4 controls;
    - interaction audit: open ok, gestures 5/5, controls 4/4.
  - **VERIFIED: the NEW round-3 audit, 9/9 properties and 9/9 controls.** Each property uses the page's own functions on a scratch copy of the opening state, and each has a control:

    | Property | Result | Control |
    |---|---|---|
    | P: the puck stays put | Stays put through 8 gestures (click, double-click, pin drag, value drag, wheel, pin click, right-click, boundary drag); moves by exactly the drag when the puck itself is dragged | MP: B268's rules replayed are seen moving it on click, double-click, pin drag, value drag, wheel, pin click, right-click and boundary drag |
    | H: the readout is the colour under it | hover = padSamples on 4032 cells over 7 mode/row cases, 0 off | MH: a readout on IDW·Δ differs in 576 cells |
    | L0: locking moves no region | 268 picking rows at T = 0.5 | ML0: a snapshot without T moves 267 rows |
    | **L1: a LOCKED row survives reseed + Temperature 2.0 + Cohesion 0.85 at once** | 0 rows moved | **ML1:** a lock re-taken after the change moves 268 |
    | **L2: an UNLOCKED row does not survive** | a reseed alone re-deals 268/268 (the gate is ≥ 90%) | **ML2:** the same seed re-deals 0 |
    | L3: one click unlocks exactly | the unlocked map equals the live map | ML3: locked ≠ live before the click (268 rows) |
    | L4: a boundary drag through padDown/padMove lands on the pointer | the A\|B score gap is 0e+0, the row is locked, and every corner still owns its own corner | ML4: 0.1 away the gap is 0.40 |
    | **S0: STEP 0 is BLEND bit for bit** | both readings, 15123 samples (pinned rows on 17×17, others on 9×9, corners and edges included) | **MS0:** STEP 0.01 differs in 1433 samples for (a) and 4332 for (b) |
    | S1: pins act in STEPPED | a pin can be added; (a) a spot that snaps onto the pin hears it; (b) the pin's spot hears its value snapped | MS1: unstepped, the off-grid spot is not the pin |
  - **VERIFIED: the gate's own must-fail controls,** which run on every call. Each plant is one fault in an in-memory copy, and each must turn ITS named audit red:
    - the shared draw read mirrored → field audit 1004/1092;
    - the corner guard removed → waypoint P2 fails on 66 samples;
    - the page opening in QUANTUM → interaction audit open fails;
    - B268's click-moves-puck → round-3 P fails;
    - a lock that is only a flag → round-3 L1, ML0 and ML3 fail.

    The planting mechanism itself is checked with an anchor that must not be found. The first parity plant I tried was a 1% Cohesion drift. It flipped **no** owner among the 1092 and was caught only by L0/L1, which is why each plant now names the audit it targets. That is also a finding about the parity audit's resolution (see open questions).
  - **VERIFIED: manual scratch plants, run through the CLI's path argument** (copies in `scratchpad/b269/`):
    - `lab_planted_dbl.html`, where a double-click moves the puck: exit 1, "FAIL round-3 audit: failing P";
    - `lab_planted_step.html`, where STEP 0 is not exact: exit 1, "failing S0".
  - **VERIFIED: a real-DOM pass in headless Chrome over CDP** (scratch `cdp.mjs`: real PointerEvent, MouseEvent and input events on the served page). 27 assertions, all true, and no page errors:
    - the click, double-click, pin drag, alt value, pin click and right-click tests keep the puck still, and each gesture does its job;
    - the hover readout appears and clears on pointerleave;
    - dragging the puck moves it exactly;
    - QUANTUM shows the banner;
    - a boundary drag on SUB Wave locks it, shows the lock bar and the row chip, and keeps the puck;
    - the ⟳ button keeps the locked row's shares and re-deals an unlocked one (1005);
    - UNLOCK in the bar unlocks;
    - STEPPED shows STEP; STEP 0 hears the same as BLEND; (b) at 0.25 snaps to quarters;
    - all four audit lines are green;
    - the label reads Cohesion.
  - **Timing:** `morph_editor_check` takes 1.3 s wall (8.4 s CPU). The six page loads, the real one and five plants, run in parallel on `node:worker_threads`, which is built in (no dependency). Serially it took 5.3 s. Under node, one page load is about 0.8 s: B235's audit 0.36 s, the round-3 audit about 0.38 s.
  - **ENTAILED, not verified:** that a lock taken in the β form never flips an owner at a floating-point tie. L0 shows zero flips over 268 rows × 225 cells, but log w + T·b and log w / T + b round differently, so exact identity holds only up to ties of measure zero.
- **Alternatives rejected:**
  - **A brush that paints cells to a corner.** It can make regions that do not contain their own corner, and it needs a grid per row where a boundary needs three numbers. The boundary drag keeps every region a pickCorner region.
  - **Storing (T, b) for a lock.** That is four or five numbers where three differences suffice.
  - **A fifth-candidate pin in QUANTUM.** B235 already rejected this.
  - **Importing `lab_load_check`'s stub.** Its top level is a CLI sweep, so the stub is copied instead.
  - **Wiring into `full`.** Parallel loads bring the check to 1.3 s.
- **Verify:** `./verify fast`, exit 0, git c0498e6 (`.harness/last-verify.json`, 2026-09-26T18:12:52Z). It printed `morph_editor_check: GREEN — 0 audit failure(s); controls 5/5 planted faults caught`. This trace and the index regeneration are committed on top; the final hash is in the PR report.
- **Screenshots** (headless Chrome, 1600×1300; scratch, not committed; `scratchpad/b269/shots/`):
  - 01-hover-{light,dark}: the preview at the pointer and on the scale.
  - 02-quantum-boundary-lock-{light,dark}: SUB Wave mid-drag of its A|B boundary, locked, with the lock bar and the inspector's β.
  - 03-stepped-a-position-{light,dark}: 5 steps, the puck's grid tick, the hover readout.
  - 04-stepped-b-value-{light,dark}.
- **Open questions:**
  1. **Cohesion's one line.** The lead proposed "how much parameters in the same GROUP pick the same corner together". The law has one shared draw for the whole field (`src/morph_core.h:84`, `gShared`); only a unit always moves as one. The lab says "the parameters pick the same corner together … at 1 the whole field flips as one". Is per-group cohesion wanted in the shell, which would be a new draw and a B100-class change? Or is the lab's wording the ruling?
  2. **Boundary editing on structural rows in BLEND and STEPPED.** They pick in every mode, so the lab lets them be edited outside QUANTUM too. The brief said "under QUANTUM". Confirm.
  3. **Exempt keeps a lock** (dormant), whereas exempt removes pins. Confirm.
  4. **B232 for picks** masks OFF corners where the source is audible. This is proposed, behind the same switch.
  5. **STEPPED:** which reading, whether mode 2 of id 157 is the right home, and gates left unstepped.
  6. **A click on a pin no longer jumps the puck,** which B268 did. JUMP and the ◆n count still do. Confirm.
  7. **The parity audit's resolution.** A 1% drift of the resolver's Cohesion flips none of its 1092 samples. It guards the law's structure, not its constants. L0 (the lock snapshot against the live pick) is what saw the drift, and only because the two formulas are written separately.
  8. **B268's 0.6 px click/double-click layout shift** was not re-examined.
