#!/usr/bin/env python3
"""build_flags_check -- the compiler and linker flags that decide numerics and safety are PINNED
(B448 C2, ADR-197 risk row 11: build/platform divergence).

WIRED: ./verify fast (static, from the sources); ./verify full adds --built (the configured tree).

WHY. One stray -ffast-math licenses the compiler to assume no NaN and no inf, folds away the
very tests that guard the output, and reassociates sums so a parity golden stops meaning
anything -- while every test still "passes" because the tests were compiled under it too. One
changed -O level or -ffp-contract moves bits (h2's whole parity story is contraction-off, ruled
2026-09-30). Review does not see a flag in a 1100-line CMakeLists. A diff against a pin does.

WHAT ALREADY EXISTED, AND WHAT THIS ADDS. tools/h2_rules_check.py rule 2 enforces that every
target compiling h2 code carries -ffp-contract=off (and the two FMA controls =fast). It does not
look at the legacy plugin, the -O level, fast-math, sanitizers, the C++ standard, the deployment
target, build types, or compiler lines outside CMake, and a flag changed on a target it does not
classify passes it. This file pins ALL of them and defers contraction-on-h2 to that rule (not
duplicated: the pin merely also records the flags it demands, so removing one is a pin diff).

THE PIN. tools/build_flags_pin.json records, extracted STATICALLY from CMakeLists.txt (no
configure, no network, ~0.1 s):
  global         every set(CMAKE_*), add_compile_options/add_link_options/... : the C++ standard,
                 deployment target, architectures (UNSET today: native only), MSVC runtime. Any
                 NEW global flag setter is a diff.
  classes        per target, the explicit compile and link options and the condition they sit
                 under (NOT MSVC, x86_64...), grouped as legacy_plugin / h2_parity / h2_product /
                 h2_fma_control / tools_oracles. Optimisation level, -ffp-contract, -mfma etc. are
                 all just flags here. A removed or new target is a diff.
  dynamic_loops  the sanitizer and MSVC-stack loops that stamp flags on "every executable
                 declared so far": the flags and WHICH executables they reach.
  entry_points   the CMAKE_BUILD_TYPE / HYPERSAW_SANITIZE each script and workflow configures with.
  direct_compiles  flags of compiler lines outside CMake (tools/*.py, *.sh, workflows).
Not pinned, and why: -DNDEBUG, -O3 and -arch come from CMake's Release defaults, not from any
file here (the entry point's build type is what is pinned); clap-wrapper's own targets get their
flags from libs/. `--built <build-dir>` reads the configured tree's flags.make to check those.

ALSO CHECKED EVERY RUN, with no pin involved: no member of the fast-math family (see FASTMATH)
appears as a code token in CMakeLists.txt, cmake/, verify, tools/*.sh, tools/*.py or the
workflows (comments and docstrings are blanked first: nan_latch_check.py names the flag in a
comment). And -ffp-contract=fast/on may only sit on a target named *_fma_control.

MODES. (none) check;  --init first pin;  --approve <target|section> <ref>  a HUMAN-approved
re-pin of one item to the tree (non-empty <ref> recorded; refused while fast-math is present);
--built <dir>  also read <dir>/CMakeFiles/*/flags.make.
SELF-CALIBRATING: selftest() plants -ffast-math (and each sibling), a changed -O level, a changed
-ffp-contract, a removed target, a new global flag, a new target and an empty approval ref on
COPIES of the real sources; each must read red / be refused, and the untouched copy must read green.
"""
import ast
import copy
import io
import json
import pathlib
import re
import sys
import tokenize

ROOT = pathlib.Path(__file__).resolve().parent.parent
PIN = pathlib.Path(__file__).resolve().parent / "build_flags_pin.json"
SELF = "tools/build_flags_check.py"

