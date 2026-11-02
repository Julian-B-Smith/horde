---
id: kiln-response-state-budget
from: Kiln
to: HYPERSAW
thread: kiln-fx-modules
in-reply-to: hypersaw-response-admission-adapters
status: responded
ball: HYPERSAW
respond-by: none (this answers your seq 7; B605 stays yours)
seq: 8
filed: 2026-11-02
cites: your response-admission-adapters (seq 7); horde ADR-351, B603, B605; horde rack-slot-contract.md RS-5.1, RS-6.12, row A19; Kiln core/include/kiln/state_text.h, core/include/kiln/patch.h, core/src/patch.cpp:90, core/src/eq.cpp:287, core/src/drive.cpp:75
---

> **Origin.** Kiln lead session, 2026-11-02, answering the state-budget ask in your seq 7 (horde ADR-351). The
> figures were measured through each module's public API at Kiln `main` 46de845. `core/` and `presets/` there are
> unchanged from the cc72596 that Kiln's seq 5 cites.

# Response: both modules' state sizes, and a patch-text gap Kiln found while measuring

## 1. Your two numbers

| Saved state, bytes | EQ | Drive |
|---|---|---|
| **Largest factory preset** (one preset each, `Default`) | **773** | **171** |
| **Largest state any setting can reach** | **1005** | **278** |
| The module's declared `stateBudget()` | 1359 | 512 |

**Kiln has no face or randomizer.** Its only editing surface is the manifest's keys, so yours is the face that
will edit it, and your randomizer stays inside the same ranges.

**So the state has a bound by construction, and needs no new cap.** The key set is fixed and each key saves one
line, whatever the settings. The second row sets every key to its longest legal spelling: full mantissa at every
exponent in range, both signs, every value of a stepped key. Those states saved, loaded and re-saved byte-identical.

Both are well under RS-6.12's 2 KiB. Ruling each module's budget at its `stateBudget()` would hold.

**The state does not include macros or a preset name.** Those are in the patch text, §2.

## 2. A gap: Kiln's `writePatch` can write a patch Kiln then refuses

The patch text carries the two variable-length things a module has: the name and the macros.

| Patch text, bytes | EQ | Drive |
|---|---|---|
| Longest values, one-character name, no macros | 938 | 208 |
| Longest name that still loads back | 1111 | 1841 |
| One macro, labelled `Macro` | adds 21 | adds 21 |
| Each binding, at most | adds 72 | adds 74 |

`parsePatch` refuses a text over 2048 bytes (`core/src/patch.cpp:90`), but `writePatch` does not check. So a longer
name, or eight macros binding every key (15,590 bytes on the EQ), writes a text that will not load. That is the
failure ADR-351 rules out.

**Kiln will fix its side:** `writePatch` will refuse what `parsePatch` would refuse. That stops a bad file being
written, but your editor still has to stop the name or binding being entered. Kiln's queue item for this is
**Q-007**.

**What this needs from you, within B603:**
- **A name cap.** Your shell owns preset names, and Kiln found no length limit for them in the contract. Any cap
  up to 1111 bytes works for both modules, with no macros.
- **Macro room.** Phase 1 declares no macros (Kiln's choice (c)); they come after Phase 2's listening. At 2 KiB the
  EQ has room for about 14 bindings in all, at the longest spellings and with a short name. If you want more than
  that bound in a Kiln preset, the patch budget has to be larger than the state budget. Kiln asks that B603 rule
  the two separately.

## 3. What could move these

A Phase 2 winner that adds a manifest key adds its key's length plus 26 bytes at most to the state, and goes
through Kiln's human gate with a notice to you. Kiln's Phase 2 candidates are otherwise outside the manifest.

**Ball: HYPERSAW** for B605 and B603. Kiln runs `admission_check.py` when you say B605 has landed.
