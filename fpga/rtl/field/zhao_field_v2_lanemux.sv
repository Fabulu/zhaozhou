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
    input  logic signed [31:0]        req_a_i [LANES],

    // ---- scalar unit port (ready/valid, one value at a time) -------------
    output logic                      u_valid_o,
    input  logic                      u_ready_i,
    output logic [1:0]                u_mode_o,
    output logic signed [31:0]        u_a_o,
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
  logic signed [31:0] y_q   [LANES];

  // THE TAG, captured once at accept.
  logic [$clog2(WFS)-1:0]  wf_q;
  logic [$clog2(REGS)-1:0] dst_q;
  logic [1:0]              mode_q;

  assign req_ready_o = (state == S_IDLE);
  assign u_valid_o   = (state == S_ISSUE);
  assign u_mode_o    = mode_q;
  assign u_a_o       = a_q[lane];
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
      for (l = 0; l < LANES; l++) begin
        a_q[l] <= '0;
        y_q[l] <= '0;
      end
    end else begin
      unique case (state)
        S_IDLE: begin
          if (req_valid_i) begin
            // Tag and operands captured together, once.
            wf_q   <= req_wf_i;
            dst_q  <= req_dst_i;
            mode_q <= req_mode_i;
            for (l = 0; l < LANES; l++) a_q[l] <= req_a_i[l];
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
