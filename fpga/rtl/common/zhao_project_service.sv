// zhao_project_service.sv — ONE physical projector for both clients.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS: 66 OF THE MACHINE'S 192 DSP ARE THE SAME NINE MULTIPLIERS
// ---------------------------------------------------------------------------
// Measured 2026-09-09 from the ALM audit and the instantiation graph:
//
//     zhao_geom_project      6,199 ALM   33 DSP
//     zhao_terrain_project   6,068 ALM   33 DSP
//                           12,267 ALM   66 DSP
//
// Both instantiate `zhao_project_core`, differing ONLY in `PAYLOAD_W` (16 against
// 42) — a tag rider carried alongside the arithmetic, not an arithmetic
// parameter. Sharing the source file did not share the silicon.
//
// The core holds NINE `mul32` call sites, three per matrix row (X, Y, W), plus
// two viewport products. A function call is not shared hardware — the lesson
// `unit_mul_logic` taught in the material combiner. Nine signed 32x32 at three
// DSP each is 27; with the viewport scalings that reconstructs the measured 33
// per engine. That reconstruction is why this is a structural account and not a
// story.
//
// Against 192 counted DSP, a 112-DSP device and an owner target of <= 94, one
// physical core instead of two is the largest single DSP item available, and it
// requires NO new arithmetic.
//
// ---------------------------------------------------------------------------
// THE THREE THINGS THAT HAD TO BE TRUE, AND THE EVIDENCE FOR EACH
// ---------------------------------------------------------------------------
// 1. ONE CORE IS FAST ENOUGH. The core's own header: "One vertex in, one
//    projected vertex out, 36 clocks later, fully pipelined at one vertex per
//    clock", and at its stage-5b repair: "LATENCY, NOT INITIATION INTERVAL.
//    This is a pipeline register, so a vertex still enters every cycle."
//    The rescue roadmap's two-view stress reaches 903,552 projections against a
//    ~1,333,333-clock window. 903,552 < 1,333,333, so one core at one vertex per
//    clock carries both clients with roughly 32% headroom. **The saving comes
//    from deleting a duplicate provider, never from slowing the survivor** —
//    which the roadmap states as a condition and this module honours.
//
// 2. THE CLIENTS WANT THE SAME MATRIX. The core stores `mat[0:1][0:15]` — one
//    set per VIEW, not per client. That is only sound if both clients project
//    through the same camera. The core says they do, at its own operand note:
//    "The matrix words are fx16 VIEW-PROJECTION coefficients; the coordinates
//    are fx16 WORLD positions." There is no per-object model matrix in this
//    core, so geometry and terrain feeding world-space vertices genuinely share
//    a view-projection per view. Checked, not assumed.
//
//    IF THAT EVER STOPS BEING TRUE — a client needing its own matrix for the
//    same view index — the bank widens to [client][view] and costs REGISTERS,
//    not DSP. The nine multipliers stay shared. Do not respond by cloning the
//    core again.
//
// 3. NOTHING HAS TO BE REWIRED. Neither wrapper is instantiated by any
//    production top today; both are billed manifest roots awaiting composition.
//    So this is a clean-slate provider choice rather than surgery on live
//    wiring, and the two wrappers remain as reference oracles.
//
// ---------------------------------------------------------------------------
// WHAT IS PRESERVED EXACTLY
// ---------------------------------------------------------------------------
// The arithmetic is untouched: this module instantiates the SAME core and adds
// no operator. Guarded W, the behind-eye record, both views, the exact rounding
// and saturation boundaries and the full signed-32 world domain are the core's
// and are not re-implemented here.
//
// LATENCY IS FIXED FROM THE ACCEPTED CYCLE. Each caller's contract latency
// (GEOM 36, TERRAIN 38 with its own two stages) is measured from the cycle its
// vertex is ACCEPTED, which is what a shared provider can promise. A client that
// is not granted this cycle is simply not accepted this cycle; it is never
// accepted and then delayed. `*_ready_o` is the whole of that contract.
//
// ---------------------------------------------------------------------------
// ARBITRATION
// ---------------------------------------------------------------------------
// The core has NO input backpressure — it consumes whenever `en_i && in_valid_i`
// — so the arbiter's only job is to present at most one vertex per clock and to
// tell the loser it was not taken. Round-robin with a toggling priority: the
// client that was granted becomes the low-priority one next time, so a saturated
// client cannot starve the other. With both saturated each gets every other
// clock, which is the fair split of a one-per-clock resource.

