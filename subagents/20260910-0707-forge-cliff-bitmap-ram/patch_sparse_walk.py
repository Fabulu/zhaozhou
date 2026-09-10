"""patch_sparse_walk.py -- the second-cut restructure of zhao_forge_cliff_ram.sv.

Kept as a record of exactly what changed between the first cut (compact
unconditionally after any merge) and the shipped shape (compact only on the
way to the threshold passes; StKeep/StEmit walk a sparse table by span).
Run once from the zhaozhou root; it refuses to run twice (patterns gone).
"""
import sys

p = 'fpga/rtl/forge/zhao_forge_cliff_ram.sv'
s = open(p, encoding='utf-8').read()


def one(old, new):
    global s
    assert s.count(old) == 1, "MISSING/AMBIGUOUS: " + old[:80]
    s = s.replace(old, new)


# ---- header S2 rewrite ----
one("""//     every live entry in scan order and no dead one. One COMPACTION pass
//     (StCompact) copies the live entries down to a dense 0..cnt'-1 prefix of
//     the SAME tables, in place — read `rd`, write `wr`, `wr <= rd` always,
//     and the write is SUPPRESSED while `wr == rd` so no same-address
//     read/write ever occurs. The priority is computed in that same pass
//     from the FINAL span endpoints (C2 preserved; the vdist schedule is the
//     golden's three clocks per edge), so the old StPrio is this pass and the
//     old StMdead's take-1 clear cycles are gone. Every later phase then runs
//     on a dense table with no liveness check; StKeep drops by compacting
//     again (kept entries slide down, `dropped` still counts BODIES, R2), and
//     StEmit walks 0..cnt''-1.
//     The roadmap's §6.2 sheet keeps a 2048x1 alive RAM; this route needs
//     none, which is one M10K fewer than that sheet.""",
    """//     every live entry in scan order and no dead one. So:
//       - StEmit and StKeep WALK the sparse table by span (`sparse_r` = a
//         merge happened) — no compaction pass at all when the merge alone
//         brought the page inside budget, or when vdist is off. This is the
//         golden's own cost or better: it skipped a dead entry in one cycle,
//         the walk skips it in zero. (The first cut compacted unconditionally
//         and the differential's cycle table showed it +512 on every
//         merge-only page; that table is the reason this rule exists.)
//       - Only the 33 threshold passes need a DENSE table, so only when a
//         priority degrade follows (over budget after merging, vdist on) does
//         StCompact run: read `rd`, write `wr`, `wr <= rd` always, the write
//         SUPPRESSED while `wr == rd` so no same-address read/write ever
//         occurs, and the priority computed in the same pass from the FINAL
//         span endpoints (C2 preserved; the golden's three clocks per edge).
//         The old StPrio IS this pass; the old StMdead's take-1 clear cycles
//         are gone. StBsCount/StGtCount/StKeep then run on the dense prefix
//         with no liveness check and no dead-entry iterations.
//       - StKeep drops by compacting (kept entries slide down to `wr`, same
//         suppression rule; `dropped` still counts BODIES, R2) and StEmit then
//         walks 0..cnt''-1 densely.
//     The roadmap's §6.2 sheet keeps a 2048x1 alive RAM; this route needs
//     none, which is one M10K fewer than that sheet.""")

one("""// `walk_fault_o` counts the span walk landing PAST `cnt_r` or reading a span
// of ZERO (which would stall the walk forever; the RTL advances by one and
// counts instead). Both states are unreachable while S2's theorem holds and
// every enumerated span was written, so no legal stimulus can move it; its
// positive control is the committed mutant tests/mutants/
// zhao_forge_cliff_ram_mutant.sv (enumeration writes span 0), driven with
// inverted polarity by tests/forge/forge_cliff_ram_mutant_control.cpp.""",
    """// `walk_fault_o` counts a span walk (StCompact, or StKeep/StEmit while
// `sparse_r`) landing PAST `cnt_r`, or reading a span of ZERO (which would
// stall the walk forever; the RTL advances by one and counts instead). Both
// states are unreachable while S2's theorem holds and every span was written
// with 1 or a take >= 2, so no legal stimulus can move it. Its positive
// controls are two committed mutants, each ONE line in the merge's span write
// (tests/mutants/): zhao_forge_cliff_ram_mutant.sv writes 0 (the zero-span
// trigger and the no-hang guard) and zhao_forge_cliff_ram_over_mutant.sv
// writes take+1 (the overshoot trigger); both are driven with inverted
// polarity by tests/forge/forge_cliff_ram_mutant_control.cpp. A first cut
// broke the ENUMERATION span instead and the counter stayed 0: with every
// span 0 no run forms, nothing merges, and no walk is ever entered — a
// positive control that never reaches the detector is not a control.""")

