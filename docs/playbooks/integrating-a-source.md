# Integrating a new source or feature: the playbook

> **Origin.** Written 2026-09-23 by an implementer agent, dispatched by the horde lead
> session for ROADMAP **B233** (the row is carried in PR #736). The human asked for it
> in these words: *"let's write a playbook for integrating new oscillators and features:
> they need to be fully wired into morph with certain contingencies handled, they need to
> be wired into the mod matrix, they need to be wired into history, and they need to play
> nice with presets"*. Its first real use is **B238**, the swarm extension.
>
> **Source of truth.** Every claim here was read from the code at `origin/main` `e07acac`,
> not from comments or other docs. Where a comment or doc disagrees with the code, §12
> lists it. A **hypothesis** is labelled as one. Anything the code does not do yet is
> marked **PROPOSED**.
>
> **Amended by B240** (2026-09-24, an implementer agent dispatched by the horde lead):
> §3.1, §3.2, §10's pin list, §11, §12 items 3–4, §13 Q1/Q2/Q2b/Q2c and checklist line 3
> were re-read against `origin/main` `6256301` plus the B240 change that added the morph
> field's append site.
>
> **Machine-checked.** Each citation below has the form `` `path:line anchor` ``. The
> anchor is text that must appear on that line. `tools/playbook_check.py` re-reads every
> citation in `./verify fast`. If an anchor vanishes from its file, the build fails. If
> the line has only moved, the check prints the new line as drift. When you see drift in
> the output, fix the line number here.

---

## 0. Read this first

**Where a new thing can land.** There are two places, and most seams below handle them
differently.

- **(A) Rows in the instrument table.** You add rows to `kParams`, either per-oscillator
  (each row gets a +1000 twin for osc 2) or global. A swarm extension such as B238 lands
  here: it changes what each swarm oscillator does.
- **(B) An engine block.** A new engine gets its own thousand ids in ADR-088's 3000..9999
  span. The SUB OSC (4000..4019) is the only block today. STATION's 3000-block is
  reserved for it.

**What "fully wired" means here.** The human named four seams: morph, the mod matrix,
history and presets. The code adds six more: ids, class, presentation/GUI, real-time
safety, parity, and checks. Every section below covers one seam with the same four
parts:

- **(a) Mechanism:** where the seam lives in the code.
- **(b) Invariant:** what your change must keep true.
- **(c) Check:** which `./verify` gate proves it, and whether it runs in `fast` or `full`.
- **(d) Incident:** the ROADMAP row, ADR or LIBRARY lesson that taught it.

**Two rules that apply everywhere.**

1. **Parity-safe superset.** Every new parameter's default is the *inert* value, so
   every golden, fixture, factory patch and user session sounds exactly as it did
   before. Example: `src/hypersaw_clap.cpp:1285 EVERY DEFAULT IS THE INERT ONE`.
2. **Append, never insert.** Ids, mod-source slots, enum labels, and above all the morph
   field's slot order are positional contracts with data already saved on disk. §3.1 is
   the seam where this goes wrong most often.

---

## 1. Parameter ids and engine blocks

**(a) Mechanism.**

- **The row format.** A parameter is a `ParamDef` row (`src/hypersaw_clap.cpp:77 struct ParamDef`)
  in `src/hypersaw_clap.cpp:174 static const ParamDef kParams[] = {`. The file header
  states the rule: `src/hypersaw_clap.cpp:6 Parameter IDs are frozen once shipped`.
- **Where new instrument ids go.** Instrument ids live below 1000. The highest id in use
  is 288 (`src/hypersaw_clap.cpp:775 {288, "env4R"`), so the next free id is **289**.
  Ids 207, 215, 223 and 231 are *not* free. They are spare slots inside the time-engine
  range, and `src/hypersaw_clap.cpp:7059 if (id >= 200 && id <= 231)` would capture them.
- **Per-oscillator or global.** A row is per-oscillator unless its id appears in
  `src/hypersaw_clap.cpp:844 constexpr clap_id kGlobalIds[] = {`. A per-oscillator row's
  twin is `id + 1000` (`src/hypersaw_clap.cpp:828 constexpr uint32_t kOscStride = 1000;`).
  Twins are derived automatically: `src/hypersaw_clap.cpp:1404 const ParamDef *findParam(clap_id id)`
  computes the oscillator from `id / kOscStride`.
- **Engine blocks.** A block is described by `src/hypersaw_clap.cpp:1199 struct EngineBlock`
  and registered in `src/hypersaw_clap.cpp:1347 constexpr EngineBlock kEngineBlocks[] = {`.
  It lives inside `src/hypersaw_clap.cpp:1344 kEngineIdLo = 3000;` .. 9999. The SUB is
  the model to copy (`src/hypersaw_clap.cpp:1238 static constexpr ParamDef kSubOscParams[] = {`).
  Its layout has three parts:
  - The core's parameters come first and are positional: `id − base` *is* the core's
    enum index.
  - The block's gate comes next (`src/hypersaw_clap.cpp:1273 {4015, "on", "SUB On"`).
  - Shell-owned rows sit above the gate (`src/hypersaw_clap.cpp:1312 constexpr clap_id kSubShellIdBase = 4016;`).
  - A `static_assert` proves at compile time that the rows agree with the core's own
    table (`src/hypersaw_clap.cpp:1321 constexpr bool subOscRowsAgreeWithCore()`).
  - The block's state keys carry a prefix (`src/hypersaw_clap.cpp:1212 const char *keyPrefix;`),
    because core keys such as `seed`, `wave` and `level` already exist in `kParams`.
- **Dispatch order.** Routing ids are resolved first
  (`src/hypersaw_clap.cpp:1411 if ((uint32_t)id >= kRoutingIdBase) return findRoutingParam(id);`),
  then engine ids (`src/hypersaw_clap.cpp:1415 if ((uint32_t)id >= kEngineIdLo) return findEngineParam(id);`).
  A new engine adds exactly one line in each of two places: the write at
  `src/hypersaw_clap.cpp:6954 if (eb->base == kSubOscIdBase) subSetParam(id, applied);`
  and the read at `src/hypersaw_clap.cpp:7477 return eb->base == kSubOscIdBase ? subGetParam(id) : 0.0;`.
- **The tools read the C++ text itself.** Parsers that scan the literal source include
  `tools/presentation_check.py:71 def engine_blocks(src):` and `tools/gen_gui_controls.py:114 blk = re.compile(`,
  plus the `kParams[] = {` … `\n};` split in several other tools. That fixes three shapes:
  - a `kEngineBlocks` row is `{kBase, kCount, kArray, kGate, "MODULE", "prefix."}`, with
    bare identifiers and no expressions;
  - every `ParamDef` row is `{id, "key", "Name", min, max, def, true|false, labels}`;
  - a label array is a named `k…` array.

**(b) Invariant.**

- **Ids never move.** Ids are append-only. They are never renumbered and never reused.
  A parameter you no longer want is **retired in place**: the row stays, its label says
  "(retired)", it keeps round-tripping through state, and it reaches nothing. See
  `src/hypersaw_clap.cpp:1261 {4011, "sync", "SUB Sync (retired)"`, pinned by subosc_check
  row 11e. The plugin id is frozen too (`src/hypersaw_clap.cpp:65 "com.lifted-truck.hypersaw",`).
- **Raw id or base id.** This is the trap behind B187, B219 and B220.
  - Per-oscillator dispatch must test `baseIdOf(id)` (`src/hypersaw_clap.cpp:919 inline clap_id baseIdOf`).
  - Patch-scope (global) dispatch tests the raw id.
  - Routing and engine ids must never reach `baseIdOf` at all, because it would alias
    them onto instrument ids (4004 would become base 4).
- **Keys stay unique.** A `coreKey` must be unique in `kParams`. It must also not collide
  with another core's `setParam` key, because the fall-through writes every core by key
  (`src/hypersaw_clap.cpp:7449 spectra.setParam(d->coreKey, applied);  //`). That is the
  lesson recorded at `src/hypersaw_clap.cpp:290 "toneTilt", not "tilt"`.
- **Keep an engine block under 35 rows** until the following sites are guarded. Some
  `baseIdOf` tests still accept engine ids, and none of them fires today only because
  the SUB block is 20 rows long:
  - the host's text formatter would print a continuous block row 35, 36 or 37 as an
    octave, semitone or cents value (`src/hypersaw_clap.cpp:8695 else if (baseIdOf(id) == 35)`);
  - `morphStep`, `intentApply` and `morphExemptSlot` would treat block row 150 as an
    oscillator enable (`src/hypersaw_clap.cpp:4216 if (baseIdOf(morphIds[i]) == 150) { morphApplyOscEnable`);
  - gui2's destination menu would silently drop block rows 161–178 and 269–288
    (`src/gui/gui2.html:3358 if (g >= 161 && g <= 178) continue;`).
  If STATION or anything else needs more rows, guard these sites first.

**(c) Check.**

- `presentation_check` (fast, `verify:92 python3 tools/presentation_check.py`): every
  declared address has exactly one row.
- `paramscope_check` (full, `verify:391 "$build_dir/paramscope_check"`,
  `tools/paramscope_check.cpp:81 1. DEFAULT TRUTH`): it walks the plugin's own
  `param_info`, so a new id is covered automatically. It checks that the readback equals
  the default and that a write round-trips.
  - **It cannot see B187- or B220-class bugs.** In those, one side is consistently wrong,
    so the readback still equals what was written.
- `paramclass_check` T1a (full) **pins the `kParams` row count at 266**
  (`tools/paramclass_check.cpp:170 T1a the table is the 266 frozen kParams rows`). A PR
  that adds rows moves that pin and states why.
- `subosc_check` has a dispatch row for the SUB
  (`tools/subosc_check.cpp:1321 11.0 DISPATCH`). A new block needs its own.

**(d) Incidents.**

- **Stride 100 made id 100 unreachable** (`src/hypersaw_clap.cpp:806 STRIDE 1000, NOT 100`,
  ADR-082 Amendment 1).
- **B172.** Without the engine dispatch, every id ≥ 3000 silently returned `nullptr`
  with every gate green.
- **The `toneTilt` / ghost-id-70 collision.**
- **B187, B219, B220 are all still OPEN at `e07acac`.** B187 is the SPECTRA branch testing
  the raw id (`src/hypersaw_clap.cpp:7381 if ((id >= 44 && id <= 55)`). B220 is the grid
  snap (`src/hypersaw_clap.cpp:6908 if (id == 23) v = snapGridStep`). B219 is the host
  text layer (`src/hypersaw_clap.cpp:8664 bool params_value_to_text`).

---

## 2. Parameter class

**(a) Mechanism.** `src/hypersaw_clap.cpp:1591 inline bool paramClassOf` is the whole
rule, written once:

1. If the id is in the override table, use that class
   (`src/hypersaw_clap.cpp:1494 static const ParamClassRule kParamClassOverrides[] = {`,
   keyed on the **base** id, ordered by id, every entry with a reason).
2. Otherwise a **stepped** parameter is **Structural**.
3. Otherwise it is **Morphable**.

Routing cells are always Morphable. Engine rows are classed by the same rule, with one
exception: an engine block's gate is Structural (B203).

**(b) Invariant.**

- **The class is a definition, not state.** It is never persisted and never differs
  between instances. Twins share a class by construction.
- **Device means "not part of the morph field".** Use it for modulation sources, intent
  plumbing, morph controls and output-stage policy, and for any continuous value that
  must not live in a corner. Such a row needs an override entry *with its reason*, the
  way B171's twenty LFO/ENV rows got one each.
- **Class ≠ membership.**
  - For engine blocks, membership in the morph field is *derived from* the class.
  - For `kParams`, membership comes from **hand-curated lists** (§3.1). Many Morphable
    globals are absent from the field. Examples: `bassMonoHz` (41) and the continuous
    delay and time-engine parameters (such as 200 and 232).
  - `paramclass_check` asserts only that no Device parameter is *in* the field. It does
    not assert that every Morphable parameter is.

**(c) Check.** `paramclass_check` (full,
`verify:308 "$build_dir/paramclass_check" > /tmp/paramclass`):

- T2 checks that no morph-field member is Device. Its must-fire control is T2c
  (`tools/paramclass_check.cpp:13 T2  no morphIds member is device.`).
- T4 checks that twins share a class.

**(d) Incidents.**

- ADR-173 (the derivation; still marked PROPOSED in DECISIONS.md, see §12).
- **B203.** The gate's class was wrong. The human overruled it: "Sub on/off is still
  exempt from morph and it ought to be wired in the way the other two oscs are."

---

## 3. The morph field

`morphIds` is one flat array. The four corner snapshots, the exempt flags and the
field's cursor are all **positional** arrays aligned to it. Every corner array ever saved
(host sessions, presets, corner-preset files, factory files) is read **by slot
position**, so the order is persisted data.

### 3.1 Membership and the append-only order

**(a) Mechanism.** `src/hypersaw_clap.cpp:3058 void morphInit()` builds the order in this
sequence:

1. **The frozen per-osc prefix.** `src/hypersaw_clap.cpp:3044 static std::vector<clap_id> buildMorphOrder`
   walks `kParams`, skips globals, and pushes each id followed by its twin.
2. **Hand-curated appends.** The FX slots come first, then the bend and note laws
   (ADR-104 A2), then the globals added in ADR-109 A1, then the scale.
3. **The late per-osc rows.** `src/hypersaw_clap.cpp:3102 for (clap_id id : kMorphLateIds)`
   appends id 181 and its twin. This list is **closed** (B240).
4. **The routing block.** `src/hypersaw_clap.cpp:3122 if (!isMorphTailId(d.id)) morphIds.push_back(d.id);`
5. **The engine blocks, in three passes** (§3.2).
6. **The tail** (B240). `src/hypersaw_clap.cpp:3206 for (clap_id id : kMorphTailIds)`
   appends every member added after layout 9, in listed order. **This is the only append
   site.**

Passes 1–5 are **frozen at layout 9** (B240). They produce the layout-9 order and cannot
grow, because each is bounded to the rows that existed then:

- The per-osc prefix admits only base ids below 182
  (`src/hypersaw_clap.cpp:3028 static constexpr clap_id kMorphL9OscIdEnd = 182;`). Every id
  from 1 to 181 is a row, every per-osc row is ≤ 181, and ids are never reused, so the bound
  is exactly the layout-9 set.
- The engine passes admit only the SUB's 20 rows, 4000..4019
  (`src/hypersaw_clap.cpp:3039 static bool inMorphL9EnginePasses`).
- The routing pass cannot be bounded this way, because a new source row's cells get ids in
  the *middle* of the table. It skips tail-listed ids instead, and T13 catches an unlisted
  one.

The **layout marker** names this order. It is written by four writers that must agree:

- `src/hypersaw_clap.cpp:5971 "{\"morphLayout\":9,\"cornerPreset\":[";`
- `src/hypersaw_clap.cpp:6095 "{\"morphLayout\":9,\"cornerPreset\":[";`
- `src/hypersaw_clap.cpp:6259 ",\"morphLayout\":9,\"cornerNames\":"`
- `tools/gen_factory_bank.cpp:456 std::string out = "{\"morphLayout\":9,\"cornerPreset\":[";`

Old arrays are remapped by `src/hypersaw_clap.cpp:6133 std::vector<size_t> morphSlotMap`,
which treats **every layout ≥ 2 as a 1:1 prefix**. The only real remap is ADR-159's
224-entry layout-1 case.

**(b) Invariant.**

- **Append at the true tail; never insert.** An id added anywhere except after the last
  existing member silently re-reads every stored corner against the wrong parameter.
- **HOW TO APPEND (B240).** Add **one line** to
  `src/hypersaw_clap.cpp:3030 static inline const std::vector<clap_id> kMorphTailIds = {`
  for each new member, in the order they join. **Brand-new ids only** (see the next
  bullet):
  - a **per-osc row**: list its **base** id only. Its +1000 twin is appended right behind
    it, as `kMorphLateIds` does. T13g rejects a bare twin. T10 requires every non-Device
    per-osc row to be in the field, so a new one that nobody lists turns T10 red;
  - a **new global row**: its id;
  - an **engine-block row**, from any block and of any non-Device class: its id. T10
    requires every non-Device engine row to be in the field, and the frozen engine passes
    will not add it for you. A new block's **gate** is listed too;
  - a **routing cell**: its id. The routing pass skips tail-listed ids.
- **An EXISTING parameter must not join through the tail without a load migration.** No
  stored chunk carries its slot, so `resetCorner` puts its **default** into all four
  corners. With morph on, `morphStep` then drives the live value to that default. The B240
  critic measured it: `bassMonoHz` saved at 300 loads as 120 once id 41 is in the tail,
  and stays at 300 with morph off. A brand-new id is safe only because its default *is*
  what every old patch already sounds like. The migration is ROADMAP **B255** and is not
  built.
- **Every appending change bumps the marker by one, at all four writers**, however many
  lines it adds: 9 → 10 at the first append (the history is in the comment at
  `src/hypersaw_clap.cpp:5942 THE LAYOUT MARKER, BUMPED ONCE HERE AND AT THE OTHER THREE WRITERS`).
- **Old chunks need no migration for brand-new ids.** `morphSlotMap` reads every layout
  ≥ 2 array 1:1 as a prefix. A layout-9 chunk loads every stored slot where it was, and
  `resetCorner` (B124) gives the new tail slots their defaults. The B240 demo append
  measured this on the 41 factory patches, the 4 corner presets and the 3 state fixtures
  (see its trace).
- **The quantum and intent draws are frozen at layout 9 (B240).** `MorphCore::reshuffle`
  used to draw one row per slot and *then* the shared vector, so a longer field re-dealt
  which corner every quantum slot drew. The shell now passes the layout-9 length as the
  draw prefix: rows 0..272, then the shared vector, then the tail rows
  (`src/morph_core.h:55 void reshuffle(uint32_t seed, int nParams, int nPrefix = kMaxParams)`).
  For a 273-slot field that is the same stream, bit for bit. `intentInit` numbers the
  layout-9 atoms, then `home`, then tail atoms, and draws the shared seed after `home`.
  T14 and T15 are the gates.
- **The field must fit the draw table.** `src/morph_core.h:21 static constexpr int kMaxParams = 512;`
  The three `pickCorner` callers index it by slot with no guard. T13f fails past 512 slots.
- **Freeze the append into `tests/morph_order.txt`.** Add the new ids at the end, in live
  order (base, then twin), and set its `layout` line
  (`tests/morph_order.txt:14 layout 9`) to the new marker, and add one row for the new
  layout to `kLayoutPins` in `tools/morphlayout_check.cpp` (T13h). Do it in the same change
  or the next one: T13 admits at most **one** unfrozen append.
- **Do not add a row to `kMorphLateIds`.** That list is appended *before* the routing
  block, so every routing and engine slot would shift. It is closed.
- **A new routing source row still inserts unless it is listed.** Raising `kRoutingNSrc`
  (`src/hypersaw_clap.cpp:976 constexpr int kRoutingNSrc = 3;`) moves no routing *id*.
  But `src/hypersaw_clap.cpp:1088 static RoutingParamTable makeRoutingTable` emits cells
  in row-major order, so the new row's cells would land in the *middle* of the routing
  block. Listing every new cell in `kMorphTailIds` moves them to the tail. T13 fails if
  any is left unlisted. This is **entailed from reading the code**; no source row has been
  added to measure it.

**(c) Check.** `morphlayout_check` (full, `verify:288 tests/morph_order.txt`):

- **T13** (B240) freezes the **whole** layout-9 order
  (`tools/morphlayout_check.cpp:40 T13 (B240) THE WHOLE ORDER IS FROZEN`). The fixture must
  be an exact prefix of the live order. Any insertion, removal or reorder fails and names
  the slot that moved. An append passes. The marker must equal the fixture's `layout`
  when nothing is appended, and be exactly one higher when something is. No id may hold
  two slots (T13d). The field must fit the draw table (T13f). Every tail id must be
  host-visible, in a real band, and a per-osc base with its twin behind it (T13g). The
  fixture's length must be the count pinned for its `layout` line (T13h), so freezing an
  append means adding one row to the check's `kLayoutPins`. T13e plants a fault for every
  one of these, and a legal append that must pass.
- **T14** (B240) re-deals the quantum draws as if the field were 164 rows longer, on
  "MO - Quantum Morph", across 3 temperatures × 3 couplings × 3 pad positions. It
  requires 0 owner flips and a bit-identical render. The pre-B240 order is its must-flip
  control. **T15** checks the intent seeds the same way, at core level.
- **T10** (widened by B240) requires every non-Device per-osc id, as well as every engine
  id, to be a member.
- **T1** freezes the 222-entry prefix and then requires 181/1181.
- **T1b** requires everything after that to be an id ≥ 3000. So a **per-osc or global**
  row appended through `kMorphTailIds` turns T1b **RED** until the band is moved by a
  ruled pin move (§13 Q1). An engine or routing append passes it.
- **T10b** checks only that the Structural engine ids form one contiguous run at the
  tail (`tools/morphlayout_check.cpp:25 T10b the Structural engine ids form one contiguous run`).
  It **cannot see** an insertion; T13 can. **Any** append after the gate ends that run's
  tail position, so T10b also turns **RED** at the first real append until its pin is
  ruled (§13 Q1).
- `playbook_check` (fast) checks that the four marker writers agree.
- `bank_check` pins the factory files' marker (`tools/bank_check.cpp:641 carries morphLayout 9`).

**(d) Incidents.**

- **ADR-159.** `oscPitch` (181) was added to the table on 2026-08-31 and landed inside
  the prefix, shifting every later slot. Every corner array saved between 2026-08-21 and
  2026-08-31 then read its bend law two slots off: "a spring-quantised 2 s step gate on
  patches that had none" (`src/hypersaw_clap.cpp:2957 kMorphAdr150Size = 224;`).
- **B124.** Short arrays kept the *previous* load's values in the slots they did not
  carry. The fix resets every corner before filling it.

### 3.2 The three passes (engine blocks), and why they are a snapshot

**(a) Mechanism.** Engine rows join the field in three passes over `kEngineBlocks`:

1. **Morphable rows** (`src/hypersaw_clap.cpp:3172 cls == ParamClass::Morphable`).
2. **Structural rows** (`src/hypersaw_clap.cpp:3182 cls == ParamClass::Structural`).
3. **Gates** (`src/hypersaw_clap.cpp:3202 if (inMorphL9EnginePasses(b.gateId)) morphIds.push_back(b.gateId);`).
   Passes 1 and 2 skip the gate so that it lands here.

The comment at `src/hypersaw_clap.cpp:3156 WHY TWO PASSES OVER THE SAME TABLE` explains why
the passes must stay separate.

**(b) Invariant.** A pass is append-safe **only for the rows that existed when it was
written**, because each pass walks *every* block before the next pass begins. Without a
bound:

- a **new block** would put its Morphable rows *before* the SUB's Structural rows and gate;
- a **new Morphable row in the SUB block** would do the same.

Either way the SUB's stored stepped values and its gate would move to different slots, so
every saved patch that holds them would be misread.

So the passes are a record of B195 and B203, not a mechanism you can extend. Since B240
they are **bounded** to the SUB's layout-9 rows (`inMorphL9EnginePasses`), so a new block
or a new SUB row is simply *absent* from them. T10 then fails ("MISSING from morphIds")
until the row is listed in `kMorphTailIds` (§3.1(b)). The correction of the old "STATION
appends here too" comment is at `src/hypersaw_clap.cpp:3131 CORRECTED BY B240`.

**(c) Check.** `morphlayout_check` T13 (§3.1(c)) sees an insertion anywhere in the order.
T10 sees a non-Device engine row that was never listed.

**(d) Incidents.**

- **B195.** Stepped SUB rows were missing from the field. That fix is why pass 2 exists.
- **B203.** The gate was missing. That fix is why pass 3 exists.

### 3.3 How a slot resolves: blend, pick, stepped, the ADR-108 hold

**(a) Mechanism.** `src/hypersaw_clap.cpp:4182 void morphStep(int samples)` runs once per
grid tick (256 samples at 44.1 kHz, derived from seconds) and only while morph is on
(`src/hypersaw_clap.cpp:8271 if (morphOn > 0.5) morphStep`).

- **Blend mode, continuous slot.** The target is the plain bilinear sum over **all four**
  corners (`src/hypersaw_clap.cpp:4218 if ((int)morphMode == 1 && !d->stepped)`).
  The lead map and the dependency graph are **not** consulted.
- **Otherwise.** `pickCorner` draws one corner per group lead (the Gumbel field). Then
  the **ADR-108 hold** applies: if the parameter is not live in the corner that won, it
  keeps its current value
  (`src/hypersaw_clap.cpp:4239 if (!depLiveInCorner(morphIds[i], k)) target`, evaluated
  by `src/hypersaw_clap.cpp:3344 bool depLiveInCorner`).
- **Stepped rows morph atomically.** `src/hypersaw_clap.cpp:4090 bool morphApplyTarget`
  takes the winner's value whole and rounds it
  (`src/hypersaw_clap.cpp:4095 if (d.stepped) next = std::round`). In blend mode a
  stepped row goes down the pick branch.
- **Atomic groups.** A group shares one decision through `morphLead`: the scale, each FX
  slot's type/amount/tone, and the whole routing block.
- **The intent bus.** When the flag is on, `src/hypersaw_clap.cpp:4725 void intentApply`
  replaces the chooser but applies through the same helpers. A new resolution rule must
  therefore be written in **both** places.

**(b) Invariant.**

- A new parameter that only means something together with others must join their group
  in the lead map.
- A dependency is declared, not coded (§7).

**(c) Check.** `routing_check` covers the routing group, and `intent_check` T-G covers
the hold order.

**(d) Incidents.**

- **B49.** An FX type came from one corner and its amount from another.
- **ADR-176 §3 / B142.** The routing block assembled from several corners.
- **ADR-108.** A flip landed on a parameter that could not sound.

### 3.4 Gates are level ramps

**(a) Mechanism.** An oscillator's enable (150/1150) and an engine block's gate are not
picked from one corner.

- The shared law is `src/hypersaw_clap.cpp:4149 double morphOnWeight`. The gain equals
  the bilinear weight of the corners that hold the switch ON.
- The osc enable applies it in `src/hypersaw_clap.cpp:4137 bool morphApplyOscEnable`
  and the engine gate in `src/hypersaw_clap.cpp:4175 bool morphApplyGateEnable`.
- The stepped flip is deferred to a weight of 1e-3 (about −60 dB).
- A new block's gate needs **one line** in `src/hypersaw_clap.cpp:4104 void setEngineGateRamp`
  (today: `src/hypersaw_clap.cpp:4106 if (id == kSubOscOnId) subOnW = w;`). Its renderer
  must then:
  - apply that weight through the ~8 ms one-pole, once per chunk, outside the voice loop;
  - silence on OFF (`src/hypersaw_clap.cpp:1915 if (v == 0 && subOn != 0) subAllOff();`);
  - re-strike held notes on ON (`src/hypersaw_clap.cpp:1928 else if (v != 0 && subOn == 0)`);
  - resume the ramp **from the weight**, not from its stale value
    (`src/hypersaw_clap.cpp:1938 subOnGainSm = subOnW;`).
- Morph-off releases every block's ramp automatically
  (`src/hypersaw_clap.cpp:7249 for (const auto &b : kEngineBlocks) setEngineGateRamp(b.gateId, 1.0);`).

**(b) Invariant.**

- The ramp is deterministic in the pad position under every law.
- At a pure corner the ramp equals that corner's stored value exactly.
- With morph off the ramp is 1.0, so it has no effect.

**(c) Check.** `morphlayout_check` T12 (`tools/morphlayout_check.cpp:35 T12 (B203) the block's GATE is a corner value`):
the audio moves continuously, the value reads only 0 or 1, and a pure corner is exact.
I found **no wired row for the oscillator case (B48)**. `grep B48 tools/` hits only T12's
comments.

**(d) Incidents.**

- **B48.** The human: "gradually bring the volume of the osc up … instead of picking an
  on/off value from one patch and a volume value from another". ADR-100's hard kill had
  been clicking at the blend boundary.
- **B203.** The SUB gate joined the field by the same law.

### 3.5 Exempt vs absent

**(a) Mechanism.**

- **Exempt** (ADR-109). The parameter *is* in `morphIds`, with its flag set
  (`src/hypersaw_clap.cpp:3174 bool morphToggleExempt`). Exempting writes the live value
  into all four corners, so un-exempting causes no jump. The field then skips the slot
  and leaves it live (`morphExemptSlot`, which also forces any gate ramp to 1.0).
- **Absent.** The parameter is not in `morphIds`:
  - edits apply live (`src/hypersaw_clap.cpp:3483 bool morphRouteEdit` returns true);
  - capture ignores it;
  - right-click has nothing to toggle;
  - `morphOwnersJson` omits its key. The GUI reads *key presence* as membership.

**(b) Invariant.** A parameter the player should be able to exempt must be a member.
Being absent is not the same as being exempt.

**(c) Check.** `morphlayout_check` T10 covers engine membership. Instrument membership
has no total check (§2).

**(d) Incidents.** ADR-109 A1. Globals outside the field made right-click "exempt"
silently do nothing.

### 3.6 Corner bit-identity

**(a)/(b)** At a pure corner the rendered state *is* that corner's stored state:

- blend multiplies by a weight of exactly 1 and 0;
- the pick is certain;
- a gate's ramp equals the stored value.

Any new resolution rule (B232 included) must keep this. It must also keep the flag-off
path bit-identical to the shipped one.

**(c)** `morphlayout_check` T12d covers gates, `routing_check` covers routing blend, and
`intent_check` section S covers the flag-off path.

### 3.7 Morph-on adoption (B222)

**(a) Mechanism.** When the **editor** switches morph on, `src/hypersaw_clap.cpp:7240 else if (morphOn > 0.5 && !morphWasOn && editorWrite`
calls `src/hypersaw_clap.cpp:3455 void morphAdoptUncontested`, which works group by
group:

- **The four corners agree:** the live value is adopted into all four, so morph-on
  changes nothing audible.
- **The corners differ:** the field keeps the group, and the live edit is *not* kept.
- **Blend mode:** each continuous slot is judged on its own.
- **What "agree" means:** `src/hypersaw_clap.cpp:3441 static bool cornerValuesAgree`.
  Continuous values agree within 6 significant digits, relative, because presets save
  at `%.6g`. Stepped values must be exactly equal.
- **Fresh instance:** it adopts unconditionally
  (`src/hypersaw_clap.cpp:7228 if (morphOn > 0.5 && !morphCornersAuthored)`).

**(b) Invariant.** A new member inherits all of this automatically, as long as
`readParam` returns the value the player set. §4 has a case where it does not.

**(c) Check.** `undo_check` layer 5 (full, `tools/undo_check.cpp:37 LAYER 5 — history fidelity, round 3 (B222)`).

**(d) Incident.** B222: "turning on morph reverts the routing matrix … kills whatever
patch you're working on". **Still owed as a ruling:** host automation of morph-on keeps
the old reverting path, and that behaviour is pinned.

### 3.8 B232: the off-corner blend rule (PROPOSED, not built)

**What the code does now.** An on/off switch is already a level ramp (§3.4). Every
*continuous* parameter of a source, though, blends across all four corners (§3.3). A
corner where the source is OFF still pulls the value. Example: Osc 2 detune is 0.1 in
the OFF corner and 0.8 in the ON corner, and the midpoint reads 0.45. ADR-108's hold
exists only on the pick path.

**The rule to build** (ROADMAP B232, not dispatched):

- In blend mode, a parameter whose source is OFF in a corner takes its weights only from
  the corners where the source is ON, renormalised.
- The rule is generalised through the dependency graph, so it covers 150/1150, 4015 and
  any future source *by declaration*.
- At a pure OFF corner the parameter still reads the corner's stored value.
- The switch-over happens under the −60 dB floor.
- It changes how existing patches sound, so it must land behind `engine_revision`
  (§6). It would be the first revision-2 law.

**What a new source must declare so it inherits the rule, whichever way B232 is built:**

1. **Exactly one on/off parameter**, stepped, with a default of OFF or ON stated in its
   own ADR. For a `kParams` source this is an `enable`-like per-osc row. For an engine
   block it is the `gateId`.
2. **A machine-readable statement that each of the source's parameters belongs to that
   switch.** Under B232's wording this is a `depends` entry such as `sub.wave` →
   `sub.on=1`. **Three preconditions are not true today:**
   - The depends tooling cannot name an engine key. The generator skips engine addresses
     (`tools/gen_depends_header.py:38 if not re.fullmatch(r'osc\d+', osc):`), and the
     checker resolves keys against `kParams` only
     (`tools/depends_check.py:54 _decl = src.split("kParams[] = {", 1)[1]`).
     A `sub.on=1` clause fails `./verify fast` today.
   - The generator flattens every clause into OR
     (`tools/gen_depends_header.py:82 flat = [c for g in groups for c in g]`), but a
     `,` means AND to the GUI (`src/gui/gui2.html:6189 !r.dataset.when.split(';').some(groupShows);`).
     So "source ON **and** mode = X" cannot be expressed to the engine.
   - `depends` also *hides* the control
     (`tools/gen_gui_controls.py:200 when = depends or`), so declaring `enable=1` on every
     oscillator row would hide an OFF oscillator's panel. The declaration probably needs
     its own column or clause kind. That is a ruling (§13).
3. **Its unarmed blend-mode edit rule.** `morphRouteEdit` spreads a blend-mode delta over
   all four corners. B232 does not say whether an OFF corner should receive a share.

---

## 4. The mod matrix

**(a) Mechanism.**

- **Sources.** There are `src/mod_core.h:43 static constexpr int kMaxSources = 24;` slots.
  - Slots **0–21 are used**, which leaves **22 and 23**. A third new source means raising
    `kMaxSources`.
  - Slot meanings are listed at `src/hypersaw_clap.cpp:2720 static hypersaw::ModCore makeModCore`.
    That function sets polarity: unipolar by default, and bipolar is declared there, as in
    `src/hypersaw_clap.cpp:2725 m.srcPol[18] = `.
  - The source value is written each tick in `src/hypersaw_clap.cpp:3855 void modStep(int samples)`,
    for example `src/hypersaw_clap.cpp:3963 mod.src[18 + i] =`.
  - The GUI's names are `src/gui/gui2.html:3174 const MOD_SRC_NAMES = [`. They are
    appended, because a saved route stores the slot index.
- **Destinations.** Any **continuous** parameter can be a destination
  (`src/hypersaw_clap.cpp:3541 bool modAddRoute`).
  - Stepped parameters are refused.
  - So are the matrix's own ids (`src/hypersaw_clap.cpp:3548 if (destId >= 161 && destId <= 177) return false;`)
    and the LFO/ENV source ids (`src/hypersaw_clap.cpp:3555 if (destId >= 269 && destId <= 288) return false;`).
  - The GUI menu mirrors this refusal (`src/gui/gui2.html:3335 function modDestOptions() {`).
    It lists only `input[type=range]` controls, so **a destination needs a knob in gui2**.
- **Depth units.** Depth is a **fraction of the destination's full range**:
  `src/hypersaw_clap.cpp:4019 double want = md->base + deltas[i] * span;`. The result is
  clamped to the destination's range, and polarity is applied before depth. A new route
  starts at a depth of 0.25 (`src/hypersaw_clap.cpp:3556 return mod.addRoute(srcSlot, destId, 0.25`).
- **The base.** The shell keeps each destination's base (`src/hypersaw_clap.cpp:2737 struct ModDest`)
  and writes `base + offset` through `applyParam`, guarded by `modFromMatrix`.
- **Pitch.** Pitch is a *synthetic* destination
  (`src/hypersaw_clap.cpp:834 constexpr uint32_t kModDestPitch`).
  - Param 161 is its depth in **semitones**, and ENV 2 is its source.
  - It is applied **per voice** (`src/hypersaw_clap.cpp:3926 noteExprSetPenv(s, semis);`)
    to the **swarm cores only**.
  - The SUB's only pitch-modulation surface is its own generic destination 4019 (±48 st),
    applied at `src/hypersaw_clap.cpp:8156 subPitchMod + (subMono`.
  - I found no write into `subs[]` from the bend (38), global pitch (101–103), MPE or
    ENV 2 paths. B196 is the queued row for the SUB and bend.
  - **A new source must state which pitch surfaces it follows.** None of them comes for
    free.

**(b) Invariant.**

- **Readback reports the base,** so state, capture and the GUI never see modulation
  (`src/hypersaw_clap.cpp:7564 if (const ModDest *md = const_cast`).
  - **This holds only for ids that reach that line.** Engine-block ids, routing ids and
    many shell-owned ids return earlier in `readParam`.
  - For a modulated SUB row, `readParam` therefore returns the **modulated** value.
    `stateJson`, `morphCapture` and B222's adoption then store that value. This is a
    hypothesis from reading the code, not a measurement (Q4).
- **Source slots and enum values append.**
- **A new modulator is a source, not a destination,** until B70's cycle rule is ruled.

**(c) Check.**

- `mod_check` (full, `verify:297 "$build_dir/mod_check"`): the sum law, scope and
  refusals.
- `lfoenv_check` (full): the B171 sources.
- `polarity_check` (full).

**(d) Incidents.**

- **ADR-136.** Destinations were added one at a time, because corrupting the base is
  "the classic matrix mistake".
- **ADR-138.** The pitch route is found by destination, never by index.
- **B171.** LFO-as-destination was refused until its cycle rule is ruled.

---

## 5. History

**(a) Mechanism.**

- **A node is made where a gesture bracket closes.**
  - `src/hypersaw_clap.cpp:6739 void guiGesture` enqueues the bracket and, on END, calls
    `src/hypersaw_clap.cpp:6760 void undoMarkParam`.
  - `src/hypersaw_clap.cpp:6788 void undoService` takes the snapshot once the queue has
    drained.
  - Editor value writes go through `src/hypersaw_clap.cpp:6754 void guiSetParam`.
- **In gui2, every control brackets through one latch per element:**
  `src/gui/gui2.html:3873 function gestureFor(el, idOf) {`.
  - A knob drag brackets on the pointer.
  - Everything else, including the keyboard path of a range and a native `<select>`
    popup, brackets around the value change itself.
  - Deliberate exclusions are *declared* in `src/gui/gui2.html:3866 const NO_HISTORY_IDS = new Set([178]);`.
  - Host automation never marks (ADR-160 (3)). That exclusion is structural: nothing on
    the audio thread can call `undoMark`.
- **What a node stores:** `src/hypersaw_clap.cpp:6696 std::string historyJson() const`.
  - That is the preset JSON at **full precision**
    (`src/hypersaw_clap.cpp:6698 std::string s = stateJson(/*lossless=*/true);`), plus an
    always-present `routing` key that only a history restore reads
    (`src/hypersaw_clap.cpp:6472 if (historyRestore)`).
- **A restore is a load.** `undoGoTo` calls `applyStateJson`, which calls `initState`
  first (§6).

**(b) Invariant.**

- **Every control marks exactly once.** A hand-built control must use `gestureFor`.
  Generated controls do so already.
- **Restores are total.** Restoring node N gives N's exact state *and* N's audio,
  whatever path led there. Any **non-parameter** state your source adds must ride
  `stateJson` / `historyJson` and be reset by `initState`. Examples: a stream
  continuation, a chunk, a name.
- **Deliberately not restored** (named in B222, still to be ruled): the ensemble stream,
  the in-flight morph glide, and the master-volume declick.
- **The snapshot budget.**
  - Each node reserves `src/undo_tree.h:56 static constexpr std::size_t kJsonReserve = 48u * 1024u;`.
  - B222 measured the worst lossless snapshot at 25,530 B.
  - A new source adds four lossless corner values per member to that.

**(c) Check.**

- `gui_history_check.mjs` (fast, `verify:175 node tools/labharness/gui_history_check.mjs`):
  - It **executes** gui2's wiring and exercises every `[data-p]` element.
  - It enumerates from the page, so generated controls are covered automatically
    (`tools/labharness/gui_history_check.mjs:52 TOTALITY, so a new parameter is covered`).
  - A control built at runtime is covered only if its builder is lifted into the
    harness, as `buildSubTab` is (`tools/labharness/gui_history_check.mjs:309 topLevelFn('buildSubTab'),`).
    **A new source's hand-built tab must be added to that list, or it is not tested.**
- `undo_check` (full, `verify:266 "$build_dir/undo_check"`):
  - layer 4 checks that one closed bracket makes one node
    (`tools/undo_check.cpp:21 LAYER 4 — every control marks (B191).`);
  - layer 5 checks restore fidelity **with rendering**;
  - the reservation assertion checks the snapshot budget.

**(d) Incidents.**

- **B191.** A `<select>` never marked a node. The pre-fix wiring failed 399 of 874
  scenarios.
- **B222.** Covers four problems:
  - morph-on had no node of its own;
  - routing was in no snapshot (B193);
  - corners were rounded to 6 digits;
  - node N sounded different depending on the path taken to reach it.
- **LIBRARY L0063.** "A state oracle must RENDER." The 120-seed gauntlet compared
  snapshots on instances that never called `process()`. It stayed green while the human
  heard the same node sound different.

---

## 6. Presets and state

**(a) Mechanism.**

- **What gets written.** `src/hypersaw_clap.cpp:6284 std::string stateJson` emits:
  - every `kParams` key;
  - every osc-2 twin as `o1.<key>`;
  - every engine row as `<prefix><key>`, unconditionally;
  - the morph chunk, `modRoutes`, `intent` and `presetName`.
  The host chunk (`state_save`) writes the same key set as `key=value` lines, plus a
  `routing=` line. **The preset JSON carries no routing** (B193). Only the host chunk
  and a history node do.
- **A load is a load.** `src/hypersaw_clap.cpp:6393 void initState(bool chunkOnlyState)`
  resets everything, chunks included, **before** any key is applied:
  - from a preset: `src/hypersaw_clap.cpp:6462 initState(/*chunkOnlyState=*/false);`;
  - from the host: `src/hypersaw_clap.cpp:8903 pl->initState(/*chunkOnlyState=*/true);`.
  An absent key also loads as its default (`src/hypersaw_clap.cpp:6210 static bool jsonNumber`).
- **Migrations run after that and are keyed on the JSON text.** An example is the
  missing-`enable` rule at `src/hypersaw_clap.cpp:6635 if (json.find("\"enable\"") == std::string::npos)`.
- **The Init patch.** The factory Init patch is an **empty** table in the generator
  (`tools/gen_factory_bank.cpp:131 {"init", "INIT - Init",`), so it is always "the shell's
  own defaults, written by the shell's own writer".
  - **Re-run `gen_factory_bank` after any parameter or default change.** The files then
    gain the new keys, and `bank_check` re-asserts byte-identical re-save.
- **The B100 header.** Every blob carries `{schema, engine_revision, build}`.
  - `src/hypersaw_clap.cpp:6165 static constexpr int kEngineRevision = 1;` is the only
    revision so far.
  - `engineRevision()` is the only read site.
  - A load sets the revision from the header, or 1 if the header is absent.
- **The queue.** A load pushes *two* writes per parameter through the queue: the default
  first, then the value. Per-osc rows cost four. The queue
  (`src/hypersaw_clap.cpp:2372 static constexpr uint32_t kQCap = 2048;`) has a measured
  peak of 1471. A large new block eats into that headroom.

**(b) Invariant.**

- **Sound-changing laws.** A change to how an *existing* patch sounds lands behind
  `engine_revision`. That means an ADR, `kEngineRevision` bumped, and the law gated on
  `engineRevision()`.
- **Defaults are inert** (§0).
- **Non-parameter state follows the chunk rule:** emitted only when it has left its
  default, keyed by id rather than slot, and reset in `initState`. Examples:
  `src/hypersaw_clap.cpp:3719 std::string ensembleChunk` and
  `src/hypersaw_clap.cpp:3765 std::string lfoChunk() const`.
- **178 `specimen` is not patch state** (ADR-147). Both load paths skip it; do not copy
  that to anything else.
- **Keep state keys short.** The host chunk formats each `key=value` line into a fixed
  buffer (`src/hypersaw_clap.cpp:8782 char line[80];`). A prefix plus key plus a `%.17g`
  value that does not fit is **truncated silently** by `snprintf`. Today's longest keys
  are far inside that limit, but nothing checks it.

**(c) Check.**

- `state_check` (full, `verify:262 "$build_dir/state_check"`): round trip, plus the B110
  queue assertion.
- `statefix_check` (full, `verify:272 "$build_dir/statefix_check"    tests/state_fixtures`):
  the B100 fixtures load bit-identically.
- `bank_check` (full, `verify:274 "$build_dir/bank_check"        docs/presets/factory`).
  Row **E** is "a load is a load": `tools/bank_check.cpp:405 E: A LOAD IS A LOAD`.
- `presetstore_check` (full).
- Block-specific rows such as `tools/subosc_check.cpp:1474 11d the state chunk carries all 20 prefixed keys`.
  That pin counts keys, so it moves with the block.

**(d) Incidents.**

- **B181 note 6.** An absent key used to keep the previous patch's value. The human
  heard it as "the sub stays on across presets".
- **B192.** The Init patch, and `initState` running first.
- **B110.** Queue truncation dropped osc 2's enable.
- **ADR-103.** The one exercised migrator.
- **B100.** The revision mechanism.

---

## 7. Presentation and GUI

**(a) Mechanism.**

- **One row per declared address.** `src/param_presentation.tsv` has exactly one row per
  declared address (`src/param_presentation.tsv:70 address	scope	label`). The columns
  are: address, scope, label, page, group, widget, unit, chunk, shown_when, scale,
  depends.
  - The address is `key` for a global row, `osc1.key` / `osc2.key` for a per-osc row, and
    `<prefix>key` for an engine row. Example: `src/param_presentation.tsv:419 sub.wave	sub	Wave`.
  - `chunk` must be named, or no control is generated.
  - `widget none` means the generator makes no control. Either the control is
    hand-placed (`sub.on`'s power button), or there is deliberately none (the retired
    `sub.sync`).
- **Generation.** `tools/gen_gui_controls.py` generates gui2's controls between its
  markers.
  - `depends` wins over `shown_when` (`tools/gen_gui_controls.py:200 when = depends or`).
  - Engine controls always carry `data-fixed`.
  - A per-osc control is remapped to the selected oscillator by `effId`.
  - A **patch-scope** row whose shell branch tests a raw id must carry `data-fixed`.
    `gui_reach` derives that set by parsing the shell's `id >= N && id <= M` /
    `if (id == N)` shapes.
- **Dependencies** (ADR-108). `depends` feeds the GUI, and through
  `tools/gen_depends_header.py` it also feeds the morph hold (§3.3). The GUI reads `;`
  as OR and `,` as AND. The engine header flattens both to OR, and engine addresses are
  skipped (§3.8).
- **Units and scale.** B213 governs these, and it is a plan awaiting D1–D6.
  - A unit currently lives in three places that can disagree: the C++ name string, the
    host text (a hand-written id list in `params_value_to_text`), and this table's `unit`
    column.
  - A new parameter reaches the host as a bare `%.3f` unless someone adds it to that list.
  - `scale log10` needs `min > 0`. Three rows declared log10 render linear because of that.
  - Mod depth and blend both act on the **linear** range (D6).

**(b) Invariant.**

- **The table is total.** Every declared address has a row. A missing row is a control
  that silently never appears.
- **Each new (page, group) needs a row in `tests/feature_tests.tsv`.**
- **The compact lab's embedded copy of the table must match**
  (`docs/design/compact-lab.html:693 BLOCK 0 — THE PARAMETER TABLE`). The join script
  that generated it is **not in the tree**, so for now new rows are added to it by hand.

**(c) Check.** All run in fast:

- `presentation_check` (`verify:92 python3 tools/presentation_check.py`).
- `depends_check` (`verify:96 python3 tools/depends_check.py`).
- `gen_gui_controls --check` (`verify:101 python3 tools/gen_gui_controls.py --check`).
- `gui_reach` (`verify:118 python3 tools/gui_reach.py`). It covers **ids < 1000 only**
  (`tools/gui_reach.py:28 if int(i) < 1000}`), so engine-block reachability is **not
  gated**.
- `test_table_check` (`verify:107 python3 tools/test_table_check.py`).
- `compact_lab_table_check` (`verify:186 python3 tools/compact_lab_table_check.py`).

**(d) Incidents.**

- **L0023.** A widened range with no control is an invisible feature.
- **The 29 dead controls.** An unpinned patch-scope control silently wrote osc 2's id.
- **B213.** The unit inventory.

---

## 8. Real-time safety and determinism

**(a) Mechanism.**

- **Allocation.** Every buffer is preallocated. There is no `std::vector` growth,
  `std::string` or `std::function` capture on the audio thread. Tables are sized once in
  `morphInit`, which runs at activate.
- **Time.** Time constants are expressed in **seconds** and converted per tick or per
  sample (ADR-009).
- **Randomness.** The only RNG is mulberry32 (`src/force_core.h:48 inline double rngNext(uint32_t &state)`).
  Seeds are derived from the patch seed, as in `src/hypersaw_clap.cpp:2847 uint32_t lfoSeed(int i) const`.
  A stream whose continuation matters rides the state chunk once it has drawn (B149's
  rule).

**(b) Invariant.**

- The audio thread performs no allocation, no lock, no clock read and no external call.
- The same seed and the same note order give identical output.

**(c) Check.**

- `rtsafety_probe` (full, `verify:380 "$build_dir/rtsafety_probe"`) counts `operator new`
  inside `process()`.
  - **Its window is only as wide as its event stream.** It sweeps ids 1..99
    (`tools/rtsafety_probe.cpp:133 pv.param_id = 1 + (uint32_t)((round * 7 + bi) % 99);`)
    and switches the SUB on (`tools/rtsafety_probe.cpp:115 subOn.param_id = 4015;`).
  - It **never turns morph on and never adds a mod route**. So `morphStep`, `intentStep`
    and the destination path are outside its window, along with any new source unless
    you add its gate and ids to the stream.
- `samplerate_check` (`verify:401 "$build_dir/samplerate_check"`), `subdiv_check`,
  `blocksize_check` (full): the code must not depend on sample rate or on block
  subdivision.
- `playbook_check` (fast) scans `src/*.h` and `src/*.cpp` for clock and unseeded-RNG
  tokens. **Before B233 nothing enforced this.**

**(d) Incidents.**

- **2026-08-06.** An allocation in `process()` "only shows up as a click on someone
  else's machine".
- **ADR-009.** Hand-tuned per-tick constants.
- **ADR-086.** A sample-count grid tracked the sample rate.
- **B149.** The unseeded ensemble-timing stream.

---

## 9. Parity

**(a) Mechanism.** A new engine or feature arrives as a single-file prototype in
`reference/` and is **protected**: an edit there is a spec change. The prototype's RNG
must be seeded; that is the one sanctioned edit. The SUB's chain is the template:

- `tools/golden/gen_subosc_goldens.mjs --selfcheck` (`verify:524 node tools/golden/gen_subosc_goldens.mjs --selfcheck`);
- the C++ oracle (`verify:526 "$build_dir/subosc_check"`), with L0-1 parity at 48 kHz
  and 44.1 kHz plus the spec's measured rows, each with a must-fail control;
- the lab-fidelity check (`verify:232 node tools/labharness/subosc_check.mjs`);
- an ADR (ADR-178).

**(b) Invariant.**

- Correctness means parity with the JS reference at ε = 1e-6 RMS, plus the invariant
  rows.
- **Any intentional divergence needs an ADR.**
- Re-baselining a golden is a protected-reference edit (LIBRARY L0054).
- A swarm extension that is default-off must keep `parity_check` green on the existing
  goldens (`verify:260 "$build_dir/parity_check"`).

**(c) Check.** The chain above, in full.

**(d) Incidents.**

- **LIBRARY L0031.** A reference oracle certifies agreement, and only over the surface
  it spans. Pair it with invariant oracles and a **shell-path** oracle, because an oracle
  that builds the core directly gives the shell zero coverage.
- **L0002 / L0012.** A failing parity check means a protocol mismatch or a chaotic
  regime. It never means "loosen ε".

---

## 10. Checks: wired or explained

**(a) Mechanism.** `tools/test_table_check.py:31 5. EVERY CHECK DECLARES ITS WIRING`.

- Every `tools/*_check.{cpp,py}` and `tools/labharness/*_check.mjs` carries **exactly
  one** line-anchored `WIRED: <where>` or `UNWIRED: <reason>` within its first 40 lines
  (`tools/test_table_check.py:104 DECL_RE = re.compile(`).
- That declaration is cross-checked against what `./verify` actually invokes.

**(b) Invariant** (ADR-180 §1, ratified 2026-09-19).

- **Adding** a check is not gated. Wire it in the PR that creates it.
- **Weakening** one is a human gate: removing, skipping, relaxing, excluding, or moving
  a pin so it admits a wrong answer.
- Moving a pin because the thing it counts has legitimately grown is not weakening, but
  it must carry its reason in the same line. Pins that move with a new source:
  - paramclass T1a (266 rows);
  - morphlayout T1b's band, and T10b's tail clause (both turn red at the first
    per-osc or global tail append, §3.1(c));
  - bank_check's `morphLayout 9`;
  - subosc 11d's key count.

**(c) Check.** `test_table_check` (fast) enforces this.

**(d) Incidents.**

- B190 layer 1 found checks whose wiring claims were false.
- ADR-179 §4 is the inversion of the old default.
- L0062: a ruling must also amend the agent charters that restate the old rule.

---

## 11. B238, pre-read (the first run)

The prototype has not arrived, so this section is an **expectation, not a plan**.

- **Where it lands.** B238 extends the swarm, which puts it in shape **(A)**: new
  per-osc `kParams` rows from 289 up, each with a twin.
- **The retirements.** It replaces hard sync (B185/B228; the swarm has no sync ids yet)
  and probably the saw-shape panel (129–132). **Retire those ids in place** (§1). Do not
  delete them.
- **Its hardest seam is §3.1.**
  - Since B240 the append site exists: one base id per line in `kMorphTailIds`, a marker
    bump to 10, and the ids frozen into `tests/morph_order.txt`.
  - `kMorphLateIds` is closed. It inserts before the routing block.
  - `morphlayout_check` T1b and T10b will still go red at the first per-osc tail append.
    Bring the ruling on their pins (§13 Q1) before writing code.
- **Parity.** It joins the swarm's parity chain. The prototype decides whether it
  becomes a new golden generator or a protected edit to `reference/swarmsaw.html`. That
  decision needs an ADR.
- **B232.** If B232 lands first, the extension's rows must declare their switch (§3.8).

---

## 12. Where the docs and the code disagree (as of `e07acac`)

1. **`depends_graph.h` claims the morph hierarchy and the GUI "can never disagree".**
   - The generator flattens `,` (AND) into OR
     (`src/depends_graph.h:11 // A parameter is LIVE when any of its conditions holds`).
   - Example: `src/depends_graph.h:85 static const DepCond kDep33[] = {{137, 0}, {138, 3}};`
     is `glide` depends `noteLawLink=0,noteLaw=3` (`src/param_presentation.tsv:134 glide	global	Note Lag`).
   - The GUI hides `glide` when either condition fails. The engine treats it as live when
     either condition holds.
2. **ADR-108 says `depends` is "the SINGLE declaration" for the GUI and the morph.**
   - The SUB's dependencies are written as `shown_when`, with an empty `depends`
     (`src/param_presentation.tsv:420 sub.width	sub	Pulse Width`), because the
     depends tooling cannot name engine keys.
   - So the morph hold never sees them.
3. **RESOLVED by B240: two comments said a new engine block is a one-row change.**
   - The dispatch comment now says the row buys dispatch only, and lists the rest
     (`src/hypersaw_clap.cpp:1189 in kEngineBlocks for the DISPATCH`).
   - The "STATION appends here too" comment is corrected in place (§3.2).
4. **RESOLVED by B240: `morphlayout_check` T10b was headed "no previously stored slot
   moved".** It is relabelled to what it checks, and T13 is the gate on that claim
   (§3.1(c)).
5. **The ADR-136 comment says "Readback reports base, so state, automation and the GUI
   never see the modulation"** (`src/hypersaw_clap.cpp:2734 reports base, so state, automation and the GUI never see the modulation.`). Engine, routing and many
   shell-owned ids return before the base intercept (§4).
6. **`presentation_check`'s docstring says "every parameter the shell declares has
   exactly one presentation row".** The routing ids (≥ 10000, host-exposed through
   `params_get_info`) have no rows and are not counted
   (`tools/presentation_check.py:44 def shell_addresses():`).
