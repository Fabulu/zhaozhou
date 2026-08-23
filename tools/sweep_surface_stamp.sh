#!/usr/bin/env bash
# sweep_surface_stamp.sh — mutation sweep for SURFACE.STAMP's three RTL files.
#
# ---------------------------------------------------------------------------
# THE SWEEP VERIFIES ITS OWN BUILDS. Guards 1-7 are carried unchanged from
# tools/sweep_geom_skin.sh, where each was written after a real instance of
# scoring a test that never ran:
#
#   1. `verilate()` elaborates at CONFIGURE time, so `ninja` alone relinks a
#      CACHED model against changed source. `cmake -S . -B build` runs every
#      iteration.
#   2. mtime is set to NOW, never forward -- a mutant stamped into the future
#      outranks the pristine source restored after it.
#   3. The whole target directory is deleted each iteration: a pristine model
#      can otherwise be linked against an OBJECT still compiled from a mutant.
#   4. Hash the whole model DIRECTORY. Vzhao_surface_stamp.cpp is Verilator's
#      wrapper and is byte-identical between a pristine and a mutated build.
#   5. THE EXECUTABLE LIVES OUTSIDE THE TARGET DIRECTORY, so a mutant that
#      fails to COMPILE leaves the previous binary in place and the sweep runs
#      that instead -- a build failure scoring as a caught mutant. Delete the
#      exe too and require it after the build.
#   6. The PRISTINE build runs first and must pass, or nothing below means
#      anything.
#   7. EVERY CONSUMER of the mutated file is cleaned, rebuilt and scored.
#
# TWO THINGS ARE DIFFERENT HERE.
#
# (a) THREE FILES, NOT ONE. SURFACE.STAMP is `zhao_surface_stamp.sv` plus the
#     shared sequential squarer `zhao_surface_sq.sv` plus `zhao_surface_blend.sv`,
#     and their consumer sets differ -- blend is also elaborated on its own by
#     test_field_stamp_modes. Each mutant names its own file and is scored
#     against THAT file's consumers.
#
# (b) GUARD 7 CANNOT BE A REGEX HERE. The surface targets say
#     `SOURCES ${ZHAO_SURFACE_STAMP_SV}`, so sweep_geom_skin.sh's
#     `verilate\((\w+)(.*?)\.sv\)` finds NO consumer for these files. Worse than
#     finding none: the two frontier targets DO spell their sources out, so a
#     regex sweep would have scored two of five builds and printed a number.
#     tools/sweep_surface_stamp_consumers.py resolves `set(VAR ...)` and
#     expands `${VAR}` instead.
#
# THE FRONTIER BUILDS ARE COVERAGE, NOT DATA. `zhao_surface_sq` retires
# SQ_RADIX bits per cycle through a chain of SQ_RADIX conditional adds. At the
# DEFAULT SQ_RADIX = 1 that chain has exactly one link, so the b > 0 arms of the
# generate are never elaborated, and `sh_q << SQ_RADIX` is indistinguishable
# from `sh_q << 1`. Mutants S03 and S04 are EXACTLY that, and they are alive at
# the default and dead at radix 2 and 4. Scoring only the default build would
# have called them survivors and sent someone hunting a test gap that is not
# there. (sweep_geom_skin.sh hit the identical shape on MUL_LANES and M27.)
#
# The scoring rule: after regeneration every model must EXIST, every exe must
# EXIST, and the hash of the whole set must DIFFER from the pristine set's.
# Anything else is discarded, never scored.
# ---------------------------------------------------------------------------
set -u

STAMP_RTL=fpga/rtl/surface/zhao_surface_stamp.sv
SQ_RTL=fpga/rtl/surface/zhao_surface_sq.sv
BLEND_RTL=fpga/rtl/surface/zhao_surface_blend.sv
ALL_RTL="$STAMP_RTL $SQ_RTL $BLEND_RTL"

BUILD=${ZHAO_BUILD_DIR:-build}

