#!/usr/bin/env python3
"""nightly_check -- the nightly frame can go green, can go red, and says which (B448 P5).

WIRED: ./verify fast

Fast and offline: seconds, no network, no build. It drives tools/nightly/run.py and
tools/nightly/freshness.py as the CLIs they are, over scratch job lists and scratch
JSON in a temp dir. Every row is a must-fail control or its green twin; a row that
fails prints WHICH one.

ROWS
  clean list runs green (a floor job at its floor, a skip beside it)
  the real tools/nightly/jobs.json is well-formed
  drill mode: exits 0 only because the runner went red on BOTH planted faults and
    named them; and a blind runner (green under a drill) makes the drill itself red
  a job below its floor is red; at its floor is green
  a job that prints no count but has a floor is red
  a skipped job is recorded as skipped, not as ran; an all-skipped night is red
  a failing job is red; a job-list entry missing a field, a bad platform, a duplicate
    name and unreadable JSON are each refused (exit 2)
  jobs for the other platform are not run
  the default receipt path is gitignored (nothing the runner writes is committed)
  freshness: 47 h fresh, 49 h HOLE, no run HOLE, newest night red HOLE, a drill run
    does not renew a stale result, unreadable input UNKNOWN, gh missing UNKNOWN
  the workflow draft is thin: only `./verify nightly`, pinned actions, no tokens widened
"""
import importlib.util
import json
import pathlib
import shlex
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
NIGHTLY = ROOT / "tools" / "nightly"
PY = sys.executable
failures = []


def row(ok, what, detail=""):
    print(f"{'PASS' if ok else 'FAIL'}  {what}" + (f"  ({detail})" if detail else ""))
    if not ok:
        failures.append(what)


def job(name, command, floor=0, platforms=("linux", "macos")):
    return {"name": name, "command": command, "platforms": list(platforms), "floor": floor}


def say(text):
    return f"{shlex.quote(PY)} -c 'import sys; print(sys.argv[1])' {shlex.quote(text)}"


def run_runner(tmp, jobs, *extra, raw=None):
    """Run run.py over a scratch job list; return (exit code, receipt or None, stderr)."""
    jl, rc = tmp / "jobs.json", tmp / "receipt.json"
    if rc.exists():
        rc.unlink()
    jl.write_text(raw if raw is not None else json.dumps({"jobs": jobs}), encoding="utf-8")
    p = subprocess.run([PY, str(NIGHTLY / "run.py"), "--jobs", str(jl), "--receipt", str(rc),
                        "--platform", "linux", *extra], capture_output=True, text=True)
    return p.returncode, (json.loads(rc.read_text()) if rc.exists() else None), p.stderr + p.stdout


def by_name(receipt):
    return {j["name"]: j for j in receipt["jobs"]}


