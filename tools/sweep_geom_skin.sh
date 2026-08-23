#!/usr/bin/env bash
# sweep_geom_skin.sh — mutation sweep for zhao_geom_skin.sv.
#
# ---------------------------------------------------------------------------
# THE SWEEP VERIFIES ITS OWN BUILDS. Every guard below was written for
# tools/sweep_geom_lod.sh and tools/sweep_field_dsp.sh, where SEVEN separate
# ways of scoring a test that never ran were hit for real. They are carried
# here unchanged rather than rediscovered:
#
#   1. `verilate()` elaborates at CONFIGURE time, so `ninja` alone relinks a
#      CACHED model against changed source. `cmake -S . -B build` runs every
#      iteration.
#   2. Stamping the mutated source's mtime into the FUTURE makes a mutant model
#      look newer than the pristine source restored after it, so the next
#      elaboration is skipped and a mutant model is served against clean RTL.
#      mtime is set to NOW, never forward.
#   3. Hashing the generated model is necessary but not sufficient: a pristine
#      model can be linked against an OBJECT still compiled from a mutant. The
#      whole target directory is deleted each iteration.
#   4. Hash the whole model DIRECTORY, not Vzhao_geom_skin.cpp — that file is
#      Verilator's wrapper and is byte-identical between a pristine and a
#      mutated build. The logic lives in Vzhao_geom_skin___024root__0.cpp.
#   5. THE EXECUTABLE LIVES OUTSIDE THE TARGET DIRECTORY, so a mutant that
#      fails to COMPILE leaves the previous binary in place and the sweep runs
#      that instead — a build failure scoring as a caught mutant, the most
#      flattering possible way to be wrong. Delete the exe too and require it
#      to exist after the build. (That error once inflated a real 22/23 to a
#      reported 21/22.)
#   6. The PRISTINE build is run first. If it does not pass its own tests, every
#      "caught" below would be meaningless.
#   7. EVERY CONSUMER of the mutated file is cleaned, rebuilt and scored. The
#      consumer set is DERIVED from tests/CMakeLists.txt at run time and
#      cross-checked against the human-readable list below; a disagreement is
#      an abort, not a warning.
#
# Guard 7 is not ceremony here. `zhao_geom_skin.sv` is elaborated by THREE
# targets, one per point on its MUL_LANES frontier, and two of the mutants
# below are alive in one configuration and dead in another. A sweep that
# scored only the default build would have reported them as survivors and
# invented a test gap that does not exist.
#
# The scoring rule: after regeneration every model must EXIST and the hash of
# the whole set must DIFFER from the pristine set's. Anything else is
# discarded, never scored.
# ---------------------------------------------------------------------------
set -u

RTL=fpga/rtl/geometry/zhao_geom_skin.sv

# The build tree. Defaults to the gate's own `build`, which is what CI and every
# other sweep use. It is overridable ONLY because this repository runs several
# agents at once and a second agent reconfiguring `build` mid-sweep would make
# every result meaningless -- the sweep is the same either way, since the guards
# are about regeneration and not about which directory it happens in.
BUILD=${ZHAO_BUILD_DIR:-build}

# Guard 7's human-readable half. The derived set must equal this exactly.
DECLARED_TARGETS="test_geom_skin_directed test_geom_skin_lanes1 test_geom_skin_lanes6"

GOLD=$(mktemp)
cp "$RTL" "$GOLD"
GOLDHASH=$(sha256sum <"$GOLD" | cut -d' ' -f1)

restore() {
  local i
  for i in 1 2 3 4 5 6 7 8 9 10; do
    cp "$GOLD" "$RTL" 2>/dev/null
    if [ "$(sha256sum <"$RTL" | cut -d' ' -f1)" = "$GOLDHASH" ]; then return 0; fi
    sleep 1
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

# The directed lane AND the pose-range random lane, for every consumer. A
# mutation that only shows up at 1-ULP residues needs the random lane; one that
# only shows up at the operand rails needs the directed lane's section 7.
run_lanes() {
  local t
  for t in $1; do
    "./$BUILD/tests/$t.exe" >/dev/null 2>&1 || return 1
    "./$BUILD/tests/$t.exe" --random 2000 >/dev/null 2>&1 || return 1
  done
  return 0
}

echo "== guard 7: consumers, derived from tests/CMakeLists.txt"
ALL=$(consumers_of "$RTL" | tr -d '\r')
if [ -z "$ALL" ]; then
  echo "ABORT: no target in tests/CMakeLists.txt verilates $RTL."
  echo "       A mutation there would be applied, built into nothing, and"
  echo "       scored against a stale binary. See guard 7."
  exit 9
fi
printf '   %-40s %s\n' "$(basename "$RTL")" "$ALL"
for t in $ALL; do
  case " $DECLARED_TARGETS " in
    *" $t "*) ;;
    *) echo "ABORT: derived consumer '$t' is not in DECLARED_TARGETS."; exit 9 ;;
  esac
