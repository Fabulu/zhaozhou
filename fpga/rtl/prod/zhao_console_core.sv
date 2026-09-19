// zhao_console_core.sv -- THE CONNECTED MACHINE.
//
// Not a census. `zhao_prod_top` is the census and stays one: it puts blocks
// side by side under independent LFSR stimulus and is labelled
// RESOURCE_CENSUS_DISCONNECTED for exactly that reason. This file is the other
// thing the completion plan asks for --
//
//   reports/Zhaozhou_True_Console_Completion_Plan_2026-09-18.txt S1.3:
//     "zhao_console_core: the connected machine, with platform transaction
//      interfaces."
//
// and S13.3's warning is the reason it had to be a NEW file rather than a
// re-labelled old one: "a resource-top selection change alone does not close
// P2."
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE IS, IN ONE SENTENCE
// ---------------------------------------------------------------------------
// `zhao_shell_top_v2` -- which already wires a real console (CMD front end,
// MEM, VIDEO, INPUT, AUDIO, DEBUG, and the geometry/raster front door) -- with
// the organs that were BUILT AND ADOPTED NOWHERE joined to it and to each
// other: the four particle blocks as one ring, the geometry group sequencer
// closed onto the shared projector it was written to drive, the compositor and
// the measurement histogram on the shell's real frame boundary.
//
// It does NOT rewrite the shell. Plan S14.4 priority 8: "keep existing hardware
// that already meets its contract rather than rewrite everything to a new
// generic framework." Every one of the shell's 217 ports is carried straight
// through, declaration and comment verbatim, so the sibling shell remains the
// authority on its own seams and this file adds only what it composes.
//
// SEEDED ONCE, THEN HAND-MAINTAINED, which is the same standing
// `zhao_shell_top_v2.sv` has and for the same reason: 217 port names carried
// across mechanically cannot be retyped without a swapped name, and a generator
// kept alive afterwards would be the stale-generated-file trap. Nothing
// regenerates this file. If the shell's port list changes, the compiler says
// so -- `--top-module zhao_console_core` reports PINMISSING, which is a gate,
// not a guess.
//
// ---------------------------------------------------------------------------
// WHAT IS GENUINELY CONNECTED HERE -- producer -> consumer, no stimulus
// ---------------------------------------------------------------------------
// The plan's standard is quoted in full because it is the whole point:
//
//   "The simulation harness supplies external clocks, input events, memory
//    behavior and host packets. It does NOT supply missing lighting, FIELD
//    results, particle updates, prepared triangles or fake material records."
//
// So nothing below is an LFSR, a constant pattern or a counter pretending to be
// a workload. These are real wires between real blocks:
//
//   1. THE PARTICLE RING (four blocks, one closed loop)
//        PART.STATE.prt      -> PART.UPDATE.in
//        PART.UPDATE.out     -> PART.COLLIDE.p   (record AND its three events)
//        PART.COLLIDE.c      -> [fork] -> PART.STATE.vrd (record AND survive)
//                                      -> PART.SPAWN.par (post-contact record,
//                                         the four events with COLLISION filled
//                                         in, and the parent ordinal)
//        PART.SPAWN.chl      -> PART.STATE.chl
//      The fork sits AFTER PART.COLLIDE, not after PART.UPDATE: owner ruling
//      2026-09-19 (`reports/RULING-I4-COLLISION-SPAWN-20260919.md`). With
//      PART.SPAWN in parallel, it saw the particle before the only block that
//      knows whether it hit anything, and spawn-on-collision was dead.
//      The fork is real glue and is documented at its declaration; it is the
//      only arithmetic-free thing standing between two real ports.
//
//   2. THE FRAME BOUNDARY IS THE PARTICLE TICK
//        SHELL.gpu_tick_o           -> PART.STATE.tick_start_i
//                                   -> PART.SPAWN.tick_start_i
//        SHELL.gpu_tick_frame_id_o  -> PART.SPAWN.tick_i
//      The particle generation advances on the console's real frame edge, not
//      on a testbench pulse.
//
//   3. THE GEOMETRY CLIENT, CLOSED ONTO ITS SERVICE
//        GEOM.SKIN.o          -> GEOM.GROUP_SEQ.v
//        GEOM.GROUP_SEQ.a     -> PROJ_SUBSYSTEM client A
//        PROJ_SUBSYSTEM.a_*_o -> GEOM.PROJ_LANE (the arena fill)
//        GEOM.PROJ_LANE       -> GEOM.GROUP_SEQ  (open_gen, rider_payload,
//                                fill_landed/fill_arena -- the seal evidence)
//      `design/prod_manifest.yml` records why the shared projector could not be
//      selected: "the producer is still absent, so this is not yet adoptable on
//      its own". `zhao_geom_group_seq` IS that producer and this is the first
//      composition in which it drives the thing it was written for.
//
//   3b. THE TERRAIN CLIENT, ON THE SAME PROJECTOR
//        TERRAIN.GROUP_SEQ.t_job -> TERRAIN.TESS (mode 1, then mode 2)
//        TERRAIN.TESS.vtx/ref    -> TERRAIN.GROUP_SEQ
//        TERRAIN.GROUP_SEQ.b     -> PROJ_SUBSYSTEM client B
//        TERRAIN.GROUP_SEQ.open/seal/r -> the same subsystem's arena and
//                                replay shell; landings come back on
//                                `fill_landed_o`.
//      This is the point of a SHARED projector and it had never been measured:
//      until this composition client B was on pins, so the arbiter always
//      granted client A, `proj_contended_o` could not move, and the block's
//      whole justification -- one projector serving two clients -- was an
//      argument rather than a circuit. `zhao_terrain_pipe.sv` composes the
//      same trio around its OWN projector instance and is deliberately not
//      used, because that would put two projectors in the core.
//
//   4. THE COMPOSITOR ON THE REAL VIDEO MODE
//        SHELL.gpu_tick_o -> POST.COMPOSITE.frame_start_i
//        SHELL.mode_act_o -> the pass width/height (Z60 384x240, Storm 320x240,
//                            Duo 256x192 per VIEW -- zhao_pkg's own numbers)
//
//   5. THE MEASUREMENT INTERVAL IS THE FRAME
//        SHELL.gpu_tick_o -> MEASURE.HISTOGRAM.snapshot_i
//
//   6. THE VERTEX DECODER FEEDS THE SKINNER, THROUGH THE PALETTE STORE
//        GEOM.VDECODE.d_{x,y,z,w0,rigid,src_id,bone0,bone1}
//                              -> GEOM.POSE's palette store -> GEOM.SKIN.v_*
//        GEOM.POSE.decode.out_{valid,bone,m[12]}
//                              -> GEOM.POSE's palette store, write port
//        palette store.{a_m_o, b_m_o} -> GEOM.SKIN.{a_m_i, b_m_i}
//      Name for name, width for width, no arithmetic between them. Until this
//      composition GEOM.SKIN's vertex port was a boundary (entry I10) and the
//      only thing that had ever driven it was a harness. It now has its real
//      producer, and the decoder's REFUSAL path (`d_refused_o` and its three
//      causes) leaves this module rather than being dropped, because a vertex
//      the decoder rejected must not look like a vertex that never arrived.
//
//      ENTRY I10 IS CLOSED AND DELETED, 2026-09-19. Its remaining half was the
//      BONE MATRICES, and it was a missing BLOCK rather than missing wiring:
//      `zhao_geom_pose_decode` streams the palette one bone per beat and
//      GEOM.SKIN wants two whole matrices latched with the vertex, so a stream
//      and a random access did not meet. `zhao_geom_pose_palette` is that
//      block -- it stores the decoded palette in RAM, answers two reads per
//      vertex in seven clocks (under GEOM.SKIN's own issue interval of 12), and
//      substitutes the identity bind pose for a bone that is out of range or
//      not yet decoded rather than reading whatever the memory held.
//
//      The gap that remains is ONE LEVEL UP and is named separately: the
//      decoder's own clip page and skeleton bake have no producer. That is
//      entry I29, and it is a narrower and more honest statement than I10 was.
//
//   7. THE TRIANGLE FRONT DOOR, CLOSED WITH ITS REAL PRODUCER
//        GEOM.CLIP.out_*  -> GEOM.SETUP.tri_*
//        GEOM.SETUP.out_* -> SHELL.render_tri_* (the raster door)
//      This is the seam entry I13 said was missing, from the other side. The
//      shell's triangle port takes EDGE FUNCTIONS -- `render_kx0_i` and its
//      siblings -- and `zhao_geom_setup` emits exactly those, port for port and
//      width for width, together with the triangle corners, the top-left mask,
//      the scan box and the source id. It is the producer, and the shell's own
//      source list records that it "starts at the binner and has no vertex
//      front end at all" precisely because this block was never composed.
//      Nothing is renamed and nothing is computed here: the refusal recorded at
//      I13 was against renaming CORNERS into edge functions, and this is not
//      that -- it is the block whose job is to derive them, doing it.
//      GEOM.CLIP's own input is still a boundary (I24).
//
//   8. THE TERRAIN PAGING SPINE -- five blocks in one chain
//        TERRAIN.CMD.fr_*/rec_*  -> TERRAIN.SEQ.fr_*/rec_*
//        TERRAIN.SEQ.lu_/cl_/pin_ <-> TERRAIN.RESIDENCY (the v2 directory)
//        TERRAIN.SEQ.ld_*        -> TERRAIN.LOADQ.j_*
//        TERRAIN.LOADQ.q_*       -> TERRAIN.PAGELOADER.j_*
//        TERRAIN.PAGELOADER.fin_* -> TERRAIN.RESIDENCY.fin_*
//      Every seam is name for name and width for width. Nothing is renamed,
//      nothing is computed, and the ONE place two clients meet is the real
//      `zhao_hps_arbiter` -- a block with a contract and a documented fairness
//      rule, instantiated rather than imitated. There is no arbiter, mux or
//      state machine that this file invented anywhere in the spine.
//
//      THE DIRECTORY IS `zhao_terrain_residency_v2`, AND THAT IS THE LEDGER'S
//      CHOICE RATHER THAN THIS FILE'S. `design/blocks.yml`'s TERRAIN.RESIDENCY
//      row says so three independent ways: its purpose line ends "the
//      direct-mapped prototype is superseded because two islands may legally
//      overlap in local patch coordinates"; its `tests:` are
//      `terrain_residency_v2_directed.cpp` and `terrain_residency_v2_random.cpp`;
//      and its UNIT_VERIFIED maturity evidence is the v2 directed test.
//      `zhao_terrain_residency.sv` -- the v1 file -- opens with "FIRST BLOCK OF
//      THE WORLD LAYER. Nothing instantiates it yet." `zhao_terrain_seq`'s own
//      header names v2 by file when it says where its SEQW comes from, and v1's
//      ports do not match it: v1 keys on {px, py} alone, which is exactly the
//      overlap defect that row records as superseded.
//
//      WIDTHS: TERRAIN.SEQ and the directory share a TERR_SLOTW = 10 handle
//      ($clog2(256 sets x 4 ways)). TERRAIN.PAGELOADER carries the POOL index,
//      one bit wider on purpose. The step is crossed in exactly two places,
//      both annotated at the instances, and the return leg is QUALIFIED rather
//      than narrowed -- a completion whose extra bit is set is not offered,
//      because passing the low ten bits back would alias slot 1,024 onto slot 0
//      and publish one patch's page under another patch's key.
//
//      WHAT THIS SPINE DOES NOT YET DO, stated so nobody reads more into it:
//      a page it loads reaches the directory's ST_MIPGEN state and stops
//      there. TERRAIN.RESIDENCY publishes on TWO completions -- a claim sets
//      `mips_stale` and only a second `fin` reaches RESIDENT_CLEAN, the only
//      state a lookup hits on. The second completion's producer is
//      TERRAIN.MIPFEED, which is not composed (see the refusal list at the
//      instances). So `terr_res_resident_o` staying at zero while
//      `terr_pl_pages_loaded_o` climbs is the EXPECTED reading of this
//      composition, not a defect in it -- and it is written down here because
//      that exact pairing was once measured and mistaken for one.
//
//   9. THE SCAR SUBSTRATE AND THE ENGINE THAT WRITES IT -- a closed pair
//        SURFACE.STAMP.req_* -> SURFACE.SHEET.req_*  (ACQUIRE, then READ)
//        SURFACE.SHEET.pg_*  -> SURFACE.STAMP.pg_*   (status + pre-blend F)
//        SURFACE.STAMP.wr_*  -> SURFACE.SHEET.wr_*   (the blended texel)
//      Port for port and width for width, three channels, nothing renamed and
//      nothing computed between them. `spec/terrain_rules.md` 7 says layer F
//      is "written only by SURFACE.STAMP", so this is not two blocks that
//      happen to fit: it is the only pairing the law permits, and SURFACE.SHEET
//      had never had its single client attached.
//
//      THE STORE IS REAL AND ON CHIP -- SURF_SLOTS x 65,536 bits of M10K --
//      so this is NOT the refusal at I23: nothing behind it waits on a
//      behavioural SDRAM model that this tree does not have.
//
//      AND IT IS AN ISLAND, which is stated here rather than left to be
//      discovered. Nothing in the console causes a stamp; the dispatch arrives
//      at this module's edge because CMD.SCHEDULER has no SurfaceStamp path
//      (entry I30). The traffic BETWEEN the two blocks is real and the traffic
//      INTO the pair is a boundary, and those are different claims.
//
// ---------------------------------------------------------------------------
// INCOMPLETE -- TIED OFF, AND WHY
// ---------------------------------------------------------------------------
// A hidden tie-off is the failure this file exists to stop, so every one of
// them is here, by name, with the owner that is missing. "Boundary" means the
// signal leaves this module as a port: the harness or the board top drives it,
// and it is NOT constant-folded away -- which matters, because a constant on a
// wide data input deletes the logic behind it and produces a resource number
// that is confidently too small.
//
// CLOSED AND DELETED -- the ledger of numbers this block no longer carries.
//
// IT LIVES HERE, ABOVE THE FIRST ENTRY, AND THE PLACEMENT IS LOAD-BEARING.
// `tools/budget/completion_register.py` parses this block by matching `// I<n>.`
// and attaching EVERY following comment line to that entry until the next one.
// So a note about a DELETED entry, left where the entry used to be, is read as
// part of the PRECEDING LIVE ENTRY'S text -- and the register classifies an
// entry by keyword. The I4 note below contains the words "tied to zero"; while
// it sat between I3 and I5 it made I3 read `tied-to-zero` when I3 was a
// boundary, and moving the note without moving it far enough would simply have
// transferred that misreading to I1. Prose before the first entry is attached
// to nothing, which is exactly what a closed entry's record should be.
// (Found on 2026-09-19 while closing I2/I3; the misclassification of I3 in
// every register run before that date is this, and it changed the KIND column
// only -- `mandatory_gap` is true for both kinds, so no total was ever wrong.)
//
//  * I2 was THE SPECIES DESCRIPTOR TABLE and I3 the SIZE/COLOUR CURVE TABLE,
//    both boundaries because four contracts handed the table to each other and
//    none implemented it. BOTH ARE CLOSED, 2026-09-19, and both entries are
//    DELETED rather than marked closed, for the reason I4 gives below.
//
//    The owner is `zhao_part_table` (PART.TABLE). It had been BUILT on
//    2026-09-19, named for these entries, and instantiated NOWHERE -- the
//    uncashed cheque CLAUDE.md has a chapter about, with this header still
//    telling the next reader to build it again. It is instantiated at
//    `u_part_table` below and serves all four reads: PART.UPDATE's species
//    descriptor and its size/colour curve, PART.COLLIDE's four coefficients,
//    PART.SPAWN's child rule. Twenty-five ports left this module's port list
//    rather than being driven.
//
//    THE ONE OBSTACLE WAS AN ADDRESS, NOT A TABLE. `zhao_part_collide` read its
//    descriptor combinationally and EMITTED NO INDEX -- it decoded the species
//    into an internal `u_spc` and kept it -- so `c_index_i` had no driver, and
//    the only composer-side answer was a SECOND `zhao_part_record` instance
//    here to re-slice a field the consumer had already read. That is a block
//    added by the composer and a second decode of a frozen layout. The fix is
//    `zhao_part_collide.d_index_o`, one output on the block that already holds
//    the value, added in the same pass.
//
//    The timing was NOT the hazard it looks like, and the deleted entry had
//    already written down why: the collider's descriptor read is combinational
//    off `p_record_i`, so an index taken from the same wire in the same instant
//    cannot swap against it. No register was added, and adding one is what
//    WOULD have created the hazard.
//
//    The successor gap is I33, one level down: the table exists and answers,
//    and nothing fills it.
//
//  * I4 was PART.UPDATE's step-6 collision response, tied to zero. It was a
//    closed contradiction between three ratified contracts rather than a
//    wiring gap, and owner ruling 2026-09-19 settled it:
//    `reports/RULING-I4-COLLISION-SPAWN-20260919.md`. The four `col_*_i` ports
//    are RETIRED, PART.COLLIDE emits the collision event it already owned, and
//    a collision-spawned child is placed POST-CONTACT. The entry is deleted
//    rather than marked closed, because this block is what the completion
//    register counts and a stale closed entry under-reports progress exactly as
//    deleting an open one would over-report it. The closure is recorded at the
//    instances below and in the ruling.
//    (Moved here from between I3 and I5 on 2026-09-19, unedited, when I2/I3
//    were deleted -- see the placement note above for why it could not stay.)
//
//  * I10 was GEOM.SKIN's BONE MATRICES; its record is still inline between I9
//    and I11 and is left there. It is the same shape as the two above and
//    SHOULD move here, but I9 is `NOT a tie-off` and that keyword wins over
//    every other, so the stray note changes nothing today. Named so the next
//    reader knows it was looked at rather than missed.
//
//  I1. PART.STATE's generation store (`part_rd_*`, `part_wr_*`) -- BOUNDARY.
//      The plan lets the harness supply "memory behavior", and this is that.
//      But the real provider is named nowhere: MEM.HPS.BRIDGE is instantiated
//      inside the shell and has no particle client port, so no route from this
//      store to that bridge exists yet. Connecting them is a shell change.
//
//
//  I5. PART.UPDATE's field sample (`part_fld_*`) -- BOUNDARY. FIELD.SEQ.FLOW
//      is not composed; `zhao_field_seq.sv` exists but exposes no bounded
//      acceleration sample of this shape.
//
//  I6. PART.COLLIDE's terrain sample (`part_ter_*`) -- BOUNDARY.
//      `zhao_terrain_patch.sv` exists and is the named owner, but it emits
//      heights (top/bottom/compose_top) and NO SURFACE NORMAL, and the
//      collision test needs {height, nx, ny, nz}. Wiring height alone and
//      inventing a normal would be the hidden-adapter failure.
//
//  I7. PART.COLLIDE's plane (`part_plane_*`) -- BOUNDARY. A per-frame owner
//      value by the block's own design; CMD.SCHEDULER has no path to it.
//
//  I9. PART.SPAWN's parent id (`par_id_i`) -- NOT a tie-off: the core assigns
//      it. The particle128 record (amendment C2) carries no id field, so the
//      only identity available is the particle's ORDINAL within the
//      generation, and this file counts it. Listed here because it is a
//      decision taken in the composer: if ids must survive compaction, a real
//      PART.ID owner is needed and this counter is wrong.
//
//      (I10 was GEOM.SKIN's BONE MATRICES. It is CLOSED and the entry is
//      DELETED, 2026-09-19. The vertex half closed when `zhao_geom_vdecode`
//      was composed; the palette half was a MISSING BLOCK rather than missing
//      wiring, and `zhao_geom_pose_palette` is that block -- GEOM.POSE's
//      decoded palette in RAM, two reads per vertex, the vertex passed through
//      beside its two matrices. `geom_skin_a_m_i` / `geom_skin_b_m_i` are gone
//      from the port list rather than driven, and `geom_vd_bone0_o` /
//      `geom_vd_bone1_o` went with them because they now have a consumer.
//      Deleted rather than marked closed, for the reason I4 gives above: a
//      stale closed entry under-reports progress exactly as deleting an open
//      one would over-report it. The successor gap is I29, one level up.)
//
// I11. GEOM.GROUP_SEQ's job port and its sealed-group output (`geom_job_*`,
//      `geom_grp_*`, `geom_rel_*`) -- BOUNDARY.
//      CORRECTED 2026-09-19. This entry used to say "the replay customer is
//      GEOM.SETUP". THAT IS WRONG, and it was worth the five minutes to check,
//      because acting on it would have produced exactly the hidden adapter I13
//      refuses. `zhao_geom_setup`'s input port is a triangle of THREE SCREEN
//      VERTICES with a signed 2A and a scissored scan box -- it is GEOM.CLIP's
//      output, and GEOM.CLIP is composed below to drive it. A sealed-group
//      handle {arena, generation, count, view, src_id} is not that and cannot
//      be turned into it by naming.
//
//      THE REPLAY CUSTOMER IS A BLOCK THAT DOES NOT EXIST. What it must do is
//      now fully determined by the two ports either side of it, so this is a
//      specification rather than a guess:
//        * take a sealed handle here and a TriangleDescriptor {v0,v1,v2} from
//          GEOM.ASSEMBLE;
//        * issue THREE lookups on GEOM.PROJ_LANE (`look_arena/look_gen/
//          look_index`, entry I12) and collect three replies;
//        * slice each 106-bit reply -- the layout is exactly
//          {x[20:0], y[20:0], d[31:0], w[30:0], behind}, which is 106 -- into
//          the corner and its behind bit;
//        * present {ax,ay,bx,by,cx,cy,behind[2:0]} to GEOM.CLIP (entry I24);
//        * pulse `rel_valid_o` back here when the group is done with.
//      The slicing is field routing. The THREE-LOOKUP SEQUENCING IS NOT: it is
//      a state machine with a reply join, and a state machine belongs in a
//      file with a contract and a test, not in this composer. It is also NOT
//      GEOM.LOOM (that is SKIN -> WARP deformation, and it is in the register's
//      NOT-BUILT list for its own reasons).
//
// I12. GEOM.PROJ_LANE's lookup/reply and arena origin (`geom_look_*`,
//      `geom_rep_*`, `geom_org_*`) -- BOUNDARY, same absent customer as I11,
//      and see the specification written there.
//
// I13. PROJ_SUBSYSTEM's TRIANGLE OUTPUT (`proj_out_*`) -- BOUNDARY.
//      CLIENT B and the reference port are CLOSED: `zhao_terrain_group_seq`
//      and `zhao_terrain_tess` are composed below and drive both, so the
//      shared projector is measured here with BOTH of its clients live and
//      `proj_contended_o` can move. What remains is the far end: the replayed
//      triangle's customer is GEOM.SETUP, which is not composed, and the
//      shell's own triangle door (`render_kx0_i` and its siblings) takes EDGE
//      FUNCTIONS -- setup's arithmetic, not a rename of these corners. Wiring
//      the corners into a port that wants edge functions would be exactly the
//      hidden adapter this file must not contain.
//
// I14. PROJ_SUBSYSTEM's matrix bank (`proj_cfg_*`, `proj_en_i`) -- BOUNDARY,
//      and HALF CLOSED 2026-09-19. The entry stays open, and the half that
//      closed is named here so nobody re-solves it.
//
//      CLOSED: THE CAMERA. This entry used to read "the camera matrices are
//      host/CMD state and CMD.SCHEDULER has no projection-config path". They
//      have one now, and it is not CMD.SCHEDULER: `zhao_cmd_exec` (section 7c)
//      lowers `SetView 0x0010`'s `mat4fx view_projection` straight onto cfg
//      addresses 0..15, one word per clock, out of a packet CMD.DECODER has
//      already ratified. `cmd_exec_views_o` counts it.
//
//      STILL OPEN, and it is two different things:
//        * THE VIEWPORT RECT, cfg addresses 16 and 17. SetView carries a
//          `viewport_id` and NOT a rectangle, and the id-to-rectangle table is
//          `spec/video_rules.md`'s -- it is not in the ABI at all. Deriving one
//          here would be this file inventing a layout, which is the thing the
//          whole ledger exists to refuse. The host port keeps them and the
//          merge above is lossless in both directions, so this is a missing
//          COMMAND rather than missing wiring.
//        * `proj_en_i`, and `SetView`'s OTHER FIVE FIELDS. `flags[1:0]` is the
//          depth profile of the frozen 2026-08-31 ruling and `zhao_project_core`
//          has no port to put it on; `pixel_error` wants MEASURE.GOVERNOR and
//          `geometry_tokens`/`fragment_tokens` want MEASURE.TOKENS, none of
//          which is composed. A ratified field with no port is still a gap.
//
// I15. POST.COMPOSITE's SOURCE PIXELS (`post_s_*`) -- BOUNDARY, and this is
//      the largest honest gap in the file.
//
//      CORRECTED 2026-09-19. This entry used to read "RASTER.RESOLVE's output
//      is INTERNAL to `zhao_geom_bin_pipe_v2` ... interposing it is a change to
//      ... `zhao_geom_bin_pipe_v2`'s port list". THAT IS FALSE, and it sent the
//      next reader at the wrong file. The resolved stream is NINE PORTS on that
//      module -- `fb_valid_o`/`fb_ready_i`/`fb_rgb565_o`/`fb_tag_o`/
//      `fb_addr_o`/`fb_x_o`/`fb_y_o`/`fb_last_o`/`fb_src_id_o`, under its own
//      comment "Resolved framebuffer stream". No bin-pipe port change is needed
//      for anything. What is internal is one level up: `zhao_shell_top_v2`
//      carries it on the `rpx_*` wires from `u_render_bin` straight into
//      `u_render_fbw` and re-exports none of it, so the shell has no pixel-
//      stream port -- but that is an ordinary port addition on a file this
//      packet may edit, and it is NOT why the compositor is unconnected.
//
//      THE REAL OBSTACLE IS ORDER AND DENSITY, AND IT IS NOT WIRING.
//
//        * `post_s_*` IS ADDRESSLESS. `zhao_post_composite` has no source
//          coordinate port; it derives one from `x_in_q`/`y_in_q`, counters
//          that step on every accepted pixel and wrap at `frame_w_i` and
//          `frame_h_i`. The Nth pixel it accepts IS, by construction, frame
//          pixel (N mod W, N div W). Its nine-line ring is built on that: the
//          write pointer and the read pointer walk the same raster at the same
//          rate, which is the whole LAG_PX argument in that file's header.
//        * RASTER.RESOLVE IS TILE-ORDERED. It resolves "one finished 16x16
//          tile"; `fb_addr_o` is `{row[3:0], col[3:0]}` WITHIN the tile and
//          `zhao_raster_tile_pipe_v2` forms the surface coordinate as tile
//          origin plus that. So stream pixel 17 is frame (0,1) of one tile
//          while the compositor's counter is at (16,0) of the frame. Feeding
//          one to the other scrambles every pixel after the first sixteen, and
//          it does so with every handshake legal, `s_ready_o`/`fb_ready_i`
//          balanced, and `output_writes_o` counting a full frame. This is the
//          plausible wrong picture with a clean instrument beside it, which is
//          the failure this file's rules are written about.
//        * AND IT IS SPARSE. `resolve_start_w` fires only out of `RS_SWAP`,
//          which a tile reaches only after the binner hands it a job sequence.
//          A tile no triangle touches is never resolved and emits no pixels at
//          all, while the compositor waits for exactly `frame_w * frame_h` of
//          them before `o_last_o`. On any frame that is not fully covered the
//          block would simply never finish a pass.
//        * `zhao_raster_fbwrite` EXISTS BECAUSE THE STREAM IS SCATTERED. It
//          recomputes a VRAM address per tile row from `px_x_i`/`px_y_i` and
//          raises `stream_error_o` if a pixel is not its predecessor's
//          successor within a row. A dense raster stream would need neither.
//
//      SO THE REORDER BUFFER BETWEEN THE TWO IS A WHOLE FRAME, and the frame
//      store that already exists is the framebuffer. That is exactly what
//      POST.COMPOSITE.md means by "an exclusive framebuffer read/write lease
//      after resolve and before publication": post reads the COMPLETED
//      framebuffer back in raster order. Interposing before FBWRITE is not a
//      cheaper version of that arrangement, it is a different and wrong one.
//
//      WHAT IS MISSING IS THEREFORE THREE THINGS, NAMED SO THE NEXT PACKET
//      DOES NOT GO LOOKING FOR A PORT:
//        1. A RASTER-ORDER FRAMEBUFFER READ MASTER in the gpu-clock domain,
//           AND IT IS NOT A NEW INVENTION -- searched, and this entry is
//           narrower than it first said. `zhao_scanout_fetch` is already
//           exactly that shape: its own header says "gpu domain", it reads a
//           slot READ-ONLY through MEM.GUARD in 64-B bursts, one display line
//           at a time, in raster order. Three things make it not droppable in
//           as it stands, and all three are bounded:
//             - it fetches `display_slot_sync`, the DISPLAYED slot, and post
//               must read the BACK buffer. Pointing it at the front one is the
//               use `zhao_post_composite`'s own header forbids by name --
//               "NEVER POST-PROCESS THE CURRENTLY SCANNED-OUT FRONT BUFFER";
//             - it re-arms on `dec_sync`/`frame_start_sync`, the VIDEO
//               raster's swap decision, not on render drain -- which is item 3;
//             - its output is 64-bit beats into `zhao_scanout_linebuf`, while
//               `s_rgb_i` wants one RGB565 at a time. That unpack is a
//               byte-lane split under `spec/video_rules.md` 3 (little-endian
//               halfwords, row-major, no row padding), NOT a colour law and not
//               arithmetic this file would be inventing.
//           So the source side is a re-armed variant of a block that exists,
//           not a block nobody has written. It is still a MODULE and a memory
//           client, which is why it is not done here as wiring.
//        2. A MEMORY IDENTITY FOR A THIRD FRAMEBUFFER AGENT, AND THIS IS THE
//           REAL BLOCKER once item 1 is read properly. The shell's lease is ONE
//           BIT -- `fb_writer_i`, 0 = DEBUG.FRAMEBLIT, 1 = RASTER.FBWRITE --
//           and `zhao_mem_guard` passes on that bit. Post is a reader AND a
//           writer inside one frame. `zhao_vram_arbiter` builds the
//           controller's client tag by CASTING THE SLOT INDEX, so the client is
//           POSITIONAL, not configurable, and the shell's five slots are all
//           spoken for: 0 SCANOUT, 1 BLIT_DMA, 2 ENGINE0 (render), 3 ENGINE1
//           (geometry fetch), 4 DEBUG. There is no index post could present
//           that both the guard admits and the arbiter would carry -- exactly
//           the refusal `zhao_shell_top_v2` already writes out at its
//           `client_req[3]` note for a second geometry fetcher. That is a
//           decision for the memory rules, not for this file.
//        3. A FRAME-COMPLETION EVENT to start the pass on. `frame_start_i` is
//           `core_tick_c` here, which is the console tick, not render drain.
//
//      REFUSED 2026-09-19, and this is the fourth refusal of this seam. The
//      earlier three were right to refuse and gave a reason that pointed at the
//      wrong file; the reason above points at the right ones.
//      AND `reports/APPROACH-CORRECTION-20260919.md` SHOULD BE READ WITH THIS.
//      Its root-cause list names "RESOLVE internal to `zhao_geom_bin_pipe_v2`
//      -- blocks the whole post path". That sentence is the one corrected
//      above: RESOLVE's output is nine ports, the internality is one level up
//      and is a port addition, and the thing that actually blocks the post path
//      is the order/density mismatch plus item 2. Same document's rule, applied
//      to this entry: a refusal must name what it searched, so item 1 names
//      `zhao_scanout_fetch` and says what is and is not missing about it rather
//      than claiming nothing exists.
//
//
// I16. POST.COMPOSITE's output and echo tap (`post_o_*`, `post_echo_*`) --
//      BOUNDARY, the other end of I15. The output end is the SMALLER half and
//      it is still blocked by I15's item 2: `o_x_o`/`o_y_o`/`o_last_o` are
//      shaped exactly like `zhao_raster_fbwrite`'s `px_x_i`/`px_y_i`/
//      `px_last_i`, so the composited stream has a writer the moment post has a
//      lease -- and until then, wiring it to the EXISTING `u_render_fbw` would
//      put two producers on one framebuffer writer, which is worse than the
//      gap.
//
// I17. POST.COMPOSITE's gather planes, HUD, grading table, flash and ink
//      (`post_gd_*`, `post_gg_*`, `post_hud_*`, `post_pv_*`, `post_bias_*`,
//      `post_flash_*`, `post_ink_*`, `post_bloom_gain_i`) -- BOUNDARY. The
//      grading curves are generated ASSETS by design, so their load port is
//      legitimately external; the plane ports are not, and are a gap.
//
//      THE ATMOSPHERE SHEET IS NO LONGER IN THIS LIST. `post_atm_*` is GONE
//      FROM THIS MODULE'S EDGE as of 2026-09-19: TWOD.PLANE, TWOD.SPRITE and
//      the new `fpga/rtl/compositor/zhao_twod_sampler.sv` are composed at the
//      end of this file and the sheet never leaves. What follows is the record
//      of why that took three refusals, because one of the three reasons was
//      wrong and the wrong one is the one that did the refusing.
//
//      CORRECTED 2026-09-19 (first pass), and the correction matters because
//      acting on the old sentence would have sent somebody to build a block
//      that exists. This entry used to read "TWOD.PLANE and the HUD source are
//      not built". BOTH ARE BUILT AND BOTH ARE TESTED:
//      `fpga/rtl/compositor/zhao_twod_plane.sv` and `zhao_twod_sprite.sv`,
//      with `tests/compositor/twod_plane_directed.cpp`, `twod_plane_random.cpp`,
//      `twod_sprite_directed.cpp` and `twod_sprite_random.cpp`.
//
//      CORRECTED AGAIN 2026-09-19 (second pass), and this one removes work
//      rather than adding it. The entry then said the missing piece was "a
//      CLUT8/RGB565 sampler with a page store behind it. Nothing in `fpga/rtl`
//      is that", and added that "inventing the CLUT lookup would additionally
//      be inventing a colour law". HALF OF THAT WAS RIGHT AND THE HALF THAT
//      WAS WRONG IS WHAT KEPT THE SEAM SHUT.
//
//        * RIGHT: there is no TEXEL PAGE STORE in the tree that a (u, v) can
//          walk into without a VRAM fill agent. `zhao_texture_cache` is 1 KiB
//          of line cache with a MANDATORY `fill_*` port; `zhao_texture_tmu_pipe`
//          has the exactly right request and response shape and its `cac_*`
//          group is equally mandatory. Both end at MEM.VRAM.ARBITER, which has
//          no spare client index (see I15 item 2), and at `zhao_sdram_ctrl`,
//          which is I23's absent behavioural model. That is a real wall and it
//          is why the new block carries a page store of its own rather than a
//          client.
//        * WRONG: THERE WAS NO COLOUR LAW TO INVENT. `atm_rgb_i` and
//          `hud_rgb_i` are RGB565. A CLUT8 palette entry is RGB565. An RGB565
//          texel is RGB565. The sampler therefore performs ZERO colour
//          arithmetic -- no expansion, no rounding, not one multiply -- and
//          the expansion law (`exp5`/`exp6`, replicate the high bits) stays
//          inside POST.COMPOSITE where it already lived. The refusal was
//          written about a step the design does not contain.
//
//      That is this repository's own "first explanation that absolves the
//      design" law: a refusal that means less work arrives first and explains
//      almost all of the evidence. The check that separated the halves was one
//      grep for `exp5` and one look at the width of `atm_rgb_i`.
//
//      WHAT IS COMPOSED NOW, and what each connection is:
//        `u_twod_sampler.pw_*` -> `u_twod_plane.p_*`      the raster walk
//        `u_twod_plane.s_*`    -> `u_twod_sampler.pl_*`   the texel request
//        `u_twod_sampler.atm_*`-> `u_post_composite`      the colour
//        `u_twod_sprite.s_*`   -> `u_twod_sampler.sp_*`   the texel request
//      Not one of those is an adapter: every connection is a port to a port of
//      the same width and meaning. The sampler owns the WALK because
//      POST.COMPOSITE's `atm_*` group is a 1-cycle RANDOM ACCESS that must
//      hold through a stall -- that block's own header says the fix is "a
//      synchronous memory", and a bare streaming sampler could not have closed
//      this seam however correct its colours were.
//
//      WHAT IS STILL A GAP, and the three are different from each other:
//        1. `post_hud_*` -- BOUNDARY, and this is the obstacle the earlier
//           refusals should have named. TWOD.SPRITE walks in DESCRIPTOR order,
//           one whole sprite at a time; `hud_*` is a random access in RASTER
//           order. Bridging them needs a frame-resident HUD store (384 x 240 x
//           17 bits, about 1.6 Mbit -- SDRAM, so I23) or a display list that
//           can re-walk ONE SCANLINE across many descriptors, which is a
//           different block from the one TWOD.SPRITE is. A small line ring
//           plus backpressure was worked through and REJECTED: it makes a
//           sprite trickle one row per composited line, so ten 32-row sprites
//           need 320 lines of a 240-line frame. That is a machine that passes
//           its tests and cannot draw a HUD. The sprite's colours therefore
//           leave on `twod_sc_*` and that port group is this entry's new gap,
//           stated rather than dressed up.
//        2. `twod_pd_*` and `twod_sd_*` -- the DESCRIPTORS. These are the CMD
//           seam and they are the SAME gap I14 and I30 already describe, not a
//           new one: `zhao_cmd_decoder` emits record headers, and the executor
//           that would turn a SetPlane record into a plane descriptor does not
//           exist.
//        3. `twod_ld_*` -- NOT A GAP. A texture page, a palette and a binding
//           are generated assets, which is this entry's own classification for
//           the grading curves, applied to a texture.
//
//      POST.GATHER IS STILL REFUSED AND STILL FOR ITS OWN REASONS, which the
//      new sampler does not touch. `fpga/rtl/compositor/zhao_post_gather.sv`
//      is built and tested; it is refused three times over:
//        * ITS INPUT DOES NOT EXIST IN THIS SHAPE. It takes per-fragment glow
//          as three 8-bit channels, a signed 8.8 displacement pair and an ink
//          bit. `zhao_raster_resolve` emits ONE 8-bit `fb_tag_o` -- an effect
//          channel and a strength, `spec/stars_and_flares.md` 1. Expanding a
//          tag byte into glow RGB plus a displacement vector IS a colour and
//          geometry law invented in the composer -- and note that this is the
//          claim the atmosphere half of this entry made falsely and this half
//          makes truly. The difference is checkable: there, both sides of the
//          seam were already RGB565; here, one side is 8 bits and the other is
//          25, and nothing in the tree says how to get from one to the other.
//        * AND THE STREAM IT WOULD READ IS INTERNAL ANYWAY. Corrected
//          2026-09-19 with I15: the RESOLVED stream is not internal -- it is
//          nine ports on `zhao_geom_bin_pipe_v2`. What never leaves is the
//          PRE-RESOLVE FRAGMENT stream that POST.GATHER's per-fragment input
//          actually describes; `stage_fragment_*` exposes a fragment's texel
//          sample as a structural probe for the directed gate and the mutants,
//          not as a handshaked stream and not as a glow value.
//        * ITS OUTPUT IS A THIRD GAP. The block flushes sixteen cells per tile
//          as a STREAM, while POST.COMPOSITE reads a plane by {view, cx, cy}.
//          The store between a flush and a random access is the same shape of
//          thing TWOD.SAMPLER's atmosphere ring is -- so this one IS now
//          buildable in principle, and it is NOT built here because the first
//          bullet makes the contents of that store undefined. Building a
//          correct store for an invented value is the worse half of the two.
//
// I18. MEASURE.HISTOGRAM's event ingress (`hist_ev_*`) -- BOUNDARY. Nothing in
//      the console produces an error-magnitude stream; the block measures a
//      difference against a reference and the console has no reference. Its
//      INTERVAL is real (I5 of the connected list), its EVENTS are not.
//
//      ITS TWO MEASURE SIBLINGS ARE REFUSED, 2026-09-19, and the reasons are
//      recorded here because "the histogram is composed, so its siblings must
//      be nearly composable" is the plausible reading and it is wrong.
//
//      MEASURE.TOKENS has no producer for either of its two inputs.
//      `budget_*` is SetPresentationContract's five per-frame budgets;
//      `zhao_cmd_scheduler` decodes that opcode and emits `mode_o` and nothing
//      else -- there is no budget port on it to connect. And the request side
//      is worse than absent, it is MISMATCHED: GEOM.BINNER's token client is
//      real and is already TIED OFF INSIDE THE SHELL --
//      `zhao_shell_top_v2.sv` reads `.tok_req_o(rp_tok_unused),
//      .tok_grant_i(1'b1)` -- and what it offers is ONE BIT, where
//      MEASURE.TOKENS' `req_*` carries view, class, essential, rung, cost and
//      source id. Adopting it would mean inventing five of the six fields,
//      and `req_essential_i` and `req_rep_i` are policy rather than routing.
//      That shell tie-off is inside I20's scope and is named here because it
//      is the specific thing a later packet must repair.
//
//      MEASURE.GOVERNOR then fails for its own reasons even if TOKENS existed.
//      Its `px_err0/1_i` is SetView's per-camera `fx16 pixel_error` and its
//      `proj0/1_i` the camera projection scale -- the same absent CMD path
//      I14 describes for the projection matrices, and the same one I30 now
//      describes for the stamp dispatch. Its `starved0/1_i` would come from
//      TOKENS' `den_*`, which is a one-cycle REGISTERED denial while the
//      governor wants a per-frame per-view verdict: the latch between them is
//      state, and state belongs in a file with a contract and a test. Its
//      outputs go to TERRAIN.LOD, which is not composed (entry I21).
//
//      DEBUG.TRACE is refused by THIS ENTRY'S OWN ARGUMENT, one block over.
//      Its event carries `ev_expected_fx_i` beside `ev_actual_fx_i` -- a
//      differential against a reference -- and the console has no reference,
//      which is the sentence above. The ledger's `upstream: [CMD.DECODER]` is
//      not the RTL's seam either: the decoder emits record headers, not
//      {stage, tile, primitive, pixel, expected, actual}.
//
// I19. MEASURE.HISTOGRAM's host read window (`hist_rd_*`) -- BOUNDARY. The
//      host is the HPS; no register path from HPS to this block exists.
//
// I20. Everything `zhao_shell_top_v2` already declares provisional at its own
//      edge -- the triangle port, `fb_writer_i`, the FRAME_RING view, the
//      geometry memory clients -- is UNCHANGED and still provisional. This
//      file adds no opinion about them; read that file's header.
//
// I21. TERRAIN.GROUP_SEQ's subpatch job port (`terr_job_*`,
//      `terr_sparse_fill_i`) -- BOUNDARY, and this one has a near-producer
//      that is NOT wired, which is worth stating precisely so nobody wires it
//      by name-matching. `fpga/rtl/terrain/zhao_terrain_lod.sv` exists and its
//      own header says its output is "EXACTLY zhao_terrain_tess's job port".
//      It is not the sequencer's job port: the sequencer additionally needs
//      `job_view_mask`, `job_mat_a`, `job_mat_b` and `job_weight`, and
//      TERRAIN.LOD emits none of the four. Taking its 12 matching fields and
//      inventing the other 4 here is the hidden-adapter failure. TERRAIN.LOD
//      is itself unfed besides: its `sp_*` patch_state descriptors come from
//      TERRAIN.PATCH, which is not composed. Closing this properly is
//      LOD + PATCH composed together, with the view mask and materials coming
//      from whoever owns the draw -- CMD.SCHEDULER, by the same absent path
//      I14 describes for the projection matrices.
//
// I22. TERRAIN.TESS's lattice and cell-state read ports (`terr_lat_*`,
//      `terr_cs_*`) -- BOUNDARY. `fpga/rtl/terrain/zhao_terrain_compcache_front.sv`
//      EXISTS and is the named owner -- it emits `lat_h_o`, `lat_wx_o`,
//      `lat_wz_o` and `cs_substance_o` on exactly this shape. It is not
//      composed here because it is a CACHE FRONT: adopting it moves the gap
//      one hop to its fill path and to TERRAIN.COMPCACHE's store, which is
//      the next packet rather than this one. The ports are real and registered
//      (data valid the cycle after the request), so the harness can play the
//      memory the plan allows it to play.
//
// I23. GEOM.VDECODE's 32-byte vertex record stream (`geom_vd_v_*`) --
//      BOUNDARY, and its decoded side-channels (`geom_vd_d_n*`, `geom_vd_u/v`)
//      leave this module because nothing here consumes them. `geom_vd_bone0/1`
//      was in that list until 2026-09-19 and is not any more: the palette store
//      consumes both, so they are internal wires (see the I10 closure note).
//      THE NAMED OWNER EXISTS AND IS NOT COMPOSED, and the reason is worth
//      writing down precisely so the next packet does not rediscover it:
//
//        `zhao_geom_assetfetch` serves exactly this port (`v_valid_o /
//        v_bytes_o[255:0] / v_src_id_o`), is fed by `zhao_geom_meshfetch`'s
//        result record, and the two share ONE MEM.GUARD client through
//        `zhao_geom_mem_adapter` -- whose A port is named for MESHFETCH and
//        whose B port is named for ASSETFETCH. `zhao_shell_top_v2` already
//        exposes the socket that chain plugs into (`geom_guard_req_i`,
//        `geom_guard_rsp_o`, `geom_beat_*_o`). So the WIRING is fully
//        determined and was not the obstacle.
//
//        THE OBSTACLE IS THAT THE BEATS COME FROM INSIDE THE SHELL. That
//        socket's read data returns through MEM.VRAM.ARBITER and
//        `zhao_sdram_ctrl`, and there is NO BEHAVIOURAL SDRAM MODEL IN THIS
//        TREE -- the smoke bench leaves `phy_*` unconnected. Composing the
//        three blocks onto that socket would elaborate cleanly, pass lint, add
//        their area to the fit, and never see a single beat: a disconnected
//        implementation wearing a connection. They are therefore left for the
//        packet that brings a memory model with it, and the register goes on
//        counting them, which is correct.
//
// I24. GEOM.CLIP's projected-triangle input (`geom_clip_tri_*`), its three
//      attribute packets (`geom_clip_attr_*`) and its cull mode
//      (`geom_clip_cull_mode_i`) -- BOUNDARY. Same absent block as I11 and
//      I12: the three screen corners and their behind bits are the replay
//      customer's output, and the attribute packets are GEOM.ATTRSETUP's.
//      NOT a boundary, and listed here so nobody re-opens it: the SCISSOR
//      (`vp_x0/vp_y0/vp_w/vp_h`) is REAL. It is driven from the same
//      mode-derived pass geometry the compositor uses (GLUE 1 below), because
//      `zhao_geom_clip`'s own header defines the rectangle as "a canvas in
//      Z60/Storm, one 256x192 view block in Duo -- video_rules.md 3.1", which
//      is that value and not a second opinion about it.
//
//      THE PARTICLE DRAW ENDPOINTS ARE NOW COMPOSED, and the paragraph that
//      refused them is kept below with its errors marked, because three of its
//      four refusals were right for the wrong reason and the fourth was simply
//      wrong. `zhao_part_expand`'s triangle and `zhao_part_soft`'s span now
//      leave this module as `part_exp_*` / `part_sft_*` -- so what is still a
//      BOUNDARY here is only their CUSTOMER, which is the same absent GEOM
//      replay/setup path I11, I12 and I13 name. The producer exists.
//
//      WHAT IT SAID: "NOTHING IN THIS CORE PROJECTS A PARTICLE.
//      `zhao_proj_subsystem` has exactly two client ports and both are live
//      (GEOM on A, TERRAIN on B), so a particle client is a third port and that
//      is an owner ruling, not wiring. The particle ring composed above carries
//      particle128 WORLD records and stops there."
//
//      WHY THAT WAS TOO STRONG. A third CLIENT does not have to be a third
//      PORT. `fpga/rtl/particles/zhao_part_project.sv` (2026-09-19) sits IN
//      FRONT OF client A and time-multiplexes it -- geometry straight through,
//      particles round-robin on the cycles geometry does not want, results
//      demultiplexed by the rider's top bit, which `zhao_geom_proj_lane`'s own
//      zero padding leaves free (ARENA_W 3 + INDEX_W 12 of GEOM_PAY_A_W 16).
//      Nothing inside `zhao_project_service`, `zhao_project_core` or
//      `zhao_proj_subsystem` changed by one character, so no verified block's
//      starvation law was touched and the owner ruling the entry called for is
//      not needed. The block contains NO projection arithmetic: a second
//      `zhao_project_core` would be 6,199 ALM and 33 DSP, and undoing the
//      deduplication campaign to draw a sprite is the trade that was actually
//      being refused. The composed demand is client A's own plus about 3.9% of
//      the frame at SLOTS = 40, and about 17.7% at the shipping SLOTS = 8 --
//      the knob and its arithmetic are in that block's header.
//
//      And the ring no longer stops: PART.PROJECT takes a THIRD BRANCH of the
//      fork on PART.COLLIDE's output (glue 3), so the records it projects are
//      the ring's own and no new record boundary was invented for them.
//
//      WHAT IT SAID: "`zhao_part_expand` IS ADDITIONALLY UNSAFE TO ADOPT
//      TODAY, by its own testimony... converting a world radius to a screen
//      half-side is a projection."
//
//      WHY THAT WAS WRONG, and this is the instructive one. The conversion was
//      ALREADY RATIFIED and the refusal was reading a stale warning.
//      `reference/include/zref/zref_particle.hpp` carries a correction dated
//      2026-09-06 whose entire purpose is to say so: "THE PROJECTION IS NOT THE
//      MISSING PIECE. Turning a world radius into a screen half-extent is
//      already implemented, already tested, and already has its trap written
//      down -- `zref::render::draw_form_marker`", whose world-space branch is
//      `half_sub = rescale_s32(fx_mul(size_fx16, c.s.d), 8)` at projection
//      scale 1. PART.PROJECT implements that expression bit for bit and hands
//      `zhao_part_expand` a SCREEN size, which is exactly the input its banner
//      asks for -- so `size << 4` is correct rather than superseded and the
//      block is composed UNCHANGED. What the reference says IS missing is
//      `base_radius_fx16`, a per-species content decision; it arrives here as
//      the declared owner port `part_prj_base_radius_i` at the end of the
//      port list.
//
//      This is the "read the SIBLING contract" lesson with the roles reversed:
//      the refusal quoted `zhao_part_expand`'s header, which was accurate about
//      itself, and never read the reference file that had already withdrawn the
//      warning it was leaning on. An instruction is not delivered until it is
//      read, and a REFUSAL is an instruction too.
//
//      WHAT IT SAID about `zhao_part_ladder`: two refusals. The first --
//      "`p_size_i` is a PROJECTED size with the same absent producer" -- is
//      closed by the above. THE SECOND STANDS AND IS NOT REPAIRED HERE: its
//      `p_prev_rung_i`/`p_hold_i` are per-(particle, camera) state its contract
//      explicitly keeps OFF chip, and I23's DDR is still absent. What changed
//      is that the state is now PAIRED: it rides PART.PROJECT's slot store with
//      its particle and comes back beside the rung that consumed it, on
//      `part_rung_*`. Where it comes FROM is the declared boundary
//      `part_prj_prev_rung_i`/`part_prj_hold_i`/`part_prj_first_i`, which is a
//      narrower gap than "nothing projects a particle" and a different one.
//
//      AND THE LEDGER'S EDGE HERE IS STILL NOT THE RTL'S, exactly as written.
//      `design/blocks.yml` gives PART.EXPAND and PART.SOFT
//      `upstream: [PART.LADDER]`; the ladder emits a RUNG and neither block has
//      a port that takes one. That is why the composition below routes BY the
//      rung with a stateless demux rather than wiring the ladder's output into
//      a port that does not exist. The declared edge is a ROUTING fact, and it
//      is implemented as one. (The same is still true of CMD.DECODER ->
//      DEBUG.TRACE: the decoder emits record headers, the trace ring takes
//      {stage, tile, primitive, pixel, expected_fx, actual_fx}. Those are
//      different things.)
//
//      TWO OF THE SIX RUNGS HAVE AN ENDPOINT IN THIS TREE. SHARD -> PART.EXPAND
//      and SPRITE -> PART.SOFT. MESHLET, RIBBON, GLINT and CULLED have no block
//      built, so a particle on one of those rungs leaves only on
//      `part_rung_*` -- it is not silently dropped, it is emitted with its rung
//      and nothing here claims to draw it. Do not "fix" that by routing GLINT
//      into PART.SOFT: a glint is its own representation on the frozen ladder
//      and inventing the equivalence in a composer is the hidden adapter this
//      file must not contain.
//
// I25. GEOM.VDECODE's format selector (`v_format_i`) -- NOT a tie-off: the
//      core assigns it, in the same standing as I9. There is exactly ONE
//      ratified vertex format and the block's own port comment says "must be
//      0"; a constant here is the ABI, not a missing owner. It is a NAMED
//      localparam (`GEOM_VERTEX_FORMAT_C`) rather than a literal so that the
//      day a second format is ratified, the thing that has to change is
//      visible and greppable instead of being a `3'd0` in a port map. If that
//      day comes, the owner is whoever owns the draw -- the same absent
//      CMD.SCHEDULER path I14 describes -- and this becomes a real entry.
//
// I26. THE TERRAIN PAGING SPINE's MEM.HPS.BRIDGE and MEM.GUARD clients
//      (`terr_hps_*`, `terr_guard_*`) -- BOUNDARY, and this one is a REACHABLE
//      boundary rather than an unreachable one, which is the whole difference
//      between it and the refusal recorded at I23.
//
//      Both providers EXIST and are already in this closure: `zhao_hps_bridge`
//      and `zhao_mem_guard` are instantiated inside `zhao_shell_top_v2`. What
//      is missing is a SOCKET. The shell exposes exactly one guard client port
//      and it is named for GEOM (`geom_guard_req_i`), and it exposes the HPS
//      bridge's HARNESS side, not a client side. So terrain cannot reach
//      either from in here without a shell port change.
//
//      The two terrain readers are merged by the REAL `zhao_hps_arbiter`
//      before they leave, so what crosses this boundary is ONE bridge client,
//      not two. That arbiter has exactly TWO client ports and its rule-5
//      starvation law is written for two guaranteed clients; a third is an
//      owner ruling, and it is the second reason TERRAIN.WRITEBACK is not
//      composed (I28).
//
//      WHY THIS IS NOT I23's REFUSAL AGAIN. GEOM.MESHFETCH's beats come back
//      from INSIDE the shell through MEM.VRAM.ARBITER and `zhao_sdram_ctrl`,
//      whose `phy_*` no bench connects -- nothing outside can put a beat on
//      them, so those blocks would elaborate and never see one. These ports
//      are on THIS MODULE'S EDGE, where the completion plan's own sentence
//      applies: the harness "supplies external clocks, input events, MEMORY
//      BEHAVIOR and host packets". A harness can drive these and the composed
//      terrain bench already drives exactly this shape.
//
// I27. THE TERRAIN COMPOSE ENGINE's door (`terr_is_*`) and the directory's
//      deformation, unpin and handle-check ports (`terr_dm_*`,
//      `terr_unpin_*`, `terr_chk_*`) -- BOUNDARY. ONE missing subsystem, so
//      one entry: TERRAIN.SEQ issues a patch, and what receives it is
//      TERRAIN.PATCH plus the field engine composing into the named compose
//      slot, with TERRAIN.COMPCACHE storing the result and unpinning the page
//      when the job is done. None of that is composed, and the reason is NOT
//      that the seams do not meet -- TERRAIN.PATCH's `st_*` output is declared
//      port-for-port with TERRAIN.COMPCACHE's `st_*` input by COMPCACHE's own
//      header, and COMPCACHE's serve side is exactly TERRAIN.TESS's lattice
//      port, which is entry I22.
//
//      THE BLOCKER IS PLACEMENT, and it is a missing owner rather than missing
//      wiring. TERRAIN.PATCH needs `wx_i`/`wz_i`, the PLACED world x and z of
//      the lattice vertex, and TERRAIN.COMPCACHE needs the same 33 column x's
//      and 33 row z's through its `pos_*` write port. TERRAIN.PAGESTREAM emits
//      the heights and the lattice indices and NOT the placement; nothing else
//      in the tree emits it either. Deriving it here from the index, the patch
//      coordinate and a pitch is arithmetic invented in the composer, which
//      this file does not do. Closing I22 is therefore PATCH + COMPCACHE + a
//      placement owner, and the third is the one that does not exist.
//
// I28. TERRAIN.SEQ's F-sheet writeback job (`terr_wb_*`), its barrier
//      completion (`terr_wb_done_*`) and TERRAIN.RESIDENCY's writeback
//      acknowledgement (`terr_wback_*`) -- BOUNDARY. All three are one gap
//      because they are three ends of ONE absent block, and that block EXISTS:
//      `fpga/rtl/terrain/zhao_terrain_writeback.sv`, tested, whose `j_*` port
//      takes TERRAIN.SEQ's `wb_*` almost field for field. It is left out for
//      two independent reasons, either of which is sufficient:
//
//        * ALMOST. Its job port also needs `j_journal_addr_i` and `j_seq_i` --
//          where in the HPS journal the sheet goes and the ticket the journal
//          echoes back. TERRAIN.SEQ emits neither and nothing in `fpga/rtl`
//          owns them; `tests/terrain/tb_terrain_world.sv` mints the ticket in
//          the bench and its own comment calls that "glue [that] is a finding
//          rather than a convenience: no contract says who owns the journal
//          ticket". Minting it here would be that finding, hidden.
//        * It would be a THIRD MEM.HPS.BRIDGE client behind a two-port
//          arbiter -- see I26.
//
//      AND IT COULD NOT SEE A BEAT ANYWAY, which is the check worth doing
//      before calling a refusal expensive. A writeback job is caused by a
//      claim evicting a page whose F-sheet is dirty, and a page becomes dirty
//      only through the directory's `dm_f` port, whose owner is TERRAIN.BAKE
//      (entry I27's subsystem, not composed). With no deformation in the core
//      there are no dirty evictions, so the block would add its area for a
//      path nothing can enter.
//
// I29. GEOM.POSE's CLIP PAGE AND SKELETON BAKE (`geom_pose_start_i`,
//      `geom_pose_bone_*`, `geom_pose_quat_*`, `geom_pose_inv_rest_i`,
//      `geom_pose_root_d*`) -- BOUNDARY. NEW 2026-09-19, and it is I10's
//      SUCCESSOR rather than a new discovery: closing I10 composed
//      `zhao_geom_pose_decode`, and a composed block's inputs become this
//      module's edge until their own producer arrives.
//
//      The decoder's source fetch is COMBINATIONAL BY CONTRACT -- it drives
//      `bone_idx_o` and the caller must present that bone's parent, rest
//      translation, quaternion and inverse-rest matrix in the SAME cycle. So
//      this is not a stream that could be tied off plausibly; it is a memory
//      the caller owns, and the block's own header says it "owns none of them
//      and holds no cache".
//
//      THE OWNER IS GEOM.MESHFETCH plus the clip-bank pages behind MEM.GUARD,
//      and it is the same obstacle as I23, one asset kind over: the pages come
//      back through MEM.VRAM.ARBITER and `zhao_sdram_ctrl`, and there is no
//      behavioural SDRAM model in this tree. A composition onto that socket
//      would elaborate, lint and never see a beat.
//
//      WHAT IS NOT PART OF THIS GAP, because the distinction is the whole
//      point of closing I10: the palette STORE is present and internal. A
//      decoded palette written by the block above is read by GEOM.SKIN through
//      `zhao_geom_pose_palette` with nothing external in between, and the
//      store's own `geom_pal_bone_unset_o` reports any vertex that arrived
//      before its pose did. The missing thing is the BYTES, not the path.
//
// I30. SURFACE.STAMP's DISPATCH (`surf_cmd_*`) -- BOUNDARY. NEW 2026-09-19,
//      opened by composing the SURFACE pair (connected item 9).
//      `spec/commands.zidl` carries SurfaceStamp with its `handle32[patch]`,
//      operation byte, tag, strength, transform, radius and ring width, so the
//      command is RATIFIED and the fields below are its fields -- what was
//      missing was the path to here. HALF CLOSED 2026-09-19.
//
//      CLOSED: THE RATIFIED FIELDS. `zhao_cmd_exec` (section 7c) supplies the
//      patch handle, operation, tag, strength, the transform's translation,
//      radius, ring width and the record header's source id, out of a packet
//      CMD.DECODER has ratified. `cmd_exec_stamps_o` counts the dispatches.
//      The owner turned out NOT to be CMD.SCHEDULER, which this entry named:
//      the scheduler works on framed 16-byte record payloads and a SurfaceStamp
//      carries its transform at record byte 28, past the framer's window. The
//      executor reads the byte stream itself, which is why it exists.
//
//      STILL OPEN: THE ENVELOPE AND THE POLICY. `surf_cmd_env_*` is the patch
//      placement entry I27 records as having no owner anywhere in the tree, and
//      `cmd_blend_en_i` / `cmd_blend_i` / `cmd_age_shift_i` / `cmd_field_en_i`
//      are console policy that no opcode carries. An executor-issued stamp
//      therefore rides the HOST port's envelope and policy, which is stated in
//      the merge above rather than left to be discovered from a stamp landing
//      in the wrong place. `brush` is a third kind of absence and
//      `zhao_surface_stamp.sv` S5 owns it: nothing in this tree defines a brush
//      page's format, so there is no port to drive.
//
// I31. SURFACE.STAMP's FIELD-DRIVEN BRUSH (`surf_fld_*`) -- BOUNDARY.
//      FIELD.SEQ.STAMP is the named owner and it is not built. This is the
//      same shape as I5 (PART.UPDATE's field sample) one sequencer over, and
//      it is a SEPARATE entry from I30 because the absent owner is a different
//      block: closing the CMD path would not close this, and vice versa.
//      NOT tied off: `cmd_field_en_i` selects whether the brush is consulted,
//      and it is an input rather than a constant, so the datapath behind it
//      survives synthesis and this row measures it.
//
// I32. SURFACE.STAMP's `stamp_results` (`surf_res_*`) -- BOUNDARY. TERRAIN.BAKE
//      is the named consumer, it is built (`fpga/rtl/terrain/zhao_terrain_bake
//      .sv`) and it is NOT composed. It is deliberately not composed here:
//      TERRAIN.BAKE belongs to the terrain compose engine that entry I27 says
//      is blocked on a placement owner, and adopting it for this one port
//      would pull that whole subsystem in behind a seam I27 already records as
//      unclosable today. The port is on this module's edge so the result
//      stream is observable rather than dropped.
//
// I33. PART.TABLE's PER-FRAME LOAD (`part_tbl_ld_*`) -- BOUNDARY. NEW
//      2026-09-19, and it is the SUCCESSOR to the deleted I2/I3 rather than a
//      restatement of them: the descriptor table is built, instantiated and
//      answering all four reads inside this module, and what has no owner here
//      is the HOST THAT FILLS IT. Six ports, one word per clock, never refused
//      for backpressure.
//
//      THE ABSENT OWNER IS CMD.SCHEDULER, the same one I14 and I30 name.
//      `zhao_cmd_decoder` is not composed (see the refusal list below) and no
//      block in this core produces a descriptor write. Inventing one here would
//      mean this file choosing what a species IS, which is owner DATA --
//      `reference/include/zref/zref_particle.hpp` says so in as many words:
//      "there is no species table ... That is a DATA/ABI question and it is
//      properly the owner's". So the load is a port and the CONTENTS are not
//      guessed.
//
//      WHAT AN UNLOADED TABLE DOES, stated rather than left to be discovered:
//      every read answers with whatever the array holds -- X in simulation,
//      zero on Cyclone V power-up -- and zero is a recipe of 0 (HOLD), an
//      unbounded lifetime, an IGNORE response, count 0 and known 0. Every
//      consumer already refuses on its own terms, with its own counter. The
//      table adds no fifth opinion, deliberately.
//
//      `part_tbl_load_refused_o` CANNOT FIRE IN THIS COMPOSITION and its zero
//      is therefore not a measurement: at PART_SPECIES_N = 128 and CRV_N = 16 a
//      seven-bit index cannot address outside the table, so the refusal is
//      structurally unreachable. It is reachable and fired at SPECIES_N = 8 in
//      `tests/particles/part_table_directed.cpp`. Said here because a counter
//      asserted zero and never seen to move is a claim, not evidence.
//
// ---------------------------------------------------------------------------
// BLOCKS OFFERED TO THIS COMPOSITION AND REFUSED -- the remainder
// ---------------------------------------------------------------------------
// Fourteen of the sixteen blocks in the 2026-09-19 small-blocks packet were
// refused. Ten of them are argued at the entry their port would have closed
// (TWOD.PLANE, TWOD.SPRITE and POST.GATHER at I17; PART.EXPAND, PART.SOFT and
// PART.LADDER at I24; MEASURE.TOKENS, MEASURE.GOVERNOR and DEBUG.TRACE at I18;
// PART.TABLE was in that sentence too, as "an uncashed cheque rather than a
// refusal", pointing at I2; it is COMPOSED as of 2026-09-19 and the pointer now
// goes to the CLOSED AND DELETED ledger above). The remaining four have no such
// entry, so
// they are here rather than nowhere:
//
//   (CMD.DECODER was refused here and the refusal is SPENT. It is composed as
//   section 7b, and CMD.EXEC beside it as section 7c. The refusal was accurate
//   about the obstacle -- the stream was real, already flowing, and enclosed in
//   `zhao_shell_top_v2` as body wires -- and the shell-owner port change it
//   named is what happened. Left as a stub rather than deleted because this
//   list is the record of what was OFFERED and why, and "it was refused, then
//   the obstacle was removed" is the useful sentence. The refusal also said
//   "its own verdict has no consumer either"; that is now the one input
//   CMD.EXEC gates every console write on.)
//
//   FIELD.PROGCACHE. Its `lu_*`/`cm_*` are a hash lookup and a commit with a
//   decode verdict. Both ends are FIELD.SEQ blocks and none of them is
//   composed; `zhao_field_v2_core` is itself in the register's
//   built-but-not-connected list.
//
//   FORGE.PRIM and FORGE.PRIM_EVAL are the TOPOLOGY and the POSITIONS of one
//   primitive -- indices from one, fx16 vertices from the other -- and they do
//   NOT meet each other: neither has a port the other drives. Both take a job
//   from CMD.SCHEDULER (absent, as at I14/I30) and both aim at GEOM.SETUP,
//   which takes SCREEN triangles with edge functions. So composing them would
//   put stimulus in at this module's edge and take results out at the same
//   edge, with no producer and no consumer inside -- a disconnected
//   implementation with extra steps, which is what the standard excludes.
//
//   FORGE.CLIFF needs a page ISSUER that walks the lattice, a 34x34 solid-bit
//   window and a vdist read master. None exists; TERRAIN.TESS is composed but
//   emits none of the three. Its own output is a RIM EDGE, not a triangle, so
//   even the far end needs a block that is not built.
//
// ---------------------------------------------------------------------------
// LIGHTING SEAM -- DELIBERATELY NOT CONNECTED
// ---------------------------------------------------------------------------
// `zhao_geom_light.sv` and every `zhao_light_*` file are being refactored into
// a lighting service while this file is written, so they are EXCLUDED from this
// composition on purpose and none of them appears in its source closure. This
// core therefore contains ZERO lighting logic and its resource number contains
// none either.
//
// `light_seam_connected_o` is that statement made machine-readable: it is tied
// low and a hierarchy census can see it. WHAT MUST BE CONNECTED HERE LATER:
//
//   * the lighting service's VERTEX/NORMAL input, from GEOM.SKIN -- the
//     skinned-normal sibling `zhao_geom_skin_norm.sv` is its producer, and it
//     is deliberately NOT instantiated here for the same exclusion reason;
//   * the service's per-light parameter load, from the same host/CMD path that
//     I14 describes for the projection matrices;
//   * the service's RGB term OUT, into the raster material stage inside
//     `zhao_geom_bin_pipe_v2` -- which is a shell-side change, so the seam is
//     not purely additive and should be planned with I15;
//   * the tie-low below becomes a real driven level once all three exist.
//
// Nothing here should be read as "lighting fits in the remaining area". It has
// not been measured in this core at all.
//
// ---------------------------------------------------------------------------
// WHAT THIS CORE IS NOT
// ---------------------------------------------------------------------------
// A lint-clean Verilator run is NOT synthesizability. This file obeys the two
// Quartus 17.0 forms this repository has been bitten by -- `$fatal` only inside
// `initial begin ... end`, explicit `generate`/`endgenerate` -- but a block
// that has never been through `quartus_map` has not been shown to synthesize,
// however clean its lint, and at the time of writing this one had not.
//
// It is also not `zhao_console_board`: there is no PLL, no physical pin
// assignment and no board framework here. Plan S1.3 keeps those separate.
//
// Conservative SystemVerilog subset (charter S2).

module zhao_console_core
  import zhao_pkg::*, zhao_abi_pkg::*, zhao_fb_tuple_pkg::*;
#(
  // ---- the shell's own knobs, carried through ------------------------------
  parameter int unsigned FRAMER_Q = 8,
  parameter int unsigned WFIFO_W  = 64,

  // ---- CMD.EXEC (section 7c) -----------------------------------------------
  // How many SurfaceStamps one packet may carry. It is the ONE number in the
  // executor that can refuse an otherwise legal packet, so it is a knob and not
  // a constant: a packet with more stamps than this is refused WHOLE and
  // counted on `cmd_exec_stamp_overflow_o`, never applied in part. Eight is
  // chosen against the sheet, not against the ABI -- each stamp walks 4,096
  // texels, so a frame that wants more than eight is asking the surface stage
  // for more work than a frame has.
  parameter int unsigned CMD_EXEC_STAMP_Q = 8,

  // ---- PARTICLES -----------------------------------------------------------
  parameter int unsigned PART_REC_W    = 128,     // particle128, amendment C2
  parameter int unsigned PART_CAPACITY = 32768,   // the required tier
  parameter int unsigned PART_CHILD_D  = 64,
  parameter int unsigned PART_SPECIES_N= 128,
  parameter int unsigned PART_AGE_W    = 10,
  parameter int unsigned PART_POS_W    = 18,
  parameter int unsigned PART_VEL_W    = 11,
  parameter int unsigned PART_NRM_W    = 12,
  parameter int unsigned PART_FX_W     = 16,
  parameter int unsigned PART_PID_W    = 16,
  parameter int unsigned PART_TICK_W   = 32,
  // PART.TABLE's load bus. DERIVED, NOT A KNOB, and restated here for the same
  // reason `zhao_part_table` carries it in its own parameter list: a PORT WIDTH
  // CANNOT REFER TO A BODY LOCALPARAM. It is the widest descriptor slice --
  // recipe 4 + lifetime + age_mark + drag 8 + grav + strength + cx,cy,cz +
  // p0,p1,p2 -- and the table's own elaboration guard $fatals if this does not
  // equal its UPD_W, so a divergence between these two expressions is loud at
  // elaboration rather than a silently truncated descriptor. It is NOT
  // arithmetic invented here: it is the table's own expression, copied with its
  // owner named, and the guard is what makes the copy safe.
  parameter int unsigned PART_TBL_LD_W = 12 + (2 * PART_AGE_W) + (5 * PART_VEL_W)
                                            + (3 * PART_POS_W),

  // ---- GEOMETRY: the client-A side of the shared projector -----------------
  parameter int unsigned GEOM_ARENAS   = 4,
  parameter int unsigned GEOM_DEPTH    = 1089,
  parameter int unsigned GEOM_NVIEWS   = 2,
  parameter int unsigned GEOM_GEN_W    = 8,
  parameter int unsigned GEOM_PAY_A_W  = 16,
  parameter int unsigned GEOM_PAYLOAD_W= 106,
  parameter int unsigned GEOM_INDEX_W  = $clog2(GEOM_DEPTH) + 1,
  parameter int unsigned GEOM_ARENA_W  = $clog2(GEOM_ARENAS) + 1,
  parameter int unsigned GEOM_MUL_LANES= 3,

  // ---- GEOMETRY: the clip/setup triangle front door ------------------------
  // `zhao_geom_clip`'s ruling-5 attribute packet: invw24, u_over_w, v_over_w,
  // lit r/g/b and alpha. The block never interprets them; it only keeps them
  // with their vertices across the winding flip. ATTRW is the flattened width
  // and exists because a port list cannot call $clog2 on another port.
  parameter int unsigned GEOM_CLIP_ATTRS = 7,
  parameter int unsigned GEOM_CLIP_ATTRW = GEOM_CLIP_ATTRS * 32,

  // ---- GEOMETRY: the client-B/terrain side of the same projector ----------
  parameter int unsigned PROJ_T_ARENAS = 4,
  parameter int unsigned PROJ_T_DEPTH  = 81,
  parameter int unsigned PROJ_T_INDEX_W= $clog2(PROJ_T_DEPTH) + 1,
  parameter int unsigned PROJ_T_ARENA_W= $clog2(PROJ_T_ARENAS) + 1,
  // TERRAIN.TESS's own window index: 81 vertices need 7 bits. The sequencer
  // widens it to PROJ_T_INDEX_W, which carries a refusal bit beside it.
  parameter int unsigned PROJ_T_IDX_W  = 7,

  // ---- COMPOSITOR ----------------------------------------------------------
  parameter int unsigned POST_LINE_W   = 384,     // Z60 is the widest view
  parameter int unsigned POST_MAX_H    = 240,
  parameter int unsigned POST_NLINE    = 9,
  parameter int unsigned POST_LAG_LINES= 4,
  parameter int unsigned POST_LAG_PX   = 9,
  parameter int unsigned POST_XW       = $clog2(POST_LINE_W + 1),
  parameter int unsigned POST_YW       = $clog2(POST_MAX_H + 1),

  // ---- TWOD: the plane, the sprite walker and the sampler between them -----
  // TWOD_LINE_W and TWOD_MAX_H are NOT separate numbers -- they are
  // POST_LINE_W and POST_MAX_H, because the ring the sampler prepares is
  // addressed by the compositor's own raster pointer and two blocks that
  // disagree about what a line is would produce a picture sheared by the
  // difference. They are written as expressions rather than repeated literals
  // so the agreement cannot be broken by editing one of them.
  parameter int unsigned TWOD_PAGE_WORDS = 8192,   // 16 KiB of texel page
  parameter int unsigned TWOD_PAL_SLOTS  = 4,
  parameter int unsigned TWOD_BIND_SLOTS = 8,
  parameter int unsigned TWOD_ATM_LINES  = 4,
  parameter int unsigned TWOD_PAW        = $clog2(TWOD_PAGE_WORDS),
  parameter int unsigned TWOD_PALAW      = $clog2(TWOD_PAL_SLOTS * 256),
  parameter int unsigned TWOD_BSW        = $clog2(TWOD_BIND_SLOTS),

  // ---- MEASURE -------------------------------------------------------------
  parameter int unsigned HIST_EW       = 32,
  parameter int unsigned HIST_SUB_BITS = 1,
  parameter int unsigned HIST_LANES    = 4,
  parameter int unsigned HIST_CW       = 24,
  parameter int unsigned HIST_BINW     = $clog2((HIST_EW - HIST_SUB_BITS + 1) << HIST_SUB_BITS),

  // ---- SURFACE: the scar substrate and the engine that writes it -----------
  // Both are the owning block's own default, named here so the day one moves
  // the thing that has to move with it is greppable, and so the owner keeps
  // control of a value that is a measured frontier rather than a law.
  //   SURF_SLOTS   -- resident 64x64 sheets. Each slot is 65,536 bits (about
  //                   seven M10K), so this is SURFACE.SHEET's whole memory
  //                   bill and the block's own header asks the first fit to
  //                   retune it.
  //   SURF_SQ_RADIX-- SURFACE.STAMP's squarer radix. All three settings meet
  //                   the 20,000 texel/frame demand; the wall is Fmax on a
  //                   SHARED gpu_clk, which is exactly why 1 is the default
  //                   and this is a knob rather than a constant.
  parameter int unsigned SURF_SLOTS    = 2,
  parameter int unsigned SURF_SQ_RADIX = 1,

  // ---- TERRAIN: the paging spine (CMD -> SEQ -> RESIDENCY/LOADQ -> LOADER) --
  // Every one of these is the value the block that owns it already defaults to;
  // they are named here rather than left implicit so the day one moves, the
  // thing that has to move with it is greppable. TERR_SLOTW is DERIVED from the
  // directory's own geometry and must not be set independently: it is the width
  // of a {set, way} handle and $clog2(SETS*WAYS) is what the directory emits.
  parameter int unsigned TERR_SETS     = 256,   // ruling T9/T10: 256 sets...
  parameter int unsigned TERR_WAYS     = 4,     //   ...x 4 ways = 1,024 slots
  parameter int unsigned TERR_SLOTW    = $clog2(TERR_SETS * TERR_WAYS),
  parameter int unsigned TERR_GENW     = 8,     // T10: "generation u8 minimum"
  parameter int unsigned TERR_SEQW     = 16,    // the loader claim sequence
  parameter int unsigned TERR_PINW     = 6,     // 63 concurrent pins on a page
  parameter int unsigned TERR_CSLOTS   = 256,   // T6's composed height cache
  parameter int unsigned TERR_LOADQ_D  = 32,    // T7's per-frame page budget

  // TERRAIN.PAGE_POOL (ruling T2). The pool can move to any unmapped range, so
  // the base and the slot count are knobs and every width below is derived from
  // them rather than restated. TERR_MEMSLOT is ONE BIT WIDER than the pool needs
  // and that extra bit is load-bearing -- see the width note at the pageloader
  // instance, which is the one place in this file the step is crossed.
  parameter logic [ZHAO_VRAM_ADDR_BITS-1:0] TERR_POOL_BASE = 27'h400_0000,
  parameter int unsigned TERR_POOL_SLOTS = 1024,
  parameter int unsigned TERR_MEMSLOT     = $clog2(TERR_POOL_SLOTS) + 1,
  parameter int unsigned TERR_PAGE_BYTES  = 21376  // terrain_rules sec 2 / sec 7
) (
  // ==========================================================================
  // THE ADOPTED ORGANS' OWN BOUNDARY.
  // Every port in this section is listed in the header's
  // "INCOMPLETE -- TIED OFF, AND WHY" table with the owner that is missing.
  // ==========================================================================

  // ---- I1: PART.STATE's generation store (harness = memory) ---------------
  input  logic                    part_rd_valid_i,
  output logic                    part_rd_ready_o,
  input  logic [PART_REC_W-1:0]   part_rd_record_i,
  input  logic                    part_rd_last_i,
  output logic                    part_wr_valid_o,
  input  logic                    part_wr_ready_i,
  output logic [PART_REC_W-1:0]   part_wr_record_o,

  // ---- I33: PART.TABLE's PER-FRAME LOAD -----------------------------------
  // I2 and I3 ARE CLOSED and their twenty-five ports are GONE from this list
  // rather than driven -- `zhao_part_table` is instantiated below and answers
  // all four reads inside this module. What is left is the host that fills it,
  // and this is that seam. One word per clock; the table never refuses for
  // backpressure (`ld_ready_o` is constant high and says so in its own file).
  input  logic                    part_tbl_ld_valid_i,
  output logic                    part_tbl_ld_ready_o,
  input  logic [1:0]              part_tbl_ld_sel_i,
  input  logic [6:0]              part_tbl_ld_index_i,
  input  logic [1:0]              part_tbl_ld_event_i,
  input  logic [PART_TBL_LD_W-1:0] part_tbl_ld_data_i,

  // ---- I5: the bounded FIELD/FLOW acceleration sample ---------------------
  input  logic                    part_fld_valid_i,
  input  logic signed [10:0]      part_fld_ax_i,
  input  logic signed [10:0]      part_fld_ay_i,
  input  logic signed [10:0]      part_fld_az_i,

  // (I2's PART.COLLIDE slice -- `part_col_d_response_i` and the three
  //  coefficients -- was here. CLOSED: PART.TABLE serves it below, addressed by
  //  PART.COLLIDE's own new `d_index_o`.)

  // ---- I6: the live deformed terrain sample -------------------------------
  input  logic                    part_ter_valid_i,
  input  logic signed [PART_POS_W-1:0] part_ter_height_i,
  input  logic signed [PART_NRM_W-1:0] part_ter_nx_i,
  input  logic signed [PART_NRM_W-1:0] part_ter_ny_i,
  input  logic signed [PART_NRM_W-1:0] part_ter_nz_i,

  // ---- I7: the one plane --------------------------------------------------
  input  logic                    part_plane_en_i,
  input  logic signed [PART_NRM_W-1:0] part_plane_nx_i,
  input  logic signed [PART_NRM_W-1:0] part_plane_ny_i,
  input  logic signed [PART_NRM_W-1:0] part_plane_nz_i,
  input  logic signed [31:0]      part_plane_c_i,

  // (I2's PART.SPAWN slice was here. CLOSED: PART.TABLE serves it below.)

  // ---- PART.TABLE's own evidence (I33's other half) -----------------------
  // Five counters, out at the boundary like every other particle counter, so a
  // bench can say WHICH slice a load landed in and a silent load path is
  // visible rather than inferred from a descriptor read that happens to work.
  // `part_tbl_load_refused_o` is STRUCTURALLY UNREACHABLE at PART_SPECIES_N =
  // 128 and PART.TABLE's CRV_N = 16 -- seven index bits cannot address outside
  // a 128-entry table -- so its zero here is arithmetic, not a measurement.
  // The reachable case is proven at SPECIES_N = 8 by
  // `tests/particles/part_table_directed.cpp`, which is where the refusal has a
  // positive control. Quoting this port's zero as evidence of anything would be
  // the broken-instrument law.
  output logic [31:0]             part_tbl_loads_update_o,
  output logic [31:0]             part_tbl_loads_collide_o,
  output logic [31:0]             part_tbl_loads_spawn_o,
  output logic [31:0]             part_tbl_loads_curve_o,
  output logic [31:0]             part_tbl_load_refused_o,

  // ---- I8: the capacity backstop ------------------------------------------
  // ---- PARTICLE evidence (every counter leaves the module) ----------------
  output logic                    part_tick_busy_o,
  output logic                    part_tick_done_o,
  output logic [31:0]             part_survivors_o,
  output logic [31:0]             part_children_written_o,
  output logic [31:0]             part_children_dropped_capacity_o,
  output logic [31:0]             part_staging_stall_cycles_o,
  output logic [31:0]             part_species_refused_o,
  output logic [31:0]             part_updated_o,
  output logic [31:0]             part_upd_refused_o,
  output logic [31:0]             part_died_by_age_o,
  output logic                    part_upd_beat_refused_o,
  output logic [31:0]             part_velocity_saturations_o,
  output logic [31:0]             part_position_saturations_o,
  // WAS structurally stuck at zero (old header entry I4). Since the ruling of
  // 2026-09-19 it is driven by PART.COLLIDE's `collision_events_o` and moves.
  output logic [31:0]             part_collisions_applied_o,
  input  logic [3:0]              part_hist_sel_i,
  output logic [31:0]             part_hist_val_o,
  output logic                    part_colour_en_o,
  output logic [7:0]              part_colour_o,
  output logic                    part_contact_o,
  output logic [2:0]              part_response_o,
  output logic                    part_col_refused_o,
  output logic [31:0]             part_contacts_ignore_o,
  output logic [31:0]             part_contacts_die_o,
  output logic [31:0]             part_contacts_stick_o,
  output logic [31:0]             part_contacts_slide_o,
  output logic [31:0]             part_contacts_bounce_o,
  output logic [31:0]             part_contacts_terrain_o,
  output logic [31:0]             part_contacts_plane_o,
  output logic [31:0]             part_already_inside_at_entry_o,
  output logic [31:0]             part_terrain_sample_unavailable_o,
  output logic [31:0]             part_response_refused_o,
  output logic [31:0]             part_field_clamps_o,
  output logic [31:0]             part_children_requested_o,
  output logic [31:0]             part_children_emitted_o,
  output logic [31:0]             part_children_refused_o,
  output logic [31:0]             part_spawn_by_event0_o,
  output logic [31:0]             part_spawn_by_event1_o,
  output logic [31:0]             part_spawn_by_event2_o,
  output logic [31:0]             part_spawn_by_event3_o,
  output logic [31:0]             part_refused_count_gt_max_o,
  output logic [31:0]             part_refused_unknown_species_o,
  output logic [31:0]             part_refused_capacity_o,
  output logic [31:0]             part_max_children_in_tick_o,

  // ---- I23: GEOM.VDECODE's 32-byte vertex record stream --------------------
  // GEOM.ASSETFETCH is its named producer and is not composed; see I23 for the
  // reason, which is a missing memory model rather than a missing wire.
  input  logic                    geom_vd_v_valid_i,
  output logic                    geom_vd_v_ready_o,
  input  logic [255:0]            geom_vd_v_bytes_i,
  input  logic [15:0]             geom_vd_v_src_id_i,

  // ---- GEOM.VDECODE's side-channels and evidence ---------------------------
  // These leave the module because nothing composed here consumes them, and an
  // output left open is an output nobody reads.
  //
  // `geom_vd_bone0_o` / `bone1_o` USED TO BE HERE, described as "the address
  // the absent GEOM.POSE palette store needs". The store is no longer absent
  // (`zhao_geom_pose_palette`, composed below), so the two bone indices are now
  // INTERNAL wires with a real consumer and they have left this port list. The
  // attribute channel below still has none.
  output logic signed [7:0]       geom_vd_d_nx_o,
  output logic signed [7:0]       geom_vd_d_ny_o,
  output logic signed [7:0]       geom_vd_d_nz_o,
  output logic signed [15:0]      geom_vd_d_u_o,
  output logic signed [15:0]      geom_vd_d_v_o,
  output logic                    geom_vd_refused_o,
  output logic                    geom_vd_reserved_nz_o,
  output logic                    geom_vd_w0_illegal_o,
  output logic                    geom_vd_format_bad_o,
  output logic [31:0]             geom_vd_vertices_o,
  output logic [31:0]             geom_vd_reserved_nz_count_o,
  output logic [31:0]             geom_vd_w0_illegal_count_o,
  output logic [31:0]             geom_vd_format_bad_count_o,

  // ---- GEOM.SKIN's evidence -----------------------------------------------
  // I10 IS CLOSED. `geom_skin_a_m_i` / `geom_skin_b_m_i` used to be here, two
  // twelve-element boundary arrays that only a harness had ever driven. They
  // are gone rather than driven: `zhao_geom_pose_palette` below stores the
  // decoded palette and answers the two reads, so GEOM.SKIN's matrix ports are
  // internal and this module's edge is two arrays smaller.
  output logic [15:0]             geom_skin_src_id_o,
  output logic [31:0]             geom_skin_vertices_transformed_o,

  // ---- I29: GEOM.POSE's clip page and skeleton bake ------------------------
  // The palette store closed I10 by giving GEOM.POSE's decoder a consumer; the
  // decoder's own SOURCE is what is now missing, and this is it. See I29.
  input  logic                    geom_pose_start_i,
  input  logic [5:0]              geom_pose_bone_count_i,
  input  logic signed [31:0]      geom_pose_root_dx_i,
  input  logic signed [31:0]      geom_pose_root_dy_i,
  input  logic signed [31:0]      geom_pose_root_dz_i,
  output logic [4:0]              geom_pose_bone_idx_o,
  input  logic [4:0]              geom_pose_bone_parent_i,
  input  logic signed [31:0]      geom_pose_bone_tx_i,
  input  logic signed [31:0]      geom_pose_bone_ty_i,
  input  logic signed [31:0]      geom_pose_bone_tz_i,
  input  logic signed [15:0]      geom_pose_quat_w_i,
  input  logic signed [15:0]      geom_pose_quat_x_i,
  input  logic signed [15:0]      geom_pose_quat_y_i,
  input  logic signed [15:0]      geom_pose_quat_z_i,
  input  logic signed [31:0]      geom_pose_inv_rest_i [0:11],
  output logic                    geom_pose_busy_o,
  output logic                    geom_pose_done_o,
  output logic [31:0]             geom_pose_palettes_decoded_o,

  // ---- GEOM.POSE's palette store: its evidence ----------------------------
  // `geom_pal_bone_unset_o` is the one to watch. It is the store's own report
  // that a vertex named a bone the current palette has not been given, which is
  // what a vertex stream that was not drained before a new decode looks like.
  output logic [31:0]             geom_pal_vertices_served_o,
  output logic [31:0]             geom_pal_bones_written_o,
  output logic [31:0]             geom_pal_bone_oob_o,
  output logic [31:0]             geom_pal_bone_unset_o,

  // ---- I11: GEOM.GROUP_SEQ's job in, sealed group out ---------------------
  input  logic                    geom_job_valid_i,
  output logic                    geom_job_ready_o,
  input  logic [GEOM_INDEX_W-1:0] geom_job_count_i,
  input  logic [GEOM_NVIEWS-1:0]  geom_job_view_mask_i,
  input  logic [15:0]             geom_job_src_id_i,
  output logic                    geom_grp_valid_o,
  input  logic                    geom_grp_ready_i,
  output logic [GEOM_ARENA_W-1:0] geom_grp_arena_o,
  output logic [GEOM_GEN_W-1:0]   geom_grp_gen_o,
  output logic [GEOM_INDEX_W-1:0] geom_grp_count_o,
  output logic                    geom_grp_view_o,
  output logic [15:0]             geom_grp_src_id_o,
  input  logic                    geom_rel_valid_i,
  input  logic [GEOM_ARENA_W-1:0] geom_rel_arena_i,

  // ---- I12: GEOM.PROJ_LANE's arena origin and lookup port -----------------
  input  logic                    geom_org_we_i,
  input  logic [GEOM_ARENA_W-1:0] geom_org_arena_i,
  input  logic signed [31:0]      geom_org_x_i,
  input  logic signed [31:0]      geom_org_y_i,
  input  logic signed [31:0]      geom_org_z_i,
  input  logic                    geom_look_valid_i,
  output logic                    geom_look_ready_o,
  input  logic [GEOM_ARENA_W-1:0] geom_look_arena_i,
  input  logic [GEOM_GEN_W-1:0]   geom_look_gen_i,
  input  logic [GEOM_INDEX_W-1:0] geom_look_index_i,
  output logic                    geom_rep_valid_o,
  output logic                    geom_rep_hit_o,
  output logic                    geom_rep_refuse_o,
  output logic [GEOM_PAYLOAD_W-1:0] geom_rep_payload_o,
  output logic signed [31:0]      geom_rep_org_x_o,
  output logic signed [31:0]      geom_rep_org_y_o,
  output logic signed [31:0]      geom_rep_org_z_o,

  // ---- GEOMETRY evidence ---------------------------------------------------
  output logic [31:0]             geom_groups_opened_o,
  output logic [31:0]             geom_groups_sealed_o,
  output logic [31:0]             geom_vertices_sent_o,
  output logic [31:0]             geom_landings_o,
  output logic [31:0]             geom_jobs_refused_o,
  output logic [31:0]             geom_alloc_stall_cycles_o,
  output logic [31:0]             geom_rel_unheld_o,
  output logic                    geom_seal_early_o,
  output logic [31:0]             geom_arena_hits_o,
  output logic [31:0]             geom_arena_misses_o,
  output logic [31:0]             geom_arena_refusals_o,
  output logic                    geom_arena_overflow_o,

  // ---- I24: GEOM.CLIP's projected triangle, attributes and cull mode -------
  // The three SCREEN corners with GEOM.PROJECT's behind verdicts. The absent
  // replay customer specified at I11 is what drives these. The SCISSOR is NOT
  // here because it is real -- see GLUE 1.
  input  logic                    geom_clip_tri_valid_i,
  output logic                    geom_clip_tri_ready_o,
  input  logic signed [20:0]      geom_clip_tri_ax_i,
  input  logic signed [20:0]      geom_clip_tri_ay_i,
  input  logic signed [20:0]      geom_clip_tri_bx_i,
  input  logic signed [20:0]      geom_clip_tri_by_i,
  input  logic signed [20:0]      geom_clip_tri_cx_i,
  input  logic signed [20:0]      geom_clip_tri_cy_i,
  input  logic [2:0]              geom_clip_tri_behind_i,
  input  logic [15:0]             geom_clip_tri_src_id_i,
  input  logic [GEOM_CLIP_ATTRW-1:0] geom_clip_attr_a_i,
  input  logic [GEOM_CLIP_ATTRW-1:0] geom_clip_attr_b_i,
  input  logic [GEOM_CLIP_ATTRW-1:0] geom_clip_attr_c_i,
  input  logic [1:0]              geom_clip_cull_mode_i,

  // ---- GEOM.CLIP / GEOM.SETUP evidence and carried attributes --------------
  // The attributes and the flip leave the module for the same reason I23's
  // side-channels do: GEOM.ATTRSETUP is not composed, and dropping the swapped
  // packets here would lose the one thing GEOM.CLIP does to them.
  output logic [GEOM_CLIP_ATTRW-1:0] geom_clip_attr_a_o,
  output logic [GEOM_CLIP_ATTRW-1:0] geom_clip_attr_b_o,
  output logic [GEOM_CLIP_ATTRW-1:0] geom_clip_attr_c_o,
  output logic                    geom_clip_flip_o,
  output logic                    geom_clip_ret_valid_o,
  output logic [2:0]              geom_clip_ret_verdict_o,
  output logic [31:0]             geom_clip_submitted_o,
  output logic [31:0]             geom_clip_clipped_o,
  output logic [31:0]             geom_clip_culled_o,
  output logic signed [47:0]      geom_setup_area2_o,
  output logic [31:0]             geom_setup_triangles_submitted_o,

  // ---- I14: the shared projector's matrix bank ----------------------------
  input  logic                    proj_cfg_we_i,
  input  logic                    proj_cfg_view_i,
  input  logic [4:0]              proj_cfg_addr_i,
  input  logic [31:0]             proj_cfg_data_i,
  input  logic                    proj_en_i,

  // ---- TERRAIN: the subpatch job that drives client B (I21) ---------------
  // TERRAIN.GROUP_SEQ's own job port. TERRAIN.LOD exists and its header says
  // its output is "EXACTLY zhao_terrain_tess's job port" -- but it emits no
  // view mask and no material riders, and its own `sp_*` producer
  // (TERRAIN.PATCH's patch_state) is not composed here. See entry I21.
  input  logic                    terr_job_valid_i,
  output logic                    terr_job_ready_o,
  input  logic [5:0]              terr_job_ox_i,
  input  logic [5:0]              terr_job_oz_i,
  input  logic [1:0]              terr_job_level_i,
  input  logic [1:0]              terr_job_lvl_nz_i,
  input  logic [1:0]              terr_job_lvl_pz_i,
  input  logic [1:0]              terr_job_lvl_nx_i,
  input  logic [1:0]              terr_job_lvl_px_i,
  input  logic [16:0]             terr_job_morph_i,
  input  logic                    terr_job_surface_i,
  input  logic                    terr_job_dual_i,
  input  logic [15:0]             terr_job_src_id_i,
  input  logic [1:0]              terr_job_view_mask_i,
  input  logic [7:0]              terr_job_mat_a_i,
  input  logic [7:0]              terr_job_mat_b_i,
  input  logic [7:0]              terr_job_weight_i,
  input  logic                    terr_sparse_fill_i,

  // ---- TERRAIN.TESS's lattice and cell-state read ports (I22) -------------
  // `zhao_terrain_compcache_front` is the named owner and is not composed.
  output logic                    terr_lat_req_o,
  output logic [5:0]              terr_lat_vi_o,
  output logic [5:0]              terr_lat_vj_o,
  output logic                    terr_lat_surface_o,
  input  logic signed [31:0]      terr_lat_h_i,
  input  logic signed [31:0]      terr_lat_wx_i,
  input  logic signed [31:0]      terr_lat_wz_i,
  output logic                    terr_cs_req_o,
  output logic [4:0]              terr_cs_ci_o,
  output logic [4:0]              terr_cs_cj_o,
  input  logic [1:0]              terr_cs_substance_i,

  // ==========================================================================
  // THE TERRAIN PAGING SPINE'S OWN BOUNDARY (composed item 8 in the header)
  // ==========================================================================

  // ---- TERRAIN.CMD's command and its configuration ------------------------
  // NOT a tie-off, and in the same standing as I9 and I25. This is T5's
  // `SubmitTerrainSet`, already unpacked -- a HOST PACKET from SW.STREAM, which
  // the completion plan names in its own list of what a harness may supply:
  // "external clocks, input events, memory behavior and host packets". The
  // arena base/bytes and the live epoch ride the same path and are host state
  // for the same reason. If CMD.SCHEDULER later owns the terrain draw, these
  // become internal and this note goes with them; it is NOT counted as a gap
  // because nothing is missing, the producer is simply outside the console.
  input  logic                    terr_cmd_valid_i,
  output logic                    terr_cmd_ready_o,
  input  logic [31:0]             terr_cmd_epoch_i,
  input  logic [31:0]             terr_cmd_list_off_i,
  input  logic [31:0]             terr_cmd_list_bytes_i,
  input  logic [31:0]             terr_cmd_list_crc_i,
  input  logic [15:0]             terr_cmd_patch_count_i,
  input  logic [31:0]             terr_cmd_sequence_i,
  input  logic [31:0]             terr_cmd_src_id_i,
  output logic                    terr_cmd_done_valid_o,
  input  logic                    terr_cmd_done_ready_i,
  output logic                    terr_cmd_done_ok_o,
  output logic [3:0]              terr_cmd_done_verdict_o,
  output logic [31:0]             terr_cmd_done_src_id_o,
  output logic [31:0]             terr_cmd_done_crc_seen_o,

  input  logic [31:0]             terr_cfg_epoch_i,
  input  logic [31:0]             terr_cfg_arena_base_i,
  input  logic [31:0]             terr_cfg_arena_bytes_i,
  input  logic [15:0]             terr_cfg_load_budget_i,

  // ---- I26: the spine's MEM.HPS.BRIDGE and MEM.GUARD clients --------------
  // ONE bridge port, because the two terrain readers go through the REAL
  // `zhao_hps_arbiter` instantiated below and not through anything invented
  // here. See entry I26 for why the port stops at this module's edge.
  output zhao_hps_burst_req_t     terr_hps_req_o,
  input  logic                    terr_hps_grant_i,
  output logic                    terr_hps_wr_valid_o,
  output logic [63:0]             terr_hps_wr_data_o,
  output logic                    terr_hps_wr_last_o,
  input  zhao_hps_burst_rsp_t     terr_hps_rsp_i,

  output zhao_guard_req_t         terr_guard_req_o,
  input  zhao_guard_rsp_t         terr_guard_rsp_i,
  output logic [63:0]             terr_guard_wdata_o,
  output logic                    terr_guard_wvalid_o,
  input  logic                    terr_guard_wready_i,
  output logic                    terr_guard_wlast_o,

  // ---- I27: the terrain COMPOSE ENGINE's door and the directory's ---------
  //      deformation, unpin and handle-check ports.
  output logic                    terr_is_valid_o,
  input  logic                    terr_is_ready_i,
  output logic [TERR_SLOTW-1:0]   terr_is_slot_o,
  output logic [TERR_GENW-1:0]    terr_is_gen_o,
  output logic [31:0]             terr_is_epoch_o,
  output logic [31:0]             terr_is_island_o,
  output logic signed [15:0]      terr_is_ix_o,
  output logic signed [15:0]      terr_is_iz_o,
  output logic                    terr_is_cslot_valid_o,
  output logic [$clog2(TERR_CSLOTS)-1:0] terr_is_cslot_o,
  output logic [15:0]             terr_is_flags_o,
  output logic [7:0]              terr_is_view_mask_o,
  output logic [7:0]              terr_is_priority_o,
  output logic [31:0]             terr_is_src_id_o,

  input  logic                    terr_dm_valid_i,
  output logic                    terr_dm_ready_o,
  input  logic [TERR_SLOTW-1:0]   terr_dm_slot_i,
  input  logic [TERR_GENW-1:0]    terr_dm_gen_i,
  input  logic [31:0]             terr_dm_epoch_i,
  input  logic                    terr_dm_bd_i,
  input  logic                    terr_dm_f_i,
  input  logic                    terr_dm_mips_i,

  input  logic                    terr_unpin_valid_i,
  output logic                    terr_unpin_ready_o,
  input  logic [TERR_SLOTW-1:0]   terr_unpin_slot_i,
  input  logic [TERR_GENW-1:0]    terr_unpin_gen_i,
  input  logic [31:0]             terr_unpin_epoch_i,

  input  logic                    terr_chk_valid_i,
  input  logic [TERR_SLOTW-1:0]   terr_chk_slot_i,
  input  logic [TERR_GENW-1:0]    terr_chk_gen_i,
  input  logic [31:0]             terr_chk_epoch_i,
  output logic                    terr_chk_valid_o,
  output logic                    terr_chk_stale_o,

  // ---- I28: TERRAIN.SEQ's F-sheet writeback job and its barrier release ---
  output logic                    terr_wb_valid_o,
  input  logic                    terr_wb_ready_i,
  output logic [TERR_SLOTW-1:0]   terr_wb_slot_o,
  output logic [TERR_GENW-1:0]    terr_wb_gen_o,
  output logic [31:0]             terr_wb_epoch_o,
  output logic [31:0]             terr_wb_island_o,
  output logic signed [15:0]      terr_wb_ix_o,
  output logic signed [15:0]      terr_wb_iz_o,
  output logic [31:0]             terr_wb_src_id_o,
  input  logic                    terr_wb_done_valid_i,
  input  logic [TERR_SLOTW-1:0]   terr_wb_done_slot_i,

  // ---- I28 (other end): TERRAIN.RESIDENCY's writeback-ACK barrier --------
  input  logic                    terr_wback_valid_i,
  output logic                    terr_wback_ready_o,
  input  logic [TERR_SLOTW-1:0]   terr_wback_slot_i,
  input  logic [TERR_GENW-1:0]    terr_wback_gen_i,
  input  logic [31:0]             terr_wback_epoch_i,

  // ---- TERRAIN PAGING evidence -------------------------------------------
  // Events, never cycles, except where the name says otherwise. These are the
  // instrument that says the spine carried a beat rather than merely
  // elaborating, which is the whole question this composition has to answer.
  output logic [31:0]             terr_cmd_sets_accepted_o,
  output logic [31:0]             terr_cmd_sets_refused_o,
  output logic [31:0]             terr_cmd_records_emitted_o,
  output logic [31:0]             terr_cmd_crc_fails_o,
  output logic [31:0]             terr_cmd_bridge_errs_o,
  output logic                    terr_seq_busy_o,
  output logic                    terr_seq_done_o,
  output logic [31:0]             terr_seq_records_consumed_o,
  output logic [31:0]             terr_seq_patches_issued_o,
  output logic [31:0]             terr_seq_claims_issued_o,
  output logic [31:0]             terr_seq_claims_refused_o,
  output logic [31:0]             terr_seq_claims_same_o,
  output logic [31:0]             terr_seq_loads_issued_o,
  output logic [31:0]             terr_seq_skipped_not_resident_o,
  output logic [31:0]             terr_seq_frame_faults_o,
  // A tripwire, not a decoration: an answer arrived with nothing waiting for
  // one. `zhao_terrain_seq`'s own header explains why every consequence of it
  // is silent, and the composed bench for that block has already caught a real
  // shim bug with it, so it is a detector that has been SEEN to fire.
  output logic                    terr_seq_err_stray_ans_o,
  output logic [31:0]             terr_res_hits_o,
  output logic [31:0]             terr_res_misses_o,
  output logic [31:0]             terr_res_claims_o,
  output logic [31:0]             terr_res_evictions_o,
  output logic [31:0]             terr_res_crc_failures_o,
  output logic [31:0]             terr_res_resident_o,
  output logic [31:0]             terr_lq_accepted_o,
  output logic [31:0]             terr_lq_issued_o,
  output logic [31:0]             terr_lq_high_water_o,
  output logic [31:0]             terr_pl_pages_loaded_o,
  output logic [31:0]             terr_pl_pages_faulted_o,
  output logic [31:0]             terr_pl_crc_fails_o,
  output logic [31:0]             terr_pl_load_bytes_o,
  output logic [31:0]             terr_pl_guard_denied_o,
  output logic [31:0]             terr_pl_bridge_errs_o,
  // THE INTEGRATION'S OWN OBLIGATION, MADE MEASURABLE. The loader carries a
  // slot ONE BIT WIDER than the directory's handle so a computed 1,024 refuses
  // instead of aliasing onto slot 0. This composition drives that port from a
  // TERR_SLOTW producer, so the extra bit can only ever be zero -- and a
  // counter that proves it is better than a comment that asserts it.
  output logic [31:0]             terr_pl_slot_overflow_o,
  // A REFUSAL IS NOT A FAULT AND MUST NOT LOOK LIKE SILENCE. `pages_refused_o`
  // counts jobs the loader judged BEFORE touching memory (bad slot, unaligned
  // or unreachable source, outside the staging arena, stale epoch) and
  // `fault_verdict_o` names which. Leaving them unexposed cost a diagnosis
  // once already: with loaded=0, faulted=0 and bytes=0 there is no way to tell
  // a refused job from a loader that never started.
  output logic [31:0]             terr_pl_pages_refused_o,
  output logic [3:0]              terr_pl_fault_verdict_o,
  // THE FAILING PAGE, LATCHED. The block emits this trio precisely so that a
  // refusal names WHICH page it refused -- its own header calls it
  // "MEASURE.HISTOGRAM's refuse-loudly lane". Dropping it turns every fault
  // into an anonymous count, which is a refusal that is not loud at all.
  output logic [31:0]             terr_pl_fault_island_o,
  output logic signed [15:0]      terr_pl_fault_ix_o,
  output logic signed [15:0]      terr_pl_fault_iz_o,
  output logic [31:0]             terr_pl_fault_src_id_o,
  output logic [31:0]             terr_pl_incomplete_o,
  output logic [31:0]             terr_pl_hdr_ident_fails_o,
  // THE ARBITER'S OWN STARVATION INSTRUMENT. Rule 5 says starvation must be
  // visible, and `c1_wait_cycles_o` is how. This composition is the first to
  // put two terrain clients on it, so the number it reports is evidence about
  // a fairness contract that had never carried two live clients before.
  output logic [31:0]             terr_hps_c0_bursts_o,
  output logic [31:0]             terr_hps_c1_bursts_o,
  output logic [31:0]             terr_hps_c1_wait_cycles_o,

  // ---- TERRAIN evidence: the sequencer's and the tessellator's ------------
  output logic [PROJ_T_ARENAS-1:0] terr_held_o,
  output logic                    terr_busy_o,
  output logic [31:0]             terr_jobs_accepted_o,
  output logic [31:0]             terr_jobs_no_view_o,
  output logic [31:0]             terr_jobs_rejected_o,
  output logic [31:0]             terr_jobs_empty_o,
  output logic [31:0]             terr_groups_opened_o,
  output logic [31:0]             terr_groups_released_o,
  output logic [31:0]             terr_fills_forwarded_o,
  output logic [31:0]             terr_fills_dropped_o,
  output logic [31:0]             terr_refs_forwarded_o,
  output logic [31:0]             terr_release_unsafe_o,
  output logic [31:0]             terr_tess_vertices_o,
  output logic [31:0]             terr_tess_refs_o,
  output logic [31:0]             terr_tess_rejected_o,
  output logic [31:0]             terr_tess_lod_clamped_o,
  output logic [31:0]             terr_tess_mode_invalid_o,
  output logic                    terr_tess_idle_o,

  // ---- I13: the projector's TRIANGLE OUTPUT -------------------------------
  output logic                    proj_out_valid_o,
  input  logic                    proj_out_ready_i,
  output logic signed [20:0]      proj_out_ax_o,
  output logic signed [20:0]      proj_out_ay_o,
  output logic signed [20:0]      proj_out_bx_o,
  output logic signed [20:0]      proj_out_by_o,
  output logic signed [20:0]      proj_out_cx_o,
  output logic signed [20:0]      proj_out_cy_o,
  output logic [2:0]              proj_out_behind_o,
  output logic [15:0]             proj_out_src_id_o,
  output logic signed [31:0]      proj_out_ad_o,
  output logic signed [31:0]      proj_out_bd_o,
  output logic signed [31:0]      proj_out_cd_o,
  output logic [30:0]             proj_out_aw_o,
  output logic [30:0]             proj_out_bw_o,
  output logic [30:0]             proj_out_cw_o,
  output logic                    proj_out_view_o,
  output logic [7:0]              proj_out_mat_a_o,
  output logic [7:0]              proj_out_mat_b_o,
  output logic [7:0]              proj_out_weight_o,
  output logic                    proj_out_refused_o,
  output logic                    proj_out_missed_o,

  // ---- PROJECTOR evidence --------------------------------------------------
  // `proj_a_view_o` is client A's result VIEW tag. GEOM.PROJ_LANE takes no
  // view (a group holds one view's results), so it has no consumer inside this
  // core and leaves the module named rather than left dangling.
  output logic                    proj_a_view_o,
  output logic [31:0]             proj_replay_triangles_o,
  output logic [31:0]             proj_replay_refused_o,
  output logic [31:0]             proj_replay_missed_o,
  output logic [31:0]             proj_corner_hits_o,
  output logic [31:0]             proj_corner_refusals_o,
  output logic [31:0]             proj_corner_misses_o,
  output logic                    proj_arena_overflow_o,
  output logic                    proj_arena_seal_short_o,
  output logic                    proj_shell_idle_o,
  output logic                    proj_svc_busy_o,
  output logic [31:0]             proj_a_grants_o,
  output logic [31:0]             proj_b_grants_o,
  output logic [31:0]             proj_contended_o,
  output logic [31:0]             proj_mat_refused_o,

  // ---- I15/I16/I17: the compositor's absent neighbours --------------------
  input  logic                    post_view_sel_i,
  input  logic                    post_s_valid_i,
  output logic                    post_s_ready_o,
  input  logic [15:0]             post_s_rgb_i,
  output logic                    post_gd_req_v_o,
  output logic                    post_gd_view_o,
  output logic [POST_XW-3:0]      post_gd_cx_o,
  output logic [POST_YW-3:0]      post_gd_cy_o,
  input  logic                    post_gd_present_i,
  input  logic signed [7:0]       post_gd_dx_i,
  input  logic signed [7:0]       post_gd_dy_i,
  output logic                    post_gg_req_v_o,
  output logic                    post_gg_view_o,
  output logic [POST_XW-3:0]      post_gg_cx_o,
  output logic [POST_YW-3:0]      post_gg_cy_o,
  input  logic                    post_gg_present_i,
  input  logic [15:0]             post_gg_glow_i,
  input  logic                    post_gg_ink_i,
  // The `atm_*` GROUP IS GONE FROM THIS EDGE, 2026-09-19. It is now internal:
  // TWOD.PLANE, TWOD.SAMPLER and POST.COMPOSITE are composed at the end of
  // this module and the atmosphere sheet never leaves. Entry I17 records what
  // changed and what did not.
  input  logic [7:0]              post_bloom_gain_i,
  input  logic                    post_grade_valid_i,
  input  logic                    post_pv_we_i,
  input  logic [1:0]              post_pv_sel_i,
  input  logic [5:0]              post_pv_addr_i,
  input  logic [71:0]             post_pv_data_i,
  input  logic signed [8:0]       post_bias_r_i,
  input  logic signed [8:0]       post_bias_g_i,
  input  logic signed [8:0]       post_bias_b_i,
  input  logic [15:0]             post_flash_rgb_i,
  input  logic [7:0]              post_flash_amt_i,
  input  logic [15:0]             post_ink_rgb_i,
  output logic                    post_hud_req_v_o,
  output logic [POST_XW-1:0]      post_hud_req_x_o,
  output logic [POST_YW-1:0]      post_hud_req_y_o,
  input  logic                    post_hud_valid_i,
  input  logic [15:0]             post_hud_rgb_i,
  output logic                    post_o_valid_o,
  input  logic                    post_o_ready_i,
  output logic [15:0]             post_o_rgb_o,
  output logic [POST_XW-1:0]      post_o_x_o,
  output logic [POST_YW-1:0]      post_o_y_o,
  output logic                    post_o_last_o,
  output logic                    post_echo_valid_o,
  output logic [15:0]             post_echo_rgb_o,

  // ---- COMPOSITOR evidence -------------------------------------------------
  output logic [31:0]             post_displacement_edge_clamps_o,
  output logic [31:0]             post_bloom_cells_contributing_o,
  output logic [31:0]             post_passes_completed_o,
  output logic [31:0]             post_grading_table_missing_o,
  output logic [31:0]             post_plane_missing_o,
  output logic [31:0]             post_line_fill_writes_o,
  output logic [31:0]             post_output_writes_o,
  output logic [31:0]             post_plane_reads_o,
  output logic [31:0]             post_ring_hazard_o,

  // ---- I18/I19: the histogram's events and its host window ----------------
  input  logic                    hist_ev_valid_i,
  input  logic [HIST_LANES-1:0]   hist_ev_lane_valid_i,
  input  logic [HIST_LANES*HIST_EW-1:0] hist_ev_err_i,
  input  logic [15:0]             hist_ev_src_id_i,
  output logic                    hist_ev_ready_o,
  input  logic                    hist_rd_valid_i,
  input  logic [HIST_BINW-1:0]    hist_rd_bin_i,
  output logic                    hist_rd_ready_o,
  output logic                    hist_rd_data_valid_o,
  output logic [HIST_CW-1:0]      hist_rd_count_o,
  output logic                    hist_snap_valid_o,
  output logic [HIST_CW-1:0]      hist_snap_total_o,
  output logic [15:0]             hist_snap_src_id_o,
  output logic [HIST_CW-1:0]      hist_snap_index_o,
  output logic [HIST_CW-1:0]      hist_events_o,
  output logic [HIST_CW-1:0]      hist_updates_o,
  output logic [HIST_CW-1:0]      hist_stall_cycles_o,
  output logic [HIST_CW-1:0]      hist_bin_sat_o,
  output logic [HIST_CW-1:0]      hist_fwd_hits_o,
  output logic [HIST_CW-1:0]      hist_host_conflict_o,
  output logic [HIST_CW-1:0]      hist_snapshots_o,
  output logic [HIST_CW-1:0]      hist_frozen_write_o,

  // ---- I20/LIGHTING SEAM: tied low, see the header ------------------------
  output logic                    light_seam_connected_o,

  // ==========================================================================
  // zhao_shell_top_v2's DECLARATION, CARRIED THROUGH VERBATIM.
  //
  // Comments and all, because the shell is the authority on its own seams and
  // a paraphrase here would be a second, drifting description of them. Every
  // one of these is connected straight to `u_shell` below; this core neither
  // renames nor reinterprets any of them.
  // ==========================================================================
  // ---- clocks + reset (harness-driven, frozen ratios: vid = gpu/2,
  // ---- audio = gpu/4, fixed phase — plan R1) -----------------------------
  input  logic gpu_clk,
  input  logic vid_clk,
  input  logic audio_clk,
  input  logic rst_n,

  // ---- PACKET-H: THE V3 PROGRAMMING CHANNEL -----------------------------
  // Twenty inputs. The historical shell has a command scheduler and an HPS
  // bridge and NO V3 programming channel at all, so these are not a rename
  // of anything -- they are the binding/palette/page path arriving at the
  // shell boundary for the first time. A command-stream decoder would be a
  // second design with its own ABI and tests that this packet's gate does
  // not ask for, and one inserted later sits BEHIND these ports and changes
  // nothing the V2 blocks see.
  input  logic        cfg_valid_i,
  output logic        cfg_ready_o,
  input  logic [1:0]  cfg_op_i,
  input  logic [7:0]  cfg_page_generation_i,
  input  logic [7:0]  cfg_selector_i,
  input  logic [74:0] cfg_row_i,
  input  logic [31:0] cfg_crc32_i,
  output logic        cfg_rsp_valid_o,
  input  logic        cfg_rsp_ready_i,
  output logic [1:0]  cfg_rsp_op_o,
  output logic [3:0]  cfg_rsp_status_o,
  output logic [7:0]  cfg_rsp_page_generation_o,
  output logic [7:0]  active_page_generation_o,
  input  logic        pal_load_valid_i,
  output logic        pal_load_ready_o,
  input  logic [1:0]  pal_load_op_i,
  input  logic [1:0]  pal_load_slot_i,
  input  logic [7:0]  pal_load_gen_i,
  input  logic [7:0]  pal_load_idx_i,
  input  logic [15:0] pal_load_rgb565_i,
  input  logic        pal_load_crc_ok_i,

  // ---- PACKET-H: attribute carriage, ENGINE1 share, clear, sheet --------
  input  logic [46:0]  tri_area2_i,
  input  logic [239:0] tri_invw_plane_i,
  input  logic [239:0] tri_u_over_w_plane_i,
  input  logic [239:0] tri_v_over_w_plane_i,
  input  logic [297:0] tri_flat_request_i,
  input  logic [47:0]  tri_continuation_tail_i,
  input  logic [31:0]  tri_fragment_state_i,
  input  logic         fill_req_ready_i,
  output logic         fill_req_valid_o,
  output logic [31:0]  fill_req_addr_o,
  input  logic         fill_data_valid_i,
  input  logic [15:0]  fill_data_i,
  input  logic         fill_refused_i,
  input  logic [63:0]  frame_clear_word_i,
  input  logic         sheet_req_ready_i,
  output logic         sheet_req_valid_o,
  output logic [1:0]   sheet_req_op_o,
  output logic [31:0]  sheet_req_handle_o,
  output logic [11:0]  sheet_req_texel_o,
  output logic [15:0]  sheet_req_src_id_o,

  // ---- PACKET-H: the video-domain barrier and echo ----------------------
  // `lease_open` is produced by zhao_video_ready_bridge_v2 and the two
  // `barrier_done` levels by zhao_fb_ready_cdc_v2, so the reset-epoch
  // barrier is self-driven and nothing outside declares it complete.
  input  logic        blank_cmd_i,
  input  logic        scanout_ack_i,
  input  logic        frame_swap_valid_i,
  input  logic        frame_swap_slot_i,
  output logic        blank_ack_o,
  output logic        blank_active_o,
  output logic        lease_open_o,
  output logic [1:0]  frame_slot_ready_o,

  // ---- PACKET-H: lifecycle evidence -------------------------------------
  // The gate asks for exact owner and hierarchy census; these are how a
  // reader gets it without a waveform.
  output logic [31:0] v2_requests_accepted_o,
  output logic [31:0] v2_responses_accepted_o,
  output logic [31:0] v2_leases_granted_o,
  output logic [31:0] v2_leases_refused_o,
  output logic [31:0] v2_faults_latched_o,
  output logic [31:0] v2_publications_o,
  output logic [31:0] v2_releases_o,
  output logic [31:0] v2_ready_events_o,
  output logic [31:0] v2_swaps_o,
  output logic [31:0] v2_contentions_o,
  output logic [31:0] v2_clear_handshakes_o,
  output logic [31:0] v2_frames_admitted_o,
  output logic [31:0] v2_blit_leases_acquired_o,
  output logic [31:0] v2_blit_leases_refused_o,

  // ---- FRAME_RING view (harness = HPS, D10; memory_rules.md 4.1) ---------
  input  logic [1:0]  hps_state_i [0:2],
  input  logic [31:0] hps_byte_len_i [0:2],
  output logic        ring_wr_valid_o,
  output logic [1:0]  ring_wr_slot_o,
  output logic [1:0]  ring_wr_state_o,
  input  logic        ring_wr_ready_i,

  // ---- HPS bridge, harness side (memory_rules.md 3) ----------------------
  output logic        hps_req_valid_o,
  output logic        hps_req_write_o,
  output logic [31:0] hps_req_addr_o,
  output logic [6:0]  hps_req_len_o,
  input  logic        hps_req_grant_i,
  output logic        hps_wr_valid_o,
  output logic [63:0] hps_wr_data_o,
  output logic        hps_wr_last_o,
  input  logic        hps_rd_valid_i,
  input  logic [63:0] hps_rd_data_i,
  input  logic        hps_rd_last_i,

  // ---- raw decoded pad state (input_rules.md 1/4) ------------------------
  input  logic [3:0]  pad_present_i,
  input  logic [31:0] pad_buttons_i [0:3],
  input  logic [15:0] pad_lx_i [0:3],
  input  logic [15:0] pad_ly_i [0:3],
  input  logic [15:0] pad_rx_i [0:3],
  input  logic [15:0] pad_ry_i [0:3],

  // ---- audio: ring-read client seam (pairs in) + PCM out -----------------
  input  logic        aud_wr_valid_i,
  input  logic [15:0] aud_wr_l_i,
  input  logic [15:0] aud_wr_r_i,
  output logic        aud_wr_ready_o,
  output logic        aud_refill_req_o,
  output logic [11:0] aud_occupancy_o,
  output logic        pcm_valid_o,
  output logic [15:0] pcm_l_o,
  output logic [15:0] pcm_r_o,
  output logic        underrun_status_o,
  output logic [31:0] audio_underruns_o,

  // ---- displayed pixel stream (vid domain, post-scaler) ------------------
  output logic        px_valid_o,
  output logic [15:0] px_rgb_o,
  output logic [9:0]  px_x_o,
  output logic [7:0]  px_y_o,
  output logic        px_hsync_o,
  output logic        px_vsync_o,
  output logic        px_hblank_o,
  output logic        px_vblank_o,
  output logic        scaler_violation_o,

  // ---- DEBUG.CRC (gpu domain): the displayed-stream CRC ------------------
  output logic [31:0] crc_frame_o,
  output logic        crc_valid_o,
  output logic [31:0] crc_bytes_o,
  output logic        crc_size_err_o,

  // ---- frame boundary observability --------------------------------------
  output logic        gpu_tick_o,
  output logic [31:0] gpu_tick_frame_id_o,
  output logic        gpu_tick_repeated_o,
  output logic [0:0]  gpu_complete_slot_o,
  output logic [63:0] deadline_faults_o,     // FRAMECTL (vid)
  output logic [63:0] frame_cycles_o,        // FRAMECTL (vid)

  // ---- CMD observability --------------------------------------------------
  output logic [2:0]  slot_state_o [0:2],
  output logic        fence_valid_o,
  output logic [1:0]  fence_slot_o,
  output logic        fence_ok_o,
  output logic [7:0]  fence_status_o,
  output logic [1:0]  mode_act_o,
  output logic        dma_done_o,
  output logic [7:0]  dma_status_o,
  output logic        blit_done_o,
  output logic [7:0]  blit_status_o,

  // ---- INPUT observability ------------------------------------------------
  output logic [639:0] pad_frame_flat_o,
  output logic [15:0]  pad_sequence_o [0:3],
  output logic [63:0]  input_gaps_o,
  output logic [7:0]   rumble_duty_o [0:3],
  output logic [3:0]   rumble_active_o,
  output logic [3:0]   rumble_pwm_o,
  output logic [63:0]  rumble_drops_o,

  // ---- DEBUG.COUNTERS read window ----------------------------------------
  input  logic        cnt_snap_ready_i,
  output logic        cnt_snap_valid_o,
  output logic [15:0] cnt_snap_id_o,
  output logic [63:0] cnt_snap_value_o,
  output logic        cnt_window_open_o,
  output logic        cnt_cat_violation_o,

  // ---- MEM observability + shell integrity tripwires ---------------------
  output logic [31:0] guard_violations_o,    // both guards, summed
  output logic [63:0] starvation_o,
  output logic        init_done_o,
  output logic [31:0] refresh_stalls_o,
  output logic [31:0] bank_conflicts_o,
  output logic [31:0] scanout_preempted_o,
  output logic [31:0] hps_err_count_o,
  output logic        shell_err_wfifo_o,     // write queue over/underflow
  output logic        shell_err_route_o,     // burst from an impossible client
  output logic        shell_err_cdc_o,       // starvation sample moved at tick
  output logic        shell_err_framer_o,    // record queue overflow (glue 3)

  // ---- RENDER: the geometry front door ----------------------------------
  // The console's first render path. GEOM.BINNER -> RASTER.TILE_PIPE ->
  // RASTER.FBWRITE -> MEM.GUARD -> the arbiter ENGINE0 port, which zhao_pkg has
  // always called a "reserved guaranteed slot" and which was tied to zero until
  // now.
  //
  // The triangle port sits at the SHELL edge because CMD.SCHEDULER does not
  // feed it yet. That is provisional and says so: when the command front end
  // grows a draw path these become internal and nothing else here changes.
  //
  // What it draws is FLAT-shaded. zhao_raster_tile_pipe carries one colour,
  // alpha, depth and texel across a triangle because interpolating them is
  // GEOM.SETUP work and GEOM.SETUP has no attribute input yet. This is the
  // path, not the picture.
  input  logic        render_frame_begin_i,
  input  logic        render_frame_end_i,
  input  logic [5:0]  render_grid_w_i,
  input  logic [5:0]  render_grid_h_i,

  // THE TRIANGLE PORT IS NO LONGER AT THIS EDGE. `render_tri_valid_i`,
  // `render_tri_ready_o`, the nine edge-function words, the top-left mask, the
  // six corners, the scan box and the source id were boundary inputs here and
  // are now driven INTERNALLY by `zhao_geom_setup`, which is composed below
  // and is their real producer. The paragraph above says "when the command
  // front end grows a draw path these become internal" -- that is half true and
  // the half that mattered was different: what was missing was not CMD but the
  // SETUP block itself, and it existed all along. The door is now fed; what
  // feeds GEOM.CLIP in front of it is entry I24.
  //
  // ---- D22 TREAD 10: the geometry memory clients -----------------------------
  // The last thing the bench still PLAYED was memory itself. Every earlier
  // tread took something the bench supplied and gave it to a composed block;
  // GEOM.MESHFETCH and GEOM.ASSETFETCH still had their guard grants answered
  // and their beats fabricated by hand, so the whole staircase rested on a
  // memory that granted immediately and answered in one cycle.
  //
  // These two ports put those fetchers behind the REAL MEM.GUARD and
  // VRAM.ARBITER that this shell already instantiates, on the arbiter's two
  // previously unused client slots. The bench keeps the fetchers -- relocating
  // them into production is a separate concern and is recorded as such -- but
  // it stops inventing the answers.
  //
  // Contention is the point. Everything measured in treads 6 through 9 assumed
  // a memory that never says no, and `prefetch_stall_o` was connected before
  // this tread precisely so its uncontended reading (27) exists to compare
  // against.
  input  var zhao_guard_req_t geom_guard_req_i,
  output var zhao_guard_rsp_t geom_guard_rsp_o,
  // ...and the beats coming back. Until this tread the shell had ONE reader,
  // so read data was wired straight to the scanout packer. Now it has two, and
  // which one a returning word belongs to is a fact that has to be tracked
  // rather than assumed.
  output var logic            geom_beat_valid_o,
  output var logic [63:0]     geom_beat_data_o,
  output var logic            geom_beat_last_o,

  input  logic [63:0] render_fill_word_i,
  input  logic [63:0] render_clear_word_i,
  input  logic [31:0] render_state_i,
  input  logic [ 7:0] render_src_a_i,
  input  logic [23:0] render_texel_rgb_i,
  input  logic [ 7:0] render_texel_a_i,
  input  logic [ 7:0] render_texel_idx_i,

  input  logic [26:0] render_fb_base_i,
  input  logic [15:0] render_fb_stride_i,

  // WHO HOLDS THE FRAMEBUFFER-WRITE LEASE THIS FRAME.
  // 0 = DEBUG.FRAMEBLIT, 1 = RASTER.FBWRITE.
  //
  // ONE SIGNAL, BOTH GUARDS. The first version of this wiring hardwired
  // `fb_writer` to 0 inside the blit guard and 1 inside the render guard, so
  // each compared the client against its OWN constant and BOTH writers passed
  // at once -- which is precisely the corruption the lease exists to prevent,
  // reintroduced by the wiring of the block that prevents it. The owner is one
  // value, and both guards are told the same one.
  //
  // Provisional at the shell edge: VIDEO.SLOTMGR already owns one lease at a
  // time with a generation, and this becomes that lease's owner field once
  // CMD.SCHEDULER selects the writer. Until then it is an input so a bench can
  // exercise either writer, and it defaults to the blit at the caller.
  input  logic        fb_writer_i,

  output logic        render_drain_done_o,
  output logic        render_busy_o,
  output logic [31:0] render_pixels_o,
  output logic [31:0] render_bursts_o,
  output logic        render_stream_error_o,
  // The frame transaction. `render_drained_o` is the ONLY signal a frame
  // controller may publish a slot on: it means every word handed to the guard
  // has been RETIRED by the arbiter. `render_busy_o` falls when the last beat
  // is merely accepted, several stages earlier.
  output logic        render_drained_o,
  output logic        render_fatal_o,
  output logic [31:0] render_issued_words_o,
  output logic [31:0] render_retired_words_o,
  output logic        render_overflow_o,
  output logic        render_fragment_error_o,

  // ==========================================================================
  // SURFACE. The pair below is composed and CLOSED ON ITSELF -- SURFACE.STAMP
  // is SURFACE.SHEET's only client and SURFACE.SHEET is SURFACE.STAMP's only
  // store, so the request, page and write channels are all internal wires and
  // none of them appears here. What DOES appear is the three ends that have no
  // owner in this tree (entries I30, I31, I32) plus the pair's evidence.
  // ==========================================================================

  // I30: the SurfaceStamp dispatch. CMD.SCHEDULER has no path to it.
  input  logic               surf_cmd_valid_i,
  output logic               surf_cmd_ready_o,
  input  logic        [31:0] surf_cmd_handle_i,
  input  logic        [ 7:0] surf_cmd_operation_i,
  input  logic        [ 7:0] surf_cmd_tag_i,
  input  logic        [15:0] surf_cmd_strength_i,
  input  logic signed [31:0] surf_cmd_tx_i,
  input  logic signed [31:0] surf_cmd_ty_i,
  input  logic signed [31:0] surf_cmd_radius_i,
  input  logic signed [31:0] surf_cmd_ring_width_i,
  input  logic signed [31:0] surf_cmd_env_x0_i,
  input  logic signed [31:0] surf_cmd_env_z0_i,
  input  logic signed [31:0] surf_cmd_env_x1_i,
  input  logic signed [31:0] surf_cmd_env_z1_i,
  input  logic               surf_cmd_blend_en_i,
  input  logic        [ 2:0] surf_cmd_blend_i,
  input  logic        [ 2:0] surf_cmd_age_shift_i,
  input  logic               surf_cmd_field_en_i,
  input  logic        [15:0] surf_cmd_src_id_i,

  // I31: the field-driven brush. FIELD.SEQ.STAMP is not built.
  input  logic        surf_fld_valid_i,
  output logic        surf_fld_ready_o,
  input  logic [31:0] surf_fld_tag_op_i,
  input  logic [15:0] surf_fld_strength_i,

  // I32: `stamp_results` -> TERRAIN.BAKE, which is not composed.
  output logic        surf_res_valid_o,
  input  logic        surf_res_ready_i,
  output logic [11:0] surf_res_texel_o,
  output logic [ 7:0] surf_res_tag_o,
  output logic [ 7:0] surf_res_strength_o,
  output logic [ 7:0] surf_res_before_o,
  output logic [15:0] surf_res_src_id_o,

  // SURFACE.SHEET's spare response fields. NOT a gap: SURFACE.STAMP consumes
  // the two it needs (`status`, `strength`) and these three are the block's
  // own evidence, which leaves the module rather than being dropped.
  output logic [ 1:0] surf_pg_op_o,
  output logic [ 7:0] surf_pg_tag_o,
  output logic [15:0] surf_pg_src_id_o,

  // residency_status and the pair's counters
  output logic [SURF_SLOTS-1:0] surf_res_occupancy_o,
  output logic        surf_res_busy_o,
  output logic        surf_res_overflow_o,
  output logic        surf_sheet_wr_miss_o,
  output logic [15:0] surf_sheet_wr_miss_src_id_o,
  output logic        surf_sheet_idle_o,
  output logic [31:0] surf_sheet_texels_touched_o,
  output logic        surf_stamp_done_o,
  output logic        surf_stamp_rejected_o,
  output logic        surf_stamp_idle_o,
  output logic [31:0] surf_stamps_o,
  output logic [31:0] surf_stamp_texels_touched_o,

  // ==========================================================================
  // I24: THE PARTICLE DRAW PATH'S TWO ENDS.
  //
  // The MIDDLE is now internal -- PART.COLLIDE's records fork into
  // PART.PROJECT, which shares the one projector, and on into PART.LADDER and
  // the two endpoints. What leaves the module is:
  //
  //   * the owner values PART.PROJECT and the ladder need and nothing here
  //     produces: the per-(particle, camera) hold state (I23's absent DDR), the
  //     ladder's species/governor inputs, and the particle's RGB. PART.TABLE's
  //     `v_colour_o` is an INDEX and no palette block exists to turn it into a
  //     colour, which is why the colour is a value here rather than a lookup;
  //   * the two endpoints' packets, whose customer is the same absent GEOM
  //     replay/setup path I11, I12 and I13 name;
  //   * the rung write-back, which is where the hold state has to GO for the
  //     next frame to have any.
  //
  // The SCISSOR is NOT here: PART.SOFT takes the same mode-derived rectangle
  // GEOM.CLIP does (GLUE 1), because it is the console's own pass geometry and
  // not a second opinion about it.
  // ==========================================================================
  input  logic signed [31:0] part_prj_base_radius_i,
  input  logic               part_prj_view_i,
  input  logic        [15:0] part_prj_trail_i,
  input  logic               part_prj_narrow_i,
  input  logic               part_prj_protected_i,
  input  logic        [ 2:0] part_prj_gov_floor_i,
  input  logic        [ 2:0] part_prj_prev_rung_i,
  input  logic        [ 3:0] part_prj_hold_i,
  input  logic               part_prj_first_i,
  input  logic        [ 7:0] part_prj_r_i,
  input  logic        [ 7:0] part_prj_g_i,
  input  logic        [ 7:0] part_prj_b_i,
  input  logic        [15:0] part_prj_src_id_i,

  // the rung and the hold state it produced, for the next frame's store
  output logic               part_rung_valid_o,
  input  logic               part_rung_ready_i,
  output logic        [ 2:0] part_rung_o,
  output logic        [ 3:0] part_rung_hold_o,
  output logic               part_rung_changed_o,
  output logic        [15:0] part_rung_src_id_o,

  // PART.EXPAND's three-vertex screen fan
  output logic               part_exp_valid_o,
  input  logic               part_exp_ready_i,
  output logic signed [21:0] part_exp_ax_o,
  output logic signed [21:0] part_exp_ay_o,
  output logic signed [21:0] part_exp_bx_o,
  output logic signed [21:0] part_exp_by_o,
  output logic signed [21:0] part_exp_cx_o,
  output logic signed [21:0] part_exp_cy_o,
  output logic signed [31:0] part_exp_d_o,
  output logic        [ 7:0] part_exp_r_o,
  output logic        [ 7:0] part_exp_g_o,
  output logic        [ 7:0] part_exp_b_o,
  output logic               part_exp_depth_test_o,
  output logic               part_exp_depth_write_o,
  output logic        [15:0] part_exp_src_id_o,

  // PART.SOFT's scissored whole-pixel span
  output logic               part_sft_valid_o,
  input  logic               part_sft_ready_i,
  output logic signed [12:0] part_sft_min_x_o,
  output logic signed [12:0] part_sft_max_x_o,
  output logic signed [12:0] part_sft_min_y_o,
  output logic signed [12:0] part_sft_max_y_o,
  output logic signed [31:0] part_sft_d_o,
  output logic        [ 7:0] part_sft_r_o,
  output logic        [ 7:0] part_sft_g_o,
  output logic        [ 7:0] part_sft_b_o,
  output logic               part_sft_depth_test_o,
  output logic               part_sft_depth_write_o,
  output logic        [15:0] part_sft_src_id_o,

  // the draw path's evidence
  output logic [31:0] part_prj_projected_o,
  output logic [31:0] part_prj_behind_o,
  output logic [31:0] part_prj_geom_grants_o,
  output logic [31:0] part_prj_part_grants_o,
  output logic [31:0] part_prj_contended_o,
  output logic [31:0] part_prj_size_sat_o,
  output logic [31:0] part_prj_slot_pressure_o,
  output logic [31:0] part_prj_tag_collision_o,
  output logic [31:0] part_prj_ladder_unexpected_o,
  output logic [31:0] part_lad_decisions_o,
  output logic [31:0] part_lad_changes_o,
  output logic [31:0] part_lad_held_o,
  output logic [31:0] part_lad_gov_forced_o,
  output logic [31:0] part_exp_polygons_o,
  output logic [31:0] part_sft_sprites_o,

  // ---- SDR PHY pins (behavioural model in the tb wrapper; D2) ------------
  output logic        phy_cs_n_o,
  output logic        phy_ras_n_o,
  output logic        phy_cas_n_o,
  output logic        phy_we_n_o,
  output logic [12:0] phy_a_o,
  output logic [1:0]  phy_ba_o,
  output logic [15:0] phy_dq_o,
  output logic        phy_dq_oe_o,
  output logic [1:0]  phy_dqm_o,
  input  logic [15:0] phy_dq_i,

  // --------------------------------------------------------------------------
  // TWOD: the plane descriptors, the sprite descriptors, the sampler's assets
  // and the sprite colour stream.  Added 2026-09-19 with TWOD.SAMPLER.
  // --------------------------------------------------------------------------
  // WHAT IS AND IS NOT A GAP HERE, because the group is large and it would be
  // easy to read all of it as one:
  //   * the two DESCRIPTOR groups are the CMD seam. SetPlane and the sprite
  //     display list are commands, `zhao_cmd_decoder` emits record headers and
  //     not decoded descriptors, and the executor that would turn one into the
  //     other is the same absent path entries I14 and I30 describe. GAP, and
  //     it is the SAME gap those two already name rather than a new one.
  //   * the three LOAD groups are ASSETS. Entry I17's own sentence about the
  //     grading curves -- "generated ASSETS by design, so their load port is
  //     legitimately external" -- covers a texture page, a palette and a
  //     binding exactly. NOT a gap.
  //   * `twod_sc_*` is the sprite colour, and it has no consumer HERE because
  //     POST.COMPOSITE's `hud_*` port is a raster-order random access and
  //     TWOD.SPRITE walks in descriptor order. GAP, and a NEW one -- see I17.
  input  logic                    twod_pd_valid_i,
  output logic                    twod_pd_ready_o,
  input  logic                    twod_pd_slot_i,
  input  logic [1:0]              twod_pd_role_i,
  input  logic [1:0]              twod_pd_blend_i,
  input  logic [7:0]              twod_pd_opacity_i,
  input  logic                    twod_pd_format_i,
  input  logic [15:0]             twod_pd_width_i,
  input  logic [15:0]             twod_pd_height_i,
  input  logic                    twod_pd_wrap_u_i,
  input  logic                    twod_pd_wrap_v_i,
  input  logic signed [31:0]      twod_pd_a_i,
  input  logic signed [31:0]      twod_pd_b_i,
  input  logic signed [31:0]      twod_pd_c_i,
  input  logic signed [31:0]      twod_pd_d_i,
  input  logic signed [31:0]      twod_pd_u0_i,
  input  logic signed [31:0]      twod_pd_v0_i,
  input  logic [1:0]              twod_pd_view_mask_i,
  input  logic [7:0]              twod_pd_palette_i,

  input  logic                    twod_sd_valid_i,
  output logic                    twod_sd_ready_o,
  input  logic signed [15:0]      twod_sd_x_i,
  input  logic signed [15:0]      twod_sd_y_i,
  input  logic [15:0]             twod_sd_w_i,
  input  logic [15:0]             twod_sd_h_i,
  input  logic signed [31:0]      twod_sd_u_i,
  input  logic signed [31:0]      twod_sd_v_i,
  input  logic signed [31:0]      twod_sd_a00_i,
  input  logic signed [31:0]      twod_sd_a01_i,
  input  logic signed [31:0]      twod_sd_a10_i,
  input  logic signed [31:0]      twod_sd_a11_i,
  input  logic [2:0]              twod_sd_format_i,
  input  logic [7:0]              twod_sd_palette_i,
  input  logic [15:0]             twod_sd_tint_i,
  input  logic [1:0]              twod_sd_blend_i,
  input  logic [1:0]              twod_sd_view_mask_i,
  input  logic [7:0]              twod_sd_order_i,
  input  logic [15:0]             twod_sd_src_id_i,

  input  logic                    twod_ld_page_we_i,
  input  logic [TWOD_PAW-1:0]     twod_ld_page_addr_i,
  input  logic [15:0]             twod_ld_page_data_i,
  input  logic                    twod_ld_pal_we_i,
  input  logic [TWOD_PALAW-1:0]   twod_ld_pal_addr_i,
  input  logic [15:0]             twod_ld_pal_data_i,
  input  logic                    twod_ld_bind_we_i,
  input  logic [TWOD_BSW-1:0]     twod_ld_bind_sel_i,
  input  logic [TWOD_PAW-1:0]     twod_ld_bind_base_i,
  input  logic [3:0]              twod_ld_bind_lstride_i,
  input  logic [3:0]              twod_ld_bind_lheight_i,
  input  logic                    twod_atm_slot_i,
  input  logic signed [31:0]      twod_line_scroll_i,

  output logic                    twod_sc_valid_o,
  input  logic                    twod_sc_ready_i,
  output logic [15:0]             twod_sc_rgb_o,
  output logic signed [15:0]      twod_sc_x_o,
  output logic signed [15:0]      twod_sc_y_o,
  output logic [15:0]             twod_sc_tint_o,
  output logic [1:0]              twod_sc_blend_o,
  output logic [7:0]              twod_sc_order_o,
  output logic [15:0]             twod_sc_src_id_o,
  output logic                    twod_sc_last_o,

  // ---- TWOD evidence -------------------------------------------------------
  output logic [31:0]             twod_plane_pixels_o,
  output logic [31:0]             twod_plane_refused_role_o,
  output logic [31:0]             twod_plane_refused_blend_o,
  output logic [31:0]             twod_plane_skipped_view_o,
  output logic [31:0]             twod_plane_wrap_fail_o,
  output logic [31:0]             twod_sprite_descriptors_o,
  output logic [31:0]             twod_sprite_skipped_view_o,
  output logic [31:0]             twod_sprite_refused_o,
  output logic [31:0]             twod_sprite_pixels_o,
  output logic [31:0]             twod_samples_o,
  output logic [31:0]             twod_plane_samples_o,
  output logic [31:0]             twod_sprite_samples_o,
  output logic [31:0]             twod_clut8_samples_o,
  output logic [31:0]             twod_rgb565_samples_o,
  output logic [31:0]             twod_texel_wrapped_o,
  output logic [31:0]             twod_page_oob_o,
  output logic [31:0]             twod_bind_missing_o,
  output logic [31:0]             twod_fmt_refused_o,
  output logic [31:0]             twod_pal_refused_o,
  output logic [31:0]             twod_skipped_fill_o,
  output logic [31:0]             twod_atm_underrun_o,
  output logic [31:0]             twod_walk_stalls_o,
  output logic [31:0]             twod_sprite_stalls_o,
  output logic [31:0]             twod_tint_unapplied_o,
  output logic [31:0]             twod_pair_lost_o,

  // --------------------------------------------------------------------------
  // CMD.DECODER's record headers and verdict.  Added 2026-09-19.
  // --------------------------------------------------------------------------
  // OUTPUTS ONLY, and every one of them is driven by real logic inside this
  // module -- see section 7b. They are on the edge because the decoder's
  // consumer (the command executor) does not exist yet, and a stream that ends
  // in a wire is pruned dead logic wearing a port's name, which this campaign
  // counts as an absent function rather than a present one.
  output logic        cmd_rec_valid_o,
  output logic [15:0] cmd_rec_opcode_o,
  output logic [15:0] cmd_rec_bytes_o,
  output logic [31:0] cmd_rec_source_id_o,
  output logic [31:0] cmd_rec_index_o,
  output logic        cmd_decode_done_o,
  output logic [ 7:0] cmd_decode_error_o,
  output logic [31:0] cmd_bytes_consumed_o,
  output logic [31:0] cmd_commands_o,

  // --------------------------------------------------------------------------
  // CMD.EXEC's evidence.  Added 2026-09-19 with section 7c.
  // --------------------------------------------------------------------------
  // OUTPUTS ONLY, driven by real logic below. The executor writes the
  // projector's matrix bank and dispatches SURFACE.STAMP, and BOTH of those go
  // to internal consumers -- so without these ports the only thing observable
  // about whether a command was executed would be a downstream side effect two
  // subsystems away. These are the numbers a bench reads to say "the packet
  // became console state", and every one of them is fired by a named case in
  // tests/command/cmd_exec_directed.cpp.
  //
  // `cmd_exec_unsupported_o` IS THE HONEST ONE. It counts records the ABI
  // defines and this executor has no arm for -- BeginFrame, EndFrame,
  // DrawForm, every reserved opcode. It is the distance between the command
  // surface and the executor expressed as a NUMBER rather than as prose in a
  // header, and it is expected to be large today.
  output logic [31:0] cmd_exec_committed_o,
  output logic [31:0] cmd_exec_abandoned_o,
  output logic [31:0] cmd_exec_views_o,
  output logic [31:0] cmd_exec_stamps_o,
  output logic [31:0] cmd_exec_stamp_overflow_o,
  output logic [31:0] cmd_exec_view_refused_o,
  output logic [31:0] cmd_exec_src_truncated_o,
  output logic [31:0] cmd_exec_unsupported_o
);

  // ==========================================================================
  // ELABORATION GUARDS.
  //
  // Inside `initial begin ... end` because Quartus 17.0 rejects a bare
  // module-scope `if` with "syntax error near text: `if`; expecting
  // `endmodule`" -- and Verilator's `--lint-only` accepts the bare form with 0
  // diagnostics, so a clean lint says nothing whatever about this. See
  // CLAUDE.md, 2026-09-08.
  // ==========================================================================
  initial begin
    if (GEOM_ARENA_W + GEOM_INDEX_W > GEOM_PAY_A_W)
      $fatal(1, "zhao_console_core: the geometry rider is %0d bits (ARENA_W %0d + INDEX_W %0d) but GEOM_PAY_A_W is %0d",
             GEOM_ARENA_W + GEOM_INDEX_W, GEOM_ARENA_W, GEOM_INDEX_W, GEOM_PAY_A_W);
    if (POST_LINE_W < 384)
      $fatal(1, "zhao_console_core: POST_LINE_W is %0d, narrower than the Z60 view (384)", POST_LINE_W);
    if (PART_REC_W != 128)
      $fatal(1, "zhao_console_core: PART_REC_W is %0d; particle128 (amendment C2) is 128", PART_REC_W);
  end

  // ==========================================================================
  // THE LIGHTING SEAM. Tied low on purpose -- see the header for exactly what
  // must be connected here, and why none of it is.
  // ==========================================================================
  assign light_seam_connected_o = 1'b0;

  // ==========================================================================
  // GLUE 1: THE VIDEO MODE IS THE COMPOSITOR'S PASS GEOMETRY.  REAL.
  //
  // `mode_act_o` is the shell's LATCHED active mode, so the compositor's pass
  // size follows the console's real mode instead of a constant. In Duo the
  // block runs once per 256x192 VIEW (its own header), which is why the Duo
  // arm is the view and not the 512-wide canvas.
  //
  // The numbers are zhao_pkg's: ZHAO_TIMING[Z60].h_active = 384,
  // [STORM].h_active = 320, and ZHAO_DUO_VIEW_W/H = 256/192.
  // ==========================================================================
  localparam logic [1:0]         MODE_Z60_C     = ZHAO_MODE_Z60;
  localparam logic [1:0]         MODE_STORM_C   = ZHAO_MODE_STORM;
  localparam logic [POST_XW-1:0] POST_W_Z60_C   = 384;
  localparam logic [POST_XW-1:0] POST_W_STORM_C = 320;
  localparam logic [POST_XW-1:0] POST_W_DUO_C   = 256;
  localparam logic [POST_YW-1:0] POST_H_FULL_C  = 240;
  localparam logic [POST_YW-1:0] POST_H_DUO_C   = 192;

  logic [POST_XW-1:0] post_frame_w_c;
  logic [POST_YW-1:0] post_frame_h_c;

  always_comb begin
    case (mode_act_o)
      MODE_Z60_C: begin
        post_frame_w_c = POST_W_Z60_C;
        post_frame_h_c = POST_H_FULL_C;
      end
      MODE_STORM_C: begin
        post_frame_w_c = POST_W_STORM_C;
        post_frame_h_c = POST_H_FULL_C;
      end
      default: begin
        post_frame_w_c = POST_W_DUO_C;
        post_frame_h_c = POST_H_DUO_C;
      end
    endcase
  end

  // ==========================================================================
  // GLUE 2: THE FRAME EDGE IS THE TICK.  REAL.
  //
  // `gpu_tick_o` is FRAMECTL's frame boundary as the shell already publishes
  // it. The particle generation, the compositor pass and the measurement
  // interval all advance on it, so all three are on the console's real cadence
  // rather than on three private pulses.
  // ==========================================================================
  wire core_tick_c = gpu_tick_o;

  // ==========================================================================
  // PARTICLES: STATE -> UPDATE -> {COLLIDE, SPAWN} -> STATE
  // ==========================================================================
  wire                  ps_prt_valid, ps_prt_ready;
  wire [PART_REC_W-1:0] ps_prt_record;

  wire                  pu_out_valid;
  wire [PART_REC_W-1:0] pu_out_record;
  wire                  pu_out_survive;
  wire [3:0]            pu_out_events;

  wire                  pc_p_ready;
  wire                  pc_c_valid, pc_c_alive;
  wire [PART_REC_W-1:0] pc_c_record;
  wire [PART_REC_W-1:0] pc_c_spawn_record;
  wire [3:0]            pc_c_events;
  wire                  ps_vrd_ready;

  wire                  sp_par_ready;
  wire                  sp_chl_valid, sp_chl_ready;
  wire [PART_REC_W-1:0] sp_chl_record;

  // PART.PROJECT's acceptance, branch P of the fork below. Declared here rather
  // than beside its instance because the fork reads it eight hundred lines
  // earlier than the draw path is composed.
  wire                  pp_p_ready;


  // --------------------------------------------------------------------------
  // GLUE 3: THE ONE-TO-TWO FORK, NOW ON PART.COLLIDE'S OUTPUT.
  //
  // REPLUMBED 2026-09-19 by owner ruling I4
  // (`reports/RULING-I4-COLLISION-SPAWN-20260919.md`). It used to sit on
  // PART.UPDATE's verdict, feeding PART.COLLIDE and PART.SPAWN IN PARALLEL.
  // That arrangement is what made spawn-on-collision impossible: PART.SPAWN saw
  // the particle BEFORE it had been through the only block that knows whether
  // it hit anything, so the collision event bit could never be true and
  // `part_spawn_by_event2_o` was structurally stuck at zero.
  //
  // The chain is now STATE -> UPDATE -> COLLIDE -> {STATE write-back, SPAWN}.
  // PART.UPDATE has exactly ONE consumer and needs no fork at all; PART.COLLIDE
  // has two, and they are the two this shape was written for. Note what did NOT
  // change: no arithmetic moved into this file, and nothing was adapted -- the
  // ruling REMOVES a seam (PART.UPDATE's four `col_*_i` ports) and this is the
  // wire order that follows from it.
  //
  // The fork itself is unchanged in kind. A plain `ready & ready` fork stalls
  // both consumers whenever either is busy AND presents the beat twice to
  // whichever accepted first. This keeps one "already took it" bit per branch
  // instead, so each consumer sees the beat exactly once and the beat retires
  // when both have taken it.
  //
  // Neither branch's VALID reads the other branch's READY, so there is no
  // combinational loop through the consumers -- which is the failure mode this
  // shape exists to avoid, not a property to be argued about afterwards.
  // PART.STATE's `vrd_ready_o` and PART.SPAWN's `par_ready_o` are both pure
  // functions of their own state, so that property still holds after the move.
  // --------------------------------------------------------------------------
  // A THIRD BRANCH, 2026-09-19, and it is the SAME SHAPE rather than a new
  // mechanism. Entry I24's "the particle ring composed above carries particle128
  // WORLD records and stops there" is what this closes: PART.PROJECT is the
  // draw pass's head and it reads the ring's own records, so no new record
  // boundary was invented to feed it.
  //
  // The property the shape was chosen for still holds, and it is the one to
  // check: NO BRANCH'S VALID READS ANOTHER BRANCH'S READY, so there is no
  // combinational loop through the consumers. `zhao_part_project.p_ready_o` is
  // `a_ready_i && sel_p && !slot_full` -- the shared projector's grant and its
  // own slot state, neither of which can see PART.STATE's or PART.SPAWN's
  // ready. The cost is the documented one: the beat retires when all THREE have
  // taken it, so a stalled draw path stalls the tick rather than dropping a
  // particle, which is what `part_prj_slot_pressure_o` measures.
  logic fork_vrd_done_q, fork_spw_done_q, fork_prj_done_q;

  wire fork_vrd_valid_c = pc_c_valid && !fork_vrd_done_q;
  wire fork_spw_valid_c = pc_c_valid && !fork_spw_done_q;
  wire fork_prj_valid_c = pc_c_valid && !fork_prj_done_q;
  wire fork_vrd_take_c  = fork_vrd_valid_c && ps_vrd_ready;
  wire fork_spw_take_c  = fork_spw_valid_c && sp_par_ready;
  wire fork_prj_take_c  = fork_prj_valid_c && pp_p_ready;
  wire fork_vrd_held_c  = fork_vrd_done_q || fork_vrd_take_c;
  wire fork_spw_held_c  = fork_spw_done_q || fork_spw_take_c;
  wire fork_prj_held_c  = fork_prj_done_q || fork_prj_take_c;
  wire pc_out_ready_c   = fork_vrd_held_c && fork_spw_held_c && fork_prj_held_c;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      fork_vrd_done_q <= 1'b0;
      fork_spw_done_q <= 1'b0;
      fork_prj_done_q <= 1'b0;
    end else if (pc_c_valid && pc_out_ready_c) begin
      fork_vrd_done_q <= 1'b0;
      fork_spw_done_q <= 1'b0;
      fork_prj_done_q <= 1'b0;
    end else begin
      if (fork_vrd_take_c) fork_vrd_done_q <= 1'b1;
      if (fork_spw_take_c) fork_spw_done_q <= 1'b1;
      if (fork_prj_take_c) fork_prj_done_q <= 1'b1;
    end
  end

  // --------------------------------------------------------------------------
  // GLUE 4: THE SURVIVE VERDICT AND THE PARENT ID, ACROSS PART.COLLIDE.
  //
  // PART.STATE's write-back needs ONE survive bit and TWO blocks decide it:
  // PART.UPDATE kills on lifetime (`out_survive_o`) and PART.COLLIDE kills on a
  // DIE response (`c_alive_o`). PART.COLLIDE has no survive input and no
  // passthrough for one, so the update's verdict has to cross it beside the
  // record. The parent ORDINAL (glue 5) has to cross for the same reason now
  // that PART.SPAWN sits downstream: it belongs to the particle, not to the
  // cycle.
  //
  // THE ALIGNMENT IS THE WHOLE POINT, and it is structural rather than argued:
  // PART.COLLIDE is "one beat in, one beat out, fixed latency" with
  // `p_ready_o = !c_valid_o || c_ready_i`, so exactly one beat is ever in
  // flight and its record register is loaded on `p_valid_i && p_ready_o`. These
  // registers are loaded on THE SAME condition, rebuilt from the same two
  // wires, so neither can separate from the record it describes -- there is no
  // second enable for them to drift across. The EVENTS cross inside
  // PART.COLLIDE itself, on its own copy of that enable, for the same reason
  // and because bit 2 is that block's to write.
  // --------------------------------------------------------------------------
  wire pu_take_c = pu_out_valid && pc_p_ready;

  logic                  pc_survive_q;
  logic [PART_PID_W-1:0] pc_ordinal_q;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      pc_survive_q <= 1'b0;
      pc_ordinal_q <= '0;
    end else if (pu_take_c) begin
      pc_survive_q <= pu_out_survive;
      pc_ordinal_q <= part_ordinal_q;
    end
  end

  wire ps_vrd_survive_c = pc_c_alive && pc_survive_q;

  // --------------------------------------------------------------------------
  // GLUE 5: THE PARENT ID.
  //
  // PART.SPAWN seeds its hash with a parent id and the particle128 record
  // (amendment C2) has no id field, so the console assigns one: the particle's
  // ORDINAL within the generation, cleared at every tick. See the header entry
  // I9 -- this is a decision taken here, not a port that was tied off, and it
  // is wrong if ids must survive compaction.
  //
  // It is counted at PART.UPDATE's retire, which is one particle per ordinal,
  // and then TRAVELS WITH THE PARTICLE through glue 4. Reading the live counter
  // at PART.SPAWN's input would be off by PART.COLLIDE's beat and would give
  // two particles the same identity across a stall.
  // --------------------------------------------------------------------------
  logic [PART_PID_W-1:0] part_ordinal_q;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n)           part_ordinal_q <= '0;
    else if (core_tick_c) part_ordinal_q <= '0;
    else if (pu_take_c)   part_ordinal_q <= part_ordinal_q + 1'b1;
  end

  wire part_capacity_full_c;   // I8: PART.STATE -> PART.SPAWN, internal

  // --------------------------------------------------------------------------
  // PART.TABLE -- THE DESCRIPTOR OWNER.  ENTRIES I2 AND I3, CLOSED.
  //
  // `fpga/rtl/particles/zhao_part_table.sv` was built on 2026-09-19, named for
  // these two entries, and instantiated NOWHERE for a day. This is the missing
  // final step -- the uncashed cheque CLAUDE.md has a chapter about, cashed.
  //
  // FOUR READS, ALL COMBINATIONAL, ALL IN THE SAME CYCLE AS THEIR INDEX, and
  // that is a property of the table rather than a convenience taken here: its
  // header states why an M10K cannot serve them and prices the variant that
  // could. Nothing below registers an index or a reply, so there is no second
  // enable for a request and a response to drift across -- which is this
  // repository's own metadata-swap defect and the reason the shape matters.
  //
  // THE WIRING IS PORT-FOR-PORT, and the table's header carries the map it was
  // written against. NO ARITHMETIC IS PERFORMED IN THIS FILE: every connection
  // below is one net to one net, same name, same width. The one place that was
  // NOT true is now true -- PART.COLLIDE emitted no index, so `c_index_i` had no
  // driver and the only composer-side answer was to instantiate a SECOND
  // `zhao_part_record` here and re-slice `species` out of a record the collide
  // instance has already decoded. That would be a block added by the composer
  // and a second decode of a frozen layout. `zhao_part_collide.d_index_o` is the
  // honest fix: one output on the block that already holds the value.
  //
  // AND THE TIMING IS NOT THE HAZARD IT LOOKS LIKE. `d_index_o` is
  // combinational off `p_record_i`, and PART.COLLIDE's four `d_*_i` inputs are
  // read in the same instant as that record. An index taken from the SAME WIRE
  // in the SAME instant cannot separate from the descriptor it selects.
  //
  // WHAT IS STILL A BOUNDARY: the per-frame LOAD (entry I33). Nothing in this
  // core fills the table -- CMD.SCHEDULER has no path to it, the same absent
  // owner as I14 and I30 -- so `part_tbl_ld_*` leaves the module. That is a
  // narrower gap than I2/I3 and a different one: the table EXISTS and ANSWERS
  // here; what is missing is the host that writes it.
  // --------------------------------------------------------------------------
  wire [6:0]                  ptb_u_index;
  wire [3:0]                  ptb_u_recipe;
  wire [PART_AGE_W-1:0]       ptb_u_lifetime;
  wire [PART_AGE_W-1:0]       ptb_u_age_mark;
  wire [7:0]                  ptb_u_drag;
  wire signed [PART_VEL_W-1:0] ptb_u_grav;
  wire signed [PART_VEL_W-1:0] ptb_u_strength;
  wire signed [PART_POS_W-1:0] ptb_u_cx, ptb_u_cy, ptb_u_cz;
  wire signed [PART_VEL_W-1:0] ptb_u_p0, ptb_u_p1, ptb_u_p2;

  wire [3:0]                  ptb_v_index;
  wire [5:0]                  ptb_v_size;
  wire [7:0]                  ptb_v_colour;

  wire [6:0]                  ptb_c_index;
  wire [2:0]                  ptb_c_response;
  wire signed [PART_FX_W-1:0] ptb_c_restitution, ptb_c_friction, ptb_c_damping;

  wire [6:0]                  ptb_s_species;
  wire [1:0]                  ptb_s_event;
  wire                        ptb_s_known;
  wire [6:0]                  ptb_s_child_spc;
  wire [4:0]                  ptb_s_count;

  zhao_part_table #(
    .SPECIES_N (PART_SPECIES_N),
    .AGE_W     (PART_AGE_W),
    .POS_W     (PART_POS_W),
    .VEL_W     (PART_VEL_W),
    .FX_W      (PART_FX_W),
    .LD_W      (PART_TBL_LD_W)
    // CRV_N is left at its default 16, which is the ceiling PART.UPDATE's
    // 4-bit `crv_index_o` sets. There is no console-level knob for it because
    // there is nothing here that could legitimately choose a smaller one.
  ) u_part_table (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // I33: the per-frame load, out at the boundary.
    .ld_valid_i (part_tbl_ld_valid_i),
    .ld_ready_o (part_tbl_ld_ready_o),
    .ld_sel_i   (part_tbl_ld_sel_i),
    .ld_index_i (part_tbl_ld_index_i),
    .ld_event_i (part_tbl_ld_event_i),
    .ld_data_i  (part_tbl_ld_data_i),

    // REAL: PART.UPDATE's species descriptor.  I2, update half.
    .u_index_i   (ptb_u_index),
    .u_recipe_o  (ptb_u_recipe),
    .u_lifetime_o(ptb_u_lifetime),
    .u_age_mark_o(ptb_u_age_mark),
    .u_drag_o    (ptb_u_drag),
    .u_grav_o    (ptb_u_grav),
    .u_strength_o(ptb_u_strength),
    .u_cx_o      (ptb_u_cx),
    .u_cy_o      (ptb_u_cy),
    .u_cz_o      (ptb_u_cz),
    .u_p0_o      (ptb_u_p0),
    .u_p1_o      (ptb_u_p1),
    .u_p2_o      (ptb_u_p2),

    // REAL: PART.UPDATE's size/colour curve.  I3, whole.
    .v_index_i (ptb_v_index),
    .v_size_o  (ptb_v_size),
    .v_colour_o(ptb_v_colour),

    // REAL: PART.COLLIDE's slice, addressed by that block's own decode.
    .c_index_i      (ptb_c_index),
    .c_response_o   (ptb_c_response),
    .c_restitution_o(ptb_c_restitution),
    .c_friction_o   (ptb_c_friction),
    .c_damping_o    (ptb_c_damping),

    // REAL: PART.SPAWN's child rule.  I2, spawn half.
    .s_species_i  (ptb_s_species),
    .s_event_i    (ptb_s_event),
    .s_known_o    (ptb_s_known),
    .s_child_spc_o(ptb_s_child_spc),
    .s_count_o    (ptb_s_count),

    .loads_update_o (part_tbl_loads_update_o),
    .loads_collide_o(part_tbl_loads_collide_o),
    .loads_spawn_o  (part_tbl_loads_spawn_o),
    .loads_curve_o  (part_tbl_loads_curve_o),
    .load_refused_o (part_tbl_load_refused_o)
  );

  zhao_part_state #(
    .CAPACITY  (PART_CAPACITY),
    .CHILD_D   (PART_CHILD_D),
    .SPECIES_N (PART_SPECIES_N),
    .REC_W     (PART_REC_W)
  ) u_part_state (
    .clk          (gpu_clk),
    .rst_n        (rst_n),
    .tick_start_i (core_tick_c),
    .tick_busy_o  (part_tick_busy_o),
    .tick_done_o  (part_tick_done_o),

    // I8 CLOSED 2026-09-19. PART.STATE now tells PART.SPAWN when the generation
    // is full, so the capacity backstop is a real internal producer->consumer
    // edge instead of a pin the board had to drive.
    .capacity_full_o (part_capacity_full_c),

    // I1: the generation store. Harness = memory; MEM.HPS.BRIDGE has no
    // particle client port, so there is no route to it inside this core.
    .rd_valid_i   (part_rd_valid_i),
    .rd_ready_o   (part_rd_ready_o),
    .rd_record_i  (part_rd_record_i),
    .rd_last_i    (part_rd_last_i),

    // REAL: straight into PART.UPDATE.
    .prt_valid_o  (ps_prt_valid),
    .prt_ready_i  (ps_prt_ready),
    .prt_record_o (ps_prt_record),

    // REAL: the verdict comes back from PART.COLLIDE, with the survive bit
    // that crossed it (glue 4), through branch V of the fork (glue 3).
    .vrd_valid_i   (fork_vrd_valid_c),
    .vrd_ready_o   (ps_vrd_ready),
    .vrd_survive_i (ps_vrd_survive_c),
    .vrd_record_i  (pc_c_record),

    // REAL: children from PART.SPAWN.
    .chl_valid_i  (sp_chl_valid),
    .chl_ready_o  (sp_chl_ready),
    .chl_record_i (sp_chl_record),

    .wr_valid_o   (part_wr_valid_o),
    .wr_ready_i   (part_wr_ready_i),
    .wr_record_o  (part_wr_record_o),

    .survivors_o                  (part_survivors_o),
    .children_written_o           (part_children_written_o),
    .children_dropped_capacity_o  (part_children_dropped_capacity_o),
    .staging_stall_cycles_o       (part_staging_stall_cycles_o),
    .species_refused_o            (part_species_refused_o)
  );

  zhao_part_update #(
    .SPECIES_N (PART_SPECIES_N),
    .REC_W     (PART_REC_W),
    .AGE_W     (PART_AGE_W)
  ) u_part_update (
    .clk        (gpu_clk),
    .rst_n      (rst_n),

    // REAL: from PART.STATE.
    .in_valid_i (ps_prt_valid),
    .in_ready_o (ps_prt_ready),
    .in_record_i(ps_prt_record),

    // REAL: I2 CLOSED 2026-09-19. The species descriptor comes from PART.TABLE
    // above, field for field, same cycle as the index.
    .spc_index_o   (ptb_u_index),
    .spc_recipe_i  (ptb_u_recipe),
    .spc_lifetime_i(ptb_u_lifetime),
    .spc_age_mark_i(ptb_u_age_mark),
    .spc_drag_i    (ptb_u_drag),
    .spc_grav_i    (ptb_u_grav),
    .spc_strength_i(ptb_u_strength),
    .spc_cx_i      (ptb_u_cx),
    .spc_cy_i      (ptb_u_cy),
    .spc_cz_i      (ptb_u_cz),
    .spc_p0_i      (ptb_u_p0),
    .spc_p1_i      (ptb_u_p1),
    .spc_p2_i      (ptb_u_p2),

    // I5: FIELD.SEQ.FLOW is not composed.
    .fld_valid_i(part_fld_valid_i),
    .fld_ax_i   (part_fld_ax_i),
    .fld_ay_i   (part_fld_ay_i),
    .fld_az_i   (part_fld_az_i),

    // I4 IS CLOSED. There is no step-6 port here any more: owner ruling
    // 2026-09-19 RETIRED `col_valid_i`/`col_vx_i`/`col_vy_i`/`col_vz_i`, and
    // PART.COLLIDE below performs step 6 on this block's own output. The four
    // tie-offs that used to sit here are gone rather than driven, so this row's
    // PART.UPDATE area goes DOWN, not up.

    // REAL: I3 CLOSED 2026-09-19. The size/colour curve is PART.TABLE's fourth
    // slice. Its index is `age_next_c[AGE_W-1 -: 4]` INSIDE PART.UPDATE -- the
    // advanced age, which exists nowhere a cycle earlier -- which is the reason
    // the table's curve read is combinational and cannot become an M10K.
    .crv_index_o (ptb_v_index),
    .crv_size_i  (ptb_v_size),
    .crv_colour_i(ptb_v_colour),

    // REAL: the verdict, straight into PART.COLLIDE. ONE consumer, no fork --
    // the fork moved to PART.COLLIDE's output when ruling I4 put PART.SPAWN
    // downstream of it (glue 3).
    .out_valid_o  (pu_out_valid),
    .out_ready_i  (pc_p_ready),
    .out_record_o (pu_out_record),
    .out_survive_o(pu_out_survive),
    .out_refused_o(part_upd_beat_refused_o),
    .out_events_o (pu_out_events),
    .out_colour_en_o(part_colour_en_o),
    .out_colour_o (part_colour_o),

    .particles_updated_o     (part_updated_o),
    .particles_refused_o     (part_upd_refused_o),
    .particles_died_by_age_o (part_died_by_age_o),
    .velocity_saturations_o  (part_velocity_saturations_o),
    .position_saturations_o  (part_position_saturations_o),
    // `collisions_applied_o` was here. It was retired with the ports it
    // counted; `part_collisions_applied_o` is driven from PART.COLLIDE below.
    .hist_sel_i              (part_hist_sel_i),
    .hist_val_o              (part_hist_val_o)
  );

  zhao_part_collide #(
    .REC_W (PART_REC_W),
    .POS_W (PART_POS_W),
    .VEL_W (PART_VEL_W),
    .NRM_W (PART_NRM_W),
    .FX_W  (PART_FX_W)
  ) u_part_collide (
    .clk      (gpu_clk),
    .rst_n    (rst_n),

    // REAL: straight from PART.UPDATE, with its three own event bits riding
    // beside the record. Bit 2 arrives zero and this block fills it (ruling I4).
    .p_valid_i(pu_out_valid),
    .p_ready_o(pc_p_ready),
    .p_record_i(pu_out_record),
    .p_events_i(pu_out_events),

    // REAL: I2 CLOSED 2026-09-19. `d_index_o` is NEW on this block and is the
    // whole reason the entry could close -- the collider publishes the species
    // it has already decoded off `p_record_i`, and PART.TABLE answers on the
    // same wire in the same instant. Neither this file nor any adapter decodes
    // the record a second time.
    .d_index_o      (ptb_c_index),
    .d_response_i   (ptb_c_response),
    .d_restitution_i(ptb_c_restitution),
    .d_friction_i   (ptb_c_friction),
    .d_damping_i    (ptb_c_damping),

    // I6: TERRAIN.PATCH emits heights and no surface normal.
    .t_valid_i (part_ter_valid_i),
    .t_height_i(part_ter_height_i),
    .t_nx_i    (part_ter_nx_i),
    .t_ny_i    (part_ter_ny_i),
    .t_nz_i    (part_ter_nz_i),

    // I7: the one plane, a per-frame owner value with no CMD path.
    .pl_en_i(part_plane_en_i),
    .pl_nx_i(part_plane_nx_i),
    .pl_ny_i(part_plane_ny_i),
    .pl_nz_i(part_plane_nz_i),
    .pl_c_i (part_plane_c_i),

    // REAL: forked to PART.STATE's write-back channel and to PART.SPAWN
    // (glue 3). `c_spawn_record_o` is the POST-CONTACT record by ruling I4 §3,
    // reversible at PART.COLLIDE's `CHILD_AT_POST_CONTACT` parameter and
    // nowhere else -- the choice does not live in this file.
    .c_valid_o  (pc_c_valid),
    .c_ready_i  (pc_out_ready_c),
    .c_record_o (pc_c_record),
    .c_alive_o  (pc_c_alive),
    .c_contact_o(part_contact_o),
    .c_response_o(part_response_o),
    .c_refused_o(part_col_refused_o),
    .c_events_o (pc_c_events),
    .c_spawn_record_o (pc_c_spawn_record),

    .contacts_ignore_o           (part_contacts_ignore_o),
    .contacts_die_o              (part_contacts_die_o),
    .contacts_stick_o            (part_contacts_stick_o),
    .contacts_slide_o            (part_contacts_slide_o),
    .contacts_bounce_o           (part_contacts_bounce_o),
    .contacts_terrain_o          (part_contacts_terrain_o),
    .contacts_plane_o            (part_contacts_plane_o),
    .already_inside_at_entry_o   (part_already_inside_at_entry_o),
    .terrain_sample_unavailable_o(part_terrain_sample_unavailable_o),
    .response_refused_o          (part_response_refused_o),
    .field_clamps_o              (part_field_clamps_o),
    // The console's collision counter, finally driven by a block that can move
    // it. Before ruling I4 this port was structurally stuck at zero and header
    // entry I4 warned against reading it as "no collisions occurred".
    .collision_events_o          (part_collisions_applied_o)
  );

  zhao_part_spawn #(
    .REC_W     (PART_REC_W),
    .SPECIES_N (PART_SPECIES_N),
    .PID_W     (PART_PID_W),
    .TICK_W    (PART_TICK_W)
  ) u_part_spawn (
    .clk         (gpu_clk),
    .rst_n       (rst_n),
    .tick_start_i(core_tick_c),

    // REAL: branch S of the fork on PART.COLLIDE's output, with the four FROZEN
    // events (bit 2 filled in by the block that observes a collision), the
    // ruled POST-CONTACT parent record, the ordinal that crossed PART.COLLIDE
    // with its particle, and the shell's real frame id as the tick seed.
    .par_valid_i (fork_spw_valid_c),
    .par_ready_o (sp_par_ready),
    .par_record_i(pc_c_spawn_record),
    .par_id_i    (pc_ordinal_q),
    .par_events_i(pc_c_events),
    .tick_i      (gpu_tick_frame_id_o),

    // REAL: I2 CLOSED 2026-09-19. PART.SPAWN presents {species, event} on
    // entering S_EVAL and consumes the reply in that same cycle, so this read is
    // combinational too -- reading it a cycle early would mean a second copy of
    // this block's event priority encoder, which IS its determinism contract.
    .spc_species_o  (ptb_s_species),
    .spc_event_o    (ptb_s_event),
    .spc_known_i    (ptb_s_known),
    .spc_child_spc_i(ptb_s_child_spc),
    .spc_count_i    (ptb_s_count),

    // REAL: children straight into PART.STATE's staging channel.
    .chl_valid_o(sp_chl_valid),
    .chl_ready_i(sp_chl_ready),
    .chl_record_o(sp_chl_record),

    // I8: PART.STATE exposes no capacity-full level.
    .cap_full_i(part_capacity_full_c),

    .children_requested_o     (part_children_requested_o),
    .children_emitted_o       (part_children_emitted_o),
    .children_refused_o       (part_children_refused_o),
    .spawn_by_event0_o        (part_spawn_by_event0_o),
    .spawn_by_event1_o        (part_spawn_by_event1_o),
    .spawn_by_event2_o        (part_spawn_by_event2_o),
    .spawn_by_event3_o        (part_spawn_by_event3_o),
    .refused_count_gt_max_o   (part_refused_count_gt_max_o),
    .refused_unknown_species_o(part_refused_unknown_species_o),
    .refused_capacity_o       (part_refused_capacity_o),
    .max_children_in_tick_o   (part_max_children_in_tick_o)
  );

  // ==========================================================================
  // GEOMETRY: SKIN -> GROUP_SEQ -> the SHARED PROJECTOR -> the LANE -> back.
  //
  // This is the composition `design/prod_manifest.yml` says the shared
  // projector was waiting for: "the producer is still absent, so this is not
  // yet adoptable on its own". `zhao_geom_group_seq` is that producer, and the
  // loop below closes -- job in, vertices through client A, landings counted,
  // arena sealed, handle out.
  // ==========================================================================
  wire                    gs_v_valid, gs_v_ready;
  wire signed [31:0]      gs_v_x, gs_v_y, gs_v_z;

  wire                    gs_a_valid, gs_a_ready;
  wire signed [31:0]      gs_a_vx, gs_a_vy, gs_a_vz;
  wire                    gs_a_view;
  wire [GEOM_PAY_A_W-1:0] gs_a_payload;

  wire                     pj_a_valid;
  wire signed [20:0]       pj_a_x, pj_a_y;
  wire signed [31:0]       pj_a_d;
  wire [30:0]              pj_a_w;
  wire                     pj_a_behind;
  wire [GEOM_PAY_A_W-1:0]  pj_a_payload;

  // ---- the shared port, between PART.PROJECT and the subsystem -------------
  // `gs_a_*` is still GEOM.GROUP_SEQ's output and `pj_a_*` is still what the
  // arena lane consumes; those two names did NOT move, so the geometry path
  // reads exactly as it did. What is new is the pair in the middle: `pa_a_*` is
  // the multiplexed request PART.PROJECT presents to client A, and `sv_a_*` is
  // client A's result on its way back into the demux. Entry I24.
  wire                     pa_a_valid, pa_a_ready;
  wire signed [31:0]       pa_a_vx, pa_a_vy, pa_a_vz;
  wire                     pa_a_view;
  wire [GEOM_PAY_A_W-1:0]  pa_a_payload;

  wire                     sv_a_valid;
  wire signed [20:0]       sv_a_x, sv_a_y;
  wire signed [31:0]       sv_a_d;
  wire [30:0]              sv_a_w;
  wire                     sv_a_behind;
  wire [GEOM_PAY_A_W-1:0]  sv_a_payload;


  wire                     gs_open, gs_seal;
  wire [GEOM_ARENA_W-1:0]  gs_open_arena, gs_seal_arena;
  wire [GEOM_GEN_W-1:0]    ln_open_gen;
  wire [GEOM_ARENA_W-1:0]  gs_rider_arena;
  wire [GEOM_INDEX_W-1:0]  gs_rider_index;
  wire [GEOM_PAY_A_W-1:0]  ln_rider_payload;
  wire                     ln_fill_landed;
  wire [GEOM_ARENA_W-1:0]  ln_fill_arena;

  // ==========================================================================
  // GEOM.VDECODE -> GEOM.POSE's PALETTE STORE -> GEOM.SKIN.  ENTRY I10, WHOLE.
  //
  // The decoder takes one 32-byte vertex record and hands the skinner the six
  // fields it wants, under the same names and the same widths. Nothing sits
  // between them: no repack, no requantisation, no width surgery. That is the
  // whole test of whether two blocks written months apart actually meet, and
  // these two do.
  //
  // WHAT USED TO NOT MEET, and is now closed: the decoder also emits
  // `d_bone0_o` and `d_bone1_o`, the two palette ADDRESSES for this vertex, and
  // GEOM.SKIN wants the two 3x4 MATRICES those addresses select. That was a
  // MISSING BLOCK, not missing wiring -- `zhao_geom_pose_decode` streams the
  // palette one bone per beat while GEOM.SKIN wants two whole matrices latched
  // with the vertex, and a stream and a random access do not meet.
  //
  // `zhao_geom_pose_palette` is that block. It stores the decoded palette in
  // RAM, answers two reads per vertex, and passes the vertex through unchanged
  // beside them. So the chain composed here is
  //
  //     GEOM.POSE decode --(one bone per beat)--> palette store
  //     GEOM.VDECODE     --(vertex + bone0/bone1)--> palette store
  //     palette store    --(vertex + A + B)--> GEOM.SKIN
  //
  // and GEOM.SKIN's `a_m_i`/`b_m_i` are internal wires rather than a boundary.
  //
  // NOTHING IS COMPUTED IN THIS FILE. The store indexes, it does not skin; the
  // decoder decodes, it does not store. The hidden adapter this composer must
  // never contain would have been the palette memory written inline here, and
  // it is a named, tested, separately-linted block instead.
  //
  // WHAT IS STILL MISSING IS ONE LEVEL UP: the decoder's own source -- the clip
  // page and the skeleton bake -- has no producer in this tree. That is entry
  // I29, and it is a smaller and more precisely named gap than I10 was.
  //
  // THE REFUSAL PATH IS FORWARDED, NOT DROPPED. `d_refused_o` is raised
  // INSTEAD of `d_valid_o`, so a refused record is silent at the skinner by
  // design; if this composer swallowed the flag, a malformed asset and an
  // absent asset would be indistinguishable from outside.
  // ==========================================================================
  // I25: one ratified vertex format, named rather than literal. See the header.
  localparam logic [2:0] GEOM_VERTEX_FORMAT_C = 3'd0;

  // The palette size, shared by the decoder and the store so a mismatch is one
  // edit rather than two. 32 is `zhao_geom_pose_decode`'s MAX_BONES default and
  // spec/creature_rules.md 2.2's ceiling.
  localparam int GEOM_POSE_BONES_C = 32;

  wire               vd_d_valid, vd_d_ready;
  wire signed [31:0] vd_d_x, vd_d_y, vd_d_z;
  wire        [ 6:0] vd_d_w0;
  wire               vd_d_rigid;
  wire        [15:0] vd_d_src_id;
  wire        [15:0] vd_d_bone0, vd_d_bone1;

  // Decoder -> store, the palette beat.
  wire               pd_out_valid, pd_out_ready;
  wire        [ 4:0] pd_out_bone;
  wire signed [31:0] pd_out_m [12];

  // Store -> skinner, the vertex with its two matrices.
  wire               pal_o_valid, pal_o_ready;
  wire signed [31:0] pal_o_x, pal_o_y, pal_o_z;
  wire        [ 6:0] pal_o_w0;
  wire               pal_o_rigid;
  wire        [15:0] pal_o_src_id;
  wire signed [31:0] pal_a_m [12];
  wire signed [31:0] pal_b_m [12];

  zhao_geom_vdecode #(
    .SRCW (16)
  ) u_geom_vdecode (
    .clk        (gpu_clk),
    .rst_n      (rst_n),

    // I23: GEOM.ASSETFETCH is the producer and is not composed; see the header.
    .v_valid_i  (geom_vd_v_valid_i),
    .v_ready_o  (geom_vd_v_ready_o),
    .v_bytes_i  (geom_vd_v_bytes_i),
    .v_format_i (GEOM_VERTEX_FORMAT_C),   // I25: assigned here, not tied off
    .v_src_id_i (geom_vd_v_src_id_i),

    // REAL: the decoded vertex into GEOM.SKIN.
    .d_valid_o  (vd_d_valid),
    .d_ready_i  (vd_d_ready),
    .d_x_o      (vd_d_x),
    .d_y_o      (vd_d_y),
    .d_z_o      (vd_d_z),
    .d_w0_o     (vd_d_w0),
    .d_rigid_o  (vd_d_rigid),
    .d_src_id_o (vd_d_src_id),

    // REAL: the two palette addresses, into the store. This was I10's
    // remaining half. The attribute path below still leaves at the edge.
    .d_bone0_o  (vd_d_bone0),
    .d_bone1_o  (vd_d_bone1),

    .d_nx_o     (geom_vd_d_nx_o),
    .d_ny_o     (geom_vd_d_ny_o),
    .d_nz_o     (geom_vd_d_nz_o),
    .d_u_o      (geom_vd_d_u_o),
    .d_v_o      (geom_vd_d_v_o),

    .d_refused_o     (geom_vd_refused_o),
    .d_reserved_nz_o (geom_vd_reserved_nz_o),
    .d_w0_illegal_o  (geom_vd_w0_illegal_o),
    .d_format_bad_o  (geom_vd_format_bad_o),

    .vertices_o    (geom_vd_vertices_o),
    .reserved_nz_o (geom_vd_reserved_nz_count_o),
    .w0_illegal_o  (geom_vd_w0_illegal_count_o),
    .format_bad_o  (geom_vd_format_bad_count_o)
  );

  // --------------------------------------------------------------------------
  // GEOM.POSE's decoder. Its palette output now has a consumer, which is the
  // whole reason it is composed here: an instantiation with nothing reading it
  // would be a disconnected implementation with extra steps.
  //
  // The parameters are LEFT AT THEIR DEFAULTS on purpose. MUL_LANES_QUAT = 1
  // and MUL_LANES_MAT = 1 are owner ruling R4 (2026-09-09, "relax the 1
  // bone/clock rule"): one shared multiplier lane per engine, 4 DSP instead of
  // 18. Restating them here would put a second copy of a ruling in a composer.
  // --------------------------------------------------------------------------
  zhao_geom_pose_decode #(
    .MAX_BONES (GEOM_POSE_BONES_C)
  ) u_geom_pose_decode (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // I29: the clip page and the skeleton bake have no producer in this tree.
    .start_i       (geom_pose_start_i),
    .busy_o        (geom_pose_busy_o),
    .bone_count_i  (geom_pose_bone_count_i),
    .root_dx_i     (geom_pose_root_dx_i),
    .root_dy_i     (geom_pose_root_dy_i),
    .root_dz_i     (geom_pose_root_dz_i),
    .bone_idx_o    (geom_pose_bone_idx_o),
    .bone_parent_i (geom_pose_bone_parent_i),
    .bone_tx_i     (geom_pose_bone_tx_i),
    .bone_ty_i     (geom_pose_bone_ty_i),
    .bone_tz_i     (geom_pose_bone_tz_i),
    .quat_w_i      (geom_pose_quat_w_i),
    .quat_x_i      (geom_pose_quat_x_i),
    .quat_y_i      (geom_pose_quat_y_i),
    .quat_z_i      (geom_pose_quat_z_i),
    .inv_rest_i    (geom_pose_inv_rest_i),

    // REAL: one bone per beat, into the palette store.
    .out_valid_o (pd_out_valid),
    .out_ready_i (pd_out_ready),
    .out_bone_o  (pd_out_bone),
    .out_m_o     (pd_out_m),

    .done_o             (geom_pose_done_o),
    .palettes_decoded_o (geom_pose_palettes_decoded_o)
  );

  // --------------------------------------------------------------------------
  // GEOM.POSE's palette store. The block entry I10 named as missing.
  //
  // `pal_begin_i` is `geom_pose_start_i` ITSELF, not a copy and not a tie-off:
  // the pulse that starts a decode is exactly the pulse that makes the previous
  // pose unreadable. Wiring them together is what makes "a vertex that arrives
  // during a decode" report itself on `geom_pal_bone_unset_o` rather than
  // silently reading half of one pose and half of another.
  // --------------------------------------------------------------------------
  zhao_geom_pose_palette #(
    .BONES (GEOM_POSE_BONES_C),
    .SRCW  (16)
  ) u_geom_pose_palette (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    .pal_begin_i (geom_pose_start_i),

    // REAL: from GEOM.POSE's decoder.
    .wr_valid_i (pd_out_valid),
    .wr_ready_o (pd_out_ready),
    .wr_bone_i  (pd_out_bone),
    .wr_m_i     (pd_out_m),

    // REAL: from GEOM.VDECODE, vertex and both palette addresses.
    .v_valid_i  (vd_d_valid),
    .v_ready_o  (vd_d_ready),
    .v_x_i      (vd_d_x),
    .v_y_i      (vd_d_y),
    .v_z_i      (vd_d_z),
    .v_w0_i     (vd_d_w0),
    .v_rigid_i  (vd_d_rigid),
    .v_bone0_i  (vd_d_bone0),
    .v_bone1_i  (vd_d_bone1),
    .v_src_id_i (vd_d_src_id),

    // REAL: the vertex and its two matrices, into GEOM.SKIN.
    .o_valid_o  (pal_o_valid),
    .o_ready_i  (pal_o_ready),
    .o_x_o      (pal_o_x),
    .o_y_o      (pal_o_y),
    .o_z_o      (pal_o_z),
    .o_w0_o     (pal_o_w0),
    .o_rigid_o  (pal_o_rigid),
    .o_src_id_o (pal_o_src_id),
    .a_m_o      (pal_a_m),
    .b_m_o      (pal_b_m),

    .vertices_served_o (geom_pal_vertices_served_o),
    .bones_written_o   (geom_pal_bones_written_o),
    .bone_oob_o        (geom_pal_bone_oob_o),
    .bone_unset_o      (geom_pal_bone_unset_o)
  );

  zhao_geom_skin #(
    .MUL_LANES (GEOM_MUL_LANES)
  ) u_geom_skin (
    .clk       (gpu_clk),
    .rst_n     (rst_n),

    // REAL: from the palette store, which took it from GEOM.VDECODE. ENTRY I10
    // IS CLOSED -- vertex and matrices arrive together from a real producer.
    .v_valid_i (pal_o_valid),
    .v_ready_o (pal_o_ready),
    .v_x_i     (pal_o_x),
    .v_y_i     (pal_o_y),
    .v_z_i     (pal_o_z),
    .v_w0_i    (pal_o_w0),
    .v_rigid_i (pal_o_rigid),
    .v_src_id_i(pal_o_src_id),
    .a_m_i     (pal_a_m),
    .b_m_i     (pal_b_m),

    // REAL: skinned, view-independent vertices into the group sequencer.
    .o_valid_o (gs_v_valid),
    .o_ready_i (gs_v_ready),
    .o_x_o     (gs_v_x),
    .o_y_o     (gs_v_y),
    .o_z_o     (gs_v_z),
    .o_src_id_o(geom_skin_src_id_o),

    .vertices_transformed_o (geom_skin_vertices_transformed_o)
  );

  zhao_geom_group_seq #(
    .ARENAS      (GEOM_ARENAS),
    .DEPTH       (GEOM_DEPTH),
    .NVIEWS      (GEOM_NVIEWS),
    .GEN_W       (GEOM_GEN_W),
    .PAYLOAD_A_W (GEOM_PAY_A_W)
  ) u_geom_group_seq (
    .clk             (gpu_clk),
    .rst_n           (rst_n),

    // I11: the job port; GEOM.SETUP, the replay customer, is not composed.
    .job_valid_i     (geom_job_valid_i),
    .job_ready_o     (geom_job_ready_o),
    .job_count_i     (geom_job_count_i),
    .job_view_mask_i (geom_job_view_mask_i),
    .job_src_id_i    (geom_job_src_id_i),

    // REAL: from GEOM.SKIN.
    .v_valid_i (gs_v_valid),
    .v_ready_o (gs_v_ready),
    .v_x_i     (gs_v_x),
    .v_y_i     (gs_v_y),
    .v_z_i     (gs_v_z),

    // REAL: client A of the shared projection service.
    .a_valid_o  (gs_a_valid),
    .a_ready_i  (gs_a_ready),
    .a_vx_o     (gs_a_vx),
    .a_vy_o     (gs_a_vy),
    .a_vz_o     (gs_a_vz),
    .a_view_o   (gs_a_view),
    .a_payload_o(gs_a_payload),

    // REAL: the lane owns the rider layout and answers with the generation.
    .open_o        (gs_open),
    .open_arena_o  (gs_open_arena),
    .open_gen_i    (ln_open_gen),
    .rider_arena_o (gs_rider_arena),
    .rider_index_o (gs_rider_index),
    .rider_payload_i(ln_rider_payload),
    .seal_o        (gs_seal),
    .seal_arena_o  (gs_seal_arena),

    // REAL: landings, so the seal waits for arrival and not acceptance.
    .fill_landed_i (ln_fill_landed),
    .fill_arena_i  (ln_fill_arena),

    // I11: the sealed handle and its release.
    .grp_valid_o (geom_grp_valid_o),
    .grp_ready_i (geom_grp_ready_i),
    .grp_arena_o (geom_grp_arena_o),
    .grp_gen_o   (geom_grp_gen_o),
    .grp_count_o (geom_grp_count_o),
    .grp_view_o  (geom_grp_view_o),
    .grp_src_id_o(geom_grp_src_id_o),
    .rel_valid_i (geom_rel_valid_i),
    .rel_arena_i (geom_rel_arena_i),

    .groups_opened_o     (geom_groups_opened_o),
    .groups_sealed_o     (geom_groups_sealed_o),
    .vertices_sent_o     (geom_vertices_sent_o),
    .landings_o          (geom_landings_o),
    .jobs_refused_o      (geom_jobs_refused_o),
    .alloc_stall_cycles_o(geom_alloc_stall_cycles_o),
    .rel_unheld_o        (geom_rel_unheld_o),
    .seal_early_o        (geom_seal_early_o)
  );

  // ==========================================================================
  // GEOM.CLIP -> GEOM.SETUP -> THE SHELL'S TRIANGLE DOOR.  REAL, all three.
  //
  // Entry I13 refused to wire the projector's replayed CORNERS into
  // `render_kx0_i`, because that port wants EDGE FUNCTIONS and renaming one
  // into the other is arithmetic invented in a composer. That refusal stands
  // and this is not a way around it: `zhao_geom_setup` is the block whose
  // contract IS that arithmetic -- E_i(px,py) = kx_i*px + ky_i*py + kc_i,
  // exact, in subpixel squared -- and it was sitting unbuilt-into-anything
  // while the door it fits went to the module edge instead.
  //
  // The match was checked port by port before anything was typed, because "it
  // is drop-in" is the sentence this repository has learned to distrust:
  //   out_kx0_o/ky0/kc0, kx1/ky1/kc1, kx2/ky2/kc2 -> render_k*_i   (23/23/48)
  //   out_tl_o                                    -> render_tl_i   (3)
  //   out_ax_o..out_cy_o                          -> render_a..c_i (21)
  //   out_min_x_o..out_max_y_o                    -> render_*_i    (12)
  //   out_src_id_o                                -> render_src_id_i (16)
  //   out_valid_o / out_ready_i                   -> render_tri_valid_i /
  //                                                  render_tri_ready_o
  // Every width is identical and every name is the counterpart's. `out_area2_o`
  // is the one output the door does not take -- 2A is setup's evidence, not the
  // rasteriser's input -- so it leaves this module rather than being dropped.
  //
  // AND GEOM.CLIP FEEDS SETUP EXACTLY. `tri_*` is `out_*`, field for field,
  // including the signed 2A and the scissored scan box. This is the ratified
  // pipeline order, not a convenient pairing.
  //
  // GLUE 1 EXTENDS HERE, and it is the same fact rather than a second one. The
  // compositor's pass geometry is derived from `mode_act_o` above; the clip
  // scissor is the same rectangle -- `zhao_geom_clip`'s header defines it as
  // "a canvas in Z60/Storm, one 256x192 view block in Duo -- video_rules.md
  // 3.1", which is what `post_frame_w_c`/`post_frame_h_c` already hold. Using
  // it twice is one source of truth used twice; recomputing it here would be a
  // second opinion about the console's own mode.
  //
  // WHAT IS STILL MISSING is in front of CLIP, not behind SETUP: entry I24.
  // ==========================================================================
  logic [11:0] clip_vp_w_c, clip_vp_h_c;
  always_comb begin
    // Widened, not converted. POST_XW/POST_YW are narrower than the clip's
    // 12-bit pixel fields, so the value is placed in the low bits and the rest
    // are zero -- there is no arithmetic here and there must not be.
    clip_vp_w_c = '0;
    clip_vp_h_c = '0;
    clip_vp_w_c[POST_XW-1:0] = post_frame_w_c;
    clip_vp_h_c[POST_YW-1:0] = post_frame_h_c;
  end

  // Quartus 17.0 requires an elaboration check to live inside `initial begin`
  // (QUARTUS_GOTCHAS: a bare module-scope `if` is a syntax error there). A
  // silent truncation of the scissor would put the clip window somewhere the
  // video mode never asked for, which is a picture bug with no counter.
  // synthesis translate_off
  initial begin
    if ((POST_XW > 12) || (POST_YW > 12))
      $fatal(1, "zhao_console_core: POST_XW/POST_YW exceed the clip viewport's 12-bit fields");
  end
  // synthesis translate_on

  wire               cl_o_valid, cl_o_ready;
  wire signed [20:0] cl_o_ax, cl_o_ay, cl_o_bx, cl_o_by, cl_o_cx, cl_o_cy;
  wire signed [47:0] cl_o_area2;
  wire signed [11:0] cl_o_min_x, cl_o_max_x, cl_o_min_y, cl_o_max_y;
  wire        [15:0] cl_o_src_id;

  zhao_geom_clip #(
    .ATTRS (GEOM_CLIP_ATTRS)
  ) u_geom_clip (
    .clk          (gpu_clk),
    .rst_n        (rst_n),

    // I24: the absent replay customer specified at I11 drives these.
    .tri_valid_i  (geom_clip_tri_valid_i),
    .tri_ready_o  (geom_clip_tri_ready_o),
    .tri_ax_i     (geom_clip_tri_ax_i),
    .tri_ay_i     (geom_clip_tri_ay_i),
    .tri_bx_i     (geom_clip_tri_bx_i),
    .tri_by_i     (geom_clip_tri_by_i),
    .tri_cx_i     (geom_clip_tri_cx_i),
    .tri_cy_i     (geom_clip_tri_cy_i),
    .tri_behind_i (geom_clip_tri_behind_i),
    .tri_src_id_i (geom_clip_tri_src_id_i),
    .tri_attr_a_i (geom_clip_attr_a_i),
    .tri_attr_b_i (geom_clip_attr_b_i),
    .tri_attr_c_i (geom_clip_attr_c_i),

    // REAL: the scissor is the console's own pass geometry (GLUE 1).
    .vp_x0_i      (12'd0),
    .vp_y0_i      (12'd0),
    .vp_w_i       (clip_vp_w_c),
    .vp_h_i       (clip_vp_h_c),
    // I24: draw state, no owner composed.
    .cull_mode_i  (geom_clip_cull_mode_i),

    // REAL: into GEOM.SETUP.
    .out_valid_o  (cl_o_valid),
    .out_ready_i  (cl_o_ready),
    .out_ax_o     (cl_o_ax),
    .out_ay_o     (cl_o_ay),
    .out_bx_o     (cl_o_bx),
    .out_by_o     (cl_o_by),
    .out_cx_o     (cl_o_cx),
    .out_cy_o     (cl_o_cy),
    .out_area2_o  (cl_o_area2),
    .out_min_x_o  (cl_o_min_x),
    .out_max_x_o  (cl_o_max_x),
    .out_min_y_o  (cl_o_min_y),
    .out_max_y_o  (cl_o_max_y),
    .out_src_id_o (cl_o_src_id),

    // The winding-flipped attributes and the verdict leave the module: their
    // customer (GEOM.ATTRSETUP) is not composed, and dropping the swap here
    // would lose the only thing this block does to them.
    .out_attr_a_o (geom_clip_attr_a_o),
    .out_attr_b_o (geom_clip_attr_b_o),
    .out_attr_c_o (geom_clip_attr_c_o),
    .out_flip_o   (geom_clip_flip_o),
    .ret_valid_o  (geom_clip_ret_valid_o),
    .ret_verdict_o(geom_clip_ret_verdict_o),

    .triangles_submitted_o (geom_clip_submitted_o),
    .triangles_clipped_o   (geom_clip_clipped_o),
    .triangles_culled_o    (geom_clip_culled_o)
  );

  wire               st_o_valid, st_o_ready;
  wire signed [22:0] st_kx0, st_ky0, st_kx1, st_ky1, st_kx2, st_ky2;
  wire signed [47:0] st_kc0, st_kc1, st_kc2;
  wire        [ 2:0] st_tl;
  wire signed [20:0] st_ax, st_ay, st_bx, st_by, st_cx, st_cy;
  wire signed [11:0] st_min_x, st_max_x, st_min_y, st_max_y;
  wire        [15:0] st_src_id;

  zhao_geom_setup u_geom_setup (
    .clk         (gpu_clk),
    .rst_n       (rst_n),

    // REAL: GEOM.CLIP's accepted packet, field for field.
    .tri_valid_i (cl_o_valid),
    .tri_ready_o (cl_o_ready),
    .tri_ax_i    (cl_o_ax),
    .tri_ay_i    (cl_o_ay),
    .tri_bx_i    (cl_o_bx),
    .tri_by_i    (cl_o_by),
    .tri_cx_i    (cl_o_cx),
    .tri_cy_i    (cl_o_cy),
    .tri_area2_i (cl_o_area2),
    .tri_min_x_i (cl_o_min_x),
    .tri_max_x_i (cl_o_max_x),
    .tri_min_y_i (cl_o_min_y),
    .tri_max_y_i (cl_o_max_y),
    .tri_src_id_i(cl_o_src_id),

    // REAL: the shell's triangle door.
    .out_valid_o (st_o_valid),
    .out_ready_i (st_o_ready),
    .out_kx0_o   (st_kx0),
    .out_ky0_o   (st_ky0),
    .out_kc0_o   (st_kc0),
    .out_kx1_o   (st_kx1),
    .out_ky1_o   (st_ky1),
    .out_kc1_o   (st_kc1),
    .out_kx2_o   (st_kx2),
    .out_ky2_o   (st_ky2),
    .out_kc2_o   (st_kc2),
    .out_tl_o    (st_tl),
    .out_area2_o (geom_setup_area2_o),
    .out_ax_o    (st_ax),
    .out_ay_o    (st_ay),
    .out_bx_o    (st_bx),
    .out_by_o    (st_by),
    .out_cx_o    (st_cx),
    .out_cy_o    (st_cy),
    .out_min_x_o (st_min_x),
    .out_max_x_o (st_max_x),
    .out_min_y_o (st_min_y),
    .out_max_y_o (st_max_y),
    .out_src_id_o(st_src_id),

    .triangles_submitted_o (geom_setup_triangles_submitted_o)
  );

  // ==========================================================================
  // TERRAIN: TESS -> GROUP_SEQ -> the SAME SHARED PROJECTOR, as CLIENT B.
  //
  // `design/prod_manifest.yml` records the shared projector as waiting for its
  // producers. Client A got one when GEOM.GROUP_SEQ was composed above; this is
  // the other, and it is the whole justification for the block being SHARED at
  // all. Until now the projector had one live client, so its arbiter, its
  // contention counter and its material-refusal path were all measured against
  // a port that never asked for anything.
  //
  // `zhao_terrain_pipe.sv` composes exactly this trio ALREADY -- and it is NOT
  // used here, deliberately: it instantiates its OWN `zhao_proj_subsystem`, so
  // adopting it would put a SECOND projector in the core and undo the sharing
  // this composition exists to measure. The tess and the sequencer are
  // instantiated directly onto the projector that is already here, and the
  // wiring below is `zhao_terrain_pipe`'s, seam for seam.
  // ==========================================================================
  wire                      ts_b_valid, ts_b_ready, ts_b_view;
  wire signed [31:0]        ts_b_vx, ts_b_vy, ts_b_vz;
  wire [PROJ_T_ARENA_W-1:0] ts_b_arena, ts_fill_arena, ts_open_arena, ts_seal_arena;
  wire [PROJ_T_INDEX_W-1:0] ts_b_index;
  wire                      ts_fill_landed, ts_open, ts_seal;
  wire [GEOM_GEN_W-1:0]     ts_open_gen;

  wire                      ts_r_valid, ts_r_ready, ts_r_view;
  wire [PROJ_T_ARENA_W-1:0] ts_r_arena;
  wire [GEOM_GEN_W-1:0]     ts_r_gen;
  wire [PROJ_T_INDEX_W-1:0] ts_r_ia, ts_r_ib, ts_r_ic;
  wire [15:0]               ts_r_src_id;
  wire [7:0]                ts_r_mat_a, ts_r_mat_b, ts_r_weight;

  // sequencer -> tess job, and the tess's two streams back
  wire                      tt_job_valid, tt_job_ready, tt_job_reject;
  wire [1:0]                tt_job_mode;
  wire [5:0]                tt_job_ox, tt_job_oz;
  wire [1:0]                tt_job_level, tt_job_nz, tt_job_pz, tt_job_nx, tt_job_px;
  wire [16:0]               tt_job_morph;
  wire                      tt_job_surface, tt_job_dual;
  wire [15:0]               tt_job_src_id;
  wire                      tt_vtx_valid, tt_vtx_ready, tt_vtx_stride;
  wire signed [31:0]        tt_vtx_x, tt_vtx_y, tt_vtx_z;
  wire [PROJ_T_IDX_W-1:0]   tt_vtx_index;
  wire                      tt_ref_valid, tt_ref_ready;
  wire [PROJ_T_IDX_W-1:0]   tt_ref_ia, tt_ref_ib, tt_ref_ic;

  // The tess's ModeTri leg. ModeTri (job_mode 0) is NEVER presented by the
  // sequencer -- it presents mode 1 then mode 2 -- so this port is dead by
  // construction, not by a tie-off. TERRAIN.NORMALS is its customer and
  // `zhao_terrain_pipe`'s header prices the three ways to feed it; none is
  // taken here either. `tri_ready_i` is held high so nothing can ever stall
  // the block on a port it does not use.
  /* verilator lint_off UNUSEDSIGNAL */
  wire                      tt_tri_valid, tt_tri_surface;
  wire signed [31:0]        tt_tri_ax, tt_tri_ay, tt_tri_az;
  wire signed [31:0]        tt_tri_bx, tt_tri_by, tt_tri_bz;
  wire signed [31:0]        tt_tri_cx, tt_tri_cy, tt_tri_cz;
  wire [15:0]               tt_tri_src_id;
  wire [31:0]               tt_tri_emitted;
  // The per-stream surface and src_id riders: the SEQUENCER carries the job's
  // own `src_id` and materials to the reference port, so the tess's copies are
  // redundant rather than missing.
  wire                      tt_vtx_surface, tt_ref_surface;
  wire [15:0]               tt_vtx_src_id, tt_ref_src_id;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_terrain_tess #(
    .IDX_W (PROJ_T_IDX_W)
  ) u_terrain_tess (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: presented twice per job by the sequencer, mode 1 then mode 2.
    .job_valid_i  (tt_job_valid),
    .job_ready_o  (tt_job_ready),
    .job_mode_i   (tt_job_mode),
    .job_ox_i     (tt_job_ox),
    .job_oz_i     (tt_job_oz),
    .job_level_i  (tt_job_level),
    .job_lvl_nz_i (tt_job_nz),
    .job_lvl_pz_i (tt_job_pz),
    .job_lvl_nx_i (tt_job_nx),
    .job_lvl_px_i (tt_job_px),
    .job_morph_i  (tt_job_morph),
    .job_surface_i(tt_job_surface),
    .job_dual_i   (tt_job_dual),
    .job_src_id_i (tt_job_src_id),

    // I22: the lattice and cell-state stores have no owner composed here.
    .lat_req_o     (terr_lat_req_o),
    .lat_vi_o      (terr_lat_vi_o),
    .lat_vj_o      (terr_lat_vj_o),
    .lat_surface_o (terr_lat_surface_o),
    .lat_h_i       (terr_lat_h_i),
    .lat_wx_i      (terr_lat_wx_i),
    .lat_wz_i      (terr_lat_wz_i),
    .cs_req_o      (terr_cs_req_o),
    .cs_ci_o       (terr_cs_ci_o),
    .cs_cj_o       (terr_cs_cj_o),
    .cs_substance_i(terr_cs_substance_i),

    // ModeTri: never presented; see the declaration above.
    .tri_valid_o(tt_tri_valid),
    .tri_ready_i(1'b1),
    .ax_o       (tt_tri_ax),
    .ay_o       (tt_tri_ay),
    .az_o       (tt_tri_az),
    .bx_o       (tt_tri_bx),
    .by_o       (tt_tri_by),
    .bz_o       (tt_tri_bz),
    .cx_o       (tt_tri_cx),
    .cy_o       (tt_tri_cy),
    .cz_o       (tt_tri_cz),
    .surface_o  (tt_tri_surface),
    .src_id_o   (tt_tri_src_id),

    // REAL: the 81 window vertices, into the sequencer's fill phase.
    .vtx_valid_o  (tt_vtx_valid),
    .vtx_ready_i  (tt_vtx_ready),
    .vtx_x_o      (tt_vtx_x),
    .vtx_y_o      (tt_vtx_y),
    .vtx_z_o      (tt_vtx_z),
    .vtx_index_o  (tt_vtx_index),
    .vtx_stride_o (tt_vtx_stride),
    .vtx_surface_o(tt_vtx_surface),
    .vtx_src_id_o (tt_vtx_src_id),

    // REAL: one triangle per clock as three window indices, into the replay.
    .ref_valid_o  (tt_ref_valid),
    .ref_ready_i  (tt_ref_ready),
    .ref_ia_o     (tt_ref_ia),
    .ref_ib_o     (tt_ref_ib),
    .ref_ic_o     (tt_ref_ic),
    .ref_surface_o(tt_ref_surface),
    .ref_src_id_o (tt_ref_src_id),

    .terrain_triangles_emitted_o(tt_tri_emitted),
    .terrain_vertices_emitted_o (terr_tess_vertices_o),
    .terrain_refs_emitted_o     (terr_tess_refs_o),
    .mode_invalid_o             (terr_tess_mode_invalid_o),
    .subpatch_rejected_o        (terr_tess_rejected_o),
    .lod_clamped_o              (terr_tess_lod_clamped_o),
    .job_reject_o               (tt_job_reject),
    .idle_o                     (terr_tess_idle_o)
  );

  zhao_terrain_group_seq #(
    .ARENAS  (PROJ_T_ARENAS),
    .DEPTH   (PROJ_T_DEPTH),
    .GEN_W   (GEOM_GEN_W),
    .IDX_W   (PROJ_T_IDX_W),
    .INDEX_W (PROJ_T_INDEX_W),
    .ARENA_W (PROJ_T_ARENA_W)
  ) u_terrain_group_seq (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // I21: the subpatch job. TERRAIN.LOD is the near-producer and cannot be
    // wired -- see the header entry.
    .job_valid_i    (terr_job_valid_i),
    .job_ready_o    (terr_job_ready_o),
    .job_ox_i       (terr_job_ox_i),
    .job_oz_i       (terr_job_oz_i),
    .job_level_i    (terr_job_level_i),
    .job_lvl_nz_i   (terr_job_lvl_nz_i),
    .job_lvl_pz_i   (terr_job_lvl_pz_i),
    .job_lvl_nx_i   (terr_job_lvl_nx_i),
    .job_lvl_px_i   (terr_job_lvl_px_i),
    .job_morph_i    (terr_job_morph_i),
    .job_surface_i  (terr_job_surface_i),
    .job_dual_i     (terr_job_dual_i),
    .job_src_id_i   (terr_job_src_id_i),
    .job_view_mask_i(terr_job_view_mask_i),
    .job_mat_a_i    (terr_job_mat_a_i),
    .job_mat_b_i    (terr_job_mat_b_i),
    .job_weight_i   (terr_job_weight_i),

    // The sparse-fill knob is an OWNER KNOB, not a tie-off: it is legal only
    // against a VALID_MODE = 0 shell and this block cannot see the shell's
    // mode, so the composition that holds both is where they must agree.
    // `zhao_proj_subsystem` here carries the dense shell, so the safe value is
    // low and it is exposed rather than frozen.
    .sparse_fill_i  (terr_sparse_fill_i),

    // REAL: the tessellator, both modes.
    .t_job_valid_o  (tt_job_valid),
    .t_job_ready_i  (tt_job_ready),
    .t_job_mode_o   (tt_job_mode),
    .t_job_ox_o     (tt_job_ox),
    .t_job_oz_o     (tt_job_oz),
    .t_job_level_o  (tt_job_level),
    .t_job_lvl_nz_o (tt_job_nz),
    .t_job_lvl_pz_o (tt_job_pz),
    .t_job_lvl_nx_o (tt_job_nx),
    .t_job_lvl_px_o (tt_job_px),
    .t_job_morph_o  (tt_job_morph),
    .t_job_surface_o(tt_job_surface),
    .t_job_dual_o   (tt_job_dual),
    .t_job_src_id_o (tt_job_src_id),
    .t_job_reject_i (tt_job_reject),
    .t_vtx_valid_i  (tt_vtx_valid),
    .t_vtx_ready_o  (tt_vtx_ready),
    .t_vtx_x_i      (tt_vtx_x),
    .t_vtx_y_i      (tt_vtx_y),
    .t_vtx_z_i      (tt_vtx_z),
    .t_vtx_index_i  (tt_vtx_index),
    .t_vtx_stride_i (tt_vtx_stride),
    .t_ref_valid_i  (tt_ref_valid),
    .t_ref_ready_o  (tt_ref_ready),
    .t_ref_ia_i     (tt_ref_ia),
    .t_ref_ib_i     (tt_ref_ib),
    .t_ref_ic_i     (tt_ref_ic),

    // REAL: CLIENT B of the shared projection service.
    .b_valid_o    (ts_b_valid),
    .b_ready_i    (ts_b_ready),
    .b_vx_o       (ts_b_vx),
    .b_vy_o       (ts_b_vy),
    .b_vz_o       (ts_b_vz),
    .b_view_o     (ts_b_view),
    .b_arena_o    (ts_b_arena),
    .b_index_o    (ts_b_index),
    .fill_landed_i(ts_fill_landed),
    .fill_arena_i (ts_fill_arena),

    // REAL: the terrain arena's lifetime.
    .open_o      (ts_open),
    .open_arena_o(ts_open_arena),
    .open_gen_i  (ts_open_gen),
    .seal_o      (ts_seal),
    .seal_arena_o(ts_seal_arena),

    // REAL: tagged references into the projector's replay shell.
    .r_valid_o (ts_r_valid),
    .r_ready_i (ts_r_ready),
    .r_arena_o (ts_r_arena),
    .r_gen_o   (ts_r_gen),
    .r_ia_o    (ts_r_ia),
    .r_ib_o    (ts_r_ib),
    .r_ic_o    (ts_r_ic),
    .r_src_id_o(ts_r_src_id),
    .r_view_o  (ts_r_view),
    .r_mat_a_o (ts_r_mat_a),
    .r_mat_b_o (ts_r_mat_b),
    .r_weight_o(ts_r_weight),

    .held_o           (terr_held_o),
    .busy_o           (terr_busy_o),
    .jobs_accepted_o  (terr_jobs_accepted_o),
    .jobs_no_view_o   (terr_jobs_no_view_o),
    .jobs_rejected_o  (terr_jobs_rejected_o),
    .jobs_empty_o     (terr_jobs_empty_o),
    .groups_opened_o  (terr_groups_opened_o),
    .groups_released_o(terr_groups_released_o),
    .fills_forwarded_o(terr_fills_forwarded_o),
    .fills_dropped_o  (terr_fills_dropped_o),
    .refs_forwarded_o (terr_refs_forwarded_o),
    .release_unsafe_o (terr_release_unsafe_o)
  );

  // ==========================================================================
  // THE MATRIX BANK HAS TWO WRITERS.  I14, and this is the closing half of it.
  // ==========================================================================
  // CMD.EXEC (section 7c) owns cfg addresses 0..15 -- SetView's `mat4fx`, the
  // camera. The host port owns 16 and 17, the viewport origin and extent, and
  // it keeps them because NO RATIFIED COMMAND CARRIES A VIEWPORT RECT: SetView
  // has a `viewport_id`, and the id-to-rectangle table is `spec/video_rules.md`'s
  // and is not in the ABI. Inventing that table here is precisely what this
  // file may not do, so the port stays and the entry stays open for it.
  //
  // LOSSLESS, IN BOTH DIRECTIONS, AND THAT IS WHY THERE IS NO COUNTER HERE.
  // The host takes any cycle it asks for; CMD.EXEC's `proj_cfg_ready_i` goes
  // low for that cycle and it re-presents the identical word. A priority mux
  // that dropped one write and counted it would have needed a detector nothing
  // in this tree can fire -- and the thing being dropped would have been one
  // matrix coefficient, which is a camera that is subtly wrong with every
  // counter still reading right.
  //
  // ENFORCED-BY: tests/command/cmd_exec_directed.cpp case 8, which refuses the
  // executor on 31 cycles in 32 and requires all 32 words to land once, in
  // address order.
  logic        cmd_exec_cfg_we_w, cmd_exec_cfg_view_w, cmd_exec_cfg_ready_w;
  logic [ 4:0] cmd_exec_cfg_addr_w;
  logic [31:0] cmd_exec_cfg_data_w;

  logic        proj_cfg_we_m, proj_cfg_view_m;
  logic [ 4:0] proj_cfg_addr_m;
  logic [31:0] proj_cfg_data_m;

  assign cmd_exec_cfg_ready_w = !proj_cfg_we_i;
  assign proj_cfg_we_m        = proj_cfg_we_i || cmd_exec_cfg_we_w;
  assign proj_cfg_view_m      = proj_cfg_we_i ? proj_cfg_view_i : cmd_exec_cfg_view_w;
  assign proj_cfg_addr_m      = proj_cfg_we_i ? proj_cfg_addr_i : cmd_exec_cfg_addr_w;
  assign proj_cfg_data_m      = proj_cfg_we_i ? proj_cfg_data_i : cmd_exec_cfg_data_w;

  zhao_proj_subsystem #(
    .PAYLOAD_A_W (GEOM_PAY_A_W),
    .ARENAS      (PROJ_T_ARENAS),
    .DEPTH       (PROJ_T_DEPTH),
    .GEN_W       (GEOM_GEN_W)
  ) u_proj_subsystem (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // I14, HALF CLOSED: the camera words come from CMD.EXEC, the viewport rect
    // still comes from the host port. See the merge directly above.
    .cfg_we_i  (proj_cfg_we_m),
    .cfg_view_i(proj_cfg_view_m),
    .cfg_addr_i(proj_cfg_addr_m),
    .cfg_data_i(proj_cfg_data_m),
    .en_i      (proj_en_i),

    // REAL: client A in, from GEOM.GROUP_SEQ -- THROUGH PART.PROJECT, which
    // time-multiplexes this one port between geometry and the particle draw
    // pass (entry I24). Geometry's beats are untouched; the block forwards them
    // on the cycles it is not using the port itself and never inserts a stage.
    // The subsystem, the service and the core are unmodified: the guest sits in
    // front of the port, not inside it.
    .a_valid_i  (pa_a_valid),
    .a_ready_o  (pa_a_ready),
    .a_vx_i     (pa_a_vx),
    .a_vy_i     (pa_a_vy),
    .a_vz_i     (pa_a_vz),
    .a_view_i   (pa_a_view),
    .a_payload_i(pa_a_payload),

    // REAL: client A's results out, back into PART.PROJECT, which demultiplexes
    // them by the rider's top bit and hands geometry's on to the arena lane
    // unchanged as `pj_a_*`.
    .a_valid_o  (sv_a_valid),
    .a_x_o      (sv_a_x),
    .a_y_o      (sv_a_y),
    .a_d_o      (sv_a_d),
    .a_w_o      (sv_a_w),
    .a_behind_o (sv_a_behind),
    // The view tag is the SERVICE's, and it now reports whichever client won
    // the cycle. Both clients project through the same per-view matrix bank --
    // that is the property `zhao_project_service`'s header check 2 establishes
    // -- so this remains "the view this result was projected in"; it is simply
    // no longer geometry's alone.
    .a_view_o   (proj_a_view_o),
    .a_payload_o(sv_a_payload),

    // REAL: client B in, from TERRAIN.GROUP_SEQ. This is the composition the
    // shared projector was built for -- BOTH of its clients are live here, and
    // `proj_contended_o` can therefore move for the first time.
    .b_valid_i(ts_b_valid),
    .b_ready_o(ts_b_ready),
    .b_vx_i   (ts_b_vx),
    .b_vy_i   (ts_b_vy),
    .b_vz_i   (ts_b_vz),
    .b_view_i (ts_b_view),
    .b_arena_i(ts_b_arena),
    .b_index_i(ts_b_index),

    // REAL: the terrain arena's lifetime and its landings, both directions.
    .fill_landed_o(ts_fill_landed),
    .fill_arena_o (ts_fill_arena),
    .open_i       (ts_open),
    .open_arena_i (ts_open_arena),
    .open_gen_o   (ts_open_gen),
    .seal_i       (ts_seal),
    .seal_arena_i (ts_seal_arena),

    // REAL: the tagged references, from the same sequencer.
    .ref_valid_i  (ts_r_valid),
    .ref_ready_o  (ts_r_ready),
    .ref_arena_i  (ts_r_arena),
    .ref_gen_i    (ts_r_gen),
    .ref_ia_i     (ts_r_ia),
    .ref_ib_i     (ts_r_ib),
    .ref_ic_i     (ts_r_ic),
    .ref_src_id_i (ts_r_src_id),
    .ref_view_i   (ts_r_view),
    .ref_mat_a_i  (ts_r_mat_a),
    .ref_mat_b_i  (ts_r_mat_b),
    .ref_weight_i (ts_r_weight),

    // I13: the projected triangle. GEOM.SETUP is the customer and is not
    // composed; the shell's own triangle door takes EDGE FUNCTIONS, which is
    // setup's arithmetic and not this file's to invent.
    .out_valid_o  (proj_out_valid_o),
    .out_ready_i  (proj_out_ready_i),
    .out_ax_o     (proj_out_ax_o),
    .out_ay_o     (proj_out_ay_o),
    .out_bx_o     (proj_out_bx_o),
    .out_by_o     (proj_out_by_o),
    .out_cx_o     (proj_out_cx_o),
    .out_cy_o     (proj_out_cy_o),
    .out_behind_o (proj_out_behind_o),
    .out_src_id_o (proj_out_src_id_o),
    .out_ad_o     (proj_out_ad_o),
    .out_bd_o     (proj_out_bd_o),
    .out_cd_o     (proj_out_cd_o),
    .out_aw_o     (proj_out_aw_o),
    .out_bw_o     (proj_out_bw_o),
    .out_cw_o     (proj_out_cw_o),
    .out_view_o   (proj_out_view_o),
    .out_mat_a_o  (proj_out_mat_a_o),
    .out_mat_b_o  (proj_out_mat_b_o),
    .out_weight_o (proj_out_weight_o),
    .out_refused_o(proj_out_refused_o),
    .out_missed_o (proj_out_missed_o),

    .replay_triangles_o(proj_replay_triangles_o),
    .replay_refused_o  (proj_replay_refused_o),
    .replay_missed_o   (proj_replay_missed_o),
    .corner_hits_o     (proj_corner_hits_o),
    .corner_refusals_o (proj_corner_refusals_o),
    .corner_misses_o   (proj_corner_misses_o),
    .arena_overflow_o  (proj_arena_overflow_o),
    .arena_seal_short_o(proj_arena_seal_short_o),
    .shell_idle_o      (proj_shell_idle_o),
    .svc_busy_o        (proj_svc_busy_o),
    .a_grants_o        (proj_a_grants_o),
    .b_grants_o        (proj_b_grants_o),
    .contended_o       (proj_contended_o),
    .mat_refused_o     (proj_mat_refused_o)
  );

  zhao_geom_proj_lane #(
    .ARENAS      (GEOM_ARENAS),
    .DEPTH       (GEOM_DEPTH),
    .GEN_W       (GEOM_GEN_W),
    .PAYLOAD_A_W (GEOM_PAY_A_W),
    .PAYLOAD_W   (GEOM_PAYLOAD_W)
  ) u_geom_proj_lane (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: client A's result port, exactly as the service presents it.
    .a_valid_i  (pj_a_valid),
    .a_x_i      (pj_a_x),
    .a_y_i      (pj_a_y),
    .a_d_i      (pj_a_d),
    .a_w_i      (pj_a_w),
    .a_behind_i (pj_a_behind),
    .a_payload_i(pj_a_payload),

    // REAL: the rider layout lives in the lane; the sequencer hands it the
    // {arena, index} and takes the packed word back.
    .rider_arena_i  (gs_rider_arena),
    .rider_index_i  (gs_rider_index),
    .rider_payload_o(ln_rider_payload),

    // REAL: the geometry arena's lifetime, driven by the sequencer.
    .open_i      (gs_open),
    .open_arena_i(gs_open_arena),
    .open_gen_o  (ln_open_gen),
    .seal_i      (gs_seal),
    .seal_arena_i(gs_seal_arena),

    // I12: the arena origin and the lookup port; same absent customer.
    .org_we_i   (geom_org_we_i),
    .org_arena_i(geom_org_arena_i),
    .org_x_i    (geom_org_x_i),
    .org_y_i    (geom_org_y_i),
    .org_z_i    (geom_org_z_i),

    // REAL: landings back to the sequencer.
    .fill_landed_o(ln_fill_landed),
    .fill_arena_o (ln_fill_arena),

    .look_valid_i(geom_look_valid_i),
    .look_ready_o(geom_look_ready_o),
    .look_arena_i(geom_look_arena_i),
    .look_gen_i  (geom_look_gen_i),
    .look_index_i(geom_look_index_i),
    .rep_valid_o (geom_rep_valid_o),
    .rep_hit_o   (geom_rep_hit_o),
    .rep_refuse_o(geom_rep_refuse_o),
    .rep_payload_o(geom_rep_payload_o),
    .rep_org_x_o (geom_rep_org_x_o),
    .rep_org_y_o (geom_rep_org_y_o),
    .rep_org_z_o (geom_rep_org_z_o),

    .arena_hits_o    (geom_arena_hits_o),
    .arena_misses_o  (geom_arena_misses_o),
    .arena_refusals_o(geom_arena_refusals_o),
    .arena_overflow_o(geom_arena_overflow_o)
  );

  // ==========================================================================
  // COMPOSITOR. On the real frame edge and the real video mode (glue 1 and 2),
  // and BESIDE the render path rather than in it -- header entry I15 says why.
  //
  // The ATMOSPHERE SHEET is internal as of 2026-09-19. These eight wires are
  // the seam that used to be eight ports; their producer is `u_twod_sampler`
  // at the end of this module, which is fed by `u_twod_plane`, which is walked
  // by the sampler. The three blocks are declared together at the bottom
  // rather than here so that this instance keeps the shape a reader already
  // knows -- and because the walk is a LOOP (sampler -> plane -> sampler) and
  // splitting a loop across two places in a file is how one half gets edited.
  // ==========================================================================
  logic                   atm_req_v_c;
  logic [POST_XW-1:0]     atm_req_x_c;
  logic [POST_YW-1:0]     atm_req_y_c;
  logic                   atm_en_c;
  logic                   atm_valid_c;
  logic [15:0]            atm_rgb_c;
  logic [7:0]             atm_opacity_c;
  logic                   atm_add_c;

  zhao_post_composite #(
    .LINE_W    (POST_LINE_W),
    .MAX_H     (POST_MAX_H),
    .NLINE     (POST_NLINE),
    .LAG_LINES (POST_LAG_LINES),
    .LAG_PX    (POST_LAG_PX)
  ) u_post_composite (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: the shell's frame boundary and its latched mode.
    .frame_start_i(core_tick_c),
    .frame_w_i    (post_frame_w_c),
    .frame_h_i    (post_frame_h_c),
    .view_sel_i   (post_view_sel_i),

    // I15: not a missing port -- a missing ORDER. This port is addressless and
    // the block counts its own raster; RASTER.RESOLVE emits 16x16 tiles, and
    // only the tiles a triangle touched. The reorder buffer between them is a
    // whole frame, so post reads the COMPLETED framebuffer or it reads nothing.
    .s_valid_i(post_s_valid_i),
    .s_ready_o(post_s_ready_o),
    .s_rgb_i  (post_s_rgb_i),

    // I17, CLOSED FOR THE ATMOSPHERE SHEET 2026-09-19: `atm_*` is now wired to
    // TWOD.SAMPLER, which is wired to TWOD.PLANE, at the end of this module.
    // The `gd_*`/`gg_*` planes and `hud_*` are still boundary and I17 says why.
    .gd_req_v_o  (post_gd_req_v_o),
    .gd_view_o   (post_gd_view_o),
    .gd_cx_o     (post_gd_cx_o),
    .gd_cy_o     (post_gd_cy_o),
    .gd_present_i(post_gd_present_i),
    .gd_dx_i     (post_gd_dx_i),
    .gd_dy_i     (post_gd_dy_i),
    .gg_req_v_o  (post_gg_req_v_o),
    .gg_view_o   (post_gg_view_o),
    .gg_cx_o     (post_gg_cx_o),
    .gg_cy_o     (post_gg_cy_o),
    .gg_present_i(post_gg_present_i),
    .gg_glow_i   (post_gg_glow_i),
    .gg_ink_i    (post_gg_ink_i),
    .atm_req_v_o (atm_req_v_c),
    .atm_req_x_o (atm_req_x_c),
    .atm_req_y_o (atm_req_y_c),
    .atm_en_i    (atm_en_c),
    .atm_valid_i (atm_valid_c),
    .atm_rgb_i   (atm_rgb_c),
    .atm_opacity_i(atm_opacity_c),
    .atm_add_i   (atm_add_c),
    .bloom_gain_i(post_bloom_gain_i),
    .grade_valid_i(post_grade_valid_i),
    .pv_we_i     (post_pv_we_i),
    .pv_sel_i    (post_pv_sel_i),
    .pv_addr_i   (post_pv_addr_i),
    .pv_data_i   (post_pv_data_i),
    .bias_r_i    (post_bias_r_i),
    .bias_g_i    (post_bias_g_i),
    .bias_b_i    (post_bias_b_i),
    .flash_rgb_i (post_flash_rgb_i),
    .flash_amt_i (post_flash_amt_i),
    .ink_rgb_i   (post_ink_rgb_i),
    .hud_req_v_o (post_hud_req_v_o),
    .hud_req_x_o (post_hud_req_x_o),
    .hud_req_y_o (post_hud_req_y_o),
    .hud_valid_i (post_hud_valid_i),
    .hud_rgb_i   (post_hud_rgb_i),

    // I16: the framebuffer writer inside the shell is already fed by
    // RASTER.FBWRITE, so this output has no consumer here.
    .o_valid_o(post_o_valid_o),
    .o_ready_i(post_o_ready_i),
    .o_rgb_o  (post_o_rgb_o),
    .o_x_o    (post_o_x_o),
    .o_y_o    (post_o_y_o),
    .o_last_o (post_o_last_o),
    .echo_valid_o(post_echo_valid_o),
    .echo_rgb_o  (post_echo_rgb_o),

    .displacement_edge_clamps_o(post_displacement_edge_clamps_o),
    .bloom_cells_contributing_o(post_bloom_cells_contributing_o),
    .passes_completed_o        (post_passes_completed_o),
    .grading_table_missing_o   (post_grading_table_missing_o),
    .plane_missing_o           (post_plane_missing_o),
    .line_fill_writes_o        (post_line_fill_writes_o),
    .output_writes_o           (post_output_writes_o),
    .plane_reads_o             (post_plane_reads_o),
    .ring_hazard_o             (post_ring_hazard_o)
  );

  // ==========================================================================
  // MEASURE. The INTERVAL is the console's real frame (glue 2). The EVENTS are
  // not: nothing here produces an error magnitude -- header entry I18.
  // ==========================================================================
  zhao_measure_histogram #(
    .EW       (HIST_EW),
    .SUB_BITS (HIST_SUB_BITS),
    .LANES    (HIST_LANES),
    .CW       (HIST_CW)
  ) u_measure_histogram (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // I18: no error-event producer exists in this console.
    .ev_valid_i     (hist_ev_valid_i),
    .ev_lane_valid_i(hist_ev_lane_valid_i),
    .ev_err_i       (hist_ev_err_i),
    .ev_src_id_i    (hist_ev_src_id_i),
    .ev_ready_o     (hist_ev_ready_o),

    // REAL: one measurement interval per console frame.
    .snapshot_i(core_tick_c),

    // I19: no HPS register path to this block.
    .rd_valid_i     (hist_rd_valid_i),
    .rd_bin_i       (hist_rd_bin_i),
    .rd_ready_o     (hist_rd_ready_o),
    .rd_data_valid_o(hist_rd_data_valid_o),
    .rd_count_o     (hist_rd_count_o),

    .snap_valid_o (hist_snap_valid_o),
    .snap_total_o (hist_snap_total_o),
    .snap_src_id_o(hist_snap_src_id_o),
    .snap_index_o (hist_snap_index_o),

    .events_o       (hist_events_o),
    .updates_o      (hist_updates_o),
    .stall_cycles_o (hist_stall_cycles_o),
    .bin_sat_o      (hist_bin_sat_o),
    .fwd_hits_o     (hist_fwd_hits_o),
    .host_conflict_o(hist_host_conflict_o),
    .snapshots_o    (hist_snapshots_o),
    .frozen_write_o (hist_frozen_write_o)
  );

  // ==========================================================================
  // SURFACE: STAMP <-> SHEET, a closed read-modify-write pair.
  //
  // `spec/terrain_rules.md` 7 -- "F written only by SURFACE.STAMP" -- is not a
  // note about this composition, it is the reason the composition is a PAIR and
  // not two blocks that happen to be adjacent. SURFACE.SHEET has exactly one
  // request port, one page port and one write port; SURFACE.STAMP is the only
  // block in the tree that drives all three, and it drives them port for port
  // and width for width. Nothing is renamed and nothing is computed between
  // them:
  //
  //   STAMP.req_*  -> SHEET.req_*   (ACQUIRE, then one READ per covered texel)
  //   SHEET.pg_*   -> STAMP.pg_*    (status and the pre-blend strength)
  //   STAMP.wr_*   -> SHEET.wr_*    (the blended texel, with byte enables)
  //
  // So the loop is real and it is INTERNAL: a stamp acquires a sheet, reads
  // layer F texel by texel, blends, and writes it back into the same on-chip
  // store. `surf_sheet_texels_touched_o` and `surf_stamp_texels_touched_o` are
  // the two ends of that loop counted independently, which is the point --
  // they are incremented by different blocks from different events, so they
  // agreeing is evidence rather than a tautology.
  //
  // WHAT THIS IS NOT, said plainly rather than left for somebody to discover.
  // This pair is an ISLAND inside the core. Nothing in the console CAUSES a
  // stamp: the dispatch arrives at this module's edge (I30) because
  // CMD.SCHEDULER has no SurfaceStamp path, so until it does, the pair only
  // moves when a harness moves it. That is the completion plan's permitted
  // "host packets" and it is a boundary, not a connection, and it is counted
  // as one below.
  //
  // THE STORE IS ON CHIP AND REAL. SURFACE.SHEET is `SURF_SLOTS` x 65,536 bits
  // of M10K, not a port standing in for hardware storage, so this is not the
  // refusal recorded at I23: there is nothing behind it that needs a beat from
  // an SDRAM model that does not exist.
  //
  // TEXTURE.AUX IS THE SECOND READER THE LEDGER NAMES, and it does not fit:
  // `design/blocks.yml` gives SURFACE.SHEET `downstream: [SURFACE.STAMP,
  // TEXTURE.AUX, MEM.GUARD]` while the block has ONE request port. Sharing it
  // is an arbiter, and an arbiter invented here is the thing this file refuses
  // to contain. TEXTURE.AUX is uncomposed for its own reasons, so nothing is
  // lost today; it is written down so the next packet does not read the single
  // port as an oversight.
  // ==========================================================================
  wire        surf_req_valid, surf_req_ready;
  wire [ 1:0] surf_req_op;
  wire [31:0] surf_req_handle;
  wire [11:0] surf_req_texel;
  wire [15:0] surf_req_src_id;

  wire        surf_pg_valid, surf_pg_ready;
  wire [ 1:0] surf_pg_status;
  wire [ 7:0] surf_pg_strength;

  wire        surf_wr_valid, surf_wr_ready;
  wire [31:0] surf_wr_handle;
  wire [11:0] surf_wr_texel;
  wire [ 7:0] surf_wr_tag, surf_wr_strength;
  wire        surf_wr_we_tag, surf_wr_we_strength;
  wire [15:0] surf_wr_src_id;

  // ==========================================================================
  // THE STAMP DISPATCH HAS TWO PRODUCERS.  I30, and this is its closing half.
  // ==========================================================================
  // CMD.EXEC (section 7c) supplies the RATIFIED fields -- `spec/commands.zidl`
  // SurfaceStamp 0x0210's patch handle, operation, tag, strength, the
  // transform's translation, radius, ring width, and the record header's
  // source id. That is the whole of what the game can say.
  //
  // THE OTHER SIX FIELDS STAY ON THE HOST PORT AND THE DISTINCTION IS THE
  // POINT. `cmd_env_*` is the patch ENVELOPE, which entry I27 records as having
  // no placement owner anywhere in the tree; `cmd_blend_en_i`, `cmd_blend_i`,
  // `cmd_age_shift_i` and `cmd_field_en_i` are console POLICY that no opcode
  // carries. An executor that filled them in would be choosing values the ABI
  // does not contain -- the hidden-contract failure this file exists to refuse.
  // So an executor-issued stamp rides the host's envelope and policy, and that
  // is stated here rather than discovered from a stamp landing in the wrong
  // place.
  //
  // PRIORITY, WITH BACKPRESSURE, SO NOTHING IS DROPPED. The executor wins;
  // `surf_cmd_ready_o` simply goes low for the host while it does, which is the
  // host's own handshake doing its job. No arbiter state, no counter, nothing
  // this file invented.
  logic        cmd_exec_stamp_valid_w, cmd_exec_stamp_ready_w;
  logic [31:0] cmd_exec_stamp_patch_w;
  logic [ 7:0] cmd_exec_stamp_oper_w, cmd_exec_stamp_tag_w;
  logic [15:0] cmd_exec_stamp_strength_w, cmd_exec_stamp_src_id_w;
  logic signed [31:0] cmd_exec_stamp_tx_w, cmd_exec_stamp_ty_w;
  logic signed [31:0] cmd_exec_stamp_radius_w, cmd_exec_stamp_ring_w;

  logic        surf_cmd_valid_m;
  logic [31:0] surf_cmd_handle_m;
  logic [ 7:0] surf_cmd_operation_m, surf_cmd_tag_m;
  logic [15:0] surf_cmd_strength_m, surf_cmd_src_id_m;
  logic signed [31:0] surf_cmd_tx_m, surf_cmd_ty_m;
  logic signed [31:0] surf_cmd_radius_m, surf_cmd_ring_width_m;
  logic        surf_cmd_ready_int;

  assign surf_cmd_valid_m       = cmd_exec_stamp_valid_w || surf_cmd_valid_i;
  assign cmd_exec_stamp_ready_w = surf_cmd_ready_int;
  assign surf_cmd_ready_o       = surf_cmd_ready_int && !cmd_exec_stamp_valid_w;

  assign surf_cmd_handle_m     = cmd_exec_stamp_valid_w ? cmd_exec_stamp_patch_w
                                                        : surf_cmd_handle_i;
  assign surf_cmd_operation_m  = cmd_exec_stamp_valid_w ? cmd_exec_stamp_oper_w
                                                        : surf_cmd_operation_i;
  assign surf_cmd_tag_m        = cmd_exec_stamp_valid_w ? cmd_exec_stamp_tag_w
                                                        : surf_cmd_tag_i;
  assign surf_cmd_strength_m   = cmd_exec_stamp_valid_w ? cmd_exec_stamp_strength_w
                                                        : surf_cmd_strength_i;
  assign surf_cmd_tx_m         = cmd_exec_stamp_valid_w ? cmd_exec_stamp_tx_w
                                                        : surf_cmd_tx_i;
  assign surf_cmd_ty_m         = cmd_exec_stamp_valid_w ? cmd_exec_stamp_ty_w
                                                        : surf_cmd_ty_i;
  assign surf_cmd_radius_m     = cmd_exec_stamp_valid_w ? cmd_exec_stamp_radius_w
                                                        : surf_cmd_radius_i;
  assign surf_cmd_ring_width_m = cmd_exec_stamp_valid_w ? cmd_exec_stamp_ring_w
                                                        : surf_cmd_ring_width_i;
  assign surf_cmd_src_id_m     = cmd_exec_stamp_valid_w ? cmd_exec_stamp_src_id_w
                                                        : surf_cmd_src_id_i;

  zhao_surface_stamp #(
    .SQ_RADIX (SURF_SQ_RADIX)
  ) u_surface_stamp (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    // I30, HALF CLOSED: the ratified fields come from CMD.EXEC, the envelope
    // and the blend policy still come from the host port. See above.
    .cmd_valid_i     (surf_cmd_valid_m),
    .cmd_ready_o     (surf_cmd_ready_int),
    .cmd_handle_i    (surf_cmd_handle_m),
    .cmd_operation_i (surf_cmd_operation_m),
    .cmd_tag_i       (surf_cmd_tag_m),
    .cmd_strength_i  (surf_cmd_strength_m),
    .cmd_tx_i        (surf_cmd_tx_m),
    .cmd_ty_i        (surf_cmd_ty_m),
    .cmd_radius_i    (surf_cmd_radius_m),
    .cmd_ring_width_i(surf_cmd_ring_width_m),
    .cmd_env_x0_i    (surf_cmd_env_x0_i),
    .cmd_env_z0_i    (surf_cmd_env_z0_i),
    .cmd_env_x1_i    (surf_cmd_env_x1_i),
    .cmd_env_z1_i    (surf_cmd_env_z1_i),
    .cmd_blend_en_i  (surf_cmd_blend_en_i),
    .cmd_blend_i     (surf_cmd_blend_i),
    .cmd_age_shift_i (surf_cmd_age_shift_i),
    .cmd_field_en_i  (surf_cmd_field_en_i),
    .cmd_src_id_i    (surf_cmd_src_id_m),

    // I31: FIELD.SEQ.STAMP is not built.
    .fld_valid_i   (surf_fld_valid_i),
    .fld_ready_o   (surf_fld_ready_o),
    .fld_tag_op_i  (surf_fld_tag_op_i),
    .fld_strength_i(surf_fld_strength_i),

    // REAL: SURFACE.SHEET's request port, name for name.
    .req_valid_o (surf_req_valid),
    .req_ready_i (surf_req_ready),
    .req_op_o    (surf_req_op),
    .req_handle_o(surf_req_handle),
    .req_texel_o (surf_req_texel),
    .req_src_id_o(surf_req_src_id),

    // REAL: SURFACE.SHEET's page responses.
    .pg_valid_i   (surf_pg_valid),
    .pg_ready_o   (surf_pg_ready),
    .pg_status_i  (surf_pg_status),
    .pg_strength_i(surf_pg_strength),

    // REAL: SURFACE.SHEET's write port -- the only writer layer F has.
    .wr_valid_o       (surf_wr_valid),
    .wr_ready_i       (surf_wr_ready),
    .wr_handle_o      (surf_wr_handle),
    .wr_texel_o       (surf_wr_texel),
    .wr_tag_o         (surf_wr_tag),
    .wr_strength_o    (surf_wr_strength),
    .wr_we_tag_o      (surf_wr_we_tag),
    .wr_we_strength_o (surf_wr_we_strength),
    .wr_src_id_o      (surf_wr_src_id),

    // I32: TERRAIN.BAKE is not composed.
    .res_valid_o   (surf_res_valid_o),
    .res_ready_i   (surf_res_ready_i),
    .res_texel_o   (surf_res_texel_o),
    .res_tag_o     (surf_res_tag_o),
    .res_strength_o(surf_res_strength_o),
    .res_before_o  (surf_res_before_o),
    .res_src_id_o  (surf_res_src_id_o),

    .stamp_done_o            (surf_stamp_done_o),
    .stamp_rejected_o        (surf_stamp_rejected_o),
    .surface_stamps_o        (surf_stamps_o),
    .surface_texels_touched_o(surf_stamp_texels_touched_o),
    .idle_o                  (surf_stamp_idle_o)
  );

  zhao_surface_sheet #(
    .Slots (SURF_SLOTS)
  ) u_surface_sheet (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    // REAL: SURFACE.STAMP is the only requester.
    .req_valid_i (surf_req_valid),
    .req_ready_o (surf_req_ready),
    .req_op_i    (surf_req_op),
    .req_handle_i(surf_req_handle),
    .req_texel_i (surf_req_texel),
    .req_src_id_i(surf_req_src_id),

    // REAL: the response stream. STAMP takes `status` and `strength`; the
    // other three leave this module as evidence (see the port declarations).
    .pg_valid_o   (surf_pg_valid),
    .pg_ready_i   (surf_pg_ready),
    .pg_op_o      (surf_pg_op_o),
    .pg_status_o  (surf_pg_status),
    .pg_tag_o     (surf_pg_tag_o),
    .pg_strength_o(surf_pg_strength),
    .pg_src_id_o  (surf_pg_src_id_o),

    // REAL: SURFACE.STAMP is the only writer.
    .wr_valid_i      (surf_wr_valid),
    .wr_ready_o      (surf_wr_ready),
    .wr_handle_i     (surf_wr_handle),
    .wr_texel_i      (surf_wr_texel),
    .wr_tag_i        (surf_wr_tag),
    .wr_strength_i   (surf_wr_strength),
    .wr_we_tag_i     (surf_wr_we_tag),
    .wr_we_strength_i(surf_wr_we_strength),
    .wr_src_id_i     (surf_wr_src_id),
    .wr_miss_o       (surf_sheet_wr_miss_o),
    .wr_miss_src_id_o(surf_sheet_wr_miss_src_id_o),

    .res_occupancy_o(surf_res_occupancy_o),
    .res_busy_o     (surf_res_busy_o),
    .res_overflow_o (surf_res_overflow_o),

    .surface_texels_touched_o(surf_sheet_texels_touched_o),
    .idle_o                  (surf_sheet_idle_o)
  );

  // ==========================================================================
  // THE SHELL. Every port straight through; nothing renamed, nothing
  // reinterpreted. Its own header is the authority on its seams.
  // ==========================================================================
  // The re-exported CMD.DMA packet stream, shell -> CMD.DECODER (section 7b)
  // AND -> CMD.EXEC (section 7c).
  logic        cmd_pkt_valid_w;
  logic [ 7:0] cmd_pkt_byte_w;
  logic [31:0] cmd_pkt_len_w;
  logic        cmd_pkt_ready_w;

  // THE FORK NOW HAS THREE CONSUMERS AND THE AND IS THE WHOLE OF IT. The shell
  // already ands its inline framer's ready with `cmd_pkt_ready_i`; this ands
  // the decoder's with the executor's. A byte moves only when ALL THREE can
  // take it, which is what makes the stream a fork rather than a tap.
  //
  // Getting this wrong is not loud. CMD.EXEC's first build took bytes on its
  // OWN ready instead of the fork's, ran one byte ahead of the true stream for
  // every packet, and reported every counter at a confident zero while the
  // decoder beside it walked four records and the packet committed cleanly. It
  // executed an empty frame and nothing failed.
  logic        cmd_dec_pkt_ready_w;
  logic        cmd_exe_pkt_ready_w;
  assign cmd_pkt_ready_w = cmd_dec_pkt_ready_w && cmd_exe_pkt_ready_w;

  zhao_shell_top_v2 #(
    .FRAMER_Q (FRAMER_Q),
    .WFIFO_W  (WFIFO_W)
  ) u_shell (
    .gpu_clk                   (gpu_clk),
    .vid_clk                   (vid_clk),
    .audio_clk                 (audio_clk),
    .rst_n                     (rst_n),
    .cfg_valid_i               (cfg_valid_i),
    .cfg_ready_o               (cfg_ready_o),
    .cfg_op_i                  (cfg_op_i),
    .cfg_page_generation_i     (cfg_page_generation_i),
    .cfg_selector_i            (cfg_selector_i),
    .cfg_row_i                 (cfg_row_i),
    .cfg_crc32_i               (cfg_crc32_i),
    .cfg_rsp_valid_o           (cfg_rsp_valid_o),
    .cfg_rsp_ready_i           (cfg_rsp_ready_i),
    .cfg_rsp_op_o              (cfg_rsp_op_o),
    .cfg_rsp_status_o          (cfg_rsp_status_o),
    .cfg_rsp_page_generation_o (cfg_rsp_page_generation_o),
    .active_page_generation_o  (active_page_generation_o),
    .pal_load_valid_i          (pal_load_valid_i),
    .pal_load_ready_o          (pal_load_ready_o),
    .pal_load_op_i             (pal_load_op_i),
    .pal_load_slot_i           (pal_load_slot_i),
    .pal_load_gen_i            (pal_load_gen_i),
    .pal_load_idx_i            (pal_load_idx_i),
    .pal_load_rgb565_i         (pal_load_rgb565_i),
    .pal_load_crc_ok_i         (pal_load_crc_ok_i),
    .tri_area2_i               (tri_area2_i),
    .tri_invw_plane_i          (tri_invw_plane_i),
    .tri_u_over_w_plane_i      (tri_u_over_w_plane_i),
    .tri_v_over_w_plane_i      (tri_v_over_w_plane_i),
    .tri_flat_request_i        (tri_flat_request_i),
    .tri_continuation_tail_i   (tri_continuation_tail_i),
    .tri_fragment_state_i      (tri_fragment_state_i),
    .fill_req_ready_i          (fill_req_ready_i),
    .fill_req_valid_o          (fill_req_valid_o),
    .fill_req_addr_o           (fill_req_addr_o),
    .fill_data_valid_i         (fill_data_valid_i),
    .fill_data_i               (fill_data_i),
    .fill_refused_i            (fill_refused_i),
    .frame_clear_word_i        (frame_clear_word_i),
    .sheet_req_ready_i         (sheet_req_ready_i),
    .sheet_req_valid_o         (sheet_req_valid_o),
    .sheet_req_op_o            (sheet_req_op_o),
    .sheet_req_handle_o        (sheet_req_handle_o),
    .sheet_req_texel_o         (sheet_req_texel_o),
    .sheet_req_src_id_o        (sheet_req_src_id_o),
    .blank_cmd_i               (blank_cmd_i),
    .scanout_ack_i             (scanout_ack_i),
    .frame_swap_valid_i        (frame_swap_valid_i),
    .frame_swap_slot_i         (frame_swap_slot_i),
    .blank_ack_o               (blank_ack_o),
    .blank_active_o            (blank_active_o),
    .lease_open_o              (lease_open_o),
    .frame_slot_ready_o        (frame_slot_ready_o),
    .v2_requests_accepted_o    (v2_requests_accepted_o),
    .v2_responses_accepted_o   (v2_responses_accepted_o),
    .v2_leases_granted_o       (v2_leases_granted_o),
    .v2_leases_refused_o       (v2_leases_refused_o),
    .v2_faults_latched_o       (v2_faults_latched_o),
    .v2_publications_o         (v2_publications_o),
    .v2_releases_o             (v2_releases_o),
    .v2_ready_events_o         (v2_ready_events_o),
    .v2_swaps_o                (v2_swaps_o),
    .v2_contentions_o          (v2_contentions_o),
    .v2_clear_handshakes_o     (v2_clear_handshakes_o),
    .v2_frames_admitted_o      (v2_frames_admitted_o),
    .v2_blit_leases_acquired_o (v2_blit_leases_acquired_o),
    .v2_blit_leases_refused_o  (v2_blit_leases_refused_o),
    .hps_state_i               (hps_state_i),
    .hps_byte_len_i            (hps_byte_len_i),
    .ring_wr_valid_o           (ring_wr_valid_o),
    .ring_wr_slot_o            (ring_wr_slot_o),
    .ring_wr_state_o           (ring_wr_state_o),
    .ring_wr_ready_i           (ring_wr_ready_i),
    .hps_req_valid_o           (hps_req_valid_o),
    .hps_req_write_o           (hps_req_write_o),
    .hps_req_addr_o            (hps_req_addr_o),
    .hps_req_len_o             (hps_req_len_o),
    .hps_req_grant_i           (hps_req_grant_i),
    .hps_wr_valid_o            (hps_wr_valid_o),
    .hps_wr_data_o             (hps_wr_data_o),
    .hps_wr_last_o             (hps_wr_last_o),
    .hps_rd_valid_i            (hps_rd_valid_i),
    .hps_rd_data_i             (hps_rd_data_i),
    .hps_rd_last_i             (hps_rd_last_i),
    .pad_present_i             (pad_present_i),
    .pad_buttons_i             (pad_buttons_i),
    .pad_lx_i                  (pad_lx_i),
    .pad_ly_i                  (pad_ly_i),
    .pad_rx_i                  (pad_rx_i),
    .pad_ry_i                  (pad_ry_i),
    .aud_wr_valid_i            (aud_wr_valid_i),
    .aud_wr_l_i                (aud_wr_l_i),
    .aud_wr_r_i                (aud_wr_r_i),
    .aud_wr_ready_o            (aud_wr_ready_o),
    .aud_refill_req_o          (aud_refill_req_o),
    .aud_occupancy_o           (aud_occupancy_o),
    .pcm_valid_o               (pcm_valid_o),
    .pcm_l_o                   (pcm_l_o),
    .pcm_r_o                   (pcm_r_o),
    .underrun_status_o         (underrun_status_o),
    .audio_underruns_o         (audio_underruns_o),
    .px_valid_o                (px_valid_o),
    .px_rgb_o                  (px_rgb_o),
    .px_x_o                    (px_x_o),
    .px_y_o                    (px_y_o),
    .px_hsync_o                (px_hsync_o),
    .px_vsync_o                (px_vsync_o),
    .px_hblank_o               (px_hblank_o),
    .px_vblank_o               (px_vblank_o),
    .scaler_violation_o        (scaler_violation_o),
    .crc_frame_o               (crc_frame_o),
    .crc_valid_o               (crc_valid_o),
    .crc_bytes_o               (crc_bytes_o),
    .crc_size_err_o            (crc_size_err_o),
    .gpu_tick_o                (gpu_tick_o),
    .gpu_tick_frame_id_o       (gpu_tick_frame_id_o),
    .gpu_tick_repeated_o       (gpu_tick_repeated_o),
    .gpu_complete_slot_o       (gpu_complete_slot_o),
    .deadline_faults_o         (deadline_faults_o),
    .frame_cycles_o            (frame_cycles_o),
    .slot_state_o              (slot_state_o),
    .fence_valid_o             (fence_valid_o),
    .fence_slot_o              (fence_slot_o),
    .fence_ok_o                (fence_ok_o),
    .fence_status_o            (fence_status_o),
    .mode_act_o                (mode_act_o),
    .dma_done_o                (dma_done_o),
    .dma_status_o              (dma_status_o),
    .blit_done_o               (blit_done_o),
    .blit_status_o             (blit_status_o),
    .pad_frame_flat_o          (pad_frame_flat_o),
    .pad_sequence_o            (pad_sequence_o),
    .input_gaps_o              (input_gaps_o),
    .rumble_duty_o             (rumble_duty_o),
    .rumble_active_o           (rumble_active_o),
    .rumble_pwm_o              (rumble_pwm_o),
    .rumble_drops_o            (rumble_drops_o),
    .cnt_snap_ready_i          (cnt_snap_ready_i),
    .cnt_snap_valid_o          (cnt_snap_valid_o),
    .cnt_snap_id_o             (cnt_snap_id_o),
    .cnt_snap_value_o          (cnt_snap_value_o),
    .cnt_window_open_o         (cnt_window_open_o),
    .cnt_cat_violation_o       (cnt_cat_violation_o),
    .guard_violations_o        (guard_violations_o),
    .starvation_o              (starvation_o),
    .init_done_o               (init_done_o),
    .refresh_stalls_o          (refresh_stalls_o),
    .bank_conflicts_o          (bank_conflicts_o),
    .scanout_preempted_o       (scanout_preempted_o),
    .hps_err_count_o           (hps_err_count_o),
    .shell_err_wfifo_o         (shell_err_wfifo_o),
    .shell_err_route_o         (shell_err_route_o),
    .shell_err_cdc_o           (shell_err_cdc_o),
    .shell_err_framer_o        (shell_err_framer_o),
    .render_frame_begin_i      (render_frame_begin_i),
    .render_frame_end_i        (render_frame_end_i),
    .render_grid_w_i           (render_grid_w_i),
    .render_grid_h_i           (render_grid_h_i),
    // REAL: GEOM.SETUP drives the triangle door. Every one of these was a
    // boundary input on this module until 2026-09-19.
    .render_tri_valid_i        (st_o_valid),
    .render_tri_ready_o        (st_o_ready),
    .geom_guard_req_i          (geom_guard_req_i),
    .geom_guard_rsp_o          (geom_guard_rsp_o),
    .geom_beat_valid_o         (geom_beat_valid_o),
    .geom_beat_data_o          (geom_beat_data_o),
    .geom_beat_last_o          (geom_beat_last_o),
    .render_kx0_i              (st_kx0),
    .render_ky0_i              (st_ky0),
    .render_kc0_i              (st_kc0),
    .render_kx1_i              (st_kx1),
    .render_ky1_i              (st_ky1),
    .render_kc1_i              (st_kc1),
    .render_kx2_i              (st_kx2),
    .render_ky2_i              (st_ky2),
    .render_kc2_i              (st_kc2),
    .render_tl_i               (st_tl),
    .render_ax_i               (st_ax),
    .render_ay_i               (st_ay),
    .render_bx_i               (st_bx),
    .render_by_i               (st_by),
    .render_cx_i               (st_cx),
    .render_cy_i               (st_cy),
    .render_min_x_i            (st_min_x),
    .render_max_x_i            (st_max_x),
    .render_min_y_i            (st_min_y),
    .render_max_y_i            (st_max_y),
    .render_src_id_i           (st_src_id),
    .render_fill_word_i        (render_fill_word_i),
    .render_clear_word_i       (render_clear_word_i),
    .render_state_i            (render_state_i),
    .render_src_a_i            (render_src_a_i),
    .render_texel_rgb_i        (render_texel_rgb_i),
    .render_texel_a_i          (render_texel_a_i),
    .render_texel_idx_i        (render_texel_idx_i),
    .render_fb_base_i          (render_fb_base_i),
    .render_fb_stride_i        (render_fb_stride_i),
    .fb_writer_i               (fb_writer_i),
    .render_drain_done_o       (render_drain_done_o),
    .render_busy_o             (render_busy_o),
    .render_pixels_o           (render_pixels_o),
    .render_bursts_o           (render_bursts_o),
    .render_stream_error_o     (render_stream_error_o),
    .render_drained_o          (render_drained_o),
    .render_fatal_o            (render_fatal_o),
    .render_issued_words_o     (render_issued_words_o),
    .render_retired_words_o    (render_retired_words_o),
    .render_overflow_o         (render_overflow_o),
    .render_fragment_error_o   (render_fragment_error_o),
    .phy_cs_n_o                (phy_cs_n_o),
    .phy_ras_n_o               (phy_ras_n_o),
    .phy_cas_n_o               (phy_cas_n_o),
    .phy_we_n_o                (phy_we_n_o),
    .phy_a_o                   (phy_a_o),
    .phy_ba_o                  (phy_ba_o),
    .phy_dq_o                  (phy_dq_o),
    .phy_dq_oe_o               (phy_dq_oe_o),
    .phy_dqm_o                 (phy_dqm_o),
    .phy_dq_i                  (phy_dq_i),

    // CMD.DMA's packet stream, re-exported by the shell 2026-09-19 so that a
    // second consumer can exist at all. `cmd_pkt_ready_i` is a real veto: the
    // shell accepts a byte only when its own inline framer AND the decoder
    // below can take it, so this is a fork and not a tap.
    .cmd_pkt_valid_o           (cmd_pkt_valid_w),
    .cmd_pkt_byte_o            (cmd_pkt_byte_w),
    .cmd_pkt_len_o             (cmd_pkt_len_w),
    .cmd_pkt_ready_i           (cmd_pkt_ready_w)
  );

  // ==========================================================================
  // 7b. CMD.DECODER -- the packet's own verdict
  // ==========================================================================
  // COMPOSED 2026-09-19, closing the refusal recorded in this file's header
  // under "BLOCKS OFFERED TO THIS COMPOSITION AND REFUSED". That refusal was
  // accurate about the obstacle and wrong about nothing: the stream WAS real
  // and already flowing, and it WAS enclosed in `zhao_shell_top_v2` with no
  // way out. Exporting it was a shell-owner change, which is what happened.
  //
  // WHAT THIS BLOCK DOES AND DOES NOT DO, said plainly so the next reader does
  // not over-read it. It walks the sealed packet and produces RECORD HEADERS
  // (opcode, byte count, source id, index) and a VERDICT (`decode_error_o`,
  // bytes consumed, records walked). It does NOT produce command PAYLOAD:
  // `SetView` carries a whole `mat4fx` (spec/commands.zidl:315) and
  // `SurfaceStamp` a transform plus radius and ring width (:360), none of which
  // fits in a record header. So composing this does NOT by itself close I14,
  // I30, I33 or I7 -- those need the command EXECUTOR that
  // spec/commands.zidl:382 says does not exist yet ("NOTHING TURNS A COMMAND
  // INTO A FRAME yet"). This block is that executor's front half, and saying so
  // here is cheaper than someone discovering it from a gap count that did not
  // move as far as they expected.
  //
  // THE VERDICT NOW HAS A CONSUMER. The refusal noted "nothing in the tree
  // takes `decode_error_o`, and the shell's framer does not validate" -- the
  // framer trusts the bytes and this block checks them. Both counters below are
  // on this module's edge, so a packet the framer accepted and the decoder
  // rejected is VISIBLE rather than silently executed.
  logic        cmd_rec_valid_w;
  logic [15:0] cmd_rec_opcode_w;
  logic [15:0] cmd_rec_bytes_w;
  logic [31:0] cmd_rec_source_id_w;
  logic [31:0] cmd_rec_index_w;

  zhao_cmd_decoder u_cmd_decoder (
    .clk              (gpu_clk),
    .rst_n            (rst_n),

    .pkt_valid_i      (cmd_pkt_valid_w),
    .pkt_ready_o      (cmd_dec_pkt_ready_w),
    .pkt_byte_i       (cmd_pkt_byte_w),
    .pkt_len_i        (cmd_pkt_len_w),

    // The record headers are retired unconditionally. The decoder's own
    // contract (its lines 70-75) says a consumer MUST NOT ACT on a record
    // before `decode_done_o` reports ZH_ABI_OK, because the payload CRC cannot
    // conclude until the last byte. Retiring is not acting: nothing downstream
    // of here changes visible state, the headers are counted and observed, and
    // the executor that will act on them is required to gate on the verdict.
    // Holding `rec_ready_o` low instead would stall the DMA and, through the
    // fork above, the shell's own framer with it.
    .rec_valid_o      (cmd_rec_valid_w),
    .rec_ready_i      (1'b1),
    .rec_opcode_o     (cmd_rec_opcode_w),
    .rec_bytes_o      (cmd_rec_bytes_w),
    .rec_source_id_o  (cmd_rec_source_id_w),
    .rec_index_o      (cmd_rec_index_w),

    .decode_done_o    (cmd_decode_done_o),
    .decode_error_o   (cmd_decode_error_o),
    .bytes_consumed_o (cmd_bytes_consumed_o),
    .commands_o       (cmd_commands_o)
  );

  // The record header stream, on this module's edge. It is observable rather
  // than dropped, for the same reason SURFACE.STAMP's `stamp_results` is (I32):
  // a stream with no consumer that leaves the module is evidence, while one
  // that ends in a wire is pruned logic wearing a port's name.
  assign cmd_rec_valid_o     = cmd_rec_valid_w;
  assign cmd_rec_opcode_o    = cmd_rec_opcode_w;
  assign cmd_rec_bytes_o     = cmd_rec_bytes_w;
  assign cmd_rec_source_id_o = cmd_rec_source_id_w;
  assign cmd_rec_index_o     = cmd_rec_index_w;

  // ==========================================================================
  // 8. THE TERRAIN PAGING SPINE
  // ==========================================================================
  // TERRAIN.CMD -> TERRAIN.SEQ -> TERRAIN.RESIDENCY / TERRAIN.LOADQ ->
  // TERRAIN.PAGELOADER -> TERRAIN.RESIDENCY. Five blocks, every seam between
  // them a port-for-port, width-for-width match declared by the blocks' own
  // headers. There is no arithmetic here, no state machine and no arbiter that
  // this file invented: the ONE place two clients meet is the REAL
  // `zhao_hps_arbiter`, which is a block with a contract and a fairness rule,
  // instantiated rather than imitated.
  //
  // WHY THE DIRECTORY IS `zhao_terrain_residency_v2` AND NOT `_residency`.
  // `design/blocks.yml`'s TERRAIN.RESIDENCY row says it in three independent
  // places: its purpose line ends "the direct-mapped prototype is SUPERSEDED
  // because two islands may legally overlap in local patch coordinates", its
  // `tests:` are `terrain_residency_v2_directed.cpp` and
  // `terrain_residency_v2_random.cpp`, and its UNIT_VERIFIED maturity evidence
  // is the v2 directed test. `zhao_terrain_residency.sv`'s own header says
  // "FIRST BLOCK OF THE WORLD LAYER. Nothing instantiates it yet." The v2
  // block is the ledger's TERRAIN.RESIDENCY and the v1 file is the prototype
  // that row retires. Every port below is v2's, and `zhao_terrain_seq`'s header
  // names v2 by file when it explains where its SEQW comes from.
  //
  // WHAT IS NOT HERE, AND WHY -- the refusals are the valuable half:
  //   * TERRAIN.WRITEBACK is a real consumer of TERRAIN.SEQ's `wb_*` and is
  //     LEFT OUT (entry I28). Two reasons, either sufficient. Its job port
  //     needs `j_journal_addr_i` and `j_seq_i`, which TERRAIN.SEQ does not
  //     emit and nothing in `fpga/rtl` owns -- the composed bench mints the
  //     journal ticket and its own comment calls that glue "a finding". And it
  //     would be a THIRD MEM.HPS.BRIDGE client: `zhao_hps_arbiter` has exactly
  //     two ports and its rule-5 starvation law is written for two guaranteed
  //     clients, so a third is an owner ruling and not a wiring act.
  //   * TERRAIN.PAGESTREAM / MIPFEED / MIPGEN connect to EACH OTHER exactly --
  //     that chain is real and its seams are clean. What it has no owner for is
  //     its HEAD: something must notice a page has landed and ask for its mips,
  //     and TERRAIN.RESIDENCY has no port to ask with. The composed bench mints
  //     that trigger in an eight-deep queue and says so. Composing the three
  //     here would add their area for a chain nothing in this core can start.
  //   * TERRAIN.NORMALS -> TERRAIN.SHADE is an exact packet match and TESS is
  //     already its producer on paper -- but `tri_valid_o` is ModeTri, and the
  //     declaration above records that the sequencer presents mode 1 then mode
  //     2 and never mode 0. The port is dead BY CONSTRUCTION, so the chain
  //     would elaborate and could not see a beat.
  //   * TERRAIN.PROJECT carries a PRIVATE `zhao_project_core`. The console
  //     already runs ONE `zhao_proj_subsystem` with client A (GEOM) and client
  //     B (TERRAIN.GROUP_SEQ) live, which is the deduplication this campaign
  //     exists to achieve; instantiating TERRAIN.PROJECT would put a second
  //     projector beside it and undo exactly that. The terrain projection this
  //     block would do is ALREADY DONE by client B, so it is not a missing
  //     function -- it is a superseded implementation of a present one.
  //   * TERRAIN.VISIBLE (and TERRAIN.ISLAND, which it instantiates) speak to a
  //     DIFFERENT directory: `res_ix_o`/`res_iz_o` are unsigned 16-bit patch
  //     coordinates answered with a 32-bit `handle`, where v2's canonical key
  //     is {epoch, island, ix, iz} answered with {slot, gen}. Those are two
  //     abstractions, not two spellings, and reconciling them here would be the
  //     hidden adapter this file must not contain.
  //   * TERRAIN.PATCH and TERRAIN.COMPCACHE would close I22, and the seam
  //     between them is declared port-for-port by COMPCACHE's own header. They
  //     are blocked on PLACEMENT: TERRAIN.PATCH needs `wx_i`/`wz_i` (placed
  //     world x/z) and COMPCACHE needs its `pos_*` write port, and no block in
  //     the tree produces either. Inventing the placement from the lattice
  //     index and a pitch is arithmetic in the composer.

  // ---- the HPS clients, merged by the block whose job that is --------------
  zhao_hps_burst_req_t tcm_hps_req,  tpl_hps_req;
  logic                tcm_hps_grant, tpl_hps_grant;
  zhao_hps_burst_rsp_t tcm_hps_rsp,  tpl_hps_rsp;

  // TERRAIN.CMD takes client 0 and TERRAIN.PAGELOADER client 1, which is the
  // arbiter's OWN documented intent rather than a preference invented here:
  // its port comments read "client 0 (high priority: CMD.DMA)" and "client 1
  // (low priority: DEBUG.FRAMEBLIT)". A short command-list read is the
  // command-class traffic; a 21,376-byte page is the bulk transfer, and
  // `c1_wait_cycles_o` is the arbiter's own instrument for saying what that
  // choice costs the loader.

  zhao_hps_arbiter u_terr_hps_arb (
    .clk           (gpu_clk),
    .rst_n         (rst_n),
    .c0_req_i      (tcm_hps_req),
    .c0_req_grant_o(tcm_hps_grant),
    .c0_wr_valid_i (1'b0),
    .c0_wr_data_i  (64'd0),
    .c0_wr_last_i  (1'b0),
    .c0_rsp_o      (tcm_hps_rsp),
    .c1_req_i      (tpl_hps_req),
    .c1_req_grant_o(tpl_hps_grant),
    .c1_wr_valid_i (1'b0),
    .c1_wr_data_i  (64'd0),
    .c1_wr_last_i  (1'b0),
    .c1_rsp_o      (tpl_hps_rsp),
    .b_req_o       (terr_hps_req_o),
    .b_req_grant_i (terr_hps_grant_i),
    .b_wr_valid_o  (terr_hps_wr_valid_o),
    .b_wr_data_o   (terr_hps_wr_data_o),
    .b_wr_last_o   (terr_hps_wr_last_o),
    .b_rsp_i       (terr_hps_rsp_i),
    .c0_bursts_o     (terr_hps_c0_bursts_o),
    .c1_bursts_o     (terr_hps_c1_bursts_o),
    .c1_wait_cycles_o(terr_hps_c1_wait_cycles_o)
  );

  // ---- TERRAIN.CMD -> TERRAIN.SEQ -----------------------------------------
  wire               tcm_fr_start;
  wire [31:0]        tcm_fr_epoch;
  wire [15:0]        tcm_fr_patch_count;
  wire [31:0]        tcm_fr_sequence;
  wire               tcm_rec_valid, tcm_rec_ready;
  wire [31:0]        tcm_rec_island;
  wire signed [15:0] tcm_rec_ix, tcm_rec_iz;
  wire [63:0]        tcm_rec_hps_addr;
  wire [31:0]        tcm_rec_crc;
  wire [15:0]        tcm_rec_flags;
  wire [7:0]         tcm_rec_view_mask, tcm_rec_priority;
  wire [31:0]        tcm_rec_src_id;
  /* verilator lint_off UNUSEDSIGNAL */
  wire [31:0]        tcm_list_bytes_read;
  wire               tcm_idle;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_terrain_cmd u_terrain_cmd (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    .cfg_hps_client_i (ZHAO_CLIENT_TERRAIN_BUILD),
    .cfg_epoch_i      (terr_cfg_epoch_i),
    .cfg_arena_base_i (terr_cfg_arena_base_i),
    .cfg_arena_bytes_i(terr_cfg_arena_bytes_i),

    .j_valid_i      (terr_cmd_valid_i),
    .j_ready_o      (terr_cmd_ready_o),
    .j_epoch_i      (terr_cmd_epoch_i),
    .j_list_off_i   (terr_cmd_list_off_i),
    .j_list_bytes_i (terr_cmd_list_bytes_i),
    .j_list_crc_i   (terr_cmd_list_crc_i),
    .j_patch_count_i(terr_cmd_patch_count_i),
    .j_sequence_i   (terr_cmd_sequence_i),
    .j_src_id_i     (terr_cmd_src_id_i),

    .hps_req_o      (tcm_hps_req),
    .hps_req_grant_i(tcm_hps_grant),
    .hps_rsp_i      (tcm_hps_rsp),

    .fr_start_o      (tcm_fr_start),
    .fr_epoch_o      (tcm_fr_epoch),
    .fr_patch_count_o(tcm_fr_patch_count),
    .fr_sequence_o   (tcm_fr_sequence),

    .rec_valid_o    (tcm_rec_valid),
    .rec_ready_i    (tcm_rec_ready),
    .rec_island_o   (tcm_rec_island),
    .rec_ix_o       (tcm_rec_ix),
    .rec_iz_o       (tcm_rec_iz),
    .rec_hps_addr_o (tcm_rec_hps_addr),
    .rec_crc_o      (tcm_rec_crc),
    .rec_flags_o    (tcm_rec_flags),
    .rec_view_mask_o(tcm_rec_view_mask),
    .rec_priority_o (tcm_rec_priority),
    .rec_src_id_o   (tcm_rec_src_id),

    .done_valid_o   (terr_cmd_done_valid_o),
    .done_ready_i   (terr_cmd_done_ready_i),
    .done_ok_o      (terr_cmd_done_ok_o),
    .done_verdict_o (terr_cmd_done_verdict_o),
    .done_src_id_o  (terr_cmd_done_src_id_o),
    .done_crc_seen_o(terr_cmd_done_crc_seen_o),

    .sets_accepted_o  (terr_cmd_sets_accepted_o),
    .sets_refused_o   (terr_cmd_sets_refused_o),
    .records_emitted_o(terr_cmd_records_emitted_o),
    .list_bytes_read_o(tcm_list_bytes_read),
    .crc_fails_o      (terr_cmd_crc_fails_o),
    .bridge_errs_o    (terr_cmd_bridge_errs_o),
    .idle_o           (tcm_idle)
  );

  // ---- TERRAIN.SEQ, and the directory it drives ---------------------------
  wire                     tsq_lu_valid, tsq_lu_ready;
  wire [31:0]              tsq_lu_epoch, tsq_lu_island;
  wire signed [15:0]       tsq_lu_ix, tsq_lu_iz;
  wire                     tres_lu_ans_valid, tres_lu_ans_hit;
  wire [TERR_SLOTW-1:0]    tres_lu_ans_slot;
  wire [TERR_GENW-1:0]     tres_lu_ans_gen;

  wire                     tsq_cl_valid, tsq_cl_ready;
  wire [31:0]              tsq_cl_epoch, tsq_cl_island, tsq_cl_expect_crc;
  wire signed [15:0]       tsq_cl_ix, tsq_cl_iz;
  wire [TERR_SEQW-1:0]     tsq_cl_seq;
  wire                     tres_cl_ans_valid, tres_cl_ans_same, tres_cl_ans_refused;
  wire [TERR_SLOTW-1:0]    tres_cl_ans_slot;
  wire [TERR_GENW-1:0]     tres_cl_ans_gen;
  wire                     tres_cl_ev_dirty;
  wire [31:0]              tres_cl_ev_island;
  wire signed [15:0]       tres_cl_ev_ix, tres_cl_ev_iz;
  wire [TERR_GENW-1:0]     tres_cl_ev_gen;

  wire                     tsq_pin_valid, tsq_pin_ready;
  wire [TERR_SLOTW-1:0]    tsq_pin_slot;
  wire [TERR_GENW-1:0]     tsq_pin_gen;
  wire [31:0]              tsq_pin_epoch;

  wire                     tsq_ld_valid, tsq_ld_ready;
  wire [TERR_SLOTW-1:0]    tsq_ld_slot;
  wire [TERR_GENW-1:0]     tsq_ld_gen;
  wire [31:0]              tsq_ld_epoch, tsq_ld_island, tsq_ld_expect_crc, tsq_ld_src_id;
  wire signed [15:0]       tsq_ld_ix, tsq_ld_iz;
  wire [63:0]              tsq_ld_hps_addr;

  // TERRAIN.RESIDENCY's own eviction-valid, its post-reset init level, and the
  // four counters no consumer in this core reads. Named rather than left as an
  // empty by-name connection, because `.cl_evicted_o()` reads as "there is no
  // such signal" when it means "nothing here consumes it".
  /* verilator lint_off UNUSEDSIGNAL */
  wire                     tres_ready, tres_cl_evicted;
  wire [31:0]              tres_dirty_evictions, tres_refused_all_pinned;
  wire [31:0]              tres_stale_events;
  wire [31:0]              tsq_prefetch_resident, tsq_loads_deferred;
  wire [31:0]              tsq_writebacks_issued, tsq_compose_slots_used;
  wire [31:0]              tsq_pins_issued, tsq_drained, tsq_wb_wait_cycles;
  wire                     tsq_frame_fault;
  wire [31:0]              tsq_fault_src_id, tsq_fault_island;
  wire signed [15:0]       tsq_fault_ix, tsq_fault_iz;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_terrain_seq #(
    .COMPOSE_SLOTS(TERR_CSLOTS),
    .SLOTW        (TERR_SLOTW),
    .GENW         (TERR_GENW),
    .SEQW         (TERR_SEQW)
  ) u_terrain_seq (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // The frame ring and the record stream, both from TERRAIN.CMD. Name for
    // name and width for width; nothing is renamed and nothing is computed.
    .fr_start_i      (tcm_fr_start),
    .fr_epoch_i      (tcm_fr_epoch),
    .fr_patch_count_i(tcm_fr_patch_count),
    .fr_sequence_i   (tcm_fr_sequence),
    .fr_busy_o       (terr_seq_busy_o),
    .fr_done_o       (terr_seq_done_o),

    .cfg_load_budget_i(terr_cfg_load_budget_i),

    .rec_valid_i    (tcm_rec_valid),
    .rec_ready_o    (tcm_rec_ready),
    .rec_island_i   (tcm_rec_island),
    .rec_ix_i       (tcm_rec_ix),
    .rec_iz_i       (tcm_rec_iz),
    .rec_hps_addr_i (tcm_rec_hps_addr),
    .rec_crc_i      (tcm_rec_crc),
    .rec_flags_i    (tcm_rec_flags),
    .rec_view_mask_i(tcm_rec_view_mask),
    .rec_priority_i (tcm_rec_priority),
    .rec_src_id_i   (tcm_rec_src_id),

    // NO LOOKUP SHIM, AND THAT IS A DELIBERATE READING OF THE RTL RATHER THAN
    // a copy of the composed bench. `tb_terrain_world.sv` carries a hold-and-
    // retry shim here, and its own comment dates it: it was written when the
    // directory had no `lu_ready_o` and the sequencer had to guess whether its
    // one-cycle offer had landed. Both ports exist now, and
    // `zhao_terrain_seq.sv:345` drives `lu_valid_o = (st == S_LOOKUP)` with
    // `S_LOOKUP: if (lu_ready_i) st_n = S_WAIT_LU` -- a HELD offer released by
    // a real ready. A lookup can no longer be dropped, so the shim would be
    // state this composer invented to solve a problem the blocks already
    // solved between them. The bench keeps it behind `cfg_dir_gate_i` to
    // reproduce the historical defect, which is a different job.
    .lu_valid_o    (tsq_lu_valid),
    .lu_ready_i    (tsq_lu_ready),
    .lu_epoch_o    (tsq_lu_epoch),
    .lu_island_o   (tsq_lu_island),
    .lu_ix_o       (tsq_lu_ix),
    .lu_iz_o       (tsq_lu_iz),
    .lu_ans_valid_i(tres_lu_ans_valid),
    .lu_ans_hit_i  (tres_lu_ans_hit),
    .lu_ans_slot_i (tres_lu_ans_slot),
    .lu_ans_gen_i  (tres_lu_ans_gen),

    .cl_valid_o        (tsq_cl_valid),
    .cl_ready_i        (tsq_cl_ready),
    .cl_epoch_o        (tsq_cl_epoch),
    .cl_island_o       (tsq_cl_island),
    .cl_ix_o           (tsq_cl_ix),
    .cl_iz_o           (tsq_cl_iz),
    .cl_expect_crc_o   (tsq_cl_expect_crc),
    .cl_seq_o          (tsq_cl_seq),
    .cl_ans_valid_i    (tres_cl_ans_valid),
    .cl_ans_same_i     (tres_cl_ans_same),
    .cl_ans_refused_i  (tres_cl_ans_refused),
    .cl_ans_slot_i     (tres_cl_ans_slot),
    .cl_ans_gen_i      (tres_cl_ans_gen),
    .cl_ans_ev_dirty_i (tres_cl_ev_dirty),
    .cl_ans_ev_island_i(tres_cl_ev_island),
    .cl_ans_ev_ix_i    (tres_cl_ev_ix),
    .cl_ans_ev_iz_i    (tres_cl_ev_iz),
    .cl_ans_ev_gen_i   (tres_cl_ev_gen),

    .pin_valid_o(tsq_pin_valid),
    .pin_ready_i(tsq_pin_ready),
    .pin_slot_o (tsq_pin_slot),
    .pin_gen_o  (tsq_pin_gen),
    .pin_epoch_o(tsq_pin_epoch),

    // TERRAIN.WRITEBACK is not composed -- entry I28. Both halves of the
    // barrier leave the module together, so the job and its completion stay
    // one seam rather than becoming a job that goes out and an answer that is
    // invented here.
    .wb_valid_o      (terr_wb_valid_o),
    .wb_ready_i      (terr_wb_ready_i),
    .wb_done_valid_i (terr_wb_done_valid_i),
    .wb_done_slot_i  (terr_wb_done_slot_i),
    .wb_slot_o       (terr_wb_slot_o),
    .wb_gen_o        (terr_wb_gen_o),
    .wb_epoch_o      (terr_wb_epoch_o),
    .wb_island_o     (terr_wb_island_o),
    .wb_ix_o         (terr_wb_ix_o),
    .wb_iz_o         (terr_wb_iz_o),
    .wb_src_id_o     (terr_wb_src_id_o),
    .wb_wait_cycles_o(tsq_wb_wait_cycles),

    .ld_valid_o     (tsq_ld_valid),
    .ld_ready_i     (tsq_ld_ready),
    .ld_slot_o      (tsq_ld_slot),
    .ld_gen_o       (tsq_ld_gen),
    .ld_epoch_o     (tsq_ld_epoch),
    .ld_island_o    (tsq_ld_island),
    .ld_ix_o        (tsq_ld_ix),
    .ld_iz_o        (tsq_ld_iz),
    .ld_hps_addr_o  (tsq_ld_hps_addr),
    .ld_expect_crc_o(tsq_ld_expect_crc),
    .ld_src_id_o    (tsq_ld_src_id),

    // The terrain COMPOSE ENGINE's door -- entry I27.
    .is_valid_o      (terr_is_valid_o),
    .is_ready_i      (terr_is_ready_i),
    .is_slot_o       (terr_is_slot_o),
    .is_gen_o        (terr_is_gen_o),
    .is_epoch_o      (terr_is_epoch_o),
    .is_island_o     (terr_is_island_o),
    .is_ix_o         (terr_is_ix_o),
    .is_iz_o         (terr_is_iz_o),
    .is_cslot_valid_o(terr_is_cslot_valid_o),
    .is_cslot_o      (terr_is_cslot_o),
    .is_flags_o      (terr_is_flags_o),
    .is_view_mask_o  (terr_is_view_mask_o),
    .is_priority_o   (terr_is_priority_o),
    .is_src_id_o     (terr_is_src_id_o),

    .frame_fault_o  (tsq_frame_fault),
    .fault_src_id_o (tsq_fault_src_id),
    .fault_island_o (tsq_fault_island),
    .fault_ix_o     (tsq_fault_ix),
    .fault_iz_o     (tsq_fault_iz),
    .err_stray_ans_o(terr_seq_err_stray_ans_o),

    .records_consumed_o    (terr_seq_records_consumed_o),
    .patches_issued_o      (terr_seq_patches_issued_o),
    .prefetch_resident_o   (tsq_prefetch_resident),
    .skipped_not_resident_o(terr_seq_skipped_not_resident_o),
    .claims_issued_o       (terr_seq_claims_issued_o),
    .claims_refused_o      (terr_seq_claims_refused_o),
    .claims_same_o         (terr_seq_claims_same_o),
    .loads_issued_o        (terr_seq_loads_issued_o),
    .loads_deferred_o      (tsq_loads_deferred),
    .writebacks_issued_o   (tsq_writebacks_issued),
    .compose_slots_used_o  (tsq_compose_slots_used),
    .pins_issued_o         (tsq_pins_issued),
    .drained_o             (tsq_drained),
    .frame_faults_o        (terr_seq_frame_faults_o)
  );

  // ---- TERRAIN.RESIDENCY (the v2 directory, per the ledger row) -----------
  wire                  tpl_fin_valid, tpl_fin_ready, tpl_fin_ok;
  wire [TERR_MEMSLOT-1:0] tpl_fin_slot_w;
  wire [TERR_GENW-1:0]  tpl_fin_gen;
  wire [31:0]           tpl_fin_epoch, tpl_fin_crc;

  // THE WIDTH STEP, CROSSED IN EXACTLY ONE DIRECTION AND QUALIFIED, NOT
  // NARROWED. TERRAIN.PAGELOADER carries a pool index ONE BIT WIDER than the
  // directory's {set, way} handle, deliberately: at exactly $clog2(1024) = 10
  // bits a computed slot of 1,024 CANNOT be expressed and would arrive
  // TRUNCATED as slot 0, overwriting a live page. "A refusal is not a clamp."
  // Passing the low ten bits alone would hand that alias straight back and
  // publish another patch's page under this patch's key.
  //
  // So the completion is OFFERED only when the extra bit is clear, and the
  // ones it is not are COUNTED. In this composition the producer of `j_slot_i`
  // is a TERR_SLOTW wire, so the bit is structurally zero and the counter
  // cannot move under any legal stimulus -- which makes it exactly the
  // `wq_overflow_o` shape CLAUDE.md describes, and it therefore OWES a
  // committed mutant under `tests/mutants/` before its silence may be quoted.
  // It is kept rather than dropped because the qualification it ledgers is
  // real: the day a wider producer appears, this is what refuses it.
  wire tpl_fin_over = tpl_fin_slot_w[TERR_MEMSLOT-1];

  // IT COUNTS COMPLETIONS, NOT THE CYCLES ONE WAITS -- AND IT DID NOT.
  // Found 2026-09-19 by firing this counter for the first time, through the
  // wrapper mutant its own comment above demanded. The counter was
  //
  //     else if (tpl_fin_valid && tpl_fin_over && ...) <= + 1
  //
  // which is a LEVEL, and `fin_valid_o` is held until `fin_ready_i`. An
  // offending completion is deliberately never offered to the directory
  // (`.fin_valid_i(tpl_fin_valid && !tpl_fin_over)` below), so its ready never
  // comes and the level stands for the rest of the run: ONE illegal completion
  // read 700,109 on the mutant. A counter whose port comment says "completions
  // carried a pool slot outside the directory's range" and whose reading is
  // six orders of magnitude off cannot be quoted for either number.
  //
  // So it is edge-qualified. `tpl_fin_over_q` holds last cycle's offer, and
  // only the 0->1 transition counts -- exactly one increment per completion,
  // whether or not the completion is ever accepted.
  //
  // AND THE MUTANT RECORDED A SECOND FACT, which is a CONSEQUENCE and not a
  // defect: because the offer is suppressed rather than consumed,
  // TERRAIN.PAGELOADER stays in S_FIN for ever on an illegal slot and the
  // paging spine stops. That is loud rather than silent -- the opposite of the
  // failure this refusal exists to prevent -- and it is the correct standing
  // for a state the composition makes unreachable. Choosing anything else
  // (accept-and-drop, or a drain) is a policy for TERRAIN.RESIDENCY's contract
  // to state, not for this composer to invent.
  logic tpl_fin_over_q;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      tpl_fin_over_q          <= 1'b0;
      terr_pl_slot_overflow_o <= 32'd0;
    end else begin
      tpl_fin_over_q <= tpl_fin_valid && tpl_fin_over;
      if (tpl_fin_valid && tpl_fin_over && !tpl_fin_over_q &&
          (terr_pl_slot_overflow_o != 32'hFFFF_FFFF))
        terr_pl_slot_overflow_o <= terr_pl_slot_overflow_o + 32'd1;
    end
  end

  zhao_terrain_residency_v2 #(
    .SETS(TERR_SETS),
    .WAYS(TERR_WAYS),
    .GENW(TERR_GENW),
    .PINW(TERR_PINW),
    .SEQW(TERR_SEQW)
  ) u_terrain_residency (
    .clk    (gpu_clk),
    .rst_n  (rst_n),
    .ready_o(tres_ready),

    .lu_valid_i (tsq_lu_valid),
    .lu_ready_o (tsq_lu_ready),
    .lu_epoch_i (tsq_lu_epoch),
    .lu_island_i(tsq_lu_island),
    .lu_ix_i    (tsq_lu_ix),
    .lu_iz_i    (tsq_lu_iz),
    .lu_valid_o (tres_lu_ans_valid),
    .lu_hit_o   (tres_lu_ans_hit),
    .lu_slot_o  (tres_lu_ans_slot),
    .lu_gen_o   (tres_lu_ans_gen),

    .cl_valid_i     (tsq_cl_valid),
    .cl_ready_o     (tsq_cl_ready),
    .cl_epoch_i     (tsq_cl_epoch),
    .cl_island_i    (tsq_cl_island),
    .cl_ix_i        (tsq_cl_ix),
    .cl_iz_i        (tsq_cl_iz),
    .cl_expect_crc_i(tsq_cl_expect_crc),
    .cl_seq_i       (tsq_cl_seq),
    .cl_valid_o         (tres_cl_ans_valid),
    .cl_same_o          (tres_cl_ans_same),
    .cl_refused_o       (tres_cl_ans_refused),
    .cl_slot_o          (tres_cl_ans_slot),
    .cl_gen_o           (tres_cl_ans_gen),
    .cl_evicted_o       (tres_cl_evicted),
    .cl_evicted_dirty_o (tres_cl_ev_dirty),
    .cl_evicted_island_o(tres_cl_ev_island),
    .cl_evicted_ix_o    (tres_cl_ev_ix),
    .cl_evicted_iz_o    (tres_cl_ev_iz),
    .cl_evicted_gen_o   (tres_cl_ev_gen),

    .fin_valid_i(tpl_fin_valid && !tpl_fin_over),
    .fin_ready_o(tpl_fin_ready),
    .fin_slot_i (tpl_fin_slot_w[TERR_SLOTW-1:0]),
    .fin_gen_i  (tpl_fin_gen),
    .fin_epoch_i(tpl_fin_epoch),
    .fin_ok_i   (tpl_fin_ok),
    .fin_crc_i  (tpl_fin_crc),

    // I27: TERRAIN.BAKE marks a page dirty and the compose engine unpins it on
    // job completion. Neither is composed, so both leave the module.
    .dm_valid_i(terr_dm_valid_i),
    .dm_ready_o(terr_dm_ready_o),
    .dm_slot_i (terr_dm_slot_i),
    .dm_gen_i  (terr_dm_gen_i),
    .dm_epoch_i(terr_dm_epoch_i),
    .dm_bd_i   (terr_dm_bd_i),
    .dm_f_i    (terr_dm_f_i),
    .dm_mips_i (terr_dm_mips_i),

    .pin_valid_i(tsq_pin_valid),
    .pin_ready_o(tsq_pin_ready),
    .pin_slot_i (tsq_pin_slot),
    .pin_gen_i  (tsq_pin_gen),
    .pin_epoch_i(tsq_pin_epoch),

    .unpin_valid_i(terr_unpin_valid_i),
    .unpin_ready_o(terr_unpin_ready_o),
    .unpin_slot_i (terr_unpin_slot_i),
    .unpin_gen_i  (terr_unpin_gen_i),
    .unpin_epoch_i(terr_unpin_epoch_i),

    // I28, other end: the F-sheet journal barrier. TERRAIN.WRITEBACK owns it
    // and is not composed.
    .wb_valid_i(terr_wback_valid_i),
    .wb_ready_o(terr_wback_ready_o),
    .wb_slot_i (terr_wback_slot_i),
    .wb_gen_i  (terr_wback_gen_i),
    .wb_epoch_i(terr_wback_epoch_i),

    .chk_valid_i(terr_chk_valid_i),
    .chk_slot_i (terr_chk_slot_i),
    .chk_gen_i  (terr_chk_gen_i),
    .chk_epoch_i(terr_chk_epoch_i),
    .chk_valid_o(terr_chk_valid_o),
    .chk_stale_o(terr_chk_stale_o),

    .hits_o              (terr_res_hits_o),
    .misses_o            (terr_res_misses_o),
    .claims_o            (terr_res_claims_o),
    .evictions_o         (terr_res_evictions_o),
    .dirty_evictions_o   (tres_dirty_evictions),
    .refused_all_pinned_o(tres_refused_all_pinned),
    .stale_events_o      (tres_stale_events),
    .crc_failures_o      (terr_res_crc_failures_o),
    .resident_o          (terr_res_resident_o)
  );

  // ---- TERRAIN.LOADQ, between the sequencer and the loader ----------------
  // The queue exists so the sequencer never waits on the LOADER'S acceptance.
  // A page load is thousands of clocks; the sequencer's job is to get through
  // the frame's record list.
  wire                  tlq_q_valid, tlq_q_ready;
  wire [TERR_SLOTW-1:0] tlq_q_slot;
  wire [TERR_GENW-1:0]  tlq_q_gen;
  wire [31:0]           tlq_q_epoch, tlq_q_island, tlq_q_expect_crc, tlq_q_src_id;
  wire signed [15:0]    tlq_q_ix, tlq_q_iz;
  wire [63:0]           tlq_q_hps_addr;
  /* verilator lint_off UNUSEDSIGNAL */
  wire [31:0]           tlq_drained, tlq_refused, tlq_level, tlq_inflight;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_terrain_loadq #(
    .DEPTH(TERR_LOADQ_D),
    .SLOTW(TERR_SLOTW),
    .GENW (TERR_GENW)
  ) u_terrain_loadq (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    .j_valid_i     (tsq_ld_valid),
    .j_ready_o     (tsq_ld_ready),
    .j_slot_i      (tsq_ld_slot),
    .j_gen_i       (tsq_ld_gen),
    .j_epoch_i     (tsq_ld_epoch),
    .j_island_i    (tsq_ld_island),
    .j_ix_i        (tsq_ld_ix),
    .j_iz_i        (tsq_ld_iz),
    .j_hps_addr_i  (tsq_ld_hps_addr),
    .j_expect_crc_i(tsq_ld_expect_crc),
    .j_src_id_i    (tsq_ld_src_id),

    .q_valid_o     (tlq_q_valid),
    .q_ready_i     (tlq_q_ready),
    .q_slot_o      (tlq_q_slot),
    .q_gen_o       (tlq_q_gen),
    .q_epoch_o     (tlq_q_epoch),
    .q_island_o    (tlq_q_island),
    .q_ix_o        (tlq_q_ix),
    .q_iz_o        (tlq_q_iz),
    .q_hps_addr_o  (tlq_q_hps_addr),
    .q_expect_crc_o(tlq_q_expect_crc),
    .q_src_id_o    (tlq_q_src_id),

    // The drain is the MECHANISM for abandoning a faulted frame's queued jobs
    // and the POLICY is an owner ruling nobody has made -- the block's own
    // header says so. Nothing in this core asserts it, which is the same
    // standing it has in the composed bench.
    .drain_i(1'b0),

    .accepted_o  (terr_lq_accepted_o),
    .issued_o    (terr_lq_issued_o),
    .drained_o   (tlq_drained),
    .refused_o   (tlq_refused),
    .level_o     (tlq_level),
    .inflight_o  (tlq_inflight),
    .high_water_o(terr_lq_high_water_o)
  );

  // ---- TERRAIN.PAGELOADER -------------------------------------------------
  /* verilator lint_off UNUSEDSIGNAL */
  wire [3:0]         tpl_fin_verdict;
  wire [31:0]        tpl_fin_src_id;
  wire [31:0]        tpl_fault_crc_seen, tpl_fault_crc_expect;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_terrain_pageloader #(
    .PAGE_BYTES  (TERR_PAGE_BYTES),
    .REGION_BASE (TERR_POOL_BASE),
    .REGION_SLOTS(TERR_POOL_SLOTS),
    .GENW        (TERR_GENW)
  ) u_terrain_pageloader (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    .cfg_vram_client_i    (ZHAO_CLIENT_TERRAIN_BUILD),
    .cfg_hps_client_i     (ZHAO_CLIENT_TERRAIN_BUILD),
    .cfg_hps_arena_base_i (terr_cfg_arena_base_i),
    .cfg_hps_arena_bytes_i(terr_cfg_arena_bytes_i),
    .cfg_epoch_i          (terr_cfg_epoch_i),

    // The zero extension across the pool's extra refusal bit, written here
    // rather than assumed. See the note at `tpl_fin_over` for the other end.
    .j_valid_i     (tlq_q_valid),
    .j_ready_o     (tlq_q_ready),
    .j_slot_i      ({1'b0, tlq_q_slot}),
    .j_gen_i       (tlq_q_gen),
    .j_epoch_i     (tlq_q_epoch),
    .j_island_i    (tlq_q_island),
    .j_ix_i        (tlq_q_ix),
    .j_iz_i        (tlq_q_iz),
    .j_hps_addr_i  (tlq_q_hps_addr),
    .j_expect_crc_i(tlq_q_expect_crc),
    .j_src_id_i    (tlq_q_src_id),

    .hps_req_o      (tpl_hps_req),
    .hps_req_grant_i(tpl_hps_grant),
    .hps_rsp_i      (tpl_hps_rsp),

    .guard_req_o   (terr_guard_req_o),
    .guard_rsp_i   (terr_guard_rsp_i),
    .guard_wdata_o (terr_guard_wdata_o),
    .guard_wvalid_o(terr_guard_wvalid_o),
    .guard_wready_i(terr_guard_wready_i),
    .guard_wlast_o (terr_guard_wlast_o),

    .fin_valid_o  (tpl_fin_valid),
    .fin_ready_i  (tpl_fin_ready),
    .fin_slot_o   (tpl_fin_slot_w),
    .fin_gen_o    (tpl_fin_gen),
    .fin_epoch_o  (tpl_fin_epoch),
    .fin_ok_o     (tpl_fin_ok),
    .fin_crc_o    (tpl_fin_crc),
    .fin_verdict_o(tpl_fin_verdict),
    .fin_src_id_o (tpl_fin_src_id),

    .fault_island_o    (terr_pl_fault_island_o),
    .fault_ix_o        (terr_pl_fault_ix_o),
    .fault_iz_o        (terr_pl_fault_iz_o),
    .fault_src_id_o    (terr_pl_fault_src_id_o),
    .fault_verdict_o   (terr_pl_fault_verdict_o),
    .fault_crc_seen_o  (tpl_fault_crc_seen),
    .fault_crc_expect_o(tpl_fault_crc_expect),

    .pages_loaded_o   (terr_pl_pages_loaded_o),
    .pages_faulted_o  (terr_pl_pages_faulted_o),
    .pages_refused_o  (terr_pl_pages_refused_o),
    .crc_fails_o      (terr_pl_crc_fails_o),
    .hdr_ident_fails_o(terr_pl_hdr_ident_fails_o),
    .incomplete_o     (terr_pl_incomplete_o),
    .guard_denied_o   (terr_pl_guard_denied_o),
    .bridge_errs_o    (terr_pl_bridge_errs_o),
    .load_bytes_o     (terr_pl_load_bytes_o)
  );

  // ==========================================================================
  // TWOD -- THE PLANE, THE SAMPLER AND THE SPRITE WALKER.  Added 2026-09-19,
  // and this closes the atmosphere half of header entry I17.
  // ==========================================================================
  // THE ARRANGEMENT IS A LOOP AND THE LOOP IS THE POINT. TWOD.PLANE is a pure
  // function of (x, y) and has no raster counter of its own -- its header says
  // "the compositor owns the walk". TWOD.SAMPLER owns it, because the sampler
  // is the only block that knows which line of its ring is free. So:
  //
  //     u_twod_sampler.pw_*  -->  u_twod_plane.p_*      (the walk)
  //     u_twod_plane.s_*     -->  u_twod_sampler.pl_*   (the texel request)
  //     u_twod_sampler.atm_* -->  u_post_composite      (the colour)
  //
  // NOTHING IS ADAPTED HERE. Every connection below is a port to a port of the
  // same width and the same meaning; there is no widening, no field invented,
  // no delay reconstructed and no arithmetic. That is the test entry I17's own
  // "hidden adapter" refusal sets, and the reason it can be met is that the
  // sampler was built to TWOD.PLANE's published request shape and to
  // POST.COMPOSITE's published `atm_*` convention rather than to a convenient
  // middle.
  //
  // THE SPRITE IS COMPOSED TO THE SAMPLER AND NOT TO THE COMPOSITOR, and that
  // is deliberate -- see entry I17 and the sampler's own header. Its colours
  // leave on `twod_sc_*`.
  // The walk, the plane's texel request and the sprite's texel request.
  logic                pw_valid_c, pw_ready_c, pw_slot_c;
  logic [15:0]         pw_x_c, pw_y_c;
  logic signed [31:0]  pw_scroll_c;

  logic                pl_valid_c, pl_ready_c, pl_fmt_c;
  logic [15:0]         pl_u_c, pl_v_c;
  logic [7:0]          pl_pal_c, pl_opacity_c;
  logic [1:0]          pl_blend_c, pl_role_c;

  logic                sp_valid_c, sp_ready_c, sp_last_c;
  logic signed [15:0]  sp_x_c, sp_y_c;
  logic signed [31:0]  sp_u_c, sp_v_c;
  logic [2:0]          sp_fmt_c;
  logic [7:0]          sp_pal_c, sp_order_c;
  logic [15:0]         sp_tint_c, sp_srcid_c;
  logic [1:0]          sp_blend_c;

  zhao_twod_plane #(
    .CW (32)
  ) u_twod_plane (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // The descriptor is the CMD seam, unchanged by this packet.
    .d_valid_i    (twod_pd_valid_i),
    .d_ready_o    (twod_pd_ready_o),
    .d_slot_i     (twod_pd_slot_i),
    .d_role_i     (twod_pd_role_i),
    .d_blend_i    (twod_pd_blend_i),
    .d_opacity_i  (twod_pd_opacity_i),
    .d_format_i   (twod_pd_format_i),
    .d_width_i    (twod_pd_width_i),
    .d_height_i   (twod_pd_height_i),
    .d_wrap_u_i   (twod_pd_wrap_u_i),
    .d_wrap_v_i   (twod_pd_wrap_v_i),
    .d_a_i        (twod_pd_a_i),
    .d_b_i        (twod_pd_b_i),
    .d_c_i        (twod_pd_c_i),
    .d_d_i        (twod_pd_d_i),
    .d_u0_i       (twod_pd_u0_i),
    .d_v0_i       (twod_pd_v0_i),
    .d_view_mask_i(twod_pd_view_mask_i),
    .d_palette_i  (twod_pd_palette_i),

    // REAL: the walk comes from the sampler, which is the block that knows
    // which ring line is free.
    .p_valid_i      (pw_valid_c),
    .p_ready_o      (pw_ready_c),
    .p_slot_i       (pw_slot_c),
    .p_x_i          (pw_x_c),
    .p_y_i          (pw_y_c),
    .p_line_scroll_i(pw_scroll_c),
    // view_sel_i is a 2-bit MASK here against POST.COMPOSITE's 1-bit view
    // INDEX. One-hot of the index is the mask, and writing it as a one-hot
    // rather than a zero-extension is the whole difference between "view 1"
    // and "view 0 and 1".
    .view_sel_i     (post_view_sel_i ? 2'b10 : 2'b01),

    .s_valid_o  (pl_valid_c),
    .s_ready_i  (pl_ready_c),
    .s_texel_u_o(pl_u_c),
    .s_texel_v_o(pl_v_c),
    .s_format_o (pl_fmt_c),
    .s_palette_o(pl_pal_c),
    .s_blend_o  (pl_blend_c),
    .s_opacity_o(pl_opacity_c),
    .s_role_o   (pl_role_c),

    .pixels_o        (twod_plane_pixels_o),
    .refused_role_o  (twod_plane_refused_role_o),
    .refused_blend_o (twod_plane_refused_blend_o),
    .skipped_view_o  (twod_plane_skipped_view_o),
    .wrap_fail_o     (twod_plane_wrap_fail_o)
  );

  zhao_twod_sprite #(
    .UVW (32)
  ) u_twod_sprite (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    .d_valid_i    (twod_sd_valid_i),
    .d_ready_o    (twod_sd_ready_o),
    .d_x_i        (twod_sd_x_i),
    .d_y_i        (twod_sd_y_i),
    .d_w_i        (twod_sd_w_i),
    .d_h_i        (twod_sd_h_i),
    .d_u_i        (twod_sd_u_i),
    .d_v_i        (twod_sd_v_i),
    .d_a00_i      (twod_sd_a00_i),
    .d_a01_i      (twod_sd_a01_i),
    .d_a10_i      (twod_sd_a10_i),
    .d_a11_i      (twod_sd_a11_i),
    .d_format_i   (twod_sd_format_i),
    .d_palette_i  (twod_sd_palette_i),
    .d_tint_i     (twod_sd_tint_i),
    .d_blend_i    (twod_sd_blend_i),
    .d_view_mask_i(twod_sd_view_mask_i),
    .d_order_i    (twod_sd_order_i),
    .d_src_id_i   (twod_sd_src_id_i),
    .view_sel_i   (post_view_sel_i ? 2'b10 : 2'b01),

    // REAL: the sample requests go to the sampler.
    .s_valid_o (sp_valid_c),
    .s_ready_i (sp_ready_c),
    .s_x_o     (sp_x_c),
    .s_y_o     (sp_y_c),
    .s_u_o     (sp_u_c),
    .s_v_o     (sp_v_c),
    .s_format_o(sp_fmt_c),
    .s_palette_o(sp_pal_c),
    .s_tint_o  (sp_tint_c),
    .s_blend_o (sp_blend_c),
    .s_order_o (sp_order_c),
    .s_src_id_o(sp_srcid_c),
    .s_last_o  (sp_last_c),

    .descriptors_o (twod_sprite_descriptors_o),
    .skipped_view_o(twod_sprite_skipped_view_o),
    .refused_o     (twod_sprite_refused_o),
    .pixels_o      (twod_sprite_pixels_o)
  );

  zhao_twod_sampler #(
    .LINE_W     (POST_LINE_W),
    .MAX_H      (POST_MAX_H),
    .PAGE_WORDS (TWOD_PAGE_WORDS),
    .PAL_SLOTS  (TWOD_PAL_SLOTS),
    .BIND_SLOTS (TWOD_BIND_SLOTS),
    .ATM_LINES  (TWOD_ATM_LINES)
  ) u_twod_sampler (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: the same frame edge and the same latched mode POST.COMPOSITE runs
    // on. Giving the sampler a second opinion about where a frame starts or
    // how wide a line is would put the ring and the compositor's raster out of
    // step by exactly the disagreement.
    .frame_start_i(core_tick_c),
    .frame_w_i    (post_frame_w_c),
    .frame_h_i    (post_frame_h_c),

    // The page, the palette and the bindings are ASSETS -- I17's own
    // classification for the grading curves, applied to a texture.
    .ld_page_we_i     (twod_ld_page_we_i),
    .ld_page_addr_i   (twod_ld_page_addr_i),
    .ld_page_data_i   (twod_ld_page_data_i),
    .ld_pal_we_i      (twod_ld_pal_we_i),
    .ld_pal_addr_i    (twod_ld_pal_addr_i),
    .ld_pal_data_i    (twod_ld_pal_data_i),
    .ld_bind_we_i     (twod_ld_bind_we_i),
    .ld_bind_sel_i    (twod_ld_bind_sel_i),
    .ld_bind_base_i   (twod_ld_bind_base_i),
    .ld_bind_lstride_i(twod_ld_bind_lstride_i),
    .ld_bind_lheight_i(twod_ld_bind_lheight_i),

    .atm_slot_i   (twod_atm_slot_i),
    .line_scroll_i(twod_line_scroll_i),

    .pw_valid_o      (pw_valid_c),
    .pw_ready_i      (pw_ready_c),
    .pw_slot_o       (pw_slot_c),
    .pw_x_o          (pw_x_c),
    .pw_y_o          (pw_y_c),
    .pw_line_scroll_o(pw_scroll_c),

    .pl_valid_i  (pl_valid_c),
    .pl_ready_o  (pl_ready_c),
    .pl_texel_u_i(pl_u_c),
    .pl_texel_v_i(pl_v_c),
    .pl_format_i (pl_fmt_c),
    .pl_palette_i(pl_pal_c),
    .pl_blend_i  (pl_blend_c),
    .pl_opacity_i(pl_opacity_c),
    .pl_role_i   (pl_role_c),

    .sp_valid_i (sp_valid_c),
    .sp_ready_o (sp_ready_c),
    .sp_x_i     (sp_x_c),
    .sp_y_i     (sp_y_c),
    .sp_u_i     (sp_u_c),
    .sp_v_i     (sp_v_c),
    .sp_format_i(sp_fmt_c),
    .sp_palette_i(sp_pal_c),
    .sp_tint_i  (sp_tint_c),
    .sp_blend_i (sp_blend_c),
    .sp_order_i (sp_order_c),
    .sp_src_id_i(sp_srcid_c),
    .sp_last_i  (sp_last_c),

    .sc_valid_o (twod_sc_valid_o),
    .sc_ready_i (twod_sc_ready_i),
    .sc_rgb_o   (twod_sc_rgb_o),
    .sc_x_o     (twod_sc_x_o),
    .sc_y_o     (twod_sc_y_o),
    .sc_tint_o  (twod_sc_tint_o),
    .sc_blend_o (twod_sc_blend_o),
    .sc_order_o (twod_sc_order_o),
    .sc_src_id_o(twod_sc_src_id_o),
    .sc_last_o  (twod_sc_last_o),

    // REAL: POST.COMPOSITE's atmosphere seam, closed.
    .atm_req_v_i (atm_req_v_c),
    .atm_req_x_i (atm_req_x_c),
    .atm_req_y_i (atm_req_y_c),
    .atm_en_o    (atm_en_c),
    .atm_valid_o (atm_valid_c),
    .atm_rgb_o   (atm_rgb_c),
    .atm_opacity_o(atm_opacity_c),
    .atm_add_o   (atm_add_c),

    .samples_o        (twod_samples_o),
    .plane_samples_o  (twod_plane_samples_o),
    .sprite_samples_o (twod_sprite_samples_o),
    .clut8_samples_o  (twod_clut8_samples_o),
    .rgb565_samples_o (twod_rgb565_samples_o),
    .texel_wrapped_o  (twod_texel_wrapped_o),
    .page_oob_o       (twod_page_oob_o),
    .bind_missing_o   (twod_bind_missing_o),
    .fmt_refused_o    (twod_fmt_refused_o),
    .pal_refused_o    (twod_pal_refused_o),
    .skipped_fill_o   (twod_skipped_fill_o),
    .atm_underrun_o   (twod_atm_underrun_o),
    .walk_stalls_o    (twod_walk_stalls_o),
    .sprite_stalls_o  (twod_sprite_stalls_o),
    .tint_unapplied_o (twod_tint_unapplied_o),
    .pair_lost_o      (twod_pair_lost_o)
  );

  // ==========================================================================
  // THE PARTICLE DRAW PATH.  ENTRY I24's PRODUCER HALF, CLOSED.
  //
  //   PART.COLLIDE --(fork branch P)--> PART.PROJECT --> PART.LADDER
  //                                          ^  |            |
  //                                          |  |            v
  //                     client A of the ONE  |  +--- rung ---+
  //                     shared projector <---+                |
  //                                                           v
  //                                        rung write-back + SHARD -> EXPAND
  //                                                          SPRITE -> SOFT
  //
  // FOUR BLOCKS, NO ARITHMETIC IN THIS FILE. Every connection below is one net
  // to one net; the only expressions are the rung comparison, which is the
  // ledger's declared PART.LADDER -> {EXPAND, SOFT} edge implemented as the
  // ROUTING it actually is, and the fork that lets the rung write-back and an
  // endpoint take the same beat.
  //
  // WHY THE ENDPOINTS ARE SAFE TO ADOPT UNCHANGED is the whole argument of the
  // I24 entry above and is not repeated here; the one-line version is that
  // PART.PROJECT hands them a SCREEN size computed by the ratified law in
  // `zref::render::draw_form_marker`, which is the input their headers were
  // always written against.
  // ==========================================================================

  // The two rungs that have an endpoint built in this tree, named rather than
  // written as literals for the reason GEOM_VERTEX_FORMAT_C is (entry I25):
  // the day a meshlet, ribbon or glint endpoint exists, the thing that has to
  // change is greppable instead of being a `3'd1` in a comparison. The values
  // are `zhao_part_ladder`'s own RUNG_SHARD and RUNG_SPRITE.
  localparam logic [2:0] PART_RUNG_SHARD_C  = 3'd1;
  localparam logic [2:0] PART_RUNG_SPRITE_C = 3'd3;

  // In-flight particles at the shared projector. A FRONTIER KNOB and the block
  // prices it: 8 caps the particle stream at 8/36 of a projection per clock
  // (about 17.7% of the frame at the required tier of 32,768 across two views),
  // 40 would cost 3.9% and several thousand ALM of slot store. Named here so
  // the console keeps the choice after the composed fit measures it.
  localparam int unsigned PART_PROJ_SLOTS_C = 8;

  wire        pp_lad_valid, pp_lad_ready;
  wire [15:0] pp_lad_size, pp_lad_trail;
  wire        pp_lad_narrow, pp_lad_protected, pp_lad_first;
  wire [ 2:0] pp_lad_gov, pp_lad_prev;
  wire [ 3:0] pp_lad_hold;

  wire        pl_r_valid, pl_r_ready;
  wire [ 2:0] pl_r_rung;
  wire [ 3:0] pl_r_hold;
  wire        pl_r_changed;

  wire               pq_valid, pq_ready;
  wire               pq_in;
  wire signed [20:0] pq_x, pq_y;
  wire signed [31:0] pq_d;
  wire        [ 7:0] pq_size;
  wire        [ 7:0] pq_r, pq_g, pq_b;
  wire        [15:0] pq_src_id;
  wire        [ 2:0] pq_rung;
  wire        [ 3:0] pq_hold;
  wire               pq_changed;

  // The U 8.8 form of the same half-extent. PART.LADDER has already consumed it
  // on `pp_lad_size` and neither endpoint takes that width, so it is named and
  // left unread rather than connected by an empty port -- the reason is in the
  // source instead of in a silence, exactly as zhao_geom_project does it.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [15:0] pq_size16;
  /* verilator lint_on UNUSEDSIGNAL */

  wire pe_p_ready, pf_p_ready;

  zhao_part_project #(
    .REC_W    (PART_REC_W),
    .PAY_W    (GEOM_PAY_A_W),
    .SLOTS    (PART_PROJ_SLOTS_C)
    // LAD_D and POS_SHIFT stay at the block's own defaults. LAD_D=2 is already
    // slack against a one-deep PART.LADDER, and POS_SHIFT=8 is the unruled
    // Class-C position scale -- a console-level opinion about it would be a
    // second opinion, and the block's header is where that knob is explained.
  ) u_part_project (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // I24: the owner values. The base radius is the piece zref_particle.hpp
    // names as missing and as a content decision; the view is which camera this
    // pass is for, because ladder selection is PER CAMERA.
    .cfg_base_radius_i(part_prj_base_radius_i),
    .cfg_view_i       (part_prj_view_i),

    // REAL: branch P of the fork on PART.COLLIDE's output. The draw pass reads
    // the ring's OWN records; nothing here mints a second record stream.
    .p_valid_i (fork_prj_valid_c),
    .p_ready_o (pp_p_ready),
    .p_record_i(pc_c_record),

    // I24: the ladder's species/governor inputs, the per-(particle, camera)
    // hold state (I23's absent DDR) and the particle's colour. They ride
    // PART.PROJECT's slot store with their particle, which is the whole reason
    // they enter here rather than being joined to the ladder by hand.
    .p_trail_i    (part_prj_trail_i),
    .p_narrow_i   (part_prj_narrow_i),
    .p_protected_i(part_prj_protected_i),
    .p_gov_floor_i(part_prj_gov_floor_i),
    .p_prev_rung_i(part_prj_prev_rung_i),
    .p_hold_i     (part_prj_hold_i),
    .p_first_i    (part_prj_first_i),
    .p_r_i        (part_prj_r_i),
    .p_g_i        (part_prj_g_i),
    .p_b_i        (part_prj_b_i),
    .p_src_id_i   (part_prj_src_id_i),

    // REAL: geometry in, from GEOM.GROUP_SEQ, and straight out again.
    .g_valid_i  (gs_a_valid),
    .g_ready_o  (gs_a_ready),
    .g_vx_i     (gs_a_vx),
    .g_vy_i     (gs_a_vy),
    .g_vz_i     (gs_a_vz),
    .g_view_i   (gs_a_view),
    .g_payload_i(gs_a_payload),

    // REAL: the multiplexed request into client A of the ONE projector.
    .a_valid_o  (pa_a_valid),
    .a_ready_i  (pa_a_ready),
    .a_vx_o     (pa_a_vx),
    .a_vy_o     (pa_a_vy),
    .a_vz_o     (pa_a_vz),
    .a_view_o   (pa_a_view),
    .a_payload_o(pa_a_payload),

    // REAL: client A's result, demultiplexed by the rider's top bit.
    .a_valid_i  (sv_a_valid),
    .a_x_i      (sv_a_x),
    .a_y_i      (sv_a_y),
    .a_d_i      (sv_a_d),
    .a_w_i      (sv_a_w),
    .a_behind_i (sv_a_behind),
    .a_payload_i(sv_a_payload),

    // REAL: geometry's half of that result, into the arena lane, untouched.
    .h_valid_o  (pj_a_valid),
    .h_x_o      (pj_a_x),
    .h_y_o      (pj_a_y),
    .h_d_o      (pj_a_d),
    .h_w_o      (pj_a_w),
    .h_behind_o (pj_a_behind),
    .h_payload_o(pj_a_payload),

    // REAL: the ladder loop, both directions.
    .lad_valid_o    (pp_lad_valid),
    .lad_ready_i    (pp_lad_ready),
    .lad_size_o     (pp_lad_size),
    .lad_trail_o    (pp_lad_trail),
    .lad_narrow_o   (pp_lad_narrow),
    .lad_protected_o(pp_lad_protected),
    .lad_gov_floor_o(pp_lad_gov),
    .lad_prev_rung_o(pp_lad_prev),
    .lad_hold_o     (pp_lad_hold),
    .lad_first_o    (pp_lad_first),
    .rng_valid_i    (pl_r_valid),
    .rng_ready_o    (pl_r_ready),
    .rng_rung_i     (pl_r_rung),
    .rng_hold_i     (pl_r_hold),
    .rng_changed_i  (pl_r_changed),

    // REAL: the projected particle, with its verdict attached.
    .q_valid_o   (pq_valid),
    .q_ready_i   (pq_ready),
    .q_in_o      (pq_in),
    .q_x_o       (pq_x),
    .q_y_o       (pq_y),
    .q_d_o       (pq_d),
    .q_size_o    (pq_size),
    .q_size16_o  (pq_size16),
    .q_r_o       (pq_r),
    .q_g_o       (pq_g),
    .q_b_o       (pq_b),
    .q_src_id_o  (pq_src_id),
    .q_rung_o    (pq_rung),
    .q_hold_new_o(pq_hold),
    .q_changed_o (pq_changed),

    .particles_projected_o(part_prj_projected_o),
    .particles_behind_o   (part_prj_behind_o),
    .geom_grants_o        (part_prj_geom_grants_o),
    .part_grants_o        (part_prj_part_grants_o),
    .contended_o          (part_prj_contended_o),
    .size_saturations_o   (part_prj_size_sat_o),
    .slot_pressure_o      (part_prj_slot_pressure_o),
    .geom_tag_collision_o (part_prj_tag_collision_o),
    .ladder_unexpected_o  (part_prj_ladder_unexpected_o)
  );

  zhao_part_ladder u_part_ladder (
    // Every threshold stays at the block's own default. They are Class B --
    // evidence-driven defaults, not ABI -- and a console-level override here
    // would be a second opinion the owner never took.
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: PART.PROJECT's ladder loop. `p_size_i` is a PROJECTED U 8.8 screen
    // size, which is what this block's port comment asks for and what I24 said
    // had no producer.
    .v_valid_i    (pp_lad_valid),
    .v_ready_o    (pp_lad_ready),
    .p_size_i     (pp_lad_size),
    .p_trail_i    (pp_lad_trail),
    .p_narrow_i   (pp_lad_narrow),
    .p_protected_i(pp_lad_protected),
    .p_gov_floor_i(pp_lad_gov),
    .p_prev_rung_i(pp_lad_prev),
    .p_hold_i     (pp_lad_hold),
    .p_first_i    (pp_lad_first),

    .r_valid_o  (pl_r_valid),
    .r_ready_i  (pl_r_ready),
    .r_rung_o   (pl_r_rung),
    .r_hold_o   (pl_r_hold),
    .r_changed_o(pl_r_changed),

    .decisions_o  (part_lad_decisions_o),
    .changes_o    (part_lad_changes_o),
    .held_o       (part_lad_held_o),
    .gov_forced_o (part_lad_gov_forced_o)
  );

  // --------------------------------------------------------------------------
  // GLUE 6: THE RUNG DEMUX, AND THE FORK THAT LETS THE WRITE-BACK SEE EVERY
  // BEAT.
  //
  // `design/blocks.yml` declares PART.LADDER -> {PART.EXPAND, PART.SOFT} and
  // neither endpoint has a port that takes a rung, which entry I24 records as
  // the ledger's edge not being the RTL's. It is not a missing port: it is a
  // ROUTING relation, and this is it -- stateless, two named rungs, no counter
  // and no register.
  //
  // The rung write-back must see EVERY beat, including the four rungs that have
  // no endpoint built. So it is an AND-fork: the beat retires when the
  // write-back and the selected endpoint have both accepted, and a rung with no
  // endpoint has a ready of constant 1. This is the plain fork glue 3's comment
  // calls out as stalling both consumers when either is busy; that cost is
  // accepted here because there are exactly two consumers, only one of which
  // ever takes a given beat, so the "presented twice" hazard the done-bit fork
  // exists to solve cannot arise.
  //
  // NO CONSUMER'S READY READS ITS OWN VALID, and that is the property to check
  // rather than assume: `zhao_part_expand.p_ready_o` and
  // `zhao_part_soft.p_ready_o` are both `!out_valid || out_ready`, functions of
  // their own output register and a boundary input, and `part_rung_ready_i` is
  // a boundary input. So nothing below closes a combinational loop.
  // --------------------------------------------------------------------------
  wire pq_to_exp_c = pq_valid && (pq_rung == PART_RUNG_SHARD_C);
  wire pq_to_sft_c = pq_valid && (pq_rung == PART_RUNG_SPRITE_C);
  wire pq_ep_ready_c = pq_to_exp_c ? pe_p_ready : (pq_to_sft_c ? pf_p_ready : 1'b1);

  assign pq_ready = part_rung_ready_i && pq_ep_ready_c;

  assign part_rung_valid_o   = pq_valid && pq_ep_ready_c;
  assign part_rung_o         = pq_rung;
  assign part_rung_hold_o    = pq_hold;
  assign part_rung_changed_o = pq_changed;
  assign part_rung_src_id_o  = pq_src_id;

  wire pe_p_valid_c = pq_to_exp_c && part_rung_ready_i;
  wire pf_p_valid_c = pq_to_sft_c && part_rung_ready_i;

  zhao_part_expand u_part_expand (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: the SHARD rung of PART.LADDER's verdict, carrying PART.PROJECT's
    // projected particle. `p_size_i` is now a SCREEN size in U 0.4.4 pixels,
    // which is what this block's `size << 4` was always written for.
    .p_valid_i (pe_p_valid_c),
    .p_ready_o (pe_p_ready),
    .p_in_i    (pq_in),
    .p_x_i     (pq_x),
    .p_y_i     (pq_y),
    .p_d_i     (pq_d),
    .p_size_i  (pq_size),
    .p_r_i     (pq_r),
    .p_g_i     (pq_g),
    .p_b_i     (pq_b),
    .p_src_id_i(pq_src_id),

    // I24: the triangle's customer is the same absent GEOM replay/setup path
    // I11, I12 and I13 name, so the packet leaves the module.
    .t_valid_o      (part_exp_valid_o),
    .t_ready_i      (part_exp_ready_i),
    .t_ax_o         (part_exp_ax_o),
    .t_ay_o         (part_exp_ay_o),
    .t_bx_o         (part_exp_bx_o),
    .t_by_o         (part_exp_by_o),
    .t_cx_o         (part_exp_cx_o),
    .t_cy_o         (part_exp_cy_o),
    .t_d_o          (part_exp_d_o),
    .t_r_o          (part_exp_r_o),
    .t_g_o          (part_exp_g_o),
    .t_b_o          (part_exp_b_o),
    .t_depth_test_o (part_exp_depth_test_o),
    .t_depth_write_o(part_exp_depth_write_o),
    .t_src_id_o     (part_exp_src_id_o),

    .polygon_particles_o(part_exp_polygons_o)
  );

  zhao_part_soft u_part_soft (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: the scissor is the console's own pass geometry -- the SAME nets
    // GEOM.CLIP takes (GLUE 1 extended). Not a boundary and not a second
    // opinion: `zhao_part_soft` defines the rectangle exactly as
    // `zhao_geom_clip` does.
    .vp_x0_i(12'd0),
    .vp_y0_i(12'd0),
    .vp_w_i (clip_vp_w_c),
    .vp_h_i (clip_vp_h_c),

    // REAL: the SPRITE rung.
    .p_valid_i (pf_p_valid_c),
    .p_ready_o (pf_p_ready),
    .p_in_i    (pq_in),
    .p_x_i     (pq_x),
    .p_y_i     (pq_y),
    .p_d_i     (pq_d),
    .p_size_i  (pq_size),
    .p_r_i     (pq_r),
    .p_g_i     (pq_g),
    .p_b_i     (pq_b),
    .p_src_id_i(pq_src_id),

    // I24: the span's customer is RASTER.FRAGMENT, which lives in the shell.
    .s_valid_o      (part_sft_valid_o),
    .s_ready_i      (part_sft_ready_i),
    .s_min_x_o      (part_sft_min_x_o),
    .s_max_x_o      (part_sft_max_x_o),
    .s_min_y_o      (part_sft_min_y_o),
    .s_max_y_o      (part_sft_max_y_o),
    .s_d_o          (part_sft_d_o),
    .s_r_o          (part_sft_r_o),
    .s_g_o          (part_sft_g_o),
    .s_b_o          (part_sft_b_o),
    .s_depth_test_o (part_sft_depth_test_o),
    .s_depth_write_o(part_sft_depth_write_o),
    .s_src_id_o     (part_sft_src_id_o),

    .soft_particles_o(part_sft_sprites_o)
  );

  // ==========================================================================
  // 7c. CMD.EXEC -- the packet becomes CONSOLE STATE
  // ==========================================================================
  // COMPOSED 2026-09-19. Section 7b's own text said what was still missing and
  // this is it: "composing this does NOT by itself close I14, I30, I33 or I7 --
  // those need the command EXECUTOR that spec/commands.zidl:382 says does not
  // exist yet". The executor exists. It is `fpga/rtl/command/zhao_cmd_exec.sv`
  // and its header is the authority on everything below.
  //
  // It sits on the SAME forked byte stream as the decoder, and it takes the
  // decoder's verdict as an input rather than re-deriving it. The division is
  // one sentence: CMD.DECODER owns the VERDICT, CMD.EXEC owns the PAYLOAD.
  //
  // NOTHING THIS BLOCK WRITES LEAVES IT BEFORE THE VERDICT, and that is
  // structural rather than argued: `pkt_ready_o` is low outside its staging
  // state, so the commit drain and the byte walk cannot overlap. The decoder's
  // contract (its lines 70-75) is therefore obeyed literally. Its header
  // carries the arithmetic that made literal compliance affordable -- staging
  // the EFFECT of a command is 2,688 bits, staging the PACKET would be 8 Mbit.
  //
  // TWO OF THE FOUR GAPS ARE NOT CLOSED HERE AND MUST NOT BE. I33 (PART.TABLE's
  // per-frame load) and I7 (PART.COLLIDE's plane) have NO command in
  // `spec/commands.zidl`. Wiring either would mean this file choosing a wire
  // layout the ABI does not define, which both entries already forbid in their
  // own words. They need an ABI ruling, not an executor arm.
  //
  // And the two that ARE closed are closed by HALVES, deliberately: I14 keeps
  // the viewport rect and I30 keeps the patch envelope, because no ratified
  // command carries either. The merges above say which half is which.
  zhao_cmd_exec #(
    .STAMP_Q (CMD_EXEC_STAMP_Q)
  ) u_cmd_exec (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    .pkt_valid_i     (cmd_pkt_valid_w),
    .pkt_ready_o     (cmd_exe_pkt_ready_w),
    .pkt_fork_ready_i(cmd_pkt_ready_w),
    .pkt_byte_i      (cmd_pkt_byte_w),
    .pkt_len_i       (cmd_pkt_len_w),

    // The verdict, from section 7b. Not re-derived.
    .verdict_valid_i (cmd_decode_done_o),
    .verdict_error_i (cmd_decode_error_o),

    // I14's closing half: SetView's mat4fx -> cfg addresses 0..15.
    .proj_cfg_we_o   (cmd_exec_cfg_we_w),
    .proj_cfg_ready_i(cmd_exec_cfg_ready_w),
    .proj_cfg_view_o (cmd_exec_cfg_view_w),
    .proj_cfg_addr_o (cmd_exec_cfg_addr_w),
    .proj_cfg_data_o (cmd_exec_cfg_data_w),

    // I30's closing half: SurfaceStamp's ratified fields -> the dispatch.
    .stamp_valid_o     (cmd_exec_stamp_valid_w),
    .stamp_ready_i     (cmd_exec_stamp_ready_w),
    .stamp_patch_o     (cmd_exec_stamp_patch_w),
    .stamp_operation_o (cmd_exec_stamp_oper_w),
    .stamp_tag_o       (cmd_exec_stamp_tag_w),
    .stamp_strength_o  (cmd_exec_stamp_strength_w),
    .stamp_tx_o        (cmd_exec_stamp_tx_w),
    .stamp_ty_o        (cmd_exec_stamp_ty_w),
    .stamp_radius_o    (cmd_exec_stamp_radius_w),
    .stamp_ring_width_o(cmd_exec_stamp_ring_w),
    .stamp_src_id_o    (cmd_exec_stamp_src_id_w),

    .packets_committed_o  (cmd_exec_committed_o),
    .packets_abandoned_o  (cmd_exec_abandoned_o),
    .views_written_o      (cmd_exec_views_o),
    .stamps_issued_o      (cmd_exec_stamps_o),
    .stamp_overflow_o     (cmd_exec_stamp_overflow_o),
    .view_range_refused_o (cmd_exec_view_refused_o),
    .stamp_src_truncated_o(cmd_exec_src_truncated_o),
    .unsupported_o        (cmd_exec_unsupported_o)
  );

endmodule : zhao_console_core
