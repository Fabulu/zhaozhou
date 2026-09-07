#!/usr/bin/env bash
# watch_for_owner_brief.sh -- block until owner direction lands in the repo.
#
# WHY THIS EXISTS
# ---------------
# CLAUDE.md records that owner direction "was posted four times because it kept
# not reaching the working agent, then relayed into a run folder that had
# already closed, and five passes solved the wrong problem in the meantime."
# The lesson recorded there is that instructions are not delivered until they
# are read. Polling by hand between other work is exactly how that happens
# again, so this watches instead.
#
# It exits 0 the moment EITHER
#   * the tracked upstream branch gains commits we do not have, or
#   * a new file appears anywhere in the tree whose name looks like owner
#     direction, an architecture brief, or a spec.
# and prints what it saw. It exits 0 with NOTHING FOUND after the cap, so a
# silent watcher cannot masquerade as "no instructions arrived" forever.
#
# Deliberately NOT a git hook and NOT a cron: it is a foreground blocker meant
# to be run in the background of a working session and to notify on completion.

set -u
cd "$(dirname "$0")/../.." || exit 1

INTERVAL="${1:-180}"
MAX="${2:-40}"

# The baseline is taken ONCE, before the loop, so a file that appears during
# the first sleep is still seen as new.
baseline_files=$(mktemp)
find . -path ./node_modules -prune -o -type f \
     \( -iname 'OWNER-DIRECTION*' -o -iname '*ARCHITECTURE*BRIEF*' \
        -o -iname '*-BRIEF-*.md' -o -iname '*RESPEC*' -o -iname '*-SPEC-*.md' \) \
     -print 2>/dev/null | sort > "$baseline_files"
baseline_head=$(git rev-parse HEAD 2>/dev/null)

echo "watching: interval ${INTERVAL}s, cap ${MAX} rounds, baseline $(wc -l < "$baseline_files") direction file(s) at ${baseline_head:0:8}"

for i in $(seq 1 "$MAX"); do
    sleep "$INTERVAL"
    git fetch --all -q 2>/dev/null

    behind=$(git rev-list --count HEAD..@{u} 2>/dev/null || echo 0)
    if [ "${behind:-0}" -gt 0 ]; then
        echo "BRIEF-WATCH: upstream gained ${behind} commit(s)"
        git log --oneline HEAD..@{u} 2>/dev/null | head -20
        rm -f "$baseline_files"
        exit 0
    fi

    now=$(mktemp)
    find . -path ./node_modules -prune -o -type f \
         \( -iname 'OWNER-DIRECTION*' -o -iname '*ARCHITECTURE*BRIEF*' \
            -o -iname '*-BRIEF-*.md' -o -iname '*RESPEC*' -o -iname '*-SPEC-*.md' \) \
         -print 2>/dev/null | sort > "$now"
    newfiles=$(comm -13 "$baseline_files" "$now")
    rm -f "$now"
    if [ -n "$newfiles" ]; then
        echo "BRIEF-WATCH: new direction file(s) appeared:"
        echo "$newfiles"
        rm -f "$baseline_files"
        exit 0
    fi
done

echo "BRIEF-WATCH: NOTHING FOUND after ${MAX} rounds (~$((MAX * INTERVAL / 60)) min). Not evidence that nothing was sent -- only that nothing landed in this tree."
rm -f "$baseline_files"
exit 0
