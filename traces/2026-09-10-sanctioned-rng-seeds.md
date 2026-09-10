# sanctioned-rng-seeds — seed the three outstanding unseeded RNGs

- **Queue item:** the three sanctioned prototype edits named in CLAUDE.md §Domain
  "Protected paths" (ADR-091 CANTO, ADR-122 STATION, ADR-152 intent bus) —
  dispatched by the HYPERSAW lead organ, 2026-09-10, brief "the three
  sanctioned prototype edits: seed the unseeded RNGs".
- **Why:** each of `reference/intent-bus.html`, `reference/formant-pulsar-fof.html`,
  `reference/station.html` carries exactly one standing sanctioned edit — seed its
  `Math.random()` — as the precondition for CANTO/STATION to leave candidate
  status and for the intent bus's reshuffle to be reproducible per
  `specs/SPEC-INTENT-BUS.md` §3.5. This dispatch closes all three in one pass,
  each edit isolated to its own file.

## `Math.random` site inventory (full-repo grep of the three files, verbatim)

```
reference/intent-bus.html:176:function reshuffle(){ P.forEach(p=>S.seeds[p]=Math.random()); S.seeds.home=Math.random(); S.seeds.unison=Math.random(); }
reference/station.html:167:  RND:i=>Math.floor(Math.random()*16),
reference/formant-pulsar-fof.html:197:          if(Math.random()<p.mask) continue;
```

Exactly one site per file — matching the brief's expectation. All three are in
an audio/state path (flip-topology seeding, Wave RAM randomize, masking
hazard), none cosmetic. **No site was left alone** — there was no third
category to report (no UI-colour/demo-animation `Math.random` present in any
of the three files).

## Replacements (one per file, nothing else touched)

All three now share the identical mulberry32 one-liner already used across
`docs/design/*.html` (e.g. `docs/design/morph-law-bench.html:211`,
`docs/design/shape-lab-mod.html:117`) — copied verbatim, not retyped:

```js
function mulberry32(a){return function(){a|=0;a=a+0x6D2B79F5|0;let t=Math.imul(a^a>>>15,1|a);t=t+Math.imul(t^t>>>7,61|t)^t;return((t^t>>>14)>>>0)/4294967296;};}
```

**Note on the brief's pointer:** the brief said to copy this form from
`reference/swarmsaw.html`; `grep -rln mulberry32 reference/` shows it is not
actually present there — the canonical form lives in `docs/design/*.html`
instead (verified: `docs/design/morph-law-bench.html:211`,
`docs/design/shape-lab-mod.html:117`, `docs/design/fx-morph-law-lab.html:179`,
all byte-identical). Used that source instead; flagging the stale pointer for
the lead to correct in the next brief that cites it.

1. **`reference/intent-bus.html`** — `reshuffle()` (line 176 pre-edit) now
   draws from a mulberry32 stream seeded by `S.seed` (new field on the device
   state object, default `1024`, next to `S.seeds` per SPEC-INTENT-BUS.md
   §3.5 "seeds persist with device state"). A `<input id="seed" type="number">`
   next to the Reshuffle button (line 79) is wired to `S.seed`. Same seed +
   press → identical `S.seeds`; a new seed value → a new flip topology.
   **Spec-interaction note (for the lead, possible ADR-152 amendment text):**
   SPEC-INTENT-BUS.md §3.5 T9 currently reads "Reshuffle: flip points change"
   as an unconditional expectation of every Reshuffle click. Under the seeded
   behavior, clicking Reshuffle twice with an *unchanged* seed now
   reproduces the same topology by design (that reproducibility is the whole
   point of the sanctioned edit) — a new topology now requires changing the
   seed value first. Suggested amendment: "T9 (seeds): ... Reshuffle with an
   unchanged seed reproduces the same flip topology; changing the seed and
   reshuffling produces a new one." Left as a recommendation only — T9's text
   lives in `specs/SPEC-INTENT-BUS.md`, out of this dispatch's scope.

