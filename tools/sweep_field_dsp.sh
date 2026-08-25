#!/usr/bin/env bash
# sweep_field_dsp.sh — mutation sweep for the Field IR shared arithmetic engine.
#
# ---------------------------------------------------------------------------
# WHAT THIS SWEEPS, AND WHY IT IS A NEW SWEEP RATHER THAN AN EXTENDED ONE
# ---------------------------------------------------------------------------
# The 2026-08-23 DSP rearchitecture replaced ten parallel op units -- each with
# its own multiplier -- with ONE shared lane, ONE integer square root, ONE sine
# table and ONE reciprocal, and moved the products of MUL/MAD/DOT2/DOT3 into the
# register-read walk. Almost none of the engine's earlier mutants can reach that
# logic, because none of it existed. The thirty mutants in
# tools/sweep_field_dsp_mutants.py are the new failure surface: the lane, the
# read-walk accumulator, the four muxes, the schedule, and the six per-op walks
# that were rewritten around the lane's two-cycle latency.
#
# THE ONE THAT MATTERS MOST IS M05. It changes the shared accumulator from
# LOADED-by-the-first-product to ADDED-to, which is the defect a shared
# accumulator can have and a parallel design could not. Every test that runs one
# operation at a time passes with it -- which is every other section of
# field_seq_directed and every block-level differential in the tree. Section 13
# (alone versus interleaved) exists for it, and this sweep is where "section 13
# would catch it" stops being a claim.
#
# ---------------------------------------------------------------------------
# THE SEVEN GUARDS, all of them ways this build system scores a run that never
# happened. Every one is in this repository's history.
# ---------------------------------------------------------------------------
#   1. `verilate()` elaborates at CONFIGURE time, so `ninja` alone relinks a
#      CACHED model against changed source. `cmake -S . -B build` runs every
#      iteration.
#   2. Stamping the mutated source's mtime into the FUTURE makes a model
#      elaborated from a MUTANT look newer than the pristine source restored
#      after it. mtime is set to NOW, never forward (mutants.py, os.utime).
#   3. Hashing the generated model is necessary but NOT sufficient -- a pristine
#      model can be linked against an object still compiled from a mutant. The
#      whole target directory is deleted each iteration.
#   4. Hash the whole model DIRECTORY, not the wrapper file: the wrapper is
#      byte-identical between a pristine and a mutated build.
#   5. THE EXECUTABLE LIVES OUTSIDE THE TARGET DIRECTORY, so a mutant that fails
#      to COMPILE leaves the previous binary in place and is scored as CAUGHT --
#      the most flattering possible way to be wrong. The exe is deleted too and
#      required to exist after the build.
#   6. The PRISTINE build is checked before any mutant runs. If it fails its own
#      tests, every "caught" below would be meaningless.
#   7. `cmake` re-elaborates EVERY target that verilates a mutated module, so a
#      sweep that cleans one consumer leaves MUTANT-GENERATED SOURCES on disk in
#      the others for somebody else's build to compile. That is what contaminated
#      the terrain sweep and produced a wrong public diagnosis. THE RULE: clean,
#      rebuild and SCORE every consumer of the mutated file.
#
# GUARD 7 IS DERIVED HERE, NOT DECLARED. This sweep spans TEN files, and a
# hand-maintained consumer list across ten files is a list that drifts. The
# consumers of a file are read out of tests/CMakeLists.txt at run time by
# `consumers_of`, and the run REFUSES TO START if any mutated file has no
# consumer at all -- which is the only way the derivation can silently produce
# nothing. The declared union below is cross-checked against the derivation for
# the same reason the terrain sweep keeps its list: so a reader can see the
# blast radius without running the script.
#
# ---------------------------------------------------------------------------
# EXPECTED SURVIVORS
# ---------------------------------------------------------------------------
# None are predicted. Every mutant here changes either an answer, a saturation
# lane, or a latency the differential's section 12 measures. Survivors are
# recorded with a PROOF of equivalence or they are holes -- "probably
# equivalent" is not a category this project has.
# ---------------------------------------------------------------------------
set -u

MUTPY=tools/sweep_field_dsp_mutants.py

# The union of every consumer, for a reader. Cross-checked below against what
# the build system actually says.
DECLARED_TARGETS="test_field_seq_directed test_field_alu_ops test_field_len_directed test_field_normalize_directed test_field_noise_directed test_field_rot_directed test_field_ring_directed test_field_curve_directed test_field_rcp_directed test_field_sinks_directed test_field_sin_directed test_field_progcache_directed test_field_v2_core_directed"

