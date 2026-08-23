#!/usr/bin/env bash
# sweep_texture_tmu.sh — mutation sweep for TEXTURE.TMU's two RTL files.
#
# ---------------------------------------------------------------------------
# THE SWEEP VERIFIES ITS OWN BUILDS. Guards 1-7 are carried unchanged from
# tools/sweep_surface_stamp.sh, which carried them from tools/sweep_geom_skin.sh,
# where each was written after a real instance of scoring a test that never ran:
#
#   1. `verilate()` elaborates at CONFIGURE time, so `ninja` alone relinks a
#      CACHED model against changed source. `cmake -S . -B build` runs every
#      iteration.
#   2. mtime is set to NOW, never forward -- a mutant stamped into the future
#      outranks the pristine source restored after it.
#   3. The whole target directory is deleted each iteration: a pristine model
#      can otherwise be linked against an OBJECT still compiled from a mutant.
#   4. Hash the whole model DIRECTORY. Vzhao_texture_tmu.cpp is Verilator's
#      wrapper and is byte-identical between a pristine and a mutated build.
#   5. THE EXECUTABLE LIVES OUTSIDE THE TARGET DIRECTORY, so a mutant that
#      fails to COMPILE leaves the previous binary in place and the sweep runs
#      that instead -- a build failure scoring as a caught mutant. Delete the
#      exe too and require it after the build.
#   6. The PRISTINE build runs first and must pass, or nothing below means
#      anything.
#   7. EVERY CONSUMER of the mutated file is cleaned, rebuilt and scored.
#
# GUARD 7 REUSES tools/sweep_surface_stamp_consumers.py RATHER THAN COPYING IT.
# That script is named for the run that needed it and is entirely generic: it
# resolves `set(VAR ...)` and expands `${VAR}` before looking for a file inside
# a `verilate()` body, which is exactly the shape the TEXTURE targets use
# (`SOURCES ${ZHAO_TEXTURE_TMU_SV}`). A regex over literal `.sv` paths -- what
# sweep_geom_skin.sh does -- finds NO consumer for either of these files, and a
# guard that finds none aborts. Copying the resolver would have given this
# repository two of it to keep in step.
#
# THE FRONTIER BUILDS ARE COVERAGE, NOT DATA, and here that is the load-bearing
# half of the sweep. `zhao_texture_tmu` multiplexes its four colour channels
# through FILT_LANES instances of `zhao_texture_bilerp` in 4/FILT_LANES passes.
# At the DEFAULT FILT_LANES = 4 there is exactly ONE pass: `pass_c` is the
# constant PASSES-1, `sel_base` is constantly zero, ST_FILT is unreachable and
# the channel mux degenerates to a wire. Mutants M11..M16 damage precisely that
# machinery, and every one of them is ALIVE at the default and dead at 2 and 1.
# Scoring only the default build would have called six mutants survivors and
# sent someone hunting a test gap that is not there.
# (sweep_surface_stamp.sh hit the identical shape on SQ_RADIX and S03/S04, and
# sweep_geom_skin.sh on MUL_LANES and M27.)
#
# The scoring rule: after regeneration every model must EXIST, every exe must
# EXIST, and the hash of the whole set must DIFFER from the pristine set's.
# Anything else is discarded, never scored.
# ---------------------------------------------------------------------------
set -u

TMU_RTL=fpga/rtl/texture/zhao_texture_tmu.sv
BIL_RTL=fpga/rtl/texture/zhao_texture_bilerp.sv
ALL_RTL="$TMU_RTL $BIL_RTL"

BUILD=${ZHAO_BUILD_DIR:-build}

# Guard 7's human-readable half. Both files sit in ZHAO_TEXTURE_TMU_SV, so both
# have the SAME consumer set -- unlike SURFACE.STAMP's three files, whose sets
# differed. Declared separately anyway, so that a future target which takes only
# one of them cannot drift past unnoticed.
DECLARED_TMU="test_texture_tmu_directed test_texture_tmu_lanes1 test_texture_tmu_lanes2 test_texture_tmu_random"
DECLARED_BIL="$DECLARED_TMU"

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

