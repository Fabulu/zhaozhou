// zhao_geom_meshfetch.sv — the geometry front end's entry point.
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK IS THE ONE THAT MATTERS
// ---------------------------------------------------------------------------
// `reports/DOCKET.md` D22: `zhao_shell_top.sv` instantiates ONE of the twenty
// blocks in `fpga/rtl/geometry`, and the console renders from screen-space
// triangles handed to it from outside. `tools/design/compose_order.py` reads
// the composition order back from the ledger's own declared edges and puts this
// block at position 1 with nothing built behind it.
//
// So nothing downstream of here can be fed by anything but a harness until this
// exists. That is the whole reason it was written before the other eighteen.
//
// ---------------------------------------------------------------------------
// IT DRIVES TWO EXISTING BLOCKS AND REIMPLEMENTS NEITHER
// ---------------------------------------------------------------------------
// The frustum test is `zhao_geom_cull.sv` and the ladder is `zhao_geom_lod.sv`,
// both already differentially proved against their oracles. This block owns
// exactly the three things `zref::MeshFetch` owns and no more:
//
//   * descriptor validation, in the SAME refusal order as the oracle;
//   * the local-bound -> world-bound transform;
//   * the refusal taxonomy.
//
// Both services share one handshake shape -- `ready_o` high means a `tick_i`
// will be accepted, `valid_o` is a later pulse -- so the two wait states below
// are the same state twice rather than two designs.
//
// ---------------------------------------------------------------------------
// THE ARITHMETIC MATCHES THE ORACLE'S WIDTHS ON PURPOSE
// ---------------------------------------------------------------------------
// `zref::MeshFetch` accumulates the row sums in `int64_t` and scales the radius
// in `int64_t`. This block uses 64 bits in both places for the same reason a
// differential exists at all: if the oracle can overflow on a hostile input,
// the RTL must overflow IDENTICALLY, or the two disagree exactly where it is
// hardest to notice. Matching the oracle is the contract; matching a wider
// ideal is not.
//
// The radius SATURATES at 32 bits (contract, Q formats): a wrapped bound is a
// SMALL bound, and small deletes geometry.
//
// ---------------------------------------------------------------------------
// THIS BLOCK CANNOT PASS MEM.GUARD TODAY, AND THE GUARD IS RIGHT
// ---------------------------------------------------------------------------
// `zhao_mem_guard.sv`'s client case is, in full:
//
//     ZHAO_CLIENT_SCANOUT:  read, inside the framebuffer slots
//     ZHAO_CLIENT_BLIT_DMA: write, inside the granted slot
//     ZHAO_CLIENT_ENGINE0:  write, inside the granted slot
//     default:              pass_ok = 1'b0    -- "ENGINE1 and DEBUG own nothing"
//
// Every region it knows is a FRAMEBUFFER region. A meshlet descriptor lives in
// asset memory, so there is no client identity under which this read is
// admitted -- the request below is correctly formed and would be denied.
//
// AND THAT IS CORRECT BEHAVIOUR, not a gap. `spec/memory_rules.md` §5 says so
// in as many words:
//
//     Phase 2 allocates exactly [the two FB regions] ... Later phases extend
//     the map (texture/terrain/particle pools per the charter allocator);
//     Phase 2 ships ONLY the two FB regions -- everything else is a violation
//     by construction.
//
// So the guard is doing exactly what it was specified to do, and this block
// needs the PHASE-3 REGION MAP rather than a fix. That the guard admits no
// region outside the two FB slots is not an assumption made here -- it is
// proved.
// ENFORCED-BY: tests/formal/mem_guard_no_escape.sby That distinction decides who
// owns the next move: extending the map is a charter-allocator decision with a
// formal proof attached (`tests/formal/mem_guard_no_escape.sby`), not a line
// this file may add.
//
// `j_client_i` is therefore an INPUT -- the caller declares the identity these
// reads use -- and `guard_denied_o` counts the denial, which makes that counter
// the measurement for whether the map has been extended yet.
//
// ENFORCED-BY: tests/geometry/geom_meshfetch_rtl_directed.cpp:main
`default_nettype none

module zhao_geom_meshfetch
  import zhao_pkg::*;
(
    input var logic clk,
    input var logic rst_n,

    // ---- job in --------------------------------------------------------------
    // The contract's job packet names `instance_transform_id`. Resolving an id
    // to a matrix is a PALETTE LOOKUP, and this block does not own it -- the
    // same split that keeps the vertex and index streams in GEOM.VDECODE. The
    // caller presents the resolved 3x4 directly.
    input  var logic              j_valid_i,
    output var logic              j_ready_o,
    input  var logic [15:0]       j_instance_id_i,
    input  var logic [26:0]       j_desc_addr_i,     // 64-byte aligned
    input  var logic [7:0]        j_format_i,        // the format this reader speaks
    input  var logic [15:0]       j_generation_i,    // expected, for stale handles
    input  var logic [1:0]        j_active_mask_i,   // the CALLER drives this
    input  var logic signed [31:0] j_xform_i [12],   // row-major 3x4, fx16
    // The memory-client identity for the descriptor read. An input because no
    // existing client owns an asset region -- see the header.
    input  var zhao_client_e      j_client_i,

    // ---- MEM.GUARD read client ------------------------------------------------
    // The shape `zhao_scanout_fetch.sv` established and `zhao_mem_guard.sv` is
    // written against (`scan_ok = !req.write`). One request at len=64 returns
    // exactly eight 64-bit beats, which is why the descriptor is 64-byte
    // aligned: the alignment ruling and the burst shape are one fact.
    output var zhao_guard_req_t   guard_req_o,
    input  var zhao_guard_rsp_t   guard_rsp_i,
    input  var logic              beat_valid_i,
    input  var logic [63:0]       beat_data_i,
    input  var logic              beat_last_i,
    // The CRC over bytes 0..59, folded by the caller's `zhao_crc32c_fold`.
    // Wired in rather than folded here: that block is the one implementation
    // and a second would be a second law, which is the same refusal that keeps
    // the cull and the ladder outside this file.
    input  var logic              crc_ok_i,

    // ---- the cull service (zhao_geom_cull) ------------------------------------
    output var logic              cull_tick_o,
    output var logic [1:0]        cull_active_o,
    output var logic signed [31:0] cull_cx_o,
    output var logic signed [31:0] cull_cy_o,
    output var logic signed [31:0] cull_cz_o,
    output var logic signed [31:0] cull_radius_o,
    input  var logic              cull_ready_i,
    input  var logic              cull_valid_i,
    input  var logic [1:0]        cull_vis_i,
    input  var logic              cull_reject_i,

    // ---- result out -----------------------------------------------------------
    output var logic              r_valid_o,
    input  var logic              r_ready_i,
    output var logic [15:0]       r_instance_id_o,
    output var logic [1:0]        r_visible_mask_o,
    output var logic [31:0]       r_vertex_offset_o,
    output var logic [31:0]       r_index_offset_o,
    output var logic [7:0]        r_vertex_count_o,
    output var logic [7:0]        r_triangle_count_o,
    output var logic [15:0]       r_material_id_o,
    output var logic [7:0]        r_flags_o,

    // ---- evidence -------------------------------------------------------------
    output var logic [31:0]       meshlets_considered_o,
    output var logic [31:0]       culled_all_cameras_o,
    output var logic [31:0]       descriptors_fetched_o,
    // A denied read is NOT a refused descriptor: nothing was read, so there is
    // nothing to distrust. It is counted apart for the same reason rejected is
    // counted apart from refused -- three different failures with three
    // different causes, and one counter for all of them would name none.
    output var logic [31:0]       guard_denied_o,
    // One per row of the contract's refusal table, in the oracle's enum order:
    // format, crc, generation, vertex_count, triangle_count, reserved, zero_bound.
    output var logic [31:0]       refused_o [7]
);

  localparam int unsigned MAX_VERTEX   = 64;
  localparam int unsigned MAX_TRIANGLE = 126;

  // ------------------------------------------------------------------------
  // THE GUARD'S VERDICT ARRIVES ONE CYCLE AFTER THE ACCEPT (found by D22
  // tread 10, 2026-09-06)
  //
  // This block used to require `guard_rsp_i.ready && guard_rsp_i.ok` IN ONE
  // CYCLE. Against `zhao_mem_guard` that condition can never be true:
  //
  //     rsp.ready = !fwd_active;      // a LEVEL: "the forwarding stage is free"
  //     rsp_ok_q  <= 1'b1;            // a PULSE, the cycle AFTER the accept
  //
  // and the accept is what sets `fwd_active`. So at the accepting cycle ready
  // is 1 and ok is 0; at the verdict cycle ok is 1 and ready is 0. A passing
  // request therefore looked like a DENIAL with no violation flag -- silently,
  // with `guard_denied_o` staying at zero, because the guard only raises
  // `violation` when it actually refuses.
  //
  // It went unseen because every bench PLAYED the guard, and a played
  // responder answers ready and ok together. That is precisely the class of
  // fault D22's staircase exists to find: the harness modelled a protocol the
  // real block does not implement, and everything measured against it agreed.
  //
  // S_VERD is the accepting cycle's successor. The request drops when it is
  // entered -- the guard has already latched it -- and exactly one of ok and
  // violation pulses there.
  //
  // THIS IS A DIVERGENCE, NOT AN AMBIGUITY. Two other guard clients already
  // had it right and had it NAMED: `zhao_raster_fbwrite` waits in `W_VERD`
  // and `zhao_debug_frameblit` in `B_GUARD_VERDICT`, and fbwrite's header
  // even quotes the guard line -- "zhao_mem_guard.sv:186  rsp.ok = rsp_ok_q;
  // verdict 1 cycle after accept". Four clients, two protocols, and the two
  // that were wrong are exactly the two whose memory was played.
  //
  // HONEST LIMIT: the identical defect is repaired here, but D22 tread 10 only
  // exercises GEOM.ASSETFETCH against the real guard. GEOM.MESHFETCH has no
  // client identity the memory law admits yet -- the arbiter tags by slot
  // index, so slot 3 is ENGINE1 and slot 4 is DEBUG, which owns nothing -- so
  // this half of the fix is REASONED, not measured.
  typedef enum logic [2:0] {
    S_IDLE, S_REQ, S_VERD, S_FILL, S_BOUND, S_CULL, S_WAIT, S_EMIT
  } state_e;
  state_e st_q;

  // The descriptor, as eight 64-bit beats. Held whole because the CRC covers
  // bytes 0..59 and a field checked before the last beat lands would be
  // checked against a descriptor that does not exist yet.
  logic [63:0] d_q [8];
  logic [2:0]  beat_q;

  // The CRC verdict is LATCHED when the last beat lands, and this is a formal
  // finding rather than a precaution. `crc_ok_i` is a live input; recomputing
  // the refusal from it combinationally meant the block could decide "not
  // refused", enter S_CULL, and then see the input drop -- offering a refused
  // descriptor's bound to the cull service. BMC found it at step 15 of a
  // properly reset design.
  //
  // The fold completes with the last beat, so that edge is exactly when the
  // verdict becomes meaningful and exactly when it should stop being a wire.
  logic               crc_ok_q;
  logic [15:0]        inst_q;
  logic [7:0]         fmt_q;
  logic [15:0]        gen_q;
  logic [1:0]         act_q;
  logic signed [31:0] xf_q [12];

  // ---- descriptor field views ----------------------------------------------
  // Little-endian, so this block and the oracle agree about what a descriptor
  // IS before they can disagree about what it MEANS.
  function automatic logic [7:0] db(input int unsigned i);
    db = d_q[i / 8][8 * (i % 8) +: 8];
  endfunction
  function automatic logic [15:0] dh(input int unsigned i);
    dh = {db(i + 1), db(i)};
  endfunction
  function automatic logic [31:0] dw(input int unsigned i);
    dw = {db(i + 3), db(i + 2), db(i + 1), db(i)};
  endfunction

  // ---- validation, in the ORACLE'S ORDER -----------------------------------
  // Format first: an unknown format means the byte layout itself is not agreed,
  // so even the CRC window may be in the wrong place. CRC second: a descriptor
  // that fails it "is not trustworthy in any field", so naming a later field
  // would name a value that means nothing.
  //
  // The CRC itself is NOT computed here -- `zhao_crc32c_fold` is the one
  // implementation and a second would be a second law. `crc_ok_i` is folded by
  // the caller's instance of it over bytes 0..59 as the beats arrive.
  logic        resv_nz_c;
  logic [2:0]  refusal_c;      // 0 = none, else 1..7 in enum order
  logic        refused_c;

  always_comb begin
    resv_nz_c = 1'b0;
    for (int unsigned i = 36; i < 60; i++) if (db(i) != 8'd0) resv_nz_c = 1'b1;

    if      (db(0) != fmt_q)              refusal_c = 3'd1;  // format
    else if (!crc_ok_q)                   refusal_c = 3'd2;  // crc
    else if (dh(32) != gen_q)             refusal_c = 3'd3;  // generation
    else if (db(2) > 8'(MAX_VERTEX))      refusal_c = 3'd4;
    else if (db(3) > 8'(MAX_TRIANGLE))    refusal_c = 3'd5;
    else if (resv_nz_c)                   refusal_c = 3'd6;  // reserved
    else if (dw(20) == 32'd0 && db(3) != 8'd0)
                                          refusal_c = 3'd7;  // zero bound
    else                                  refusal_c = 3'd0;
    refused_c = (refusal_c != 3'd0);
  end

  // ---- the bound transform, one multiply per cycle -------------------------
  // Nine products for the centre and one for the radius, sequenced through a
  // single 32x32 multiplier. This block runs at MESHLET rate, not vertex rate,
  // so ten clocks per meshlet is free and a parallel form would spend nine
  // multipliers to save nothing that matters.
  logic [3:0]        mi_q;                 // 0..8 centre, 9 radius, 10 done
  logic signed [63:0] acc_q [3];
  logic signed [31:0] wc_q [3];
  logic [31:0]        wr_q;
  logic [31:0]        max_abs_q;

  function automatic logic signed [31:0] rescale16(input logic signed [63:0] v);
    // fx16 round-half-up, ties toward +infinity (qformats 3/4). The contract
    // pins this for the world centre; without it an RTL that truncated would
    // agree with the oracle everywhere except on ties.
    rescale16 = 32'((v + 64'sd32768) >>> 16);
  endfunction

  function automatic logic [31:0] abs32(input logic signed [31:0] v);
    abs32 = v[31] ? 32'(-v) : 32'(v);
  endfunction

  // `mi_q` is 4 bits, so a bare `mi_q / 3` asks the divider for 32 and Verilator
  // says so. The lane and column are named instead of derived inline twice.
  logic [1:0]         mlane_c, mcol_c;
  logic signed [31:0] mul_a_c, mul_b_c;
  logic signed [63:0] mul_p_c;
  always_comb begin
    mlane_c = 2'(int'(mi_q) / 3);
    mcol_c  = 2'(int'(mi_q) % 3);
    mul_a_c = xf_q[int'(mlane_c) * 4 + int'(mcol_c)];
    mul_b_c = $signed(dw(8 + 4 * int'(mcol_c)));   // bound_centre lane
    mul_p_c = 64'(mul_a_c) * 64'(mul_b_c);
  end

  // radius scale, saturating at 32 bits
  logic signed [63:0] rad_scaled_c;
  logic [31:0]        rad_sat_c;
  always_comb begin
    rad_scaled_c = (64'(dw(20)) * 64'(max_abs_q) + 64'sd65535) >>> 16;
    rad_sat_c    = (rad_scaled_c > 64'sh0000_0000_FFFF_FFFF) ? 32'hFFFF_FFFF
                                                             : 32'(rad_scaled_c);
  end

  assign j_ready_o = (st_q == S_IDLE);

  assign guard_req_o.valid  = (st_q == S_REQ);
  assign guard_req_o.write  = 1'b0;
  assign guard_req_o.client = j_client_i;
  assign guard_req_o.addr   = j_desc_addr_i;
  assign guard_req_o.len    = 7'd64;
  assign guard_req_o.be     = {64{1'b1}};

  assign cull_tick_o   = (st_q == S_CULL) && cull_ready_i;
  assign cull_active_o = act_q;
  assign cull_cx_o     = wc_q[0];
  assign cull_cy_o     = wc_q[1];
  assign cull_cz_o     = wc_q[2];
  assign cull_radius_o = $signed(wr_q);

  assign r_valid_o          = (st_q == S_EMIT);
  assign r_instance_id_o    = inst_q;
  assign r_vertex_offset_o  = dw(24);
  assign r_index_offset_o   = dw(28);
  assign r_vertex_count_o   = db(2);
  assign r_triangle_count_o = db(3);
  assign r_material_id_o    = dh(4);
  assign r_flags_o          = db(1);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q                  <= S_IDLE;
      beat_q                <= '0;
      mi_q                  <= '0;
      crc_ok_q              <= 1'b0;
      r_visible_mask_o      <= '0;
      meshlets_considered_o <= '0;
      culled_all_cameras_o  <= '0;
      descriptors_fetched_o <= '0;
      guard_denied_o        <= '0;
      for (int i = 0; i < 7; i++) refused_o[i] <= '0;
    end else begin
      case (st_q)
        S_IDLE: begin
          if (j_valid_i) begin
            inst_q <= j_instance_id_i;
            fmt_q  <= j_format_i;
            gen_q  <= j_generation_i;
            act_q  <= j_active_mask_i;
            for (int i = 0; i < 12; i++) xf_q[i] <= j_xform_i[i];
            meshlets_considered_o <= meshlets_considered_o + 32'd1;
            beat_q <= '0;
            st_q   <= S_REQ;
          end
        end

        S_REQ: if (guard_rsp_i.ready) begin
          st_q <= S_VERD;   // accepted; the verdict is the NEXT cycle
        end

        S_VERD: begin
          // A guard denial is not a descriptor fault: nothing was read, so
          // there is nothing to refuse. The job ends and is counted as its own
          // kind of failure. Today EVERY read lands here -- the guard owns no
          // asset region, see the header -- so this counter is also the
          // measurement that says whether that gap has been closed.
          if (guard_rsp_i.ok) begin
            st_q <= S_FILL;
          end else if (guard_rsp_i.violation) begin
            guard_denied_o <= guard_denied_o + 32'd1;
            st_q           <= S_IDLE;
          end
        end

        S_FILL: if (beat_valid_i) begin
          d_q[beat_q] <= beat_data_i;
          beat_q      <= beat_q + 3'd1;
          if (beat_last_i) begin
            crc_ok_q <= crc_ok_i;
            descriptors_fetched_o <= descriptors_fetched_o + 32'd1;
            mi_q       <= '0;
            acc_q[0]   <= '0;
            acc_q[1]   <= '0;
            acc_q[2]   <= '0;
            max_abs_q  <= '0;
            st_q       <= S_BOUND;
          end
        end

        S_BOUND: begin
          // Validation is combinational over the whole descriptor and is
          // consulted here, once the last beat has landed.
          if (refused_c) begin
            refused_o[refusal_c - 1] <= refused_o[refusal_c - 1] + 32'd1;
            st_q <= S_IDLE;
          end else if (mi_q < 4'd9) begin
            acc_q[mlane_c] <= acc_q[mlane_c] + mul_p_c;
            if (abs32(mul_a_c) > max_abs_q) max_abs_q <= abs32(mul_a_c);
            mi_q <= mi_q + 4'd1;
          end else begin
            // the translation column, then one rescale per lane
            for (int r = 0; r < 3; r++)
              wc_q[r] <= rescale16(acc_q[r] + (64'(xf_q[r * 4 + 3]) <<< 16));
            wr_q <= rad_sat_c;
            st_q <= S_CULL;
          end
        end

        S_CULL: if (cull_ready_i) st_q <= S_WAIT;

        S_WAIT: if (cull_valid_i) begin
          r_visible_mask_o <= cull_vis_i;
          if (cull_reject_i) begin
            // REJECTED IS NOT REFUSED. A meshlet with visible_mask 0 is not
            // emitted at all, and it is counted in its OWN counter -- a
            // refusal counter that also carried invisibility would report
            // corruption every time the camera turned around.
            culled_all_cameras_o <= culled_all_cameras_o + 32'd1;
            st_q <= S_IDLE;
          end else begin
            st_q <= S_EMIT;
          end
        end

        S_EMIT: if (r_ready_i) st_q <= S_IDLE;

        default: st_q <= S_IDLE;
      endcase
    end
  end

`ifdef FORMAL
  // -------------------------------------------------------------------------
  // THE REFUSAL LAW, AS A PROPERTY
  // -------------------------------------------------------------------------
  // The contract's refusal table ends "emits no meshlet". A directed test shows
  // that on the descriptors someone thought of; this shows it on every input
  // sequence the block can be given, including adversarial beat patterns and a
  // guard that answers arbitrarily.
  //
  // It is the property most worth proving rather than testing, because the
  // failure it excludes is silent: a meshlet emitted from a descriptor whose
  // CRC failed carries offsets nothing downstream can distrust, and it looks
  // exactly like correct geometry in the wrong place.
  //
  // ENFORCED-BY: tests/formal/geom_meshfetch_refuse.sby
  //
  // THE PRECONDITION IS THE DESIGN HAVING BEEN RESET, and saying so is not a
  // formality. The first version guarded only on `rst_n`, and BMC returned a
  // counterexample at step 1: with no reset ever applied the engine is free to
  // START in S_CULL holding a refused descriptor. That is a true statement
  // about an unreachable state, and the fix is to state the precondition rather
  // than to weaken the property -- the same shape as every other harness in
  // this directory.
  logic f_past_valid = 1'b0;
  always_ff @(posedge clk) f_past_valid <= 1'b1;

  // reset is applied at time zero, and once released it stays released
  always_ff @(posedge clk) begin
    if (!f_past_valid) assume (!rst_n);
    if (f_past_valid && $past(rst_n)) assume (rst_n);
  end

  // ---- SELF-ASSERTING SCOPE GUARD (ledger rule V19) ----------------------
  // The refusal law is proven at bmc depth 24, which is the full path from job
  // accept through the guard grant, eight beats, validation, the nine bound
  // steps and the cull handshake to the one state where it could fail. This
  // PINS that window: raising `depth` makes the assertion FIRE, so the bound
  // cannot silently change meaning. A deeper proof has to re-justify what the
  // extra cycles are covering rather than re-run with a bigger number and
  // believe more was proved than was.
  logic [6:0] f_scope_cyc = 7'd0;
  always_ff @(posedge clk) begin
    if (f_scope_cyc != 7'h7F) f_scope_cyc <= f_scope_cyc + 7'd1;
  end
  always_comb begin
    a_scope_bmc_window : assert (f_scope_cyc <= 7'd24);
  end

  always @(posedge clk) begin
    if (f_past_valid && rst_n) begin
      // 1. A result is never emitted for a descriptor that failed validation.
      assert (!(r_valid_o && refused_c));
      // 2. Nor is a refused descriptor's bound ever offered to the cull
      //    service. "Not trustworthy in ANY field" includes the bound, and a
      //    culled-but-refused meshlet would still spend the service's cycles.
      assert (!(cull_tick_o && refused_c));
    end
  end
`endif

endmodule : zhao_geom_meshfetch

`default_nettype wire
