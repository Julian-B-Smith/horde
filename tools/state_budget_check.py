#!/usr/bin/env python3
"""state_budget_check -- one state-budget table, read by the admission harness, held to the
contract's ruled table, with every factory preset horde holds inside its module's figure
(ROADMAP B701, ADR-401 rulings 1-4, ADR-359 r1).

WIRED: ./verify fast

  python3 tools/state_budget_check.py

WHY. ADR-401 ruled each hosted module's per-corner budget (what the module writes, measured its
own way). The figures live in two places by design: the contract's §12.9 table (the ruling, for
people; docs/proposals/rack-slot-contract.md, under the `state-budget-table` marker comment) and
tools/admission/state_budgets.json (the one table the harness reads). Before B701 the harness
carried its own `kBudget = 2048` in rowA19, a second copy that went stale a day after it was
written (docs/audits/2026-10-29-repo-audit.md, "One rule, two copies"). This check keeps the
copies one.

ROWS.
  TABLE    state_budgets.json and the contract's §12.9 table hold the same modules, row for row,
           with the same bytes, counted figure and status. The JSON's `counts` is the contract's
           "Counted figure" cell as the harness measures it: patch-text where the cell names the
           patch text, state where it reads "its state", own otherwise (ADR-401 r1: each module is
           measured its own way, and A19 fails `no-measure` on a kind it has no measure for). The
           contract's RS-6.12 restates the figures in prose; only the marked table is machine-read.
  HARNESS  tools/admission/harness.cpp's rowA19 (and the comment above it) carries no budget
           literal (2048, 2 KiB, or any figure in the table) and reads Ctx::budget and Ctx::counts;
           no figure has a default anywhere a run without --budget could reach it: Ctx's and
           main()'s `budget` start at 0, plants.h's `ruledFigure` at 0, and nothing assigns a
           number (or a count kind) to them; plants.h registers A19's three plants that land one
           byte past the figure (budget-over-figure returning ruledFigure + 1, patch-text-over,
           preset-text-over); and tools/admission_check.py, imported and run with a stubbed
           subprocess, passes each admission module's figure and count from the table as --budget
           and --counts, and passes neither for a module with no row. Every admission module has a
           row. (That a harness run with no --budget or --counts reads A19 red is shown by
           admission_check itself, every ./verify full: its no-figure and no-measure controls.)
  PRESETS  every hosted-module factory preset horde holds in its TRACKED tree fits its module's
           figure. Discovery is by the module formats horde knows (FORMATS below). Today horde
           holds none: the modules' factory presets live in their own trees, and A19
           (tools/admission_check.py, ./verify full) judges them there. The output says so per
           module rather than reading as a pass over files that were never there.

MUST-FAIL CONTROLS, every run, each red or the check is red:
  - a table row that differs from the contract's (bytes, then counts, then status, mutated in
    memory) fails TABLE;
  - a table missing a contract row fails TABLE;
  - a preset of 4,097 B against a 4,096 B figure (synthetic) fails PRESETS, and one of 4,096 B
    does not;
  - a synthetic preset path in each known format is discovered as its own module's (so "none
    held" is a reading, not a blind glob);
  - rowA19 with `kBudget = 2048` planted back fails HARNESS;
  - a 2048 default planted in Ctx's `budget`, a `std::size_t{1} << 11` default in main()'s, a
    2048 default in plants.h's `ruledFigure`, and a "patch-text" default in Ctx's `counts`, each
    fails HARNESS;
  - plants.h with budget-over-figure declaring the figure itself (not + 1), and with
    preset-text-over unregistered, each fails HARNESS;
  - admission_check fed a table whose figure differs from the one it passes fails HARNESS;
  - admission_check fed a table missing an admission module passes that module no --budget and no
    --counts, and reads no figure for it (its run_module then reports RED).

WHERE THE DECLARED-BUDGET CONTROL LIVES. B701's "a module that declares 4,097 B" is a module, and
only a built module declares a stateBudget(). Its control is plants.h `budget-over-figure` (the
module's declared figure + 1, so 4,097 B against 4,096 B), run on A19 by tools/admission_check.py
in ./verify full against each module tree; it is SKIPPED where no tree exists. This check, with no
build, holds that plant's shape and registration (HARNESS above), not its firing.

FAILS CLOSED: a missing marker, an unparseable table or JSON, no rowA19, or no git file list is RED.
"""
import copy
import fnmatch
import importlib.util
import json
import pathlib
import re
import subprocess
import sys
import tempfile
import types

