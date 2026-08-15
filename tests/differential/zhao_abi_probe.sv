// zhao_abi_probe.sv — test-side SV probe around the generated zhao_abi_pkg
// (plan W4). Ports only, no DPI: the C++ differential tests drive bytes in
// and read bytes/results out, so every assertion runs the SAME generated
// pack/unpack/CRC/validate code the hardware consumes.
//
// Three independent engines (parallel, no arbitration):
//
//   1. pack/unpack check (pu_*): feed a complete command record byte by
//      byte; the probe unpacks it into the generated packed struct and
//      re-packs it (combinational, in repack_vec), streams the result back
//      out (pu_out_*) and flags any differing byte (pu_mismatch, sticky).
//      This is the plan-R1 reverse-field-order hazard guard: any drift
//      between struct declaration order and wire layout shows up here.
//
//   2. streaming CRC-32C (crc_*): feed bytes, read the running register;
//      the finalized value is ~crc_reg (matches the C++/TS table form).
//
//   3. frame validation (fv_*): feed a whole sealed packet, then pulse
//      fv_trigger for one cycle; fv_error/fv_commands carry the generated
//      validator's verdict (capture_format.md 3.2 order).
//
// Conservative subset (charter 2). Lint: clean under -Wall.

module zhao_abi_probe
  import zhao_abi_pkg::*;
