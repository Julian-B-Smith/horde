#!/usr/bin/env python3
"""h2_rt_lint_check -- no construct in h2/ that can allocate, lock, block or throw.

WIRED: ./verify fast

ROADMAP B448 phase 2, package P16 (docs/strategy/blind-spot-armor-phase2.md §3);
risk row R1, the engine-level slice. h2/README.md rule 5 says "render() allocates
nothing, locks nothing"; until now nothing read the text to hold it there.

WHY A TEXT SCAN beside tools/h2_engine_rtsan_check.py. RealtimeSanitizer judges
the calls one build makes on the scenarios it is given. A growth call on a path
no scenario reaches, or an allocation one optimiser removes and another keeps, is
invisible to it and plain in the source. The scan is the other half: every line
of h2/, whether or not anything runs it.

WHAT IS BANNED, on code with comments and string literals removed (so prose about
a rule cannot trip it). Each class is a row of BANS below:
  growth     a growing call on a container: push_back, emplace_back, push_front,
             emplace_front, emplace, insert, resize, reserve, append, assign,
             shrink_to_fit. The scan cannot see the receiver's type, so ANY
             member call by these names is red: a fixed-size container in h2/
             names its methods otherwise, or carries a waiver.
  heap       new, delete (not `= delete`), malloc and its family, make_unique,
             make_shared, alloca.
  owning     a std type that owns heap memory: vector, deque, list, map, set,
             their unordered and multi forms, queue, stack, any, unique_ptr,
             shared_ptr, and their headers.
  string     std::string and its builders (to_string, the string streams),
             <string>, <sstream>.
  lock       std::mutex and friends: every std mutex and lock type,
             condition_variable, call_once, thread, future, promise, semaphore,
             the pthread and os_unfair_lock calls, and their headers.
  blocking   sleep_for, sleep_until, usleep, nanosleep, sleep(), sched_yield,
             this_thread::yield.
  io         iostream and stdio: cout, cerr, clog, the printf and file families,
             <iostream>, <fstream>, <cstdio>, <stdio.h>.
  throw      throw, and <stdexcept>.
  function   std::function, <functional>.

WHAT IT DOES NOT BAN, and who does: a clock or an unseeded random source
(tools/playbook_check.py rule 3 scans src/ for those; it does not scan h2/, see
the hand-back of P16); an include across the legacy boundary and the contraction
flag (tools/h2_rules_check.py); a function-local static whose first use takes a
guard lock, and anything libc does inside a call the text cannot see into
(std::strtod in the engine's setString path): those are the RealtimeSanitizer
check's, which runs them.

WAIVERS. Construction-time code may need a banned construct. It carries, ON THE
SAME LINE:

    // RT-LINT-WAIVER: <the reason, in words>

Every waiver in use is printed on every run, with its file, line, construct and
reason. A waiver with no reason is red. A waiver on a line with nothing banned is
red too (stale: the construct left and the excuse stayed).

FROZEN WAIVERS. A lifted core is its legacy source byte for byte (h2/README.md
rule 8, gated by tools/h2_lift_check.py), so it cannot carry a comment the
legacy file lacks. Its waivers live in FROZEN below instead, keyed by file and
the exact code of the line (not its number), and are printed with the others. A
FROZEN entry that matches no line is red. The list is short and adding to it is
visible in review; h2/engine/ takes no FROZEN entry (FROZEN_DIRS).

MUST-FAIL CONTROLS, every run (LIBRARY L0032), through the same scan() as the
verdict:
  PLANT   a temporary copy of h2/ with one `push_back` planted inside
          Engine::render in its engine.h: exactly that line is red, by number,
          and the unplanted copy reads the same as the tree (the plant is the
          only difference). The anchor the plant hangs on is asserted, so a plant
          that planted nothing cannot pass as a clean scan.
  CLASS   one synthetic line per ban class must fire, and lines that only look
          banned (comments, strings, `= delete`, identifiers that contain a
          banned word) must not.
  WAIVER  a waived line is listed and not red; a bare waiver, and one with no
          reason, are red.

    tools/h2_rt_lint_check.py
"""
import pathlib
import re
import shutil
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
CODE = {".h", ".hh", ".hpp", ".hxx", ".inc", ".ipp", ".tpp", ".c", ".cc", ".cpp", ".cxx", ".m", ".mm"}
DATA = {".md", ".json", ".txt"}   # anything else under h2/ is red: decide whether it is code
ANCHOR = "engine/engine.h"        # must be among the scanned files, or the scan is looking at nothing
WAIVER = re.compile(r"//.*?RT-LINT-WAIVER:(.*)$")

