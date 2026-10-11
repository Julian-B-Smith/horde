# Flush-to-zero in horde 2: what the ruling changes, and what it leaves open

> **Origin.** horde lead session, brief dated 2026-10-11, for ROADMAP B448 (Blind-Spot Armor).
> The human ruled flush-to-zero on in the horde 2 shell in ADR-211 item 1, closing ADR-209
> item 6, and made the re-pin of eight self-digest rows "its own dispatch". This page is that
> dispatch's account. An implementer wrote it from that brief.

**Status: the measurements are done and the re-pin is made (same pull request). Section 8 is
open and waits for the lead and the human.** Last verified 2026-10-11 on the dev Mac
(darwin-arm64, Apple clang 16, Node 24), on the branch this page arrives with.

Words used below. **Measured** means a command was run for this page and its output read.
**Read** means the answer comes from reading the code, and nothing was run with the mode on.
**Argued** means reasoning from documentation.

## 1. What flush-to-zero is, and what was ruled

Flush-to-zero is a CPU mode. With it on, arithmetic that would produce a subnormal number (one
smaller than about 2.2e-308) produces zero. It stops a near-silent tail from costing many times
the CPU of a loud one on some processors.

ADR-211 item 1: the horde 2 shell runs with it on from its first build. The shell does not exist
yet (B398). What exists is the engine, `h2/engine/`, and two reference files that pin its output
bit for bit over 543 scenarios (`h2/engine/selfdigest.parity.*` and `selfdigest.product.*`).
ADR-187 A1 says the shipped build is the tested build. So the references should pin the engine
with the mode on. This page checks what else that touches, then says how it was done.

## 2. Every gate that compares the C++ engine with something

The question for each: if the mode is on in the C++ side, does the gate's verdict change?

| Gate | What it compares, and how strictly | Verdict with the mode on | How known |
|---|---|---|---|
| `h2_engine_selfdigest_check` (parity build) | The engine's samples, events and readouts against pinned bits. Bit-exact. | **Changes.** 8 of 543 rows differ. | Measured |
| `h2_engine_selfdigest_product` (release flags) | The same, for the release build. Bit-exact. | **Changes.** The same 8 rows. | Measured |
| `h2_engine_parity_check` | The engine against the composed JS: rms under 1e-6, largest sample difference under 1e-6, identical events and readouts. Also a floor of 30% on the mean share of bit-exact samples. | Unchanged. 540 of 543 at parity, 0 red, every control fires as before. Mean bit-exact share 37.00% before and after. | Measured |
| `h2_engine_fma_control` | Must FAIL parity (it is the fused-multiply build). | Unchanged. Still fires: 24 of 543 rows miss parity. | Measured |
| `parity_floor_check --stream` | That the parity check refuses a stream with scenarios dropped. Counts only. | Unchanged. | Read |
| `h2_scalpel_parity_check` and its FMA control | The blade test reference against its oracle, within 1e-6. | Unchanged. 383 of 386 at parity; the control fires on 19 of 386. | Measured |
| `h2_swarm_parity_check`, `h2_swarm48_check` | The swarm test reference against SwarmSynth's goldens, within 1e-6. | Unchanged. Their output is the same line for line. | Measured |
| `h2_swarm_lift_check` | Two C++ paths of the swarm reference against each other, in one process. | Unchanged: both sides would run in the same mode. | Read |
| `divergence_ledger_check`, `docs/port/divergences.json` | The composed JS against its own pinned fingerprints. It runs no C++. | Unchanged. Green. | Read, and run |
| `h2/engine/libm-calls.*` (`h2_libm_count.py`) | A count of maths-library call sites in the engine's compiled code. Not a verdict. | Unchanged. Neither file it compiles was edited. It still matches. | Read, and run |
| `golden_pin_check` (the pins in `h2/README.md`) | Five prototype files against their pinned content hashes. | Unchanged. No pinned file was edited. Green. | Run |
| The engine harness selftest (`h2/harness/`) | Renders compared with each other inside one run. It pins no bits. | Unchanged. | Read |
| The legacy gates (`denormal_check`, `robustness_matrix`) | The legacy shell, which this ruling does not touch. | Not affected. | Read |

How the "measured" rows were measured:

- **The two self-digest checks.** Before the re-pin, the probe as it then stood
  (`tools/h2_engine_fz_probe.cpp`, built for both builds with one scratch line added to compare
  the scopes): with the mode off all 543 rows equalled the pins; with it on, 8 rows differed,
  the same 8 in both builds. After the re-pin the committed probe reports the mirror image:
  the mode-on replay equals all 543 pins, and with the mode forced off the same 8 rows differ.
- **The parity check and its FMA control.** A scratch copy of `tools/h2_engine_parity_check.cpp`,
  not committed, with the mode switched on around each of its 12 replay calls, built with the
  target's own flags. Its counter read the bit back as set on all 543 clean replays. A second
  build of the same wrapper with the bit left alone gave the stock output line for line, so the
  wrapper itself changes nothing.