# The -ffast-math umbrella and its parts, plus the MSVC and Intel spellings. A token must be
# delimited (the lookbehind) so "-fno-finite-math-only", the SAFE negation, cannot match.
FASTMATH = re.compile(
    r"(?<![\w-])(-ffast-math|-Ofast|-funsafe-math-optimizations|-menable-unsafe-fp-math|"
    r"-ffinite-math-only|-fno-signed-zeros|-fno-trapping-math|-fassociative-math|"
    r"-freciprocal-math|-fno-math-errno|-fcx-limited-range|-fcx-fortran-rules|-fapprox-func|"
    r"-fno-honor-nans|-fno-honor-infinities|-ffp-model=fast|-fp-model[ =]fast|/fp:fast|/Ofast)"
    r"(?![\w=-])")
CONTRACT_ON = re.compile(r"-ffp-contract=(fast|on)")
FLAG = re.compile(r"^-(O[0-3sgz]?|std=\S+|f(?!ramework)\S+|W\S*|m\S+|g|g\d|DNDEBUG|arch)$")
BUILD_CFG = re.compile(r"-D(CMAKE_BUILD_TYPE|HYPERSAW_SANITIZE)=([A-Za-z0-9_,;]+)")
SHELL_CC = re.compile(r"(?:^|[\s;&|(])(?:g\+\+|c\+\+|clang\+\+|\$CXX|\$\{CXX\})\s+([^\n|;&]*)")
GLOBAL_CMDS = {"add_compile_options", "add_link_options", "add_compile_definitions", "add_definitions",
               "link_libraries", "link_directories"}
FLAG_PROPS = re.compile(r"COMPILE_OPTIONS|COMPILE_FLAGS|LINK_OPTIONS|LINK_FLAGS|INTERPROCEDURAL|"
                        r"CXX_STANDARD|CXX_EXTENSIONS|OSX_ARCHITECTURES|MSVC_RUNTIME|POSITION_INDEPENDENT")


# ---- reading the sources (comments and docstrings blanked) -------------------------

def strip_hash_comments(text, cmake=False):
    """Blank `#` comments. CMake: quote-aware, so a `#` in "..." survives. Shell/YAML: from the
    first ` #`/line-start `#` (a `$#` or `${#x}` is not a comment, and no flag hides there)."""
    out = []
    for line in text.splitlines():
        if cmake:
            q, cut = False, len(line)
            for i, ch in enumerate(line):
                if ch == '"' and (i == 0 or line[i - 1] != "\\"):
                    q = not q
                elif ch == "#" and not q:
                    cut = i
                    break
            out.append(line[:cut])
        else:
            out.append(re.sub(r"(^|\s)#.*", r"\1", line))
    return "\n".join(out)


def python_strings(text):
    """The string literals of a Python file that are CODE: docstrings and comments excluded.
    (nan_latch_check.py writes -ffast-math in a comment explaining why it is not used.)"""
    try:
        doc_lines = set()
        for node in ast.walk(ast.parse(text)):
            if isinstance(node, (ast.Module, ast.FunctionDef, ast.ClassDef, ast.AsyncFunctionDef)):
                b = node.body[0] if node.body else None
                if isinstance(b, ast.Expr) and isinstance(getattr(b, "value", None), ast.Constant) \
                        and isinstance(b.value.value, str):
                    doc_lines.update(range(b.lineno, b.end_lineno + 1))
        out = []
        for tok in tokenize.generate_tokens(io.StringIO(text).readline):
            if tok.type == tokenize.STRING and tok.start[0] not in doc_lines:
                try:
                    out.append(ast.literal_eval(tok.string))
                except (ValueError, SyntaxError):
                    out.append(tok.string)
        return [s for s in out if isinstance(s, str)]
    except (SyntaxError, tokenize.TokenError):
        return [text]


def read_sources(root=ROOT):
    """-> {relpath: text} for every file whose flags matter. A plain dict, so the selftest can
    plant a fault in a COPY and run the very same extraction."""
    files = {}
    pats = ["CMakeLists.txt", "cmake/**/*.cmake", "cmake/**/CMakeLists.txt", "verify", "tools/*.sh",
            "tools/*.py", ".github/workflows/*.yml", ".github/workflows/*.yaml"]
    for pat in pats:
        for p in sorted(root.glob(pat)):
            rel = p.relative_to(root).as_posix()
            if p.is_file() and rel != SELF:
                files[rel] = p.read_text(encoding="utf-8", errors="replace")
    return files