def runner_rows(tmp):
    ok_job = job("clean", say("KIT-GATE cases=5"), floor=5)
    skip_job = job("skippy", say("KIT-GATE skipped: no compiler here"))

    rc, r, _ = run_runner(tmp, [ok_job, skip_job])
    row(rc == 0 and r and r["verdict"] == "green" and by_name(r)["clean"]["status"] == "ran"
        and by_name(r)["clean"]["cases"] == 5, "clean job list runs green", f"exit {rc}")

    rc, r, _ = run_runner(tmp, [skip_job, ok_job])
    s = by_name(r)["skippy"] if r else {}
    row(rc == 0 and s.get("status") == "skipped" and "no compiler" in (s.get("reason") or "")
        and r["ran"] == 1 and r["skipped"] == 1 and s.get("cases") is None,
        "a skipped job is recorded as skipped, not as ran", f"status {s.get('status')}, ran {r and r['ran']}")

    rc, r, _ = run_runner(tmp, [skip_job])
    row(rc == 1 and r and r["verdict"] == "red" and r["ran"] == 0,
        "a night where every job skipped is red (nothing was judged)", f"exit {rc}")

    rc, r, _ = run_runner(tmp, [job("short", say("KIT-GATE cases=4"), floor=5)])
    f = by_name(r)["short"] if r else {}
    row(rc == 1 and f.get("status") == "failed" and "below floor" in (f.get("reason") or "")
        and r["red_jobs"] == ["short"], "a job below its floor is red and named", f"exit {rc}, {f.get('reason')}")

    rc, r, _ = run_runner(tmp, [job("silent", say("all good, no count"), floor=5)])
    f = by_name(r)["silent"] if r else {}
    row(rc == 1 and f.get("status") == "failed" and "no KIT-GATE cases=" in (f.get("reason") or ""),
        "a job that prints no count but has a floor is red", f"exit {rc}, {f.get('reason')}")

    rc, r, _ = run_runner(tmp, [job("nofloor", say("no count needed"), floor=0), ok_job])
    row(rc == 0 and by_name(r)["nofloor"]["status"] == "ran", "a job with no floor needs no count (green twin)")

    rc, r, _ = run_runner(tmp, [job("boom", f"{shlex.quote(PY)} -c 'raise SystemExit(7)'"), ok_job])
    row(rc == 1 and by_name(r)["boom"]["status"] == "failed" and r["red_jobs"] == ["boom"],
        "a failing job is red and the others still run", f"exit {rc}")

    rc, r, _ = run_runner(tmp, [job("mac-only", "exit 9", platforms=("macos",)), ok_job])
    row(rc == 0 and "mac-only" not in by_name(r) and r["other_platform"] == ["mac-only"],
        "a job for the other platform is not run")

    gate = f"{shlex.quote(PY)} {shlex.quote(str(NIGHTLY / 'legacy_gate.py'))} {shlex.quote(PY)} -c"
    rc, r, _ = run_runner(tmp, [job("old-skip", f"{gate} 'print(\"gate: SKIPPED - no compiler\")'"), ok_job])
    s = by_name(r)["old-skip"] if r else {}
    row(rc == 0 and s.get("status") == "skipped" and r["ran"] == 1,
        "legacy_gate: an old gate that prints SKIPPED is recorded as skipped", s.get("status"))
    rc, r, _ = run_runner(tmp, [job("old-ran", f"{gate} 'print(\"gate: GREEN\")'"), ok_job])
    row(rc == 0 and by_name(r)["old-ran"]["status"] == "ran", "legacy_gate: an old gate that ran stays ran (green twin)")
    rc, r, _ = run_runner(tmp, [job("old-fail", f"{gate} 'print(\"SKIPPED but\"); raise SystemExit(1)'"), ok_job])
    row(rc == 1 and by_name(r)["old-fail"]["status"] == "failed",
        "legacy_gate: a non-zero exit is never turned into a skip")

    bad = {
        "missing field": json.dumps({"jobs": [{"name": "x", "command": "true", "platforms": ["linux"]}]}),
        "bad platform": json.dumps({"jobs": [job("x", "true", platforms=("windows",))]}),
        "duplicate name": json.dumps({"jobs": [job("x", "true"), job("x", "true")]}),
        "not JSON": "{ nope",
    }
    for what, raw in bad.items():
        rc, r, err = run_runner(tmp, None, raw=raw)
        row(rc == 2 and r is None, f"a bad job list is refused: {what}", f"exit {rc}")
    rc, r, err = run_runner(tmp, None, raw=bad["missing field"])
    row("missing field 'floor'" in err, "the refusal names the missing field", err.strip().splitlines()[-1][:80] if err else "")


def drill_rows(tmp):
    rc, r, err = run_runner(tmp, [job("clean", say("KIT-GATE cases=5"), floor=5)], "--drill")
    row(rc == 0 and r and r["drill"] is True and r["verdict"] == "red"
        and sorted(r["red_jobs"]) == ["drill-planted-below-floor", "drill-planted-fail"],
        "drill mode is detected red on both planted faults, and passes", f"exit {rc}")
    row(r and "clean" not in by_name(r), "a drill runs the planted jobs only, not the real list")

    spec = importlib.util.spec_from_file_location("nightly_run", NIGHTLY / "run.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    good = {"verdict": "red", "red_jobs": [mod.DRILL_FAIL, mod.DRILL_FLOOR], "jobs": [
        {"name": mod.DRILL_FAIL, "status": "failed", "reason": "exit 3"},
        {"name": mod.DRILL_FLOOR, "status": "failed", "reason": "below floor: judged 1, floor 5"},
        {"name": mod.DRILL_CONTROL, "status": "ran", "reason": None}]}
    row(mod.drill_problems(good) == [], "the drill accepts a runner that went red on each fault")
    blind = json.loads(json.dumps(good))
    blind.update(verdict="green", red_jobs=[])
    for j in blind["jobs"]:
        j.update(status="ran", reason=None)
    row(len(mod.drill_problems(blind)) >= 3, "a blind runner (green under a drill) fails the drill",
        f"{len(mod.drill_problems(blind))} problems")
    unnamed = json.loads(json.dumps(good))
    unnamed["red_jobs"] = [mod.DRILL_FAIL]
    row(any(mod.DRILL_FLOOR in p for p in mod.drill_problems(unnamed)),
        "a fault the runner went red on but did not name fails the drill")
    over = json.loads(json.dumps(good))
    over["jobs"][2].update(status="failed")
    over["red_jobs"].append(mod.DRILL_CONTROL)
    row(any("control" in p for p in mod.drill_problems(over)), "a runner that fails the clean control fails the drill")

    jl = mod.load_jobs(NIGHTLY / "jobs.json")
    row(len(jl) >= 1 and all(j["floor"] >= 0 for j in jl), "the real tools/nightly/jobs.json is well-formed", f"{len(jl)} jobs")
    gi = (ROOT / ".gitignore").read_text(encoding="utf-8").splitlines()
    row(".harness/" in gi and str(mod.DEFAULT_RECEIPT.relative_to(ROOT)).startswith(".harness/"),
        "the default receipt path is gitignored (.harness/)")


