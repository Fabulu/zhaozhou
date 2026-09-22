// zhao_forge_pagebank.sv -- the FORGE_PROGRAM page reader AND the DrawProcedural
// dispatch, in one block.
//
// Law: spec/cartridge.md 4d (the page, frozen)
//      spec/commands.zidl `command DrawProcedural 0x0302`
//      reference/include/zref/zref_forge_page.hpp (the byte offsets, frozen)
//      owner ruling R234 D2 -- "the page kind is to be frozen and FORGE.PRIM /
//      FORGE.PRIM_EVAL built"
//      owner ruling R241 D-TICK-A -- `frame_tick` is `DrawProcedural.pad[11]`,
//      per draw, not a console broadcast
//
// ===========================================================================
// WHY THE READER AND THE DISPATCH ARE ONE BLOCK AND NOT TWO
// ===========================================================================
// FINDINGS-forgeprim named these as two items -- "a CMD.EXEC arm for 0x0302 and
// a page reader" -- and scoped the reader with its request side "depend[ing] on
// the dispatch that does not exist". That dependency is the argument for one
// block: the only thing that ever asks this bank a question is the draw, the
// only thing the answer is ever used for is the draw's two evaluator jobs, and
// a seam between them would be a handshake with exactly one producer and one
// consumer carrying a record neither of them owns. `zhao_geom_drawjob` is the
// same shape for DrawForm -- resolver and job issuer in one -- and is the block
// this one is modelled on.
//
// ===========================================================================
// THE KEY IS THE HANDLE INDEX. THE PAGE'S FAMILY GOVERNS. THE ROTATION IS REAL.
// ===========================================================================
// `DrawProcedural.program` is a handle32 = {index[31:8], generation[7:0]} and
// the record key is the INDEX -- R26's decision for the ladder table, R45's for
// the stamp, and `zref::forge_page`'s for this one. The generation is NOT part
// of the key and is not compared here; a page is adopted whole or not at all.
//
// `DrawProcedural.kind` declares the family a SECOND time, in a different place
// (the game's draw) from the page's own `family` (the packer). Comparing them
// is therefore a real check and not two operands moving together -- the exact
// property CLAUDE.md's metadata-swap chapter says to look for. **THE PAGE
// GOVERNS**, per `zref_forge_page.hpp`: a disagreeing `kind` REFUSES the draw
// and is counted, never substituted.
//
// AND THE COMPARISON IS A ROTATION. `spec/commands.zidl` warns in capitals that
// `forge_kind` is NOT `zhao_forge_prim`'s `j_family_i` encoding and that "a
// straight-through assignment is silently wrong for all six values":
//
//     forge_kind = (family + 1) mod 6
//
// `KIND_OF_FAMILY` below is the ONE place that lives in RTL, written as a
// lookup rather than as arithmetic so that a reader can check it against the
// zidl's own hand-written table by eye. `zref::forge_page::kind_of_family` is
// the reference side and `forge_pagebank_directed` walks all six against it.
//
// ===========================================================================
// ONE RESIDENT RECORD, A BOUNDED LINEAR SCAN, AND WHY NOT A REGISTER FILE
// ===========================================================================
// `zhao_geom_ladderbank` holds its whole page in flops because a CREATURE_FORM
// record is 32 bytes and sixteen of them is ~2,000. A forge record is 192 bytes
// -- THREE MEM.GUARD lines -- so the same treatment is ~15,000 flops on a device
// where ALMs are the binding constraint (CLAUDE.md, the 5CSEBA6U23I7 ceiling).
// So this block holds ONE record, fetched per draw:
//
//   * at PUBLICATION: one 64-byte read of the page header. Magic, version and
//     record count are judged there and the base/count are kept. A page that
//     fails is not adopted and the PREVIOUS page stays live -- the ladderbank's
//     "abandoned with the live half untouched" property, one page deep.
//   * at DRAW: read record line 0 at base + 64 + 192*i for ascending i,
//     comparing the stored program index. Bounded by the header's own count and
//     by MAX_SCAN, so a malformed count cannot make this walk forever.
//     On a hit, lines 1 and 2 follow. Worst case is count+2 reads.
//
// THE COST IS RECORDED, NOT ARGUED (R236). A scan is O(count) reads per draw
// against a register file's O(1), and at ROWS=16 that is at most 18 sixty-four
// byte reads on the ENGINE1 share per procedural draw. That is the rarest
// traffic on that share by a wide margin -- one per DRAW, against GEOM.MESHFETCH's
// one per MESHLET -- and the round robin's bound is N-1 turns, so it cannot
// starve the others. If a future scene makes procedural draws common, the trade
// to revisit is named here: a resident index column (count x 24 bits) would turn
// the scan into one read, for ~400 flops, without touching the page format.
//
// ===========================================================================
// THE TWO EVALUATORS, AND WHY BOTH JOBS LEAVE ON ONE BEAT
// ===========================================================================
// A forge program is ONE primitive whose TOPOLOGY (`zhao_forge_prim`) and
// POSITIONS (`zhao_forge_prim_eval` for the ribbon, `zhao_forge_ring_eval` for
// the fan, tube, shell and billboard sheet) are "two halves of one meshlet
// rather than two stages of a chain" -- `zref_forge_page.hpp`'s sentence. They
// are joined only by the ring-major ordering convention, so NOTHING downstream
// can re-pair them if they separate. This block therefore issues both halves as
// ONE ACCEPTANCE: `p_valid_o` and `e_valid_o` rise together and the job is not
// retired until BOTH have been taken. A fork that let one half go early would
// put job A's topology against job B's positions, which is the metadata-swap
// fault with the triangles rather than the counters.
//
// FAM_CLIFF (5) IS NOT DISPATCHED HERE AND THAT IS NOT A NARROWING. Its
// positions come from the terrain lattice, not from a parameter block --
// `zref_forge_page.hpp` says so and FORGE.CLIFF owns it. A cliff record is
// counted on `refused_cliff_o` and the draw is refused, rather than sent to an
// evaluator that would place it from anchors it does not use.
//
// ===========================================================================
// `frame_tick`: OWNER RULING R241 D-TICK-A, AND THE PAGE HOLDS ONLY THE BASE
// ===========================================================================
// A cartridge page is immutable and a lightning bolt animates, so the live
// phase is `tick_phase_base + frame_tick` and the page carries only the base
// (`spec/cartridge.md` 4d). R241 sources `frame_tick` from `DrawProcedural`'s
// `pad[11]`, PER DRAW, under the same mandatory-zero reinterpretation
// `forge_kind` itself used -- so it arrives here on `d_frame_tick_i` beside the
// program handle and is ADDED to the base with a plain 16-bit wrap. The wrap is
// correct and not a saturation: a phase is an angle16 and a whole turn is the
// WIDTH of the field, which is the same law `zhao_forge_ring_eval` uses to make
// a ring close by index.
//
// With `d_frame_tick_i == 0` the page alone is a complete, deterministic,
// static primitive, so nothing already built becomes wrong.
//
// ===========================================================================
// THE MATERIAL PAIR, AND THE READING THIS BLOCK USED TO HAVE
// ===========================================================================
// OWNER COMPLETION RULING 2, 2026-09-22. `zhao_material_window` is keyed by
// (set handle32, record id u16) and `DrawProcedural` now carries exactly that:
// `material_set` is the COMPLETE handle and `material_id` is an independent u16
// in its own record bytes (payload 40..41).
//
// THE COMPATIBILITY DISCLOSURE THE RULING REQUIRES. Until this change, this
// block read `d_material_i[15:0]` as the record id AND presented the same word
// whole as the set -- the double reading the owner forbade by name. Because a
// handle32 is {index[31:8], generation[7:0]}, the id's low byte WAS the
// generation: bumping a material set's generation (an ordinary residency event)
// silently selected a different record and repainted the primitive.
//
// THE RENDERING DIFFERENCE, exactly. For a draw whose set handle is H and whose
// (legacy, zero-filled) material_id bytes are 0:
//
//     before:  id = H[15:0]   -> {index[15:8], generation[7:0]}
//     after:   id = 0         -> record 0 of the set, A VALID INDEX
//
// So every set handle with a nonzero generation or nonzero index bits 8..15
// selects a DIFFERENT record than it used to. The ABI's own golden is the
// demonstration: its material sample is 0x2A000002, which the old reading
// turned into record 2 -- the generation byte. Nothing in the tree consumed
// that id in a committed rendered artifact (the forge chain has never run in
// the console smoke), and no golden is quietly rewritten: the ABI goldens are
// regenerated by `npm run abi:gen` from the .zidl, which is the declared path,
// and the difference is stated here, in the packet commit and in FINDINGS.
//
// AND THE PAIR IS CARRIED ON ONE ENABLE AND HANDED OVER ON ONE HANDSHAKE.
// `d_mset_q` and `d_mid_q` are loaded by the same `d_valid_i` acceptance in
// S_IDLE and by nothing else -- that is the capture. `a_valid_o` / `a_ready_i`
// is the HANDOVER, and it exists because the capture alone was not enough:
// holding the pair as a level and letting the assembler latch it at ITS first
// vertex left a window in which this block had already taken the NEXT draw.
// See the port's own comment below, and FINDINGS. CLAUDE.md's metadata-swap
// chapter says a detector whose two operands move together cannot fire; the
// answer here is not a detector but a join with no second cadence to skew from.
//
// ===========================================================================
// COUNTERS: EVERY ONE IS FIRED BY LEGAL STIMULUS AT THESE PORTS
// ===========================================================================
// There is no counter here that needs a committed mutant, and that is a design
// choice rather than a happy accident -- every refusal this block makes is
// reachable by presenting a page or a draw that deserves it. `forge_pagebank_directed`
// fires all nine, and asserts each SILENT on the vectors that must not move it.
// R95: a counter must DISCRIMINATE, so `refused_kind_o` and `lookup_miss_o` are
// separate ports and the test asserts that a kind disagreement leaves the miss
// counter PUT.