_GROW = r"push_back|emplace_back|push_front|emplace_front|emplace|insert|resize|reserve|append|assign|shrink_to_fit"
_OWN = (r"vector|deque|list|forward_list|map|multimap|set|multiset|unordered_map|unordered_multimap|unordered_set"
        r"|unordered_multiset|queue|priority_queue|stack|any|unique_ptr|shared_ptr|weak_ptr")
_LOCK = (r"mutex|timed_mutex|recursive_mutex|recursive_timed_mutex|shared_mutex|shared_timed_mutex|lock_guard|unique_lock"
         r"|shared_lock|scoped_lock|condition_variable|condition_variable_any|call_once|once_flag|thread|jthread|future"
         r"|shared_future|promise|async|counting_semaphore|binary_semaphore|latch|barrier")


def _inc(names):
    return r"#\s*include\s*[<\"](?:" + names + r")[>\"]"


# (class, why, pattern). Each token is specific enough not to occur by accident:
# `renewal` is not `new`, `insertBlade(` is not `.insert(`.
BANS = [
    ("growth", "a growing call on a container can allocate",
     re.compile(r"(?:\.|->)\s*(?:" + _GROW + r")\s*\(")),
    ("heap", "heap allocation or release",
     re.compile(r"\bnew\b|(?<![=\s])\s*\bdelete\b|^\s*delete\b|\b(?:malloc|calloc|realloc|reallocf|valloc|aligned_alloc|posix_memalign|strdup|alloca)\s*\("
                r"|\bfree\s*\(|\bmake_(?:unique|shared)\b|" + _inc("new|memory"))),
    ("owning", "a std type that owns heap memory",
     re.compile(r"\bstd\s*::\s*(?:" + _OWN + r")\b|" + _inc(_OWN.replace("unique_ptr|shared_ptr|weak_ptr", "memory_resource")))),
    ("string", "std::string and its builders allocate",
     re.compile(r"\bstd\s*::\s*(?:w?string|basic_string|to_w?string|[io]?stringstream|sto(?:i|l|ll|ul|ull|f|d|ld))\b|" + _inc("string|sstream"))),
    ("lock", "a lock, a thread or a wait",
     re.compile(r"\bstd\s*::\s*(?:" + _LOCK + r")\b|\bpthread_\w+\s*\(|\bos_unfair_lock\w*|\bdispatch_\w+\s*\("
                r"|" + _inc("mutex|shared_mutex|condition_variable|thread|future|semaphore|latch|barrier|stop_token|pthread\\.h"))),
    ("blocking", "a blocking call",
     re.compile(r"\bsleep_(?:for|until)\b|\b(?:usleep|nanosleep|sleep|sched_yield)\s*\(|\bthis_thread\s*::\s*yield\b")),
    ("io", "stream or file I/O",
     re.compile(r"\bstd\s*::\s*(?:cout|cerr|clog|cin|endl|[io]?fstream|ostream|istream)\b"
                r"|\b(?:std\s*::\s*)?(?:f|s|sn|vf|vs|vsn|v)?printf\s*\(|\b(?:std\s*::\s*)?(?:fopen|fclose|fread|fwrite|fputs|fputc|fgets|fgetc|fflush|puts|putchar|perror)\s*\("
                r"|" + _inc("iostream|fstream|ostream|istream|iomanip|cstdio|stdio\\.h"))),
    ("throw", "an exception",
     re.compile(r"\bthrow\b|" + _inc("stdexcept"))),
    ("function", "std::function type-erases onto the heap",
     re.compile(r"\bstd\s*::\s*function\b|" + _inc("functional"))),
]

