# Robustness matrix — measured (B101)

*Last measured: **2026-09-10**, branch `b101-release-engineering`, on an Apple
M3 (macOS 26.6.2, AppleClang 16.0.0 / clang-1600.0.26.6), Release build. Every
number here was printed by a tool in this tree — the command is beside each
table. If this date is old, regenerate rather than trust: the tools are cheap.*

Three kinds of claim are kept apart on purpose:

- **MEASURED here** — a tool ran on the date above and printed the value.
- **MEASURED in CI** — a Linux runner produced it; the link is the evidence.
- **UNVERIFIED — human** — nobody has done it; the row exists so the gap is
  visible rather than forgotten.

## 1. Sample rate x buffer size

`build-release/robustness_matrix` — the real plugin through the CLAP factory
(`src/hypersaw_clap.cpp`, id `com.lifted-truck.hypersaw`), the human's heavy
patch from `user_patch_bench` (both oscillators at 16 voices, comb + drive in
the rack, drift + width), a four-note chord held 1 s then released, tail
rendered 3 s. Per cell: `allocs` = operator new/delete calls inside
`process()` (the `rtsafety_probe` counter, applied at every rate and buffer —
`rtsafety_probe` itself runs at 44.1 kHz only); `nan/inf` = non-finite output
samples; `subn` = subnormal output samples (reported, not judged — see §3);
`peak` over the run (above 0 dBFS at this patch — B46's open gain-staging
ruling, not a defect this probe judges); `held` = RMS of the held second, the
must-read-loud control (a row whose notes never sounded would pass the rest
trivially; it must exceed -40 dBFS); `tail@3s` = RMS of the last 100 ms, must
sit under -60 dBFS (release 0.3 s puts the envelope ~87 dB down and the -80 dB
cull retires the voice); `cost` = wall time / audio time on this machine, for
the reader only. Verdict per cell: allocs 0, nan/inf 0, held > -40, tail < -60.

| rate | buffer | allocs | nan/inf | subn | peak dBFS | held dBFS | tail@3s dBFS | held cost | tail cost | result |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|:---|
| 44.1 k | 32 | 0 | 0 | 0 | +4.55 | -10.06 | -139.0 | 4.0% | 3.6% | OK |
| 44.1 k | 64 | 0 | 0 | 0 | +4.49 | -10.07 | -139.0 | 4.0% | 3.8% | OK |
| 44.1 k | 128 | 0 | 0 | 0 | +4.36 | -10.09 | -139.1 | 3.9% | 3.6% | OK |
| 44.1 k | 256 | 0 | 0 | 0 | +4.09 | -10.16 | -139.3 | 4.0% | 3.7% | OK |
| 44.1 k | 512 | 0 | 0 | 0 | +4.09 | -10.16 | -137.4 | 3.9% | 3.6% | OK |
| 44.1 k | 1024 | 0 | 0 | 0 | +4.09 | -10.16 | -137.4 | 3.9% | 3.6% | OK |
| 44.1 k | 2048 | 0 | 0 | 0 | +4.09 | -10.19 | -144.6 | 4.1% | 3.8% | OK |
| 48.0 k | 32 | 0 | 0 | 0 | +4.52 | -10.08 | -140.6 | 4.4% | 4.0% | OK |
| 48.0 k | 64 | 0 | 0 | 0 | +4.52 | -10.08 | -140.6 | 4.5% | 4.1% | OK |
| 48.0 k | 128 | 0 | 0 | 0 | +4.52 | -10.08 | -140.6 | 4.3% | 4.3% | OK |
| 48.0 k | 256 | 0 | 0 | 0 | +4.52 | -10.09 | -141.6 | 4.4% | 4.0% | OK |
| 48.0 k | 512 | 0 | 0 | 0 | +4.06 | -10.19 | -139.8 | 6.3% | 4.6% | OK |
| 48.0 k | 1024 | 0 | 0 | 0 | +4.06 | -10.19 | -142.6 | 4.5% | 4.3% | OK |
| 48.0 k | 2048 | 0 | 0 | 0 | +4.06 | -10.16 | -137.2 | 4.5% | 4.1% | OK |
| 96.0 k | 32 | 0 | 0 | 0 | +4.58 | -9.88 | -140.0 | 8.9% | 8.2% | OK |
| 96.0 k | 64 | 0 | 0 | 0 | +4.56 | -9.89 | -140.0 | 8.8% | 8.2% | OK |
| 96.0 k | 128 | 0 | 0 | 0 | +4.56 | -9.89 | -140.0 | 9.2% | 8.4% | OK |
| 96.0 k | 256 | 0 | 0 | 0 | +4.56 | -9.89 | -140.0 | 8.5% | 7.9% | OK |
| 96.0 k | 512 | 0 | 0 | 0 | +4.56 | -9.89 | -140.8 | 8.6% | 7.8% | OK |
| 96.0 k | 1024 | 0 | 0 | 0 | +4.10 | -10.01 | -139.2 | 8.5% | 7.8% | OK |
| 96.0 k | 2048 | 0 | 0 | 0 | +4.10 | -10.01 | -142.3 | 8.5% | 7.8% | OK |
| 192.0 k | 32 | 0 | 0 | 0 | +4.55 | -9.90 | -138.4 | 16.7% | 15.7% | OK |
| 192.0 k | 64 | 0 | 0 | 0 | +4.55 | -9.90 | -138.4 | 16.7% | 15.5% | OK |
| 192.0 k | 128 | 0 | 0 | 0 | +4.54 | -9.91 | -138.4 | 17.4% | 15.5% | OK |
| 192.0 k | 256 | 0 | 0 | 0 | +4.54 | -9.91 | -138.4 | 16.9% | 15.4% | OK |
| 192.0 k | 512 | 0 | 0 | 0 | +4.54 | -9.91 | -138.4 | 16.5% | 15.7% | OK |
| 192.0 k | 1024 | 0 | 0 | 0 | +4.54 | -9.91 | -138.8 | 16.6% | 15.4% | OK |
| 192.0 k | 2048 | 0 | 0 | 0 | +4.11 | -10.03 | -137.4 | 16.6% | 15.9% | OK |

