# Nightly runner (B448 P5, ADR-209 item 4)

One scheduled GitHub workflow calls `./verify nightly`. `./verify nightly` runs the
jobs in `jobs.json` for the current platform and writes one receipt. A row may be
green on a nightly result less than 48 hours old (ADR-209 item 4). Receipts are CI
artifacts, never commits.

| File | Role |
|---|---|
| `jobs.json` | The job list. The only file a later package edits. |
| `run.py` | The runner and the drill. Exit 0 green, 1 red, 2 bad job list. |
| `freshness.py` | Prints the one roundup line: last green night, `HOLE`, or `UNKNOWN`. |
| `stability_job.py` | One of the six jobs: the legacy SAW stability check, run long. |
| `legacy_gate.py` | Runs an older gate that prints `SKIPPED` and turns that into a recorded skip. |
| `../../.github/workflows/nightly.yml` | The workflow, installed 2026-11-10 (ADR-393). |
| `../nightly_check.py` | The fast offline check of all of the above (must-fail controls). |

## The job protocol

A job is a shell line run from the repo root. Exit non-zero means failed. On its
output it may print exactly these lines (the sibling kit's own forms):

```
KIT-GATE cases=<n>             how many cases it judged
KIT-GATE skipped: <reason>     it could not run here; exit 0
```

The receipt uses the kit's three words per job: `ran`, `skipped`, `failed`. A skip
is recorded and reported and is never counted as ran. A job with a `floor` that
prints no count, or fewer cases than the floor, is `failed`. A night where nothing
ran is red. No clock decides anything; `seconds` is information.

## Add a job

Add one entry to `jobs.json` and nothing else:

```json
{ "name": "soak-engine-x", "command": "python3 tools/soak.py --hours=4", "platforms": ["linux"], "floor": 12 }
```

`floor` is the fewest cases the job must judge (0 if it is not a loop over a
corpus). A new Linux job shares the one 180-minute Linux job (ADR-397), which whole
nights already fill to 96 to 146.5 minutes: check the margin in the table under "Measured
on the first nights" before adding one. Run `python3 tools/nightly_check.py` and `tools/nightly/run.py --platform
linux` with a short setting before pushing. The workflow and `verify` never change.

## Run a drill (monthly)

The drill proves the nightly can fail. In GitHub: Actions, nightly, Run workflow,
tick `drill`. Or here: `./verify nightly --drill`. It runs three planted jobs instead
of the real ones: one that fails, one below its floor, and a clean control. It
passes (exit 0) only if the runner went red on each planted fault, named it, and read
the control as ran. If the runner would have gone green, the drill exits 1. A drill
run is titled "(drill)" and is never counted as a night.

## Read freshness (the lead's roundup)

```
python3 tools/nightly/freshness.py
NIGHTLY green 2026-10-11 (9.4 h old, run 123456)
NIGHTLY HOLE -- last green night 2026-10-07 is 61.2 h old (limit 48 h)
NIGHTLY UNKNOWN -- gh failed: ...
```

It needs `gh` logged in. Paste the line into the roundup. The newest completed
night decides: a red night is a hole even if the night before was green. `UNKNOWN`
means it could not tell, and is never fresh. `--now ISO` and `--from-json FILE`
exist for tests.

## How `verify` runs it

The `nightly` target is one line in the final `case "$TARGET" in` block of `verify`:

```bash
  nightly) shift; exec python3 tools/nightly/run.py "$@" ;;
```

It does NOT call `record`: that overwrites `.harness/last-verify.json`, which the push
gate reads, so a local drill or nightly would erase the last `fast` result. The `shift`
matters: `$1` is the word `nightly`, and the runner takes only its own options. `exec`
makes the runner's exit code the target's exit code (0 green, 1 red, 2 bad job list).

The check, `tools/nightly_check.py`, runs in the `fast` target, right after
`test_table_check.py`; its header says `WIRED: ./verify fast`.

## The workflow

Installed at `.github/workflows/nightly.yml` on 2026-11-10 (ADR-393; drafted here 2026-10-10). The drill passed on
GitHub (run 42931019358, 2026-11-11), and the scheduled nights have run since (B671).

## Measured on the first nights (B671, B681)

| Night | Run | Linux | macOS |
|---|---|---|---|
| 2026-11-11 | 43007445035 | cancelled at its then 120-minute limit (stability 417 s, fuzz 3251 s, soak long-fold 1608 s; extreme-max read 100 of 150 periods at 23 min and was cancelled 32.1 min in, unfinished) | green |
| 2026-11-12 | 43205570893 | green, 96 min (stability 410 s, fuzz 3343 s, soak long-192 1267 s, extreme-loud 713 s) | green, 6 min |
| 2026-11-13 | 43412915840 | green, 146.5 min (stability 508 s, fuzz 3914 s, soak long-defaults 2082 s, extreme-dense 2274 s) | green, 6.7 min |

Each job's seconds are its receipt's, its build included (11-11 has no Linux receipt: its times are read from the log).
The Linux limit is 180 minutes since ADR-397; if a night ever needs more, the fuzz job moves to its own runner rather
than the limit rising again. The cron fires late (03:17 UTC scheduled, about 11:00 started). `brew install llvm` and both
sanitizer checks ran on the macOS runner every night so far (2026-11-11 to 13), none skipped.
STILL UNMEASURED: runner memory. UNCHECKED: whether the minutes are billed (see the workflow's macos comment). The stability job runs 3600 simulated seconds per patch, not the proposal's 14400,
because the check holds the whole render in memory: see `stability_job.py`.
