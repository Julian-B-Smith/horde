#!/usr/bin/env python3
"""admission_check — horde's hosted-module admission test, run against a module's own code.

WIRED: ./verify full

ROADMAP B561, ADR-295. The test is specified in docs/proposals/rack-slot-contract.md §2.2 (rows
A1-A21, each with a must-fail control), read with its §12 amendments (ADR-218, ADR-234). This runs
the MODULE-ONLY rows, standalone, before horde 2's shell exists:

  A1-A5; A6 tests 1-3 and 7 (docs/proposals/module-io-gain.md); A8-A12; A15; A17-A20; and A21 for
  a module that declares morphPosition.

The SLOT rows wait for the shell (B398) and are listed in the report as such, never as passes:
A6 tests 4-6, A7, A13, A14, A16. CPU is never a gate (ADR-187 §8, RS-8.3): A10's one timing is a
reported figure.

    tools/admission_check.py [--module scape]

THE MODULE'S TREE is located at run time, never committed: $HORDE_SCAPE_DIR, else
~/Documents/Claude/synthetic-worlds/Scape (expanded here). A missing tree prints SKIPPED and exits
0: nothing was measured, and nothing says GREEN (the convention of tools/rtsan_check.py). The tree
is only READ: tools/admission/CMakeLists.txt builds its sources with its own build into
build-admission/<module>/ here. Point the variable at a clean export when the sibling's resident
has work in progress (`git -C <tree> archive origin/main | tar -x -C <dir>`); set
$HORDE_SCAPE_REV to label an export, which has no .git to read a revision from.

HOW A ROW IS JUDGED. tools/admission/harness.cpp prints, per row, the sub-checks that failed, each
with a stable tag. Then every plant (tools/admission/plants.h: a wrapper that injects one fault
through the neutral ABI) is run on its row. A control FIRED when its run reports a failure tag the
clean run did not have, so a control still proves something on a row the clean module already
fails. Verdicts:
  PASS        no failure, and every control of the row fired
  FAIL        a sub-check failed (its controls are still judged and shown)
  NOT PROVEN  no failure, but a control did not fire: the row's detector is not shown to see
  NOT RUN     the row could not run here (A10 with no RealtimeSanitizer compiler), with its reason
  N/A         the row does not apply (A21 for a module that does not declare morphPosition)

RED (exit 1): a build or run failure; any NOT PROVEN; any failure not in PINNED below; a PINNED
failure that no longer fails (a stale pin hides nothing, so it is removed in the PR that fixes it).

PINNED is a ledger of known failures, each a debt with a name and a reason, so the gate is red on
anything NEW while a module's open item is worked; the report prints a pinned row as FAIL, never
as a pass. Adding a pin is weakening this gate: it is the human's call (CLAUDE.md, human gates).

A10, REAL TIME. The harness is rebuilt with RealtimeSanitizer: the compiler rule is
tools/rtsan_check.py's, imported, not restated ($HORDE_SANITIZER_CXX, else Homebrew LLVM, with the
SDK passed as the sysroot). Order: the planted malloc in process (plants.h MallocInProcess) through
the full session FIRST, and it must be reported, or nothing after it is believed; then an empty
scope, which must read zero; then the clean session, which must read zero. No compiler: NOT RUN.
The first block after a reset() whose parameters moved is TIMED and reported (Scape owes a decay
design there), never judged.

The report is written whole to build-admission/<module>/admission-report.{md,json}; stdout carries
the per-row table.
"""
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from rtsan_check import CALLEE, sanitizer_cxx, sdk_path  # ADR-199: one compiler rule, imported

HARNESS_SRC = os.path.join(ROOT, "tools", "admission")

