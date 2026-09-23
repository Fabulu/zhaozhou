// zhao_forge_fanindex.sv -- a RING becomes a TRIANGLE FAN, and nothing else.
//
// WHY THIS BLOCK EXISTS, AND WHY IT IS THE ONLY NEW GEOMETRY THE SHADOW
// SUBSYSTEM NEEDED (SHADOWRIDE, 2026-09-23)
// ---------------------------------------------------------------------------
// `zhao_forge_shadow` emits a CLOSED RING of 16 / 8 / 4 world vertices with
// `vtx_last_o` on the final one, and there is NO CENTRE VERTEX
// (`zhao_forge_shadow.sv:291-296`).  `zhao_forge_assemble` -- the block
// FORGE.PRIM already runs -- wants a world vertex stream AND an index stream
// over it, and it does everything else a shadow hull needs: it projects
// through client A on owner `2'd2`, makes its own `invw24` with its own
// `zhao_geom_depthquant_stream`, declares `o_untex_o = 1'b1` unconditionally
// (`:576`), carries a named per-primitive alpha, and lands its triangles on
// `zhao_geom_clipdoor`.  Its `v_*` port is SPECIFIED as a composer mux
// (`:199-204`) -- "muxed at the composer by family ... this block does not read
// the family and must not".  A shadow hull is another producer on that mux.
//
// So the whole of the missing geometry is this: a ring of N becomes N-2
// triangles, `(0,1,2)`, `(0,2,3)` ... `(0,N-2,N-1)`.  A counter and a
// comparator.  The alternative -- a second assembler -- would have cost a fifth
// client-A demand (EXHAUSTING the 2-bit owner field, which has exactly `2'd3`
// left and the instance centre needs it), a fourth clipdoor client, a second
// `zhao_geom_depthquant_stream` and a second 520-slot vertex store.
//
// WHAT THIS BLOCK DOES NOT DO, deliberately
// -----------------------------------------
//   * It does not BUFFER vertices.  The assembler already stores them; storing
//     them twice would be a second copy to keep in step, which is the
//     stale-copy fault this tree has paid for more than once.
//   * It does not decide a winding, a material or a colour.  The fan order is
//     the ring's own emission order, which `zhao_forge_shadow` declares
//     deterministic -- that ordering convention is the whole contract between
//     the two halves, exactly as it is for `zhao_forge_prim`.
//   * It does not arbitrate.  `zhao_forge_jobarb` owns the choice between this
//     producer and FORGE.PRIM, and it owns it at JOB granularity.
//
// THE TWO FAULTS IT REFUSES RATHER THAN WEDGES ON
// -----------------------------------------------
// Both are reachable with legal stimulus at this block's own port, which is why
// they are counters and not comments:
//
//   * A RING SHORTER THAN THREE VERTICES cannot make a triangle.  The assembler
//     is already committed by then (it took the vertices) and waits in `A_TRIS`
//     for a `t_last`, so returning nothing would DEADLOCK it.  This block emits
//     ONE degenerate triple `(0,0,0)` with `t_last` and counts it on
//     `short_ring_o`.  A zero-area triangle is refused downstream by GEOM.SETUP
//     rather than drawn, so the disposal is a retirement and not a picture.
//   * A RING LONGER THAN `MAXV` is TRUNCATED: `v_last_o` is forced on vertex
//     `MAXV-1` and `ring_overflow_o` counts it.  The hull that comes out is
//     wrong and says so on a counter; the alternative is a store that runs past
//     its end or a producer that never terminates.
//
// THE SIDEBAND THIS BLOCK CARRIES, and why it is only one value
// -------------------------------------------------------------
// Every other field of a shadow job is a named constant at the composer -- the
// material pair is `{0, 0}` (owner ruling: `MATMODE_NONE` is REFUSED by
// `zhao_material_window` with a non-zero pair, `zhao_material_window.sv:354`),
// the art colour and the cull mode are the owner's knobs.  The ONE value that
// is produced rather than authored here is the per-primitive ALPHA:
// `zhao_forge_shadow.sv:295` is `assign vtx_alpha_o = strength_q`, latched PER
// CASTER, and owner ruling R89 routes exactly that value down
// `tri_continuation_tail_i`'s flat `vertex_alpha` to `zhao_raster_blend_prod`.
// So it is LATCHED HERE at the hull's first vertex and held for the job --
// because during the triangle phase the shadow block has already retired and
// its port no longer describes this hull.  A combinational passthrough would
// read the NEXT caster's strength onto THIS hull's triangles.
`default_nettype none

module zhao_forge_fanindex #(
    // The longest ring this block will pass.  The hero rung is 16 and
    // `zhao_forge_shadow` has an elaboration guard fixing the ladder at
    // 16 / 8 / 4, so this is the ladder's own ceiling and not a guess.
    parameter int unsigned MAXV     = 16,
    parameter int unsigned IDW      = 16,
    parameter int unsigned CENSUS_W = 32
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the RING, from zhao_forge_shadow's vtx_* -------------------------
    input  var logic               vtx_valid_i,
    output var logic               vtx_ready_o,
    input  var logic signed [31:0] vtx_x_i,
    input  var logic signed [31:0] vtx_y_i,
    input  var logic signed [31:0] vtx_z_i,
    input  var logic        [ 7:0] vtx_alpha_i,
    input  var logic               vtx_last_i,
    input  var logic        [IDW-1:0] vtx_src_id_i,
    input  var logic        [ 1:0] vtx_rung_i,

    // ---- the WORLD vertex stream, into the shared assembler's v_* ---------
    output var logic               v_valid_o,
    input  var logic               v_ready_i,
    output var logic signed [31:0] v_x_o,
    output var logic signed [31:0] v_y_o,
    output var logic signed [31:0] v_z_o,
    output var logic               v_last_o,

    // ---- the INDEX stream, into the shared assembler's t_* ----------------
    output var logic               t_valid_o,
    input  var logic               t_ready_i,
    output var logic [15:0]        t_i0_o,
    output var logic [15:0]        t_i1_o,
    output var logic [15:0]        t_i2_o,
    // The assembler's `mat_skew_o` differences this against the job's own id.
    // A shadow job's id is ZERO by the material window's `MATMODE_NONE` law, so
    // this is zero for the same reason and not by coincidence.
    output var logic [15:0]        t_material_o,
    output var logic [IDW-1:0]     t_src_id_o,
    output var logic               t_last_o,

    // ---- the job's ONE produced sideband value ----------------------------
    // Valid from the first vertex OFFER (not its take), because the arbiter
    // presents the sideband to the assembler one cycle BEFORE it releases the
    // first vertex -- the assembler moves `jset_q` into `mset_q` on the first
    // vertex take, so a sideband arriving on that same clock would be a cycle
    // late and the job would run under the previous job's values.
    output var logic [ 7:0]        j_vertex_alpha_o,

    // ---- census -----------------------------------------------------------
    output var logic [CENSUS_W-1:0] hulls_o,          // rings fanned out whole
    output var logic [CENSUS_W-1:0] triangles_o,      // triples offered
    output var logic [CENSUS_W-1:0] short_ring_o,     // FAULT: fewer than 3
    output var logic [CENSUS_W-1:0] ring_overflow_o,  // FAULT: longer than MAXV
    output var logic [CENSUS_W-1:0] hulls_rung_o [4]  // hulls settled at each rung
);

  // Quartus 17.0 needs an elaboration check inside an `initial`; a bare
  // module-scope `if` is a syntax error there even though Verilator lints it
  // clean (CLAUDE.md, 2026-09-08).
  initial begin
    if (MAXV < 3) begin
      $fatal(1, "zhao_forge_fanindex: MAXV must be at least 3 -- a shorter ring cannot make a triangle");
    end
    if (MAXV > 65535) begin
      $fatal(1, "zhao_forge_fanindex: MAXV exceeds the 16-bit index the assembler's triple carries");
    end
  end

  localparam int unsigned NW = $clog2(MAXV + 1);      // 5

  typedef enum logic [1:0] { S_RING, S_TRIS, S_DEGEN } state_e;
  state_e st_q;

  logic [NW-1:0]    n_q;        // vertices taken so far this ring
  logic [NW-1:0]    nv_q;       // the ring's final count, known at v_last
  logic [NW-1:0]    k_q;        // the fan's middle index
  logic [ 7:0]      alpha_q;    // the caster's strength, latched at vertex 0
  logic [IDW-1:0]   src_q;
  logic [ 1:0]      rung_q;

  // TRUNCATION, not wrapping.  A ring that has already handed over MAXV-1
  // vertices is terminated here whatever the producer says, so the assembler's
  // store cannot be overrun and the job always retires.
  wire last_c = vtx_last_i || (32'(n_q) == 32'(MAXV) - 32'd1);

  assign v_valid_o = (st_q == S_RING) && vtx_valid_i;
  assign vtx_ready_o = (st_q == S_RING) && v_ready_i;
  assign v_x_o    = vtx_x_i;
  assign v_y_o    = vtx_y_i;
  assign v_z_o    = vtx_z_i;
  assign v_last_o = last_c;

  assign t_valid_o    = (st_q == S_TRIS) || (st_q == S_DEGEN);
  assign t_i0_o       = 16'd0;
  assign t_i1_o       = (st_q == S_DEGEN) ? 16'd0 : {{(16-NW){1'b0}}, k_q};
  assign t_i2_o       = (st_q == S_DEGEN) ? 16'd0
                                          : ({{(16-NW){1'b0}}, k_q} + 16'd1);
  assign t_material_o = 16'd0;
  assign t_src_id_o   = src_q;
  assign t_last_o     = (st_q == S_DEGEN)
                     || (32'(k_q) + 32'd2 == 32'(nv_q));

  // The offer's own alpha while vertex 0 is on the wire, the latched one
  // afterwards.  See the port's comment for why a pure passthrough is wrong.
  assign j_vertex_alpha_o = ((st_q == S_RING) && (32'(n_q) == 32'd0))
                          ? vtx_alpha_i : alpha_q;

  wire v_take_c = v_valid_o && v_ready_i;
  wire t_take_c = t_valid_o && t_ready_i;

  integer r;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q            <= S_RING;
      n_q             <= '0;
      nv_q            <= '0;
      k_q             <= '0;
      alpha_q         <= 8'd0;
      src_q           <= '0;
      rung_q          <= 2'd0;
      hulls_o         <= '0;
      triangles_o     <= '0;
      short_ring_o    <= '0;
      ring_overflow_o <= '0;
      for (r = 0; r < 4; r = r + 1) hulls_rung_o[r] <= '0;
    end else begin
      unique case (st_q)
        S_RING: begin
          if (v_take_c) begin
            if (32'(n_q) == 32'd0) begin
              // ONE ENABLE, EVERY HELD FIELD.  The alpha, the source id and the
              // rung are the caster's and they are captured together, so a
              // later caster cannot contribute one of the three to this hull.
              alpha_q <= vtx_alpha_i;
              src_q   <= vtx_src_id_i;
              rung_q  <= vtx_rung_i;
            end
            n_q <= n_q + {{(NW-1){1'b0}}, 1'b1};
            if (last_c) begin
              nv_q <= n_q + {{(NW-1){1'b0}}, 1'b1};
              k_q  <= {{(NW-1){1'b0}}, 1'b1};
              if (vtx_last_i == 1'b0) begin
                // Terminated by the ceiling rather than by the producer.
                if (ring_overflow_o != {CENSUS_W{1'b1}})
                  ring_overflow_o <= ring_overflow_o + {{(CENSUS_W-1){1'b0}}, 1'b1};
              end
              if (32'(n_q) + 32'd1 >= 32'd3) begin
                st_q <= S_TRIS;
              end else begin
                st_q <= S_DEGEN;
                if (short_ring_o != {CENSUS_W{1'b1}})
                  short_ring_o <= short_ring_o + {{(CENSUS_W-1){1'b0}}, 1'b1};
              end
            end
          end
        end

        S_TRIS: begin
          if (t_take_c) begin
            if (triangles_o != {CENSUS_W{1'b1}})
              triangles_o <= triangles_o + {{(CENSUS_W-1){1'b0}}, 1'b1};
            if (t_last_o) begin
              st_q <= S_RING;
              n_q  <= '0;
              if (hulls_o != {CENSUS_W{1'b1}})
                hulls_o <= hulls_o + {{(CENSUS_W-1){1'b0}}, 1'b1};
              if (hulls_rung_o[rung_q] != {CENSUS_W{1'b1}})
                hulls_rung_o[rung_q] <= hulls_rung_o[rung_q]
                                      + {{(CENSUS_W-1){1'b0}}, 1'b1};
            end else begin
              k_q <= k_q + {{(NW-1){1'b0}}, 1'b1};
            end
          end
        end

        S_DEGEN: begin
          if (t_take_c) begin
            st_q <= S_RING;
            n_q  <= '0;
            // NOT counted on `hulls_o`: no hull was drawn.  `short_ring_o` is
            // this ring's whole record, and a fault must not also read as a
            // success on the neighbouring counter.
          end
        end

        default: st_q <= S_RING;
      endcase
    end
  end

endmodule : zhao_forge_fanindex

`default_nettype wire
