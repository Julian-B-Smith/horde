#!/usr/bin/env python3
"""paste_cap_check -- one size cap on state text from the page and the clipboard,
derived from the factory bank, enforced at all three doors before allocating
(B446, critic MEDIUM-3 on PR #973).

WIRED: ./verify fast

  python3 tools/paste_cap_check.py

ROWS.
  DERIVE   kMaxPastedStateBytes (src/input_guards.h) equals 4 x the largest
           docs/presets/factory/**/*.json, rounded up to a power of two: the
           derivation its comment states. A bank that outgrows the cap is red
           here rather than a factory patch refused by PASTE.
  DOORS    each door tests the cap before it copies or grows anything:
             hzApplyState (src/gui/hypersaw_gui_common.h): `pastedStateFits`
               on the view before `std::string(json)`;
             macOS hzPasteState (src/gui/hypersaw_gui.mm): `pastedStateFits`
               before `UTF8String`, and `pasteStatus` for the rest;
             Windows clipboardTextUtf8 (src/gui/hypersaw_gui_win.cpp):
               `pastedStateFits` before `out.resize`, `ClipboardOpen` and
               `GlobalLocked` guards (CloseClipboard / GlobalUnlock on every
               path), and hzPasteState through `pasteStatus`.
           By source: the Windows half compiles only on CI.
  BEHAVIOUR tools/paste_cap_check.cpp, compiled with the host compiler and run:
           pasteStatus at the cap's edges, counting allocations (its header).

MUST-FAIL CONTROLS, every run: a cap that is not the derivation's value fails
DERIVE; a bank with a preset larger than the cap allows fails DERIVE; each door
with its cap test moved after the copy (or removed) fails DOORS; the Windows
backend without its guards fails DOORS. The .cpp carries its own two controls.

FAILS CLOSED: no compiler, a compile error, no factory presets, or a constant
that cannot be read is RED.
"""
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
GUARDS = ROOT / "src/input_guards.h"
BANK = ROOT / "docs/presets/factory"
COMMON = ROOT / "src/gui/hypersaw_gui_common.h"
MM = ROOT / "src/gui/hypersaw_gui.mm"
WIN = ROOT / "src/gui/hypersaw_gui_win.cpp"
CPP = ROOT / "tools/paste_cap_check.cpp"


def next_pow2(n):
    p = 1
    while p < n:
        p *= 2
    return p


def rule_derive(cap, largest):
    if cap is None:
        return ["src/input_guards.h: no `constexpr size_t kMaxPastedStateBytes = <n>;`"]
    if largest is None:
        return ["docs/presets/factory holds no .json preset to derive the cap from"]
    want = next_pow2(4 * largest)
    if cap != want:
        return [f"kMaxPastedStateBytes is {cap}, but 4 x the largest factory preset ({largest} bytes) "
                f"rounded up to a power of two is {want}: re-derive it and update its comment"]
    return []


def before(text, first, second, start=0):
    """True when `first` occurs at or after `start` and before the next `second`."""
    a = text.find(first, start)
    b = text.find(second, start)
    return a >= 0 and b >= 0 and a < b


def rule_doors(common, mm, win):
    errs = []
    at = common.find('web.bind("hzApplyState"')
    if at < 0 or not before(common, "pastedStateFits(json.size())", "std::string(json)", at):
        errs.append("hypersaw_gui_common.h: hzApplyState does not test pastedStateFits before std::string(json)")
    at = mm.find('w.bind("hzPasteState"')
    if at < 0 or not before(mm, "pastedStateFits(", "UTF8String", at) or "pasteStatus(" not in mm[at:]:
        errs.append("hypersaw_gui.mm: hzPasteState does not test pastedStateFits before UTF8String "
                    "and return pasteStatus")
    at = win.find("ClipText clipboardTextUtf8(")
    if at < 0 or not before(win, "pastedStateFits(", "out.resize(", at):
        errs.append("hypersaw_gui_win.cpp: clipboardTextUtf8 does not test pastedStateFits before out.resize")
    for guard, call in (("ClipboardOpen", "CloseClipboard()"), ("GlobalLocked", "GlobalUnlock(")):
        m = re.search(r"struct " + guard + r"\b.*?~" + guard + r"\(\)\s*\{[^}]*" + re.escape(call), win, re.S)
        if not m or (at >= 0 and guard + " " not in win[at:]):
            errs.append(f"hypersaw_gui_win.cpp: no {guard} guard whose destructor calls {call}, used by "
                        "clipboardTextUtf8")
    at = win.find('w.bind("hzPasteState"')
    if at < 0 or "pasteStatus(" not in win[at:at + 800]:
        errs.append("hypersaw_gui_win.cpp: hzPasteState does not return pasteStatus")
    return errs


