#!/usr/bin/env bash
# run_pair_equivalence.sh -- RUN-20260824-0522, step 2.
#
# Builds `pair_equivalence.cpp` against BOTH shipped projectors in one binary
# and runs it. Then runs POSITIVE CONTROLS: deliberately damaged copies of one
# or other block that the harness MUST report as different.
#
# The controls are not decoration.
# RUN-20260823-2226's fifth disclosed failure was a detector that returned zero
# across 91 modules because it could never fire, and RUN-20260824-0317's first
# failure was a differential whose own harness generated the mismatches. A
# differential that has only ever printed "identical" is indistinguishable from
# one that cannot print anything else.
#
# So: PASS on the real pair, and the expected verdict on every probe, or the
# run means nothing.
#
# A BUILD FAILURE IS NOT A VERDICT. The first draft of this script returned
# "the two shipped projectors are NOT equivalent" when the harness had merely
# failed to find a generated header. rc 2 from link_and_run is now separated
# from rc 1 and aborts loudly instead of being reported as a finding about the
# RTL.
#
# Usage:  runs/CLAUDE-RUNS/RUN-20260824-0522-.../run_pair_equivalence.sh

set -u
REPO=/c/programmieren/zencrifice/zhaozhou
RUN="$REPO/runs/CLAUDE-RUNS/RUN-20260824-0522-projector-merge-phase1"
WORK=/c/programmieren/zencrifice/.paireq   # NO SPACES: verilated.mk hard-fails
GEOM=fpga/rtl/geometry/zhao_geom_project.sv
TERR=fpga/rtl/terrain/zhao_terrain_project.sv

export VERILATOR_ROOT='C:\programmieren\zencrifice\.tools\oss-cad-suite\share\verilator'
export PATH="/c/programmieren/zencrifice/.tools/oss-cad-suite/bin:/c/programmieren/zencrifice/.tools/oss-cad-suite/lib:/c/programmieren/dsstuff/mingw64/bin:$PATH"

rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK" || exit 2
printf 'double sc_time_stamp() { return 0; }\n' > tsstub.cpp

verilate() {  # $1 = .sv path, $2 = Mdir, $3 = prefix, $4 = top
  verilator_bin --cc --build -O2 -CFLAGS "-O2" --Mdir "$2" --prefix "$3" \
    --top-module "$4" "$1" >/dev/null 2>&1
}

# rc 0 = agree, rc 1 = differ, rc 2 = DID NOT BUILD (never a verdict).
link_and_run() {
  rm -f pair_eq.exe
  g++ -O2 -std=c++20 -o pair_eq.exe "$RUN/pair_equivalence.cpp" tsstub.cpp \
    -I obj_g -I obj_t \
    -I "$REPO/reference/include" -I "$REPO/reference/src" -I "$REPO/runtime/include" \
    -I "$VERILATOR_ROOT/include" -I "$VERILATOR_ROOT/include/vltstd" \
    "$REPO/reference/src/zrender/rast.cpp" \
    obj_g/Vzhao_geom_project__ALL.a obj_t/Vzhao_terrain_project__ALL.a \
    obj_g/libverilated.a 2> link.err
  [ -x pair_eq.exe ] || { echo "   (did not build -- see $WORK/link.err)"; return 2; }
  ./pair_eq.exe
}

# --------------------------------------------------------------- the pair --
cp "$REPO/$GEOM" geom.sv
cp "$REPO/$TERR" terr.sv
rm -rf obj_g obj_t
verilate geom.sv obj_g Vzhao_geom_project zhao_geom_project || { echo "geom did not verilate"; exit 4; }
verilate terr.sv obj_t Vzhao_terrain_project zhao_terrain_project || { echo "terrain did not verilate"; exit 4; }

echo "== the real pair, at HEAD"
link_and_run
REAL=$?
if [ $REAL -eq 2 ]; then
  echo "ABORT: the harness did not BUILD. This is NOT a verdict about the RTL."
  exit 7
fi
if [ $REAL -ne 0 ]; then
  echo "RESULT: the two shipped projectors are NOT equivalent. STOP -- report before merging."
  exit 1
fi