`robustness_matrix: GREEN (0 failures)` — 28/28 cells. The probe's own
allocation counter was proven to fire first (`control: allocation counter
fires on a planted new/delete: yes`; the first draft's control read NO because
clang elided a paired new/delete expression — a direct `::operator new` call is
what the control now plants).

What the columns do NOT show: the held level moves slightly with buffer size
(-10.06 to -10.19 dBFS at 44.1 k) and the peak with it. That is the ADR-064 pan
motion known exclusion `subdiv_check` prints every run — buffer-dependent by
declaration, pending its own ruling — not a new finding.

**Time-declared behaviour across rates** — `build-release/samplerate_check`
(the ADR-009 gate `./verify full` runs; unchanged, quoted):

```
rate      attack 90% (s) gravity settle (s)
44100            0.23341          1.56744
48000            0.23311          1.57000
88200            0.23338          1.56744
96000            0.23314          1.56700
OK     envelope attack time is expressed in seconds  (worst drift 0.125% across 44.1-96 kHz (tol 0.3%))
OK     gravity settle time is expressed in seconds  (worst drift 0.163% across 44.1-96 kHz (tol 0.3%))
samplerate_check: GREEN (0 failures)
```

192 kHz is NOT in `samplerate_check`'s rate set — extending it changes a
gate's measurement (protected). At 192 k the matrix above certifies
allocation-free, finite, decaying output; time-constant drift there is
**unmeasured**.

**Allocation-free audio thread at 44.1 kHz, mixed buffers** —
`build-release/rtsafety_probe` (the gate; unchanged, quoted):

```
rtsafety_probe: 320 process() calls, block sizes 33..2048
  allocations inside process(): 0  (0 bytes)
  frees inside process():       0
rtsafety_probe: GREEN (audio thread is allocation-free)
```

## 2. Sanitizers

`tools/sanitize_oracles.sh <list> [build-dir]` builds every compiled oracle
`./verify full` runs — parsed from `./verify` itself, so the list cannot drift
— with `-DHYPERSAW_SANITIZE=<list>` (host-side executables only; the shipped
build is unaffected, see `CMakeLists.txt`), generates the goldens, and runs
each oracle with UBSan set to halt on the first report. The CI job `sanitize`
(`.github/workflows/ci.yml`) runs the same script on Linux for
`address,undefined` and `thread` on every PR.

| sanitizer | where | result |
|---|---|---|
| UBSan (`-fsanitize=undefined`, halt_on_error) | MEASURED here (AppleClang) | **GREEN — 25/25 oracles**, zero `runtime error` lines (`tools/sanitize_oracles.sh undefined build-ubsan`; parity 156/156, glide worst 3.5e-08, time worst 5.6e-12 — the same numbers as the unsanitized gate) |
| TSan (`-fsanitize=thread`) | NOT MEASURABLE here (AppleClang) | every oracle **segfaults at start**, and so does a hello world — see below; the verdict is CI's |
| ASan+UBSan (`address,undefined`) | MEASURED in CI (Linux, gcc 13; verdicts in run 34481570813, job GREEN in run 34482366205) | **23/24 oracles GREEN**, zero ASan/UBSan reports in them; `state_check` NOT BUILT on Linux; `rtsafety_probe` not run under sanitizers (both explained below) |
| TSan (`thread`) | MEASURED in CI (Linux, gcc 13; same two runs) | **23/24 oracles GREEN**, zero TSan reports; same two exceptions |

