// zhao_geom_loomfeed.sv -- GEOM.LOOM's NODE-STREAM CARRIER: SW.STREAM's staged
// node stream in HPS DDR, played into `zhao_geom_loom`'s `in_*` port.
//
// Owner ruling R69 (reports/OWNER-RULINGS-20260919-EVENING.md):
//
//   > R58 / I50 (GEOM.LOOM's carrier) is not blocked, only unbuilt:
//   > `zhao_part_hps` is the exact pattern and the arbiter is already
//   > N-client. It needs a block, a frozen record layout, a 5th client and a
//   > bench change -- a packet's worth, not a tail item.
//
// and R58, which names the shape:
//
//   > The R14/R43 doorbell pattern: SW.STREAM stages the sorted stream, a CSR
//   > mailbox names it, hardware acknowledges. No second transform law.
//
// Contract: design/contracts/GEOM.LOOM.STREAM.md.
// Part of GEOM.LOOM's capability; not a ledger block of its own, exactly as
// `zhao_terrain_jdoorbell` and `zhao_field_doorbell` are parts of theirs.
//
// ENFORCED-BY: tests/geometry/geom_loomfeed_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY A CARRIER AND NOT A BLOCK THAT BUILDS THE STREAM
// ---------------------------------------------------------------------------
// The owner ruling of 2026-08-31 section 6.4 puts the producer outside this
// console IN TERMS: "The ARM/compiler supplies a parent-before-child
// topologically sorted stream. Loom only composes transforms ... Keep-world
// reparenting is computed on the ARM between frames." GEOM.LOOM's own contract
// calls that deletion "what makes this block buildable".
//
// So the stream is HOST STATE, like the frame ring and the terrain arena, and
// what was missing at core entry I50 was never a block -- it was a CARRIER.
// This file is that carrier and nothing else. It sorts nothing, composes
// nothing, invents no transform and re-decides no refusal: every number it
// hands the loom was written by the ARM, and every verdict it reports is the
// loom's own or the bridge's own.
//
// ---------------------------------------------------------------------------
// WHY DDR AND NOT A WIDER MAILBOX
// ---------------------------------------------------------------------------
// GEOM.LOOM.md's content tier is 256 creatures at ~28 bones plus attachments,
// about 9,000 nodes. At the frozen 64-byte record that is 576,000 bytes per
// frame. A CSR mailbox carrying whole node records would be a 445-bit write
// port on the console edge played 9,000 times a frame, which is the shape
// entry I50 already refused. The HPS-DDR bridge is the transport this console
// has for bulk host state, `zhao_part_hps` is its worked example at a larger
// volume still (1 MiB per tick), and `zhao_hps_arbiter_n` is already N-client
// by owner ruling R4. The mailbox therefore carries a POINTER and a TICKET,
// and the bulk rides the bridge.
//
// ---------------------------------------------------------------------------
// THE FROZEN RECORD LAYOUT -- 64 BYTES, EIGHT 64-BIT BEATS, LOW HALF FIRST
// ---------------------------------------------------------------------------
// 64 bytes is not padding for its own sake and the number is not free-chosen:
// `zhao_hps_bridge` takes 64-B ALIGNED bursts of 1..64 bytes, so 64 is the
// largest burst and the only size at which one record is exactly one burst.
// The natural packing of a node is 56 bytes (445 bits of fields rounded up,
// dominated by twelve fx16 parameters = 48 bytes), and 56 is NOT a burst
// multiple: a stream of 56-byte records would put the fifth record across a
// burst boundary and cost a second request for every eighth node, plus an
// alignment law nobody would be able to read off the record. The eight spare
// bytes buy one-record-one-burst, and that is the whole argument.
//
// Byte offsets are little-endian within each 64-bit beat, which is the same
// convention `zhao_part_hps` states for its 16-byte particle record.
//
// ---- RECORD 0 OF EVERY STREAM IS THE STREAM HEADER ------------------------
//   beat 0  [31:0]  magic, STREAM_MAGIC ("LOOM")
//           [47:32] node_count, 1..MAX_NODES
//           [63:48] reserved, ignored
//   beat 1  [31:0]  cam_basis[0]   [63:32] cam_basis[1]
//   beat 2  [31:0]  cam_basis[2]   [63:32] cam_basis[3]
//   beat 3  [31:0]  cam_basis[4]   [63:32] cam_basis[5]
//   beat 4  [31:0]  cam_basis[6]   [63:32] cam_basis[7]
//   beat 5  [31:0]  cam_basis[8]   [63:32] reserved
//   beats 6..7      reserved
//
// THE CAMERA BASIS TRAVELS IN THE STREAM RATHER THAN IN THE MAILBOX, and that
// is a correctness choice, not a packing one. `cam_basis_i` is a HELD port on
// GEOM.LOOM that BILLBOARD nodes read as they compose. A basis latched by a
// mailbox write while a stream composes is two quantities loaded by two
// different enables that must agree -- CLAUDE.md's metadata-swap shape, where
// "response A's data and B's metadata" becomes "this frame's nodes and the
// next frame's camera". Carried in record 0 it is loaded ONCE, by the same act
// that starts the stream, and it cannot be replaced while that stream is in
// flight: `S_PLAY` never writes `cam_q`.
//
// ---- RECORDS 1..node_count ARE NODES --------------------------------------
//   beat 0  [9:0]   node_index          [19:10] parent_index
//           [23:20] kind                [25:24] axis
//           [26]    bodypatch           [31:27] reserved
//           [47:32] angle16             [63:48] src_id
//   beat 1  [31:0]  param[0]   [63:32]  param[1]
//   beat 2  [31:0]  param[2]   [63:32]  param[3]
//   ...
//   beat 6  [31:0]  param[10]  [63:32]  param[11]
//   beat 7          reserved
//
// `first` AND `last` ARE NOT IN THE RECORD, and their absence is deliberate.
// They are the FRAMING of the stream, and this block derives them from the
// record index against the header's `node_count`, so a staged stream cannot
// carry framing that disagrees with its own length. GEOM.LOOM's FRAMING
// refusal (reason 5) is therefore unreachable from this feed in normal play --
// a guard made unreachable by a structure, which is the same standing as that
// block's own OVERFLOW guard at the shipping parameters. It stays reachable at
// the block level, where `tests/geometry/geom_loom_directed.cpp` fires it, and
// it is reachable HERE on law 5's abandonment path, which is the one place a
// partial stream can exist. Said out loud rather than discovered: a guard that
// quietly stopped being reachable is the reading CLAUDE.md warns about.
//
// ---------------------------------------------------------------------------
// THE EXCHANGE -- three messages, the R14 shape
// ---------------------------------------------------------------------------
//   D0  HPS -> FPGA  the plan's epoch identity `cfg_plan_base_i`, held. Trace
//                    only: it rides every return so a return can be tied to
//                    the plan that produced it.
//   D1  HPS -> FPGA  a POST: {base, ticket} -- "a stream is staged at `base`;
//                    play it". Posted ahead of need into a real queue so the
//                    ARM can stage frame N+1 while frame N composes.
//   D2  FPGA -> HPS  a RETURN, exactly one per consumed post:
//                    {ticket, ok, refused, reason, nodes, plan}.
//
// There is no D3. The journal doorbell needs an ACK because its consumer holds
// a ticket table that must be released; nothing here holds state past the
// return, so an ACK would be a message with no reader. Same sentence, same
// reason, as `zhao_field_doorbell`'s.
//
// ---------------------------------------------------------------------------
// THE LAWS THIS FILE ENFORCES
// ---------------------------------------------------------------------------
// 1. POSTS ARE CONSUMED IN ORDER, ONE AT A TIME, AND ARE NEVER DROPPED. A post
//    offered into a full mailbox is HELD -- `post_ready_o` is low and
//    `post_stalls_o` counts the CYCLES, which is the number that says whether
//    POSTS is big enough. Owner ruling R55's shape.
//
// 2. A MISALIGNED BASE IS REFUSED BEFORE A BURST IS ISSUED, answered with a
//    return (`reason = RR_ALIGN`) and counted. `zhao_hps_bridge` would reject a
//    misaligned burst itself (`addr[5:0] != 0` is malformed), but it would do
//    so as an `err` with no way for the HPS to tell "your pointer was wrong"
//    from "the port was busy". Judging it here makes the answer specific. This
//    is `zhao_part_hps`'s seed-alignment refusal, at this seam.
//
// 3. A HEADER THAT IS NOT A HEADER IS REFUSED AND ANSWERED. Wrong magic
//    (`RR_MAGIC`) or a node count of zero or above MAX_NODES (`RR_COUNT`). A
//    stream of zero nodes is not a stream, and a count above the loom's bound
//    would be refused by the loom as OVERFLOW after MAX_NODES nodes had already
//    been composed and thrown away -- answering here costs one burst instead of
//    a thousand. NOTHING IS CLAMPED: a count of 2,000 is refused, never played
//    as 1,024.
//
// 4. THE LOOM'S REFUSAL IS RECORDED AND THE STREAM IS PLAYED ON TO ITS `last`.
//    This is the law whose first draft was BACKWARDS, and the correction is
//    worth the paragraph because the wrong version was the intuitive one.
//
//    The obvious reading is "the loom refused, so stop feeding it". Read
//    `zhao_geom_loom.sv`'s S_REF/S_DRAIN instead: a refused beat that did NOT
//    carry `last` puts the loom in S_DRAIN, where `in_ready_o` stays HIGH and
//    beats are SWALLOWED until the stream's own `last` goes by, and only then
//    is the store scrubbed. Its own comment says why -- "waiting for a `last`
//    that has gone by would swallow the NEXT stream's beats instead".
//
//    So a feeder that stopped would leave the loom draining, and the next
//    stream would be eaten up to ITS `last` -- with no refusal raised, because
//    S_DRAIN raises none. A whole frame of poses gone, reported by this block
//    as `ok`. That is strictly worse than the refusal, and it is invisible.
//
//    The verdict is therefore LATCHED (`loom_ref_q`, with the loom's reason and
//    the node count at the refusal) and the stream runs to its end; the ticket
//    is returned with `ok = 0`, `reason = RR_LOOM`, the loom's own reason on
//    `ret_loom_reason_o` and `ret_nodes_o` = how far it got. THE VERDICT IS NOT
//    RE-DERIVED HERE: this block does not check sortedness, parentage, kinds or
//    shear, because that would be a second opinion about a law the loom owns.
//
// 5. A BRIDGE REFUSAL IS HANDLED, NOT ARGUED -- owner ruling R54, the same
//    repair `zhao_part_hps` carries. `err` at the request is COUNTED and the
//    request is taken DOWN so the re-offer is a new request rather than the
//    same one being re-served by `zhao_hps_arbiter_n`. ERR_RETRY_N consecutive
//    refusals of ONE burst are permanent by definition and FAULT the stream
//    (`RR_BRIDGE`), which is answered and counted. Never a spin.
//
// 6. AN ABANDONED STREAM LEAVES THE LOOM WAITING FOR A `last`, AND THE NEXT
//    STREAM IS PLAYED TWICE TO GIVE IT ONE. Law 5 is the ONLY path that stops
//    a feed without a `last` -- every other ending delivers one -- so it is the
//    only path that can leave the loom open. When it does, the loom is in one
//    of exactly two states, S_RUN (still composing) or S_DRAIN (already refused
//    the partial), and BOTH are released by a `last` and by nothing else.
//
//    So the recovery does not need to know which, and deliberately does not
//    look: `stale_q` makes the next stream a FLUSH PASS. It is played whole,
//    the `last` releases the loom, the scrub follows, and the loom is then
//    provably idle -- at which point the same stream is played AGAIN, for real,
//    and only that second pass is reported. `streams_replayed_o` counts it.
//
//    The first draft keyed this on seeing a FRAMING refusal instead, and that
//    version was wrong for the S_DRAIN case: a draining loom raises no refusal
//    at all, so the trigger never came and the stream was reported `ok` while
//    every beat of it was swallowed. Keying on the ABANDONMENT rather than on
//    a symptom covers both states with one rule and no observation.
//
//    Bounded by construction: `stale_q` is set only by an abandonment and is
//    cleared by the flush pass's `last`, so a stream is flushed at most once.
//    NOTHING IS FABRICATED on this path -- the flushed beats are the same
//    bytes, re-read from the same addresses.
//
// 7. THE RETURN QUEUE CANNOT OVERFLOW, BY CREDIT. A post is consumed only while
//    fewer than RETQ consumed posts still owe their return record, and each owes
//    exactly one. `ret_overflow_o` is therefore UNREACHABLE while the credit is
//    right, so no legal stimulus can fire it and its zero is an argument rather
//    than a measurement. It carries a committed inverted-polarity mutant:
//    `tests/mutants/zhao_geom_loomfeed_mutant.sv`.
//
// 8. NOTHING TIMES OUT. A return the ARM never drains holds its credit and the
//    mailbox backs up; `post_stalls_o` says so. A carrier that discarded a
//    return to keep moving would lose the only record that a frame's poses
//    reached the palette.
//
// ---------------------------------------------------------------------------
// THE RATE IS A COST, NOT A GAP
// ---------------------------------------------------------------------------
// One record is read, then offered, then the next is read: there is no
// prefetch. The arithmetic that makes that acceptable is GEOM.LOOM's own
// measured 48 clocks per node at MUL_LANES = 1 (its header, section "THE
// THROUGHPUT TARGET IS MISSED, ON PURPOSE"), against this block's 8 beats plus
// the bridge's 16-cycle first-beat latency -- about 25 clocks, all of which the
// loom is busy for anyway. The feed is therefore not the limit at the shipping
// parameter, and the number that would say otherwise is `feed_wait_cycles_o`:
// cycles the loom was ready while this block had no record to give. If that
// ever dominates, the lever is a second record in flight and it is a change to
// this file alone.
//
// Conservative SystemVerilog subset (Quartus 17.0): no module-scope `if`, no
// implicit generate, elaboration guards inside `initial begin ... end`.
`default_nettype none

module zhao_geom_loomfeed #(
    // GEOM.LOOM's node bound, mirrored so a count above it is refused HERE
    // rather than after a thousand nodes have been composed and dropped. A
    // literal rather than an expression for the reason `zhao_field_doorbell`'s
    // parameter list gives: `tools/quartus/gen_prod_top.py` cannot evaluate a
    // parameter expression when it sizes a port and SKIPS a module it cannot
    // size, silently.
    parameter int unsigned MAX_NODES = 1024,
    parameter int unsigned IDXW      = 10,
    // The posted mailbox, in streams. Two lets the ARM stage frame N+1 while
    // frame N plays, which is the whole point of a posted mailbox.
    parameter int unsigned POSTS     = 2,
    // Consumed posts that may still owe a return record.
    parameter int unsigned RETQ      = 4,
    // Consecutive refusals of ONE burst before the stream is abandoned. Named
    // and editable for the reason `zhao_part_hps` gives: it is the boundary
    // between "the bridge was busy" and "this burst will never be accepted",
    // and nothing in the protocol distinguishes them.
    parameter int unsigned ERR_RETRY_N = 4,
    // The header's magic. "LOOM" in ASCII, little-endian in beat 0's low word.
    parameter logic [31:0] STREAM_MAGIC = 32'h4D4F_4F4C,
    // The bridge client tag this traffic carries. A parameter so the composer
    // states it; see the console core for the choice.
    parameter zhao_pkg::zhao_client_e CLIENT = zhao_pkg::ZHAO_CLIENT_ENGINE1
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- D0: the plan's epoch identity (HPS-owned, held, trace only) --------
    input  var logic [31:0] cfg_plan_base_i,

    // ---- D1: posts ----------------------------------------------------------
    input  var logic        post_valid_i,
    output var logic        post_ready_o,
    input  var logic [31:0] post_base_i,     // byte address of record 0
    input  var logic [31:0] post_ticket_i,

    // ---- D2: returns --------------------------------------------------------
    output var logic        ret_valid_o,
    input  var logic        ret_ready_i,
    output var logic [31:0] ret_ticket_o,
    output var logic        ret_ok_o,        // the stream reached the loom whole
    output var logic        ret_refused_o,   // laws 2/3: refused before any beat
    output var logic [2:0]  ret_reason_o,
    output var logic [2:0]  ret_loom_reason_o, // law 4: the loom's own reason
    output var logic [15:0] ret_nodes_o,     // nodes delivered under this ticket
    output var logic [31:0] ret_plan_o,

    // ---- MEM.HPS.BRIDGE client (read only -- this block never writes) -------
    output zhao_pkg::zhao_hps_burst_req_t hps_req_o,
    input  var logic                      hps_grant_i,
    input  zhao_pkg::zhao_hps_burst_rsp_t hps_rsp_i,

    // ---- GEOM.LOOM's node stream, name for name ----------------------------
    output var logic               lm_valid_o,
    input  var logic               lm_ready_i,
    output var logic [IDXW-1:0]    lm_node_index_o,
    output var logic [IDXW-1:0]    lm_parent_index_o,
    output var logic [3:0]         lm_kind_o,
    output var logic signed [31:0] lm_param_o [12],
    output var logic [15:0]        lm_angle_o,
    output var logic [1:0]         lm_axis_o,
    output var logic               lm_bodypatch_o,
    output var logic [15:0]        lm_src_id_o,
    output var logic               lm_first_o,
    output var logic               lm_last_o,
    output var logic signed [31:0] lm_cam_basis_o [9],

    // ---- GEOM.LOOM's refusal, OBSERVED (law 4) -----------------------------
    input  var logic       lm_refuse_valid_i,
    input  var logic [2:0] lm_refuse_reason_i,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] posts_o,             // posts consumed, all outcomes
    output var logic [31:0] streams_o,           // streams delivered whole
    output var logic [31:0] nodes_o,             // node beats accepted by the loom
    output var logic [31:0] bursts_o,            // 64-B reads granted
    output var logic [31:0] posts_refused_align_o,
    output var logic [31:0] headers_refused_o,   // magic or count
    output var logic [31:0] streams_refused_o,   // the loom refused one
    output var logic [31:0] streams_faulted_o,   // the bridge refused one
    output var logic [31:0] streams_replayed_o,  // law 6
    output var logic [31:0] post_stalls_o,       // cycles a post was held
    output var logic [31:0] bridge_errs_o,       // `err` seen at the request
    output var logic [31:0] feed_wait_cycles_o,  // the loom ready, nothing to give
    output var logic [31:0] ret_overflow_o       // unreachable by credit; see the mutant
);

  // ---- return reasons ------------------------------------------------------
  localparam logic [2:0] RR_NONE   = 3'd0;
  localparam logic [2:0] RR_ALIGN  = 3'd1;   // law 2
  localparam logic [2:0] RR_MAGIC  = 3'd2;   // law 3
  localparam logic [2:0] RR_COUNT  = 3'd3;   // law 3
  localparam logic [2:0] RR_LOOM   = 3'd4;   // law 4
  localparam logic [2:0] RR_BRIDGE = 3'd5;   // law 5

  localparam int unsigned PTRW = (POSTS > 1) ? $clog2(POSTS) : 1;
  localparam int unsigned RETW = (RETQ  > 1) ? $clog2(RETQ)  : 1;
  localparam int unsigned ERW  = $clog2(ERR_RETRY_N + 1);

  // Quartus 17.0 rejects a bare module-scope `if`; and `--lint-only` does not
  // run these, so a clean lint says nothing whatever about them (CLAUDE.md,
  // 2026-09-08).
  initial begin
    if (POSTS < 1) $fatal(1, "zhao_geom_loomfeed: POSTS must be at least 1");
    if (RETQ  < 1) $fatal(1, "zhao_geom_loomfeed: RETQ must be at least 1");
    if (ERR_RETRY_N < 1) $fatal(1, "zhao_geom_loomfeed: ERR_RETRY_N must be at least 1");
    // THE FROZEN RECORD GIVES EACH INDEX TEN BITS, so a carrier built at any
    // other IDXW would be reading a different record -- and it would do it
    // SILENTLY, by truncating a node index into something that still looks
    // legal to the loom. This guard is what makes the coupling loud: if
    // GEOM.LOOM's IDXW ever moves, the record layout moves with it in
    // design/contracts/GEOM.LOOM.STREAM.md and this line moves with both.
    if (IDXW != 10) begin
      $fatal(1, "zhao_geom_loomfeed: IDXW must be 10 (the frozen record's index width), got %0d", IDXW);
    end
    if (MAX_NODES < 1 || MAX_NODES > 65535) begin
      $fatal(1, "zhao_geom_loomfeed: MAX_NODES must be 1..65535, got %0d", MAX_NODES);
    end
  end

  // ==========================================================================
  // THE POSTED MAILBOX
  // ==========================================================================
  logic [31:0] q_base   [0:POSTS-1];
  logic [31:0] q_ticket [0:POSTS-1];

  logic [PTRW:0] q_wr, q_rd;
  wire  [PTRW:0] q_used  = q_wr - q_rd;
  wire           q_full  = (q_used == (PTRW+1)'(POSTS));
  wire           q_empty = (q_wr == q_rd);
  wire [PTRW-1:0] q_wi = q_wr[PTRW-1:0];
  wire [PTRW-1:0] q_ri = q_rd[PTRW-1:0];

  assign post_ready_o = !q_full;

  // ==========================================================================
  // THE RETURN QUEUE, AND THE CREDIT THAT MAKES IT UNOVERFLOWABLE (law 7)
  // ==========================================================================
  logic [31:0] r_ticket [0:RETQ-1];
  logic        r_ok     [0:RETQ-1];
  logic        r_refused[0:RETQ-1];
  logic [2:0]  r_reason [0:RETQ-1];
  logic [2:0]  r_lreason[0:RETQ-1];
  logic [15:0] r_nodes  [0:RETQ-1];
  logic [31:0] r_plan   [0:RETQ-1];

  logic [RETW:0] r_wr, r_rd;
  wire  [RETW:0] r_used  = r_wr - r_rd;
  wire           r_empty = (r_wr == r_rd);
  wire [RETW-1:0] r_wi = r_wr[RETW-1:0];
  wire [RETW-1:0] r_ri = r_rd[RETW-1:0];

  // Consumed posts that have not yet written their return record. THE CREDIT.
  logic [RETW:0] owed;
  wire           ret_credit = (owed + r_used) < (RETW+1)'(RETQ);

  assign ret_valid_o       = !r_empty;
  assign ret_ticket_o      = r_ticket[r_ri];
  assign ret_ok_o          = r_ok[r_ri];
  assign ret_refused_o     = r_refused[r_ri];
  assign ret_reason_o      = r_reason[r_ri];
  assign ret_loom_reason_o = r_lreason[r_ri];
  assign ret_nodes_o       = r_nodes[r_ri];
  assign ret_plan_o        = r_plan[r_ri];

  // ==========================================================================
  // THE STREAM IN FLIGHT
  // ==========================================================================
  localparam logic [2:0] S_IDLE = 3'd0;  // waiting for a post with credit
  localparam logic [2:0] S_HDR  = 3'd1;  // reading and judging record 0
  localparam logic [2:0] S_READ = 3'd2;  // reading node record `rec_q`
  localparam logic [2:0] S_PLAY = 3'd3;  // offering the assembled node
  localparam logic [2:0] S_RET  = 3'd4;  // writing the return record

  logic [2:0]  s_q;
  logic [31:0] base_q;        // record 0's byte address
  logic [31:0] ticket_q;
  logic [31:0] plan_q;
  logic [15:0] count_q;       // nodes in this stream, from the header
  logic [15:0] rec_q;         // 1..count_q: the node record being fetched
  logic [15:0] done_q;        // nodes the loom has accepted
  logic [2:0]  reason_q;
  logic        ok_q;
  logic        refused_q;
  logic [2:0]  lreason_q;

  // Law 6: the loom may be holding a stream this block abandoned without a
  // `last`, in which case it is waiting for one -- either composing (S_RUN) or
  // swallowing (S_DRAIN). Both are released by a `last` and neither can be
  // released any other way.
  logic        stale_q;
  logic        flush_q;       // this PASS is the flush pass, not the real one
  // The loom refused this stream. LATCHED rather than acted on: the loom
  // drains to `last`, so the feed must not stop (law 4).
  logic        loom_ref_q;
  logic [15:0] ref_at_q;      // nodes delivered when the refusal arrived

  // The eight landed beats of the record being read.
  logic [63:0] w_q [0:7];
  logic [2:0]  beat_q;

  // The camera basis, loaded ONCE per stream from record 0 and never written
  // while S_PLAY runs. See the header: this is the anti-metadata-swap property.
  logic signed [31:0] cam_q [9];

  // ---- the one burst in flight --------------------------------------------
  localparam logic [1:0] B_IDLE = 2'd0;
  localparam logic [1:0] B_REQ  = 2'd1;   // request held until grant or err
  localparam logic [1:0] B_RD   = 2'd2;   // read beats landing

  logic [1:0]     b_q;
  logic [31:0]    b_addr_q;
  logic [ERW-1:0] err_run_q;
  // A burst this block still wants. Set with the address by the stream FSM,
  // cleared by the burst's last beat. It is what makes a TRANSIENT bridge
  // refusal re-offer rather than hang: B_REQ drops to B_IDLE on `err` so the
  // re-offer is a NEW request (the arbiter re-serves a held one), and B_IDLE
  // raises it again while this flag stands.
  logic           b_pending_q;

  assign hps_req_o.valid  = (b_q == B_REQ);
  assign hps_req_o.write  = 1'b0;
  assign hps_req_o.client = CLIENT;
  assign hps_req_o.addr   = b_addr_q;
  assign hps_req_o.len    = 7'd64;

  wire rbeat_c = (b_q == B_RD) && hps_rsp_i.beat_valid;
  // The record in `w_q` is complete and the engine is quiet.
  wire rec_landed_c = !b_pending_q && (b_q == B_IDLE);
  // LAW 5's permanent case, read by the stream FSM on the same edge the burst
  // engine sees the refusal.
  wire err_permanent_c = (b_q == B_REQ) && hps_rsp_i.err &&
                         (ERW'(err_run_q + ERW'(1)) >= ERW'(ERR_RETRY_N));

  // ---- the node the loom is being offered ---------------------------------
  // Combinational slices of the landed beats. Nothing is recomputed: every
  // field is a bit range of bytes the ARM wrote.
  wire [9:0] rec_node_c   = w_q[0][9:0];
  wire [9:0] rec_parent_c = w_q[0][19:10];

  assign lm_valid_o        = (s_q == S_PLAY);
  assign lm_node_index_o   = rec_node_c;
  assign lm_parent_index_o = rec_parent_c;
  assign lm_kind_o         = w_q[0][23:20];
  assign lm_axis_o         = w_q[0][25:24];
  assign lm_bodypatch_o    = w_q[0][26];
  assign lm_angle_o        = w_q[0][47:32];
  assign lm_src_id_o       = w_q[0][63:48];
  assign lm_first_o        = (rec_q == 16'd1);
  assign lm_last_o         = (rec_q == count_q);

  // Unrolled rather than looped: Quartus 17.0's subset is the constraint this
  // file is written to (charter 2), and twelve explicit lines cannot be read
  // two ways.
  assign lm_param_o[0]  = signed'(w_q[1][31:0]);
  assign lm_param_o[1]  = signed'(w_q[1][63:32]);
  assign lm_param_o[2]  = signed'(w_q[2][31:0]);
  assign lm_param_o[3]  = signed'(w_q[2][63:32]);
  assign lm_param_o[4]  = signed'(w_q[3][31:0]);
  assign lm_param_o[5]  = signed'(w_q[3][63:32]);
  assign lm_param_o[6]  = signed'(w_q[4][31:0]);
  assign lm_param_o[7]  = signed'(w_q[4][63:32]);
  assign lm_param_o[8]  = signed'(w_q[5][31:0]);
  assign lm_param_o[9]  = signed'(w_q[5][63:32]);
  assign lm_param_o[10] = signed'(w_q[6][31:0]);
  assign lm_param_o[11] = signed'(w_q[6][63:32]);

  assign lm_cam_basis_o[0] = cam_q[0];
  assign lm_cam_basis_o[1] = cam_q[1];
  assign lm_cam_basis_o[2] = cam_q[2];
  assign lm_cam_basis_o[3] = cam_q[3];
  assign lm_cam_basis_o[4] = cam_q[4];
  assign lm_cam_basis_o[5] = cam_q[5];
  assign lm_cam_basis_o[6] = cam_q[6];
  assign lm_cam_basis_o[7] = cam_q[7];
  assign lm_cam_basis_o[8] = cam_q[8];

  wire lm_fire_c = lm_valid_o && lm_ready_i;

  // The loom is ready and this block has nothing to give it. The number that
  // says whether the one-record-in-flight choice costs anything.
  wire feed_wait_c = lm_ready_i && !lm_valid_o &&
                     ((s_q == S_READ) || (s_q == S_HDR));

  // ---- the header's verdict (law 3) ---------------------------------------
  wire [15:0] hdr_count_c = w_q[0][47:32];
  wire        hdr_magic_ok_c = (w_q[0][31:0] == STREAM_MAGIC);
  wire        hdr_count_ok_c = (hdr_count_c != 16'd0) &&
                               (hdr_count_c <= 16'(MAX_NODES));

  // ---- the post's verdict (law 2) -----------------------------------------
  wire post_aligned_c = (q_base[q_ri][5:0] == 6'd0);

  integer i;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      q_wr <= '0;
      q_rd <= '0;
      r_wr <= '0;
      r_rd <= '0;
      owed <= '0;
      s_q  <= S_IDLE;
      b_q  <= B_IDLE;
      base_q <= 32'd0;
      ticket_q <= 32'd0;
      plan_q <= 32'd0;
      count_q <= 16'd0;
      rec_q <= 16'd0;
      done_q <= 16'd0;
      reason_q <= RR_NONE;
      ok_q <= 1'b0;
      refused_q <= 1'b0;
      lreason_q <= 3'd0;
      stale_q <= 1'b0;
      flush_q <= 1'b0;
      loom_ref_q <= 1'b0;
      ref_at_q <= 16'd0;
      beat_q <= 3'd0;
      b_addr_q <= 32'd0;
      b_pending_q <= 1'b0;
      err_run_q <= '0;
      posts_o <= 32'd0;
      streams_o <= 32'd0;
      nodes_o <= 32'd0;
      bursts_o <= 32'd0;
      posts_refused_align_o <= 32'd0;
      headers_refused_o <= 32'd0;
      streams_refused_o <= 32'd0;
      streams_faulted_o <= 32'd0;
      streams_replayed_o <= 32'd0;
      post_stalls_o <= 32'd0;
      bridge_errs_o <= 32'd0;
      feed_wait_cycles_o <= 32'd0;
      ret_overflow_o <= 32'd0;
      for (i = 0; i < int'(POSTS); i = i + 1) begin
        q_base[i]   <= 32'd0;
        q_ticket[i] <= 32'd0;
      end
      for (i = 0; i < int'(RETQ); i = i + 1) begin
        r_ticket[i]  <= 32'd0;
        r_ok[i]      <= 1'b0;
        r_refused[i] <= 1'b0;
        r_reason[i]  <= RR_NONE;
        r_lreason[i] <= 3'd0;
        r_nodes[i]   <= 16'd0;
        r_plan[i]    <= 32'd0;
      end
      for (i = 0; i < 8; i = i + 1) w_q[i] <= 64'd0;
      for (i = 0; i < 9; i = i + 1) cam_q[i] <= 32'sd0;
    end else begin
      // ---- D1 intake (law 1) ------------------------------------------------
      if (post_valid_i && post_ready_o) begin
        q_base[q_wi]   <= post_base_i;
        q_ticket[q_wi] <= post_ticket_i;
        q_wr <= q_wr + 1'b1;
        if (posts_o != 32'hFFFF_FFFF) posts_o <= posts_o + 32'd1;
      end else if (post_valid_i && !post_ready_o) begin
        // HELD, not dropped. The count is cycles.
        if (post_stalls_o != 32'hFFFF_FFFF) post_stalls_o <= post_stalls_o + 32'd1;
      end

      // ---- D2 drain ---------------------------------------------------------
      if (ret_valid_o && ret_ready_i) r_rd <= r_rd + 1'b1;

      if (feed_wait_c && (feed_wait_cycles_o != 32'hFFFF_FFFF)) begin
        feed_wait_cycles_o <= feed_wait_cycles_o + 32'd1;
      end

      // ---- LAW 4: the loom's verdict, LATCHED --------------------------------
      // `zhao_geom_loom` accepts the offending beat (`in_ready_o` is a pure
      // state decode that never looks at the fault) and pulses `refuse_valid_o`
      // the cycle AFTER, from S_REF -- so the refusal always lands while this
      // block is between beats, in S_READ or S_PLAY, never on the fire edge.
      // It is latched rather than acted on because the loom then enters
      // S_DRAIN and SWALLOWS beats until the stream's own `last`. Stopping the
      // feed here would leave it draining, and the NEXT stream would be eaten
      // to ITS `last` with no refusal raised at all -- a whole frame lost
      // silently, which is strictly worse than the refusal being reported.
      // This is why law 4 reads "record, and play on".
      if (lm_refuse_valid_i && (s_q != S_IDLE) && (s_q != S_RET) && !loom_ref_q) begin
        loom_ref_q <= 1'b1;
        lreason_q  <= lm_refuse_reason_i;
        ref_at_q   <= done_q;
        if (!flush_q && (streams_refused_o != 32'hFFFF_FFFF)) begin
          streams_refused_o <= streams_refused_o + 32'd1;
        end
      end

      // ---- the burst engine -------------------------------------------------
      // Driven by the stream FSM, which sets B_REQ and the address together.
      case (b_q)
        B_IDLE: begin
          beat_q <= 3'd0;
          // RE-OFFER. A burst the FSM still wants and that the bridge refused
          // transiently comes back here as a NEW request.
          if (b_pending_q) b_q <= B_REQ;
        end

        // HELD UNTIL GRANTED OR REFUSED (law 5, owner ruling R54). The bridge
        // refuses a malformed burst or one that collides with a busy port with
        // `err` and NO grant. Every burst this block asks for is 64 bytes at a
        // 64-byte-aligned address -- law 2 refuses any base that is not -- so
        // the first cause is believed unreachable and the second is transient.
        // That belief is the reason `bridge_errs_o` is expected to read zero in
        // the console; it is NOT the handling, because `zhao_hps_arbiter_n`
        // re-serves a held request and a refusal this state could not see would
        // be an unbounded spin with every counter here frozen.
        B_REQ: begin
          if (hps_rsp_i.err) begin
            if (bridge_errs_o != 32'hFFFF_FFFF) bridge_errs_o <= bridge_errs_o + 32'd1;
            b_q <= B_IDLE;   // take the request DOWN, then re-offer
            if (ERW'(err_run_q + ERW'(1)) < ERW'(ERR_RETRY_N)) begin
              err_run_q <= ERW'(err_run_q + ERW'(1));
            end
            // The permanent case is handled by the stream FSM below, which owns
            // the return record; it reads `err_run_q` on the same edge.
          end else if (hps_grant_i) begin
            b_q       <= B_RD;
            err_run_q <= '0;
            beat_q    <= 3'd0;
            if (bursts_o != 32'hFFFF_FFFF) bursts_o <= bursts_o + 32'd1;
          end
        end

        // The bridge answers `err` only at the REQUEST, never once a burst is
        // granted, so a granted read runs to its last beat.
        B_RD: begin
          if (rbeat_c) begin
            w_q[beat_q] <= hps_rsp_i.data;
            beat_q <= beat_q + 3'd1;
            if (hps_rsp_i.last) begin
              b_q         <= B_IDLE;
              b_pending_q <= 1'b0;
            end
          end
        end

        default: b_q <= B_IDLE;
      endcase

      // ---- the stream FSM ---------------------------------------------------
      case (s_q)
        S_IDLE: begin
          if (!q_empty && ret_credit) begin
            ticket_q   <= q_ticket[q_ri];
            plan_q     <= cfg_plan_base_i;
            base_q     <= q_base[q_ri];
            done_q     <= 16'd0;
            rec_q      <= 16'd0;
            count_q    <= 16'd0;
            lreason_q  <= 3'd0;
            loom_ref_q <= 1'b0;
            ref_at_q   <= 16'd0;
            // LAW 6: a stream that starts while the loom holds an abandoned
            // one is played twice -- once to hand over the `last` the loom is
            // waiting for, once for real.
            flush_q    <= stale_q;
            owed       <= owed + 1'b1;
            q_rd       <= q_rd + 1'b1;
            if (!post_aligned_c) begin
              // LAW 2: refused before a burst is issued.
              ok_q      <= 1'b0;
              refused_q <= 1'b1;
              reason_q  <= RR_ALIGN;
              s_q       <= S_RET;
              if (posts_refused_align_o != 32'hFFFF_FFFF) begin
                posts_refused_align_o <= posts_refused_align_o + 32'd1;
              end
            end else begin
              b_addr_q    <= q_base[q_ri];
              b_pending_q <= 1'b1;
              b_q         <= B_REQ;
              err_run_q   <= '0;
              s_q         <= S_HDR;
            end
          end
        end

        // Record 0. The burst engine lands eight beats; judge them when the
        // engine is quiet again.
        S_HDR: begin
          if (err_permanent_c) begin
            // LAW 5: permanent. Nothing has reached the loom, so the loom is
            // NOT left mid-stream and `stale_q` is not set.
            ok_q        <= 1'b0;
            refused_q   <= 1'b0;
            reason_q    <= RR_BRIDGE;
            err_run_q   <= '0;
            b_pending_q <= 1'b0;
            b_q         <= B_IDLE;
            s_q         <= S_RET;
            if (streams_faulted_o != 32'hFFFF_FFFF) begin
              streams_faulted_o <= streams_faulted_o + 32'd1;
            end
          end else if (rec_landed_c) begin
            if (!hdr_magic_ok_c) begin
              ok_q      <= 1'b0;
              refused_q <= 1'b1;
              reason_q  <= RR_MAGIC;
              s_q       <= S_RET;
              if (headers_refused_o != 32'hFFFF_FFFF) begin
                headers_refused_o <= headers_refused_o + 32'd1;
              end
            end else if (!hdr_count_ok_c) begin
              ok_q      <= 1'b0;
              refused_q <= 1'b1;
              reason_q  <= RR_COUNT;
              s_q       <= S_RET;
              if (headers_refused_o != 32'hFFFF_FFFF) begin
                headers_refused_o <= headers_refused_o + 32'd1;
              end
            end else begin
              // The basis is loaded ONCE, here, by the act that starts the
              // stream. Nothing writes `cam_q` again until the next S_HDR.
              count_q  <= hdr_count_c;
              cam_q[0] <= signed'(w_q[1][31:0]);
              cam_q[1] <= signed'(w_q[1][63:32]);
              cam_q[2] <= signed'(w_q[2][31:0]);
              cam_q[3] <= signed'(w_q[2][63:32]);
              cam_q[4] <= signed'(w_q[3][31:0]);
              cam_q[5] <= signed'(w_q[3][63:32]);
              cam_q[6] <= signed'(w_q[4][31:0]);
              cam_q[7] <= signed'(w_q[4][63:32]);
              cam_q[8]    <= signed'(w_q[5][31:0]);
              rec_q       <= 16'd1;
              b_addr_q    <= base_q + 32'd64;
              b_pending_q <= 1'b1;
              b_q         <= B_REQ;
              s_q         <= S_READ;
            end
          end
        end

        // A node record is in flight.
        S_READ: begin
          if (err_permanent_c) begin
            // LAW 5. If any beat has already reached the loom, the stream was
            // opened without a `last`, so the loom IS left mid-stream and the
            // next stream pays law 6's single replay. If none has -- the fault
            // landed on record 1 -- nothing was opened and there is nothing to
            // drop, which is why `stale_q` follows `done_q` rather than being
            // set unconditionally. Getting that wrong would have thrown away a
            // good stream's first node for a stream that never started.
            ok_q        <= 1'b0;
            refused_q   <= 1'b0;
            reason_q    <= RR_BRIDGE;
            err_run_q   <= '0;
            stale_q     <= (done_q != 16'd0);
            b_pending_q <= 1'b0;
            b_q         <= B_IDLE;
            s_q         <= S_RET;
            if (streams_faulted_o != 32'hFFFF_FFFF) begin
              streams_faulted_o <= streams_faulted_o + 32'd1;
            end
          end else if (rec_landed_c) begin
            s_q <= S_PLAY;
          end
        end

        // The record is offered to the loom. The loom's verdict is RECORDED
        // above and never re-derived (law 4); the stream is played to its
        // `last` either way, because that is what releases the loom's S_DRAIN.
        S_PLAY: begin
          if (lm_fire_c) begin
            done_q <= done_q + 16'd1;
            if (nodes_o != 32'hFFFF_FFFF) nodes_o <= nodes_o + 32'd1;
            if (rec_q == count_q) begin
              // `last` went with this beat. Whatever the loom was doing --
              // composing this stream, DRAINING it after a refusal, or draining
              // an abandoned one -- a `last` ends it and the scrub follows, so
              // the loom is IDLE next. That is the property law 6 stands on.
              if (flush_q) begin
                // LAW 6. This pass existed only to hand the loom the `last` it
                // was waiting for. It proves nothing about this stream and is
                // not reported; play the stream again, for real, into a loom
                // that is now provably idle. Bounded by construction:
                // `stale_q` is cleared here and only a fresh abandonment sets
                // it, so a stream is flushed at most once. NOTHING IS
                // FABRICATED -- the same bytes, re-read from the same addresses.
                flush_q     <= 1'b0;
                stale_q     <= 1'b0;
                loom_ref_q  <= 1'b0;
                lreason_q   <= 3'd0;
                ref_at_q    <= 16'd0;
                done_q      <= 16'd0;
                rec_q       <= 16'd1;
                b_addr_q    <= base_q + 32'd64;
                b_pending_q <= 1'b1;
                b_q         <= B_REQ;
                err_run_q   <= '0;
                s_q         <= S_READ;
                if (streams_replayed_o != 32'hFFFF_FFFF) begin
                  streams_replayed_o <= streams_replayed_o + 32'd1;
                end
              end else if (loom_ref_q) begin
                ok_q      <= 1'b0;
                refused_q <= 1'b0;
                reason_q  <= RR_LOOM;
                stale_q   <= 1'b0;
                s_q       <= S_RET;
              end else begin
                ok_q      <= 1'b1;
                refused_q <= 1'b0;
                reason_q  <= RR_NONE;
                stale_q   <= 1'b0;
                s_q       <= S_RET;
                if (streams_o != 32'hFFFF_FFFF) streams_o <= streams_o + 32'd1;
              end
            end else begin
              rec_q       <= rec_q + 16'd1;
              b_addr_q    <= base_q + 32'({rec_q + 16'd1, 6'd0});
              b_pending_q <= 1'b1;
              b_q         <= B_REQ;
              err_run_q   <= '0;
              s_q         <= S_READ;
            end
          end
        end

        // The return record. The space was reserved when the post was consumed
        // (law 7), so this write cannot be refused; `ret_overflow_o` says so if
        // it ever is.
        S_RET: begin
          r_ticket[r_wi]  <= ticket_q;
          r_ok[r_wi]      <= ok_q;
          r_refused[r_wi] <= refused_q;
          r_reason[r_wi]  <= reason_q;
          r_lreason[r_wi] <= lreason_q;
          // HOW FAR IT GOT. On a clean stream that is every node; on a refused
          // one it is the count AT THE REFUSAL, not the larger number the loom
          // then swallowed into its drain -- reporting the drained total would
          // tell the ARM the stream nearly worked when the fault was at node 2.
          r_nodes[r_wi]   <= loom_ref_q ? ref_at_q : done_q;
          r_plan[r_wi]    <= plan_q;
          r_wr  <= r_wr + 1'b1;
          owed  <= owed - 1'b1;
          s_q   <= S_IDLE;
          if (r_used == (RETW+1)'(RETQ)) begin
            if (ret_overflow_o != 32'hFFFF_FFFF) begin
              ret_overflow_o <= ret_overflow_o + 32'd1;
            end
          end
        end

        default: s_q <= S_IDLE;
      endcase
    end
  end

endmodule

`default_nettype wire
