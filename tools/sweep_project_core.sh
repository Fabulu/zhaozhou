#!/usr/bin/env bash
# sweep_project_core.sh — mutation sweep for the merged projector.
#
# ---------------------------------------------------------------------------
# WHAT THIS SWEEPS, AND WHY IT IS THREE FILES AND NOT ONE.
#
# RUN-20260824-0522 extracted `zhao_project_core.sv` from `zhao_geom_project.sv`
# and `zhao_terrain_project.sv`, which had contained byte-identical copies of
# `zref::render::project_vertex`. The sweep therefore covers all three: the core
# holds the law, and the two shells hold the handshakes, the sequencer, the
# triangle assembler and the counters that the merge had to leave untouched.
#
# A MUTATION IN THE CORE CHANGES BOTH SHELLS. That is the whole point of the
# merge and it is also the sweep's main risk: guard 7 derives the consumer set
# per file from tests/CMakeLists.txt, so a core mutant is scored against all
# four lanes and a shell mutant only against that shell's.
#
# GUARDS 1-7 ARE CARRIED UNCHANGED from tools/sweep_surface_sheet.sh, which
# carried them from tools/sweep_texture_tmu.sh, which carried them from
# tools/sweep_surface_stamp.sh. Each was written after a real instance of
# scoring a test that never ran:
#
#   1. `verilate()` elaborates at CONFIGURE time, so `ninja` alone relinks a
#      CACHED model against changed source. `cmake -S . -B build` runs every
#      iteration.
#   2. mtime is set to NOW, never forward.
#   3. The whole target directory is deleted each iteration.
#   4. Hash the whole model DIRECTORY, not one file.
#   5. THE EXECUTABLE LIVES OUTSIDE THE TARGET DIRECTORY, so a mutant that fails
#      to COMPILE would otherwise leave the previous binary in place and be
#      scored as caught. Delete the exe too and require it after the build.
#   6. The PRISTINE build runs first and must pass.
#   7. EVERY CONSUMER of the mutated file is cleaned, rebuilt and scored.
#
# GUARD 8, ADDED HERE: THE RANDOM LANES ARE RUN, NOT JUST THE DIRECTED ONES.
# `test_geom_project_directed` with no arguments runs only its directed suite;
# ctest also invokes it as `--random 100` and `--random 2000`, and
# `test_terrain_project_random` as `--nightly`. Every sweep in this tree so far
# has run each consumer exe bare, which scores a mutant against a strictly
# smaller test set than CI applies. LANE_ARGS below fixes that for this sweep.
#
# THE MUTANT TABLE IS IN THREE HALVES.
#
#   M01-M08 are THE FIVE DEFECTS THE BRIEF NAMES FOR THIS CHANGE, plus their
#   near neighbours: a view/caller tag swapped between the two users, a config
#   write landing in the wrong view's matrix, the viewport transform applied
#   with the other view's viewport, results returned out of order, and a stale
#   matrix used after a reconfiguration.
#
#     A NOTE ON "BOTH CALLERS IN FLIGHT", because the wording matters. The two
#     shells each instantiate their OWN core (see the run log for the measured
#     consequence), so there is no state shared between the two callers for a
#     mutant to corrupt. The faithful translation of "the two users" is
#     therefore "the two VIEWS", which IS shared state -- one register file,
#     two matrix sets, selected per vertex -- and "results out of order" is the
#     three corners of a triangle, which is the only reordering this design can
#     express. M01-M08 are those.
#
#   M09-M16 attack THE LAW, which the merge moved from two files into one and
#   which must not have changed while it moved.
#
#   M17-M23 attack THE SEAM ITSELF -- the caller-owned enable, the opaque
#   payload riding in lockstep, `busy_o`, and each shell's handshake. These are
#   the defects the extraction newly made possible and that no earlier sweep
#   could have contained.
#
# THE SCORING RULE: after regeneration every model must EXIST, every exe must
# EXIST, and the hash of the whole set must DIFFER from the pristine set's.
# Anything else is discarded, never scored.
# ---------------------------------------------------------------------------
set -u

CORE_RTL=fpga/rtl/common/zhao_project_core.sv
GEOM_RTL=fpga/rtl/geometry/zhao_geom_project.sv
TERR_RTL=fpga/rtl/terrain/zhao_terrain_project.sv
ALL_RTL="$CORE_RTL $GEOM_RTL $TERR_RTL"