ROOT = pathlib.Path(__file__).resolve().parent.parent
CONTRACT = ROOT / "docs/proposals/rack-slot-contract.md"
TABLE = ROOT / "tools/admission/state_budgets.json"
HARNESS = ROOT / "tools/admission/harness.cpp"
DRIVER = ROOT / "tools/admission_check.py"
MARKER = "<!-- state-budget-table: read by tools/state_budget_check.py -->"
HEADER = ["Module", "Budget per corner (bytes)", "Counted figure", "Status"]
PLANTS = ROOT / "tools/admission/plants.h"
KINDS = ("patch-text", "state", "own")


def counted_kind(cell):
    """The contract's "Counted figure" cell -> the kind the harness measures (KINDS)."""
    if "patch text" in cell.lower():
        return "patch-text"
    if cell.strip().lower() == "its state":
        return "state"
    return "own"


# Where a module's factory presets would sit if horde held them in its tracked tree, by the
# module's own written format. A module absent from FORMATS has no format horde knows yet; the
# output names it. Paths are fnmatch globs over `git ls-files`, lowercased.
FORMATS = {
    "scape": ["*.scape-patch"],
    "kiln-eq": ["*kiln*/presets/eq/factory/*"],
    "kiln-drive": ["*kiln*/presets/drive/factory/*"],
    "mangle": ["*mangle*/presets/factory/*"],
    "bulwark": ["*bulwark*/presets/factory/*"],
    "shriek": ["*shriek*/presets/factory/*"],
    "sluice": ["*sluice*/presets/factory/*"],
    "echo": ["*echo*/presets/factory/*"],
    "arp": ["*arp*/presets/factory/*"],
    "weir": ["*weir*/presets/factory/*"],
}
# Tracked files that look like a module's presets and are not in its written format: named so a
# reader sees they were looked at, never counted.
NOT_A_MODULE_FORMAT = {
    "reference/maw/presets.js": "Shriek's prototype lab (MAW) presets as JS literals, not its written blob",
    "docs/design/arp-face/bank.snapshot.json": "Arp's starter bank of step patterns (vendored), not a corner record",
}
SKIP_DIRS = ("traces/", "integrations/", "docs/audits/")   # notes about presets, never presets


def parse_contract(text):
    """-> ({name: (bytes, counts kind, status)}, errors) from the table under MARKER."""
    lines = text.splitlines()
    at = [i for i, l in enumerate(lines) if l.strip() == MARKER]
    if len(at) != 1:
        return {}, [f"{CONTRACT.relative_to(ROOT)}: {len(at)} state-budget-table markers (need exactly 1)"]
    rows = []
    for l in lines[at[0] + 1:]:
        if not l.startswith("|"):
            break
        rows.append([c.strip() for c in l.strip().strip("|").split("|")])
    if len(rows) < 3 or rows[0] != HEADER:
        return {}, [f"the marked table's header is {rows[0] if rows else None}, not {HEADER}"]
    out, errs = {}, []
    for r in rows[2:]:
        if len(r) != len(HEADER):
            errs.append(f"contract row {r} has {len(r)} cells, not {len(HEADER)}")
            continue
        name, b, counted, status = r
        if not b.isdigit():
            errs.append(f"contract row {name!r}: budget {b!r} is not a plain byte count")
            continue
        if name in out:
            errs.append(f"contract row {name!r} appears twice")
        out[name] = (int(b), counted_kind(counted), status)
    return out, errs


def load_table(path=TABLE):
    """-> ({key: row}, errors)."""
    try:
        mods = json.loads(pathlib.Path(path).read_text(encoding="utf-8"))["modules"]
    except (OSError, ValueError, KeyError, TypeError) as e:
        return {}, [f"{TABLE.relative_to(ROOT)}: unreadable ({type(e).__name__}: {e})"]
    errs = []
    for k, r in mods.items():
        if not (isinstance(r, dict) and isinstance(r.get("bytes"), int) and r["bytes"] > 0
                and r.get("counts") in KINDS
                and r.get("status") in ("ruled", "provisional") and isinstance(r.get("name"), str)):
            errs.append(f"state_budgets.json: {k!r} needs a name, positive integer bytes, counts "
                        f"{'|'.join(KINDS)} and status ruled|provisional")
    return mods, errs