- **The SCALPEL and swarm gates.** The stock source, with the mode on for the whole thread
  before `main`. This also puts the comparison arithmetic under the mode, which can only hide a
  difference smaller than 2.2e-308.

The SCALPEL and swarm cores are test references. No product target builds them and the shell
will not run them, so the ruling does not reach them. They were measured because it was cheap.

**The gate between the stages held:** only the two self-digest checks change, and nothing under
`h2/engine/` or `h2/cores/` needed editing.

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

No row changes in its events or its readouts. Every one is a tail fading out: for a hundred or
two hundred samples the output is a subnormal number, and with the mode on it is zero.

## 4. How the eight rows stand against the JS parity target

The JS cannot set the mode. In those tails it produces subnormal samples, and the C++ with the
mode off matched them bit for bit. With the mode on, the C++ gives zeros there.

What the parity check sees, measured:

- Each of the eight rows keeps the same rms and the same largest difference to the printed
  digits. Events and readouts are identical.
- Only their share of bit-exact samples falls, by 0.29 to 1.42 percentage points (for example
  64.47% to 64.14%, and 81.37% to 79.95%).
- The mean share over all rows is 37.00% before and after. By family: E 46.33% to 46.28%, T
  43.39% to 43.38%, C 36.28% to 36.27%. The floor is 30%.

So no scenario leaves JS parity and no threshold is touched. The difference is at most 4.1e-308
against a bound of 1e-6. Under ADR-187 item 10 the engine with the mode on is still correct on
every scenario by its JS target, and **no ratified divergence is needed to keep a gate green.**

Whether it should still be WRITTEN DOWN as a divergence is a separate question, and it is not
settled here (section 8, question 1). The case for: it is a ruled, deliberate difference from
the JS that the browser cannot follow, which is what ADR-187's `js_limit` marker describes, and
the self-digest tool's own re-pin rules name three reasons, none of which is this. The case
against: ADR-187 item 5 says work that stays within the parity bound "is NOT a divergence", and
the ledger's check is built around flags in the composed JS (its rows L3 and L5 need every built
entry to have one), so an entry for a C++-only mode does not fit it as built.

This PR does not touch the ledger. The re-pin's recorded reason is the ruling itself, ADR-211
item 1, and the tool's header says so without adding a fourth standing reason.

## 5. The scope that is pinned

"On per block" can mean three things for a test tool. All three were measured, in both builds:

| Scope | The mode is on... | Rows that differ from the whole-replay scope |
|---|---|---|
| render | inside `Engine::render` only | 0 of 543 |
| calls | around every call a host block makes (render and the event calls); the engine is constructed with it off | 0 of 543 |
| whole replay | from before the engine is constructed until after its last render | (this is the pinned scope) |

**The three scopes give the same bits on every row.** Today's pin does not depend on the choice.

**Pinned: the whole replay.** Two reasons.

1. It uses the one replay function, `h2engine_stream::replay`, unedited: the function the parity
   check uses. The narrower scopes need a hook around each engine call, which exists only as a
   copy for the probes (`tools/h2_engine_hooked_replay.h`) and cannot plant the fault that the
   self-digest's FAULT control needs.
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

That file is the lead's to edit. Proposed text for the row's three middle columns:

> **Gap.** Flush-to-zero is ruled ON (ADR-211 item 1). The engine's self-digests pin its bits
> with the mode on over the whole replay. No shell exists to set it.
>
> **Gate.** `h2_ftz_scope_check`. Every shell entry that runs engine code runs it inside
> `horde2::underFlushToZero` (`h2/flush_to_zero.h`, the copy the self-digest tools use): the
> audio-thread entries (process, params flush, start and stop processing, reset) and every
> main-thread site that constructs, seeds or sets the engine. Inside each,
> `horde2::flushToZeroState().on()` holds: the bits read back, and a subnormal result and a
> subnormal input both come out zero. On return the host's control word is back bit for bit,
> read immediately around the call. It is set per entry, not once per thread.
>
> **Control.** A build with the setter removed is red. A build with the restore removed is red.
> A stub host that enters with the mode already on, and one with it off, must each get its own
> word back. A build that constructs the engine outside the switch is red by a check at the
> shell's engine boundary, and NOT by the digests: measured 2026-10-11, the narrower scopes give
> the same bits on today's 543 rows, so the digests cannot see it.

A further gate the lead may want in that list, offered and not decided: the shell's own render
of the 543 scenarios equals the pinned product digests.

## 6. One copy: where the switch lives

`h2/flush_to_zero.h`, namespace `horde2`. The self-digest tools and the probe include it today.
The shell includes the same file when it is built.

- It offers `underFlushToZero(f)` (the shell's call), `withoutFlushToZero(f)` (for tests: the
  forced-off control, and a stub host that enters with the mode on), and `flushToZeroState()`,
  which says whether the mode really took effect: the bits read back, and one multiply into a
  subnormal as a result and one as an input.
- It is a call and not a guard object. The compiler does not know that writing the control
  word changes arithmetic and may move an operation across the write. The call goes through a
  function that cannot be inlined, so everything inside runs between the two writes.
