#!/usr/bin/env bash
# sweep_surface_sheet.sh — mutation sweep for SURFACE.SHEET.
#
# ---------------------------------------------------------------------------
# WHY THIS FILE DID NOT EXIST UNTIL NOW, which is itself the finding.
# SURFACE.STAMP, TEXTURE.TMU, GEOM.SKIN, GEOM.LOD, GEOM.CULL, TERRAIN.LOD and
# FIELD all have sweeps. The block holding the repository's largest single
# resource item had none. RUN-20260824-0317 rearchitected its storage shape, and
# a storage-shape change with no sweep is precisely the change whose damage a
# passing suite hides.
#
# GUARDS 1-7 ARE CARRIED UNCHANGED from tools/sweep_texture_tmu.sh, which
# carried them from tools/sweep_surface_stamp.sh, which carried them from
# tools/sweep_geom_skin.sh. Each was written after a real instance of scoring a
# test that never ran:
#
#   1. `verilate()` elaborates at CONFIGURE time, so `ninja` alone relinks a
#      CACHED model against changed source. `cmake -S . -B build` runs every
#      iteration.
#   2. mtime is set to NOW, never forward — a mutant stamped into the future
#      outranks the pristine source restored after it.
#   3. The whole target directory is deleted each iteration: a pristine model
#      can otherwise be linked against an OBJECT still compiled from a mutant.
#   4. Hash the whole model DIRECTORY. Vzhao_surface_sheet.cpp is Verilator's
#      wrapper and is byte-identical between a pristine and a mutated build.
#   5. THE EXECUTABLE LIVES OUTSIDE THE TARGET DIRECTORY, so a mutant that
#      fails to COMPILE leaves the previous binary in place and the sweep runs
#      that instead — a build failure scoring as a caught mutant. Delete the
#      exe too and require it after the build.
#   6. The PRISTINE build runs first and must pass, or nothing below means
#      anything.
#   7. EVERY CONSUMER of the mutated file is cleaned, rebuilt and scored.
#
# GUARD 7 MATTERS MORE HERE THAN ANYWHERE IT HAS BEEN USED SO FAR. This one file
# has SIX consumers and only three of them are named for it — `test_field_write_tag`,
# `test_surface_stamp_chain` and `test_texture_aux_directed` instantiate the
# sheet as a component of somebody else's system. A sweep that scored only the
# three `test_surface_sheet_*` lanes would call mutants caught that those three
# lanes never execute, and would miss the case this block exists to prevent:
# one patch's scars appearing under another patch's stamp.
#
# THE MUTANT TABLE IS IN TWO HALVES, and the split is deliberate.
#
#   S01-S08 attack THE NEW SHAPE — two 8-bit planes where there was one 16-bit
#   array with byte enables. These are the defects the rearchitecture made
#   possible and that no earlier sweep could have contained: a write landing in
#   the wrong plane, one plane's enable driving the other, a clear sweep that
#   clears only half, read-during-write answering post-write where the contract
#   says pre-write, and the read pipeline off by a cycle.
#
#   S09-S18 attack BEHAVIOUR THAT MUST NOT HAVE MOVED — persistence, overflow
#   never evicting, a non-resident write landing nowhere, the sweep's length and
#   the backpressure it asserts. A rearchitecture that leaves these uncaught has
#   broken the suite, not just the block, and they are re-run for that reason.
#
# ONE EXPECTED SURVIVOR, COSTED RATHER THAN CALLED AN EQUIVALENT.
#
# S18 deletes the saturation guard on `surface_texels_touched_o`. It is NOT an
# equivalent -- `spec/counters.md` 4 says a counter never wraps, and the two
# behaviours differ, at exactly one value. They differ only after 2^32 =
# 4,294,967,296 ACCEPTED WRITES, and the write port takes at most one per clock,
# so no shorter stimulus can reach it. A Verilated model of this block runs at
# about 2.4 M cycles/s on this machine (measured: 228,144 cycle-pairs in 188 ms
# across two models), which puts the cheapest possible killing test at roughly
# HALF AN HOUR of pure simulation -- a nightly lane at best, never `ctest -L fast`.
#
# It is left alive deliberately and named here so that a future reader does not
# mistake a 17/18 for a hole. `tools/sweep_geom_skin.sh` carries the identical
# mutant on the identical 32-bit saturating counter (its M21) and labels it
# "EXPECTED EQUIVALENT", which is the one word this note avoids: it is not
# equivalent, it is unreachable, and those are different claims.
#
# THE SCORING RULE: after regeneration every model must EXIST, every exe must
# EXIST, and the hash of the whole set must DIFFER from the pristine set's.
# Anything else is discarded, never scored.
# ---------------------------------------------------------------------------
set -u

SHEET_RTL=fpga/rtl/surface/zhao_surface_sheet.sv
ALL_RTL="$SHEET_RTL"