BUILD=${ZHAO_BUILD_DIR:-build}

# Guard 7's human-readable half: what tests/CMakeLists.txt is expected to reach.
# If a target is added or removed, this list and the derived one disagree and
# the sweep ABORTS rather than quietly scoring a smaller set.
DECLARED_CORE="test_geom_project_directed test_terrain_project_chain test_terrain_project_directed test_terrain_project_random"
DECLARED_GEOM="test_geom_project_directed"
DECLARED_TERR="test_terrain_project_chain test_terrain_project_directed test_terrain_project_random"

# Guard 8: the argument sets ctest actually uses. A lane with no entry runs bare.
lane_args() {
  case "$1" in
    test_geom_project_directed) echo "|--random 2000" ;;
    test_terrain_project_random) echo "|--nightly" ;;
    *) echo "" ;;
  esac
}

declare -A GOLD GOLDHASH
for f in $ALL_RTL; do
  g=$(mktemp)
  cp "$f" "$g"
  GOLD[$f]=$g
  GOLDHASH[$f]=$(sha256sum <"$g" | cut -d' ' -f1)
done

restore_one() {
  local f=$1 i
  for i in 1 2 3 4 5 6 7 8 9 10; do
    cp "${GOLD[$f]}" "$f" 2>/dev/null
    if [ "$(sha256sum <"$f" | cut -d' ' -f1)" = "${GOLDHASH[$f]}" ]; then return 0; fi
    sleep 1
  done
  return 1
}
restore_all() { local f; for f in $ALL_RTL; do restore_one "$f" || return 1; done; return 0; }

model_hash() {
  local t h=""
  for t in $1; do
    h="$h$(find "$BUILD/tests/CMakeFiles/$t.dir" -type f \( -name "*.cpp" -o -name "*.h" \) \
             2>/dev/null | sort | xargs sha256sum 2>/dev/null | sha256sum | cut -d" " -f1)"
  done
  printf '%s' "$h" | sha256sum | cut -d" " -f1
}

models_present() { local t; for t in $1; do [ -d "$BUILD/tests/CMakeFiles/$t.dir" ] || return 1; done; return 0; }
exes_present()   { local t; for t in $1; do [ -x "$BUILD/tests/$t.exe" ]           || return 1; done; return 0; }

rebuild() {
  local t
  for t in $1; do
    rm -rf "$BUILD/tests/CMakeFiles/$t.dir"
    rm -f "$BUILD/tests/$t.exe"    # guard 5
  done
  cmake -S . -B "$BUILD" >/dev/null 2>&1
  # shellcheck disable=SC2086
  ninja -C "$BUILD" $1 >/dev/null 2>&1
}

# Guard 8: run each consumer bare AND with every extra argument set ctest uses.
run_lanes() {
  local t extra IFS_SAVE
  for t in $1; do
    "./$BUILD/tests/$t.exe" >/dev/null 2>&1 || return 1
    IFS_SAVE=$IFS
    IFS='|'
    for extra in $(lane_args "$t"); do
      [ -z "$extra" ] && continue
      IFS=$IFS_SAVE
      # shellcheck disable=SC2086
      "./$BUILD/tests/$t.exe" $extra >/dev/null 2>&1 || { IFS=$IFS_SAVE; return 1; }
      IFS='|'
    done
    IFS=$IFS_SAVE
  done
  return 0
}

echo "== guard 7: consumers, derived from tests/CMakeLists.txt"
CONS_CORE=$(python tools/sweep_surface_stamp_consumers.py "$CORE_RTL" | tr -d '\r') || exit 9
CONS_GEOM=$(python tools/sweep_surface_stamp_consumers.py "$GEOM_RTL" | tr -d '\r') || exit 9
CONS_TERR=$(python tools/sweep_surface_stamp_consumers.py "$TERR_RTL" | tr -d '\r') || exit 9

check_set() {  # $1 = label, $2 = derived, $3 = declared
  local t
  printf '   %-30s %s\n' "$1" "$2"
  for t in $2; do
    case " $3 " in *" $t "*) ;; *) echo "ABORT: derived '$t' not in DECLARED for $1"; exit 9 ;; esac
  done
  for t in $3; do
    case " $2 " in *" $t "*) ;; *) echo "ABORT: DECLARED names '$t' for $1, which no verilate() reaches"; exit 9 ;; esac
  done
}
check_set "zhao_project_core.sv" "$CONS_CORE" "$DECLARED_CORE"
check_set "zhao_geom_project.sv" "$CONS_GEOM" "$DECLARED_GEOM"
check_set "zhao_terrain_project.sv" "$CONS_TERR" "$DECLARED_TERR"

