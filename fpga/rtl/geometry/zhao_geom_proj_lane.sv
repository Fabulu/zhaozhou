// zhao_geom_proj_lane.sv — GEOMETRY'S HALF OF THE SHARED PROJECTOR: the arena
// shell fed straight off a projection service's CLIENT A result port.
//
//     zhao_proj_subsystem          (already exists: one core + terrain on B)
//         | a_valid_o, a_x_o, a_y_o, a_d_o, a_w_o, a_behind_o, a_payload_o
//         v
//     zhao_geom_proj_lane  --->  zhao_geom_wcache.fill
//                                (the {arena,index} rider IS the fill address)
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// `zhao_proj_subsystem` composes ONE projector with terrain as client B and
// deliberately leaves client A exposed. Its header names the gap exactly:
//
//   "client A raw -- the geometry producer (GEOM.SKIN/WARP into GEOM.WCACHE)
//    is not composed anywhere in this tree; when it is, it plugs in here and
//    nothing in this file changes."
//
// The roadmap's R3 row says the same in its own words -- *"Real geometry
// client ... remain open"* -- and it is why the shared projector cannot be
// SELECTED even though it is built, fitted and cheaper:
//
//   two unshared wrappers   ~12,400 ALM   66 DSP   (two leaf rows)
//   one shared service       6,598 ALM    33 DSP   (@cheque-price, clean tree,
//                                                   BOTH clients on pins)
//
// Selecting the service while its geometry half does not exist would be the
// golden path's own error -- "an omitted function is an error, not a free
// saving". This file is that function, and NOTHING IN IT COMPUTES.
//
// ---------------------------------------------------------------------------
// A SIBLING, NOT A WRAPPER, AND THE REASON IS PORT SURFACE
// ---------------------------------------------------------------------------
// The first version of this file instantiated `zhao_proj_subsystem` and passed
// its terrain-side control through -- open/seal/ref/out, about forty ports that
// this module has no opinion about. Verilator's PINMISSING said so immediately.
// Wrapping a block to reach one of its ports means owning all of them.
//
// So this is a sibling: the composer instantiates `zhao_proj_subsystem` and
// this, and wires client A between them. Each file then has only the ports it
// actually decides something about, which is also exactly how the subsystem's
// own header describes the plug-in.
//
// ---------------------------------------------------------------------------
// THE WIDTHS WERE CHECKED BEFORE A LINE WAS WRITTEN
// ---------------------------------------------------------------------------
// Client A's result carries exactly the fields `zhao_geom_wcache` stores, in
// the order `zhao_terrain_wcache` already packs them on the client-B side:
//
//     {behind, w[30:0], d[31:0], y[20:0], x[20:0]}  =  1+31+32+21+21 = 106
//
// and `zhao_geom_wcache`'s `PAYLOAD_W` is 106. One convention describes both
// halves of the subsystem. The rider matches too: `PAYLOAD_A_W` is 16 and
// `ARENA_W + INDEX_W` is 2 + 12 = 14, so the {arena, index} the producer put in
// comes back out of `a_payload_o` and IS the fill address -- no adapter, no
// FIFO, no shadow state, so a fill can never drift from the vertex it
// describes. That is the property the terrain differentials lean on.
//
// ---------------------------------------------------------------------------
// THE RESULT PORT HAS NO BACKPRESSURE, AND THAT IS SAFE HERE
// ---------------------------------------------------------------------------
// `zhao_project_service` pushes client A's results unconditionally: there is no
// `a_ready_i`. The consumer must never stall, and this one cannot --
// `zhao_geom_wcache` instantiates one `zhao_vertex_arena`, whose `fill_ready_o`
// is a STATED CONSTANT:
//
//     // zhao_vertex_arena.sv
//     // Always accept: neither channel can stall ... Stated as constants so a
//     // future banking change has to change them deliberately rather than by
//     // accident.
//     assign fill_ready_o = 1'b1;
//
// `fill_landed_o` is exposed instead of a ready, for the same reason the
// terrain side exposes it: the core is deep, so a sequencer that wants to seal
// an arena must count LANDINGS, not acceptances.
//
// Conservative SystemVerilog subset only (charter §2).
`default_nettype none

module zhao_geom_proj_lane #(
    parameter int unsigned ARENAS      = 2,
    parameter int unsigned DEPTH       = 1089,
    parameter int unsigned GEN_W       = 8,
    parameter int unsigned PAYLOAD_A_W = 16,
    parameter int unsigned PAYLOAD_W   = 106,
    parameter int unsigned INDEX_W     = $clog2(DEPTH) + 1,
    parameter int unsigned ARENA_W     = $clog2(ARENAS) + 1
) (
    input var logic clk,
    input var logic rst_n,

    // ---- CLIENT A's RESULT PORT, as the service presents it ------------------
    input  wire                     a_valid_i,
    input  wire signed [20:0]       a_x_i,
    input  wire signed [20:0]       a_y_i,
    input  wire signed [31:0]       a_d_i,
    input  wire        [30:0]       a_w_i,
    input  wire                     a_behind_i,
    // The top PAYLOAD_A_W-(ARENA_W+INDEX_W) bits are the zero padding this
    // module writes into the rider, so they are deliberately not read back.
    /* verilator lint_off UNUSEDSIGNAL */
    input  wire [PAYLOAD_A_W-1:0]   a_payload_i,
    /* verilator lint_on UNUSEDSIGNAL */

    // ---- the rider a producer must put IN, exposed so one file owns the layout
    input  wire [ARENA_W-1:0]       rider_arena_i,
    input  wire [INDEX_W-1:0]       rider_index_i,
    output wire [PAYLOAD_A_W-1:0]   rider_payload_o,

    // ---- the geometry arena's own control ------------------------------------
    input  wire                     open_i,
    input  wire [ARENA_W-1:0]       open_arena_i,
    output wire [GEN_W-1:0]         open_gen_o,
    input  wire                     org_we_i,
    input  wire [ARENA_W-1:0]       org_arena_i,
    input  wire signed [31:0]       org_x_i,
    input  wire signed [31:0]       org_y_i,
    input  wire signed [31:0]       org_z_i,
    input  wire                     seal_i,
    input  wire [ARENA_W-1:0]       seal_arena_i,

    // ---- landings, so a sequencer seals on ARRIVAL and not on acceptance ------
    output wire                     fill_landed_o,
    output wire [ARENA_W-1:0]       fill_arena_o,
    // The landed vertex's INDEX, beside its arena (2026-09-19). GEOM.VATTR
    // writes the vertex's attribute row at the moment its position lands, keyed
    // by the same {arena, index}; the rider layout lives HERE, so the index is
    // unpacked here too rather than re-sliced by the composer.
    output wire [INDEX_W-1:0]       fill_index_o,

    // ---- lookups, straight out of the cache -----------------------------------
    input  wire                     look_valid_i,
    output wire                     look_ready_o,
    input  wire [ARENA_W-1:0]       look_arena_i,
    input  wire [GEN_W-1:0]         look_gen_i,
    input  wire [INDEX_W-1:0]       look_index_i,
    output wire                     rep_valid_o,
    output wire                     rep_hit_o,
    output wire                     rep_refuse_o,
    output wire [PAYLOAD_W-1:0]     rep_payload_o,
    output wire signed [31:0]       rep_org_x_o,
    output wire signed [31:0]       rep_org_y_o,
    output wire signed [31:0]       rep_org_z_o,

    output wire [31:0]              arena_hits_o,
    output wire [31:0]              arena_misses_o,

    // FORWARDED, NOT DROPPED. An error output left open is an error nobody
    // reads, and this repository has the counter-reading-zero lesson written
    // down twice. Both of these are faults, so they leave the module.
    output wire [31:0]              arena_refusals_o,
    output wire                     arena_overflow_o
);

  // THE RIDER MUST FIT, AND A COMMENT IS NOT A CHECK.
  //
  // The no-adapter property rests on {arena, index} surviving the round trip
  // through the service's client-A payload. If a future DEPTH or ARENAS pushed
  // that past PAYLOAD_A_W the address would be silently truncated and fills
  // would land in the wrong slot -- a corruption with no counter on it.
  initial begin
    if (ARENA_W + INDEX_W > PAYLOAD_A_W)
      $fatal(1, "zhao_geom_proj_lane: rider is %0d bits (ARENA_W %0d + INDEX_W %0d) but PAYLOAD_A_W is %0d",
             ARENA_W + INDEX_W, ARENA_W, INDEX_W, PAYLOAD_A_W);
    if (PAYLOAD_W != 106)
      $fatal(1, "zhao_geom_proj_lane: PAYLOAD_W is %0d; the client-A result fields pack to 106", PAYLOAD_W);
  end

  // The rider is built HERE and unpacked HERE, so the layout is stated once.
  assign rider_payload_o =
      {{(PAYLOAD_A_W-ARENA_W-INDEX_W){1'b0}}, rider_arena_i, rider_index_i};

  // The top PAYLOAD_A_W-ARENA_W-INDEX_W bits are the zero padding this module
  // wrote into the rider, so they are deliberately not read back.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [ARENA_W-1:0] fill_arena_c = a_payload_i[ARENA_W+INDEX_W-1 -: ARENA_W];
  /* verilator lint_on UNUSEDSIGNAL */
  wire [INDEX_W-1:0] fill_index_c = a_payload_i[INDEX_W-1:0];

  // Packed in the SAME field order zhao_terrain_wcache uses on the client-B
  // side, so one convention describes both halves of the subsystem.
  wire [PAYLOAD_W-1:0] fill_payload_c = {a_behind_i, a_w_i, a_d_i, a_y_i, a_x_i};

  wire fill_ready_unused;   // the primitive never stalls a fill; see the header

  zhao_geom_wcache #(
      .ARENAS   (ARENAS),
      .DEPTH    (DEPTH),
      .PAYLOAD_W(PAYLOAD_W),
      .GEN_W    (GEN_W)
  ) u_wcache (
      .clk           (clk),
      .rst_n         (rst_n),
      .open_i        (open_i),
      .open_arena_i  (open_arena_i),
      .open_gen_o    (open_gen_o),
      .org_we_i      (org_we_i),
      .org_arena_i   (org_arena_i),
      .org_x_i       (org_x_i),
      .org_y_i       (org_y_i),
      .org_z_i       (org_z_i),

      .fill_valid_i  (a_valid_i),
      .fill_ready_o  (fill_ready_unused),
      .fill_arena_i  (fill_arena_c),
      .fill_index_i  (fill_index_c),
      .fill_payload_i(fill_payload_c),

      .seal_i        (seal_i),
      .seal_arena_i  (seal_arena_i),
      .look_valid_i  (look_valid_i),
      .look_ready_o  (look_ready_o),
      .look_arena_i  (look_arena_i),
      .look_gen_i    (look_gen_i),
      .look_index_i  (look_index_i),
      .rep_valid_o   (rep_valid_o),
      .rep_hit_o     (rep_hit_o),
      .rep_refuse_o  (rep_refuse_o),
      .rep_payload_o (rep_payload_o),
      .rep_org_x_o   (rep_org_x_o),
      .rep_org_y_o   (rep_org_y_o),
      .rep_org_z_o   (rep_org_z_o),
      .arena_hits_o    (arena_hits_o),
      .arena_misses_o  (arena_misses_o),
      .arena_refusals_o(arena_refusals_o),
      .arena_overflow_o(arena_overflow_o)
  );

  assign fill_landed_o = a_valid_i;
  assign fill_arena_o  = fill_arena_c;
  assign fill_index_o  = fill_index_c;

endmodule : zhao_geom_proj_lane

`default_nettype wire
