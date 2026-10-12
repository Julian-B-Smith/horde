# Flush-to-zero in horde 2: what the ruling changes, and what it leaves open

> **Origin.** horde lead session, briefs dated 2026-10-11, for ROADMAP B448 (Blind-Spot Armor).
> The human ruled flush-to-zero on in the horde 2 shell in ADR-211 item 1, closing ADR-209
> item 6, and made the re-pin of eight self-digest rows "its own dispatch". This page is that
> dispatch's account. An implementer wrote it from the first brief; a second implementer
> reworked it the same day from a second brief, after a critic's review of the first commit.
> A third brief, dated 2026-10-12, carried the human's choice on the rounding mode (section 8,
> option A with the count from option B; the lead records it as ADR-213). A third implementer
> built it and rewrote sections 2, 5, 6, 7, 8 and 9 where that choice changes them.

**Status: the re-pin is made, every gate that replays the engine runs it with the mode on, and
since 2026-10-12 the same switch forces round-to-nearest inside its scope. Two things wait for
the human: how the eight rows stand under ADR-187 (section 4), and a run on Windows (section
7).** Last verified 2026-10-12 on the dev Mac (darwin-arm64, Apple clang 16, Node 24), on the
branch this page's third revision arrives with.

Words used below. **Measured** means a command was run for this page and its output read;
where an earlier dispatch ran it and a later one did not, the sentence says so. **Read** means
the answer comes from reading the code. **Argued** means reasoning from documentation.

## 1. What flush-to-zero is, and what was ruled

Flush-to-zero is a CPU mode. With it on, arithmetic that would produce a subnormal number (one
smaller than about 2.2e-308) produces zero. It stops a near-silent tail from costing many times
the CPU of a loud one on some processors.

ADR-211 item 1: the horde 2 shell runs with it on from its first build. The shell does not exist
yet (B398). What exists is the engine, `h2/engine/`, the parity check that holds it to the JS
reference, and two reference files that pin its output bit for bit over 543 scenarios
(`h2/engine/selfdigest.parity.*` and `selfdigest.product.*`). The ruling means the engine that
ships runs in the mode, so the engine that is judged and pinned must run in it too.

A second ruling followed on 2026-10-12: the switch that sets the mode also forces
round-to-nearest inside its scope and gives the host its own rounding mode back (section 8).

## 2. Every gate that replays the engine, and the mode it runs in

The first commit switched the mode on inside the self-digest tool only. The critic's finding
(H1): the self-digests then pinned the mode-on engine while the parity check, the one gate that
ties the C++ to the JS, still judged the mode-off engine. Nothing tied the two together.

**The fix: the switch is inside `h2engine_stream::replay` (`tools/h2_engine_stream.h`), the one
function every tool replays a scenario with.** No tool sets the mode for itself any more, and
none can replay with it off by leaving something out.

| Gate | What it compares | Mode it runs in | The lines that prove it | Verdict (measured 2026-10-12, after the rounding change) |
|---|---|---|---|---|
| `h2_engine_parity_check` | The engine against the composed JS: rms and largest difference under 1e-6, identical events and readouts, mean bit-exact share at least 30%. | ON and rounding to nearest, every replay (scenarios, controls, determinism rows) | `PASS  control FZ ... 753 of 753 replays ran with it on`, `PASS  control ROUND ... 753 of 753 replays ran rounding to nearest`, `PASS  control WORD` | GREEN. 540 of 543 at parity, 0 red, mean bit-exact share 37.00%. |
| `h2_engine_fma_control` | The same source, fused build. It must FAIL parity. | The same | The same three lines, 543 of 543. A failed line makes it exit 2 (the run broke), never "fired". | FIRED. 24 of 543 miss parity. |
| `parity_floor_check --stream` | That the parity check refuses a stream with scenarios dropped. | The same (it runs the parity binary) | The parity binary's own. | GREEN, 4 controls red. |
| `h2_engine_selfdigest_check` (parity build) | The engine against its pinned bits. Bit-exact. | The same | `PASS  control FZ ... 543 of 543`, `PASS  control ROUND ... 543 of 543`, `PASS  control WORD` | GREEN. 543 of 543, 0 differ. |
| `h2_engine_selfdigest_product` (release flags) | The same, for the release build. | The same | The same three lines. | GREEN. 543 of 543, 0 differ. |
| `h2_engine_rtsan_check.py` (RealtimeSanitizer) | Every engine call inside a realtime scope; the hooked replay against `replay`, bit for bit. | The same, in both replays, and the switch itself runs inside the realtime scope around every engine call | The same three lines over 1086 replays, then `every replay ran with flush-to-zero on and rounding to nearest`; otherwise it reports "judged 0", which is red. | GREEN. 543 of 543 judged, 0 violations, the planted allocation reported. |
| `measure_h2_engine` (a CPU bench, in no gate) | Nothing: it times renders. | Its stream-replay mode goes through `replay`. Its ledger, sweep, hold and stages modes construct the engine themselves and never enter the switch (read, not run). | None: it prints no proof. | Builds. Not run. |
| `h2_libm_count.py` (context, never a verdict) | A count of maths-library call sites in the compiled replay. | Not applicable (it runs nothing) | | Still matches the committed count. |
| `tools/h2_engine_fz_probe.py` (a measurement, in no gate) | The pinned scope against the mode off and against narrower scopes. | Its pinned scope is the same scope as `replay`; the others are the thing it measures, and since 2026-10-12 they too round to nearest. | Its BASELINE line, and the same three control lines. | MEASURED, both builds (section 3). |

