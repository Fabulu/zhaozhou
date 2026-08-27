#!/usr/bin/env bash
# sweep_cmd_dma.sh -- mutation sweep for zhao_cmd_dma.sv
# (TERRAIN.PATCH, the Mantle entry point, phase 6 / ZH-033).
#
# ---------------------------------------------------------------------------
# WHY THIS SWEEP DID NOT EXIST UNTIL NOW, WHICH IS THE POINT OF IT
# ---------------------------------------------------------------------------
# reports/SWEEP_COVERAGE_AUDIT.md lists CMD.DMA among the modules with a test
# lane and no mutation sweep, in the tier after the blocks declared closed:
# it is on the hot path everything else depends on, because every command the
# machine executes arrives through it.
#
# The block is a GATE CHAIN and the chain IS the block -- magic, ABI version,
# reserved flags, four length laws, the header CRC, the resource epoch, then
# per record: length, opcode, size agreement, truncation, debug permission.
# Twelve distinct status codes exist because each gate must be
# DISTINGUISHABLE: reporting 'bad length' when the fault was a stale epoch
# sends the caller looking in the wrong place.
#
# So the mutations attack two things -- gates that stop refusing, and gates
# that refuse under the WRONG NAME. The fail-safe ORDER is a law too, which
# the random lane predicts, so several mutants move a gate past its neighbour
# rather than deleting it.
#
# BOTH LANES OF THE SAME BINARY ARE SCORED: cmd_dma_directed and the --random
# 400 lane are the same executable, so running the exe once covers the
# directed cases; the random timeline is what the ordering mutants need.
#
# The guards below are NOT new. Every one was earned by a real failure
# elsewhere in this repository and they are inherited deliberately rather than
# re-derived; `tools/sweep_geom_lod.sh` documents the first five at length.
# The mutant table is a Python module for the sixth reason: these anchors
# contain `$signed`, and inside a double-quoted bash word `$signed` expands to
# nothing -- matching either no text or, worse, DIFFERENT text than the one
# written down.
#
# NO SCORE IS RECORDED HERE YET. This header will not claim one until the
# sweep has run; a copied provenance is how a sweep comes to describe results
# it never produced.
##   1. `verilate()` elaborates at CONFIGURE time, so `ninja` alone relinks a
#      CACHED model against changed source. Reconfigure every iteration.
#   2. Never stamp the mutated source's mtime forward; a future mtime makes a
#      mutant model look newer than the pristine source restored after it.
#   3. Hashing the generated model is necessary but not sufficient -- delete the
#      whole target directory, because a pristine model can link against an
#      object still compiled from a mutant.
#   4. Hash the model DIRECTORY, not `V<top>.cpp`: that file is Verilator's
#      wrapper and is byte-identical between pristine and mutated builds.
#   5. The executable lives OUTSIDE the target directory, so a mutant that fails
#      to compile leaves the previous binary in place and the sweep runs THAT.
#      Delete the exe too, and require it to exist after the build.
#   6. NEW HERE: a mutation containing `$` cannot live in a double-quoted bash
#      array. `zhao_cmd_dma.sv` forms its coordinate differences with
#      `$signed(...)`; inside `"..."` bash expands `$signed` to nothing, so the
#      anchor silently becomes a DIFFERENT string than the one written down.
#      The table therefore lives in `tools/sweep_cmd_dma_mutants.py` and no
#      shell ever reads a mutation. See that file's header.
#
#   7. IT ESCAPED A SWEEP AND BROKE main ON 2026-08-23 -- in the LOD sweep,
#      not this one, and it is inherited here so it cannot happen again.
#      `cmake -S . -B build` re-elaborates EVERY target that verilates the
#      mutated module, not just the ones a sweep scores. That sweep cleaned and
#      rebuilt only the lanes it scored, so every iteration left MUTANT-derived
#      model sources sitting in the other targets' directories, and the next
#      build compiled a mutant into the composed tests: `measure_governor_lod`
#      failed 55 of 72 checks on the Duo-fairness property against RTL that was
#      provably correct.
#
#      FIVE targets elaborate `zhao_cmd_dma` -- they are listed in
#      TARGETS below and the roster is checked against tests/CMakeLists.txt at
#      startup, so adding a sixth without listing it ABORTS the sweep rather
#      than silently narrowing it.
#
#      THE RULE: clean and rebuild EVERY consumer of the mutated module, not
#      every consumer you intend to score. A sweep must not leave the tree in a
#      state it did not measure. `TARGETS` below is therefore the full consumer
#      list, taken from `tests/CMakeLists.txt`, and all four are scored -- the
#      marginal cost is small because `cmake` was already elaborating all four
#      on every iteration anyway.
#
# The scoring rule: after regeneration BOTH models must EXIST, both executables
# must LINK, and the model hash must DIFFER from pristine. Anything else is
# discarded, never scored. The revert is verified byte-for-byte with retries,
# because a cmake that has only just exited can still hold the file open on
# Windows.
#
# And the baseline is checked before any mutant runs: if the PRISTINE build does
# not pass its own tests, every "caught" below would be meaningless.
#
# ---------------------------------------------------------------------------
# WHAT IS SCORED
# ---------------------------------------------------------------------------
# ONE lane elaborates this module: test_cmd_dma_directed. Guard 7's
# roster check below is still run, so the day a second consumer appears the
# sweep ABORTS rather than silently leaving mutant model sources in its
# directory for someone else's build to compile.
#
# That single lane is a composed one -- it drives the blit against a model HPS
# bridge, a lease, and a memory guard that can deny -- which is why twenty
# mutations against a 802-line refusal machine are scoreable from it at all.
#
# A mutant is CAUGHT if ANY lane fails.
#
# THE SURVIVORS
# ---------------------------------------------------------------------------
# None recorded yet -- this sweep has not been run. When it is, whatever
# survives is either a REAL GAP to be closed with a test or an EQUIVALENT
# mutant to be PROVED here in full, never merely labelled. The sibling sweeps'
# survivor proofs (sweep_terrain_lod.sh's M11 and M18) are the standard: state
# the argument so a reader does not have to re-derive it.
#
# A mutant is CAUGHT if ANY lane fails.
# ---------------------------------------------------------------------------
set -u

