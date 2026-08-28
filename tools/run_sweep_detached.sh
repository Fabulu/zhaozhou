#!/usr/bin/env bash
# Run a mutation sweep DETACHED from the agent task.
#
# WHY THIS EXISTS
# ---------------
# An agent background task can be killed at any moment. A sweep killed between
# applying a mutant and restoring it leaves that mutant in the RTL -- which has
# happened four times in this project, and once reached a PUSHED COMMIT whose
# message said the block was fixed. Launched detached, the sweep is not a child
# of the task, so a task kill cannot interrupt it mid-mutation.
#
# WHY THIS IS A SHELL SCRIPT AND NOT A POWERSHELL WRAPPER
# --------------------------------------------------------
# The first version was PowerShell invoking bash, and it set PATH from the
# Windows environment. That put devkitPro's toolchain ahead of mingw64, so the
# driver's own consumer-roster check ran under a `grep` that rejected its
# regex and the sweep aborted with a roster error that had nothing to do with
# the roster -- which was correct all along.
#
# The environment a sweep runs in is part of the sweep. It is set here, in one
# place, exactly as the working invocations set it.
#
#   bash tools/run_sweep_detached.sh field_v3_mulbank [logdir]
#
# AND NOTHING ELSE MAY TOUCH A BUILD TREE WHILE A SWEEP RUNS. Not just the
# sweep's own tree -- anything. On 2026-08-28 a sweep aborted three times with
# "pristine target did not link" while its rebuild log showed the target
# reaching [10/12] Linking every time. The exe was being built and then
# disappearing, because a build of a DIFFERENT tree was running concurrently
# and the two share ccache and the source directory's cmake state.
#
# A sweep deletes the target's model directory AND its exe before rebuilding,
# so anything that disturbs that rebuild leaves no binary at all and the
# driver's guard correctly refuses to score. The guard is right every time;
# the cost is that the cause is three layers away.
#
# The rule that actually works: one build tree, one writer, and no other
# build anywhere while a sweep is in flight.
## AND NEVER PIPE A SWEEP. Running one as
#
#     bash tools/sweep_x.sh 2>&1 | head -25
#
# kills it with SIGPIPE the moment the 25th line arrives -- which lands
# mid-run, between applying a mutant and restoring it, and strands that mutant
# in the RTL. That is exactly how M07 was stranded on 2026-08-28, and it is
# self-inflicted rather than a harness problem: `head` closing the pipe IS a
# kill signal. Redirect to a file and tail the file instead, which is what
# this runner does.
set -u

SWEEP="${1:?usage: run_sweep_detached.sh <sweep-name> [logdir]}"
LOGDIR="${2:-runs/CLAUDE-RUNS/RUN-20260827-1747-field-v3-rearchitecture}"
# Optional third argument: the build tree to use. Two sweeps CAN run at once
# provided each has its own tree -- what breaks is two writers in one tree.
#
# BUT DO NOT RELY ON THIS FROM Start-Process. Argument passing into the
# detached bash is unreliable on this machine, the same way USERPROFILE fails
# to propagate into it: an attempt to run the engine sweep in build-exec
# silently fell back to build-verify, collided with the bank sweep already
# there, and both aborted on "pristine target did not link" -- the exact
# two-writers-one-tree failure the knob exists to avoid.
#
# It went unnoticed for one run because the fallback is SILENT and plausible.
# So the log name carries the tree: a run that lands in the wrong tree is
# visible in its own filename.
#
# Until the argument passing is understood, RUN SWEEPS ONE AT A TIME.
export BUILD_DIR="${3:-build-verify}"
echo "run_sweep_detached: sweep=$1 build_dir=$BUILD_DIR" >&2

cd "$(dirname "$0")/.." || exit 1

# CCACHE NEEDS BOTH, AND I TESTED THEM ONE AT A TIME.
#
# Four aborted sweeps, all showing "ABORT: pristine target did not link". The
# real error was in the rebuild log all along once it started being written:
#
#     ccache: error: The USERPROFILE environment variable must be set
#
# A detached process does not inherit USERPROFILE the way an interactive shell
# does. I tried inheritance, an inline export, and deriving it from HOME --
# and then replaced that attempt with CCACHE_DIR, which removed the export
# while adding the cache path. So the two fixes were never in the file at the
# same time, and each looked like it had failed.
#
# Both are set here. CCACHE_DIR also puts the cache inside the repo, where
# every shell agrees on the path, rather than in a per-user temp directory
# that Git bash and msys bash resolve differently.
export USERPROFILE="C:\Users\Fabian Trunz"
export CCACHE_DIR="$(pwd)/.ccache"
mkdir -p "$CCACHE_DIR"

# A LOST NEWLINE ATE THIS EXPORT. An earlier scripted edit joined the mkdir
# and the next line into `mkdir -p "$CCACHE_DIR"export VERILATOR_ROOT=...`,
# which silently created two junk directories and left VERILATOR_ROOT unset
# for every detached sweep since. It kept working only because PATH below
# still found verilator; the day it stops, the error will name something
# else entirely.
export VERILATOR_ROOT="C:/Programmieren/zencrifice/.tools/oss-cad-suite/share/verilator"
export PATH="/c/programmieren/dsstuff/mingw64/bin:/c/Programmieren/zencrifice/.tools/oss-cad-suite/share/verilator/bin:$PATH"

LOG="$LOGDIR/${SWEEP}_sweep.log"

bash "tools/sweep_${SWEEP}.sh" >"$LOG" 2>&1
echo "EXIT:$?" >>"$LOG"

# The restore cannot vouch for itself: it copies back a snapshot taken at
# startup, so if the file was already mutated then, "restore" restores the
# mutant and every guard downstream agrees the tree is pristine. Check from
# outside that chain, here as well as inside the driver, in case the driver
# itself was interrupted before reaching its own check.
python tools/sweep_check_clean.py "tools/sweep_${SWEEP}_mutants.py" >>"$LOG" 2>&1
echo "CLEAN:$?" >>"$LOG"
