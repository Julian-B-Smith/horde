# The horde user manual — scaffolding

**Last verified: 2026-10-09** (B451, scaffolding only; no chapter prose is written yet).

This folder is the user manual for **horde 1.0**, which is horde 2: the new plugin shell of
ADR-186, with the roster frozen by ADR-190. It is not a manual for horde legacy (the frozen
HYPERSAW plugin). Today it holds structure only: every chapter, every section with a one-line
purpose and a status, and a placeholder for every figure, diagram, table, screenshot and audio
example the finished manual needs. The prose is written later, section by section, as each part
is ruled and built (B413: the manual follows the feature freeze).

## Audience

Musicians and sound designers who play and program horde in a DAW. Not developers: the specs,
labs, ADRs and ROADMAP are the developer record, and the manual never asks a reader to open them.
The manual explains what a control does to the sound and how to reach it, in the words on the
panel. Where the engineering matters to a player (CPU, latency, why a ceiling is −1.0 dBFS), it
gives the consequence, not the derivation.

## The map

| File | Chapter |
|---|---|
| [01-welcome.md](01-welcome.md) | Welcome and quick start |
| [02-installing.md](02-installing.md) | Installing horde |
| [03-interface.md](03-interface.md) | A tour of the interface |
| [04-engine.md](04-engine.md) | The engine: SCALPEL on the Kuramoto swarm |
| [05-sub-and-noise.md](05-sub-and-noise.md) | The Sub and the noise oscillator |
| [06-filters.md](06-filters.md) | Filters |
| [07-mixer.md](07-mixer.md) | The mixer, routing and gain staging |
| [08-modulation.md](08-modulation.md) | Modulation |
| [09-morph.md](09-morph.md) | Morph |
| [10-intent-and-macros.md](10-intent-and-macros.md) | The intent bus and macros |
| [11-fx-rack.md](11-fx-rack.md) | The FX rack |
| [12-master.md](12-master.md) | The master strip |
| [13-pitch.md](13-pitch.md) | Pitch: bend laws, glide, the quantizer, microtuning |
| [14-presets.md](14-presets.md) | Presets and the factory library |
| [15-history.md](15-history.md) | History (undo) |
| [16-performance.md](16-performance.md) | Performance, CPU and quality |
| [17-midi-and-mpe.md](17-midi-and-mpe.md) | MIDI, MPE, the arpeggiator and automation |
| [18-troubleshooting.md](18-troubleshooting.md) | Troubleshooting |
| [19-parameter-reference.md](19-parameter-reference.md) | Parameter reference (generated) |
| [20-glossary.md](20-glossary.md) | Glossary |
| [figures.json](figures.json) | The registry of every figure, table and audio example |

## Conventions

**Terminology.** Use the names the human ruled, and the panel's own words once GUI 3 exists:
- The product is **horde** (lower case in running text; the display name in a DAW is "Horde",
  vendor "Mindlathe", ADR-186 §3). The old plugin is **horde legacy**.
- The FX modules are **Shriek** (formerly MAW), **Sluice**, **Scape**, **ECHO**, **Bulwark**
  (the compressor and the master limiter; formerly Dynamite), **EQ**, the **FX filter** and the
  **drive** module, with the **Kuramoto chorus** CONDITIONAL (ADR-190).
- A **corner** is one of the four morph snapshots; a **global preset** holds four corners plus
  global attributes (ADR-192). **Blend**, **quantum** and **stepped** are the morph modes.
- The **swarm** is the set of coupled oscillators in one voice; one oscillator in it is a
  **member**; a **blade** is a SCALPEL window inside a member's cycle.
- The glossary (chapter 20) is the authority for any term; add a term there before using it.

**How parameters are written.** In **bold**, with the page and panel path the reader sees:
**OSC 1 › Blade 1 › Width**. A choice value is in small capitals style, written as it appears
(`SYNC`, `FM FREE`). A key or mouse gesture is written in words: double-click, Alt/Option-click.
Parameter names follow the generated lockfile (chapter 19); until it exists, a placeholder name
is marked *(name pending)*.

**Units.** SI and audio-standard, with a thin space before the unit when typeset:
Hz and kHz; ms and s; dB for gain, dBFS for absolute level; % for proportions; st for
semitones, cents, oct for octaves; bpm and note values (1/8, 1/8T, 1/8D) for tempo sync. A
default is written "default −1.0 dBFS"; a range "−24 … 0 dB". These match the units the shell's
value parser accepts (B449: Hz, k, dB, %, ms, st, cents, oct, choice labels).

**Status vocabulary.** Every H2 and H3 section carries exactly one status line, citing the
ROADMAP row or ADR that governs it:
- `Status: PENDING — <rows/ADRs>`: the part is not yet ruled or not yet built. Do not write its
  prose; the cited rows say what it waits on. Never invent behaviour to fill a pending section.
- `Status: READY-TO-WRITE — <rows/ADRs>`: the behaviour is ruled and stable enough that prose
  written now would survive. It still waits on screenshots where the GUI is unbuilt.

A section moves from PENDING to READY-TO-WRITE in the PR that cites the ruling or the build.

## Figures, tables and audio

A placeholder is a line of its own in a chapter, in exactly one of three forms:
`[[FIG:<id>]]` (a figure, diagram or screenshot), `[[TAB:<id>]]` (a table) or
`[[AUD:<id>]]` (an audio example). The id is lower case, dotted by chapter
(`engine.blade-anatomy`), and unique across the manual.

Every placeholder has one entry in [figures.json](figures.json), and every entry has one
placeholder. Each entry carries:

| Field | Holds |
|---|---|
| `id` | The placeholder id. |
| `kind` | `figure`, `diagram`, `table`, `screenshot` or `audio`. |
| `chapter` | The chapter file's stem, e.g. `04-engine`. |
| `purpose` | One line: what the reader learns from it. |
| `content` | What it must show. |
| `source` | Where the truth comes from: a lab, a spec, the manifest, a measurement. |
| `depends_on` | H2-PLAN part ids, ROADMAP rows or ADR ids it waits on. |
| `status` | `needed`, `pending-design` or `ready-to-make` (below). |
| `generated` | `true` if a tool should produce it (parameter tables from the manifest, measured plots), not a hand. |

Figure statuses:
- `ready-to-make`: its source of truth exists and is ruled; it can be made now (a diagram from a
  normative spec section, a curve from a closed form).
- `needed`: what it shows is ruled, but the thing to capture is unbuilt (a screenshot of a GUI 3
  page whose design is settled, a measurement of an unported core).
- `pending-design`: what it shows is not ruled yet; making it now would be inventing.

**To add a figure:** put the placeholder line where it belongs in the chapter, add its entry to
`figures.json` in the same change, then run `python3 tools/manual_scaffold_check.py` (it is also
in `./verify fast`). The check is red on a placeholder without an entry or an entry without a
placeholder, a duplicate id, a missing field, an unknown kind or status, an unknown
`depends_on` id, and a section without a status line. When the asset is made, it goes in
`docs/manual/img/` or `docs/manual/audio/` under its id, and the entry's `source` names how to
regenerate it.

## Not here yet

- No prose, no figures, no audio: this is scaffolding (B451).
- The parameter reference is generated later from the manifest and the parameter lockfile
  (B275, B398); chapter 19 holds its outline only.