BUILD=${ZHAO_BUILD_DIR:-build}

# Guard 7's human-readable half: what tests/CMakeLists.txt is expected to reach.
# If a target is added or removed, this list and the derived one disagree and
# the sweep ABORTS rather than quietly scoring a smaller set.
DECLARED_SHEET="test_field_write_tag test_surface_sheet_directed test_surface_sheet_random test_surface_sheet_store_diff test_surface_stamp_chain test_texture_aux_directed"

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
CONS_SHEET=$(python tools/sweep_surface_stamp_consumers.py "$SHEET_RTL" | tr -d '\r') || exit 9

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
check_set "zhao_surface_sheet.sv" "$CONS_SHEET" "$DECLARED_SHEET"

UNION="$CONS_SHEET"

# PREFLIGHT: EVERY MUTANT MUST LINT BEFORE ANY OF THEM IS SCORED.
python tools/sweep_surface_sheet_preflight.py || {
  echo "ABORT: at least one mutant does not build -- fix the mutation, not the guard"
  exit 8
}

echo "== establishing the pristine baseline"
restore_all || { echo "ABORT: cannot establish pristine source"; exit 4; }
rebuild "$UNION"
models_present "$UNION" || { echo "ABORT: a pristine model did not elaborate"; exit 6; }
exes_present "$UNION"   || { echo "ABORT: a pristine target did not link"; exit 6; }
declare -A PRISTINE
PRISTINE[sheet]=$(model_hash "$CONS_SHEET")
if ! run_lanes "$UNION"; then
  echo "ABORT: the PRISTINE build fails its own tests -- nothing below would mean anything"
  exit 7
fi
echo "   pristine models ${PRISTINE[sheet]:0:12}, $(echo $UNION | wc -w) lanes green"

