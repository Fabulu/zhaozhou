// zhao_geom_pose_decode.sv — the per-bone pose decode chain.
//
// GEOM.POSE's decode half (design/contracts/GEOM.POSE.md). The cache, the slot
// eviction policy and the clip-page fetch are NOT here; this block turns one
// (clip, frame) into one palette of bone matrices, which is the work a cache
// miss has to do.
//
// Reference: `zref::creature::decode_pose`
// (reference/src/zcreature/creature_core.cpp:133).
//
// ---------------------------------------------------------------------------
// THE CHAIN, per bone b, in the reference's own order
// ---------------------------------------------------------------------------
//     R    = quat16_to_mat3(quats[frame][b])
//     LR   = R, with m[3]/m[7]/m[11] += rest translation,
//            plus the clip's root displacement at b == 0 only
//     A_b  = (b == 0) ? LR : A_parent * LR
//     S_b  = A_b * inv_rest[b]          <-- the emitted palette entry
//
// Bones are decoded in index order, which is safe because the skeleton bake
// VALIDATES parent-before-child (`bake_skeleton`). `A_parent` is therefore
// always already written when bone b reads it. This block does not re-check
// that ordering; a malformed skeleton would read a stale ancestor rather than
// hang, and the validation lives at bake time where it can reject the asset.
//
// `inv_rest` is supplied, not computed. The reference bakes it at load as a
// pure negated translation — "exact because rest rotations are identity" — so
// there is no matrix inversion anywhere in this path and no inverter in this
// block.
//
// ---------------------------------------------------------------------------
// WHY IT LOOKS EXPENSIVE AND IS NOT
// ---------------------------------------------------------------------------
// One quaternion engine and ONE multiply engine, time-shared across both
// multiplies of every bone. The multiply engine is itself sequential (twelve
// elements, three products each), so the whole block carries three 32x32
// multipliers rather than the seventy-two a fully parallel chain would need.
//
// That matters because the project is already over its DSP budget (171 against
// 112). The cost is paid in cycles: roughly 1 + 13 + 12 + 12 + 12 per bone, so
// about 1,600 cycles for a full 32-bone palette.
//
// This is affordable for the same reason the sequential multiply was:
// `spec/creature_rules.md` §2.2 rejected baking every pose at load (x6 memory),
// so a decode is a cache MISS cost, paid once per (type, clip, frame) and
// shared by every instance of that type on that frame — not per instance and
// not per frame.
//
// THE ANCESTOR STORE. `A_b` for every decoded bone must stay readable until the
// last child is done, so 32 bones x 12 elements x 32 bits has to live
// somewhere. As flip-flops that is 12,288 registers — about 15% of the device's
// entire FF budget for one block's scratch space. It is a memory instead, read
// one element per cycle. That is the whole reason the parent read costs
// thirteen cycles, and it is a deliberate trade of latency (which this path
// has) for registers (which the project does not).
//
// M10K INFERENCE RULES, followed deliberately: no initializer, no reset branch
// touching the array, and the read happens ONLY inside the clocked process
// (registered read). A combinational read or an async reset on the array pushes
// Quartus into logic cells and the block stops fitting — a failure this project
// has already paid for once in TEXTURE.CACHE.
module zhao_geom_pose_decode #(
    parameter int MAX_BONES = 32
) (
    input  logic clk,
    input  logic rst_n,

    // ---- start one palette decode -----------------------------------------
    input  logic               start_i,
    output logic               busy_o,
    input  logic        [ 5:0] bone_count_i,   // 0..MAX_BONES
    input  logic signed [31:0] root_dx_i,      // clip root displacement, bone 0 only
    input  logic signed [31:0] root_dy_i,
    input  logic signed [31:0] root_dz_i,

    // ---- per-bone source data, addressed by this block --------------------
    // Combinational fetch: the caller must present the data for `bone_idx_o`
    // in the SAME cycle. The caller owns the skeleton, the clip page and the
    // bake; this block owns none of them and holds no cache.
    output logic        [ 4:0] bone_idx_o,
    input  logic        [ 4:0] bone_parent_i,
    input  logic signed [31:0] bone_tx_i,
    input  logic signed [31:0] bone_ty_i,
    input  logic signed [31:0] bone_tz_i,
    input  logic signed [15:0] quat_w_i,
    input  logic signed [15:0] quat_x_i,
    input  logic signed [15:0] quat_y_i,
    input  logic signed [15:0] quat_z_i,
    input  logic signed [31:0] inv_rest_i [12],

    // ---- the palette out, one bone per beat -------------------------------
    output logic               out_valid_o,
    input  logic               out_ready_i,
    output logic        [ 4:0] out_bone_o,
    output logic signed [31:0] out_m_o [12],

    output logic               done_o,
    output logic [31:0] palettes_decoded_o
);

  localparam int ELEMS = 12;
  localparam int AMEM_W = 32;
  localparam int AMEM_D = MAX_BONES * ELEMS;

  typedef enum logic [3:0] {
    S_IDLE,
    S_FETCH,
    S_QUAT,
    S_PREAD,
    S_BONE0,
    S_MUL1,
    S_MUL1_WAIT,
    S_STORE,
    S_MUL2,
    S_MUL2_WAIT,
    S_EMIT,
    S_DONE
  } state_e;

  state_e state;

  logic [5:0] b;        // bone cursor
  logic [4:0] k;        // element cursor, 0..12
  logic [5:0] count_q;
  logic [4:0] parent_q;
  logic signed [31:0] tx_q, ty_q, tz_q;
  logic signed [31:0] lr   [ELEMS];
  logic signed [31:0] pa   [ELEMS];
  logic signed [31:0] acur [ELEMS];
  logic signed [31:0] invq [ELEMS];
  logic signed [31:0] res  [ELEMS];

  assign bone_idx_o = b[4:0];
  assign busy_o = (state != S_IDLE);

  // ---- the ancestor store -------------------------------------------------
  // Deliberately plain: no initializer, no reset, read only in the clocked
  // process. See the M10K note in the header.
  logic signed [AMEM_W-1:0] a_mem [0:AMEM_D-1];
  logic [$clog2(AMEM_D)-1:0] a_raddr, a_waddr;
  logic signed [AMEM_W-1:0] a_rdata, a_wdata;
  logic a_we;

  always_ff @(posedge clk) begin
    if (a_we) a_mem[a_waddr] <= a_wdata;
    a_rdata <= a_mem[a_raddr];
  end

  // ---- the quaternion engine ----------------------------------------------
  logic q_valid, q_ready, qm_valid, qm_ready;
  logic signed [31:0] qm_m [12];
  logic [7:0] qm_bone_unused;
  logic [31:0] q_count_unused;

  zhao_geom_quat2mat u_quat (
      .clk(clk),
      .rst_n(rst_n),
      .q_valid_i(q_valid),
      .q_ready_o(q_ready),
      .q_w_i(quat_w_i),
      .q_x_i(quat_x_i),
      .q_y_i(quat_y_i),
      .q_z_i(quat_z_i),
      .q_bone_i(8'(b)),
      .m_valid_o(qm_valid),
      .m_ready_i(qm_ready),
      .m_o(qm_m),
      .m_bone_o(qm_bone_unused),
      .bones_decoded_o(q_count_unused)
  );

  // ---- the single shared multiply engine ----------------------------------
  logic mul_in_valid, mul_in_ready, mul_out_valid, mul_out_ready;
  logic signed [31:0] mul_a [12];
  logic signed [31:0] mul_b [12];
  logic signed [31:0] mul_out [12];
  logic [7:0] mul_tag_unused;
  logic [31:0] mul_count_unused;

  // MUL1 is A_parent * LR; MUL2 is A_b * inv_rest. Both feed the same engine,
  // which is the point of the time-share.
  logic sel_mul2;
  always_comb begin
    for (int i = 0; i < ELEMS; i++) begin
      mul_a[i] = sel_mul2 ? acur[i] : pa[i];
      mul_b[i] = sel_mul2 ? invq[i] : lr[i];
    end
  end

  zhao_geom_mat3x4_mul u_mul (
      .clk(clk),
      .rst_n(rst_n),
      .in_valid_i(mul_in_valid),
      .in_ready_o(mul_in_ready),
      .a_m_i(mul_a),
      .b_m_i(mul_b),
      .in_tag_i(8'(b)),
      .out_valid_o(mul_out_valid),
      .out_ready_i(mul_out_ready),
      .out_m_o(mul_out),
      .out_tag_o(mul_tag_unused),
      .products_done_o(mul_count_unused)
  );

  always_comb begin
    q_valid = (state == S_FETCH);
    qm_ready = (state == S_QUAT);
    mul_in_valid = (state == S_MUL1) || (state == S_MUL2);
    mul_out_ready = (state == S_MUL1_WAIT) || (state == S_MUL2_WAIT);
    sel_mul2 = (state == S_MUL2) || (state == S_MUL2_WAIT);

    // k runs to 12 so the last registered read can be captured; the address
    // for that extra cycle is unused, and is clamped rather than allowed to
    // walk off the end of the array.
    a_raddr = ($clog2(AMEM_D))'(parent_q * ELEMS + ((k < 5'd12) ? 32'(k) : 32'd0));
    a_waddr = ($clog2(AMEM_D))'(b * ELEMS + k);
    a_wdata = acur[k[3:0]];
    a_we = (state == S_STORE) && (32'(k) < ELEMS);
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= S_IDLE;
      b <= '0;
      k <= '0;
      count_q <= '0;
      parent_q <= '0;
      tx_q <= '0; ty_q <= '0; tz_q <= '0;
      out_valid_o <= 1'b0;
      out_bone_o <= '0;
      done_o <= 1'b0;
      palettes_decoded_o <= '0;
      for (int i = 0; i < ELEMS; i++) begin
        lr[i] <= '0; pa[i] <= '0; acur[i] <= '0; invq[i] <= '0; res[i] <= '0;
        out_m_o[i] <= '0;
      end
    end else begin
      done_o <= 1'b0;
      if (out_valid_o && out_ready_i) out_valid_o <= 1'b0;

      unique case (state)
        S_IDLE: begin
          if (start_i) begin
            count_q <= bone_count_i;
            b <= '0;
            // A zero-bone palette is legal and decodes to nothing. Treating it
            // as an error would make an empty creature type a hang.
            state <= (bone_count_i == 6'd0) ? S_DONE : S_FETCH;
          end
        end

        // Latch this bone's source data and hand the quaternion to the engine.
        S_FETCH: begin
          parent_q <= bone_parent_i;
          tx_q <= bone_tx_i;
          ty_q <= bone_ty_i;
          tz_q <= bone_tz_i;
          for (int i = 0; i < ELEMS; i++) invq[i] <= inv_rest_i[i];
          if (q_valid && q_ready) state <= S_QUAT;
        end

        // R has landed; build LR by adding the rest translation, and the clip
        // root displacement at bone 0 only.
        S_QUAT: begin
          if (qm_valid) begin
            for (int i = 0; i < ELEMS; i++) lr[i] <= qm_m[i];
            lr[3]  <= qm_m[3]  + tx_q + ((b == 6'd0) ? root_dx_i : 32'sd0);
            lr[7]  <= qm_m[7]  + ty_q + ((b == 6'd0) ? root_dy_i : 32'sd0);
            lr[11] <= qm_m[11] + tz_q + ((b == 6'd0) ? root_dz_i : 32'sd0);
            k <= '0;
            state <= (b == 6'd0) ? S_BONE0 : S_PREAD;
          end
        end

        // Read A_parent one element per cycle. The store is a memory with a
        // registered read, so element k issues at cycle k and lands at k+1;
        // the cursor runs to 12 so the last one is captured.
        S_PREAD: begin
          if (k > 5'd0) pa[k - 1] <= a_rdata;
          if (k == 5'd12) begin
            state <= S_MUL1;
            k <= '0;
          end else begin
            k <= k + 5'd1;
          end
        end

        S_MUL1: begin
          if (mul_in_valid && mul_in_ready) state <= S_MUL1_WAIT;
        end

        // Bone 0 has no parent, so there is no A_parent to multiply by and the
        // reference assigns `a[0] = lr` outright.
        //
        // Note it is NOT a rounding argument: multiplying by an identity matrix
        // here is exactly lossless, since rescale(x * 65536, 16) == x and the
        // translation column passes through as (a[i][3] << 16) >> 16. The
        // reason is structural -- there is no parent matrix, and reading one
        // would read whatever the store last held.
        S_BONE0: begin
          for (int i = 0; i < ELEMS; i++) acur[i] <= lr[i];
          k <= '0;
          state <= S_STORE;
        end

        S_MUL1_WAIT: begin
          if (mul_out_valid) begin
            for (int i = 0; i < ELEMS; i++) acur[i] <= mul_out[i];
            k <= '0;
            state <= S_STORE;
          end
        end

        // Write A_b back for its children.
        S_STORE: begin
          if (k == 5'd11) begin
            k <= '0;
            state <= S_MUL2;
          end else begin
            k <= k + 5'd1;
          end
        end

        S_MUL2: begin
          if (mul_in_valid && mul_in_ready) state <= S_MUL2_WAIT;
        end

        S_MUL2_WAIT: begin
          if (mul_out_valid) begin
            for (int i = 0; i < ELEMS; i++) res[i] <= mul_out[i];
            state <= S_EMIT;
          end
        end

        // Offer S_b. The palette is a stream, so a slow consumer stalls the
        // decode rather than losing a bone.
        S_EMIT: begin
          if (!out_valid_o) begin
            for (int i = 0; i < ELEMS; i++) out_m_o[i] <= res[i];
            out_bone_o <= b[4:0];
            out_valid_o <= 1'b1;
          end else if (out_ready_i) begin
            if (b + 6'd1 == count_q) begin
              state <= S_DONE;
            end else begin
              b <= b + 6'd1;
              state <= S_FETCH;
            end
          end
        end

        S_DONE: begin
          done_o <= 1'b1;
          if (palettes_decoded_o != 32'hFFFF_FFFF) palettes_decoded_o <= palettes_decoded_o + 32'd1;
          state <= S_IDLE;
        end

        default: state <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_geom_pose_decode