def compiler():
    for c in (os.environ.get("CXX"), "c++", "clang++", "g++"):
        if c and shutil.which(c):
            return c
    return None


def main():
    fails = []

    def row(tag, errs):
        fails.extend(f"{tag}: {e}" for e in errs)
        print(f"  {'PASS' if not errs else 'FAIL'}  {tag}")

    def control(tag, red):
        if not red:
            fails.append(f"CONTROL {tag}: read green; the rule cannot see the fault it exists for")
        print(f"  {'PASS' if red else 'FAIL'}  control {tag}")

    print("paste_cap_check:")
    guards = GUARDS.read_text()
    m = re.search(r"constexpr size_t kMaxPastedStateBytes = (\d+);", guards)
    cap = int(m.group(1)) if m else None
    sizes = [p.stat().st_size for p in BANK.rglob("*.json")] if BANK.is_dir() else []
    largest = max(sizes) if sizes else None
    row("DERIVE", rule_derive(cap, largest))
    common, mm, win = COMMON.read_text(), MM.read_text(), WIN.read_text()
    row("DOORS", rule_doors(common, mm, win))

    cxx = compiler()
    if cxx is None:
        row("BEHAVIOUR", ["no C++ compiler (set CXX)"])
    else:
        with tempfile.TemporaryDirectory() as tmp:
            exe = pathlib.Path(tmp) / "paste_cap_check"
            b = subprocess.run([cxx, "-std=c++20", "-O1", "-Wall", "-Wextra", "-Werror", str(CPP), "-o", str(exe)],
                               capture_output=True, text=True)
            if b.returncode != 0:
                row("BEHAVIOUR", ["compile failed:\n" + b.stdout + b.stderr])
            else:
                r = subprocess.run([str(exe)], capture_output=True, text=True)
                sys.stdout.write("".join("    " + ln + "\n" for ln in r.stdout.splitlines()))
                row("BEHAVIOUR", [] if r.returncode == 0 else [f"paste_cap_check.cpp exited {r.returncode}"])

    if cap is not None and largest is not None:
        control("a cap other than the derivation fails DERIVE", bool(rule_derive(cap * 2, largest)))
        control("a preset larger than the cap allows fails DERIVE", bool(rule_derive(cap, cap)))
    else:
        fails.append("DERIVE controls could not run (no cap or no bank)")
    late_apply = common.replace("if (pastedStateFits(json.size()))", "", 1)
    control("hzApplyState without its cap test fails DOORS",
            late_apply != common and bool(rule_doors(late_apply, mm, win)))
    late_mm = re.sub(r"if \(!pastedStateFits\(s\.length\)[^\n]*\n[^\n]*\n", "", mm, count=1)
    control("macOS hzPasteState without its cap test fails DOORS", late_mm != mm and bool(rule_doors(common, late_mm, win)))
    late_win = win.replace("if (!pastedStateFits((size_t)bytes)) return ClipText::tooLarge;\n", "", 1) \
                  .replace("if (!pastedStateFits(n)) return ClipText::tooLarge;\n", "", 1)
    control("Windows clipboardTextUtf8 without its cap tests fails DOORS",
            late_win != win and bool(rule_doors(common, mm, late_win)))
    unguarded = win.replace("~ClipboardOpen() { if (open) CloseClipboard(); }", "~ClipboardOpen() {}", 1)
    control("the Windows clipboard guard that does not close fails DOORS",
            unguarded != win and bool(rule_doors(common, mm, unguarded)))

    if fails:
        print("paste_cap_check: RED", file=sys.stderr)
        for f in fails:
            print(f"    {f}", file=sys.stderr)
        return 1
    print(f"paste_cap_check: GREEN (cap {cap} = next pow2 of 4 x {largest}; 3 doors; 6 controls + 2 in the .cpp)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
