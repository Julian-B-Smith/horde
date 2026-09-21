# subosc-morph-and-init — the engine blocks' Structural rows join the morph field, and every load initialises first

- **Queue item:** B195 (Part A) and B192 / B183 (Part B)
- **Why:** The human, 2026-09-21: (a) "some Sub Osc parameters don't reach
  morph" and (b) "let's make sure there's a factory Init patch and that
  everything initializes as the first step before a load." Part A rules that an
  engine block's membership of the morph field is the SAME rule as the
  instrument table's — Morphable and Structural join, Device stays out — so the
  sub's stepped rows stop being an asymmetry. Part B replaces "every loop
  defaults its absent keys" (three loops in `applyStateJson`, and a fourth in
  `state_load` that never got it — B183) with ONE rule, `initState`, run as the
  first act of both load paths, and ships the `Init` patch the bank never had.

## What this session inherited, and what it did with it

This work resumes two commits a previous agent left unverified. Judged line by
line; the verdict per piece:

**KEPT, verified correct.** `7eb257b` (Part A) did the thing the risk lived in:
it appended the Structural rows in a SECOND pass over `kEngineBlocks` rather
than widening the class test in place. Pass 1 is textually the loop that was
there before, at the same point in `morphInit`, so the first 264 slots are
produced by identical code and nothing before them changed — the diff of
`morphInit` is a comment block plus one appended loop and nothing else. The
positional gate `T10b` then measures it: "the 8 Structural engine ids are the
TAIL of the order (first at slot 264 of 272) — no previously stored slot
moved". Its T10/T10c/T11a/b/c rows are well-built (T10c is a must-report-one
control, T11c a calibration that proves T11b's detector could have seen an
interpolated wave) and are kept as written.

**KEPT, verified correct.** `0e02c81` (Part B WIP): `initState`, its two call
sites, and `state_check`'s Q1–Q5. The corpus is the part that matters and the
WIP got it right — holes punched into a real chunk, plus the named `sub.` anchor
— and it is measured RED below on the unfixed binary. Kept as written.

**CORRECTED.** The `kQCap` 1024 → 2048 comment claimed headroom of "~1740 of
2048", a number that was never measured, and argued the doubling from the
previous comment's claim rather than from evidence. Measured instead (temporary
counter in `enqueueParam`, reverted): peak in-flight depth 1471 of 2048 over
state_check's corpus, and holding the cap at 1024 with this change set turns
state_check RED at "B100: unknown JSON header keys ignored, params still apply"
— so the doubling is load-bearing, not precautionary. The comment now says
that, and additionally names a PRE-EXISTING artifact the next reader would
otherwise blame on this change: a rig that keeps `processing` true and loads
without calling `process()` never drains and saturates whatever `kQCap` is —
`morphlayout_check` hits exactly 1024 on main and exactly 2048 here. No rig
assertion depends on the drops; raising the cap cannot fix a rig that never
drains.

**CORRECTED.** The `morphInit` comment listed seven stepped sub rows; there are
eight. The eighth is the retired `sync` (4011), which is Structural like the
rest and joins with them because membership is the class, not a list of rows
someone judged interesting. Seven in prose against T10b's eight is exactly the
kind of drift that costs the next reader an hour.

**ADDED (the WIP had none of this).** The factory `Init` patch, and the gate
gap that would have let it ship untested.

**DISCARDED:** nothing.

## Part A — the ordering, and whether the marker had to move

*Ordering, proven, not assumed.* Two independent grounds: (1) the code — pass 1
is the pre-B195 loop unchanged and everything appending before it is unchanged,
so slots 0..263 are byte-for-byte the order they were; (2) the gate — T10b
asserts positionally that every Structural engine id lives in the last
`structural` slots, with the first at 264 of 272. The block's GATE (`sub.on`,
4015, Device) is still out, by the class test and not by a list; T10 asserts
both directions over ADR-088's whole engine span, so STATION inherits the
coverage the day its block joins `kEngineBlocks` — the span is the contract,
today's block list is an accident.

*Did the layout marker have to move from 7 to 8?* **No — not to READ.** Evidence:
`morphSlotMap` (src/hypersaw_clap.cpp:5639-5645) returns the identity-prefix map
for every `layout >= 2` regardless of the number, and `resetCorner` defaults
every slot before filling, so a stored 264-entry layout-7 array loads into the
272-entry order with its eight new slots at their defaults whether the marker
moved or not. The bump was taken anyway, and is kept, because the repo's own
rule at `cornerJson` is "one bump per appending change" — the marker's job is to
NAME an order, and B175's cross-layout remap will have to ask which order an
array was written in. So: not required for correctness, required by the stated
convention. `bank_check`'s pin moved with its reason at the pin.

## Part B — the corpus is the work

**The gate was proven RED before the change, and the corpus is why.** Method:
rebuild with `initState` removed from both paths and `state_load`'s five
hand-listed chunk resets restored — i.e. the exact pre-B192 behaviour, not a
strawman. Result, verbatim from that binary:

```
FAIL Q2 a HOST CHUNK with holes loads byte-identically into a fresh and a disturbed instance (B183: a load is a load on the session path too)
FAIL Q3 a chunk that says nothing about the SUB block loads it OFF (its default), not ON — the report, on the session path
FAIL Q5 a JSON patch with NO morph chunk leaves corner D at its defaults, not at what the previous patch authored
state_check: RED (3 failures)
```

**Which corpus made it red, named (L0059).** Q2/Q3 run on a HOST CHUNK — a real
`state_save` blob with every 7th `key=value` line cut out (53 of 372 punched),
and for Q3 with every `sub.` line stripped and the gate switched on by hand.
Q5 runs on a JSON patch with its whole `morph` chunk truncated off — a patch
from before corners existed. On the SAME unfixed binary:

- `bank_check docs/presets/factory` → **0 failure(s)**. The factory bank names
  every key, so skip-the-absent-key and default-the-absent-key agree everywhere
  on it. A bank-only corpus certifies nothing here, exactly as L0059 says.
- `state_check`'s own pre-existing row "missing keys keep defaults; unknown keys
  ignored" → **OK** on the unfixed binary. A row that sounds like the assertion
  and is not it.

**The reach a key-by-key default does not have.** Q5 is the demonstration:
B181 note 6 defaults an absent *parameter* key, and there is no key at all for
a chunk a patch never wrote. `initState` resets the four corners (reusing
`resetCorner`, which was already the "back to per-slot defaults" routine), the
corner names, the exempt set, `morphCornersAuthored`, the mod routes, the
intent chunk, the preset name and the engine revision, so a patch that predates
any of them loads the way it reads.

*Reuse, and what it misses, stated:* the defaults come from `defaultFor`, the
same function that serves the GUI's `SHELL_DEFAULTS` — one source, not a second
table of every default free to drift. `resetCorner` was reused as-is and is
complete for its slot array. Two deliberate NON-resets, named rather than
silently skipped: id 178 (`specimen`, ADR-147 says it is not patch state and
both load paths already skip it), and `ens=` / `lfo=`, which are RNG stream
continuations rather than patch values — whether a stream's identity should
survive a load is a separate question and no row asks it yet. `chunkOnlyState`
gates the routing-matrix reset to the transport that can restore it: the host
chunk carries `routing=`, `stateJson` does not (B193), so resetting the matrix
on a preset load would delete a topology the patch cannot put back. That flag
retires the day B193 puts routing in `stateJson`.

**The `Init` patch, and the gate gap it exposed.** `init/INIT - Init.json`,
generated by `gen_factory_bank` from an EMPTY parameter table: the generator
boots a fresh instance and saves what the shell holds, so the file is the
shell's own defaults written by the shell's own writer, and re-running the
generator after any default change carries it along for nothing. A hand-listed
init table would be a second statement of every default, free to drift
silently.

Adding it surfaced a real gap, and this is the finding of the session that was
not in either brief: **`bank_check` carried its own hand-written list of eight
category directories.** CMake globs the whole tree, so the new patch was
embedded, installed and shipped — while every row of `bank_check` (loads,
re-saves identically, makes sound, no NaN, loads identically into a disturbed
instance) simply never ran on it. Measured: `grep -c applies` was 40 with the
new patch present. `bank_check` now DISCOVERS the category directories, with
`corners/` the single, real exclusion; it reports 41 patches and the Init patch
picks up its rows (`init/INIT - Init: RMS -22.0 dBFS > -60, peak 0.304 < 1.0`).
This is adding a check, not weakening one — ADR-180 §1.

- **Evidence consulted:** `src/hypersaw_clap.cpp` (`morphInit` ~2879-2992,
  `morphSlotMap` 5639, `cornerJson` ~5449, `initState`, `applyStateJson`,
  `state_load`, `state_save` ~8290-8380, `kSubOscParams` 1238-1300,
  `enqueueParam` 4817); `tools/morphlayout_check.cpp`, `tools/state_check.cpp`,
  `tools/bank_check.cpp`, `tools/gen_factory_bank.cpp`; `CMakeLists.txt` 55-72
  (the bank glob); `./verify` (gate list); the two inherited commits `7eb257b`
  and `0e02c81` and their messages; LIBRARY L0059, L0051, L0032, L0036.
- **Alternatives rejected:**
  - *Widening the class test in place (one loop).* Rejected — it interleaves the
    newly-admitted ids in block order and shifts every stored slot after the
    first of them. This is the defect the brief warned about; the inherited
    commit had already avoided it.
  - *Leaving the layout marker at 7.* Defensible and would have spared the bank
    re-save, but it breaks the repo's own "one bump per appending change" rule
    and leaves B175 unable to tell the two orders apart. Rejected.
  - *Adding `"init"` to `bank_check`'s category list.* Rejected in favour of
    discovering the directories: a gate whose corpus is a second hand-list of
    what the product contains cannot see anything new, which is the one thing a
    gate is for. The next category would have repeated the bug.
  - *Deleting `applyStateJson`'s per-key defaulting now that `initState` runs.*
    Rejected — it is also `presetMatches`' rule (B174 moved it into
    `jsonNumber`), and two independent statements of "absent means default" is
    the cheap kind of redundancy.
  - *Fixing `morphlayout_check`'s queue saturation.* Out of scope and not this
    change's doing — measured identical on main at its own cap.
- **Verify:** `./verify full`, exit 0, git `bb755fa` per `.harness/last-verify.json`
  — the first `verify full` ever run against this work. (`7eb257b` and `0e02c81`
  were committed with the ledger still naming main's commit; that is what made
  every claim in them a proposal rather than a result.) The follow-up commit
  that fills in this hash is re-verified the same way.
- **Open questions:**
  1. `ens=` / `lfo=` survive a load by design (named above). Whether an RNG
     stream's continuation should be reset by "a load is a load" is a ruling
     nobody has made; no row asks it, so no row answers it.
  2. `chunkOnlyState` is a flag that exists only because `stateJson` omits the
     routing matrix. It should be deleted when B193 lands, not carried.
  3. The factory bank's `build=` stamp churns on every regeneration (41 files
     changed here for that field alone). It is dropped from every comparison
     the gates make, so it is diff noise rather than state — but it is noise on
     every future bank change too.
  4. `bank_check` now discovers categories; `gen_factory_bank`'s BANK.md still
     has a hand-listed `cats[]` (a patch in an unlisted category ships and is
     merely undocumented, which is why it was left). Worth the same treatment
     one day.