def rule_table(contract, table):
    errs = []
    by_name = {}
    for k, r in table.items():
        if r.get("name") in by_name:
            errs.append(f"state_budgets.json: {by_name[r['name']]!r} and {k!r} both claim {r['name']!r}")
        by_name[r.get("name")] = k
    for name in sorted(set(contract) - set(by_name)):
        errs.append(f"the contract's row {name!r} has no row in state_budgets.json")
    for name in sorted(set(by_name) - set(contract)):
        errs.append(f"state_budgets.json's {by_name[name]!r} ({name!r}) has no row in the contract's table")
    for name in sorted(set(contract) & set(by_name)):
        r = table[by_name[name]]
        if (r["bytes"], r.get("counts"), r["status"]) != contract[name]:
            errs.append(f"{name}: state_budgets.json says {r['bytes']} B counting {r.get('counts')} {r['status']}, "
                        f"the contract {contract[name][0]} B counting {contract[name][1]} {contract[name][2]}")
    return errs


def a19_source(src):
    """-> rowA19's leading comment block plus its body, or None."""
    m = re.search(r"((?://[^\n]*\n)*)Out rowA19\(const Ctx& c\) \{\n.*?\n\}\n", src, re.S)
    return m.group(0) if m else None


def rule_harness_source(src, table):
    body = a19_source(src)
    if body is None:
        return ["tools/admission/harness.cpp: no `Out rowA19(const Ctx& c)` found"]
    errs = []
    figures = {2048} | {r["bytes"] for r in table.values()}
    for lit in sorted({int(x) for x in re.findall(r"(?<![\w.])(\d+)(?![\w.])", body)} & figures):
        errs.append(f"rowA19 carries the literal {lit}: the figure comes from state_budgets.json via --budget")
    if re.search(r"\b2 KiB\b|\b4 KiB\b|\b12 KiB\b", body):
        errs.append("rowA19's comment or failure text names a KiB figure; it should name the ruled figure")
    if re.search(r"(?i)\w*budget\w*\s*=\s*\d", body):
        errs.append("rowA19 assigns a number to a budget")
    if "c.budget" not in body:
        errs.append("rowA19 does not read Ctx::budget")
    if "c.counts" not in body:
        errs.append("rowA19 does not read Ctx::counts")
    return errs


def block(src, opener):
    """-> the brace-balanced body that follows `opener`, or None."""
    i = src.find(opener)
    if i < 0:
        return None
    j = src.find("{", i)
    depth = 0
    for k in range(j, len(src)):
        depth += {"{": 1, "}": -1}.get(src[k], 0)
        if depth == 0:
            return src[j:k + 1]
    return None


def rule_defaults(src, plants_src):
    """No figure and no count kind has a default a run without --budget/--counts could reach."""
    errs = []
    ctx, main_ = block(src, "struct Ctx {"), block(src, "int main(int argc, char** argv)")
    for where, body in (("struct Ctx", ctx), ("main()", main_)):
        if body is None:
            errs.append(f"tools/admission/harness.cpp: no {where} found")
            continue
        if not re.search(r"^\s*std::size_t budget = 0;", body, re.M):
            errs.append(f"harness.cpp {where}: `budget` does not start at 0 (`std::size_t budget = 0;`)")
        if not re.search(r"^\s*std::string counts;", body, re.M):
            errs.append(f"harness.cpp {where}: `counts` does not start empty (`std::string counts;`)")
    for name, text in (("harness.cpp", src), ("plants.h", plants_src)):
        # a number anywhere on the right of an assignment to a figure (2048, 1 << 11, a cast of
        # one); the declarations' `= 0` are the only literal allowed
        for m in re.finditer(r"\b(budget|ruledFigure)\s*=(?!=)\s*([^;]*);", text):
            if re.search(r"\d", m.group(2)) and m.group(2).strip() != "0":
                errs.append(f"{name}: `{m.group(0).strip()}` gives a figure a number; it comes only from --budget")
        for m in re.finditer(r"\bcounts\s*=(?!=)\s*\"", text):
            errs.append(f"{name}: `counts` is given a literal kind; it comes only from --counts")
    if not re.search(r"^inline std::size_t ruledFigure = 0;", plants_src, re.M):
        errs.append("plants.h: `ruledFigure` does not start at 0 (`inline std::size_t ruledFigure = 0;`)")
    return errs