(
  input  logic        clk,
  input  logic        rst_n,

  // 1. pack/unpack byte-stream check
  input  logic        pu_valid,
  output logic        pu_ready,
  input  logic [7:0]  pu_in_data,
  output logic        pu_out_valid,
  input  logic        pu_out_ready,
  output logic [7:0]  pu_out_data,
  output logic        pu_mismatch,     // sticky since the record start
  output logic        pu_active,       // high while a record is in flight

  // 2. streaming CRC-32C (register form; finalize with ~crc_reg)
  input  logic        crc_in_valid,
  input  logic [7:0]  crc_in_data,
  input  logic        crc_seed,        // restart the accumulation
  output logic [31:0] crc_reg,

  // 3. whole-packet frame validation
  input  logic        fv_valid,
  output logic        fv_ready,
  input  logic [7:0]  fv_data,
  input  logic [31:0] fv_expect_len,   // total packet bytes the test will feed
  input  logic        fv_trigger,      // 1-cycle strobe after the last byte
  output logic [7:0]  fv_error,        // zhao_abi_error code (valid with trigger)
  output logic [31:0] fv_commands,
  output logic        fv_collecting,
  output logic        fv_layout_ok     // zhao_layout_ok()
);

  localparam int unsigned MAX_REC = ZHAO_MAX_RECORD_BYTES;

  // ---------------------------------------------------------------- engine 1
  typedef enum logic [1:0] { PU_IDLE = 2'd0, PU_COLLECT = 2'd1, PU_EMIT = 2'd2 } pu_state_e;
  pu_state_e  pu_state;
  logic [7:0] pu_buf [0:MAX_REC-1];
  logic [31:0] pu_in_idx;
  logic [31:0] pu_in_len;
  logic [31:0] pu_out_idx;

  assign pu_ready  = (pu_state == PU_IDLE) || (pu_state == PU_COLLECT);
  assign pu_active = (pu_state != PU_IDLE);

  // combinational unpack->pack over the collected buffer (pure function of
  // pu_buf + the opcode/length decoded from it)
  logic [15:0]      pu_op;
  logic [MAX_REC*8-1:0] pu_vec;     // collected bytes, byte 0 at bits [7:0]
  logic [MAX_REC*8-1:0] pu_repacked;

  always_comb begin
    pu_op  = {pu_buf[1], pu_buf[0]};
    pu_vec = '0;
    for (int unsigned i = 0; i < MAX_REC; i++) begin
      pu_vec[i*8 +: 8] = pu_buf[i];
    end
    pu_repacked = '0;
    case (pu_op)
      ZHAO_OP_NOP: begin
        pu_repacked[ZHAO_NOP_BYTES*8-1:0] =
          zhao_pack_nop(zhao_unpack_nop(pu_vec[ZHAO_NOP_BYTES*8-1:0]));
      end
      ZHAO_OP_BEGIN_FRAME: begin
        pu_repacked[ZHAO_BEGIN_FRAME_BYTES*8-1:0] =
          zhao_pack_begin_frame(zhao_unpack_begin_frame(pu_vec[ZHAO_BEGIN_FRAME_BYTES*8-1:0]));
      end
      ZHAO_OP_END_FRAME: begin
        pu_repacked[ZHAO_END_FRAME_BYTES*8-1:0] =
          zhao_pack_end_frame(zhao_unpack_end_frame(pu_vec[ZHAO_END_FRAME_BYTES*8-1:0]));
      end
      ZHAO_OP_SET_VIEW: begin
        pu_repacked[ZHAO_SET_VIEW_BYTES*8-1:0] =
          zhao_pack_set_view(zhao_unpack_set_view(pu_vec[ZHAO_SET_VIEW_BYTES*8-1:0]));
      end
      ZHAO_OP_SET_PRESENTATION_CONTRACT: begin
        pu_repacked[ZHAO_SET_PRESENTATION_CONTRACT_BYTES*8-1:0] =
          zhao_pack_set_presentation_contract(
            zhao_unpack_set_presentation_contract(
              pu_vec[ZHAO_SET_PRESENTATION_CONTRACT_BYTES*8-1:0]));
      end
      ZHAO_OP_TERRAIN_FIELD: begin
        pu_repacked[ZHAO_TERRAIN_FIELD_BYTES*8-1:0] =
          zhao_pack_terrain_field(zhao_unpack_terrain_field(pu_vec[ZHAO_TERRAIN_FIELD_BYTES*8-1:0]));
      end
      ZHAO_OP_SURFACE_STAMP: begin
        pu_repacked[ZHAO_SURFACE_STAMP_BYTES*8-1:0] =
          zhao_pack_surface_stamp(zhao_unpack_surface_stamp(pu_vec[ZHAO_SURFACE_STAMP_BYTES*8-1:0]));
      end
      ZHAO_OP_DRAW_FORM: begin
        pu_repacked[ZHAO_DRAW_FORM_BYTES*8-1:0] =
          zhao_pack_draw_form(zhao_unpack_draw_form(pu_vec[ZHAO_DRAW_FORM_BYTES*8-1:0]));
      end
      ZHAO_OP_DRAW_POPULATION: begin
        pu_repacked[ZHAO_DRAW_POPULATION_BYTES*8-1:0] =
          zhao_pack_draw_population(zhao_unpack_draw_population(pu_vec[ZHAO_DRAW_POPULATION_BYTES*8-1:0]));
      end
      ZHAO_OP_DRAW_PROCEDURAL: begin
        pu_repacked[ZHAO_DRAW_PROCEDURAL_BYTES*8-1:0] =
          zhao_pack_draw_procedural(zhao_unpack_draw_procedural(pu_vec[ZHAO_DRAW_PROCEDURAL_BYTES*8-1:0]));
      end
      ZHAO_OP_EMIT_AUDIO_EVENT: begin
        pu_repacked[ZHAO_EMIT_AUDIO_EVENT_BYTES*8-1:0] =
          zhao_pack_emit_audio_event(zhao_unpack_emit_audio_event(pu_vec[ZHAO_EMIT_AUDIO_EVENT_BYTES*8-1:0]));
      end
      ZHAO_OP_DEBUG_BOOTSTRAP: begin
        pu_repacked[ZHAO_DEBUG_BOOTSTRAP_BYTES*8-1:0] =
          zhao_pack_debug_bootstrap(zhao_unpack_debug_bootstrap(pu_vec[ZHAO_DEBUG_BOOTSTRAP_BYTES*8-1:0]));
      end
      ZHAO_OP_DRAW_SKY: begin
        pu_repacked[ZHAO_DRAW_SKY_BYTES*8-1:0] =
          zhao_pack_draw_sky(zhao_unpack_draw_sky(pu_vec[ZHAO_DRAW_SKY_BYTES*8-1:0]));
      end
      ZHAO_OP_DEBUG_FRAME_BLIT: begin
        pu_repacked[ZHAO_DEBUG_FRAME_BLIT_BYTES*8-1:0] =
          zhao_pack_debug_frame_blit(zhao_unpack_debug_frame_blit(pu_vec[ZHAO_DEBUG_FRAME_BLIT_BYTES*8-1:0]));
      end
      ZHAO_OP_DEBUG_RUMBLE: begin
        pu_repacked[ZHAO_DEBUG_RUMBLE_BYTES*8-1:0] =
          zhao_pack_debug_rumble(zhao_unpack_debug_rumble(pu_vec[ZHAO_DEBUG_RUMBLE_BYTES*8-1:0]));
      end
      default: pu_repacked = '0;  // never reached (tests drive known records)
    endcase
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      pu_state     <= PU_IDLE;
      pu_in_idx    <= 32'd0;
      pu_in_len    <= 32'd0;
      pu_out_idx   <= 32'd0;
      pu_mismatch  <= 1'b0;
      pu_out_valid <= 1'b0;
    end else begin
      pu_out_valid <= 1'b0;
      case (pu_state)
        PU_IDLE: begin
          if (pu_valid && pu_ready) begin
            pu_buf[0]  <= pu_in_data;
            pu_in_idx  <= 32'd1;
            pu_in_len  <= 32'd0;
            pu_mismatch<= 1'b0;
            pu_state   <= PU_COLLECT;
          end
        end

        PU_COLLECT: begin
          if (pu_valid && pu_ready) begin
            pu_buf[pu_in_idx] <= pu_in_data;
            // record_bytes arrives as stream bytes [3] and [2] (LE u16 at +2)
            if (pu_in_idx == 32'd2) pu_in_len[7:0]  <= pu_in_data;
            if (pu_in_idx == 32'd3) pu_in_len[15:8] <= pu_in_data;
            // record_bytes (u16 at stream +2) is fully visible from
            // index 4 on, so gate the termination on that
            if (pu_in_idx >= 32'd4 && pu_in_idx + 32'd1 >= pu_in_len) begin
              pu_state   <= PU_EMIT;
              pu_out_idx <= 32'd0;
            end else begin
              pu_in_idx <= pu_in_idx + 32'd1;
            end
          end
        end

        PU_EMIT: begin
          // first cycle: pu_buf is fully visible; stream repacked bytes out
          if (pu_out_idx < pu_in_len) begin
            if (pu_out_valid && pu_out_ready) begin
              pu_out_idx <= pu_out_idx + 32'd1;
            end else if (!pu_out_valid) begin
              pu_out_valid <= 1'b1;
            end
          end else begin
            pu_out_valid <= 1'b0;
            pu_state     <= PU_IDLE;
            pu_in_idx    <= 32'd0;
          end
          // compare the byte being presented
          if (pu_out_valid) begin
            if (pu_repacked[pu_out_idx*8 +: 8] != pu_buf[pu_out_idx]) pu_mismatch <= 1'b1;
          end
        end

        default: pu_state <= PU_IDLE;
      endcase
    end
  end

  // pu_out_data mux
  always_comb begin
    pu_out_data = pu_repacked[pu_out_idx*8 +: 8];
  end

  // ---------------------------------------------------------------- engine 2
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      crc_reg <= 32'hFFFFFFFF;
    end else if (crc_seed) begin
      crc_reg <= 32'hFFFFFFFF;
    end else if (crc_in_valid) begin
      crc_reg <= zhao_crc32c_step(crc_reg, crc_in_data);
    end
  end

  // ---------------------------------------------------------------- engine 3
  logic [7:0]  fv_slot [0:FRAME_SLOT_BYTES-1];
  logic [31:0] fv_idx;

  assign fv_ready      = 1'b1;
  assign fv_collecting = (fv_idx != 32'd0);  // informational
  assign fv_layout_ok  = zhao_layout_ok();

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      fv_idx <= 32'd0;
    end else begin
      if (fv_valid && fv_ready && fv_idx < fv_expect_len) begin
        fv_slot[fv_idx] <= fv_data;
        fv_idx <= fv_idx + 32'd1;
      end else if (fv_idx >= fv_expect_len) begin
        fv_idx <= 32'd0;  // rearm for the next packet
      end
    end
  end

  // verdict: combinational while fv_trigger is high (the C++ test reads
  // fv_error/fv_commands in the same eval)
  always_comb begin
    fv_error    = ZH_ABI_OK;
    fv_commands = 32'd0;
    if (fv_trigger) begin
      fv_error = zhao_frame_validate(fv_slot, fv_expect_len, FRAME_SLOT_BYTES, fv_commands);
    end
  end

endmodule : zhao_abi_probe
