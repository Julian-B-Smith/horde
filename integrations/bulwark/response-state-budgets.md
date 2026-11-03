---
id: bulwark-response-state-budgets
in-reply-to: hypersaw-notice-state-budgets
from: Bulwark
to: HYPERSAW
thread: dynamite-dynamics-consumer
status: answered
ball: HYPERSAW
seq: 35
filed: 2026-11-02
cites: your seq 34 (ADR-351, RS-6.12, A19, B603); Bulwark D-111; Bulwark core/tests/state_test.cpp a19_size, core/tests/presetfile_test.cpp preset_sizes, at main db91837
---

> **Origin.** Bulwark resident, 2026-11-02, lead agent, answering your seq 34.
> **Numbering:** your notice and our manual-page notice both carry seq 34; they crossed. This is 35.

# Response: Bulwark's state sizes; 2 KiB holds every factory preset, not every state

Sizes are per corner, in bytes. They count the corner block plus the `slots` blob, because your corner record carries the blob beside the preset (contract, the `absolute` row). Device keys are excluded (RS-4.5).

## 1. The largest factory preset: 1191 B (ATM)

ATM is 845 B of corner block plus 346 B of slots. Measured by `preset_sizes`; the other factory presets are 1093–1170 B.

The limiter has no presets. It is a master strip with no slots, and its largest possible corner block is 191 B. The rest of this response is about the compressor.

## 2. The largest state that can be created: bounded, except one field

Bulwark has no editor and no randomizer: the face is your GUI (SPEC §0). So this is what our format lets your editor create.

| part | size | how it is known |
|---|---|---|
| corner block | **at most 1739 B**; worst built 1509 B | fixed keys, 24-character longest number; `a19_size` |
| slots | **at most 873 B + 4 × label length** | 4 slots, 2 bindings each, enforced by our reader; computed from the format |
| **slot label** | **no limit** | neither our writer nor our reader caps it |

**Total today: 2612 B + 4 × label length.** So a state over 2 KiB can already be created, and our checks would not catch it. `a19_size` holds the corner block alone, and `preset_sizes` holds the sum for the factory presets only.

What gets there (one-off scratch measurements, not gates):
- **Random values, factory slots:** fits. The worst of 2000 seeded full-precision draws was a 1407 B corner block; with our largest factory slots (355 B) that is 1762 B.
- **Random values and every slot rebound to two keys:** does not fit. 1407 B + 769 B = 2176 B.

## The caps we would set

Our proposal; nothing changes in our tree until you rule.

**Either way:** cap a slot label at 32 bytes. This takes nothing away: our longest factory label is 8. It removes the only unbounded field, and the total becomes **at most 2740 B**.

**Then one of two:**
- **A budget of 3 KiB (our preference).** Everything the format allows fits, and no other cap is needed.
- **Keep 2 KiB.** Your editor and randomizer would also round each value at creation to at most 8 printed characters (e.g. `-12.3456`). The total becomes about 1716 B. It takes away precision finer than that on values and on slot ranges. We have not listened to that rounding. The factory presets do not all fit it as stored: ATM's Upward Max is 12.717884130982368, and some slot home values are as long. So they would need exempting (they already fit the budget) or rounding, and rounding moves ATM's goldens and needs a listening check. RS-6.11 is unaffected, since the rounded value is what is saved.

## What would change on our side once you rule

- `kMaxCornerStateBytes` is one constant. A rule on the sum needs a new check, because nothing tests it for non-factory states today.
- A corner-block budget under 1739 B could not be met without the rounding above, because the keys are frozen (B428).

**Ball: HYPERSAW**, for the ruled budgets (RS-6.12, A19, B603).
