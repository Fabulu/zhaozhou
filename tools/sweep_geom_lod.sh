#!/usr/bin/env bash
# sweep_geom_lod.sh — mutation sweep for zhao_geom_lod.sv.
#
# ---------------------------------------------------------------------------
# THE SWEEP VERIFIES ITS OWN BUILDS, and this file is mostly that machinery
# rather than the mutations. FOUR separate ways of scoring a test that never
# ran were hit while writing it, all of them already in this project's history:
#
#   1. `verilate()` elaborates at CONFIGURE time, so `ninja` alone relinks a
#      CACHED model against changed source. The first run of this sweep
#      discarded all 20 mutants for exactly that reason — the guard working,
#      not a nuisance.
#
#   2. Stamping the mutated source's mtime into the FUTURE (an earlier version
#      did, to force a rebuild) makes a model elaborated from a MUTANT look
#      newer than the pristine source restored after it, so the next
#      elaboration is skipped and a mutant model is served against clean RTL.
#      mtime is set to NOW here, never forward.
#
#   3. Hashing the generated model is NECESSARY BUT NOT SUFFICIENT. A pristine
#      model can be linked against an OBJECT still compiled from a mutant —
#      observed here as 994 failures against provably clean RTL. So the whole
#      target directory is deleted each iteration, not just the model.
#
# The scoring rule is therefore: after regeneration the model must EXIST and
# its hash must DIFFER from the pristine model's. Anything else is discarded,
# never scored. The revert is verified byte-for-byte with retries, because a
# cmake that has only just exited can still hold the file open on Windows —
# which is how an earlier run aborted with "revert was not byte-identical".
#
# And the baseline is checked before any mutant runs: if the PRISTINE build
# does not pass its own test, every "caught" below would be meaningless.
# ---------------------------------------------------------------------------
set -u

RTL=fpga/rtl/geometry/zhao_geom_lod.sv
TARGETDIR=build/tests/CMakeFiles/test_geom_lod_directed.dir
MODELDIR="$TARGETDIR/Vzhao_geom_lod.dir"

# HASH THE WHOLE MODEL DIRECTORY, not Vzhao_geom_lod.cpp.
#
# That file is Verilator's WRAPPER and it is byte-identical between a pristine
# build and a mutated one -- the logic lives in Vzhao_geom_lod___024root__0.cpp.
# Hashing the wrapper made every mutant look like "did not re-elaborate" while
# the executable had the mutation in it and the test was failing correctly. That
# is the fourth distinct way this sweep found to score a run that never happened,
# and the only one that reported DISCARDED for work that was actually fine.
model_hash() {
  find "$MODELDIR" -type f \( -name "*.cpp" -o -name "*.h" \) 2>/dev/null \
    | sort | xargs sha256sum 2>/dev/null | sha256sum | cut -d" " -f1
}
EXE=./build/tests/test_geom_lod_directed.exe

GOLD=$(mktemp)
cp "$RTL" "$GOLD"
GOLDHASH=$(sha256sum <"$GOLD" | cut -d' ' -f1)

rebuild() {
  rm -rf "$TARGETDIR"
  cmake -S . -B build >/dev/null 2>&1
  ninja -C build test_geom_lod_directed >/dev/null 2>&1
}

# Restore the pristine source and PROVE it, retrying through any lingering
# file lock left by the cmake that just exited.
restore() {
  local i
  for i in 1 2 3 4 5 6 7 8 9 10; do
    cp "$GOLD" "$RTL" 2>/dev/null
    if [ "$(sha256sum <"$RTL" | cut -d' ' -f1)" = "$GOLDHASH" ]; then return 0; fi
    sleep 1
  done
  return 1
}

echo "== establishing the pristine baseline"
restore || { echo "ABORT: cannot establish pristine source"; exit 4; }
rebuild
[ -d "$MODELDIR" ] || { echo "ABORT: pristine model did not elaborate"; exit 6; }
PRISTINE_MODEL=$(model_hash)
if ! "$EXE" >/dev/null 2>&1; then
  echo "ABORT: the PRISTINE build fails its own test — nothing below would mean anything"
  exit 7
