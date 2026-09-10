#!/usr/bin/env bash
# regen-mutants.sh -- regenerate the three FORGE.CLIFF bitmap-RAM mutants from
# the CURRENT candidate source, so a mutant is never a positive control for a
# block that no longer exists. Each mutant = header + renamed module + ONE
# substantive line; the two walk-fault mutants also disable the two assertions
# that watch the invariant they break. Run from the zhaozhou root. Fails
# loudly if a pattern no longer matches (the candidate changed shape).
set -euo pipefail
SRC=fpga/rtl/forge/zhao_forge_cliff_ram.sv
D=$(dirname "$0")
python - "$SRC" "$D" <<'PY'
import sys
src,d=sys.argv[1:3]
s=open(src,encoding='utf-8').read()
def one(text,old,new):
    assert text.count(old)==1, "pattern not unique/missing: "+old[:70]
    return text.replace(old,new)
ASSERT_OLD="""      if (walk_sparse_c) begin
        a_walk_exact : assert (!walk_over_c);
        a_span_nonzero : assert (walk_done_c || !span_zero_c);
      end"""
ASSERT_NEW="""      // MUTANT: a_walk_exact and a_span_nonzero are DISABLED here -- they
      // watch exactly the invariant this copy breaks and would $stop the run
      // before walk_fault_o, the synthesizable instrument, can be read.
      // if (walk_sparse_c) begin
      //   a_walk_exact : assert (!walk_over_c);
      //   a_span_nonzero : assert (walk_done_c || !span_zero_c);
      // end"""
MERGE_W="""        es_wd_c = mtake_r;"""
# ---- A: merge writes a ZERO span ----
m=one(s,"module zhao_forge_cliff_ram (","module zhao_forge_cliff_ram_mutant (")
m=one(m,MERGE_W,"        es_wd_c = 6'd0;  // MUTANT: the merged head's span is written as ZERO (was mtake_r)")
m=one(m,ASSERT_OLD,ASSERT_NEW)
open('tests/mutants/zhao_forge_cliff_ram_mutant.sv','w',encoding='utf-8').write(open(d+'/mutant-A-header.txt',encoding='utf-8').read()+m)
# ---- A2: merge writes take+1 (overshoot) ----
m=one(s,"module zhao_forge_cliff_ram (","module zhao_forge_cliff_ram_over_mutant (")
m=one(m,MERGE_W,"        es_wd_c = mtake_r + 6'd1;  // MUTANT: one entry more than the merge consumed (was mtake_r)")
m=one(m,ASSERT_OLD,ASSERT_NEW)
open('tests/mutants/zhao_forge_cliff_ram_over_mutant.sv','w',encoding='utf-8').write(open(d+'/mutant-A2-header.txt',encoding='utf-8').read()+m)
# ---- B: prefetch one row off ----
m=one(s,"module zhao_forge_cliff_ram (","module zhao_forge_cliff_ram_rowoff_mutant (")
m=one(m,"    pf_row_c    = WinAW'(sc_cj_r) + WinAW'(3);",
        "    pf_row_c    = WinAW'(sc_cj_r) + WinAW'(2);  // MUTANT: prefetch row cj+2 (was cj+3)")
open('tests/mutants/zhao_forge_cliff_ram_rowoff_mutant.sv','w',encoding='utf-8').write(open(d+'/mutant-B-header.txt',encoding='utf-8').read()+m)
print("mutants regenerated: A (merge span 0), A2 (merge span take+1), B (prefetch row off)")
PY
