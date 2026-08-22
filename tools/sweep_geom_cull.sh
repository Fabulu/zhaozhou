#!/usr/bin/env bash
# sweep_geom_cull.sh — mutation sweep for zhao_geom_cull.sv.
#
# ---------------------------------------------------------------------------
# THE SWEEP VERIFIES ITS OWN BUILDS. Every guard below was written for
# tools/sweep_geom_lod.sh, where FIVE separate ways of scoring a test that never
# ran were hit for real, and they are carried here unchanged rather than
# rediscovered:
#
#   1. `verilate()` elaborates at CONFIGURE time, so `ninja` alone relinks a
#      CACHED model against changed source.
#   2. Stamping the mutated source's mtime into the FUTURE makes a mutant model
#      look newer than the pristine source restored after it, so the next
#      elaboration is skipped and a mutant model is served against clean RTL.
#      mtime is set to NOW, never forward.
#   3. Hashing the generated model is necessary but not sufficient: a pristine
#      model can be linked against an OBJECT still compiled from a mutant. The
#      whole target directory is deleted each iteration.
#   4. Hash the whole model DIRECTORY, not Vzhao_geom_cull.cpp — that file is
#      Verilator's wrapper and is byte-identical between a pristine and a
#      mutated build. The logic lives in Vzhao_geom_cull___024root__0.cpp.
#   5. THE EXECUTABLE LIVES OUTSIDE THE TARGET DIRECTORY, so a mutant that
#      fails to COMPILE leaves the previous binary in place and the sweep runs
#      that instead — a build failure scoring as a caught mutant, the most
#      flattering possible way to be wrong. Delete the exe too and require it
#      to exist after the build.
#
# The scoring rule: after regeneration the model must EXIST and its hash must
# DIFFER from the pristine model's. Anything else is discarded, never scored.
# And the baseline is checked before any mutant runs: if the PRISTINE build does
# not pass its own test, every "caught" below would be meaningless.
#
# ---------------------------------------------------------------------------
# RESULT, 2026-08-22: 32 attempted / 32 accounted / 30 caught / 2 EQUIVALENT.
#
# Both survivors are equivalent with a PROOF, not with a label. One was
# predicted before the run; the other was predicted to be CAUGHT and was not,
# which is the more useful of the two because it corrected a comment in the RTL
# that claimed a necessity the design does not have.
#
# M08 — TOP AND BOTTOM SWAPPED. The five planes are used only as a SET: a sphere
# is outside a camera iff it is outside AT LEAST ONE of them, so relabelling two
# of them cannot change the answer. {row3+row1, row3-row1} is the same pair of
# half-spaces however the two are named. What could still break it is the
# PAIRING of a plane with its stored length bound — and that cannot come apart,
# because the extraction and the evaluation reach the plane through the SAME
# `sel_plane` mux and index `len_ceil` with the same number. Permute the labels
# and every (plane, length) pair permutes with them.
#
#   Note what this does NOT say. A plane built from the WRONG ROW is not a
#   relabelling and is caught: M02 (bottom takes row0), M04 (row1 read where
#   row0 belongs) and M05 (row0 read where row3 belongs) all die. The distinction
#   is the reason three of the six cameras in the differential are SHEARED: on a
#   symmetric camera left and right are mirror images and so are top and bottom,
#   so a genuinely wrong row can masquerade as a relabelling.
#
# M29 — CLEAR DOMINATES SET ON THE DIRTY BIT. Expected to be caught; it is not,
# and it cannot be. The clear fires only in a cycle where `prep_start` is high,
# which means the state is S_IDLE and an extraction is about to begin. A matrix
# write in that same cycle commits `mat` on the same edge, and the extraction
# just started makes its first read of `mat` in the NEXT cycle through a
# COMBINATIONAL mux — so the write whose dirty bit was discarded is already in
# the matrix being extracted. Every later write lands while the state is not
# S_IDLE, where `prep_start` is low and nothing is cleared. No input
# distinguishes the two orderings.
#
#   The RTL comment claiming the clear-dominates form would "leave the block
#   culling against a stale length bound" was therefore WRONG, and has been
#   rewritten to say what is actually true: the ordering is defensive, and it
#   matters only if the extraction ever latches `mat` at `prep_start` instead of
#   reading it combinationally. That is exactly the change a timing pass would
#   make, which is why the ordering stays.
# ---------------------------------------------------------------------------
set -u

RTL=fpga/rtl/geometry/zhao_geom_cull.sv
TARGETDIR=build/tests/CMakeFiles/test_geom_cull_directed.dir
MODELDIR="$TARGETDIR/Vzhao_geom_cull.dir"