MODULES = {
    "scape": {
        "name": "Scape",
        "env": "HORDE_SCAPE_DIR",
        "rev_env": "HORDE_SCAPE_REV",
        "default": "~/Documents/Claude/synthetic-worlds/Scape",
        "marker": os.path.join("core", "include", "scape", "fx_module.h"),
        "cmake_var": "HORDE_SCAPE_DIR",
        # the module's own sources and the harness's, for naming a sanitizer report's frame
        "sources": ("reverb.cpp", "design.cpp", "fx_module.cpp", "patch.cpp", "dsp_math.h", "reverb.h",
                    "design.h", "fx_module.h", "patch.h", "harness.cpp", "scape_adapter.cpp", "plants.h"),
    },
}

# (row, what) in contract order; the slot rows are listed so the report shows them as waiting.
ROWS = [
    ("A1", "Declarations"), ("A2", "Reset identity"), ("A3", "Block independence"), ("A4", "Latency truth"),
    ("A5", "Silence and tails"), ("A6", "I/O gain, tests 1-3 and 7"), ("A8", "Stereo image"), ("A9", "Non-finite"),
    ("A10", "Real-time"), ("A11", "Parameter classes"), ("A12", "Macros"), ("A15", "Determinism"),
    ("A17", "State completeness"), ("A18", "Restore exactness"), ("A19", "State size"),
    ("A20", "Live-state exclusion"), ("A21", "Morph position"),
]
SLOT_ROWS = [("A6 tests 4-6", "I/O gain at the slot"), ("A7", "Off"), ("A13", "Corner presets"),
             ("A14", "Feedback"), ("A16", "Sleep and wake")]

# Known failures: (module, row, failure tag) -> the human's ref. See the docstring: adding one
# is the human's call. A pinned failure still prints as FAIL [PINNED] with its readings; when it
# stops failing, the run is RED with a STALE PIN notice until the entry is deleted here.
PINNED = {
    # Era's switch is a duck / swap / un-duck inside the module, not an admitted method (§12.1).
    ("scape", "A11", "method[verb.era]"):
        "ADR-296 / B562 (the human, 2026-10-27: Scape crossfades Era; pinned until Scape's fix lands)",
}

LINE = re.compile(r"^(FAIL|NOTE|DETAIL|ROW) (A\d+)(?: (\S+))? :: (.*)$")


def resolve_tree(mod):
    env = os.environ.get(mod["env"])
    return os.path.abspath(os.path.expanduser(env if env else mod["default"]))


def revision(tree, mod):
    label = os.environ.get(mod["rev_env"])
    if label:
        return label
    r = subprocess.run(["git", "-C", tree, "rev-parse", "HEAD"], capture_output=True, text=True)
    if r.returncode != 0:
        return "unknown (not a git checkout; set " + mod["rev_env"] + ")"
    dirty = subprocess.run(["git", "-C", tree, "status", "--porcelain", "--untracked-files=no"],
                           capture_output=True, text=True).stdout.strip()
    return r.stdout.strip() + (" + UNCOMMITTED CHANGES" if dirty else "")


def sh(cmd, log, env=None):
    with open(log, "w", encoding="utf-8") as f:
        return subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, cwd=ROOT, env=env).returncode


def build(build_dir, tree, mod, extra=()):
    os.makedirs(build_dir, exist_ok=True)
    jobs = os.environ.get("HORDE_ADMISSION_JOBS", "2")
    if sh(["cmake", "-S", HARNESS_SRC, "-B", build_dir, "-G", "Unix Makefiles",
           f"-D{mod['cmake_var']}={tree}", *extra], os.path.join(build_dir, "configure.log")) != 0:
        return f"configure FAILED (see {build_dir}/configure.log)"
    if sh(["cmake", "--build", build_dir, "--target", "admission_harness", f"-j{jobs}"],
          os.path.join(build_dir, "build.log")) != 0:
        return f"build FAILED (see {build_dir}/build.log)"
    return None


def harness(exe, module, tree, *args):
    r = subprocess.run([exe, "--module", module, "--tree", tree, *args], capture_output=True, text=True, cwd=ROOT)
    return r.returncode, r.stdout, r.stderr