def rule_plants(plants_src):
    """A19's plants that land one byte past the figure exist and are registered on A19."""
    errs = []
    cls = block(plants_src, "class BudgetOverFigure final : public Forward")
    if cls is None or not re.search(r"stateBudget\(\) const override \{ return ruledFigure \+ 1; \}", cls):
        errs.append("plants.h: BudgetOverFigure does not declare `ruledFigure + 1` as its stateBudget()")
    for pid, wrap in (("budget-over-figure", "wrapWith<BudgetOverFigure>"), ("patch-text-over", "wrapWith<PatchTextOver>"),
                      ("preset-text-over", "presetTextOver")):
        if not re.search(r'\{"%s", "A19", "[^"]*", [^}]*%s' % (re.escape(pid), re.escape(wrap)), plants_src):
            errs.append(f"plants.h: plant {pid!r} is not registered on A19 with {wrap}")
    return errs


def load_driver():
    spec = importlib.util.spec_from_file_location("admission_check_for_budget", DRIVER)
    mod = importlib.util.module_from_spec(spec)
    sys.path.insert(0, str(ROOT / "tools"))
    spec.loader.exec_module(mod)
    return mod


def rule_driver(drv, table):
    """Run admission_check.harness() for every admission module with subprocess.run stubbed, and
    require it to pass the table's figure as --budget."""
    errs = []
    seen = []
    real = drv.subprocess

    def fake_run(cmd, **_):
        seen.append(list(cmd))
        return types.SimpleNamespace(returncode=0, stdout="", stderr="")

    drv.subprocess = types.SimpleNamespace(run=fake_run)
    try:
        for key in drv.MODULES:
            if key not in table:
                errs.append(f"admission module {key!r} has no row in state_budgets.json")
                continue
            seen.clear()
            drv.harness("/nonexistent/admission_harness", key, "/nonexistent/tree", "--rows", "A19")
            cmd = seen[0] if seen else []
            for flag, want in (("--budget", str(table[key]["bytes"])), ("--counts", table[key].get("counts"))):
                i = cmd.index(flag) if flag in cmd else -1
                if i < 0 or i + 1 >= len(cmd) or cmd[i + 1] != want:
                    errs.append(f"admission_check.harness() for {key!r} does not pass {flag} {want} (passed: "
                                f"{cmd[i + 1] if 0 <= i < len(cmd) - 1 else 'none'})")
    finally:
        drv.subprocess = real
    return errs


def driver_without_row(drv, table, key):
    """admission_check with a table lacking `key`: -> (figure it reads, the harness command it builds)."""
    seen = []
    real_sp, real_b = drv.subprocess, drv.BUDGETS
    with tempfile.TemporaryDirectory() as tmp:
        alt = pathlib.Path(tmp) / "state_budgets.json"
        alt.write_text(json.dumps({"modules": {k: r for k, r in table.items() if k != key}}), encoding="utf-8")
        drv.BUDGETS = str(alt)
        drv.subprocess = types.SimpleNamespace(
            run=lambda cmd, **_: seen.append(list(cmd)) or types.SimpleNamespace(returncode=0, stdout="", stderr=""))
        try:
            figure = drv.ruled_budget(key)[0]
            drv.harness("/nonexistent/admission_harness", key, "/nonexistent/tree", "--rows", "A19")
        finally:
            drv.subprocess, drv.BUDGETS = real_sp, real_b
    return figure, (seen[0] if seen else [])


def tracked():
    r = subprocess.run(["git", "-C", str(ROOT), "ls-files", "-z"], capture_output=True)
    if r.returncode != 0:
        return None
    return [p for p in r.stdout.decode("utf-8", "replace").split("\0") if p]


