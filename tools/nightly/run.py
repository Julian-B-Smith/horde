#!/usr/bin/env python3
"""nightly runner -- runs the jobs in tools/nightly/jobs.json for this platform and
writes one receipt (B448 P5, ADR-209 item 4).

    tools/nightly/run.py [--jobs FILE] [--receipt FILE] [--platform linux|macos] [--drill]
    ./verify nightly [same options]          (the lead wires this; see README.md)

THE JOB PROTOCOL. A job is a shell line run from the repo root. Exit non-zero is
failed. On its output, it may print, exactly:
    KIT-GATE cases=<n>          how many cases it judged (the LAST such line counts)
    KIT-GATE skipped: <reason>  it could not run here; exit 0; recorded as skipped
These are the sibling kit's own forms, and the receipt uses the kit's three words
for a job -- ran, skipped, failed -- so the two read alike.

WHAT IS RED (exit 1). Any job failed; any job with a floor that printed no count or
a count below the floor (recorded as failed, reason says so: a corpus loop that
judged fewer cases than it should reads green by itself); and a run in which NOTHING
ran (all skipped), because a night that judged nothing is not a green night. A skip
is reported, never counted as ran. A bad job list is exit 2.

NO CLOCK DECIDES. `seconds` in the receipt is information; no timeout lives here
(the workflow's timeout-minutes is the external backstop).

DRILL (--drill). Runs three planted jobs INSTEAD of the real ones (a monthly drill
that took four hours would prove nothing more): one that fails, one below its floor,
and a clean control. The drill PASSES (exit 0) only if the runner went red on each
planted fault AND named it, and read the control as ran. If the runner would have
gone green, the drill exits 1. The receipt carries "drill": true so nothing mistakes
a drill for a night.

RECEIPT. JSON at --receipt, default .harness/nightly-receipt.json (gitignored).
Nothing this writes is ever committed; CI uploads it as an artifact.
"""
import argparse
import json
import pathlib
import platform as _platform
import re
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
DEFAULT_JOBS = ROOT / "tools" / "nightly" / "jobs.json"
DEFAULT_RECEIPT = ROOT / ".harness" / "nightly-receipt.json"
PLATFORMS = ("linux", "macos")
FIELDS = ("name", "command", "platforms", "floor")
CASES_RE = re.compile(r"^KIT-GATE cases=(\d+)\s*$")
SKIP_RE = re.compile(r"^KIT-GATE skipped:\s*(\S.*?)\s*$")
TAIL_LINES = 15

# The planted jobs of a drill. Each is a plain job-list entry.
DRILL_FAIL = "drill-planted-fail"
DRILL_FLOOR = "drill-planted-below-floor"
DRILL_CONTROL = "drill-control-clean"
DRILL_JOBS = [
    {"name": DRILL_FAIL, "command": "echo planted failure; exit 3", "platforms": list(PLATFORMS), "floor": 0},
    {"name": DRILL_FLOOR, "command": "echo KIT-GATE cases=1", "platforms": list(PLATFORMS), "floor": 5},
    {"name": DRILL_CONTROL, "command": "echo KIT-GATE cases=5", "platforms": list(PLATFORMS), "floor": 5},
]


class JobListError(Exception):
    pass


def validate_jobs(doc):
    """Return the job list or raise JobListError naming the entry and the field."""
    if not isinstance(doc, dict) or not isinstance(doc.get("jobs"), list) or not doc["jobs"]:
        raise JobListError('job list must be an object with a non-empty "jobs" array')
    seen = set()
    for i, j in enumerate(doc["jobs"]):
        who = f"jobs[{i}]" + (f" ({j.get('name')})" if isinstance(j, dict) and j.get("name") else "")
        if not isinstance(j, dict):
            raise JobListError(f"{who}: not an object")
        for f in FIELDS:
            if f not in j:
                raise JobListError(f"{who}: missing field '{f}'")
        if not isinstance(j["name"], str) or not j["name"].strip():
            raise JobListError(f"{who}: name must be a non-empty string")
        if j["name"] in seen:
            raise JobListError(f"{who}: duplicate name")
        seen.add(j["name"])
        if not isinstance(j["command"], str) or not j["command"].strip():
            raise JobListError(f"{who}: command must be a non-empty string")
        p = j["platforms"]
        if not isinstance(p, list) or not p or any(x not in PLATFORMS for x in p):
            raise JobListError(f"{who}: platforms must be a non-empty list drawn from {list(PLATFORMS)}")
        fl = j["floor"]
        if isinstance(fl, bool) or not isinstance(fl, int) or fl < 0:
            raise JobListError(f"{who}: floor must be an integer >= 0")
    return doc["jobs"]