# ---- declarations ----
one("""  logic           degrade_r;  // S2: (cnt - merged) > Budget, latched at the merge phase's end""",
    """  logic           sparse_r;   // S2: a merge happened, the table has dead entries — walk by span""")

# ---- comb: walk fault plumbing ----
one("""  logic           walk_done_c, walk_over_c, span_zero_c;
  logic [EIW-1:0] walk_adv_c;
  logic           compact_adv_c;   // this cycle consumes the entry at idx_r
  logic           compact_copy_c;  // ... and it moves (wr != rd)
  logic           run_close_c;
  logic           over_c;
  always_comb begin
    walk_done_c   = (idx_r >= cnt_r);
    walk_over_c   = (idx_r > cnt_r);
    span_zero_c   = (cur_span_c == 6'd0);
    walk_adv_c    = span_zero_c ? EIW'(1) : {{(EIW - 6) {1'b0}}, cur_span_c};
    compact_adv_c = (st_r == StCompact) && !walk_done_c && (!vden_r || (pr_ph_r == 2'd2));
    compact_copy_c = compact_adv_c && (wr_r != idx_r);""",
    """  logic           walk_done_c, walk_over_c, span_zero_c;
  logic [EIW-1:0] walk_adv_c, step_c;
  logic           compact_adv_c;   // StCompact: this cycle consumes the entry at idx_r
  logic           compact_copy_c;  // ... and it moves (wr != rd)
  logic           walk_sparse_c;   // a span walk is in progress in this state
  logic           walk_step_c;     // ... and idx_r advances by span this cycle
  logic           walk_fault_inc_c;
  logic           run_close_c;
  logic           over_c;
  always_comb begin
    walk_done_c   = (idx_r >= cnt_r);
    walk_over_c   = (idx_r > cnt_r);
    span_zero_c   = (cur_span_c == 6'd0);
    walk_adv_c    = span_zero_c ? EIW'(1) : {{(EIW - 6) {1'b0}}, cur_span_c};
    compact_adv_c = (st_r == StCompact) && !walk_done_c && (pr_ph_r == 2'd2);
    compact_copy_c = compact_adv_c && (wr_r != idx_r);
    walk_sparse_c = (st_r == StCompact) || (sparse_r && (st_r == StKeep || st_r == StEmit));
    // the cursor step: by span while sparse, by one on a dense table
    step_c = walk_sparse_c ? walk_adv_c : EIW'(1);
    walk_step_c = compact_adv_c ||
                  (sparse_r && st_r == StKeep && !walk_done_c) ||
                  (sparse_r && st_r == StEmit && emit_live_r && edge_ready_i);
    // walk_over_c can only be true on the single cycle a walk terminates
    // (idx_r is reset on leaving), so it is counted exactly once.
    walk_fault_inc_c = walk_sparse_c && ((walk_step_c && span_zero_c) || walk_over_c);""")

one("""        pm_we_c = compact_adv_c && vden_r;""", """        pm_we_c = compact_adv_c;""")

# ---- ff reset ----
one("""      degrade_r   <= 1'b0;
""", """      sparse_r    <= 1'b0;
""")

# ---- counter increment site, before the case ----
one("""      // S1: the row rotate is shared by StPrime and StEnum.
      if (rotate_c) begin""",
    """      // S2: the one instrument (see header). Saturating.
      if (walk_fault_inc_c && walk_fault_o != {WalkFW{1'b1}}) begin
        walk_fault_o <= walk_fault_o + {{(WalkFW - 1) {1'b0}}, 1'b1};
      end

      // S1: the row rotate is shared by StPrime and StEnum.
      if (rotate_c) begin""")