# Each entry: name @@ file @@ old @@ new
MUTS=(
# ---- S01-S08 : THE NEW SHAPE ----------------------------------------------
# Two 8-bit planes replaced one 16-bit array written with byte enables
# (RUN-20260824-0317; the byte enables were what stopped M10K inference and cost
# 229 % of the device). Everything below is a defect that shape makes possible.
# S01 TRANSPOSES the two planes rather than redirecting one write into the
# other. Redirecting leaves `mem_tag` written by nothing, which trips
# -Wall UNDRIVEN and the preflight rejects it -- correctly: a mutant that
# orphans a signal is a build failure wearing a defect's name, and this
# repository has three runs' worth of those scoring as CAUGHT. The mutation was
# fixed, not the guard.
"S01 a write lands in the WRONG ARRAY: the two planes are transposed@@fpga/rtl/surface/zhao_surface_sheet.sv@@    if (mem_we_tag) mem_tag[wr_addr] <= wr_tag;
    if (mem_we_str) mem_str[wr_addr] <= wr_str;@@    if (mem_we_tag) mem_str[wr_addr] <= wr_tag;
    if (mem_we_str) mem_tag[wr_addr] <= wr_str;"
"S02 the TAG enable also writes the strength plane (a tag-only blend erases strength)@@fpga/rtl/surface/zhao_surface_sheet.sv@@  wire mem_we_str = mem_we && (clr_active || wr_we_strength_i);@@  wire mem_we_str = mem_we && (clr_active || wr_we_strength_i || wr_we_tag_i);"
"S03 the STRENGTH enable also writes the tag plane@@fpga/rtl/surface/zhao_surface_sheet.sv@@  wire mem_we_tag = mem_we && (clr_active || wr_we_tag_i);@@  wire mem_we_tag = mem_we && (clr_active || wr_we_tag_i || wr_we_strength_i);"
"S04 the clear sweep clears only the TAG plane -- stale strength survives an invalidated address@@fpga/rtl/surface/zhao_surface_sheet.sv@@  wire mem_we_str = mem_we && (clr_active || wr_we_strength_i);@@  wire mem_we_str = mem_we && wr_we_strength_i;"
"S05 read-during-write returns the POST-write word where C5 says pre-write@@fpga/rtl/surface/zhao_surface_sheet.sv@@    if (do_read_hit) begin
      ram_tag_q <= mem_tag[rd_addr];
      ram_str_q <= mem_str[rd_addr];
    end@@    if (do_read_hit) begin
      ram_tag_q <= (mem_we_tag && wr_addr == rd_addr) ? wr_tag : mem_tag[rd_addr];
      ram_str_q <= (mem_we_str && wr_addr == rd_addr) ? wr_str : mem_str[rd_addr];
    end"
"S06 the read pipeline is off by ONE CYCLE (the array is read a cycle after the accept)@@fpga/rtl/surface/zhao_surface_sheet.sv@@    if (do_read_hit) begin
      ram_tag_q <= mem_tag[rd_addr];@@    if (pg_is_read_q) begin
      ram_tag_q <= mem_tag[rd_addr];"
"S07 the read enable is dropped, so a stalled response loses its word@@fpga/rtl/surface/zhao_surface_sheet.sv@@    if (do_read_hit) begin
      ram_tag_q <= mem_tag[rd_addr];
      ram_str_q <= mem_str[rd_addr];
    end@@    begin
      ram_tag_q <= mem_tag[rd_addr];
      ram_str_q <= mem_str[rd_addr];
    end"
"S08 the READ names the write port's slot -- one patch reads another's scars@@fpga/rtl/surface/zhao_surface_sheet.sv@@  wire [AddrBits-1:0] rd_addr = {req_hit_slot, req_texel_i};@@  wire [AddrBits-1:0] rd_addr = {wr_hit_slot, req_texel_i};"
# ---- S09-S18 : BEHAVIOUR THAT MUST NOT HAVE MOVED --------------------------
# The three laws the differential's own header calls the ones an independent
# implementation would get wrong, plus the sweep, the counter and the
# backpressure. Re-run because a rearchitecture that leaves these uncaught has
# broken the SUITE, not just the block.
"S09 re-acquiring a resident handle CLEARS it -- persistence lost@@fpga/rtl/surface/zhao_surface_sheet.sv@@        if (do_acquire_hit) begin
          // C3: a resident handle is NOT cleared. This line is persistence.
          pg_status_q <= StHit;@@        if (do_acquire_hit) begin
          clr_active <= 1'b1;
          clr_slot <= req_hit_slot;
          clr_addr <= 12'd0;
          pg_status_q <= StHit;"
"S10 OVERFLOW EVICTS: a full directory steals slot 0 instead of refusing@@fpga/rtl/surface/zhao_surface_sheet.sv@@          pg_status_q <= StOverflow;
          res_overflow_o <= 1'b1;@@          dir_handle[0] <= req_handle_i;
          pg_status_q <= StOverflow;
          res_overflow_o <= 1'b1;"
"S11 a write naming a NON-RESIDENT handle lands in slot 0 instead of nowhere@@fpga/rtl/surface/zhao_surface_sheet.sv@@  wire do_write = wr_fire && wr_hit;@@  wire do_write = wr_fire;"
"S12 the counter counts DROPPED writes, so it stops agreeing with the sheet@@fpga/rtl/surface/zhao_surface_sheet.sv@@      if (do_write) begin@@      if (wr_fire) begin"
"S13 the clear sweep is one texel short -- the last texel of a fresh sheet is not zero@@fpga/rtl/surface/zhao_surface_sheet.sv@@        if (clr_addr == 12'((Texels - 1))) clr_active <= 1'b0;@@        if (clr_addr == 12'((Texels - 2))) clr_active <= 1'b0;"
# S14 IS A TRUE EQUIVALENT, and it is kept because proving that was worth doing.
# `!clr_active` is REDUNDANT in `req_ready_o`: `clr_active` and `pend_valid` are
# set together in the same `do_acquire_new` branch, and `pend_valid` can only
# clear under `!clr_active`, so `clr_active` implies `pend_valid` and
# `!clr_active && !pend_valid` is just `!pend_valid`. Argued from those two
# lines and then CONFIRMED: 0 mismatches in 228,144 compared port-cycles against
# the shipped behaviour (this run's shape differential, probe S14).
"S14 the request port is NOT refused during the sweep (TRUE EQUIVALENT, confirmed)@@fpga/rtl/surface/zhao_surface_sheet.sv@@  assign req_ready_o   = !clr_active && !pend_valid && pg_slot_free;@@  assign req_ready_o   = !pend_valid && pg_slot_free;"
"S15 a MISSED read returns the array contents instead of zero@@fpga/rtl/surface/zhao_surface_sheet.sv@@  assign pg_tag_o      = (pg_is_read_q && pg_status_q == StHit) ? ram_tag_q : 8'd0;@@  assign pg_tag_o      = pg_is_read_q ? ram_tag_q : 8'd0;"
"S16 RELEASE does not free the slot@@fpga/rtl/surface/zhao_surface_sheet.sv@@          dir_live[req_hit_slot] <= 1'b0;@@          dir_live[req_hit_slot] <= 1'b1;"
"S17 the WRITE port is accepted during the sweep@@fpga/rtl/surface/zhao_surface_sheet.sv@@  assign wr_ready_o    = !clr_active;@@  assign wr_ready_o    = 1'b1;"
"S18 the touched counter is not saturating (EXPECTED SURVIVOR -- see the note above the table)@@fpga/rtl/surface/zhao_surface_sheet.sv@@        if (surface_texels_touched_o != 32'hFFFF_FFFF)
          surface_texels_touched_o <= surface_texels_touched_o + 32'd1;@@        surface_texels_touched_o <= surface_texels_touched_o + 32'd1;"
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
    "$SHEET_RTL") cons=$CONS_SHEET; key=sheet ;;
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
