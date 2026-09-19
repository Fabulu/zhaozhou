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
//      CORRECTED 2026-09-19: "port for port" was TWENTY OF TWENTY-ONE. The
//      packet's twenty-first field is `out_area2_o`, and it went to
//      `geom_setup_area2_o` -- out of the module, as evidence -- while the
//      door's `tri_area2_i` stayed a boundary port that nothing in this tree
//      drives. It is wired now (see `st_area2`), and the omission was not
//      cosmetic: `zhao_raster_tile_pipe_v2` reads `tri_area2_i == 47'd0` as
//      PROFILE AREA BAD, so the FIRST job of every frame raised
//      `range_fault_event_w`, latched `local_abort_q` and made the tile pipe
//      SINK the rest. 72 jobs taken, 72 sunk, 0 started, 0 pixels -- a
//      reading indistinguishable from a binner that was never fed.
//
//      The lesson is the entry itself: a composition entry that says CLOSED
//      is a claim about a LIST, and nobody had counted the list.
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
//      THE SECOND COMPLETION IS CLOSED, 2026-09-19, and this paragraph is kept
//      with its correction rather than deleted, because it is the clearest
//      statement in the file of what was wrong.
//
//      IT USED TO SAY: "a page it loads reaches the directory's ST_MIPGEN
//      state and stops there. TERRAIN.RESIDENCY publishes on TWO completions
//      -- a claim sets `mips_stale` and only a second `fin` reaches
//      RESIDENT_CLEAN, the only state a lookup hits on. The second
//      completion's producer is TERRAIN.MIPFEED, which is not composed. So
//      `terr_res_resident_o` staying at zero while `terr_pl_pages_loaded_o`
//      climbs is the EXPECTED reading of this composition, not a defect in
//      it."
//
//      EVERY WORD OF THAT WAS TRUE and the conclusion drawn from it was the
//      dangerous one: a spine that loads pages and can never call one ground
//      is not a spine with a documented limitation, it is a spine that does
//      nothing, with a paragraph explaining why that is fine. The check that
//      separated the two took one grep -- `zhao_terrain_residency_v2.sv`'s
//      claim arm writes `s_pack('0, 1'b0, victim_dirty_c, 1'b1, ...)`, whose
//      fourth argument is `mips`, so EVERY claim is mips-stale and the state
//      was not an edge case but the only path.
//
//      It is composition item 12 now. TERRAIN.MIPFEED and TERRAIN.MIPGEN are
//      composed, the request has an owner (`zhao_terrain_mipreq`), and the
//      streamer is SHARED rather than duplicated (`zhao_terrain_psmux`).
//      `terr_res_resident_o` is no longer structurally zero, which means it is
//      a measurement again rather than a foregone conclusion.
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
//  10. THE TERRAIN COMPOSE ENGINE -- a page becomes a lattice
//        TERRAIN.SEQ.is_*          -> TERRAIN.PAGESTREAM.j_*
//                                  -> TERRAIN.PLACE.hdr_*
//        TERRAIN.PAGESTREAM.v_*    -> TERRAIN.PATCH.vtx_*
//        TERRAIN.PLACE.vtx_w{x,z}  -> TERRAIN.PATCH.w{x,z}_i
//        TERRAIN.PLACE.pos_*       -> TERRAIN.COMPCACHE.pos_*
//        TERRAIN.PATCH.st_*        -> TERRAIN.COMPCACHE.st_*
//        TERRAIN.COMPCACHE.lat_/cs_-> TERRAIN.TESS
//        TERRAIN.PAGESTREAM.done_* -> TERRAIN.RESIDENCY.unpin_*
//      Four blocks joined to the paging spine (item 8) and to the tessellator
//      (item 3b), so the terrain path now runs end to end inside this module:
//      a command list becomes page loads, a loaded page becomes a composed
//      lattice, and the tessellator reads that lattice instead of a harness.
//
//      THE BLOCKER WAS PLACEMENT AND IT IS NAMED NOW. Entry I27 refused this
//      chain because "TERRAIN.PAGESTREAM emits the heights and the lattice
//      indices and NOT the placement; nothing else in the tree emits it
//      either", and said flatly that deriving it in the composer was not
//      allowed. `zhao_terrain_place` is the owner that removes the refusal:
//      spec/terrain_rules.md 1.3/2.1 frozen into one block, with the patch
//      header's redundant envelope CHECKED against its own shifter rather than
//      trusted or ignored. This file wires two ports and computes nothing.
//
//      THE UNPIN IS REAL AND IT IS THE OTHER HALF OF T10. TERRAIN.SEQ pins a
//      page before issuing and its port comment says the unpin is "the
//      engine's, on job completion". The engine exists, so `terr_unpin_*` left
//      the port list rather than being driven by a harness.
//
//      WHAT THIS CHAIN STILL CANNOT DO, said here so nobody reads more into it
//      than it claims: the FIELD half of section 3.4 is absent (entry I34), so
//      `live_top` collapses to `compose_top`; and layer D is never written
//      (entry I32), so every cell reads SOLID. Each is a port and an entry, and
//      neither is a constant standing in for a producer.
//
//      THE THIRD ITEM ON THIS LIST IS GONE. It read "the placement's pitch and
//      envelope arrive at this module's edge (entry I35)", and on 2026-09-19
//      `zhao_terrain_hdrread` was built as the header reader that entry asked
//      for. The pitch, the envelope and the patch coordinate now come off the
//      page's own 64 bytes on the compose path. I35 is CLOSED and DELETED, and
//      composed item 13 below is the connection.
//
//  11. THE GEOMETRY ASSET PATH -- five blocks, ONE memory client, and the
//      first time this console reads a mesh out of memory.
//        GEOM.MESHFETCH.guard  -> GEOM.MEM_ADAPTER requester A
//        GEOM.ASSETFETCH.guard -> GEOM.MEM_ADAPTER requester B
//        GEOM.MEM_ADAPTER.m_*  -> the shell's ONE geometry MEM.GUARD socket
//        GEOM.MESHFETCH.cull_* <-> GEOM.CULL
//        GEOM.MESHFETCH.r_*    -> GEOM.ASSETFETCH.m_* (the meshlet record)
//        GEOM.ASSETFETCH.v_*   -> GEOM.VDECODE.v_*    (entry I23, CLOSED)
//        GEOM.ASSETFETCH.s_*   -> GEOM.ASSEMBLE.m_*
//        GEOM.ASSEMBLE.ix_*    <-> GEOM.ASSETFETCH.ix_* (the index service)
//      Every seam is name for name and width for width. Nothing is renamed
//      and nothing is computed between them, with ONE value assigned by this
//      file, declared at entry I40.
//
//      SHARING BEAT BUILDING, AND THE BLOCK THAT DOES IT ALREADY EXISTED.
//      Two fetchers want memory and `zhao_vram_arbiter` builds the
//      controller's client tag by CASTING THE SLOT INDEX, so a client is
//      POSITIONAL; `zhao_mem_guard` grants the render asset pool to ENGINE1
//      alone and everything else falls to `default: pass_ok = 1'b0`. A
//      second geometry client is therefore not a wire, it is a memory-rules
//      ruling -- the same wall entry I15 item 2 records for the compositor.
//      `zhao_geom_mem_adapter` is the owner's answer (recovery brief 12.1):
//      round-robin at LOGICAL REQUEST boundaries, one request in flight, the
//      client field forced to ENGINE1 and `write` forced low. So the console
//      gains a whole asset path and MEM.VRAM.ARBITER gains no client, and
//      `geom_ma_contention_o` is the number that says what the sharing cost.
//      That counter could not move before this composition: until now only
//      ONE requester was ever behind the adapter in any bench.
//
//      THE CULL IS SHARED TOO, AND IT SHARES THE MATRIX BANK RATHER THAN A
//      SECOND ONE. `zhao_geom_cull`'s own port comment says its sixteen
//      configuration words are "EXACTLY the words zhao_geom_project takes at
//      the same addresses", and it deliberately ignores addr >= 16 because
//      rejection happens in clip space and the viewport only maps NDC to
//      pixels afterwards. So it is wired to `proj_cfg_*_m` -- the SAME
//      merged bank CMD.EXEC's `SetView` already drives into the projector
//      (entry I14's closed half) -- and this file adds no second
//      configuration path and no second opinion about where a camera is.
//
//      WHAT THIS PATH DOES NOT YET DO, said here rather than left to be
//      discovered: the DRAW that starts it arrives at this module's edge
//      (I36), the descriptor's CRC verdict does (I37), and the meshlet
//      release does (I38) -- so the chain fetches ONE meshlet and then
//      holds, exactly as the terrain compose cache holds at I21 and for the
//      same kind of missing owner. That is the EXPECTED reading of this
//      composition, not a defect in it.
//
//  13. THE PATCH HEADER IS READ ON THE COMPOSE PATH -- entry I35's absent
//      owner, built and wired, and a THIRD memory reader that costs no
//      memory client.
//        TERRAIN.SEQ.is_*          -> TERRAIN.HDRREAD.j_*
//        TERRAIN.HDRREAD.h_*       -> TERRAIN.PLACE.hdr_*   (entry I35, CLOSED)
//        TERRAIN.HDRREAD.f_*       -> TERRAIN.PSMUX client A -> PAGESTREAM
//        TERRAIN.HDRREAD.guard_*   -> MEM.SHARE2 requester A
//        TERRAIN.PAGESTREAM.guard_* -> MEM.SHARE2 requester B
//        MEM.SHARE2.m_*            -> the one terrain MEM.GUARD read client
//
//      WHAT WAS ACTUALLY MISSING. Entry I35 named it exactly and the naming is
//      why this was an afternoon rather than an argument: TERRAIN.PLACE wants
//      `pitch_log2` at header +2 and the envelope at +16, TERRAIN.PAGELOADER
//      captures neither and holds what it does capture FOR THE LAST PAGE IT
//      LOADED, and TERRAIN.PAGESTREAM reads from +64 and never sees the header.
//      `zhao_terrain_hdrread` is the reader that entry asked for: one 64-byte
//      guard burst per patch, decoded to spec/terrain_rules.md 2.1, emitted
//      with the job's own identity beside it.
//
//      THE COORDINATE MOVED AS WELL, AND THAT IS A REPAIR RATHER THAN A RIDER.
//      `hdr_patch_ix_i`/`hdr_patch_iz_i` used to come from `tis_ix`/`tis_iz`,
//      read live off TERRAIN.SEQ's port. That was sound only because the header
//      and the streamer's job were accepted on the SAME CYCLE. They no longer
//      are -- a burst happens in between -- so the coordinate the envelope is
//      checked against now comes from the same latched record as the pitch. The
//      same change applies at TERRAIN.PSMUX client A, whose five job fields are
//      now TERRAIN.HDRREAD's forwarded copies. Leaving either as it was would
//      have introduced the exact join fault the new block exists to remove.
//
//      A HEADER THAT COULD NOT BE READ REFUSES THE PATCH, AND IT DOES IT
//      THROUGH THE BLOCK THAT OWNS REFUSAL. On a guard denial, a short burst or
//      an identity mismatch, TERRAIN.HDRREAD emits `pitch_log2 = 127` -- not a
//      value spec 1.3 can carry -- so `zhao_terrain_place` refuses on its own
//      `pitch_ok_c` law, `terr_place_pitch_bad_o` moves, and the composer
//      discards the page's vertices exactly as decision (c) already says. The
//      page still streams and still unpins, because the streamer's completion
//      is the unpin. That is a declared poison value, not a tie-off: a tie-off
//      invents a number the machine treats as real.
//
//      AND IT ADDS NO MEMORY CLIENT, which is the part that had to be got right
//      rather than merely done. See entry I26: `zhao_vram_arbiter` casts the
//      slot index to build its client tag and `zhao_mem_guard` grants
//      TERRAIN.PAGE_POOL to TERRAIN.BUILD alone, so a third reader with its own
//      socket is a memory-rules ruling. `zhao_mem_share2` is the answer and it
//      is not new: it is `zhao_geom_mem_adapter`'s body, lifted out with its
//      client identity made a parameter, so the guard's level-then-pulse
//      verdict law is implemented ONCE for the whole console instead of a third
//      time. `terr_rdshare_contention_o` is what the sharing cost.
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
//  * I22 was TERRAIN.TESS's LATTICE AND CELL-STATE READ PORTS. CLOSED and
//    DELETED, 2026-09-19. Its own text named `zhao_terrain_compcache_front` as
//    the owner and refused it for one stated reason -- "adopting it moves the
//    gap one hop to its fill path and to TERRAIN.COMPCACHE's store". That
//    sentence was exactly right and the fill path is what this packet built:
//    the cache's `st_*` port is fed by TERRAIN.PATCH, TERRAIN.PATCH by
//    TERRAIN.PAGESTREAM, and the `pos_*` write port by TERRAIN.PLACE. Eleven
//    ports left this module's port list rather than being driven, and the gap
//    did NOT move one hop: it moved to three named entries with three different
//    absent owners (I32 for layer D, I34 for the field lane, I35 for the patch
//    header's pitch and envelope), which is a smaller and more honest statement
//    than the one it replaces. ONE OF THE THREE IS NOW CLOSED: I35's absent
//    owner was built the same day as `zhao_terrain_hdrread` and the entry is
//    deleted -- which is what naming an absent owner precisely is for.
//
//  * I23 was GEOM.VDECODE's 32-BYTE VERTEX RECORD STREAM. CLOSED and
//    DELETED, 2026-09-19. Its own text said the wiring "is fully determined
//    and was not the obstacle", and that half was exactly right:
//    `zhao_geom_assetfetch` serves that port, `zhao_geom_meshfetch` feeds
//    it, and `zhao_geom_mem_adapter` merges the two onto the socket the
//    shell already exposes. All three are composed below and four ports left
//    this module's port list rather than being driven.
//
//    THE OBSTACLE IT NAMED WAS NOT TRUE, and the correction is recorded
//    rather than quietly acted on, because the sentence had already stopped
//    two packets. It read: "there is NO BEHAVIOURAL SDRAM MODEL IN THIS
//    TREE -- the smoke bench leaves `phy_*` unconnected". The second clause
//    was true of the smoke bench and the first was false of the tree.
//    SEARCHED, and naming what was searched is the point:
//      * `sim/models/zhao_sdram_model.sv` -- 219 lines, cycle-true against
//        `zhao_sdram_params_pkg`, with sticky per-law timing error outputs;
//      * it carries a POKE BACKDOOR whose own header comment was written for
//        this exact case -- "so a test can place asset bytes in memory
//        BEFORE the machine reads them. Without it a fetcher pointed at real
//        memory reads whatever the model was initialised to";
//      * `tests/shell/tb_zhao_shell.sv` already instantiates that model
//        against `zhao_shell_top_v2` and drives BOTH fetchers through the
//        real guard and arbiter in `realmem_mode`
//        (`tests/shell/shell_realmem_path_directed.cpp`);
//      * and the denial that genuinely HAD kept the geometry front end out
//        of the console was removed one level lower still --
//        `ZHAO_RENDER_ASSET_BASE` in `zhao_pkg`, whose comment says in as
//        many words that "every region MEM.GUARD knew was a FRAME BUFFER
//        region, so `default: pass_ok = 1'b0` denied every meshlet
//        descriptor read BY DESIGN. That denial -- not eighteen wiring jobs
//        -- is what has kept the geometry front end out of the console".
//
//    So the refusal survived its own cause by four files and some days. It
//    is this repository's "a thing BUILT is not a thing INSTALLED" chapter
//    with the cheque written by the person who then refused to cash it, and
//    the lesson worth keeping is the cheap one: a refusal that says
//    something does not exist must name what it searched, and this one named
//    nothing. The successor gaps are I36 (the draw), I37 (the CRC verdict)
//    and I38 (the release), which are three narrower statements than the one
//    they replace.
//
//  * I10 was GEOM.SKIN's BONE MATRICES; its record is still inline between I9
//    and I11 and is left there. It is the same shape as the two above and
//    SHOULD move here, but I9 is `NOT a tie-off` and that keyword wins over
//    every other, so the stray note changes nothing today. Named so the next
//    reader knows it was looked at rather than missed.
//
// ---------------------------------------------------------------------------
// THE TERRAIN CLUSTER, SWEPT 2026-09-19 -- what was composed and what was not
// ---------------------------------------------------------------------------
// MOVED HERE BY THE SECOND SWEEP, AND THE MOVE IS THIS HEADER'S OWN RULE
// RATHER THAN TIDINESS. This section is a RECORD of capabilities looked at and
// refused; it is not itself a gap. `tools/budget/completion_register.py`
// attaches every comment line to the PRECEDING `// I<n>.` entry until the next
// one, so while these eighty lines sat between two entries they were read as
// one entry's BODY -- the exact misreading the placement note above records for
// I3 and for I4. Prose before the first entry is attached to nothing, which is
// what a record should be. The KIND never changed (the entry it landed on was
// BOUNDARY and so is every sentence here), so no total was ever wrong; only the
// entry's text was somebody else's.
//
// Twelve terrain capabilities were in the completion register's
// built-but-not-connected list. TWO were composed earlier on 2026-09-19
// (MIPFEED and MIPGEN, connected item 12). The other ten were each read against
// what this module can actually offer them, and the refusals are here rather
// than nowhere, because "it was looked at" and "it was missed" are
// indistinguishable from an empty list.
//
// RE-SWEPT LATER THE SAME DAY, and this is the second reading. ALL TEN
// REFUSALS STILL STAND -- nothing is withdrawn and nothing new is composed --
// but five of them were resting on a cause that had expired, that was never the
// strongest one available, or that named an absent owner which is not absent.
// Those corrections are written INTO the paragraphs below rather than appended,
// each with the search that found it named, because a refusal that says
// something does not exist has to say where it looked. The ALM this sweep
// avoided is the ALM it avoided the first time, and it is quoted again below
// only where the second reading changed the argument for it.
//
//   TERRAIN.PROJECT -- REFUSED, AND IT IS A SAVING RATHER THAN A GAP. It is a
//   SECOND PROJECTOR. `design/blocks.yml` declares `zref::render::project_vertex`
//   as the reference model of BOTH GEOM.PROJECT and TERRAIN.PROJECT, which is
//   the duplication `tools/budget/uncashed_cheques.py` check 3 exists to find,
//   and the block ledger prices it: 6,068 ALM and 33 DSP. Terrain already
//   reaches the SHARED `zhao_proj_subsystem` on CLIENT B, through
//   TERRAIN.GROUP_SEQ, and entry I13 records that seam as closed. So composing
//   this block would spend 6,068 ALM and 33 DSP to compute a second time what
//   the machine already computes -- and would undo the deduplication campaign
//   that `zhao_project_core` exists because of. It is SUPERSEDED, not pending,
//   and the block's own header says so in as many words: "THE PROJECTOR IS NO
//   LONGER IN THIS FILE ... the law lives once, in
//   `fpga/rtl/common/zhao_project_core.sv`" (`zhao_terrain_project.sv` 19-29).
//
//   AND IT STAYS IN THE REGISTER'S DISCONNECTED LIST ON PURPOSE, which is the
//   second reading's one addition here. The obvious tidy-up is a
//   `completion_register._ALIAS` entry pointing TERRAIN.PROJECT at the shared
//   subsystem, exactly as the TEXTURE cluster and TERRAIN.RESIDENCY were
//   hand-resolved. It was NOT taken, and the reason is two lines of
//   `design/prod_manifest.yml` (588 and 921): the selected census "still counts
//   `zhao_geom_project` and `zhao_terrain_project` separately, and changing that
//   is one deliberate edit AFTER THE COMPOSED FIT CLOSES". Retiring the row now
//   would make the gap count smaller ahead of the ruling that is supposed to
//   make it smaller -- a free reduction in the flattering direction, taken by a
//   packet that is not the one holding the fit. Over-reporting one gap is the
//   safe side of that, and this paragraph is why it is over-reported.
//
//   TERRAIN.NORMALS and TERRAIN.SHADE -- REFUSED TOGETHER, on a path that
//   cannot be entered. They are a genuine pair: NORMALS takes a world triangle
//   (`ax..cz` plus `src_id`) and emits an unnormalised face normal with a
//   degenerate bit; SHADE takes exactly those fields, and its own header says
//   so ("UN-normalised -- exactly what `zhao_terrain_normals` emits"). And
//   NORMALS' input is exactly `zhao_terrain_tess`'s `tri_*` port, field for
//   field -- which is why this looks composable and is not.
//
//   THE TESSELLATOR'S TRIANGLE PORT IS NEVER PRESENTED IN THIS COMPOSITION.
//   `zhao_terrain_tess` expands a job into ModeTri (0), ModeVtx (1) or ModeRef
//   (2), and `zhao_terrain_group_seq` declares only `ModeVtx = 2'd1` and
//   `ModeRef = 2'd2` -- there is no mode-0 localparam in that file and no arm
//   that could drive one (`mode_q` is written at three sites, 449, 532 and 563,
//   and none of them writes 0). So `tri_valid_o` cannot fire while the
//   sequencer owns the job port, and composing NORMALS onto it would add
//   ~789 ALM (plus SHADE) for a path nothing can enter.
//
//   THE SECOND READING MOVED THE BAR, AND IT MOVED IT UP. This entry used to
//   say the missing piece is "a SECOND PRESENTATION from TERRAIN.GROUP_SEQ,
//   which is an RTL change to a block with its own differential". True, and not
//   the binding constraint. SEARCHED -- `reports/TERRAIN-PIPELINE-COMPOSITION-
//   20260910.md` 1 and `zhao_terrain_pipe.sv` 27-34, which is the composition
//   that already holds tess and sequencer together -- and the three options are
//   PRICED there, with none adopted:
//     * a third ModeTri pass: "+456 clocks per level-0 job -- DOES NOT FIT THE
//       TWO-VIEW SCHEDULE". So it is a throughput ruling, not an afternoon;
//     * a world-vertex arena beside the projected one: "a second 4x81 shell at
//       96 bits", which is ALM on a budget already over;
//     * a per-cell normal produced upstream, which is a block nobody has
//       specified.
//   `zhao_terrain_pipe.sv` ties the same port off for the same reason and says
//   so at its line 241, so this module is the SECOND composition to refuse it,
//   not the first. Worth knowing for whoever takes the ruling:
//   `zhao_terrain_tess.sv` 314 DOES declare `ModeTri = 2'd0` -- the mode exists
//   in the tessellator and is exercised by four drivers
//   (`fpga/rtl/synth/zhao_pair_tess_normals.sv` 113,
//   `tests/terrain/tb_terrain_compose.sv` 556, `tests/terrain/tess_harness.hpp`
//   55, and the invalid-mode case of `terrain_tess_modes_directed.cpp`). Only
//   the SEQUENCER lacks the arm.
//
//   AND TERRAIN.SHADE HAS A SECOND CLIENT THE FIRST SWEEP DID NOT NAME, which
//   matters because it means SHADE does not depend on the terrain ruling above
//   at all: `fpga/rtl/geometry/zhao_geom_light.sv` 422 INSTANTIATES it as
//   `u_shade`, deliberately -- "GEOM.LIGHT's vertex-RGB block INSTANTIATES this
//   module; building a second engine for the other normal producers is the
//   mistake both contracts now forbid" (`zhao_terrain_shade.sv` 9-11). So SHADE
//   reaches the machine through whichever of the two arrives first, and
//   GEOM.LIGHT is itself blocked on a different question again: an owner ruling
//   between it and `zhao_light_stream`, with `design/prod_manifest.yml` 950
//   recording GEOM.LIGHT MEASURED at 48.1x the frame for the ruled 120,000-
//   vertex profile. Refusing SHADE here does not decide that, and this
//   paragraph exists so the next reader does not think it did.
//   SHADE's `sun_*` would be a boundary besides -- three lanes with no producer
//   anywhere in `fpga/rtl`, whose ratified values (`zref::terrain::kShadeLight*`
//   = 26758 / 53521 / 26758) are constants a DRIVER writes.
//
//   (The block ledger's 18-DSP row for `zhao_terrain_normals` is DIRTY --
//   `rtlCleanAtHead: false`, dated before the 2026-08-24 change that took it
//   from six multipliers to one -- so that number reads HIGH and should not be
//   quoted as the cost. Named here so the next reader does not re-derive it.)
//
//   TERRAIN.VISIBLE and TERRAIN.ISLAND_DIR -- REFUSED as one, because VISIBLE
//   INSTANTIATES the directory (`u_dir` at its line 334) and composing one
//   composes both. It is refused because it would be a CENSUS INSTANCE and not
//   a connection: all three of its input groups and its only output group would
//   be boundaries.
//
//   THE SECOND READING SHARPENED BOTH HALVES OF THAT, and both corrections make
//   the refusal harder rather than softer.
//
//     * THE HANDLE IS NOT MERELY UNRECONCILED, IT IS THE WRONG WIDTH. The first
//       sweep said adopting it "would need an adapter that invents a handle
//       encoding". SEARCHED for an encoding it could use instead --
//       `design/contracts/TERRAIN.RESIDENCY.md` 47,
//       `reports/OWNER-RULINGS-BUILDABILITY-20260902.md` 712 (which is ruling
//       T10 itself), `zhao_terrain_residency_v2.sv` 35 and
//       `reports/digests/LANE2-TERRAIN-8KM.md` 60 -- and all four give the SAME
//       handle: `{resource_epoch:u32, slot:u10, generation:u8}`. That is FIFTY
//       BITS. `zhao_terrain_island_dir`'s `res_ans_handle_i` is THIRTY-TWO, and
//       the block never offers an epoch or an island id to be keyed on. So the
//       two do not merely disagree about a packing; the narrower port cannot
//       carry the ratified value at all, and `zref_island.hpp` 81-84 and 128
//       confirm why -- its `uint32_t page_handle` is an OPAQUE token the CALLER
//       supplies and the directory only stores. Closing this is a contract
//       amendment about what an island handle is, not a wiring job.
//     * THE VIEW'S OWNER IS NOT ABSENT -- IT IS SOFTWARE, BY RULING. The first
//       sweep said "the view is a camera in patch coordinates and no block here
//       produces one", which is true of `fpga/rtl` and misses the reason.
//       `design/contracts/SW.STREAM.md` 77 gives `zref::island::visible_set` to
//       the software streamer "for both the streamer and TERRAIN.VISIBLE", and
//       ruling T5 makes the sealed list CAPTURE DATA -- "replay does not rerun
//       the HPS visibility walk". `zref_island.hpp` 196-201 says the same from
//       the other side. So the work VISIBLE would do IS BEING DONE, in
//       software, and its result already reaches this module: the sealed list
//       arrives at `terr_cmd_*`, `zhao_terrain_cmd` walks it and
//       `zhao_terrain_seq` consumes the records, both composed below. VISIBLE
//       is an unadopted HARDWARE ALTERNATIVE to a ratified software path that
//       is already live here, which is a different thing from a missing
//       producer and is why composing it would add area and remove nothing.
//       Its own contract agrees (`TERRAIN.VISIBLE.md` 341-344: "Nothing
//       downstream consumes the stream yet"), and both its benches model the
//       store rather than connect one (`tests/terrain/tb_island_visible.sv`
//       4-8, `tb_island_dir.sv` 3-7) -- so the composition has never been
//       attempted anywhere, by anyone, and that is recorded in the contracts
//       too (`TERRAIN.ISLAND.md` 150-156).
//     * and its `p_*` visible-patch stream still has no consumer here:
//       TERRAIN.SEQ takes full command records with an island id, a 64-bit HPS
//       address, a CRC, flags, a view mask and a priority -- not a coordinate
//       pair and a handle.
//
//   TERRAIN.VELOCITY -- REFUSED, and the FIRST SWEEP'S STATED CAUSE HAS
//   EXPIRED. It said "Its `lane_velocity_i` is FIELD.SEQ.EARTH's out-lane 1,
//   the same block I34 names and THE SAME ONE THAT IS NOT BUILT". Entry I34,
//   thirty lines up in this same header, already says the opposite:
//   `design/contracts/FIELD.SEQ.EARTH.md` rules that sequencer out of existence
//   ("one engine, five profiles ... there is not going to be one") and the
//   FIELD v3 fabric is composed here as `u_field_host`. A refusal that
//   contradicts a live entry in its own file is worth more than a correction,
//   so here is what the second reading found instead. THREE reasons, none of
//   which is "not built":
//
//     * THE UNIFORMS, which is I34's blocker and not a separate one. The E
//       record's `age`, `phase` and `p0..p7` are UNIFORM and come from the
//       field descriptor; nothing produces that descriptor yet. Velocity is
//       out-lane 1 of the SAME evaluation whose out-lane 0 the height uses, so
//       it is blocked by exactly what the height is blocked by.
//     * TWO WALKERS OVER ONE PAGE. The block drives its own `vtx_vi_o`/
//       `vtx_vj_o` by its own chosen law ("the block OWNS the sweep"), while
//       TERRAIN.PAGESTREAM walks the lattice on its own schedule. Joining two
//       address masters is a scheduler, and a composer may not write one.
//     * AND ITS OUTPUT HAS NOWHERE TO GO, which the first sweep did not state
//       and which is the same shape as entry I44. `vv_*` is the
//       `spec/terrain_rules.md` 4.2 velocity lattice -- "height16-scaled,
//       2 B/vertex, 545 KiB" per frame -- and the block's own header says "no
//       VRAM port and no lattice-sized buffer ... the 2 B/vertex store belongs
//       to whoever owns the VRAM page". Nothing owns it. Composing the block
//       would produce a lattice with no reader, exactly as I44's decimated
//       planes do, and its `moving_mask_o` is already documented as "PRODUCED,
//       NEVER CONSUMED".
//
//   ONE THING THAT IS NOT A BLOCKER, named so it is not re-derived:
//   `lane_covers_i` DOES have a producer here -- TERRAIN.PATCH's `fld_covers_o`,
//   exported deliberately for it. And whoever takes the height lane should look
//   at `fpga/rtl/synth/zhao_probe_walk_earth.sv` FIRST: it is the Earth lattice
//   walker, differentially tested
//   (`tests/differential/field_walk_earth_directed.cpp`), its own header names
//   "ready/valid toward TERRAIN.PATCH's field-major reducer" as its downstream,
//   and it is still in `synth/` under a probe name -- which is precisely the
//   shape of the thing FIELD v3 was promoted out of earlier today. A file is not
//   a probe because its name says so.
//
//   TERRAIN.LOD -- REFUSED; entry I21 carries the whole argument and was
//   CORRECTED by both sweeps, because the blocker it named first (TERRAIN.PATCH
//   not composed) had expired, the real one is the deviation store of I44, and
//   the second reading narrowed its claim about the four extra job fields from
//   four to three. See I21.
//
//   TERRAIN.BAKE and TERRAIN.WRITEBACK -- REFUSED; entries I32 and I28 carry
//   those arguments, and BOTH were rewritten by the second sweep. I32's stated
//   cause had expired (it cited I27's placement blocker, which I27 itself
//   records as closed) and the real refusal is a packet-shape conflict plus two
//   missing LAWS; I28's third reason survived but the block's own header names
//   a blocker that has expired. Both entries carry the detail.
//
//   TERRAIN.NORMALMAP -- REFUSED, and it is the one whose seam is furthest
//   away. It is a FRAGMENT-stage block: its input is a perspective-correct
//   terrain (u, v) with an integer mip level, "tapped from the stream that
//   feeds the texture path". That stream is the pre-resolve fragment stream
//   entry I17 records as never leaving `zhao_geom_bin_pipe_v2`, and its tile
//   upload port wants a generated asset. Neither end is here, and the second
//   reading adds two facts that make that concrete rather than argued: NOTHING
//   IN THE WHOLE REPOSITORY INSTANTIATES THIS MODULE -- not `fpga/rtl`, not
//   `zhao_prod_top`, not `zhao_terrain_pipe`; the only place it is elaborated
//   at all is `tests/texture/terrain_normalmap_directed.cpp`. And its detail
//   pyramid is built OFFLINE ("by averaging SIGNED dx/dz", its line 61), so the
//   `tw_*` upload port is waiting on an asset pipeline rather than on a block.
//   `design/prod_manifest.yml` 856 already calls it an "OPEN DEFERRAL until the
//   fragment-seam wiring lands", which is the same statement from the ledger's
//   side.
//
//  I1. PART.STATE's generation store (`part_rd_*`, `part_wr_*`) -- BOUNDARY.
//      The plan lets the harness supply "memory behavior", and this is that.
//      But the real provider is named nowhere: MEM.HPS.BRIDGE is instantiated
//      inside the shell and has no particle client port, so no route from this
//      store to that bridge exists yet. Connecting them is a shell change.
//
//
//  I5. PART.UPDATE's field sample (`part_fld_*`) -- BOUNDARY, and the REASON
//      CHANGED 2026-09-19. The old text said "FIELD.SEQ.FLOW is not composed",
//      and that named a block `design/contracts/FIELD.SEQ.FLOW.md` rules will
//      never exist: "one engine, five profiles ... There is no separate
//      FIELD.SEQ.FLOW sequencer in hardware and there is not going to be one."
//      The engine IS composed now -- `u_field_host`, at the end of this
//      module -- so what this seam lacks is its own stream adapter, and the
//      adapter lacks one specific thing.
//
//      WHAT IS ACTUALLY MISSING IS THE F PROFILE'S LANE BINDING, and unlike the
//      S profile's it cannot be assembled out of parts that are already
//      ratified. I31 closed because both of its halves existed:
//      FIELD.SEQ.CORE.md names the S varying lanes ("Stamp: stencil u,v") and
//      `zhao_surface_stamp` already unpacks field-ir 7.1's stamp record byte
//      for byte. For F, FIELD.SEQ.CORE.md names only the input KIND ("Flow:
//      particle state"), and nothing in this tree says which fields of the
//      128-bit particle128 record become which registers, nor which registers
//      the three s11 accelerations are read back from. FIELD.SEQ.FLOW.md says
//      that binding is still open and belongs "with the blocks that consume the
//      output"; choosing it inside a composition packet would be inventing an
//      ABI, and an invented lane map produces a perfectly plausible wind.
//
//      THE JOIN IS ALREADY SOLVED, recorded here so the next packet does not
//      re-derive it. `zhao_part_update` takes this sample COMBINATIONALLY in
//      the same cycle as `in_record_i`, exactly as it takes the species
//      descriptor, and its header says why: "both sides of every comparison are
//      the same cycle's wires, so there is no stall for them to come apart
//      across." So a FLOW adapter needs NO new port on that block -- this file
//      gates `in_valid_i` and PART.STATE's `prt_ready_i` on "the answer for
//      THIS record is ready", and the record is held stable for the whole
//      offer. That is a join a composer may write. The binding is not.
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
//      GEOM.LOOM, and the parenthesis that used to stand here said so for two
//      WRONG reasons -- "that is SKIN -> WARP deformation, and it is in the
//      register's NOT-BUILT list". GEOM.LOOM is a streaming affine matrix
//      composer whose contract excludes skinning by name, and it is BUILT and
//      UNIT_VERIFIED. It is still not this customer, and the refusal is now
//      argued properly in the refused-blocks list below.
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
//      host/CMD state and CMD.SCHEDULER has no projection-config path". That
//      sentence was wrong about the block as well as about the path --
//      CMD.SCHEDULER is composed, as `u_sched` inside `u_shell`, and entry I36
//      carries the evidence and the correction for all four entries that said
//      "absent". They
//      have a path now, and it is not CMD.SCHEDULER: `zhao_cmd_exec` (section 7c)
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
//        * `proj_en_i`, and `SetView`'s OTHER FOUR FIELDS. `pixel_error` wants
//          MEASURE.GOVERNOR and `geometry_tokens`/`fragment_tokens` want
//          MEASURE.TOKENS, none of which is composed. A ratified field with no
//          port is still a gap.
//
//      CLOSED 2026-09-19: THE DEPTH PROFILE. This entry used to read
//      "`flags[1:0]` is the depth profile of the frozen 2026-08-31 ruling and
//      `zhao_project_core` has no port to put it on". It has one now, and NO
//      PORT ON THIS MODULE'S CFG BUS HAD TO MOVE FOR IT: `cfg_addr_i` has been
//      five bits since the viewport words landed, so address 18 was already
//      reachable, and `zhao_geom_cull` -- which shares the bus -- ignores
//      addr >= 16 on purpose, so the word reaches the projector and nobody
//      else. CMD.EXEC's view walk is SEVENTEEN steps instead of sixteen, and
//      step 16 carries `flags[1:0]` to cfg address 18 under the SAME dirty bit
//      as the matrix, so a view's camera and its profile cannot land in
//      different frames.
//
//      ZERO KEEPS ITS MEANING (`spec/commands.zidl:301`). 2'd0 is WORLD_LONG,
//      it is the bank's reset value, and no existing capture decodes
//      differently -- which is the whole reason the ruling chose `flags` over a
//      new opcode. The reserved value 2'd3 is REFUSED by the bank (the register
//      keeps its previous profile) because `zhao_geom_depthquant` indexes
//      THREE-entry tables with this two-bit field, and one past the end is X
//      rather than a diagnosis.
//
//      WHAT IS NOT CLOSED BY IT, said plainly, because a carried field with no
//      reader is the uncashed cheque this file has a chapter about.
//      `proj_a_profile_o` and `proj_fill_profile_o` LEAVE this module. Their
//      consumer is GEOM.DEPTHQUANT, whose `v_profile_i` is exactly this width
//      and meaning and whose own header opens with the audit finding this port
//      answers -- "no `depth_profile` port exists anywhere in fpga/rtl" -- and
//      that block is not composed. Terrain's per-TRIANGLE profile is owed
//      besides: the replay arena carries no profile field, so
//      `zhao_vertex_arena`'s payload would have to widen, and that is a change
//      to that block rather than to a composer.
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
//      DEBUG.TRACE IS NO LONGER REFUSED. It is COMPOSED at section 7b-ii as
//      of 2026-09-19, and the refusal that stood here was WRONG rather than
//      stale -- it is preserved below because a refusal that reasoned from the
//      wrong port is worth more as a record than as a deletion. It read:
//
//        "DEBUG.TRACE is refused by THIS ENTRY'S OWN ARGUMENT, one block over.
//         Its event carries `ev_expected_fx_i` beside `ev_actual_fx_i` -- a
//         differential against a reference -- and the console has no reference,
//         which is the sentence above. The ledger's `upstream: [CMD.DECODER]`
//         is not the RTL's seam either: the decoder emits record headers, not
//         {stage, tile, primitive, pixel, expected, actual}."
//
//      IT COMPARED THE DECODER'S OUTPUT AGAINST THE RING'S OUTPUT. The nine
//      `ev_*` fields are the RATIFIED TRACE RECORD the ring stores
//      (`spec/capture_format.md` chunk 0x000A, 32 bytes). What the block
//      CONSUMES is the table in its own contract's "Input and output packet
//      layouts": `rec_valid_i`/`rec_ready_o`, `rec_opcode_i` (16),
//      `rec_bytes_i` (16), `rec_source_id_i` (32), `rec_index_i` (32) -- which
//      is `zhao_cmd_decoder`'s record port, field for field and width for
//      width. The same contract's Integration section says "this block's only
//      producer is that block's record port", and that producer has been
//      composed in this file since earlier the same day.
//
//      THE ADAPTATION IS RATIFIED, NOT INVENTED, which is the claim that had
//      to be checked because five of the nine fields are driven by constants.
//      `zref::trace::Ring::on_record()` -- the ledger's declared
//      `reference_model` -- IS the mapping: stage = `kCommandDecoder` (0),
//      `source_id` and `command_seq` from the record, and the five raster
//      fields zero by that file's recorded choice 3, "zero, because zero is
//      checkable ... leaving them undefined is how a trace format rots".
//
//      SO THE PREMISE SURVIVES EXACTLY WHERE IT WAS TRUE. The console still
//      has no reference, and the two fields that would need one are the two a
//      decoder-stage event does not carry. The other six charter 20.6 stages
//      have no producer here; entry I45 records that as the remaining half.
//
// I19. MEASURE.HISTOGRAM's host read window (`hist_rd_*`) -- BOUNDARY. The
//      host is the HPS; no register path from HPS to this block exists.
//
// I20. Everything `zhao_shell_top_v2` already declares provisional at its own
//      edge -- the triangle port, `fb_writer_i`, the FRAME_RING view, the
//      geometry memory clients -- is UNCHANGED and still provisional. This
//      file adds no opinion about them; read that file's header.
//
//      NARROWED AND MADE SPECIFIC 2026-09-19, because "read that file's
//      header" was hiding a group of six ports that file's header does not
//      discuss and this one had never named -- the PACKET-D ATTRIBUTE
//      CARRIAGE. HALF OF IT IS NOW CLOSED. What is left, and what it is
//      waiting for, is the rest of this entry.
//
//        tri_invw_plane_i       240b   CLOSED -- GEOM.ATTRPACK, plane 0
//        tri_u_over_w_plane_i   240b   CLOSED -- GEOM.ATTRPACK, plane 1
//        tri_v_over_w_plane_i   240b   CLOSED -- GEOM.ATTRPACK, plane 2
//        tri_flat_request_i     298b   OPEN, still a BOUNDARY
//        tri_continuation_tail_i 48b   OPEN, still a BOUNDARY
//        tri_fragment_state_i    32b   OPEN, still a BOUNDARY
//
//      The three OPEN ports are a BOUNDARY and this entry stays one. Their
//      owner is MATERIAL.RESOLVE, which IS built and is NOT composed -- the
//      distinction matters and this line used to get it wrong ("it is not
//      built"). See below.
//
//      HOW THE THREE PLANES CLOSED, and why it was a FRONT END and never
//      missing arithmetic. `fpga/rtl/geometry/zhao_geom_attrsetup.sv` was real,
//      directed-tested and emitting `n0_o`/`dndx_o`/`dndy_o` -- 96+72+72 =
//      exactly the 240 bits of ONE plane -- the whole time. Its own header says
//      "ONE ATTRIBUTE PER REQUEST. A textured Gouraud triangle needs seven
//      planes and asks seven times." What did not exist was the block that
//      ASKS. `zhao_geom_attrpack` is that block: one shared attrsetup core,
//      three lanes, three packed plane words, composed below off GEOM.CLIP's
//      own winding-flipped attribute packet -- which this module was ALREADY
//      emitting to nobody through `geom_clip_attr_*_o`. The three ports are
//      retired from this edge exactly as `tri_area2_i` was.
//
//      THE MATERIAL RECORD IS A DIFFERENT KIND OF GAP and it is named now
//      rather than described. `tri_flat_request_i` is
//      `zhao_texture_v3_request_v2_t[297:0]`: palette generation and slot,
//      response class, base rgb and alpha, the 224-bit aux surface context,
//      aux_required, recipe weight, material recipe, LOD, base binding
//      selector, sample count. Every one of those fields is
//      MATERIAL.RESOLVE's output.
//
//      CORRECTED 2026-09-19, and the sentence it replaces was wrong in BOTH
//      halves, each for a different reason. It read: "`design/blocks.yml`'s
//      MATERIAL.RESOLVE row reads `maturity: SPECIFIED`, both tests 'PLANNED
//      -- NOT WRITTEN', and a note recording that it is blocked on a cartridge
//      decision (audit R4: .zpak has no generic texture-page or material-set
//      kind). So this is not an unwired block -- it is an unbuilt one."
//
//        * THE BLOCK IS BUILT. `fpga/rtl/texture/zhao_material_resolve.sv`,
//          UNIT_VERIFIED, differenced against `zref::material::Resolver` by
//          `tests/texture/material_resolve_rtl_directed.cpp` -- 91 checks, 0
//          failures, every counter seen to fire, with a committed positive
//          control (`tests/mutants/zhao_material_resolve_gen8tag_mutant.sv`)
//          for owner ruling D-3's cache tag. So it IS an unwired block, which
//          is the opposite classification and a different repair.
//        * THE CARTRIDGE BLOCKER WAS ALREADY DEAD WHEN THAT LINE WAS WRITTEN.
//          Owner ruling D-2 chose option A on 2026-09-03 and
//          `spec/cartridge.md` 4a allocates the kinds -- 10 TEXTURE_PAGE
//          (0x000E), 11 MATERIAL_SET (0x000F), 12 MESH_STREAM (0x0010) -- and
//          `spec/commands.zidl:220-262` froze the 32-byte record on
//          2026-09-05. Sixteen days.
//
//      THE RULING IT WAS BLOCKED ON WAS MADE, AND IT WAS NOT THE LAST THING.
//      `spec/memory_rules.md` 5f.1, 2026-09-19: a published slot is named by
//      the handle index of the resource it holds, so the residency directory
//      is {index:24} keyed with row {slot, base, extent, kind}, and
//      `zhao_mem_upload` now publishes all five. The BASE and EXTENT were never
//      missing from the design -- they were missing from the PUBLICATION: that
//      block already took `req_vram_addr_i` and `req_len_i` and bounds-checked
//      both against `cfg_region_*` before writing a byte, then dropped them.
//      What was genuinely absent was the KEY, and 5f.1 names it.
//
//      SO THE ENTRY STAYS OPEN, AND ITS REASON IS NOW FOUR SEAMS RATHER THAN
//      ONE UNDECIDED SENTENCE. `reports/OWNER-DOCKET-20260919.md` item 4 called
//      the ruling "the last thing between the texture island and sampling
//      anything". It is not, and every one of the four is an entry this file
//      ALREADY CARRIES -- which is the part worth keeping: the distance was
//      written down in four places and nothing had added them up.
//
//        1. MEM.UPLOAD IS COMPOSED NOWHERE, so nothing can WRITE the
//           directory. `zhao_hps_arbiter` has exactly two client ports and
//           both are taken in both instances -- CMD.DMA and DEBUG.FRAMEBLIT
//           inside `zhao_shell_top_v2`, TERRAIN.CMD and TERRAIN.PAGELOADER in
//           `u_terr_hps_arb` below. Entry I27 already says a third client is
//           an owner ruling; this is a SECOND customer for that same ruling,
//           which is worth knowing before it is priced as a terrain-only one.
//        2. THE RECORD FETCH wants a third ENGINE1 requester.
//           `u_geom_mem_adapter` has exactly two and they are spent on
//           GEOM.MESHFETCH and GEOM.ASSETFETCH.
//        3. THE RESOLVE REQUEST HAS NO HONEST PRODUCER, and this is the one
//           that looks closed and is not. Both nouns are live in this module
//           -- `cmd_draw_material_set_o` and `mf_r_material_id` -- and they
//           MAY NOT BE JOINED. The first is the DRAW's, and CMD.EXEC's draw
//           dispatch is entry I41, a boundary; the second is GEOM.MESHFETCH's
//           result register, which entry I39 records has already moved on by
//           the time the meshlet is offered. Joining them would assemble
//           meshlet N's triangles with meshlet M's material -- the fault I39
//           refuses by name. TWO LIVE WIRES ARE NOT A PRODUCER.
//        4. `tri_flat_request_i` WANTS MORE THAN THE RECORD: the binding
//           page's `palette_slot`, `palette_generation` and `response_class`
//           are `zhao_texture_binding_resolver_v2`'s, which takes the
//           request's copies as WITNESSES and CHECKS them, so a resolver that
//           invented them would manufacture that mismatch. MATERIAL.RESOLVE's
//           own projection section says this in as many words.
//
//      DRIVING THE REMAINING THREE FROM HERE IS STILL REFUSED for the usual
//      reason: the only legal-value constructor in the tree is
//      `tools/quartus/gen_shell_fit_top.py` lines 596-654, which exists to keep
//      a FIT honest. That is stimulus. And a zero flat request is a LEGAL
//      profile (`profile_aux_bad_ref_c` is `flat_request[268] ||
//      flat_request[267:44] != 0`), so a composer that invented plausible
//      constants here would produce a picture and prove nothing -- which is the
//      most dangerous shape a fake connection can take in this file.
//
//      WHAT THIS MEANS FOR THE PICTURE, measured rather than assumed. Before:
//      all six at zero, 1,536 pixels through RASTER.FBWRITE, planes
//      interpolating to zero without setting `attr_error` -- the path proven
//      and the SHADING not. Now: the three interpolants are real functions of
//      the triangle's own vertex attributes, so depth and the two texture
//      coordinates vary across the surface for the first time. What is still
//      flat is the MATERIAL -- with `sample_count`, `material_recipe` and
//      `base_binding_selector` all zero the texture island runs and samples
//      nothing. Perspective-correct interpolation is composed; the surface it
//      would sample is not bound.
//
//      FIVE THINGS LEFT THIS EDGE 2026-09-19 and are no longer provisional:
//        * `tri_area2_i` -- retired as a port, driven by GEOM.SETUP. See
//          composition entry 7.
//        * the three attribute planes -- retired as ports, driven by
//          GEOM.ATTRPACK off GEOM.CLIP's attribute packet. See the
//          GEOM.ATTRPACK composition block for the fork, the join and what
//          the shared core costs in clocks.
//        * the RENDER guard's window -- `zhao_shell_top_v2`'s `u_guard_render`
//          took the BLITTER's `map_*` and now takes VIDEO.SLOTMGR's live lease.
//      `fb_writer_i` is NOT one of them and stays exactly as that file
//      declares it: the manager's `lease_writer_o` is the value it will
//      become, and making that substitution is CMD.SCHEDULER's act, not this
//      composer's.
//
// I21. TERRAIN.GROUP_SEQ's subpatch job port (`terr_job_*`,
//      `terr_sparse_fill_i`) -- BOUNDARY, and this one has a near-producer
//      that is NOT wired, which is worth stating precisely so nobody wires it
//      by name-matching. `fpga/rtl/terrain/zhao_terrain_lod.sv` exists and its
//      own header says its output is "EXACTLY zhao_terrain_tess's job port".
//      It is not the sequencer's job port: the sequencer additionally needs
//      `job_view_mask`, `job_mat_a`, `job_mat_b` and `job_weight`, and
//      TERRAIN.LOD emits none of the four. Taking its 12 matching fields and
//      inventing the other 4 here is the hidden-adapter failure.
//
//      CORRECTED 2026-09-19, and the correction makes this entry HARDER rather
//      than easier, which is why it is worth the space. The entry used to add:
//      "TERRAIN.LOD is itself unfed besides: its `sp_*` patch_state
//      descriptors come from TERRAIN.PATCH, which is not composed. Closing
//      this properly is LOD + PATCH composed together, with the view mask and
//      materials coming from whoever owns the draw -- CMD.SCHEDULER."
//
//      TERRAIN.PATCH IS COMPOSED NOW (connected item 10) AND IT STILL CANNOT
//      FEED TERRAIN.LOD. The two ports share a name in `design/blocks.yml` --
//      both are called `patch_state` -- and they are different things, which
//      is this file's standing lesson about the declared edge not being the
//      RTL's seam:
//        * `zhao_terrain_patch.st_*` is PER VERTEX. One record per lattice
//          vertex: `top_o`, `bottom_o`, `compose_top_o`, a dirty bit and a
//          source id. It is a composed height.
//        * `zhao_terrain_lod.sp_*` is PER SUBPATCH, sixteen per patch, and it
//          carries a CENTRE (`sp_cx/cy/cz`), three STORED COARSE DEVIATIONS
//          (`sp_dev1/2/3`) and the previous frame's HISTORY (`sp_prev_level`,
//          `sp_prev_morph`, `sp_hold`).
//      Not one field of the first is a field of the second. Wiring them by
//      name-matching would have produced a LOD decision made from a height.
//
//      SO THE REAL BLOCKER IS THE DEVIATIONS, and this entry now names it
//      instead of naming the wrong block. `sp_dev1/2/3` are the differences
//      between the fine lattice and its coarse mips -- the "17x17 + 9x9 ...
//      for TERRAIN.LOD" of `spec/terrain_rules.md` 2. TERRAIN.MIPGEN computes
//      those mips and is composed as of 2026-09-19, and THE STORE THAT WOULD
//      HOLD THEM DOES NOT EXIST: that is entry I44 (it was written as I42 and
//      renumbered later the same day; see that entry for why). The quantity
//      itself has no executable definition anywhere either, which is a second
//      statement and a worse one: `reports/TERRAIN-LOD-DEVIATION-20260907.md`
//      found the only driver of `sp_dev1_i` in the tree to be an LFSR in
//      `zhao_prod_top`, every test writing the three by hand, and TWO
//      DEFENSIBLE READINGS of `dev[L]` (the morph deviation, which excludes the
//      boundary ring, against the mesh deviation, which includes it) that
//      differ on every subpatch edge. The law is derivable from `coarse_height`
//      and `morph_case` with no new arithmetic -- and choosing WHICH of the two
//      it is, is an owner ruling, not a composition. The history is the other
//      half and is the caller's by TERRAIN.LOD's own chosen law 5 -- it
//      "rides the packet" and the block deliberately keeps no RAM -- so
//      whoever owns the deviation store owns the history beside it.
//
//      AND THE OWNER OF THE FOUR EXTRA FIELDS IS NOT CMD.SCHEDULER. That
//      block exists and has been instantiated as `u_sched` inside
//      `zhao_shell_top_v2` since 2026-08-16, so the old sentence was false
//      twice over -- it named an absent owner that is not absent and would
//      have been the wrong owner anyway, its own first line calling it "the
//      3-slot frame ownership FSM". `job_view_mask`, `job_mat_a`, `job_mat_b`
//      and `job_weight` belong to whoever owns the terrain DRAW.
//
//      NARROWED FROM FOUR FIELDS TO THREE, 2026-09-19, and the correction is
//      against THIS FILE'S OWN WIRE DECLARATION rather than against a document.
//      The sentence that stood here said all four "belong to whoever owns the
//      terrain DRAW, and no block in this tree claims that. The owner is
//      UNIDENTIFIED". That is FALSE FOR `job_view_mask`, and the refutation is
//      ninety lines below at `tis_view_mask`, where this module already says
//      the field is produced and names where it goes. The chain is composed
//      end to end today: ruling T5's `SubmitTerrainSet` record carries a
//      per-record view mask, `zhao_terrain_cmd` emits it as `rec_view_mask_o`,
//      `zhao_terrain_seq` carries it to the compose door as `is_view_mask_o`
//      (its line 394, from `r_view_q`), and it lands here as `tis_view_mask` --
//      declared, waived as unconsumed AT the declaration, with the comment
//      "`is_view_mask_o` and `is_priority_o` belong to the subpatch job
//      TERRAIN.LOD would build, which is entry I21's boundary". So the producer
//      is not absent; the CONSUMER is, and the consumer is TERRAIN.LOD.
//
//      WHAT THE VIEW MASK STILL NEEDS IS A RECONCILIATION, NOT AN OWNER, and
//      it is a real question this entry does not answer: the door's mask is
//      EIGHT bits (T5's per-player tag, `zhao_terrain_cmd.sv` 124-131 recording
//      the SET-level copy as work with no consumer) while
//      `zhao_terrain_group_seq.job_view_mask_i` is TWO, documented "bit v =
//      project into view v" and compared against `2'b11`/`2'b10`/`2'b00` to
//      choose one or two arena slots. Those are a PLAYER mask and a PROJECTOR
//      VIEW mask. On a two-view machine they very probably coincide, and
//      "very probably coincide" is exactly the reasoning that produces a hidden
//      adapter, so the narrowing is named here and not performed.
//
//      THE THREE THAT REMAIN UNOWNED are `job_mat_a`, `job_mat_b` and
//      `job_weight` -- the material pair and its blend weight. SEARCHED: the
//      T5 record carries {island, ix, iz, hps_addr, crc, flags, view_mask,
//      priority, src_id} and no material at all, TERRAIN.CMD adds none, and
//      nothing else in `fpga/rtl` drives a `job_mat_*`. For those three the
//      owner is UNIDENTIFIED, which is a smaller and truer statement than a
//      name -- and it is now a statement about three fields instead of four.
//
//      THE DATA IS NOT UNOWNED; THE CARRIER IS WRONG-GRAINED. Added 2026-09-19
//      by the terrain packet. `spec/terrain_rules.md` 2's page table has layer
//      E, "Base material | 32x32 | {matA u8, matB u8, weight unit8} | Mosaic
//      candidates", and its section-6 draw law reads "matA/matB/weight from
//      layer E at cell". So the triple has a ratified SOURCE -- the resident
//      page -- and it is PER CELL, while `job_mat_a/_b/_weight` are PER
//      SUBPATCH JOB and ride every reference of that job unchanged. A job
//      covers many cells, so no producer can fill the job port with layer E
//      honestly: picking one cell's triple, or any reduction of 64, would be a
//      look law invented in the composer. What is missing is (a) a reader of
//      layer E (TERRAIN.PAGESTREAM reads A, B, C and not E; the compose cache
//      holds no E) and (b) a decision on WHERE the per-cell triple joins the
//      triangle -- at the job, or per reference by the triangle's cell. (b) is
//      a contract conflict between TERRAIN.GROUP_SEQ's port and terrain_rules
//      6, and it is the owner's, not this file's. SEARCHED: `grep -i "layer
//      E|mat_a|material" fpga/rtl/terrain` -- only riders and comments.
//
//      WIDENED 2026-09-19, and it is one more end of the SAME absent owner
//      rather than a second gap: `terr_cc_serve_release_i`, TERRAIN.COMPCACHE's
//      patch retirement. The cache's port means "TESS is finished with the
//      served patch" and is a PULSE, one patch per rising edge. The block that
//      knows when a patch is finished is the block that issued its subpatch
//      jobs and counted them home -- the same one this entry is about. Wiring
//      it to anything this composition can see (a tessellator idle, a job
//      handshake, a counter) would be a retirement POLICY invented in the
//      composer, and the cache's own header records what a wrong reading of
//      this port already cost once: a whole patch retired without one vertex
//      being read, with `patches_served_o` counting it as consumed.
//
//      A CONSEQUENCE WORTH STATING, because it looks like a stall and is not.
//      With nothing driving the release, the compose engine fills ONE patch,
//      hands it to the serve side, fills a second, and then holds: the second
//      fill cannot hand over until the first is retired, and the composition
//      gates a new page on the cache's own `fill_busy_o` (see the engine's
//      note (b)). So `terr_cc_patches_filled_o` reaching 1 and stopping, with
//      `terr_ps_lattices_o` at 2, is the EXPECTED reading of an unretired
//      cache and not a defect in the chain.
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
//      WIDENED 2026-09-19 BY A SECOND GUARD CLIENT (`terr_ps_guard_*`,
//      `terr_ps_beat_*`). The compose engine's TERRAIN.PAGESTREAM reads the
//      page back out of the pool that TERRAIN.PAGELOADER wrote, so it is a READ
//      client and needs the return leg -- `beat_valid/data/last` -- that a
//      write client has no use for. It is listed here rather than as its own
//      entry because it is the same absent thing: the shell exposes ONE guard
//      socket and it is named for GEOM.
//
//      AND NOT WIDENED AGAIN BY THE HEADER READER, 2026-09-19, which is the
//      part worth writing down because the obvious version of that packet WOULD
//      have widened it. TERRAIN.HDRREAD (composed item 13) is a third reader of
//      the same pool, and a third guard port on this module's edge would have
//      turned a two-port boundary into a three-port one while closing I35 --
//      a gap count that goes down by one and a boundary that goes up by one.
//
//      IT SHARES INSTEAD, through `zhao_mem_share2` as `u_terrain_rdshare`, and
//      the share is not invented here: it is the SAME MODULE
//      `zhao_geom_mem_adapter` is now a wrapper over, with ENGINE1's two values
//      swapped for TERRAIN.BUILD's. That is the pattern the geometry asset path
//      established and this entry's own note about `zhao_vram_arbiter` casting
//      the slot index is why: a genuinely new client id is a memory-rules
//      ruling, and a share avoids needing one.
//
//      THIS IS NOT THE MUX THE PARAGRAPH BELOW REFUSES, and the difference is
//      exact. That paragraph refuses joining the WRITE client and the READ
//      client, because `zhao_mem_guard`'s arbitration between two different
//      privileges is the shell's business. `u_terrain_rdshare` joins two
//      readers of ONE arm of ONE window under ONE client id, upstream of the
//      guard, in a committed block with its own directed test and its own
//      contention counter. Two ports out, still, and one owner to join them
//      when the shell grows the socket.
//
//      THE TWO GUARD CLIENTS ARE NOT MERGED HERE, and that is deliberate.
//      `zhao_mem_guard`'s arbitration is the shell's, and putting a mux between
//      two clients in this file would be an arbiter the composer invented --
//      the thing the paging spine's own note above is proud of not having done.
//      Two ports out, one owner to join them, when the shell grows the socket.
//
// I27. THE DIRECTORY's DEFORMATION MARK and HANDLE CHECK (`terr_dm_*`,
//      `terr_chk_*`) -- BOUNDARY. NARROWED 2026-09-19, and the two halves that
//      left are recorded here rather than deleted with them, because this entry
//      is where the next reader will look for them:
//
//        * THE COMPOSE DOOR (`terr_is_*`) IS CLOSED. It drives
//          TERRAIN.PAGESTREAM's job port and TERRAIN.PLACE's patch header
//          inside this module (connected item 10). The blocker this entry named
//          -- "THE BLOCKER IS PLACEMENT, and it is a missing owner rather than
//          missing wiring" -- was accurate, and `zhao_terrain_place` is that
//          owner. Fourteen ports left the port list rather than being driven.
//        * THE UNPIN (`terr_unpin_*`) IS CLOSED. TERRAIN.PAGESTREAM's `done_*`
//          fires once per issued patch, whatever the verdict, and that is the
//          "engine's, on job completion" unpin TERRAIN.SEQ's port comment asks
//          for. Five more ports left the list.
//
//      WHAT IS LEFT, and they are two different absent owners kept in one entry
//      only because they are two ports of ONE block, the directory:
//
//        * `terr_dm_*`, the deformation mark. Its writer is TERRAIN.BAKE, which
//          is built and not composed -- entry I32 carries the full argument and
//          this is the same refusal seen from the directory's side. Nothing in
//          this core can dirty a page, which is also why I28's writeback could
//          not see a beat.
//        * `terr_chk_*`, the handle staleness check. Its caller is whoever
//          holds a page handle across a frame and wants to know it is still
//          valid -- the subpatch issuer of entry I21. There is no such block.
//
//      NOT A GAP AND NAMED SO IT IS NOT RE-OPENED: `is_cslot_o` and
//      `is_cslot_valid_o` are T6's 256-entry composed-height cache index, and
//      `zhao_terrain_compcache_front` is a two-buffer FRONT rather than that
//      store. They are carried into this module and read by nobody, which is
//      declared at their wire rather than hidden behind an empty port
//      connection. The store behind that index is a later packet's block.
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
//      RE-READ 2026-09-19 AND THE REFUSAL SURVIVED -- but ONE BLOCKER THE
//      BLOCK'S OWN HEADER STILL ADVERTISES HAS EXPIRED, and it is named here
//      because it is the first thing the next reader will hit and it will stop
//      them. `zhao_terrain_writeback.sv` 67-84 says "MEM.GUARD MUST GAIN A READ
//      ARM, AND IT IS NOT MADE HERE ... TERRAIN.PAGE_POOL, WRITE-ONLY UNTIL
//      2026-09-06", and concludes "until it lands, every sheet faults as
//      V_INCOMPLETE with `guard_denied_o` counting, and NO slot is released".
//      SEARCHED `fpga/rtl/memory/zhao_mem_guard.sv`: IT LANDED. Its region
//      table now reads "TERRAIN.PAGE_POOL ... TERRAIN.BUILD, WRITE (pages in)
//      and READ (F sheets" (lines 15-16), line 163 says the arm is "READ for
//      the writeback (rulings T2 / T3 / T4)", and lines 198-200 are the guard
//      answering the writeback's own sentence: "THE READ WAS WITHHELD UNTIL ITS
//      BLOCK EXISTED ... brings its own arm and its own proof." So the deadline
//      in that header is thirteen days past and the file has not been told.
//      This is the uncashed-cheque shape one step on: the prerequisite was
//      built, and the note asking for it was never read back.
//
//      THE THREE REASONS ABOVE ARE UNAFFECTED, and it is worth being explicit
//      that removing an expired blocker did not weaken them. The journal ticket
//      still has no owner (`tests/terrain/tb_terrain_world.sv` 1651-1655 mints
//      it in glue and calls that "a finding rather than a convenience"); the
//      HPS arbiter in this module is `u_terr_hps_arb` with its two ports taken
//      by TERRAIN.CMD and TERRAIN.PAGELOADER, so a third client is still an
//      owner ruling; and the dirty-eviction path is still unreachable, which
//      traces to I32 rather than to I27 now that I32 has been rewritten.
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
//      (I31 was SURFACE.STAMP's FIELD-DRIVEN BRUSH. It is CLOSED and the
//      entry is DELETED, 2026-09-19. Its old text said "FIELD.SEQ.STAMP is the
//      named owner and it is not built", and that owner is ruled never to
//      exist -- `design/contracts/FIELD.SEQ.STAMP.md`: "one engine, five
//      profiles ... There is no separate FIELD.SEQ.STAMP sequencer in hardware
//      and there is not going to be one." What was missing was the engine,
//      which is `u_field_host` now, and the S profile's STREAM ADAPTER, which
//      FIELD.SEQ.CORE.md permits by name and which is
//      `u_field_stamp_adapter`. `surf_fld_valid_i`, `surf_fld_ready_o`,
//      `surf_fld_tag_op_i` and `surf_fld_strength_i` are GONE from the port
//      list rather than driven from it.
//
//      The binding was assembled rather than chosen: FIELD.SEQ.CORE.md names
//      the S varying lanes "stencil u,v", and `zhao_surface_stamp` already
//      unpacks spec/form/field-ir.md 7.1's {tag_op, strength} byte for byte.
//      The ONE decision this file takes is policy and is stated beside the
//      stamp: `cmd_field_en_i` is ANDed with "a stamp program is resident", so
//      a stamp that asks for the brush with nothing loaded runs as a plain ABI
//      stamp instead of stalling forever on records that cannot come.)
//
// I32. SURFACE.STAMP's `stamp_results` (`surf_res_*`) -- BOUNDARY. TERRAIN.BAKE
//      is the named consumer, it is built and it is NOT composed. The port is
//      on this module's edge so the result stream is observable rather than
//      dropped.
//
//      THE REASON CHANGED 2026-09-19 AND THE ONE IT REPLACES HAD EXPIRED.
//      This entry used to read: "TERRAIN.BAKE belongs to the terrain compose
//      engine that entry I27 says is blocked on a placement owner, and adopting
//      it for this one port would pull that whole subsystem in behind a seam
//      I27 already records as unclosable today." Entry I27 does not say that
//      any more and says the opposite in its first bullet -- "THE COMPOSE DOOR
//      (`terr_is_*`) IS CLOSED ... `zhao_terrain_place` is that owner" -- and
//      the compose engine is composed below. A refusal whose cited authority
//      has since withdrawn the citation is the shape this header exists to
//      catch, so it is replaced rather than patched, and the real one is
//      harder.
//
//      THE TWO PORTS ARE NOT THE SAME PACKET, AND THE BLOCK'S OWN HEADER SAYS
//      SO FIRST. `zhao_surface_stamp`'s result stream is PER TEXEL of a 64x64
//      layer-F sheet -- `res_texel_o[11:0]`, `res_tag_o`, `res_strength_o`,
//      `res_before_o`, `res_src_id_o`, which is what `surf_res_*` carries out
//      of this module. `zhao_terrain_bake`'s `cmd_*` is ONE RECORD PER PATCH
//      BAKE: {patch_id, cx, cz, radius, depth_from, depth_to, env_x0/z0/x1/z1,
//      dual, cells, src_id}. Every field differs but `src_id`. The block wrote
//      the conflict down when it was built (`zhao_terrain_bake.sv` 29-48): "the
//      ledger says `inputs: [stamp_results]` ... This contract's own packet
//      table says something DIFFERENT ... Those are two different wires wearing
//      one name", and it refuses to bridge them for a stated reason -- closing
//      that seam needs TWO LAWS THAT DO NOT EXIST ANYWHERE IN THIS TREE: a
//      strength(u8) -> depth(fx16) mapping and a 64x64 -> 33x33 resample.
//      "Inventing them here would put a fabrication under every permanent
//      wound in the game." That is a better refusal than the one it replaces
//      because it names what would have to be RATIFIED, not what is not wired.
//
//      AND THERE IS A SECOND, INDEPENDENT ABSENCE: THE PAGE PORT. Bake's DIG
//      phase drives `vtx_vi_o`/`vtx_vj_o` and expects layers A, B and C back
//      ({base, scar, bottom, nobake}) with a layer-B writeback on `sc_*`, and
//      its BREACH phase does the same for layer D on `cell_*`. It has no VRAM
//      port by design ("no VRAM port and no residency directory", its lines
//      118-124), so a composer must SERVE those. `zhao_terrain_compcache_front`
//      cannot: it holds COMPOSED HEIGHTS (top/bottom) and a cell-state plane,
//      not the page's A/B/C layers, and the resident page itself is reachable
//      from in here only through the guard socket entry I26 records as absent.
//      So two of bake's four input groups have no server in this module even
//      if the stamp seam were ratified tomorrow.
//
//      WHAT WOULD BE FIELD ROUTING, SAID SO NOBODY RE-DERIVES IT: the layer-D
//      WRITE half matches. Bake's `cs_event_o`/`cs_sub_o`/`cs_ci_o`/`cs_cj_o`
//      map onto `zhao_terrain_compcache_front`'s `cs_we_i`/`cs_w_substance_i`/
//      `cs_w_ci_i`/`cs_w_cj_i` with only a 6-to-5 bit address narrowing, and
//      that is the port the next paragraph exports. The gap is the block, not
//      that seam.
//
//      AND THE BLOCK TO COMPOSE IS NOT THE ONE THIS ENTRY USED TO NAME.
//      `design/console_inventory.yml` already records `zhao_terrain_bake:
//      superseded_by: zhao_terrain_bake_v2`, and v2's own first lines say it is
//      "PORT-COMPATIBLE with zhao_terrain_bake: same ports, same laws, same
//      counters, same handshake contracts", trading seven private multipliers
//      and 1,089 flops for one operand-muxed multiply and one M10K. Under the
//      ONLY-THE-LATEST-VERSION ruling v2 is what a composition packet may
//      instantiate, and `design/prod_manifest.yml` 474 keeps v1 selected only
//      "until fit gate T1 runs" on the structural DSP prediction. Pointing this
//      entry at `zhao_terrain_bake.sv` would have sent the next packet to
//      compose the superseded file, which is the trap that ruling is about.
//
//      WIDENED 2026-09-19 BY LAYER D (`terr_cc_cs_*`), TERRAIN.COMPCACHE's
//      cell-state write port, and it is the same absent block from the other
//      side: TERRAIN.BAKE emits `cs_event_o`/`cs_sub_o` and is the only thing
//      in the tree that does. TERRAIN.PAGESTREAM reads planes A, B and C out of
//      a page (offsets 64, 2242 and 4420) and does NOT read D, so the page's
//      own copy of the plane has no reader either -- both halves of the gap are
//      real and neither is closed by this packet.
//
//      TIEING IT TO ZERO WOULD HAVE BEEN INVISIBLE, which is why it is a port.
//      spec/terrain_rules.md 3.3 makes 0 = SOLID, so a never-written plane
//      reads as solid rock everywhere and every triangle TERRAIN.TESS emits
//      from it is legitimate-looking terrain. There is no counter that could
//      distinguish "the world is solid" from "nothing ever wrote the world".
//      The cache's `cs_oob_o` is exported beside it, and reads zero because no
//      write is attempted rather than because every write was in range.
//
// I33. PART.TABLE's PER-FRAME LOAD (`part_tbl_ld_*`) -- BOUNDARY. NEW
//      2026-09-19, and it is the SUCCESSOR to the deleted I2/I3 rather than a
//      restatement of them: the descriptor table is built, instantiated and
//      answering all four reads inside this module, and what has no owner here
//      is the HOST THAT FILLS IT. Six ports, one word per clock, never refused
//      for backpressure.
//
//      CORRECTED 2026-09-19. This entry used to say "THE ABSENT OWNER IS
//      CMD.SCHEDULER, the same one I14 and I30 name. `zhao_cmd_decoder` is not
//      composed (see the refusal list below)". BOTH CLAUSES ARE STALE.
//      CMD.DECODER is composed, as section 7b, and CMD.SCHEDULER is not absent
//      at all -- it is `u_sched` inside `u_shell`, running in this composition
//      (the evidence is at entry I36). The conclusion is unchanged and is now
//      the only thing holding the entry open, so it is stated on its own:
//
//      NO RATIFIED COMMAND CARRIES A SPECIES DESCRIPTOR. `spec/commands.zidl`
//      has no opcode with one, which is why CMD.EXEC has no arm for it and
//      says so in its own header. No block in this core produces a descriptor
//      write either. Inventing one here would
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
// I34. TERRAIN.PATCH's FIELD-HEIGHT LANE (`terr_pt_fld_*`) and its section 9.1
//      LIST INTAKE (`terr_pt_fld_add_*`) -- BOUNDARY. NEW 2026-09-19, opened by
//      composing the terrain compose engine (connected item 10).
//
//      THE REASON CHANGED 2026-09-19 AND IT IS NOW A SHARPER ONE. The old text
//      said "THE ABSENT OWNER IS FIELD.SEQ.EARTH and it is not built", citing
//      `tests/terrain/tb_terrain_compose.sv` and the fact that
//      `zhao_field_progcache` had "no sequencer above" it. Both halves of that
//      have moved. `design/contracts/FIELD.SEQ.EARTH.md` rules that owner out
//      of existence -- "one engine, five profiles ... There is no separate
//      FIELD.SEQ.EARTH sequencer in hardware and there is not going to be one"
//      -- and the FIELD v3 fabric is composed here as `u_field_host`, with
//      FIELD.PROGCACHE inside it.
//
//      WHAT ACTUALLY BLOCKS THE HEIGHT LANE IS THE UNIFORMS, and it is a
//      MISSING PRODUCER rather than a missing block. FIELD.SEQ.EARTH.md
//      ratifies the E record exactly: x and z are VARYING and come from the
//      lattice walk, but `age` (R2), `phase` (R3) and `p0..p7` (R4..R11) are
//      UNIFORM and come from the field descriptor. `spec/commands.zidl`
//      TerrainField 0x0200 carries all ten -- `start_tick`, `duration_ticks`
//      and `parameters[64]` -- and the 9.1 list intake beside this entry
//      carries NONE of them: `terr_pt_fld_add_*` is a footprint, a program hash
//      and a command index, and `zhao_terrain_patch.sv` marks the last two
//      "trace only". Running an Earth program with ten zeroed uniform lanes is
//      precisely the thing this entry already forbids for the height itself --
//      a field program applied to every vertex, invisible in the result.
//
//      SO CLOSING IT NEEDS A DESCRIPTOR TABLE keyed by `fld_add_cmd_i`, filled
//      by CMD.EXEC's TerrainField arm, which is the same absent producer entry
//      I42 names for the program store. That is one seam, not two. (The `I42`
//      meant there is THE FIELD ENGINE'S PROGRAM LOADER, below. A second entry
//      was written as I42 the same hour and has been renumbered I44; this
//      citation was one of the two the collision made ambiguous.)
//
//      AND BEFORE BUILDING THE WALKER THAT FEEDS THIS LANE, READ
//      `fpga/rtl/synth/zhao_probe_walk_earth.sv`. Added 2026-09-19 by the
//      terrain sweep, because the file is exactly the shape this repository
//      lost three weeks to once already -- a finished engine kept out of the
//      machine because `probe` was in its filename. It is the Earth LATTICE
//      WALKER, it is differentially tested
//      (`tests/differential/field_walk_earth_directed.cpp`), its own header
//      names its downstream as "ready/valid toward TERRAIN.PATCH's field-major
//      reducer" -- this port -- and it deletes the v2 transport that cost
//      27,225 clocks per association against a 10,416-clock allowance by
//      GENERATING the lattice points from two prepared 33-entry tables.
//      `zhao_probe_patch_acc.sv` beside it is the accumulator of the same
//      chain. This entry does NOT claim they close the lane: the walker takes a
//      PREPARED descriptor, so the uniforms above are still its input and still
//      have no producer, and promoting a file out of `synth/` is the FIELD
//      lane's act and not a terrain packet's. It is named so the next reader
//      searches before building a second one.
//
//      THE REST OF THE ADAPTER IS ALREADY DESIGNABLE and is recorded so the
//      next packet does not re-derive it: the per-lane PROGRAM is knowable
//      without touching TERRAIN.PATCH, because `fld_add_accept_o` is a pulse
//      this module can count and `fld_add_hash_i` is on the same port, so an
//      EARTH adapter can shadow {hash -> slot} per lane through the directory's
//      lookup while the patch keeps the ratified footprint test (its chosen law
//      2). This is still a SEPARATE entry from I5 for the reason I31 gave: the
//      missing piece differs per seam, and the F profile's is a lane binding
//      while this one's is a descriptor producer.
//
//      IT IS NOT TIED OFF AND IT MAY NOT BE. Section 3.4 is
//      `live_top = max(compose_top + SUM field lanes, fx(bottom))`, so a
//      constant on `fld_height_i` is not a neutral value -- it is a field
//      program that raises or lowers every vertex of every patch by the same
//      amount, and it would be invisible because the result is still a real
//      composed height. With `fld_valid_i` LOW there are no lanes at all and
//      the law collapses to `compose_top`, which is section 3.4 with an empty
//      program list: the exact half this composition can honestly carry. That
//      is the difference between an absent input and a faked one.
//
//      THE LIST INTAKE RIDES THE SAME ENTRY because it is the same owner: the
//      per-patch rectangle list is what decides how many lanes a vertex gets,
//      and the block that adds a rectangle is the block that then answers for
//      it. `list_clear_i` is NOT part of this gap -- it is driven from the
//      patch's own issue acceptance inside this module, which is what the
//      block's contract asks for ("once per patch per frame, before the first
//      record").
//
// I36. GEOM.MESHFETCH's DRAW JOB (`geom_mf_job_*`) -- BOUNDARY. NEW
//      2026-09-19, opened by composing the geometry asset path (connected
//      item 11), and it is one of I23's three successors.
//
//      CORRECTED 2026-09-19, TWICE OVER, AND BOTH CORRECTIONS MATTER. This
//      entry used to read "THE ABSENT OWNER IS CMD.SCHEDULER, the same one
//      I14, I30 and I33 name ... and there is no opcode in
//      `spec/commands.zidl` this file could lower into one without choosing
//      the layout itself." Neither half survived being checked.
//
//      CMD.SCHEDULER IS NOT ABSENT. SEARCHED:
//      `fpga/rtl/command/zhao_cmd_scheduler.sv` has been committed since
//      2026-08-16 (e60ba85a), carries a contract
//      (`design/contracts/CMD.SCHEDULER.md`), a directed suite and a formal
//      proof (`tests/formal/cmd_scheduler_slot_fsm.sby`), and is
//      INSTANTIATED as `u_sched` at `zhao_shell_top_v2.sv:725` -- inside the
//      very shell this module instantiates as `u_shell`. It is running in
//      this composition right now. It is ALSO the wrong owner: by its own
//      first line it is "the 3-slot frame ownership FSM", and its dispatch
//      sinks are DEBUG.FRAMEBLIT, INPUT.RUMBLE and VIDEO.MODE. I30 already
//      took this correction for SurfaceStamp; the other three entries had
//      not, and "absent" was doing load-bearing work in all of them.
//
//      THE OPCODE EXISTS AND IS RATIFIED. `DrawForm 0x0300` is
//      `implemented` in `spec/commands.zidl`, 32 bytes, and it carries
//      `handle32[form]`, `handle32[material_set]`, `handle32[transform]`,
//      `viewport_mask`, `semantic_weight` and `flags`, with every offset in
//      `zhao_abi_pkg.sv`. `DrawPopulation 0x0301` and `DrawProcedural
//      0x0302` are `implemented` beside it. So the DISPATCH half is closed:
//      `zhao_cmd_exec`'s draw arm (section 7c) lowers DrawForm whole, and
//      entry I41 is where that dispatch leaves this module.
//
//      WHAT IS STILL OPEN IS A RULING, NOT A WIRE, and it is three of the
//      six job fields. The job is {instance_id, desc_addr, format,
//      generation, active_mask, xform[12]}. DrawForm ratifies
//      `active_mask` (viewport_mask) and `generation` (the handle's own
//      byte). It does not ratify the other three, and nothing else does
//      either:
//
//        * `j_desc_addr_i` needs handle32{index:24} -> a 64-byte aligned
//          pool address. SEARCHED: `spec/memory_rules.md` 5f ratifies the
//          REGION (`ZHAO_RENDER_ASSET_BASE` = 0x06A0_0000, 22 MiB, ENGINE1,
//          read-only) and then says in as many words "Not decided: the
//          pool's internal layout (descriptors vs index streams vs vertex
//          records) ... how it is carved up is the asset fetcher's business
//          and is still open." `design/contracts/GEOM.ASSETFETCH.md`
//          repeats it. There is no `BASE + index*64` law to apply, and
//          writing one here would be this file choosing a memory layout the
//          ABI deliberately declines to define -- the same refusal I33 and
//          I7 carry.
//        * `j_format_i` is the format this reader expects, compared against
//          the descriptor's own byte 0. No command carries it and no
//          registry defines it; it waits on the same ruling.
//        * `j_xform_i[12]` needs the instance-transform palette. SEARCHED:
//          the resolver of this shape is
//          `reference/include/zref/zref_material_resolve.hpp`'s
//          `zref::material::Resolver`, whose RTL is MATERIAL.RESOLVE. THE
//          CITATION THAT STOOD HERE IS A PHANTOM: it read "whose contract's
//          line 4 reads 'RTL: not built'", and that line now reads
//          "RTL: `fpga/rtl/texture/zhao_material_resolve.sv` -- BUILT
//          2026-09-19". The refusal survives on its other half -- the block is
//          BUILT AND NOT COMPOSED, blocked on `spec/memory_rules.md` 5f
//          (entry I20) -- so the port stays, for a reason that is true.
//
//      SO THE JOB PORT STAYS, AND IT IS NOT HALF-DRIVEN. A job is ATOMIC --
//      six fields in one handshake -- so driving the two ratified fields
//      from CMD.EXEC while the other four came from this module's edge would
//      not be a half closure. It would fetch a descriptor at whatever
//      address the boundary happened to be holding, with `j_valid_i` timed
//      by a command and `j_desc_addr_i` timed by nothing, which is the
//      join-between-two-things-that-move-independently fault entry I35
//      recorded (CLOSED and DELETED 2026-09-19 -- zhao_terrain_hdrread is
//      the header reader it asked for) and I39 records still.
//
//      `j_xform_i` IS RESOLVED BY THE CALLER BY CONTRACT, which is why it is
//      a port and not a lookup here: the block's own comment says "the
//      contract's job packet names `instance_transform_id`. Resolving an id
//      to a matrix is a PALETTE LOOKUP, and this block does not own it".
//      GEOM.POSE's palette is composed above and holds BONE matrices for a
//      creature, which is a different table from an instance transform;
//      reading one as the other would be the hidden adapter this file
//      refuses.
//
//      NOT part of this gap: `j_client_i`. See I40.
//
// I37. GEOM.MESHFETCH's DESCRIPTOR CRC VERDICT (`geom_mf_crc_ok_i`) --
//      BOUNDARY. NEW 2026-09-19, and it is a MISSING BLOCK rather than
//      missing wiring, which is why it is its own entry and not a clause of
//      I36.
//
//      The block takes the verdict and does not compute it -- "the CRC over
//      bytes 0..59, folded by the caller's `zhao_crc32c_fold`. Wired in
//      rather than folded here: that block is the one implementation and a
//      second would be a second law." SEARCHED:
//      `fpga/rtl/common/zhao_crc32c_fold.sv` exists and is exactly that one
//      implementation, but it is COMBINATIONAL -- {state, up to eight bytes,
//      a count} in, next state out. What nothing in `fpga/rtl` owns is the
//      WALKER: the thing that runs that fold across the descriptor's beats
//      as they return, stops at byte 60, and compares the result with bytes
//      60..63. That is a state machine with a beat-counting law, and a state
//      machine belongs in a file with a contract and a test, not in this
//      composer.
//
//      IT IS A PORT AND THE FAILURE MODE IS LOUD IN ONE DIRECTION ONLY,
//      which is worth stating because the two directions are not
//      symmetrical. Held LOW, every descriptor is refused, nothing reaches
//      GEOM.ASSETFETCH, and `geom_mf_refused_crc_o` counts every one of
//      them. Held HIGH, a corrupt descriptor is believed. The block's own
//      formal lane already found and fixed the subtle half of this (it
//      recomputed its refusal from the LIVE input instead of latching the
//      verdict, `design/formal_runs.yml`), so what remains is only the
//      absent producer. Taking the verdict from the caller is this tree's
//      standing pattern for exactly this shape -- see
//      `zhao_texture_palette_res`'s `ld_crc_ok_i`, whose END(slot,
//      generation, crc_ok) protocol is the same split.
//
// I38. GEOM.ASSETFETCH's MESHLET RELEASE (`geom_af_release_i`) -- BOUNDARY.
//      NEW 2026-09-19, the third of I23's successors, and it is the same
//      SHAPE as I21's compose-cache retirement one subsystem over.
//
//      The port's owner is whoever knows that BOTH readers have finished
//      with the buffered meshlet, and the block says why it refuses to guess:
//      it is "EXPLICIT rather than inferred from 'all vertices streamed and
//      the last triplet asked for', because two consumers finish
//      independently and a buffer released on a guess is a buffer
//      overwritten under a reader". The two consumers are composed here --
//      GEOM.VDECODE on the vertex stream and GEOM.ASSEMBLE on the index
//      service -- and neither emits a done. GEOM.ASSEMBLE's `t_last_o` marks
//      the last TRIANGLE and says nothing about the vertex run; joining it
//      to a guess about GEOM.VDECODE would be a retirement policy invented
//      in the composer, which is the thing the terrain spine is proud of not
//      having done.
//
//      A CONSEQUENCE WORTH STATING, because it looks like a stall and is
//      not. With nothing driving the release, GEOM.ASSETFETCH fetches ONE
//      meshlet's footprint, hands it over, serves it, and holds in S_SERVE.
//      So `geom_af_meshlets_fetched_o` reaching 1 and stopping, while
//      `geom_af_beats_read_o` shows a whole footprint and
//      `geom_asm_triangles_o` shows the meshlet's triangles, is the EXPECTED
//      reading of an unreleased buffer and not a defect in the chain.
//
// I39. GEOM.ASSEMBLE's THREE DESCRIPTOR FIELDS
//      (`geom_asm_vertex_offset_i`, `geom_asm_material_id_i`,
//      `geom_asm_raster_state_i`) and its TRIANGLE OUTPUT (`geom_asm_t_*`)
//      -- BOUNDARY. NEW 2026-09-19. One entry because they are two ends of
//      one block, and the SAME STANDING I35 had before it closed: the block
//      is composed on its real producer for everything that has one, and the
//      fields that have no owner inside this module are real ports rather
//      than constants.
//
//      WHAT IS REAL. `m_valid_i`/`m_ready_o`, `m_vertex_count_i`,
//      `m_triangle_count_i` and `m_src_id_i` come from GEOM.ASSETFETCH's
//      `s_*` port, and the whole `ix_*` index service is that block's, port
//      for port: nine bits of triplet number out, three u8 local indices
//      back, request/valid with no ready, exactly as both files declare it.
//
//      WHY THE VERTEX OFFSET IS NOT. `m_vertex_offset_i` is a PER-VIEW
//      VERTEX-ID BASE: the block adds it to a u8 local index to name a
//      projected vertex, and its own header says the field is per view
//      because "a single walk emitting into both views gives view 1 the
//      vertices of view 0". The nearby value that looks like it --
//      GEOM.MESHFETCH's `r_vertex_offset_o` -- is a POOL-RELATIVE BYTE
//      OFFSET into the render asset pool, which is what GEOM.ASSETFETCH's
//      own port comment calls it and what that block adds
//      `ZHAO_GEOM_ASSET_BASE` to. Those are different quantities in
//      different spaces, and the one that would produce the first is the
//      arena allocator inside GEOM.PARAMBUF, which has no composed owner
//      (see the refusal list below). Truncating a byte offset to sixteen
//      bits and calling it a vertex id is precisely the rename I13 refuses.
//
//      WHY THE MATERIAL IS NOT, AND IT IS NOT THE OBVIOUS REASON.
//      GEOM.MESHFETCH DOES emit `r_material_id_o` -- but it emits it beside
//      the descriptor it has just read, and by the time GEOM.ASSETFETCH has
//      finished the footprint and offered the meshlet to GEOM.ASSEMBLE, the
//      fetcher's result register has moved on. Wiring them would be a join
//      between two things that move independently and would assemble meshlet
//      N's triangles with meshlet M's material -- the identical fault entry I35
//      recorded for the pageloader's header registers, and the identical answer
//      is available: I35 closed by BUILDING the reader rather than by wiring
//      the stale registers, which is what this entry is waiting for too. Carrying it properly
//      means a field on GEOM.ASSETFETCH's `s_*` port, which is an RTL change
//      to a block with its own differential and is not smuggled into a
//      composition packet. `raster_state` has no producer anywhere.
//
//      THE TRIANGLE OUTPUT's customer is the absent replay block of I11 --
//      the specification written there takes "a TriangleDescriptor
//      {v0,v1,v2} from GEOM.ASSEMBLE", and this is that port, leaving the
//      module so the descriptors are observable rather than dropped.
//
// I40. THE GEOMETRY ASSET PATH's TWO ASSIGNED IDENTITIES -- NOT a tie-off:
//      the core assigns both, in the same standing as I9 and I25.
//
//      THE MEMORY CLIENT (`j_client_i`, `m_client_i`). Both fetchers take
//      one because, in their own words, "no block invents which client it
//      is". In this console the answer is not a choice: `zhao_vram_arbiter`
//      builds the controller's tag by casting the slot index, so slot 3 IS
//      ENGINE1 positionally, and `zhao_mem_guard` grants the render asset
//      pool to ENGINE1 alone. It is the named localparam
//      `GEOM_ASSET_CLIENT_C` rather than a literal so the day client id 5 is
//      spent -- `zhao_pkg` holds it deliberately unspent under ruling T3 --
//      the thing that has to change is greppable. `zhao_geom_mem_adapter`
//      forces the field to ENGINE1 downstream anyway, by its section 11.2,
//      so this value is the truthful one and not the load-bearing one.
//
//      THE MESHLET'S SOURCE ID (GEOM.ASSETFETCH's `m_src_id_i`) is
//      GEOM.MESHFETCH's `r_instance_id_o`, the job's own instance id echoed
//      back beside the meshlet record it describes. The alternative was a
//      separate port on this module's edge, and it was rejected for the
//      reason I39 gives about the material: a draw identity arriving on an
//      independent path can drift from the meshlet it is supposed to label,
//      and a vertex attributed to the wrong draw is a measurement fault no
//      output check can see. The echoed id cannot drift, because it travels
//      in the same handshake as the counts beside it. Listed here because it
//      is a decision taken in the composer: if `src_id` must ever mean
//      something other than the instance that caused the fetch, the owner is
//      whoever owns the draw and this assignment is wrong.
//
// I41. CMD.EXEC's DRAW DISPATCH (`cmd_draw_*`) -- BOUNDARY. NEW 2026-09-19,
//      and it is a gap this packet OPENED DELIBERATELY, by building a
//      producer for a ratified command whose consumer needs a ruling. That is
//      worth saying plainly, because the register's count goes UP by one here
//      and the trade is on purpose.
//
//      WHAT IT IS. `DrawForm 0x0300`, whole: the three handle32s (`form`,
//      `material_set`, `transform`), `viewport_mask`, `semantic_weight`,
//      `flags`, and the record header's `source_id`. Every offset comes from
//      `zhao_abi_pkg.sv` and nothing is dropped. Before this packet the
//      opcode arrived at the console and died: `zhao_cmd_scheduler.sv:384`
//      says "all other opcodes: counted, no dispatch (Phase-2 no-op sinks)",
//      and CMD.EXEC counted it on `unsupported_o`. It now leaves the module
//      as a real, observable stream.
//
//      WHY IT IS A PORT AND NOT A WIRE INTO GEOM.MESHFETCH. Entry I36 is the
//      long answer and it is a MISSING RULING: `spec/memory_rules.md` 5f
//      leaves the render asset pool's internal layout undecided, so there is
//      no law that turns `form`'s 24-bit index into a descriptor address, and
//      MATERIAL.RESOLVE -- the resolver that would turn `transform` into a
//      3x4 -- is BUILT and NOT COMPOSED, for the ruling reason entry I20 now
//      states (this line said "has no RTL" until 2026-09-19; it is the same
//      conclusion reached from a fact that stopped being true).
//      The handles therefore leave as HANDLES, unresolved,
//      which is the honest shape: a consumer that needs the pool layout gets
//      the handle and the ruling it is waiting for, rather than an address
//      this file made up.
//
//      THE DATAPATH BEHIND IT SURVIVES SYNTHESIS, stated positively and
//      deliberately so. `cmd_draw_ready_i` is an INPUT, so the ring, the
//      commit phase and the whole draw arm are live logic that no constant
//      folds away; a consumer that refuses holds the executor in its commit,
//      which `tests/command/cmd_exec_directed.cpp` case 12 drives under three
//      ready patterns.
//
//      THE WORDING OF THAT PARAGRAPH IS LOAD-BEARING and this is the second
//      time this file has paid for it. `completion_register.py` HARD-FAILS on
//      the phrase one would naturally reach for there, because an entry that
//      denies being settled is how an open gap gets read as a closed one --
//      I35 recorded the same trap from the other direction, where writing the
//      phrase marked a live gap CLOSED (that entry is now genuinely closed and
//      deleted, which is why this citation is in the past tense). So the denial does not appear here
//      and the positive statement is used instead.
//
//      THE COUNTERS BESIDE IT HAVE ALL BEEN FIRED, with legal stimulus and no
//      mutant: `cmd_exec_draws_o` (case 9), `cmd_exec_draw_overflow_o` (case
//      10, DRAW_Q+1 forms in one packet) and `cmd_exec_draw_src_truncated_o`
//      (case 11). None is asserted zero here.
//
// I44. TERRAIN.MIPGEN's COARSE-HEIGHT PLANES (`terr_mg_m17_*`,
//      `terr_mg_m9_*`) -- BOUNDARY. NEW 2026-09-19, opened by composing the
//      second completion (connected item 12), and it is the SUCCESSOR to a
//      larger absence rather than a new discovery.
//
//      RENUMBERED FROM I42 LATER THE SAME DAY, AND THE COLLISION IS WORTH ONE
//      PARAGRAPH because it had already made two cross-references ambiguous.
//      Two entries were written as `I42` by two packets in the same hour --
//      this one and THE FIELD ENGINE'S PROGRAM LOADER below -- and both were
//      then cited by number: I21 pointed at `I42` meaning the deviation store
//      (this entry) while I34 pointed at `I42` meaning the program store (the
//      other one). `completion_register.py` keeps its entries in a LIST, so it
//      counted both and reported no defect; the ambiguity was only ever
//      readable by a person, which is the kind a gate cannot catch. This entry
//      moved because I43 was already taken and the FIELD entry is another
//      packet's to edit. I21's citation is updated with it.
//
//      WHAT LEAVES. The 17x17 and 9x9 decimations of the page's height
//      lattice, address and surface beside each value, one write per clock.
//      `spec/terrain_rules.md` 2 calls them "17x17 + 9x9 ... for TERRAIN.LOD"
//      and ruling T8 makes the decimation NESTED so shared vertices stay
//      bit-identical -- the law is settled and the arithmetic is built and
//      tested (`tests/terrain/terrain_mipgen_directed.cpp`).
//
//      WHAT IS ABSENT IS THE STORE, and the owner is not identified. It is
//      1,024 patches x 2 surfaces x (289 + 81) words of height16, about 12
//      Mbit if every resident page keeps both levels -- so it is an SDRAM
//      structure with a residency of its own, not an M10K this composer could
//      add. Nothing in `fpga/rtl` declares it: SEARCHED for a consumer of a
//      17x17 or 9x9 height plane and the only block that names one is
//      TERRAIN.LOD, which takes three PER-SUBPATCH DEVIATIONS (`sp_dev1_i`,
//      `sp_dev2_i`, `sp_dev3_i`) and not the planes themselves. The block
//      between them -- the one that keeps the mips and differences them
//      against the fine lattice to produce a deviation -- has no name in the
//      ledger and no file. Naming CMD.SCHEDULER or TERRAIN.SEQ here would be
//      guessing; the honest statement is that the owner is UNIDENTIFIED.
//
//      WHY THE CHAIN IS COMPOSED ANYWAY, which is the part worth reading. The
//      mip pass is composed for its COMPLETION, not for its planes.
//      TERRAIN.RESIDENCY publishes on two completions and had one, so
//      `resident_o` was structurally zero and the compose door could never be
//      offered a patch (see item 8). TERRAIN.MIPGEN's `done_o` with its
//      {slot, gen, epoch} is that second completion, and it is real whether or
//      not the planes have a home: the block decimates the page it was given
//      and reports on the page it was given. A composition that waited for the
//      store would have kept a working machine switched off for a store
//      nobody has specified.
//
//      THEY ARE PORTS AND NOT DROPPED OUTPUTS for the reason I32 gives about
//      layer D: a decimated height written nowhere and a decimated height
//      written wrongly are indistinguishable from inside this module, and a
//      port is the one place the difference can be seen.
//
// I42. THE FIELD ENGINE'S PROGRAM LOADER (`fld_ld_*`), ITS DIRECTORY PHASES
//      (`fld_pc_*`) AND ITS SECOND CLIENT (`fld_req_*` / `fld_resp_*`) --
//      BOUNDARY. NEW 2026-09-19, and it is ONE entry replacing part of THREE:
//      I5, I31 and I34 each named a different absent FIELD.SEQ.* block, and all
//      three of those blocks are ruled never to exist. What they were all
//      actually waiting on is here.
//
//      THE LOADER. `zhao_field_seq`'s own header says the shell owns the
//      knot tables and uniform bank, and `zhao_field_host` is that shell.
//      Nothing inside this console fills them. The named owner is CMD.EXEC's
//      `TerrainField 0x0200` arm -- `handle32[program] program` naming a
//      cartridge PROGRAM page, spec/cartridge.md 3 kind 0 -- with
//      `zfield::decode` in software producing the one verdict bit the directory
//      takes. That arm is not built; CMD.EXEC's draw arm landed the same day
//      and this is the next one.
//
//      THE DIRECTORY'S TWO PHASES face outward for the reason FIELD.PROGCACHE's
//      contract gives: the decode a miss requires costs orders of magnitude
//      more than the lookup and belongs to the caller. The phase that is
//      INTERNAL is the insert, and it is why the directory is composed inside
//      the engine rather than left at this edge -- it invalidates the program
//      store's slot, so a profile can never run microcode the directory has
//      promised to a new hash.
//
//      THE SECOND CLIENT IS A REAL PORT AND IT IS ALSO AN INSTRUMENT. Client 0
//      is `u_field_stamp_adapter`, internal. Client 1 is at this edge, and it
//      is what makes the arbiter's contention reachable with legal stimulus:
//      `fld_contended_grants_o` cannot be fired by one client, and a counter
//      that cannot be fired is not evidence about the thing it watches. It is
//      also the seam the FLOW and EARTH adapters take over when I5 and I34
//      close.
//
//      WHY NOT A CONSTANT, for each of the three. A tied-off loader leaves the
//      store empty forever, so every profile run returns ST_NO_PROGRAM and the
//      whole engine folds away in synthesis -- this row would then measure
//      nothing. A tied-off directory makes `hdr_loaded` a constant and removes
//      the invalidation edge that is the block's whole reason for being here. A
//      tied-off second client removes the arbiter's reachable contention and
//      with it the only positive control for two of its counters.
//
//      EVERY COUNTER BEHIND THIS PORT HAS BEEN FIRED except one, and that one
//      is structural rather than untested: `fld_ld_oob_o` watches for a write
//      outside a slot's window, and the header clamp that fires
//      `fld_ld_oob_o` is exactly what makes that state unreachable. The
//      other eight are driven by `tests/field/field_host_directed.cpp`.
//
// I43. GEOM.SKIN.NORM's WORLD NORMAL (`geom_sn_n_*`) -- BOUNDARY. NEW
//      2026-09-19, and it is the SUCCESSOR to the lighting seam's first
//      bullet, which is CLOSED. The block is COMPOSED; what leaves is its
//      output, and only its output.
//
//      WHAT CLOSED, AND THE REFUSAL WAS ACCURATE RIGHT UP TO ITS LAST CLAUSE.
//      The lighting section used to say this block's "THREE OPERANDS ARE NEVER
//      SIMULTANEOUSLY VALID IN THIS MODULE" -- the normal valid at
//      GEOM.VDECODE's output, the matrices at the palette store's, "several
//      clocks and one lookup apart, for what may not even be the same vertex".
//      That was exactly true, it named the fix ("`zhao_geom_pose_palette`
//      carrying the normal through beside the vertex"), and it classified the
//      fix correctly as "an RTL change to a block with its own directed test,
//      not a composition". So this entry is not a correction of a wrong
//      refusal: it is a refusal whose stated price was PAID.
//
//      THE RTL CHANGE IS A PAYLOAD AND NOTHING ELSE. `v_nx_i/v_ny_i/v_nz_i` in,
//      `o_nx_o/o_ny_o/o_nz_o` out, captured in R_IDLE by THE SAME ENABLE, on
//      THE SAME CLOCK, as the two bone indices that select the matrices, and
//      held through R_HOLD with the rest of the record. No arithmetic was added
//      to that block and none was added here. The point of putting it inside
//      the store rather than joining it outside is the record-swap law: two
//      quantities a composer pairs from independent paths produce a normal
//      skinned by another vertex's bones, and a lit vertex no output check can
//      distinguish from a correct one.
//
//      THE FORK IS AN AND-FORK AND ITS COST IS MEASURED, NOT ARGUED.
//      GEOM.SKIN and GEOM.SKIN.NORM accept the same beat on the same clock.
//      Neither block's `ready` is a function of its own `valid`, so it cannot
//      deadlock. What it CAN do is throttle, and it does: GEOM.SKIN.NORM is
//      strictly ONE AT A TIME -- S_IDLE, S_MUL x3, S_REDUCE, S_SQ, S_WAIT,
//      S_EMIT with a THIRTY-TWO ITERATION serial root in the middle -- against
//      GEOM.SKIN's one weighted vertex per twelve clocks. That is stated here
//      because the block's OWN comment says the opposite: "this block runs at
//      vertex rate behind GEOM.SKIN's one-per-twelve-clocks, so three clocks of
//      transform is free". The three transform clocks ARE free; the root is not
//      counted in that sentence, and it dominates. `geom_sn_fork_stall_o`
//      counts the cycles the skinner waits, and the composed smoke bench reads
//      63 of them over 4 vertices -- so the number is real, it is on a port,
//      and nobody has to take a comment's word for it.
//
//      WHY THE OUTPUT IS A PORT. Its consumer is GEOM.LIGHT, refused below for
//      reasons of its own, and I42's argument applies unchanged: a world normal
//      written nowhere and a world normal written WRONGLY are indistinguishable
//      from inside this module, and a port is the one place the difference can
//      be seen. The smoke bench uses it as exactly that -- the fixture's normal
//      (127, 0, 0) at w0 = 64 through the identity substitution must give
//      n = (64 * 65536 * 127, 0, 0) and |n| equal to that x component, since the
//      square is perfect and the root is floor-exact. That last equality is the
//      evidence that the `zhao_field_isqrt` instance beside the block answered
//      at all, and an approximation would not satisfy it.
//
//      NOT A GAP, and listed so nobody re-opens it: the ROOT is a second
//      INSTANCE of a block already in this closure, not a second LAW. Both
//      `zhao_geom_skin_norm` and the FIELD engine cite `zref::isqrt_u64`, and
//      the block's own header says a second implementation would be the fault.
//      No source file joined the fit for it.
//
// I45. DEBUG.TRACE's ARMING AND HOST READOUT (`dbg_trace_arm_*`,
//      `dbg_trace_clear_i`, `dbg_trace_rd_*`) -- BOUNDARY. NEW 2026-09-19 with
//      the ring at section 7b-ii, and it is the SMALL half that the composition
//      left open rather than a restatement of the refusal it replaced.
//
//      THE DATA PATH IS CLOSED. CMD.DECODER's record port drives the ring
//      through `zref::trace::Ring::on_record()`'s own mapping; nothing about
//      the event is invented here and I18 carries that argument in full.
//
//      WHAT LEAVES IS CONTROL, AND IT IS EXTERNAL BY DESIGN, not by omission.
//      `arm_mask_i` is a DEBUG COMMAND -- charter 20.6 says the ring is
//      "selectable", the contract calls trace selection a debug command, and
//      the block's own header calls arming "a seven-bit MASK, not a selector,
//      so two stages can be traced in one run". Nothing in this console decodes
//      such a command: `zhao_cmd_exec` has arms for SetView, SurfaceStamp and
//      DrawForm and its `unsupported_o` counts the rest. So this is the same
//      absent owner I14, I30 and I41 name, seen from the debug surface.
//
//      AND UNARMED IS THE CORRECT DEFAULT, WHICH IS WHAT SEPARATES THIS FROM A
//      CONSTANT WEARING A PORT'S NAME. An unarmed stage "is not an event either. It is
//      not stored and NOT counted" -- the block's own words. A ring nobody
//      armed stores nothing, which is precisely what a trace ring does when no
//      trace was asked for. A tie-off is a value invented so a consumer sees
//      something; this is the absence of a request.
//
//      THE READOUT IS I19'S SHAPE EXACTLY. `rd_addr_i`/`rd_data_o` is a host
//      drain -- "writing events into the trace arena is MEM.HPS.BRIDGE's job
//      downstream, which is why this block's output is a stream rather than an
//      address" -- and the same sentence I19 writes about MEASURE.HISTOGRAM
//      applies unchanged: the host is the HPS and no register path from HPS to
//      this block exists. Closing one closes both, and they should be closed
//      together rather than twice.
//
//      SIX OF THE SEVEN STAGES HAVE NO PRODUCER, stated so the composition is
//      not over-read. `kCommandDecoder` (0) is wired. Stages 1..6 -- vertex
//      output, clipped triangle, tile insertion, texture address, depth test,
//      final pixel -- have no offer port in this file, so arming them produces
//      nothing. SEARCHED: no module in `fpga/rtl` has an output group shaped
//      like {tile, primitive, pixel, expected, actual}, and the reason is the
//      one I18 gives and that survives intact -- `expected_fx` against
//      `actual_fx` is a differential against a reference, and the console has
//      no reference. Those six are what MEASURE.HISTOGRAM's `hist_ev_*` wants
//      too. One absent owner, two blocks waiting on it.
//
// ---------------------------------------------------------------------------
// BLOCKS OFFERED TO THIS COMPOSITION AND REFUSED -- the remainder
// ---------------------------------------------------------------------------
// Fourteen of the sixteen blocks in the 2026-09-19 small-blocks packet were
// refused. Ten of them are argued at the entry their port would have closed
// (TWOD.PLANE, TWOD.SPRITE and POST.GATHER at I17; PART.EXPAND, PART.SOFT and
// PART.LADDER at I24; MEASURE.TOKENS and MEASURE.GOVERNOR at I18 -- DEBUG.TRACE
// was in that sentence and is COMPOSED as of 2026-09-19, section 7b-ii, with
// its refusal preserved at I18 and its remaining boundary at I45;
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
//   (FIELD.PROGCACHE was refused here and the refusal is SPENT, 2026-09-19. It
//   said "Both ends are FIELD.SEQ blocks and none of them is composed". The
//   first half was wrong even then: the lookup end is the program LOADER, which
//   is software plus CMD.EXEC, not a FIELD.SEQ block -- that block's own
//   contract says the caller decodes and reports one bit. The second half is
//   what changed: FIELD.SEQ.CORE is composed as `u_field_host`, and the
//   directory is INSIDE it, because the edge that matters is internal. An
//   insert invalidates the store's slot, so no profile can run microcode the
//   directory has already promised to another hash -- the stale-prepared-values
//   failure class the PROGCACHE contract names. Both PHASES still face outward
//   and I42 says who owns them. It was also true that `zhao_field_v2_core` is
//   unconnected; it still is, and it is FROZEN as the fallback, so that is a
//   ruling rather than a gap.)
//
//   THE FORGE CLUSTER -- all four blocks, refused together, RE-ARGUED
//   2026-09-19 because the reason on file had gone stale in the flattering
//   direction. It said their job comes "from CMD.SCHEDULER (absent)". That
//   block is not absent (entry I36), so the refusal needed a real cause and
//   it turns out to have four different ones. Every input of every one of the
//   four is an unsourced boundary in this tree, and `design/blocks.yml` never
//   names a producer for the resource types they consume.
//
//   FORGE.PRIM and FORGE.PRIM_EVAL are the TOPOLOGY and the POSITIONS of one
//   primitive -- indices from one, fx16 vertices from the other -- and they do
//   NOT meet each other: neither has a port the other drives. Both aim at
//   GEOM.SETUP, which takes SCREEN triangles with edge functions.
//
//   AND THE DRAW ARM DOES NOT REACH THEM, which is the useful new fact.
//   `DrawProcedural 0x0302` is the Primitive Forge dispatch and it is
//   ratified -- but `spec/commands.zidl` says of it, in as many words:
//   "forge parameters do NOT travel inline; `program` names the cartridge
//   terrain-patch page (spec/cartridge.md 4 kind 4)". SEARCHED, and kind 4 is
//   `{u16 width; u16 height; fx16 x0,z0,x1,z1; u16 rsv[6]}` plus a
//   heightfield body -- a HEIGHTFIELD, with no field for `j_family`,
//   `j_segments`, `j_sides`, and nothing at all resembling PRIM_EVAL's thirty
//   fx16 anchors, jitter axes, seed and branch descriptors. There is no forge
//   PROGRAM page kind defined anywhere in `spec/cartridge.md`. SEARCHED
//   FURTHER: no RTL in `fpga/rtl` reads a cartridge page of any kind. So the
//   missing owner is a PAGE READER for a page format that does not exist yet,
//   and the same file marks DrawProcedural "[w3] EXECUTED by the software
//   renderer" -- it does not dispatch to these blocks today even on paper.
//   Lowering it in CMD.EXEC would stage a record for nobody.
//
//   FORGE.CLIFF needs a page ISSUER that walks the lattice, a 34x34 solid-bit
//   window and a vdist read master. None exists; TERRAIN.TESS is composed but
//   its output is `terrain_mesh`, a vertex/triangle stream, and it emits none
//   of the three. The only per-cell substance producer in the tree is
//   `zhao_terrain_compcache_front`'s `cs_req_i`/`cs_ci_i`/`cs_cj_i` ->
//   `cs_substance_o`, a SINGLE-CELL indexed query already consumed by
//   TERRAIN.TESS above -- and the block's own comment C1 records rejecting
//   exactly that port, because it "puts this block in contention with
//   TERRAIN.PATCH's own consumers". Nothing anywhere produces a vdist field.
//   Its own output is a RIM EDGE, not a triangle, so even the far end needs a
//   block that is not built.
//
//   FORGE.SHADOW landed 2026-09-19 and is refused for the same shape, listed
//   here because it is new and would otherwise be absent from this record. It
//   needs a shadow CASTER -- {world x, world z, radius, strength, rung,
//   src_id} -- and `design/blocks.yml`'s `shadow_caster` is nobody's output:
//   the string occurs exactly once in all of `design/`, in FORGE.SHADOW's own
//   `inputs:` list.
//
//   RE-SEARCHED 2026-09-19 AND THE NEAREST CANDIDATE NAMED HERE WAS THE WRONG
//   ONE, which matters because the sentence invited the next reader to conclude
//   no world-position-and-radius producer exists. One does. The entry used to
//   say only that "`zhao_geom_lod`'s `rung_o` is the right width and only the
//   rung: that block emits no world position or radius" -- true of that block,
//   and the weakest candidate in the tree. The two strongest:
//     * `zhao_geom_meshfetch` emits `cull_cx_o`/`cull_cy_o`/`cull_cz_o`/
//       `cull_radius_o`, all `signed [31:0]`, which `zhao_geom_cull` calls "the
//       instance bounding sphere, fx16 world", beside `r_instance_id_o[15:0]`.
//       That is a world position and a radius at the exact widths. It is still
//       refused, and now for reasons that survive: it is a point-to-point
//       REQUEST CHANNEL into GEOM.CULL rather than a stream anyone may tap; it
//       carries no strength; and a 3D bounding-sphere cull radius is not a
//       ground-contact footprint radius.
//     * `zhao_cmd_exec` emits `stamp_tx_o`/`stamp_ty_o`/`stamp_radius_o`
//       (`signed [31:0]`) and `stamp_src_id_o[15:0]` -- four of the six fields
//       at exact width. It is disqualified SEMANTICALLY rather than
//       structurally, which is the more dangerous kind of near-miss: that is
//       SURFACE.STAMP's terrain-deformation brush, live in this file, so wiring
//       it would put a contact shadow under every crater and scar and none
//       under a creature. Shape match, wrong source.
//   SO THE BLOCKER IS NARROWER THAN "nobody emits a caster", AND THE VERSION OF
//   IT THIS FILE CARRIED UNTIL 2026-09-19 WAS ITSELF FALSE. It said: "no block
//   emits a RUNG together with a world position, because the creature rung is
//   unported state inside `zhao_geom_meshfetch`'s LodState (`zhao_geom_lod`'s own
//   comment says that block 'holds one LodState per live instance')."
//
//   THERE IS NO LodState IN `zhao_geom_meshfetch`. The strings `rung`, `lod` and
//   `LodState` occur exactly ONCE in all 505 lines of that file, in a header
//   comment at line 18 pointing AT `zhao_geom_lod.sv`. The sentence quoted was
//   `zhao_geom_lod`'s statement about its INTENDED consumer, and that same file
//   says two hundred lines later that "the consumer (GEOM.MESHFETCH's descriptor
//   fetch) is still unbuilt". So the rung is not unported state: NOBODY HOLDS
//   LADDER STATE AT ALL, and `zhao_geom_lod` is a built, stateless evaluator
//   waiting for an owner. This is the fifteenth false-absence claim found in this
//   tree and the first that was false in the ARCHITECTURAL direction -- it named
//   a block as the holder of state that block has never had, which would have
//   sent the next reader to add a port to the wrong module.
//
//   THE THREE CASTER FIELDS, EACH TRACED TO WHAT ACTUALLY STOPS IT:
//
//     * WORLD x/z, RADIUS AND src_id ARE AVAILABLE AND THE JOIN IS PROVABLE.
//       `zhao_geom_meshfetch` is a STRICT SINGLE-IN-FLIGHT state machine --
//       S_IDLE -> S_REQ -> S_VERD -> S_FILL -> S_BOUND -> S_CULL -> S_WAIT ->
//       S_EMIT -- with the instance latched in `inst_q` at accept. So
//       `cull_c{x,y,z}_o`/`cull_radius_o` at S_CULL and `r_instance_id_o` at
//       S_EMIT describe the SAME instance by construction, not by an ordering
//       assumption a workload happens to satisfy. A caster is a FANOUT of those
//       registered outputs plus that join; it does not participate in the cull
//       channel and cannot perturb it, which answers the "point-to-point request
//       channel rather than a stream anyone may tap" objection above.
//     * STRENGTH HAS NO PRODUCER AND MUST NOT ACQUIRE ONE. It is an ART value.
//       CLAUDE.md's rule 6 puts every colour and timing value in a named,
//       editable constant, and a per-rung authored strength IS its correct home.
//       "Nothing emits a shadow strength" was read as a gap; it is a knob nobody
//       has written down yet, which is a different and much smaller thing.
//     * THE RUNG IS THE WHOLE REFUSAL, and it is not a missing block.
//       `zhao_geom_lod` is BUILT and UNIT_VERIFIED. Its five inputs are the
//       refusal, and each was searched:
//         - `proj_radius_q8_i`, a PROJECTED bound radius. The arithmetic is
//           SETTLED and already CALLED in this console: `zhao_part_project`
//           transcribes `zref::render::draw_form_marker`'s world branch bit for
//           bit -- half = |rescale_s32(fx_mul(radius_fx16, d), 8)| with d = 1/w
//           in Q16.16 -- and its header warns that dividing by `d` instead makes
//           markers GROW with distance. So what is missing is `1/w` FOR THE
//           INSTANCE CENTRE, which means a PROJECTOR CLIENT. `zhao_part_project`
//           is already the time-multiplexer in front of client A, so a third
//           stream through it is the same question
//           `reports/OWNER-DOCKET-20260919.md` already asks about a third
//           projector port. FORGE.SHADOW is a second dependent on that decision
//           and nobody knew it.
//         - `thresh_q8_i`, the governor's per-camera pixel-error target.
//           SEARCHED: `zhao_measure_governor` EXISTS and is UNIT_VERIFIED, is
//           NOT composed in this file, and emits per-camera SCALES
//           (`cam0_scale_o`/`cam1_scale_o`, default 16'd256) and no pixel-error
//           threshold at all. The ledger edge GEOM.MESHFETCH `upstream:
//           [..., MEASURE.GOVERNOR]` is an INTENTION, not a port that exists.
//         - `bound_radius_i`, `micro_error_i`, `splat_error_i`, `glint_error_i`:
//           per-creature-TYPE constants. `zhao_geom_meshfetch`'s `cull_radius_o`
//           is the only bound-radius-shaped output in `fpga/rtl` and it is the
//           wrong quantity -- a world-space INSTANCE bound scaled by the
//           instance matrix, not the bind-pose type radius `zref::lod_raw`
//           divides by. NO module emits a micro, splat or glint error.
//
//   AND THE REASON THAT LAST ONE CANNOT SIMPLY BE BUILT IS A RULING, NOT AN
//   OMISSION. Those four live in the compiled creature form page,
//   `spec/creature_rules.md` 5 kind 8 ("LOD ladder refs"), and
//   `spec/cartridge.md` 202-208 says of kinds 8 and 9: "Byte-exact layouts
//   freeze with SW.TOOLS.ASSET at Phase-12 entry (creature_rules 9); until then
//   the packer refuses to emit them (deterministic refusal, never a guessed
//   layout)." There is no layout to read because the project has ruled that
//   there must not be one yet. Building a reader for it would be the guessed
//   layout that sentence exists to forbid.
//
//     - AND A CORRECTION TO THE FORGE.PRIM PARAGRAPH ABOVE, which says "the
//       missing owner is a PAGE READER". That is the right shape for the wrong
//       layer. RE-SEARCHED: no RTL touches a `.zpak` container, a RESOURCE_PAGES
//       record, `page_id`, `byte_length` or a `kind` byte -- zero hits on all
//       four across `fpga/rtl` -- but that is because PARSING THE CARTRIDGE IS
//       SOFTWARE'S JOB BY RULING. `design/contracts/SW.STREAM.md` 35-56 assigns
//       it in as many words ("parse ISLAND_TABLE and the sparse page maps ...
//       stage COMPLETE 21,376-byte pages in HPS DDR ... SW.STREAM stages; the
//       loader fetches"), and hardware reads already-staged page BODIES:
//       `zhao_terrain_hdrread`, `zhao_terrain_pageloader` and
//       `zhao_terrain_writeback` all parse a page-body header per
//       `spec/terrain_rules.md` 2.1. So "no RTL reads a cartridge page" is true
//       and would mislead the next reader into building the wrong block. What
//       FORGE.PRIM actually needs is a forge page KIND with a frozen layout and
//       a staging path, on the terrain pattern -- not a hardware cartridge
//       reader.
//
//   ITS TERRAIN HEIGHT TAPS ARE NO LONGER A REFUSAL. THE BLOCK IS BUILT.
//   This entry used to end: "Across all of `fpga/rtl` -- every one of the 23
//   subdirectories, `synth/` and every `probe`-named file included -- ZERO output
//   ports match a height keyed by a world coordinate ... NOTHING IN THE TREE
//   PERFORMS THE INVERSE. That is the missing half of a tap service, and it is a
//   block rather than a wrapper." It was right, it was the argument for building
//   rather than refusing again, and `fpga/rtl/terrain/zhao_terrain_heighttap.sv`
//   is that block, committed 2026-09-19.
//
//     - It MIRRORS `zhao_forge_shadow.sv`'s `tap_*` port signal for signal, so
//       there is no adapter between them.
//     - The arithmetic is `spec/terrain_rules.md` 4.3 and the directed suite
//       LINKS `zhao_zref` and differentials against `zref::terrain::column_query`
//       ITSELF across all four frozen pitches -- 783 checks -- rather than
//       transcribing the law into a test, which is the duplication CLAUDE.md
//       records for the terrain shade header.
//     - NO DIVIDER. `spec/terrain_rules.md` 1.3 froze the pitch set to powers of
//       two so that world->cell is a shift, and two exact algebraic collapses
//       spend that ruling: with ud == vd == D the cross-multiplied triangle pick
//       is `un >= vn`, and `div_rhu(num, den)` becomes one arithmetic shift with
//       a round-half-up bias. Both are licensed by a runtime check that D is what
//       the pitch says, never assumed.
//     - IT DOES NOT DELAY TERRAIN.TESS BY ONE CLOCK. The compose cache's lattice
//       port has no handshake and its owner cannot be stalled, so the tap does
//       not arbitrate for it: it sits in front as a pass-through and takes only
//       cycles the owner did not want. `tap_stall_clocks_o` is what that costs
//       the tap; the owner pays one 2:1 mux in its address path and nothing else.
//     - AND IT NEVER LEARNS WHICH PATCH IS STAGED, deliberately. Taking a served
//       patch index as a port and comparing would be a detector wired to two
//       operands that move together, since this composer would drive that index
//       from the same sequencer nets that placed the lattice. The containment
//       test is instead the reference's own `un` read back through the RAM: a
//       shift-derived index on one side, TERRAIN.PLACE's stored placement on the
//       other, and they are clocked by different things.
//
//     - THE EARLIER CORRECTION STANDS and is kept: the compose cache's
//       `lat_req_i` is keyed by LATTICE INDEX and carries no void bit, but the
//       block DOES answer a void query on a SEPARATE channel -- `cs_req_i`/
//       `cs_ci_i`/`cs_cj_i` -> `cs_substance_o`, substance 0 is SOLID -- keyed by
//       a 5-bit CELL index. The tap consumes BOTH channels and joins them, which
//       is what "unjoined to the height" was asking for.
//     - AND `zhao_texture_aux` IS STILL THE CLOSEST PRECEDENT and still not the
//       answer: a ready/valid service keyed by `req_wx_i`/`req_wz_i` with two
//       no-answer bits, which makes the CALLER supply the patch envelope and so
//       never resolves world -> patch. The new block does resolve it.
//
//   WHY THE TAP IS BUILT AND NOT COMPOSED, which is the honest state and not an
//   oversight. It has no live consumer yet. FORGE.SHADOW cannot compose until the
//   rung above is settled. Entry I6's PART.COLLIDE is the other customer and IT
//   IS BLOCKED ON AN OWNER DECISION NOBODY HAD WRITTEN DOWN -- see the addition
//   to I6. Composing the tap now would connect nothing at either end, which is
//   the same test TERRAIN.VISIBLE and GEOM.LOOM fail below. It is carried with a
//   disposition in `design/console_inventory.yml` and a `not-yet-adopted` row in
//   `design/prod_manifest.yml` until one of its two customers can take it.
//
//   THE NEW FACT FOR ENTRY I6, recorded here because it was found from this side
//   and I6 is where it must be read. I6 says PART.COLLIDE's normal has a named
//   owner that does not emit one, and that "wiring height alone and inventing a
//   normal would be the hidden-adapter failure". Both true. What is new is that
//   THE NORMAL'S LAW IS NOT MERELY UNIMPLEMENTED, IT IS CONTRADICTED:
//     * `spec/terrain_rules.md` 4.4: "Normals are derived from the composed
//       lattice by FINITE DIFFERENCES at tessellation time";
//     * `design/contracts/TERRAIN.NORMALS.md` 194: "This block emits FACE
//       normals, not vertex normals ... Averaging adjacent face normals into a
//       vertex normal is a real technique and is NOT RATIFIED ANYWHERE ...
//       The vertex-normal question is left open for whoever ratifies it."
//     * and the formats do not meet either: `zref::terrain::face_normal` is
//       DELIBERATELY UNNORMALISED Q16.16 ("the ratified quantity is the
//       unnormalised cross product"), while `zhao_part_collide` declares
//       `NRM_W=12, NRM_Q=10` -- a UNIT normal. The bridge exists
//       (`normalize3_approx`, qformats 7.4) but no contract says PART.COLLIDE's
//       normal is `normalize3_approx(face_normal(...))`.
//   `zref::terrain::column_query` returns no normal at all -- `ColumnResult` is
//   {cls, top, bottom}. So a block holding the four corner heights could produce
//   a normal with one cross product and NO new arithmetic, and it still must not,
//   because WHICH normal is an unratified question with two written answers that
//   disagree. That is an owner decision and it belongs on the docket, not in a
//   composer.
//
//   GEOM.LOOM -- REFUSED, and the description this file carried of it was
//   WRONG on two of three points. It is added to this list 2026-09-19 because
//   until now its only mention was a parenthesis inside entry I11, and a
//   capability the register counts deserves a stated cause rather than an
//   aside. The parenthesis said it "is SKIN -> WARP deformation, and it is in
//   the register's NOT-BUILT list for its own reasons".
//
//   WRONG 1: IT IS NOT DEFORMATION AND IT DOES NOT TOUCH A VERTEX.
//   `zhao_geom_loom` is a STREAMING AFFINE MATRIX COMPOSER over a
//   parent-before-child transform node stream -- ROOT, RIGID, SCALE, ORBIT,
//   AIM, BILLBOARD, OSC, SPLINE, GAIT, FORM -- and its header quotes owner
//   ruling 2026-08-31 6.4: "WHAT SURVIVES IS A STREAMING MATRIX COMPOSER, and
//   this file is exactly that and nothing else." `design/contracts/GEOM.LOOM.md`
//   excludes the rest by name: "no skinning (GEOM.SKIN), no pose decode
//   (GEOM.POSE)". "SKIN -> WARP" is the LEDGER'S EDGE POSITION
//   (`upstream: [GEOM.SKIN]`, `downstream: [GEOM.WARP]`), not a description of
//   the block -- the same declared-edge-is-not-the-RTL's-seam trap entry I21
//   records for the two ports both called `patch_state`.
//
//   WRONG 2: IT IS BUILT. `design/blocks.yml` has it `UNIT_VERIFIED`, dated
//   2026-09-19, commit 80e8d55d, evidence `tests/geometry/geom_loom_directed.cpp`
//   -- 1,006 lines of RTL with a directed test and two extra registered builds
//   for its OVERFLOW control and its DSP knob. It is BUILT AND NOT COMPOSED,
//   which is this repository's own distinction, and calling it not-built is the
//   error that gets a block written twice.
//
//   RIGHT 3, AND IT IS THE WHOLE REFUSAL: NOTHING PRODUCES ITS INPUT AND
//   NOTHING CONSUMES ITS OUTPUT. Its node stream takes `in_node_index_i[9:0]`,
//   `in_parent_index_i[9:0]`, `in_kind_i[3:0]`, `in_param_i[12]` (s32),
//   `in_angle_i[15:0]`, `in_axis_i[1:0]`, `in_bodypatch_i`, `in_first_i`,
//   `in_last_i`. `zhao_geom_skin`'s ENTIRE output is `o_valid_o`, `o_x_o`,
//   `o_y_o`, `o_z_o`, `o_src_id_o` -- not one field of the first is a field of
//   the second, so the ledger's `inputs: [skinned_vertices]` is itself stale
//   against the 6.4 ruling that took vertices out of this block. SEARCHED for
//   the consumer: `warp` appears EIGHT times in all of `fpga/`, every one of
//   them prose, and one of them is the header of `zhao_geom_group_seq` which is
//   literally titled "WHY THIS FILE EXISTS -- AND WHY IT IS NOT GEOM.WARP".
//   There is no GEOM.WARP module, in `fpga/rtl/synth/` or anywhere else.
//   `design/contracts/GEOM.WARP.md` DOES exist -- the ledger's note that it has
//   "no RTL and no contract" is false in its letter -- but every section of it
//   reads "Deliberately unwritten", so it is a stub and the substance holds.
//   (Its stated justification has expired besides: the 2026-08-31 deferral it
//   cites was REVOKED by the owner on 2026-09-18, "I don't want to defer any
//   unfinished blocks now". That is a reason to WRITE the contract, not a
//   reason to compose this block into a machine with neither end.)
//
//   So composing it would connect nothing and open new tie-off entries at both
//   ends, which is the same test TERRAIN.VISIBLE fails above. It is carried as
//   a production fit top instead, so that it gets PRICED -- 48 clk/node at
//   MUL_LANES=1 for 3 DSP, 23 at MUL_LANES=3 for 9 -- and that is the right
//   place for it until GEOM.WARP has RTL.
//
//   GEOM.PARAMBUF is the ENGINE1 arena's RECORD LAYER -- 24-byte
//   ProjectedVertex, 16-byte TriangleDescriptor and 64-byte tile-reference
//   chunk, bytes in and fields out, with the s21 legality rule and the
//   chunk's frame-generation staleness gate. Its three inputs are records
//   READ BACK OUT of that arena, and SEARCHED: nothing in `fpga/rtl` writes
//   one into memory. RE-SEARCHED 2026-09-19 and the refusal stands, with one
//   sentence added because the next reader will otherwise think it is stale.
//   Every geometry memory client is hard-coded READ-ONLY in source --
//   `zhao_geom_meshfetch.sv:319`, `zhao_geom_assetfetch.sv:341` and
//   `zhao_geom_mem_adapter.sv:190` all assign `write = 1'b0`, the last with the
//   comment "the asset window is READ-ONLY by construction" -- and the only
//   five blocks in the whole tree that assert `write = 1'b1` are RASTER.FBWRITE,
//   DEBUG.FRAMEBLIT, MEM.UPLOAD and two terrain paths. None is geometry, and
//   `spec/memory_rules.md` 5f declares RENDER.ASSET_POOL read-only with a formal
//   assertion (`a1_render_asset_ro`) to match, so a writer today would be built
//   against a region the guard is PROVEN to refuse.
//
//   AND `zhao_geom_arena.sv` IS NOT THIS ARENA'S ALLOCATOR, said explicitly
//   because it is a file with "arena" in its name containing a real bump
//   allocator, and finding it is how somebody concludes this entry is out of
//   date. It is GEOM.BINNER's, by its own first line, and the widths settle it:
//   `PTR_W = 8` over `CHUNKS = 256` against this block's 32-bit `next_chunk`
//   over `ARENA_CHUNKS = 65536`. An 8-bit pointer cannot name a 64 Ki-chunk
//   arena. `zhao_geom_binner_v2` has no memory port at all.
//
//   THE LEDGER CORROBORATES THE ABSENCE FROM A SECOND, INDEPENDENT DIRECTION,
//   which is worth more than the port search repeated. `design/blocks.yml`
//   declares four counters for this block -- `parambuf_records_written`,
//   `parambuf_chunks_allocated`, `parambuf_stale_handles`,
//   `parambuf_overflow_frames` -- and all four strings appear in that file and
//   in NO `.sv` FILE IN THE REPOSITORY. The RTL exports `pv_illegal_count_o`,
//   `td_illegal_count_o`, `ck_stale_count_o` and `ck_illegal_count_o` instead.
//   A counter named *records_written* that no RTL drives is the ledger
//   describing a writer nobody built, and it agrees with the port search
//   without sharing an operand with it.
//
//   The block's own header says the same thing from the inside: "It does not
//   own SDRAM, does not arbitrate, and does not allocate the arena", and hands
//   the capacity policy, the quota seal and the frame-fault path to "the
//   composed block's" -- a block that does not exist.
//
//   GEOM.ASSEMBLE, composed below, emits a
//   TriangleDescriptor's FIELDS, which is this block's job run backwards --
//   pairing the two would be an encode immediately undone by a decode with
//   no memory between them, which is a disconnected implementation with
//   extra steps whatever the handshakes did. The absent owner is the arena's
//   WRITER and its allocator, which is the same one entry I39 names for
//   GEOM.ASSEMBLE's per-view vertex-id base, one level up.
//
//   GEOM.DEPTHQUANT is refused for a HANDSHAKE and not for a missing
//   producer, which is the opposite of what its entry would have said a day
//   ago and is worth writing out because the producer half is genuinely
//   closed. `zhao_proj_subsystem` is composed above and emits `w` twice
//   over: per vertex on client A's `a_w_o`, and per triangle on
//   `out_aw_o`/`out_bw_o`/`out_cw_o` -- its own header ends the list of what
//   it exposes with "which is what GEOM.CLIP consumes today and what
//   GEOM.DEPTHQUANT needs". Three things stand between that and a wire:
//
//     * THE PER-VERTEX PORT CANNOT BE BACK-PRESSURED. Client A's result port
//       has no `ready`; it is a PUSH into `zhao_geom_proj_lane`'s arena, and
//       `zhao_project_core` is fully pipelined at ONE VERTEX PER CLOCK (its
//       stage-5b note: "LATENCY, NOT INITIATION INTERVAL ... a vertex still
//       enters every cycle"). GEOM.DEPTHQUANT is strictly one at a time --
//       S_IDLE, S_RCP, S_WAIT, S_COMB, S_HOLD, with a reciprocal in the
//       middle -- so a tap there would silently drop vertices with every
//       handshake legal and every counter balancing. "It keeps up at the
//       rate GEOM.SKIN actually issues" is a WORKLOAD argument about a
//       structural hazard, which is the shape of reasoning this file's own
//       rules refuse.
//     * THE TRIANGLE PORT HAS THE HANDSHAKE AND THE WRONG ARITY. `out_*` is
//       ready/valid and carries all three corners' `w` with their behind
//       bits and a source id -- but it presents three corners in ONE beat
//       and the block takes one vertex. The thing between them is a
//       three-corner serialiser with a reply join, which is a state machine,
//       which belongs in a file with a contract and a test and not in this
//       composer. That is the same sentence entry I11 writes about the
//       replay customer, and it is the same missing block seen from the
//       depth side.
//     * AND THE RECIPROCAL SERVICE WOULD HAVE TO COME WITH IT.
//       `rcp_valid_o`/`rcp_ready_i`/`rcp_d_o` and
//       `rcp_rvalid_i`/`rcp_rready_o`/`rcp_r_i`/`rcp_k_i` match
//       `fpga/rtl/raster/zhao_raster_rcp24.sv` port for port -- SEARCHED,
//       and that is the golden implementation and the oracle.
//
//       CORRECTED 2026-09-19, because the clause that followed -- "but it is
//       not in this closure" -- IS NO LONGER TRUE IN ITS LETTER, and a reader
//       checking it would find the opposite and distrust the rest.
//       `zhao_raster_rcp24_v4` IS in this console's closure today, and it is
//       the right one to look at besides: under the owner's "only the latest
//       version" ruling, `zhao_raster_rcp24` is not the block a new client
//       would be given. Its reply port carries `r_o[23:0]` and `k_o[5:0]`,
//       which is this block's `rcp_r_i`/`rcp_k_i` exactly.
//
//       THE REFUSAL SURVIVES ON ITS SUBSTANCE, which was never really about
//       the file being absent: there is no reciprocal service with a SPARE
//       CLIENT PORT. `zhao_raster_rcp24_v4` is instantiated exactly once in
//       the whole tree -- `zhao_texture_island_v3_top:922`, at NCTX = 12 --
//       and it has ONE request port, which the texture island owns. Serving
//       GEOM.DEPTHQUANT from it means an arbiter between two clients written
//       in this composer, which is the thing the terrain spine's own note is
//       proud of not having done; serving it from a SECOND instance means
//       paying for a second reciprocal on a device over on both ALM and DSP,
//       to feed a block the two bullets above already refuse for arity and
//       backpressure. Either way the handshake argument is what decides it,
//       and that is unchanged.
//
//       Worth keeping the distinction: "the file is not in the closure" is a
//       claim that expires the moment somebody composes the file, and this one
//       did. "No client port is free" is a claim about the arrangement, and it
//       is the one that was doing the work all along.
//   `v_profile_i` is additionally SetView's `flags[1:0]`. CLOSED 2026-09-19:
//   `zhao_project_core` has cfg address 18 and emits `out_profile_o`, and this
//   module re-exports it as `proj_a_profile_o`. What is still missing is THIS
//   BLOCK, not the field -- see entry I14.
//
//   GEOM.PROJECT IS NOT REFUSED AND IT IS NOT COMPOSED, which is a third
//   thing and the only one of its kind in this file. `zhao_geom_project.sv`
//   is BY ITS OWN HEADER "a thin shell" over
//   `fpga/rtl/common/zhao_project_core.sv` -- "a ready/valid handshake, the
//   accepted-vertex counter, and nothing else" -- and that core is already
//   in this composition, inside `u_proj_subsystem`, serving GEOM on client A
//   and TERRAIN on client B. Instantiating the shell as well would put a
//   SECOND `zhao_project_core` in the console: about 6,199 ALM and 33 DSP,
//   spent to re-do arithmetic the console already performs, in a design
//   whose binding constraint is ALMs. That is the deduplication campaign
//   undone to satisfy a ledger row.
//
//   THE LEDGER ALREADY KNOWS THIS, AND IT HAS ALREADY RULED WHEN TO ACT ON
//   IT, which is why nothing about the ledger is changed here.
//   `design/prod_manifest.yml` says of `zhao_geom_proj_lane` that
//   "selecting one shared service instead of two wrappers (6,598 ALM / 33
//   DSP against ~12,400 / 66) needs this composed AND a producer driving
//   it", and of `zhao_geom_group_seq` that the producer now exists but
//   "the selected census still counts zhao_geom_project and
//   zhao_terrain_project separately, and changing that is one deliberate
//   edit after the composed fit closes". Both preconditions except the fit
//   are met. THE FIT IS NOT THIS PACKET'S TO SPEND, and flipping a census
//   onto an arrangement nobody has measured is precisely what that row
//   gated. So what this file adds is the observation and not the act: the
//   capability is already PRESENT -- client A of `u_proj_subsystem` is
//   `zhao_geom_project`'s port shape, vertex for vertex, minus the
//   accepted-vertex counter -- and the remaining work is an accounting edit
//   with a named precondition rather than a composition.
//
//   RESOLVED IN THE REGISTER 2026-09-19 BY OWNER RULING R3
//   (`reports/OWNER-RULINGS-20260919-EVENING.md`: "Keep the time-multiplex.
//   No third port in v1."). GEOM.PROJECT now resolves to `zhao_proj_subsystem`
//   in `completion_register.py`'s `_ALIAS`, with four witnesses beside it, and
//   `zhao_geom_project` carries `superseded_by: zhao_proj_subsystem` in
//   `design/console_inventory.yml`. The CENSUS edit above is untouched: it is
//   a different question (what the fit prices) with its own precondition.
//
// ---------------------------------------------------------------------------
// LIGHTING SEAM -- STILL NOT CONNECTED, FOR THREE NEW REASONS
// ---------------------------------------------------------------------------
// CORRECTED 2026-09-19. This section used to read: "`zhao_geom_light.sv` and
// every `zhao_light_*` file are being refactored into a lighting service
// while this file is written, so they are EXCLUDED from this composition on
// purpose". THAT REFACTOR HAS LANDED, so the exclusion's stated reason had
// outlived its cause and the next reader would have been waiting for work
// that was already done. SEARCHED: `zhao_light_stream.sv` is the streamed
// service (commit fb3d30f4, "GEOM.LIGHT: streamed lighting service at II2"),
// `zhao_geom_light.sv` is a shell AROUND the shared light engine (d4f837d8),
// and `zhao_light_skin_adapter.sv` is the asserted narrowing between
// SKIN.NORM's {direction:s64x3, magnitude:u64} and the service's s32/u32
// prepared form. Nothing is mid-flight.
//
// The seam is still not connected, and the reasons are now specific:
//
//   * GEOM.SKIN.NORM's THREE OPERANDS -- CLOSED 2026-09-19, see entry I43.
//     The paragraph that stood here was right in every particular: the normal
//     was GEOM.VDECODE's and the matrices `zhao_geom_pose_palette`'s, and the
//     store's pass-through payload did not carry the normal, so the two were
//     several clocks and one lookup apart, for what may not even be the same
//     vertex. It named the fix and priced it as an RTL change rather than a
//     composition. The change is made -- the store carries the normal now,
//     captured by the same enable as the two bone indices -- and the block is
//     composed on an AND-fork beside GEOM.SKIN. Its OUTPUT is the gap now,
//     which is a narrower statement than this one was.
//   * GEOM.LIGHT's DESCRIPTOR BANK HAS NO PRODUCER -- the conclusion is TRUE
//     and the REASON GIVEN HERE WAS FALSE, corrected 2026-09-19. It read: "No
//     opcode in `spec/commands.zidl` carries any of it." SEARCHED, and naming
//     what was searched is the point: `spec/commands.zidl:522` defines
//     `SetEnvironment 0x0311` with `angle16 sun_yaw`, `angle16 sun_pitch`,
//     `rgb565 sun_colour`, `rgb565 ambient`, `rgb565 tint`, `u8 tint_strength`
//     and the fog fields -- a sun direction, a sun colour and an ambient,
//     roughly HALF this bank. `zhao_abi_pkg.sv:93` already declares
//     `ZHAO_OP_SET_ENVIRONMENT`, so the opcode is visible to RTL today.
//
//     The sentence was the flattering simplification of one this tree already
//     had RIGHT: `zhao_cmd_exec.sv:40-43` says 0x0311 "is the nearest thing in
//     the opcode space and it carries sun, ambient, tint and fog -- no
//     geometry -- and it is `reserved`, not `implemented`, so it has no
//     execution semantics to borrow even if it did". Two files in this tree
//     disagreed about a checkable fact, and the looser one was the one doing
//     the refusing.
//
//     AND THE REAL BLOCKER IS AN OWNERSHIP AND FORMAT CONFLICT, which has to be
//     RULED rather than built, and is a harder thing than a missing command.
//     `spec/sky_and_beams.md` 4a assigns vertex light to GEOM.PROJECT, not to
//     GEOM.LIGHT, and ratifies a ONE-SUN rgb565 model -- ndl = clamp(N.L, 0, 1)
//     then lit = sat_u8(ambient_c + rescale_u(sun_c * ndl, 8)) -- saying in as
//     many words "no dynamic point lights in the format (the donor never had
//     them)". `zhao_geom_light` implements an EIGHT-LIGHT, ten-word Q16.16 bank
//     with emission and spill and an `nlights_i`; normal detail, emission,
//     spill and `nlights` have no ABI representation at all. The numeric forms
//     do not match either -- an angle16 pair against an s32 direction vector,
//     rgb565 against u20 Q16.16. Wiring 0x0311 into `cfg_*` would be choosing
//     between two ratified laws inside a composition packet.
//     `design/blocks.yml`'s own GEOM.LIGHT row carries the other half of the
//     contradiction: "the ledger's description of GEOM.PROJECT as
//     projection-plus-lighting is aspirational".
//
//   * AND `zhao_geom_light` IS THE SUPERSEDED IMPLEMENTATION BESIDES, which is
//     the finding that matters most here and is new on 2026-09-19. Even if the
//     bank had a producer, THIS is not the block to wire it to.
//     `fpga/rtl/geometry/zhao_light_stream.sv` opens "THIS REPLACES THE OWNER,
//     IT DOES NOT ADD A SECOND LAW", and prices what it replaces:
//     "`zhao_geom_light.sv` is the scalar arrangement: one `zhao_terrain_shade`
//     turn per light term, MEASURED at II = 167.0 clocks, which is 48.1x over
//     the frame for the ruled 120,000-vertex / 480,000-term stress profile."
//
//     So composing `zhao_geom_light` would be the owner's 2026-09-19 ruling
//     broken exactly as it was broken for FIELD -- fitting a machine nobody
//     ships, spending ALM and DSP on dead weight on a device already over on
//     both. And it would not be CAUGHT, because
//     `completion_register.superseded_in_closure()` matches a version SUFFIX or
//     INFIX and `zhao_light_stream` is neither: it supersedes by RENAME, a
//     third shape the tree now has and no tool looks for. The capability
//     GEOM.LIGHT resolves to `zhao_geom_light` in the register, so "connected"
//     is today satisfiable by wiring the superseded block and the instrument
//     would say fine -- the identical defect CLAUDE.md records for
//     FIELD.SEQ.CORE resolving to `zhao_field_v2_core`.
//
//     `design/console_inventory.yml` now carries the disposition, so the gate
//     holds the ruling even though the register cannot see it. What is NOT done
//     here, and is named so it is not mistaken for done: `design/blocks.yml`'s
//     GEOM.LIGHT row still reads `maturity: SPECIFIED`, `superseded_by: null`
//     and both tests "PLANNED -- NOT WRITTEN", which is stale on all three
//     counts. Correcting a ledger row is not this packet's to do quietly while
//     two other packets hold that file open.
//   * ITS OUTPUT IS A SHELL-SIDE CHANGE. The RGB term goes into the raster
//     material stage inside `zhao_geom_bin_pipe_v2`, so the seam is not
//     purely additive to this file and should be planned with I15.
//
// So no `zhao_light_*` file appears in this composition's source closure.
// This core contains ZERO lighting logic and its resource number contains
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

  // ---- GEOMETRY: the asset fetch path -------------------------------------
  // ONE pair of limits for TWO blocks, written once for the same reason
  // TWOD_LINE_W is: GEOM.ASSETFETCH sizes the private buffer it fills from
  // these, and GEOM.ASSEMBLE decides which local index is legal against the
  // same numbers. Two blocks disagreeing about how big a meshlet may be is a
  // walk off the end of a buffer that every handshake calls legal, so they
  // are one expression rather than two literals. Both are the owning blocks'
  // own defaults (GEOM.MESHFETCH.md's ruling limits: a u8 local index cannot
  // address past 255, and 126 triangles x 3 indices is 378 bytes).
  parameter int unsigned GEOM_ASSET_MAX_VERTICES  = 64,
  parameter int unsigned GEOM_ASSET_MAX_TRIANGLES = 126,
  // GEOM.ASSEMBLE's vertex-id width. 16 is NOT a free choice and is named
  // here so it reads as the constraint it is: it is GEOM.PARAMBUF's
  // TriangleDescriptor field (`vertex_id[3] u16`, ruling R7), so widening it
  // would emit a descriptor the record layer cannot store.
  parameter int unsigned GEOM_ASM_VIDW = 16,

  // ---- GEOMETRY: the clip/setup triangle front door ------------------------
  // `zhao_geom_clip`'s ruling-5 attribute packet: invw24, u_over_w, v_over_w,
  // lit r/g/b and alpha. The block never interprets them; it only keeps them
  // with their vertices across the winding flip. ATTRW is the flattened width
  // and exists because a port list cannot call $clog2 on another port.
  parameter int unsigned GEOM_CLIP_ATTRS = 7,
  parameter int unsigned GEOM_CLIP_ATTRW = GEOM_CLIP_ATTRS * 32,
  // WHICH SLOT OF THAT PACKET CARRIES WHICH PACKET-D PLANE. Named constants
  // rather than literals inside `u_geom_attrpack`, because CLAUDE.md's rule is
  // that a ratified layout is still a knob: "this is generated from the
  // reference, so it is not a knob" is how a wrong number becomes an
  // unadjustable wrong number. The order is `zhao_geom_clip`'s ruling 5 and
  // `tests/geometry/geom_clip_attrswap_directed.cpp`'s own line 41.
  parameter int unsigned GEOM_ATTR_SLOT_INVW     = 0,
  parameter int unsigned GEOM_ATTR_SLOT_U_OVER_W = 1,
  parameter int unsigned GEOM_ATTR_SLOT_V_OVER_W = 2,

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

  // ---- THE GEOMETRY ASSET PATH (connected item 11) -------------------------
  // I23's four ports -- `geom_vd_v_valid_i`, `geom_vd_v_ready_o`,
  // `geom_vd_v_bytes_i`, `geom_vd_v_src_id_i` -- LEFT THIS LIST on 2026-09-19
  // rather than being driven: GEOM.ASSETFETCH is composed below and is that
  // port's real producer. What follows is what the path still asks of the
  // outside, and each group is one numbered entry in the header.

  // ---- I36: GEOM.MESHFETCH's DRAW JOB -------------------------------------
  input  logic                    geom_mf_job_valid_i,
  output logic                    geom_mf_job_ready_o,
  input  logic [15:0]             geom_mf_job_instance_id_i,
  input  logic [26:0]             geom_mf_job_desc_addr_i,
  input  logic [7:0]              geom_mf_job_format_i,
  input  logic [15:0]             geom_mf_job_generation_i,
  input  logic [1:0]              geom_mf_job_active_mask_i,
  input  logic signed [31:0]      geom_mf_job_xform_i [0:11],

  // ---- I37: the descriptor's CRC VERDICT ----------------------------------
  input  logic                    geom_mf_crc_ok_i,

  // ---- I38: GEOM.ASSETFETCH's meshlet RELEASE -----------------------------
  input  logic                    geom_af_release_i,

  // ---- I39: GEOM.ASSEMBLE's three descriptor fields -----------------------
  input  logic [GEOM_ASM_VIDW-1:0] geom_asm_vertex_offset_i,
  input  logic [15:0]              geom_asm_material_id_i,
  input  logic [31:0]              geom_asm_raster_state_i,

  // ---- I39: GEOM.ASSEMBLE's TriangleDescriptor, out to the absent replay --
  output logic                     geom_asm_t_valid_o,
  input  logic                     geom_asm_t_ready_i,
  output logic [GEOM_ASM_VIDW-1:0] geom_asm_t_v0_o,
  output logic [GEOM_ASM_VIDW-1:0] geom_asm_t_v1_o,
  output logic [GEOM_ASM_VIDW-1:0] geom_asm_t_v2_o,
  output logic [15:0]              geom_asm_t_material_o,
  output logic [31:0]              geom_asm_t_raster_o,
  output logic [15:0]              geom_asm_t_src_id_o,
  output logic                     geom_asm_t_last_o,

  // ---- I41: CMD.EXEC's DRAW DISPATCH, the ratified DrawForm ---------------
  // NEW 2026-09-19. DrawForm 0x0300 now reaches the console; what has no
  // consumer INSIDE this module is the resolver that would turn its three
  // handles into GEOM.MESHFETCH's job. See the header entry.
  output logic                     cmd_draw_valid_o,
  input  logic                     cmd_draw_ready_i,
  output logic [31:0]              cmd_draw_form_o,
  output logic [31:0]              cmd_draw_material_set_o,
  output logic [31:0]              cmd_draw_transform_o,
  output logic [ 7:0]              cmd_draw_viewport_mask_o,
  output logic [ 7:0]              cmd_draw_semantic_weight_o,
  output logic [15:0]              cmd_draw_flags_o,
  output logic [15:0]              cmd_draw_src_id_o,
  output logic [31:0]              cmd_exec_draws_o,
  output logic [31:0]              cmd_exec_draw_overflow_o,
  output logic [31:0]              cmd_exec_draw_src_truncated_o,

  // ---- the asset path's evidence ------------------------------------------
  // GEOM.MESHFETCH's seven refusal rows are exported SEPARATELY rather than
  // as the block's `refused_o [7]`, in the block's own documented order
  // (format, crc, generation, vertex_count, triangle_count, reserved,
  // zero_bound). One counter for all seven would name none of them, which is
  // that block's own argument for keeping them apart.
  output logic [31:0] geom_mf_meshlets_considered_o,
  output logic [31:0] geom_mf_culled_all_cameras_o,
  output logic [31:0] geom_mf_descriptors_fetched_o,
  output logic [31:0] geom_mf_guard_denied_o,
  output logic [31:0] geom_mf_refused_format_o,
  output logic [31:0] geom_mf_refused_crc_o,
  output logic [31:0] geom_mf_refused_generation_o,
  output logic [31:0] geom_mf_refused_vertex_count_o,
  output logic [31:0] geom_mf_refused_triangle_count_o,
  output logic [31:0] geom_mf_refused_reserved_o,
  output logic [31:0] geom_mf_refused_zero_bound_o,

  // GEOM.MEM_ADAPTER: `geom_ma_contention_o` is the number that says what
  // sharing ONE ENGINE1 client between two fetchers actually costs, and it
  // could not move until both of them were behind it.
  output logic [31:0] geom_ma_jobs_a_o,
  output logic [31:0] geom_ma_jobs_b_o,
  output logic [31:0] geom_ma_denied_o,
  output logic [31:0] geom_ma_contention_o,
  output logic [31:0] geom_ma_err_short_o,
  output logic [31:0] geom_ma_err_long_o,
  output logic [31:0] geom_ma_err_unowned_o,

  output logic [31:0] geom_af_meshlets_fetched_o,
  output logic [31:0] geom_af_beats_read_o,
  output logic [31:0] geom_af_guard_denied_o,
  output logic [31:0] geom_af_refused_footprint_o,
  output logic [31:0] geom_af_prefetch_stall_o,
  output logic [31:0] geom_af_err_beat_truncated_o,
  output logic [31:0] geom_af_err_beat_overrun_o,
  output logic [31:0] geom_af_err_beat_unowned_o,

  output logic [31:0] geom_asm_meshlets_o,
  output logic [31:0] geom_asm_triangles_o,
  output logic [31:0] geom_asm_refused_limits_o,
  output logic [31:0] geom_asm_refused_index_o,

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

  // ---- GEOM.SKIN.NORM's world normal and evidence --------------------------
  // NEW 2026-09-19. GEOM.SKIN.NORM is COMPOSED below, and this is its OUTPUT
  // leaving the module because its consumer does not exist -- see entry I43.
  // The block's INPUTS are all real: the packed normal is GEOM.VDECODE's and
  // the two matrices are GEOM.POSE's palette store's, arriving in one
  // handshake because the palette now carries the normal through beside them.
  //
  // It is a PORT and not a dropped output for the reason I42 gives about the
  // mip planes: a world normal computed and written nowhere and a world normal
  // computed WRONGLY are indistinguishable from inside this module, and a port
  // is the one place the difference can be seen.
  output logic                    geom_sn_n_valid_o,
  input  logic                    geom_sn_n_ready_i,
  output logic signed [63:0]      geom_sn_n_x_o,
  output logic signed [63:0]      geom_sn_n_y_o,
  output logic signed [63:0]      geom_sn_n_z_o,
  output logic [63:0]             geom_sn_n_mag_o,
  output logic                    geom_sn_n_degenerate_o,
  output logic [15:0]             geom_sn_n_src_id_o,
  output logic [31:0]             geom_sn_vertices_o,
  output logic [31:0]             geom_sn_degenerate_o,
  output logic [31:0]             geom_sn_reduced_o,
  // The fork's own cost, made visible rather than argued. It counts cycles in
  // which GEOM.POSE's palette held a vertex that GEOM.SKIN was ready for and
  // GEOM.SKIN.NORM was not. See I43 for why that number is expected to be
  // large and what it means.
  output logic [31:0]             geom_sn_fork_stall_o,

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
  // GEOM.ATTRPACK's two counters, out of the module for the same reason every
  // other block's are: a counter nobody can read is not evidence. Their RATIO
  // is the thing worth asserting -- `planes` must be exactly three times
  // `triangles`, because one shared attrsetup core runs three lanes per
  // triangle, and a lane that quietly stopped asking would leave every
  // handshake and every other counter looking perfectly healthy.
  output logic [31:0]             geom_attrpack_triangles_o,
  output logic [31:0]             geom_attrpack_planes_o,

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

  // TERRAIN.TESS's lattice and cell-state read ports USED TO BE HERE, as entry
  // I22.  `zhao_terrain_compcache_front` is composed below and its serve side
  // drives them, so the eleven ports are gone from this list rather than being
  // driven by a harness.  The retirement pulse the cache's serve side needs --
  // `terr_cc_serve_release_i` -- is further down with the rest of the compose
  // engine's boundary, because its owner is the block that issues the subpatch
  // jobs (entry I21) and not the cache.

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

  // ---- I27 (narrowed): the directory's deformation and handle-check ports --
  //      The COMPOSE DOOR (`terr_is_*`) and the UNPIN (`terr_unpin_*`) left this
  //      list on 2026-09-19: TERRAIN.SEQ's issue now reaches TERRAIN.PAGESTREAM
  //      and TERRAIN.PLACE inside this module, and the streamer's own completion
  //      is what unpins the page.  What is left here is the deformation mark,
  //      whose writer is TERRAIN.BAKE (entry I32), and the handle check, whose
  //      caller is the same absent subpatch issuer as entry I21.
  input  logic                    terr_dm_valid_i,
  output logic                    terr_dm_ready_o,
  input  logic [TERR_SLOTW-1:0]   terr_dm_slot_i,
  input  logic [TERR_GENW-1:0]    terr_dm_gen_i,
  input  logic [31:0]             terr_dm_epoch_i,
  input  logic                    terr_dm_bd_i,
  input  logic                    terr_dm_f_i,
  input  logic                    terr_dm_mips_i,

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

  // ==========================================================================
  // THE TERRAIN COMPOSE ENGINE'S OWN BOUNDARY (connected item 10)
  // ==========================================================================

  // ---- I26 (extended): THE COMPOSE PATH's ONE MEM.GUARD READ client -------
  // A SECOND guard client, not a second opinion about the first.
  // TERRAIN.PAGELOADER's client above WRITES a page into the pool; this one
  // READS the same page back out, so it needs the return path
  // (`beat_valid/data/last`) a write client has no use for.  The shell exposes
  // one guard socket and it is named for GEOM, so both stop here -- see entry
  // I26 for why that is a REACHABLE boundary and not I23's refusal.
  //
  // STILL ONE PORT AFTER TERRAIN.HDRREAD LANDED, 2026-09-19, and that is the
  // point of the block behind it.  TWO readers now sit on this socket --
  // TERRAIN.PAGESTREAM's three plane bursts and TERRAIN.HDRREAD's one header
  // burst -- joined by `zhao_mem_share2`, the SAME block GEOM.MEM.ADAPTER is a
  // wrapper over.  The arbiter gains no client, the guard gains no arm, and
  // this boundary does not widen.  Composing the header reader with its own
  // socket would have made I26 a three-port entry instead of closing anything.
  output zhao_guard_req_t         terr_ps_guard_req_o,
  input  zhao_guard_rsp_t         terr_ps_guard_rsp_i,
  input  logic                    terr_ps_beat_valid_i,
  input  logic [63:0]             terr_ps_beat_data_i,
  input  logic                    terr_ps_beat_last_i,

  // ---- I34: TERRAIN.PATCH's field lane and its 9.1 list intake ------------
  input  logic                    terr_pt_fld_valid_i,
  output logic                    terr_pt_fld_ready_o,
  input  logic signed [31:0]      terr_pt_fld_height_i,
  input  logic                    terr_pt_fld_add_valid_i,
  output logic                    terr_pt_fld_add_ready_o,
  input  logic signed [31:0]      terr_pt_fld_add_x0_i,
  input  logic signed [31:0]      terr_pt_fld_add_z0_i,
  input  logic signed [31:0]      terr_pt_fld_add_x1_i,
  input  logic signed [31:0]      terr_pt_fld_add_z1_i,
  input  logic [31:0]             terr_pt_fld_add_hash_i,
  input  logic [15:0]             terr_pt_fld_add_cmd_i,
  output logic                    terr_pt_fld_add_accept_o,
  output logic                    terr_pt_fld_add_reject_o,
  output logic                    terr_pt_fld_covers_o,
  output logic [4:0]              terr_pt_fields_active_o,
  output logic [15:0]             terr_pt_trace_patch_id_o,
  output logic [31:0]             terr_pt_trace_hash_o,
  output logic [15:0]             terr_pt_trace_cmd_o,
  output logic [31:0]             terr_pt_programs_rejected_o,

  // ---- I32 (extended): TERRAIN.COMPCACHE's layer-D cell-state write -------
  input  logic                    terr_cc_cs_we_i,
  input  logic [4:0]              terr_cc_cs_ci_i,
  input  logic [4:0]              terr_cc_cs_cj_i,
  input  logic [1:0]              terr_cc_cs_substance_i,

  // ---- I21 (extended): the served patch's RETIREMENT pulse ---------------
  // "TESS is finished with the served patch", one patch per RISING EDGE.  The
  // block that knows is the one that issued the subpatch jobs, and that is the
  // absent owner entry I21 already names.
  input  logic                    terr_cc_serve_release_i,

  // ---- THE COMPOSE ENGINE'S EVIDENCE --------------------------------------
  // Events, never cycles.  These are what say a PAGE became a LATTICE rather
  // than four blocks having elaborated next to each other.
  output logic [31:0]             terr_ps_lattices_o,
  output logic [31:0]             terr_ps_lattices_refused_o,
  output logic [31:0]             terr_ps_vertices_o,
  output logic [31:0]             terr_ps_bursts_o,
  output logic [31:0]             terr_ps_guard_denied_o,
  output logic [31:0]             terr_ps_incomplete_o,
  output logic                    terr_ps_idle_o,
  // A TAP on the streamer's completion, not a handshake: the READY belongs to
  // TERRAIN.RESIDENCY's unpin port inside this module.  Exported so a refusal
  // can be READ rather than only counted.
  output logic                    terr_ps_done_valid_o,
  output logic                    terr_ps_done_ok_o,
  output logic [3:0]              terr_ps_done_verdict_o,

  // ---- TERRAIN.HDRREAD's evidence (composed item 13) ----------------------
  // The patch header reader entry I35 named as the absent owner.  These are
  // the numbers that separate "the placement was fed" from "the placement was
  // fed SOMETHING": `terr_hr_headers_o` counts headers that returned and
  // passed their identity test, and every other counter here is a distinct
  // reason a patch was refused instead.  Their SUM against
  // `terr_place_patches_o` is the assertion worth making -- a header this
  // block refused arrives at TERRAIN.PLACE as an impossible pitch, so
  // `terr_hr_*` and `terr_place_pitch_bad_o` must move together or one of the
  // two is lying.
  output logic [31:0]             terr_hr_headers_o,
  output logic [31:0]             terr_hr_refused_o,
  output logic [31:0]             terr_hr_guard_denied_o,
  output logic [31:0]             terr_hr_incomplete_o,
  output logic [31:0]             terr_hr_ident_fails_o,
  output logic                    terr_hr_idle_o,

  // ---- THE READ SHARE's evidence (composed item 13) ----------------------
  // `zhao_mem_share2` joining TERRAIN.HDRREAD (A) and TERRAIN.PAGESTREAM (B)
  // onto the one guard read client.  `terr_rdshare_contention_o` is the number
  // that says what the sharing COST: it moves once per cycle in which both
  // readers asked and one was held.  It is exported rather than counted
  // privately because the decision to widen this to two outstanding requests
  // has to be made against a number, which is the block's own stated reason
  // for having it.
  output logic [31:0]             terr_rdshare_jobs_a_o,
  output logic [31:0]             terr_rdshare_jobs_b_o,
  output logic [31:0]             terr_rdshare_denied_o,
  output logic [31:0]             terr_rdshare_contention_o,
  output logic [31:0]             terr_rdshare_err_short_o,
  output logic [31:0]             terr_rdshare_err_long_o,
  output logic [31:0]             terr_rdshare_err_unowned_o,

  output logic                    terr_place_valid_o,
  output logic [15:0]             terr_place_src_id_o,
  output logic [15:0]             terr_place_env_mismatch_o,
  output logic [15:0]             terr_place_pitch_bad_o,
  output logic [15:0]             terr_place_range_o,
  output logic [15:0]             terr_place_patches_o,

  output logic [31:0]             terr_pt_samples_o,
  output logic [15:0]             terr_pt_subpatch_dirty_o,
  output logic                    terr_pt_idle_o,

  output logic                    terr_cc_fill_busy_o,
  output logic                    terr_cc_fill_done_o,
  output logic                    terr_cc_serve_valid_o,
  output logic [15:0]             terr_cc_serve_src_id_o,
  output logic [31:0]             terr_cc_fill_records_o,
  output logic [31:0]             terr_cc_patches_filled_o,
  output logic [31:0]             terr_cc_patches_served_o,
  output logic [31:0]             terr_cc_fill_overrun_o,
  output logic [31:0]             terr_cc_lat_oob_o,
  output logic [31:0]             terr_cc_cs_oob_o,

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
  // THE DEPTH PROFILE THE RESULT WAS PROJECTED UNDER -- NEW 2026-09-19, and it
  // closes entry I14's depth-profile item. `SetView`'s `flags[1:0]` is
  // the depth profile of the frozen 2026-08-31 ruling; `zhao_project_core` now
  // carries it on cfg address 18 and emits it beside the view, and CMD.EXEC's
  // SetView arm writes it as the seventeenth step of the view walk.
  //
  // IT LEAVES THIS MODULE RATHER THAN BEING CONSUMED HERE, exactly as
  // `proj_a_view_o` does and for the same reason: GEOM.DEPTHQUANT is the
  // consumer -- its `v_profile_i` is this port's width and meaning -- and that
  // block is not composed. `proj_fill_profile_o` is the terrain client's
  // per-VERTEX half; terrain's per-TRIANGLE profile is still owed, because the
  // replay arena carries no profile field and widening it is a change to
  // `zhao_vertex_arena`, not to a composer.
  output logic [1:0]              proj_a_profile_o,
  output logic [1:0]              proj_fill_profile_o,
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
  // `tri_area2_i` USED TO BE HERE and is now driven internally by
  // `zhao_geom_setup`, which is composed below -- see the wire `st_area2` and
  // header entry 7. It was the twenty-first field of a twenty-one-field
  // packet whose other twenty were already internal, and leaving it at the
  // edge held every pixel out of the framebuffer.
  // AND THE THREE ATTRIBUTE PLANES LEFT THIS EDGE 2026-09-19, by the same act
  // and for the same reason: `zhao_geom_attrpack` is composed below and is
  // their producer. They were never a boundary in the sense the other entries
  // mean -- the arithmetic was in the tree the whole time, in
  // `zhao_geom_attrsetup`, with no block in front of it to ask three times.
  //
  // `tri_flat_request_i` STAYS AT THE EDGE and is not an oversight. It is the
  // MATERIAL RECORD, and its owner is MATERIAL.RESOLVE, which is BUILT
  // (`fpga/rtl/texture/zhao_material_resolve.sv`, UNIT_VERIFIED, 91 directed
  // checks) and NOT COMPOSED. This line read "`maturity: SPECIFIED` with both
  // tests PLANNED -- NOT WRITTEN and a note blocking it on a cartridge
  // decision" until 2026-09-19; all three clauses had gone stale, the cartridge
  // one by sixteen days. What it waits on is a `spec/memory_rules.md` 5f
  // sentence naming the residency directory's KEY. See entry I20.
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
  // THESE FIVE PORTS ARE GONE, 2026-09-19, and the sentence above is why the
  // removal is the point rather than a tidy-up. `geom_guard_req_i`,
  // `geom_guard_rsp_o` and the three `geom_beat_*_o` were this module's edge:
  // the bench answered the grants and fabricated the beats, so "the whole
  // staircase rested on a memory that granted immediately and answered in one
  // cycle" was still true of the CONSOLE even after it stopped being true of
  // the shell. `u_geom_mem_adapter` drives that socket now (connected item
  // 11), so the fetchers are behind the real guard and the real controller
  // and the only memory left for a harness to supply is the SDRAM itself, at
  // `phy_*`, where the completion plan puts it.

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

  // I31 CLOSED 2026-09-19. SURFACE.STAMP's field-driven brush is driven from
  // INSIDE this module now: `u_field_stamp_adapter` walks the stencil and
  // `u_field_host` runs the program. The four ports are GONE from this edge
  // rather than driven from it, which is the difference between a seam that
  // closed and a seam that acquired a producer.

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
  // DEBUG.TRACE's arming and its host readout.  Added 2026-09-19 with the ring.
  // --------------------------------------------------------------------------
  // BOUNDARY, and entry I45 argues it. The trace ring's DATA path is closed
  // inside this module -- CMD.DECODER's record port feeds it and nothing is
  // invented on the way -- so what leaves here is the debug CONTROL surface
  // (arming is a debug command, charter 20.6 "selectable") and the host drain
  // (the ring streams into the HPS trace arena through MEM.HPS.BRIDGE, which
  // has no register path to this block). Both are the same shape as
  // MEASURE.HISTOGRAM's `hist_rd_*` at entry I19, and for the same reason.
  //
  // UNARMED IS THE CORRECT DEFAULT and it is not a tie-off: an unarmed stage
  // produces no event AT ALL -- not a suppressed one -- so an undriven
  // `dbg_trace_arm_we_i` leaves a ring that costs its memory and stores
  // nothing, which is exactly what a trace ring does when nobody asked for a
  // trace.
  input  logic        dbg_trace_arm_we_i,
  input  logic [ 6:0] dbg_trace_arm_mask_i,
  input  logic        dbg_trace_clear_i,
  input  logic [ 8:0] dbg_trace_rd_addr_i,     // {event[5:0], word[2:0]}
  output logic [31:0] dbg_trace_rd_data_o,
  output logic [ 6:0] dbg_trace_armed_o,
  output logic [31:0] dbg_trace_count_o,
  output logic [31:0] dbg_trace_dropped_o,

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
  output logic [31:0] cmd_exec_unsupported_o,

  // ==========================================================================
  // TERRAIN.MIPFEED / TERRAIN.MIPGEN -- THE SECOND COMPLETION.  Added
  // 2026-09-19 with composition item 12.
  // ==========================================================================
  // The counters are here because this chain's whole purpose is a STATE
  // TRANSITION inside the directory, and a state transition has no other
  // symptom.  `terr_res_resident_o` rising from zero is the result; these say
  // which block produced it.
  output logic [31:0]  terr_mip_pages_mipped_o,
  output logic [31:0]  terr_mip_pages_faulted_o,
  output logic [31:0]  terr_mip_samples_sent_o,
  output logic [31:0]  terr_mipreq_requests_o,
  output logic [31:0]  terr_mipreq_issued_o,
  output logic [31:0]  terr_mipreq_drops_o,
  output logic [31:0]  terr_psmux_a_jobs_o,
  output logic [31:0]  terr_psmux_b_jobs_o,
  output logic [31:0]  terr_psmux_stray_v_o,
  output logic [31:0]  terr_psmux_stray_done_o,

  // I42: THE COARSE-HEIGHT MIP PLANES.  `spec/terrain_rules.md` 2's "17x17 +
  // 9x9 ... for TERRAIN.LOD" is a STORED quantity, and the store does not
  // exist.  These are real ports rather than dropped outputs for the reason
  // I32 gives about layer D: a decimated height written nowhere and a
  // decimated height written wrongly are indistinguishable from inside, and a
  // port is the one place the difference is visible.
  output logic         terr_mg_m17_valid_o,
  output logic [ 8:0]  terr_mg_m17_addr_o,
  output logic         terr_mg_m17_surf_o,
  output logic [15:0]  terr_mg_m17_h_o,
  output logic         terr_mg_m9_valid_o,
  output logic [ 6:0]  terr_mg_m9_addr_o,
  output logic         terr_mg_m9_surf_o,
  output logic [15:0]  terr_mg_m9_h_o,
  output logic [31:0]  terr_mg_m17_writes_o,
  output logic [31:0]  terr_mg_m9_writes_o,
  output logic [31:0]  terr_mg_aborts_o,

  // ==========================================================================
  // THE FIELD ENGINE'S EDGE. I42, and it is ONE entry where there were THREE.
  // ==========================================================================
  // THE PROGRAM LOADER. Nothing inside this console loads a field program, and
  // the named owner is CMD.EXEC's TerrainField 0x0200 arm (spec/commands.zidl:
  // `handle32[program] program` -> the cartridge PROGRAM page, spec/cartridge.md
  // 3 kind 0) together with the software decoder. `ld_kind_i` is 0 instruction,
  // 1 table entry, 2 header; the header is written LAST and is what marks a slot
  // runnable, so a partially written program can never execute.
  input  logic         fld_ld_valid_i,
  output logic         fld_ld_ready_o,
  input  logic [ 1:0]  fld_ld_kind_i,
  input  logic [ 2:0]  fld_ld_slot_i,
  input  logic [ 6:0]  fld_ld_addr_i,
  input  logic [95:0]  fld_ld_data_i,

  // FIELD.PROGCACHE's TWO PHASES. Both face outward because the decode a miss
  // requires is `zfield::decode`'s and lives in software -- that block's own
  // contract says the caller decodes and reports one bit. What is INTERNAL, and
  // is why the directory is composed rather than left at this edge, is the
  // insert: it invalidates the program store's slot, so no profile can run
  // microcode the directory has already promised to another hash.
  input  logic         fld_pc_lu_valid_i,
  output logic         fld_pc_lu_ready_o,
  input  logic [31:0]  fld_pc_lu_hash_i,
  output logic         fld_pc_lu_resp_valid_o,
  input  logic         fld_pc_lu_resp_ready_i,
  output logic         fld_pc_lu_hit_o,
  output logic [ 2:0]  fld_pc_lu_slot_o,
  input  logic         fld_pc_cm_valid_i,
  output logic         fld_pc_cm_ready_o,
  input  logic [31:0]  fld_pc_cm_hash_i,
  input  logic         fld_pc_cm_ok_i,
  output logic         fld_pc_cm_resp_valid_o,
  input  logic         fld_pc_cm_resp_ready_i,
  output logic         fld_pc_cm_inserted_o,
  output logic         fld_pc_cm_evicted_o,
  output logic [ 2:0]  fld_pc_cm_slot_o,

  // CONSOLE POLICY: which resident program is the stamp brush, and whether one
  // is resident at all. The same shape as `surf_cmd_field_en_i` beside it and
  // for the same reason -- no opcode carries either, and I30 already records
  // that an executor filling them in would be choosing values the ABI does not
  // contain.
  input  logic [ 2:0]  fld_stamp_slot_i,
  input  logic         fld_stamp_slot_valid_i,

  // THE ENGINE'S SECOND CLIENT. It is the seam the FLOW and EARTH adapters take
  // over when I5 and I34 close, and until then it is what makes the arbiter's
  // contention reachable with legal stimulus: a counter that cannot be fired is
  // not evidence about the thing it watches.
  //
  // The two widths are literals because a port list cannot see a body
  // localparam. `u_field_host` is instantiated with IN_LANES = 12 (the
  // ratified E record of spec/form/field-ir.md 7.1) and OUT_LANES = 4, and the
  // elaboration guard beside the instance refuses any disagreement rather than
  // leaving the two places to drift.
  input  logic          fld_req_valid_i,
  output logic          fld_req_ready_o,
  input  logic [  2:0]  fld_req_slot_i,
  input  logic          fld_req_noprog_i,
  input  logic [383:0]  fld_req_in_i,
  output logic          fld_resp_valid_o,
  input  logic          fld_resp_ready_i,
  output logic [127:0]  fld_resp_out_o,
  output logic [  7:0]  fld_resp_status_o,

  output logic [31:0]  fld_runs_o,
  output logic [31:0]  fld_run_faults_o,
  output logic [31:0]  fld_noprog_o,
  output logic [31:0]  fld_instr_retired_o,
  output logic [31:0]  fld_loads_o,
  output logic [31:0]  fld_load_defers_o,
  output logic [31:0]  fld_grants_o,
  output logic [31:0]  fld_contended_grants_o,
  // A load word addressed past the uop store. CLAMPED, not wrapped: a wrapped
  // uop write lands on another instruction of the same program, which is silent
  // and produces a plausible field.
  output logic [31:0]  fld_ld_oob_o,
  // A point whose run wrote NOTHING into its declared output window. The lanes
  // then hold the zeroes the front cleared them to, and a caller reading only
  // the lanes could not tell that from a field whose value is zero.
  output logic [31:0]  fld_no_result_o,
  // EVERY ALARM THE v3 FABRIC OWNS, UNMERGED AND SEPARATELY COUNTED.
  // `zhao_field_v3_engine`'s own header is right that five faults reduced to
  // one bit is a bit that says "something, somewhere", and a guard that cannot
  // name its own failure gets read as noise.
  output logic [31:0]  fld_exec_desync_o,
  output logic [31:0]  fld_bank_desync_o,
  output logic [31:0]  fld_svc_bank_desync_o,
  output logic [31:0]  fld_tag_mismatch_o,
  output logic [31:0]  fld_wrong_op_o,
  output logic [31:0]  fld_unsupported_o,
  output logic [31:0]  fld_skid_overflow_o,
  output logic [31:0]  fld_uniform_bad_o,
  // {sat_rescale, sat_mul, sat_add} -- the op ledger, latched over the run.
  output logic [ 2:0]  fld_sat_o,
  output logic [31:0]  fld_pc_hits_o,
  output logic [31:0]  fld_pc_misses_o,
  output logic [31:0]  fld_pc_rejected_o,
  output logic [31:0]  fld_pc_evictions_o,
  output logic [ 3:0]  fld_pc_occupancy_o,
  output logic [31:0]  surf_fld_stamps_o,
  output logic [31:0]  surf_fld_texels_o,
  output logic [31:0]  surf_fld_faults_o,
  output logic [31:0]  surf_fld_restarts_o,
  // High while the stencil walk is running. Exported rather than dropped: it
  // is the one signal that separates "the brush produced nothing" from "the
  // brush never started", and those have different causes and different fixes.
  output logic         surf_fld_busy_o
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
  wire signed [ 7:0] pal_o_nx, pal_o_ny, pal_o_nz;
  wire signed [31:0] pal_a_m [12];
  wire signed [31:0] pal_b_m [12];

  // THE FORK. GEOM.SKIN takes the position and GEOM.SKIN.NORM takes the normal,
  // and BOTH need the same vertex's two matrices in the same handshake. See
  // entry I43 for the whole argument, including what this costs in clocks.
  wire               skin_v_ready;      // GEOM.SKIN's own ready
  wire               sn_v_ready;        // GEOM.SKIN.NORM's own ready
  wire               skin_v_valid;
  wire               sn_v_valid;

  // A plain AND-fork. Neither consumer's `ready` is a function of its `valid`
  // -- `zhao_geom_skin` is `!busy && (!o_valid_o || o_ready_i)` and
  // `zhao_geom_skin_norm` is `(st_q == S_IDLE)` -- so this cannot deadlock, and
  // the two accept in the SAME cycle, which is what makes the normal and the
  // position provably the same vertex's.
  assign pal_o_ready = skin_v_ready && sn_v_ready;
  assign skin_v_valid = pal_o_valid && sn_v_ready;
  assign sn_v_valid   = pal_o_valid && skin_v_ready;

  // The root service GEOM.SKIN.NORM names by file. `zhao_field_isqrt` is
  // already in this closure (the FIELD engine instantiates it), so this costs a
  // second instance and NOT a second law -- and a second LAW is what would have
  // been wrong: that block's header says "a second implementation would be a
  // second law", and `zref::isqrt_u64` is the one both cite.
  wire        sn_sq_valid, sn_sq_ready, sn_sq_rvalid, sn_sq_rready;
  wire [63:0] sn_sq_n, sn_sq_r;

  zhao_geom_vdecode #(
    .SRCW (16)
  ) u_geom_vdecode (
    .clk        (gpu_clk),
    .rst_n      (rst_n),

    // REAL: GEOM.ASSETFETCH's 32-byte vertex record. This was entry I23 and
    // it is CLOSED -- the producer is `u_geom_assetfetch` at the end of this
    // file, reading the render asset pool through the real MEM.GUARD. Name
    // for name, width for width, nothing computed between them.
    .v_valid_i  (af_v_valid),
    .v_ready_o  (af_v_ready),
    .v_bytes_i  (af_v_bytes),
    .v_format_i (GEOM_VERTEX_FORMAT_C),   // I25: assigned here, not tied off
    .v_src_id_i (af_v_src_id),

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

    // REAL: the packed bind-space normal, from the SAME GEOM.VDECODE beat as
    // the position and the two bone indices above. This is the port added
    // 2026-09-19 that made GEOM.SKIN.NORM composable -- see I43.
    .v_nx_i     (geom_vd_d_nx_o),
    .v_ny_i     (geom_vd_d_ny_o),
    .v_nz_i     (geom_vd_d_nz_o),

    // REAL: the vertex and its two matrices, into GEOM.SKIN and GEOM.SKIN.NORM.
    .o_valid_o  (pal_o_valid),
    .o_ready_i  (pal_o_ready),
    .o_x_o      (pal_o_x),
    .o_y_o      (pal_o_y),
    .o_z_o      (pal_o_z),
    .o_w0_o     (pal_o_w0),
    .o_rigid_o  (pal_o_rigid),
    .o_src_id_o (pal_o_src_id),
    .o_nx_o     (pal_o_nx),
    .o_ny_o     (pal_o_ny),
    .o_nz_o     (pal_o_nz),
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
    // `v_valid_i` is the fork's, not the store's raw valid: this block and
    // GEOM.SKIN.NORM accept the same beat on the same clock (I43).
    .v_valid_i (skin_v_valid),
    .v_ready_o (skin_v_ready),
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

  // --------------------------------------------------------------------------
  // GEOM.SKIN.NORM -- the blended world normal, composed 2026-09-19.
  //
  // WHAT MADE IT COMPOSABLE. The lighting-seam note above used to say its
  // "THREE OPERANDS ARE NEVER SIMULTANEOUSLY VALID IN THIS MODULE", and that
  // was exactly right about the tree as it stood: the normal was valid at
  // GEOM.VDECODE's output and the matrices at the palette store's, several
  // clocks and one lookup apart, for what may not even be the same vertex. The
  // note also named the fix -- "`zhao_geom_pose_palette` carrying the normal
  // through beside the vertex" -- and called it "an RTL change to a block with
  // its own directed test, not a composition". That change is made, the
  // directed test covers it, and the mutant copy was regenerated with it, so
  // the three operands now arrive on ONE handshake. Nothing is paired here by
  // this file's guesswork.
  //
  // THE ROOT IS A SECOND INSTANCE AND NOT A SECOND LAW. `zhao_field_isqrt` is
  // the block's own named service and is already in this closure, so no source
  // file joins the fit for it.
  // --------------------------------------------------------------------------
  zhao_geom_skin_norm #(
    .SRCW (16)
  ) u_geom_skin_norm (
    .clk        (gpu_clk),
    .rst_n      (rst_n),

    // REAL: the packed normal and its two matrices, one handshake, one vertex.
    .v_valid_i  (sn_v_valid),
    .v_ready_o  (sn_v_ready),
    .v_nx_i     (pal_o_nx),
    .v_ny_i     (pal_o_ny),
    .v_nz_i     (pal_o_nz),
    .v_w0_i     (pal_o_w0),
    .v_src_id_i (pal_o_src_id),
    .a_i        (pal_a_m),
    .b_i        (pal_b_m),

    // REAL: the exact floor root, as a service.
    .sq_valid_o  (sn_sq_valid),
    .sq_ready_i  (sn_sq_ready),
    .sq_n_o      (sn_sq_n),
    .sq_rvalid_i (sn_sq_rvalid),
    .sq_rready_o (sn_sq_rready),
    .sq_r_i      (sn_sq_r),

    // I43: the world normal leaves the module. Its consumer is GEOM.LIGHT and
    // that seam is refused for reasons of its own -- see the lighting section.
    .n_valid_o      (geom_sn_n_valid_o),
    .n_ready_i      (geom_sn_n_ready_i),
    .n_x_o          (geom_sn_n_x_o),
    .n_y_o          (geom_sn_n_y_o),
    .n_z_o          (geom_sn_n_z_o),
    .n_mag_o        (geom_sn_n_mag_o),
    .n_degenerate_o (geom_sn_n_degenerate_o),
    .n_src_id_o     (geom_sn_n_src_id_o),

    .vertices_o   (geom_sn_vertices_o),
    .degenerate_o (geom_sn_degenerate_o),
    .reduced_o    (geom_sn_reduced_o)
  );

  zhao_field_isqrt u_geom_skin_norm_isqrt (
    .clk       (gpu_clk),
    .rst_n     (rst_n),
    .n_valid_i (sn_sq_valid),
    .n_ready_o (sn_sq_ready),
    .n_i       (sn_sq_n),
    .r_valid_o (sn_sq_rvalid),
    .r_ready_i (sn_sq_rready),
    .r_o       (sn_sq_r)
  );

  // THE FORK'S COST, COUNTED RATHER THAN ARGUED (I43). A cycle in which the
  // store is offering a vertex, GEOM.SKIN would take it and GEOM.SKIN.NORM
  // would not. This number is EXPECTED to be large -- the normal path is a
  // ~38-clock one-at-a-time walk against the skinner's twelve -- and it is a
  // port so that the rate is measured in the composed machine instead of
  // being asserted in a comment.
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      geom_sn_fork_stall_o <= '0;
    end else if (pal_o_valid && skin_v_ready && !sn_v_ready
                 && geom_sn_fork_stall_o != 32'hFFFF_FFFF) begin
      geom_sn_fork_stall_o <= geom_sn_fork_stall_o + 32'd1;
    end
  end

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

  // THE TWENTY-FIRST FIELD OF THE SAME PACKET, and until now the only one of
  // them that left this module instead of reaching the door.
  //
  // `out_area2_o` went straight to `geom_setup_area2_o` -- evidence -- while
  // the shell's `tri_area2_i` took a boundary port nothing drove. That single
  // omission kept EVERY pixel out of the framebuffer, and not by dropping
  // triangles: `zhao_raster_tile_pipe_v2` reads `tri_area2_i == 47'd0` as
  // PROFILE AREA BAD (`profile_area_bad_ref_c`), raises `range_fault_event_w`
  // on the first job it accepts, latches `local_abort_q`, and from then on
  // SINKS every job of the frame instead of starting it.
  //
  // Measured on the console smoke bench with the port still at the edge:
  // 96 tile references, 72 jobs taken, 72 sunk, 0 STARTED, one
  // `range_fault_count_o`, `raster_abort_o` high, 0 fragments. Every one of
  // those reads like a binner that never got any work, which is why it
  // survived: the counters that were zero were the reassuring ones.
  //
  // 47 BITS AND NOT 48, and that is a truncation with a proof rather than a
  // convenience. GEOM.CLIP has already applied the winding flip, so an
  // ACCEPTED triangle's area2 is positive -- `geom_clip_attrswap_directed.cpp`
  // asserts "and the emitted area is positive" -- and bit 47 is its sign bit
  // sitting at zero. The low 47 bits are therefore the whole value, and it is
  // the same truncation `tests/geometry/geom_bin_pipe_v2_directed.cpp`
  // performs when it drives this same port from the setup oracle.
  wire signed [47:0] st_area2;
  assign geom_setup_area2_o = st_area2;

  // ---- the GEOM.SETUP / GEOM.ATTRPACK fork and join ------------------------
  // Declared here because `u_geom_setup` below is the first user. `cl_o_ready`
  // and `st_o_ready` are declared with their streams and are DRIVEN here; see
  // the GEOM.ATTRPACK block for what each one is.
  wire         st_tri_ready_w;
  wire         ap_tri_ready_w, ap_o_valid_w;
  wire [239:0] ap_invw_plane_w, ap_u_over_w_plane_w, ap_v_over_w_plane_w;
  wire [ 15:0] ap_src_id_w;
  wire [ 31:0] ap_triangles_w, ap_planes_w;
  wire         door_tri_valid_w, door_tri_ready_w;

  zhao_geom_setup u_geom_setup (
    .clk         (gpu_clk),
    .rst_n       (rst_n),

    // REAL: GEOM.CLIP's accepted packet, field for field -- now through the
    // FORK declared above, because GEOM.ATTRPACK takes the same packet.
    .tri_valid_i (cl_o_valid && ap_tri_ready_w),
    .tri_ready_o (st_tri_ready_w),
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
    .out_area2_o (st_area2),
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
  // GEOM.ATTRPACK: the OTHER half of GEOM.CLIP's packet, and the producer the
  // Packet-D attribute planes never had.
  //
  // GEOM.CLIP has always emitted two things and only one of them had a
  // customer. The vertices went to GEOM.SETUP and became edge functions; the
  // ruling-5 attribute packet -- invw24, u_over_w, v_over_w, lit r/g/b, alpha,
  // winding-flipped with their vertices -- left this module through
  // `geom_clip_attr_a_o` and was read by nothing, because the block that turns
  // three of those slots into three interpolation planes did not exist. That
  // is what `zhao_geom_attrpack` is, and its own header says why it holds ONE
  // `zhao_geom_attrsetup` rather than three.
  //
  // THE FORK AND THE JOIN, AND WHY THE PLANES CANNOT BELONG TO ANOTHER
  // TRIANGLE. This is the shape CLAUDE.md warns about most specifically: two
  // streams derived from one, rejoined downstream, with a field that moves
  // independently of the counters watching it. It is made safe structurally
  // rather than by a detector:
  //
  //   * the FORK gives both consumers ONE ready. `cl_o_ready` is the AND, and
  //     each consumer's valid is gated by the OTHER's ready, so an accept at
  //     GEOM.CLIP's output is an accept at BOTH or at neither. Neither ready is
  //     a function of its own valid, which is what makes that legal rather than
  //     a deadlock.
  //   * the JOIN gives the shell's triangle door ONE valid, the AND of the two,
  //     and hands each side a ready qualified by the other's valid. Neither
  //     block reorders, so same order in plus same accept edge equals same
  //     triangle out.
  //
  // WHAT IT COSTS, stated rather than discovered later. GEOM.SETUP alone
  // accepted a triangle per clock. The pair accepts one about every EIGHT,
  // because the shared attrsetup core runs three lanes at two clocks each and
  // the fork holds SETUP back to its rate. At the owner-ruled 120,000
  // vertices/frame -- roughly 40,000 triangles at 60 Hz -- the budget is about
  // 41 gpu clocks per triangle, so eight fits with room. Buying the clock back
  // means three attrsetup cores or a two-deep pack, and both spend ALMs on a
  // margin that is already there.
  //
  // THE IDENTITY RIDES ALONG (`ap_src_id_w`) so that a future consumer can
  // check it. It is deliberately NOT differenced against `st_src_id` here: on
  // a frame drawn from one source every triangle carries the same id, so that
  // comparison would read zero for a reason that has nothing to do with
  // whether the join is sound -- a detector wired to two operands that agree by
  // accident. The lockstep check below differences two counters incremented by
  // two DIFFERENT enables in two different modules instead, which is the
  // comparison that can actually see a broken fork.
  // ==========================================================================
  zhao_geom_attrpack #(
    .ATTRS         (GEOM_CLIP_ATTRS),
    .SLOT_INVW     (GEOM_ATTR_SLOT_INVW),
    .SLOT_U_OVER_W (GEOM_ATTR_SLOT_U_OVER_W),
    .SLOT_V_OVER_W (GEOM_ATTR_SLOT_V_OVER_W),
    .IDW           (16)
  ) u_geom_attrpack (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: the same GEOM.CLIP packet GEOM.SETUP takes, through the fork.
    .tri_valid_i  (cl_o_valid && st_tri_ready_w),
    .tri_ready_o  (ap_tri_ready_w),
    .tri_ax_i     (cl_o_ax),
    .tri_ay_i     (cl_o_ay),
    .tri_bx_i     (cl_o_bx),
    .tri_by_i     (cl_o_by),
    .tri_cx_i     (cl_o_cx),
    .tri_cy_i     (cl_o_cy),
    // REAL: GEOM.CLIP's winding-flipped attributes, which until now left this
    // module with no customer. Their own producer is still at the edge (I24),
    // and that is the honest state: the PLANE now has a producer, the VERTEX
    // ATTRIBUTE still arrives from outside.
    .tri_attr_a_i (geom_clip_attr_a_o),
    .tri_attr_b_i (geom_clip_attr_b_o),
    .tri_attr_c_i (geom_clip_attr_c_o),
    .tri_src_id_i (cl_o_src_id),

    // REAL: the shell's Packet-D attribute carriage.
    .out_valid_o          (ap_o_valid_w),
    .out_ready_i          (door_tri_ready_w && st_o_valid),
    .out_invw_plane_o     (ap_invw_plane_w),
    .out_u_over_w_plane_o (ap_u_over_w_plane_w),
    .out_v_over_w_plane_o (ap_v_over_w_plane_w),
    .out_src_id_o         (ap_src_id_w),

    .triangles_o (ap_triangles_w),
    .planes_o    (ap_planes_w)
  );

  assign geom_attrpack_triangles_o = ap_triangles_w;
  assign geom_attrpack_planes_o    = ap_planes_w;

  // The fork's single ready and the join's single valid.
  assign cl_o_ready       = st_tri_ready_w && ap_tri_ready_w;
  assign st_o_ready       = door_tri_ready_w && ap_o_valid_w;
  assign door_tri_valid_w = st_o_valid && ap_o_valid_w;

  // THE LOCKSTEP CHECK. `geom_setup_triangles_submitted_o` increments inside
  // `zhao_geom_setup`'s `if (pipe_en)` on `tri_valid_i`; `ap_triangles_w`
  // increments inside `zhao_geom_attrpack` on `tri_valid_i && (state == IDLE)`.
  // Two different expressions, in two different modules, on two different
  // register enables -- so this is NOT the pattern where one enable drives both
  // sides of a comparison and the check is blind to every fault that enable
  // participates in. A fork that stopped ANDing the two readys would let one
  // side take a triangle the other refused, and these two numbers would part
  // company on that clock.
  //
  // `synthesis translate_off` keeps it out of the fabric and does NOT keep it
  // out of Verilator, which is exactly what is wanted here.
  // synthesis translate_off
  always_ff @(posedge gpu_clk) begin
    if (rst_n) begin
      a_attrpack_setup_lockstep : assert
          (ap_triangles_w == geom_setup_triangles_submitted_o)
        else $fatal(1,
            "GEOM.ATTRPACK and GEOM.SETUP disagree about how many triangles GEOM.CLIP handed over (%0d vs %0d) -- the fork is no longer giving them one ready",
            ap_triangles_w, geom_setup_triangles_submitted_o);
      if (door_tri_valid_w && door_tri_ready_w) begin
        // AND THE IDENTITY, on the clock the door actually takes the pair.
        // STATED WITHOUT OVERCLAIMING: on a frame drawn from a single source
        // every triangle carries the same `src_id`, so this cannot fail there
        // and is not evidence about the join on such a frame. It is a real
        // check the moment two sources are in flight, and it is the only
        // reason `ap_src_id_w` is carried at all -- the counter difference
        // above is what watches the fork.
        a_attrpack_setup_same_triangle : assert (ap_src_id_w == st_src_id)
          else $fatal(1,
              "the shell's triangle door took GEOM.SETUP's edge functions for source %0h beside GEOM.ATTRPACK's planes for source %0h",
              st_src_id, ap_src_id_w);
      end
    end
  end
  // synthesis translate_on

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

  // ==========================================================================
  // THE TERRAIN COMPOSE ENGINE'S NETS.  Added 2026-09-19 (connected item 10).
  // ==========================================================================
  // DECLARED HERE, DRIVEN AT `u_terrain_place` / `u_terrain_pagestream` /
  // `u_terrain_patch` / `u_terrain_compcache` AT THE END OF THIS FILE.  The
  // three blocks that READ them -- TERRAIN.TESS just below, TERRAIN.SEQ and
  // TERRAIN.RESIDENCY a thousand lines down -- all come first in the file, so
  // one declaration block ahead of the earliest reader is the only arrangement
  // in which no name is an implicit net.

  // TERRAIN.SEQ's compose door, internal from 2026-09-19.
  wire                           tis_valid, tis_ready;
  wire [TERR_SLOTW-1:0]          tis_slot;
  wire [TERR_GENW-1:0]           tis_gen;
  wire [31:0]                    tis_epoch;
  wire signed [15:0]             tis_ix, tis_iz;
  wire [15:0]                    tis_flags;
  wire [31:0]                    tis_src_id;
  // FOUR FIELDS OF THE DOOR HAVE NO CONSUMER IN THIS ENGINE, and they are named
  // rather than left as an empty by-name connection, because `.is_island_o()`
  // reads as "there is no such signal" when it means "nothing here consumes it".
  // `is_island_o` identifies the island and the streamer addresses by POOL SLOT;
  // `is_cslot_*` indexes T6's 256-entry composed-height cache and
  // `zhao_terrain_compcache_front` is a two-buffer FRONT, not that store;
  // `is_view_mask_o` and `is_priority_o` belong to the subpatch job TERRAIN.LOD
  // would build, which is entry I21's boundary.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [31:0]                    tis_island;
  wire                           tis_cslot_valid;
  wire [$clog2(TERR_CSLOTS)-1:0] tis_cslot;
  wire [7:0]                     tis_view_mask, tis_priority;
  /* verilator lint_on UNUSEDSIGNAL */

  // TERRAIN.PAGESTREAM.  `tps_*` is the COMPOSE DOOR'S side of the streamer
  // and has not changed meaning; `tpsx_*` below is the STREAMER'S side, which
  // from 2026-09-19 is reached through `u_terrain_psmux` because the mip pass
  // shares it (composition item 12).
  wire                    tps_j_valid, tps_j_ready;
  wire                    tps_v_valid, tps_v_ready;
  wire signed [15:0]      tps_v_base, tps_v_scar, tps_v_bottom;
  wire [5:0]              tps_v_vi, tps_v_vj;
  wire                    tps_v_first;
  wire [15:0]             tps_v_flags;
  // TWO DELIBERATE NARROWINGS, WAIVED AT THE DECLARATION AND NOWHERE ELSE.
  // `tps_v_src_id` is T5's 32-bit record id against TERRAIN.PATCH's 16-bit
  // trace field; `tps_done_slot` is the POOL index against the directory's
  // handle, one bit narrower, and that bit is structurally zero on this path --
  // the argument is written out at the residency's unpin connection above.
  // Both are named at the instance that drops the bits; the waiver sits here so
  // the closure's lint stays SILENT rather than carrying two warnings whose
  // reasoning a reader would have to re-derive every time.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [31:0]             tps_v_src_id;
  wire [TERR_MEMSLOT-1:0] tps_done_slot;
  /* verilator lint_on UNUSEDSIGNAL */
  wire                    tps_done_valid;
  wire [TERR_GENW-1:0]    tps_done_gen;
  wire [31:0]             tps_done_epoch;

  // TERRAIN.HDRREAD.  `thr_j_*` is the compose door's side, `thr_h_*` the
  // header record it emits to TERRAIN.PLACE, and `thr_f_*` the SAME JOB
  // forwarded on to the streamer afterwards -- carried in the block's own
  // registers rather than re-read from TERRAIN.SEQ's port, which is the whole
  // reason the block exists.  See composed item 13.
  wire                    thr_j_ready;
  wire                    thr_h_valid;
  wire signed [7:0]       thr_h_pitch_log2;
  wire signed [15:0]      thr_h_patch_ix, thr_h_patch_iz;
  wire signed [31:0]      thr_h_env_x0, thr_h_env_z0;
  wire [15:0]             thr_h_src_id;
  // THE VERDICT IS CARRIED AND NOT READ HERE, and that is deliberate rather
  // than an oversight: TERRAIN.PLACE has no verdict input, and the refusal it
  // must perform arrives as an impossible pitch on the port above.  A composer
  // that read the verdict and acted on it would be making the placement
  // decision outside the block that owns placement.  Waived at the declaration
  // so the closure's lint stays SILENT, the same way the two narrowings above
  // are.
  /* verilator lint_off UNUSEDSIGNAL */
  wire                    thr_h_ok;
  wire [3:0]              thr_h_verdict;
  /* verilator lint_on UNUSEDSIGNAL */
  wire                    thr_f_valid;
  wire [TERR_MEMSLOT-1:0] thr_f_slot;
  wire [TERR_GENW-1:0]    thr_f_gen;
  wire [31:0]             thr_f_epoch, thr_f_src_id;
  wire [15:0]             thr_f_flags;

  // THE ONE GUARD READ CLIENT, and the two readers behind it.  `trs_*` is the
  // share's downstream side; the two upstream sides are the blocks' own ports.
  zhao_guard_req_t        trs_a_req, trs_b_req;
  zhao_guard_rsp_t        trs_a_rsp, trs_b_rsp;
  wire                    trs_a_beat_valid, trs_b_beat_valid;
  wire [63:0]             trs_a_beat_data,  trs_b_beat_data;
  wire                    trs_a_beat_last,  trs_b_beat_last;

  // THE MIP REQUEST QUEUE'S DEPTH, a named knob rather than a literal in a
  // parameter map.  Eight is the composed frame; `terr_mipreq_drops_o` is what
  // says whether it is enough, and it is counted rather than inferred.
  localparam int unsigned TERR_MIPQ_DEPTH = 8;

  // THE SHARED STREAMER'S OWN SIDE.  Declared here, beside the compose door's,
  // so the two are read together and nobody mistakes one for the other.
  wire                    tpsx_j_valid, tpsx_j_ready;
  wire [TERR_MEMSLOT-1:0] tpsx_j_slot;
  wire [TERR_GENW-1:0]    tpsx_j_gen;
  wire [31:0]             tpsx_j_epoch, tpsx_j_src_id;
  wire [15:0]             tpsx_j_flags;
  wire                    tpsx_v_valid, tpsx_v_ready;
  wire                    tpsx_done_valid, tpsx_done_ready;

  // The MIP PASS's side of the share, and the chain behind it.
  wire                    tmf_ps_valid, tmf_ps_ready;
  wire [TERR_MEMSLOT-1:0] tmf_ps_slot;
  wire [TERR_GENW-1:0]    tmf_ps_gen;
  wire [31:0]             tmf_ps_epoch, tmf_ps_src_id;
  wire                    tmf_v_valid, tmf_v_ready;
  wire                    tmf_done_valid, tmf_done_ready;

  wire                    tmf_fin_valid, tmf_fin_ready, tmf_fin_ok;
  // THE THIRD DELIBERATE NARROWING, WAIVED HERE AND NOWHERE ELSE, exactly as
  // `tps_done_slot` is above and for a stronger reason.  TERRAIN.MIPFEED
  // carries the STREAMER's slot width because its `ps_slot_o` has to reach
  // TERRAIN.PAGESTREAM, while the directory's completion takes the narrower
  // {set, way} handle.  The extra bit is structurally zero on this path and the
  // argument is a chain rather than an assumption: the job is presented as
  // `{1'b0, tmq_j_slot}` at `u_terrain_mipfeed` below, `tmq_j_slot` is
  // TERR_SLOTW wide, and TERRAIN.MIPFEED's own header says the identity is
  // "returned unaltered" -- it has no arithmetic that could disturb it.  So
  // this is not the alias `tpl_fin_over` refuses; it is the same bit put on by
  // this composition eight lines earlier.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [TERR_MEMSLOT-1:0] tmf_fin_slot;
  /* verilator lint_on UNUSEDSIGNAL */
  wire [TERR_GENW-1:0]    tmf_fin_gen;
  wire [31:0]             tmf_fin_epoch, tmf_fin_crc;

  wire                    tmq_j_valid, tmq_j_ready;
  wire [TERR_SLOTW-1:0]   tmq_j_slot;
  wire [TERR_GENW-1:0]    tmq_j_gen;
  wire [31:0]             tmq_j_epoch, tmq_j_src_id, tmq_j_crc;

  wire                    tmg_start, tmg_done;
  wire                    tmg_fine_valid, tmg_fine_ready;
  wire [15:0]             tmg_fine_h;
  wire [TERR_MEMSLOT-1:0] tmg_job_slot;
  wire [TERR_GENW-1:0]    tmg_job_gen;
  wire [31:0]             tmg_job_epoch;

  // The share's owner bit and the block idles: real outputs with no consumer
  // in this core, named rather than left as empty by-name connections.  The
  // mip completion's source id is the same case and the reason is the bench's:
  // the directory's `fin` port matches on {slot, gen, epoch} and carries no
  // source id, so nothing here can read one.
  /* verilator lint_off UNUSEDSIGNAL */
  wire                    tpsx_busy, tpsx_owner;
  wire                    tmf_idle, tmg_busy;
  wire [31:0]             tmf_fin_src_id;
  wire [TERR_MEMSLOT-1:0] tmg_done_slot;
  wire [TERR_GENW-1:0]    tmg_done_gen;
  wire [31:0]             tmg_done_epoch, tmg_samples;
  wire [$clog2(TERR_MIPQ_DEPTH):0] tmq_level;
  wire                    tmq_idle;
  /* verilator lint_on UNUSEDSIGNAL */
  // The identity riders the compose lane does not read.  TERRAIN.PATCH takes
  // only the source id; the slot, generation and epoch that ride the stream are
  // checked by the streamer's OWN differential, and re-deriving a verdict from
  // them here would be a second place the same fact can disagree.  `v_last_o` is
  // unread because the cache counts its own capacity.
  /* verilator lint_off UNUSEDSIGNAL */
  wire                    tps_v_last;
  wire [TERR_MEMSLOT-1:0] tps_v_slot;
  wire [TERR_GENW-1:0]    tps_v_gen;
  wire [31:0]             tps_v_epoch;
  wire [31:0]             tps_done_src_id;
  /* verilator lint_on UNUSEDSIGNAL */

  // TERRAIN.PLACE.
  wire                tpc_pos_we, tpc_pos_axis;
  wire [5:0]          tpc_pos_idx;
  wire signed [31:0]  tpc_pos_val;
  wire signed [31:0]  tpc_wx, tpc_wz;
  wire                tpc_placed;
  // `hdr_ready_o` is constant 1 by the block's contract -- a header is always
  // retired, whatever the verdict -- and `pos_done_o` is the completion pulse of
  // a 66-write burst nothing here has to wait on, because the burst finishes
  // long before the first page beat returns.  Both named, neither consumed.
  /* verilator lint_off UNUSEDSIGNAL */
  wire                tpc_hdr_ready, tpc_pos_done;
  /* verilator lint_on UNUSEDSIGNAL */

  // TERRAIN.PATCH.
  wire                tpt_vtx_valid, tpt_vtx_ready, tpt_st_valid;
  wire signed [31:0]  tpt_top, tpt_bottom;
  wire [15:0]         tpt_st_src_id;
  // `compose_top_o` is the PRE-FIELD height, a diagnostic beside `top_o`, and
  // `st_dirty_o` is the per-vertex moved bit whose consumer is the subpatch
  // requester (entry I21).  The 4x4 mask those bits accumulate into IS exported,
  // on `terr_pt_subpatch_dirty_o`.
  /* verilator lint_off UNUSEDSIGNAL */
  wire signed [31:0]  tpt_compose_top;
  wire                tpt_st_dirty;
  /* verilator lint_on UNUSEDSIGNAL */

  // TERRAIN.COMPCACHE, and TERRAIN.TESS's side of the same seam.
  wire                tcc_fill_start, tcc_st_ready, tcc_fill_busy;
  wire signed [31:0]  tcc_lat_h, tcc_lat_wx, tcc_lat_wz;
  wire [1:0]          tcc_cs_substance;
  wire                tt_lat_req, tt_lat_surface;
  wire [5:0]          tt_lat_vi, tt_lat_vj;
  wire                tt_cs_req;
  wire [4:0]          tt_cs_ci, tt_cs_cj;
  // `fill_accept_o` is the one-cycle echo of a start this composition already
  // knows it made; the count that matters is `patches_filled_o`, exported.
  /* verilator lint_off UNUSEDSIGNAL */
  wire                tcc_fill_accept;
  /* verilator lint_on UNUSEDSIGNAL */

  // TERRAIN.RESIDENCY's unpin ready, back to the streamer's completion.
  wire                tres_unpin_ready;

  // The gates.  See (b), (c) and the acceptance note at the engine below.
  wire                tce_can_start, tce_job_take;

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

    // REAL, from 2026-09-19: TERRAIN.COMPCACHE's serve side.  Entry I22 said
    // `zhao_terrain_compcache_front` was the named owner and was not composed;
    // it is composed at `u_terrain_compcache` below and these eleven wires are
    // internal.  Nothing is adapted -- the cache's one-cycle read latency is its
    // own published contract and this block was built to it.
    .lat_req_o     (tt_lat_req),
    .lat_vi_o      (tt_lat_vi),
    .lat_vj_o      (tt_lat_vj),
    .lat_surface_o (tt_lat_surface),
    .lat_h_i       (tcc_lat_h),
    .lat_wx_i      (tcc_lat_wx),
    .lat_wz_i      (tcc_lat_wz),
    .cs_req_o      (tt_cs_req),
    .cs_ci_o       (tt_cs_ci),
    .cs_cj_o       (tt_cs_cj),
    .cs_substance_i(tcc_cs_substance),

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
    .a_profile_o(proj_a_profile_o),
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
    .fill_profile_o(proj_fill_profile_o),
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
  // ---- the FIELD engine's internal client and its shared response bus -----
  // Client 0 of `u_field_host`, driven by `u_field_stamp_adapter`. Both are
  // declared at the end of this module; these wires are here so the engine's
  // port map can be read without scrolling for a declaration.
  logic         sfa_req_valid, sfa_req_ready;
  logic [  2:0] sfa_req_slot;
  logic         sfa_req_noprog;
  logic [383:0] sfa_req_in;
  logic         sfa_resp_valid, sfa_resp_ready;
  logic [127:0] fld_resp_out_c;
  logic [  7:0] fld_resp_status_c;

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

  // ---- the field brush's wires, and the one policy decision this makes -----
  // `surf_field_en_c` is the host's request for the brush ANDed with "a stamp
  // program is actually resident". Without that gate a stamp issued with
  // `cmd_field_en_i` high and nothing loaded would stall forever waiting for
  // 4,096 records that cannot come, and `zhao_field_seq`'s own header is right
  // that a hang is the worse failure and the one nobody can debug from a frame
  // capture. It runs as a plain ABI stamp instead, and `fld_noprog_o` is not
  // the counter that says so -- `fld_stamp_slot_valid_i` is an input the board
  // can read back, so the condition is visible from outside without inventing
  // a counter for a state the console chose.
  //
  // THIS IS A POLICY DECISION TAKEN IN THE COMPOSER, which is where I30 already
  // says the stamp's other four policy bits are decided.
  logic        surf_field_en_c;
  logic        sfa_fld_valid, sfa_fld_ready;
  logic [31:0] sfa_fld_tag_op;
  logic [15:0] sfa_fld_strength;
  logic        sfa_arm_ready;
  assign surf_field_en_c = surf_cmd_field_en_i && sfa_arm_ready;

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
    .cmd_field_en_i  (surf_field_en_c),
    .cmd_src_id_i    (surf_cmd_src_id_m),

    // I31 CLOSED: the S-profile stream adapter, at the end of this module.
    .fld_valid_i   (sfa_fld_valid),
    .fld_ready_o   (sfa_fld_ready),
    .fld_tag_op_i  (sfa_fld_tag_op),
    .fld_strength_i(sfa_fld_strength),

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
    // REAL: GEOM.SETUP's own area, the twenty-first field of the triangle
    // packet whose other twenty arrive as `st_*` two lines below. See the
    // `st_area2` declaration for why this is 47 bits of a 48-bit value and
    // for what its absence did to the raster.
    .tri_area2_i               (st_area2[46:0]),
    // REAL: GEOM.ATTRPACK, one plane per lane of the shared attrsetup core.
    .tri_invw_plane_i          (ap_invw_plane_w),
    .tri_u_over_w_plane_i      (ap_u_over_w_plane_w),
    .tri_v_over_w_plane_i      (ap_v_over_w_plane_w),
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
    // REAL: the JOIN of GEOM.SETUP's edge functions and GEOM.ATTRPACK's three
    // planes. Both describe the same triangle by construction; see the
    // GEOM.ATTRPACK composition block.
    .render_tri_valid_i        (door_tri_valid_w),
    .render_tri_ready_o        (door_tri_ready_w),
    // REAL: the shell's ONE geometry guard socket, driven by
    // `u_geom_mem_adapter`, which merges GEOM.MESHFETCH and GEOM.ASSETFETCH
    // into it. These five were boundary ports (a bench answered the grants
    // and fabricated the beats); they are internal wires now and the traffic
    // on them is two real fetchers against the real MEM.GUARD, MEM.VRAM
    // .ARBITER and `zhao_sdram_ctrl` this shell already instantiates.
    .geom_guard_req_i          (ma_m_req),
    .geom_guard_rsp_o          (ma_m_rsp),
    .geom_beat_valid_o         (ma_m_beat_valid),
    .geom_beat_data_o          (ma_m_beat_data),
    .geom_beat_last_o          (ma_m_beat_last),
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
  // 7b-ii. DEBUG.TRACE -- the record stream's first real consumer
  // ==========================================================================
  // COMPOSED 2026-09-19, and the refusal it closes was WRONG rather than
  // merely stale, which is worth saying because the wrong sentence is the one
  // that did the refusing. Entry I18 read:
  //
  //   "DEBUG.TRACE is refused by THIS ENTRY'S OWN ARGUMENT, one block over.
  //    Its event carries `ev_expected_fx_i` beside `ev_actual_fx_i` -- a
  //    differential against a reference -- and the console has no reference
  //    ... The ledger's `upstream: [CMD.DECODER]` is not the RTL's seam
  //    either: the decoder emits record headers, not {stage, tile, primitive,
  //    pixel, expected, actual}."
  //
  // THAT COMPARED THE DECODER'S OUTPUT AGAINST THIS BLOCK'S OUTPUT. The
  // nine-field `ev_*` group is the RATIFIED TRACE RECORD (capture_format.md
  // chunk 0x000A, 32 bytes) -- what the ring STORES. What the block CONSUMES
  // is named in its own contract's "Input and output packet layouts" table,
  // and the table is this, verbatim: `rec_valid_i` `rec_ready_o`,
  // `rec_opcode_i` (16), `rec_bytes_i` (16), `rec_source_id_i` (32),
  // `rec_index_i` (32). That is `zhao_cmd_decoder`'s record port, field for
  // field and width for width. The contract's own Integration section says so
  // in as many words: "Composition with CMD.DECODER is the point: this
  // block's only producer is that block's record port."
  //
  // NOTHING BETWEEN THE TWO IS INVENTED HERE, and that is the part that had to
  // be checked rather than asserted, because five of the nine fields below are
  // CONSTANTS and a constant standing in for a producer is the failure this
  // file exists to stop. They are not standing in for anything. The mapping is
  // `zref::trace::Ring::on_record()` in
  // `reference/include/zref/zref_trace.hpp`, which is the ledger's declared
  // `reference_model` for this block, and it reads:
  //
  //     e.stage       = kCommandDecoder;   // 0
  //     e.source_id   = r.source_id;
  //     e.command_seq = r.index;
  //     // tile/primitive/pixel/expected/actual stay zero: see choice 3
  //
  // That file's header enumerates the three questions the spec leaves open and
  // answers them in one place. Choice 3 is this one, and its reasoning is the
  // opposite of a convenience: "`tile`, `primitive`, `pixel`, `expected_fx`
  // and `actual_fx` describe a raster divergence and mean nothing for a
  // decoded command record. Chosen: zero, because zero is checkable.
  // Rejected: leaving them undefined, which makes a byte-comparison of
  // captures impossible and is how a trace format rots." Choice 1 is the stage
  // BYTE: the charter names seven sources and numbers none, so the reference
  // takes their listed order and `kCommandDecoder` is 0.
  //
  // So the refusal's premise survives exactly where it was true and nowhere
  // else: the console still has no reference, and the two fields that WOULD
  // need one are the two the ruling says a decoder-stage event does not carry.
  // The other six raster stages (1..6) have no producer in this console and
  // are not wired; they are unarmable in practice because nothing offers an
  // event on them, and that is entry I44's other half.
  //
  // `ev_valid_i` IS THE RETIREMENT, NOT THE OFFER, and on this seam they are
  // the same wire: `rec_ready_i` above is tied to `1'b1` with its reasoning,
  // so every valid record moves in the cycle it is presented. Written out
  // because the CMD.EXEC fork two sections down is the counter-example -- a
  // consumer that read `pkt_valid && pkt_ready_o` on a FORKED stream ran one
  // byte ahead for a whole packet -- and if `rec_ready_i` ever stops being a
  // constant this line must become `cmd_rec_valid_w && <that ready>`.
  //
  // THE RING NEVER STALLS THE DECODER. There is no ready on `ev_*` at all;
  // that absence is the block's stated law ("a ring that back-pressured its
  // producer would make the act of tracing alter the timing being traced"), so
  // composing it cannot change the command path's behaviour whether armed or
  // not. A full ring counts the loss on `dropped_o`.
  //
  // COST, stated rather than discovered later: the ring is DEPTH x 256 bits,
  // 16 Kbit at the default 64 events, one write port and one registered read
  // port -- the simple-dual-port shape, so it should land in M10K rather than
  // ALMs, which is the trade this budget wants (ALMs bind; memory is the
  // slack). DEPTH stays a parameter and is left at the contract's default; it
  // is the knob to turn if the fit says otherwise.
  zhao_debug_trace #(
    .DEPTH(64)
  ) u_debug_trace (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    .arm_we_i   (dbg_trace_arm_we_i),
    .arm_mask_i (dbg_trace_arm_mask_i),
    .armed_o    (dbg_trace_armed_o),
    .clear_i    (dbg_trace_clear_i),

    .ev_valid_i       (cmd_rec_valid_w),
    // `zref::trace::kCommandDecoder`. An IDENTITY, not data: it names which of
    // the charter's seven sources this port is, and there is nothing for a
    // producer to supply.
    .ev_stage_i       (8'd0),
    // Ruled zero for a decoder-stage event -- zref_trace.hpp choice 3.
    .ev_tile_i        (32'd0),
    .ev_primitive_i   (32'd0),
    .ev_pixel_i       (32'd0),
    .ev_expected_fx_i (32'd0),
    .ev_actual_fx_i   (32'd0),
    // The two fields the reference DOES fill, from the record that fills them.
    .ev_source_id_i   (cmd_rec_source_id_w),
    .ev_command_seq_i (cmd_rec_index_w),

    .rd_addr_i (dbg_trace_rd_addr_i),
    .rd_data_o (dbg_trace_rd_data_o),

    .count_o   (dbg_trace_count_o),
    .dropped_o (dbg_trace_dropped_o)
  );

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

    // The terrain COMPOSE ENGINE's door.  INTERNAL from 2026-09-19: it drives
    // TERRAIN.PAGESTREAM's job port and TERRAIN.PLACE's patch header, both
    // instantiated at the end of this file.  Entry I27 is narrowed accordingly.
    .is_valid_o      (tis_valid),
    .is_ready_i      (tis_ready),
    .is_slot_o       (tis_slot),
    .is_gen_o        (tis_gen),
    .is_epoch_o      (tis_epoch),
    .is_island_o     (tis_island),
    .is_ix_o         (tis_ix),
    .is_iz_o         (tis_iz),
    .is_cslot_valid_o(tis_cslot_valid),
    .is_cslot_o      (tis_cslot),
    .is_flags_o      (tis_flags),
    .is_view_mask_o  (tis_view_mask),
    .is_priority_o   (tis_priority),
    .is_src_id_o     (tis_src_id),

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

  // THE TWO-CLAIMANT COMPLETION.  `tres_fin_a_v` is the loader's QUALIFIED
  // offer -- the one the over-slot refusal above has already filtered -- and
  // the mip completion sits behind it.  Written here rather than at the
  // instance because `tpl_fin_ready` is what the loader watches, and a reader
  // chasing "who readies the loader" must land on the same lines that decide
  // who the directory hears.
  wire tres_fin_a_v = tpl_fin_valid && !tpl_fin_over;
  wire tres_fin_ready;
  assign tpl_fin_ready = tres_fin_a_v  && tres_fin_ready;
  assign tmf_fin_ready = !tres_fin_a_v && tmf_fin_valid && tres_fin_ready;

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

    // THE COMPLETION PORT HAS TWO CLAIMANTS FROM 2026-09-19, and that is the
    // directory's own design rather than a crowding of it: it PUBLISHES ON TWO
    // COMPLETIONS.  A claim writes `mips_stale` unconditionally, the loader's
    // `fin` therefore lands the entry in ST_MIPGEN, and only a SECOND `fin`
    // reaches ST_RESIDENT_CLEAN -- the only state `lu_hit_o` and `resident_o`
    // recognise.  See composition item 12 for the chain that now produces it.
    //
    // LOADER FIRST.  It is the one that can BLOCK -- TERRAIN.PAGELOADER parks
    // in S_FIN until its completion is taken -- while TERRAIN.MIPFEED holds
    // `fin_valid_o` and simply waits.  So priority to the loader is lossless in
    // both directions and there is nothing to count here.
    //
    // AND THE OVER-SLOT REFUSAL STILL WINS.  `tres_fin_a_v` is the QUALIFIED
    // loader offer, so a completion carrying the pool's extra bit is neither
    // presented nor readied (the argument is at `tpl_fin_over` above) AND it
    // does not block the mip completion behind it -- the loader parks, which is
    // the loud failure that refusal is written to produce, and the mip chain
    // goes on working for every legal page.
    .fin_valid_i(tres_fin_a_v || tmf_fin_valid),
    .fin_ready_o(tres_fin_ready),
    .fin_slot_i (tres_fin_a_v ? tpl_fin_slot_w[TERR_SLOTW-1:0]
                              : tmf_fin_slot[TERR_SLOTW-1:0]),
    .fin_gen_i  (tres_fin_a_v ? tpl_fin_gen   : tmf_fin_gen),
    .fin_epoch_i(tres_fin_a_v ? tpl_fin_epoch : tmf_fin_epoch),
    .fin_ok_i   (tres_fin_a_v ? tpl_fin_ok    : tmf_fin_ok),
    .fin_crc_i  (tres_fin_a_v ? tpl_fin_crc   : tmf_fin_crc),

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

    // REAL, from 2026-09-19.  TERRAIN.SEQ's own port comment says the unpin is
    // "the engine's, on job completion, and is deliberately not this block's --
    // a pump that unpinned at issue would be promising the page is free while
    // TESS is still reading it."  The engine exists now and its completion is
    // TERRAIN.PAGESTREAM's `done_*`, which fires once per issued patch whatever
    // the verdict, so a refused page unpins too rather than parking a slot for
    // ever.
    //
    // THE SLOT NARROWS AND THE BIT IT DROPS IS STRUCTURALLY ZERO -- the same
    // step `tpl_fin_over` above QUALIFIES rather than clamps.  Here it needs no
    // counter, and the reason is that the argument is structural on BOTH ends
    // rather than on one: the streamer's `j_slot_i` is `{1'b0, tis_slot}` with
    // `tis_slot` TERR_SLOTW wide, and the block carries the job's slot to its
    // completion verbatim (`job_slot_q <= j_slot_i`, emitted as `done_slot_o`).
    // Bit TERR_MEMSLOT-1 cannot be set on this path by any stimulus, legal or
    // otherwise, so a detector on it would be a counter reporting zero about a
    // wire that is tied to zero -- which is not evidence about anything.
    .unpin_valid_i(tps_done_valid),
    .unpin_ready_o(tres_unpin_ready),
    .unpin_slot_i (tps_done_slot[TERR_SLOTW-1:0]),
    .unpin_gen_i  (tps_done_gen),
    .unpin_epoch_i(tps_done_epoch),

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
  //
  // A THIRD ARM, ADDED 2026-09-19: THE DRAW. `DrawForm 0x0300` is
  // `implemented` in `spec/commands.zidl` and was reaching this console and
  // dying -- CMD.SCHEDULER's record dispatch ends with "all other opcodes:
  // counted, no dispatch" and CMD.EXEC counted it on `unsupported_o`. It now
  // lowers whole, onto `cmd_draw_*`, which leaves this module as entry I41.
  // The three handles leave UNRESOLVED: the pool layout that would turn
  // `form` into a descriptor address is `spec/memory_rules.md` 5f's, and that
  // section says it is not decided.
  //
  // THE COMMIT ORDER IS NOW VIEWS -> STAMPS -> DRAWS AND THAT IS LOAD
  // BEARING. A form dispatched before its own packet's SetView would be drawn
  // through the previous frame's camera -- one wrong frame per camera move,
  // with every counter in this file balancing. The executor's FSM gives the
  // order structurally rather than by timing luck, and
  // `tests/command/cmd_exec_directed.cpp` case 12 measures it: the first form
  // leaves strictly after the thirty-second matrix word, under three
  // different ready patterns.
  //
  // ONE PACKET PER RESET, AND IT IS THE DECODER'S BOUND, NOT THIS ONE'S.
  // `zhao_cmd_decoder`'s `S_DONE` holds the verdict until reset and drives
  // `pkt_ready_o` low there, so the shared stream stops after one packet and
  // this composition executes exactly one frame's commands per reset. Written
  // down because it is invisible from here: every counter below reads a
  // perfectly sensible number for packet one and then never moves again, which
  // looks like an executor that stalled rather than a decoder that finished.
  // CMD.EXEC itself is already re-armable -- `pos` wraps at `pkt_len`, the view
  // shadow's dirty bits and the stamp ring are cleared on every commit and on
  // every abandon -- so it needs no change when the decoder learns to re-arm.
  // That change belongs to CMD.DECODER, whose verdict 19 committed goldens
  // pin, and it is not smuggled in here.
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

    // I41: the DRAW DISPATCH, straight out of this module. It does NOT go to
    // `u_geom_meshfetch` and the header entry says why in full: three of that
    // block's six job fields have no ratified producer, a job is atomic, and
    // half-driving one is a fetch at whatever address the other half was
    // holding rather than a half closure.
    .draw_valid_o          (cmd_draw_valid_o),
    .draw_ready_i          (cmd_draw_ready_i),
    .draw_form_o           (cmd_draw_form_o),
    .draw_material_set_o   (cmd_draw_material_set_o),
    .draw_transform_o      (cmd_draw_transform_o),
    .draw_viewport_mask_o  (cmd_draw_viewport_mask_o),
    .draw_semantic_weight_o(cmd_draw_semantic_weight_o),
    .draw_flags_o          (cmd_draw_flags_o),
    .draw_src_id_o         (cmd_draw_src_id_o),

    .packets_committed_o  (cmd_exec_committed_o),
    .packets_abandoned_o  (cmd_exec_abandoned_o),
    .views_written_o      (cmd_exec_views_o),
    .stamps_issued_o      (cmd_exec_stamps_o),
    .stamp_overflow_o     (cmd_exec_stamp_overflow_o),
    .view_range_refused_o (cmd_exec_view_refused_o),
    .stamp_src_truncated_o(cmd_exec_src_truncated_o),
    .draws_issued_o       (cmd_exec_draws_o),
    .draw_overflow_o      (cmd_exec_draw_overflow_o),
    .draw_src_truncated_o (cmd_exec_draw_src_truncated_o),
    .unsupported_o        (cmd_exec_unsupported_o)
  );

  // ==========================================================================
  // THE TERRAIN COMPOSE ENGINE.  Connected item 10, added 2026-09-19.
  // ==========================================================================
  // This is the chain entry I27 described and refused, with the block it was
  // waiting for:
  //
  //   TERRAIN.SEQ.is_*          -> TERRAIN.PAGESTREAM.j_*
  //                             -> TERRAIN.PLACE.hdr_*
  //   TERRAIN.PAGESTREAM.v_*    -> TERRAIN.PATCH.vtx_*   (3 planes, one beat)
  //   TERRAIN.PLACE.vtx_w{x,z}  -> TERRAIN.PATCH.w{x,z}_i
  //   TERRAIN.PLACE.pos_*       -> TERRAIN.COMPCACHE.pos_*
  //   TERRAIN.PATCH.st_*        -> TERRAIN.COMPCACHE.st_*
  //   TERRAIN.COMPCACHE.lat_/cs_-> TERRAIN.TESS          (entry I22, closed)
  //   TERRAIN.PAGESTREAM.done_* -> TERRAIN.RESIDENCY.unpin_*
  //
  // WHAT CHANGED SINCE I27 WAS WRITTEN.  That entry's whole argument was:
  //
  //     "THE BLOCKER IS PLACEMENT, and it is a missing owner rather than
  //      missing wiring ... nothing else in the tree emits it either.  Deriving
  //      it here from the index, the patch coordinate and a pitch is arithmetic
  //      invented in the composer, which this file does not do."
  //
  // `fpga/rtl/terrain/zhao_terrain_place.sv` is that owner, built 2026-09-19,
  // its law taken from spec/terrain_rules.md 1.3 and 2.1, with its own directed
  // test and three census counters that have been fired.  The arithmetic is
  // therefore RATIFIED and NAMED, and this file still does not do it: it wires
  // two ports together.
  //
  // THE SEAMS ARE PORT-FOR-PORT AND NOTHING HERE ADAPTS ANYTHING.  The three
  // height planes, the lattice indices, `st_*` into the cache's fill port and
  // the cache's serve side into TESS are all name-for-name, width-for-width
  // matches the blocks' own headers declare.
  // `tests/terrain/tb_terrain_compose.sv` makes the same four-block claim on
  // real page bytes and is the evidence that they meet.
  //
  // THE FOUR PLACES THIS COMPOSITION DECIDES SOMETHING, each named so it can be
  // argued with rather than discovered:
  //
  //   (a) THE DUAL FLAG IS BIT 3.  `zref::swstream::kFlagDual` is `1u << 3` of
  //       T5's patch-record flags; TERRAIN.SEQ carries all sixteen bits out on
  //       `is_flags_o` and TERRAIN.PAGESTREAM carries them whole and
  //       uninterpreted to every vertex, both deliberately.  SOMEBODY has to
  //       read the bit, because the two consumers that need it
  //       (`zhao_terrain_patch.dual_i` and the cache's `dual_i`) take a bit and
  //       not a field.  It is a named localparam rather than a bare index so it
  //       is greppable, and it is the same constant `tb_terrain_compose.sv`
  //       uses.  A page composed with the wrong `dual` is a different island
  //       underside, in the right shape, with every counter agreeing.
  //
  //   (b) A NEW PAGE MAY NOT START WHILE A FILL IS STILL IN THE CACHE.  The
  //       cache's position planes are written through `pos_we_i`, which is NOT
  //       gated on `fill_active_q` -- its own header says so, in the paragraph
  //       explaining why `fill_par_q` has to move off a buffer at handover.  So
  //       TERRAIN.PLACE writing patch N+1's 66 coordinates while patch N's fill
  //       is complete-but-unretired would write them into N's buffer.  The gate
  //       is the cache's OWN published `fill_busy_o`, not state invented here,
  //       and it is exactly the backpressure a two-buffer store exists to give.
  //
  //   (c) A REFUSED PLACEMENT DISCARDS THE PAGE'S VERTICES.  TERRAIN.PLACE
  //       exports `vtx_placed_o` -- "low = this patch was refused" -- precisely
  //       so a consumer can act on it, and TERRAIN.PATCH has no such input.  A
  //       bad pitch, a patch outside the representable world, or a page header
  //       whose envelope disagrees with its coordinate all mean one thing: this
  //       patch's world position is not known.  Composing it anyway would put
  //       real composed heights at a STALE world position, which the block's own
  //       header calls "the worst of the three outcomes: plausible geometry in
  //       the wrong place".  So the vertices are consumed and dropped, no fill
  //       is started, and three census counters move.  The page is still
  //       streamed to completion and still unpinned, because a refusal that
  //       stalled the page path would take the whole spine down with it.
  //
  //   (d) THE FIELD LANE IS A BOUNDARY AND IS NOT FAKED.  See entry I34.
  //       `fld_valid_i` low means section 3.4's `live_top` collapses to
  //       `compose_top`, which is that law with an empty program list and is
  //       exactly the half this composition can honestly carry.  Tying a HEIGHT
  //       here would be the fake stimulus the completion plan names by name.
  //
  //   (e) THE PATCH HEADER IS NOW READ, AND IT IS READ BY A BLOCK.  Added
  //       2026-09-19 with TERRAIN.HDRREAD (composed item 13).  The chain above
  //       gains one link at its head:
  //
  //           TERRAIN.SEQ.is_*  ->  TERRAIN.HDRREAD.j_*
  //           TERRAIN.HDRREAD.h_*  ->  TERRAIN.PLACE.hdr_*
  //           TERRAIN.HDRREAD.f_*  ->  TERRAIN.PSMUX client A -> PAGESTREAM
  //
  //       The three fields entry I35 recorded as having no producer --
  //       `pitch_log2` and the envelope's origin corner -- now come off the
  //       page's own 64 bytes on the compose path, with the job's identity
  //       beside them.  THE PATCH COORDINATE MOVED TOO, and that is not
  //       incidental: it used to be `tis_ix`/`tis_iz`, read live off
  //       TERRAIN.SEQ's port, which was correct only because the header and the
  //       job were taken on the same cycle.  They no longer are -- the header
  //       read takes a burst -- so the coordinate comes from the same record as
  //       the pitch it is checked against.  Reading one from the page and the
  //       other from a port that has already advanced is the exact
  //       join-between-two-things-that-move-independently fault I35 named.
  // ==========================================================================

  // `zref::swstream::kFlagDual`, T5's patch-record flags bit 3.  See (a).
  localparam int unsigned TERR_FLAG_DUAL_BIT = 3;

  assign tce_can_start = !tcc_fill_busy;
  // THE DOOR NOW OPENS ONTO THE HEADER READER, NOT THE STREAMER.  The cache's
  // `fill_busy_o` gate (b) is unchanged and still sits here, which gives it MORE
  // slack than before rather than less: the header burst now runs between the
  // gate and the first position write.
  assign tis_ready     = thr_j_ready && tce_can_start;

  // The compose door's side of the streamer is the FORWARDED job, held in
  // TERRAIN.HDRREAD's own registers.  See (e).
  assign tps_j_valid   = thr_f_valid;

  // ONE PULSE PER PATCH, AND IT IS THE ACCEPTANCE RATHER THAN THE OFFER.
  // TERRAIN.HDRREAD HOLDS `f_valid_o` until its ready comes, so a pulse driven
  // from the offer would re-count on every cycle of the wait -- a census that
  // measures how long the streamer was busy.  (This used to read `tis_valid`'s
  // acceptance and the sentence was the same one about TERRAIN.SEQ; the holder
  // changed, the hazard did not.)
  assign tce_job_take  = tps_j_valid && tps_j_ready;

  assign tpt_vtx_valid  = tps_v_valid && tpc_placed;
  assign tps_v_ready    = tpc_placed ? tpt_vtx_ready : 1'b1;
  assign tcc_fill_start = tps_v_valid && tps_v_ready && tps_v_first && tpc_placed;

  assign terr_cc_fill_busy_o  = tcc_fill_busy;
  assign terr_ps_done_valid_o = tps_done_valid;

  // ---- TERRAIN.PLACE -------------------------------------------------------
  zhao_terrain_place #(
    .LAT_W   (33),
    .LAT_H   (33),
    .CENSUS_W(16)
  ) u_terrain_place (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    // REAL, AND WHOLE FROM 2026-09-19: the patch header, every field of it off
    // the page's own 64 bytes, from TERRAIN.HDRREAD.  Entry I35 is closed and
    // deleted.  This port used to take the coordinate from TERRAIN.SEQ live and
    // the pitch and envelope from this module's edge; all five now arrive from
    // ONE record, which is what made the entry's join argument go away rather
    // than move.
    .hdr_valid_i     (thr_h_valid),
    .hdr_ready_o     (tpc_hdr_ready),
    .hdr_pitch_log2_i(thr_h_pitch_log2),
    .hdr_patch_ix_i  (thr_h_patch_ix),
    .hdr_patch_iz_i  (thr_h_patch_iz),
    .hdr_env_x0_i    (thr_h_env_x0),
    .hdr_env_z0_i    (thr_h_env_z0),
    .hdr_src_id_i    (thr_h_src_id),

    // REAL: TERRAIN.COMPCACHE's position fill, 33 column x's then 33 row z's.
    .pos_we_o  (tpc_pos_we),
    .pos_axis_o(tpc_pos_axis),
    .pos_idx_o (tpc_pos_idx),
    .pos_val_o (tpc_pos_val),
    .pos_done_o(tpc_pos_done),

    // REAL: TERRAIN.PATCH's per-vertex placement, combinational off the
    // streamer's own lattice indices and riding the same beat.
    .vtx_vi_i    (tps_v_vi),
    .vtx_vj_i    (tps_v_vj),
    .vtx_wx_o    (tpc_wx),
    .vtx_wz_o    (tpc_wz),
    .vtx_placed_o(tpc_placed),

    .place_valid_o       (terr_place_valid_o),
    .place_env_mismatch_o(terr_place_env_mismatch_o),
    .place_pitch_bad_o   (terr_place_pitch_bad_o),
    .place_range_o       (terr_place_range_o),
    .place_patches_o     (terr_place_patches_o),
    .place_src_id_o      (terr_place_src_id_o)
  );

  // ---- TERRAIN.HDRREAD (composed item 13) ----------------------------------
  // THE SAME THREE POOL PARAMETERS THE STREAMER AND THE LOADER GET, passed
  // rather than defaulted, for the identical reason: this block reads byte 0 of
  // the slot `u_terrain_pageloader` wrote and `u_terrain_pagestream` reads from
  // +64, and a base or a page size that agreed only by coincidence would read
  // the previous page's header and place this page where that one belongs.
  zhao_terrain_hdrread #(
    .PAGE_BYTES  (TERR_PAGE_BYTES),
    .REGION_BASE (TERR_POOL_BASE),
    .REGION_SLOTS(TERR_POOL_SLOTS),
    .SLOTW       (TERR_MEMSLOT),
    .GENW        (TERR_GENW)
  ) u_terrain_hdrread (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    .cfg_vram_client_i(ZHAO_CLIENT_TERRAIN_BUILD),
    .cfg_epoch_i      (terr_cfg_epoch_i),

    // REAL: the compose door, gated by the cache exactly as it was before this
    // block sat in front of the streamer.
    .j_valid_i (tis_valid && tce_can_start),
    .j_ready_o (thr_j_ready),
    // The same zero extension across the pool's extra refusal bit that both
    // other pool clients get, written here rather than assumed.
    .j_slot_i  ({1'b0, tis_slot}),
    .j_gen_i   (tis_gen),
    .j_epoch_i (tis_epoch),
    .j_src_id_i(tis_src_id),
    .j_flags_i (tis_flags),
    // REAL: the record's own identity, which the header restates.  This is the
    // only consumer of `tis_island` in this module and it is what turns the
    // format's redundancy into a check instead of a comment.
    .j_island_i(tis_island),
    .j_ix_i    (tis_ix),
    .j_iz_i    (tis_iz),

    // REAL: requester A of the one guard read client.  See `u_terrain_rdshare`.
    .guard_req_o (trs_a_req),
    .guard_rsp_i (trs_a_rsp),
    .beat_valid_i(trs_a_beat_valid),
    .beat_data_i (trs_a_beat_data),
    .beat_last_i (trs_a_beat_last),

    // REAL: the header record, to TERRAIN.PLACE.
    .h_valid_o     (thr_h_valid),
    .h_ready_i     (tpc_hdr_ready),
    .h_pitch_log2_o(thr_h_pitch_log2),
    .h_patch_ix_o  (thr_h_patch_ix),
    .h_patch_iz_o  (thr_h_patch_iz),
    .h_env_x0_o    (thr_h_env_x0),
    .h_env_z0_o    (thr_h_env_z0),
    .h_src_id_o    (thr_h_src_id),
    .h_ok_o        (thr_h_ok),
    .h_verdict_o   (thr_h_verdict),

    // REAL: the same job, forwarded to the streamer's share after the header
    // has been handed over.  UNCONDITIONAL -- a page whose header could not be
    // read still streams, because the streamer's completion is the page's
    // unpin and a swallowed job would park the directory entry forever.
    .f_valid_o (thr_f_valid),
    .f_ready_i (tps_j_ready),
    .f_slot_o  (thr_f_slot),
    .f_gen_o   (thr_f_gen),
    .f_epoch_o (thr_f_epoch),
    .f_src_id_o(thr_f_src_id),
    .f_flags_o (thr_f_flags),

    .headers_read_o   (terr_hr_headers_o),
    .headers_refused_o(terr_hr_refused_o),
    .guard_denied_o   (terr_hr_guard_denied_o),
    .incomplete_o     (terr_hr_incomplete_o),
    .ident_fails_o    (terr_hr_ident_fails_o),
    .idle_o           (terr_hr_idle_o)
  );

  // ---- THE COMPOSE PATH's ONE GUARD READ CLIENT (composed item 13) ---------
  // TWO READERS, ONE CLIENT, AND NO NEW PRIVILEGE.  `zhao_vram_arbiter` builds
  // its client tag by casting the slot index, so a genuinely new client id is a
  // memory-rules ruling and not a wire; `zhao_mem_guard` grants TERRAIN.PAGE_POOL
  // to ZHAO_CLIENT_TERRAIN_BUILD alone.  This is the same answer the geometry
  // asset path reached for the same reason, in the same block:
  // `zhao_geom_mem_adapter` is a wrapper over `zhao_mem_share2` with ENGINE1's
  // two values, and this instance is the terrain binding of it.
  //
  // WHAT IT SAVED, said in numbers rather than as a virtue: entry I26 stays a
  // TWO-port boundary instead of becoming a three-port one, the shell needs no
  // second socket, the arbiter needs no client, MEM.GUARD needs no arm, and the
  // ~200-line arbitration FSM -- with the guard's two-cycle verdict law in it,
  // the law two separate clients have already got wrong once each -- exists
  // once for the whole console rather than three times.
  //
  // CONTENTION IS REAL HERE AND IT IS COUNTED.  The two readers are NOT
  // mutually exclusive: TERRAIN.PSMUX can have the MIP PASS streaming a page
  // through requester B while the compose door's next header is read on A.
  // `terr_rdshare_contention_o` is what says how often, and it is the number
  // any decision to widen this to two outstanding requests has to be made
  // against.
  zhao_mem_share2 #(
    .CLIENT_ID (6),        // ZHAO_CLIENT_TERRAIN_BUILD -- see zhao_pkg
    .FORCE_READ(1'b1)      // both users READ; the pool's write arm is the loader's
  ) u_terrain_rdshare (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    // A: TERRAIN.HDRREAD, one 64-byte header burst per patch.
    .a_req_i       (trs_a_req),
    .a_rsp_o       (trs_a_rsp),
    .a_beat_valid_o(trs_a_beat_valid),
    .a_beat_data_o (trs_a_beat_data),
    .a_beat_last_o (trs_a_beat_last),

    // B: TERRAIN.PAGESTREAM, three plane bursts per refill.
    .b_req_i       (trs_b_req),
    .b_rsp_o       (trs_b_rsp),
    .b_beat_valid_o(trs_b_beat_valid),
    .b_beat_data_o (trs_b_beat_data),
    .b_beat_last_o (trs_b_beat_last),

    // I26: the one client leaves this module.
    .m_req_o       (terr_ps_guard_req_o),
    .m_rsp_i       (terr_ps_guard_rsp_i),
    .m_beat_valid_i(terr_ps_beat_valid_i),
    .m_beat_data_i (terr_ps_beat_data_i),
    .m_beat_last_i (terr_ps_beat_last_i),

    .jobs_a_o     (terr_rdshare_jobs_a_o),
    .jobs_b_o     (terr_rdshare_jobs_b_o),
    .denied_o     (terr_rdshare_denied_o),
    .contention_o (terr_rdshare_contention_o),
    .err_short_o  (terr_rdshare_err_short_o),
    .err_long_o   (terr_rdshare_err_long_o),
    .err_unowned_o(terr_rdshare_err_unowned_o)
  );

  // ---- TERRAIN.PAGESTREAM --------------------------------------------------
  // THE POOL IS THE LOADER'S POOL, said by passing the same three parameters
  // rather than by both defaulting to the same numbers: this block reads back
  // exactly the bytes `u_terrain_pageloader` wrote, and a page size or a base
  // that agreed only by coincidence would read half a page from the right slot.
  zhao_terrain_pagestream #(
    .PAGE_BYTES  (TERR_PAGE_BYTES),
    .REGION_BASE (TERR_POOL_BASE),
    .REGION_SLOTS(TERR_POOL_SLOTS),
    .SLOTW       (TERR_MEMSLOT),
    .GENW        (TERR_GENW)
  ) u_terrain_pagestream (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    .cfg_vram_client_i(ZHAO_CLIENT_TERRAIN_BUILD),
    .cfg_epoch_i      (terr_cfg_epoch_i),

    // The same zero extension across the pool's extra refusal bit that
    // `u_terrain_pageloader` gets, written here rather than assumed.
    // SHARED FROM 2026-09-19.  The job port is no longer the compose door's
    // alone: `u_terrain_psmux` (composition item 12) puts the MIP PASS on it
    // too, so what arrives here is whichever client the share granted.  The
    // compose door's own side is unchanged and is still `tps_j_*` -- every
    // assign below the engine's note reads exactly as it did.
    .j_valid_i (tpsx_j_valid),
    .j_ready_o (tpsx_j_ready),
    .j_slot_i  (tpsx_j_slot),
    .j_gen_i   (tpsx_j_gen),
    .j_epoch_i (tpsx_j_epoch),
    .j_src_id_i(tpsx_j_src_id),
    .j_flags_i (tpsx_j_flags),

    // I26, extended: requester B of the one guard read client.  This used to
    // reach the module edge directly; from 2026-09-19 it reaches it through
    // `u_terrain_rdshare`, which the streamer cannot tell apart -- the share
    // reproduces the guard's level-then-pulse verdict law deliberately, and the
    // `last` it returns is the accepted request's own expected word count.
    .guard_req_o (trs_b_req),
    .guard_rsp_i (trs_b_rsp),
    .beat_valid_i(trs_b_beat_valid),
    .beat_data_i (trs_b_beat_data),
    .beat_last_i (trs_b_beat_last),

    // The vertex DATA is broadcast to both clients and the HANDSHAKE is
    // demuxed by the share's captured owner, which is what makes this a share
    // and not a buffer: no beat is stored, copied or reordered anywhere.
    .v_valid_o (tpsx_v_valid),
    .v_ready_i (tpsx_v_ready),
    .v_base_o  (tps_v_base),
    .v_scar_o  (tps_v_scar),
    .v_bottom_o(tps_v_bottom),
    .v_vi_o    (tps_v_vi),
    .v_vj_o    (tps_v_vj),
    .v_first_o (tps_v_first),
    .v_last_o  (tps_v_last),
    .v_slot_o  (tps_v_slot),
    .v_gen_o   (tps_v_gen),
    .v_epoch_o (tps_v_epoch),
    .v_src_id_o(tps_v_src_id),
    .v_flags_o (tps_v_flags),

    // REAL: one job, one completion, and the completion is the UNPIN.
    .done_valid_o  (tpsx_done_valid),
    .done_ready_i  (tpsx_done_ready),
    .done_slot_o   (tps_done_slot),
    .done_gen_o    (tps_done_gen),
    .done_epoch_o  (tps_done_epoch),
    .done_ok_o     (terr_ps_done_ok_o),
    .done_verdict_o(terr_ps_done_verdict_o),
    .done_src_id_o (tps_done_src_id),

    .lattices_streamed_o(terr_ps_lattices_o),
    .lattices_refused_o (terr_ps_lattices_refused_o),
    .vertices_streamed_o(terr_ps_vertices_o),
    .bursts_read_o      (terr_ps_bursts_o),
    .guard_denied_o     (terr_ps_guard_denied_o),
    .incomplete_o       (terr_ps_incomplete_o),
    .idle_o             (terr_ps_idle_o)
  );

  // ---- TERRAIN.PATCH -------------------------------------------------------
  // NO GLUE ON THE HEIGHT PATH: base/scar/bottom and the two lattice indices go
  // straight across, which is why the streamer emits three planes on one beat.
  zhao_terrain_patch u_terrain_patch (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    // "Asserted once per patch per frame, BEFORE the first record."  The job
    // acceptance is that cycle: the streamer has taken the patch and has not yet
    // read a byte of it.
    .list_clear_i(tce_job_take),
    .patch_id_i  (tis_src_id[15:0]),

    // I34: the section 9.1 live-field list intake.  FIELD.SEQ.EARTH is not built.
    .fld_add_valid_i(terr_pt_fld_add_valid_i),
    .fld_add_ready_o(terr_pt_fld_add_ready_o),
    .fld_add_x0_i   (terr_pt_fld_add_x0_i),
    .fld_add_z0_i   (terr_pt_fld_add_z0_i),
    .fld_add_x1_i   (terr_pt_fld_add_x1_i),
    .fld_add_z1_i   (terr_pt_fld_add_z1_i),
    .fld_add_hash_i (terr_pt_fld_add_hash_i),
    .fld_add_cmd_i  (terr_pt_fld_add_cmd_i),

    .fld_add_accept_o(terr_pt_fld_add_accept_o),
    .fld_add_reject_o(terr_pt_fld_add_reject_o),
    .fields_active_o (terr_pt_fields_active_o),

    .trace_patch_id_o   (terr_pt_trace_patch_id_o),
    .trace_hash_o       (terr_pt_trace_hash_o),
    .trace_cmd_o        (terr_pt_trace_cmd_o),
    .programs_rejected_o(terr_pt_programs_rejected_o),

    // REAL: the streamer's lattice, gated on the placement being legal.
    // The source id NARROWS from 32 bits to 16 on this seam.  That is the
    // streamer carrying T5's 32-bit record id against TERRAIN.PATCH's 16-bit
    // trace field, and it is named here rather than left to a truncation nobody
    // wrote down -- the same narrowing `tb_terrain_compose.sv` records.
    .vtx_valid_i(tpt_vtx_valid),
    .vtx_ready_o(tpt_vtx_ready),
    .base_i     (tps_v_base),
    .scar_i     (tps_v_scar),
    .bottom_i   (tps_v_bottom),
    .dual_i     (tps_v_flags[TERR_FLAG_DUAL_BIT]),
    .wx_i       (tpc_wx),
    .wz_i       (tpc_wz),
    .vi_i       (tps_v_vi),
    .vj_i       (tps_v_vj),
    .src_id_i   (tps_v_src_id[15:0]),

    // I34: the field-height lane.  NOT tied to a constant height -- see (d).
    .fld_valid_i (terr_pt_fld_valid_i),
    .fld_ready_o (terr_pt_fld_ready_o),
    .fld_height_i(terr_pt_fld_height_i),
    .fld_covers_o(terr_pt_fld_covers_o),

    // REAL: TERRAIN.COMPCACHE's fill port, port-for-port, with the cache's own
    // ready as the backpressure.  The cache accepts on alternate clocks -- one
    // record is two writes -- and this lane obeys it rather than free-running.
    .st_valid_o      (tpt_st_valid),
    .st_ready_i      (tcc_st_ready),
    .top_o           (tpt_top),
    .bottom_o        (tpt_bottom),
    .compose_top_o   (tpt_compose_top),
    .st_dirty_o      (tpt_st_dirty),
    .st_src_id_o     (tpt_st_src_id),
    .subpatch_dirty_o(terr_pt_subpatch_dirty_o),

    .terrain_samples_evaluated_o(terr_pt_samples_o),
    .idle_o                     (terr_pt_idle_o)
  );

  // ---- TERRAIN.COMPCACHE (the front) --------------------------------------
  zhao_terrain_compcache_front #(
    .LAT_W(33),
    .LAT_H(33)
  ) u_terrain_compcache (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    // The fill begins on the vertex that IS the first, not on a cycle this
    // composition counted, so a lattice that began somewhere else would start
    // the fill somewhere else too instead of quietly filling from the middle.
    .fill_start_i (tcc_fill_start),
    .fill_accept_o(tcc_fill_accept),
    .fill_busy_o  (tcc_fill_busy),

    .st_valid_i (tpt_st_valid),
    .st_ready_o (tcc_st_ready),
    .st_top_i   (tpt_top),
    .st_bottom_i(tpt_bottom),
    .st_src_id_i(tpt_st_src_id),

    // REAL: TERRAIN.PLACE's 33 column x's and 33 row z's.
    .pos_we_i  (tpc_pos_we),
    .pos_axis_i(tpc_pos_axis),
    .pos_idx_i (tpc_pos_idx),
    .pos_val_i (tpc_pos_val),

    // I32, extended: layer D.  TERRAIN.BAKE writes cell substance and is not
    // composed; TERRAIN.PAGESTREAM reads planes A, B and C and not D.
    .cs_we_i         (terr_cc_cs_we_i),
    .cs_w_ci_i       (terr_cc_cs_ci_i),
    .cs_w_cj_i       (terr_cc_cs_cj_i),
    .cs_w_substance_i(terr_cc_cs_substance_i),

    .dual_i(tps_v_flags[TERR_FLAG_DUAL_BIT]),

    .fill_done_o(terr_cc_fill_done_o),

    // I21, extended: the retirement pulse's owner is the subpatch issuer.
    .serve_release_i(terr_cc_serve_release_i),
    .serve_valid_o  (terr_cc_serve_valid_o),
    .serve_src_id_o (terr_cc_serve_src_id_o),

    // REAL: TERRAIN.TESS's lattice and cell-state read ports.  Entry I22.
    .lat_req_i    (tt_lat_req),
    .lat_vi_i     (tt_lat_vi),
    .lat_vj_i     (tt_lat_vj),
    .lat_surface_i(tt_lat_surface),
    .lat_h_o      (tcc_lat_h),
    .lat_wx_o     (tcc_lat_wx),
    .lat_wz_o     (tcc_lat_wz),

    .cs_req_i      (tt_cs_req),
    .cs_ci_i       (tt_cs_ci),
    .cs_cj_i       (tt_cs_cj),
    .cs_substance_o(tcc_cs_substance),

    .fill_records_o  (terr_cc_fill_records_o),
    .patches_filled_o(terr_cc_patches_filled_o),
    .patches_served_o(terr_cc_patches_served_o),
    .fill_overrun_o  (terr_cc_fill_overrun_o),
    .lat_oob_o       (terr_cc_lat_oob_o),
    .cs_oob_o        (terr_cc_cs_oob_o)
  );

  // ==========================================================================
  // 11. THE GEOMETRY ASSET PATH.  MESHFETCH and ASSETFETCH behind ONE ENGINE1
  //     client, the cull they share a matrix bank with, and the index walk.
  //
  //       GEOM.MESHFETCH --- guard A ---\
  //                                      GEOM.MEM_ADAPTER -> the shell's ONE
  //       GEOM.ASSETFETCH -- guard B ---/   geometry MEM.GUARD socket
  //             |  v_*  -> GEOM.VDECODE        (entry I23, CLOSED)
  //             |  s_*  -> GEOM.ASSEMBLE
  //             '  ix_* <-> GEOM.ASSEMBLE      (the index service)
  //
  //  Header connected item 11 carries the argument.  Declared at the END of the
  //  instantiation region and not beside GEOM.VDECODE, even though it feeds it,
  //  because these five blocks are one subsystem and splitting a subsystem
  //  across two places in a file is how one half gets edited.
  // ==========================================================================

  // The memory-client identity.  Entry I40: assigned here, not tied off.
  localparam zhao_client_e GEOM_ASSET_CLIENT_C = ZHAO_CLIENT_ENGINE1;

  // ---- GEOM.MESHFETCH <-> GEOM.CULL ----------------------------------------
  wire               mf_cull_tick, mf_cull_ready, mf_cull_valid, mf_cull_reject;
  wire        [ 1:0] mf_cull_active, mf_cull_vis;
  wire signed [31:0] mf_cull_cx, mf_cull_cy, mf_cull_cz, mf_cull_radius;

  // ---- GEOM.MESHFETCH -> GEOM.ASSETFETCH: the meshlet record ---------------
  wire        mf_r_valid, mf_r_ready;
  wire [15:0] mf_r_instance_id;
  wire [31:0] mf_r_vertex_offset, mf_r_index_offset;
  wire [ 7:0] mf_r_vertex_count, mf_r_triangle_count;
  // The descriptor's visible mask, material and flags are read by nothing in
  // this module.  They are DECLARED here and left unread rather than hidden
  // behind an empty port connection: entry I39 says why the material in
  // particular may not be handed to GEOM.ASSEMBLE from this register.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [ 1:0] mf_r_visible_mask;
  wire [15:0] mf_r_material_id;
  wire [ 7:0] mf_r_flags;
  /* verilator lint_on UNUSEDSIGNAL */
  wire [31:0] mf_refused [7];

  // ---- the two guard requesters, and the one client they share -------------
  zhao_guard_req_t mf_guard_req, af_guard_req, ma_m_req;
  zhao_guard_rsp_t mf_guard_rsp, af_guard_rsp, ma_m_rsp;
  wire        mf_beat_valid, af_beat_valid, ma_m_beat_valid;
  wire [63:0] mf_beat_data,  af_beat_data,  ma_m_beat_data;
  wire        mf_beat_last,  af_beat_last,  ma_m_beat_last;

  // ---- GEOM.ASSETFETCH -> GEOM.VDECODE: the 32-byte vertex record ----------
  wire         af_v_valid, af_v_ready;
  wire [255:0] af_v_bytes;
  wire [15:0]  af_v_src_id;

  // ---- GEOM.ASSETFETCH <-> GEOM.ASSEMBLE -----------------------------------
  wire        af_s_valid, af_s_ready;
  wire [ 7:0] af_s_vertex_count, af_s_triangle_count;
  wire [15:0] af_s_src_id;
  wire        asm_ix_req, af_ix_valid;
  wire [ 8:0] asm_ix_index;
  wire [ 7:0] af_ix_a, af_ix_b, af_ix_c;

  // --------------------------------------------------------------------------
  // GEOM.CULL.  MUL_LANES is LEFT AT THE BLOCK'S OWN DEFAULT (2, two shared
  // 33x33 lanes with the products registered); restating it here would put a
  // second copy of a measured frontier in a composer.
  // --------------------------------------------------------------------------
  zhao_geom_cull u_geom_cull (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL, AND SHARED: the same merged matrix bank CMD.EXEC's SetView drives
    // into `u_proj_subsystem` (entry I14's closed half).  This block's own port
    // comment says its words are "EXACTLY the words zhao_geom_project takes at
    // the same addresses", and it ignores addr >= 16 on purpose -- rejection
    // happens in clip space, so the viewport words pass it harmlessly.  One
    // camera, one bank, no second opinion about where it is.
    .cfg_we_i  (proj_cfg_we_m),
    .cfg_view_i(proj_cfg_view_m),
    .cfg_addr_i(proj_cfg_addr_m),
    .cfg_data_i(proj_cfg_data_m),

    // REAL: the instance bounding sphere, from GEOM.MESHFETCH's descriptor.
    .tick_i    (mf_cull_tick),
    .active_i  (mf_cull_active),
    .centre_x_i(mf_cull_cx),
    .centre_y_i(mf_cull_cy),
    .centre_z_i(mf_cull_cz),
    .radius_i  (mf_cull_radius),
    .ready_o   (mf_cull_ready),

    // REAL: the verdict, back to the block that asked.
    .valid_o (mf_cull_valid),
    .vis_o   (mf_cull_vis),
    .reject_o(mf_cull_reject)
  );

  zhao_geom_meshfetch u_geom_meshfetch (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // I36: the DRAW.  CMD.SCHEDULER is the absent owner; see the header.
    .j_valid_i      (geom_mf_job_valid_i),
    .j_ready_o      (geom_mf_job_ready_o),
    .j_instance_id_i(geom_mf_job_instance_id_i),
    .j_desc_addr_i  (geom_mf_job_desc_addr_i),
    .j_format_i     (geom_mf_job_format_i),
    .j_generation_i (geom_mf_job_generation_i),
    .j_active_mask_i(geom_mf_job_active_mask_i),
    .j_xform_i      (geom_mf_job_xform_i),
    .j_client_i     (GEOM_ASSET_CLIENT_C),   // I40: assigned here

    // REAL: requester A of the shared ENGINE1 client.
    .guard_req_o (mf_guard_req),
    .guard_rsp_i (mf_guard_rsp),
    .beat_valid_i(mf_beat_valid),
    .beat_data_i (mf_beat_data),
    .beat_last_i (mf_beat_last),

    // I37: the descriptor's CRC verdict.  `zhao_crc32c_fold` is the fold step
    // and exists; the walker that runs it over the returning beats does not.
    .crc_ok_i(geom_mf_crc_ok_i),

    // REAL: the cull service, which is a block and not a played answer.
    .cull_tick_o  (mf_cull_tick),
    .cull_active_o(mf_cull_active),
    .cull_cx_o    (mf_cull_cx),
    .cull_cy_o    (mf_cull_cy),
    .cull_cz_o    (mf_cull_cz),
    .cull_radius_o(mf_cull_radius),
    .cull_ready_i (mf_cull_ready),
    .cull_valid_i (mf_cull_valid),
    .cull_vis_i   (mf_cull_vis),
    .cull_reject_i(mf_cull_reject),

    // REAL: the meshlet record into GEOM.ASSETFETCH.  Ordinary ready/valid, so
    // one accepted record is one meshlet -- the re-submission fault the shell
    // bench hit came from driving `m_valid_i` off a held LEVEL, and there is no
    // level here.
    .r_valid_o         (mf_r_valid),
    .r_ready_i         (mf_r_ready),
    .r_instance_id_o   (mf_r_instance_id),
    .r_visible_mask_o  (mf_r_visible_mask),
    .r_vertex_offset_o (mf_r_vertex_offset),
    .r_index_offset_o  (mf_r_index_offset),
    .r_vertex_count_o  (mf_r_vertex_count),
    .r_triangle_count_o(mf_r_triangle_count),
    .r_material_id_o   (mf_r_material_id),
    .r_flags_o         (mf_r_flags),

    .meshlets_considered_o(geom_mf_meshlets_considered_o),
    .culled_all_cameras_o (geom_mf_culled_all_cameras_o),
    .descriptors_fetched_o(geom_mf_descriptors_fetched_o),
    .guard_denied_o       (geom_mf_guard_denied_o),
    .refused_o            (mf_refused)
  );

  // The seven refusal rows, in the block's own documented order.  Split rather
  // than or-ed together because they have seven different causes.
  assign geom_mf_refused_format_o         = mf_refused[0];
  assign geom_mf_refused_crc_o            = mf_refused[1];
  assign geom_mf_refused_generation_o     = mf_refused[2];
  assign geom_mf_refused_vertex_count_o   = mf_refused[3];
  assign geom_mf_refused_triangle_count_o = mf_refused[4];
  assign geom_mf_refused_reserved_o       = mf_refused[5];
  assign geom_mf_refused_zero_bound_o     = mf_refused[6];

  // --------------------------------------------------------------------------
  // GEOM.MEM_ADAPTER.  The whole reason the geometry front end can be in this
  // console at all: two logical requesters, one permitted client.  It forces
  // `client` to ENGINE1 and `write` low itself (its section 11.2), so the
  // identity above is the truthful value rather than the load-bearing one.
  // --------------------------------------------------------------------------
  zhao_geom_mem_adapter u_geom_mem_adapter (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: requester A, GEOM.MESHFETCH's 32-byte descriptors.
    .a_req_i       (mf_guard_req),
    .a_rsp_o       (mf_guard_rsp),
    .a_beat_valid_o(mf_beat_valid),
    .a_beat_data_o (mf_beat_data),
    .a_beat_last_o (mf_beat_last),

    // REAL: requester B, GEOM.ASSETFETCH's 64-byte payload lines.
    .b_req_i       (af_guard_req),
    .b_rsp_o       (af_guard_rsp),
    .b_beat_valid_o(af_beat_valid),
    .b_beat_data_o (af_beat_data),
    .b_beat_last_o (af_beat_last),

    // REAL: the one permitted client, into the shell's MEM.GUARD socket.
    .m_req_o      (ma_m_req),
    .m_rsp_i      (ma_m_rsp),
    .m_beat_valid_i(ma_m_beat_valid),
    .m_beat_data_i (ma_m_beat_data),
    .m_beat_last_i (ma_m_beat_last),

    .jobs_a_o     (geom_ma_jobs_a_o),
    .jobs_b_o     (geom_ma_jobs_b_o),
    .denied_o     (geom_ma_denied_o),
    .contention_o (geom_ma_contention_o),
    .err_short_o  (geom_ma_err_short_o),
    .err_long_o   (geom_ma_err_long_o),
    .err_unowned_o(geom_ma_err_unowned_o)
  );

  zhao_geom_assetfetch #(
    .MAX_VERTICES  (GEOM_ASSET_MAX_VERTICES),
    .MAX_TRIANGLES (GEOM_ASSET_MAX_TRIANGLES),
    .SRCW          (16)
  ) u_geom_assetfetch (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: GEOM.MESHFETCH's own result record.  The two offsets are
    // POOL-RELATIVE BYTES on both sides of this seam -- this block adds
    // `ZHAO_GEOM_ASSET_BASE` itself -- so nothing is rebased here.
    .m_valid_i         (mf_r_valid),
    .m_ready_o         (mf_r_ready),
    .m_vertex_offset_i (mf_r_vertex_offset),
    .m_index_offset_i  (mf_r_index_offset),
    .m_vertex_count_i  (mf_r_vertex_count),
    .m_triangle_count_i(mf_r_triangle_count),
    // I40: the job's instance id, echoed beside the meshlet it describes, so
    // the label cannot drift from the thing it labels.
    .m_src_id_i        (mf_r_instance_id),
    .m_client_i        (GEOM_ASSET_CLIENT_C),

    // REAL: requester B of the shared ENGINE1 client.
    .guard_req_o (af_guard_req),
    .guard_rsp_i (af_guard_rsp),
    .beat_valid_i(af_beat_valid),
    .beat_data_i (af_beat_data),
    .beat_last_i (af_beat_last),

    // REAL: the servable meshlet, into GEOM.ASSEMBLE.
    .s_valid_o         (af_s_valid),
    .s_ready_i         (af_s_ready),
    .s_vertex_count_o  (af_s_vertex_count),
    .s_triangle_count_o(af_s_triangle_count),
    .s_src_id_o        (af_s_src_id),

    // I38: the release.  Its owner is whoever knows BOTH readers are done, and
    // neither GEOM.VDECODE nor GEOM.ASSEMBLE emits that.  See the header for
    // what an unreleased buffer reads like, because it looks like a stall.
    .release_i(geom_af_release_i),

    // REAL: the index service, GEOM.ASSEMBLE's other half.
    .ix_req_i  (asm_ix_req),
    .ix_index_i(asm_ix_index),
    .ix_valid_o(af_ix_valid),
    .ix_a_o    (af_ix_a),
    .ix_b_o    (af_ix_b),
    .ix_c_o    (af_ix_c),

    // REAL: the 32-byte vertex record into GEOM.VDECODE.  Entry I23, closed.
    .v_valid_o (af_v_valid),
    .v_ready_i (af_v_ready),
    .v_bytes_o (af_v_bytes),
    .v_src_id_o(af_v_src_id),

    .meshlets_fetched_o  (geom_af_meshlets_fetched_o),
    .beats_read_o        (geom_af_beats_read_o),
    .guard_denied_o      (geom_af_guard_denied_o),
    .refused_footprint_o (geom_af_refused_footprint_o),
    .prefetch_stall_o    (geom_af_prefetch_stall_o),
    .err_beat_truncated_o(geom_af_err_beat_truncated_o),
    .err_beat_overrun_o  (geom_af_err_beat_overrun_o),
    .err_beat_unowned_o  (geom_af_err_beat_unowned_o)
  );

  zhao_geom_assemble #(
    .MAX_VERTICES  (GEOM_ASSET_MAX_VERTICES),
    .MAX_TRIANGLES (GEOM_ASSET_MAX_TRIANGLES),
    .VIDW          (GEOM_ASM_VIDW),
    .SRCW          (16)
  ) u_geom_assemble (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: the meshlet, from GEOM.ASSETFETCH's servable port.  The counts come
    // from the block that has BUFFERED the footprint, so they hold still for
    // the whole walk by construction rather than by a latch this file added.
    .m_valid_i         (af_s_valid),
    .m_ready_o         (af_s_ready),
    .m_vertex_count_i  (af_s_vertex_count),
    .m_triangle_count_i(af_s_triangle_count),
    .m_src_id_i        (af_s_src_id),

    // I39: the three fields with no owner inside this module.
    .m_vertex_offset_i(geom_asm_vertex_offset_i),
    .m_material_id_i  (geom_asm_material_id_i),
    .m_raster_state_i (geom_asm_raster_state_i),

    // REAL: the index service.  `ix_valid_i` follows the FETCHER's valid and is
    // not tied to the request -- the served answer has a real valid, and tying
    // it high would turn a missed answer into a silently wrong triplet.
    .ix_req_o  (asm_ix_req),
    .ix_index_o(asm_ix_index),
    .ix_valid_i(af_ix_valid),
    .ix_a_i    (af_ix_a),
    .ix_b_i    (af_ix_b),
    .ix_c_i    (af_ix_c),

    // I39: the TriangleDescriptor, out to the absent replay customer of I11.
    .t_valid_o  (geom_asm_t_valid_o),
    .t_ready_i  (geom_asm_t_ready_i),
    .t_v0_o     (geom_asm_t_v0_o),
    .t_v1_o     (geom_asm_t_v1_o),
    .t_v2_o     (geom_asm_t_v2_o),
    .t_material_o(geom_asm_t_material_o),
    .t_raster_o (geom_asm_t_raster_o),
    .t_src_id_o (geom_asm_t_src_id_o),
    .t_last_o   (geom_asm_t_last_o),

    .meshlets_o      (geom_asm_meshlets_o),
    .triangles_o     (geom_asm_triangles_o),
    .refused_limits_o(geom_asm_refused_limits_o),
    .refused_index_o (geom_asm_refused_index_o)
  );

  // ==========================================================================
  // 12. THE SECOND COMPLETION -- the mip pass that makes a page GROUND.
  // ==========================================================================
  //      TERRAIN.PAGELOADER.fin (accepted, ok) -> TERRAIN.MIPREQ.ev_*
  //      TERRAIN.MIPREQ.j_*      -> TERRAIN.MIPFEED.j_*
  //      TERRAIN.MIPFEED.ps_*   <-> TERRAIN.PSMUX client B <-> TERRAIN.PAGESTREAM
  //      TERRAIN.MIPFEED.mg_*   <-> TERRAIN.MIPGEN
  //      TERRAIN.MIPFEED.fin_*   -> TERRAIN.RESIDENCY.fin_* (claimant 2)
  //
  // WHY IT IS HERE AT ALL: the directory publishes on TWO completions and this
  // core had one. See the corrected paragraph at connected item 8; the short
  // version is that `terr_res_resident_o` could not leave zero, so every
  // lookup missed, so the compose door could never be offered a patch, so the
  // whole spine above ran and produced nothing. That is not a limitation with
  // a note, it is a machine that does not work.
  //
  // NOTHING HERE IS AN ADAPTER. TERRAIN.MIPFEED's `j_*` is TERRAIN.MIPREQ's
  // `j_*` field for field; its `ps_*` is TERRAIN.PAGESTREAM's job port field
  // for field; its `mg_*` is TERRAIN.MIPGEN's control and sample ports field
  // for field; its `fin_*` is the directory's completion field for field. The
  // two blocks this packet ADDED exist because two things had no owner, and
  // both are named in the bench that stood in for them:
  //
  //   (a) THE REQUEST. `tb_terrain_world.sv`: "Something has to notice that a
  //       page has landed and ask for its mips. Nothing in `fpga/rtl` does ...
  //       that glue is a finding rather than a convenience: no contract says
  //       who owns the mip request." `zhao_terrain_mipreq` is that owner, and
  //       the trigger is read off the directory's own transition rather than
  //       chosen -- the set of pages needing mips is EXACTLY the set of loader
  //       completions the directory accepted with `ok`, because that is the
  //       event that produces ST_MIPGEN. It is a QUEUE because MIPFEED is
  //       slower than the loader (about 7,088 clocks against 6,726), and a
  //       dropped request's only symptom is a page that is never ground.
  //
  //   (b) THE STREAMER. The bench gives the mip pass a SECOND read engine and
  //       says why: "teaching it to arbitrate two would put a scheduler in the
  //       bench, and a bench that schedules is a bench whose timing is its own
  //       invention." Right for a bench, wrong here: a second
  //       `zhao_terrain_pagestream` is a MEASURED 1,649 ALM
  //       (`reports/synthesis/zhao_block_fit.json`, clean tree) against a
  //       budget already breached at 47,582 of 41,910. `zhao_terrain_psmux`
  //       shares the one that exists, round-robin, for about 20 flip-flops.
  //       It is a BLOCK and not four assigns here for the reason this file
  //       refuses every other inline arbiter: arbitration is state.
  //
  // THE ORDER IS SAFE AND IT IS NOT AN ACCIDENT OF PRIORITY. A page cannot be
  // composed until it is resident; it cannot be resident until it is mipped;
  // it cannot be mipped until it is loaded. So the share's two clients want
  // the streamer at DIFFERENT points in one page's life, and the round-robin
  // rule exists for the case where several pages are at different points at
  // once -- which is the normal case at the composed frame's eight.
  //
  // WHAT THIS DOES NOT DO: the 17x17 and 9x9 words MIPGEN produces leave this
  // module. Nothing in the tree stores them, which is entry I42, and it is the
  // reason TERRAIN.LOD's `sp_dev*` has no producer either (entry I21). The mip
  // chain is composed for the COMPLETION it emits, and the completion is real
  // whether or not the planes have a home yet -- MIPGEN decimates the page it
  // was given and reports on the page it was given, and the directory matches
  // on {slot, gen, epoch}.
  zhao_terrain_psmux #(
    .SLOTW(TERR_MEMSLOT),
    .GENW (TERR_GENW)
  ) u_terrain_psmux (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    // CLIENT A -- the compose door.
    //
    // EVERY FIELD IS TERRAIN.HDRREAD's LATCHED COPY FROM 2026-09-19, and the
    // change matters more than it looks.  These used to read `tis_*` live, which
    // was correct while the streamer's job and TERRAIN.SEQ's acceptance were the
    // SAME CYCLE.  They no longer are: the header reader accepts the job first,
    // retires `is_valid_o`, spends a burst reading the page header, and only
    // then offers the job here -- by which time the sequencer may be presenting
    // the next patch.  Wiring `tis_*` to this port after that would stream page
    // N's slot under page M's identity, with every handshake and every counter
    // agreeing.  The zero extension across the pool's extra refusal bit still
    // happens, one block earlier, at `u_terrain_hdrread.j_slot_i`.
    .a_j_valid_i (tps_j_valid),
    .a_j_ready_o (tps_j_ready),
    .a_j_slot_i  (thr_f_slot),
    .a_j_gen_i   (thr_f_gen),
    .a_j_epoch_i (thr_f_epoch),
    .a_j_src_id_i(thr_f_src_id),
    .a_j_flags_i (thr_f_flags),
    .a_v_valid_o (tps_v_valid),
    .a_v_ready_i (tps_v_ready),
    // The compose pass's completion IS the unpin -- entry I27's closed half.
    .a_done_valid_o(tps_done_valid),
    .a_done_ready_i(tres_unpin_ready),

    // CLIENT B -- the mip pass.
    .b_j_valid_i (tmf_ps_valid),
    .b_j_ready_o (tmf_ps_ready),
    .b_j_slot_i  (tmf_ps_slot),
    .b_j_gen_i   (tmf_ps_gen),
    .b_j_epoch_i (tmf_ps_epoch),
    .b_j_src_id_i(tmf_ps_src_id),
    // TIED, AND THE REASON IS THE BENCH'S OWN.  The streamer carries T5's
    // record flags as identity because TERRAIN.PATCH's compose lane needs
    // `kFlagDual`; MIPGEN decimates heights and has no use for any of them, so
    // TERRAIN.MIPFEED has no flags port.  A passthrough nobody reads would be
    // worse than this zero, which says plainly that nothing on this path wants
    // them.
    .b_j_flags_i (16'd0),
    .b_v_valid_o (tmf_v_valid),
    .b_v_ready_i (tmf_v_ready),
    .b_done_valid_o(tmf_done_valid),
    .b_done_ready_i(tmf_done_ready),

    // THE SHARED STREAMER.
    .p_j_valid_o (tpsx_j_valid),
    .p_j_ready_i (tpsx_j_ready),
    .p_j_slot_o  (tpsx_j_slot),
    .p_j_gen_o   (tpsx_j_gen),
    .p_j_epoch_o (tpsx_j_epoch),
    .p_j_src_id_o(tpsx_j_src_id),
    .p_j_flags_o (tpsx_j_flags),
    .p_v_valid_i (tpsx_v_valid),
    .p_v_ready_o (tpsx_v_ready),
    .p_done_valid_i(tpsx_done_valid),
    .p_done_ready_o(tpsx_done_ready),

    .busy_o      (tpsx_busy),
    .owner_o     (tpsx_owner),
    .a_jobs_o    (terr_psmux_a_jobs_o),
    .b_jobs_o    (terr_psmux_b_jobs_o),
    .stray_v_o   (terr_psmux_stray_v_o),
    .stray_done_o(terr_psmux_stray_done_o)
  );

  // ---- TERRAIN.MIPREQ ------------------------------------------------------
  // THE EVENT IS THE ACCEPTANCE, NOT THE OFFER.  TERRAIN.PAGELOADER holds
  // `fin_valid_o` until its ready comes, so a trigger taken from the offer
  // would re-enqueue the same page on every cycle of the wait -- the same
  // level-versus-edge defect this file already records at
  // `terr_pl_slot_overflow_o`, and it would fill the queue with one page.
  //
  // THE SLOT NARROWS AND IT IS SAFE HERE FOR A STATED REASON, not by habit:
  // `tpl_fin_ready` is `tres_fin_a_v && ...`, and `tres_fin_a_v` is already
  // `!tpl_fin_over`.  So on the only cycle this pulse can fire, the pool's
  // extra bit is KNOWN clear -- the narrowing is a consequence of the refusal
  // above rather than an assumption beside it.
  zhao_terrain_mipreq #(
    .SLOTW(TERR_SLOTW),
    .GENW (TERR_GENW),
    .DEPTH(TERR_MIPQ_DEPTH)
  ) u_terrain_mipreq (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    .ev_valid_i (tpl_fin_valid && tpl_fin_ready && tpl_fin_ok),
    .ev_slot_i  (tpl_fin_slot_w[TERR_SLOTW-1:0]),
    .ev_gen_i   (tpl_fin_gen),
    .ev_epoch_i (tpl_fin_epoch),
    .ev_src_id_i(tpl_fin_src_id),
    .ev_crc_i   (tpl_fin_crc),

    .j_valid_o (tmq_j_valid),
    .j_ready_i (tmq_j_ready),
    .j_slot_o  (tmq_j_slot),
    .j_gen_o   (tmq_j_gen),
    .j_epoch_o (tmq_j_epoch),
    .j_src_id_o(tmq_j_src_id),
    .j_crc_o   (tmq_j_crc),

    .requests_o(terr_mipreq_requests_o),
    .issued_o  (terr_mipreq_issued_o),
    .drops_o   (terr_mipreq_drops_o),
    .level_o   (tmq_level),
    .idle_o    (tmq_idle)
  );

  // ---- TERRAIN.MIPFEED -----------------------------------------------------
  // IT CARRIES THE STREAMER'S SLOT WIDTH, not the directory's, because its
  // `ps_slot_o` has to reach TERRAIN.PAGESTREAM.  The extension in and the
  // narrowing out are the SAME BIT, made two lines apart, and the block's own
  // header says the identity is "returned unaltered" -- it has no arithmetic
  // that could disturb it.  Written out rather than assumed because a width
  // step nobody named is how slot 1,024 becomes slot 0.
  zhao_terrain_mipfeed #(
    .SLOTW(TERR_MEMSLOT),
    .GENW (TERR_GENW)
  ) u_terrain_mipfeed (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    .j_valid_i (tmq_j_valid),
    .j_ready_o (tmq_j_ready),
    .j_slot_i  ({1'b0, tmq_j_slot}),
    .j_gen_i   (tmq_j_gen),
    .j_epoch_i (tmq_j_epoch),
    .j_src_id_i(tmq_j_src_id),
    // THE CRC IS A TOKEN AND NOT A CLAIM.  TERRAIN.PAGELOADER checked the body
    // before anyone called it loaded and nothing on this path re-reads it; the
    // directory validates the CRC on EVERY completion it accepts, not only the
    // loader's, so a second `fin` carrying zero is a CRC FAILURE.  The bench
    // measured exactly that: "16 lattices streamed, 17,424 samples delivered,
    // 4,624 mip17 writes -- and EIGHT CRC FAILURES with zero pages resident."
    .j_crc_i   (tmq_j_crc),

    .ps_valid_o (tmf_ps_valid),
    .ps_ready_i (tmf_ps_ready),
    .ps_slot_o  (tmf_ps_slot),
    .ps_gen_o   (tmf_ps_gen),
    .ps_epoch_o (tmf_ps_epoch),
    .ps_src_id_o(tmf_ps_src_id),

    // The vertex DATA comes straight off the streamer and the HANDSHAKE comes
    // off the share, which is what makes the two clients independent without
    // anything being copied.
    .v_valid_i (tmf_v_valid),
    .v_ready_o (tmf_v_ready),
    .v_base_i  (tps_v_base),
    .v_scar_i  (tps_v_scar),
    .v_bottom_i(tps_v_bottom),
    .v_last_i  (tps_v_last),

    .ps_done_valid_i(tmf_done_valid),
    .ps_done_ready_o(tmf_done_ready),
    .ps_done_ok_i   (terr_ps_done_ok_o),

    .mg_start_o     (tmg_start),
    .mg_job_slot_o  (tmg_job_slot),
    .mg_job_gen_o   (tmg_job_gen),
    .mg_job_epoch_o (tmg_job_epoch),
    .mg_fine_valid_o(tmg_fine_valid),
    .mg_fine_ready_i(tmg_fine_ready),
    .mg_fine_h_o    (tmg_fine_h),
    .mg_done_i      (tmg_done),

    .fin_valid_o (tmf_fin_valid),
    .fin_ready_i (tmf_fin_ready),
    .fin_slot_o  (tmf_fin_slot),
    .fin_gen_o   (tmf_fin_gen),
    .fin_epoch_o (tmf_fin_epoch),
    .fin_ok_o    (tmf_fin_ok),
    .fin_crc_o   (tmf_fin_crc),
    .fin_src_id_o(tmf_fin_src_id),

    .pages_mipped_o (terr_mip_pages_mipped_o),
    .pages_faulted_o(terr_mip_pages_faulted_o),
    .samples_sent_o (terr_mip_samples_sent_o),
    .idle_o         (tmf_idle)
  );

  // ---- TERRAIN.MIPGEN ------------------------------------------------------
  zhao_terrain_mipgen #(
    .SLOTW(TERR_MEMSLOT),
    .GENW (TERR_GENW)
  ) u_terrain_mipgen (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    .start_i(tmg_start),
    .busy_o (tmg_busy),
    .done_o (tmg_done),

    .job_slot_i (tmg_job_slot),
    .job_gen_i  (tmg_job_gen),
    .job_epoch_i(tmg_job_epoch),

    // The identity comes back out and TERRAIN.MIPFEED is the block that uses
    // it; these three are exported for observability and the mip completion
    // that reaches the directory is MIPFEED's, not this one.
    .done_slot_o (tmg_done_slot),
    .done_gen_o  (tmg_done_gen),
    .done_epoch_o(tmg_done_epoch),

    .fine_valid_i(tmg_fine_valid),
    .fine_ready_o(tmg_fine_ready),
    .fine_h_i    (tmg_fine_h),

    // I42: the decimated planes.  No store exists; see the note above.
    .m17_valid_o(terr_mg_m17_valid_o),
    .m17_addr_o (terr_mg_m17_addr_o),
    .m17_surf_o (terr_mg_m17_surf_o),
    .m17_h_o    (terr_mg_m17_h_o),
    .m9_valid_o (terr_mg_m9_valid_o),
    .m9_addr_o  (terr_mg_m9_addr_o),
    .m9_surf_o  (terr_mg_m9_surf_o),
    .m9_h_o     (terr_mg_m9_h_o),

    .samples_o   (tmg_samples),
    .m17_writes_o(terr_mg_m17_writes_o),
    .m9_writes_o (terr_mg_m9_writes_o),
    .aborts_o    (terr_mg_aborts_o)
  );

  // ==========================================================================
  // FIELD.SEQ.CORE, COMPOSED ONCE -- and the S profile riding on it.
  // ==========================================================================
  // THE CORRECTION THIS INSTANCE IS. Entries I5, I31 and I34 each said their
  // owner -- FIELD.SEQ.FLOW, FIELD.SEQ.STAMP, FIELD.SEQ.EARTH -- "is not
  // built". `design/contracts/FIELD.SEQ.{FLOW,STAMP,EARTH}.md` are identical on
  // the point and say the opposite in as many words:
  //
  //   > Owner ruling, 2026-08-22: one engine, five profiles. This contract
  //   > describes a CONFIGURATION of FIELD.SEQ.CORE ... There is no separate
  //   > FIELD.SEQ.EARTH sequencer in hardware and there is not going to be one.
  //
  // `design/blocks.yml` records all five as `kind: profile` with
  // `implemented_by: FIELD.SEQ.CORE`, rule V21. So three entries were each
  // waiting on a block that is RULED never to exist, while the block that
  // implements all three -- `zhao_field_seq`, RTL_VERIFIED, fit-measured at
  // 4,494 ALM / 5 M10K / 3 DSP on a clean tree -- was outside this file
  // entirely. What each seam was actually missing is its own STREAM ADAPTER,
  // which FIELD.SEQ.CORE.md permits by name, and a program to run.
  //
  // WHAT THE SHARING SAVES. Three profiles on three engines is 13,482 ALM and
  // 9 DSP. One engine and an arbiter is 4,494 ALM and 3 DSP, so ~8,988 ALM and
  // 6 DSP are not spent -- and the program store is M10K rather than logic,
  // which is the owner's ruling that memory is the slack and ALMs are the debt.
  //
  // IT IS v3, AND AN EARLIER REVISION OF THIS FILE GOT THAT WRONG. The wrong
  // version was composed here for a reason that had gone stale:
  // `zhao_field_v3_engine`'s own header still said "a program containing SPLINE
  // or RING PARKS THAT CONTEXT FOREVER ... IT IS NOT FIXED HERE". It IS fixed --
  // `zhao_field_ops_pkg` is the one table and BOTH the executor's `is_long` and
  // the dispatcher's `dst_width_of` call it, so an op cannot be offered by one
  // and refused by the other. A comment describing a bug that no longer exists
  // is the one kind of bug that never shows up red, and this one cost a
  // composition built on a superseded generation.
  //
  // The owner's ruling of 2026-09-19 is the law here: "YOU ONLY GET TO FIT THE
  // LATEST VERSION. IF IT IS BROKEN YOU FIX IT." Fitting v1 would have spent
  // ALM and DSP on a machine that is not being shipped and produced a number
  // describing the wrong design. `tools/quartus/check_console_inventory.py` is
  // the gate that now says so mechanically, and its G1 named every one of the
  // nine v1 modules that came back out of this console's closure.
  logic [2:0] fld_sat_c;

  // ==========================================================================
  // THE FABRIC'S PARAMETERISATION IS A DECISION, AND IT IS MADE HERE
  // ==========================================================================
  // `zhao_field_v3_engine.sv`'s header names the SHIPPED configuration and
  // `tests/CMakeLists.txt:2326` proves it -- `field_v3_earth_quad`, the gate
  // the engine names as its own, verilates at exactly:
  //
  //   CTX=32 OUTSTANDING=16 LANES=4 LONGQ=16 DIST_BANKS=8 RING_UNITS=8 REGS=64
  //
  // and the engine adds "a fit that does not override them is measuring the
  // bench". Until 2026-09-19 five of those seven were LITERALS inside
  // `zhao_field_host` and could not be reached from here at all. They are
  // parameters now. What follows is why this console selects the values it
  // selects, and it is an argument rather than a default.
  //
  // ---------------------------------------------------------------------------
  // THE FRONT CANNOT SPEND THE WIDTH. THIS IS STRUCTURAL, NOT A TUNING OPINION.
  // ---------------------------------------------------------------------------
  // `zhao_field_host`'s run state machine holds ONE `state`, ONE `cur_slot`,
  // ONE `cur_in[]` and ONE `cur_out[]`. It grants one client, zeroes one
  // context, preloads one point's E record, waits for that context's `done`,
  // and answers. Its own `resp_out_o` port comment states the fact outright:
  // "exactly one point is in flight through this front". `req_in_i` carries
  // CLIENTS * IN_LANES * 32 bits -- one E record per client, never four.
  //
  // So at the shipped numbers the machine degenerates on every axis at once:
  //
  //   * LANES=4 -- the front has one point, so the other three lanes recompute
  //     it and are discarded. The linter says so at `fab_wr_data`: 96 of 128
  //     result bits unused.
  //   * CTX=32 -- the FSM never starts a second context before the first
  //     retires, so thirty-one contexts of register file are state no client
  //     can reach.
  //   * GATHERS=4 -- the dispatcher gathers four long ops from four DIFFERENT
  //     parked contexts. With one context live it can never gather more than
  //     one, so OUTSTANDING=16, LONGQ=16, DIST_BANKS=8 and RING_UNITS=8 are
  //     all downstream of a supply that does not exist.
  //
  // The deadline evidence that DEFINES the shipped configuration is
  // `field_v3_earth_quad`, which drives the engine DIRECTLY from
  // `tests/differential/field_v3_earth_directed.cpp` at 1024 points. No client
  // wired to this console can present that workload: client 0 is the S-profile
  // stamp adapter and client 1 is an edge seam, and BOTH go through the serial
  // FSM above. The EARTH adapter is not built.
  //
  // ---------------------------------------------------------------------------
  // WHAT THE SHIPPED NUMBERS WOULD COST, COUNTED RATHER THAN GUESSED
  // ---------------------------------------------------------------------------
  // Two structural counts, both exact, and neither is a fit:
  //
  //   * ROOTS. `zhao_field_v3_len` instantiates BANKS x LANES
  //     `zhao_field_isqrt` (its generate at :201-203) -- and note that
  //     `zhao_field_v3_svcpath.sv:611` passes only `.BANKS(DIST_BANKS)`, NOT
  //     LANES, so the distance service keeps its own LANES=4 default however
  //     narrow the executor is. The roots are therefore BANKS x 4 always:
  //     EIGHT here at DIST_BANKS=2, THIRTY-TWO at the shipped DIST_BANKS=8.
  //     The whole-machine map probe corroborates the eight exactly --
  //     `gen_bank[0..1].gen_root[0..3]`, 8 x 248 ALUT.
  //     `zhao_field_v3_len.sv:53` prices a root at ~251 ALM and eight at
  //     "roughly 2,000 ALMs", so the shipped point is about +6,000 ALM on this
  //     axis alone. `design/fit_targets.yml:1995` caps that module at
  //     `max_alms: 2000` -- a rule written for EIGHT roots, which the shipped
  //     configuration exceeds fourfold. That target has never been run.
  //
  //     WORTH SAYING SEPARATELY, because it is a live inefficiency and not a
  //     consequence of anything chosen here: the distance service is FOUR
  //     POINTS WIDE while this console's executor is one. Eight floor-exact
  //     roots are elaborated to serve a front that presents one point at a
  //     time. Forwarding LANES into `zhao_field_v3_len` would cut that to two
  //     and is a genuine saving, but it changes a module with its own closed
  //     tally, so it belongs in a FIELD pass rather than in this instantiation.
  //   * MULTIPLIERS. `zhao_field_v3_mulbank` hard-codes `for (l = 0; l < 4;)`
  //     -- four lanes ALWAYS, independent of LANES -- and the engine has TWO
  //     banks (`zhao_field_v3_core.sv:235`, `zhao_field_v3_svcpath.sv:768`).
  //     At LANES<4 the core ties the spare lanes to CONSTANTS, which Quartus
  //     folds away; at LANES=4 all eight are live. The whole-machine map probe
  //     measured one bank at 8 DSP, so this axis is roughly +12 DSP.
  //
  // AND THE NUMBER THAT MATTERS MOST IS THE ONE THAT IS MISSING. The console's
  // 47,582 ALM / 151 DSP figure (`reports/synthesis/zhao_block_fit.json`, row
  // `zhao_console_core@console-core-first-light`, rtlCleanAtHead true) does
  // NOT CONTAIN FIELD. `reports/THE-NUMBER-20260919.md:62-69` says so and the
  // fit's own `.sources.sha256` lists exactly one field file,
  // `zhao_field_rcp24_rom.sv`. FIELD was composed into this console after that
  // fit. So FIELD's whole cost is ADDITIVE to a budget already 5,672 ALM and
  // 39 DSP over, and the roadmap charges the complete FIELD subsystem at about
  // 13,700 ALM against a 4,500 ALM envelope AT THE BENCH POINT
  // (`reports/RESOURCE-RESCUE-ROADMAP-CURRENT-20260913.md:1198`).
  //
  // NEITHER CONFIGURATION FITS. That is the finding and it is not fixed by
  // choosing the smaller one; the smaller one is chosen because the front
  // cannot use the larger, and the budget is over either way.
  //
  // ---------------------------------------------------------------------------
  // SO: THE VALUES BELOW ARE THE CONSOLE'S, AND WHAT WOULD CHANGE THEM
  // ---------------------------------------------------------------------------
  // Every one is written out even where it equals the module default, because
  // an absent parameter reads as an oversight and this one is a decision. The
  // trigger to move them is A FRONT THAT GATHERS POINTS: when
  // `zhao_field_host` can accept FAB_LANES points per grant and keep several
  // contexts in flight, the shipped numbers become reachable and must be
  // adopted together with the area they cost. Until then, widening the fabric
  // buys area and no throughput, which is the one trade this device cannot
  // afford.
  zhao_field_host #(
    .CLIENTS  (2),
    // PROGS is one number wearing three hats: the directory's ENTRIES, the
    // executor's CONTEXT count and the front's slot space. The v3 uop store is
    // indexed by context, so a program IS a context.
    //
    // The shipped CTX is 32. Eight is kept because the front runs ONE context
    // at a time, so the other twenty-four would be register file nothing can
    // reach -- and because PROGS is the only one of these knobs that is also a
    // PORT WIDTH: SLOTW appears on five of this module's field ports, so
    // PROGS=32 changes the console's edge and `zhao_prod_top` with it. That is
    // a real change with a real cost and it belongs in the pass that builds
    // the gathering front, not ahead of it.
    .PROGS    (8),
    // The executor's PLAN depth, not a memory this file owns.
    .INSTR_N  (32),
    // The shipped REGS is 64, and 64 is also the uop encoding's native size --
    // the 64-bit word packs four SIX-bit register fields, so at 32 the top bit
    // of each is wasted (`zhao_field_host.sv` guards REGW > 6 for the other
    // end of that). It is kept at 32 anyway, and for a reason that is the
    // opposite of the usual one: REGS is the length of this front's E_ZERO
    // state, which clears the context one register per clock ONCE PER POINT.
    // REGS=64 would double that from 32 clocks to 64 on the critical path of
    // every point the console answers, and buy capacity no shipped program
    // asks for. Here the big number is the slower one.
    .REGS     (32),
    .TABLES   (2),
    .TBL_N    (64),
    .IN_LANES (12),
    .OUT_LANES(4),

    // ---- the fabric's own knobs. Shipped values in the comment, always. ----
    // FAB_LANES: shipped 4. One point per grant means three discarded lanes,
    // ~+2,200 ALM of vector ALU and ~+12 DSP for nothing.
    .FAB_LANES      (1),
    // FAB_OUTSTANDING/FAB_LONGQ: shipped 16/16. Long ops in flight across
    // PARKED CONTEXTS; with one context live the supply is one.
    .FAB_OUTSTANDING(4),
    .FAB_LONGQ      (4),
    // FAB_GATHERS: shipped 4, kept at 4. It is the only one of the seven that
    // costs nothing to leave wide and it is already the shipped value.
    .FAB_GATHERS    (4),
    // FAB_DIST_BANKS: shipped 8. THE most expensive parameter in the engine --
    // `zhao_field_v3_svcpath.sv:68` says so in as many words -- because each
    // bank is four floor-exact roots. 2 banks = 8 roots here against the
    // shipped 8 banks = 32, at ~251 ALM each.
    .FAB_DIST_BANKS (2),
    // FAB_RING_UNITS: shipped 8. `zhao_field_v3_ring.sv:164` records a sweep
    // of RING_UNITS 8/16/32 against DIST_BANKS 4/8 that "moved the frame cost
    // by not one clock" -- so even on the Earth workload this axis is already
    // known to be past its knee.
    .FAB_RING_UNITS (2),
    .FAB_RING_DESC  (2)
  ) u_field_host (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    // I42: the program loader. CMD.EXEC's TerrainField arm is the named owner.
    .ld_valid_i(fld_ld_valid_i),
    .ld_ready_o(fld_ld_ready_o),
    .ld_kind_i (fld_ld_kind_i),
    .ld_slot_i (fld_ld_slot_i),
    .ld_addr_i (fld_ld_addr_i),
    .ld_data_i (fld_ld_data_i),

    // I42: FIELD.PROGCACHE's two phases. The insert edge is internal.
    .pc_lu_valid_i     (fld_pc_lu_valid_i),
    .pc_lu_ready_o     (fld_pc_lu_ready_o),
    .pc_lu_hash_i      (fld_pc_lu_hash_i),
    .pc_lu_resp_valid_o(fld_pc_lu_resp_valid_o),
    .pc_lu_resp_ready_i(fld_pc_lu_resp_ready_i),
    .pc_lu_hit_o       (fld_pc_lu_hit_o),
    .pc_lu_slot_o      (fld_pc_lu_slot_o),
    .pc_cm_valid_i     (fld_pc_cm_valid_i),
    .pc_cm_ready_o     (fld_pc_cm_ready_o),
    .pc_cm_hash_i      (fld_pc_cm_hash_i),
    .pc_cm_ok_i        (fld_pc_cm_ok_i),
    .pc_cm_resp_valid_o(fld_pc_cm_resp_valid_o),
    .pc_cm_resp_ready_i(fld_pc_cm_resp_ready_i),
    .pc_cm_inserted_o  (fld_pc_cm_inserted_o),
    .pc_cm_evicted_o   (fld_pc_cm_evicted_o),
    .pc_cm_slot_o      (fld_pc_cm_slot_o),
    .pc_hits_o         (fld_pc_hits_o),
    .pc_misses_o       (fld_pc_misses_o),
    .pc_rejected_o     (fld_pc_rejected_o),
    .pc_evictions_o    (fld_pc_evictions_o),
    .pc_occupancy_o    (fld_pc_occupancy_o),

    // Client 0 is REAL: the S-profile adapter below. Client 1 is at this
    // module's edge and is the seam I5 and I34 will take.
    .req_valid_i ({fld_req_valid_i, sfa_req_valid}),
    .req_ready_o ({fld_req_ready_o, sfa_req_ready}),
    .req_slot_i  ({fld_req_slot_i, sfa_req_slot}),
    .req_noprog_i({fld_req_noprog_i, sfa_req_noprog}),
    .req_in_i    ({fld_req_in_i, sfa_req_in}),
    .resp_valid_o({fld_resp_valid_o, sfa_resp_valid}),
    .resp_ready_i({fld_resp_ready_i, sfa_resp_ready}),
    .resp_out_o  (fld_resp_out_c),
    .resp_status_o(fld_resp_status_c),

    .runs_o            (fld_runs_o),
    .run_faults_o      (fld_run_faults_o),
    .noprog_o          (fld_noprog_o),
    .instr_retired_o   (fld_instr_retired_o),
    .loads_o           (fld_loads_o),
    .load_defers_o     (fld_load_defers_o),
    .grants_o          (fld_grants_o),
    .contended_grants_o(fld_contended_grants_o),
    .ld_oob_o          (fld_ld_oob_o),
    .no_result_o       (fld_no_result_o),
    .exec_desync_o     (fld_exec_desync_o),
    .bank_desync_o     (fld_bank_desync_o),
    .svc_bank_desync_o (fld_svc_bank_desync_o),
    .tag_mismatch_o    (fld_tag_mismatch_o),
    .wrong_op_o        (fld_wrong_op_o),
    .unsupported_o     (fld_unsupported_o),
    .skid_overflow_o   (fld_skid_overflow_o),
    .uniform_bad_o     (fld_uniform_bad_o),
    .sat_o             (fld_sat_c)
  );

  assign fld_sat_o         = fld_sat_c;
  assign fld_resp_out_o    = fld_resp_out_c;
  assign fld_resp_status_o = fld_resp_status_c;

  // The S profile's stream adapter. It computes nothing -- no coverage, no
  // blend, no arithmetic -- because `zhao_surface_stamp`'s S2 chose to deliver
  // one record per VISITED texel rather than per COVERED texel precisely so
  // that a producer would not have to reproduce its circle geometry. The two
  // cursors are held together by the shared start (`surf_cmd_valid_m &&
  // surf_cmd_ready_int`, the same accept the stamp itself begins on) and by the
  // handshake, not by a duplicated rule.
  zhao_field_stamp_adapter #(
    .SHEET_W  (64),
    .SHEET_H  (64),
    .SLOTW    (3),
    .IN_LANES (12),
    .OUT_LANES(4)
  ) u_field_stamp_adapter (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    .cmd_fire_i    (surf_cmd_valid_m && surf_cmd_ready_int),
    .cmd_field_en_i(surf_field_en_c),

    .slot_i      (fld_stamp_slot_i),
    .slot_valid_i(fld_stamp_slot_valid_i),
    .arm_ready_o (sfa_arm_ready),

    .fld_valid_o   (sfa_fld_valid),
    .fld_ready_i   (sfa_fld_ready),
    .fld_tag_op_o  (sfa_fld_tag_op),
    .fld_strength_o(sfa_fld_strength),

    .req_valid_o  (sfa_req_valid),
    .req_ready_i  (sfa_req_ready),
    .req_slot_o   (sfa_req_slot),
    .req_noprog_o (sfa_req_noprog),
    .req_in_o     (sfa_req_in),
    .resp_valid_i (sfa_resp_valid),
    .resp_ready_o (sfa_resp_ready),
    .resp_out_i   (fld_resp_out_c),
    .resp_status_i(fld_resp_status_c),

    .stamps_o  (surf_fld_stamps_o),
    .texels_o  (surf_fld_texels_o),
    .faults_o  (surf_fld_faults_o),
    .restarts_o(surf_fld_restarts_o),
    .busy_o    (surf_fld_busy_o)
  );

endmodule : zhao_console_core