Gates that do not replay the engine through that function, and are unchanged: the SCALPEL and
swarm test references (`h2_scalpel_parity_check`, `h2_swarm_parity_check`, `h2_swarm48_check`,
`h2_swarm_lift_check`), which no product target builds and the shell will not run (the first
dispatch measured their verdicts unchanged with the mode on for the whole thread);
`divergence_ledger_check`, which runs no C++; `golden_pin_check`; the legacy gates.

**The engine harness (`h2/harness/`) still renders with the mode off.** It is outside this
dispatch (section 9).

What the parity check sees with the mode on, measured by running the wired gate before and after
the fix and comparing the two outputs line by line: the only lines that differ are the eight
rows' bit-exact shares (section 4), three family means, and the two new control lines. (That
was the rework. The same comparison across the rounding change of 2026-10-12, measured: the
control lines differ and nothing else does.)

### How a gate proves its mode

Three lines, printed and judged by code in `tools/h2_engine_stream.h` that all the gates share.
(Two until 2026-10-12. `control ROUND` changed its meaning that day and `control WORD` is new:
section 8.)

- **`control FZ`.** Every replay counted itself from INSIDE the switch: the bits read back, and a
  subnormal result and a subnormal input both came out zero. The count must cover every replay
  the process made and be at least its scenario count. Then the switch itself is run from a
  host that enters with the mode off and from one that enters with it on: on inside, off
  inside a nested forced-off call (so "on" is not a reading that is always true), the host's own
  state and control word back afterwards; and after a throw inside the switch the word must be
  exactly as it was. The state the process was entered in is printed and not judged.
