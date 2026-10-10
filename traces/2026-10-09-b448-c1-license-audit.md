# b448-c1-license-audit — third-party license inventory, allow list and gate, wired into verify fast

- **Queue item:** B448 Phase 1 Wave C item C1 (ADR-197; `docs/strategy/blind-spot-armor.md` risk row 12:
  "an automated dependency license audit (SPDX) with an allow list").
- **Why:** the SBOM (`tools/gen_sbom.py`) lists what is pinned, not what each pin is licensed under or what
  rides inside it, and nothing checked either. `tools/license_audit_check.py` re-derives the components from
  the tree and holds them against a hand-kept inventory, a proposed allow list and the SBOM.
- **What landed:** `tools/license_inventory.json` (11 components: 8 ship, 1 build, 1 test, 1 docs),
  `tools/license_allowlist.json` (PROPOSED, not ratified), `tools/license_audit_check.py`,
  `docs/LICENSES-THIRD-PARTY.md` (generated; `--check` freshness), and one block in `verify` fast directly
  after `depends_check` (ADR-180 section 1).
- **Findings (facts, read from each component's own text at its pin):**
  - Ships: clap MIT, clap-wrapper MIT, choc ISC, VST 3 SDK MIT, AudioUnitSDK Apache-2.0, and three that no
    earlier list names: Microsoft WebView2Loader.dll (BSD-3-Clause, a binary embedded in
    `libs/choc/choc/gui/choc_WebView.h` lines 2093-2119, Windows only), {fmt} 11.1.4 (MIT with fmt's
    exception, `libs/clap-wrapper/libs/fmt`), and one PreSonus interface header
    (`libs/clap-wrapper/libs/psl`, a public-domain statement, no license text).
  - ROADMAP row B433's planned notices file names five components; the audit finds eight that ship.
  - The SBOM lists none of the three nested ones. The check prints them; it does not fail on them.
  - `docs/design-system/support.js` and `docs/design/logo/support.js` (byte-identical) are a third party's
    runtime with no license statement, tracked in a public repository. Docs only.
  - No third-party marker in `src/`, `h2/`, `tools/` or `reference/`; no external asset in `src/gui/*.html`.
- **PENDING THE HUMAN (exit 0 only while listed in `pending_human`; red on 2027-01-31):**
  `presonus-extensions` (ships) and `dc-runtime` (docs). A visible hole, not a pass.
- **Evidence consulted:** `.gitmodules`, the gitlinks, `CMakeLists.txt` 20-90 and 192; `libs/clap/LICENSE`,
  `libs/clap-wrapper/LICENSE`, `libs/choc/LICENSE.md`; `libs/clap-wrapper/cmake/base_sdks.cmake` (seven CPM
  packages) and `shared_prologue.cmake` 78-88 (an eighth) and 238-243 (fmt); `libs/clap-wrapper/src/wrapasvst3.h`
  31; `libs/choc/choc/gui/choc_WebView.h` 968, 2078-2127; the VST 3 SDK and AudioUnitSDK trees, LICENSE and
  README blobs at the pinned commits, and pluginval's at tag v1.0.4, read through the GitHub API; the SPDX
  license and exception lists 3.29.0 (every id on record exists there); `tools/gen_sbom.py`,
  `tools/release_path_check.py`, `tools/armor_coverage_check.py` (the `--today` and expiry precedent),
  `tools/choc_patch_check.py` (the not-checked-out WARNING), `tools/weakening_check.py`,
  `tools/test_table_check.py`; `docs/LICENSE-OPTIONS.md`; `docs/design-system/README.md`; ROADMAP rows B448
  and B433.
- **Calibration:** 29 planted controls read red, each on its own rule and for the stated reason (messages
  inspected), and 4 read green as planted, in a temp git repo with real gitlinks. On the REAL tree, each of
  the 11 entries was dropped in memory in turn and every one was noticed; so were an emptied `own_files`, a
  dropped nested fetch, an emptied `pending_human` and an id taken off each list. With every submodule
  treated as not checked out: 0 failures, 3 warnings. With `--today 2027-02-01`: red on both pending items.
  `--online` re-read the three upstream texts from their pinned commits: all three match.
- **Alternatives rejected:** a full SPDX matcher or a license-scanning dependency (the brief forbids the
  first, the charter the second); reading fetched SDKs from a `build-*/_deps` directory (untracked, may be
  stale, and makes a fast gate depend on a build); scanning untracked files (the audit is about what the
  repository redistributes, and the human's local drafts would turn it red); putting the public-domain
  header on the shipping list (not an SPDX id, and not this agent's call); editing `tools/gen_sbom.py` to
  list nested components (changes a release artifact, outside this brief).
- **Verify:** `./verify fast`, exit 0, git 95b202c (`.harness/last-verify.json`), with this trace in the
  tree. `license_audit_check` prints 0 failures and 2 PENDING THE HUMAN; `weakening` 0 increases;
  `test_table_check` GREEN. `verify full` was not run: the brief's target is fast, and the change adds no
  compiled code.
- **Open questions:**
  1. The check also holds non-shipping components to the wider list (shipping plus not-shipped). The brief
     lists only the shipping rule as a failure; without this the wider list would be read by nothing.
  2. `GPL-3.0-only` sits on the not-shipped list for pluginval (run by CI, never linked or redistributed).
     Proposed, like the rest of the list; the human may want an exception instead.
  3. The pending items' expiry (2027-01-31) is copied from the armor catalogue's holes, not chosen by the
     human.
  4. Should the SBOM name what rides inside a pin (three shipping components today)?
  5. Does B433's notices file take the three additional shipping components?
  6. The ten release questions in `docs/LICENSES-THIRD-PARTY.md` (VST name and logo, the Steinberg README's
     two statements about a signed agreement, Audio Units logo terms, platform runtimes, delivered design
     artifacts). Two PDFs were not read: `VST3_Usage_Guidelines.pdf` and `CLAP Logo Guidelines.pdf`.
  7. Offline, a fetched SDK's license text is held by its pinned commit; a bump needs `--online` run by hand.
