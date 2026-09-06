// zhao_texture_v3own.sv -- the V3 owner / completion / retire experiment.
//
// reports/TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt section 26.1, verbatim:
//
//   > Do not begin with a giant top-level rewrite or another full-island fit.
//   > Build one small V3 owner/completion/retire experiment with the real
//   > proposed capacities:
//   >   64 owners and full generation identity;
//   >   three statically banked 64x40 sample-result stores;
//   >   one 64x40 AUX result store;
//   >   required/issued/claimed/committed control;
//   >   simultaneous TMU and AUX terminal inputs;
//   >   once-only ready-ticket creation;
//   >   a final 64x40 result store and 64-bit context path;
//   >   synchronous credited ordered output;
//   >   an adversarial testbench and exact-tool synthesis/fit wrapper.
//
// It is not the island, it does not sample a texture, and it computes no
// colour. It is the LIFETIME and the STORAGE, which is the part the fit says
// is expensive.
//
// ---------------------------------------------------------------------------
// THE DEFECT THIS REPLACES, NAMED AT ITS LINE NUMBER
// ---------------------------------------------------------------------------
// fpga/rtl/texture/zhao_texture_fragrob.sv:626
//
//     if (tmu_ok_c) begin
//       res_rgb_m[tmu_sidx_c][tmu_rslot_i] <= tmu_rgb_i;
//
// and `tmu_ok_c` at :443 is
//
//     tmu_rvalid_i && val_q[tmu_rslot_i] && (gen_q[tmu_rslot_i]==tmu_rgen_i)
//       && tmu_sidx_ok_c && req_q[..][..] && iss_q[..][..] && !arr_q[..][..]
//
// -- an INPUT PIN, through five slot-indexed table lookups and a seven-term
// predicate, arriving at a payload RAM's write enable in the same cycle. That
// is section 0's point D, "the reported return-to-RAM-write-enable cone".
//
// In this module the corresponding write enable is
//
//     .wr_en_i (c3t_we_q[s])
//
// a bare flip-flop output with NOTHING between it and the bank. The predicate
// still exists, still rejects exactly the same six classes, and now runs two
// pipeline stages earlier where its result has a whole clock to settle before
// anything writes. Section 0 again: "A delayed write-enable alone is not this
// pipeline" -- so the claim/commit split below is the substance and the
// registered enable is only its visible consequence.
//
// ---------------------------------------------------------------------------
// WHAT IS IN FABRIC AND WHY THAT IS NOT A CHEAT
// ---------------------------------------------------------------------------
// Section 0 point C: "Keep only small scoreboards, queue pointers, valid bits,
// and genuinely bounded pipeline registers in fabric." The scoreboard here is
// 64 x (live + gen8 + required4 + issued4 + claimed4 + committed4 + 5 single
// bits) = 64 x 30 = 1,920 flops, and section 8.1 asks for it in fabric
// explicitly ("They have genuine concurrent events and are not the place to
// force an impossible multiported M10K"). Every WIDE payload -- context64,
// four result40 planes, the final result40, and the three ready queues -- is a
// zhao_texture_v3bank instance and appears by name in the Quartus RAM summary.
//
// ---------------------------------------------------------------------------
// EVERY PAYLOAD WRITE ENABLE IN THIS FILE IS A FLOP OUTPUT
// ---------------------------------------------------------------------------
//   OWNER_CONTEXT    .wr_en_i(ctxw_v_q)
//   SAMPLE_RESULT_s  .wr_en_i(c3t_we_q[s])     one-hot, registered at C2->C3
//   AUX_RESULT       .wr_en_i(c3a_we_q)
//   FINAL_RESULT     .wr_en_i(c3f_we_q)
//   READY_TMU        .wr_en_i(q0t_v_q)
//   READY_AUX        .wr_en_i(q0a_v_q)
//   READY_INITIAL    .wr_en_i(q0i_v_q)
//
// The ready-queue writes are registered one edge behind the eligibility
// decision, which Appendix B.4 authorises directly: "A queue write delayed by
// a register does not delay the claim that prevents a second ticket." The
// ticket CLAIM (rdy_q) still happens on the eligibility edge, so a second
// event for the same owner cannot produce a second ticket during the delay.
//
// The three sample banks are instantiated from a `genvar` generate with the
// bank instance INSIDE the loop, and the write enable is one bit of a
// registered one-hot. That shape is deliberate and is the same one
// reports/V3-DIAGNOSIS-VERIFICATION-20260906.md section 3.2 says must not be
// "tidied" into `data_r [LANES][N]`: a register-selected lane index costs a
// wide mux, and in the predecessor block that rewrite cost 5,402 ALMs and
// produced ZERO M10K. Static index, ports inside the generate, always.
//
// ---------------------------------------------------------------------------
// STAGE MAP (section 8.2, and the same shape reused three times)
// ---------------------------------------------------------------------------
//   C0 CAPTURE   register the whole return packet and its range check
//   C1 CONTROL   register the addressed narrow owner-state snapshot
//   C2 VALIDATE  identity/required/issued/duplicate + recent-claim forwarding;
//                an accepted return sets claimed AT THIS EDGE
//   C3 COMMIT    the registered one-hot enable drives exactly one bank
//   C4 PUBLISH   committed rises one edge AFTER the payload write edge, and
//                readiness is evaluated from the coalesced masks
//
// The forwarding window is exactly ONE cycle and that is derived, not assumed:
// C1's snapshot is taken at the edge that also applies C2's claim, so the very
// next packet's C2 (one cycle later) is the only one that can miss it; the
// packet after that reads a snapshot taken after the claim landed. The C3
// record IS the forwarding record -- it carries {valid, slot, generation,
// mask} of the claim made on the previous edge. If a stage is ever inserted
// between C1 and C2, this window grows and FWD_WINDOW below must grow with it.
//
// ---------------------------------------------------------------------------
// EVERY TRIPWIRE REACHES A PORT
// ---------------------------------------------------------------------------
// reports/V3-DIAGNOSIS-VERIFICATION-20260906.md section 4 item 2: twelve
// island signals are declared, port-connected and consumed by NOTHING,
// including FRAGROB's own `id_error_o` and `wq_overflow_o`. "Preserving the
// logic that sets them is not enough -- they must reach a port, or V3 inherits
// the same blindness." So every rejection class here has its own counter on
// its own port, the classes are mutually exclusive by construction so the
// counters partition the traffic, and the adversarial bench asserts on every
// one of them. A counter nothing reads is decoration.
//
// ENFORCED-BY: fpga/rtl/texture/zhao_texture_v3own.sv:a_reject_partition_t
// ENFORCED-BY: fpga/rtl/texture/zhao_texture_v3own.sv:a_reject_partition_a
//
// The assertions are named above rather than left to prose because that is the
// kind of sentence which stays true right up until somebody adds a sixth
// condition. Were two classes ever to fire for one packet, the per-port totals
// would stop being addable -- and a number that cannot be added up is worse
// than no number at all.
//
// ---------------------------------------------------------------------------
// SCOPE HONESTY -- what section 6 asks for that is deliberately NOT here
// ---------------------------------------------------------------------------
// Section 6's bank table also lists MATERIAL 64x48, AUX_GEOMETRY 64x80, three
// SAMPLE_DESC 64x80 planes, RCP_RESULT 64x32 and SAMPLE_METADATA 256x40.
// Section 26.1's experiment does not name them, and they belong to admission /
// descriptor expansion / the planner rather than to the completion and
// retirement lifetime under test. Adding them would inflate the M10K count of
// an experiment whose whole purpose is attribution. They are absent ON PURPOSE
// and their absence is reported, not quietly enjoyed.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_texture_v3own #(
    // 64 owners. Section 5.1: "The baseline owner capacity is 64."
    parameter int unsigned OWNERS = 64,
    parameter int unsigned SLOTW  = 6,
    parameter int unsigned GENW   = 8,
    // result40 = STATUS8 | alpha8 | RGB888 (Appendix B.1).
    parameter int unsigned RESW   = 40,
    // Section 5.6: the FULL 64-bit opaque context, returned at the output. A
    // 16-bit legacy tag is a wrapper's business, not this bank's.
    parameter int unsigned CTXW   = 64,
    // Output reservation domain (section 18.1/18.2). 4 covers the three
    // read/capture stages plus one queued row, which is what lets the read
    // path sustain one fragment per clock (section 18.3).
    parameter int unsigned OUTQD  = 4,
    // COMBINE input reservation domain (section 9.4).
    parameter int unsigned CMBQD  = 4,
    // Derived; ports cannot name a localparam. Do not override.
    parameter int unsigned OWNERW = SLOTW + GENW,
    parameter int unsigned SMPW   = SLOTW + 2 + GENW,
    parameter int unsigned CNTW   = SLOTW + 1
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- admission (section 5.2) --------------------------------------------
    input  var logic              adm_valid_i,
    output var logic              adm_ready_o,
    input  var logic [CTXW-1:0]   adm_ctx_i,
    // {AUX, sample2, sample1, sample0}. The FROZEN required mask, section 9.1.
    input  var logic [3:0]        adm_req_i,
    output var logic [OWNERW-1:0] adm_owner_o,
    output var logic              adm_accept_o,

    // ---- issue notification (section 19.3: ISSUED is its own moment) --------
    input  var logic              iss_tmu_valid_i,
    input  var logic [SMPW-1:0]   iss_tmu_handle_i,
    input  var logic              iss_aux_valid_i,
    input  var logic [OWNERW-1:0] iss_aux_owner_i,

    // ---- TMU terminal return ------------------------------------------------
    input  var logic              tmu_rvalid_i,
    output var logic              tmu_rready_o,
    input  var logic [SMPW-1:0]   tmu_rhandle_i,
    input  var logic [RESW-1:0]   tmu_rresult_i,

    // ---- AUX terminal return (its own typed port, section 5.1) -------------
    input  var logic              aux_rvalid_i,
    output var logic              aux_rready_o,
    input  var logic [OWNERW-1:0] aux_rowner_i,
    input  var logic [RESW-1:0]   aux_rresult_i,

    // ---- COMBINE admission (section 9.4) ------------------------------------
    output var logic              cmb_valid_o,
    input  var logic              cmb_ready_i,
    output var logic [OWNERW-1:0] cmb_owner_o,
    output var logic [RESW-1:0]   cmb_s0_o,
    output var logic [RESW-1:0]   cmb_s1_o,
    output var logic [RESW-1:0]   cmb_s2_o,
    output var logic [RESW-1:0]   cmb_aux_o,

    // ---- COMBINE final return (section 18.4) --------------------------------
    input  var logic              fin_valid_i,
    output var logic              fin_ready_o,
    input  var logic [OWNERW-1:0] fin_owner_i,
    input  var logic [RESW-1:0]   fin_result_i,

    // ---- ordered output (section 18) ----------------------------------------
    output var logic              out_valid_o,
    input  var logic              out_ready_i,
    output var logic [OWNERW-1:0] out_owner_o,
    output var logic [RESW-1:0]   out_result_o,
    output var logic [CTXW-1:0]   out_ctx_o,

    // ---- evidence (section 19.7) --------------------------------------------
    output var logic [31:0]       ev_admitted_o,
    output var logic [31:0]       ev_emitted_o,
    output var logic [31:0]       ev_commits_o,
    output var logic [31:0]       ev_tickets_o,
    output var logic [31:0]       ev_err_range_o,
    output var logic [31:0]       ev_err_stale_o,
    output var logic [31:0]       ev_err_unsol_o,
    output var logic [31:0]       ev_err_dup_o,
    output var logic [31:0]       ev_err_final_o,
    output var logic [31:0]       ev_err_issue_o,
    output var logic [31:0]       ev_wrap_drains_o,
    output var logic [CNTW-1:0]   ev_live_o,
    output var logic [CNTW-1:0]   ev_live_peak_o,
    output var logic              ev_quiet_o
);

  localparam logic [3:0] SRC_AUX = 4'b1000;

  // ==========================================================================
  // NARROW OWNER SCOREBOARD -- fabric, section 8.1
  // ==========================================================================
  logic            live_q [OWNERS];
  logic [GENW-1:0] gen_q  [OWNERS];
  logic [3:0]      req_q  [OWNERS];
  logic [3:0]      iss_q  [OWNERS];
  logic [3:0]      clm_q  [OWNERS];
  logic [3:0]      cmt_q  [OWNERS];
  logic            rdy_q  [OWNERS];   // ready_claimed
  logic            cbi_q  [OWNERS];   // combine_issued
  logic            fcl_q  [OWNERS];   // final_claimed
  logic            fdn_q  [OWNERS];   // final_done
  logic            ftc_q  [OWNERS];   // fetched (section 18.1)

  logic [SLOTW-1:0] tail_q, emit_q, fetch_q;
  logic [CNTW-1:0]  live_cnt_q, unf_cnt_q, peak_q;

  // ==========================================================================
  // ADMISSION
  // ==========================================================================
  logic [GENW-1:0] adm_gen_c;
  logic            wrap_block_c;
  logic            adm_fire_c;
  logic            quiet_c;

  assign adm_gen_c    = gen_q[tail_q] + GENW'(1);
  assign wrap_block_c = (gen_q[tail_q] == {GENW{1'b1}});

  // SECTION 5.5, the BASELINE DRAIN POLICY, implemented rather than discussed.
  //
  //   > before the owner namespace wraps, stop new admission, drain all live
  //   > owners and all external outstanding transactions, empty return and
  //   > execution queues, ... and only then reuse the wrapped namespace.
  //
  // The slot about to be allocated is the one whose generation is about to
  // wrap, so the gate is exactly "this allocation may proceed only when the
  // island is quiet". Widening the field would postpone wrap, not abolish it,
  // and the document says so in as many words.
  assign adm_ready_o  = (live_cnt_q < CNTW'(OWNERS)) && (!wrap_block_c || quiet_c);
  assign adm_fire_c   = adm_valid_i && adm_ready_o;
  assign adm_accept_o = adm_fire_c;
  // Section 5.4: "Never write a payload using next_tail while stamping the
  // handle with current_tail." One tail, one generation, one edge.
  assign adm_owner_o  = {tail_q, adm_gen_c};

  // OWNER_CONTEXT write, registered. Admission is a ready/valid handshake, so
  // an unregistered enable would be `adm_valid_i && adm_ready_o` -- an input
  // pin on a RAM write enable, the same shape as the defect being removed.
  // The row lands one edge later; the earliest possible retirement read of the
  // same owner is many edges later even for a zero-work owner (READY_INITIAL
  // push +1, pop +1, bank read +2, COMBINE hand-off +1, final return +4), so
  // the separation is not marginal.
  logic              ctxw_v_q;
  logic [SLOTW-1:0]  ctxw_addr_q;
  logic [CTXW-1:0]   ctxw_data_q;

  // ==========================================================================
  // ISSUE (section 19.3 -- ISSUED is a separate moment from REQUIRED)
  // ==========================================================================
  logic [SLOTW-1:0] iss_t_slot_c;
  logic [1:0]       iss_t_sidx_c;
  logic [GENW-1:0]  iss_t_gen_c;
  logic             iss_t_rng_c;
  logic [3:0]       iss_t_bit_c;
  logic             iss_t_ok_c;

  assign iss_t_slot_c = iss_tmu_handle_i[SMPW-1 -: SLOTW];
  assign iss_t_sidx_c = iss_tmu_handle_i[GENW+1 -: 2];
  assign iss_t_gen_c  = iss_tmu_handle_i[GENW-1:0];
  assign iss_t_rng_c  = (iss_t_sidx_c != 2'd3);
  assign iss_t_bit_c  = iss_t_rng_c ? (4'b0001 << iss_t_sidx_c) : 4'b0000;
  assign iss_t_ok_c   = iss_tmu_valid_i && iss_t_rng_c
                     && live_q[iss_t_slot_c]
                     && (gen_q[iss_t_slot_c] == iss_t_gen_c)
                     && ((req_q[iss_t_slot_c] & iss_t_bit_c) != 4'd0)
                     && ((iss_q[iss_t_slot_c] & iss_t_bit_c) == 4'd0);

  logic [SLOTW-1:0] iss_a_slot_c;
  logic [GENW-1:0]  iss_a_gen_c;
  logic             iss_a_ok_c;
  assign iss_a_slot_c = iss_aux_owner_i[OWNERW-1 -: SLOTW];
  assign iss_a_gen_c  = iss_aux_owner_i[GENW-1:0];
  assign iss_a_ok_c   = iss_aux_valid_i
                     && live_q[iss_a_slot_c]
                     && (gen_q[iss_a_slot_c] == iss_a_gen_c)
                     && ((req_q[iss_a_slot_c] & SRC_AUX) != 4'd0)
                     && ((iss_q[iss_a_slot_c] & SRC_AUX) == 4'd0);

  // ==========================================================================
  // RETURN PORTS
  // ==========================================================================
  // Section 19.1 RESERVED FIXED LATENCY: these segments never stall after
  // capture, so ready is unconditional and the proof is a conservation
  // argument rather than a backward ready chain. Downstream storage is (a) the
  // owner's own result bank row, which is written at most once per source per
  // owner, and (b) READY_TMU / READY_AUX, whose depth 64 equals the owner
  // capacity while every owner claims at most one ticket. Neither can refuse.
  assign tmu_rready_o = 1'b1;
  assign aux_rready_o = 1'b1;
  assign fin_ready_o  = 1'b1;

  logic             c0t_v_q, c0t_rng_q;
  logic [SLOTW-1:0] c0t_slot_q;
  logic [1:0]       c0t_sidx_q;
  logic [GENW-1:0]  c0t_gen_q;
  logic [RESW-1:0]  c0t_res_q;

  logic             c1t_v_q, c1t_rng_q;
  logic [SLOTW-1:0] c1t_slot_q;
  logic [1:0]       c1t_sidx_q;
  logic [GENW-1:0]  c1t_gen_q;
  logic [RESW-1:0]  c1t_res_q;
  logic             c1t_live_q;
  logic [GENW-1:0]  c1t_tgen_q;
  logic [3:0]       c1t_req_q, c1t_iss_q, c1t_clm_q, c1t_cmt_q;

  logic             c3t_v_q;
  logic [2:0]       c3t_we_q;
  logic [SLOTW-1:0] c3t_slot_q;
  logic [GENW-1:0]  c3t_gen_q;
  logic [3:0]       c3t_mask_q;
  logic [RESW-1:0]  c3t_data_q;

  logic             c4t_v_q;
  logic [SLOTW-1:0] c4t_slot_q;
  logic [GENW-1:0]  c4t_gen_q;
  logic [3:0]       c4t_mask_q;

  logic [3:0] c1t_bit_c;
  assign c1t_bit_c = c1t_rng_q ? (4'b0001 << c1t_sidx_q) : 4'b0000;

  // ---- AUX return pipeline registers --------------------------------------
  logic             c0a_v_q;
  logic [SLOTW-1:0] c0a_slot_q;
  logic [GENW-1:0]  c0a_gen_q;
  logic [RESW-1:0]  c0a_res_q;

  logic             c1a_v_q;
  logic [SLOTW-1:0] c1a_slot_q;
  logic [GENW-1:0]  c1a_gen_q;
  logic [RESW-1:0]  c1a_res_q;
  logic             c1a_live_q;
  logic [GENW-1:0]  c1a_tgen_q;
  logic [3:0]       c1a_req_q, c1a_iss_q, c1a_clm_q, c1a_cmt_q;

  logic             c3a_v_q, c3a_we_q;
  logic [SLOTW-1:0] c3a_slot_q;
  logic [GENW-1:0]  c3a_gen_q;
  logic [RESW-1:0]  c3a_data_q;

  logic             c4a_v_q;
  logic [SLOTW-1:0] c4a_slot_q;
  logic [GENW-1:0]  c4a_gen_q;

  // ---- FINAL return pipeline registers ------------------------------------
  logic             c0f_v_q;
  logic [SLOTW-1:0] c0f_slot_q;
  logic [GENW-1:0]  c0f_gen_q;
  logic [RESW-1:0]  c0f_res_q;

  logic             c1f_v_q;
  logic [SLOTW-1:0] c1f_slot_q;
  logic [GENW-1:0]  c1f_gen_q;
  logic [RESW-1:0]  c1f_res_q;
  logic             c1f_live_q, c1f_cbi_q, c1f_fcl_q, c1f_fdn_q;
  logic [GENW-1:0]  c1f_tgen_q;

  logic             c3f_v_q, c3f_we_q;
  logic [SLOTW-1:0] c3f_slot_q;
  logic [GENW-1:0]  c3f_gen_q;
  logic [RESW-1:0]  c3f_data_q;

  logic             c4f_v_q;
  logic [SLOTW-1:0] c4f_slot_q;
  logic [GENW-1:0]  c4f_gen_q;

  // ==========================================================================
  // C2 VALIDATION -- section 8.3's predicate, all six terms, plus forwarding
  // ==========================================================================
  // FWD_WINDOW is DERIVED from the pipeline depth, not chosen. Section 8.4:
  // "The required forwarding window equals the number of cycles from
  // scoreboard snapshot to claim publication... It is not assumed to be one
  // forever." C1 registers the snapshot; C2 applies the claim on the very next
  // edge; so exactly one cycle of returns can hold a stale snapshot, and the
  // C3 record covers exactly that cycle.
  localparam int unsigned FWD_WINDOW = 1;

  logic fwd_t_hit_c, fwd_a_hit_c, fwd_f_hit_c;
  // Forward by FULL owner handle and source bit (section 8.4), never by slot
  // alone -- a slot match across a generation boundary is a different owner.
  assign fwd_t_hit_c = c3t_v_q && (c3t_slot_q == c1t_slot_q)
                    && (c3t_gen_q == c1t_gen_q)
                    && ((c3t_mask_q & c1t_bit_c) != 4'd0);
  assign fwd_a_hit_c = c3a_v_q && (c3a_slot_q == c1a_slot_q)
                    && (c3a_gen_q == c1a_gen_q);
  assign fwd_f_hit_c = c3f_v_q && (c3f_slot_q == c1f_slot_q)
                    && (c3f_gen_q == c1f_gen_q);

  logic c2t_idok_c, c2t_rng_bad_c, c2t_stale_c, c2t_unsol_c, c2t_dup_c, c2t_acc_c;
  assign c2t_idok_c    = c1t_live_q && (c1t_tgen_q == c1t_gen_q);
  assign c2t_rng_bad_c = c1t_v_q && !c1t_rng_q;
  assign c2t_stale_c   = c1t_v_q && c1t_rng_q && !c2t_idok_c;
  assign c2t_unsol_c   = c1t_v_q && c1t_rng_q && c2t_idok_c
                      && (((c1t_req_q & c1t_bit_c) == 4'd0)
                       || ((c1t_iss_q & c1t_bit_c) == 4'd0));
  assign c2t_dup_c     = c1t_v_q && c1t_rng_q && c2t_idok_c
                      && ((c1t_req_q & c1t_bit_c) != 4'd0)
                      && ((c1t_iss_q & c1t_bit_c) != 4'd0)
                      && (((c1t_clm_q & c1t_bit_c) != 4'd0)
                       || ((c1t_cmt_q & c1t_bit_c) != 4'd0)
                       || fwd_t_hit_c);
  assign c2t_acc_c     = c1t_v_q && c1t_rng_q && c2t_idok_c
                      && ((c1t_req_q & c1t_bit_c) != 4'd0)
                      && ((c1t_iss_q & c1t_bit_c) != 4'd0)
                      && ((c1t_clm_q & c1t_bit_c) == 4'd0)
                      && ((c1t_cmt_q & c1t_bit_c) == 4'd0)
                      && !fwd_t_hit_c;

  logic c2a_idok_c, c2a_stale_c, c2a_unsol_c, c2a_dup_c, c2a_acc_c;
  assign c2a_idok_c  = c1a_live_q && (c1a_tgen_q == c1a_gen_q);
  assign c2a_stale_c = c1a_v_q && !c2a_idok_c;
  assign c2a_unsol_c = c1a_v_q && c2a_idok_c
                    && (((c1a_req_q & SRC_AUX) == 4'd0)
                     || ((c1a_iss_q & SRC_AUX) == 4'd0));
  assign c2a_dup_c   = c1a_v_q && c2a_idok_c
                    && ((c1a_req_q & SRC_AUX) != 4'd0)
                    && ((c1a_iss_q & SRC_AUX) != 4'd0)
                    && (((c1a_clm_q & SRC_AUX) != 4'd0)
                     || ((c1a_cmt_q & SRC_AUX) != 4'd0)
                     || fwd_a_hit_c);
  assign c2a_acc_c   = c1a_v_q && c2a_idok_c
                    && ((c1a_req_q & SRC_AUX) != 4'd0)
                    && ((c1a_iss_q & SRC_AUX) != 4'd0)
                    && ((c1a_clm_q & SRC_AUX) == 4'd0)
                    && ((c1a_cmt_q & SRC_AUX) == 4'd0)
                    && !fwd_a_hit_c;

  // Section 18.4: "Final returns use a credited capture/validate/claim/write/
  // publish sequence analogous to sample returns; final_done is not used as a
  // substitute for the earlier claim while a final write is still in flight."
  logic c2f_idok_c, c2f_bad_c, c2f_acc_c;
  assign c2f_idok_c = c1f_live_q && (c1f_tgen_q == c1f_gen_q);
  assign c2f_acc_c  = c1f_v_q && c2f_idok_c && c1f_cbi_q
                   && !c1f_fcl_q && !c1f_fdn_q && !fwd_f_hit_c;
  assign c2f_bad_c  = c1f_v_q && !c2f_acc_c;

  // ==========================================================================
  // C4 PUBLICATION, COALESCING AND THE ONCE-ONLY READY TICKET
  // ==========================================================================
  // Appendix D.2 worked exactly: required 1111, committed 0011, texture
  // source2 and AUX publish on the SAME edge for the SAME full handle -> ONE
  // eligibility transition and ONE ticket.
  logic same_owner_c;
  assign same_owner_c = c4t_v_q && c4a_v_q
                     && (c4t_slot_q == c4a_slot_q)
                     && (c4t_gen_q  == c4a_gen_q);

  logic [3:0] t_cmt_next_c, a_cmt_next_c;
  assign t_cmt_next_c = cmt_q[c4t_slot_q] | c4t_mask_q
                      | (same_owner_c ? SRC_AUX : 4'd0);
  assign a_cmt_next_c = cmt_q[c4a_slot_q] | SRC_AUX
                      | (same_owner_c ? c4t_mask_q : 4'd0);

  logic t_elig_c, a_elig_c, tkt_t_c, tkt_a_c;
  assign t_elig_c = c4t_v_q && live_q[c4t_slot_q]
                 && (gen_q[c4t_slot_q] == c4t_gen_q)
                 && ((t_cmt_next_c & req_q[c4t_slot_q]) == req_q[c4t_slot_q])
                 && !rdy_q[c4t_slot_q] && !cbi_q[c4t_slot_q];
  assign a_elig_c = c4a_v_q && live_q[c4a_slot_q]
                 && (gen_q[c4a_slot_q] == c4a_gen_q)
                 && ((a_cmt_next_c & req_q[c4a_slot_q]) == req_q[c4a_slot_q])
                 && !rdy_q[c4a_slot_q] && !cbi_q[c4a_slot_q];
  assign tkt_t_c  = t_elig_c;
  // The deterministic winner named by D.2: READY_TMU takes the coalesced
  // ticket and the AUX insertion is suppressed. Suppression is not a dropped
  // ticket -- the owner has exactly one, in the other queue.
  assign tkt_a_c  = a_elig_c && !same_owner_c;

  // Registered ready-queue writes (one edge behind the claim).
  logic              q0t_v_q, q0a_v_q, q0i_v_q;
  logic [OWNERW-1:0] q0t_owner_q, q0a_owner_q, q0i_owner_q;

  // ==========================================================================
  // READY QUEUES -- three, single-writer, depth 64 (section 9.3)
  // ==========================================================================
  logic              rq_full_c [3];
  logic              rq_valid_c[3];
  logic [OWNERW-1:0] rq_data_c [3];
  logic              rq_pop_c  [3];
  logic [SLOTW:0]    rq_occ_c  [3];

  // INSTANTIATED THREE TIMES BY HAND, NOT IN A GENERATE LOOP, and that is the
  // point rather than a missed tidy-up. Each queue has a DIFFERENT single
  // writer (section 9.3), so a loop would have to select the writer through a
  // combinational alias array -- and `.wr_en_i(rq_wr_c[gq])` puts a wire
  // between the flop and the memory enable, which is exactly the property
  // this experiment exists to make checkable. Written out, every `.wr_en_i()`
  // in this file is a bare identifier or a bit-select of one, so a source-level
  // gate can decide the "driven by a register output" law without elaborating.
  //
  // V3-WREN-REG: q0t_v_q
  // V3-BANK: READY_TMU
  zhao_texture_v3rq #(.WIDTH(OWNERW), .DEPTH(OWNERS)) u_rq_tmu (
      .clk      (clk),
      .rst_n    (rst_n),
      .wr_en_i  (q0t_v_q),
      .wr_data_i(q0t_owner_q),
      .full_o   (rq_full_c[0]),
      .valid_o  (rq_valid_c[0]),
      .data_o   (rq_data_c[0]),
      .pop_i    (rq_pop_c[0]),
      .occ_o    (rq_occ_c[0])
  );

  // V3-WREN-REG: q0a_v_q
  // V3-BANK: READY_AUX
  zhao_texture_v3rq #(.WIDTH(OWNERW), .DEPTH(OWNERS)) u_rq_aux (
      .clk      (clk),
      .rst_n    (rst_n),
      .wr_en_i  (q0a_v_q),
      .wr_data_i(q0a_owner_q),
      .full_o   (rq_full_c[1]),
      .valid_o  (rq_valid_c[1]),
      .data_o   (rq_data_c[1]),
      .pop_i    (rq_pop_c[1]),
      .occ_o    (rq_occ_c[1])
  );

  // V3-WREN-REG: q0i_v_q
  // V3-BANK: READY_INITIAL
  zhao_texture_v3rq #(.WIDTH(OWNERW), .DEPTH(OWNERS)) u_rq_init (
      .clk      (clk),
      .rst_n    (rst_n),
      .wr_en_i  (q0i_v_q),
      .wr_data_i(q0i_owner_q),
      .full_o   (rq_full_c[2]),
      .valid_o  (rq_valid_c[2]),
      .data_o   (rq_data_c[2]),
      .pop_i    (rq_pop_c[2]),
      .occ_o    (rq_occ_c[2])
  );

  // ==========================================================================
  // ROUND-ROBIN ARBITER AND COMBINE ADMISSION READ (section 9.4)
  // ==========================================================================
  // "The arbiter selects only handles. Wide material and sample data are read
  // after the selection is registered." Exactly that: the arbiter sees three
  // 14-bit heads, and the four 40-bit planes are read at the registered
  // address on the following cycles.
  logic [1:0] rr_q;
  logic [1:0] ord_c [3];
  logic [1:0] sel_c;
  logic       sel_v_c;
  logic [OWNERW-1:0] sel_data_c;

  always_comb begin
    case (rr_q)
      2'd0:    begin ord_c[0] = 2'd0; ord_c[1] = 2'd1; ord_c[2] = 2'd2; end
      2'd1:    begin ord_c[0] = 2'd1; ord_c[1] = 2'd2; ord_c[2] = 2'd0; end
      default: begin ord_c[0] = 2'd2; ord_c[1] = 2'd0; ord_c[2] = 2'd1; end
    endcase
    sel_c   = 2'd0;
    sel_v_c = 1'b0;
    for (int unsigned k = 0; k < 3; k++) begin
      if (!sel_v_c && rq_valid_c[ord_c[k]]) begin
        sel_v_c = 1'b1;
        sel_c   = ord_c[k];
      end
    end
  end
  assign sel_data_c = rq_data_c[sel_c];

  logic [CNTW-1:0] cmb_res_q;
  logic            cmb_pop_c, cmb_fire_c;
  // Section 9.4: "Do not pop a ready ticket merely because COMBINE ready is
  // high now if the memory read will return several cycles later. The
  // destination credit must cover that latency." cmb_res_q counts reads in
  // flight AND queued rows, and the reservation is taken at the pop.
  assign cmb_pop_c = sel_v_c && ((cmb_res_q - CNTW'(cmb_fire_c)) < CNTW'(CMBQD));
  always_comb begin
    for (int unsigned k = 0; k < 3; k++) begin
      rq_pop_c[k] = cmb_pop_c && (sel_c == 2'(k));
    end
  end

  logic              k0_v_q, k1_v_q, k2_v_q;
  logic [OWNERW-1:0] k0_owner_q, k1_owner_q, k2_owner_q;
  logic [SLOTW-1:0]  cmb_rd_addr_q;

  // ==========================================================================
  // THE PERSISTENT BANKS
  // ==========================================================================
  logic [RESW-1:0] sres_rd_c [3];
  logic [RESW-1:0] ares_rd_c;
  logic [RESW-1:0] fres_rd_c;
  logic [CTXW-1:0] ctx_rd_c;

  logic [SLOTW-1:0] fin_rd_addr_q;

  generate
    genvar gs;
    for (gs = 0; gs < 3; gs++) begin : g_sres
      // SAMPLE_RESULT_0/1/2, 64 x 40. Single writer: TMU commit bank gs.
      // Single reader: COMBINE admission. The write enable is one bit of a
      // registered one-hot -- there is no bank decode in this cone at all.
      //
      // V3-WREN-REG: c3t_we_q
      // V3-BANK: SAMPLE_RESULT_0, SAMPLE_RESULT_1, SAMPLE_RESULT_2
      zhao_texture_v3bank #(.WIDTH(RESW), .DEPTH(OWNERS)) u_sres (
          .clk      (clk),
          .wr_en_i  (c3t_we_q[gs]),
          .wr_addr_i(c3t_slot_q),
          .wr_data_i(c3t_data_q),
          .rd_addr_i(cmb_rd_addr_q),
          .rd_data_o(sres_rd_c[gs])
      );
    end
  endgenerate

  // AUX_RESULT, 64 x 40. Its own bank so a TMU and an AUX commit for the same
  // owner can land on the SAME clock (section 8.6) without any arbitration.
  //
  // V3-WREN-REG: c3a_we_q
  // V3-BANK: AUX_RESULT
  zhao_texture_v3bank #(.WIDTH(RESW), .DEPTH(OWNERS)) u_ares (
      .clk      (clk),
      .wr_en_i  (c3a_we_q),
      .wr_addr_i(c3a_slot_q),
      .wr_data_i(c3a_data_q),
      .rd_addr_i(cmb_rd_addr_q),
      .rd_data_o(ares_rd_c)
  );

  // FINAL_RESULT, 64 x 40. Writer: final commit. Reader: ordered retirement.
  //
  // V3-WREN-REG: c3f_we_q
  // V3-BANK: FINAL_RESULT
  zhao_texture_v3bank #(.WIDTH(RESW), .DEPTH(OWNERS)) u_fres (
      .clk      (clk),
      .wr_en_i  (c3f_we_q),
      .wr_addr_i(c3f_slot_q),
      .wr_data_i(c3f_data_q),
      .rd_addr_i(fin_rd_addr_q),
      .rd_data_o(fres_rd_c)
  );

  // OWNER_CONTEXT, 64 x 64. Immutable from admission through output emission
  // (section 18.4). Writer: admission. Reader: ordered retirement.
  //
  // V3-WREN-REG: ctxw_v_q
  // V3-BANK: OWNER_CONTEXT
  zhao_texture_v3bank #(.WIDTH(CTXW), .DEPTH(OWNERS)) u_ctx (
      .clk      (clk),
      .wr_en_i  (ctxw_v_q),
      .wr_addr_i(ctxw_addr_q),
      .wr_data_i(ctxw_data_q),
      .rd_addr_i(fin_rd_addr_q),
      .rd_data_o(ctx_rd_c)
  );

  // ---- RC capture (section 6.3): one fabric register, nothing before it ----
  logic [RESW-1:0] sres_cap_q [3];
  logic [RESW-1:0] ares_cap_q;
  logic [RESW-1:0] fres_cap_q;
  logic [CTXW-1:0] ctx_cap_q;

  // ==========================================================================
  // COMBINE INPUT HOLDING QUEUE (registers, depth CMBQD)
  // ==========================================================================
  localparam int unsigned CQPW = $clog2(CMBQD);
  logic [OWNERW-1:0]        cq_own_q [CMBQD];
  logic [RESW-1:0]          cq_s0_q  [CMBQD];
  logic [RESW-1:0]          cq_s1_q  [CMBQD];
  logic [RESW-1:0]          cq_s2_q  [CMBQD];
  logic [RESW-1:0]          cq_ax_q  [CMBQD];
  logic [CQPW:0]            cq_wp_q, cq_rp_q;
  logic [CQPW:0]            cq_occ_c;
  logic                     cq_push_c;
  assign cq_occ_c  = cq_wp_q - cq_rp_q;
  assign cq_push_c = k2_v_q;

  assign cmb_valid_o = (cq_occ_c != '0);
  assign cmb_owner_o = cq_own_q[cq_rp_q[CQPW-1:0]];
  assign cmb_s0_o    = cq_s0_q [cq_rp_q[CQPW-1:0]];
  assign cmb_s1_o    = cq_s1_q [cq_rp_q[CQPW-1:0]];
  assign cmb_s2_o    = cq_s2_q [cq_rp_q[CQPW-1:0]];
  assign cmb_aux_o   = cq_ax_q [cq_rp_q[CQPW-1:0]];
  assign cmb_fire_c  = cmb_valid_o && cmb_ready_i;

  // ==========================================================================
  // ORDERED RETIREMENT (section 18)
  // ==========================================================================
  logic [CNTW-1:0] out_res_q;
  logic            out_fire_c, fetch_fire_c;

  logic              g0_v_q, g1_v_q, g2_v_q;
  logic [OWNERW-1:0] g0_owner_q, g1_owner_q, g2_owner_q;

  localparam int unsigned OQPW = $clog2(OUTQD);
  logic [OWNERW-1:0] oq_own_q [OUTQD];
  logic [RESW-1:0]   oq_res_q [OUTQD];
  logic [CTXW-1:0]   oq_ctx_q [OUTQD];
  logic [OQPW:0]     oq_wp_q, oq_rp_q;
  logic [OQPW:0]     oq_occ_c;
  assign oq_occ_c = oq_wp_q - oq_rp_q;

  assign out_valid_o  = (oq_occ_c != '0);
  assign out_owner_o  = oq_own_q[oq_rp_q[OQPW-1:0]];
  assign out_result_o = oq_res_q[oq_rp_q[OQPW-1:0]];
  assign out_ctx_o    = oq_ctx_q[oq_rp_q[OQPW-1:0]];
  assign out_fire_c   = out_valid_o && out_ready_i;

  // F0. Section 18.1: unfetched work, a live owner with final_done at
  // fetch_head, and a RESERVED output slot -- reserved before the read is
  // launched, so the proof does not depend on the consumer staying ready
  // while the memory answers. Section 18.3: "Stop when the next owner is
  // incomplete; do not scan for a younger completed owner to skip the hole."
  // There is no scan here at all -- only fetch_q is ever examined.
  assign fetch_fire_c = (unf_cnt_q != '0)
                     && live_q[fetch_q] && fdn_q[fetch_q] && !ftc_q[fetch_q]
                     && ((out_res_q - CNTW'(out_fire_c)) < CNTW'(OUTQD));

  // ==========================================================================
  // QUIESCENCE (used by the generation-wrap drain)
  // ==========================================================================
  assign quiet_c = (live_cnt_q == '0) && (unf_cnt_q == '0)
                && !ctxw_v_q
                && !c0t_v_q && !c1t_v_q && !c3t_v_q && !c4t_v_q
                && !c0a_v_q && !c1a_v_q && !c3a_v_q && !c4a_v_q
                && !c0f_v_q && !c1f_v_q && !c3f_v_q && !c4f_v_q
                && !q0t_v_q && !q0a_v_q && !q0i_v_q
                && (rq_occ_c[0] == '0) && (rq_occ_c[1] == '0) && (rq_occ_c[2] == '0)
                && !k0_v_q && !k1_v_q && !k2_v_q
                && (cq_occ_c == '0) && (cmb_res_q == '0)
                && !g0_v_q && !g1_v_q && !g2_v_q
                && (oq_occ_c == '0) && (out_res_q == '0);
  assign ev_quiet_o = quiet_c;

  // ==========================================================================
  // THE ONE SCOREBOARD NEXT-STATE, COMPUTED ONCE PER OWNER
  // ==========================================================================
  // CLAUDE.md, and this repository's own fragrob defect: two nonblocking
  // assignments to the same register in one always_ff means the LAST one wins
  // and an event is silently lost. So every per-owner field gets ONE
  // combinational next value that all events fold into, and ONE assignment.
  // Appendix B.5's precedence is explicit at the bottom: admission
  // reinitialises its slot and overrides everything else for that slot.
  logic            live_n_c [OWNERS];
  logic [GENW-1:0] gen_n_c  [OWNERS];
  logic [3:0]      req_n_c  [OWNERS];
  logic [3:0]      iss_n_c  [OWNERS];
  logic [3:0]      clm_n_c  [OWNERS];
  logic [3:0]      cmt_n_c  [OWNERS];
  logic            rdy_n_c  [OWNERS];
  logic            cbi_n_c  [OWNERS];
  logic            fcl_n_c  [OWNERS];
  logic            fdn_n_c  [OWNERS];
  logic            ftc_n_c  [OWNERS];

  always_comb begin
    for (int unsigned i = 0; i < OWNERS; i++) begin
      live_n_c[i] = live_q[i];
      gen_n_c [i] = gen_q [i];
      req_n_c [i] = req_q [i];
      iss_n_c [i] = iss_q [i];
      clm_n_c [i] = clm_q [i];
      cmt_n_c [i] = cmt_q [i];
      rdy_n_c [i] = rdy_q [i];
      cbi_n_c [i] = cbi_q [i];
      fcl_n_c [i] = fcl_q [i];
      fdn_n_c [i] = fdn_q [i];
      ftc_n_c [i] = ftc_q [i];

      // ---- ISSUE ----
      if (iss_t_ok_c && (iss_t_slot_c == SLOTW'(i)))
        iss_n_c[i] = iss_n_c[i] | iss_t_bit_c;
      if (iss_a_ok_c && (iss_a_slot_c == SLOTW'(i)))
        iss_n_c[i] = iss_n_c[i] | SRC_AUX;

      // ---- CLAIM (C2). Section 8.6: merge masks with OR. ----
      if (c2t_acc_c && (c1t_slot_q == SLOTW'(i)))
        clm_n_c[i] = clm_n_c[i] | c1t_bit_c;
      if (c2a_acc_c && (c1a_slot_q == SLOTW'(i)))
        clm_n_c[i] = clm_n_c[i] | SRC_AUX;
      if (c2f_acc_c && (c1f_slot_q == SLOTW'(i)))
        fcl_n_c[i] = 1'b1;

      // ---- PUBLISH (C4), one edge after the payload write edge ----
      // Matched on the FULL owner handle, not the slot. Appendix B.5: "never
      // combine events from different generations merely because slot
      // matches." In ordinary operation the generation cannot change between
      // C2's claim and C4's publication -- the owner is not final until this
      // very source commits -- so this comparison is a fault-injection and
      // drain-boundary guard, and it costs one 8-bit compare per lane.
      if (c4t_v_q && (c4t_slot_q == SLOTW'(i)) && (gen_q[i] == c4t_gen_q))
        cmt_n_c[i] = cmt_n_c[i] | c4t_mask_q;
      if (c4a_v_q && (c4a_slot_q == SLOTW'(i)) && (gen_q[i] == c4a_gen_q))
        cmt_n_c[i] = cmt_n_c[i] | SRC_AUX;
      if (c4f_v_q && (c4f_slot_q == SLOTW'(i)) && (gen_q[i] == c4f_gen_q))
        fdn_n_c[i] = 1'b1;

      // ---- READY TICKET CLAIM, atomic with the reservation ----
      if (tkt_t_c && (c4t_slot_q == SLOTW'(i))) rdy_n_c[i] = 1'b1;
      if (tkt_a_c && (c4a_slot_q == SLOTW'(i))) rdy_n_c[i] = 1'b1;

      // ---- COMBINE ISSUE ----
      if (cmb_pop_c && (sel_data_c[OWNERW-1 -: SLOTW] == SLOTW'(i)))
        cbi_n_c[i] = 1'b1;

      // ---- FETCH LAUNCH (18.1: an owner can be fetched at most once) ----
      if (fetch_fire_c && (fetch_q == SLOTW'(i))) ftc_n_c[i] = 1'b1;

      // ---- OUTPUT RELEASE. Section 5.4: THE ONLY ordinary owner-free event.
      // Appendix D.6: the final write does NOT free the owner, prefetching
      // does NOT free the owner, COMBINE completing does NOT free the owner.
      if (out_fire_c && (emit_q == SLOTW'(i))) live_n_c[i] = 1'b0;

      // ---- ADMISSION, last and therefore highest precedence ----
      if (adm_fire_c && (tail_q == SLOTW'(i))) begin
        live_n_c[i] = 1'b1;
        gen_n_c [i] = adm_gen_c;
        req_n_c [i] = adm_req_i;
        iss_n_c [i] = 4'd0;
        clm_n_c [i] = 4'd0;
        cmt_n_c [i] = 4'd0;
        // A zero-work owner is eligible at admission and claims its one ticket
        // there (section 9.1: "Admission can create a zero-work ready owner").
        rdy_n_c [i] = (adm_req_i == 4'd0);
        cbi_n_c [i] = 1'b0;
        fcl_n_c [i] = 1'b0;
        fdn_n_c [i] = 1'b0;
        ftc_n_c [i] = 1'b0;
      end
    end
  end

  // ==========================================================================
  // DIAGNOSTIC DELTAS -- computed ONCE, assigned ONCE (section 19.7)
  // ==========================================================================
  // "Simultaneous TMU and AUX faults increment the error total by two." That
  // is the fragrob defect this repository already paid for once; the delta is
  // calculated in one place and the accumulator is assigned in one place.
  logic [1:0] d_stale_c, d_unsol_c, d_dup_c, d_commit_c, d_ticket_c, d_issue_c;
  logic       d_range_c, d_final_c;
  always_comb begin
    d_stale_c = 2'd0;
    if (c2t_stale_c) d_stale_c = d_stale_c + 2'd1;
    if (c2a_stale_c) d_stale_c = d_stale_c + 2'd1;
    d_unsol_c = 2'd0;
    if (c2t_unsol_c) d_unsol_c = d_unsol_c + 2'd1;
    if (c2a_unsol_c) d_unsol_c = d_unsol_c + 2'd1;
    d_dup_c = 2'd0;
    if (c2t_dup_c) d_dup_c = d_dup_c + 2'd1;
    if (c2a_dup_c) d_dup_c = d_dup_c + 2'd1;
    d_commit_c = 2'd0;
    if (c4t_v_q) d_commit_c = d_commit_c + 2'd1;
    if (c4a_v_q) d_commit_c = d_commit_c + 2'd1;
    d_ticket_c = 2'd0;
    if (tkt_t_c) d_ticket_c = d_ticket_c + 2'd1;
    if (tkt_a_c) d_ticket_c = d_ticket_c + 2'd1;
    if (adm_fire_c && (adm_req_i == 4'd0)) d_ticket_c = d_ticket_c + 2'd1;
    d_issue_c = 2'd0;
    if (iss_tmu_valid_i && !iss_t_ok_c) d_issue_c = d_issue_c + 2'd1;
    if (iss_aux_valid_i && !iss_a_ok_c) d_issue_c = d_issue_c + 2'd1;
    d_range_c = c2t_rng_bad_c;
    d_final_c = c2f_bad_c;
  end

  // ==========================================================================
  // SEQUENTIAL
  // ==========================================================================
  logic [CNTW-1:0] live_next_c;
  assign live_next_c = live_cnt_q + CNTW'(adm_fire_c) - CNTW'(out_fire_c);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int unsigned i = 0; i < OWNERS; i++) begin
        live_q[i] <= 1'b0;
        gen_q [i] <= '0;
        req_q [i] <= 4'd0;
        iss_q [i] <= 4'd0;
        clm_q [i] <= 4'd0;
        cmt_q [i] <= 4'd0;
        rdy_q [i] <= 1'b0;
        cbi_q [i] <= 1'b0;
        fcl_q [i] <= 1'b0;
        fdn_q [i] <= 1'b0;
        ftc_q [i] <= 1'b0;
      end
      tail_q     <= '0;
      emit_q     <= '0;
      fetch_q    <= '0;
      live_cnt_q <= '0;
      unf_cnt_q  <= '0;
      peak_q     <= '0;
    end else begin
      for (int unsigned i = 0; i < OWNERS; i++) begin
        live_q[i] <= live_n_c[i];
        gen_q [i] <= gen_n_c [i];
        req_q [i] <= req_n_c [i];
        iss_q [i] <= iss_n_c [i];
        clm_q [i] <= clm_n_c [i];
        cmt_q [i] <= cmt_n_c [i];
        rdy_q [i] <= rdy_n_c [i];
        cbi_q [i] <= cbi_n_c [i];
        fcl_q [i] <= fcl_n_c [i];
        fdn_q [i] <= fdn_n_c [i];
        ftc_q [i] <= ftc_n_c [i];
      end
      if (adm_fire_c)   tail_q  <= tail_q  + SLOTW'(1);
      if (out_fire_c)   emit_q  <= emit_q  + SLOTW'(1);
      if (fetch_fire_c) fetch_q <= fetch_q + SLOTW'(1);

      // ONE delta, ONE assignment: simultaneous admission and retirement is a
      // net owner-count change of zero (section 19.7).
      live_cnt_q <= live_next_c;
      unf_cnt_q  <= unf_cnt_q + CNTW'(adm_fire_c) - CNTW'(fetch_fire_c);

      // A RETAINED high-water mark. Section 19.7: "A statistic rebuilt fresh
      // on every query is current occupancy, not a historical peak."
      if (live_next_c > peak_q) peak_q <= live_next_c;
    end
  end

  // ---- admission context write (registered enable) -------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) ctxw_v_q <= 1'b0;
    else        ctxw_v_q <= adm_fire_c;
  end
  always_ff @(posedge clk) begin
    ctxw_addr_q <= tail_q;
    ctxw_data_q <= adm_ctx_i;
  end

  // ---- TMU return pipeline -------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      c0t_v_q  <= 1'b0;
      c1t_v_q  <= 1'b0;
      c3t_v_q  <= 1'b0;
      c3t_we_q <= 3'b000;
      c4t_v_q  <= 1'b0;
    end else begin
      c0t_v_q  <= tmu_rvalid_i;
      c1t_v_q  <= c0t_v_q;
      c3t_v_q  <= c2t_acc_c;
      c3t_we_q <= c2t_acc_c ? c1t_bit_c[2:0] : 3'b000;
      c4t_v_q  <= c3t_v_q;
    end
  end
  always_ff @(posedge clk) begin
    c0t_slot_q <= tmu_rhandle_i[SMPW-1 -: SLOTW];
    c0t_sidx_q <= tmu_rhandle_i[GENW+1 -: 2];
    c0t_gen_q  <= tmu_rhandle_i[GENW-1:0];
    c0t_res_q  <= tmu_rresult_i;
    c0t_rng_q  <= (tmu_rhandle_i[GENW+1 -: 2] != 2'd3);

    c1t_slot_q <= c0t_slot_q;
    c1t_sidx_q <= c0t_sidx_q;
    c1t_gen_q  <= c0t_gen_q;
    c1t_res_q  <= c0t_res_q;
    c1t_rng_q  <= c0t_rng_q;
    c1t_live_q <= live_q[c0t_slot_q];
    c1t_tgen_q <= gen_q [c0t_slot_q];
    c1t_req_q  <= req_q [c0t_slot_q];
    c1t_iss_q  <= iss_q [c0t_slot_q];
    c1t_clm_q  <= clm_q [c0t_slot_q];
    c1t_cmt_q  <= cmt_q [c0t_slot_q];

    c3t_slot_q <= c1t_slot_q;
    c3t_gen_q  <= c1t_gen_q;
    c3t_mask_q <= c1t_bit_c;
    c3t_data_q <= c1t_res_q;

    c4t_slot_q <= c3t_slot_q;
    c4t_gen_q  <= c3t_gen_q;
    c4t_mask_q <= c3t_mask_q;
  end

  // ---- AUX return pipeline -------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      c0a_v_q  <= 1'b0;
      c1a_v_q  <= 1'b0;
      c3a_v_q  <= 1'b0;
      c3a_we_q <= 1'b0;
      c4a_v_q  <= 1'b0;
    end else begin
      c0a_v_q  <= aux_rvalid_i;
      c1a_v_q  <= c0a_v_q;
      c3a_v_q  <= c2a_acc_c;
      c3a_we_q <= c2a_acc_c;
      c4a_v_q  <= c3a_v_q;
    end
  end
  always_ff @(posedge clk) begin
    c0a_slot_q <= aux_rowner_i[OWNERW-1 -: SLOTW];
    c0a_gen_q  <= aux_rowner_i[GENW-1:0];
    c0a_res_q  <= aux_rresult_i;

    c1a_slot_q <= c0a_slot_q;
    c1a_gen_q  <= c0a_gen_q;
    c1a_res_q  <= c0a_res_q;
    c1a_live_q <= live_q[c0a_slot_q];
    c1a_tgen_q <= gen_q [c0a_slot_q];
    c1a_req_q  <= req_q [c0a_slot_q];
    c1a_iss_q  <= iss_q [c0a_slot_q];
    c1a_clm_q  <= clm_q [c0a_slot_q];
    c1a_cmt_q  <= cmt_q [c0a_slot_q];

    c3a_slot_q <= c1a_slot_q;
    c3a_gen_q  <= c1a_gen_q;
    c3a_data_q <= c1a_res_q;

    c4a_slot_q <= c3a_slot_q;
    c4a_gen_q  <= c3a_gen_q;
  end

  // ---- FINAL return pipeline ----------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      c0f_v_q  <= 1'b0;
      c1f_v_q  <= 1'b0;
      c3f_v_q  <= 1'b0;
      c3f_we_q <= 1'b0;
      c4f_v_q  <= 1'b0;
    end else begin
      c0f_v_q  <= fin_valid_i;
      c1f_v_q  <= c0f_v_q;
      c3f_v_q  <= c2f_acc_c;
      c3f_we_q <= c2f_acc_c;
      c4f_v_q  <= c3f_v_q;
    end
  end
  always_ff @(posedge clk) begin
    c0f_slot_q <= fin_owner_i[OWNERW-1 -: SLOTW];
    c0f_gen_q  <= fin_owner_i[GENW-1:0];
    c0f_res_q  <= fin_result_i;

    c1f_slot_q <= c0f_slot_q;
    c1f_gen_q  <= c0f_gen_q;
    c1f_res_q  <= c0f_res_q;
    c1f_live_q <= live_q[c0f_slot_q];
    c1f_tgen_q <= gen_q [c0f_slot_q];
    c1f_cbi_q  <= cbi_q [c0f_slot_q];
    c1f_fcl_q  <= fcl_q [c0f_slot_q];
    c1f_fdn_q  <= fdn_q [c0f_slot_q];

    c3f_slot_q <= c1f_slot_q;
    c3f_gen_q  <= c1f_gen_q;
    c3f_data_q <= c1f_res_q;

    c4f_slot_q <= c3f_slot_q;
    c4f_gen_q  <= c3f_gen_q;
  end

  // ---- ready-queue write registers ----------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      q0t_v_q <= 1'b0;
      q0a_v_q <= 1'b0;
      q0i_v_q <= 1'b0;
    end else begin
      q0t_v_q <= tkt_t_c;
      q0a_v_q <= tkt_a_c;
      q0i_v_q <= adm_fire_c && (adm_req_i == 4'd0);
    end
  end
  always_ff @(posedge clk) begin
    q0t_owner_q <= {c4t_slot_q, c4t_gen_q};
    q0a_owner_q <= {c4a_slot_q, c4a_gen_q};
    q0i_owner_q <= {tail_q, adm_gen_c};
  end

  // ---- COMBINE admission read pipeline ------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      k0_v_q    <= 1'b0;
      k1_v_q    <= 1'b0;
      k2_v_q    <= 1'b0;
      rr_q      <= 2'd0;
      cmb_res_q <= '0;
      cq_wp_q   <= '0;
      cq_rp_q   <= '0;
    end else begin
      k0_v_q <= cmb_pop_c;
      k1_v_q <= k0_v_q;
      k2_v_q <= k1_v_q;
      if (cmb_pop_c) rr_q <= (sel_c == 2'd2) ? 2'd0 : (sel_c + 2'd1);
      cmb_res_q <= cmb_res_q + CNTW'(cmb_pop_c) - CNTW'(cmb_fire_c);
      if (cq_push_c)  cq_wp_q <= cq_wp_q + (CQPW+1)'(1);
      if (cmb_fire_c) cq_rp_q <= cq_rp_q + (CQPW+1)'(1);
    end
  end
  always_ff @(posedge clk) begin
    if (cmb_pop_c) cmb_rd_addr_q <= sel_data_c[OWNERW-1 -: SLOTW];
    if (cmb_pop_c) k0_owner_q <= sel_data_c;
    k1_owner_q <= k0_owner_q;
    k2_owner_q <= k1_owner_q;
    for (int unsigned s = 0; s < 3; s++) sres_cap_q[s] <= sres_rd_c[s];
    ares_cap_q <= ares_rd_c;
    if (cq_push_c) begin
      cq_own_q[cq_wp_q[CQPW-1:0]] <= k2_owner_q;
      cq_s0_q [cq_wp_q[CQPW-1:0]] <= sres_cap_q[0];
      cq_s1_q [cq_wp_q[CQPW-1:0]] <= sres_cap_q[1];
      cq_s2_q [cq_wp_q[CQPW-1:0]] <= sres_cap_q[2];
      cq_ax_q [cq_wp_q[CQPW-1:0]] <= ares_cap_q;
    end
  end

  // ---- retirement read pipeline -------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      g0_v_q    <= 1'b0;
      g1_v_q    <= 1'b0;
      g2_v_q    <= 1'b0;
      out_res_q <= '0;
      oq_wp_q   <= '0;
      oq_rp_q   <= '0;
    end else begin
      g0_v_q <= fetch_fire_c;
      g1_v_q <= g0_v_q;
      g2_v_q <= g1_v_q;
      out_res_q <= out_res_q + CNTW'(fetch_fire_c) - CNTW'(out_fire_c);
      if (g2_v_q)     oq_wp_q <= oq_wp_q + (OQPW+1)'(1);
      if (out_fire_c) oq_rp_q <= oq_rp_q + (OQPW+1)'(1);
    end
  end
  always_ff @(posedge clk) begin
    if (fetch_fire_c) fin_rd_addr_q <= fetch_q;
    if (fetch_fire_c) g0_owner_q <= {fetch_q, gen_q[fetch_q]};
    g1_owner_q <= g0_owner_q;
    g2_owner_q <= g1_owner_q;
    fres_cap_q <= fres_rd_c;
    ctx_cap_q  <= ctx_rd_c;
    if (g2_v_q) begin
      oq_own_q[oq_wp_q[OQPW-1:0]] <= g2_owner_q;
      oq_res_q[oq_wp_q[OQPW-1:0]] <= fres_cap_q;
      oq_ctx_q[oq_wp_q[OQPW-1:0]] <= ctx_cap_q;
    end
  end

  // ---- evidence counters ---------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      ev_admitted_o    <= 32'd0;
      ev_emitted_o     <= 32'd0;
      ev_commits_o     <= 32'd0;
      ev_tickets_o     <= 32'd0;
      ev_err_range_o   <= 32'd0;
      ev_err_stale_o   <= 32'd0;
      ev_err_unsol_o   <= 32'd0;
      ev_err_dup_o     <= 32'd0;
      ev_err_final_o   <= 32'd0;
      ev_err_issue_o   <= 32'd0;
      ev_wrap_drains_o <= 32'd0;
    end else begin
      ev_admitted_o    <= ev_admitted_o    + 32'(adm_fire_c);
      ev_emitted_o     <= ev_emitted_o     + 32'(out_fire_c);
      ev_commits_o     <= ev_commits_o     + 32'(d_commit_c);
      ev_tickets_o     <= ev_tickets_o     + 32'(d_ticket_c);
      ev_err_range_o   <= ev_err_range_o   + 32'(d_range_c);
      ev_err_stale_o   <= ev_err_stale_o   + 32'(d_stale_c);
      ev_err_unsol_o   <= ev_err_unsol_o   + 32'(d_unsol_c);
      ev_err_dup_o     <= ev_err_dup_o     + 32'(d_dup_c);
      ev_err_final_o   <= ev_err_final_o   + 32'(d_final_c);
      ev_err_issue_o   <= ev_err_issue_o   + 32'(d_issue_c);
      ev_wrap_drains_o <= ev_wrap_drains_o + 32'(adm_fire_c && wrap_block_c);
    end
  end

  assign ev_live_o      = live_cnt_q;
  assign ev_live_peak_o = peak_q;

  // ==========================================================================
  // ASSERTION CONTRACTS (Appendix B.7)
  // ==========================================================================
  // armed_q is a plain SYNCHRONOUS flag that says "reset has released". Reading
  // rst_n synchronously in a block where it is also an asynchronous reset
  // raises SYNCASYNCNET in the simulator's lint, and that is a real caution
  // rather than a style note: a net used both ways is a net whose timing
  // closure is being asked for twice.
  //
  // (That sentence is phrased to avoid opening a comment line with the
  // simulator's name. A `//` line that BEGINS with it is read as a pragma and
  // the file stops lexing -- "%Error-BADVLTPRAGMA: Unknown verilator comment".
  // This file hit it on its first lint, which is the fifth time in this
  // repository.)
  //
  // And it is armed by RESET, symmetrically -- reports/
  // V3-DIAGNOSIS-VERIFICATION-20260906.md section 4 item 6: `bil_expect_r` is
  // a checker that does NOT reset while its issuer does, giving a false
  // negative at cold start and a sticky false positive after a warm reset, so
  // its zero counter is no evidence about anything. Every checker register
  // below resets with the thing it checks.
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else        armed_q <= 1'b1;
  end

  // out_valid && !out_ready |=> stable(entire_output_packet). Checked against a
  // real previous-cycle snapshot rather than by hoping the FIFO head is stable.
  logic              ost_v_q;
  logic [OWNERW-1:0] ost_own_q;
  logic [RESW-1:0]   ost_res_q;
  logic [CTXW-1:0]   ost_ctx_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      ost_v_q   <= 1'b0;
      ost_own_q <= '0;
      ost_res_q <= '0;
      ost_ctx_q <= '0;
    end else begin
      ost_v_q   <= out_valid_o && !out_ready_i;
      ost_own_q <= out_owner_o;
      ost_res_q <= out_result_o;
      ost_ctx_q <= out_ctx_o;
    end
  end

  // c3*_we_seen_q makes "committed rises only after the payload write edge"
  // checkable without $past.
  logic c3_we_seen_t_q, c3_we_seen_a_q, c3_we_seen_f_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      c3_we_seen_t_q <= 1'b0;
      c3_we_seen_a_q <= 1'b0;
      c3_we_seen_f_q <= 1'b0;
    end else begin
      c3_we_seen_t_q <= (c3t_we_q != 3'b000);
      c3_we_seen_a_q <= c3a_we_q;
      c3_we_seen_f_q <= c3f_we_q;
    end
  end

  always_ff @(posedge clk) begin
    if (armed_q) begin
      a_owner_bound      : assert (live_cnt_q <= CNTW'(OWNERS));
      a_unfetched_bound  : assert (unf_cnt_q  <= live_cnt_q);
      a_out_reserved     : assert (out_res_q  <= CNTW'(OUTQD));
      a_cmb_reserved     : assert (cmb_res_q  <= CNTW'(CMBQD));
      a_outq_bound       : assert (oq_occ_c   <= (OQPW+1)'(OUTQD));
      a_cmbq_bound       : assert (cq_occ_c   <= (CQPW+1)'(CMBQD));

      // sample_claim -> live && generation_matches && sample_index<3
      //                 && required && issued && !already_claimed
      a_sample_claim : assert (!c2t_acc_c || (c1t_live_q
                        && (c1t_tgen_q == c1t_gen_q)
                        && (c1t_sidx_q != 2'd3)
                        && ((c1t_req_q & c1t_bit_c) != 4'd0)
                        && ((c1t_iss_q & c1t_bit_c) != 4'd0)
                        && ((c1t_clm_q & c1t_bit_c) == 4'd0)));

      // sample_ram_write -> prior accepted commit packet && legal bank
      a_ram_write_onehot : assert ((c3t_we_q == 3'b000) || (c3t_we_q == 3'b001)
                                || (c3t_we_q == 3'b010) || (c3t_we_q == 3'b100));
      a_ram_write_claimed : assert ((c3t_we_q == 3'b000) || c3t_v_q);

      // committed_bit_rises -> payload_write_already_occurred
      a_commit_after_write_t : assert (!c4t_v_q || c3_we_seen_t_q);
      a_commit_after_write_a : assert (!c4a_v_q || c3_we_seen_a_q);
      a_commit_after_write_f : assert (!c4f_v_q || c3_we_seen_f_q);

      // ready_ticket_insert(owner) -> !prior_ready_claimed, and at most ONE
      // ticket per owner per edge (Appendix D.2).
      a_ticket_once_t : assert (!tkt_t_c || !rdy_q[c4t_slot_q]);
      a_ticket_once_a : assert (!tkt_a_c || !rdy_q[c4a_slot_q]);
      a_ticket_coalesced : assert (!(same_owner_c && tkt_t_c && tkt_a_c));

      // combine_admit(owner) -> ready_claimed && !combine_issued
      a_combine_admit : assert (!cmb_pop_c
                          || (rdy_q[sel_data_c[OWNERW-1 -: SLOTW]]
                              && !cbi_q[sel_data_c[OWNERW-1 -: SLOTW]]));

      // final_read_launch -> unfetched>0 && live && final_done && !fetched
      a_fetch_launch : assert (!fetch_fire_c
                          || ((unf_cnt_q != '0) && live_q[fetch_q]
                              && fdn_q[fetch_q] && !ftc_q[fetch_q]));

      // output_fire(owner) -> owner == emit_head_handle && owner_is_live
      a_out_in_order : assert (!out_fire_c
                          || ((out_owner_o[OWNERW-1 -: SLOTW] == emit_q)
                              && live_q[emit_q]));

      // out_valid && !out_ready |=> stable(entire packet)
      a_out_stable : assert (!ost_v_q || (out_valid_o
                          && (out_owner_o  == ost_own_q)
                          && (out_result_o == ost_res_q)
                          && (out_ctx_o    == ost_ctx_q)));

      // An admission must never overwrite a live owner's row (section 6.4).
      a_no_live_overwrite : assert (!adm_fire_c || !live_q[tail_q]);

      // The forwarding window this design was proved for.
      a_fwd_window : assert (FWD_WINDOW == 1);

      // THE REJECTION CLASSES PARTITION THE TRAFFIC.
      // Every presented packet has exactly ONE outcome: out of range, stale,
      // unsolicited, duplicate, or accepted. This is what makes the separate
      // ev_err_* ports addable -- section 19.7 wants "current occupancy, peak
      // occupancy, cumulative stalls and outstanding-service age" to have
      // distinct names and tests, and the same discipline applies to error
      // classes. A packet counted twice inflates two totals; a packet counted
      // zero times is the silent-drop failure this repository has already paid
      // for. The sum is the enforcement.
      a_reject_partition_t : assert (
          (4'(c2t_rng_bad_c) + 4'(c2t_stale_c) + 4'(c2t_unsol_c)
           + 4'(c2t_dup_c) + 4'(c2t_acc_c)) == 4'(c1t_v_q));
      a_reject_partition_a : assert (
          (4'(c2a_stale_c) + 4'(c2a_unsol_c) + 4'(c2a_dup_c) + 4'(c2a_acc_c))
          == 4'(c1a_v_q));

      // A ready queue must never be written when full.
      a_rq_not_full : assert (!((q0t_v_q && rq_full_c[0])
                             || (q0a_v_q && rq_full_c[1])
                             || (q0i_v_q && rq_full_c[2])));
    end
  end

endmodule

`default_nettype wire
