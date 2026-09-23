// zhao_terrain_normalloader.sv -- TERRAIN.NORMALMAP's HOST: the block that
// fills the detail pyramid from a published DETAIL_NORMAL page.
//
// ENFORCED-BY: tests/terrain/normalloader_directed.cpp:main
// REFERENCE:   zref::normal_page (reference/include/zref/zref_normal_page.hpp)
// SPEC:        spec/cartridge.md 4g (kind 16, section 0x0014)
//
// ---------------------------------------------------------------------------
// WHAT WAS MISSING, AND WHAT R243 D-NORMALS-A RULED
// ---------------------------------------------------------------------------
// `zhao_terrain_normalmap` has held a seven-level signed detail pyramid
// (`tile_m [0:PYR_WORDS-1]`) and a write-only upload port since it was rebuilt
// to contract on 2026-09-09, and it is instantiated NOWHERE. The reason is not
// the block: NOTHING IN THE TREE COULD PUT BYTES IN THAT PYRAMID.
// `zhao_console_core.sv` records it in those terms -- "its tile upload port
// wants a generated asset ... the `tw_*` upload port is waiting on an asset
// pipeline rather than on a block".
//
// Owner ruling R243 D-NORMALS-A (2026-09-23) -- "In v1 -- commission the
// pyramid" -- answers that with DATA rather than with a new opcode. The detail
// tile travels as a DETAIL_NORMAL page, published by the `PublishResource`
// (R17) this console already executes, and this block carries its words to
// `tw_we_i` / `tw_addr_i` / `tw_data_i`.
//
// NOTHING IN THIS BLOCK INTERPRETS A TEXEL. It carries a flat 16-bit word to a
// flat word address and forms no view about which pyramid level that address
// belongs to -- the level structure is
// `zref::terrain::normalmap_pyramid_addr`'s law and the consumer's `LEVELS`
// parameter, and a second opinion here would be a second place for it to drift.
//
// ---------------------------------------------------------------------------
// MODELLED ON zhao_part_table_loader, WITH ONE DELIBERATE DEPARTURE
// ---------------------------------------------------------------------------
// The publication trigger, the whole-line read, the refuse-whole discipline and
// the drop-while-busy counter are all that block's, for the reasons its header
// gives. Two things differ, and both make this block SIMPLER:
//
//   * THE PAYLOAD IS FLAT. A species entry is {sel, index, event, data} and
//     needs four slices; a detail texel is one 16-bit word at one address.
//   * THE DESTINATION NEVER REFUSES. `tw_we_i` is an unconditional write port
//     with no `ready`, so there is no handshake to stall on and a line's
//     thirty-two words leave on thirty-two consecutive cycles.
//
// AND ONE DEPARTURE THAT IS A CORRECTION, NOT A SIMPLIFICATION -- see below.
//
// ---------------------------------------------------------------------------
// THE GUARD'S VERDICT IS READ IN A SEPARATE STATE, AND THAT IS THE POINT
// ---------------------------------------------------------------------------
// `zhao_mem_guard` drives `rsp.ready = !fwd_active` as a LEVEL and pulses
// `rsp.ok` the cycle AFTER the accept (zhao_mem_guard.sv:624-626). They are
// never high together on a passing request, so an arm that waits for both
// reads EVERY PASS AS A DENIAL -- silently, with the denial counter at zero,
// because the guard only raises `violation` when it actually refuses. That
// defect was found in both geometry fetchers on 2026-09-06 and
// `tools/rtl/check_guard_verdict.py` exists because of it.
//
// So this block uses the TWO-STATE shape that gate requires: `ready` moves the
// machine to a verdict state (S_HVERD / S_WVERD), and `ok` / `violation` are
// read THERE, one cycle later. `zhao_terrain_devstore` and
// `zhao_raster_fbwrite` are the committed precedents.
//
// THIS IS WHERE IT DEPARTS FROM `zhao_part_table_loader`, ON PURPOSE.
// That block tests `ready` and `violation` in a single cycle, and it is one of
// the seven files `check_guard_verdict.py`'s own CLIENTS comment names as NOT
// in the list and therefore never scanned. Copying a block whose protocol
// nothing checks would have inherited an unexamined shape; this file is in the
// CLIENTS list from the commit that creates it, which is the only way that
// audit stays exact.
//
// ---------------------------------------------------------------------------
// THE READ IS WHOLE 64-BYTE LINES, WHICH IS WHY THE LAYOUT IS SHAPED THAT WAY
// ---------------------------------------------------------------------------
// MEM.GUARD's read is at most 64 bytes and its shape rule requires the byte
// mask to match the length, so a reader that asks for a whole line is the
// simplest one that can be correct. A word is 2 bytes, so a line carries
// exactly THIRTY-TWO words and no word ever straddles a read.
//
// A DENIED READ ABANDONS THE PAGE. `zhao_mem_guard` drops a denied request and
// nothing was read, so continuing would fill the pyramid out of whatever the
// beat bus held. `denied_o` counts it and the pyramid keeps what it had.
//
// ---------------------------------------------------------------------------
// A REFUSED PAGE IS REFUSED WHOLE
// ---------------------------------------------------------------------------
// The magic, the version, the word count against the layout's ceiling and the
// word count against the declared extent are all judged from the HEADER LINE,
// before a single word is written. A page that fails one of them writes
// NOTHING: a half-loaded pyramid is terrain whose relief changes at a mip
// boundary for no authored reason, which reads as an aliasing bug and is not
// one.
//
// Conservative SystemVerilog subset only (charter 2).
`default_nettype none

module zhao_terrain_normalloader
  import zhao_pkg::*;
#(
    // spec/cartridge.md 3's page-kind registry; DETAIL_NORMAL is 16
    // (R243 D-NORMALS-A).
    parameter logic [7:0] PAGE_KIND = 8'd16,
    // The LAYOUT's ceiling, `zref::normal_page::kMaxWords` -- the full seven
    // level pyramid. It is NOT the consumer's `PYR_WORDS`: a page built for a
    // deeper pyramid than the silicon carries is truncated BY THE SILICON,
    // which ignores writes at or beyond its own depth ("an upload-tool fault,
    // not a machine state"), rather than refused here. This block refuses only
    // what the LAYOUT cannot express.
    parameter int unsigned LAYOUT_WORDS = 5461,
    // The bridge tag this block's reads carry. ENGINE1 is the asset window's
    // client, the same one PART.TABLE's page loader uses.
    parameter zhao_client_e CLIENT = ZHAO_CLIENT_ENGINE1
) (
    input var logic clk,
    input var logic rst_n,

    // ---- MEM.UPLOAD's publication ------------------------------------------
    input  var logic        pub_valid_i,
    input  var logic [ 7:0] pub_tag_i,
    input  var logic [31:0] pub_base_i,
    input  var logic [31:0] pub_extent_i,

    // ---- the asset read window, through the shared requester ---------------
    output var zhao_guard_req_t g_req_o,
    input  var zhao_guard_rsp_t g_rsp_i,
    input  var logic            g_beat_valid_i,
    input  var logic [63:0]     g_beat_data_i,

    // ---- TERRAIN.NORMALMAP's tile upload port ------------------------------
    // Write-only and unconditional: the destination has no `ready`.
    output var logic        tw_we_o,
    output var logic [12:0] tw_addr_o,
    output var logic [15:0] tw_data_o,   // {s8 dz, s8 dx}

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] pages_o,          // pages loaded whole
    output var logic [31:0] words_o,          // texels written
    output var logic [31:0] pages_dropped_o,  // a publication while busy
    output var logic [31:0] bad_magic_o,      // magic or version wrong
    output var logic [31:0] oversize_o,       // `words` above the layout ceiling
    output var logic [31:0] truncated_o,      // words run past the extent
    output var logic [31:0] denied_o,         // MEM.GUARD refused a read
    output var logic        busy_o
);

  localparam int unsigned LINE_BYTES     = 64;
  localparam int unsigned WORD_BYTES     = 2;
  localparam int unsigned WORDS_PER_LINE = LINE_BYTES / WORD_BYTES;   // 32
  localparam logic [31:0] MAGIC          = 32'h4D4E_445A;   // 'ZDNM' little-endian
  localparam logic [15:0] VERSION        = 16'd1;

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`;
  // a bare module-scope `if` is a syntax error there, and `--lint-only` does
  // NOT run this block -- the directed test elaborates the module, which does.
  initial begin
    if (LAYOUT_WORDS < 1 || LAYOUT_WORDS > 8191)
      $fatal(1, "zhao_terrain_normalloader: LAYOUT_WORDS=%0d outside the 13-bit address",
             LAYOUT_WORDS);
  end

  // ---- the line buffer ----------------------------------------------------
  // One 64-byte line, assembled LSB-first from eight beats: beat k is bytes
  // 8k..8k+7, which is the wire order MEM.GUARD delivers and the order the
  // page is written in.
  logic [511:0] line_q;
  logic [  2:0] beat_q;

  // ---- the walk -----------------------------------------------------------
  // Nine states. The two REQ states each have their own VERDICT state, which
  // is the whole subject of this file's guard-protocol note above.
  localparam logic [3:0] S_IDLE   = 4'd0;
  localparam logic [3:0] S_HREQ   = 4'd1;   // ask for the header line
  localparam logic [3:0] S_HVERD  = 4'd2;   // read ok/violation, ONE cycle later
  localparam logic [3:0] S_HBEATS = 4'd3;   // take its eight beats
  localparam logic [3:0] S_HDR    = 4'd4;   // judge the header, ONCE
  localparam logic [3:0] S_WREQ   = 4'd5;   // ask for a word line
  localparam logic [3:0] S_WVERD  = 4'd6;
  localparam logic [3:0] S_WBEATS = 4'd7;
  localparam logic [3:0] S_EMIT   = 4'd8;   // hand over its thirty-two words

  logic [ 3:0] st_q;
  logic [31:0] base_q;
  logic [31:0] extent_q;
  logic [15:0] count_q;      // words the header declares
  logic [15:0] done_q;       // words written so far -- ALSO the flat address
  logic [31:0] addr_q;       // the line being read
  logic [ 4:0] sub_q;        // which of the line's thirty-two words is next

  // ---- the header fields, read off the line -------------------------------
  wire [31:0] hdr_magic_c   = line_q[31:0];
  wire [15:0] hdr_version_c = line_q[47:32];
  wire [15:0] hdr_words_c   = line_q[63:48];
  // `words` must fit inside what the publication declared.
  wire [47:0] need_c = 48'(LINE_BYTES) + (48'(hdr_words_c) * 48'(WORD_BYTES));
  wire hdr_ok_c    = (hdr_magic_c == MAGIC) && (hdr_version_c == VERSION);
  wire hdr_size_c  = (hdr_words_c <= 16'(LAYOUT_WORDS));
  wire hdr_fits_c  = (need_c <= 48'(extent_q));

  // ---- the upload port ----------------------------------------------------
  // The word is taken from the line by its index alone. There are no field
  // slices because there are no fields: the page's word IS the consumer's
  // `tw_data_i`, and this block never looks inside it.
  assign tw_we_o   = (st_q == S_EMIT);
  assign tw_addr_o = done_q[12:0];
  assign tw_data_o = line_q[{sub_q, 4'd0} +: 16];   // sub_q * 16

  assign busy_o = (st_q != S_IDLE);

  // ---- the read request ---------------------------------------------------
  wire req_c = (st_q == S_HREQ) || (st_q == S_WREQ);
  always_comb begin
    g_req_o        = '0;
    g_req_o.valid  = req_c;
    g_req_o.write  = 1'b0;
    g_req_o.client = CLIENT;
    g_req_o.addr   = addr_q[ZHAO_VRAM_ADDR_BITS-1:0];
    g_req_o.len    = 7'(LINE_BYTES);
    // The shape rule: the mask must match the length. A whole line is all
    // sixty-four lanes.
    g_req_o.be     = {64{1'b1}};
  end

  // A publication is taken only between loads; while busy it is a DROP, and
  // the drop is the number the host needs -- two detail pyramids in flight is
  // two authors' relief interleaved, which reads as a tuning problem.
  wire pub_match_c = pub_valid_i && (pub_tag_i == PAGE_KIND);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q     <= S_IDLE;
      line_q   <= '0;
      beat_q   <= 3'd0;
      base_q   <= 32'd0;
      extent_q <= 32'd0;
      count_q  <= 16'd0;
      done_q   <= 16'd0;
      addr_q   <= 32'd0;
      sub_q    <= 5'd0;
      pages_o         <= 32'd0;
      words_o         <= 32'd0;
      pages_dropped_o <= 32'd0;
      bad_magic_o     <= 32'd0;
      oversize_o      <= 32'd0;
      truncated_o     <= 32'd0;
      denied_o        <= 32'd0;
    end else begin
      if (pub_match_c && (st_q != S_IDLE)) pages_dropped_o <= pages_dropped_o + 32'd1;

      case (st_q)
        S_IDLE: begin
          if (pub_match_c) begin
            base_q   <= pub_base_i;
            extent_q <= pub_extent_i;
            addr_q   <= pub_base_i;
            done_q   <= 16'd0;
            beat_q   <= 3'd0;
            sub_q    <= 5'd0;
            // A page too short to hold a header is truncated by definition,
            // and is refused before a single byte is asked for.
            if (pub_extent_i < 32'(LINE_BYTES)) begin
              truncated_o <= truncated_o + 32'd1;
            end else begin
              st_q <= S_HREQ;
            end
          end
        end

        // The guard answers in TWO cycles: `ready` is a level, `ok` is pulsed
        // the cycle after the accept. Testing them in one arm reads every pass
        // as a denial.
        S_HREQ:  if (g_rsp_i.ready) st_q <= S_HVERD;

        S_HVERD: begin
          if (g_rsp_i.violation) begin
            denied_o <= denied_o + 32'd1;
            st_q     <= S_IDLE;
          end else if (g_rsp_i.ok) begin
            beat_q <= 3'd0;
            st_q   <= S_HBEATS;
          end
        end

        S_HBEATS: begin
          if (g_beat_valid_i) begin
            line_q <= {g_beat_data_i, line_q[511:64]};   // LSB-first assembly
            beat_q <= beat_q + 3'd1;
            // The eighth beat is written on THIS edge, so the header is not
            // readable until the next one. S_HDR exists for that one cycle.
            if (beat_q == 3'd7) st_q <= S_HDR;
          end
        end

        S_HDR: begin
          count_q <= hdr_words_c;
          if (!hdr_ok_c) begin
            bad_magic_o <= bad_magic_o + 32'd1;
            st_q        <= S_IDLE;
          end else if (!hdr_size_c) begin
            // More words than the LAYOUT can express. Refused here, because a
            // page like this cannot be a DETAIL_NORMAL page at all -- unlike a
            // page merely deeper than this silicon, which the consumer
            // truncates by its own rule.
            oversize_o <= oversize_o + 32'd1;
            st_q       <= S_IDLE;
          end else if (!hdr_fits_c) begin
            truncated_o <= truncated_o + 32'd1;
            st_q        <= S_IDLE;
          end else if (hdr_words_c == 16'd0) begin
            // A legal, EMPTY page. It is a load: the host said "no detail",
            // and saying so is different from saying nothing.
            pages_o <= pages_o + 32'd1;
            st_q    <= S_IDLE;
          end else begin
            addr_q <= base_q + 32'(LINE_BYTES);
            beat_q <= 3'd0;
            sub_q  <= 5'd0;
            st_q   <= S_WREQ;
          end
        end

        S_WREQ:  if (g_rsp_i.ready) st_q <= S_WVERD;

        S_WVERD: begin
          if (g_rsp_i.violation) begin
            denied_o <= denied_o + 32'd1;
            st_q     <= S_IDLE;
          end else if (g_rsp_i.ok) begin
            beat_q <= 3'd0;
            st_q   <= S_WBEATS;
          end
        end

        S_WBEATS: begin
          if (g_beat_valid_i) begin
            line_q <= {g_beat_data_i, line_q[511:64]};
            beat_q <= beat_q + 3'd1;
            if (beat_q == 3'd7) begin
              sub_q <= 5'd0;
              st_q  <= S_EMIT;
            end
          end
        end

        // One word per cycle, unconditionally: the destination has no `ready`.
        // `done_q` is both the count and the flat address, which is why the
        // address needs no separate register -- the page's word order IS
        // `normalmap_pyramid_addr` order and the walk never skips.
        S_EMIT: begin
          done_q  <= done_q + 16'd1;
          words_o <= words_o + 32'd1;
          if (done_q + 16'd1 >= count_q) begin
            pages_o <= pages_o + 32'd1;
            st_q    <= S_IDLE;
          end else if (sub_q == 5'(WORDS_PER_LINE - 1)) begin
            addr_q <= addr_q + 32'(LINE_BYTES);
            beat_q <= 3'd0;
            sub_q  <= 5'd0;
            st_q   <= S_WREQ;
          end else begin
            sub_q <= sub_q + 5'd1;
          end
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_terrain_normalloader

`default_nettype wire
