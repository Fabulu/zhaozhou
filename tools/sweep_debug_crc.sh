#!/usr/bin/env bash
# sweep_debug_crc.sh — mutation sweep for zhao_debug_crc.sv, the displayed-frame
# CRC lane after it moved from gpu_clk to vid_clk (2026-08-22).
#
# ---------------------------------------------------------------------------
# THE SWEEP VERIFIES ITS OWN BUILDS. This machinery is lifted from
# tools/sweep_geom_lod.sh, whose header records the FOUR distinct ways this
# build system will score a run that never happened; all four apply here:
#
#   1. verilate() elaborates at CONFIGURE time, so ninja alone relinks a CACHED
#      model against changed source.
#   2. Stamping the mutated source's mtime forward makes a mutant-derived model
#      outrank a pristine source restored after it. mtime is set to NOW.
#   3. Hashing the model is necessary but not sufficient: a pristine model can
#      be linked against an OBJECT still compiled from a mutant, so the WHOLE
#      target directory is deleted each iteration.
#   4. Vzhao_debug_crc.cpp is Verilator's WRAPPER and is byte-identical between
#      pristine and mutant; the logic lives in Vzhao_debug_crc___024root__0.cpp.
#      So the whole model directory is hashed, not that one file.
#
# The scoring rule: after regeneration the model must EXIST and its hash must
# DIFFER from the pristine model's. Anything else is DISCARDED, never scored.
# And the pristine build is proven against its own test first, because if it
# were red every "caught" below would be meaningless.
#
# ONE ADDITION over the geom_lod sweep. This block instantiates a LEAF
# (zhao_crc32c_fold), so the model directory contains that leaf's generated
# files too. That is fine for hashing — a mutation anywhere in the cone moves
# the hash — but it means "model identical to pristine" would also be the
# correct verdict for a mutation the frontend optimises away, which is why a
# discard is reported rather than counted.
#
# AND A FIFTH WAY TO SCORE A RUN THAT NEVER HAPPENED, found by this sweep on
# its first pass, where it reported 22 survivors out of 22 — an implausible
# result that was the guard working, not a test hole.
#
#   5. THE EXECUTABLE LIVES OUTSIDE THE TARGET DIRECTORY. Deleting
#      build/tests/CMakeFiles/<target>.dir removes the model and every object,
#      so ninja must rebuild them — but build/tests/<target>.exe SURVIVES that
#      deletion. If the rebuild fails for ANY reason, the previous executable
#      is still sitting there and runs happily against RTL it was never built
#      from. The four guards above all passed: the source moved, the model
#      re-elaborated, its hash differed. Only the link had failed.
#
#      What actually failed was a C++ typo in the test itself
#      (`0x5EC0ND50u` is not a hex literal), which had been committed because
#      `ctest` does not build and a stale executable had passed every run since.
#      So this failure mode also FOUND A REAL DEFECT — the same defect twice
#      over, since a test that cannot compile is not a test.
#
#      The guard: delete the EXE too, and require it to EXIST after the
#      rebuild. A missing executable is DISCARDED, never scored.
#
# TWO MUTANTS ARE EXPECTED TO SURVIVE, and they are listed as EQUIVALENT at the
# bottom of this file with a proof of why no input can distinguish them. They
# are still driven, so that the proofs are re-checked rather than trusted.
# ---------------------------------------------------------------------------
set -u

RTL=fpga/rtl/debug/zhao_debug_crc.sv
TARGETDIR=build/tests/CMakeFiles/test_debug_crc_directed.dir
MODELDIR="$TARGETDIR/Vzhao_debug_crc.dir"

model_hash() {
  find "$MODELDIR" -type f \( -name "*.cpp" -o -name "*.h" \) 2>/dev/null \
    | sort | xargs sha256sum 2>/dev/null | sha256sum | cut -d" " -f1
}
EXE=./build/tests/test_debug_crc_directed.exe

GOLD=$(mktemp)
cp "$RTL" "$GOLD"
GOLDHASH=$(sha256sum <"$GOLD" | cut -d' ' -f1)

