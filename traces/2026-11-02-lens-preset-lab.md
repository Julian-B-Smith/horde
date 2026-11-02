# lens-preset-lab and GUI-4 — Main is one deck; the Lens in one place and a place for presets (B581, B589)

- **Queue item:** B581. GUI session. The human polled on `main-workshop-lab.html` and chose B, stacked pads, with all four small panels, ENV 1 and Voice as tabs. They added: "Ideally there could be a lens on every page in the same place" and "there isn't any intuitive spot for loading global presets. Maybe somewhere in the top bar", and chose to see those before Main is built. The FX line went back to be workshopped.
- **Ruled in this PR:** GUI-4, amending GUI-2 ruling 1. The workshop lab opens on B, with ENV 1 and Voice as real tabs.
- **What the new lab is:** `docs/design/lens-preset-lab.html` loads the shell in a frame. The Lens is the bundle's `H.Lens` with every key in scope; each option places it and hides the pages' own. Two independent pickers: L0 to L4 for the Lens and P0 to P3 for presets.
- **Evidence consulted:** ADR-338 r5, ADR-340 r4, the master's Lens README, ADR-105, ADR-112, ADR-192 r1, ADR-321, the port ledger's `presets` row (B261), `docs/presets/factory` for the names.
- **Real:** the Lens, the pages and the measurements. **MOCK:** the preset control; the names are the factory bank's and nothing loads. Those files are in the legacy state format.
- **Measured on all 14 pages:** L1 and L2 gain no scroll on any page. L3 gains up to 567 px, on Mix. L4 gains 40 px on the five pages that already scroll. L3 and L4 cost every page, though pages that had room hide it.
- **Found while building:** a Lens in the foot row moves the bank's width from 651 px to 431 (L1) or 532 (L2). The bank's rows at those widths were not inspected.
- **Not checked:** the info sidebar setting with L2, the bank at the narrower widths, the morph dock with L3, a host, other window scales, the light scheme.
- **Checks:** the lab's self-check, 11 of 11, with its controls; the shell's own, 49 of 49, in the frame. The workshop lab's self-check, 22 of 22 after its edit, with its ten tabs measured.