def code_text(rel, text):
    """The file's text with comments (and Python docstrings) removed."""
    if rel.endswith(".py"):
        return "\n".join(python_strings(text))
    return strip_hash_comments(text, cmake=rel.endswith((".txt", ".cmake")))


def scan_fastmath(files):
    hits = []
    for rel, text in files.items():
        body = code_text(rel, text)
        for m in FASTMATH.finditer(body):
            hits.append(f"{rel}: fast-math family flag {m.group(1)!r}")
    return hits


# ---- CMake: a small interpreter, only as far as flags need ------------------------

def cmake_commands(text):
    """-> [(name, raw_args)] of a comment-stripped CMake file; quote- and paren-aware."""
    cmds, i, n = [], 0, len(text)
    ident = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
    while i < n:
        m = ident.match(text, i)
        if not m:
            i += 1
            continue
        j = m.end()
        while j < n and text[j] in " \t":
            j += 1
        if j >= n or text[j] != "(":
            i = m.end()
            continue
        depth, k, q = 0, j, False
        while k < n:
            ch = text[k]
            if ch == '"' and text[k - 1] != "\\":
                q = not q
            elif not q and ch == "(":
                depth += 1
            elif not q and ch == ")":
                depth -= 1
                if depth == 0:
                    break
            k += 1
        cmds.append((m.group(0).lower(), text[j + 1:k]))
        i = k + 1
    return cmds


def split_args(raw):
    return [a[1:-1] if len(a) > 1 and a[0] == '"' and a[-1] == '"' else a
            for a in re.findall(r'"[^"]*"|[^\s"]+', raw)]


VISIBILITY = {"PRIVATE", "PUBLIC", "INTERFACE"}


def extract_cmake(text):
    """-> (targets, global_, dynamic_loops).
    targets: {name: [{"when","compile","link"}...]} (every add_executable/add_library, even
    with no options -- a target with none is exactly the legacy plugin).
    `when` is the conjunction of the enclosing if() conditions, so `-O2` under `if(NOT MSVC)` and
    `-mfma` under that AND an x86_64 test stay distinct facts."""
    body = strip_hash_comments(text, cmake=True)
    project = (re.search(r"project\(\s*(\w+)", body) or [None, "PROJECT"])[1]
    sub = lambda s: s.replace("${PROJECT_NAME}", project)
    targets, exes, global_, dyn = {}, [], {}, []
    conds, loops = [], []

    def when():
        return " AND ".join(conds)

    def variant(t):
        for v in targets[t]:
            if v["when"] == when():
                return v
        v = {"when": when(), "compile": [], "link": []}
        targets[t].append(v)
        return v

    for name, raw in cmake_commands(body):
        args = split_args(sub(raw))
        if name == "if":
            conds.append(" ".join(args))
        elif name == "elseif":
            conds[-1] = f"NOT({conds[-1]}) AND {' '.join(args)}"
        elif name == "else":
            conds[-1] = f"NOT({conds[-1]})"
        elif name == "endif":
            conds.pop()
        elif name == "foreach":
            loops.append((args[0], None if "IN" in args else args[1:], len(dyn)))
        elif name == "endforeach":
            loops.pop()
        elif name in ("add_executable", "add_library"):
            targets.setdefault(args[0], [])
            if name == "add_executable":
                exes.append(args[0])
        elif name in ("target_compile_options", "target_link_options") and args:
            flags = [a for a in args[1:] if a not in VISIBILITY]
            field = "compile" if name == "target_compile_options" else "link"
            tgt = args[0]
            m = re.fullmatch(r"\$\{(\w+)\}", tgt)
            loop = next((lp for lp in reversed(loops) if m and lp[0] == m.group(1)), None)
            if loop and loop[1] is None:              # IN LISTS <dynamic>: every executable so far
                d = next((e for e in dyn if e["_loop"] == loop[2] and e["when"] == when()), None)
                if d is None:
                    d = {"when": when(), "compile": [], "link": [], "_loop": loop[2], "_covers": list(exes)}
                    dyn.append(d)
                d[field] += flags
            else:
                for t in (loop[1] if loop else [tgt]):
                    if t not in targets:
                        targets[t] = []               # options on a name no add_* declared: still a fact
                    variant(t)[field] += flags
        elif name == "set" and args and (args[0].startswith("CMAKE_") or args[0] == "HYPERSAW_SANITIZE"):
            key = args[0] + (f" [{when()}]" if when() else "")
            vals = args[1:args.index("CACHE")] if "CACHE" in args else args[1:]   # drop the docstring
            global_[key] = " ".join(vals)
        elif name in GLOBAL_CMDS or (name in ("set_target_properties", "set_property",
                                              "set_source_files_properties")
                                     and FLAG_PROPS.search(raw)):
            global_[f"{name}({' '.join(args)})" + (f" [{when()}]" if when() else "")] = "present"
    covered_by = {}
    for d in dyn:
        covered = d.pop("_covers")
        d.pop("_loop")
        d["executables_covered"] = sorted(covered)
        covered_by[d["when"]] = covered
    targets = {t: [v for v in vs if v["compile"] or v["link"]] for t, vs in targets.items()}
    return targets, global_, dyn