# Guard 7's human-readable half, per file. The derived sets must equal these.
DECLARED_STAMP="test_surface_stamp_chain test_surface_stamp_directed test_surface_stamp_radix2 test_surface_stamp_radix4 test_surface_stamp_random"
# zhao_surface_sq has TWO consumers the stamp files do not: its own directed
# suite, added 2026-08-23 because three sweep mutants proved the coverage test
# is scale-invariant and therefore blind to a uniformly scaled squarer.
DECLARED_SQ="test_surface_sq_directed test_surface_sq_radix4 $DECLARED_STAMP"
DECLARED_BLEND="test_field_stamp_modes $DECLARED_STAMP"

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
CONS_STAMP=$(python tools/sweep_surface_stamp_consumers.py "$STAMP_RTL" | tr -d '\r') || exit 9
CONS_SQ=$(python tools/sweep_surface_stamp_consumers.py "$SQ_RTL" | tr -d '\r') || exit 9
CONS_BLEND=$(python tools/sweep_surface_stamp_consumers.py "$BLEND_RTL" | tr -d '\r') || exit 9

check_set() {  # $1 = label, $2 = derived, $3 = declared
  local t
  printf '   %-28s %s\n' "$1" "$2"
  for t in $2; do
    case " $3 " in *" $t "*) ;; *) echo "ABORT: derived '$t' not in DECLARED for $1"; exit 9 ;; esac
  done
  for t in $3; do
    case " $2 " in *" $t "*) ;; *) echo "ABORT: DECLARED names '$t' for $1, which no verilate() reaches"; exit 9 ;; esac
  done
}
check_set "zhao_surface_stamp.sv" "$CONS_STAMP" "$DECLARED_STAMP"
check_set "zhao_surface_sq.sv"    "$CONS_SQ"    "$DECLARED_SQ"
check_set "zhao_surface_blend.sv" "$CONS_BLEND" "$DECLARED_BLEND"

UNION=$(printf '%s\n' $CONS_STAMP $CONS_SQ $CONS_BLEND | sort -u | tr '\n' ' ')

# PREFLIGHT: EVERY MUTANT MUST LINT BEFORE ANY OF THEM IS SCORED.
python tools/sweep_surface_stamp_preflight.py || {
  echo "ABORT: at least one mutant does not build -- fix the mutation, not the guard"
  exit 8
}

echo "== establishing the pristine baseline"
restore_all || { echo "ABORT: cannot establish pristine source"; exit 4; }
rebuild "$UNION"
models_present "$UNION" || { echo "ABORT: a pristine model did not elaborate"; exit 6; }
exes_present "$UNION"   || { echo "ABORT: a pristine target did not link"; exit 6; }
declare -A PRISTINE
PRISTINE[stamp]=$(model_hash "$CONS_STAMP")
PRISTINE[sq]=$(model_hash "$CONS_SQ")
PRISTINE[blend]=$(model_hash "$CONS_BLEND")
if ! run_lanes "$UNION"; then
  echo "ABORT: the PRISTINE build fails its own tests -- nothing below would mean anything"
  exit 7
fi
echo "   pristine models ${PRISTINE[stamp]:0:12}/${PRISTINE[sq]:0:12}/${PRISTINE[blend]:0:12}, $(echo $UNION | wc -w) lanes green"

