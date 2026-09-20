// zhao_field_doorbell.sv - THE FIELD PROGRAM DOORBELL: SW.STREAM's staged
// program plan, attached to `zhao_field_host`'s loader and to FIELD.PROGCACHE's
// commit phase.
//
// Owner ruling R43 (reports/OWNER-RULINGS-20260919-EVENING.md):
//
//   > I42: FIELD's program loader -- A doorbell contract on the R14 pattern:
//   > SW.STREAM stages the plan and writes the EXISTING loader words, and
//   > CMD.EXEC does only the handle -> program-hash lookup that TerrainField
//   > needs.
//
// The R14 instance this copies is `zhao_terrain_jdoorbell` and its contract
// `design/contracts/TERRAIN.WRITEBACK.DOORBELL.md`. What is copied is the
// SHAPE -- a posted mailbox, a credit-reserved return queue, a ticket the
// hardware hands back, and every refusal answered rather than dropped -- not
// the fields, which are this seam's own.
//
// ---------------------------------------------------------------------------
// WHY A DOORBELL AND NOT A WIDER PORT ON THE CORE
// ---------------------------------------------------------------------------
// `zhao_field_host`'s `ld_*` is a RAW LEAF PORT: four fields and a 96-bit word,
// with no identity, no ordering law and no answer. Exposed at the console's
// edge it was entry I42 -- a boundary with no owner, and the entry says what a
// tie-off there would cost: "a tied-off loader leaves the store empty forever,
// so every profile run returns ST_NO_PROGRAM and the whole engine folds away in
// synthesis".
//
// The HPS is genuinely on the other side of this edge -- `zfield::decode` is
// software by FIELD.PROGCACHE's own contract, and the plan that orders the
// words is SW.STREAM's. So what closes I42 is not moving the port; it is
// giving the exchange a CONTRACT, the way I28 was closed for the journal:
// a posted mailbox the HPS fills ahead of need, an order law this block
// enforces, a ticketed return, and counters that make every refusal visible.
// In Verilator the harness IS the HPS, exactly as it is for `terr_jdb_*`.
//
// ---------------------------------------------------------------------------
// THE EXCHANGE
// ---------------------------------------------------------------------------
//   D0  HPS -> FPGA  the plan's epoch identity {plan base}, held. Trace only:
//                    it travels out on every return so a return can be tied to
//                    the plan that produced it.
//   D1  HPS -> FPGA  a POST, one of three:
//                      LOOKUP  a hash -> is it resident, and in which slot
//                      LOAD    a loader word (kind/slot/addr/data96), verbatim
//                      COMMIT  a hash + the one verdict bit `zfield::decode`
//                              produces
//   D2  FPGA -> HPS  a RETURN, one per consumed LOOKUP and one per consumed
//                    COMMIT: {ticket, op, ok, refused, hit/inserted, evicted,
//                    slot, plan}.
//
// There is no D3. The journal doorbell needs an ACK because its consumer holds
// a ticket table that must be released; nothing here holds state past the
// return, so an ACK would be a message with no reader.
//
// WHY THE LOOKUP IS THE HPS's AND NOT A HARDWARE CLIENT'S. FIELD.PROGCACHE's
// contract is explicit that "the decode a miss requires costs orders of
// magnitude more than the lookup and belongs to the caller", and `zfield::decode`
// is software. The caller of a lookup is therefore whoever would act on a miss,
// and only software can: it asks whether a hash is resident, and on a miss it
// decodes, posts the words and commits. Splitting the question from the answer
// -- hardware asking, software repairing -- would be two halves of one decision
// on opposite sides of the edge. Entry I42's own sentence is this paragraph in
// shorter form.
//
// ---------------------------------------------------------------------------
// THE LAWS THIS FILE ENFORCES
// ---------------------------------------------------------------------------
// 1. POSTS ARE CONSUMED IN ORDER, one at a time, and the mailbox is a real
//    queue so the HPS may stage a whole program ahead of the fabric going idle
//    (the host accepts `ld_*` only while the fabric is IDLE -- its own law).
//    A post offered into a full mailbox is NOT dropped: `post_ready_o` is low
//    and `post_stalls_o` counts the cycles. Owner ruling R55's shape --
//    a request that cannot be taken is HELD, never silently discarded.
//
// 2. A COMMIT MAY NOT PROMISE A SLOT WHOSE HEADER WAS NOT WRITTEN. This is the
//    order law and it is the reason the block exists rather than being a FIFO.
//    `zhao_field_host` marks a slot runnable on the HEADER write and clears
//    that bit on any uop/table/uniform write; FIELD.PROGCACHE's contract names
//    the failure class as a directory promising a hash to microcode that is not
//    there. So this block shadows the same bit, and a COMMIT for a slot whose
//    header has not been written since the last commit is REFUSED: the
//    directory is never offered the hash, the HPS gets a return with
//    `ret_refused_o` set and `ret_ok_o` clear, and `commits_refused_o` counts
//    it. ANSWERED AND COUNTED, NEVER HUNG -- owner ruling R20.
//
// 2b. TO WHOEVER WRITES THE PLAN THAT FILLS THIS MAILBOX, and it is the one
//    thing in this file that is easy to leave out: the HEADER word now carries
//    a REQUIRED-OUTPUT MASK in bits [32 +: OUT_LANES] (owner ruling R101).
//    This block forwards the 96 bits verbatim and cannot supply it.
//
//    `mask == 0` means "the program declared nothing", and `zhao_field_host`
//    then falls back to its pre-R101 test -- a run counts as successful if ANY
//    lane of the output window was written. A plan that omits the mask
//    therefore LOADS AND RUNS AND PASSES EVERY GATE, and silently restores the
//    exact defect R101 repaired: a program that writes four of its seven
//    declared lanes answers 8'h00 SUCCESS with three lanes carrying the zero
//    the host cleared them to.
//
//    That is not hypothetical arithmetic. `tools/field/zprog_output_coverage.py`
//    measured all three Earth programs this repo ships -- crater_ring,
//    impact_wave, wave_pool -- and every one of them leaves THREE of the
//    console's seven window lanes unwritten, because the IR does not require a
//    program's output registers to be contiguous and the capture window is.
//    The mask each one needs is printed by that probe (0x17, 0x1D, 0x17).
//
//    The mask is derived, not invented: it is `spec/form/field-ir.md` 5.3's
//    I/O map, which names the output registers, indexed from `out_base`. The
//    probe computes it from the `.zprog` and the plan writer should too,
//    rather than by hand.
//
// 3. THE RETURN QUEUE CANNOT OVERFLOW, BY CREDIT. A commit is consumed only
//    while fewer than RETQ consumed commits still owe their return record.
//    Each owes exactly one. The directory's response cannot be stalled by this
//    block without stalling the directory itself, so the space is RESERVED when
//    the commit is consumed rather than checked when the response arrives --
//    the same law, and for the same reason, as the journal doorbell's rule 5.
//    `ret_overflow_o` is therefore UNREACHABLE while the credit is right, which
//    means no legal stimulus can fire it and its zero is an argument rather
//    than a measurement. It carries a committed inverted-polarity mutant:
//    `tests/mutants/zhao_field_doorbell_mutant.sv`.
//
// 4. NOTHING TIMES OUT. A return the HPS never drains holds its credit and the
//    mailbox backs up; `post_stalls_o` says so. A doorbell that discarded a
//    return to keep moving would lose the only record that a program became
//    resident.
//
// 5. EVERY OPERAND IS CAPTURED AT POST ACCEPTANCE. Owner directive section
//    10.2: "All operands are captured on POST ACCEPTANCE, including plan/frame
//    identity. The old drain samples cfg_plan_base_i later; do not propagate
//    that live-pin metadata pattern into FH2."
//
//    THIS FILE WAS THE "OLD DRAIN" THE DIRECTIVE IS DESCRIBING, and the defect
//    was real rather than stylistic. `q_ticket` was captured at intake while
//    the plan identity was read LIVE off `cfg_plan_base_i` at drain time, so a
//    return paired POST A's TICKET with WHATEVER PLAN WAS ON THE PIN WHEN THE
//    DRAIN GOT AROUND TO IT. With two posts queued across a plan change, both
//    returns carried the second plan and the first post's plan was
//    unrecoverable -- the exact shape of CLAUDE.md's metadata-swap chapter,
//    where "the two things a return pairs must not be loaded by one enable".
//
//    The irony worth recording: the comment at the `cm_*` capture below already
//    claimed this property, correctly, FOR THE RESPONSE WINDOW -- and the queue
//    upstream of it broke the same rule in the same block. A guarantee stated
//    about one stage is not a guarantee about the path.
//
//    `zhao_terrain_pageloader.sv:143` states the rule positively and was doing
//    it right all along: "Read live rather than captured: they are the
//    machine's state, not the job's payload. The JOB's fields are captured at
//    acceptance."
//
// 6. OP 3 IS EXPLICITLY DECODED, AND THE DECODE IS EXHAUSTIVE. Owner decision
//    FH14: "The old doorbell's catch-all LOAD arm currently catches
//    post_op=3. Replace it with exhaustive decoding before introducing the
//    extension. Unknown operations must return a counted refusal, never become
//    a load."
//
//    The old final `else` sent EVERYTHING that was not a commit or a lookup to
//    `D_LOAD`, and `post_op_i` is two bits, so op 3 was encodable and landed on
//    the loader -- performing a real uop/table/header/uniform write with
//    whatever `post_kind_i` happened to carry, and moving the header shadow
//    with it. An unknown operation silently became a load into the host.
//
//    Op 3 is now the FH2 transaction (directive section 10.2), routed to
//    `zhao_field_loader` and sub-decoded by `post_kind_i`:
//      kind 0 INSTALL_CAPSULE, 1 BIND_PROGRAM, 2 CONTROL, 3 reserved.
//    The reserved kind and every unknown CONTROL verb are answered
//    BAD_OPERATION by the loader, counted, and write nothing.
//
// Conservative SystemVerilog subset (Quartus 17.0): no module-scope `if`, no
// implicit generate, elaboration guards inside `initial begin ... end`.
//
// ENFORCED-BY: tests/field/field_doorbell_directed.cpp:main
`default_nettype none

module zhao_field_doorbell #(
    // Resident programs -- `zhao_field_host`'s PROGS, and the width of the
    // header shadow. A literal rather than $clog2 for the reason that module's
    // parameter list gives: `tools/quartus/gen_prod_top.py` cannot evaluate a
    // parameter expression when it sizes a port and SKIPS a module it cannot
    // size, silently.
    parameter int unsigned PROGS   = 8,
    parameter int unsigned SLOTW   = 3,   // $clog2(PROGS) at PROGS = 8
    parameter int unsigned LDADDRW = 7,   // the host's LDADDRW
    // THE MAILBOX'S OWN ADDRESS WIDTH, and it is DELIBERATELY WIDER than the
    // loader's. Owner directive section 10.2: "The present LDADDRW default is
    // 7. FH2 control decoding needs 8 bits only if using this proposed 8-bit
    // verb field; widen that mailbox field explicitly to at least 8 through its
    // wrappers. ... Do not write [7:0] onto an unchanged 7-bit port. This brief
    // selects an 8-bit FH2 control address; legacy loader addressing retains
    // its separately derived width."
    //
    // So `post_addr_i` is POSTADDRW and `ld_addr_o` stays LDADDRW. A LEGACY
    // LOAD whose address does not fit the loader's width is REFUSED rather
    // than truncated -- a silent narrowing here would write the wrong microcode
    // address, which is the most expensive possible way to lose a bit.
    parameter int unsigned POSTADDRW = 8,
    // The posted mailbox: load words staged ahead of the fabric going idle.
    parameter int unsigned POSTS   = 4,
    // Consumed commits that may still owe a return record.
    parameter int unsigned RETQ    = 4
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- D0: the plan's epoch identity (HPS-owned, held) --------------------
    input  var logic [31:0] cfg_plan_base_i,

    // ---- D1: posts ----------------------------------------------------------
    input  var logic                 post_valid_i,
    output var logic                 post_ready_o,
    // 0 = LOAD WORD, 1 = COMMIT (the decode verdict), 2 = LOOKUP, 3 = FH2.
    // All four encodings are now DEFINED; there is no catch-all.
    input  var logic [1:0]           post_op_i,
    // op 0: host ld_kind -- 0 uop, 1 table, 2 header, 3 uniform.
    // op 3: FH2 kind    -- 0 INSTALL_CAPSULE, 1 BIND_PROGRAM, 2 CONTROL,
    //                      3 reserved (answered BAD_OPERATION).
    // THREE BITS AS OF PACKET C1. `zhao_field_host_v2` decodes eight load kinds
    // and implements all eight; at two bits, kinds 4..7 (OUTMAP, ASSOC,
    // INITPROOF, PREPARED) had no producer anywhere on the console and the new
    // host's ordinal machinery was unreachable. `fh2_kind_o` below stays TWO
    // bits and takes the low pair -- the FH2 sub-decode has four kinds and is a
    // different quantity that happens to share this mailbox word. Widening both
    // because they sit in one field is the mistake directive 10.2 names for
    // `post_addr_i`, one field over.
    input  var logic [2:0]           post_kind_i,
    input  var logic [SLOTW-1:0]     post_slot_i,
    input  var logic [POSTADDRW-1:0] post_addr_i,
    input  var logic [95:0]          post_data_i,
    input  var logic [31:0]          post_hash_i,    // COMMIT only
    input  var logic                 post_ok_i,      // COMMIT only: zfield::decode's one bit
    input  var logic [31:0]          post_ticket_i,

    // ---- to zhao_field_host's loader ----------------------------------------
    output var logic                 ld_valid_o,
    input  var logic                 ld_ready_i,
    output var logic [2:0]           ld_kind_o,
    output var logic [SLOTW-1:0]     ld_slot_o,
    output var logic [LDADDRW-1:0]   ld_addr_o,
    output var logic [95:0]          ld_data_o,

    // ---- to FIELD.PROGCACHE's lookup phase ----------------------------------
    output var logic                 pc_lu_valid_o,
    input  var logic                 pc_lu_ready_i,
    output var logic [31:0]          pc_lu_hash_o,
    input  var logic                 pc_lu_resp_valid_i,
    output var logic                 pc_lu_resp_ready_o,
    input  var logic                 pc_lu_hit_i,
    input  var logic [SLOTW-1:0]     pc_lu_slot_i,

    // ---- to FIELD.PROGCACHE's commit phase ----------------------------------
    output var logic                 pc_cm_valid_o,
    input  var logic                 pc_cm_ready_i,
    output var logic [31:0]          pc_cm_hash_o,
    output var logic                 pc_cm_ok_o,
    input  var logic                 pc_cm_resp_valid_i,
    output var logic                 pc_cm_resp_ready_o,
    input  var logic                 pc_cm_inserted_i,
    input  var logic                 pc_cm_evicted_i,
    input  var logic [SLOTW-1:0]     pc_cm_slot_i,

    // ---- to zhao_field_loader: the FH2 transaction (op 3) -------------------
    // Every field here was captured AT POST ACCEPTANCE, law 5. The loader
    // re-captures on its own accept and never reads a live pin either, so the
    // live-pin pattern stops at this boundary rather than being handed across
    // it -- which is what directive 10.2 asks for in as many words.
    output var logic                 fh2_valid_o,
    input  var logic                 fh2_ready_i,
    output var logic [1:0]           fh2_kind_o,
    output var logic [7:0]           fh2_verb_o,
    output var logic [95:0]          fh2_data_o,
    output var logic [31:0]          fh2_hash_o,
    output var logic [31:0]          fh2_ticket_o,
    output var logic [31:0]          fh2_plan_o,
    input  var logic                 fh2_resp_valid_i,
    output var logic                 fh2_resp_ready_o,
    input  var logic                 fh2_resp_ok_i,
    input  var logic [3:0]           fh2_resp_verdict_i,
    input  var logic [31:0]          fh2_resp_handle_i,
    input  var logic [SLOTW-1:0]     fh2_resp_slot_i,
    input  var logic                 fh2_resp_evicted_i,

    // ---- D2: returns --------------------------------------------------------
    output var logic                 ret_valid_o,
    input  var logic                 ret_ready_i,
    output var logic [31:0]          ret_ticket_o,
    output var logic [1:0]           ret_op_o,        // the post it answers
    output var logic                 ret_ok_o,        // the directory took it
    output var logic                 ret_refused_o,   // law 2: header not written
    // COMMIT: the entry was inserted. LOOKUP: the hash is resident. One bit
    // because they are the same question -- "is this hash now in that slot" --
    // and two would need a rule about which to read.
    output var logic                 ret_inserted_o,
    output var logic                 ret_evicted_o,
    output var logic [SLOTW-1:0]     ret_slot_o,
    output var logic [31:0]          ret_plan_o,
    // FH2 only: the loader's verdict and the binding handle it issued. Zero on
    // a legacy return, which is why `ret_op_o` has to be read alongside it --
    // a verdict of 0 means V_OK for an FH2 record and means nothing at all for
    // a LOAD/COMMIT/LOOKUP one.
    output var logic [3:0]           ret_verdict_o,
    output var logic [31:0]          ret_handle_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] posts_o,            // posts consumed, all three kinds
    output var logic [31:0] load_words_o,       // words handed to the loader
    output var logic [31:0] lookups_o,          // lookups handed to the directory
    output var logic [31:0] commits_o,          // commits handed to the directory
    output var logic [31:0] commits_refused_o,  // law 2 refusals, answered
    output var logic [31:0] post_stalls_o,      // cycles a post was held, never dropped
    output var logic [31:0] ret_overflow_o,     // unreachable by credit; see the mutant
    output var logic [31:0] fh2_posts_o,        // op-3 posts handed to the loader
    // A LEGACY LOAD whose address does not fit LDADDRW. Counted and answered
    // rather than truncated -- see POSTADDRW above. Reachable with legal
    // stimulus (post an address >= 2**LDADDRW), so this one needs no mutant.
    output var logic [31:0] addr_refused_o
);

  localparam logic [2:0] LdHeader = 3'd2;

  localparam logic [1:0] OpLoad   = 2'd0;
  localparam logic [1:0] OpCommit = 2'd1;
  localparam logic [1:0] OpLookup = 2'd2;
  localparam logic [1:0] OpFh2    = 2'd3;

  localparam int unsigned PTRW = (POSTS > 1) ? $clog2(POSTS) : 1;
  localparam int unsigned RETW = (RETQ  > 1) ? $clog2(RETQ)  : 1;

  // Quartus 17.0 rejects a bare module-scope `if`; and `--lint-only` does not
  // run these, so a clean lint says nothing whatever about them (CLAUDE.md,
  // 2026-09-08).
  initial begin
    if ((1 << SLOTW) < int'(PROGS)) begin
      $fatal(1, "zhao_field_doorbell: SLOTW=%0d cannot address PROGS=%0d", SLOTW, PROGS);
    end
    if (POSTS < 1) $fatal(1, "zhao_field_doorbell: POSTS must be at least 1");
    if (RETQ  < 1) $fatal(1, "zhao_field_doorbell: RETQ must be at least 1");
  end

  // ==========================================================================
  // THE POSTED MAILBOX
  // ==========================================================================
  logic [1:0]           q_op     [0:POSTS-1];
  logic [2:0]           q_kind   [0:POSTS-1];
  logic [SLOTW-1:0]     q_slot   [0:POSTS-1];
  logic [POSTADDRW-1:0] q_addr   [0:POSTS-1];
  logic [95:0]          q_data   [0:POSTS-1];
  logic [31:0]          q_hash   [0:POSTS-1];
  logic                 q_ok     [0:POSTS-1];
  logic [31:0]          q_ticket [0:POSTS-1];
  // LAW 5. The plan identity belongs to the post, not to the drain. It is
  // captured by the SAME enable as `q_ticket`, one line below it, so the two
  // quantities a return pairs move together and a stall cannot separate them.
  logic [31:0]          q_plan   [0:POSTS-1];

  logic [PTRW:0] q_wr, q_rd;
  wire  [PTRW:0] q_used  = q_wr - q_rd;
  wire           q_full  = (q_used == (PTRW+1)'(POSTS));
  wire           q_empty = (q_wr == q_rd);

  wire [PTRW-1:0] q_wi = q_wr[PTRW-1:0];
  wire [PTRW-1:0] q_ri = q_rd[PTRW-1:0];

  // ==========================================================================
  // THE RETURN QUEUE, AND THE CREDIT THAT MAKES IT UNOVERFLOWABLE
  // ==========================================================================
  logic [31:0]       r_ticket [0:RETQ-1];
  logic [1:0]        r_op     [0:RETQ-1];
  logic              r_ok     [0:RETQ-1];
  logic              r_refused[0:RETQ-1];
  logic              r_ins    [0:RETQ-1];
  logic              r_evi    [0:RETQ-1];
  logic [SLOTW-1:0]  r_slot   [0:RETQ-1];
  logic [31:0]       r_plan   [0:RETQ-1];
  logic [3:0]        r_verdict[0:RETQ-1];
  logic [31:0]       r_handle [0:RETQ-1];

  logic [RETW:0] r_wr, r_rd;
  wire  [RETW:0] r_used  = r_wr - r_rd;
  wire           r_empty = (r_wr == r_rd);
  wire [RETW-1:0] r_wi = r_wr[RETW-1:0];
  wire [RETW-1:0] r_ri = r_rd[RETW-1:0];

  // Consumed commits that have not yet written their return record. THE CREDIT.
  logic [RETW:0] owed;
  wire           ret_credit = (owed + r_used) < (RETW+1)'(RETQ);

  // ==========================================================================
  // THE HEADER SHADOW -- law 2
  // ==========================================================================
  // The same bit `zhao_field_host` keeps as `hdr_loaded`, kept here because the
  // host does not export it and the refusal has to happen BEFORE the directory
  // is offered a hash. It is a shadow and not a second opinion: both are set by
  // the SAME event (a HEADER word accepted by the loader) and cleared by the
  // same one (any other load word to that slot). The host additionally clears
  // it on an insert, which this block cannot see -- and must not, because the
  // commit whose insert clears it is the commit being judged.
  logic [PROGS-1:0] hdr_written;

  // ==========================================================================
  // THE DRAIN
  // ==========================================================================
  typedef enum logic [2:0] {
    D_IDLE, D_LOAD, D_CM, D_CM_RESP, D_LU, D_LU_RESP, D_FH2, D_FH2_RESP
  } dstate_e;
  dstate_e dstate;

  // THE EXHAUSTIVE DECODE (law 6). Four ops, four named predicates, and the
  // `default` arm of the case below is a REFUSAL rather than a fall-through to
  // the loader. `head_is_load` is stated positively on purpose: the old code
  // said "not a commit and not a lookup, therefore a load", which is the
  // reasoning that made op 3 a load.
  wire head_is_load   = (q_op[q_ri] == OpLoad);
  wire head_is_commit = (q_op[q_ri] == OpCommit);
  wire head_is_lookup = (q_op[q_ri] == OpLookup);
  wire head_is_fh2    = (q_op[q_ri] == OpFh2);
  wire head_hdr_ok    = hdr_written[q_slot[q_ri]];

  // A legacy LOAD address that does not fit the loader's own width. Refused
  // and counted; never narrowed. Constant-false when POSTADDRW == LDADDRW, and
  // that is correct rather than dead -- the console composes them different.
  wire head_addr_bad  = head_is_load &&
                        (POSTADDRW > LDADDRW) &&
                        (|q_addr[q_ri][POSTADDRW-1:LDADDRW]);

  // A commit whose header was never written is refused WITHOUT touching the
  // directory, and it still costs a return record -- so it takes credit exactly
  // like an accepted one. An answer that needed no credit would be the silent
  // path this law exists to remove.
  wire head_refuse = head_is_commit && !head_hdr_ok;

  assign ld_valid_o = (dstate == D_LOAD);
  assign ld_kind_o  = q_kind[q_ri];
  assign ld_slot_o  = q_slot[q_ri];
  // Narrowed only on the path that has already been proved to fit, above.
  assign ld_addr_o  = q_addr[q_ri][LDADDRW-1:0];
  assign ld_data_o  = q_data[q_ri];

  // ---- the FH2 seam -------------------------------------------------------
  assign fh2_valid_o      = (dstate == D_FH2);
  // THE LOW TWO BITS, deliberately. The FH2 transaction has four kinds
  // (0 INSTALL_CAPSULE, 1 BIND_PROGRAM, 2 CONTROL, 3 reserved) and the legacy
  // LOAD path has eight; they share this mailbox field and they are not the
  // same quantity. A post that reaches the FH2 arm carries a kind in 0..3, so
  // the truncation is exact for every legal FH2 command -- and an illegal one
  // is refused by `u_field_loader`'s `bad_operation_o`, which is where an
  // unknown FH2 kind is supposed to be counted.
  assign fh2_kind_o       = q_kind[q_ri][1:0];
  assign fh2_verb_o       = 8'(q_addr[q_ri]);
  assign fh2_data_o       = q_data[q_ri];
  assign fh2_hash_o       = q_hash[q_ri];
  assign fh2_ticket_o     = q_ticket[q_ri];
  assign fh2_plan_o       = q_plan[q_ri];
  assign fh2_resp_ready_o = (dstate == D_FH2_RESP);

  assign pc_cm_valid_o = (dstate == D_CM);
  assign pc_cm_hash_o  = q_hash[q_ri];
  assign pc_cm_ok_o    = q_ok[q_ri];

  assign pc_cm_resp_ready_o = (dstate == D_CM_RESP);

  assign pc_lu_valid_o      = (dstate == D_LU);
  assign pc_lu_hash_o       = q_hash[q_ri];
  assign pc_lu_resp_ready_o = (dstate == D_LU_RESP);

  assign post_ready_o = !q_full;

  assign ret_valid_o    = !r_empty;
  assign ret_ticket_o   = r_ticket[r_ri];
  assign ret_op_o       = r_op[r_ri];
  assign ret_ok_o       = r_ok[r_ri];
  assign ret_refused_o  = r_refused[r_ri];
  assign ret_inserted_o = r_ins[r_ri];
  assign ret_evicted_o  = r_evi[r_ri];
  assign ret_slot_o     = r_slot[r_ri];
  assign ret_plan_o     = r_plan[r_ri];
  assign ret_verdict_o  = r_verdict[r_ri];
  assign ret_handle_o   = r_handle[r_ri];

  // The ticket of the post currently at the directory, held across the
  // response so the return names the post that caused it. Captured ONCE, at the
  // cycle the post is consumed, and never re-loaded while the response is
  // outstanding -- the property CLAUDE.md's metadata-swap chapter requires of
  // any join.
  //
  // THAT PARAGRAPH WAS TRUE OF THIS STAGE AND FALSE OF THE PATH, which is the
  // thing worth remembering. It correctly protected ticket-versus-response
  // across the directory's latency, and it sat directly above a `cm_plan`
  // assignment that read `cfg_plan_base_i` LIVE -- so the pairing it defended
  // was already wrong by the time it got here, broken one stage upstream in
  // the mailbox. `cm_plan` now comes from `q_plan[q_ri]`, captured with the
  // ticket at intake, so the guarantee holds end to end instead of locally.
  logic [31:0]     cm_ticket;
  logic [31:0]     cm_plan;
  logic [1:0]      cm_op;

  integer i;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      q_wr <= '0;
      q_rd <= '0;
      r_wr <= '0;
      r_rd <= '0;
      owed <= '0;
      hdr_written <= '0;
      dstate <= D_IDLE;
      cm_ticket <= 32'd0;
      cm_plan <= 32'd0;
      cm_op <= OpLoad;
      posts_o <= 32'd0;
      load_words_o <= 32'd0;
      lookups_o <= 32'd0;
      commits_o <= 32'd0;
      commits_refused_o <= 32'd0;
      post_stalls_o <= 32'd0;
      ret_overflow_o <= 32'd0;
      for (i = 0; i < int'(POSTS); i = i + 1) begin
        q_op[i] <= OpLoad;
        q_kind[i] <= 3'd0;
        q_slot[i] <= '0;
        q_addr[i] <= '0;
        q_data[i] <= 96'd0;
        q_hash[i] <= 32'd0;
        q_ok[i] <= 1'b0;
        q_ticket[i] <= 32'd0;
        q_plan[i] <= 32'd0;
      end
      for (i = 0; i < int'(RETQ); i = i + 1) begin
        r_ticket[i] <= 32'd0;
        r_op[i] <= OpLoad;
        r_ok[i] <= 1'b0;
        r_refused[i] <= 1'b0;
        r_ins[i] <= 1'b0;
        r_evi[i] <= 1'b0;
        r_slot[i] <= '0;
        r_plan[i] <= 32'd0;
        r_verdict[i] <= 4'd0;
        r_handle[i] <= 32'd0;
      end
      fh2_posts_o <= 32'd0;
      addr_refused_o <= 32'd0;
    end else begin
      // ---- D1 intake ------------------------------------------------------
      if (post_valid_i && post_ready_o) begin
        q_op[q_wi]        <= post_op_i;
        q_kind[q_wi]      <= post_kind_i;
        q_slot[q_wi]      <= post_slot_i;
        q_addr[q_wi]      <= post_addr_i;
        q_data[q_wi]      <= post_data_i;
        q_hash[q_wi]      <= post_hash_i;
        q_ok[q_wi]        <= post_ok_i;
        q_ticket[q_wi]    <= post_ticket_i;
        // LAW 5, and the whole of FT061. Same enable, same cycle, same row as
        // the ticket. The plan a return reports is now the plan that was live
        // when the post was ACCEPTED, which is the only reading under which the
        // D0 comment above -- "it travels out on every return so a return can
        // be tied to the plan that produced it" -- is actually true.
        q_plan[q_wi]      <= cfg_plan_base_i;
        q_wr <= q_wr + 1'b1;
        if (posts_o != 32'hFFFF_FFFF) posts_o <= posts_o + 32'd1;
      end else if (post_valid_i && !post_ready_o) begin
        // HELD, not dropped. The count is cycles, and it is the number that
        // says whether POSTS is big enough.
        if (post_stalls_o != 32'hFFFF_FFFF) post_stalls_o <= post_stalls_o + 32'd1;
      end

      // ---- D2 drain -------------------------------------------------------
      if (ret_valid_o && ret_ready_i) r_rd <= r_rd + 1'b1;

      // ---- the drain FSM --------------------------------------------------
      case (dstate)
        D_IDLE: begin
          if (!q_empty) begin
            if (head_refuse || head_addr_bad) begin
              // Law 2 (a commit with no header) and the POSTADDRW refusal, both
              // answered here in one cycle and counted. Neither the directory
              // nor the loader ever sees the request.
              if (ret_credit) begin
                r_ticket[r_wi]  <= q_ticket[q_ri];
                r_op[r_wi]      <= q_op[q_ri];
                r_ok[r_wi]      <= 1'b0;
                r_refused[r_wi] <= 1'b1;
                r_ins[r_wi]     <= 1'b0;
                r_evi[r_wi]     <= 1'b0;
                r_slot[r_wi]    <= q_slot[q_ri];
                r_plan[r_wi]    <= q_plan[q_ri];   // LAW 5
                r_verdict[r_wi] <= 4'd0;
                r_handle[r_wi]  <= 32'd0;
                r_wr <= r_wr + 1'b1;
                q_rd <= q_rd + 1'b1;
                if (head_refuse) begin
                  if (commits_refused_o != 32'hFFFF_FFFF) begin
                    commits_refused_o <= commits_refused_o + 32'd1;
                  end
                end else begin
                  if (addr_refused_o != 32'hFFFF_FFFF) begin
                    addr_refused_o <= addr_refused_o + 32'd1;
                  end
                end
              end
            end else if (head_is_commit || head_is_lookup || head_is_fh2) begin
              if (ret_credit) begin
                cm_ticket <= q_ticket[q_ri];
                cm_plan   <= q_plan[q_ri];         // LAW 5
                cm_op     <= q_op[q_ri];
                owed      <= owed + 1'b1;
                if (head_is_fh2) begin
                  dstate <= D_FH2;
                  if (fh2_posts_o != 32'hFFFF_FFFF) fh2_posts_o <= fh2_posts_o + 32'd1;
                end else begin
                  dstate <= head_is_lookup ? D_LU : D_CM;
                end
              end
            end else if (head_is_load) begin
              // LAW 6: the LOADER IS REACHED ONLY BY A DECODED LOAD. This arm
              // used to be the catch-all `else`, and that is precisely how op 3
              // became a uop/table/header/uniform write.
              dstate <= D_LOAD;
            end else begin
              // UNREACHABLE while `post_op_i` is two bits, and kept anyway:
              // FH14 asks for EXHAUSTIVE decoding, and an exhaustive decode
              // whose last arm silently loads is the defect this replaced. If
              // the field is ever widened, an unknown op lands here and is
              // refused, not loaded. `tests/mutants/
              // zhao_field_doorbell_op3_alias_mutant.sv` restores the old
              // catch-all and its driver must go red.
              if (ret_credit) begin
                r_ticket[r_wi]  <= q_ticket[q_ri];
                r_op[r_wi]      <= q_op[q_ri];
                r_ok[r_wi]      <= 1'b0;
                r_refused[r_wi] <= 1'b1;
                r_ins[r_wi]     <= 1'b0;
                r_evi[r_wi]     <= 1'b0;
                r_slot[r_wi]    <= q_slot[q_ri];
                r_plan[r_wi]    <= q_plan[q_ri];
                r_verdict[r_wi] <= 4'd8;           // the loader's V_BAD_OPERATION
                r_handle[r_wi]  <= 32'd0;
                r_wr <= r_wr + 1'b1;
                q_rd <= q_rd + 1'b1;
                if (addr_refused_o != 32'hFFFF_FFFF) begin
                  addr_refused_o <= addr_refused_o + 32'd1;
                end
              end
            end
          end
        end

        D_LOAD: begin
          if (ld_ready_i) begin
            // The shadow of the host's own runnable bit. Set by a HEADER,
            // cleared by anything else -- the host's law, mirrored.
            if (ld_kind_o == LdHeader) hdr_written[ld_slot_o] <= 1'b1;
            else                       hdr_written[ld_slot_o] <= 1'b0;
            q_rd <= q_rd + 1'b1;
            if (load_words_o != 32'hFFFF_FFFF) load_words_o <= load_words_o + 32'd1;
            dstate <= D_IDLE;
          end
        end

        D_CM: begin
          if (pc_cm_ready_i) begin
            if (commits_o != 32'hFFFF_FFFF) commits_o <= commits_o + 32'd1;
            dstate <= D_CM_RESP;
          end
        end

        D_CM_RESP: begin
          if (pc_cm_resp_valid_i) begin
            // The space was reserved when the commit was consumed, so this
            // write cannot be refused. `ret_overflow_o` says so if it ever is.
            r_ticket[r_wi]  <= cm_ticket;
            r_op[r_wi]      <= cm_op;
            r_ok[r_wi]      <= 1'b1;
            r_refused[r_wi] <= 1'b0;
            r_ins[r_wi]     <= pc_cm_inserted_i;
            r_evi[r_wi]     <= pc_cm_evicted_i;
            r_slot[r_wi]    <= pc_cm_slot_i;
            r_plan[r_wi]    <= cm_plan;
            r_wr <= r_wr + 1'b1;
            owed <= owed - 1'b1;
            // An insert makes the slot the directory's; the header it was
            // promised against has been spent, so the next commit for that slot
            // needs its own header. Mirrors the host clearing `hdr_loaded` on
            // an insert.
            if (pc_cm_inserted_i) hdr_written[pc_cm_slot_i] <= 1'b0;
            q_rd <= q_rd + 1'b1;
            dstate <= D_IDLE;
            if (r_used == (RETW+1)'(RETQ)) begin
              if (ret_overflow_o != 32'hFFFF_FFFF) begin
                ret_overflow_o <= ret_overflow_o + 32'd1;
              end
            end
          end
        end

        // --------------------------------------------------------------------
        // THE FH2 TRANSACTION. The doorbell reserved the return record before
        // handing the command to the loader, so the loader's reply has
        // somewhere to go by construction -- the same credit law as a commit,
        // and the reason FT070's "exactly one terminal reply" is structural
        // rather than hoped for.
        // --------------------------------------------------------------------
        D_FH2: begin
          if (fh2_ready_i) begin
            dstate <= D_FH2_RESP;
          end
        end

        D_FH2_RESP: begin
          if (fh2_resp_valid_i) begin
            r_ticket[r_wi]  <= cm_ticket;
            r_op[r_wi]      <= cm_op;
            // `ok` says the LOADER ANSWERED; `refused` says what it answered.
            // Collapsing them would make a BAD_CRC indistinguishable from a
            // loader that never replied, which is the silence R20 forbids and
            // the same distinction the lookup path draws below.
            r_ok[r_wi]      <= fh2_resp_ok_i;
            r_refused[r_wi] <= !fh2_resp_ok_i;
            r_ins[r_wi]     <= fh2_resp_ok_i;
            r_evi[r_wi]     <= fh2_resp_evicted_i;
            r_slot[r_wi]    <= fh2_resp_slot_i;
            r_plan[r_wi]    <= cm_plan;
            r_verdict[r_wi] <= fh2_resp_verdict_i;
            r_handle[r_wi]  <= fh2_resp_handle_i;
            r_wr <= r_wr + 1'b1;
            owed <= owed - 1'b1;
            q_rd <= q_rd + 1'b1;
            dstate <= D_IDLE;
            if (r_used == (RETW+1)'(RETQ)) begin
              if (ret_overflow_o != 32'hFFFF_FFFF) begin
                ret_overflow_o <= ret_overflow_o + 32'd1;
              end
            end
          end
        end

        D_LU: begin
          if (pc_lu_ready_i) begin
            if (lookups_o != 32'hFFFF_FFFF) lookups_o <= lookups_o + 32'd1;
            dstate <= D_LU_RESP;
          end
        end

        D_LU_RESP: begin
          if (pc_lu_resp_valid_i) begin
            // A MISS IS AN ANSWER, not a failure: `ret_ok_o` says the directory
            // answered, `ret_inserted_o` says whether the hash is resident.
            // Collapsing the two would make "not resident" indistinguishable
            // from "the doorbell never asked", which is the silence R20 forbids.
            r_ticket[r_wi]  <= cm_ticket;
            r_op[r_wi]      <= cm_op;
            r_ok[r_wi]      <= 1'b1;
            r_refused[r_wi] <= 1'b0;
            r_ins[r_wi]     <= pc_lu_hit_i;
            r_evi[r_wi]     <= 1'b0;
            r_slot[r_wi]    <= pc_lu_slot_i;
            r_plan[r_wi]    <= cm_plan;
            r_wr <= r_wr + 1'b1;
            owed <= owed - 1'b1;
            q_rd <= q_rd + 1'b1;
            dstate <= D_IDLE;
            if (r_used == (RETW+1)'(RETQ)) begin
              if (ret_overflow_o != 32'hFFFF_FFFF) begin
                ret_overflow_o <= ret_overflow_o + 32'd1;
              end
            end
          end
        end

        default: dstate <= D_IDLE;
      endcase
    end
  end

endmodule
