# 2026-09-10 — B111: the audio-thread stable sort is allocation-free by construction

**What changed.** `SwarmCore::finishRebuild` sorted the ≤32 voice indices with
`std::stable_sort`, which on libstdc++ heap-allocates its scratch
(`get_temporary_buffer`, nothrow new) — and `finishRebuild` runs on the AUDIO
THREAD (`setParam` → `rebuild()`, swarm_core.h:421-424). B101's first Linux
sanitizer run saw it (TSan: 9 frees inside process(); ASan: the mismatch
inside the temporary buffer). libc++/MSVC happened to be allocation-free by
library detail, which is why rtsafety_probe never fired on this Mac — and it
is also blind to nothrow-new (B112). Replaced with an in-place stable
insertion sort: zero bytes, O(n²) ≤ ~500 compares, identical ordering.

**Evidence.** `./verify full` exit 0 on the branch (with the leak hotfix
applied). A/B against main's binary on the same goldens: all 156 per-scenario
parity RMS values identical to the digit (`diff` empty) — bit-identical
output, ties included. rtsafety_probe GREEN (0/0).

**Also here.** The trace-line alias from the leak hotfix (#539), so this
branch's own verify passes the leak gate; identical change, merges clean.
