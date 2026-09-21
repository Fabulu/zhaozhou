// zhao_geom_bonesrc_latefetch_mutant.sv — A COMMITTED MUTANT. NOT PRODUCTION.
// Renamed so no source list can elaborate it by mistake.
//
// WHAT IT IS EVIDENCE ABOUT: the instrument, not the design. It exists so that
// `bone_prefetch_late_o` can be SEEN TO FIRE, and it is here because that
// counter is UNREACHABLE BY LEGAL STIMULUS — which CLAUDE.md names as exactly
// the case that needs a committed mutant rather than an argument that goes on
// forever.
//
// WHY IT IS UNREACHABLE. `zhao_geom_pose_decode` spends a measured 115.4
// cycles per bone and this block's refill takes seven, so no legal clip, bone
// count or backpressure can make the prefetch late. The guard is correct, and
// a correct guard makes its own detector dead to every legal input.
//
// THE FAULT MODELLED, and it is a REAL RISK rather than an invented one:
// **the page read is slower than the decode.** Today the two stores are filled
// from on-chip state in seven cycles. The producer entry I29 actually wants is
// a kind-8/kind-9 page read through MEM.GUARD to SDRAM, and a page read that
// misses can easily exceed 115 cycles. If that ever happens the decoder
// advances onto a bone whose data has not landed, latches the PREVIOUS bone's
// quaternion and rest translation, and emits a wrong palette with every
// handshake intact, `done_o` still rising and `palettes_decoded_o` still
// incrementing. `bone_prefetch_late_o` is the only thing that would say so.
//
// THE SUBSTANTIVE CHANGE, and nothing else in this file differs from
// `fpga/rtl/geometry/zhao_geom_bonesrc.sv`:
//
//   P_BODY's word cursor no longer advances every cycle. It advances once per
//   256 cycles, gated on a `stall_q` counter added for that purpose — so one
//   four-word body fill takes ~1,024 cycles against the decoder's ~115 per
//   bone, and the decoder is guaranteed to advance with a fill in flight.
//
// That is one behaviour changed plus the register it requires. The register is
// not a second mutation: without it the stall cannot be expressed at all, and
// naming it here is cheaper than a mutation nobody can read.
//
// ITS DRIVER'S POLARITY IS INVERTED: `geom_bonesrc_latefetch_mutant` PASSES
// WHEN THE COUNTER FIRES. The NEGATIVE control is separate and lives in
// `geom_bonesrc_directed` cases 3 and 4, where the same counter must read ZERO
// across two correct back-to-back palettes — so the pair proves the detector
// DISCRIMINATES rather than merely moves (owner ruling R95).
//
// DO NOT "FIX" THIS FILE. A green run here means the mutation is gone.
//
// DRIFT RE-VERIFIED 2026-09-21 (coordinator), and the verification is recorded
// rather than the gate silenced. `mutant_copy_drift` went RED here because
// production `zhao_geom_bonesrc.sv` was committed TWELVE MINUTES after this
// copy, at `10635433` ("POSEABI: re-measure after the repairs"). The tool was
// right on its own terms and it is deliberately provenance-based — CLAUDE.md:
// "the signal is provenance, not similarity" — so a later production commit is
// always worth the alarm.
//
// WHAT THAT COMMIT ACTUALLY CHANGED: nothing this copy contains. The whole
// diff `3b87e411..10635433` on that file is COMMENT-ONLY — an ALM row
// re-measured 829 -> 830 and 865 -> 866 registers, being the one flip-flop
// `started_q` added — and it lives in a header block this trimmed copy does
// not carry at all. Checked by filtering the diff to non-comment lines: zero.
//
// So the BODY is current and the mutation is intact. Re-committing this file
// is what clears the gate, and the note is here because the next reader
// deserves the evidence rather than an unexplained touch — a refreshed copy
// with no record of what was compared is the stale-copy trap with a newer
// timestamp.
module zhao_geom_bonesrc_latefetch_mutant #(
    parameter int MAX_BONES = 32,

    // 0 = SYNC_M10K (shipped), 1 = ASYNC_DERIVED, 2 = ASYNC_FLAT (R90's).
    // See the table in the header. This is a PRICING knob and the owner's:
    // it is not narrowed to the winner once the winner is known, because the
    // next device or the next decode rate re-opens the same question.
    parameter int SRC_STYLE = 0,

    // The bind convention of the ring format (zref_creature.hpp): rest
    // rotations are identity, so inv_rest is translate(-world_rest). 0 makes
    // the nine constants into stored bits again and is the extension path.
    parameter int INV_REST_RIGID = 1
) (
    input  logic clk,
    input  logic rst_n,

    // ---- fill, from the page reader ----------------------------------------
    // One 64-bit word at a time, which is what a page read delivers. `sel`
    // picks the kind-8 body (0) or the kind-9 clip frame (1); the two are
    // separate stores because the skeleton is per-TYPE and the quaternions are
    // per-FRAME, and merging them would refill the skeleton every frame.
    input  logic        fill_we_i,
    input  logic        fill_sel_i,
    input  logic [ 4:0] fill_bone_i,
    input  logic [ 3:0] fill_word_i,
    input  logic [63:0] fill_data_i,

    // ---- begin one palette --------------------------------------------------
    // The source owns `start_o`, not the caller: bone 0 must be on the wires
    // BEFORE the decoder's first S_FETCH, and only this block knows when its
    // prefetch has landed. `req_i` asks; `start_o` is the answer.
    input  logic        req_i,
    input  logic [ 5:0] bone_count_i,
    output logic        start_o,
    output logic        ready_o,

    // ---- the decoder's combinational source --------------------------------
    input  logic [ 4:0] bone_idx_i,
    output logic [ 4:0] bone_parent_o,
    output logic signed [31:0] bone_tx_o,
    output logic signed [31:0] bone_ty_o,
    output logic signed [31:0] bone_tz_o,
    output logic signed [15:0] quat_w_o,
    output logic signed [15:0] quat_x_o,
    output logic signed [15:0] quat_y_o,
    output logic signed [15:0] quat_z_o,
    output logic signed [31:0] inv_rest_o [12],

    // ---- detectors ----------------------------------------------------------
    output logic [31:0] bone_prefetch_late_o,
    output logic [31:0] bone_rest_nonrigid_o,
    output logic [31:0] bone_reserved_nz_o,
    output logic [31:0] bone_fills_o
  );

  // Q16.16 one, the diagonal of an identity mat3x4. Named rather than literal
  // so `check_case_labels` / a reader can see what 65536 is doing here.
  localparam logic signed [31:0] FX16_ONE_C = 32'sd65536;

  // Per-bone stored payload. The body record is 32 B = 256 bits (4 x 64) and
  // the clip lane is 8 B = 64 bits.
  localparam int BODY_BITS = 256;
  localparam int QUAT_BITS = 64;
  localparam int BODY_WORDS = BODY_BITS / 64;   // 4
  // R90's arrangement stores inv_rest in full: 5 + 96 + 64 + 384 = 549, padded
  // to 9 x 64 = 576 so the fill port is identical across all three styles and
  // the map rows compare like with like.
  localparam int FLAT_BITS = 576;

  initial begin
    if (MAX_BONES != 32)
      $fatal(1, "zhao_geom_bonesrc: MAX_BONES must be 32 (creature_rules 1.2)");
    if (SRC_STYLE < 0 || SRC_STYLE > 2)
      $fatal(1, "zhao_geom_bonesrc: SRC_STYLE must be 0, 1 or 2");
    if (INV_REST_RIGID != 1)
      $fatal(1, "zhao_geom_bonesrc: INV_REST_RIGID=0 needs the wide record; unimplemented");
  end

  // ==========================================================================
  // The decoded view of one bone, shared by every style.
  // ==========================================================================
  // The reserved fields of the record (+1 flags 1..7, +2 u16, +28 u32) and the
  // unused high bits of the parent byte are deliberately not read by the decode
  // path -- that is what "reserved" means. They are not unchecked, though:
  // `bone_reserved_nz_o` below counts any fill that sets one, at the door,
  // which is where `zhao_geom_vdecode` checks its own reserved bits too.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [BODY_BITS-1:0] body_sel;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [QUAT_BITS-1:0] quat_sel;

  // Field extraction, one place, so three styles cannot drift into three
  // layouts. Offsets are the record above, little-endian.
  assign bone_parent_o = body_sel[4:0];
  assign bone_tx_o = body_sel[63:32];
  assign bone_ty_o = body_sel[95:64];
  assign bone_tz_o = body_sel[127:96];

  // Unused under SRC_STYLE 2 on purpose: that style reads all twelve elements
  // out of the store instead of deriving three, which is the whole point of it.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [31:0] inv_tx_c, inv_ty_c, inv_tz_c;
  /* verilator lint_on UNUSEDSIGNAL */
  assign inv_tx_c = body_sel[159:128];
  assign inv_ty_c = body_sel[191:160];
  assign inv_tz_c = body_sel[223:192];

  assign quat_w_o = quat_sel[15:0];
  assign quat_x_o = quat_sel[31:16];
  assign quat_y_o = quat_sel[47:32];
  assign quat_z_o = quat_sel[63:48];

  // The twelve inverse-rest elements, flat, so the three styles drive ONE
  // signal and the unpack below is written once.
  //
  // STYLES 0 AND 1 rebuild it from the invariant: nine constants and one
  // negated vector (LEVER 1 in the header). STYLE 2 reads all twelve out of the
  // store, which is R90's literal arrangement and the thing being priced.
  //
  // THE FIRST DRAFT OF THIS PROBE GOT THAT WRONG AND THE MAP SAID SO. Style 2
  // stored 576 bits per bone and still DERIVED inv_rest, so Quartus pruned the
  // 320 bits nothing read and style 2 collapsed onto style 1 — two rows 19 ALM
  // apart, which looked like a result and was an artefact. A store is only
  // priced by what is READ out of it.
  logic [383:0] invrest_sel;

  generate
    if (SRC_STYLE != 2) begin : g_inv_rigid
      assign invrest_sel = {
          inv_tz_c, FX16_ONE_C, 32'sd0, 32'sd0,
          inv_ty_c, 32'sd0, FX16_ONE_C, 32'sd0,
          inv_tx_c, 32'sd0, 32'sd0, FX16_ONE_C
      };
    end
  endgenerate

  genvar gi;
  generate
    for (gi = 0; gi < 12; gi = gi + 1) begin : g_inv_unpack
      assign inv_rest_o[gi] = invrest_sel[gi*32 +: 32];
    end
  endgenerate

  // ==========================================================================
  // SRC_STYLE 0 — the shipped arrangement: synchronous stores behind a 2-deep
  // prefetch.
  // ==========================================================================
  generate
    if (SRC_STYLE == 0) begin : g_sync

      // The stores. Plain, per the M10K rules in the header.
      logic [63:0] body_mem [0:MAX_BONES*BODY_WORDS-1];
      logic [63:0] quat_mem [0:MAX_BONES-1];
      logic [63:0] body_rd_q, quat_rd_q;
      logic [$clog2(MAX_BONES*BODY_WORDS)-1:0] body_raddr;
      logic [$clog2(MAX_BONES)-1:0] quat_raddr;

      // The two held bones. d0 serves `i0_q`, d1 serves `i0_q + 1`.
      logic [BODY_BITS-1:0] d0_body, d1_body;
      logic [QUAT_BITS-1:0] d0_quat, d1_quat;
      logic [4:0] i0_q;

      // Fill sequencer over the RAM into d1.
      typedef enum logic [1:0] { P_IDLE, P_BODY, P_QUAT, P_LAND } pf_e;
      pf_e pf_q;
      logic [2:0] pf_w;
      logic [4:0] pf_bone;
      logic [BODY_BITS-1:0] pf_body;

      // ARMED ONLY WHILE A PALETTE IS ACTUALLY RUNNING, and this is a REPAIR
      // rather than a decoration. The first draft cleared `i0_q` to 0 when the
      // request arrived, while the decoder's `b` still held the LAST bone of
      // the previous palette -- so on the second palette, for the one cycle
      // between the prefetch landing and the decoder accepting `start_o`,
      // `bone_idx_i != i0_q` with the sequencer idle, and the jump detector
      // FIRED ON A CORRECT RUN. A detector that cries on healthy traffic is
      // worse than none: it trains a reader to discount it.
      //
      // Found by asking what legal stimulus reaches the counter, which is the
      // question a committed mutant exists to answer when the answer is "none"
      // -- here the answer was "a false one", which is the other thing that
      // question finds.
      //
      // `i0_q` now moves only when bone 0 actually lands, on the same edge the
      // decoder takes `start_i`, so the two are aligned by construction.
      logic started_q;
      logic [7:0] stall_q;   // MUTANT ONLY: the stall counter

      assign body_raddr = ($clog2(MAX_BONES*BODY_WORDS))'(pf_bone * BODY_WORDS + 32'(pf_w));
      assign quat_raddr = ($clog2(MAX_BONES))'(pf_bone);

      always_ff @(posedge clk) begin
        if (fill_we_i && !fill_sel_i)
          body_mem[{27'd0, fill_bone_i} * BODY_WORDS + {30'd0, fill_word_i[1:0]}] <= fill_data_i;
        if (fill_we_i &&  fill_sel_i) quat_mem[fill_bone_i] <= fill_data_i;
        body_rd_q <= body_mem[body_raddr];
        quat_rd_q <= quat_mem[quat_raddr];
      end

      // THE COMBINATIONAL SOURCE. A 2:1 over 320 bits, not a 32:1 over 549.
      assign body_sel = (bone_idx_i == i0_q) ? d0_body : d1_body;
      assign quat_sel = (bone_idx_i == i0_q) ? d0_quat : d1_quat;

      assign ready_o = (pf_q == P_IDLE);

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          pf_q <= P_IDLE; pf_w <= '0; pf_bone <= '0; i0_q <= '0;
          d0_body <= '0; d1_body <= '0; d0_quat <= '0; d1_quat <= '0;
          pf_body <= '0; start_o <= 1'b0; started_q <= 1'b0; stall_q <= '0;
          bone_prefetch_late_o <= '0;
        end else begin
          start_o <= 1'b0;

          // ARM ONE CYCLE AFTER `start_o`, NOT WITH IT. `start_o` is registered,
          // so it is high during cycle C and the decoder takes it at the END of
          // C — which is also when its `b` becomes 0. Arming in the same place
          // `start_o` is raised leaves one cycle in which `i0_q` is already 0
          // while `bone_idx_i` still holds the PREVIOUS palette's last bone, and
          // the jump detector fires on a perfectly correct second run.
          //
          // That is the SECOND off-by-one of exactly this shape in this block,
          // and the first one's repair is what made this one visible. Both were
          // caught by `geom_bonesrc_directed` case 4 — a second palette, back to
          // back — which one palette could never have shown.
          if (start_o) started_q <= 1'b1;

          if (started_q) begin
            // The decoder advanced. Shift and refill.
            if (bone_idx_i == i0_q + 5'd1) begin
              d0_body <= d1_body;
              d0_quat <= d1_quat;
              i0_q <= bone_idx_i;
              // LATE: the decoder has moved on and d1 was not yet filled, so
              // the bone it is about to latch is stale. Two operands, two
              // different loaders — see the header.
              if (pf_q != P_IDLE) bone_prefetch_late_o <= bone_prefetch_late_o + 32'd1;
              pf_q <= P_BODY; pf_w <= '0; pf_bone <= bone_idx_i + 5'd1;
            end else if (bone_idx_i != i0_q && pf_q == P_IDLE) begin
              // A jump the prefetch cannot cover. Counted, never silent.
              bone_prefetch_late_o <= bone_prefetch_late_o + 32'd1;
            end
          end

          unique case (pf_q)
            P_IDLE: begin
              if (req_i) begin
                pf_q <= P_BODY; pf_w <= '0; pf_bone <= '0; started_q <= 1'b0;
              end
            end
            P_BODY: begin
              // MUTATION: one word per 256 cycles instead of one per cycle.
              stall_q <= stall_q + 8'd1;
              if (stall_q == 8'hFF) begin
                if (pf_w > 3'd0) pf_body[({29'd0, pf_w} - 1) * 64 +: 64] <= body_rd_q;
                if (pf_w == 3'd4) begin pf_q <= P_QUAT; pf_w <= '0; end
                else pf_w <= pf_w + 3'd1;
              end
            end
            P_QUAT: begin
              if (pf_w == 3'd1) pf_q <= P_LAND;
              pf_w <= pf_w + 3'd1;
            end
            P_LAND: begin
              if (pf_bone == 5'd0) begin
                d0_body <= pf_body; d0_quat <= quat_rd_q;
                pf_q <= P_BODY; pf_w <= '0; pf_bone <= 5'd1;
              end else begin
                d1_body <= pf_body; d1_quat <= quat_rd_q;
                pf_q <= P_IDLE;
                // Bone 1 landing is the start: both held bones are resident,
                // so `i0_q` and the decoder's `b` become 0 on the SAME edge.
                if (pf_bone == 5'd1 && bone_count_i != 6'd0) begin
                  start_o <= 1'b1;
                  i0_q <= 5'd0;   // armed one cycle later, on `start_o` above
                end
              end
            end
            default: pf_q <= P_IDLE;
          endcase
        end
      end

    end else begin : g_async

      // ======================================================================
      // SRC_STYLE 1 and 2 — the asynchronous arrays. 1 stores the derived
      // record (320 bits/bone); 2 is R90's literal reading (576 bits/bone,
      // inv_rest carried in full). Both are read with the address the decoder
      // presents THIS cycle, which is what "combinational by contract" asks
      // for read naively.
      // ======================================================================
      localparam int AW = (SRC_STYLE == 2) ? FLAT_BITS : BODY_BITS;

      logic [AW-1:0]        abody [0:MAX_BONES-1];
      logic [QUAT_BITS-1:0] aquat [0:MAX_BONES-1];

      always_ff @(posedge clk) begin
        if (fill_we_i && !fill_sel_i) abody[fill_bone_i][{28'd0, fill_word_i} * 64 +: 64] <= fill_data_i;
        if (fill_we_i &&  fill_sel_i) aquat[fill_bone_i] <= fill_data_i;
      end

      // THE ASYNCHRONOUS READ. This is the line the price is about.
      assign body_sel = abody[bone_idx_i][BODY_BITS-1:0];
      assign quat_sel = aquat[bone_idx_i];

      // Style 2 only: the twelve elements come OUT OF THE STORE. Without this
      // the upper 320 bits are written and never read, Quartus deletes them,
      // and the row silently prices style 1 a second time.
      if (SRC_STYLE == 2) begin : g_inv_stored
        assign invrest_sel = abody[bone_idx_i][511:128];
      end

      assign ready_o = 1'b1;

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          start_o <= 1'b0;
          bone_prefetch_late_o <= '0;
        end else begin
          // Same guard as the sync style: a zero-bone request never starts.
          start_o <= req_i && (bone_count_i != 6'd0);
          // An async store is never late by construction; the counter exists so
          // the port list does not change with the style, and it is declared
          // dead here rather than left looking live.
          bone_prefetch_late_o <= bone_prefetch_late_o;
        end
      end

    end
  endgenerate

  // ==========================================================================
  // Detectors common to every style.
  // ==========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      bone_rest_nonrigid_o <= '0;
      bone_reserved_nz_o <= '0;
      bone_fills_o <= '0;
    end else begin
      if (fill_we_i) bone_fills_o <= bone_fills_o + 32'd1;
      // A body record whose word 0 arrives with RIGID_REST clear is claiming a
      // rest rotation this build cannot decode. Counted at the door rather than
      // decoded into a wrong palette.
      if (fill_we_i && !fill_sel_i && fill_word_i == 4'd0 && !fill_data_i[8])
        bone_rest_nonrigid_o <= bone_rest_nonrigid_o + 32'd1;
      // The record's reserved fields must be zero. Word 0 carries the parent
      // byte's high bits, flags 1..7 and the +2 u16; word 3 carries the +28
      // u32. A packer that starts using them without this build knowing is a
      // format change, and it moves a counter instead of being ignored.
      if (fill_we_i && !fill_sel_i && fill_word_i == 4'd0 &&
          (fill_data_i[7:5] != 3'd0 || fill_data_i[15:9] != 7'd0 || fill_data_i[31:16] != 16'd0))
        bone_reserved_nz_o <= bone_reserved_nz_o + 32'd1;
      if (fill_we_i && !fill_sel_i && fill_word_i == 4'd3 && fill_data_i[63:32] != 32'd0)
        bone_reserved_nz_o <= bone_reserved_nz_o + 32'd1;
    end
  end

endmodule : zhao_geom_bonesrc_latefetch_mutant
