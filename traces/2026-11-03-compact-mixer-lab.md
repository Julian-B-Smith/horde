# 2026-11-03 — The compact mixer lab (GB1)

**What changed.** `docs/design/compact-mixer-lab.html`, a new lab: M1 cap over the meter, M2 an arrow on its edge, M3 a ghost fill and M4 rows, beside today's M0, at 104 and 72 px a strip. It runs on the real shell, unedited apart from one hook, so each fader is still the shell's Slider. Ledger: the lab's row, status exploring. ROADMAP: GB1's note. Index regenerated.

**Why.** The human, 2026-11-02: "Could we try a compact mixer? Maybe the slider could go on the level meter itself to save space". Then, 2026-11-03: "Yes, compact mixer".

**Evidence consulted.** `ChannelStrip` and `Slider` in the master; ADR-230 and ADR-333 (what the strip must keep, and "a little more space"); `docs/design/gui3/params.json` for the fader ranges.

**What the lab found.** The meter runs −66 to +6 dBFS, a source's fader −60 to +6 dB and Out's −60 to +26 dB. Side by side that doesn't show. With the fader on the meter it would lie, so M1 to M4 draw each meter on its own fader's scale. The self-check measures the bar's top against the cap on every strip, and today's Out strip, 25.3 px out, is its control.

**What the sizes show.** The merge alone saves no room in a vertical strip: M0 to M3 all measure 574 × 285 px at 104 px a strip and 414 × 285 px at 72, because the name, the peak and lamp, and M and S set a strip's width. M4 is the only form that changes the console's shape, at 505 × 201 px.

**Not checked.** A plugin host, the light scheme, a real signal (the levels are mock), and M4's rotated slider as a design: the master's Slider has a real horizontal form, which a build would use.

**Verify.** See the PR.
