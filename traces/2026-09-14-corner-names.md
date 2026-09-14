# 2026-09-14 — B122: corner preset names in the shell; history labels; the asterisk

**Asks (human).** Undo the selection or capture of a corner preset; an asterisk
when a corner's settings differ from its preset.

**Finding.** Corner VALUES were already snapshotted (morphJson rides
stateJson) and restored; the loaded NAME lived only in the GUI dropdown, so
an undo restored the values and the dropdown kept lying — "doesn't handle
presets elegantly".

**Shell.** `cornerName[4]`; `setCornerName` (relabels the pending "corner X
loaded" mark to "corner X ← name" / "corner X captured"); `cornerNamesJson`
stamped in `morphJson` as `"cornerNames"`; parsed in `applyMorphChunk`
(absent → cleared); `cornerMatches(k, json)` through `morphSlotMap`; binds
`hzMorphCornerNames` / `hzMorphCornerName` / `hzMorphCornerDirty`; three
headless exports for the check.

**GUI.** Load → name to the shell; capture → "" ; `syncCornerNames()` on
every `syncFromEngine`: dropdown value follows the shell, asterisk from the
shell's dirty answer (preset JSON cached per name, dropped on store refresh).

**Oracle.** `morphlayout_check` T7a–e PASS; `./verify full` in the PR body.
State format: one append-only key in the morph chunk; tolerant parse.
