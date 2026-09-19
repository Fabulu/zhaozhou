// zhao_cmd_exec.sv -- CMD.EXEC: the thing that turns a COMMAND into CONSOLE STATE.
//
// spec/commands.zidl:382 is the sentence this block exists to delete:
//
//     "NOTHING TURNS A COMMAND INTO A FRAME yet -- the composed bench plays the
//      frame ring by hand."
//
// It is the most-cited blocker in `tools/budget/completion_register.py`'s output:
// `zhao_console_core.sv` entries I14 (the projector's matrix bank), I30
// (SURFACE.STAMP's dispatch), I33 (PART.TABLE's per-frame load) and I7 (the
// collision plane) all say the same thing in four different vocabularies --
// "the port is real, the field is ratified, and no block in the tree produces
// it".
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS NOT. Read this before adding an arm.
// ---------------------------------------------------------------------------
// It is NOT a second validator. CMD.DECODER walks the same byte stream and
// reaches the ratified verdict; this block takes that verdict as an INPUT and
// never re-derives it. Writing a CRC, a length law or an opcode-legality test
// here would be a second implementation of the ratified arithmetic -- the exact
// failure `design/contracts/GEOM.LIGHT.md` line 118 names and the one that
// shipped twice in this tree already. The division is:
//
//     CMD.DECODER owns the VERDICT.   CMD.EXEC owns the PAYLOAD.
//
// It is NOT allowed to invent a command. Two of the four entries above are left
// OPEN on purpose and the reason is recorded here rather than in a run folder:
//
//   I33  PART.TABLE's per-frame load. `spec/commands.zidl` contains NO command
//        that carries a particle species descriptor. The core's own I33 text is
//        explicit that species CONTENTS are owner DATA
//        (`reference/include/zref/zref_particle.hpp`: "there is no species
//        table ... That is a DATA/ABI question and it is properly the
//        owner's"). Building a load path here would mean choosing a wire layout
//        for a record the ABI does not define, which is exactly the hidden
//        contract this file must not contain. It needs an ABI ruling first.
//
//   I7   PART.COLLIDE's plane. Same shape, shorter story: no ratified command
//        carries a plane. `SetEnvironment 0x0311` is the nearest thing in the
//        opcode space and it carries sun, ambient, tint and fog -- no geometry
//        -- and it is `reserved`, not `implemented`, so it has no execution
//        semantics to borrow even if it did.
//
// ---------------------------------------------------------------------------
// THE VERDICT TENSION, AND WHY THIS BLOCK DOES NOT NEED THE SPECULATIVE ESCAPE
// ---------------------------------------------------------------------------
// `zhao_cmd_decoder.sv` lines 70-75 state the problem exactly:
//
//     "THE CONSUMER MUST NOT RETIRE THESE until decode_done_o with
//      decode_error_o == ZH_ABI_OK. The payload CRC (4) and the count laws (9)
//      cannot conclude until the last byte ... The contract argues this choice
//      and records the rejected alternative (buffer and replay), which would
//      re-introduce exactly the storage this block exists to avoid."
//
// So an executor may not act before the last byte, and may not hold the packet
// to wait for it. The obvious way out is to act speculatively and abandon the
// frame on a bad verdict. THAT IS NOT WHAT THIS BLOCK DOES, and the reason is
// worth the paragraph, because the escape hatch is weaker than it looks:
// `SurfaceStamp` writes the 64x64 surface sheet, which is PERSISTENT -- scars
// survive the frame. "Abandon the frame" does not un-scar a sheet. A
// speculative stamp on a packet that then fails its CRC is a permanent
// corruption with a clean instrument beside it, which is the failure mode
// CLAUDE.md is mostly about.
//
// THE RESOLUTION IS TO NOTICE THAT THE DECODER'S OBJECTION IS ABOUT BYTES AND
// NOT ABOUT EFFECTS. The storage the decoder refuses is O(packet) -- a frame
// slot is FRAME_SLOT_BYTES = 1,048,576 bytes, 8 Mbit, and CMD.DMA's 1.97 Mbit
// blit buffer already made the composed shell unsynthesizable once. The storage
// this block needs is O(console state), because it stages the EFFECT of each
// command rather than the command:
//
//   * SetView is IDEMPOTENT STATE keyed by view. Two SetViews for one view
//     collapse -- the last one wins, which is what the ABI already means. So it
//     stages into a shadow of the bank it writes: 2 views x 16 words x 32 bits
//     = 1,024 bits, and NO packet length can make it larger.
//   * SurfaceStamp is an EVENT and events do not collapse, so it stages into a
//     ring of STAMP_Q entries x 208 bits. That IS bounded by the packet, so the
//     bound is DECLARED (STAMP_Q, an owner knob) and a packet that exceeds it
//     is REFUSED WHOLE and counted on `stamp_overflow_o` -- never half-applied.
//
// Total at the defaults: 1,024 + 8*208 = 2,688 bits. Against 8,388,608. Three
// orders of magnitude and a constant, versus a length. That is the whole
// argument, and it means this block obeys the decoder's contract LITERALLY --
// not one console-visible bit moves before `verdict_valid_i` -- rather than
// obeying a softer restatement of it.
//
// ASSUMPTION, stated so it is cheap to reverse: the cost above is paid in
// flip-flops on a device already at 97% ALM occupancy. If the shadow bank turns
// out to be the wrong side of the budget, the reversal is to move `sv_mat` into
// an M10K (it is a one-write/one-read array with no concurrent access) rather
// than to weaken the commit rule. `trade-alms-for-m10k` is the standing ruling
// and this array is exactly its shape.
//
// ---------------------------------------------------------------------------
// COMMIT ORDER IS VIEWS, THEN STAMPS -- and that is a REORDERING
// ---------------------------------------------------------------------------
// Records arrive interleaved and commit grouped. That is safe ONLY because the
// two targets are disjoint machines: the projector's matrix bank and the
// surface sheet share no state and no ordering law. A future arm whose command
// interacts with either of them -- anything that reads a matrix, or a second
// writer of the sheet -- BREAKS this and needs one ordered queue instead of two
// staging structures. Said here because the next person will add an arm.
//
// ---------------------------------------------------------------------------
// WHAT IS CARRIED, AND WHAT IS DECLARED NOT CARRIED
// ---------------------------------------------------------------------------
// Every byte offset below comes from `fpga/rtl/generated/zhao_abi_pkg.sv`
// (`ZHAO_SET_VIEW_OFF_*`, `ZHAO_SURFACE_STAMP_OFF_*`). None is written by hand,
// for CMD.DECODER's own reason: "layouts come EXCLUSIVELY from the generated
// package". The two exceptions are `tx` and `ty` INSIDE `transform2fx`, which
// the generator emits as one struct offset; they are `+0` and `+4` by that
// struct's declaration order (`fx16 tx; fx16 ty;`) and an elaboration guard
// below checks `$bits(zhao_transform2fx_t)` so a layout change cannot pass
// silently.
//
// SetView 0x0010 carries seven fields. This block consumes TWO of them and the
// other five have NO PORT ON THIS CONSOLE -- that is a gap, not a decision:
//   CARRIED   view_id           -> `proj_cfg_view_o` (see the refusal below)
//   CARRIED   view_projection   -> `proj_cfg_addr_o` 0..15, one word per clock
//   NOT       viewport_id       -- the bank's viewport rect is at cfg addresses
//                                  16/17 and SetView carries no rect, only an
//                                  id; the id -> rect table is video_rules.md's
//                                  and is not in the ABI.
//   NOT       flags[1:0]        -- depth_profile. The frozen ruling of
//                                  2026-08-31 assigns it and `zhao_project_core`
//                                  has no depth-profile port to put it on.
//   NOT       pixel_error       -- MEASURE.GOVERNOR is not composed.
//   NOT       geometry_tokens   -- MEASURE.TOKENS is not composed.
//   NOT       fragment_tokens   -- likewise.
//
// `view_id` IS A u8 AND THE BANK HAS TWO VIEWS. It is NOT masked down to one
// bit: a `view_id` the bank cannot address is REFUSED, the record is not
// staged, and `view_range_refused_o` counts it. Masking would execute a
// command the game did not issue, silently, which is the worse of the two
// failures by a distance.
//
// SurfaceStamp 0x0210 likewise:
//   CARRIED   patch, operation, tag, strength, transform.tx, transform.ty,
//             radius, ring_width, and the RECORD HEADER's source_id.
//   NOT       brush             -- `zhao_surface_stamp.sv` S5 says it plainly:
//                                  "`brush` IS NOT AN INPUT ... NOTHING in this
//                                  tree defines a brush page's format". There
//                                  is no port to drive.
//   NOT       transform.r00..r11 -- the stamp is a disc or an annulus and a
//                                  circle is rotation-invariant; the consumer
//                                  has no rotation port. This is the one place
//                                  a dropped field is provably inert.
//   NARROWED  source_id is u32 on the wire and `cmd_src_id_i` is 16 bits. The
//             low half is carried; a nonzero HIGH half is counted on
//             `stamp_src_truncated_o` rather than dropped in silence.
//
// ---------------------------------------------------------------------------
// FRAMING
// ---------------------------------------------------------------------------
// The packet walk is the same one `zhao_shell_top_v2`'s glue 3 already ships
// and `tests/shell/shell_golden.cpp` already pins: header [0,36), records
// [36, len-4), trailing payload CRC word [len-4, len). `pkt_len_m4_q` is
// registered for the timing reason that file measured on the 2026-08-24 composed
// fit, and the same argument applies unchanged -- a value consulted no earlier
// than 36 accepted bytes into a packet cannot observe a one-cycle lag.
//
// A MALFORMED RECORD CANNOT HURT, and this is the second half of the commit
// rule doing real work. A garbage `record_bytes` walks this framer into
// nonsense, stages nonsense, and is then thrown away, because the same packet
// cannot pass CMD.DECODER. Staging is allowed to be wrong; committing is not.
// ---------------------------------------------------------------------------
module zhao_cmd_exec
  import zhao_abi_pkg::*;
