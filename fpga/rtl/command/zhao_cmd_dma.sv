// zhao_cmd_dma.sv — CMD.DMA: sealed-frame-packet fetch + the debug-blit
// DMA engine (plan W2.6, decisions D8/D10).
//
// Law (in citation order):
//   design/contracts/CMD.DMA.md  — the block contract (this file matures
//       the stub to real fetch: CRC gate BEFORE the first byte, payload CRC
//       at the end, epoch check, fail-safe drop order)
//   spec/capture_format.md 3/3.2 — the frame packet layout, the fail-safe
//       validation order and the bytes_consumed law (36 on header-level
//       abort, else 40+N). Checks 6 (pad-zero) / 7 (enum range) / 8
//       (handles) are RECORD-SEMANTIC checks owned by CMD.DECODER (wave 3);
//       this module owns the packet-level gate: 1,2,3,4,5,9,10 + epoch.
//   spec/memory_rules.md 3/4.1/4.3/5 — bridge burst law (64-B aligned,
//       1..64 B, one in flight per client), the FRAME_RING fetch, the blit
//       source (address/length/expected CRC from the DebugFrameBlit record)
//       and the region law: the blit writes exactly dst_slot's FB region,
//       exactly canvas_bytes(mode) bytes, any other length rejected BEFORE
//       the first byte.
//   spec/counters.md (D9) — commands / hps_ddr_bytes_by_client /
//       deadline_faults (drops) owned here; frame_tick latches shadows.
//
// D10 (harness-as-HPS): the bridge port is the generic burst channel; in
// simulation the harness C++ answers reads with the frozen sim profile
// (16 gpu cycles to first beat, 1 beat/cycle, `last` on the final beat of
// the burst). The RTL is latency-agnostic: it holds the request and counts
// beats until last.
//
// THE GATE (formal cmd_dma_crc_gate): NOTHING is emitted downstream —
// neither a verified packet byte (pkt_valid_o) nor a VRAM write
// (guard_req_o) — before the corresponding header-CRC / blit-CRC check
// passed. The architecture is buffer-then-release: bytes land in an
// internal staging buffer during fetch, are verified in full, and only a
// fully verified packet/stream is released. No partial delivery, ever.
//
// Status space: 0..14 = zhao_abi_error (ABI law); 15+ are MODULE-LOCAL
// extensions shared with zref_cmd2.hpp (never on the wire):
//   15  epoch mismatch (slot epoch != current epoch, dropped before the
//       first payload byte)
//   17  bridge error (rsp.err — malformed burst, nothing issued)
//   18  blit rejected (byte_len != canvas_bytes(mode) or bad dst_slot —
//       rejected before the first byte, zero guard writes)
//
// Client ids (frozen zhao_pkg enum; memory_rules.md 2 maps command DMA to
// the reserved engine RR slot): packet fetch = ZHAO_CLIENT_ENGINE0,
// blit = ZHAO_CLIENT_BLIT_DMA (both feed hps_ddr_bytes_by_client).
//
// VRAM write-data seam (CORRECTED for W2.7 shell composition): the frozen
// zhao_guard_req_t carries the transaction {client, write, addr, len, be}
// but no data lane. As shipped by W2.6 the guard_wdata_o sideband carried
// ONLY the first 8 bytes of each 64-byte write request — the other 56 bytes
// existed in blit_buf and on no wire, and cmd_dma_directed's "bit-exact"
// check only ever compared those first 8 bytes. Composing the real SDRAM
// datapath exposed the gap. The seam is now a BEAT STREAM: after the guard
// accepts a write request (guard_rsp_i.ready with guard_req_o.valid), this
// module streams ceil(len/8) beats of guard_wdata_o, one per cycle, marked
// by guard_wvalid_o — beat k carries bytes [k*8, k*8+8) of the request.
// The shell's write-data queue consumes exactly these beats and feeds the
// SDRAM controller's wr_beat pace. W2.5's guard owns the request-path
// shape; this data seam is recorded in the contract notes.
//
// Conservative SystemVerilog subset only (charter 2).
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_cmd_dma).

