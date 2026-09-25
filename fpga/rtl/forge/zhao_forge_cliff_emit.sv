// zhao_forge_cliff_emit.sv -- THE EMISSION STAGE FORGE.CLIFF NEVER HAD: one rim
// edge in, one WALL QUAD out, as world-fx16 vertices and index triples for
// `zhao_forge_assemble`.
//
// `design/contracts/FORGE.CLIFF.md` states the absence in as many words:
// "`zhao_forge_cliff` does NOT turn a rim edge into wall VERTICES ... THE
// EMISSION STAGE IS NOT WRITTEN." This is that stage, and the contract's own
// list of the three things it needs is the specification it is built against.
//
// ===========================================================================
// DECISION RECORD 1 -- ONE RIM EDGE IS ONE JOB
// ===========================================================================
// QUESTION. `zhao_forge_assemble`'s per-job vertex arena is `MAX_VERTS`, composed
// at 520 (`zhao_console_core.sv:8281`), indices are REBASED PER JOB
// (`zhao_forge_assemble.sv:557`, `:804`), and an over-long job is not truncated
// -- `v_ready_o` goes low and STAYS low, `v_last_i` can then never be accepted,
// and the assembler sits in A_COLLECT forever ticking `vtx_overflow_o`
// (`zhao_forge_assemble.sv:717-726`). A page may emit up to the 512-edge budget,
// which is 2,048 vertices. How is the page cut into jobs?
//
// CHOSEN. One rim edge = one job = FOUR vertices and TWO index triples.
//
// REASON. It makes the overflow UNREACHABLE rather than guarded: four is not
// near 520 under any input, so there is no bound to get wrong, no truncation
// rule to test, and no counter whose silence has to be trusted. The index base
// is then trivially 0..3, which matches the assembler's rebase exactly, and the
// job boundary is where `zhao_forge_jobarb` is allowed to switch owner
// (`zhao_forge_jobarb.sv:372-377`), so short jobs make the cliff a POLITE client
// that cannot hold the assembler across a whole page.
//
// CONSTRAINTS / COST. A descriptor beat and jobarb's two-clock A_SIDE/grant
// turnaround per edge, plus the assembler's five clocks per triangle. Order
// 20 clocks per edge, so a full 512-edge page is order 10k clocks against a
// 1.67M-clock frame. Measured against the budget, not assumed.
//
// ALTERNATIVE REJECTED. Batching 130 quads per job to amortise the descriptor.
// It saves clocks that are not scarce and buys a hard 520-vertex edge to sit
// next to, with a truncation rule (`zhao_forge_fanindex.sv:157` is the pattern)
// and a `ring_overflow`-shaped counter that legal stimulus would almost never
// reach. Not worth its own bugs.
//
// ===========================================================================
// DECISION RECORD 2 -- THE TWO ENDPOINTS ARE THE EVALUATOR'S OWN, NOT RE-DERIVED
// ===========================================================================
// QUESTION. A rim edge is `(ci, cj, side, span)`. Which two lattice vertices are
// its ends, and which way round?
//
// CHOSEN. Exactly the `va`/`vb` map `zhao_forge_cliff_ram.sv:439-467` already
// computes for its vdist addresses, transcribed side for side:
//
//     side 0:  A = (ci,        cj    )   B = (ci + span, cj       )
//     side 1:  A = (ci + span, cj + 1)   B = (ci,        cj + 1   )
//     side 2:  A = (ci,        cj + span) B = (ci,       cj       )
//     side 3:  A = (ci + 1,    cj    )   B = (ci + 1,    cj + span)
//
// REASON. That map is ALREADY IN THE TREE and already ratified by the
// differential against `zref::forge::rim_plan`; it is where the evaluator's own
// priority degrade reads its endpoint nearness. Writing a second endpoint map
// here would be a second opinion about what an edge's ends are, and the first
// time the two disagreed the wall would be built across the wrong vertices while
// the degrade ranked the right ones. The A-before-B order is the reference's own
// and is kept, so the quad's winding is consistent across all four sides without
// a per-side special case.
//
// ALTERNATIVE REJECTED. Deriving the endpoints from the side's geometry here.
// Cheaper to read, and it is the duplicate-law shape this repository has paid
// for repeatedly.
//
// ===========================================================================
// DECISION RECORD 3 -- THE PLACEMENT LAW IS CONSUMED, NEVER RE-DERIVED
// ===========================================================================
// QUESTION. A wall vertex needs a WORLD position. Where does (x, z) come from?
//
// CHOSEN. `zhao_terrain_compcache_front`'s lattice service, shared through
// `zhao_forge_cliff_srvshare`: `lat_wx_o`/`lat_wz_o` are TERRAIN.PLACE's PLACED
// coordinates for that vertex, read back rather than recomputed, and `lat_h_o`
// is the surface height at it. Four reads per edge -- (A, top), (A, bottom),
// (B, top), (B, bottom) -- give all four corners of the quad.
//
// REASON. `zhao_terrain_place_law_pkg` holds the placement law in ONE
// definition. Multiplying a lattice index by a pitch here would be a second
// implementation of a ratified law, which is exactly what the packet forbids and
// what the campaign has repeatedly paid for. Reading the placed value back costs
// four service beats and cannot disagree with the tessellator about where a
// vertex is, because it IS the tessellator's number.
//
// The BOTTOM surface is `lat_surface_i = 1`, layer C -- the modelled underside.
// On a legacy single-surface page bottom == top, the quad is degenerate and the
// clipper drops it, which is the correct behaviour for a page with no underside
// (`terrain_rules` 3.1 option (a)).
//
// ===========================================================================
// DECISION RECORD 4 -- ONE POSITION REGISTER, SO NOTHING CAN DRIFT APART
// ===========================================================================
// QUESTION. The lattice read is one clock, registered, untagged, and this
// block's client is DENIED any cycle the incumbent asks. CLAUDE.md: "a producer
// feeding a RAM-backed evaluator is the shape where a stall pairs one record's
// geometry with another's occupancy while every counter balances."
//
// CHOSEN. Removed, not detected. The fetch phase has exactly ONE piece of
// position state, `fk_r` (0..3). The read address is computed COMBINATIONALLY
// from it and from the edge registers, and it advances only when an answer
// LANDS. One read is in flight at a time. A denied cycle, a stalled cycle and a
// late answer all produce the same thing, which is nothing happening -- there is
// no second quantity for the first to disagree with.
//
// ALTERNATIVE REJECTED. Pipelining all four reads behind a free-running address.
// Three clocks faster per edge and it is the `zhao_field_earth_adapter` defect
// exactly: on a denial the address and the landing pointer separate, corner A's
// height is stored as corner B's, and every counter still balances because no
// counter looks at the field that moved.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK DOES NOT DO, STATED SO NOBODY INFERS IT FROM A MISSING TEST
// ---------------------------------------------------------------------------
// It emits no U/V. `zhao_forge_assemble` hardwires `o_untex_o = 1'b1`
// (`zhao_forge_assemble.sv:617`) -- a forge primitive has no u/v by law (R197) --
// so the ACCUMULATED RIM LENGTH that `spec/terrain_rules.md` 6.6 defines for the
// strata U has no port to arrive on and is not accumulated here. That is the
// contract's own "second half" and it stays open; the wall is shaded by the
// job's flat art lanes. Adding it is a change to the assembler's attribute set,
// not to this block's law.
//
// Conservative SystemVerilog subset (charter 2): elaboration checks inside
// `initial begin`, no bare module-scope `if`, explicit generate where used.

