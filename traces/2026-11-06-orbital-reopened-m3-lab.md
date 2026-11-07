# 2026-11-06 — ORBITAL reopened (sin and cos of the angle); M3's LFO kinds, mix against switch

**What changed.** `docs/design/shape-lab-mod.html` only, as round 6. ORBITAL (the lab's transcription) gains sin and cos of each body's angle, `accel` on a per-preset ceiling, ADR-375's per-preset scales for speed and energy (with a CLAMP switch that keeps SPEC-ORBITAL §4's reading), and the reference's thermostat (switchable, v* and τ). A new scope shows eight source rows for one body, each with its range per second over the last minute, and any row can be heard on the test tone. A new M3 bench puts two LFO kinds in corners A and B and morphs between them by MIX or SWITCH. `r6SelfCheck` covers both. No engine, spec or reference file changes.

**Why.** The human, 2026-11-06, through the lead: "let's re-open the orbital lab", after "if we do sine/cosine of angle for orbital, it might solve the diminishing scope issue"; and on round 62's [OPEN M3]: "Let's test a mix in a lab before deciding".

**Evidence consulted.** ROADMAP B126; DECISIONS ADR-165 with A1 and A2, ADR-375 with A1, ADR-377; `specs/SPEC-ORBITAL.md` §4, §11 and §12 (read only); `reference/gravity-modulator.html`'s `thermostat()` and `meanSpeed()` (:325-361, read only); round 62's notes and SPEC-ENGINE draft (commit d8693366: M3, M4, MB-1 to MB-10, X46, X73); SPEC-ENGINE MO-5, MO-7 and MO-8; the lab's round-2 findings 6 and 7.

**What "the diminishing scope issue" turned out to be.** No record names it. Two records match it, and the lab builds both. (1) B126, 2026-09-15: the human asked for "some force or process counteracting drag". Under drag every body spirals into the centre and every position and motion output shrinks to a flat line. Measured on binary at drag 0.05: x's peak-to-peak is 0.19 in the first 10 s, 0.01 by 30 s and 0.00 by 60 s. (2) The lab's round-2 finding 6: speed and energy over the 3.0 clamp read about 0.1 and 0.01, which ADR-375 answered with per-preset scales.

**Findings.**
- sin and cos keep a full ±1 at any radius, so they survive the collapse that x and y do not. At binary with drag 0.3 after 35 s, x's range was about 10⁻⁸ while sin's was 2.0.
- They are not seam-free everywhere. The angle is taken about the field's centre, so a body passing near the centre turns by up to half a turn in one step. Measured largest one-sample steps over 10 s: sun 0.43, figure-eight 1.73, chaos 1.75, and binary 0.02, since binary circles the centre. Once a body has been pulled into the centre its angle is numerical dust, and sin and cos chatter there.
- The thermostat restores pace, not size. On the page, binary was drained by drag 0.29, then the thermostat was switched on and the body kicked. The mean speed came back to 0.366, as predicted, but x's range came back only to 0.034 (0.24 before), while sin and cos stayed at about 2.0.
- The thermostat cannot restart a field at rest. The reference's guard skips a mean speed below 10⁻⁹, so it did nothing until a KICK.
- ADR-165 A1's ratified "sin/cos observables" are sin and cos of each position in Wrap, which is round 62's M4 (c). Today's ask is sin and cos of the angle, a different source. Only the angle's is built.
- accel's ceilings, by MB-7's rule: binary 4.3, sun 23, figure-eight 1.5, chaos 8.7.

**Oracle.** `r6SelfCheck` runs in the page at load and again when the LFO card's points change, and it runs headless. Each check has a control that must fire:
- sin and cos against the bodies' own positions in a twin: 3e-16. Control, the raw angle as ±1: 1.55.
- No seam on binary: largest step 0.023. Control, the raw angle's wrap: 0.999.
- accel against the force summed from scratch: 1e-15. Control, G × 1.1: 0.09.
- The ceilings are at or above the 30 s peak and within 10 %. Control: rounded down, some fall below.
- The shrinking, binary at drag 0.3 for 40 s: with the thermostat off the mean speed goes from 0.425 to 6e-8 (the detector fires). With it on, 0.363 against 0.363 predicted.
- The heard scope row against the destination: 0. Control, 100 ms off: 0.995.
- MIX, 12 kind pairs at two positions, against (1 − m)·A + m·B: 0. Control, weights swapped: 0.98.
- SWITCH: no sample is neither corner (control: MIX gives 38,398 such samples). It flips at the closed form's up and down positions. Two seeded runs are identical, and with Math.random as the plant they differ. A 50 ms glide reaches 99 % in 1,105 samples, against 1,105.

