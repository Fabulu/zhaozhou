// tb_warp_field_chain_winidx_mutant.sv -- THE MUTANT ARRANGEMENT.
// THIS IS NOT PRODUCTION RTL and it is not a test bench anybody should reach
// for. It is a COPY of tests/field/tb_warp_field_chain.sv differing in exactly
// TWO textual substitutions, ONE of which is substantive:
//
//   production:  zhao_field_host_v2 #( ... ) u_host ( ... );
//   here:        zhao_field_host_v2_winidx_mutant #( ... ) u_host ( ... );
//
// The other is the bench's own module name, so the two can be elaborated in
// one tree.
//
// WHY THE COPY IS THE RIGHT SHAPE HERE. SystemVerilog has no way to make the
// instantiated module a parameter, so "the same bench with a different host"
// can only be spelled as a second file. Keeping the difference down to one
// instantiation line is what makes the pair a LIKE-FOR-LIKE comparison rather
// than two experiments: `zhao_geom_warp` and `zhao_field_warp_adapter` below
// are the REAL production modules in both arrangements, and the stimulus is
// built by the same shared header in both drivers.
//
// WHAT IT IS EVIDENCE ABOUT. `zhao_field_host_v2_winidx_mutant` publishes its
// results WINDOW-indexed, which is the old `zhao_field_host`'s meaning and
// owner ruling R168's defect. Driven by the SPARSE program -- Warp's six
// canonical outputs at R15,R16,R17,R18,R19 and R21, so ordinal 5 lives at R21
// and window lane 5 (R20) is never written -- this arrangement hands the
// adapter the cleared zero from R20 where the program's nz' should be.
//
// Its driver, tests/mutants/warp_sparse_ordinal_mutant_driver.cpp, HAS
// INVERTED POLARITY: it passes when that wrong value IS detected. It is
// evidence about the INSTRUMENT, not about the design.
//
// REGENERATE IT if tests/field/tb_warp_field_chain.sv changes shape. This is a
// COPY and a stale copy is a positive control for an arrangement that no
// longer exists.

