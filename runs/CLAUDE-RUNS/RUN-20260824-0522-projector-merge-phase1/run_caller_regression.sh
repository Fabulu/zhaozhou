#!/usr/bin/env bash
# run_caller_regression.sh -- RUN-20260824-0522, step 3.
#
# Builds the working tree's two projector shells beside VERBATIM copies of their
# pre-merge selves, recovered from git, in one binary, and compares every output
# port on every cycle.
#
# Then runs POSITIVE CONTROLS on the post-merge RTL, each of which changes only
# TIMING or ORDERING and none of which changes any projected value. Those are
# exactly the defects `pair_equivalence` cannot see and this harness must, and a
# harness that has only ever printed "cycle-identical" proves nothing.
#
# `git show` is used, NOT `git checkout <rev> -- <path>`: the latter STAGES,
# and this repository has already lost a rearchitecture to that exact call.
#
# Usage:  run_caller_regression.sh [<pre-merge-rev>]

set -u
REPO=/c/programmieren/zencrifice/zhaozhou
RUN="$REPO/runs/CLAUDE-RUNS/RUN-20260824-0522-projector-merge-phase1"
WORK=/c/programmieren/zencrifice/.callerreg   # NO SPACES: verilated.mk hard-fails
GEOM=fpga/rtl/geometry/zhao_geom_project.sv
TERR=fpga/rtl/terrain/zhao_terrain_project.sv
CORE=fpga/rtl/common/zhao_project_core.sv
PRE_REV=${1:-f7dd30f}   # step-2 commit: run folder only, RTL still pre-merge

export VERILATOR_ROOT='C:\programmieren\zencrifice\.tools\oss-cad-suite\share\verilator'
export PATH="/c/programmieren/zencrifice/.tools/oss-cad-suite/bin:/c/programmieren/zencrifice/.tools/oss-cad-suite/lib:/c/programmieren/dsstuff/mingw64/bin:$PATH"

rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK" || exit 2
printf 'double sc_time_stamp() { return 0; }\n' > tsstub.cpp

# ---- the pre-merge pair, renamed so both generations live in one binary -----
( cd "$REPO" && git show "$PRE_REV:$GEOM" ) \
  | sed 's/\bzhao_geom_project\b/zhao_geom_project_pre/g' > geom_pre.sv
( cd "$REPO" && git show "$PRE_REV:$TERR" ) \
  | sed 's/\bzhao_terrain_project\b/zhao_terrain_project_pre/g' > terr_pre.sv

grep -q 'module zhao_geom_project_pre' geom_pre.sv || {
  echo "ABORT: could not recover pre-merge GEOM from $PRE_REV"; exit 3; }
grep -q 'module zhao_terrain_project_pre' terr_pre.sv || {
  echo "ABORT: could not recover pre-merge TERRAIN from $PRE_REV"; exit 3; }
# The pre-merge blocks must contain the divider THEMSELVES, or the "pre" rev is
# wrong and the whole comparison is one generation against itself.
grep -q 'DIV_STEPS' geom_pre.sv || {
  echo "ABORT: $PRE_REV GEOM has no divider -- wrong rev, this is not pre-merge"; exit 3; }
grep -q 'DIV_STEPS' terr_pre.sv || {
  echo "ABORT: $PRE_REV TERRAIN has no divider -- wrong rev, this is not pre-merge"; exit 3; }
# ...and the post-merge ones must NOT, or nothing was actually extracted.
grep -q 'zhao_project_core' "$REPO/$GEOM" || {
  echo "ABORT: working-tree GEOM does not instantiate the core"; exit 3; }
grep -q 'zhao_project_core' "$REPO/$TERR" || {
  echo "ABORT: working-tree TERRAIN does not instantiate the core"; exit 3; }

verilate() {  # $1 = Mdir, $2 = prefix, $3 = top, $4.. = sources
  local mdir=$1 prefix=$2 top=$3; shift 3
  verilator_bin --cc --build -O2 -CFLAGS "-O2" --Mdir "$mdir" --prefix "$prefix" \
    --top-module "$top" "$@" >/dev/null 2>&1
}

verilate obj_gp Vzhao_geom_project_pre    zhao_geom_project_pre    geom_pre.sv || exit 4
verilate obj_tp Vzhao_terrain_project_pre zhao_terrain_project_pre terr_pre.sv || exit 4

# rc 0 = identical, rc 1 = differ, rc 2 = DID NOT BUILD (never a verdict).
link_and_run() {
  rm -f caller_reg.exe
  g++ -O2 -std=c++20 -o caller_reg.exe "$RUN/caller_regression.cpp" tsstub.cpp \
    -I obj_gn -I obj_gp -I obj_tn -I obj_tp \
    -I "$VERILATOR_ROOT/include" -I "$VERILATOR_ROOT/include/vltstd" \
    obj_gn/Vzhao_geom_project__ALL.a obj_gp/Vzhao_geom_project_pre__ALL.a \
    obj_tn/Vzhao_terrain_project__ALL.a obj_tp/Vzhao_terrain_project_pre__ALL.a \
    obj_gn/libverilated.a 2> link.err
  [ -x caller_reg.exe ] || { echo "   (did not build -- see $WORK/link.err)"; return 2; }
  ./caller_reg.exe "$@"
}

