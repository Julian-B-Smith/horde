#!/usr/bin/env bash
# engine2_cleanroom.sh -- make the builder's tree: a separate git worktree, sparse-checked-out to
# ONLY the new engine's tree and its spec (B497 part 1, ADR-223; spec BD-10).
#
#   tools/engine2_cleanroom.sh <dest> [<ref>]        the tree at <ref> (default HEAD)
#   tools/engine2_cleanroom.sh <dest> --snapshot     HEAD, with engine2/ and the spec taken from
#                                                    the working tree (the check uses this: it
#                                                    must see edits that are not committed yet)
#   --also <pattern>   add a sparse pattern to the recipe. Exists for ONE caller: the must-fail
#                      control in tools/engine2_cleanroom_check.py, which proves the no-old-code
#                      check goes red when the tree holds old code. Never use it to build.
#
# <dest> is an absolute path that does not exist yet. Remove the tree afterwards with
# `git worktree remove --force <dest>`.
#
# WHY NON-CONE. Cone-mode sparse checkout always materialises every file at the repository root
# (the root README describes the old engine's file map; the root CMakeLists builds it), so the
# recipe lists its paths literally instead. The patterns are the whole recipe: add a line to
# INCLUDE and the tree grows, which engine2_cleanroom_check.py then reads as red unless it is
# also allowed there.
#
# THE TWO EXCLUSIONS. The reference renders and the version-0 example states are the test author's
# input data: spec 10.1 says "the test author receives the reference renders ... the builder does
# not", and IF-4 sends the example states to the test author with them. They live under engine2/data/
# so the test author has one tree, and the builder's recipe leaves them out. A later pattern wins
# over an earlier one, which is how --also can put them back (the control does).
#
# libs/clap is NOT in the recipe: the engine's API is plain data in the units of the spec's
# section 5 (IF-1 names CLAP's expressions only as a mapping), and CLAP lives in the plugin shell.
set -euo pipefail

INCLUDE=(
  "/engine2/"
  "!/engine2/data/reference-renders/"
  "!/engine2/data/version0-states/"
  "/specs/SPEC-ENGINE.md"
)

dest="${1:?usage: engine2_cleanroom.sh <absolute dest> [<ref> | --snapshot] [--also <pattern>]...}"
shift
case "$dest" in /*) ;; *) echo "engine2_cleanroom: <dest> must be an absolute path (the sandbox resets cwd)" >&2; exit 2 ;; esac
[ ! -e "$dest" ] || { echo "engine2_cleanroom: $dest already exists" >&2; exit 2; }

mode=ref
ref=HEAD
also=()
while [ $# -gt 0 ]; do
  case "$1" in
    --snapshot) mode=snapshot ;;
    --also) shift; also+=("${1:?--also needs a pattern}") ;;
    *) ref="$1" ;;
  esac
  shift
done

repo="$(git rev-parse --show-toplevel)"
cd "$repo"

if [ "$mode" = snapshot ]; then
  # A commit made from a throwaway index, so the working tree, the real index and the branch are
  # not touched. `git add -A` honours .gitignore, so ignored audio never rides along. The
  # committer identity is set because CI has none and commit-tree refuses without one.
  idx="$(mktemp)"; rm -f "$idx"
  export GIT_INDEX_FILE="$idx"
  git read-tree HEAD
  git add -A -- engine2 specs/SPEC-ENGINE.md
  tree="$(git write-tree)"
  unset GIT_INDEX_FILE
  rm -f "$idx"
  commit="$(GIT_AUTHOR_NAME=engine2-cleanroom GIT_AUTHOR_EMAIL=engine2-cleanroom@invalid \
            GIT_COMMITTER_NAME=engine2-cleanroom GIT_COMMITTER_EMAIL=engine2-cleanroom@invalid \
            git commit-tree "$tree" -p HEAD -m "engine2 cleanroom snapshot")"
else
  commit="$(git rev-parse --verify "$ref^{commit}")"
fi

git worktree add --quiet --no-checkout --detach "$dest" "$commit"
git -C "$dest" sparse-checkout set --no-cone "${INCLUDE[@]}" ${also[@]+"${also[@]}"}
# --no-checkout left the index empty; this fills index and files in one step, through the patterns.
git -C "$dest" read-tree -m -u HEAD
echo "engine2_cleanroom: tree at $dest ($(git -C "$dest" ls-files -v | grep -vc '^S' || true) files on disk)"