2. **`reference/formant-pulsar-fof.html`** — `FormantCore`'s masking draw
   (line 197 pre-edit, `if(Math.random()<p.mask) continue;`) now reads
   `if(this.rnd()<p.mask) continue;`, `this.rnd` a mulberry32 stream owned by
   the core instance, seeded from `this.p.seed` (new param, default `1024`,
   matching the existing `this.p` defaults object). A `param` message with
   `k:'seed'` reseeds `this.rnd` (mirrors how every other `this.p` field is
   already live-updated via `msg()`). A `<input id="seed" type="range">` was
   added to the masking card (next to burst on/off) and wired through the
   file's existing generic `input[type=range]` → `postMessage({type:'param',...})`
   plumbing (`pushParam`), so no new event-wiring code was needed for the UI
   side — reduce, don't invent a second mechanism.

3. **`reference/station.html`** — the Wave RAM randomize preset (line 167
   pre-edit, `RND:i=>Math.floor(Math.random()*16)`) now reads
   `RND:i=>Math.floor(rndStream()*16)`, where `rndStream` is a module-level
   mulberry32 stream reseeded from `state.seed` (new field, default `1024`)
   immediately before each RND-button click's 32-cell table generation (so
   one click = one reproducible draw of the whole table, not one free call
   per cell drifting against a shared un-reset stream). A
   `<input id="rndSeed" type="number">` next to the wave-preset buttons
   (line 109 area) is wired to `state.seed`.

## Verification

### Lab-load check
```
$ node tools/labharness/lab_load_check.mjs reference/intent-bus.html reference/formant-pulsar-fof.html reference/station.html
FAIL  intent-bus.html  script block 1: ReferenceError: Option is not defined
        intent-bus.html#script1:283 | P.forEach(p=>{ cLfoT.add(new Option(PN[p],p)); }); |                              ^
FAIL  formant-pulsar-fof.html  script block 1: ReferenceError: devicePixelRatio is not defined
        formant-pulsar-fof.html#script1:239 | function fit(c){const r=c.getBoundingClientRect();if(c.width!==Math.floor(r.width*devicePixelRatio)){c.width=Math.floor(r.width*devicePixelRatio);c.height=Math.floor(parseInt(c.getAttribute('height'))*devicePixelRatio);}return c.getContext('2d');} |                                                                                   ^
OK    station.html

RED — 3 labs loaded, 2 broken, 0 skipped
```
Both failures are **pre-existing**, not introduced by this change — confirmed
by re-running the identical check against the pre-edit files (`git stash`):
same two failures, same messages (only the reported line numbers shift, by
exactly the number of lines this dispatch added above each throw site).
`Option` (used by `new Option(...)` for a `<select>`) and `devicePixelRatio`
are real browser globals the checker's stub sandbox
(`tools/labharness/lab_load_check.mjs`) does not provide — unrelated to
`Math.random`/mulberry32 and out of this dispatch's scope (no edit made to
either site). `station.html` was already clean and remains clean.

### Determinism (Node vm harnesses, one per file, run against the exact shipped
lines extracted verbatim by line number — not retyped — from each file)

**intent-bus.html — `reshuffle()`:**
```
seed=1024 run A: {"detune":0.3682216997258365,"width":0.28839943348430097,"sub":0.8307245629839599,"cutoff":0.3829720092471689,"reso":0.7636703725438565,"drive":0.2516439822502434,"delay":0.3828093472402543,"lfoRate":0.7753798109479249,"home":0.7567864344455302,"unison":0.08268806454725564}
seed=1024 run B: {"detune":0.3682216997258365,"width":0.28839943348430097,"sub":0.8307245629839599,"cutoff":0.3829720092471689,"reso":0.7636703725438565,"drive":0.2516439822502434,"delay":0.3828093472402543,"lfoRate":0.7753798109479249,"home":0.7567864344455302,"unison":0.08268806454725564}
seed=2048 run C: {"detune":0.8132568097207695,"width":0.9656568083446473,"sub":0.124044309835881,"cutoff":0.1343851420097053,"reso":0.17017259332351387,"drive":0.5252706371247768,"delay":0.9906456000171602,"lfoRate":0.014497517375275493,"home":0.6984088097233325,"unison":0.12040519015863538}
same seed -> identical output: true
different seed -> different output: true
PASS intent-bus.html reshuffle() determinism
```

