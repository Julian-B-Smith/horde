# rslot-width — ADR-340 A1: a readout slot is fixed at the width its header leaves, up to 250 px (B589)

- **Queue item:** B589. GUI session. The self-check failed below a 980 px window. The human first agreed to a fix on the session's claim that it only showed in a narrow browser. Measuring all the slots proved that claim wrong, and the human then chose "Keep them narrower".
- **Number:** an amendment, so `ADR-340 A1`. No new number was needed while #1324 (per-domain prefixes) is open.
- **Evidence consulted:** ADR-340 r4, `.hd-rslot` in the bundle's CSS (`max-width: 100%` inside `.sh-end`), the shell's check 39.
- **Readings:** at 980 px, 13 of 38 slots are under 250 px (narrowest 138 px); at 922 px, 22. In a one-off trial, wrapping headers took Noise from fitting to 12 px of scroll and grew FX by 9 px.
- **Why the slot was at 246-248 px at load but 250 px later, at 922 px:** the value varied by load, and the cause wasn't traced. Check 39 no longer depends on an exact width, so it no longer matters to the gate.
- **A check relaxed, by the human's ruling:** check 39 no longer requires exactly 250 px, only 26 px tall and one size at rest and hot. **A check added:** 39b tests every slot on all 14 pages for one rectangle at rest and hot, at most 250 px, 26 px tall.
- **39b's first readings were wrong twice, and neither was the shell:** it flagged FX · Echo as moved, because a page's first update lays out what sits above its sections, so it now settles each page first; and its control failed because Sub · Colour's summary and hot line are both 23 monospace characters, so it now tries every slot on the page. It also first drove only 7 of 38, because only sized controls drive a slot; it now tries controls until one does. 36 go live. Sub · Voice and Settings · Engine have none and are checked at rest.
- **Master:** wording only, at `1793572935-0ce9`, plus the bank's stale 472 px (ADR-345 r1). No behaviour changed.
- **Checks:** the shell's self-checks, 46 of 46, at both 922 px and 980 px. `./verify fast` exit 0 on this tree. `./verify` doesn't run the browser self-checks, so 39b is in-page only.
- **Not checked:** how the cut-off second line reads with a long modulation readout; the light scheme.
