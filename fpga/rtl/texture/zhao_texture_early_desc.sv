// zhao_texture_early_desc.sv — the typed early descriptor bank, §5.2/§5.4.
//
// ---------------------------------------------------------------------------
// WHAT THIS IS
// ---------------------------------------------------------------------------
// One row per owner slot, written ONCE on the owner's admission handshake and
// read synchronously when PERSPUV presents that owner's result. It replaces the
// scattered live early tables the expander and Mosaic currently read from
// whatever stage happens to be holding a value.
//
// The field list is the Decrufter brief's §5.2 verbatim, and its 119 bits are a
// READER INVENTORY -- what today's consumers actually read -- not a promise
// that all 119 survive optimisation. Two of them are compatibility boundaries
// that the brief names explicitly and this file names again, because a boundary
// that is only recorded in a review gets implemented as a fact:
//
//   * `aux_context` is the CURRENT compatibility contract, not free bits.
//   * `mosaic_material_a/b` retain today's behaviour of taking material choices
//     from colour bytes. That is not a claim it is the final material ABI.
//   * `binding_selector` is RESERVED for the intended resolver contract. With no
//     binding consumer in the build, synthesis may prune it. That is a recorded
//     limitation, not binding support.
//
// ---------------------------------------------------------------------------
// THE HOLD LAW, WHICH IS NOT OPTIONAL HERE
// ---------------------------------------------------------------------------
// §5.3: "The RAM output must obey the same hold law established by D0, or be
// captured into reserved output storage before it can be overwritten."
//
// D0 was `zhao_texture_metajoin`'s read register updating unconditionally, so it
// tracked whatever address was OFFERED while the stage downstream held the
// previous response -- yielding response A's data with response B's metadata,
// every counter balancing. Reproduced in `tests/texture/metajoin_seam_directed`
// and repaired by gating the register on an actual read.
//
// The same gate is here from the start. `rd_q` moves only when a read fires.
//
// ---------------------------------------------------------------------------
// GEOMETRY, AND WHY IT IS THREE SLICES AND NOT ONE BLOCK
// ---------------------------------------------------------------------------
// Brief §D: "A 119-bit-wide descriptor is not one M10K just because 64 x 119 is
// less than 10,240." Capacity is not the binding constraint -- WIDTH is. A
// simple-dual-port M10K tops out at 40 bits per access, so a same-width
// realisation needs ceil(119/40) = 3 parallel slices, and the slicing is written
// out explicitly rather than left for the tool to guess at.
//
// The owner GENERATION is deliberately NOT in the descriptor word. Adding it
// would make the row 127 bits and push the geometry to four slices for eight
// bits of identity. It lives in a separate 64x8 array instead, which is register
// or MLAB territory. Paying a fourth M10K for that would be exactly the "not one
// M10K just because the arithmetic works" mistake in the other direction.
//
// ---------------------------------------------------------------------------
// D0d: THE GENERATION LEAVES THE BANK
// ---------------------------------------------------------------------------
// `metajoin` stores the owner generation, differences it internally into a
// counter, and has no output port for it -- so the island packs 8'd0 where it
// would go and no downstream stage can independently notice a stale join. That
// gap is not reproduced here: `rd_owner_gen_o` is a port.
//
// The internal mismatch counter is also the INSTRUMENT the brief asks for in
// §5.5. It does not enforce a lifetime invariant and deliberately does not add a
// lease -- "prove the window or reproduce it" -- but if an owner slot is
// recycled while a frontend transaction can still read its old row, the stored
// generation will not match the generation offered with the read, and this
// counter says so. The brief asks to instrument actual last-read events; this is
// that instrument, and it costs one comparator.
//
// Unlike metajoin's, it can actually fire: `rd_gen_q` is captured inside the
// same gate as `rd_q`, so the two operands belong to the same read instead of
// riding one ungated register and corrupting in lockstep.
module zhao_texture_early_desc #(
    parameter int unsigned SLOTW  = 6,
    parameter int unsigned GENW   = 8,
    parameter int unsigned SLICEW = 40
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- write: ONE row, on the owner's admission handshake -----------------
    // Brief §5.2: "Write the row once on the EXACT owner admission handshake,
    // using that owner's slot. Do not use a local rolling pointer that can drift
    // from owner admission." So the slot is an INPUT here; this block owns no
    // pointer of its own and therefore has nothing to drift.
    input  var logic                 wr_valid_i,
    input  var logic [SLOTW-1:0]     wr_slot_i,
    input  var logic [GENW-1:0]      wr_owner_gen_i,

    input  var logic [63:0]          wr_aux_context_i,
    input  var logic [7:0]           wr_lod_q4_4_i,
    input  var logic [1:0]           wr_raw_class_i,
    input  var logic                 wr_needs_aux_i,
    input  var logic [1:0]           wr_sample_count_i,
    input  var logic [1:0]           wr_palette_slot_i,
    input  var logic [GENW-1:0]      wr_palette_gen_i,
    input  var logic [7:0]           wr_mosaic_mat_a_i,
    input  var logic [7:0]           wr_mosaic_mat_b_i,
    input  var logic [7:0]           wr_mosaic_weight_i,
    input  var logic [7:0]           wr_binding_sel_i,

    // ---- read: synchronous, one cycle, held ---------------------------------
    input  var logic                 rd_valid_i,
    input  var logic [SLOTW-1:0]     rd_slot_i,
    input  var logic [GENW-1:0]      rd_owner_gen_i,

    output var logic                 rd_result_valid_o,
    output var logic [GENW-1:0]      rd_owner_gen_o,
    output var logic [63:0]          rd_aux_context_o,
    output var logic [7:0]           rd_lod_q4_4_o,
    output var logic [1:0]           rd_raw_class_o,
    output var logic                 rd_needs_aux_o,
    output var logic [1:0]           rd_sample_count_o,
    output var logic [1:0]           rd_palette_slot_o,
    output var logic [GENW-1:0]      rd_palette_gen_o,
    output var logic [7:0]           rd_mosaic_mat_a_o,
    output var logic [7:0]           rd_mosaic_mat_b_o,
    output var logic [7:0]           rd_mosaic_weight_o,
    output var logic [7:0]           rd_binding_sel_o,

    // ---- instruments --------------------------------------------------------
    output var logic [31:0]          writes_o,
    output var logic [31:0]          reads_o,
    output var logic [31:0]          rd_gen_mismatch_o
);

  localparam int unsigned ROWS = 1 << SLOTW;

  // ---- FIELD OFFSETS ARE DERIVED, NEVER HAND-WRITTEN -----------------------
  // metajoin's first version had five hand-written slices and ALL FIVE were off
  // by one. It linted clean, every output had a driver, and every field would
  // have read the wrong bits. The offsets are therefore computed from the widths
  // and checked at elaboration; adding a field in the middle cannot silently
  // shift the ones above it.
  localparam int unsigned AUXCTX_W = 64;
  localparam int unsigned LOD_W    = 8;
  localparam int unsigned CLASS_W  = 2;
  localparam int unsigned AUX_W    = 1;
  localparam int unsigned COUNT_W  = 2;
  localparam int unsigned PSLOT_W  = 2;
  localparam int unsigned PGEN_W   = GENW;
  localparam int unsigned MOSA_W   = 8;
  localparam int unsigned MOSB_W   = 8;
  localparam int unsigned MOSW_W   = 8;
  localparam int unsigned BSEL_W   = 8;

  localparam int unsigned AUXCTX_LO = 0;
  localparam int unsigned LOD_LO    = AUXCTX_LO + AUXCTX_W;
  localparam int unsigned CLASS_LO  = LOD_LO    + LOD_W;
  localparam int unsigned AUX_LO    = CLASS_LO  + CLASS_W;
  localparam int unsigned COUNT_LO  = AUX_LO    + AUX_W;
  localparam int unsigned PSLOT_LO  = COUNT_LO  + COUNT_W;
  localparam int unsigned PGEN_LO   = PSLOT_LO  + PSLOT_W;
  localparam int unsigned MOSA_LO   = PGEN_LO   + PGEN_W;
  localparam int unsigned MOSB_LO   = MOSA_LO   + MOSA_W;
  localparam int unsigned MOSW_LO   = MOSB_LO   + MOSB_W;
  localparam int unsigned BSEL_LO   = MOSW_LO   + MOSW_W;

  localparam int unsigned DESCW  = BSEL_LO + BSEL_W;         // 119
  localparam int unsigned NSLICE = (DESCW + SLICEW - 1) / SLICEW;
  localparam int unsigned PADW   = (NSLICE * SLICEW) - DESCW;

  // synthesis translate_off
  initial begin
    if (DESCW != 119)
      $fatal(1, "early_desc: layout is %0d bits, not the reviewed 119 -- a field was added or resized without updating the profile", DESCW);
    if (NSLICE != 3)
      $fatal(1, "early_desc: %0d width slices, not 3 -- geometry changed; §D says width, not capacity, is the constraint", NSLICE);
  end
  // synthesis translate_on

  // ---- STORAGE: explicit parallel width slices -----------------------------
  // Brief §5.4: "Implement one logical bank, with static parallel width slices
  // as necessary." Written out rather than declared as one wide array and left
  // to the tool, so the three-block geometry is visible in the source and a
  // future width change trips the elaboration check above instead of quietly
  // becoming four blocks.
  //
  // Rows are left UNWRITTEN until their owner is admitted -- §5.4's "leave
  // unused storage words unwritten; validity/lifetime controls protect reads".
  // Initialising them would be a second write address, which is the defect that
  // kept two of perspuv's arrays in flip-flops.
  logic [SLICEW-1:0] mem_q [NSLICE][ROWS];

  wire [NSLICE*SLICEW-1:0] wr_word_c = {
      {PADW{1'b0}},
      wr_binding_sel_i,
      wr_mosaic_weight_i,
      wr_mosaic_mat_b_i,
      wr_mosaic_mat_a_i,
      wr_palette_gen_i,
      wr_palette_slot_i,
      wr_sample_count_i,
      wr_needs_aux_i,
      wr_raw_class_i,
      wr_lod_q4_4_i,
      wr_aux_context_i
  };

  logic [NSLICE*SLICEW-1:0] rd_q;
  logic [GENW-1:0]          rd_gen_q;
  logic                     rd_v_q;

  // The generation table, separate from the descriptor for the geometry reason
  // in the header. One writer, one reader, 64 x 8.
  logic [GENW-1:0] gen_q [ROWS];

  // The generation the READER claimed, held alongside the row it fetched.
  logic [GENW-1:0] rd_owner_gen_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n)          rd_owner_gen_q <= {GENW{1'b0}};
    else if (rd_valid_i) rd_owner_gen_q <= rd_owner_gen_i;
  end

  genvar s;
  generate
    for (s = 0; s < NSLICE; s = s + 1) begin : g_slice
      always_ff @(posedge clk) begin
        if (wr_valid_i)
          mem_q[s][wr_slot_i] <= wr_word_c[s*SLICEW +: SLICEW];
        // THE HOLD LAW. Gated on an actual read, so the output belongs to the
        // read that fetched it and cannot be overwritten by the next address
        // merely being offered while a consumer stalls. This is D0's repair
        // applied at construction rather than after a defect.
        if (rd_valid_i)
          rd_q[s*SLICEW +: SLICEW] <= mem_q[s][rd_slot_i];
      end
    end
  endgenerate

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rd_v_q            <= 1'b0;
      rd_gen_q          <= {GENW{1'b0}};
      writes_o          <= 32'd0;
      reads_o           <= 32'd0;
      rd_gen_mismatch_o <= 32'd0;
    end else begin
      if (wr_valid_i) begin
        gen_q[wr_slot_i] <= wr_owner_gen_i;
        writes_o         <= writes_o + 32'd1;
      end

      // Inside the same gate as the payload, so the captured generation belongs
      // to the same read as the row. metajoin's did not, which is why its
      // mismatch counter could never fire on the defect it looked built for.
      if (rd_valid_i) begin
        rd_gen_q <= gen_q[rd_slot_i];
        reads_o  <= reads_o + 32'd1;
      end
      rd_v_q <= rd_valid_i;

      // §5.5's instrument: an owner recycled while a frontend transaction can
      // still read its old row shows up here as a generation that does not match
      // the one offered with the read. It reports; it does not enforce.
      if (rd_v_q && (rd_gen_q != rd_owner_gen_q))
        rd_gen_mismatch_o <= rd_gen_mismatch_o + 32'd1;
    end
  end

  assign rd_result_valid_o = rd_v_q;
  assign rd_owner_gen_o    = rd_gen_q;
  assign rd_aux_context_o  = rd_q[AUXCTX_LO +: AUXCTX_W];
  assign rd_lod_q4_4_o     = rd_q[LOD_LO    +: LOD_W];
  assign rd_raw_class_o    = rd_q[CLASS_LO  +: CLASS_W];
  assign rd_needs_aux_o    = rd_q[AUX_LO];
  assign rd_sample_count_o = rd_q[COUNT_LO  +: COUNT_W];
  assign rd_palette_slot_o = rd_q[PSLOT_LO  +: PSLOT_W];
  assign rd_palette_gen_o  = rd_q[PGEN_LO   +: PGEN_W];
  assign rd_mosaic_mat_a_o = rd_q[MOSA_LO   +: MOSA_W];
  assign rd_mosaic_mat_b_o = rd_q[MOSB_LO   +: MOSB_W];
  assign rd_mosaic_weight_o= rd_q[MOSW_LO   +: MOSW_W];
  assign rd_binding_sel_o  = rd_q[BSEL_LO   +: BSEL_W];

endmodule
