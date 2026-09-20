// zhao_geom_bonesrc.sv — the per-bone source `zhao_geom_pose_decode` fetches
// from, and THE PRICING PROBE for owner decision D-GEOMSEAM-A.
//
// Entry I29 of `zhao_console_core.sv`. The decoder drives `bone_idx_o` and
// requires that bone's parent, rest translation, quaternion and inverse-rest
// matrix on the wires in the SAME cycle. This block is that caller.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE IS PARAMETERISED BY STORAGE STYLE, AND WHY THAT IS NOT
// DECORATION
// ---------------------------------------------------------------------------
// Owner ruling R90's 2026-09-20 amendment priced the source at "a ~17.6 kbit
// ASYNCHRONOUS-READ store that a synchronous M10K cannot serve", on a device
// measured at ~113% of its ALM ceiling, and made PRICING IT a precondition of
// building it. A price is not a price until two arrangements are measured
// against each other, so the two arrangements live here as `SRC_STYLE` and are
// mapped from ONE source. The alternative — measure one, estimate the other —
// is the thing CLAUDE.md's art law forbids one directory over.
//
//   SRC_STYLE  name           store                        what it isolates
//   ---------  -------------  ---------------------------  --------------------
//       0      SYNC_M10K      2 sync RAMs + 2-deep         the shipped design
//                             320-bit prefetch + 2:1 mux
//       1      ASYNC_DERIVED  32 x 320 async array         the SYNC lever alone
//       2      ASYNC_FLAT     32 x 576 async array         R90's literal reading
//
// SRC_STYLE 2 is the arrangement R90 priced; 1 and 2 differ ONLY by whether
// `inv_rest` is stored or derived; 0 and 1 differ ONLY by whether the read is
// synchronous. Three rows therefore say which of the two levers does the work,
// which one number never could.
//
// ---------------------------------------------------------------------------
// THE PRICE, MEASURED. `quartus_map` 17.0.2, device 5CSEBA6U23I7 (THE TARGET
// DEVICE), standalone leaf, VIRTUAL_PIN ON, SEED 1, BALANCED, MAX_BONES = 32.
// Analysis & Synthesis "Estimate of Logic utilization (ALMs needed)".
// ---------------------------------------------------------------------------
//
//   SRC_STYLE        ALM     ALUT    regs   blockmem  MLAB   % of 41,910
//   ---------------  ------  ------  -----  --------  -----  -----------
//   0 SYNC_M10K         830     458    866    10,240      0      2.0%
//   1 ASYNC_DERIVED   6,440   9,234  8,449         0      0     15.4%
//   2 ASYNC_FLAT     14,056  21,617 17,665         0      0     33.5%
//
// **R90's arrangement costs 14,056 ALM — ONE THIRD OF THE ENTIRE DEVICE**, on
// a console already reported over its ALM ceiling. It is not affordable, and
// the amendment was right to make the pricing a precondition of building.
//
// **The arrangement actually built costs 830 ALM, 2.0%.** The saving is 13,226
// ALM, a 16.9x reduction, and the three rows attribute it rather than asserting
// it: deriving `inv_rest` is worth 7,616 ALM (14,056 -> 6,440) and making the
// read synchronous is worth a further 5,610 (6,440 -> 830).
//
// RE-MEASURED after the two detector-arming repairs below, because a number
// taken before a change is a number about a machine that no longer exists
// (CLAUDE.md, "FIXED, NEVER RE-MEASURED"). Style 0 moved 829 -> 830 ALM and
// 865 -> 866 registers: EXACTLY the one flip-flop `started_q` added. The two
// async rows did not move at all, because the repair is inside the
// `SRC_STYLE == 0` generate branch. A delta that matches the edit is the only
// form in which "nothing moved" is evidence rather than a stale binary.
//
// TWO THINGS THE ROWS SETTLE THAT ARITHMETIC COULD NOT:
//
//   * **Quartus infers NO MLAB for the asynchronous array** — "Total MLAB
//     memory bits: 0" on styles 1 and 2, with every stored bit landing in
//     dedicated registers. The optimistic reading (an async store becomes cheap
//     LUT RAM) is false on this device and this tool, and style 2's 17,665
//     registers are R90's own 17,568-bit figure plus this block's 97 counter
//     and control bits. **R90's BIT COUNT WAS EXACTLY RIGHT.** What was
//     unpriced was what those bits cost in ALMs, and the answer is that a
//     32-deep multiplexer over 549 bits costs more than the storage does:
//     21,617 ALUTs against 17,665 registers.
//   * **Style 0 infers BLOCK memory, not MLAB** — 10,240 block memory bits,
//     0 MLAB bits, exactly the 8,192 + 2,048 the two stores ask for. This is
//     the measurement `zhao_geom_pose_palette`'s header and
//     `design/fit_targets.yml` both say is missing for that block and must not
//     be claimed from arithmetic; it is made here, for these two arrays.
//
// A MAP IS NOT A FIT and this file does not pretend otherwise: the ALM figure
// is Analysis & Synthesis's ESTIMATE, there is no placement and no routing, and
// no timing number appears above because none was measured. What a map settles
// is exactly what was asked — relative area and whether storage becomes RAM or
// flops — and the gap between 830 and 14,056 is not a number a fit reverses.
//
// ---------------------------------------------------------------------------
// LEVER 1 — `inv_rest` IS DERIVED, AND THE REFERENCE IS WHERE THAT IS DECIDED
// ---------------------------------------------------------------------------
// R90 counted the decoder's source at 549 bits per bone, of which
// `inv_rest_i[12]` is 384. THOSE 384 BITS ARE NOT 384 BITS OF ASSET.
// `bake_skeleton` (reference/src/zcreature/creature_core.cpp) builds every
// inverse-rest matrix as
//
//     inv_rest[b] = identity, with m[3]/m[7]/m[11] = -world_rest[b]
//
// and `zref_creature.hpp`'s `Bone` carries NO rest rotation at all — only
// `{uint8_t parent; int32_t tx, ty, tz;}`. The header states the invariant as a
// bind convention: *"rings are authored in rest orientation, so B_rest is a
// pure translation chain and its inverse is EXACT (translate(-world_rest_pos),
// zero rounding)."*
//
// So nine of the twelve elements are the CONSTANTS 65536 and 0, and the other
// three are one negated vector. 288 of R90's 384 bits are wires to constants
// and cost nothing to store, nothing to fetch and nothing to multiplex.
//
// **THIS IS NOT HARDWIRED PAST THE OWNER.** `INV_REST_RIGID` is the knob, the
// page record carries a `flags` bit that declares the invariant per creature,
// and `bone_rest_nonrigid_o` COUNTS any record that arrives claiming otherwise
// rather than decoding it wrongly in silence. A future skeleton with real rest
// rotations is a parameter flip and a wider record, not a rewrite — and until
// one exists, paying 288 bits per bone to store nine constants would be paying
// for a generality nothing in the tree can author.
//
// ---------------------------------------------------------------------------
// LEVER 2 — THE FETCH IS COMBINATIONAL, AND IT IS STILL NOT AN ASYNC STORE
// ---------------------------------------------------------------------------
// This is the finding that decides the packet, and it is a statement about the
// decoder's FSM rather than about its header.
//
// `zhao_geom_pose_decode`'s `bone_idx_o` is `b`, A REGISTER, and `b` moves in
// exactly two places: `S_IDLE` on `start_i`, and `S_EMIT` on the accepted last
// beat. Between them the decoder spends a MEASURED 115.4 cycles per bone
// (its own header, `geom_pose_decode_directed`). So the address is stable for
// ~115 cycles and changes on one edge, and the DATA is consumed on exactly one
// of those cycles — the single `S_FETCH` beat that follows the change.
//
// A naive synchronous read of `bone_idx_o` misses by ONE CYCLE and no more:
// `b` becomes b+1 at the same edge that enters `S_FETCH`, `S_FETCH` latches at
// the next edge, and a registered read would land one edge after that. That one
// cycle is the whole of the "asynchronous store" requirement.
//
// It is bought with a PREFETCH, because bone b+1's address is knowable from the
// moment bone b begins — roughly 115 cycles of notice for a fetch that needs
// five. This block holds two bones: `d0` for `i0_q` and `d1` for `i0_q + 1`,
// selects between them combinationally on the decoder's own `bone_idx_i`, and
// on observing the advance shifts d1 into d0 and refills d1 from RAM. The
// decoder sees a purely combinational source and DOES NOT CHANGE — no port, no
// state, no contract. R90's fallback ("the thing that has to move is the
// DECODER's combinational contract") is not needed.
//
// The cost of that is a 2:1 mux over 320 bits and 640 flip-flops of prefetch,
// against a 32-deep multiplexer over 549. The measured difference is the packet.
//
// WHY NOT MIRROR THE DECODER'S FSM to know the advance a cycle early: that puts
// a second copy of a ratified sequencing law in a second file, which is the
// duplication `duplicate_functions.py` exists to find. The mux is cheaper than
// the maintenance.
//
// ---------------------------------------------------------------------------
// THE PREFETCH CANNOT BE LATE, AND THAT IS COUNTED RATHER THAN ASSERTED
// ---------------------------------------------------------------------------
// The whole scheme rests on "115 cycles is more than five". If a future
// arrangement shortens the decode or lengthens the fill, the prefetch is late
// and the decoder silently latches a stale bone — a wrong palette with every
// handshake intact and every counter balanced, which is this repository's
// named worst case.
//
// `bone_prefetch_late_o` is the detector, and it is NOT wired to two operands
// that move together: it compares the decoder's OWN `bone_idx_i` against
// `i0_q`, which is loaded by this block's fill sequencer on a different
// condition entirely. A late refill moves it; a correct one cannot.
//
// ---------------------------------------------------------------------------
// THE BYTES — kind-8 BODY bone record, FROZEN HERE
// ---------------------------------------------------------------------------
// 32 bytes per bone, the same record size the ladder table already uses, at
// `body_off` from the page header. Little-endian, the page's own convention.
//
//   +0   u8   parent        bone 0 must carry 0 (validated at bake)
//   +1   u8   flags         bit0 RIGID_REST (must be 1 in v1); rest reserved 0
//   +2   u16  reserved      must be 0
//   +4   s32  rest_tx       LOCAL rest translation, fx16 (Q16.16)
//   +8   s32  rest_ty
//   +12  s32  rest_tz
//   +16  s32  inv_rest_tx   = -world_rest_x, BAKED by the packer
//   +20  s32  inv_rest_ty
//   +24  s32  inv_rest_tz
//   +28  u32  reserved      must be 0
//
// `inv_rest_t` is baked by the packer and not recomputed here, for the reason
// the reference gives: it is a running sum down the parent chain, it is done
// ONCE at load in `bake_skeleton`, and a second implementation of it in RTL
// would be a second owner of a ratified law.
//
// THE CLIP FRAME (kind 9) IS NOT FROZEN HERE BECAUSE IT ALREADY IS.
// `spec/creature_rules.md` 2.1 is headed "Storage (frozen; the Q formats are
// frozen)" and gives the bytes outright — 12 B root displacement (3 x fx16)
// then `bone_count` x 8 B of `quat16`, <= 268 B/frame at 32 bones — and
// `spec/qformats.md` 7.6 ratifies the lane format under amendment C1
// (four s16 lanes, S 1.0.14, hemisphere-canonical). `zref_creature.hpp`'s
// "PROPOSED, NOT FROZEN" note predates that amendment and is corrected there.
// This block READS that layout; it does not re-freeze it.
//
// ---------------------------------------------------------------------------
// M10K INFERENCE RULES, copied deliberately from `zhao_geom_pose_decode`'s
// ancestor store: no initializer, no reset branch touching the array, and the
// read happens ONLY inside the clocked process. Whether Quartus agrees is the
// question the map answers, and until it does every memory figure here is
// arithmetic and says so.
// ---------------------------------------------------------------------------
module zhao_geom_bonesrc #(
    parameter int MAX_BONES = 32,

    // 0 = SYNC_M10K (shipped), 1 = ASYNC_DERIVED, 2 = ASYNC_FLAT (R90's).
    // See the table in the header. This is a PRICING knob and the owner's:
    // it is not narrowed to the winner once the winner is known, because the
    // next device or the next decode rate re-opens the same question.
    parameter int SRC_STYLE = 0,

    // The bind convention of the ring format (zref_creature.hpp): rest
    // rotations are identity, so inv_rest is translate(-world_rest). 0 makes
    // the nine constants into stored bits again and is the extension path.
    parameter int INV_REST_RIGID = 1
) (
    input  logic clk,
    input  logic rst_n,

    // ---- fill, from the page reader ----------------------------------------
    // One 64-bit word at a time, which is what a page read delivers. `sel`
    // picks the kind-8 body (0) or the kind-9 clip frame (1); the two are
    // separate stores because the skeleton is per-TYPE and the quaternions are
    // per-FRAME, and merging them would refill the skeleton every frame.
    input  logic        fill_we_i,
    input  logic        fill_sel_i,
    input  logic [ 4:0] fill_bone_i,
    input  logic [ 3:0] fill_word_i,
    input  logic [63:0] fill_data_i,

    // ---- begin one palette --------------------------------------------------
    // The source owns `start_o`, not the caller: bone 0 must be on the wires
    // BEFORE the decoder's first S_FETCH, and only this block knows when its
    // prefetch has landed. `req_i` asks; `start_o` is the answer.
    input  logic        req_i,
    input  logic [ 5:0] bone_count_i,
    output logic        start_o,
    output logic        ready_o,

    // ---- the decoder's combinational source --------------------------------
    input  logic [ 4:0] bone_idx_i,
    output logic [ 4:0] bone_parent_o,
    output logic signed [31:0] bone_tx_o,
    output logic signed [31:0] bone_ty_o,
    output logic signed [31:0] bone_tz_o,
    output logic signed [15:0] quat_w_o,
    output logic signed [15:0] quat_x_o,
    output logic signed [15:0] quat_y_o,
    output logic signed [15:0] quat_z_o,
    output logic signed [31:0] inv_rest_o [12],

    // ---- detectors ----------------------------------------------------------
    output logic [31:0] bone_prefetch_late_o,
    output logic [31:0] bone_rest_nonrigid_o,
    output logic [31:0] bone_reserved_nz_o,
    output logic [31:0] bone_fills_o
  );

  // Q16.16 one, the diagonal of an identity mat3x4. Named rather than literal
  // so `check_case_labels` / a reader can see what 65536 is doing here.
  localparam logic signed [31:0] FX16_ONE_C = 32'sd65536;

  // Per-bone stored payload. The body record is 32 B = 256 bits (4 x 64) and
  // the clip lane is 8 B = 64 bits.
  localparam int BODY_BITS = 256;
  localparam int QUAT_BITS = 64;
  localparam int BODY_WORDS = BODY_BITS / 64;   // 4
  // R90's arrangement stores inv_rest in full: 5 + 96 + 64 + 384 = 549, padded
  // to 9 x 64 = 576 so the fill port is identical across all three styles and
  // the map rows compare like with like.
  localparam int FLAT_BITS = 576;

  initial begin
    if (MAX_BONES != 32)
      $fatal(1, "zhao_geom_bonesrc: MAX_BONES must be 32 (creature_rules 1.2)");
    if (SRC_STYLE < 0 || SRC_STYLE > 2)
      $fatal(1, "zhao_geom_bonesrc: SRC_STYLE must be 0, 1 or 2");
    if (INV_REST_RIGID != 1)
      $fatal(1, "zhao_geom_bonesrc: INV_REST_RIGID=0 needs the wide record; unimplemented");
  end

  // ==========================================================================
  // The decoded view of one bone, shared by every style.
  // ==========================================================================
  // The reserved fields of the record (+1 flags 1..7, +2 u16, +28 u32) and the
  // unused high bits of the parent byte are deliberately not read by the decode
  // path -- that is what "reserved" means. They are not unchecked, though:
  // `bone_reserved_nz_o` below counts any fill that sets one, at the door,
  // which is where `zhao_geom_vdecode` checks its own reserved bits too.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [BODY_BITS-1:0] body_sel;
  /* verilator lint_on UNUSEDSIGNAL */
  logic [QUAT_BITS-1:0] quat_sel;

  // Field extraction, one place, so three styles cannot drift into three
  // layouts. Offsets are the record above, little-endian.
  assign bone_parent_o = body_sel[4:0];
  assign bone_tx_o = body_sel[63:32];
  assign bone_ty_o = body_sel[95:64];
  assign bone_tz_o = body_sel[127:96];

  // Unused under SRC_STYLE 2 on purpose: that style reads all twelve elements
  // out of the store instead of deriving three, which is the whole point of it.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [31:0] inv_tx_c, inv_ty_c, inv_tz_c;
  /* verilator lint_on UNUSEDSIGNAL */
  assign inv_tx_c = body_sel[159:128];
  assign inv_ty_c = body_sel[191:160];
  assign inv_tz_c = body_sel[223:192];

  assign quat_w_o = quat_sel[15:0];
  assign quat_x_o = quat_sel[31:16];
  assign quat_y_o = quat_sel[47:32];
  assign quat_z_o = quat_sel[63:48];

  // The twelve inverse-rest elements, flat, so the three styles drive ONE
  // signal and the unpack below is written once.
  //
  // STYLES 0 AND 1 rebuild it from the invariant: nine constants and one
  // negated vector (LEVER 1 in the header). STYLE 2 reads all twelve out of the
  // store, which is R90's literal arrangement and the thing being priced.
  //
  // THE FIRST DRAFT OF THIS PROBE GOT THAT WRONG AND THE MAP SAID SO. Style 2
  // stored 576 bits per bone and still DERIVED inv_rest, so Quartus pruned the
  // 320 bits nothing read and style 2 collapsed onto style 1 — two rows 19 ALM
  // apart, which looked like a result and was an artefact. A store is only
  // priced by what is READ out of it.
  logic [383:0] invrest_sel;

  generate
    if (SRC_STYLE != 2) begin : g_inv_rigid
      assign invrest_sel = {
          inv_tz_c, FX16_ONE_C, 32'sd0, 32'sd0,
          inv_ty_c, 32'sd0, FX16_ONE_C, 32'sd0,
          inv_tx_c, 32'sd0, 32'sd0, FX16_ONE_C
      };
    end
  endgenerate

  genvar gi;
  generate
    for (gi = 0; gi < 12; gi = gi + 1) begin : g_inv_unpack
      assign inv_rest_o[gi] = invrest_sel[gi*32 +: 32];
    end
  endgenerate

  // ==========================================================================
  // SRC_STYLE 0 — the shipped arrangement: synchronous stores behind a 2-deep
  // prefetch.
  // ==========================================================================
  generate
    if (SRC_STYLE == 0) begin : g_sync

      // The stores. Plain, per the M10K rules in the header.
      logic [63:0] body_mem [0:MAX_BONES*BODY_WORDS-1];
      logic [63:0] quat_mem [0:MAX_BONES-1];
      logic [63:0] body_rd_q, quat_rd_q;
      logic [$clog2(MAX_BONES*BODY_WORDS)-1:0] body_raddr;
      logic [$clog2(MAX_BONES)-1:0] quat_raddr;

      // The two held bones. d0 serves `i0_q`, d1 serves `i0_q + 1`.
      logic [BODY_BITS-1:0] d0_body, d1_body;
      logic [QUAT_BITS-1:0] d0_quat, d1_quat;
      logic [4:0] i0_q;

      // Fill sequencer over the RAM into d1.
      typedef enum logic [1:0] { P_IDLE, P_BODY, P_QUAT, P_LAND } pf_e;
      pf_e pf_q;
      logic [2:0] pf_w;
      logic [4:0] pf_bone;
      logic [BODY_BITS-1:0] pf_body;

      // ARMED ONLY WHILE A PALETTE IS ACTUALLY RUNNING, and this is a REPAIR
      // rather than a decoration. The first draft cleared `i0_q` to 0 when the
      // request arrived, while the decoder's `b` still held the LAST bone of
      // the previous palette -- so on the second palette, for the one cycle
      // between the prefetch landing and the decoder accepting `start_o`,
      // `bone_idx_i != i0_q` with the sequencer idle, and the jump detector
      // FIRED ON A CORRECT RUN. A detector that cries on healthy traffic is
      // worse than none: it trains a reader to discount it.
      //
      // Found by asking what legal stimulus reaches the counter, which is the
      // question a committed mutant exists to answer when the answer is "none"
      // -- here the answer was "a false one", which is the other thing that
      // question finds.
      //
      // `i0_q` now moves only when bone 0 actually lands, on the same edge the
      // decoder takes `start_i`, so the two are aligned by construction.
      logic started_q;

      assign body_raddr = ($clog2(MAX_BONES*BODY_WORDS))'(pf_bone * BODY_WORDS + 32'(pf_w));
      assign quat_raddr = ($clog2(MAX_BONES))'(pf_bone);

      always_ff @(posedge clk) begin
        if (fill_we_i && !fill_sel_i)
          body_mem[{27'd0, fill_bone_i} * BODY_WORDS + {30'd0, fill_word_i[1:0]}] <= fill_data_i;
        if (fill_we_i &&  fill_sel_i) quat_mem[fill_bone_i] <= fill_data_i;
        body_rd_q <= body_mem[body_raddr];
        quat_rd_q <= quat_mem[quat_raddr];
      end

      // THE COMBINATIONAL SOURCE. A 2:1 over 320 bits, not a 32:1 over 549.
      assign body_sel = (bone_idx_i == i0_q) ? d0_body : d1_body;
      assign quat_sel = (bone_idx_i == i0_q) ? d0_quat : d1_quat;

      assign ready_o = (pf_q == P_IDLE);

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          pf_q <= P_IDLE; pf_w <= '0; pf_bone <= '0; i0_q <= '0;
          d0_body <= '0; d1_body <= '0; d0_quat <= '0; d1_quat <= '0;
          pf_body <= '0; start_o <= 1'b0; started_q <= 1'b0;
          bone_prefetch_late_o <= '0;
        end else begin
          start_o <= 1'b0;

          // ARM ONE CYCLE AFTER `start_o`, NOT WITH IT. `start_o` is registered,
          // so it is high during cycle C and the decoder takes it at the END of
          // C — which is also when its `b` becomes 0. Arming in the same place
          // `start_o` is raised leaves one cycle in which `i0_q` is already 0
          // while `bone_idx_i` still holds the PREVIOUS palette's last bone, and
          // the jump detector fires on a perfectly correct second run.
          //
          // That is the SECOND off-by-one of exactly this shape in this block,
          // and the first one's repair is what made this one visible. Both were
          // caught by `geom_bonesrc_directed` case 4 — a second palette, back to
          // back — which one palette could never have shown.
          if (start_o) started_q <= 1'b1;

          if (started_q) begin
            // The decoder advanced. Shift and refill.
            if (bone_idx_i == i0_q + 5'd1) begin
              d0_body <= d1_body;
              d0_quat <= d1_quat;
              i0_q <= bone_idx_i;
              // LATE: the decoder has moved on and d1 was not yet filled, so
              // the bone it is about to latch is stale. Two operands, two
              // different loaders — see the header.
              if (pf_q != P_IDLE) bone_prefetch_late_o <= bone_prefetch_late_o + 32'd1;
              pf_q <= P_BODY; pf_w <= '0; pf_bone <= bone_idx_i + 5'd1;
            end else if (bone_idx_i != i0_q && pf_q == P_IDLE) begin
              // A jump the prefetch cannot cover. Counted, never silent.
              bone_prefetch_late_o <= bone_prefetch_late_o + 32'd1;
            end
          end

          unique case (pf_q)
            P_IDLE: begin
              if (req_i) begin
                pf_q <= P_BODY; pf_w <= '0; pf_bone <= '0; started_q <= 1'b0;
              end
            end
            P_BODY: begin
              if (pf_w > 3'd0) pf_body[({29'd0, pf_w} - 1) * 64 +: 64] <= body_rd_q;
              if (pf_w == 3'd4) begin pf_q <= P_QUAT; pf_w <= '0; end
              else pf_w <= pf_w + 3'd1;
            end
            P_QUAT: begin
              if (pf_w == 3'd1) pf_q <= P_LAND;
              pf_w <= pf_w + 3'd1;
            end
            P_LAND: begin
              if (pf_bone == 5'd0) begin
                d0_body <= pf_body; d0_quat <= quat_rd_q;
                pf_q <= P_BODY; pf_w <= '0; pf_bone <= 5'd1;
              end else begin
                d1_body <= pf_body; d1_quat <= quat_rd_q;
                pf_q <= P_IDLE;
                // Bone 1 landing is the start: both held bones are resident,
                // so `i0_q` and the decoder's `b` become 0 on the SAME edge.
                if (pf_bone == 5'd1 && bone_count_i != 6'd0) begin
                  start_o <= 1'b1;
                  i0_q <= 5'd0;   // armed one cycle later, on `start_o` above
                end
              end
            end
            default: pf_q <= P_IDLE;
          endcase
        end
      end

    end else begin : g_async

      // ======================================================================
      // SRC_STYLE 1 and 2 — the asynchronous arrays. 1 stores the derived
      // record (320 bits/bone); 2 is R90's literal reading (576 bits/bone,
      // inv_rest carried in full). Both are read with the address the decoder
      // presents THIS cycle, which is what "combinational by contract" asks
      // for read naively.
      // ======================================================================
      localparam int AW = (SRC_STYLE == 2) ? FLAT_BITS : BODY_BITS;

      logic [AW-1:0]        abody [0:MAX_BONES-1];
      logic [QUAT_BITS-1:0] aquat [0:MAX_BONES-1];

      always_ff @(posedge clk) begin
        if (fill_we_i && !fill_sel_i) abody[fill_bone_i][{28'd0, fill_word_i} * 64 +: 64] <= fill_data_i;
        if (fill_we_i &&  fill_sel_i) aquat[fill_bone_i] <= fill_data_i;
      end

      // THE ASYNCHRONOUS READ. This is the line the price is about.
      assign body_sel = abody[bone_idx_i][BODY_BITS-1:0];
      assign quat_sel = aquat[bone_idx_i];

      // Style 2 only: the twelve elements come OUT OF THE STORE. Without this
      // the upper 320 bits are written and never read, Quartus deletes them,
      // and the row silently prices style 1 a second time.
      if (SRC_STYLE == 2) begin : g_inv_stored
        assign invrest_sel = abody[bone_idx_i][511:128];
      end

      assign ready_o = 1'b1;

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          start_o <= 1'b0;
          bone_prefetch_late_o <= '0;
        end else begin
          // Same guard as the sync style: a zero-bone request never starts.
          start_o <= req_i && (bone_count_i != 6'd0);
          // An async store is never late by construction; the counter exists so
          // the port list does not change with the style, and it is declared
          // dead here rather than left looking live.
          bone_prefetch_late_o <= bone_prefetch_late_o;
        end
      end

    end
  endgenerate

  // ==========================================================================
  // Detectors common to every style.
  // ==========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      bone_rest_nonrigid_o <= '0;
      bone_reserved_nz_o <= '0;
      bone_fills_o <= '0;
    end else begin
      if (fill_we_i) bone_fills_o <= bone_fills_o + 32'd1;
      // A body record whose word 0 arrives with RIGID_REST clear is claiming a
      // rest rotation this build cannot decode. Counted at the door rather than
      // decoded into a wrong palette.
      if (fill_we_i && !fill_sel_i && fill_word_i == 4'd0 && !fill_data_i[8])
        bone_rest_nonrigid_o <= bone_rest_nonrigid_o + 32'd1;
      // The record's reserved fields must be zero. Word 0 carries the parent
      // byte's high bits, flags 1..7 and the +2 u16; word 3 carries the +28
      // u32. A packer that starts using them without this build knowing is a
      // format change, and it moves a counter instead of being ignored.
      if (fill_we_i && !fill_sel_i && fill_word_i == 4'd0 &&
          (fill_data_i[7:5] != 3'd0 || fill_data_i[15:9] != 7'd0 || fill_data_i[31:16] != 16'd0))
        bone_reserved_nz_o <= bone_reserved_nz_o + 32'd1;
      if (fill_we_i && !fill_sel_i && fill_word_i == 4'd3 && fill_data_i[63:32] != 32'd0)
        bone_reserved_nz_o <= bone_reserved_nz_o + 32'd1;
    end
  end

endmodule : zhao_geom_bonesrc