**Two things the Linux runs surfaced that this PR could not fix (both
outside its scope; recorded for the lead):**

1. **`state_check` does not link on Linux.** It calls `hypersaw_debug_state`
   / `_apply` / `_exempt` / `_cornervals` / `_exemptjson`, which
   `src/hypersaw_clap.cpp` defines (lines 4755-4768) inside the
   `#if defined(__APPLE__) || defined(_WIN32)` GUI block opened at line 4729.
   `corner_probe` has the same dependency. Until those hooks compile on Linux,
   the sanitizer job cannot cover the state oracle; the driver prints
   `DOES NOT BUILD on this platform — SKIPPED` and counts it, and its verdict
   remains the unsanitized `./verify full` on macOS/Windows.
2. **`rtsafety_probe` cannot run under a sanitizer, and its ASan report points
   at a real blind spot.** The probe replaces the global `operator new(size_t)`
   / `new[]` / `delete` with malloc-backed counters — the operators libasan and
   libtsan interpose — so under ASan it aborts with `alloc-dealloc-mismatch
   (operator new vs free)` and under TSan it reports `allocations 0 / frees 9`
   and goes RED. The mismatched buffer is `std::stable_sort`'s temporary
   buffer in `SwarmCore::finishRebuild` (`src/swarm_core.h:1355`), obtained
   through libstdc++'s `get_temporary_buffer`, i.e. the **nothrow**
   `operator new`, which the probe does NOT replace. `finishRebuild` runs from
   `rebuild()`, which `setParam` calls for `n`/`dist`/`seed`/`width`/`topo`/…
   (`swarm_core.h:421-424`) — on the audio thread whenever a host param event
   changes one of them. So: (a) the gate has a blind spot for nothrow
   allocations; (b) on **libstdc++** that sort heap-allocates on every rebuild
   (the 9 frees are 9 rebuilds inside armed `process()` windows), while on the
   two shipped platforms it happens to be allocation-free by library detail —
   libc++ sorts small trivially-copyable ranges in place and MSVC keeps a
   small temporary buffer on the stack (hypothesis for the mechanism; the
   measured fact is 0/0 on macOS and on MSVC, trace 2026-09-09). Allocation-free
   by construction would be a fixed-size stable sort over the ≤ 32 voice
   indices; that is a `src/` change and a gate change, both human-gated.

