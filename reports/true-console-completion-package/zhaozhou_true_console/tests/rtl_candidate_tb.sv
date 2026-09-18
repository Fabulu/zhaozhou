`timescale 1ns/1ps
module rtl_candidate_tb;
    reg clk=0; always #5 clk=~clk;
    reg rst_n=0;
    reg fv=0, fr=0;
    reg [31:0] fd=0;
    wire fir, fov;
    wire [31:0] fod;
    wire [2:0] level;
    zhao_cpl_ram_fifo #(.WIDTH(32),.RAM_WORDS(3),.ADDR_W(2)) fifo (
        .clk(clk),.rst_n(rst_n),.in_valid_i(fv),.in_ready_o(fir),.in_data_i(fd),
        .out_valid_o(fov),.out_ready_i(fr),.out_data_o(fod),.memory_words_o(level)
    );
    reg gv=0, gr=0;
    wire gir, gov;
    wire [3:0] verdict;
    reg [63:0] hps=0;
    reg [31:0] dst=0,len=0,sb=0,sz=0,db=0,dz=0;
    reg [15:0] re=0,ce=0;
    zhao_cpl_upload_guard guard (
        .clk(clk),.rst_n(rst_n),.req_valid_i(gv),.req_ready_o(gir),
        .hps_addr_i(hps),.vram_addr_i(dst),.length_i(len),
        .source_base_i(sb),.source_bytes_i(sz),.dest_base_i(db),.dest_bytes_i(dz),
        .request_epoch_i(re),.current_epoch_i(ce),
        .rsp_valid_o(gov),.rsp_ready_i(gr),.verdict_o(verdict)
    );
    reg [31:0] expected_fifo [0:19999];
    reg [31:0] rng=32'h19a382df;
    integer wr=0,rd=0,n,fdfile,got,expect_code,timeout_count,vector_count=0;
    initial begin
        repeat(3) @(negedge clk);
        rst_n=1;
        // Standard FIFO scoreboard; accepted transfers, not offered requests.
        for(n=0;n<10000;n=n+1) begin
            @(negedge clk);
            rng={rng[30:0],rng[31]^rng[21]^rng[1]^rng[0]};
            fv=rng[0]; fr=rng[3]; fd=n;
            @(posedge clk);
            if(fov && fr) begin
                if(rd>=wr || fod!==expected_fifo[rd])
                    $fatal(1,"FIFO order/data mismatch at cycle %0d",n);
                rd=rd+1;
            end
            if(fv && fir) begin expected_fifo[wr]=fd; wr=wr+1; end
        end
        @(negedge clk);fv=0;fr=1;
        timeout_count=0;
        while(rd<wr) begin
            @(posedge clk);
            if(fov && fr) begin
                if(fod!==expected_fifo[rd]) $fatal(1,"FIFO drain mismatch");
                rd=rd+1;
            end
            timeout_count=timeout_count+1;
            if(timeout_count>100) $fatal(1,"FIFO failed to drain");
        end
        @(negedge clk);fr=0;
        // Reset intentionally discards queued work; fresh data must not leak old RAM.
        rst_n=0;repeat(2)@(negedge clk);rst_n=1;
        @(negedge clk);fv=1;fd=32'hbada55aa;
        @(posedge clk);if(!fir)$fatal(1,"FIFO not ready after reset");
        @(negedge clk);fv=0;fr=1;
        timeout_count=0;
        while(!fov) begin @(negedge clk);timeout_count=timeout_count+1;
            if(timeout_count>10)$fatal(1,"fresh FIFO output missing");end
        if(fod!==32'hbada55aa)$fatal(1,"stale RAM data after reset");
        @(posedge clk);@(negedge clk);fr=0;

        fdfile=$fopen("upload_vectors.txt","r");
        if(fdfile==0)$fatal(1,"upload_vectors.txt unavailable");
        while(!$feof(fdfile)) begin
            got=$fscanf(fdfile,"%h %h %h %h %h %h %h %h %h %d\n",hps,dst,len,sb,sz,db,dz,re,ce,expect_code);
            if(got==10) begin
                while(!gir)@(negedge clk);
                gv=1;
                @(posedge clk);@(negedge clk);gv=0;
                timeout_count=0;
                while(!gov)begin
                    @(negedge clk);timeout_count=timeout_count+1;
                    if(timeout_count>20)$fatal(1,"upload guard timeout");
                end
                if(verdict!==expect_code[3:0])
                    $fatal(1,"guard vector %0d expected %0d got %0d",vector_count,expect_code,verdict);
                repeat(3) begin
                    @(negedge clk);
                    if(!gov || verdict!==expect_code[3:0])$fatal(1,"guard response unstable while stalled");
                end
                gr=1;@(posedge clk);@(negedge clk);gr=0;
                vector_count=vector_count+1;
            end else if(got!=-1) $fatal(1,"malformed vector line");
        end
        $fclose(fdfile);
        $display("PASS: FIFO %0d transferred records, reset, and %0d guard vectors",wr,vector_count);
        $finish;
    end
    initial begin #3000000;$fatal(1,"global watchdog");end
endmodule
