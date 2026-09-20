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
//      ruling -- the same wall the compositor met, and answered the same way:
//      by sharing the one permitted client (post shares ENGINE0; see the
//      I15/I16 closure record at the compositor instance).
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
//  14. PART.COLLIDE STANDS ON THE LIVE TERRAIN -- entry I6, closed under owner
//      ruling R1 (2026-09-19). Two blocks and one borrowed port:
//        TERRAIN.COMPCACHE read ports <- TERRAIN.HEIGHTTAP (pass-through; TESS
//                                        first, the tap on cycles TESS leaves)
//        TERRAIN.HEIGHTTAP rsp_* (a whole CELL + both normals)
//                                     -> PART.TERRAIN_TAP's cell cache
//        PART.UPDATE -> PART.TERRAIN_TAP -> PART.COLLIDE, with t_* on the beat
//      The normal is R1's law, normalize3_approx(face_normal) of the 4.3-picked
//      triangle, computed by the tree's one implementation of normalize3_approx
//      (`zhao_field_v3_normalize`) inside the tap. The particle path never waits
//      on a terrain read -- PART.COLLIDE's contract forbids it -- so a particle
//      over an uncached cell goes on with no sample, counted, while the cell is
//      fetched behind it. Glue 4's survive/ordinal join moved WITH the new stage
//      (they ride in its sideband), because leaving it on PART.UPDATE's retire
//      would have joined each particle's verdict to the one in front of it.
//      Evidence: `tests/particles/part_terrain_tap_directed.cpp` (the three
//      blocks, differenced against zref::terrain::column_query and
//      ::collision_normal; a particle below the reference surface leaves
//      standing on it). The population ORIGIN the frame change needs has no
//      producer here and widens entry I7.
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
//  * I33 was PART.TABLE's PER-FRAME LOAD (`part_tbl_ld_*`), six ports whose
//    entry's whole argument was one sentence: "NO RATIFIED COMMAND CARRIES A
//    SPECIES DESCRIPTOR ... Inventing one here would mean this file choosing
//    what a species IS, which is owner DATA". CLOSED AND DELETED 2026-09-19
//    (gz/pfs2) under owner ruling R42, which answers it without anyone
//    choosing that: the descriptors travel as a SPECIES_TABLE page
//    (spec/cartridge.md 4, kind 13), authored by the owner and published by
//    the PublishResource this console already executes.
//    `u_part_table_loader` watches the publication for that kind, reads the
//    page as whole 64-byte lines through requester E of `u_geom_mem_adapter`
//    -- the same asset window MATERIAL.RESOLVE's record fetch uses -- and
//    hands PART.TABLE its OWN load word, {sel, index, event, data},
//    uninterpreted. Nothing in this module reads a descriptor field. The page
//    is refused WHOLE on a wrong magic, a wrong version or a count that runs
//    past the declared extent, each counted; a half-loaded species table is a
//    particle engine on a mixture of two authors' physics.
//
//  * I30 was SURFACE.STAMP's DISPATCH (`surf_cmd_*`). It closed in two halves
//    and the second one is CLOSED AND DELETED 2026-09-19 (gz/pfs2), under
//    owner ruling R45. The first half closed when CMD.EXEC grew its
//    SurfaceStamp arm: every RATIFIED field comes off a validated packet. What
//    stayed open was the ENVELOPE -- which entry I27 recorded as having "no
//    placement owner anywhere in the tree" -- and three policy bits no opcode
//    carries. R45: "The stamp's patch is resolved by the SAME world->patch law
//    `zhao_terrain_heighttap` implements (powers-of-two pitch, no divider);
//    the directory is keyed by the resulting patch coordinates. No second
//    mapping law. blend_en=0 is the ratified policy." `u_surface_dispatch` is
//    that, combinational from the stamp's own translation and the LIVE pitch
//    `ptt_pitch_c` the tap already uses, emitting the patch rectangle and the
//    key {patch_ix, patch_iz}. Nine ports are GONE from this edge rather than
//    driven: the four `surf_cmd_env_*`, the three policy bits, and
//    `surf_cmd_field_en_i` -- whose console policy was already "a stamp
//    program is resident", so the residency is the producer and the host half
//    of that AND carried no information.
//
//    WHAT REMAINS UNDER `surf_cmd_*` IS THE HOST'S OWN DISPATCH PORT, and it
//    is not a tie-off in the register's sense: every field of it also has an
//    in-core producer now (CMD.EXEC for the ratified ones, the dispatch for
//    the rest), the executor has priority with backpressure, and the host path
//    is a parallel convenience -- the same shape `part_cfg_base*` and
//    `terr_cfg_*` have. Removing it would remove function, which is the one
//    thing a closure may not do.
//
//  * I7 was PART.COLLIDE's PLANE (`part_plane_*`) and the POPULATION ORIGIN
//    (`part_pop_origin_*`), eight board pins whose entry said "No ratified
//    command carries a population descriptor to this core -- DrawPopulation
//    names a pool handle, not an origin". CLOSED AND DELETED 2026-09-19
//    (gz/pfs2) under owner ruling R41: one is ratified now. `SetPopulation`
//    0x0303 carries origin, plane and `active_count`; `u_cmd_exec` lowers it
//    on a clean verdict exactly as it lowers SetEnvironment; `u_part_pop`
//    holds it as levels and REFUSES what PART.COLLIDE's formats cannot carry,
//    counting each refusal. The refusals live in the bank rather than in the
//    executor because the widths that can be breached are PART.COLLIDE's, and
//    a second opinion about them in the command path is how two truths drift.
//    I1's four provisional `part_seed_*` ports went with it (R46): the store's
//    first generation is seeded from `active_count`, in buffer 0 by the same
//    ratification. The smoke bench no longer drives any of it -- the values
//    are in its command packet and `part_pop_handle_o` reads back the handle
//    the packet named.
//
//  * I1 was PART.STATE's GENERATION STORE (`part_rd_*`, `part_wr_*`), a
//    boundary whose text said "MEM.HPS.BRIDGE is instantiated inside the shell
//    and has no particle client port". CLOSED AND DELETED 2026-09-19 (gz/pfs).
//    The provider was never the shell's bridge in particular -- it was a
//    STREAMER, which nothing in the tree was: PART.STATE.md puts both
//    generations in HPS DDR ("Dense sequential ping-pong ... Owns the two
//    particle buffers in HPS DDR and nothing else on chip", 512 KiB each, so
//    not M10K by contract or by arithmetic), and `zhao_part_state` streams
//    records without knowing memory exists. `zhao_part_hps` is that streamer,
//    composed as `u_part_hps`: ping-pong buffers swapped per tick, 64-byte
//    bursts, the next generation's count taken from what was written. It is
//    client 3 of `u_terr_hps_arb`, the socket this module already exposes
//    (entry I26 -- the same move TERRAIN.WRITEBACK made when I28 closed).
//    `zhao_part_state` gained `rd_empty_i`: an empty generation could not end
//    its tick. Seven ports left the list; the HPS's buffer bases and its seed
//    ({buffer, count}) arrive in the `terr_cfg_*` shape (plan D10). Evidence:
//    tests/particles/part_hps_directed.cpp (93 checks; a record crosses DDR
//    twice, bit for bit) and the smoke bench, whose particles now come out of
//    its played DDR and go back into it.
//
//  * I6 was PART.COLLIDE's TERRAIN SAMPLE (`part_ter_*`), a boundary because
//    "the collision test needs {height, nx, ny, nz}" and nothing in the tree
//    emitted a normal -- the law for one was contradicted in writing
//    (terrain_rules 4.4 against TERRAIN.NORMALS.md:194). CLOSED 2026-09-19 by
//    owner ruling R1 and composed item 14; the five ports are gone from the
//    list rather than driven. The one new input the frame change needs, the
//    population origin, joined entry I7 rather than opening a new entry: it is
//    the same kind of per-frame population value with the same absent owner.
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
//  * I37 was GEOM.MESHFETCH's DESCRIPTOR CRC VERDICT (`geom_mf_crc_ok_i`), a
//    MISSING BLOCK rather than missing wiring. CLOSED AND DELETED 2026-09-19.
//    `zhao_crc32c_fold` was the one fold law and is combinational; nothing
//    owned the WALKER that runs it across the eight returning beats, stops at
//    byte 60 and compares with 60..63. `zhao_geom_desc_crc` is that block,
//    composed as `u_geom_desc_crc` on requester A's own beat nets, and its
//    directed test differences it against `zhao_abi::zhao_crc32c` itself
//    (the call `zref_meshfetch.hpp:139` refuses on): 728 checks, all three
//    counters fired by legal stimulus. A burst that is not eight beats is
//    REFUSED and counted apart from a CRC mismatch. The port left this list
//    rather than being driven; `geom_mf_crc_descriptors_o`/`_fail_o`/
//    `_framing_o` leave instead, and the smoke fixture now writes the real CRC.
//
//  * I11 was GEOM.GROUP_SEQ's job port and sealed-group output, and I38
//    GEOM.ASSETFETCH's meshlet release. BOTH CLOSED AND DELETED 2026-09-19 (geom
//    packet). I11 wrote down the block it was waiting for -- "take a sealed
//    handle here and a TriangleDescriptor {v0,v1,v2} from GEOM.ASSEMBLE; issue
//    THREE lookups on GEOM.PROJ_LANE ... present {ax,ay,bx,by,cx,cy,behind} to
//    GEOM.CLIP; pulse `rel_valid_o` back" -- and `zhao_geom_replay` is that
//    block, composed as `u_geom_replay` with a directed test (78 checks, every
//    counter fired) and GEOM.DEPTHQUANT inside it. The job is issued by the
//    meshlet dispatcher fork on GEOM.ASSETFETCH's `s_*`, which offers each
//    meshlet to GROUP_SEQ, ASSEMBLE and REPLAY on one clock. I38's release is
//    PROVEN rather than guessed: handles arrive only after every vertex landed,
//    and GEOM.ASSEMBLE's new `m_done_o` ends the walk on every path -- the
//    replay releases the buffer only when both hold. Sixteen I11 ports, the
//    twelve I12 lookup/reply ports, the fourteen-port I24 triangle door, the
//    I38 release and eleven I39 ports left the list rather than being driven.
//    The smoke bench now draws the fixture meshlet through all of it, in both
//    views, and pins the pixel count the REFERENCE derives.
//
//  * I43 was GEOM.SKIN.NORM's WORLD NORMAL, out of the module because its
//    consumer GEOM.LIGHT was refused (two owners, and the one in the register was
//    the superseded scalar block). CLOSED AND DELETED 2026-09-19 by owner ruling
//    R2: `zhao_light_stream` owns vertex light. It is composed on the normal's
//    own handshake through `zhao_light_skin_adapter` -- the creature seam,
//    narrowed by assertion -- on the creature path the two blocks' contracts
//    name (magnitude supplied, early clamp). The normal still leaves as a TAP;
//    its `_ready_i` port is gone. What the light produces is GEOM.VATTR's r/g/b
//    (I46, closed), and what configures it is SetEnvironment (I48, closed).
//
//  * I48 was GEOM.LIGHT's DESCRIPTOR BANK (`geom_light_cfg_*`,
//    `geom_light_nlights_i`), host-written. CLOSED AND DELETED 2026-09-19 (geom2
//    packet) by owner ruling R25: "Promote SetEnvironment 0x0311 from reserved
//    to IMPLEMENTED. The bank's Q16.16 / u20-gain values are THE LAW; section
//    4a's u8 formula becomes a derived view with a zref bridge. CMD.EXEC lowers
//    SetEnvironment into the bank." 0x0311 is `implemented` in
//    spec/commands.zidl; the bridge is `zref::light_env::bank_of`
//    (reference/include/zref/zref_light_env.hpp), every value derived from a
//    4a sentence it quotes; CMD.EXEC stages the record like SetView and
//    presents it only on a clean verdict; `zhao_light_env` (GEOM.LIGHT.ENV,
//    `u_light_env`) computes the sun direction on its own `zhao_field_sin`,
//    writes light 0 and the environment words and commits once, holding the
//    stream's vertex input until the stream is idle, so no vertex is lit under
//    a half-written bank and no bank write can be refused. The power-on default
//    is that same path applied to 4a's default record, not a reset constant.
//    Five host ports left the list. TINT and FOG ride the record and are NOT
//    bank words; the bridge header says why and where they go.
//    Evidence: light_env_directed (every bank word of 400 random records and
//    the power-on default against the bridge, hold, supersede fired),
//    cmd_exec_directed cases 17/18, and the smoke bench, whose packet now
//    carries a SetEnvironment and whose every lit vertex is checked per channel
//    against the reference applied to THAT record.
//
//  * I46 was THE VERTEX-ATTRIBUTE STORE's WRITER (`geom_att_look_*`,
//    `geom_att_rep_*`, and GEOM.LIGHT's ready). CLOSED AND DELETED 2026-09-19
//    (geom2 packet), under owner rulings R11 and R31. It said the writer was
//    "a join between two things that move independently unless one block owns
//    both". `zhao_geom_vattr` (GEOM.VATTR) is that block, composed as
//    `u_geom_vattr` at section 11. It owns the INDEX'S DEFINITION rather than
//    joining streams: GROUP_SEQ's arena index is the ordinal of a batch's
//    decoded vertices, and the store keys u/v and colour by the same ordinal
//    of the same events; the per-view row is written at the LANDING, whose
//    rider IS {arena, index}. invw24 (GEOM.DEPTHQUANT, now streamed once per
//    landed vertex instead of three lanes per triangle corner inside REPLAY),
//    u_over_w/v_over_w (`zref::geom_over_w`, derived from qformats 8's three
//    formats), the lit colour and alpha (ALPHA_C, opaque: format 0 carries no
//    alpha and GEOM.LIGHT emits none) are all its rows; its `done_o` gates
//    GROUP_SEQ's handle so REPLAY never reads a row that is not written yet.
//    Eleven `geom_att_*` ports and `geom_light_ready_i` left the list rather
//    than being driven; REPLAY's per-view-triangle cost went from 56 clocks
//    to 5 (geom_replay_directed case B). Evidence: geom_vattr_directed (every
//    row both views against zref::depth_of_raw and zref::geom_over_w, every
//    counter fired), geom_depthquant_stream_directed, the smoke's VATTR census.
//
//  * I12 was GEOM.PROJ_LANE's ARENA ORIGIN (`geom_org_*`, `geom_rep_org_*`).
//    CLOSED AND DELETED 2026-09-19 (geom2 packet) by owner ruling R27: "No
//    arena-origin producer is owed in v1: the projector consumes WORLD
//    positions, so nothing reads the origin. Remove the dead port pair and
//    record the ruling. It carries no function, so removing it removes none."
//    The entry itself had said so -- "nothing in this console writes it and
//    nothing reads it" -- and that was checked before removing: the only
//    reader of the arena's rep_org_* was this module's edge. Eight ports left
//    the list; `zhao_geom_proj_lane` holds the shared arena primitive's origin
//    write disabled, as `zhao_terrain_wcache` always has, and the primitive's
//    own tested origin feature is untouched. If a rebased-coordinate producer
//    is ever ruled in, it arrives as a NEW entry against that ruling.
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
//  * I19 was MEASURE.HISTOGRAM's HOST READ WINDOW (`hist_rd_*`) and I45 was
//    DEBUG.TRACE's ARMING AND HOST READOUT (`dbg_trace_arm_*`,
//    `dbg_trace_clear_i`, `dbg_trace_rd_*`). BOTH CLOSED AND DELETED
//    2026-09-20 (gz/hostdbg), in one pass, because I45's own text asked for
//    that: "the same sentence I19 writes about MEASURE.HISTOGRAM applies
//    unchanged ... Closing one closes both, and they should be closed together
//    rather than twice."
//
//    THE SHARED PREMISE WAS TRUE AND IT WAS A MISSING CARRIER, NOT A MISSING
//    BLOCK. "The host is the HPS and no register path from HPS to this block
//    exists" was correct: `zhao_hps_bridge` is a 64-byte BURST engine on the
//    h2f data bridge (spec/memory_rules.md 3) and a 24-bit bin count is not in
//    DRAM for it to fetch. SEARCHED before building: `fpga/rtl` for `csr`,
//    `regwin`, `lwh2f`, `h2f_lw` and `lightweight bridge` (zero hits outside
//    ruling R51's own text); every `.sv` whose NAME contains hps, csr, bridge,
//    reg, host, dbg or debug (nine files -- `zhao_hps_bridge`,
//    `zhao_hps_arbiter`, `zhao_part_hps`, `zhao_field_host`,
//    `zhao_video_ready_bridge_v2` and the four `fpga/rtl/debug/` blocks); and
//    `spec/memory_rules.md`'s twenty-six section headings. Nothing in the tree
//    was a register aperture. `zhao_debug_counters` is the nearest thing and is
//    not one -- it STREAMS (counter_id, u64) pairs in ascending order at
//    vblank, which is a different protocol serving a different law
//    (spec/counters.md), and bending it into an address-mapped window would
//    have been a second opinion about what a host read is.
//
//    WHAT CLOSED THEM. Owner ruling R51 ratified a HOST REGISTER WINDOW on the
//    HPS LIGHTWEIGHT bridge -- the Cyclone V's SECOND host port, 32 bits wide,
//    whose whole purpose is register access. `zhao_host_regwin` is the
//    aperture (section 7b-iii), `zhao_host_reg_hist` and `zhao_host_reg_trace`
//    its first two tenants, and the map is frozen in `spec/memory_rules.md`
//    section 8. The word offset handed to a tenant is ten bits wide, so
//    no-escape is STRUCTURAL: there is no wire on which one tenant could be
//    given another's address.
//
//    AND I45'S OTHER HALF, THE ARMING, DID NOT GO TO THE APERTURE. Owner ruling
//    R52 ratified `DebugTraceArm` 0xF003 in the reserved debug range, lowered
//    by CMD.EXEC at that record's last byte, because ruling R18's principle -- ONE
//    AUTHORITY PER LEVEL -- forbids a ring with two writers and no ordering
//    between them. The aperture can READ `armed` and is refused a write, so a
//    capture is always attributable to the packet that asked for it.
//
//    WHAT DID NOT CLOSE, so the composition is not over-read: six of the seven
//    charter 20.6 stages still have no producer and arming them stores nothing.
//    That is entry I18's argument, not this one's, and it is where it lives.
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
//   RESOLVED IN THE REGISTER 2026-09-19, AND THE PARAGRAPH THAT KEPT IT OUT IS
//   SUPERSEDED BY ITS TWIN. This used to argue that the `_ALIAS` entry should
//   wait, because `design/prod_manifest.yml` counts the two projector shells
//   separately "until the composed fit closes". The geometry packet then
//   resolved GEOM.PROJECT to client A under owner ruling R3 and SEPARATED the
//   two questions: the REGISTER asks whether the capability is present in the
//   console (it is -- measured in the smoke, `proj_b_grants_o` 81 and
//   `proj_replay_triangles_o` 128 for terrain); the CENSUS asks what the fit
//   prices, and that row stays as it is until the fit. TERRAIN.PROJECT now
//   resolves to `zhao_proj_subsystem` (client B) the same way, with four
//   witnesses beside the alias in `tools/budget/completion_register.py`, and
//   `zhao_terrain_project` is `superseded` in `design/console_inventory.yml`.
//   The projected triangle's consumer is still entry I13, counted separately.
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
//   COMPOSED 2026-09-19 (owner ruling R21) AND THIS PARAGRAPH IS THE RECORD OF
//   WHAT CHANGED. The refusal above said SHADE's `sun_*` "would be a boundary
//   besides -- three lanes with no producer anywhere in `fpga/rtl`". That was
//   true and it is not any more: `zhao_light_env` computes the direction for
//   light 0's own bank words out of SetEnvironment (R25), and publishes it on
//   `sun_x/y/z_o` -- the SAME values, not a second implementation of 4a's law.
//   The other half of the refusal -- that TERRAIN.SEQ has no ModeTri arm --
//   stopped mattering when R21 moved the normal to the REPLAY stage:
//   `zhao_terrain_lightlane` stores the world vertex on the projector's fill
//   beat and reads three rows per reference, so no third tessellation pass is
//   needed and the sequencer's arm is not the question any more.
//   (GEOM.LIGHT's own instantiation of SHADE, described above, is unaffected:
//   sharing the lighting core is what that block's header asks for.)
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
//     * AND ITS OUTPUT HAS NO WRITER, which is NOT the same statement as the
//       one that stood here and the difference is worth the correction.
//
//       WHAT THIS BULLET USED TO SAY, until 2026-09-20: "AND ITS OUTPUT HAS
//       NOWHERE TO GO ... Nothing owns it." THAT IS FALSE AND THE SPEC SAYS
//       SO. `spec/memory_rules.md` section 5b ratifies the destination by
//       name: `| 0x056F_0000 .. 0x0577_FFFF | TERRAIN.COMPOSED_VELOCITY |
//       256 x 2,304 B |`. The lattice has a ratified home, an address and a
//       size. Calling it unowned was the third refusal in this file found
//       asserting an absence that a two-minute grep of `spec/` refutes, and
//       "nothing owns it" is the sentence that would have sent the next
//       person to invent a region that already exists.
//
//       WHAT IS ACTUALLY MISSING is the WRITE PATH between the two: `vv_*` is
//       the `spec/terrain_rules.md` 4.2 velocity lattice ("height16-scaled,
//       2 B/vertex, 545 KiB" per frame), the block's own header says it has
//       "no VRAM port and no lattice-sized buffer ... the 2 B/vertex store
//       belongs to whoever owns the VRAM page", and no client in this module
//       writes that region. A destination without a writer is a smaller gap
//       than a destination that does not exist, and it is a different job.
//       `moving_mask_o` remains honestly "PRODUCED, NEVER CONSUMED" -- the
//       block's own chosen law V5 -- and that half is unchanged.
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
//   TERRAIN.BAKE -- REFUSED; entry I32 carries the argument, rewritten by the
//   second sweep: its stated cause had expired (it cited I27's placement
//   blocker, which I27 itself records as closed) and the real refusal is a
//   packet-shape conflict plus two missing LAWS.
//   TERRAIN.WRITEBACK -- COMPOSED 2026-09-19 (the I28 note in the table below),
//   once owner rulings R14 and R4 answered both of its refusals.
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

// (I5 CLOSED 2026-09-20 (gz/field), under owner ruling R40. PART.UPDATE's
//      field sample has a producer: `zhao_field_flow_adapter`, the F profile's
//      stream adapter, composed as CLIENT 1 of the one `u_field_host`. The
//      entry is kept in full below rather than deleted, because it names the
//      exact thing that was missing and the next reader is owed the closure
//      against it.
//
//      WHAT CLOSED IT, ITEM BY ITEM AGAINST THE ENTRY'S OWN TEXT:
//
//      * "nothing in this tree says which fields of the 128-bit particle128
//        record become which registers". `spec/form/field-ir.md` 7.1 does, and
//        it is ratified: the flow profile's input record is
//        px,py,pz,vx,vy,vz:fx, age:u32, seed:u32, dt:fx, p0..p3:fx, mapped to
//        R0.. in that order. The adapter presents exactly that, and it unpacks
//        the record through `zhao_part_record` -- the one codec -- rather than
//        re-slicing it.
//      * "nor which registers the three s11 accelerations are read back from".
//        7.1's output record is px',py',pz',vx',vy',vz':fx, attr0:fx, and R40
//        rules the mapping: `acceleration = sat_s11((v' - v) >> 8)`, seed = the
//        variation byte, dt = 1 tick. Every constant of it is a parameter of
//        the adapter, because R40 is provisional and says particle motion is
//        judged by eye.
//      * "the host widened to 13 inputs / 7 outputs" -- R40 again, and done:
//        `u_field_host` and both adapters carry IN_LANES 13 / OUT_LANES 7.
//      * "the join is already solved" -- and it is written where the entry said
//        a composer may write it, at PART.UPDATE below, with an identity guard
//        (`part_fld_rec_changed_o`) whose two operands are clocked by different
//        things so it can actually fire.
//
//      WHAT IS NOT CLOSED BY THIS AND IS NOT A GAP: the RATE. One particle per
//      field run against PART.UPDATE's one per clock. `part_fld_stall_cycles_o`
//      measures it, and the lever is the gathering front `zhao_field_host`'s
//      header calls for, not this seam.
//
//      The original entry, unedited:
//
//      PART.UPDATE's field sample (`part_fld_*`) -- BOUNDARY, and the REASON
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
//      offer. That is a join a composer may write. The binding is not.)
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
// I13. PROJ_SUBSYSTEM's TRIANGLE OUTPUT (`proj_out_*`) -- BOUNDARY.
//      CORRECTED 2026-09-19 (geom packet): the GEOMETRY side of this sentence
//      is closed -- GEOM.REPLAY feeds GEOM.CLIP, which feeds GEOM.SETUP -- so
//      what remains is TERRAIN's replayed triangles only. Their customer is
//      the same GEOM.CLIP, which now has a producer, and joining terrain there
//      needs a two-producer triangle merge AND terrain's own attribute packet
//      (invw24 from GEOM.DEPTHQUANT for terrain w, and TERRAIN.SHADE's light):
//      terrain-lane work, named here so it is not mistaken for wiring.
//
//      HALF OF THAT IS DONE, 2026-09-19 (owner ruling R21), and the entry is
//      narrowed rather than closed. `TERRAIN.SHADE's light` in the sentence
//      above is no longer missing: `u_terrain_lightlane` computes it here --
//      the world vertex stored on the projector's own fill beat, the ratified
//      face normal, the ratified flat shade, the sun from SetEnvironment -- and
//      it leaves on `terr_light_*`, tagged with the same `src_id` the
//      triangle carries. What is STILL absent is (a) the two-producer triangle
//      merge into GEOM.CLIP and (b) terrain's `invw24` from GEOM.DEPTHQUANT.
//      The light's ports are part of THIS entry's packet and not a new
//      boundary: the same absent consumer takes both, and a light exported
//      beside a triangle it belongs to is the shape that consumer will want.
//      CLIENT B and the reference port are CLOSED: `zhao_terrain_group_seq`
//      and `zhao_terrain_tess` are composed below and drive both, so the
//      shared projector is measured here with BOTH of its clients live and
//      `proj_contended_o` can move.
//
//      CORRECTED 2026-09-20 (terrain pass 4): this entry said "the replayed
//      triangle's customer is GEOM.SETUP, which is not composed". THAT CLAUSE
//      IS FALSE. `zhao_geom_setup u_geom_setup` is instantiated in this file,
//      takes GEOM.CLIP's packet and drives the shell's triangle door with real
//      edge functions -- it landed with the geometry pass that closed the draw
//      chain, and this entry was not re-read afterwards. The smoke's
//      `raster pixels=2560` from 14 triangles is that path running.
//
//      WHAT ACTUALLY REMAINS, which the stale clause was hiding: GEOM.CLIP has
//      exactly ONE triangle producer, `rp_o_valid` from GEOM.REPLAY, and there
//      is NO TWO-PRODUCER MERGE. Terrain's projected corners leave this module
//      on `proj_out_*` and its lit colour on `terr_light_*`, and both land
//      nowhere. So the missing pieces are (a) the merge into GEOM.CLIP and
//      (b) terrain's attribute packet. Wiring the corners into a port that
//      wants edge functions would still be the hidden adapter this file must
//      not contain; that part of the old text stands.
//
//      (b) WAS UNDERSTATED AND IS CORRECTED HERE, 2026-09-20 (projinput). It
//      used to read "terrain's `invw24` -- GEOM.DEPTHQUANT lives inside
//      GEOM.VATTR and serves the geometry lane only". That is ONE slot of
//      SEVEN. `GEOM_CLIP_ATTRS = 7`: GEOM.CLIP's ratified per-corner packet is
//      invw24, u/w, v/w, lit r, g, b, alpha, and a terrain triangle entering
//      `tri_*` has to fill all of it. Terrain today has:
//
//        invw24    -- needs a DEPTHQUANT on `proj_out_aw/bw/cw`; the composed
//                     one (`zhao_geom_depthquant_stream`) is inside GEOM.VATTR
//                     on the geometry lane's tagged schedule. As the old text
//                     said.
//        u/w, v/w  -- NO PRODUCER ANYWHERE. Searched `fpga/rtl/terrain/**` for
//                     `u_over_w`, `v_over_w`, `out_u_o` and `tex_u`: ZERO hits.
//                     The projector carries terrain's `mat_a`/`mat_b`/`weight`,
//                     which is the Mosaic layer-E triple -- it names WHICH
//                     materials blend, not WHERE on them to sample. A terrain
//                     texture-coordinate law does not exist in this tree.
//        lit r/g/b -- `terr_light_base_o` is ONE signed 32-bit SCALAR shade,
//                     not three channels. Turning it into lit rgb needs the
//                     material's colour, which is the same missing binding
//                     `tri_flat_request_i` waits on (entry I49).
//        alpha     -- owner ruling R48's named constant. NOT a gap.
//
//      So this entry is blocked on TWO ABSENT LAWS and not on one absent wire,
//      and both of them are TERRAIN-lane laws with art content -- which is what
//      the sentence above already meant by "terrain-lane work, named here so it
//      is not mistaken for wiring". The difference matters for scheduling: the
//      merge in (a) is a day's work once (b) exists, and (b) is not.
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
//      STILL OPEN, and it is THREE things. It used to say two, and both of
//      those sentences were wrong when re-read on 2026-09-20 (projinput).
//
//        * THE VIEWPORT RECT, cfg addresses 16 and 17. SetView carries a
//          `viewport_id` and NOT a rectangle.
//
//          THE REST OF WHAT THIS BULLET USED TO SAY WAS FALSE, and it is the
//          expensive kind of false -- it asserted a PRESENCE and then refused
//          on it. It read: "the id-to-rectangle table is
//          `spec/video_rules.md`'s -- it is not in the ABI at all. Deriving one
//          here would be this file inventing a layout, which is the thing the
//          whole ledger exists to refuse."
//
//          THERE WAS NO SUCH TABLE IN `video_rules.md`. A case-insensitive
//          search of `spec/*.md` and `spec/*.zidl` for "viewport" returns five
//          hits: four `viewport_mask` fields on DrawSky/DrawForm, and the
//          `SetView.viewport_id` declaration. The same sentence is in
//          `zhao_cmd_exec.sv` and in owner ruling R30, which says to MOVE a
//          table that does not exist.
//
//          AND DERIVING IT WOULD INVENT NOTHING, which is the half that cost
//          the month: `zref::render::viewports_of()`
//          (`reference/src/zrender/internal.hpp:41`) has held the table since
//          the 2026-08-15 ratification -- Duo to {0,0,256,192} and
//          {0,192,256,192} returning 2, every other mode to the full canvas
//          returning 1 -- and the reference oracle is the thing RTL is
//          verified AGAINST. Two other sites had derived the same rectangles
//          independently (`terrain_project_directed.cpp:462`,
//          `GEOM.BINNER.md:25`).
//
//          The table is now WRITTEN, in `spec/video_rules.md` section 3.2,
//          citing all three. It is DERIVED and not an ABI field (owner ruling
//          R73's distinction), so the lowering costs no zidl change and no
//          capture regeneration. What is still owed here is the LOWERING --
//          CMD.EXEC indexing that table and writing cfg 16/17 -- plus R67's
//          fixture move to Duo, and one decision: section 1.1 latches the mode
//          at frame start while a SetView commits immediately, so WHICH mode
//          indexes the table is a real choice and is recorded as OPEN in 3.2
//          rather than picked here.
//
//        * `proj_en_i`. THIS BULLET USED TO SAY "and `SetView`'s OTHER FOUR
//          FIELDS ... `geometry_tokens`/`fragment_tokens` want MEASURE.TOKENS,
//          none of which is composed". TWO OF THE FOUR ARE NOT OPEN, and have
//          not been since the CMD.EXEC packet landed R18/R33: MEASURE.TOKENS
//          IS composed, as `u_measure_tokens` in this file, and both token
//          fields traverse -- `sv_gtok`/`sv_ftok` in `zhao_cmd_exec.sv` (lines
//          1203/1205) onto `tok_vreq_geom_o`/`tok_vreq_frag_o` (1529/1530) and
//          into the guard's `vreq_*_i`. The smoke prints it
//          (`SMOKE: tokens contracts=1 views=1 ... clamped=1`). The entry was
//          simply never re-read afterwards.
//
//          `proj_en_i` itself is still a tie-off and still has NO producer
//          anywhere: every instantiation of `zhao_proj_subsystem` (this file
//          and `zhao_terrain_pipe`) and of `zhao_project_service` passes `en_i`
//          straight through from its own port, and the smoke bench drives it
//          with a literal 1. It is an OWNER DECISION and not wiring, because
//          `zhao_project_core.sv:53` calls it "the rigid-pipeline enable,
//          owned by the CALLER", and BOTH callers are now internal to the
//          subsystem -- there is no caller left outside to own it. See
//          FINDINGS-projinput.md decision D-1: the recommendation is to give
//          it the CONFIG-VALID meaning the smoke bench is already faking with
//          `geom_camera_ready_q` (hold the projector off until a view's matrix
//          bank has been written), rather than R27's remove-the-dead-port
//          route, because under that reading the port does carry function.
//
//        * `pixel_error` -> MEASURE.GOVERNOR. `zhao_measure_governor` is BUILT
//          and NOT COMPOSED; its `px_err0_i`/`px_err1_i` are exactly this
//          field. That block is another lane's (the packet queue gives it to
//          POST3/MEASURE, and R68 gives its `thresh_q8` half to GEOM.LOD), so
//          THIS ENTRY CANNOT CLOSE UNTIL IT COMPOSES, whatever is done to the
//          viewport.
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
// I17. POST.COMPOSITE's gather planes and HUD (`post_gd_*`, `post_gg_*`,
//      `post_hud_*`) -- BOUNDARY. HALVES (a) AND (b) CLOSED 2026-09-19 (post
//      pass 2, owner rulings R35/R36): the look values and the grading table
//      are no longer on this edge -- see (a)/(b) at the end of this entry.
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
//          group is equally mandatory. Both end at MEM.VRAM.ARBITER. (The
//          'no spare client index' this used to cite was I15's item 2 and
//          is ANSWERED for post by sharing ENGINE0 under the render lease --
//          `zhao_post_lease`; a texel page fill is a different client with a
//          different window and is not answered by it.) That is a real wall and it
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
//
//      REFUSED AGAIN 2026-09-19 BY THE POST PACKET, which closed I15 and I16
//      beside it, and the three remaining halves each need a DECISION rather
//      than a composer. Searched, and named: `spec/commands.zidl` (no opcode
//      carries bloom gain, grading bias, flash, ink colour or a grading-table
//      load -- grep for Post/Grade/Flash/Ink/Hud/Echo returns nothing but
//      comments), `fpga/rtl/command/` (CMD.EXEC lowers SurfaceStamp,
//      PublishResource and the draw path; nothing post-shaped),
//      `fpga/rtl/synth/` and probe-named files (no post, grading or HUD probe),
//      `spec/stars_and_flares.md` 1 (the tag is `(channel << 6) | strength`
//      with ONE channel defined, GLOW = 0b01 -- no refraction, shockwave or ink
//      channel and no glow-RGB law), `design/contracts/POST.GATHER.md` ("Resolved
//      tile pixels with their material tags" -- the tag byte, not a glow RGB).
//        a. CLOSED 2026-09-19 (post pass 2, R36). `SetPost` 0x0040 carries bloom
//           gain, grade-valid, the three biases, flash colour and amount, ink
//           and POST.ECHO's ARM (R35); CMD.EXEC's EX_POST phase drives them into
//           POST.COMPOSITE (`post_look_*_w`) only while the post lease is idle,
//           and holds a pass start while it writes. zidl + generated packers +
//           `zref::post::look` + captures moved together; cmd_exec_directed
//           cases 17-21 difference the RTL against zref field for field.
//        b. CLOSED 2026-09-19 (post pass 2, R36). `SetGradeTable` 0x0041 carries
//           up to eight product vectors per record; `zref::post::look::
//           emit_grade_table` is the emitter (through `grade_product_vector`,
//           the one generator of the table), and CMD.EXEC stages entries in an
//           M10K and writes them through `pv_*` behind the same door.
//        c. `post_gd_*` / `post_gg_*` (POST.GATHER): STILL OPEN, and now with a
//           PROPOSAL in front of the owner rather than a blank. Ruling R37 asks
//           for the tag->gather law to be proposed from stars_and_flares.md 1
//           with every coefficient in a named constant, a zref model and a
//           render to judge by eye: `zref::post::gather` (knee 24, slope 0x1C,
//           tint 255/236/224, master 255), the law written into
//           design/contracts/POST.GATHER.md, and
//           reports/post-gather-law/gather_law_contact.png from
//           tools/post/gather_law_render.cpp + gather_law_sheet.py. The glow
//           borrows the fragment's own colour; DISPLACEMENT AND INK ARE NOT
//           INVENTED (channels 0b10/0b11 are unallocated, so they contribute
//           nothing and are counted). What is owed AFTER the ruling: the RTL
//           adapter, the HUD plane store, and composing zhao_post_gather.
//           `post_hud_*`: the HUD store is unbuilt, as bullet 1 says.
// (I18 CLOSED 2026-09-20, owner ruling R70 and the terrain6 packet.
//      MEASURE.HISTOGRAM's event ingress has a real producer inside this
//      module: `u_terrain_lodfeed` observes the mip pass's fine stream, walks
//      the lattice through `zhao_terrain_loddev`, and its three deviation
//      magnitudes are the events. Five ports -- `hist_ev_valid_i`,
//      `hist_ev_lane_valid_i`, `hist_ev_err_i`, `hist_ev_src_id_i` and
//      `hist_ev_ready_o` -- left this module's port list rather than being
//      driven from a harness. The metric is written into
//      `spec/measure_rules.md` section 3, the 24 -> 32 widening is declared at
//      the instance, and the traverse is shown by
//      `tests/terrain/terrain_lodhist_directed.cpp`.
//
//      AND THE THING RULING R70 MADE THE PACKET CHECK FIRST, MEASURED RATHER
//      THAN ARGUED, because it is the one fact that decides whether any of the
//      above is real. THE SMOKE'S STIMULUS DOES NOT MOVE MIPFEED'S FINE STREAM
//      AT ALL. Measured 2026-09-20 by printing the lane that had never been
//      printed:
//
//        SMOKE:   pl    loaded=0 faulted=3 ... SMOKE:   res ... crc_fail=3
//        SMOKE:   mip   mipreq requests=0 issued=0 drops=0 | mipfeed
//                       pages_mipped=0 faulted=0 samples_sent=0 | mipgen
//                       m17_writes=0 m9_writes=0 aborts=0
//
//      The cause is structural and one line up from the counters:
//      `u_terrain_mipreq`'s trigger is `tpl_fin_valid && tpl_fin_ready &&
//      tpl_fin_ok`, the bench plays zero pages whose CRC cannot match the
//      record's declared `expected_page_crc32c` (`32'hDEAD_BEEF`, written by
//      hand at `tb_zhao_console_core_smoke.sv`'s arena layout), so `fin_ok` is
//      never high, so no mip job is ever issued. EVERY number downstream of it
//      is zero for that reason and not because the chain is broken.
//
//      SO THE SMOKE'S GREEN IS NOT EVIDENCE ABOUT THIS CHAIN, and the bench
//      now says so in its own output rather than leaving the next reader to
//      infer it. What the smoke DOES assert is two things that hold at zero
//      and at a thousand: no fine sample ever arrives without a lattice start
//      (a `$fatal`, because that fault is wrong under any stimulus), and every
//      record lodfeed emits is accepted by the histogram (`dev_records ==
//      hist events`). Neither asserts the gap; both survive the fixture
//      getting better.
//
//      WHAT WOULD MAKE THE SMOKE EXERCISE IT, named because it is the next
//      cheap thing and it is not this packet's: the bench already instantiates
//      the production CRC folder (`u_bench_fold`) and already uses it to seal
//      the record list. Folding each played page the same way and writing that
//      value into the record instead of `DEAD_BEEF` would make pages load,
//      then mip, then reach this chain. It is NOT a one-line change -- the
//      page also has to satisfy `zhao_terrain_hdrread`'s identity check -- and
//      it would move the terrain spine from "no page resident" to "pages
//      resident", which is a change to what the whole bench measures. That
//      belongs to a packet that owns the fixture, with R12's reference-derived
//      pixel count regenerated in the same commit.
//
//      THE CORRECTIONS THIS ENTRY MADE ARE KEPT, because both of them were
//      quoted as blockers after they had stopped being true:
//
//        * `ev_err_i` is `LANES*EW` bits -- ONE UNSIGNED MAGNITUDE PER LANE.
//          There is no expected, no actual and no difference anywhere on this
//          block's event port; the differential sentence belongs to
//          `ev_expected_fx_i` beside `ev_actual_fx_i`, which is DEBUG.TRACE's
//          port. This entry once reasoned from that wrong port, recorded the
//          trace refusal for doing the same, and then did it again itself.
//        * DO NOT WIRE `RASTER.FRAGMENT`'s `fragment_error_o` BY NAME. It is
//          `s1_v_r && !rd_valid_i`, a one-bit tilestore-read protocol flag that
//          "should never fire", and it carries no value. The ledger's declared
//          `inputs: [fragment_error]` points at it, which is the false-PRESENCE
//          shape this header records elsewhere.
//        * DO NOT BUILD A SECOND PRODUCER. One exists, it is tested, and a
//          second would be the duplication `uncashed_cheques.py` check 3 exists
//          to catch.)
//
// I18-siblings. THE OTHER TWO MEASURE BLOCKS, and they are NOT closed by the
//      above. They are kept under this number because "the histogram is
//      composed, so its siblings must be nearly composable" is the plausible
//      reading and it is wrong.
//
//      MEASURE.TOKENS IS COMPOSED ON ITS BUDGET SIDE, 2026-09-19 (cmdmem,
//      rulings R18/R33). The refusal that stood here -- "`zhao_cmd_scheduler`
//      decodes that opcode and emits `mode_o` and nothing else" -- was true of
//      the SCHEDULER and missed the EXECUTOR: CMD.EXEC walks every record of a
//      committed packet, so it now lifts SetPresentationContract's five COUNTS
//      (the ceiling) and each SetView's two (the request) and commits them in
//      phase EX_TOK, ceiling first. MEASURE.TOKENS clamps each request to the
//      ceiling and counts every cut (`tok_vreq_clamped_o`); nothing converts,
//      and the compiler now writes counts as authored (R33 retired the
//      percentages). THE REQUEST SIDE REMAINS A BOUNDARY (`tok_req_*`,
//      `tok_ret_*`), for the reason this entry gave and which still holds:
//      GEOM.BINNER's token client is ONE BIT, tied off inside the shell as
//      `.tok_req_o(rp_tok_unused), .tok_grant_i(1'b1)`, against a seven-field
//      request whose `essential` and `rep` are policy nobody produces. That
//      shell tie-off is the specific thing a later packet must repair.
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
//        tri_flat_request_i     298b   CLOSED 2026-09-20 -- MATERIAL.RESOLVE,
//                                      through `u_material_window` (I49)
//        tri_continuation_tail_i 48b   OPEN, still a BOUNDARY
//        tri_fragment_state_i    32b   OPEN, still a BOUNDARY
//
//      TWO OPEN PORTS REMAIN and this entry stays a BOUNDARY for them. The
//      third, `tri_flat_request_i`, is retired as of 2026-09-20: it is built
//      in this module from MATERIAL.RESOLVE's published answer, field by
//      field, with the owner of each named at the assignment. Read that
//      block before re-deriving anything here -- in particular, a ZERO flat
//      request is still a LEGAL profile and is exactly what this console
//      presents until a material has actually been resolved, so the
//      retirement changes behaviour only where a real record exists.
//
//      WHAT THE FLAT REQUEST STILL CARRIES AS A CONSTANT, and why each is a
//      LAW or a RULING rather than a plausible value:
//        base_alpha    owner ruling R48 -- no ratified vertex format carries
//                      alpha, so OPAQUE is a named editable constant and
//                      nothing is stubbed;
//        base_rgb      the VERTEX's, not the material's. GEOM.VATTR holds a
//                      PER-VERTEX colour (ruling R11) and this field is a
//                      FLAT one; picking a corner would be an art decision
//                      made by a composer. This is I20's remaining half and
//                      it is named rather than quietly filled;
//        lod_q4_4      the SAMPLER's by MATERIAL.RESOLVE's contract, and the
//                      row this console programs has mip_enable low;
//        aux_*         ZERO IS THE NON-TERRAIN PROFILE, in the resolver's own
//                      words, and the tile pipe REFUSES a non-zero one that
//                      has no producer;
//        palette_slot / palette_generation
//                      ZERO IS THE BINDING ROW'S OWN LAW for a direct format
//                      (`binding_row_legal` refuses a direct row whose pair is
//                      not zero). For a CLUT row they are real and this
//                      console produces neither -- COUNTED on
//                      `mat_win_clut_unowned_o`, and an owner decision.
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
//        1. CLOSED 2026-09-19 (cmdmem). MEM.UPLOAD is COMPOSED on the shell's
//           TERRAIN.BUILD socket (the N-client HPS arbiter of ruling R4, VRAM
//           slot 6 behind the real MEM.GUARD) and its request is CMD.EXEC's
//           lowering of `PublishResource` (ruling R17). It lands a
//           MATERIAL_SET where ENGINE1 reads it since owner ruling R32
//           (provisional): MEM.GUARD's TERRAIN_BUILD arm WRITES the one
//           published-resource region (the core's `upl_cfg_region_*`, the
//           same bound MEM.UPLOAD checks) when it lies wholly inside
//           RENDER.ASSET_POOL, and nothing else of the pool. mem_guard_no_escape
//           is re-proved with it, and `tests/mutants/zhao_mem_guard_resbound_
//           mutant.sv` (containment removed) makes that proof FAIL.
//        2. CLOSED 2026-09-19 (cmdmem, R20). The record fetch is requester C
//           of `u_geom_mem_adapter`, now on the N-requester share, and a
//           refused read resolves to kFetchDenied instead of hanging.
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
//      RE-EXAMINED 2026-09-20 (gz/hostdbg), because a refusal's stated cause
//      is worth checking before it is inherited a third time. The cause above
//      is WEAKER than it reads. `zhao_video_slotmgr_v2` loads `lease_writer_q`
//      from `rsp_writer_q` -- whoever was GRANTED the lease -- so it is the
//      manager's own record rather than a policy invention, and
//      `zhao_shell_top_v2` ALREADY consumes it one line over
//      (`rmap_valid_q = v2_lease_valid && v2_lease_writer`). The substitution
//      is more defensible than this entry admits.
//
//      IT IS STILL NOT MADE, and the reason is cost rather than principle:
//      IT WOULD NOT CLOSE THIS ENTRY. The three MATERIAL.RESOLVE ports below
//      keep I20 a BOUNDARY whatever happens to `fb_writer_i`, so the trade is
//      a SHELL port removed -- which forces the paired-diff harness AND its
//      mutant to be regenerated, and re-proves a framebuffer SAFETY guard whose
//      first version let both writers pass at once -- against zero register
//      movement. It belongs in the packet that closes I49, where the triangle
//      port is being opened anyway.
//
//      THE EXACT BLOCKER FOR THE THREE OPEN PORTS, stated once so it is not
//      re-derived: MATERIAL.RESOLVE's request ISSUE POINT and its RESPONSE
//      JOIN -- entry I49, the texture lane's. The block is built and composed
//      and its record path is proven end to end; what is missing is a request
//      issued per meshlet and its answer joined back to the triangles that
//      asked. The 2026-09-20 host register window (ruling R51) has nothing to
//      do with any of it and closes none of it.
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
//      THE DEVIATION HALF OF THIS ENTRY IS SPENT, 2026-09-20 (terrain6). What
//      follows was written when neither the quantity nor a store for it
//      existed. BOTH EXIST NOW AND THE PRODUCER IS COMPOSED IN THIS FILE:
//
//        * the LAW was chosen by owner rulings R8/R22 (the MESH reading) and
//          is `zref::terrain::lod_deviation` with `zhao_terrain_loddev` as its
//          RTL, differentially tested in both readings;
//        * WHEN was chosen by ruling R24: at page load, and
//          `u_terrain_lodfeed` is composed below doing exactly that -- it
//          observes TERRAIN.MIPFEED's fine stream and emits the sixteen
//          records. So `sp_dev1/2/3` now has a LIVE producer in this module;
//        * the STORE exists: `zhao_terrain_devstore`, built and tested by
//          `terrain_lodpath_directed`, priced by ruling R59 at ~77 M10K.
//
//      SO WHAT IS LEFT OF THE DEVIATIONS IS A COST, NOT AN ABSENCE. Composing
//      the store today spends ~77 M10K of 553 on records whose only reader is
//      TERRAIN.LOD, which the rest of this entry still refuses -- the
//      "BUILT, INSTALLED NOWHERE" shape. `u_terrain_lodfeed`'s `tlf_w_ready`
//      is a named wire for that reason: the store joins as an AND and nothing
//      else moves. THE EYE IS ALSO NO LONGER MISSING -- ruling R63 landed
//      `fx16 eye[3]` on `SetView 0x0010` and `zhao_view_eye` is built and
//      tested. What has NOT moved is everything below this paragraph: the
//      job port's three material fields, the view-mask reconciliation, the
//      neighbour edge levels and the compose-cache retirement. Read on.
//
//      THE ORIGINAL STATEMENT IS KEPT BELOW because the argument it makes --
//      that `zhao_terrain_patch.st_*` and `zhao_terrain_lod.sp_*` are
//      different things wearing one ledger name -- is still true and is the
//      reason nobody should wire them together.
//
//      `sp_dev1/2/3` are the differences
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
// (I26 CLOSED 2026-09-19 (terrain3). THE TERRAIN PAGING SPINE's MEM.HPS.BRIDGE
//      and MEM.GUARD clients are INTERNAL now. The entry's own diagnosis was
//      right -- "Both providers EXIST and are already in this closure ... What
//      is missing is a SOCKET" -- and the socket arrived on 2026-09-19 as
//      `build_*` on `zhao_shell_top_v2`. What this packet added is what a
//      socket with WRITERS on it needs:
//
//        * `u_build_share` (`zhao_mem_share_wr`): MEM.UPLOAD and
//          TERRAIN.PAGELOADER write and `u_terrain_rdshare` reads, all as
//          ZHAO_CLIENT_TERRAIN_BUILD on slot 6. A readers-only share cannot
//          hold two writers: the write-data words of two bursts would
//          interleave in slot 6's one queue, and every requester would see
//          every other requester's retirement credits -- MEM.UPLOAD would
//          count a page load's retirement as its own and publish a mapping to
//          bytes still in flight. The share owns write-data ordering and an
//          in-order retirement ledger; `tests/memory/mem_share_wr_directed.cpp`
//          proves both and FIRES its two tripwires.
//        * HPS WRITES on the socket. The shell's port comment said the arbiter
//          "has no `b_wr_ready_i`, so the bridge's write READY cannot reach a
//          writer". The ready did not need routing THROUGH the arbiter: it is a
//          level that is high only while the one granted burst streams, and
//          both socket writers already gate on it. So the socket carries the
//          beats and hands the level back as `build_hps_wr_ready_o`.
//
//      WHAT IT COST TO FIND, recorded because the next composition onto real
//      fabric will meet the same class: with the spine on the real bridge,
//      TERRAIN.CMD asked for a 64-byte burst at a 32-byte-aligned address on
//      every resume after an abandoned burst, and `zhao_hps_bridge` refuses
//      that as malformed. The block's own bench checked only `len`. Fixed in
//      `zhao_terrain_cmd` (aligned base, skipped lead, counted in
//      `list_refetch_bytes_o`) and the bench now carries the bridge's law.
//
//      AND WHAT UNBLOCKED IT: `zhao_hps_arbiter_n` latched a client's request
//      only while IDLE, and CMD.DMA pulses its request for ONE cycle, so with
//      the spine keeping the bridge busy the ring read stopped after one burst
//      and the whole render path read as dead. That is the cmdmem packet's
//      pending-slot repair (ruling R55); this composition waited for it rather
//      than working around it.)
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
//          NARROWED 2026-09-20 (terrain6), because a block that holds a page
//          handle over TIME now exists and it is worth saying exactly why it
//          is still not this port's caller. `u_terrain_lodfeed`, composed
//          below, holds `slot_q` across a ~11,000-clock walk. IT DOES NOT NEED
//          THE CHECK, and the reason is structural rather than lucky: it walks
//          a BUFFERED COPY of the lattice, so its answer cannot be corrupted
//          by an eviction, and its consumer here -- MEASURE.HISTOGRAM -- keys
//          on `src_id`, not on the slot.
//
//          THE MOMENT THAT CHANGES is when `zhao_terrain_devstore` composes:
//          the store keys records BY SLOT, so a slot evicted and reused during
//          a walk would file page A's deviations under page B's handle, and
//          nothing downstream could see it. So this port's first honest caller
//          is lodfeed-with-the-store, not the subpatch issuer, and whoever
//          composes the store owes this check in the same commit. A staleness
//          port with no caller is not a gap for lack of a block; it is waiting
//          on the one consumer that makes the handle load-bearing.
//
//      NOT A GAP AND NAMED SO IT IS NOT RE-OPENED: `is_cslot_o` and
//      `is_cslot_valid_o` are T6's 256-entry composed-height cache index, and
//      `zhao_terrain_compcache_front` is a two-buffer FRONT rather than that
//      store. They are carried into this module and read by nobody, which is
//      declared at their wire rather than hidden behind an empty port
//      connection. The store behind that index is a later packet's block.
//
// (I28 CLOSED 2026-09-19, owner rulings R14 and R4. TERRAIN.SEQ's F-sheet
//      writeback job, its barrier completion and TERRAIN.RESIDENCY's writeback
//      acknowledgement -- the three ends this entry named -- are now internal:
//      TERRAIN.SEQ -> `u_terrain_jdoorbell` -> `u_terrain_writeback` ->
//      TERRAIN.RESIDENCY, with the writeback's sheet reads the third requester
//      of `u_terrain_rdshare` and its journal writes client 2 of
//      `u_terr_hps_arb`. Its two refusals were answered by rulings, not by this
//      file: SW.STREAM owns the journal address and ticket through the doorbell
//      contract (design/contracts/TERRAIN.WRITEBACK.DOORBELL.md), and the
//      arbiter is N-wide. What crosses the edge now is the HPS's own half of
//      that contract (`terr_jdb_*`, `terr_cfg_journal_*`): plan D10's
//      harness-is-the-HPS edge, the same kind as the FRAME_RING view. The block's header carried an
//      expired blocker (the guard read arm, landed 2026-09-06); it is corrected
//      in place. STILL TRUE and recorded at the instance: nothing in this core
//      can DIRTY a page until TERRAIN.BAKE composes (I27/I32), so the path is
//      exercised by `tests/terrain/world_composed_directed.cpp`, which composes
//      the same chain around a played HPS, rather than by the smoke bench.)
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
//      THE OWNER IS GEOM.MESHFETCH plus the clip-bank pages behind MEM.GUARD.
//
//      ITS STATED CAUSE EXPIRED BEFORE IT WAS WRITTEN, and it is the
//      SIXTEENTH false-absence claim in this tree. Corrected 2026-09-20 by
//      the forge packet. The sentence was: "it is the same obstacle as I23,
//      one asset kind over: the pages come back through MEM.VRAM.ARBITER and
//      `zhao_sdram_ctrl`, and THERE IS NO BEHAVIOURAL SDRAM MODEL IN THIS
//      TREE. A composition onto that socket would elaborate, lint and never
//      see a beat."
//
//      It was right to name I23 and wrong about what I23 says. The I23 record
//      some fourteen hundred lines ABOVE THIS ONE, in this same file, is the
//      correction of that exact sentence: `sim/models/zhao_sdram_model.sv` is
//      219 lines, cycle-true against `zhao_sdram_params_pkg`, carries a POKE
//      BACKDOOR written for this case, and is already instantiated against
//      `zhao_shell_top_v2` by `tests/shell/tb_zhao_shell.sv` in `realmem_mode`.
//      SEARCHED 2026-09-20 and the file is still there. So this entry inherited
//      a cause that the paragraph above it had already retired -- the same
//      refusal outliving its own cause, in one file, twice.
//
//      THE REAL BLOCKER IS A RULING, NOT AN ABSENCE, and it is the one
//      FORGE.SHADOW's ladder constants had until owner ruling R26.
//      `spec/cartridge.md` 4 puts the skeleton in KIND 8 (creature form:
//      "parts->meshlet ids, BONE HIERARCHY, attachments, hitboxes") and the
//      animation clips in KIND 9 (clip bank), and freezes both: "Byte-exact
//      layouts freeze with SW.TOOLS.ASSET at Phase-12 entry (creature_rules
//      9); until then the packer refuses to emit them (deterministic refusal,
//      never a guessed layout)."
//
//      R26 LIFTED THAT FREEZE ONLY PARTLY, and 4c says exactly how far in its
//      own words: "What is frozen here is the HEADER and the LADDER TABLE.
//      Nothing else. Parts, meshlet ids, THE BONE HIERARCHY, attachments and
//      hitboxes remain frozen until SW.TOOLS.ASSET at Phase-12 entry, exactly
//      as 4 says." Kind 9 was not touched at all. `geom_pose_bone_parent_i`,
//      `geom_pose_bone_t*_i` and `geom_pose_inv_rest_i` ARE the bone
//      hierarchy; `geom_pose_quat_*_i` and `geom_pose_root_d*_i` are a kind-9
//      clip frame. Building a reader for either is the guessed layout that
//      sentence exists to forbid.
//
//      SO THE MEMORY PATH IS NOT THE OBSTACLE AND NOBODY SHOULD BUILD ONE FOR
//      IT. What this gap needs is an OWNER DECISION of exactly R26's shape --
//      a second partial lift, for the bone hierarchy and a clip frame -- and
//      4c's own `body_off` mechanism was designed to make one cheap: the
//      header "names the byte offset at which that body will begin, so the
//      unfrozen half can be appended later without moving a byte of the frozen
//      half". The recommendation and its evidence are in
//      FINDINGS-forge.md. Until it is ruled, this stays a gap and the reason
//      is a freeze, not a missing model.
//
//      WHAT IS NOT PART OF THIS GAP, because the distinction is the whole
//      point of closing I10: the palette STORE is present and internal. A
//      decoded palette written by the block above is read by GEOM.SKIN through
//      `zhao_geom_pose_palette` with nothing external in between, and the
//      store's own `geom_pal_bone_unset_o` reports any vertex that arrived
//      before its pose did. The missing thing is the BYTES, not the path.
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
//      of this module. `zhao_terrain_bake_v2`'s `cmd_*` is ONE RECORD PER PATCH
//      BAKE: {patch_id, cx, cz, radius, depth_from, depth_to, env_x0/z0/x1/z1,
//      dual, cells, src_id}. Every field differs but `src_id`. The block wrote
//      the conflict down when it was built (`zhao_terrain_bake.sv` 29-48): "the
//      ledger says `inputs: [stamp_results]` ... This contract's own packet
//      table says something DIFFERENT ... Those are two different wires wearing
//      one name."
//
//      AND ITS STATED BLOCKER EXPIRED ON 2026-09-19, ONE DAY AFTER IT WAS
//      WRITTEN. Corrected 2026-09-20 by the terrain6 packet, because this is
//      the third refusal in this file found still quoting a cause that had
//      lapsed. The sentence was: closing the seam "needs TWO LAWS THAT DO NOT
//      EXIST ANYWHERE IN THIS TREE: a strength(u8) -> depth(fx16) mapping and
//      a 64x64 -> 33x33 resample." BOTH EXIST NOW, ratified by owner rulings
//      R15/R56/R65 and landed at commit `d1568a61`:
//        * `spec/terrain_rules.md` section 9.3(a) and
//          `zref::terrain::kStampDepthTable` -- sixteen EDITABLE fx16 metres
//          indexed by `strength >> 4`, one round-half-up on the delta,
//          endpoints exact, last segment held;
//        * section 9.3(b) and `zref::terrain::sheet_texel_for_vertex` -- the
//          resample, NEAREST-TEXEL with the seam error measured and declared,
//          because the format must stay frozen (three elaboration guards
//          reject the vertex-aligned 65x65; R65 revised R56's price from
//          +1.2% to +38.3%);
//        * `tests/terrain/stamp_to_bake_laws_directed.cpp`, 306 checks.
//
//      SO THE REAL BLOCKER IS SMALLER, HARDER AND IS AN OWNER DECISION.
//      SEARCHED: `fpga/rtl` for `stamp_depth|kStampDepthTable|
//      sheet_texel_for_vertex|depth_table` (case-insensitive, all of
//      `fpga/rtl` including `synth/`) -- ZERO HITS. The laws exist in the
//      REFERENCE and in the SPEC and in NO RTL. And when they are built, they
//      do not fit this block's port:
//
//        R15 describes a PER-VERTEX depth read out of layer F -- pick the
//        vertex's texel, look its strength up in the art table, get a depth.
//        `zhao_terrain_bake_v2`'s dig is a PARAMETRIC DISC: `cmd_radius_i`
//        (fx16, "<= 0 writes nothing"), `cmd_depth_from_i`/`cmd_depth_to_i`,
//        with a radial falloff whose oracle is `zref::terrain::bake_dig`.
//        THOSE ARE TWO DIFFERENT DIG LAWS producing two different scars, and
//        no adapter can turn a texel stream into {cx, cz, radius, depth_from,
//        depth_to} without fitting a disc to a field -- an invention exactly
//        of the kind the original refusal was right to refuse.
//
//      The owner decision is therefore WHICH LAW DIGS: bake keeps its disc and
//      layer F drives something else, or bake gains a second, per-vertex depth
//      input and the disc becomes one way of filling it. Written up with a
//      recommendation in FINDINGS-terrain6.md. R15 ratified the CONVERSION and
//      left the CONSUMER's shape untouched, which is why reading R15 as "the
//      blocker is gone" would put a fabrication under every permanent wound in
//      the game after all -- just one ruling later.
//
//      A THIRD ABSENCE, AND IT IS ARBITRATION: `zhao_surface_sheet` IS COMPOSED
//      in this module (search for `u_surface_sheet`) and holds layer F, so the
//      sheet is NOT missing -- that would have been a false-absence claim. But
//      its request port is annotated at the instance "REAL: SURFACE.STAMP is
//      the only requester", one `req_*` channel with no arbitration. Whatever
//      reads layer F for the dig is its SECOND requester, and choosing between
//      them is a scheduler, which a composer may not write.
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
//      ---------------------------------------------------------------------
//      WORKED 2026-09-20 (gz/field) AND STOPPED ON AN OWNER DECISION. Three
//      things were established; the entry stays OPEN and is not half-built.
//      Its two prerequisites, I42 and I5, are CLOSED above, so what remains is
//      this seam alone.
//
//      (1) R44's MEANS DOES NOT FIT THIS SEAM, with evidence. The ruling says
//      to "promote `zhao_probe_walk_earth`/`zhao_probe_patch_acc` out of
//      `fpga/rtl/synth/` ... rather than rebuilding them". READ BOTH FILES
//      BEFORE INHERITING THAT: they are the FIELD-MAJOR, FOUR-WIDE, WHOLE-PATCH
//      topology of reports/Fieldv3.md Phase 4, and this console composes the
//      VERTEX-MAJOR one.
//        * `zhao_probe_walk_earth` GENERATES lattice points from two prepared
//          33-entry tables, precisely to "delete that transport". But
//          `zhao_terrain_patch` ALREADY OFFERS the point -- `vtx_valid_i` with
//          `wx_i`/`wz_i`/`vi_i`/`vj_i` -- so on this seam the walker would be a
//          second producer of a coordinate the consumer just handed over.
//        * `zhao_probe_patch_acc` is not a feeder of this port, it is a
//          REPLACEMENT FOR THE BLOCK BEHIND IT: four M10K banks by vertex mod
//          4, sixteen RAMs, with its own height/velocity/material/nav_cost
//          reducers and INIT/ACCUM/DRAIN phases. It has no `fld_height` lane
//          because it IS the reducer `zhao_terrain_patch` already is, composed,
//          fit-targeted and in the manifest.
//        * The console's own FIELD parameterisation argues the other way too:
//          `FAB_GROUP_PTS=1`, because this front holds one point in flight. The
//          probes' four-wide group is the configuration that setting rejects.
//      So promoting them is not a wiring act: it is swapping TERRAIN.PATCH's
//      composed architecture for another one, which is an owner call and not a
//      packet's. R44's GOAL -- do not rebuild what exists -- still stands, and
//      on this seam the thing that already exists is TERRAIN.PATCH.
//
//      (2) THE FRAME TICK IS AVAILABLE, so that half of R44 is fine.
//      `gpu_tick_o` is the shell's frame boundary (already `core_tick_c` in
//      this file) and the shell publishes a frame id beside it, which
//      PART.SPAWN's `tick_i` already consumes. `age = tick - start_tick` and
//      the phase are computable from it.
//
//      (3) THE BLOCKER IS THE HANDLE -> PROGRAM-HASH MAPPING, AND IT HAS NO
//      PRODUCER. R43 gives CMD.EXEC "only the handle -> program-hash lookup
//      that TerrainField needs", and TerrainField 0x0200 carries
//      `handle32[program] program`. But FIELD.PROGCACHE's contract fixes the
//      directory key as a CONTENT hash -- "the program hash
//      `CRC32C(code||tables) + instr_count`", computed by
//      `zfield::programHashOfBytes` -- so the handle is NOT the key and
//      CMD.EXEC cannot derive one from the other without reading and hashing
//      the cartridge page.
//      SEARCHED, and named so the next reader does not repeat it:
//      `program_hash`, `prog_hash` and `programHash` across all of `fpga/rtl`
//      (including `synth/`) -- ZERO hits; `programHashOfBytes` exists only in
//      `reference/src/zfield/zfield_decode.cpp`. Nothing in hardware publishes
//      {handle -> hash}.
//      RECOMMENDED, for the owner to confirm: SW.STREAM owns the mapping,
//      because it is the only party that has both -- it names the program by
//      handle in the plan and computes the hash with `zfield::programHashOfBytes`
//      to post the commit. A fourth doorbell post kind, BIND {handle32, hash32},
//      writing a small handle->hash table that CMD.EXEC's TerrainField arm
//      reads, closes it with no ABI change and no second hashing law. The
//      alternative -- re-key the directory by handle -- is cheaper still but
//      contradicts FIELD.PROGCACHE's contract in writing, which is why it is
//      not taken here.
//
//      WHAT IS THEN LEFT TO BUILD, so the next packet can be scoped rather than
//      re-derived: (a) CMD.EXEC's TerrainField arm, staging ~480 bits per
//      record (program, four footprint fx16, start_tick, duration_ticks and
//      p0..p7) and emitting {footprint, hash, cmd index} onto
//      `terr_pt_fld_add_*` at commit -- area-significant, so it wants a
//      measurement; (b) a descriptor table keyed by that cmd index holding the
//      uniforms; (c) the EARTH stream adapter, which on each accepted vertex
//      issues one E record per active lane and returns `height` on
//      `terr_pt_fld_*`, reading the patch's own `fld_covers_o` to skip a run
//      for a lane that does not cover rather than re-deciding the section 9.1
//      test it owns; (d) a two-client share on the host's single `pc_lu_*`
//      port, since the doorbell holds it for the HPS.
//
//
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
//
// (I44 CLOSED 2026-09-20 (terrain5), under owner ruling R64. TERRAIN.MIPGEN's
//      COARSE-HEIGHT PLANES are RETIRED as a DUPLICATE PROVIDER, which is a
//      Phase-2 allowed act. The entry is kept in full below rather than
//      deleted, because a retirement is a claim and the next person is owed
//      the argument and the way back.
//
//      THE ARGUMENT, in one line: ruling T8's decimation is NESTED and
//      UNROUNDED, so a coarse vertex IS a fine vertex, bit for bit. A plane
//      store can therefore never yield a number the fine lattice does not
//      already contain. It buys BANDWIDTH and never a different answer -- and
//      the deviation pass this campaign built reads the fine lattice at page
//      load anyway, off the mip pass's own stream, so even the bandwidth is
//      not bought.
//
//      IT IS A TEST NOW, NOT PROSE, which is what R64 asked for and is the
//      part that had been missing. `tests/terrain/terrain_mipgen_directed.cpp`
//      case 3b compares the RTL's mip17/mip9 against the FINE LATTICE at
//      stride, with the strides written out and NO ORACLE IN BETWEEN:
//
//          mip17[i,j] == fine33[2i,2j]      mip9[i,j] == fine33[4i,4j]
//
//      That is a different claim from the case beside it. Case 2 compares the
//      RTL against `zref::terrain::mipgen`, which is the right check for "is
//      the mip law implemented correctly" and is USELESS for this question:
//      both sides could round or average the same way and agree perfectly.
//      Case 3b also pins the ORACLE to the same identity, so the redundancy
//      argument cannot hold for the hardware while zref quietly models
//      something else. Vacuity control, run by hand on the fixture: at a WRONG
//      stride 272 of the 289 coarse cells mismatch, so the check is not
//      passing because every sample happens to be equal.
//
//      WHAT REVERTS IT, in one ledger line: a named consumer. If the
//      decimation is ever changed to an AVERAGE, case 3b fails -- and it
//      should, because an averaged mip would be a real second source of
//      information, and the seams would crack as shared vertices stopped
//      matching. That test failing means this retirement has expired.
//
//      WHAT WAS ACTUALLY REMOVED, and what was not:
//        REMOVED  the eight data ports `terr_mg_m17_{valid,addr,surf,h}_o` and
//                 `terr_mg_m9_{...}_o` from this module, the generated board,
//                 the slot-overflow mutant wrapper's port block and the smoke
//                 bench. The block's own outputs are left named-and-empty at
//                 the instantiation, so the retirement is visible where it is
//                 used.
//        KEPT     `terr_mg_m17_writes_o` / `terr_mg_m9_writes_o`. They are the
//                 only remaining evidence at this boundary that the decimation
//                 ran, and dropping them too would let the whole coarse path
//                 be pruned with nothing able to notice -- the exact shape
//                 entry I32 records for layer D.
//        KEPT     TERRAIN.MIPGEN, composed. Its `done_o` is
//                 TERRAIN.RESIDENCY's SECOND COMPLETION; without it
//                 `resident_o` is structurally zero and no patch ever reaches
//                 the compose door. The block is not the duplicate; the
//                 PLANES were.
//        NOT DONE, AND OWED TO WHOEVER OWNS MEMORY NEXT: the two pools
//                 themselves. `TERRAIN.RESIDENT_MIP_POOL` (1,024 x 1,536 B)
//                 and `TERRAIN.COMPOSED_MIP_POOL` (256 x 1,536 B) are marked
//                 RETIRED in `spec/memory_rules.md` 5b, but their region
//                 constants still exist in `fpga/rtl/common/zhao_pkg.sv` and
//                 `fpga/rtl/memory/zhao_mem_guard.sv`, with
//                 `tests/formal/formal_mem_guard.sv` and
//                 `tests/mutants/zhao_mem_guard_resbound_mutant.sv` behind
//                 them. Removing a guard region means re-proving
//                 `mem_guard_no_escape`, and MEM is another packet's lane
//                 under rulings R4/R32/R55 -- editing it from here while that
//                 packet is live is the shared-file hazard CLAUDE.md now has
//                 three sections about. 1.875 MB of the memory map is
//                 recoverable and is NOT recovered by this commit. Said
//                 plainly rather than left to look done.
//
// THE ENTRY AS IT STOOD, kept because the search behind it is the evidence:
//
// I44-was. TERRAIN.MIPGEN's COARSE-HEIGHT PLANES (`terr_mg_m17_*`,
//      `terr_mg_m9_*`) -- was a boundary. SEARCHED for any consumer of a 17x17
//      or 9x9 coarse height plane: `fpga/rtl/**` including `fpga/rtl/synth/`
//      for `terr_mg_m17_|terr_mg_m9_` (four files: this one, the board, the
//      smoke bench and a mutant copy -- all carriers, no consumer);
//      `reference/` for `mip17_at|mip9_at` (declared in zref_terrain.hpp
//      284-294, called only by `tests/terrain/terrain_mipgen_directed.cpp`);
//      `design/contracts/` and `design/blocks.yml` (`downstream: [TERRAIN.LOD]`
//      and nothing else); `spec/` (terrain_rules 109 "for TERRAIN.LOD",
//      memory_rules 5b for the addresses); `tools/`; and the cross-cutting
//      sweep `mip.*(collision|cull|shadow|physic|occlus|nav|query|stream)` over
//      the whole tree, whose every hit is TEXTURE mip policy. PART.COLLIDE's
//      terrain sample goes through TERRAIN.HEIGHTTAP on the FINE lattice.
//      That search was terrain4's; terrain5 repeated the `terr_mg_m17_|
//      terr_mg_m9_` sweep and the `mip17|mip9` sweep over sources after two
//      more packets had landed, and found no new hit. There is no consumer.
//
//      The entry also recorded a FALSE ABSENCE worth keeping: it used to say
//      the block that differences mips against the fine lattice "has no name
//      in the ledger and no file". `fpga/rtl/terrain/zhao_terrain_loddev.sv`
//      had landed at 786f52ba the day before the sentence was written. False
//      in the direction that MANUFACTURES WORK -- it sends the next reader to
//      build an SDRAM mip-pool residency engine that nothing needs.)
//
//
// (I42 CLOSED 2026-09-20 (gz/field), under owner rulings R43, R20 and R55.
//      All three of its parts have producers, and the entry is kept in full
//      below because it argued each of them and is owed the answer beside the
//      question.
//
//      THE LOADER and BOTH DIRECTORY PHASES are `zhao_field_doorbell`,
//      composed below. R43: "A doorbell contract on the R14 pattern:
//      SW.STREAM stages the plan and writes the EXISTING loader words". That
//      is the same pattern I28 closed the F-sheet journal with, and what
//      crosses the console's edge now is the HPS itself -- posts and ticketed
//      returns -- not a raw leaf port. The three tie-offs the entry priced are
//      therefore not taken: the store is filled, `hdr_loaded` is a live bit,
//      and the invalidation edge stays internal.
//
//      WHY THE LOOKUP PHASE IS THE HPS's TOO, which the entry left open. The
//      caller of a lookup is whoever can act on a miss, and only software can:
//      `zfield::decode` is software by FIELD.PROGCACHE's own contract, which
//      this entry quotes. Hardware asking and software repairing would put the
//      two halves of one decision on opposite sides of the edge.
//
//      THE ORDER LAW IS NEW AND IT IS THE REASON THE BLOCK IS NOT A FIFO. A
//      COMMIT for a slot whose HEADER was not written since the last commit is
//      REFUSED without the directory ever seeing the hash, ANSWERED with a
//      return record, and COUNTED on `fld_db_commits_refused_o` -- ruling R20's
//      shape exactly, and it removes the failure class FIELD.PROGCACHE's
//      contract names (a directory promising a hash to microcode that is not
//      there). It is reachable with legal stimulus and is fired by
//      `tests/field/field_doorbell_directed.cpp`.
//
//      A POST THAT CANNOT BE TAKEN IS HELD, NEVER DROPPED (ruling R55's
//      shape), and `fld_db_post_stalls_o` counts the cycles.
//
//      THE SECOND CLIENT is `zhao_field_flow_adapter`, entry I5's closure.
//      The entry's stated reason for the port -- "it is what makes the
//      arbiter's contention reachable with legal stimulus" -- is better served
//      by it: two REAL profiles offering in the same cycle is the traffic
//      `fld_contended_grants_o` exists to measure, where the edge client was a
//      stand-in for one.
//
//      WHAT IS STILL OPEN AND IS SOMEBODY ELSE'S ENTRY: `fld_ld_oob_o` remains
//      structurally unreachable, exactly as the entry says. It is not made
//      reachable by this change and it is not claimed to be.
//
//      The original entry, unedited:
//
//      THE FIELD ENGINE'S PROGRAM LOADER (`fld_ld_*`), ITS DIRECTORY PHASES
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
//      other eight are driven by `tests/field/field_host_directed.cpp`.)
//
// I49 IS CLOSED AND DELETED, 2026-09-20 (texmat2). MATERIAL.RESOLVE's
//      REQUEST and RESPONSE. Five ports left this module's list --
//      mat_req_valid_i, mat_req_material_set_i, mat_req_material_id_i,
//      mat_req_quality_tier_i and mat_rsp_ready_i -- and the entry is
//      kept here only as the one sentence that says what closed it and how,
//      because it was open for one day and its argument is reused by I20.
//
//      WHAT THE ENTRY ASKED FOR: 'a request must be issued per meshlet (or
//      per triangle) and its answer joined back to the triangles that asked'.
//      zhao_material_window, composed with GEOM.REPLAY below, is that. It
//      reads the triangle's OWN {material_set, material_id, semantic weight},
//      all three of which ride with the meshlet from GEOM.DRAWJOB under
//      ruling R29 and are latched by the same enable at every stage, so the
//      request's halves are joined BY CONSTRUCTION. Entry I39's refusal --
//      'TWO LIVE WIRES ARE NOT A PRODUCER' -- is answered rather than
//      side-stepped: these are not two wires, they are fields of one record.
//
//      AND THE JOIN, which is the half that is easy to fake. The published
//      answer is read combinationally at the shell's triangle door, several
//      stages downstream -- the exact shape of the metadata-swap defect this
//      file has a chapter about. What makes it sound is an INTERLOCK: the
//      window never changes what it publishes while any triangle is between
//      GEOM.CLIP's input and the door, and the three disposal events of that
//      span (GEOM.CLIP accepts, GEOM.CLIP retires a non-ACCEPT verdict, the
//      door takes) are exhaustive, so the drain always completes. The
//      argument does not rest on any block's latency.
//      mat_win_err_unpublished_o watches the one thing that would falsify
//      it, and both of the window's guards were FIRED in
//      	ests/texture/material_window_directed.cpp (45 checks).
//
//      WHAT IS STILL OWED AND IS NOW I20's, NOT THIS ENTRY'S: the binding
//      page's palette_slot and palette_generation are ZERO BY LAW for a
//      direct format and UNPRODUCED for a CLUT one -- counted, loudly, on
//      mat_win_clut_unowned_o -- and the tmu_mode -> response_class
//      encoding is an OWNER DECISION written up in FINDINGS-texmat2.md and
//      held in ONE editable parameter.
//
// I50. GEOM.LOOM's NODE STREAM and CAMERA BASIS (`geom_loom_*`) -- BOUNDARY.
//      NEW 2026-09-20 (geom3 packet), and like I41 before it this is a gap
//      OPENED DELIBERATELY: the register goes up by one here and the trade is
//      on purpose, because it buys four.
//
//      WHY GEOM.LOOM IS COMPOSED AT ALL. Owner ruling R29 ratified the draw
//      job's `xform[12]` as "the instance transform palette row named by the
//      transform handle", and the palette's writer has to be a real producer of
//      instance transforms. There is exactly one in this tree and it is this
//      block: `design/contracts/GEOM.LOOM.md`'s purpose line is "producing
//      instance transforms" and its output is "{node_index, transform[12]} --
//      a 3x4 affine, row-major, fx16 S15.16". Composing anything else, or
//      taking the palette's rows from a port, would have been a second opinion
//      about where a world transform comes from.
//
//      WHY ITS INPUT IS A BOUNDARY AND NOT A MISSING BLOCK, which is the part
//      worth reading before anyone goes looking for the producer. The owner
//      ruling of 2026-08-31 6.4 put it outside this console IN TERMS: "The
//      ARM/compiler supplies a parent-before-child topologically sorted
//      stream. Loom only composes transforms ... Keep-world reparenting is
//      computed on the ARM between frames." The contract calls that deletion
//      "what makes this block buildable". So the stream is host state, like the
//      frame ring itself, and what is missing is not a block but a CARRIER --
//      the same shape as I42's field program loader, and its recommendation is
//      the same: a doorbell on the R14/R43 pattern (SW.STREAM stages the sorted
//      stream, a CSR mailbox hands over base/count, hardware acknowledges).
//      That is an ABI addition and an owner call, and it is written up in the
//      geom3 findings rather than decided here.
//
//      `cam_basis_i` RIDES THE SAME ENTRY because it has the same owner and the
//      same absence. It is the frame's camera 3x3 for BILLBOARD nodes, and it
//      cannot be derived from anything this console holds: `SetView` carries a
//      combined view-PROJECTION matrix, and recovering a rotation basis from it
//      needs the inversion the ruling excludes by name.
//
//      WHAT IS NOT PART OF THIS GAP: the palette itself, its writer and its
//      reader are all real and composed, and `geom_dj_pal_writes_o` counts the
//      rows GEOM.LOOM lands in it. A draw naming a row nobody wrote is REFUSED
//      and counted (`geom_dj_refused_xform_o`), never drawn at the identity --
//      an unset matrix is not a pose, it is an unset matrix.
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
//   RE-VERIFIED 2026-09-20 (forge packet) AND THE CAUSE SURVIVES, named
//   search by named search so the next reader need not repeat it:
//     * `solid\w*_o` as an output port across every subdirectory of
//       `fpga/rtl` including `synth/` and the probes: ZERO HITS. The only
//       34-dimensioned solid structures in the tree are the two CONSUMERS'
//       own storage (`zhao_forge_cliff.sv:291`, `zhao_forge_cliff_ram.sv:18`).
//       Nearest misses, each rejected for a reason: `zhao_terrain_tess.sv:377`
//       holds a 64-bit solid mask that is an INTERNAL 8x8 subpatch variable,
//       not a port; `zhao_terrain_heighttap.sv:245`'s `taps_void_o` is a
//       per-tap census, not a window.
//     * `vdist` across `fpga/rtl`: FOUR files, and all four are the wrong
//       end -- `zhao_forge_cliff.sv` and `zhao_forge_cliff_ram.sv` (the two
//       consumers), this file (the refusal quoting itself) and
//       `zhao_prod_top.sv` (the LFSR pricing harness). It exists as a real
//       quantity ONLY in the C++ oracle (`zref_terrain.hpp`'s `rim_plan`
//       taking `const int32_t* vdist`) and in the contract. No RTL computes
//       one.
//     * a lattice-walking PAGE ISSUER: none. `fpga/rtl/synth/` was read file
//       by file and `zhao_probe_walk_earth.sv` IS a real lattice walker --
//       and the wrong one: it emits four-wide world (x,z) groups for the
//       FIELD v3 executor over a 33x33 patch, with no page ci/cj, no cell
//       extents, no solid bits and no vdist. A genuine near-miss, recorded so
//       it is not re-found and mistaken for the answer.
//     * AND `design/blocks.yml` DECLARES AN UPSTREAM THAT DOES NOT FIT THE
//       PORTS: `inputs: [terrain_mesh]`, `upstream: [TERRAIN.TESS]`. TESS's
//       output is a vertex/triangle stream and this block's inputs are a page
//       command, a 34x34 solid window and a vdist master. The ledger edge is
//       an INTENTION, exactly as GEOM.MESHFETCH's `upstream: MEASURE.GOVERNOR`
//       turned out to be -- and a ledger edge read as a port is how somebody
//       concludes this entry is out of date.
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
//   THAT PARAGRAPH IS NOW SPENT, AND SO IS THE ONE AFTER IT. Owner ruling R26
//   (2026-09-19) lifted the kind-8 freeze FOR THOSE FOUR CONSTANTS ONLY and
//   `spec/cartridge.md` 4c froze their layout; the R68 packet built the whole
//   input chain on 2026-09-20 -- `zhao_geom_ladderbank` (the page reader),
//   `zhao_geom_lodstate` (the per-instance LodState nobody held),
//   `zhao_geom_projradius` and `zhao_view_projscale`. Five blocks, all
//   unit-verified against `zref::creature`, all `pending_compose`. THE CASTER
//   IS REAL. What is still owed on the INPUT side is one thing and it is
//   named: `GEOM_PAY_A_W` is 16 and FULL -- ARENA_W 3 + INDEX_W 12 = 15 with
//   the geometry/particle owner tag at bit 15 (see the elaboration guard
//   below) -- so the instance centre's `1/w` needs the payload widened to 17
//   AND a front mux on `zhao_part_project`'s geometry arm, owner ruling R3
//   keeping client A a time-multiplex. That is R68's sub-build 4, unlanded.
//
//   AND THE BLOCKER HAS MOVED TWICE MORE, WHICH IS WHY THIS ENTRY IS LONG
//   RATHER THAN CLOSED. Traced 2026-09-20 by the forge packet, under owner
//   ruling R75 ("take the ARENA ROUTE ... rather than building a second
//   geometry path"). The arena route is the right architecture and it is NOT
//   REACHABLE TODAY, for two reasons that are structural rather than
//   arguable, and NEITHER WAS KNOWN WHEN R75 WAS WRITTEN:
//
//     1. COMPOSING THE HULL AS A GEOMETRY BATCH WEDGES THE WHOLE FRONT END.
//        `zhao_geom_vattr`'s `done_o` (its :490) is a six-term AND including
//        `lit_ord_q == uv_ord_q` -- every decoded vertex must receive a lit
//        r/g/b from GEOM.LIGHT and every landing a u/v from GEOM.VDECODE. A
//        shadow hull has NEITHER: it is not skinned, carries no texture
//        coordinate and is not lit. `va_done` gates BOTH sides of the
//        GROUP_SEQ -> REPLAY handshake (this file's :7354 and :13722), so a
//        batch that never earns its rows never hands over its arena, never
//        gets it released, exhausts `GEOM_ARENAS` and backpressures
//        GEOM.PROJ_LANE until no triangle reaches GEOM.CLIP at all. There is
//        NO timeout, NO abort and NO counter that fires for it -- the tell
//        would be `colours_written_o` frozen while `uv_staged_o` and
//        `landings_o` climb, and nothing differences them. This is CLAUDE.md's
//        ALIVE-AT-ZERO-CPU shape in silicon, and composing it would not leave
//        a gap open, it would ship a DEADLOCK behind a closed gap.
//
//     2. THE CONSOLE HAS NO VERTEX ALPHA, SO THE SHADOW WOULD DRAW OPAQUE.
//        FORGE.SHADOW's whole output is "ordinary TRANSPARENT geometry
//        through the main renderer" and its `vtx_alpha_o` is a per-vertex
//        unit8. SEARCHED, and the blend ALU is NOT the missing piece -- it is
//        real and composed (`zhao_raster_blend`, six instances in
//        `zhao_raster_fragment.sv:490-510`, live through
//        `zhao_raster_tile_pipe_v2`). What is missing is every path that
//        would reach it:
//          * `zhao_geom_vattr.sv:474` injects the packet's alpha slot as the
//            named constant `ALPHA_C` = fx16 1.0 (OPAQUE) and its own header
//            :69-74 says why: "ALPHA HAS NO PRODUCER ... R11's 'rgb/alpha
//            from zhao_light_stream' names a quantity that does not exist";
//          * `zhao_geom_attrpack` emits exactly THREE planes and indexes only
//            SLOT_INVW / SLOT_U_OVER_W / SLOT_V_OVER_W (its :225-227, :267-275),
//            and the rasteriser has exactly three attribute lanes
//            (`zhao_raster_tile_pipe_v2.sv:601`), so slots 3..6 -- lit r/g/b
//            AND alpha -- have no interpolator and no carriage even if a
//            producer existed;
//          * the flat per-triangle alpha and the blend-mode selector are
//            `tri_continuation_tail_i` and `tri_fragment_state_i`, which this
//            module's own port table already marks "OPEN, still a BOUNDARY";
//          * and the one alpha the console does supply is
//            `MAT_BASE_ALPHA_C = 8'hFF` by owner ruling R48, whose stated
//            reason is "no ratified vertex format carries alpha".
//        R48 AND THE FORGE.SHADOW CONTRACT CONTRADICT EACH OTHER IN WRITING.
//        That is an OWNER DECISION, written up with evidence and a
//        recommendation in FINDINGS-forge.md, and it is the thing to settle
//        before any more of this chain is built: a shadow composed today is a
//        flat opaque dark polygon with a depth bias under every creature,
//        which is exactly the art defect CLAUDE.md's ground-contact law
//        exists to refuse -- shipped, and counted as a gap closed.
//
//   TWO SMALLER CORRECTIONS FROM THE SAME RE-SEARCH, both about what the
//   forge files ARE, because each one would send a reader to the wrong block:
//     * `zhao_forge_cliff_ram.sv` IS NOT A CHILD OF `zhao_forge_cliff`. Its
//       own first lines call it "the bitmap-RAM CANDIDATE beside the golden
//       `zhao_forge_cliff.sv`" -- a RIVAL implementation of the same contract
//       (the 34x34 window in one RAM instead of 1,156 flops), verilated side
//       by side with it by `forge_cliff_ram_differential`. `zhao_forge_cliff`
//       instantiates NOTHING. So "compose FORGE.CLIFF" is also a
//       latest-version question with two candidates and no ruling.
//     * `zhao_forge_jitter_rom.sv` belongs to FORGE.PRIM_EVAL, not to the
//       cliff: it is instantiated at `zhao_forge_prim_eval.sv:314`.
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
//     - RE-VERIFIED 2026-09-20 (forge packet) AND THE CAUSE SURVIVES, with
//       TWO facts added that make it stronger and narrower. Searched: every
//       page kind in `spec/cartridge.md` 4 (0 field programs, 1 sourceids, 2
//       generated-code manifest, 3 sky set, 4 terrain patch, 5 tone bank, 6
//       island patch, 7 island table, 8 creature form, 9 clip bank, plus 4b
//       SPECIES_TABLE and 4c the ladder table) -- NONE is a forge program
//       page; and `j_family|j_segments|j_sides` across all of `fpga/rtl`
//       including `synth/` and every `probe`-named file, whose only hits are
//       the two forge blocks' own inputs and the LFSR noise source in the
//       generated fit harness `zhao_prod_top.sv`.
//         * THE ABI CANNOT NAME ONE OF THE SIX. `spec/commands.zidl:133-136`
//           declares `enum forge_kind : u8` with EXACTLY ONE member,
//           `FORGE_HEIGHTFIELD_PATCH = 0`, and :131 calls it "the one
//           implemented kind". `zhao_forge_prim`'s six families -- ribbon,
//           fan, tube, shell, billboard, cliff -- have no encoding in any
//           ratified command, so the gap is not only a missing page: it is a
//           missing ENUM MEMBER SET as well, and both are owner decisions.
//         * FORGE.PRIM AND FORGE.PRIM_EVAL DO NOT MEET BY A SINGLE WIRE, and
//           the paragraph above is right that neither drives the other -- but
//           the reason is worth stating, because "wire them together" is the
//           obvious wrong move. They are the two HALVES of a meshlet, not two
//           stages of a chain: PRIM emits index triples, PRIM_EVAL emits fx16
//           positions, and they are joined only by an ORDERING CONVENTION
//           written in `zhao_forge_prim_eval.sv:24-32` ("the topology walker's
//           ribbon references vertices in ring-major order ... this block
//           emits positions in EXACTLY that order"). Their meeting place is
//           GEOM.SETUP, which `design/blocks.yml` declares and no RTL wires.
//           And PRIM_EVAL is NOT a general evaluator for PRIM's six: its own
//           header calls it "the lightning position evaluator"
//           (reports/ADDLIGHTNING.md), and it serves the RIBBON family only.
//           So even with a page and an enum, four of the six families would
//           still have no evaluator.
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
//   Every geometry memory client is hard-coded READ-ONLY in source. THE
//   CITATIONS WERE RE-CHECKED 2026-09-20 (forge packet) AND ALL THREE HAD
//   DRIFTED, one of them onto a mechanism that is not the one described --
//   which matters because a refusal is only as durable as the line it stands
//   on, and a reader who spot-checks one stale citation throws out the whole
//   entry. Now, verified line by line:
//     * `zhao_geom_meshfetch.sv:340`  `assign guard_req_o.write = 1'b0;`
//       (was cited as :319, which is now a multiplier declaration)
//     * `zhao_geom_assetfetch.sv:360` `assign guard_req_o.write = 1'b0;` with
//       the comment "READ ONLY. The pool admits" (was cited as :341)
//     * `zhao_geom_mem_adapter.sv` HAS NO `write = 1'b0` ANYWHERE. It is
//       read-only by a STRONGER mechanism than the one this entry claimed: a
//       parameter on its `zhao_mem_share2`, `.FORCE_READ(1'b1)` at :175, with
//       the quoted comment "the asset window is READ-ONLY by construction"
//       at :16 and :175 rather than at the cited :190 (a port connection).
//   TWO MORE GEOMETRY CLIENTS THE OLD LIST MISSED, both agreeing with it:
//   `zhao_geom_drawjob.sv:308` and `zhao_geom_ladderbank.sv:222`, both
//   `write = 1'b0`. So the claim got STRONGER when it was checked, which is
//   the direction that almost never happens and is worth recording -- and the
//   only five blocks in the whole tree that assert `write = 1'b1` are
//   RASTER.FBWRITE (`zhao_raster_fbwrite.sv:218`), DEBUG.FRAMEBLIT
//   (`zhao_debug_frameblit.sv:391`), MEM.UPLOAD (`zhao_mem_upload.sv:394`) and
//   two terrain paths -- TERRAIN.PAGELOADER (`zhao_terrain_pageloader.sv:381`)
//   and TERRAIN.WRITEBACK (`zhao_terrain_writeback.sv:583`), both named
//   2026-09-20 so the phrase "two terrain paths" stops being a thing the next
//   reader has to re-find. The count is exact and was re-counted. None is
//   geometry, and
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
//   GEOM.DEPTHQUANT -- THE REFUSAL BELOW IS SPENT (2026-09-19, geom packet).
//   It is COMPOSED inside GEOM.REPLAY, and each of the three objections was
//   answered by a structure rather than argued away: the per-vertex port is not
//   tapped (depth is taken per CORNER, after the arena, where there IS
//   backpressure); the arity is the replay's own -- three lanes, one per corner;
//   and the reciprocal is a PRIVATE `zhao_raster_rcp24_v4` (the latest version,
//   a second instance of the one law) shared by the lanes through the service's
//   own token, with each lane's reply latched at its own handshake because the
//   block reads it one clock later. The text is kept as the record of why the
//   composition had to be a block and not wiring.
//
//   WHAT IT SAID: GEOM.DEPTHQUANT is refused for a HANDSHAKE and not for a missing
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
// LIGHTING SEAM -- GEOM.LIGHT COMPOSED (owner ruling R2), TWO SEAMS STILL OPEN
// ---------------------------------------------------------------------------
// REWRITTEN 2026-09-19 (geom packet). This section used to say the seam was
// "STILL NOT CONNECTED, FOR THREE NEW REASONS". Each reason's fate:
//
//   * GEOM.SKIN.NORM's three operands -- closed earlier the same day (the
//     palette store carries the normal); its OUTPUT was entry I43.
//   * TWO OWNERS -- RULED. R2 (owner, explicit): `zhao_light_stream` owns
//     vertex light and `zhao_geom_light` is superseded. The register resolves
//     GEOM.LIGHT to `zhao_light_stream` (with witnesses at its alias) and
//     `console_inventory.yml` and `prod_manifest.yml` record the supersession,
//     so the rename-shaped supersession no tool detects is now written down in
//     all three places a tool reads.
//   * THE DESCRIPTOR BANK -- CLOSED 2026-09-19 (geom2, owner ruling R25):
//     SetEnvironment is implemented and lowered by CMD.EXEC through
//     GEOM.LIGHT.ENV (`u_light_env`). Entry I48 is in the CLOSED ledger.
//
// WHAT IS COMPOSED. GEOM.SKIN.NORM -> `zhao_light_skin_adapter` (the creature
// seam, narrowed by assertion) -> `zhao_light_stream` on the creature path,
// with the service's two arithmetic leaves inside it. Entry I43 is closed.
// The smoke bench's command packet carries a SetEnvironment, and every lit
// vertex is checked per channel against `zref::light_env::bank_of` of THAT
// record and `zref::creature::lambert_from_world_normal`, through the fixture
// generator.
//
// WHAT IS STILL OPEN, and `light_seam_connected_o` stays LOW until it is:
//   * the lit RGB's route to the RASTER. GEOM.VATTR stores it (I46, closed)
//     and GEOM.REPLAY reads it with every corner, but GEOM.ATTRPACK's three
//     planes are invw, u_over_w and v_over_w, and none of them is colour.
//     Nothing here should be read as "the raster is lit".
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

  // ---- INPUT.SNAC (owner ruling R7) ----------------------------------------
  // How many SNAC connectors the board carries. They drive canonical pad slots
  // 0..SNAC_PORTS-1; slots above that are always the incoming route's. Two is
  // the MiSTer SNAC shape. The bus rate, the /ACK timeout and the inter-poll
  // gap are the ADAPTER's knobs and stay there (spec/input_rules.md 7.1); this
  // one is here because it sets a PORT WIDTH on this module.
  parameter int unsigned SNAC_PORTS = 2,

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
  // The vertex-attribute store's word (owner ruling R11): slots 1..6 of the
  // packet above -- everything but invw24, which is GEOM.DEPTHQUANT's alone.
  parameter int unsigned GEOM_ATTR_STORE_W = (GEOM_CLIP_ATTRS - 1) * 32,
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

  // (I1's seven `part_rd_*` / `part_wr_*` ports were here. CLOSED 2026-09-19:
  //  the generation store is HPS DDR by contract and `u_part_hps` streams it
  //  through client 3 of `u_terr_hps_arb`. What crosses this edge now is the
  //  HPS's own configuration, in the `terr_cfg_*` shape, below.)

  // ---- PART.STATE's HPS DDR buffers: the HPS's configuration and seed ------
  // NOT A TIE-OFF. PART.STATE.md: "Owns the two particle buffers in HPS DDR";
  // the HPS allocates them (spec/memory_rules.md 5, "particle pools per the
  // charter allocator") and in Verilator the harness IS the HPS (plan D10),
  // exactly as for `terr_cfg_arena_*`. The seed says "buffer `buf` holds
  // `count` records" -- the population descriptor's `active_count`
  // (spec/qformats.md 10) -- and is taken only between ticks.
  input  logic [31:0]             part_cfg_base0_i,
  input  logic [31:0]             part_cfg_base1_i,
  // (I1's four provisional `part_seed_*` ports were here. CLOSED 2026-09-19
  //  under owner rulings R41/R46: the seed is `SetPopulation`'s `active_count`
  //  and the buffer is 0 by that ratification, so `u_part_pop` drives the
  //  store's seed handshake and the board drives neither. The two BASES stay:
  //  they are the HPS allocator's (spec/memory_rules.md 5), not a game-facing
  //  command field, and putting an allocator address in a ratified record is a
  //  decision nobody has made.)
  // The store's evidence. `cur_count` is the generation's length as the
  // hardware counted it; the rest are `zhao_part_hps`'s counters, each fired by
  // stimulus in tests/particles/part_hps_directed.cpp.
  output logic                    part_hps_cur_buf_o,
  output logic [$clog2(PART_CAPACITY):0] part_hps_cur_count_o,
  output logic [31:0]             part_hps_ticks_o,
  output logic [31:0]             part_hps_ticks_dropped_o,
  output logic [31:0]             part_hps_ticks_unseeded_o,
  output logic [31:0]             part_hps_seeds_o,
  output logic [31:0]             part_hps_seeds_refused_o,
  output logic [31:0]             part_hps_rd_bursts_o,
  output logic [31:0]             part_hps_wr_bursts_o,
  output logic [31:0]             part_hps_records_read_o,
  output logic [31:0]             part_hps_records_written_o,
  // R54, 2026-09-19 evening: the bridge refusal `zhao_part_hps` used to be
  // blind to. `bridge_errs` is expected to read ZERO here -- the arbiter
  // pulses the bridge only from A_IDLE and every burst is aligned -- and that
  // expectation is now an EXPECTATION with an instrument behind it rather than
  // an argument standing in place of one. It is fired by stimulus in
  // tests/particles/part_hps_directed.cpp CASE H/I.
  output logic [31:0]             part_hps_bridge_errs_o,
  output logic [31:0]             part_hps_ticks_faulted_o,
  output logic [31:0]             part_hps_records_discarded_o,

  // ---- I33: PART.TABLE's PER-FRAME LOAD -----------------------------------
  // I2 and I3 ARE CLOSED and their twenty-five ports are GONE from this list
  // rather than driven -- `zhao_part_table` is instantiated below and answers
  // all four reads inside this module. What is left is the host that fills it,
  // and this is that seam. One word per clock; the table never refuses for
  // backpressure (`ld_ready_o` is constant high and says so in its own file).
  // (I33's six part_tbl_ld_* ports were here. CLOSED 2026-09-19 under owner
  //  ruling R42: the descriptors travel as DATA in a SPECIES_TABLE page the
  //  owner authors, published by the command that publishes every other
  //  resource, and u_part_table_loader carries the load words from the page
  //  to the port. Nothing in this console chooses what a species IS, which is
  //  the whole reason the entry stayed open. The evidence below is that
  //  block's.)
  output logic [31:0]             part_tbl_pages_o,
  output logic [31:0]             part_tbl_entries_o,
  output logic [31:0]             part_tbl_pages_dropped_o,
  output logic [31:0]             part_tbl_bad_magic_o,
  output logic [31:0]             part_tbl_truncated_o,
  output logic [31:0]             part_tbl_denied_o,

  // ---- (I5's four `part_fld_*` inputs were here. CLOSED 2026-09-20 under
  //  owner ruling R40: `zhao_field_flow_adapter` is the F profile's stream
  //  adapter and it is composed below as client 1 of `u_field_host`. The
  //  acceleration is computed from the flow program's own velocity output by
  //  R40's law, joined to the record it belongs to in the same cycle. The
  //  evidence below is that adapter's.)
  //
  // WHICH RESIDENT PROGRAM IS THE WIND, and whether one is resident at all.
  // NOT A TIE-OFF and not I5 moved sideways: it is the same console-policy
  // shape as `fld_stamp_slot_i` below, for the same reason I30 records --
  // no ratified opcode carries it, so an executor filling it in would be
  // choosing a value the ABI does not contain. With `slot_valid` LOW the
  // adapter answers every record immediately with the sample ABSENT, which
  // PART.UPDATE already handles by not adding the term; that is a console
  // with no wind armed, not a wind of zero.
  input  logic [2:0]              fld_flow_slot_i,
  input  logic                    fld_flow_slot_valid_i,
  // p0..p3 of the flow profile's input record (spec/form/field-ir.md 7.1).
  // The PROGRAM's parameters, not the particle's -- SW.STREAM's, travelling
  // with the plan that named the program (owner ruling R43).
  input  logic [127:0]            fld_flow_par_i,

  output logic [31:0]             part_fld_samples_o,
  output logic [31:0]             part_fld_bypassed_o,
  output logic [31:0]             part_fld_noprog_o,
  output logic [31:0]             part_fld_faults_o,
  output logic [31:0]             part_fld_saturations_o,
  output logic [31:0]             part_fld_stall_cycles_o,
  // The identity guard: the record offered while an answer is held is not the
  // record that answer was computed from. Its two operands are clocked by
  // different things, which is what makes it able to fire at all.
  output logic [31:0]             part_fld_rec_changed_o,

  // (I2's PART.COLLIDE slice -- `part_col_d_response_i` and the three
  //  coefficients -- was here. CLOSED: PART.TABLE serves it below, addressed by
  //  PART.COLLIDE's own new `d_index_o`.)

  // (I6's five `part_ter_*` inputs were here. CLOSED 2026-09-19 under owner
  //  ruling R1: PART.TERRAIN_TAP produces the sample from the live compose
  //  cache through TERRAIN.HEIGHTTAP, inside this module. See item 14.)

  // ---- I6's evidence: the terrain sample's census --------------------------
  // Every particle lands in exactly one of the first four, so a bench can say
  // WHY a particle did or did not see ground -- a cold cell, a void or an
  // unstaged patch, or a fault -- instead of reading PART.COLLIDE's single
  // "unavailable" total. The tap's three are the service's own view of the
  // same traffic.
  output logic [31:0]             part_ter_particles_o,
  output logic [31:0]             part_ter_ground_o,
  output logic [31:0]             part_ter_no_ground_o,
  output logic [31:0]             part_ter_missed_o,
  output logic [31:0]             part_ter_faults_o,     // cell mismatch + out of range
  output logic [31:0]             part_ter_fills_landed_o,
  output logic [31:0]             terr_tap_answered_o,
  output logic [31:0]             terr_tap_off_patch_o,
  output logic [31:0]             terr_tap_faults_o,     // placement + pitch + overflow

  // (I7's eight `part_pop_origin_*` / `part_plane_*` inputs were here. CLOSED
  //  2026-09-19 under owner ruling R41: `SetPopulation` 0x0303 is ratified and
  //  carries all eight, CMD.EXEC lowers it, and `u_part_pop` holds the
  //  descriptor as the levels PART.COLLIDE and PART.TERRAIN_TAP read on every
  //  beat. The evidence below is that bank's.)
  output logic [31:0]             part_pop_taken_o,
  output logic [31:0]             part_pop_refused_normal_o,
  output logic [31:0]             part_pop_refused_count_o,
  output logic [31:0]             part_pop_refused_flags_o,
  output logic [31:0]             part_pop_seeds_issued_o,
  output logic [31:0]             part_pop_handle_o,     // the population it holds
  output logic [31:0]             cmd_exec_pops_o,       // records CMD.EXEC lowered

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

  // ---- I36 IS CLOSED: GEOM.DRAWJOB builds the job (owner ruling R29) -------
  // The nine job ports that stood here are GONE. `zhao_geom_drawjob` resolves
  // the ratified DrawForm -- the MESH_STREAM residency row, that page's frozen
  // header, and the instance transform palette GEOM.LOOM writes -- and drives
  // GEOM.MESHFETCH directly. What leaves is its evidence, one port per REASON,
  // because the nine refusals have nine different diagnoses.
  output logic [31:0]             geom_dj_draws_o,
  output logic [31:0]             geom_dj_jobs_o,
  output logic [31:0]             geom_dj_masked_o,
  output logic [31:0]             geom_dj_empty_o,
  output logic [31:0]             geom_dj_pal_writes_o,
  output logic [31:0]             geom_dj_pal_dropped_o,
  output logic [31:0]             geom_dj_refused_cull_o,
  output logic [31:0]             geom_dj_refused_resident_o,
  output logic [31:0]             geom_dj_refused_stale_o,
  output logic [31:0]             geom_dj_refused_xform_o,
  output logic [31:0]             geom_dj_refused_denied_o,
  output logic [31:0]             geom_dj_refused_format_o,
  output logic [31:0]             geom_dj_refused_crc_o,
  output logic [31:0]             geom_dj_refused_reserved_o,
  output logic [31:0]             geom_dj_refused_layout_o,
  output logic [31:0]             geom_dj_hdr_reads_o,
  output logic [31:0]             geom_dj_hdr_crc_fail_o,
  output logic [31:0]             geom_dj_hdr_framing_o,

  // ---- I50: GEOM.LOOM's NODE STREAM and CAMERA BASIS -- BOUNDARY ----------
  // NEW 2026-09-20, and it is a gap this packet OPENED DELIBERATELY by
  // composing GEOM.LOOM for R29's transform palette. The owner ruling of
  // 2026-08-31 6.4 puts the stream's producer OUTSIDE this console on purpose:
  // "The ARM/compiler supplies a parent-before-child topologically sorted
  // stream." There is no command that carries one and no block that builds
  // one, so the stream arrives here, at the edge, whole -- see the header.
  input  logic                    geom_loom_valid_i,
  output logic                    geom_loom_ready_o,
  input  logic [9:0]              geom_loom_node_index_i,
  input  logic [9:0]              geom_loom_parent_index_i,
  input  logic [3:0]              geom_loom_kind_i,
  input  logic signed [31:0]      geom_loom_param_i [0:11],
  input  logic [15:0]             geom_loom_angle_i,
  input  logic [1:0]              geom_loom_axis_i,
  input  logic                    geom_loom_bodypatch_i,
  input  logic [15:0]             geom_loom_src_id_i,
  input  logic                    geom_loom_first_i,
  input  logic                    geom_loom_last_i,
  input  logic signed [31:0]      geom_loom_cam_basis_i [0:8],
  output logic [31:0]             geom_loom_nodes_o,
  output logic [31:0]             geom_loom_streams_o,
  output logic [31:0]             geom_loom_refused_sorted_o,
  output logic [31:0]             geom_loom_refused_parent_o,
  output logic [31:0]             geom_loom_refused_overflow_o,
  output logic [31:0]             geom_loom_refused_kind_o,
  output logic [31:0]             geom_loom_refused_shear_o,
  output logic [31:0]             geom_loom_refused_framing_o,

  // ---- I37 IS CLOSED: the descriptor's CRC verdict is computed inside ------
  // `u_geom_desc_crc` walks the fold over the returning beats. What leaves is
  // its evidence, so a refused descriptor says WHY at the edge: a CRC that
  // mismatched and a burst that was not eight beats are different faults.
  output logic [31:0]             geom_mf_crc_descriptors_o,
  output logic [31:0]             geom_mf_crc_fail_o,
  output logic [31:0]             geom_mf_crc_framing_o,

  // ---- I38 IS CLOSED: GEOM.REPLAY releases the meshlet, by proof ---------

  // ---- I39 IS CLOSED: the raster word RIDES THE MESHLET -------------------
  // The word is built by GEOM.DRAWJOB from the draw's own flags (R28's cull
  // mode) and carried in the JOB'S handshake through GEOM.MESHFETCH and
  // GEOM.ASSETFETCH to GEOM.ASSEMBLE, so a meshlet's triangles cannot take
  // another draw's state. The port that stood here is GONE.

  // ---- I41 IS CLOSED: the draw dispatch has a consumer INSIDE -------------
  // `cmd_draw_*` no longer leaves the module: GEOM.DRAWJOB is the resolver
  // entry I36 said was missing, and the three handles are resolved against
  // real residency rather than shipped out unresolved. The COUNTERS stay --
  // they are evidence, not a boundary.
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
  // GEOM.SKIN.NORM's world normal, OBSERVED. Entry I43 is CLOSED
  // (2026-09-19, owner ruling R2): its consumer is GEOM.LIGHT --
  // `zhao_light_stream`, through `zhao_light_skin_adapter` -- composed below.
  // The ready is the adapter's, so the port that stood here as `_ready_i`
  // is gone; the normal itself still leaves as a TAP, because the smoke
  // bench differences it against a hand computation and a tap costs nothing.
  output logic                    geom_sn_n_valid_o,
  output logic signed [63:0]      geom_sn_n_x_o,
  output logic signed [63:0]      geom_sn_n_y_o,
  output logic signed [63:0]      geom_sn_n_z_o,
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

  // ---- GEOM.LIGHT (owner ruling R2: `zhao_light_stream` owns vertex light) --
  // The prepared descriptor bank is loaded by COMMAND since owner ruling R25
  // (entry I48, CLOSED 2026-09-19): SetEnvironment 0x0311 -> CMD.EXEC ->
  // GEOM.LIGHT.ENV (`zhao_light_env`) -> the bank, and the power-on default is
  // the same path applied to 4a's default record. The host ports that stood
  // here are gone; the published generation stays as evidence.
  output logic                    geom_light_cfg_gen_o,
  // GEOM.LIGHT.ENV's evidence: bank loads published (the power-on load
  // included), SetEnvironment records taken, and records replaced before they
  // were loaded (fired in tests/geometry/light_env_directed.cpp case 4).
  output logic [31:0]             geom_light_env_loads_o,
  output logic [31:0]             geom_light_env_records_o,
  output logic [31:0]             geom_light_env_superseded_o,
  // The lit vertex RGB, OBSERVED. Its consumer is GEOM.VATTR (entry I46,
  // CLOSED 2026-09-19): the ready is the store's, so the port that stood here
  // as `_ready_i` is gone and the store's ready leaves as a tap beside it, so
  // the smoke bench can count the handshakes it checks against the reference.
  output logic                    geom_light_valid_o,
  output logic                    geom_light_ready_o,
  output logic [16:0]             geom_light_r_o,
  output logic [16:0]             geom_light_g_o,
  output logic [16:0]             geom_light_b_o,
  output logic                    geom_light_degenerate_vtx_o,
  output logic [15:0]             geom_light_src_id_o,
  // Evidence: the lit count, the adapter's narrowing refusals, and every
  // FAULT counter the service has. Its throughput observers (slot and
  // backpressure clocks) stay inside; its directed test is where they are read.
  output logic [31:0]             geom_light_vertices_lit_o,
  output logic [31:0]             geom_light_degenerate_o,
  output logic [31:0]             geom_light_cfg_refused_o,
  output logic [31:0]             geom_light_epoch_refusals_o,
  output logic [31:0]             geom_light_seam_mismatch_o,
  output logic [31:0]             geom_light_tag_mismatch_o,
  output logic [31:0]             geom_light_root_queue_overflow_o,
  output logic [31:0]             geom_light_rgb_sat_o,
  output logic [31:0]             geom_light_nlights_clamped_o,
  output logic [31:0]             geom_light_adapter_refused_o,

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

  // ---- I11 IS CLOSED: GEOM.GROUP_SEQ's job, handle and release are INTERNAL.
  // The job comes from the meshlet dispatcher fork, the handle goes to
  // GEOM.REPLAY and the release comes back from it. Sixteen ports left this
  // list rather than being driven; see the closed ledger in the header.

  // ---- I12 IS CLOSED (owner ruling R27): no arena origin is owed in v1 -----
  // The eight geom_org_* / geom_rep_org_* ports left the list; see the ledger.

  // ---- GEOMETRY evidence ---------------------------------------------------
  output logic [31:0]             geom_groups_opened_o,
  output logic [31:0]             geom_groups_sealed_o,
  // client-A accepts, one per vertex PER VIEW (2x the vertices in dual view)
  output logic [31:0]             geom_view_vertices_sent_o,
  output logic [31:0]             geom_landings_o,
  output logic [31:0]             geom_jobs_refused_o,
  output logic [31:0]             geom_alloc_stall_cycles_o,
  output logic [31:0]             geom_rel_unheld_o,
  output logic                    geom_seal_early_o,
  // R31: GEOM.VDECODE's refusals as GEOM.GROUP_SEQ absorbs them. A hole is a
  // record that will never arrive; its batch is poisoned and dropped whole,
  // and an EARLY hole is one that arrived with no batch held and was carried
  // to the next (structurally excluded by ASSETFETCH's S_HAND -> S_SERVE
  // handshake, so 0 here; fired by stimulus in the directed test).
  output logic [31:0]             geom_holes_o,
  output logic [31:0]             geom_groups_poisoned_o,
  output logic [31:0]             geom_holes_early_o,
  output logic [31:0]             geom_arena_hits_o,
  output logic [31:0]             geom_arena_misses_o,
  output logic [31:0]             geom_arena_refusals_o,
  output logic                    geom_arena_overflow_o,

  // ---- I24 IS CLOSED: the cull mode is the TRIANGLE'S OWN -----------------
  // GEOM.CLIP takes `rp_o_raster[1:0]`, the word GEOM.REPLAY presents beside
  // the corners, which travelled from the draw with the meshlet. The port that
  // stood here is GONE.

  // ---- GEOM.VATTR's evidence (entry I46 CLOSED; owner rulings R11, R31) ----
  // The vertex-attribute store and its writer are INTERNAL: the eleven
  // `geom_att_*` ports that modelled the store at the edge are gone. What
  // leaves is the store's census and its faults, and the depth law's two
  // faults, which moved here from GEOM.REPLAY with the law itself.
  output logic [31:0]             geom_va_landings_o,
  output logic [31:0]             geom_va_rows_written_o,
  output logic [31:0]             geom_va_colours_written_o,
  output logic [31:0]             geom_va_uv_staged_o,
  output logic [31:0]             geom_va_lq_overflow_o,
  output logic [31:0]             geom_va_index_oob_o,
  output logic [31:0]             geom_va_look_oob_o,
  output logic [31:0]             geom_va_profile_mixed_o,
  output logic [31:0]             geom_va_dq_refused_o,
  output logic [31:0]             geom_va_dq_stray_o,
  // Review of d52ae6c0: depth results that WAITED for their u/v (a handshake,
  // not a fault), and the batch poison GEOM.VATTR adds to GROUP_SEQ's.
  output logic [31:0]             geom_va_uv_waits_o,
  output logic                    geom_va_poison_o,

  // ---- GEOM.REPLAY's evidence ----------------------------------------------
  output logic [31:0]             geom_rp_meshlets_o,
  output logic [31:0]             geom_rp_groups_o,
  output logic [31:0]             geom_rp_triangles_in_o,
  output logic [31:0]             geom_rp_triangles_out_o,
  output logic [31:0]             geom_rp_refused_o,
  output logic [31:0]             geom_rp_missed_o,
  output logic [31:0]             geom_rp_att_skew_o,
  output logic [31:0]             geom_rp_view_bad_o,
  // R31: triangles GEOM.REPLAY dropped because their batch lost a record.
  output logic [31:0]             geom_rp_poisoned_o,
  // R57: a TriangleDescriptor refused because the two-meshlet descriptor queue
  // was full. Backpressure, never a drop -- and the instrument that says the
  // queue's sizing assumption (twice MAX_TRIANGLES) still holds.
  output logic [31:0]             geom_rp_triq_stall_o,

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

  // ---- I26 CLOSED 2026-09-19 (terrain3): the spine is ON the TERRAIN.BUILD socket
  // The terrain HPS arbiter's one bridge port and the pool's two guard clients
  // (TERRAIN.PAGELOADER's writes, the compose path's read share) used to stop
  // at this edge. They now reach the shell's REAL `zhao_hps_bridge` and
  // `zhao_mem_guard` through `u_build_share` and the socket's HPS client 1 --
  // see `u_build_share` below. What crosses the edge is the socket share's
  // evidence: contention between its three requesters, and its two tripwires.
  output logic [31:0]             terr_bsock_contention_o,
  output logic [31:0]             terr_bsock_retire_unowned_o,
  output logic [31:0]             terr_bsock_wbeat_unowned_o,

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

  // ---- THE F-SHEET JOURNAL DOORBELL: SW.STREAM's own words (R14, D10) ------
  // NOT A TIE-OFF, and not entry I28 moved sideways: I28 is CLOSED. TERRAIN.SEQ
  // -> the doorbell -> TERRAIN.WRITEBACK -> TERRAIN.RESIDENCY is composed below,
  // and what crosses this edge is the HPS itself -- the journal descriptor, the
  // grants it posts, the tickets the hardware returns and the ACKs it sends.
  // In Verilator the harness IS the HPS (plan D10), exactly as it is for the
  // FRAME_RING view and `terr_cfg_*` above. Owner ruling R14 names SW.STREAM the
  // owner; design/contracts/TERRAIN.WRITEBACK.DOORBELL.md is the exchange.
  input  logic [31:0]             terr_cfg_journal_base_i,   // D0
  input  logic [31:0]             terr_cfg_journal_bytes_i,  // D0
  input  logic                    terr_jdb_post_valid_i,     // D1: a grant
  output logic                    terr_jdb_post_ready_o,
  input  logic [15:0]             terr_jdb_post_slot_i,
  input  logic [31:0]             terr_jdb_post_ticket_i,
  output logic                    terr_jdb_ret_valid_o,      // D2: a return
  input  logic                    terr_jdb_ret_ready_i,
  output logic [31:0]             terr_jdb_ret_ticket_o,
  output logic                    terr_jdb_ret_final_o,
  output logic                    terr_jdb_ret_ok_o,
  output logic [3:0]              terr_jdb_ret_verdict_o,
  input  logic                    terr_jdb_ack_valid_i,      // D3: the ACK
  output logic                    terr_jdb_ack_ready_o,
  input  logic [31:0]             terr_jdb_ack_ticket_i,
  input  logic                    terr_jdb_ack_ok_i,
  // ...and the evidence both blocks keep. Events and cycles named apart.
  output logic [31:0]             terr_wb_sheets_written_o,
  output logic [31:0]             terr_wb_sheets_refused_o,
  output logic [31:0]             terr_wb_sheets_faulted_o,
  output logic [31:0]             terr_wb_guard_denied_o,
  output logic [31:0]             terr_wb_acks_unmatched_o,
  output logic [31:0]             terr_wb_acks_overdue_o,
  output logic [31:0]             terr_jdb_starved_cycles_o,
  output logic [31:0]             terr_jdb_ret_overflow_o,

  // ==========================================================================
  // THE TERRAIN COMPOSE ENGINE'S OWN BOUNDARY (connected item 10)
  // ==========================================================================

  // ---- I26 (extended) CLOSED 2026-09-19: the compose path's read share is
  // requester 2 of `u_build_share`, on the shell's slot-6 socket. Its ports
  // left this list with the rest of I26.

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
  output logic [31:0]             terr_rdshare_jobs_wb_o,  // 2: TERRAIN.WRITEBACK
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
  // Client 2, TERRAIN.WRITEBACK's journal writes (owner ruling R4's N-client
  // arbiter). Its wait is the number that says what the loader costs it.
  output logic [31:0]             terr_hps_c2_bursts_o,
  output logic [31:0]             terr_hps_c2_wait_cycles_o,
  // Client 3, PART.STATE's generation store (`u_part_hps`, entry I1 closed).
  output logic [31:0]             terr_hps_c3_bursts_o,
  output logic [31:0]             terr_hps_c3_wait_cycles_o,
  // Rule 6c / R55: a second, DIFFERENT request offered by a client whose
  // pending slot is already occupied is DROPPED, and used to be dropped in
  // silence. These two are that reading -- a count of distinct dropped
  // offerings and a sticky mask naming the client. Expected zero here, and
  // the arbiter's header argues structurally why; the argument is no longer
  // the only thing standing where the instrument should be.
  output logic [31:0]             terr_hps_pend_dropped_o,
  output logic [3:0]              terr_hps_pend_dropped_mask_o,

  // ---- MEM.UPLOAD, composed on the shell's TERRAIN.BUILD socket ----------
  // Its REQUEST is internal: CMD.EXEC lowers the ratified `PublishResource`
  // onto it (owner ruling R17). These two are CMD.EXEC's upload evidence.
  output logic [31:0]             cmd_exec_uploads_o,
  output logic [31:0]             cmd_exec_upload_overflow_o,
  // R35/R36: SetPost / SetGradeTable, each fired by tests/command/cmd_exec_directed.cpp.
  output logic [31:0]             cmd_exec_post_looks_o,       // looks handed to POST.COMPOSITE
  output logic [31:0]             cmd_exec_grade_entries_o,    // product vectors written
  output logic [31:0]             cmd_exec_post_refused_o,     // records REFUSED (flags/bias/header)
  output logic [31:0]             cmd_exec_grade_overflow_o,   // entries refused for staging room
  // Host configuration, the terrain spine's `terr_cfg_*` shape: the
  // destination region MEM.GUARD's TERRAIN_BUILD arm must also admit (in
  // TERRAIN.PAGE_POOL always; in RENDER.ASSET_POOL through R32's arm, which is
  // bounded by exactly this region), the HPS
  // staging arena the active epoch registered, and that epoch.
  input  logic [31:0]             upl_cfg_region_base_i,
  input  logic [31:0]             upl_cfg_region_bytes_i,
  input  logic [63:0]             upl_cfg_arena_base_i,
  input  logic [31:0]             upl_cfg_arena_bytes_i,
  input  logic [15:0]             upl_cfg_epoch_i,
  // The PUBLICATION, `spec/memory_rules.md` 5f.1's directory row, and the
  // verdict. Observable here as well as consumed inside, so a harness can
  // difference a publication against `zref::mem` without reaching in.
  output logic                    upl_publish_valid_o,
  output logic [ 7:0]             upl_publish_slot_o,
  output logic [15:0]             upl_publish_generation_o,
  output logic [ 7:0]             upl_publish_tag_o,
  output logic [23:0]             upl_publish_index_o,
  output logic [31:0]             upl_publish_base_o,
  output logic [31:0]             upl_publish_extent_o,
  output logic                    upl_done_o,
  output logic [ 7:0]             upl_status_o,
  output logic [15:0]             upl_published_o,
  output logic [127:0]            upl_refused_o,
  output logic [31:0]             upl_hps_wait_o,

  // ---- MATERIAL.RESOLVE (composed 2026-09-19, cmdmem packet, ruling R20) ---
  // I49: its REQUEST and its RESPONSE -- BOUNDARY. Directory and fetch are
  // internal and real; see the entry for the one seam in the way.
  // I49, CLOSED 2026-09-20 (texmat2). The REQUEST and the RESPONSE's ready
  // are INTERNAL: `u_material_window` issues one resolve per distinct material
  // from the triangle's own {material_set, material_id, semantic weight} and
  // consumes the answer. Five ports left this list rather than being driven
  // from constants. The response FIELDS stay as outputs, because a harness
  // differencing a resolve against `zref::material` must be able to read them
  // without reaching inside.
  output logic                    mat_rsp_valid_o,
  output logic [ 2:0]             mat_rsp_status_o,
  output logic                    mat_rsp_has_record_o,
  output logic [255:0]            mat_rsp_record_o,
  output logic [ 7:0]             mat_rsp_quality_tier_o,
  output logic [ 1:0]             mat_rsp_sample_count_o,
  output logic [ 2:0]             mat_rsp_material_recipe_o,
  output logic [ 7:0]             mat_rsp_recipe_weight_o,
  output logic [ 7:0]             mat_rsp_base_binding_o,
  output logic                    mat_rsp_selector_overflow_o,
  output logic [31:0]             mat_rsp_palette_base_o,
  output logic [31:0]             mat_rsp_raster_state_o,
  output logic [ 7:0]             mat_rsp_flags_o,
  output logic [ 7:0]             mat_rsp_sample0_modes_o,
  output logic [ 7:0]             mat_rsp_sample1_modes_o,
  output logic [ 7:0]             mat_rsp_sample2_modes_o,
  output logic [31:0]             mat_hits_o,
  output logic [31:0]             mat_misses_o,
  output logic [31:0]             mat_refused_o,
  output logic [31:0]             mat_refused_id_o,
  output logic [31:0]             mat_refused_record_o,
  output logic [31:0]             mat_not_resident_o,
  output logic [31:0]             mat_selector_overflow_o,
  output logic [31:0]             mat_recipe_count_mismatch_o,
  output logic [31:0]             mat_fetch_denied_o,
  // ---- the WINDOW's evidence (entry I49) ---------------------------------
  // `mat_win_resolves_o` against `mat_win_switches_o` is the "counters see
  // what pictures cannot" reading: a window that re-resolved a material it
  // already held would produce a byte-identical frame and spend the meshlet
  // loop's clocks twice. The two stall counters are split because they have
  // different cures. The last two are STRUCTURAL guards and read zero in any
  // correct composition.
  output logic [31:0]             mat_win_resolves_o,
  output logic [31:0]             mat_win_switches_o,
  output logic [31:0]             mat_win_drain_stall_o,
  output logic [31:0]             mat_win_answer_stall_o,
  output logic [31:0]             mat_win_occupancy_max_o,
  output logic [31:0]             mat_win_no_record_o,
  output logic [31:0]             mat_win_selector_overflow_o,
  // Loud rather than silent: a CLUT material needs the binding page's palette
  // slot and generation as witnesses and NOTHING in this console produces
  // them. See FINDINGS-texmat2's owner decision.
  output logic [31:0]             mat_win_clut_unowned_o,
  output logic [31:0]             mat_win_err_unpublished_o,
  output logic [31:0]             mat_win_err_underflow_o,
  output logic [31:0]             geom_ma_jobs_c_o,
  output logic [31:0]             geom_ma_jobs_d_o,
  output logic [31:0]             geom_ma_jobs_e_o,

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

  // ---- TERRAIN's LIT NORMALS: the per-triangle base light (R21) -----------
  // Part of entry I13's terrain triangle packet, not a new boundary: the
  // entry's own sentence says joining terrain to GEOM.CLIP needs "terrain's
  // own attribute packet (invw24 from GEOM.DEPTHQUANT for terrain w, and
  // TERRAIN.SHADE's light)". This is that light, computed here, leaving on the
  // same edge as the triangle it belongs to and tagged with the same src_id.
  // Its producer chain is REAL end to end: the world vertex is stored on the
  // projector's own fill beat, the face normal is `zhao_terrain_normals` and
  // the shade is `zhao_terrain_shade`, with the sun from SetEnvironment
  // through `zhao_light_env` (R25). The consumer is I13's absent merge.
  output logic                    terr_light_valid_o,
  input  logic                    terr_light_ready_i,
  output logic signed [31:0]      terr_light_base_o,
  output logic                    terr_light_degenerate_o,
  output logic [15:0]             terr_light_src_id_o,
  output logic [31:0]             terr_light_refs_taken_o,
  output logic [31:0]             terr_light_emitted_o,
  output logic [31:0]             terr_light_stale_reads_o,
  output logic [31:0]             terr_light_normals_o,
  output logic [31:0]             terr_light_shaded_o,
  output logic [31:0]             terr_light_degenerate_count_o,
  output logic [31:0]             terr_light_base_sat_o,
  output logic [31:0]             terr_light_degen_mismatch_o,
  output logic [31:0]             proj_contended_o,
  output logic [31:0]             proj_mat_refused_o,

  // ---- I17: the compositor's absent neighbours ----------------------------
  // `post_view_sel_i` and the source stream `post_s_*` are GONE FROM THIS EDGE
  // (I15, 2026-09-19): the pass, its view and its pixels come from the shell's
  // `zhao_post_lease`, which reads the back buffer in raster order.
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
  // THE LOOK AND THE GRADING TABLE ARE GONE FROM THIS EDGE, 2026-09-19 (post
  // pass 2, owner rulings R35/R36): post_bloom_gain_i, post_grade_valid_i,
  // post_pv_*, post_bias_*, post_flash_* and post_ink_rgb_i are driven by
  // CMD.EXEC's SetPost / SetGradeTable arm (section 7c). Entry I17 (a)(b).
  output logic                    post_hud_req_v_o,
  output logic [POST_XW-1:0]      post_hud_req_x_o,
  output logic [POST_YW-1:0]      post_hud_req_y_o,
  input  logic                    post_hud_valid_i,
  input  logic [15:0]             post_hud_rgb_i,
  // `post_o_*` and `post_echo_*` are GONE FROM THIS EDGE (I16, 2026-09-19):
  // the composited stream is written back through RASTER.FBWRITE inside the
  // shell's post lease, and the echo tap feeds POST.ECHO there.

  // ---- POST.COMPOSITE's LEASE and POST.ECHO: evidence ----------------------
  output logic                    post_busy_o,             // armed/running: frame not publishable
  output logic [31:0]             post_passes_o,           // compositor passes written back
  output logic [31:0]             post_frames_o,           // render frames fully post-processed
  output logic                    post_fault_o,            // a refused source read
  output logic [31:0]             post_src_reads_o,        // 64-byte back-buffer reads
  output logic [31:0]             post_src_pixels_o,       // pixels handed to the compositor
  output logic [31:0]             post_retire_unowned_o,   // tripwire: must read 0
  output logic [31:0]             post_share_contention_o, // ENGINE0 share waits
  output logic [31:0]             echo_passes_complete_o,  // WHOLE captures
  output logic [31:0]             echo_passes_torn_o,
  output logic [31:0]             echo_pixels_written_o,
  output logic [31:0]             echo_pixels_dropped_o,
  output logic                    echo_fault_o,

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

  // ---- THE HISTOGRAM. Its EVENTS ARE NO LONGER HERE ------------------------
  // I18 CLOSED 2026-09-20 (owner ruling R70). `hist_ev_valid_i`,
  // `hist_ev_lane_valid_i`, `hist_ev_err_i`, `hist_ev_src_id_i` and
  // `hist_ev_ready_o` LEFT THIS PORT LIST rather than being driven from a
  // harness: `zhao_terrain_lodfeed` is composed below and its deviation
  // records are the events. Its HOST WINDOW went the same way one day earlier
  // -- `hist_rd_*` was entry I19 and is now driven inside this file by
  // `u_hostreg_hist` off the HPS register aperture (section 7b-iii, ruling
  // R51). What is left here is only the block's OUTPUT evidence.
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
  // `tri_flat_request_i` LEFT THIS LIST 2026-09-20 (entry I49). It is built a
  // few thousand lines below from MATERIAL.RESOLVE's published answer, exactly
  // as `tri_area2_i` and the three attribute planes were retired before it.
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

  // ---- INPUT.SNAC's physical edge (owner ruling R7, input_rules.md 7) -----
  // PINS OF THE PART, NOT A BOUNDARY. Exactly the class `pad_buttons_i` above
  // and `hps_req_*` are in, and for the same reason the HOST.REGWIN
  // composition states at length further down: the far end is a connector on
  // the board, not a block nobody has built. In Verilator the harness IS the
  // controller, as it is the HPS for the burst bridge.
  //
  // The adapter is composed below, BETWEEN these pads and the shell, so a
  // slot with a real PS1 pad on it is driven by that pad and every other slot
  // carries `pad_*_i` through untouched. With nothing plugged in -- DAT idles
  // high, every poll times out -- the merge is the identity and this console
  // behaves exactly as it did before the block existed.
  input  logic [SNAC_PORTS-1:0] snac_dat_i,
  input  logic [SNAC_PORTS-1:0] snac_ack_n_i,
  output logic [SNAC_PORTS-1:0] snac_att_n_o,
  output logic                  snac_clk_o,
  output logic                  snac_cmd_o,
  output logic [3:0]            snac_present_o,
  output logic [63:0]           snac_polls_o,
  output logic [63:0]           snac_timeouts_o,
  output logic [63:0]           snac_bad_header_o,
  output logic [63:0]           snac_overrides_o,
  output logic [63:0]           snac_seq_gaps_o,

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
  // ---- TEXTURE EVIDENCE (entry I49, 2026-09-20, texmat2) ------------------
  // Promoted from inside the raster tile pipe, where seven of these dangled at
  // the shell's instantiation and the eighth was sunk as an unused wire. The
  // composed console could not previously answer the only question that
  // matters at this seam: DID THE ISLAND SAMPLE. It can now, and the answer is
  // measured rather than argued.
  output logic [31:0] render_texture_fragments_o,
  output logic [31:0] render_texture_cache_hits_o,
  output logic [31:0] render_texture_cache_misses_o,
  output logic [31:0] render_texture_palette_lookups_o,
  output logic [31:0] render_texture_plan_accepted_o,
  output logic [31:0] render_texture_dispatch_accepted_o,
  output logic [31:0] render_texture_combine_refused_o,
  output logic [31:0] render_texture_samples_o,

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
  // (I30's OPEN HALF was here: surf_cmd_env_* -- the patch envelope, which
  //  entry I27 recorded as having no placement owner anywhere in the tree --
  //  and the three policy bits no opcode carries. CLOSED 2026-09-19 under
  //  owner ruling R45: u_surface_dispatch resolves the patch by the SAME
  //  world->patch law zhao_terrain_heighttap inverts, from the stamp's own
  //  translation and the live pitch, and carries the policy in three named
  //  parameters with blend_en = 0 as R45 ratifies. surf_cmd_field_en_i went
  //  with them: the policy was already "a stamp program is resident", so the
  //  residency IS the producer and the host had nothing to add.)
  input  logic        [15:0] surf_cmd_src_id_i,
  // The dispatch's evidence.
  output logic [31:0]        surf_disp_dispatched_o,
  output logic [31:0]        surf_disp_pitch_refused_o,
  output logic [31:0]        surf_disp_env_clamped_o,
  output logic signed [15:0] surf_disp_patch_ix_o,
  output logic signed [15:0] surf_disp_patch_iz_o,
  // The rectangle itself, because a patch index alone cannot be checked
  // against the stamp's own geometry and an unchecked envelope is how a stamp
  // lands somewhere plausible and wrong.
  output logic signed [31:0] surf_disp_env_x0_o,
  output logic signed [31:0] surf_disp_env_x1_o,

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
  // DEBUG.TRACE's evidence.  Added 2026-09-19 with the ring.
  // --------------------------------------------------------------------------
  // OUTPUTS ONLY as of 2026-09-20. The arming and the host readout that stood
  // here as entry I45's BOUNDARY are both closed INSIDE this file now:
  //
  //   arming   CMD.EXEC lowers `DebugTraceArm` 0xF003 (owner ruling R52) and
  //            drives `arm_we`/`arm_mask`/`clear` at section 7c.
  //   readout  `u_hostreg_trace` answers the HPS register aperture at section
  //            7b-iii (owner ruling R51), which is the same carrier that closed
  //            MEASURE.HISTOGRAM's I19 in the same pass -- I45's own text asked
  //            for exactly that: "Closing one closes both, and they should be
  //            closed together rather than twice."
  //
  // These three remain because they are what a BENCH reads. `armed_o` is also
  // readable by the host at aperture word 0x1800, which is the useful asymmetry:
  // a host can confirm what the command stream armed without being able to arm
  // behind its back (ruling R18, one authority per level).
  output logic [ 6:0] dbg_trace_armed_o,
  output logic [31:0] dbg_trace_count_o,
  output logic [31:0] dbg_trace_dropped_o,

  // --------------------------------------------------------------------------
  // HOST.REGWIN -- the HPS lightweight-bridge CSR aperture (owner ruling R51).
  // --------------------------------------------------------------------------
  // THE CONSOLE'S SECOND HOST PORT, and it is a physical edge of the part in
  // the same class as `hps_req_*` and `pad_buttons_i`, not a boundary standing
  // in for something unbuilt. On the Cyclone V SoC the HPS drives two bridges:
  // the h2f DATA bridge, which `zhao_hps_bridge` uses for 64-byte bursts, and
  // this narrow 32-bit lightweight bridge, whose entire purpose is the ARM
  // reading and writing FPGA registers one word at a time.
  //
  // The aperture is 64 KiB of byte address, sixteen 4 KiB tenant regions, and
  // the map is FROZEN in `spec/memory_rules.md` section 8. Two tenants are
  // populated: MEASURE.HISTOGRAM at 0x0000 and DEBUG.TRACE at 0x1000. Every
  // other region, every misaligned address and every write is REFUSED with a
  // response and counted -- it never hangs, ruling R20's law.
  //
  // No-escape is STRUCTURAL rather than checked: the word offset handed to a
  // tenant is TENANT_LSB-2 bits wide, so there is no wire on which one tenant
  // could be given another's address. See the block's own header.
  input  logic        hostreg_valid_i,
  input  logic        hostreg_write_i,
  input  logic [15:0] hostreg_addr_i,     // byte address within the aperture
  input  logic [31:0] hostreg_wdata_i,
  output logic        hostreg_ready_o,
  output logic        hostreg_rvalid_o,
  output logic [31:0] hostreg_rdata_o,
  output logic        hostreg_err_o,
  output logic [31:0] hostreg_reads_o,
  output logic [31:0] hostreg_writes_o,
  output logic [31:0] hostreg_refused_unmapped_o,
  output logic [31:0] hostreg_refused_misaligned_o,
  output logic [31:0] hostreg_refused_tenant_o,
  output logic [31:0] hostreg_refused_timeout_o,
  output logic [31:0] hostreg_stall_cycles_o,

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
  // R25: committed SetEnvironment records handed to GEOM.LIGHT.ENV.
  output logic [31:0] cmd_exec_envs_o,
  // R52: committed DebugTraceArm records handed to DEBUG.TRACE, and the ones
  // REFUSED for a reserved bit set on the wire. The second is the interesting
  // one: it is the guard that keeps a stray bit from arming a set of stages
  // nobody asked for, and the smoke fires it deliberately.
  output logic [31:0] cmd_exec_trace_arms_o,
  output logic [31:0] cmd_exec_trace_arm_refused_o,

  // ==========================================================================
  // MEASURE.TOKENS (rulings R18/R33, 2026-09-19, cmdmem packet)
  // ==========================================================================
  // Its BUDGET side is composed: CMD.EXEC commits SetPresentationContract's
  // five counts as the CEILING and each SetView's two counts as that view's
  // REQUEST, which the guard clamps to the ceiling. Its REQUEST/RETURN side is
  // a BOUNDARY (entry I18): the one consumer that exists, GEOM.BINNER, offers a
  // ONE-BIT token client (`tok_req_o`, tied off inside the shell) against this
  // block's seven-field request, and the other five fields are policy nobody
  // produces yet. So the request and return enter here, and everything the
  // guard decides leaves here, where a bench -- or the next packet -- reads it.
  input  logic        tok_req_valid_i,
  input  logic        tok_req_view_i,
  input  logic        tok_req_class_i,
  input  logic        tok_req_essential_i,
  input  logic [ 2:0] tok_req_rep_i,
  input  logic [31:0] tok_req_cost_i,
  input  logic [15:0] tok_req_src_id_i,
  output logic        tok_grant_o,
  output logic        tok_shared_o,
  input  logic        tok_ret_valid_i,
  input  logic        tok_ret_view_i,
  input  logic        tok_ret_class_i,
  input  logic        tok_ret_shared_i,
  input  logic [31:0] tok_ret_cost_i,
  output logic        tok_den_valid_o,
  output logic        tok_den_view_o,
  output logic        tok_den_class_o,
  output logic [ 2:0] tok_den_rep_o,
  output logic [ 1:0] tok_den_reason_o,
  output logic [15:0] tok_den_src_id_o,
  output logic [31:0] tok_den_cost_o,
  output logic [31:0] tok_avail_geom0_o,
  output logic [31:0] tok_avail_geom1_o,
  output logic [31:0] tok_avail_frag0_o,
  output logic [31:0] tok_avail_frag1_o,
  output logic [31:0] tok_avail_shared_o,
  output logic [31:0] tok_rep_count0_o,
  output logic [31:0] tok_rep_count1_o,
  output logic [31:0] tok_rep_count2_o,
  output logic [31:0] tok_rep_count3_o,
  output logic [31:0] tok_rep_count4_o,
  output logic [31:0] tok_rep_count5_o,
  output logic [31:0] tok_rep_count6_o,
  output logic [31:0] tok_rep_count7_o,
  output logic [31:0] tok_triangles_culled_o,
  output logic [31:0] tok_vreq_clamped_o,
  output logic [31:0] cmd_exec_contracts_o,

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

  // THE COARSE-HEIGHT MIP PLANES ARE RETIRED, owner ruling R64, 2026-09-20.
  // Entry I44 carried them as a boundary. They are gone, and the argument is
  // in that entry's closure note; the short form is that ruling T8's
  // decimation is NESTED and UNROUNDED, so `mip17[i,j] == fine33[2i,2j]` bit
  // for bit and a coarse vertex IS a fine vertex. A plane store could
  // therefore never produce a number the fine lattice does not already
  // contain -- it is a DUPLICATE PROVIDER, not a source -- and the bit
  // identity is now a committed test (`tests/terrain/
  // terrain_mipgen_directed.cpp` case 3b) rather than prose in a contract.
  //
  // THE COUNTERS STAY, and that is deliberate rather than an oversight. They
  // are the only remaining evidence at this boundary that the decimation ran
  // at all, and without them the block's whole coarse path would be pruned
  // silently -- which is the shape this file's own I32 entry warns about for
  // layer D. TERRAIN.MIPGEN also stays composed: its `done_o` is
  // TERRAIN.RESIDENCY's SECOND COMPLETION, and without it `resident_o` is
  // structurally zero and no patch ever reaches the compose door.
  output logic [31:0]  terr_mg_m17_writes_o,
  output logic [31:0]  terr_mg_m9_writes_o,
  output logic [31:0]  terr_mg_aborts_o,

  // ---- TERRAIN.LODFEED, and these four are why entry I18 can be read at all
  // Composed 2026-09-20 under owner ruling R70: `zhao_terrain_lodfeed` observes
  // the mip pass's fine stream and its records are MEASURE.HISTOGRAM's events.
  // The chain is entirely internal, so WITHOUT THESE COUNTERS a console-level
  // bench could not tell "the histogram saw no events because the metric is
  // broken" from "because no page was ever mipped" -- and those two need
  // different repairs.
  //
  // THEY READ ZERO IN THE SMOKE AND THAT IS THE MEASURED, EXPLAINED ANSWER,
  // not an unexamined zero: every page the bench plays fails its CRC (the
  // directory reports `crc_fail=3`), so `tpl_fin_ok` never rises, so
  // TERRAIN.MIPREQ issues no job, so TERRAIN.MIPFEED never streams a lattice.
  // The smoke prints the whole chain of zeros on one line for exactly this
  // reason. The counters are FIRED, non-zero, by
  // `tests/terrain/terrain_lodhist_directed.cpp`, which drives the same
  // arrangement with a lattice that moves.
  output logic [31:0]  terr_lodfeed_lattices_walked_o,
  output logic [31:0]  terr_lodfeed_lattices_dropped_o,
  output logic [31:0]  terr_lodfeed_dev_records_o,
  output logic [31:0]  terr_lodfeed_stray_samples_o,

  // ==========================================================================
  // THE FIELD ENGINE'S EDGE. I42, and it is ONE entry where there were THREE.
  // ==========================================================================
  // THE FIELD PROGRAM DOORBELL -- SW.STREAM's own words, owner ruling R43.
  // NOT A TIE-OFF, and not entry I42 moved sideways: I42 is CLOSED. The
  // loader words and both directory phases are driven by
  // `zhao_field_doorbell` below, and what crosses THIS edge is the HPS
  // itself -- the plan's epoch identity, the posts it makes and the ticketed
  // returns hardware hands back. In Verilator the harness IS the HPS,
  // exactly as it is for `terr_jdb_*` above (owner ruling R14, the pattern
  // R43 names) and for the FRAME_RING view.
  //
  // `post_op_i` is 0 LOAD WORD, 1 COMMIT, 2 LOOKUP. For a LOAD WORD,
  // `post_kind_i` is the host's own 0 uop / 1 table entry / 2 header /
  // 3 uniform, and the HEADER is written LAST because it is what marks a slot
  // runnable -- so a partially written program can never execute.
  input  logic [31:0]  fld_cfg_plan_base_i,   // D0: held, trace only
  input  logic         fld_db_post_valid_i,
  output logic         fld_db_post_ready_o,
  input  logic [ 1:0]  fld_db_post_op_i,
  input  logic [ 1:0]  fld_db_post_kind_i,
  input  logic [ 2:0]  fld_db_post_slot_i,
  input  logic [ 6:0]  fld_db_post_addr_i,
  input  logic [95:0]  fld_db_post_data_i,
  input  logic [31:0]  fld_db_post_hash_i,
  input  logic         fld_db_post_ok_i,
  input  logic [31:0]  fld_db_post_ticket_i,
  output logic         fld_db_ret_valid_o,
  input  logic         fld_db_ret_ready_i,
  output logic [31:0]  fld_db_ret_ticket_o,
  output logic [ 1:0]  fld_db_ret_op_o,
  output logic         fld_db_ret_ok_o,
  output logic         fld_db_ret_refused_o,
  output logic         fld_db_ret_inserted_o,
  output logic         fld_db_ret_evicted_o,
  output logic [ 2:0]  fld_db_ret_slot_o,
  output logic [31:0]  fld_db_ret_plan_o,
  output logic [31:0]  fld_db_posts_o,
  output logic [31:0]  fld_db_load_words_o,
  output logic [31:0]  fld_db_lookups_o,
  output logic [31:0]  fld_db_commits_o,
  // A COMMIT for a slot whose HEADER was not written since the last commit.
  // REFUSED, ANSWERED and COUNTED (owner ruling R20) -- the directory is never
  // offered a hash for microcode that is not there.
  output logic [31:0]  fld_db_commits_refused_o,
  // Cycles a post was offered into a full mailbox. HELD, never dropped: owner
  // ruling R55's shape, and the number that says whether POSTS is big enough.
  output logic [31:0]  fld_db_post_stalls_o,
  // Unreachable while the return credit is right, so its zero is an argument
  // and not a measurement. Fired by tests/mutants/zhao_field_doorbell_mutant.sv.
  output logic [31:0]  fld_db_ret_overflow_o,

  // CONSOLE POLICY: which resident program is the stamp brush, and whether one
  // is resident at all. The same shape as `surf_cmd_field_en_i` beside it and
  // for the same reason -- no opcode carries either, and I30 already records
  // that an executor filling them in would be choosing values the ABI does not
  // contain.
  input  logic [ 2:0]  fld_stamp_slot_i,
  input  logic         fld_stamp_slot_valid_i,

  // (THE ENGINE'S SECOND CLIENT was here, as `fld_req_*` / `fld_resp_*`. It is
  //  CLOSED 2026-09-20: entry I42 said it "is the seam the FLOW and EARTH
  //  adapters take over when I5 and I34 close", and the FLOW adapter has taken
  //  it. `zhao_field_flow_adapter` is client 1 and `zhao_field_stamp_adapter`
  //  is client 0, so the arbiter's contention is still reachable with legal
  //  stimulus -- two REAL profiles offering in the same cycle, which is what
  //  the edge port was standing in for. `fld_contended_grants_o` below is the
  //  counter that was the entry's reason for keeping it.)

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

  // THE TERRAIN SPINE's HPS PORT, internal since entry I26 closed: the bridge
  // side of `u_terr_hps_arb`, carried to the TERRAIN.BUILD socket's client 1.
  zhao_hps_burst_req_t     terr_hps_req;
  logic                    terr_hps_grant;
  logic                    terr_hps_wr_valid, terr_hps_wr_last, terr_hps_wr_ready;
  logic [63:0]             terr_hps_wr_data;
  zhao_hps_burst_rsp_t     terr_hps_rsp;

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
  // THE LIGHTING SEAM. LOW on purpose: GEOM.LIGHT is composed (R2) and its
  // bank is loaded by SetEnvironment (R25, I48 closed), but its RGB stops at
  // GEOM.REPLAY -- the raster's attribute planes carry no colour. The header's
  // lighting section says what is connected and what is not.
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
  // PART.TERRAIN_TAP, between PART.UPDATE and PART.COLLIDE (item 14).
  wire                  ptt_p_ready, ptt_q_valid;
  wire [PART_REC_W-1:0] ptt_q_record;
  wire [3:0]            ptt_q_events;
  wire [PART_PID_W:0]   ptt_q_side;      // {survive, ordinal}: glue 4, amended
  wire                  ptt_t_valid;
  wire signed [PART_POS_W-1:0] ptt_t_height;
  wire signed [PART_NRM_W-1:0] ptt_t_nx, ptt_t_ny, ptt_t_nz;
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
  //
  // AMENDED 2026-09-19, AND THE AMENDMENT IS THE SAME ARGUMENT ONE STAGE LATER.
  // PART.TERRAIN_TAP (item 14) now sits between PART.UPDATE and PART.COLLIDE,
  // so a particle can be held in it while the collider holds the one before.
  // Loading these two on PART.UPDATE's retire would then describe the particle
  // BEHIND the collider's -- a metadata swap with every count still balancing.
  // So they ride THROUGH the new stage in its `side` register, loaded by the
  // same enable as the record there, and are loaded here on the COLLIDER'S
  // take, which is the same condition PART.COLLIDE loads its record on.
  wire pu_take_c = pu_out_valid && ptt_p_ready;
  wire pc_take_c = ptt_q_valid  && pc_p_ready;

  logic                  pc_survive_q;
  logic [PART_PID_W-1:0] pc_ordinal_q;

  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      pc_survive_q <= 1'b0;
      pc_ordinal_q <= '0;
    end else if (pc_take_c) begin
      pc_survive_q <= ptt_q_side[PART_PID_W];
      pc_ordinal_q <= ptt_q_side[PART_PID_W-1:0];
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

  // ==========================================================================
  // PART.TABLE's HOST -- entry I33 CLOSED 2026-09-19 (gz/pfs2, ruling R42).
  // ==========================================================================
  // The entry's argument was "NO RATIFIED COMMAND CARRIES A SPECIES DESCRIPTOR
  // ... Inventing one here would mean this file choosing what a species IS,
  // which is owner DATA". R42 answers it without anyone choosing that: the
  // descriptors travel as a SPECIES_TABLE page (spec/cartridge.md 4 kind 13),
  // published by the PublishResource this console already executes, and
  // `u_part_table_loader` carries the load words from the page to the port.
  // The page's byte layout is frozen in `zref::species_page`; its CONTENTS are
  // the owner's and nothing in this module reads a descriptor field.
  //
  // THE TRIGGER IS THE PUBLICATION, not the command: by then the page is
  // resident, CRC-checked and bounded. The READ is requester E of
  // `u_geom_mem_adapter` -- the same asset window MATERIAL.RESOLVE's record
  // fetch uses, and the rarest traffic on it.
  logic                      ptl_ld_valid, ptl_ld_ready;
  logic [1:0]                ptl_ld_sel, ptl_ld_event;
  logic [6:0]                ptl_ld_index;
  logic [PART_TBL_LD_W-1:0]  ptl_ld_data;
  zhao_guard_req_t           ptl_guard_req;
  zhao_guard_rsp_t           ptl_guard_rsp;
  logic                      ptl_beat_valid;
  logic [63:0]               ptl_beat_data;
  /* verilator lint_off UNUSEDSIGNAL */
  // The loader counts its own eight beats per line, so `last` is corroboration
  // rather than control -- the same reading MATERIAL.RESOLVE takes of it. And
  // `busy` is PART.TABLE's own back-pressure by construction: the load port
  // never refuses, so nothing here needs to wait for the loader.
  logic                      ptl_beat_last;
  logic                      ptl_busy_unused;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_part_table_loader #(
    .LD_W     (PART_TBL_LD_W),
    .PAGE_KIND(PART_KIND_SPECIES_TABLE),
    .CLIENT   (ZHAO_CLIENT_ENGINE1)
  ) u_part_table_loader (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    .pub_valid_i (upl_publish_valid_o),
    .pub_tag_i   (upl_publish_tag_o),
    .pub_base_i  (upl_publish_base_o),
    .pub_extent_i(upl_publish_extent_o),

    .g_req_o       (ptl_guard_req),
    .g_rsp_i       (ptl_guard_rsp),
    .g_beat_valid_i(ptl_beat_valid),
    .g_beat_data_i (ptl_beat_data),

    .ld_valid_o(ptl_ld_valid),
    .ld_ready_i(ptl_ld_ready),
    .ld_sel_o  (ptl_ld_sel),
    .ld_index_o(ptl_ld_index),
    .ld_event_o(ptl_ld_event),
    .ld_data_o (ptl_ld_data),

    .pages_o        (part_tbl_pages_o),
    .entries_o      (part_tbl_entries_o),
    .pages_dropped_o(part_tbl_pages_dropped_o),
    .bad_magic_o    (part_tbl_bad_magic_o),
    .truncated_o    (part_tbl_truncated_o),
    .denied_o       (part_tbl_denied_o),
    .busy_o         (ptl_busy_unused)
  );

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

    // REAL (I33 closed, R42): the load comes from a published SPECIES_TABLE
    // page, read by u_part_table_loader below.
    .ld_valid_i (ptl_ld_valid),
    .ld_ready_o (ptl_ld_ready),
    .ld_sel_i   (ptl_ld_sel),
    .ld_index_i (ptl_ld_index),
    .ld_event_i (ptl_ld_event),
    .ld_data_i  (ptl_ld_data),

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

  // ==========================================================================
  // PART.STATE's GENERATION STORE -- entry I1, CLOSED 2026-09-19 (gz/pfs).
  // ==========================================================================
  // PART.STATE.md puts both generations in HPS DDR ("Dense sequential
  // ping-pong in HPS DDR ... Owns the two particle buffers in HPS DDR and
  // nothing else on chip") -- 512 KiB each at the required tier, which no
  // amount of M10K holds. `zhao_part_state` streams records and knows nothing
  // of memory; `zhao_part_hps` is the streamer between it and the bridge: two
  // buffers swapped per tick, 64-byte bursts of four records, a hardware count
  // of what was written, and a tick that is atomic at the buffer level.
  //
  // IT ALSO OWNS THE TICK'S START. PART.STATE starts only when the store has a
  // population and is not still flushing the previous generation's tail; a
  // console tick that arrives while it is is counted (`part_hps_ticks_dropped_o`)
  // -- the same drop PART.STATE already made silently while busy.
  //
  // `rd_empty` is new on PART.STATE with this closure: a generation of zero
  // records had no way to say it was over, and the survivor pass waited for a
  // last record that could not come.
  //
  // The bridge tag is ENGINE1. `zhao_client_e` is full at three bits and id 5
  // is held unspent by ruling T3, so a new id is not available; the tag on an
  // HPS burst selects only which `hps_ddr_bytes_by_client` row the bridge
  // charges. Particle state is engine-side per-tick work, not background
  // streaming (TERRAIN.BUILD) and not command acquisition (ENGINE0, CMD.DMA's).
  // Provisional, one constant to change: `PART_HPS_CLIENT` below.
  localparam zhao_client_e PART_HPS_CLIENT = ZHAO_CLIENT_ENGINE1;

  // ==========================================================================
  // PART.POP -- entry I7 CLOSED 2026-09-19 (gz/pfs2, owner ruling R41).
  // ==========================================================================
  // The population descriptor's frame values -- origin, analytic plane and
  // `active_count` -- were eight board pins and a provisional seed port,
  // because "no ratified command carries a population descriptor to this
  // core". R41 ratified one: `SetPopulation` 0x0303. The chain composed here
  // is a REAL one end to end -- the command packet carries the record,
  // `u_cmd_decoder` validates it, `u_cmd_exec` lowers it on a clean verdict,
  // `u_part_pop` holds it and refuses what the engine's formats cannot carry,
  // and PART.COLLIDE, PART.TERRAIN_TAP and the generation store read it.
  //
  // THE REFUSALS ARE THE BANK'S, NOT THE EXECUTOR'S, on purpose: the widths
  // that can be breached are PART.COLLIDE's, and a second opinion about them
  // living in the command path is how two truths start to drift.
  logic        cmd_pop_valid, cmd_pop_ready;
  logic [31:0] cmd_pop_population, cmd_pop_ox, cmd_pop_oy, cmd_pop_oz;
  logic [31:0] cmd_pop_count, cmd_pop_pc;
  logic [15:0] cmd_pop_nx, cmd_pop_ny, cmd_pop_nz, cmd_pop_flags;

  logic signed [31:0]           pop_origin_x_c, pop_origin_y_c, pop_origin_z_c;
  logic                         pop_plane_en_c;
  logic signed [PART_NRM_W-1:0] pop_plane_nx_c, pop_plane_ny_c, pop_plane_nz_c;
  logic signed [31:0]           pop_plane_c_c;
  logic                         pop_seed_valid, pop_seed_ready, pop_seed_buf;
  logic [$clog2(PART_CAPACITY):0] pop_seed_count;

  zhao_part_pop #(
    .CAPACITY(PART_CAPACITY),
    .CNT_W   ($clog2(PART_CAPACITY) + 1),
    .NRM_W   (PART_NRM_W)
  ) u_part_pop (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    .rec_valid_i       (cmd_pop_valid),
    .rec_ready_o       (cmd_pop_ready),
    .rec_population_i  (cmd_pop_population),
    .rec_origin_x_i    (cmd_pop_ox),
    .rec_origin_y_i    (cmd_pop_oy),
    .rec_origin_z_i    (cmd_pop_oz),
    .rec_active_count_i(cmd_pop_count),
    .rec_plane_c_i     (cmd_pop_pc),
    .rec_plane_nx_i    (cmd_pop_nx),
    .rec_plane_ny_i    (cmd_pop_ny),
    .rec_plane_nz_i    (cmd_pop_nz),
    .rec_flags_i       (cmd_pop_flags),

    .origin_x_o  (pop_origin_x_c),
    .origin_y_o  (pop_origin_y_c),
    .origin_z_o  (pop_origin_z_c),
    .plane_en_o  (pop_plane_en_c),
    .plane_nx_o  (pop_plane_nx_c),
    .plane_ny_o  (pop_plane_ny_c),
    .plane_nz_o  (pop_plane_nz_c),
    .plane_c_o   (pop_plane_c_c),
    .population_o(part_pop_handle_o),

    .seed_valid_o(pop_seed_valid),
    .seed_ready_i(pop_seed_ready),
    .seed_buf_o  (pop_seed_buf),
    .seed_count_o(pop_seed_count),

    .taken_o          (part_pop_taken_o),
    .refused_normal_o (part_pop_refused_normal_o),
    .refused_count_o  (part_pop_refused_count_o),
    .refused_flags_o  (part_pop_refused_flags_o),
    .seeds_issued_o   (part_pop_seeds_issued_o)
  );

  zhao_hps_burst_req_t ptb_hps_req;
  logic                ptb_hps_grant;
  zhao_hps_burst_rsp_t ptb_hps_rsp;
  logic [63:0]         ptb_hps_wdata;
  logic                ptb_hps_wvalid, ptb_hps_wlast;

  logic                   ph_tick_start, ph_rd_empty, ph_tick_abort;
  logic                   ph_rd_valid, ph_rd_ready, ph_rd_last;
  logic [PART_REC_W-1:0]  ph_rd_record;
  logic                   ph_wr_valid, ph_wr_ready;
  logic [PART_REC_W-1:0]  ph_wr_record;
  /* verilator lint_off UNUSEDSIGNAL */
  logic                   ph_busy_unused;   // PART.STATE's own tick_busy is the port
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_part_hps #(
    .CAPACITY(PART_CAPACITY),
    .CNT_W   ($clog2(PART_CAPACITY) + 1),
    .CLIENT  (PART_HPS_CLIENT)
  ) u_part_hps (
    .clk              (gpu_clk),
    .rst_n            (rst_n),
    .cfg_base0_i      (part_cfg_base0_i),
    .cfg_base1_i      (part_cfg_base1_i),
    // REAL (I7/R41): the seed is SetPopulation's `active_count`, held by
    // `u_part_pop` until this block takes it.
    .seed_valid_i     (pop_seed_valid),
    .seed_ready_o     (pop_seed_ready),
    .seed_buf_i       (pop_seed_buf),
    .seed_count_i     (pop_seed_count),
    .tick_i           (core_tick_c),
    .ps_tick_start_o  (ph_tick_start),
    .ps_rd_empty_o    (ph_rd_empty),
    .ps_tick_abort_o  (ph_tick_abort),
    .ps_tick_done_i   (part_tick_done_o),
    .rd_valid_o       (ph_rd_valid),
    .rd_ready_i       (ph_rd_ready),
    .rd_record_o      (ph_rd_record),
    .rd_last_o        (ph_rd_last),
    .wr_valid_i       (ph_wr_valid),
    .wr_ready_o       (ph_wr_ready),
    .wr_record_i      (ph_wr_record),
    .hps_req_o        (ptb_hps_req),
    .hps_grant_i      (ptb_hps_grant),
    .hps_rsp_i        (ptb_hps_rsp),
    .hps_wr_valid_o   (ptb_hps_wvalid),
    .hps_wr_data_o    (ptb_hps_wdata),
    .hps_wr_last_o    (ptb_hps_wlast),
    .hps_wr_ready_i   (terr_hps_wr_ready),
    .busy_o           (ph_busy_unused),
    .cur_buf_o        (part_hps_cur_buf_o),
    .cur_count_o      (part_hps_cur_count_o),
    .ticks_o          (part_hps_ticks_o),
    .ticks_dropped_o  (part_hps_ticks_dropped_o),
    .ticks_unseeded_o (part_hps_ticks_unseeded_o),
    .seeds_o          (part_hps_seeds_o),
    .seeds_refused_o  (part_hps_seeds_refused_o),
    .rd_bursts_o      (part_hps_rd_bursts_o),
    .wr_bursts_o      (part_hps_wr_bursts_o),
    .records_read_o   (part_hps_records_read_o),
    .records_written_o(part_hps_records_written_o),
    .bridge_errs_o       (part_hps_bridge_errs_o),
    .ticks_faulted_o     (part_hps_ticks_faulted_o),
    .records_discarded_o (part_hps_records_discarded_o)
  );

  zhao_part_state #(
    .CAPACITY  (PART_CAPACITY),
    .CHILD_D   (PART_CHILD_D),
    .SPECIES_N (PART_SPECIES_N),
    .REC_W     (PART_REC_W)
  ) u_part_state (
    .clk          (gpu_clk),
    .rst_n        (rst_n),
    .tick_start_i (ph_tick_start),
    .tick_busy_o  (part_tick_busy_o),
    .tick_done_o  (part_tick_done_o),

    // I8 CLOSED 2026-09-19. PART.STATE now tells PART.SPAWN when the generation
    // is full, so the capacity backstop is a real internal producer->consumer
    // edge instead of a pin the board had to drive.
    .capacity_full_o (part_capacity_full_c),

    // REAL (I1 closed): the previous generation, read out of HPS DDR by
    // `u_part_hps`.
    .rd_valid_i   (ph_rd_valid),
    .rd_ready_o   (ph_rd_ready),
    .rd_record_i  (ph_rd_record),
    .rd_last_i    (ph_rd_last),
    .rd_empty_i   (ph_rd_empty),
    // REAL (R54): the store could not finish reading the generation out of
    // DDR, so the survivor pass ends here instead of waiting forever.
    .tick_abort_i (ph_tick_abort),

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
    // REAL, 2026-09-19: PART.SPAWN still holds a parent of this generation --
    // it is not idle, or the fork's S branch still holds the collider's beat
    // for it. The append phase waits for both, so a collision child is
    // written in the tick whose collision made it (qformats 10's tick law),
    // whatever the latency in front of PART.COLLIDE. Item 14 moved that
    // latency and exposed the race; this is the defect report's option (a).
    .chl_busy_i   (!sp_par_ready || fork_spw_valid_c),

    // REAL (I1 closed): the next generation, posted to `u_part_hps`.
    .wr_valid_o   (ph_wr_valid),
    .wr_ready_i   (ph_wr_ready),
    .wr_record_o  (ph_wr_record),

    .survivors_o                  (part_survivors_o),
    .children_written_o           (part_children_written_o),
    .children_dropped_capacity_o  (part_children_dropped_capacity_o),
    .staging_stall_cycles_o       (part_staging_stall_cycles_o),
    .species_refused_o            (part_species_refused_o)
  );

  // The field join's three wires (entry I5). `pu_in_ready_c` is PART.UPDATE's
  // own ready; PART.STATE sees it ANDed with the answer's presence, and
  // `pfa_rec_take` is the one cycle the record actually moves -- which is what
  // retires the adapter's answer. Retiring on `ps_prt_valid` falling instead
  // would carry record A's acceleration into record B's offer whenever
  // PART.STATE presents back to back, which is the metadata-swap shape.
  logic pu_in_ready_c;
  assign ps_prt_ready  = pu_in_ready_c && pfa_ans_valid;
  assign pfa_rec_take  = ps_prt_valid && pfa_ans_valid && pu_in_ready_c;

  zhao_part_update #(
    .SPECIES_N (PART_SPECIES_N),
    .REC_W     (PART_REC_W),
    .AGE_W     (PART_AGE_W)
  ) u_part_update (
    .clk        (gpu_clk),
    .rst_n      (rst_n),

    // REAL: from PART.STATE, THROUGH THE FIELD JOIN (entry I5, closed
    // 2026-09-20). The entry named this join itself and said a composer may
    // write it: "this file gates `in_valid_i` and PART.STATE's `prt_ready_i`
    // on 'the answer for THIS record is ready', and the record is held stable
    // for the whole offer."
    //
    // So the record is offered to PART.UPDATE only while the FLOW adapter is
    // holding the acceleration computed FROM THAT RECORD, and PART.STATE's
    // ready is the AND of the two. Both sides of every comparison inside
    // PART.UPDATE are then the same cycle's wires, which is what its own
    // header requires of this seam. `part_fld_rec_changed_o` is the guard that
    // says so out loud rather than leaving it as an argument.
    .in_valid_i (ps_prt_valid && pfa_ans_valid),
    .in_ready_o (pu_in_ready_c),
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

    // I5 CLOSED 2026-09-20: the F profile's stream adapter, on client 1 of the
    // one field engine. `fld_valid_i` LOW is the honest answer when no wind is
    // armed or the run refused -- PART.UPDATE adds the term only when it is
    // high, so a low valid REMOVES the acceleration rather than adding a zero
    // one, which is the same distinction entry I34 draws for terrain height.
    .fld_valid_i(pfa_fld_valid),
    .fld_ax_i   (pfa_fld_ax),
    .fld_ay_i   (pfa_fld_ay),
    .fld_az_i   (pfa_fld_az),

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
    .out_ready_i  (ptt_p_ready),
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

  // ==========================================================================
  // 14. PART.COLLIDE'S TERRAIN SAMPLE.  ENTRY I6, CLOSED 2026-09-19.
  //
  //   TERRAIN.COMPCACHE --read ports-- TERRAIN.HEIGHTTAP --cells-- PART.TERRAIN_TAP
  //                                                                   |  t_*
  //         PART.UPDATE ---------------- particle ------------------> PART.COLLIDE
  //
  // I6 said "the collision test needs {height, nx, ny, nz}" and nothing emitted
  // a normal; the law for one was contradicted in writing. OWNER RULING R1
  // (reports/OWNER-RULINGS-20260919-EVENING.md) made it `normalize3_approx(
  // face_normal(t))` of the triangle spec/terrain_rules.md 4.3 picks for the
  // height, and spec 4.4 now says so for collision. `zhao_terrain_heighttap`
  // computes it (through `zhao_field_v3_normalize`, the one implementation of
  // that law) and answers with the whole CELL; `zhao_part_terrain_tap` caches
  // cells so the particle path is NEVER stalled on this multi-cycle read, which
  // PART.COLLIDE's contract forbids by name -- a particle over an uncached cell
  // goes on with no sample, counted, and the cell is fetched behind it.
  //
  // THE TAP IS A PASS-THROUGH IN FRONT OF THE COMPOSE CACHE'S READ PORTS, and
  // TERRAIN.TESS -- their owner -- is never gated or delayed by it (its header,
  // and `terrain_heighttap_directed.cpp` case 8). It takes only cycles TESS did
  // not want.
  //
  // COHERENCE. A cached cell is a copy of what the compose cache SERVES. The
  // invalidation below is every event that can change that: a fill starting or
  // completing, a served patch retired, a placement write, a cell-state write.
  // Over-invalidating costs refetches; under-invalidating would hand a
  // particle a crater that has been filled in. It is deliberately the former.
  //
  // WHAT THIS DOES NOT CLAIM. The compose cache stages ONE patch at a time, so
  // a particle over any other patch gets no sample -- counted as no-ground,
  // not faked. That is the cache's shape (T6's 256-entry composed cache is a
  // later block), not this seam's, and the census says how often it bites.
  // ==========================================================================
  // Both are driven in the terrain section (item 14's second half, beside the
  // compose cache), where the nets they are built from are declared.
  wire              ptt_inval_c;
  wire signed [7:0] ptt_pitch_c;

  wire                      htp_req_valid, htp_req_ready, htp_req_surface;
  wire signed [31:0]        htp_req_x, htp_req_z;
  wire                      htp_rsp_valid, htp_rsp_no_ground;
  wire signed [31:0]        htp_h00, htp_h10, htp_h01, htp_h11, htp_wx00, htp_wz00;
  wire [4:0]                htp_sh;
  wire signed [31:0]        htp_na_x, htp_na_y, htp_na_z, htp_nb_x, htp_nb_y, htp_nb_z;
  wire [31:0]               ptt_mismatch, ptt_range;
  // The point answer's height and normal are the SERVICE's answer to its
  // requester; this requester evaluates its own points from the cell, so it
  // reads the cell and not the point. FORGE.SHADOW, the point answer's other
  // customer, is not composed (its own blocker, the creature rung, is in the
  // FORGE.SHADOW header). The census members not exported are summed into the
  // two fault ports or are cadence evidence tested at block level.
  /* verilator lint_off UNUSEDSIGNAL */
  wire signed [31:0]        htp_height, htp_nx, htp_ny, htp_nz;
  wire [31:0]               htp_void, htp_place_bad, htp_pitch_bad, htp_ovf, htp_stall, htp_nsat;
  wire [31:0]               ptt_hsat, ptt_issued, ptt_discarded, ptt_invals;
  /* verilator lint_on UNUSEDSIGNAL */

  assign part_ter_faults_o = ptt_mismatch + ptt_range;
  assign terr_tap_faults_o = htp_place_bad + htp_pitch_bad + htp_ovf;

  zhao_part_terrain_tap #(
    .REC_W  (PART_REC_W),
    .POS_W  (PART_POS_W),
    .NRM_W  (PART_NRM_W),
    .NRM_Q  (10),
    .CELLS  (4),
    .SIDE_W (PART_PID_W + 1)
  ) u_part_terrain_tap (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    // I7, widened: the population origin has no producer in this core.
    .origin_x_i(pop_origin_x_c),
    .origin_y_i(pop_origin_y_c),
    .origin_z_i(pop_origin_z_c),
    // REAL: the same net TERRAIN.PLACE and TERRAIN.HEIGHTTAP read.
    .pitch_log2_i(ptt_pitch_c),
    .inval_i     (ptt_inval_c),

    // REAL: PART.UPDATE's retire, with glue 4's two facts riding beside it.
    .p_valid_i (pu_out_valid),
    .p_ready_o (ptt_p_ready),
    .p_record_i(pu_out_record),
    .p_events_i(pu_out_events),
    .p_side_i  ({pu_out_survive, part_ordinal_q}),

    // REAL: into PART.COLLIDE, sample and all.
    .q_valid_o (ptt_q_valid),
    .q_ready_i (pc_p_ready),
    .q_record_o(ptt_q_record),
    .q_events_o(ptt_q_events),
    .q_side_o  (ptt_q_side),
    .t_valid_o (ptt_t_valid),
    .t_height_o(ptt_t_height),
    .t_nx_o    (ptt_t_nx),
    .t_ny_o    (ptt_t_ny),
    .t_nz_o    (ptt_t_nz),

    // REAL: TERRAIN.HEIGHTTAP.
    .tap_req_valid_o  (htp_req_valid),
    .tap_req_ready_i  (htp_req_ready),
    .tap_req_x_o      (htp_req_x),
    .tap_req_z_o      (htp_req_z),
    .tap_req_surface_o(htp_req_surface),
    .tap_rsp_valid_i    (htp_rsp_valid),
    .tap_rsp_no_ground_i(htp_rsp_no_ground),
    .tap_rsp_h00_i (htp_h00),
    .tap_rsp_h10_i (htp_h10),
    .tap_rsp_h01_i (htp_h01),
    .tap_rsp_h11_i (htp_h11),
    .tap_rsp_wx00_i(htp_wx00),
    .tap_rsp_wz00_i(htp_wz00),
    .tap_rsp_sh_i  (htp_sh),
    .tap_rsp_na_x_i(htp_na_x),
    .tap_rsp_na_y_i(htp_na_y),
    .tap_rsp_na_z_i(htp_na_z),
    .tap_rsp_nb_x_i(htp_nb_x),
    .tap_rsp_nb_y_i(htp_nb_y),
    .tap_rsp_nb_z_i(htp_nb_z),

    .particles_o        (part_ter_particles_o),
    .samples_ground_o   (part_ter_ground_o),
    .samples_no_ground_o(part_ter_no_ground_o),
    .samples_missed_o   (part_ter_missed_o),
    .cell_mismatch_o    (ptt_mismatch),
    .out_of_range_o     (ptt_range),
    .height_sats_o      (ptt_hsat),
    .fills_issued_o     (ptt_issued),
    .fills_landed_o     (part_ter_fills_landed_o),
    .fills_discarded_o  (ptt_discarded),
    .invalidations_o    (ptt_invals)
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

    // REAL: from PART.TERRAIN_TAP (item 14), which carries PART.UPDATE's
    // record and its three own event bits through unchanged. Bit 2 arrives
    // zero and this block fills it (ruling I4).
    .p_valid_i(ptt_q_valid),
    .p_ready_o(pc_p_ready),
    .p_record_i(ptt_q_record),
    .p_events_i(ptt_q_events),

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

    // REAL: I6 CLOSED 2026-09-19 -- the live composed terrain, through
    // TERRAIN.HEIGHTTAP and PART.TERRAIN_TAP (item 14), on the same beat.
    .t_valid_i (ptt_t_valid),
    .t_height_i(ptt_t_height),
    .t_nx_i    (ptt_t_nx),
    .t_ny_i    (ptt_t_ny),
    .t_nz_i    (ptt_t_nz),

    // I7: the one plane, a per-frame owner value with no CMD path.
    .pl_en_i(pop_plane_en_c),
    .pl_nx_i(pop_plane_nx_c),
    .pl_ny_i(pop_plane_ny_c),
    .pl_nz_i(pop_plane_nz_c),
    .pl_c_i (pop_plane_c_c),

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
  wire [GEOM_INDEX_W-1:0]  ln_fill_index;   // the landed vertex's arena index (GEOM.VATTR)
  // GEOM.VATTR's colour ready (into GEOM.LIGHT) and its batch-complete verdict
  // (the gate on GROUP_SEQ's handle into REPLAY). Declared here, ahead of their
  // first readers; the block is instantiated at section 11 beside GEOM.REPLAY.
  wire                     va_lit_ready;
  wire                     va_done;
  wire                     va_poison;   // the batch lost a row (VATTR)
  assign geom_va_poison_o = va_poison;

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
  //
  // AND IT REACHES GEOM.GROUP_SEQ AS A HOLE (owner ruling R31, 2026-09-19).
  // Silent at the skinner is right; silent at the sequencer was a DEADLOCK: it
  // waited for `count` skinned vertices, one never came, nothing sealed, and
  // REPLAY, ASSETFETCH and the whole geometry path stopped with every counter
  // still. `d_refused_o` now drives `u_geom_group_seq.hole_i`, the batch ends on
  // vertices + holes == count, its groups are handed over POISONED, and
  // GEOM.REPLAY drops the meshlet's triangles and releases it -- the contract's
  // "a refusal drops the BATCH, not the frame". Counted on `geom_holes_o`,
  // `geom_groups_poisoned_o` and `geom_rp_poisoned_o`; the smoke bench's
  // `-BadVertex` control corrupts one record in SDRAM and requires all three to
  // move and the frame to complete.
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

  // GEOM.SKIN.NORM's ROOT IS GONE FROM HERE (owner ruling R31, 2026-09-19). A
  // second `zhao_field_isqrt` instance used to serve it: 32 serial steps a
  // vertex, which made the AND-fork above stall GEOM.SKIN ~27 clocks a vertex
  // (smoke: fork_stall_cycles=217 over 8). The magnitude is now taken by
  // GEOM.LIGHT's own II8 root -- idle on the creature path until now -- so the
  // root is time-multiplexed, not duplicated. See `zhao_geom_skin_norm`.
  //
  // GEOM.LIGHT's skin adapter takes the normal (entry I43 closed); its ready
  // is read by GEOM.SKIN.NORM just below, so it is declared here.
  wire        la_s_ready;

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


    // I43: the world normal leaves the module. Its consumer is GEOM.LIGHT and
    // that seam is refused for reasons of its own -- see the lighting section.
    .n_valid_o      (geom_sn_n_valid_o),
    .n_ready_i      (la_s_ready),
    .n_x_o          (geom_sn_n_x_o),
    .n_y_o          (geom_sn_n_y_o),
    .n_z_o          (geom_sn_n_z_o),
    .n_degenerate_o (geom_sn_n_degenerate_o),
    .n_src_id_o     (geom_sn_n_src_id_o),

    .vertices_o   (geom_sn_vertices_o),
    .degenerate_o (geom_sn_degenerate_o),
    .reduced_o    (geom_sn_reduced_o)
  );


  // --------------------------------------------------------------------------
  // GEOM.LIGHT -- `zhao_light_stream`, composed 2026-09-19 by owner ruling R2
  // ("zhao_light_stream owns vertex light; zhao_geom_light is superseded").
  //
  //   GEOM.SKIN.NORM --{s64x3}--> zhao_light_skin_adapter
  //                  --{s32x3}--> zhao_light_stream (|n| rooted HERE) --> RGB
  //
  // THE CREATURE PATH, by both blocks' own contracts: the profile is CREATURE
  // (no detail term, the early clamp), and since owner ruling R31 the stream
  // ROOTS the normal itself (`n_mag_valid_i` LOW -- the stream's own
  // "computed here, ONCE" case), because its II8 root is the one root this
  // path needs and SKIN.NORM's serial one was stalling the skinner. The two
  // constants below are that contract, named, not tie-offs; `n_mag_i` is not
  // read while `n_mag_valid_i` is low (the stream seals the slot on its root).
  // The adapter NARROWS by assertion, not by cast, and refuses and counts a
  // tuple outside the producer's range reduction.
  //
  // NO SECOND LAW: the service holds the shared dot/root/divide, the
  // `zhao_light_div32_ii2` and `zhao_light_isqrt64_ii8` leaves inside it, and
  // `zhao_geom_light` -- the scalar arrangement measured at II = 167 -- is NOT
  // composed (console_inventory: superseded, R2).
  // --------------------------------------------------------------------------
  localparam logic GEOM_LIGHT_MAG_SUPPLIED_C = 1'b0;  // GEOM.LIGHT roots |n| (R31)
  localparam logic GEOM_LIGHT_PROFILE_C      = 1'b1;  // 1 = creature
  assign geom_light_ready_o = va_lit_ready;

  wire               la_p_valid, la_p_ready;
  wire signed [31:0] la_p_nx, la_p_ny, la_p_nz;
  wire               la_p_degenerate;
  wire        [ 3:0] la_p_nlights;
  wire        [15:0] la_p_src_id;
  /* verilator lint_off UNUSEDSIGNAL */
  wire        [31:0] la_accepted;
  // The service's throughput observers: read by its directed test, where a
  // stall has a stimulus to be measured against; not faults.
  wire [31:0] ls_normal_inputs, ls_normal_prepared, ls_roots_issued, ls_roots_retired,
              ls_supplied_mags, ls_terms_accepted, ls_terms_retired, ls_terms_null,
              ls_normal_queue_wait, ls_descriptor_wait, ls_dot_slots, ls_square_slots,
              ls_unused_slots, ls_divider_bp, ls_colour_bp, ls_output_bp,
              ls_raw_sat, ls_degen_terms, ls_clamp_lo, ls_clamp_hi;
  /* verilator lint_on UNUSEDSIGNAL */
  wire        ls_idle, ls_v_ready;

  // ---- GEOM.LIGHT.ENV: SetEnvironment -> the bank (owner ruling R25, I48) ---
  // CMD.EXEC presents the committed record (section 7b's u_cmd_exec, further
  // down; the nets are declared here, ahead of their first reader). The block
  // computes 4a's sun direction on its own `zhao_field_sin`, writes light 0 and
  // the environment words, and publishes them with ONE commit -- while HOLDING
  // the stream's vertex input, and only once the stream reports idle, so the
  // environment never changes under a vertex and no bank write can be refused.
  wire        cmd_env_valid, cmd_env_ready;
  wire [15:0] cmd_env_yaw, cmd_env_pitch, cmd_env_sun, cmd_env_amb;
  wire        le_cfg_we, le_cfg_commit, le_hold;
  // R21/R25: the sun direction the bank publishes, for TERRAIN's lit normals.
  wire signed [31:0] le_sun_x, le_sun_y, le_sun_z;
  wire [7:0]  le_cfg_addr;
  wire [31:0] le_cfg_data;
  wire [3:0]  le_nlights;

  zhao_light_env u_light_env (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    .e_valid_i      (cmd_env_valid),
    .e_ready_o      (cmd_env_ready),
    .e_sun_yaw_i    (cmd_env_yaw),
    .e_sun_pitch_i  (cmd_env_pitch),
    .e_sun_colour_i (cmd_env_sun),
    .e_ambient_i    (cmd_env_amb),

    .cfg_we_o     (le_cfg_we),
    .cfg_commit_o (le_cfg_commit),
    .cfg_addr_o   (le_cfg_addr),
    .cfg_data_o   (le_cfg_data),

    .hold_o         (le_hold),
    .stream_idle_i  (ls_idle),
    .nlights_o      (le_nlights),

    // R21: the same direction light 0's words carry, published for terrain.
    .sun_x_o        (le_sun_x),
    .sun_y_o        (le_sun_y),
    .sun_z_o        (le_sun_z),

    .loads_o      (geom_light_env_loads_o),
    .records_o    (geom_light_env_records_o),
    .superseded_o (geom_light_env_superseded_o)
  );

  // The hold is an AND on BOTH halves of the adapter -> stream handshake, so a
  // held vertex is neither taken nor lost: it waits in the adapter.
  assign la_p_ready = ls_v_ready && !le_hold;

  zhao_light_skin_adapter #(
    .SRCW (16)
  ) u_light_skin_adapter (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: GEOM.SKIN.NORM's world normal. Entry I43, CLOSED.
    .s_valid_i      (geom_sn_n_valid_o),
    .s_ready_o      (la_s_ready),
    .s_nx_i         (geom_sn_n_x_o),
    .s_ny_i         (geom_sn_n_y_o),
    .s_nz_i         (geom_sn_n_z_o),
    .s_degenerate_i (geom_sn_n_degenerate_o),
    // How many lights the published set holds: GEOM.LIGHT.ENV's, with the set.
    .s_nlights_i    (le_nlights),
    .s_src_id_i     (geom_sn_n_src_id_o),

    .p_valid_o      (la_p_valid),
    .p_ready_i      (la_p_ready),
    .p_nx_o         (la_p_nx),
    .p_ny_o         (la_p_ny),
    .p_nz_o         (la_p_nz),
    .p_degenerate_o (la_p_degenerate),
    .p_nlights_o    (la_p_nlights),
    .p_src_id_o     (la_p_src_id),

    .accepted_o     (la_accepted),
    .refused_o      (geom_light_adapter_refused_o)
  );

  zhao_light_stream #(
    .SRCW (16)
  ) u_light_stream (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // The prepared descriptor bank, loaded by GEOM.LIGHT.ENV (R25, I48 closed).
    .cfg_we_i     (le_cfg_we),
    .cfg_commit_i (le_cfg_commit),
    .cfg_addr_i   (le_cfg_addr),
    .cfg_data_i   (le_cfg_data),
    .cfg_gen_o    (geom_light_cfg_gen_o),

    // REAL: the prepared normal, from the adapter -- held while the bank loads.
    .v_valid_i      (la_p_valid && !le_hold),
    .v_ready_o      (ls_v_ready),
    .n_x_i          (la_p_nx),
    .n_y_i          (la_p_ny),
    .n_z_i          (la_p_nz),
    .n_mag_valid_i  (GEOM_LIGHT_MAG_SUPPLIED_C),
    .n_mag_i        (32'd0),   // not read: n_mag_valid_i is low
    .n_degenerate_i (la_p_degenerate),
    .n_profile_i    (GEOM_LIGHT_PROFILE_C),
    .n_lights_i     (la_p_nlights),
    .n_src_id_i     (la_p_src_id),

    // I46: the lit RGB, the attribute store's r/g/b input.
    .r_valid_o        (geom_light_valid_o),
    .r_ready_i        (va_lit_ready),   // GEOM.VATTR takes the colour (I46)
    .rgb_r_o          (geom_light_r_o),
    .rgb_g_o          (geom_light_g_o),
    .rgb_b_o          (geom_light_b_o),
    .degenerate_vtx_o (geom_light_degenerate_vtx_o),
    .src_id_o         (geom_light_src_id_o),

    .normal_inputs_o         (ls_normal_inputs),
    .normal_prepared_o       (ls_normal_prepared),
    .roots_issued_o          (ls_roots_issued),
    .roots_retired_o         (ls_roots_retired),
    .supplied_mags_o         (ls_supplied_mags),
    .terms_accepted_o        (ls_terms_accepted),
    .terms_retired_o         (ls_terms_retired),
    .terms_null_o            (ls_terms_null),
    .normal_queue_wait_o     (ls_normal_queue_wait),
    .descriptor_wait_o       (ls_descriptor_wait),
    .dot_product_slots_o     (ls_dot_slots),
    .square_product_slots_o  (ls_square_slots),
    .unused_product_slots_o  (ls_unused_slots),
    .divider_backpressure_o  (ls_divider_bp),
    .colour_backpressure_o   (ls_colour_bp),
    .output_backpressure_o   (ls_output_bp),
    .epoch_refusals_o        (geom_light_epoch_refusals_o),
    .logical_raw_saturations_o(ls_raw_sat),
    .degenerate_terms_o      (ls_degen_terms),
    .vertices_lit_o          (geom_light_vertices_lit_o),
    .degenerate_o            (geom_light_degenerate_o),
    .ndl_clamp_lo_o          (ls_clamp_lo),
    .ndl_clamp_hi_o          (ls_clamp_hi),
    .rgb_sat_o               (geom_light_rgb_sat_o),
    .cfg_refused_o           (geom_light_cfg_refused_o),
    .nlights_clamped_o       (geom_light_nlights_clamped_o),
    .seam_mismatch_o         (geom_light_seam_mismatch_o),
    .tag_mismatch_o          (geom_light_tag_mismatch_o),
    .root_queue_overflow_o   (geom_light_root_queue_overflow_o),
    .idle_o                  (ls_idle)
  );

  // THE FORK'S COST, COUNTED RATHER THAN ARGUED (I43). A cycle in which the
  // store is offering a vertex, GEOM.SKIN would take it and GEOM.SKIN.NORM
  // would not. It WAS large -- 217 over the smoke's 8 vertices, the normal path
  // then being a ~38-clock walk with a serial root against the skinner's
  // twelve -- and owner ruling R31 moved that root into GEOM.LIGHT, leaving
  // SKIN.NORM at six clocks a vertex. It stays a port so the rate is measured
  // in the composed machine instead of being asserted in a comment.
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) begin
      geom_sn_fork_stall_o <= '0;
    end else if (pal_o_valid && skin_v_ready && !sn_v_ready
                 && geom_sn_fork_stall_o != 32'hFFFF_FFFF) begin
      geom_sn_fork_stall_o <= geom_sn_fork_stall_o + 32'd1;
    end
  end

  // ==========================================================================
  // GEOM.REPLAY's NETS, declared ahead of their earliest reader. The block
  // itself is instantiated with the asset path at section 11, beside the
  // dispatcher fork that feeds it; GEOM.GROUP_SEQ, GEOM.CLIP and
  // GEOM.PROJ_LANE, which come first in this file, read these.
  // ==========================================================================
  // the dispatcher fork -> GEOM.GROUP_SEQ's job
  wire                    dsp_job_valid, gs_job_ready;
  wire [GEOM_INDEX_W-1:0] dsp_job_count;
  wire [1:0]              dsp_job_mask;
  wire [15:0]             dsp_job_src;
  // GEOM.GROUP_SEQ -> GEOM.REPLAY: the sealed handle, and the release back
  wire                    gs_grp_valid, rp_grp_ready, gs_grp_view;
  wire [GEOM_ARENA_W-1:0] gs_grp_arena, rp_rel_arena;
  wire [GEOM_GEN_W-1:0]   gs_grp_gen;
  wire                    gs_grp_poison;   // R31: the batch lost a record
  wire                    rp_rel_valid;
  // The handle's count and source id are not read: GEOM.REPLAY takes the
  // meshlet's vertex count from the dispatcher token, which is the SAME number
  // from the same handshake, so comparing the two would be a detector whose
  // operands one enable moves together.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [GEOM_INDEX_W-1:0] gs_grp_count;
  wire [15:0]             gs_grp_src_id;
  /* verilator lint_on UNUSEDSIGNAL */
  // GEOM.REPLAY <-> GEOM.PROJ_LANE: the lookups and the replies
  wire                    rp_look_valid, ln_look_ready;
  wire [GEOM_ARENA_W-1:0] rp_look_arena;
  wire [GEOM_GEN_W-1:0]   rp_look_gen;
  wire [GEOM_INDEX_W-1:0] rp_look_index;
  wire                    ln_rep_valid, ln_rep_hit, ln_rep_refuse;
  wire [GEOM_PAYLOAD_W-1:0] ln_rep_payload;
  // GEOM.REPLAY -> GEOM.CLIP: the triangle and its three attribute packets
  wire                    rp_o_valid, rp_o_ready;
  // ---- MATERIAL.RESOLVE's WINDOW (entry I49, 2026-09-20) ------------------
  // The window sits in this handshake, between GEOM.REPLAY and GEOM.CLIP. It
  // gates the VALID/READY pair only -- every data wire below still runs
  // straight from GEOM.REPLAY to GEOM.CLIP, so nothing is re-registered and no
  // second copy of a triangle exists. Declared here because `default_nettype
  // none` makes a net used before its declaration an error, and GEOM.CLIP is
  // instantiated some five thousand lines above the window.
  wire                    mw_t_valid, mw_t_ready;
  wire [31:0]             rp_o_material_set;
  wire [ 7:0]             rp_o_quality_tier;
  wire [15:0]             rp_o_material;
  // What the window PUBLISHES: the material-owned half of the 298-bit flat
  // request, plus the two binding witnesses the binding page's own legality
  // law fixes at zero for a direct format.
  wire                    mw_pub_valid;
  wire [ 1:0]             mw_pub_sample_count;
  wire [ 2:0]             mw_pub_material_recipe;
  wire [ 7:0]             mw_pub_recipe_weight;
  wire [ 7:0]             mw_pub_base_binding;
  wire [ 1:0]             mw_pub_response_class;
  wire signed [20:0]      rp_o_ax, rp_o_ay, rp_o_bx, rp_o_by, rp_o_cx, rp_o_cy;
  wire [2:0]              rp_o_behind;
  wire [15:0]             rp_o_src_id;
  wire [GEOM_CLIP_ATTRW-1:0] rp_attr_a, rp_attr_b, rp_attr_c;
  // The triangle's RASTER WORD (R28), declared here rather than beside
  // GEOM.REPLAY five thousand lines below because GEOM.CLIP -- which is
  // composed FIRST in this file -- is the consumer of its cull field, and a
  // net used before it is declared is an error under `default_nettype none`.
  /* verilator lint_off UNUSEDSIGNAL */
  // [31:2] is the MATERIAL's half, and R28 says in as many words that no bit of
  // it has a ratified consumer in v1 -- "so a v1 material writes 0 there". It
  // is carried rather than dropped because the word is the triangle's, whole:
  // the day a material format gives one of those bits a meaning, the value is
  // already here and only its reader is new.
  wire [31:0]             rp_o_raster;
  /* verilator lint_on UNUSEDSIGNAL */
  // GEOM.REPLAY -> GEOM.ASSETFETCH: the proven release (entry I38)
  wire                    rp_af_release;

  zhao_geom_group_seq #(
    .ARENAS      (GEOM_ARENAS),
    .DEPTH       (GEOM_DEPTH),
    .NVIEWS      (GEOM_NVIEWS),
    .GEN_W       (GEOM_GEN_W),
    .PAYLOAD_A_W (GEOM_PAY_A_W)
  ) u_geom_group_seq (
    .clk             (gpu_clk),
    .rst_n           (rst_n),

    // REAL: the job, from the meshlet dispatcher fork at section 11 -- the
    // meshlet's own vertex count and visible mask, in the SAME handshake that
    // offers it to GEOM.ASSEMBLE and GEOM.REPLAY. Entry I11, CLOSED.
    .job_valid_i     (dsp_job_valid),
    .job_ready_o     (gs_job_ready),
    .job_count_i     (dsp_job_count),
    .job_view_mask_i (dsp_job_mask),
    .job_src_id_i    (dsp_job_src),

    // REAL: from GEOM.SKIN.
    .v_valid_i (gs_v_valid),
    .v_ready_o (gs_v_ready),
    .v_x_i     (gs_v_x),
    .v_y_i     (gs_v_y),
    .v_z_i     (gs_v_z),
    // REAL: GEOM.VDECODE's refusal pulse, one per record it refused -- the
    // HOLE a batch must account for or wait for ever (owner ruling R31). The
    // same net leaves the module as `geom_vd_refused_o` for evidence.
    .hole_i    (geom_vd_refused_o),

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

    // REAL: the sealed handle into GEOM.REPLAY, and its release back. The
    // handle is GATED on GEOM.VATTR's verdict that every row and colour the
    // batch owes is in the store (va_done): the seal waits for LANDINGS, the
    // rows those landings owe are written ~45 clocks later, and REPLAY must
    // not read a row before it exists. Same gate on both sides of the handshake.
    .grp_valid_o (gs_grp_valid),
    .grp_ready_i (rp_grp_ready && va_done),
    .grp_arena_o (gs_grp_arena),
    .grp_gen_o   (gs_grp_gen),
    .grp_count_o (gs_grp_count),
    .grp_view_o  (gs_grp_view),
    .grp_src_id_o(gs_grp_src_id),
    .grp_poison_o(gs_grp_poison),
    .rel_valid_i (rp_rel_valid),
    .rel_arena_i (rp_rel_arena),

    .groups_opened_o     (geom_groups_opened_o),
    .groups_sealed_o     (geom_groups_sealed_o),
    .view_vertices_sent_o(geom_view_vertices_sent_o),
    .landings_o          (geom_landings_o),
    .jobs_refused_o      (geom_jobs_refused_o),
    .alloc_stall_cycles_o(geom_alloc_stall_cycles_o),
    .rel_unheld_o        (geom_rel_unheld_o),
    .holes_o             (geom_holes_o),
    .groups_poisoned_o   (geom_groups_poisoned_o),
    .holes_early_o       (geom_holes_early_o),
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

    // REAL: GEOM.REPLAY's triangle, corners and behind verdicts straight out
    // of the arena, the packets built at section 11 from GEOM.DEPTHQUANT's
    // invw24 and the attribute store's slots. Entry I24's triangle half and
    // the bench's triangle door are both GONE.
    // I49, 2026-09-20: the handshake is the MATERIAL WINDOW's, not
    // GEOM.REPLAY's. The window holds a triangle whose material is not the
    // published one until the span below has drained and the resolve has
    // answered. Every DATA wire beneath is still GEOM.REPLAY's own, unbuffered.
    .tri_valid_i  (mw_t_valid),
    .tri_ready_o  (mw_t_ready),
    .tri_ax_i     (rp_o_ax),
    .tri_ay_i     (rp_o_ay),
    .tri_bx_i     (rp_o_bx),
    .tri_by_i     (rp_o_by),
    .tri_cx_i     (rp_o_cx),
    .tri_cy_i     (rp_o_cy),
    .tri_behind_i (rp_o_behind),
    .tri_src_id_i (rp_o_src_id),
    .tri_attr_a_i (rp_attr_a),
    .tri_attr_b_i (rp_attr_b),
    .tri_attr_c_i (rp_attr_c),

    // REAL: the scissor is the console's own pass geometry (GLUE 1).
    .vp_x0_i      (12'd0),
    .vp_y0_i      (12'd0),
    .vp_w_i       (clip_vp_w_c),
    .vp_h_i       (clip_vp_h_c),
    // I24, CLOSED 2026-09-20 (owner rulings R28/R29). REAL: the cull mode is
    // bits [1:0] of THIS TRIANGLE'S OWN raster word, the one GEOM.REPLAY
    // presents beside the corners. It reached the triangle by travelling with
    // the meshlet from GEOM.DRAWJOB -- job, to meshlet, to triangle -- never on
    // a second path, which is why meshlet N cannot be culled by draw M's mode.
    // `zref::raster_state::cull_mode` is the same field selector.
    .cull_mode_i  (rp_o_raster[1:0]),

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
  // ---- THE REFERENCE FORK (R21): TWO CONSUMERS, EACH SERVED EXACTLY ONCE ---
  // The reference stream now has two customers -- the projector's replay shell
  // and the light lane -- and this is GLUE 6's AND-fork, not an AND of their
  // readies. The difference is not cosmetic and it was MEASURED: with the
  // producer's ready ANDed but each consumer seeing the raw `valid`, the replay
  // shell accepted the SAME reference on every cycle the producer held it
  // waiting for the slower lane. `proj_replay_triangles_o` went from 128 to
  // 18,244 for 128 triangles -- about 147 replays each, which is exactly the
  // lane's clocks per triangle. The output was identical every time, so nothing
  // downstream could see it; only the counter could (CLAUDE.md, "counters see
  // what pictures cannot").
  //
  // So each consumer's VALID is gated on the other's READY, both accept on the
  // same clock, and the producer's ready is the AND. NO CONSUMER'S READY READS
  // ITS OWN VALID: `tl_r_ready` is `st_q == S_IDLE` in the lane, a register,
  // and the shell's is its own arena state -- so nothing here closes a
  // combinational loop.
  wire                      ps_r_ready, tl_r_ready;
  wire                      ps_r_valid_c = ts_r_valid && tl_r_ready;
  wire                      tl_r_valid_c = ts_r_valid && ps_r_ready;
  assign ts_r_ready = ps_r_ready && tl_r_ready;
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

  // ---- TERRAIN.LODFEED, the page-load deviation pass (entry I18, ruling R70)
  // Its write port is MEASURE.HISTOGRAM's event ingress.  The three deviation
  // magnitudes are the v1 histogram metric; `w_slot_o` and `w_cy_o` are
  // `zhao_terrain_devstore`'s and that store is NOT composed here, so they are
  // waived AT the declaration rather than left as empty pins.
  //
  // WHY THE STORE IS ABSENT AND THIS IS NOT A HALF-COMPOSITION.  The store's
  // only reader is `zhao_terrain_lod`, which entry I21 still refuses, and ruling
  // R59 prices the store at ~77 M10K of 553.  Composing 77 M10K of RAM whose
  // every word is unread is the "BUILT, INSTALLED NOWHERE" shape CLAUDE.md
  // names, and it would buy nothing this pass: the histogram is a SECOND
  // consumer of the same records, not a substitute for the first.  When I21
  // closes, the store joins this stream and `w_ready_i` becomes the AND of the
  // two readies -- which is why the ready below is written as a named wire.
  wire                    tlf_w_valid, tlf_w_ready;
  wire [23:0]             tlf_w_dev1, tlf_w_dev2, tlf_w_dev3;
  wire [15:0]             tlf_w_src_id;
  /* verilator lint_off UNUSEDSIGNAL */
  // EVERY FIELD OF THE WRITE PORT THAT THE HISTOGRAM DOES NOT READ, waived
  // here in one place rather than one at a time, because the set is exactly
  // "what `zhao_terrain_devstore` would take" and it should read as one
  // absence and not five coincidences. The store keys on {slot, subpatch} and
  // holds the centre height and the invalidation; MEASURE.HISTOGRAM keys on
  // `src_id` and takes only the three magnitudes.
  wire [TERR_MEMSLOT-1:0] tlf_w_slot;        // the devstore's key (absent, above)
  wire [3:0]              tlf_w_sp;          // ... and its subpatch index
  wire signed [15:0]      tlf_w_cy;          // the devstore's centre height
  wire                    tlf_inv_valid;     // the devstore's invalidation strobe
  wire [TERR_MEMSLOT-1:0] tlf_inv_slot;      // ... and the slot it invalidates
  wire                    tlf_busy;
  // Four of the block's ten counters stop here rather than at this module's
  // edge, and the choice is stated because "which counters get exported" is
  // exactly the kind of decision that looks arbitrary later. `walk_clocks` is a
  // BUDGET input and the block's header says a pinned clock count goes stale
  // silently, so its home is the directed test that prints it. `lattices_seen`
  // is `walked + dropped` by construction. `dev_clipped`, `dev_vertices` and
  // `dev_lattice_reads` describe the WALK's interior, which is
  // `terrain_lodpath_directed`'s subject, not a console bench's. The four that
  // DO leave are the four a console-level reader needs to tell an empty
  // histogram's two causes apart.
  wire [31:0]             tlf_lattices_seen, tlf_surface1_samples;
  wire [31:0]             tlf_walk_clocks, tlf_dev_clipped;
  wire [31:0]             tlf_dev_vertices, tlf_dev_lattice_reads;
  /* verilator lint_on UNUSEDSIGNAL */

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

  // ---- item 14, second half: TERRAIN.HEIGHTTAP in front of the cache ------
  // TESS's read ports now pass THROUGH the tap (`htp_o_*`, its side) to the
  // compose cache (`htp_c_*`, the cache's side). The tap never gates TESS; it
  // borrows only cycles TESS does not request. Its client is PART.TERRAIN_TAP,
  // instantiated with the particles in item 14's first half.
  wire                htp_c_lat_req, htp_c_lat_surface, htp_c_cs_req;
  wire [5:0]          htp_c_lat_vi, htp_c_lat_vj;
  wire [4:0]          htp_c_cs_ci, htp_c_cs_cj;
  wire signed [31:0]  htp_o_lat_h, htp_o_lat_wx, htp_o_lat_wz;
  wire [1:0]          htp_o_cs_substance;

  // The invalidation: every event that can change what the compose cache
  // SERVES. See item 14's COHERENCE paragraph for why it errs wide.
  assign ptt_inval_c = tcc_fill_start || terr_cc_fill_done_o || terr_cc_serve_release_i ||
                       terr_cc_cs_we_i || tpc_pos_we;
  // THE PITCH THE SERVED LATTICE WAS PLACED AT -- HELD, NOT THE HEADER WIRE.
  //
  // The tap's own header asks for "the SAME net that drives
  // zhao_terrain_place.hdr_pitch_log2_i", and that request cannot be honoured
  // literally, which is worth a paragraph because the literal wiring LINTS
  // CLEAN AND IS WRONG. `thr_h_pitch_log2` is TERRAIN.HDRREAD's `h_pitch_log2_o`,
  // and that is `ok ? pitch : HDR_PITCH_REFUSE (127)` -- a value that is only
  // the pitch on the cycle a header is being presented. TERRAIN.PLACE takes it
  // on that handshake and holds it privately. A tap reading the wire directly
  // would see 127 on nearly every cycle and answer PITCH FAULT for a lattice
  // that is perfectly placed.
  //
  // So the pitch is held HERE, loaded by the one handshake TERRAIN.PLACE loads
  // it on (`hdr_valid` -- PLACE's `hdr_ready_o` is constant 1 by contract), and
  // only when it is a legal 1.3 pitch: a REFUSED header places nothing, so it
  // must not change the pitch the served lattice was placed at. It is the same
  // source of truth as PLACE's, taken on the same enable. An island has one
  // pitch (spec 1.3); if two islands ever alternate, the tap's `ud == vd == D`
  // check against the stored placement answers PLACEMENT FAULT, counted on
  // `terr_tap_faults_o`, rather than a quiet wrong height.
  //
  // Reset to 0 (1 m): before anything is placed nothing is served, so every
  // tap answers off-patch from the cache's poison whatever this holds.
  logic signed [7:0] ptt_pitch_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n)
      ptt_pitch_q <= 8'sd0;
    else if (thr_h_valid && (thr_h_pitch_log2 >= -8'sd1) && (thr_h_pitch_log2 <= 8'sd2))
      ptt_pitch_q <= thr_h_pitch_log2;
  end
  assign ptt_pitch_c = ptt_pitch_q;

  zhao_terrain_heighttap u_terrain_heighttap (
    .clk  (gpu_clk),
    .rst_n(rst_n),
    .pitch_log2_i(ptt_pitch_c),

    // REAL: PART.TERRAIN_TAP's cell fills.
    .req_valid_i  (htp_req_valid),
    .req_ready_o  (htp_req_ready),
    .req_x_i      (htp_req_x),
    .req_z_i      (htp_req_z),
    .req_surface_i(htp_req_surface),
    .rsp_valid_o    (htp_rsp_valid),
    .rsp_height_o   (htp_height),
    .rsp_no_ground_o(htp_rsp_no_ground),
    .rsp_nx_o(htp_nx),
    .rsp_ny_o(htp_ny),
    .rsp_nz_o(htp_nz),
    .rsp_h00_o (htp_h00),
    .rsp_h10_o (htp_h10),
    .rsp_h01_o (htp_h01),
    .rsp_h11_o (htp_h11),
    .rsp_wx00_o(htp_wx00),
    .rsp_wz00_o(htp_wz00),
    .rsp_sh_o  (htp_sh),
    .rsp_na_x_o(htp_na_x),
    .rsp_na_y_o(htp_na_y),
    .rsp_na_z_o(htp_na_z),
    .rsp_nb_x_o(htp_nb_x),
    .rsp_nb_y_o(htp_nb_y),
    .rsp_nb_z_o(htp_nb_z),

    // REAL: TERRAIN.TESS, the owner, passed straight through.
    .o_lat_req_i     (tt_lat_req),
    .o_lat_vi_i      (tt_lat_vi),
    .o_lat_vj_i      (tt_lat_vj),
    .o_lat_surface_i (tt_lat_surface),
    .o_lat_h_o       (htp_o_lat_h),
    .o_lat_wx_o      (htp_o_lat_wx),
    .o_lat_wz_o      (htp_o_lat_wz),
    .o_cs_req_i      (tt_cs_req),
    .o_cs_ci_i       (tt_cs_ci),
    .o_cs_cj_i       (tt_cs_cj),
    .o_cs_substance_o(htp_o_cs_substance),

    // REAL: the compose cache's read ports.
    .c_lat_req_o     (htp_c_lat_req),
    .c_lat_vi_o      (htp_c_lat_vi),
    .c_lat_vj_o      (htp_c_lat_vj),
    .c_lat_surface_o (htp_c_lat_surface),
    .c_lat_h_i       (tcc_lat_h),
    .c_lat_wx_i      (tcc_lat_wx),
    .c_lat_wz_i      (tcc_lat_wz),
    .c_cs_req_o      (htp_c_cs_req),
    .c_cs_ci_o       (htp_c_cs_ci),
    .c_cs_cj_o       (htp_c_cs_cj),
    .c_cs_substance_i(tcc_cs_substance),

    .taps_answered_o   (terr_tap_answered_o),
    .taps_void_o       (htp_void),
    .taps_off_patch_o  (terr_tap_off_patch_o),
    .place_mismatch_o  (htp_place_bad),
    .pitch_bad_o       (htp_pitch_bad),
    .interp_overflow_o (htp_ovf),
    .tap_stall_clocks_o(htp_stall),
    .normal_sats_o     (htp_nsat)
  );

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
    // From 2026-09-19 the answers come back THROUGH TERRAIN.HEIGHTTAP (item
    // 14), which passes them straight through with no register and no mux:
    // this block only reads the cycle after one it requested, and on every
    // such cycle the tap did not inject.
    .lat_req_o     (tt_lat_req),
    .lat_vi_o      (tt_lat_vi),
    .lat_vj_o      (tt_lat_vj),
    .lat_surface_o (tt_lat_surface),
    .lat_h_i       (htp_o_lat_h),
    .lat_wx_i      (htp_o_lat_wx),
    .lat_wz_i      (htp_o_lat_wz),
    .cs_req_o      (tt_cs_req),
    .cs_ci_o       (tt_cs_ci),
    .cs_cj_o       (tt_cs_cj),
    .cs_substance_i(htp_o_cs_substance),

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
  // has a `viewport_id` and not a rectangle.
  //
  // THE REASON THIS SENTENCE USED TO GIVE WAS FALSE, corrected 2026-09-20
  // (projinput). It said "the id-to-rectangle table is `spec/video_rules.md`'s
  // and is not in the ABI. Inventing that table here is precisely what this
  // file may not do." `video_rules.md` contained no such table -- and
  // `zref::render::viewports_of()` has contained it since 2026-08-15, so
  // lowering it would invent nothing. The table is now written down, in
  // `spec/video_rules.md` section 3.2, with the oracle named as the thing to
  // differential against. Entry I14 carries the full correction and what is
  // still owed; the host port stays until the lowering lands.
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
    // R21: the fork above. This shell sees a reference only when the light
    // lane can take it too, so it accepts each one exactly once.
    .ref_valid_i  (ps_r_valid_c),
    .ref_ready_o  (ps_r_ready),
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

  // ==========================================================================
  // TERRAIN's LIT NORMALS (owner ruling R21): TERRAIN.NORMALS and TERRAIN.SHADE
  // ==========================================================================
  // Both blocks have been BUILT and disconnected since they were written --
  // `zhao_terrain_normals` because the only ratified normal needed a triangle
  // and terrain had no triangle stage, `zhao_terrain_shade` because its sun had
  // no producer and its output had no customer. R5 asked for a third ModeTri
  // pass and terrain2 measured that it cannot fit (4,096 jobs x 456 clocks
  // against a 1,666,666-clock frame). R21 kept the GOAL and changed the MEANS:
  // compute the face normal at the REPLAY stage from a vertex store.
  //
  // `zhao_terrain_lightlane` is that store and that lane. It writes the WORLD
  // vertex on the projector's own fill beat -- the same {arena, index} the
  // arena is keyed by, R11's pattern, so no two streams are joined -- and reads
  // three rows per reference. The sun is `zhao_light_env`'s published direction
  // (R25's SetEnvironment, the value the bank already computes) rather than a
  // second implementation of 4a's law.
  //
  // WHAT IT COSTS, in clocks rather than in adjectives: `zhao_terrain_shade` is
  // 147 clocks per triangle with one in flight, so the lane's ready throttles
  // the reference stream and terrain replay runs at that rate. At the ruled
  // 2,000 terrain triangles per frame that is 294,000 of 1,666,666 clocks --
  // 17.6% -- and `tests/terrain/terrain_lightlane_directed.cpp` measures the
  // interval rather than asserting it.
  zhao_terrain_lightlane #(
    .ARENAS (PROJ_T_ARENAS),
    .DEPTH  (PROJ_T_DEPTH),
    .GEN_W  (GEOM_GEN_W),
    .SRCW   (16)
  ) u_terrain_lightlane (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: the fill beat is client B's accepted vertex -- the same handshake
    // that writes the projector's arena, so the store cannot hold a vertex the
    // arena does not.
    .fill_valid_i(ts_b_valid),
    .fill_ready_i(ts_b_ready),
    .fill_arena_i(ts_b_arena),
    .fill_index_i(ts_b_index),
    .fill_vx_i   (ts_b_vx),
    .fill_vy_i   (ts_b_vy),
    .fill_vz_i   (ts_b_vz),

    // REAL: the arena's lifetime, from the same sequencer and the same shell.
    .open_i      (ts_open),
    .open_arena_i(ts_open_arena),
    .open_gen_i  (ts_open_gen),

    // REAL: TERRAIN.GROUP_SEQ's reference stream, joined with the replay's.
    .ref_valid_i (tl_r_valid_c),
    .ref_ready_o (tl_r_ready),
    .ref_arena_i (ts_r_arena),
    .ref_gen_i   (ts_r_gen),
    .ref_ia_i    (ts_r_ia),
    .ref_ib_i    (ts_r_ib),
    .ref_ic_i    (ts_r_ic),
    .ref_src_id_i(ts_r_src_id),

    // REAL: the sun SetEnvironment loaded, published by GEOM.LIGHT.ENV.
    .sun_x_i(le_sun_x),
    .sun_y_i(le_sun_y),
    .sun_z_i(le_sun_z),

    // I13: the light leaves with the triangle it belongs to.
    .light_valid_o     (terr_light_valid_o),
    .light_ready_i     (terr_light_ready_i),
    .light_base_o      (terr_light_base_o),
    .light_degenerate_o(terr_light_degenerate_o),
    .light_src_id_o    (terr_light_src_id_o),

    .refs_taken_o       (terr_light_refs_taken_o),
    .lights_emitted_o   (terr_light_emitted_o),
    .stale_reads_o      (terr_light_stale_reads_o),
    .normals_evaluated_o(terr_light_normals_o),
    .triangles_shaded_o (terr_light_shaded_o),
    .degenerate_count_o (terr_light_degenerate_count_o),
    .base_sat_o         (terr_light_base_sat_o),
    .degen_mismatch_o   (terr_light_degen_mismatch_o),
    /* verilator lint_off PINCONNECTEMPTY */
    .idle_o             ()
    /* verilator lint_on PINCONNECTEMPTY */
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

    // (I12's arena origin is not owed in v1 -- owner ruling R27; the lane no
    // longer has the port pair.)

    // REAL: landings back to the sequencer.
    .fill_landed_o(ln_fill_landed),
    .fill_arena_o (ln_fill_arena),
    .fill_index_o (ln_fill_index),

    // REAL: GEOM.REPLAY's three lookups per triangle per view. Entry I12's
    // lookup half, CLOSED.
    .look_valid_i(rp_look_valid),
    .look_ready_o(ln_look_ready),
    .look_arena_i(rp_look_arena),
    .look_gen_i  (rp_look_gen),
    .look_index_i(rp_look_index),
    .rep_valid_o (ln_rep_valid),
    .rep_hit_o   (ln_rep_hit),
    .rep_refuse_o(ln_rep_refuse),
    .rep_payload_o(ln_rep_payload),

    .arena_hits_o    (geom_arena_hits_o),
    .arena_misses_o  (geom_arena_misses_o),
    .arena_refusals_o(geom_arena_refusals_o),
    .arena_overflow_o(geom_arena_overflow_o)
  );

  // ==========================================================================
  // COMPOSITOR. On the real video mode (glue 1), and IN the render path since
  // 2026-09-19: between the raster's drain and publication, through the shell's
  // post lease -- the I15/I16 closure record below says how.
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
  // The two widths are IN_LANES*32 and OUT_LANES*32 at the values the
  // instantiation below selects (13 and 7, owner ruling R40). They are
  // literals because a body localparam declared here would still have to agree
  // with the instance's parameter overrides; the host's own elaboration guards
  // are what refuse a disagreement, and a width mismatch on these wires is
  // caught as a WIDTH error by the linter rather than silently truncated.
  logic         sfa_req_valid, sfa_req_ready;
  logic [  2:0] sfa_req_slot;
  logic         sfa_req_noprog;
  logic [415:0] sfa_req_in;
  logic         sfa_resp_valid, sfa_resp_ready;
  logic [223:0] fld_resp_out_c;
  logic [  7:0] fld_resp_status_c;

  // ---- the FIELD engine's SECOND client, and the doorbell in front of it ---
  // Client 1, driven by `u_field_flow_adapter` (entry I5, owner ruling R40).
  logic         pfa_req_valid, pfa_req_ready;
  logic [  2:0] pfa_req_slot;
  logic         pfa_req_noprog;
  logic [415:0] pfa_req_in;
  logic         pfa_resp_valid, pfa_resp_ready;
  logic         pfa_ans_valid, pfa_fld_valid;
  logic signed [10:0] pfa_fld_ax, pfa_fld_ay, pfa_fld_az;
  logic         pfa_rec_take;

  // The doorbell's side of the loader and of both directory phases (entry I42,
  // owner ruling R43).
  logic        fdb_ld_valid, fdb_ld_ready;
  logic [ 1:0] fdb_ld_kind;
  logic [ 2:0] fdb_ld_slot;
  logic [ 6:0] fdb_ld_addr;
  logic [95:0] fdb_ld_data;
  logic        fdb_lu_valid, fdb_lu_ready;
  logic [31:0] fdb_lu_hash;
  logic        fdb_lu_resp_valid, fdb_lu_resp_ready, fdb_lu_hit;
  logic [ 2:0] fdb_lu_slot;
  logic        fdb_cm_valid, fdb_cm_ready;
  logic [31:0] fdb_cm_hash;
  logic        fdb_cm_ok;
  logic        fdb_cm_resp_valid, fdb_cm_resp_ready;
  logic        fdb_cm_inserted, fdb_cm_evicted;
  logic [ 2:0] fdb_cm_slot;

  logic                   atm_req_v_c;
  logic [POST_XW-1:0]     atm_req_x_c;
  logic [POST_YW-1:0]     atm_req_y_c;
  logic                   atm_en_c;
  logic                   atm_valid_c;
  logic [15:0]            atm_rgb_c;
  logic [7:0]             atm_opacity_c;
  logic                   atm_add_c;

  // ==========================================================================
  // I15 AND I16 ARE CLOSED (2026-09-19). What they were, and what closed them.
  // ==========================================================================
  // I15 was "POST.COMPOSITE's SOURCE PIXELS (`post_s_*`) -- BOUNDARY, and this
  // is the largest honest gap in the file". Its last refusal named three
  // missing things; each is now a block or a law, and none is a tie-off moved:
  //   1. A RASTER-ORDER READ MASTER on the BACK buffer, re-armed on render
  //      drain: `fpga/rtl/compositor/zhao_post_fbread.sv` (41 directed checks,
  //      tests/compositor/post_fbread_directed.cpp).
  //   2. A MEMORY IDENTITY. Not a new client: ENGINE0, the render engine's own,
  //      under the render lease -- POST.COMPOSITE.md's "exclusive framebuffer
  //      read/write lease after resolve and before publication" is the render
  //      lease extended past the raster. `zhao_mem_guard` gains ENGINE0's READ
  //      arm inside the leased window (lease-gated) and POST.ECHO's capture
  //      WRITE arm; `mem_guard_no_escape` re-proven with both (bmc + every
  //      cover, including one per ENGINE0 arm), and a scratch mutant that drops
  //      the capture's lease term makes a1_echo_lease and a1_region FAIL.
  //      Client 5 stays unspent (T3).
  //   3. A FRAME-COMPLETION EVENT: `zhao_post_lease` arms on the render frame's
  //      `frame_end` and starts only when the bin pipe is quiet, no raster
  //      pixel is on offer and RASTER.FBWRITE has RETIRED every word.
  // I16 was the output end and the echo tap. The output is written back IN
  // PLACE through the SAME RASTER.FBWRITE (switched at the phase change, so
  // there is still exactly one producer on it at a time -- the objection I16
  // recorded), and the tap feeds POST.ECHO (`zhao_post_echo.sv`, owner ruling
  // R7, 33 directed checks against `zref::post::echo`). The smoke bench shows
  // the value traverse: the capture equals the framebuffer word for word.
  // ---- THE POST LEASE's seam (I15/I16, 2026-09-19) --------------------------
  // The shell's `zhao_post_lease` owns the pass: it starts it when the raster
  // has drained, names the view, reads the back buffer in raster order into
  // `s_*`, writes `o_*` back through RASTER.FBWRITE in place, and hands the
  // `echo_*` tap -- qualified by the output handshake -- to POST.ECHO. Every
  // connection below is a port to a port of the same width and meaning.
  logic                   post_pass_start_c;
  logic                   post_view_c;
  logic                   post_s_valid_c, post_s_ready_c;
  logic [15:0]            post_s_rgb_c;
  logic                   post_o_valid_c, post_o_ready_c, post_o_last_c;
  logic [15:0]            post_o_rgb_c;
  logic [POST_XW-1:0]     post_o_x_c;
  logic [POST_YW-1:0]     post_o_y_c;
  logic                   post_echo_valid_c;
  logic [15:0]            post_echo_rgb_c;

  // ---- R35/R36: THE LOOK, from CMD.EXEC (section 7c) --------------------------
  // SetPost's look, SetGradeTable's product vectors, POST.ECHO's arm and the
  // pass-start hold, declared here because the compositor and the shell's post
  // lease consume them before CMD.EXEC's instance appears in this file.
  logic [7:0]        post_look_bloom_gain_w, post_look_flash_amt_w;
  logic              post_look_grade_valid_w, post_look_echo_arm_w, post_look_hold_w;
  logic              post_look_pv_we_w;
  logic [1:0]        post_look_pv_sel_w;
  logic [5:0]        post_look_pv_addr_w;
  logic [71:0]       post_look_pv_data_w;
  logic signed [8:0] post_look_bias_r_w, post_look_bias_g_w, post_look_bias_b_w;
  logic [15:0]       post_look_flash_rgb_w, post_look_ink_rgb_w;

  zhao_post_composite #(
    .LINE_W    (POST_LINE_W),
    .MAX_H     (POST_MAX_H),
    .NLINE     (POST_NLINE),
    .LAG_LINES (POST_LAG_LINES),
    .LAG_PX    (POST_LAG_PX)
  ) u_post_composite (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: the PASS is the post lease's, not the console tick. It starts only
    // after the raster has drained into the back buffer (I15, closed
    // 2026-09-19); the geometry is the shell's latched mode.
    .frame_start_i(post_pass_start_c),
    .frame_w_i    (post_frame_w_c),
    .frame_h_i    (post_frame_h_c),
    .view_sel_i   (post_view_c),

    // REAL (I15): the COMPLETED back buffer, read back in raster order by
    // `zhao_post_fbread` inside the shell's lease. The order this port counts
    // is the order the reader walks; the reorder buffer is the framebuffer.
    .s_valid_i(post_s_valid_c),
    .s_ready_o(post_s_ready_c),
    .s_rgb_i  (post_s_rgb_c),

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
    // REAL (R36): the look and the grading table, from CMD.EXEC's committed
    // SetPost / SetGradeTable. Applied only while the post lease is idle.
    .bloom_gain_i(post_look_bloom_gain_w),
    .grade_valid_i(post_look_grade_valid_w),
    .pv_we_i     (post_look_pv_we_w),
    .pv_sel_i    (post_look_pv_sel_w),
    .pv_addr_i   (post_look_pv_addr_w),
    .pv_data_i   (post_look_pv_data_w),
    .bias_r_i    (post_look_bias_r_w),
    .bias_g_i    (post_look_bias_g_w),
    .bias_b_i    (post_look_bias_b_w),
    .flash_rgb_i (post_look_flash_rgb_w),
    .flash_amt_i (post_look_flash_amt_w),
    .ink_rgb_i   (post_look_ink_rgb_w),
    .hud_req_v_o (post_hud_req_v_o),
    .hud_req_x_o (post_hud_req_x_o),
    .hud_req_y_o (post_hud_req_y_o),
    .hud_valid_i (post_hud_valid_i),
    .hud_rgb_i   (post_hud_rgb_i),

    // REAL (I16): written back IN PLACE through the shell's RASTER.FBWRITE --
    // the same engine that wrote the raster, switched at the phase change --
    // and the echo tap feeds POST.ECHO (ruling R7) beside it.
    .o_valid_o(post_o_valid_c),
    .o_ready_i(post_o_ready_c),
    .o_rgb_o  (post_o_rgb_c),
    .o_x_o    (post_o_x_c),
    .o_y_o    (post_o_y_c),
    .o_last_o (post_o_last_c),
    .echo_valid_o(post_echo_valid_c),
    .echo_rgb_o  (post_echo_rgb_c),

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
  // 7b-iii. HOST.REGWIN -- the HPS register aperture and its two tenants.
  //         Owner ruling R51.  Added 2026-09-20.  THIS CLOSES ENTRY I19 AND
  //         THE READOUT HALF OF ENTRY I45.
  // ==========================================================================
  // Both entries asked for the same thing in the same words. I19: "the host is
  // the HPS; no register path from HPS to this block exists." I45, of the trace
  // ring's drain: "the same sentence I19 writes about MEASURE.HISTOGRAM applies
  // unchanged ... Closing one closes both, and they should be closed together
  // rather than twice." So one carrier, two tenants, one pass.
  //
  // WHY THIS IS A CARRIER AND NOT A RELOCATED TIE-OFF, stated plainly because
  // that is the accusation this composition has to answer. `hostreg_*` leaves
  // this module, so the question is whether the gap simply moved outward one
  // port. It did not, and the test is whether the far end is a REAL THING or a
  // thing nobody has built:
  //
  //   * the far end is the Cyclone V HPS's lightweight bridge, a port of the
  //     part. It is the same class of edge as `hps_req_*` (the h2f DATA bridge,
  //     which `zhao_hps_bridge` has driven since plan D10) and as
  //     `pad_buttons_i`. Neither of those is a register entry either;
  //   * in Verilator the harness IS the HPS, exactly as it is for the burst
  //     bridge and the FRAME_RING view, and the smoke bench drives this port as
  //     the host -- reading a histogram bin and a trace word back through it;
  //   * nothing inside this module is invented to make it look driven. An
  //     aperture nobody accesses simply answers nothing, and every counter it
  //     owns reads zero honestly.
  //
  // THE MAP IS FROZEN IN `spec/memory_rules.md` SECTION 8 and is bit-sliced,
  // not compared: tenant = addr[15:12], word offset = addr[11:2]. A tenant
  // cannot be handed an address outside its own 4 KiB region because the offset
  // wire is only ten bits wide. Sixteen regions exist; two are populated and
  // fourteen are refused and counted.
  logic [1:0]  hw_sel_c;
  logic [9:0]  hw_woff_c;
  logic        hw_write_c;
  logic [31:0] hw_wdata_c;
  logic [1:0]  hw_ack_c;
  logic [1:0]  hw_rvalid_c;
  logic [63:0] hw_rdata_c;
  logic [1:0]  hw_err_c;

  // tenant 0 <-> MEASURE.HISTOGRAM
  logic                 hrh_rd_valid_c;
  logic [HIST_BINW-1:0] hrh_rd_bin_c;
  logic                 hrh_rd_ready_c;
  logic                 hrh_rd_data_valid_c;
  logic [HIST_CW-1:0]   hrh_rd_count_c;

  // tenant 1 <-> DEBUG.TRACE
  logic [ 8:0] hrt_rd_addr_c;
  logic [31:0] hrt_rd_data_c;

  // R52: CMD.EXEC's lowering of DebugTraceArm 0xF003 into the ring.
  logic       cx_trace_arm_we_c;
  logic [6:0] cx_trace_arm_mask_c;
  logic       cx_trace_clear_c;

  zhao_host_regwin #(
    .NTENANT    (2),
    .AW         (16),
    .TENANT_LSB (12),
    .CW         (32),
    .ACK_LIMIT  (255)
  ) u_hostreg (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    .h_valid_i (hostreg_valid_i),
    .h_write_i (hostreg_write_i),
    .h_addr_i  (hostreg_addr_i),
    .h_wdata_i (hostreg_wdata_i),
    .h_ready_o (hostreg_ready_o),
    .h_rvalid_o(hostreg_rvalid_o),
    .h_rdata_o (hostreg_rdata_o),
    .h_err_o   (hostreg_err_o),

    .t_sel_o   (hw_sel_c),
    .t_woff_o  (hw_woff_c),
    .t_write_o (hw_write_c),
    .t_wdata_o (hw_wdata_c),
    .t_ack_i   (hw_ack_c),
    .t_rvalid_i(hw_rvalid_c),
    .t_rdata_i (hw_rdata_c),
    .t_err_i   (hw_err_c),

    .reads_o              (hostreg_reads_o),
    .writes_o             (hostreg_writes_o),
    .refused_unmapped_o   (hostreg_refused_unmapped_o),
    .refused_misaligned_o (hostreg_refused_misaligned_o),
    .refused_tenant_o     (hostreg_refused_tenant_o),
    .refused_timeout_o    (hostreg_refused_timeout_o),
    .stall_cycles_o       (hostreg_stall_cycles_o)
  );

  // Tenant 0: the histogram. Word offset IS the bin.
  zhao_host_reg_hist #(
    .OFFW (10),
    .BINW (HIST_BINW),
    .CW   (HIST_CW)
  ) u_hostreg_hist (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    .sel_i   (hw_sel_c[0]),
    .woff_i  (hw_woff_c),
    .write_i (hw_write_c),
    .ack_o   (hw_ack_c[0]),
    .rvalid_o(hw_rvalid_c[0]),
    .rdata_o (hw_rdata_c[31:0]),
    .err_o   (hw_err_c[0]),

    .rd_valid_o     (hrh_rd_valid_c),
    .rd_bin_o       (hrh_rd_bin_c),
    .rd_ready_i     (hrh_rd_ready_c),
    .rd_data_valid_i(hrh_rd_data_valid_c),
    .rd_count_i     (hrh_rd_count_c),

    .snap_valid_i   (hist_snap_valid_o),
    .snap_total_i   (hist_snap_total_o),
    .snap_src_id_i  (hist_snap_src_id_o),
    .snap_index_i   (hist_snap_index_o),
    .events_i       (hist_events_o),
    .updates_i      (hist_updates_o),
    .stall_cycles_i (hist_stall_cycles_o),
    .bin_sat_i      (hist_bin_sat_o),
    .fwd_hits_i     (hist_fwd_hits_o),
    .host_conflict_i(hist_host_conflict_o),
    .snapshots_i    (hist_snapshots_o),
    .frozen_write_i (hist_frozen_write_o)
  );

  // Tenant 1: the trace ring. Word offset IS {event, word}.
  zhao_host_reg_trace #(
    .OFFW  (10),
    .DEPTH (64)
  ) u_hostreg_trace (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    .sel_i   (hw_sel_c[1]),
    .woff_i  (hw_woff_c),
    .write_i (hw_write_c),
    .ack_o   (hw_ack_c[1]),
    .rvalid_o(hw_rvalid_c[1]),
    .rdata_o (hw_rdata_c[63:32]),
    .err_o   (hw_err_c[1]),

    .rd_addr_o (hrt_rd_addr_c),
    .rd_data_i (hrt_rd_data_c),
    .armed_i   (dbg_trace_armed_o),
    .count_i   (dbg_trace_count_o),
    .dropped_i (dbg_trace_dropped_o)
  );

  // `hw_wdata_c` reaches both tenants and both REFUSE a write (the aperture's
  // header says why: arming is DebugTraceArm's, ruling R52/R18). Declared
  // unused rather than deleted, because the wire is the aperture's contract
  // with a third tenant that will want it.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [31:0] hostreg_wdata_unused = hw_wdata_c;
  /* verilator lint_on UNUSEDSIGNAL */

  // ==========================================================================
  // MEASURE. The INTERVAL is the console's real frame (glue 2), and as of
  // 2026-09-20 THE EVENTS ARE REAL TOO -- entry I18, owner ruling R70.
  // ==========================================================================
  // THE V1 METRIC IS THE TERRAIN PAGE-LOAD LOD DEVIATION. Ruling R70 ratified
  // it and `spec/measure_rules.md` section 3 is the written definition, which is
  // the ruling's first requirement: the metric is a DOCUMENT, so that the next
  // person wiring something into this port has a sentence to check it against
  // rather than a precedent to copy.
  //
  // THE WIDTH ADAPTATION, DECLARED HERE BECAUSE THIS IS WHERE IT IS DONE
  // (ruling R70's second requirement). `zhao_terrain_loddev` emits three 24-bit
  // unsigned magnitudes; this block takes LANES=4 of EW=32. Lanes 0..2 carry
  // dev1/dev2/dev3 ZERO-EXTENDED 24 -> 32 and lane 3's `lane_valid` stays LOW,
  // so lane 3 contributes no event rather than contributing a zero one.
  //
  // WHY ZERO-EXTENSION IS AN ADAPTATION AND NOT AN INVENTION. The block bins by
  // `log2` of the magnitude, so widening an UNSIGNED value by leading zeros
  // moves no bin: the bin index of x is the position of its top set bit, and
  // zero-extension adds no set bits. It changes the RANGE the histogram could
  // represent, never the bucket any actual value lands in. Had the adaptation
  // been a shift or a truncation it would have been a law and would have needed
  // a ruling of its own.
  //
  // THE SOURCE ID IS THE WALK'S, NOT THE FILL'S, and `zhao_terrain_lodfeed`'s
  // `w_src_id_o` port comment carries the argument. Taking the id of whatever
  // page is currently streaming would attribute page A's deviations to page B
  // on every drop, and this block is metric-agnostic by design, so nothing
  // downstream could ever see it.
  // IT IS A SIZE CAST AND NOT A `{(HIST_EW-24){1'b0}}` REPLICATION, and that
  // is not style. The replication was written first and it made the guard
  // below UNREACHABLE: at HIST_EW < 24 the width expression underflows to a
  // huge unsigned replication count and elaboration dies inside the
  // concatenation -- measured, `verilator --lint-only -GEW=16` returns
  // "Internal Error: V3Number.h:242 `num` member accessed when data type is
  // UNINITIALIZED" -- so the `$fatal` that exists to explain that exact
  // mistake could never be reached to explain it. A guard the broken case
  // cannot reach is CLAUDE.md's detector-that-cannot-fire with the fault one
  // line away from it.
  //
  // `HIST_EW'(x)` zero-extends an unsigned value and cannot underflow, so the
  // guard below is now the thing that speaks when the width is wrong. (A size
  // cast is legal in Quartus 17.0; what that tool rejects is unary minus in
  // front of one, `-W'(x)`, which is not this.)
  wire [HIST_LANES*HIST_EW-1:0] hist_ev_err_c =
      { {HIST_EW{1'b0}},                                 // lane 3: not valid
        HIST_EW'(tlf_w_dev3),                            // lane 2
        HIST_EW'(tlf_w_dev2),                            // lane 1
        HIST_EW'(tlf_w_dev1) };                          // lane 0
  wire                    hist_ev_ready_c;

  // THE ADAPTATION ABOVE IS ONLY TRUE AT THESE WIDTHS, so it is GUARDED rather
  // than commented. A LANES of anything but 4 makes `4'b0111` name the wrong
  // lanes silently, and an EW below 24 makes the concatenation truncate a
  // deviation -- which reads LOW, the flattering direction, and would be
  // invisible in a histogram that has no idea what it is counting.
  // `initial begin ... end` and not a module-scope `if`: Quartus 17.0 rejects
  // the latter (CLAUDE.md), and `--lint-only` does not run this, which is why
  // it is here AND the shape is checked by the syntax gate.
  // synthesis translate_off
  initial begin
    if (HIST_LANES != 4)
      $fatal(1, "zhao_console_core: MEASURE.HISTOGRAM's R70 event mapping assumes LANES=4, got %0d", HIST_LANES);
    if (HIST_EW < 24)
      $fatal(1, "zhao_console_core: MEASURE.HISTOGRAM's EW=%0d truncates a 24-bit LOD deviation", HIST_EW);
  end
  // synthesis translate_on

  // TERRAIN.LODFEED's write port is accepted when the histogram accepts. When
  // entry I21 closes and `zhao_terrain_devstore` joins this stream, this
  // becomes the AND of the two readies and NOTHING ELSE CHANGES -- which is
  // why it is a named wire and not the port connection itself.
  assign tlf_w_ready = hist_ev_ready_c;

  zhao_measure_histogram #(
    .EW       (HIST_EW),
    .SUB_BITS (HIST_SUB_BITS),
    .LANES    (HIST_LANES),
    .CW       (HIST_CW)
  ) u_measure_histogram (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: TERRAIN.LODFEED's deviation records, one event per subpatch.
    .ev_valid_i     (tlf_w_valid),
    .ev_lane_valid_i(4'b0111),
    .ev_err_i       (hist_ev_err_c),
    .ev_src_id_i    (tlf_w_src_id),
    .ev_ready_o     (hist_ev_ready_c),

    // REAL: one measurement interval per console frame.
    .snapshot_i(core_tick_c),

    // R51, 2026-09-20: entry I19 is CLOSED. The HPS register path exists and
    // this is its far end -- `u_hostreg_hist` is tenant 0 of the aperture and
    // the word offset IS the bin index, so a read carries its own bin and
    // nothing is stateful between accesses.
    .rd_valid_i     (hrh_rd_valid_c),
    .rd_bin_i       (hrh_rd_bin_c),
    .rd_ready_o     (hrh_rd_ready_c),
    .rd_data_valid_o(hrh_rd_data_valid_c),
    .rd_count_o     (hrh_rd_count_c),

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

  // ==========================================================================
  // SURFACE.DISPATCH -- entry I30's OPEN HALF, CLOSED 2026-09-19 (ruling R45).
  // ==========================================================================
  // The paragraph above used to end "an executor-issued stamp rides the host's
  // envelope and policy", and that was honest and was a gap: the envelope had
  // no owner anywhere in the tree (entry I27 said so) and the policy was three
  // bits no opcode carries. R45 gave both an owner. `u_surface_dispatch`
  // resolves the patch from the stamp's OWN translation by the world->patch
  // law `zhao_terrain_heighttap` inverts -- powers-of-two pitch, arithmetic
  // shift, no divider, floor -- and emits the patch rectangle plus the
  // directory key {patch_ix, patch_iz}. NO SECOND MAPPING LAW EXISTS: this is
  // the same shift, on the same live pitch (`ptt_pitch_c`, from
  // TERRAIN.HDRREAD's staged header) that the tap uses.
  //
  // It is COMBINATIONAL from the merged command's translation, so the envelope
  // and the command it belongs to cannot be a cycle apart -- the join that
  // would otherwise have to be argued about is not a join at all.
  logic signed [31:0] sd_env_x0_c, sd_env_z0_c, sd_env_x1_c, sd_env_z1_c;
  logic               sd_blend_en_c;
  logic [2:0]         sd_blend_c, sd_age_shift_c;
  /* verilator lint_off UNUSEDSIGNAL */
  // The directory key leaves this module as evidence; nothing inside it keys
  // on a patch yet, and inventing a consumer would be worse than saying so.
  logic               sd_patch_valid_c;
  /* verilator lint_on UNUSEDSIGNAL */

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
  // R45: the policy WAS surf_cmd_field_en_i && sfa_arm_ready, and the host
  // half of that AND carried no information the console did not already have.
  // A stamp uses the brush exactly when a stamp program is resident.
  assign surf_field_en_c = sfa_arm_ready;

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

  zhao_surface_dispatch u_surface_dispatch (
    .clk   (gpu_clk),
    .rst_n (rst_n),
    .pitch_log2_i (ptt_pitch_c),
    .cmd_tx_i     (surf_cmd_tx_m),
    .cmd_ty_i     (surf_cmd_ty_m),
    .cmd_fire_i   (surf_cmd_valid_m && surf_cmd_ready_int),
    .env_x0_o     (sd_env_x0_c),

    .env_z0_o     (sd_env_z0_c),
    .env_x1_o     (sd_env_x1_c),
    .env_z1_o     (sd_env_z1_c),
    .patch_ix_o   (surf_disp_patch_ix_o),
    .patch_iz_o   (surf_disp_patch_iz_o),
    .patch_valid_o(sd_patch_valid_c),
    .blend_en_o   (sd_blend_en_c),
    .blend_o      (sd_blend_c),
    .age_shift_o  (sd_age_shift_c),
    .dispatched_o   (surf_disp_dispatched_o),
    .pitch_refused_o(surf_disp_pitch_refused_o),
    .env_clamped_o  (surf_disp_env_clamped_o)
  );

  assign surf_disp_env_x0_o = sd_env_x0_c;
  assign surf_disp_env_x1_o = sd_env_x1_c;

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
    // REAL (I30 closed, R45): the envelope is the patch the stamp's own
    // translation lands on, by the world->patch law, and the policy is the
    // dispatch's three named constants.
    .cmd_env_x0_i    (sd_env_x0_c),
    .cmd_env_z0_i    (sd_env_z0_c),
    .cmd_env_x1_i    (sd_env_x1_c),
    .cmd_env_z1_i    (sd_env_z1_c),
    .cmd_blend_en_i  (sd_blend_en_c),
    .cmd_blend_i     (sd_blend_c),
    .cmd_age_shift_i (sd_age_shift_c),
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

  // ==========================================================================
  // MEM.UPLOAD -- the HPS->VRAM resource upload engine, on the shell's
  // TERRAIN.BUILD socket (2026-09-19, cmdmem packet; owner rulings R4, R17)
  // ==========================================================================
  // It was "composable nowhere" (entry I20's first seam) because both HPS
  // arbiters were full. Ruling R4 widened the arbiter; the shell now runs the
  // N-client core with a socket below its two historical clients, and this is
  // that socket's first client -- through the REAL `zhao_hps_bridge` and the
  // REAL `zhao_mem_guard` / `zhao_vram_arbiter` slot 6, with retirement read
  // off the arbiter's own credit stream. Nothing between them is invented here.
  //
  // Its REQUEST is CMD.EXEC's lowering of the ratified `PublishResource`
  // 0x0030 (owner ruling R17): every field off the generated offsets, through a
  // pending queue in CMD.EXEC that outlives the packet's commit, so a frame's
  // draws are never held behind a background copy. Its PUBLICATION is
  // `spec/memory_rules.md` 5f.1's row. (Entry I47 carried this request as a
  // boundary for one commit, and is closed and deleted.)
  logic        cmd_upl_valid, cmd_upl_ready;
  // I41, CLOSED: CMD.EXEC's DrawForm arm, now an INTERNAL stream into
  // GEOM.DRAWJOB rather than nine ports at the console's edge.
  logic        cmd_draw_valid_w, cmd_draw_ready_w;
  logic [31:0] cmd_draw_form_w, cmd_draw_material_set_w, cmd_draw_transform_w;
  logic [ 7:0] cmd_draw_viewport_mask_w, cmd_draw_semantic_weight_w;
  logic [15:0] cmd_draw_flags_w, cmd_draw_src_id_w;
  logic [23:0] cmd_upl_index;
  logic [ 7:0] cmd_upl_kind, cmd_upl_slot;
  logic [63:0] cmd_upl_hps;
  logic [31:0] cmd_upl_vram, cmd_upl_len, cmd_upl_crc;
  logic [15:0] cmd_upl_epoch, cmd_upl_gen;
  zhao_guard_req_t         upl_guard_req;
  zhao_guard_rsp_t         upl_guard_rsp;
  logic [63:0]             upl_wdata;
  logic                    upl_wvalid, upl_wready, upl_wlast;
  logic [7:0]              upl_retire_words;
  zhao_hps_burst_req_t [0:0] upl_hps_req;
  logic                [0:0] upl_hps_grant;
  zhao_hps_burst_rsp_t [0:0] upl_hps_rsp;
  // ---- THE TERRAIN.BUILD SOCKET's UPSTREAM SHARE (entry I26, CLOSED) -------
  // The shell exposes ONE slot-6 guard client and says "upstream sharing of
  // the one guard port is the composer's". It has three customers here:
  //   0: MEM.UPLOAD            -- writes (a resource into R32's region)
  //   1: TERRAIN.PAGELOADER    -- writes (a page into TERRAIN.PAGE_POOL)
  //   2: `u_terrain_rdshare`   -- reads  (HDRREAD, PAGESTREAM, WRITEBACK)
  // A readers-only share cannot hold two WRITERS: it would let their data
  // words interleave in slot 6's one queue, and hand every requester every
  // other requester's retirement credits -- MEM.UPLOAD would count a page
  // load's retirement as its own and publish a mapping to bytes in flight.
  // `zhao_mem_share_wr` is `zhao_mem_share_n` with those two channels added
  // (write-data ownership, an in-order retirement ledger), proved in
  // tests/memory/mem_share_wr_directed.cpp. All three already carried
  // ZHAO_CLIENT_TERRAIN_BUILD's privilege; the share makes that identity
  // TRUSTED rather than claimed. The rotation's bound is N-1: a page load waits
  // at most for one upload request and one read.
  zhao_guard_req_t         tpl_g_req;
  zhao_guard_rsp_t         tpl_g_rsp;
  logic [63:0]             tpl_g_wdata;
  logic                    tpl_g_wvalid, tpl_g_wready, tpl_g_wlast;
  zhao_guard_req_t         trs_m_req;
  zhao_guard_rsp_t         trs_m_rsp;
  logic                    trs_m_beat_valid, trs_m_beat_last;
  logic [63:0]             trs_m_beat_data;

  zhao_guard_req_t         bsk_req;
  zhao_guard_rsp_t         bsk_rsp;
  logic [63:0]             bsk_wdata;
  logic                    bsk_wvalid, bsk_wready, bsk_wlast;
  logic [7:0]              bsk_credits;
  logic                    bsk_beat_valid, bsk_beat_last;
  logic [63:0]             bsk_beat_data;

  zhao_guard_req_t [2:0]       bs_req;
  zhao_guard_rsp_t [2:0]       bs_rsp;
  logic            [63:0]      bs_beat_data;
  logic            [2:0][63:0] bs_wdata;
  logic            [2:0]       bs_wvalid, bs_wlast;
  /* verilator lint_off UNUSEDSIGNAL */
  // Beats for requesters 0 and 1 (the two writers never read, so the share
  // never routes them one), requester 2's write ready (the read share never
  // writes), and the two retirement streams nobody consumes -- see below.
  logic            [2:0]       bs_beat_valid, bs_beat_last;
  logic            [2:0]       bs_wready;
  logic            [2:0][7:0]  bs_retire;
  // Per-requester job counts, the share's own denial/short/long/unowned and
  // ledger counters, and the retirement streams of the two requesters that do
  // not consume one: TERRAIN.PAGELOADER publishes on its last beat's
  // acceptance (the pool is read back through the SAME client, so the
  // arbiter's per-slot order already puts its reads after its writes), and a
  // read's retirement means nothing to a reader. All sunk here, named.
  logic            [2:0][31:0] bs_jobs;
  logic [31:0]                 bs_denied, bs_short, bs_long, bs_unowned, bs_ledger_full;
  /* verilator lint_on UNUSEDSIGNAL */

  assign bs_req[0]      = upl_guard_req;
  assign bs_req[1]      = tpl_g_req;
  assign bs_req[2]      = trs_m_req;
  assign upl_guard_rsp  = bs_rsp[0];
  assign tpl_g_rsp      = bs_rsp[1];
  assign trs_m_rsp      = bs_rsp[2];
  assign bs_wdata       = {64'd0, tpl_g_wdata, upl_wdata};
  assign bs_wvalid      = {1'b0, tpl_g_wvalid, upl_wvalid};
  assign bs_wlast       = {1'b0, tpl_g_wlast, upl_wlast};
  assign upl_wready     = bs_wready[0];
  assign tpl_g_wready   = bs_wready[1];
  assign upl_retire_words = bs_retire[0];
  assign trs_m_beat_valid = bs_beat_valid[2];
  assign trs_m_beat_last  = bs_beat_last[2];
  assign trs_m_beat_data  = bs_beat_data;

  zhao_mem_share_wr #(
    .N         (3),
    .CLIENT_ID (6),        // ZHAO_CLIENT_TERRAIN_BUILD -- see zhao_pkg
    .RQ        (4)
  ) u_build_share (
    .clk             (gpu_clk),
    .rst_n           (rst_n),
    .req_i           (bs_req),
    .rsp_o           (bs_rsp),
    .beat_valid_o    (bs_beat_valid),
    .beat_data_o     (bs_beat_data),
    .beat_last_o     (bs_beat_last),
    .wdata_i         (bs_wdata),
    .wvalid_i        (bs_wvalid),
    .wlast_i         (bs_wlast),
    .wready_o        (bs_wready),
    .retire_o        (bs_retire),
    .m_req_o         (bsk_req),
    .m_rsp_i         (bsk_rsp),
    .m_beat_valid_i  (bsk_beat_valid),
    .m_beat_data_i   (bsk_beat_data),
    .m_beat_last_i   (bsk_beat_last),
    .m_wdata_o       (bsk_wdata),
    .m_wvalid_o      (bsk_wvalid),
    .m_wlast_o       (bsk_wlast),
    .m_wready_i      (bsk_wready),
    .m_credits_i     (bsk_credits),
    .jobs_o          (bs_jobs),
    .denied_o        (bs_denied),
    .contention_o    (terr_bsock_contention_o),
    .err_short_o     (bs_short),
    .err_long_o      (bs_long),
    .err_unowned_o   (bs_unowned),
    .retire_unowned_o(terr_bsock_retire_unowned_o),
    .wbeat_unowned_o (terr_bsock_wbeat_unowned_o),
    .ledger_full_o   (bs_ledger_full)
  );

  // THE TERRAIN HPS ARBITER's ONE BRIDGE PORT, now the socket's HPS client 1.
  // Client 0 stays MEM.UPLOAD's, so `upl_hps_wait_o` reads what it read before;
  // index order is the arbiter's priority law, so the terrain spine sits BELOW
  // the upload -- a background copy is short and bounded, while PART.STATE's
  // generation stream behind this port asks for most of a tick, and putting it
  // above would starve every upload for the tick. Starvation of the spine is
  // visible as `build_hps_wait_o[1]` -> `terr_hps_sock_wait` (sunk: the spine's
  // own per-client waits are already exported by `u_terr_hps_arb`).
  // (`terr_hps_*` are declared at the top of the body: PART.STATE reads
  // `terr_hps_wr_ready` several thousand lines above this point.)
  zhao_hps_burst_req_t [1:0]       sock_hps_req;
  logic                [1:0]       sock_hps_grant;
  zhao_hps_burst_rsp_t [1:0]       sock_hps_rsp;
  logic                [1:0][31:0] sock_hps_wait;
  /* verilator lint_off UNUSEDSIGNAL */
  logic                [31:0]      terr_hps_sock_wait;
  /* verilator lint_on UNUSEDSIGNAL */
  assign sock_hps_req      = {terr_hps_req, upl_hps_req[0]};
  assign upl_hps_grant[0]  = sock_hps_grant[0];
  assign upl_hps_rsp[0]    = sock_hps_rsp[0];
  assign terr_hps_grant    = sock_hps_grant[1];
  assign terr_hps_rsp      = sock_hps_rsp[1];
  assign upl_hps_wait_o    = sock_hps_wait[0];
  assign terr_hps_sock_wait = sock_hps_wait[1];

  zhao_mem_upload u_mem_upload (
    .clk                  (gpu_clk),
    .rst_n                (rst_n),
    .req_valid_i          (cmd_upl_valid),
    .req_ready_o          (cmd_upl_ready),
    // the .zpak KIND is the tag: 5f.1 publishes it as the row's kind
    .req_tag_i            (cmd_upl_kind),
    .req_index_i          (cmd_upl_index),
    .req_hps_addr_i       (cmd_upl_hps),
    .req_vram_addr_i      (cmd_upl_vram),
    .req_len_i            (cmd_upl_len),
    .req_epoch_i          (cmd_upl_epoch),
    .req_dst_slot_i       (cmd_upl_slot),
    .req_new_gen_i        (cmd_upl_gen),
    .req_crc_i            (cmd_upl_crc),
    .cfg_region_base_i    (upl_cfg_region_base_i),
    .cfg_region_bytes_i   (upl_cfg_region_bytes_i),
    .cfg_arena_base_i     (upl_cfg_arena_base_i),
    .cfg_arena_bytes_i    (upl_cfg_arena_bytes_i),
    .cfg_epoch_i          (upl_cfg_epoch_i),
    .hps_req_o            (upl_hps_req[0]),
    .hps_req_grant_i      (upl_hps_grant[0]),
    .hps_rsp_i            (upl_hps_rsp[0]),
    .guard_req_o          (upl_guard_req),
    .guard_rsp_i          (upl_guard_rsp),
    .guard_wdata_o        (upl_wdata),
    .guard_wvalid_o       (upl_wvalid),
    .guard_wready_i       (upl_wready),
    .guard_wlast_o        (upl_wlast),
    .retire_words_i       (upl_retire_words),
    .publish_valid_o      (upl_publish_valid_o),
    .publish_slot_o       (upl_publish_slot_o),
    .publish_generation_o (upl_publish_generation_o),
    .publish_tag_o        (upl_publish_tag_o),
    .publish_index_o      (upl_publish_index_o),
    .publish_base_o       (upl_publish_base_o),
    .publish_extent_o     (upl_publish_extent_o),
    .done_o               (upl_done_o),
    .status_o             (upl_status_o),
    .uploads_published_o  (upl_published_o),
    .refused_o            (upl_refused_o)
  );

  zhao_shell_top_v2 #(
    .FRAMER_Q    (FRAMER_Q),
    .WFIFO_W     (WFIFO_W),
    .BUILD_HPS_N (2)
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
    // I49: the RESOLVED material, per triangle, from u_material_window.
    .tri_flat_request_i        (tri_flat_request_c),
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
    // REAL: the pad bus AFTER the SNAC merge (input_rules.md 7.4). See the
    // `u_input_snac` composition below the shell for why the merge is the
    // adapter's and not this file's.
    .pad_present_i             (snacm_present_c),
    .pad_buttons_i             (snacm_buttons_c),
    .pad_lx_i                  (snacm_lx_c),
    .pad_ly_i                  (snacm_ly_c),
    .pad_rx_i                  (snacm_rx_c),
    .pad_ry_i                  (snacm_ry_c),
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
    // THE TERRAIN.BUILD SOCKET (slot 6 + HPS clients 2 and 3). Since entry I26
    // closed its guard client is `u_build_share` (MEM.UPLOAD, TERRAIN.PAGELOADER
    // and the terrain read share), and its HPS client 1 is `u_terr_hps_arb`.
    .build_guard_req_i         (bsk_req),
    .build_guard_rsp_o         (bsk_rsp),
    .build_wdata_i             (bsk_wdata),
    .build_wvalid_i            (bsk_wvalid),
    .build_wready_o            (bsk_wready),
    .build_wlast_i             (bsk_wlast),
    .build_retire_words_o      (bsk_credits),
    .build_beat_valid_o        (bsk_beat_valid),
    .build_beat_data_o         (bsk_beat_data),
    .build_beat_last_o         (bsk_beat_last),
    .build_hps_req_i           (sock_hps_req),
    .build_hps_grant_o         (sock_hps_grant),
    .build_hps_rsp_o           (sock_hps_rsp),
    .build_hps_wait_o          (sock_hps_wait),
    // MEM.UPLOAD reads only; the terrain arbiter's port carries TERRAIN.WRITEBACK's
    // journal and PART.STATE's generation writes.
    .build_hps_wr_valid_i      ({terr_hps_wr_valid, 1'b0}),
    .build_hps_wr_data_i       ({terr_hps_wr_data, 64'd0}),
    .build_hps_wr_last_i       ({terr_hps_wr_last, 1'b0}),
    .build_hps_wr_ready_o      (terr_hps_wr_ready),
    // R32: the guard's write arm into RENDER.ASSET_POOL is bounded by the SAME
    // region MEM.UPLOAD checks every request against and publishes into, so
    // the two cannot disagree about where a resource may land.
    .build_res_valid_i         (upl_cfg_region_bytes_i != 32'd0),
    .build_res_base_i          (upl_cfg_region_base_i),
    .build_res_span_i          (upl_cfg_region_bytes_i),
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
    .render_texture_fragments_o        (render_texture_fragments_o),
    .render_texture_cache_hits_o       (render_texture_cache_hits_o),
    .render_texture_cache_misses_o     (render_texture_cache_misses_o),
    .render_texture_palette_lookups_o  (render_texture_palette_lookups_o),
    .render_texture_plan_accepted_o    (render_texture_plan_accepted_o),
    .render_texture_dispatch_accepted_o(render_texture_dispatch_accepted_o),
    .render_texture_combine_refused_o  (render_texture_combine_refused_o),
    .render_texture_samples_o          (render_texture_samples_o),
    // ---- POST.COMPOSITE's lease (I15/I16) and POST.ECHO --------------------
    .post_frame_w_i            (post_frame_w_c),
    .post_frame_h_i            (post_frame_h_c),
    // Duo exactly where `post_frame_w_c/h_c` chose the Duo view (the case's default).
    .post_duo_i                ((mode_act_o != MODE_Z60_C) && (mode_act_o != MODE_STORM_C)),
    // R35/R36: CMD.EXEC's committed echo ARM, and its hold on a pass start
    // while the look or the grading table is being written.
    .post_echo_arm_i           (post_look_echo_arm_w),
    .post_look_hold_i          (post_look_hold_w),
    .post_pass_start_o         (post_pass_start_c),
    .post_view_o               (post_view_c),
    .post_src_valid_o          (post_s_valid_c),
    .post_src_ready_i          (post_s_ready_c),
    .post_src_rgb_o            (post_s_rgb_c),
    .post_out_valid_i          (post_o_valid_c),
    .post_out_ready_o          (post_o_ready_c),
    .post_out_rgb_i            (post_o_rgb_c),
    .post_out_x_i              (post_o_x_c),
    .post_out_y_i              (post_o_y_c),
    .post_out_last_i           (post_o_last_c),
    .post_echo_valid_i         (post_echo_valid_c),
    .post_echo_rgb_i           (post_echo_rgb_c),
    .post_busy_o               (post_busy_o),
    .post_passes_o             (post_passes_o),
    .post_frames_o             (post_frames_o),
    .post_fault_o              (post_fault_o),
    .post_src_reads_o          (post_src_reads_o),
    .post_src_pixels_o         (post_src_pixels_o),
    .post_retire_unowned_o     (post_retire_unowned_o),
    .post_share_contention_o   (post_share_contention_o),
    .echo_passes_complete_o    (echo_passes_complete_o),
    .echo_passes_torn_o        (echo_passes_torn_o),
    .echo_pixels_written_o     (echo_pixels_written_o),
    .echo_pixels_dropped_o     (echo_pixels_dropped_o),
    .echo_fault_o              (echo_fault_o),
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
  // INPUT.SNAC -- the second ROUTE to the pad state, and the merge (R7)
  // ==========================================================================
  // Owner ruling R7: "INPUT.SNAC, GEOM.WARP, POST.ECHO -- Build all three
  // (owner, explicit). They stay mandatory; the 2026-09-18 revocation stands."
  //
  // The 2026-08-31 section 6.6 sentence survives that ruling and is the whole
  // shape of the block: "It must emit the same canonical PadFrame and may not
  // create a second input semantics." So this is a ROUTE, not a capability:
  // `spec/input_rules.md` 2's atomic latch, 2.2's absent-pad law, 2.3's
  // sequence law and 4's button table are all unchanged and all still
  // INPUT.SNAPSHOT's. Section 7 (written 2026-09-20) adds the bus, the two
  // normalisations and the merge, and nothing else.
  //
  // REAL PRODUCER -> REAL IMPLEMENTATION -> REAL CONSUMER:
  //   the SNAC connector's five pins  ->  `zhao_input_snac`'s serial engine
  //   and decode  ->  `zhao_input_snapshot` inside `u_shell`, which latches
  //   the merged bus at the frame tick exactly as it latched the old one.
  //
  // WHY THE MERGE IS IN THE BLOCK AND NOT IN THESE BRACES. A composer holding
  // a pad mux would be inventing an input law in the one file whose whole
  // discipline is that it invents none -- and the ledger agrees from the other
  // side: INPUT.SNAC's declared `outputs: [pad_pins]`, `downstream:
  // [INPUT.SNAPSHOT]`. The adapter's output IS the pad bus.
  //
  // IT IS THE IDENTITY WHEN IDLE. With no connector populated, DAT idles high,
  // every poll times out, every slot reads absent, and `pad_*_i` reaches the
  // shell bit for bit. That is asserted against a fixture with distinct values
  // per slot (`input_snac_directed` case 1), not argued here -- "it is
  // transparent" is exactly the kind of sentence that gets written in a
  // comment and never measured.
  //
  // THE COUNTERS ARE NOT DECORATION. `spec/input_rules.md` 2.3 has said since
  // 2026-08-14 that `input_sequence_gaps` counts a gap "in the INPUT.SNAC
  // merge path"; section 7.5 now says executably what that gap is, and the
  // counter's two operands are loaded by DIFFERENT events (the serial
  // engine's completion, the frame tick) so it measures TIMING rather than
  // values. Every one of the block's counters has been SEEN TO FIRE on legal
  // stimulus, so none owes a mutant.
  logic [3:0]  snacm_present_c;
  logic [31:0] snacm_buttons_c [0:3];
  logic [15:0] snacm_lx_c [0:3];
  logic [15:0] snacm_ly_c [0:3];
  logic [15:0] snacm_rx_c [0:3];
  logic [15:0] snacm_ry_c [0:3];

  zhao_input_snac #(
    .PORTS (SNAC_PORTS)
  ) u_input_snac (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: pins of the part, the same class as `pad_buttons_i`.
    .snac_att_n_o (snac_att_n_o),
    .snac_clk_o   (snac_clk_o),
    .snac_cmd_o   (snac_cmd_o),
    .snac_dat_i   (snac_dat_i),
    .snac_ack_n_i (snac_ack_n_i),

    // REAL: VIDEO.FRAMECTL's frame boundary, the same pulse INPUT.SNAPSHOT
    // latches on -- so the gap law measures the interval the snapshot uses
    // and not some other clock's idea of a frame.
    .frame_tick_i (core_tick_c),

    // REAL: the existing route in.
    .host_pad_present_i (pad_present_i),
    .host_pad_buttons_i (pad_buttons_i),
    .host_pad_lx_i      (pad_lx_i),
    .host_pad_ly_i      (pad_ly_i),
    .host_pad_rx_i      (pad_rx_i),
    .host_pad_ry_i      (pad_ry_i),

    // REAL: the merged route out, into the shell's INPUT.SNAPSHOT.
    .pad_present_o (snacm_present_c),
    .pad_buttons_o (snacm_buttons_c),
    .pad_lx_o      (snacm_lx_c),
    .pad_ly_o      (snacm_ly_c),
    .pad_rx_o      (snacm_rx_c),
    .pad_ry_o      (snacm_ry_c),

    .snac_present_o    (snac_present_o),
    .snac_polls_o      (snac_polls_o),
    .snac_timeouts_o   (snac_timeouts_o),
    .snac_bad_header_o (snac_bad_header_o),
    .snac_overrides_o  (snac_overrides_o),
    .input_snac_input_sequence_gaps_o (snac_seq_gaps_o)
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

    // R52: the arming is CMD.EXEC's, lowered from `DebugTraceArm` 0xF003 as a
    // one-cycle pulse at that record's LAST BYTE. `u_cmd_decoder` offers a
    // record to this ring at the record's byte 15, so the arming record itself
    // is not traced and every record after it in the packet is -- a guarantee,
    // because the next header cannot complete for sixteen more byte-cycles.
    .arm_we_i   (cx_trace_arm_we_c),
    .arm_mask_i (cx_trace_arm_mask_c),
    .armed_o    (dbg_trace_armed_o),
    .clear_i    (cx_trace_clear_c),

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

    // R51: the readout is the HPS register aperture's tenant 1, section 7b-iii.
    .rd_addr_i (hrt_rd_addr_c),
    .rd_data_o (hrt_rd_data_c),

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
  //   * TERRAIN.WRITEBACK WAS LEFT OUT HERE UNTIL 2026-09-19 (entry I28), for
  //     two reasons that were both true: its journal address and ticket had no
  //     owner, and it would have been a third client of a two-port arbiter.
  //     Owner ruling R14 gave the first to SW.STREAM and R4 widened the arbiter,
  //     so it is composed below, behind `u_terrain_jdoorbell`.
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

  // THREE CLIENTS SINCE 2026-09-19 (owner ruling R4, `zhao_hps_arbiter_n`).
  // TERRAIN.WRITEBACK's journal writes take index 2, BELOW the loader, and that
  // placement is a statement rather than a default: the arbiter's law is that
  // a continuously-asking lower index starves every higher one, so a burst of
  // page loads makes the writeback wait -- visibly, in `c2_wait_cycles`. It
  // cannot deadlock: a dirty victim's slot is barred from LOADING until its
  // sheet is ACKed (the directory's EVICT_PENDING), so the load that needs the
  // writeback is never among the loads that starve it, and the loader goes
  // idle when TERRAIN.SEQ is held behind its own barrier. Indices 0 and 1 keep
  // their meaning, so `c0/c1` counters read exactly what they read before.
  // The writeback's `wready` is the bridge's level, and only the burst's owner
  // is streaming, so it needs no routing through the arbiter.
  zhao_hps_burst_req_t twb_hps_req;
  logic                twb_hps_grant;
  zhao_hps_burst_rsp_t twb_hps_rsp;
  logic [63:0]         twb_hps_wdata;
  logic                twb_hps_wvalid, twb_hps_wlast;
  // FOUR CLIENTS SINCE 2026-09-19 (gz/pfs, entry I1). PART.STATE's generation
  // store takes index 3, the LOWEST, by the same argument the writeback's
  // placement makes: a burst of page loads makes the particle stream wait,
  // visibly, in `c3_wait_cycles`, and cannot deadlock it -- no terrain client
  // waits on a particle. The store is a streamer with bounded staging on both
  // sides, so waiting stalls PART.STATE's tick; it never drops a record. At the
  // required tier a tick is 32,768 records in 8,192 read and 8,192 write bursts
  // of 64 B (PART.STATE.md's 1 MiB per tick). Indices 0-2 keep their meaning.
  // Like the writeback, the store gates its write beats on the bridge's level,
  // `terr_hps_wr_ready_i`, and only the burst's owner is streaming.
  // (`ptb_hps_*` are declared with `u_part_hps`, beside PART.STATE.)
  zhao_hps_burst_req_t [3:0]       thps_req;
  logic                [3:0]       thps_grant;
  logic                [3:0]       thps_wr_valid, thps_wr_last;
  logic                [3:0][63:0] thps_wr_data;
  zhao_hps_burst_rsp_t [3:0]       thps_rsp;
  logic                [3:0][31:0] thps_bursts;
  logic                [3:1][31:0] thps_wait;

  assign thps_req      = {ptb_hps_req, twb_hps_req, tpl_hps_req, tcm_hps_req};
  assign thps_wr_valid = {ptb_hps_wvalid, twb_hps_wvalid, 1'b0, 1'b0};
  assign thps_wr_last  = {ptb_hps_wlast, twb_hps_wlast, 1'b0, 1'b0};
  assign thps_wr_data  = {ptb_hps_wdata, twb_hps_wdata, 64'd0, 64'd0};
  assign tcm_hps_grant = thps_grant[0];
  assign tpl_hps_grant = thps_grant[1];
  assign twb_hps_grant = thps_grant[2];
  assign ptb_hps_grant = thps_grant[3];
  assign tcm_hps_rsp   = thps_rsp[0];
  assign tpl_hps_rsp   = thps_rsp[1];
  assign twb_hps_rsp   = thps_rsp[2];
  assign ptb_hps_rsp   = thps_rsp[3];
  assign terr_hps_c0_bursts_o      = thps_bursts[0];
  assign terr_hps_c1_bursts_o      = thps_bursts[1];
  assign terr_hps_c2_bursts_o      = thps_bursts[2];
  assign terr_hps_c3_bursts_o      = thps_bursts[3];
  assign terr_hps_c1_wait_cycles_o = thps_wait[1];
  assign terr_hps_c2_wait_cycles_o = thps_wait[2];
  assign terr_hps_c3_wait_cycles_o = thps_wait[3];

  zhao_hps_arbiter_n #(
    .N(4)
  ) u_terr_hps_arb (
    .clk          (gpu_clk),
    .rst_n        (rst_n),
    .req_i        (thps_req),
    .req_grant_o  (thps_grant),
    .wr_valid_i   (thps_wr_valid),
    .wr_data_i    (thps_wr_data),
    .wr_last_i    (thps_wr_last),
    .rsp_o        (thps_rsp),
    .b_req_o      (terr_hps_req),
    .b_req_grant_i(terr_hps_grant),
    .b_wr_valid_o (terr_hps_wr_valid),
    .b_wr_data_o  (terr_hps_wr_data),
    .b_wr_last_o  (terr_hps_wr_last),
    .b_rsp_i      (terr_hps_rsp),
    .bursts_o     (thps_bursts),
    .wait_cycles_o(thps_wait),
    // R55: the pending slot holds ONE request per client, and a second,
    // different one offered while it is occupied is dropped. No client here
    // can do it -- each is a holder whose request fields do not move inside
    // its request state (the arbiter's rule 6c names all four) -- so this
    // reads zero, and now it reads zero rather than being argued to.
    .pend_dropped_o     (terr_hps_pend_dropped_o),
    .pend_dropped_mask_o(terr_hps_pend_dropped_mask_o)
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
  // The re-fetched lead of a burst resumed after an abandoned one: the cost of
  // `zhao_hps_bridge`'s 64-byte alignment against T5's 32-byte record. Sunk
  // here beside `list_bytes_read_o`, which is sunk for the same reason -- the
  // block's own directed test owns both numbers
  // (tests/terrain/terrain_cmd_rtl_directed.cpp, case C).
  wire [31:0]        tcm_list_refetch;
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
    .list_refetch_bytes_o(tcm_list_refetch),
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

  // ---- TERRAIN.SEQ's writeback job, and its answer ------------------------
  wire                     tsq_wb_valid, tsq_wb_ready;
  wire [TERR_SLOTW-1:0]    tsq_wb_slot;
  wire [TERR_GENW-1:0]     tsq_wb_gen;
  wire [31:0]              tsq_wb_epoch, tsq_wb_island, tsq_wb_src_id;
  wire signed [15:0]       tsq_wb_ix, tsq_wb_iz;
  wire                     tsq_wb_done_valid;
  wire [TERR_SLOTW-1:0]    tsq_wb_done_slot;
  // TERRAIN.WRITEBACK's barrier release, to the directory.
  wire                     twb_rel_valid, twb_rel_ready;
  wire [TERR_SLOTW-1:0]    twb_rel_slot;
  wire [TERR_GENW-1:0]     twb_rel_gen;
  wire [31:0]              twb_rel_epoch;
  // TERRAIN.WRITEBACK's read client, the third requester of the one terrain
  // guard read client (`u_terrain_rdshare`).
  zhao_guard_req_t         twb_g_req;
  zhao_guard_rsp_t         twb_g_rsp;
  wire                     twb_g_beat_valid, twb_g_beat_last;
  wire [63:0]              twb_g_beat_data;

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

    // TERRAIN.WRITEBACK IS COMPOSED (entry I28 closed 2026-09-19), through
    // SW.STREAM's journal doorbell, which attaches the two fields this port
    // does not carry. The completion comes back through the doorbell too, so
    // the job and its answer are still one seam.
    .wb_valid_o      (tsq_wb_valid),
    .wb_ready_i      (tsq_wb_ready),
    .wb_done_valid_i (tsq_wb_done_valid),
    .wb_done_slot_i  (tsq_wb_done_slot),
    .wb_slot_o       (tsq_wb_slot),
    .wb_gen_o        (tsq_wb_gen),
    .wb_epoch_o      (tsq_wb_epoch),
    .wb_island_o     (tsq_wb_island),
    .wb_ix_o         (tsq_wb_ix),
    .wb_iz_o         (tsq_wb_iz),
    .wb_src_id_o     (tsq_wb_src_id),
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

    // THE BARRIER RELEASE, from TERRAIN.WRITEBACK: raised only on a matched,
    // good journal ACK for a ticket that block allocated (entry I28, closed).
    .wb_valid_i(twb_rel_valid),
    .wb_ready_o(twb_rel_ready),
    .wb_slot_i (twb_rel_slot),
    .wb_gen_i  (twb_rel_gen),
    .wb_epoch_i(twb_rel_epoch),

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

  // ---- SW.STREAM's JOURNAL DOORBELL and TERRAIN.WRITEBACK (entry I28) -------
  // CLOSED 2026-09-19 under owner ruling R14. The two reasons I28 gave for
  // leaving the writeback out are both answered, and neither by this file:
  //   * the journal ADDRESS and TICKET now have an owner -- SW.STREAM, whose
  //     grants `zhao_terrain_jdoorbell` attaches to TERRAIN.SEQ's job verbatim
  //     (design/contracts/TERRAIN.WRITEBACK.DOORBELL.md). Nothing here mints a
  //     ticket; a job with no grant posted WAITS and the wait is counted;
  //   * the THIRD HPS client exists: owner ruling R4 widened the arbiter to N,
  //     and the writeback is index 2 of `u_terr_hps_arb` above.
  // Its sheet READS go through the SAME one guard read client as the compose
  // path's two readers -- a third requester of `u_terrain_rdshare`, under the
  // same TERRAIN.BUILD identity MEM.GUARD's read arm already admits -- so entry
  // I26's boundary gains no port for it.
  //
  // WHAT CANNOT YET ENTER IT, said so a quiet counter is not misread: a
  // writeback job is caused by claiming a slot whose F sheet is DIRTY, and a
  // page becomes dirty only through the directory's `dm_f`, whose writer is
  // TERRAIN.BAKE (entries I27/I32, still boundaries). Until then the path is
  // reachable only by a harness driving `terr_dm_*`. The traversal evidence is
  // `tests/terrain/world_composed_directed.cpp`, which composes this same
  // sequencer -> doorbell -> writeback -> directory chain with the doorbell's
  // HPS side played, and journals, ACKs and releases a real dirty victim.
  //
  // THE WIDTH STEP, the same one the pageloader's completion crosses: the
  // writeback and the doorbell carry TERR_MEMSLOT (one bit wider than the
  // directory's handle) so a computed slot of 1,024 cannot alias to slot 0.
  // Here the producer is TERR_SLOTW wide, the job's slot is carried to the
  // release and the completion VERBATIM, and so the top bit is structurally
  // zero on both ends -- a detector on it would report zero about a wire tied
  // to zero, which is the unpin path's argument and reached the same way.
  wire                    tjd_wj_valid, tjd_wj_ready;
  wire [TERR_MEMSLOT-1:0] tjd_wj_slot;
  wire [TERR_GENW-1:0]    tjd_wj_gen;
  wire [31:0]             tjd_wj_epoch, tjd_wj_island, tjd_wj_seq, tjd_wj_src;
  wire signed [15:0]      tjd_wj_ix, tjd_wj_iz;
  wire [63:0]             tjd_wj_addr;
  wire                    twb_landed_valid;
  wire [31:0]             twb_landed_seq;
  wire                    twb_done_valid, twb_done_ready, twb_done_ok;
  wire [TERR_MEMSLOT-1:0] twb_done_slot_w, tjd_seq_done_slot_w, twb_rel_slot_w;
  wire [3:0]              twb_done_verdict;
  wire [31:0]             twb_done_seq;
  /* verilator lint_off UNUSEDSIGNAL */
  // The top slot bit (see the width step above), the completion's restated
  // identity (TERRAIN.SEQ's barrier port takes {valid, slot} only), the
  // fault trace, and the counters no port of this module carries.
  wire                    twb_slot_msbs = twb_rel_slot_w[TERR_MEMSLOT-1]
                                        ^ tjd_seq_done_slot_w[TERR_MEMSLOT-1];
  wire [TERR_GENW-1:0]    twb_done_gen;
  wire [31:0]             twb_done_epoch, twb_done_src_id;
  wire [31:0]             twb_fault_island, twb_fault_seq, twb_fault_src_id;
  wire signed [15:0]      twb_fault_ix, twb_fault_iz;
  wire [3:0]              twb_fault_verdict;
  wire [31:0]             twb_hdr_ident_fails, twb_bridge_errs, twb_acks_ok;
  wire [31:0]             twb_acks_nak, twb_acks_after_epoch, twb_seq_conflicts;
  wire [31:0]             twb_bytes, twb_outstanding_hwm, twb_ack_wait_max;
  wire [31:0]             twb_jobs_stall;
  wire [31:0]             tjd_grants_posted, tjd_grants_taken, tjd_returns_landed;
  wire [31:0]             tjd_returns_final, tjd_credit_stall, tjd_owed;
  /* verilator lint_on UNUSEDSIGNAL */

  assign tsq_wb_done_slot = tjd_seq_done_slot_w[TERR_SLOTW-1:0];
  assign twb_rel_slot     = twb_rel_slot_w[TERR_SLOTW-1:0];

  zhao_terrain_jdoorbell #(
    .SLOTW  (TERR_MEMSLOT),
    .GENW   (TERR_GENW),
    .GRANTS (4),
    .TICKETS(4)
  ) u_terrain_jdoorbell (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    .cfg_journal_base_i(terr_cfg_journal_base_i),

    .post_valid_i (terr_jdb_post_valid_i),
    .post_ready_o (terr_jdb_post_ready_o),
    .post_slot_i  (terr_jdb_post_slot_i),
    .post_ticket_i(terr_jdb_post_ticket_i),

    .sj_valid_i (tsq_wb_valid),
    .sj_ready_o (tsq_wb_ready),
    .sj_slot_i  ({1'b0, tsq_wb_slot}),
    .sj_gen_i   (tsq_wb_gen),
    .sj_epoch_i (tsq_wb_epoch),
    .sj_island_i(tsq_wb_island),
    .sj_ix_i    (tsq_wb_ix),
    .sj_iz_i    (tsq_wb_iz),
    .sj_src_id_i(tsq_wb_src_id),

    .wj_valid_o       (tjd_wj_valid),
    .wj_ready_i       (tjd_wj_ready),
    .wj_slot_o        (tjd_wj_slot),
    .wj_gen_o         (tjd_wj_gen),
    .wj_epoch_o       (tjd_wj_epoch),
    .wj_island_o      (tjd_wj_island),
    .wj_ix_o          (tjd_wj_ix),
    .wj_iz_o          (tjd_wj_iz),
    .wj_journal_addr_o(tjd_wj_addr),
    .wj_seq_o         (tjd_wj_seq),
    .wj_src_id_o      (tjd_wj_src),

    .landed_valid_i(twb_landed_valid),
    .landed_seq_i  (twb_landed_seq),
    .done_valid_i  (twb_done_valid),
    .done_ready_o  (twb_done_ready),
    .done_slot_i   (twb_done_slot_w),
    .done_ok_i     (twb_done_ok),
    .done_verdict_i(twb_done_verdict),
    .done_seq_i    (twb_done_seq),

    .seq_done_valid_o(tsq_wb_done_valid),
    .seq_done_slot_o (tjd_seq_done_slot_w),

    .ret_valid_o  (terr_jdb_ret_valid_o),
    .ret_ready_i  (terr_jdb_ret_ready_i),
    .ret_ticket_o (terr_jdb_ret_ticket_o),
    .ret_final_o  (terr_jdb_ret_final_o),
    .ret_ok_o     (terr_jdb_ret_ok_o),
    .ret_verdict_o(terr_jdb_ret_verdict_o),

    .grants_posted_o      (tjd_grants_posted),
    .grants_taken_o       (tjd_grants_taken),
    .returns_landed_o     (tjd_returns_landed),
    .returns_final_o      (tjd_returns_final),
    .starved_cycles_o     (terr_jdb_starved_cycles_o),
    .credit_stall_cycles_o(tjd_credit_stall),
    .tickets_owed_o       (tjd_owed),
    .ret_overflow_o       (terr_jdb_ret_overflow_o)
  );

  zhao_terrain_writeback #(
    .PAGE_BYTES  (TERR_PAGE_BYTES),
    .REGION_BASE (TERR_POOL_BASE),
    .REGION_SLOTS(TERR_POOL_SLOTS),
    .SLOTW       (TERR_MEMSLOT),
    .GENW        (TERR_GENW)
  ) u_terrain_writeback (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    .cfg_vram_client_i  (ZHAO_CLIENT_TERRAIN_BUILD),
    .cfg_hps_client_i   (ZHAO_CLIENT_TERRAIN_BUILD),
    .cfg_journal_base_i (terr_cfg_journal_base_i),
    .cfg_journal_bytes_i(terr_cfg_journal_bytes_i),
    .cfg_epoch_i        (terr_cfg_epoch_i),

    .j_valid_i       (tjd_wj_valid),
    .j_ready_o       (tjd_wj_ready),
    .j_slot_i        (tjd_wj_slot),
    .j_gen_i         (tjd_wj_gen),
    .j_epoch_i       (tjd_wj_epoch),
    .j_island_i      (tjd_wj_island),
    .j_ix_i          (tjd_wj_ix),
    .j_iz_i          (tjd_wj_iz),
    .j_journal_addr_i(tjd_wj_addr),
    .j_seq_i         (tjd_wj_seq),
    .j_src_id_i      (tjd_wj_src),

    .guard_req_o (twb_g_req),
    .guard_rsp_i (twb_g_rsp),
    .beat_valid_i(twb_g_beat_valid),
    .beat_data_i (twb_g_beat_data),
    .beat_last_i (twb_g_beat_last),

    .hps_req_o      (twb_hps_req),
    .hps_req_grant_i(twb_hps_grant),
    .hps_rsp_i      (twb_hps_rsp),
    .hps_wdata_o    (twb_hps_wdata),
    .hps_wvalid_o   (twb_hps_wvalid),
    .hps_wready_i   (terr_hps_wr_ready),
    .hps_wlast_o    (twb_hps_wlast),

    // D3: SW.STREAM's ACK goes straight to the ticket table, which is the one
    // matcher -- an ACK for a ticket it does not hold is counted unmatched and
    // releases nothing. The doorbell adds no second matcher.
    .ack_valid_i(terr_jdb_ack_valid_i),
    .ack_ready_o(terr_jdb_ack_ready_o),
    .ack_seq_i  (terr_jdb_ack_ticket_i),
    .ack_ok_i   (terr_jdb_ack_ok_i),

    .wb_valid_o(twb_rel_valid),
    .wb_ready_i(twb_rel_ready),
    .wb_slot_o (twb_rel_slot_w),
    .wb_gen_o  (twb_rel_gen),
    .wb_epoch_o(twb_rel_epoch),

    .done_valid_o  (twb_done_valid),
    .done_ready_i  (twb_done_ready),
    .done_slot_o   (twb_done_slot_w),
    .done_gen_o    (twb_done_gen),
    .done_epoch_o  (twb_done_epoch),
    .done_ok_o     (twb_done_ok),
    .done_verdict_o(twb_done_verdict),
    .done_seq_o    (twb_done_seq),
    .done_src_id_o (twb_done_src_id),

    .landed_valid_o(twb_landed_valid),
    .landed_seq_o  (twb_landed_seq),

    .fault_island_o (twb_fault_island),
    .fault_ix_o     (twb_fault_ix),
    .fault_iz_o     (twb_fault_iz),
    .fault_seq_o    (twb_fault_seq),
    .fault_src_id_o (twb_fault_src_id),
    .fault_verdict_o(twb_fault_verdict),

    .sheets_written_o     (terr_wb_sheets_written_o),
    .sheets_refused_o     (terr_wb_sheets_refused_o),
    .sheets_faulted_o     (terr_wb_sheets_faulted_o),
    .hdr_ident_fails_o    (twb_hdr_ident_fails),
    .guard_denied_o       (terr_wb_guard_denied_o),
    .bridge_errs_o        (twb_bridge_errs),
    .acks_ok_o            (twb_acks_ok),
    .acks_nak_o           (twb_acks_nak),
    .acks_unmatched_o     (terr_wb_acks_unmatched_o),
    .acks_after_epoch_o   (twb_acks_after_epoch),
    .acks_overdue_o       (terr_wb_acks_overdue_o),
    .seq_conflicts_o      (twb_seq_conflicts),
    .wb_bytes_o           (twb_bytes),
    .outstanding_hwm_o    (twb_outstanding_hwm),
    .ack_wait_max_cycles_o(twb_ack_wait_max),
    .jobs_stall_cycles_o  (twb_jobs_stall)
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

    .guard_req_o   (tpl_g_req),
    .guard_rsp_i   (tpl_g_rsp),
    .guard_wdata_o (tpl_g_wdata),
    .guard_wvalid_o(tpl_g_wvalid),
    .guard_wready_i(tpl_g_wready),
    .guard_wlast_o (tpl_g_wlast),

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
    .view_sel_i     (post_view_c ? 2'b10 : 2'b01),

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
    .view_sel_i   (post_view_c ? 2'b10 : 2'b01),

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
  // R18/R33: CMD.EXEC's token outputs, declared ahead of both instances.
  logic        cmd_tok_budget_valid, cmd_tok_vreq_valid, cmd_tok_vreq_view;
  logic [31:0] cmd_tok_budget_geom0, cmd_tok_budget_geom1;
  logic [31:0] cmd_tok_budget_frag0, cmd_tok_budget_frag1, cmd_tok_budget_shared;
  logic [31:0] cmd_tok_vreq_geom, cmd_tok_vreq_frag;
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

    // I41, CLOSED 2026-09-20: the DRAW DISPATCH now has a consumer INSIDE this
    // module. `u_geom_drawjob` is the resolver entry I36 said did not exist,
    // and owner ruling R29 gave it the three field laws it was missing. The
    // handles are resolved against real residency here rather than leaving the
    // console unresolved.
    .draw_valid_o          (cmd_draw_valid_w),
    .draw_ready_i          (cmd_draw_ready_w),
    .draw_form_o           (cmd_draw_form_w),
    .draw_material_set_o   (cmd_draw_material_set_w),
    .draw_transform_o      (cmd_draw_transform_w),
    .draw_viewport_mask_o  (cmd_draw_viewport_mask_w),
    .draw_semantic_weight_o(cmd_draw_semantic_weight_w),
    .draw_flags_o          (cmd_draw_flags_w),
    .draw_src_id_o         (cmd_draw_src_id_w),

    // R17: PublishResource -> MEM.UPLOAD's request port.
    .upl_valid_o    (cmd_upl_valid),
    .upl_ready_i    (cmd_upl_ready),
    .upl_index_o    (cmd_upl_index),
    .upl_kind_o     (cmd_upl_kind),
    .upl_hps_addr_o (cmd_upl_hps),
    .upl_vram_addr_o(cmd_upl_vram),
    .upl_len_o      (cmd_upl_len),
    .upl_epoch_o    (cmd_upl_epoch),
    .upl_dst_slot_o (cmd_upl_slot),
    .upl_new_gen_o  (cmd_upl_gen),
    .upl_crc_o      (cmd_upl_crc),

    // R25: SetEnvironment -> GEOM.LIGHT.ENV (u_light_env, beside u_light_stream).
    .env_valid_o     (cmd_env_valid),
    .env_ready_i     (cmd_env_ready),
    .env_sun_yaw_o   (cmd_env_yaw),
    .env_sun_pitch_o (cmd_env_pitch),
    .env_sun_colour_o(cmd_env_sun),
    .env_ambient_o   (cmd_env_amb),
    .envs_issued_o   (cmd_exec_envs_o),
    // R41: SetPopulation -> PART.POP (u_part_pop, beside the particle engine).
    .pop_valid_o       (cmd_pop_valid),
    .pop_ready_i       (cmd_pop_ready),
    .pop_population_o  (cmd_pop_population),
    .pop_origin_x_o    (cmd_pop_ox),
    .pop_origin_y_o    (cmd_pop_oy),
    .pop_origin_z_o    (cmd_pop_oz),
    .pop_active_count_o(cmd_pop_count),
    .pop_plane_c_o     (cmd_pop_pc),
    .pop_plane_nx_o    (cmd_pop_nx),
    .pop_plane_ny_o    (cmd_pop_ny),
    .pop_plane_nz_o    (cmd_pop_nz),
    .pop_flags_o       (cmd_pop_flags),
    .pops_issued_o     (cmd_exec_pops_o),
    // R18/R33: the token CEILING and each view's REQUEST -> MEASURE.TOKENS.
    .tok_budget_valid_o (cmd_tok_budget_valid),
    .tok_budget_geom0_o (cmd_tok_budget_geom0),
    .tok_budget_geom1_o (cmd_tok_budget_geom1),
    .tok_budget_frag0_o (cmd_tok_budget_frag0),
    .tok_budget_frag1_o (cmd_tok_budget_frag1),
    .tok_budget_shared_o(cmd_tok_budget_shared),
    .tok_vreq_valid_o   (cmd_tok_vreq_valid),
    .tok_vreq_view_o    (cmd_tok_vreq_view),
    .tok_vreq_geom_o    (cmd_tok_vreq_geom),
    .tok_vreq_frag_o    (cmd_tok_vreq_frag),
    .contracts_applied_o(cmd_exec_contracts_o),
    // R35/R36: SetPost / SetGradeTable -> POST.COMPOSITE's look and table, and
    // POST.ECHO's arm, through the door of an IDLE post lease.
    .post_idle_i       (!post_busy_o),
    .post_look_busy_o  (post_look_hold_w),
    .post_bloom_gain_o (post_look_bloom_gain_w),
    .post_grade_valid_o(post_look_grade_valid_w),
    .post_echo_arm_o   (post_look_echo_arm_w),
    .post_bias_r_o     (post_look_bias_r_w),
    .post_bias_g_o     (post_look_bias_g_w),
    .post_bias_b_o     (post_look_bias_b_w),
    .post_flash_rgb_o  (post_look_flash_rgb_w),
    .post_flash_amt_o  (post_look_flash_amt_w),
    .post_ink_rgb_o    (post_look_ink_rgb_w),
    .post_pv_we_o      (post_look_pv_we_w),
    .post_pv_sel_o     (post_look_pv_sel_w),
    .post_pv_addr_o    (post_look_pv_addr_w),
    .post_pv_data_o    (post_look_pv_data_w),

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
    .uploads_issued_o     (cmd_exec_uploads_o),
    .upload_overflow_o    (cmd_exec_upload_overflow_o),
    .post_looks_applied_o (cmd_exec_post_looks_o),
    .grade_entries_written_o(cmd_exec_grade_entries_o),
    .post_refused_o       (cmd_exec_post_refused_o),
    .grade_overflow_o     (cmd_exec_grade_overflow_o),
    // R52: DebugTraceArm 0xF003 -> DEBUG.TRACE at section 7b-ii.
    .dbg_trace_arm_we_o   (cx_trace_arm_we_c),
    .dbg_trace_arm_mask_o (cx_trace_arm_mask_c),
    .dbg_trace_clear_o    (cx_trace_clear_c),
    .trace_arms_applied_o (cmd_exec_trace_arms_o),
    .trace_arm_refused_o  (cmd_exec_trace_arm_refused_o),
    .unsupported_o        (cmd_exec_unsupported_o)
  );

  // ---- MEASURE.TOKENS, composed on its budget side (R18/R33) --------------
  // ONE unit end to end: the counts CMD.EXEC lifted off the wire, unchanged.
  // The contract's numbers are the ceiling; a SetView's are the request, and
  // the guard clamps (and counts) any request above the ceiling. Nothing here
  // converts, scales or invents a capacity.

  zhao_measure_tokens #(
    .TOK_W (32)
  ) u_measure_tokens (
    .clk             (gpu_clk),
    .rst_n           (rst_n),
    .budget_valid_i  (cmd_tok_budget_valid),
    .budget_geom0_i  (cmd_tok_budget_geom0),
    .budget_geom1_i  (cmd_tok_budget_geom1),
    .budget_frag0_i  (cmd_tok_budget_frag0),
    .budget_frag1_i  (cmd_tok_budget_frag1),
    .budget_shared_i (cmd_tok_budget_shared),
    .vreq_valid_i    (cmd_tok_vreq_valid),
    .vreq_view_i     (cmd_tok_vreq_view),
    .vreq_geom_i     (cmd_tok_vreq_geom),
    .vreq_frag_i     (cmd_tok_vreq_frag),
    .req_valid_i     (tok_req_valid_i),
    .req_view_i      (tok_req_view_i),
    .req_class_i     (tok_req_class_i),
    .req_essential_i (tok_req_essential_i),
    .req_rep_i       (tok_req_rep_i),
    .req_cost_i      (tok_req_cost_i),
    .req_src_id_i    (tok_req_src_id_i),
    .tok_grant_o     (tok_grant_o),
    .tok_shared_o    (tok_shared_o),
    .ret_valid_i     (tok_ret_valid_i),
    .ret_view_i      (tok_ret_view_i),
    .ret_class_i     (tok_ret_class_i),
    .ret_shared_i    (tok_ret_shared_i),
    .ret_cost_i      (tok_ret_cost_i),
    .den_valid_o     (tok_den_valid_o),
    .den_view_o      (tok_den_view_o),
    .den_class_o     (tok_den_class_o),
    .den_rep_o       (tok_den_rep_o),
    .den_reason_o    (tok_den_reason_o),
    .den_src_id_o    (tok_den_src_id_o),
    .den_cost_o      (tok_den_cost_o),
    .avail_geom0_o   (tok_avail_geom0_o),
    .avail_geom1_o   (tok_avail_geom1_o),
    .avail_frag0_o   (tok_avail_frag0_o),
    .avail_frag1_o   (tok_avail_frag1_o),
    .avail_shared_o  (tok_avail_shared_o),
    .tok_rep_count0_o(tok_rep_count0_o),
    .tok_rep_count1_o(tok_rep_count1_o),
    .tok_rep_count2_o(tok_rep_count2_o),
    .tok_rep_count3_o(tok_rep_count3_o),
    .tok_rep_count4_o(tok_rep_count4_o),
    .tok_rep_count5_o(tok_rep_count5_o),
    .tok_rep_count6_o(tok_rep_count6_o),
    .tok_rep_count7_o(tok_rep_count7_o),
    .triangles_culled_o(tok_triangles_culled_o),
    .vreq_clamped_o  (tok_vreq_clamped_o)
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
  // THREE READERS SINCE 2026-09-19. TERRAIN.WRITEBACK reads the evicted page's
  // header and its F sheet (130 bursts a sheet) out of the SAME pool, under the
  // SAME identity, and MEM.GUARD's `terrain_rd_ok` arm is the writeback's own
  // (spec/memory_rules.md 5b: "landed with TERRAIN.WRITEBACK"). So it is a third
  // requester of this share -- `zhao_mem_share_n`, the body `zhao_mem_share2`
  // has wrapped since MEM.SHARE was widened to N and re-proved at N=3 -- and
  // not a third port on this module's edge. Indices 0 and 1 are A and B as
  // before; the rotation is round robin with bound N-1, so a sheet read can
  // delay a refill by at most two requests, and `contention_o` counts it.
  zhao_guard_req_t [2:0] trs_req;
  zhao_guard_rsp_t [2:0] trs_rsp;
  logic            [2:0] trs_beat_valid, trs_beat_last;
  logic           [63:0] trs_beat_data;
  logic       [2:0][31:0] trs_jobs;

  assign trs_req          = {twb_g_req, trs_b_req, trs_a_req};
  assign trs_a_rsp        = trs_rsp[0];
  assign trs_b_rsp        = trs_rsp[1];
  assign twb_g_rsp        = trs_rsp[2];
  assign trs_a_beat_valid = trs_beat_valid[0];
  assign trs_b_beat_valid = trs_beat_valid[1];
  assign twb_g_beat_valid = trs_beat_valid[2];
  assign trs_a_beat_last  = trs_beat_last[0];
  assign trs_b_beat_last  = trs_beat_last[1];
  assign twb_g_beat_last  = trs_beat_last[2];
  assign trs_a_beat_data  = trs_beat_data;
  assign trs_b_beat_data  = trs_beat_data;
  assign twb_g_beat_data  = trs_beat_data;
  assign terr_rdshare_jobs_a_o  = trs_jobs[0];
  assign terr_rdshare_jobs_b_o  = trs_jobs[1];
  assign terr_rdshare_jobs_wb_o = trs_jobs[2];

  zhao_mem_share_n #(
    .N         (3),
    .CLIENT_ID (6),        // ZHAO_CLIENT_TERRAIN_BUILD -- see zhao_pkg
    .FORCE_READ(1'b1)      // all three READ; the pool's write arm is the loader's
  ) u_terrain_rdshare (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    // 0: TERRAIN.HDRREAD, one 64-byte header burst per patch.
    // 1: TERRAIN.PAGESTREAM, three plane bursts per refill.
    // 2: TERRAIN.WRITEBACK, one header and 129 sheet chunks per dirty victim.
    .req_i       (trs_req),
    .rsp_o       (trs_rsp),
    .beat_valid_o(trs_beat_valid),
    .beat_data_o (trs_beat_data),
    .beat_last_o (trs_beat_last),

    // I26 CLOSED: the one client is requester 2 of `u_build_share`.
    .m_req_o       (trs_m_req),
    .m_rsp_i       (trs_m_rsp),
    .m_beat_valid_i(trs_m_beat_valid),
    .m_beat_data_i (trs_m_beat_data),
    .m_beat_last_i (trs_m_beat_last),

    .jobs_o       (trs_jobs),
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
    // Through TERRAIN.HEIGHTTAP from 2026-09-19 (item 14): TESS's request
    // when it makes one, the tap's borrowed read when it does not.
    .lat_req_i    (htp_c_lat_req),
    .lat_vi_i     (htp_c_lat_vi),
    .lat_vj_i     (htp_c_lat_vj),
    .lat_surface_i(htp_c_lat_surface),
    .lat_h_o      (tcc_lat_h),
    .lat_wx_o     (tcc_lat_wx),
    .lat_wz_o     (tcc_lat_wz),

    .cs_req_i      (htp_c_cs_req),
    .cs_ci_i       (htp_c_cs_ci),
    .cs_cj_i       (htp_c_cs_cj),
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

  // The DRAW-STATE SIDEBAND's width (owner rulings R28/R29). One packing,
  // `zref::drawjob`'s: {semantic_weight[71:64], material_set[63:32],
  // raster_state[31:0]}. Named once here so the three blocks that carry it
  // cannot disagree about what it is.
  localparam int unsigned GEOM_SIDE_W = 72;
  // Draw-addressable instance transforms. 256 is the ruling's content tier
  // (256 creatures); it is a KNOB and costs ~10 M10K, and 1,024 -- GEOM.LOOM's
  // whole node space -- would cost ~39. A node index at or above it is
  // COUNTED (`geom_dj_pal_dropped_o`), never wrapped onto another instance's
  // row, and a draw naming an unwritten row is refused.
  localparam int unsigned GEOM_XFORMS = 256;

  // ---- GEOM.MESHFETCH <-> GEOM.CULL ----------------------------------------
  wire               mf_cull_tick, mf_cull_ready, mf_cull_valid, mf_cull_reject;
  wire        [ 1:0] mf_cull_active, mf_cull_vis;
  wire signed [31:0] mf_cull_cx, mf_cull_cy, mf_cull_cz, mf_cull_radius;

  // ---- GEOM.MESHFETCH -> GEOM.ASSETFETCH: the meshlet record ---------------
  wire        mf_r_valid, mf_r_ready;
  wire [15:0] mf_r_instance_id;
  wire [31:0] mf_r_vertex_offset, mf_r_index_offset;
  wire [ 7:0] mf_r_vertex_count, mf_r_triangle_count;
  // The descriptor's visible mask and material are CARRIED by GEOM.ASSETFETCH
  // beside the meshlet (captured with its counts), because this register has
  // moved on by the time the meshlet is servable -- entry I39's argument, and
  // the fix it asked for. The flags byte is read by nothing: no ratified
  // meaning, so no consumer.
  wire [ 1:0] mf_r_visible_mask;
  wire [15:0] mf_r_material_id;
  /* verilator lint_off UNUSEDSIGNAL */
  // [71:32] is the draw's MATERIAL SET and SEMANTIC WEIGHT, riding beside the
  // meshlet to consumers this console does not compose yet: the material set is
  // half of MATERIAL.RESOLVE's request (entry I49 -- and the OTHER half,
  // `af_s_material_id`, is on this same handshake, which is what that entry
  // said it was missing), and the weight is the Measure policy's degrade order.
  // Carried, not dropped: the draw is the only place either value exists, and
  // re-deriving them at the consumer is the parallel path R29 exists to refuse.
  wire [GEOM_SIDE_W-1:0] mf_r_side, af_s_side;
  /* verilator lint_on UNUSEDSIGNAL */
  /* verilator lint_off UNUSEDSIGNAL */
  wire [ 7:0] mf_r_flags;
  /* verilator lint_on UNUSEDSIGNAL */
  wire [31:0] mf_refused [7];

  // ---- the guard requesters, and the one client they share -----------------
  zhao_guard_req_t mf_guard_req, af_guard_req, ma_m_req, dj_guard_req;
  zhao_guard_rsp_t mf_guard_rsp, af_guard_rsp, ma_m_rsp, dj_guard_rsp;
  wire        mf_beat_valid, af_beat_valid, ma_m_beat_valid, dj_beat_valid;
  wire [63:0] mf_beat_data,  af_beat_data,  ma_m_beat_data,  dj_beat_data;
  wire        mf_beat_last,  af_beat_last,  ma_m_beat_last,  dj_beat_last;

  // ---- GEOM.DRAWJOB -> GEOM.MESHFETCH: the job, whole (R29) ---------------
  wire                  dj_j_valid, dj_j_ready, dj_d_ready;
  wire [15:0]           dj_j_instance_id;
  wire [26:0]           dj_j_desc_addr;
  wire [ 7:0]           dj_j_format;
  wire [15:0]           dj_j_generation;
  wire [ 1:0]           dj_j_active_mask;
  wire signed [31:0]    dj_j_xform [12];
  wire [31:0]           dj_j_stream_base;
  wire [GEOM_SIDE_W-1:0] dj_j_side;
  wire [31:0]           dj_refused [9];
  // ---- GEOM.LOOM -> the instance transform palette -------------------------
  wire               lm_out_valid;
  wire [9:0]         lm_out_node;
  wire signed [31:0] lm_out_m [12];
  wire [31:0]        lm_refused [6];
  /* verilator lint_off UNUSEDSIGNAL */
  // GEOM.LOOM's stream identity and end marker. The palette is keyed by node
  // index alone, so it reads neither. They are exported by no port BECAUSE
  // their consumer is GEOM.WARP (owner ruling R7), which is not composed --
  // and a stream's `last` is not the palette's business in any case.
  wire [15:0]        lm_out_src_id;
  wire               lm_out_last;
  wire               lm_refuse_valid;
  wire [2:0]         lm_refuse_reason;
  wire [9:0]         lm_refuse_node;
  wire [15:0]        lm_refuse_src;
  wire [15:0]        lm_nodes_max, lm_depth_max;
  wire [31:0]        lm_kind_hist [10];
  wire [31:0]        lm_stall_cycles;
  /* verilator lint_on UNUSEDSIGNAL */

  // ---- GEOM.ASSETFETCH -> GEOM.VDECODE: the 32-byte vertex record ----------
  wire         af_v_valid, af_v_ready;
  wire [255:0] af_v_bytes;
  wire [15:0]  af_v_src_id;

  // ---- GEOM.ASSETFETCH <-> GEOM.ASSEMBLE -----------------------------------
  wire        af_s_valid, af_s_ready;
  wire [ 7:0] af_s_vertex_count, af_s_triangle_count;
  wire [15:0] af_s_src_id;
  wire [ 1:0] af_s_visible_mask;
  wire [15:0] af_s_material_id;
  wire        asm_ix_req, af_ix_valid;
  wire [ 8:0] asm_ix_index;
  wire [ 7:0] af_ix_a, af_ix_b, af_ix_c;

  // ==========================================================================
  // GEOM.LOOM and GEOM.DRAWJOB -- the draw becomes jobs (I36, I41; R29)
  // ==========================================================================
  // THE CHAIN, end to end, with no wire that arrives on a second path:
  //
  //   DrawForm 0x0300 -> CMD.EXEC's draw arm -> GEOM.DRAWJOB
  //        form      -> the MESH_STREAM residency row (MEM.UPLOAD's own
  //                     publication, `spec/memory_rules.md` 5f.1, kind 12)
  //                  -> that page's frozen 64-byte header, read through
  //                     requester D of the geometry adapter
  //                  -> desc_addr = base + desc_offset + 64*i
  //        transform -> the instance transform palette, written by GEOM.LOOM
  //        flags     -> the raster word (R28) -> carried with every meshlet
  //   -> GEOM.MESHFETCH -> GEOM.ASSETFETCH -> GEOM.ASSEMBLE -> GEOM.REPLAY
  //   -> GEOM.CLIP's cull mode
  //
  // A DRAW WAITS WHILE A PUBLICATION IS IN FLIGHT, and this is the one
  // composition rule here that is not in a block. MEM.UPLOAD publishes its row
  // only when the last byte is written and the CRC has verified, so a draw that
  // resolved mid-publication would be told NOT RESIDENT about bytes that are
  // arriving. That is a false refusal with a real counter, which is worse than
  // a wait. It is NOT a stall on missing residency: a resource nobody is
  // publishing still refuses, immediately, and is counted.
  // `zhao_mem_upload` always answers (done_o pulses on every retired request,
  // refused or not), so the wait is bounded by one upload.
  logic upl_inflight_q;
  always_ff @(posedge gpu_clk or negedge rst_n) begin
    if (!rst_n) upl_inflight_q <= 1'b0;
    else if (cmd_upl_valid && cmd_upl_ready) upl_inflight_q <= 1'b1;
    else if (upl_done_o) upl_inflight_q <= 1'b0;
  end
  wire publication_in_flight_c = cmd_upl_valid || upl_inflight_q;

  // The .zpak resource kind whose pages this path reads (spec/cartridge.md 3).
  localparam logic [7:0] GEOM_KIND_MESH_STREAM = 8'd12;

  zhao_geom_loom u_geom_loom (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // I50, BOUNDARY: the ARM's topologically sorted node stream and the frame's
    // camera basis. The 2026-08-31 6.4 ruling puts both outside the console.
    .in_valid_i       (geom_loom_valid_i),
    .in_ready_o       (geom_loom_ready_o),
    .in_node_index_i  (geom_loom_node_index_i),
    .in_parent_index_i(geom_loom_parent_index_i),
    .in_kind_i        (geom_loom_kind_i),
    .in_param_i       (geom_loom_param_i),
    .in_angle_i       (geom_loom_angle_i),
    .in_axis_i        (geom_loom_axis_i),
    .in_bodypatch_i   (geom_loom_bodypatch_i),
    .in_src_id_i      (geom_loom_src_id_i),
    .in_first_i       (geom_loom_first_i),
    .in_last_i        (geom_loom_last_i),
    .cam_basis_i      (geom_loom_cam_basis_i),

    // REAL: the composed world transforms, into the palette. The palette is a
    // WRITE PORT that never stalls (one clock, one row), so `out_ready_i` is
    // constant -- and that is a statement about this consumer, not a tie-off:
    // GEOM.WARP (owner ruling R7) will fork this stream when it is built, and
    // THEN the ready becomes an AND.
    .out_valid_o     (lm_out_valid),
    .out_ready_i     (1'b1),
    .out_node_index_o(lm_out_node),
    .out_m_o         (lm_out_m),
    .out_src_id_o    (lm_out_src_id),
    .out_last_o      (lm_out_last),

    .refuse_valid_o     (lm_refuse_valid),
    .refuse_reason_o    (lm_refuse_reason),
    .refuse_node_index_o(lm_refuse_node),
    .refuse_src_id_o    (lm_refuse_src),

    .nodes_transformed_o   (geom_loom_nodes_o),
    .streams_composed_o    (geom_loom_streams_o),
    .streams_refused_o     (lm_refused),
    .nodes_per_stream_max_o(lm_nodes_max),
    .chain_depth_max_o     (lm_depth_max),
    .node_kind_hist_o      (lm_kind_hist),
    .consumer_stall_cycles_o(lm_stall_cycles)
  );

  // The six refusal rows, split by REASON in the block's own order, for the
  // reason GEOM.MESHFETCH's seven are split: one counter for six causes names
  // none of them.
  assign geom_loom_refused_sorted_o   = lm_refused[0];
  assign geom_loom_refused_parent_o   = lm_refused[1];
  assign geom_loom_refused_overflow_o = lm_refused[2];
  assign geom_loom_refused_kind_o     = lm_refused[3];
  assign geom_loom_refused_shear_o    = lm_refused[4];
  assign geom_loom_refused_framing_o  = lm_refused[5];

  zhao_geom_drawjob #(
    .DIR_SETS (4),
    .XFORMS   (GEOM_XFORMS),
    .LOOM_IDXW(10),
    .SIDEW    (GEOM_SIDE_W)
  ) u_geom_drawjob (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: CMD.EXEC's ratified DrawForm, held only while a publication is
    // still writing the directory this block reads.
    .d_valid_i          (cmd_draw_valid_w && !publication_in_flight_c),
    .d_ready_o          (dj_d_ready),
    .d_form_i           (cmd_draw_form_w),
    .d_material_set_i   (cmd_draw_material_set_w),
    .d_transform_i      (cmd_draw_transform_w),
    .d_viewport_mask_i  (cmd_draw_viewport_mask_w),
    .d_semantic_weight_i(cmd_draw_semantic_weight_w),
    .d_flags_i          (cmd_draw_flags_w),
    .d_src_id_i         (cmd_draw_src_id_w),

    // REAL: MEM.UPLOAD's own publication, the 5f.1 row, for MESH_STREAM pages.
    // The same publication drives MATERIAL.RESOLVE's directory for kind 11:
    // one rule, two consumers, each taking the kind it owns.
    .dir_we_i        (upl_publish_valid_o && (upl_publish_tag_o == GEOM_KIND_MESH_STREAM)),
    .dir_entry_i     (upl_publish_slot_o),
    .dir_index_i     (upl_publish_index_o),
    .dir_generation_i(upl_publish_generation_o),
    .dir_base_i      (upl_publish_base_o),
    .dir_extent_i    (upl_publish_extent_o),

    // REAL: GEOM.LOOM's composed world transforms.
    .px_valid_i(lm_out_valid),
    .px_index_i(lm_out_node),
    .px_m_i    (lm_out_m),

    // REAL: requester D of the shared ENGINE1 client, for the page header.
    .client_i    (GEOM_ASSET_CLIENT_C),
    .guard_req_o (dj_guard_req),
    .guard_rsp_i (dj_guard_rsp),
    .beat_valid_i(dj_beat_valid),
    .beat_data_i (dj_beat_data),
    .beat_last_i (dj_beat_last),
    // REAL: the SAME walker law the descriptor fetch uses, over the SAME beats
    // this block receives -- a second instance of one implementation, not a
    // second implementation.
    .crc_ok_i    (dj_crc_ok),

    // REAL: the job, into GEOM.MESHFETCH.
    .j_valid_o      (dj_j_valid),
    .j_ready_i      (dj_j_ready),
    .j_instance_id_o(dj_j_instance_id),
    .j_desc_addr_o  (dj_j_desc_addr),
    .j_format_o     (dj_j_format),
    .j_generation_o (dj_j_generation),
    .j_active_mask_o(dj_j_active_mask),
    .j_xform_o      (dj_j_xform),
    .j_stream_base_o(dj_j_stream_base),
    .j_side_o       (dj_j_side),

    .draws_o      (geom_dj_draws_o),
    .jobs_o       (geom_dj_jobs_o),
    .masked_o     (geom_dj_masked_o),
    .empty_o      (geom_dj_empty_o),
    .pal_writes_o (geom_dj_pal_writes_o),
    .pal_dropped_o(geom_dj_pal_dropped_o),
    .refused_o    (dj_refused)
  );

  // GEOM.DRAWJOB's header CRC walker: the descriptor's own, on requester D's
  // beats. The header was given the descriptor's framing precisely so this
  // block could read it unmodified.
  wire dj_crc_ok;
  zhao_geom_desc_crc u_dj_hdr_crc (
    .clk          (gpu_clk),
    .rst_n        (rst_n),
    .beat_valid_i (dj_beat_valid),
    .beat_data_i  (dj_beat_data),
    .beat_last_i  (dj_beat_last),
    .crc_ok_o     (dj_crc_ok),
    .descriptors_o(geom_dj_hdr_reads_o),
    .crc_fail_o   (geom_dj_hdr_crc_fail_o),
    .framing_err_o(geom_dj_hdr_framing_o)
  );

  // The draw stream's ready carries the same hold as its valid, so a held
  // draw is not accepted by one side of the handshake and refused by the other.
  assign cmd_draw_ready_w = dj_d_ready && !publication_in_flight_c;

  assign geom_dj_refused_cull_o     = dj_refused[0];
  assign geom_dj_refused_resident_o = dj_refused[1];
  assign geom_dj_refused_stale_o    = dj_refused[2];
  assign geom_dj_refused_xform_o    = dj_refused[3];
  assign geom_dj_refused_denied_o   = dj_refused[4];
  assign geom_dj_refused_format_o   = dj_refused[5];
  assign geom_dj_refused_crc_o      = dj_refused[6];
  assign geom_dj_refused_reserved_o = dj_refused[7];
  assign geom_dj_refused_layout_o   = dj_refused[8];

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

    // I36, CLOSED 2026-09-20: the DRAW, from GEOM.DRAWJOB. Every field is a
    // resolved value with a ruling behind it (R29), not a port at the edge.
    .j_valid_i      (dj_j_valid),
    .j_ready_o      (dj_j_ready),
    .j_instance_id_i(dj_j_instance_id),
    .j_desc_addr_i  (dj_j_desc_addr),
    .j_format_i     (dj_j_format),
    .j_generation_i (dj_j_generation),
    .j_active_mask_i(dj_j_active_mask),
    .j_xform_i      (dj_j_xform),
    // R29: the page's pool-relative base, added to the descriptor's
    // PAGE-relative offsets once, here; and the draw's own state, carried.
    .j_stream_base_i(dj_j_stream_base),
    .j_side_i       (dj_j_side),
    .j_client_i     (GEOM_ASSET_CLIENT_C),   // I40: assigned here

    // REAL: requester A of the shared ENGINE1 client.
    .guard_req_o (mf_guard_req),
    .guard_rsp_i (mf_guard_rsp),
    .beat_valid_i(mf_beat_valid),
    .beat_data_i (mf_beat_data),
    .beat_last_i (mf_beat_last),

    // REAL: the descriptor's CRC verdict, from the walker below over the SAME
    // beats this block receives.  Entry I37, CLOSED.
    .crc_ok_i(mf_crc_ok),

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
    // R29: the draw's state, out in the SAME handshake as the meshlet.
    .r_side_o          (mf_r_side),

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
  // GEOM.MESHFETCH's DESCRIPTOR CRC WALKER.  Entry I37, CLOSED 2026-09-19.
  //
  // It taps requester A's beat stream, the one `u_geom_mem_adapter` hands
  // `u_geom_meshfetch`: the same three nets, not a copy, so the bytes the
  // CRC is computed over are the bytes the fetcher stores. It does NOT sit in
  // the stream and cannot stall it (beats have no ready). The fetcher latches
  // the verdict on the last beat, which is exactly when this block presents
  // it. `zhao_crc32c_fold` is the one fold law; this is a walker around two
  // instances of it, not a second CRC.
  // --------------------------------------------------------------------------
  wire mf_crc_ok;

  zhao_geom_desc_crc u_geom_desc_crc (
    .clk          (gpu_clk),
    .rst_n        (rst_n),
    .beat_valid_i (mf_beat_valid),
    .beat_data_i  (mf_beat_data),
    .beat_last_i  (mf_beat_last),
    .crc_ok_o     (mf_crc_ok),
    .descriptors_o(geom_mf_crc_descriptors_o),
    .crc_fail_o   (geom_mf_crc_fail_o),
    .framing_err_o(geom_mf_crc_framing_o)
  );

  // --------------------------------------------------------------------------
  // MATERIAL.RESOLVE (R20), composed 2026-09-19 (cmdmem packet)
  // --------------------------------------------------------------------------
  // THREE OF ITS FOUR SEAMS ARE REAL HERE, and the fourth is entry I49.
  //
  //   DIRECTORY <- MEM.UPLOAD's publication, `spec/memory_rules.md` 5f.1's row,
  //     for resources of kind MATERIAL_SET (`spec/cartridge.md` 4 kind 11)
  //     and nothing else: {index -> set_index, generation, base}, the arena
  //     slot as the directory entry, and the record COUNT as extent / 32 --
  //     the frozen record stride (`spec/commands.zidl` MaterialRecord, 32 B).
  //     A count above what a 16-bit material_id can address saturates at
  //     65,536, which loses nothing: no id can name a record past it. A slot at
  //     or above SETS is not directory-visible and resolves as NOT_RESIDENT,
  //     which is COUNTED -- a named fault, never a silent miss.
  //   FETCH <- requester C of `u_geom_mem_adapter`, the ENGINE1 mux 5f says
  //     texture reads join. A refused read (`violation`, no beats) is the new
  //     `mem_rsp_denied_i` and resolves to kFetchDenied: counted, never a hang.
  //   REQUEST / RESPONSE -> entry I49.
  //
  // THE SHIM BELOW IS FIELD MAPPING, not arbitration: a record is one 32-byte
  // read, so `len` is the frozen record size; `client` and `write` are forced
  // inside the adapter anyway. The 27-bit guard address is the low bits of the
  // resolver's 32-bit one, the same narrowing MEM.UPLOAD's own guard request
  // makes -- VRAM is 2^27 bytes and every region either block may name is
  // inside it.
  localparam logic [7:0] MAT_KIND_MATERIAL_SET = 8'd11;   // cartridge.md 4, kind 11
  // R42: the species descriptor page. cartridge.md 4, kind 13, allocated by
  // that ruling. Named here rather than repeated as a literal, so the console
  // and the loader cannot disagree about which publication is a table.
  localparam logic [7:0] PART_KIND_SPECIES_TABLE = 8'd13;

  zhao_guard_req_t mr_guard_req;
  zhao_guard_rsp_t mr_guard_rsp;
  logic            mr_beat_valid;
  logic [63:0]     mr_beat_data;
  /* verilator lint_off UNUSEDSIGNAL */
  // `last` is not needed: the resolver counts its own four beats. `ok` is the
  // acceptance verdict; only a DENIAL changes what the resolver does.
  logic            mr_beat_last;
  wire             mr_rsp_ok_unused = mr_guard_rsp.ok;
  // Address bits above VRAM's 27. The directory's base is MEM.UPLOAD's own
  // published destination, which that block bounds-checks in 32 bits against a
  // HOST-configured region and then writes through the same 27-bit
  // truncation -- so a region configured above 2^27 would ALIAS in both
  // blocks, consistently. That is a MEM.UPLOAD configuration hazard recorded in
  // the cmdmem findings, not something this shim can refuse without a status
  // the oracle does not have.
  wire [4:0]       mr_addr_hi_unused = mr_mem_req_addr[31:27];
  /* verilator lint_on UNUSEDSIGNAL */
  logic            mr_mem_req_valid;
  logic [31:0]     mr_mem_req_addr;

  always_comb begin
    mr_guard_req        = '0;
    mr_guard_req.valid  = mr_mem_req_valid;
    mr_guard_req.write  = 1'b0;
    mr_guard_req.client = ZHAO_CLIENT_ENGINE1;
    mr_guard_req.addr   = mr_mem_req_addr[ZHAO_VRAM_ADDR_BITS-1:0];
    mr_guard_req.len    = 7'd32;
    // EXACTLY the record's 32 byte lanes. MEM.GUARD's shape rule demands a
    // mask equal to the length's (`shape_ok`); an all-ones mask on a 32-byte
    // read was refused on SHAPE, which hid behind the region refusal until
    // R32 removed that one -- two reasons for one denial, agreeing.
    mr_guard_req.be     = 64'h0000_0000_FFFF_FFFF;
  end

  wire [26:0] mr_extent_records = upl_publish_extent_o[31:5];

  // Entry I49's seam, now internal. `u_material_window` is instantiated with
  // GEOM.REPLAY, several thousand lines below; these are declared here because
  // this is where they are CONSUMED and `default_nettype none` wants them
  // before their first use.
  wire        mat_req_valid_c, mat_req_ready_c, mat_rsp_ready_c;
  wire [31:0] mat_req_material_set_c;
  wire [15:0] mat_req_material_id_c;
  wire [ 7:0] mat_req_quality_tier_c;

  zhao_material_resolve u_material_resolve (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    .dir_we_i         (upl_publish_valid_o && (upl_publish_tag_o == MAT_KIND_MATERIAL_SET)),
    .dir_entry_i      (upl_publish_slot_o),
    .dir_valid_i      (1'b1),
    .dir_set_index_i  (upl_publish_index_o),
    .dir_generation_i (upl_publish_generation_o),
    .dir_base_i       (upl_publish_base_o),
    .dir_count_i      ((mr_extent_records > 27'd65536) ? 17'd65536 : mr_extent_records[16:0]),

    // I49, CLOSED: the REQUEST, from `u_material_window`. Both halves are
    // fields of ONE triangle record, not two live wires paired by timing.
    .req_valid_i        (mat_req_valid_c),
    .req_ready_o        (mat_req_ready_c),
    .req_material_set_i (mat_req_material_set_c),
    .req_material_id_i  (mat_req_material_id_c),
    .req_quality_tier_i (mat_req_quality_tier_c),

    .mem_req_valid_o  (mr_mem_req_valid),
    .mem_req_ready_i  (mr_guard_rsp.ready),
    .mem_req_addr_o   (mr_mem_req_addr),
    .mem_rsp_valid_i  (mr_beat_valid),
    .mem_rsp_data_i   (mr_beat_data),
    .mem_rsp_denied_i (mr_guard_rsp.violation),

    .rsp_valid_o            (mat_rsp_valid_o),
    .rsp_ready_i            (mat_rsp_ready_c),
    .rsp_status_o           (mat_rsp_status_o),
    .rsp_has_record_o       (mat_rsp_has_record_o),
    .rsp_record_o           (mat_rsp_record_o),
    .rsp_quality_tier_o     (mat_rsp_quality_tier_o),
    .rsp_sample_count_o     (mat_rsp_sample_count_o),
    .rsp_material_recipe_o  (mat_rsp_material_recipe_o),
    .rsp_recipe_weight_o    (mat_rsp_recipe_weight_o),
    .rsp_base_binding_o     (mat_rsp_base_binding_o),
    .rsp_selector_overflow_o(mat_rsp_selector_overflow_o),
    .rsp_palette_base_o     (mat_rsp_palette_base_o),
    .rsp_raster_state_o     (mat_rsp_raster_state_o),
    .rsp_flags_o            (mat_rsp_flags_o),
    .rsp_sample0_modes_o    (mat_rsp_sample0_modes_o),
    .rsp_sample1_modes_o    (mat_rsp_sample1_modes_o),
    .rsp_sample2_modes_o    (mat_rsp_sample2_modes_o),

    .material_hits_o        (mat_hits_o),
    .material_misses_o      (mat_misses_o),
    .material_refused_o     (mat_refused_o),
    .refused_id_o           (mat_refused_id_o),
    .refused_record_o       (mat_refused_record_o),
    .not_resident_o         (mat_not_resident_o),
    .selector_overflow_o    (mat_selector_overflow_o),
    .recipe_count_mismatch_o(mat_recipe_count_mismatch_o),
    .fetch_denied_o         (mat_fetch_denied_o)
  );
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

    // REAL: requester C, MATERIAL.RESOLVE's 32-byte record fetch (R20).
    .c_req_i       (mr_guard_req),
    .c_rsp_o       (mr_guard_rsp),
    .c_beat_valid_o(mr_beat_valid),
    .c_beat_data_o (mr_beat_data),
    .c_beat_last_o (mr_beat_last),

    // REAL: requester D, GEOM.DRAWJOB's 64-byte MESH_STREAM header (R29). One
    // read per DRAW, against A's one per meshlet, so it is the lightest of the
    // four and the round robin's bound is unchanged in kind.
    // REAL (I33 closed, R42): requester E, PART.TABLE's species-page loader.
    .e_req_i       (ptl_guard_req),
    .e_rsp_o       (ptl_guard_rsp),
    .e_beat_valid_o(ptl_beat_valid),
    .e_beat_data_o (ptl_beat_data),
    .e_beat_last_o (ptl_beat_last),

    .d_req_i       (dj_guard_req),
    .d_rsp_o       (dj_guard_rsp),
    .d_beat_valid_o(dj_beat_valid),
    .d_beat_data_o (dj_beat_data),
    .d_beat_last_o (dj_beat_last),

    // REAL: the one permitted client, into the shell's MEM.GUARD socket.
    .m_req_o      (ma_m_req),
    .m_rsp_i      (ma_m_rsp),
    .m_beat_valid_i(ma_m_beat_valid),
    .m_beat_data_i (ma_m_beat_data),
    .m_beat_last_i (ma_m_beat_last),

    .jobs_a_o     (geom_ma_jobs_a_o),
    .jobs_b_o     (geom_ma_jobs_b_o),
    .jobs_c_o     (geom_ma_jobs_c_o),
    .jobs_d_o     (geom_ma_jobs_d_o),
    .jobs_e_o     (geom_ma_jobs_e_o),
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
    // REAL, and CARRIED: the descriptor's visible mask and material, captured
    // with the counts in this handshake and offered again on `s_*`.
    .m_visible_mask_i  (mf_r_visible_mask),
    .m_material_id_i   (mf_r_material_id),
    // R29: the draw's raster word, material set and weight, in the same
    // handshake as the counts -- the meshlet and its draw cannot separate.
    .m_side_i          (mf_r_side),
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
    .s_visible_mask_o  (af_s_visible_mask),
    .s_material_id_o   (af_s_material_id),
    .s_side_o          (af_s_side),

    // REAL: the release, from GEOM.REPLAY, which PROVES both readers are done
    // (its header). Entry I38, CLOSED.
    .release_i(rp_af_release),

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

  // ---- GEOM.ASSEMBLE <-> GEOM.REPLAY, and the dispatcher fork ---------------
  // Arena-local vertex ids: GEOM.GROUP_SEQ fills vertex i of a meshlet at arena
  // index i in EVERY view, so the per-view base is the arena handle and the
  // offset is zero BY CONSTRUCTION. Named, for the reason I25 gives: the day a
  // shared vertex pool replaces per-meshlet arenas, this is the line to change.
  localparam logic [GEOM_ASM_VIDW-1:0] GEOM_ASM_VOFF_C = '0;

  wire                     asm_m_valid, asm_m_ready, rp_mt_ready;
  wire                     asm_t_valid, asm_t_ready, asm_m_done;
  // `t_last_o` is not read: it rides an EMITTED triangle, so a refused last
  // triplet or an empty meshlet ends the walk without one. GEOM.REPLAY ends a
  // meshlet on `m_done_o`, which fires on every ending.
  /* verilator lint_off UNUSEDSIGNAL */
  wire                     asm_t_last;
  /* verilator lint_on UNUSEDSIGNAL */
  wire [GEOM_ASM_VIDW-1:0] asm_t_v0, asm_t_v1, asm_t_v2;
  wire [15:0]              asm_t_material, asm_t_src_id;
  wire [31:0]              asm_t_raster;
  // R29's OTHER sideband half, now CARRIED rather than dropped: the draw's
  // MATERIAL_SET handle32, on the same handshake and the same latch as the
  // meshlet's material id. Entry I49.
  wire [31:0]              asm_t_material_set;
  wire [ 7:0]              asm_t_quality_tier;

  zhao_geom_assemble #(
    .MAX_VERTICES  (GEOM_ASSET_MAX_VERTICES),
    .MAX_TRIANGLES (GEOM_ASSET_MAX_TRIANGLES),
    .VIDW          (GEOM_ASM_VIDW),
    .SRCW          (16)
  ) u_geom_assemble (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: the meshlet, from GEOM.ASSETFETCH's servable port THROUGH THE
    // DISPATCHER FORK below. The counts come from the block that has BUFFERED
    // the footprint, so they hold still for the whole walk by construction.
    .m_valid_i         (asm_m_valid),
    .m_ready_o         (asm_m_ready),
    .m_vertex_count_i  (af_s_vertex_count),
    .m_triangle_count_i(af_s_triangle_count),
    .m_src_id_i        (af_s_src_id),

    // ASSIGNED, not tied off: arena-local vertex ids (GEOM.REPLAY's header).
    .m_vertex_offset_i(GEOM_ASM_VOFF_C),
    // REAL: the material, carried beside the meshlet by GEOM.ASSETFETCH.
    .m_material_id_i  (af_s_material_id),
    // I39, CLOSED 2026-09-20: the RASTER WORD, off the sideband GEOM.ASSETFETCH
    // carries beside THIS meshlet. R28's layout: [1:0] the draw's cull mode,
    // [31:2] the material's half, which every v1 material writes as zero and
    // GEOM.DRAWJOB names as a constant at the seam a resolved record replaces.
    .m_raster_state_i (af_s_side[31:0]),
    // I49: the DRAW's MATERIAL_SET, [63:32] of the same sideband word. It is
    // offered on the SAME handshake as f_s_material_id above, which is why
    // the two halves of MATERIAL.RESOLVE's request are joined by construction.
    .m_material_set_i (af_s_side[63:32]),
    // I49: the draw's SEMANTIC WEIGHT, [71:64] of the same word -- the Measure
    // policy's degrade order, which travels as the resolve's quality tier.
    .m_quality_tier_i (af_s_side[71:64]),

    // REAL: the index service.  `ix_valid_i` follows the FETCHER's valid and is
    // not tied to the request -- the served answer has a real valid, and tying
    // it high would turn a missed answer into a silently wrong triplet.
    .ix_req_o  (asm_ix_req),
    .ix_index_o(asm_ix_index),
    .ix_valid_i(af_ix_valid),
    .ix_a_i    (af_ix_a),
    .ix_b_i    (af_ix_b),
    .ix_c_i    (af_ix_c),

    // REAL: the TriangleDescriptor, into GEOM.REPLAY, and the walk's END.
    .t_valid_o  (asm_t_valid),
    .t_ready_i  (asm_t_ready),
    .t_v0_o     (asm_t_v0),
    .t_v1_o     (asm_t_v1),
    .t_v2_o     (asm_t_v2),
    .t_material_o(asm_t_material),
    .t_raster_o (asm_t_raster),
    .t_material_set_o(asm_t_material_set),
    .t_quality_tier_o(asm_t_quality_tier),
    .t_src_id_o (asm_t_src_id),
    .t_last_o   (asm_t_last),
    .m_done_o   (asm_m_done),

    .meshlets_o      (geom_asm_meshlets_o),
    .triangles_o     (geom_asm_triangles_o),
    .refused_limits_o(geom_asm_refused_limits_o),
    .refused_index_o (geom_asm_refused_index_o)
  );

  // --------------------------------------------------------------------------
  // THE MESHLET DISPATCHER FORK. GEOM.ASSETFETCH offers ONE servable meshlet;
  // THREE blocks must take it on the same clock: GEOM.GROUP_SEQ (its vertices
  // become an arena group per visible view), GEOM.ASSEMBLE (its index walk)
  // and GEOM.REPLAY (the token that says how many handles to wait for). An
  // AND-fork, the shape this file already uses twice: one ready is the AND of
  // all three, and each consumer's valid is gated by the OTHER two readies.
  // None of the three readies is a function of its own valid -- each is a
  // state or occupancy decode (StIdle, S_IDLE, "a meshlet slot is free") -- so
  // it cannot deadlock, and the three accept the SAME meshlet or none does.
  //
  // OWNER RULING R57 (2026-09-20): THE FORK IS THE SAME AND THE READIES MOVED.
  // GEOM.REPLAY's `mt_ready_o` used to be `st_q == S_IDLE` -- busy for the whole
  // replay -- and its buffer release came only after the last triangle was
  // drawn, so GEOM.ASSETFETCH and GEOM.ASSEMBLE were held for that long too and
  // meshlet N+1's vertex phase could not begin until meshlet N's replay ended.
  // GEOM.REPLAY now carries TWO meshlet slots and a descriptor queue: it is
  // ready while a slot is free, it takes GEOM.ASSEMBLE's whole walk at
  // ASSEMBLE's rate, and it releases the asset buffer as soon as both readers
  // are proven done (its header's I38 paragraph). That is the whole overlap --
  // no wire here changed, and no second dispatcher exists.
  //
  // THE VERTEX STREAM CANNOT RACE THE JOB: GEOM.ASSETFETCH starts streaming
  // vertex records only AFTER `s_*` is accepted (its S_HAND -> S_SERVE), which
  // is this very clock, so GEOM.GROUP_SEQ already holds the job when the first
  // record reaches it.
  // --------------------------------------------------------------------------
  assign af_s_ready    = gs_job_ready && asm_m_ready && rp_mt_ready;
  assign dsp_job_valid = af_s_valid && asm_m_ready && rp_mt_ready;
  assign asm_m_valid   = af_s_valid && gs_job_ready && rp_mt_ready;
  assign dsp_job_count = GEOM_INDEX_W'(af_s_vertex_count);
  assign dsp_job_mask  = af_s_visible_mask;
  assign dsp_job_src   = af_s_src_id;

  // --------------------------------------------------------------------------
  // GEOM.REPLAY -- the replay customer entries I11 and I12 specified -- and
  // GEOM.VATTR, the vertex-attribute store it reads (entry I46, CLOSED
  // 2026-09-19; owner rulings R11 and R31). Depth is no longer computed per
  // triangle corner inside REPLAY: GEOM.VATTR computes invw24 once per LANDED
  // vertex on the streaming GEOM.DEPTHQUANT and answers it with the vertex's
  // other attributes, on the arena's own lookup nets and clock.
  // --------------------------------------------------------------------------
  wire                         va_rep_valid;
  wire [23:0]                  va_rep_invw;
  wire [GEOM_ATTR_STORE_W-1:0] va_rep_data;
  wire [23:0]                  rp_invw_a, rp_invw_b, rp_invw_c;
  wire [GEOM_ATTR_STORE_W-1:0] rp_st_a, rp_st_b, rp_st_c;
  // The triangle's VIEW rides out of GEOM.REPLAY and is read by nothing here:
  // GEOM.CLIP does not take it. Its MATERIAL and MATERIAL SET are no longer in
  // this list -- as of 2026-09-20 they are MATERIAL.RESOLVE's request, through
  // u_material_window (entry I49), and are declared with that handshake.
  /* verilator lint_off UNUSEDSIGNAL */
  wire                         rp_o_view;
  /* verilator lint_on UNUSEDSIGNAL */

  zhao_geom_replay #(
    .ARENA_W   (GEOM_ARENA_W),
    .GEN_W     (GEOM_GEN_W),
    .INDEX_W   (GEOM_INDEX_W),
    .VIDW      (GEOM_ASM_VIDW),
    .SRCW      (16),
    .PAYLOAD_W (GEOM_PAYLOAD_W),
    .ATTRW     (GEOM_ATTR_STORE_W),
    // R57: the descriptor queue holds the TWO meshlets the arena budget allows
    // in flight, so the pipeline never backpressures itself. 256 x 48 bits.
    .MAX_TRIANGLES (GEOM_ASSET_MAX_TRIANGLES),
    .TRIQ_DEPTH    (256)
  ) u_geom_replay (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: the dispatcher's token -- the meshlet's visible mask and count.
    .mt_valid_i       (af_s_valid && gs_job_ready && asm_m_ready),
    .mt_ready_o       (rp_mt_ready),
    .mt_view_mask_i   (af_s_visible_mask),
    .mt_vertex_count_i(af_s_vertex_count),

    // REAL: GEOM.GROUP_SEQ's sealed handles, and their release -- through the
    // store's batch-complete gate (see GEOM.GROUP_SEQ's instance).
    .grp_valid_i (gs_grp_valid && va_done),
    .grp_ready_o (rp_grp_ready),
    .grp_arena_i (gs_grp_arena),
    .grp_gen_i   (gs_grp_gen),
    .grp_view_i  (gs_grp_view),
    // REAL: R31 -- a batch GEOM.VDECODE refused part of is dropped here, whole.
    // R31 poison, from BOTH owners of a batch's rows: GROUP_SEQ (a refused
    // record) and GEOM.VATTR (a row dropped or outside the store) -- a batch
    // whose store rows are not all its own is dropped, never drawn.
    .grp_poison_i(gs_grp_poison || va_poison),
    .rel_valid_o (rp_rel_valid),
    .rel_arena_o (rp_rel_arena),


    // REAL: GEOM.ASSEMBLE's triangles and the end of its walk.
    .t_valid_i    (asm_t_valid),
    .t_ready_o    (asm_t_ready),
    .t_v0_i       (asm_t_v0),
    .t_v1_i       (asm_t_v1),
    .t_v2_i       (asm_t_v2),
    .t_material_i (asm_t_material),
    .t_raster_i   (asm_t_raster),
    .t_material_set_i(asm_t_material_set),
    .t_quality_tier_i(asm_t_quality_tier),
    .t_src_id_i   (asm_t_src_id),
    .m_done_i     (asm_m_done),

    // REAL: the arena, and -- on the SAME nets -- the attribute store (I46).
    .look_valid_o   (rp_look_valid),
    .look_ready_i   (ln_look_ready),
    .look_arena_o   (rp_look_arena),
    .look_gen_o     (rp_look_gen),
    .look_index_o   (rp_look_index),
    .rep_valid_i    (ln_rep_valid),
    .rep_hit_i      (ln_rep_hit),
    .rep_refuse_i   (ln_rep_refuse),
    .rep_payload_i  (ln_rep_payload),
    .att_rep_valid_i(va_rep_valid),
    .att_invw_i     (va_rep_invw),
    .att_rep_data_i (va_rep_data),

    // REAL: GEOM.ASSETFETCH's release (entry I38, closed).
    .af_release_o (rp_af_release),

    // REAL: the triangle, into GEOM.CLIP.
    .o_valid_o    (rp_o_valid),
    .o_ready_i    (rp_o_ready),
    .o_ax_o       (rp_o_ax),
    .o_ay_o       (rp_o_ay),
    .o_bx_o       (rp_o_bx),
    .o_by_o       (rp_o_by),
    .o_cx_o       (rp_o_cx),
    .o_cy_o       (rp_o_cy),
    .o_behind_o   (rp_o_behind),
    .o_invw_a_o   (rp_invw_a),
    .o_invw_b_o   (rp_invw_b),
    .o_invw_c_o   (rp_invw_c),
    .o_attr_a_o   (rp_st_a),
    .o_attr_b_o   (rp_st_b),
    .o_attr_c_o   (rp_st_c),
    .o_view_o     (rp_o_view),
    .o_src_id_o   (rp_o_src_id),
    .o_material_o (rp_o_material),
    .o_raster_o   (rp_o_raster),
    .o_material_set_o(rp_o_material_set),
    .o_quality_tier_o(rp_o_quality_tier),

    .meshlets_o      (geom_rp_meshlets_o),
    .groups_o        (geom_rp_groups_o),
    .triangles_in_o  (geom_rp_triangles_in_o),
    .triangles_out_o (geom_rp_triangles_out_o),
    .refused_o       (geom_rp_refused_o),
    .missed_o        (geom_rp_missed_o),
    .att_skew_o      (geom_rp_att_skew_o),
    .view_bad_o      (geom_rp_view_bad_o),
    .poisoned_o      (geom_rp_poisoned_o),
    .triq_stall_o    (geom_rp_triq_stall_o)
  );

  // ==========================================================================
  // MATERIAL.RESOLVE's WINDOW -- entry I49, CLOSED 2026-09-20 (texmat2)
  // ==========================================================================
  // `zhao_material_resolve` was composed on 2026-09-19 with its DIRECTORY
  // (MEM.UPLOAD's 5f.1 publication) and its FETCH (requester C of the ENGINE1
  // adapter) both real, and entry I49 recorded what was left: "a request must
  // be issued per meshlet (or per triangle) and its answer joined back to the
  // triangles that asked". `u_material_window` is that, and the five ports
  // `mat_req_valid_i`, `mat_req_material_set_i`, `mat_req_material_id_i`,
  // `mat_req_quality_tier_i` and `mat_rsp_ready_i` have LEFT this module's port
  // list rather than being driven from constants -- the same retirement
  // `tri_area2_i` and the three attribute planes took.
  //
  // THE REQUEST'S TWO HALVES ARE THE TRIANGLE'S OWN. `rp_o_material_set` and
  // `rp_o_material` are both per-meshlet fields of the triangle GEOM.REPLAY is
  // presenting on this very handshake. They reached it on ONE path -- the
  // draw's sideband through GEOM.DRAWJOB, GEOM.MESHFETCH, GEOM.ASSETFETCH (on
  // the same handshake as the meshlet's own material id), GEOM.ASSEMBLE and
  // GEOM.REPLAY, latched by the same enable at every stage. That is what entry
  // I39 demanded and what entry I20's third seam said did not yet exist:
  // "TWO LIVE WIRES ARE NOT A PRODUCER". These are not two live wires; they are
  // two fields of one record.
  //
  // THE QUALITY TIER is the draw's SEMANTIC WEIGHT, `mf_r_side[71:64]`, carried
  // beside the meshlet for exactly this reason (its declaration says so: "the
  // weight is the Measure policy's degrade order"). `zhao_material_resolve`
  // ECHOES the tier and reads it nowhere, so this is a label travelling with
  // its request rather than a policy invented at a composer.
  //
  // THE DOWNSTREAM SPAN the window protects is GEOM.CLIP's input to the shell's
  // triangle door, and its three disposal events are all real nets of this
  // module:
  //
  //   d_enter_i   GEOM.CLIP accepted a triangle  (mw_t_valid && mw_t_ready)
  //   d_reject_i  GEOM.CLIP retired one with a non-ACCEPT verdict -- its own
  //               `ret_valid_o` / `ret_verdict_o`, which fire once per
  //               submitted triangle, so a dropped triangle is never lost to
  //               the accounting
  //   d_leave_i   the door took one   (door_tri_valid_w && door_tri_ready_w)
  //
  // There is no fourth outcome for a triangle in that span, so the occupancy
  // always returns to zero and the drain always completes. The window's own
  // header carries the full argument and its two structural guards.
  wire mw_clip_reject_c = geom_clip_ret_valid_o && (geom_clip_ret_verdict_o != 3'd0);

  zhao_material_window #(
    // The default mapping: the material record's tmu_mode IS the binding
    // resolver's response class. See the block's header and FINDINGS-texmat2 --
    // the ENCODING is an owner decision and this parameter is where it lives.
    .TMU_MODE_CLASS (8'b11_10_01_00),
    .OCCW           (8)
  ) u_material_window (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    // REAL: GEOM.REPLAY's triangle handshake, and the triangle's own material.
    .t_valid_i        (rp_o_valid),
    .t_ready_o        (rp_o_ready),
    .t_material_set_i (rp_o_material_set),
    .t_material_id_i  (rp_o_material),
    .t_quality_tier_i (rp_o_quality_tier),

    // REAL: into GEOM.CLIP, gated.
    .t_valid_o (mw_t_valid),
    .t_ready_i (mw_t_ready),

    // REAL: the span's three disposal events.
    .d_enter_i  (mw_t_valid && mw_t_ready),
    .d_reject_i (mw_clip_reject_c),
    .d_leave_i  (door_tri_valid_w && door_tri_ready_w),

    // REAL: MATERIAL.RESOLVE, whose request had no producer until now.
    .req_valid_o        (mat_req_valid_c),
    .req_ready_i        (mat_req_ready_c),
    .req_material_set_o (mat_req_material_set_c),
    .req_material_id_o  (mat_req_material_id_c),
    .req_quality_tier_o (mat_req_quality_tier_c),
    .rsp_valid_i        (mat_rsp_valid_o),
    .rsp_ready_o        (mat_rsp_ready_c),
    .rsp_status_i       (mat_rsp_status_o),
    .rsp_has_record_i   (mat_rsp_has_record_o),
    .rsp_sample_count_i (mat_rsp_sample_count_o),
    .rsp_material_recipe_i  (mat_rsp_material_recipe_o),
    .rsp_recipe_weight_i    (mat_rsp_recipe_weight_o),
    .rsp_base_binding_i     (mat_rsp_base_binding_o),
    .rsp_selector_overflow_i(mat_rsp_selector_overflow_o),
    .rsp_sample0_modes_i    (mat_rsp_sample0_modes_o),

    // REAL: the published material, into the flat request below.
    .pub_valid_o           (mw_pub_valid),
    .pub_sample_count_o    (mw_pub_sample_count),
    .pub_material_recipe_o (mw_pub_material_recipe),
    .pub_recipe_weight_o   (mw_pub_recipe_weight),
    .pub_base_binding_o    (mw_pub_base_binding),
    .pub_response_class_o  (mw_pub_response_class),

    .resolves_o                (mat_win_resolves_o),
    .switches_o                (mat_win_switches_o),
    .drain_stall_cycles_o      (mat_win_drain_stall_o),
    .answer_stall_cycles_o     (mat_win_answer_stall_o),
    .occupancy_max_o           (mat_win_occupancy_max_o),
    .no_record_o               (mat_win_no_record_o),
    .selector_overflow_o       (mat_win_selector_overflow_o),
    .clut_unowned_o            (mat_win_clut_unowned_o),
    .err_unpublished_o         (mat_win_err_unpublished_o),
    .err_occupancy_underflow_o (mat_win_err_underflow_o)
  );

  // --------------------------------------------------------------------------
  // THE FLAT REQUEST -- entry I20's `tri_flat_request_i`, driven from here
  // --------------------------------------------------------------------------
  // `zhao_texture_v3_request_v2_t`'s 298 bits, field by field, with the OWNER
  // of each named. Nothing below is a plausible constant chosen to make a
  // picture: every field is either the resolved record's, or a LAW, or a named
  // editable constant with a ruling behind it.
  //
  //   [297:296] sample_count           MATERIAL.RESOLVE  (the record's)
  //   [295:288] base_binding_selector  MATERIAL.RESOLVE  (sample0.binding_slot)
  //   [287:280] lod_q4_4               THE SAMPLER's. MATERIAL.RESOLVE's
  //             contract excludes it in terms -- "it returns the mip policy;
  //             the sampler picks the level" -- and the binding row this
  //             console programs has mip_enable low, so level 0 is the only
  //             level there is. A named constant, not a guess.
  //   [279:277] material_recipe        MATERIAL.RESOLVE  (control[4:2])
  //   [276:269] recipe_weight          MATERIAL.RESOLVE  (the record's)
  //   [268]     aux_required           ZERO IS THE PROFILE, not a tie-off.
  //             MATERIAL.RESOLVE's own header says a zero aux context is "the
  //             LEGAL and CORRECT non-terrain profile", and
  //             `zhao_raster_tile_pipe_v2` REFUSES a job whose aux context is
  //             non-zero without a producer for it. The console's AUX response
  //             (`pg_*` inside the shell) has no producer either, so a fragment
  //             that asked would never retire.
  //   [267:44]  aux_surface_ctx        the same, 224 bits of it
  //   [43:20]   base_rgb               THE VERTEX's, not the material's.
  //   [19:12]   base_alpha             THE VERTEX's. Owner ruling R48: no
  //             ratified vertex format carries alpha, so OPAQUE lives in a
  //             named editable constant and nothing is stubbed.
  //   [11:10]   response_class         THE BINDING PAGE's, witnessed from the
  //             material record's tmu_mode (see the window's header and the
  //             owner decision in FINDINGS-texmat2).
  //   [9:8]     palette_slot           THE BINDING PAGE's, and ZERO IS ITS LAW
  //   [7:0]     palette_generation     for a direct format:
  //             `binding_row_legal` REFUSES a direct row whose
  //             {palette_generation, palette_slot} is not zero. For a CLUT row
  //             they are real and unproduced, and `mat_win_clut_unowned_o`
  //             counts every material that would need them.
  //
  // THE VERTEX COLOUR IS THE REMAINING HALF OF ENTRY I20 and it is left at its
  // constants deliberately rather than invented: GEOM.VATTR holds a per-VERTEX
  // rgb/alpha (owner ruling R11) and this request wants a FLAT base colour for
  // the triangle, which is a different quantity. Picking one of the three
  // corners would be an art decision made by a composer.
  localparam logic [ 7:0] MAT_LOD_Q4_4_C     = 8'd0;    // level 0, mip_enable low
  localparam logic [23:0] MAT_BASE_RGB_C     = 24'hFF_FF_FF;  // white: the record's own colour, unmodulated
  localparam logic [ 7:0] MAT_BASE_ALPHA_C   = 8'hFF;   // opaque -- owner ruling R48
  localparam logic [ 1:0] MAT_PALETTE_SLOT_C = 2'd0;    // the direct-format row's law
  localparam logic [ 7:0] MAT_PALETTE_GEN_C  = 8'd0;    // the direct-format row's law

  wire [297:0] mat_flat_request_c = {
      mw_pub_sample_count,                      // [297:296]
      mw_pub_base_binding,                      // [295:288]
      MAT_LOD_Q4_4_C,                           // [287:280]
      mw_pub_material_recipe,                   // [279:277]
      mw_pub_recipe_weight,                     // [276:269]
      1'b0,                                     // [268]     aux_required
      224'd0,                                   // [267:44]  aux_surface_ctx
      MAT_BASE_RGB_C,                           // [43:20]
      MAT_BASE_ALPHA_C,                         // [19:12]
      mw_pub_response_class,                    // [11:10]
      MAT_PALETTE_SLOT_C,                       // [9:8]
      MAT_PALETTE_GEN_C                         // [7:0]
  };

  // Before anything is published the request is ALL ZERO, which is the legal
  // "this surface takes no texture sample" profile -- the same value the port
  // carried when it was a boundary. So the retirement of the port changes what
  // the console does only once a material has actually been resolved.
  wire [297:0] tri_flat_request_c = mw_pub_valid ? mat_flat_request_c : 298'd0;


  // --------------------------------------------------------------------------
  // GEOM.VATTR. Every input is a REAL producer's own net, in its own handshake:
  //
  //   batch   the dispatcher fork's accept (af_s_valid && af_s_ready) and the
  //           meshlet's visible mask -- the clock the record stream begins;
  //   opens   GEOM.GROUP_SEQ's open_o/open_arena_o -- the batch's arenas;
  //   u, v    GEOM.VDECODE's decoded vertex, on its handshake INTO the palette
  //           store (vd_d_valid && vd_d_ready): one per DECODED vertex, in
  //           order -- exactly the events GROUP_SEQ's arena index counts, so the
  //           ordinal the store keys u/v by IS the arena index (its header);
  //   colour  GEOM.LIGHT's lit result; the store owns the ready (va_lit_ready);
  //   landing GEOM.PROJ_LANE's fill_landed/arena/index, with `w` and the profile
  //           off client A's result port on the same clock (pj_a_w is the lane's
  //           own a_w_i; proj_a_profile_o is the service's, as REPLAY used it);
  //   lookup  GEOM.REPLAY's lookups as the arena ACCEPTED them.
  //
  // It answers REPLAY on the arena's clock (att_skew_o watches), and its
  // `done_o` gates GROUP_SEQ's handle into REPLAY (see those instances).
  // `GEOM_ASSET_MAX_VERTICES` sizes its rows: no batch holds more.
  // --------------------------------------------------------------------------
  zhao_geom_vattr #(
    .ARENAS  (GEOM_ARENAS),
    .ARENA_W (GEOM_ARENA_W),
    .INDEX_W (GEOM_INDEX_W),
    .VSLOTS  (GEOM_ASSET_MAX_VERTICES)
  ) u_geom_vattr (
    .clk   (gpu_clk),
    .rst_n (rst_n),

    .batch_i       (af_s_valid && af_s_ready),
    .batch_views_i (af_s_visible_mask),

    .op_valid_i    (gs_open),
    .op_arena_i    (gs_open_arena),

    .uv_valid_i    (vd_d_valid && vd_d_ready),
    .uv_u_i        (geom_vd_d_u_o),
    .uv_v_i        (geom_vd_d_v_o),

    .lit_valid_i   (geom_light_valid_o),
    .lit_ready_o   (va_lit_ready),
    .lit_r_i       (geom_light_r_o),
    .lit_g_i       (geom_light_g_o),
    .lit_b_i       (geom_light_b_o),

    .fl_valid_i    (ln_fill_landed),
    .fl_arena_i    (ln_fill_arena),
    .fl_index_i    (ln_fill_index),
    .fl_w_i        (pj_a_w),
    .fl_profile_i  (proj_a_profile_o),

    .done_o        (va_done),
    .poison_o      (va_poison),

    .look_valid_i  (rp_look_valid && ln_look_ready),
    .look_arena_i  (rp_look_arena),
    .look_index_i  (rp_look_index),
    .rep_valid_o   (va_rep_valid),
    .rep_invw24_o  (va_rep_invw),
    .rep_data_o    (va_rep_data),

    .landings_o        (geom_va_landings_o),
    .rows_written_o    (geom_va_rows_written_o),
    .colours_written_o (geom_va_colours_written_o),
    .uv_staged_o       (geom_va_uv_staged_o),
    .lq_overflow_o     (geom_va_lq_overflow_o),
    .index_oob_o       (geom_va_index_oob_o),
    .look_oob_o        (geom_va_look_oob_o),
    .profile_mixed_o   (geom_va_profile_mixed_o),
    .dq_refused_o      (geom_va_dq_refused_o),
    .dq_stray_o        (geom_va_dq_stray_o),
    .uv_waits_o        (geom_va_uv_waits_o)
  );

  // THE RULING-5 PACKET, per corner: slot 0 is GEOM.DEPTHQUANT's invw24,
  // zero-extended (the tile pipe refuses anything above bit 23), and slots 1..6
  // are the store's, in its order. Field placement only -- no arithmetic, and
  // the slot index is the named `GEOM_ATTR_SLOT_INVW` the packer reads.
  assign rp_attr_a = {rp_st_a, 8'd0, rp_invw_a};
  assign rp_attr_b = {rp_st_b, 8'd0, rp_invw_b};
  assign rp_attr_c = {rp_st_c, 8'd0, rp_invw_c};

  // synthesis translate_off
  initial begin
    if (GEOM_ATTR_SLOT_INVW != 0)
      $fatal(1, "zhao_console_core: the packet build above puts invw24 in slot 0 and GEOM_ATTR_SLOT_INVW says otherwise");
  end
  // synthesis translate_on

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

    // THE DECIMATED PLANES ARE RETIRED (owner ruling R64) and are left
    // UNCONNECTED HERE ON PURPOSE. The block keeps them because the block is
    // correct and is ruling T8 in hardware, differentially tested; what was
    // retired is the CLAIM that anything downstream needs them. Naming them
    // with empty parentheses -- rather than deleting them from the instance --
    // is what makes the retirement visible at the point of use instead of
    // being a silent absence somebody later reads as an oversight.
    //
    // THE WAIVER IS SCOPED TO THESE EIGHT PINS AND CITES ITS RULING, added
    // 2026-09-20. The eight empty pins are DELIBERATE by R64, and the linter
    // cannot tell a deliberate one from a forgotten one -- so unwaived they
    // turned the WAIVED console-board lint from "silent RC 0" into eight
    // warnings and a non-zero exit, for every packet that ran it after
    // 91335fe2. A gate that is red for a reason nobody owns is a gate people
    // learn to skip, which is the more expensive failure. Deleting the pins
    // instead would have hidden the retirement, which is the opposite of what
    // R64 asked for. The waiver is deliberately NOT file-wide: a pin left
    // empty by accident anywhere else in this module still fails, which is the
    // property worth keeping.
    //
    // TWO PACKETS FOUND THIS INDEPENDENTLY AND WROTE THE SAME PRAGMA -- gz/field
    // and gz/texmat2, within an hour, each verifying it was INHERITED rather
    // than assuming so (`git diff <base> HEAD` on this file empty, `git log -S
    // m17_valid_o` blaming 91335fe2). Both attributions are kept in one comment
    // rather than one being dropped in the merge: a defect that two independent
    // readers reach the same repair for is worth recording as such.
    /* verilator lint_off PINCONNECTEMPTY */
    .m17_valid_o(),
    .m17_addr_o (),
    .m17_surf_o (),
    .m17_h_o    (),
    .m9_valid_o (),
    .m9_addr_o  (),
    .m9_surf_o  (),
    .m9_h_o     (),
    /* verilator lint_on PINCONNECTEMPTY */

    .samples_o   (tmg_samples),
    .m17_writes_o(terr_mg_m17_writes_o),
    .m9_writes_o (terr_mg_m9_writes_o),
    .aborts_o    (terr_mg_aborts_o)
  );

  // ---- TERRAIN.LODFEED -----------------------------------------------------
  // ENTRY I18's PRODUCER, composed 2026-09-20 under owner ruling R70. It
  // OBSERVES the fine stream between TERRAIN.MIPFEED and TERRAIN.MIPGEN and
  // takes nothing from it: `f_valid_i` is the handshake that has ALREADY
  // happened (`valid && ready`), so this block cannot stall the mip pass, which
  // cannot stall TERRAIN.PAGESTREAM, which would hold a MEM.GUARD burst open.
  // The block's own header states that as a law and this connection is what
  // makes it true -- a `ready` here would have been a tap that bites.
  //
  // THE SOURCE ID COMES OFF `ps_src_id_o` AND IS NARROWED, which is a real
  // decision and not a cast: TERRAIN.MIPFEED carries 32 bits and this block
  // takes 16, because MEASURE.HISTOGRAM's `ev_src_id_i` is 16 and
  // `spec/measure_rules.md` section 3 says so. The bench's ids are 1000..1002,
  // T5's `src_id` field is a u32, and a source above 65,535 would ALIAS. That
  // is declared in the spec section rather than hidden here, and it is the
  // histogram's limit, not this chain's.
  //
  // MEASURED 2026-09-20: in `tests/prod/run_console_core_smoke.ps1` this block
  // sees NOTHING, and the reason is upstream and printed. Every page the smoke
  // plays fails its CRC, `tpl_fin_ok` never rises, TERRAIN.MIPREQ issues no job
  // and no lattice is ever streamed -- `mipreq requests=0 ... samples_sent=0`
  // on the smoke's `mip` line. The traverse is shown instead by
  // `tests/terrain/terrain_lodhist_directed.cpp`, which drives this exact
  // arrangement -- lodfeed, the same widening, the same histogram -- with a
  // lattice that moves. Quoting the smoke's green as evidence for this chain
  // would be the gap re-opened under a green gate, which is the thing ruling
  // R70 spends its last paragraph warning about.
  zhao_terrain_lodfeed #(
    .SLOTW               (TERR_MEMSLOT),
    .EDGE                (33),
    .DEV_INCLUDE_BOUNDARY(1'b1)          // owner ruling R22: the MESH reading
  ) u_terrain_lodfeed (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    .f_start_i (tmg_start),
    .f_slot_i  (tmg_job_slot),
    .f_src_id_i(tmf_ps_src_id[15:0]),
    .f_valid_i (tmg_fine_valid && tmg_fine_ready),
    .f_h_i     (tmg_fine_h),

    .w_valid_o (tlf_w_valid),
    .w_ready_i (tlf_w_ready),
    .w_slot_o  (tlf_w_slot),
    .w_sp_o    (tlf_w_sp),
    .w_dev1_o  (tlf_w_dev1),
    .w_dev2_o  (tlf_w_dev2),
    .w_dev3_o  (tlf_w_dev3),
    .w_cy_o    (tlf_w_cy),
    .w_src_id_o(tlf_w_src_id),

    .inv_valid_o(tlf_inv_valid),
    .inv_slot_o (tlf_inv_slot),

    .lattices_seen_o    (tlf_lattices_seen),
    .lattices_walked_o  (terr_lodfeed_lattices_walked_o),
    .lattices_dropped_o (terr_lodfeed_lattices_dropped_o),
    .surface1_samples_o (tlf_surface1_samples),
    .stray_samples_o    (terr_lodfeed_stray_samples_o),
    .walk_clocks_o      (tlf_walk_clocks),
    .dev_records_o      (terr_lodfeed_dev_records_o),
    .dev_clipped_o      (tlf_dev_clipped),
    .dev_vertices_o     (tlf_dev_vertices),
    .dev_lattice_reads_o(tlf_dev_lattice_reads),
    .busy_o             (tlf_busy)
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
  //     `zhao_field_isqrt`, and since 2026-09-19 `zhao_field_v3_svcpath`
  //     passes it `.LANES(GROUP_PTS)` -- the dispatcher's group cap -- where it
  //     used to pass only `.BANKS` and leave the default of four. So the roots
  //     are DIST_BANKS x FAB_GROUP_PTS: TWO here (2 x 1), THIRTY-TWO at the
  //     shipped 8 x 4. The whole-machine map probe measured the OLD eight --
  //     `gen_bank[0..1].gen_root[0..3]`, 8 x 248 ALUT -- which is the figure the
  //     six removed roots are priced against.
  //     `zhao_field_v3_len.sv:53` prices a root at ~251 ALM and eight at
  //     "roughly 2,000 ALMs", so the shipped point is about +6,000 ALM on this
  //     axis alone. `design/fit_targets.yml:1995` caps that module at
  //     `max_alms: 2000` -- a rule written for EIGHT roots, which the shipped
  //     configuration exceeds fourfold. That target has never been run.
  //
  //     SPENT 2026-09-19 (gz/pfs), AND THE SENTENCE THAT WAS HERE WAS WRONG
  //     ABOUT HOW. It said "forwarding LANES into `zhao_field_v3_len` would cut
  //     that to two". LANES is points per CONTEXT; at LANES=1 the dispatcher
  //     gathers four CONTEXTS into one four-point group
  //     (`zhao_field_v3_dispatch.sv` 17-20), so forwarding it would have
  //     computed one of four real points. What makes a narrower service exact
  //     is a cap on the GROUP, and that is `FAB_GROUP_PTS` below: the
  //     dispatcher is capped and the service sized by the same value. Roots
  //     here are now DIST_BANKS x GROUP_PTS = 2.
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
    // 13 IN / 7 OUT, and the number is owner ruling R40's ("the host widened to
    // 13 inputs / 7 outputs"), which is `spec/form/field-ir.md` 7.1's FLOW
    // record -- the widest of the profiles this console composes. It was 12/4,
    // the EARTH record, chosen when the S profile was the only client; the F
    // profile needs px,py,pz,vx,vy,vz,age,seed,dt,p0..p3 in and
    // px',py',pz',vx',vy',vz',attr0 out, and a lane it cannot present is a lane
    // its program reads as whatever the front cleared the register to.
    //
    // WHAT IS NOT WIDE ENOUGH, SAID HERE SO IT IS NOT DISCOVERED LATER: the
    // WARP profile is 14 in. GEOM.WARP is NOT BUILT AT ALL (the register's own
    // list), so nothing offers a warp record today; the day it does, this pair
    // moves to 14/7 and the adapters' elaboration guards are what will say so.
    .IN_LANES (13),
    .OUT_LANES(7),

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
    // bank is FAB_GROUP_PTS floor-exact roots. 2 banks x 1 = 2 roots here
    // against the shipped 8 banks x 4 = 32, at ~251 ALM each.
    .FAB_DIST_BANKS (2),
    // FAB_GROUP_PTS: shipped 4. POINTS PER LONG-OP GROUP -- the dispatcher's
    // group cap and the distance service's width, as one value. This front
    // holds ONE point in flight, so no group here ever held a second point:
    // lanes 1..3 of every group were padding, and the distance service still
    // computed them on six of its eight roots. At 1 the cap makes that
    // structural, so the service is built one lane wide and it is EXACT -- a
    // real point cannot reach a lane that is not computed -- with no clock
    // lost, because a group of one is all this front could ever form. Proven
    // by `field_v3_earth_group1`: the real Earth programs, every value against
    // the oracle, at GROUP_PTS=1. Handover 3 asked for LANES to be forwarded
    // instead; that would have been WRONG (at LANES=1 the dispatcher gathers
    // four CONTEXTS into one group), and this is the exact form of the same
    // saving: six roots and their front-end and bank registers.
    .FAB_GROUP_PTS  (1),
    // FAB_RING_UNITS: shipped 8. `zhao_field_v3_ring.sv:164` records a sweep
    // of RING_UNITS 8/16/32 against DIST_BANKS 4/8 that "moved the frame cost
    // by not one clock" -- so even on the Earth workload this axis is already
    // known to be past its knee.
    .FAB_RING_UNITS (2),
    .FAB_RING_DESC  (2)
  ) u_field_host (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    // I42 CLOSED: the program loader is `u_field_doorbell` below, SW.STREAM's
    // mailbox on the R14 pattern that owner ruling R43 names.
    .ld_valid_i(fdb_ld_valid),
    .ld_ready_o(fdb_ld_ready),
    .ld_kind_i (fdb_ld_kind),
    .ld_slot_i (fdb_ld_slot),
    .ld_addr_i (fdb_ld_addr),
    .ld_data_i (fdb_ld_data),

    // I42 CLOSED: FIELD.PROGCACHE's two phases, both driven by the same
    // doorbell. The insert edge stays internal, which is why the directory is
    // composed inside the engine.
    .pc_lu_valid_i     (fdb_lu_valid),
    .pc_lu_ready_o     (fdb_lu_ready),
    .pc_lu_hash_i      (fdb_lu_hash),
    .pc_lu_resp_valid_o(fdb_lu_resp_valid),
    .pc_lu_resp_ready_i(fdb_lu_resp_ready),
    .pc_lu_hit_o       (fdb_lu_hit),
    .pc_lu_slot_o      (fdb_lu_slot),
    .pc_cm_valid_i     (fdb_cm_valid),
    .pc_cm_ready_o     (fdb_cm_ready),
    .pc_cm_hash_i      (fdb_cm_hash),
    .pc_cm_ok_i        (fdb_cm_ok),
    .pc_cm_resp_valid_o(fdb_cm_resp_valid),
    .pc_cm_resp_ready_i(fdb_cm_resp_ready),
    .pc_cm_inserted_o  (fdb_cm_inserted),
    .pc_cm_evicted_o   (fdb_cm_evicted),
    .pc_cm_slot_o      (fdb_cm_slot),
    .pc_hits_o         (fld_pc_hits_o),
    .pc_misses_o       (fld_pc_misses_o),
    .pc_rejected_o     (fld_pc_rejected_o),
    .pc_evictions_o    (fld_pc_evictions_o),
    .pc_occupancy_o    (fld_pc_occupancy_o),

    // BOTH CLIENTS ARE REAL AS OF 2026-09-20. Client 0 is the S-profile stamp
    // adapter below; client 1 is the F-profile FLOW adapter, which is what
    // entry I5 called "its own stream adapter" and what entry I42 said would
    // take the edge seam. Two live profiles is also what makes
    // `fld_contended_grants_o` reachable with legal stimulus -- the reason the
    // edge client existed at all.
    .req_valid_i ({pfa_req_valid, sfa_req_valid}),
    .req_ready_o ({pfa_req_ready, sfa_req_ready}),
    .req_slot_i  ({pfa_req_slot, sfa_req_slot}),
    .req_noprog_i({pfa_req_noprog, sfa_req_noprog}),
    .req_in_i    ({pfa_req_in, sfa_req_in}),
    .resp_valid_o({pfa_resp_valid, sfa_resp_valid}),
    .resp_ready_i({pfa_resp_ready, sfa_resp_ready}),
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

  assign fld_sat_o = fld_sat_c;

  // ==========================================================================
  // THE FIELD PROGRAM DOORBELL -- entry I42, owner ruling R43
  // ==========================================================================
  // R43: "A doorbell contract on the R14 pattern: SW.STREAM stages the plan and
  // writes the EXISTING loader words". It writes them through this block, which
  // is `zhao_terrain_jdoorbell`'s shape with this seam's fields: a posted
  // mailbox, an order law, a ticketed return and a credit that makes the return
  // queue unoverflowable.
  //
  // WHY IT IS A CLOSURE AND NOT A PORT RENAME. `fld_ld_*` at the edge was four
  // fields with no identity, no ordering law and no answer -- a raw leaf port
  // with nobody on the far side. What is there now is a CONTRACT: every post is
  // answered, a commit that would promise the directory a slot whose header was
  // never written is refused and counted, a post that cannot be taken is held
  // rather than dropped, and the HPS gets a ticket back naming the plan. The
  // HPS is genuinely across this edge -- `zfield::decode` is software by
  // FIELD.PROGCACHE's own contract -- so the exchange is the thing that can be
  // built here, and it is built.
  zhao_field_doorbell #(
    .PROGS  (8),
    .SLOTW  (3),
    .LDADDRW(7),
    // Four posts staged ahead of the fabric going idle. The host accepts a
    // load word only while the fabric is IDLE (its own law), so a mailbox is
    // what keeps the HPS from having to watch for that window.
    .POSTS  (4),
    .RETQ   (4)
  ) u_field_doorbell (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    .cfg_plan_base_i(fld_cfg_plan_base_i),

    .post_valid_i (fld_db_post_valid_i),
    .post_ready_o (fld_db_post_ready_o),
    .post_op_i    (fld_db_post_op_i),
    .post_kind_i  (fld_db_post_kind_i),
    .post_slot_i  (fld_db_post_slot_i),
    .post_addr_i  (fld_db_post_addr_i),
    .post_data_i  (fld_db_post_data_i),
    .post_hash_i  (fld_db_post_hash_i),
    .post_ok_i    (fld_db_post_ok_i),
    .post_ticket_i(fld_db_post_ticket_i),

    .ld_valid_o(fdb_ld_valid),
    .ld_ready_i(fdb_ld_ready),
    .ld_kind_o (fdb_ld_kind),
    .ld_slot_o (fdb_ld_slot),
    .ld_addr_o (fdb_ld_addr),
    .ld_data_o (fdb_ld_data),

    .pc_lu_valid_o     (fdb_lu_valid),
    .pc_lu_ready_i     (fdb_lu_ready),
    .pc_lu_hash_o      (fdb_lu_hash),
    .pc_lu_resp_valid_i(fdb_lu_resp_valid),
    .pc_lu_resp_ready_o(fdb_lu_resp_ready),
    .pc_lu_hit_i       (fdb_lu_hit),
    .pc_lu_slot_i      (fdb_lu_slot),

    .pc_cm_valid_o     (fdb_cm_valid),
    .pc_cm_ready_i     (fdb_cm_ready),
    .pc_cm_hash_o      (fdb_cm_hash),
    .pc_cm_ok_o        (fdb_cm_ok),
    .pc_cm_resp_valid_i(fdb_cm_resp_valid),
    .pc_cm_resp_ready_o(fdb_cm_resp_ready),
    .pc_cm_inserted_i  (fdb_cm_inserted),
    .pc_cm_evicted_i   (fdb_cm_evicted),
    .pc_cm_slot_i      (fdb_cm_slot),

    .ret_valid_o   (fld_db_ret_valid_o),
    .ret_ready_i   (fld_db_ret_ready_i),
    .ret_ticket_o  (fld_db_ret_ticket_o),
    .ret_op_o      (fld_db_ret_op_o),
    .ret_ok_o      (fld_db_ret_ok_o),
    .ret_refused_o (fld_db_ret_refused_o),
    .ret_inserted_o(fld_db_ret_inserted_o),
    .ret_evicted_o (fld_db_ret_evicted_o),
    .ret_slot_o    (fld_db_ret_slot_o),
    .ret_plan_o    (fld_db_ret_plan_o),

    .posts_o           (fld_db_posts_o),
    .load_words_o      (fld_db_load_words_o),
    .lookups_o         (fld_db_lookups_o),
    .commits_o         (fld_db_commits_o),
    .commits_refused_o (fld_db_commits_refused_o),
    .post_stalls_o     (fld_db_post_stalls_o),
    .ret_overflow_o    (fld_db_ret_overflow_o)
  );

  // ==========================================================================
  // THE F PROFILE'S STREAM ADAPTER -- entry I5, owner ruling R40
  // ==========================================================================
  // The lane map is `spec/form/field-ir.md` 7.1's flow record and the
  // acceleration law is R40's `sat_s11((v' - v) >> 8)`; both live in that
  // file, with every constant R40 names as a parameter here so the owner's
  // revision is a one-line change (R40 is provisional and says particle motion
  // is judged by eye).
  //
  // THE JOIN IS THE ONE ENTRY I5 ALREADY SOLVED and it is written below at
  // PART.UPDATE rather than inside the adapter: this file gates `in_valid_i`
  // and PART.STATE's `prt_ready_i` on "the answer for THIS record is ready".
  zhao_field_flow_adapter #(
    .SLOTW    (3),
    .IN_LANES (13),
    .OUT_LANES(7)
  ) u_field_flow_adapter (
    .clk  (gpu_clk),
    .rst_n(rst_n),

    .rec_valid_i(ps_prt_valid),
    .rec_i      (ps_prt_record),
    .rec_take_i (pfa_rec_take),

    // The SAME population origin PART.COLLIDE and PART.TERRAIN_TAP read, so a
    // particle's world position is one law in this console and not two.
    .origin_x_i(pop_origin_x_c),
    .origin_y_i(pop_origin_y_c),
    .origin_z_i(pop_origin_z_c),

    .par_i(fld_flow_par_i),

    .slot_i      (fld_flow_slot_i),
    .slot_valid_i(fld_flow_slot_valid_i),

    .req_valid_o  (pfa_req_valid),
    .req_ready_i  (pfa_req_ready),
    .req_slot_o   (pfa_req_slot),
    .req_noprog_o (pfa_req_noprog),
    .req_in_o     (pfa_req_in),
    .resp_valid_i (pfa_resp_valid),
    .resp_ready_o (pfa_resp_ready),
    .resp_out_i   (fld_resp_out_c),
    .resp_status_i(fld_resp_status_c),

    .ans_valid_o(pfa_ans_valid),
    .fld_valid_o(pfa_fld_valid),
    .fld_ax_o   (pfa_fld_ax),
    .fld_ay_o   (pfa_fld_ay),
    .fld_az_o   (pfa_fld_az),

    .samples_o     (part_fld_samples_o),
    .bypassed_o    (part_fld_bypassed_o),
    .noprog_o      (part_fld_noprog_o),
    .faults_o      (part_fld_faults_o),
    .saturations_o (part_fld_saturations_o),
    .stall_cycles_o(part_fld_stall_cycles_o),
    .rec_changed_o (part_fld_rec_changed_o)
  );

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
    // The SHARED port's lane counts, not the S profile's own (8 in / 3 out).
    // They follow `u_field_host`'s, which owner ruling R40 set to the FLOW
    // record's 13/7; the stamp adapter fills lanes 0 and 1 and reads lanes 0
    // and 1, and the rest are the wider client's. Two different numbers here
    // and there would be a silent width truncation on the concatenated bus.
    .IN_LANES (13),
    .OUT_LANES(7)
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
