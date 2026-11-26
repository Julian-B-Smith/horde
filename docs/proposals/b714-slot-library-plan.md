# B714 — one generic module slot: the staged plan

Origin: horde lead session, 2026-11-26. ROADMAP B714 (ADR-410 r4, ADR-411); rulings ADR-412, ADR-413.

**How this was made.**
1. Four Opus readers surveyed the test plug-in's FX hosting, the bridge, the contract and `src/routing_core.h`.
2. A design pass split the work into stages.
3. An Opus critic attacked the design, and its blockers were fixed before stage 1 began.

**Kept for a fresh agent.** Where the text below and DECISIONS.md disagree, DECISIONS.md holds. The amendments listed here come after the original design.

**Amendments since the design** (rulings and the critic):
- **S0 is not a PR.** The notices and questions went without one, and the readings are ADR-413's.
- **S1 keeps the page's `preview.*` keys,** generated from the ledger's alias data. They are retired in S6, after the GUI session's GB42.
- **S1 proves "no per-module branch" by behaviour.** A third, do-nothing module is hosted by data alone, and a planted branch must fail. `module_neutral_check` is a secondary gate.
- **Host ids are morph rows,** as RS-5.6 says (ADR-412 r2). The design's R3, frozen plain units, is superseded.
- **FOUNDATIONS is pinned** (ADR-412 r3), at `tools/rack/pins/foundations.pin`.
- **Weir keeps a -140 dB idle stop** (ADR-413 r3).
- **Device keys sit in `fx`, once.** Device preferences have their own non-patch section.
- **The S3 runaway ceiling** (16.0 in R5) is a guess, to be measured or ruled. RS-3.5's loop bound, RS-7.2's min_block refusal and A14 are built in S3.
- **At a load, a corner whose wires do not connect** loads with the default serial chain and is reported. "Keep the previous set" applies to edits only.

---

B714 IMPLEMENTATION PLAN (design lead, read-only; worktree wt-b714 at 3159a196)

I spot-checked these against the worktree:
- ROADMAP.md:7754-7756 (B713, B714, B715).
- DECISIONS.md:13902 (ADR-410) and :13964 (ADR-411).
- src/routing_core.h:42-129, 156-305.
- tools/engine-preview/bridge_core.cpp:463-525.
- preview_plugin.cpp:1609-1645, 1692-1735, 2210-2245.
- test_rows.h:17-58, reverb_core.h:20-65, filter_core.h:30-80.
- tools/admission/module.h:1-145, tools/admission/state_budgets.json.
- Weir: `git diff 078f1436..origin/main -- include/` and the manifest at origin/main.

Reader claims confirmed:
- FX persist across a preset change.
- The idle-path guard gap is real (PP:1697-1704): guard_output runs only `if (rang)`, and only the reverb sets rang.
- edgeLive is constexpr true (RC:83).
- The acyclic branch never latches zPrev (RC:215-221).
- An unknown chunk magic returns false without a word (bridge_core.cpp:488-490).
- RS-3.8 names routing_core.h's edgeLive by file (rack-slot-contract.md:182).

New facts from the spot-checks:
- **Weir.** include/ is byte-identical between the pin 078f1436 and origin/main 92ff8af. The pin can move to 92ff8af, gaining the ratified manifest with stateBudget 4096, and the filter's DSP does not change.
- **Weir's defaults.** Its manifest defaults (cutoff 1000 Hz, mix 1, resonance 0, morph 0, mode 0) equal horde's filter_core.h State{} (:39). So "off at defaults" does not move the filter's sound.
- **Weir's old ids map one to one, generated.** Take the order "slot on" then manifest order, and leave out `fxfilter.type` (steps 1, one value). That gives on/mode/cutoff/resonance/morph/mix = 0xA00010-15, exactly the old block. Scape's old block is already generated: on at 0xA00016, manifest id k at 0xA00017+k.
- **Host value forms.** Weir's old host form is v: the log position for the cutoff, index form for on and mode (filter_core.h:33-46). Scape's old host form is plain Scape units (reverb_core.h:23-31). Old DAW automation lanes on both blocks stay valid only if the form of each existing id is frozen with it (reading R3 below).

