#!/usr/bin/env python3
"""labs_preview_check -- labs_preview.sh runs nothing that a lab branch supplied (B446 W3b, ADR-194 W2-06).

WIRED: ./verify fast

WHY. labs_preview.sh merges every open lab branch into a scratch worktree. It used
to run that worktree's tools/gen_lab_index.py and to point the human at its
tools/serve_labs.py, so a branch could pick code to run in the human's shell. Now
only the main checkout's copies run and the merged tree is data.

HOW (about a second, no network, no gh). A fixture origin plus a "main checkout"
clone holding the real tools. A branch `lab-x` adds a lab AND replaces
tools/gen_lab_index.py, tools/serve_labs.py and tools/labs_preview.sh with marker
scripts that create a file when run. The real labs_preview.sh runs in the clone
with `lab-x` as an explicit argument (the explicit-branch mode is kept).

MUST-FAIL CONTROLS (a green only counts next to these):
  - the branch really merged (x.html is in the preview tree) and its marker
    scripts really are in it;
  - running the merged tree's marker script by hand DOES create the marker, so a
    missing marker means "not run", not "marker cannot fire";
  - the navigator the preview built lists x.html, so the main checkout's
    generator did run, on the merged tree;
  - a static scan for the old failure shape (a relative `tools/` or `$PREVIEW`
    interpreter call) flags the old script's line and passes the new script.

THE SLUICE OPT-IN (B446 W3c). `--allow-sluice` is OFF by default. Without it the
preview has no `local/sluice` link and the printed serve command is the plain one.
With it the script links the preview's `local/sluice` to the MAIN checkout's AFTER the
merges, so a branch that force-added its own `local/sluice` (a symlink anywhere) does
not choose where the link points, and the printed command carries `--allow-sluice`.
Controls: the branch's own link really is in the merged tree without the flag; with the
flag but no main-checkout link nothing is linked and the command stays plain.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
fails = []


def check(ok, what):
    if not ok:
        fails.append(what)
        print(f"FAIL  {what}")


def sh(args, cwd, **kw):
    env = dict(os.environ, GIT_AUTHOR_NAME="fixture", GIT_AUTHOR_EMAIL="f@localhost",
               GIT_COMMITTER_NAME="fixture", GIT_COMMITTER_EMAIL="f@localhost",
               GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_SYSTEM=os.devnull)
    return subprocess.run(args, cwd=cwd, env=env, capture_output=True, text=True, **kw)


def git(cwd, *a):
    r = sh(["git", *a], cwd)
    if r.returncode != 0:
        raise SystemExit(f"fixture git {' '.join(a)} failed: {r.stderr}")
    return r.stdout


LAB = ('<!doctype html><title>{t}</title><meta name="lab-review" content="W3b fixture">'
       '<div class="tagline">fixture</div>\n')

# What the OLD script did, as a line the static scan must catch.
OLD_SHAPE = 'python3 tools/gen_lab_index.py >/dev/null && git add docs/design/index.html && \\'
EXEC_RE = re.compile(r'^\s*(?:[\w./$"{}-]*/)?(?:python3?|bash|sh|node)\s+(?:-\w+\s+)*"?(?:\$PREVIEW|\$\{PREVIEW\}|\.?/?tools/)')


def main():
    # Static scan.
    script = open(os.path.join(ROOT, "tools/labs_preview.sh"), encoding="utf-8").read()
    code = [l for l in script.splitlines() if not l.lstrip().startswith("#")]
    check(EXEC_RE.search(OLD_SHAPE) is not None, "CONTROL: the static scan no longer flags the old script's line")
    for l in code:
        check(not EXEC_RE.search(l), f"labs_preview.sh runs from a relative or $PREVIEW tools/ path: {l.strip()}")
    check("git merge" in script and "origin/main" in script, "labs_preview.sh no longer merges branches (scan stale?)")

    with tempfile.TemporaryDirectory() as base:
        marker = os.path.join(base, "MARKER")
        origin, seed, main_co = (os.path.realpath(os.path.join(base, n)) for n in ("origin.git", "seed", "main"))
        git(base, "init", "-q", "--bare", "-b", "main", origin)
        git(base, "clone", "-q", origin, seed)
        git(seed, "checkout", "-q", "-b", "main")
        os.makedirs(os.path.join(seed, "tools")); os.makedirs(os.path.join(seed, "docs/design"))
        for f in ("labs_preview.sh", "gen_lab_index.py", "serve_labs.py"):
            shutil.copy(os.path.join(ROOT, "tools", f), os.path.join(seed, "tools", f))
        open(os.path.join(seed, "docs/design/a.html"), "w").write(LAB.format(t="lab a"))
        open(os.path.join(seed, "docs/design/index.html"), "w").write("placeholder\n")
        git(seed, "add", "."); git(seed, "commit", "-q", "-m", "seed"); git(seed, "push", "-q", "origin", "main")

        git(seed, "checkout", "-q", "-b", "lab-x")
        open(os.path.join(seed, "docs/design/x.html"), "w").write(LAB.format(t="lab x"))
        plant = f"import pathlib; pathlib.Path({marker!r}).write_text('ran')\n"
        for f in ("gen_lab_index.py", "serve_labs.py"):
            open(os.path.join(seed, "tools", f), "w").write(plant)
        open(os.path.join(seed, "tools/labs_preview.sh"), "w").write(f"#!/usr/bin/env bash\ntouch {marker}\n")
        git(seed, "add", "."); git(seed, "commit", "-q", "-m", "lab x + planted tools")
        git(seed, "push", "-q", "origin", "lab-x")

        git(base, "clone", "-q", "-b", "main", origin, main_co)
        r = sh(["bash", "tools/labs_preview.sh", "lab-x"], main_co)
        check(r.returncode == 0, f"labs_preview.sh exited {r.returncode}: {r.stderr.strip()[:200]}")
        out = r.stdout
        check("origin/main + lab-x" in out, f"the branch did not merge: {out.strip()[:200]}")

        preview = os.path.join(main_co, ".claude/worktrees/labs-preview")
        # THE ASSERTION: the planted scripts did not run.
        check(not os.path.exists(marker), "a file from the merged branch RAN (marker exists)")

        # Controls: the plant is in the merged tree, is live, and the real generator ran.
        check(os.path.exists(os.path.join(preview, "docs/design/x.html")), "CONTROL: lab-x is not in the preview tree")
        check("pathlib" in open(os.path.join(preview, "tools/gen_lab_index.py")).read(),
              "CONTROL: the planted generator is not in the preview tree")
        index = open(os.path.join(preview, "docs/design/index.html"), encoding="utf-8").read()
        check("x.html" in index and "AWAITING YOUR REVIEW (2)" in index,
              "CONTROL: the preview navigator does not list the merged lab (the main checkout's generator did not run on it)")
        check(f"serve_labs.py 8146 {preview}" in out and os.path.join(main_co, "tools") in out,
              "the printed serve command does not use the main checkout's server and the preview as root")
        sh(["python3", "tools/gen_lab_index.py"], preview)
        check(os.path.exists(marker), "CONTROL: running the merged tree's planted script by hand did not create the marker; "
                                      "an absent marker proves nothing")

        # ---- the Sluice opt-in ----
        # Default: no link, plain command (the run above).
        check(not os.path.lexists(os.path.join(preview, "local/sluice")), "without --allow-sluice the preview has a local/sluice")
        check("--allow-sluice" not in out, "without --allow-sluice the printed serve command carries the flag")

        evil = os.path.realpath(os.path.join(base, "evil"))
        os.makedirs(evil)
        open(os.path.join(evil, "secret.md"), "w").write("EVIL")
        git(seed, "checkout", "-q", "-b", "lab-y", "main")
        open(os.path.join(seed, "docs/design/y.html"), "w").write(LAB.format(t="lab y"))
        os.makedirs(os.path.join(seed, "local"))
        os.symlink(evil, os.path.join(seed, "local/sluice"))            # a branch choosing where the link points
        git(seed, "add", "-f", "local/sluice", "docs/design/y.html"); git(seed, "commit", "-q", "-m", "lab y + a forced local/sluice")
        git(seed, "push", "-q", "origin", "lab-y")
        git(main_co, "fetch", "-q", "origin")
        mine = os.path.join(os.path.realpath(main_co), "local/sluice")
        os.makedirs(mine)
        open(os.path.join(mine, "spec.md"), "w").write("MINE")

        r = sh(["bash", "tools/labs_preview.sh", "lab-y"], main_co)
        check(r.returncode == 0 and "origin/main + lab-y" in r.stdout, f"lab-y did not merge without the flag: {r.stdout.strip()[:200]} {r.stderr.strip()[:200]}")
        check(os.path.islink(os.path.join(preview, "local/sluice")) and os.path.realpath(os.path.join(preview, "local/sluice")) == evil,
              "CONTROL: the branch's own local/sluice did not arrive in the merged tree; the relink below proves nothing")
        check("--allow-sluice" not in r.stdout, "without --allow-sluice the printed serve command carries the flag (lab-y run)")

        r = sh(["bash", "tools/labs_preview.sh", "--allow-sluice", "lab-y"], main_co)
        check(r.returncode == 0 and "origin/main + lab-y" in r.stdout, f"--allow-sluice lab-y did not merge: {r.stdout.strip()[:200]} {r.stderr.strip()[:200]}")
        link = os.path.join(preview, "local/sluice")
        check(os.path.islink(link) and os.path.realpath(link) == mine,
              f"--allow-sluice: the preview's local/sluice is {os.path.realpath(link)}, not the main checkout's {mine}")
        check(not os.path.exists(os.path.join(link, "secret.md")) and open(os.path.join(link, "spec.md")).read() == "MINE",
              "--allow-sluice: the branch's link target is reachable through the preview's local/sluice")
        check(f"serve_labs.py --allow-sluice 8146 {preview}" in r.stdout, f"--allow-sluice: the serve command lacks the flag: {r.stdout.strip()[-300:]}")

        # The flag with nothing to link: nothing linked, plain command, a stated reason.
        shutil.rmtree(mine)
        r = sh(["bash", "tools/labs_preview.sh", "--allow-sluice", "lab-x"], main_co)
        check(not os.path.lexists(os.path.join(preview, "local/sluice")) and "--allow-sluice 8146" not in r.stdout and "does not exist" in r.stdout,
              f"--allow-sluice with no main-checkout link: expected no link and a plain command, got: {r.stdout.strip()[-300:]}")

    if fails:
        print(f"\nRED -- labs_preview_check: {len(fails)} failure(s)")
        return 1
    print("labs_preview_check: GREEN -- a branch that replaces tools/ ran nothing; its lab was merged, "
          "indexed by the main checkout's generator and served from the main checkout's server; "
          "--allow-sluice is off by default and links only the main checkout's local/sluice, after the merges")
    return 0


if __name__ == "__main__":
    sys.exit(main())