model_hash() {
  find "$MODELDIR" -type f \( -name "*.cpp" -o -name "*.h" \) 2>/dev/null \
    | sort | xargs sha256sum 2>/dev/null | sha256sum | cut -d" " -f1
}
EXE=./build/tests/test_geom_cull_directed.exe

GOLD=$(mktemp)
cp "$RTL" "$GOLD"
GOLDHASH=$(sha256sum <"$GOLD" | cut -d' ' -f1)

rebuild() {
  rm -rf "$TARGETDIR"
  rm -f "$EXE"   # guard 5: the executable lives OUTSIDE the target dir
  cmake -S . -B build >/dev/null 2>&1
  ninja -C build test_geom_cull_directed >/dev/null 2>&1
}

restore() {
  local i
  for i in 1 2 3 4 5 6 7 8 9 10; do
    cp "$GOLD" "$RTL" 2>/dev/null
    if [ "$(sha256sum <"$RTL" | cut -d' ' -f1)" = "$GOLDHASH" ]; then return 0; fi
    sleep 1
  done
  return 1
}

# PREFLIGHT: EVERY MUTANT MUST LINT BEFORE ANY OF THEM IS SCORED.
python3 tools/sweep_geom_cull_preflight.py || {
  echo "ABORT: at least one mutant does not build -- fix the mutation, not the guard"
  exit 8
}

echo "== establishing the pristine baseline"
restore || { echo "ABORT: cannot establish pristine source"; exit 4; }
rebuild
[ -d "$MODELDIR" ] || { echo "ABORT: pristine model did not elaborate"; exit 6; }
[ -x "$EXE" ] || { echo "ABORT: pristine target did not link"; exit 6; }
PRISTINE_MODEL=$(model_hash)
if ! "$EXE" >/dev/null 2>&1; then
  echo "ABORT: the PRISTINE build fails its own test — nothing below would mean anything"
  exit 7
fi
echo "   pristine model ${PRISTINE_MODEL:0:16}, directed lane green"

