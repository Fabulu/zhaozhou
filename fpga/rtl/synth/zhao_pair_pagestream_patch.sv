// zhao_pair_pagestream_patch.sv — CHARACTERIZATION WRAPPER, not a console block.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS, AND WHY IT IS THE SECOND VERSION
// ---------------------------------------------------------------------------
// `zhao_terrain_pagestream` refitted at 93.91 MHz against the 100 MHz product
// clock and failed its `min_fmax_mhz` rule. Splitting all 2,000 summarised
// paths by endpoint (`tools/quartus/split_setup_paths.py`) showed all 23
// negative-slack paths ending at a VIRTUAL PIN, with the worst core-to-core
// path at 107.38 MHz. The block is not slow. What the leaf fit cannot see is
// the SEAM:
//
//   * PAGESTREAM's height outputs are COMBINATIONAL -- `assign v_base_o =
//     sample_of(2'd0)`, a staging-buffer byte lane indexed by `vidx_q`, 4.30 ns
//     of it. That is deliberate: a memory there would cost a cycle per vertex,
//     1,089 per lattice, which is why `max_m10k: 0` gates it.
//   * TERRAIN.PATCH puts `fx_add_sat` -- a 33-bit saturating add -- plus two
//     32-bit clamp comparators on exactly those inputs BEFORE its first flop.
//
// Each leaf fit sees one half. PAGESTREAM measures 4.3 ns terminating at a pad;
// PATCH measures its adder starting from one. Both rows are wrong at once, one
// pessimistic and one optimistic.
//
// THE FIRST VERSION OF THIS FILE (`zhao_terrain_compose_seam.sv`, replaced by
// this one) wired the two blocks together and then exposed every remaining port
// of the pair directly at the top. That reintroduced the exact contamination
// the measurement was built to remove -- hundreds of virtual pins, hanging off
// combinational paths, on a block whose whole problem was combinational paths
// to virtual pins.
//
// The repository had already solved this on 2026-08-23 and I did not look. The
// 2026-08-23 budget audit asked for "registered characterisation wrappers --
// registered stimulus -> DUT -> registered hash sink", because "raw leaf blocks
// with hundreds of virtual pins are poor physical models", and four such
// wrappers exist in this directory. This is the fifth, in their shape:
// TESS+NORMALS, TMU+CACHE, FRAGMENT+TILESTORE, SETUP+BINNER, and now
// PAGESTREAM+PATCH.
//
// NOT INSTANTIATED BY THE CONSOLE. Absent from the shell QSF and from
// ZHAO_SHELL_RTL, so it cannot affect the shell fit or the source-list parity
// gate. Synthesis reports only.
//
// ---------------------------------------------------------------------------
// WHAT IT MODELS, AND WHAT IT DOES NOT
// ---------------------------------------------------------------------------
// The page bytes arrive from a REGISTERED memory read, because that is what
// they are in the console: MEM.GUARD forwards a burst out of SDRAM and the
// beats arrive registered. Driving them combinationally from the stimulus would
// flatter the result by deleting a real cycle boundary on the exact path being
// measured.
//
// The guard response is registered stimulus. Nothing here models the guard's
// two-cycle protocol -- `rsp.ready` as a level, `rsp.ok` as a pulse one cycle
// after accept -- and it does not need to: this wrapper is never simulated and
// never taped out. It exists to put the pair's real logic in front of the
// fitter with its seam internal and its ends registered.
//
// IT DOES NOT MODEL backpressure from whatever consumes patch_state; `st_ready`
// is tied high, so this asks "how fast can the pair run when nothing stalls
// it", which is the question a clock target is about. Same choice, same reason,
// as `zhao_pair_tess_normals`'s `nrm_ready_i`.
//
// ---------------------------------------------------------------------------
// THE THREE PLACES THE BLOCKS DO NOT MEET, marked GLUE at their sites
// ---------------------------------------------------------------------------
// Each is a live owner ruling, and each exists ONLY IN A TESTBENCH today, which
// is itself the finding:
//
//   1. PLACEMENT. `wx_i`/`wz_i` are the vertex's world position and NOTHING IN
//      fpga/rtl computes them. The composed bench uses a multiply per vertex
//      that lands combinationally on PATCH's coverage comparators. Reproduced
//      so the fit measures the seam that exists.
//   2. THE DUAL FLAG. `dual_i` is bit 3 of the flags PAGESTREAM carries whole
//      and uninterpreted. Somebody must interpret it; today it is a bench.
//   3. THE SOURCE-ID NARROWING. PAGESTREAM carries T5's 32-bit `source_id`,
//      PATCH takes 16. Composing the pair drops sixteen bits of provenance.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_pair_pagestream_patch
  import zhao_pkg::*;
