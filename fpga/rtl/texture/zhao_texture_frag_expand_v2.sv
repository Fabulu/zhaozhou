// zhao_texture_frag_expand_v2.sv -- Packet-B typed fragment expansion.
//
// Input is the exact owner-aligned UV-join record split into its typed fields:
// {owner14, logical287, U32, V32}.  The descriptor's page generation remains
// solely in logical287[286:279] and is copied once into each sample job.  The
// owner's frozen required mask is the obligation authority; descriptor copies
// are checked, never allowed to rewrite that mask.
`default_nettype none

module zhao_texture_frag_expand_v2 #(
    parameter int unsigned FQD   = 4,
    parameter int unsigned SLOTW = 6,
    parameter int unsigned GENW  = 8
) (
    input  logic clk,
    input  logic rst_n,
    input  logic frame_fault_clear_i,

    // Exact joined fragment plus independently owner-held obligation state.
    input  logic                 frag_valid_i,
    output logic                 frag_ready_o,
    input  logic [SLOTW+GENW-1:0] frag_owner_i,
    input  logic [286:0]         frag_logical_descriptor_i,
    input  logic signed [31:0]   frag_u_i,
    input  logic signed [31:0]   frag_v_i,
    input  logic [3:0]           frag_required_mask_i,
    input  logic                 frag_material_refused_i,

    // One typed logical sample job.  Its acceptance is the resolver's logical
    // TMU issue event; this block does not emit a second issue notification.
    output logic                 sample_valid_o,
    input  logic                 sample_ready_i,
    output logic [SLOTW+2+GENW-1:0] sample_handle_o,
    output logic [7:0]           sample_page_generation_o,
    output logic                 sample_selector_overflow_o,
    output logic                 sample_force_refuse_o,
    output logic [7:0]           sample_binding_selector_o,
    output logic signed [31:0]   sample_u_o,
    output logic signed [31:0]   sample_v_o,
    output logic [7:0]           sample_lod_q4_4_o,
    output logic [1:0]           sample0_class_witness_o,
    output logic [1:0]           sample0_palette_slot_witness_o,
    output logic [7:0]           sample0_palette_generation_witness_o,

    // One explicit Mosaic side record per accepted fragment. It is owner- and
    // UV-aligned with the descriptor bytes and held independently of TMU/AUX.
    output logic                 mosaic_valid_o,
    input  logic                 mosaic_ready_i,
    output logic [SLOTW+GENW-1:0] mosaic_owner_o,
    output logic signed [31:0]   mosaic_u_o,
    output logic signed [31:0]   mosaic_v_o,
    output logic [7:0]           mosaic_material_a_o,
    output logic [7:0]           mosaic_material_b_o,
    output logic [7:0]           mosaic_weight_o,

    // AUX is a separate typed owner obligation; it never occupies sample 2.
    output logic                 aux_valid_o,
    input  logic                 aux_ready_i,
    output logic [SLOTW+GENW-1:0] aux_owner_o,
    output logic [zhao_render_texture_pkg::AUX_SURFACE_CTX_W-1:0]
                                      aux_context_o,
    output logic                 aux_force_refuse_o,
    output logic                 iss_aux_valid_o,
    output logic [SLOTW+GENW-1:0] iss_aux_owner_o,

    // Acceptance/completion evidence and structural observation.
    output logic [31:0]          fragments_accepted_o,
    output logic [31:0]          sample_jobs_accepted_o,
    output logic [31:0]          mosaic_jobs_accepted_o,
    output logic [31:0]          aux_jobs_accepted_o,
    output logic [31:0]          zero_sample_fragments_o,
    output logic [31:0]          malformed_descriptors_o,
    output logic [31:0]          wq_overflow_o,
    output logic                 frame_fault_o,
    output logic                 idle_o
);
  import zhao_render_texture_pkg::*;

  localparam int unsigned OWNERW = SLOTW + GENW;
  localparam int unsigned SMPW   = SLOTW + 2 + GENW;
  localparam int unsigned FQPW   = (FQD <= 1) ? 1 : $clog2(FQD);
  localparam int unsigned FQCW   = (FQD <= 1) ? 1 : $clog2(FQD + 1);

  typedef struct packed {
    logic [7:0]                 active_page_generation; // [286:279]
    logic [7:0]                 binding_selector;       // [278:271]
    logic [7:0]                 mosaic_weight;          // [270:263]
    logic [7:0]                 mosaic_material_b;      // [262:255]
    logic [7:0]                 mosaic_material_a;      // [254:247]
    logic [7:0]                 palette_generation;     // [246:239]
    logic [1:0]                 palette_slot;           // [238:237]
    logic [1:0]                 sample_count;           // [236:235]
    logic                       aux_required;           // [234]
    logic [1:0]                 response_class;         // [233:232]
    logic [7:0]                 lod_q4_4;                // [231:224]
    zhao_aux_surface_ctx_v2_t   aux_context;             // [223:0]
  } descriptor_t;

  typedef struct packed {
    logic [OWNERW-1:0]          owner;
    descriptor_t                descriptor;
    logic signed [31:0]         u;
    logic signed [31:0]         v;
    logic                       force_refuse;
  } fragment_t;

  typedef struct packed {
    logic [SMPW-1:0]            handle;
    logic [7:0]                 page_generation;
    logic                       selector_overflow;
    logic                       force_refuse;
    logic [7:0]                 binding_selector;
    logic signed [31:0]         u;
    logic signed [31:0]         v;
    logic [7:0]                 lod_q4_4;
    logic [1:0]                 witness_class;
    logic [1:0]                 witness_palette_slot;
    logic [7:0]                 witness_palette_generation;
  } sample_job_t;

  // NO READ-DURING-WRITE BYPASS. This attribute is worth +3.42 MHz on the whole
  // machine and it is the only reason this comment is long.
  //
  // `fragment_m` is written in an `always_ff` and read by a bare `assign` two
  // lines below, which forces the synthesiser to build bypass logic so the read
  // reflects a same-cycle write. `@packet-h-texorder`'s WORST PATH in the entire
  // design launched from that logic --
  // `fragment_m_rtl_0|...|ram_block1a140~PORT_B_WRITE_ENABLE_REG`, a RAM's write
  // ENABLE register driving a data path -- through an adder and three mux levels
  // into `zhao_texture_binding_resolver_v2`'s `read_row_present_q`, at -2.540 ns.
  // Removing that one path takes `gpu_clk` from 79.74 to 83.16 MHz; it is the
  // largest single lever left in the render path and the smallest change.
  //
  // WHY IT IS SAFE, and the argument is about a fault that is already fatal.
  //
  // The RAM sees the same address when `read_pointer_q == write_pointer_q`,
  // which for this queue is EMPTY or FULL. (An extra pointer wrap bit would NOT
  // change this -- the memory addresses with the low bits, which are equal at
  // full either way. That was tried on paper and withdrawn.)
  //
  //   EMPTY: `sample_valid_o` is `!queue_empty_c && ...`, and `occupancy_q` does
  //          not move until the next edge, so the handshake's valid is LOW in
  //          exactly the cycle a bypass would matter. The value is a don't-care.
  //
  //   FULL:  the write is blocked by the full guard. If that guard ever failed,
  //          the write would land on `read_pointer_q` and DESTROY THE LIVE HEAD
  //          -- a correctness failure today, with or without this attribute. So
  //          this does not add a dependency; it changes the symptom of a fault
  //          that is fatal either way. And the guard is not un-evidenced:
  //          `wq_overflow_o` has a committed positive control in
  //          `tests/mutants/zhao_texture_frag_expand_mutant.sv`, written because
  //          the overflow state is unreachable while the guard holds.
  //
  // The empty-case don't-care is ASSERTED below rather than argued, so it is a
  // property the suite checks on every offered job instead of a paragraph.
  (* ramstyle = "no_rw_check" *)
  fragment_t fragment_m [FQD];
  logic [2:0] sample_pending_m [FQD];
  logic       mosaic_pending_m [FQD];
  logic       aux_pending_m [FQD];
  logic [FQPW-1:0] read_pointer_q, write_pointer_q;
  logic [FQCW:0] occupancy_q;

  descriptor_t offered_descriptor_c;
  descriptor_t head_descriptor_c;
  fragment_t   head_c;
  sample_job_t sample_job_c;

  assign offered_descriptor_c = descriptor_t'(frag_logical_descriptor_i);
  assign head_c = fragment_m[read_pointer_q];
  assign head_descriptor_c = head_c.descriptor;

  function automatic logic [3:0] descriptor_mask(input descriptor_t d);
    logic [2:0] samples;
    begin
      unique case (d.sample_count)
        2'd0:    samples = 3'b000;
        2'd1:    samples = 3'b001;
        2'd2:    samples = 3'b011;
        default: samples = 3'b111;
      endcase
      descriptor_mask = {d.aux_required, samples};
    end
  endfunction

  function automatic logic [FQPW-1:0] increment_pointer(
      input logic [FQPW-1:0] pointer);
    if (pointer == FQPW'(FQD-1)) increment_pointer = '0;
    else                         increment_pointer = pointer + FQPW'(1);
  endfunction

  function automatic logic [1:0] first_sample(input logic [2:0] pending);
    if (pending[0])      first_sample = 2'd0;
    else if (pending[1]) first_sample = 2'd1;
    else                 first_sample = 2'd2;
  endfunction

  wire offered_count_zero_c =
      frag_required_mask_i[2:0] == 3'b000;
  wire offered_witness_canonical_c =
      !offered_count_zero_c ||
      ({offered_descriptor_c.response_class,
        offered_descriptor_c.palette_slot,
        offered_descriptor_c.palette_generation} == 12'd0);
  wire offered_aux_canonical_c =
      offered_descriptor_c.aux_required ||
      (offered_descriptor_c.aux_context == zhao_aux_surface_ctx_v2_t'('0));
  wire offered_descriptor_bad_c =
      (descriptor_mask(offered_descriptor_c) != frag_required_mask_i) ||
      !offered_witness_canonical_c || !offered_aux_canonical_c;
  wire offered_force_refuse_c = frag_material_refused_i ||
                                  offered_descriptor_bad_c;

  wire queue_empty_c = occupancy_q == '0;
  wire queue_full_c  = occupancy_q >= (FQCW+1)'(FQD);
  assign frag_ready_o = !queue_full_c;
  wire frag_accept_c = frag_valid_i && frag_ready_o;

  wire [2:0] head_sample_pending_c = queue_empty_c
                                    ? 3'b000
                                    : sample_pending_m[read_pointer_q];
  wire head_mosaic_pending_c =
      !queue_empty_c && mosaic_pending_m[read_pointer_q];
  wire head_aux_pending_c = !queue_empty_c && aux_pending_m[read_pointer_q];
  wire [1:0] sample_index_c = first_sample(head_sample_pending_c);
  logic [8:0] selector9_c;
  always_comb begin
    selector9_c = {1'b0, head_descriptor_c.binding_selector}
                + {7'd0, sample_index_c};

    sample_job_c.handle = {
        head_c.owner[OWNERW-1:GENW], sample_index_c,
        head_c.owner[GENW-1:0]};
    sample_job_c.page_generation =
        head_descriptor_c.active_page_generation;
    sample_job_c.selector_overflow = selector9_c[8];
    sample_job_c.force_refuse = head_c.force_refuse;
    sample_job_c.binding_selector = selector9_c[7:0];
    sample_job_c.u = head_c.u;
    sample_job_c.v = head_c.v;
    sample_job_c.lod_q4_4 = head_descriptor_c.lod_q4_4;
    sample_job_c.witness_class = head_descriptor_c.response_class;
    sample_job_c.witness_palette_slot = head_descriptor_c.palette_slot;
    sample_job_c.witness_palette_generation =
        head_descriptor_c.palette_generation;
  end

  assign sample_valid_o = !queue_empty_c && (|head_sample_pending_c);
  assign sample_handle_o = sample_job_c.handle;
  assign sample_page_generation_o = sample_job_c.page_generation;
  assign sample_selector_overflow_o = sample_job_c.selector_overflow;
  assign sample_force_refuse_o = sample_job_c.force_refuse;
  assign sample_binding_selector_o = sample_job_c.binding_selector;
  assign sample_u_o = sample_job_c.u;
  assign sample_v_o = sample_job_c.v;
  assign sample_lod_q4_4_o = sample_job_c.lod_q4_4;
  assign sample0_class_witness_o = sample_job_c.witness_class;
  assign sample0_palette_slot_witness_o = sample_job_c.witness_palette_slot;
  assign sample0_palette_generation_witness_o =
      sample_job_c.witness_palette_generation;

  assign mosaic_valid_o = head_mosaic_pending_c;
  assign mosaic_owner_o = head_c.owner;
  assign mosaic_u_o = head_c.u;
  assign mosaic_v_o = head_c.v;
  assign mosaic_material_a_o = head_descriptor_c.mosaic_material_a;
  assign mosaic_material_b_o = head_descriptor_c.mosaic_material_b;
  assign mosaic_weight_o = head_descriptor_c.mosaic_weight;

  assign aux_valid_o = head_aux_pending_c;
  assign aux_owner_o = head_c.owner;
  assign aux_context_o = head_c.force_refuse ? '0 : head_descriptor_c.aux_context;
  assign aux_force_refuse_o = head_c.force_refuse;

  wire sample_accept_c = sample_valid_o && sample_ready_i;
  wire mosaic_accept_c = mosaic_valid_o && mosaic_ready_i;
  wire aux_accept_c = aux_valid_o && aux_ready_i;
  assign iss_aux_valid_o = aux_accept_c;
  assign iss_aux_owner_o = head_c.owner;

  logic [2:0] samples_after_c;
  logic       mosaic_after_c;
  logic       aux_after_c;
  always_comb begin
    samples_after_c = head_sample_pending_c;
    if (sample_accept_c)
      samples_after_c[sample_index_c] = 1'b0;
    mosaic_after_c = head_mosaic_pending_c && !mosaic_accept_c;
    aux_after_c = head_aux_pending_c && !aux_accept_c;
  end
  wire head_done_c = !queue_empty_c &&
                     (samples_after_c == 3'b000) &&
                     !mosaic_after_c && !aux_after_c;

  assign idle_o = queue_empty_c;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      read_pointer_q          <= '0;
      write_pointer_q         <= '0;
      occupancy_q             <= '0;
      fragments_accepted_o    <= 32'd0;
      sample_jobs_accepted_o  <= 32'd0;
      mosaic_jobs_accepted_o  <= 32'd0;
      aux_jobs_accepted_o     <= 32'd0;
      zero_sample_fragments_o <= 32'd0;
      malformed_descriptors_o <= 32'd0;
      wq_overflow_o           <= 32'd0;
      frame_fault_o           <= 1'b0;
    end else begin
      if (frame_fault_clear_i)
        frame_fault_o <= 1'b0;
      // This detector does not share the full predicate with frag_ready_o.  A
      // committed mutant weakens that guard and demonstrates this can fire.
      if (occupancy_q > (FQCW+1)'(FQD))
        wq_overflow_o <= wq_overflow_o + 32'd1;

      if (frag_accept_c) begin
        fragment_m[write_pointer_q] <= '{
            owner:         frag_owner_i,
            descriptor:    offered_descriptor_c,
            u:             frag_u_i,
            v:             frag_v_i,
            force_refuse:  offered_force_refuse_c};
        sample_pending_m[write_pointer_q] <= frag_required_mask_i[2:0];
        mosaic_pending_m[write_pointer_q] <= 1'b1;
        aux_pending_m[write_pointer_q] <= frag_required_mask_i[3];
        write_pointer_q <= increment_pointer(write_pointer_q);
        fragments_accepted_o <= fragments_accepted_o + 32'd1;
        if (frag_required_mask_i[2:0] == 3'b000)
          zero_sample_fragments_o <= zero_sample_fragments_o + 32'd1;
        if (offered_descriptor_bad_c) begin
          malformed_descriptors_o <= malformed_descriptors_o + 32'd1;
          // Set after clear so a same-edge malformed acceptance wins.
          frame_fault_o <= 1'b1;
        end
      end

      if (sample_accept_c) begin
        sample_pending_m[read_pointer_q][sample_index_c] <= 1'b0;
        sample_jobs_accepted_o <= sample_jobs_accepted_o + 32'd1;
      end
      if (mosaic_accept_c) begin
        mosaic_pending_m[read_pointer_q] <= 1'b0;
        mosaic_jobs_accepted_o <= mosaic_jobs_accepted_o + 32'd1;
      end
      if (aux_accept_c) begin
        aux_pending_m[read_pointer_q] <= 1'b0;
        aux_jobs_accepted_o <= aux_jobs_accepted_o + 32'd1;
      end
      if (head_done_c)
        read_pointer_q <= increment_pointer(read_pointer_q);

      unique case ({frag_accept_c, head_done_c})
        2'b10: occupancy_q <= occupancy_q + (FQCW+1)'(1);
        2'b01: occupancy_q <= occupancy_q - (FQCW+1)'(1);
        default: occupancy_q <= occupancy_q;
      endcase
    end
  end

  initial begin : p_layout_contract
    if ((SLOTW != 6) || (GENW != 8))
      $fatal(1, "frag_expand_v2 requires Packet-B owner widths 6+8");
    if (FQD < 2)
      $fatal(1, "frag_expand_v2 FQD must be at least two");
    if (($bits(descriptor_t) != 287) || ($bits(sample_job_t) != 118))
      $fatal(1, "frag_expand_v2 typed record width changed");
  end

`ifndef SYNTHESIS
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else        armed_q <= 1'b1;
  end
  always_ff @(posedge clk) begin
    if (armed_q) begin
      a_queue_in_range: assert (occupancy_q <= (FQCW+1)'(FQD));
      a_aux_issue_atomic: assert (!iss_aux_valid_o ||
                                 (aux_valid_o && aux_ready_i));
      a_no_sample3: if (sample_valid_o)
        assert (sample_handle_o[GENW +: 2] != 2'd3);

      // THE PROPERTY `ramstyle = "no_rw_check"` ON `fragment_m` RESTS ON.
      //
      // The attribute tells the fitter that a read and a write never collide at
      // one address in one cycle in a way anyone observes. They CAN collide --
      // `read_pointer_q == write_pointer_q` at empty and at full -- so what must
      // hold is that nothing consumes the read when they do.
      //
      // At FULL the write cannot happen (`frag_ready_o` is `!queue_full_c`), so
      // `frag_accept_c` is low. At EMPTY the write can happen but
      // `sample_valid_o` is low. This asserts the conjunction directly rather
      // than either half, so it covers both cases and any third nobody thought
      // of -- if a write ever lands on the address being read while a consumer
      // is looking, the suite says so instead of one fragment going quietly
      // wrong on hardware.
      a_no_observed_read_during_write:
        assert (!(frag_accept_c &&
                  (write_pointer_q == read_pointer_q) &&
                  sample_valid_o))
          else $fatal(1,
              "fragment_m read-during-write is OBSERVED: write ptr %0d == read ptr %0d with sample_valid_o high, occupancy %0d -- ramstyle no_rw_check is no longer safe",
              write_pointer_q, read_pointer_q, occupancy_q);
    end
  end
`endif

endmodule : zhao_texture_frag_expand_v2

`default_nettype wire
