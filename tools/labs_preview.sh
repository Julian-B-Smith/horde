#!/usr/bin/env bash
# labs_preview.sh — a LOCAL preview of the design labs: origin/main plus every
# open PR that touches docs/design/, merged in a throwaway worktree and served
# by tools/serve_labs.py. Nothing here is ever pushed and no PR is touched.
#
# WHY (human, 2026-09-26): lab PRs have to be reviewable the moment they go up,
# not after they merge — "I was under the impression that you could update the
# navigator locally without a merge." Merging to make something visible is not
# on the table; a local merge in a scratch worktree is.
#
# NOTHING FROM THE MERGED TREE EVER RUNS (ADR-194 / B446 W3b, W2-06). A lab branch is
# unreviewed, agent-written text. The old version of this script ran the merged
# worktree's tools/gen_lab_index.py and told the human to run its tools/serve_labs.py,
# so a branch got to choose code that runs in the human's shell. Now the helpers (index
# generator, server) are the MAIN checkout's copies and the merged tree is DATA they
# read. Git runs inside the scratch tree with --no-verify, and the hooks it could run
# are the shared repo's, never the branch's.
#
# Usage: tools/labs_preview.sh [--allow-sluice] [branch ...]   (no branches: every open PR touching docs/design/)
# Then serve it with the MAIN checkout's server, the scratch merge as its root argument;
# the script prints the exact command when it finishes:
#   python3 <main checkout>/tools/serve_labs.py 8146 <main checkout>/.claude/worktrees/labs-preview
#
# --allow-sluice (OFF by default; the human's per-run opt-in, ratified 2026-10-08, B446 W3c).
# The Sluice lab (B329) reads a private sibling's spec in place through the gitignored link
# local/sluice, which the labs server does not serve. With the flag this script links the
# preview's local/sluice to the MAIN checkout's, AFTER the merges (so a branch that force-added
# its own local/sluice cannot choose where the link points), and the printed serve command
# carries --allow-sluice, which opens that one subtree on the server. Without the flag nothing
# below changes: no link, and the plain serve command.
set -u
ALLOW_SLUICE=0; ARGS=()
for a in "$@"; do
  if [ "$a" = "--allow-sluice" ]; then ALLOW_SLUICE=1; else ARGS+=("$a"); fi
done
set -- ${ARGS[@]+"${ARGS[@]}"}
ROOT=$(git rev-parse --show-toplevel) || exit 1
PREVIEW="$ROOT/.claude/worktrees/labs-preview"
# The main checkout is computed from git's common dir (this script may run from any
# worktree), never written down: no machine path is committed.
MAIN=$(dirname "$(git rev-parse --path-format=absolute --git-common-dir)")
for t in gen_lab_index.py serve_labs.py; do
  [ -f "$MAIN/tools/$t" ] || { echo "labs_preview: $MAIN/tools/$t missing" >&2; exit 1; }
done
cd "$ROOT" || exit 1
git fetch -q origin || { echo "labs_preview: fetch failed" >&2; exit 1; }

if [ $# -gt 0 ]; then
  BRANCHES="$*"
else
  # gh reports each open PR's files; keep the ones touching a lab.
  BRANCHES=$(gh pr list --state open --json headRefName,files \
    -q '.[] | select(any(.files[]; .path | startswith("docs/design/"))) | .headRefName' 2>/dev/null)
fi

# A fresh detached worktree at origin/main every time: the preview never
# accumulates a merge that has since been closed or rewritten.
if [ -d "$PREVIEW" ]; then git worktree remove --force "$PREVIEW" >/dev/null 2>&1 || rm -rf "$PREVIEW"; fi
git worktree prune
git worktree add -q --detach "$PREVIEW" origin/main || exit 1

# local/sluice is linked only under --allow-sluice (see the header), and then only after
# the merges below. By default the labs server serves only the lab trees (W2-05), so the
# Sluice lab shows its no-spec state.
cd "$PREVIEW" || exit 1

merged=""; skipped=""
for b in $BRANCHES; do
  if git merge -q --no-edit --no-verify "origin/$b" >/dev/null 2>&1; then
    merged="$merged $b"
  else
    git merge --abort >/dev/null 2>&1
    skipped="$skipped $b"
  fi
done
# The navigator lists TRACKED labs only (gen_lab_index reads git ls-files), and
# the local merges above committed them, so the regenerated index includes them.
# The generator is the MAIN checkout's copy (-I: no cwd or user-site imports); the
# preview tree is its argument, never its code.
python3 -I "$MAIN/tools/gen_lab_index.py" "$PREVIEW" >/dev/null && git add docs/design/index.html && \
  git -c user.name=labs-preview -c user.email=labs-preview@localhost commit -q --no-verify -m "labs preview (local only)" >/dev/null 2>&1

SLUICE_FLAG=""; sluice="not linked"
if [ "$ALLOW_SLUICE" = 1 ]; then
  # Our link, made now that no branch can touch the tree again. A merged branch may have
  # force-added local/ or local/sluice (a symlink anywhere): remove whatever is there first.
  # rm on a symlink removes the link, never its target.
  if [ -e "$MAIN/local/sluice" ]; then
    rm -rf "$PREVIEW/local/sluice"; [ -L "$PREVIEW/local" ] && rm -f "$PREVIEW/local"
    mkdir -p "$PREVIEW/local" && ln -s "$MAIN/local/sluice" "$PREVIEW/local/sluice" \
      && SLUICE_FLAG="--allow-sluice " && sluice="linked, and the serve command opens it"
  else
    sluice="requested, but $MAIN/local/sluice does not exist (the Sluice lab shows its no-spec state)"
  fi
fi

echo "labs_preview: origin/main$( [ -n "$merged" ] && echo " +$merged" )"
[ -n "$skipped" ] && echo "labs_preview: SKIPPED (conflicts with main or another lab):$skipped"
echo "labs_preview: $(grep -o 'AWAITING YOUR REVIEW ([0-9]*)' docs/design/index.html || echo 'no labs awaiting review')"
[ "$ALLOW_SLUICE" = 1 ] && echo "labs_preview: local/sluice $sluice"
echo "labs_preview: serve with: python3 $MAIN/tools/serve_labs.py ${SLUICE_FLAG}8146 $PREVIEW"
exit 0