Seven source mutations, each planted in scratch, each turned its part red. `lab_load_check` is GREEN.

**Checked in the in-app browser** (served from this worktree, after a full load). All six audits pass. There are no errors in the console; the only warning is the existing ScriptProcessor deprecation. Real mouse clicks were used for: a scope row (it soloed the row and started the drone, and the scope filled), the drag slider (the rows' ranges fell live: x from 0.146 to 0.003, energy from 0.77 to 0.004, sin held at 1.99), the thermostat, KICK, and M3's HEAR, SWITCH and SWEEP (two flips at the readout's boundary).

**Not checked.** Listening. Whether the human hears what the numbers say is the point of the lab. The dark theme by eye. A phone-width layout beyond the existing breakpoints.

**Verify.** `nice -n 10 ./verify fast` in this worktree; see the report.

## Round 6b — both alternative angles; auto-kick (same day)

**What changed.** `docs/design/shape-lab-mod.html` only.
- Each body gains sin and cos of two more angle references, each with its own scope rows and test tone, and an **angle about** selector to A/B them by ear:
  - (a) the angle about the centre of mass of the other bodies;
  - (b) the heading, the velocity's direction.
- Every scope row prints its largest one-sample jump over the last 10 s, measured on the raw output before the 5 ms smoothing.
- With the thermostat on, a field at or below the thermostat's 10⁻⁹ skip threshold gets KICK's kick by itself. The direction comes from the auto-kick's own seeded stream, and the readout counts the auto-kicks. KICK stays.
- M3 is left as it was: it is the record of the SWITCH ruling.

**Why.** The human, reviewing round 6 (through the lead, 2026-11-06): "Build both, then decide" and "Auto-kick a stopped field". M3 was ruled SWITCH for 1.0.

**Findings.**
- Largest one-sample jump of sin or cos over 10 s, bodies 1 to 4, given as centre / others' COM / heading:
  - binary: 0.023 / 0.023 / 0.014
  - sun: 0.43 / 0.54 / 1.99
  - figure-eight: 1.73 / 1.73 / 0.006
  - chaos: 1.75 / 1.75 / 1.99
- **Under the COM lock, (a) is the field-centre angle exactly.** The lock holds the field's COM at the centre, so the others' COM lies on the line through the body and the centre. It differs only on sun, where the pinned body turns the lock off, and there it is worse.
- **(b) never sees the centre, but it jumps elsewhere.** On sun, all six large jumps were wall bounces. On chaos, 26 of its 29 were away from the walls, where a body nearly stops and turns back: the worst was 1.94, at a speed of 0.066.
- A third candidate is named on the page but not built: the angle about the nearest body.
- **The scope's first jump figure was wrong, and is fixed.** It first read the smoothed rows, and the 5 ms one-pole made a 1.45 flip read 0.055. The figure is now taken on the raw observable, and a self-check pins this.

**Oracle.** `r6SelfCheck` gains three parts, each with a control that must fire:
- **The two references against their own definitions:** others' COM 1e-13, heading 5e-16. Controls: COM against the field centre on sun, 1.18; heading against position, 2.0.
- **The auto-kick.** Binary at drag 1 runs down to 9e-15, then the thermostat goes on. It is kicked on the first step and settles at 0.251 against 0.25 predicted. Control: auto-kick held off stays at 9e-15. Two seeded runs end identical; with Math.random as its stream, two runs differ.
- **The scope's jump figure** against a twin's raw sin: 1.452 = 1.452. Control: measured on the smoothed row, 0.055.

Four new source mutations, planted in scratch, each turned its part red; so did a smoothed-row jump measure.

**Checked in the in-app browser** (port 8147, after a full load). All six audits pass, and the console shows no errors. Real clicks:
- FIGURE EIGHT, then HEADING (soloed tone), then body 3.
- Drag to its maximum, with the thermostat off, until the mean |v| reached 4e-10.
- Thermostat ON: the auto-kick fired on that click, the field settled at 0.251 (0.25 predicted), and the readout showed "Auto-kicks: 1".
- Drag reset by double-click, then FOUR-BODY CHAOS. Body 1's jumps over 10 s read 0.84 for the centre and the COM, and 2.0 for the heading.
