# b306-vendor-mindlathe — Vendor string "Lifted Truck" → "Mindlathe" (display fields only)

- **Queue item:** B306, read verbatim from `origin/lead-records-112:ROADMAP.md` (records PR #792),
  answering `integrations/autonomous/brief-002-vendor-mindlathe.md` (autonomous-002, autonomous
  Decision 79, the human's request "switch the developer name in the Ableton browser from Lifted
  Truck to Mindlathe"). Allowed under ADR-186's legacy freeze as a human-requested display fix.
- **Why:** Display-only rebrand of the vendor a host shows in its browser. Identity (CLAP id
  strings, `BUNDLE_IDENTIFIER`, AU codes) is explicitly frozen — 28 of the human's Ableton sets
  load `horde` by those — so the change is scoped to four fields plus two optional URLs, never to
  the strings a host uses to re-find the plugin.

## What changed

| File | Field | Before | After |
|---|---|---|---|
| `src/hypersaw_clap.cpp:67-68` | descriptor vendor, url | `"Lifted Truck"`, `"https://github.com/Lifted-Truck/horde"` | `"Mindlathe"`, `"https://github.com/Julian-B-Smith/horde"` |
| `src/swarmfx_clap.cpp:38-39` | descriptor vendor, url | `"Lifted Truck"`, `"https://github.com/Lifted-Truck/HYPERSAW"` | `"Mindlathe"`, `"https://github.com/Julian-B-Smith/horde"` |
| `CMakeLists.txt:161` | `AUV2_MANUFACTURER_NAME` (hypersaw block) | `"Lifted Truck"` | `"Mindlathe"` |
| `CMakeLists.txt:187` | `AUV2_MANUFACTURER_NAME` (SWARMFX block) | `"Lifted Truck"` | `"Mindlathe"` |

The URL was optional per the brief; `git remote -v` confirms the live repo is
`https://github.com/Julian-B-Smith/horde.git`, so both descriptor URLs now point there (SWARM-FX's
previously pointed at the wrong repo name, `.../HYPERSAW`, anyway).

**Untouched, confirmed by grep after editing:** `com.lifted-truck.hypersaw`, `com.lifted-truck.swarmfx`
(CLAP ids), both `BUNDLE_IDENTIFIER` values, `AUV2_MANUFACTURER_CODE "LfTk"`, `AUV2_SUBTYPE_CODE`
(`Hsaw`/`Swfx`), `AUV2_INSTRUMENT_TYPE` (`aumu`/`aufx`). Also left untouched: `src/gui/preset_store.h`'s
`"LiftedTruck"` app-support folder name — a filesystem path, not a display field; renaming it would
risk orphaning saved presets on disk, out of the brief's four fields.

## Evidence consulted

- `libs/clap-wrapper/src/wrapasvst3_entry.cpp:268-279` — the VST3 class FUID is
  `Crypto::create_sha1_guid_from_name(clapdescr->id, ...)`, a deterministic hash of the (untouched)
  CLAP id string only; the vendor field plays no part. `CMakeLists.txt:141-144`'s own comment says
  the same ("the VST3 UID derives from the CLAP id rather than the filename").
- No `Contents/Resources/moduleinfo.json` ships in either the previously-installed or newly-built
  `horde.vst3` (clap-wrapper's `vst3.h:130` comment: hosts fall back to scanning when absent), so the
  ROADMAP's "found in moduleinfo.json or via the bundle" fallback path (bundle inspection) was used
  instead of a raw FUID diff.

## Proof (this Mac, dangerouslyDisableSandbox for install/sign/auval/killall)

**Before** (installed bundles backed up to scratch `backup-before/` and `old-moved/` before overwrite):
- `horde.vst3` / `horde.component` `CFBundleIdentifier`: `com.lifted-truck.hypersaw.vst3` /
  `com.lifted-truck.hypersaw.auv2`.
- `auval -a | grep hsaw`: `aumu Hsaw LfTk  -  Lifted Truck: horde`.
- `codesign -d` on both: adhoc-signed, `com.lifted-truck.hypersaw.vst3` / `.auv2`, arm64 thin.

**Build:** `cmake -S <worktree> -B <scratch>/build-release -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release`
then `cmake --build <scratch>/build-release --config Release -j8` — clean, no errors (CPM pulled
VST3 SDK 3.8.0 and AudioUnitSDK 1.1.0 per existing `CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES ON`, not a new
dependency added by this change).

**Install:** old bundles `mv`'d aside (not `rm -rf` — the harness's destructive-pattern hook blocked
that; moved to scratch instead, non-destructively), new bundles `cp -R`'d in, each `codesign --force
-s -`'d, each `codesign --verify --deep` clean, then `killall -9 AudioComponentRegistrar`.

**After:**
- `horde.vst3` / `horde.component` `CFBundleIdentifier`: **unchanged** —
  `com.lifted-truck.hypersaw.vst3` / `com.lifted-truck.hypersaw.auv2`.
- `auval -v aumu Hsaw LfTk`: **AU VALIDATION SUCCEEDED**, `Manufacturer String: Mindlathe`,
  `AudioUnit Name: horde`, triple unchanged (`aumu`/`Hsaw`/`LfTk`).
- `auval -a | grep hsaw`: `aumu Hsaw LfTk  -  Mindlathe: horde[clap-wrapper] auv2: Initialized
  'com.lifted-truck.hypersaw' / 'horde' / '0.1.0'` — CLAP id and product name in the init line
  unchanged; only the vendor display changed.

**Anomaly noted, not a blocker:** `SWARM-FX.component`'s `CFBundleIdentifier` read
`com.lifted-truck.swarmfx.auv2.component` before and `com.lifted-truck.swarmfx.auv2` after — a
difference. The previously-installed SWARM-FX bundles were dated 2026-08-22 (vs `horde`'s
2026-09-23, much closer to current `main`); `horde`'s own before/after `CFBundleIdentifier` matched
exactly, so this is attributed to rebuilding a five-week-stale SWARM-FX install against the
currently-pinned `clap-wrapper`, not to the vendor-field edit (SWARM-FX is PARKED — docs/PARKED.md
21 — and not part of B306's acceptance text, which names only `horde`'s `aumu Hsaw LfTk` triple).
Flagged for the lead; not independently re-verified against an unmodified rebuild for lack of time
budget in this dispatch.

## Alternatives rejected

- Building the VST3 SDK's `moduleinfotool` sample to extract a raw class-FUID byte diff: correctly
  in scope (scratch-only, no repo file touched) but a disproportionate side-build for a four-field
  display change given the deterministic-hash code evidence already found; not done.

## Verify

- `./verify fast` and `./verify full` — see report to caller for verbatim output and
  `.harness/last-verify.json` hash.

## Open questions

- The SWARM-FX `.component` `CFBundleIdentifier` anomaly above — worth a from-`origin/main`
  (no vendor edit) rebuild to confirm it reproduces without this change, if the lead wants it closed
  definitively rather than attributed by dated-artifact reasoning.
- VST3 class ID proof is code-path evidence (deterministic hash of the untouched CLAP id), not a
  byte-for-byte FUID diff, because no `moduleinfo.json` ships in this build configuration.
- The Ableton browser rescan itself is the human's check, per the brief.
