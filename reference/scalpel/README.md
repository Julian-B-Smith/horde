# SCALPEL handoff packet

Recommendation spec and oracle for porting the Scalpel bench (Waverazor-style blade synthesis on a Kuramoto swarm) into HORDE.

**v1.1 (addendum):** blade interplay — serial stacking, stacking order, collision → pitch / bite. See SPEC §13; verifier checks 14–18. Everything else is unchanged from v1, and patches with interplay off render identically.

| Path | What it is |
|---|---|
| `SPEC.md` | The recommendation: scope, normative DSP, port guidance, acceptance criteria, what not to replicate |
| `DECISIONS-staged.md` | Open decisions to resolve into `DECISIONS.md` |
| `ADR-draft-scalpel-parameters.md` | Draft ADR: new parameters, legacy defaults, parity policy |
| `prototype/scalpel-bench.html` | The bench itself (open in a browser; the published version of the same file) |
| `prototype/razor-core.js` | The DSP core, verbatim from the bench — the parity oracle (Node-loadable) |
| `verify/verify.js` | Headless verification battery; `node verify/verify.js [--bench]` |
| `verify/render-goldens.js` | Renders every preset to float WAVs + hash manifest for parity work |
| `verify/rng.js` | Seeded RNG installed over `Math.random` for reproducible oracle renders |
| `data/presets.json` | All 83 presets, fully resolved (base merged), by category |
| `data/parameters.json` | 111 parameters: label, group, default, range, curve, pad-assignable, smoothing class |

Requirements: Node 18+. No dependencies.

```
node verify/verify.js --bench        # ~30 s; exit 0 when every check passes (24 checks)
node verify/render-goldens.js        # ~45 MB into verify/goldens/
```
