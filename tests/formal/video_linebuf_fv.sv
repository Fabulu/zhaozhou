// video_linebuf_fv.sv — formal harness for zhao_scanout_linebuf (tests/
// formal/video_scanout_linebuf.sby). FORMAL COMPONENT — never synthesized,
// never linted by the RTL lanes.
//
// An abstract FETCH driver (fill protocol as assumptions) and DISPLAY
// driver (consume protocol) exercise the ping-pong handshake; a shadow
// memory written exactly on the accepted fill beats proves the NEVER-TORN
// law: a fresh buffer's read port returns exactly its last completed fill.

module video_linebuf_fv
  import zhao_pkg::*;
(
  input logic clk,
  input logic rst_n
);

  // ---- free (constrained) stimulus --------------------------------------
  logic        fill_we;
  reg          fill_buf_q = 1'b0;   // fetch-side ping-pong select
  reg          filling_q = 1'b0;    // a fill session is open
  logic [6:0]  fill_addr;
  logic [63:0] fill_data;
  logic        fill_line_done;
  logic [1:0]  fill_abort;
  logic [1:0]  consume_start;
  logic [1:0]  consume_done;
  logic        rd_buf;
  logic [6:0]  rd_addr;
  logic [1:0]  buf_fresh;
  logic [1:0]  buf_empty;
  logic [63:0] rd_word;

  zhao_scanout_linebuf dut (
    .gpu_clk(clk), .rst_n(rst_n),
    .fill_buf(fill_buf_q), .fill_addr(fill_addr), .fill_data(fill_data),
    .fill_we(fill_we), .fill_line_done(fill_line_done),
    .fill_abort(fill_abort), .buf_empty(buf_empty),
    .vid_clk(clk),
    .consume_start(consume_start), .consume_done(consume_done),
    .rd_buf(rd_buf), .rd_addr(rd_addr), .rd_word(rd_word),
    .buf_fresh(buf_fresh)
  );

  // ---- the fetch protocol (ASSUMED; zhao_scanout_fetch guarantees it) --
  // * a fill beat is only offered to a buffer that is EMPTY (first beat)
  //   or FILLING (the rest) — the module's own gate re-checks it
  // * line_done only after at least one fill beat of this line
  reg f_past_valid = 0;
  always @(posedge clk) begin
    f_past_valid <= 1;
    // reset discipline: the proof run starts in reset, released at most once
    if (!f_past_valid) assume(!rst_n);
    else assume(!$past(rst_n) || rst_n);   // release is monotonic
  end

  reg [1:0] beats_of_line = 2'd0;
  always @(posedge clk) begin   // harness trackers: synchronous reset
    if (!rst_n) begin
      beats_of_line <= 2'd0;
    end else begin
      if (fill_line_done) beats_of_line <= 2'd0;
      else if (fill_we && beats_of_line < 2'd3) beats_of_line <= beats_of_line + 2'd1;
    end
  end

  always @(posedge clk) begin
    if (rst_n) begin
      // never write a FULL buffer (fetch-side law)
      assume(!(fill_we && !buf_empty[fill_buf_q] && !filling_q));
      // line_done only for a line that actually filled beats this session
      assume(!(fill_line_done && beats_of_line == 2'd0));
      // abort and line_done never together
      assume(!(fill_abort != 2'b00 && fill_line_done));
    end
  end
  always @(posedge clk) begin   // harness tracker: synchronous reset
    if (!rst_n) filling_q <= 1'b0;
    else if (fill_line_done || (fill_abort[fill_buf_q] != 1'b0)) filling_q <= 1'b0;
    else if (fill_we && !filling_q) filling_q <= 1'b1;
  end

  // the display protocol (ASSUMED; zhao_scanout_serializer guarantees it):
  // consume_start only on a FRESH buffer, consume_done only after its start
  reg [1:0] started = 2'b00;
  always @(posedge clk) begin   // harness tracker: synchronous reset
    if (!rst_n) begin
      started <= 2'b00;
    end else begin
      if (consume_start[0] && buf_fresh[0]) started[0] <= 1'b1;
      else if (consume_done[0]) started[0] <= 1'b0;
      if (consume_start[1] && buf_fresh[1]) started[1] <= 1'b1;
      else if (consume_done[1]) started[1] <= 1'b0;
    end
  end
  always @(posedge clk) begin
    if (rst_n) begin
      assume(!(consume_start[0] && !buf_fresh[0]));
      assume(!(consume_start[1] && !buf_fresh[1]));
      assume(!(consume_done[0] && !started[0]));
      assume(!(consume_done[1] && !started[1]));
    end
  end

  // ---- NEVER-TORN: shadow memory written on accepted fill beats --------
  reg [63:0] shadow [0:1][0:127];
  integer i, j;
  initial begin
    for (i = 0; i < 2; i = i + 1)
      for (j = 0; j < 128; j = j + 1) shadow[i][j] = 64'd0;
  end
  always @(posedge clk) begin
    if (rst_n && fill_we && (buf_empty[fill_buf_q] || filling_q)) begin
      shadow[fill_buf_q][fill_addr] <= fill_data;
    end
  end

  // reading a FRESH buffer always returns the shadow content of its last
  // completed fill (no mid-refill word can ever surface: a fresh buffer is
  // never written — the serializer never overtakes the fill)
  always @(posedge clk) begin
    if (rst_n && buf_fresh[rd_buf]) begin
      assert(rd_word == shadow[rd_buf][rd_addr]);
    end
  end

endmodule : video_linebuf_fv
