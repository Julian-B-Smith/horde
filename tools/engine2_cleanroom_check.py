#!/usr/bin/env python3
"""engine2_cleanroom_check -- the new engine's clean-room tree holds what it should and none of the old
engine (ROADMAP B497 part 1; ADR-223; spec BD-10).

WIRED: ./verify fast

WHY. BD-10 says the builder works in a tree that contains no earlier engine, no earlier engine's
tests, no prototype and no legacy plugin. Nothing but a check keeps that true: a sparse-checkout recipe
is a list of paths, and one added line (or one pasted identifier in a copied file) puts the old engine
back in front of the builder while every build stays green.

WHAT IT CHECKS.
  1. DETECTOR. engine2/tools/click_detector.mjs is byte-identical to tools/click_detector.mjs. The builder
     and the test author get a copy so they need none of the old tools; two copies drift silently
     unless this reads red.
  2. DATA. engine2/data/envelopes/presets.json is the spec's Appendix A, exactly (12 presets: name, shape,
     every listed key and value), and engine2/data/bank/presets.json holds the 83 presets of B.1 in the
     bank format.
  3. THE RECIPE'S TREE. tools/engine2_cleanroom.sh --snapshot makes a fresh sparse worktree (local, no
     network). In it: (a) every path is under engine2/ or is the spec (an allow-list: a path the
     recipe was never meant to carry is red whatever it is); (b) none is under a denied top-level
     directory (named, so the message says what came back), and none is under the test author's
     data (the reference renders and the version-0 states, which the builder does not receive,
     spec 10.1 and IF-4); (c) no file is named like the old engine's
     twin or prototypes; (d) no file's text holds one of the old engine's identifiers, except the
     spec's BD-10 line; (e) no symlink. With --build the tree is also built and its suite run from
     there, which proves it needs nothing outside itself.
  4. MUST-FAIL CONTROLS, every run: a recipe variant that adds the old engine's directory, a variant
     that adds one source file, a variant that gives the builder the reference renders, and (on the
     green tree) a planted identifier, a planted twin-named file,
     a planted file at the root and a planted symlink must each read red with their own tag; the
     untouched tree reads green; an altered detector copy, a changed envelope value and a short bank
     must each be refused.

Scratch trees are git worktrees made by the recipe script and removed afterwards, also on failure.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCRIPT = ROOT / "tools" / "engine2_cleanroom.sh"
SPEC = "specs/SPEC-ENGINE.md"

DETECTOR_ORIGINAL = "tools/click_detector.mjs"
DETECTOR_COPY = "engine2/tools/click_detector.mjs"
ENVELOPES = "engine2/data/envelopes/presets.json"
BANK = "engine2/data/bank/presets.json"
BANK_COUNT = 83   # B.1: "holding 83 presets"
ENVELOPE_COUNT = 12   # Appendix A: "Twelve presets"

# The tree may hold exactly these. engine2/ is the whole new engine; the spec is its only reference.
ALLOWED_PREFIXES = ("engine2/",)
ALLOWED_FILES = (SPEC,)

# Top-level directories that are the old engine, its tests, its prototypes, the legacy plugin or
# the records about them. Redundant with the allow-list on purpose: the allow-list says "not
# allowed", this says what came back.
DENIED_PREFIXES = (
    "h2/", "src/", "reference/", "docs/", "tools/", "tests/", "libs/", "integrations/", "briefs/",
    "audits/", "traces/", "dist/", "install/", ".github/", ".claude/", ".kit/",
)

# Inside engine2/, the test author's input data: the builder does not receive it (spec 10.1, IF-4).
TEST_AUTHOR_ONLY = ("engine2/data/reference-renders/", "engine2/data/version0-states/")

# File names that mark the old engine's twin and prototypes, tested on each path component's name.
OLD_NAME = re.compile(
    r"swarm(saw|spectra|dynamics|filter|phaser|time|alator)|hypersaw|prototype|_core\.h$|(^|[-_.])twin([-_.]|$)",
    re.IGNORECASE)

# The old engine's identifiers. Case-insensitive; the spec's BD-10 line is the one place allowed to
# speak about an earlier engine at all.
OLD_IDENTIFIERS = re.compile(rb"razor|swarmsynth|composed|h2/engine|scalpel-horde-engine", re.IGNORECASE)
SPEC_EXEMPT_LINE = b"BD-10"


# ---- 1. the detector copy ---------------------------------------------------------------------

def detector_copies_identical(original: bytes, copy: bytes) -> bool:
    return original == copy


# ---- 2. the data ------------------------------------------------------------------------------

def parse_appendix_a(spec_text: str):
    """The envelope presets of Appendix A, as [{name, shape, params}] in table order."""
    section = spec_text.split("## Appendix A.", 1)[1].split("## Appendix B.", 1)[0]
    rows = []
    for line in section.splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if not line.startswith("|") or len(cells) != 3 or cells[0] in ("name", "---") or set(cells[0]) <= {"-"}:
            continue
        params = {}
        for pair in cells[2].split(", "):
            key, value = pair.rsplit(" ", 1)
            number = float(value)
            params[key] = int(number) if number == int(number) else number
        rows.append({"name": cells[0], "shape": cells[1], "params": params})
    return rows


# Read off Appendix A by hand, so the parser is not the only witness of what the table says
# (a parser that mis-reads the table would otherwise agree with a file generated by it).
HAND_READ_ANCHORS = {
    "Long tail · five seconds": ("tail", {"c": 0.875, "R": 5000, "S": 0.6}),
    "Legato swell lead": ("swell", {"polyMode": 2, "glide": 150, "I": 1.2, "R": 800}),
}


def envelope_file_problems(spec_text: str, document: dict):
    expected = parse_appendix_a(spec_text)
    problems = []
    for name, (shape, values) in HAND_READ_ANCHORS.items():
        row = next((r for r in expected if r["name"] == name), None)
        if row is None or row["shape"] != shape or any(row["params"].get(k) != v for k, v in values.items()):
            problems.append(f"the parser's reading of {name!r} disagrees with the hand-read anchor {shape} {values}")
    table_pairs = sum(line.rsplit("|", 2)[1].count(",") + 1 for line in
                      spec_text.split("## Appendix A.", 1)[1].split("## Appendix B.", 1)[0].splitlines()
                      if line.startswith("| ") and not line.startswith(("| name", "|---")))
    if table_pairs != sum(len(r["params"]) for r in expected):
        problems.append(f"the table has {table_pairs} key-value pairs, the parser kept {sum(len(r['params']) for r in expected)}")
    if len(expected) != ENVELOPE_COUNT:
        problems.append(f"Appendix A parsed to {len(expected)} presets, not {ENVELOPE_COUNT} (did the table change shape?)")
    got = document.get("presets", [])
    if len(got) != len(expected):
        problems.append(f"{ENVELOPES} holds {len(got)} presets, Appendix A has {len(expected)}")
    for want, have in zip(expected, got):
        mine = {k: have.get(k) for k in ("name", "shape", "params")}
        if mine != want:
            problems.append(f"preset {want['name']!r} differs from Appendix A: file {mine} vs spec {want}")
    return problems


def bank_file_problems(document: dict):
    presets = document.get("presets", [])
    problems = []
    if len(presets) != BANK_COUNT:
        problems.append(f"{BANK} holds {len(presets)} presets, B.1 says {BANK_COUNT}")
    for index, preset in enumerate(presets):
        if set(preset) != {"category", "name", "params"} or not isinstance(preset["params"], dict):
            problems.append(f"{BANK} preset {index} is not {{category, name, params}}")
    return problems


# ---- 3. the recipe's tree ---------------------------------------------------------------------

def audit_tree(tree: Path):
    """-> [(tag, message)] for everything in the tree that the clean room forbids."""
    problems = []
    for here, dirs, files in os.walk(tree):
        dirs[:] = sorted(d for d in dirs if d != ".git")
        # os.walk lists a symlink to a directory among dirs and does not follow it.
        for name in dirs:
            if (Path(here) / name).is_symlink():
                problems.append(("symlink", f"{(Path(here) / name).relative_to(tree).as_posix()}: a symlink can point out of the tree"))
        for name in sorted(files):
            if name == ".git":
                continue
            path = Path(here) / name
            rel = path.relative_to(tree).as_posix()
            if path.is_symlink():
                problems.append(("symlink", f"{rel}: a symlink can point out of the tree"))
                continue
            if not (rel.startswith(ALLOWED_PREFIXES) or rel in ALLOWED_FILES):
                problems.append(("outside-recipe", f"{rel}: not under engine2/ and not the spec"))
            for prefix in DENIED_PREFIXES:
                if rel.startswith(prefix):
                    problems.append(("denied-directory", f"{rel}: under {prefix}, which is old-engine territory"))
            for prefix in TEST_AUTHOR_ONLY:
                if rel.startswith(prefix):
                    problems.append(("test-author-only", f"{rel}: under {prefix}, which the builder does not receive"))
            for part in Path(rel).parts:
                if OLD_NAME.search(part):
                    problems.append(("old-name", f"{rel}: {part!r} is named like the old engine's twin or prototypes"))
                    break
            data = path.read_bytes()
            for number, line in enumerate(data.split(b"\n"), 1):
                if rel == SPEC and SPEC_EXEMPT_LINE in line:
                    continue
                hit = OLD_IDENTIFIERS.search(line)
                if hit:
                    problems.append(("identifier", f"{rel}:{number}: {hit.group(0).decode()!r}, an old-engine identifier"))
    return problems


class RecipeTree:
    """A fresh sparse worktree from the recipe script; removed on exit, success or not."""

    def __init__(self, also=()):
        self.also = list(also)
        self.parent = Path(tempfile.mkdtemp(prefix="engine2-cleanroom-"))
        self.path = self.parent / "tree"

    def __enter__(self):
        cmd = ["bash", str(SCRIPT), str(self.path), "--snapshot"]
        for pattern in self.also:
            cmd += ["--also", pattern]
        done = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
        if done.returncode != 0:
            self.__exit__()
            raise RuntimeError(f"recipe script failed ({done.returncode}):\n{done.stdout}{done.stderr}")
        return self.path

    def __exit__(self, *exc):
        subprocess.run(["git", "worktree", "remove", "--force", str(self.path)], cwd=ROOT, capture_output=True)
        shutil.rmtree(self.parent, ignore_errors=True)
        subprocess.run(["git", "worktree", "prune"], cwd=ROOT, capture_output=True)
        return False


def build_in_tree(tree: Path):
    build = tree.parent / "build"
    done = subprocess.run(["bash", str(tree / "engine2" / "build.sh"), str(build)], capture_output=True, text=True)
    return done.returncode, done.stdout + done.stderr


# ---- the run ----------------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", action="store_true", help="also build the recipe's tree and run its suite there")
    args = parser.parse_args()

    failures = []

    def expect(label, ok, detail=""):
        print(f"engine2_cleanroom_check: {'ok  ' if ok else 'FAIL'} {label}" + ("" if ok else f"\n    {detail}"))
        if not ok:
            failures.append(label)

    spec_text = (ROOT / SPEC).read_text(encoding="utf-8")

    # 1. detector
    original = (ROOT / DETECTOR_ORIGINAL).read_bytes()
    copy = (ROOT / DETECTOR_COPY).read_bytes()
    expect("detector copy is byte-identical", detector_copies_identical(original, copy),
           f"{DETECTOR_COPY} differs from {DETECTOR_ORIGINAL}; copy the original over it")
    expect("control: an altered detector copy is refused", not detector_copies_identical(original, copy + b"\n"))

    # 2. data
    envelopes = json.loads((ROOT / ENVELOPES).read_text(encoding="utf-8"))
    problems = envelope_file_problems(spec_text, envelopes)
    expect("envelope presets equal Appendix A", not problems, "\n    ".join(problems))
    planted = json.loads(json.dumps(envelopes))
    first_key = next(iter(planted["presets"][0]["params"]))
    planted["presets"][0]["params"][first_key] += 1
    expect("control: a changed envelope value is refused", bool(envelope_file_problems(spec_text, planted)))
    bank = json.loads((ROOT / BANK).read_text(encoding="utf-8"))
    problems = bank_file_problems(bank)
    expect("bank holds the 83 presets of B.1", not problems, "\n    ".join(problems))
    short = {"presets": bank["presets"][:-1]}
    expect("control: a short bank is refused", bool(bank_file_problems(short)))

    # 3 and 4. the recipe's tree and its controls
    try:
        with RecipeTree() as tree:
            problems = audit_tree(tree)
            expect("recipe tree passes the no-old-code check", not problems,
                   "\n    ".join(f"[{t}] {m}" for t, m in problems))
            files = sorted(p.relative_to(tree).as_posix() for p in tree.rglob("*") if p.is_file() and p.name != ".git")
            expect("recipe tree is not empty and holds the spec", SPEC in files and any(f.startswith("engine2/") for f in files),
                   f"files: {files[:10]}")

            def plant(tag, relpath, content=b"x\n", link_to=None):
                target = tree / relpath
                target.parent.mkdir(parents=True, exist_ok=True)
                if link_to:
                    target.symlink_to(link_to)
                else:
                    target.write_bytes(content)
                tags = {t for t, _ in audit_tree(tree)}
                target.unlink()
                expect(f"control: {tag} reads red", tag_of[tag] in tags, f"expected [{tag_of[tag]}], got {sorted(tags)}")

            tag_of = {
                "a planted old identifier": "identifier",
                "a planted twin-named file": "old-name",
                "a planted file at the root": "outside-recipe",
                "a planted symlink": "symlink",
            }
            plant("a planted old identifier", "engine2/core/note.h", b"// see SwarmSynth\n")
            plant("a planted twin-named file", "engine2/core/swarmsaw.h")
            plant("a planted file at the root", "README.md")
            plant("a planted symlink", "engine2/core/out", link_to="/")

            if args.build:
                code, output = build_in_tree(tree)
                expect("recipe tree builds an empty library and runs an empty suite", code == 0,
                       output[-1500:])
                expect("the empty suite says so", "suite: 0 items, 0 failed" in output, output[-800:])

        for label, also, want_tag in (
            ("a recipe that adds the old engine's directory", ["/h2/engine/"], "denied-directory"),
            ("a recipe that adds one legacy source file", ["/src/hypersaw_clap.cpp"], "denied-directory"),
            ("a recipe that gives the builder the reference renders", ["/engine2/data/reference-renders/"],
             "test-author-only"),
        ):
            with RecipeTree(also=also) as tree:
                tags = {t for t, _ in audit_tree(tree)}
                expect(f"control: {label} reads red", want_tag in tags,
                       f"expected [{want_tag}], got {sorted(tags)}")
    except RuntimeError as err:
        expect("recipe tree could be made", False, str(err))

    if failures:
        print(f"engine2_cleanroom_check: FAILED ({len(failures)}): {'; '.join(failures)}")
        return 1
    print("engine2_cleanroom_check: ok (detector copy, data, recipe tree, 10 must-fail controls)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
