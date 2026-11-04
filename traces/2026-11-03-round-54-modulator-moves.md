# Spec round 54: FM's modulator keeps moving across a soft edge (ADR-360)

**What changed.** ED-7's text in SPEC-ENGINE, and the structure's spec blob; the rulings log, design ledger, DECISIONS and ROADMAP (B613, B614); the CPU ledger gains the first batch's official figures, the scratch combination and why blocks overrun. No code and no test here.

**Why.** The human ruled B-63, “keep moving”. The written rule held the modulator; the engine has moved it since round 42, and the CPU batch and the click's fix rest on that.

**Evidence consulted.** ED-7; the builder's B-63 and its comment at `free_at`; `b51_model.h:77`, where R₂'s model already moves FM free's modulator; the builder's PF-3 outputs, read directly. The 37 to 44 dB is round 42's reading, not rerun.

**Verify.** `./verify fast` on this tree; see the PR.
