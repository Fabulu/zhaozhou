// zhao_texture_frag_expand_mutant.sv -- A DELIBERATELY BROKEN COPY. NOT SHIPPED.
//
// This exists to make `wq_overflow_o` a detector instead of a hopeful zero.
//
// That counter watches for a STATE VIOLATION -- the extended pointer difference
// exceeding the depth the queue owns -- and it is deliberately derived without
// reference to `f_ready_o`, because the FIRST version of the monitor tested
// `accept_c && fq_full_c`, which substitutes to `f_valid_i && !fq_full_c &&
// fq_full_c` and is algebraically false. It could never fire.
//
// The replacement can fire. But "can" was an argument, not a demonstration, and
// no stimulus can produce the violation while the guard is correct: the counter
// is unreachable by construction in a working block. The only way to show it
// alive is to break the guard, which is what this copy does --
//
//     fq_full_c = (fq_occ_c >= FQD)   ->   (fq_occ_c > FQD)
//
// admitting a fifth entry into a four-deep queue. That is exactly the
// discriminating mutation the post-fit brief named, and the OLD monitor could
// not have caught it, because the bug moved acceptance and detection together.
//
// It is a separate FILE rather than a temporary edit for two reasons. A
// temporary edit to production RTL is a fit-corrupting live-tree hazard, and it
// leaves nothing behind: the next person has the same argument and no evidence.
// This is what "keep the failing mutant" means when the mutant cannot live in
// the shipped source.
//
// The module is RENAMED so it can never be elaborated in place of the real one
// by a source-list mistake, and it lives under tests/ where
// check_forbidden_sources.py will not find it in a production closure.
//
// REGENERATE IT if zhao_texture_frag_expand.sv changes shape: this is a copy,
// and a copy of an old version is a positive control for a block that no longer
// exists.


