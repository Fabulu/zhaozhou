// zhao_material_window.sv -- MATERIAL.RESOLVE's REQUEST ISSUE POINT and its
// RESPONSE JOIN.  `zhao_console_core` entry I49, 2026-09-20 (texmat2 packet).
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS FOR
// ---------------------------------------------------------------------------
// MATERIAL.RESOLVE was built, tested and COMPOSED on 2026-09-19, with its
// directory (MEM.UPLOAD's 5f.1 publication) and its fetch (requester C of the
// ENGINE1 adapter) both real.  What it did not have was a producer for its
// REQUEST and a consumer for its RESPONSE: entry I49 said, in as many words,
// "a request must be issued per meshlet (or per triangle) and its answer
// joined back to the triangles that asked".  This is that block.
//
// It sits in the triangle stream between GEOM.REPLAY and GEOM.CLIP and does
// three things:
//
//   1. it reads the triangle's OWN {material_set, material_id} -- both carried
//      with the meshlet from GEOM.DRAWJOB through GEOM.ASSETFETCH,
//      GEOM.ASSEMBLE and GEOM.REPLAY, so the two halves are joined BY
//      CONSTRUCTION and not by their timing (the fault entry I39 refuses by
//      name);
//   2. when that pair differs from the one currently PUBLISHED it issues a
//      resolve and holds the triangle until the answer is in hand;
//   3. it publishes the answer's MATERIAL-OWNED fields as the flat request's
//      material half, for every triangle it then lets through.
//
// ---------------------------------------------------------------------------
// WHY THE ANSWER AT THE DOOR BELONGS TO THE TRIANGLE AT THE DOOR
// ---------------------------------------------------------------------------
// This is the whole correctness argument and it is STRUCTURAL, not a latency
// coincidence.  The published material is a single register read combinationally
// by the shell's triangle door, several pipeline stages downstream of here --
// exactly the shape that produced the metadata-swap defect CLAUDE.md has a
// chapter about.  What makes it sound here is the INTERLOCK:
//
//   * the window never changes what it publishes while ANY triangle is between
//     GEOM.CLIP's input and the door.  `d_enter_i`, `d_reject_i` and
//     `d_leave_i` are the three disposal events of that span -- a triangle is
//     accepted by GEOM.CLIP, and is then either retired by GEOM.CLIP with a
//     non-ACCEPT verdict or taken by the door.  There is no fourth outcome, so
//     `occupancy_q` returns to zero and the drain always completes;
//   * and the window holds the triangle that asked for the new material until
//     the drain AND the answer are both done.
//
// So the span downstream of this block is, at every instant, occupied by
// triangles of ONE material, and that material is the published one.  The
// argument does not depend on GEOM.CLIP's latency, on GEOM.SETUP's, on whether
// GEOM.CLIP drops a triangle, or on the order in which it does -- only on the
// fact that every triangle that enters leaves.  `err_unpublished_o` watches the
// one thing that would falsify it.
//
// THE COST IS A DRAIN PER MATERIAL CHANGE, and it is measured rather than
// argued: `drain_stall_cycles_o` and `answer_stall_cycles_o` are counted
// separately, because they have different cures.  A repeat of the SAME
// {set, id} -- which the oracle's own header says is the common case ("the
// same material serves every triangle of a meshlet and usually many
// meshlets") -- costs nothing at all: no drain, no request, no stall.
//
// ---------------------------------------------------------------------------
// WHAT IT PUBLISHES, AND THE THREE FIELDS IT DOES *NOT* OWN
// ---------------------------------------------------------------------------
// `zhao_material_resolve`'s own header is the authority and this block obeys
// it.  The flat request's MATERIAL-OWNED fields are `sample_count`,
// `material_recipe`, `recipe_weight` and `base_binding_selector`, and those
// four come straight off the response.
//
// `palette_slot`, `palette_generation` and `response_class` are the BINDING
// PAGE's, and `zhao_texture_binding_resolver_v2` takes the request's copies as
// WITNESSES and CHECKS them.  Two of the three have a LAW rather than a
// producer and one is an OWNER DECISION:
//
//   * palette_slot / palette_generation.  For a DIRECT format (RGB565,
//     ARGB1555, ARGB4444) `binding_row_legal` REQUIRES
//     `{palette_generation, palette_slot} == 0` -- so publishing zero is the
//     binding page's own law, not an invented value, and the witness is exact.
//     For a CLUT format the pair is real and nothing in this console produces
//     it: no ratified material field carries the palette's identity.  That is
//     not hidden -- `clut_unowned_o` COUNTS every material published with a
//     CLUT class, so the gap is loud at the exact moment it would matter.
//   * response_class.  `spec/commands.zidl` ratifies
//     `MaterialSample.modes[3:0]` as "tmu_mode u4 nearest/bilinear/CLUT/direct"
//     and NEVER ASSIGNS THE NUMBERS.  Searched: `spec/`, `reference/`,
//     `design/contracts/`, `tests/` and `tools/` for `tmu_mode` -- four hits,
//     none of them an encoding (the directed tests use 1, 2 and 3 as opaque
//     bytes and check only that the field unpacks).  So the mapping is an
//     OWNER DECISION, it is recorded as one, and it lives HERE in a single
//     editable parameter so the owner's pick is a one-line change
//     (CLAUDE.md rule 6).  The default is the one that makes the witness
//     MEANINGFUL: tmu_mode[1:0] is read as the class in the binding resolver's
//     own numbering (0 CLUT, 1 NEAR, 2 BIL), so the check differences the
//     MATERIAL RECORD in VRAM against the BINDING PAGE from the config stream
//     -- two independent sources, which is what lets `witness_mismatch_o` fire
//     at all.  A mapping derived from the binding page would have been the
//     detector-wired-to-two-operands-that-move-together defect.
//
// `lod_q4_4` is excluded by MATERIAL.RESOLVE's contract in terms ("it returns
// the mip policy; the sampler picks the level"), and `base_rgb` / `base_alpha`
// are the VERTEX's, not the material's.  Neither is published here, and the
// composer says what it drives them with and why.
//
// ---------------------------------------------------------------------------
// A DENIED OR MISSING RECORD RESOLVES, AND IS COUNTED.  IT NEVER HANGS.
// ---------------------------------------------------------------------------
// Owner ruling R20's law, carried one seam further.  `zhao_material_resolve`
// always answers -- a miss, a not-resident set, an illegal record and a denied
// fetch all produce `rsp_valid_o` with `rsp_has_record_o` low.  This block
// publishes the DEFINED FAULT MATERIAL for that case (sample_count 0, which is
// the legal "this surface takes no texture sample" profile) and counts it on
// `no_record_o`.  The stream never stops; the fault is visible; nothing is
// invented to cover it.
//
// Conservative SystemVerilog subset only; Quartus 17.0 syntax rules apply
// (elaboration checks inside `initial begin ... end`, explicit generate, no
// inline `for (genvar ...)`).
`default_nettype none

module zhao_material_window #(
    // THE OWNER'S KNOB for the tmu_mode -> response_class mapping described
    // above, four two-bit entries packed low-to-high: mode 0 in [1:0], mode 1
    // in [3:2], mode 2 in [5:4], mode 3 in [7:6].  The default is the identity
    // on the binding resolver's own class numbering (0 CLUT, 1 NEAR, 2 BIL);
    // entry 3 is the reserved mode and maps to the reserved class 3, which the
    // binding resolver refuses rather than samples.
    parameter logic [7:0] TMU_MODE_CLASS = 8'b11_10_01_00,
    // Deepest downstream occupancy the drain counter must represent.  The span
    // it watches is GEOM.CLIP (3 stages), GEOM.SETUP and GEOM.ATTRPACK, so a
    // dozen is generous; the counter REFUSES to wrap and says so.
    parameter int unsigned OCCW = 8
) (
    input  wire                     clk,
    input  wire                     rst_n,

    // ---- the triangle stream, in: GEOM.REPLAY -----------------------------
    input  wire                     t_valid_i,
    output wire                     t_ready_o,
    input  wire        [31:0]       t_material_set_i,
    input  wire        [15:0]       t_material_id_i,
    // The draw's semantic weight, carried as the resolve's quality tier.  The
    // resolver ECHOES it (`tier_q`) and reads it nowhere, so this is a label
    // travelling with its request, not a policy this block invents.
    input  wire        [ 7:0]       t_quality_tier_i,

    // ---- the triangle stream, out: GEOM.CLIP ------------------------------
    output wire                     t_valid_o,
    input  wire                     t_ready_i,

    // ---- the downstream span's three disposal events ----------------------
    input  wire                     d_enter_i,
    input  wire                     d_reject_i,
    input  wire                     d_leave_i,

    // ---- MATERIAL.RESOLVE's request ---------------------------------------
    output logic                    req_valid_o,
    input  wire                     req_ready_i,
    output logic       [31:0]       req_material_set_o,
    output logic       [15:0]       req_material_id_o,
    output logic       [ 7:0]       req_quality_tier_o,

    // ---- MATERIAL.RESOLVE's response --------------------------------------
    input  wire                     rsp_valid_i,
    output logic                    rsp_ready_o,
    input  wire        [ 2:0]       rsp_status_i,
    input  wire                     rsp_has_record_i,
    input  wire        [ 1:0]       rsp_sample_count_i,
    input  wire        [ 2:0]       rsp_material_recipe_i,
    input  wire        [ 7:0]       rsp_recipe_weight_i,
    input  wire        [ 7:0]       rsp_base_binding_i,
    input  wire                     rsp_selector_overflow_i,
    input  wire        [ 7:0]       rsp_sample0_modes_i,

    // ---- the published material, into the flat request --------------------
    output logic                    pub_valid_o,
    output logic       [ 1:0]       pub_sample_count_o,
    output logic       [ 2:0]       pub_material_recipe_o,
    output logic       [ 7:0]       pub_recipe_weight_o,
    output logic       [ 7:0]       pub_base_binding_o,
    output logic       [ 1:0]       pub_response_class_o,

    // ---- evidence ---------------------------------------------------------
    output logic       [31:0]       resolves_o,
    output logic       [31:0]       switches_o,
    output logic       [31:0]       drain_stall_cycles_o,
    output logic       [31:0]       answer_stall_cycles_o,
    output logic       [31:0]       occupancy_max_o,
    output logic       [31:0]       no_record_o,
    output logic       [31:0]       selector_overflow_o,
    output logic       [31:0]       clut_unowned_o,
    // THE TWO STRUCTURAL GUARDS.  Both are zero in any correct composition and
    // both are reachable with LEGAL STIMULUS AT THIS BLOCK'S OWN PORTS -- the
    // disposal events are inputs, so a directed test fires them by pulsing a
    // departure that never had an arrival.  That is the `t_ack_i` shape, not
    // the `wq_overflow_o` shape, so neither owes a committed mutant.
    output logic       [31:0]       err_unpublished_o,
    output logic       [31:0]       err_occupancy_underflow_o
);

  // The binding resolver's own class numbering, repeated here only so the
  // parameter's default is readable.  It is NOT a second definition: nothing
  // below compares against these names, they name the parameter's nibbles.
  localparam logic [1:0] CLS_CLUT_C = 2'd0;

  localparam logic [2:0] ST_RUN    = 3'd0;   // pass triangles of the published material
  localparam logic [2:0] ST_DRAIN  = 3'd1;   // hold, waiting for the span to empty
  localparam logic [2:0] ST_REQ    = 3'd2;   // hold, offering the resolve
  localparam logic [2:0] ST_WAIT   = 3'd3;   // hold, waiting for the answer

  logic [2:0]  st_q;
  logic        pub_valid_q;
  logic [31:0] pub_set_q;
  logic [15:0] pub_id_q;
  logic [1:0]  pub_count_q;
  logic [2:0]  pub_recipe_q;
  logic [7:0]  pub_weight_q;
  logic [7:0]  pub_binding_q;
  logic [1:0]  pub_class_q;

  // The pending request, captured from the triangle that asked for it.  It is
  // captured ONCE, on the transition out of ST_RUN, while that triangle is
  // still being offered and its data is therefore stable -- the stream is
  // held from the same clock, so nothing can move underneath it.
  logic [31:0] ask_set_q;
  logic [15:0] ask_id_q;
  logic [7:0]  ask_tier_q;

  logic [OCCW-1:0] occupancy_q;

  // ---- the stream ---------------------------------------------------------
  // `match_c` is a function of the OFFERED data and of registered state, never
  // of `t_valid_i`, so the ready handed upstream is not a function of the valid
  // handed downstream and the pair cannot lock.
  wire match_c = pub_valid_q &&
                 (t_material_set_i == pub_set_q) &&
                 (t_material_id_i  == pub_id_q);
  wire pass_c  = (st_q == ST_RUN) && match_c;

  assign t_valid_o = t_valid_i && pass_c;
  assign t_ready_o = t_ready_i && pass_c;

  wire drained_c = (occupancy_q == {OCCW{1'b0}});

  // ---- the published material --------------------------------------------
  assign pub_valid_o           = pub_valid_q;
  assign pub_sample_count_o    = pub_count_q;
  assign pub_material_recipe_o = pub_recipe_q;
  assign pub_recipe_weight_o   = pub_weight_q;
  assign pub_base_binding_o    = pub_binding_q;
  assign pub_response_class_o  = pub_class_q;

  // ---- the request --------------------------------------------------------
  assign req_valid_o        = (st_q == ST_REQ);
  assign req_material_set_o = ask_set_q;
  assign req_material_id_o  = ask_id_q;
  assign req_quality_tier_o = ask_tier_q;
  assign rsp_ready_o        = (st_q == ST_WAIT);

  // The answer's class, through the owner's editable mapping.  `modes[3:2]`
  // are the reserved half of the ratified u4 and take no part in it.
  wire [1:0] rsp_mode_c  = rsp_sample0_modes_i[1:0];
  wire [1:0] rsp_class_c = (rsp_mode_c == 2'd0) ? TMU_MODE_CLASS[1:0] :
                           (rsp_mode_c == 2'd1) ? TMU_MODE_CLASS[3:2] :
                           (rsp_mode_c == 2'd2) ? TMU_MODE_CLASS[5:4] :
                                                  TMU_MODE_CLASS[7:6];

  wire rsp_take_c = rsp_valid_i && (st_q == ST_WAIT);

  // ---- the occupancy of the span this block protects ----------------------
  // Arrivals and departures on the same clock cancel, which is why the two are
  // summed rather than sequenced.
  wire        occ_up_c   = d_enter_i;
  wire [1:0]  occ_down_c = {1'b0, d_reject_i} + {1'b0, d_leave_i};

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      occupancy_q               <= {OCCW{1'b0}};
      occupancy_max_o           <= 32'd0;
      err_occupancy_underflow_o <= 32'd0;
      err_unpublished_o         <= 32'd0;
    end else begin
      // A departure with nothing outstanding is a broken accounting, not a
      // wrap: it is COUNTED and the counter is clamped at zero, so one fault
      // does not turn the drain condition into a lie for the rest of the frame.
      if (({1'b0, occupancy_q} + {{OCCW{1'b0}}, occ_up_c}) <
          {{(OCCW-1){1'b0}}, occ_down_c}) begin
        err_occupancy_underflow_o <= err_occupancy_underflow_o + 32'd1;
        occupancy_q               <= {OCCW{1'b0}};
      end else begin
        occupancy_q <= occupancy_q + {{(OCCW-1){1'b0}}, occ_up_c}
                                   - {{(OCCW-2){1'b0}}, occ_down_c};
      end

      if ({24'd0, occupancy_q} > occupancy_max_o)
        occupancy_max_o <= {24'd0, occupancy_q};

      // THE GUARD THE WHOLE INTERLOCK RESTS ON.  A triangle may only reach the
      // door through this block, and this block only passes one when something
      // is published -- so a departure with `pub_valid_q` low means a triangle
      // is being shaded with a material nobody resolved.
      if (d_leave_i && !pub_valid_q)
        err_unpublished_o <= err_unpublished_o + 32'd1;
    end
  end

  // ---- the state machine and the published registers ----------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q                  <= ST_RUN;
      pub_valid_q           <= 1'b0;
      pub_set_q             <= 32'd0;
      pub_id_q              <= 16'd0;
      pub_count_q           <= 2'd0;
      pub_recipe_q          <= 3'd0;
      pub_weight_q          <= 8'd0;
      pub_binding_q         <= 8'd0;
      pub_class_q           <= 2'd0;
      ask_set_q             <= 32'd0;
      ask_id_q              <= 16'd0;
      ask_tier_q            <= 8'd0;
      resolves_o            <= 32'd0;
      switches_o            <= 32'd0;
      drain_stall_cycles_o  <= 32'd0;
      answer_stall_cycles_o <= 32'd0;
      no_record_o           <= 32'd0;
      selector_overflow_o   <= 32'd0;
      clut_unowned_o        <= 32'd0;
    end else begin
      case (st_q)
        ST_RUN: begin
          if (t_valid_i && !match_c) begin
            ask_set_q  <= t_material_set_i;
            ask_id_q   <= t_material_id_i;
            ask_tier_q <= t_quality_tier_i;
            switches_o <= switches_o + 32'd1;
            st_q       <= ST_DRAIN;
          end
        end

        ST_DRAIN: begin
          // Counted only while the span is ACTUALLY occupied, so a switch that
          // costs nothing reads as nothing. A counter that charges one cycle
          // for the unavoidable state transition would report a permanent
          // floor and hide the moment the drain starts to matter.
          if (!drained_c) drain_stall_cycles_o <= drain_stall_cycles_o + 32'd1;
          if (drained_c) st_q <= ST_REQ;
        end

        ST_REQ: begin
          answer_stall_cycles_o <= answer_stall_cycles_o + 32'd1;
          if (req_ready_i) begin
            resolves_o <= resolves_o + 32'd1;
            st_q       <= ST_WAIT;
          end
        end

        ST_WAIT: begin
          answer_stall_cycles_o <= answer_stall_cycles_o + 32'd1;
          if (rsp_valid_i) begin
            pub_valid_q <= 1'b1;
            pub_set_q   <= ask_set_q;
            pub_id_q    <= ask_id_q;
            if (rsp_has_record_i) begin
              pub_count_q   <= rsp_sample_count_i;
              pub_recipe_q  <= rsp_material_recipe_i;
              pub_weight_q  <= rsp_recipe_weight_i;
              pub_binding_q <= rsp_base_binding_i;
              pub_class_q   <= rsp_class_c;
              if (rsp_class_c == CLS_CLUT_C)
                clut_unowned_o <= clut_unowned_o + 32'd1;
            end else begin
              // R20's defined fault material: a surface that takes no sample.
              // It is published, so the stream runs; it is counted, so the
              // fault is not a silent black triangle.
              pub_count_q   <= 2'd0;
              pub_recipe_q  <= 3'd0;
              pub_weight_q  <= 8'd0;
              pub_binding_q <= 8'd0;
              pub_class_q   <= 2'd0;
              no_record_o   <= no_record_o + 32'd1;
            end
            if (rsp_selector_overflow_i)
              selector_overflow_o <= selector_overflow_o + 32'd1;
            st_q <= ST_RUN;
          end
        end

        default: st_q <= ST_RUN;
      endcase
    end
  end

  // `rsp_status_i` and `rsp_take_c` are carried for the waveform and for the
  // bench; the block's own decisions are made on `rsp_has_record_i`, which is
  // the resolver's single answer to "is there a record", and on the counted
  // fields beside it.  Reading `status` here as well would be a second opinion
  // about the same question.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [2:0] unused_status_c = rsp_status_i;
  wire       unused_take_c   = rsp_take_c;
  // `modes[3:2]` is the reserved half of the ratified u4 tmu_mode and takes no
  // part in the class; `modes[5:4]` is WRAP and `modes[7:6]` is MIP POLICY,
  // and both are the SAMPLER's, not the flat request's -- the 298-bit request
  // has no field for either. Named rather than masked away, so the day one of
  // them gains a consumer this line is where it is found.
  wire [5:0] unused_modes_c  = rsp_sample0_modes_i[7:2];
  /* verilator lint_on UNUSEDSIGNAL */

  // synthesis translate_off
  initial begin
    if (OCCW < 4)
      $fatal(1, "zhao_material_window: OCCW=%0d cannot represent the GEOM.CLIP..door span", OCCW);
  end
  // synthesis translate_on

endmodule

`default_nettype wire