def parse(out):
    rows = {}
    for line in out.splitlines():
        m = LINE.match(line)
        if not m:
            continue
        kind, row, tag, text = m.groups()
        r = rows.setdefault(row, {"fails": {}, "notes": [], "details": [], "status": None, "summary": ""})
        if kind == "FAIL":
            r["fails"][tag] = text
        elif kind == "NOTE":
            r["notes"].append(text)
        elif kind == "DETAIL":
            r["details"].append(text)
        else:
            r["status"], r["summary"] = tag, text
    return rows


# ---- A1's manifest-text half: the module's own manifest, as it ships, parsed as JSON ----------

MANIFEST_FIELDS = ("address", "class", "rate", "min", "max", "default", "unit")


def manifest_faults(text):
    """Faults of a manifest text: unparseable, or a parameter missing a field or a legal class."""
    try:
        doc = json.loads(text)
    except ValueError as e:
        return [f"json[parse] :: the manifest is not valid JSON ({e})"]
    params = doc.get("params") if isinstance(doc, dict) else None
    if not isinstance(params, list) or not params:
        return ["json[params] :: no parameter list"]
    out = []
    for p in params:
        addr = p.get("address", "?") if isinstance(p, dict) else "?"
        for f in MANIFEST_FIELDS:
            if not isinstance(p, dict) or f not in p:
                out.append(f"json[{f}:{addr}] :: parameter {addr} has no '{f}'")
        if isinstance(p, dict) and p.get("class") not in ("morph", "flip", "device"):
            out.append(f"json[class:{addr}] :: parameter {addr} has class {p.get('class')!r}")
    for f in ("changes_image", "changes_level"):
        if not isinstance(doc.get(f), bool):
            out.append(f"json[{f}] :: '{f}' is not declared (RS-2.4)")
    return out


def manifest_half(exe, module, tree):
    """-> (faults, control_fired, note). The control: the same text with one parameter's class
    removed must read red through the same function."""
    rc, out, err = harness(exe, module, tree, "--manifest-json")
    if rc != 0:
        return [f"json[run] :: --manifest-json exited {rc}: {err.strip()[:200]}"], False, ""
    faults = manifest_faults(out)
    fired = False
    try:
        doc = json.loads(out)
        del doc["params"][0]["class"]
        fired = bool(manifest_faults(json.dumps(doc)))
    except (ValueError, KeyError, IndexError, TypeError):
        fired = False
    mem = ""
    try:
        mb = json.loads(out).get("memory_bytes")
        if isinstance(mb, dict) and mb:
            lo, hi = min(mb.values()), max(mb.values())
            mem = f"memory per instance (RS-2.6, the module's figure): {lo / 2**20:.1f} MiB to {hi / 2**20:.1f} MiB over {len(mb)} rates"
    except ValueError:
        pass
    return faults, fired, mem


# ---- A10 -------------------------------------------------------------------------------------

def rtsan_violations(text, sources):
    """-> (count, [first few 'callee at file:line'])."""
    blocks = text.split("ERROR: RealtimeSanitizer: ")[1:]
    named = []
    frame = re.compile(r"^\s+#\d+ 0x[0-9a-f]+ in (.*?) (\S+?):(\d+)")
    for b in blocks[:5]:
        m = CALLEE.search(b)
        callee = m.group(1) if m else b.split("\n", 1)[0].strip()
        loc = "(no module or harness frame)"
        for line in b.splitlines():
            fm = frame.match(line)
            if fm and os.path.basename(fm.group(2)) in sources:
                loc = f"{os.path.basename(fm.group(2))}:{fm.group(3)}"
                break
        named.append(f"{callee} at {loc}")
    return len(blocks), named