# `| tr -d` because Python's text stdout emits CRLF on Windows and a bare carriage return
# on the end of a path turns every guard below into a false ABORT.
FILES=$(python "$MUTPY" --files | tr -d '\r')

GOLDDIR=$(mktemp -d)
for f in $FILES; do
  cp "$f" "$GOLDDIR/$(basename "$f")"
done

restore() {
  local i f ok
  for i in 1 2 3 4 5 6 7 8 9 10; do
    ok=1
    for f in $FILES; do
      cp "$GOLDDIR/$(basename "$f")" "$f" 2>/dev/null
      if ! cmp -s "$GOLDDIR/$(basename "$f")" "$f"; then ok=0; fi
    done
    [ "$ok" = "1" ] && return 0
    sleep 1
  done
  return 1
}

moved_from_gold() {
  local f
  for f in $FILES; do
    if ! cmp -s "$GOLDDIR/$(basename "$f")" "$f"; then return 0; fi
  done
  return 1
}

# GUARD 7: which targets does the build system elaborate this file into?
consumers_of() {
  python - "$1" <<'PY'
import io, re, sys
path = sys.argv[1].replace(chr(92), '/')   # no literal backslash: heredocs mangle it
base = path.split('/')[-1]
s = io.open('tests/CMakeLists.txt', encoding='utf-8').read()
out = []
for m in re.finditer(r'verilate\((\w+)(.*?)\.sv\)', s, re.S):
    if base in m.group(2) + '.sv':
        out.append(m.group(1))
print(' '.join(sorted(set(out))))
PY
}


# The verilated model directory a target holds is named after its TOP module,
# which is not the mutated module's name. Hash every model directory the target
# has, which is the strictly stronger check.
model_hash() {
  local t h=""
  for t in $1; do
    h="$h$(find "build/tests/CMakeFiles/$t.dir" -type f \( -name "*.cpp" -o -name "*.h" \) \
             2>/dev/null | sort | xargs sha256sum 2>/dev/null | sha256sum | cut -d" " -f1)"
  done
  printf '%s' "$h" | sha256sum | cut -d" " -f1
}

models_present() {
  local t
  for t in $1; do
    [ -d "build/tests/CMakeFiles/$t.dir" ] || return 1
  done
  return 0
}

exes_present() {
  local t
  for t in $1; do
    [ -x "build/tests/$t.exe" ] || return 1
  done
  return 0
}

rebuild() {
  local t
  for t in $1; do
    rm -rf "build/tests/CMakeFiles/$t.dir"
    rm -f "build/tests/$t.exe"    # guard 5
  done
  cmake -S . -B build >/dev/null 2>&1
  # shellcheck disable=SC2086
  ninja -C build $1 >/dev/null 2>&1
}

run_lanes() {
  local t
  for t in $1; do
    "./build/tests/$t.exe" >/dev/null 2>&1 || return 1
  done
  return 0
}

# ---- guard 7's own check --------------------------------------------------
echo "== guard 7: consumers, derived from tests/CMakeLists.txt"
ALL=""
for f in $FILES; do
  c=$(consumers_of "$f" | tr -d '\r')
  if [ -z "$c" ]; then
    echo "ABORT: no target in tests/CMakeLists.txt verilates $f."
    echo "       A mutation there would be applied, built into nothing, and"
    echo "       scored against a stale binary. See guard 7."
    exit 9
  fi
  printf '   %-46s %s\n' "$(basename "$f")" "$c"
  ALL="$ALL $c"
done
ALL=$(echo "$ALL" | tr ' ' '\n' | sed '/^$/d' | sort -u | tr '\n' ' ')
for t in $ALL; do
  case " $DECLARED_TARGETS " in
    *" $t "*) ;;
    *) echo "ABORT: derived consumer '$t' is not in DECLARED_TARGETS."; exit 9 ;;
  esac
done
for t in $DECLARED_TARGETS; do
  case " $ALL " in
    *" $t "*) ;;
    *) echo "ABORT: DECLARED_TARGETS names '$t', which no mutated file reaches."; exit 9 ;;
  esac
done
echo "   union: $ALL"

# ---- preflight: every mutant must lint ------------------------------------
python tools/sweep_field_dsp_preflight.py || {
  echo "ABORT: at least one mutant does not build -- fix the mutation, not the guard"
  exit 8
}

