// zhao_texture_fragrob.sv -- the texture island's transaction centre.
//
// ---------------------------------------------------------------------------
// WHY A NEW BLOCK AND NOT AN EDIT
// ---------------------------------------------------------------------------
// reports/islandrearchitecture5.md S6.1, verbatim:
//
//   Create a new production block beside v1 and v2: zhao_texture_fragrob.
//   Do not mutate zhao_raster_texjoin_v2 until it becomes impossible to tell
//   which implementation its tests describe. Keep v2 as the oracle for token
//   allocation, multi-sample completion, generation rejection and
//   allocation-order retirement.
//
// So the port list is deliberately IDENTICAL to v2's: the existing v2 suite
// becomes the differential oracle for this block, and any behavioural
// divergence is a test failure rather than a discussion.
//
// ---------------------------------------------------------------------------
// WHAT WAS WRONG WITH v2, MEASURED
// ---------------------------------------------------------------------------
// v2 fits at 3,824 ALM and 7,151 REGISTERS against a budget of 900 / 1,200.
// Its entry table is 7,056 bits, and 7,151 measured registers is that table
// living entirely in flip-flops. Three causes, all of them storage:
//
//   1. the arrays are written from an ASYNC-RESET process. An M10K has no
//      reset port, so an array touched by `always_ff @(posedge clk or negedge
//      rst_n)` cannot be one. (The texture cache had the identical defect, and
//      the block IT replaced had already diagnosed it in a comment nobody
//      read -- zhao_texture_cache.sv:495-523.)
//   2. the retire path reads `srgb_q[head_q][0]` COMBINATIONALLY through a
//      dynamic index, which forces a 16:1 mux per bit and pins the array in
//      flops whatever the writes do.
//   3. two dynamic write addresses into one array, which S5.3 forbids by name.
//
// ---------------------------------------------------------------------------
// THE STRUCTURE THIS BLOCK USES INSTEAD (S6.3, S6.4)
// ---------------------------------------------------------------------------
// CONTROL STATE IN FLOPS, deliberately: valid, generation, required/arrived
// masks, aux flags, ready flags. "This is a few hundred registers total and
// permits TMU and AUX return events to update independent fields without a RAM
// read-modify-write loop." Wide U/V, context, bindings and colours are
// explicitly NOT allowed in these flops.
//
// PAYLOAD IN BANKS, banked BY SAMPLE INDEX so one accepted three-sample
// fragment writes sample 0, 1 and 2 in the same clock without needing a
// three-write-port memory -- and so the combiner can read all three in
// parallel. Every bank is written and read ONLY inside clock-only processes,
// through registered addresses.
//
// The cost of that is a pipeline stage on issue and on retire, which is why
// both paths below are small state machines rather than combinational picks.
// That is the trade the brief asks for: latency for structure.
//
// ENFORCED-BY: tests/raster/raster_texjoin_v2_directed.cpp:main
`default_nettype none

module zhao_texture_fragrob #(
    parameter int unsigned DEPTH = 16,
    parameter int unsigned CTXW  = 64,
    // Width of the ingress TOKEN, carried as a TYPED FIELD beside the
    // caller's context rather than packed into it. See `f_tok_i`.
    parameter int unsigned TOKW_F = 6,
    parameter int unsigned BINDW = 8,
    parameter int unsigned LODW  = 4,
    // 8 per ruling X5: a 2-bit generation wraps after four reuses, so a return
    // delayed longer than four reuses matches the WRONG fragment and is
    // silently accepted -- worse than the stale return it exists to catch.
    parameter int unsigned GENW  = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- fragment in -------------------------------------------------------
    input  var logic                   f_valid_i,
    output var logic                   f_ready_o,
    input  var logic [1:0]             f_sample_count_i,
    input  var logic signed [31:0]     f_u_i        [3],
    input  var logic signed [31:0]     f_v_i        [3],
    input  var logic [BINDW-1:0]       f_binding_i  [3],
    input  var logic [LODW-1:0]        f_lod_i      [3],
    input  var logic [2:0]             f_recipe_i,
    // THE CALLER'S CONTEXT, CARRIED VERBATIM. Not a place to stash island
    // fields: the caller interprets this word as its own data, so overwriting
    // any of its bits silently destroys something this module cannot see.
    input  var logic [CTXW-1:0]        f_ctx_i,
    // The ingress token, so a consumer downstream of this buffer can still
    // find the fragment's captured attributes -- a separate typed field
    // precisely because the context above must stay untouched.
    input  var logic [TOKW_F-1:0]      f_tok_i,
    input  var logic                   f_aux_i,
    input  var logic                   f_uv_sat_i,

    // ---- TMU request / return ---------------------------------------------
    output var logic                   tmu_valid_o,
    input  var logic                   tmu_ready_i,
    output var logic signed [31:0]     tmu_u_o,
    output var logic signed [31:0]     tmu_v_o,
    output var logic [BINDW-1:0]       tmu_binding_o,
    output var logic [LODW-1:0]        tmu_lod_o,
    output var logic [$clog2(DEPTH)-1:0] tmu_slot_o,
    output var logic [1:0]             tmu_sidx_o,
    output var logic [GENW-1:0]        tmu_gen_o,

    input  var logic                   tmu_rvalid_i,
    output var logic                   tmu_rready_o,
    input  var logic [23:0]            tmu_rgb_i,
    input  var logic [7:0]             tmu_a_i,
    input  var logic [$clog2(DEPTH)-1:0] tmu_rslot_i,
    input  var logic [1:0]             tmu_rsidx_i,
    input  var logic [GENW-1:0]        tmu_rgen_i,

    // ---- AUX request / return ---------------------------------------------
    output var logic                   aux_valid_o,
    input  var logic                   aux_ready_i,
    output var logic [CTXW-1:0]        aux_ctx_o,
    output var logic [$clog2(DEPTH)-1:0] aux_slot_o,
    output var logic [GENW-1:0]        aux_gen_o,
    input  var logic                   aux_rvalid_i,
    output var logic                   aux_rready_o,
    input  var logic [23:0]            aux_rgb_i,
    input  var logic [7:0]             aux_a_i,
    input  var logic [$clog2(DEPTH)-1:0] aux_rslot_i,
    input  var logic [GENW-1:0]        aux_rgen_i,

    // ---- retired fragment out ---------------------------------------------
    output var logic                   o_valid_o,
    input  var logic                   o_ready_i,
    // WHICH SLOT AN ACCEPTED FRAGMENT LANDED IN, valid with `alloc_valid_o`.
    // The island keys per-fragment ROUTING state (the sample class) by slot,
    // because a TMU request identifies its fragment by slot and nothing else
    // travels with it. Without this port the island had to read the class from
    // its own input pin at request time -- which is a DIFFERENT fragment, and
    // is the defect this port exists to make fixable.
    output var logic [SW-1:0]          alloc_slot_o,
    output var logic                   alloc_valid_o,
    output var logic [CTXW-1:0]        o_ctx_o,
    output var logic [TOKW_F-1:0]      o_tok_o,
    output var logic [23:0]            o_rgb_o,
    output var logic [7:0]             o_a_o,
    // ---- the three banked sample results, exposed ------------------------
    // islandrearchitecture5.md line 884: "Sample results must also be banked
    // by sample index so the material combiner can" read them together. They
    // ALREADY WERE banked here -- `res_rgb_m [3][DEPTH]` -- and the retire
    // read took `res_rgb_m[0]` and nothing else, so a three-sample fragment
    // left this block as sample 0. That is the exact behaviour
    // MATERIAL.RESOLVE.md complains about in the surviving TEXJOIN, living one
    // block upstream of where anyone was looking for it.
    //
    // `o_rgb_o`/`o_a_o` are RETAINED and unchanged: they are sample 0, every
    // existing consumer keeps working, and nothing downstream has to move
    // before the combiner is wired.
    output var logic [23:0]            o_s_rgb_o [3],
    output var logic [7:0]             o_s_a_o   [3],
    output var logic [23:0]            o_aux_rgb_o,
    output var logic [7:0]             o_aux_a_o,
    output var logic                   o_has_aux_o,
    output var logic                   o_uv_sat_o,

    // ---- evidence ----------------------------------------------------------
    output var logic [31:0]            fragments_o,
    output var logic [31:0]            samples_o,
    output var logic [31:0]            full_clocks_o,
    output var logic [31:0]            id_errors_o,
    output var logic                   wq_overflow_o,
    output var logic                   id_error_o,
    output var logic                   combiner_unfrozen_o
);

  localparam int unsigned SW   = $clog2(DEPTH);
  localparam int unsigned METW = BINDW + LODW;

  // The recipe encoding, documented here and STORED per slot, but not switched
  // on: the material combiner is a separate block (S3.3 gives it its own
  // budget line) and its arithmetic is explicitly not this block's. Until it
  // exists, every recipe retires sample 0 -- which is v2's behaviour, and is
  // what `combiner_unfrozen_o` is for. A gate that ignores that signal is
  // testing a placeholder.
  /* verilator lint_off UNUSEDPARAM */
  localparam logic [2:0] RECIPE_PASSTHRU   = 3'd0;
  localparam logic [2:0] RECIPE_MODULATE   = 3'd1;
  localparam logic [2:0] RECIPE_MODULATE2X = 3'd2;
  localparam logic [2:0] RECIPE_LERP       = 3'd3;
  localparam logic [2:0] RECIPE_ADD_SAT    = 3'd4;
  localparam logic [2:0] RECIPE_MASK       = 3'd5;
  /* verilator lint_on UNUSEDPARAM */

  // ==========================================================================
  // CONTROL STATE -- FLOPS ON PURPOSE (S6.3)
  // ==========================================================================
  // A few hundred registers. TMU and AUX returns update independent fields of
  // independent slots on the same clock, which a RAM would turn into a
  // read-modify-write loop and a structural hazard.
  logic            val_q    [DEPTH];
  logic [GENW-1:0] gen_q    [DEPTH];
  logic [2:0]      req_q    [DEPTH];
  logic [2:0]      arr_q    [DEPTH];
  logic            auxreq_q [DEPTH];
  logic            auxarr_q [DEPTH];
  // ---- THE ISSUED MASKS, WHICH DID NOT EXIST -----------------------------
  // reports/ZHAOZHOU-PREFIT-VERIFICATION-AND-REARCHITECT-20260906.txt S3.3:
  // "MISSING on the TMU side: sample-index range, requested, issued,
  //  not-already-arrived. THERE IS NO ISSUED MASK IN THE FILE."
  //
  // `req_q` says a sample is WANTED. It does not say a request for it has left
  // the block, so a return that arrives before -- or instead of -- its request
  // was accepted as genuine. `iss_q`/`auxiss_q` are the missing half: set when
  // the request is actually HANDSHAKEN out, cleared when its one legitimate
  // answer is taken. A second copy of the same answer therefore finds its bit
  // already clear and is refused instead of silently overwriting a good sample.
  logic [2:0]      iss_q    [DEPTH];
  logic            auxiss_q [DEPTH];
  // Stored on allocation and carried to retirement, where the material
  // combiner will read it. Unread today for the reason above; the alternative
  // -- not storing it until the combiner exists -- would mean the descriptor
  // that carries it is not actually being captured, which is the harder bug to
  // find later.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [2:0]      recipe_q [DEPTH];
  /* verilator lint_on UNUSEDSIGNAL */
  logic            sat_q    [DEPTH];

  // ==========================================================================
  // PAYLOAD BANKS (S6.4) -- written and read only in clock-only processes
  // ==========================================================================
  // Banked by SAMPLE INDEX, so a three-sample fragment writes all three banks
  // on one clock without a three-write-port memory, and the retire path reads
  // all three in parallel.
  logic signed [31:0] desc_u_m  [3][DEPTH];
  logic signed [31:0] desc_v_m  [3][DEPTH];
  logic [METW-1:0]    desc_met_m[3][DEPTH];
  logic [23:0]        res_rgb_m [3][DEPTH];
  logic [7:0]         res_a_m   [3][DEPTH];
  logic [CTXW-1:0]    ctx_m     [DEPTH];
  logic [TOKW_F-1:0]  tok_m     [DEPTH];
  logic [23:0]        auxrgb_m  [DEPTH];
  logic [7:0]         auxa_m    [DEPTH];

  // ==========================================================================
  // FREE-SLOT FIFO (S6.5) -- no scan
  // ==========================================================================
  // "Use a sixteen-entry free-slot FIFO initialized by a sixteen-cycle sweep
  // after reset/abort. allocation pops one slot, final output acceptance
  // pushes one slot. No free-slot scan."
  //
  // The scan is what a priority encoder over `val_q` would be, and it is
  // exactly the shape that put a 16:1 mux in front of everything in v2.
  logic [SW-1:0] free_m [DEPTH];
  logic [SW:0]   free_wp_q, free_rp_q;
  logic [SW:0]   free_cnt_q;
  logic          init_q;
  logic [SW:0]   init_i_q;

  logic free_empty_c;
  assign free_empty_c = (free_cnt_q == '0);

  // THE LOST-UPDATE FAULT, which this block had and which CLAUDE.md names.
  // `free_cnt_q` and `live_cnt_q` are each touched by allocation AND by
  // retirement. Written as two separate `if` blocks in one always_ff, only the
  // LAST assignment lands, so an accept and a retire on the same clock produce
  // +1 or -1 instead of a net zero -- the count drifts, the free list
  // eventually reports empty while holding slots, and allocation wedges.
  //
  // The differential caught it as exactly DEPTH fragments never retiring. The
  // fix is to compute each net delta ONCE and assign once.
  logic alloc_ev_c, retire_ev_c;
  logic [SW:0] free_cnt_n_c, live_cnt_n_c;
  always_comb begin
    free_cnt_n_c = free_cnt_q;
    live_cnt_n_c = live_cnt_q;
    if (alloc_ev_c && !retire_ev_c) begin
      free_cnt_n_c = free_cnt_q - 1;
      live_cnt_n_c = live_cnt_q + 1;
    end else if (!alloc_ev_c && retire_ev_c) begin
      free_cnt_n_c = free_cnt_q + 1;
      live_cnt_n_c = live_cnt_q - 1;
    end
    // both, or neither: the counts do not move.
  end

  // ==========================================================================
  // WORK QUEUE: samples that still need issuing to the TMU
  // ==========================================================================
  // Power of two, so the occupancy is a pointer subtraction that only counts
  // correctly if the pointers wrap at a multiple of the depth.
  // 64 is not arbitrary and the overflow below is not reachable.
  //
  // At most DEPTH slots are live and each contributes at most three samples,
  // so the queue never holds more than 16 x 3 = 48 entries. WQN must be a
  // POWER OF TWO because the occupancy is a pointer subtraction that only
  // counts correctly if the pointers wrap at a multiple of the depth, and 64
  // is the smallest power of two at or above 48. So the sizing is exactly
  // right and `wq_overflow_o` is defensive, not live.
  //
  // That is stated here rather than left as a "planned test", because a test
  // for an unreachable case can never pass or fail and would sit in the
  // contract forever looking like missing work. The assertion below is the
  // enforcement: if DEPTH or the sample count ever grows past this sizing, it
  // fires in simulation instead of the queue silently wrapping.
  localparam int unsigned WQN = 64;
  localparam int unsigned WQW = $clog2(WQN);
  logic [SW+1:0] wq_m [WQN];
  logic [WQW:0]  wq_wp_q, wq_rp_q;
  logic          wq_empty_c;
  assign wq_empty_c = (wq_wp_q == wq_rp_q);

  // The three write addresses, computed into variables. A part-select of an
  // EXPRESSION -- `(wq_wp_q + WQW'(s))[WQW-1:0]` -- is a syntax error; only a
  // variable can be sliced.
  logic [WQW:0]   wq_wsum_c [3];
  logic [WQW-1:0] wq_wa_c   [3];
  logic [WQW-1:0] wq_ra_c;
  logic [WQW:0]   wq_occ_c;
  always_comb begin
    for (int unsigned s = 0; s < 3; s++) begin
      wq_wsum_c[s] = wq_wp_q + (WQW+1)'(s);
      wq_wa_c[s]   = wq_wsum_c[s][WQW-1:0];
    end
    wq_ra_c  = wq_rp_q[WQW-1:0];
    wq_occ_c = wq_wp_q - wq_rp_q;
  end

  // `rst_n` must not be read SYNCHRONOUSLY in a block while it is also an
  // asynchronous reset elsewhere -- Verilator's SYNCASYNCNET, and it is a real
  // caution rather than a style note: a net used both ways is a net whose
  // timing closure is being asked for twice. `assert_armed_q` is a plain
  // synchronous flag that says "reset has released", which is what the
  // assertions actually want.
  logic assert_armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) assert_armed_q <= 1'b0;
    else        assert_armed_q <= 1'b1;
  end

  // ---- SELF-ASSERTING QUEUE-SIZING GUARD ----------------------------------
  // ENFORCED-BY: fpga/rtl/texture/zhao_texture_fragrob.sv:a_wq_never_overflows
  // The comment above claims WQN is sized so the queue cannot overflow. This
  // is the enforcer that claim cites, in the idiom of zhao_cmd_scheduler's
  // a_mode_act_in_range. It costs nothing in synthesis and catches a future
  // DEPTH or sample-count change that quietly invalidates the sizing.
  always_ff @(posedge clk) begin
    if (assert_armed_q) begin
      a_wq_never_overflows : assert (wq_occ_c <= (WQW+1)'(DEPTH * 3));
    end
  end

  // ==========================================================================
  // ALLOCATION
  // ==========================================================================
  logic [SW-1:0] alloc_slot_c;
  assign alloc_slot_c = free_m[free_rp_q[SW-1:0]];

  logic accept_c;
  assign f_ready_o = !init_q && !free_empty_c;
  assign alloc_slot_o  = alloc_slot_c;
  assign alloc_valid_o = accept_c;
  assign accept_c  = f_valid_i && f_ready_o;
  assign alloc_ev_c = accept_c;

  logic [2:0] req_mask_c;
  always_comb begin
    unique case (f_sample_count_i)
      2'd0:    req_mask_c = 3'b000;
      2'd1:    req_mask_c = 3'b001;
      2'd2:    req_mask_c = 3'b011;
      default: req_mask_c = 3'b111;
    endcase
  end

  // ==========================================================================
  // TMU ISSUE -- a two-stage pipeline, because the descriptor is in a RAM
  // ==========================================================================
  // v2 could pick a descriptor combinationally because it was in flops. The
  // price of the banks is one stage: pop the work queue, register the address,
  // and present the request when the bank answers.
  typedef enum logic [1:0] { I_IDLE, I_READ, I_HOLD } istate_e;
  istate_e       i_st_q;
  logic [SW-1:0] i_slot_q;
  logic [1:0]    i_sidx_q;
  logic [GENW-1:0] i_gen_q;

  // Registered bank read addresses and their outputs.
  logic [SW-1:0]      rd_slot_q;
  logic [1:0]         rd_sidx_q;
  logic signed [31:0] bank_u_r, bank_v_r;
  logic [METW-1:0]    bank_met_r;

  assign tmu_valid_o   = (i_st_q == I_HOLD);
  assign tmu_u_o       = bank_u_r;
  assign tmu_v_o       = bank_v_r;
  assign tmu_binding_o = bank_met_r[METW-1:LODW];
  assign tmu_lod_o     = bank_met_r[LODW-1:0];
  assign tmu_slot_o    = i_slot_q;
  assign tmu_sidx_o    = i_sidx_q;
  assign tmu_gen_o     = i_gen_q;

  // ==========================================================================
  // RETURN ACCEPTANCE -- identity, not timing
  // ==========================================================================
  // A return is matched by {slot, generation}. A stale return -- one whose slot
  // has been reallocated since it was issued -- is REFUSED and counted, never
  // applied. This is the property GENW=8 exists to protect.
  //
  // ---- DEFECT 2: A MATCHING GENERATION IS NOT VALIDATION ------------------
  // ENFORCED-BY: tests/texture/fragrob_differential.cpp (directed section
  //              "an out-of-range / unrequested / duplicate return")
  //
  // WHAT WAS HERE. Two terms on the TMU side -- `val_q` and generation
  // equality -- and three on the AUX side. Verified at
  // reports/ZHAOZHOU-PREFIT-VERIFICATION-AND-REARCHITECT-20260906.txt S3.3 and
  // reports/TEXTURE-ISLAND-PREFIT-ADDENDUM-20260906.txt S5.2.
  //
  // WHY THAT IS FATAL, NOT MERELY LOOSE. `arr_q` is a MASK compared for
  // equality against `req_q`, and retirement is strictly ALLOCATION-ORDERED
  // with no timeout. So a return that sets a bit nobody asked for makes
  // `arr_q != req_q` PERMANENTLY: the head fragment never reports done, the
  // ordered retire never advances past it, and every slot behind it is stuck
  // too. THE WHOLE BLOCK WEDGES. That is the failure mode for all four holes:
  //
  //   sidx == 3          the write and the bit-select are silently DROPPED --
  //                      no bank is corrupted, but the sample is accepted as
  //                      valid, no id_error is counted, and it never arrives.
  //   not requested      sets a bit outside req_q -> arr_q != req_q forever.
  //   not issued         an answer to a question this block never asked; it
  //                      cannot be a genuine result for this owner.
  //   already arrived    a duplicate OVERWRITES a correct sample, and can
  //                      mutate a result after the retire read has started.
  //
  // All six terms, in the order S5.2 names them:
  //   live(owner) && generation_matches && sample_index_in_range
  //   && requested(sample) && issued(sample) && !arrived(sample)
  //
  // `tmu_sidx_c` is the range-CLAMPED index. The three-bit selects and the
  // three-deep banks below are addressed through it and never through the raw
  // return field, so an out-of-range return cannot reach storage at all --
  // being refused by the predicate and being unable to address anything are
  // two independent defences, on purpose.
  logic       tmu_sidx_ok_c;
  logic [1:0] tmu_sidx_c;
  assign tmu_sidx_ok_c = (tmu_rsidx_i != 2'd3);
  assign tmu_sidx_c    = tmu_sidx_ok_c ? tmu_rsidx_i : 2'd0;

  logic tmu_ok_c, aux_ok_c;
  assign tmu_ok_c = tmu_rvalid_i
                 && val_q[tmu_rslot_i]
                 && (gen_q[tmu_rslot_i] == tmu_rgen_i)
                 && tmu_sidx_ok_c
                 && req_q[tmu_rslot_i][tmu_sidx_c]
                 && iss_q[tmu_rslot_i][tmu_sidx_c]
                 && !arr_q[tmu_rslot_i][tmu_sidx_c];
  assign aux_ok_c = aux_rvalid_i
                 && val_q[aux_rslot_i]
                 && (gen_q[aux_rslot_i] == aux_rgen_i)
                 && auxreq_q[aux_rslot_i]
                 && auxiss_q[aux_rslot_i]
                 && !auxarr_q[aux_rslot_i];
  assign tmu_rready_o = 1'b1;
  assign aux_rready_o = 1'b1;

  // ---- DEFECT 3: TWO WRITERS ON ONE COUNTER IN ONE PROCESS ---------------
  // ENFORCED-BY: tests/texture/fragrob_differential.cpp (directed section
  //              "simultaneous TMU and AUX identity faults count TWO")
  //
  // WHAT WAS HERE. `id_errors_o <= id_errors_o + 32'd1` appeared TWICE in the
  // control process -- once in the TMU return branch, once in the AUX return
  // branch. Both are nonblocking assignments to the same register in the same
  // always_ff, so on a clock where BOTH faults occur the last one wins and the
  // counter moves by ONE. Two events, one count. S5.3 / S3.3.
  //
  // This is the SAME defect as the lost-update fault on free_cnt_q documented
  // above, and it takes the same repair: compute the net delta ONCE, in one
  // combinational place, and assign the accumulator ONCE. A telemetry counter
  // that under-reports exactly when the machine is in the most trouble is
  // worse than no counter, because a green evidence line is read as health.
  logic [1:0] id_err_delta_c;
  always_comb begin
    id_err_delta_c = 2'd0;
    if (tmu_rvalid_i && !tmu_ok_c) id_err_delta_c = id_err_delta_c + 2'd1;
    if (aux_rvalid_i && !aux_ok_c) id_err_delta_c = id_err_delta_c + 2'd1;
  end

  // ==========================================================================
  // AUX ISSUE
  // ==========================================================================
  // A QUEUE, not a single pending register. Sixteen fragments may be live and
  // every one of them may want AUX, so a single register silently DROPS the
  // second request and the fragment that needed it never completes -- a
  // deadlock, not a dropped pixel. Found by the differential.
  logic [SW-1:0]   axq_m   [DEPTH];
  logic [GENW-1:0] axg_m   [DEPTH];
  logic [SW:0]     axq_wp_q, axq_rp_q;
  logic            axq_empty_c;
  assign axq_empty_c = (axq_wp_q == axq_rp_q);

  // ---- DEFECT 1: THE AUX CONTEXT WAS ALWAYS THE PREVIOUS FRAGMENT'S ------
  // ENFORCED-BY: tests/texture/fragrob_differential.cpp (directed section
  //              "the AUX context belongs to the AUX slot")
  //
  // WHAT WAS HERE. The pop below wrote `ax_slot_q <= <new slot>` and raised
  // `ax_pend_q` in the same clock, while the bank process ran
  // `ax_ctx_r <= ctx_m[ax_slot_q]` UNCONDITIONALLY. Both nonblocking, so on the
  // pop edge ax_ctx_r loaded ctx_m[OLD slot] and ax_slot_q took the NEW one:
  // valid rose over a tuple whose identity and whose payload came from two
  // different fragments.
  //
  // WHY IT WAS NOT A ONE-CYCLE WINDOW. `aux_ready_i` is tied HIGH through the
  // whole island -- zhao_texture_island_top.sv:1102 wires it to
  // zhao_texture_aux_pipe.sv:165 `req_ready_o = 1'b1` -- so the receiver
  // accepts on the first cycle valid is asserted, every time. The wrong-context
  // cycle was THE ONLY CYCLE ANY REQUEST EVER GOT. The corrupted word is then
  // consumed as GEOMETRY at island_top.sv:1107
  // (`.req_wx_i(fr_aux_ctx[31:0]), .req_wz_i(fr_aux_ctx[63:32])`), so every AUX
  // sheet lookup in the island sampled the PREVIOUS AUX fragment's world X/Z
  // under the current fragment's slot and generation -- and every identity
  // check passed while it happened.
  // (reports/ZHAOZHOU-PREFIT-VERIFICATION-AND-REARCHITECT-20260906.txt S3.1-3.2)
  //
  // THE REPAIR, S5.1's "explicit AUX_READ phase": `ax_rdv_q` is that phase. The
  // pop LAUNCHES the address and raises ax_rdv_q, which is NOT a request. One
  // clock later the bank has answered with ctx_m[ax_slot_q] for the slot now
  // held in ax_slot_q, and only then does ax_pend_q rise. Because the pop is
  // gated on both flags being clear, ax_slot_q cannot move while a request is
  // outstanding, so the unconditional re-read below keeps reloading the SAME
  // word: the whole held tuple is stable to the acceptance edge by
  // construction, not by the receiver happening to be fast. This is the seam
  // S5.1 says "must remain correct after context banks are split or their
  // latency changes" -- adding latency here means adding read states, never
  // moving the valid.
  logic          ax_pend_q;
  logic          ax_rdv_q;       // AUX_READ phase: address launched, not a request
  logic [SW-1:0] ax_slot_q;
  logic [GENW-1:0] ax_gen_q;
  logic [CTXW-1:0] ax_ctx_r;      // read from the context bank

  assign aux_valid_o = ax_pend_q;
  assign aux_ctx_o   = ax_ctx_r;
  assign aux_slot_o  = ax_slot_q;
  assign aux_gen_o   = ax_gen_q;

  // ==========================================================================
  // ORDERED RETIRE (S6.8) -- allocation order IS retirement order
  // ==========================================================================
  // The combiner may finish fragments out of order; the output does not. head_q
  // is the allocation-order cursor, and it is READ THROUGH A REGISTER rather
  // than used to index an array combinationally, which is defect 2 above.
  typedef enum logic [1:0] { R_IDLE, R_READ, R_HOLD } rstate_e;
  rstate_e       r_st_q;
  logic [SW-1:0] head_q, tail_q;
  logic [SW:0]   live_cnt_q;

  // ALLOCATION ORDER IS NOT SLOT ORDER. Slots come from a free list, so the
  // order they are handed out in is the order they were RETURNED in, which
  // after the first recycle has nothing to do with their index. The first
  // version used `head_q` directly as the slot and retired the wrong entries
  // the moment the free list wrapped -- the differential caught it as exactly
  // DEPTH fragments never retiring.
  //
  // `order_m` is the allocation-order ring: written at `tail_q` when a slot is
  // handed out, read at `head_q` when one retires. It is 16 x 4 bits, so it is
  // control state and belongs in flops with the rest.
  logic [SW-1:0]   order_m [DEPTH];
  logic [SW-1:0]   head_slot_c;
  assign head_slot_c = order_m[head_q];

  logic [23:0]     out_rgb_r, out_aux_rgb_r;
  logic [7:0]      out_a_r, out_aux_a_r;
  logic [23:0]     out_s_rgb_r [3];
  logic [7:0]      out_s_a_r   [3];
  logic [CTXW-1:0] out_ctx_r;
  logic [TOKW_F-1:0] out_tok_r;
  logic            out_hasaux_r, out_sat_r;
  logic [SW-1:0]   r_slot_q;

  assign o_valid_o   = (r_st_q == R_HOLD);
  assign retire_ev_c = o_valid_o && o_ready_i;
  assign o_ctx_o     = out_ctx_r;
  assign o_tok_o     = out_tok_r;
  assign o_rgb_o     = out_rgb_r;
  assign o_a_o       = out_a_r;
  assign o_s_rgb_o   = out_s_rgb_r;
  assign o_s_a_o     = out_s_a_r;
  assign o_aux_rgb_o = out_aux_rgb_r;
  assign o_aux_a_o   = out_aux_a_r;
  assign o_has_aux_o = out_hasaux_r;
  assign o_uv_sat_o  = out_sat_r;

  // Is the head fragment finished?
  logic head_done_c;
  assign head_done_c = val_q[head_slot_c] &&
                       (arr_q[head_slot_c] == req_q[head_slot_c]) &&
                       (!auxreq_q[head_slot_c] || auxarr_q[head_slot_c]);

  // THE COMBINER IS NOT FROZEN. Every recipe currently returns sample 0, which
  // is v2's behaviour and is why this signal exists: a gate that ignores it is
  // testing a placeholder. The material combiner is a separate block (S3.3)
  // and its arithmetic is not this block's.
  assign combiner_unfrozen_o = o_valid_o;

  // ==========================================================================
  // BANK PROCESS -- CLOCK ONLY. NO RESET. THIS IS THE WHOLE POINT.
  // ==========================================================================
  // An M10K has no reset port. Every array below is written and read here and
  // nowhere else, through addresses that are registers. The control flops keep
  // their asynchronous reset in the process after this one; the payload does
  // not need one, because `val_q` gates every read of it.
  always_ff @(posedge clk) begin
    // ---- descriptor write: all three banks on one clock -------------------
    if (accept_c) begin
      for (int unsigned s = 0; s < 3; s++) begin
        desc_u_m[s][alloc_slot_c]   <= f_u_i[s];
        desc_v_m[s][alloc_slot_c]   <= f_v_i[s];
        desc_met_m[s][alloc_slot_c] <= {f_binding_i[s], f_lod_i[s]};
      end
      ctx_m[alloc_slot_c] <= f_ctx_i;
      tok_m[alloc_slot_c] <= f_tok_i;
    end

    // ---- descriptor read for issue ----------------------------------------
    bank_u_r   <= desc_u_m[rd_sidx_q][rd_slot_q];
    bank_v_r   <= desc_v_m[rd_sidx_q][rd_slot_q];
    bank_met_r <= desc_met_m[rd_sidx_q][rd_slot_q];

    // ---- sample results, banked by sample index ---------------------------
    // Addressed through the CLAMPED index: an out-of-range return is already
    // refused by tmu_ok_c, and additionally cannot name a bank.
    if (tmu_ok_c) begin
      res_rgb_m[tmu_sidx_c][tmu_rslot_i] <= tmu_rgb_i;
      res_a_m  [tmu_sidx_c][tmu_rslot_i] <= tmu_a_i;
    end
    if (aux_ok_c) begin
      auxrgb_m[aux_rslot_i] <= aux_rgb_i;
      auxa_m  [aux_rslot_i] <= aux_a_i;
    end

    // ---- the retire read, and the AUX context read ------------------------
    out_rgb_r     <= res_rgb_m[0][head_slot_c];
    out_a_r       <= res_a_m[0][head_slot_c];
    for (int s = 0; s < 3; s++) begin
      out_s_rgb_r[s] <= res_rgb_m[s][head_slot_c];
      out_s_a_r[s]   <= res_a_m[s][head_slot_c];
    end
    out_aux_rgb_r <= auxrgb_m[head_slot_c];
    out_aux_a_r   <= auxa_m[head_slot_c];
    out_ctx_r     <= ctx_m[head_slot_c];
    out_tok_r     <= tok_m[head_slot_c];
    // The AUX context read. UNCONDITIONAL is now safe -- and is deliberately
    // kept unconditional so the bank keeps the shape a RAM can take -- because
    // `ax_slot_q` is frozen from the pop until the request is accepted (see the
    // AUX_READ phase above). Before that, this line raced the pop and lost.
    ax_ctx_r      <= ctx_m[ax_slot_q];
  end

  // ==========================================================================
  // CONTROL PROCESS -- the flops, with their reset
  // ==========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      init_q      <= 1'b1;
      init_i_q    <= '0;
      free_wp_q   <= '0;
      free_rp_q   <= '0;
      free_cnt_q  <= '0;
      wq_wp_q     <= '0;
      wq_rp_q     <= '0;
      head_q      <= '0;
      tail_q      <= '0;
      live_cnt_q  <= '0;
      i_st_q      <= I_IDLE;
      r_st_q      <= R_IDLE;
      // ax_slot_q and ax_gen_q were NOT in this list. The first AUX request
      // after reset therefore read ctx_m[0] -- an arbitrary slot's context --
      // and shipped it under whatever identity the pop supplied. S3.2.
      ax_pend_q   <= 1'b0;
      ax_rdv_q    <= 1'b0;
      ax_slot_q   <= '0;
      ax_gen_q    <= '0;
      axq_wp_q    <= '0;
      axq_rp_q    <= '0;
      rd_slot_q   <= '0;
      rd_sidx_q   <= '0;
      fragments_o <= '0;
      samples_o   <= '0;
      full_clocks_o <= '0;
      id_errors_o <= '0;
      wq_overflow_o <= 1'b0;
      id_error_o    <= 1'b0;
      for (int unsigned k = 0; k < DEPTH; k++) begin
        val_q[k]    <= 1'b0;
        gen_q[k]    <= '0;
        req_q[k]    <= 3'd0;
        arr_q[k]    <= 3'd0;
        auxreq_q[k] <= 1'b0;
        auxarr_q[k] <= 1'b0;
        iss_q[k]    <= 3'd0;
        auxiss_q[k] <= 1'b0;
        recipe_q[k] <= 3'd0;
        sat_q[k]    <= 1'b0;
      end
    end else begin
      // `id_error_o` is driven from a single place at the bottom of this
      // process now; a default-clear here would be a second writer on it.

      // The counts move here and ONLY here.
      if (!init_q) begin
        free_cnt_q <= free_cnt_n_c;
        live_cnt_q <= live_cnt_n_c;
      end

      // ---- the sixteen-cycle free-list sweep ------------------------------
      if (init_q) begin
        free_m[init_i_q[SW-1:0]] <= init_i_q[SW-1:0];
        free_wp_q  <= init_i_q + 1;
        free_cnt_q <= init_i_q + 1;
        if (init_i_q == (SW+1)'(DEPTH - 1)) init_q <= 1'b0;
        else init_i_q <= init_i_q + 1;
      end

      if (!init_q && f_valid_i && free_empty_c) begin
        full_clocks_o <= full_clocks_o + 32'd1;
      end

      // ---- allocation ------------------------------------------------------
      if (accept_c) begin
        val_q[alloc_slot_c]    <= 1'b1;
        // The generation increments when allocation commits, BEFORE any
        // request carrying the token leaves the block (S6.5).
        gen_q[alloc_slot_c]    <= gen_q[alloc_slot_c] + GENW'(1);
        req_q[alloc_slot_c]    <= req_mask_c;
        arr_q[alloc_slot_c]    <= 3'd0;
        // The issued masks start empty for every new occupant, so a return
        // still in flight for the PREVIOUS occupant of this slot fails the
        // issued term as well as the generation term.
        iss_q[alloc_slot_c]    <= 3'd0;
        auxiss_q[alloc_slot_c] <= 1'b0;
        auxreq_q[alloc_slot_c] <= f_aux_i;
        auxarr_q[alloc_slot_c] <= 1'b0;
        recipe_q[alloc_slot_c] <= f_recipe_i;
        sat_q[alloc_slot_c]    <= f_uv_sat_i;
        free_rp_q   <= free_rp_q + 1;
        order_m[tail_q] <= alloc_slot_c;
        tail_q      <= tail_q + SW'(1);
        fragments_o <= fragments_o + 32'd1;

        // queue every required sample
        for (int unsigned s = 0; s < 3; s++) begin
          if (req_mask_c[s]) begin
            wq_m[wq_wa_c[s]] <= {alloc_slot_c, 2'(s)};
          end
        end
        wq_wp_q <= wq_wp_q + (WQW+1)'(f_sample_count_i);
        if (wq_occ_c + (WQW+1)'(f_sample_count_i) > (WQW+1)'(WQN)) begin
          wq_overflow_o <= 1'b1;
        end
      end

      // ---- TMU issue pipeline ---------------------------------------------
      case (i_st_q)
        I_IDLE: begin
          if (!wq_empty_c) begin
            rd_slot_q <= wq_m[wq_ra_c][SW+1:2];
            rd_sidx_q <= wq_m[wq_ra_c][1:0];
            i_slot_q  <= wq_m[wq_ra_c][SW+1:2];
            i_sidx_q  <= wq_m[wq_ra_c][1:0];
            i_gen_q   <= gen_q[wq_m[wq_ra_c][SW+1:2]];
            wq_rp_q   <= wq_rp_q + 1;
            i_st_q    <= I_READ;
          end
        end
        I_READ: i_st_q <= I_HOLD;   // the bank answers this clock
        I_HOLD: begin
          if (tmu_ready_i) begin
            samples_o <= samples_o + 32'd1;
            // ISSUED means HANDSHAKEN OUT, not merely queued: this is the
            // exact edge on which an answer becomes possible, so it is the
            // exact edge on which the acceptance predicate starts allowing one.
            iss_q[i_slot_q][i_sidx_q] <= 1'b1;
            i_st_q    <= I_IDLE;
          end
        end
        default: i_st_q <= I_IDLE;
      endcase

      // ---- AUX issue -------------------------------------------------------
      // enqueue on allocation; the generation is the one just committed
      if (accept_c && f_aux_i) begin
        axq_m[axq_wp_q[SW-1:0]] <= alloc_slot_c;
        axg_m[axq_wp_q[SW-1:0]] <= gen_q[alloc_slot_c] + GENW'(1);
        axq_wp_q <= axq_wp_q + 1;
      end
      // Present one at a time, through POP -> AUX_READ -> HOLD, which mirrors
      // the TMU issue path's I_IDLE/I_READ/I_HOLD for the same reason: a
      // payload that lives in a bank cannot be presented on the clock its
      // address is chosen.
      if (!ax_pend_q && !ax_rdv_q) begin
        if (!axq_empty_c) begin
          // POP. The address is launched; ax_ctx_r still holds the previous
          // request's word this clock, which is precisely why valid stays low.
          ax_slot_q <= axq_m[axq_rp_q[SW-1:0]];
          ax_gen_q  <= axg_m[axq_rp_q[SW-1:0]];
          axq_rp_q  <= axq_rp_q + 1;
          ax_rdv_q  <= 1'b1;
        end
      end else if (ax_rdv_q) begin
        // AUX_READ retired: on THIS edge the bank process sampled
        // ctx_m[ax_slot_q] with ax_slot_q already holding the popped slot, so
        // ax_ctx_r and ax_slot_q now describe the same fragment. Valid may rise.
        ax_rdv_q  <= 1'b0;
        ax_pend_q <= 1'b1;
      end else if (aux_ready_i) begin
        // Accepted. The AUX request has genuinely left the block, so an AUX
        // return for this slot is now legal -- and not before.
        auxiss_q[ax_slot_q] <= 1'b1;
        ax_pend_q <= 1'b0;
      end

      // ---- returns ---------------------------------------------------------
      // A return that fails ANY of the six terms -- reallocated slot, wrong
      // generation, sample index 3, a sample nobody asked for, a sample no
      // request was ever issued for, or a duplicate of one already taken -- is
      // refused and counted, never applied to whatever now lives in that slot.
      // Accepting the sample CLEARS its issued bit, which is what makes the
      // second copy of a duplicated answer a refusal instead of an overwrite.
      if (tmu_ok_c) begin
        arr_q[tmu_rslot_i][tmu_sidx_c] <= 1'b1;
        iss_q[tmu_rslot_i][tmu_sidx_c] <= 1'b0;
      end
      if (aux_ok_c) begin
        auxarr_q[aux_rslot_i] <= 1'b1;
        auxiss_q[aux_rslot_i] <= 1'b0;
      end

      // ONE writer, ONE delta, so two faults on one clock count TWO. See the
      // id_err_delta_c comment above for what the two separate `+ 32'd1`
      // assignments that used to live here actually counted.
      id_errors_o <= id_errors_o + {30'd0, id_err_delta_c};
      id_error_o  <= (id_err_delta_c != 2'd0);

      // ---- ordered retire --------------------------------------------------
      case (r_st_q)
        R_IDLE: begin
          if (head_done_c && (live_cnt_q != '0)) begin
            r_slot_q <= head_slot_c;
            out_hasaux_r <= auxreq_q[head_slot_c];
            out_sat_r    <= sat_q[head_slot_c];
            r_st_q       <= R_READ;
          end
        end
        R_READ: r_st_q <= R_HOLD;   // the banks answer this clock
        R_HOLD: begin
          if (o_ready_i) begin
            val_q[r_slot_q] <= 1'b0;
            head_q     <= head_q + SW'(1);
            // final acceptance returns the slot to the free list
            free_m[free_wp_q[SW-1:0]] <= r_slot_q;
            free_wp_q  <= free_wp_q + 1;
            r_st_q     <= R_IDLE;
          end
        end
        default: r_st_q <= R_IDLE;
      endcase
    end
  end

endmodule : zhao_texture_fragrob

`default_nettype wire