# Directories under h2/ that are test tooling and never run on the audio thread. They are
# not scanned, and each is printed on every run so the exemption stays visible. The engine
# harness (B448 P2) renders offline through the engine's public interface; its render tool
# reads and writes files by design.
OFF_AUDIO_PATH = {"harness/": "the engine harness: an offline test tool, never on the audio thread (B448 P2)"}

# (file under h2/, the line's code with runs of whitespace collapsed) -> reason.
FROZEN_DIRS = ("cores/swarm/",)   # only a byte-frozen lifted core may take a FROZEN entry
_LIFTED = ("lifted from src/ byte for byte (h2/README.md rule 8), so it cannot carry a comment; a test reference since "
           "B385, not the engine; ")
FROZEN = {
    ("cores/swarm/swarm_core.h", "#include <string>"): _LIFTED + "the header for the three parameter-by-name methods below",
    ("cores/swarm/swarm_core.h", "double getParam(const std::string &k) const"): _LIFTED + "a by-name parameter read, control side",
    ("cores/swarm/swarm_core.h", "bool setParam(const std::string &k, double v)"): _LIFTED + "a by-name parameter write, control side",
    ("cores/swarm/swarm_core.h", "double *paramSlot(const std::string &k)"): _LIFTED + "the name lookup behind the two above",
}


def strip(text):
    """Comments and string/char literals become spaces; newlines stay, so line numbers hold."""
    out, i, n = [], 0, len(text)
    while i < n:
        c, d = text[i], text[i:i + 2]
        if d == "//":
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif d == "/*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(ch if ch == "\n" else " " for ch in text[i:j]))
            i = j
        elif c in "\"'":
            # a digit separator (1'000) is not a char literal
            if c == "'" and i > 0 and text[i - 1].isalnum() and i + 1 < n and text[i + 1].isalnum():
                out.append(c)
                i += 1
                continue
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            # an #include "path" keeps its text: the include bans read it
            keep = c == '"' and re.match(r"\s*#\s*include\s*$", text[text.rfind("\n", 0, i) + 1:i])
            out.append(text[i:j] if keep else c + " " * (j - i - 2) + (c if j - i >= 2 else ""))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def scan(tree):
    """tree: a directory laid out like h2/. -> (reds, waivers, nfiles).
    reds: [(file, line, class, construct, message)]; waivers: [(file, line, class, construct, reason)]."""
    reds, waivers, nfiles, frozen_hit = [], [], 0, set()
    files = sorted(p for p in tree.rglob("*") if p.is_file())
    for path in files:
        rel = path.relative_to(tree).as_posix()
        if rel.startswith(tuple(OFF_AUDIO_PATH)):
            continue
        if path.suffix not in CODE:
            if path.suffix not in DATA:
                reds.append((rel, 0, "scope", path.suffix or path.name, "a file kind this scan does not know: decide whether it is code"))
            continue
        nfiles += 1
        raw = path.read_text(encoding="utf-8", errors="replace").split("\n")
        code = strip("\n".join(raw)).split("\n")
        for n, (line, src) in enumerate(zip(code, raw), 1):
            hits = [(cls, why, m.group(0).strip()) for cls, why, rx in BANS for m in [rx.search(line)] if m]
            w = WAIVER.search(src)
            reason = w.group(1).strip() if w else None
            key = (rel, " ".join(line.split()))
            if w and not hits:
                reds.append((rel, n, "waiver", "RT-LINT-WAIVER", "a waiver on a line with nothing banned (stale)"))
            if w and hits and not reason:
                reds.append((rel, n, "waiver", "RT-LINT-WAIVER", "a waiver with no reason"))
            for cls, why, tok in hits:
                if reason:
                    waivers.append((rel, n, cls, tok, reason))
                elif key in FROZEN and rel.startswith(FROZEN_DIRS):
                    frozen_hit.add(key)
                    waivers.append((rel, n, cls, tok, "FROZEN: " + FROZEN[key]))
                elif not w:
                    reds.append((rel, n, cls, tok, why))
    if not any(p.relative_to(tree).as_posix() == ANCHOR for p in files):
        reds.append((ANCHOR, 0, "scope", "absent", "the engine's own file was not scanned: the scan is looking at the wrong tree"))
    for key in FROZEN:
        if not key[0].startswith(FROZEN_DIRS):
            reds.append((key[0], 0, "waiver", "FROZEN", "a FROZEN entry outside the byte-frozen lifted cores"))
        elif key not in frozen_hit and (tree / key[0]).exists():
            reds.append((key[0], 0, "waiver", "FROZEN", f"a FROZEN entry that matches no line (stale): `{key[1]}`"))
    return reds, waivers, nfiles


