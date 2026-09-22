FINDINGS-layere -- packet LAYERE
(the harness refuses a report file under runs/, so this commit message IS the
document, as the packet directed)

================================================================================
THE NUMBER: 10 -> 10.  `completion_register` RC 1, run BARE.
================================================================================

Unchanged, and that is the honest answer rather than a disappointing one. The
register counts ENTRIES, not signals. I21 is one boundary entry, it held four
signals, THREE OF THEM LEFT, and the fourth -- the `edge_*` tie-off -- still
holds it open. The packet predicted exactly this ("So I21 may not close even
with your build"), and manufacturing a fall out of it was the trap.

    MANDATORY GAPS REMAINING          : 10
      (5 tie-offs + 5 disconnected + 0 unbuilt + 0 uncited + 0 unresolvable)
      I13  I17  I20  I21  I34

Same five entries and the same five disconnected modules the packet named, so
the composition is identical before and after, not merely the total.

`packet_h_tieoff_audit`: 8 declared, 1 reasoned, 16 by group comment, 0 SILENT.
No new literal was introduced and none was hidden.

================================================================================
I21: NARROWED, NOT CLOSED.  Three of four signals discharged.
================================================================================

  (1) `terr_job_mat_a_i` / `_b_i` / `_weight_i`
      BEFORE: HELD -- "must stay until TESS gains a per-triangle layer-E path"
      AFTER:  DISCHARGED BY REMOVAL. The path exists; they are gone from the
              core's port list and from the board's.

  (2) `terr_job_view_mask_i`   discharged by TERRCLOSE, unchanged
  (3) `terr_sparse_fill_i`     confirmed OWNER KNOB, unchanged; no producer is
                               owed for a knob
  (4) the four `edge_*`        STILL HELD, literal 8'h00, argument untouched

The `edge_*` literal was NOT touched and must not be: a named constant hid four
tie-offs from `packet_h_tieoff_audit` once already, `MEASURE.GOVERNOR.md`
refuses ownership in writing, and the real producer is a frame-wide
reconciliation this console's one-patch-at-a-time serve order cannot supply.

WHAT I21 NEEDS NOW IS ONE THING AND IT IS NOT A WIRING JOB: a frame-wide LOD
decision pass holding sixteen levels per live patch across the whole visible
set.

================================================================================
THE RIDERS WERE REMOVED.  The evidence the replacement is real.
================================================================================

R13 ruled `job_mat_a`/`job_mat_b`/`job_weight` the WRONG CARRIER -- layer E is
per CELL, the job port is per SUBPATCH -- and named REMOVAL as their honest
closure ONCE a per-triangle layer-E path existed. It exists end to end, and the
consumer at the far end never moved:

  zhao_terrain_pagestream        reads layer E, emits {matA,matB,weight} per
                                 cell ON THE VERTEX BEAT
  zhao_terrain_compcache_front   files it in a material plane -- same parity,
                                 same arming law as the heights
  zhao_terrain_tess              reads it at the TRIANGLE's own cell and hands
                                 it out on ModeRef          <-- R13's reader
  zhao_terrain_group_seq         forwards it to `r_mat_*_o` off that same beat
  zhao_project_core              takes it on `ref_mat_*_i`, exactly as before

EVIDENCE, in order of how hard it is to fool:

1. terrain_pipe_differential -- 478 packets, 37 checks, 0 failures.
   It runs the whole pipe against zref and compares the OUTPUT PACKET field for
   field, mat_a/mat_b/weight included. Its expectation USED TO BE `s.mat_a`,
   pushed in on the job port and read straight back out -- a passthrough that
   would have passed whatever the tessellator did. It is now the played plane
   at the cell derived from THE ORACLE'S OWN TRIANGLE: the per-axis minimum
   world corner mapped back through `lat.wx`, an exact inverse because the
   placement is monotone with constant step and the geomorph moves y alone. The
   RTL's enumerator plays no part in forming the expectation.

2. terrain_tess_directed -- 6,758 checks (was 6,751).
   Every ModeRef triple carries the material of its own cell, across all four
   levels, with and without a coarser neighbour (so the ANNULUS path runs --
   its ring fans are not run-cell aligned and are the one place the cell law is
   a choice), under backpressure.

3. THE MATERIAL IS NOT SUBPATCH-UNIFORM, asserted as a property: one
   128-triangle level-0 job must carry >= 32 DISTINCT triples. This is the
   check a per-triangle comparison cannot replace -- a block emitting one
   cell's material for every triangle, compared against that same cell, passes.

4. pagestream_rtl_directed -- 54 checks, 0 failures, including all 1,024
   layer-E cells in cell order, byte for byte, across every straddling burst.

================================================================================
`mat_unarmed_o` WAS FIRED, with its negative control (R95)
================================================================================

terrain_tess_directed records the armed sweep's ZERO as a negative control
first, then disarms the played plane and the counter moves ONCE PER TRIPLE.
Disarming lowers `mat_valid_i` -- which is what the compose cache does with no
patch served -- so the stimulus is the real fault, not a simulation of it.

ITS TWO OPERANDS ARE NOT IN LOCKSTEP, checked before it was written:
`mat_valid_i` is written by the responder's arming register, the request by
TESS's enumerator. Different enables, different modules.

AND THE UNARMED VALUE IS CHECKED AGAINST POISON, NOT AGAINST ZERO. The harness
drives 0xA5 on the data wires while `mat_valid_i` is low, so a block that
passed the wires through emits 0xA5 and is caught. Had the model driven zeros,
"emits the declared {0,0,0}" and "passed the wires through" would be
indistinguishable -- the flattering direction.

`mat_oob_o` and `mat_cells_o` are fired too, on `tb_compcache_front`'s 9x9
instance: `mat_oob_o` is STRUCTURALLY UNREACHABLE at the production 33x33
because TESS clamps its cell to 0..31, which is the situation `cs_oob_o` is
already in and the reason that small instance exists.

THE SMOKE CANNOT REACH ANY OF THEM AND THE BENCH NOW SAYS SO. Every terrain
page the smoke plays fails its CRC, so no page becomes resident, PAGESTREAM is
never given a job and the compose cache never fills. `terr_tess_mat_unarmed_o`
is the near miss: the injected job DOES run TESS, but in ModeTri, and the
layer-E read is issued on ModeRef beats alone.

================================================================================
THREE DESIGN DECISIONS WORTH INHERITING
================================================================================

1. LAYER E RIDES THE VERTEX BEAT, and that is an IDENTITY, not a convenience.
   A 33x33 lattice walked in scan order visits every one of the 32x32 CELL
   ORIGINS exactly once, in cell order, because cell (ci,cj) IS vertex (ci,cj)
   for ci,cj < 32. 1,024 of the 1,089 beats carry a cell; 65 do not. A stream
   of its own would have been A SECOND WALK over the same page -- two cursors,
   one patch, reconciled by nothing, which is the join shape this console keeps
   getting wrong (I39, FIELDARM, TERRCLOSE's own slot-across-a-walk hazard). On
   one beat there is nothing left to reconcile.

2. THE PLANE LIVES INSIDE compcache_front BECAUSE OF THE ARMING LAW, not the
   storage. A separate block would have had to COPY `fill_par_q`, `serve_par_q`
   and the handover branch, and a copy of an arming law diverges in the
   direction nobody looks: patch N's material under patch N+1's heights, which
   RENDERS. `zhao_terrain_spdesc`'s header already records this tree's rule
   that two blocks arming off one event must not have two laws.

3. THE MODEREF OUTPUT HAD TO BECOME A QUEUE, and that IS the packet's named
   failure mode caught in advance. ModeRef performed no read before, so the
   triple and its identity registered in one cycle. A registered layer-E read
   puts one item in flight, so driving `ref_mat_a_o` from the live `mat_a_i`
   beside an `r_valid` set a cycle earlier is I39 IN ONE LINE: the response
   register has already moved on the moment the shell stalls, and the triple
   ships under the NEXT triangle's material with every counter balancing. The
   triple rides a pend register to its landing -- the same reason `pend_idx`
   exists. Depth 2 with one item in flight keeps the rate: 1 + 1 - 1 = 1 <= 1,
   one triangle per clock.

THE CELL LAW. The triangle's cell is the PER-AXIS MINIMUM LATTICE CORNER. On
the unstitched path that is an identity: SS4.3's pair puts both triangles of a
run-cell inside {i0,i0+s} x {j0,j0+s}, so the minimum IS the run-cell origin,
and at level 0 the run-cell IS the patch cell. Two places a choice is made --
stride > 1 takes the origin cell, a ring fan takes its footprint's min corner
-- and both are declared in the RTL with their rejected alternatives.

THE THREE-BYTE ELEMENT STRADDLES, and the header's no-straddle proof for the
height planes DOES NOT extend to it. The answer is a TWO-BYTE CARRY, not a
second buffer: the cursor advances by 3, strictly less than a burst, so
consecutive refills are exactly one burst apart and the bytes falling off the
front of the window are always the two the next cell might want. 16 flops
against 512. Verified over all 1,024 cells: 49 bursts, 0 stale-carry uses,
first cell at lane 6 so it never needs one.

================================================================================
TWO DEFECTS IN MY OWN WORK, both found by reading rather than by a gate
================================================================================

A. `mat_cells_o` WOULD HAVE READ 1,023 ON EVERY PAGE.

   `zhao_console_core`'s fill start is
       tps_v_valid && tps_v_ready && tps_v_first && tpc_placed
   -- the FIRST accepted vertex beat. Vertex (0,0). Which IS a cell origin. So
   `fill_go_c` and the first material write land on the SAME CLOCK, every page.
   My clear and my increment were two assignments in one always_ff racing to be
   last, and the `!fill_go_c` guard I added to settle that race drops cell 0:
   1,023 forever -- PRECISELY THE SIGNATURE OF THE MISSING-ROW DEFECT THE
   COUNTER EXISTS TO DETECT.

   No test could have caught it: the coincidence is a property of the CONSOLE's
   composition, and every component bench pulses fill_start on its own cycle.
   The compcache bench read 64 happily. Repaired as ONE decision with both
   cases named, not a guard.

B. AN OUT-OF-BOUNDS ARRAY READ THAT `--lint-only -Wall` PASSED IN SILENCE.

   `wantal_c` has THREE entries. Adding layer E as arbitration index 3 made
       refill_addr_c = (refill_c == P_E) ? wantalE_c : wantal_c[refill_c];
   index it with FOUR whenever layer E won. The ternary picks the other
   operand; the index is still evaluated and its value is not defined by the
   language. Verilator returned 0 diagnostics.

   That is this tree's own law about gates wearing a new costume. Repaired
   STRUCTURALLY -- `refill_addr_c` is assigned in the same arm that picks the
   plane, so the case that would make the index out of range never performs an
   index. A bounds check would have left the question alive.

   (A third, in a bench: the write loop's `else` cleared `s_cs_we` and not
   `s_mat_we`, so the plane was written at cell (7,7) for another ~97 cycles
   and `mat_cells_o` read 161. It failed HIGH -- "too many cells" about an RTL
   that was fine. A bench bug reading LOW would have looked like the
   missing-row fault and been chased in the module.)

================================================================================
TWO CORRECTIONS TO WHAT I WAS HANDED (R240)
================================================================================

TERRCLOSE said `zhao_terrain_tess.sv` has "no layer-E port, no material output
and NO CELL-KEYED READ OF ANY KIND". The first two were right. The third was
not: `cs_req_o`/`cs_ci_o`/`cs_cj_o`/`cs_substance_i` -- a cell-keyed single
query with a one-cycle registered response -- have been in that block since it
was written. THE LAYER-E PORT WAS MODELLED ON IT, which made the build cheaper
than the entry implied. The entry now carries the correction; the rest of
TERRCLOSE's tally was accurate and is kept, because it is the record of how
each signal was identified.

`zhao_terrain_topo` STILL HAS `job_mat_a_i` AND THAT IS DELIBERATE. It is not
composed in the console, not on the ruled port, and superseded by TESS's
ModeRef. R13 does not reach it; changing it would be scope creep with its own
tests to break.

================================================================================
COST -- hand-counted, in the UNFLATTERING direction.  Recorded, never a veto.
================================================================================

REGISTERS, counted by name from the RTL (not estimated):

    zhao_terrain_tess              +146  -22   (rq_* queue, rp_*, counter)
    zhao_terrain_compcache_front    +89        (mat_rd_q, 2 counters)
    zhao_terrain_pagestream        +594        (bufE_q 512, carry, cursor)
    zhao_terrain_group_seq                -24  (j_mat_a/b, j_weight)
    ------------------------------------------------------------------
    NET                            +783 flops

ALM, assuming NO register/LUT packing:

    783 net flops at 2/ALM, not packed with logic        ~390
    TESS cell min, clamps, credit, queue shift muxes      ~90
    COMPCACHE two 11-bit addresses, four range compares   ~20
    PAGESTREAM wantE_c and the align mask                 ~35
    PAGESTREAM the three byte extractors, 3 x (64:1 x 8) ~500
    ------------------------------------------------------------------
    TOTAL                                               ~1,035  -> call it 1,200

M10K: 6 for the material plane. 2 x 1,024 x 24 b = 49,152 bit, and 49,152 /
10,240 = 4.8 IS A BIT COUNT, NOT AN M10K COUNT -- the plane is 2,048 deep,
which forces the 2048x4 mode, so it takes SIX side by side to make 24 bits.
I wrote 5 in a first contract draft and corrected it in its own commit; the
flattering arithmetic arrives first and was off by 25%.

BANDWIDTH: +49 bursts per page, 105 -> 154, +47% OF PAGESTREAM'S READ
BANDWIDTH. One lattice goes from ~3,544 to 4,181 gpu clocks on the played
fabric. `blocks.yml`'s throughput row is CORRECTED rather than left describing
a block that no longer exists, and `pagestream_rtl_directed` derives 49 from
the layout independently of the RTL.

THE ONE CHEQUE THIS LEAVES, named so it is cashable:
~500 of those ALMs are the three 64:1 byte muxes, AND THEY ARE AVOIDABLE. The
layer-E cursor advances by exactly 3 bytes, monotonically, and never seeks --
so the extractor needs no random access at all. A 3-BYTE SHIFT REGISTER FED 8
BYTES AT A TIME replaces the whole mux tree with ~30 flops. That is a
restructure of `e_byte` and its carry, not a wiring change, and it was not
attempted here because it would have put the correctness this packet just
established back in play. It is worth roughly 480 ALM on a device at 97%.

NO QUARTUS WAS RUN (packet instruction). The area number is a hand count.

================================================================================
MODULES THAT MOVED
================================================================================

  zhao_terrain_tess              R13's reader; ModeRef queue; mat_unarmed_o
  zhao_terrain_compcache_front   the layer-E plane, write face, query face,
                                 two counters
  zhao_terrain_pagestream        the layer-E cursor, the two-byte carry,
                                 cells_streamed_o
  zhao_terrain_group_seq         riders REMOVED; material from the ModeRef beat
  zhao_terrain_pipe              riders REMOVED; layer-E read surfaced
  zhao_console_core              composition; THREE BOUNDARY INPUTS REMOVED;
                                 four counters added; entry I21 rewritten
  zhao_console_board             REGENERATED by gen_console_board.py
  zhao_prod_top                  REGENERATED by gen_prod_top.py
  zhao_terrain_pipe_rpp3_..._top REGENERATED from its template, which gains a
                                 played layer-E plane
  zhao_pair_tess_normals,
  zhao_pair_pagestream_patch     ports named

NO TIE-OFF WAS CREATED. No module was added, so NO `design/blocks.yml` ROW IS
OWED under R214 -- the three existing rows were amended (inputs, outputs,
counters, counter_ports, and PAGESTREAM's corrected throughput).

================================================================================
GATES -- every one run BARE
================================================================================

  check_console_inventory        0   374 declared, 246 elaborated, 252 sources
  check_prod_manifest            0   374 modules, 75 tops
  check_quartus17_syntax         0   590 files, no rejected forms
  check_case_labels              0
  mutant_copy_drift (post-commit)0   60 copies, all current            (R121)
  mutant_drivers                 0   105/107 run; 2 pre-existing, other lanes'
  uncashed_cheques               0   6 pre-existing unresolved ref models
  refmodel_liveness              0   the same 6, all pre-existing
  duplicate_functions            0   all pre-existing _v2 pairs
  completion_register            1   NORMAL -- 10 gaps remain
  wrapper_port_parity            0   after repairing both core mutant wrappers
  check_counters                 0   reports; new counters now NAMED (R110)
  check_findings_citations       0   no NEW dangling
  packet_h_tieoff_audit          0   8 declared, 1 reasoned, 16 grouped,
                                     0 SILENT
  packet_h_driver_contract       0
  packet_h_sibling_diff          0   no undeclared substantive change

FULL BUILD on the frozen committed tree: RC 0.

TERRAIN-SIDE ctest (-R terrain|compcache|pagestream|tess|compose|wcache|
group_seq|proj): 150 of 153 pass, 705 s. The three reds are ALL PROVABLY NOT
MINE, and one of them I repaired anyway:

  * lint_terrain_world -- WAS RED BEFORE THIS PACKET, now green. `lint_off`/
    `lint_on` do not nest: an inner `lint_on` added with entry I21's forwarded
    view-mask field re-enabled UNUSEDSIGNAL for the fifteen deliberately-unread
    signals below it. Verified pre-existing rather than assumed -- the merge
    base's own copy of the bench, linted against the merge base's own
    pagestream, produces the identical fifteen warnings at the identical lines.
  * terrain_mipreq_directed, terrain_psmux_directed -- both hang to their 300 s
    timeout. Each verilates EXACTLY ONE RTL file (zhao_terrain_mipreq.sv,
    zhao_terrain_psmux.sv) and neither those nor their drivers appear anywhere
    in this packet's diff, so the change cannot reach them. STRUCTURAL, not an
    alibi. Left for whoever owns them, reported rather than rediscovered.

SMOKE SWEEP -- ten forms, ONE FROZEN COMMITTED TREE, switches by SPLAT with
-Repo explicit, output files uniquely prefixed `layere_` because the scratchpad
is shared:

@@SMOKE@@
