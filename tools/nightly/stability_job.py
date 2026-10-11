#!/usr/bin/env python3
"""stability_job -- the nightly's one real job: the legacy SAW core's stability
check (tools/stability_check.cpp, B147 layer 2) run long (B448 P5/P6, ADR-209).

    tools/nightly/stability_job.py [--seconds=N] [--build-dir DIR]

Configures, builds ONLY the stability_check target, runs it for N simulated seconds
per patch, and speaks the nightly job protocol (tools/nightly/run.py): it prints
`KIT-GATE cases=<n>` where n is the number of patch rows the check actually printed
(counted from its own output, never assumed), and exits non-zero on anything but a
clean GREEN. The check's own must-fail controls run inside it.

WHY 3600 AND NOT 14400. The proposal names `--seconds=14400`. The check keeps the
default patch's whole render in memory and its controls hold three more copies, so
peak memory is linear in N: measured 474 MB at 600 s on the dev Mac, which puts 14400
at about 11 GB (extrapolated, NOT measured) against a 16 GB hosted runner. jobs.json
passes 3600 (about 3 GB, about 6 minutes here) until P6 has measured the long run
or the check stops holding the render. Raising it is a one-number edit in jobs.json.
"""
import os
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
# A patch row: name, then six numeric columns (RMS0, RMSn, peak, f0, Ksm, nonfin).
ROW_RE = re.compile(r"^\S.*?\s+-?\d+\.\d+\s+-?\d+\.\d+\s+\d+\.\d+\s+-?\d+\.\d+\s+-?\d+\.\d+\s+\d+\s*$")


def sh(cmd):
    print("+ " + " ".join(cmd), flush=True)
    return subprocess.run(cmd, cwd=ROOT).returncode


def main(argv):
    seconds, build = 3600, "build-nightly"
    for a in argv:
        if a.startswith("--seconds="):
            seconds = int(a.split("=", 1)[1])
        elif a.startswith("--build-dir="):
            build = a.split("=", 1)[1]
        else:
            print(f"stability_job: unknown argument {a}", file=sys.stderr)
            return 2
    bdir = (ROOT / build).resolve()  # absolute: this sandbox resets cwd between calls
    jobs = str(os.cpu_count() or 2)
    if sh(["cmake", "-S", str(ROOT), "-B", str(bdir), "-G", "Unix Makefiles", "-DCMAKE_BUILD_TYPE=Release"]):
        return 1
    if sh(["cmake", "--build", str(bdir), "--target", "stability_check", "-j", jobs]):
        return 1
    print(f"+ stability_check --seconds={seconds}", flush=True)
    p = subprocess.Popen([str(bdir / "stability_check"), f"--seconds={seconds}"], cwd=ROOT,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    rows, last = 0, ""
    for line in p.stdout:
        sys.stdout.write(line)
        if line.strip():
            last = line.strip()
        if ROW_RE.match(line):
            rows += 1
    rc = p.wait()
    if rc != 0 or not last.startswith("stability_check: GREEN"):
        print(f"stability_job: stability_check exit {rc}, last line {last!r}", file=sys.stderr)
        return 1
    print(f"KIT-GATE cases={rows}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