- It passes `tools/h2_rt_lint_check.py` (it is one of the 9 files that scan reads) and
  `tools/h2_rules_check.py`. It allocates nothing and makes no system call.

Why there, and not somewhere else:

- **Not `h2/engine/`.** The engine is DSP. The mode belongs to the calling thread, which is the
  shell's business. The brief also kept `h2/engine/` read-only.
- **Not `tools/`.** Product code must not include from `tools/`, so the shell would need a
  second copy or a move.
- **Not a new directory under `h2/`.** `engine_enrolment_check` requires every directory under
  `h2/` that holds code to be listed in `docs/armor/engines.json`, and that registry's kinds
  (engine, module, test-reference, harness) have no place for support code. The registry is an
  owned ledger and outside this brief.

**To be plain about the last point:** that check skips files that sit directly in `h2/`. So the
header is in the one place under `h2/` where it does not look. The header is not an engine or a
module, which is what the check exists to find, but it is the first code file at that level
and it sets a precedent (section 8, question 4).

## 7. Windows x64, and what is not measured

Windows x64 is a 1.0 platform (ADR-209 item 8). There the mode is two bits of the MXCSR
register: FTZ (bit 15) flushes subnormal results, and DAZ (bit 6) reads subnormal inputs as
zero. On the arm64 CPU measured here, the one FZ bit does both. **So the x86-64 switch sets
both FTZ and DAZ.** With FTZ alone, a subnormal that arrives as an input would still be
computed with, and `flushToZeroState()` reports that case as neither on nor off.

**None of the x86-64 half was measured on an x86 CPU.** What it has had:

- It was written from the documentation.
- It was compiled as an x86-64 binary and run on the dev Mac under Rosetta 2 translation: the
  bits read back, results and inputs both flush, the word is restored. The whole parity-build
  self-digest tool was run the same way and its `control FZ` passed (control word 0x1fa2
  before and after, 0x9fe2 inside; 543 of 543 replays with the bits set; no reference exists
  for that key, so no digest was judged). That is a translator, not an x86 CPU.
- The Microsoft compiler's spelling has not been compiled by anyone yet. CI's Windows job
  builds every target, so it will be the first to try.
- CI's Linux sanitizer lanes run both self-digest tools on x86-64, and the tools' `control FZ`
  is judged on every platform. So the first real x86 result is CI's run of this pull request.

Known ways x86-64 will differ from arm64, argued from the two manuals:

- x86 decides that a result is "tiny" after rounding it, and arm64 before. A result that
  rounds up to the smallest normal number is kept on x86 and flushed on arm64. Seen once:
  1.5 × DBL_MIN times the double nearest 2/3 gives zero natively and DBL_MIN in the Rosetta run.
- On x86 the same register holds the sticky exception flags. The switch restores the whole
  word, so flags raised inside the call are dropped on the way out.

Neither matters to the pins: digests are bit-exact on one platform only and each platform has
its own reference (ADR-187 item 7). They matter to anyone who expects the two platforms to
agree to the last bit in a tail.

Also not measured: any other compiler; any other arm64 CPU; anything audible; the CPU cost the
mode is there to remove.

## 8. Open questions

1. **For the human: is flush-to-zero a ledgered divergence?** Section 4 gives both cases. The
   options: (a) an entry in `docs/port/divergences.json` marked `js_limit`, which needs the
   ledger's check to accept an entry with no JS flag; (b) a ruling that a mode change inside
   the parity bound is not a divergence, with the self-digest tool's re-pin rules gaining a
   named case for it; (c) leave it as the one recorded exception it is today.
2. **For the lead: should the parity check set the mode too?** Today the wired parity check
   judges the engine with the mode off, and the pinned bits are with it on. The link between
   them is the probe's measurement (8 rows, under 4.1e-308), which no gate runs. Measured:
   with the mode on, the parity check and its FMA control keep their verdicts and the mean
   bit-exact share stays 37.00%. The tidiest form would be the switch inside
   `tools/h2_engine_stream.h`'s `replay`, so every tool that replays a script gets it. Both
   files were outside this brief.
3. **For the lead: the engine harness** (`h2/harness/`) renders with the mode off. Its fuzz,
   invariance and soak gates are not built yet. They should render as the shell will.
4. **For the lead: where support code under `h2/` belongs.** Either the registry gains a kind
   for it and the header moves into a directory, or files directly in `h2/` are accepted as the
   place for it and `engine_enrolment_check`'s skip is written down as deliberate.
5. **For the lead: the reference files' ORACLE header line is stale.** Both files name
   `docs/design/scalpel-interface-lab.html` at blob `6abf848e`; the stream and `h2/README.md`
   pin `77ccee77`. All 543 rows reproduced regardless before this change, so the file moved
   without changing any scenario. The tool's `--repin` rewrites that line; this PR did not carry
   that change, because only the eight rows are sanctioned.
6. **For the human, later: Windows.** One run of the two self-digest tools on the Windows PC
   would turn the x86-64 half from "written from the documentation" into "measured".
