# 2026-09-14 — B125: a load's parameter writes must not be routed into corners

**Report (human).** After a corner's preset is changed and reverted, the corner
sometimes becomes corner A's preset; sometimes a corner load seems missing
from history.

**Mechanism.** `applyParam` → `morphRouteEdit` (ADR-109) with morph on writes
INTO a corner (armed / winning / weighted). `applyStateJson` and `state_load`
enqueue every parameter; `applyMorphChunk` sets the corners synchronously; the
drain then routes the queued live values into the corners. With the pad on A,
live == corner A → the armed / winning corner becomes A.

**Fix.** `src/hypersaw_clap.cpp`: `loadingState`; `ParamMsg.kind == 3` for
load values (`applyStateJson`'s enqueues incl. twins/migrator/enable; the
host chunk's queued branch); `drainQueue` sets the flag around kind-3
applies; the host chunk's direct branch sets it too; `morphRouteEdit` returns
"plain live write" while it is set.

**Evidence.** `morphlayout_check` T9 (processing rig; blend + B armed + pad on
A; quantum + pad on B control): FAIL with the bypass line disabled, PASS with
it. `./verify full` in the PR body.
