# b296-blade-clock-lab — the blade window on its own clock: a listening prototype, measured

- **Queue item:** B296. I read the row verbatim from `origin/lead-records-105:ROADMAP.md` (records PR #781). The demo-preset addition is recorded on records PR #782 (`lead-records-106`). The human, 2026-09-27:
  - "is there a way of desynchronizing the blade distance from the phase of the governing waveform, to create irrational frequency relationships?"
  - "Let's prototype it so I can make sure I like the sound before deciding on locking it in"
  - "Could you please add some presets demonstrating the effect?"
- **Why:** This is a prototype for a listening decision, not a ship decision. Shipping it needs an ADR (it diverges from the SCALPEL reference) and the B275 readiness fields; neither is attempted here.
- **What changed:**
  - One new lab, `docs/design/blade-clock-lab.html`. Its `lab-review` meta is `B296 · 2026-09-27`, and the title says PROTOTYPE.
  - `docs/design/index.html`, regenerated.
  - This trace.
  - Nothing under `src/`, `specs/` or `reference/` changed.
- **The override (the exact point):**
  - `BladeClockCore extends RazorCore`. Its `render()` is the oracle's own source text, read at load from `RazorCore.prototype.render.toString()`, with four anchored substitutions. Each substitution must match exactly once, or the class is not built and the page says so.
  - All four are in the rotation block of `reference/scalpel/prototype/razor-core.js` render:
    - `:704` rotBase
    - `:709` rotBase2
    - `:712` the `v.rot` advance
    - `:713` the `v.rot2` advance
  - With the clock on, rotation becomes per voice and advances by `r·v.freq·2^(bend/12)/sr`.
  - Every replacement keeps the oracle's line as its `else` branch.
  - The same class source is appended to `RazorCore.toString()` to build the AudioWorklet, the SCALPEL lab's route.
  - Rejected: an instance override of a smaller method, as the filter lab's P4 does. There is none: rotation is inline in the monolithic per-sample `render()`.
- **Modes and ranges:**
  - **Hz** is the oracle's own `rotRate`, driven past the bench's ±4 Hz. It adds no new law: `set()` stores any value, and `render()` advances by `rotRate/sr`. The range is ±2 kHz on a signed log taper with a 1 Hz knee.
  - **× f0** takes a ratio r from −2 to 2. The cut recurs at (1 − r)·f0.
  - One-click ratios: φ−1, 1/√2, π−3 and √2−1, plus 1/2 and 1/4 for comparison.
  - Blade 2 either follows blade 1's clock (`rot2Follow` 1) or runs its own clock (Hz or × f0).
  - A/B: A is the patch as the oracle plays it; B is the clock. Space toggles between them.
- **Presets:**
  - **8 bench presets**, fetched and never copied, one or two per mode, all with partial-width windows:
    - Sync: Quarter sync, Locked horde
    - FM reset: Zap bass, Vowel choir
    - Ring: Ring saw, Golden bells
    - Crush: Slewed crush, Crush vs FM
  - **12 demos, in "prototype voicing"**:
    - Golden cut
    - Half-rate cut (rational)
    - Horde at 1/√2
    - FM reset at π−3
    - Vowel on a √2−1 clock
    - Ring at φ−1
    - Crush at 1/√2
    - Fold at 1/4 (rational)
    - Fixed 300 Hz clock (Hz mode)
    - Phasing · slow (1.5 Hz) and Phasing · audio (150 Hz), the slow-to-fast pair
    - Two blades, two clocks
  - Each demo has a B button and an A button (its locked twin).
  - Each demo uses the oracle's DC **blocker** (dcMode 1), not its default per-cycle estimate (dcMode 2). The reason is measured: in a scratch render of Quarter sync at A3 with r = 1/2, dcMode 2 put the energy off the f0/2 lines at −20.7 dB, against −31.9 dB for the blocker and −32.1 dB with DC correction off.
- **Evidence consulted:**
  - ROADMAP B252 and B296.
  - `docs/scalpel/ACCOUNTING.md` rows 5 and 70 (`rotRate`, `rotRate2`).
  - The B293, B294 and B295 traces.
  - `reference/scalpel/prototype/razor-core.js`, read in full. The key sections: `:340-363` (startVoice), `:520-590` (stepM, tryE), `:590-819` (render).
  - `reference/scalpel/data/presets.json`.
  - `docs/design/scalpel-interface-lab.html`: tokens at `:93-194` and `:205-282`, the worklet route in section J, and the seeding.
  - `docs/design/filter-lab.html` (the P4 idiom).
- **Verified vs entailed:**
  - **VERIFIED, in-page self-check 20/20, 11 of them controls that must fail.** I read the results from headless Chrome via CDP on the committed lab, in both themes. They also run identically in a scratch Node vm.
    - The four anchors each match once. **Control:** a stale anchor matches 0× and the builder refuses to build the class.
    - **Clock OFF equals the oracle, sample-exact,** on L and R, 8192 samples each, for 11 patches: the 8 bench picks, Two clocks, Counter-rotation (the oracle's own Rotate, blade 2 on its own clock), and a free-running variant. **Control:** with the clock on, the two differ (max |Δ| 0.72).
    - **Hz mode at 1000 Hz equals the plain oracle, sample-exact.** **Control:** the oracle at 1000 Hz differs from the oracle at 4 Hz, so the value is neither clamped nor ignored.
    - **× f0 tracks the key.** After 4800 samples, the window's advance matches r·f0·t at A3 and at A5 (errors 6.6e-14 and 5.7e-14). **Control:** a fixed Hz clock misses at A5 by 0.210 cycle.
    - **Periodicity** (one member, f0 = 187.5 Hz, so the period is exactly 256 samples):
      - r = 1/2 repeats at the predicted 512 samples (relative error 2.5e-11).
      - r = φ−1 repeats at no lag from 64 to 4096 (best is 0.483).
      - **Control:** r = 1/2 at the carrier's period of 256 fails (0.956).
      - **Control:** with the clock off, the output repeats at 256 (1.3e-11), so the detector can say "periodic".
    - **Off-grid by the predicted amount** at r = φ−1. The two strongest off-grid peaks sit at 0.3820 and 0.6180 of a harmonic, the first sideband pair ±(1 − r)·f0. All 12 strongest peaks sit on the lattice a·f0 + b·(1 − r)·f0 (worst error 0.0000). **Control:** at r = 1/2 every peak is on the half-harmonic grid. **Control:** the φ−1 peaks do not fit that grid (error 0.236).
    - **No non-finite sample** in 360 renders: 8 picks × C1/C4/C7 × 10 ratios (including 1) and 5 Hz rates (±2 kHz), with blade 2 on the opposite clock. **Control:** a planted NaN clock is caught.
    - **The aliasing meter's own control** (this check caught a bug, below). A pure sine reads −∞ dBc, and a tone planted at −40 dBc reads −40.0.
    - **The demos:** all 12 load, render finite and above −60 dBFS on their clocks, and their locked twins equal the oracle, sample-exact. **Control:** an invented key is caught.
  - **VERIFIED, the audible path runs the same class.** An OfflineAudioContext render through the worklet (TEST AUDIO PATH) equals the main-thread BladeClockCore render in 24000/24000 samples (float32), on both B and A.
  - **MEASURED (reported in the page, not gated):**
    - **Aliasing** (one member, 48 kHz against the same wrapper at 192 kHz, dBc; bins at least 6 dB above the reference only, so a lower bound):

      | Patch | f0 | Oracle (locked) | r −(φ−1) | Hz 1000 |
      |---|---|---|---|---|
      | Quarter sync | 220 | −38.0 | −26.4 | −20.2 |
      | Zap bass | 220 | −30.6 | −31.0 | −21.3 |
      | Ring saw | 220 | −52.2 | −29.1 | −21.4 |
      | Slewed crush | 220 | −72.2 | −95.0 | −40.3 |

      - r = 1/2, φ−1 and 0.9 on Sync and FM reset stay within 3 dB of the oracle's own figure, and are sometimes lower.
      - Ring and Crush rise at most settings, by up to +32 dB (Crush at 220 Hz, Hz 1000). The exceptions are Ring at 880 Hz on Hz 1000 (−63.0) and Crush at 220 Hz on −(φ−1) (−95.0).
      - The worst cases are the negative ratio and the Hz clock above the note.
    - **DC.** Figures are dB relative to the uncorrected power, at A3.
      - The per-cycle estimate (dcMode 2), the oracle's default, does not hold under the clock:
        - At r = 1/2 it leaves −14.9 dB of DC on Quarter sync, where the blocker leaves −47.5.
        - Its own correction signal carries −9.5 to −20 dB of AC, against −29 to −41 when locked.
      - With the blocker (dcMode 1), DC is at most −34.8 dB (−44.7 to −83.1 at φ−1 and 0.9).
      - Sub-20 Hz energy also rises with the clock with no correction at all (−43 to −46 dB locked, −25 to −29 at r 0.9). That is real content, since the cut recurs at 22 Hz there.
    - **Clicks** (edges crossed by the window's jump; a per-call instance override of stepM on the measuring instance only):
      - Locked and the oracle's own Rotate at 4 Hz: 0.
      - r = 1/2: 0.
      - r = φ−1: 48 of 264 crossings per second on Quarter sync.
      - r = 0.9: 160 of 360.
      - r = −(φ−1): 284 of 716.
      - Hz ±1000: 1804 to 2004 of 2044 to 2448.
      - Mechanism (entailed from `render()` and `stepM`): the window moves once per sample and the carrier once per substep, so an edge can jump over the carrier. `tryE` only band-limits crossings the carrier makes.
  - **NOT VERIFIED:** nobody has listened. The page says so, and describes no sound as good or bad.
- **A bug the self-check caught, fixed before commit.** The first aliasing meter summed max(0, P1 − P4), and it read −36 dBc on a pure sine. A 0.1% level mismatch on strong partials (the FIR's ripple, and an envelope read 128 samples apart) was being counted as aliasing. The meter now counts only bins at least 6 dB above the reference, and aligns the FIR delay.
- **Alternatives rejected:**
  - **A full copy of `render()`.** It would drift silently. The anchored patch refuses instead.
  - **Hz mode as new code.** It is the oracle's own law.
  - **Rendering one sample at a time and correcting `v.rot` between samples.** It needs no source patch, but it costs a render call per sample and cannot reach the per-voice rotBase.
  - **Fixing the edge band-limiting.** That is `stepM`, not rotation, and out of scope. It is open question 4.
  - **An AnalyserNode spectrum.** It is not deterministic, and cannot see what the worklet renders (memory: browser audio probes are blind).
  - **A one-cycle view.** The brief rules it out.
- **tools/labharness coverage: UNWIRED.** It is not cheap:
  - The in-page checks take about 19 s in Node, and the measurements about 45 s more.
  - A gate would need the morph_editor_check idiom (pinned counts plus planted faults).
  - The self-check lives in the page and prints its verdict in the audit line.
- **Verify:** `./verify fast`, exit 0, git `0616e79` (`.harness/last-verify.json`, 2026-09-27T16:27:53Z). `lab_load_check`: GREEN, 54 labs loaded, 0 broken, 1 skipped. This trace and the index regeneration are committed on top and re-verified.
- **Screenshots** (scratch, not committed; `scratchpad/b296/shots/`), each in light and dark:
  - `01-{light,dark}-rest.png`: the lab at rest.
  - `02-{light,dark}-spectrum-r0618.png` and `03-{light,dark}-spectrum-r05.png`: LIN spectra, off-grid against on-grid.
  - `04-{light,dark}-ab-{B,A}.png`: the A/B control.
  - `05-{light,dark}-presets.png`: the preset list.
  - `06-{light,dark}-measurements.png`: the checks, the measurement tables and the override diff.
- **Open questions:**
  1. Keep the clock at all? This is the human's call, after listening.
  2. Hz, × f0, or both?
  3. Should the ratio range stop short of r = 1 (where the cut freezes, and the edges chatter as r approaches 1)?
  4. A port needs edge detection in window-relative phase in `stepM` (the click table).
  5. Should the per-cycle DC estimate give way to the blocker whenever the clock is on?
  6. Is a second clock for blade 2 worth a control?
  7. Morph behaviour, maximum modulation rate and pitch relation (B275) are not built.
