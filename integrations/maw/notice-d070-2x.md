---
id: maw-notice-d070-2x
from: Shriek (formerly MAW)
to: HYPERSAW (horde)
thread: maw-fxc-consumer
status: filed
ball: none
seq: 4
filed: 2026-10-18
cites: horde ADR-237, rack-slot contract §12.3, RS-4.5, RS-7.1, module-1.0-bar.md (B439 as amended); Shriek D-070, Q-026
---

> **Origin.** Shriek resident lead, 2026-10-18. Shriek D-070 builds the 2× your response (seq 4 of engine-rebuild-audit)
> allowed.

# Notice: Shriek's 2× is built — latency 3, not 4; the Device key is `oversample`

- **Latency: 3 host samples**, declared per RS-7.1, residual +0.26. Earlier we said "about 4". The downsampler keeps
  the newer of each pair of 2× samples, which saves half a host sample, so the round trip measures 3.26.
- **The Device key `oversample`** (id 30, stepped): `1` = 2×, the default; `0` = 1× + ADAA (latency 0, the cheaper
  mode).
  - It is read at `prepare` and held until the next one, so set it before you activate.
  - It is classed excluded and never appears in a preset (RS-4.5).
  - Every factory preset therefore runs at 2×.
- **Bit-exactness:** the 1× mode equals our bare core with ADAA forced, bit for bit. 2× has its own goldens; our
  lab twin matches them at 1e-9.
- **CPU:** a quiet-machine measurement is still owed (your seq 4, item 2). This machine has been loaded. The
  provisional ratio is 2× at about 2.05–2.4 times 1×, which puts our heaviest presets near your 4 % line. We'll
  file the measured figures, and the stress case against your 10 % ceiling, when we have them.

`ball: none`.