#(
    // Staged SurfaceStamp capacity. An owner knob, and the one number in this
    // block that can refuse a legal packet -- see `stamp_overflow_o`.
    parameter int unsigned STAMP_Q = 8
) (
    input  logic clk,
    input  logic rst_n,

    // ---- the sealed packet byte stream, forked from CMD.DMA ----------------
    // The SAME stream CMD.DECODER sees. The fork is an AND of the two readies
    // in the composer; a consumer that cannot say "not yet" drops bytes the
    // other consumer has already counted.
    //
    // `pkt_fork_ready_i` IS THE AND, HANDED BACK, and it is not decoration --
    // it is the difference between this block working and this block reading
    // every field one byte out of place. The obvious `take = pkt_valid_i &&
    // pkt_ready_o` is wrong on a FORK: it says "I could have taken it", and the
    // byte only actually moves when the OTHER consumer could too. CMD.DECODER
    // holds its ready low for one cycle in S_CHECK, at packet offset 36 --
    // which is the first byte of the record region -- so a walk driven by the
    // local ready runs exactly one byte ahead for the whole packet and captures
    // nothing at any offset it believes in. Measured, not reasoned: the first
    // build of this block staged zero records with every counter reading a
    // confident zero and the decoder beside it reporting four records walked.
    // `zhao_shell_top_v2`'s glue 3 has the same shape from the other side
    // (`pkt_ready = ... && cmd_pkt_ready_i`) and its comment says why: a tap
    // "would let this framer advance past a byte the other consumer never saw".
    input  logic        pkt_valid_i,
    output logic        pkt_ready_o,
    input  logic        pkt_fork_ready_i,
    input  logic [ 7:0] pkt_byte_i,
    input  logic [31:0] pkt_len_i,

    // ---- the verdict, from CMD.DECODER -------------------------------------
    // NOT re-derived here. `verdict_valid_i` is that block's `decode_done_o`
    // and `verdict_error_i` its `decode_error_o`.
    input  logic        verdict_valid_i,
    input  logic [ 7:0] verdict_error_i,

    // ---- I14: the shared projector's matrix bank ---------------------------
    // Address map is `zhao_project_core.sv`'s: 0..15 matrix, 16 viewport
    // origin, 17 viewport extent. This block writes 0..15 and nothing else.
    // `proj_cfg_ready_i` IS A REFUSAL, not a stall of the bank. The bank has a
    // second writer -- the console's host cfg port, which owns addresses 16
    // and 17 (the viewport rect) because SetView carries an id and not a rect.
    // The composer gives that writer the cycle and drops this one's ready; the
    // word is then re-presented unchanged. Nothing is dropped on either side,
    // so the composer needs no conflict counter -- and a counter it could not
    // fire would have been worse than none.
    output logic        proj_cfg_we_o,
    input  logic        proj_cfg_ready_i,
    output logic        proj_cfg_view_o,
    output logic [ 4:0] proj_cfg_addr_o,
    output logic [31:0] proj_cfg_data_o,

    // ---- I30: SURFACE.STAMP's dispatch, the ratified fields only -----------
    output logic               stamp_valid_o,
    input  logic               stamp_ready_i,
    output logic        [31:0] stamp_patch_o,
    output logic        [ 7:0] stamp_operation_o,
    output logic        [ 7:0] stamp_tag_o,
    output logic        [15:0] stamp_strength_o,
    output logic signed [31:0] stamp_tx_o,
    output logic signed [31:0] stamp_ty_o,
    output logic signed [31:0] stamp_radius_o,
    output logic signed [31:0] stamp_ring_width_o,
    output logic        [15:0] stamp_src_id_o,

    // ---- evidence ----------------------------------------------------------
    // Every one of these is fired by a directed case in
    // tests/command/cmd_exec_directed.cpp. None is asserted zero without one.
    output logic [31:0] packets_committed_o,
    output logic [31:0] packets_abandoned_o,
    output logic [31:0] views_written_o,
    output logic [31:0] stamps_issued_o,
    output logic [31:0] stamp_overflow_o,
    output logic [31:0] view_range_refused_o,
    output logic [31:0] stamp_src_truncated_o,
    output logic [31:0] unsupported_o
);

  // Saturating, like every other counter in the composed core: a wrapped
  // counter reads low, and low is the flattering direction.
  `define ZHAO_EXEC_INC(c) if (c != 32'hFFFF_FFFF) c <= c + 32'd1

  // ---- ABI geometry, all of it from the generated package ------------------
  localparam int unsigned SV_VIEW_ID = ZHAO_SET_VIEW_OFF_VIEW_ID;
  localparam int unsigned SV_MAT_LO  = ZHAO_SET_VIEW_OFF_VIEW_PROJECTION;
  localparam int unsigned SV_MAT_HI  = ZHAO_SET_VIEW_OFF_PIXEL_ERROR;  // exclusive

  localparam int unsigned OFF_PATCH = ZHAO_SURFACE_STAMP_OFF_PATCH;
  localparam int unsigned OFF_OPER  = ZHAO_SURFACE_STAMP_OFF_OPERATION;
  localparam int unsigned OFF_TAG   = ZHAO_SURFACE_STAMP_OFF_TAG;
  localparam int unsigned OFF_STR   = ZHAO_SURFACE_STAMP_OFF_STRENGTH;
  // transform2fx declares `fx16 tx; fx16 ty;` first; the generator emits one
  // offset for the struct. The $bits guard below is what keeps these honest.
  localparam int unsigned OFF_TX   = ZHAO_SURFACE_STAMP_OFF_TRANSFORM;
  localparam int unsigned OFF_TY   = ZHAO_SURFACE_STAMP_OFF_TRANSFORM + 4;
  localparam int unsigned OFF_RAD  = ZHAO_SURFACE_STAMP_OFF_RADIUS;
  localparam int unsigned OFF_RING = ZHAO_SURFACE_STAMP_OFF_RING_WIDTH;

  // The record header's source_id, by the same rule.
  localparam int unsigned RH_SRC = ZHAO_SURFACE_STAMP_OFF_H_SOURCE_ID;

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`; a
  // bare module-scope `if` is a syntax error there however clean the lint
  // (CLAUDE.md, "Verilator lint-clean is not Quartus-synthesizable"). And
  // `--lint-only` does not run this block, so it is a build-time guard and is
  // not claimed as a tested one. NOT wrapped in `synthesis translate_off`: the
  // whole point of the Quartus form is that quartus_map evaluates it, and the
  // pragma would put it back exactly where the rule says it must not be.
  initial begin
    if ($bits(zhao_transform2fx_t) != 24 * 8)
      $fatal(1, "zhao_cmd_exec: transform2fx is no longer 24 B; OFF_TX/OFF_TY are stale");
    if (ZHAO_SET_VIEW_BYTES != 96)
      $fatal(1, "zhao_cmd_exec: SetView record size moved; re-read the offsets");
    if (ZHAO_SURFACE_STAMP_BYTES != 64)
      $fatal(1, "zhao_cmd_exec: SurfaceStamp record size moved; re-read the offsets");
    if ((SV_MAT_HI - SV_MAT_LO) != 64)
      $fatal(1, "zhao_cmd_exec: mat4fx is no longer 16 words");
    if (STAMP_Q < 2)
      $fatal(1, "zhao_cmd_exec: STAMP_Q must be >= 2 (the pointers need a bit)");
  end

  // ---- the packet walk (glue 3's framing, port for port) -------------------
  logic [31:0] pos;
  logic [31:0] pkt_len_m4_q;
  logic [15:0] rpos, rlen, r_op;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) pkt_len_m4_q <= 32'd0;
    else        pkt_len_m4_q <= pkt_len_i - 32'd4;
  end

  logic take, in_rec_region, rec_done;
  // NOT `pkt_valid_i && pkt_ready_o` -- see the port comment. This is the byte
  // that MOVED, not the byte this block would have accepted.
  assign take          = pkt_valid_i && pkt_fork_ready_i;
  assign in_rec_region = (pos >= 32'd36) && (pos < pkt_len_m4_q);
  // `rlen` is complete from record offset 4 onward, which is exactly the guard
  // glue 3 uses and for the same reason.
  assign rec_done = take && in_rec_region && (rpos >= 16'd4)
                 && ((rpos + 16'd1) == rlen);

  // ---- SetView staging: a shadow of the bank, not of the packet ------------
  logic [31:0] sv_mat [0:1][0:15];
  logic [ 1:0] sv_dirty;
  logic        sv_view;   // the bank view this record targets
  logic        sv_ok;     // ... and whether view_id could address it at all
  logic [23:0] wacc;      // little-endian word assembler: three bytes of
                          // history, like the decoder's pcrc_hist -- the
                          // fourth byte is compared straight off the wire

  logic        sv_in_mat;
  logic [ 5:0] mo;
  assign sv_in_mat = (r_op == ZHAO_OP_SET_VIEW)
                  && (rpos >= 16'(SV_MAT_LO)) && (rpos < 16'(SV_MAT_HI));
  assign mo = 6'(rpos - 16'(SV_MAT_LO));

  // ---- SurfaceStamp staging: a ring of EFFECTS ----------------------------
  localparam int unsigned SQ_PATCH_LO = 0;
  localparam int unsigned SQ_OPER_LO  = 32;
  localparam int unsigned SQ_TAG_LO   = 40;
  localparam int unsigned SQ_STR_LO   = 48;
  localparam int unsigned SQ_TX_LO    = 64;
  localparam int unsigned SQ_TY_LO    = 96;
  localparam int unsigned SQ_RAD_LO   = 128;
  localparam int unsigned SQ_RING_LO  = 160;
  localparam int unsigned SQ_SRC_LO   = 192;
  localparam int unsigned STAMP_W     = 208;
  localparam int unsigned SQW         = $clog2(STAMP_Q);

  logic [31:0] ss_patch, ss_tx, ss_ty, ss_rad, ss_ring;
  logic [ 7:0] ss_oper, ss_tag;
  logic [15:0] ss_str, ss_src;
  logic        ss_src_hi_nz;   // the dropped half of source_id was not zero

  logic [STAMP_W-1:0] sq [0:STAMP_Q-1];
  logic [SQW:0]       sq_wp, sq_rp;
  logic [SQW:0]       sq_occ;
  logic               sq_full;
  assign sq_occ  = sq_wp - sq_rp;
  assign sq_full = (sq_occ >= (SQW+1)'(STAMP_Q));

  logic [STAMP_W-1:0] sq_head;
  assign stamp_patch_o      = sq_head[SQ_PATCH_LO +: 32];
  assign stamp_operation_o  = sq_head[SQ_OPER_LO  +: 8];
  assign stamp_tag_o        = sq_head[SQ_TAG_LO   +: 8];
  assign stamp_strength_o   = sq_head[SQ_STR_LO   +: 16];
  assign stamp_tx_o         = $signed(sq_head[SQ_TX_LO   +: 32]);
  assign stamp_ty_o         = $signed(sq_head[SQ_TY_LO   +: 32]);
  assign stamp_radius_o     = $signed(sq_head[SQ_RAD_LO  +: 32]);
  assign stamp_ring_width_o = $signed(sq_head[SQ_RING_LO +: 32]);
  assign stamp_src_id_o     = sq_head[SQ_SRC_LO   +: 16];

  // A packet that overflowed the stamp ring is POISONED: it is refused WHOLE at
  // its own verdict, even if the verdict is ZH_ABI_OK. Half of a frame's scars
  // is not a degraded frame, it is a wrong one.
  logic poisoned;

  // ---- commit ------------------------------------------------------------
  typedef enum logic [1:0] {
    EX_STAGE,  // walking a packet; NOTHING leaves this block
    EX_CFG,    // draining the view shadow into the matrix bank
    EX_STAMP   // draining the stamp ring into SURFACE.STAMP
  } ex_e;
  ex_e st;

  logic       cv;   // view being committed
  logic [3:0] cw;   // matrix word being committed

  // The byte stream is accepted only while staging. During a commit the fork's
  // AND holds CMD.DMA off, which is what makes "nothing escapes early" a
  // STRUCTURAL property rather than a timing argument.
  assign pkt_ready_o = (st == EX_STAGE);

  integer vi, wi;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      pos <= 32'd0; rpos <= 16'd0; rlen <= 16'd0; r_op <= 16'd0;
      for (vi = 0; vi < 2; vi = vi + 1)
        for (wi = 0; wi < 16; wi = wi + 1) sv_mat[vi][wi] <= 32'd0;
      sv_dirty <= 2'd0; sv_view <= 1'b0; sv_ok <= 1'b0; wacc <= 24'd0;
      ss_patch <= 32'd0; ss_tx <= 32'd0; ss_ty <= 32'd0;
      ss_rad <= 32'd0; ss_ring <= 32'd0;
      ss_oper <= 8'd0; ss_tag <= 8'd0; ss_str <= 16'd0; ss_src <= 16'd0;
      ss_src_hi_nz <= 1'b0;
      sq_wp <= '0; sq_rp <= '0; sq_head <= '0;
      poisoned <= 1'b0;
      st <= EX_STAGE; cv <= 1'b0; cw <= 4'd0;
      proj_cfg_we_o <= 1'b0; proj_cfg_view_o <= 1'b0;
      proj_cfg_addr_o <= 5'd0; proj_cfg_data_o <= 32'd0;
      stamp_valid_o <= 1'b0;
      packets_committed_o <= 32'd0; packets_abandoned_o <= 32'd0;
      views_written_o <= 32'd0; stamps_issued_o <= 32'd0;
      stamp_overflow_o <= 32'd0; view_range_refused_o <= 32'd0;
      stamp_src_truncated_o <= 32'd0; unsupported_o <= 32'd0;
    end else begin
      proj_cfg_we_o <= 1'b0;   // a write is one cycle wide, always

      unique case (st)

        // ------------------------------------------------------------------
        // STAGE -- walk the packet, capture the fields, touch nothing outside
        // ------------------------------------------------------------------
        EX_STAGE: begin
          if (take) begin
            if (in_rec_region) begin
              // record header
              if      (rpos == 16'd0) r_op[ 7:0] <= pkt_byte_i;
              else if (rpos == 16'd1) r_op[15:8] <= pkt_byte_i;
              else if (rpos == 16'd2) rlen[ 7:0] <= pkt_byte_i;
              else if (rpos == 16'd3) rlen[15:8] <= pkt_byte_i;

              // source_id: the low half is carried, the high half is WATCHED
              if ((rpos >= 16'(RH_SRC)) && (rpos < 16'(RH_SRC + 2)))
                ss_src <= {pkt_byte_i, ss_src[15:8]};
              if ((rpos >= 16'(RH_SRC + 2)) && (rpos < 16'(RH_SRC + 4)) && (pkt_byte_i != 8'd0))
                ss_src_hi_nz <= 1'b1;

              // ---- SetView ------------------------------------------------
              if (r_op == ZHAO_OP_SET_VIEW) begin
                if (rpos == 16'(SV_VIEW_ID)) begin
                  // A u8 against a two-view bank. Refuse, never mask.
                  sv_view <= pkt_byte_i[0];
                  sv_ok   <= (pkt_byte_i < 8'd2);
                  if (pkt_byte_i >= 8'd2) begin
                    `ZHAO_EXEC_INC(view_range_refused_o);
                  end
                end
                if (sv_in_mat) begin
                  wacc <= {pkt_byte_i, wacc[23:8]};
                  if ((mo[1:0] == 2'd3) && sv_ok)
                    sv_mat[sv_view][mo[5:2]] <= {pkt_byte_i, wacc};
                end
              end

              // ---- SurfaceStamp -------------------------------------------
              if (r_op == ZHAO_OP_SURFACE_STAMP) begin
                if ((rpos >= 16'(OFF_PATCH)) && (rpos < 16'(OFF_PATCH + 4)))
                  ss_patch <= {pkt_byte_i, ss_patch[31:8]};
                if (rpos == 16'(OFF_OPER)) ss_oper <= pkt_byte_i;
                if (rpos == 16'(OFF_TAG))  ss_tag  <= pkt_byte_i;
                if ((rpos >= 16'(OFF_STR)) && (rpos < 16'(OFF_STR + 2)))
                  ss_str <= {pkt_byte_i, ss_str[15:8]};
                if ((rpos >= 16'(OFF_TX)) && (rpos < 16'(OFF_TX + 4)))
                  ss_tx <= {pkt_byte_i, ss_tx[31:8]};
                if ((rpos >= 16'(OFF_TY)) && (rpos < 16'(OFF_TY + 4)))
                  ss_ty <= {pkt_byte_i, ss_ty[31:8]};
                if ((rpos >= 16'(OFF_RAD)) && (rpos < 16'(OFF_RAD + 4)))
                  ss_rad <= {pkt_byte_i, ss_rad[31:8]};
                if ((rpos >= 16'(OFF_RING)) && (rpos < 16'(OFF_RING + 4)))
                  ss_ring <= {pkt_byte_i, ss_ring[31:8]};
              end

              // ---- the record ends ----------------------------------------
              // Every field above lands at or before the last field byte of a
              // well-formed record (83 of 96 for SetView, 59 of 64 for
              // SurfaceStamp), so no capture collides with this instant.
              if (rec_done) begin
                if (r_op == ZHAO_OP_SET_VIEW) begin
                  if (sv_ok) sv_dirty[sv_view] <= 1'b1;
                end else if (r_op == ZHAO_OP_SURFACE_STAMP) begin
                  if (ss_src_hi_nz) begin
                    `ZHAO_EXEC_INC(stamp_src_truncated_o);
                  end
                  if (sq_full) begin
                    // Declared capacity, refused whole, counted.
                    poisoned <= 1'b1;
                    `ZHAO_EXEC_INC(stamp_overflow_o);
                  end else begin
                    sq[sq_wp[SQW-1:0]] <= {ss_src, ss_ring, ss_rad, ss_ty,
                                           ss_tx, ss_str, ss_tag, ss_oper,
                                           ss_patch};
                    sq_wp <= sq_wp + (SQW+1)'(1);
                  end
                end else if (zhao_opcode_record_bytes(r_op) != 32'd0) begin
                  // A record the ABI defines and this block has no arm for.
                  // Counted rather than narrated, so the distance between the
                  // command surface and the executor is a NUMBER.
                  `ZHAO_EXEC_INC(unsupported_o);
                end
                // An opcode the ABI does not define is CMD.DECODER's to
                // report, and this block deliberately has no opinion on it.

                rpos         <= 16'd0;
                ss_src_hi_nz <= 1'b0;
              end else begin
                rpos <= rpos + 16'd1;
              end
            end

            if ((pos + 32'd1) >= pkt_len_i) begin
              pos          <= 32'd0;
              rpos         <= 16'd0;
              ss_src_hi_nz <= 1'b0;
            end else begin
              pos <= pos + 32'd1;
            end
          end

          // ---- the verdict ---------------------------------------------
          // It cannot arrive in any other state: `pkt_ready_o` is low outside
          // EX_STAGE, so CMD.DECODER cannot reach the last byte of a packet
          // while this block is committing the previous one.
          if (verdict_valid_i) begin
            if ((verdict_error_i == ZH_ABI_OK) && !poisoned) begin
              st <= EX_CFG;
              cv <= 1'b0;
              cw <= 4'd0;
            end else begin
              `ZHAO_EXEC_INC(packets_abandoned_o);
              sv_dirty <= 2'd0;
              sq_wp    <= '0;
              sq_rp    <= '0;
              poisoned <= 1'b0;
            end
          end
        end

        // ------------------------------------------------------------------
        // COMMIT phase 1 -- the view shadow into the matrix bank
        // ------------------------------------------------------------------
        // ISSUE, THEN RETIRE -- one word per two clocks, and the second clock
        // is the point. `proj_cfg_ready_i` exists because the matrix bank has
        // a SECOND writer: the console's own `proj_cfg_*_i` host port, which
        // owns cfg addresses 16 and 17 (the viewport rect) that no ratified
        // command carries. Without a handshake the composer would have to drop
        // one of the two writes and count it, and a dropped matrix word is a
        // silently wrong camera. With it the merge is LOSSLESS in both
        // directions: the host wins the cycle, this block re-presents.
        //
        // The bubble costs 64 clocks per frame at two full views. That is not
        // a throughput question by any measure that matters here.
        EX_CFG: begin
          if (proj_cfg_we_o) begin
            if (proj_cfg_ready_i) begin
              // Accepted. Retire this word and drop `we` for one cycle.
              if (cw == 4'd15) begin
                cw           <= 4'd0;
                sv_dirty[cv] <= 1'b0;
                `ZHAO_EXEC_INC(views_written_o);
                if (cv) st <= EX_STAMP;
                else    cv <= 1'b1;
              end else begin
                cw <= cw + 4'd1;
              end
            end else begin
              proj_cfg_we_o <= 1'b1;  // refused: hold the identical word
            end
          end else if (sv_dirty[cv]) begin
            proj_cfg_we_o   <= 1'b1;
            proj_cfg_view_o <= cv;
            proj_cfg_addr_o <= {1'b0, cw};
            proj_cfg_data_o <= sv_mat[cv][cw];
          end else begin
            if (cv) st <= EX_STAMP;
            else    cv <= 1'b1;
          end
        end

        // ------------------------------------------------------------------
        // COMMIT phase 2 -- the stamp ring into SURFACE.STAMP
        // ------------------------------------------------------------------
        // One stamp per two clocks, not one per clock: the head is registered
        // and re-presented after each accept. Throughput was not the question
        // this block answers -- a frame's stamps are a handful and the sheet
        // walk behind them is 4,096 texels each.
        EX_STAMP: begin
          if (stamp_valid_o) begin
            if (stamp_ready_i) begin
              stamp_valid_o <= 1'b0;
              sq_rp         <= sq_rp + (SQW+1)'(1);
              `ZHAO_EXEC_INC(stamps_issued_o);
            end
          end else if (sq_occ != '0) begin
            sq_head       <= sq[sq_rp[SQW-1:0]];
            stamp_valid_o <= 1'b1;
          end else begin
            `ZHAO_EXEC_INC(packets_committed_o);
            sq_wp <= '0;
            sq_rp <= '0;
            st    <= EX_STAGE;
          end
        end

        default: st <= EX_STAGE;
      endcase
    end
  end

  `undef ZHAO_EXEC_INC

endmodule : zhao_cmd_exec