# THE CORE'S CONSUMER SET MUST BE THE UNION OF THE TWO SHELLS'. If it is not,
# the extraction did not actually land in both callers and every core mutant
# below would be scored against a smaller set than it reaches.
for t in $CONS_GEOM $CONS_TERR; do
  case " $CONS_CORE " in *" $t "*) ;; *) echo "ABORT: '$t' consumes a shell but not the core"; exit 9 ;; esac
done

UNION="$CONS_CORE"

# PREFLIGHT: EVERY MUTANT MUST LINT BEFORE ANY OF THEM IS SCORED.
python tools/sweep_project_core_preflight.py || {
  echo "ABORT: at least one mutant does not build -- fix the mutation, not the guard"
  exit 8
}

echo "== establishing the pristine baseline"
restore_all || { echo "ABORT: cannot establish pristine source"; exit 4; }
rebuild "$UNION"
models_present "$UNION" || { echo "ABORT: a pristine model did not elaborate"; exit 6; }
exes_present "$UNION"   || { echo "ABORT: a pristine target did not link"; exit 6; }
declare -A PRISTINE
PRISTINE[core]=$(model_hash "$CONS_CORE")
PRISTINE[geom]=$(model_hash "$CONS_GEOM")
PRISTINE[terr]=$(model_hash "$CONS_TERR")
if ! run_lanes "$UNION"; then
  echo "ABORT: the PRISTINE build fails its own tests -- nothing below would mean anything"
  exit 7
fi
echo "   pristine models ${PRISTINE[core]:0:12}, $(echo $UNION | wc -w) lanes green"