# Each entry: name @@ old @@ new
MUTS=(
"M01 left plane subtracts (becomes right)@@      3'd0: begin  // left   = row3 + row0
        cmb_row  = 1'b0;
        cmb_add  = 1'b1;@@      3'd0: begin  // left   = row3 + row0
        cmb_row  = 1'b0;
        cmb_add  = 1'b0;"
"M02 bottom plane combines row0, not row1@@      3'd2: begin  // bottom = row3 + row1
        cmb_row  = 1'b1;@@      3'd2: begin  // bottom = row3 + row1
        cmb_row  = 1'b0;"
"M03 near plane is not row3 alone@@        cmb_none = 1'b1;@@        cmb_none = 1'b0;"
"M04 row1 read where row0 belongs@@      othr0 = mat[sel_view][4];@@      othr0 = mat[sel_view][0];"
"M05 planes combine row0, not row3 (base row wrong)@@    base0 = mat[sel_view][12];@@    base0 = mat[sel_view][0];"
"M06 the plane offset enters with the wrong sign@@      ext_dot(mul_pc(pl_c, ev_cz)) + (ext_pc_d(pl_d) <<< 16);@@      ext_dot(mul_pc(pl_c, ev_cz)) - (ext_pc_d(pl_d) <<< 16);"
"M07 the d term is scaled by 2^15, not 2^16@@      ext_dot(mul_pc(pl_c, ev_cz)) + (ext_pc_d(pl_d) <<< 16);@@      ext_dot(mul_pc(pl_c, ev_cz)) + (ext_pc_d(pl_d) <<< 15);"
"M08 top and bottom swapped (EXPECTED EQUIVALENT)@@      3'd2: begin  // bottom = row3 + row1
        cmb_row  = 1'b1;
        cmb_add  = 1'b1;
        cmb_none = 1'b0;
      end
      3'd3: begin  // top    = row3 - row1
        cmb_row  = 1'b1;
        cmb_add  = 1'b0;@@      3'd2: begin  // bottom = row3 + row1
        cmb_row  = 1'b1;
        cmb_add  = 1'b0;
        cmb_none = 1'b0;
      end
      3'd3: begin  // top    = row3 - row1
        cmb_row  = 1'b1;
        cmb_add  = 1'b1;"
"M09 THE ROUNDING BECOMES A FLOOR@@          len_ceil[prep_view][prep_plane] <= sq_res[LEN_W-1:0] +
              {{(LEN_W - 1) {1'b0}}, (sq_num != '0)};@@          len_ceil[prep_view][prep_plane] <= sq_res[LEN_W-1:0];"
"M10 the perfect-square test is inverted@@              {{(LEN_W - 1) {1'b0}}, (sq_num != '0)};@@              {{(LEN_W - 1) {1'b0}}, (sq_num == '0)};"
"M11 the recurrence starts at 4^31 (the u64 assumption)@@            sq_bit <= {1'b0, 1'b1, {64{1'b0}}};  // 4^32@@            sq_bit <= {4'b0001, {62{1'b0}}};  // 4^31"
"M12 one recurrence step short@@          if (sq_cnt == 6'(SQRT_STEPS - 1)) state <= S_STORE;@@          if (sq_cnt == 6'(SQRT_STEPS - 2)) state <= S_STORE;"
"M13 the non-taken branch does not shift the root@@            sq_res <= sq_res >> 1;@@            sq_res <= sq_res;"
"M14 the remainder subtracts the root, not the trial@@            sq_num <= sq_num - sq_trial;@@            sq_num <= sq_num - sq_res;"
"M15 the taken branch drops the trial bit@@            sq_res <= (sq_res >> 1) + sq_bit;@@            sq_res <= (sq_res >> 1);"
"M16 only two components enter the sum of squares@@          if (sq_j == 2'd2) begin@@          if (sq_j == 2'd1) begin"
"M17 outside test widened to <=@@  assign outside_here = (dot < -slack);@@  assign outside_here = (dot <= -slack);"
"M18 the slack is added, not subtracted@@  assign outside_here = (dot < -slack);@@  assign outside_here = (dot < slack);"
"M19 the length of plane 0 is used for every plane@@  assign slack = ext_slack(mul_slack(ev_r, len_ceil[ev_view][ev_plane]));@@  assign slack = ext_slack(mul_slack(ev_r, len_ceil[ev_view][3'd0]));"
"M20 view 0's lengths are used for both cameras@@  assign slack = ext_slack(mul_slack(ev_r, len_ceil[ev_view][ev_plane]));@@  assign slack = ext_slack(mul_slack(ev_r, len_ceil[1'b0][ev_plane]));"
"M21 reject when outside EITHER camera, not both@@              reject_o <= ~((ev_active[1] & ~(ev_outside[1] | outside_here)) |
                            (ev_active[0] & ~ev_outside[0]));@@              reject_o <= ~((ev_active[1] & ~(ev_outside[1] | outside_here)) &
                            (ev_active[0] & ~ev_outside[0]));"
"M22 visibility ignores whether the camera is active@@                ev_active[1] & ~(ev_outside[1] | outside_here), ev_active[0] & ~ev_outside[0]@@                ~(ev_outside[1] | outside_here), ~ev_outside[0]"
"M23 the outside bit lands in view 0 always@@          if (outside_here) ev_outside[ev_view] <= 1'b1;@@          if (outside_here) ev_outside[1'b0] <= 1'b1;"
"M24 only four planes are extracted@@          if (prep_plane == 3'd4) begin@@          if (prep_plane == 3'd3) begin"
"M25 only four planes are evaluated@@          if (ev_plane == 3'd4) begin@@          if (ev_plane == 3'd3) begin"
"M26 the second camera is never evaluated@@            if (ev_view == 1'b1) begin@@            if (ev_view == 1'b0) begin"
"M27 ready ignores the dirty view@@  assign ready_o = (state == S_IDLE) && (dirty == 2'b00) && !cfg_mat_we;@@  assign ready_o = (state == S_IDLE) && !cfg_mat_we;"
"M28 ready ignores a write landing this cycle@@  assign ready_o = (state == S_IDLE) && (dirty == 2'b00) && !cfg_mat_we;@@  assign ready_o = (state == S_IDLE) && (dirty == 2'b00);"
"M29 clear dominates set on the dirty bit@@      if (prep_start) dirty[prep_start_view] <= 1'b0;
      if (cfg_mat_we) dirty[cfg_view_i] <= 1'b1;@@      if (cfg_mat_we) dirty[cfg_view_i] <= 1'b1;
      if (prep_start) dirty[prep_start_view] <= 1'b0;"
"M30 the dirty view chosen is the wrong one@@  assign prep_start_view = ~dirty[0];  // view 0 first when both are dirty@@  assign prep_start_view = dirty[0];  // view 0 first when both are dirty"
"M31 the viewport address dirties the cull@@  assign cfg_mat_we = cfg_we_i && (cfg_addr_i < 5'd16);@@  assign cfg_mat_we = cfg_we_i && (cfg_addr_i < 5'd17);"
"M32 every matrix write lands in view 0@@        mat[cfg_view_i][cfg_addr_i[3:0]] <=@@        mat[1'b0][cfg_addr_i[3:0]] <="
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
  if [ ! -x "$EXE" ]; then
    echo "  $name  DISCARDED: the target did not LINK (a build failure would"
    echo "                    otherwise be scored as a caught mutant)"
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
