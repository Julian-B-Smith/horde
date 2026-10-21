# Design system

## What is here

| file | what it is |
|---|---|
| `HORDE-UI-Spec.dc.html` | **The UI implementation spec, v1** (2026-08-23) — tokens, widget state tables, visualizer recipes, morph presentation. Normative for **values**. |
| `HORDE-Design-System.dc.html` | **The consolidated system** (2026-08-23) — page compositions for MAIN, OSC, FX, MIX and the bend+scale panels, with the reasoning. Normative for **layout**. |
| `HORDE-Vaporwave-Phosphor.dc.html` | **The display direction** (2026-08-24, adopted) — data wells become dark phosphor tubes; the cream chrome stays. Normative for **screen tokens and trace rendering**; where it touches chrome values, the UI Spec still wins. |
| `HORDE-Morph-Knob-Spec.md` | **Morph knobs** (2026-10-21) — how a knob and a slider show and edit a parameter between morph corners: the blended value arc, weight-sized corner pips, the editing corner's pip and tether, latching, ownership cues, per-corner mod depth, locked parameters, quantum mode. Normative for **interaction** (ADR-261). |
| `HORDE-Map-Mode-Spec.md` | **Map mode** (2026-10-16) — the persistent MAP control and source selector; click to assign a source, drag for depth; stacked mod rings with origin dots and live ticks; the inspector; host constraints. Normative for **interaction** (ADR-261). |
| `HORDE-Morph-Knobs.html`, `HORDE-Map-Mode.html`, `HORDE-Slider-Marks.html` | The three browser prototypes behind those specs (knobs, map mode, and both on sliders). **Behaviour references, not parity targets**, as the specs say; their bilinear weights, random sources and fonts are illustrative. |
| `support.js` | The Claude Design canvas runtime both `.dc.html` files load. Vendored beside them **only** so the specs render from a checkout; it is not ours and is not edited. A separately-delivered `hp-support.js` was byte-identical (same SHA-256), so there is one copy, not two. |

The spec is kept **as delivered**, not transcribed into markdown. A normative
document with two copies is a normative document that will disagree with itself,
and this one is 200-odd exact values — hexes, pixel counts, angles, timings —
which is precisely the content transcription gets wrong. Read the `.dc.html`.

## How to read it

The spec declares its own precedence, and it is worth repeating here because it
decides every future argument about it:

> §1–7 are format-neutral values … normative for both stacks. §8 is the webview
> build … §9 is the native port appendix. **Where they conflict, §1–7 win.**

So: §1–7 is the design. §8 is one implementation of it (the one we ship today),
§9 is a second implementation for a C++ painter that does not exist yet. A
question like "what colour is a knob's filled arc" is answered in §1, never in
§8's CSS.

## Status: SPECIFIED, NOT BUILT

Nothing in `src/gui/gui2.html` implements this yet. The shipped interface is a
**dark** theme; the spec is a **cream** one. This is a re-skin, not a tweak, and
it is tracked on ROADMAP (B37) rather than applied piecemeal.

Read `DECISIONS.md` ADR-116 for the intake analysis: what the spec changes, what
it specifies that does not exist yet, and the one trap that must be fixed before
any of it lands (the corner palette currently exists twice in gui2, and the two
copies already disagree).

## Which document wins

Both, on different questions, and they say so themselves:

- `HORDE-UI-Spec.dc.html` — **values**. "§1–7 are format-neutral values …
  normative for both stacks."
- `HORDE-Design-System.dc.html` — **compositions**: what goes on MAIN, OSC, FX,
  MIX and the bend+scale panels, and the reasoning for each layout. Its own
  header defers on values: *"Values are normative in HORDE UI Spec."*

So a question about a hex, a radius or a timing is answered by the UI Spec even
if the Design System shows something else; a question about what a page contains
is answered by the Design System.

### Where they actually disagree

Found by reading them against each other (ADR-117). The value conflicts are
settled by the rule above — **UI Spec wins** — and are listed so nobody
"corrects" the implementation toward the losing side:

| | UI Spec (wins) | Design System |
|---|---|---|
| window | 1180 × 820, min 1000 × 700 | 1180 × 780, min 1000 × 660 |
| shadow | 1.5px 1.5px 0 | 2px 2px 0 |
| meter fill | `#A6F219` | acid `#9BE514` |
| `well` | `#FFFFFF` (`well-alt` = `#E9E3D4`) | `#E9E3D4` |

Two conflicts are **not** value questions and are **unresolved — they need a
human ruling**, recorded on ROADMAP as B37's blocking questions:

1. **Glyphs.** UI Spec: "NO glyphs anywhere", corner identity is colour + preset
   name. Design System: every corner carries one (GLASS ◆ / GRIT ▲ / HOLLOW ● /
   BLOOM ■) and "glyph always rides with colour". This is an accessibility
   argument as much as an aesthetic one — a glyph is how corner identity
   survives colour-blindness, which is why the UI Spec's own §8 offers preset
   chips for that job instead.
2. **Re-theming.** UI Spec: "corner colours never re-theme". Design System:
   "corner identity becomes per-direction, not global." These cannot both hold
   if a second visual direction ever ships. *Note (2026-08-24): the Vaporwave
   Phosphor direction shipped WITHOUT triggering this — it re-tunes only the
   signal hexes inside wells and leaves corner colours alone, so the ruling
   stays open rather than decided by accident.*

## Open question 3 — is there a DARK chassis? (raised 2026-08-24)

Both source documents describe exactly one world: a cream ground with flat ink
outlines. The human has since asked for "a light and dark mode for the whole
design", so `body.dark` now exists in gui2 as a **derivation, not a ruling** —
the values are mine, the designer has not seen them, and they are named here so
nobody later mistakes them for spec.

What the derivation assumes, so it can be corrected precisely rather than
wholesale:

1. **Ink inverts, ground gains a violet bias.** Near-black neutral read as a
   colour nobody chose; the bias keeps the chassis inside the instrument's own
   hyperpop world.
2. **Well/card contrast DIRECTION flips, not just lightness.** On cream a data
   well is *lighter* than its card (`--well #FFFFFF` on `--panel #FCFAF4`); on
   dark it must be *darker* (`#0E0C16` under `#1F1B2E`). "Recessed" is what a
   well means, and depth reads by contrast direction. Flipping tokens without
   flipping this relationship makes every well read as raised.
3. **Signal keeps its hues and its jobs; only emission moves.** One exception:
   `--ghost` is the dry trace *behind* the wet one, so on a dark ground it goes
   DARKER, not lighter — lifting it with everything else would put the
   reference in front of the signal.
4. **Screens are a separate axis from the chassis.** MODE switches the chassis;
   SCR switches `--scr-*` only. They pair by default (dark+tube, light+lumen)
   but can be crossed, because a design that cannot show you a pale screen in a
   dark chassis has hidden a decision inside a preset.

### A measured finding for the designer

The **light** chassis has elements at **2.84:1** — `.note`, `.vlabel`, and the
inactive `.tab`, all on `--dim` (#9A93A3) over `--panel`. The dark chassis puts
the same elements at 4.63:1. This is pre-existing and comes from the spec's own
`--dim` (the *hint* role), so it has NOT been changed here: `--dim` is a
normative value, and an inactive tab arguably is not a hint but navigation —
which is the same mapping question that was already got wrong once for labels
(they were on `--dim` at 2.84 and moved to `--t2` at 9.06). Ruling wanted:
should the inactive tab and `.vlabel` be *secondary* rather than *hint*?
