# 2026-11-26 — FX routing reaches the design master (GUI-75, GB41)

**What changed.**
- **The master (`1795724511-bd90`)** gains two pieces.
  - **`FxWires`**, the pure wire model:
    - nodes and wires, with a level per corner;
    - one writer, `setLevel`: 0 or less removes the wire at that corner, and a wire absent everywhere leaves the model;
    - `can()` refuses self wires, wires from Out and wires into an input;
    - Heard: the dominant corner decides whether a wire exists, and the corners that hold it blend its level;
    - Prevent feedback's held back edges;
    - the ladder's layout, its chip placement and `ladder()`, which is everything the view draws;
    - the lab's measuring instruments.
  - **`FxRouting`**, the view: the ladder beside the send matrix. It has GUI-75's gestures (drag box to box, drag a level, 0 removes, double-click for 100 %, keys in keyboard mode) and one `onChange` per gesture. It draws with FX cables' cards, cable colours and waiting line.
  - A README, a preview with the lab's busy patch plus its two return wires, typings, and a line in the master README.
- **FX patch's README** marks it superseded. Its code stays until the shell moves.
- **Vendored** `docs/design/arp-face/` at that version.
- **`tools/labharness/fx_routing_check.mjs`** is new, wired into `./verify fast`, with a read-only sandbox profile.
- **Records:** GB41's status, the ledger row `gui75-fx-routing-ladder` (vendored), `master_versions`, and the arp-face provenance row.

**Why.** GB41's next hop after the ruling. The human chose it on 2026-11-26: "Yes, start it", to "Start the master's routing component next?".

**Departures from the lab (in the README, open to the human):**
- **Cords are cased in `ink`**, as FX cables cases every cord. The lab cased them in `panel`, and the pale cables (Drive, ECHO) then fail 3:1 on cream.
- **Inputs are lanes** (`t2` or `hint`). The lab used `src-engine`, which is reserved for modulation.
- **Cord width runs 1 to 3.5 px**, narrower than FX cables' 0.8 to 5.6 px, because each wire has its own port.
- **Ladder columns are at least 44 px**, and a narrow host scrolls the ladder sideways. At a 333 px host the ports had packed 1.44 px apart.
- **Below 760 px of host width the two views stack.**

**Evidence.**
- `fx_routing_check`: GREEN. 56 ladder views: 0 shared ports, 0 overlaps, 0 band points, 0 chip clashes, the closest ports 3.16 px apart, 6 controls fired.
- **Plants on the vendored copy**, each red with its own line, then restored byte for byte:
  - one port per box half: shared ports and overlaps;
  - a 20 px minimum column: ports 1.44 px apart;
  - 0 kept as a level: "a level of 0 at A did not remove the wire";
  - every return wire held: the no-loop control;
  - every wire on track 1: overlaps and chip clashes.
- **Headless renders** of the preview: light at 1100 px (side by side), dark with Prevent feedback on at corner D, and 400 px (stacked, the ladder scrolling).
- **A browser probe** of the gestures, at 1100 and 760 px:
  - a matrix drag gives 80 %, and the chip agrees;
  - a drag to 0 removes the wire at A, and B keeps it;
  - a double-click sets 100 %;
  - a drag at Heard changes nothing;
  - a box drag Sub → Echo makes a wire at 100 %. A drop on nothing makes none, and a drop on an input makes none;
  - Prevent feedback switches "z⁻¹ 20 %" to "held 0 %";
  - four `onChange` calls for four gestures.
- **`design_chain_check`:** the master at 199 files matches VERSION, the vendored files equal vendor(master), and all 32 plants fired.
- **The artifact** had a newer version (1795724282-d259) than the repo recorded. Only the type's own files had changed, and the project files matched VERSION's hashes, so publishing kept everything.

**Engine and FX device.** Engine: GN-61 and GN-62, unchanged. FX device: the same, with its own inputs.

**Open.**
- The shell's FX page still mounts FX patch. That is the next hop.
- The readings above are the human's to confirm.
- The pick switch and the dead-end guard wait on B714.
- **Verify:** `./verify fast`, exit code in the PR.
