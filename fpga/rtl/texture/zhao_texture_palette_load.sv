// zhao_texture_palette_load.sv -- TEXTURE.PALETTELOAD: the palette identity's
// producer.  A CLUT material names a palette by ADDRESS; this block makes that
// palette RESIDENT in one of `zhao_texture_palette_res_v2`'s slots and answers
// with the {slot, generation} pair the fragment's witness has to carry.
//
// ===========================================================================
// WHY THIS BLOCK EXISTS, AND WHY IT IS NOT A CONSTANT
// ===========================================================================
// `zhao_material_window.sv`'s header has said this for weeks, and
// `clut_unowned_o` has counted it:
//
//     "For a CLUT format the pair is real and nothing in this console produces
//      it: no ratified material field carries the palette's identity."
//
// It is real in a harder way than that sentence says.  `zhao_texture_binding_
// resolver_v2`'s `read_witness_bad_c` REFUSES a sample whose
// {class, palette_slot, palette_generation} does not equal the BINDING ROW's,
// and a TILESET row -- terrain's mosaic -- is CLUT8 BY LAW
// (`tileset_shape_ok`).  So a console that publishes the pair as composer
// constants can bind exactly one palette identity, {0, 0}, and generation ZERO
// was the one generation `zhao_texture_palette_res_v2` could not be handed in a
// single pass.  Six packets refused I13's mosaic on that chain.
//
// The gap was never the LOOKUP.  It was that nothing ever WROTE the palette RAM
// in this console: `pal_load_*` left `zhao_console_core` as eight boundary
// inputs and the smoke tied them to zero, which is the exact shape the owner's
// completion ruling of 2026-09-22 item 3 refuses --
//
//     "This includes any required legitimate asset/palette loading producer.
//      AN OPCODE PLUS DESCRIPTORS REFERRING TO DATA THAT ONLY THE TESTBENCH CAN
//      INJECT IS NOT COMPLETION.  Reuse PublishResource and the existing
//      validated resource mechanisms wherever applicable."
//
// `zhao_twod_asset` is that ruling's other half, one subsystem over, and its
// header carries the same quotation.  This block is the texture island's.
//
// ===========================================================================
// IT READS VRAM, NOT THE HPS ARENA, AND THAT IS THE DIFFERENCE FROM TWOD.ASSET
// ===========================================================================
// TWOD.ASSET reads the HPS staging arena because `spec/cartridge.md` 4f ruled
// that kind 15 never travels to VRAM -- the compositor has no memory client and
// no window in `spec/memory_rules.md`.  A palette is not in that position:
//
//   * `spec/cartridge.md` says outright that "palette data is a SUBTYPE of
//     TEXTURE_PAGE, not a separate family.  It uses the same publication and
//     generation machinery rather than growing an unrelated loader path";
//   * `MaterialRecord.palette_base` is `u32` and the oracle reads the palette
//     FROM THAT ADDRESS, so the address is the ratified name of the object;
//   * and the asset-pool window is already open to this island's subsystem --
//     MATERIAL.RESOLVE fetches its 32-byte records through the very same
//     ENGINE1 share.
//
// So no cartridge kind is allocated, no ABI field moves, `zhao_cmd_exec` gains
// no fork, and a palette arrives through the ordinary
// `PublishResource` -> MEM.UPLOAD -> VRAM path the console smoke already
// exercises for MATERIAL_SET.  This block is one more requester on
// `zhao_geom_mem_adapter`, which is an exercised pattern -- C landed
// 2026-09-19, F on the 21st, G on the 22nd, H on the 23rd.
//
// ===========================================================================
// ONE BEAT PER REQUEST, ON PURPOSE
// ===========================================================================
// `zhao_guard_req_t.len` is 1..64 bytes, so a 512-byte palette could be eight
// 64-byte reads.  It is SIXTY-FOUR eight-byte reads instead, and the reason is
// area: a 64-byte burst delivers eight 64-bit beats that CANNOT BE STALLED
// (there is no `ready` on `mem_rsp_valid_i`; MATERIAL.RESOLVE takes its four
// beats at line rate into a 256-bit register), while the palette's programming
// port takes ONE 16-bit entry per cycle -- four cycles per beat.  Absorbing a
// 64-byte burst therefore costs a 512-bit shadow register on a device measured
// at 97% of its ALM ceiling, to save latency on an event that happens at most
// once per material span.  One beat in, four entries out, no buffer.
//
// ===========================================================================
// THE GENERATION IS PRODUCED, AND IT STARTS AT ZERO
// ===========================================================================
// `next_gen_q[slot]` resets to zero and advances by one on every BEGIN this
// block issues for that slot, whatever the outcome.  The resolver holds the
// PREVIOUSLY issued value, so the one this block presents differs from it by
// exactly one and can never collide -- there is no wrap hazard and therefore no
// guard for one.  The FIRST load of a slot is at generation ZERO, which is the
// value the old `LD_BEGIN` guard refused; that guard is repaired in
// `zhao_texture_palette_res_v2.sv` and `gen_zero_loads_o` here is the counter
// that says the repair is being USED rather than merely being present.
//
// ===========================================================================
// TWO RESIDENCY RECORDS, AND WHY THE DISAGREEMENT IS NOT SILENT
// ===========================================================================
// This block keeps a CACHE TAG (`resident_q`, `base_q`, `gen_live_q`) and the
// resolver keeps the CONTENT residency.  They are written by different enables
// in different modules, which is the shape CLAUDE.md's metadata-bank chapter
// warns about -- so it is stated rather than assumed where they can diverge and
// what happens if they do.  Both are driven by the SAME protocol events: a
// BEGIN clears both, a successful END sets both, and the resolver refuses an
// END whose slot or generation is not the one it opened.  If they ever did
// disagree the fragment would carry a pair the resolver does not hold, and the
// resolver answers that with SOURCE_REFUSED magenta while counting `stale_o` or
// `cold_o` -- a loud wrong pixel with a counter beside it, never a quiet right
// one.
//
// ===========================================================================
// EVERY OUTCOME IS AN ANSWER.  IT NEVER HANGS.
// ===========================================================================
// Owner ruling R20's law, carried one seam further, exactly as MATERIAL.RESOLVE
// carries it: a null base, a misaligned base, a base outside VRAM and a denied
// fetch all produce `r_valid_o` with `r_owned_o` LOW and a counter moving.  The
// caller stalls for the load and never for a fault.
//
// Conservative SystemVerilog subset only (charter 2).
`default_nettype none

module zhao_texture_palette_load #(
    parameter int unsigned SLOTS     = 4,
    parameter int unsigned ENTRIES   = 256,
    parameter int unsigned GENW      = 8,
    // VRAM is 2^ADDR_BITS bytes; a palette wholly outside it is refused rather
    // than truncated into a legal-looking address, which is MEM.UPLOAD's own
    // `kUploadSourceUnreachable` direction.
    parameter int unsigned ADDR_BITS = 27
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the lookup: "what is the identity of the palette at this base?" ----
    input  var logic                     q_valid_i,
    output var logic                     q_ready_o,
    input  var logic [31:0]              q_base_i,

    output var logic                     r_valid_o,
    input  var logic                     r_ready_i,
    output var logic                     r_owned_o,
    output var logic [$clog2(SLOTS)-1:0] r_slot_o,
    output var logic [GENW-1:0]          r_gen_o,

    // ---- ENGINE1 read, one requester of zhao_geom_mem_adapter --------------
    output var logic                     mem_req_valid_o,
    input  var logic                     mem_req_ready_i,
    output var logic [31:0]              mem_req_addr_o,
    input  var logic                     mem_rsp_valid_i,
    input  var logic [63:0]              mem_rsp_data_i,
    input  var logic                     mem_rsp_denied_i,

    // ---- zhao_texture_palette_res_v2's BEGIN/WRITE/END programming port ----
    output var logic                     ld_valid_o,
    input  var logic                     ld_ready_i,
    output var logic [1:0]               ld_op_o,
    output var logic [$clog2(SLOTS)-1:0] ld_slot_o,
    output var logic [GENW-1:0]          ld_gen_o,
    output var logic [$clog2(ENTRIES)-1:0] ld_idx_o,
    output var logic [15:0]              ld_rgb565_o,
    output var logic                     ld_crc_ok_o,

    // ---- evidence ----------------------------------------------------------
    output var logic [31:0]              lookups_o,
    output var logic [31:0]              hits_o,
    output var logic [31:0]              loads_o,
    output var logic [31:0]              evictions_o,
    output var logic [31:0]              entries_written_o,
    output var logic [31:0]              denied_o,
    output var logic [31:0]              base_refused_o,
    output var logic [31:0]              gen_zero_loads_o
);

  localparam int unsigned SLOTW = $clog2(SLOTS);
  localparam int unsigned IDXW  = $clog2(ENTRIES);
  localparam int unsigned ENTW  = IDXW + 1;              // 0..ENTRIES inclusive
  localparam int unsigned BYTES = ENTRIES * 2;

  localparam logic [1:0] LD_BEGIN = 2'd0;
  localparam logic [1:0] LD_WRITE = 2'd1;
  localparam logic [1:0] LD_END   = 2'd2;

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`; a
  // bare module-scope `if` is a syntax error there while linting CLEAN here,
  // which is why this shape is spelled out rather than assumed.
  initial begin : p_layout_contract
    if ((SLOTS < 2) || ((1 << SLOTW) != SLOTS))
      $fatal(1, "ZHAO_PALLOAD_PARAM_FIRE[SLOTS_POWER_OF_TWO]");
    if ((ENTRIES != 256) || (IDXW != 8))
      $fatal(1, "ZHAO_PALLOAD_PARAM_FIRE[ENTRIES256_IDXW8]");
    if (GENW != 8)
      $fatal(1, "ZHAO_PALLOAD_PARAM_FIRE[GENW8]");
    if ((ADDR_BITS < 10) || (ADDR_BITS > 32))
      $fatal(1, "ZHAO_PALLOAD_PARAM_FIRE[ADDR_BITS_RANGE]");
  end

  typedef enum logic [2:0] {
    S_IDLE  = 3'd0,
    S_BEGIN = 3'd1,
    S_ADDR  = 3'd2,
    S_FILL  = 3'd3,
    S_WRITE = 3'd4,
    S_END   = 3'd5,
    S_ANS   = 3'd6
  } state_e;

  state_e state_q;

  logic                resident_q [SLOTS];
  logic [31:0]         base_q     [SLOTS];
  logic [GENW-1:0]     gen_live_q [SLOTS];
  logic [GENW-1:0]     next_gen_q [SLOTS];

  logic [SLOTW-1:0]    rr_q;
  logic [31:0]         ask_base_q;
  logic [SLOTW-1:0]    victim_q;
  logic [GENW-1:0]     use_gen_q;
  logic [ENTW-1:0]     ent_q;
  logic [1:0]          sub_q;
  logic [63:0]         beat_q;
  logic                ok_q;

  logic                ans_owned_q;
  logic [SLOTW-1:0]    ans_slot_q;
  logic [GENW-1:0]     ans_gen_q;

  // ---- the tag lookup, combinational over a four-entry table ---------------
  logic             hit_c;
  logic [SLOTW-1:0] hit_slot_c;
  logic             free_c;
  logic [SLOTW-1:0] free_slot_c;

  always_comb begin
    hit_c       = 1'b0;
    hit_slot_c  = '0;
    free_c      = 1'b0;
    free_slot_c = '0;
    for (int unsigned s = 0; s < SLOTS; s++) begin
      if (resident_q[s] && (base_q[s] == q_base_i) && !hit_c) begin
        hit_c      = 1'b1;
        hit_slot_c = SLOTW'(s);
      end
      if (!resident_q[s] && !free_c) begin
        free_c      = 1'b1;
        free_slot_c = SLOTW'(s);
      end
    end
  end

  // A base is REFUSED, never corrected.  Zero is the record's own "no sample is
  // a CLUT mode"; the eight-byte alignment is the beat this block reads in; and
  // the extent test is done one bit wider than the address so a palette running
  // off the end of VRAM cannot wrap into a small, comfortable-looking number --
  // MEM.UPLOAD's 33-bit containment reasoning at this block's scale.
  wire [32:0] end_c = {1'b0, q_base_i} + 33'(BYTES);
  wire bad_base_c = (q_base_i == 32'd0)
                 || (q_base_i[2:0] != 3'd0)
                 || (end_c > (33'd1 << ADDR_BITS));

  wire [SLOTW-1:0] victim_c = free_c ? free_slot_c : rr_q;

  assign q_ready_o = (state_q == S_IDLE);
  assign r_valid_o = (state_q == S_ANS);
  assign r_owned_o = ans_owned_q;
  assign r_slot_o  = ans_slot_q;
  assign r_gen_o   = ans_gen_q;

  assign mem_req_valid_o = (state_q == S_ADDR);
  // `ent_q` counts ENTRIES and an entry is two bytes.  The walk advances four
  // entries per accepted beat, so this address is eight-byte aligned whenever
  // the base is -- which `bad_base_c` guarantees.
  assign mem_req_addr_o  = ask_base_q + {{(32-ENTW-1){1'b0}}, ent_q, 1'b0};

  assign ld_valid_o  = (state_q == S_BEGIN) || (state_q == S_WRITE)
                    || (state_q == S_END);
  always_comb begin
    ld_op_o = LD_WRITE;
    if (state_q == S_BEGIN) ld_op_o = LD_BEGIN;
    else if (state_q == S_END) ld_op_o = LD_END;
  end
  assign ld_slot_o   = victim_q;
  assign ld_gen_o    = use_gen_q;
  assign ld_idx_o    = ent_q[IDXW-1:0];
  assign ld_rgb565_o = beat_q[{sub_q, 4'd0} +: 16];
  // NOT a constant.  The resolver's `ld_crc_ok_i` asks "are these the bytes the
  // producer intended?", and the page's CRC-32C was already folded and enforced
  // by MEM.UPLOAD before publication -- re-folding a 512-byte slice of it here
  // would have no reference to check against.  What this block CAN answer is
  // whether every beat of the transfer was delivered, so a DENIED fetch ends
  // the load with this low and the resolver refuses residency and counts it.
  assign ld_crc_ok_o = ok_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q           <= S_IDLE;
      rr_q              <= '0;
      ask_base_q        <= 32'd0;
      victim_q          <= '0;
      use_gen_q         <= '0;
      ent_q             <= '0;
      sub_q             <= 2'd0;
      beat_q            <= 64'd0;
      ok_q              <= 1'b0;
      ans_owned_q       <= 1'b0;
      ans_slot_q        <= '0;
      ans_gen_q         <= '0;
      lookups_o         <= 32'd0;
      hits_o            <= 32'd0;
      loads_o           <= 32'd0;
      evictions_o       <= 32'd0;
      entries_written_o <= 32'd0;
      denied_o          <= 32'd0;
      base_refused_o    <= 32'd0;
      gen_zero_loads_o  <= 32'd0;
      for (int unsigned s = 0; s < SLOTS; s++) begin
        resident_q[s] <= 1'b0;
        base_q[s]     <= 32'd0;
        gen_live_q[s] <= '0;
        next_gen_q[s] <= '0;
      end
    end else begin
      unique case (state_q)
        S_IDLE: begin
          if (q_valid_i) begin
            lookups_o  <= lookups_o + 32'd1;
            ask_base_q <= q_base_i;
            if (hit_c) begin
              hits_o      <= hits_o + 32'd1;
              ans_owned_q <= 1'b1;
              ans_slot_q  <= hit_slot_c;
              ans_gen_q   <= gen_live_q[hit_slot_c];
              state_q     <= S_ANS;
            end else if (bad_base_c) begin
              base_refused_o <= base_refused_o + 32'd1;
              ans_owned_q    <= 1'b0;
              ans_slot_q     <= '0;
              ans_gen_q      <= '0;
              state_q        <= S_ANS;
            end else begin
              victim_q  <= victim_c;
              use_gen_q <= next_gen_q[victim_c];
              if (resident_q[victim_c])
                evictions_o <= evictions_o + 32'd1;
              if (!free_c) rr_q <= rr_q + SLOTW'(1);
              state_q <= S_BEGIN;
            end
          end
        end

        // The BEGIN invalidates the binding in BOTH records on the same beat,
        // and advances this slot's generation counter whatever the load's
        // outcome turns out to be -- so a failed load never offers the resolver
        // a generation it is already holding.
        S_BEGIN: begin
          if (ld_ready_i) begin
            resident_q[victim_q] <= 1'b0;
            next_gen_q[victim_q] <= use_gen_q + GENW'(1);
            loads_o              <= loads_o + 32'd1;
            if (use_gen_q == '0)
              gen_zero_loads_o <= gen_zero_loads_o + 32'd1;
            ent_q   <= '0;
            sub_q   <= 2'd0;
            ok_q    <= 1'b1;
            state_q <= S_ADDR;
          end
        end

        S_ADDR: begin
          if (mem_req_ready_i) state_q <= S_FILL;
        end

        S_FILL: begin
          // The guard's verdict arrives the cycle AFTER it accepted the
          // request.  A denial ends the load here: the END still runs, with
          // `ok_q` low, so the resolver refuses residency rather than this
          // block deciding on its behalf.
          if (mem_rsp_denied_i) begin
            ok_q     <= 1'b0;
            denied_o <= denied_o + 32'd1;
            state_q  <= S_END;
          end else if (mem_rsp_valid_i) begin
            beat_q  <= mem_rsp_data_i;
            sub_q   <= 2'd0;
            state_q <= S_WRITE;
          end
        end

        S_WRITE: begin
          if (ld_ready_i) begin
            entries_written_o <= entries_written_o + 32'd1;
            ent_q             <= ent_q + ENTW'(1);
            if (sub_q == 2'd3) begin
              state_q <= ((ent_q + ENTW'(1)) == ENTW'(ENTRIES)) ? S_END : S_ADDR;
            end else begin
              sub_q <= sub_q + 2'd1;
            end
          end
        end

        S_END: begin
          if (ld_ready_i) begin
            if (ok_q && (ent_q == ENTW'(ENTRIES))) begin
              resident_q[victim_q] <= 1'b1;
              base_q[victim_q]     <= ask_base_q;
              gen_live_q[victim_q] <= use_gen_q;
              ans_owned_q          <= 1'b1;
            end else begin
              ans_owned_q <= 1'b0;
            end
            ans_slot_q <= victim_q;
            ans_gen_q  <= use_gen_q;
            state_q    <= S_ANS;
          end
        end

        S_ANS: begin
          if (r_ready_i) state_q <= S_IDLE;
        end

        default: state_q <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_texture_palette_load

`default_nettype wire
