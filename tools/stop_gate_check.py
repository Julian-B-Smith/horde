#!/usr/bin/env python3
"""stop_gate_check — the closing gate's verdict table (B455 H2, ADR-206 item 3).

WIRED: ./verify fast

`.claude/hooks/stop-gate.sh` is what makes "a session cannot finish on unverified
edits" true. Until 2026-10-10 nothing exercised it, and it had two blind spots: it
trusted a marker only the Edit and Write tools set, and it passed when a worktree
had no verify record at all. This check runs the REAL hook file in scratch git
repositories against a table of situations, each with the verdict it must give.

The table is the contract. A row is (name, setup, expected exit): 0 allows the
stop, 2 blocks it. The LIMIT row records a known hole on purpose (a file deleted
after verify has no mtime to read), so that closing it later is a visible change.

Self-calibrating: the same table is run against two faulty hooks, one that never
blocks and the hook as it stood before this change. Each must get rows wrong, and
the pre-change hook must get exactly the rows this change exists for wrong.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HOOK = os.path.join(ROOT, ".claude", "hooks", "stop-gate.sh")

NEVER_BLOCKS = "#!/usr/bin/env bash\ncat >/dev/null\nexit 0\n"

# The hook as it stood before B455 H2, kept verbatim as the control: the marker
# and the red record, and nothing that looks at the tree.
BEFORE = r'''#!/usr/bin/env bash
set -uo pipefail
INPUT=$(cat)
ACTIVE=$(printf '%s' "$INPUT" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("stop_hook_active",False))' 2>/dev/null || echo "False")
[ "$ACTIVE" = "True" ] && exit 0
if [ -f .harness/dirty ]; then exit 2; fi
if [ -f .harness/last-verify.json ]; then
  EXIT=$(python3 -c 'import json; print(json.load(open(".harness/last-verify.json")).get("exit",0))' 2>/dev/null || echo 0)
  if [ "$EXIT" != "0" ]; then exit 2; fi
fi
exit 0
'''

T0 = 1_700_000_000          # a fixed clock: every mtime below is set, never read from now


def git(repo, *args):
    env = dict(os.environ, GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t", GIT_COMMITTER_NAME="t",
               GIT_COMMITTER_EMAIL="t@t", GIT_AUTHOR_DATE="2023-11-14T22:13:20Z",
               GIT_COMMITTER_DATE="2023-11-14T22:13:20Z")
    return subprocess.run(("git", "-C", repo) + args, capture_output=True, text=True, env=env)


def write(repo, rel, text, when):
    path = os.path.join(repo, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(text)
    os.utime(path, (when, when))


def record(repo, exit_code, when, commit=None):
    h = commit if commit is not None else git(repo, "rev-parse", "--short", "HEAD").stdout.strip()
    write(repo, ".harness/last-verify.json",
          json.dumps({"target": "fast", "exit": exit_code, "git": h}), when)


def new_repo(tmp):
    """A repo with one commit on main, an origin/main ref at it, and .harness ignored."""
    repo = tempfile.mkdtemp(dir=tmp)
    git(repo, "init", "-q", "-b", "main")
    write(repo, ".gitignore", ".harness/\nbuild/\n", T0)
    write(repo, "a.txt", "one\n", T0)
    write(repo, "b.txt", "one\n", T0)
    git(repo, "add", ".gitignore", "a.txt", "b.txt")
    git(repo, "commit", "-q", "-m", "base")
    git(repo, "update-ref", "refs/remotes/origin/main", "HEAD")
    return repo


# --- the situations --------------------------------------------------------
def s_clean_no_record(r):
    pass


def s_tracked_edit_no_record(r):
    write(r, "a.txt", "two\n", T0 + 50)


def s_new_file_no_record(r):
    write(r, "c.txt", "new\n", T0 + 50)


def s_commit_no_record(r):
    write(r, "a.txt", "two\n", T0 + 50)
    git(r, "commit", "-q", "-am", "work")


def s_verified_then_idle(r):
    write(r, "a.txt", "two\n", T0 + 50)
    record(r, 0, T0 + 100)


def s_verified_then_committed(r):
    write(r, "a.txt", "two\n", T0 + 50)
    record(r, 0, T0 + 100)
    git(r, "commit", "-q", "-am", "work")
    os.utime(os.path.join(r, "a.txt"), (T0 + 50, T0 + 50))


def s_shell_edit_after_verify(r):
    record(r, 0, T0 + 100)
    write(r, "a.txt", "two\n", T0 + 200)


def s_new_file_after_verify(r):
    record(r, 0, T0 + 100)
    write(r, "c.txt", "new\n", T0 + 200)


def s_commit_of_unverified_after_verify(r):
    record(r, 0, T0 + 100)
    write(r, "a.txt", "two\n", T0 + 200)
    git(r, "commit", "-q", "-am", "work")
    os.utime(os.path.join(r, "a.txt"), (T0 + 200, T0 + 200))


def s_red_record(r):
    record(r, 1, T0 + 100)


def s_dirty_marker(r):
    record(r, 0, T0 + 100)
    write(r, ".harness/dirty", "x\n", T0 + 150)


def s_ignored_file_after_verify(r):
    record(r, 0, T0 + 100)
    write(r, "build/out.o", "x\n", T0 + 200)


def s_unknown_commit_clean(r):
    record(r, 0, T0 + 100, commit="0123abc")


def s_unknown_commit_edit(r):
    record(r, 0, T0 + 100, commit="0123abc")
    write(r, "a.txt", "two\n", T0 + 200)


def s_deleted_after_verify(r):
    record(r, 0, T0 + 100)
    os.remove(os.path.join(r, "b.txt"))


TABLE = [
    # name, setup, stdin, expected exit
    ("clean tree, no record: allow", s_clean_no_record, {}, 0),
    ("NO-RECORD tracked edit, no record: block", s_tracked_edit_no_record, {}, 2),
    ("NO-RECORD new file, no record: block", s_new_file_no_record, {}, 2),
    ("NO-RECORD commit made, no record: block", s_commit_no_record, {}, 2),
    ("edit, then verify: allow", s_verified_then_idle, {}, 0),
    ("edit, verify, then commit the same bytes: allow", s_verified_then_committed, {}, 0),
    ("SHELL edit after verify, no marker: block", s_shell_edit_after_verify, {}, 2),
    ("SHELL new file after verify, no marker: block", s_new_file_after_verify, {}, 2),
    ("SHELL unverified edit committed after verify: block", s_commit_of_unverified_after_verify, {}, 2),
    ("red record: block", s_red_record, {}, 2),
    ("dirty marker: block", s_dirty_marker, {}, 2),
    ("dirty marker, already blocked once this cycle: allow", s_dirty_marker, {"stop_hook_active": True}, 0),
    ("ignored file written after verify: allow", s_ignored_file_after_verify, {}, 0),
    ("record names an unknown commit, clean tree: allow", s_unknown_commit_clean, {}, 0),
    ("record names an unknown commit, edit after: block", s_unknown_commit_edit, {}, 2),
    ("LIMIT file deleted after verify: allow (known hole; CI sees it)", s_deleted_after_verify, {}, 0),
]
# The rows the pre-change hook must get wrong: the two blind spots, and nothing else.
NEW_ROWS = {name for name, _, _, _ in TABLE if name.startswith(("NO-RECORD", "SHELL"))
            or name == "record names an unknown commit, edit after: block"}


def run_table(hook_text, tmp):
    """Returns the names of the rows the given hook gets wrong."""
    hook = os.path.join(tmp, "hook-%d.sh" % len(os.listdir(tmp)))
    with open(hook, "w") as f:
        f.write(hook_text)
    os.chmod(hook, 0o755)
    wrong = []
    for name, setup, stdin, expected in TABLE:
        repo = new_repo(tmp)
        setup(repo)
        r = subprocess.run(["bash", hook], cwd=repo, input=json.dumps(stdin),
                           capture_output=True, text=True)
        if r.returncode != expected:
            wrong.append("%s (got exit %d, want %d)" % (name, r.returncode, expected))
    return wrong


def main():
    if shutil.which("git") is None:
        print("stop_gate_check: FAILED -- git is not available")
        return 1
    with open(HOOK) as f:
        real = f.read()
    tmp = tempfile.mkdtemp(prefix="stopgate-")
    try:
        failures = []
        wrong = run_table(real, tmp)
        failures += ["the hook: " + w for w in wrong]

        never = run_table(NEVER_BLOCKS, tmp)
        blocks = sum(1 for _, _, _, e in TABLE if e == 2)
        if len(never) != blocks:
            failures.append("control: a hook that never blocks got %d rows wrong, want %d"
                            % (len(never), blocks))

        before = {w.split(" (got")[0] for w in run_table(BEFORE, tmp)}
        if before != NEW_ROWS:
            failures.append("control: the pre-change hook got %s wrong, want exactly %s"
                            % (sorted(before), sorted(NEW_ROWS)))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    if failures:
        print("stop_gate_check: FAILED (%d)" % len(failures))
        for f in failures:
            print("  " + f)
        return 1
    print("stop_gate_check: GREEN (%d rows; a never-blocking hook fails all %d block rows; "
          "the pre-change hook fails exactly the %d tree-state rows)"
          % (len(TABLE), blocks, len(NEW_ROWS)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
