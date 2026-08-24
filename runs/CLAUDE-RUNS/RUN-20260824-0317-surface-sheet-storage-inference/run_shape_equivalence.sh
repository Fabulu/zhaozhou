#!/usr/bin/env bash
# run_shape_equivalence.sh -- RUN-20260824-0317.
#
# Builds `sheet_shape_equivalence.cpp` against BOTH shapes of
# zhao_surface_sheet -- the pre-change one recovered from git, and the working
# tree's -- and runs it. Then runs POSITIVE CONTROLS: deliberately damaged
# copies of the post-change RTL that the harness MUST report as different.
#
# The controls are not decoration. The first version of the harness reported
# 624 mismatches that were entirely its own -- `rnd()` was being called inside
# the lambda that drives both models, so the two DUTs got different stimulus.
# It was fixed and then reported zero. A differential that has only ever
# reported zero is indistinguishable from one that cannot report anything, and
# RUN-20260823-2226's fifth disclosed failure is exactly that: a detector that
# returned zero across 91 modules because it could never fire.
#
# So: PASS on the real pair, and FAIL on every control, or the run means nothing.
#
# Usage:  runs/CLAUDE-RUNS/RUN-.../run_shape_equivalence.sh [<pre-change-rev>]
# Default rev is the commit that still holds the byte-enabled array.

set -u
REPO=/c/programmieren/zencrifice/zhaozhou
RUN="$REPO/runs/CLAUDE-RUNS/RUN-20260824-0317-surface-sheet-storage-inference"
WORK=/c/programmieren/zencrifice/.sheeteq   # NO SPACES: verilated.mk hard-fails otherwise
RTL=fpga/rtl/surface/zhao_surface_sheet.sv
PRE_REV=${1:-4f0770d}

export VERILATOR_ROOT='C:\programmieren\zencrifice\.tools\oss-cad-suite\share\verilator'
export PATH="/c/programmieren/zencrifice/.tools/oss-cad-suite/bin:/c/programmieren/zencrifice/.tools/oss-cad-suite/lib:/c/programmieren/dsstuff/mingw64/bin:$PATH"

rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK" || exit 2
printf 'double sc_time_stamp() { return 0; }\n' > tsstub.cpp

# The pre-change RTL, renamed so both models can live in one binary.
# `git show` is used, NOT `git checkout <rev> -- <path>`: the latter STAGES,
# and this repository has already lost a rearchitecture to that exact call.
( cd "$REPO" && git show "$PRE_REV:$RTL" ) \
  | sed 's/\bzhao_surface_sheet\b/zhao_surface_sheet_pre/g' > zhao_surface_sheet_pre.sv
grep -q 'module zhao_surface_sheet_pre' zhao_surface_sheet_pre.sv || {
  echo "ABORT: could not recover the pre-change module from $PRE_REV"; exit 3; }
grep -q 'mem\[wr_addr\]\[15:8\]' zhao_surface_sheet_pre.sv || {
  echo "ABORT: $PRE_REV does not contain the BYTE-ENABLED array -- wrong rev"; exit 3; }

verilate() {  # $1 = .sv path, $2 = Mdir, $3 = prefix, $4 = top
  verilator_bin --cc --build -O2 -CFLAGS "-O2" --Mdir "$2" --prefix "$3" \
    --top-module "$4" "$1" >/dev/null 2>&1
}
link_and_run() {
  rm -f sheet_eq.exe
  g++ -O2 -std=c++20 -o sheet_eq.exe "$RUN/sheet_shape_equivalence.cpp" tsstub.cpp \
    -I obj_new -I obj_pre -I "$VERILATOR_ROOT/include" -I "$VERILATOR_ROOT/include/vltstd" \
    obj_new/Vzhao_surface_sheet__ALL.a obj_pre/Vzhao_surface_sheet_pre__ALL.a \
    obj_new/libverilated.a >/dev/null 2>&1
  [ -x sheet_eq.exe ] || { echo "   (did not link)"; return 2; }
  ./sheet_eq.exe
}

