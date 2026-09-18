// zhao_light_div32_ii2.sv -- the SHARED light-term quotient, folded to II=2.
//
// PROVENANCE. The architecture and the first body of this file arrived in
// `reports/Zhaozhou_Lighting_Emergency_Rescue_2026-09-18.txt` (appendix,
// "UNVERIFIED RTL CANDIDATE"). The author stated plainly that it had never
// been through an HDL simulator, a linter or Quartus. This copy is the
// QUALIFIED one: `tests/geometry/light_div32_ii2_directed.cpp` drives it
// against the compiled renderer law and its own edge set. What follows is a
// list of what qualification actually established, because "we adopted the
// candidate" is not evidence.
//
// WHAT THIS BLOCK IS
// ------------------
// Exactly one rounded quotient of the ratified light law:
//
//     raw = sat_s32( floor( (dot + floor(mag/2)) / mag ) )
//
// with `mag == 0` producing zero and raising `degenerate_o`. The CALLER
// supplies the numerator already biased and split into sign + magnitude --
// `num_i = |dot + floor(mag/2)|`, `neg_i = (dot + floor(mag/2)) < 0`. It is
// NOT `|dot|` plus a bias applied here; the bias is added in the caller's
// full-width signed domain, because `|dot| + mag/2` and `|dot + mag/2|` are
// different numbers for a negative dot and the law wants the second.
//
// The domain is proved rather than assumed: for s32 normal lanes and s32
// light lanes, |dot| <= 3 * 2^62 and floor(mag/2) < 2^31, so the biased
// numerator is below 4 * 2^62 == 2^64 and fits `num_i` exactly. That is the
// sentence that makes a 64-bit numerator port sufficient.
//
// THE FOLD, AND WHY IT IS 16 CELLS AND NOT 32
// -------------------------------------------
// A restoring divide needs 32 recurrence steps for a 32-bit quotient. This
// block contains SIXTEEN physical step cells and runs each of them twice per
// request, on alternating phases of a one-bit counter:
//
//   phase 0: cell g consumes cell g-1's finished word (cell 0 takes a new
//            request) and performs one step;
//   phase 1: cell g feeds itself back and performs the second step.
//
// So each request receives 16 * 2 == 32 steps, and a new request can enter
// every TWO enabled clocks. Latency is long (33 clocks accept-to-consume,
// measured, not reasoned) and throughput is what the workload needs. Latency
// and initiation interval are separately measured in the bench and separately
// printed, because a block that overlaps nothing has them accidentally equal
// and the habit of quoting one for the other survives the moment it stops
// being true.
//
// THE SKIPPED HIGH HALF IS NOT DISCARDED NUMERATOR
// ------------------------------------------------
// The seed sets `rem = num[63:32]` and `word_q = num[31:0]`, then runs 32
// steps rather than 64. That is exact ONLY because the `big` test below
// guarantees `num[63:32] < den` before the recurrence starts, which is the
// standard invariant proving every quotient bit above bit 31 is zero.
// Seeding `rem` at zero instead -- the obvious-looking "just divide the low
// word" mistake -- is wrong, and the bench carries a NEGATIVE CONTROL for it
// (section 6) rather than trusting this paragraph.
//
// When `num[63:32] >= den` the true quotient is at least 2^32, so the signed
// 32-bit answer is on its sign's rail whatever the low bits do. That verdict
// is carried as `big` through the same ordered pipeline as the payload, and
// the recurrence runs on zeros so no cell needs a bypass.
//
// FINALIZATION
// ------------
//   h >= 0:  result = sat_s32(q)
//   h <  0:  result = sat_s32(-q - (r != 0))          -- floor, not truncate
// and `-q-1 == ~q`, `-q == ~q + 1`, so the negative arm is one adder rather
// than a negate-then-decrement chain. INT32_MIN is exactly representable and
// is NOT reported saturated; INT32_MIN-1 is. The bench pins both.
//
// STALLS
// ------
// `advance` gates every register in the block, including `phase_q`, so the
// whole local pipeline freezes as ONE unit when the consumer is not ready.
// That is safe HERE because every register in the block shares that enable.
// It is NOT a licence to freeze tags while a DSP or RAM output keeps moving:
// `zhao_light_stream.sv` therefore does not gate its multiplier lanes with
// this signal -- it uses an elastic request queue and a side-channel FIFO
// whose occupancy is asserted equal to this block's in-flight count.
//
// TAGS
// ----
// `tag_i` rides the request and comes back with its own result. The block is
// strictly in-order and holds at most STAGES*2 requests, which is what lets
// the caller use an ordinary FIFO for the wide per-term state instead of
// pushing 120 coefficient bits through every divider stage.
//
// SYNTHESIS
// ---------
// Quartus 17 form law is obeyed: the elaboration guard is inside
// `initial begin ... end` and the generate region carries explicit
// `generate` / `endgenerate`. THAT IS FORM, NOT PROOF. This block has not
// been through `quartus_map`; a clean `verilator --lint-only -Wall` settles
// one tool's opinion and says nothing about synthesizability, and
// `--lint-only` does not execute the `initial` block either -- the directed
// simulation does.
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
        if (TAGW < 1) $fatal(1,"zhao_light_div32_ii2: TAGW must be positive");
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