def row_a10(build_root, tree, module, mod, exe):
    """-> row dict for A10, in parse()'s shape plus 'controls'."""
    r = {"fails": {}, "notes": [], "details": [], "status": None, "summary": "", "controls": []}
    rc, out, _ = harness(exe, module, tree, "--timing")
    r["notes"] += [l.split(" :: ", 1)[1] for l in out.splitlines() if l.startswith("NOTE A10 :: ")]
    cxx, sdk = sanitizer_cxx(), sdk_path()
    if not cxx or not sdk:
        r["status"] = "NOTRUN"
        r["summary"] = ("no RealtimeSanitizer compiler (set HORDE_SANITIZER_CXX, or `brew install llvm`); Apple clang has none"
                        if not cxx else "xcrun finds no SDK")
        return r
    bdir = os.path.join(build_root, module + "-rtsan")
    err = build(bdir, tree, mod, ("-DCMAKE_CXX_COMPILER=" + cxx, "-DCMAKE_OSX_SYSROOT=" + sdk, "-DADMISSION_RTSAN=ON"))
    if err:
        r["fails"]["build"] = "the sanitizer build: " + err
        r["status"] = "FAIL"
        return r
    rexe = os.path.join(bdir, "admission_harness")
    env = dict(os.environ, RTSAN_OPTIONS="halt_on_error=false")

    def probe(name, *args):
        log = os.path.join(bdir, f"rtsan.{name}.log")
        with open(log, "w", encoding="utf-8") as f:
            code = subprocess.run([rexe, "--module", module, "--tree", tree, *args], stdout=f,
                                  stderr=subprocess.STDOUT, cwd=ROOT, env=env).returncode
        text = open(log, encoding="utf-8", errors="replace").read()
        n, named = rtsan_violations(text, mod["sources"])
        return code, n, named, text

    # THE PLANT FIRST: a clean session means nothing unless this binary reports the malloc.
    code, n, named, _ = probe("plant", "--rtsan", "session", "--plant", "malloc-in-process")
    fired = n >= 1 and any(x.startswith("malloc") for x in named)
    r["controls"].append({"plant": "malloc-in-process", "what": "a malloc in process", "fired": fired,
                          "new": [f"{n} violation(s): " + "; ".join(named[:2])] if n else []})
    if not fired:
        r["status"] = "NOTPROVEN"
        r["summary"] = f"the planted malloc in process was NOT reported ({n} violations): RealtimeSanitizer is not judging this build"
        return r
    code, n, _, _ = probe("control", "--rtsan", "control")
    if n or code != 0:
        r["status"] = "NOTPROVEN"
        r["summary"] = f"an EMPTY realtime scope reported {n} violation(s) (exit {code}): the session's verdict cannot be trusted"
        return r
    code, n, named, text = probe("session", "--rtsan", "session")
    done = [l for l in text.splitlines() if l.startswith("rtsan: session done")]
    if n:
        r["fails"]["violations"] = f"{n} RealtimeSanitizer report(s) in the session; first: " + "; ".join(named[:3])
    if code != 0 or not done:
        r["fails"]["session"] = f"the session did not complete (exit {code})"
    r["status"] = "FAIL" if r["fails"] else "PASS"
    r["summary"] = (done[0][len("rtsan: "):] if done else "session incomplete") + \
        f"; compiler {os.path.basename(os.path.dirname(os.path.dirname(cxx)))}/{os.path.basename(cxx)}"
    return r


# ---- the run ----------------------------------------------------------------------------------

def verdict(row):
    st = row["status"]
    if st == "NA":
        return "N/A"
    if st == "NOTRUN":
        return "NOT RUN"
    if st == "NOTPROVEN":
        return "NOT PROVEN"
    if row["fails"]:
        return "FAIL"
    if not row["controls"] or not all(c["fired"] for c in row["controls"]):
        return "NOT PROVEN"
    return "PASS"