def scan_text(text, rel="engine/engine.h"):
    """scan() over one synthetic file, for the controls."""
    with tempfile.TemporaryDirectory() as d:
        p = pathlib.Path(d) / rel
        p.parent.mkdir(parents=True)
        p.write_text(text, encoding="utf-8")
        reds, waivers, _ = scan(pathlib.Path(d))
    return reds, waivers


# One line per ban class that MUST fire, with the construct it must name.
MUST_FIRE = [
    ("growth", "  void f() { scratch.push_back(x); }", "push_back"),
    ("growth", "  void f() { p->resize(n); }", "resize"),
    ("growth", "  void f() { m.insert({k, v}); }", "insert"),
    ("heap", "  double* b = new double[n];", "new"),
    ("heap", "  void f() { delete[] b; }", "delete"),
    ("heap", "  void* p = std::malloc(64);", "malloc"),
    ("heap", "  auto p = std::make_unique<Voice>();", "make_unique"),
    ("owning", "  std::vector<double> scratch;", "vector"),
    ("owning", "#include <vector>", "vector"),
    ("string", "  std::string name;", "string"),
    ("string", "  auto s = std::to_string(n);", "to_string"),
    ("lock", "  std::mutex m;", "mutex"),
    ("lock", "  std::lock_guard<std::mutex> g(m);", "lock_guard"),
    ("lock", "  pthread_mutex_lock(&m);", "pthread_mutex_lock"),
    ("blocking", "  std::this_thread::sleep_for(d);", "sleep_for"),
    ("blocking", "  usleep(10);", "usleep"),
    ("io", "  std::cout << x;", "cout"),
    ("io", "  std::printf(\"%d\", n);", "printf"),
    ("io", "#include <iostream>", "iostream"),
    ("throw", "  if (bad) throw 1;", "throw"),
    ("function", "  std::function<void()> cb;", "function"),
]
# Lines that only LOOK banned. Each must read clean.
MUST_PASS = [
    "  // a new voice: push_back would allocate, std::mutex would lock, throw would unwind",
    "  /* v.resize(n); new; malloc(1); std::string */",
    "  const char* why = \"new std::vector<int> via push_back(\";",
    "  Engine(const Engine&) = delete;",
    "  Engine& operator=(const Engine&) =delete;",
    "  double renewal = news + deleted;   int newest = 1'000;",
    "  void insertBlade(int i); void reserveVoice(); bool resized = false;",
    "  void f() { assignVoice(v); appendix(); throwaway = 1; freeSlot(i); sleepy(); }",
    "  double x = std::sin(a) + std::floor(b); std::memset(p, 0, n);",
    "#include <cmath>",
    "#include \"swarm.h\"",
]
PLANT_ANCHOR = "  void render(double* L, double* R, int n) {\n"
PLANT_LINE = "    scratchPlant.push_back(static_cast<double>(n));\n"