PLUGIN_TARGETS = ("HYPERSAW-impl", "SWARMFX-impl")


def classify(name, variants):
    """The class a target's flags put it in. Names a REASON for each, because a class is only
    useful if reading it tells you what the flags are for."""
    flat = {f for v in variants for f in v["compile"]}
    if name.endswith("_fma_control"):
        return "h2_fma_control"
    if name in PLUGIN_TARGETS:
        return "legacy_plugin"
    if "-ffp-contract=off" in flat:
        return "h2_product" if "-O3" in flat else "h2_parity"
    return "tools_oracles"


CLASS_DOC = {
    "legacy_plugin": "the shipped CLAP/VST3/AU code: no explicit options, so CMAKE_BUILD_TYPE decides (-O3 -DNDEBUG in Release)",
    "h2_parity": "horde 2 parity builds: -O2 -ffp-contract=off, doubles; the tested arithmetic",
    "h2_product": "horde 2 release builds: -O3 -ffp-contract=off; the shipped arithmetic (ADR-187 A1)",
    "h2_fma_control": "the deliberate must-fail controls: -ffp-contract=fast. The ONLY targets allowed to contract",
    "tools_oracles": "host-side oracles, probes and benches built with the legacy contraction default (-O2/-O3)",
}


# ---- the whole extraction ------------------------------------------------------------

def extract_all(files):
    targets, global_, dyn = extract_cmake(files.get("CMakeLists.txt", ""))
    entry, direct = {}, {}
    for rel, text in files.items():
        if rel.endswith(".py"):
            strs = python_strings(text)
            cfg = [f"{k}={v}" for s in strs for k, v in BUILD_CFG.findall(s)]
            flags = [s for s in strs if FLAG.match(s)]
            if flags and "subprocess" in text:
                direct[rel] = list(dict.fromkeys(flags))
        elif rel != "CMakeLists.txt":
            body = strip_hash_comments(text)
            cfg = [f"{k}={v}" for k, v in BUILD_CFG.findall(body)]
            flags = [t for m in SHELL_CC.finditer(body) for t in m.group(1).split() if FLAG.match(t)]
            if flags:
                direct[rel] = list(dict.fromkeys(flags))
        else:
            cfg = []
        if cfg:
            entry[rel] = list(dict.fromkeys(cfg))
    return {"global": global_, "dynamic_loops": dyn, "entry_points": entry, "direct_compiles": direct,
            "targets": {t: v for t, v in sorted(targets.items())}}


def pin_targets(pin):
    return {t: v for c in pin["classes"].values() for t, v in c["targets"].items()}


def to_pin(tree, approved=None):
    classes = {c: {"what": doc, "targets": {}} for c, doc in CLASS_DOC.items()}
    for t, v in tree["targets"].items():
        classes[classify(t, v)]["targets"][t] = v
    return {"schema": 1,
            "global": tree["global"], "dynamic_loops": tree["dynamic_loops"],
            "entry_points": tree["entry_points"], "direct_compiles": tree["direct_compiles"],
            "classes": classes, "approved": approved or {}}