module zhao_forge_cliff_emit #(
    parameter int unsigned LAT_W    = 33,
    parameter int unsigned LAT_H    = 33,
    parameter int unsigned CENSUS_W = 32
) (
    input var logic clk,
    input var logic rst_n,

    // ---- rim edges in, from zhao_forge_cliff_ram ----------------------------
    input  var logic        edge_valid_i,
    output var logic        edge_ready_o,
    input  var logic [15:0] edge_ci_i,
    input  var logic [15:0] edge_cj_i,
    input  var logic [ 1:0] edge_side_i,
    input  var logic [ 5:0] edge_span_i,
    input  var logic [15:0] edge_src_id_i,

    // ---- the lattice client (through zhao_forge_cliff_srvshare) -------------
    output var logic              lat_req_o,
    output var logic [ 5:0]       lat_vi_o,
    output var logic [ 5:0]       lat_vj_o,
    output var logic              lat_surface_o,
    input  var logic              lat_grant_i,
    input  var logic              lat_rsp_valid_i,
    input  var logic signed [31:0] lat_h_i,
    input  var logic signed [31:0] lat_wx_i,
    input  var logic signed [31:0] lat_wz_i,

    // ---- the vertex stream, into zhao_forge_jobarb --------------------------
    output var logic               v_valid_o,
    input  var logic               v_ready_i,
    output var logic signed [31:0] v_x_o,
    output var logic signed [31:0] v_y_o,
    output var logic signed [31:0] v_z_o,
    output var logic               v_last_o,

    // ---- the index-triple stream --------------------------------------------
    output var logic        t_valid_o,
    input  var logic        t_ready_i,
    output var logic [15:0] t_i0_o,
    output var logic [15:0] t_i1_o,
    output var logic [15:0] t_i2_o,
    output var logic [15:0] t_material_o,
    output var logic [15:0] t_src_id_o,
    output var logic        t_last_o,

    // The material id this block stamps on every triple. A port, not a
    // constant, so the wall's material stays the owner's to choose.
    input var logic [15:0] material_id_i,

    // ---- evidence -------------------------------------------------------------
    output var logic                busy_o,
    output var logic [CENSUS_W-1:0] edges_taken_o,    // rim edges accepted
    output var logic [CENSUS_W-1:0] quads_emitted_o,  // jobs completed
    output var logic [CENSUS_W-1:0] tris_emitted_o,   // index triples accepted
    output var logic [CENSUS_W-1:0] lat_reads_o,      // lattice beats issued
    output var logic [CENSUS_W-1:0] lat_denied_o,     // cycles lost to the incumbent
    // FAULT: an endpoint outside the staged lattice. Unreachable from the
    // evaluator (a run cannot leave its page), reachable at THIS BLOCK'S OWN
    // PORT by offering an illegal edge, which is what the bench does.
    output var logic [CENSUS_W-1:0] endpoint_clamped_o
);

  localparam int unsigned VMax = LAT_W - 1;  // highest legal lattice index, 32

  initial begin
    if (LAT_W != 33 || LAT_H != 33) begin
      $fatal(1, "zhao_forge_cliff_emit: only the 33x33 staged patch is supported (%0dx%0d)",
             LAT_W, LAT_H);
    end
    if (CENSUS_W < 8) begin
      $fatal(1, "zhao_forge_cliff_emit: CENSUS_W must be >= 8 (is %0d)", CENSUS_W);
    end
  end

  localparam logic [1:0] StIdle  = 2'd0;
  localparam logic [1:0] StFetch = 2'd1;
  localparam logic [1:0] StVerts = 2'd2;
  localparam logic [1:0] StTris  = 2'd3;

  logic [1:0] st_r;

  // ---- the edge under construction ------------------------------------------
  logic [ 5:0] e_ci_r, e_cj_r, e_span_r;
  logic [ 1:0] e_side_r;
  logic [15:0] e_src_id_r;
  // The page is patch-local (zhao_forge_cliff_feed DECISION RECORD 1), so the
  // evaluator's 16-bit cell fields carry nothing above bit 5. Rather than
  // lint-silencing the upper bits, they are FOLDED INTO THE OUT-OF-RANGE TEST:
  // an edge naming a cell outside the staged patch is exactly the fault
  // `endpoint_clamped_o` exists to see, and an absolute-coordinate page
  // arriving here by mistake would set them.
  logic        e_hici_r;

  // DECISION RECORD 4: the fetch phase's ONLY position state, plus the
  // single-flight bit that keeps the address and the answer married.
  logic [1:0] fk_r;
  logic       fl_r;

  // corner stores: 0 = A top, 1 = A bottom, 2 = B top, 3 = B bottom
  logic signed [31:0] h_r    [4];
  logic signed [31:0] ax_r, az_r, bx_r, bz_r;

  logic [1:0] vk_r;  // vertex 0..3
  logic       tk_r;  // triple 0..1

  // ---- DECISION RECORD 2: the endpoints, transcribed ------------------------
  logic [6:0] a_vi_c, a_vj_c, b_vi_c, b_vj_c;
  always_comb begin
    case (e_side_r)
      2'd0: begin
        a_vi_c = {1'b0, e_ci_r};
        a_vj_c = {1'b0, e_cj_r};
        b_vi_c = {1'b0, e_ci_r} + {1'b0, e_span_r};
        b_vj_c = {1'b0, e_cj_r};
      end
      2'd1: begin
        a_vi_c = {1'b0, e_ci_r} + {1'b0, e_span_r};
        a_vj_c = {1'b0, e_cj_r} + 7'd1;
        b_vi_c = {1'b0, e_ci_r};
        b_vj_c = {1'b0, e_cj_r} + 7'd1;
      end
      2'd2: begin
        a_vi_c = {1'b0, e_ci_r};
        a_vj_c = {1'b0, e_cj_r} + {1'b0, e_span_r};
        b_vi_c = {1'b0, e_ci_r};
        b_vj_c = {1'b0, e_cj_r};
      end
      default: begin
        a_vi_c = {1'b0, e_ci_r} + 7'd1;
        a_vj_c = {1'b0, e_cj_r};
        b_vi_c = {1'b0, e_ci_r} + 7'd1;
        b_vj_c = {1'b0, e_cj_r} + {1'b0, e_span_r};
      end
    endcase
  end

  // The read this fetch beat wants. Combinational from fk_r and the edge.
  logic [6:0] want_vi_c, want_vj_c;
  logic       want_surface_c;
  always_comb begin
    want_surface_c = fk_r[0];              // 0 = top, 1 = bottom
    if (fk_r[1]) begin
      want_vi_c = b_vi_c;
      want_vj_c = b_vj_c;
    end else begin
      want_vi_c = a_vi_c;
      want_vj_c = a_vj_c;
    end
  end

  logic oob_c;
  assign oob_c = (want_vi_c > 7'(VMax)) || (want_vj_c > 7'(VMax)) || e_hici_r;

  logic [5:0] clamp_vi_c, clamp_vj_c;
  assign clamp_vi_c = (want_vi_c > 7'(VMax)) ? 6'(VMax) : want_vi_c[5:0];
  assign clamp_vj_c = (want_vj_c > 7'(VMax)) ? 6'(VMax) : want_vj_c[5:0];

  // SINGLE FLIGHT, and this line is load-bearing. `fk_r` and the read address
  // update on the SAME clock edge as the capture, so a continuously-asserted
  // request re-issues the OLD address on the capture cycle and the next answer
  // lands in the next slot -- corner A's height stored as corner A's twice and
  // corner B's never read. That is precisely the drift DECISION RECORD 4 claims
  // to have removed, it was written here and NOT implemented on the first cut,
  // and `forge_cliff_chain` lane 1 caught it: 218 edges correct, every vertex 1
  // holding vertex 0's x and a height of zero. A decision record is not the
  // code; the bench is what makes them the same thing.
  assign lat_req_o     = (st_r == StFetch) && !fl_r;
  assign lat_vi_o      = clamp_vi_c;
  assign lat_vj_o      = clamp_vj_c;
  assign lat_surface_o = want_surface_c;

  // ---- the quad's four vertices ---------------------------------------------
  //   0 = A top     1 = B top     2 = B bottom     3 = A bottom
  logic signed [31:0] vx_c, vy_c, vz_c;
  always_comb begin
    case (vk_r)
      2'd0: begin vx_c = ax_r; vy_c = h_r[0]; vz_c = az_r; end
      2'd1: begin vx_c = bx_r; vy_c = h_r[2]; vz_c = bz_r; end
      2'd2: begin vx_c = bx_r; vy_c = h_r[3]; vz_c = bz_r; end
      default: begin vx_c = ax_r; vy_c = h_r[1]; vz_c = az_r; end
    endcase
  end

  assign v_valid_o = (st_r == StVerts);
  assign v_x_o     = vx_c;
  assign v_y_o     = vy_c;
  assign v_z_o     = vz_c;
  assign v_last_o  = (st_r == StVerts) && (vk_r == 2'd3);

  // (0,1,2) then (0,2,3) -- the A-before-B order of DECISION RECORD 2 carries
  // the outward sense, so there is no per-side winding case here.
  assign t_valid_o    = (st_r == StTris);
  assign t_i0_o       = 16'd0;
  assign t_i1_o       = tk_r ? 16'd2 : 16'd1;
  assign t_i2_o       = tk_r ? 16'd3 : 16'd2;
  assign t_material_o = material_id_i;
  assign t_src_id_o   = e_src_id_r;
  assign t_last_o     = (st_r == StTris) && tk_r;

  assign edge_ready_o = (st_r == StIdle);
  assign busy_o       = (st_r != StIdle);

  logic v_take_c, t_take_c;
  assign v_take_c = v_valid_o && v_ready_i;
  assign t_take_c = t_valid_o && t_ready_i;

  // ---- evidence -------------------------------------------------------------
  logic [CENSUS_W-1:0] edges_taken_r, quads_emitted_r, tris_emitted_r;
  logic [CENSUS_W-1:0] lat_reads_r, lat_denied_r, endpoint_clamped_r;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_r       <= StIdle;
      e_ci_r     <= '0;
      e_cj_r     <= '0;
      e_span_r   <= '0;
      e_side_r   <= '0;
      e_src_id_r <= '0;
      e_hici_r   <= 1'b0;
      fk_r       <= '0;
      fl_r       <= 1'b0;
      vk_r       <= '0;
      tk_r       <= 1'b0;
      ax_r       <= '0;
      az_r       <= '0;
      bx_r       <= '0;
      bz_r       <= '0;
      h_r[0]     <= '0;
      h_r[1]     <= '0;
      h_r[2]     <= '0;
      h_r[3]     <= '0;

      edges_taken_r      <= '0;
      quads_emitted_r    <= '0;
      tris_emitted_r     <= '0;
      lat_reads_r        <= '0;
      lat_denied_r       <= '0;
      endpoint_clamped_r <= '0;
    end else begin
      if (lat_req_o && lat_grant_i) begin
        fl_r <= 1'b1;
        if (!(&lat_reads_r)) begin
          lat_reads_r <= lat_reads_r + CENSUS_W'(1);
        end
        if (oob_c && !(&endpoint_clamped_r)) begin
          endpoint_clamped_r <= endpoint_clamped_r + CENSUS_W'(1);
        end
      end
      if (lat_req_o && !lat_grant_i && !(&lat_denied_r)) begin
        lat_denied_r <= lat_denied_r + CENSUS_W'(1);
      end

      case (st_r)
        StIdle: begin
          if (edge_valid_i) begin
            e_ci_r     <= edge_ci_i[5:0];
            e_cj_r     <= edge_cj_i[5:0];
            e_side_r   <= edge_side_i;
            e_span_r   <= edge_span_i;
            e_src_id_r <= edge_src_id_i;
            e_hici_r   <= (edge_ci_i[15:6] != 10'd0) || (edge_cj_i[15:6] != 10'd0);
            fk_r       <= '0;
            fl_r       <= 1'b0;
            vk_r       <= '0;
            tk_r       <= 1'b0;
            st_r       <= StFetch;
            if (!(&edges_taken_r)) begin
              edges_taken_r <= edges_taken_r + CENSUS_W'(1);
            end
          end
        end

        // DECISION RECORD 4: fk_r advances only when an answer LANDS, and the
        // address it advances from is the address that answer was asked at.
        StFetch: begin
          if (lat_rsp_valid_i && fl_r) begin
            fl_r      <= 1'b0;
            h_r[fk_r] <= lat_h_i;
            if (fk_r == 2'd0) begin
              ax_r <= lat_wx_i;
              az_r <= lat_wz_i;
            end
            if (fk_r == 2'd2) begin
              bx_r <= lat_wx_i;
              bz_r <= lat_wz_i;
            end
            if (fk_r == 2'd3) begin
              st_r <= StVerts;
            end else begin
              fk_r <= fk_r + 2'd1;
            end
          end
        end

        StVerts: begin
          if (v_take_c) begin
            if (vk_r == 2'd3) begin
              st_r <= StTris;
            end else begin
              vk_r <= vk_r + 2'd1;
            end
          end
        end

        StTris: begin
          if (t_take_c) begin
            if (!(&tris_emitted_r)) begin
              tris_emitted_r <= tris_emitted_r + CENSUS_W'(1);
            end
            if (tk_r) begin
              st_r <= StIdle;
              if (!(&quads_emitted_r)) begin
                quads_emitted_r <= quads_emitted_r + CENSUS_W'(1);
              end
            end else begin
              tk_r <= 1'b1;
            end
          end
        end

        default: st_r <= StIdle;
      endcase
    end
  end

  assign edges_taken_o      = edges_taken_r;
  assign quads_emitted_o    = quads_emitted_r;
  assign tris_emitted_o     = tris_emitted_r;
  assign lat_reads_o        = lat_reads_r;
  assign lat_denied_o       = lat_denied_r;
  assign endpoint_clamped_o = endpoint_clamped_r;

endmodule