# Each entry: name @@ file @@ old @@ new
MUTS=(
# ---- M01-M08 : THE FIVE DEFECTS THE BRIEF NAMES, AND THEIR NEIGHBOURS -------
"M01 the viewport transform uses the OTHER view's viewport@@fpga/rtl/common/zhao_project_core.sv@@    cx13 = {1'b0, vp_x0[s5_view]} + {2'b0, vp_w[s5_view][11:1]};@@    cx13 = {1'b0, vp_x0[!s5_view]} + {2'b0, vp_w[!s5_view][11:1]};"
"M02 a config write lands in the WRONG VIEW'S MATRIX@@fpga/rtl/common/zhao_project_core.sv@@        mat[cfg_view_i][cfg_addr_i[3:0]] <= \$signed(cfg_data_i);@@        mat[!cfg_view_i][cfg_addr_i[3:0]] <= \$signed(cfg_data_i);"
"M03 a config write lands in the wrong view's VIEWPORT@@fpga/rtl/common/zhao_project_core.sv@@        vp_w[cfg_view_i] <= cfg_data_i[11:0];@@        vp_w[!cfg_view_i] <= cfg_data_i[11:0];"
"M04 the row sums select the matrix by the WRONG VIEW TAG@@fpga/rtl/common/zhao_project_core.sv@@    row_cw = ext64(mul32(mat[view_i][12], vx_i)) + ext64(mul32(mat[view_i][13], vy_i)) +@@    row_cw = ext64(mul32(mat[!view_i][12], vx_i)) + ext64(mul32(mat[!view_i][13], vy_i)) +"
"M05 STALE MATRIX: row 0 is pinned to view 0 whatever the packet says@@fpga/rtl/common/zhao_project_core.sv@@    row_x = ext64(mul32(mat[view_i][0], vx_i)) + ext64(mul32(mat[view_i][1], vy_i)) +@@    row_x = ext64(mul32(mat[1'b0][0], vx_i)) + ext64(mul32(mat[1'b0][1], vy_i)) +"
"M06 the view does not RIDE the pipeline: stage 6 uses the INCOMING view@@fpga/rtl/common/zhao_project_core.sv@@      s5_view <= dstep_view[DIV_STEPS];@@      s5_view <= view_i;"
"M07 RESULTS OUT OF ORDER: the corner index is one ahead of its vertex@@fpga/rtl/terrain/zhao_terrain_project.sv@@      .payload_i ({job_k, job_src, job_mat_a, job_mat_b, job_weight}),@@      .payload_i ({job_k + 2'd1, job_src, job_mat_a, job_mat_b, job_weight}),"
"M08 RESULTS OUT OF ORDER: B takes C's coordinates at reassembly@@fpga/rtl/terrain/zhao_terrain_project.sv@@          out_bx_o     <= acc_x[1];@@          out_bx_o     <= s6_px;"
# ---- M09-M16 : THE LAW, WHICH MOVED FILES AND MUST NOT HAVE MOVED ----------
"M09 the near plane becomes STRICT: w == 0 moves to the accept side@@fpga/rtl/common/zhao_project_core.sv@@    pre_behind = (s2_cw <= 32'sd0);@@    pre_behind = (s2_cw < 32'sd0);"
"M10 a behind-the-eye vertex KEEPS its coordinates instead of zeroing@@fpga/rtl/common/zhao_project_core.sv@@      out_x_o <= s5_behind ? 21'sd0 : to_screen_xy(scr_fx_x);@@      out_x_o <= to_screen_xy(scr_fx_x);"
"M11 the row rescale rounds TOWARD ZERO instead of half up@@fpga/rtl/common/zhao_project_core.sv@@      r = (x + 68'sd32768) >>> 16;@@      r = x >>> 16;"
"M12 the fx_mad rescale rounds toward zero@@fpga/rtl/common/zhao_project_core.sv@@      r = (x + 64'sd32768) >>> 16;@@      r = x >>> 16;"
"M13 the guard band clamps ONE COUNT SHORT of the rail@@fpga/rtl/common/zhao_project_core.sv@@      if (r > 41'sd524288) to_screen_xy = 21'sd524288;@@      if (r > 41'sd524287) to_screen_xy = 21'sd524287;"
"M14 the 1/w lane divides the WRONG NUMERATOR@@fpga/rtl/common/zhao_project_core.sv@@    pre_n[2] = 48'h0001_0000_0000;  // (1 << 16) << 16@@    pre_n[2] = 48'h0002_0000_0000;  // (1 << 16) << 16"
"M15 the divider's SATURATION COMPARE is dropped@@fpga/rtl/common/zhao_project_core.sv@@      pre_sat[li] = ({14'b0, pre_h[li][47:31]} >= pre_d);@@      pre_sat[li] = 1'b0;"
"M16 the divisor is NOT forced to 1 on the behind-the-eye path@@fpga/rtl/common/zhao_project_core.sv@@    pre_d  = pre_behind ? 31'd1 : s2_cw[30:0];@@    pre_d  = s2_cw[30:0];"
# ---- M17-M23 : THE SEAM, WHICH THE EXTRACTION NEWLY MADE POSSIBLE -----------
"M17 the CALLER'S ENABLE is ignored by the main pipeline@@fpga/rtl/common/zhao_project_core.sv@@    end else if (en_i) begin
      // stage 1@@    end else if (1'b1) begin
      // stage 1"
"M18 the caller's enable is ignored by the DIVIDER LANES@@fpga/rtl/common/zhao_project_core.sv@@          if (!rst_n) r_dv <= '0;
          else if (en_i) r_dv <= nxt;@@          if (!rst_n) r_dv <= '0;
          else r_dv <= nxt;"
"M19 the PAYLOAD does not ride in lockstep -- it skips the divider@@fpga/rtl/common/zhao_project_core.sv@@      s5_pay <= dstep_pay[DIV_STEPS];@@      s5_pay <= s3_pay;"
"M20 busy_o forgets the OUTPUT REGISTER, so idle_o lies one cycle early@@fpga/rtl/common/zhao_project_core.sv@@    busy_o = s1_valid || s2_valid || s5_valid || out_valid_o;@@    busy_o = s1_valid || s2_valid || s5_valid;"
"M21 GEOM: v_ready_o no longer tracks the stall@@fpga/rtl/geometry/zhao_geom_project.sv@@  assign v_ready_o = advance;@@  assign v_ready_o = 1'b1;"
"M22 TERRAIN: tri_ready_o forgets the sequencer is busy@@fpga/rtl/terrain/zhao_terrain_project.sv@@  assign tri_ready_o = advance && job_free;@@  assign tri_ready_o = advance;"
"M23 TERRAIN: idle_o asserts while a triangle still waits at the output@@fpga/rtl/terrain/zhao_terrain_project.sv@@  assign idle_o = !job_valid && !core_busy && !out_valid_r;@@  assign idle_o = !job_valid && !core_busy;"
)