def compare(pin, tree):
    """-> failures. Pure: pin dict vs extraction dict."""
    fails = []
    for sec in ("global", "dynamic_loops", "entry_points", "direct_compiles"):
        if pin[sec] != tree[sec]:
            a, b = pin[sec], tree[sec]
            if isinstance(a, dict) and isinstance(b, dict):
                for k in sorted(set(a) | set(b)):
                    if a.get(k) != b.get(k):
                        fails.append(f"{sec}: {k!r} pinned {a.get(k, '<absent>')!r}, tree {b.get(k, '<absent>')!r}")
            else:
                fails.append(f"{sec}: pinned {json.dumps(a)[:300]}\n      tree   {json.dumps(b)[:300]}")
    pt, tt = pin_targets(pin), tree["targets"]
    for t in sorted(set(pt) | set(tt)):
        if t not in tt:
            fails.append(f"target {t}: pinned but REMOVED from the tree")
        elif t not in pt:
            fails.append(f"target {t}: NEW and unpinned (flags {json.dumps(tt[t])})")
        elif pt[t] != tt[t]:
            fails.append(f"target {t}: pinned {json.dumps(pt[t])}\n      tree   {json.dumps(tt[t])}")
        else:
            cls = next(c for c, d in pin["classes"].items() if t in d["targets"])
            if classify(t, tt[t]) != cls:
                fails.append(f"target {t}: pinned in class {cls}, its flags now put it in {classify(t, tt[t])}")
    for t, v in tt.items():
        if any(CONTRACT_ON.search(f) for x in v for f in x["compile"]) and not t.endswith("_fma_control"):
            fails.append(f"target {t}: -ffp-contract=fast/on is allowed only on a *_fma_control target")
    return fails


def approve(pin, tree, name, ref, fastmath):
    """A human-approved re-pin of ONE item to the tree. -> new pin or ValueError."""
    if not (ref or "").strip():
        raise ValueError("--approve needs a non-empty <ref> (the ADR or ROADMAP row that approves it)")
    if fastmath:
        raise ValueError("refusing to re-pin while a fast-math flag is present:\n  " + "\n  ".join(fastmath))
    keep = copy.deepcopy(pin)
    if name in ("global", "dynamic_loops", "entry_points", "direct_compiles"):
        if pin[name] == tree[name]:
            raise ValueError(f"{name} already matches the tree; nothing to approve")
        keep[name] = tree[name]
    else:
        pt, tt = pin_targets(pin), tree["targets"]
        if pt.get(name) == tt.get(name):
            raise ValueError(f"target {name} already matches the tree (or exists in neither); nothing to approve")
        for c in keep["classes"].values():
            c["targets"].pop(name, None)
        if name in tt:
            keep["classes"][classify(name, tt[name])]["targets"][name] = tt[name]
    keep["approved"].setdefault(name, []).append(ref)
    return keep


def dump_pin(obj, indent=0):
    """JSON that reviews as a diff: anything that fits on one line stays on one line (a target is
    one line), larger dicts/lists expand. Keys sorted, so the file is deterministic."""
    flat = json.dumps(obj, ensure_ascii=False, sort_keys=True)
    if len(flat) + indent <= 118 or not isinstance(obj, (dict, list)):
        return flat
    pad = " " * (indent + 1)
    if isinstance(obj, dict):
        items = [f'{pad}{json.dumps(k)}: {dump_pin(v, indent + 1)}' for k, v in sorted(obj.items())]
        return "{\n" + ",\n".join(items) + "\n" + " " * indent + "}"
    return "[\n" + ",\n".join(pad + dump_pin(v, indent + 1) for v in obj) + "\n" + " " * indent + "]"


# ---- the configured tree (full) -------------------------------------------------------

