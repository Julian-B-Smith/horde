# 2026-11-26 — The source routing lab: every source to the filters, the FX and Out, one graph (GB43); GB42 filed

**What changed.**
- **`docs/design/source-routing-lab.html`** (new, exploring, pinned for review):
  - The filters join the FX routing's row, so one ladder and one matrix route everything. It uses the master's FxRouting and FxWires, vendored and unchanged.
  - The lab's model is a subclass that adds the voice boundary: nothing from the summed side feeds a filter or a source, and the refusal is said in words. It also adds Q1's on/off sends.
  - The boundary is drawn as a dashed line over the ladder.
  - **The Mix picture:** the bundle's ChainConsole, its flow read from the graph at the pad. The lab swaps `H.ChainConsole.flow.edges` and `feeds` for flows it marks; any other flow is untouched, and the self-check holds that.
  - **Questions:**
    - Q1: filter sends as levels or on/off.
    - Q2: the dry path, a wire to Out or a Σ VOICES box.
    - Q3: make-up per path or at the master, asked in words.
    - Q4, a finding: with 11 boxes beside the matrix, the ladder scrolls at 980 px, so side by side or stacked.
- **Records:**
  - **ROADMAP GB43:** this lab.
  - **ROADMAP GB42:** the FX page on `hello.slots`, the row owed to the lead since the B714 S1 shapes, with its fixture and acceptance.
  - The ledger's lab entry, and the lab index regenerated.

**Why.** The human, 2026-11-26, asked for the Mix picture (GUI-14 r5), then: "Ideally I would like each of the sources to be able to route to filters and FX independently". Then, to the proposal to try it as a lab: "Yes, build the lab". The Mix picture waits on this, because what it draws depends on the answer.

**Evidence.**
- **The lab's self-check, headless at 1044 px:** GREEN, 9 checks, each with a control.
  - **The voice boundary:** Echo → Filter 1 is refused, and a refused wire is never written. Control: a filter into a module, a source into a filter and Filter 1 → Filter 2 are allowed.
  - **The ladder with 11 boxes** keeps the master's tidy measures: 0 shared ports, overlaps, band points, chip clashes and heads over a neighbour, with ports 4.7 px apart.
  - **Q1:** on/off puts a source-to-filter send at 100 %. Control: levels keeps 40 %.
  - **The Mix picture follows the graph:** Osc 2 → Shriek is osc2 → x3 IN. Control: removing the wire removes the edge.
  - **It is live with the pad:** at B the filters are in series. Control: at A they are not.
  - **Q2:** with dry as a wire to Out, SUM · VOICES and RACK IN are unfed. Control: with the Σ box they carry a level.
  - **The console's own flow is untouched** for an unmarked flow.
- **Renders:** side by side (the ladder scrolls, needing 500 px and getting 449) and stacked (950 px, nothing scrolls).

**Engine and FX device.**
- **Engine:** none yet. A lab rules nothing. Its engine half is per-source paths into the rack (GN-50), filter sends as levels (X13, RC-1) and make-up per path (Q3), a request until a spec round (ADR-361).
- **FX device:** no sources or filters; its row starts at the audio input.

**Ruled, GUI-76 (the human, 2026-11-26): "A, A, A, B".**
- **Q1 A:** source-to-filter sends are levels.
- **Q2 A:** the dry path is a wire to Out.
- **Q3 A:** one make-up, at the master.
- **Q4 B:** the routing views stack.

**Records:**
- the GUI-76 ruling;
- GN-63 (filter routing as wires), with GUI-76 added to GN-50;
- the ledger row `gui76-source-routing-graph` (ruled), with `engine_impact` GN-50 and GN-63;
- the lab entry (ruled);
- GB43's status;
- the lab opens on the answers, and its review tag is removed.

**The lead, 2026-11-26:** GUI-76 reopens ADR-411's Q15 pick (X110). The engine side (per-source taps in 1.0 or after, the CPU measurement first) is the lead's to put to the human. Filters in the graph are a SPEC-ENGINE round. B714's S3 holds its node list open.

**Open.**
- The master hop: FxRouting with source and filter nodes, the voice boundary, and stacking; ChainConsole's flow from the graph.
- Then the FX and Mix pages.
- The engine ruling is the lead's.
- **Verify:** `./verify fast`, exit code in the PR.