def selftest():
    """-> failures. Every control goes through scan(), the code that judges."""
    bad = []
    for cls, line, tok in MUST_FIRE:
        reds, _ = scan_text(line + "\n")
        if not any(r[2] == cls and tok in r[3] and r[1] == 1 for r in reds):
            bad.append(f"CLASS {cls}: `{line.strip()}` did not fire as {tok} (read {[(r[2], r[3]) for r in reds]})")
    for line in MUST_PASS:
        reds, _ = scan_text(line + "\n")
        if reds:
            bad.append(f"CLASS: the clean line `{line.strip()}` fired as {[(r[2], r[3]) for r in reds]}")

    # WAIVER: listed and not red with a reason; red when bare or reasonless.
    grow = "  std::vector<double> table;"
    reds, waivers = scan_text(grow + "   // RT-LINT-WAIVER: built once in the constructor\n")
    if reds or [(w[2], w[4]) for w in waivers] != [("owning", "built once in the constructor")]:
        bad.append(f"WAIVER: a waived line read red {reds} or was not listed {waivers}")
    reds, _ = scan_text(grow + "   // RT-LINT-WAIVER:\n")
    if not any(r[4] == "a waiver with no reason" for r in reds):
        bad.append("WAIVER: a waiver with no reason was accepted")
    reds, _ = scan_text("  double x = 1;   // RT-LINT-WAIVER: nothing here needs one\n")
    if not any("stale" in r[4] for r in reds):
        bad.append("WAIVER: a waiver on a clean line was accepted")
    # FROZEN: honoured only in the lifted core's own file, and stale when its line is gone.
    frozen_line = "  double getParam(const std::string &k) const\n"
    reds, _ = scan_text(frozen_line)
    if not any(r[2] == "string" for r in reds):
        bad.append("FROZEN: a frozen line was waived in h2/engine/, where no FROZEN entry applies")

    # PLANT: a real copy of h2/, one growth call inside Engine::render.
    with tempfile.TemporaryDirectory() as d:
        copy = pathlib.Path(d) / "h2"
        shutil.copytree(ROOT / "h2", copy)
        base = scan(copy)
        if (base[0], base[1]) != scan(ROOT / "h2")[:2]:
            bad.append("PLANT: the unplanted copy of h2/ does not read the same as the tree")
        eng = copy / "engine" / "engine.h"
        text = eng.read_text(encoding="utf-8")
        if text.count(PLANT_ANCHOR) != 1:
            return bad + [f"PLANT: the anchor `{PLANT_ANCHOR.strip()}` is in engine.h {text.count(PLANT_ANCHOR)} time(s), not once: nothing was planted"]
        at = text[:text.index(PLANT_ANCHOR)].count("\n") + 2
        eng.write_text(text.replace(PLANT_ANCHOR, PLANT_ANCHOR + PLANT_LINE), encoding="utf-8")
        planted = scan(copy)
        new = [r for r in planted[0] if r not in base[0]]
        if new != [("engine/engine.h", at, "growth", ".push_back(", "a growing call on a container can allocate")]:
            bad.append(f"PLANT: a push_back planted at engine.h:{at} read {new}")
        else:
            print(f"PASS  control PLANT  a temporary copy of h2/ with `push_back` planted inside Engine::render is RED at "
                  f"engine/engine.h:{at} [growth], and the unplanted copy reads the same as the tree")
    return bad


def main():
    bad = selftest()
    if bad:
        for b in bad:
            print("FAIL  control  " + b)
        print("h2_rt_lint_check: RED — a must-fail control did not behave, so the scan below proves nothing")
        return 1
    print(f"PASS  control CLASS  {len(MUST_FIRE)} banned lines fire across {len(BANS)} classes, {len(MUST_PASS)} look-alike lines read clean")
    print("PASS  control WAIVER  a waived line is listed and not red; a bare waiver, one with no reason, and a FROZEN line in h2/engine/ are red")

    reds, waivers, nfiles = scan(ROOT / "h2")
    for d, why in sorted(OFF_AUDIO_PATH.items()):
        if (ROOT / "h2" / d).is_dir():
            print(f"EXEMPT  h2/{d}  not scanned — {why}")
    for rel, n, cls, tok, reason in waivers:
        print(f"WAIVER  h2/{rel}:{n}  [{cls}] {tok}  — {reason}")
    for rel, n, cls, tok, why in reds:
        print(f"FAIL  h2/{rel}:{n}  [{cls}] {tok}  — {why}")
    in_file = sum(1 for w in waivers if not w[4].startswith("FROZEN: "))
    print(f"h2_rt_lint_check: {'RED' if reds else 'GREEN'} — {nfiles} source files under h2/, {len(reds)} banned construct(s), "
          f"{len(waivers)} waiver(s) in use ({in_file} in-file, {len(waivers) - in_file} FROZEN)")
    return 1 if reds else 0


if __name__ == "__main__":
    sys.exit(main())