fi
echo "   pristine model ${PRISTINE_MODEL:0:16}, directed lane green"

# Each entry: name @@ old @@ new
MUTS=(
"M01 legality edge  (< becomes <=)@@assign legal_glint = (lhs_glint < legal_rhs);@@assign legal_glint = (lhs_glint <= legal_rhs);"
"M02 rounding term dropped (micro)@@assign lhs_micro = (72'(proj_radius_q8_i) * 72'(micro_error_i)) + (72'(bound_radius_i) >>> 1);@@assign lhs_micro = (72'(proj_radius_q8_i) * 72'(micro_error_i)) + 72'sd0;"
"M03 rounding rounds down (glint)@@assign lhs_glint = (72'(proj_radius_q8_i) * 72'(glint_error_i)) + (72'(bound_radius_i) >>> 1);@@assign lhs_glint = (72'(proj_radius_q8_i) * 72'(glint_error_i)) - (72'(bound_radius_i) >>> 1);"
"M04 finest legal rung, not coarsest@@    if (legal_glint) raw = 2'd3;
    else if (legal_splat) raw = 2'd2;
    else if (legal_micro) raw = 2'd1;
    else raw = 2'd0;@@    if (legal_micro) raw = 2'd1;
    else if (legal_splat) raw = 2'd2;
    else if (legal_glint) raw = 2'd3;
    else raw = 2'd0;"
"M05 minimum hold 15 -> 14@@localparam logic [15:0] HOLD_TICKS = 16'd15;@@localparam logic [15:0] HOLD_TICKS = 16'd14;"
"M06 minimum hold removed@@localparam logic [15:0] HOLD_TICKS = 16'd15;@@localparam logic [15:0] HOLD_TICKS = 16'd0;"
"M07 coarsen boundary >= becomes >@@switch_ok = (bnd_num >= (k_ceil * 72'(e_sel)));@@switch_ok = (bnd_num > (k_ceil * 72'(e_sel)));"
"M08 refine boundary < becomes <=@@switch_ok = (bnd_num < ((m_floor + 72'sd1) * 72'(e_sel)));@@switch_ok = (bnd_num <= ((m_floor + 72'sd1) * 72'(e_sel)));"
"M09 ceil becomes floor (the +8)@@assign k_ceil  = (proj10 + 72'sd8) / 72'sd9;@@assign k_ceil  = proj10 / 72'sd9;"
"M10 hysteresis 9 becomes 10 (no band)@@assign k_ceil  = (proj10 + 72'sd8) / 72'sd9;@@assign k_ceil  = (proj10 + 72'sd8) / 72'sd10;"
"M11 hysteresis 11 becomes 10 (no band)@@assign m_floor = proj10 / 72'sd11;@@assign m_floor = proj10 / 72'sd10;"
"M12 refine drops the +1@@switch_ok = (bnd_num < ((m_floor + 72'sd1) * 72'(e_sel)));@@switch_ok = (bnd_num < (m_floor * 72'(e_sel)));"
"M13 boundary uses the wrong rung@@assign bnd_rung   = coarsening ? raw : rung_i;@@assign bnd_rung   = coarsening ? rung_i : raw;"
"M14 boundary rounding term dropped@@+ (72'(e_sel) >>> 1);@@+ 72'sd0;"
"M15 hold does not saturate@@assign hold_inc = (hold_i == 16'hFFFF) ? 16'hFFFF : (hold_i + 16'd1);@@assign hold_inc = hold_i + 16'd1;"
"M16 hold not cleared on a switch@@      rung_next = raw;
      hold_next = 16'd0;@@      rung_next = raw;
      hold_next = hold_inc;"
"M17 e==0 coarsening always allowed@@switch_ok = coarsening ? (proj_radius_q8_i == 32'sd0) : 1'b1;@@switch_ok = 1'b1;"
"M18 e==0 refining always refused@@switch_ok = coarsening ? (proj_radius_q8_i == 32'sd0) : 1'b1;@@switch_ok = coarsening ? (proj_radius_q8_i == 32'sd0) : 1'b0;"
"M19 coarsening test inverted@@assign coarsening = (raw > rung_i);@@assign coarsening = (raw < rung_i);"
"M20 legality uses R where e belongs@@assign lhs_splat = (72'(proj_radius_q8_i) * 72'(splat_error_i)) + (72'(bound_radius_i) >>> 1);@@assign lhs_splat = (72'(proj_radius_q8_i) * 72'(bound_radius_i)) + (72'(bound_radius_i) >>> 1);"
"M21 two rungs share one error term@@assign lhs_splat = (72'(proj_radius_q8_i) * 72'(splat_error_i)) + (72'(bound_radius_i) >>> 1);@@assign lhs_splat = (72'(proj_radius_q8_i) * 72'(glint_error_i)) + (72'(bound_radius_i) >>> 1);"
"M22 micro rung shares the glint term@@assign lhs_micro = (72'(proj_radius_q8_i) * 72'(micro_error_i)) + (72'(bound_radius_i) >>> 1);@@assign lhs_micro = (72'(proj_radius_q8_i) * 72'(glint_error_i)) + (72'(bound_radius_i) >>> 1);"
)