======================================================================
STAGES (each one PR off main, never stacked; records ride as the last commit; CPU reported beside the test count at every stage)
======================================================================

S0 — Coordination and records (no code). Must merge or be sent before S1.
- Send the GUI session every message shape in section (B) in one notice, marked with the stage each goes live. Ask for:
  - a GB row moving previewPanel (gui3.html:1675-1755), device.js:102/286/633-639, engine-stub.js:306-308 and the gui3_browser_check.mjs:319-402 fixtures from `hello.preview` to `hello.slots`;
  - ENGINE-NEEDS rows for the feedback toggle, the slot-pick switch, the device-approval screen and module meters;
  - a GUI-74 amendment (its State paragraph, DECISIONS.md:12778, is superseded by ADR-410 r1/r2);
  - GUI-41's six-lane mock reconciled with Q15(a)'s one input.
- Write an ADR (next free number) recording readings R1-R7 below, each open to overrule.
- Dispatch the critic (Opus) on R1 (where the crosspoint lives) and R3 (frozen id forms), since both touch invariants.
- Ask the human the section (C) questions.
- Needs nothing from others.

S1 — The slot library and generic hosting on the existing serial chain.
Closes: (1) for the plug-in and bridge sides (the page side closes when the GUI's GB row lands), (9), and (6) for the chunk.
Partly closes: (5): off bit-identical, deaf, idle cut, state round trip, old chunk migrates.

Files, new:
- `rack/CMakeLists.txt`: a self-contained STATIC `horde_rack`, with no engine-preview dependency and no machine paths, so S8's repo can add_subdirectory it.
- `rack/modules.json`: per module, the budget key, the pin file, the env var for its checkout, the adapter target and the overlay file.
- `rack/slot_table.h/.cpp` (main thread): manifest + overlay + budget row + ledger → SlotTable, refusing per row A1 and RS-2.x.
- `rack/slot_runtime.h` (audio thread): a POD flattening of the table with preallocated double planar buffers, sized at prepare.
- `rack/rack.h/.cpp`: owns N admission::Module instances, the serial chain, true bypass, the slot I/O gains where a module declares none (RS-4.6/4.7), meters, and the non-finite latch.
- `rack/law/<module>.json`: horde's shim overlay, law and mod per address. It is the swap-in point named in the FOUNDATIONS brief (§4). Values: Weir cutoff log, mode step, the rest lin; Scape's transcribed from its units, with era and delayMotion as step and io.* as io.
- `rack/chunk.h/.cpp`: a generic `fx` section and a generic legacy reader.
- `tools/engine-preview/host_ids.lock.json`: the append-only ledger, (module, address) → {id, form: v|plain}. Seeded with 0xA00010-15 (Weir, form v), 0xA00016-33 (Scape, form plain), and next-free 0xA00034.
- `tools/admission/weir_adapter.cpp` over weir/svf.h, the same shape as scape_adapter.cpp.

Files, moved with git mv: scape_adapter.cpp and kiln_adapter.cpp go into a `rack_adapters` target that the admission harness also links.

Files, edited:
- `tools/engine-preview/weir.pin` → 92ff8af.
- CMakeLists.txt:27-76, 153-159, 182-258: pins from modules.json, and the library linked into preview-impl and into every plant MODULE target (they compile preview_plugin.cpp directly, :203-214).
- build.sh:13-17, 41-70.
- preview_plugin.cpp: every site listed by the hosting reader, replaced by rack calls. These include :54-61, 88-103, 385-396, 787-802, 832-837, 1609-1645, 1697-1704, 1725-1729, 1798-1803, 1851-1854, 1883-1886, 1898-1925, 1960-1997, 2138-2147, 2232-2240.
- bridge_core.h:19-20, 51-84, 163-181 and bridge_core.cpp:158-204, 472-501, 771: the descriptor generated from the table.

Files, deleted (human gate, Q-C1): filter_core.h, reverb_core.h, post_filter.h, post_reverb.h, test_rows.h.

Behaviour:
- **Chunk v7.** It writes an `fx` section in the final MO-31 per-corner layout, with the four corners identical until S5, plus a `device` section.
- **Legacy reads.** v5's positional "filter" line and v6's "scape" lines are read through a frozen legacy table in the ledger (a prefix → an id list or an id base), not through module code. So v5/v6 migrate into fx, and v1-4 load dry.
- **Loud refusal.** An unknown magic is refused loudly: state_load returns false, a session-log line is written, and a `load` frame report is published.
- **Preset change.** FX still persist across a preset change in S1. S2 flips this, so the must-fail control is born with the fix.
- **Pre-existing bug fixed generically.** On the idle path, guard_output and the meter run whenever any slot wrote.

Tests: generated per slot from the table, in smoke.cpp, with plants made by CMake foreach over modules.json.
- `plant-absent-<module>`: off bit-identical, over the whole bank.
- `plant-off-keeps-gains`: the RS-4.8/A7 must-fail. Checked with I/O at ±6 dB, slot-supplied (Weir) and module-owned (Scape).
- `plant-deaf-<module>`: the slot passes x and never calls process. Detected through the adapter's stressProbe.
- `plant-idle-cut`: generic.
- Round trip: save → load → save byte-identical, with %.17g.
- Migration fixtures: a v5 line and v6 lines, checked bit-identical against the pre-B714 render of the same chunk (golden captured in this PR from main's build), plus `plant-ignore-legacy`.
- `plant-silent-refusal`.

Gates added to ./verify fast:
- `tools/host_id_ledger_check.py`: append-only. A renumbered or re-formed id is red, and a plant proves it.
- `tools/rack_overlay_check.py`: every pinned manifest address has a law and mod, with no extras.
- `tools/module_neutral_check.py`: red if any module name, address or legacy identifier from modules.json or the ledger appears in preview_plugin.cpp, bridge_core.* or the descriptor code. A planted "scape" string proves it. This is acceptance (1)'s gate.

Risks:
- smoke and bridge_test are not wired into ./verify (only build.sh). Record it as a visible hole on the armor dashboard, or wire build.sh's smoke into full.
- Scape's kMaxStateBytes is 2048 against the ruled 4096. The effective budget is min(row, module). Report the difference (A19); do not hide it.
- A single FOUNDATIONS include path must be pinned (foundations.pin), not Scape's vendored copy.
- Scape allocates 12-27 MiB at prepare (main thread, as today).
- Weir's mode now switches by its declared 5 ms equal-gain crossfade, not instantly: a small audible change. Note it in the listening batch.

Needs first: S0's GUI notice sent. Without it, the GUI session's GB row must land in the same window, or the live page loses the filter and Scape cards. gui3_browser_check stays green on its own fixture, which is exactly why this is a silent-break risk.

S2 — Patches carry FX.
Closes: (2). Partly closes: (5): "preset replaces FX".

Files:
- rack/rack.cpp: a staged load (RS-6.6). Parse and validate off-thread, adopt at one block boundary, no ramp (RS-6.7).
- preview_plugin.cpp: build_slot :679-740, build_morph :619-677, adopt_demo :588-613, Kind::Preset :2528-2539, load_completes :2318-2332, and the swap in plugin_process :1666-1676. These call the rack's load with the preset's fx, which is empty for every bank entry.
- bank.h:3-22: an optional fx text, empty by default.
- gen_bank.cpp: passes the fx through.

Rules:
- RS-6.15. A load replaces every slot's state, its on switch, the wires and the levels.
- No fx means dry: every switch off, keys at manifest defaults, setSerialChain, true bypass.
- Device keys and the spill-over setting survive.
- Q18(a): a demo or corner load with no fx loads that corner dry.

Tests:
- `plant-preset-keeps-fx` (the must-fail: today's behaviour).
- Whole-bank dry load, bit-identical to the absent-all plant.
- A save after a preset change carries the new patch's (dry) FX.
- A chunk with FX, then a preset change, then back to the chunk restores the FX.

Risk: a preset change with Scape on now cuts its tail, since X115(a) resets fade levels at a load. Put this in the capped listening batch; no silent workaround.

Needs: nothing external. The resolver's landing marks for module keys (X116) wait for S5. Until then a load sets targets directly.

S3 — Crosspoint, feedback toggle, guard, connectivity.
Closes: (7) and (8), second half. Partly closes: (4): the form and consumption-side legality; per-corner content until S5.

Files: new `rack/crosspoint.h` (R1); rack/rack.cpp; rack/chunk.cpp (wires per corner, uniform until S5); bridge_core.* (`wires`, `deviceSettings`, frame `wires`, `latches.feedback_runaway`, `observe.meters.slots`); preview_plugin.cpp (settings and drain).

Behaviour:
- **Wires.** Presence and level are split. Presence is never derived from level, which reverses hypersaw_clap.cpp:8260-8262. Explicit `N/out` and `in/out` wires, with no isTerminal gate (MO-30). Levels are linear 0-1 and glide over 20 ms, stated in seconds (ADR-009).
- **One predicate.** `edgeLive(from, to) = edgeForward || (allowFb && !latched[from][to])`. It is called by every consumer: the gather, the output sum, the regime choice (RC:229-231's trap: a held edge must not select the cyclic branch) and the bridge publisher.
- **Double accumulation throughout.** That makes Prevent bit-identical to a back-edge level of 0 by construction.
- **Feedback toggle.** zPrev is latched every sample in the cyclic regime, and resetFeedback() runs at a toggle flip (the stale-carry gap, RC:215-221). The toggle is `device.fxFeedback`: Prevent by default, applied at the next block boundary, never a host parameter and never in a patch. It lives in the chunk's device section, which a project restore keeps.
- **Runaway guard.** Under Allow, a non-finite back-edge source sample, or one with |x| > 16.0 (+24 dBFS, the RS-4.6 I/O ceiling, proposed in R5), latches that edge to 0 from the next sample until reset or load. It counts the event and names the edge. It never clamps.
- **Wire-set change (RC-5.3).** An equal-gain crossfade per wire. Removed wires scale by 1−a, added by a, a shared wire is untouched. Tails ring out. The landing mark is honoured from S5.
- **Connectivity (R6).** A corner's wire set is refused at write and at load if no present forward-edge path runs from in to out. The previous set is kept and a report is published. Forward edges only, so validity never depends on the device toggle (Q-C2).

Tests:
- `rack/tests/crosspoint_check.cpp`, wired in full beside routing_check (verify:1581). It ports routing_check 17-19 (:1006-1150) onto the library form.
- Prevent bit-identity on in→1, 1→2, 2→1, 2→out, at the 4 corners. The 100 seeded positions come in S5.
- Must-fails:
  - Prevent as a write-time refusal only;
  - a back edge read in the same sample;
  - a guard that clamps without latching or counting;
  - a one-sample set change with no crossfade;
  - a tail cut at a change;
  - 1→out scaled by 1−a;
  - a connectivity check that counts back edges.

CPU: a cycle run sample by sample against the same chain with no back edge, with morph_probe's `--pair` pattern (morph_probe.cpp:424). Reported, never gated, and printed at every stage from here on.

Needs: GUI shapes (already sent in S0). For the human to hear feedback before the routing lab exists, either two demo patches with fx (Q-C3) or the GUI's GB21 routing lab sending `wires`.

S4 — Device settings in patches, with approval.
Closes: (10) on the bridge side.

Files: rack/chunk.cpp (`device[<module>]` written once per patch); rack/rack.cpp (a pending diff); bridge_core.* (frame `deviceDiff`, message `approveDevice`); preview_plugin.cpp (two load paths).

Behaviour:
- A patch load (preset or page) keeps the running Device values and publishes the diff until it is answered. Approve applies the stored values; decline, or another load first, keeps the running ones.
- A DAW project restore (state_load) applies the stored values silently (ADR-411 reading, X126).
- A patch with no Device values differs in nothing.
- Device keys never enter the resolver's table (RS-4.1, MO-28) but keep their host ids per the ledger.

Tests: the must-fail "a load applies a differing Device value without approval" (S:1403-1404); restore is silent; decline keeps; a second load before the answer keeps.

Needs: the GUI's approval screen. Until it exists, the stand-in page or the smoke drives approveDevice. A latency-changing Device value (RS-7.1) does not arise for Scape or Weir. Note it, do not build it.

S5 — FX morph per corner and the switchable slot pick.
Closes: (3), (4) in full, (8) first half. Closes the rest of (5): "morph reaches a key", plus the 100 seeded positions of the Prevent check.

BLOCKED on the builder implementing round 67:
- IF-12's slot table;
- MO-28 to MO-31;
- the slot-pick call (MO-29(3));
- landing marks (X116);
- then an engine export carrying them.

Today cleanroom-engine 4ac631f is spec-only, and preview_plugin.cpp:1388 drops every `fx/` set.

Files: preview_plugin.cpp (hand SlotTable::resolver_view() to the resolver at prepare, and route rack events by rack index to the rack, never to the engine; replace :1388's drop); bridge_core.cpp:343-360 (`corner` accepts slot keys); the morph frame member carries slot keys; engine-stub.js's mirror change is the GUI's.

Tests:
- Generated per slot: corner A at lo and D at hi, then position sweeps. Each is checked against the module's getParam target by the key's law.
- Flip keys switch only at the quantum boundary, by the declared switch.
- On/off follows MO-26.
- Must-fails: `plant-morph-drops-fx`, and a saved slot pick (the pick must never be saved or exposed as a host parameter).

Needs: the builder. Then a human listening batch on the pick default (Alone is interim) and the feedback default.

S6 — Cleanup and handoff.
- Retire `hello.preview` and the `preview.` prefix once the GUI's GB row is merged. This closes (1)'s page side.
- File a notice to the wavetable build repo that `rack/` is linkable by S8, by add_subdirectory, from its pins.
- Hand B715 the frame members.
- Rename overlay fields when FOUNDATIONS answers brief seq 6, then send the module notices.
- Delete merged mailbox duplicates per the standing OK.

FX device (ADR-254), every stage: the FX device links the same library. Per-shell differences: ids (the product follows RS-5.6) and the toggle and pick settings screens (GUI).

======================================================================
LEAD READINGS TO RECORD IN S0 (open to overrule)
======================================================================
- **R1. The crosspoint lives in rack/crosspoint.h, in routing_core.h's form; routing_core.h stays untouched.** routing_core.h feeds the legacy shell's goldens: float processBlock (RC:224), the isTerminal output gate (RC:134-150) and coefficient-derived presence. Every one of those diverges from MO-30 and RC-5. The amendment is RS-3.8's pointer (rack-slot-contract.md:182, prose, outside §12.9's table): "the slot library's edgeLive(), in routing_core.h's form". The alternative is a defaulted policy template parameter on RoutingMatrix. That gives one literal predicate, but routing_core.h's public interface becomes a gated change, and the float, double and terminal divergences would remain. Critic to judge.
- **R2. A flip key with fewer than two declared values is a constant.** It is held at its default, kept out of the slot table, the host ids and the page. This covers Weir's fxfilter.type and keeps MO-28's n ≥ 2 from refusing Weir's whole table (X119). The rule is generic, so no notice to Weir.
- **R3. host_ids.lock.json freezes each id's meaning and its host form.** The form is v by law, or plain module units. Old ids keep their forms (Weir v, Scape plain), so old automation lanes keep their meaning. New keys get v, like engine rows. The slot switch `fx/N.on` keeps the old "on" ids. Device keys keep their ids as plain rows outside corners.
- **R4. Until S5, host and page edits to a module key write that value at all four corners.** The `fx` member is always written in its final per-corner layout, so the chunk version does not churn when morph lands.
- **R5. Runaway ceiling: |x| > 16.0 (+24 dBFS) or non-finite.** Wire level glide is 20 ms. The wire-set crossfade length is the contract's X124 rack length if stated, else 10 ms, recorded.
- **R6. ROADMAP outranks ADR-411's "candidate".** The connectivity refusal is built as row (8) says, counting forward edges only. Q-C2 checks the one case this choice decides.
- **R7. Old chunks.** v5/v6 FX lines migrate into a corner-uniform `fx`. v1-4 load dry. Anything else is refused loudly. This is the ruled id fork's consequence, not a new fork.

======================================================================
(A) THE SLOT TABLE (C++)
======================================================================
Built on the main thread at activate. Sources: pinned manifest, rack/law overlay, tools/admission/state_budgets.json, the ledger, and module properties after prepare. The resolver view and the audio-thread POD are derived from it.

```cpp
namespace rack {
enum class Cls : uint8_t { Morph, Flip, Device };            // manifest "class"; None refused (A1)
enum class Law : uint8_t { Lin, Log, Step, Io, IoCut };      // overlay now; FOUNDATIONS 2.1 swap-in
enum class Switch : uint8_t { EqualGainCrossfade, Gate, SeededJump, Silent }; // Undeclared refused (A11)
enum class HostForm : uint8_t { V, Plain };                  // ledger-frozen (R3)
enum class BudgetStatus : uint8_t { Ruled, Provisional };    // state_budgets.json "status"

struct KeyRow {
  std::string address;            // manifest "address", e.g. "verb.decay"; name rule RS-2.9
  std::string label, unit;        // label_by_type or address tail; "unit"
  Cls cls; Law law; bool mod;     // "class"; overlay law/mod
  bool latched;                   // manifest "rate": latched | continuous
  double lo, hi, def;             // module units: "min","max","default"
  double def_v;                   // law^-1(def), the resolver's default v
  int n;                          // >=2 for Flip/Step (from "steps"/"values"), 0 continuous
  std::vector<std::string> labels;// "values"
  Switch sw; double switch_s;     // Flip only: "switch".method / length_s
  double transition_s;            // "transition_s" (the module smooths; the slot never smooths again)
  int handle;                     // module.resolve(address) at build; -1 refused
  uint32_t host_id; HostForm form;// host_ids.lock.json
};
struct BlobRow { std::string name; uint32_t max_bytes, def_bytes; };   // none declared today
struct FixedControl { std::string label, address; };                    // <= 4 (RS-5.8)
struct SlotRow {
  int index;                      // N in fx/N, fixed at activate (static nodes, RS-3.1)
  std::string module;             // manifest "module" ("fxfilter", "scape")
  std::string budget_key;         // modules.json -> state_budgets.json key ("weir", "scape")
  std::string pin;                // 40-hex from the .pin file
  std::string module_version;     // sha256 of the pinned manifest text (MO-31 module_version)
  uint32_t state_budget;          // bytes per corner; effective = min(row, module.stateBudget())
  BudgetStatus budget_status;
  uint32_t on_host_id;            // fx/N.on
  bool owns_io;                   // declares io.inGain + io.outGain (RS-4.7); else the slot supplies the pair
  int io_in_handle = -1, io_out_handle = -1;
  bool changes_image, changes_level;            // RS-2.4
  foundations::OperatorProperties props;        // validated after prepare (RS-2.3)
  std::vector<KeyRow> keys;       // morph+flip in manifest order, then device
  int first_device;               // keys[first_device..] are Device: never in the resolver view
  std::vector<BlobRow> blobs;
  std::vector<FixedControl> fixed;
};
struct WireRow { int from, to; bool present; double level; };   // from: -1 = in; to: kOut = out
struct SlotTable {
  std::vector<SlotRow> slots;     // order = modules.json order = the serial-chain order
  std::vector<WireRow> default_wires;   // setSerialChain(): in->0->1->...->out at 1.0
  uint32_t ledger_hash;           // the ledger's content hash, in hello
  // MO-28 names: fx/N/<address>, fx/N/blob/<name>, fx/N.on, fx/N.structure,
  // wire/<from>/<to>.on|.level. Device rows excluded.
  ResolverSlotView resolver_view() const;
};
// Audio thread: fixed arrays only (kMaxSlots = 8, kMaxKeys = 64), handles, per-block targets
// (std::atomic<double>), the on fade, io gains, meters, nonfinite latches, the crosspoint.
struct SlotRuntime;
}
```

======================================================================
(B) BRIDGE SHAPES TO SEND THE GUI SESSION (S0; each marked with its stage)
======================================================================
1. **[S1] `hello.slots`.** It replaces `hello.preview`, which S1 drops (it cannot be kept generically).
   - `{ledger, slots:[{slot, module, pin, version, budget:{bytes, status}, owns_io, fixed:[{label, key}], keys:[{key:"fx.<N>.<address>", id, name, cls:"morph"|"flip"|"device", law, mod, min, max, default, stepped, labels?, unit}], on:{key:"fx.<N>.on", id}}]}`
   - Keys go by slot, not by module ("by slot, never by module", S:2123). The descriptor names the module, so Scape's rack card binds where `module=="scape"`.
2. **[S1] Module keys on the existing messages.** `set`, `gesture` and `params` take `fx.<N>.*` keys in the page's v (0-1 by the key's law). `rack {values}` stays Held, and the page moves its rack-card traffic to `set`.
3. **[S1] `observe.meters.slots`.** `[{slot, in:{peak, clip}, out:{peak, clip}, nonfinite}]`.
4. **[S1] Frame `load`.** `{version, migrated:true|false, refused?:{reason}}`.
5. **[S3] Wires.**
   - Message `wires {seq, corner:0-3|"all", edges:[{from:"in"|N, to:"out"|N, present, level}]}`. "all" only until S5.
   - Frame `wires {feedback, corners:[[edge]], heard:[{from, to, level, back:bool, held:bool, latched:bool}], refused:[{corner, reason:"no-path"}]}`.
6. **[S3] Device-level settings.** Message `deviceSettings {seq, fxFeedback:"prevent"|"allow", slotPick:"alone"|"wires"}`, echoed in the frame. Never a host parameter, never saved in a patch. slotPick takes effect from S5.
7. **[S3] `latches.feedback_runaway`.** `[{from, to, count}]`.
8. **[S4] Device approval.**
   - Frame `deviceDiff {seq, items:[{slot, module, key, stored, running}]}` while pending.
   - Message `approveDevice {seq, accept:bool}`.
9. **[S5] Corner and morph members.** `corner {seq, index, values}` accepts `fx.<N>.*`. The `morph` member's corners and heard carry slot keys, and the wire sets per corner.

======================================================================
(C) QUESTIONS FOR THE HUMAN (only the real ones)
======================================================================
- **Q-C1. Deleting files (charter gate).** S1 deletes five files that the generic slot replaces: tools/engine-preview/filter_core.h, reverb_core.h, post_filter.h, post_reverb.h and test_rows.h. May it?
- **Q-C2. Feedback and connectivity.** With feedback allowed, should a corner whose only path from input to output runs through a feedback wire count as connected? The recommendation is no: forward wires only, so a patch's validity never depends on the device toggle.
- **Q-C3. Hearing feedback before the routing lab.** May B714 add two demo patches that carry FX: the serial chain, and the same chain with a 2→1 feedback wire? Or should the feedback listening wait for the GUI session's routing lab (GB21)?
- **Q-C4. Weir's state budget.** Weir has declared 4,096 B in its manifest (Weir D-020), but §12.9's row is still provisional. Rule it at 4,096 B? That is a separate one-line change to the table and the JSON, outside B714. B714 meanwhile hosts it as provisional and reports that status.

Not questions, because they are measured or ruled: the toggle's default and the slot-pick default (ruled to be picked after listening, in S3/S5's capped batch), the ceiling and glide figures (R5), and old chunks (R7).

======================================================================
DEPENDENCIES ON OTHERS
======================================================================
- **GUI session:** the S0 notice before S1. GB rows for hello.slots, toggle and pick settings, the approval screen, module meters and feedback wires (GB21). A GUI-74 amendment.
- **Builder (clean room):** round 67's resolver, for S5 only.
- **FOUNDATIONS:** brief seq 6 (law/mod/name rule, respond-by 2026-12-09). Not blocking; it is a rename in rack/law and the overlay check. process() and loadState() stay open, so the horde shim (admission::Module) carries them.
- **Weir and Scape:** none. Pins only: Weir → 92ff8af, Scape unchanged at 8350fb8a.
- **The human:** Q-C1 to Q-C4, then the capped listening batch after S3 (feedback default, tail cut at preset change, Weir's mode crossfade) and after S5 (pick default).

Key files: ROADMAP.md (B714 at :7755), DECISIONS.md (:13902, :13964), src/routing_core.h, tools/engine-preview/preview_plugin.cpp, tools/engine-preview/bridge_core.cpp, tools/admission/module.h, tools/admission/state_budgets.json, docs/proposals/rack-slot-contract.md (:175-301), ~/Documents/Claude/synthetic-worlds/FOUNDATIONS/integrations/hypersaw/brief-manifest-law-mod-names.md.