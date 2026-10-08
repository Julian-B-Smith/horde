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
# Usage: tools/labs_preview.sh [branch ...]   (no args: every open PR touching docs/design/)
# Then serve it with the MAIN checkout's server, the scratch merge as its root argument;
# the script prints the exact command when it finishes:
#   python3 <main checkout>/tools/serve_labs.py 8146 <main checkout>/.claude/worktrees/labs-preview
set -u
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

# local/sluice is NOT linked into the preview any more (it was, for B329): the labs
# server serves only the lab trees (W2-05), so the Sluice lab shows its no-spec state
# here. A narrow, explicit opt-in is a ruling for the lead, not something to add quietly.
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

echo "labs_preview: origin/main$( [ -n "$merged" ] && echo " +$merged" )"
[ -n "$skipped" ] && echo "labs_preview: SKIPPED (conflicts with main or another lab):$skipped"
echo "labs_preview: $(grep -o 'AWAITING YOUR REVIEW ([0-9]*)' docs/design/index.html || echo 'no labs awaiting review')"
echo "labs_preview: serve with: python3 $MAIN/tools/serve_labs.py 8146 $PREVIEW"
exit 0