(
    input  var logic        clk,
    input  var logic        rst_n,
    input  var logic        stim_valid_i,
    input  var logic [31:0] stim_i,
    output var logic [31:0] hash_o
);

  localparam int unsigned SLOTW = 11;
  localparam int unsigned GENW  = 8;

  // GLUE 2: which bit of the record's flags means DUAL.
  localparam int unsigned FLAG_DUAL_BIT = 3;

  // ---- registered stimulus -------------------------------------------------
  logic        stim_valid_q;
  logic [31:0] stim_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      stim_valid_q <= 1'b0;
      stim_q       <= 32'd0;
    end else begin
      stim_valid_q <= stim_valid_i;
      stim_q       <= stim_i;
    end
  end

  // ---- the page, as a registered-read memory -------------------------------
  // 64 entries of 64 bits is not a page; it is enough to make the beat a MEMORY
  // READ rather than a wire, which is the property being modelled. The beat
  // path is the one that feeds the staging buffers whose combinational read is
  // the whole reason this wrapper exists.
  logic [63:0] beat_mem [0:63];
  logic [63:0] beat_data_q;
  logic        beat_valid_q, beat_last_q;
  logic [5:0]  beat_addr_q;

  always_ff @(posedge clk) begin
    beat_addr_q  <= stim_q[5:0];
    beat_data_q  <= beat_mem[beat_addr_q];
    beat_valid_q <= stim_valid_q;
    beat_last_q  <= (stim_q[5:0] == 6'h3F);
    if (stim_valid_i) beat_mem[stim_i[5:0]] <= {stim_i, ~stim_i};
  end

  // The guard's answer, registered. Not the real two-cycle protocol; see the
  // header. What matters is that PAGESTREAM's guard inputs are register
  // outputs and not pins.
  zhao_guard_rsp_t guard_rsp_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) guard_rsp_q <= '0;
    else        guard_rsp_q <= '{ready: stim_q[24], ok: stim_q[25], violation: stim_q[26]};
  end

  // ---- DUT 1: the page streamer -------------------------------------------
  zhao_guard_req_t    guard_req_w;
  // Flat view of the request, so the hash sink can fold every bit including
  // the 64-bit `be` mask. $bits rather than a written-out 103, because the
  // struct is versioned and a hand-copied width is a stale width waiting.
  logic [$bits(zhao_guard_req_t)-1:0] guard_req_bits;
  logic               ps_v_valid, ps_v_ready;
  logic signed [15:0] ps_base, ps_scar, ps_bottom;
  logic [5:0]         ps_vi, ps_vj;
  logic               ps_first, ps_last;
  logic [SLOTW-1:0]   ps_slot;
  logic [GENW-1:0]    ps_gen;
  logic [31:0]        ps_epoch, ps_src_id;
  logic [15:0]        ps_flags;
  logic               ps_j_ready, ps_idle;
  logic               ps_done_valid, ps_done_ok;
  logic [SLOTW-1:0]   ps_done_slot;
  logic [GENW-1:0]    ps_done_gen;
  logic [31:0]        ps_done_epoch, ps_done_src;
  logic [3:0]         ps_done_verdict;
  logic [31:0]        ps_lat_str, ps_lat_ref, ps_vtx_str, ps_bursts, ps_denied, ps_incomplete;

  zhao_terrain_pagestream #(
      .SLOTW(SLOTW),
      .GENW (GENW)
  ) u_pagestream (
      .clk(clk), .rst_n(rst_n),
      .cfg_vram_client_i(zhao_client_e'(stim_q[30:28])),
      .cfg_epoch_i      (stim_q),
      .j_valid_i        (stim_valid_q),
      .j_ready_o        (ps_j_ready),
      .j_slot_i         (stim_q[SLOTW-1:0]),
      .j_gen_i          (stim_q[GENW-1:0]),
      .j_epoch_i        (stim_q),
      .j_src_id_i       (stim_q),
      .j_flags_i        (stim_q[15:0]),
      .guard_req_o      (guard_req_w),
      .guard_rsp_i      (guard_rsp_q),
      .beat_valid_i     (beat_valid_q),
      .beat_data_i      (beat_data_q),
      .beat_last_i      (beat_last_q),
      .v_valid_o        (ps_v_valid),
      .v_ready_i        (ps_v_ready),
      .v_base_o         (ps_base),
      .v_scar_o         (ps_scar),
      .v_bottom_o       (ps_bottom),
      .v_vi_o           (ps_vi),
      .v_vj_o           (ps_vj),
      .v_first_o        (ps_first),
      .v_last_o         (ps_last),
      .v_slot_o         (ps_slot),
      .v_gen_o          (ps_gen),
      .v_epoch_o        (ps_epoch),
      .v_src_id_o       (ps_src_id),
      .v_flags_o        (ps_flags),
      .done_valid_o     (ps_done_valid),
      .done_ready_i     (1'b1),
      .done_slot_o      (ps_done_slot),
      .done_gen_o       (ps_done_gen),
      .done_epoch_o     (ps_done_epoch),
      .done_ok_o        (ps_done_ok),
      .done_verdict_o   (ps_done_verdict),
      .done_src_id_o    (ps_done_src),
      .lattices_streamed_o(ps_lat_str),
      .lattices_refused_o (ps_lat_ref),
      .vertices_streamed_o(ps_vtx_str),
      .bursts_read_o      (ps_bursts),
      .guard_denied_o     (ps_denied),
      .incomplete_o       (ps_incomplete),
      .idle_o             (ps_idle)
  );

  assign guard_req_bits = guard_req_w;

  // GLUE 1: placement. wx FOLLOWS vi (the COLUMN) and wz FOLLOWS vj (the ROW).
  // The multiply is on the vertex's own cycle, so it is part of the seam being
  // measured -- and if it is what costs the clock, an accumulator is the answer
  // and this fit is what says so.
  //
  // `$signed(...)` and NOT `signed'(...)`: the composed bench writes the cast
  // form and is never synthesised; Quartus 17.0.2 has already killed two fits
  // of this lane on cast syntax, 57 s and 30 s in (QUARTUS_GOTCHAS 4b).
  logic signed [31:0] wx_c, wz_c;
  assign wx_c = $signed(stim_q) + ($signed(stim_q) * $signed({26'd0, ps_vi}));
  assign wz_c = $signed(stim_q) + ($signed(stim_q) * $signed({26'd0, ps_vj}));

  // ---- DUT 2: the composer. THIS is the seam being measured. --------------
  logic               pt_st_valid, pt_st_dirty, pt_idle;
  logic signed [31:0] pt_top, pt_bottom, pt_ctop;
  logic [15:0]        pt_src_id, pt_sp_dirty;
  logic               pt_add_ready, pt_add_accept, pt_add_reject;
  logic [4:0]         pt_fields_active;
  logic [15:0]        pt_trace_patch, pt_trace_cmd;
  logic [31:0]        pt_trace_hash, pt_rejected, pt_evaluated;
  logic               pt_fld_ready, pt_fld_covers;

  zhao_terrain_patch u_patch (
      .clk(clk), .rst_n(rst_n),
      .list_clear_i    (stim_q[27]),
      .patch_id_i      (stim_q[15:0]),
      .fld_add_valid_i (stim_valid_q),
      .fld_add_ready_o (pt_add_ready),
      .fld_add_x0_i    ($signed(stim_q)),
      .fld_add_z0_i    ($signed(stim_q)),
      .fld_add_x1_i    ($signed(~stim_q)),
      .fld_add_z1_i    ($signed(~stim_q)),
      .fld_add_hash_i  (stim_q),
      .fld_add_cmd_i   (stim_q[15:0]),
      .fld_add_accept_o(pt_add_accept),
      .fld_add_reject_o(pt_add_reject),
      .fields_active_o (pt_fields_active),
      .trace_patch_id_o(pt_trace_patch),
      .trace_hash_o    (pt_trace_hash),
      .trace_cmd_o     (pt_trace_cmd),
      .programs_rejected_o(pt_rejected),

      // THE SEAM. Five real wires between two real blocks, no LFSR standing in
      // for a neighbour -- which is the distinction zhao_prod_top cannot make.
      .vtx_valid_i(ps_v_valid),
      .vtx_ready_o(ps_v_ready),
      .base_i     (ps_base),
      .scar_i     (ps_scar),
      .bottom_i   (ps_bottom),
      .dual_i     (ps_flags[FLAG_DUAL_BIT]),
      .wx_i       (wx_c),
      .wz_i       (wz_c),
      .vi_i       (ps_vi),
      .vj_i       (ps_vj),
      // GLUE 3: the 32 -> 16 narrowing, written as an explicit slice so it is
      // visible in the source rather than happening silently at the port.
      .src_id_i   (ps_src_id[15:0]),

      .fld_valid_i (stim_valid_q),
      .fld_ready_o (pt_fld_ready),
      .fld_height_i($signed(stim_q)),
      .fld_covers_o(pt_fld_covers),

      .st_valid_o      (pt_st_valid),
      .st_ready_i      (1'b1),
      .top_o           (pt_top),
      .bottom_o        (pt_bottom),
      .compose_top_o   (pt_ctop),
      .st_dirty_o      (pt_st_dirty),
      .st_src_id_o     (pt_src_id),
      .subpatch_dirty_o(pt_sp_dirty),
      .terrain_samples_evaluated_o(pt_evaluated),
      .idle_o          (pt_idle)
  );

  // ---- registered hash sink -----------------------------------------------
  // Everything the pair produces folds into one register, so no result reaches
  // a pin and the fitter cannot optimise a lane away for being unobserved.
  logic [31:0] hash_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) hash_q <= 32'd0;
    else if (pt_st_valid)
      hash_q <= (hash_q ^ pt_top) + (hash_q << 5) + pt_bottom + pt_ctop
              + {15'd0, pt_st_dirty, pt_src_id}
              + {16'd0, pt_sp_dirty};
    else
      hash_q <= hash_q
              ^ {31'd0, ps_j_ready}      ^ {31'd0, ps_idle}
              ^ {31'd0, ps_v_valid}      ^ {31'd0, ps_first}
              ^ {31'd0, ps_last}         ^ {31'd0, ps_done_valid}
              ^ {31'd0, ps_done_ok}      ^ {28'd0, ps_done_verdict}
              ^ {21'd0, ps_done_slot}    ^ {24'd0, ps_done_gen}
              ^ {21'd0, ps_slot}         ^ {24'd0, ps_gen}
              ^ {26'd0, ps_vi}           ^ {26'd0, ps_vj}
              ^ {16'd0, ps_flags}
              ^ ps_epoch ^ ps_src_id ^ ps_done_epoch ^ ps_done_src
              ^ ps_lat_str ^ ps_lat_ref ^ ps_vtx_str ^ ps_bursts
              ^ ps_denied ^ ps_incomplete
              ^ {31'd0, pt_idle}         ^ {31'd0, pt_add_ready}
              ^ {31'd0, pt_add_accept}   ^ {31'd0, pt_add_reject}
              ^ {27'd0, pt_fields_active}
              ^ {31'd0, pt_fld_ready}    ^ {31'd0, pt_fld_covers}
              ^ {16'd0, pt_trace_patch}  ^ {16'd0, pt_trace_cmd}
              ^ pt_trace_hash ^ pt_rejected ^ pt_evaluated
              // THE WHOLE GUARD REQUEST, folded as flat bits rather than
              // field by field. Picking fields left `be` -- the 64-bit byte
              // enable mask -- unobserved, and an unobserved output is one the
              // fitter may delete, which would measure a read client that does
              // not compute its own masks. Every bit of the struct is folded,
              // so nothing in it can be optimised away for being unwatched.
              ^ guard_req_bits[31:0] ^ guard_req_bits[63:32]
              ^ guard_req_bits[95:64] ^ {25'd0, guard_req_bits[102:96]};
  end
  assign hash_o = hash_q;

endmodule

`default_nettype wire
