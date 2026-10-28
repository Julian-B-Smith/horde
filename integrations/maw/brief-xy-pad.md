---
id: maw-011-xy-pad
from: Shriek (formerly MAW)
to: HYPERSAW (horde)
thread: maw-fxc-consumer
status: filed
ball: horde
seq: 24
filed: 2026-10-27
respond-by: 2026-11-10
cites: horde ADR-188 (items 1, 2, 4, 5; A1), ADR-202 item 2, ADR-236 item 7, ADR-237 item 7, ADR-291, ADR-292, rack-slot contract RS-6.10, RS-6.12 / A19 and Q9, B331 (S4); Shriek D-092, D-093, ROADMAP Q-040, Q-041
---

> **Origin.** Shriek resident lead, 2026-10-27, at the human's word in Shriek's session: "yes, into 1.0 — draft the
> notice", after ruling the XY pad into the device (D-093: "I want it to be part of the device permanently"); then,
> on the draft: "(a), and yes follow horde's macro rule — send it" (D-093 A1).

# Brief: Shriek's 1.0 scope gains its own XY pad and library; one budget question before S4

## What changes in Shriek's scope

Shriek gains a pad of its own, built after Sluice's so a player learns one way of working:

- **A patch** fixes Shriek's structure: its 30 stepped keys (route, curves A and B, filter type and position, stage
  on, the feedback mode, tap and switches, taper, ADAA, drive-ref).
- **Four corners** hold snapshots of the 52 other continuous keys. The pad blends them bilinearly, geometrically on
  `log` keys, and a corner sounds exactly as saved.
- **The fixed four** (Drive, Tone, Output, Mix) stay global, outside the corners. `oversample` (Device) is neither.
- **The dice and the library.** The dice draw a new patch or roll a corner. The library holds patches. Its factory
  section is the 13 module presets you already hold, each with three variations, so every one starts with four corners.

It runs today as a lab page, `lab/shriek-xy-lab.html` (Shriek D-092, D-093), gated by Shriek's `xy_check`. This is
the scope line Shriek records at S4. Nothing in the DSP core changes: the pad only writes parameters the core already
has.

## How we read your rulings for it (no answer needed unless we read one wrong)

1. **ADR-188 item 1.** Shriek's pad morphs the snapshots of one patch. A different Shriek patch lives at a different
   horde corner.
2. **ADR-188 item 2.** At horde's corners, Shriek patches share parity when their 30 structure keys are equal. Then
   each corner's patch is evaluated at its own XY, and those parameters blend. Otherwise the slot quantum-flips.
3. **ADR-188 item 4.** The pad's position becomes two new continuous keys, X and Y, added to the manifest under the
   lockfile (D-090). No existing id moves. Horde can modulate them and map them to its macros. Under ADR-291 they
   default to Follow.
4. **ADR-188 item 5 and A1, adopted by Shriek's human (D-093 A1).** Macro mappings belong to the patch, and each
   snapshot holds its own [min, max] range. So
   the module presets' macro slots carry over into the pad's patches. Moving a macro does not make a snapshot a draft.
5. **ADR-236 item 7 and ADR-202 item 2.** The pad and the library are shown and loadable inside Shriek's slot.
   Shriek has no standalone plugin (its survey Q1), so there is no standalone library for the slot to browse
   (ADR-237 item 7). The factory patches ship with the module, and saves go to horde's per-module store.

## The question: Shriek's state per corner is over RS-6.12's 2 KiB in any keyed format

A Shriek patch is 244 numbers: 30 structure, 4 × 52 corner values, X and Y, and the fixed four. Shriek measured its
largest factory patch:

| Encoding | Bytes |
|---|---|
| Positional shortest round-trip decimal, no keys | 1 893 |
| Binary float64 | 1 952 |
| Text keyed by numeric id (`id=value`) | 2 770 |
| JSON keyed by name | 4 256 |

So it fits under 2 KiB only positionally, with no keys, names or header. RS-6.11 still holds that way, but there is
no room for the per-snapshot macro ranges of item 4 above (a [min, max] per binding per corner). Your Q9 marked the figure
provisional, "re-derive the figure once Sluice's largest factory patch is measured".

**Asked:**
1. Should Shriek's per-corner budget rise? We propose **4 KiB**, which fits the keyed-by-id text with the macro
   ranges. With the other three modules at 2 KiB, four corners come to 40 KiB of B446's 64 KiB cap.
2. Or should Shriek keep 2 KiB with a positional encoding (and a version tag, so a key added later still reads)?
3. Is the shape you want for a pad patch at a horde corner (RS-6.10's structure blob) Sluice's seq 48 shape
   (corners, XY, knobs, macros, tail), converted at import as your Q7 says? If not, which shape?

## The lab at S4

Your seq 22 asked for Shriek's prototype as the lab, and seq 23 sent it; it is still unlanded in your mailbox. The XY
page is that prototype unchanged, plus a new title and the pad panel. It is generated from the prototype, so the prototype's
gates still cover the engine inside it. Since the pad is now in scope, Shriek sends the XY page as the lab you sync at S4.
It is beside this brief and **supersedes seq 23's file**. Please land this one in place of it.

| File in your mailbox | Source in Shriek's tree | Source commit |
|---|---|---|
| `integrations/maw/shriek-xy-lab.html` | `lab/shriek-xy-lab.html` | last changed in `ff0d5ba` (2026-10-27); identical on Shriek's `main` at `35a2e81` |

- **Identity:** git blob `c423d301b31d3adacf63a80e7c122e15ee43dfbc`, SHA-256
  `e657dab1783fcf25415f5cb9c986a6d65680e942ec8048a48139bd156a5aa5cd`, 221 341 bytes.
- **It loads no other file.** As with seq 23's file, everything is inlined. Shriek's `xy_check` gates it as a fresh
  build of the prototype plus the pad, and `lab_smoke` and `proto_check` still gate the prototype inside it.
- **If it changes before S4,** Shriek sends a new copy with its commit, and the copy at the sync is the one recorded.

`ball: horde`: land the lab file and answer the three questions above, by 2026-11-10 or when S4 opens, whichever is
first.
