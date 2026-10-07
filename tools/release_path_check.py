#!/usr/bin/env python3
"""release_path_check -- releases attach an explicit file list, and nothing is packaged in the tree (B446 Wave 2).

WIRED: ./verify fast

RULES, over every .github/workflows/*.yml and *.yaml:
  1. EXPLICIT ATTACHMENTS. Every `gh release create` / `gh release upload` names
     its files one by one: no glob character (`*`, `?`, `[`), no command
     substitution, no array expansion. A glob attaches whatever happens to sit in
     a directory; that is how a committed binary once rode into the release path.
     The attachment list must include SHA256SUMS and the SBOM (`.cdx.json`), the
     script must build them (tools/gen_sbom.py, `sha256sum`), and the job must
     check the tag's commit is on main (`merge-base --is-ancestor`) before it.
     A third-party release action (any `uses:` naming `release`) is RED: its file
     list is not something this check can read.
  2. ARTIFACTS LIVE OUTSIDE THE TREE. Every upload-artifact and download-artifact
     `path:` starts with `${{ runner.temp }}`.
  3. NOTHING IS PACKAGED UNDER A TRACKED PATH. On any `run:` line that packages
     (ditto -c, pkgbuild, productbuild, productsign, Compress-Archive, zip,
     tar -c, hdiutil create), no argument may begin with a top-level name the
     repository tracks (`dist/...`, `tools/...`): read from `git ls-files`, never a
     hardcoded list.
  4. THE SBOM GENERATOR STILL WORKS. tools/gen_sbom.py runs only on a tag, so it
     is exercised here on every `verify fast`: it must succeed on this tree and
     list every submodule, every FetchContent SDK pin and both pluginval pins.

Parsing reuses workflow_pin_check's line-based run-block reader (stdlib only).

FAIL CLOSED: no workflow file, an unreadable one, `git ls-files` failing, or a
tree with no `gh release create` anywhere (a release path that vanished is not a
clean one) is RED.

MUST-FAIL CONTROLS, in memory, every run: a glob attachment, a `$(...)` list, an
array expansion, an attachment list missing SHA256SUMS, a release step with no
ancestry check, an upload `path:` naming `dist/`, a download into the workspace,
a `ditto -c` and a `Compress-Archive` writing into a tracked directory, and a
third-party release action are each caught; the clean fixture passes. `--paths
FILE...` scans the given files instead of the tree (the pre-change control).
"""
import pathlib
import re
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from workflow_pin_check import USES_RE, indent, run_blocks  # noqa: E402
import gen_sbom  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
RELEASE_CMD_RE = re.compile(r"\bgh\s+release\s+(create|upload)\b")
PACKAGING_RE = re.compile(r"\b(ditto\s+-c|pkgbuild|productbuild|productsign|Compress-Archive|zip|"
                          r"tar\s+-c\w*|hdiutil\s+create)\b")
ARTIFACT_ACTION_RE = re.compile(r"^actions/(upload|download)-artifact@")


def tracked_top_level(root):
    out = subprocess.run(["git", "-C", str(root), "ls-files"], check=True,
                         capture_output=True, text=True).stdout
    return {p.split("/", 1)[0] for p in out.splitlines() if p}


def joined_commands(body):
    """Script lines with backslash (bash) and backtick (pwsh) continuations joined."""
    out, cur = [], ""
    for line in body:
        s = line.rstrip()
        if s.endswith("\\") or s.endswith("`"):
            cur += s[:-1] + " "
            continue
        out.append(cur + s)
        cur = ""
    if cur:
        out.append(cur)
    return out


def step_with_paths(lines, i):
    """`path:` values of the step whose `uses:` is at line i."""
    base = indent(lines[i])
    paths = []
    j = i + 1
    while j < len(lines):
        ln = lines[j]
        if ln.strip() and (indent(ln) < base or re.match(r"^\s*-\s", ln) and indent(ln) <= base):
            break
        m = re.match(r"^(\s*)path:\s*(.*)$", ln)
        if m:
            val = m.group(2).strip()
            if val[:1] in ("|", ">"):
                k = j + 1
                while k < len(lines) and (not lines[k].strip() or indent(lines[k]) > len(m.group(1))):
                    if lines[k].strip():
                        paths.append(lines[k].strip())
                    k += 1
            else:
                paths.append(val)
        j += 1
    return paths