def main():
    args = sys.argv[1:]
    module = "scape"
    if "--module" in args:
        module = args[args.index("--module") + 1]
    if module not in MODULES:
        print(f"admission_check: unknown module {module!r} (known: {', '.join(MODULES)})")
        return 2
    mod = MODULES[module]
    tree = resolve_tree(mod)
    if not os.path.isfile(os.path.join(tree, mod["marker"])):
        print(f"admission_check: WARNING — no {mod['name']} tree at {tree} (set {mod['env']})")
        print("admission_check: SKIPPED — nothing was measured; this is NOT a pass")
        return 0
    rev = revision(tree, mod)
    build_root = os.path.join(ROOT, "build-admission")
    bdir = os.path.join(build_root, module)
    err = build(bdir, tree, mod)
    if err:
        print(f"admission_check: RED — {err}")
        return 1
    exe = os.path.join(bdir, "admission_harness")

    rc, out, errout = harness(exe, module, tree)
    if rc != 0:
        print(f"admission_check: RED — the harness exited {rc} on the clean module:\n{errout.strip()[-800:]}")
        return 1
    rows = parse(out)
    for row in rows.values():
        row["controls"] = []

    rc, out, _ = harness(exe, module, tree, "--list-plants")
    plants = [l.split(" ", 3) for l in out.splitlines() if l.startswith("PLANT ")]
    for _, pid, prow, rest in plants:
        if prow == "A10":
            continue   # run under the sanitizer, below
        what = rest.removeprefix(":: ")
        rc, pout, perr = harness(exe, module, tree, "--plant", pid, "--rows", prow, "--no-readings")
        clean = set(rows.get(prow, {}).get("fails", {}))
        planted = parse(pout).get(prow, {"fails": {}})["fails"] if rc == 0 else {}
        new = sorted(set(planted) - clean)
        entry = {"plant": pid, "what": what, "fired": bool(new), "new": [f"{t}: {planted[t]}" for t in new[:2]]}
        if rc != 0:
            entry["new"] = [f"the planted run exited {rc}: {perr.strip()[-200:]}"]
        rows.setdefault(prow, {"fails": {}, "notes": [], "details": [], "status": None, "summary": "", "controls": []})
        rows[prow]["controls"].append(entry)

    faults, fired, mem = manifest_half(exe, module, tree)
    a1 = rows.setdefault("A1", {"fails": {}, "notes": [], "details": [], "status": None, "summary": "", "controls": []})
    for f in faults:
        tag, _, text = f.partition(" :: ")
        a1["fails"][tag] = text
    a1["controls"].append({"plant": "json-class-removed", "what": "the manifest text with one class removed", "fired": fired,
                           "new": ["read red by the manifest-text check"] if fired else []})
    if mem:
        a1["notes"].append(mem)

    rows["A10"] = row_a10(build_root, tree, module, mod, exe)

    # ---- verdicts, pins, the report
    red = []
    table = []
    for rid, what in ROWS:
        row = rows.get(rid)
        if row is None:
            red.append(f"{rid}: the harness printed no verdict")
            table.append((rid, what, "MISSING", "", "", []))
            continue
        v = verdict(row)
        pinned = {t for t in row["fails"] if (module, rid, t) in PINNED}
        unpinned = sorted(set(row["fails"]) - pinned)
        if v == "NOT PROVEN":
            red.append(f"{rid}: NOT PROVEN — " + (row["summary"] if row["status"] == "NOTPROVEN" else
                       "control(s) that did not fire: " + ", ".join(c["plant"] for c in row["controls"] if not c["fired"])))
        if unpinned:
            red.append(f"{rid}: FAIL — " + "; ".join(f"{t}: {row['fails'][t]}" for t in unpinned[:3]))
        ctl = ", ".join(f"{c['plant']} {'fired' if c['fired'] else 'DID NOT FIRE'}" for c in row["controls"]) or "-"
        table.append((rid, what, v + (" PINNED" if pinned and not unpinned else ""), ctl, row["summary"], row))
    for (m, rid, tag), why in PINNED.items():
        if m == module and tag not in rows.get(rid, {}).get("fails", {}):
            red.append(f"{rid}: STALE PIN — {tag} no longer fails: remove its entry from PINNED in "
                       f"tools/admission_check.py ({why})")

    head = git_head()
    write_report(bdir, mod, module, rev, head, table)
    print(f"== admission_check: {mod['name']} at {rev} (horde {head})")
    for rid, what, v, ctl, summary, _ in table:
        print(f"   {rid:<4} {v:<11} {what:<26} controls: {ctl}")
    for rid, what in SLOT_ROWS:
        print(f"   {rid:<4} {'SLOT':<11} {what:<26} waits for horde 2's shell (B398)")
    for rid, _, v, _, _, row in table:
        if not row:
            continue
        for t, d in row["fails"].items():
            pin = PINNED.get((module, rid, t))
            print(f"   {rid} FAIL {t}{' [PINNED: ' + pin + ']' if pin else ''}: {d}")
        if v.startswith("NOT RUN"):
            print(f"   {rid} NOT RUN: {row['summary']}")
        for n in row["notes"]:
            print(f"   {rid} note: {n}")
    print(f"   report: build-admission/{module}/admission-report.md")
    if red:
        print("== admission_check: RED")
        for r in red:
            print("   " + r)
        return 1
    held = [f"{r} {t}" for (m, r, t) in PINNED if m == module]
    print("== admission_check: GREEN (every module-only row judged or reported; every control fired"
          + (f"; {len(held)} PINNED failure(s) still failing, shown above: {', '.join(held)})" if held else ")"))
    return 0