echo "== establishing the pristine baseline"
restore || { echo "ABORT: cannot establish pristine source"; exit 4; }
rebuild "$ALL"
models_present "$ALL" || { echo "ABORT: a pristine model did not elaborate"; exit 6; }
exes_present "$ALL" || { echo "ABORT: a pristine target did not link"; exit 6; }
PRISTINE_MODEL=$(model_hash "$ALL")
if ! run_lanes "$ALL"; then
  echo "ABORT: the PRISTINE build fails its own tests -- nothing below would mean anything"
  exit 7
fi
echo "   pristine model ${PRISTINE_MODEL:0:16}, all $(echo $ALL | wc -w) lanes green"

# GUARDS 3 AND 4, PER TARGET SET. A mutant is only scored if the models its own
# consumers hold actually CHANGED. The union's hash cannot answer that: a
# mutation in zhao_field_mul.sv touches eight targets and one in
# zhao_field_exec_shared.sv touches one, and a union hash that moved says
# nothing about which. So the pristine hash is recorded for every distinct
# consumer set the mutant list can produce, from this one clean build.
setkey() { echo "$1" | tr ' ' '_'; }
declare -A PRISTINE_SET
for f in $FILES; do
  ts=$(consumers_of "$f" | tr -d '\r')
  PRISTINE_SET[$(setkey "$ts")]=$(model_hash "$ts")
done

expected=$(python "$MUTPY" --count | tr -d '\r')
attempted=0
accounted=0
caught=0
survivors=()

k=0
while [ "$k" -lt "$expected" ]; do
  name=$(python "$MUTPY" --name "$k" | tr -d '\r')
  file=$(python "$MUTPY" --file "$k" | tr -d '\r')
  targets=$(consumers_of "$file" | tr -d '\r')
  attempted=$((attempted + 1))

  restore || { echo "  $name  ABORT: could not restore before applying"; exit 4; }

  if ! python "$MUTPY" --apply "$k"; then
    echo "  $name  ABORT: could not apply"
    restore
    exit 3
  fi
  if ! moved_from_gold; then
    echo "  $name  ABORT: source unchanged after apply"
    restore
    exit 3
  fi

  rebuild "$targets"

  if ! models_present "$targets"; then
    echo "  $name  DISCARDED: a model was absent after regeneration"
    restore || { echo "ABORT: revert failed"; exit 4; }
    k=$((k + 1)); continue
  fi
  if ! exes_present "$targets"; then
    echo "  $name  DISCARDED: a target did not LINK (a build failure would"
    echo "                    otherwise be scored as a caught mutant)"
    restore || { echo "ABORT: revert failed"; exit 4; }
    k=$((k + 1)); continue
  fi
  # GUARDS 3 AND 4: the mutant's generated model must DIFFER from the pristine
  # one for THIS file's consumers. Identical means the target did not
  # re-elaborate, and running it would score a build that never happened.
  mutant_model=$(model_hash "$targets")
  if [ "$mutant_model" = "${PRISTINE_SET[$(setkey "$targets")]}" ]; then
    echo "  $name  DISCARDED: model identical to pristine (did not re-elaborate)"
    restore || { echo "ABORT: revert failed"; exit 4; }
    k=$((k + 1)); continue
  fi

  if ! run_lanes "$targets"; then
    echo "  $name  caught   [$targets]"
    caught=$((caught + 1))
  else
    echo "  $name  *** SURVIVED ***   [$targets]"
    survivors+=("$name")
  fi
  accounted=$((accounted + 1))

  restore || { echo "  $name  ABORT: revert was not byte-identical"; exit 4; }
  k=$((k + 1))
done

echo "== restoring the pristine build"
restore || { echo "ABORT: final restore failed"; exit 4; }
rebuild "$ALL"
if ! run_lanes "$ALL"; then
  echo "ABORT: the restored build fails its own tests -- the tree was left dirty"
  exit 7
fi

echo "----"
echo "attempted=$attempted expected=$expected accounted=$accounted caught=$caught"
for s in "${survivors[@]:-}"; do [ -n "$s" ] && echo "SURVIVOR: $s"; done
if [ "$attempted" != "$expected" ] || [ "$accounted" != "$expected" ]; then
  echo "CROSS-CHECK FAILED (attempted/accounted must both equal $expected)"
  exit 5
fi
rm -rf "$GOLDDIR"
