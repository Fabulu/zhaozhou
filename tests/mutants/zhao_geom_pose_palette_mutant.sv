// zhao_geom_pose_palette_mutant.sv -- A DELIBERATELY BROKEN COPY. NOT SHIPPED.
//
// It exists to make one claim demonstrable rather than arguable: that the
// directed suite's section-1 differential is the ONLY detector for a
// bone-ROUTING fault in this block, because every counter the block owns
// balances perfectly while one is live.
//
// The one substantive change, in the R_IDLE accept arm:
//
//     b1_q <= b1_eff_c;      // the second matrix comes from bone1
//  -> b1_q <= b0_eff_c;      // MUTANT: it comes from bone0
//
// With that in place the walk is still exactly seven clocks, `wr_ready_o`
// still completes a beat in three, `vertices_served_o` still advances once per
// vertex, `bones_written_o` still counts every beat, and `bone_oob_o` /
// `bone_unset_o` still fire on exactly the references that deserve them --
// because none of them looks at WHICH matrix came back. A two-weight vertex is
// silently skinned with one bone twice, which is a plausible wrong picture with
// nothing to say so.
//
// INVERTED POLARITY: driven by
// tests/geometry/geom_pose_palette_mutant_control.cpp, which shares the
// directed suite's own stimulus header; the control PASSES when the routing
// differential FAILS and the counters stay balanced. Evidence about the
// instrument, not about the design.
//
// The module is RENAMED so a source-list mistake can never elaborate it in
// place of the real one, and it lives under tests/ where
// check_forbidden_sources.py will not find it in a production closure.
//
// REGENERATE IT if zhao_geom_pose_palette.sv changes shape: this is a copy, and
// a copy of an old version is a positive control for a block that no longer
// exists. tools/budget/mutant_copy_drift.py watches for exactly that.
// zhao_geom_pose_palette.sv — GEOM.POSE's bone-matrix palette store, and the
// latch that hands GEOM.SKIN two whole matrices with the vertex they belong to.
//
// Contract: design/contracts/GEOM.POSE.md ("holding decoded poses ... VRAM hot
// region + **M10K staging for the pose in use**"). This block is that staging.
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS AT ALL — a streamed producer meeting a random-access
// consumer, with nothing in between
// ---------------------------------------------------------------------------
// `zhao_geom_pose_decode` emits the palette ONE BONE PER BEAT on
// `out_valid_o / out_bone_o / out_m_o[12]`. `zhao_geom_skin` wants TWO WHOLE
// MATRICES presented WITH the vertex and latched on accept, selected by that
// vertex's `bone0` / `bone1` — and GEOM.SKIN.md is explicit that it will never
// do the selecting itself:
//
//     "The block does not index the palette. It receives two already-selected
//      matrices. b0 / b1 never enter it ... the bone indices live upstream."
//
// So a stream and a random access do not meet, and BEFORE THIS FILE nothing in
// the tree stored the palette in between. `zhao_geom_pose_cache` is NOT it: it
// is the {type, clip, frame, sub, gen} tuple cache that answers "which slot",
// and its own header says so in as many words — *"**The palettes themselves.**
// ... this block emits a VERDICT and a slot index, and the caller owns the
// store."* This block is that caller's store. Entry I10 of
// `zhao_console_core.sv` is the gap it closes.
//
// ---------------------------------------------------------------------------
// THE INTERFACE IS NOT INVENTED HERE — IT IS THE TWO ENDS, VERBATIM
// ---------------------------------------------------------------------------
// The write port is `zhao_geom_pose_decode`'s palette output, name for name:
// {valid, ready, bone, m[12]}. The vertex ports are `zhao_geom_vdecode`'s
// decoded-vertex output on one side and `zhao_geom_skin`'s vertex input on the
// other, name for name and width for width. Nothing is renamed, requantised or
// repacked anywhere in this file; the only thing added between the two is the
// pair of matrices, which is the whole job.
//
// **NO CONTRACT SPECIFIES THIS BLOCK'S INTERFACE, and that is recorded rather
// than glossed.** GEOM.POSE.md names the storage ("M10K staging for the pose in
// use") and GEOM.SKIN.md names the obligation ("the bone indices live
// upstream"), but neither writes down a port list, a walk length or a
// substitution rule for a bad bone index. Everything below that is not a direct
// quotation from one of those two files is an UNWRITTEN INTERFACE being
// implemented for the first time, by the shape of the two blocks it joins.
//
// ---------------------------------------------------------------------------
// MEMORY, NOT FLIP-FLOPS — and the width is chosen by the THROUGHPUT, not by
// taste
// ---------------------------------------------------------------------------
// A 32-bone palette is 32 x 12 x 32 = 12,288 bits. In flip-flops that is 12,288
// registers for one block's scratch — the misplaced state this campaign exists
// to remove, and the same arithmetic that put `zhao_geom_pose_decode`'s
// ancestor store in a memory. It goes in RAM.
//
// The only thing RAM costs is cycles, so the word width is set by the rate the
// consumer needs and by nothing else. GEOM.SKIN's owner-ruled demand is 120,000
// skinned vertex instances per 60 Hz frame; at 100 MHz that is 13.88 clocks per
// vertex, and at MUL_LANES=3 GEOM.SKIN's own issue interval is 12. So this
// block must deliver a vertex's two matrices in FEWER THAN 12 CYCLES or it
// silently becomes the bottleneck and the frame budget fails.
//
//   word width | words/matrix | reads/vertex | walk | verdict
//   -----------+--------------+--------------+------+---------------------------
//     32 bit   |     12       |     24       |  25  | FAILS (II 27 > 12)
//     64 bit   |      6       |     12       |  13  | FAILS (II 15 > 12)
//    128 bit   |      3       |      6       |   7  | II 9 — comfortable
//    384 bit   |      1       |      2       |   3  | 10 M10K, 64 deep -> MLAB
//
// **128 bits — one matrix ROW per word — is the first width that fits, and the
// widest one that stays memory-shaped.** The 384-bit form would be faster and
// buys nothing (GEOM.SKIN's 12 already dominates) while being 64 words deep,
// which is exactly the shape Quartus turns into LUT RAM.
//
// SHAPE ARITHMETIC, NOT A MEASUREMENT (owner ruling
// reports/OWNER-RULING-M10K-CEILINGS-20260918.md §3: *"Logical bits are still
// not physical M10Ks"*):
//
//     logical      96 words x 128 bits = 12,288 bits
//     M10K at x40  ceil(128/40) = 4 slices, 96 of 256 rows each
//     => 4 M10K, 0.7% of the device's 553
//
// Whether Quartus infers M10K here at all is UNVERIFIED — no `quartus_map` and
// no fit has been run on this file. `(* ramstyle = "M10K" *)` is a HINT and a
// hint is not an inference (`zhao_part_table.sv`'s header, same lesson). If
// Quartus declines and picks MLAB instead, the 4 M10K become roughly 21 MLABs
// of LUT RAM and this block costs ALMs it was written to avoid. That must be
// MEASURED before it is claimed.
//
// The M10K inference rules are followed deliberately, copied from
// `zhao_geom_pose_decode`'s ancestor store: no initializer, no reset branch
// touching the array, and the read happens ONLY inside the clocked process.
//
// ---------------------------------------------------------------------------
// ONE PALETTE, AND THE CONSTRAINT THAT COMES WITH IT
// ---------------------------------------------------------------------------
// This block holds ONE palette — "the pose in use", the contract's own phrase.
// It deliberately has no slot dimension:
//
//   * the slot index is `zhao_geom_pose_cache`'s `resp_slot_o`, and that block
//     is not composed in the console. A slot port here would close entry I10
//     by opening three new boundary ports, which is moving a gap, not closing
//     one;
//   * the 128-tuple cache the contract sizes is 128 x 12,288 = 1.5 Mbit ≈ 154
//     M10K, 28% of the device. `zhao_geom_pose_cache`'s header refuses to
//     settle that silently and so does this file. The contract puts those
//     tuples in the VRAM hot region, not in M10K.
//
// The consequence is a real obligation on the caller: **a new decode must be
// announced with `pal_begin_i`, and the vertex stream reading the previous pose
// must have drained before it starts.** Multi-slot is the natural extension and
// it is one address line — `addr = (slot * BONES + bone) * 3 + row` with the
// residency vector indexed the same way; nothing else in this file changes.
//
// **THE OBLIGATION IS NOT ENFORCED BY A COMMENT.** `pal_begin_i` clears the
// residency vector, so a vertex that arrives during a decode reads a bone that
// is not yet resident, gets the identity bind pose, and MOVES `bone_unset_o`.
// Misuse is therefore visible on a counter rather than silent — which is the
// only form of "the caller must not do that" this repository accepts.
//
// ---------------------------------------------------------------------------
// A BAD BONE INDEX IS SUBSTITUTED AND COUNTED, NEVER A WILD READ
// ---------------------------------------------------------------------------
// `zhao_geom_vdecode` emits SIXTEEN-bit bone indices (the vertex record's own
// field width); the palette has at most 32 entries. An index past the end is a
// MALFORMED ASSET, exactly like the `w0 > 64` the decoder already refuses.
//
// GEOM.POSE.md ratifies the response for the neighbouring case: *"Bad clip/
// frame ids: safe no-op palette (identity bind pose) + error counter, never a
// wild read"*. This block does the same thing one level down — identity bind
// pose, `bone_oob_o`, and the vertex still flows. Two alternatives were
// rejected: CLAMPING the index hands over some other bone's matrix, which is a
// silently wrong skin; DROPPING the vertex punches a hole in a stream whose
// consumers count beats.
//
// Note it is an INTERPRETATION of that clause, not a quotation of it — the
// contract's sentence is about clip and frame ids at the cache, not bone
// indices at the store. It is the closest ratified behaviour and it is applied
// deliberately; a contract amendment would be the right home for it.
//
// ---------------------------------------------------------------------------
// READ-DURING-WRITE
// ---------------------------------------------------------------------------
// The write walk and the read walk use the two ports of one simple-dual-port
// memory and can collide on an address. They cannot produce a wrong matrix,
// because a bone is not resident until its LAST row lands, so a read of a bone
// being written takes the identity substitution and discards `rd_data_q`
// entirely. That property rests on `pal_begin_i` preceding every re-decode —
// which is the caller obligation above — so `no_rw_check` is deliberately NOT
// asserted on the array. The attribute would be a claim about a schedule this
// block does not control.
module zhao_geom_pose_palette_mutant #(
    // The palette size. A power of two so `wr_bone_i` cannot encode an address
    // outside the store: a write is then structurally incapable of escaping,
    // which is why there is no write-out-of-range counter. An unfireable
    // counter is worse than no counter (CLAUDE.md).
    parameter int BONES = 32,
    parameter int SRCW  = 16
) (
    input  logic clk,
    input  logic rst_n,

    // ---- a new palette is about to be decoded ------------------------------
    // One cycle. Clears the residency vector; the stored matrices are NOT
    // touched (an M10K has no reset and a walk that cleared 96 words would be
    // 96 cycles of nothing). Residency is what makes the stale bytes
    // unreadable.
    input  logic                     pal_begin_i,

    // ---- write: the decoded palette, one bone per beat ---------------------
    // `zhao_geom_pose_decode`'s output port, name for name. The data must be
    // held while `wr_valid_i` is high and `wr_ready_o` is low — ordinary
    // ready/valid, and what that block already does.
    input  logic                     wr_valid_i,
    output logic                     wr_ready_o,
    input  logic [$clog2(BONES)-1:0] wr_bone_i,
    input  logic signed [31:0]       wr_m_i [12],

    // ---- vertex in: `zhao_geom_vdecode`'s decoded vertex -------------------
    input  logic                     v_valid_i,
    output logic                     v_ready_o,
    input  logic signed [31:0]       v_x_i,
    input  logic signed [31:0]       v_y_i,
    input  logic signed [31:0]       v_z_i,
    input  logic        [ 6:0]       v_w0_i,
    input  logic                     v_rigid_i,
    input  logic        [15:0]       v_bone0_i,
    input  logic        [15:0]       v_bone1_i,
    input  logic [SRCW-1:0]          v_src_id_i,
    // The PACKED BIND-SPACE NORMAL, carried through beside the vertex and
    // NOT interpreted here. Added 2026-09-19 for GEOM.SKIN.NORM, which needs
    // {normal, w0} AND the same two bone matrices the vertex was skinned
    // with. Before this the normal was valid at `zhao_geom_vdecode`'s output
    // and the matrices at this block's, several clocks and one lookup apart,
    // for what may not even be the same vertex -- joining them outside would
    // have been a composer pairing two things that move independently, and
    // the result is a normal skinned by another vertex's bones, which is a
    // lit vertex no output check can distinguish from a correct one.
    //
    // It is captured by THE SAME ENABLE, in THE SAME cycle, as `v_bone0_i`
    // and `v_bone1_i` (R_IDLE below), so the normal and the two matrices
    // that answer for it cannot come apart -- and it is held through R_HOLD
    // with the rest of the record, under the same law the R_HOLD comment
    // states. This block performs NO arithmetic on it.
    input  logic signed [ 7:0]       v_nx_i,
    input  logic signed [ 7:0]       v_ny_i,
    input  logic signed [ 7:0]       v_nz_i,

    // ---- vertex + its two matrices out: `zhao_geom_skin`'s vertex input ----
    output logic                     o_valid_o,
    input  logic                     o_ready_i,
    output logic signed [31:0]       o_x_o,
    output logic signed [31:0]       o_y_o,
    output logic signed [31:0]       o_z_o,
    output logic        [ 6:0]       o_w0_o,
    output logic                     o_rigid_o,
    output logic [SRCW-1:0]          o_src_id_o,
    // The same packed normal, beside the matrices that answer for it.
    // `zhao_geom_skin` does not take it and does not have to: it is a payload
    // this block carries, exactly as `o_rigid_o` is.
    output logic signed [ 7:0]       o_nx_o,
    output logic signed [ 7:0]       o_ny_o,
    output logic signed [ 7:0]       o_nz_o,
    output logic signed [31:0]       a_m_o [12],
    output logic signed [31:0]       b_m_o [12],

    // ---- evidence ----------------------------------------------------------
    // `bone_oob_o` and `bone_unset_o` count BONE REFERENCES, not vertices: a
    // vertex naming two bad bones moves the counter by two, and a rigid vertex
    // names its one bone twice. Stated because a counter whose unit is guessed
    // is a counter that gets quoted wrong.
    output logic [31:0]              vertices_served_o,
    output logic [31:0]              bones_written_o,
    output logic [31:0]              bone_oob_o,
    output logic [31:0]              bone_unset_o
);

  localparam int ELEMS = 12;
  localparam int ROWS  = 3;              // a 3x4 matrix is three four-element rows
  localparam int MEMW  = 128;            // one row: four s32 elements
  localparam int MEMD  = BONES * ROWS;
  localparam int ADDRW = $clog2(MEMD);
  localparam int BONEW = $clog2(BONES);

  // The bind pose. A VALUE, so it is a named editable constant and not a
  // literal buried in a mux (CLAUDE.md: never remove the owner's control).
  localparam logic signed [31:0] ONE_FX16 = 32'sd65536;

  // The walk, derived in the header. Six reads at one row per cycle, plus one
  // cycle for the last registered read to land.
  localparam int WALK = (2 * ROWS) + 1;  // 7

  // Quartus 17.0.2 needs an elaboration check inside `initial begin ... end`;
  // a bare module-scope `if` is a SYNTAX error there and lints clean in the
  // open toolchain (CLAUDE.md, Build note). Note also that `--lint-only` does
  // NOT run initial blocks, so a clean lint says nothing about these.
  //
  // (A comment line whose first word is the linter's own name is parsed as a
  // PRAGMA and rejected as `BADVLTPRAGMA`. Found here on the first lint, and
  // worth the parenthesis: it looks like a syntax error in the RTL.)
  initial begin
    if (BONES < 2 || BONES > 32) begin
      $fatal(1, "zhao_geom_pose_palette: BONES must be in 2..32; zhao_geom_pose_decode emits a 5-bit bone index");
    end
    if ((BONES & (BONES - 1)) != 0) begin
      $fatal(1, "zhao_geom_pose_palette: BONES must be a power of two, or wr_bone_i can address outside the store");
    end
  end

  // ---- the store ----------------------------------------------------------
  // Deliberately plain: no initializer, no reset, read only in the clocked
  // process. See the M10K note in the header.
  (* ramstyle = "M10K" *) logic [MEMW-1:0] pal_m [0:MEMD-1];

  logic [ADDRW-1:0] wr_addr_c, rd_addr_c;
  logic [MEMW-1:0]  wr_data_c;
  logic [MEMW-1:0]  rd_data_q;
  logic             wr_we_c;

  always_ff @(posedge clk) begin
    if (wr_we_c) pal_m[wr_addr_c] <= wr_data_c;
    rd_data_q <= pal_m[rd_addr_c];
  end

  // ---- residency ----------------------------------------------------------
  // One bit per bone, in flip-flops on purpose: it is cleared for every bone in
  // ONE cycle at `pal_begin_i`, which a memory cannot do, and BONES bits is 32.
  logic [BONES-1:0] resident_q;

  // ==========================================================================
  // THE WRITE WALK — three cycles per bone, no data register
  // ==========================================================================
  // `wr_ready_o` is high only on the third cycle, so the beat completes there.
  // The producer holds `wr_m_i` across all three (ready/valid), which is why
  // this side needs no 384-bit capture register. `wr_ready_o` is a function of
  // `wq` alone and NEVER of `wr_valid_i`: ready must not depend on valid.
  logic [1:0] wq;
  logic [3:0] wbase_c;

  assign wr_ready_o = (wq == 2'd2);
  assign wr_we_c    = wr_valid_i;
  assign wbase_c    = {wq, 2'b00};                       // 0, 4, 8
  assign wr_addr_c  = ADDRW'((32'(wr_bone_i) * ROWS) + 32'(wq));
  assign wr_data_c  = {wr_m_i[wbase_c + 4'd3], wr_m_i[wbase_c + 4'd2],
                       wr_m_i[wbase_c + 4'd1], wr_m_i[wbase_c]};

  // ==========================================================================
  // THE READ WALK — six reads, then hold
  // ==========================================================================
  typedef enum logic [1:0] {
    R_IDLE,
    R_WALK,
    R_HOLD
  } rstate_e;

  rstate_e rstate;
  logic [2:0] k;                    // 0 .. WALK-1
  logic [BONEW-1:0] b0_q, b1_q;
  logic sub_a_q, sub_b_q;

  assign v_ready_o = (rstate == R_IDLE);

  // ---- what the two offered bone indices resolve to ------------------------
  logic oob0_c, oob1_c, unset0_c, unset1_c;
  logic [BONEW-1:0] b0_eff_c, b1_eff_c;

  always_comb begin
    oob0_c   = (v_bone0_i >= 16'(BONES));
    oob1_c   = (v_bone1_i >= 16'(BONES));
    b0_eff_c = oob0_c ? {BONEW{1'b0}} : v_bone0_i[BONEW-1:0];
    b1_eff_c = oob1_c ? {BONEW{1'b0}} : v_bone1_i[BONEW-1:0];
    unset0_c = !oob0_c && !resident_q[b0_eff_c];
    unset1_c = !oob1_c && !resident_q[b1_eff_c];
  end

  // ---- the address this cycle ---------------------------------------------
  logic [BONEW-1:0] rb_c;
  logic [1:0]       rr_c;

  always_comb begin
    if (k < 3'd3) begin
      rb_c = b0_q;
      rr_c = k[1:0];
    end else if (k < 3'd6) begin
      rb_c = b1_q;
      rr_c = 2'(k - 3'd3);
    end else begin
      // The walk runs one cycle past the last issue so the registered read can
      // be captured. That extra address is never used, and it is CLAMPED
      // rather than allowed to walk off the end of the array —
      // `zhao_geom_pose_decode`'s S_PREAD precedent.
      rb_c = b0_q;
      rr_c = 2'd0;
    end
    rd_addr_c = ADDRW'((32'(rb_c) * ROWS) + 32'(rr_c));
  end

  // ---- the word landing this cycle ----------------------------------------
  logic [2:0]       w_c;        // 0..5, meaningful only while k > 0
  logic [1:0]       wrow_c;
  logic             sub_c;
  logic [3:0]       cbase_c;
  logic [MEMW-1:0]  ident_c, cap_c;

  always_comb begin
    w_c     = k - 3'd1;
    sub_c   = (w_c < 3'd3) ? sub_a_q : sub_b_q;
    wrow_c  = (w_c < 3'd3) ? w_c[1:0] : 2'(w_c - 3'd3);
    cbase_c = {wrow_c, 2'b00};                            // 0, 4, 8

    // The identity bind pose, one row at a time. Written out rather than
    // part-selected so the PACKING ORDER is visible: element 0 at bit 0.
    unique case (wrow_c)
      2'd0:    ident_c = {32'sd0,   32'sd0,   32'sd0,   ONE_FX16};
      2'd1:    ident_c = {32'sd0,   32'sd0,   ONE_FX16, 32'sd0  };
      default: ident_c = {32'sd0,   ONE_FX16, 32'sd0,   32'sd0  };
    endcase

    cap_c = sub_c ? ident_c : rd_data_q;
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rstate            <= R_IDLE;
      k                 <= '0;
      wq                <= '0;
      b0_q              <= '0;
      b1_q              <= '0;
      sub_a_q           <= 1'b0;
      sub_b_q           <= 1'b0;
      resident_q        <= '0;
      o_valid_o         <= 1'b0;
      o_x_o             <= '0;
      o_y_o             <= '0;
      o_z_o             <= '0;
      o_w0_o            <= '0;
      o_rigid_o         <= 1'b0;
      o_src_id_o        <= '0;
      o_nx_o            <= '0;
      o_ny_o            <= '0;
      o_nz_o            <= '0;
      vertices_served_o <= '0;
      bones_written_o   <= '0;
      bone_oob_o        <= '0;
      bone_unset_o      <= '0;
      for (int i = 0; i < ELEMS; i++) begin
        a_m_o[i] <= '0;
        b_m_o[i] <= '0;
      end
    end else begin
      // ---- write walk ------------------------------------------------------
      if (wr_valid_i) begin
        if (wq == 2'd2) begin
          wq <= 2'd0;
          resident_q[wr_bone_i] <= 1'b1;
          if (bones_written_o != 32'hFFFF_FFFF) begin
            bones_written_o <= bones_written_o + 32'd1;
          end
        end else begin
          wq <= wq + 2'd1;
        end
      end

      // `pal_begin_i` last: a begin in the same cycle as a completing write
      // means the write belonged to the OLD palette and its residency must not
      // survive. Ordering it after the write arm is the whole statement.
      if (pal_begin_i) resident_q <= '0;

      // ---- read walk -------------------------------------------------------
      unique case (rstate)
        R_IDLE: begin
          if (v_valid_i) begin
            o_x_o      <= v_x_i;
            o_y_o      <= v_y_i;
            o_z_o      <= v_z_i;
            o_w0_o     <= v_w0_i;
            o_rigid_o  <= v_rigid_i;
            o_src_id_o <= v_src_id_i;
            // Same enable, same cycle as the bone indices below. That is the
            // whole guarantee the port comment claims.
            o_nx_o     <= v_nx_i;
            o_ny_o     <= v_ny_i;
            o_nz_o     <= v_nz_i;

            b0_q    <= b0_eff_c;
            b1_q    <= b0_eff_c;  // MUTANT: bone1's matrix is fetched from bone0
            sub_a_q <= oob0_c || unset0_c;
            sub_b_q <= oob1_c || unset1_c;

            if (bone_oob_o != 32'hFFFF_FFFF) begin
              bone_oob_o <= bone_oob_o + 32'({1'b0, oob0_c}) + 32'({1'b0, oob1_c});
            end
            if (bone_unset_o != 32'hFFFF_FFFF) begin
              bone_unset_o <= bone_unset_o + 32'({1'b0, unset0_c}) + 32'({1'b0, unset1_c});
            end

            k      <= '0;
            rstate <= R_WALK;
          end
        end

        R_WALK: begin
          if (k > 3'd0) begin
            if (w_c < 3'd3) begin
              a_m_o[cbase_c]         <= $signed(cap_c[  0 +: 32]);
              a_m_o[cbase_c + 4'd1]  <= $signed(cap_c[ 32 +: 32]);
              a_m_o[cbase_c + 4'd2]  <= $signed(cap_c[ 64 +: 32]);
              a_m_o[cbase_c + 4'd3]  <= $signed(cap_c[ 96 +: 32]);
            end else begin
              b_m_o[cbase_c]         <= $signed(cap_c[  0 +: 32]);
              b_m_o[cbase_c + 4'd1]  <= $signed(cap_c[ 32 +: 32]);
              b_m_o[cbase_c + 4'd2]  <= $signed(cap_c[ 64 +: 32]);
              b_m_o[cbase_c + 4'd3]  <= $signed(cap_c[ 96 +: 32]);
            end
          end

          if (k == 3'(WALK - 1)) begin
            o_valid_o <= 1'b1;
            rstate    <= R_HOLD;
          end else begin
            k <= k + 3'd1;
          end
        end

        // The offer HOLDS. Nothing in this arm may touch `a_m_o`, `b_m_o` or
        // the vertex registers: a consumer that stalls must see the same
        // matrices beside the same vertex on every cycle of the stall. That is
        // the record-swap law of CLAUDE.md, and it is why `v_ready_o` is low
        // here rather than accepting a vertex into a buffer nothing holds.
        R_HOLD: begin
          if (o_ready_i) begin
            o_valid_o <= 1'b0;
            rstate    <= R_IDLE;
            if (vertices_served_o != 32'hFFFF_FFFF) begin
              vertices_served_o <= vertices_served_o + 32'd1;
            end
          end
        end

        default: rstate <= R_IDLE;
      endcase
    end
  end

endmodule : zhao_geom_pose_palette_mutant