done
for t in $DECLARED_TARGETS; do
  case " $ALL " in
    *" $t "*) ;;
    *) echo "ABORT: DECLARED_TARGETS names '$t', which no verilate() reaches."; exit 9 ;;
  esac
done

# PREFLIGHT: EVERY MUTANT MUST LINT BEFORE ANY OF THEM IS SCORED.
python tools/sweep_geom_skin_preflight.py || {
  echo "ABORT: at least one mutant does not build -- fix the mutation, not the guard"
  exit 8
}

echo "== establishing the pristine baseline"
restore || { echo "ABORT: cannot establish pristine source"; exit 4; }
rebuild "$ALL"
models_present "$ALL" || { echo "ABORT: a pristine model did not elaborate"; exit 6; }
exes_present "$ALL"   || { echo "ABORT: a pristine target did not link"; exit 6; }
PRISTINE_MODEL=$(model_hash "$ALL")
if ! run_lanes "$ALL"; then
  echo "ABORT: the PRISTINE build fails its own tests -- nothing below would mean anything"
  exit 7
fi
echo "   pristine model ${PRISTINE_MODEL:0:16}, all $(echo $ALL | wc -w) lanes green"

# Each entry: name @@ old @@ new
MUTS=(
"M01 the rigid boundary is w0 == 63, not 64@@        rigid_q <= v_rigid_i || (v_w0_i == 7'd64);@@        rigid_q <= v_rigid_i || (v_w0_i == 7'd63);"
"M02 rigid path rescales by 22, not 16@@  assign res_row = rigid_q ? rescale_sat(BLENDW'(pa_sel), 16) : rescale_sat(blend_v, 22);@@  assign res_row = rigid_q ? rescale_sat(BLENDW'(pa_sel), 22) : rescale_sat(blend_v, 22);"
"M03 blend rescales by 16, not 22@@  assign res_row = rigid_q ? rescale_sat(BLENDW'(pa_sel), 16) : rescale_sat(blend_v, 22);@@  assign res_row = rigid_q ? rescale_sat(BLENDW'(pa_sel), 16) : rescale_sat(blend_v, 16);"
"M04 round-half-up becomes a floor@@      r = (v + (73'sd1 <<< (sh - 1))) >>> sh;@@      r = v >>> sh;"
"M05 the positive saturation rail is off by one@@      if (r > 73'sd2147483647) rescale_sat = 32'sh7FFF_FFFF;@@      if (r > 73'sd2147483647) rescale_sat = 32'sh7FFF_FFFE;"
"M06 the negative saturation rail is off by one@@      else if (r < -73'sd2147483648) rescale_sat = 32'sh8000_0000;@@      else if (r < -73'sd2147483648) rescale_sat = 32'sh8000_0001;"
"M07 the pb term is scaled by 2^5, not 2^6@@  assign blend_v = (BLENDW'(pb_sel) <<< 6) + wprod;@@  assign blend_v = (BLENDW'(pb_sel) <<< 5) + wprod;"
"M08 the weight difference is reversed@@  assign pdiff  = DIFFW'(pa_sel) - DIFFW'(pb_sel);@@  assign pdiff  = DIFFW'(pb_sel) - DIFFW'(pa_sel);"
"M09 weight bit 0 contributes 2^1@@    wp0 = (w0_q[0] ? pd_ext : ZERO_B) + (w0_q[1] ? (pd_ext <<< 1) : ZERO_B);@@    wp0 = (w0_q[0] ? (pd_ext <<< 1) : ZERO_B) + (w0_q[1] ? (pd_ext <<< 1) : ZERO_B);"
"M10 weight bit 5 contributes 2^4@@    wp2 = (w0_q[4] ? (pd_ext <<< 4) : ZERO_B) + (w0_q[5] ? (pd_ext <<< 5) : ZERO_B);@@    wp2 = (w0_q[4] ? (pd_ext <<< 4) : ZERO_B) + (w0_q[5] ? (pd_ext <<< 4) : ZERO_B);"
"M11 the weight is truncated to five bits@@        w0_q <= v_w0_i[5:0];@@        w0_q <= {1'b0, v_w0_i[4:0]};"
"M12 the translation is not pre-shifted into the accumulator@@          acc[rp]     <= ACCW'(a_m_i[rp*4 + 3]) <<< 16;@@          acc[rp]     <= ACCW'(a_m_i[rp*4 + 3]);"
"M13 B's accumulator is seeded from A's translation@@          acc[rp + 3] <= ACCW'(b_m_i[rp*4 + 3]) <<< 16;@@          acc[rp + 3] <= ACCW'(a_m_i[rp*4 + 3]) <<< 16;"
"M14 the B palette is never latched@@          m_q[12 + i] <= b_m_i[i];@@          m_q[12 + i] <= a_m_i[i];"
"M15 the y and z lane operands are swapped@@      mul_b[l] = (trm == 0) ? vx_q : ((trm == 1) ? vy_q : vz_q);@@      mul_b[l] = (trm == 0) ? vx_q : ((trm == 1) ? vz_q : vy_q);"
"M16 the matrix row stride is three, not four@@      idx = 5'(rp * 4 + trm);@@      idx = 5'(rp * 3 + trm);"
"M17 the accumulator is LOADED rather than accumulated@@          acc[dst_d2[r]] <= acc[dst_d2[r]] + lane_sum[r];@@          acc[dst_d2[r]] <= lane_sum[r];"
"M18 a busy engine still offers ready@@  assign v_ready_o = !busy && (!o_valid_o || o_ready_i);@@  assign v_ready_o = (!o_valid_o || o_ready_i);"
"M19 a full output register still offers ready@@  assign v_ready_o = !busy && (!o_valid_o || o_ready_i);@@  assign v_ready_o = !busy;"
"M20 the src_id passthrough is off by one@@          o_src_id_o <= src_q;@@          o_src_id_o <= src_q + 16'd1;"
"M21 the counter loses its saturation (EXPECTED EQUIVALENT)@@        if (vertices_transformed_o != 32'hFFFF_FFFF)@@        if (1'b1)"
"M22 the counter advances by two@@          vertices_transformed_o <= vertices_transformed_o + 32'd1;@@          vertices_transformed_o <= vertices_transformed_o + 32'd2;"
"M23 the blend row waits only on pa@@  assign row_ready = busy && (br != 2'd3) && acc_done[br_a] && (rigid_q || acc_done[br_b]);@@  assign row_ready = busy && (br != 2'd3) && acc_done[br_a];"
"M24 every row is blended against B's row 0@@  assign br_b = br_a + 3'd3;@@  assign br_b = 3'd3;"
"M25 the issue walk stops one group early@@          if (rp_grp == n_groups - 3'd1) issuing <= 1'b0;@@          if (rp_grp == n_groups - 3'd2) issuing <= 1'b0;"
"M26 a landed product goes to the neighbouring accumulator@@        dst_d2[r]   <= dst_d1[r];@@        dst_d2[r]   <= dst_d1[r] ^ 3'd1;"
"M27 acc_done is set by the FIRST term, not the last (EQUIVALENT at MUL_LANES 3 and 6)@@          if (dlast_d2[r]) acc_done[dst_d2[r]] <= 1'b1;@@          if (dlast_d2[r] || dv_d2[r]) acc_done[dst_d2[r]] <= 1'b1;"
"M28 rigid still issues six row-products (EQUIVALENT at MUL_LANES 1 and 3)@@  assign n_rp     = rigid_q ? 3'd3 : 3'd6;@@  assign n_rp     = 3'd6;"
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

  rebuild "$ALL"

  if ! models_present "$ALL"; then
    echo "  $name  DISCARDED: a model was absent after regeneration"
    restore || { echo "ABORT: revert failed"; exit 4; }
    continue
  fi
  if ! exes_present "$ALL"; then
    echo "  $name  DISCARDED: a target did not LINK (a build failure would"
    echo "                    otherwise be scored as a caught mutant)"
    restore || { echo "ABORT: revert failed"; exit 4; }
    continue
  fi
  mutant_model=$(model_hash "$ALL")
  if [ "$mutant_model" = "$PRISTINE_MODEL" ]; then
    echo "  $name  DISCARDED: models identical to pristine (did not re-elaborate)"
    restore || { echo "ABORT: revert failed"; exit 4; }
    continue
  fi

  if run_lanes "$ALL"; then
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
rebuild "$ALL"

echo "----"
echo "attempted=$attempted expected=$expected accounted=$accounted caught=$caught"
for s in "${survivors[@]:-}"; do [ -n "$s" ] && echo "SURVIVOR: $s"; done
if [ "$attempted" != "$expected" ] || [ "$accounted" != "$expected" ]; then
  echo "CROSS-CHECK FAILED (attempted/accounted must both equal $expected)"
  exit 5
fi
