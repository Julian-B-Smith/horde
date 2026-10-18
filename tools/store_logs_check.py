#!/usr/bin/env python3
"""store_logs_check -- a ./verify run leaves the REAL preset store's logs/ untouched (B507).

WIRED: ./verify fast (the controls and the wiring check run in fast(); the snapshot and compare bracket fast and full in the dispatcher)

THE LEAK. trace_check and endprobe press PANIC, and PANIC writes
$HOME/<store>/logs/panic-<trace counter>.txt (src/hypersaw_clap.cpp writeForensics,
through presetRoot()). Run serially by ./verify with the developer's real HOME,
every run dropped test dumps into the real preset store, beside the human's own
forensic dumps from the field. B505 already gave each oracle its own HOME under
the CI sanitizer (tools/sanitize_oracles.sh); this is the same rule for ./verify.

THREE PARTS, one file:
  (no argument)       fast(): the wiring check and the controls. Nothing here touches the real store.
  snapshot FILE       the dispatcher, before fast/full: write the real store's logs/ listing to FILE.
  compare FILE        the dispatcher, after: red if any file in logs/ is NEW or CHANGED since FILE.

WHAT "CHANGED" MEANS. A panic dump is named by a counter, so a leaking run that
reuses a counter OVERWRITES panic-10.txt instead of adding a file; a check for
"gained a file" alone would read that as clean. The listing is (name, size,
mtime_ns). A file that is only DELETED is not a leak and is not reported.

THE WIRING CHECK (static, no build needed). The store-touching oracles are found
from their sources, not listed here: a tools/*.cpp|mm that calls the panic or
dump hooks (hypersaw_test_panic, hypersaw_test_dump_forensics, panicWithDump),
minus those that redirect HOME themselves (setenv/setEnv of HOME). Each such
oracle that ./verify runs as "$build_dir/<name>" must be run through
in_scratch_home. A floor demands the two known ones are found, so a drifted regex
reads red rather than as an empty list.

THE CONTROLS (every run; they use a FAKE HOME laid out like the real store, never
the real one):
  1. plant a new file in logs/      -> compare MUST read red.
  2. overwrite an existing dump     -> compare MUST read red (the counter-reuse case).
  3. change nothing                 -> compare MUST read green (a detector that is
     always red proves nothing).
  4. run a store-writing child with ./verify's own in_scratch_home (sourced out of
     ./verify, not re-implemented here): the fake store must be unchanged. The same
     child WITHOUT the wrapper must change it -- the must-fire half, proving the
     child really writes where the wrapper is claimed to keep it away.
  5. the wiring check on a copy of ./verify with the wrapper stripped from endprobe
     MUST read red.

NO REAL STORE (CI, a machine that never ran the plugin): snapshot and compare
print that once and pass; the controls still run in full.

KNOWN LIMIT. Anything else writing to the real store's logs/ while a run is in
flight (a DAW with the plugin open, a PANIC pressed by hand) reads as a leak.
That is a false red, not a missed leak; re-run with the host closed.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERIFY = os.path.join(ROOT, "verify")
STORE_VENDOR = "LiftedTruck"   # mirrors src/gui/preset_store.h presetRootFor (presetstore_check pins the C++ side)
HOOK_RE = re.compile(r"hypersaw_test_panic|hypersaw_test_dump_forensics|panicWithDump")
SELF_HOME_RE = re.compile(r"set[eE]nv\(\s*\"HOME\"")
KNOWN_TOUCHERS = {"trace_check", "endprobe"}   # the floor, from the B507 audit


def store_vendor_dir(home, appdata=None):
    """The directory holding one folder per identity (HYPERSAW, and the test build's)."""
    if sys.platform == "darwin":
        return os.path.join(home, "Library", "Application Support", STORE_VENDOR) if home else None
    if sys.platform == "win32":
        return os.path.join(appdata, STORE_VENDOR) if appdata else None
    return os.path.join(home, ".local", "share", STORE_VENDOR) if home else None


def listing(vendor):
    """{relative path: [size, mtime_ns]} of every file under <vendor>/*/logs/."""
    out = {}
    if not vendor or not os.path.isdir(vendor):
        return out
    for ident in sorted(os.listdir(vendor)):
        logs = os.path.join(vendor, ident, "logs")
        if not os.path.isdir(logs):
            continue
        for dirpath, _, files in os.walk(logs):
            for f in files:
                p = os.path.join(dirpath, f)
                st = os.stat(p)
                out[os.path.relpath(p, vendor)] = [st.st_size, st.st_mtime_ns]
    return out


def diff(before, after):
    """Lines naming each file that is new or changed in `after`."""
    bad = []
    for k in sorted(after):
        if k not in before:
            bad.append(f"NEW      {k}")
        elif before[k] != after[k]:
            bad.append(f"CHANGED  {k}")
    return bad


def real_vendor():
    return store_vendor_dir(os.environ.get("HOME"), os.environ.get("APPDATA"))


def cmd_snapshot(path):
    vendor = real_vendor()
    if not vendor or not os.path.isdir(vendor):
        print(f"store_logs_check: no real store at {vendor or '(home unset)'} on this machine; nothing to compare")
        snap = None
    else:
        snap = listing(vendor)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(snap, f)
    return 0


def cmd_compare(path):
    with open(path, encoding="utf-8") as f:
        before = json.load(f)
    if before is None:
        return 0
    bad = diff(before, listing(real_vendor()))
    if bad:
        print("store_logs_check: FAILED -- this run wrote into the REAL store's logs/ "
              "(an oracle ran under the real HOME; wrap it in in_scratch_home in ./verify):", file=sys.stderr)
        for line in bad:
            print("    " + line, file=sys.stderr)
        return 1
    return 0


def store_touchers(verify_text):
    """[(oracle, [verify lines running it])] for every non-self-redirecting store-touching oracle ./verify runs."""
    code = [ln for ln in verify_text.splitlines() if not ln.lstrip().startswith("#")]
    found = []
    tools = os.path.join(ROOT, "tools")
    for fn in sorted(os.listdir(tools)):
        stem, ext = os.path.splitext(fn)
        if ext not in (".cpp", ".mm"):
            continue
        with open(os.path.join(tools, fn), encoding="utf-8", errors="replace") as f:
            src = f.read()
        if not HOOK_RE.search(src) or SELF_HOME_RE.search(src):
            continue
        runs = [ln for ln in code if f'"$build_dir/{stem}"' in ln]
        if runs:   # rtsan_probe / tsan_stress are run by python scripts that redirect HOME themselves
            found.append((stem, runs))
    return found


def wiring_problems(verify_text):
    found = store_touchers(verify_text)
    problems = []
    missing = KNOWN_TOUCHERS - {n for n, _ in found}
    if missing:
        problems.append(f"detector found no store-touching oracle named {sorted(missing)} (its patterns drifted, or ./verify stopped running them)")
    for name, runs in found:
        for ln in runs:
            if not re.search(r'\bin_scratch_home\s+"\$build_dir/' + re.escape(name) + '"', ln):
                problems.append(f"{name} runs under the real HOME: {ln.strip()}")
    return found, problems


def verify_function(verify_text):
    m = re.search(r"^in_scratch_home\(\) \{.*?^\}", verify_text, re.S | re.M)
    return m.group(0) if m else None


def run_controls(verify_text):
    errors = []
    fake = tempfile.mkdtemp(prefix="store_logs_check.")
    try:
        vendor = store_vendor_dir(fake, fake)
        logs = os.path.join(vendor, "HYPERSAW", "logs")
        os.makedirs(logs)
        dump = os.path.join(logs, "panic-10.txt")
        with open(dump, "w") as f:
            f.write("an earlier, legitimate dump\n")
        base = listing(vendor)
        if not base:
            errors.append("control setup: the fake store lists empty")

        # 3 first: nothing changed -> green.
        if diff(base, listing(vendor)):
            errors.append("control 3: an untouched store read as changed (detector is always red)")

        # 1: a new file.
        planted = os.path.join(logs, "panic-11.txt")
        with open(planted, "w") as f:
            f.write("planted\n")
        if not diff(base, listing(vendor)):
            errors.append("control 1: a planted NEW dump read clean")
        os.remove(planted)

        # 2: overwrite an existing dump (size changes, so this does not depend on mtime resolution).
        with open(dump, "w") as f:
            f.write("an earlier, legitimate dump, overwritten by a leaking run\n")
        if not diff(base, listing(vendor)):
            errors.append("control 2: an overwritten dump read clean")
        with open(dump, "w") as f:
            f.write("an earlier, legitimate dump\n")
        base = listing(vendor)

        # 4: ./verify's own wrapper, sourced out of ./verify.
        fn = verify_function(verify_text)
        if fn is None:
            errors.append("control 4: in_scratch_home() not found in ./verify")
        else:
            rel = os.path.relpath(logs, fake)
            child = f'mkdir -p "$HOME/{rel}" && echo leaked > "$HOME/{rel}/panic-12.txt"'
            env = dict(os.environ, HOME=fake)
            prefix = fn + "\n"
            r = subprocess.run(["bash", "-c", prefix + f"in_scratch_home bash -c '{child}'"], env=env, capture_output=True, text=True)
            if r.returncode != 0:
                errors.append(f"control 4: the wrapped child failed (exit {r.returncode}): {r.stderr.strip()}")
            if diff(base, listing(vendor)):
                errors.append("control 4: a child run through in_scratch_home still wrote into the fake real store")
            r = subprocess.run(["bash", "-c", child], env=env, capture_output=True, text=True)
            if not diff(base, listing(vendor)):
                errors.append("control 4 (must-fire): the same child WITHOUT the wrapper left the fake store unchanged")
            # Both halves share one scratch HOME that is removed after the run; a leftover would be a leak of its own.
            if "mktemp" not in fn or "rm -rf" not in fn:
                errors.append("control 4: in_scratch_home neither makes (mktemp) nor removes (rm -rf) its scratch HOME")
    finally:
        shutil.rmtree(fake, ignore_errors=True)

    # 5: strip the wrapper from endprobe in an in-memory copy of ./verify.
    stripped = re.sub(r'\bin_scratch_home\s+("\$build_dir/endprobe")', r"\1", verify_text)
    if stripped == verify_text:
        errors.append("control 5: no 'in_scratch_home \"$build_dir/endprobe\"' to strip")
    elif not wiring_problems(stripped)[1]:
        errors.append("control 5: ./verify with endprobe unwrapped read clean")
    return errors


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "snapshot":
        return cmd_snapshot(sys.argv[2])
    if len(sys.argv) == 3 and sys.argv[1] == "compare":
        return cmd_compare(sys.argv[2])
    if len(sys.argv) != 1:
        print("usage: store_logs_check.py [snapshot FILE | compare FILE]", file=sys.stderr)
        return 64
    with open(VERIFY, encoding="utf-8") as f:
        verify_text = f.read()
    found, problems = wiring_problems(verify_text)
    problems += run_controls(verify_text)
    if problems:
        print("store_logs_check: FAILED", file=sys.stderr)
        for p in problems:
            print("    " + p, file=sys.stderr)
        return 1
    print(f"store_logs_check: GREEN -- {len(found)} store-touching oracle(s) run under in_scratch_home "
          f"({', '.join(n for n, _ in found)}); 5 controls fired as required")
    return 0


if __name__ == "__main__":
    sys.exit(main())
