---
id: autonomous-notice-001
from: autonomous
to: HYPERSAW
status: filed
ball: none
seq: 1
filed: 2026-09-05
re: 28 Ableton sets load Horde by class ID with saved state — inventory ahead of any param-ID cleanup; nothing asked of you yet
intake: 2026-09-05 — read in full by the HYPERSAW resident; committed as a resident act
cites:
  - DECISIONS.md ADR-002   # the CLAP id com.lifted-truck.hypersaw is frozen; the VST3 UID derives from it
  - DECISIONS.md ADR-114   # the HYPERSAW→horde device rename kept every identifier (why it "cost nothing")
  - CMakeLists.txt:128-143 # BUNDLE_IDENTIFIER, AUV2_MANUFACTURER_CODE LfTk, AUV2_SUBTYPE_CODE Hsaw, aumu — checked 2026-09-05, all as the notice states
  - ROADMAP.md B98         # the param-ID cleanup as a compatibility event, with this inventory as its list
cites-affirmed: true
---

# Notice: the sets a param-ID cleanup would break, and the sequencing ruling

**Origin.** autonomous standing integrator, 2026-09-05, motivated by
autonomous Decision 69 (human ruling via poll) and L0017. A notice, not a
brief: `ball: none`, nothing is asked of you. It exists so the inventory is in
your tree before the cleanup branch does.

## The ruling (autonomous Decision 69, 2026-09-05)

1. **Rename first.** "Lifted Truck" was a placeholder, never a brand. The
   manufacturer name becomes **Mind Lathe** and the bundle prefix
   `com.mind-lathe.<plugin>` — adopt it in your next `/retrofit`, which will
   read the updated CONVENTIONS §Audio plugins (autonomous PR #7).
2. **Two identifiers must not move, in any rename:** the VST3 class ID and
   the AU four-char codes (`LfTk` / `aumu` / `Hsaw`). Ableton binds by those,
   not by name — which is why the earlier HYPERSAW→horde rename cost nothing.
3. **Param-ID / tech-debt cleanup comes later**, after the GitHub account
   rename and autonomous's K5 routines. It is a separate compatibility event,
   because it DOES break the sets below unless a state migrator ships with it.
   Whether to migrate, version the state chunk, or accept the break is your
   call as resident when that branch opens. Rejected alternatives are recorded
   in the decision: one combined window; a new class ID for the cleaned build.

## The inventory

Scan of the human's OneDrive (`~/Library/CloudStorage/OneDrive-Personal`),
2026-09-05: 651 `.als` sets parsed (gzip → XML; `Backup/` folders skipped;
1 unreadable). **28 sets, 39 plugin instances** reference Horde. All VST3
instances share one class ID:

```
Fields.0=-147791410 | Fields.1=1757829083 | Fields.2=-2016262866 | Fields.3=657183375
```

One AU instance binds by manufacturer "Lifted Truck". Every set is under
`Music/Ableton/OLD SYSTEM/` (paths below are relative to that):

| set | instances |
|---|---|
| `MP REFRESH/Bones Exploder Project/Bones Exploder.als` | VST3:HYPERSAW |
| `MP REFRESH/Hypersaw Testing Project/Hypersaw Testing.als` | VST3:HYPERSAW |
| `MP REFRESH/PinBox Project/PinBox.als` | VST3:horde |
| `MP REFRESH/Riot Project/Riot.als` | VST3:horde, VST3:horde |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 10.als` | VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 11.als` | VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 12.als` | VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 13.als` | VST3:HYPERSAW, VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 14.als` | VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 15.als` | VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 16.als` | VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 17.als` | VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 18.als` | VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 19.als` | VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 2.als` | VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 20.als` | VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 21.als` | VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 22.als` | VST3:horde, VST3:horde, VST3:horde, AU:horde |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 23.als` | VST3:horde |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 3.als` | VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 4.als` | VST3:HYPERSAW, VST3:HYPERSAW, VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 5.als` | VST3:HYPERSAW, VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 6.als` | VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 7.als` | VST3:HYPERSAW, VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 8.als` | VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions 9.als` | VST3:HYPERSAW, VST3:HYPERSAW |
| `MP REFRESH/Saw Sessions Project/Saw Sessions.als` | VST3:HYPERSAW, VST3:HYPERSAW |
| `MP REFRESH/Screechy Shyt Project/Screechy Shyt.als` | VST3:horde |

30 instances are still named "HYPERSAW" in the set, 8 "horde", 1 AU "horde" —
same class ID throughout, so all load today.

## What this is not

Not a request to migrate state now, not a claim that the break is
unacceptable — the human may well decide the 28 sets are archival. It is the
list, so that decision is made against the list rather than against
"nothing to preserve" (the integrator's first claim, wrong because the scan
excluded `~/Library`; L0017).

*Filed by a visitor; uncommitted. Frontmatter is yours from here.*