def read_flags_make(build):
    """-> {target: [CXX_FLAGS tokens]} from <build>/CMakeFiles/<target>.dir/flags.make. Only the
    flags line is read: the include lines hold this machine's paths and are never printed."""
    out = {}
    for fm in sorted(pathlib.Path(build).glob("CMakeFiles/*.dir/flags.make")):
        m = re.search(r"^CXX_FLAGS = (.*)$", fm.read_text(errors="replace"), re.M)
        if m:
            out[fm.parent.name[:-4]] = m.group(1).split()
    return out


def check_built(pin, build):
    """The effective flags CMake actually emitted: every pinned flag present, no fast-math family
    token in ANY flags.make or link.txt (clap-wrapper's targets included), contraction only on the
    controls. -> (failures, summary lines)."""
    fails, summary = [], []
    root = pathlib.Path(build)
    if not (root / "CMakeFiles").is_dir():
        return [f"{build}: no CMakeFiles/ -- not a configured build tree"], []
    for f in sorted(list(root.glob("CMakeFiles/**/flags.make")) + list(root.glob("CMakeFiles/**/link.txt"))):
        for m in FASTMATH.finditer(f.read_text(errors="replace")):
            fails.append(f"{f.relative_to(root).as_posix()}: fast-math family flag {m.group(1)!r} in the emitted flags")
    flags = read_flags_make(build)
    checked = skipped = 0
    for t, variants in pin_targets(pin).items():
        if t not in flags:
            skipped += 1
            continue
        for v in variants:
            if "CMAKE_SYSTEM_PROCESSOR" in v["when"] or "MSVC" in v["when"] and "NOT MSVC" not in v["when"]:
                continue                                # a platform branch this host does not take
            missing = [f for f in v["compile"] if f not in flags[t]]
            if missing:
                fails.append(f"target {t}: pinned flag(s) {missing} are not in the emitted CXX_FLAGS {flags[t]}")
        if t.endswith("_fma_control"):
            if "-ffp-contract=fast" not in flags[t]:
                fails.append(f"target {t}: the FMA control is not contracting in the emitted flags")
        elif any(CONTRACT_ON.search(x) for x in flags[t]):
            fails.append(f"target {t}: emitted flags contract (-ffp-contract=fast/on) outside a *_fma_control target")
        checked += 1
    if "HYPERSAW-impl" in flags:
        summary.append("emitted legacy plugin CXX_FLAGS: " + " ".join(flags["HYPERSAW-impl"]))
    for cls in ("h2_parity", "h2_product", "tools_oracles"):
        ts = [t for t in pin["classes"][cls]["targets"] if t in flags]
        levels = sorted({[x for x in flags[t] if re.fullmatch(r"-O[0-3sz]?", x)][-1] for t in ts
                         if any(re.fullmatch(r"-O[0-3sz]?", x) for x in flags[t])})
        summary.append(f"emitted {cls}: {len(ts)} targets, effective -O {levels} (the last -O wins)")
    summary.append(f"emitted flags: {checked} pinned targets confirmed, {skipped} not built in this tree")
    return fails, summary


# ---- self-calibration --------------------------------------------------------------------