run_lanes() {
  local t
  for t in $1; do
    "./$BUILD/tests/$t.exe" >/dev/null 2>&1 || return 1
  done
  return 0
}

echo "== guard 7: consumers, derived from tests/CMakeLists.txt"
CONS_TMU=$(python tools/sweep_surface_stamp_consumers.py "$TMU_RTL" | tr -d '\r') || exit 9
CONS_BIL=$(python tools/sweep_surface_stamp_consumers.py "$BIL_RTL" | tr -d '\r') || exit 9

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
check_set "zhao_texture_tmu.sv"    "$CONS_TMU" "$DECLARED_TMU"
check_set "zhao_texture_bilerp.sv" "$CONS_BIL" "$DECLARED_BIL"

UNION=$(printf '%s\n' $CONS_TMU $CONS_BIL | sort -u | tr '\n' ' ')

# PREFLIGHT: EVERY MUTANT MUST LINT BEFORE ANY OF THEM IS SCORED.
python tools/sweep_texture_tmu_preflight.py || {
  echo "ABORT: at least one mutant does not build -- fix the mutation, not the guard"
  exit 8
}

echo "== establishing the pristine baseline"
restore_all || { echo "ABORT: cannot establish pristine source"; exit 4; }
rebuild "$UNION"
models_present "$UNION" || { echo "ABORT: a pristine model did not elaborate"; exit 6; }
exes_present "$UNION"   || { echo "ABORT: a pristine target did not link"; exit 6; }
declare -A PRISTINE
PRISTINE[tmu]=$(model_hash "$CONS_TMU")
PRISTINE[bil]=$(model_hash "$CONS_BIL")
if ! run_lanes "$UNION"; then
  echo "ABORT: the PRISTINE build fails its own tests -- nothing below would mean anything"
  exit 7
fi
echo "   pristine models ${PRISTINE[tmu]:0:12}/${PRISTINE[bil]:0:12}, $(echo $UNION | wc -w) lanes green"

