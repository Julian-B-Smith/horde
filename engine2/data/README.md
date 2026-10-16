# Input data

Everything here is input to the build and its tests; none of it is engine code. Appendix B of the
spec says how each file's keys translate to the engine's parameters.

| path | what it is |
|---|---|
| `bank/presets.json` | The bank as voiced: 83 presets in the input format of Appendix B (B.1), every preset fully resolved. Its `note` member names a sibling file that is not part of this tree; Appendix B's base values and key tables are the reference for the keys. |
| `envelopes/presets.json` | The 12 envelope presets of Appendix A in the same format, each with the envelope `shape` it must play (acceptance item 16) added to the bank format's members. Only the listed keys are present; the others take their base values (B.1). Equal to Appendix A, checked on every verify. |
| `phrases/` | The phrase set (10.3). The spec fixes its names and some durations but not its events: see the README there. |
| `version0-states/` | Example version-0 states (IF-4, B.2), for the test author. Not in the builder's tree. |
| `reference-renders/` | The frozen renders of the bank, for the test author. Not in the builder's tree (spec 10.1). |
