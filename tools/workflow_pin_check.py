#!/usr/bin/env python3
"""workflow_pin_check -- GitHub workflows pin actions by commit and keep a read-only token (B446 Wave 2).

WIRED: ./verify fast

RULES, over every .github/workflows/*.yml and *.yaml:
  1. Every `uses:` names a full 40-hex commit SHA, followed by a `# v<version>`
     comment so a human can still read which release it is. A tag or a branch is
     a pointer its owner can move; a commit is not. Local `./` actions are exempt;
     a `docker://` image must carry an `@sha256:` digest.
  2. Every workflow declares a TOP-LEVEL `permissions:` (column 0), so the
     token's scope is written in the file rather than inherited from a repository
     toggle. Jobs that need more widen it themselves.
  3. No `pull_request_target` trigger.
  4. No `${{ github.event... }}` expression inside a `run:` script. Event fields
     reach a script through `env:` and are then quoted shell variables.

PARSING. Line-based, stdlib only (no YAML library is installed anywhere in this
harness, on purpose). A `run:` value is the rest of its line, or, after `|` or
`>`, every following line indented deeper than the `run:` key or blank. A `#`
starting a line, or following whitespace, opens a YAML comment.

FAIL CLOSED. No workflow file found, or one that cannot be read, is RED.

MUST-FAIL CONTROLS, in memory, every run: a tag-pinned action, a SHA with no
version comment, a short SHA, an unpinned docker image, a workflow with only
job-level permissions, a pull_request_target trigger, and an event expression
in a block and in a one-line `run:` are each caught; a local action, the same
event expression under `env:`, `github.event_name`, and the trigger name inside
a comment are not. `--paths FILE...` scans the given files instead of the tree,
which is how the control against a pre-change copy of the workflows is run.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
USES_RE = re.compile(r"^(\s*)(?:-\s+)?uses:\s*(.*)$")
RUN_RE = re.compile(r"^(\s*(?:-\s+)?)run:\s*(.*)$")
PINNED_RE = re.compile(r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_./-]+@[0-9a-f]{40}$")
VERSION_COMMENT_RE = re.compile(r"^#\s*v\d")
EVENT_EXPR_RE = re.compile(r"\$\{\{[^}]*\bgithub\.event\b")
COMMENT_RE = re.compile(r"(^|\s)#.*$")


def indent(line):
    return len(line) - len(line.lstrip(" "))


def run_blocks(text):
    """-> [(first_line_no, [script lines])] for every `run:` in a workflow."""
    lines = text.splitlines()
    out = []
    for i, line in enumerate(lines):
        m = RUN_RE.match(line)
        if not m:
            continue
        key_col = len(m.group(1))
        value = m.group(2).strip()
        if value[:1] in ("|", ">"):
            body = []
            for nxt in lines[i + 1:]:
                if nxt.strip() and indent(nxt) <= key_col:
                    break
                body.append(nxt)
            out.append((i + 1, body))
        else:
            out.append((i + 1, [value]))
    return out


def scan(name, text):
    """-> list of problems for one workflow's text. Pure, so the controls can feed it."""
    problems = []
    if not re.search(r"^permissions:", text, re.M):
        problems.append(f"{name}: no top-level `permissions:` (rule 2)")
    for n, line in enumerate(text.splitlines(), 1):
        code = COMMENT_RE.sub("", line)
        if re.search(r"\bpull_request_target\b", code):
            problems.append(f"{name}:{n}: pull_request_target trigger (rule 3)")
        m = USES_RE.match(line)
        if not m:
            continue
        rest = m.group(2).strip()
        ref, _, comment = rest.partition("#")
        ref = ref.strip().strip("'\"")
        comment = "#" + comment if comment else ""
        if ref.startswith("./"):
            continue
        if ref.startswith("docker://"):
            if "@sha256:" not in ref:
                problems.append(f"{name}:{n}: docker image `{ref}` has no @sha256 digest (rule 1)")
            continue
        if not PINNED_RE.match(ref):
            problems.append(f"{name}:{n}: `{ref}` is not pinned to a 40-hex commit SHA (rule 1)")
        elif not VERSION_COMMENT_RE.match(comment.strip()):
            problems.append(f"{name}:{n}: `{ref}` lacks a trailing `# v<version>` comment (rule 1)")
    for first, body in run_blocks(text):
        for k, line in enumerate(body):
            if EVENT_EXPR_RE.search(line):
                problems.append(f"{name}:{first + k}: github.event expression inside run: (rule 4)")
    return problems


SHA = "0123456789abcdef0123456789abcdef01234567"
CLEAN = f"""name: x
on: [push]
permissions:
  contents: read
jobs:
  a:
    runs-on: ubuntu-latest
    # pull_request_target is named in this comment only
    steps:
      - uses: actions/checkout@{SHA} # v4.4.0
      - uses: ./local-action
      - name: safe
        env:
          T: ${{{{ github.event.pull_request.title }}}}
        run: |
          echo "$T" "${{{{ github.event_name }}}}"
"""


def selftest():
    """Planted fixtures in both directions. -> None, or what the detector got wrong."""
    cases = [
        ("clean", CLEAN, 0),
        ("tag pin", CLEAN.replace(f"@{SHA} # v4.4.0", "@v4"), 1),
        ("no version comment", CLEAN.replace(" # v4.4.0", ""), 1),
        ("short sha", CLEAN.replace(f"@{SHA}", f"@{SHA[:7]}"), 1),
        ("docker unpinned", CLEAN.replace("./local-action", "docker://alpine:3"), 1),
        ("job-level permissions only", CLEAN.replace("permissions:\n  contents: read\n", "")
         .replace("    runs-on:", "    permissions:\n      contents: read\n    runs-on:"), 1),
        ("pull_request_target", CLEAN.replace("on: [push]", "on: [pull_request_target]"), 1),
        ("event in run block", CLEAN.replace('echo "$T"', 'echo "${{ github.event.issue.title }}"'), 1),
        ("event in one-line run", CLEAN + "      - run: echo ${{ github.event.head_commit.message }}\n", 1),
    ]
    for label, text, want in cases:
        got = scan("fixture", text)
        if (want == 0) != (not got):
            return f"selftest '{label}': expected {'clean' if want == 0 else 'RED'}, got {got or 'clean'}"
    return None


def main(argv):
    broken = selftest()
    if broken:
        print(f"workflow_pin_check: FAILED -- detector miscalibrated: {broken}", file=sys.stderr)
        return 1
    if argv[:1] == ["--paths"]:
        files = [pathlib.Path(p) for p in argv[1:]]
    else:
        wf = ROOT / ".github" / "workflows"
        files = sorted(list(wf.glob("*.yml")) + list(wf.glob("*.yaml")))
    if not files:
        print("workflow_pin_check: FAILED -- no workflow files found", file=sys.stderr)
        return 1
    problems = []
    for f in files:
        try:
            text = f.read_text(encoding="utf-8")
        except OSError as e:
            problems.append(f"{f}: unreadable ({e})")
            continue
        name = f.relative_to(ROOT).as_posix() if f.is_absolute() and ROOT in f.parents else str(f)
        problems.extend(scan(name, text))
    if problems:
        print("workflow_pin_check: FAILED", file=sys.stderr)
        for p in problems:
            print(f"  {p}", file=sys.stderr)
        return 1
    print(f"workflow_pin_check: GREEN ({len(files)} workflows; controls ok)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
