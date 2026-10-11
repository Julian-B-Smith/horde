#!/usr/bin/env python3
"""legacy_gate -- run an existing gate under the nightly job protocol.

    tools/nightly/legacy_gate.py <command> [args...]

Some gates (tools/rtsan_check.py, tools/tsan_stress_check.py) report "could not
measure here" by printing the word SKIPPED and exiting 0 -- ADR-199's rule, written
before the nightly's protocol existed. Run bare, the runner would count that exit 0
as ran, and a night with no compiler would read as a night of sanitizer checks.
This wrapper passes the gate's output and exit code through unchanged and, when the
gate exited 0 AND printed SKIPPED, adds the protocol line `KIT-GATE skipped: <that
line>`. It never turns a failure into a skip: a non-zero exit stays non-zero.
"""
import subprocess
import sys


def main(argv):
    if not argv:
        print("usage: legacy_gate.py <command> [args...]", file=sys.stderr)
        return 2
    p = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    skipped = None
    for line in p.stdout:
        sys.stdout.write(line)
        if "SKIPPED" in line and skipped is None:
            skipped = line.strip()
    rc = p.wait()
    if rc == 0 and skipped is not None:
        print(f"KIT-GATE skipped: {skipped}")
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
