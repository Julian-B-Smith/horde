# b382-m1-platform-pins — M1's Crunch horde pin is per platform; the cause is a fragile scenario, not M1's coefficient

- **Queue item:** ROADMAP B382, PR #880 (`composed-m1-ksm`), red in CI (Linux, verify-fast):
  `FAIL L4 ... Crunch horde: f07ffc9432bcb49d at the defaults, claimed by M1's moves 57a4d13ff66d7a9f`.
  The lead's message of 2026-09-30 asked for the cause and for honest pins across platforms. This
  follows `traces/2026-09-30-b382-m1-ksm.md` and does not amend it.
- **Cause (measured on this Mac):** the lead's hypothesis was that M1's `ksmC` differs by one ULP between
  macOS and Linux. That is REFUTED as the mechanism:
  - moving `ksmC` by ±1, ±10, ±100, ±1e3 or ±1e4 ULP changes none of the three claimed fingerprints;
  - Crunch horde first flips at +1e5 ULP (about 1e-12 relative), and it flips to exactly f07ffc9432bcb49d,
    CI's Linux hash.

  So the scenario sits next to a discrete event, and a last-bit difference ANYWHERE upstream decides which
  of two realisations it renders. The difference is not in the coefficient. Which function differs on
  Linux is unmeasured: Linux is not available here, and CI uses ubuntu-latest's preinstalled Node against
  24.10.0 here.
  - The all-off pins pass on Linux: their realisation is not near such an event.
  - Cross-mod roar and Zap bass matched on Linux; L4 names only Crunch horde. They flip only at 1e5 and
    1e6 ULP respectively.
- **What changed:**
  - `divergence_ledger_check`: a `moves` value may be {platform: fingerprint}. L4 compares against the pin
    for `process.platform`, and a claim with no pin for this platform is printed as `SKIP L4 ... NOT
    proven here`, never passed.
  - New control C8: this platform's pin off by one digit must be caught, and the claim read as an unpinned
    platform ('aix') must be skipped, not passed.
  - The M1 entry pins Crunch horde as darwin 57a4d13ff66d7a9f and linux f07ffc9432bcb49d, and its `finding`
    records (3).
  - No pin is removed. Crunch horde does move at the defaults on both platforms, so L4 must still see it
    claimed. The same claim ("M1 moves pinned presets at 48 kHz") is proven platform-robustly by Cross-mod
    roar and Zap bass.
- **M1p (composed_engine_check, verify full, macOS only):** the same perturbation probe on its renders
  finds all of them fragile at some scale. ZERO horde rows flips at ±100 ULP, ZERO Two blades at -1e4,
  AA0 Crunch horde / Ring saw / Glass horde pad at 1e5, and AA0 Cross-mod roar / Zap bass at 1e5–1e6.
  They are float32-hash pins of chaotic swarms. They hold on the canonical macOS platform where verify
  full runs, but they are NOT platform-portable. The pre-B382 AA0/ZERO pins are the same kind of render.
- **Alternatives rejected:**
  - An exactly reproducible coefficient (a rational literal table). It would not help, because the
    coefficient is not the cause.
  - Dropping Crunch horde. L4 would then read its moved default as unclaimed.
- **Verify:** `./verify fast` on this commit, reported verbatim to the lead with CI's verify-fast result.
- **Open questions:**
  - Which upstream function differs on Linux.
  - Whether pinned-hash scenarios should be screened for this fragility, by the ULP probe above, before
    they are pinned.
