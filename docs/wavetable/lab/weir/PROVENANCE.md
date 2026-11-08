# weir/weir_lab.wasm — provenance

A **vendored binary of a sibling repo's code**, held here under horde ADR-307 (the filters live in Weir;
"what is heard must be the code that ships", so the wavetable lab plays Weir's compiled C++, never a
JavaScript copy). Weir's name is allowed in horde's tracked files (ADR-307). Added by B659 (horde lead
session, 2026-11-08; the human, by poll: "Weir in the wavetable lab").

| field | value |
|---|---|
| source repo | Weir (`synthetic-worlds/Weir`, private remote) |
| Weir commit | `078f143` (`078f1436d78eccf52ffcc3471d1c6334d7d6f10e`, 2026-11-06), a clean tree: the module's own registry stamps `"build":"078f143"` (no `+edits`) |
| source files | `lab/weir_abi.cpp` (the exported C ABI, `WEIR_API`), `include/weir/filter.h`, `include/weir/svf.h`, `include/weir/smoother.h`, `lab/faults.h` |
| build command | Weir's `lab/build.sh`, run in Weir's tree by the horde lead (Weir is read-only to horde; nothing was built into it from here) |
| compiler flags (from `lab/build.sh`) | `em++ -O3 -std=c++20 -ffp-contract=off -fno-exceptions -fno-rtti -I include -DWEIR_BUILD="\"078f143\"" --no-entry -sSTANDALONE_WASM -sFILESYSTEM=0 -sINITIAL_MEMORY=16MB -sALLOW_MEMORY_GROWTH=0 lab/weir_abi.cpp -o lab/dist/weir_lab.wasm` |
| copied from | Weir `lab/dist/weir_lab.wasm`, byte-identical (sha256 checked on both copies) |
| size | 59,795 bytes |
| sha256 | `f8ccfb9a4af7857c56c6e8bbee6190f97806278bba496d6797bf47b1668fa87d` |
| date vendored | 2026-11-08 |

**What it exports** (used by `../wavetable-lab.html`): `weir_registry_json`, `weir_create`, `weir_prepare`,
`weir_reset`, `weir_left`, `weir_right`, `weir_mod_cutoff`, `weir_set`, `weir_process`, `weir_tail`,
`weir_response`, `weir_scratch`, `memory`, `_initialize`. It imports three inert WASI stubs
(`fd_close`, `fd_write`, `fd_seek`; Weir's `lab/wasm_env.mjs`). Memory is a fixed 16 MiB.

**Kind played.** `svf12` (registry index 0): SVF 12 in Simper's form with the energy level law, Weir's
ruled form (Weir D-013), Ring as its morph knob (D-015). The lab looks it up by id in the registry.

**Updating it.** Rebuild in Weir (`lab/build.sh`) from a clean Weir commit, copy the file here, and
update this table (commit, size, sha256, date) in the same change. A binary whose sha256 is not the one
above is not this provenance. The lab's interface to it is Weir's lab ABI, which Weir may change; the
lab's own calls are listed in `traces/2026-11-08-b659-wavetable-lab-weir.md`.
