// zhao_geom_ladderbank.sv -- GEOM.LOD's FOUR PER-CREATURE-TYPE CONSTANTS, from
// a published CREATURE_FORM page. Owner rulings R26 and R68, sub-builds 1 and 3.
//
// ENFORCED-BY: tests/geometry/geom_ladderbank_directed.cpp:main
// REFERENCE:   zref::creature_page (reference/include/zref/zref_creature_page.hpp)
//
// ---------------------------------------------------------------------------
// WHAT WAS MISSING, AND WHY IT WAS A RULING RATHER THAN AN OMISSION
// ---------------------------------------------------------------------------
// `fpga/rtl/prod/zhao_console_core.sv`'s FORGE.SHADOW entry traced the whole
// refusal to this one place and said so in terms:
//
//   "`bound_radius_i`, `micro_error_i`, `splat_error_i`, `glint_error_i`:
//    per-creature-TYPE constants. ... NO module emits a micro, splat or glint
//    error. AND THE REASON THAT LAST ONE CANNOT SIMPLY BE BUILT IS A RULING,
//    NOT AN OMISSION. Those four live in the compiled creature form page ...
//    'until then the packer refuses to emit them (deterministic refusal, never
//    a guessed layout)'. There is no layout to read because the project has
//    ruled that there must not be one yet."
//
// R26 lifted that freeze for these four fields and NO OTHERS, and
// `zref::creature_page` is the frozen layout. This block is its reader. It
// invents no field, reads no field outside the ladder table, and treats the
// page's declared `body_off` -- the Phase-12 body it is not allowed to know
// about -- as bytes it never asks for.
//
// ---------------------------------------------------------------------------
// THE TRIGGER IS THE PUBLICATION, NOT A COMMAND
// ---------------------------------------------------------------------------
// `zhao_mem_upload` raises `publish_valid` with the resource's tag, base and
// extent once an upload has been verified and its directory row written. A
// publication whose tag is the CREATURE_FORM kind is this block's start, which
// is `zhao_part_table_loader`'s trigger for the same reason: hooking the
// COMMAND instead would read a page that is still landing.
//
// A publication arriving while a load runs is REFUSED AND COUNTED
// (`pages_dropped_o`), never queued.
//
// ---------------------------------------------------------------------------
// A PAGE IS ADOPTED ATOMICALLY -- TWO BANKS, ONE SELECTOR
// ---------------------------------------------------------------------------
// The magic, the version and the record count are judged from the header line
// before a record is stored, but record legality (a positive bound radius, no
// negative error) can only be judged as each record arrives. Storing into the
// live rows and then discovering record nine is illegal would leave a bank
// holding eight rows of the new page and the rest of the old one -- a LADDER
// RUNNING ON TWO AUTHORS' CREATURES, which reads as an art problem and is not.
//
// So the store is DOUBLE-BANKED and one bit wide: a load fills the inactive
// half and `bank_q` flips only when the last record has landed legally. A
// refused page therefore leaves the live half byte-identical to what it was,
// and the refusal is a counter rather than a silence. The cost is ROWS*128
// spare bits, which at the default ROWS=16 is 2,048.
//
// ---------------------------------------------------------------------------
// A MISS IS A MISS
// ---------------------------------------------------------------------------
// The lookup is by MESH_STREAM handle index, the key `zhao_geom_drawjob`
// already uses for its residency directory (rule 5f.1, one key for both). A
// form with no row answers `a_hit_o` LOW and counts `lookup_miss_o`; it never
// answers with row zero and never with the nearest row. A ladder run against
// another creature's bound radius picks a rung `zref::creature::lod_raw` never
// picks, and nothing downstream can tell that it did -- the caller must see the
// miss and decline to tick.
//
// Conservative SystemVerilog subset only (charter 2). No divider, no
// multiplier, no function-call result indexed anywhere.
`default_nettype none

module zhao_geom_ladderbank
  import zhao_pkg::*;
#(
    // Creature types resident at once. NAMED AND EDITABLE (CLAUDE.md rule 6).
    // 16 is chosen against `zhao_geom_drawjob`'s XFORMS=256 instance tier: the
    // ruling's content tier is 256 creature INSTANCES, and a scene with more
    // than sixteen distinct creature TYPES on screen is a different content
    // question than the one that tier answers. A page declaring more rows than
    // this is REFUSED WHOLE (`overflow_o`), never truncated -- see the header.
    parameter int unsigned ROWS = 16,
    // spec/cartridge.md 4's page-kind registry; CREATURE_FORM is 8.
    parameter logic [7:0] PAGE_KIND = 8'd8,
    // The bridge tag this block's reads carry. ENGINE1 is the asset window's
    // client, the same one MATERIAL.RESOLVE's record fetch and
    // `zhao_part_table_loader` use.
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
    // `ok` is not read: a violation is the verdict this block acts on, and a
    // request that is neither ready nor violating is still waiting.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var zhao_guard_rsp_t g_rsp_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  var logic            g_beat_valid_i,
    input  var logic [63:0]     g_beat_data_i,

    // ---- the lookup: one creature type's ladder constants ------------------
    // Combinational-free: a query is accepted every cycle and its answer is
    // registered one cycle later. There is no handshake because there is no
    // state to stall -- the bank is a register file and the compare is one
    // level of logic.
    input  var logic        q_valid_i,
    input  var logic [23:0] q_form_i,
    output var logic        a_valid_o,
    output var logic        a_hit_o,
    output var logic signed [31:0] a_bound_o,
    output var logic signed [31:0] a_micro_o,
    output var logic signed [31:0] a_splat_o,
    output var logic signed [31:0] a_glint_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] pages_o,          // pages adopted whole
    output var logic [31:0] records_o,        // rows stored by adopted pages
    output var logic [31:0] pages_dropped_o,  // a publication while busy
    output var logic [31:0] bad_magic_o,      // magic or version wrong
    output var logic [31:0] truncated_o,      // records run past the extent
    output var logic [31:0] bad_record_o,     // zero/negative bound, or negative error
    output var logic [31:0] overflow_o,       // more records than ROWS
    output var logic [31:0] denied_o,         // MEM.GUARD refused a read
    output var logic [31:0] lookup_miss_o,    // a query whose form has no row
    output var logic        busy_o
);

  localparam int unsigned LINE_BYTES = 64;
  localparam int unsigned RECORD_BYTES = 32;
  localparam logic [31:0] MAGIC = 32'h4D46435A;  // 'Z','C','F','M' little-endian
  localparam logic [15:0] VERSION = 16'd1;
  localparam int unsigned ROWW = (ROWS <= 1) ? 1 : $clog2(ROWS);

  // Quartus 17.0 rejects a bare module-scope `if`; the guard lives inside
  // `initial begin ... end`. `--lint-only` does not run it, so a clean lint is
  // not evidence about it (CLAUDE.md, 2026-09-08). It is fired by parameter
  // override in the directed test.
  initial begin
    if (ROWS < 1 || ROWS > 65535)
      $fatal(1, "zhao_geom_ladderbank: ROWS is %0d; the header's record count is a u16", ROWS);
  end

  // ---- the line being assembled -------------------------------------------
  logic [511:0] line_q;
  logic [  2:0] beat_q;

  // ---- the double bank ----------------------------------------------------
  // Index {bank, row}: the live half is `bank_q`, the filling half its inverse.
  logic               key_v_q [2*ROWS];
  logic [23:0]        key_q   [2*ROWS];
  logic signed [31:0] bnd_q   [2*ROWS];
  logic signed [31:0] mic_q   [2*ROWS];
  logic signed [31:0] spl_q   [2*ROWS];
  logic signed [31:0] gli_q   [2*ROWS];
  logic               bank_q;

  // ---- the walk -----------------------------------------------------------
  localparam logic [2:0] S_IDLE   = 3'd0;
  localparam logic [2:0] S_HREQ   = 3'd1;   // ask for the header line
  localparam logic [2:0] S_HBEATS = 3'd2;   // take its eight beats
  localparam logic [2:0] S_HDR    = 3'd3;   // judge the header, ONCE
  localparam logic [2:0] S_RREQ   = 3'd4;   // ask for a record line
  localparam logic [2:0] S_RBEATS = 3'd5;
  localparam logic [2:0] S_STORE0 = 3'd6;   // store the line's first record
  localparam logic [2:0] S_STORE1 = 3'd7;   // ...and its second

  logic [ 2:0] st_q;
  logic [31:0] base_q;
  logic [31:0] extent_q;
  logic [15:0] count_q;      // records the header declares
  logic [15:0] done_q;       // records stored so far
  logic [31:0] addr_q;       // the line being read

  // ---- the header fields, read off the line -------------------------------
  // `body_off` at bytes 8..11 is DELIBERATELY not read. It names where the
  // Phase-12 body begins, and this block's whole licence is the four ladder
  // fields; reading it would be this block forming an opinion about the half
  // of the page R26 did not unfreeze.
  wire [31:0] hdr_magic_c   = line_q[31:0];
  wire [15:0] hdr_version_c = line_q[47:32];
  wire [15:0] hdr_count_c   = line_q[63:48];
  wire [47:0] need_c = 48'(LINE_BYTES) + (48'(hdr_count_c) * 48'(RECORD_BYTES));
  wire hdr_ok_c    = (hdr_magic_c == MAGIC) && (hdr_version_c == VERSION);
  wire hdr_fits_c  = (need_c <= 48'(extent_q));
  wire hdr_rooms_c = (48'(hdr_count_c) <= 48'(ROWS));

  // ---- the two records in a line ------------------------------------------
  // Bit offsets are `zref::creature_page`'s, RESTATED rather than derived: a
  // change on either side must show up as a failure rather than track silently.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [255:0] rec_c = (st_q == S_STORE1) ? line_q[511:256] : line_q[255:0];
  /* verilator lint_on UNUSEDSIGNAL */

  wire [31:0]        rec_form_c  = rec_c[31:0];
  wire signed [31:0] rec_bound_c = $signed(rec_c[63:32]);
  wire signed [31:0] rec_micro_c = $signed(rec_c[95:64]);
  wire signed [31:0] rec_splat_c = $signed(rec_c[127:96]);
  wire signed [31:0] rec_glint_c = $signed(rec_c[159:128]);

  // `zref::creature_page::record_legal`, restated. The high byte of
  // `form_index` is reserved because a handle32's low byte is its GENERATION
  // and the index is 24 bits; a page carrying a generation in the key would be
  // keyed by something the drawjob's directory is not keyed by.
  wire rec_legal_c = (rec_form_c[31:24] == 8'd0)
                  && (rec_bound_c > 32'sd0)
                  && (rec_micro_c >= 32'sd0)
                  && (rec_splat_c >= 32'sd0)
                  && (rec_glint_c >= 32'sd0);

  assign busy_o = (st_q != S_IDLE);

  // ---- the read request ---------------------------------------------------
  wire req_c = (st_q == S_HREQ) || (st_q == S_RREQ);
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

  wire denied_c = req_c && g_rsp_i.violation;
  wire accept_c = req_c && g_rsp_i.ready && !g_rsp_i.violation;

  wire pub_match_c = pub_valid_i && (pub_tag_i == PAGE_KIND);

  // ---- the lookup ---------------------------------------------------------
  // One level of compare across the live half. ROWS is small by construction
  // and the answer is registered, so this is a wide OR and not a search.
  logic            hit_c;
  logic [ROWW-1:0] hit_row_c;
  integer r;
  always_comb begin
    hit_c     = 1'b0;
    hit_row_c = '0;
    for (r = 0; r < ROWS; r = r + 1) begin
      if (!hit_c && key_v_q[{bank_q, ROWW'(r)}] && (key_q[{bank_q, ROWW'(r)}] == q_form_i)) begin
        hit_c     = 1'b1;
        hit_row_c = ROWW'(r);
      end
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      a_valid_o <= 1'b0;
      a_hit_o   <= 1'b0;
      a_bound_o <= 32'sd0;
      a_micro_o <= 32'sd0;
      a_splat_o <= 32'sd0;
      a_glint_o <= 32'sd0;
    end else begin
      a_valid_o <= q_valid_i;
      a_hit_o   <= q_valid_i && hit_c;
      if (q_valid_i && hit_c) begin
        a_bound_o <= bnd_q[{bank_q, hit_row_c}];
        a_micro_o <= mic_q[{bank_q, hit_row_c}];
        a_splat_o <= spl_q[{bank_q, hit_row_c}];
        a_glint_o <= gli_q[{bank_q, hit_row_c}];
      end else begin
        // A miss answers ZERO, not the last hit. A held stale answer beside a
        // low `a_hit_o` is the shape a careless caller reads as data.
        a_bound_o <= 32'sd0;
        a_micro_o <= 32'sd0;
        a_splat_o <= 32'sd0;
        a_glint_o <= 32'sd0;
      end
    end
  end

  // ---- the load -----------------------------------------------------------
  wire fill_c = ~bank_q;
  integer i;
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
      bank_q   <= 1'b0;
      for (i = 0; i < 2*ROWS; i = i + 1) begin
        key_v_q[i] <= 1'b0;
        key_q[i]   <= 24'd0;
        bnd_q[i]   <= 32'sd0;
        mic_q[i]   <= 32'sd0;
        spl_q[i]   <= 32'sd0;
        gli_q[i]   <= 32'sd0;
      end
      pages_o         <= 32'd0;
      records_o       <= 32'd0;
      pages_dropped_o <= 32'd0;
      bad_magic_o     <= 32'd0;
      truncated_o     <= 32'd0;
      bad_record_o    <= 32'd0;
      overflow_o      <= 32'd0;
      denied_o        <= 32'd0;
      lookup_miss_o   <= 32'd0;
    end else begin
      if (q_valid_i && !hit_c) lookup_miss_o <= lookup_miss_o + 32'd1;
      if (pub_match_c && (st_q != S_IDLE)) pages_dropped_o <= pages_dropped_o + 32'd1;

      case (st_q)
        S_IDLE: begin
          if (pub_match_c) begin
            base_q   <= pub_base_i;
            extent_q <= pub_extent_i;
            addr_q   <= pub_base_i;
            done_q   <= 16'd0;
            beat_q   <= 3'd0;
            // The filling half starts empty, so a short page cannot leave a
            // previous load's rows behind it.
            for (i = 0; i < ROWS; i = i + 1) key_v_q[{fill_c, ROWW'(i)}] <= 1'b0;
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
            // The eighth beat lands on THIS edge, so the header is not
            // readable until the next one. S_HDR is that cycle.
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
          end else if (!hdr_rooms_c) begin
            overflow_o <= overflow_o + 32'd1;
            st_q       <= S_IDLE;
          end else if (hdr_count_c == 16'd0) begin
            // A legal, EMPTY page. It IS a load: the author said "no creature
            // types", which is different from saying nothing, and the bank
            // adopts the empty half so every lookup misses honestly.
            bank_q  <= fill_c;
            pages_o <= pages_o + 32'd1;
            st_q    <= S_IDLE;
          end else begin
            addr_q <= base_q + 32'(LINE_BYTES);
            beat_q <= 3'd0;
            st_q   <= S_RREQ;
          end
        end

        S_RREQ: begin
          if (denied_c) begin
            denied_o <= denied_o + 32'd1;
            st_q     <= S_IDLE;
          end else if (accept_c) begin
            beat_q <= 3'd0;
            st_q   <= S_RBEATS;
          end
        end

        S_RBEATS: begin
          if (g_beat_valid_i) begin
            line_q <= {g_beat_data_i, line_q[511:64]};
            beat_q <= beat_q + 3'd1;
            if (beat_q == 3'd7) st_q <= S_STORE0;
          end
        end

        S_STORE0, S_STORE1: begin
          if (!rec_legal_c) begin
            // The page is abandoned with the LIVE half untouched: `bank_q` has
            // not moved, so nothing a lookup can see has changed.
            bad_record_o <= bad_record_o + 32'd1;
            st_q         <= S_IDLE;
          end else begin
            key_v_q[{fill_c, ROWW'(done_q[ROWW-1:0])}] <= 1'b1;
            key_q  [{fill_c, ROWW'(done_q[ROWW-1:0])}] <= rec_form_c[23:0];
            bnd_q  [{fill_c, ROWW'(done_q[ROWW-1:0])}] <= rec_bound_c;
            mic_q  [{fill_c, ROWW'(done_q[ROWW-1:0])}] <= rec_micro_c;
            spl_q  [{fill_c, ROWW'(done_q[ROWW-1:0])}] <= rec_splat_c;
            gli_q  [{fill_c, ROWW'(done_q[ROWW-1:0])}] <= rec_glint_c;
            done_q    <= done_q + 16'd1;
            records_o <= records_o + 32'd1;
            if (done_q + 16'd1 >= count_q) begin
              bank_q  <= fill_c;                 // ADOPT, atomically
              pages_o <= pages_o + 32'd1;
              st_q    <= S_IDLE;
            end else if (st_q == S_STORE0) begin
              st_q <= S_STORE1;
            end else begin
              addr_q <= addr_q + 32'(LINE_BYTES);
              beat_q <= 3'd0;
              st_q   <= S_RREQ;
            end
          end
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_geom_ladderbank

`default_nettype wire