expected=${#MUTS[@]}
attempted=0
accounted=0
caught=0
survivors=()

for entry in "${MUTS[@]}"; do
  name=${entry%%@@*}
  rest=${entry#*@@}
  old=${rest%%@@*}
  new=${rest#*@@}
  attempted=$((attempted + 1))

  restore || { echo "  $name  ABORT: could not restore before applying"; exit 4; }

  OLD="$old" NEW="$new" RTL="$RTL" python - <<'PY'
import io, os, sys
p = os.environ['RTL']
raw = io.open(p, encoding='utf-8', newline='').read()
NL = '\r\n' if '\r\n' in raw else '\n'
o = os.environ['OLD'].replace('\n', NL)
n = os.environ['NEW'].replace('\n', NL)
if raw.count(o) != 1:
    sys.stderr.write('ANCHOR NOT UNIQUE (%d)\n' % raw.count(o)); sys.exit(9)
if o == n:
    sys.stderr.write('MUTANT IDENTICAL TO BASE\n'); sys.exit(9)
io.open(p, 'w', encoding='utf-8', newline='').write(raw.replace(o, n, 1))
os.utime(p, None)   # NOW, never the future -- see the header
PY
  if [ $? -ne 0 ]; then
    echo "  $name  ABORT: could not apply"
    restore
    exit 3
  fi

  # PREFLIGHT: the source must actually have moved.
  if [ "$(sha256sum <"$RTL" | cut -d' ' -f1)" = "$GOLDHASH" ]; then
    echo "  $name  ABORT: source unchanged after apply"
    restore
    exit 3
  fi

  rebuild

  if [ ! -d "$MODELDIR" ]; then
    echo "  $name  DISCARDED: model absent after regeneration"
    restore || { echo "ABORT: revert failed"; exit 4; }
    continue
  fi
  mutant_model=$(model_hash)
  if [ "$mutant_model" = "$PRISTINE_MODEL" ]; then
    echo "  $name  DISCARDED: model identical to pristine (did not re-elaborate)"
    restore || { echo "ABORT: revert failed"; exit 4; }
    continue
  fi

  if "$EXE" >/dev/null 2>&1 && "$EXE" --random 4000 >/dev/null 2>&1; then
    echo "  $name  *** SURVIVED ***"
    survivors+=("$name")
  else
    echo "  $name  caught"
    caught=$((caught + 1))
  fi
  accounted=$((accounted + 1))

  restore || { echo "  $name  ABORT: revert was not byte-identical"; exit 4; }
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