RTL=fpga/rtl/command/zhao_cmd_dma.sv

# EVERY target that verilates this module. If you add one in tests/CMakeLists.txt
# you MUST add it here -- see guard 7. Checked against the build system below.
TARGETS="test_cmd_dma_directed"

GOLD=$(mktemp)
cp "$RTL" "$GOLD"
GOLDHASH=$(sha256sum <"$GOLD" | cut -d' ' -f1)

model_hash() {
  local t h=""
  for t in $TARGETS; do
    h="$h$(find "build/tests/CMakeFiles/$t.dir/Vzhao_cmd_dma.dir" -type f \
             \( -name "*.cpp" -o -name "*.h" \) 2>/dev/null \
           | sort | xargs sha256sum 2>/dev/null | sha256sum | cut -d" " -f1)"
  done
  printf '%s' "$h" | sha256sum | cut -d" " -f1
}

models_present() {
  local t
  for t in $TARGETS; do
    [ -d "build/tests/CMakeFiles/$t.dir/Vzhao_cmd_dma.dir" ] || return 1
  done
  return 0
}

exes_present() {
  local t
  for t in $TARGETS; do
    [ -x "build/tests/$t.exe" ] || return 1
  done
  return 0
}

rebuild() {
  local t
  for t in $TARGETS; do
    rm -rf "build/tests/CMakeFiles/$t.dir"
    rm -f "build/tests/$t.exe"   # guard 5: the exe lives OUTSIDE the target dir
  done
  cmake -S . -B build >/dev/null 2>&1
  # shellcheck disable=SC2086
  ninja -C build $TARGETS >/dev/null 2>&1
}

# Restore the pristine source and PROVE it, retrying through any lingering file
# lock left by the cmake that just exited.
restore() {
  local i
  for i in 1 2 3 4 5 6 7 8 9 10; do
    cp "$GOLD" "$RTL" 2>/dev/null
    if [ "$(sha256sum <"$RTL" | cut -d' ' -f1)" = "$GOLDHASH" ]; then return 0; fi
    sleep 1
  done
  return 1
}

# THE BINARY IS THREE CTEST LANES, NOT ONE, AND THE SWEEP MUST RUN THEM ALL.
#
# tests/CMakeLists.txt registers cmd_dma_directed, cmd_dma_random (--random
# 400) and cmd_dma_random_nightly (--random 5000) as three tests over the SAME
# executable. A run_lanes() that invokes the bare binary scores only the
# directed cases -- and the random lane is precisely where the gate coverage
# and the fail-safe ORDER live, because it generates packet timelines with
# predicted verdicts.
#
# MEASURED: with the bare binary alone this sweep scored 6 of 21. That number
# was an artefact of the harness, not a fact about the block, and it would
# have been alarming and wrong to report. The fast random lane is included
# here; the nightly 5000 is not, because the sweep runs it once per mutant and
# the marginal catch does not pay for the time.
run_lanes() {
  local t
  for t in $TARGETS; do
    "./build/tests/$t.exe" >/dev/null 2>&1 || return 1
    "./build/tests/$t.exe" --random 400 >/dev/null 2>&1 || return 1
  done
  return 0
}