7. **`gui_reach` says "every declared param must be REACHABLE".** It only looks at
   ids < 1000.
8. **`params_count`'s comment says "every index a host already knows keeps its
   parameter"** (`src/hypersaw_clap.cpp:8587 uint32_t params_count`). Appending a
   `kParams` row shifts the enumeration index of every osc-2 twin, routing id and engine
   id. It is harmless, because CLAP keys on ids, but the comment is false.
9. **`applyStateJson` cites "subosc_check's 40-patch byte-identity row".** That row is
   `bank_check`'s row E (`tools/bank_check.cpp:405 E: A LOAD IS A LOAD`).
10. **The factory bank's header is stale.** `gen_factory_bank`'s BANK.md text says files
    carry "`morphLayout 2`" and its header says "325 parameter keys, four 224-entry corner
    arrays". The writer emits 9, and both counts have grown. CMakeLists calls
    `bank_check` "standalone and unwired", but `./verify full` runs it.
11. **ADR-173's heading still reads PROPOSED, with counts "over the 243 rows".** The
    derivation is live, `paramclass_check` is wired, and the table has 266 rows.
    `paramclass_check`'s header says "245 base rows" while its T1a pins 266.
12. **Two stale comments on the morph writers.**
    - `morphJson`'s comment says "2 = late per-osc rows appended last" and the function
      writes 9. The history lives at `cornerJson`.
    - The block at `src/hypersaw_clap.cpp:2678 QUANTUM MORPH (ADR-104)` calls the set
      "every PER-OSC parameter … Globals … stay patch-level". The ADR-104 A2 and ADR-109
      A1 appends since then say otherwise.
