# audit-h2-debug-header — one header for the 44 `hypersaw_debug_*` exports, 73 hand re-declarations deleted

- **Queue item:** unqueued: repo audit 2026-09-19 finding H2 (`docs/audits/2026-09-19-repo-audit.md`
  on branch `audit-2026-09-19`, PR #670). Lead dispatch, no ruling required — a src header
  and tool includes, not a gate.

- **Why:** 44 `extern "C"` debug exports were defined in `src/hypersaw_clap.cpp` and
  re-declared by hand in 14 files under `tools/`. `extern "C"` suppresses name mangling, so a
  prototype that drifts from its definition **links cleanly and reads garbage at runtime** —
  the failure class no oracle can see, because the oracle is the thing holding the wrong
  prototype (the shape `./verify:352` already names for accessors). `src/hypersaw_debug.h`
  declares each export once; `src/hypersaw_clap.cpp` includes it too, so a drifted
  *definition* is an error at the definition site as well as at every call site.

- **Evidence consulted:**
  - `docs/audits/2026-09-19-repo-audit.md` §H2 (claim, evidence, and the rejection of a
    table-driven `hypersaw_debug_query` — it would delete the compile-time checking that is
    the entire value).
  - `src/hypersaw_clap.cpp:7048-7385` — the 44 definitions and their section comments (the
    per-export "why" lines were compressed into the header, one line each).
  - `src/hypersaw_clap_entry.h` — the `hypersaw_test_*` hooks already have a header; the new
    one deliberately does not absorb them (they belong to the exported entry's contract).
  - `tools/include_check.py` — the include-what-you-use gate the new header must satisfy.

- **Counts (audit's figures corrected):**
  - Exports: **44**, exactly matching the audit. Set equality between
    `grep -oE 'hypersaw_debug_[a-z_]+\(' src/hypersaw_debug.h` and the same grep over
    `src/hypersaw_clap.cpp` was checked mechanically — identical, 44 each.
  - Hand declarations deleted: **73 declarations / 74 physical lines across 14 files**, not
    "70 across 13". Three corrections to the audit's table: `tools/statefix_common.h` (3
    declarations) was missed entirely; `tools/gen_factory_bank.cpp` has 4, not 5;
    `tools/ncap_check.cpp` has **0** — its only `hypersaw_debug_phases` is inside a prose
    comment at `tools/ncap_check.cpp:37`, which the audit's grep counted as a declaration.
  - Net lines: **+146** (`+236 / -90`), not the audit's predicted −11. The audit costed a
    bare 44-line prototype list; the brief's acceptance (a) requires "a one-line comment each
    (what it reads, which check owns it)", which is 44 comments plus subsystem banners —
    68 declaration lines, 56 blank, the rest comment, 217 lines total. The reduction is in
    *copies* (74 → 1), not in lines, and that was the point of the finding.

- **Findings reported, not acted on (out of scope):**
  1. **No live drift today.** All 73 pre-change hand declarations, transcribed verbatim from
     `origin/main`, compile clean against the new header
     (`c++ -std=c++20 -fsyntax-only`, scratch `drift_probe.cpp`, exit 0). The header closes a
     latent hazard; it did not uncover a present bug.
  2. **Three exports have no caller anywhere in the repo** — `hypersaw_debug_panic`,
     `hypersaw_debug_modpolarity`, `hypersaw_debug_set_engine_revision`
     (`grep -rn` over `src tools verify` finds only the definitions). They are declared in the
     header and marked `NO CALLER`. Deleting them is a separate decision (and would compound
     with the audit's separate note that the whole 668-line block has no `HYPERSAW_DEBUG`
     preprocessor guard and therefore ships).

- **Drift proof (acceptance (c)).** With one declaration mutated in a scratch copy
  (`hypersaw_debug_phases`'s `double *out` → `float *out`), the build fails at compile time
  and names the offending file, on BOTH sides:
  ```
  src/hypersaw_clap.cpp:7377:16: error: conflicting types for 'hypersaw_debug_phases'
   7377 | extern "C" int hypersaw_debug_phases(const clap_plugin_t *p, int osc, double *out, int cap)
        |                ^
  src/hypersaw_debug.h:205:7: note: previous declaration is here

  tools/bank_check.cpp:207:54: error: no matching function for call to 'hypersaw_debug_phases'
    207 |   double gapU() const { double ph[64]; const int n = hypersaw_debug_phases(p, 0, ph, 64); ...
        |                                                      ^~~~~~~~~~~~~~~~~~~~~
  src/hypersaw_debug.h:205:7: note: candidate function not viable: no known conversion from
                                   'double[64]' to 'float *' for 3rd argument
  ```
  Restored from a scratch backup; `shasum` of the restored header equals the backup's
  (`b29a958a0d90ec0c1f212efd7b885e4c79c76e4a`).

- **Alternatives rejected:**
  - *A table-driven `hypersaw_debug_query(verb, ...)`* — rejected by the audit and not
    revisited: one stringly-typed signature replacing 44 typed ones deletes the compile-time
    checking, marshals at every call site, and turns a rename into a runtime miss.
  - *Folding the declarations into `src/hypersaw_clap_entry.h`* — rejected: that header is the
    shipped entry's contract (the reason `routing_check.cpp:24-26` gave for hand-declaring in
    the first place). The separation is preserved; only the duplication is removed.
  - *Leaving `tools/statefix_common.h`'s three declarations in place* (its three dependents
    would have inherited them transitively) — rejected: every file that uses an export now
    includes the header explicitly, which is the `include_check` doctrine applied to a
    non-std header.

- **Verify:** `./verify full`, exit **0**, on committed hash — see the commit this trace lands
  in. `include_check: GREEN (107 files, every used std symbol has its header in the file)`;
  `state_check`, `routing_check`, `intent_check`, `undo_check`, `polarity_check`,
  `paramclass_check`, `bank_check`, `glide_check`, `time_check` all GREEN. Nothing in the
  audio path moved: the change is declarations and includes only.

- **Open questions:**
  1. Do the three NO-CALLER exports stay? They are dead weight in the shipping binary and are
     the cheap half of the audit's separate `HYPERSAW_DEBUG`-guard question. Lead's call.
  2. The audit's H2 line counts should be corrected at the source (14 files / 73 declarations,
     `ncap_check` has none) — `docs/audits/` is out of this brief's scope, so it is untouched.