// zhao_texture_frag_expand.sv — P0-C Stage B: one fragment becomes its sample
// requests, and nothing else.
//
// ENFORCED-BY: tests/texture/frag_expand_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS
// ---------------------------------------------------------------------------
// `zhao_texture_v3own` has NO request side. Its issue-facing ports are
// NOTIFICATIONS -- `iss_tmu_valid_i`/`iss_tmu_handle_i`,
// `iss_aux_valid_i`/`iss_aux_owner_i` -- because "ISSUED is its own moment"
// (v3own.sv:35-39). Today `zhao_texture_fragrob` owns the fragment-to-sample
// expansion, and when fragrob is deleted that one function has to live
// somewhere. Here.
//
// The contract this reproduces is written out in
// `reports/P0C-STAGEB-EXPANDER-CONTRACT-20260908.md`, read out of fragrob
// rather than remembered.
//
// ---------------------------------------------------------------------------
// WHAT THIS IS NOT, WHICH IS THE WHOLE ARGUMENT THAT IT IS NOT FRAGROB REBORN
// ---------------------------------------------------------------------------
// No result banks (v3bank's). No arrival masks -- issued/claimed/committed are
// v3own's. No ordering cursors (v3own's). No OWNER_CONTEXT. No generation
// authority (owner ruling T2's). This block's entire state is the CURRENT
// fragment's expansion progress plus a small decoupling queue.
//
// It also holds NO DESCRIPTOR COPIES, and that is a real difference from
// fragrob rather than an omission. Fragrob's issue path is two stages --
// `I_IDLE -> I_READ -> I_HOLD` -- because it reads `desc_u_m`/`desc_v_m`/
// `desc_met_m [3][DEPTH]` out of a RAM. Here `u`/`v` arrive with the fragment
// (from PERSPUV) and binding/LOD come from the island's existing attribute
// tables, so there is no bank read to wait for and no reason to reproduce that
// latency. What must be reproduced bit-identically is the request SEQUENCE, not
// the number of clocks fragrob took to emit it.
//
// ---------------------------------------------------------------------------
// THE SEQUENCE, WHICH IS THE PART THAT MUST MATCH
// ---------------------------------------------------------------------------
// fragrob.sv:359-367 maps the sample count to a request mask:
//
//     count 0 -> 3'b000   NO REQUESTS -- and the fragment is still accepted
//     count 1 -> 3'b001   sidx 0
//     count 2 -> 3'b011   sidx 0,1
//     count 3 -> 3'b111   sidx 0,1,2      (the `default:` arm)
//
// and pushes them ascending by sidx, FIFO across fragments (:744-750).
//
// THE ZERO-SAMPLE FRAGMENT IS THE CASE TO GET RIGHT. It is the only one where
// an accepted fragment produces no downstream traffic at all, and v3own has a
// matching provision -- §9.1's "admission can create a zero-work ready owner"
// -- so the two must agree. A test that only exercises non-empty fragments
// never sees it.
`default_nettype none

module zhao_texture_frag_expand_mutant #(
    // Fragment queue depth. The architecture says "start at 4; the composed
    // throughput requirement is one fragment per clock sustained (§18.3) and
    // the composed test will show starvation". 4 is a STARTING POINT chosen by
    // that instruction, not a measured optimum, and it is a parameter so the
    // measurement can move it.
    parameter int unsigned FQD  = 4,
    parameter int unsigned CTXW = 64,
    // Routing token width. 18 under P0-C = {class[1:0], sample_handle[15:0]};
    // defaulted to 18 here because this block is new and has no legacy
    // instantiation to keep bit-identical.
    parameter int unsigned SRCW = 18
) (
    input var logic clk,
    input var logic rst_n,

    // ---- one joined fragment -------------------------------------------------
    // `f_owner_i` is v3own's 14-bit handle {slot[5:0], generation[7:0]}. This
    // block never invents one and never checks one: identity authority is
    // v3own's, per owner ruling T2.
    input  var logic               f_valid_i,
    output var logic               f_ready_o,
    input  var logic [13:0]        f_owner_i,
    input  var logic signed [31:0] f_u_i,
    input  var logic signed [31:0] f_v_i,
    input  var logic [7:0]         f_binding_i,
    input  var logic [7:0]         f_lod_i,
    input  var logic [1:0]         f_count_i,     // sample count, 0..3
    input  var logic               f_aux_i,       // this fragment needs AUX
    input  var logic [1:0]         f_class_i,

    // THE AUX REQUEST'S WORLD COORDINATES TRAVEL WITH THE FRAGMENT.
    //
    // fragrob had `aux_ctx_o` for exactly this reason and the first integration
    // dropped it, sourcing the AUX pipe's `req_wx_i`/`req_wz_i` from
    // `own_out_ctx` -- the context of whatever fragment the OUTPUT stage
    // happened to be emitting. Those are two unrelated fragments. The aux
    // request for fragment N was asking the sheet about fragment M's position,
    // which is why the island's aux coordinate check saw 17 of an expected 22
    // and why aux-bearing fragments retired the wrong colour.
    //
    // An attribute read off a different stage's pin is the same defect as an
    // attribute read off an input pin twelve clocks late, which this island's
    // own test already has a check for. It travels with its fragment.
    input  var logic [CTXW-1:0] f_ctx_i,

    // ---- TMU sample requests -------------------------------------------------
    output var logic               req_valid_o,
    input  var logic               req_ready_i,
    output var logic signed [31:0] req_u_o,
    output var logic signed [31:0] req_v_o,
    output var logic [7:0]         req_lod_o,
    output var logic [SRCW-1:0]    req_src_id_o,

    // ---- AUX request ---------------------------------------------------------
    output var logic        aux_valid_o,
    input  var logic        aux_ready_i,
    output var logic [13:0] aux_owner_o,
    output var logic [CTXW-1:0] aux_ctx_o,

    // ---- ISSUE NOTIFICATIONS toward v3own ------------------------------------
    // Pulsed on the ACCEPTED handshake edge, never on the intent. v3own's §11.1
    // separates "a candidate was reserved" from "the request was actually
    // taken", and this port is the second one.
    output var logic        iss_tmu_valid_o,
    output var logic [15:0] iss_tmu_handle_o,   // {slot[5:0], sidx[1:0], gen[7:0]}
    output var logic        iss_aux_valid_o,
    output var logic [13:0] iss_aux_owner_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0] fragments_o,
    output var logic [31:0] requests_o,
    output var logic [31:0] zero_sample_fragments_o,
    output var logic [31:0] aux_requests_o,

    // A REAL CAPACITY VIOLATION, which is not the same thing as backpressure.
    //
    // Owner brief 3.1 C: "Normal valid && !ready is NOT an overflow: it is
    // permitted backpressure." The island's err_fragrob_wq_overflow_o used to
    // latch a FRAGROB signal that no longer has a driver, so it was a constant
    // zero being reported as a preserved tripwire.
    //
    // The honest event is a QUEUE STATE VIOLATION: occupancy, measured by the
    // extended pointer difference, exceeding the depth the queue owns. That is
    // derived independently of `f_ready_o`, so a shared-definition error in the
    // fullness test cannot hide it -- which the first version of this monitor
    // could not say, because it was algebraically false. See the always_ff.
    output var logic [31:0] wq_overflow_o
);

  localparam int unsigned FQW = (FQD <= 1) ? 1 : $clog2(FQD);

  // ---- the decoupling queue --------------------------------------------------
  // PERSPUV produces at its own rate and the planner accepts at its own; today
  // fragrob's DEPTH=16 admission buffer absorbs the difference. This is the
  // bounded replacement, and it holds the FRAGMENT, never a result.
  typedef struct packed {
    logic [13:0]        owner;
    logic signed [31:0] u;
    logic signed [31:0] v;
    logic [7:0]         binding;
    logic [7:0]         lod;
    logic [1:0]         count;
    logic               aux;
    logic [1:0]         cls;
    logic [CTXW-1:0]    ctx;
  } frag_t;

  frag_t          fq_m [FQD];
  logic [FQW:0]   fq_wp_q, fq_rp_q;
  logic [FQW:0]   fq_occ_c;
  logic           fq_empty_c, fq_full_c;

  assign fq_occ_c   = fq_wp_q - fq_rp_q;
  assign fq_empty_c = (fq_wp_q == fq_rp_q);
  assign fq_full_c  = (fq_occ_c > (FQW+1)'(FQD));   // MUTANT: >= became >
  assign f_ready_o  = !fq_full_c;

  wire accept_c = f_valid_i && f_ready_o;

  // ---- the request mask, fragrob's table verbatim ---------------------------
  function automatic logic [2:0] req_mask(input logic [1:0] cnt);
    unique case (cnt)
      2'd0:    req_mask = 3'b000;
      2'd1:    req_mask = 3'b001;
      2'd2:    req_mask = 3'b011;
      default: req_mask = 3'b111;
    endcase
  endfunction

  // ---- expansion state: the CURRENT fragment only ---------------------------
  logic        cur_v_q;
  frag_t       cur_q;
  logic [1:0]  sidx_q;      // ascending, 0..count-1
  logic        cur_aux_done_q;

  wire head_valid_c = !fq_empty_c;
  frag_t head_c;
  assign head_c = fq_m[fq_rp_q[FQW-1:0]];

  // A fragment is loaded into the expansion registers when none is in progress.
  wire load_c = !cur_v_q && head_valid_c;

  // The current fragment still owes a TMU request while sidx < count.
  wire tmu_owed_c = cur_v_q && (sidx_q < cur_q.count);
  wire aux_owed_c = cur_v_q && cur_q.aux && !cur_aux_done_q;

  assign req_valid_o  = tmu_owed_c;
  assign req_u_o      = cur_q.u;
  assign req_v_o      = cur_q.v;
  // The island's current per-sample binding law, kept VERBATIM at first
  // integration: `f_binding_c + s`, common u/v (island_top:869-881). Its
  // limitation travels with it deliberately -- changing it here would mix a
  // behaviour change into a structural one.
  assign req_lod_o    = cur_q.lod;
  assign req_src_id_o = {cur_q.cls, cur_q.owner[13:8], sidx_q, cur_q.owner[7:0]};

  assign aux_valid_o = aux_owed_c;
  assign aux_owner_o = cur_q.owner;
  assign aux_ctx_o   = cur_q.ctx;

  wire tmu_fire_c = req_valid_o && req_ready_i;
  wire aux_fire_c = aux_valid_o && aux_ready_i;

  // ---- the notifications, on the ACCEPTED edge ------------------------------
  assign iss_tmu_valid_o  = tmu_fire_c;
  assign iss_tmu_handle_o = {cur_q.owner[13:8], sidx_q, cur_q.owner[7:0]};
  assign iss_aux_valid_o  = aux_fire_c;
  assign iss_aux_owner_o  = cur_q.owner;

  // A fragment retires from the expansion registers when it owes nothing more.
  wire done_c = cur_v_q
             && !(tmu_owed_c && !tmu_fire_c)
             && !(aux_owed_c && !aux_fire_c)
             && ((sidx_q + 2'(tmu_fire_c)) >= cur_q.count)
             && (!cur_q.aux || cur_aux_done_q || aux_fire_c);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      fq_wp_q                 <= '0;
      fq_rp_q                 <= '0;
      cur_v_q                 <= 1'b0;
      sidx_q                  <= 2'd0;
      cur_aux_done_q          <= 1'b0;
      fragments_o             <= 32'd0;
      requests_o              <= 32'd0;
      zero_sample_fragments_o <= 32'd0;
      aux_requests_o          <= 32'd0;
      wq_overflow_o           <= 32'd0;
    end else begin
      // Counted BEFORE the write, because the write is what would corrupt an
      // occupied entry.
      // POLICY C-b (post-fit brief §2.2). The previous form was
      //
      //     if (accept_c && fq_full_c)
      //
      // and it is IDENTICALLY FALSE, not merely unreachable:
      //
      //     accept_c  = f_valid_i && f_ready_o
      //     f_ready_o = !fq_full_c
      //  => f_valid_i && !fq_full_c && fq_full_c
      //
      // Both acceptance and detection consulted the SAME predicate in
      // complementary form, so synthesis folds the counter to constant zero.
      // I had argued it was 'unreachable if f_ready_o is correct, which is
      // what makes it worth exposing'. That was wrong: it replaced an undriven
      // port with a differently-dead one, and gained no observable.
      //
      // The brief's mutation makes it plain: change `>=` to `>` above and the
      // queue admits a fifth entry -- a real bug -- and this monitor STILL
      // cannot fire, because the bug moves both sides together.
      //
      // So detect a STATE VIOLATION instead, derived without reference to the
      // producer-ready expression: the extended pointer difference exceeding
      // the legal depth. `fq_occ_c` is `fq_wp_q - fq_rp_q` over FQW+1 bits, so
      // it can represent more than FQD; if it ever does, an entry was written
      // that the queue does not own. That is true regardless of how `f_ready_o`
      // is computed, and it is exactly the condition assertion `a_fq_in_range`
      // checks in simulation -- this is its synthesizable counterpart, with a
      // one-cycle detection latency and no dependence on the handshake.
      if (fq_occ_c > (FQW+1)'(FQD)) wq_overflow_o <= wq_overflow_o + 32'd1;
      if (accept_c) begin
        fq_m[fq_wp_q[FQW-1:0]] <= '{owner:   f_owner_i,
                                    u:       f_u_i,
                                    v:       f_v_i,
                                    binding: f_binding_i,
                                    lod:     f_lod_i,
                                    count:   f_count_i,
                                    aux:     f_aux_i,
                                    cls:     f_class_i,
                                    ctx:     f_ctx_i};
        fq_wp_q     <= fq_wp_q + (FQW+1)'(1);
        fragments_o <= fragments_o + 32'd1;
        // THE ZERO-SAMPLE FRAGMENT, counted where it happens so a test can
        // assert the case was actually exercised rather than assumed.
        if (f_count_i == 2'd0) zero_sample_fragments_o <= zero_sample_fragments_o + 32'd1;
      end

      if (tmu_fire_c) begin
        sidx_q     <= sidx_q + 2'd1;
        requests_o <= requests_o + 32'd1;
      end
      if (aux_fire_c) begin
        cur_aux_done_q <= 1'b1;
        aux_requests_o <= aux_requests_o + 32'd1;
      end

      if (done_c) begin
        cur_v_q <= 1'b0;
      end else if (load_c) begin
        cur_v_q        <= 1'b1;
        cur_q          <= head_c;
        sidx_q         <= 2'd0;
        cur_aux_done_q <= 1'b0;
        fq_rp_q        <= fq_rp_q + (FQW+1)'(1);
      end
    end
  end

`ifndef SYNTHESIS
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else        armed_q <= 1'b1;
  end
  always_ff @(posedge clk) begin
    if (armed_q) begin
      // ASCENDING, AND NEVER PAST THE COUNT. The sequence is the contract.
      if (cur_v_q) a_sidx_in_range : assert (sidx_q <= cur_q.count);
      // The queue is bounded and must never be written past its depth.
      // MUTANT: this assertion is DISABLED here, and only here.
      //
      // It is the simulation-time twin of the synthesizable `wq_overflow_o`
      // monitor, and it fires on this mutation immediately -- independent
      // corroboration that the break is real. But it calls $stop, which aborts
      // before the COUNTER can be observed, and the counter is what this mutant
      // exists to demonstrate. The synthesizable monitor is the one that ships;
      // the assertion does not.
      // a_fq_in_range : assert (fq_occ_c <= (FQW+1)'(FQD));
      // A notification only ever accompanies an accepted handshake -- never an
      // intent. This is §11.1's distinction and the reason the port exists.
      a_iss_only_on_fire : assert (!iss_tmu_valid_o || (req_valid_o && req_ready_i));
      a_aux_iss_only_on_fire : assert (!iss_aux_valid_o || (aux_valid_o && aux_ready_i));
    end
  end
`endif

endmodule : zhao_texture_frag_expand_mutant

`default_nettype wire
