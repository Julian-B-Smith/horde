# The note-processor 1.0 bar: the minimum a note processor meets to ship in horde 1.0 (B539)

> **Origin.** horde lead, 2026-10-21, answering Arp's spin-up notice (seq 1, Q-H1), on the human's rulings by poll
> (ADR-269). This is the module 1.0 bar (`module-1.0-bar.md`, B439) adapted for a module that turns notes into notes
> rather than audio into audio. Rows that only make sense for audio (the I/O gain standard, mono safety, the rack
> slot, tails) are left out; note rows take their place. Where a row is the module bar's, its wording there governs.

## 1. It is defined

| | requirement | evidence |
|---|---|---|
| MUST | **A scope list:** the modes, rates and controls that ship, and what is after 1.0. Frozen at the feature freeze. | the list |
| MUST | **Declared in a manifest:** every parameter's key, unit, range, default, morph class (every setting is a morph target, as Arp's survey ruled), and rate class. Keys are frozen from the first release. | the manifest, and a check |

## 2. It plugs in

| | requirement | evidence |
|---|---|---|
| MUST | **The note vocabulary** (horde B92, FOUNDATIONS #115): `NoteRef` identity, never a bare key; canonical base pitch, with bend and glide as separate offsets. | a check |
| MUST | **No engine assumed downstream:** a pure note processor, so it runs on horde's FX-device sidechain after 1.0 (ADR-254). | the build without horde's engine |
| MUST | **Morph and presets:** every setting is a per-corner morph target; discrete settings switch at the specified threshold; the pattern bank selection is a morph target too. | a morph test |
| MUST | **Triggers** (ADR-267 A1), if steps can fire them: firing is a performance event that never creates a history node. | a check |

## 3. It is correct

| | requirement | evidence |
|---|---|---|
| MUST | **Every emitted note ends exactly once, on every path:** pattern and chord change, release, the arp switched off mid-note, transport stop and jump, panic, reset, and preset load. | an invariant suite with must-fail plants |
| MUST | **Sample-exact timing:** each step lands at the sample offset of its grid position against the transport, never at a block boundary; time signatures as data. | a check at several tempos, meters and block sizes |
| MUST | **Deterministic:** the same input notes, transport and seed give identical output; seeded mulberry32 only; no wall clock. | a determinism check |
| MUST | **Sample-rate and block-size independent:** the same notes at the same musical times across 44.1–192 kHz and blocks 1–4096. | a check |
| MUST | **Real-time safe:** no allocation, locks or system calls on the audio thread. | a sanitizer run |
| MUST | **No input, no output:** with no notes held (and no latch), it emits nothing. | a check |
| MUST | **The pedal and restart behave as ruled:** CC 64 holds Arp's input and is not forwarded; each restart mode does what it says. | a check per mode |
| MUST | **Scale quantize** keeps every emitted note in horde's global scale when it is on. | a check |

## 4. It is affordable

| | requirement | evidence |
|---|---|---|
| MUST | **A measured CPU cost** in Release, inside the **modulation, morph and intent bus slice (≤ 2 %)** of the approved budget (the human, by poll, 2026-10-21). | the measurement |
| MUST | **A memory figure,** worst case included, all of it allocated at prepare. | the figure |

## 5. It is usable

| | requirement | evidence |
|---|---|---|
| MUST | **A face** in GUI 3's design system (B531), keyboard-reachable, every control named. | the face |
| MUST | **Undo** covers every control edit (not trigger firing). | an undo test |
| MUST | **Factory content:** an init, and the 1.0 pattern bank's starter set. | the presets |
| SHOULD | **A manual page** (B413). | the page |

## 6. It is signed off

| | requirement | evidence |
|---|---|---|
| MUST | **An independent critic review** of the core and its oracles, sourced separately from the author. | the review |
| MUST | **The human's listening sign-off.** | the sign-off |
| MUST | **Its name cleared** (B433), for any user-visible name. | the clearance |

**Cutoff:** horde's module cutoff, **2026-12-11**. Every MUST row met and evidenced by then ships Arp in 1.0.