// tb_warp_field_chain_winidx_mutant.sv -- THE THREE REAL MODULES, JOINED.
//
// `zhao_geom_warp` -> `zhao_field_warp_adapter` -> `zhao_field_host_v2`.
// No played adapter, no played host, no stubbed response. This is the bench
// that answers whether GEOM.WARP can be a REAL client of the ONE shared Field
// fabric, and it answers it by running a real Warp program on the real v3
// engine and carrying the six canonical output words back into the application
// law.
//
// WHY IT EXISTS. Before this file, `zhao_field_warp_adapter` had a directed
// test that STUBBED the host by hand (`d.resp_status_i = 0; for (i<7)
// d.resp_out_i[i] = 0;`), `zhao_geom_warp` had a directed test that PLAYED the
// adapter, and `zhao_field_host_v2` had a directed test that drove its client
// ports from C++. Three green suites, and NOTHING anywhere instantiated any two
// of them together. Each was correct about its own half; none was evidence
// about the seam. W18 asks for "real command -> real binding -> real Field v3
// execution -> coherent warped vertex", and a bench of stubs cannot supply the
// middle of that sentence.
//
// WHAT IT DOES NOT CLAIM. There is no `DrawWarpedForm` command, so the leftmost
// term of W18's chain is absent and this bench drives the per-draw descriptor
// directly. It is evidence about the BINDING, the EXECUTION and the
// APPLICATION. It is not evidence about the command path, and the packet's
// findings say so in those words.
//
// ---------------------------------------------------------------------------
// THE CLIENT INDEX IS 2, DELIBERATELY.
// ---------------------------------------------------------------------------
// The host is instantiated at CLIENTS=3 with Warp on index 2 and indices 0 and
// 1 idle, because that is exactly the arrangement R103's prerequisite P2 asks
// for: the console composes CLIENTS=2 with the stamp and flow adapters holding
// both ports, and Warp needs a THIRD. Wiring Warp to client 0 would have been
// simpler and would have proved less -- it would not have shown that a client
// at a nonzero index is granted, that its response is selected by
// `resp_valid_o[2]`, or that the round-robin reaches it.
//
// ---------------------------------------------------------------------------
// TWO PARAMETERS NAMED `OUT_LANES`, MEANING DIFFERENT THINGS. READ THIS.
// ---------------------------------------------------------------------------
// This is the sharpest edge in the whole composition and nothing in either
// port list points at it.
//
//   * `zhao_field_host_v2.OUT_LANES` is the CAPTURE WINDOW width. It is pinned
//     to exactly 7 by an elaboration equality against the generated schema's
//     `ZFH_WINDOW_MASK_BITS` (R126's guard). It sizes `resp_window_o`, which is
//     the ONLY window-indexed port on that module.
//   * `zhao_field_host_v2.OUT_ORDINALS` is the CANONICAL OUTPUT COUNT. It sizes
//     `resp_out_o` and `resp_present_o`, which are ORDINAL-indexed. Warp has
//     six canonical outputs, so it is 6.
//   * `zhao_field_warp_adapter.OUT_LANES` sizes `resp_out_i` -- and that port
//     is ORDINAL-indexed, as the adapter's own header states. So it must be
//     connected to the host's OUT_ORDINALS, NOT to the host's OUT_LANES.
//
// Hence `.OUT_LANES(W_ORDINALS)` on the adapter below, where the host gets
// `.OUT_ORDINALS(W_ORDINALS)` and `.OUT_LANES(HOST_WINDOW)`. Leave the adapter
// at its default 7 against a 6-ordinal host and the widths silently disagree by
// one word.
//
// AND THE DEEPER HAZARD, WHICH THIS BENCH IS THE ONLY DEFENCE AGAINST: the
// adapter's host-side port SET matches the OLD `zhao_field_host` name for name,
// while its MEANING matches `zhao_field_host_v2`. The old host's `resp_out_o`
// is WINDOW-indexed; the new one's is ORDINAL-indexed. Connect the adapter to
// the old host and it elaborates cleanly, runs cleanly, and reads window
// positions as if they were ordinals -- which is exactly the ordinal-versus-
// window confusion the whole FIELD repair exists to prevent. Nothing in the
// port list distinguishes the correct wiring from the wrong one. The
// SPARSE-OUTPUT case in the driver is what distinguishes them, and it is the
// reason that case is in the bench at all.
//
// SINCE OWNER RULING R168 BOTH HALVES OF THAT ARE REPAIRED, and this comment
// is kept because the reasoning is still the reasoning:
//
//   * the adapter now has a `resp_present_i` port, which the OLD host does not
//     offer -- so the wrong wiring is a PINMISSING rather than a silent
//     success, and the hazard is no longer invisible to the port list;
//   * the SPARSE case is no longer only a case in this driver. It is the named
//     test `warp_sparse_ordinal_directed`, with an inverted-polarity positive
//     control `warp_sparse_ordinal_winidx_mutant` proving it discriminates.
//
// ---------------------------------------------------------------------------
// The parameters are named constants, per CLAUDE.md rule 6.