- **`control ROUND`.** It proves that the switch SETS round-to-nearest. (Until 2026-10-12 it
  proved the opposite half: that replays ran rounding to nearest and the switch left the mode
  alone.) Three parts.
  - Every replay ran rounding to nearest, ties to even, read from inside the switch by the
    header's own read-back: the field, and four additions.
  - A host that calls in rounding to nearest, toward zero, upward and downward, one after the
    other. Each must find arithmetic rounding to nearest inside the switch and inside its
    forced-off form, must be named correctly in what the switch returns, and must have its own
    mode back afterwards, also after a throw. Three sources have to agree on each reading: three
    additions in the tool, the C library (the host's mode is set and read with `fesetround` and
    `fegetround`, never with the header's constants), and the header's read-back.
  - The first clean replay the gate made is made again from each of the four hosts. Through
    the switch it must give the samples the gate judged, and the same events and readouts.
    With no switch at all, each of the three other modes must give OTHER samples than the same
    body rounding to nearest. That second half is what stops "the same bits" from being the
    reading of a script that does not care how it is rounded, or of a `fesetround` that did
    nothing.
- **`control WORD`.** The switch writes its two fields and no other bit. The bits that really
  move are measured: the rounding field is whatever `fesetround` changes between its four modes,
  and the flush bits are whatever differs between the switch and its forced-off form. Both must
  equal what the header declares it owns. Then, from each of the four hosts as it came and
  again with every field the switch does not own flipped (on arm64 the trap enables,
  default-NaN, AHP, FZ16 and FEAT_AFP's three; on x86-64 the six sticky flags), the word inside
  the switch must be the host's word with the flush bits set and the rounding field cleared,
  bit for bit; the word in its forced-off form the same with the flush bits clear; and the word
  after, the host's. Flipping means each of those bits is seen both set and clear.

What the three lines do not prove: the whole 543-scenario set from a host in another rounding
mode. That is section 8's measurement, made by hand. The every-run version is one scenario,
the first the gate replays.

What makes each red, measured in the rework (2026-10-11) with scratch copies that were not
committed. The fourth row is as it was THEN; the next table has what the same entry gives now.

| Scratch change | Self-digest | Parity | FMA control | RealtimeSanitizer probe |
|---|---|---|---|---|
| The header's register write does nothing | RED: `control FZ`, 0 of 543; the eight rows | RED: `control FZ`, 0 of 753; the three determinism rows (the child process judges its own replays) | exit 2, the run broke | RED: judged 0 of 543 |
| `replay` calls its body outside the switch (the state the critic found for parity) | RED: 0 of 0 replays counted; the eight rows | RED: 0 of 0 | exit 2 | RED: judged 0; 8 scenarios where the hooked replay differs from `replay` |
| The restore guard removed | RED: `a throw inside it: ... control word 0x0 before, 0x1000000 after, on after` | (shares the control) | | |
| The process entered rounding toward zero, while the switch still left the mode to the host | RED: `control ROUND`, 0 of 543; all 543 rows differ | RED: `control ROUND`; 484 of 543 miss parity | | |
| The process entered with the mode already on | GREEN, and it says so: `this thread is on here (reported, not judged)`. The first commit's tool read RED here with every digest matching. | | | |

What makes the rounding controls red, measured 2026-10-12 with scratch copies of the header
that were not committed. Each row is the product-build self-digest tool against the untouched
references, the process entered in the default mode unless the row says otherwise. **In every
row but the second the 543 digests still match:** only the controls see the fault.

| Scratch change to the switch | The 543 digests | `control FZ` | `control ROUND` | `control WORD` | The tool |
|---|---|---|---|---|---|
| (a) The rounding write does nothing | 543 of 543 match | PASS | FAIL. `a host that rounds toward zero: arithmetic rounds toward zero before, toward zero inside the switch`; the first clean replay through the switch `1 0 0 0 (must be 1 1 1 1)` | FAIL. `control word 0xc00000, 0x1c00000 inside` | RED, exit 1 (both builds) |
| (a), and every replay entered rounding toward zero | 0 of 543 match (both builds) | PASS | FAIL. `0 of 543 replays ran rounding to nearest` | FAIL | RED, exit 1 |
| (b) The restore gives back everything but the caller's rounding mode | 543 of 543 match | PASS | FAIL. `a host that rounds toward zero: ... the switch reports its caller toward zero, arithmetic rounds to nearest after and <cfenv> reads to nearest` | FAIL. `control word 0xc00000, 0x1000000 inside, 0x0 forced off, 0x0 after` | RED, exit 1 |
| (b) The restore removed altogether | 543 of 543 match | FAIL. `a throw inside it: ... control word 0x0 before, 0x1000000 after, on after` | FAIL. `arithmetic rounds to nearest after and <cfenv> reads to nearest` | FAIL. `around a throw 0x0, 0x1000000` | RED, exit 1 |
| (c) The write also clears default-NaN | 543 of 543 match | PASS | PASS | FAIL. `with its other fields flipped (0x0 as it came) 0x6089f07, 0x5089f07 inside` | RED, exit 1 |
| (c) The write also sets default-NaN | 543 of 543 match | PASS | PASS | FAIL. `control word 0x0, 0x3000000 inside, 0x2000000 forced off` | RED, exit 1 |
| (c) The write also clears one trap enable | 543 of 543 match | PASS | PASS | FAIL. `with its other fields flipped (0x0 as it came) 0x6089f07, 0x7089d07 inside` | RED, exit 1 |
| (c) A constant word is written, not the caller's word with two fields changed | 543 of 543 match | PASS | PASS | FAIL. `with its other fields flipped (0x0 as it came) 0x6089f07, 0x1000000 inside` | RED, exit 1 |

The last four are seen only by `control WORD`, and the two that clear a bit only from the host
whose other fields were flipped: from the default host those bits are already clear. Row (b)
is seen by neither the digests nor `control FZ`, because both enter rounding to nearest and
the fault gives back exactly that.

The same lines in the other gates, measured over mutation (a). The parity check ends RED, exit
1, with 540 of 543 scenarios still at parity: `control ROUND` and `control WORD` fail, and so
do its three determinism rows, whose child process judges its own replays. The FMA control
exits 2, "the run broke", and never "fired". The RealtimeSanitizer probe was not run over a
mutant: it calls the same function and reports "judged 0" when that returns false (read).

One mutant taught something about the control itself. With the restore removed, the first
version of the tools-only host (the one that flips the other fields) left the trap enables on,
and the tool died on an illegal instruction in the middle of its controls, its output lost. A
non-zero exit, so red, but mute. That host now puts its word back by a plain write and the
same mutant reports the three FAIL lines in the table.

## 3. The eight rows

Measured in both builds, and identical in both:

| Row | Samples that differ | Largest difference |
|---|---|---|
| E/Swell · cut at the key-off :: chord | 95 of 28160 | 2.67e-308 |
| E/Swell · cut at the key-off :: repeat | 92 of 28160 | 1.45e-308 |
| E/Swell · cut at the key-off :: arp | 97 of 28160 | 3.33e-308 |
| E/Gated stab · organ :: chord | 100 of 28160 | 2.64e-308 |
| E/Gated stab · organ :: repeat | 93 of 28160 | 2.57e-308 |
| E/Gated stab · organ :: arp | 100 of 28160 | 3.56e-308 |
| T/restrike after full release :: restrike | 196 of 23040 | 2.87e-308 |
| C/VL tier 1, a faded slot | 200 of 14080 | 4.09e-308 |

(The per-row numbers are the first dispatch's. The rework re-ran the probe and read its totals.)

**No row changes in its events or its readouts, and the probe now shows it.** Its count line
takes each differing row apart into the three things a digest covers and counts each on its own.
Measured in the rework, both builds: `FZ forced OFF: 8 of 543 rows differ from the pinned bits:
8 in their samples (largest |difference| in any sample 4.09e-308), 0 in their events, 0 in their
readouts`. The first commit's probe could not show this: its counter covered only rows whose
samples were equal. A row whose digest differs and in which none of the three differs is now a
failure of the probe.

Every one of the eight is a tail fading out: for a hundred or two hundred samples the output is
a subnormal number, and with the mode on it is zero.

## 4. The eight rows and the JS parity target: two readings, for the human to rule

The JS cannot set the mode. In those tails it produces subnormal samples; the C++ with the mode
on gives zeros there.

What the parity check sees, measured by the wired gate with the mode off and then on:

- Each of the eight rows keeps the same rms and the same largest difference to the printed
  digits. Events and readouts are identical.
- Only their share of bit-exact samples falls, by 0.29 to 1.42 percentage points (for example
  64.47% to 64.14%, and 81.37% to 79.95%).
- The mean share over all rows is 37.00% before and after. By family: E 46.33% to 46.28%, T
  43.39% to 43.38%, C 36.28% to 36.27%. The floor is 30%.

So no scenario leaves JS parity and no threshold is touched. The difference is at most 4.1e-308
against a bound of 1e-6. No ruling is needed to keep a gate green. A ruling is needed on how the
change is RECORDED, because ADR-187 says two things that pull apart here: item 5, that work
which stays within the parity bound "is NOT a divergence"; and item 7, that a golden is
re-baselined "ONLY through a divergence record". This dispatch does not edit the ledger.

**Reading 1: a ledgered divergence that the browser cannot follow.**

- The entry: in `docs/port/divergences.json`, citing ADR-211 item 1, scoped to the eight rows by
  name, with the probe's numbers as evidence and a `js_limit` statement: the browser cannot set
  the CPU's flush-to-zero mode, so in those tails the lab renders subnormal samples where the
  plugin renders zeros, at most 4.1e-308 apart.
- What it needs first: the ledger's check (`tools/labharness/divergence_ledger_check.mjs`)
  requires every built entry to name a flag in the composed JS (its rows L3 and L5), and this
  entry can have none. And ADR-187's ratification says a `js_limit` entry "removes its declared
  scope from lab parity"; here the eight rows pass parity, so the entry must say they STAY in
  parity, or the port-bug detector is lost on eight rows for nothing.
- What it does to the two headers: nothing in the reference files, whose header line ("re-pin
  only for ... the tool header's cases a-c") becomes true as written, because the re-pin is then
  case (a). The tool's header loses its sentence "ONE RE-PIN ON RECORD IS NONE OF a-c" and cites
  the entry instead. Item 5's "claimed by a divergence in the same commit" is met late, and the
  entry should say so.

**Reading 2: a ruled non-divergence.**

- The sentence, in ADR-187's terms: "Flush-to-zero is part of the floating-point environment the
  shell is ruled to run the engine in (ADR-211 item 1), as `-ffp-contract=off` is part of its
  build. It changes no law of the engine. No scenario leaves the parity bound (540 of 543 at
  parity before and after; the largest change is 4.1e-308 against 1e-6), so under item 5 it is
  not a divergence, no scenario leaves JS parity, and no ledger entry is owed. For item 7,
  ADR-211 item 1 stands as the record for the one re-baseline of the eight rows."
- What it needs first: that last clause. Without it item 7 is not met, because item 7 names a
  divergence record as the only way to re-baseline.
- What it does to the two headers: either the tool's header gains a named case (d), "a change
  to the floating-point environment the shell is ruled to run in, by an ADR that names the
  rows", and the reference files' header line then has to read "a-d", which is an edit to two
  protected files (or waits for their next sanctioned re-pin, since the tool writes that line);
  or the exception stays a single recorded one, as today, the files are not touched, and the
  tool's sentence "whether that change also belongs in the divergence ledger is an open
  question" is replaced by the ruling's number.

Not a recommendation, only what each costs: reading 1 changes a ledger and its check; reading 2
changes one ADR by a sentence.

## 5. The scope that is pinned

"On per block" can mean three things for a test tool. All three were measured, in both builds
(re-measured in the rework):

| Scope | The mode is on... | Rows that differ from the whole-replay scope |
|---|---|---|
| render | inside `Engine::render` only | 0 of 543 |
| calls | around every call a host block makes (render and the event calls); the engine is constructed with it off | 0 of 543 |
| whole replay | from before the engine is constructed until after its last render | (this is the pinned scope) |

**The three scopes give the same bits on every row.** Today's pin does not depend on the choice.

**Pinned: the whole replay.** Two reasons.

1. It is the one replay function, `h2engine_stream::replay`, with its whole body inside the
   switch. The narrower scopes need a hook around each engine call, which exists only as a copy
   for the probes (`tools/h2_engine_hooked_replay.h`). That copy runs its whole body through the
   same scope, and the RealtimeSanitizer probe compares the two bit for bit on every scenario.
2. It gives the shell a rule that can be checked at one boundary: **no engine code runs with
   the mode off.** "Per block" leaves a question at every call made outside a block (the
   engine's construction, a parameter set on the main thread while the plugin is inactive).

What a shell must then do to match the pin: run every entry into the engine inside the switch.
That is the audio-thread entries the ruling names, and also any main-thread site that
constructs, seeds or sets the engine. It is more than "per block", never less.

If a shell switched the mode on only at its audio-thread entries, the measurement above says
no pinned row would differ today. Nothing proves that for a future scenario, for example an
engine constructor that computes a coefficient small enough to underflow.

### Words for the shell's acceptance list (row OG-4)

That file is the lead's to edit. Proposed text for the row's three middle columns, rewritten
2026-10-12 for the human's choice on the rounding mode (section 8: the switch forces
round-to-nearest, and the shell counts the hosts that arrive in another mode).

> **Gap.** Flush-to-zero is ruled ON (ADR-211 item 1), and the switch that sets it also forces
> round-to-nearest and gives the host its own rounding mode back (ADR-213). Every gate that
> replays the engine runs it inside that switch over the whole replay, set in one place
> (`h2engine_stream::replay`), and the self-digests pin those bits. No shell exists to enter
> the switch, and nothing yet counts a host that calls in under another rounding mode.
>
> **Gate.** `h2_ftz_scope_check`. Every shell entry that runs engine code runs it inside
> `horde2::underFlushToZero` (`h2/flush_to_zero.h`, the copy the tools' replay uses): the
> audio-thread entries (process, params flush, start and stop processing, reset) and every
> main-thread site that constructs, seeds or sets the engine. Inside each,
> `horde2::flushToZeroState().engineMode()` holds: the flush bits read back, a subnormal result
> and a subnormal input both come out zero, the rounding field reads nearest, and arithmetic
> rounds to nearest, ties to even. Inside each, the control word differs from the host's in the
> flush bits and the rounding field and in no other bit. On return the host's control word is
> back bit for bit, read immediately around the call, also when the entry leaves by a throw.
> It is set per entry, not once per thread. Every number the engine is handed is computed
> inside the switch: a value computed before the entry is computed in the host's rounding
> mode. The shell's latch-and-report guard keeps a count of the entries where
> `underFlushToZero` returned anything but `Rounding::nearest`. The count is reported and
> changes nothing: the entry renders as every other entry does.
>
> **Control.** A build with the setter removed is red. A build whose switch leaves the rounding
> field to the host is red: a stub host that enters rounding toward zero, upward and downward
> must each get the pinned bits and its own rounding mode back, and must be counted once per
> entry; a stub host that enters rounding to nearest must leave the count at zero. A build
> with the restore removed is red, and so is one whose restore gives back everything but the
> rounding mode. A stub host that enters with the mode already on, and one with it off, must
> each get its own word back. A stub host that enters with its other fields set (trap enables
> and default-NaN on arm64, the exception flags on x86-64) must find them unchanged inside
> the entry and after it. A build that constructs the engine outside the switch, or computes
> one of its inputs there, is red by a check at the shell's engine boundary, and NOT by the
> digests: measured 2026-10-11, the narrower scopes give the same bits on today's 543 rows, so
> the digests cannot see it.

A further gate the lead may want in that list, offered and not decided: the shell's own render
of the 543 scenarios equals the pinned product digests.

## 6. One copy: where the switch lives, and what it offers

`h2/flush_to_zero.h`, namespace `horde2`. `tools/h2_engine_stream.h` includes it, and through
that every tool that replays the engine. The shell includes the same file when it is built.

What it offers a shell (as of 2026-10-12; section 8 has what changed that day):

- `underFlushToZero(f)`: runs `f` with flush-to-zero on and rounding to nearest, and puts the
  caller's control word back. It returns the rounding mode the caller arrived with
  (`Rounding::nearest`, `towardZero`, `upward` or `downward`), for the shell's count.
- `flushToZeroState()`: what is in force, never what was asked for. For flush-to-zero, the bits
  read back, and one multiply into a subnormal as a result and one as an input. For rounding,
  the field read back, and four additions that only round-to-nearest, ties to even, gets
  right. `on()` and `off()` are about flush-to-zero alone; `nearest()` is about rounding;
  `engineMode()` is both, and is what a shell entry must read inside the switch.
- `fpControlWord()`: the word, read only, for a caller that proves it came back.

What changed in the rework, each from the critic's review:

- **The word comes back on every way out (M2).** The restore is a destructor, so it runs when
  `f` returns and when `f` throws. It is one register write: no allocation, no lock, no call.
  The first commit restored the word after the call and so leaked the mode on a throw.
- **An unknown target does not compile (M3).** The first commit compiled to a switch that did
  nothing on any target it did not know, with a constant saying so. That constant is gone.
  Known: arm64 under GCC or Clang, and x86-64. Everything else stops at an `#error`: 32-bit
  x86, arm64 under Microsoft's compiler, and ARM64EC (where Microsoft's compiler defines the
  x86-64 macro too). Checked on the dev Mac by defining those targets' macros by hand, which is
  not a cross-compile. The branch for Microsoft's compiler on arm64 is NOT written: it reaches
  the register through intrinsics (`_ReadStatusReg` and `_WriteStatusReg`, recalled and not
  checked against its documentation here), and nothing here can compile or run it, so
  unwritten and refused is safer than written and never compiled.
- **The test-only pieces are out of the shell's reach.** The forced-off call is in a namespace
  named `flush_tools_only`. The raw register write and the bit masks are private to the file.
  There is still one copy of the bit-twiddling, and every call goes through it. (Since
  2026-10-12 that namespace also holds read-only copies of the two masks the switch declares
  it owns, and a second tools-only call that plays a host whose other fields are flipped;
  section 8.)
- It is a call and not a guard object, as before. The compiler does not know that writing the
  control word changes arithmetic and may move an operation across the write. The call goes
  through a function that cannot be inlined, so everything inside runs between the two writes.
  The destructor only does the restore.

It passes `tools/h2_rt_lint_check.py` (it is one of the 9 files that scan reads) and
`tools/h2_rules_check.py`. RealtimeSanitizer agrees that the switch is safe on a realtime
thread: the probe now runs it inside the realtime scope around every engine call (88,140
switches over the 543 scenarios, measured) and reports no violation.

Why there, and not somewhere else:

- **Not `h2/engine/`.** The engine is DSP. The mode belongs to the calling thread, which is the
  shell's business.
- **Not `tools/`.** Product code must not include from `tools/`, so the shell would need a
  second copy or a move.
- **Not a new directory under `h2/`.** `engine_enrolment_check` requires every directory under
  `h2/` that holds code to be listed in `docs/armor/engines.json`, and that registry's kinds
  (engine, module, test-reference, harness) have no place for support code. The registry is an
  owned ledger and outside this dispatch.

**To be plain about the last point:** that check skips files that sit directly in `h2/`. So the
header is in the one place under `h2/` where it does not look. The header is not an engine or a
module, which is what the check exists to find, but it is the first code file at that level
and it sets a precedent (section 9, question 3).

## 7. Windows x64, and what is not measured

**This section is the one place that says what the x86-64 half has run on.** The header and
`h2/README.md` point here.

Windows x64 is a 1.0 platform (ADR-209 item 8). There the mode is two bits of the MXCSR
register: FTZ (bit 15) flushes subnormal results, and DAZ (bit 6) reads subnormal inputs as
zero. On the arm64 CPU measured here, the one FZ bit does both. **So the x86-64 switch sets
both FTZ and DAZ.** With FTZ alone, a subnormal that arrives as an input would still be
computed with, and `flushToZeroState()` reports that case as neither on nor off.

**None of the x86-64 half could be measured on the dev machine, and none of it has run on
Windows.** What it has had:

- It was written from the documentation.
- **The reworked header and controls, under Rosetta 2 translation on the dev Mac** (measured
  in the rework): the parity-build self-digest tool, built as an x86-64 binary, ended `SKIP —
  unkeyed (other.appleclang-16.node24), controls fired`. Its `control FZ` passed: 543 of 543
  replays with the mode on; control word 0x1fa2 before and after and 0x9fe2 inside, from a
  host that enters off; the word back as it was after a throw. Its `control ROUND` passed:
  543 of 543 replays rounding to nearest, and the four rounding modes told apart. No
  reference exists for that key, so no digest was judged. That is a translator, not an x86
  CPU, and Apple's compiler, not gcc or Microsoft's.
- **CI's first run of this pull request** (run 38165894374, 2026-10-11), on the FIRST commit's
  header, is the only run on x86 hardware so far. Both Linux sanitizer lanes (x86-64, gcc 13; one under the address and
  undefined-behaviour sanitizers, one under the thread sanitizer) ran both self-digest tools,
  and each ended `SKIP — unkeyed (linux-x86_64.gcc-13.node22), controls fired`. The tool
  prints "controls fired" only when every control fired, `FZ` among them. The control's own
  line is not in the log: that lane prints a tool's full output only when it fails.
- The same run's Windows job built both self-digest tools, and so this header, with the
  Microsoft compiler. It does not run them. **So on Windows the switch is compiled and never
  run.**

(Both sanitizer lanes ended red in that run for another reason: `h2_harness_render`, run
with no arguments, exits 2. That is the base branch's known state.)

The rework adds code those lanes had not compiled when this page was written: the restore as
a destructor, a throw and a catch in the tools' shared control, `<cfenv>`'s four rounding
modes, and a hexadecimal floating-point literal. All are standard C++. None had been built by
gcc or by Microsoft's compiler; CI's run of the rework's commit is the first.

**The rounding half, added 2026-10-12 (section 8), has the same standing as the flush half:
written from the documentation, run under a translator, and never run on an x86 CPU or on
Windows.** What is written: the rounding control is bits 14 and 13 of MXCSR; cleared, it is
round-to-nearest; 01 is toward minus infinity, 10 toward plus infinity, 11 toward zero. What
it has had:

- **Under Rosetta 2 on the dev Mac** (measured 2026-10-12): the parity-build self-digest tool
  as an x86-64 binary ended `SKIP — unkeyed (other.appleclang-16.node24), controls fired`. Its
  `control ROUND` passed: 543 of 543 replays rounding to nearest; a host that rounds toward
  zero, upward and downward each found nearest inside, was named correctly by the switch, and
  had its own mode back; the first row came out the same bits from all four. Its `control WORD`
  passed: the field `fesetround` moves is 0x6000, which is what the header declares; from a
  host that rounds toward zero the word is 0x7fa2 before, 0x9fe2 inside and 0x7fa2 after; and
  with the six sticky flags flipped, 0x7f9d, 0x9fdd and 0x7f9d. The agreement between the
  header's table of the four codes and the C library's is the one thing here that checks the
  documentation against something else. It is still a translator, and Apple's compiler.
- **On x86 hardware: nothing yet.** CI's Linux lanes will run the three controls on this
  commit's header when the pull request's checks run; this page was written before that.
- **On Windows: compiled at most, never run,** as for the flush half.

Two things in the controls are new to those compilers: `fegetround`, and a tools-only call
that flips fields of the control word. On x86-64 that call flips only the six sticky flags
and never an exception mask, so it cannot turn an exception into a fault (argued from the
manual: loading MXCSR with a flag set and its mask set raises nothing).

Known ways x86-64 will differ from arm64, argued from the two manuals:

- x86 decides that a result is "tiny" after rounding it, and arm64 before. A result that
  rounds up to the smallest normal number is kept on x86 and flushed on arm64. Seen once by the
  first dispatch: 1.5 × DBL_MIN times the double nearest 2/3 gives zero natively and DBL_MIN in
  the Rosetta run.
- On x86 the same register holds the sticky exception flags. The switch restores the whole
  word, so flags raised inside the call are dropped on the way out.

Neither matters to the pins: digests are bit-exact on one platform only and each platform has
its own reference (ADR-187 item 7). They matter to anyone who expects the two platforms to
agree to the last bit in a tail.

Also not measured: any other compiler; any other arm64 CPU; anything audible; the CPU cost the
mode is there to remove.

## 8. The rounding mode: the choice, and what was built

**Chosen by the human on 2026-10-12: option A below, with the count from option B** (the
recommendation at the end of this section as it then stood; the lead records the ruling as
ADR-213). The switch forces round-to-nearest inside its scope and restores the host's setting
afterwards, and it reports the mode the host arrived with, so that the shell can count such
entries.

### What was found (2026-10-11)

The critic's finding (M1): the switch changed the flush-to-zero bits and nothing else. It kept
the host's rounding mode by design. So "run every entry inside the switch" did not by itself
reproduce the pinned bits.

Measured in the rework, with a scratch copy in which each replay ran rounding toward zero (the
tool's own reading of the script's numbers stayed rounding to nearest):

- all 543 self-digest rows differ, while `control FZ` passes;
- the parity check reads 56 of 543 scenarios at parity and 484 missing it.

So another rounding mode does not move the last bit of a tail. It takes most scenarios outside
the 1e-6 bound to the JS. A host that calls in that way would have got a different-sounding
engine. How often a host does that is NOT known here: nothing was measured on any host.

### The two options that were on the table

**Option A (CHOSEN): the switch forces round-to-nearest inside the scope, and restores the
host's word after.**

- What it is: the same two register writes, with the rounding field cleared in the word that is
  written on the way in. The whole word is already restored on the way out.
- Cost: none at run time. The header's promise changes from "only the flush-to-zero bits are
  changed inside" to "the flush-to-zero bits and the rounding field". The x86-64 half of that is
  again written from documentation (bits 13 and 14 of MXCSR). On x86 the older x87 control word
  has its own rounding field, which this does not touch; the engine's doubles do not use it on
  x86-64 (argued, not measured).
- What it buys: the shell's rule stays one sentence, and a host's rounding mode cannot change
  the sound.

**Option B (not chosen, except for its count): the shell asserts round-to-nearest at each entry
and treats another mode as a host fault.**

- What it is: the header stays as it was. The shell reads the rounding mode at each entry and,
  when it is not to nearest, counts it in the latch-and-report guard and does something
  defined: renders silence, or renders anyway.
- Cost: a branch and a policy the shell must carry and test. Rendering anyway ships the
  different-sounding engine. Rendering silence makes the plugin mute in a host that every other
  plugin may tolerate.
- What it buys: the switch stays minimal, and a misbehaving host is visible instead of
  corrected.

The recommendation was option A plus the count from option B: the sound is then the pinned one
in every host, and an odd host is still seen. The cost is nil, and the alternative leaves the
user with a wrong or silent plugin for a fault they cannot fix.

### What was built (2026-10-12)

- **The switch (`h2/flush_to_zero.h`).** The word written on the way in is the caller's word
  with the flush bits set and the rounding field cleared. A cleared field is round-to-nearest,
  ties to even, on both architectures. It is still two register writes per switch. On arm64
  the field is bits 23 and 22 of FPCR (RMode). On x86-64 it is bits 14 and 13 of MXCSR (RC),
  written from the documentation (section 7). The x87 control word is not touched.
- **The report.** `underFlushToZero(f)` returns the rounding mode its caller arrived with,
  decoded from the word the switch had read anyway: no second register read, no allocation, no
  lock, no call. The shell's latch counts the entries where that is not `Rounding::nearest`.
  The counter is the shell's and is NOT built. An entry that leaves by a throw returns
  nothing. The tools' replay keeps the same count for itself and prints it in `control ROUND`.
- **The read-back.** `flushToZeroState()` reports rounding as it is in force, not as it was
  asked for: the field as read back, and four additions next to 1.0 that only
  round-to-nearest, ties to even, gets right.
- **What the switch owns, and what it leaves alone,** is written at the top of the header. It
  owns the flush bits and the rounding field. It leaves, on arm64, the six trap enables,
  default-NaN, the two half-precision bits and FEAT_AFP's three; on x86-64, the six exception
  masks and the six sticky flags. On the way out it restores the whole word, as before.
- **The tools-only forced-off call** goes through the same switch, so it rounds to nearest too.
  The scopes of the flush-to-zero probe therefore differ in flush-to-zero alone.
- **Two tools-only additions in the header's `flush_tools_only` namespace,** which nothing a
  shell ships may call: read-only copies of the two masks the switch declares it owns, and a
  call that plays a host whose other fields are flipped. The second is the one place outside
  the switch that writes the word. On arm64 it turns the trap enables on, so nothing inside it
  may do arithmetic; its comment says so.
- **The controls** (section 2). `control ROUND` now proves that the switch sets the mode, and
  `control WORD` is new. An unknown target still stops at the `#error` (checked again by
  defining three other targets' macros by hand).

### Measured (2026-10-12, dev Mac)

**The whole set, from a host in another rounding mode.** The proof was made with a scratch
copy of `tools/h2_engine_stream.h` in which `replay` sets the calling thread's mode with
`fesetround` immediately before the switch, and afterwards aborts unless the thread is still
in that mode (by `fegetround` and by arithmetic). Nothing else differs from the committed
files. Both self-digest builds, against the untouched references:

| Every replay entered rounding | Parity build | Product build | The switch's own count |
|---|---|---|---|
| toward zero | 543 of 543 rows bit-identical, GREEN | 543 of 543, GREEN | 543 of 543 replays called from another mode |
| upward | 543 of 543, GREEN | 543 of 543, GREEN | 543 of 543 |
| downward | 543 of 543, GREEN | 543 of 543, GREEN | 543 of 543 |

TOTAL is `894c3bc4b62bcab0` in all six runs, as in the references. The same scratch copy over
a header whose rounding write does nothing: 543 of 543 rows differ in both builds, and
`control ROUND` reads 0 of 543. That is the state before the ruling.

**With the process entered in the default mode nothing moved.** Both self-digest tools read
543 of 543 against the untouched references, and the parity check's output is the same as
before the change line for line, except the control lines.

**What the switch cannot reach.** With the WHOLE process rounding toward zero from the first
line of `main` (another scratch copy), 507 of 543 rows differ in both builds while all three
controls pass. The cause is outside the switch: the tool reads each script before it enters
the switch, and turns the script's numbers into doubles with `strtod` in whatever mode the
process is in. Shown by a third scratch copy, the same as the second except that the reading
of each script is put back to nearest: 543 of 543 rows bit-identical in both builds, GREEN,
with the tool reporting `this thread rounds toward zero here` and 543 of 543 replays called
from another mode. So the process CAN be entered rounding toward zero and give the pinned
bits, as long as every number the engine is handed was made rounding to nearest.

For the shell this is a rule: **a number computed for the engine outside the switch is computed
in the host's rounding mode.** A note's frequency, a parameter's value converted from what
the host sent, a coefficient: each has to be computed inside an entry that is inside the
switch. The words for OG-4 carry it (section 5).

### The fields a future ruling could add

The switch leaves these to the host. What each would change is argued from the two
architectures' manuals; apart from the one data point under default-NaN, none was measured for
what it does to the engine.

On arm64 (FPCR):

- **The trap enables** (bits 8 to 12, and 15). With one set, the matching exception stops the
  thread where it would otherwise only raise a flag. Almost every operation in the engine is
  inexact, so a host that left the inexact trap on would stop in the first block, in this
  plugin and in any other. Forcing them off would be a protection and not a change of sound.
  Measured on the dev Mac: the bits can be set (they read back as written).
- **Default-NaN** (DN, bit 25). It changes which NaN an operation returns when an operand is
  already a NaN. It cannot move a row in which no NaN exists. One data point, measured as a
  by-product of a control: a scratch switch that also forces default-NaN ON gives 543 of 543
  product-build rows bit-identical (section 2, the second table).
- **FEAT_AFP's three** (AH, FIZ and NEP: bits 1, 0 and 2). AH changes what the flush bit means:
  with it set, FZ flushes results only, a result is judged tiny after rounding as on x86, and
  flushing inputs is FIZ's job. A host that entered with AH set would get other bits in a tail
  out of the same switch. Measured: all three bits can be set on the dev Mac's CPU, so that
  CPU has the feature. What they do to the engine was not measured.
- **The half-precision bits** (FZ16, bit 19, and AHP, bit 26). The engine has no
  half-precision number.

On x86-64 (MXCSR):

- **The six exception masks** (bits 7 to 12). The x86 form of the trap enables: a cleared mask
  turns the exception into a fault.
- **The six sticky flags** (bits 0 to 5) are status and not control. The restore already
  drops whatever was raised inside.
- **The x87 control word** is another register, with its own rounding and precision fields.
  The switch does not read or write it. A `long double` would be computed under it; the
  engine and the header have none (read).

The widest form would write one whole known word on the way in. The engine's floating-point
environment would then be a constant of the build and no longer partly the host's. Its cost:
the header would own every field it now promises to leave alone, and each would have to be
measured on each architecture before it is pinned.

## 9. Open questions

1. **For the human: how the eight rows stand under ADR-187.** Section 4, two readings.
2. **CLOSED 2026-10-12: the shell's rule for the rounding mode.** The human chose option A
   with the count from option B; section 8 has what was built. What it leaves:
   - **For the lead: the ruling's record.** The header, the tools and this page cite it as
     "the human's ruling of 2026-10-12, which the lead records as ADR-213". That entry was not
     in `DECISIONS.md` on the commit this work was cut from.
   - **For the lead: the shell's counter is not built.** The switch returns what the counter
     needs. The words for OG-4 (section 5) ask for it.
   - **For the lead: a number computed outside the switch is computed in the host's mode**
     (section 8, measured). The words for OG-4 carry that too. It is a rule for the shell's
     design and nothing in the tree holds it today.
   - **For the human, later: the fields the switch still leaves to the host** (section 8, the
     last part). The trap enables are the ones a host could hurt the plugin with.
   - **For the lead, offered and not built: the whole-set proof needs a scratch copy.** A flag
     on the two self-digest tools that enters every replay in a named rounding mode would let
     anyone repeat section 8's table, on Windows too. It was left out because it is a new
     way to run a gate binary, and the brief asked for the proof by hand.
   - **For the lead, small: `underFlushToZero`'s return can be ignored without a warning.**
     Marking it must-use would make a shell entry that drops the count a compile warning, and
     would need a cast at the tool call sites that have no use for it (about a dozen, read).
3. **For the lead: where support code under `h2/` belongs.** Either the registry gains a kind
   for it and the header moves into a directory, or files directly in `h2/` are accepted as the
   place for it and `engine_enrolment_check`'s skip is written down as deliberate.
4. **For the lead: the engine harness** (`h2/harness/`) renders with the mode off. Its fuzz,
   invariance and soak gates are not built yet. They should render as the shell will.
5. **For the lead: `verify` does not echo the parity check's mode lines on a green run.**
   They are judged by the exit code either way. Its filter for that tool names the lines it
   echoes; `control FZ`, `control ROUND` and `control WORD` are not among them. (The
   self-digest tools' filter echoes every `control` line, so theirs show.) Output only.
6. **For the lead: the CPU ledger measures the engine with the mode off.**
   `tools/measure_h2_engine.cpp` uses the shared `replay` only in its stream-replay mode, which
   now runs in the mode and prints no proof of it. Its ledger, sweep, hold and stages modes
   construct the engine themselves and never touch the switch (read, not run). The mode exists
   to remove CPU cost in tails, so the ledger's figures are for an engine the shell will not
   run. The file is outside this dispatch.
7. **For the lead: the reference files' ORACLE header line is stale.** Both files name
   `docs/design/scalpel-interface-lab.html` at blob `6abf848e`; the stream and `h2/README.md`
   pin `77ccee77`. The tool's `--repin` rewrites that line; only the eight rows were sanctioned.
8. **For the human, later: Windows.** One run of the two self-digest tools on the Windows PC
   would turn "compiled and never run on Windows" into "measured".

Closed by the rework: the first version's question 2 ("should the parity check set the mode
too?"). It does, through the shared replay (section 2).
