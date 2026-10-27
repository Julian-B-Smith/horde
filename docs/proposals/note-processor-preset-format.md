# How a horde preset carries a hosted note processor (B550)

> **Origin.** horde lead, 2026-10-27, for ROADMAP B550 (ADR-282; Arp Q-H6, Q-H10). horde owes Arp this format before
> Arp authors its factory presets (the init and the starter bank) as horde preset files. **Status: RATIFIED 2026-10-27 (ADR-294)**: the human ruled on its one gap (§4), and the lead settled §6's other two points from the records. It
> applies a rule the human already ratified for FX modules (ADR-201 Q7: a module's patch at a morph corner is a
> FOUNDATIONS cascade `Preset` at the module's scope; `docs/proposals/rack-slot-contract.md` §6.2). It invents
> nothing beyond the one gap named in §4. The open points are in §6.

## 1. What is carried

A note processor (Arp, horde 1.0) contributes three kinds of data to a horde preset:

- **its settings**, the keys of its manifest (Arp `spec/params.json`: 47 keys, each continuous or discrete, every one
  a morph target, ADR-269);
- **its structured data**: Arp's user pattern, a small versioned blob (`arp-user-pattern/1`, Arp SPEC §4.6), one per
  preset and per corner (Q-H6, ADR-282);
- **horde's own state about it**: which corner holds which module preset, and the per-parameter morph mode overrides
  (ADR-291.3). These are horde's, never the module's.

## 2. The module preset (what Arp authors)

A note processor's module preset is a FOUNDATIONS cascade `Preset` (`core/include/foundations/preset_cascade.h`) at
the module's scope, plus its blobs. The scope is the module's address prefix in horde, `arp`, so Arp's key `order` is
`arp.order` in horde.

| field | what goes there |
|---|---|
| `scope` | `"arp"` |
| `values` + `basis` | every **continuous** setting (gate, swing, vel_fixed, …), stored absolutely with its basis (the cascade's exact-identity rule) |
| `absolute` | every **discrete** setting, as its value's **index in the manifest's `values` list** (enums) or its number (ints, bools as 0/1). The lists are append-only in Arp's manifest, so an index never changes meaning. |
| `routes`, `symbolic`, `payloads` | empty: a note processor has no routings of its own, and no adaptive state (RS-6.13) |
| `blobs` (beside the `Preset`, §4) | `{"arp.userPattern": "<the arp-user-pattern/1 text>"}`, stored verbatim; horde never parses it, and Arp validates it on load (`setUserPattern`) |

### The file Arp writes

One JSON object per factory preset, which horde imports:

```json
{
  "format": "horde.module-preset/1",
  "module": "arp",
  "module_version": "<sha256 of Arp's spec/params.json at authoring>",
  "origin_id": "<stable id, e.g. arp.factory.init>",
  "name": "Init",
  "preset": { "scope": "arp", "values": {}, "basis": {}, "absolute": {} },
  "blobs": { "arp.userPattern": "<arp-user-pattern/1 text, or omitted for the default>" }
}
```

- **Every key present.** A factory preset lists every manifest key, so a later default change never changes it.
- **Floats** round-trip exactly (the shortest round-trip form, never `%.6g`; RS-6.11).
- **Save, load, save** gives byte-identical text (RS-6.11).

## 3. The horde preset (what horde saves)

Per morph corner, `np["arp"] = {origin_id, module_version, preset, blobs}`, embedded by value (as `fx[<module>]` is in
RS §6.2):

- **One module preset locked across the four corners is the default.** Per-corner identity is opt-in (RS-6.3).
- **At the morph threshold,** discrete settings flip and continuous ones blend, as for any morph target. The selected
  pattern is a discrete setting, and so is `pattern = user` (Arp SPEC §4.6); the user pattern's content does not
  blend.
- **Outside the corners,** horde keeps its own state about the module: the per-parameter morph mode overrides
  (Follow / Quantum / Exempt, ADR-291.3) and the stream selectors (ADR-270). Neither is in a module preset.
- **Device-class keys:** none for Arp today. If Arp declares one, it is saved once per instance, outside every corner
  (RS-4.5).

## 4. The one gap: where a blob lives

RS §6.2 says a module's structure goes in `absolute` "as one `kBlob` key". But FOUNDATIONS' `Preset::absolute` is a
`map<string, double>`, so a blob cannot sit there. `payloads` holds opaque strings, but it is reserved for adaptive
state (RS-6.13, none in 1.0). **Ruled (the human, 2026-10-27, "Beside the preset"):** blobs are carried **beside** the cascade `Preset`, in horde's own corner
record (`blobs`), which needs nothing from FOUNDATIONS. The same reading would apply to Shriek's macro slots and Sluice's
patch blob, and RS §6.2's sentence would be amended to match.

## 5. Acceptance (B550)

- this format is ratified, and Arp is told;
- Arp's init and starter-bank presets load into horde's corner record and round-trip byte-identically (an admission
  row like A13 and A18, for a note processor).

## 6. Settled points

1. **The blob's home** (§4): beside the `Preset`, in horde's corner record (the human's ruling, ADR-294).
2. **`module_version`:** Arp has no manifest lockfile like Bulwark's or Scape's. Until it has one, the version is the
   sha256 of `spec/params.json`, which Arp's MANIFEST already pins. A lockfile, if Arp adds one, replaces it.
3. **The scope name** `arp`: no horde key or spec address starts with `arp.` (checked 2026-10-27), so it is free.