# Each entry: name @@ file @@ old @@ new
MUTS=(
# ---- zhao_surface_sq.sv : the shared sequential squarer ---------------------
"S01 the magnitude negation drops its +1@@fpga/rtl/surface/zhao_surface_sq.sv@@  wire [MAG_W-1:0] mag = a_i[MAG_W-1] ? (~a_u + {{(MAG_W - 1) {1'b0}}, 1'b1}) : a_u;@@  wire [MAG_W-1:0] mag = a_i[MAG_W-1] ? (~a_u) : a_u;"
"S02 the sign is read from the wrong bit@@fpga/rtl/surface/zhao_surface_sq.sv@@  wire [MAG_W-1:0] mag = a_i[MAG_W-1] ? (~a_u + {{(MAG_W - 1) {1'b0}}, 1'b1}) : a_u;@@  wire [MAG_W-1:0] mag = a_i[MAG_W-2] ? (~a_u + {{(MAG_W - 1) {1'b0}}, 1'b1}) : a_u;"
"S03 the addend shifts by one regardless of radix (EQUIVALENT at SQ_RADIX 1)@@fpga/rtl/surface/zhao_surface_sq.sv@@      sh_q   <= sh_q << SQ_RADIX;@@      sh_q   <= sh_q << 1;"
"S04 the multiplier bits shift by one regardless of radix (EQUIVALENT at SQ_RADIX 1)@@fpga/rtl/surface/zhao_surface_sq.sv@@      bits_q <= bits_q >> SQ_RADIX;@@      bits_q <= bits_q >> 1;"
"S05 each chain link's place value is one too high@@fpga/rtl/surface/zhao_surface_sq.sv@@      assign chain[b+1] = chain[b] + (bits_q[b] ? (sh_q << b) : {ACC_W{1'b0}});@@      assign chain[b+1] = chain[b] + (bits_q[b] ? (sh_q << (b + 1)) : {ACC_W{1'b0}});"
"S06 each chain link tests the neighbouring multiplier bit@@fpga/rtl/surface/zhao_surface_sq.sv@@      assign chain[b+1] = chain[b] + (bits_q[b] ? (sh_q << b) : {ACC_W{1'b0}});@@      assign chain[b+1] = chain[b] + (bits_q[b+1] ? (sh_q << b) : {ACC_W{1'b0}});"
"S07 the square runs one step short, dropping the TOP magnitude bit@@fpga/rtl/surface/zhao_surface_sq.sv@@      cnt_q  <= 8'(Steps);@@      cnt_q  <= 8'(Steps) - 8'd1;"
"S08 the LOW magnitude bit is dropped@@fpga/rtl/surface/zhao_surface_sq.sv@@      bits_q <= mag;@@      bits_q <= {1'b0, mag[MAG_W-1:1]};"
"S09 the accumulator is not cleared, so squares accumulate across calls@@fpga/rtl/surface/zhao_surface_sq.sv@@      acc_q  <= {ACC_W{1'b0}};
      cnt_q  <= 8'(Steps);@@      acc_q  <= acc_q;
      cnt_q  <= 8'(Steps);"
"S10 vld_o is asserted one step early@@fpga/rtl/surface/zhao_surface_sq.sv@@      if (cnt_q == 8'd1) vld_o <= 1'b1;@@      if (cnt_q == 8'd2) vld_o <= 1'b1;"
"S11 a fresh start does not clear the previous result's valid@@fpga/rtl/surface/zhao_surface_sq.sv@@      cnt_q  <= 8'(Steps);
      vld_o  <= 1'b0;@@      cnt_q  <= 8'(Steps);
      vld_o  <= vld_o;"
"S12 the addend is loaded pre-doubled@@fpga/rtl/surface/zhao_surface_sq.sv@@      sh_q   <= {{(ACC_W - MAG_W) {1'b0}}, mag};@@      sh_q   <= {{(ACC_W - MAG_W - 1) {1'b0}}, mag, 1'b0};"
# ---- zhao_surface_stamp.sv : the accumulators and the sequencing ------------
"T01 the texel-centre accumulator steps by span, not 2*span@@fpga/rtl/surface/zhao_surface_stamp.sv@@              numx_q <= numx_q + spanx2;@@              numx_q <= numx_q + (spanx2 >>> 1);"
"T02 the row accumulator steps by span, not 2*span@@fpga/rtl/surface/zhao_surface_stamp.sv@@              numz_q <= numz_q + spanz2;@@              numz_q <= numz_q + (spanz2 >>> 1);"
"T03 the row reset seeds numx at zero, making (2i+1) into 2i@@fpga/rtl/surface/zhao_surface_stamp.sv@@              numx_q <= 41'(st_spanx);@@              numx_q <= 41'sd0;"
"T04 the row wraps one texel early@@fpga/rtl/surface/zhao_surface_stamp.sv@@            if (cur_i == 6'd63) begin@@            if (cur_i == 6'd62) begin"
"T05 the z seed is taken from the x span@@fpga/rtl/surface/zhao_surface_stamp.sv@@            numz_q <= 41'(span_z_c);@@            numz_q <= 41'(span_x_c);"
"T06 the coverage sum becomes a difference@@fpga/rtl/surface/zhao_surface_stamp.sv@@  wire signed [63:0] d2 = \$signed(sq_o) + \$signed(dz2_q);@@  wire signed [63:0] d2 = \$signed(sq_o) - \$signed(dz2_q);"
"T07 the geometry answer is taken without waiting for the squarer@@fpga/rtl/surface/zhao_surface_stamp.sv@@  wire geom_ready = (state == SRun) && (gstate == GWaitX) && sq_vld;@@  wire geom_ready = (state == SRun) && (gstate == GWaitX);"
"T08 the squarer's dx and dz operands are swapped@@fpga/rtl/surface/zhao_surface_stamp.sv@@                (gstate == GStartZ) ? dz : dx;@@                (gstate == GStartZ) ? dx : dz;"
"T09 the outer radius square lands in the inner register@@fpga/rtl/surface/zhao_surface_stamp.sv@@          st_r_outer2 <= \$signed(sq_o);@@          st_r_inner2 <= \$signed(sq_o);"
"T10 the two radius operands are swapped, so the annulus inverts@@fpga/rtl/surface/zhao_surface_stamp.sv@@  assign sq_a = (state == SSqRo)   ? 36'(st_radius) :
                (state == SSqRi)   ? 36'(st_rinner) :@@  assign sq_a = (state == SSqRo)   ? 36'(st_rinner) :
                (state == SSqRi)   ? 36'(st_radius) :"
"T11 dz^2 is dropped, collapsing the disc to a vertical band@@fpga/rtl/surface/zhao_surface_stamp.sv@@              dz2_q  <= sq_o;@@              dz2_q  <= 64'd0;"
"T12 the write texel index is transposed@@fpga/rtl/surface/zhao_surface_stamp.sv@@  assign wr_texel_o = s2_texel;@@  assign wr_texel_o = {s2_texel[5:0], s2_texel[11:6]};"
"T13 stamp_results transposes before and after, losing the delta TERRAIN.BAKE needs@@fpga/rtl/surface/zhao_surface_stamp.sv@@  assign res_strength_o = s2_after;
  assign res_before_o = s2_before;@@  assign res_strength_o = s2_before;
  assign res_before_o = s2_after;"
"T14 the outer rim is made exclusive, clipping the edge texel@@fpga/rtl/surface/zhao_surface_stamp.sv@@  wire covered = !((d2 > st_r_outer2) || (d2 < st_r_inner2));@@  wire covered = !((d2 >= st_r_outer2) || (d2 < st_r_inner2));"
"T15 the cursor advances without the geometry@@fpga/rtl/surface/zhao_surface_stamp.sv@@  wire advance = cursor_slot && geom_ready && fld_ok && read_path_ok;@@  wire advance = cursor_slot && fld_ok && read_path_ok;"
"T16 the sheet read fires without the geometry@@fpga/rtl/surface/zhao_surface_stamp.sv@@      (cursor_slot && geom_ready && fld_ok && covered && s1_free_next);@@      (cursor_slot && fld_ok && covered && s1_free_next);"
# ---- zhao_surface_blend.sv : the behaviour that must NOT have moved ---------
"B01 decay-accumulate REPLACES instead of accumulating@@fpga/rtl/surface/zhao_surface_blend.sv@@        acc   = {2'b0, dst_i[7:1]} + {1'b0, src_i};@@        acc   = {1'b0, src_i};"
"B02 ADD's saturation is removed@@fpga/rtl/surface/zhao_surface_blend.sv@@        acc   = {1'b0, dst_i} + {1'b0, src_i};
        out_o = acc[8] ? 8'hFF : acc[7:0];@@        acc   = {1'b0, dst_i} + {1'b0, src_i};
        out_o = acc[7:0];"
"B03 SUB's saturation is removed@@fpga/rtl/surface/zhao_surface_blend.sv@@        acc   = {1'b0, dst_i} - {1'b0, src_i};
        out_o = acc[8] ? 8'h00 : acc[7:0];  // acc[8] is the borrow@@        acc   = {1'b0, dst_i} - {1'b0, src_i};
        out_o = acc[7:0];"
"B04 decay-accumulate's saturation is removed (the RATIFIED path)@@fpga/rtl/surface/zhao_surface_blend.sv@@        acc   = {2'b0, dst_i[7:1]} + {1'b0, src_i};
        out_o = acc[8] ? 8'hFF : acc[7:0];@@        acc   = {2'b0, dst_i[7:1]} + {1'b0, src_i};
        out_o = acc[7:0];"
)

# ---------------------------------------------------------------------------
# ZHAO_SWEEP_ONLY — re-score a NAMED SUBSET, for confirming a fix.
#
# Added 2026-08-23 after this sweep's first run left five survivors. Two of them
# were a real test gap (see the contract's mutation table); the fix is a new
# directed case, and the honest way to show a fix works is to re-run the
# mutants it was written for. Re-running all 32 to check two is 100 minutes of
# machine time to learn nothing new about the other 30.
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
    "$STAMP_RTL") cons=$CONS_STAMP; key=stamp ;;
    "$SQ_RTL")    cons=$CONS_SQ;    key=sq    ;;
    "$BLEND_RTL") cons=$CONS_BLEND; key=blend ;;
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