def selftest(files):
    """Plant each fault on a COPY of the real sources; every one must be refused. -> (problems, n)."""
    bad, n = [], [0]
    pin = to_pin(extract_all(files))

    def edit(path, fn):
        f = dict(files)
        f[path] = fn(f[path])
        return f

    def red(label, f, needle, pin_=None):
        n[0] += 1
        fails = compare(pin_ or pin, extract_all(f)) + scan_fastmath(f)
        if not any(needle in x for x in fails):
            bad.append(f"control '{label}' was NOT refused (wanted {needle!r}; got {fails[:2] or 'green'})")

    n[0] += 1
    if compare(pin, extract_all(files)) or scan_fastmath(files) or len(pin_targets(pin)) < 50:
        bad.append("control 'untouched sources' read RED or empty")
    cm = lambda fn: edit("CMakeLists.txt", fn)
    red("planted -ffast-math (CMake)", cm(lambda t: t + "\ntarget_compile_options(sr_check PRIVATE -ffast-math)\n"),
        "fast-math")
    red("planted -ffast-math (verify)", edit("verify", lambda t: t + "\nc++ -O2 -ffast-math x.cpp\n"), "fast-math")
    red("planted -ffast-math (workflow)", edit(".github/workflows/ci.yml", lambda t: t + "\n  run: g++ -Ofast x.cpp\n"),
        "fast-math")
    red("planted -ffast-math (python string)", edit("tools/embedded_page_policy_check.py",
        lambda t: t + '\nFLAGS = ["-ffast-math"]\n'), "fast-math")
    for tok in ("-Ofast", "/fp:fast", "-funsafe-math-optimizations", "-ffinite-math-only", "-fno-math-errno",
                "-fassociative-math", "-freciprocal-math", "-fno-signed-zeros", "-fno-trapping-math",
                "-ffp-model=fast"):
        red(f"planted {tok}", cm(lambda t, tok=tok: t + f"\nadd_compile_options({tok})\n"), "fast-math")
    for label, f in (
            ("comment naming -ffast-math", cm(lambda t: t + "\n# we never use -ffast-math here\n")),
            ("the safe -fno-finite-math-only", cm(lambda t: t + "\n# target_compile_options(x PRIVATE -fno-finite-math-only)\n")),
            ("python docstring naming it", edit("tools/embedded_page_policy_check.py",
                lambda t: t + '\ndef _d():\n    """never -ffast-math"""\n'))):
        n[0] += 1
        if scan_fastmath(f):
            bad.append(f"control '{label}' (must read ZERO) tripped the fast-math scan")
    n[0] += 1
    if not FASTMATH.search("x -ffast-math") or FASTMATH.search("x -fno-finite-math-only"):
        bad.append("FASTMATH pattern: -ffast-math must match and -fno-finite-math-only must not")
    red("changed -O level", cm(lambda t: t.replace("target_compile_options(measure_h2_engine PRIVATE -O3",
                                                   "target_compile_options(measure_h2_engine PRIVATE -O2", 1)),
        "target measure_h2_engine")
    red("changed -ffp-contract", cm(lambda t: t.replace("h2_swarm_parity_check PRIVATE -O2 -ffp-contract=off",
                                                        "h2_swarm_parity_check PRIVATE -O2 -ffp-contract=fast", 1)),
        "target h2_swarm_parity_check")
    red("contraction on outside a control", cm(lambda t: t + "\ntarget_compile_options(sr_check PRIVATE -ffp-contract=fast)\n"),
        "allowed only on a *_fma_control")
    red("removed target", cm(lambda t: "\n".join(l for l in t.splitlines() if "h2_swarm48_check" not in l)),
        "target h2_swarm48_check: pinned but REMOVED")
    red("new unpinned target", cm(lambda t: t + "\nadd_executable(zz_new tools/zz.cpp)\n"
                                    "target_compile_options(zz_new PRIVATE -O2)\n"), "target zz_new: NEW")
    red("new global flag setter", cm(lambda t: t + "\nadd_compile_options(-march=native)\n"), "global")
    red("changed C++ standard", cm(lambda t: t.replace("CMAKE_CXX_STANDARD 20", "CMAKE_CXX_STANDARD 23", 1)),
        "CMAKE_CXX_STANDARD")
    red("new architectures setting", cm(lambda t: t + '\nset(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")\n'),
        "CMAKE_OSX_ARCHITECTURES")
    red("changed deployment target", cm(lambda t: t.replace('"11.0"', '"10.13"', 1)), "CMAKE_OSX_DEPLOYMENT_TARGET")
    red("changed build type of verify", edit("verify", lambda t: t.replace("-DCMAKE_BUILD_TYPE=Release",
                                                                             "-DCMAKE_BUILD_TYPE=Debug", 1)),
        "entry_points")
    red("sanitizer flags changed", cm(lambda t: t.replace("-fsanitize=${HYPERSAW_SANITIZE} -fno-omit-frame-pointer -g",
                                                          "-fsanitize=${HYPERSAW_SANITIZE} -g", 1)), "dynamic_loops")
    tree = extract_all(cm(lambda t: t.replace("measure_h2_engine PRIVATE -O3", "measure_h2_engine PRIVATE -O2", 1)))
    for ref in ("", "  ", None):
        n[0] += 1
        try:
            approve(pin, tree, "measure_h2_engine", ref, [])
            bad.append(f"control 'approve with ref {ref!r}' was NOT refused")
        except ValueError:
            pass
    n[0] += 1
    try:
        approve(pin, tree, "measure_h2_engine", "ADR-test", ["f: fast-math"])
        bad.append("control 'approve while fast-math is present' was NOT refused")
    except ValueError:
        pass
    n[0] += 1
    ok = approve(pin, tree, "measure_h2_engine", "ADR-test", [])
    if compare(ok, tree) or ok["approved"].get("measure_h2_engine") != ["ADR-test"]:
        bad.append("control 'valid approval' did not leave the pin matching the tree with its ref recorded")
    n[0] += 1
    try:
        approve(pin, extract_all(files), "measure_h2_engine", "ADR-test", [])
        bad.append("control 'approve with nothing to approve' was NOT refused")
    except ValueError:
        pass
    return bad, n[0]


