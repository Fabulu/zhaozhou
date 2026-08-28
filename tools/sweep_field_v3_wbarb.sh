#!/usr/bin/env bash
# sweep_field_v3_wbarb.sh - mutation sweep for FIELD.V3.WBARB
# (fpga/rtl/field/zhao_field_v3_wbarb.sv): the writeback arbiter -- merge several
# streams onto the register file's single write port.
#
# Inherits the house guards, in the shape the executor sweep left them:
#   the mutant table is Python -- bash mangles $ and quotes;
#   preflight -- every mutant LINTS before anything is scored, and a mutant
#     that cannot build is a DISCARD rather than a flattering "caught";
#   rebuild deletes the model dir AND the exe, which lives outside it;
#   discard -- model hash identical to pristine = did not re-elaborate;
#   consumer roster -- read from SOURCES, not TOP_MODULE, because the day a
#     block becomes a submodule the TOP_MODULE grep returns an EMPTY roster
#     and the sweep aborts naming nothing;
#   the model directory comes from the verilate() PREFIX, because hashing a
#     directory the mutation cannot reach would pass every mutant as "changed"
#     while scoring a model that never moved.
#
# Exit codes: 3 apply, 4 restore, 5 cross-check, 6 pristine build,
#             7 pristine tests red, 8 preflight, 9 consumer roster,
#             12 undeclared survivor.
#
set -u
cd "$(dirname "$0")/.." || exit 1

MUT=tools/sweep_field_v3_wbarb_mutants.py
RTL=fpga/rtl/field/zhao_field_v3_wbarb.sv
TARGETS="test_field_v3_wbarb_directed"

# An ABSOLUTE path inside the repo, not /tmp. Git-for-Windows bash and the
# msys bash map /tmp to DIFFERENT directories, so a log written by a detached
# runner was invisible to the shell reading it -- which is why the rebuild
# diagnostics appeared to be missing entirely when they were simply somewhere
# else.
# THE BUILD DIRECTORY IS A KNOB, so two sweeps can run at once in trees of
# their own. Separate build directories have separate caches and the source
# tree is only read, so concurrent sweeps in DIFFERENT trees are fine -- what
# breaks is two writers in the SAME tree, because a sweep deletes its target's
# model directory and exe before every rebuild.
#
# (An earlier note here claimed no build could run anywhere during a sweep.
# That was inferred from a failure later explained by ccache, and is wrong.)
BUILD_DIR="${BUILD_DIR:-build-verify}"
REBUILD_LOG="${REBUILD_LOG:-$(pwd)/runs/CLAUDE-RUNS/sweep_rebuild_${BUILD_DIR}.log}"

hash_of() { sha256sum <"$1" | cut -d' ' -f1; }

# CONSUMERS ARE READ FROM SOURCES, NOT FROM TOP_MODULE.
#
# This guard used to be `grep -B12 "TOP_MODULE <module>"`, which was correct
# only while every swept block was its own top. The DOT fix composed this
# executor into zhao_probe_v3_engine, the grep found nothing, and the sweep
# aborted naming an EMPTY roster. The guard was right to refuse -- it could no
# longer see what it was guarding.
#
# The rule is now: every target this sweep RUNS must really elaborate the
# file, and every OTHER target that elaborates it must be declared here with a
# reason. A consumer that is neither run nor declared is the hole the guard
# exists to find.
UNRUN_CONSUMERS="${UNRUN_CONSUMERS:-}"

check_consumers() {
  local declared t c
  declared=$(python tools/sweep_consumers.py "$RTL") || {
    echo "ABORT: no verilate() target elaborates $RTL"
    return 1
  }
  for t in $TARGETS; do
    echo "$declared" | grep -qx "$t" || {
      echo "ABORT: this sweep runs $t, which does not elaborate $RTL"
      return 1
    }
  done
  for c in $declared; do
    case " $TARGETS $UNRUN_CONSUMERS " in
      *" $c "*) ;;
      *)
        echo "ABORT: $c also elaborates $RTL and is neither run by this sweep"
        echo "       nor declared in UNRUN_CONSUMERS -- its models would carry"
        echo "       mutant-derived code that nothing scores."
        return 1 ;;
    esac
  done
  return 0
}

