// Candidate only: NOT HDL-simulated, NOT Quartus mapped/fitted.
// Four physical base-four restoring root steps; each reused for eight clocks.
// Exact floor(sqrt(u64)); II=8 enabled clocks, 32 recurrence clocks.
// Ready/valid global pipeline stall. No reset on payload, only validity.
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
        if (TAGW<1) $fatal(1,"TAGW must be positive");
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
            // remainder may need 33 bits but is never stepped again.
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
