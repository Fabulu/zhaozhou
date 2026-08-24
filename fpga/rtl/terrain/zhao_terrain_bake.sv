// zhao_terrain_bake.sv — TERRAIN.BAKE: the persistent scar bake and the
// breach law (phase 7, ZH-036).
//
// Law, in citation order:
//   design/contracts/TERRAIN.BAKE.md — the block contract.
//   design/blocks.yml — `inputs: [stamp_results]`, `outputs: [baked_scars]`,
//       upstream SURFACE.STAMP, downstream TERRAIN.PATCH,
//       `backpressure: ready_valid`, `latency: variable`, "1 bake texel per
//       clock", counter `surface_texels_touched`, `source_ids: true`.
//   spec/terrain_rules.md §2 — layers A (base), B (scar), C (bottom),
//       D (cell state), all on the 33x33 / 32x32 Island Patch v1 grid.
//   spec/terrain_rules.md §3.3 — the cell state byte and the no_bake CORNER
//       SHADOW (the clamp is vertex-level, corners are shared, so protection
//       reaches one cell further in every direction).
//   spec/terrain_rules.md §3.4 — the breach law, evaluated ONLY here.
//   spec/terrain_rules.md §7 — "B written only by TERRAIN.BAKE; D written
//       only by TERRAIN.BAKE (breach/heal)". This block is that writer.
//   spec/terrain_rules.md §9 / §9.2 — incremental scaling, the frozen
//       BAKE_PATCH_BUDGET = 64 and its four-part deferral law.
//   spec/qformats.md §2/§9 — height16 <-> fx16; §3/§4 — rescale and the
//       single round-half-up.
//   reference/src/zterrain/terrain_core.cpp `bake_dig` and
//       `apply_breach_law` — THE EXECUTED LAW, reproduced below line for
//       line. `zref::terrain::lattice_lerp` likewise.
//
// ---------------------------------------------------------------------------
// WHAT THE RATIFIED INPUT ACTUALLY IS — read before assuming
// ---------------------------------------------------------------------------
// The ledger says `inputs: [stamp_results]`, `upstream: [SURFACE.STAMP]`, and
// SURFACE.STAMP's landed `stamp_results` port is a PER-TEXEL layer-F stream
// {texel, tag, strength_after, strength_before}. This contract's own packet
// table says something DIFFERENT: "stamp records {patch_id, stencil handle,
// from fx16, to fx16, footprint} in command order". Those are two different
// wires wearing one name, and only the second one has arithmetic behind it:
// `zref::terrain::bake_dig` — the reference this block is the hardware image
// of — takes exactly {stencil, depth_from, depth_to} against a patch, and
// terrain_rules §9.2's deferral identity is written in `from`/`to` depths and
// in nothing else.
//
// SO THIS BLOCK TAKES THE STAMP RECORD, and the layer-F -> layer-B conversion
// is left EXPLICITLY UNDECIDED rather than invented (the discipline
// SURFACE.SHEET's contract set). What is missing for it is not a wire, it is
// two laws: a strength(u8) -> height16 depth mapping, and a 64x64 -> 33x33
// resample. Neither exists anywhere in this tree, and inventing them here
// would put a fabrication under every permanent wound in the game. The seam is
// recorded in design/contracts/TERRAIN.BAKE.md under "the sheet seam" with the
// two candidate laws written down so the next increment negotiates them
// instead of discovering them.
//
// ---------------------------------------------------------------------------
// THE TWO PHASES, AND WHY THEY ARE ONE BLOCK
// ---------------------------------------------------------------------------
// `bake_dig` writes layer B; `apply_breach_law` then writes layer D. The
// reference splits them into two functions and the header says why: "bake
// writes B, the breach law writes D; the split mirrors the contract's two
// planes". terrain_rules §3.4 nonetheless makes the breach law's TIMING part
// of the bake ("after a bake writes scar values"), and §7 gives both planes to
// this one owner, so one block runs both, back to back, per stamp record:
//
//   DIG phase    — 33x33 vertices in z-then-x scan order. Per vertex the
//                  block places the lattice point, evaluates the paraboloid
//                  stencil, applies the incremental delta, the no_bake clamp
//                  and the height16 rails, and emits the new layer-B word.
//   BREACH phase — 32x32 cells in the same scan order, consuming layer D and
//                  emitting the transitions in the reference's own order.
//
// The bridge between them is the `meets` plane: one bit per vertex,
// `base + scar <= bottom` on the height16 grid, which is `apply_breach_law`'s
// `meets_bottom` exactly (its comment: "compose_top == bottom after the §3.4
// clamp <=> base + scar <= bottom"). It is 1,089 flops, held HERE.
//
// ---------------------------------------------------------------------------
// LAWS CHOSEN, NOT FOUND (each argued again in the contract)
// ---------------------------------------------------------------------------
// B1. THE 33x33 SWEEP LIVES IN THIS BLOCK, and so does `lattice_lerp`. The
//     alternative was to take `vx`/`vz` per vertex from the caller. REJECTED:
//     `bake_dig` computes the placed lattice point itself, so a caller-side
//     sweep would put a ratified rounding law (and its truncating divide) in
//     whatever drives this block — and, in a differential test, in the test
//     itself, which is charter §29-6's exact failure mode. The extents are
//     hard-wired at 33x33 because Island Patch v1 is (terrain_rules §2), which
//     also turns `lattice_lerp`'s divide into a shift-shaped constant.
// B2. THE `meets` PLANE IS RESIDENT (1,089 flops). REJECTED ALTERNATIVE:
//     taking four `meets` bits per cell from the caller, which is cheaper in
//     flops and moves §3.4's breach EQUALITY — the exact height16 compare the
//     whole law turns on — outside the only block terrain_rules §7 permits to
//     own it. The equality is the law; it stays here.
// B3. THE no_bake CORNER SHADOW ARRIVES AS ONE BIT PER VERTEX. `bake_dig`
//     re-derives it per vertex by OR-ing the no_bake bit of the <=4 cells
//     touching that vertex out of layer D. REJECTED ALTERNATIVE: holding all
//     of layer D resident so the block could do that OR itself — 1,024 bytes
//     of state to reproduce a fact the reader of D already has, in a block
//     that deliberately has no VRAM port (TERRAIN.PATCH made the same call for
//     the same reason). terrain_rules §3.3 states the shadow as VERTEX-level
//     law, so a vertex-level wire is its natural shape.
// B4. THE §9.2 CADENCE BUDGET IS ENFORCED BY BACKPRESSURE, not by an internal
//     queue. `cmd_ready_o` drops once BAKE_PATCH_BUDGET patch-bakes have been
//     accepted in the current frame window and returns on `frame_start_i`.
//     Law 2 wants the remainder to carry "to the head of the next frame's
//     window, ahead of newly issued bakes (FIFO)" — refusing to accept leaves
//     the record exactly where it was, at the head of the upstream queue, so
//     the FIFO order is preserved STRUCTURALLY and nothing can be dropped or
//     reordered by this block at all. REJECTED ALTERNATIVE: an internal
//     deferral FIFO of stamp records, which buys nothing (the upstream queue
//     already exists and is already ordered) and adds a second place where
//     order could be lost. `frame_start_i` during an in-flight bake resets the
//     window without disturbing the bake: the budget counts ACCEPTANCES.
// B5. A RECORD WITH `radius <= 0` IS ACCEPTED AND SWEEPS, WRITING NOTHING.
//     `bake_dig` returns immediately on `radius <= 0` — but its caller still
//     runs `apply_breach_law`, so the frame's D plane still updates. Accepting
//     the record and sweeping with an all-zero stencil reproduces exactly
//     that: every scar word passes through unchanged, `surface_texels_touched`
//     does not move, and the breach phase still runs. REJECTED ALTERNATIVE:
//     rejecting the record, which would silently skip a breach/heal the
//     reference performs.
//
// NOT IN THIS BLOCK, deliberately: no VRAM port and no residency directory (a
// stamp naming a non-resident patch is the caller's no-op; there is no
// directory here to check it against), no page CRC, no live-field composition
// (TERRAIN.PATCH), no surface sheet (SURFACE.STAMP owns layer F), no keel or
// bottom generation (`zref::terrain::generate_bottom` is a load-time tool, not
// fabric), and no byte-stencil asset fetch — `zref_terrain.hpp` says plainly
// that "a byte-stencil bake lands with the asset lane", and when it does, the
// paraboloid evaluator below is the one piece that is replaced.
//
// ---------------------------------------------------------------------------
// THE STATED INPUT DOMAIN
// ---------------------------------------------------------------------------
// `bake_dig` computes `dx*dx + dz*dz` in int64. For |dx| >= 2^31.5 that
// overflows and the reference has left its own arithmetic, so a differential
// out there compares two different wraps. The domain is |vx - cx| < 2^31 and
// |vz - cz| < 2^31 in fx16 raw — guaranteed whenever every coordinate lies
// within +-2^30 raw (+-16,384 world metres, four times the +-4,096 m envelope
// SURFACE.STAMP states for the same reason). The datapath here is sized to be
// EXACT over that whole domain and beyond, so nothing in this block wraps; the
// domain is a statement about where the reference is still meaningful.
//
// The envelope lerp is a different matter and is faithful rather than exact:
// `lattice_lerp` finishes with `static_cast<int32_t>(v)` on an int64, which
// WRAPS for an envelope wider than the fx16 word, and its `/ 32` is C++
// integer division — TRUNCATION TOWARD ZERO, which is not an arithmetic shift
// when `env_x1 - env_x0` is negative. Both are reproduced. (SURFACE.STAMP
// records the identical truncation trap for its `/ 128`.)
//
// Conservative SystemVerilog subset only (charter §2).