# ---- StIdle: dense ----
one("""            merged_r  <= 12'd0;
            dropped_r <= 12'd0;
            st_r      <= StLoad;""",
    """            merged_r  <= 12'd0;
            dropped_r <= 12'd0;
            sparse_r  <= 1'b0;
            st_r      <= StLoad;""")

# ---- StMsel exit ----
one("""            // The merge phase is over. S2: compact whenever something died,
            // or whenever a priority pass is needed anyway (it IS the
            // priority pass). merged == 0 with no vdist needs neither: the
            // table is already dense and StKeep runs on it directly, exactly
            // as the golden did.
            idx_r     <= {EIW{1'b0}};
            wr_r      <= {EIW{1'b0}};
            pr_ph_r   <= 2'd0;
            degrade_r <= over_c;
            if (merged_r != 12'd0 || (over_c && vden_r)) begin
              st_r <= StCompact;
            end else if (over_c) begin""",
    """            // The merge phase is over. S2: the table is sparse iff something
            // merged; only the threshold passes need it dense, so StCompact
            // runs only on the way to them. Otherwise StKeep / StEmit walk
            // the sparse table by span at the golden's own cost or better.
            idx_r    <= {EIW{1'b0}};
            wr_r     <= {EIW{1'b0}};
            pr_ph_r  <= 2'd0;
            sparse_r <= (merged_r != 12'd0);
            if (over_c && vden_r) begin
              st_r <= StCompact;
            end else if (over_c) begin""")

# ---- StCompact ----
one("""        // S2: the span-walk compaction, fused with the priority build (C2).
        // rd = idx_r walks live entries (rd += span); wr trails it. With vdist
        // on, three clocks per live entry exactly as the golden's StPrio:
        // issue va, capture va and issue vb, capture vb and store the max at
        // wr. With vdist off, one clock per live entry. The copies and the
        // priority write are ek/es/pm_we_c.
        StCompact: begin
          if (walk_done_c) begin
            if (walk_over_c && walk_fault_o != {WalkFW{1'b1}}) begin
              walk_fault_o <= walk_fault_o + {{(WalkFW - 1) {1'b0}}, 1'b1};
            end
            cnt_r   <= wr_r;   // dense from here on
            idx_r   <= {EIW{1'b0}};
            wr_r    <= {EIW{1'b0}};
            pr_ph_r <= 2'd0;
            if (degrade_r) begin
              if (vden_r) begin
                thr_r      <= 32'd0;
                bs_bit_r   <= 6'd31;
                bs_count_r <= {EIW{1'b0}};
                st_r       <= StBsCount;
              end else begin
                thr_r      <= 32'd0;
                tie_left_r <= EIW'(Budget);
                st_r       <= StKeep;
              end
            end else begin
              emit_live_r <= 1'b0;
              st_r        <= StEmit;
            end
          end else if (vden_r && pr_ph_r == 2'd0) begin
            pr_ph_r <= 2'd1;
          end else if (vden_r && pr_ph_r == 2'd1) begin
            pr_va_r <= vd_data_i;
            pr_ph_r <= 2'd2;
          end else begin
            // compact_adv_c: consume the entry at rd.
            if (span_zero_c && walk_fault_o != {WalkFW{1'b1}}) begin
              walk_fault_o <= walk_fault_o + {{(WalkFW - 1) {1'b0}}, 1'b1};
            end
            pr_ph_r <= 2'd0;
            wr_r    <= wr_r + {{(EIW - 1) {1'b0}}, 1'b1};
            idx_r   <= idx_r + walk_adv_c;
          end
        end""",
    """        // S2: the span-walk compaction, fused with the priority build (C2).
        // Entered only when a priority degrade follows (vdist on, still over
        // budget). rd = idx_r walks live entries (rd += span); wr trails it.
        // Three clocks per live entry exactly as the golden's StPrio: issue
        // va, capture va and issue vb, capture vb and store the max at wr.
        // The copies and the priority write are ek/es/pm_we_c.
        StCompact: begin
          if (walk_done_c) begin
            cnt_r      <= wr_r;   // dense from here on
            sparse_r   <= 1'b0;
            idx_r      <= {EIW{1'b0}};
            wr_r       <= {EIW{1'b0}};
            pr_ph_r    <= 2'd0;
            thr_r      <= 32'd0;
            bs_bit_r   <= 6'd31;
            bs_count_r <= {EIW{1'b0}};
            st_r       <= StBsCount;
          end else if (pr_ph_r == 2'd0) begin
            pr_ph_r <= 2'd1;
          end else if (pr_ph_r == 2'd1) begin
            pr_va_r <= vd_data_i;
            pr_ph_r <= 2'd2;
          end else begin
            // compact_adv_c: consume the entry at rd.
            pr_ph_r <= 2'd0;
            wr_r    <= wr_r + {{(EIW - 1) {1'b0}}, 1'b1};
            idx_r   <= idx_r + walk_adv_c;
          end
        end""")