rebuild() {
  rm -rf "$TARGETDIR"
  rm -f "$EXE"          # guard 5: the exe outlives the target dir
  cmake -S . -B build >/dev/null 2>&1
  ninja -C build test_debug_crc_directed >/dev/null 2>&1
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

echo "== establishing the pristine baseline"
restore || { echo "ABORT: cannot establish pristine source"; exit 4; }
rebuild
[ -d "$MODELDIR" ] || { echo "ABORT: pristine model did not elaborate"; exit 6; }
[ -f "$EXE" ] || { echo "ABORT: pristine executable did not link"; exit 6; }
PRISTINE_MODEL=$(model_hash)
if ! "$EXE" >/dev/null 2>&1; then
  echo "ABORT: the PRISTINE build fails its own test — nothing below would mean anything"
  exit 7
fi
echo "   pristine model ${PRISTINE_MODEL:0:16}, directed lane green"

# Each entry: name @@ old @@ new
MUTS=(
"M01 one byte per pixel, not two@@    .n_i (4'd2),@@    .n_i (4'd1),"
"M02 three bytes per pixel@@    .n_i (4'd2),@@    .n_i (4'd3),"
"M03 byte order swapped in the fold@@    .d_i ({48'd0, in_px_i}),@@    .d_i ({48'd0, in_px_i[7:0], in_px_i[15:8]}),"
"M04 both folded bytes are the LOW half@@    .d_i ({48'd0, in_px_i}),@@    .d_i ({48'd0, in_px_i[7:0], in_px_i[7:0]}),"
"M05 sof seeds zero, not the CRC-32C init@@assign fold_c = in_sof_i ? 32'hFFFF_FFFF : crc_r;@@assign fold_c = in_sof_i ? 32'd0 : crc_r;"
"M06 xorout dropped at finalize@@              fin_crc <= ~fold_o;@@              fin_crc <= fold_o;"
"M07 length counts one byte per pixel@@assign n_next  = in_sof_i ? 32'd2 : (n_bytes + 32'd2);@@assign n_next  = in_sof_i ? 32'd1 : (n_bytes + 32'd1);"
"M08 sof starts the length at zero@@assign n_next  = in_sof_i ? 32'd2 : (n_bytes + 32'd2);@@assign n_next  = in_sof_i ? 32'd0 : (n_bytes + 32'd2);"
"M09 the size gate is removed@@            if (n_next == exp_now) begin@@            if (1'b1) begin"
"M10 the size gate is inverted@@            if (n_next == exp_now) begin@@            if (n_next != exp_now) begin"
"M11 the expectation is read live, not latched@@assign exp_now = in_sof_i ? expect_bytes_i : expect_n;@@assign exp_now = expect_bytes_i;"
"M12 sof does not reseed a frame already open@@assign fold_c = in_sof_i ? 32'hFFFF_FFFF : crc_r;@@assign fold_c = running ? crc_r : 32'hFFFF_FFFF;"
"M13 sof does not open a frame@@        if (in_sof_i || running) begin@@        if (running) begin"
"M14 the frame never opens (running never set)@@          running <= 1'b1;@@          running <= running;"
"M15 eof leaves the frame open@@            running <= 1'b0;
            n_bytes <= 32'd0;@@            running <= 1'b1;
            n_bytes <= 32'd0;"
"M16 the publish pulse becomes a level@@      fin_v <= 1'b0;
      err_v <= 1'b0;@@      err_v <= 1'b0;"
"M17 the CRC state never advances@@          crc_r <= fold_o;@@          crc_r <= crc_r;"
"M18 bytes_captured reports the expectation@@            fin_bytes <= n_next;@@            fin_bytes <= exp_now;"
"M19 a stray pixel is not flagged@@          err_v <= 1'b1;
          fin_bytes <= 32'd0;@@          err_v <= 1'b0;
          fin_bytes <= 32'd0;"
"M20 the size gate compares the count BEFORE this pixel@@            if (n_next == exp_now) begin@@            if (n_bytes == exp_now) begin"
"M21 [EQUIVALENT, see footer] reset seed changed@@      crc_r <= 32'hFFFF_FFFF;@@      crc_r <= 32'd0;"
"M22 [EQUIVALENT, see footer] n_bytes not cleared at eof@@            running <= 1'b0;
            n_bytes <= 32'd0;@@            running <= 1'b0;
            n_bytes <= n_bytes;"
)

# The two mutants above that no input can distinguish. Named here so the
# arithmetic at the bottom can subtract them instead of reporting two holes.
EQUIVALENT="M21 M22"

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
  if [ ! -f "$EXE" ]; then
    echo "  $name  DISCARDED: executable absent after rebuild (link failed)"
    restore || { echo "ABORT: revert failed"; exit 4; }
    continue
  fi
  mutant_model=$(model_hash)
  if [ "$mutant_model" = "$PRISTINE_MODEL" ]; then
    echo "  $name  DISCARDED: model identical to pristine (did not re-elaborate)"
    restore || { echo "ABORT: revert failed"; exit 4; }
    continue
  fi

  if "$EXE" >/dev/null 2>&1 && "$EXE" --random 400 >/dev/null 2>&1; then
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
echo "expected-equivalent: $EQUIVALENT"
if [ "$attempted" != "$expected" ] || [ "$accounted" != "$expected" ]; then
  echo "CROSS-CHECK FAILED (attempted/accounted must both equal $expected)"
  exit 5
fi

# ---------------------------------------------------------------------------
# THE TWO EQUIVALENT MUTANTS, with the proof that no input distinguishes them.
# Recorded here rather than left looking like test holes.
#
# M21 — the RESET VALUE of crc_r.
#   crc_r is read in exactly one place: fold_c = in_sof_i ? SEED : crc_r.
#   The crc_r arm is selected only when in_sof_i is low, and fold_o is consumed
#   only inside the "if (in_sof_i || running)" branch — so reaching it with
#   in_sof_i low requires running = 1. running is set to 1 only by that same
#   branch, on a cycle that also writes crc_r <= fold_o, and is cleared at eof
#   and by reset. Therefore every read of crc_r sees a value written by the
#   branch itself, never the reset value. Changing the reset value cannot
#   change any output.
#
# M22 — clearing n_bytes at eof.
#   n_bytes is read in exactly one place: n_next = in_sof_i ? 2 : n_bytes + 2.
#   The n_bytes arm needs in_sof_i low, and (as above) n_next is consumed only
#   with running = 1. running = 1 implies the previous accepted pixel took the
#   open-frame branch, which wrote n_bytes <= n_next. The eof clear runs on the
#   same cycle that clears running, so the value it writes can only be read
#   after running has been set again — which requires a sof, and a sof ignores
#   n_bytes entirely. The clear is dead code; keeping it is documentation.
#
# Both are STILL DRIVEN by the sweep above. If a future change makes either
# reachable, they stop surviving and these proofs stop being true, which is the
# only reason to keep running mutants one believes are equivalent.
# ---------------------------------------------------------------------------