# Each entry: name @@ file @@ old @@ new
MUTS=(
# ---- zhao_texture_bilerp.sv : THE FACTORED ARITHMETIC ----------------------
# The three one-LSB traps the contract names, in the shapes they take in the
# factored form, plus the places the factoring itself could be wrong.
"B01 the single rounding is truncated (+32768 dropped)@@fpga/rtl/texture/zhao_texture_bilerp.sv@@    out_o = 8'((s_w + 27'sd32768) >>> 16);@@    out_o = 8'(s_w >>> 16);"
"B02 the rounding constant is half of what rescale asks for@@fpga/rtl/texture/zhao_texture_bilerp.sv@@    out_o = 8'((s_w + 27'sd32768) >>> 16);@@    out_o = 8'((s_w + 27'sd16384) >>> 16);"
"B03 the final rescale shifts 15, i.e. a /255-shaped scale error@@fpga/rtl/texture/zhao_texture_bilerp.sv@@    out_o = 8'((s_w + 27'sd32768) >>> 16);@@    out_o = 8'((s_w + 27'sd32768) >>> 15);"
"B04 the U-lerp taps are transposed (the swapped-weight trap)@@fpga/rtl/texture/zhao_texture_bilerp.sv@@    du1 = \$signed({1'b0, t11_i}) - \$signed({1'b0, t01_i});@@    du1 = \$signed({1'b0, t01_i}) - \$signed({1'b0, t11_i});"
"B05 the u difference runs the wrong way@@fpga/rtl/texture/zhao_texture_bilerp.sv@@    du0 = \$signed({1'b0, t10_i}) - \$signed({1'b0, t00_i});@@    du0 = \$signed({1'b0, t00_i}) - \$signed({1'b0, t10_i});"
"B06 the v difference runs the wrong way@@fpga/rtl/texture/zhao_texture_bilerp.sv@@    dv    = b_s - a_s;@@    dv    = a_s - b_s;"
"B07 the A row's u lerp uses the v fraction@@fpga/rtl/texture/zhao_texture_bilerp.sv@@    pu0 = du0 * fu_s;@@    pu0 = du0 * fv_s;"
"B08 the v lerp uses the u fraction in BOTH stages@@fpga/rtl/texture/zhao_texture_bilerp.sv@@    pu1 = du1 * fu_s;@@    pu1 = du1 * fv_s;"
"B09 the U-stage place value is 128, not 256@@fpga/rtl/texture/zhao_texture_bilerp.sv@@    a_s = \$signed({2'b00, t00_i, 8'd0}) + pu0;@@    a_s = \$signed({2'b00, t00_i, 8'd0}) - pu0;"
"B10 the B lerp is seeded from the wrong tap@@fpga/rtl/texture/zhao_texture_bilerp.sv@@    b_s = \$signed({2'b00, t01_i, 8'd0}) + pu1;@@    b_s = \$signed({2'b00, t00_i, 8'd0}) + pu1;"
"B11 the V-stage place value is 128, not 256@@fpga/rtl/texture/zhao_texture_bilerp.sv@@    s_w   = (a_ext <<< 8) + pv;@@    s_w   = (a_ext <<< 7) + pv;"
"B12 the V lerp drops its A term, so the filter forgets the u row@@fpga/rtl/texture/zhao_texture_bilerp.sv@@    s_w   = (a_ext <<< 8) + pv;@@    s_w   = (a_ext <<< 8) - pv;"
"B13 the v lerp is based on the wrong u row@@fpga/rtl/texture/zhao_texture_bilerp.sv@@    a_ext = 27'(a_s);@@    a_ext = 27'(b_s);"
# ---- zhao_texture_tmu.sv : THE CHANNEL MUX AND THE PASS COUNTER ------------
# These six are the FRONTIER's reason for existing. Every one is textually live
# and behaviourally INVISIBLE at FILT_LANES = 4, where there is one pass and
# `sel_base` is constantly zero.
"M11 the channel select ignores the pass, so pass 0's channels are filtered every pass@@fpga/rtl/texture/zhao_texture_tmu.sv@@      assign tsel = ch_pack[sel_base | 2'(gj)];@@      assign tsel = ch_pack[2'(gj)];"
"M12 ST_OUT filters the pass ST_FILT last ran instead of the LAST pass@@fpga/rtl/texture/zhao_texture_tmu.sv@@    pass_c   = (st_r == ST_OUT) ? 2'(PASSES - 1) : pass_r;@@    pass_c   = pass_r;"
"M13 the lane base shifts the wrong way@@fpga/rtl/texture/zhao_texture_tmu.sv@@    sel_base = 2'(pass_c << LANE_SHIFT);@@    sel_base = 2'(pass_c >> LANE_SHIFT);"
"M14 the held-channel bank is written at the wrong channel index@@fpga/rtl/texture/zhao_texture_tmu.sv@@            fres_r[{(sel_base | 2'(j)), 3'd0} +: 8] <= bl_out[8*j +: 8];@@            fres_r[{2'(j), 3'd0} +: 8] <= bl_out[8*j +: 8];"
"M15 the pass counter never advances@@fpga/rtl/texture/zhao_texture_tmu.sv@@            pass_r <= pass_r + 2'd1;@@            pass_r <= pass_r;"
"M16 ST_FILT always leaves after the FIRST pass (EQUIVALENT at FILT_LANES 4 and 2)@@fpga/rtl/texture/zhao_texture_tmu.sv@@  localparam int unsigned LAST_FILT_PASS = (PASSES > 1) ? (PASSES - 2) : 0;@@  localparam int unsigned LAST_FILT_PASS = 0;"
# ---- zhao_texture_tmu.sv : the channel order and the sample assembly -------
"M17 red and blue are transposed on the way into the filter@@fpga/rtl/texture/zhao_texture_tmu.sv@@      ch_pack[0][8*k +: 8] = dec_c[k][23:16];  // R@@      ch_pack[0][8*k +: 8] = dec_c[k][7:0];    // R"
"M18 a CLUT texel reports the filter's alpha instead of the law's 255@@fpga/rtl/texture/zhao_texture_tmu.sv@@  assign smp_a_o      = q_clut_r ? 8'd255 : fin[3];@@  assign smp_a_o      = fin[3];"
"M19 the held channels are ignored, so only the last pass survives@@fpga/rtl/texture/zhao_texture_tmu.sv@@        assign fin[gc] = fres_r[8*gc +: 8];@@        assign fin[gc] = bl_out[7:0];"
# ---- zhao_texture_tmu.sv : the behaviour that must NOT have moved ----------
# The contract's own mutation table (2026-08-18) names these; they are re-run
# because a rearchitecture that leaves them uncaught has broken the suite, not
# just the block.
"M20 wrap MIRROR clamps instead of folding@@fpga/rtl/texture/zhao_texture_tmu.sv@@        WRAP_MIRROR: begin
          per = tu_ & ((mask << 1) | 32'd1);@@        WRAP_MIRROR: begin
          per = (tu_ > mask) ? mask : tu_;"
"M21 wrap REPEAT clamps instead of repeating@@fpga/rtl/texture/zhao_texture_tmu.sv@@        WRAP_REPEAT: wrap_coord = tu_ & mask;@@        WRAP_REPEAT: wrap_coord = (tu_ > mask) ? mask : tu_;"
"M22 the mip level is off by one@@fpga/rtl/texture/zhao_texture_tmu.sv@@    lvl_req = m_mip_en ? req_lod_i[7:4] : 4'd0;@@    lvl_req = m_mip_en ? (req_lod_i[7:4] + 4'd1) : 4'd0;"
"M23 the level offset closed form reads the next repunit@@fpga/rtl/texture/zhao_texture_tmu.sv@@    lvl_off   = (level == 4'd0) ? 32'd0 : (REP4[level] << lvl_shift);@@    lvl_off   = (level == 4'd0) ? 32'd0 : (REP4[level + 4'd1] << lvl_shift);"
"M24 the CLUT index is forced to zero (the alpha-test case lost)@@fpga/rtl/texture/zhao_texture_tmu.sv@@              idx_r             <= bus_idx;@@              idx_r             <= 8'd0;"
"M25 the half-texel bias is dropped@@fpga/rtl/texture/zhao_texture_tmu.sv@@    tu_b = filter_eff ? (tu_q - 48'sd32768) : tu_q;@@    tu_b = tu_q;"
"M26 stars 1 is not enforced in the fabric: a palette IS filtered@@fpga/rtl/texture/zhao_texture_tmu.sv@@    filter_eff   = m_filter && !is_clut;@@    filter_eff   = m_filter;"
"M27 a bilinear request fetches one tap instead of four@@fpga/rtl/texture/zhao_texture_tmu.sv@@            q_en_r       <= filter_eff ? 4'b1111 : 4'b0001;@@            q_en_r       <= 4'b0001;"
)

# ---------------------------------------------------------------------------
# ZHAO_SWEEP_ONLY — re-score a NAMED SUBSET, for confirming a fix.
#
# THE BANNER BELOW IS NOT DECORATION. A filtered run produces a score line that
# looks exactly like a full run's, and this repository's whole failure history
# is partial evidence read as complete. The banner, the [FILTERED] tag on the
# score line and the refusal to match nothing are all there so a filtered log
# cannot be mistaken for a sweep.
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
    "$TMU_RTL") cons=$CONS_TMU; key=tmu ;;
    "$BIL_RTL") cons=$CONS_BIL; key=bil ;;
    *) echo "  $name  ABORT: unknown file '$file'"; exit 3 ;;
  esac

  restore_all || { echo "  $name  ABORT: could not restore before applying"; exit 4; }

  OLD="$old" NEW="$new" RTL="$file" python - <<'PY'
import io, os, sys
p = os.environ['RTL']
raw = io.open(p, encoding='utf-8', newline='').read()
CR, LF = chr(13), chr(10)
NL = CR + LF if CR + LF in raw else LF
o = os.environ['OLD'].replace(LF, NL)
n = os.environ['NEW'].replace(LF, NL)
if raw.count(o) != 1:
    sys.stderr.write('ANCHOR NOT UNIQUE (%d)\n' % raw.count(o)); sys.exit(9)
if o == n:
    sys.stderr.write('MUTANT IDENTICAL TO BASE\n'); sys.exit(9)
io.open(p, 'w', encoding='utf-8', newline='').write(raw.replace(o, n, 1))
os.utime(p, None)   # NOW, never the future -- see the header
PY
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