# ---- StKeep ----
one("""        // One pass in SCAN ORDER over the dense table: a kept entry slides
        // down to wr (ek/es_we_c, suppressed while wr == rd); a dropped one
        // adds its whole BODY count (R2). cnt' <- wr at the end.
        StKeep: begin
          if (idx_r == cnt_r) begin
            cnt_r       <= wr_r;
            idx_r       <= {EIW{1'b0}};
            emit_live_r <= 1'b0;
            st_r        <= StEmit;
          end else begin
            if (keep_c) begin
              if (spend_c) tie_left_r <= tie_left_r - {{(EIW - 1) {1'b0}}, 1'b1};
              wr_r <= wr_r + {{(EIW - 1) {1'b0}}, 1'b1};
            end else begin
              dropped_r <= dropped_r + {6'd0, cur_span_c};
            end
            idx_r <= idx_r + {{(EIW - 1) {1'b0}}, 1'b1};
          end
        end""",
    """        // One pass in SCAN ORDER — by span while the table is sparse (vdist
        // off, so no StCompact ran), by one when dense: a kept entry slides
        // down to wr (ek/es_we_c, suppressed while wr == rd); a dropped one
        // adds its whole BODY count (R2). cnt' <- wr at the end, dense.
        StKeep: begin
          if (walk_done_c) begin
            cnt_r       <= wr_r;
            sparse_r    <= 1'b0;
            idx_r       <= {EIW{1'b0}};
            emit_live_r <= 1'b0;
            st_r        <= StEmit;
          end else begin
            if (keep_c) begin
              if (spend_c) tie_left_r <= tie_left_r - {{(EIW - 1) {1'b0}}, 1'b1};
              wr_r <= wr_r + {{(EIW - 1) {1'b0}}, 1'b1};
            end else begin
              dropped_r <= dropped_r + {6'd0, cur_span_c};
            end
            idx_r <= idx_r + step_c;
          end
        end""")

# ---- StEmit ----
one("""        // The table is dense: every entry below cnt_r is emitted, in order.
        StEmit: begin
          if (emit_live_r) begin
            if (edge_ready_i) begin
              emit_live_r <= 1'b0;
              if (triangles_submitted_o < (CntMax - 32'd1)) begin
                triangles_submitted_o <= triangles_submitted_o + 32'd2;  // C3
              end
              idx_r <= idx_r + {{(EIW - 1) {1'b0}}, 1'b1};
            end
          end else if (idx_r == cnt_r) begin""",
    """        // Every live entry below cnt_r is emitted, in order: by span while
        // sparse (the merge alone brought the page inside budget), by one on
        // a dense table. edge_rd_c still addresses the emitted entry while
        // emit_live_r, so its span is the step.
        StEmit: begin
          if (emit_live_r) begin
            if (edge_ready_i) begin
              emit_live_r <= 1'b0;
              if (triangles_submitted_o < (CntMax - 32'd1)) begin
                triangles_submitted_o <= triangles_submitted_o + 32'd2;  // C3
              end
              idx_r <= idx_r + step_c;
            end
          end else if (walk_done_c) begin""")

# ---- assertions ----
one("""      // The span walk lands exactly on cnt_r and never reads a zero span.
      if (st_r == StCompact) begin
        a_walk_exact : assert (!walk_over_c);
        a_span_nonzero : assert (walk_done_c || !span_zero_c);
      end""",
    """      // Every span walk lands exactly on cnt_r and never reads a zero span.
      if (walk_sparse_c) begin
        a_walk_exact : assert (!walk_over_c);
        a_span_nonzero : assert (walk_done_c || !span_zero_c);
      end""")

open(p, 'w', encoding='utf-8').write(s)
print("restructured", p)
