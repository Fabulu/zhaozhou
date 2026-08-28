#!/usr/bin/env bash
# sweep_field_v3_exec.sh — mutation sweep for the FIELD v3 four-bank patch
# probe (fpga/rtl/synth/zhao_probe_v3_exec.sv; accumulator; Phase 3 probe 5 of
# reports/Fieldv3.md).
#
# Inherits the house guards (sweep_geom_wcache.sh / sweep_cmd_dma.sh):
#   6  the mutant table is Python — bash mangles $ and quotes;
#   preflight — every mutant LINTS before anything is scored;
#   5  rebuild deletes the model dir AND the exe (it lives outside the dir);
#   discard — model hash identical to pristine = did not re-elaborate;
#   7  consumer roster — the probe must be elaborated by exactly the targets
#      this sweep runs, or mutant-derived models could survive in unscored
#      consumers;
#   8  the binary is THREE ctest lanes (bare, --random 40, nightly 400);
#      the sweep runs every FAST lane; the nightly is excluded by the
#      cmd_dma precedent.
#
# Exit codes: 3 apply, 4 restore, 5 cross-check, 6 pristine build,
#             7 pristine tests red, 8 preflight, 9 consumer roster,
#             12 undeclared survivor.
#
# DEDICATED BUILD DIR (build-verify), added after a MEASURED collision:
# 2026-08-27, this sweep ran against the shared build/ while a concurrent
# session's ninja was live in it; nine of fifteen mutants were DISCARDED
# with "model or exe absent after rebuild" -- two writers, one build dir.
# The sweep now configures its own tree (same pinned toolchain as the
# windows-native preset). Sources are still the LIVE working tree, so the
# guard-7 single-consumer rule is what keeps a concurrently-edited file
# out of this sweep's scored cone.
set -u
cd "$(dirname "$0")/.." || exit 1

MUT=tools/sweep_field_v3_exec_mutants.py
RTL=fpga/rtl/synth/zhao_probe_v3_exec.sv
TARGETS="test_field_v3_exec_directed"

# An ABSOLUTE path inside the repo, not /tmp. Git-for-Windows bash and the
# msys bash map /tmp to DIFFERENT directories, so a log written by a detached
# runner was invisible to the shell reading it -- which is why the rebuild
# diagnostics appeared to be missing entirely when they were simply somewhere
# else.
REBUILD_LOG="${REBUILD_LOG:-$(pwd)/runs/CLAUDE-RUNS/sweep_rebuild.log}"

hash_of() { sha256sum <"$1" | cut -d' ' -f1; }

check_consumers() {
  local declared
  declared=$(grep -B12 "TOP_MODULE zhao_probe_v3_exec" tests/CMakeLists.txt \
             | grep -oE "verilate\(test_[a-z_0-9]+" | sed 's/verilate(//' | sort -u)
  if [ "$declared" != "$TARGETS" ]; then
    echo "ABORT: tests/CMakeLists.txt elaborates zhao_probe_v3_exec into target(s):"
    echo "$declared"
    echo "but this sweep runs: $TARGETS — update TARGETS or the roster."
    return 1
  fi
  return 0
}

model_hash() {
  local t h=""
  for t in $TARGETS; do
    h="$h$(find "build-verify/tests/CMakeFiles/$t.dir/Vzhao_probe_v3_exec.dir" -type f \
             \( -name "*.cpp" -o -name "*.h" \) 2>/dev/null \
           | sort | xargs sha256sum 2>/dev/null | sha256sum | cut -d" " -f1)"
  done
  echo "$h"
}

models_present() {
  local t
  for t in $TARGETS; do
    [ -d "build-verify/tests/CMakeFiles/$t.dir/Vzhao_probe_v3_exec.dir" ] || return 1
  done
  return 0
}

exes_present() {
  local t
  for t in $TARGETS; do
    [ -x "build-verify/tests/$t.exe" ] || return 1
  done
  return 0
}

