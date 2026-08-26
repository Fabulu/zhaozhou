// zhao_field_v2_lanemux.sv — FIELD v2's tagged lane serialiser for scalar units.
//
// FIELD v2 issues VECTOR instructions: LANES vertices under one PC. But every
// long-operation unit in the engine (`zhao_field_curve`, `zhao_field_len`,
// `zhao_field_ring`, ...) is SCALAR — it takes one value and answers one value.
//
// So a vector long operation is LANES scalar requests, and something has to
// serialise them, keep track of which lane each reply belongs to, and hand back
// one vector result. That is this block.
//
// ---------------------------------------------------------------------------
// THE COST THIS MAKES VISIBLE, stated plainly because it is the point
// ---------------------------------------------------------------------------
// A shared scalar unit serving LANES lanes costs **LANES x II** per vector
// instruction. At LANES=4 and CURVE's measured-family II of ~23, one vector
// CURVE is ~92 clocks. The barrel front end cannot help: it hides latency by
// running other wavefronts, but the unit's ISSUE RATE is the ceiling, and every
// wavefront wanting CURVE queues behind the same unit.
//
// That is not an argument against this block — it is the measurement the
// architecture needs. `reports/FIELD_V2_MODEL.md` puts CURVE/DCURVE first in
// the work order at 6.13M cycles/frame against a 1.33M budget, and the fix is
// either to pipeline the unit (II -> 1, so LANES x 1) or to replicate it per
// lane (LANES units at II each). Those are different silicon and the choice
// wants a measurement, not a preference.
//
// THIS BLOCK IS THE INTERFACE, NOT THE FIX. It proves the tagging works and
// makes the serialisation cost explicit and measurable. Pipelining or
// replicating the unit behind it changes nothing here.
//
// ---------------------------------------------------------------------------
// THE TAG
// ---------------------------------------------------------------------------
// Every request carries {wavefront, destination}, and the reply carries it
// back. v1 needed no tags: it had one instruction in flight, so a reply could
// only ever belong to the one thing that asked. The moment several wavefronts
// share a unit, an untagged reply is a reply to whoever happens to be waiting —
// which is the defect class this whole redesign exists to make impossible.
//
// The tag is captured ONCE at accept and travels with the transaction. It is
// not recomputed from whatever the front end is doing when the reply lands.
//
// The OPERAND BUNDLE and the UNIT SELECTOR are captured on the same terms. The
// bundle is five components wide because zhao_field_len's DIST2 takes five
// (a0,a1,a2,b0,b1) while zhao_field_curve takes one; a unit that reads fewer
// simply ignores the rest, which is a property of that unit and not a hole
// here. The selector exists so this block can stay unit-agnostic: it serialises
// and it tags, and the core does the routing.
//
// ENFORCED-BY: tests/differential/field_v2_lanemux_directed.cpp:main
module zhao_field_v2_lanemux #(
    parameter int LANES = 4,
    parameter int WFS   = 8,
    parameter int REGS  = 64
) (
    input  logic clk,
    input  logic rst_n,

    // ---- vector request from the front end -------------------------------
    input  logic                      req_valid_i,
    output logic                      req_ready_o,
    input  logic [$clog2(WFS)-1:0]    req_wf_i,
    input  logic [$clog2(REGS)-1:0]   req_dst_i,
    input  logic [1:0]                req_mode_i,
    // WHICH UNIT, carried like the tag. This block stays unit-agnostic -- it
    // serialises and tags, it does not know what a curve or a length is -- so
    // the selector travels with the transaction and the core routes on it.
    input  logic [1:0]                req_unit_i,
    // The instruction's immediate, captured on the same terms as the tag and
    // for the same reason: a unit given it live reads whatever the front end is
    // doing when its turn comes, not what the instruction asked for.
    input  logic [31:0]               req_imm_i,
    // THE OPERAND BUNDLE. CURVE takes one value; the length family takes up to
    // five (a0,a1,a2 for LEN3, plus b0,b1 for DIST2), which is why this is a
    // bundle rather than a single operand. Named after zhao_field_len's own
    // ports so the wiring can be read against the unit it feeds.
    input  logic signed [31:0]        req_a_i  [LANES],
    input  logic signed [31:0]        req_a1_i [LANES],
    input  logic signed [31:0]        req_a2_i [LANES],
    input  logic signed [31:0]        req_b0_i [LANES],
    input  logic signed [31:0]        req_b1_i [LANES],

    // ---- scalar unit port (ready/valid, one value at a time) -------------
    output logic                      u_valid_o,
    input  logic                      u_ready_i,
    output logic [1:0]                u_mode_o,
    output logic [1:0]                u_unit_o,
    output logic [31:0]               u_imm_o,
    output logic signed [31:0]        u_a_o,
    output logic signed [31:0]        u_a1_o,
    output logic signed [31:0]        u_a2_o,
    output logic signed [31:0]        u_b0_o,
    output logic signed [31:0]        u_b1_o,
    input  logic                      u_rvalid_i,
    output logic                      u_rready_o,
    input  logic signed [31:0]        u_result_i,

    // ---- vector reply ----------------------------------------------------
    output logic                      rsp_valid_o,
    input  logic                      rsp_ready_i,
    output logic [$clog2(WFS)-1:0]    rsp_wf_o,
    output logic [$clog2(REGS)-1:0]   rsp_dst_o,
    output logic signed [31:0]        rsp_y_o [LANES]
);

  localparam int LW = $clog2(LANES);

  typedef enum logic [1:0] {S_IDLE, S_ISSUE, S_WAIT, S_DONE} state_t;
  state_t state;

  logic [LW-1:0]      lane;         // which lane is in flight
  logic signed [31:0] a_q   [LANES];
  logic signed [31:0] a1_q  [LANES];
  logic signed [31:0] a2_q  [LANES];
  logic signed [31:0] b0_q  [LANES];
  logic signed [31:0] b1_q  [LANES];
  logic signed [31:0] y_q   [LANES];

  // THE TAG, captured once at accept.
  logic [$clog2(WFS)-1:0]  wf_q;
  logic [$clog2(REGS)-1:0] dst_q;
  logic [1:0]              mode_q;
  logic [1:0]              unit_q;
  logic [31:0]             imm_q;

  assign req_ready_o = (state == S_IDLE);
  assign u_valid_o   = (state == S_ISSUE);
  assign u_mode_o    = mode_q;
  assign u_unit_o    = unit_q;
  assign u_imm_o     = imm_q;
  assign u_a_o       = a_q[lane];
  assign u_a1_o      = a1_q[lane];
  assign u_a2_o      = a2_q[lane];
  assign u_b0_o      = b0_q[lane];
  assign u_b1_o      = b1_q[lane];
  assign u_rready_o  = (state == S_WAIT);
  assign rsp_valid_o = (state == S_DONE);
  assign rsp_wf_o    = wf_q;
  assign rsp_dst_o   = dst_q;

  always_comb begin
    for (int l = 0; l < LANES; l++) rsp_y_o[l] = y_q[l];
  end

  integer l;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state  <= S_IDLE;
      lane   <= '0;
      wf_q   <= '0;
      dst_q  <= '0;
      mode_q <= 2'd0;
      unit_q <= 2'd0;
      imm_q  <= 32'd0;
      for (l = 0; l < LANES; l++) begin
        a_q[l]  <= '0;
        a1_q[l] <= '0;
        a2_q[l] <= '0;
        b0_q[l] <= '0;
        b1_q[l] <= '0;
        y_q[l]  <= '0;
      end
    end else begin
      unique case (state)
        S_IDLE: begin
          if (req_valid_i) begin
            // Tag and operands captured together, once.
            wf_q   <= req_wf_i;
            dst_q  <= req_dst_i;
            mode_q <= req_mode_i;
            unit_q <= req_unit_i;
            imm_q  <= req_imm_i;
            for (l = 0; l < LANES; l++) begin
              a_q[l]  <= req_a_i[l];
              a1_q[l] <= req_a1_i[l];
              a2_q[l] <= req_a2_i[l];
              b0_q[l] <= req_b0_i[l];
              b1_q[l] <= req_b1_i[l];
            end
            lane   <= '0;
            state  <= S_ISSUE;
          end
        end

        // Present lane `lane` to the scalar unit and wait for it to accept.
        S_ISSUE: if (u_ready_i) state <= S_WAIT;

        // Collect that lane's answer, then either advance or finish. The reply
        // is written to `y_q[lane]` -- the lane that ASKED -- and not to
        // whatever lane the counter would hold after an increment.
        S_WAIT: if (u_rvalid_i) begin
          y_q[lane] <= u_result_i;
          if (lane == LW'(LANES-1)) begin
            state <= S_DONE;
          end else begin
            lane  <= lane + LW'(1);
            state <= S_ISSUE;
          end
        end

        S_DONE: if (rsp_ready_i) state <= S_IDLE;

        default: state <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_field_v2_lanemux
