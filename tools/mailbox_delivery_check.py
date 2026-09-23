#!/usr/bin/env python3
"""mailbox_delivery_check — did our outbound filings actually REACH the reader?

WIRED: ./verify fast

WHY THIS EXISTS. On 2026-08-15 we hit three versions of one failure in a single
day: a filing left uncommitted, a filing committed to a side branch, and a filing
committed inside a sibling's checkout and never pushed. The last one stranded an
ack FOUNDATIONS was actively waiting on for seven hours. Every version was
SILENT FROM BOTH ENDS -- we saw a commit, they saw nothing, and neither side had
a signal. Their own outbox sweep reported "awaiting HYPERSAW" correctly the whole
time: the sweep was right and the world was wrong, because a file absent from
their `origin` is indistinguishable from a file never written.

Care does not fix a failure with no feedback. This is the feedback.

WHAT IT CHECKS. For every sibling mailbox `../<SIBLING>/integrations/hypersaw/`,
every file whose front matter says `from: HYPERSAW` must exist at that sibling's
`origin/main`. Anything else is a draft that looks like a filing to its author.
That is the amended rule (their R9, our proposal, adopted verbatim):

    A filing is FILED when it is PUSHED to the correspondent's origin/main.

NO MACHINE PATHS. Siblings are discovered repo-relative (`../*/integrations/
hypersaw`), never from an absolute path or an env var holding one -- committing
either would bake this machine's layout into a public repo. Where no sibling
checkout exists, there is nothing to check and the gate says so rather than
implying it verified something.

WHAT IT DELIBERATELY DOES NOT DO: it never writes to a sibling. Writes stay home;
this only reads. Fixing a stranded filing is a human-visible act, so it reports
and exits non-zero rather than quietly pushing on someone's behalf.
"""
import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
FROM_US = re.compile(r"^from:\s*HYPERSAW\s*$", re.MULTILINE)


def git(repo, *args):
    """Run git in `repo`; (rc, stdout). Never raises -- a sibling that is not a
    git repo, or has no origin, is a skip condition and not a failure."""
    try:
        p = subprocess.run(["git", "-C", str(repo), *args], capture_output=True, text=True)
        return p.returncode, p.stdout.strip()
    except OSError:
        return 1, ""


def main():
    mailboxes = sorted(REPO.parent.glob("*/integrations/hypersaw"))
    if not mailboxes:
        print("mailbox_delivery: no sibling mailbox checked out — SKIPPED")
        return 0

    undelivered, checked, skipped = [], 0, []
    for box in mailboxes:
        sibling = box.parents[1]
        # Compare against the REMOTE, not a local branch: the whole failure class
        # is local state the reader cannot see. Fetch is deliberately NOT run --
        # this gate must never touch the network, so it reports staleness risk
        # instead of hiding it behind a slow, failable fetch.
        rc, _ = git(sibling, "rev-parse", "--verify", "--quiet", "origin/main")
        if rc != 0:
            skipped.append(f"{sibling.name} (no origin/main)")
            continue
        for f in sorted(box.glob("*.md")):
            try:
                if not FROM_US.search(f.read_text(errors="replace")):
                    continue   # theirs, not ours to deliver
            except OSError:
                continue
            rel = f.relative_to(sibling).as_posix()
            checked += 1
            rc, _ = git(sibling, "cat-file", "-e", f"origin/main:{rel}")
            if rc != 0:
                undelivered.append(f"{sibling.name}: {f.name}")

    for s in skipped:
        print(f"mailbox_delivery: skipped {s}")
    if undelivered:
        print("mailbox_delivery: FAILED — filings that exist here and NOT on the reader's origin/main:",
              file=sys.stderr)
        for u in undelivered:
            print(f"  {u}", file=sys.stderr)
        print("  These are drafts, not filings. Push them to the sibling's main, then re-run.",
              file=sys.stderr)
        return 1
    print(f"mailbox_delivery: GREEN ({checked} outbound filing(s) present on the reader's origin/main)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