# THE MODEL DIRECTORY IS NAMED FOR THE TOP, NOT FOR THE MUTATED FILE.
#
# This driver hardcoded `Vzhao_field_v3_wbarb.dir` for both the presence check
# and the binary-hash discard check. Once the executor became a submodule of
# zhao_probe_v3_engine that directory stopped existing, and the sweep aborted
# with "pristine model did not elaborate" over a build that had linked
# cleanly.
#
# The abort was the harmless outcome. The discard check hashes this directory
# to prove a mutant really re-elaborated -- so if a directory of that name had
# happened to exist, every mutant would have passed the check as "changed"
# while the sweep scored a model the mutation never touched. Full marks over
# nothing at all.
#
# It is read from the verilate() PREFIX now, so it cannot drift again.
MODEL=$(python tools/sweep_consumers.py --prefix "$(echo $TARGETS | cut -d" " -f1)") || {
  echo "ABORT: cannot resolve the verilate PREFIX for $TARGETS"
  exit 9
}

model_hash() {
  local t h=""
  for t in $TARGETS; do
    h="$h$(find "$BUILD_DIR/tests/CMakeFiles/$t.dir/${MODEL}.dir" -type f \
             \( -name "*.cpp" -o -name "*.h" \) 2>/dev/null \
           | sort | xargs sha256sum 2>/dev/null | sha256sum | cut -d" " -f1)"
  done
  echo "$h"
}

models_present() {
  local t
  for t in $TARGETS; do
    [ -d "$BUILD_DIR/tests/CMakeFiles/$t.dir/${MODEL}.dir" ] || return 1
  done
  return 0
}

exes_present() {
  local t
  for t in $TARGETS; do
    [ -x "$BUILD_DIR/tests/$t.exe" ] || return 1
  done
  return 0
}

rebuild() {
  local t
  for t in $TARGETS; do
    rm -rf "$BUILD_DIR/tests/CMakeFiles/$t.dir"
    # guard 5: the exe lives OUTSIDE the target dir, so it must be deleted
    # too -- and it must be deleted from THIS sweep's own build dir. This
    # line said "build/tests" until 2026-08-27, which both left the real
    # stale exe in place (defeating the guard) and deleted another
    # session's binary out of the shared tree.
    rm -f "$BUILD_DIR/tests/$t.exe"
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
  # -DOBJCACHE_ENABLED=OFF: Verilator's own CMake auto-detects ccache, and
  # ccache refuses to run without USERPROFILE, which a DETACHED process does
  # not inherit. Four sweeps aborted on "pristine target did not link" whose
  # real cause was `ccache: error: The USERPROFILE environment variable must
  # be set` -- ninja exited 1 and produced no binary.
  #
  # Setting USERPROFILE in the detached process did not work through any of
  # inheritance, an inline export, or derivation from HOME. Turning the
  # object cache OFF removes the dependency rather than negotiating with it.
  # A sweep rebuilds one target dozens of times, so losing the cache costs
  # real time -- but a sweep that cannot build costs all of it.
  cmake -S . -B $BUILD_DIR -G Ninja -DCMAKE_BUILD_TYPE=Release     -DCMAKE_CXX_COMPILER=C:/programmieren/dsstuff/mingw64/bin/g++.exe     -DCMAKE_MAKE_PROGRAM=C:/programmieren/dsstuff/mingw64/bin/ninja.exe     -DOBJCACHE_ENABLED=OFF     >"$REBUILD_LOG" 2>&1
  echo "CMAKE_EXIT:$?" >>"$REBUILD_LOG"
  # shellcheck disable=SC2086
  ninja -C $BUILD_DIR $TARGETS >>"$REBUILD_LOG" 2>&1
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
# Guard 8 says run every FAST ctest lane of this binary. THIS BINARY HAS ONE.
# The clone this driver came from has a --random lane; the dispatcher does not,
# because it computes no answer and there is nothing to randomise against -- the
# properties are exhaustive over the shapes that exist (widths 1, 2 and 3, group
# sizes 1 to 4). Running `--random 40` here would silently re-run the directed
# suite and let the driver claim two lanes where there is one.
run_lanes() {
  ./$BUILD_DIR/tests/test_field_v3_wbarb_directed.exe >/dev/null 2>&1 || return 1
  return 0
}

echo "== consumer roster =="
check_consumers || exit 9

echo "== preflight =="
python tools/sweep_field_v3_wbarb_preflight.py || exit 8

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
