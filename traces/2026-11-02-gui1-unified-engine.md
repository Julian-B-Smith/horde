# gui1-unified-engine — GUI-1: one unified engine, gated; three bugs of the human's fixed (B589, B584)

- **Queue items:** B589, B584. GUI session. The human reported the failed assumption and three bugs on 2026-11-01, and answered "Yes, all together".
- **The ruling:** GUI-1, the first on this session's own counter (ADR-346). Dated as the human said it.
- **Evidence consulted:** the master's samples and README, `docs/design/gui3/params.json` (the real rows hold no engine choice; Spread law and a blade's Frame really offer `swarm`), ADR-223, ADR-224, ADR-335 r3.
- **The gate:** `tools/engine_words_check.py`, wired in this PR under ADR-180 §1. It went red on five of the master's sample lines and four of its wording lines before any cleanup, so it sees what it claims to. Nine plants, eight must-read-clean lines. Three old-lab lines are pinned by name after reading them; a pin nothing trips goes red, so the list can only shrink.
- **The bugs:**
  - **Missing line:** `Viz.envPoints` stepped over stages shorter than one sample. With a 0.002 attack and 0.004 decay the line peaked at 0.06 under a node at 1.0. Corners are points now.
  - **Stripes:** a picture was drawn at 2 pixels while squeezed, then stretched when MAP went off, with no value changed so no redraw. The change test now includes the size. Reproduced on Sub with morph and MAP on, and after MAP off the canvas is back at 242 pixels for a 121 px box.
  - **Chips:** the shell's no-wrap strip let 16 chips shrink to 28 px. They hold their natural width now, and a picked source scrolls into view (an addition the fix needs, since only about five fit).
- **Not fixed:** Main's square envelope picture (B581's layout decides it), and a section's picture having almost no width while MAP and morph are both on.
- **A publish guard:** the artifact refused to overwrite files this session had not read. They were read, and their hashes equalled main's, before publishing.
- **Checks:** the shell's self-checks, 49 of 49 at the 980 px window; `gui3_seam_check` new test `canvases` with two plants; the five changed previews load with no errors and no engine wording; `./verify fast` exit 0 on this tree.
- **A gate's plant corrected:** `design_chain_check`'s clean case for "a GUI ruling sharing an ADR's digits" planted `GUI-1`, so the first real GUI ruling read as a duplicate and the check called itself blind. It now plants `GUI-300`, which still shares digits with an ADR. Nothing about the check was relaxed.
- **Not checked:** the light scheme beyond these pages; a plugin host. The envelope's peak was read by numbers and at a small size on screen.
