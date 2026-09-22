// zhao_twod_cmd.sv -- the TWOD descriptor producer: records in, a SEALED
// per-frame list out.
//
// ===========================================================================
// WHAT THIS BLOCK IS FOR, IN THE RULING'S OWN WORDS
// ===========================================================================
// Owner completion ruling 2026-09-22, item 3 (ratified in
// `reports/OWNER-RATIFICATION-20260922-COMPLETION.md`):
//
//   "Descriptors are frame-scoped. STAGE AND VALIDATE the frame's descriptors,
//    then publish a SEALED LIST at the boundary that owns that frame, BEFORE
//    its TWOD pass. NEITHER A LATER PACKET NOR THE NEXT FRAME MAY MUTATE THE
//    LIST BEING CONSUMED. Preserve deterministic order, define ties by command
//    order, and provide explicit plane disable behavior and an empty-frame path
//    that cannot retain old HUD contents."
//
// Before this block existed, `zhao_console_core.sv`'s `twod_pd_*` and
// `twod_sd_*` were nineteen and twenty ports hanging at the console's edge with
// no producer inside the chip. `zhao_twod_band` was built, composed, and
// PERMANENTLY IDLE: nothing could tell it what to draw.
//
// ===========================================================================
// WHY THE SEAL EDGE COMES FROM TWOD.BAND AND NOT FROM THE FRAME TICK
// ===========================================================================
// THIS IS THE WHOLE HAZARD OF A FRAME-SCOPED LIST, and this repository has a
// name for the way it goes wrong: I39, "two live wires are not a producer". A
// lane joined `cmd_exec`'s per-PACKET publish to `terrain_patch`'s per-JOB
// clear and filled the list for the first job only -- silently, with every
// counter balancing.
//
// `zhao_twod_band` clears its display list on its own `restart_c`, which is
//
//     (frame_start_i && !sweeping_q) || (sweep_sync_c && !armed_q)
//
// and NOT on `frame_start_i`. A tick landing mid-pass is DELIBERATELY IGNORED
// there -- ignoring it is what fixed the composed console's 582,261 underruns.
// So a producer that sealed on the tick would, on exactly those frames, write
// the new frame's descriptors into the OLD frame's list, which is the mutation
// the ruling forbids, and every counter on both sides would still balance.
//
// The band therefore EXPORTS that cycle as `list_restart_o` and this block
// consumes it as `seal_i`. One signal, one meaning, one consumer, nothing to
// disagree with. The band's `desc_mid_sweep_o` is the independent check that it
// worked: its two operands are this block's replay walk and the compositor's
// read sweep, which nothing clocks together.
//
// ===========================================================================
// ONE RING, TWO RECORD KINDS, AND WHY THAT IS THE CHEAP SHAPE
// ===========================================================================
// SetPlane and DrawSprite stage into the SAME ring, tagged. The first design
// gave the plane its own staged and committed register pair -- 2 slots x 2
// copies x 303 bits = 1,212 flip-flops, about 1,030 ALMs by this console's own
// measured 0.849 ALM/register -- purely so a plane record could be rolled back
// with its packet. Putting it in the ring makes the rollback THE SAME POINTER
// ARITHMETIC the sprites already need, and the flops go away entirely.
//
// The ring is 2 * MAX_DESC entries and the pointers are:
//
//     wp          next write; every accepted record lands here
//     cp          commit pointer; a packet's verdict moves it or rewinds wp
//     seal_end    the last sealed frame's end -- also the CURRENT frame's start
//     seal_start  the window being published, [seal_start, seal_start+seal_len)
//
// A FRAME'S LIST IS CAPPED at MAX_DESC by `wp - seal_end >= MAX_DESC`, which is
// the TWOD.SPRITE.md budget rule -- "drop the tail, deterministically by
// `order`, and count -- THE HUD MUST NOT FAULT A FRAME". The handshake always
// completes; lowering `ready` would backpressure the command stream for the
// least important thing on the screen, and would also make the drop
// uncountable, because a held offer and a new offer are indistinguishable on
// the wire. That sentence is `zhao_twod_band`'s, at the same door.
//
// THE RING CANNOT WRAP INTO THE WINDOW BEING PUBLISHED, and this is an
// arithmetic fact rather than a guard: `wp - seal_end < MAX_DESC` by the cap,
// `seal_end - seal_start = seal_len <= MAX_DESC` by the same cap one frame
// earlier, so `wp - seal_start <= 2*MAX_DESC` -- the ring's exact size. There
// is deliberately NO wrap counter here, because it could never move: a counter
// that cannot fire is not evidence, and the cap's counter (`list_overflow_o`)
// is reachable with legal stimulus and is fired by the directed bench.
//
// A PACKET IN FLIGHT AT THE SEAL IS NOT SPLIT. Its staged records sit at
// `cp .. wp`, beyond the sealed window, and commit into the NEXT frame whole.
// That falls out of the ring; the abandoned alternative -- two half-lists
// ping-ponged -- had to discard them and count it.
//
// ===========================================================================
// THE PLANE IS FRAME-SCOPED TOO, AND THAT IS THE CONTRACT'S OWN SENTENCE
// ===========================================================================
// `TWOD.PLANE.md`: "no descriptor state survives a reset, BECAUSE DESCRIPTORS
// ARE RE-SENT PER FRAME." So a slot the frame's committed packets never named
// is DISABLED at the seal, on the lawful `d_enable_i` path added to
// `zhao_twod_plane` for this, counted on `slots_auto_disabled_o` and NOT on
// either refusal counter. That is the ruling's "explicit plane disable
// behavior", and with the band's own generation tag clearing the HUD it is also
// the "empty-frame path that cannot retain old HUD contents": a frame carrying
// no records publishes two disables and an empty list, and the screen shows the
// world.
//
// PLANES ARE PUBLISHED BEFORE SPRITES, in two passes over the same window, and
// the reason is a race rather than tidiness. `zhao_twod_sampler` begins
// prefilling atmosphere lines at the frame tick; a line is at least LINE_W
// clocks. Pass 1 visits every entry at two clocks each, so the plane pair is
// programmed within `2*MAX_DESC + 4` clocks of the seal -- 132 at the shipped
// MAX_DESC = 64 -- against 384 for the sampler's first line. A single
// interleaved pass would have put a SetPlane sitting at entry 63 after the
// sampler had already filled most of a line from the previous frame's sky.
//
// ===========================================================================
// WHAT THIS BLOCK DOES NOT DO
// ===========================================================================
// It does not judge a VALUE the consumer owns. A role of 2, a BACKDROP asking
// for ALPHA, a zero-width sprite and an unknown format are all refused by
// `zhao_twod_plane`, `zhao_twod_sprite` and `zhao_twod_sampler` on their own
// counters, and the ruling requires those rules be retained. What is refused
// HERE is only what cannot be REPRESENTED downstream -- a slot index above 1, a
// value too wide for the port it must travel on, a reserved bit set, a page
// base outside the store. Those are counted separately from the consumers'
// refusals so the two can never be read as each other.
//
// It contains no font engine, no text layout, no glyph cache and no sampler.
// Text is glyph sprites and the game authors the layout; the ruling is explicit
// and so is `TWOD.SPRITE.md`.