def held_presets(files, size=lambda p: (ROOT / p).stat().st_size):
    """-> [(path, module key, bytes)] for tracked files in a module's written format."""
    out = []
    for p in files:
        if p.startswith(SKIP_DIRS) or p in NOT_A_MODULE_FORMAT:
            continue
        low = p.lower()
        for key, globs in FORMATS.items():
            if any(fnmatch.fnmatch(low, g) for g in globs):
                out.append((p, key, size(p)))
                break
    return out


def rule_presets(held, table):
    errs = []
    for path, key, n in held:
        if key not in table:
            errs.append(f"{path}: module {key!r} has no row in state_budgets.json")
        elif n > table[key]["bytes"]:
            errs.append(f"{path}: {n} bytes, over {table[key]['name']}'s ruled {table[key]['bytes']} B per corner")
    return errs


def main():
    fails, controls = [], 0

    def row(tag, errs):
        fails.extend(f"{tag}: {e}" for e in errs)
        print(f"  {'PASS' if not errs else 'FAIL'}  {tag}")

    def control(tag, red):
        nonlocal controls
        controls += 1
        if not red:
            fails.append(f"CONTROL {tag}: read green; the rule cannot see the fault it exists for")
        print(f"  {'PASS' if red else 'FAIL'}  control {tag}")

    print("state_budget_check:")
    contract, cerr = parse_contract(CONTRACT.read_text(encoding="utf-8"))
    table, terr = load_table()
    row("TABLE", cerr + terr + (rule_table(contract, table) if contract and table else []))

    src = HARNESS.read_text(encoding="utf-8")
    psrc = PLANTS.read_text(encoding="utf-8")
    herr = rule_harness_source(src, table) + rule_defaults(src, psrc) + rule_plants(psrc)
    try:
        drv = load_driver()
        derr = rule_driver(drv, table)
    except Exception as e:   # fail closed: a driver that cannot be imported has not been shown to pass anything
        drv, derr = None, [f"tools/admission_check.py could not be imported and run ({type(e).__name__}: {e})"]
    row("HARNESS", herr + derr)

    files = tracked()
    if files is None:
        row("PRESETS", ["`git ls-files` failed: the tracked tree could not be listed"])
        held = []
    else:
        held = held_presets(files)
        row("PRESETS", rule_presets(held, table))
        counts = {k: sum(1 for _, m, _ in held if m == k) for k in sorted(table)}
        none = [table[k]["name"] for k, n in counts.items() if n == 0]
        some = [f"{table[k]['name']} {n}" for k, n in counts.items() if n]
        print(f"    factory presets horde holds: {', '.join(some) if some else 'none for any module'}"
              + (f"; none held for {', '.join(none)} (their presets live in their own trees; A19 judges "
                 f"Scape's and Kiln's there)" if none and some else
                 " (every module's presets live in its own tree; A19 judges Scape's and Kiln's there)" if none else ""))
        for p, why in NOT_A_MODULE_FORMAT.items():
            if p in files:
                print(f"    looked at, not counted: {p} ({why})")

    # ---- controls
    if contract and table:
        k0 = sorted(table)[0]
        bumped = copy.deepcopy(table)
        bumped[k0]["bytes"] += 1
        control(f"a table row whose bytes differ from the contract's ({table[k0]['name']} "
                f"{bumped[k0]['bytes']} B) fails TABLE", bool(rule_table(contract, bumped)))
        recount = copy.deepcopy(table)
        recount[k0]["counts"] = next(k for k in KINDS if k != recount[k0]["counts"])
        control("a table row whose counted figure differs from the contract's fails TABLE",
                bool(rule_table(contract, recount)))
        flipped = copy.deepcopy(table)
        flipped[k0]["status"] = "provisional" if flipped[k0]["status"] == "ruled" else "ruled"
        control("a table row whose status differs from the contract's fails TABLE", bool(rule_table(contract, flipped)))
        dropped = copy.deepcopy(table)
        del dropped[k0]
        control("a table missing a contract row fails TABLE", bool(rule_table(contract, dropped)))
    else:
        fails.append("TABLE controls could not run (no contract table or no JSON)")
    # discovery is not blind: "none held" means something only if each format's glob can match
    probe = {k: g[0].replace("*", "x") for k, g in FORMATS.items()}
    found = {k for _, k, _ in held_presets(list(probe.values()), size=lambda p: 0)}
    control(f"a synthetic preset path in each of the {len(FORMATS)} known formats is discovered as its module's",
            all(k in found for k in probe) and
            [k for _, k, _ in held_presets(list(probe.values()), size=lambda p: 0)] == list(probe))
    synthetic = {"synthetic": {"name": "Synthetic", "bytes": 4096, "status": "ruled"}}
    control("a preset of 4,097 B against a 4,096 B figure fails PRESETS (one of 4,096 B does not)",
            bool(rule_presets([("(synthetic)", "synthetic", 4097)], synthetic))
            and not rule_presets([("(synthetic)", "synthetic", 4096)], synthetic))
    planted = re.sub(r"(Out rowA19\(const Ctx& c\) \{\n)", r"\1  const std::size_t kBudget = 2048;\n", src, count=1)
    control("rowA19 with `kBudget = 2048` planted back fails HARNESS",
            planted != src and bool(rule_harness_source(planted, table)))
    for tag, mutate in (
            ("a 2048 default in Ctx's `budget`",
             lambda t, p: (t.replace("struct Ctx {", "struct Ctx {\n  std::size_t budget = 2048;", 1).replace(
                 "  std::size_t budget = 0;\n  // What", "  // What", 1), p)),
            ("a `std::size_t{1} << 11` default in main()'s `budget`",
             lambda t, p: (re.sub(r"(int main\(int argc, char\*\* argv\) \{\n(?:.*\n){0,4}?\s*)std::size_t budget = 0;",
                                  r"\1std::size_t budget = std::size_t{1} << 11;", t, count=1), p)),
            ("a 2048 default in plants.h's `ruledFigure`",
             lambda t, p: (t, p.replace("inline std::size_t ruledFigure = 0;", "inline std::size_t ruledFigure = 2048;", 1))),
            ("a \"patch-text\" default in Ctx's `counts`",
             lambda t, p: (t.replace("  std::string counts;\n};", '  std::string counts = "patch-text";\n};', 1), p)),
            ("budget-over-figure declaring the figure itself, not + 1",
             lambda t, p: (t, p.replace("{ return ruledFigure + 1; }", "{ return ruledFigure; }", 1))),
            ("preset-text-over unregistered",
             lambda t, p: (t, re.sub(r'\s*\{"preset-text-over",[^\n]*', "", p, count=1)))):
        t2, p2 = mutate(src, psrc)
        control(f"{tag} fails HARNESS", (t2, p2) != (src, psrc) and
                bool(rule_defaults(t2, p2) + rule_plants(p2) + rule_harness_source(t2, table)))
    if drv is not None:
        key = sorted(drv.MODULES)[0]
        fig, cmd = driver_without_row(drv, table, key)
        control(f"admission_check fed a table with no {key!r} row reads no figure and passes no --budget or --counts",
                fig is None and bool(cmd) and "--budget" not in cmd and "--counts" not in cmd)
        with tempfile.TemporaryDirectory() as tmp:
            other = copy.deepcopy(table)
            for r in other.values():
                r["bytes"] += 1
            alt = pathlib.Path(tmp) / "state_budgets.json"
            alt.write_text(json.dumps({"modules": other}), encoding="utf-8")
            real, drv.BUDGETS = drv.BUDGETS, str(alt)
            try:
                # the driver now passes the altered figures; judged against the real table, it must read red
                red = bool(rule_driver(drv, table))
            finally:
                drv.BUDGETS = real
        control("admission_check passing a figure other than the table's fails HARNESS", red)
    else:
        fails.append("the driver control could not run (admission_check not importable)")

    if fails:
        print("state_budget_check: RED", file=sys.stderr)
        for f in fails:
            print(f"    {f}", file=sys.stderr)
        return 1
    ruled = sum(1 for r in table.values() if r["status"] == "ruled")
    print(f"state_budget_check: GREEN ({len(table)} modules = the contract's §12.9 table, {ruled} ruled, "
          f"{len(table) - ruled} provisional; harness reads --budget for {len(drv.MODULES)} admission modules; "
          f"{len(held)} held factory presets; {controls} controls fired)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
