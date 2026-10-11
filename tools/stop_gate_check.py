#!/usr/bin/env python3
"""stop_gate_check — horde's contract test for the closing gate (B455 H2, B457, ADR-210).

WIRED: ./verify fast

The closing gate is what makes "a session cannot finish on unverified edits" true.
Since kit 2.9.0 the gate itself is kit-owned (`.kit/stop-gate.sh`, checksummed), and
`.claude/hooks/stop-gate.sh` is a short shim that runs it. The kit tests its gate in
its own repo. This check tests what is horde's: that OUR shim, with OUR vendored
files, blocks in OUR repo. It runs the real shim, the real vendored gate and the real
`record` function in scratch git repositories against a table of situations, each
with the verdict it must give. 0 allows the stop, 2 blocks it.

Every record below is written by the kit's own `record`, never by hand, so the
fingerprint in it is the one a real `./verify` would write.

Self-calibrating, with two faulty hooks run against the same table:
  - a hook that never blocks must get every block row wrong;
  - our shim with its `KIT_STOP_GATE_MODE=deny` line removed must get exactly the
    tree-state rows wrong. That line is the one thing horde adds to the kit's
    default (ADR-210 item 2), so this proves the line is what makes those rows block.

History: until kit 2.9.0 this file tested horde's own gate, which compared file
times. Its LIMIT row (a file deleted after verify was not seen) is a block row now.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHIM = os.path.join(ROOT, ".claude", "hooks", "stop-gate.sh")
KIT_FILES = [".kit/stop-gate.sh", ".kit/kit-gates.sh"]
DENY_LINE = "export KIT_STOP_GATE_MODE=deny\n"

NEVER_BLOCKS = "#!/usr/bin/env bash\ncat >/dev/null\nexit 0\n"

# Every edit below changes the file's SIZE, on purpose. Measured 2026-10-11 on kit
# 2.9.0: an edit that keeps a tracked file's size, made in the same second as the time
# git last recorded for that file, is not seen by the fingerprint when the gate runs in
# a later second (60 of 60 allowed). The fingerprint works on a copy of the index, and
# the copy's fresh time makes git trust the recorded size and time. With same-size
# edits this table passed or failed by the clock. Fixed in kit 2.9.1, which also tests it
# with the timing arranged (thread stop-gate-tree-state). The edits here still change size,
# so this table never depends on the clock.
EDIT = "a second version, longer\n"


def git(repo, *args):
    env = dict(os.environ, GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t", GIT_COMMITTER_NAME="t",
               GIT_COMMITTER_EMAIL="t@t", GIT_AUTHOR_DATE="2023-11-14T22:13:20Z",
               GIT_COMMITTER_DATE="2023-11-14T22:13:20Z")
    return subprocess.run(("git", "-C", repo) + args, capture_output=True, text=True, env=env)


def write(repo, rel, text):
    path = os.path.join(repo, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(text)


def record(repo, exit_code):
    """A verify record written by the vendored `record`, as ./verify writes it."""
    r = subprocess.run(
        ["bash", "-c", 'HARNESS_DIR=.harness; mkdir -p .harness; . .kit/kit-gates.sh; record fast "$1"',
         "record", str(exit_code)], cwd=repo, capture_output=True, text=True)
    if r.returncode != exit_code or not os.path.exists(os.path.join(repo, ".harness/last-verify.json")):
        raise RuntimeError("the vendored record() did not write a record: %s" % r.stderr.strip())


def new_repo(tmp):
    """One commit on main holding the vendored kit files, origin/main at it, .harness ignored."""
    repo = tempfile.mkdtemp(dir=tmp)
    git(repo, "init", "-q", "-b", "main")
    write(repo, ".gitignore", ".harness/\nbuild/\n")
    write(repo, "a.txt", "one\n")
    write(repo, "b.txt", "one\n")
    write(repo, "sub/c.txt", "one\n")
    for rel in KIT_FILES:
        os.makedirs(os.path.dirname(os.path.join(repo, rel)), exist_ok=True)
        shutil.copy(os.path.join(ROOT, rel), os.path.join(repo, rel))
    git(repo, "add", ".gitignore", "a.txt", "b.txt", "sub/c.txt", *KIT_FILES)
    git(repo, "commit", "-q", "-m", "base")
    git(repo, "update-ref", "refs/remotes/origin/main", "HEAD")
    return repo


# --- the situations --------------------------------------------------------
def s_clean_no_record(r):
    pass


def s_tracked_edit_no_record(r):
    write(r, "a.txt", EDIT)


def s_new_file_no_record(r):
    write(r, "c.txt", "new\n")


def s_commit_no_record(r):
    write(r, "a.txt", EDIT)
    git(r, "commit", "-q", "-am", "work")


def s_verified_then_idle(r):
    write(r, "a.txt", EDIT)
    record(r, 0)


def s_verified_then_committed(r):
    write(r, "a.txt", EDIT)
    record(r, 0)
    git(r, "commit", "-q", "-am", "work")


def s_shell_edit_after_verify(r):
    record(r, 0)
    write(r, "a.txt", EDIT)


def s_new_file_after_verify(r):
    record(r, 0)
    write(r, "c.txt", "new\n")


def s_commit_of_unverified_after_verify(r):
    record(r, 0)
    write(r, "a.txt", EDIT)
    git(r, "commit", "-q", "-am", "work")


def s_deleted_after_verify(r):
    write(r, "a.txt", EDIT)
    record(r, 0)
    os.remove(os.path.join(r, "b.txt"))


def s_edit_reverted_after_verify(r):
    write(r, "a.txt", EDIT)
    record(r, 0)
    write(r, "a.txt", "a third, longer still\n")
    write(r, "a.txt", EDIT)


def s_stale_record_untouched_tree(r):
    # A record from other work, then back to a tree with no local work (a pull, a
    # switch to main). There is nothing here to verify, whatever the record says.
    write(r, "a.txt", EDIT)
    record(r, 0)
    git(r, "checkout", "-q", "--", "a.txt")


def s_ignored_file_after_verify(r):
    write(r, "a.txt", EDIT)
    record(r, 0)
    write(r, "build/out.o", "x\n")


def s_red_record(r):
    record(r, 1)


def s_red_record_from_subdir(r):
    record(r, 1)
    return "sub"


def s_unreadable_record(r):
    record(r, 1)
    write(r, ".harness/last-verify.json", "{ this is not json")


def s_dirty_marker(r):
    record(r, 0)
    write(r, ".harness/dirty", "x\n")


def s_gate_missing(r):
    os.remove(os.path.join(r, ".kit/stop-gate.sh"))


ONCE = {"stop_hook_active": True}
TABLE = [
    # name, setup, stdin, expected exit
    ("clean tree, no record: allow", s_clean_no_record, {}, 0),
    ("TREE tracked edit, no record: block", s_tracked_edit_no_record, {}, 2),
    ("TREE new file, no record: block", s_new_file_no_record, {}, 2),
    ("TREE commit made, no record: block", s_commit_no_record, {}, 2),
    ("edit, then verify: allow", s_verified_then_idle, {}, 0),
    ("edit, verify, then commit the same bytes: allow", s_verified_then_committed, {}, 0),
    ("TREE shell edit after verify, no marker: block", s_shell_edit_after_verify, {}, 2),
    ("TREE new file after verify, no marker: block", s_new_file_after_verify, {}, 2),
    ("TREE unverified edit committed after verify: block", s_commit_of_unverified_after_verify, {}, 2),
    ("TREE file deleted after verify: block", s_deleted_after_verify, {}, 2),
    ("edit after verify, put back to the verified bytes: allow", s_edit_reverted_after_verify, {}, 0),
    ("old record, tree with no local work: allow", s_stale_record_untouched_tree, {}, 0),
    ("ignored file written after verify: allow", s_ignored_file_after_verify, {}, 0),
    ("red record: block", s_red_record, {}, 2),
    ("red record, hook started in a subdirectory: block", s_red_record_from_subdir, {}, 2),
    ("record that cannot be read: block", s_unreadable_record, {}, 2),
    ("dirty marker: block", s_dirty_marker, {}, 2),
    ("dirty marker, already blocked once this cycle: allow", s_dirty_marker, ONCE, 0),
    ("vendored gate missing: block (the shim must not fail open)", s_gate_missing, {}, 2),
    ("vendored gate missing, already blocked once this cycle: allow", s_gate_missing, ONCE, 0),
]
# The rows only the tree test blocks. Without horde's deny line they are logged, not blocked.
TREE_ROWS = {name for name, _, _, _ in TABLE if name.startswith("TREE ")}


def run_table(hook_text, tmp):
    """Returns the names of the rows the given hook gets wrong."""
    hook = os.path.join(tmp, "hook-%d.sh" % len(os.listdir(tmp)))
    with open(hook, "w") as f:
        f.write(hook_text)
    os.chmod(hook, 0o755)
    wrong = []
    for name, setup, stdin, expected in TABLE:
        repo = new_repo(tmp)
        sub = setup(repo)
        # KIT_FLEET_DIR points at nothing, so an observing gate writes no event to the
        # machine's real fleet log from inside a test.
        env = dict(os.environ, CLAUDE_PROJECT_DIR=repo, KIT_FLEET_DIR=os.path.join(tmp, "no-fleet"))
        env.pop("KIT_STOP_GATE_MODE", None)
        env.pop("HARNESS_DIR", None)
        r = subprocess.run(["bash", hook], cwd=os.path.join(repo, sub or ""), input=json.dumps(stdin),
                           capture_output=True, text=True, env=env)
        if r.returncode != expected:
            wrong.append("%s (got exit %d, want %d)" % (name, r.returncode, expected))
    return wrong


def main():
    if shutil.which("git") is None:
        print("stop_gate_check: FAILED -- git is not available")
        return 1
    for rel in KIT_FILES:
        if not os.path.exists(os.path.join(ROOT, rel)):
            print("stop_gate_check: FAILED -- %s is missing; run kit_sync.py" % rel)
            return 1
    with open(SHIM) as f:
        real = f.read()
    tmp = tempfile.mkdtemp(prefix="stopgate-")
    try:
        failures = []
        failures += ["the shim: " + w for w in run_table(real, tmp)]

        never = run_table(NEVER_BLOCKS, tmp)
        blocks = sum(1 for _, _, _, e in TABLE if e == 2)
        if len(never) != blocks:
            failures.append("control: a hook that never blocks got %d rows wrong, want %d"
                            % (len(never), blocks))

        if real.count(DENY_LINE) != 1:
            failures.append("the shim does not carry exactly one `%s` line" % DENY_LINE.strip())
        else:
            observing = {w.split(" (got")[0] for w in run_table(real.replace(DENY_LINE, ""), tmp)}
            if observing != TREE_ROWS:
                failures.append("control: the shim without its deny line got %s wrong, want exactly %s"
                                % (sorted(observing), sorted(TREE_ROWS)))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    if failures:
        print("stop_gate_check: FAILED (%d)" % len(failures))
        for f in failures:
            print("  " + f)
        return 1
    print("stop_gate_check: GREEN (%d rows; a never-blocking hook fails all %d block rows; "
          "the shim without its deny line fails exactly the %d tree-state rows)"
          % (len(TABLE), blocks, len(TREE_ROWS)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
