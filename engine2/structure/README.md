# engine2/structure: the engine's declared structure

`engine-structure.json` is the structure of the new engine, as `specs/SPEC-ENGINE.md` states it: its
blocks, the reserved slots, what lies on its boundary, and the edges between them. loupe draws the
engine's diagram from this file alone, with no inference (ROADMAP B500). A gap here is drawn as a gap.

**The builder does not receive it.** The builder reads the spec and the tests and nothing else (BD-10);
this file is a reading of the spec, so the clean-room recipe leaves `engine2/structure/` out
(`tools/engine2_cleanroom.sh`, and its control in `tools/engine2_cleanroom_check.py`).

**It states only what the spec states.** Every item and edge cites the requirement ids, laws or
sections it rests on. What the spec leaves to the builder (code structure, threading, file layout) is
not declared: a block here is a concept the spec names, not a file or a class.

## The format, `horde.engine-structure/1`

One JSON object, in canonical form (sorted keys, two-space indents, items and edges ordered by id,
cites in natural order). Every entry has exactly these fields:

| field | what |
|---|---|
| `format` | `"horde.engine-structure/1"` |
| `spec` | `{path, blob}`: the spec this was checked against, by its git blob hash |
| `edge_kinds` | the closed list of edge kinds, below |
| `items[]` | `id`, `label`, `parent` (the item it nests in, or `null` at a root), `kind` (`block`, or `slot` for a reserved slot), `scope` (`engine`, or `external` for the boundary: the host, the mixer and its make-up gain, the FX rack, the master, the GUI, the pitch system, the modulation systems, history, the device's policy), `multiplicity` (`null` for one, else how many: a voice per sounding note, two oscillators, U members), `cites` |
| `edges[]` | `id` (`<from>-><to>`), `from`, `to`, `kind`, `label` (or `null`), `cites` |
| `signal_path[]` | each stage of §4.2's diagram, in its reading order, with the `ids` of the items or edges that are that stage |
| `counts` | items by kind and by scope, edges by kind, and the number of stages |

**Nesting:** the instrument holds the engine and the boundary; the engine holds the voice; the voice
holds the oscillator, the sub and the noise oscillator; the oscillator holds the member. The host is
the other root.

**Edge kinds:** `audio` (a sound signal), `control` (a value that sets or drives a block: pitch,
frequencies, parameter values, state), `event` (sample-timed discrete events: notes, voice starts,
transport, gestures), `modulation` (an offset that bends a parameter or a carrier's phase: per-voice
offsets, cross-mod and feedback), `observation` (a read-only tap: meters, the GUI snapshot, the
observation view, published modulation sources). An edge from a block to itself on a block with a
multiplicity runs between its instances (gravity between voices; cross-mod and feedback between members).

## The gate

`tools/engine2_structure_check.py`, in `./verify fast`. It checks the file against the spec's own text
(every cite exists, every §4.2 stage is mapped, every RC-n is cited, the counts are true, the deny list
holds) and fails with "the spec changed: review the declaration, then update spec.blob" whenever the
spec's blob moves, so every spec edit forces a review of this file. `--stages` prints §4.2's stages as
it parses them; `--canonical` prints this file in canonical form.
