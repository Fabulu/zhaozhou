// zhao_part_table_loader.sv -- PART.TABLE's HOST: the block that fills the
// species descriptor table from a published SPECIES_TABLE page. Core entry
// I33's absent owner.
//
// ENFORCED-BY: tests/particles/part_table_loader_directed.cpp:main
// REFERENCE:   zref::species_page (reference/include/zref/zref_species_page.hpp)
//
// ---------------------------------------------------------------------------
// WHAT WAS MISSING, AND WHAT R42 RULED
// ---------------------------------------------------------------------------
// Entry I33's whole argument was one sentence: "NO RATIFIED COMMAND CARRIES A
// SPECIES DESCRIPTOR ... Inventing one here would mean this file choosing what
// a species IS, which is owner DATA". Owner ruling R42 (2026-09-19) answers it
// without anyone choosing that: "A SPECIES_TABLE page kind, published through
// PublishResource/MEM.UPLOAD (R17/R32), plus a loader into PART.TABLE. The
// byte layout is frozen by the packet with a zref model."
//
// So the descriptors travel as DATA, in a page the owner authors, published by
// the command that already publishes every other resource. NOTHING IN THIS
// BLOCK INTERPRETS A DESCRIPTOR. It carries `zhao_part_table`'s own load word
// -- {sel, index, event, data} -- from the page to the port, and the only
// fields it reads are the ones that say WHERE a word goes.
//
// ---------------------------------------------------------------------------
// THE TRIGGER IS THE PUBLICATION, NOT A COMMAND
// ---------------------------------------------------------------------------
// `zhao_mem_upload` raises `publish_valid` with the resource's tag, base and
// extent when an upload has been verified and its directory row written. A
// publication whose tag is the SPECIES_TABLE kind is this block's start: the
// page is resident, CRC-checked and bounded before a single byte is read.
// Hooking the COMMAND instead would mean reading a page that is still landing.
//
// A publication that arrives while a load is running is REFUSED AND COUNTED
// (`pages_dropped_o`) rather than restarting or queueing: two species tables
// in flight is two authors' physics interleaved, and the page that lost is a
// fact the host needs rather than a silence.
//
// ---------------------------------------------------------------------------
// THE READ IS WHOLE 64-BYTE LINES, WHICH IS WHY THE LAYOUT IS SHAPED THAT WAY
// ---------------------------------------------------------------------------
// MEM.GUARD's shape rule requires the byte-enable mask to match the length, so
// a reader that asks for a whole line is the simplest one that can be correct.
// The page's header is one line and an entry is 32 bytes, so every line after
// the header carries exactly TWO entries and no entry ever straddles a read.
//
// A DENIED READ ABANDONS THE PAGE. `zhao_mem_guard` drops a denied request and
// nothing was read, so continuing would load a table out of whatever the beat
// bus held. `denied_o` counts it and the table keeps what it had -- which is
// the same answer an unloaded table gives, and entry I33 already records what
// that is (every consumer refuses on its own terms, with its own counter).
//
// ---------------------------------------------------------------------------
// A REFUSED PAGE IS REFUSED WHOLE
// ---------------------------------------------------------------------------
// The magic, the version and the entry count against the declared extent are
// all checked from the HEADER LINE, before any entry is emitted. A page that
// fails one of them loads NOTHING: a half-loaded species table is a particle
// engine running on a mixture of two authors' physics, which reads as a tuning
// problem and is not one.
//
// Conservative SystemVerilog subset only (charter 2).
`default_nettype none

module zhao_part_table_loader
  import zhao_pkg::*;
#(
    // `zhao_part_table`'s load word width. Checked against the port below at
    // elaboration rather than assumed.
    parameter int unsigned LD_W = 141,
    // spec/cartridge.md 4's page-kind registry; SPECIES_TABLE is 13 (R42).
    parameter logic [7:0] PAGE_KIND = 8'd13,
    // The bridge tag this block's reads carry. ENGINE1 is the asset window's
    // client, the same one MATERIAL.RESOLVE's record fetch uses.
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
    // ok is not read: iolation is the verdict this block acts on, and a
    // request that is neither ready nor violating is simply still waiting.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var zhao_guard_rsp_t g_rsp_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  var logic            g_beat_valid_i,
    input  var logic [63:0]     g_beat_data_i,

    // ---- PART.TABLE's load port --------------------------------------------
    output var logic            ld_valid_o,
    input  var logic            ld_ready_i,
    output var logic [1:0]      ld_sel_o,
    output var logic [6:0]      ld_index_o,
    output var logic [1:0]      ld_event_o,
    output var logic [LD_W-1:0] ld_data_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] pages_o,          // pages loaded whole
    output var logic [31:0] entries_o,        // load words handed over
    output var logic [31:0] pages_dropped_o,  // a publication while busy
    output var logic [31:0] bad_magic_o,      // magic or version wrong
    output var logic [31:0] truncated_o,      // entries run past the extent
    output var logic [31:0] denied_o,         // MEM.GUARD refused a read
    output var logic        busy_o
);

  localparam int unsigned LINE_BYTES  = 64;
  localparam int unsigned ENTRY_BYTES = 32;
  localparam logic [31:0] MAGIC       = 32'h5450_535A;   // 'ZSPT' little-endian
  localparam logic [15:0] VERSION     = 16'd1;

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`.
  initial begin
    if (LD_W < 1 || LD_W > 141)
      $fatal(1, "zhao_part_table_loader: LD_W=%0d outside the frozen entry's data field", LD_W);
  end

  // ---- the line buffer ----------------------------------------------------
  // One 64-byte line, assembled LSB-first from eight beats: beat k is bytes
  // 8k..8k+7, which is the wire order MEM.GUARD delivers and the order the
  // page is written in.
  logic [511:0] line_q;
  logic [  2:0] beat_q;

  // ---- the walk -----------------------------------------------------------
  localparam logic [2:0] S_IDLE   = 3'd0;
  localparam logic [2:0] S_HREQ   = 3'd1;   // ask for the header line
  localparam logic [2:0] S_HBEATS = 3'd2;   // take its eight beats
  localparam logic [2:0] S_HDR    = 3'd3;   // judge the header, ONCE
  localparam logic [2:0] S_EREQ   = 3'd4;   // ask for an entry line
  localparam logic [2:0] S_EBEATS = 3'd5;
  localparam logic [2:0] S_EMIT0  = 3'd6;   // hand over the line's first entry
  localparam logic [2:0] S_EMIT1  = 3'd7;   // ...and its second

  logic [ 2:0] st_q;
  logic [31:0] base_q;
  logic [31:0] extent_q;
  logic [15:0] count_q;      // entries the header declares
  logic [15:0] done_q;       // entries handed over so far
  logic [31:0] addr_q;       // the line being read

  // ---- the header fields, read off the line -------------------------------
  wire [31:0] hdr_magic_c   = line_q[31:0];
  wire [15:0] hdr_version_c = line_q[47:32];
  wire [15:0] hdr_count_c   = line_q[63:48];
  // `entries` must fit inside what the publication declared.
  wire [47:0] need_c = 48'(LINE_BYTES) + (48'(hdr_count_c) * 48'(ENTRY_BYTES));
  wire hdr_ok_c   = (hdr_magic_c == MAGIC) && (hdr_version_c == VERSION);
  wire hdr_fits_c = (need_c <= 48'(extent_q));

  // ---- the two entries in a line ------------------------------------------
  // Bit offsets are `zref::species_page`'s, restated rather than derived: a
  // change on either side must show up as a failure, not be tracked silently.
  // The line's two entries, selected by which one is being handed over.
  // Written as slices of ONE selected word rather than as three accessor
  // functions: a function taking the whole 256-bit entry and returning two
  // bits reads to the linter as 254 unused bits, which is true and unhelpful.
  // The reserved bits of an entry are DELIBERATELY not read -- they are zero
  // by the frozen layout and reading them would be this block developing an
  // opinion about a field the owner has not defined.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [255:0] ent_c = (st_q == S_EMIT1) ? line_q[511:256] : line_q[255:0];
  /* verilator lint_on UNUSEDSIGNAL */

  assign ld_sel_o   = ent_c[1:0];
  assign ld_event_o = ent_c[3:2];
  assign ld_index_o = ent_c[14:8];
  assign ld_data_o  = ent_c[32 +: LD_W];
  assign ld_valid_o = (st_q == S_EMIT0) || (st_q == S_EMIT1);
  wire   ld_fire_c  = ld_valid_o && ld_ready_i;

  assign busy_o = (st_q != S_IDLE);

  // ---- the read request ---------------------------------------------------
  wire req_c = (st_q == S_HREQ) || (st_q == S_EREQ);
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

  // The guard's verdict is REGISTERED one cycle after the request, and the
  // request is a LEVEL here, so both arrive while `req_c` is still up. A
  // denial is read on `violation` alone: `zhao_mem_guard` drops a denied
  // request, so whether `ready` rose with it changes nothing about what was
  // read, and reading the conjunction would make the abandon depend on a
  // detail of the share rather than on the verdict.
  wire denied_c = req_c && g_rsp_i.violation;
  wire accept_c = req_c && g_rsp_i.ready && !g_rsp_i.violation;

  // A publication is taken only between loads; while busy it is a DROP, and
  // the drop is the number the host needs.
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
      pages_o         <= 32'd0;
      entries_o       <= 32'd0;
      pages_dropped_o <= 32'd0;
      bad_magic_o     <= 32'd0;
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
            // A page too short to hold a header is truncated by definition,
            // and is refused before a single byte is asked for.
            if (pub_extent_i < 32'(LINE_BYTES)) begin
              truncated_o <= truncated_o + 32'd1;
            end else begin
              st_q <= S_HREQ;
            end
          end
        end

        S_HREQ: begin
          if (denied_c) begin
            denied_o <= denied_o + 32'd1;
            st_q     <= S_IDLE;
          end else if (accept_c) begin
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
          count_q <= hdr_count_c;
          if (!hdr_ok_c) begin
            bad_magic_o <= bad_magic_o + 32'd1;
            st_q        <= S_IDLE;
          end else if (!hdr_fits_c) begin
            truncated_o <= truncated_o + 32'd1;
            st_q        <= S_IDLE;
          end else if (hdr_count_c == 16'd0) begin
            // A legal, EMPTY page. It is a load: the host said "no
            // descriptors", and saying so is different from saying nothing.
            pages_o <= pages_o + 32'd1;
            st_q    <= S_IDLE;
          end else begin
            addr_q <= base_q + 32'(LINE_BYTES);
            beat_q <= 3'd0;
            st_q   <= S_EREQ;
          end
        end

        S_EREQ: begin
          if (denied_c) begin
            denied_o <= denied_o + 32'd1;
            st_q     <= S_IDLE;
          end else if (accept_c) begin
            beat_q <= 3'd0;
            st_q   <= S_EBEATS;
          end
        end

        S_EBEATS: begin
          if (g_beat_valid_i) begin
            line_q <= {g_beat_data_i, line_q[511:64]};
            beat_q <= beat_q + 3'd1;
            if (beat_q == 3'd7) st_q <= S_EMIT0;
          end
        end

        S_EMIT0: begin
          if (ld_fire_c) begin
            done_q    <= done_q + 16'd1;
            entries_o <= entries_o + 32'd1;
            if (done_q + 16'd1 >= count_q) begin
              pages_o <= pages_o + 32'd1;
              st_q    <= S_IDLE;
            end else begin
              st_q <= S_EMIT1;
            end
          end
        end

        S_EMIT1: begin
          if (ld_fire_c) begin
            done_q    <= done_q + 16'd1;
            entries_o <= entries_o + 32'd1;
            if (done_q + 16'd1 >= count_q) begin
              pages_o <= pages_o + 32'd1;
              st_q    <= S_IDLE;
            end else begin
              addr_q <= addr_q + 32'(LINE_BYTES);
              beat_q <= 3'd0;
              st_q   <= S_EREQ;
            end
          end
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_part_table_loader

`default_nettype wire