def path_token(tok):
    t = tok.strip("'\"(),;")
    while t.startswith("./") or t.startswith(".\\"):
        t = t[2:]
    return re.split(r"[/\\]", t, maxsplit=1)[0] if ("/" in t or "\\" in t) else None


def scan(name, text, tracked):
    """-> (problems, number of `gh release create` sites). Pure apart from `tracked`."""
    problems, creates = [], 0
    lines = text.splitlines()
    for i, line in enumerate(lines):
        m = USES_RE.match(line)
        if not m:
            continue
        ref = m.group(2).split("#", 1)[0].strip().strip("'\"")
        if ARTIFACT_ACTION_RE.match(ref):
            for p in step_with_paths(lines, i):
                if not p.startswith("${{ runner.temp }}"):
                    problems.append(f"{name}:{i + 1}: artifact path `{p}` is not under ${{{{ runner.temp }}}} (rule 2)")
        elif "release" in ref.lower():
            problems.append(f"{name}:{i + 1}: third-party release action `{ref}`; attach with `gh release` (rule 1)")
    for first, body in run_blocks(text):
        script = "\n".join(body)
        for cmd in joined_commands(body):
            rm = RELEASE_CMD_RE.search(cmd)
            if rm:
                if rm.group(1) == "create":
                    creates += 1
                args = cmd[rm.end():]
                # --title / --notes values are prose, not files: drop them first.
                files_part = re.sub(r"--(?:notes|title)\s+\"[^\"]*\"", "", args)
                if re.search(r"[*?\[]", files_part):
                    problems.append(f"{name}:{first}: `gh release` attaches by glob or expansion (rule 1)")
                if "$(" in args or "`" in args:
                    problems.append(f"{name}:{first}: `gh release` attachment list built by command substitution (rule 1)")
                if rm.group(1) == "create":
                    if "SHA256SUMS" not in args or ".cdx.json" not in args:
                        problems.append(f"{name}:{first}: `gh release create` does not attach SHA256SUMS and the .cdx.json SBOM (rule 1)")
                    if "gen_sbom.py" not in script or "sha256sum" not in script:
                        problems.append(f"{name}:{first}: release script does not build the SBOM and checksums (rule 1)")
            if PACKAGING_RE.search(cmd):
                for tok in cmd.split():
                    top = path_token(tok)
                    if top and top in tracked:
                        problems.append(f"{name}:{first}: packaging command writes or reads `{tok.strip(chr(34))}` under tracked `{top}/` (rule 3)")
    if creates and "merge-base --is-ancestor" not in text:
        problems.append(f"{name}: drafts a release with no check that the tag's commit is on main (rule 1)")
    return problems, creates


SHA = "0123456789abcdef0123456789abcdef01234567"
CLEAN = f"""permissions:
  contents: read
jobs:
  package:
    steps:
      - run: |
          ditto -c -k stage "$RUNNER_TEMP/dist/horde.zip"
          Compress-Archive -Path stage/* -DestinationPath "$env:RUNNER_TEMP/dist/w.zip"
      - uses: actions/upload-artifact@{SHA} # v4.6.2
        with:
          name: horde
          path: ${{{{ runner.temp }}}}/dist/
  release:
    steps:
      - run: git merge-base --is-ancestor "$GITHUB_SHA" refs/remotes/origin/main
      - uses: actions/download-artifact@{SHA} # v4.3.0
        with:
          path: ${{{{ runner.temp }}}}/dist
      - run: |
          python3 tools/gen_sbom.py --version "$V" --out "$D/horde-$V.cdx.json"
          (cd "$D" && sha256sum a.zip "horde-$V.cdx.json" > SHA256SUMS)
          gh release create "$T" --draft --title "horde [x]" --notes "see *notes*" \\
             "$D/a.zip" "$D/horde-$V.cdx.json" "$D/SHA256SUMS"
"""


