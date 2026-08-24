#!/usr/bin/env bash
# launch_sweep.sh -- RUN-20260824-0522, step 4.
#
# Runs tools/sweep_project_core.sh in a SEPARATE GIT WORKTREE with a SEPARATE
# BUILD DIRECTORY. That is a standing owner ruling
# (docs/OWNER_DOCKET.md, "Process rulings adopted with it"), not a preference:
# the terrain sweep once contaminated other targets with mutant-generated
# Verilator sources and made clean RTL look broken, and sharing one build tree
# between agents "is no longer defensible".
#
# The worktree is at the SHIPPING COMMIT, so the sweep scores what was pushed
# rather than whatever the main tree happens to hold while it runs. It also
# means the main tree can be edited freely while the sweep is alive, which is
# the other half of why the ruling exists.
set -u

WT=/c/programmieren/zencrifice/.pcsweep
LOG=${1:?usage: launch_sweep.sh <logfile>}

export VERILATOR_ROOT='C:\programmieren\zencrifice\.tools\oss-cad-suite\share\verilator'
export PATH="/c/programmieren/zencrifice/.tools/oss-cad-suite/bin:/c/programmieren/zencrifice/.tools/oss-cad-suite/lib:/c/programmieren/dsstuff/mingw64/bin:$PATH"
export ZHAO_BUILD_DIR=build-pcsweep

cd "$WT" || exit 2
echo "sweep worktree: $WT at $(git rev-parse --short HEAD)"
echo "build dir:      $ZHAO_BUILD_DIR"
echo

cmake -S . -B "$ZHAO_BUILD_DIR" -G Ninja > "$LOG.configure" 2>&1 || {
  echo "ABORT: the worktree's fresh build tree did not configure -- see $LOG.configure"
  exit 3
}

bash tools/sweep_project_core.sh
