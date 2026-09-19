// zhao_part_table.sv -- PART.TABLE: the species descriptor table and the
// size/colour curve table. The owner that three already-built blocks read.
//
// Law, in citation order:
//   design/contracts/PART.UPDATE.md  -- "Reads the species table (on-chip,
//                                       loaded per frame, owned by PART.STATE)"
//   design/contracts/PART.COLLIDE.md -- "This block owns no memory"
//   design/contracts/PART.SPAWN.md   -- "The species table is read as a port"
//   design/contracts/PART.STATE.md   -- "Species descriptors are read-only and
//                                       small; they belong in an on-chip table
//                                       loaded per frame, not re-fetched per
//                                       particle."
//   Owner ruling 2026-09-18 (reports/OWNER-RULING-M10K-CEILINGS-20260918.md)
//   Owner ruling 2026-08-31 SS2.1 / amendment C2 -- the frozen record layout.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS: GAPS I2 AND I3, AND A TABLE WITH NO OWNER
// ---------------------------------------------------------------------------
// `fpga/rtl/prod/zhao_console_core.sv` says it plainly:
//
//   "I2. THE SPECIES DESCRIPTOR TABLE ... NO OWNER EXISTS IN THE TREE.
//    PART.COLLIDE's contract says 'this block owns no memory'; PART.UPDATE's
//    says the table 'belongs to PART.STATE'; and zhao_part_state.sv has no
//    descriptor port and no such memory. All three blocks read a table that
//    nothing implements."
//
// Four contracts each hand the table to somebody else and the chain closes on
// nobody. This block is that owner. It is a NEW block id, PART.TABLE, and not a
// growth of PART.STATE: PART.STATE is a DDR streamer whose contract caps it at
// 900 ALM / 4 M10K for staging, and its ports carry records, not descriptors.
//
// THE READ SHAPES ARE NOT NEGOTIATED HERE. Every output below is width-for-
// width and name-for-name the port the consumer already declares -- see the
// PORT MAP at the foot of this header. Three blocks are built, tested and
// frozen against these shapes; inventing a fourth and adapting to it is the
// composer-side arithmetic that zhao_console_core.sv exists to refuse.
//
// ---------------------------------------------------------------------------
// WHAT IS AUTHORED HERE, AND SAID SO
// ---------------------------------------------------------------------------
// `reference/include/zref/zref_particle.hpp` lines 50-55:
//
//   "there is no species table -- not in reference/, not in spec/, not in
//    design/. That is a DATA/ABI question and it is properly the owner's"
//
// So the CONTENTS of a descriptor are owner data and nothing here guesses one.
// What this file authors is only the CONTAINER:
//
//   * which fields a descriptor holds -- taken entirely from what the three
//     consumers already read, not from any new opinion about particles;
//   * the bit offsets of the load word (the LOAD WORD MAP below);
//   * the load handshake.
//
// Every one of those is a named, editable localparam. None is derived from a
// measurement and none is hidden. `base_radius_fx16`, which zref names as the
// missing per-species datum, is DELIBERATELY ABSENT: no built block reads it,
// so carrying it here would be inventing an ABI ahead of the owner.
//
// ---------------------------------------------------------------------------
// STORAGE: RAM, NOT FLIP-FLOPS -- AND THE ARITHMETIC, LABELLED AS ARITHMETIC
// ---------------------------------------------------------------------------
// Standing owner direction, 2026-09-18: "Using some more M10K is fine, we have
// enough, particularly if it saves ALMs. They're our only weapon against our
// massive ALM debt."
//
// EVERY NUMBER IN THIS SECTION IS SHAPE ARITHMETIC. Nothing here has been
// through `quartus_map` or a fit. Owner ruling 2026-09-18 point 3: "Logical
// bits are still not physical M10Ks ... The ruling authorises the trade; only a
// fit reports what it cost." Read these as the case for the arrangement, never
// as a measurement of it.
//
// WHAT IS STORED (SPECIES_N = 128, CRV_N = 16, the production tier):
//
//     update slice    141 b x 128 =  18,048 b
//     collide slice    51 b x 128 =   6,528 b
//     spawn rule       13 b x 512 =   6,656 b   (4 events per species)
//     curve entry      14 b x  16 =     224 b
//                                    ---------
//                                     31,456 b
//
// IF IT WERE FLIP-FLOPS -- which is what a naive descriptor table is, and the
// misplaced state the campaign is removing:
//
//     31,456 registers / 4 per ALM                     =  7,864 ALM
//     128:1 read mux x 141 b   (~43 ALM per bit)       =  6,063 ALM
//     128:1 read mux x  51 b                           =  2,193 ALM
//     512:1 read mux x  13 b   (~171 ALM per bit)      =  2,223 ALM
//      16:1 read mux x  14 b                           =     70 ALM
//                                                        ---------
//                                                        ~18,400 ALM
//
// AS SHIPPED -- MLAB (32 x 20 b per block, 10 ALM per block):
//
//     update   4 deep x 8 wide = 32 MLAB = 320 ALM, + 4:1 mux x 141 b =  141
//     collide  4 deep x 3 wide = 12 MLAB = 120 ALM, + 4:1 mux x  51 b =   51
//     spawn   16 deep x 1 wide = 16 MLAB = 160 ALM, + 16:1 mux x 13 b =   65
//     curve    1 block          =  1 MLAB =  10 ALM
//                                                        ---------
//                                                          ~870 ALM
//
// So the arrangement is worth roughly 17,500 ALM against the flip-flop table,
// by this arithmetic, at 0 M10K.
//
// ---------------------------------------------------------------------------
// WHY 0 M10K, WHEN THE DIRECTION SAYS SPEND M10K -- THE HONEST REASON
// ---------------------------------------------------------------------------
// Because every one of these four reads must answer IN THE SAME CYCLE as its
// index, and an M10K cannot do that. The Cyclone V M10K's address register is
// mandatory, so its minimum read latency is one clock. MLAB's read can be
// unregistered; M10K's cannot.
//
// The consumers say so themselves, and they are right to:
//
//   * PART.UPDATE: "what crosses here is the descriptor for spc_index_o,
//     combinationally, in the SAME cycle as in_record_i. There is no capture
//     register on this path and that is on purpose. A bank that registers its
//     read and a stage that holds its request move apart on a stall and deliver
//     one record's data with another's metadata." That is this repository's own
//     metadata-swap defect, and refusing to reintroduce it is correct.
//   * PART.UPDATE's CURVE index is `age_next_c[AGE_W-1 -: 4]` -- the ADVANCED
//     age, computed inside PART.UPDATE from the record. It does not exist one
//     cycle earlier anywhere, in any block.
//   * PART.SPAWN presents {par_q, ev_q} on entering S_EVAL and consumes the
//     reply in that same cycle. Reading it a cycle early would mean replicating
//     PART.SPAWN's event priority encoder here -- and that encoder IS its
//     determinism contract ("parent stream order -> event order -> child index").
//     A second copy of ratified arithmetic is the failure CLAUDE.md has a
//     chapter about.
//   * PART.COLLIDE emits no index at all: the descriptor is "sampled with the
//     particle", so the index is the species of the record on its input wire
//     this cycle.
//
// THE M10K VARIANT EXISTS AND IS PRICED, so the next reader does not have to
// re-derive it. Making this block a LOOKUP STAGE -- accepting the record from
// PART.STATE, registering it beside the RAM address under ONE enable, and
// offering {record, descriptor} together to PART.UPDATE, then the same again
// between PART.UPDATE and PART.COLLIDE -- gives the two big slices a cycle of
// notice and puts them in M10K:
//
//     update   128 x 141, M10K at x40 -> ceil(141/40) = 4 M10K
//     collide  128 x  51, M10K at x40 -> ceil( 51/40) = 2 M10K
//
// That removes ~630 ALM of MLAB and mux for 6 M10K: about 105 ALM per M10K,
// BELOW the owner's own ~200 ALM/M10K ranking bar. It also adds one clock of
// latency to the particle path and REWIRES PART.STATE -> PART.UPDATE ->
// PART.COLLIDE, which is a change to zhao_console_core.sv -- a composition act
// this packet is forbidden to perform. The spawn and curve slices cannot go to
// M10K under any arrangement, for the reasons above.
//
// Recorded as a decision, not an omission: the cheap 17,500 ALM is taken here
// and the expensive 630 is left on the table with its prerequisite named.
//
// ---------------------------------------------------------------------------
// `ramstyle` IS A HINT, AND A HINT IS NOT AN INFERENCE
// ---------------------------------------------------------------------------
// The four arrays carry `(* ramstyle = "MLAB, no_rw_check" *)`.
// Quartus may decline. If it does, the arrays become the ~18,400-ALM flip-flop
// table above and the block blows every ceiling in sight -- which is exactly
// the kind of thing that must be MEASURED rather than assumed. Until a fit
// reports the MLAB count, the ALM figures in this header are arithmetic.
//
// `no_rw_check` is correct here and not a shortcut: there is no simultaneous
// load and read of the same address in any legal schedule, because the table is
// loaded between ticks and read during them. If that ever stops being true the
// attribute must go, and the read-during-write behaviour becomes a contract.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK DELIBERATELY DOES NOT DO
// ---------------------------------------------------------------------------
//  1. IT DOES NOT VALIDATE A DESCRIPTOR IT SERVES. PART.UPDATE refuses an
//     out-of-vocabulary recipe, PART.SPAWN refuses a count above 16 and an
//     out-of-range child species, PART.STATE refuses an out-of-range species.
//     Three blocks already own those refusals, with counters and tests. A
//     fourth opinion here would be a second implementation that can disagree.
//     This block refuses exactly one thing -- a LOAD outside the table -- and
//     that is a refusal nobody else can make.
//  2. IT HAS NO BLANKET CLEAR. `known` is bit 12 of the spawn word, so a
//     species/event with no rule is loaded with known = 0 like any other datum.
//     A clearable 512-entry presence vector would be 512 flip-flops and roughly
//     300 ALM by the arithmetic above -- spending the ALMs this block exists to
//     save, to spare the host 512 writes per frame. The load discipline is the
//     host's: a partial load leaves the previous frame's rules in place, and
//     that is stated rather than silently guarded.
//  3. IT HAS NO READ-VALID SIDE CHANNEL for update, collide or curve. Neither
//     contract has one, so there is nothing to connect it to. An entry never
//     loaded reads as whatever the array holds -- X in simulation, zero on
//     Cyclone V power-up. PART.SPAWN's `known` is the one place a contract
//     asked for the question and it is answered there.
//
// ---------------------------------------------------------------------------
// PORT MAP -- what the composer connects, consumer port by consumer port
// ---------------------------------------------------------------------------
//   zhao_part_update            zhao_part_table
//     spc_index_o        ->       u_index_i
//     spc_recipe_i       <-       u_recipe_o
//     spc_lifetime_i     <-       u_lifetime_o
//     spc_age_mark_i     <-       u_age_mark_o
//     spc_drag_i         <-       u_drag_o
//     spc_grav_i         <-       u_grav_o
//     spc_strength_i     <-       u_strength_o
//     spc_cx_i           <-       u_cx_o
//     spc_cy_i           <-       u_cy_o
//     spc_cz_i           <-       u_cz_o
//     spc_p0_i           <-       u_p0_o
//     spc_p1_i           <-       u_p1_o
//     spc_p2_i           <-       u_p2_o
//     crv_index_o        ->       v_index_i
//     crv_size_i         <-       v_size_o
//     crv_colour_i       <-       v_colour_o
//
//   zhao_part_collide           zhao_part_table
//     (no index port)    ->       c_index_i  = p_record_i[97 +: 7]
//     d_response_i       <-       c_response_o
//     d_restitution_i    <-       c_restitution_o
//     d_friction_i       <-       c_friction_o
//     d_damping_i        <-       c_damping_o
//
//   zhao_part_spawn             zhao_part_table
//     spc_species_o      ->       s_species_i
//     spc_event_o        ->       s_event_i
//     spc_known_i        <-       s_known_o
//     spc_child_spc_i    <-       s_child_spc_o
//     spc_count_i        <-       s_count_o
//
// PART.COLLIDE's index is the ONE place the composer must do something rather
// than connect something, and it is a field select from the frozen layout, not
// arithmetic: `zhao_part_record`'s `species_o` off the same `p_record_i` the
// collide instance is being offered. Named here so it is a wiring instruction
// and not an invention left to whoever composes.
//
// ---------------------------------------------------------------------------
// SYNTHESIZABILITY
// ---------------------------------------------------------------------------
// This file has NOT been through `quartus_map`. Verilator lint-clean is one
// tool's opinion and is not evidence of synthesizability (CLAUDE.md). The four
// Quartus 17.0 forms this repository has been bitten by are respected on
// purpose: elaboration checks live inside `initial begin ... end`, there is no
// implicit generate and no loop-generate at all, and no unary minus is applied
// to a size cast.
//
// Conservative SystemVerilog subset only (charter SS2).
`default_nettype none

module zhao_part_table #(
    // The table's depth. 7 bits of `species` address 128 (amendment C2), so 128
    // is the ceiling and not a choice. SMALLER MAKES THE LOAD REFUSAL
    // REACHABLE, which at 128 it is not -- the same reason PART.STATE's bench
    // runs at SPECIES_N = 4.
    parameter int unsigned SPECIES_N = 128,

    // Curve buckets. PART.UPDATE emits a 4-bit `crv_index_o`, so 16 is the
    // ceiling for the same reason.
    parameter int unsigned CRV_N = 16,

    // The consumers' field widths. Parameters rather than literals so the
    // elaboration guard below has something to check -- NOT so they can be
    // changed: POS_W and VEL_W are frozen by amendment C2 / ruling R3, and
    // AGE_W and FX_W are the widths zhao_part_update.sv and
    // zhao_part_collide.sv already declare on the ports this block drives.
    parameter int unsigned AGE_W = 10,
    parameter int unsigned POS_W = 18,
    parameter int unsigned VEL_W = 11,
    parameter int unsigned FX_W  = 16,

    // DERIVED, NOT A KNOB. The load bus is the widest slice, which is the
    // update descriptor: recipe 4 + lifetime + age_mark + drag 8 + grav +
    // strength + cx,cy,cz + p0,p1,p2. It lives in the parameter list only
    // because a port width cannot refer to a body localparam, and the
    // elaboration guard below requires it to equal UPD_W -- so an override
    // fails loudly instead of truncating a descriptor.
    parameter int unsigned LD_W = 12 + (2 * AGE_W) + (5 * VEL_W) + (3 * POS_W)
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the per-frame load ---------------------------------------------------
    // One word per clock, never refused for backpressure: `ld_ready_o` is
    // constant high and says so. The only refusal is an index outside the
    // table, which is a fault and is counted.
    input  wire                 ld_valid_i,
    output wire                 ld_ready_o,
    input  wire [1:0]           ld_sel_i,    // LD_UPD / LD_COL / LD_SPW / LD_CRV
    input  wire [6:0]           ld_index_i,  // species, or curve bucket in [3:0]
    input  wire [1:0]           ld_event_i,  // LD_SPW only: 0 birth 1 mark 2 collision 3 death
    input  wire [LD_W-1:0]      ld_data_i,   // the selected slice, LSB-aligned

    // ---- PART.UPDATE's species descriptor (combinational, same cycle) --------
    input  wire [6:0]           u_index_i,
    output wire [3:0]           u_recipe_o,
    output wire [AGE_W-1:0]     u_lifetime_o,
    output wire [AGE_W-1:0]     u_age_mark_o,
    output wire [7:0]           u_drag_o,
    output wire signed [VEL_W-1:0] u_grav_o,
    output wire signed [VEL_W-1:0] u_strength_o,
    output wire signed [POS_W-1:0] u_cx_o,
    output wire signed [POS_W-1:0] u_cy_o,
    output wire signed [POS_W-1:0] u_cz_o,
    output wire signed [VEL_W-1:0] u_p0_o,
    output wire signed [VEL_W-1:0] u_p1_o,
    output wire signed [VEL_W-1:0] u_p2_o,

    // ---- PART.UPDATE's size/colour curve (combinational, same cycle) ---------
    input  wire [3:0]           v_index_i,
    output wire [5:0]           v_size_o,
    output wire [7:0]           v_colour_o,

    // ---- PART.COLLIDE's descriptor slice (combinational, same cycle) ---------
    input  wire [6:0]           c_index_i,
    output wire [2:0]           c_response_o,
    output wire signed [FX_W-1:0] c_restitution_o,
    output wire signed [FX_W-1:0] c_friction_o,
    output wire signed [FX_W-1:0] c_damping_o,

    // ---- PART.SPAWN's child rule (combinational, same cycle) -----------------
    input  wire [6:0]           s_species_i,
    input  wire [1:0]           s_event_i,
    output wire                 s_known_o,
    output wire [6:0]           s_child_spc_o,
    output wire [4:0]           s_count_o,

    // ---- counters. A counter asserted zero and never seen to move is a claim;
    // ---- every one of these is fired by tests/particles/part_table_directed.cpp
    // ---- as a DELTA, not as a final total.
    output var logic [31:0]     loads_update_o,
    output var logic [31:0]     loads_collide_o,
    output var logic [31:0]     loads_spawn_o,
    output var logic [31:0]     loads_curve_o,
    output var logic [31:0]     load_refused_o
);

  // ---- the load selector ----------------------------------------------------
  localparam logic [1:0] LD_UPD = 2'd0;
  localparam logic [1:0] LD_COL = 2'd1;
  localparam logic [1:0] LD_SPW = 2'd2;
  localparam logic [1:0] LD_CRV = 2'd3;

  // ---- LOAD WORD MAP --------------------------------------------------------
  // AUTHORED HERE (see the header): no ratified layout for a descriptor exists.
  // Stated as running offsets so a width change re-tiles the word instead of
  // silently overlapping two fields, and so the elaboration guard below is
  // arithmetic over them rather than a comment about them.
  //
  //   update slice, LSB first:
  //     recipe 4 | lifetime AGE_W | age_mark AGE_W | drag 8 | grav VEL_W |
  //     strength VEL_W | cx POS_W | cy POS_W | cz POS_W | p0 p1 p2 VEL_W each
  localparam int unsigned U_W_RCP = 4;
  localparam int unsigned U_W_DRG = 8;

  localparam int unsigned U_OFF_RCP = 0;
  localparam int unsigned U_OFF_LIF = U_OFF_RCP + U_W_RCP;
  localparam int unsigned U_OFF_MRK = U_OFF_LIF + AGE_W;
  localparam int unsigned U_OFF_DRG = U_OFF_MRK + AGE_W;
  localparam int unsigned U_OFF_GRV = U_OFF_DRG + U_W_DRG;
  localparam int unsigned U_OFF_STR = U_OFF_GRV + VEL_W;
  localparam int unsigned U_OFF_CX  = U_OFF_STR + VEL_W;
  localparam int unsigned U_OFF_CY  = U_OFF_CX + POS_W;
  localparam int unsigned U_OFF_CZ  = U_OFF_CY + POS_W;
  localparam int unsigned U_OFF_P0  = U_OFF_CZ + POS_W;
  localparam int unsigned U_OFF_P1  = U_OFF_P0 + VEL_W;
  localparam int unsigned U_OFF_P2  = U_OFF_P1 + VEL_W;
  localparam int unsigned UPD_W     = U_OFF_P2 + VEL_W;   // 141 at the defaults

  //   collide slice: response 3 | restitution FX_W | friction FX_W | damping FX_W
  localparam int unsigned C_W_RSP   = 3;
  localparam int unsigned C_OFF_RSP = 0;
  localparam int unsigned C_OFF_RES = C_OFF_RSP + C_W_RSP;
  localparam int unsigned C_OFF_FRI = C_OFF_RES + FX_W;
  localparam int unsigned C_OFF_DMP = C_OFF_FRI + FX_W;
  localparam int unsigned COLD_W    = C_OFF_DMP + FX_W;   // 51 at the defaults

  //   spawn rule: child species 7 | count 5 | known 1
  localparam int unsigned S_W_CHD   = 7;
  localparam int unsigned S_W_CNT   = 5;
  localparam int unsigned S_OFF_CHD = 0;
  localparam int unsigned S_OFF_CNT = S_OFF_CHD + S_W_CHD;
  localparam int unsigned S_OFF_KNW = S_OFF_CNT + S_W_CNT;
  localparam int unsigned SPWD_W    = S_OFF_KNW + 1;      // 13

  //   curve entry: size 6 | colour 8
  localparam int unsigned V_W_SIZ   = 6;
  localparam int unsigned V_W_CLR   = 8;
  localparam int unsigned V_OFF_SIZ = 0;
  localparam int unsigned V_OFF_CLR = V_OFF_SIZ + V_W_SIZ;
  localparam int unsigned CRVD_W    = V_OFF_CLR + V_W_CLR;  // 14

  // ---- addressing -----------------------------------------------------------
  localparam int unsigned SPC_AW = (SPECIES_N <= 1) ? 1 : $clog2(SPECIES_N);
  localparam int unsigned CRV_AW = (CRV_N <= 1) ? 1 : $clog2(CRV_N);
  localparam int unsigned SPW_AW = SPC_AW + 2;
  localparam int unsigned SPW_N  = SPECIES_N * 4;

  // TRUE only when the table is smaller than the field that addresses it, which
  // is the only case in which an index can be out of range at all. Written as a
  // gate rather than relying on the comparison folding, exactly as
  // zhao_part_state.sv does for the same question.
  localparam bit SPC_PARTIAL = (SPECIES_N < 128);
  localparam bit CRV_PARTIAL = (CRV_N < 16);

  // ---- elaboration guards ---------------------------------------------------
  // Quartus 17.0 needs these inside `initial begin ... end` -- a bare
  // module-scope `if` is a syntax error there however clean the lint. And
  // --lint-only does not run initial blocks, so a clean lint says nothing
  // whatever about these firing.
  initial begin
    if (SPECIES_N < 1 || SPECIES_N > 128)
      $fatal(1, "zhao_part_table: SPECIES_N=%0d outside 1..128; the species field is 7 bits (amendment C2)",
             SPECIES_N);
    if (CRV_N < 1 || CRV_N > 16)
      $fatal(1, "zhao_part_table: CRV_N=%0d outside 1..16; PART.UPDATE's crv_index_o is 4 bits", CRV_N);
    if (POS_W != 18)
      $fatal(1, "zhao_part_table: POS_W=%0d; position is s18, FROZEN by amendment C2 / ruling R3", POS_W);
    if (VEL_W != 11)
      $fatal(1, "zhao_part_table: VEL_W=%0d; velocity is s11, FROZEN by amendment C2 / ruling R3", VEL_W);
    // The load bus must be able to carry every slice. If a width change ever
    // makes another slice the widest, or LD_W is overridden, this fails at
    // elaboration instead of truncating a descriptor in silence.
    if (LD_W != UPD_W)
      $fatal(1, "zhao_part_table: LD_W=%0d is DERIVED and must equal UPD_W=%0d", LD_W, UPD_W);
    if (COLD_W > LD_W || SPWD_W > LD_W || CRVD_W > LD_W)
      $fatal(1, "zhao_part_table: LD_W=%0d cannot carry every slice (upd=%0d col=%0d spw=%0d crv=%0d)",
             LD_W, UPD_W, COLD_W, SPWD_W, CRVD_W);
  end

  // ---- the arrays -----------------------------------------------------------
  // RAM, not registers: see the header's arithmetic. The ramstyle is a HINT and
  // Quartus may decline it; only a fit says what these became.
  // The ATTRIBUTE form, not the `/* synthesis ... */` comment form, because the
  // attribute form is the one this repository has already put through Quartus:
  // `fpga/rtl/field/zhao_field_progdir_scan.sv` carries
  // `(* ramstyle = "M10K" *)` on exactly this kind of declaration. Choosing the
  // proven spelling over the equivalent one costs nothing and removes a way for
  // a first `quartus_map` to fail on syntax rather than on substance.
  (* ramstyle = "MLAB, no_rw_check" *) logic [UPD_W-1:0]  upd_m[SPECIES_N];
  (* ramstyle = "MLAB, no_rw_check" *) logic [COLD_W-1:0] col_m[SPECIES_N];
  (* ramstyle = "MLAB, no_rw_check" *) logic [SPWD_W-1:0] spw_m[SPW_N];
  (* ramstyle = "MLAB, no_rw_check" *) logic [CRVD_W-1:0] crv_m[CRV_N];

  // ---- the load -------------------------------------------------------------
  // Constant ready. A table that can stall its own load is a table whose
  // contents depend on when the host wrote them, and there is nothing here that
  // could ever be busy: one word, one clock, one array.
  assign ld_ready_o = 1'b1;

  wire ld_fire_c = ld_valid_i && ld_ready_o;

  // The one refusal this block owns. Every other refusal belongs to a consumer
  // that already has it, with its own counter and its own test.
  wire ld_spc_oor_c = SPC_PARTIAL && ({25'd0, ld_index_i} >= 32'(SPECIES_N));
  wire ld_crv_oor_c = CRV_PARTIAL && ({28'd0, ld_index_i[3:0]} >= 32'(CRV_N));
  wire ld_oor_c     = (ld_sel_i == LD_CRV) ? ld_crv_oor_c : ld_spc_oor_c;
  wire ld_ok_c      = ld_fire_c && !ld_oor_c;

  wire [SPC_AW-1:0] ld_spc_a_c = ld_index_i[SPC_AW-1:0];
  wire [CRV_AW-1:0] ld_crv_a_c = ld_index_i[CRV_AW-1:0];
  wire [SPW_AW-1:0] ld_spw_a_c = {ld_spc_a_c, ld_event_i};

  // The arrays are written in their OWN always_ff with no reset. A reset arm
  // over a memory is how RAM inference is lost -- and there is nothing to
  // reset: the table is content, loaded per frame, and a power-up value it
  // never had is not a state anyone should rely on.
  always_ff @(posedge clk) begin
    if (ld_ok_c) begin
      case (ld_sel_i)
        LD_UPD: upd_m[ld_spc_a_c] <= ld_data_i[UPD_W-1:0];
        LD_COL: col_m[ld_spc_a_c] <= ld_data_i[COLD_W-1:0];
        LD_SPW: spw_m[ld_spw_a_c] <= ld_data_i[SPWD_W-1:0];
        LD_CRV: crv_m[ld_crv_a_c] <= ld_data_i[CRVD_W-1:0];
      endcase
    end
  end

  // ---- counters -------------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      loads_update_o  <= 32'd0;
      loads_collide_o <= 32'd0;
      loads_spawn_o   <= 32'd0;
      loads_curve_o   <= 32'd0;
      load_refused_o  <= 32'd0;
    end else begin
      if (ld_fire_c && ld_oor_c) load_refused_o <= load_refused_o + 32'd1;
      if (ld_ok_c) begin
        case (ld_sel_i)
          LD_UPD: loads_update_o <= loads_update_o + 32'd1;
          LD_COL: loads_collide_o <= loads_collide_o + 32'd1;
          LD_SPW: loads_spawn_o <= loads_spawn_o + 32'd1;
          LD_CRV: loads_curve_o <= loads_curve_o + 32'd1;
        endcase
      end
    end
  end

  // ---- the reads ------------------------------------------------------------
  // Asynchronous, by contract. See the header for why an M10K cannot serve
  // these and what the alternative would cost.
  //
  // An index outside the table reads as ZERO rather than as a wrapped entry.
  // Wrapping would hand a consumer a REAL descriptor belonging to a different
  // species, which is the worst answer available: it looks correct. Zero is a
  // recipe of 0 (HOLD), a lifetime of 0 (unbounded), a count of 0 and
  // known = 0, and every consumer already refuses on its own terms.
  wire [SPC_AW-1:0] u_a_c = u_index_i[SPC_AW-1:0];
  wire [SPC_AW-1:0] c_a_c = c_index_i[SPC_AW-1:0];
  wire [CRV_AW-1:0] v_a_c = v_index_i[CRV_AW-1:0];
  wire [SPW_AW-1:0] s_a_c = {s_species_i[SPC_AW-1:0], s_event_i};

  wire u_oor_c = SPC_PARTIAL && ({25'd0, u_index_i} >= 32'(SPECIES_N));
  wire c_oor_c = SPC_PARTIAL && ({25'd0, c_index_i} >= 32'(SPECIES_N));
  wire v_oor_c = CRV_PARTIAL && ({28'd0, v_index_i} >= 32'(CRV_N));
  wire s_oor_c = SPC_PARTIAL && ({25'd0, s_species_i} >= 32'(SPECIES_N));

  wire [UPD_W-1:0]  u_w_c = u_oor_c ? {UPD_W{1'b0}} : upd_m[u_a_c];
  wire [COLD_W-1:0] c_w_c = c_oor_c ? {COLD_W{1'b0}} : col_m[c_a_c];
  wire [CRVD_W-1:0] v_w_c = v_oor_c ? {CRVD_W{1'b0}} : crv_m[v_a_c];
  wire [SPWD_W-1:0] s_w_c = s_oor_c ? {SPWD_W{1'b0}} : spw_m[s_a_c];

  assign u_recipe_o   = u_w_c[U_OFF_RCP+:U_W_RCP];
  assign u_lifetime_o = u_w_c[U_OFF_LIF+:AGE_W];
  assign u_age_mark_o = u_w_c[U_OFF_MRK+:AGE_W];
  assign u_drag_o     = u_w_c[U_OFF_DRG+:U_W_DRG];
  assign u_grav_o     = $signed(u_w_c[U_OFF_GRV+:VEL_W]);
  assign u_strength_o = $signed(u_w_c[U_OFF_STR+:VEL_W]);
  assign u_cx_o       = $signed(u_w_c[U_OFF_CX+:POS_W]);
  assign u_cy_o       = $signed(u_w_c[U_OFF_CY+:POS_W]);
  assign u_cz_o       = $signed(u_w_c[U_OFF_CZ+:POS_W]);
  assign u_p0_o       = $signed(u_w_c[U_OFF_P0+:VEL_W]);
  assign u_p1_o       = $signed(u_w_c[U_OFF_P1+:VEL_W]);
  assign u_p2_o       = $signed(u_w_c[U_OFF_P2+:VEL_W]);

  assign c_response_o    = c_w_c[C_OFF_RSP+:C_W_RSP];
  assign c_restitution_o = $signed(c_w_c[C_OFF_RES+:FX_W]);
  assign c_friction_o    = $signed(c_w_c[C_OFF_FRI+:FX_W]);
  assign c_damping_o     = $signed(c_w_c[C_OFF_DMP+:FX_W]);

  assign v_size_o   = v_w_c[V_OFF_SIZ+:V_W_SIZ];
  assign v_colour_o = v_w_c[V_OFF_CLR+:V_W_CLR];

  assign s_child_spc_o = s_w_c[S_OFF_CHD+:S_W_CHD];
  assign s_count_o     = s_w_c[S_OFF_CNT+:S_W_CNT];
  assign s_known_o     = s_w_c[S_OFF_KNW];

endmodule : zhao_part_table

`default_nettype wire
