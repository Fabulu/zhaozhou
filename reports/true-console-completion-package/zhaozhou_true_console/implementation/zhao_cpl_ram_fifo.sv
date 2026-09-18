// Candidate completion primitive, NOT installed in Fabulu/zhaozhou.
// One synchronous memory read port, one whole-word write port, no RAM reset.
// Capacity is RAM_WORDS + ONE output register, explicitly; not a drop-in
// replacement for a FIFO whose externally promised capacity is RAM_WORDS.
// A full memory refuses push even when a same-edge fetch frees a word. This
// avoids same-address read/write and a combinational output-ready -> input-ready
// path. Payload is uninterpreted; the caller includes tags/generations in WIDTH.
// Reset discards queued work. Use only where the owning transaction allows that.
// HDL simulation / Quartus inference / area / timing: NOT performed here.
`default_nettype none
module zhao_cpl_ram_fifo #(
    parameter integer WIDTH = 128,
    parameter integer RAM_WORDS = 16,
    parameter integer ADDR_W = 4
) (
    input wire clk,
    input wire rst_n,
    input wire in_valid_i,
    output wire in_ready_o,
    input wire [WIDTH-1:0] in_data_i,
    output wire out_valid_o,
    input wire out_ready_i,
    output wire [WIDTH-1:0] out_data_o,
    output wire [ADDR_W:0] memory_words_o
);
    (* ramstyle = "M10K" *) reg [WIDTH-1:0] memory [0:RAM_WORDS-1];
    reg [ADDR_W-1:0] wr_ptr, rd_ptr;
    reg [ADDR_W:0] memory_words;
    reg out_valid;
    reg [WIDTH-1:0] out_data;
    wire push, fetch, output_free;
    localparam [ADDR_W:0] LIMIT = RAM_WORDS;
    localparam [ADDR_W-1:0] LAST = RAM_WORDS-1;

    initial begin
        if (WIDTH < 1 || RAM_WORDS < 1 || ADDR_W < 1 ||
            RAM_WORDS > (2 ** ADDR_W))
            $fatal(1, "invalid FIFO geometry");
    end
    assign in_ready_o = rst_n && memory_words < LIMIT;
    assign push = in_valid_i && in_ready_o;
    assign output_free = !out_valid || out_ready_i;
    assign fetch = rst_n && output_free && memory_words != 0;
    assign out_valid_o = rst_n && out_valid;
    assign out_data_o = out_data;
    assign memory_words_o = memory_words;

    // Deliberately separate from reset logic. fetch/push are suppressed in reset.
    always @(posedge clk) begin
        if (push) memory[wr_ptr] <= in_data_i;
        if (fetch) out_data <= memory[rd_ptr];
    end

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            wr_ptr <= 0;
            rd_ptr <= 0;
            memory_words <= 0;
            out_valid <= 1'b0;
        end else begin
            if (push) wr_ptr <= (wr_ptr == LAST) ? 0 : wr_ptr + 1'b1;
            if (fetch) rd_ptr <= (rd_ptr == LAST) ? 0 : rd_ptr + 1'b1;
            case ({push,fetch})
                2'b10: memory_words <= memory_words + 1'b1;
                2'b01: memory_words <= memory_words - 1'b1;
                default: memory_words <= memory_words;
            endcase
            if (output_free) out_valid <= fetch;
        end
    end
`ifndef SYNTHESIS
    always @(posedge clk) begin
        if (rst_n && push && fetch && wr_ptr == rd_ptr)
            $fatal(1, "FIFO must not rely on same-address RAM collision semantics");
        if (rst_n && memory_words > LIMIT)
            $fatal(1, "FIFO occupancy overflow");
    end
`endif
endmodule
`default_nettype wire
