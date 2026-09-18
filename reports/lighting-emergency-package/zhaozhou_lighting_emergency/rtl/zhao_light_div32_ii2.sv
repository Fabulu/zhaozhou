// Candidate only: NOT HDL-simulated, NOT Quartus mapped/fitted.
// Exact sat_s32(floor((neg ? -num : num)/den)); den==0 produces zero/degenerate.
// Caller supplies |dot + floor(den/2)| and its sign, NOT |dot| plus a bias.
// 16 physical restoring steps reused in two phases: II=2 enabled clocks.
// 32 arithmetic clocks + registered finalization. Pipeline stalls as ONE unit
// on output backpressure; input is ordinary ready/valid and has no fixed-rate
// requirement. This global stall must not be used to stall an unrelated DSP
// pipeline without elastic buffers and correctly delayed tags.
`default_nettype none
module zhao_light_div32_ii2 #(
    parameter integer TAGW = 16
) (
    input wire clk,
    input wire rst_n,
    input wire v_valid_i,
    output wire v_ready_o,
    input wire [63:0] num_i,
    input wire [31:0] den_i,
    input wire neg_i,
    input wire [TAGW-1:0] tag_i,
    output logic r_valid_o,
    input wire r_ready_i,
    output logic signed [31:0] result_o,
    output logic saturated_o,
    output logic degenerate_o,
    output logic [TAGW-1:0] tag_o
);
    localparam integer STAGES = 16;
    typedef struct packed {
        logic [32:0] rem;
        logic [31:0] word_q;
        logic [31:0] den;
        logic neg;
        logic big;
        logic zero;
        logic [TAGW-1:0] tag;
    } state_t;
    state_t data_q [0:STAGES-1];
    state_t data_d [0:STAGES-1];
    state_t seed_c;
    logic [STAGES-1:0] valid_q;
    wire [STAGES-1:0] valid_d;
    logic phase_q;
    wire advance = !r_valid_o || r_ready_i;
    assign v_ready_o = advance && !phase_q;

    initial begin
        if (TAGW < 1) $fatal(1,"TAGW must be positive");
    end

    always_comb begin
        seed_c.zero = (den_i == 32'd0);
        seed_c.big = !seed_c.zero && (num_i[63:32] >= den_i);
        seed_c.den = seed_c.zero ? 32'd1 : den_i;
        seed_c.rem = (seed_c.zero || seed_c.big) ? 33'd0 : {1'b0,num_i[63:32]};
        seed_c.word_q = (seed_c.zero || seed_c.big) ? 32'd0 : num_i[31:0];
        seed_c.neg = neg_i;
        seed_c.tag = tag_i;
    end

    // Explicit widened unsigned subtraction: the BORROW is bit 33.
    function automatic state_t step(input state_t a);
        state_t b;
        logic [32:0] shifted;
        logic [33:0] difference;
        logic take;
        begin
            b = a;
            shifted = {a.rem[31:0],a.word_q[31]};
            difference = {1'b0,shifted} - {2'b0,a.den};
            take = !difference[33];
            b.rem = take ? difference[32:0] : shifted;
            b.word_q = {a.word_q[30:0],take};
            step = b;
        end
    endfunction

    genvar g;
    generate
        for (g=0;g<STAGES;g=g+1) begin : gen_step
            state_t src_c;
            if (g==0) begin : gen_first
                always_comb src_c = phase_q ? data_q[g] : seed_c;
                assign valid_d[g] = phase_q ? valid_q[g] : v_valid_i;
            end else begin : gen_later
                always_comb src_c = phase_q ? data_q[g] : data_q[g-1];
                assign valid_d[g] = phase_q ? valid_q[g] : valid_q[g-1];
            end
            always_comb data_d[g] = step(src_c);
            // Payload does not require reset; validity owns its lifetime.
            always_ff @(posedge clk) begin
                if (advance) data_q[g] <= data_d[g];
            end
        end
    endgenerate

    logic signed [31:0] finish_c;
    logic sat_c;
    logic rem_nz_c;
    always_comb begin
        rem_nz_c = |data_q[STAGES-1].rem;
        finish_c = 32'sd0;
        sat_c = 1'b0;
        if (!data_q[STAGES-1].zero) begin
            if (data_q[STAGES-1].big) begin
                sat_c = 1'b1;
                finish_c = data_q[STAGES-1].neg ? 32'sh80000000 : 32'sh7fffffff;
            end else if (!data_q[STAGES-1].neg) begin
                sat_c = data_q[STAGES-1].word_q[31];
                finish_c = sat_c ? 32'sh7fffffff : $signed(data_q[STAGES-1].word_q);
            end else begin
                sat_c = (data_q[STAGES-1].word_q > 32'h80000000) ||
                        ((data_q[STAGES-1].word_q == 32'h80000000) && rem_nz_c);
                // floor(-N/D) = -q-(r!=0). For nonzero r this is simply ~q.
                finish_c = sat_c ? 32'sh80000000 :
                           (rem_nz_c ? $signed(~data_q[STAGES-1].word_q) :
                            $signed(~data_q[STAGES-1].word_q + 32'd1));
            end
        end
    end

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            phase_q <= 1'b0;
            valid_q <= '0;
            r_valid_o <= 1'b0;
            result_o <= 32'sd0;
            saturated_o <= 1'b0;
            degenerate_o <= 1'b0;
            tag_o <= '0;
        end else if (advance) begin
            phase_q <= !phase_q;
            valid_q <= valid_d;
            r_valid_o <= !phase_q && valid_q[STAGES-1];
            if (!phase_q && valid_q[STAGES-1]) begin
                result_o <= finish_c;
                saturated_o <= sat_c;
                degenerate_o <= data_q[STAGES-1].zero;
                tag_o <= data_q[STAGES-1].tag;
            end
        end
    end
endmodule
`default_nettype wire
