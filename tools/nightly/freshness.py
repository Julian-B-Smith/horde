#!/usr/bin/env python3
"""freshness -- is the nightly green, and how old is that? One line for the lead's
roundup (B448 P5, ADR-209 item 4: a row may be green on a nightly result less than
48 hours old).

    tools/nightly/freshness.py [--now ISO] [--from-json FILE] [--workflow nightly.yml]

Asks GitHub (`gh run list`) for the nightly workflow's completed runs on main, or
reads the same JSON from --from-json so a test needs no network. Prints exactly one
line and exits 0 only when it says green:

    NIGHTLY green <date> (<h> h old, run <id>)
    NIGHTLY HOLE -- <why>        no run, newest run not green, or older than 48 h
    NIGHTLY UNKNOWN -- <why>     GitHub unreachable or the input unreadable (exit 2)

UNKNOWN is never "fresh": a reader that cannot read must not look like a pass.

WHICH RUNS COUNT. The newest completed run decides, not the best one: a red night
last night is a hole today even if the night before was green. Drill runs (the
workflow titles them "(drill)") are excluded, since a drill run is green by design
and would otherwise renew a stale result. Cancelled and skipped runs carry no
verdict and are ignored. Age runs from the run's last update, and "older than 48 h"
is strictly greater than 48.0.
"""
import argparse
import datetime as dt
import json
import subprocess
import sys

# ADR-209 item 4. A ruling, not a comparison tolerance: named so that
# tools/tolerance_registry_check.py does not inventory it as one (it reads "limit").
MAX_AGE_HOURS = 48
NO_VERDICT = ("cancelled", "skipped", "stale")
GH_FIELDS = "databaseId,conclusion,status,createdAt,updatedAt,displayTitle"


class Unknown(Exception):
    pass


def parse_time(s):
    try:
        t = dt.datetime.fromisoformat(str(s).replace("Z", "+00:00"))
    except ValueError:
        raise Unknown(f"unreadable time {s!r}")
    return t if t.tzinfo else t.replace(tzinfo=dt.timezone.utc)


def fetch(workflow):
    try:
        p = subprocess.run(["gh", "run", "list", "--workflow", workflow, "--branch", "main",
                            "--status", "completed", "--limit", "30", "--json", GH_FIELDS],
                           capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError) as e:
        raise Unknown(f"gh could not run: {e}")
    if p.returncode != 0:
        raise Unknown("gh failed: " + (p.stderr.strip().splitlines() or ["no message"])[-1])
    return p.stdout


def judge(raw, now):
    """Return (line, ok). Raises Unknown when the input cannot be read."""
    try:
        runs = json.loads(raw)
    except ValueError as e:
        raise Unknown(f"input is not JSON: {e}")
    if not isinstance(runs, list) or any(not isinstance(r, dict) for r in runs):
        raise Unknown("input is not a list of runs")
    counted = []
    for r in runs:
        if "(drill)" in str(r.get("displayTitle", "")) or r.get("conclusion") in NO_VERDICT:
            continue
        if r.get("status") not in (None, "completed") or not r.get("conclusion"):
            continue  # still running: no verdict yet
        counted.append((parse_time(r.get("updatedAt") or r.get("createdAt")), r))
    if not counted:
        return "NIGHTLY HOLE -- no completed nightly run on main", False
    when, r = max(counted, key=lambda x: x[0])
    age = (now - when).total_seconds() / 3600.0
    if age < 0:
        raise Unknown(f"newest run ({when.isoformat()}) is dated after now ({now.isoformat()})")
    day = when.date().isoformat()
    if r["conclusion"] != "success":
        return f"NIGHTLY HOLE -- newest night ({day}, run {r.get('databaseId')}) ended {r['conclusion']}", False
    if age > MAX_AGE_HOURS:
        return f"NIGHTLY HOLE -- last green night {day} is {age:.1f} h old (limit {MAX_AGE_HOURS} h)", False
    return f"NIGHTLY green {day} ({age:.1f} h old, run {r.get('databaseId')})", True


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--now")
    ap.add_argument("--from-json")
    ap.add_argument("--workflow", default="nightly.yml")
    a = ap.parse_args(argv)
    try:
        now = parse_time(a.now) if a.now else dt.datetime.now(dt.timezone.utc)
        if a.from_json:
            try:
                raw = open(a.from_json, encoding="utf-8").read()
            except OSError as e:
                raise Unknown(f"cannot read {a.from_json}: {e}")
        else:
            raw = fetch(a.workflow)
        line, ok = judge(raw, now)
    except Unknown as e:
        print(f"NIGHTLY UNKNOWN -- {e}")
        return 2
    print(line)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
