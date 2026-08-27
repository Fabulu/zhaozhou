#!/usr/bin/env bash
# sweep_field_ctx_fifo.sh — mutation sweep for the FIELD v3 ready-context
# FIFO scheduler probe (fpga/rtl/synth/zhao_probe_ctx_fifo.sv; Phase 3
# probe of reports/Fieldv3.md).
#
# Inherits the house guards (sweep_geom_wcache.sh / sweep_cmd_dma.sh):
#   6  the mutant table is Python — bash mangles $ and quotes;
#   preflight — every mutant LINTS before anything is scored;
#   5  rebuild deletes the model dir AND the exe (it lives outside the dir);
#   discard — model hash identical to pristine = did not re-elaborate;
#   7  consumer roster — the probe must be elaborated by exactly the targets
#      this sweep runs, or mutant-derived models could survive in unscored
#      consumers;
#   8  the binary is THREE ctest lanes (bare, --random 60, nightly 800);
#      the sweep runs every FAST lane; the nightly is excluded by the
#      cmd_dma precedent.
#
# Exit codes: 3 apply, 4 restore, 5 cross-check, 6 pristine build,
#             7 pristine tests red, 8 preflight, 9 consumer roster,
#             12 undeclared survivor.
set -u
cd "$(dirname "$0")/.." || exit 1

MUT=tools/sweep_field_ctx_fifo_mutants.py
RTL=fpga/rtl/synth/zhao_probe_ctx_fifo.sv
TARGETS="test_field_ctx_fifo_directed"

hash_of() { sha256sum <"$1" | cut -d' ' -f1; }

check_consumers() {
  local declared
  declared=$(grep -B12 "TOP_MODULE zhao_probe_ctx_fifo" tests/CMakeLists.txt \
             | grep -oE "verilate\(test_[a-z_0-9]+" | sed 's/verilate(//' | sort -u)
  if [ "$declared" != "$TARGETS" ]; then
    echo "ABORT: tests/CMakeLists.txt elaborates zhao_probe_ctx_fifo into target(s):"
    echo "$declared"
    echo "but this sweep runs: $TARGETS — update TARGETS or the roster."
    return 1
  fi
  return 0
}

model_hash() {
  local t h=""
  for t in $TARGETS; do
    h="$h$(find "build/tests/CMakeFiles/$t.dir/Vzhao_probe_ctx_fifo.dir" -type f \
             \( -name "*.cpp" -o -name "*.h" \) 2>/dev/null \
           | sort | xargs sha256sum 2>/dev/null | sha256sum | cut -d" " -f1)"
  done
  echo "$h"
}

models_present() {
  local t
  for t in $TARGETS; do
    [ -d "build/tests/CMakeFiles/$t.dir/Vzhao_probe_ctx_fifo.dir" ] || return 1
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
    rm -f "build/tests/$t.exe"  # guard 5: the exe lives OUTSIDE the target dir
  done
  # deleting a verilated target dir removes files only CONFIGURE regenerates
  # (the house sweeps learned this the same way); VERILATOR_ROOT must be set
  # or the configure fails and ninja silently uses the OLD source list.
  export VERILATOR_ROOT="${VERILATOR_ROOT:-C:/programmieren/zencrifice/.tools/oss-cad-suite/share/verilator}"
  # BUILD.md: three cmakes are on PATH and the msys2 one reports MSYS, which
  # disables every preset and leaves the OLD build.ninja in place. Pin it.
  export PATH="/c/programmieren/dsstuff/mingw64/bin:$PATH"
  cmake -S . -B build >/dev/null 2>&1
  # shellcheck disable=SC2086
  ninja -C build $TARGETS >/dev/null 2>&1
}

# Guard 8: bare AND --random 60, the two fast ctest lanes of this binary.
run_lanes() {
  ./build/tests/test_field_ctx_fifo_directed.exe >/dev/null 2>&1 || return 1
  ./build/tests/test_field_ctx_fifo_directed.exe --random 60 >/dev/null 2>&1 || return 1
  return 0
}

echo "== consumer roster =="
check_consumers || exit 9

echo "== preflight =="
python tools/sweep_field_ctx_fifo_preflight.py || exit 8

expected=$(python "$MUT" --count) || exit 3

GOLDTMP=$(mktemp)
cp "$RTL" "$GOLDTMP" || exit 4
GOLDHASH=$(hash_of "$RTL")

restore() {
  local i
  for i in 1 2 3 4 5 6 7 8 9 10; do
    cp "$GOLDTMP" "$RTL" 2>/dev/null
    if [ "$(hash_of "$RTL")" = "$GOLDHASH" ]; then return 0; fi
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
    echo "  $name  DISCARDED (model identical to pristine — did not re-elaborate)"
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
rm -f "$GOLDTMP"

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
  echo "NOTE: ${#discards[@]} discarded mutant(s) were NOT scored — fix and re-run"
fi
echo "SWEEP OK"
