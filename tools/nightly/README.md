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
| `stability_job.py` | The first real job: the legacy SAW stability check, run long. |
| `legacy_gate.py` | Runs an older gate that prints `SKIPPED` and turns that into a recorded skip. |
| `nightly.workflow.yml` | DRAFT of the workflow. The human installs it; see below. |
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
corpus). Run `python3 tools/nightly_check.py` and `tools/nightly/run.py --platform
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

## What the lead adds to `verify`

The `nightly` target is the lead's to add (this package changed `verify` only to wire
the check). In
the final `case "$TARGET" in` block (currently `verify:1264`), one line, before the
`*)` catch-all:

```bash
  nightly) shift; exec python3 tools/nightly/run.py "$@" ;;
```

and change the usage text to `./verify fast|full|nightly|report`. Do NOT call
`record`: it overwrites `.harness/last-verify.json`, which the push gate reads, so a
local drill or nightly would erase the last `fast` result. The `shift` matters: `$1`
is the word `nightly`, and the runner takes only its own options. `exec` makes the
runner's exit code the target's exit code (0 green, 1 red, 2 bad job list).

The check, `tools/nightly_check.py`, is already wired: `verify` calls it from the
`fast` target, right after `test_table_check.py`, and its header says
`WIRED: ./verify fast`. The only `verify` change left is the `nightly)` line above.

## What the human does with the workflow

1. Read `nightly.workflow.yml` (its header lists what is unverified).
2. From their own login, copy it to `.github/workflows/nightly.yml` and push. The
   file name must stay `nightly.yml`; `freshness.py` asks for it by that name.
3. Run it once by hand with `drill` ticked, then once without, and read the two
   receipts in the run's artifacts before trusting the schedule.

## Unverified until the first run on GitHub

The cron time; runner memory and speed against the dev Mac; `brew install llvm` and
the sanitizers on GitHub's macOS runner; whether macOS minutes are free on this
repository. The stability job runs 3600 simulated seconds per patch, not the
proposal's 14400, because the check holds the whole render in memory: see
`stability_job.py`.