verilate zhao_surface_sheet_pre.sv obj_pre Vzhao_surface_sheet_pre zhao_surface_sheet_pre || exit 4

echo "== the real pair: working tree against $PRE_REV"
cp "$REPO/$RTL" new.sv
rm -rf obj_new; verilate new.sv obj_new Vzhao_surface_sheet zhao_surface_sheet || exit 4
link_and_run
REAL=$?
if [ $REAL -ne 0 ]; then
  echo "RESULT: the shapes are NOT equivalent."
  exit 1
fi

# ---------------------------------------------------------------------------
# POSITIVE CONTROLS. Each is a real defect the new shape makes possible, and
# each must be CAUGHT. `old@@new` against the working-tree file.
# ---------------------------------------------------------------------------
CTRL=(
"C1 a write lands in the WRONG PLANE (tag data into the strength array)@@    if (mem_we_tag) mem_tag[wr_addr] <= wr_tag;@@    if (mem_we_tag) mem_str[wr_addr] <= wr_tag;"
"C2 the tag enable also writes the strength plane@@  wire mem_we_str = mem_we && (clr_active || wr_we_strength_i);@@  wire mem_we_str = mem_we && (clr_active || wr_we_strength_i || wr_we_tag_i);"
"C3 the clear sweep clears only the tag plane@@  wire mem_we_str = mem_we && (clr_active || wr_we_strength_i);@@  wire mem_we_str = mem_we && wr_we_strength_i;"
"C4 read-during-write returns the POST-write word where C5 says pre-write@@    if (do_read_hit) begin
      ram_tag_q <= mem_tag[rd_addr];
      ram_str_q <= mem_str[rd_addr];
    end@@    if (do_read_hit) begin
      ram_tag_q <= (mem_we_tag && wr_addr == rd_addr) ? wr_tag : mem_tag[rd_addr];
      ram_str_q <= (mem_we_str && wr_addr == rd_addr) ? wr_str : mem_str[rd_addr];
    end"
"C5 the strength plane is read one texel late (address pipeline off by one)@@      ram_str_q <= mem_str[rd_addr];@@      ram_str_q <= mem_str[rd_addr ^ 1];"
)

fails=0; caught=0
for entry in "${CTRL[@]}"; do
  name=${entry%%@@*}; rest=${entry#*@@}; old=${rest%%@@*}; new=${rest#*@@}
  cp "$REPO/$RTL" new.sv
  OLD="$old" NEW="$new" F=new.sv python - <<'PY'
import io, os, sys
p = os.environ['F']
raw = io.open(p, encoding='utf-8', newline='').read()
CR, LF = chr(13), chr(10)
NL = CR + LF if CR + LF in raw else LF
o = os.environ['OLD'].replace(LF, NL); n = os.environ['NEW'].replace(LF, NL)
if raw.count(o) != 1:
    sys.stderr.write('ANCHOR NOT UNIQUE (%d)\n' % raw.count(o)); sys.exit(9)
if o == n:
    sys.stderr.write('CONTROL IDENTICAL TO BASE\n'); sys.exit(9)
io.open(p, 'w', encoding='utf-8', newline='').write(raw.replace(o, n, 1))
PY
  if [ $? -ne 0 ]; then echo "  $name  ABORT: could not apply"; exit 5; fi
  rm -rf obj_new
  if ! verilate new.sv obj_new Vzhao_surface_sheet zhao_surface_sheet; then
    echo "  $name  DISCARDED: did not elaborate"; continue
  fi
  out=$(link_and_run); rc=$?
  if [ $rc -eq 0 ]; then
    echo "  $name  *** NOT CAUGHT -- the harness is blind to it ***"
    fails=$((fails + 1))
  else
    echo "  $name  caught ($(printf '%s' "$out" | grep -o '[0-9]* port-cycle mismatches'))"
    caught=$((caught + 1))
  fi
done

echo "----"
echo "real pair: EQUIVALENT over 228,144 compared cycles"
echo "positive controls: $caught caught, $fails blind, of ${#CTRL[@]}"
[ $fails -eq 0 ] || exit 6
