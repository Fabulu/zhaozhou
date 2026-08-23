#!/usr/bin/env bash
# sweep_terrain_lod.sh -- mutation sweep for zhao_terrain_lod.sv (TERRAIN.LOD).
#
# ---------------------------------------------------------------------------
# THE SWEEP VERIFIES ITS OWN BUILDS. Every guard below was earned by a real
# failure in this repository, and `tools/sweep_geom_lod.sh` documents the first
# five at length. They are restated in one line each here, plus the SIXTH,
# which is new and is why this sweep's mutant table is a Python module:
#
#   1. `verilate()` elaborates at CONFIGURE time, so `ninja` alone relinks a
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
#      array. `zhao_terrain_lod.sv` forms its coordinate differences with
#      `$signed(...)`; inside `"..."` bash expands `$signed` to nothing, so the
#      anchor silently becomes a DIFFERENT string than the one written down.
#      The table therefore lives in `tools/sweep_terrain_lod_mutants.py` and no
#      shell ever reads a mutation. See that file's header.
#
#   7. ALSO NEW HERE, AND IT ESCAPED THE SWEEP AND BROKE main ON 2026-08-23.
#      `cmake -S . -B build` re-elaborates EVERY target that verilates the
#      mutated module, not just the ones this sweep scores. FOUR targets
#      elaborate `zhao_terrain_lod`: the two lanes below, `test_terrain_lod_tess`
#      and `test_measure_governor_lod`. An earlier version of this file deleted
#      and rebuilt only the two it scored, so every iteration left MUTANT-derived
#      model sources sitting in the other two targets' directories -- and the
#      next person to run a build compiled a mutant into the composed tests.
#      That is exactly what happened: `measure_governor_lod` failed 55 of 72
#      checks on the Duo-fairness property, reproduced here bit for bit by
#      applying M14 ("the cameras take the coarser strict decision"), against RTL
#      that was provably correct.
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
# TWO lanes, because this block's defects split cleanly between them and the
# contract's own hand-run mutation table proves it: `<` versus `<=` in the
# ladder is an exact-equality event that random input never produces and only
# `terrain_lod_directed` §2 constructs, while a transposed neighbour index shows
# up in both. A mutant is CAUGHT if either lane fails.
#
# `terrain_lod_tess` (the real LOD driving the real TESS, asserting the island
# is crack-free) is NOT in the loop, deliberately: it costs a second elaborated
# model per iteration and the contract's table shows it catching a strict SUBSET
# of what the directed lane catches -- three of its four hand-run mutations were
# green there and red in `terrain_lod_directed`. It stays in `ctest -L fast`,
# where it guards the composition rather than the arithmetic.
#
# ---------------------------------------------------------------------------
# THE SURVIVORS, PROVED RATHER THAN LABELLED
# ---------------------------------------------------------------------------
# First run, 2026-08-23, against the pre-sequencing RTL:
#   attempted=34 accounted=34 caught=31, three survivors.
#
# TWO OF THE THREE ARE EQUIVALENT MUTANTS, and here is why, so that a reader
# does not have to re-derive it:
#
#   M11 (the squared distance saturates to 2^64-2 instead of 2^64-1) is
#   equivalent because the ONLY consumer of that value is the floor square
#   root, and floor(sqrt(2^64-2)) == floor(sqrt(2^64-1)) == 4294967295:
#   4294967295^2 = 18446744065119617025 <= 2^64-2, and 4294967296^2 = 2^64.
#   The rail and one below it are the same distance. No test can tell them
#   apart because there is nothing to tell apart.
#
#   M18 (the band targets the FAR edge instead of the near one) is equivalent
#   because `want` is never used as a MAGNITUDE -- only its comparison against
#   `d_level` is. The band logic asks `want > d_level` (coarsen by exactly one
#   rung) or `want < d_level` (refine by exactly one rung), and it is a law of
#   this block that THE LADDER MOVES ONE RUNG PER FRAME. Since the strict
#   ladder is always at least as fine as the relaxed one (a larger `h` makes
#   the right-hand side larger, so more rungs pass), t_strict <= t_relaxed, and
#   swapping the two edges cannot change the SIGN of either comparison:
#     d_level < t_strict  =>  d_level < t_strict <= t_relaxed  (both coarsen)
#     d_level > t_relaxed =>  d_level > t_relaxed >= t_strict  (both refine)
#   The middle case does not mention either edge. So the two spellings emit
#   identical packets for every input.
#
# THE THIRD, M03, IS NOT EQUIVALENT. IT IS A REAL HOLE, AND IT IS NOW CLOSED.
# `rhs + 1` differs from `rhs` only when `dev*scale == dist*h + 1` exactly.
# `terrain_lod_directed` §2 pins the flip point at dev = distance-1, distance
# and distance+1 -- but it does so with scale = h = 256, where BOTH sides are
# multiples of 256 and an off-by-one on the right can never be reached. Random
# input finds an exact 40-bit equality with probability ~2^-40, i.e. never.
# Section 2 therefore grew a case with an ODD scale that constructs the
# equality by hand (dist = 255, scale = 65281, dev = 1, so dev*scale = 65281
# and dist*256 = 65280). That is the same argument the contract already makes
# for `<` versus `<=`: an exact-equality event has to be built, not sampled.
#
# EXPECTED: two survivors, M11 and M18, both proved above.
#
# ---------------------------------------------------------------------------
# WHAT SEQUENCING CHANGED HERE, 2026-08-23
# ---------------------------------------------------------------------------
# The block's thirty products now walk through ONE 32x32 unsigned multiplier
# over fourteen clocks instead of standing side by side (28 DSPs -> 3). The
# sweep grew 34 -> 40 with the code, because a sweep that did not grow would
# report the same score for strictly less coverage.
#
# TEN MUTANTS MOVED, and where they landed says what the restructuring did:
#   M01  the twelve ladder comparisons are now TWO, so `<=` lands on the one
#        shared strict comparison
#   M02  `dev` reaches the multiplier through an OPERAND MUX, not an expression
#   M03  the strict right-hand side is a SHIFT now, so an off-by-one there is a
#        mutation of `{9'b0, ev_dst, 8'd0}` rather than of a product
#   M04  "coarsest wins" is now "the LAST passing rung to be written wins",
#        because the rungs are walked 1, 2, 3 in TIME -- so keeping the first
#        write is exactly the finest-wins defect, re-spelled
#   M05  re-aimed onto the deviation mux: a rung can now silently borrow
#        another rung's operand, which is the sequencer's own failure mode and
#        the terrain twin of the geom pilot's M20/M21/M22
#   M06  the strict `h` is a shift amount, not an argument
#   M12  the squared distance is a six-step schedule, so "wrong axis" became
#        "a step reaches for the wrong coordinate"
#   M13  follows `diff33` into `absdiff32`
#   M14/M15  the ladder answers are REGISTERS now, not combinational wires
#
# SIX ARE NEW, because before the restructuring there was no schedule to get
# wrong: a ladder answer latched into the wrong flop (M35), the squaring phase
# ending a step early (M36), the accumulator not cleared between the two eyes
# (M37), the evaluation phase ending a step early (M38), a relaxed right-hand
# side filed under the wrong camera (M39), and the ladder starting at level 1
# instead of level 0 (M40). ALL SIX ARE CAUGHT.
#
# SCORE AFTER: attempted=40 accounted=40 caught=38, survivors M11 and M18 --
# the same two, and only those two.
# ---------------------------------------------------------------------------
set -u