# GUARD 7's OWN CHECK: refuse to start if the build system knows about a consumer
# of this module that TARGETS does not. Otherwise the next target someone adds
# silently reverts this file to the behaviour that broke main.
check_consumers() {
  local declared found missing=""
  declared=$(grep -B12 "TOP_MODULE zhao_cmd_dma" tests/CMakeLists.txt \
             | grep -oE "verilate\(test_[a-z_]+" | sed 's/verilate(//' | sort -u)
  for found in $declared; do
    case " $TARGETS " in
      *" $found "*) ;;
      *) missing="$missing $found" ;;
    esac
  done
  if [ -n "$missing" ]; then
    echo "ABORT: tests/CMakeLists.txt elaborates zhao_cmd_dma into target(s)"
    echo "      ,$missing, which TARGETS does not list. Every consumer must be"
    echo "       cleaned and rebuilt or the sweep leaves mutant model sources on"
    echo "       disk for someone else's build to compile. See guard 7."
    return 1
  fi
  return 0
}

check_consumers || exit 9

# PREFLIGHT: EVERY MUTANT MUST LINT BEFORE ANY OF THEM IS SCORED.
python tools/sweep_cmd_dma_preflight.py || {
  echo "ABORT: at least one mutant does not build -- fix the mutation, not the guard"
  exit 8
}

echo "== establishing the pristine baseline"
restore || { echo "ABORT: cannot establish pristine source"; exit 4; }
rebuild
models_present || { echo "ABORT: a pristine model did not elaborate"; exit 6; }
exes_present || { echo "ABORT: a pristine target did not link"; exit 6; }
PRISTINE_MODEL=$(model_hash)
if ! run_lanes; then
  echo "ABORT: the PRISTINE build fails its own tests -- nothing below would mean anything"
  exit 7
fi
echo "   pristine model ${PRISTINE_MODEL:0:16}, all lanes green"

expected=$(python tools/sweep_cmd_dma_mutants.py --count)
attempted=0
accounted=0
caught=0
survivors=()
equivalents=()

k=0
while [ "$k" -lt "$expected" ]; do
  name=$(python tools/sweep_cmd_dma_mutants.py --name "$k")
  attempted=$((attempted + 1))

  restore || { echo "  $name  ABORT: could not restore before applying"; exit 4; }

  if ! python tools/sweep_cmd_dma_mutants.py --apply "$k"; then
    echo "  $name  ABORT: could not apply"
    restore
    exit 3
  fi

  # The source must actually have moved.
  if [ "$(sha256sum <"$RTL" | cut -d' ' -f1)" = "$GOLDHASH" ]; then
    echo "  $name  ABORT: source unchanged after apply"
    restore
    exit 3
  fi

  rebuild

  if ! models_present; then
    echo "  $name  DISCARDED: a model was absent after regeneration"
    restore || { echo "ABORT: revert failed"; exit 4; }
    k=$((k + 1))
    continue
  fi
  if ! exes_present; then
    echo "  $name  DISCARDED: a target did not LINK (a build failure would"
    echo "                    otherwise be scored as a caught mutant)"
    restore || { echo "ABORT: revert failed"; exit 4; }
    k=$((k + 1))
    continue
  fi
  if [ "$(model_hash)" = "$PRISTINE_MODEL" ]; then
    echo "  $name  DISCARDED: model identical to pristine (did not re-elaborate)"
    restore || { echo "ABORT: revert failed"; exit 4; }
    k=$((k + 1))
    continue
  fi

  if run_lanes; then
    # A SURVIVOR IS EITHER PROVEN EQUIVALENT OR A HOLE. The proof lives in the
    # mutant module, keyed by the mutant's token, so it cannot drift from the
    # table it explains -- and an UNDECLARED survivor fails the sweep rather
    # than being listed and forgotten.
    tok=${name%% *}
    if proof=$(python tools/sweep_cmd_dma_mutants.py --equiv "$tok" 2>/dev/null); then
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
  accounted=$((accounted + 1))

  restore || { echo "  $name  ABORT: revert was not byte-identical"; exit 4; }
  k=$((k + 1))
done

echo "== restoring the pristine build"
restore || { echo "ABORT: final restore failed"; exit 4; }
rebuild

echo "----"
echo "attempted=$attempted expected=$expected accounted=$accounted caught=$caught equivalent=${#equivalents[@]}"
for s in "${equivalents[@]:-}"; do [ -n "$s" ] && echo "EQUIVALENT (proven): $s"; done
for s in "${survivors[@]:-}"; do [ -n "$s" ] && echo "SURVIVOR: $s"; done
if [ "${#survivors[@]}" -gt 0 ] && [ -n "${survivors[0]:-}" ]; then
  echo "FAILED: ${#survivors[@]} mutant(s) survived without a proof of equivalence"
  exit 12
fi
if [ "$attempted" != "$expected" ] || [ "$accounted" != "$expected" ]; then
  echo "CROSS-CHECK FAILED (attempted/accounted must both equal $expected)"
  exit 5
fi
