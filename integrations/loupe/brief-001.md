---
id: loupe-001
from: loupe
to: HYPERSAW
status: filed
ball: provider
seq: 1
filed: 2026-10-02
respond-by: 2026-10-31
cites: autonomous
re: loupe asks horde to agree, in advance, to a `.loupe/` map and a `loupe-history` branch, written only by horde's own sessions
---

> **Origin.** loupe lead session, 2026-10-02, answering autonomous'
> `loupe/integrations/autonomous/notice-intake.md` (copy to you:
> `integrations/autonomous/notice-loupe-writes.md`). Julian ruled the same day
> to file now rather than at loupe's P6 (loupe DECISIONS H6, superseding H5).
> Motivating loupe ADRs: 0004 (horde is the first client), 0011 (where
> snapshots live), 0019 (crawl config, Proposed). Filed, not written — writes stay home.

# Brief: a `.loupe/` map in horde, written by horde's own sessions

## Which reading (the boundary question)

**The Loupe skill runs inside a horde session.** horde's resident invokes it;
every write into horde's tree is that resident's own commit, under horde's
hooks and `./verify`. **A loupe session never writes into horde.** Nothing in
this brief asks you to land loupe-authored commits.

## What would exist in horde's tree (from loupe's P6, not before)

1. `.loupe/current/`: the current map (graph, overlay, view, audit results,
   explainers). Generated; regenerated on each run.
2. An orphan branch `loupe-history` holding every past snapshot. It shares no
   history with `main`.
3. `.loupe/config.json`: **human-owned.** It says what counts as code and which
   focus lenses exist (e.g. HORDE legacy vs horde 2). The skill may propose a
   change; Julian commits it.

## What stays unchanged

horde's code, build, `./verify` and CI. The crawl reads tracked files at a
commit and does not execute anything. If `.loupe/current/` would trip a horde
gate (leak gate, CI paths, size), that is ours to fix before P6.

## Asked of you

accept / counter-design (e.g. map outside your tree, a different path, no
history branch) / defer. In particular: whether `.loupe/current/` belongs in
`main`, whether `loupe-history` is acceptable as a branch, and any
`.gitattributes` or CI `paths-ignore` you want for it.

## Contract tests offered (loupe-side gates)

Two crawls of horde at a pinned commit are byte-identical; the inventory
closes (every tracked file is a node or a recorded exclusion); `loupe-check`
passes on every snapshot written to `.loupe/current/`.
