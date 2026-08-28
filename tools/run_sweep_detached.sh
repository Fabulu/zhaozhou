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
set -u

SWEEP="${1:?usage: run_sweep_detached.sh <sweep-name> [logdir]}"
LOGDIR="${2:-runs/CLAUDE-RUNS/RUN-20260827-1747-field-v3-rearchitecture}"

cd "$(dirname "$0")/.." || exit 1

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