13. **The snapshot-size comment is stale.** `undo_tree.h` measures the adversarial
    snapshot at 10,774 B, and B222 measured 25,530 B. B222 already flags this.
14. **The table header is spliced.** The `scale` paragraph of `param_presentation.tsv`'s
    header sits in the middle of the `shown_when` grammar paragraph (lines 41–53).

---

## 13. Open questions (for the lead / human)

- **Q1.** *Half answered by B240:* the append site is `kMorphTailIds` (§3.1). **Still
  open:** T1b's band (ids ≥ 3000 after the ADR-159 prefix) and T10b's tail clause both
  reject a per-osc or global tail append. One option is to bound both to the layout-9
  slots of `tests/morph_order.txt` and leave everything beyond it to T13. That admits
  what they reject today, so it is a pin move and needs a ruling. B238 needs the answer
  first.
- **Q2.** *Answered by B240 (ratified 2026-09-24):* the whole layout-9 order is frozen in
  `tests/morph_order.txt` and checked by `morphlayout_check` T13.
- **Q2b** (found by B240). An append re-dealt the quantum draw. *Answered by B240:* the
  lead adopted the layout-9 draw prefix (§3.1(b)), which is bit-identical today and gated
  by T14/T15. It departs from the morph lab's draw order for fields longer than 273
  slots, so the lead owns recording it in DECISIONS.md.
