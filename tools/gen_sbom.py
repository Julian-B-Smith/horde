#!/usr/bin/env python3
"""gen_sbom -- the CycloneDX SBOM attached to every horde release (B446 Wave 2).

Usage: python3 tools/gen_sbom.py --version 1.2.3 [--out FILE]

WHAT IT LISTS, each read from the file that actually pins it, never a second copy:
  - every git submodule (`git ls-files --stage`, gitlink mode 160000; URL from
    .gitmodules): compiled into the plugin;
  - every SDK the root CMakeLists.txt fetches with FetchContent_Declare, at the
    commit its GIT_TAG resolves to (the VST3 SDK and the AudioUnitSDK). The VST3
    SDK's own submodules are pinned by that commit's gitlinks and ride with it;
  - pluginval, from ci.yml's PLUGINVAL_* env: a CI validation tool, NOT shipped,
    so its scope is `excluded`, with the SHA-256 the workflow enforces.

FAILS CLOSED: a piece it expects but cannot find (no submodules, an SDK whose tag
is not a 40-hex commit, a missing pluginval pin) exits 1 instead of emitting a
thinner SBOM. tools/release_path_check.py runs build_sbom() on every
`verify fast`, so this release-only path cannot rot unseen until a tag.

DETERMINISTIC: no wall-clock read. The timestamp is the HEAD commit's committer
time and the serial number is derived from the version and the HEAD commit, so
the same tag always yields the same file.
"""
import argparse
import json
import pathlib
import re
import subprocess
import sys
import uuid

ROOT = pathlib.Path(__file__).resolve().parent.parent
HEX40 = re.compile(r"^[0-9a-f]{40}$")
HEX64 = re.compile(r"^[0-9a-f]{64}$")


class SbomError(Exception):
    pass


def git(root, *args):
    return subprocess.run(["git", "-C", str(root), *args], check=True,
                          capture_output=True, text=True).stdout


def github_purl(url, commit):
    m = re.match(r"^https://github\.com/([^/]+)/([^/]+?)(?:\.git)?/?$", url)
    if not m:
        raise SbomError(f"not a GitHub https URL: {url}")
    return f"pkg:github/{m.group(1).lower()}/{m.group(2).lower()}@{commit}"


def submodules(root):
    urls = {}
    for line in git(root, "config", "-f", ".gitmodules", "--get-regexp", r"^submodule\..*\.url$").splitlines():
        key, url = line.split(None, 1)
        urls[key[len("submodule."):-len(".url")]] = url.strip()
    out = []
    for line in git(root, "ls-files", "--stage").splitlines():
        meta, path = line.split("\t", 1)
        mode, sha, _stage = meta.split()
        if mode != "160000":
            continue
        url = urls.get(path)
        if not url:
            raise SbomError(f"submodule {path} has no URL in .gitmodules")
        out.append({"type": "library", "name": path.rsplit("/", 1)[-1], "version": sha,
                    "purl": github_purl(url, sha),
                    "externalReferences": [{"type": "vcs", "url": url}],
                    "properties": [{"name": "horde:pinned-by", "value": f"git submodule {path}"}]})
    if not out:
        raise SbomError("no submodules found")
    return out


def fetched_sdks(cmake_text):
    variables = dict(re.findall(r"^\s*set\((\w+)\s+([0-9a-f]{40})\)", cmake_text, re.M))
    out = []
    for block in re.findall(r"FetchContent_Declare\((.*?)\)", cmake_text, re.S):
        name = block.split()[0]
        repo = re.search(r"GIT_REPOSITORY\s+(\S+)", block)
        tag = re.search(r"GIT_TAG\s+(\S+)", block)
        if not repo or not tag:
            raise SbomError(f"FetchContent_Declare({name}) lacks GIT_REPOSITORY or GIT_TAG")
        ref = tag.group(1)
        var = re.fullmatch(r"\$\{(\w+)\}", ref)
        commit = variables.get(var.group(1)) if var else ref
        if not commit or not HEX40.match(commit):
            raise SbomError(f"FetchContent_Declare({name}) GIT_TAG {ref} is not a 40-hex commit")
        url = repo.group(1)
        out.append({"type": "library", "name": url.rstrip("/").rsplit("/", 1)[-1].removesuffix(".git"),
                    "version": commit, "purl": github_purl(url, commit),
                    "externalReferences": [{"type": "vcs", "url": url}],
                    "properties": [{"name": "horde:pinned-by", "value": f"CMakeLists.txt FetchContent {name}"}]})
    if not out:
        raise SbomError("no FetchContent_Declare SDK pins found in CMakeLists.txt")
    return out


def pluginval(ci_text):
    env = dict(re.findall(r"^\s*(PLUGINVAL_\w+):\s*(\S+)\s*$", ci_text, re.M))
    base = env.get("PLUGINVAL_URL_BASE", "")
    ver = re.search(r"/download/(v[^/]+)$", base)
    if not ver:
        raise SbomError("ci.yml has no PLUGINVAL_URL_BASE ending in /download/<version>")
    out = []
    for asset, key in (("pluginval_macOS.zip", "PLUGINVAL_MACOS_SHA256"),
                       ("pluginval_Windows.zip", "PLUGINVAL_WINDOWS_SHA256")):
        digest = env.get(key, "")
        if not HEX64.match(digest):
            raise SbomError(f"ci.yml {key} is not a 64-hex SHA-256")
        out.append({"type": "application", "name": asset, "version": ver.group(1),
                    "scope": "excluded",
                    "hashes": [{"alg": "SHA-256", "content": digest}],
                    "externalReferences": [{"type": "distribution", "url": f"{base}/{asset}"}],
                    "properties": [{"name": "horde:role", "value": "CI validation tool, not shipped"}]})
    return out


def build_sbom(root, version, cmake_text=None, ci_text=None):
    """-> the SBOM dict. Texts may be passed in so a check can feed fixtures."""
    root = pathlib.Path(root)
    if cmake_text is None:
        cmake_text = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    if ci_text is None:
        ci_text = (root / ".github/workflows/ci.yml").read_text(encoding="utf-8")
    head = git(root, "rev-parse", "HEAD").strip()
    when = git(root, "log", "-1", "--format=%cI", "HEAD").strip()
    return {
        "bomFormat": "CycloneDX",
        "specVersion": "1.5",
        "serialNumber": f"urn:uuid:{uuid.uuid5(uuid.NAMESPACE_URL, f'horde@{version}@{head}')}",
        "version": 1,
        "metadata": {
            "timestamp": when,
            "component": {"type": "application", "name": "horde", "version": version,
                          "properties": [{"name": "horde:source-commit", "value": head}]},
        },
        "components": submodules(root) + fetched_sdks(cmake_text) + pluginval(ci_text),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", required=True)
    ap.add_argument("--out")
    a = ap.parse_args()
    try:
        bom = build_sbom(ROOT, a.version)
    except (SbomError, subprocess.CalledProcessError, OSError) as e:
        print(f"gen_sbom: FAILED -- {e}", file=sys.stderr)
        return 1
    text = json.dumps(bom, indent=2) + "\n"
    if a.out:
        pathlib.Path(a.out).write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
