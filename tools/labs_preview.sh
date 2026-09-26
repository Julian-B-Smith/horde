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
# Usage: tools/labs_preview.sh [branch ...]   (no args: every open PR touching docs/design/)
# Then serve it:  python3 .claude/worktrees/labs-preview/tools/serve_labs.py 8146
set -u
ROOT=$(git rev-parse --show-toplevel) || exit 1
PREVIEW="$ROOT/.claude/worktrees/labs-preview"
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
cd "$PREVIEW" || exit 1

merged=""; skipped=""
for b in $BRANCHES; do
  if git merge -q --no-edit "origin/$b" >/dev/null 2>&1; then
    merged="$merged $b"
  else
    git merge --abort >/dev/null 2>&1
    skipped="$skipped $b"
  fi
done
# The navigator lists TRACKED labs only (gen_lab_index reads git ls-files), and
# the local merges above committed them, so the regenerated index includes them.
python3 tools/gen_lab_index.py >/dev/null && git add docs/design/index.html && \
  git -c user.name=labs-preview -c user.email=labs-preview@localhost commit -q -m "labs preview (local only)" >/dev/null 2>&1

echo "labs_preview: origin/main$( [ -n "$merged" ] && echo " +$merged" )"
[ -n "$skipped" ] && echo "labs_preview: SKIPPED (conflicts with main or another lab):$skipped"
echo "labs_preview: $(grep -o 'AWAITING YOUR REVIEW ([0-9]*)' docs/design/index.html || echo 'no labs awaiting review')"
exit 0