- **Q2c** (the B240 critic). An **existing** parameter joining the field needs a load
  migration (B255), or old patches load it at its default under morph (§3.1(b)).
- **Q3.** What is B232's declaration channel? Options: a new column, a `depends` clause
  kind that does not hide the control, or accepting that an OFF source's panel hides.
  Related decisions: whether `,` becomes AND in the engine header, and whether an
  unarmed blend-mode edit feeds OFF corners.
- **Q4.** Should `readParam` report the **base** for modulated engine and shell-owned
  ids (§4)? If it should not, capture, state and B222 adoption store modulated values.
  This is not measured. A probe that routes LFO 1 to `sub.fine` and then saves would
  settle it.
- **Q5.** Should `rtsafety_probe`'s event stream switch morph on and add a route, so
  that the field and the matrix are inside its window?
- **Q6.** Should `gui_reach` cover engine ids, and should `presentation_check` cover
  routing ids, or should their docstrings say they do not?

---

## CHECKLIST: copy into the PR description, one line per seam

```
- [ ] 1  Ids: appended (next free 289 / a new ≥3000 block), never renumbered; retired ids kept in place; per-osc dispatch uses baseIdOf, global uses raw id; engine block < 35 rows or §1 sites guarded; new block has a dispatch row in its oracle; paramclass T1a pin moved with a reason
- [ ] 2  Class: every new row's class stated; Device rows carry a kParamClassOverrides entry + reason; membership decided separately from class
- [ ] 3  Morph: members APPENDED through kMorphTailIds, one line each (not kMorphLateIds, not a new pass-1/2 row); morphLayout bumped by one at all 4 writers; brand-new ids only (an existing param needs B255's migration); appended ids frozen into tests/morph_order.txt + a kLayoutPins row; atomic groups joined; switch is a level ramp (setEngineGateRamp line + renderer ramp, OFF silences, ON re-strikes, resume from weight); pure corner bit-identical; B232 declaration (switch id + belongs-to) stated
- [ ] 4  Mod: new sources appended (slots 22–23 left; srcPol + MOD_SRC_NAMES + modStep write); destinations continuous with a gui2 knob; depth unit = fraction of range; pitch surfaces the source follows stated
- [ ] 5  History: every control marks once (gestureFor; hand-built builders lifted into gui_history_check); non-parameter state rides historyJson and is reset by initState; undo_check layer 5 renders
- [ ] 6  Presets: defaults inert; absent key = default; gen_factory_bank re-run (Init follows); any sound change to existing patches gated on engine_revision with an ADR; queue headroom (kQCap) considered
- [ ] 7  Presentation: one TSV row per address (chunk named, widget, unit, scale, shown_when/depends); gen_gui_controls run; feature_tests.tsv row per new (page, group); compact-lab PT rows added
- [ ] 8  RT/determinism: no alloc/lock/clock on the audio thread; seconds not ticks; mulberry32 seeded from the patch seed; rtsafety_probe event stream extended to the new ids/gate
- [ ] 9  Parity: prototype protected in reference/ with seeded RNG; golden generator (--selfcheck) + <x>_check wired in verify full; ADR for every divergence; existing goldens untouched
- [ ] 10 Checks: every new *_check has one WIRED:/UNWIRED: line and is wired in this PR; no gate weakened; every moved pin states its reason
- [ ] ./verify fast and ./verify full green on the COMMITTED hash (.harness/last-verify.json); trace written
```
