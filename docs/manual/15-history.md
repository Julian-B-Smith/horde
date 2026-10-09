# History (undo)

Purpose: explain horde's history: a tree of every edit, one step per gesture, that never loses a
branch when you go back and try something else. The chapter covers what counts as a step, undo
and redo, the history page, how history survives preset changes, morphing, modulation, mapping
changes, signal-flow changes and FX edits, and how it relates to the DAW's own undo.

## What history records

Which actions make a history step and which do not, including whether modulated values in
motion count.

Status: PENDING — B389

[[TAB:history.recorded-actions]]

## Undo and redo

Stepping back and forward, by shortcut and by button.

Status: PENDING — B389, B302

## The history tree

Going back and making a new edit starts a branch; the old branch stays.

Status: PENDING — B389, ADR-160

[[FIG:history.tree]]

[[FIG:history.page]]

## What history survives

Preset changes, morphing, modulation, mapping changes, signal-flow changes and FX module edits.

Status: PENDING — B389, B222

[[TAB:history.survives]]

## History and your DAW's undo

What horde's history covers that the DAW's undo does not, and the reverse.

Status: PENDING — B389