rebuild() {
  local t
  for t in $TARGETS; do
    rm -rf "build-verify/tests/CMakeFiles/$t.dir"
    # guard 5: the exe lives OUTSIDE the target dir, so it must be deleted
    # too -- and it must be deleted from THIS sweep's own build dir. This
    # line said "build/tests" until 2026-08-27, which both left the real
    # stale exe in place (defeating the guard) and deleted another
    # session's binary out of the shared tree.
    rm -f "build-verify/tests/$t.exe"
  done
  # deleting a verilated target dir removes files only CONFIGURE regenerates
  # (the house sweeps learned this the same way); VERILATOR_ROOT must be set
  # or the configure fails and ninja silently uses the OLD source list.
  export VERILATOR_ROOT="${VERILATOR_ROOT:-C:/programmieren/zencrifice/.tools/oss-cad-suite/share/verilator}"
  # BUILD.md: three cmakes are on PATH and the msys2 one reports MSYS, which
  # disables every preset and leaves the OLD build.ninja in place. Pin it.
  # The verilator binary lives under share/verilator/bin, NOT under
  # oss-cad-suite/bin -- that directory does not exist, and this line named it
  # for five drivers. It survived only because cmake CACHES find_program, so a
  # tree that had once configured successfully kept working while a fresh one
  # would fail its verilator guard and leave the target unbuilt.
  export PATH="/c/programmieren/dsstuff/mingw64/bin:/c/programmieren/zencrifice/.tools/oss-cad-suite/share/verilator/bin:$PATH"
  # THE REBUILD LOGS. It used to send both commands to /dev/null, so a failed
  # configure or link produced only the downstream "pristine target did not
  # link" abort with no cause -- a guard firing correctly while the reason sat
  # three layers away and invisible. The log is overwritten each rebuild and
  # is the first place to look when a sweep aborts at exit 6.
  cmake -S . -B build-verify -G Ninja -DCMAKE_BUILD_TYPE=Release     -DCMAKE_CXX_COMPILER=C:/programmieren/dsstuff/mingw64/bin/g++.exe     -DCMAKE_MAKE_PROGRAM=C:/programmieren/dsstuff/mingw64/bin/ninja.exe     >"$REBUILD_LOG" 2>&1
  echo "CMAKE_EXIT:$?" >>"$REBUILD_LOG"
  # shellcheck disable=SC2086
  ninja -C build-verify $TARGETS >>"$REBUILD_LOG" 2>&1
  # THE EXIT CODES ARE RECORDED. Without them a failed rebuild surfaces only
  # as the downstream "pristine target did not link", and the log's last line
  # is ninja ANNOUNCING the link step -- ninja prints a step before running
  # it, so a log ending at "Linking" says the link STARTED, not that it
  # finished. That ambiguity cost several misdiagnoses.
  echo "NINJA_EXIT:$?" >>"$REBUILD_LOG"
  echo "ENV: USERPROFILE=${USERPROFILE:-<unset>} VERILATOR_ROOT=${VERILATOR_ROOT:-<unset>}" >>"$REBUILD_LOG"
  command -v verilator_bin >/dev/null 2>&1 && echo "verilator_bin: found" >>"$REBUILD_LOG" || echo "verilator_bin: NOT ON PATH" >>"$REBUILD_LOG"
}

# Guard 8: bare AND --random 40, the two fast ctest lanes of this binary.
run_lanes() {
  ./build-verify/tests/test_field_v3_exec_directed.exe >/dev/null 2>&1 || return 1
  ./build-verify/tests/test_field_v3_exec_directed.exe --random 40 >/dev/null 2>&1 || return 1
  return 0
}

echo "== consumer roster =="
check_consumers || exit 9

echo "== preflight =="
python tools/sweep_field_v3_exec_preflight.py || exit 8

expected=$(python "$MUT" --count) || exit 3

# EVERY FILE THE TABLE CAN MUTATE GETS A SNAPSHOT, not just $RTL.
#
# 2026-08-28: this driver snapshotted $RTL alone while the mutant table had
# grown to cover the register file too. Mutants applied to that second file
# were therefore NEVER RESTORED, and because X30/X31/X32 touch three different
# lines they ACCUMULATED -- each later mutant was scored against a file that
# still carried the earlier ones. Every result for that file was contaminated.
#
# The driver's own model-hash guard caught it at the final restore and the run
# aborted with exit 4, which is that guard doing its job. This removes the
# cause rather than relying on the guard to keep finding it.
FILES=$(python - "$MUT" <<'PY'
import importlib.util, sys
spec = importlib.util.spec_from_file_location("mut", sys.argv[1])
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
paths = {e[1] if len(e) == 4 else m.RTL for e in m.MUTANTS}
print(" ".join(sorted(paths)))
PY
) || exit 3

GOLDDIR=$(mktemp -d)
gi=0
for f in $FILES; do
  cp "$f" "$GOLDDIR/g$gi" || exit 4
  gi=$((gi + 1))
done
GOLDTMP=$(mktemp)
cp "$RTL" "$GOLDTMP" || exit 4
GOLDHASH=$(hash_of "$RTL")

restore() {
  local i
  for i in 1 2 3 4 5 6 7 8 9 10; do
    ri=0
    for f in $FILES; do
      cp "$GOLDDIR/g$ri" "$f" 2>/dev/null
      ri=$((ri + 1))
    done
    ok=1
    ri=0
    for f in $FILES; do
      cmp -s "$GOLDDIR/g$ri" "$f" || ok=0
      ri=$((ri + 1))
    done
    if [ "$ok" = "1" ] && [ "$(hash_of "$RTL")" = "$GOLDHASH" ]; then return 0; fi
    sleep 1
  done
  return 1
}