# ---------------------------------------------------------------------------
# ZHAO_SWEEP_ONLY — re-score a NAMED SUBSET, for confirming a fix.
#
# THE BANNER BELOW IS NOT DECORATION. A filtered run produces a score line that
# looks exactly like a full run's, and this repository's whole failure history
# is partial evidence read as complete.
# ---------------------------------------------------------------------------
ONLY=${ZHAO_SWEEP_ONLY:-}
SEL=()
for entry in "${MUTS[@]}"; do
  nm=${entry%%@@*}; id=${nm%% *}
  if [ -z "$ONLY" ]; then
    SEL+=("$entry")
  else
    case " $ONLY " in *" $id "*) SEL+=("$entry") ;; esac
  fi
done
if [ -n "$ONLY" ]; then
  if [ ${#SEL[@]} -eq 0 ]; then
    echo "ABORT: ZHAO_SWEEP_ONLY='$ONLY' matched no mutant. A filtered run that"
    echo "       scores an empty set is the failure this sweep's preflight exists"
    echo "       to prevent; it is not allowed here either."
    exit 10
  fi
  echo "########################################################################"
  echo "## FILTERED RUN — ${#SEL[@]} of ${#MUTS[@]} mutants: $ONLY"
  echo "## THIS IS NOT A SWEEP SCORE. It confirms a fix on named mutants only."
  echo "########################################################################"
fi

expected=${#SEL[@]}
attempted=0
accounted=0
caught=0
survivors=()

for entry in "${SEL[@]}"; do
  name=${entry%%@@*}
  rest=${entry#*@@}
  file=${rest%%@@*}
  rest=${rest#*@@}
  old=${rest%%@@*}
  new=${rest#*@@}
  attempted=$((attempted + 1))

  case "$file" in
    "$CORE_RTL") cons=$CONS_CORE; key=core ;;
    "$GEOM_RTL") cons=$CONS_GEOM; key=geom ;;
    "$TERR_RTL") cons=$CONS_TERR; key=terr ;;
    *) echo "  $name  ABORT: unknown file '$file'"; exit 3 ;;
  esac

  restore_all || { echo "  $name  ABORT: could not restore before applying"; exit 4; }

  OLD="$old" NEW="$new" F="$file" python tools/sweep_apply_mutant.py
  if [ $? -ne 0 ]; then
    echo "  $name  ABORT: could not apply"
    restore_all
    exit 3
  fi

  if [ "$(sha256sum <"$file" | cut -d' ' -f1)" = "${GOLDHASH[$file]}" ]; then
    echo "  $name  ABORT: source unchanged after apply"
    restore_all
    exit 3
  fi

  rebuild "$cons"

  if ! models_present "$cons"; then
    echo "  $name  DISCARDED: a model was absent after regeneration"
    restore_all || { echo "ABORT: revert failed"; exit 4; }
    continue
  fi
  if ! exes_present "$cons"; then
    echo "  $name  DISCARDED: a target did not LINK (a build failure would"
    echo "                    otherwise be scored as a caught mutant)"
    restore_all || { echo "ABORT: revert failed"; exit 4; }
    continue
  fi
  if [ "$(model_hash "$cons")" = "${PRISTINE[$key]}" ]; then
    echo "  $name  DISCARDED: models identical to pristine (did not re-elaborate)"
    restore_all || { echo "ABORT: revert failed"; exit 4; }
    continue
  fi

  if run_lanes "$cons"; then
    echo "  $name  *** SURVIVED ***"
    survivors+=("$name")
  else
    echo "  $name  caught"
    caught=$((caught + 1))
  fi
  accounted=$((accounted + 1))

  restore_all || { echo "  $name  ABORT: revert was not byte-identical"; exit 4; }
done

echo "== restoring the pristine build"
restore_all || { echo "ABORT: final restore failed"; exit 4; }
rebuild "$UNION"

echo "----"
TAG=""
if [ -n "$ONLY" ]; then TAG=" [FILTERED: $ONLY -- NOT a sweep score]"; fi
echo "attempted=$attempted expected=$expected accounted=$accounted caught=$caught$TAG"
for s in "${survivors[@]:-}"; do [ -n "$s" ] && echo "SURVIVOR: $s"; done
if [ "$attempted" != "$expected" ] || [ "$accounted" != "$expected" ]; then
  echo "CROSS-CHECK FAILED (attempted/accounted must both equal $expected)"
  exit 5
fi