def selftest():
    tracked = {"dist", "tools", "src"}
    cases = [
        ("clean", CLEAN, 0),
        ("glob attachment", CLEAN.replace('"$D/a.zip"', "dist/*"), 1),
        ("command substitution", CLEAN.replace('"$D/a.zip"', '$(ls "$D")'), 1),
        ("array expansion", CLEAN.replace('"$D/a.zip"', '"${ASSETS[@]}"'), 1),
        ("no SHA256SUMS attached", CLEAN.replace(' "$D/SHA256SUMS"', ""), 1),
        ("no ancestry check", CLEAN.replace("merge-base --is-ancestor", "echo"), 1),
        ("upload from dist/", CLEAN.replace("path: ${{ runner.temp }}/dist/", "path: dist/"), 1),
        ("download into workspace", CLEAN.replace("path: ${{ runner.temp }}/dist\n", "path: dist\n"), 1),
        ("ditto into tracked dir", CLEAN.replace('"$RUNNER_TEMP/dist/horde.zip"', '"dist/horde.zip"'), 1),
        ("Compress-Archive into tracked dir", CLEAN.replace('"$env:RUNNER_TEMP/dist/w.zip"', '"dist/w.zip"'), 1),
        ("third-party release action", CLEAN + f"      - uses: softprops/action-gh-release@{SHA} # v2\n", 1),
    ]
    for label, text, want in cases:
        got, _ = scan("fixture", text, tracked)
        if (want == 0) != (not got):
            return f"selftest '{label}': expected {'clean' if want == 0 else 'RED'}, got {got or 'clean'}"
    return None


def sbom_problems():
    try:
        bom = gen_sbom.build_sbom(ROOT, "0.0.0-check")
    except Exception as e:  # any failure of the release-only path is the finding
        return [f"tools/gen_sbom.py fails on this tree: {e} (rule 4)"]
    names = {c["name"] for c in bom["components"]}
    gitmodules = (ROOT / ".gitmodules").read_text(encoding="utf-8")
    want = {p.rsplit("/", 1)[-1] for p in re.findall(r"^\s*path\s*=\s*(\S+)", gitmodules, re.M)}
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    want |= {u.rstrip("/").rsplit("/", 1)[-1].removesuffix(".git")
             for u in re.findall(r"GIT_REPOSITORY\s+(\S+)", cmake)}
    want |= {"pluginval_macOS.zip", "pluginval_Windows.zip"}
    missing = sorted(want - names)
    return [f"SBOM is missing {', '.join(missing)} (rule 4)"] if missing else []


def main(argv):
    broken = selftest()
    if broken:
        print(f"release_path_check: FAILED -- detector miscalibrated: {broken}", file=sys.stderr)
        return 1
    explicit = argv[:1] == ["--paths"]
    if explicit:
        files = [pathlib.Path(p) for p in argv[1:]]
    else:
        wf = ROOT / ".github" / "workflows"
        files = sorted(list(wf.glob("*.yml")) + list(wf.glob("*.yaml")))
    if not files:
        print("release_path_check: FAILED -- no workflow files found", file=sys.stderr)
        return 1
    try:
        tracked = tracked_top_level(ROOT)
    except (subprocess.CalledProcessError, OSError) as e:
        print(f"release_path_check: FAILED -- git ls-files: {e}", file=sys.stderr)
        return 1
    problems, creates = [], 0
    for f in files:
        try:
            text = f.read_text(encoding="utf-8")
        except OSError as e:
            problems.append(f"{f}: unreadable ({e})")
            continue
        p, c = scan(f.name, text, tracked)
        problems += p
        creates += c
    if not creates:
        problems.append("no workflow drafts a release (`gh release create`); the release path is gone, not clean")
    if not explicit:
        problems += sbom_problems()
    if problems:
        print("release_path_check: FAILED", file=sys.stderr)
        for p in problems:
            print(f"  {p}", file=sys.stderr)
        return 1
    print(f"release_path_check: GREEN ({len(files)} workflows, {creates} release step; SBOM ok; controls ok)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