`default_nettype none

module tb_warp_field_chain_winidx_mutant #(
    // Warp's canonical shape. W01: FIFTEEN in, SIX out. Not fourteen.
    parameter int unsigned W_IN_LANES = 15,
    parameter int unsigned W_ORDINALS = 6,
    // The host's capture window, pinned to the generated schema by R126's
    // elaboration guard. It is NOT the ordinal count and it is NOT six.
    parameter int unsigned HOST_WINDOW = 7,
    // Three clients, Warp on index 2 -- prerequisite P2's arrangement.
    parameter int unsigned CLIENTS = 3,
    parameter int unsigned WARP_CLIENT = 2,
    parameter int unsigned SLOTW = 3,
    parameter int unsigned LDADDRW = 7,
    // One credit makes the response reservation deterministic, so a miscompare
    // is a miscompare and not a race. FH20's pool is exercised by the host's own
    // directed test; this bench is about the seam.
    parameter int unsigned CREDITS = 1,
    parameter int unsigned SRCW = 16
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the post-skin vertex, into zhao_geom_warp --------------------------
    input  var logic               v_valid_i,
    output var logic               v_ready_o,
    input  var logic signed [31:0] v_px_i,
    input  var logic signed [31:0] v_py_i,
    input  var logic signed [31:0] v_pz_i,
    input  var logic signed [63:0] v_nx_i,
    input  var logic signed [63:0] v_ny_i,
    input  var logic signed [63:0] v_nz_i,
    input  var logic               v_n_degenerate_i,
    input  var logic [127:0]       v_attr_i,
    input  var logic [SRCW-1:0]    v_src_id_i,

    // ---- the per-draw descriptor [W05] --------------------------------------
    input  var logic               d_warp_en_i,
    input  var logic [SLOTW-1:0]   d_slot_i,
    input  var logic               d_slot_valid_i,
    input  var logic [7:0]         d_profile_i,
    input  var logic [31:0]        d_time_i,
    input  var logic [127:0]       d_par_i,
    input  var logic signed [31:0] d_bx_i,
    input  var logic signed [31:0] d_by_i,
    input  var logic signed [31:0] d_bz_i,

    // ---- the host's program load port, straight through ---------------------
    input  var logic               ld_valid_i,
    output var logic               ld_ready_o,
    input  var logic [2:0]         ld_kind_i,
    input  var logic [SLOTW-1:0]   ld_slot_i,
    input  var logic [LDADDRW-1:0] ld_addr_i,
    input  var logic [95:0]        ld_data_i,
    input  var logic               cfg_slow_clear_i,

    // ---- the published vertex ----------------------------------------------
    output var logic               o_p_valid_o,
    input  var logic               o_p_ready_i,
    output var logic signed [31:0] o_px_o,
    output var logic signed [31:0] o_py_o,
    output var logic signed [31:0] o_pz_o,
    output var logic               o_n_valid_o,
    input  var logic               o_n_ready_i,
    output var logic signed [31:0] o_nx_o,
    output var logic signed [31:0] o_ny_o,
    output var logic signed [31:0] o_nz_o,
    output var logic               o_n_degenerate_o,
    output var logic [SRCW-1:0]    o_p_src_id_o,

    output var logic               poison_valid_o,
    output var logic [2:0]         poison_cause_o,

    // ---- evidence from ALL THREE modules -----------------------------------
    // Read together, these are the conservation equations of contract section
    // 10: the host's completed responses, the adapter's vertices, and the
    // application's successful outcomes must agree.
    output var logic [31:0] w_transformed_o,
    output var logic [31:0] w_bypassed_o,
    output var logic [31:0] w_bound_viol_o,
    output var logic [31:0] w_field_faults_o,
    output var logic [31:0] w_p_accepts_o,
    output var logic [31:0] w_n_accepts_o,

    output var logic [31:0] a_vertices_o,
    output var logic [31:0] a_identities_o,
    output var logic [31:0] a_bypassed_o,
    output var logic [31:0] a_noprog_o,
    output var logic [31:0] a_faults_o,
    // R168: the run said OK and the record was short. Separate from faults.
    output var logic [31:0] a_absent_outputs_o,
    output var logic [31:0] a_stall_cycles_o,

    output var logic [31:0] h_runs_o,
    output var logic [31:0] h_run_faults_o,
    output var logic [31:0] h_noprog_o,
    output var logic [31:0] h_no_result_o,
    output var logic [31:0] h_out_incomplete_o,
    output var logic [31:0] h_bad_image_o,
    output var logic [31:0] h_zero_mask_o,
    output var logic [31:0] h_late_write_o,
    output var logic [31:0] h_credit_stall_o,
    output var logic [31:0] h_fast_path_o,
    output var logic [31:0] h_slow_path_o,
    output var logic [31:0] h_grants_o,
    output var logic [31:0] h_prep_bad_o,
    output var logic [3:0]  h_num_status_o,
    output var logic [HOST_WINDOW-1:0] h_resp_window_o,
    output var logic [W_ORDINALS-1:0]  h_resp_present_o,
    output var logic [3:0]  h_resp_count_o
);

  // ---- geom_warp <-> adapter ------------------------------------------------
  logic               w_vtx_valid, w_vtx_take, w_slot_valid;
  logic signed [31:0] w_px, w_py, w_pz, w_nx, w_ny, w_nz;
  logic [127:0]       w_attr, w_par;
  logic [31:0]        w_time;
  logic [SLOTW-1:0]   w_slot;
  logic [7:0]         w_profile;
  logic               a_ans_valid, a_warp_valid;
  logic signed [31:0] a_dx, a_dy, a_dz, a_nx, a_ny, a_nz;

  // ---- adapter <-> host (client WARP_CLIENT) --------------------------------
  logic                     ad_req_valid, ad_req_ready, ad_req_noprog;
  logic [SLOTW-1:0]         ad_req_slot;
  logic [W_IN_LANES*32-1:0] ad_req_in;
  logic                     ad_resp_valid, ad_resp_ready;
  logic [W_ORDINALS*32-1:0] host_resp_out;
  logic [7:0]               host_resp_status;

  logic [CLIENTS-1:0]             req_valid_v, req_ready_v, req_noprog_v;
  logic [CLIENTS*SLOTW-1:0]       req_slot_v;
  logic [CLIENTS*W_IN_LANES*32-1:0] req_in_v;
  logic [CLIENTS-1:0]             resp_valid_v, resp_ready_v;

  // Only WARP_CLIENT is driven; the other two are idle, exactly as an unused
  // console client port would be. They are held at zero rather than left
  // floating so that a grant to an idle client would be a visible fault rather
  // than an X propagating into the arbiter.
  always_comb begin
    req_valid_v  = '0;
    req_noprog_v = '0;
    req_slot_v   = '0;
    req_in_v     = '0;
    resp_ready_v = '0;
    req_valid_v[WARP_CLIENT]  = ad_req_valid;
    req_noprog_v[WARP_CLIENT] = ad_req_noprog;
    req_slot_v[WARP_CLIENT*SLOTW +: SLOTW] = ad_req_slot;
    req_in_v[WARP_CLIENT*W_IN_LANES*32 +: W_IN_LANES*32] = ad_req_in;
    resp_ready_v[WARP_CLIENT] = ad_resp_ready;
  end
  assign ad_req_ready  = req_ready_v[WARP_CLIENT];
  // The host's response PAYLOAD is a single shared bus; only `resp_valid_o[c]`
  // selects the owner. The adapter samples it solely while it is waiting and
  // its own valid bit is high, which is what makes that safe.
  assign ad_resp_valid = resp_valid_v[WARP_CLIENT];

  // PINCONNECTEMPTY is waived HERE, in the bench, and nowhere else. Each `()`
  // below is an output this bench deliberately does not observe -- the three
  // modules between them expose about ninety counters and traces, and routing
  // every one to a bench port would bury the dozen that carry the argument.
  // The waiver is scoped to the instantiations rather than passed on the lint
  // command line, so it cannot silently cover a production file: `lint_geom_warp`
  // still lints zhao_geom_warp.sv at full -Wall with no waiver at all.
  /* verilator lint_off PINCONNECTEMPTY */
  zhao_geom_warp #(
      .SRCW (SRCW),
      .SLOTW(SLOTW)
  ) u_warp (
      .clk(clk), .rst_n(rst_n),
      .v_valid_i(v_valid_i), .v_ready_o(v_ready_o),
      .v_px_i(v_px_i), .v_py_i(v_py_i), .v_pz_i(v_pz_i),
      .v_nx_i(v_nx_i), .v_ny_i(v_ny_i), .v_nz_i(v_nz_i),
      .v_n_degenerate_i(v_n_degenerate_i),
      .v_attr_i(v_attr_i), .v_src_id_i(v_src_id_i),
      .d_warp_en_i(d_warp_en_i), .d_slot_i(d_slot_i),
      .d_slot_valid_i(d_slot_valid_i), .d_profile_i(d_profile_i),
      .d_time_i(d_time_i), .d_par_i(d_par_i),
      .d_bx_i(d_bx_i), .d_by_i(d_by_i), .d_bz_i(d_bz_i),
      .f_vtx_valid_o(w_vtx_valid),
      .f_px_o(w_px), .f_py_o(w_py), .f_pz_o(w_pz),
      .f_nx_o(w_nx), .f_ny_o(w_ny), .f_nz_o(w_nz),
      .f_attr_o(w_attr), .f_vtx_take_o(w_vtx_take),
      .f_time_o(w_time), .f_par_o(w_par),
      .f_slot_o(w_slot), .f_slot_valid_o(w_slot_valid), .f_profile_o(w_profile),
      .f_ans_valid_i(a_ans_valid), .f_warp_valid_i(a_warp_valid),
      .f_dx_i(a_dx), .f_dy_i(a_dy), .f_dz_i(a_dz),
      .f_onx_i(a_nx), .f_ony_i(a_ny), .f_onz_i(a_nz),
      .o_p_valid_o(o_p_valid_o), .o_p_ready_i(o_p_ready_i),
      .o_px_o(o_px_o), .o_py_o(o_py_o), .o_pz_o(o_pz_o),
      .o_p_src_id_o(o_p_src_id_o),
      .o_n_valid_o(o_n_valid_o), .o_n_ready_i(o_n_ready_i),
      .o_nx_o(o_nx_o), .o_ny_o(o_ny_o), .o_nz_o(o_nz_o),
      .o_n_degenerate_o(o_n_degenerate_o), .o_n_src_id_o(),
      .poison_valid_o(poison_valid_o), .poison_src_id_o(),
      .poison_cause_o(poison_cause_o),
      .poison_dx_o(), .poison_dy_o(), .poison_dz_o(),
      .vertices_transformed_o(w_transformed_o),
      .bypassed_o(w_bypassed_o),
      .app_saturations_o(), .normal_reduced_o(), .degenerate_o(),
      .bound_violations_o(w_bound_viol_o),
      .negative_bounds_o(), .normal_width_faults_o(), .profile_mismatches_o(),
      .field_faults_o(w_field_faults_o),
      .p_accepts_o(w_p_accepts_o), .n_accepts_o(w_n_accepts_o)
  );

  // `.OUT_LANES(W_ORDINALS)` -- see the header. This port is ORDINAL-indexed
  // and must follow the host's OUT_ORDINALS, never its OUT_LANES.
  zhao_field_warp_adapter #(
      .SLOTW    (SLOTW),
      .IN_LANES (W_IN_LANES),
      .OUT_LANES(W_ORDINALS),
      .WARP_PROFILE_ID(8'd1)
  ) u_adapter (
      .clk(clk), .rst_n(rst_n),
      .vtx_valid_i(w_vtx_valid),
      .px_i(w_px), .py_i(w_py), .pz_i(w_pz),
      .nx_i(w_nx), .ny_i(w_ny), .nz_i(w_nz),
      .attr_i(w_attr), .vtx_take_i(w_vtx_take),
      .time_i(w_time), .par_i(w_par),
      .slot_i(w_slot), .slot_valid_i(w_slot_valid), .prog_profile_i(w_profile),
      .req_valid_o(ad_req_valid), .req_ready_i(ad_req_ready),
      .req_slot_o(ad_req_slot), .req_noprog_o(ad_req_noprog),
      .req_in_o(ad_req_in),
      .resp_valid_i(ad_resp_valid), .resp_ready_o(ad_resp_ready),
      // R168: the adapter READS presence now. `h_resp_present_o` is the
      // host's ordinal-indexed `output_present_mask` and this is the
      // connection that makes the old host structurally unusable here -- it
      // has no such port to offer.
      .resp_out_i(host_resp_out), .resp_present_i(h_resp_present_o),
      .resp_status_i(host_resp_status),
      .ans_valid_o(a_ans_valid), .warp_valid_o(a_warp_valid),
      .dx_o(a_dx), .dy_o(a_dy), .dz_o(a_dz),
      .nx_o(a_nx), .ny_o(a_ny), .nz_o(a_nz),
      .vertices_o(a_vertices_o), .identities_o(a_identities_o),
      .bypassed_o(a_bypassed_o), .noprog_o(a_noprog_o),
      .sig_refused_o(), .faults_o(a_faults_o),
      .absent_outputs_o(a_absent_outputs_o),
      .stall_cycles_o(a_stall_cycles_o), .vtx_changed_o()
  );

  zhao_field_host_v2_winidx_mutant #(
      .CLIENTS     (CLIENTS),
      .IN_LANES    (W_IN_LANES),
      .OUT_LANES   (HOST_WINDOW),
      .OUT_ORDINALS(W_ORDINALS),
      .CREDITS     (CREDITS),
      .SLOTW       (SLOTW),
      .LDADDRW     (LDADDRW)
  ) u_host (
      .clk(clk), .rst_n(rst_n),
      .cfg_slow_clear_i(cfg_slow_clear_i),
      .ld_valid_i(ld_valid_i), .ld_ready_o(ld_ready_o),
      .ld_kind_i(ld_kind_i), .ld_slot_i(ld_slot_i),
      .ld_addr_i(ld_addr_i), .ld_data_i(ld_data_i),
      // FIELD.PROGCACHE is instantiated INSIDE the host and exported whole.
      // This bench binds a resident slot directly, so the directory is idle --
      // and idle means its request valids low and its response readys HIGH, or
      // the host would stall waiting to hand back a response nobody takes.
      .pc_lu_valid_i(1'b0), .pc_lu_ready_o(), .pc_lu_hash_i(32'd0),
      .pc_lu_resp_valid_o(), .pc_lu_resp_ready_i(1'b1),
      .pc_lu_hit_o(), .pc_lu_slot_o(),
      .pc_cm_valid_i(1'b0), .pc_cm_ready_o(), .pc_cm_hash_i(32'd0),
      .pc_cm_ok_i(1'b0), .pc_cm_resp_valid_o(), .pc_cm_resp_ready_i(1'b1),
      .pc_cm_inserted_o(), .pc_cm_evicted_o(), .pc_cm_slot_o(),
      .pc_hits_o(), .pc_misses_o(), .pc_rejected_o(), .pc_evictions_o(),
      .pc_occupancy_o(),
      .req_valid_i(req_valid_v), .req_ready_o(req_ready_v),
      .req_slot_i(req_slot_v), .req_noprog_i(req_noprog_v),
      .req_in_i(req_in_v),
      .resp_valid_o(resp_valid_v), .resp_ready_i(resp_ready_v),
      .resp_out_o(host_resp_out),
      .resp_present_o(h_resp_present_o),
      .resp_window_o(h_resp_window_o),
      .resp_count_o(h_resp_count_o),
      .resp_status_o(host_resp_status),
      .rcp0_i(1'b0),
      .runs_o(h_runs_o), .run_faults_o(h_run_faults_o), .noprog_o(h_noprog_o),
      .instr_retired_o(), .loads_o(), .load_defers_o(),
      .grants_o(h_grants_o), .contended_grants_o(), .ld_oob_o(),
      .no_result_o(h_no_result_o), .out_incomplete_o(h_out_incomplete_o),
      .prep_bad_o(h_prep_bad_o), .bad_image_o(h_bad_image_o),
      .zero_mask_o(h_zero_mask_o), .late_write_o(h_late_write_o),
      .fence_writes_o(), .uniform_runs_o(),
      .credit_stall_o(h_credit_stall_o),
      .fast_path_o(h_fast_path_o), .slow_path_o(h_slow_path_o),
      .exec_desync_o(), .bank_desync_o(), .svc_bank_desync_o(),
      .tag_mismatch_o(), .wrong_op_o(), .unsupported_o(),
      .skid_overflow_o(), .uniform_bad_o(),
      .num_status_o(h_num_status_o)
  );
  /* verilator lint_on PINCONNECTEMPTY */

  initial begin
    if (WARP_CLIENT >= CLIENTS)
      $fatal(1, "tb_warp_field_chain_winidx_mutant: WARP_CLIENT=%0d is not a client of CLIENTS=%0d",
             WARP_CLIENT, CLIENTS);
    if (W_ORDINALS != 6)
      $fatal(1, "tb_warp_field_chain_winidx_mutant: W01 fixes Warp at SIX output ordinals, not %0d",
             W_ORDINALS);
    if (W_IN_LANES != 15)
      $fatal(1, "tb_warp_field_chain_winidx_mutant: W01 fixes Warp at FIFTEEN input lanes, not %0d",
             W_IN_LANES);
  end

endmodule

`default_nettype wire
