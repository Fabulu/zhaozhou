// Candidate request validator for the missing MEM.UPLOAD path.
// NOT the uploader. No transfer, CRC, residency publication, pinning, quarantine,
// cache invalidation or late-write cancellation is implemented by this leaf.
// Mirrors zref_mem_upload.hpp verdict precedence. Input regions must themselves
// be validated by the owning epoch configuration; wider endpoints never wrap.
// One request at a time, variable latency, held result under backpressure.
// Inline/share these request latches with the real uploader where appropriate;
// do not blindly keep two permanent copies of the complete upload descriptor.
// HDL simulation / Quartus synthesis / area / timing: NOT performed here.
`default_nettype none
module zhao_cpl_upload_guard (
    input wire clk,
    input wire rst_n,
    input wire req_valid_i,
    output wire req_ready_o,
    input wire [63:0] hps_addr_i,
    input wire [31:0] vram_addr_i,
    input wire [31:0] length_i,
    input wire [31:0] source_base_i,
    input wire [31:0] source_bytes_i,
    input wire [31:0] dest_base_i,
    input wire [31:0] dest_bytes_i,
    input wire [15:0] request_epoch_i,
    input wire [15:0] current_epoch_i,
    output wire rsp_valid_o,
    input wire rsp_ready_i,
    output wire [3:0] verdict_o
);
    localparam [3:0] IDLE=0, BASIC=1, SRC_LO=2, SRC_HI=3,
                     DST_LO=4, DST_HI=5, EPOCH=6, ANSWER=7;
    localparam [3:0] OK=0, UNALIGNED=1, ZERO_LENGTH=2,
                     OUTSIDE_GUARD=3, EPOCH_STALE=4,
                     SOURCE_OUTSIDE_ARENA=6, SOURCE_UNREACHABLE=7;
    reg [3:0] state, verdict;
    reg [63:0] hps_addr;
    reg [31:0] vram_addr, length_q, source_base, dest_base;
    reg [32:0] source_end, dest_end, requested_source_end, requested_dest_end;
    reg [15:0] request_epoch, epoch_at_accept;

    assign req_ready_o = rst_n && state == IDLE;
    assign rsp_valid_o = rst_n && state == ANSWER;
    assign verdict_o = verdict;

    // Payload has no reset value: it is read only after an accepted request.
    // Resetting only ownership/control is intentional, not a source of validity.
    always @(posedge clk) begin
        if (req_valid_i && req_ready_o) begin
            hps_addr <= hps_addr_i;
            vram_addr <= vram_addr_i;
            length_q <= length_i;
            source_base <= source_base_i;
            dest_base <= dest_base_i;
            source_end <= {1'b0,source_base_i} + {1'b0,source_bytes_i};
            dest_end <= {1'b0,dest_base_i} + {1'b0,dest_bytes_i};
            requested_source_end <= {1'b0,hps_addr_i[31:0]} + {1'b0,length_i};
            requested_dest_end <= {1'b0,vram_addr_i} + {1'b0,length_i};
            request_epoch <= request_epoch_i;
            epoch_at_accept <= current_epoch_i;
        end
    end
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state <= IDLE;
            verdict <= OK;
        end else begin
            case (state)
                IDLE: if (req_valid_i && req_ready_o) state <= BASIC;
                BASIC: begin
                    if (length_q == 0) begin verdict <= ZERO_LENGTH; state <= ANSWER; end
                    else if (|{hps_addr[5:0],vram_addr[5:0],length_q[5:0]})
                        begin verdict <= UNALIGNED; state <= ANSWER; end
                    else if (|hps_addr[63:32])
                        begin verdict <= SOURCE_UNREACHABLE; state <= ANSWER; end
                    else state <= SRC_LO;
                end
                SRC_LO: begin
                    if (hps_addr[31:0] < source_base)
                        begin verdict <= SOURCE_OUTSIDE_ARENA; state <= ANSWER; end
                    else state <= SRC_HI;
                end
                SRC_HI: begin
                    if (requested_source_end > source_end)
                        begin verdict <= SOURCE_OUTSIDE_ARENA; state <= ANSWER; end
                    else state <= DST_LO;
                end
                DST_LO: begin
                    if (vram_addr < dest_base)
                        begin verdict <= OUTSIDE_GUARD; state <= ANSWER; end
                    else state <= DST_HI;
                end
                DST_HI: begin
                    if (requested_dest_end > dest_end)
                        begin verdict <= OUTSIDE_GUARD; state <= ANSWER; end
                    else state <= EPOCH;
                end
                EPOCH: begin
                    verdict <= (request_epoch == epoch_at_accept) ? OK : EPOCH_STALE;
                    state <= ANSWER;
                end
                ANSWER: if (rsp_ready_i) state <= IDLE;
                default: begin state <= IDLE; verdict <= OK; end
            endcase
        end
    end
endmodule
`default_nettype wire
