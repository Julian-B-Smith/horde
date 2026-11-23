# Spec round 65: the path modulator, after 1.0 (ADR-403, B704)

> **Origin.** Written for horde's lead, 2026-11-22, by an agent the lead dispatched. The human proposed a modulator
> that draws a path on an XY pad, whose x and y map to parameters, in place of B133's morph rail. The human ruled by
> poll: after 1.0, specified now; global or per note, like the LFOs; one shape per patch; two per patch. The lead
> records it as ADR-403. An independent reviewer read the first draft, and its findings are applied here.

## 1. What changed

| file | row | change |
|---|---|---|
| `specs/SPEC-ENGINE.md` | **E.10 (new)** | PM-1 to PM-15, after 1.0: two paths, the shape per patch, the curve, arc-length placing, the position p, global or per note, the sources, pads, the morph position, loops, real-time safety, determinism, what 1.0 reserves, the view and the FX device. |
| | **Appendix E's opening, E.9's after-1.0 list** | They name E.10. Rails are replaced by the path modulator. |
| | **MS-20, MS-29, MR-1** | Tagged [X93] only. No wording changes. |
| | **MS-36** | Tagged [X94] only. No wording changes. |
| | **§11.1** | X93 and X94 added; the count 73 → 75. |
| | **§11.2** | X95 to X100 added; the count 9 → 15. |
| `engine2/structure/engine-structure.json` | `spec.blob` | Re-pinned to the new text. E.10 is after 1.0, and the 1.0 rows gain tags only, so no item or edge changes. |

## 2. The two questions for 1.0

- **[X93] A Control route on the morph position.** MS-20 says the position sums as an engine key does, with
  (1 − W)·b + P + A. MS-29, IF-12 and IF-13 give the resolver only an offset added to `morph.x` and `morph.y`, and the
  modulation system never receives the host's position. As written, a Control route there still lets the host's
  position be heard. Item 36.9 does not pin it. The 2-D random's one-gesture mapping (MR-1) meets this in 1.0. What is
  built until it is ruled is IF-12 and IF-13 as written.
- **[X94] A later patch in a 1.0 build.** RC-9.3 keeps the engine's unknown keys. Nothing says what a 1.0 build does
  with an unknown modulator, source or route inside `mod`. The lead proposes carrying RC-9.3 into `mod`. Every
  after-1.0 modulator of E.9 shares the question.

## 3. The lead's readings after 1.0 (§11.2)

- **[X95]** 32 anchors, 64 samples per segment, and a new path's default.
- **[X96]** p is a corner key, resolved on its lin law whether the path is open or closed. Only MS-20's sum wraps on a
  closed path, so the resolver never reads `pathN.closed`.
- **[X97]** Scope and shape are per patch. A per-note path runs in the modulation system. Switching a path to Per note
  mutes its morph routes and names them, as MS-47 does.
- **[X98]** ADR-137's amendment ("rail wins") is superseded for horde 2. A macro may move the pad through a path's p.
- **[X99]** The intent bus's X and Y pads are sources only. Driving them is a new destination kind for the human.
- **[X100]** On the FX device a path reaches only the rack's declared mod values (MS-38).

## 4. Consequences outside the spec (the lead's to route)

- **The builder and the test author.** Nothing to build or test for E.10. Each is asked whether 1.0's format, as round
  62 specifies it and as each plans to build or test it, forecloses E.10, and is told of X93 and X94.
- **The GUI session.** GN-31 and GB17 still cite B133. The notice that they should point at B704 is owed (ADR-336).