echo "== pristine baseline =="
restore || exit 4
rebuild
models_present || { echo "ABORT: pristine model did not elaborate"; exit 6; }
exes_present || { echo "ABORT: pristine target did not link"; exit 6; }
PRISTINE_MODEL=$(model_hash)
if ! run_lanes; then
  echo "ABORT: the PRISTINE tree fails its own lanes — nothing can be scored"
  exit 7
fi
echo "   pristine lanes green (model ${PRISTINE_MODEL:0:16})"

caught=0
survivors=(); equivalents=(); discards=()
k=0
while [ "$k" -lt "$expected" ]; do
  name=$(python "$MUT" --name "$k") || exit 3
  restore || exit 4
  python "$MUT" --apply "$k" >/dev/null || exit 3
  rebuild
  if ! models_present || ! exes_present; then
    echo "  $name  DISCARDED (model or exe absent after rebuild)"
    discards+=("$name")
    k=$((k + 1))
    continue
  fi
  if [ "$(model_hash)" = "$PRISTINE_MODEL" ]; then
    # An identical model has TWO possible causes and they are not the same
    # finding. rebuild() deletes the verilated model directory before every
    # mutant, so elaboration definitely RAN; an identical result therefore
    # means the mutation was semantically null -- the mutant is EQUIVALENT,
    # not unscored. The guard cannot tell that apart from a genuine failure
    # to re-elaborate on its own, so it only accepts the equivalence reading
    # when a PROOF has been written down for this mutant. Everything else
    # stays a discard and still fails the run.
    tok=${name%% *}
    if proof=$(python "$MUT" --equiv "$tok" 2>/dev/null); then
      echo "  $name  equivalent (proven, and the model is byte-identical)"
      echo "        $proof"
      equivalents+=("$name")
    else
      echo "  $name  DISCARDED (model identical to pristine — did not re-elaborate)"
      discards+=("$name")
    fi
    k=$((k + 1))
    continue
  fi
  if run_lanes; then
    tok=${name%% *}
    if proof=$(python "$MUT" --equiv "$tok" 2>/dev/null); then
      echo "  $name  equivalent (proven)"
      echo "        $proof"
      equivalents+=("$name")
    else
      echo "  $name  *** SURVIVED ***"
      survivors+=("$name")
    fi
  else
    echo "  $name  caught"
    caught=$((caught + 1))
  fi
  k=$((k + 1))
done

echo "== final restore =="
restore || exit 4
rebuild
if [ "$(model_hash)" != "$PRISTINE_MODEL" ]; then
  echo "ABORT: restored model differs from pristine — the tree is NOT clean"
  exit 4
fi
run_lanes || { echo "ABORT: restored tree fails its lanes"; exit 4; }

# THE RESTORE CANNOT VOUCH FOR ITSELF. `restore` copies back the `gold`
# snapshot taken at startup, so if the file was ALREADY mutated when that
# snapshot was taken -- because a previous run was killed and left a mutant,
# or because an orphaned run was still writing -- then "restore" faithfully
# restores the mutant, the model hash matches it, and every guard above
# agrees the tree is pristine.
#
# 2026-08-28: that is not hypothetical. A killed run left orphaned processes
# still mutating the file, and one of those mutants reached a pushed commit.
# This check comes from OUTSIDE the sweep: it asks whether any mutant's
# replacement text is present in the RTL at all.
if ! python tools/sweep_check_clean.py "$MUT"; then
  echo "ABORT: a mutant's text is still in the RTL after the final restore"
  exit 14
fi
rm -f "$GOLDTMP"
rm -rf "$GOLDDIR"

attempted=$k
accounted=$((caught + ${#survivors[@]} + ${#equivalents[@]} + ${#discards[@]}))
echo ""
echo "== tally =="
echo "  attempted:  $attempted / $expected"
echo "  caught:     $caught"
echo "  equivalent: ${#equivalents[@]}"
echo "  survived:   ${#survivors[@]}"
echo "  discarded:  ${#discards[@]}"

if [ "${#survivors[@]}" -gt 0 ] && [ -n "${survivors[0]:-}" ]; then
  echo "FAILED: ${#survivors[@]} mutant(s) survived without a proof of equivalence"
  printf '  %s\n' "${survivors[@]}"
  exit 12
fi
if [ "$attempted" != "$expected" ] || [ "$accounted" != "$expected" ]; then
  echo "CROSS-CHECK FAILED (attempted/accounted must both equal $expected)"
  exit 5
fi
if [ "${#discards[@]}" -gt 0 ]; then
  # A mutant that could not build was never SCORED. Counting it as accounted
  # and then printing SWEEP OK reports a run that tested nothing as a clean
  # sweep -- which is what this driver's 22:40 rerun did: 7 of 15 unscored,
  # tally printed, exit 0.
  echo "FAILED: ${#discards[@]} discarded mutant(s) were NOT scored -- fix and re-run"
  printf '  %s\n' "${discards[@]}"
  exit 13
fi
echo "SWEEP OK"