def git_head():
    r = subprocess.run(["git", "-C", ROOT, "rev-parse", "--short", "HEAD"], capture_output=True, text=True)
    return r.stdout.strip() or "?"


def write_report(bdir, mod, module, rev, head, table):
    md = [f"# Admission test: {mod['name']}", "",
          f"- module revision: `{rev}`", f"- horde: `{head}`",
          "- specification: docs/proposals/rack-slot-contract.md §2.2 with §12; tools/admission_check.py", "",
          "| row | result | controls fired | notes |", "|---|---|---|---|"]
    js = {"module": mod["name"], "revision": rev, "horde": head, "rows": []}
    for rid, what, v, ctl, summary, row in table:
        notes = summary
        if row and row["fails"]:
            notes = "; ".join(f"{t}: {d}" for t, d in row["fails"].items()) + " | " + summary
        md.append(f"| {rid} {what} | {v} | {ctl} | {notes} |")
        js["rows"].append({"row": rid, "what": what, "result": v, "summary": summary,
                           "pinned": {t: PINNED[(module, rid, t)] for t in (row["fails"] if row else {})
                                      if (module, rid, t) in PINNED},
                           "fails": row["fails"] if row else {}, "notes": row["notes"] if row else [],
                           "details": row["details"] if row else [],
                           "controls": row["controls"] if row else []})
    for rid, what in SLOT_ROWS:
        md.append(f"| {rid} {what} | SLOT | - | waits for horde 2's shell (B398) |")
    md.append("")
    for rid, what, v, ctl, summary, row in table:
        if not row:
            continue
        md += [f"## {rid} {what}: {v}", "", summary, ""]
        for c in row["controls"]:
            md.append(f"- control `{c['plant']}` ({c['what']}): {'fired' if c['fired'] else 'DID NOT FIRE'}"
                      + (" — " + "; ".join(c["new"]) if c["new"] else ""))
        for t, d in row["fails"].items():
            pin = PINNED.get((module, rid, t))
            md.append(f"- FAIL `{t}`{' **[PINNED: ' + pin + ']**' if pin else ''}: {d}")
        for n in row["notes"]:
            md.append(f"- note: {n}")
        for d in row["details"]:
            md.append(f"  - {d}")
        md.append("")
    with open(os.path.join(bdir, "admission-report.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(md) + "\n")
    with open(os.path.join(bdir, "admission-report.json"), "w", encoding="utf-8") as f:
        json.dump(js, f, indent=1)


if __name__ == "__main__":
    sys.exit(main())
