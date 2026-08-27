#!/usr/bin/env bash
# sweep_field_plan.sh — mutation sweep for the FIELD v3 exact software planner
# (reference/src/zfield/zfield_plan.cpp + zfield_steps.hpp; Phase 2 of
# reports/Fieldv3.md).
#
# Modeled on tools/sweep_geom_wcache.sh / sweep_cmd_dma.sh and carries their
# earned guards, translated to a C++ (non-Verilator) seam:
#   - the mutant table lives in Python (guard 6: bash mangles $ and quotes);
#   - preflight proves every mutant COMPILES before anything is scored
#     (a non-compiling mutant scored as caught is flattering nonsense);
#   - a mutant whose object file hashes IDENTICAL to pristine after rebuild
#     is DISCARDED, never scored (stale-binary trap, TERRAIN.PATCH.md);
#   - guard 8: the fplan binary is THREE ctest lanes (bare, --random 40,
#     nightly --random 600); the sweep runs every FAST lane. The nightly lane
#     is excluded by the cmd_dma precedent (marginal catch vs. time).
#   - the sweep ALSO runs test_field_crater_ring: a steps-layer mutant hits
#     interpret() and the executor IDENTICALLY, so the differential alone is
#     blind to it — the committed golden .zvec is the oracle that is not.
#   - a survivor without a machine-readable equivalence proof FAILS the sweep.
#
# Exit codes: 3 apply, 4 restore, 5 cross-check, 6 pristine build,
#             7 pristine tests red, 8 preflight, 12 undeclared survivor.
set -u
cd "$(dirname "$0")/.." || exit 1

MUT=tools/sweep_field_plan_mutants.py
PLAN_OBJ="build/reference/CMakeFiles/zhao_zref.dir/src/zfield/zfield_plan.cpp.obj"
TARGETS="test_field_fplan_diff test_field_crater_ring"

hash_of() { sha256sum <"$1" | cut -d' ' -f1; }

rebuild() {
  # shellcheck disable=SC2086
  ninja -C build $TARGETS >/dev/null 2>&1
}

run_lanes() {
  ./build/tests/test_field_fplan_diff.exe >/dev/null 2>&1 || return 1
  ./build/tests/test_field_fplan_diff.exe --random 40 >/dev/null 2>&1 || return 1
  ./build/tests/test_field_crater_ring.exe >/dev/null 2>&1 || return 1
  return 0
}

exes_present() {
  local t
  for t in $TARGETS; do
    [ -x "build/tests/$t.exe" ] || return 1
  done
  return 0
}

# a red crater replay may write failure vectors; keep the tree clean per run
scrub_captures() {
  git checkout -- captures/golden/field 2>/dev/null
  git clean -fq captures/failures/field 2>/dev/null
}

echo "== preflight =="
python tools/sweep_field_plan_preflight.py || exit 8

expected=$(python "$MUT" --count) || exit 3

# snapshot the pristine sources
declare -A GOLD GOLDHASH
files=$(for i in $(seq 0 $((expected - 1))); do python "$MUT" --file "$i"; done | sort -u)
tmpdir=$(mktemp -d)
for f in $files; do
  key=$(echo "$f" | tr '/.' '__')
  cp "$f" "$tmpdir/$key" || exit 4
  GOLD[$f]="$tmpdir/$key"
  GOLDHASH[$f]=$(hash_of "$f")
done

restore() {
  local f i
  for f in $files; do
    for i in 1 2 3 4 5; do
      cp "${GOLD[$f]}" "$f" 2>/dev/null
      [ "$(hash_of "$f")" = "${GOLDHASH[$f]}" ] && continue 2
      sleep 1
    done
    return 1
  done
  return 0
}

echo "== pristine baseline =="
restore || exit 4
rebuild
exes_present || { echo "ABORT: pristine build did not produce the exes"; exit 6; }
PRISTINE_OBJ=$(hash_of "$PLAN_OBJ")
if ! run_lanes; then
  echo "ABORT: the PRISTINE tree fails its own lanes — nothing can be scored"
  exit 7
fi
scrub_captures
echo "   pristine lanes green (plan obj ${PRISTINE_OBJ:0:12})"

caught=0
survivors=(); equivalents=(); discards=()
k=0
while [ "$k" -lt "$expected" ]; do
  name=$(python "$MUT" --name "$k") || exit 3
  restore || exit 4
  python "$MUT" --apply "$k" >/dev/null || exit 3
  rebuild
  if ! exes_present; then
    echo "  $name  DISCARDED (exe did not build)"
    discards+=("$name")
    k=$((k + 1))
    continue
  fi
  if [ "$(hash_of "$PLAN_OBJ")" = "$PRISTINE_OBJ" ]; then
    echo "  $name  DISCARDED (object identical to pristine — did not recompile)"
    discards+=("$name")
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
  scrub_captures
  k=$((k + 1))
done

echo "== final restore =="
restore || exit 4
rebuild
if [ "$(hash_of "$PLAN_OBJ")" != "$PRISTINE_OBJ" ]; then
  echo "ABORT: restored object differs from pristine — the tree is NOT clean"
  exit 4
fi
run_lanes || { echo "ABORT: restored tree fails its lanes"; exit 4; }
scrub_captures

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
  # sweep -- which is what the 2026-08-27 22:40 patch-accumulator rerun did:
  # 7 of 15 unscored, tally printed, exit 0.
  echo "FAILED: ${#discards[@]} discarded mutant(s) were NOT scored -- fix and re-run"
  printf '  %s\n' "${discards[@]}"
  exit 13
fi
echo "SWEEP OK"
