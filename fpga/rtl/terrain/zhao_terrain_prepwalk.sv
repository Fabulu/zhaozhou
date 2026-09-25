// zhao_terrain_prepwalk.sv -- the ADMITTED-SET PREPARE WALKER: a REPLAYABLE
// reader of the frame's sealed patch list that never touches the compose spine.
//
// Law: reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt section 2 and section 7
//      design/contracts/TERRAIN.EDGERECON.md ("What still has no producer")
//      reports/OWNER-RULINGS-BUILDABILITY-20260902.md  T3, T5, T6, T7
//      fpga/rtl/prod/zhao_console_core.sv entry I21 item (e), step 1 and step 2
//
// ===========================================================================
// THE DEADLOCK THIS BLOCK EXISTS TO AVOID (blocker A, and it is REAL)
// ===========================================================================
// `TERRAIN.EDGERECON.md`'s correction says the admitted-set enumerator "EXISTS
// AND IS COMPOSED" -- `u_terrain_seq`'s issue port. It does, and it CANNOT be
// used that way, which packet EDGERECON2 measured and this block accepts:
//
//   * PREPARE must hold the WHOLE admitted set before EMIT opens, because the
//     symmetry law needs a FROZEN bank -- an edge answered from a decision iff
//     ok(P) && ok(N), evaluated against bits that cannot move between P's query
//     and N's query.
//   * The issue stream is ONE-SHOT. `is_valid_o == (st == S_ISSUE)`, the record
//     is overwritten on the next fetch, frame state is wiped on `fr_start_i`,
//     and architecture 2.5 rejects by name the persistent cache a replay needs.
//   * So a walker teed off that port must drink the stream as it flies -- and
//     THAT STREAM'S READY IS THE COMPOSE CACHE'S. `tis_ready = thr_j_ready &&
//     tce_can_start`, `tce_can_start = !tcc_fill_busy`, against a front holding
//     EXACTLY TWO lattices. Holding tessellation back until the last patch is
//     issued stalls the compose, drops `tce_can_start`, stalls the issue port,
//     and THE WALK NEVER REACHES THE LAST PATCH.
//
// It is a structural deadlock, not a tuning problem, and no depth of buffer
// fixes it: the buffer that would fix it is the persistent copy of the world
// list that 2.5 forbids.
//
// ===========================================================================
// THE ANSWER WAS ALREADY IN THE TREE, AND IT IS THE LIST'S OWN ADDRESS
// ===========================================================================
// The directive asks for "a REPLAYABLE sealed-list PREPARE reader independent
// of compose-cache backpressure". The replay mechanism exists, is proven, is
// composed, and is used TWICE PER FRAME ALREADY -- by `zhao_terrain_cmd`:
//
//     "The alternatives are to BUFFER the list or to READ IT TWICE. Buffering
//      is 32 bytes per record and T6 permits 256 composed patches, so the
//      visible set can be 8,192 bytes = 65,536 bits, which is seven M10K held
//      for one command. Reading twice costs 16 KB of HPS traffic per frame
//      against the 684 KB that T7's 32 whole pages already cost -- 2.4% more."
//                                       -- zhao_terrain_cmd.sv, its own header
//
// THE SEALED LIST IS AT A KNOWN ADDRESS, WITH A KNOWN LENGTH AND A KNOWN CRC.
// `SubmitTerrainSet` carries `list_off`, `list_bytes` and `list_crc32c`, and
// ruling T5 makes the list CAPTURE DATA -- immutable for the frame. That is
// the definition of replayable. This block is a THIRD reader of the same
// bytes, and its ready is its OWN consumer's, so the compose spine is not in
// its loop anywhere.
//
// WHY A SEPARATE BLOCK RATHER THAN A THIRD PASS INSIDE `zhao_terrain_cmd`:
// that block is COMPOSED. New output ports on it would dangle at
// `zhao_console_core`'s boundary until P4 wires them, which puts the
// completion register UP and buys a gap with a tie-off -- the trade R75
// endorses refusing and this packet's brief forbids by name. A new file is in
// nobody's closure and costs nothing until it is instantiated.
//
// WHAT IT COSTS, HONESTLY: one more pass over the list is +8 KiB of HPS
// traffic per frame at T6's 256 patches (256 x 32 B). That is HPS-BRIDGE
// traffic, a DIFFERENT socket from the local-SDRAM devstore reads, and it is
// budgeted separately in `tools/budget/sdram_bandwidth.py`. It is not the
// expensive half: the devstore reads are (see below).
//
// ===========================================================================
// WHAT THIS BLOCK IS NOT
// ===========================================================================
//   * NOT a second terrain engine. It decides no level -- `zhao_terrain_lod`
//     does, once, through the time-share. It holds no heights, no lattice, no
//     page bytes and no deviations of its own.
//   * NOT a duplicate world store. It holds ONE record at a time. The sealed
//     list stays in HPS memory where the command put it; that is the whole
//     point of re-reading it.
//   * NOT a second placement provider. `sp_cx_o`/`sp_cz_o` come from
//     `zhao_terrain_place_law_pkg`, the SAME functions `zhao_terrain_place`
//     calls, so PREPARE and EMIT are bit-identical BY CONSTRUCTION. See that
//     package's header for why the three candidates TERRAIN.EDGERECON.md lists
//     were all refused.
//   * NOT the sequencer. It does not claim slots, issue loads, evict, pin or
//     writeback. It LOOKS UP residency and skips a miss, because PREPARE must
//     not change any state EMIT then reads.
//
// ===========================================================================
// PREPARE MUST NOT COMMIT ANYTHING. THAT IS A CORRECTNESS RULE, NOT TIDINESS
// ===========================================================================
// The directive: "PREPARE must not commit hysteresis history or advance the
// state EMIT treats as last frame. Commit history once, for the completed/
// admitted result."
//
// The history writeback is NOT suppressed here, because it cannot be: this
// block does not drive it. `zhao_terrain_devstore`'s read FSM enters `R_HWR`
// from `R_STREAM` on `(h_any_q || h_valid_i)` and `h_any_q` is set ONLY inside
// `if (h_valid_i)` (`zhao_terrain_devstore.sv:859-865, :877`). So holding
// `h_valid` LOW for the whole PREPARE pass is exactly sufficient, and devstore
// needs no change at all -- its own comment already describes the path: "A
// patch whose LOD pass emitted NOTHING skips the write entirely."
//
// That gating belongs where LOD's history port is muxed, which is
// `zhao_terrain_lodshare`. It is recorded HERE because this block is what
// makes the second pass exist, and a reader who finds the second pass and not
// the suppression would ship a hysteresis corruption with every counter
// balancing.
//
// ===========================================================================
// THE FROZEN INPUTS, AND WHY THEY ARE INPUTS
// ===========================================================================
// `frz_pitch_log2_i` is the ISLAND DESCRIPTOR's pitch, which owner directive
// section 2 makes AUTHORITATIVE for every page of that island generation:
//
//     "the frame-sealed island descriptor's pitch_log2 is authoritative for
//      EVERY page belonging to that island generation. The page-header pitch
//      remains a checked redundant value, not an independent source of
//      placement law. This resolves D-2 and removes the need for the proposed
//      per-resident-slot pitch table."
//
// That ruling is what DELETED packet P1. It is taken here as a held frame
// input rather than looked up per patch, because a per-patch lookup would be
// exactly the per-slot table the directive removed.
//
// `frz_tok_i` is the frame's freeze witness -- residency/bake generation. It
// is NOT used to compute anything. It is CAPTURED at the start of the walk and
// republished on `frz_tok_o` so that the EMIT side can compare the value the
// bank was built under against the value live at query time. See
// `restart_req_o` below, and `zhao_terrain_lodshare`, which owns the compare.
//
// Conservative SystemVerilog subset only (charter section 2).
`default_nettype none

module zhao_terrain_prepwalk
  // ONE `import` STATEMENT, NOT TWO. Quartus 17.0 takes a single item list in
  // a module header and rejects a second `import` line outright -- while
  // `verilator --lint-only -Wall` accepts both forms with 0 diagnostics. This
  // file was written with two and `tools/quartus/check_quartus17_syntax.py`
  // caught it; because `run_block_map.ps1` compiles every .sv under fpga/rtl,
  // the two-line form would have failed EVERY map in the tree, not just this
  // block's. A clean lint settles one tool's opinion and nothing else.
  import zhao_pkg::*, zhao_terrain_place_law_pkg::*;
#(
    // T5's record, and `zref::swstream::kRecordBytes`. A parameter so the
    // elaboration checks have something to check, NOT a knob: a different
    // record is a different ABI.
    parameter int unsigned REC_BYTES   = 32,
    parameter int unsigned BURST_BYTES = 64,
    parameter int unsigned MAX_PATCHES = 1024,
    // T9/T10: 256 sets x 4 ways.
    parameter int unsigned SLOTW  = 10,
    parameter int unsigned GENW   = 8,
    parameter int unsigned DEVW   = 24,
    parameter int unsigned MORPHW = 17,
    // The subpatch grid, the same two constants `zhao_terrain_spdesc` holds.
    // Both are here so that a reader comparing the two files sees one number
    // in each rather than one number and a derivation.
    parameter int unsigned SUBPATCHES = 16,
    parameter int unsigned SUB_EDGE   = 8,
    parameter int unsigned CENTRE_OFF = 4,
    parameter int unsigned LAT_W      = 33,
    parameter int unsigned LAT_H      = 33,
    // The freeze witness. Width is the caller's business; it is compared, never
    // interpreted.
    parameter int unsigned TOKW = 32
) (
    input var logic clk,
    input var logic rst_n,

    // ---- configuration -------------------------------------------------------
    input var zhao_client_e cfg_hps_client_i,
    input var logic [31:0]  cfg_epoch_i,
    input var logic [31:0]  cfg_arena_base_i,
    input var logic [31:0]  cfg_arena_bytes_i,

    // ---- the PREPARE job: SubmitTerrainSet's own fields ----------------------
    // The SAME fields `zhao_terrain_cmd`'s job port takes. P4 forks the command
    // to both; this block walks first, that block walks after.
    input  var logic        j_valid_i,
    output var logic        j_ready_o,
    input  var logic [31:0] j_epoch_i,
    input  var logic [31:0] j_list_off_i,
    input  var logic [31:0] j_list_bytes_i,
    input  var logic [31:0] j_list_crc_i,
    input  var logic [15:0] j_patch_count_i,

    // ---- the frame's FROZEN inputs -------------------------------------------
    input  var logic signed [7:0]   frz_pitch_log2_i,  // island-authoritative
    input  var logic [TOKW-1:0]     frz_tok_i,         // residency/bake witness
    output var logic [TOKW-1:0]     frz_tok_o,         // what this walk used

    // ---- MEM.HPS.BRIDGE read client (the sealed list, re-read) ---------------
    output var zhao_hps_burst_req_t hps_req_o,
    input  var logic                hps_req_grant_i,
    input  var zhao_hps_burst_rsp_t hps_rsp_i,

    // ---- TERRAIN.RESIDENCY lookup (master) -----------------------------------
    // LOOKUP ONLY. No claim, no pin, no load, no eviction: PREPARE may not
    // change one bit of the state EMIT reads.
    output var logic               lu_valid_o,
    input  var logic               lu_ready_i,
    output var logic [31:0]        lu_epoch_o,
    output var logic [31:0]        lu_island_o,
    output var logic signed [15:0] lu_ix_o,
    output var logic signed [15:0] lu_iz_o,
    input  var logic               lu_ans_valid_i,
    input  var logic               lu_ans_hit_i,
    input  var logic [SLOTW-1:0]   lu_ans_slot_i,
    input  var logic [GENW-1:0]    lu_ans_gen_i,

    // ---- TERRAIN.DEVSTORE read (master) --------------------------------------
    output var logic               r_start_o,
    output var logic [SLOTW-1:0]   r_slot_o,
    input  var logic               r_ready_i,
    input  var logic               r_valid_i,
    output var logic               r_ready_o,
    input  var logic [3:0]         r_sp_i,
    input  var logic [DEVW-1:0]    r_dev1_i,
    input  var logic [DEVW-1:0]    r_dev2_i,
    input  var logic [DEVW-1:0]    r_dev3_i,
    input  var logic signed [15:0] r_cy_i,
    input  var logic [1:0]         r_prev_level_i,
    input  var logic [MORPHW-1:0]  r_prev_morph_i,
    input  var logic [7:0]         r_hold_i,
    input  var logic               r_fresh_i,

    // ---- the PREPARE descriptor stream, TERRAIN.LOD's port field for field ---
    // Ten payload fields plus the handshake, exactly `zhao_terrain_spdesc`'s
    // `sp_*` group, so the time-share muxes two identical shapes.
    output var logic               sp_valid_o,
    input  var logic               sp_ready_i,
    output var logic signed [31:0] sp_cx_o,
    output var logic signed [31:0] sp_cy_o,
    output var logic signed [31:0] sp_cz_o,
    output var logic [DEVW-1:0]    sp_dev1_o,
    output var logic [DEVW-1:0]    sp_dev2_o,
    output var logic [DEVW-1:0]    sp_dev3_o,
    output var logic [1:0]         sp_prev_level_o,
    output var logic [MORPHW-1:0]  sp_prev_morph_o,
    output var logic [7:0]         sp_hold_o,
    output var logic [15:0]        sp_src_id_o,

    // METADATA TRAVELS WITH THE RECORD. `zhao_terrain_lod` carries `src_id`
    // and nothing else, and TERRAIN.EDGERECON's file port needs the PATCH
    // COORDINATE. Handing the coordinate over a side channel that the
    // descriptor does not ride with is precisely the record-swapping shape
    // CLAUDE.md documents: a stall pairs one patch's levels with another
    // patch's coordinate, and every counter balances. These two ride WITH the
    // beat and `zhao_terrain_lodshare` queues them against it.
    output var logic signed [15:0] sp_ix_o,
    output var logic signed [15:0] sp_iz_o,
    // The RESIDENCY GENERATION this walk looked the patch up under. PREPARE
    // banks a decision made from slot S at generation G; if that slot is
    // recycled before EMIT reads it, the bank describes a page that is no
    // longer there. Carrying G with the descriptor lets the consumer compare
    // it against the generation live at EMIT -- two values loaded by two
    // DIFFERENT enables, which is exactly the property CLAUDE.md's metadata-
    // bank defect lacked and the reason that block's counter could never fire.
    output var logic [GENW-1:0]    sp_gen_o,
    // The subpatch this descriptor is for, 0..15, as devstore streamed it.
    // Carried so the consumer can check the order rather than assume it.
    output var logic [3:0]         sp_sub_o,

    // ---- phase ---------------------------------------------------------------
    // `prep_begin_o` is TERRAIN.EDGERECON's `frame_begin_i`: it SWEEPS the bank
    // before anything is filed, which is the ruling's "no unvalidated
    // previous-frame shortcut" enforced structurally.
    output var logic prep_begin_o,
    // `prep_gate_i` is that block's "the sweep is done and PREPARE is open"
    // (`phase_o == 1`). Waiting on it rather than counting 257 clocks is the
    // same discipline TERRAIN.EDGERECON's own directed suite uses: a test that
    // counted clocks would go green on a sweep that never ran.
    input  var logic prep_gate_i,
    // `prep_done_o` is that block's `prepare_done_i`: the bank FREEZES and
    // EMIT opens. One cycle, after the last admitted patch's last descriptor.
    output var logic prep_done_o,

    // ---- the freeze verdict --------------------------------------------------
    // HIGH from the end of the walk until the next job, and it means: the walk
    // completed and what it banked describes THIS frame. LOW means the caller
    // must treat the bank as unusable and fall back -- which is the console's
    // existing behaviour and therefore always a safe answer.
    output var logic prep_valid_o,
    // A restart REQUEST, not a restart. The directive: "A bake or residency
    // change either waits for the pinned lifetime or produces a DETECTED
    // restart; never combine half of one generation with half of another."
    // This block detects; the caller decides.
    output var logic restart_req_o,

    output var logic busy_o,
    output var logic idle_o,

    // ---- counters (events, except where the name says clocks) ----------------
    output var logic [31:0] walks_started_o,
    output var logic [31:0] walks_completed_o,
    output var logic [31:0] records_walked_o,
    output var logic [31:0] patches_prepared_o,
    // A record whose patch is not resident. NOT an error: architecture 2.6
    // skips a miss rather than waiting on it, and its neighbours fall back
    // symmetrically, which is the console's existing behaviour on that seam.
    output var logic [31:0] skipped_not_resident_o,
    output var logic [31:0] descriptors_emitted_o,
    // The patch was resident but devstore had never written it. Its deviations
    // are the neutral triple, so the ladder decides from the centre height
    // alone -- legal, and worth counting because a frame full of these means
    // the bake has not caught up.
    output var logic [31:0] patches_unfresh_o,
    // THE REPLAY'S OWN CHECK. The list is capture data and must be immutable
    // for the frame; if the bytes this pass folds do not match the CRC the
    // command declared, something rewrote the arena underneath the frame.
    output var logic [31:0] list_crc_mismatch_o,
    // The frozen witness moved while the walk was running.
    output var logic [31:0] freeze_broken_o,
    // The island pitch is not one of the four spec 1.3 admits. Every placement
    // this walk would compute is refused, so the walk is abandoned rather than
    // banking coordinates from a default.
    output var logic [31:0] pitch_illegal_o,
    // The job was refused before a single byte was read: a stale resource
    // epoch, a count above T6's ceiling, a length that is not 32 x count, an
    // unaligned list, or a list running past the end of the arena. The SAME
    // pre-checks `zhao_terrain_cmd` applies, because a walk that accepted a
    // command that block refuses would prepare a frame nobody emits.
    output var logic [31:0] jobs_refused_o,
    // A placement that does not fit s32 at this pitch -- `units_fits` refusing.
    // The patch is skipped; a wrapped placement puts it on the far side of the
    // world and reads as legitimate geometry downstream.
    output var logic [31:0] place_range_o,
    output var logic [31:0] bridge_errs_o,
    // devstore streamed its sixteen records out of order. The consumer pairs
    // by position, so an out-of-order record would bank a level in the wrong
    // lane -- a crack whose cause is a stream.
    output var logic [31:0] sub_order_bad_o,
    // CLOCKS, and the name says so because the unit differs from every counter
    // above. This is the instrument `spec/memory_rules.md:396-402` already
    // names for exactly this question: PREPARE doubles the frame-critical read
    // demand on the one client ruling T3 starves first, so what matters is how
    // long the reads WAIT, not how many bursts they are.
    output var logic [31:0] store_wait_clocks_o,
    output var logic [31:0] list_wait_clocks_o,
    output var logic [31:0] list_bytes_read_o,
    output var logic [31:0] list_refetch_bytes_o
);

  localparam int unsigned BEATS_PER_REC = REC_BYTES / 8;    // 4
  localparam int unsigned BEATS_PER_BST = BURST_BYTES / 8;  // 8

`ifndef SYNTHESIS
  // Quartus 17.0 rejects a bare module-scope `if`; it must sit inside an
  // `initial begin ... end`. And `--lint-only` does not RUN this block, so a
  // clean lint says nothing about it (CLAUDE.md).
  initial begin
    if (REC_BYTES != 32)
      $fatal(1, "prepwalk: T5's record is 32 bytes; REC_BYTES=%0d is a different ABI", REC_BYTES);
    if (BEATS_PER_REC != 4)
      $fatal(1, "prepwalk: a record must be four 64-bit beats, not %0d", BEATS_PER_REC);
    if ((BURST_BYTES % REC_BYTES) != 0)
      $fatal(1, "prepwalk: a burst must hold a whole number of records");
    if (SUBPATCHES != 16)
      $fatal(1, "prepwalk: the subpatch grid is 4x4; SUBPATCHES=%0d", SUBPATCHES);
    // The same guard `zhao_terrain_spdesc` carries: a CENTRE_OFF that walks off
    // the lattice edge places a centre outside the patch it belongs to.
    if ((3 * SUB_EDGE + CENTRE_OFF) >= LAT_W)
      $fatal(1, "prepwalk: subpatch centre %0d walks off a %0d lattice",
             3 * SUB_EDGE + CENTRE_OFF, LAT_W);
    if ((3 * SUB_EDGE + CENTRE_OFF) >= LAT_H)
      $fatal(1, "prepwalk: subpatch centre %0d walks off a %0d lattice",
             3 * SUB_EDGE + CENTRE_OFF, LAT_H);
  end
