// zhao_geom_vdecode.sv — the format-0 RECORD decoder. Bytes into one vertex.
//
// SCOPE, BEFORE ANYTHING ELSE: this is the 32-byte record layout of format 0
// and NOT the whole of GEOM.VDECODE. That contract owns a batch engine --
// `vertex_count`, memory addressing across burst boundaries, "a batch either
// emits vertex_count vertices or none", and a refusal taxonomy at the batch
// level. This block is the leaf that engine will decode each record with, in
// the same relation `zhao_part_record` has to PART.STATE.
//
// The ledger entry for GEOM.VDECODE therefore stays SPECIFIED. Advancing it on
// the strength of this file would claim the batch engine exists.
//
// ---------------------------------------------------------------------------
// WHY THIS FORMAT AND ONLY THIS FORMAT
// ---------------------------------------------------------------------------
// Owner ruling, 2026-08-31 §6.2, restated by R11:
//
//   > Write the contract and land RAW/CANONICAL format 0 first. Packed rigid
//   > format 1 and two-weight skinned format 2 are additive formats chosen
//   > after an asset bake-off. DO NOT BLOCK THE GEOMETRY PATH ON PERFECT
//   > COMPRESSION.
//
// This block could not previously be specified because the compressed vertex
// format was not pinned anywhere in `spec/`. The ruling's answer was not to pin
// one -- it was to make format 0 UNCOMPRESSED, so the geometry path can be
// built and measured while compression is still open.
//
// **Format 0 is therefore not a placeholder.** It is the permanent fallback and
// the differential reference: every later format must decode to bit-identical
// output for the same source mesh, and this is what "the same" is measured
// against. Formats 1 and 2 are BAKE-OFF GATED and no RTL for them is
// authorised until that comparison exists.
//
// ---------------------------------------------------------------------------
// THE LAYOUT, 32 BYTES, NATURALLY ALIGNED
// ---------------------------------------------------------------------------
//   off  0  12  position s32 x3, fx16 S15.16
//   off 12   3  normal s8 x3, the packed bind normal the cel path uses
//   off 15   1  w0 in 1/64 quanta (64 = rigid)
//   off 16   4  UV, 2 x s16 fx16
//   off 20   2  bone0
//   off 22   2  bone1
//   off 24   8  reserved, MUST BE ZERO
//
// The reserved eight bytes are what formats 1 and 2 grow into without changing
// the stride. Requiring them zero is what stops an older decoder silently
// reading a newer file.
//
// A MALFORMED RECORD IS REFUSED, NOT FLAGGED AND EMITTED. The first version of
// this block emitted the vertex with a flag raised, on the argument that a
// dropped vertex is a mesh with a hole while a flagged one is a mesh with a
// flag. That argument is not mine to make: GEOM.VDECODE.md already ratifies
// the refusal taxonomy -- "each of the 8 reserved bytes nonzero in turn: 8
// refusals" and "unknown format_id: refused, and NO VERTEX EMITTED -- the
// safety case". The contract is the decision; a comment in an implementation
// arguing with it is how a block quietly stops being the thing it is named
// after.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS NOT
// ---------------------------------------------------------------------------
// It does not skin, does not project, does not fetch descriptors (that is
// GEOM.MESHFETCH, which hands it offsets) and does not choose LOD.
//
// And normals decoded here TRAVEL A PARALLEL PATH. `zhao_geom_skin` outputs
// positions and not normals, and nothing may be bolted onto its output
// (`reports/CREATURESANDLIGHTS`), so the normal leaves this block beside the
// position rather than inside it.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_geom_vdecode #(
    parameter int unsigned SRCW = 16
) (
    input var logic clk,
    input var logic rst_n,

    // ---- one 32-byte vertex record -------------------------------------------
    input  var logic                 v_valid_i,
    output var logic                 v_ready_o,
    input  var logic [255:0]         v_bytes_i,     // little-endian, off 0 at bit 0
    input  var logic [2:0]           v_format_i,    // must be 0
    input  var logic [SRCW-1:0]      v_src_id_i,

    // ---- the decoded vertex --------------------------------------------------
    output var logic                 d_valid_o,
    input  var logic                 d_ready_i,
    output var logic signed [31:0]   d_x_o,
    output var logic signed [31:0]   d_y_o,
    output var logic signed [31:0]   d_z_o,
    output var logic signed [7:0]    d_nx_o,
    output var logic signed [7:0]    d_ny_o,
    output var logic signed [7:0]    d_nz_o,
    output var logic [6:0]           d_w0_o,        // 1/64 quanta, 64 == rigid
    output var logic                 d_rigid_o,     // bone1 == bone0
    output var logic signed [15:0]   d_u_o,
    output var logic signed [15:0]   d_v_o,
    output var logic [15:0]          d_bone0_o,
    output var logic [15:0]          d_bone1_o,
    output var logic [SRCW-1:0]      d_src_id_o,
    // The refusal, raised INSTEAD of `d_valid_o` on the same clock, so a caller
    // that can see WHICH record was refused can name the asset. Exactly one of
    // the two is high for each accepted record.
    output var logic                 d_refused_o,
    output var logic                 d_reserved_nz_o,
    output var logic                 d_w0_illegal_o,
    output var logic                 d_format_bad_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]          vertices_o,
    output var logic [31:0]          reserved_nz_o,
    output var logic [31:0]          w0_illegal_o,
    output var logic [31:0]          format_bad_o
);

  // ---- field extraction, one place ----------------------------------------
  logic signed [31:0] x_c, y_c, z_c;
  logic signed [7:0]  nx_c, ny_c, nz_c;
  logic [7:0]         w0_raw_c;
  logic signed [15:0] u_c, v_c;
  logic [15:0]        b0_c, b1_c;
  logic               resv_nz_c, w0_bad_c, fmt_bad_c;

  always_comb begin
    x_c      = $signed(v_bytes_i[  0 +: 32]);
    y_c      = $signed(v_bytes_i[ 32 +: 32]);
    z_c      = $signed(v_bytes_i[ 64 +: 32]);
    nx_c     = $signed(v_bytes_i[ 96 +:  8]);
    ny_c     = $signed(v_bytes_i[104 +:  8]);
    nz_c     = $signed(v_bytes_i[112 +:  8]);
    w0_raw_c = v_bytes_i[120 +: 8];
    u_c      = $signed(v_bytes_i[128 +: 16]);
    v_c      = $signed(v_bytes_i[144 +: 16]);
    b0_c     = v_bytes_i[160 +: 16];
    b1_c     = v_bytes_i[176 +: 16];

    // The reserved eight bytes, checked as one OR rather than eight compares.
    resv_nz_c = |v_bytes_i[192 +: 64];

    // `w0` is legal in [0, 64]. 64 means rigid; anything above is a MALFORMED
    // ASSET, not a saturating weight. Saturating it here would turn a bad file
    // into a silently wrong skin, which is the difference this flag exists for.
    w0_bad_c = (w0_raw_c > 8'd64);

    fmt_bad_c = (v_format_i != 3'd0);
  end

  // Straight-through, one vertex per clock, with a single registered stage so
  // the consumer sees a held packet rather than a view of the input bus.
  assign v_ready_o = !d_valid_o || d_ready_i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      d_valid_o     <= 1'b0;
      d_refused_o   <= 1'b0;
      vertices_o    <= '0;
      reserved_nz_o <= '0;
      w0_illegal_o  <= '0;
      format_bad_o  <= '0;
    end else begin
      if (!d_valid_o || d_ready_i) begin
        // REFUSED records emit nothing. `d_valid_o` is the batch engine's
        // guarantee that what it receives is decodable.
        // Set together, so `d_valid_o` and `d_refused_o` are never both high
        // and never both stale: exactly one of them describes each accepted
        // record, and neither survives into a clock with no record.
        d_valid_o   <= v_valid_i && !(resv_nz_c || w0_bad_c || fmt_bad_c);
        d_refused_o <= v_valid_i &&  (resv_nz_c || w0_bad_c || fmt_bad_c);
        if (v_valid_i) begin
          d_x_o     <= x_c;
          d_y_o     <= y_c;
          d_z_o     <= z_c;
          d_nx_o    <= nx_c;
          d_ny_o    <= ny_c;
          d_nz_o    <= nz_c;
          // Seven bits carry 0..64 exactly. A wider value is refused above,
          // so what is latched here never reaches a consumer.
          d_w0_o    <= w0_raw_c[6:0];
          d_u_o     <= u_c;
          d_v_o     <= v_c;
          d_bone0_o <= b0_c;
          d_bone1_o <= b1_c;
          d_rigid_o <= (b1_c == b0_c);
          d_src_id_o<= v_src_id_i;

          d_reserved_nz_o <= resv_nz_c;
          d_w0_illegal_o  <= w0_bad_c;
          d_format_bad_o  <= fmt_bad_c;

          if (!(resv_nz_c || w0_bad_c || fmt_bad_c))
            vertices_o <= vertices_o + 32'd1;
          if (resv_nz_c) reserved_nz_o <= reserved_nz_o + 32'd1;
          if (w0_bad_c)  w0_illegal_o  <= w0_illegal_o  + 32'd1;
          if (fmt_bad_c) format_bad_o  <= format_bad_o  + 32'd1;
        end
      end
    end
  end

endmodule : zhao_geom_vdecode

`default_nettype wire