def load_jobs(path):
    try:
        doc = json.loads(pathlib.Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as e:
        raise JobListError(f"cannot read job list {path}: {e}")
    return validate_jobs(doc)


def this_platform():
    s = _platform.system()
    if s == "Linux":
        return "linux"
    if s == "Darwin":
        return "macos"
    raise JobListError(f"unsupported platform {s!r}; pass --platform linux|macos")


def run_job(job):
    """Run one job; return its receipt row. Streams the job's output as it comes."""
    t0 = time.monotonic()
    p = subprocess.Popen(job["command"], shell=True, cwd=ROOT, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True, errors="replace", bufsize=1)
    lines, cases, skip = [], None, None
    for line in p.stdout:
        sys.stdout.write(line)
        sys.stdout.flush()
        lines.append(line.rstrip("\n"))
        m = CASES_RE.match(line)
        if m:
            cases = int(m.group(1))
        m = SKIP_RE.match(line)
        if m:
            skip = m.group(1)
    rc = p.wait()
    row = {"name": job["name"], "status": "ran", "reason": None, "cases": cases,
           "floor": job["floor"], "seconds": round(time.monotonic() - t0, 1)}
    if rc != 0:
        row["status"], row["reason"] = "failed", f"exit {rc}"
        row["tail"] = lines[-TAIL_LINES:]
    elif skip is not None:
        row["status"], row["reason"] = "skipped", skip
    elif job["floor"] > 0 and cases is None:
        row["status"], row["reason"] = "failed", f"printed no KIT-GATE cases= line but has a floor of {job['floor']}"
    elif job["floor"] > 0 and cases < job["floor"]:
        row["status"], row["reason"] = "failed", f"below floor: judged {cases}, floor {job['floor']}"
    return row


def run_all(jobs, plat):
    """Run every job listed for `plat`; return the receipt (not yet written)."""
    mine = [j for j in jobs if plat in j["platforms"]]
    rows = []
    for j in mine:
        print(f"== nightly: {j['name']}", flush=True)
        rows.append(run_job(j))
    red = [r["name"] for r in rows if r["status"] == "failed"]
    problems = [f"{r['name']}: {r['reason']}" for r in rows if r["status"] == "failed"]
    ran = sum(1 for r in rows if r["status"] == "ran")
    if ran == 0 and not red:
        problems.append("nothing ran: every job was skipped or none is listed for " + plat)
    return {
        "schema": "nightly-receipt/1", "drill": False, "platform": plat,
        "commit": _commit(), "verdict": "red" if problems else "green",
        "ran": ran, "skipped": sum(1 for r in rows if r["status"] == "skipped"), "failed": len(red),
        "red_jobs": red, "problems": problems, "jobs": rows,
        "other_platform": [j["name"] for j in jobs if plat not in j["platforms"]],
    }


def _commit():
    try:
        out = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True)
        return out.stdout.strip() or None if out.returncode == 0 else None
    except OSError:
        return None


def drill_problems(receipt):
    """What is wrong with how the runner handled the planted jobs (empty = the
    runner can fail). Pure, so tools/nightly_check.py can feed it a blind runner."""
    bad = []
    rows = {r["name"]: r for r in receipt.get("jobs", [])}
    if receipt.get("verdict") != "red":
        bad.append("the runner read GREEN with a planted fault in the list")
    for name in (DRILL_FAIL, DRILL_FLOOR):
        r = rows.get(name)
        if r is None or r["status"] != "failed":
            bad.append(f"{name} was not read as failed (status {r['status'] if r else 'absent'})")
        if name not in receipt.get("red_jobs", []):
            bad.append(f"{name} is not named among the red jobs")
    if rows.get(DRILL_FLOOR, {}).get("status") == "failed" and "floor" not in (rows[DRILL_FLOOR]["reason"] or ""):
        bad.append(f"{DRILL_FLOOR} went red for a reason other than its floor")
    c = rows.get(DRILL_CONTROL)
    if c is None or c["status"] != "ran" or DRILL_CONTROL in receipt.get("red_jobs", []):
        bad.append(f"the clean control {DRILL_CONTROL} did not read as ran")
    return bad


def write_receipt(receipt, path):
    path = pathlib.Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--jobs", default=str(DEFAULT_JOBS))
    ap.add_argument("--receipt", default=str(DEFAULT_RECEIPT))
    ap.add_argument("--platform", choices=PLATFORMS)
    ap.add_argument("--drill", action="store_true")
    a = ap.parse_args(argv)
    try:
        jobs = load_jobs(a.jobs)  # validated even in a drill: a bad list is red either way
        plat = a.platform or this_platform()
    except JobListError as e:
        print(f"nightly: BAD JOB LIST -- {e}", file=sys.stderr)
        return 2
    if a.drill:
        receipt = run_all(DRILL_JOBS, plat)
        receipt["drill"] = True
        write_receipt(receipt, a.receipt)
        bad = drill_problems(receipt)
        for p in receipt["problems"]:
            print(f"nightly drill: runner went red on -- {p}")
        for b in bad:
            print(f"nightly drill: FAILED -- {b}", file=sys.stderr)
        print("nightly drill: " + ("PASSED (the runner can fail, and named each planted fault)" if not bad else "FAILED"))
        return 1 if bad else 0
    receipt = run_all(jobs, plat)
    write_receipt(receipt, a.receipt)
    print(f"nightly: {receipt['verdict'].upper()} on {plat} -- ran {receipt['ran']}, "
          f"skipped {receipt['skipped']}, failed {receipt['failed']}; receipt {a.receipt}")
    for p in receipt["problems"]:
        print(f"nightly: RED -- {p}", file=sys.stderr)
    for r in receipt["jobs"]:
        if r["status"] == "skipped":
            print(f"nightly: SKIPPED (not counted as ran) -- {r['name']}: {r['reason']}")
    return 1 if receipt["verdict"] == "red" else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