RTL=fpga/rtl/terrain/zhao_terrain_lod.sv

# EVERY target that verilates this module. If you add one in tests/CMakeLists.txt
# you MUST add it here -- see guard 7. Checked against the build system below.
TARGETS="test_terrain_lod_directed test_terrain_lod_random test_terrain_lod_tess test_measure_governor_lod"

GOLD=$(mktemp)
cp "$RTL" "$GOLD"
GOLDHASH=$(sha256sum <"$GOLD" | cut -d' ' -f1)

model_hash() {
  local t h=""
  for t in $TARGETS; do
    h="$h$(find "build/tests/CMakeFiles/$t.dir/Vzhao_terrain_lod.dir" -type f \
             \( -name "*.cpp" -o -name "*.h" \) 2>/dev/null \
           | sort | xargs sha256sum 2>/dev/null | sha256sum | cut -d" " -f1)"
  done
  printf '%s' "$h" | sha256sum | cut -d" " -f1
}

models_present() {
  local t
  for t in $TARGETS; do
    [ -d "build/tests/CMakeFiles/$t.dir/Vzhao_terrain_lod.dir" ] || return 1
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

run_lanes() {
  local t
  for t in $TARGETS; do
    "./build/tests/$t.exe" >/dev/null 2>&1 || return 1
  done
  return 0
}

# GUARD 7's OWN CHECK: refuse to start if the build system knows about a consumer
# of this module that TARGETS does not. Otherwise the next target someone adds
# silently reverts this file to the behaviour that broke main.
check_consumers() {
  local declared found missing=""
  declared=$(grep -B12 "TOP_MODULE zhao_terrain_lod" tests/CMakeLists.txt \
             | grep -oE "verilate\(test_[a-z_]+" | sed 's/verilate(//' | sort -u)
  for found in $declared; do
    case " $TARGETS " in
      *" $found "*) ;;
      *) missing="$missing $found" ;;
    esac
  done
  if [ -n "$missing" ]; then
    echo "ABORT: tests/CMakeLists.txt elaborates zhao_terrain_lod into target(s)"
    echo "      ,$missing, which TARGETS does not list. Every consumer must be"
    echo "       cleaned and rebuilt or the sweep leaves mutant model sources on"
    echo "       disk for someone else's build to compile. See guard 7."
    return 1
  fi
  return 0
}

check_consumers || exit 9

# PREFLIGHT: EVERY MUTANT MUST LINT BEFORE ANY OF THEM IS SCORED.
python tools/sweep_terrain_lod_preflight.py || {
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

expected=$(python tools/sweep_terrain_lod_mutants.py --count)
attempted=0
accounted=0
caught=0
survivors=()

k=0
while [ "$k" -lt "$expected" ]; do
  name=$(python tools/sweep_terrain_lod_mutants.py --name "$k")
  attempted=$((attempted + 1))

  restore || { echo "  $name  ABORT: could not restore before applying"; exit 4; }

  if ! python tools/sweep_terrain_lod_mutants.py --apply "$k"; then
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
    echo "  $name  *** SURVIVED ***"
    survivors+=("$name")
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
echo "attempted=$attempted expected=$expected accounted=$accounted caught=$caught"
for s in "${survivors[@]:-}"; do [ -n "$s" ] && echo "SURVIVOR: $s"; done
if [ "$attempted" != "$expected" ] || [ "$accounted" != "$expected" ]; then
  echo "CROSS-CHECK FAILED (attempted/accounted must both equal $expected)"
  exit 5
fi
