// GENERATED FILE -- DO NOT EDIT.
// Generator: tools/quartus/gen_terrain_pipe_rpp3_matw18_fit_top.py
// generator-sha256: 002614c039bff7e0a924c69afa70723740ae8b5280df18563f5c0f155cb1ba30
// template-sha256: e73cf8326374c6193c9f9fb3816672498116b01898d001504641f4cb9eab2cf7
// manifest: fpga/rtl/generated/zhao_terrain_pipe_rpp3_matw18_fit_top.manifest.json
// Parameter witness: u_terrain_pipe sets ROWS_PER_PASS=3 and MATW=18
// as LITERALS. The G8B target must not inherit either from a module
// default, a fit-target comment or a runtime convention.
// Characterization traffic is legal and deterministic; this is not a
// shell or board top.

// Packet-I's fixed real-pin/MISR G8B terrain wrapper.
//
// WHY THIS EXISTS. The G8B target is
//
//     zhao_terrain_pipe #(.ROWS_PER_PASS(3), .MATW(18))
//
// and the composition architecture is explicit that the wrapper "must not rely
// on zhao_terrain_pipe's default MATW=32, a fit-target comment, or a runtime
// convention to establish the parameters", and that the target source closure
// names THIS wrapper rather than raw zhao_terrain_pipe as the top. Both
// parameters are therefore written at the instantiation below, as literals, in
// one place a reader can see.
//
// The pipe has 102 ports and a fit top may have almost none, so everything the
// design produces is compressed into one 8-bit signature pin and one 8-bit
// epoch pin, both `useioff = 1` so the fit measures the DESIGN and not a pad
// path. Every counter and every output-stream field takes its turn in the MISR;
// the epoch says which one is being fed, so a signature that stops moving can
// be attributed instead of guessed at.
//
// THE MATRIX WORDS ARE NOT RANDOM, AND THAT IS THE POINT.
//
// At MATW=18 the nine ROW PRODUCT words -- matrix addresses with `addr < 16`,
// `addr[1:0] != 3` and `addr[3:2] != 2`, i.e. rows 0/1/3 by columns 0/1/2 --
// must fit a signed 18-bit value. A write that does not fit is REFUSED: the
// register keeps its previous value and `mat_refused_o` counts it. Never a
// clamp. The translation column and row 2 keep their full 32 bits and accept
// anything without counting.
//
// So a wrapper that drove random 32-bit words at those nine addresses would
// characterise a machine whose matrix never loaded -- every product word
// refused, the projector running on reset values, and an ALM/Fmax number that
// describes nothing anyone wants. The configuration below writes deliberate
// small scales into the nine, full-width values elsewhere, and the assertion at
// the bottom fails the simulation if `mat_refused_o` is ever nonzero. The fit
// cannot quietly measure a refusing machine.
//
// LEGAL-MASK ACTIVITY. The receipt gate wants nonempty view masks 2'b01, 2'b10
// and headline 2'b11 exercised with distinct matrices and viewports, with the
// dense 2'b11 case as the receipt workload. `job_view_mask_w` cycles the three
// legal values and never emits 2'b00; the two views are configured with
// different scales and different viewport extents so the masks are not three
// names for one arrangement.
//
// Traffic is legal and deterministic. This is not a shell or a board top.
`default_nettype none

module zhao_terrain_pipe_rpp3_matw18_fit_top (
    input  logic       clk,
    input  logic       rst_n,
    (* useioff = 1 *) output logic [7:0] fit_signature_o,
    (* useioff = 1 *) output logic [7:0] fit_epoch_o
);

  // The two parameters the whole target exists to pin, as literals.
  localparam int unsigned G8B_ROWS_PER_PASS = 3;
  localparam int unsigned G8B_MATW          = 18;
  localparam int unsigned G8B_PAYLOAD_A_W   = 16;
  localparam int unsigned G8B_ARENAS        = 4;

  localparam logic [1:0] S_CFG = 2'd0;
  localparam logic [1:0] S_WARM = 2'd1;
  localparam logic [1:0] S_RUN = 2'd2;

  (* keep = "true" *) logic [63:0] stimulus_lfsr_q;
  (* keep = "true" *) logic [31:0] signature_misr_q;
  (* keep = "true" *) logic [5:0]  signature_source_q;
  logic [31:0] signature_word_c;
  logic [31:0] signature_word_q;

  logic [1:0] setup_state_q;
  logic [5:0] cfg_index_q;      // 0..35 -> view*18 + addr
  logic [7:0] warm_count_q;

  // ---- configuration -------------------------------------------------------
  wire        cfg_view_c = cfg_index_q >= 6'd18;
  wire [5:0]  cfg_offset_c = cfg_view_c ? (cfg_index_q - 6'd18) : cfg_index_q;
  wire [4:0]  cfg_addr_c = cfg_offset_c[4:0];
  wire        cfg_we_c   = (setup_state_q == S_CFG);

  // The nine product words, by the core's own decode.
  wire cfg_is_prod_c = (cfg_addr_c < 5'd16) && (cfg_addr_c[1:0] != 2'd3) &&
                       (cfg_addr_c[3:2] != 2'd2);

  // A diagonal projection, scaled differently per view so the two views are
  // genuinely distinct. 256 and 192 are small, exact, and comfortably inside
  // signed 18 bits, so no product word can be refused.
  wire signed [31:0] scale_c = cfg_view_c ? 32'sd192 : 32'sd256;

  logic [31:0] cfg_data_c;
  always_comb begin
    unique case (cfg_addr_c)
      // row 0: x = scale*vx + tx
      5'd0:  cfg_data_c = scale_c;
      5'd1:  cfg_data_c = 32'd0;
      5'd2:  cfg_data_c = 32'd0;
      5'd3:  cfg_data_c = 32'sd4096;              // translation, full width
      // row 1: y = scale*vy + ty
      5'd4:  cfg_data_c = 32'd0;
      5'd5:  cfg_data_c = scale_c;
      5'd6:  cfg_data_c = 32'd0;
      5'd7:  cfg_data_c = 32'sd8192;              // translation, full width
      // row 2 is depth and keeps 32 bits at every MATW
      5'd8:  cfg_data_c = 32'd0;
      5'd9:  cfg_data_c = 32'd0;
      5'd10: cfg_data_c = 32'sd65536;
      5'd11: cfg_data_c = 32'sd131072;
      // row 3: w = scale*vz + tw
      5'd12: cfg_data_c = 32'd0;
      5'd13: cfg_data_c = 32'd0;
      5'd14: cfg_data_c = scale_c;
      5'd15: cfg_data_c = 32'sd16384;             // translation, full width
      // viewport origin: x0 = [11:0], y0 = [27:16]
      5'd16: cfg_data_c = 32'd0;
      // viewport extent: w = [11:0], h = [27:16]; distinct per view
      5'd17: cfg_data_c = cfg_view_c ? {4'd0, 12'd200, 4'd0, 12'd256}
                                     : {4'd0, 12'd240, 4'd0, 12'd320};
      default: cfg_data_c = 32'd0;
    endcase
  end

  // ---- job stream ----------------------------------------------------------
  logic [1:0] mask_rotor_q;     // 0 -> 01, 1 -> 10, 2 -> 11
  logic [1:0] job_view_mask_c;
  always_comb begin
    unique case (mask_rotor_q)
      2'd0:    job_view_mask_c = 2'b01;
      2'd1:    job_view_mask_c = 2'b10;
      default: job_view_mask_c = 2'b11;   // the headline dense case
    endcase
  end

  // AN OFFER HOLDS ITS PAYLOAD UNTIL IT IS TAKEN.
  //
  // The first version of this drove every job and vertex field straight from
  // the LFSR, which changes every clock -- so an offer that was not accepted
  // immediately presented a DIFFERENT job on the next edge, under the same
  // asserted valid. That breaks the ready/valid contract this repo enforces
  // everywhere else ("offers hold identity and payload under backpressure"),
  // and the terrain pipe noticed: legal-looking traffic produced an arena fill
  // fault, because the group it opened was not the group it went on to fill.
  //
  // Both streams now latch a payload and refresh it only when the transfer
  // actually fires.
  wire        job_valid_w = (setup_state_q == S_RUN);
  logic       job_ready_w;
  wire        job_fire_w = job_valid_w && job_ready_w;

  logic [5:0]  job_ox_q, job_oz_q;
  logic [1:0]  job_level_q;
  logic [16:0] job_morph_q;
  logic        job_surface_q, job_dual_q;
  logic [15:0] job_src_id_q;
  logic [7:0]  job_mat_a_q, job_mat_b_q, job_weight_q;

  // THE SUBPATCH GEOMETRY IS A ROM OF LEGAL JOBS, NOT RANDOM BITS.
  //
  // The first version drew ox/oz/level/morph from the LFSR, which produced
  // unaligned subpatch origins and groups the tessellator could not fill to
  // DEPTH. In DENSE_SEAL mode (VALID_MODE=1, the pipe's default) a seal is
  // REFUSED unless the arena holds exactly DEPTH vertices, and the refusal is
  // sticky on arena_seal_short_o -- so random geometry does not merely produce
  // odd pictures, it makes the machine report a fill fault and the fit would
  // have characterised a permanently faulted pipe.
  //
  // These eight are the shapes tests/terrain/terrain_pipe_differential.cpp
  // drives: origins on the 8-aligned subpatch grid, both surfaces, all four
  // levels, and morph values spanning the clamp. Neighbour levels are held
  // EQUAL to the patch level, so no stitched-plus-void combination appears --
  // that combination is legal and deliberately REJECTED by the pipe, which
  // would be activity without work.
  logic [2:0] job_rotor_q;
  logic [5:0]  job_rom_ox_c, job_rom_oz_c;
  logic [1:0]  job_rom_level_c;
  logic [16:0] job_rom_morph_c;
  logic        job_rom_surface_c, job_rom_dual_c;
  always_comb begin
    unique case (job_rotor_q)
      3'd0: begin job_rom_ox_c = 6'd0;  job_rom_oz_c = 6'd0;
                  job_rom_level_c = 2'd0; job_rom_morph_c = 17'h04000;
                  job_rom_surface_c = 1'b0; job_rom_dual_c = 1'b1; end
      3'd1: begin job_rom_ox_c = 6'd8;  job_rom_oz_c = 6'd0;
                  job_rom_level_c = 2'd0; job_rom_morph_c = 17'h08000;
                  job_rom_surface_c = 1'b0; job_rom_dual_c = 1'b1; end
      3'd2: begin job_rom_ox_c = 6'd16; job_rom_oz_c = 6'd8;
                  job_rom_level_c = 2'd1; job_rom_morph_c = 17'h0FFFF;
                  job_rom_surface_c = 1'b0; job_rom_dual_c = 1'b1; end
      3'd3: begin job_rom_ox_c = 6'd8;  job_rom_oz_c = 6'd16;
                  job_rom_level_c = 2'd1; job_rom_morph_c = 17'h00001;
                  job_rom_surface_c = 1'b1; job_rom_dual_c = 1'b1; end
      3'd4: begin job_rom_ox_c = 6'd16; job_rom_oz_c = 6'd16;
                  job_rom_level_c = 2'd2; job_rom_morph_c = 17'h10001;
                  job_rom_surface_c = 1'b0; job_rom_dual_c = 1'b1; end
      3'd5: begin job_rom_ox_c = 6'd24; job_rom_oz_c = 6'd24;
                  job_rom_level_c = 2'd3; job_rom_morph_c = 17'h02000;
                  job_rom_surface_c = 1'b1; job_rom_dual_c = 1'b1; end
      3'd6: begin job_rom_ox_c = 6'd8;  job_rom_oz_c = 6'd8;
                  job_rom_level_c = 2'd1; job_rom_morph_c = 17'h03000;
                  job_rom_surface_c = 1'b0; job_rom_dual_c = 1'b1; end
      default: begin job_rom_ox_c = 6'd24; job_rom_oz_c = 6'd8;
                  job_rom_level_c = 2'd2; job_rom_morph_c = 17'h00800;
                  job_rom_surface_c = 1'b1; job_rom_dual_c = 1'b1; end
    endcase
  end

  // DENSE_SEAL is the pipe's default valid mode and this wrapper characterises
  // it, so the sparse-fill path is deliberately not taken: a sparse fill in a
  // dense arena is what arena_seal_short_o exists to refuse.
  wire sparse_fill_c = 1'b0;

  // ---- geometry stream A ---------------------------------------------------
  logic       a_valid_q;
  logic       a_ready_w;
  wire        a_fire_w = a_valid_q && a_ready_w;
  logic signed [31:0] a_vx_q, a_vy_q, a_vz_q;
  logic       a_view_q;
  logic [G8B_PAYLOAD_A_W-1:0] a_payload_q;

  // ---- lattice and corner responses, REGISTERED BY ONE CLOCK ---------------
  // Never answered combinationally: a memory that replies in the same cycle it
  // is asked hides exactly the timing this fit is meant to measure.
  logic        lat_req_w;
  logic [5:0]  lat_vi_w, lat_vj_w;
  logic        lat_surface_w;
  logic signed [31:0] lat_h_q, lat_wx_q, lat_wz_q;

  logic        cs_req_w;
  logic [4:0]  cs_ci_w, cs_cj_w;
  logic [1:0]  cs_substance_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      lat_h_q  <= '0;
      lat_wx_q <= '0;
      lat_wz_q <= '0;
      cs_substance_q <= 2'd0;
    end else begin
      // A deterministic height field of the requested coordinate, so a replay
      // of the same lattice point answers the same way -- the pipe's three-copy
      // arena law depends on that and a random answer would break it.
      lat_h_q  <= $signed({{24{1'b0}}, lat_vi_w, lat_vj_w[1:0]});
      lat_wx_q <= $signed({{26{1'b0}}, lat_vi_w});
      lat_wz_q <= $signed({{26{1'b0}}, lat_vj_w});
      cs_substance_q <= cs_ci_w[1:0] ^ cs_cj_w[1:0];
    end
  end

  // ---- output sink ---------------------------------------------------------
  // Backpressure that actually varies, so held-output behaviour is in the
  // measured circuit rather than optimised away by a constant ready.
  wire out_ready_w = stimulus_lfsr_q[52] | stimulus_lfsr_q[53];

  logic        out_valid_w;
  logic signed [20:0] out_ax_w, out_ay_w, out_bx_w, out_by_w, out_cx_w, out_cy_w;
  logic [2:0]  out_behind_w;
  logic [15:0] out_src_id_w;
  logic signed [31:0] out_ad_w, out_bd_w, out_cd_w;
  logic [30:0] out_aw_w, out_bw_w, out_cw_w;
  logic        out_view_w;
  logic [7:0]  out_mat_a_w, out_mat_b_w, out_weight_w;
  logic        out_refused_w, out_missed_w;

  logic        a_valid_o_w;
  logic signed [20:0] a_x_w, a_y_w;
  logic signed [31:0] a_d_w;
  logic [30:0] a_w_w;
  logic        a_behind_w, a_view_o_w;
  // The depth profile (commit ac4f293d added both outputs to the pipe). They
  // ride the padding of word 39 so the fitter keeps the profile register;
  // an unread output here would be pruned and the row would under-count it.
  logic [1:0]  a_profile_w, fill_profile_w;
  logic [G8B_PAYLOAD_A_W-1:0] a_payload_o_w;

  logic        idle_w;
  logic [G8B_ARENAS-1:0] held_w;
  logic [31:0] jobs_accepted_w, jobs_no_view_w, jobs_rejected_w, jobs_empty_w;
  logic [31:0] groups_opened_w, groups_released_w;
  logic [31:0] fills_forwarded_w, fills_dropped_w, refs_forwarded_w;
  logic [31:0] release_unsafe_w;
  logic [31:0] tess_vertices_w, tess_refs_w, tess_rejected_w;
  logic [31:0] tess_lod_clamped_w, tess_mode_invalid_w;
  logic [31:0] replay_triangles_w, replay_refused_w, replay_missed_w;
  logic [31:0] corner_hits_w, corner_refusals_w, corner_misses_w;
  logic        arena_overflow_w, arena_seal_short_w;
  logic [31:0] a_grants_w, b_grants_w, contended_w, mat_refused_w;

  // ---- local activity witnesses -------------------------------------------
  logic [31:0] cfg_writes_q, jobs_offered_q, out_words_q, out_stalls_q;
  logic [2:0]  masks_seen_q;    // bit0 = 01, bit1 = 10, bit2 = 11
  logic        saw_dense_output_q;

  wire out_fire_w = out_valid_w && out_ready_w;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      setup_state_q <= S_CFG;
      cfg_index_q <= 6'd0;
      warm_count_q <= 8'd0;
      mask_rotor_q <= 2'd0;
      fit_signature_o <= 8'h01;
      fit_epoch_o <= 8'h00;
      stimulus_lfsr_q <= 64'h9e37_79b9_7f4a_7c15;
      signature_misr_q <= 32'h0000_0001;
      signature_source_q <= 6'd0;
      signature_word_q <= 32'd0;
      cfg_writes_q <= 32'd0;
      jobs_offered_q <= 32'd0;
      out_words_q <= 32'd0;
      out_stalls_q <= 32'd0;
      masks_seen_q <= 3'd0;
      saw_dense_output_q <= 1'b0;
      job_ox_q <= 6'd0; job_oz_q <= 6'd0; job_level_q <= 2'd0;
      job_morph_q <= 17'd0; job_surface_q <= 1'b0; job_dual_q <= 1'b0;
      job_src_id_q <= 16'd0; job_mat_a_q <= 8'd0; job_mat_b_q <= 8'd0;
      job_weight_q <= 8'd0; job_rotor_q <= 3'd0;
      a_valid_q <= 1'b0; a_vx_q <= '0; a_vy_q <= '0; a_vz_q <= '0;
      a_view_q <= 1'b0; a_payload_q <= '0;
    end else begin
      fit_signature_o <= signature_misr_q[7:0] ^ signature_misr_q[15:8]
                       ^ signature_misr_q[23:16] ^ signature_misr_q[31:24];
      fit_epoch_o <= {2'b00, signature_source_q};
      stimulus_lfsr_q <= {stimulus_lfsr_q[62:0],
          stimulus_lfsr_q[63] ^ stimulus_lfsr_q[62] ^
          stimulus_lfsr_q[60] ^ stimulus_lfsr_q[59]};
      signature_word_q <= signature_word_c;
      signature_misr_q <= {signature_misr_q[30:0], 1'b0}
                        ^ (signature_misr_q[31] ? 32'h0040_0007 : 32'd0)
                        ^ signature_word_q;
      signature_source_q <= signature_source_q + 6'd1;

      unique case (setup_state_q)
        S_CFG: begin
          cfg_writes_q <= cfg_writes_q + 32'd1;
          if (cfg_index_q == 6'd35) begin
            setup_state_q <= S_WARM;
            cfg_index_q <= 6'd0;
          end else begin
            cfg_index_q <= cfg_index_q + 6'd1;
          end
        end
        S_WARM: begin
          // One quiet window after the last configuration write, so no job can
          // race a matrix that is still being loaded.
          if (warm_count_q == 8'd15) setup_state_q <= S_RUN;
          else warm_count_q <= warm_count_q + 8'd1;
        end
        default: begin
          if (job_valid_w) jobs_offered_q <= jobs_offered_q + 32'd1;
          if (job_fire_w) begin
            masks_seen_q <= masks_seen_q | (3'd1 << mask_rotor_q);
            mask_rotor_q <= (mask_rotor_q == 2'd2) ? 2'd0 : (mask_rotor_q + 2'd1);
            job_rotor_q <= job_rotor_q + 3'd1;
            // A new job only after the previous one was taken.
            job_ox_q      <= job_rom_ox_c;
            job_oz_q      <= job_rom_oz_c;
            job_level_q   <= job_rom_level_c;
            job_morph_q   <= job_rom_morph_c;
            job_surface_q <= job_rom_surface_c;
            job_dual_q    <= job_rom_dual_c;
            // Identity and material vary freely: nothing downstream constrains
            // them, and varying them keeps the payload paths in the circuit.
            job_src_id_q  <= stimulus_lfsr_q[48:33];
            job_mat_a_q   <= stimulus_lfsr_q[7:0];
            job_mat_b_q   <= stimulus_lfsr_q[15:8];
            job_weight_q  <= stimulus_lfsr_q[23:16];
          end

          // Stream A: hold the vertex until it is accepted, then offer another.
          if (!a_valid_q || a_fire_w) begin
            a_valid_q   <= stimulus_lfsr_q[50];
            a_vx_q      <= {{20{stimulus_lfsr_q[11]}}, stimulus_lfsr_q[11:0]};
            a_vy_q      <= {{20{stimulus_lfsr_q[23]}}, stimulus_lfsr_q[23:12]};
            a_vz_q      <= {{20{stimulus_lfsr_q[35]}}, stimulus_lfsr_q[35:24]};
            a_view_q    <= stimulus_lfsr_q[36];
            a_payload_q <= stimulus_lfsr_q[51:36];
          end
          if (out_fire_w) begin
            out_words_q <= out_words_q + 32'd1;
            if (out_behind_w == 3'd0) saw_dense_output_q <= 1'b1;
          end
          if (out_valid_w && !out_ready_w) out_stalls_q <= out_stalls_q + 32'd1;
        end
      endcase
    end
  end

  always_comb begin
    unique case (signature_source_q)
      6'd0:  signature_word_c = jobs_accepted_w;
      6'd1:  signature_word_c = jobs_no_view_w;
      6'd2:  signature_word_c = jobs_rejected_w;
      6'd3:  signature_word_c = jobs_empty_w;
      6'd4:  signature_word_c = groups_opened_w;
      6'd5:  signature_word_c = groups_released_w;
      6'd6:  signature_word_c = fills_forwarded_w;
      6'd7:  signature_word_c = fills_dropped_w;
      6'd8:  signature_word_c = refs_forwarded_w;
      6'd9:  signature_word_c = release_unsafe_w;
      6'd10: signature_word_c = tess_vertices_w;
      6'd11: signature_word_c = tess_refs_w;
      6'd12: signature_word_c = tess_rejected_w;
      6'd13: signature_word_c = tess_lod_clamped_w;
      6'd14: signature_word_c = tess_mode_invalid_w;
      6'd15: signature_word_c = replay_triangles_w;
      6'd16: signature_word_c = replay_refused_w;
      6'd17: signature_word_c = replay_missed_w;
      6'd18: signature_word_c = corner_hits_w;
      6'd19: signature_word_c = corner_refusals_w;
      6'd20: signature_word_c = corner_misses_w;
      6'd21: signature_word_c = a_grants_w;
      6'd22: signature_word_c = b_grants_w;
      6'd23: signature_word_c = contended_w;
      6'd24: signature_word_c = mat_refused_w;
      6'd25: signature_word_c = {out_ax_w, out_ay_w[10:0]};
      6'd26: signature_word_c = {out_bx_w, out_by_w[10:0]};
      6'd27: signature_word_c = {out_cx_w, out_cy_w[10:0]};
      6'd28: signature_word_c = out_ad_w;
      6'd29: signature_word_c = out_bd_w;
      6'd30: signature_word_c = out_cd_w;
      6'd31: signature_word_c = {1'b0, out_aw_w};
      6'd32: signature_word_c = {1'b0, out_bw_w};
      6'd33: signature_word_c = {1'b0, out_cw_w};
      6'd34: signature_word_c = {out_src_id_w, out_mat_a_w, out_mat_b_w};
      // 8 + 5 + 3 + 1 + 1 + 1 + 1 + 1 + 11 = 32.
      6'd35: signature_word_c = {out_weight_w, 5'd0, out_behind_w, out_view_w,
                                 out_refused_w, out_missed_w,
                                 out_valid_w, out_ready_w,
                                 11'd0};
      6'd36: signature_word_c = {a_x_w, a_y_w[10:0]};
      6'd37: signature_word_c = a_d_w;
      6'd38: signature_word_c = {1'b0, a_w_w};
      6'd39: signature_word_c = {a_payload_o_w, 8'd0, a_profile_w, fill_profile_w, a_valid_o_w, a_behind_w,
                                 a_view_o_w, a_ready_w};
      6'd40: signature_word_c = {28'd0, held_w};
      6'd41: signature_word_c = {29'd0, idle_w, arena_overflow_w,
                                 arena_seal_short_w};
      6'd42: signature_word_c = cfg_writes_q;
      6'd43: signature_word_c = jobs_offered_q;
      6'd44: signature_word_c = out_words_q;
      6'd45: signature_word_c = out_stalls_q;
      6'd46: signature_word_c = {26'd0, masks_seen_q, saw_dense_output_q,
                                 job_view_mask_c};
      6'd47: signature_word_c = stimulus_lfsr_q[31:0];
      default: signature_word_c = stimulus_lfsr_q[63:32];
    endcase
  end

  // ---- the design under characterization -----------------------------------
  // ROWS_PER_PASS and MATW are LITERALS here. The architecture forbids
  // inheriting them from the module default, a fit-target comment or a runtime
  // convention, because a fit that silently characterised MATW=32 would look
  // exactly like one that characterised MATW=18.
  zhao_terrain_pipe #(
      .PAYLOAD_A_W  (G8B_PAYLOAD_A_W),
      .ROWS_PER_PASS(3),
      .MATW         (18),
      .ARENAS       (G8B_ARENAS)
  ) u_terrain_pipe (
      .clk(clk), .rst_n(rst_n),
      .cfg_we_i(cfg_we_c), .cfg_view_i(cfg_view_c),
      .cfg_addr_i(cfg_addr_c), .cfg_data_i(cfg_data_c),
      .en_i(setup_state_q == S_RUN),

      .a_valid_i(a_valid_q), .a_ready_o(a_ready_w),
      .a_vx_i(a_vx_q), .a_vy_i(a_vy_q), .a_vz_i(a_vz_q),
      .a_view_i(a_view_q), .a_payload_i(a_payload_q),
      .a_valid_o(a_valid_o_w), .a_x_o(a_x_w), .a_y_o(a_y_w), .a_d_o(a_d_w),
      .a_w_o(a_w_w), .a_behind_o(a_behind_w), .a_view_o(a_view_o_w),
      .a_payload_o(a_payload_o_w),
      .a_profile_o(a_profile_w), .fill_profile_o(fill_profile_w),

      .job_valid_i(job_valid_w), .job_ready_o(job_ready_w),
      .job_ox_i(job_ox_q), .job_oz_i(job_oz_q), .job_level_i(job_level_q),
      .job_lvl_nz_i(job_level_q), .job_lvl_pz_i(job_level_q),
      .job_lvl_nx_i(job_level_q), .job_lvl_px_i(job_level_q),
      .job_morph_i(job_morph_q), .job_surface_i(job_surface_q),
      .job_dual_i(job_dual_q), .job_src_id_i(job_src_id_q),
      .job_view_mask_i(job_view_mask_c),
      .job_mat_a_i(job_mat_a_q), .job_mat_b_i(job_mat_b_q),
      .job_weight_i(job_weight_q), .sparse_fill_i(sparse_fill_c),

      .lat_req_o(lat_req_w), .lat_vi_o(lat_vi_w), .lat_vj_o(lat_vj_w),
      .lat_surface_o(lat_surface_w),
      .lat_h_i(lat_h_q), .lat_wx_i(lat_wx_q), .lat_wz_i(lat_wz_q),

      .cs_req_o(cs_req_w), .cs_ci_o(cs_ci_w), .cs_cj_o(cs_cj_w),
      .cs_substance_i(cs_substance_q),

      .out_valid_o(out_valid_w), .out_ready_i(out_ready_w),
      .out_ax_o(out_ax_w), .out_ay_o(out_ay_w),
      .out_bx_o(out_bx_w), .out_by_o(out_by_w),
      .out_cx_o(out_cx_w), .out_cy_o(out_cy_w),
      .out_behind_o(out_behind_w), .out_src_id_o(out_src_id_w),
      .out_ad_o(out_ad_w), .out_bd_o(out_bd_w), .out_cd_o(out_cd_w),
      .out_aw_o(out_aw_w), .out_bw_o(out_bw_w), .out_cw_o(out_cw_w),
      .out_view_o(out_view_w), .out_mat_a_o(out_mat_a_w),
      .out_mat_b_o(out_mat_b_w), .out_weight_o(out_weight_w),
      .out_refused_o(out_refused_w), .out_missed_o(out_missed_w),

      .idle_o(idle_w), .held_o(held_w),
      .jobs_accepted_o(jobs_accepted_w), .jobs_no_view_o(jobs_no_view_w),
      .jobs_rejected_o(jobs_rejected_w), .jobs_empty_o(jobs_empty_w),
      .groups_opened_o(groups_opened_w),
      .groups_released_o(groups_released_w),
      .fills_forwarded_o(fills_forwarded_w),
      .fills_dropped_o(fills_dropped_w),
      .refs_forwarded_o(refs_forwarded_w),
      .release_unsafe_o(release_unsafe_w),
      .tess_vertices_o(tess_vertices_w), .tess_refs_o(tess_refs_w),
      .tess_rejected_o(tess_rejected_w),
      .tess_lod_clamped_o(tess_lod_clamped_w),
      .tess_mode_invalid_o(tess_mode_invalid_w),
      .replay_triangles_o(replay_triangles_w),
      .replay_refused_o(replay_refused_w),
      .replay_missed_o(replay_missed_w),
      .corner_hits_o(corner_hits_w), .corner_refusals_o(corner_refusals_w),
      .corner_misses_o(corner_misses_w),
      .arena_overflow_o(arena_overflow_w),
      .arena_seal_short_o(arena_seal_short_w),
      .a_grants_o(a_grants_w), .b_grants_o(b_grants_w),
      .contended_o(contended_w), .mat_refused_o(mat_refused_w));

`ifndef SYNTHESIS
`ifndef QUARTUS_SYNTHESIS
  // ARMED AFTER RESET, not merely gated on rst_n. The counters these watch are
  // only meaningful once the design has actually left reset and begun running;
  // checking them on the first edge reports the reset value of an unstarted
  // machine, which is what the first version of this did.
  logic assert_armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) assert_armed_q <= 1'b0;
    else        assert_armed_q <= (setup_state_q == S_RUN);
  end

  // THE ONE THING THAT WOULD MAKE THIS FIT MEANINGLESS. If a product word is
  // ever refused, the matrix never loaded, the projector is running on reset
  // values, and the ALM/Fmax numbers describe a machine nobody asked for.
  always_ff @(posedge clk) begin
    if (rst_n && assert_armed_q) begin
      assert (mat_refused_w == 32'd0)
        else $error("g8b_fit_top: MATW=18 refused a product word; the matrix never loaded and this fit would characterise an unconfigured projector");
      assert (!(arena_overflow_w || arena_seal_short_w))
        else $error("g8b_fit_top: legal traffic produced an arena fill fault");
    end
  end
`endif
`endif

`ifndef QUARTUS_SYNTHESIS
  export "DPI-C" task zhao_g8b_get_activity;
  task zhao_g8b_get_activity(
      output int unsigned cfg_writes_o,
      output int unsigned jobs_offered_o,
      output int unsigned jobs_accepted_out,
      output int unsigned out_words_o,
      output int unsigned out_stalls_o,
      output int unsigned tess_vertices_out,
      output int unsigned replay_triangles_out,
      output int unsigned a_grants_out,
      output int unsigned b_grants_out,
      output int unsigned contended_out,
      output int unsigned mat_refused_out,
      output bit [2:0]    masks_seen_o,
      output bit          saw_dense_output_o,
      output bit          arena_fault_o,
      output bit          idle_out);
    cfg_writes_o = cfg_writes_q;
    jobs_offered_o = jobs_offered_q;
    jobs_accepted_out = jobs_accepted_w;
    out_words_o = out_words_q;
    out_stalls_o = out_stalls_q;
    tess_vertices_out = tess_vertices_w;
    replay_triangles_out = replay_triangles_w;
    a_grants_out = a_grants_w;
    b_grants_out = b_grants_w;
    contended_out = contended_w;
    mat_refused_out = mat_refused_w;
    masks_seen_o = masks_seen_q;
    saw_dense_output_o = saw_dense_output_q;
    arena_fault_o = arena_overflow_w || arena_seal_short_w;
    idle_out = idle_w;
  endtask
`endif

endmodule

`default_nettype wire
