// zhao_light_isqrt64_ii8.sv -- exact floor(sqrt(u64)), folded to II=8.
//
// PROVENANCE. Architecture and first body from the appendix of
// `reports/Zhaozhou_Lighting_Emergency_Rescue_2026-09-18.txt`, delivered
// explicitly UNSIMULATED. `tests/geometry/light_isqrt64_ii8_directed.cpp`
// qualifies this copy against the compiled `zref::isqrt_u64`, which is the
// same function the renderer law calls, so the two sides of that comparison
// are not two readings of one prose paragraph.
//
// WHY A ROOT AT ALL, AND WHY II8 SPECIFICALLY
// -------------------------------------------
// The law divides by |n|, and |n| is the exact integer floor square root of
// the exact u64 sum of three s32 squares. It is computed ONCE PER NORMAL,
// never once per light term -- that is the invariant-reuse lever, and it is
// necessary but nowhere near sufficient on its own (the emergency report's
// section 0.1 does that arithmetic: the redundant roots overlapped the old
// engine's dot walk and cost zero additional clocks).
//
// The rate follows from the workload rather than from taste. The stress
// fixture is 120,000 normals inside a 960,000-clock term schedule, so the
// root must retire one job per 8 clocks or it, not the divider, becomes the
// bottleneck. A single NONPIPELINED 32-cycle root would need 3,840,000
// clocks for the same normals and misses the frame on its own. Caching a
// magnitude does not help if nothing can produce magnitudes at rate.
//
// FOUR CELLS, EIGHT PHASES
// ------------------------
// One base-four restoring digit step per clock per cell:
//     t     = (rem << 2) | next_two_radicand_bits
//     trial = (root << 2) | 1
//     take  = t >= trial
//     rem   = take ? t - trial : t
//     root  = (root << 1) | take
// Four physical cells, each reused for eight consecutive phases, gives
// 4 * 8 == 32 digit steps and a new job every eight enabled clocks. 32 steps
// on a 64-bit radicand is exactly floor(sqrt), with no rounding anywhere.
//
// THE REMAINDER WIDTH, WHICH IS THE ONE PLACE THIS CAN BE QUIETLY WRONG
// --------------------------------------------------------------------
// After k steps the partial root R_k < 2^k and the invariant remainder
// r_k <= 2*R_k. So after step 31, r <= 2^32 - 2 and still fits the 32 bits
// that `shifted` reads. After step 32 it can reach 2^33 - 2 and needs 34
// bits -- but it is never stepped again, only reported. That is why `rem` is
// 34 bits wide while `shifted` indexes `rem[31:0]`, and why narrowing either
// one is not the harmless tidy-up it looks like. The bench drives
// UINT64_MAX and the perfect squares either side of every power of two.
//
// STALLS
// ------
// As in the divider: `advance` gates every register including the phase
// counter, so the local pipeline freezes as one unit. Payload is not reset;
// validity owns lifetime.
//
// SYNTHESIS
// ---------
// Quartus 17 form law obeyed (elaboration guard inside `initial begin`,
// explicit `generate`/`endgenerate`). Form is not proof: this block has not
// been through `quartus_map`, and lint-clean is one tool's opinion about
// syntax, not evidence of synthesizability.
`default_nettype none
module zhao_light_isqrt64_ii8 #(
    parameter integer TAGW = 16
) (
    input wire clk,
    input wire rst_n,
    input wire v_valid_i,
    output wire v_ready_o,
    input wire [63:0] radicand_i,
    input wire [TAGW-1:0] tag_i,
    output logic r_valid_o,
    input wire r_ready_i,
    output logic [31:0] root_o,
    output logic [TAGW-1:0] tag_o
);
    localparam integer STAGES=4;
    typedef struct packed {
        logic [63:0] rad;
        logic [33:0] rem;
        logic [31:0] root;
        logic [TAGW-1:0] tag;
    } state_t;
    state_t data_q[0:STAGES-1];
    state_t data_d[0:STAGES-1];
    state_t seed_c;
    logic [STAGES-1:0] valid_q;
    wire [STAGES-1:0] valid_d;
    logic [2:0] phase_q;
    wire advance = !r_valid_o || r_ready_i;
    assign v_ready_o = advance && (phase_q == 3'd0);
    initial begin
        if (TAGW<1) $fatal(1,"zhao_light_isqrt64_ii8: TAGW must be positive");
    end
    always_comb begin
        seed_c.rad = radicand_i;
        seed_c.rem = 34'd0;
        seed_c.root = 32'd0;
        seed_c.tag = tag_i;
    end
    function automatic state_t step(input state_t a);
        state_t b;
        logic [33:0] shifted;
        logic [33:0] trial;
        logic [34:0] difference;
        logic take;
        begin
            b = a;
            // Before any of the 32 steps rem fits in 32 bits; the final
            // remainder may need 34 bits but is never stepped again.
            shifted = {a.rem[31:0],a.rad[63:62]};
            trial = {a.root,2'b01};
            difference = {1'b0,shifted} - {1'b0,trial};
            take = !difference[34];
            b.rem = take ? difference[33:0] : shifted;
            b.rad = {a.rad[61:0],2'b00};
            b.root = {a.root[30:0],take};
            step = b;
        end
    endfunction
    genvar g;
    generate
        for(g=0;g<STAGES;g=g+1) begin: gen_step
            state_t src_c;
            if(g==0) begin: gen_first
                always_comb src_c = (phase_q==3'd0) ? seed_c : data_q[g];
                assign valid_d[g] = (phase_q==3'd0) ? v_valid_i : valid_q[g];
            end else begin: gen_later
                always_comb src_c = (phase_q==3'd0) ? data_q[g-1] : data_q[g];
                assign valid_d[g] = (phase_q==3'd0) ? valid_q[g-1] : valid_q[g];
            end
            always_comb data_d[g] = step(src_c);
            always_ff @(posedge clk) begin
                if(advance) data_q[g] <= data_d[g];
            end
        end
    endgenerate
    always_ff @(posedge clk or negedge rst_n) begin
        if(!rst_n) begin
            phase_q <= 3'd0;
            valid_q <= '0;
            r_valid_o <= 1'b0;
            root_o <= 32'd0;
            tag_o <= '0;
        end else if(advance) begin
            phase_q <= phase_q + 3'd1;
            valid_q <= valid_d;
            r_valid_o <= (phase_q==3'd0) && valid_q[STAGES-1];
            if(phase_q==3'd0 && valid_q[STAGES-1]) begin
                root_o <= data_q[STAGES-1].root;
                tag_o <= data_q[STAGES-1].tag;
            end
        end
    end
endmodule
`default_nettype wire