# ---- driver ------------------------------------------------------------------------------------

def summarise(pin):
    parts = []
    for cls, d in pin["classes"].items():
        levels = sorted({f for v in d["targets"].values() for x in v for f in x["compile"]
                         if re.fullmatch(r"-O[0-3sz]?|-ffp-contract=\w+", f)})
        parts.append(f"{cls} {len(d['targets'])} [{' '.join(levels) or 'no explicit flags'}]")
    return "; ".join(parts)


def load_pin():
    if not PIN.exists():
        sys.exit(f"build_flags_check: {PIN.relative_to(ROOT)} is missing -- a deleted pin is a RED, not a reset "
                 "(a first pin is made with --init)")
    return json.loads(PIN.read_text(encoding="utf-8"))


def main(argv):
    args = list(argv)
    build = None
    if "--built" in args:
        i = args.index("--built")
        build = args[i + 1] if i + 1 < len(args) else sys.exit("--built needs a build directory")
        del args[i:i + 2]
    files = read_sources()
    bad, planted = selftest(files)
    if bad:
        print("build_flags_check: SELFTEST FAILED -- the checker cannot be trusted:\n  " + "\n  ".join(bad))
        return 1
    tree, fast = extract_all(files), scan_fastmath(files)
    try:
        if args[:1] == ["--init"]:
            if PIN.exists():
                raise ValueError("the pin exists; --init only makes a first one")
            if fast:
                raise ValueError("refusing to pin while fast-math is present:\n  " + "\n  ".join(fast))
            PIN.write_text(dump_pin(to_pin(tree)) + "\n", encoding="utf-8")
            print(f"build_flags_check: wrote {PIN.relative_to(ROOT)} ({len(tree['targets'])} targets)")
            return 0
        if args[:1] == ["--approve"]:
            if len(args) != 3:
                raise ValueError("usage: --approve <target|global|dynamic_loops|entry_points|direct_compiles> <ref>")
            PIN.write_text(dump_pin(approve(load_pin(), tree, args[1], args[2], fast)) + "\n", encoding="utf-8")
            print(f"build_flags_check: re-pinned {args[1]} under {args[2]}")
            return 0
    except ValueError as e:
        print(f"build_flags_check: {e}")
        return 1
    if args:
        print(__doc__.split("MODES.")[1].split("SELF-CALIBRATING")[0])
        return 2
    pin = load_pin()
    problems = fast + compare(pin, tree)
    summary = []
    if build:
        f2, summary = check_built(pin, build)
        problems += f2
    if problems:
        print("build_flags_check: RED")
        for p in problems:
            print("  " + p)
        print("  (a human-approved change: python3 tools/build_flags_check.py --approve <target> <ref>)")
        return 1
    print(f"build_flags_check: GREEN ({len(tree['targets'])} targets match the pin: {summarise(pin)}; no fast-math "
          f"family flag in {len(files)} build files; {planted} planted faults/controls behaved)")
    for s in summary:
        print("  " + s)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
