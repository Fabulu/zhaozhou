// zhao_twod_sampler.sv -- TWOD.PLANE's and TWOD.SPRITE's texel sample requests
// turned into colours, with the page store that makes that possible and the
// screen-addressed read port POST.COMPOSITE's atmosphere seam actually asks
// for.
//
// ===========================================================================
// WHAT WAS SEARCHED BEFORE THIS FILE WAS WRITTEN
// ===========================================================================
// `zhao_console_core.sv` entry I17 said "Nothing in `fpga/rtl` is that:
// TEXTURE.CACHE and the TMU are fragment-shaped and sit on the raster path",
// and this repository has already produced four false "X does not exist"
// claims. So the claim was checked before a line was written. The sweep
// covered every RGB565 expansion, every palette lookup, every texel store and
// every module whose ports are shaped like (u, v, format, palette) -> colour,
// across `fpga/rtl/**`. What it found, file by file:
//
//   * `fpga/rtl/texture/zhao_texture_tmu_pipe.sv` -- IS the right SHAPE
//     (`req_u_i`/`req_v_i`/`req_mode_i`/`req_pal_base_i` in, `smp_rgb_o` out)
//     and DOES contain both decodes plus its own palette M10K at its line 403.
//     It is unusable here for a reason that is structural rather than
//     stylistic: its third port group `cac_*` is MANDATORY and goes to
//     `zhao_texture_cache`, which owns no pages -- every miss must be serviced
//     from VRAM. That is entry I23's absent SDRAM model, and I17's note that
//     "there is no spare arbiter client index" (entry I15 item 2) is the same
//     wall. It also carries a reorder buffer, mip chains, bilinear and LOD --
//     34 ports of fragment machinery for a block whose whole contract forbids
//     bilinear.
//   * `fpga/rtl/texture/zhao_texture_palette_res_v2.sv` -- a genuinely
//     reusable CLUT8 palette store with a preload port and no memory client.
//     NOT reused, and the reason is the opposite of the one I17 gives: it
//     EXPANDS to RGB888 (`expand_rgb565`, its line 155) and delivers the
//     result inside a 48-bit `zhao_render_texture_pkg` result tuple with a
//     generation token and a slot tag. POST.COMPOSITE consumes RGB565
//     (`atm_rgb_i[15:0]`, and it runs its own `exp5`/`exp6` at line 527), so
//     reusing that block would mean building a tuple, expanding to 888 and
//     re-compressing to 565 -- three conversions to arrive where the palette
//     entry already was. The store here holds the same RGB565 entries.
//   * `fpga/rtl/texture/zhao_texture_cache.sv` -- 1 KiB of line cache with a
//     mandatory `fill_*` VRAM port. Its own header puts format selection
//     outside itself. It is a bandwidth filter, not a page pool.
//   * `fpga/rtl/texture/zhao_texture_binding_resolver_v2.sv` -- defines
//     `FMT_CLUT8`/`FMT_RGB565` (3'd0 / 3'd1) and routes them; no colour.
//   * `fpga/rtl/texture/zhao_texture_mosaic_v2.sv` -- (u,v) -> tile picker, no
//     colour at all.
//   * `zhao_texture_tmu.sv`, `zhao_texture_island_top.sv`,
//     `zhao_texture_island_v3_top.sv` -- three further copies of `decode16`.
//
// ===========================================================================
// SO I17's STATED OBSTACLE WAS HALF WRONG, AND THE HALF THAT WAS WRONG IS THE
// HALF THAT REFUSED THE WORK
// ===========================================================================
// I17 refused on two grounds. One survives and one does not.
//
//   SURVIVES: there is no TEXEL PAGE STORE anywhere in the tree that a (u, v)
//   can walk into without a VRAM fill agent. That is real, it is I23, and it
//   is why this file contains a page store of its own rather than a client.
//
//   DOES NOT SURVIVE: "inventing the CLUT lookup would additionally be
//   inventing a colour law." THERE IS NO COLOUR LAW HERE TO INVENT.
//   POST.COMPOSITE's `atm_rgb_i` and `hud_rgb_i` are RGB565. A CLUT8 palette
//   entry is RGB565. An RGB565 texel is RGB565. This block therefore performs
//   ZERO COLOUR ARITHMETIC -- no expansion, no rounding, no matrix, not one
//   multiply. It selects between a palette entry and a texel word. The
//   expansion law (replicate the high bits; `zref::sky::rgb565::to_rgb888`)
//   stays entirely inside POST.COMPOSITE where it already lives, and a
//   `git grep exp5` shows it unchanged.
//
// The cost of that wrong half was a whole seam left open on a reason that a
// five-minute grep disproves. It is recorded here rather than quietly fixed,
// because the next refusal will be written by somebody reading this file.
//
// ===========================================================================
// WHY THIS BLOCK OWNS A RASTER WALK, WHICH IS THE PART THAT LOOKS LIKE SCOPE
// CREEP AND IS NOT
// ===========================================================================
// POST.COMPOSITE does not accept a stream of atmosphere pixels. Its own header
// states the contract twice over: the `atm_*` group is a RANDOM ACCESS --
// "address out in cycle N, data in cycle N+1", and "when the pipeline stalls,
// the request holds, and the response MUST hold with it. A SYNCHRONOUS MEMORY
// DOES THIS FOR FREE; a plane or sprite engine that free-runs does not."
//
// That sentence is an instruction to put a memory between the engine and the
// compositor, and it is the reason a bare streaming sampler could not have
// closed this seam however correct its colours were. TWOD.PLANE is one
// register deep and this sampler is three more, so wiring `atm_req_x_o`
// straight into `p_x_i` would answer four cycles late, every cycle.
//
// So the atmosphere line is prepared AHEAD, into a small ring of whole lines,
// and `atm_*` is served by a plain synchronous read of that ring. The hold
// property then costs nothing and cannot be got wrong: on a stall the
// compositor's address registers do not move, this block re-reads the same
// address, and the same data comes back.
//
// Somebody has to generate the (x, y) TWOD.PLANE is a function of. It is this
// block, because this block is the one that knows which ring slot is free --
// and TWOD.PLANE's own header says "the compositor owns the walk". The walk
// leaves here as `pw_*` and is wired to that block's `p_*` in the composer:
// ordinary wiring, no invented logic in `zhao_console_core.sv`.
//
// THE PAIRING IS STRUCTURAL, NOT RECONSTRUCTED. TWOD.PLANE emits no screen
// coordinate, so the sample returning from it has to be matched to the pixel
// that asked for it. It is exactly one register deep with `p_ready_o =
// !s_valid_o || s_ready_i`, so a pixel accepted on cycle N produces its sample
// on N+1 and nothing else can appear there -- one shadow register, no FIFO,
// no reconstructed delay. And a pixel the plane SKIPS (view mask, disabled
// slot) produces NO sample on N+1, which is how the skip is detected: the
// shadow is set and `pl_valid_i` is low. That case writes a transparent cell
// rather than leaving the previous line's colour in the ring, and counts
// itself in `skipped_fill_o`.
//
// `pair_lost_o` is the detector for the assumption itself -- a sample arriving
// with no shadow behind it. Its two operands are clocked by DIFFERENT things
// (the shadow by this block's walk, `pl_valid_i` by TWOD.PLANE's own register)
// so it is not blind in the way `CLAUDE.md`'s metadata-bank detector was. It
// reads zero in correct operation and the directed bench FIRES IT DELIBERATELY
// by presenting a plane sample nobody asked for.
//
// ===========================================================================
// WHAT THIS BLOCK REFUSES
// ===========================================================================
// * HUD IS NOT SERVED FROM HERE, and the reason is not the sampler. The
//   sprite half below is complete: it takes TWOD.SPRITE's request shape, fx16
//   UV and all, and emits the colour. What it cannot do is answer
//   POST.COMPOSITE's `hud_*` port, because that port is the same 1-cycle
//   RANDOM ACCESS in RASTER ORDER and TWOD.SPRITE walks in DESCRIPTOR order,
//   one whole sprite at a time. Bridging those needs either a frame-resident
//   HUD store (384 x 240 x 17 bits is about 1.6 Mbit, which is SDRAM, which is
//   I23) or a display list that can re-walk one SCANLINE across many
//   descriptors -- which is a different block from the one TWOD.SPRITE is.
//   A two-line ring plus backpressure was worked through and rejected: it
//   makes a sprite trickle one row per composited line, so ten 32-row sprites
//   need 320 lines of a 240-line frame. That is a machine that passes its
//   tests and cannot draw a HUD, which is worse than a declared gap.
//   The sprite colour therefore leaves on `sc_*` as an honest stream.
// * TINT IS FORWARDED, NOT APPLIED. `sp_tint_i` is a modulation and the
//   console already has an owner for modulation arithmetic
//   (`zhao_texture_combine`). Choosing a second one here would be inventing
//   the colour law this file's header just finished saying it does not invent.
//   `tint_unapplied_o` counts every sample whose tint is not unity, so the
//   omission is visible rather than silent.
// * FORMATS OTHER THAN CLUT8 AND RGB565 ARE REFUSED. TWOD.PLANE's header:
//   "CLUT8 and RGB565 only". TWOD.SPRITE's format field is three bits wide
//   because it shares the TMU's encoding; values 2..7 are counted in
//   `fmt_refused_o` and sampled as `REFUSED_RGB`.
// * NEAREST ONLY, structurally: the only coordinates that enter are integers
//   (the plane's) or an fx16 pair whose fraction is discarded here and nowhere
//   stored. There is no filter weight port for a future bilinear to use.
// * PAGES ARE POWER-OF-TWO. A binding carries log2(row stride) and
//   log2(height), never a width, so the address is shifts and adds and the
//   wrap is a mask -- exact, and no divider and no multiplier anywhere in this
//   file. A caller whose page is not a power of two is not silently tiled:
//   `texel_wrapped_o` counts every coordinate the mask changed.
//
// ===========================================================================
// STORAGE: M10K, DELIBERATELY
// ===========================================================================
// Owner ruling: "Using some more M10K is fine, we have enough, particularly if
// it saves ALMs. They're our only weapon against our massive ALM debt." Every
// array here is declared for M10K and READ THROUGH A REGISTER, because Cyclone
// V's M10K has a mandatory address register and a design that wanted a
// same-cycle answer would be pushed into MLABs or flops, which is the ALM bill
// the ruling is trying to avoid. `ramstyle` is a HINT: nothing here has been
// through `quartus_map` yet, and the first fit is where the inference is
// confirmed.
//
//   page store   PAGE_WORDS x 16          default 8,192 words = 16 KiB
//   palette      PAL_SLOTS x 256 x 16     default 4 slots
//   atm ring     ATM_LINES x LINE_W x 25  default 4 x 384
//
// The bindings are a small table and stay in flops on purpose: BIND_SLOTS is 8
// and the entry is 21 bits, which is about 170 registers -- an MLAB with a
// read port on the critical address path would cost more than it saves.
// ===========================================================================
`default_nettype none

module zhao_twod_sampler #(
    // The widest and tallest VIEW this instance serves. 384 x 240 covers Z60,
    // Storm and a Duo view -- the same bound POST.COMPOSITE is parameterised
    // on, and it must be given the same numbers or the ring and the compositor
    // disagree about what a line is.
    parameter int unsigned LINE_W     = 384,
    parameter int unsigned MAX_H      = 240,
    // The page store, in 16-bit words. CLUT8 packs two texels per word.
    parameter int unsigned PAGE_WORDS = 8192,
    // Palette slots of 256 RGB565 entries.
    parameter int unsigned PAL_SLOTS  = 4,
    // Binding slots: 0..1 are the plane's two ROLES, 4..7 the sprite's low two
    // `src_id` bits. A v1 ceiling, and it is a knob.
    parameter int unsigned BIND_SLOTS = 8,
    // Whole lines of atmosphere held ahead of the compositor. Must be a power
    // of two: the slot is the low bits of y, which is what makes "the line in
    // this slot is that line" a comparison rather than a divide.
    parameter int unsigned ATM_LINES  = 4
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the frame ---------------------------------------------------------
    input  var logic                        frame_start_i,
    input  var logic [$clog2(LINE_W+1)-1:0] frame_w_i,
    input  var logic [$clog2(MAX_H +1)-1:0] frame_h_i,

    // ---- asset load ---------------------------------------------------------
    // Pages, palettes and bindings are generated ASSETS. Entry I17 already
    // accepts that shape for the grading curves -- "their load port is
    // legitimately external" -- and the same sentence covers these.
    input  var logic                          ld_page_we_i,
    input  var logic [$clog2(PAGE_WORDS)-1:0] ld_page_addr_i,
    input  var logic [15:0]                   ld_page_data_i,
    input  var logic                            ld_pal_we_i,
    input  var logic [$clog2(PAL_SLOTS*256)-1:0] ld_pal_addr_i,
    input  var logic [15:0]                     ld_pal_data_i,
    input  var logic                          ld_bind_we_i,
    input  var logic [$clog2(BIND_SLOTS)-1:0] ld_bind_sel_i,
    input  var logic [$clog2(PAGE_WORDS)-1:0] ld_bind_base_i,
    input  var logic [3:0]                    ld_bind_lstride_i,  // log2 words per row
    input  var logic [3:0]                    ld_bind_lheight_i,  // log2 rows

    // ---- the plane walk this block drives ----------------------------------
    input  var logic                 atm_slot_i,       // which TWOD.PLANE slot is the sheet
    input  var logic signed [31:0]   line_scroll_i,
    output var logic                 pw_valid_o,
    input  var logic                 pw_ready_i,
    output var logic                 pw_slot_o,
    output var logic [15:0]          pw_x_o,
    output var logic [15:0]          pw_y_o,
    output var logic signed [31:0]   pw_line_scroll_o,

    // ---- TWOD.PLANE's sample request (its `s_*` group, port for port) -------
    input  var logic                 pl_valid_i,
    output var logic                 pl_ready_o,
    input  var logic [15:0]          pl_texel_u_i,
    input  var logic [15:0]          pl_texel_v_i,
    input  var logic                 pl_format_i,      // 0 CLUT8, 1 RGB565
    input  var logic [7:0]           pl_palette_i,
    input  var logic [1:0]           pl_blend_i,
    input  var logic [7:0]           pl_opacity_i,
    input  var logic [1:0]           pl_role_i,

    // ---- TWOD.SPRITE's sample request (its `s_*` group, port for port) ------
    input  var logic                 sp_valid_i,
    output var logic                 sp_ready_o,
    input  var logic signed [15:0]   sp_x_i,
    input  var logic signed [15:0]   sp_y_i,
    // The low 16 bits of these two are the FRACTION, and nothing reads them.
    // That is the "nearest only" guarantee made structural on the sprite side:
    // a future bilinear cannot quietly appear here, it would have to grow a
    // port. The waiver is narrow and is the evidence, not an oversight.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic signed [31:0]   sp_u_i,           // fx16 S15.16
    input  var logic signed [31:0]   sp_v_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  var logic [2:0]           sp_format_i,      // 0 CLUT8, 1 RGB565, else refused
    input  var logic [7:0]           sp_palette_i,
    input  var logic [15:0]          sp_tint_i,
    input  var logic [1:0]           sp_blend_i,
    input  var logic [7:0]           sp_order_i,
    input  var logic [15:0]          sp_src_id_i,
    input  var logic                 sp_last_i,

    // ---- the sprite colour stream ------------------------------------------
    output var logic                 sc_valid_o,
    input  var logic                 sc_ready_i,
    output var logic [15:0]          sc_rgb_o,         // RGB565, native
    output var logic signed [15:0]   sc_x_o,
    output var logic signed [15:0]   sc_y_o,
    output var logic [15:0]          sc_tint_o,        // FORWARDED, not applied
    output var logic [1:0]           sc_blend_o,
    output var logic [7:0]           sc_order_o,
    output var logic [15:0]          sc_src_id_o,
    output var logic                 sc_last_o,

    // ---- POST.COMPOSITE's atmosphere read, on its own convention -----------
    // Address in cycle N, data in cycle N+1, and both hold through a stall
    // because a synchronous read of an address that did not move returns the
    // same word.
    input  var logic                        atm_req_v_i,
    input  var logic [$clog2(LINE_W+1)-1:0] atm_req_x_i,
    input  var logic [$clog2(MAX_H +1)-1:0] atm_req_y_i,
    output var logic                        atm_en_o,
    output var logic                        atm_valid_o,
    output var logic [15:0]                 atm_rgb_o,
    output var logic [7:0]                  atm_opacity_o,
    output var logic                        atm_add_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0]          samples_o,
    output var logic [31:0]          plane_samples_o,
    output var logic [31:0]          sprite_samples_o,
    output var logic [31:0]          clut8_samples_o,
    output var logic [31:0]          rgb565_samples_o,
    output var logic [31:0]          texel_wrapped_o,   // a mask changed the coordinate
    output var logic [31:0]          page_oob_o,        // word address past the store
    output var logic [31:0]          bind_missing_o,    // binding slot never programmed
    output var logic [31:0]          fmt_refused_o,     // sprite format 2..7
    output var logic [31:0]          pal_refused_o,     // palette id past PAL_SLOTS
    output var logic [31:0]          skipped_fill_o,    // the plane skipped; cell cleared
    output var logic [31:0]          atm_underrun_o,    // compositor asked for an unready line
    output var logic [31:0]          walk_stalls_o,     // no ring slot free
    output var logic [31:0]          sprite_stalls_o,   // sprite offered, not accepted
    output var logic [31:0]          tint_unapplied_o,  // tint forwarded, not applied
    output var logic [31:0]          pair_lost_o        // a plane sample nobody asked for
);

  // The key colour for a refused format. RGB565 magenta, the 565 of the
  // 24'hFF00FF `zhao_texture_palette_res_v2` already uses for a bad lookup --
  // the same convention, not a new one.
  localparam logic [15:0] REFUSED_RGB = 16'hF81F;

  localparam int unsigned XW    = $clog2(LINE_W + 1);
  localparam int unsigned YW    = $clog2(MAX_H + 1);
  localparam int unsigned PAW   = $clog2(PAGE_WORDS);
  localparam int unsigned PALAW = $clog2(PAL_SLOTS * 256);
  localparam int unsigned PALSW = $clog2(PAL_SLOTS);
  localparam int unsigned BSW   = $clog2(BIND_SLOTS);
  localparam int unsigned ASW   = $clog2(ATM_LINES);
  localparam int unsigned RINGW = 25;                      // {add, opacity, rgb}
  localparam int unsigned RAW   = $clog2(ATM_LINES * LINE_W);

  // Elaboration guards. Inside `initial begin ... end` because Quartus 17.0
  // rejects a bare module-scope `if` -- CLAUDE.md records that exact failure.
  // `--lint-only` does not run these, so a clean lint says nothing about them.
  // synthesis translate_off
  initial begin
    if ((ATM_LINES & (ATM_LINES - 1)) != 0)
      $fatal(1, "zhao_twod_sampler: ATM_LINES must be a power of two");
    if (ATM_LINES < 3)
      $fatal(1, "zhao_twod_sampler: ATM_LINES < 3 cannot hold a line being written, a line being read and the gap between them");
    if ((PAL_SLOTS & (PAL_SLOTS - 1)) != 0)
      $fatal(1, "zhao_twod_sampler: PAL_SLOTS must be a power of two");
    if (BIND_SLOTS < 8)
      $fatal(1, "zhao_twod_sampler: BIND_SLOTS < 8 leaves the sprite with no binding range");
  end
  // synthesis translate_on

  // ==========================================================================
  // STORES
  // ==========================================================================
  (* ramstyle = "M10K" *) logic [15:0]      page_m [0:PAGE_WORDS-1];
  (* ramstyle = "M10K" *) logic [15:0]      pal_m  [0:PAL_SLOTS*256-1];
  (* ramstyle = "M10K" *) logic [RINGW-1:0] ring_m [0:ATM_LINES*LINE_W-1];

  // The binding table. 8 x 21 bits in flops -- see the header.
  logic [PAW-1:0] bind_base_q    [0:BIND_SLOTS-1];
  logic [3:0]     bind_lstride_q [0:BIND_SLOTS-1];
  logic [3:0]     bind_lheight_q [0:BIND_SLOTS-1];
  logic           bind_prog_q    [0:BIND_SLOTS-1];

  // ==========================================================================
  // THE OUTPUT STAGE AND THE ONE STALL IN THE BLOCK
  // ==========================================================================
  // Only the sprite stream can refuse a colour. The atmosphere ring is a
  // memory and never does, which is the whole reason the walk can be allowed
  // to run at one pixel per clock.
  logic                 o_v_q, o_dest_q;          // dest 0 = plane, 1 = sprite
  logic [15:0]          o_rgb_q;
  logic [15:0]          o_x_q, o_y_q;
  logic [7:0]           o_opacity_q;
  logic                 o_add_q;
  logic [15:0]          o_tint_q;
  logic [1:0]           o_blend_q;
  logic [7:0]           o_order_q;
  logic [15:0]          o_srcid_q;
  logic                 o_last_q;

  logic stall_c, pipe_en_c;
  assign stall_c   = o_v_q && o_dest_q && !sc_ready_i;
  assign pipe_en_c = !stall_c;

  assign sc_valid_o  = o_v_q && o_dest_q;
  assign sc_rgb_o    = o_rgb_q;
  assign sc_x_o      = 16'(o_x_q);
  assign sc_y_o      = 16'(o_y_q);
  assign sc_tint_o   = o_tint_q;
  assign sc_blend_o  = o_blend_q;
  assign sc_order_o  = o_order_q;
  assign sc_src_id_o = o_srcid_q;
  assign sc_last_o   = o_last_q;

  // ==========================================================================
  // THE WALK
  // ==========================================================================
  logic [XW-1:0] walk_x_q;
  logic [YW-1:0] walk_y_q;
  logic          walk_run_q;
  logic signed [31:0] scroll_q;

  // Ring slot bookkeeping. Two bits of state rather than one valid bit,
  // because "being filled" and "empty" must not be the same thing to the
  // claim test or the walk would reclaim the slot it is writing.
  localparam logic [1:0] SLOT_EMPTY  = 2'd0;
  localparam logic [1:0] SLOT_FILLING = 2'd1;
  localparam logic [1:0] SLOT_READY  = 2'd2;

  logic [1:0]    slot_st_q  [0:ATM_LINES-1];
  logic [YW-1:0] slot_y_q   [0:ATM_LINES-1];
  logic [YW-1:0] rd_line_q;          // the last line the compositor asked for

  logic [ASW-1:0] walk_slot_c;
  logic           slot_ok_c;
  assign walk_slot_c = walk_y_q[ASW-1:0];
  assign slot_ok_c   = (slot_st_q[walk_slot_c] == SLOT_EMPTY)
                    || ((slot_st_q[walk_slot_c] == SLOT_READY)
                        && (slot_y_q[walk_slot_c] < rd_line_q))
                    || ((slot_st_q[walk_slot_c] == SLOT_FILLING)
                        && (slot_y_q[walk_slot_c] == walk_y_q));

  assign pw_valid_o       = walk_run_q && slot_ok_c && pipe_en_c;
  assign pw_slot_o        = atm_slot_i;
  assign pw_x_o           = 16'(walk_x_q);
  assign pw_y_o           = 16'(walk_y_q);
  assign pw_line_scroll_o = scroll_q;

  logic pw_fire_c;
  assign pw_fire_c = pw_valid_o && pw_ready_i;

  // The shadow: the pixel whose sample arrives next cycle.
  logic          sh_v_q;
  logic [XW-1:0] sh_x_q;
  logic [YW-1:0] sh_y_q;

  // ==========================================================================
  // ACCEPT AND ADDRESS  (stage 0)
  // ==========================================================================
  // The plane always wins. Its deadline is the compositor's raster and the
  // sprite has a handshake that can wait; inverting this would let a HUD
  // descriptor starve the atmosphere of the line it is about to need.
  logic take_plane_c, take_fill_c, take_sprite_c;
  assign take_plane_c  = pipe_en_c && pl_valid_i;
  assign take_fill_c   = pipe_en_c && !pl_valid_i && sh_v_q;
  assign take_sprite_c = pipe_en_c && !pl_valid_i && !sh_v_q && sp_valid_i;

  assign pl_ready_o = pipe_en_c;
  assign sp_ready_o = take_sprite_c;

  // ---- the binding -------------------------------------------------------
  // Plane: slot = role (0 BACKDROP, 1 ATMOSPHERE). Sprite: 4 + src_id[1:0].
  // Disjoint by construction, which is why a sprite cannot quietly sample the
  // sheet's page because two ids collided.
  logic [BSW-1:0] bsel_c;
  always_comb begin
    if (take_sprite_c) bsel_c = BSW'(4) + BSW'(sp_src_id_i[1:0]);
    else               bsel_c = BSW'(pl_role_i);
  end

  logic [PAW-1:0] base_c;
  logic [3:0]     lstride_c, lheight_c;
  logic           bind_ok_c;
  assign base_c    = bind_base_q[bsel_c];
  assign lstride_c = bind_lstride_q[bsel_c];
  assign lheight_c = bind_lheight_q[bsel_c];
  assign bind_ok_c = bind_prog_q[bsel_c];

  // ---- format ------------------------------------------------------------
  logic fmt565_c, fmt_bad_c;
  always_comb begin
    if (take_sprite_c) begin
      fmt565_c  = (sp_format_i == 3'd1);
      fmt_bad_c = (sp_format_i > 3'd1);
    end else begin
      fmt565_c  = pl_format_i;
      fmt_bad_c = 1'b0;
    end
  end

  // ---- the palette slot ---------------------------------------------------
  // Both producers carry an 8-bit palette id and this store holds PAL_SLOTS of
  // them. The upper bits are not ignored: a slot this instance does not own is
  // COUNTED and folded to slot 0, because silently aliasing palette 9 onto
  // palette 1 is the kind of wrong picture nobody can trace back to a number.
  logic [7:0]       pal_sel_c;
  logic             pal_bad_c;
  logic [PALSW-1:0] pal_slot_c;
  always_comb begin
    pal_sel_c  = take_sprite_c ? sp_palette_i : pl_palette_i;
    pal_bad_c  = (32'({24'd0, pal_sel_c}) >= 32'(PAL_SLOTS));
    pal_slot_c = pal_bad_c ? PALSW'(0) : pal_sel_c[PALSW-1:0];
  end

  // ---- the requested texel ------------------------------------------------
  // The sprite's UV is fx16; the fraction is DISCARDED here and stored
  // nowhere, which is what keeps "nearest only" structural on this side too.
  logic [15:0] req_u_c, req_v_c;
  always_comb begin
    if (take_sprite_c) begin
      req_u_c = sp_u_i[31:16];
      req_v_c = sp_v_i[31:16];
    end else begin
      req_u_c = pl_texel_u_i;
      req_v_c = pl_texel_v_i;
    end
  end

  // ---- mask to the page, count what the mask changed ----------------------
  // A power-of-two page makes REPEAT a mask, so there is no divider and no
  // "one correction is enough" assumption to violate. A caller whose page is
  // not that shape is counted, never silently tiled.
  logic [4:0]  uw_c;
  logic [15:0] umask_c, vmask_c, um_c, vm_c;
  logic        wrapped_c;
  always_comb begin
    uw_c    = fmt565_c ? 5'({1'b0, lstride_c}) : (5'({1'b0, lstride_c}) + 5'd1);
    umask_c = 16'((16'h0001 << uw_c) - 16'h0001);
    vmask_c = 16'((16'h0001 << lheight_c) - 16'h0001);
    um_c    = req_u_c & umask_c;
    vm_c    = req_v_c & vmask_c;
    wrapped_c = (um_c != req_u_c) || (vm_c != req_v_c);
  end

  // ---- the word address: shifts and adds, no multiplier -------------------
  // Widened to 32 bits on purpose. A masked v can be up to 32,767 and the row
  // shift can carry it past the store; truncating into the store's own address
  // width FIRST would make the out-of-range case wrap silently and the
  // detector below would then be reading an address that had already been
  // made legal. Compute wide, detect, then narrow.
  logic [31:0]    addr_sum_c;
  logic [PAW-1:0] addr_c;
  logic           oob_c;
  logic           bytesel_c;
  always_comb begin
    addr_sum_c = 32'(base_c)
               + (32'(vm_c) << lstride_c)
               + (fmt565_c ? 32'(um_c) : (32'(um_c) >> 1));
    oob_c      = addr_sum_c >= 32'(PAGE_WORDS);
    addr_c     = oob_c ? PAW'(0) : PAW'(addr_sum_c);
    bytesel_c  = um_c[0];
  end

  // ==========================================================================
  // STAGE 1 -- the page word is here
  // ==========================================================================
  logic [15:0]  page_rd_q;
  logic         a1_v_q, a1_dest_q, a1_fmt565_q, a1_bad_q, a1_bytesel_q, a1_present_q;
  logic [PALSW-1:0] a1_pal_q;
  logic [15:0]  a1_x_q, a1_y_q;
  logic [7:0]   a1_opacity_q;
  logic         a1_add_q;
  logic [1:0]   a1_blend_q;
  logic [15:0]  a1_tint_q;
  logic [7:0]   a1_order_q;
  logic [15:0]  a1_srcid_q;
  logic         a1_last_q;

  // ==========================================================================
  // STAGE 2 -- the palette entry is here
  // ==========================================================================
  logic [15:0]  pal_rd_q, a2_page_q;
  logic         a2_v_q, a2_dest_q, a2_fmt565_q, a2_bad_q, a2_present_q;
  logic [15:0]  a2_x_q, a2_y_q;
  logic [7:0]   a2_opacity_q;
  logic         a2_add_q;
  logic [1:0]   a2_blend_q;
  logic [15:0]  a2_tint_q;
  logic [7:0]   a2_order_q;
  logic [15:0]  a2_srcid_q;
  logic         a2_last_q;

  logic [PALAW-1:0] pal_addr_c;
  logic [7:0]       clut_idx_c;
  assign clut_idx_c = a1_bytesel_q ? page_rd_q[15:8] : page_rd_q[7:0];
  assign pal_addr_c = {a1_pal_q, clut_idx_c};

  logic [15:0] colour_c;
  always_comb begin
    if (!a2_present_q)     colour_c = 16'h0000;
    else if (a2_bad_q)     colour_c = REFUSED_RGB;
    else if (a2_fmt565_q)  colour_c = a2_page_q;
    else                   colour_c = pal_rd_q;
  end

  // ==========================================================================
  // THE RING WRITE
  // ==========================================================================
  logic            ring_we_c;
  logic [RAW-1:0]  ring_wa_c;
  logic [ASW-1:0]  wr_slot_c;
  assign ring_we_c = pipe_en_c && o_v_q && !o_dest_q;
  assign wr_slot_c = o_y_q[ASW-1:0];
  assign ring_wa_c = RAW'((32'(wr_slot_c) * 32'(LINE_W)) + 32'(o_x_q));

  // ==========================================================================
  // THE RING READ -- POST.COMPOSITE's convention, served by a plain
  // synchronous read. Nothing here looks at the pipeline's stall, and that is
  // the point: the compositor holds its own address, so this returns the same
  // word for as long as it does.
  // ==========================================================================
  logic [ASW-1:0] rd_slot_c;
  logic [RAW-1:0] ring_ra_c;
  logic           resident_c;
  assign rd_slot_c  = atm_req_y_i[ASW-1:0];
  assign ring_ra_c  = RAW'((32'(rd_slot_c) * 32'(LINE_W)) + 32'(atm_req_x_i));
  // The two sides of this comparison are clocked by different things: the tag
  // by this block's walk, the address by POST.COMPOSITE's raster pointer. A
  // fault that moved one would not move the other.
  assign resident_c = (slot_st_q[rd_slot_c] == SLOT_READY)
                   && (slot_y_q[rd_slot_c] == atm_req_y_i);

  logic [RINGW-1:0] ring_rd_q;
  assign atm_rgb_o     = ring_rd_q[15:0];
  assign atm_opacity_o = ring_rd_q[23:16];
  assign atm_add_o     = ring_rd_q[24];

  // `atm_en_o` is latched once per frame from whether the PREVIOUS frame
  // produced any atmosphere at all. Derived, not a port: a plane that is
  // disabled or masked out of this view emits no samples, so the sheet turns
  // itself off. One frame of latency, stated rather than hidden, and the
  // alternative -- a mid-frame enable -- would put a seam across the picture.
  logic        atm_any_q;

  // ==========================================================================
  // The one sequential block
  // ==========================================================================
  integer i;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      walk_x_q   <= '0;
      walk_y_q   <= '0;
      walk_run_q <= 1'b0;
      scroll_q   <= '0;
      rd_line_q  <= '0;
      sh_v_q     <= 1'b0;
      sh_x_q     <= '0;
      sh_y_q     <= '0;
      a1_v_q     <= 1'b0;
      a2_v_q     <= 1'b0;
      o_v_q      <= 1'b0;
      atm_valid_o<= 1'b0;
      atm_en_o   <= 1'b0;
      atm_any_q  <= 1'b0;
      for (i = 0; i < ATM_LINES; i = i + 1) begin
        slot_st_q[i] <= SLOT_EMPTY;
        slot_y_q[i]  <= '0;
      end
      for (i = 0; i < BIND_SLOTS; i = i + 1) bind_prog_q[i] <= 1'b0;
      samples_o        <= '0;
      plane_samples_o  <= '0;
      sprite_samples_o <= '0;
      clut8_samples_o  <= '0;
      rgb565_samples_o <= '0;
      texel_wrapped_o  <= '0;
      page_oob_o       <= '0;
      bind_missing_o   <= '0;
      fmt_refused_o    <= '0;
      pal_refused_o    <= '0;
      skipped_fill_o   <= '0;
      atm_underrun_o   <= '0;
      walk_stalls_o    <= '0;
      sprite_stalls_o  <= '0;
      tint_unapplied_o <= '0;
      pair_lost_o      <= '0;
    end else begin
      // ---- asset load ----------------------------------------------------
      if (ld_page_we_i) page_m[ld_page_addr_i] <= ld_page_data_i;
      if (ld_pal_we_i)  pal_m[ld_pal_addr_i]   <= ld_pal_data_i;
      if (ld_bind_we_i) begin
        bind_base_q[ld_bind_sel_i]    <= ld_bind_base_i;
        bind_lstride_q[ld_bind_sel_i] <= ld_bind_lstride_i;
        bind_lheight_q[ld_bind_sel_i] <= ld_bind_lheight_i;
        bind_prog_q[ld_bind_sel_i]    <= 1'b1;
      end

      // ---- the frame edge -------------------------------------------------
      if (frame_start_i) begin
        walk_x_q        <= '0;
        walk_y_q        <= '0;
        walk_run_q      <= (frame_h_i != 0) && (frame_w_i != 0);
        scroll_q        <= line_scroll_i;
        rd_line_q       <= '0;
        atm_en_o        <= atm_any_q;
        atm_any_q       <= 1'b0;
        for (i = 0; i < ATM_LINES; i = i + 1) slot_st_q[i] <= SLOT_EMPTY;
      end

      // ---- the walk -------------------------------------------------------
      if (pw_fire_c) begin
        if (walk_x_q + XW'(1) >= frame_w_i) begin
          walk_x_q <= '0;
          scroll_q <= line_scroll_i;     // latched once per line, never mid-line
          if (walk_y_q + YW'(1) >= frame_h_i) walk_run_q <= 1'b0;
          else                                walk_y_q   <= walk_y_q + YW'(1);
        end else begin
          walk_x_q <= walk_x_q + XW'(1);
        end
      end else if (walk_run_q && !slot_ok_c) begin
        walk_stalls_o <= walk_stalls_o + 32'd1;
      end

      if (pipe_en_c) begin
        sh_v_q <= pw_fire_c;
        sh_x_q <= walk_x_q;
        sh_y_q <= walk_y_q;
      end

      if (sp_valid_i && !sp_ready_o) sprite_stalls_o <= sprite_stalls_o + 32'd1;

      // ---- stage 0 -> 1 ---------------------------------------------------
      if (pipe_en_c) begin
        page_rd_q    <= page_m[addr_c];
        // A plane sample with no shadow behind it is DISCARDED rather than
        // written: its screen coordinate would be the previous pixel's, so
        // admitting it would put a colour in a cell nobody asked about. It is
        // counted in `pair_lost_o` below, which is the only place this
        // structural violation is visible.
        a1_v_q       <= (take_plane_c && sh_v_q) || take_fill_c || take_sprite_c;
        a1_dest_q    <= take_sprite_c;
        a1_fmt565_q  <= fmt565_c;
        a1_bad_q     <= fmt_bad_c;
        a1_bytesel_q <= bytesel_c;
        a1_present_q <= !take_fill_c;
        a1_pal_q     <= pal_slot_c;
        a1_tint_q    <= take_sprite_c ? sp_tint_i    : 16'hFFFF;
        a1_blend_q   <= take_sprite_c ? sp_blend_i   : pl_blend_i;
        a1_order_q   <= take_sprite_c ? sp_order_i   : 8'd0;
        a1_srcid_q   <= take_sprite_c ? sp_src_id_i  : 16'd0;
        a1_last_q    <= take_sprite_c ? sp_last_i    : 1'b0;
        a1_opacity_q <= take_plane_c  ? pl_opacity_i : 8'd0;
        // BLEND 2 is ADD; anything else composites as ALPHA. The mapping is
        // TWOD.PLANE's own (`0 REPLACE, 1 ALPHA, 2 ADD`) and POST.COMPOSITE's
        // `atm_add_i` is one bit, so this is a re-encoding of an existing
        // agreement, not a new one.
        a1_add_q     <= take_plane_c && (pl_blend_i == 2'd2);
        if (take_sprite_c) begin
          a1_x_q <= 16'(sp_x_i);
          a1_y_q <= 16'(sp_y_i);
        end else begin
          a1_x_q <= 16'(sh_x_q);
          a1_y_q <= 16'(sh_y_q);
        end

        // counting at the point of acceptance, where the fields are still the
        // request's own
        if (take_plane_c && !sh_v_q) pair_lost_o <= pair_lost_o + 32'd1;
        if (take_fill_c)             skipped_fill_o <= skipped_fill_o + 32'd1;
        if (take_plane_c || take_sprite_c) begin
          if (wrapped_c)  texel_wrapped_o <= texel_wrapped_o + 32'd1;
          if (oob_c)      page_oob_o      <= page_oob_o      + 32'd1;
          if (!bind_ok_c) bind_missing_o  <= bind_missing_o  + 32'd1;
          if (fmt_bad_c)  fmt_refused_o   <= fmt_refused_o   + 32'd1;
          if (pal_bad_c && !fmt565_c)
                          pal_refused_o   <= pal_refused_o   + 32'd1;
        end
        if (take_sprite_c && (sp_tint_i != 16'hFFFF))
          tint_unapplied_o <= tint_unapplied_o + 32'd1;
      end

      // ---- stage 1 -> 2 ---------------------------------------------------
      if (pipe_en_c) begin
        pal_rd_q     <= pal_m[pal_addr_c];
        a2_page_q    <= page_rd_q;
        a2_v_q       <= a1_v_q;
        a2_dest_q    <= a1_dest_q;
        a2_fmt565_q  <= a1_fmt565_q;
        a2_bad_q     <= a1_bad_q;
        a2_present_q <= a1_present_q;
        a2_x_q       <= a1_x_q;
        a2_y_q       <= a1_y_q;
        a2_opacity_q <= a1_opacity_q;
        a2_add_q     <= a1_add_q;
        a2_blend_q   <= a1_blend_q;
        a2_tint_q    <= a1_tint_q;
        a2_order_q   <= a1_order_q;
        a2_srcid_q   <= a1_srcid_q;
        a2_last_q    <= a1_last_q;
      end

      // ---- stage 2 -> out --------------------------------------------------
      if (pipe_en_c) begin
        o_v_q       <= a2_v_q;
        o_dest_q    <= a2_dest_q;
        o_rgb_q     <= colour_c;
        o_x_q       <= a2_x_q;
        o_y_q       <= a2_y_q;
        o_opacity_q <= a2_opacity_q;
        o_add_q     <= a2_add_q;
        o_blend_q   <= a2_blend_q;
        o_tint_q    <= a2_tint_q;
        o_order_q   <= a2_order_q;
        o_srcid_q   <= a2_srcid_q;
        o_last_q    <= a2_last_q;

        if (a2_v_q) begin
          samples_o <= samples_o + 32'd1;
          if (a2_dest_q) sprite_samples_o <= sprite_samples_o + 32'd1;
          else           plane_samples_o  <= plane_samples_o  + 32'd1;
          if (a2_present_q && !a2_bad_q) begin
            if (a2_fmt565_q) rgb565_samples_o <= rgb565_samples_o + 32'd1;
            else             clut8_samples_o  <= clut8_samples_o  + 32'd1;
          end
        end
      end

      // ---- the ring write and the line bookkeeping ------------------------
      if (ring_we_c) begin
        ring_m[ring_wa_c] <= {o_add_q, o_opacity_q, o_rgb_q};
        if (o_x_q == 16'd0) begin
          slot_st_q[wr_slot_c] <= SLOT_FILLING;
          slot_y_q[wr_slot_c]  <= o_y_q[YW-1:0];
        end
        if (o_x_q + 16'd1 >= 16'(frame_w_i)) begin
          slot_st_q[wr_slot_c] <= SLOT_READY;
          slot_y_q[wr_slot_c]  <= o_y_q[YW-1:0];
        end
        if (o_opacity_q != 8'd0) atm_any_q <= 1'b1;
      end

      // ---- the ring read --------------------------------------------------
      ring_rd_q   <= ring_m[ring_ra_c];
      atm_valid_o <= atm_req_v_i && resident_c;
      if (atm_req_v_i) begin
        rd_line_q <= atm_req_y_i;
        if (atm_en_o && !resident_c) atm_underrun_o <= atm_underrun_o + 32'd1;
      end
    end
  end

endmodule : zhao_twod_sampler

`default_nettype wire