module zhao_terrain_bake (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // the §9.2 frame window
    // -----------------------------------------------------------------------
    input  logic       frame_start_i,       // 1 cycle: open a new bake window
    output logic       budget_full_o,       // BAKE_PATCH_BUDGET reached
    output logic [7:0] bakes_this_frame_o,

    // -----------------------------------------------------------------------
    // stamp_results: ONE patch-bake record (see the header on what this is)
    // -----------------------------------------------------------------------
    input  logic               cmd_valid_i,
    output logic               cmd_ready_o,
    input  logic        [15:0] cmd_patch_id_i,
    input  logic signed [31:0] cmd_cx_i,          // stencil centre, fx16 raw
    input  logic signed [31:0] cmd_cz_i,
    input  logic signed [31:0] cmd_radius_i,      // fx16 raw; <= 0 writes nothing
    input  logic signed [31:0] cmd_depth_from_i,  // fx16 raw, absolute depth
    input  logic signed [31:0] cmd_depth_to_i,
    input  logic signed [31:0] cmd_env_x0_i,      // the patch envelope, fx16 raw
    input  logic signed [31:0] cmd_env_z0_i,
    input  logic signed [31:0] cmd_env_x1_i,
    input  logic signed [31:0] cmd_env_z1_i,
    input  logic               cmd_dual_i,        // layer C present
    input  logic               cmd_cells_i,       // layer D present
    input  logic        [15:0] cmd_src_id_i,
    output logic        [15:0] trace_patch_id_o,  // the record under bake

    // -----------------------------------------------------------------------
    // DIG phase: layer B read-modify-write, 33x33 vertices, z-then-x
    // -----------------------------------------------------------------------
    // The block OWNS the sweep (chosen B1), so it must say which lattice word
    // it wants: `vtx_vi_o`/`vtx_vj_o` is the read address a layer reader needs,
    // and it is stable for the whole time `vtx_ready_o` can be high.
    output logic        [ 5:0] vtx_vi_o,
    output logic        [ 5:0] vtx_vj_o,
    input  logic               vtx_valid_i,
    output logic               vtx_ready_o,
    input  logic signed [15:0] vtx_base_i,    // layer A
    input  logic signed [15:0] vtx_scar_i,    // layer B in
    input  logic signed [15:0] vtx_bottom_i,  // layer C
    input  logic               vtx_nobake_i,  // §3.3 corner shadow (chosen B3)

    output logic               sc_valid_o,
    input  logic               sc_ready_i,
    output logic signed [15:0] sc_scar_o,     // layer B out
    output logic        [ 5:0] sc_vi_o,
    output logic        [ 5:0] sc_vj_o,
    output logic               sc_touched_o,  // inside the stencil
    output logic               sc_meets_o,    // base + scar <= bottom (§3.4)
    output logic               sc_clamped_o,  // the no_bake clamp fired
    output logic        [15:0] sc_src_id_o,

    // -----------------------------------------------------------------------
    // BREACH phase: layer D read-modify-write, 32x32 cells, z-then-x
    // -----------------------------------------------------------------------
    output logic [5:0] cell_ci_o,
    output logic [5:0] cell_cj_o,
    input  logic       cell_valid_i,
    output logic       cell_ready_o,
    input  logic [7:0] cell_state_i,

    output logic       cs_valid_o,
    input  logic       cs_ready_i,
    output logic [7:0] cs_state_o,
    output logic [5:0] cs_ci_o,
    output logic [5:0] cs_cj_o,
    output logic       cs_event_o,  // a §3.4 transition: the trace event
    output logic [1:0] cs_sub_o,    // the new substance, valid with cs_event_o
    output logic [15:0] cs_src_id_o,

    // -----------------------------------------------------------------------
    // status, counters
    // -----------------------------------------------------------------------
    output logic        dig_done_o,       // 1-cycle pulse: layer B complete
    output logic        bake_done_o,      // 1-cycle pulse: the record retired
    output logic        breach_active_o,  // the block wants cells, not vertices
    output logic [31:0] surface_texels_touched_o,
    output logic [31:0] breach_events_o,
    output logic [31:0] scar_saturations_o,
    output logic [31:0] nobake_clamps_o,
    // OWNER RULING 2026-08-24: MAX_BAKE_RADIUS = 512 m. A larger radius is
    // REJECTED, never clamped -- a clamp would silently reshape a crater the
    // designer asked for, which is the kind of quiet substitution this project
    // refuses elsewhere (SetView rejects rather than clamps for the same
    // reason). Counted so a rejected bake is visible rather than merely absent.
    output logic [31:0] bake_radius_rejects_o,
    output logic        idle_o
);

  // ---- frozen constants ----------------------------------------------------
  localparam int unsigned Lat = 33;  // Island Patch v1 lattice (terrain_rules §2)
  localparam logic [5:0] LatMax = 6'd32;  // last lattice index
  localparam logic [5:0] CellMax = 6'd31;  // last cell index
  localparam logic [7:0] BakePatchBudget = 8'd64;  // terrain_rules §9.2, frozen

  // ---- cell state byte (terrain_rules §3.3) -------------------------------
  localparam logic [1:0] SubSolid = 2'd0;
  localparam logic [1:0] SubVoidAuthored = 2'd1;
  localparam logic [1:0] SubVoidBreached = 2'd2;

  // ---- states --------------------------------------------------------------
  localparam logic [2:0] StIdle = 3'd0;
  localparam logic [2:0] StVtx = 3'd1;  // waiting for a vertex
  localparam logic [2:0] StDiv = 3'd2;  // the 17-step stencil divide
  localparam logic [2:0] StEmit = 3'd3;  // publish the scar word
  localparam logic [2:0] StCell = 3'd4;  // waiting for a cell
  logic [2:0] state;

  // -------------------------------------------------------------------------
  // lattice_lerp (zref::terrain::lattice_lerp with den = 32), FAITHFUL
  // -------------------------------------------------------------------------
  // a + (b - a) * num / 32, with `+ den/2` before a division that TRUNCATES
  // TOWARD ZERO, and a final wrapping narrow to 32 bits. The truncation is
  // visible only for a negative numerator (an inverted envelope); it is not an
  // arithmetic shift and it is not elided.
  function automatic logic signed [31:0] lat_lerp(input logic signed [31:0] a,
                                                  input logic signed [31:0] b,
                                                  input logic [5:0] num);
    logic signed [32:0] span;
    logic signed [39:0] prod;
    logic signed [39:0] nadj;
    logic signed [39:0] quot;
    // `static_cast<int32_t>(v)` on an int64: the high bits are DROPPED, which
    // is the reference's own wrap and not an unread result.
    /* verilator lint_off UNUSEDSIGNAL */
    logic signed [39:0] sum;
    /* verilator lint_on UNUSEDSIGNAL */
    begin
      span = $signed({b[31], b}) - $signed({a[31], a});
      prod = span * $signed({1'b0, num});
      nadj = prod + 40'sd16;
      quot = nadj >>> 5;
      if (nadj[39] && (nadj[4:0] != 5'd0)) quot = quot + 40'sd1;  // toward zero
      sum = {{8{a[31]}}, a} + quot;
      lat_lerp = sum[31:0];  // static_cast<int32_t>: wraps, and so does this
    end
  endfunction

  // -------------------------------------------------------------------------
  // the held record
  // -------------------------------------------------------------------------
  logic signed [31:0] c_cx, c_cz, c_from, c_to;
  logic signed [31:0] c_x0, c_z0, c_x1, c_z1;
  logic               c_dual, c_cells;
  logic        [15:0] c_src;
  logic        [62:0] c_r2;  // radius^2, unsigned; 0 when radius <= 0 (B5)

  // 512.0 m in fx16 raw = 512 * 65536 = 0x0200_0000.
  //
  // WHY 512 AND NOT MORE, from the ruling's own arithmetic: at the largest
  // legal pitch a patch is 128 m across and ~181 m corner to corner, so the
  // farthest swept vertex of a barely-intersecting patch sits under ~694 m from
  // the stencil centre -- inside signed-27-bit Q16.16 (+-1024 m). That is what
  // lets `dx`/`dz` be re-domained around the CENTRE instead of carrying
  // absolute island coordinates, which is the whole point.
  localparam logic signed [31:0] MAX_BAKE_RADIUS_RAW = 32'sh0200_0000;
  wire radius_illegal = (cmd_radius_i > MAX_BAKE_RADIUS_RAW);

  logic [5:0] vi, vj;  // lattice indices, 0..32
  logic [5:0] ci, cj;  // cell indices, 0..31

  // ---- the vertex under evaluation ----------------------------------------
  logic signed [15:0] h_base, h_scar, h_bottom;
  logic               h_nobake;
  logic               v_covered;  // d2 < r2

  // ---- the stencil divider -------------------------------------------------
  // s = ((r2 - d2) << 16 + r2/2) / r2, exactly `bake_dig`'s __int128 form.
  // 17 restoring steps from bit 16 down: sn < r2 * (2^16 + 1), so the quotient
  // is at most 65536 and 17 bits hold it. That bound is STRUCTURAL, and it is
  // what makes zhao_terrain_bake_delta's saturate unreachable.
  logic [79:0] div_rem;
  logic [79:0] div_dsh;
  logic [16:0] div_quo;
  logic [ 4:0] div_cnt;

  // ---- the meets plane (chosen B2) ----------------------------------------
  logic [32:0] meets_row[Lat];

  // -------------------------------------------------------------------------
  // the placed lattice point and the radial test (bake_dig, verbatim)
  // -------------------------------------------------------------------------
  logic signed [31:0] vx, vz;
  assign vx = lat_lerp(c_x0, c_x1, vi);
  assign vz = lat_lerp(c_z0, c_z1, vj);

  logic signed [32:0] dx, dz;
  assign dx = $signed({vx[31], vx}) - $signed({c_cx[31], c_cx});
  assign dz = $signed({vz[31], vz}) - $signed({c_cz[31], c_cz});

  // dx and dz are exact 33-bit signed differences, so the squares are exact
  // 66-bit non-negative values and the sum is exact in 67. Nothing here wraps
  // anywhere in the int32 input space; see the header on where the REFERENCE
  // stops being exact.
  logic signed [65:0] dx2, dz2;
  assign dx2 = dx * dx;
  assign dz2 = dz * dz;

  // Everything from here runs at the divider's width so no intermediate ever
  // needs a partial select (which is also what keeps the lint lane quiet).
  logic [66:0] d2;
  assign d2 = {1'b0, dx2} + {1'b0, dz2};

  logic [79:0] d2_ext, r2_ext;
  assign d2_ext = {13'b0, d2};
  assign r2_ext = {17'b0, c_r2};

  logic covers;
  assign covers = d2_ext < r2_ext;

  // sn = ((r2 - d2) << 16) + r2/2, and the initial shifted divisor: `bake_dig`
  // computes both in __int128 and so, in effect, does this.
  logic [79:0] sn_init;
  assign sn_init = ((r2_ext - d2_ext) << 16) + (r2_ext >> 1);

  logic [79:0] dsh_init;
  assign dsh_init = r2_ext << 16;

  // -------------------------------------------------------------------------
  // the incremental delta (the factored, proved arithmetic core)
  // -------------------------------------------------------------------------
  logic        [16:0] stencil;
  logic signed [31:0] delta16;
  logic               delta_sat;

  assign stencil = v_covered ? div_quo : 17'd0;

  zhao_terrain_bake_delta u_delta (
      .stencil_i   (stencil),
      .depth_from_i(c_from),
      .depth_to_i  (c_to),
      .delta_o     (delta16),
      .sat_o       (delta_sat)
  );

  // -------------------------------------------------------------------------
  // scar = scar + delta, the no_bake clamp, the height16 rails
  // -------------------------------------------------------------------------
  // The clamp and the rails apply to TOUCHED vertices ONLY: `bake_dig`
  // `continue`s past an uncovered vertex before either can run, so an
  // out-of-range scar word already in layer B is passed through untouched
  // rather than quietly corrected. That asymmetry is the reference's and it is
  // kept.
  logic signed [33:0] scar_sum;
  assign scar_sum = {{18{h_scar[15]}}, h_scar} + {{2{delta16[31]}}, delta16};

  // min_scar = bottom + 1 - base: the protected vertex keeps
  // base + scar >= bottom + one height16 LSB, so the §3.4 equality is
  // unreachable on it and the cell can never breach.
  logic signed [33:0] min_scar;
  assign min_scar = {{18{h_bottom[15]}}, h_bottom} + 34'sd1 - {{18{h_base[15]}}, h_base};

  logic guard_on;
  assign guard_on = c_dual && c_cells && h_nobake;

  logic clamp_fires;
  assign clamp_fires = v_covered && guard_on && (scar_sum < min_scar);

  logic signed [33:0] scar_guarded;
  assign scar_guarded = clamp_fires ? min_scar : scar_sum;

  logic rail_hi, rail_lo;
  assign rail_hi = v_covered && (scar_guarded > 34'sd32767);
  assign rail_lo = v_covered && (scar_guarded < -34'sd32768);

  logic signed [15:0] scar_new;
  assign scar_new = !v_covered ? h_scar :
                    rail_hi    ? 16'sh7FFF :
                    rail_lo    ? 16'sh8000 : scar_guarded[15:0];

  // apply_breach_law's `meets_bottom`, on the height16 grid: the fx16 forms
  // are these values << 8, so the compare is identical and exact.
  // VERILOG SIGNEDNESS. A concatenation is UNSIGNED, and a comparison goes
  // unsigned if either side is — the trap that cost GEOM.BINNER 29 tiles and
  // that the composition test caught here: an unsigned compare made a composed
  // height of -200 read as 261,944 and every cell on the island breached.
  logic signed [17:0] composed18, bottom18;
  assign composed18 = $signed({{2{h_base[15]}}, h_base}) + $signed({{2{scar_new[15]}}, scar_new});
  assign bottom18   = $signed({{2{h_bottom[15]}}, h_bottom});

  logic meets_new;
  assign meets_new = composed18 <= bottom18;

  // -------------------------------------------------------------------------
  // the breach law (apply_breach_law, verbatim)
  // -------------------------------------------------------------------------
  logic [32:0] m_row_lo, m_row_hi;
  assign m_row_lo = meets_row[cj];
  assign m_row_hi = meets_row[cj+6'd1];

  logic all4;
  assign all4 = m_row_lo[ci] && m_row_lo[ci+6'd1] && m_row_hi[ci] && m_row_hi[ci+6'd1];

  logic [1:0] sub_in;
  assign sub_in = cell_state_i[1:0];

  logic cell_nobake;
  assign cell_nobake = cell_state_i[2];

  logic breach_fires, heal_fires;
  assign breach_fires = (sub_in == SubSolid) && all4 && !cell_nobake;
  // VOID_AUTHORED never becomes ground (§3.4); the heal arm does NOT consult
  // no_bake, and that is the reference's asymmetry, not an oversight: a
  // protected cell that somehow reached VOID_BREACHED must still be able to
  // come back.
  assign heal_fires = (sub_in == SubVoidBreached) && !all4;

  logic [1:0] sub_out;
  assign sub_out = breach_fires ? SubVoidBreached : heal_fires ? SubSolid : sub_in;

  logic cell_event;
  assign cell_event = (sub_in != SubVoidAuthored) && (breach_fires || heal_fires);

  // -------------------------------------------------------------------------
  // handshakes
  // -------------------------------------------------------------------------
  logic sc_free, cs_free;
  assign sc_free = !sc_valid_o || sc_ready_i;
  assign cs_free = !cs_valid_o || cs_ready_i;

  // A vertex is taken only when the scar register can receive its answer, and
  // the same argument TERRAIN.PATCH writes out applies: `sc_free` at the
  // accept cycle T means the register is free at T+1, and nothing raises it
  // again until this vertex's own divide completes. The result register is
  // therefore ALWAYS free at StEmit, which is why StEmit carries no stall term
  // (an unreachable one is what the directed suite caught in TERRAIN.PATCH).
  assign vtx_ready_o = (state == StVtx) && sc_free;
  assign cell_ready_o = (state == StCell) && cs_free;
  assign cmd_ready_o = (state == StIdle) && !budget_full_o;

  assign vtx_vi_o = vi;
  assign vtx_vj_o = vj;
  assign cell_ci_o = ci;
  assign cell_cj_o = cj;

  assign breach_active_o = (state == StCell);
  assign idle_o = (state == StIdle) && !sc_valid_o && !cs_valid_o;

  logic [7:0] bakes_this_frame;
  assign bakes_this_frame_o = bakes_this_frame;
  assign budget_full_o = bakes_this_frame >= BakePatchBudget;

  wire last_vtx = (vi == LatMax) && (vj == LatMax);
  wire last_cell = (ci == CellMax) && (cj == CellMax);

  // radius <= 0: `bake_dig` returns before it touches anything, which this
  // block reproduces by carrying r2 = 0 (chosen B5) — `covers` is then false
  // at every vertex, so every scar word passes through.
  wire signed [62:0] radius_sq = cmd_radius_i * cmd_radius_i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= StIdle;
      c_cx <= '0;
      c_cz <= '0;
      c_from <= '0;
      c_to <= '0;
      c_x0 <= '0;
      c_z0 <= '0;
      c_x1 <= '0;
      c_z1 <= '0;
      c_dual <= 1'b0;
      c_cells <= 1'b0;
      c_src <= '0;
      c_r2 <= '0;
      vi <= '0;
      vj <= '0;
      ci <= '0;
      cj <= '0;
      h_base <= '0;
      h_scar <= '0;
      h_bottom <= '0;
      h_nobake <= 1'b0;
      v_covered <= 1'b0;
      div_rem <= '0;
      div_dsh <= '0;
      div_quo <= '0;
      div_cnt <= '0;
      for (int k = 0; k < int'(Lat); k++) meets_row[k] <= '0;
      sc_valid_o <= 1'b0;
      sc_scar_o <= '0;
      sc_vi_o <= '0;
      sc_vj_o <= '0;
      sc_touched_o <= 1'b0;
      sc_meets_o <= 1'b0;
      sc_clamped_o <= 1'b0;
      sc_src_id_o <= '0;
      cs_valid_o <= 1'b0;
      cs_state_o <= '0;
      cs_ci_o <= '0;
      cs_cj_o <= '0;
      cs_event_o <= 1'b0;
      cs_sub_o <= '0;
      cs_src_id_o <= '0;
      trace_patch_id_o <= '0;
      dig_done_o <= 1'b0;
      bake_done_o <= 1'b0;
      bakes_this_frame <= '0;
      surface_texels_touched_o <= '0;
      breach_events_o <= '0;
      scar_saturations_o <= '0;
      nobake_clamps_o <= '0;
      bake_radius_rejects_o <= '0;
    end else begin
      dig_done_o  <= 1'b0;
      bake_done_o <= 1'b0;

      // The §9.2 window. Counting ACCEPTANCES means an in-flight bake is never
      // disturbed by a frame boundary (chosen B4).
      if (frame_start_i) bakes_this_frame <= '0;

      if (sc_valid_o && sc_ready_i) sc_valid_o <= 1'b0;
      if (cs_valid_o && cs_ready_i) cs_valid_o <= 1'b0;

      case (state)
        StIdle: begin
          // A RECORD WHOSE RADIUS EXCEEDS THE LAW IS REJECTED WHOLE. It is
          // consumed so the producer cannot wedge, it sweeps nothing, and it is
          // COUNTED. Deliberately unlike B5's `radius <= 0`, which is a lawful
          // no-op that still sweeps: a zero radius is a legal request for
          // nothing, an oversized one is a request the machine refuses.
          if (cmd_valid_i && cmd_ready_o && radius_illegal) begin
            bake_radius_rejects_o <= bake_radius_rejects_o + 32'd1;
          end else if (cmd_valid_i && cmd_ready_o) begin
            c_cx <= cmd_cx_i;
            c_cz <= cmd_cz_i;
            c_from <= cmd_depth_from_i;
            c_to <= cmd_depth_to_i;
            c_x0 <= cmd_env_x0_i;
            c_z0 <= cmd_env_z0_i;
            c_x1 <= cmd_env_x1_i;
            c_z1 <= cmd_env_z1_i;
            c_dual <= cmd_dual_i;
            c_cells <= cmd_cells_i;
            c_src <= cmd_src_id_i;
            trace_patch_id_o <= cmd_patch_id_i;
            c_r2 <= (cmd_radius_i > 0) ? radius_sq[62:0] : 63'd0;
            vi <= '0;
            vj <= '0;
            ci <= '0;
            cj <= '0;
            bakes_this_frame <= frame_start_i ? 8'd1 : (bakes_this_frame + 8'd1);
            state <= StVtx;
          end
        end

        StVtx: begin
          if (vtx_valid_i && vtx_ready_o) begin
            h_base <= vtx_base_i;
            h_scar <= vtx_scar_i;
            h_bottom <= vtx_bottom_i;
            h_nobake <= vtx_nobake_i;
            v_covered <= covers;
            if (covers) begin
              div_rem <= sn_init;
              div_dsh <= dsh_init;
              div_quo <= '0;
              div_cnt <= 5'd16;
              state   <= StDiv;
            end else begin
              div_quo <= '0;
              state   <= StEmit;
            end
          end
        end

        StDiv: begin
          if (div_rem >= div_dsh) begin
            div_rem <= div_rem - div_dsh;
            div_quo <= {div_quo[15:0], 1'b1};
          end else begin
            div_quo <= {div_quo[15:0], 1'b0};
          end
          div_dsh <= div_dsh >> 1;
          if (div_cnt == 5'd0) state <= StEmit;
          else div_cnt <= div_cnt - 5'd1;
        end

        StEmit: begin
          sc_valid_o <= 1'b1;
          sc_scar_o <= scar_new;
          sc_vi_o <= vi;
          sc_vj_o <= vj;
          sc_touched_o <= v_covered;
          sc_meets_o <= meets_new;
          sc_clamped_o <= clamp_fires;
          sc_src_id_o <= c_src;
          if (meets_new) meets_row[vj] <= meets_row[vj] | (33'd1 << vi);
          else meets_row[vj] <= meets_row[vj] & ~(33'd1 << vi);
          if (v_covered) surface_texels_touched_o <= surface_texels_touched_o + 32'd1;
          if (clamp_fires) nobake_clamps_o <= nobake_clamps_o + 32'd1;
          if (rail_hi || rail_lo || (v_covered && delta_sat))
            scar_saturations_o <= scar_saturations_o + 32'd1;
          if (last_vtx) begin
            dig_done_o <= 1'b1;
            // apply_breach_law returns EMPTY without a bottom plane (a legacy
            // page cannot breach) and without a cell-state plane; the phase is
            // then skipped entirely rather than run against nothing.
            if (c_dual && c_cells) begin
              state <= StCell;
            end else begin
              bake_done_o <= 1'b1;
              state <= StIdle;
            end
          end else begin
            if (vi == LatMax) begin
              vi <= '0;
              vj <= vj + 6'd1;
            end else begin
              vi <= vi + 6'd1;
            end
            state <= StVtx;
          end
        end

        StCell: begin
          if (cell_valid_i && cell_ready_o) begin
            cs_valid_o <= 1'b1;
            cs_state_o <= {cell_state_i[7:2], sub_out};
            cs_ci_o <= ci;
            cs_cj_o <= cj;
            cs_event_o <= cell_event;
            cs_sub_o <= sub_out;
            cs_src_id_o <= c_src;
            if (cell_event) breach_events_o <= breach_events_o + 32'd1;
            if (last_cell) begin
              bake_done_o <= 1'b1;
              state <= StIdle;
            end else begin
              if (ci == CellMax) begin
                ci <= '0;
                cj <= cj + 6'd1;
              end else begin
                ci <= ci + 6'd1;
              end
            end
          end
        end

        default: state <= StIdle;
      endcase
    end
  end

endmodule : zhao_terrain_bake
