// Not run here. Run with Verilator --binary --timing --assert -Wall;
// run executable from package root so tests/*.hex resolve.
`timescale 1ns/1ps
module tb_lightarith;
    localparam integer N=1024;
    logic clk=1'b0;
    logic rst_n=1'b0;
    always #5 clk=~clk;
    logic [130:0] dv[0:N-1];
    logic [95:0] rv[0:N-1];
    integer di=0, dout=0, ri=0, rout=0, cycle=0;
    wire dvalid=(di<N), rvalid=(ri<N);
    wire dready, rready;
    wire doutvalid, routvalid;
    logic doutready=1'b1, routready=1'b1;
    wire signed [31:0] dresult;
    wire dsat, dzero;
    wire [31:0] dtag, rtag, root;
    wire [130:0] din = (di<N) ? dv[di] : '0;
    wire [95:0] rin = (ri<N) ? rv[ri] : '0;
    zhao_light_div32_ii2 #(.TAGW(32)) d (
        .clk(clk),.rst_n(rst_n),.v_valid_i(dvalid),.v_ready_o(dready),
        .num_i(din[130:67]),.den_i(din[66:35]),.neg_i(din[34]),.tag_i(32'(di)),
        .r_valid_o(doutvalid),.r_ready_i(doutready),.result_o(dresult),
        .saturated_o(dsat),.degenerate_o(dzero),.tag_o(dtag));
    zhao_light_isqrt64_ii8 #(.TAGW(32)) r (
        .clk(clk),.rst_n(rst_n),.v_valid_i(rvalid),.v_ready_o(rready),
        .radicand_i(rin[95:32]),.tag_i(32'(ri)),.r_valid_o(routvalid),
        .r_ready_i(routready),.root_o(root),.tag_o(rtag));
    initial begin
        $readmemh("tests/div_vectors.hex",dv);
        $readmemh("tests/root_vectors.hex",rv);
        repeat(3) @(negedge clk);
        rst_n=1'b1;
    end
    always @(negedge clk) begin
        doutready=((cycle%11)!=0) && ((cycle%37)<30);
        routready=((cycle%13)!=0) && ((cycle%41)<35);
    end
    always @(posedge clk) begin
        if(rst_n) begin
            cycle=cycle+1;
            if(doutvalid && doutready) begin
                if(dout>=N) $fatal(1,"extra divider output");
                if(dtag!==32'(dout) || dresult!==dv[dout][33:2] ||
                   dsat!==dv[dout][1] || dzero!==dv[dout][0])
                    $fatal(1,"divider mismatch at %0d",dout);
                dout=dout+1;
            end
            if(routvalid && routready) begin
                if(rout>=N) $fatal(1,"extra root output");
                if(rtag!==32'(rout) || root!==rv[rout][31:0])
                    $fatal(1,"root mismatch at %0d",rout);
                rout=rout+1;
            end
            // Producer indices update AFTER DUT sampling in the NBA region.
            if(dvalid && dready) di<=di+1;
            if(rvalid && rready) ri<=ri+1;
            if(dout==N && rout==N) begin
                $display("PASS: 1024 divider + 1024 root vectors, with output stalls");
                $finish;
            end
            if(cycle>N*20) $fatal(1,"timeout: divider %0d root %0d",dout,rout);
        end
    end
endmodule