`endif

  // Quartus 17.0 is not reliable about a width cast on a localparam expression
  // used in arithmetic, so the two subpatch constants are pre-narrowed, exactly
  // as `zhao_terrain_spdesc` pre-narrows them.
  localparam logic [5:0] SUB_EDGE_6   = 6'(SUB_EDGE);
  localparam logic [5:0] CENTRE_OFF_6 = 6'(CENTRE_OFF);

  // ---- the job, captured at acceptance -------------------------------------
  logic [31:0] job_off_q, job_bytes_q, job_crc_q, job_epoch_q;
  logic [15:0] job_count_q;

  // ---- the walk ------------------------------------------------------------
  logic [31:0] byte_q;      // bytes of the list consumed so far
  logic [31:0] crc_q;       // the re-read's own fold
  logic [15:0] walked_q;    // records consumed
  logic [ 1:0] rbeat_q;     // which beat of the current record
  logic [ 2:0] skip_q;      // beats of this burst below the wanted byte
  // Beat 0 only. Beat 1 is the page's HPS address and beats 2 and 3 carry the
  // page CRC, flags, view mask, priority and source id. PREPARE does not LOAD,
  // so it needs neither middle beat; the source id is taken straight off the
  // wire on beat 3, because that is the beat the record completes on.
  logic [63:0] rec_b0_q;
  logic [$clog2(BEATS_PER_BST+1)-1:0] beat_q;

  // ---- the record under consideration --------------------------------------
  logic [31:0]        cur_island_q;
  logic signed [15:0] cur_ix_q, cur_iz_q;
  logic [15:0]        cur_src_q;
  logic [SLOTW-1:0]   cur_slot_q;

  // ---- the descriptor being emitted ----------------------------------------
  // THERE IS NO PAYLOAD REGISTER HERE, DELIBERATELY. The first version of this
  // block staged devstore's record into `d_*_q` and presented the STAGED copy
  // while `sp_valid_o` tracked the LIVE `r_valid_i` -- so the descriptor on the
  // wire belonged to the PREVIOUS beat while its handshake belonged to this
  // one. That is the record-swapping shape CLAUDE.md documents, authored here
  // by accident, and it would have banked every subpatch's deviations one lane
  // late with every counter balancing.
  //
  // The repair is to hold no second copy at all: devstore's record goes
  // straight out, and the only thing this block ADDS is the placement, which is
  // a pure function of that record's own `r_sp_i`. One beat in, one beat out,
  // nothing held, nothing to slip.
  logic [GENW-1:0]    cur_gen_q;
  logic [4:0]         d_count_q;    // descriptors accepted for this patch, 0..16

  // ---- the freeze ----------------------------------------------------------
  logic            begin_q;       // the one-cycle sweep request
  logic [TOKW-1:0] tok_q;
  logic            prep_valid_q;
  logic            restart_q;

  typedef enum logic [3:0] {
    P_IDLE,
    P_SWEEP,    // frame_begin pulsed; waiting for the reconciler to open
    P_NEXT,     // dispatch the next record, or finish
    P_REQ,      // offer a burst to the bridge
    P_BEAT,     // consume beats, assemble records
    P_LOOKUP,   // ask residency where this patch lives
    P_LUWAIT,
    P_START,    // pulse devstore's r_start
    P_STREAM,   // pair devstore's sixteen records with sixteen placements
    P_FIN,      // the list is walked: check the CRC
    P_DONE      // pulse prepare_done
  } state_e;

  state_e state_q;

  // ---- CRC folding, on CONSUMED beats only ---------------------------------
  // The skipped lead of a resumed burst is NOT folded and NOT counted in
  // `byte_q`, so every byte of the list is folded exactly once and in list
  // order. That is the property that makes this a real check rather than a
  // number that happens to match.
  logic [31:0] fold_out;
  zhao_crc32c_fold u_fold (
      .c_i(crc_q),
      .d_i(hps_rsp_i.data),
      .n_i(4'd8),
      .c_o(fold_out)
  );

  // ==========================================================================
  // THE BRIDGE REQUEST -- the 64-byte-alignment rule, and why it is copied
  // ==========================================================================
  // `zhao_hps_bridge` rejects a request whose address is not 64-byte aligned
  // (`malformed = (len == 0) || (len > 64) || (addr[5:0] != 0)`), answering
  // with `err | last` and NOTHING ISSUED. A T5 record boundary is 32 bytes, so
  // a walk that resumes at a record boundary asks from a 32-byte-aligned
  // address every other time. `zhao_terrain_cmd` shipped exactly that defect
  // and it was found only by composing against the REAL bridge.
  //
  // So: ask from the 64-byte boundary at or below the wanted byte, and discard
  // `skip_q` beats on arrival. The cost is MEASURED on
  // `list_refetch_bytes_o` rather than argued about.
  logic [31:0] abs_c;
  logic [ 5:0] skip_bytes_c;
  logic [31:0] left_c, want_bytes_c;
  logic [ 6:0] this_bytes_c;

  assign abs_c        = cfg_arena_base_i + job_off_q + byte_q;
  assign skip_bytes_c = abs_c[5:0];
  assign left_c       = job_bytes_q - byte_q;
  assign want_bytes_c = left_c + {26'd0, skip_bytes_c};
  assign this_bytes_c = (want_bytes_c > 32'(BURST_BYTES)) ? 7'(BURST_BYTES)
                                                         : 7'(want_bytes_c);

  always_comb begin
    hps_req_o        = '0;
    hps_req_o.valid  = (state_q == P_REQ);
    hps_req_o.write  = 1'b0;
    hps_req_o.client = cfg_hps_client_i;
    hps_req_o.addr   = abs_c - {26'd0, skip_bytes_c};
    hps_req_o.len    = this_bytes_c;
  end

  // ==========================================================================
  // PLACEMENT -- the package's functions, never a local copy
  // ==========================================================================
  // The subpatch centre is `sp[1:0] * SUB_EDGE + CENTRE_OFF` in i and
  // `sp[3:2] * SUB_EDGE + CENTRE_OFF` in j: `zhao_terrain_spdesc.sv:387-388`'s
  // own expression, so the two passes ask about the same lattice vertex.
  wire [5:0] centre_vi_c = 6'({4'b0, r_sp_i[1:0]} * SUB_EDGE_6 + CENTRE_OFF_6);
  wire [5:0] centre_vj_c = 6'({4'b0, r_sp_i[3:2]} * SUB_EDGE_6 + CENTRE_OFF_6);

  wire signed [UNITS_W-1:0] units_x_c = units_of(cur_ix_q, centre_vi_c);
  wire signed [UNITS_W-1:0] units_z_c = units_of(cur_iz_q, centre_vj_c);

  wire pitch_ok_c = pitch_legal(frz_pitch_log2_i);

  // ---- the pre-checks, in `zhao_terrain_cmd`'s own order -------------------
  wire [63:0] end_off_c = {32'd0, j_list_off_i} + {32'd0, j_list_bytes_i};
  wire job_bad_c =
        (j_epoch_i != cfg_epoch_i)
     || (j_patch_count_i == 16'd0)
     || (32'({16'd0, j_patch_count_i}) > 32'(MAX_PATCHES))
     || (j_list_bytes_i != ({16'd0, j_patch_count_i} << 5))
     || (j_list_off_i[2:0] != 3'd0)
     || (end_off_c > {32'd0, cfg_arena_bytes_i});

  // The FAR corner is the widest value this patch will ever place, so testing
  // it is what makes the whole patch safe rather than just this subpatch.
  // Identical reasoning to `zhao_terrain_place`'s `range_ok_c`.
  wire range_ok_c =
        units_fits(units_of(cur_ix_q, 6'd0),          frz_pitch_log2_i)
     && units_fits(units_of(cur_iz_q, 6'd0),          frz_pitch_log2_i)
     && units_fits(units_of(cur_ix_q, 6'(LAT_W - 1)), frz_pitch_log2_i)
     && units_fits(units_of(cur_iz_q, 6'(LAT_H - 1)), frz_pitch_log2_i);

  // ---- the descriptor on the wire ------------------------------------------
  assign sp_valid_o      = (state_q == P_STREAM) && r_valid_i;
  assign sp_cx_o         = place32(units_x_c, frz_pitch_log2_i);
  assign sp_cz_o         = place32(units_z_c, frz_pitch_log2_i);
  // height16 -> fx16, sign-extend then shift: `zhao_terrain_spdesc.sv:422`.
  assign sp_cy_o         = {{8{r_cy_i[15]}}, r_cy_i, 8'b0};
  assign sp_dev1_o       = r_dev1_i;
  assign sp_dev2_o       = r_dev2_i;
  assign sp_dev3_o       = r_dev3_i;
  assign sp_prev_level_o = r_prev_level_i;
  assign sp_prev_morph_o = r_prev_morph_i;
  assign sp_hold_o       = r_hold_i;
  assign sp_src_id_o     = cur_src_q;
  assign sp_ix_o         = cur_ix_q;
  assign sp_iz_o         = cur_iz_q;
  assign sp_gen_o        = cur_gen_q;
  assign sp_sub_o        = r_sp_i;

  // devstore's record is taken on the cycle the descriptor is taken from us,
  // so one record in is one descriptor out and the stream cannot run ahead.
  assign r_ready_o = (state_q == P_STREAM) && sp_ready_i;

  assign r_start_o = (state_q == P_START) && r_ready_i;
  assign r_slot_o  = cur_slot_q;

  assign lu_valid_o  = (state_q == P_LOOKUP);
  assign lu_epoch_o  = job_epoch_q;
  assign lu_island_o = cur_island_q;
  assign lu_ix_o     = cur_ix_q;
  assign lu_iz_o     = cur_iz_q;

  // A ONE-CYCLE PULSE, and the first version of this block had it as a LEVEL
  // held for as long as `prep_gate_i` was low. That is a REAL defect, not a
  // style point: `zhao_terrain_edgerecon`'s `frame_begin_i` is documented as a
  // pulse, and "frame_begin_i during a sweep RESTARTS it rather than queueing".
  // A level would therefore have restarted the 257-clock sweep on every cycle
  // it was asserted, so the sweep would never complete, `prep_gate_i` would
  // never rise, and the two blocks would hold each other still for ever --
  // each waiting for the other, with every counter reading zero.
  assign prep_begin_o  = begin_q;
  assign prep_done_o   = (state_q == P_DONE);
  assign prep_valid_o  = prep_valid_q;
  assign restart_req_o = restart_q;
  assign frz_tok_o     = tok_q;

  assign j_ready_o = (state_q == P_IDLE);
  assign idle_o    = (state_q == P_IDLE);
  assign busy_o    = (state_q != P_IDLE);

  // The freeze witness is watched for the WHOLE walk, and the comparison's two
  // sides are clocked by DIFFERENT enables on purpose: `tok_q` is loaded once,
  // by the accept of the job, and `frz_tok_i` is the live value. CLAUDE.md's
  // metadata-bank defect is a detector whose two operands were loaded by ONE
  // enable and which therefore could never fire; this one can, and
  // `terrain_prepwalk_directed` case 7 fires it.
  wire tok_moved_c = busy_o && (frz_tok_i != tok_q);

  logic [3:0] next_sub_c;
  assign next_sub_c = d_count_q[3:0];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q      <= P_IDLE;
      byte_q       <= '0;
      crc_q        <= 32'hFFFF_FFFF;
      walked_q     <= '0;
      rbeat_q      <= '0;
      skip_q       <= '0;
      beat_q       <= '0;
      rec_b0_q     <= '0;
      job_off_q    <= '0;
      job_bytes_q  <= '0;
      job_crc_q    <= '0;
      job_epoch_q  <= '0;
      job_count_q  <= '0;
      cur_island_q <= '0;
      cur_ix_q     <= '0;
      cur_iz_q     <= '0;
      cur_src_q    <= '0;
      cur_slot_q   <= '0;
      cur_gen_q    <= '0;
      d_count_q    <= '0;
      begin_q      <= 1'b0;
      tok_q        <= '0;
      prep_valid_q <= 1'b0;
      restart_q    <= 1'b0;

      walks_started_o        <= '0;
      walks_completed_o      <= '0;
      records_walked_o       <= '0;
      patches_prepared_o     <= '0;
      skipped_not_resident_o <= '0;
      descriptors_emitted_o  <= '0;
      patches_unfresh_o      <= '0;
      list_crc_mismatch_o    <= '0;
      freeze_broken_o        <= '0;
      pitch_illegal_o        <= '0;
      jobs_refused_o         <= '0;
      place_range_o          <= '0;
      bridge_errs_o          <= '0;
      sub_order_bad_o        <= '0;
      store_wait_clocks_o    <= '0;
      list_wait_clocks_o     <= '0;
      list_bytes_read_o      <= '0;
      list_refetch_bytes_o   <= '0;
    end else begin
      // The sweep request is ONE cycle wide, always. Raised on the transition
      // into P_SWEEP below, dropped here on the cycle after.
      begin_q <= 1'b0;

      // ---- the two CLOCK counters, and they are the budget ------------------
      // `store_wait_clocks_o` is what P2 was told to budget instead of bursts:
      // "Case 8's 660 clocks for 11 patch reads is 12 clocks per 64-byte burst
      // ON A PLAYED FABRIC AT 2-CYCLE READ LATENCY. A real request is four
      // SDRAM bursts at 12-18, so the honest figure is 48-72 and that bench
      // floor understates by 4-6x. DO NOT QUOTE 660 AS THE COST OF ANYTHING."
      // Counting the WAIT here makes the real fabric's answer measurable on a
      // composed console rather than inferred from a bench.
      if ((state_q == P_START) || ((state_q == P_STREAM) && !r_valid_i))
        store_wait_clocks_o <= store_wait_clocks_o + 32'd1;
      if ((state_q == P_REQ) || ((state_q == P_BEAT) && !hps_rsp_i.beat_valid))
        list_wait_clocks_o <= list_wait_clocks_o + 32'd1;

      // ---- the freeze watch, every cycle of the walk ------------------------
      if (tok_moved_c && !restart_q) begin
        restart_q       <= 1'b1;
        freeze_broken_o <= freeze_broken_o + 32'd1;
      end

      case (state_q)
        // ------------------------------------------------------------------
        P_IDLE: if (j_valid_i) begin
          job_off_q    <= j_list_off_i;
          job_bytes_q  <= j_list_bytes_i;
          job_crc_q    <= j_list_crc_i;
          job_count_q  <= j_patch_count_i;
          job_epoch_q  <= j_epoch_i;
          tok_q        <= frz_tok_i;
          byte_q       <= '0;
          crc_q        <= 32'hFFFF_FFFF;
          walked_q     <= '0;
          rbeat_q      <= '0;
          restart_q    <= 1'b0;
          prep_valid_q <= 1'b0;
          walks_started_o <= walks_started_o + 32'd1;

          // AN ILLEGAL PITCH IS REFUSED BEFORE ANYTHING IS SWEPT. `place32`'s
          // default arm returns zero, so walking with one would bank every
          // patch at the world origin -- placements that are wrong, identical
          // and perfectly plausible. The walk does not start.
          if (!pitch_ok_c) begin
            pitch_illegal_o <= pitch_illegal_o + 32'd1;
            restart_q       <= 1'b1;
            state_q         <= P_DONE;
          end else if (job_bad_c) begin
            jobs_refused_o <= jobs_refused_o + 32'd1;
            restart_q      <= 1'b1;
            state_q        <= P_DONE;
          end else begin
            begin_q <= 1'b1;      // sweep the reconciler's bank, once
            state_q <= P_SWEEP;
          end
        end

        // ------------------------------------------------------------------
        // `prep_begin_o` is asserted while here and `prep_gate_i` is low, so
        // the reconciler's sweep is REQUESTED until it acknowledges by opening
        // PREPARE. Waiting on the gate rather than counting clocks is what
        // makes a sweep that never ran visible instead of invisible.
        P_SWEEP: if (prep_gate_i) state_q <= P_NEXT;

        // ------------------------------------------------------------------
        // The per-record dispatch is its OWN state and not a second job for
        // P_SWEEP. Sharing them would re-enter the sweep wait after every
        // patch, which works only for as long as `prep_gate_i` happens to stay
        // high -- a correctness property resting on a level nobody promised.
        P_NEXT: begin
          if (walked_q >= job_count_q) state_q <= P_FIN;
          else                         state_q <= P_REQ;
        end

        // ------------------------------------------------------------------
        P_REQ: begin
          if (hps_rsp_i.err) begin
            bridge_errs_o <= bridge_errs_o + 32'd1;
            restart_q     <= 1'b1;
            state_q       <= P_DONE;
          end else if (hps_req_grant_i) begin
            beat_q  <= '0;
            skip_q  <= 3'(skip_bytes_c[5:3]);
            if (skip_bytes_c != 6'd0)
              list_refetch_bytes_o <= list_refetch_bytes_o + 32'({26'd0, skip_bytes_c});
            state_q <= P_BEAT;
          end
        end

        // ------------------------------------------------------------------
        P_BEAT: begin
          if (hps_rsp_i.err) begin
            bridge_errs_o <= bridge_errs_o + 32'd1;
            restart_q     <= 1'b1;
            state_q       <= P_DONE;
          end else if (hps_rsp_i.beat_valid) begin
            if (skip_q != 3'd0) begin
              // A byte this walk has already consumed, re-delivered because the
              // bridge admits only 64-byte-aligned bursts. Dropped, and NOT
              // folded: folding it would corrupt a CRC that is otherwise exact.
              skip_q <= skip_q - 3'd1;
            end else begin
              crc_q             <= fold_out;
              byte_q            <= byte_q + 32'd8;
              list_bytes_read_o <= list_bytes_read_o + 32'd8;
              rbeat_q           <= rbeat_q + 2'd1;

              if (rbeat_q == 2'd0) rec_b0_q <= hps_rsp_i.data;

              if (rbeat_q == 2'd3) begin
                // The record is complete. Beat 0 carries {island, ix, iz};
                // beat 3's low word carries source_id. Beat 1 is the page's
                // HPS address, which PREPARE does not use -- it does not load.
                cur_island_q <= rec_b0_q[31:0];
                cur_ix_q     <= signed'(rec_b0_q[47:32]);
                cur_iz_q     <= signed'(rec_b0_q[63:48]);
                cur_src_q    <= hps_rsp_i.data[15:0];
                walked_q     <= walked_q + 16'd1;
                records_walked_o <= records_walked_o + 32'd1;
                state_q      <= P_LOOKUP;
              end else if (hps_rsp_i.last) begin
                // The burst ended MID-RECORD. The elaboration guard makes a
                // burst a whole number of records, so this is unreachable for a
                // burst that started aligned; but a burst can also end short at
                // the tail of the list, and re-requesting from `byte_q` is the
                // arm that is right either way. `last` is READ rather than the
                // beat counter inferred: the bridge states where its burst
                // ends, and inferring it is how a short burst becomes a hang.
                state_q <= P_REQ;
              end else begin
                beat_q <= beat_q + ($clog2(BEATS_PER_BST+1))'(1);
              end
            end
          end
        end

        // ------------------------------------------------------------------
        // RESIDENCY IS ASKED, NEVER CHANGED. A miss is SKIPPED: architecture
        // 2.6 forbids waiting on an 80 microsecond page load mid-walk, and a
        // patch absent from the bank makes its own four edges AND every query
        // naming it fall back -- symmetric, so the symmetry law survives it.
        P_LOOKUP: if (lu_ready_i) state_q <= P_LUWAIT;

        P_LUWAIT: if (lu_ans_valid_i) begin
          if (lu_ans_hit_i) begin
            cur_slot_q <= lu_ans_slot_i;
            cur_gen_q  <= lu_ans_gen_i;
            // A placement this pitch cannot represent is refused rather than
            // wrapped. A wrapped placement puts the patch on the opposite side
            // of the world and every downstream block treats it as geometry.
            if (!range_ok_c) begin
              place_range_o <= place_range_o + 32'd1;
              state_q       <= P_NEXT;
            end else begin
              d_count_q <= 5'd0;
              state_q   <= P_START;
            end
          end else begin
            skipped_not_resident_o <= skipped_not_resident_o + 32'd1;
            state_q                <= P_NEXT;
          end
        end

        // ------------------------------------------------------------------
        P_START: if (r_ready_i) begin
          patches_prepared_o <= patches_prepared_o + 32'd1;
          state_q            <= P_STREAM;
        end

        // ------------------------------------------------------------------
        // Sixteen records in, sixteen descriptors out, paired by POSITION and
        // CHECKED by index. devstore streams `r_sp_i` with each record; if it
        // ever arrives out of order the descriptor would carry one subpatch's
        // deviations under another subpatch's centre, which banks a level in
        // the wrong lane. The check costs a 4-bit compare.
        P_STREAM: begin
          if (r_valid_i) begin
            // Register this record's payload for the NEXT beat's placement and
            // take the descriptor out in the same cycle: `sp_valid_o` is
            // `r_valid_i` and the payload registers below are what the CURRENT
            // beat presents, loaded when the previous record was accepted.
            if (r_sp_i != next_sub_c) sub_order_bad_o <= sub_order_bad_o + 32'd1;

            if (sp_ready_i) begin
              d_count_q <= d_count_q + 5'd1;
              descriptors_emitted_o <= descriptors_emitted_o + 32'd1;

              if ((d_count_q == 5'd0) && !r_fresh_i)
                patches_unfresh_o <= patches_unfresh_o + 32'd1;

              if (d_count_q == 5'(SUBPATCHES - 1)) state_q <= P_NEXT;
            end
          end
        end

        // ------------------------------------------------------------------
        // THE REPLAY'S OWN VERDICT. T5 makes the list capture data, immutable
        // for the frame; a fold that disagrees with the command's declared CRC
        // means the arena moved underneath the frame, and everything banked
        // from it describes a list nobody sealed.
        P_FIN: begin
          if ((crc_q ^ 32'hFFFF_FFFF) != job_crc_q) begin
            list_crc_mismatch_o <= list_crc_mismatch_o + 32'd1;
            restart_q           <= 1'b1;
          end
          state_q <= P_DONE;
        end

        // ------------------------------------------------------------------
        // `prep_done_o` pulses here whatever the verdict: TERRAIN.EDGERECON
        // must leave PREPARE or its query port never opens and the whole frame
        // stalls waiting for an answer. `prep_valid_o` carries the verdict
        // separately, and it is LOW on any refusal -- so the caller falls back
        // to the conservative constant, which is the console's existing and
        // always-safe behaviour.
        P_DONE: begin
          prep_valid_q <= !restart_q;
          if (!restart_q) walks_completed_o <= walks_completed_o + 32'd1;
          state_q <= P_IDLE;
        end

        default: state_q <= P_IDLE;
      endcase
    end
  end

`ifndef SYNTHESIS
  // The simulation-only corroboration. These assert the CORRECT behaviour, not
  // the defect: CLAUDE.md's "do not write a test that asserts the bug".
  // No `rst_n` term: reading it here synchronously while the design's own
  // always_ff takes it asynchronously is a SYNCASYNCNET warning, and it is not
  // needed -- both predicates depend on `state_q`, which reset clears to
  // P_IDLE. This is `zhao_terrain_tess.sv:1106-1113`'s pattern.
  always_ff @(posedge clk) begin
    if (sp_valid_o && (state_q != P_STREAM))
      $fatal(1, "prepwalk: a descriptor was offered outside the stream phase");
    // PREPARE MAY NOT CHANGE RESIDENCY. Lookup is the only residency port this
    // block has; this asserts the thing a future edit could quietly break.
    if (lu_valid_o && (state_q != P_LOOKUP))
      $fatal(1, "prepwalk: a residency lookup escaped the lookup phase");
  end
`endif

endmodule

`default_nettype wire