`default_nettype none

module zhao_forge_pagebank
  import zhao_pkg::*;
#(
    // spec/cartridge.md 4's page-kind registry; FORGE_PROGRAM is 14.
    parameter logic [7:0] PAGE_KIND = 8'd14,
    // The asset window's client. The same identity MATERIAL.RESOLVE's record
    // fetch, GEOM.DRAWJOB's header read and `zhao_part_table_loader` use --
    // spec/memory_rules.md 5f, "immutable ... reads [are serialized] with the
    // existing geometry adapter behind one local ENGINE1 mux".
    parameter zhao_client_e CLIENT = ZHAO_CLIENT_ENGINE1,
    // The hard bound on the scan. NAMED AND EDITABLE (CLAUDE.md rule 6). A page
    // declaring more records than this is REFUSED WHOLE at publication rather
    // than silently scanned short -- a bank answering from the first MAX_SCAN
    // programs of a larger page reads as a content bug and is not one.
    parameter int unsigned MAX_SCAN = 64
) (
    input var logic clk,
    input var logic rst_n,

    // ---- MEM.UPLOAD's publication ------------------------------------------
    input  var logic        pub_valid_i,
    input  var logic [ 7:0] pub_tag_i,
    input  var logic [31:0] pub_base_i,
    input  var logic [31:0] pub_extent_i,

    // ---- the asset read window, through the shared requester ---------------
    output var zhao_guard_req_t g_req_o,
    input  var zhao_guard_rsp_t g_rsp_i,
    input  var logic            g_beat_valid_i,
    input  var logic [63:0]     g_beat_data_i,

    // ---- the ratified DrawProcedural, from CMD.EXEC ------------------------
    input  var logic         d_valid_i,
    output var logic         d_ready_o,
    // handle32 = {index[31:8], generation[7:0]}. THE GENERATION IS NOT PART OF
    // THE KEY and is deliberately not compared: a forge page is adopted WHOLE
    // or not at all, so there is no per-record generation for it to disagree
    // with. R26 took the same decision for the ladder table's `form_index`.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic [31:0]  d_program_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  var logic [ 7:0]  d_kind_i,        // forge_kind -- the ROTATED numbering
    // ======================================================================
    // THE MATERIAL REFERENCE IS TWO INDEPENDENT VALUES -- owner completion
    // ruling 2, 2026-09-22, and this block's PREVIOUS reading is the "earlier
    // implementation" that ruling's compatibility clause is about.
    // ======================================================================
    // `d_material_i` is the COMPLETE `handle32[material_set]`, used ONLY as the
    // set. `d_material_id_i` is the independent u16 record index, decoded from
    // `DrawProcedural.material_id`'s own bytes by `zhao_cmd_exec` and arriving
    // in the SAME queue entry as the set, so the pair cannot skew.
    //
    // WHAT THIS BLOCK USED TO DO, stated plainly because the ruling requires it
    // stated: it took `d_material_i[15:0]` as the material ID while also
    // presenting the whole word as the set. A handle32 is
    // {index[31:8], generation[7:0]}, so THE LOW BYTE OF THAT ID WAS THE
    // GENERATION -- a residency event that bumped the generation selected a
    // different material record and repainted the primitive, with nothing
    // anywhere able to notice. The owner named that mechanism and forbade the
    // reading: "Do not read the existing word both as the complete set handle
    // and as its low-16-bit material ID."
    input  var logic [31:0]  d_material_i,
    input  var logic [15:0]  d_material_id_i, // u16 record index within the set
    input  var logic [15:0]  d_frame_tick_i,  // R241 D-TICK-A, frame_tick[2]
    input  var logic [ 1:0]  d_view_mask_i,
    input  var logic [15:0]  d_src_id_i,

    // ---- the assembler's interlock -----------------------------------------
    // High while a forge primitive is in flight downstream. THIS BLOCK HOLDS
    // ITS NEXT DRAW AGAINST IT, and the reason is specific rather than
    // cautious: the material SET is a 32-bit per-primitive value and
    // `zhao_forge_prim`'s material port is 16 bits wide, so the set cannot
    // travel on the triangle stream and must be a LEVEL. A level that changed
    // while the assembler still held the previous primitive would put draw B's
    // material under draw A's triangles with every counter in the chain
    // balancing -- CLAUDE.md's metadata-swap fault exactly, and invisible to
    // any count because no count looks at the field that moved.
    input  var logic         asm_busy_i,

    // ---- the TOPOLOGY job -> zhao_forge_prim -------------------------------
    output var logic         p_valid_o,
    input  var logic         p_ready_i,
    output var logic [ 2:0]  p_family_o,
    output var logic [ 6:0]  p_segments_o,
    output var logic [ 3:0]  p_sides_o,
    output var logic [15:0]  p_material_o,
    output var logic [ 1:0]  p_view_mask_o,
    output var logic [15:0]  p_src_id_o,

    // ---- the primitive's material PAIR, HANDSHAKED ------------------------
    // `a_valid_o` rises with `p_valid_o` and `e_valid_o`/`r_valid_o` and the
    // job is not retired until ALL THREE have been taken. That is this block's
    // own "both halves of one primitive leave on ONE acceptance" law extended
    // to three, and it exists because A LEVEL WAS NOT ENOUGH.
    //
    // THE DEFECT IT REPAIRS, found by `mat_skew_o` on its first run: the pair
    // used to be a pure LEVEL that `zhao_forge_assemble` latched at ITS FIRST
    // VERTEX, and `d_ready_o` released the next draw as soon as `asm_busy_i`
    // was low. Between a job's issue and its first vertex the assembler is NOT
    // busy, so the bank accepted the NEXT draw and the level moved -- putting
    // draw B's material on draw A's primitive, with every counter in the chain
    // balancing because no count looks at the field that moved. CLAUDE.md's
    // metadata-swap chapter, in this block, reached by ordinary stimulus.
    //
    // With the handshake the assembler captures the pair AT THE ISSUE, which
    // is an event that belongs to the job, and what the bank does with its
    // registers afterwards cannot reach the primitive in flight.
    output var logic         a_valid_o,
    input  var logic         a_ready_i,
    // `zhao_material_window` is keyed by (set handle32, id u16) and
    // `DrawProcedural` now carries exactly that pair: the complete handle in
    // `material_set` and the record index in `material_id`. BOTH HALVES ARE
    // DRIVEN FROM THE SAME TWO REGISTERS, loaded by the SAME ENABLE at the
    // draw's own accepted handshake, and held for the whole primitive against
    // `asm_busy_i`. That is the ruling's carriage clause discharged
    // structurally rather than by a comment: there is no cadence here for the
    // two halves to differ on, so "a later draw must not replace an earlier
    // primitive's material" is a property of the wiring and not of a check.
    output var logic [31:0]  p_material_set_o,
    output var logic [15:0]  p_material_id_o,

    // ---- the POSITIONS job, RIBBON -> zhao_forge_prim_eval -----------------
    output var logic         e_valid_o,
    input  var logic         e_ready_i,
    output var logic signed [31:0] e_start_x_o,
    output var logic signed [31:0] e_start_y_o,
    output var logic signed [31:0] e_start_z_o,
    output var logic signed [31:0] e_end_x_o,
    output var logic signed [31:0] e_end_y_o,
    output var logic signed [31:0] e_end_z_o,
    output var logic signed [31:0] e_perp1_x_o,
    output var logic signed [31:0] e_perp1_y_o,
    output var logic signed [31:0] e_perp1_z_o,
    output var logic signed [31:0] e_perp2_x_o,
    output var logic signed [31:0] e_perp2_y_o,
    output var logic signed [31:0] e_perp2_z_o,
    output var logic signed [31:0] e_waxis_x_o,
    output var logic signed [31:0] e_waxis_y_o,
    output var logic signed [31:0] e_waxis_z_o,
    output var logic signed [31:0] e_half_width_o,
    output var logic signed [31:0] e_branch_half_width_o,
    output var logic signed [31:0] e_amp_o,
    output var logic signed [31:0] e_branch_amp_o,
    output var logic        [31:0] e_seed_o,
    output var logic        [15:0] e_tick_phase_o,
    output var logic        [ 6:0] e_segments_o,
    output var logic        [ 1:0] e_branch_count_o,
    output var logic        [ 6:0] e_br0_attach_o,
    output var logic        [ 3:0] e_br0_segments_o,
    output var logic signed [31:0] e_br0_end_x_o,
    output var logic signed [31:0] e_br0_end_y_o,
    output var logic signed [31:0] e_br0_end_z_o,
    output var logic        [ 6:0] e_br1_attach_o,
    output var logic        [ 3:0] e_br1_segments_o,
    output var logic signed [31:0] e_br1_end_x_o,
    output var logic signed [31:0] e_br1_end_y_o,
    output var logic signed [31:0] e_br1_end_z_o,
    output var logic        [ 1:0] e_view_mask_o,
    output var logic        [15:0] e_src_id_o,

    // ---- the POSITIONS job, SWEPT RING -> zhao_forge_ring_eval -------------
    output var logic         r_valid_o,
    input  var logic         r_ready_i,
    output var logic [ 2:0]  r_family_o,
    output var logic         r_sweep_o,
    output var logic [ 6:0]  r_segments_o,
    output var logic [ 3:0]  r_sides_o,
    output var logic signed [31:0] r_a0_x_o,
    output var logic signed [31:0] r_a0_y_o,
    output var logic signed [31:0] r_a0_z_o,
    output var logic signed [31:0] r_a1_x_o,
    output var logic signed [31:0] r_a1_y_o,
    output var logic signed [31:0] r_a1_z_o,
    output var logic signed [31:0] r_u_x_o,
    output var logic signed [31:0] r_u_y_o,
    output var logic signed [31:0] r_u_z_o,
    output var logic signed [31:0] r_v_x_o,
    output var logic signed [31:0] r_v_y_o,
    output var logic signed [31:0] r_v_z_o,
    output var logic signed [31:0] r_r0_o,
    output var logic signed [31:0] r_r1_o,
    output var logic [ 1:0]  r_view_mask_o,
    output var logic [15:0]  r_src_id_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] pages_o,           // page headers adopted
    output var logic [31:0] draws_o,           // draws dispatched to both halves
    output var logic [31:0] bad_magic_o,       // magic or version wrong
    output var logic [31:0] page_overflow_o,   // record count beyond MAX_SCAN
    output var logic [31:0] truncated_o,       // records run past the extent
    output var logic [31:0] lookup_miss_o,     // no record with that index
    output var logic [31:0] refused_kind_o,    // command `kind` disagrees with the page
    output var logic [31:0] refused_cliff_o,   // FAM_CLIFF: FORGE.CLIFF's, not ours
    output var logic [31:0] bad_record_o,      // a narrowed field the evaluator cannot hold
    output var logic [31:0] refused_nopage_o,  // a draw before any page was adopted
    output var logic [31:0] denied_o,          // MEM.GUARD refused a read
    output var logic        busy_o
);

  // ==========================================================================
  // THE FROZEN CONSTANTS, restated from `zref_forge_page.hpp`.
  // ==========================================================================
  localparam logic [31:0] MAGIC        = 32'h4746505A;  // 'Z','F','P','G' LE
  localparam logic [15:0] VERSION      = 16'd1;
  localparam int unsigned HEADER_BYTES = 64;
  localparam int unsigned RECORD_BYTES = 192;
  localparam int unsigned LINE_BYTES   = 64;

  // The SILICON family encoding -- `zhao_forge_prim.sv`'s FAM_*, mirrored by
  // `zref::forge_page::Family`. NOT `forge_kind`.
  localparam logic [2:0] FAM_RIBBON    = 3'd0;
  localparam logic [2:0] FAM_FAN       = 3'd1;
  localparam logic [2:0] FAM_TUBE      = 3'd2;
  localparam logic [2:0] FAM_SHELL     = 3'd3;
  localparam logic [2:0] FAM_BILLBOARD = 3'd4;
  localparam logic [2:0] FAM_CLIFF     = 3'd5;

  // THE ROTATION, AS A TABLE. `spec/commands.zidl`'s own hand-written column,
  // transcribed so it can be checked by eye against that file. Writing it as
  // `(family + 1) % 6` would be correct and unreadable; writing it as
  // `family` would be silently wrong for all six values, which is the trap the
  // zidl shouts about.
  function automatic logic [7:0] kind_of_family(input logic [2:0] fam);
    case (fam)
      FAM_CLIFF:     kind_of_family = 8'd0;  // FORGE_HEIGHTFIELD_PATCH
      FAM_RIBBON:    kind_of_family = 8'd1;  // FORGE_RIBBON
      FAM_FAN:       kind_of_family = 8'd2;  // FORGE_RADIAL_FAN
      FAM_TUBE:      kind_of_family = 8'd3;  // FORGE_TUBE
      FAM_SHELL:     kind_of_family = 8'd4;  // FORGE_RADIAL_SHELL
      FAM_BILLBOARD: kind_of_family = 8'd5;  // FORGE_BILLBOARD_SHEET
      default:       kind_of_family = 8'hFF; // no legal kind; refuses
    endcase
  endfunction

  // Quartus 17.0 will not take a bare module-scope elaboration check
  // (QUARTUS_GOTCHAS; CLAUDE.md records the exact diagnostic), so it lives
  // inside `initial begin`. And `--lint-only` does NOT run `initial` blocks, so
  // a clean lint says nothing whatever about this guard -- it is here for
  // elaboration and for the smoke, not for the linter.
  // synthesis translate_off
  initial begin
    if (MAX_SCAN == 0)
      $fatal(1, "zhao_forge_pagebank: MAX_SCAN must be at least 1");
    if (RECORD_BYTES != 3 * LINE_BYTES)
      $fatal(1, "zhao_forge_pagebank: a record is THREE MEM.GUARD lines by spec/cartridge.md 4d");
    if (kind_of_family(FAM_RIBBON) == 8'(FAM_RIBBON))
      $fatal(1, "zhao_forge_pagebank: the kind<->family map is an IDENTITY on FAM_RIBBON; it is a ROTATION (spec/commands.zidl)");
  end
  // synthesis translate_on

  // ==========================================================================
  // STATE
  // ==========================================================================
  typedef enum logic [3:0] {
    S_IDLE,
    S_HREQ, S_HVERD, S_HBEATS, S_HDR,     // the publication's header line
    S_SREQ, S_SVERD, S_SBEATS, S_SCHK,    // a record's line 0, scanned
    S_L1REQ, S_L1VERD, S_L1BEATS,         // line 1
    S_L2REQ, S_L2VERD, S_L2BEATS,         // line 2
    S_ISSUE
  } state_e;

  state_e state_q;

  logic [511:0] line_q;      // the line being assembled, LSB-first
  logic [2:0]   beat_q;
  logic [31:0]  addr_q;

  // The adopted page. `page_v_q` is the whole adoption: a draw arriving before
  // any page was adopted is REFUSED and counted, never answered from an
  // uninitialised base.
  logic         page_v_q;
  logic [31:0]  page_base_q;
  logic [15:0]  page_count_q;

  // The draw in flight.
  logic [23:0]  d_index_q;
  logic [ 7:0]  d_kind_q;
  logic [15:0]  d_mid_q;      // the draw's material_id, its OWN ABI field
  logic [31:0]  d_mset_q;     // ... and the complete set handle, same enable
  logic [15:0]  d_tick_q;
  logic [ 1:0]  d_vmask_q;
  logic [15:0]  d_src_q;
  logic [15:0]  scan_q;      // which record the scan is on

  // The resident record's captured halves.
  // The three captured lines. Each carries RESERVED bytes (`rsv0`..`rsv5` in
  // `zref_forge_page.hpp`) that this block does not read and must not read: a
  // reserved byte is the page format's room to grow, and a decoder that gives
  // one a meaning today is what makes the growth a breaking change tomorrow.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [511:0] rec0_q, rec1_q, rec2_q;
  /* verilator lint_on UNUSEDSIGNAL */

  // The fork's two halves, retired independently and issued together.
  logic         p_done_q, x_done_q, a_done_q;

  assign busy_o   = (state_q != S_IDLE);
  assign d_ready_o = (state_q == S_IDLE) && !asm_busy_i;

  // ==========================================================================
  // THE READ REQUEST
  //
  // TWO CYCLES, LEVEL THEN PULSE -- MEM.GUARD's protocol. `rsp.ready` is a
  // LEVEL and `rsp.ok` a PULSE one cycle after the accept, so the verdict gets
  // its own state. A block that tested both on the same cycle would read every
  // pass as a denial.
  // ENFORCED-BY: tools/rtl/check_guard_verdict.py
  // ==========================================================================
  wire req_c = (state_q == S_HREQ) || (state_q == S_SREQ)
            || (state_q == S_L1REQ) || (state_q == S_L2REQ);

  always_comb begin
    g_req_o        = '0;
    g_req_o.valid  = req_c;
    g_req_o.write  = 1'b0;
    g_req_o.client = CLIENT;
    g_req_o.addr   = addr_q[ZHAO_VRAM_ADDR_BITS-1:0];
    g_req_o.len    = 7'(LINE_BYTES);
    // The shape rule: the mask must match the length. A whole line is all
    // sixty-four lanes.
    g_req_o.be     = {64{1'b1}};
  end

  wire verd_c  = (state_q == S_HVERD) || (state_q == S_SVERD)
              || (state_q == S_L1VERD) || (state_q == S_L2VERD);
  wire beats_c = (state_q == S_HBEATS) || (state_q == S_SBEATS)
              || (state_q == S_L1BEATS) || (state_q == S_L2BEATS);

  wire denied_c = (req_c || verd_c || beats_c) && g_rsp_i.violation;
  wire accept_c = req_c  && g_rsp_i.ready && !g_rsp_i.violation;
  wire verdok_c = verd_c && g_rsp_i.ok    && !g_rsp_i.violation;

  wire pub_match_c = pub_valid_i && (pub_tag_i == PAGE_KIND);

  // ==========================================================================
  // THE PAGE HEADER, read out of the assembled line
  // ==========================================================================
  wire [31:0] hdr_magic_c = line_q[31:0];
  wire [15:0] hdr_ver_c   = line_q[47:32];
  wire [15:0] hdr_count_c = line_q[63:48];

  wire hdr_ok_c    = (hdr_magic_c == MAGIC) && (hdr_ver_c == VERSION);
  wire hdr_rooms_c = (32'(hdr_count_c) <= 32'(MAX_SCAN));
  // Every record must lie wholly inside the published extent. `<=` because the
  // extent is a length and the last record's final byte is at
  // HEADER + count*RECORD - 1.
  wire hdr_fits_c  = ((32'(HEADER_BYTES) + 32'(hdr_count_c) * 32'(RECORD_BYTES))
                      <= pub_extent_i);

  // ==========================================================================
  // RECORD LINE 0 -- identity, topology and the common frame.
  // Offsets are `zref_forge_page.hpp`'s, frozen, little-endian.
  // ==========================================================================
  // A BYTE OFFSET IS A BIT RANGE [8N+7 : 8N], AND GETTING IT WRONG IS SILENT.
  // The first draft of this block read `anchor0` one 32-bit word low -- out of
  // `rsv0` -- and every field beside it was right, so no count, no handshake
  // and no verdict could have seen it: a lightning bolt would simply have
  // started in the wrong place. `verilator --lint-only -Wall` found it, as
  // UNUSEDSIGNAL on the bits the record HAS and this block never read. That is
  // the whole argument for reading a lint's unused-bit warnings on a record
  // decoder instead of silencing them.
  wire [31:0] r0_program_c = rec0_q[ 31:  0];   // byte 0
  wire [ 7:0] r0_familyb_c = rec0_q[ 39: 32];   // byte 4
  wire [ 2:0] r0_family_c  = r0_familyb_c[2:0];
  wire [ 7:0] r0_sweepb_c  = rec0_q[ 47: 40];   // byte 5
  wire        r0_sweep_c   = r0_sweepb_c[0];
  wire [ 7:0] r0_segs_c    = rec0_q[ 55: 48];   // byte 6
  wire [ 7:0] r0_sides_c   = rec0_q[ 63: 56];   // byte 7
  wire [ 7:0] r0_vmaskb_c  = rec0_q[ 71: 64];   // byte 8
  wire [ 1:0] r0_vmask_c   = r0_vmaskb_c[1:0];
  wire [ 7:0] r0_brcntb_c  = rec0_q[ 79: 72];   // byte 9
  wire [ 1:0] r0_brcnt_c   = r0_brcntb_c[1:0];
  wire signed [31:0] r0_a0x_c = $signed(rec0_q[159:128]);   // byte 16
  wire signed [31:0] r0_a0y_c = $signed(rec0_q[191:160]);
  wire signed [31:0] r0_a0z_c = $signed(rec0_q[223:192]);
  wire signed [31:0] r0_a1x_c = $signed(rec0_q[255:224]);   // byte 28
  wire signed [31:0] r0_a1y_c = $signed(rec0_q[287:256]);
  wire signed [31:0] r0_a1z_c = $signed(rec0_q[319:288]);
  wire signed [31:0] r0_ux_c  = $signed(rec0_q[351:320]);   // byte 40
  wire signed [31:0] r0_uy_c  = $signed(rec0_q[383:352]);
  wire signed [31:0] r0_uz_c  = $signed(rec0_q[415:384]);
  wire signed [31:0] r0_vx_c  = $signed(rec0_q[447:416]);   // byte 52
  wire signed [31:0] r0_vy_c  = $signed(rec0_q[479:448]);
  wire signed [31:0] r0_vz_c  = $signed(rec0_q[511:480]);

  // ---- record line 1 -- radii and the ribbon's jitter law -------------------
  wire signed [31:0] r1_rad0_c  = $signed(rec1_q[ 31:  0]);  // byte 64
  wire signed [31:0] r1_rad1_c  = $signed(rec1_q[ 63: 32]);  // byte 68
  wire signed [31:0] r1_amp_c   = $signed(rec1_q[ 95: 64]);  // byte 72
  wire signed [31:0] r1_bamp_c  = $signed(rec1_q[127: 96]);  // byte 76
  wire signed [31:0] r1_brad_c  = $signed(rec1_q[159:128]);  // byte 80
  wire        [31:0] r1_seed_c  = rec1_q[191:160];           // byte 84
  wire        [15:0] r1_phase_c = rec1_q[207:192];           // byte 88
  wire signed [31:0] r1_wx_c    = $signed(rec1_q[255:224]);  // byte 92
  wire signed [31:0] r1_wy_c    = $signed(rec1_q[287:256]);
  wire signed [31:0] r1_wz_c    = $signed(rec1_q[319:288]);

  // ---- record line 2 -- the ribbon's branches -------------------------------
  // Narrow at the DECLARATION, because the high bits are not this expression's
  // business: `l2_legal_c` has already refused any record whose byte does not
  // fit the evaluator's field, so what survives to here is exactly 7 and 4 bits
  // wide. Declaring them 8 and slicing later would leave a reader asking which
  // of the two places does the refusing.
  wire [ 6:0] r2_b0att_c = rec2_q[  6:  0];                  // byte 128
  wire [ 3:0] r2_b0seg_c = rec2_q[ 11:  8];                  // byte 129
  wire signed [31:0] r2_b0x_c = $signed(rec2_q[ 63: 32]);    // byte 132
  wire signed [31:0] r2_b0y_c = $signed(rec2_q[ 95: 64]);
  wire signed [31:0] r2_b0z_c = $signed(rec2_q[127: 96]);
  wire [ 6:0] r2_b1att_c = rec2_q[134:128];                  // byte 144
  wire [ 3:0] r2_b1seg_c = rec2_q[139:136];                  // byte 145
  wire signed [31:0] r2_b1x_c = $signed(rec2_q[191:160]);    // byte 148
  wire signed [31:0] r2_b1y_c = $signed(rec2_q[223:192]);
  wire signed [31:0] r2_b1z_c = $signed(rec2_q[255:224]);

  // The scan's compare. The high byte of the stored program index is reserved
  // because a handle32's low byte is its GENERATION -- the same reservation
  // `zhao_geom_ladderbank` makes, for the same reason.
  wire scan_hit_c = (r0_program_c[31:24] == 8'd0)
                 && (r0_program_c[23:0] == d_index_q);

  // ==========================================================================
  // THE RECORD'S OWN LEGALITY -- the NARROWED fields, checked rather than
  // truncated.
  //
  // The page carries `segments`, `sides`, `view_mask`, `branch_count`,
  // `family`, `sweep` and the two branch descriptors as whole BYTES, and the
  // evaluators take them as 7, 4, 2, 2, 3 and 1 bits. Silently dropping the
  // high bits would turn a record declaring 130 segments into one declaring 2,
  // which is a picture bug with no counter -- so every high bit that the
  // evaluators cannot represent is a REFUSAL here. This is also what keeps the
  // lint's unused-bit warnings honest: the bits are read, by the check that
  // exists because they must be zero.
  //
  // The LOW bounds are the evaluators' own and are NOT re-stated as arithmetic:
  // `zhao_forge_prim` refuses `segments` or `sides` out of range on its own
  // port and counts it on `refused_limit_o`. Duplicating that here would be a
  // second opinion about one law. What this block refuses is only what the
  // evaluator could never SEE.
  // ==========================================================================
  // Line 0's half, judged at the scan's hit -- BEFORE the two remaining lines
  // are fetched, so an illegal record costs one read rather than three.
  wire rec0_legal_c = (r0_familyb_c[7:3] == 5'd0)   // family fits 3 bits
                   && (r0_sweepb_c[7:1]  == 7'd0)   // sweep is one bit
                   && (r0_segs_c[7]      == 1'b0)   // segments fit 7 bits
                   && (r0_sides_c[7:4]   == 4'd0)   // sides fit 4 bits
                   && (r0_vmaskb_c[7:2]  == 6'd0)   // view_mask fits 2 bits
                   && (r0_brcntb_c[7:2]  == 6'd0);  // branch_count fits 2 bits

  // Line 2's half, judged from the LANDING composite rather than from `rec2_q`
  // -- the same value by the same expression, one cycle earlier, on the edge
  // that decides whether to issue. The branches are the RIBBON's alone, so this
  // is applied on the ribbon path only: a swept-ring record's line 2 is
  // reserved and a packer is free to leave anything there.
  // The whole composite is declared so the bit numbers below read as the
  // record's own byte offsets; only the four narrowing bits are read from it.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [511:0] land_c = {g_beat_data_i, line_q[511:64]};
  /* verilator lint_on UNUSEDSIGNAL */
  wire l2_legal_c = (land_c[  7] == 1'b0)   // br0_attach fits 7 bits
                 && (land_c[135] == 1'b0)   // br1_attach fits 7 bits
                 && (land_c[ 15: 12] == 4'd0)   // br0_segments fits 4 bits
                 && (land_c[143:140] == 4'd0);  // br1_segments fits 4 bits

  // The page's family governs; the command's `kind` must agree AFTER the
  // rotation. Both operands are captured from different places, so this is a
  // real comparison.
  wire kind_ok_c  = (d_kind_q == kind_of_family(r0_family_c))
                 && (r0_familyb_c[7:3] == 5'd0);
  wire is_cliff_c = (r0_family_c == FAM_CLIFF);
  wire is_ribbon_c= (r0_family_c == FAM_RIBBON);

  // ==========================================================================
  // THE TWO JOB PORTS. Both halves of one primitive, issued as ONE acceptance.
  // ==========================================================================
  assign a_valid_o = (state_q == S_ISSUE) && !a_done_q;
  assign p_valid_o = (state_q == S_ISSUE) && !p_done_q;
  assign e_valid_o = (state_q == S_ISSUE) && !x_done_q &&  is_ribbon_c;
  assign r_valid_o = (state_q == S_ISSUE) && !x_done_q && !is_ribbon_c;

  assign p_family_o    = r0_family_c;
  assign p_segments_o  = r0_segs_c[6:0];
  assign p_sides_o     = r0_sides_c[3:0];
  // The per-triangle material id `zhao_forge_prim` already carries. It is the
  // DRAW'S OWN `material_id` now, not a slice of the set handle.
  assign p_material_o  = d_mid_q;
  assign p_view_mask_o = r0_vmask_c & d_vmask_q;
  assign p_src_id_o    = d_src_q;
  assign p_material_set_o = d_mset_q;
  assign p_material_id_o  = d_mid_q;

  // THE LIVE PHASE IS base + frame_tick (R241 D-TICK-A), and the add WRAPS.
  // A phase is an angle16 whose whole turn is the WIDTH of the field, so a
  // saturating add here would stall the animation at the top of the turn --
  // the opposite of what an animating phase must do.
  wire [15:0] tick_phase_c = r1_phase_c + d_tick_q;

  assign e_start_x_o = r0_a0x_c;
  assign e_start_y_o = r0_a0y_c;
  assign e_start_z_o = r0_a0z_c;
  assign e_end_x_o   = r0_a1x_c;
  assign e_end_y_o   = r0_a1y_c;
  assign e_end_z_o   = r0_a1z_c;
  assign e_perp1_x_o = r0_ux_c;
  assign e_perp1_y_o = r0_uy_c;
  assign e_perp1_z_o = r0_uz_c;
  assign e_perp2_x_o = r0_vx_c;
  assign e_perp2_y_o = r0_vy_c;
  assign e_perp2_z_o = r0_vz_c;
  assign e_waxis_x_o = r1_wx_c;
  assign e_waxis_y_o = r1_wy_c;
  assign e_waxis_z_o = r1_wz_c;
  // A RIBBON'S TWO RADII MUST BE EQUAL -- `zref_forge_page.hpp`'s first
  // declared hole, because this evaluator carries ONE half_width. The page
  // REFUSES a tapering ribbon at pack time; here the agreed value is radius0,
  // named rather than averaged, so that if the packer's refusal is ever lifted
  // this line is the one that has to change.
  assign e_half_width_o        = r1_rad0_c;
  assign e_branch_half_width_o = r1_brad_c;
  assign e_amp_o               = r1_amp_c;
  assign e_branch_amp_o        = r1_bamp_c;
  assign e_seed_o              = r1_seed_c;
  assign e_tick_phase_o        = tick_phase_c;
  assign e_segments_o          = r0_segs_c[6:0];
  assign e_branch_count_o      = r0_brcnt_c;
  assign e_br0_attach_o        = r2_b0att_c;
  assign e_br0_segments_o      = r2_b0seg_c;
  assign e_br0_end_x_o         = r2_b0x_c;
  assign e_br0_end_y_o         = r2_b0y_c;
  assign e_br0_end_z_o         = r2_b0z_c;
  assign e_br1_attach_o        = r2_b1att_c;
  assign e_br1_segments_o      = r2_b1seg_c;
  assign e_br1_end_x_o         = r2_b1x_c;
  assign e_br1_end_y_o         = r2_b1y_c;
  assign e_br1_end_z_o         = r2_b1z_c;
  assign e_view_mask_o         = r0_vmask_c & d_vmask_q;
  assign e_src_id_o            = d_src_q;

  assign r_family_o    = r0_family_c;
  assign r_sweep_o     = r0_sweep_c;
  assign r_segments_o  = r0_segs_c[6:0];
  assign r_sides_o     = r0_sides_c[3:0];
  assign r_a0_x_o      = r0_a0x_c;
  assign r_a0_y_o      = r0_a0y_c;
  assign r_a0_z_o      = r0_a0z_c;
  assign r_a1_x_o      = r0_a1x_c;
  assign r_a1_y_o      = r0_a1y_c;
  assign r_a1_z_o      = r0_a1z_c;
  assign r_u_x_o       = r0_ux_c;
  assign r_u_y_o       = r0_uy_c;
  assign r_u_z_o       = r0_uz_c;
  assign r_v_x_o       = r0_vx_c;
  assign r_v_y_o       = r0_vy_c;
  assign r_v_z_o       = r0_vz_c;
  assign r_r0_o        = r1_rad0_c;
  assign r_r1_o        = r1_rad1_c;
  assign r_view_mask_o = r0_vmask_c & d_vmask_q;
  assign r_src_id_o    = d_src_q;

  wire p_take_c = p_valid_o && p_ready_i;
  wire x_take_c = (e_valid_o && e_ready_i) || (r_valid_o && r_ready_i);
  wire a_take_c = a_valid_o && a_ready_i;
  wire p_held_c = p_done_q || p_take_c;
  wire x_held_c = x_done_q || x_take_c;
  wire a_held_c = a_done_q || a_take_c;

  // ==========================================================================
  // THE MACHINE
  // ==========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q          <= S_IDLE;
      line_q           <= '0;
      beat_q           <= 3'd0;
      addr_q           <= 32'd0;
      page_v_q         <= 1'b0;
      page_base_q      <= 32'd0;
      page_count_q     <= 16'd0;
      d_index_q        <= 24'd0;
      d_kind_q         <= 8'd0;
      d_mid_q          <= 16'd0;
      d_mset_q         <= 32'd0;
      d_tick_q         <= 16'd0;
      d_vmask_q        <= 2'd0;
      d_src_q          <= 16'd0;
      scan_q           <= 16'd0;
      rec0_q           <= '0;
      rec1_q           <= '0;
      rec2_q           <= '0;
      p_done_q         <= 1'b0;
      x_done_q         <= 1'b0;
      a_done_q         <= 1'b0;
      pages_o          <= 32'd0;
      draws_o          <= 32'd0;
      bad_magic_o      <= 32'd0;
      page_overflow_o  <= 32'd0;
      truncated_o      <= 32'd0;
      lookup_miss_o    <= 32'd0;
      refused_kind_o   <= 32'd0;
      refused_cliff_o  <= 32'd0;
      bad_record_o     <= 32'd0;
      refused_nopage_o <= 32'd0;
      denied_o         <= 32'd0;
    end else begin

      // A GUARD VIOLATION ABANDONS WHATEVER WAS IN FLIGHT, from any read state.
      // The adopted page is NOT dropped by a failed per-draw read: the page is
      // still whatever the publication left, and a draw that could not be read
      // is one refused draw rather than a bank that empties itself.
      if (denied_c) begin
        denied_o <= denied_o + 32'd1;
        state_q  <= S_IDLE;
        if (state_q == S_HREQ || state_q == S_HVERD || state_q == S_HBEATS) begin
          page_v_q <= 1'b0;
        end
      end else begin
        unique case (state_q)

          S_IDLE: begin
            p_done_q <= 1'b0;
            x_done_q <= 1'b0;
            a_done_q <= 1'b0;
            beat_q   <= 3'd0;
            if (pub_match_c) begin
              // A PUBLICATION OUTRANKS A DRAW on the same cycle, and the draw
              // is simply not accepted (`d_ready_o` is low from the next
              // cycle). CMD.EXEC holds its head until taken, so nothing is
              // lost; the alternative -- answering a draw from a page that is
              // being replaced -- is the stale-copy fault.
              page_base_q <= pub_base_i;
              addr_q      <= pub_base_i;
              if (pub_extent_i < 32'(HEADER_BYTES)) begin
                truncated_o <= truncated_o + 32'd1;
                page_v_q    <= 1'b0;
              end else begin
                state_q <= S_HREQ;
              end
            end else if (d_valid_i && d_ready_o) begin
              // `&& d_ready_o` IS THE REPAIR, AND WITHOUT IT THIS BLOCK
              // CONSUMED A DRAW IT HAD NOT ACCEPTED. `d_ready_o` is low while
              // `asm_busy_i` is high, so CMD.EXEC does NOT advance its queue
              // head -- but this arm used to start the scan anyway, issue the
              // primitive, return here and find THE SAME RECORD STILL OFFERED.
              // The result was the same procedural draw executed over and over:
              // 472 issues from two commands, measured by
              // `procmat_acceptance`. Every handshake in the chain balanced,
              // because the duplicate is a whole legitimate job.
              d_index_q    <= d_program_i[31:8];
              d_kind_q     <= d_kind_i;
              // ONE ENABLE, TWO FIELDS. The pair is captured on the draw's
              // own accepted handshake and nothing else writes either
              // register, so they can only ever describe the same draw.
              d_mid_q      <= d_material_id_i;
              d_mset_q     <= d_material_i;
              d_tick_q     <= d_frame_tick_i;
              d_vmask_q    <= d_view_mask_i;
              d_src_q      <= d_src_id_i;
              scan_q       <= 16'd0;
              if (!page_v_q) begin
                refused_nopage_o <= refused_nopage_o + 32'd1;
              end else if (page_count_q == 16'd0) begin
                // A legal, EMPTY page. Every lookup misses HONESTLY.
                lookup_miss_o <= lookup_miss_o + 32'd1;
              end else begin
                addr_q  <= page_base_q + 32'(HEADER_BYTES);
                state_q <= S_SREQ;
              end
            end
          end

          // ---- the publication's header -------------------------------------
          S_HREQ:  if (accept_c) begin beat_q <= 3'd0; state_q <= S_HVERD; end
          S_HVERD: if (verdok_c) state_q <= S_HBEATS;
          S_HBEATS: begin
            if (g_beat_valid_i) begin
              line_q <= {g_beat_data_i, line_q[511:64]};   // LSB-first assembly
              beat_q <= beat_q + 3'd1;
              // The eighth beat lands on THIS edge, so the line is not readable
              // until the next one. S_HDR is that cycle.
              if (beat_q == 3'd7) state_q <= S_HDR;
            end
          end

          S_HDR: begin
            state_q <= S_IDLE;
            if (!hdr_ok_c) begin
              bad_magic_o <= bad_magic_o + 32'd1;
              page_v_q    <= 1'b0;
            end else if (!hdr_rooms_c) begin
              // REFUSED WHOLE, never scanned short. A bank answering from the
              // first MAX_SCAN programs of a larger page reads as a content bug
              // and is not one -- `zref::species_page`'s sentence.
              page_overflow_o <= page_overflow_o + 32'd1;
              page_v_q        <= 1'b0;
            end else if (!hdr_fits_c) begin
              truncated_o <= truncated_o + 32'd1;
              page_v_q    <= 1'b0;
            end else begin
              page_count_q <= hdr_count_c;
              page_v_q     <= 1'b1;
              pages_o      <= pages_o + 32'd1;
            end
          end

          // ---- the scan: record line 0 --------------------------------------
          S_SREQ:  if (accept_c) begin beat_q <= 3'd0; state_q <= S_SVERD; end
          S_SVERD: if (verdok_c) state_q <= S_SBEATS;
          S_SBEATS: begin
            if (g_beat_valid_i) begin
              line_q <= {g_beat_data_i, line_q[511:64]};
              beat_q <= beat_q + 3'd1;
              if (beat_q == 3'd7) begin
                rec0_q  <= {g_beat_data_i, line_q[511:64]};
                state_q <= S_SCHK;
              end
            end
          end

          S_SCHK: begin
            if (scan_hit_c) begin
              if (!rec0_legal_c) begin
                // A field the evaluators cannot hold. REFUSED, never truncated:
                // 130 segments silently becoming 2 is a picture bug with no
                // counter, which is the one outcome this block exists to avoid.
                bad_record_o <= bad_record_o + 32'd1;
                state_q      <= S_IDLE;
              end else if (is_cliff_c) begin
                // FORGE.CLIFF's positions come from the terrain lattice, not
                // from a parameter block. Refusing here is narrower and more
                // honest than sending anchors to an evaluator that does not
                // read them.
                refused_cliff_o <= refused_cliff_o + 32'd1;
                state_q         <= S_IDLE;
              end else if (!kind_ok_c) begin
                // THE PAGE GOVERNS. `zref_forge_page.hpp`: "a `kind` that
                // disagrees REFUSES the draw and is counted".
                refused_kind_o <= refused_kind_o + 32'd1;
                state_q        <= S_IDLE;
              end else begin
                addr_q  <= addr_q + 32'(LINE_BYTES);
                beat_q  <= 3'd0;
                state_q <= S_L1REQ;
              end
            end else if (scan_q + 16'd1 >= page_count_q) begin
              lookup_miss_o <= lookup_miss_o + 32'd1;
              state_q       <= S_IDLE;
            end else begin
              scan_q  <= scan_q + 16'd1;
              addr_q  <= addr_q + 32'(RECORD_BYTES);
              beat_q  <= 3'd0;
              state_q <= S_SREQ;
            end
          end

          // ---- the hit's remaining two lines --------------------------------
          S_L1REQ:  if (accept_c) begin beat_q <= 3'd0; state_q <= S_L1VERD; end
          S_L1VERD: if (verdok_c) state_q <= S_L1BEATS;
          S_L1BEATS: begin
            if (g_beat_valid_i) begin
              line_q <= {g_beat_data_i, line_q[511:64]};
              beat_q <= beat_q + 3'd1;
              if (beat_q == 3'd7) begin
                rec1_q  <= {g_beat_data_i, line_q[511:64]};
                addr_q  <= addr_q + 32'(LINE_BYTES);
                state_q <= S_L2REQ;
              end
            end
          end

          S_L2REQ:  if (accept_c) begin beat_q <= 3'd0; state_q <= S_L2VERD; end
          S_L2VERD: if (verdok_c) state_q <= S_L2BEATS;
          S_L2BEATS: begin
            if (g_beat_valid_i) begin
              line_q <= {g_beat_data_i, line_q[511:64]};
              beat_q <= beat_q + 3'd1;
              if (beat_q == 3'd7) begin
                rec2_q  <= {g_beat_data_i, line_q[511:64]};
                // The branch descriptors are judged HERE, from the beat that is
                // landing, rather than one cycle later from `rec2_q` -- the
                // same value by the same expression, but available on the edge
                // that decides. A ribbon only: for a swept ring this line is
                // reserved and a packer may leave anything in it.
                if (is_ribbon_c && !l2_legal_c) begin
                  bad_record_o <= bad_record_o + 32'd1;
                  state_q      <= S_IDLE;
                end else begin
                  state_q <= S_ISSUE;
                end
              end
            end
          end

          // ---- both halves of one primitive, one acceptance -----------------
          S_ISSUE: begin
            p_done_q <= p_held_c;
            x_done_q <= x_held_c;
            a_done_q <= a_held_c;
            if (p_held_c && x_held_c && a_held_c) begin
              draws_o  <= draws_o + 32'd1;
              p_done_q <= 1'b0;
              x_done_q <= 1'b0;
              a_done_q <= 1'b0;
              state_q  <= S_IDLE;
            end
          end

          default: state_q <= S_IDLE;
        endcase
      end
    end
  end

endmodule : zhao_forge_pagebank

`default_nettype wire