# ---------------------------------------------------------------------------
# POSITIVE CONTROLS.  `name @@ file @@ old @@ new`
#
# Each is a defect a merge could plausibly introduce, applied to ONE of the two
# blocks so the pair genuinely disagrees. Every one MUST be caught. P1-P5 are
# the five defect classes the brief names for the mutation sweep; P6-P10 are the
# law-level ones this differential is uniquely placed to see.
# ---------------------------------------------------------------------------
CTRL=(
"P1 terrain: the view tag is swapped at the viewport stage@@terr.sv@@    cx13 = {1'b0, vp_x0[s5_view]} + {2'b0, vp_w[s5_view][11:1]};@@    cx13 = {1'b0, vp_x0[!s5_view]} + {2'b0, vp_w[!s5_view][11:1]};"
"P2 terrain: a config write lands in the wrong view's matrix@@terr.sv@@        mat[cfg_view_i][cfg_addr_i[3:0]] <= \$signed(cfg_data_i);@@        mat[!cfg_view_i][cfg_addr_i[3:0]] <= \$signed(cfg_data_i);"
"P3 geom: the viewport transform uses the other view's viewport@@geom.sv@@    cy13 = {1'b0, vp_y0[s5_view]} + {2'b0, vp_h[s5_view][11:1]};@@    cy13 = {1'b0, vp_y0[!s5_view]} + {2'b0, vp_h[!s5_view][11:1]};"
"P4 terrain: the triangle is reassembled out of order (B takes C's word)@@terr.sv@@          out_bx_o     <= acc_x[1];@@          out_bx_o     <= s6_px;"
"P5 geom: a stale matrix -- row 0 uses view 0 regardless of the packet@@geom.sv@@    row_x = ext64(mul32(mat[view_i][0], vx_i)) + ext64(mul32(mat[view_i][1], vy_i)) +@@    row_x = ext64(mul32(mat[1'b0][0], vx_i)) + ext64(mul32(mat[1'b0][1], vy_i)) +"
"P6 terrain: the near plane becomes strict (w == 0 moves to the accept side)@@terr.sv@@    pre_behind = (s2_cw <= 32'sd0);@@    pre_behind = (s2_cw < 32'sd0);"
"P7 geom: the guard band clamps one count short of the rail@@geom.sv@@      if (r > 41'sd524288) to_screen_xy = 21'sd524288;@@      if (r > 41'sd524287) to_screen_xy = 21'sd524287;"
"P8 terrain: the row sum rounds toward zero instead of half up@@terr.sv@@      r = (x + 68'sd32768) >>> 16;@@      r = x >>> 16;"
"P9 geom: the 1/w lane divides the wrong numerator@@geom.sv@@    pre_n[2] = 48'h0001_0000_0000;  // (1 << 16) << 16@@    pre_n[2] = 48'h0002_0000_0000;"
"P10 terrain: a behind-the-eye vertex keeps its coordinates instead of zeroing@@terr.sv@@      s6_px     <= s5_behind ? 21'sd0 : to_screen_xy(scr_fx_x);@@      s6_px     <= to_screen_xy(scr_fx_x);"
)

caught=0; blind=0
for entry in "${CTRL[@]}"; do
  name=${entry%%@@*}; rest=${entry#*@@}
  file=${rest%%@@*}; rest=${rest#*@@}
  old=${rest%%@@*}; new=${rest#*@@}

  cp "$REPO/$GEOM" geom.sv
  cp "$REPO/$TERR" terr.sv
  OLD="$old" NEW="$new" F="$file" python "$RUN/apply_probe.py"
  if [ $? -ne 0 ]; then echo "  $name -- ABORT: could not apply"; exit 5; fi

  rm -rf obj_g obj_t
  if ! verilate geom.sv obj_g Vzhao_geom_project zhao_geom_project; then
    echo "  $name -- DISCARDED: geom did not elaborate"; continue
  fi
  if ! verilate terr.sv obj_t Vzhao_terrain_project zhao_terrain_project; then
    echo "  $name -- DISCARDED: terrain did not elaborate"; continue
  fi
  out=$(link_and_run); rc=$?
  n=$(printf '%s' "$out" | grep -o '[0-9]* mismatches' | head -1)
  if [ $rc -eq 2 ]; then
    echo "  $name"
    echo "      ABORT: probe did not build -- not a verdict"; exit 7
  fi
  if [ $rc -eq 0 ]; then
    echo "  $name"
    echo "      *** NOT CAUGHT -- the differential is blind to it ***"
    blind=$((blind + 1))
  else
    echo "  $name"
    echo "      caught ($n)"
    caught=$((caught + 1))
  fi
done

echo "----"
echo "real pair: EQUIVALENT"
echo "controls: $caught caught, $blind BLIND, of ${#CTRL[@]}"
[ $blind -eq 0 ] || exit 6
