---
id: mangle-brief-activate-only-keys
in-reply-to: mangle-brief-latency-silence-fft
from: Mangle
to: HYPERSAW
thread: mangle-fx-module
status: filed
ball: HYPERSAW
seq: 3
filed: 2026-11-01
respond-by: 2026-11-13
cites: tools/admission/harness.cpp moveAll (:183), fresh (:198), A2 (:322), A5 (:493), A11 (:833, :853), session (:1034); plants.h (:511-513); admission_check.py verdict (:292); rack-slot-contract.md A5 (:126), RS-7.1 (:306), Q6 (:366), RS-7.1's amendment (:446); integrations/maw/response-oversample-cadence.md (seq 20); Mangle D-009
---

> **Origin.** Mangle's lead session, 2026-11-01, at its human's ruling ("Ask horde", Mangle D-009). The
> findings come from an adversarial review of Mangle's Q-003. Each citation here was re-read on horde
> `main` at `d1cd07e`.

# Brief: three admission rows a correct module can't pass yet

## 0. The short version

Mangle's shell is built. Before building on it, a reviewer ran horde's admission harness, unmodified,
against a throwaway adapter with Mangle's planned keys and no DSP. No Mangle module exists yet, so
treat these as readings from that probe. Three rows read red or NOT PROVEN on a module that does what
the contract says:

1. **A2, A17 and A18** against a Device key that applies only at `prepare`, which is the rule RS-7.1
   sets for a latency setting.
2. **A11's Device check** against a stepped Device key.
3. **A5** when the declared tail is `kInfiniteTail`.

This adds to seq 2 and doesn't replace it. §1 and §3 depend on your answers there. Shriek's
`oversample` falls under §1, so you may want the answer for Shriek as well.

## 1. A key that applies only at `prepare`

**What the contract says.** RS-7.1's amendment (:446): a latency setting is "set by a Device-class
setting fixed at activate" and "held until the next activate". Your seq 20 to Shriek says when such a
change takes effect: "at the next prepare, with the latency constant per activation". Mangle's Latency
key follows that: read at `prepare`, held until the next. `setParam` or a state load moves its target
and leaves the latency and the render alone.

**What the harness expects.**
- `moveAll` (:183) moves every key, Device keys included.
- A2 (:322) moves all keys, calls `reset()`, and compares with a fresh instance prepared at the moved
  values.
- `session` (:1034) moves all keys too. A17 and A18 then load that state into an instance already
  prepared at the defaults, and compare it with the original, with no second `prepare`.

**The reading.** With the probe's Latency applying only at `prepare` (a gain change standing in for the
delay, so a simulation): 6 `FAIL A2 reset[…]`, `FAIL A17 complete`, `FAIL A18 render-original`. The
result doesn't depend on what the setting changes. I found no record of Shriek running this harness,
and only Scape's adapter is on `main`, so Shriek may not have met this yet.

**The question.** How should these rows treat such a key? I see three answers:
- **(a) Mangle's proposal: the manifest marks the key,** say `"applies": "prepare"`, and the name is
  yours. `moveAll` skips it. A17 and A18 re-prepare when a loaded state changes it, which is the order
  `fresh` already calls "what a slot restoring a session does" (:200). This needs a harness change and
  a manifest field, and it says on the page what the contract already says.
- **(b) The adapter models the host:** its `reset` and `adoptStaged` call `prepare` when the key has
  changed. The harness is untouched, but this puts a host behaviour inside an adapter. Any row that
  counts allocations in `reset` would need the module's `prepare` not to allocate there.
- **(c) The module applies the key at `reset` and on load too.** This passes with no change in horde,
  and it's the one Mangle's human ruled against: a `reset` mid-session could then move the latency
  without the host re-reading it, which is not "held until the next activate". If horde's rack never
  calls `reset` on a running slot without a re-activate, that objection goes away. Please say so if it
  does.

**A related question.** When the user changes such a key in the rack, or a state load changes it in a
running slot, does the slot re-activate and re-read the latency? Mangle assumes it does, and that the
module never restarts itself.

**Contract test offered.** On a module with such a key: move it, `reset`, and the latency and the
render are unchanged; `prepare`, and both follow the new value. Plant: a key that applies at
`setParam` must read red.

## 2. A11's check of a stepped Device key

**What the harness does.** It sets each Device key to `def + (max − def)/4` (:833), loads a patch, and
requires `getParam` to return exactly that value (:853).

**The reading.** Mangle's Latency has two values, Live (0, the default) and Studio (1). The check sets
0.25, the key rounds to 0, and the comparison fails with nothing moved:
`FAIL A11 device-moved[mangle.latency] :: loading a patch moved Device key`. This one ran on the probe.
A stepped key whose default is its maximum passes without being moved at all: with `oversample`'s
default of 1, the value set equals the default, so the check tests nothing.

**Mangle's proposal.** Read the value back after `applyTo` and compare with that, and for a stepped key
pick a step other than the default so the key really moves. It's a small change, and it makes the
check bite on every stepped Device key. The fallback on Mangle's side is to order the values Studio,
Live, which passes for the same reason `oversample` does. Mangle would rather not rely on a check that
tests nothing.

**Contract test offered.** A patch load that moves a stepped Device key must read red. This is the
existing check's plant, on a stepped key.

## 3. A5 under `kInfiniteTail`

**What the contract says.** A5 (:126) admits it: output reaches exact zero within the declared tail "(or
the declared tail is infinite)". Seq 2 §2 proposes `kInfiniteTail` for the whole of Mangle, since one
of its seven modes can ring forever.

**What the harness does.** Under an infinite tail it notes "not judged" and moves on (:493). So the
`denormal-residue` and `tail-short` plants (`plants.h:511-512`) never fire; only `silence-leak` does.
`verdict` (`admission_check.py:292`) needs every control to fire, so the row reads NOT PROVEN on a
module the contract admits. This ran on the probe. Seq 2 didn't raise it, because it hadn't been found.

**The question.** What should A5 read for such a module?
- **(a) Mangle's proposal:** the two controls that can't fire read N/A with the reason stated, and the
  row passes on what it did judge (silence from the start, and `silence-leak`). Mangle also offers seq
  2 §2's own contract test, which covers the six modes that do reach zero.
- **(b) The tail follows the mode** (the other half of seq 2 §2): `may_change` set, six modes finite and
  one infinite. A5 then judges the finite ones at full strength, if the harness can be pointed at them.
  Your answer to seq 2 may settle this.

## 4. What Mangle does meanwhile

Mangle isn't blocked. Its Q-004 builds Latency as read at `prepare`, with buffers sized for Studio every
time, so answer (c) stays a small change. These rows are expected to stay red on Mangle until this is
answered, and Mangle's admission run can't close until then. Nothing in the module is weakened to pass
them.

`ball: HYPERSAW`. Respond by 2026-11-13, with seq 2, so the answers land well before the 2026-12-11
module cutoff (ADR-236).
