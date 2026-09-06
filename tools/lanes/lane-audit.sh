#!/usr/bin/env bash
# lane-audit.sh -- find work that exists on ONE MACHINE ONLY.
#
# This project spawns isolated clones constantly ("lanes"), and a lane is a
# place work goes to die quietly. On 2026-09-06 a creature pass reported "both
# repos clean and pushed" while all sixteen of its commits were still local --
# and the live site served the build, so every downstream signal looked correct.
# A publish deploys from LOCAL FILES; it proves nothing about the remote.
#
# Sweeping every lane afterwards found a second casualty nobody was looking for:
# nine commits from six days earlier, on no remote branch in either repo,
# including one recording owner direction.
#
# Run it from the directory that holds the lanes (default: the zencrifice root).
#
#   bash zhaozhou/tools/lanes/lane-audit.sh [root]
#
# Reports, per repo checkout:
#   NOT-ON-ANY-REMOTE  HEAD is on no remote branch -- work exists in one place
#   UNPUSHED:n         n commits reachable from no remote ref
#   DIRTY:n            uncommitted tracked changes (ignores .wrangler, stackdumps)
#   SIZE               how much disk the lane holds
#
# SIZE is reported because on 2026-09-06 this project filled a 952 GB disk to
# ZERO while a render was running. Twenty-four finished agent lanes were still
# on disk, each a full clone with its own build tree and rendered frames.
# Reclaiming them freed 217 GB. An audit that finds orphaned commits but says
# nothing about size lets the same lanes kill the machine instead.
#
# ⚠ AND `git clean -fd` DOES NOT REACH RENDER FRAMES. They are gitignored, so
# only `git clean -fdX` removes them. In the coordinator's own lane the
# committed evidence was 0.2 GB and the ignored working frames were 16.6 GB --
# eighty times larger, and invisible to the usual clean.
#
# It only READS. Nothing is pushed, merged or deleted -- preserving orphaned
# work is a judgement call (an archive/ branch is usually right; merging
# superseded work into main is usually wrong).

set -uo pipefail
root="${1:-$(pwd)}"
found=0

for repo in "$root"/*/*/.git; do
  d="$(dirname "$repo")"
  cd "$d" 2>/dev/null || continue
  git rev-parse --git-dir >/dev/null 2>&1 || continue

  unpushed=$(git log --oneline --all --not --remotes 2>/dev/null | wc -l | tr -d ' ')
  onremote=$(git branch -r --contains HEAD 2>/dev/null | wc -l | tr -d ' ')
  dirty=$(git status --porcelain=v1 2>/dev/null \
            | grep -vE '^\?\? (\.wrangler|bash\.exe\.stackdump)' | wc -l | tr -d ' ')

  flags=""
  [ "$onremote" = "0" ] && flags="$flags NOT-ON-ANY-REMOTE"
  [ "$unpushed" != "0" ] && flags="$flags UNPUSHED:$unpushed"
  [ "$dirty" != "0" ] && flags="$flags DIRTY:$dirty"

  if [ -n "$flags" ]; then
    found=1
    printf '%-56s%s\n' "${d#$root/}" "$flags"
    [ "$unpushed" != "0" ] && git log --oneline --all --not --remotes 2>/dev/null \
      | head -5 | sed 's/^/      /'
  fi
done

if [ "$found" = "0" ]; then
  echo "All lanes clean: every HEAD is on a remote branch, nothing unpushed."
else
  echo
  echo "A lane flagged NOT-ON-ANY-REMOTE or UNPUSHED holds work that exists in"
  echo "one place. Preserve it (push to archive/<lane-name>) before deleting the"
  echo "lane. Do NOT merge superseded work into main -- rejected approaches are"
  echo "worth keeping as history and harmful as live code."
fi
