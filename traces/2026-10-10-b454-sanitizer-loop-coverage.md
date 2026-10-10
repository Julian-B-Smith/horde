# b454-sanitizer-loop-coverage — the HYPERSAW_SANITIZE loop now reaches every executable

- **Queue item:** B454 item 1 (ADR-205 item 2).
- **Why:** the sanitizer `foreach` in `CMakeLists.txt` stamps flags on "the executables declared so far" and sat
  mid-file, so the 12 declared after it (`anchor_check` .. `bank_check`) were never instrumented; CI's `sanitize`
  job (which parses its oracle list from `./verify`) therefore ran 10 of them uninstrumented and never saw the
  other 2. The loop is now the LAST block of the file (executables covered 94 -> 106), with a comment saying why,
  and `tools/build_flags_check.py` goes RED on any `add_executable` below it.
- **Evidence consulted:** `CMakeLists.txt` (loop formerly at ~1052-1078, executables from `anchor_check` on at
  1084-1151); `tools/build_flags_check.py` / `build_flags_pin.json` (`dynamic_loops[...].executables_covered` was
  94 long); `tools/sanitize_oracles.sh` (oracle list parsed from `./verify`; env copied into the local runs);
  `./verify` lines 676-690 and 846 (10 of the 12 are wired; `preset_probe` and `gen_factory_bank` are not).
- **Alternatives rejected:** `cmake_language(DEFER ...)` (would cover every target by construction, but the
  guard's flat CMake reader would have to model function bodies and DEFER, and the brief's "executable after the
  loop reads red" control would have no meaning); a second collected list of targets (a list that drifts).
  Move-to-end plus a static RED on any later `add_executable` is the smaller change and fails loudly, not silently.
- **Guard (`sanitizer_reach_gaps`):** reads the tree alone (not the pin), so re-pinning cannot approve a gap away.
  Controls added to `selftest()`: executable after the loop (RED), the loop moved back above `anchor_check`
  (RED, names `bank_check`), no sanitizer loop at all (RED, so an empty reach cannot pass vacuously), executable
  above the loop bare and under `if(APPLE)` (zero gaps). The existing controls that appended an executable to
  the end of the file now insert it above the loop (`before_san_loop`). Selftest: 49 controls, all behave.
- **Re-pin:** `--approve <target>` cannot apply: the 12 targets' own flags did not change; the change is in the
  `dynamic_loops` section's `executables_covered`. One `--approve dynamic_loops "ADR-205 item 2; approved by the
  human 2026-10-10"`; the pin diff is exactly the 12 names plus the `approved` record.
- **Local sanitizer runs** (Homebrew LLVM 23.1.3, `-DHYPERSAW_SANITIZE=address,undefined` and `=thread`,
  RelWithDebInfo; env as `sanitize_oracles.sh`; args as `./verify`): 11 of 12 clean under both; no sanitizer report
  in any log. `preset_probe` exits 2 under both and ALSO in an unsanitized build of the same compiler: it is an
  unwired diagnostic whose printed verdict is `SYMPTOM` (the 2026-09-11 two-note bug), not a sanitizer report.
  Local-only workarounds, nothing committed: `-fno-objc-msgsend-selector-stubs -fno-objc-msgsend-class-selector-stubs`
  (the CLT linker cannot resolve LLVM 23's `_objc_msgSendClass$` stubs) and `-include cstdlib` (see open questions).
- **Verify:** `./verify fast`, exit 0, git 4a3dbaf (base commit; the change set was uncommitted when it ran),
  per `.harness/last-verify.json`.
- **Open questions:** (1) The MSVC `/STACK` loop sits above the same 12 executables and has the same shape; this
  brief covered the sanitizer loop only, so the 12 get no 64 MB stack on Windows. Not touched. (2)
  `src/glide_core.h:243` uses `std::labs` without `<cstdlib>`; `twocluster_check` fails to compile on libc++ from
  LLVM 23 until `-include cstdlib`. A portability note, not a sanitizer finding; not fixed (out of scope).
  (3) `preset_probe`'s standing `SYMPTOM` verdict is pre-existing and unwired; not investigated.
