#!/usr/bin/env python3
"""engine_words_check — nothing presents swarm and SCALPEL as engines to choose between (GUI-1).

WIRED: ./verify fast (wired in the PR that creates it, under ADR-180 §1)

WHY. horde has one unified engine: every oscillator is a swarm cut by blades. An "Engine: SWARM |
SCALPEL" selection survived in the design master's samples long after it stopped being true, and no
real parameter ever carried it, so nothing noticed. Samples get copied, which is how a false notion
comes back, so this is a gate and not a note (the human, 2026-11-01: "I don't want creeping back in").

WHAT IT REFUSES, and where.
  selector   everywhere below, labs included: an Engine label on a control, an option list naming
             both swarm and scalpel, a summary like "scalpel · 5 members", a detune law offering swarm.
  wording    the master and the shell only: "founding engine", "dynamical engines", and "SCALPEL's"
             as though it were a thing the instrument contains. Old labs may say that: they are
             history, and they describe the two laws they compared (ADR-316).

WHAT IT DELIBERATELY ALLOWS. "swarm" alone is correct: it is the oscillator's part, and Spread law and
a blade's Frame really offer it, so a ban on the word would be wrong. "The SCALPEL lab" or "bench" names
a file. The clean controls below hold those open. reference/ and specs/ are not scanned (protected).
"""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
QUOTE = r"""['"]"""
SELECTOR = [
    ("an Engine label on a control", re.compile(r"label:\s*" + QUOTE + r"Engine" + QUOTE)),
    ("options naming both swarm and scalpel", re.compile(
        r"\[[^\]\n]*" + QUOTE + r"(?:swarm" + QUOTE + r"[^\]\n]*" + QUOTE + r"scalpel|scalpel" + QUOTE + r"[^\]\n]*" + QUOTE + r"swarm)" + QUOTE, re.I)),
    ("a summary naming an oscillator's kind", re.compile(r"\b(?:swarm|scalpel)(?: \d+)? · (?:\d+ members|scalpel|swarm)", re.I)),
    ("a detune law offering swarm", re.compile(r"Detune law[^\n]*" + QUOTE + r"swarm" + QUOTE, re.I)),
]
WORDING = [
    ("the founding engine", re.compile(r"founding engine", re.I)),
    ("engines in the plural", re.compile(r"dynamical engines", re.I)),
    ("SCALPEL as a part of the instrument", re.compile(r"SCALPEL['’]s\b")),
]


def findings(text, rules):
    return [(n + 1, name) for n, line in enumerate(text.split("\n")) for name, rx in rules if rx.search(line)]


def tracked(*specs):
    out = subprocess.run(["git", "ls-files", "--", *specs], cwd=ROOT, capture_output=True, text=True, check=True).stdout.split("\n")
    return [p for p in out if p and pathlib.Path(p).suffix in {".html", ".js", ".json", ".md", ".ts", ".css"}]


def scan(path, rules):
    try:
        return findings((ROOT / path).read_text(encoding="utf-8"), rules)
    except FileNotFoundError:   # deleted in the working tree: nothing to read
        return []


# Old labs that trip a selector rule and are not offering a selection, each read and named. A pin is one file and one rule, so a NEW
# hit in the same lab under another rule is still red, and a pin nothing trips any more is red too, so this list can only shrink.
PINNED = {
    ("docs/design/scalpel-interface-lab.html", "a summary naming an oscillator's kind"): "The bench's footer, 'OSC · SWARM 1 · SCALPEL', naming the lab's own view. History; do not copy it when porting from the bench.",
    ("docs/design/fx-screens-workshop.html", "options naming both swarm and scalpel"): "RESERVED_NAMES, the names a module may not take. Not a choice.",
    ("docs/design/engine-audit.html", "a detune law offering swarm"): "One line of audit data holding the Detune law row and a Swarm section name. Not a choice.",
}

# The plants: each must be refused by its own rule. The clean lines must pass: they are true.
PLANTS = [
    ("modes: { label: 'Engine', options: ['A', 'B'], value: 1 }", SELECTOR, "an Engine label on a control"),
    ("options: ['SWARM', 'SCALPEL'], value: 1", SELECTOR, "options naming both swarm and scalpel"),
    ('options: ["scalpel", "noise", "swarm"]', SELECTOR, "options naming both swarm and scalpel"),
    ("sum: 'scalpel · 5 members', pic: 'blade'", SELECTOR, "a summary naming an oscillator's kind"),
    ("summary: 'swarm 1 · scalpel · 5 members'", SELECTOR, "a summary naming an oscillator's kind"),
    ("name: 'Detune law', options: ['LINEAR', 'EXP', 'SWARM']", SELECTOR, "a detune law offering swarm"),
    ("The founding engine is SWARM SAW;", WORDING, "the founding engine"),
    ("a synthesizer of responsive, dynamical engines whose", WORDING, "engines in the plural"),
    ("the sub, SCALPEL's cut rules and later devices", WORDING, "SCALPEL as a part of the instrument"),
]
CLEAN = [
    "options: ['gradient', 'random', 'drift', 'alternate', 'swarm']",   # Spread law, a real row
    "options: ['as blade 1', 'member', 'swarm']",                       # a blade's Frame, a real row
    "name: 'Detune law', options: ['cents', 'Hz', 'ERB', 'harmonic']",
    "{ label: 'Swarm', items: [{ knob: wide.detune }] }",
    "'Detune 28 % · Coupling 0 % · Cross-mod 0 % · SAMPLE swarm'",
    "copied precisely from the SCALPEL bench, the SCALPEL lab's phase ring",
    "kindLabel: 'ENGINE', group: 'eng', note: 'published by the engine'",
    "sum: '5 members', pic: 'blade'",
]


def main():
    bad = []
    for line, rules, want in PLANTS:
        if want not in [n for _, n in findings(line, rules)]:
            bad.append(f"plant did not fire ({want}): {line}")
    for line in CLEAN:
        if findings(line, SELECTOR + WORDING):
            bad.append(f"a true line was refused: {line}")
    selector_files = tracked("docs/design-system", "docs/design/*.html", "docs/design/gui3")
    wording_files = tracked("docs/design-system", "docs/design/gui3.html", "docs/design/gui3")
    seen = set()
    for path in selector_files:
        for n, name in scan(path, SELECTOR):
            if (path, name) in PINNED:
                seen.add((path, name))
            else:
                bad.append(f"{path}:{n}: {name} (GUI-1: one unified engine)")
    bad += [f"stale pin, nothing trips it: {p[0]}, {p[1]}" for p in PINNED if p not in seen]
    for path in wording_files:
        bad += [f"{path}:{n}: {name} (GUI-1: one unified engine)" for n, name in scan(path, WORDING)]
    if bad:
        print("engine_words_check: RED")
        print("\n".join("  " + b for b in bad))
        return 1
    print(f"engine_words_check: GREEN ({len(selector_files)} files for selectors, {len(wording_files)} for wording; {len(PLANTS)} plants fired, {len(CLEAN)} clean controls; {len(PINNED)} old-lab lines pinned)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