build_new() {  # rebuilds the two post-merge shells from geom.sv/terr.sv/core.sv
  rm -rf obj_gn obj_tn
  verilate obj_gn Vzhao_geom_project    zhao_geom_project    geom.sv core.sv || return 1
  verilate obj_tn Vzhao_terrain_project zhao_terrain_project terr.sv core.sv || return 1
  return 0
}

cp "$REPO/$GEOM" geom.sv; cp "$REPO/$TERR" terr.sv; cp "$REPO/$CORE" core.sv
build_new || { echo "post-merge RTL did not verilate"; exit 4; }

echo "== the real pair: working tree against $PRE_REV"
link_and_run --cycles 40000
REAL=$?
if [ $REAL -eq 2 ]; then
  echo "ABORT: the harness did not BUILD. This is NOT a verdict about the RTL."
  exit 7
fi
if [ $REAL -ne 0 ]; then
  echo "RESULT: a caller's timing or ordering CHANGED. Do not ship."
  exit 1
fi

# ---------------------------------------------------------------------------
# TIMING/ORDERING CONTROLS. `name @@ file @@ old @@ new`
#
# Every one of these leaves the projection law untouched and changes only WHEN
# or IN WHAT ORDER results appear -- which is precisely what `pair_equivalence`
# is blind to and this harness exists for.
#
# T6 WAS WITHDRAWN AND REPLACED, and the reason is worth more than the control.
# It first read `accept` -> `v_valid_i` in the counter, and the harness reported
# NOT CAUGHT. That was not a gap: inside `else if (advance)` the signal
# `v_ready_o` IS `advance` and is therefore 1, so `accept = v_valid_i && v_ready_o`
# reduces to `v_valid_i` exactly. The mutation was a PROVABLE no-op and no test
# can distinguish it -- a badly chosen control, not a blind harness. It is
# replaced by one that removes the `advance` guard entirely, so the counter ticks
# on frozen cycles, which is a real difference and is caught.
# ---------------------------------------------------------------------------
CTRL=(
"T1 core: one extra pipeline stage (the seam moved by one)@@core.sv@@      out_valid_o <= s5_valid;@@      out_valid_o <= dstep_valid[DIV_STEPS];"
"T2 geom: v_ready_o no longer tracks the stall@@geom.sv@@  assign v_ready_o = advance;@@  assign v_ready_o = 1'b1;"
"T3 terrain: tri_ready_o forgets the sequencer is busy@@terr.sv@@  assign tri_ready_o = advance && job_free;@@  assign tri_ready_o = advance;"
"T4 core: the main pipeline advances even when the caller says stop@@core.sv@@    end else if (en_i) begin
      // stage 1@@    end else if (1'b1) begin
      // stage 1"
"T5 terrain: idle_o ignores the core@@terr.sv@@  assign idle_o = !job_valid && !core_busy && !out_valid_r;@@  assign idle_o = !job_valid && !out_valid_r;"
"T6 geom: the counter ticks on STALLED cycles too@@geom.sv@@    end else if (advance) begin
      if (accept && vertices_transformed_o != 32'hFFFF_FFFF) begin@@    end else begin
      if (v_valid_i && vertices_transformed_o != 32'hFFFF_FFFF) begin"
"T7 terrain: the corner index rides one stage out of step@@terr.sv@@      .payload_i ({job_k, job_src, job_mat_a, job_mat_b, job_weight}),@@      .payload_i ({job_k + 2'd1, job_src, job_mat_a, job_mat_b, job_weight}),"
)

caught=0; blind=0
for entry in "${CTRL[@]}"; do
  name=${entry%%@@*}; rest=${entry#*@@}
  file=${rest%%@@*}; rest=${rest#*@@}
  old=${rest%%@@*}; new=${rest#*@@}

  cp "$REPO/$GEOM" geom.sv; cp "$REPO/$TERR" terr.sv; cp "$REPO/$CORE" core.sv
  OLD="$old" NEW="$new" F="$file" python "$RUN/apply_probe.py"
  if [ $? -ne 0 ]; then echo "  $name -- ABORT: could not apply"; exit 5; fi

  if ! build_new; then echo "  $name -- DISCARDED: did not elaborate"; continue; fi
  out=$(link_and_run --cycles 8000); rc=$?
  n=$(printf '%s' "$out" | grep -o '[0-9]* mismatches' | head -1)
  if [ $rc -eq 2 ]; then echo "  $name"; echo "      ABORT: probe did not build"; exit 7; fi
  if [ $rc -eq 0 ]; then
    echo "  $name"
    echo "      *** NOT CAUGHT -- the regression is blind to it ***"
    blind=$((blind + 1))
  else
    echo "  $name"
    echo "      caught ($n)"
    caught=$((caught + 1))
  fi
done

echo "----"
echo "real pair: CYCLE-IDENTICAL on every output port"
echo "timing controls: $caught caught, $blind BLIND, of ${#CTRL[@]}"
[ $blind -eq 0 ] || exit 6