def hours_ago(now_iso, h):
    import datetime as dt
    n = dt.datetime.fromisoformat(now_iso.replace("Z", "+00:00"))
    return (n - dt.timedelta(hours=h)).isoformat().replace("+00:00", "Z")


def fresh(tmp, runs=None, raw=None, env=None):
    NOW = "2026-10-11T12:00:00Z"
    f = tmp / "runs.json"
    f.write_text(raw if raw is not None else json.dumps(runs), encoding="utf-8")
    p = subprocess.run([PY, str(NIGHTLY / "freshness.py"), "--now", NOW, "--from-json", str(f)],
                       capture_output=True, text=True, env=env)
    return p.returncode, p.stdout.strip()


def run_json(h, conclusion="success", title="nightly", i=1):
    t = hours_ago("2026-10-11T12:00:00Z", h)
    return {"databaseId": i, "conclusion": conclusion, "status": "completed",
            "createdAt": t, "updatedAt": t, "displayTitle": title}


def freshness_rows(tmp):
    rc, out = fresh(tmp, [run_json(47)])
    row(rc == 0 and out.startswith("NIGHTLY green 2026-10-09"), "freshness: 47 h old is fresh", out)
    rc, out = fresh(tmp, [run_json(49)])
    row(rc == 1 and out.startswith("NIGHTLY HOLE") and "49.0 h" in out, "freshness: 49 h old is HOLE", out)
    rc, out = fresh(tmp, [])
    row(rc == 1 and out.startswith("NIGHTLY HOLE"), "freshness: no run is HOLE", out)
    rc, out = fresh(tmp, [run_json(5, "failure", i=2), run_json(29, "success", i=1)])
    row(rc == 1 and out.startswith("NIGHTLY HOLE") and "failure" in out, "freshness: a red newest night is HOLE", out)
    rc, out = fresh(tmp, [run_json(1, "success", "nightly (drill)", 3), run_json(60, "success", i=1)])
    row(rc == 1 and out.startswith("NIGHTLY HOLE"), "freshness: a drill run does not renew a stale result", out)
    rc, out = fresh(tmp, [run_json(2, "cancelled", i=4), run_json(30, "success", i=1)])
    row(rc == 0 and out.startswith("NIGHTLY green"), "freshness: a cancelled run is ignored, the green before it counts", out)
    for what, raw in (("not JSON", "<html>"), ("an object", "{}"), ("a bad time", json.dumps([dict(run_json(1), updatedAt="soon")]))):
        rc, out = fresh(tmp, raw=raw)
        row(rc == 2 and out.startswith("NIGHTLY UNKNOWN"), f"freshness: unreadable input is UNKNOWN ({what})", out)
    p = subprocess.run([PY, str(NIGHTLY / "freshness.py"), "--from-json", str(tmp / "absent.json")], capture_output=True, text=True)
    row(p.returncode == 2 and "UNKNOWN" in p.stdout, "freshness: a missing input file is UNKNOWN")
    empty = tmp / "nobin"
    empty.mkdir(exist_ok=True)
    p = subprocess.run([PY, str(NIGHTLY / "freshness.py")], capture_output=True, text=True,
                       env={"PATH": str(empty), "HOME": str(tmp)})
    row(p.returncode == 2 and p.stdout.startswith("NIGHTLY UNKNOWN"),
        "freshness: GitHub unreachable (no gh) is UNKNOWN, never fresh", p.stdout.strip()[:70])


def workflow_rows():
    wf = NIGHTLY / "nightly.workflow.yml"
    text = wf.read_text(encoding="utf-8") if wf.exists() else ""
    runs = [l.strip() for l in text.splitlines() if l.strip().startswith("run:") or "./verify" in l]
    row(bool(text) and "schedule:" in text and "workflow_dispatch:" in text and "drill" in text,
        "workflow draft: a schedule and a manual trigger with a drill input")
    row(any("./verify nightly" in l for l in runs) and not any(
        "./verify" in l and "nightly" not in l for l in runs), "workflow draft only calls ./verify nightly")
    p = subprocess.run([PY, str(ROOT / "tools" / "workflow_pin_check.py"), "--paths", str(wf)], capture_output=True, text=True)
    row(p.returncode == 0, "workflow draft passes the house pin rules (workflow_pin_check)", p.stdout.strip().splitlines()[-1][:70] if p.stdout.strip() else "")


def main():
    with tempfile.TemporaryDirectory() as d:
        tmp = pathlib.Path(d)
        runner_rows(tmp)
        drill_rows(tmp)
        freshness_rows(tmp)
    workflow_rows()
    print(f"nightly_check: {'RED' if failures else 'GREEN'} ({len(failures)} failing)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
