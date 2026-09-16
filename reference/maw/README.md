# MAW handoff packet

- `SPEC-MAW.md` — spec recommendation sheet (read first)
- `core.js` — DSP core, parity oracle (`defineCore()` → `{MawCore, SHAPERS, AD, ROUTES, defaultParams}`)
- `fidelity.js` — property battery; `node fidelity.js` → `fidelity-report.txt`
- `tail.js` — release-tail timing per curve / floor
- `presets.js`, `preset-test.js` — shipped presets and their audibility/decay check
- `maw-horde-distortion-prototype.html` — the browser prototype (single file; core + presets inlined)

All scripts run with `node` and need nothing else.