`default_nettype none

module zhao_project_service #(
    // Client A's rider (geometry uses 16), client B's rider (terrain uses 42).
    parameter int unsigned PAYLOAD_A_W = 16,
    parameter int unsigned PAYLOAD_B_W = 42
) (
    input  wire                       clk,
    input  wire                       rst_n,

    // ---- ONE configuration bus, shared, identical to the core's ------------
    // Both wrappers already expose exactly this interface and pass it straight
    // through, with the same address map (0..15 matrix, 16 viewport origin,
    // 17 viewport extent). Sharing it is what makes one matrix bank correct.
    input  wire                       cfg_we_i,
    input  wire                       cfg_view_i,
    input  wire [4:0]                 cfg_addr_i,
    input  wire [31:0]                cfg_data_i,

    input  wire                       en_i,

    // ---- client A ---------------------------------------------------------
    input  wire                       a_valid_i,
    output wire                       a_ready_o,
    input  wire signed [31:0]         a_vx_i,
    input  wire signed [31:0]         a_vy_i,
    input  wire signed [31:0]         a_vz_i,
    input  wire                       a_view_i,
    input  wire [PAYLOAD_A_W-1:0]     a_payload_i,

    output wire                       a_valid_o,
    output wire signed [20:0]         a_x_o,
    output wire signed [20:0]         a_y_o,
    output wire signed [31:0]         a_d_o,
    output wire [30:0]                a_w_o,
    output wire                       a_behind_o,
    output wire                       a_view_o,
    output wire [PAYLOAD_A_W-1:0]     a_payload_o,

    // ---- client B ---------------------------------------------------------
    input  wire                       b_valid_i,
    output wire                       b_ready_o,
    input  wire signed [31:0]         b_vx_i,
    input  wire signed [31:0]         b_vy_i,
    input  wire signed [31:0]         b_vz_i,
    input  wire                       b_view_i,
    input  wire [PAYLOAD_B_W-1:0]     b_payload_i,

    output wire                       b_valid_o,
    output wire signed [20:0]         b_x_o,
    output wire signed [20:0]         b_y_o,
    output wire signed [31:0]         b_d_o,
    output wire [30:0]                b_w_o,
    output wire                       b_behind_o,
    output wire                       b_view_o,
    output wire [PAYLOAD_B_W-1:0]     b_payload_o,

    output wire                       busy_o,

    // ---- observation ------------------------------------------------------
    // Grants per client, and the count of clocks BOTH asked and one had to
    // wait. The last is the number that decides whether one core remains the
    // right choice: if contention is a large fraction of grants under a real
    // workload, the two clients' combined demand is near the one-per-clock
    // limit and that is a measurement, not an opinion.
    output logic [31:0]               a_grants_o,
    output logic [31:0]               b_grants_o,
    output logic [31:0]               contended_o
);

  // The rider carries the client id in its top bit. 42 + 1 for terrain, and
  // geometry's 16 rides in the low bits of the same field.
  localparam int unsigned RIDE_W = (PAYLOAD_A_W > PAYLOAD_B_W)
                                 ? PAYLOAD_A_W : PAYLOAD_B_W;
  localparam int unsigned PAY_W  = RIDE_W + 1;
  localparam int unsigned ID_BIT = RIDE_W;

  // ---- round-robin grant --------------------------------------------------
  logic prefer_b_q;                       // whose turn it is when both ask
  wire  both_c   = a_valid_i && b_valid_i;
  wire  grant_a  = a_valid_i && !(both_c &&  prefer_b_q);
  wire  grant_b  = b_valid_i && !(both_c && !prefer_b_q);

  // The core takes a vertex only while enabled; a grant that the core is not
  // enabled to consume is not a grant.
  wire  take_a   = en_i && grant_a;
  wire  take_b   = en_i && grant_b;

  assign a_ready_o = take_a;
  assign b_ready_o = take_b;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      prefer_b_q  <= 1'b0;
      a_grants_o  <= '0;
      b_grants_o  <= '0;
      contended_o <= '0;
    end else begin
      // Flip only on a contended grant. Alternating on every grant would let a
      // lone client hand its turn away to an idle one and gain nothing.
      if (en_i && both_c) begin
        prefer_b_q  <= ~prefer_b_q;
        contended_o <= contended_o + 32'd1;
      end
      if (take_a) a_grants_o <= a_grants_o + 32'd1;
      if (take_b) b_grants_o <= b_grants_o + 32'd1;
    end
  end

  // ---- the single core ----------------------------------------------------
  wire                  core_in_valid = take_a || take_b;
  wire signed [31:0]    core_vx = take_a ? a_vx_i : b_vx_i;
  wire signed [31:0]    core_vy = take_a ? a_vy_i : b_vy_i;
  wire signed [31:0]    core_vz = take_a ? a_vz_i : b_vz_i;
  wire                  core_view = take_a ? a_view_i : b_view_i;

  wire [RIDE_W-1:0] ride_a = {{(RIDE_W - PAYLOAD_A_W) {1'b0}}, a_payload_i};
  wire [RIDE_W-1:0] ride_b = {{(RIDE_W - PAYLOAD_B_W) {1'b0}}, b_payload_i};
  wire [PAY_W-1:0]  core_payload = take_a ? {1'b0, ride_a} : {1'b1, ride_b};

  wire              o_valid;
  wire [PAY_W-1:0]  o_payload;
  wire signed [20:0] o_x, o_y;
  wire signed [31:0] o_d;
  wire        [30:0] o_w;
  wire               o_behind, o_view;

  zhao_project_core #(
      .PAYLOAD_W(PAY_W)
  ) u_core (
      .clk        (clk),
      .rst_n      (rst_n),
      .cfg_we_i   (cfg_we_i),
      .cfg_view_i (cfg_view_i),
      .cfg_addr_i (cfg_addr_i),
      .cfg_data_i (cfg_data_i),
      .en_i       (en_i),
      .in_valid_i (core_in_valid),
      .vx_i       (core_vx),
      .vy_i       (core_vy),
      .vz_i       (core_vz),
      .view_i     (core_view),
      .payload_i  (core_payload),
      .out_valid_o(o_valid),
      .out_x_o    (o_x),
      .out_y_o    (o_y),
      .out_d_o    (o_d),
      .out_w_o    (o_w),
      .out_behind_o(o_behind),
      .out_view_o (o_view),
      .out_payload_o(o_payload),
      .busy_o     (busy_o)
  );

  // ---- result demux -------------------------------------------------------
  // The rider carried the owner all the way through the pipeline, so routing
  // needs no shadow FIFO and cannot drift from the data it describes.
  wire to_b = o_payload[ID_BIT];

  assign a_valid_o   = o_valid && !to_b;
  assign b_valid_o   = o_valid &&  to_b;

  assign a_x_o       = o_x;
  assign a_y_o       = o_y;
  assign a_d_o       = o_d;
  assign a_w_o       = o_w;
  assign a_behind_o  = o_behind;
  assign a_view_o    = o_view;
  assign a_payload_o = o_payload[PAYLOAD_A_W-1:0];

  assign b_x_o       = o_x;
  assign b_y_o       = o_y;
  assign b_d_o       = o_d;
  assign b_w_o       = o_w;
  assign b_behind_o  = o_behind;
  assign b_view_o    = o_view;
  assign b_payload_o = o_payload[PAYLOAD_B_W-1:0];

endmodule

`default_nettype wire
