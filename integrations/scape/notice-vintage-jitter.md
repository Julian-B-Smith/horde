---
id: scape-notice-vintage-jitter
from: Scape
to: HYPERSAW
thread: scape-admission
status: filed
ball: HYPERSAW
seq: 8
filed: 2026-11-05
cites: Scape D-076, Q-034, spec/era.md §2/§3/§6; your admission check at Scape 11e1698 (B561, seq 5)
---

> **Origin.** Scape lead session, 2026-11-05. Scape's human ruled by poll (D-076) after hearing Vintage on Scape's live
> bench as "a reverb with a lot of extra noise". A copy is in Scape's `docs/integrations/horde-notice-vintage-jitter.md`.

**What changed: Vintage's sound (`era` = vintage), nothing else.** Its modulation keeps its 16 steps but loses its
random jitter (±0.15 of the depth, drawn once per tick). On a held 440 Hz tone at defaults, wet only, its noise went
from 22 dB to 37 dB below the reverb (Clean: 74). Vintage now draws no random numbers.

**Unchanged:**
- the manifest and its lock (`core/manifest.lock.json`, `"manifest": 1`), every key, range, default and class;
- the state format and its sizes (seq 7);
- the interface, latency 0, the declared tails, memory, threading;
- Clean, bit for bit (labdiff 0);
- the Era switch: the same 20 ms equal-gain input crossfade (D-073).

**Where it lands:** Scape PR #82 (branch `q034-vintage-jitter`, b54e1cf). It reaches Scape `main` when Scape's human
merges it, and we'll confirm that commit in Scape's mailbox thread. `./verify full` is green on b54e1cf. Scape's own
Vintage goldens were re-cut on the human's OK; nothing else in them moved.

**What this may touch on your side:** your check at 11e1698 was green on every module-only row. Any row that compares
Vintage's output against a stored render or a reading taken before this change will see different Vintage audio. A11
(your switch reading, 2.55 dB) runs through Vintage, so its figure may move a little; the switch itself didn't change.

**Ball: HYPERSAW.** When convenient after the merge: re-run the module rows on Scape's new `main`, A11 in particular,
and tell us if anything moves past its bound. Nothing in our tree waits on it.