**formant-pulsar-fof.html — `FormantCore.rnd` (masking):**
```
seed=1024 run A[0..4]: [ 0.3682216997258365, 0.28839943348430097, 0.8307245629839599, 0.3829720092471689, 0.7636703725438565 ]
seed=1024 run B[0..4]: [ 0.3682216997258365, 0.28839943348430097, 0.8307245629839599, 0.3829720092471689, 0.7636703725438565 ]
seed=2048 run C[0..4]: [ 0.8132568097207695, 0.9656568083446473, 0.124044309835881, 0.1343851420097053, 0.17017259332351387 ]
same seed -> identical output: true
different seed -> different output: true
default-seed (no msg) run1 == run2: true
default-seed run1 == explicit seed=1024 run: true
PASS formant-pulsar-fof.html FormantCore masking RNG determinism
```

**station.html — Wave RAM `TP.RND`:**
```
seed=1024 table A: 5,4,13,6,12,4,6,12,12,1,8,0,2,15,8,6,5,3,1,2,10,14,11,4,7,4,4,3,2,1,1,5
seed=1024 table B: 5,4,13,6,12,4,6,12,12,1,8,0,2,15,8,6,5,3,1,2,10,14,11,4,7,4,4,3,2,1,1,5
seed=2048 table C: 13,15,1,2,2,8,15,0,11,1,15,2,14,4,8,11,0,5,4,5,2,4,10,2,9,6,9,9,5,6,8,12
same seed -> identical output: true
different seed -> different output: true
PASS station.html Wave RAM RND determinism
```

Harness scripts were written to the session scratchpad (not committed — they
are ad hoc verification, not a repo tool; the brief's file scope names only
the three edited files plus this trace) and modelled on
`tools/labharness/lab_load_check.mjs`'s vm-sandbox approach: each extracts the
exact shipped lines by line number (verified against a regex guard before
running, so a future edit that shifts the lines fails loud instead of quietly
testing stale code) and executes them in `node:vm`, never a hand-retyped copy.

### `./verify fast`
```
verify: .leakcheck-names absent — private-name leak check SKIPPED (expected off this Mac)
mailbox_delivery: no sibling mailbox checked out — SKIPPED
presentation_check: GREEN (323 rows, scopes: global, osc1, osc2; 34 undesigned (no chunk named), 5 ungrouped)
  note  engine guards law==0 but no parameter declares it
  note  engine guards law==1 but no parameter declares it
depends_check: GREEN (121 declared dependencies, header current, 2 advisory)
gen_gui_controls: GREEN (197 generated control(s), gui2 markup current)
test_table_check: GREEN (187 tests — 103 agentic, 84 human; 14 awaiting an oracle)
  gui.html     reaches 102 / 241 params
  gui2.html    reaches 224 / 241 params
  exempt: inertiaCurve — dev-only, labelled (dev) in the param table
  patch-scope params (raw-id dispatch, must be data-fixed): 87
gui_reach: GREEN (every declared param is reachable in some GUI)
```
Exit 0. `.harness/last-verify.json`: `{"target":"fast","exit":0,"git":"638dfcc",...}`
(git hash pre-dates this change set, as expected — this dispatch's commit is
made after this trace).

### Machine identity / alias discipline
`git diff | grep -in "<author-name>|<machine-user>|/Users/|<terrain-sibling>|\.local"` — no
matches. No tracked-file leak introduced.

## Alternatives rejected
- Auto-incrementing the seed on every Reshuffle/RND-button click (so a bare
  click always looks "randomized," matching old `Math.random()` UX) — rejected
  as inventing behavior the brief did not ask for; the brief's literal ask
  ("same seed → same flip topology") is satisfied by a user-editable seed
  field alone, and reduce-never-invent favors the smaller surface. Flagged as
  the T9-wording tension above for the lead to resolve explicitly.
- Retyping the vm-harness snippets by hand instead of extracting by line
  number from the real files — rejected: line-number extraction with a regex
  guard is strictly more trustworthy evidence that the *shipped* code is
  deterministic, not a reimplementation that could silently diverge.

## Verify
- Target: `fast`. Exit: `0`. Git hash before this dispatch's commit: `638dfcc`
  (from `.harness/last-verify.json`).

## Open questions
- The T9 wording tension in `specs/SPEC-INTENT-BUS.md` §3.5 (see above) —
  needs a lead decision/ADR-152 amendment, out of this dispatch's scope
  (specs/** excluded).
- The brief's pointer to `reference/swarmsaw.html` for the mulberry32 source
  was stale (function not present there); used `docs/design/*.html` instead,
  which carries the byte-identical form. Worth correcting in future briefs
  that cite it.