`default_nettype none

module zhao_twod_cmd #(
    // MUST equal `zhao_twod_band`'s MAX_DESC: it is the same budget, and the
    // band's `desc_overflow_o` would otherwise fire on a list this block
    // believed it had already capped. Checked at elaboration by the composer.
    parameter int unsigned MAX_DESC   = 64,
    parameter int unsigned UVW        = 32,
    // The TWOD sampler's page store, in 16-bit words, and its binding count.
    // A `base` outside the store is refused rather than wrapped.
    parameter int unsigned PAGE_WORDS = 8192,
    parameter int unsigned BIND_SLOTS = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- SetPlane 0x0306, from CMD.EXEC's staging arm -----------------------
    // Presented ONCE per record, at the record's end, in command order. The
    // fields are the generated record's widths, not the consumer's: narrowing
    // is this block's job and is counted.
    input  var logic                    pl_valid_i,
    output var logic                    pl_ready_o,
    input  var logic [7:0]              pl_slot_i,
    input  var logic [7:0]              pl_role_i,
    input  var logic [7:0]              pl_blend_i,
    input  var logic [7:0]              pl_opacity_i,
    input  var logic [7:0]              pl_format_i,
    input  var logic [7:0]              pl_wrap_i,       // b0 wrap_u, b1 wrap_v
    input  var logic [7:0]              pl_view_mask_i,
    input  var logic [7:0]              pl_palette_i,
    input  var logic [15:0]             pl_width_i,
    input  var logic [15:0]             pl_height_i,
    input  var logic [15:0]             pl_flags_i,      // b0 ENABLE
    input  var logic [15:0]             pl_base_i,
    input  var logic [7:0]              pl_lstride_i,
    input  var logic [7:0]              pl_lheight_i,
    input  var logic signed [UVW-1:0]   pl_a_i,
    input  var logic signed [UVW-1:0]   pl_b_i,
    input  var logic signed [UVW-1:0]   pl_c_i,
    input  var logic signed [UVW-1:0]   pl_d_i,
    input  var logic signed [UVW-1:0]   pl_u0_i,
    input  var logic signed [UVW-1:0]   pl_v0_i,
    input  var logic signed [UVW-1:0]   pl_line_scroll_i,

    // ---- DrawSprite 0x0307, from CMD.EXEC's staging arm ---------------------
    input  var logic                    sp_valid_i,
    output var logic                    sp_ready_o,
    input  var logic signed [15:0]      sp_x_i,
    input  var logic signed [15:0]      sp_y_i,
    input  var logic [15:0]             sp_w_i,
    input  var logic [15:0]             sp_h_i,
    input  var logic [15:0]             sp_base_i,
    input  var logic [7:0]              sp_lstride_i,
    input  var logic [7:0]              sp_lheight_i,
    input  var logic [7:0]              sp_format_i,
    input  var logic [7:0]              sp_palette_i,
    input  var logic [7:0]              sp_blend_i,
    input  var logic [7:0]              sp_view_mask_i,
    input  var logic [15:0]             sp_tint_i,
    input  var logic [7:0]              sp_order_i,
    input  var logic [7:0]              sp_flags_i,      // reserved, must be 0
    input  var logic [15:0]             sp_src_id_i,
    input  var logic signed [UVW-1:0]   sp_u_i,
    input  var logic signed [UVW-1:0]   sp_v_i,
    input  var logic signed [UVW-1:0]   sp_a00_i,
    input  var logic signed [UVW-1:0]   sp_a01_i,
    input  var logic signed [UVW-1:0]   sp_a10_i,
    input  var logic signed [UVW-1:0]   sp_a11_i,

    // ---- the packet verdict, from CMD.EXEC ----------------------------------
    // ONE of these pulses per packet CMD.EXEC walked. Commit moves `cp` to
    // `wp`; abandon rewinds `wp` to `cp`. An abandoned packet's descriptors
    // never reach a frame, which is the same atomicity every other arm of that
    // block has, expressed with the same two pointers the forge arm uses.
    input  var logic                    pkt_commit_i,
    input  var logic                    pkt_abandon_i,

    // ---- the seal edge, from zhao_twod_band.list_restart_o ------------------
    input  var logic                    seal_i,

    // ---- the published plane descriptor, to zhao_twod_plane.d_* -------------
    output var logic                    d_valid_o,
    input  var logic                    d_ready_i,
    output var logic                    d_slot_o,
    output var logic                    d_enable_o,
    output var logic [1:0]              d_role_o,
    output var logic [1:0]              d_blend_o,
    output var logic [7:0]              d_opacity_o,
    output var logic                    d_format_o,
    output var logic [15:0]             d_width_o,
    output var logic [15:0]             d_height_o,
    output var logic                    d_wrap_u_o,
    output var logic                    d_wrap_v_o,
    output var logic signed [UVW-1:0]   d_a_o,
    output var logic signed [UVW-1:0]   d_b_o,
    output var logic signed [UVW-1:0]   d_c_o,
    output var logic signed [UVW-1:0]   d_d_o,
    output var logic signed [UVW-1:0]   d_u0_o,
    output var logic signed [UVW-1:0]   d_v0_o,
    output var logic [1:0]              d_view_mask_o,
    output var logic [7:0]              d_palette_o,

    // ---- the published sprite list, to zhao_twod_band.d_* -------------------
    output var logic                    s_valid_o,
    input  var logic                    s_ready_i,
    output var logic signed [15:0]      s_x_o,
    output var logic signed [15:0]      s_y_o,
    output var logic [15:0]             s_w_o,
    output var logic [15:0]             s_h_o,
    output var logic signed [UVW-1:0]   s_u_o,
    output var logic signed [UVW-1:0]   s_v_o,
    output var logic signed [UVW-1:0]   s_a00_o,
    output var logic signed [UVW-1:0]   s_a01_o,
    output var logic signed [UVW-1:0]   s_a10_o,
    output var logic signed [UVW-1:0]   s_a11_o,
    output var logic [2:0]              s_format_o,
    output var logic [7:0]              s_palette_o,
    output var logic [15:0]             s_tint_o,
    output var logic [1:0]              s_blend_o,
    output var logic [1:0]              s_view_mask_o,
    output var logic [7:0]              s_order_o,
    output var logic [15:0]             s_src_id_o,

    // ---- the sampler's binding writes ---------------------------------------
    // A descriptor names WHERE its texels are; the asset loader puts bytes
    // there. Slot selection is the sampler's own published convention, not one
    // invented here: 0..1 are the plane's two ROLES, 4..7 the sprite's low two
    // `src_id` bits.
    output var logic                            ld_bind_we_o,
    output var logic [$clog2(BIND_SLOTS)-1:0]   ld_bind_sel_o,
    output var logic [$clog2(PAGE_WORDS)-1:0]   ld_bind_base_o,
    output var logic [3:0]                      ld_bind_lstride_o,
    output var logic [3:0]                      ld_bind_lheight_o,

    // ---- the publish walk, to zhao_twod_band.list_busy_i --------------------
    // HIGH FROM THE SEAL UNTIL THE LAST DESCRIPTOR HAS LANDED. The band clears
    // its list on the same cycle it publishes `list_restart_o`, and replaying
    // the frame's descriptors into it takes clocks; without this the band's
    // scan opens its first bands against an EMPTY list and every sprite in the
    // top rows draws nothing, silently, with `descriptors_o` reading the right
    // number. It is the sealed-list law made structural: the consumer cannot
    // read a list that is still being written.
    //
    // `seal_i` is folded in because `st_q` is still W_IDLE on the seal cycle
    // itself -- a one-cycle gap there is one band the scan could open.
    output var logic                    publishing_o,

    // ---- per-frame plane state the sampler reads continuously ---------------
    // `atm_slot_o` is the slot whose sealed role is ATMOSPHERE; slot 1 wins if
    // both claim it, because the contract's tie rule for the same role is
    // "slot 0 composites first" and the sheet is the LAST thing over the world.
    // `line_scroll_o` is that slot's own record field.
    output var logic                    atm_slot_o,
    output var logic signed [UVW-1:0]   line_scroll_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0]             planes_staged_o,
    output var logic [31:0]             sprites_staged_o,
    output var logic [31:0]             plane_refused_o,      // unrepresentable
    output var logic [31:0]             sprite_refused_o,     // unrepresentable
    output var logic [31:0]             list_overflow_o,      // frame budget, tail dropped
    output var logic [31:0]             packets_committed_o,
    output var logic [31:0]             packets_abandoned_o,
    output var logic [31:0]             frames_sealed_o,
    output var logic [31:0]             planes_published_o,
    output var logic [31:0]             sprites_published_o,
    output var logic [31:0]             slots_auto_disabled_o,
    // Two sprites whose `src_id[1:0]` agree but whose page region does not:
    // the second overwrites the first's binding and BOTH then sample the
    // second's texels. Software aliasing, not a hardware fault, and it is
    // counted rather than repaired because repairing it would mean inventing
    // a binding allocator the sampler does not have.
    output var logic [31:0]             bind_conflict_o,
    // A seal arriving while the previous frame's publish walk is still running.
    // The walk is bounded at 4*MAX_DESC + 6 clocks and a frame is 92,160, so
    // this reads zero -- and it is reachable with legal stimulus (seal twice in
    // consecutive cycles), which is why it is a counter and not a comment.
    output var logic [31:0]             seal_overrun_o
);

  // ==========================================================================
  // GEOMETRY
  // ==========================================================================
  localparam int unsigned PAW   = $clog2(PAGE_WORDS);
  localparam int unsigned BSW   = $clog2(BIND_SLOTS);
  localparam int unsigned RING  = 2 * MAX_DESC;
  localparam int unsigned RPW   = $clog2(RING) + 1;   // pointers carry a wrap bit

  // ---- the ring entry ------------------------------------------------------
  // ONE word, two record kinds, tagged at bit 0. The plane's fields end at 284
  // and the sprite's at 312, so the three the two SHARE -- the page region --
  // sit above both and are written by either.
  localparam int unsigned E_TAG  = 0;    // 1 = plane, 0 = sprite

  localparam int unsigned E_SX   = 1;
  localparam int unsigned E_SY   = 17;
  localparam int unsigned E_SW   = 33;
  localparam int unsigned E_SH   = 49;
  localparam int unsigned E_SU   = 65;
  localparam int unsigned E_SV   = 97;
  localparam int unsigned E_SA00 = 129;
  localparam int unsigned E_SA01 = 161;
  localparam int unsigned E_SA10 = 193;
  localparam int unsigned E_SA11 = 225;
  localparam int unsigned E_SFMT = 257;
  localparam int unsigned E_SPAL = 260;
  localparam int unsigned E_STNT = 268;
  localparam int unsigned E_SBLD = 284;
  localparam int unsigned E_SVM  = 286;
  localparam int unsigned E_SORD = 288;
  localparam int unsigned E_SSRC = 296;

  localparam int unsigned E_PEN  = 1;
  localparam int unsigned E_PSLT = 2;
  localparam int unsigned E_PROL = 3;
  localparam int unsigned E_PBLD = 5;
  localparam int unsigned E_POPA = 7;
  localparam int unsigned E_PFMT = 15;
  localparam int unsigned E_PWU  = 16;
  localparam int unsigned E_PWV  = 17;
  localparam int unsigned E_PVM  = 18;
  localparam int unsigned E_PPAL = 20;
  localparam int unsigned E_PW   = 28;
  localparam int unsigned E_PH   = 44;
  localparam int unsigned E_PA   = 60;
  localparam int unsigned E_PB   = 92;
  localparam int unsigned E_PC   = 124;
  localparam int unsigned E_PD   = 156;
  localparam int unsigned E_PU0  = 188;
  localparam int unsigned E_PV0  = 220;
  localparam int unsigned E_PLS  = 252;

  localparam int unsigned E_BASE = 312;
  localparam int unsigned E_LSTR = E_BASE + PAW;
  localparam int unsigned E_LHGT = E_LSTR + 4;
  localparam int unsigned EW     = E_LHGT + 4;

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`, and
  // `--lint-only` does not run one -- so a clean lint is not evidence about any
  // of these. They are here because a parameter change is exactly how a shared
  // word silently starts overlapping.
  // synthesis translate_off
  initial begin
    if (UVW != 32)
      $fatal(1, "zhao_twod_cmd: the ring word is laid out for UVW = 32");
    if (E_SSRC + 16 > E_BASE)
      $fatal(1, "zhao_twod_cmd: the sprite fields have grown into the page region");
    if (E_PLS + 32 > E_BASE)
      $fatal(1, "zhao_twod_cmd: the plane fields have grown into the page region");
    if (MAX_DESC < 2)
      $fatal(1, "zhao_twod_cmd: MAX_DESC must be >= 2 (the pointers need a bit)");
    if (RING != (1 << (RPW - 1)))
      $fatal(1, "zhao_twod_cmd: MAX_DESC must be a power of two (the ring index is the low bits)");
  end
  // synthesis translate_on

  logic [EW-1:0] ring_q [0:RING-1];
  logic [EW-1:0] rd_data_q;
  logic          rw_en_c;
  logic [RPW-2:0] rw_addr_c, rr_addr_c;
  logic [EW-1:0]  rw_data_c;

  always_ff @(posedge clk) begin
    if (rw_en_c) ring_q[rw_addr_c] <= rw_data_c;
    rd_data_q <= ring_q[rr_addr_c];
  end

  // ==========================================================================
  // POINTERS
  // ==========================================================================
  logic [RPW-1:0] wp_q, cp_q, seal_end_q, seal_start_q;
  logic [RPW-1:0] seal_len_q;

  logic list_full_c;
  assign list_full_c = ((wp_q - seal_end_q) >= RPW'(MAX_DESC));

  // ==========================================================================
  // INTAKE -- narrowing, and what is refused HERE rather than downstream
  // ==========================================================================
  // Only what cannot be REPRESENTED on the port it must travel on. Everything
  // a consumer owns a refusal for travels on unchanged, because the ruling
  // requires those rules be retained and because two blocks refusing the same
  // thing is two blocks that can disagree about it.
  logic pl_bad_c, sp_bad_c;
  always_comb begin
    pl_bad_c = (pl_slot_i      > 8'd1)
            || (pl_role_i      > 8'd3)
            || (pl_blend_i     > 8'd3)
            || (pl_format_i    > 8'd1)
            || (pl_wrap_i[7:2]      != 6'd0)
            || (pl_view_mask_i[7:2] != 6'd0)
            || (pl_flags_i[15:1]    != 15'd0)
            || (32'(pl_base_i)   >= 32'(PAGE_WORDS))
            || (pl_lstride_i[7:4] != 4'd0)
            || (pl_lheight_i[7:4] != 4'd0);

    sp_bad_c = (sp_format_i[7:3]    != 5'd0)
            || (sp_blend_i[7:2]     != 6'd0)
            || (sp_view_mask_i[7:2] != 6'd0)
            || (sp_flags_i          != 8'd0)
            || (32'(sp_base_i)   >= 32'(PAGE_WORDS))
            || (sp_lstride_i[7:4] != 4'd0)
            || (sp_lheight_i[7:4] != 4'd0);
  end

  // The handshake ALWAYS completes. See the header: lowering `ready` would
  // backpressure the command stream for the least important thing on the
  // screen, and would make the drop uncountable.
  assign pl_ready_o = 1'b1;
  assign sp_ready_o = 1'b1;

  // Plane and sprite records never arrive on the same cycle -- CMD.EXEC
  // presents one record at its own `rec_done`, and a byte stream carries one
  // record at a time. The arbitration below gives the plane the cycle and is
  // there so the write port has ONE driver rather than because the case occurs.
  logic take_pl_c, take_sp_c;
  assign take_pl_c = pl_valid_i && !pl_bad_c && !list_full_c;
  assign take_sp_c = sp_valid_i && !sp_bad_c && !list_full_c && !take_pl_c;

  logic [EW-1:0] pl_word_c, sp_word_c;
  always_comb begin
    pl_word_c = '0;
    pl_word_c[E_TAG]          = 1'b1;
    pl_word_c[E_PEN]          = pl_flags_i[0];
    pl_word_c[E_PSLT]         = pl_slot_i[0];
    pl_word_c[E_PROL +:  2]   = pl_role_i[1:0];
    pl_word_c[E_PBLD +:  2]   = pl_blend_i[1:0];
    pl_word_c[E_POPA +:  8]   = pl_opacity_i;
    pl_word_c[E_PFMT]         = pl_format_i[0];
    pl_word_c[E_PWU]          = pl_wrap_i[0];
    pl_word_c[E_PWV]          = pl_wrap_i[1];
    pl_word_c[E_PVM  +:  2]   = pl_view_mask_i[1:0];
    pl_word_c[E_PPAL +:  8]   = pl_palette_i;
    pl_word_c[E_PW   +: 16]   = pl_width_i;
    pl_word_c[E_PH   +: 16]   = pl_height_i;
    pl_word_c[E_PA   +: 32]   = pl_a_i;
    pl_word_c[E_PB   +: 32]   = pl_b_i;
    pl_word_c[E_PC   +: 32]   = pl_c_i;
    pl_word_c[E_PD   +: 32]   = pl_d_i;
    pl_word_c[E_PU0  +: 32]   = pl_u0_i;
    pl_word_c[E_PV0  +: 32]   = pl_v0_i;
    pl_word_c[E_PLS  +: 32]   = pl_line_scroll_i;
    pl_word_c[E_BASE +: PAW]  = pl_base_i[PAW-1:0];
    pl_word_c[E_LSTR +:  4]   = pl_lstride_i[3:0];
    pl_word_c[E_LHGT +:  4]   = pl_lheight_i[3:0];

    sp_word_c = '0;
    sp_word_c[E_TAG]          = 1'b0;
    sp_word_c[E_SX   +: 16]   = sp_x_i;
    sp_word_c[E_SY   +: 16]   = sp_y_i;
    sp_word_c[E_SW   +: 16]   = sp_w_i;
    sp_word_c[E_SH   +: 16]   = sp_h_i;
    sp_word_c[E_SU   +: 32]   = sp_u_i;
    sp_word_c[E_SV   +: 32]   = sp_v_i;
    sp_word_c[E_SA00 +: 32]   = sp_a00_i;
    sp_word_c[E_SA01 +: 32]   = sp_a01_i;
    sp_word_c[E_SA10 +: 32]   = sp_a10_i;
    sp_word_c[E_SA11 +: 32]   = sp_a11_i;
    sp_word_c[E_SFMT +:  3]   = sp_format_i[2:0];
    sp_word_c[E_SPAL +:  8]   = sp_palette_i;
    sp_word_c[E_STNT +: 16]   = sp_tint_i;
    sp_word_c[E_SBLD +:  2]   = sp_blend_i[1:0];
    sp_word_c[E_SVM  +:  2]   = sp_view_mask_i[1:0];
    sp_word_c[E_SORD +:  8]   = sp_order_i;
    sp_word_c[E_SSRC +: 16]   = sp_src_id_i;
    sp_word_c[E_BASE +: PAW]  = sp_base_i[PAW-1:0];
    sp_word_c[E_LSTR +:  4]   = sp_lstride_i[3:0];
    sp_word_c[E_LHGT +:  4]   = sp_lheight_i[3:0];
  end

  always_comb begin
    rw_en_c   = take_pl_c || take_sp_c;
    rw_addr_c = wp_q[RPW-2:0];
    rw_data_c = take_pl_c ? pl_word_c : sp_word_c;
  end

  // ==========================================================================
  // THE PUBLISH WALK
  // ==========================================================================
  // Two passes over the sealed window, planes first (see the header for the
  // race that buys), then the two auto-disables, then the sprites. Two clocks
  // per entry: address out, word back, drive and wait for ready. The
  // destinations are `zhao_twod_plane` and `zhao_twod_band`, both of which hold
  // `ready` high permanently -- the handshake is honoured anyway, because a
  // producer that assumes a constant is a producer that breaks when the
  // constant stops being one.
  typedef enum logic [3:0] {
    W_IDLE, W_PA, W_PD, W_PW, W_DIS, W_DISW, W_SA, W_SD, W_SW
  } wstate_e;
  wstate_e         st_q;
  logic [RPW-1:0]  walk_q, left_q;
  logic            named_q [0:1];
  logic            dis_slot_q;

  logic [RPW-1:0] walk_next_c;
  assign walk_next_c = walk_q + RPW'(1);

  // The read address: the walk's current entry. It is held across the emit and
  // wait states, so `rd_data_q` is stable for the whole visit -- which is what
  // lets the emit state load the outputs once and the wait state judge only the
  // handshake. An emit state that also judged the handshake would re-emit the
  // same entry every cycle it was stalled, and the published counter would
  // report a HUD several times the size of the one on screen.
  assign rr_addr_c = walk_q[RPW-2:0];

  logic rd_is_plane_c;
  assign rd_is_plane_c = rd_data_q[E_TAG];

  assign publishing_o = (st_q != W_IDLE) || seal_i;

  // ==========================================================================
  // BINDING SHADOW -- what `bind_conflict_o` differences
  // ==========================================================================
  // Its two operands are written by DIFFERENT descriptors, on different cycles,
  // from different ring entries. No single register enable drives both, which
  // is the property CLAUDE.md's metadata-bank defect demands be checked before
  // a detector's silence is quoted.
  logic [PAW+7:0] bind_shadow_q [0:BIND_SLOTS-1];
  logic           bind_seen_q   [0:BIND_SLOTS-1];
  logic [PAW+7:0] bind_word_c;
  logic [BSW-1:0] bind_sel_c;

  assign bind_word_c = {rd_data_q[E_LHGT +: 4], rd_data_q[E_LSTR +: 4],
                        rd_data_q[E_BASE +: PAW]};
  // The sampler's own published convention: 0..1 the plane's two ROLES, 4..7
  // the sprite's low two `src_id` bits.
  assign bind_sel_c = rd_is_plane_c
                    ? BSW'({1'b0, rd_data_q[E_PROL]})
                    : BSW'({2'b01, rd_data_q[E_SSRC +: 2]});

  // ==========================================================================
  // THE SEAL'S COMMIT POINTER
  // ==========================================================================
  // `cp_q` is read by the seal and possibly written by a commit on the SAME
  // edge. A packet committing exactly on the tick belongs to the frame that is
  // CLOSING -- its records were staged before it -- so a non-blocking read of
  // the old `cp_q` would drop them silently. Folded in explicitly rather than
  // left to assignment order, which is the class of bug this repository calls
  // "check what a signal MEANS and WHEN IT IS VALID".
  logic [RPW-1:0] cp_now_c;
  assign cp_now_c = pkt_commit_i ? wp_q : cp_q;

  // ==========================================================================
  // THE SEQUENTIAL BODY
  // ==========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wp_q <= '0; cp_q <= '0; seal_end_q <= '0; seal_start_q <= '0;
      seal_len_q <= '0;
      st_q <= W_IDLE; walk_q <= '0; left_q <= '0; dis_slot_q <= 1'b0;
      named_q[0] <= 1'b0; named_q[1] <= 1'b0;
      d_valid_o <= 1'b0; s_valid_o <= 1'b0;
      d_slot_o <= 1'b0; d_enable_o <= 1'b0; d_role_o <= 2'd0; d_blend_o <= 2'd0;
      d_opacity_o <= 8'd0; d_format_o <= 1'b0;
      d_width_o <= 16'd0; d_height_o <= 16'd0;
      d_wrap_u_o <= 1'b0; d_wrap_v_o <= 1'b0;
      d_a_o <= '0; d_b_o <= '0; d_c_o <= '0; d_d_o <= '0;
      d_u0_o <= '0; d_v0_o <= '0;
      d_view_mask_o <= 2'd0; d_palette_o <= 8'd0;
      s_x_o <= '0; s_y_o <= '0; s_w_o <= 16'd0; s_h_o <= 16'd0;
      s_u_o <= '0; s_v_o <= '0;
      s_a00_o <= '0; s_a01_o <= '0; s_a10_o <= '0; s_a11_o <= '0;
      s_format_o <= 3'd0; s_palette_o <= 8'd0; s_tint_o <= 16'd0;
      s_blend_o <= 2'd0; s_view_mask_o <= 2'd0; s_order_o <= 8'd0;
      s_src_id_o <= 16'd0;
      ld_bind_we_o <= 1'b0; ld_bind_sel_o <= '0; ld_bind_base_o <= '0;
      ld_bind_lstride_o <= 4'd0; ld_bind_lheight_o <= 4'd0;
      atm_slot_o <= 1'b0; line_scroll_o <= '0;
      planes_staged_o <= '0; sprites_staged_o <= '0;
      plane_refused_o <= '0; sprite_refused_o <= '0;
      list_overflow_o <= '0;
      packets_committed_o <= '0; packets_abandoned_o <= '0;
      frames_sealed_o <= '0;
      planes_published_o <= '0; sprites_published_o <= '0;
      slots_auto_disabled_o <= '0;
      bind_conflict_o <= '0; seal_overrun_o <= '0;
      for (int i = 0; i < BIND_SLOTS; i++) begin
        bind_shadow_q[i] <= '0;
        bind_seen_q[i]   <= 1'b0;
      end
    end else begin
      ld_bind_we_o <= 1'b0;

      // ---- intake ---------------------------------------------------------
      if (pl_valid_i) begin
        if (pl_bad_c)          plane_refused_o <= plane_refused_o + 32'd1;
        else if (list_full_c)  list_overflow_o <= list_overflow_o + 32'd1;
        else begin
          wp_q            <= wp_q + RPW'(1);
          planes_staged_o <= planes_staged_o + 32'd1;
        end
      end
      if (sp_valid_i && !take_pl_c) begin
        if (sp_bad_c)          sprite_refused_o <= sprite_refused_o + 32'd1;
        else if (list_full_c)  list_overflow_o  <= list_overflow_o + 32'd1;
        else begin
          wp_q             <= wp_q + RPW'(1);
          sprites_staged_o <= sprites_staged_o + 32'd1;
        end
      end

      // ---- the packet verdict ---------------------------------------------
      if (pkt_commit_i) begin
        cp_q                <= wp_q;
        packets_committed_o <= packets_committed_o + 32'd1;
      end else if (pkt_abandon_i) begin
        wp_q                <= cp_q;
        packets_abandoned_o <= packets_abandoned_o + 32'd1;
      end

      // ---- the seal -------------------------------------------------------
      if (seal_i) begin
        if (st_q != W_IDLE) begin
          seal_overrun_o <= seal_overrun_o + 32'd1;
        end else begin
          seal_start_q    <= seal_end_q;
          seal_len_q      <= cp_now_c - seal_end_q;
          seal_end_q      <= cp_now_c;
          walk_q          <= seal_end_q;
          left_q          <= cp_now_c - seal_end_q;
          st_q            <= W_PA;
          named_q[0]      <= 1'b0;
          named_q[1]      <= 1'b0;
          dis_slot_q      <= 1'b0;
          frames_sealed_o <= frames_sealed_o + 32'd1;
          for (int i = 0; i < BIND_SLOTS; i++) bind_seen_q[i] <= 1'b0;
        end
      end

      // ---- the publish walk -----------------------------------------------
      case (st_q)
        W_IDLE: begin
          d_valid_o <= 1'b0;
          s_valid_o <= 1'b0;
        end

        // ---- pass 1: the planes, first, and the header says why -------------
        W_PA: begin
          if (left_q == RPW'(0)) begin
            st_q       <= W_DIS;
            dis_slot_q <= 1'b0;
            walk_q     <= seal_start_q;   // re-based for pass 2
            left_q     <= seal_len_q;
          end else begin
            st_q <= W_PD;
          end
        end
        W_PD: begin
          if (rd_is_plane_c) begin
            d_valid_o     <= 1'b1;
            d_slot_o      <= rd_data_q[E_PSLT];
            d_enable_o    <= rd_data_q[E_PEN];
            d_role_o      <= rd_data_q[E_PROL +:  2];
            d_blend_o     <= rd_data_q[E_PBLD +:  2];
            d_opacity_o   <= rd_data_q[E_POPA +:  8];
            d_format_o    <= rd_data_q[E_PFMT];
            d_width_o     <= rd_data_q[E_PW   +: 16];
            d_height_o    <= rd_data_q[E_PH   +: 16];
            d_wrap_u_o    <= rd_data_q[E_PWU];
            d_wrap_v_o    <= rd_data_q[E_PWV];
            d_a_o         <= rd_data_q[E_PA   +: 32];
            d_b_o         <= rd_data_q[E_PB   +: 32];
            d_c_o         <= rd_data_q[E_PC   +: 32];
            d_d_o         <= rd_data_q[E_PD   +: 32];
            d_u0_o        <= rd_data_q[E_PU0  +: 32];
            d_v0_o        <= rd_data_q[E_PV0  +: 32];
            d_view_mask_o <= rd_data_q[E_PVM  +:  2];
            d_palette_o   <= rd_data_q[E_PPAL +:  8];

            named_q[rd_data_q[E_PSLT]] <= 1'b1;
            planes_published_o <= planes_published_o + 32'd1;

            // The ATMOSPHERE selector and its line scroll, held for the frame.
            // NOT cleared at the seal: the sampler reads them continuously and
            // a frame almost always re-declares the same sheet, so holding the
            // previous value across the publish window is strictly less wrong
            // than a transient default.
            if (rd_data_q[E_PEN] && (rd_data_q[E_PROL +: 2] == 2'd1)) begin
              atm_slot_o    <= rd_data_q[E_PSLT];
              line_scroll_o <= rd_data_q[E_PLS +: 32];
            end

            // The page region this plane samples from.
            ld_bind_we_o      <= 1'b1;
            ld_bind_sel_o     <= bind_sel_c;
            ld_bind_base_o    <= rd_data_q[E_BASE +: PAW];
            ld_bind_lstride_o <= rd_data_q[E_LSTR +: 4];
            ld_bind_lheight_o <= rd_data_q[E_LHGT +: 4];
            if (bind_seen_q[bind_sel_c] && (bind_shadow_q[bind_sel_c] != bind_word_c))
              bind_conflict_o <= bind_conflict_o + 32'd1;
            bind_shadow_q[bind_sel_c] <= bind_word_c;
            bind_seen_q[bind_sel_c]   <= 1'b1;

            st_q <= W_PW;
          end else begin
            // A sprite in pass 1: skipped in one clock, not emitted.
            walk_q <= walk_next_c;
            left_q <= left_q - RPW'(1);
            st_q   <= W_PA;
          end
        end
        W_PW: begin
          if (d_ready_i) begin
            d_valid_o <= 1'b0;
            walk_q    <= walk_next_c;
            left_q    <= left_q - RPW'(1);
            st_q      <= W_PA;
          end
        end

        // ---- the two auto-disables -------------------------------------------
        W_DIS: begin
          if (named_q[dis_slot_q]) begin
            if (dis_slot_q) st_q <= W_SA;
            else            dis_slot_q <= 1'b1;
          end else begin
            // `TWOD.PLANE.md`: descriptors are RE-SENT PER FRAME. A slot the
            // frame never named is OFF, on the lawful enable path, counted
            // apart from both refusal counters so an intent can never be read
            // as a fault.
            d_valid_o  <= 1'b1;
            d_slot_o   <= dis_slot_q;
            d_enable_o <= 1'b0;
            slots_auto_disabled_o <= slots_auto_disabled_o + 32'd1;
            st_q <= W_DISW;
          end
        end
        W_DISW: begin
          if (d_ready_i) begin
            d_valid_o <= 1'b0;
            if (dis_slot_q) st_q <= W_SA;
            else begin
              dis_slot_q <= 1'b1;
              st_q       <= W_DIS;
            end
          end
        end

        // ---- pass 2: the sprites, in command order ---------------------------
        W_SA: begin
          if (left_q == RPW'(0)) st_q <= W_IDLE;
          else                   st_q <= W_SD;
        end
        W_SD: begin
          if (!rd_is_plane_c) begin
            s_valid_o     <= 1'b1;
            s_x_o         <= rd_data_q[E_SX   +: 16];
            s_y_o         <= rd_data_q[E_SY   +: 16];
            s_w_o         <= rd_data_q[E_SW   +: 16];
            s_h_o         <= rd_data_q[E_SH   +: 16];
            s_u_o         <= rd_data_q[E_SU   +: 32];
            s_v_o         <= rd_data_q[E_SV   +: 32];
            s_a00_o       <= rd_data_q[E_SA00 +: 32];
            s_a01_o       <= rd_data_q[E_SA01 +: 32];
            s_a10_o       <= rd_data_q[E_SA10 +: 32];
            s_a11_o       <= rd_data_q[E_SA11 +: 32];
            s_format_o    <= rd_data_q[E_SFMT +:  3];
            s_palette_o   <= rd_data_q[E_SPAL +:  8];
            s_tint_o      <= rd_data_q[E_STNT +: 16];
            s_blend_o     <= rd_data_q[E_SBLD +:  2];
            s_view_mask_o <= rd_data_q[E_SVM  +:  2];
            s_order_o     <= rd_data_q[E_SORD +:  8];
            s_src_id_o    <= rd_data_q[E_SSRC +: 16];
            sprites_published_o <= sprites_published_o + 32'd1;

            ld_bind_we_o      <= 1'b1;
            ld_bind_sel_o     <= bind_sel_c;
            ld_bind_base_o    <= rd_data_q[E_BASE +: PAW];
            ld_bind_lstride_o <= rd_data_q[E_LSTR +: 4];
            ld_bind_lheight_o <= rd_data_q[E_LHGT +: 4];
            if (bind_seen_q[bind_sel_c] && (bind_shadow_q[bind_sel_c] != bind_word_c))
              bind_conflict_o <= bind_conflict_o + 32'd1;
            bind_shadow_q[bind_sel_c] <= bind_word_c;
            bind_seen_q[bind_sel_c]   <= 1'b1;

            st_q <= W_SW;
          end else begin
            walk_q <= walk_next_c;
            left_q <= left_q - RPW'(1);
            st_q   <= W_SA;
          end
        end
        W_SW: begin
          if (s_ready_i) begin
            s_valid_o <= 1'b0;
            walk_q    <= walk_next_c;
            left_q    <= left_q - RPW'(1);
            st_q      <= W_SA;
          end
        end

        default: st_q <= W_IDLE;
      endcase
    end
  end

endmodule : zhao_twod_cmd

`default_nettype wire
