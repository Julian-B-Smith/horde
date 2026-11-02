# wire-edit-lab — selecting, editing, rerouting and deleting wires, on the running shell (B589)

- **Queue item:** B589. GUI session. The human's notes of 2026-11-01 asked to "select existing wires to reroute them or delete them, or change the modulation depth", and named the wires that control other wires' depth in particular. They then chose "Wires first" on 2026-11-02.
- **Why a lab:** W-a (ADR-341 r5) ruled the valve and the drop target and said nothing about editing a wire that exists. Nothing in the shell, master or bundle changed.
- **What it is:** `docs/design/wire-edit-lab.html` loads `gui3.html#mod,win` in a frame, silences the shell's wires view and mounts its own in the same place. The drawing is the master's `CompactWires`, kept close so a ruling ports back. Five questions, each with options that apply live.
- **Evidence consulted:** ADR-266 r2, ADR-340 r9 and r10, ADR-341 r5, B70, B72, the master's CompactWires, RouteRow and PolChip, `gui3/device.js` `routeFault` and `syncRoutes`, `gui3/shell-logic.js` `attachMap`.
- **Real:** ordinary wires. Every edit goes through the shell's bridge into the device's routes, so RC-4's refusals and the history are the real ones. S.drag holds the bridge off during a drag, which makes one drag one undo step.
- **MOCK:** depth wires, in the master's `S.depthRoutes` placeholder (B70 is the lead's). Undo restores one only when it left with its target wire. Sources read 0: the shell has no voices.
- **Found while building, for the build after a ruling (read from the code, not run on the shell):**
  - the master's destination button is 34% of the canvas wide, so it covers the wires' right ends (`bundle.js`, CompactWires `layout`);
  - the master's drop on a destination pushes a mapping with no duplicate check and the device has none either, so a second wire can be made for one source and destination (`bundle.js`, `drop`; `device.js`, `routeFault`).
- **Found by running it:** the ◇ pile up where the wires cross, four of them in one spot on the sample. The lab slides a ◇ along its wire to the nearest clear spot. That departs from W-a's "at its midpoint" and is flagged to the human.
- **Readings for the human to overrule:** one level only stays (B70's restriction, the master's refusal); a far end dropped on a box or a ◇ makes that kind of wire; rerouting keeps the route's id, while B72's proposed identity is (source, destination), so a reroute would need a rule if B72 lands.
- **Not checked:** a host's handling of right-click and Delete in a plugin window; the light scheme; other window scales; a keyboard route on the canvas (the grid stays that route, ADR-266).
- **Checks:** the lab's self-check drives the frame with pointer events, with controls in its header; a real mouse selected a wire and dragged its ◇ (40% to 61%, 34 px at 160 px per full range). The self-check reads 30 of 30, with the shell's own 46 of 46 in the frame (main today; #1327 adds its own). `./verify fast` exit 0 on this tree and on a scratch merge with #1327 and #1328.
- **Left out on purpose:** the note on B589, because #1327 edits the same row and two edits there conflict. It goes in with the ruling.