module zhao_cmd_dma #(
  // The one staging size left. BLIT_BUF_BYTES and the FORMAL_BLIT_LEN
  // override went with the blit engine in step 6: the whole-canvas buffer
  // is DEBUG.FRAMEBLIT's 64-byte chunk now, and there is no blit-length law
  // here to shrink for a tractable BMC depth.
  parameter int unsigned SLOT_BUF_BYTES = 4096
) (
  input  logic clk,
  input  logic rst_n,

  // ---- packet fetch request (from CMD.SCHEDULER's claim) -------------------
  input  logic        fetch_req_valid_i,
  output logic        fetch_req_ready_o,
  input  logic [1:0]  fetch_slot_i,
  input  logic [31:0] fetch_addr_i,      // HPS byte address of the slot body
  input  logic [31:0] fetch_byte_len_i,  // sealed length from the descriptor
  input  logic [31:0] fetch_epoch_i,     // the CURRENT resource epoch

  // ---- the verdict (one pulse per fetch) -----------------------------------
  output logic        dma_done_o,
  output logic [1:0]  dma_slot_o,
  output logic [7:0]  dma_status_o,      // zhao_abi_error / local 15/17/18
  output logic [31:0] dma_bytes_consumed_o,  // 36 | 40+N (capture_format 3.2)
  output logic [31:0] dma_cmds_consumed_o,   // records walked (on success)

  // ---- HPS bridge port (D10; the harness C++ is the HPS in sim) ------------
  output zhao_pkg::zhao_hps_burst_req_t hps_req_o,
  input  zhao_pkg::zhao_hps_burst_rsp_t hps_rsp_i,

  // ---- verified packet byte stream (decoder-facing) ------------------------
  output logic        pkt_valid_o,
  input  logic        pkt_ready_i,
  output logic [7:0]  pkt_byte_o,
  output logic [31:0] pkt_len_o,         // verified length (40+N)


  // ---- frame boundary (D9 shadow latch) ------------------------------------
  /* verilator lint_off UNUSEDSIGNAL */
  input  zhao_pkg::zhao_frame_tick_t frame_tick_i,
  /* verilator lint_on UNUSEDSIGNAL */

  // ---- D9 counters: owned here, shadows latched at frame_tick --------------
  output zhao_pkg::zhao_counter_snap_t snap_cmds_o,    // id 2  commands
  output zhao_pkg::zhao_counter_snap_t snap_bytes_o,   // id 29 hps bytes
  output zhao_pkg::zhao_counter_snap_t snap_drops_o    // id 1  deadline_faults
);

  // ---- status codes (ABI error codes verbatim; locals documented above) ----
  localparam logic [7:0] ST_OK              = 8'd0;
  localparam logic [7:0] ST_BAD_MAGIC       = 8'd1;
  localparam logic [7:0] ST_BAD_ABI_VER     = 8'd2;
  localparam logic [7:0] ST_RESERVED_FLAG   = 8'd3;
  localparam logic [7:0] ST_BAD_LENGTH      = 8'd4;
  localparam logic [7:0] ST_BAD_HEADER_CRC  = 8'd5;
  localparam logic [7:0] ST_BAD_PAYLOAD_CRC = 8'd6;
  localparam logic [7:0] ST_UNKNOWN_OPCODE  = 8'd7;
  localparam logic [7:0] ST_TRUNCATED       = 8'd11;
  localparam logic [7:0] ST_DEBUG_FLAG      = 8'd12;
  localparam logic [7:0] ST_COUNT_MISMATCH  = 8'd13;
  localparam logic [7:0] ST_EPOCH           = 8'd15;  // module-local (zref)
  localparam logic [7:0] ST_BRIDGE_ERR      = 8'd17;  // module-local (zref)

  // ---- record size table (generated LayoutIR sizes — never hand-derived) ---
  function automatic logic [15:0] rec_size(input logic [15:0] op);
    case (op)
      zhao_abi_pkg::ZHAO_OP_NOP:                       rec_size = 16'd16;
      zhao_abi_pkg::ZHAO_OP_BEGIN_FRAME:               rec_size = 16'd32;
      zhao_abi_pkg::ZHAO_OP_END_FRAME:                 rec_size = 16'd32;
      zhao_abi_pkg::ZHAO_OP_SET_VIEW:                  rec_size = 16'd96;
      zhao_abi_pkg::ZHAO_OP_SET_PRESENTATION_CONTRACT: rec_size = 16'd48;
      zhao_abi_pkg::ZHAO_OP_TERRAIN_FIELD:             rec_size = 16'd112;
      zhao_abi_pkg::ZHAO_OP_SURFACE_STAMP:             rec_size = 16'd64;
      zhao_abi_pkg::ZHAO_OP_DRAW_FORM:                 rec_size = 16'd32;
      zhao_abi_pkg::ZHAO_OP_DRAW_POPULATION:           rec_size = 16'd32;
      zhao_abi_pkg::ZHAO_OP_DRAW_PROCEDURAL:           rec_size = 16'd64;
      zhao_abi_pkg::ZHAO_OP_DRAW_SKY:                  rec_size = 16'd176;
      zhao_abi_pkg::ZHAO_OP_EMIT_AUDIO_EVENT:          rec_size = 16'd32;
      zhao_abi_pkg::ZHAO_OP_DEBUG_BOOTSTRAP:           rec_size = 16'd64;
      zhao_abi_pkg::ZHAO_OP_DEBUG_FRAME_BLIT:          rec_size = 16'd48;
      zhao_abi_pkg::ZHAO_OP_DEBUG_RUMBLE:              rec_size = 16'd32;
      default:                                         rec_size = 16'd0;
    endcase
  endfunction

  function automatic logic [31:0] canvas_bytes(input logic [7:0] m);
    canvas_bytes = zhao_pkg::zhao_canvas_bytes(zhao_pkg::zhao_mode_from_abi(m));
  endfunction

  // the mode's lawful blit length. Under `ifdef FORMAL the harness may
  // override it downward (FORMAL_BLIT_LEN != 0) so the blit CRC gate is
  // reachable within the BMC depth; everywhere else this IS canvas_bytes.

  // ------------------------------------------------------------ state -----
  // FIVE bits, not four. Seventeen states now, and squeezing logic together to
  // stay inside sixteen would be optimising the wrong thing during timing
  // closure -- the encoding costs a flop or two.
  typedef enum logic [4:0] {
    M_IDLE,
    M_HDR_REQ, M_HDR_WAIT, M_HCRC, M_HDR_CHK, M_SEED_PREP, M_SEED,
    M_PAY_REQ, M_PAY_WAIT,
    M_PCRC_RD, M_PCRC,
    M_WALK_RD, M_WALK_DEC, M_WALK,
    M_PKT_DONE,
    M_STREAM_RD, M_STREAM_LD, M_STREAM
  } dma_state_e;

  /* verilator lint_off PROCASSINIT */
  dma_state_e m = M_IDLE;

  // THE STAGING BUFFER, IN TWO PARTS.
  //
  // It used to be one 4,096-entry byte array with a variable read address and
  // a variable write address. Quartus built exactly that: 94,698 combinational
  // ALUTs and 33,680 registers -- 32,768 of them this array -- against a device
  // holding ~41,910 ALMs, with `Total block memory bits: 0`. Twice the device,
  // and no mux restructuring could touch it, because the cost WAS the register
  // file.
  //
  // Split by who reads it:
  //
  //   hdr_win  the first 64 bytes, in registers. EVERY read on the header path
  //            lands here -- the header fields (all below offset 40), the
  //            header CRC over [0,32), and the payload-CRC seed over [36,64).
  //            So M_HDR_CHK keeps its one-cycle ladder unchanged. 512 registers.
  //
  //   slot_ram the whole packet, 512 x 64b, for the three readers that need
  //            arbitrary offsets: the payload-CRC word, the record walk, and
  //            the verified stream. M10K rules: no initialiser, written by a
  //            process with NO reset, one registered read port.
  //
  // Every multi-byte read is contained in ONE 64-bit word, so none of them
  // costs a second access: command_bytes is a multiple of 16 and record
  // lengths are multiples of 16, so 36+cb and 36+walk_off are both 4 mod 8,
  // and a 32-bit field at 4 mod 8 -- or the walk's TWO 16-bit fields at 4 and
  // 6 -- sits inside the word.
  logic [7:0]  hdr_win [0:63];
  logic [63:0] slot_ram [0:(SLOT_BUF_BYTES/8)-1];
  logic [63:0] ram_q;
  logic [8:0]  ram_addr;
  logic [63:0] stream_w;


  // fetch latches
  logic [1:0]  f_slot = 2'd0;
  logic [31:0] f_addr = 32'd0;
  logic [31:0] f_len = 32'd0;   // descriptor byte_len
  logic [31:0] f_epoch = 32'd0;

  // parsed header (latched at the header check)
  logic [31:0] cb = 32'd0;      // command_bytes
  logic [31:0] cc = 32'd0;      // command_count
  logic        h_debug = 1'b0;  // frame header flags bit0 (debug umbrella)

  // burst machinery
  logic [31:0] fetched = 32'd0;    // packet bytes landed in slot_buf
  logic [31:0] need_total = 32'd0; // 40 + cb
  logic [31:0] wr_off = 32'd0;
  // End offset of the burst IN FLIGHT. The bridge forwards `last` straight
  // through from the external HPS without counting beats against the length
  // it was asked for (zhao_hps_bridge.sv: `rsp.last <= hps_rd_last`), so a
  // bridge that over-runs its own burst would otherwise keep landing beats
  // here. This makes the burst end a fact THIS module knows.
  logic [31:0] burst_end = 32'd0;
  // THE HEADER CRC AND THE PAYLOAD SEED ARE NOW WALKED, NOT SWEPT.
  //
  // Both used to run as single-cycle chains of bit-serial CRC-32C steps --
  // 32 bytes for the header, up to 28 for the payload seed -- and the composed
  // fit measured what that costs:
  //
  //   -55.199 ns  zhao_cmd_dma|hdr_win[28][4] -> zhao_cmd_dma|crc_pay_r[3]
  //
  // hdr_win[28] is command_bytes, so that path is: read cb, derive seed_end,
  // then 28 dependent CRC steps. 224 XOR levels against a 10 ns budget.
  //
  // Now each walks its byte range EIGHT BYTES PER CYCLE through one shared
  // zhao_crc32c_fold, which does eight bytes in one tree about seven levels
  // deep. Four cycles each, eight cycles added per packet, on a path that runs
  // once per packet and never per beat.
  logic [1:0]  cw = 2'd0;        // which eight-byte group of the walk
  logic [31:0] crc_hdr_r = 32'hFFFF_FFFF;
  // LATCHED at the end of the header check, and read by nothing before it.
  // The old worst path was hdr_win[28] -> crc_pay_r: command_bytes reached the
  // CRC's loop bound in the same cycle as the CRC itself. These registers cut
  // that, so M_SEED and M_PAY_WAIT decide from state, not from the window.
  logic [31:0] payload_end_q = 32'd0;   // 36 + command_bytes
  logic [5:0]  seed_bytes_q = 6'd0;     // 0, 16 or 28 -- and only those
  logic [1:0]  seed_steps_q = 2'd0;     // 0, 2 or 4 folds
  logic [31:0] crc_pay_r = 32'hFFFF_FFFF;

  // record walk
  // THE RECORD'S FIELDS AND ITS EXPECTED SIZE, REGISTERED.
  //
  // M_WALK used to do all of this in the cycle the staging word arrived:
  // extract the opcode and length from ram_q, look the opcode's lawful size up
  // in rec_size() -- a case over the whole opcode space -- and then run the
  // four-deep validity ladder into done_status, walk_off and walk_cnt. The
  // census named it as the design's worst family once the CRCs were gone:
  //
  //   623 paths  slot_ram -> done_status   -1.472
  //   1,138      slot_ram -> walk_off      -1.155
  //   1,398      slot_ram -> walk_cnt      -1.082
  //
  // The lookup now ends at a register and the ladder starts from one.
  logic [15:0] op_q = 16'd0;
  logic [15:0] rb_q = 16'd0;
  logic [15:0] rsz_q = 16'd0;      // rec_size(op_q), the lawful record length
  logic [31:0] walk_off = 32'd0;   // offset within the command stream
  logic [31:0] walk_cnt = 32'd0;

  // verified-packet stream
  logic [31:0] rd_off = 32'd0;
  logic        pkt_v = 1'b0;
  logic [31:0] pkt_len_r = 32'd0;

  // verdict pipeline (every outcome shaped identically)
  logic        done_v = 1'b0;
  logic [1:0]  done_slot = 2'd0;
  logic [7:0]  done_status = 8'd0;
  logic [31:0] done_bytes = 32'd36;
  logic [31:0] done_cmds = 32'd0;

  // bridge request registers
  logic        hps_req_v = 1'b0;
  logic [31:0] hps_addr = 32'd0;
  logic [6:0]  hps_len = 7'd0;

  // gate flags (formal anchors): set ONLY by the matching CRC comparison
  // (consumed by the `ifdef FORMAL properties — lint anchor note)
  /* verilator lint_off UNUSEDSIGNAL */
  logic hdr_gate = 1'b0;   // header checks + header CRC passed
  logic pay_gate = 1'b0;   // payload CRC + record walk passed
  /* verilator lint_on UNUSEDSIGNAL */

  // D9 counters (u64, saturate never wrap)
  logic [63:0] live_cmds = 64'd0;
  logic [63:0] live_bytes = 64'd0;
  logic [63:0] live_drops = 64'd0;
  logic [63:0] sh_cmds = 64'd0;
  logic [63:0] sh_bytes = 64'd0;
  logic [63:0] sh_drops = 64'd0;
  logic        snap_v = 1'b0;
  /* verilator lint_on PROCASSINIT */

  // saturating add (spec/counters.md 4)
  function automatic logic [63:0] sat_add(input logic [63:0] a,
                                          input logic [63:0] b);
    sat_add = (a > 64'hFFFF_FFFF_FFFF_FFFF - b) ? 64'hFFFF_FFFF_FFFF_FFFF
                                                : (a + b);
  endfunction

  // header field readers (little-endian, capture_format.md 3)
  function automatic logic [15:0] hget16(input int unsigned off);
    hget16 = {hdr_win[off + 1], hdr_win[off]};
  endfunction

  function automatic logic [31:0] hget32(input int unsigned off);
    hget32 = {hdr_win[off + 3], hdr_win[off + 2], hdr_win[off + 1],
              hdr_win[off]};
  endfunction

  // CRC-32C (finalized) over the 32-byte frame header (capture_format.md 3:
  // header_crc32c covers bytes [0,32) — the ONLY use of this function, so
  // the loop bound is the constant 32; a parameter-bounded loop would put
  // SLOT_BUF_BYTES muxed CRC steps in the formal cone for nothing)
  // crc_final() is gone. It swept 32 bytes of hdr_win in ONE cycle -- 32
  // chained bit-serial steps, 256 dependent XOR levels -- and the header CRC
  // is now accumulated in crc_hdr_r by the M_HCRC walk instead. The comparison
  // in the ladder reads a register.

  // next burst length: multiple of 8, capped at 64, covering `left` bytes
  function automatic logic [6:0] burst_len(input logic [31:0] left);
    logic [31:0] n8;
    n8 = ((left + 32'd7) >> 3) << 3;
    burst_len = (n8 > 32'd64) ? 7'd64 : 7'(n8);
  endfunction

  // ------------------------------------------------------ ready/valid -------
  assign fetch_req_ready_o = (m == M_IDLE);

  always_comb begin
    hps_req_o.valid  = hps_req_v;
    hps_req_o.write  = 1'b0;
    // Only the command-packet fetch reaches HPS from here now. The blit was
    // the sole ZHAO_CLIENT_BLIT_DMA user and it has moved to DEBUG.FRAMEBLIT.
    hps_req_o.client = zhao_pkg::ZHAO_CLIENT_ENGINE0;
    hps_req_o.addr   = hps_addr;
    hps_req_o.len    = hps_len;
  end


  // ---- the CRC folds, at CONSTANT byte counts -----------------------------
  //
  // TWO instances with n_i tied to constants, NOT one with a runtime n_i.
  // The generic module elaborates nine matrices, one per byte count; with n_i
  // constant Quartus discards the other eight, which is why DEBUG.FRAMEBLIT's
  // fold cost 14 ALMs. A runtime n_i would keep all nine and put a nine-way
  // mux AFTER the XOR trees -- the opposite of what is wanted on the path
  // being shortened.
  //
  // Eight and four are the only counts this block needs, and that is a
  // consequence of the header laws rather than a convenience:
  //
  //   * command_bytes % 16 == 0, so 36 + command_bytes is always 4 mod 8;
  //     the final payload beat therefore contributes EXACTLY four bytes.
  //   * the first burst is a multiple of 8 capped at 64, and 40 + cb <= f_len,
  //     so the bytes of payload already present when the header is checked can
  //     only be 0, 16 or 28 -- never an arbitrary count.
  //
  //     cb = 0   -> fetched 40, seed_end 36  ->  0 bytes
  //     cb = 16  -> fetched >= 56, seed_end 52 -> 16 bytes
  //     cb >= 32 -> fetched 64, seed_end 64  -> 28 bytes
  //
  // So the seed is 0, two folds of 8, or three of 8 plus one of 4. No shifter
  // and no variable count anywhere.
  logic [31:0] fold_c;
  logic [63:0] fold_d;
  logic [31:0] fold8_o;
  logic [31:0] fold4_o;
  logic        fold_is4;
  logic [31:0] fold_o;

  zhao_crc32c_fold u_fold8 (.c_i(fold_c), .d_i(fold_d), .n_i(4'd8), .c_o(fold8_o));
  zhao_crc32c_fold u_fold4 (.c_i(fold_c), .d_i(fold_d), .n_i(4'd4), .c_o(fold4_o));
  assign fold_o = fold_is4 ? fold4_o : fold8_o;

  // the eight header-window bytes this walk step covers; the base is one of
  // eight fixed offsets (0,8,16,24 for the header CRC, 36,44,52,60 for the
  // seed), so this is a byte mux and not a shifter
  logic [5:0]  fold_base;
  logic [63:0] hw_word;
  always_comb begin
    // 0, 8, 16, 24 for the header CRC; 36, 44, 52, 60 for the seed. Six bits
    // is exactly the hdr_win index width, and 36 + 24 = 60 is the largest base.
    fold_base = (m == M_SEED) ? (6'd36 + {1'b0, cw, 3'd0}) : {1'b0, cw, 3'd0};
    for (int k = 0; k < 8; k++) begin
      hw_word[8*k +: 8] = hdr_win[fold_base + 6'(k)];
    end
  end

  // The streaming case needs NO shifter. M_PAY_WAIT's wr_off starts at
  // `fetched`, which is at least 40, and the payload starts at 36 -- so the
  // LOWER bound never clips a beat. Only the upper one does, and it lands
  // 4 mod 8, so the last payload beat is exactly a fold of four.
  logic beat_full;    // this beat is entirely inside the payload
  logic beat_tail;    // this beat holds the final four payload bytes
  always_comb begin
    beat_full = ((wr_off + 32'd8) <= payload_end_q);
    beat_tail = !beat_full && (wr_off < payload_end_q);
  end

  always_comb begin
    unique case (m)
      M_HCRC: begin
        fold_c   = crc_hdr_r;
        fold_d   = hw_word;
        fold_is4 = 1'b0;             // bytes 0..31, four whole groups
      end
      M_SEED: begin
        fold_c = crc_pay_r;
        fold_d = hw_word;
        // Only the 28-byte seed has a tail, and it is its fourth step. The
        // decision reads LATCHED controls, never hdr_win or command_bytes --
        // the old worst path began at hdr_win[28] precisely because the loop
        // bound was derived in the same cycle as the CRC.
        fold_is4 = (seed_bytes_q == 6'd28) && (cw == 2'd3);
      end
      default: begin
        fold_c   = crc_pay_r;
        fold_d   = hps_rsp_i.data;
        fold_is4 = beat_tail;
      end
    endcase
  end

  // The read address, muxed by state. The four readers are in four DIFFERENT
  // states, which is what lets them share one port -- splitting the payload
  // CRC into M_PCRC was the precondition for this, not a detour.
  always_comb begin
    case (m)
      M_PCRC_RD:   ram_addr = 9'((32'd36 + cb) >> 3);
      M_WALK_RD:   ram_addr = 9'((32'd36 + walk_off) >> 3);
      M_STREAM_RD: ram_addr = 9'd0;
      // one word of run-up: the stream reads a word every EIGHT bytes, so the
      // next one is always fetched seven cycles before it is needed
      default:     ram_addr = 9'((rd_off >> 3) + 32'd1);
    endcase
  end

  // M10K: no initialiser on the array, no reset in this process, registered
  // read. One word per bridge beat, which is exactly what a beat is.
  always_ff @(posedge clk) begin
    if (((m == M_HDR_WAIT) || (m == M_PAY_WAIT))
        && hps_rsp_i.beat_valid && !hps_rsp_i.err
        && (wr_off < SLOT_BUF_BYTES)) begin
      slot_ram[wr_off[11:3]] <= hps_rsp_i.data;
    end
    ram_q <= slot_ram[ram_addr];
  end

  assign pkt_valid_o = pkt_v;
  assign pkt_byte_o = stream_w[8*rd_off[2:0] +: 8];
  assign pkt_len_o = pkt_len_r;

  assign dma_done_o = done_v;
  assign dma_slot_o = done_slot;
  assign dma_status_o = done_status;
  assign dma_bytes_consumed_o = done_bytes;
  assign dma_cmds_consumed_o = done_cmds;


  // ------------------------------------------------------ sequential ------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      m <= M_IDLE;
      f_slot <= 2'd0; f_addr <= 32'd0; f_len <= 32'd0; f_epoch <= 32'd0;
      cb <= 32'd0; cc <= 32'd0; h_debug <= 1'b0;
      fetched <= 32'd0; need_total <= 32'd0; wr_off <= 32'd0;
      burst_end <= 32'd0;
      cw <= 2'd0; crc_hdr_r <= 32'hFFFF_FFFF;
      payload_end_q <= 32'd0; seed_bytes_q <= 6'd0; seed_steps_q <= 2'd0;
      crc_pay_r <= 32'hFFFF_FFFF;
      walk_off <= 32'd0; walk_cnt <= 32'd0;
      op_q <= 16'd0; rb_q <= 16'd0; rsz_q <= 16'd0;
      rd_off <= 32'd0; pkt_v <= 1'b0; pkt_len_r <= 32'd0;
      stream_w <= 64'd0;
      done_v <= 1'b0; done_slot <= 2'd0; done_status <= 8'd0;
      hps_req_v <= 1'b0; hps_addr <= 32'd0; hps_len <= 7'd0;
      hdr_gate <= 1'b0; pay_gate <= 1'b0;
      live_cmds <= 64'd0; live_bytes <= 64'd0; live_drops <= 64'd0;
      sh_cmds <= 64'd0; sh_bytes <= 64'd0; sh_drops <= 64'd0;
      snap_v <= 1'b0;
    end else begin
      // pulse defaults
      done_v <= 1'b0;
      snap_v <= 1'b0;
      hps_req_v <= 1'b0;

      // fetched-byte accounting (every accepted bridge beat, both engines)
      if (hps_rsp_i.beat_valid && !hps_rsp_i.err) begin
        live_bytes <= sat_add(live_bytes, 64'd8);
      end

      case (m)
        // ------------------------------------------------ packet fetch -----
        M_IDLE: begin
          hdr_gate <= 1'b0;
          pay_gate <= 1'b0;
          if (fetch_req_valid_i) begin
            f_slot <= fetch_slot_i;
            f_addr <= fetch_addr_i;
            f_len <= fetch_byte_len_i;
            f_epoch <= fetch_epoch_i;
            fetched <= 32'd0;
            walk_off <= 32'd0;
            walk_cnt <= 32'd0;
            if (fetch_byte_len_i < 32'd36) begin
              // fail-safe #1: cannot even hold the 36-byte header
              done_v <= 1'b1; done_slot <= fetch_slot_i;
              done_status <= ST_BAD_LENGTH; done_bytes <= 32'd36;
              live_drops <= sat_add(live_drops, 64'd1);
              m <= M_IDLE;  // nothing fetched: return immediately
            end else begin
              m <= M_HDR_REQ;
            end
          end
        end

        M_HDR_REQ: begin
          hps_req_v <= 1'b1;
          hps_addr <= f_addr;
          hps_len <= burst_len(f_len);
          wr_off <= 32'd0;
          burst_end <= 32'(burst_len(f_len));
          m <= M_HDR_WAIT;
        end

        M_HDR_WAIT: begin
          if (hps_rsp_i.err) begin
            done_v <= 1'b1; done_slot <= f_slot; done_status <= ST_BRIDGE_ERR;
            done_bytes <= 32'd36; done_cmds <= 32'd0;
            live_drops <= sat_add(live_drops, 64'd1);
            m <= M_IDLE;
          end else if (hps_rsp_i.beat_valid) begin
            for (int i = 0; i < 8; i++) begin
              if ((wr_off + 32'(i)) < 32'd64) begin
                hdr_win[wr_off + 32'(i)] <= hps_rsp_i.data[8*i +: 8];
              end
            end
            wr_off <= wr_off + 32'd8;
            fetched <= fetched + 32'd8;
            if (hps_rsp_i.last || ((wr_off + 32'd8) >= burst_end)) begin
              crc_hdr_r <= 32'hFFFF_FFFF;
              cw <= 2'd0;
              m <= M_HCRC;
            end
          end
        end

        // Four cycles, eight bytes each, over hdr_win[0..31]. This is the
        // header CRC that crc_final() used to sweep in one cycle.
        M_HCRC: begin
          crc_hdr_r <= fold_o;
          cw <= cw + 2'd1;
          if (cw == 2'd3) begin
            cw <= 2'd0;
            m <= M_HDR_CHK;
          end
        end

        M_HDR_CHK: begin
          // fail-safe order, checks 1-3 + epoch (capture_format.md 3.2).
          // Blocking locals first: one ladder, first failure wins.
          logic [31:0] cb_v;
          logic [31:0] cc_v;
          logic [15:0] fl_v;
          logic [7:0]  st_v;
          logic        ok_v;
          cb_v = hget32(zhao_abi_pkg::ZHAO_OFF_COMMAND_BYTES);
          cc_v = hget32(zhao_abi_pkg::ZHAO_OFF_COMMAND_COUNT);
          fl_v = hget16(zhao_abi_pkg::ZHAO_OFF_FLAGS);
          ok_v = 1'b1;
          st_v = ST_OK;
          if (fetched < 32'd36) begin
            ok_v = 1'b0; st_v = ST_BAD_LENGTH;      // truncated header burst
          end else if (hget32(0) != zhao_abi_pkg::ZHAO_FRAME_MAGIC) begin
            ok_v = 1'b0; st_v = ST_BAD_MAGIC;
          end else if (hget16(zhao_abi_pkg::ZHAO_OFF_ABI_VERSION)
                       != 16'(zhao_abi_pkg::ZHAO_ABI_VERSION)) begin
            ok_v = 1'b0; st_v = ST_BAD_ABI_VER;
          end else if (|(fl_v & 16'hFFFE)) begin
            ok_v = 1'b0; st_v = ST_RESERVED_FLAG;
          end else if ((cb_v & 32'd15) != 32'd0) begin
            ok_v = 1'b0; st_v = ST_BAD_LENGTH;      // N % 16
          end else if (cc_v > (cb_v >> 5'd4)) begin
            ok_v = 1'b0; st_v = ST_BAD_LENGTH;      // count*16 <= command_bytes
          end else if ((32'd40 + cb_v) > f_len) begin
            ok_v = 1'b0; st_v = ST_BAD_LENGTH;      // packet == descriptor len
          end else if ((32'd40 + cb_v) > SLOT_BUF_BYTES) begin
            ok_v = 1'b0; st_v = ST_BAD_LENGTH;      // staging bound
          end else if ((32'd40 + cb_v)
                       > (zhao_abi_pkg::FRAME_SLOT_BYTES - 32'd40)) begin
            ok_v = 1'b0; st_v = ST_BAD_LENGTH;      // FRAME_SLOT_BYTES law
          end else if ({~crc_hdr_r} != hget32(zhao_abi_pkg::ZHAO_OFF_HEADER_CRC)) begin
            ok_v = 1'b0; st_v = ST_BAD_HEADER_CRC;  // THE header gate
          end else if (hget32(zhao_abi_pkg::ZHAO_OFF_RESOURCE_EPOCH) != f_epoch)
          begin
            ok_v = 1'b0; st_v = ST_EPOCH;           // drop before payload
          end
          if (!ok_v) begin
            done_v <= 1'b1; done_slot <= f_slot; done_status <= st_v;
            done_bytes <= 32'd36; done_cmds <= 32'd0;  // header-level abort
            live_drops <= sat_add(live_drops, 64'd1);
            m <= M_IDLE;
          end else begin
            hdr_gate <= 1'b1;
            cb <= cb_v;
            cc <= cc_v;
            h_debug <= fl_v[0];
            need_total <= 32'd40 + cb_v;
            // Seed the payload CRC over the payload bytes burst 1 already
            // landed: bytes [36, min(fetched, 40+N)).
            //
            // THE BOUND IS 64 AND IT USED TO BE 192, WHICH MADE THIS BLOCK
            // UNFITTABLE. Every iteration is a real CRC-32C byte step, so the
            // bound is the length of a DEPENDENT combinational chain: 156
            // steps, about 1,248 chained XOR/shift stages, each with a guarded
            // read of a 4,096-entry register array. Measured 2026-08-21,
            // quartus_map did not finish it in 4,838 s, and the census row for
            // this block has never carried data for the same reason.
            //
            // At most 28 of those steps could ever be active. Derivation:
            // `fetched` is zeroed when the fetch is accepted; M_HDR_REQ issues
            // exactly ONE burst; `burst_len` caps at 64 bytes; M_HDR_WAIT adds
            // 8 per beat and leaves on `last`. So `fetched` <= 64 here, always,
            // and `seed_end` is then capped lower still by 36 + N. Iterations
            // 64..191 were guarded dead logic that synthesis had to build
            // before it could discard them.
            //
            // Bounding at 64 is therefore EXACTLY equivalent, not an
            // approximation: the removed iterations had `k < seed_end` false
            // for every reachable state.
            //
            // The bound is ASSERTED below rather than left as prose, because
            // this is the whole reason the loop is safe.
            //
            // ONE EQUIVALENT MUTANT, recorded so it does not read as a hole.
            // Replacing `seed_end = fetched` with the constant 64 survives the
            // sweep, and it is genuinely equivalent rather than untested:
            // `fetched` is min(64, roundup8(f_len)), and the cap on the next
            // line is 36 + N = f_len - 4, which is ALWAYS below `fetched`
            // whenever `fetched` is under 64. So the cap decides in exactly the
            // cases where the two inputs differ. Checked over every packet
            // length from 40 to 4,096: zero differences.
            //
            // `fetched` stays because it says what the value MEANS -- how much
            // burst 1 actually landed -- where 64 would be a constant that
            // happens to work.
            // ENFORCED-BY: tests/command/cmd_dma_directed.cpp
            // The seed controls are derived in M_SEED_PREP, from the LATCHED
            // cb, not here from the window.
            //
            // This cycle already checks magic, ABI version, reserved flags,
            // four length laws, the header CRC and the epoch. Deriving the
            // seed controls here as well put command_bytes' own bits at the
            // end of all of it, and the census named the result: 6,973 of the
            // design's 13,651 negative-slack paths ran
            // hdr_win -> seed_steps_q, worst -1.096 ns. Half the remaining
            // timing problem was this one extra job in an already full cycle.
            crc_pay_r <= 32'hFFFF_FFFF;
            cw <= 2'd0;
            m <= M_SEED_PREP;
          end
        end

        // One cycle, entirely from registers: cb and fetched were latched by
        // the header check, so nothing here reads hdr_win.
        M_SEED_PREP: begin
          // THE SEED LENGTH IS A THREE-WAY DECISION ON cb, NOT A CALCULATION.
          //
          // I proved this to justify the constant fold widths and then went on
          // computing it the long way -- min(fetched, 36+cb) - 36, then two
          // range compares. The census showed exactly what that cost:
          // 18,013 of 20,000 negative-slack paths ran cb -> seed_steps_q at
          // -1.324 ns, ninety percent of the design's remaining timing
          // problem, for a value that can only ever be one of three numbers.
          //
          //   cb =  0  -> f_len >= 40, fetched = 40, seed_end = 36  ->  0 bytes
          //   cb = 16  -> f_len >= 56, seed_end = 52                -> 16 bytes
          //   cb >= 32 -> 36+cb >= 68 > 64 >= fetched = 64          -> 28 bytes
          //
          // The middle line is the one worth checking: fetched is
          // min(roundup8(f_len), 64) and 36+cb is 52, so seed_end is 52 for
          // every lawful f_len, not just the smallest. And cb % 16 == 0 is
          // already enforced by the header ladder, so there is no cb = 8 case.
          //
          // Two equality compares against constants, no adder, no subtractor.
          // ENFORCED-BY: fpga/rtl/command/zhao_cmd_dma.sv:a_seed_bytes_lawful
          payload_end_q <= 32'd36 + cb;
          seed_bytes_q  <= (cb == 32'd0)  ? 6'd0
                         : (cb == 32'd16) ? 6'd16
                         :                  6'd28;
          seed_steps_q  <= (cb == 32'd0)  ? 2'd3   // no folds
                         : (cb == 32'd16) ? 2'd2   // two folds
                         :                  2'd0;  // four folds
          m <= M_SEED;
        end

        // Four cycles over hdr_win[36..63], each folding however many of its
        // eight bytes are still inside the payload. Where the old version put
        // 28 dependent CRC steps behind a command_bytes-derived bound, this
        // puts one fold behind it.
        M_SEED: begin
          // seed_steps_q encodes where the walk stops: 2'd0 means run all four
          // groups (28 bytes, the last a fold of 4), 2'd2 means stop after two
          // (16 bytes), 2'd3 means there is nothing to fold at all.
          if (seed_steps_q == 2'd3) begin
            cw <= 2'd0;
            m  <= (fetched >= (32'd40 + cb)) ? M_PCRC_RD : M_PAY_REQ;
          end else begin
            crc_pay_r <= fold_o;
            cw <= cw + 2'd1;
            if ((seed_steps_q == 2'd0) ? (cw == 2'd3) : (cw == (seed_steps_q - 2'd1)))
            begin
              cw <= 2'd0;
              // tiny packet: one burst covered everything
              m <= (fetched >= (32'd40 + cb)) ? M_PCRC_RD : M_PAY_REQ;
            end
          end
        end

        M_PAY_REQ: begin
          hps_req_v <= 1'b1;
          hps_addr <= f_addr + fetched;
          hps_len <= burst_len(need_total - fetched);
          wr_off <= fetched;
          burst_end <= fetched + 32'(burst_len(need_total - fetched));
          m <= M_PAY_WAIT;
        end

        M_PAY_WAIT: begin
          if (hps_rsp_i.err) begin
            done_v <= 1'b1; done_slot <= f_slot; done_status <= ST_BRIDGE_ERR;
            done_bytes <= 32'd36; done_cmds <= 32'd0;
            live_drops <= sat_add(live_drops, 64'd1);
            m <= M_IDLE;
          end else if (hps_rsp_i.beat_valid) begin
            for (int i = 0; i < 8; i++) begin
              if ((wr_off + 32'(i)) < 32'd64) begin
                hdr_win[wr_off + 32'(i)] <= hps_rsp_i.data[8*i +: 8];
              end
            end
            // payload CRC over this beat's bytes inside [36, 40+cb), in one
            // fold rather than eight chained steps. beat_start shifts the beat
            // so the first enabled byte sits at position zero, which is where
            // the fold takes its bytes from; beat_n is how many are inside.
            crc_pay_r <= fold_o;
            wr_off <= wr_off + 32'd8;
            fetched <= fetched + 32'd8;
            if (hps_rsp_i.last || ((wr_off + 32'd8) >= burst_end)) begin
              if ((fetched + 32'd8) >= need_total) m <= M_PCRC_RD;
              else m <= M_PAY_REQ;
            end
          end
        end

        // THE PAYLOAD CRC, CHECKED ONCE, IN A STATE OF ITS OWN.
        //
        // This comparison used to sit at the top of M_WALK, so it was
        // re-evaluated on EVERY walk cycle -- once per record -- for a value
        // that only matters before the first one. Two costs, and the second is
        // the one that mattered:
        //
        //   * `hget32(36 + cb)` is FOUR byte-wide 4,096:1 muxes into
        //     `slot_buf`, live for the whole walk;
        //   * being in the same state as the record walk's own `hget16` reads
        //     made the two SIMULTANEOUS in hardware, even though only one is
        //     ever used. Three variable-offset readers that are logically
        //     exclusive could not share a port while two of them shared a
        //     state.
        //
        // The fitter measured the consequence: 95,328 combinational nodes
        // against a device holding 83,820, from this block alone.
        //
        // Splitting it out changes no behaviour -- the check happens before
        // any record is walked either way, and on the same data -- and it is
        // the precondition for the readers ever sharing a port.
        // one cycle presenting the address; the word lands in ram_q
        M_PCRC_RD: m <= M_PCRC;

        M_PCRC: begin
          // 36+cb is 4 mod 8 whenever cb is a multiple of 16, which the header
          // ladder proved before this state is reachable; the other alignment
          // is kept rather than assumed away.
          if ({~crc_pay_r} != ((((32'd36 + cb) & 32'd4) != 32'd0) ? ram_q[63:32]
                                                       : ram_q[31:0])) begin
            done_v <= 1'b1; done_slot <= f_slot;
            done_status <= ST_BAD_PAYLOAD_CRC;
            done_bytes <= need_total; done_cmds <= 32'd0;
            live_drops <= sat_add(live_drops, 64'd1);
            m <= M_IDLE;
          end else begin
            m <= M_WALK_RD;
          end
        end

        // the walk reads one word per record: present, decode, then evaluate
        M_WALK_RD: m <= M_WALK_DEC;

        // The staging word is in ram_q now. Pull the two fields out and look
        // the opcode's lawful size up -- and stop there. The ladder runs next
        // cycle, from these registers.
        M_WALK_DEC: begin
          begin
            logic [15:0] op_x;
            // both 16-bit fields sit in the SAME word -- one read serves the
            // pair, which is why the walk did not need a second access
            op_x = (((32'd36 + walk_off) & 32'd4) != 32'd0) ? ram_q[47:32] : ram_q[15:0];
            op_q  <= op_x;
            rb_q  <= (((32'd36 + walk_off) & 32'd4) != 32'd0) ? ram_q[63:48] : ram_q[31:16];
            rsz_q <= rec_size(op_x);
          end
          m <= M_WALK;
        end

        M_WALK: begin
          // the record walk (5/9/10); the payload CRC passed in M_PCRC
          if (walk_off >= cb) begin
            if (walk_cnt != cc) begin
              done_v <= 1'b1; done_slot <= f_slot;
              done_status <= ST_COUNT_MISMATCH;
              done_bytes <= need_total; done_cmds <= walk_cnt;
              live_drops <= sat_add(live_drops, 64'd1);
              m <= M_IDLE;
            end else begin
              pay_gate <= 1'b1;  // fully verified: release the packet
              done_v <= 1'b1; done_slot <= f_slot; done_status <= ST_OK;
              done_bytes <= need_total; done_cmds <= walk_cnt;
              live_cmds <= sat_add(live_cmds, {32'd0, walk_cnt});
              pkt_len_r <= need_total;
              rd_off <= 32'd0;
              m <= M_STREAM_RD;   // fetch word 0 before offering a byte
            end
          end else begin
            logic [15:0] op_v;
            logic [15:0] rb_v;
            logic [7:0]  wst_v;
            logic        wok_v;
            // from REGISTERS now: M_WALK_DEC did the extraction and the
            // rec_size lookup, so this ladder no longer starts at the RAM
            op_v = op_q;
            rb_v = rb_q;
            wok_v = 1'b1;
            wst_v = ST_OK;
            if ((rb_v & 16'h000F) != 16'd0 || rb_v < 16'd16) begin
              wok_v = 1'b0; wst_v = ST_BAD_LENGTH;
            end else if (rsz_q == 16'd0) begin
              wok_v = 1'b0; wst_v = ST_UNKNOWN_OPCODE;
            end else if (rsz_q != rb_v) begin
              wok_v = 1'b0; wst_v = ST_BAD_LENGTH;
            end else if ((walk_off + 32'(rb_v)) > cb) begin
              wok_v = 1'b0; wst_v = ST_TRUNCATED;
            end else if ((op_v >= zhao_abi_pkg::ZHAO_DEBUG_OPCODE_LO)
                         && (op_v <= zhao_abi_pkg::ZHAO_DEBUG_OPCODE_HI)
                         && !h_debug) begin
              wok_v = 1'b0; wst_v = ST_DEBUG_FLAG;
            end
            if (!wok_v) begin
              done_v <= 1'b1; done_slot <= f_slot; done_status <= wst_v;
              done_bytes <= need_total; done_cmds <= walk_cnt;
              live_drops <= sat_add(live_drops, 64'd1);
              m <= M_IDLE;
            end else begin
              walk_off <= walk_off + 32'(rb_v);
              walk_cnt <= walk_cnt + 32'd1;
              m <= M_WALK_RD;
            end
          end
        end

        M_STREAM_RD: m <= M_STREAM_LD;

        M_STREAM_LD: begin
          stream_w <= ram_q;      // word 0 is in
          pkt_v <= 1'b1;          // the verified stream starts now
          m <= M_STREAM;
        end

        M_STREAM: begin
          if (pkt_ready_i) begin
            if ((rd_off + 32'd1) >= pkt_len_r) begin
              pkt_v <= 1'b0;
              rd_off <= 32'd0;
              m <= M_IDLE;
            end else begin
              rd_off <= rd_off + 32'd1;
              // crossing into the next word: ram_q has held it for seven
              // cycles by now
              if (rd_off[2:0] == 3'd7) stream_w <= ram_q;
            end
          end
        end


        default: m <= M_IDLE;
      endcase

      // D9 shadow latch at the frame boundary
      if (frame_tick_i.pulse) begin
        sh_cmds <= live_cmds;
        sh_bytes <= live_bytes;
        sh_drops <= live_drops;
        snap_v <= 1'b1;
      end
    end
  end

  // ------------------------------------------------------- snapshots -------
  always_comb begin
    snap_cmds_o.valid      = snap_v;
    snap_cmds_o.counter_id = zhao_pkg::ZHAO_CNT_COMMANDS;
    snap_cmds_o.value      = sh_cmds;
    snap_bytes_o.valid      = snap_v;
    snap_bytes_o.counter_id = zhao_pkg::ZHAO_CNT_HPS_BYTES;
    snap_bytes_o.value      = sh_bytes;
    snap_drops_o.valid      = snap_v;
    snap_drops_o.counter_id = zhao_pkg::ZHAO_CNT_DEADLINE_FAULTS;
    snap_drops_o.value      = sh_drops;
  end

  // ------------------------------------------------------- formal ---------
  // cmd_dma_crc_gate (plan 4 / capture_format.md 3.2 / memory_rules.md 4.3):
  //   (a) no verified-packet byte is offered before the header CRC passed
  //       (hdr_gate) AND the payload CRC + walk passed (pay_gate)
  //   (b) no VRAM write is offered before the blit payload CRC passed
  //   (c) reset leaves no partial handoff (all valids low after reset)
  // The formal harness (cmd_dma_crc_gate_harness.sv) shrinks the staging
  // buffers for tractability; the committed defaults are verified by the
  // ctest lane (cmd_dma_directed).
`ifdef FORMAL
  logic f_past_valid = 1'b0;
  always_ff @(posedge clk) f_past_valid <= 1'b1;
  always_ff @(posedge clk) begin
    if (f_past_valid && $past(rst_n)) assume(rst_n);
  end
  always_ff @(posedge clk) begin
    if (f_past_valid && !$past(rst_n)) begin
      assert(!pkt_valid_o);
      assert(!hps_req_o.valid);
    end
  end
  always_ff @(posedge clk) begin
    if (rst_n) begin
      if (pkt_valid_o) begin
        assert(hdr_gate && pay_gate);
      end
      // The seed loop's bound. One burst, capped at 64 bytes, so the
      // catch-up can never reach past byte 64 -- which is what lets the
      // loop stop there instead of at 192.
      if (m == M_HDR_CHK) begin
        assert(fetched <= 32'd64);
      end
      // The seed length is one of THREE values, and the fold widths depend on
      // it: M_SEED runs four groups of eight for 28 bytes with a four-byte
      // tail, two for 16, none for 0. If this were ever a fourth number the
      // walk would fold the wrong count and the payload CRC would be wrong on
      // a packet that is otherwise lawful.
      a_seed_bytes_lawful:
        if (m == M_SEED) assert((seed_bytes_q == 6'd0)
                             || (seed_bytes_q == 6'd16)
                             || (seed_bytes_q == 6'd28));
      // and the alignment the fold widths rest on
      a_payload_end_aligned:
        if (m == M_SEED) assert(payload_end_q[2:0] == 3'b100);
    end
  end

  // ---- non-vacuity covers (V16: covers must prove the antecedents) --------
  // The assertion above is an implication, and a model that cannot raise
  // pkt_valid_o satisfies it while proving nothing (the MEM.GUARD failure
  // shape), so the antecedent is covered explicitly.
  //
  // PROPERTY (b) AND ITS COVERS WENT WITH THE BLIT ENGINE (step 6). It said
  // no VRAM write is offered before the blit payload CRC passed, and this
  // module no longer offers VRAM writes at all -- it has no MEM.GUARD client.
  // The same law now lives on DEBUG.FRAMEBLIT, whose own formal harness is
  // tests/formal/debug_frameblit_safety.sby.
  //
  // Worth recording rather than quietly dropping: (b) was VACUOUS until the
  // harness gained FORMAL_BLIT_LEN, because the smallest lawful canvas is
  // 153,600 B and the blit gate never opened at a tractable BMC depth. The
  // property that had to be rescued from vacuity is the one being deleted.
  always_ff @(posedge clk) begin
    if (f_past_valid && rst_n) begin
      c_pkt:      cover (pkt_valid_o);                 // (a) antecedent
      c_gates:    cover (hdr_gate && pay_gate);
      c_hdr_bad:  cover (dma_done_o
                         && (dma_status_o == ST_BAD_HEADER_CRC));
    end
  end
`endif

endmodule : zhao_cmd_dma