**ASan and TSan cannot be measured on this Mac**, and this is an environment
fact, not a finding about the code. With AppleClang 16.0.0 on macOS 26.6.2 a
four-line hello-world compiled with `-fsanitize=address` aborts inside the
runtime's own initialisation — `AddressSanitizer: CHECK failed:
sanitizer_malloc_mac.inc:189 "((!asan_init_is_running)) != (0)"` — and the
same file compiled with `-fsanitize=thread` segfaults (exit 139) before
`main`. Both reproduce inside and outside the sandbox, with and without
`ASAN_OPTIONS`; the oracles fail identically (`build-asan/*.sanitize.log`,
`build-tsan/*.sanitize.log`). No alternative toolchain is installed (Command
Line Tools only, no Homebrew LLVM). UBSan works, and is the one local
sanitizer measurement; ASan's and TSan's verdicts are the CI job's.

**What the sanitizer verdicts cover, honestly.** The header-only cores
(`swarm_core.h`, `spectra_core.h`, `filter_core.h`, `notch_core.h`,
`time_core.h`, `force_core.h`, `glide_core.h`, `swarmalator_core.h`) are
compiled INTO the oracles that include them, so they are fully instrumented
there. The shell (`HYPERSAW-impl`, the static library the plugin ships) is
linked into the impl-driven oracles (`state_check`, `rtsafety_probe`,
`mpe_check`, `steal_check`, …) UNinstrumented: ASan sees its heap misuse at
malloc/free boundaries only, and UBSan sees nothing of it. Instrumenting the
impl means a sanitized plugin library, which is a different build product —
recorded as an open question, not done here. TSan's verdict is narrower
still: every oracle is single-threaded by construction, so TSan certifies the
absence of races on paths where none can occur; the real concurrency — the
GUI-to-audio parameter queue and the host's note/param events — is not
exercised by any oracle and is UNMEASURED by TSan.

## 3. Denormals

**What the code does:** nothing explicit. `grep -rniE 'denormal|FTZ|DAZ' src/`
returns no hits (re-verified 2026-09-10; `tools/user_patch_bench.cpp` is the
only mention, and it is the A/B switch, not protection). The 2026-08-24 CPU
audit's finding 1 said this must cost; its own falsifier fired on 2026-08-25
(`docs/research/2026-08-24-cpu-audit.md` §1): with `HZ_FTZ=1` vs baseline,
M3 ~9.1 vs ~9.2 %, EPYC 7763 ~21.6 vs ~21.5 % over an 8 s release tail —
noise. The mechanism the audit later identified: voices are culled at
-80 dBFS (`voiceCull`, id 160) and the envelopes never linger near zero, so
the states that would go subnormal are retired first.

**How it was verified here** — `robustness_matrix` §Denormals, 48 kHz / 64,
release 5 s, 8 s tail in 1 s windows, FTZ/DAZ confirmed OFF in the process
(a subnormal survives a multiply):

| window | cost | subnormal samples | rms dBFS |
|:---|---:|---:|---:|
| held 1 s | 4.3% | 0 | -10.1 |
| tail t+1 s | 4.4% | 0 | -11.4 |
| tail t+2 s | 4.4% | 0 | -13.4 |
| tail t+3 s | 4.4% | 0 | -14.2 |
| tail t+4 s | 4.4% | 0 | -16.9 |
| tail t+5 s | 4.4% | 0 | -17.5 |
| tail t+6 s | 4.3% | 0 | -20.8 |
| tail t+7 s | 4.3% | 0 | -21.8 |
| tail t+8 s | 4.3% | 0 | -23.4 |

`worst tail/held cost ratio 1.01 (stall threshold 2.0)` — no stall signature,
and zero subnormal output samples across the tail and across all 28 matrix
cells.

**The control, and what it means for this row.** The probe times a synthetic
all-subnormal multiply-add chain against the same chain on normal values:
`x1.02 (22.0 ms vs 21.7 ms)` on this M3. **This CPU handles subnormals at
full speed, so the clean tail above is not evidence about x86**, where the
per-op penalty is 10-100x. The x86 evidence is the EPYC row of the 2026-08-25
A/B (`.github/workflows/cpu-derate.yml`, run 32812667405: baseline 21.18-21.24 %,
FTZ 21.21-21.48 % per tail second — no difference). Re-running
`robustness_matrix` on an x86 host is the way to make this row's control fire;
`cpu-derate.yml` is the workflow to add it to (`push` to a `measure/**` branch).

## 4. Host loads

`pluginval` strictness 10 is the standing proxy on both CI platforms. Real
host loads are a human act — no host is scriptable from here.

| host | platform | status | evidence |
|---|---|---|---|
| Ableton Live 12 | macOS | MEASURED — human | load + play + MPE + GUI (README "Validated"); stale-install trap recorded 2026-08-29 |
| Ableton Live 12.3 Beta | Windows 11 | MEASURED — human | `traces/2026-09-09-windows-first-run.md`: loads, plays, GUI renders; B99 residuals |
| auval (`aumu Hsaw LfTk`) | macOS | MEASURED — human | AU VALIDATION SUCCEEDED (Phase 0 gate, re-run at installs) |
| pluginval 10, VST3 | macOS (CI, post-merge) | MEASURED in CI | `ci.yml` job `build-macos` — searched for `HYPERSAW.vst3` from 2026-08-27 to this change and FAILED post-merge (run 34421409089); fixed here |
| pluginval 10, VST3 (GUI skipped) | Windows (CI) | MEASURED in CI | `ci.yml` job `build-windows` — same wrong name, validated nothing; a missing bundle now fails the step |
| **Reaper** | macOS | **UNVERIFIED — human** | neither host is installed on the dev Mac (Phase 0 residual, ROADMAP status 2026-07-17) |
| **Reaper** | Windows | **UNVERIFIED — human** | — |
| **Bitwig** | macOS | **UNVERIFIED — human** | — |
| **Bitwig** | Windows | **UNVERIFIED — human** | — |

## Regenerating this file

```bash
cmake --build "$PWD/build-release" --target robustness_matrix -j8 && ./build-release/robustness_matrix
./build-release/samplerate_check && ./build-release/rtsafety_probe
tools/sanitize_oracles.sh undefined build-ubsan      # this Mac: UBSan only; ASan/TSan are CI's
tools/sanitize_oracles.sh address,undefined build-asan   # Linux (or a Mac whose ASan runtime works)
tools/sanitize_oracles.sh thread build-tsan
```

Paste the printed tables, update the date line, and say which machine.
