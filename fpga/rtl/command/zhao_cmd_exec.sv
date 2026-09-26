// zhao_cmd_exec.sv -- CMD.EXEC: the thing that turns a COMMAND into CONSOLE STATE.
//
// spec/commands.zidl:382 is the sentence this block exists to delete:
//
//     "NOTHING TURNS A COMMAND INTO A FRAME yet -- the composed bench plays the
//      frame ring by hand."
//
// It is the most-cited blocker in `tools/budget/completion_register.py`'s output:
// `zhao_console_core.sv` entries I14 (the projector's matrix bank), I30
// (SURFACE.STAMP's dispatch), I33 (PART.TABLE's per-frame load) and I7 (the
// collision plane) all say the same thing in four different vocabularies --
// "the port is real, the field is ratified, and no block in the tree produces
// it".
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS NOT. Read this before adding an arm.
// ---------------------------------------------------------------------------
// It is NOT a second validator. CMD.DECODER walks the same byte stream and
// reaches the ratified verdict; this block takes that verdict as an INPUT and
// never re-derives it. Writing a CRC, a length law or an opcode-legality test
// here would be a second implementation of the ratified arithmetic -- the exact
// failure `design/contracts/GEOM.LIGHT.md` line 118 names and the one that
// shipped twice in this tree already. The division is:
//
//     CMD.DECODER owns the VERDICT.   CMD.EXEC owns the PAYLOAD.
//
// It is NOT allowed to invent a command. Two of the four entries above are left
// OPEN on purpose and the reason is recorded here rather than in a run folder:
//
//   I33  PART.TABLE's per-frame load. `spec/commands.zidl` contains NO command
//        that carries a particle species descriptor. The core's own I33 text is
//        explicit that species CONTENTS are owner DATA
//        (`reference/include/zref/zref_particle.hpp`: "there is no species
//        table ... That is a DATA/ABI question and it is properly the
//        owner's"). Building a load path here would mean choosing a wire layout
//        for a record the ABI does not define, which is exactly the hidden
//        contract this file must not contain. It needs an ABI ruling first.
//
//   I7   PART.COLLIDE's plane. Same shape, shorter story: no ratified command
//        carries a plane. `SetEnvironment 0x0311` is the nearest thing in the
//        opcode space and it carries sun, ambient, tint and fog -- no geometry
//        -- and its execution semantics (owner ruling R25, implemented
//        2026-09-19) are the LIGHT bank's, so there is nothing to borrow.
//
// ---------------------------------------------------------------------------
// THE VERDICT TENSION, AND WHY THIS BLOCK DOES NOT NEED THE SPECULATIVE ESCAPE
// ---------------------------------------------------------------------------
// `zhao_cmd_decoder.sv` lines 70-75 state the problem exactly:
//
//     "THE CONSUMER MUST NOT RETIRE THESE until decode_done_o with
//      decode_error_o == ZH_ABI_OK. The payload CRC (4) and the count laws (9)
//      cannot conclude until the last byte ... The contract argues this choice
//      and records the rejected alternative (buffer and replay), which would
//      re-introduce exactly the storage this block exists to avoid."
//
// So an executor may not act before the last byte, and may not hold the packet
// to wait for it. The obvious way out is to act speculatively and abandon the
// frame on a bad verdict. THAT IS NOT WHAT THIS BLOCK DOES, and the reason is
// worth the paragraph, because the escape hatch is weaker than it looks:
// `SurfaceStamp` writes the 64x64 surface sheet, which is PERSISTENT -- scars
// survive the frame. "Abandon the frame" does not un-scar a sheet. A
// speculative stamp on a packet that then fails its CRC is a permanent
// corruption with a clean instrument beside it, which is the failure mode
// CLAUDE.md is mostly about.
//
// THE RESOLUTION IS TO NOTICE THAT THE DECODER'S OBJECTION IS ABOUT BYTES AND
// NOT ABOUT EFFECTS. The storage the decoder refuses is O(packet) -- a frame
// slot is FRAME_SLOT_BYTES = 1,048,576 bytes, 8 Mbit, and CMD.DMA's 1.97 Mbit
// blit buffer already made the composed shell unsynthesizable once. The storage
// this block needs is O(console state), because it stages the EFFECT of each
// command rather than the command:
//
//   * SetView is IDEMPOTENT STATE keyed by view. Two SetViews for one view
//     collapse -- the last one wins, which is what the ABI already means. So it
//     stages into a shadow of the bank it writes: 2 views x 16 words x 32 bits
//     = 1,024 bits, and NO packet length can make it larger.
//   * SurfaceStamp is an EVENT and events do not collapse, so it stages into a
//     ring of STAMP_Q entries x 208 bits. That IS bounded by the packet, so the
//     bound is DECLARED (STAMP_Q, an owner knob) and a packet that exceeds it
//     is REFUSED WHOLE and counted on `stamp_overflow_o` -- never half-applied.
//
// Total at the defaults: 1,024 + 8*208 = 2,688 bits. Against 8,388,608. Three
// orders of magnitude and a constant, versus a length. That is the whole
// argument, and it means this block obeys the decoder's contract LITERALLY --
// not one console-visible bit moves before `verdict_valid_i` -- rather than
// obeying a softer restatement of it.
//
// ASSUMPTION, stated so it is cheap to reverse: the cost above is paid in
// flip-flops on a device already at 97% ALM occupancy. If the shadow bank turns
// out to be the wrong side of the budget, the reversal is to move `sv_mat` into
// an M10K (it is a one-write/one-read array with no concurrent access) rather
// than to weaken the commit rule. `trade-alms-for-m10k` is the standing ruling
// and this array is exactly its shape.
//
// ---------------------------------------------------------------------------
// COMMIT ORDER IS VIEWS, THEN STAMPS, THEN DRAWS -- and that is a REORDERING
// ---------------------------------------------------------------------------
// Records arrive interleaved and commit grouped. That is safe ONLY because the
// targets are disjoint machines: the projector's matrix bank and the surface
// sheet share no state and no ordering law. A future arm whose command
// interacts with either of them -- anything that reads a matrix, or a second
// writer of the sheet -- BREAKS this and needs one ordered queue instead of two
// staging structures. Said here because the next person will add an arm.
//
// THE DRAW ARM IS THE NEXT PERSON, 2026-09-19, AND IT IS THE CASE THE
// PARAGRAPH ABOVE WARNS ABOUT -- so the placement is argued rather than
// assumed. A draw DOES read a matrix: `DrawForm`'s geometry is projected
// through whatever `SetView` last wrote. It does not read it HERE, though;
// nothing in this block touches `sv_mat` on the draw path. The interaction is
// one stage downstream, and its only requirement is an ORDER: a draw must be
// dispatched AFTER the view its own packet set, or the first frame of every
// camera move renders through the previous frame's camera -- a wrong picture
// with every counter balancing, which is the failure this file's rules exist
// for. `EX_CFG -> EX_STAMP -> EX_DRAW` delivers exactly that order
// STRUCTURALLY: the matrix bank has retired all 32 words before the first
// draw leaves. It is not a second ordered queue because it does not need to
// be -- draws commute with stamps (the sheet is not read by a draw) and are
// strictly after views.
//
// DRAWS DO NOT COMMUTE WITH EACH OTHER, which is why the ring is a RING and
// not a shadow: two DrawForms are two draws, and the ABI's submission order is
// the order they must dispatch in. That is the same argument SurfaceStamp
// already makes one paragraph up -- an EVENT does not collapse.
//
// ---------------------------------------------------------------------------
// WHAT IS CARRIED, AND WHAT IS DECLARED NOT CARRIED
// ---------------------------------------------------------------------------
// Every byte offset below comes from `fpga/rtl/generated/zhao_abi_pkg.sv`
// (`ZHAO_SET_VIEW_OFF_*`, `ZHAO_SURFACE_STAMP_OFF_*`). None is written by hand,
// for CMD.DECODER's own reason: "layouts come EXCLUSIVELY from the generated
// package". The two exceptions are `tx` and `ty` INSIDE `transform2fx`, which
// the generator emits as one struct offset; they are `+0` and `+4` by that
// struct's declaration order (`fx16 tx; fx16 ty;`) and an elaboration guard
// below checks `$bits(zhao_transform2fx_t)` so a layout change cannot pass
// silently.
//
// SetView 0x0010 carries EIGHT fields since ruling R63 added `eye[3]`. This
// block carries or reads SEVEN of them since 2026-09-20; the one it does not
// has NO PORT ON THIS CONSOLE -- that is a gap, not a decision:
//   CARRIED   view_id           -> `proj_cfg_view_o` (see the refusal below)
//   CARRIED   view_projection   -> `proj_cfg_addr_o` 0..15, one word per clock
//   CARRIED   viewport_id       -- LOWERED TO A RECTANGLE, as of 2026-09-20,
//                                  at cfg addresses 16/17 (steps 20/21 of the
//                                  view walk).
//
//                                  THE SENTENCE HERE UNTIL TODAY WAS FALSE AND
//                                  IT ASSERTED A PRESENCE, which is the
//                                  expensive kind: "the id -> rect table is
//                                  video_rules.md's and is not in the ABI."
//                                  There was no such table in video_rules.md.
//                                  There is now -- section 3.2 -- and
//                                  `zref::render::viewports_of()` has held the
//                                  same rectangles since the 2026-08-15
//                                  ratification, so lowering them here invents
//                                  nothing and differentials against the
//                                  oracle like everything else this block
//                                  lowers. The table is DERIVED and not an ABI
//                                  field (ruling R73's distinction), so this
//                                  costs no zidl change and no capture
//                                  regeneration.
//
//                                  WHICH MODE indexes it is the one decision
//                                  the lowering needed: video_rules 3.2 and
//                                  FINDINGS-projinput.md D-2 both recommend
//                                  the mode the CONTRACT set, and that is what
//                                  `pc_mode` holds. The reasoning, including
//                                  the part neither of them could see, is at
//                                  `pc_mode`'s declaration.
//
//                                  An id that names no viewport in that mode
//                                  is REFUSED, never aliased, and counted on
//                                  `viewport_range_refused_o`; the bank keeps
//                                  its previous rectangle and the camera that
//                                  arrived with it still lands.
//   CARRIED   flags[1:0]        -- depth_profile, as of 2026-09-19. The frozen
//                                  ruling of 2026-08-31 assigns it, and
//                                  `zhao_project_core` now has cfg address 18 to
//                                  put it on. Committed as the SEVENTEENTH step
//                                  of the view walk, so a view's camera and its
//                                  profile land from ONE record under ONE dirty
//                                  bit and cannot be split across frames. The
//                                  reserved value 2'd3 is refused by the BANK,
//                                  not here: CMD.DECODER owns the verdict and
//                                  this block owns the payload, and a second
//                                  legality test here would be the duplicate
//                                  implementation this header opens by
//                                  forbidding.
//                                  `flags[15:2]` stay UNASSIGNED and are not
//                                  read -- zhao_sample_set_view() already ships
//                                  0x8261, so demanding zero would refuse a
//                                  legal command.
//   CARRIED   eye[3]            -- the camera position in world metres, as of
//                                  2026-09-20 (ruling R63). Committed as steps
//                                  17/18/19 of the view walk to cfg addresses
//                                  19/20/21, where `zhao_view_eye` holds it for
//                                  TERRAIN.LOD. Under the SAME dirty bit as the
//                                  matrix and the profile, so a view can never
//                                  run with this frame's camera and last
//                                  frame's eye.
//   CARRIED   pixel_error       -- the view's per-pixel error budget, fx16, as
//                                  of 2026-09-21 (packet TERRACOMP). Published
//                                  on `gov_px_err0_o`/`gov_px_err1_o` at the
//                                  END of the same view walk that writes the
//                                  matrix, the profile and the eye, under the
//                                  SAME `sv_dirty` bit -- so MEASURE.GOVERNOR
//                                  can never decide a level from this frame's
//                                  camera and last frame's error budget.
//                                  THE SENTENCE THIS REPLACES SAID "MEASURE.
//                                  GOVERNOR is not composed", and three passes
//                                  of this campaign read that as "the field is
//                                  not in the ABI". It always was: the zidl has
//                                  declared `fx16 pixel_error` since
//                                  ratification and `ZHAO_SET_VIEW_OFF_
//                                  PIXEL_ERROR` has been generated all along.
//                                  What was missing was this decode arm.
//   READ      geometry_tokens   -- the view's token REQUEST (rulings R18/R33),
//   READ      fragment_tokens      committed to MEASURE.TOKENS as `tok_vreq_*`,
//                                  which clamps it to the contract's ceiling.
//
// SetPresentationContract's five token counts are that CEILING, committed as
// `tok_budget_*` BEFORE the views' requests (phase EX_TOK), so a request always
// meets the ceiling of its own packet. COUNTS, as the wire carries them:
// nothing here converts or scales anything (R33).
//
// `view_id` IS A u8 AND THE BANK HAS TWO VIEWS. It is NOT masked down to one
// bit: a `view_id` the bank cannot address is REFUSED, the record is not
// staged, and `view_range_refused_o` counts it. Masking would execute a
// command the game did not issue, silently, which is the worse of the two
// failures by a distance.
//
// SurfaceStamp 0x0210 likewise:
//   CARRIED   patch, operation, tag, strength, transform.tx, transform.ty,
//             radius, ring_width, and the RECORD HEADER's source_id.
//   NOT       brush             -- `zhao_surface_stamp.sv` S5 says it plainly:
//                                  "`brush` IS NOT AN INPUT ... NOTHING in this
//                                  tree defines a brush page's format". There
//                                  is no port to drive.
//   NOT       transform.r00..r11 -- the stamp is a disc or an annulus and a
//                                  circle is rotation-invariant; the consumer
//                                  has no rotation port. This is the one place
//                                  a dropped field is provably inert.
//   NARROWED  source_id is u32 on the wire and `cmd_src_id_i` is 16 bits. The
//             low half is carried; a nonzero HIGH half is counted on
//             `stamp_src_truncated_o` rather than dropped in silence.
//
// DrawForm 0x0300 -- ADDED 2026-09-19, and it carries ALL SIX of its fields
// plus the record header's source_id. Nothing about this record is dropped:
//   CARRIED   form, material_set, transform (three handle32), viewport_mask,
//             semantic_weight, flags, and the header's source_id (narrowed to
//             16 bits on `draw_src_truncated_o`, the SurfaceStamp rule).
// Three handles leave this block as handles, UNRESOLVED and on purpose. See
// the section below.
//
// PublishResource 0x0030 -- ADDED 2026-09-19 by owner ruling R17. Every field
// is one of MEM.UPLOAD's request fields and is carried whole:
//   CARRIED   resource[31:8] (5f.1's directory key), hps_addr_lo/hi (64 bits,
//             so MEM.UPLOAD can REFUSE an unreachable source rather than this
//             block narrowing it), vram_dst, length, crc32c, new_generation,
//             epoch, dst_slot, kind (-> MEM.UPLOAD's req_tag_i).
//   NOT       resource[7:0], the handle's generation byte: MEM.UPLOAD's
//             generation is the 16-bit RESIDENCY one and nothing in the upload
//             path consumes the handle's. Sunk visibly, see `pq_handle_gen_unused`.
//   NOT       the header's source_id: MEM.UPLOAD has no attribution port.
// Uploads are EVENTS (ring, refused whole on `upload_overflow_o`) and commit
// into a PENDING queue that outlives the commit, so a background copy never
// holds a frame's draws. `tests/command/cmd_exec_directed.cpp` cases 13-16.
//
// ---------------------------------------------------------------------------
// WHY THE DRAW ARM EMITS HANDLES AND NOT A MESHFETCH JOB
// ---------------------------------------------------------------------------
// This is the question the arm exists to answer honestly, so it is written out
// rather than left to be discovered from a wrong address.
//
// `zhao_geom_meshfetch`'s job packet is
// {instance_id, desc_addr[26:0], format, generation, active_mask, xform[12]}.
// DrawForm supplies, RATIFIED, only part of it:
//
//   j_active_mask_i   <- viewport_mask[1:0].            AVAILABLE.
//   j_generation_i    <- form handle32's generation:8.  AVAILABLE.
//   j_desc_addr_i     -- NOT AVAILABLE, and it is a MISSING RULING rather than
//                        missing wiring. `form` is handle32 {index:24,
//                        generation:8} and turning an index into a 64-byte
//                        aligned pool address needs the pool's internal
//                        layout. SEARCHED: `spec/memory_rules.md` 5f ratifies
//                        the REGION (`ZHAO_RENDER_ASSET_BASE` 0x06A0_0000,
//                        22 MiB, ENGINE1, read-only) and then says in as many
//                        words: "Not decided: the pool's internal layout
//                        (descriptors vs index streams vs vertex records) ...
//                        how it is carved up is the asset fetcher's business
//                        and is still open." `design/contracts/
//                        GEOM.ASSETFETCH.md` repeats it. There is no
//                        `BASE + index*64` law to apply, and inventing one
//                        here would be this block choosing a memory layout the
//                        ABI declines to define -- the same refusal I33 and I7
//                        already carry, one subsystem over.
//   j_format_i        -- NOT AVAILABLE for the same reason. It is "the format
//                        this reader speaks", checked against the descriptor's
//                        own byte 0; no command carries it and no registry
//                        defines it, so it is a caller declaration waiting on
//                        the same ruling.
//   j_xform_i[12]     -- NOT AVAILABLE. `transform` is handle32[transform], an
//                        ID, and GEOM.MESHFETCH's own header says resolving an
//                        id to a matrix "is a PALETTE LOOKUP, and this block
//                        does not own it". SEARCHED: the resolver of this
//                        shape is `zref::material::Resolver`
//                        (`reference/include/zref/zref_material_resolve.hpp`),
//                        whose RTL is `MATERIAL.RESOLVE`. THE CITATION HERE
//                        WAS A PHANTOM until 2026-09-19: it quoted "whose
//                        contract's line 4 reads 'RTL: not built'", and that
//                        line now reads "BUILT 2026-09-19". The block is BUILT
//                        AND NOT COMPOSED, waiting on a
//                        `spec/memory_rules.md` 5f ruling about the residency
//                        directory's key, so this arm's conclusion stands on a
//                        fact that is still true.
//
// A job is ATOMIC -- six fields in one handshake -- so half-driving it is not
// a half closure, it is a fetch at whatever address the other half happened to
// be holding. That is the join-between-two-things-that-move-independently
// fault this tree has written down three times. So this block emits the
// RATIFIED RECORD and stops, and the resolver is a named absent owner instead
// of a plausible wrong address.
//
// ---------------------------------------------------------------------------
// FRAMING
// ---------------------------------------------------------------------------
// The packet walk is the same one `zhao_shell_top_v2`'s glue 3 already ships
// and `tests/shell/shell_golden.cpp` already pins: header [0,36), records
// [36, len-4), trailing payload CRC word [len-4, len). `pkt_len_m4_q` is
// registered for the timing reason that file measured on the 2026-08-24 composed
// fit, and the same argument applies unchanged -- a value consulted no earlier
// than 36 accepted bytes into a packet cannot observe a one-cycle lag.
//
// A MALFORMED RECORD CANNOT HURT, and this is the second half of the commit
// rule doing real work. A garbage `record_bytes` walks this framer into
// nonsense, stages nonsense, and is then thrown away, because the same packet
// cannot pass CMD.DECODER. Staging is allowed to be wrong; committing is not.
// ---------------------------------------------------------------------------
module zhao_cmd_exec
  import zhao_abi_pkg::*;
#(
    // Staged SurfaceStamp capacity. An owner knob, and the one number in this
    // block that can refuse a legal packet -- see `stamp_overflow_o`.
    parameter int unsigned STAMP_Q = 8,

    // Staged DrawForm capacity, the same kind of knob and the same law: a
    // packet carrying more draws than this is refused WHOLE on
    // `draw_overflow_o`, never half-drawn. Smaller than STAMP_Q because a
    // Phase-2 frame submits a handful of forms and the entry is wider (144
    // bits against 208 x 8); raise it when a frame's form count does.
    parameter int unsigned DRAW_Q = 4,

    // Staged PublishResource capacity per packet (R17), the same law as the
    // two above: more than this in one packet is refused WHOLE on
    // `upload_overflow_o`, never half-published.
    parameter int unsigned UPL_Q = 4,
    // The PENDING upload queue between a committed packet and MEM.UPLOAD. It
    // outlives the commit, so a packet's draws are not held behind a
    // multi-burst upload -- MEM.UPLOAD takes one request at a time and is a
    // background client (memory_rules 5d). A commit that finds it full WAITS
    // (backpressure, counted in no counter because nothing is lost).
    parameter int unsigned UPL_PQ = 4,

    // Staged grading-table entries per packet (R36): one whole product-vector
    // table is 32 + 64 + 32 = 128. More than this in one packet is refused WHOLE
    // on `grade_overflow_o`. The staging is a memory (128 x 80 b, two M10K at the
    // 256x40 shape), written once per entry and read once at commit.
    parameter int unsigned GRADE_Q = 128,

    // Staged TerrainField records per packet (entry I34 build item (a)). A
    // field application is an EVENT and events do not collapse: two
    // TerrainFields over the same footprint are two field lanes on the
    // section 3.4 sum, not a last-one-wins shadow. So it stages into a ring
    // and the bound is DECLARED, exactly as STAMP_Q is, and a packet that
    // exceeds it is REFUSED WHOLE on `tfld_overflow_o` rather than
    // half-applied -- the surface-sheet argument in this file's header
    // applies verbatim, because a composed height that is missing one of its
    // lanes is a wrong terrain with every counter balancing.
    //
    // 4 x 496 b = 1,984 flip-flops at the default. TERRAIN.PATCH's own list
    // is the thing that bounds how many lanes a vertex can get, and the
    // 16-field tail-reject policy of the directive's 13.7 lives THERE, in
    // command order, not here -- this block does not re-decide it.
    parameter int unsigned TFLD_Q = 4,
    // Procedural draws staged per packet. Four, for TFLD_Q's reason: the queue
    // exists so a packet's records are published or rolled back WHOLE, not so
    // the console can buffer a frame's worth. A packet carrying more is refused
    // entire on `forge_overflow_o`.
    parameter int unsigned FORGE_Q = 4
) (
    input  logic clk,
    input  logic rst_n,

    // ---- the sealed packet byte stream, forked from CMD.DMA ----------------
    // The SAME stream CMD.DECODER sees. The fork is an AND of the two readies
    // in the composer; a consumer that cannot say "not yet" drops bytes the
    // other consumer has already counted.
    //
    // `pkt_fork_ready_i` IS THE AND, HANDED BACK, and it is not decoration --
    // it is the difference between this block working and this block reading
    // every field one byte out of place. The obvious `take = pkt_valid_i &&
    // pkt_ready_o` is wrong on a FORK: it says "I could have taken it", and the
    // byte only actually moves when the OTHER consumer could too. CMD.DECODER
    // holds its ready low for one cycle in S_CHECK, at packet offset 36 --
    // which is the first byte of the record region -- so a walk driven by the
    // local ready runs exactly one byte ahead for the whole packet and captures
    // nothing at any offset it believes in. Measured, not reasoned: the first
    // build of this block staged zero records with every counter reading a
    // confident zero and the decoder beside it reporting four records walked.
    // `zhao_shell_top_v2`'s glue 3 has the same shape from the other side
    // (`pkt_ready = ... && cmd_pkt_ready_i`) and its comment says why: a tap
    // "would let this framer advance past a byte the other consumer never saw".
    input  logic        pkt_valid_i,
    output logic        pkt_ready_o,
    input  logic        pkt_fork_ready_i,
    input  logic [ 7:0] pkt_byte_i,
    input  logic [31:0] pkt_len_i,

    // ---- the verdict, from CMD.DECODER -------------------------------------
    // NOT re-derived here. `verdict_valid_i` is that block's `decode_done_o`
    // and `verdict_error_i` its `decode_error_o`.
    input  logic        verdict_valid_i,
    input  logic [ 7:0] verdict_error_i,

    // ---- I14: the shared projector's matrix bank ---------------------------
    // Address map is `zhao_project_core.sv`'s: 0..15 matrix, 16 viewport
    // origin, 17 viewport extent. This block writes 0..15 and nothing else.
    // `proj_cfg_ready_i` IS A REFUSAL, not a stall of the bank. The bank has a
    // second writer -- the console's host cfg port, which owns addresses 16
    // and 17 (the viewport rect) because SetView carries an id and not a rect.
    // The composer gives that writer the cycle and drops this one's ready; the
    // word is then re-presented unchanged. Nothing is dropped on either side,
    // so the composer needs no conflict counter -- and a counter it could not
    // fire would have been worse than none.
    output logic        proj_cfg_we_o,
    input  logic        proj_cfg_ready_i,
    output logic        proj_cfg_view_o,
    output logic [ 4:0] proj_cfg_addr_o,
    output logic [31:0] proj_cfg_data_o,

    // ---- I30: SURFACE.STAMP's dispatch, the ratified fields only -----------
    output logic               stamp_valid_o,
    input  logic               stamp_ready_i,
    output logic        [31:0] stamp_patch_o,
    output logic        [ 7:0] stamp_operation_o,
    output logic        [ 7:0] stamp_tag_o,
    output logic        [15:0] stamp_strength_o,
    output logic signed [31:0] stamp_tx_o,
    output logic signed [31:0] stamp_ty_o,
    output logic signed [31:0] stamp_radius_o,
    output logic signed [31:0] stamp_ring_width_o,
    output logic        [15:0] stamp_src_id_o,

    // ---- I41: THE DRAW DISPATCH, the ratified fields only -------------------
    // DrawForm 0x0300, whole. The three handles are NOT resolved here and the
    // section above says why at length: the pool layout that would turn
    // `form` into a descriptor address is `spec/memory_rules.md` 5f's
    // explicitly UNDECIDED, and the palette that would turn `transform` into a
    // 3x4 is MATERIAL.RESOLVE, which is BUILT and NOT COMPOSED -- this line
    // said "whose RTL is not built" until 2026-09-19, which was the right
    // conclusion from a fact that had stopped being true.
    output logic        draw_valid_o,
    input  logic        draw_ready_i,
    output logic [31:0] draw_form_o,           // handle32 {index:24, gen:8}
    output logic [31:0] draw_material_set_o,   // handle32
    output logic [31:0] draw_transform_o,      // handle32
    output logic [ 7:0] draw_viewport_mask_o,
    output logic [ 7:0] draw_semantic_weight_o,
    output logic [15:0] draw_flags_o,
    output logic [15:0] draw_src_id_o,

    // ---- R229: the POSED half of the same dispatch -------------------------
    // `DrawPosedForm` 0x0305 (owner ruling R229, decision D-POSEPAGE-A) is a
    // DrawForm whose payload continues past byte 15 with the animation key the
    // pose cache needs. It is NOT a second dispatch port, and that is the whole
    // design: the pose travels on `draw_valid_o`/`draw_ready_i`, in the SAME
    // `dq` entry, latched by the SAME write.
    //
    // WHY THAT MATTERS MORE THAN THE THREE WIRES IT SAVES. CLAUDE.md's
    // metadata-bank defect was a record and its metadata held by two different
    // register enables, so a stall produced one response's data beside another
    // response's metadata while every counter balanced. A pose and its draw are
    // ONE record; giving them one enable means there is no stall that can
    // separate them, and no checker is needed for a skew that cannot occur.
    //
    // R13's clause -- "the job port is not widened to carry a subpatch-uniform
    // value that is not true" -- FORBIDS widening for a uniform value and is
    // what SELECTS this shape for a per-draw one. R229: "pose varies per
    // creature", which is why the `SetPose` state alternative was refused.
    //
    // `draw_posed_o` LOW is the BIND POSE, and it is low for every `DrawForm`
    // 0x0300 ever recorded. Without it, clip 0 frame 0 would be
    // indistinguishable from "no pose named", and the clean split R229 asked
    // for would exist in the ABI and not on the wire.
    output logic        draw_posed_o,     // 0 = bind pose (DrawForm 0x0300)
    output logic [15:0] draw_clip_id_o,   // clip SLOT id; valid only when posed
    output logic [15:0] draw_frame_no_o,  // key index within the clip
    output logic [ 7:0] draw_sub_o,       // half-key phase (pose cache acq_sub_i)

    // ---- W04: DrawWarpedForm 0x0304's per-draw Warp snapshot ---------------
    // THE SAME HANDSHAKE AND THE SAME QUEUE ENTRY AS THE DRAW, for the reason
    // the pose block above gives at length: one register enable means no stall
    // can separate a draw from its snapshot, so there is no skew for a checker
    // to look for. These are `zhao_geom_warp.sv`'s `d_*_i` port group, field
    // for field, and they are the reason that block stops being unreachable.
    //
    // WHY A SNAPSHOT AND NOT A STATE COMMAND, once, here, because this is the
    // port that would have been a register: directive 6.2 refuses
    // `SetWarp` + `DrawForm` BY NAME, because CMD.EXEC groups some state
    // updates before draws and a last-writer Warp setting "could retroactively
    // change older draws". W05: "No mutable global `current_warp` register."
    //
    // `draw_warp_en_o` LOW is an ORDINARY DRAW, and it is low for every
    // `DrawForm` 0x0300 and every `DrawPosedForm` 0x0305 ever recorded -- and
    // also for a 0x0304 whose `warp_program` is zero, which is a legal record
    // asking for no deformation. W09 requires that path to perform zero Warp
    // lookups and zero Warp evaluations, so the ENABLE is the thing the
    // consumer switches on, never the data.
    output logic        draw_warp_en_o,
    output logic [31:0] draw_warp_program_o,   // W-profile program handle
    output logic [31:0] draw_warp_time_o,      // THIS draw's tick, lane 10
    output logic [127:0] draw_warp_par_o,      // p0..p3, lanes 11..14, p0 low
    output logic [127:0] draw_warp_attr_o,     // a0..a3 INLINE4, lanes 6..9
    output logic [31:0] draw_warp_attr_res_o,  // STREAM4 resource handle
    output logic [ 7:0] draw_warp_attr_mode_o, // warp_attribute_mode
    // W11's componentwise world-space bound. NONNEGATIVE by the refusal below,
    // so a consumer may compare against it without re-checking its sign.
    output logic signed [31:0] draw_warp_bx_o,
    output logic signed [31:0] draw_warp_by_o,
    output logic signed [31:0] draw_warp_bz_o,

    // ---- R17: PublishResource -> MEM.UPLOAD's request port -----------------
    // Field for field MEM.UPLOAD's `req_*`, from the GENERATED offsets. What is
    // NOT carried, and why: the handle's 8-bit GENERATION. MEM.UPLOAD's
    // generation is the 16-bit RESIDENCY generation (`new_generation`), which
    // is what D-3's cache tag keys on; the handle's byte names which
    // incarnation of the resource the game means, and no port in the upload
    // path consumes it. Carrying it to nothing would be a wire, not a check.
    output logic        upl_valid_o,
    input  logic        upl_ready_i,
    output logic [23:0] upl_index_o,      // handle32[31:8]: 5f.1's directory key
    output logic [ 7:0] upl_kind_o,       // .zpak kind -> MEM.UPLOAD req_tag_i
    output logic [63:0] upl_hps_addr_o,
    output logic [31:0] upl_vram_addr_o,
    output logic [31:0] upl_len_o,
    output logic [15:0] upl_epoch_o,
    output logic [ 7:0] upl_dst_slot_o,
    output logic [15:0] upl_new_gen_o,
    output logic [31:0] upl_crc_o,
    // ---- R25: SetEnvironment 0x0311, the fields the light bank consumes ------
    // Staged like SetView (a shadow, last record wins), presented ONCE per
    // committed packet that carried one, never on an abandoned packet. Its
    // consumer is zhao_light_env. Tint and fog are not presented: they are not
    // bank words (reference/include/zref/zref_light_env.hpp says why).
    output logic        env_valid_o,
    input  logic        env_ready_i,
    output logic [15:0] env_sun_yaw_o,
    output logic [15:0] env_sun_pitch_o,
    output logic [15:0] env_sun_colour_o,   // rgb565
    output logic [15:0] env_ambient_o,      // rgb565
    // TERRAIN'S MATERIAL IDENTITY (TERRAINMAT, 2026-09-26), allocated out of
    // this record's own declared pad -- see `spec/commands.zidl`'s
    // SetEnvironment. It rides the SAME `env_valid_o` handshake and the SAME
    // shadow/verdict atomicity as the four light fields above, because it is
    // one more field of ONE record: a consumer can never see this frame's
    // terrain material beside the previous frame's sun. It is NOT a light-bank
    // word and `zhao_light_env` does not read it; the composer forks it to the
    // terrain arm. A ZERO set is MATMODE_NONE, which is what every capture
    // written before this field existed says, so the default is the old
    // behaviour rather than a new one.
    output logic [31:0] env_terr_mat_set_o,
    output logic [15:0] env_terr_mat_id_o,
    output logic [31:0] envs_issued_o,

    // ---- R41: SetPopulation 0x0303, the population descriptor ---------------
    // Staged and presented exactly like SetEnvironment above -- a shadow during
    // STAGE, last record wins, offered ONCE per committed packet that carried
    // one and never on an abandoned packet. Its consumer is `zhao_part_pop`,
    // which is also what REFUSES a field the engine cannot carry; this block
    // lowers the record and interprets none of it.
    output logic        pop_valid_o,
    input  logic        pop_ready_i,
    output logic [31:0] pop_population_o,
    output logic [31:0] pop_origin_x_o,      // 1/256-m grid (qformats 10)
    output logic [31:0] pop_origin_y_o,
    output logic [31:0] pop_origin_z_o,
    output logic [31:0] pop_active_count_o,
    output logic [31:0] pop_plane_c_o,       // Q10
    output logic [15:0] pop_plane_nx_o,      // Q1.10
    output logic [15:0] pop_plane_ny_o,
    output logic [15:0] pop_plane_nz_o,
    output logic [15:0] pop_flags_o,         // b0 seed, b1 plane_enable
    output logic [31:0] pops_issued_o,

    // ---- TerrainField 0x0200: the producer entry I34 has been waiting for --
    //
    // Entry I34 in `zhao_console_core.sv` lists four things left to build and
    // this is (a) and (b) of them, quoted so the division is not re-derived:
    //
    //   (a) CMD.EXEC's TerrainField arm, staging ~480 bits per record
    //       (program, four footprint fx16, start_tick, duration_ticks and
    //       p0..p7) and emitting {footprint, hash, cmd index} onto
    //       `terr_pt_fld_add_*` at commit
    //   (b) a descriptor table keyed by that cmd index holding the uniforms
    //
    // (c) the EARTH stream adapter and (d) the `pc_lu_*` two-client share are
    // NOT here and are not this block's. I34 DOES NOT CLOSE on this commit.
    //
    // THE DESTINATION ALREADY EXISTS AND IS RATIFIED. `zhao_terrain_patch`'s
    // `fld_add_valid_i / fld_add_ready_o / fld_add_x0_i / fld_add_z0_i /
    // fld_add_x1_i / fld_add_z1_i / fld_add_hash_i / fld_add_cmd_i` is the
    // section 9.1 list intake, and `zhao_console_core` promotes it to the
    // CORE BOUNDARY as inputs it does not produce -- which is precisely why
    // the register calls I34 a `boundary` and not a tie-off. This arm is the
    // missing producer, not a new interface.
    //
    // (b) IS EMITTED ON THE SAME HANDSHAKE rather than held in a table with a
    // read port. A table indexed by `cmd` would need an address input that
    // nothing drives until (c) exists, and a disconnected input is a gap --
    // so the uniforms ride the record and the consumer keeps them. The two
    // fan out to different owners: {footprint, handle, cmd} is the patch's
    // list intake, {start_tick, duration, p0..p7} is the EARTH adapter's
    // descriptor. One producer, one handshake, two readers.
    //
    // START_TICK AND DURATION ARE EMITTED RAW, NOT AS AGE OR PHASE. The
    // directive's 13.7 says software "uses the existing canonical age/phase
    // helper/model" and that "no new approximate phase divider is invented in
    // the core composer". `age = tick - start_tick` is the consumer's, and
    // computing it here would be that forbidden second divider.
    //
    // THIS IS A HANDLE AND IT IS NOT A HASH -- the one thing 20.8 forbids is
    // declaring a channel present because the bits are there. TerrainField
    // carries `handle32[program] program`; FIELD.PROGCACHE's directory key is
    // a CONTENT hash (`CRC32C(code||tables) + instr_count`), and I34 records
    // that NOTHING IN HARDWARE publishes {handle -> hash} -- `program_hash`,
    // `prog_hash` and `programHash` across all of `fpga/rtl` including
    // `synth/` are zero hits, re-checked this pass. So this port carries the
    // handle, is NAMED the handle, and `fld_add_hash_i` stays unfed until the
    // BIND post kind I34 recommends exists. Wiring this output into a port
    // called `hash` would put a handle in a trace field that says hash, which
    // is a lie that costs nothing today and an afternoon later.
    //
    // THE ZERO-HIT SWEEP IS STRUCK, 2026-09-21 (gz/fieldlane); THE DECISION IT
    // SUPPORTS IS NOT. Re-running that exact pattern today returns 22 hits
    // under `fpga/rtl`, and the decisive one is `pub_prog_hash_o` in
    // `zhao_field_loader.sv` -- which MATCHES `prog_hash`. The pattern was
    // never wrong; the sweep was a claim about a MOMENT, and the producer
    // landed either side of it. So {handle -> hash} IS resolvable in hardware:
    // the loader publishes `pub_handle_o` and `pub_prog_hash_o` together off
    // `pub_sel_i` over its 8 objects, and its own comment says that indexed
    // read "costs one mux the descriptor table needs anyway".
    //
    // KEEPING THE HANDLE ON THIS PORT REMAINS RIGHT, for a reason that
    // outlives the sweep: resolving a handle needs `pub_sel_i`, which
    // `zhao_console_core` promotes to its BOUNDARY rather than driving, so the
    // resolver is a two-client share and not a wire. Naming this output the
    // handle keeps the unresolved step visible instead of burying it in a
    // field called hash. That is the right call for a better reason than the
    // one originally written, and the original reason should not be re-quoted.
    output logic               tfld_valid_o,
    input  logic               tfld_ready_i,
    output logic signed [31:0] tfld_x0_o,          // footprint rectfx.x0, fx16
    output logic signed [31:0] tfld_z0_o,          // rectfx.y0 -- world Z here
    output logic signed [31:0] tfld_x1_o,          // rectfx.x1
    output logic signed [31:0] tfld_z1_o,          // rectfx.y1 -- world Z here
    output logic        [31:0] tfld_handle_o,      // handle32[program]: NOT a hash
    output logic        [15:0] tfld_cmd_o,         // record source_id, low 16
    output logic        [31:0] tfld_start_tick_o,  // R2 age uniform's origin
    output logic        [31:0] tfld_duration_o,    // R3 phase uniform's span
    output logic       [255:0] tfld_params_o,      // p0..p7, Q16.16 LE, R4..R11
    // THE SET BOUNDARY, added 2026-09-22 (FIELDARM). High on the LAST record
    // of the set the verdict published, and it is a STORED PER-RECORD BIT
    // rather than `(tq_rp + 1 == tq_cp)`.
    //
    // The pointer comparison would have been one wire and it is subtly wrong:
    // `tq_cp` moves at every commit, so a packet that published while the
    // previous packet's records were still draining would MERGE two frames'
    // sets into one -- and the consumer's whole reason for wanting this bit is
    // that a field list belongs to exactly one frame. The merge is not
    // reachable in today's console (staging a packet is thousands of clocks
    // and a drain is a handful), which is precisely why it would have been an
    // unverified promise in a header rather than a property.
    //
    // `zhao_terrain_fieldlist` is the consumer, and what it does with the bit
    // is SEAL its list: a section 9.1 list is per frame, the patch list it
    // refills is per patch job, and nothing else in this console knows where
    // one set ends. That is this block's fact because `tq_cp` is this block's
    // register.
    output logic               tfld_last_o,
    output logic        [31:0] tflds_issued_o,     // records handed downstream
    // ---- DrawProcedural 0x0302, the PRIMITIVE FORGE dispatch ---------------
    // NEW 2026-09-21 (FORGECOMP), under owner rulings R234 D2 (the page kind
    // frozen and the evaluators paid for) and R241 D-TICK-A (frame_tick from
    // pad[11]). Until this arm existed the record fell through the chain below
    // and incremented `unsupported_o` -- the core's own header said so in as
    // many words, and that sentence is now out of date.
    //
    // THE RECORD IS CARRIED, NOT INTERPRETED. `program` and `material` are
    // handle32s this block does not resolve, `kind` is the ROTATED forge_kind
    // numbering this block does not convert (`zhao_forge_pagebank` owns that
    // conversion and owes it the directed check), and `frame_tick` is two bytes
    // of a field the ABI calls padding. An executor that decoded any of them
    // would be a second opinion about a law that lives elsewhere.
    output logic               forge_valid_o,
    input  logic               forge_ready_i,
    output logic        [31:0] forge_program_o,    // handle32[forge_program]
    // THE MATERIAL REFERENCE IS A PAIR, and both halves leave on ONE
    // acceptance -- owner completion ruling 2 (2026-09-22). `forge_material_o`
    // is the COMPLETE handle32[material_set]; `forge_material_id_o` is the
    // independent u16 record index from the record's own bytes. They are two
    // fields of ONE queue entry below, so no arrangement of backpressure can
    // present one draw's set beside another draw's id.
    output logic        [31:0] forge_material_o,   // handle32[material_set], WHOLE
    output logic        [15:0] forge_material_id_o,// u16 record index in that set
    output logic        [ 7:0] forge_kind_o,       // forge_kind, the ROTATED numbering
    output logic        [15:0] forge_frame_tick_o, // R241 D-TICK-A, from frame_tick[2]
    output logic        [15:0] forge_src_id_o,     // record source_id, low 16
    output logic        [31:0] forges_issued_o,
    output logic        [31:0] forge_overflow_o,      // packets refused: > FORGE_Q
    output logic        [31:0] forge_src_truncated_o, // source_id did not fit 16 b

    output logic        [31:0] tfld_overflow_o,    // packets refused: > TFLD_Q
    output logic        [31:0] tfld_src_truncated_o,  // source_id did not fit 16 b

    // ---- MEASURE.TOKENS (R18/R33): the ceiling, then each view's request ---
    // One-cycle pulses in commit phase EX_TOK. MEASURE.TOKENS takes both every
    // cycle (a load is never refused), so no ready is needed or offered.
    // ---- SealFramePlan 0x0003 -> MEASURE.SEALPLAN (directive section 5) ----
    // THE FRAME ADMISSION PLAN, decoded and handed on. `plan_valid_o` is a
    // ONE-CYCLE PULSE raised in the packet's COMMIT walk, never at the
    // record's end: a plan from a packet that is later abandoned must never
    // reach the validator, for the same reason a SetPost from one does not
    // reach POST.COMPOSITE. The fields hold stable across the pulse.
    //
    // THIS BLOCK DOES NOT VALIDATE THE NUMBERS. It checks the record's own
    // hygiene -- the reserved flag bits, and that each field fits the seal
    // width -- and forwards. Whether a plan FITS is a question about four
    // hardware capacities and R7's reservation, and `zhao_measure_sealplan`
    // owns it. Two blocks deciding admission is two blocks that can disagree.
    output logic        plan_valid_o,
    output logic [ 7:0] plan_view_o,
    output logic [ 7:0] plan_flags_o,
    output logic [15:0] plan_res_gen_o,
    output logic [15:0] plan_view_gen_o,
    output logic [15:0] plan_giant_inst_o,
    output logic [17:0] plan_verts_o,        // VERTICES
    output logic [17:0] plan_tris_o,         // TRIANGLES
    output logic [17:0] plan_chunks_o,       // CHUNKS
    output logic [17:0] plan_refs_o,         // TILE REFERENCES
    output logic [17:0] plan_giant_refs_o,   // TILE REFERENCES, the reservation
    output logic [31:0] plans_forwarded_o,   // records that reached the pulse
    output logic [31:0] plans_malformed_o,   // records refused by record hygiene

    output logic        tok_budget_valid_o,
    output logic [31:0] tok_budget_geom0_o,
    output logic [31:0] tok_budget_geom1_o,
    output logic [31:0] tok_budget_frag0_o,
    output logic [31:0] tok_budget_frag1_o,
    output logic [31:0] tok_budget_shared_o,
    output logic        tok_vreq_valid_o,
    output logic        tok_vreq_view_o,
    output logic [31:0] tok_vreq_geom_o,
    output logic [31:0] tok_vreq_frag_o,
    output logic [31:0] contracts_applied_o,  // SetPresentationContract records committed

    // ---- MEASURE.GOVERNOR: the two ratified fields it reads ---------------
    // Both are LEVELS, not pulses: the governor samples them on its own
    // `frame_i` and holds between frames, so a handshake would only invent a
    // second opinion about when a budget is current. They persist across
    // packets exactly as `pc_mode` does -- a packet carrying neither field
    // leaves the governor deciding from the last one that did.
    output logic [ 1:0] gov_view_count_o,
    output logic [31:0] gov_px_err0_o,
    output logic [31:0] gov_px_err1_o,
    // `view_count` OUT OF RANGE. `video_rules.md` 3.1 ratifies TWO views, so 1
    // and 2 are the lawful bytes. The refusal law is `pc_mode`'s, verbatim and
    // for its reason: CMD.DMA's Phase-2 structural walk deliberately omits the
    // decoder's BAD_VALUE step, so an unlawful byte CAN arrive. LAST VALID
    // WINS. UNLIKE `pc_mode` IT IS COUNTED HERE, because CMD.SCHEDULER parses
    // this record and does NOT judge this field -- it judges `mode` only. An
    // unowned verdict is how a field gets adopted silently.
    output logic [31:0] view_count_refused_o,

    // ---- R35/R36: SetPost and SetGradeTable -> POST.COMPOSITE and POST.ECHO --
    // The LOOK (every value POST.COMPOSITE takes per frame) and the grading
    // table's product vectors, applied ONLY while the post lease is idle
    // (`post_idle_i`) and HELD against a pass start while a table is streaming
    // (`post_look_busy_o`), so no pass ever sees half a look or half a table.
    input  logic              post_idle_i,
    output logic              post_look_busy_o,
    output logic [ 7:0]       post_bloom_gain_o,
    output logic              post_grade_valid_o,
    output logic              post_echo_arm_o,     // R35: capture only when armed
    output logic signed [8:0] post_bias_r_o,
    output logic signed [8:0] post_bias_g_o,
    output logic signed [8:0] post_bias_b_o,
    output logic [15:0]       post_flash_rgb_o,
    output logic [ 7:0]       post_flash_amt_o,
    output logic [15:0]       post_ink_rgb_o,
    output logic              post_pv_we_o,
    output logic [ 1:0]       post_pv_sel_o,
    output logic [ 5:0]       post_pv_addr_o,
    output logic [71:0]       post_pv_data_o,

    // ---- R52: DebugTraceArm 0xF003 -> DEBUG.TRACE --------------------------
    // A ONE-CYCLE pulse at the DebugTraceArm record's LAST BYTE, during the
    // walk -- not after the packet's verdict like every other command here.
    // The arm below carries the argument in full; the short form is that the
    // decoder reports a record at its byte 15 and may still reject the packet,
    // so the ring is speculative by contract and arming after the verdict would
    // make it blind to the packet somebody is debugging.
    //
    // THE ORDERING IS EXACT, AND THE SENTENCE NEEDS ITS QUALIFIERS. The first
    // version of this comment said flatly "the arming record itself is not
    // traced, and every record after it in the packet is". Both halves are
    // over-broad, found by review 2026-09-20, and an over-broad guarantee is
    // worse than a hedged one because somebody writes a test against it:
    //
    //   * THE ARM IS NOT TRACED only when stage 0 was not ALREADY armed. A
    //     SECOND DebugTraceArm, in a packet whose ring is already watching the
    //     decoder, IS offered to the ring at its own byte 15 and IS stored --
    //     correctly, because by then it is just another record.
    //   * EVERY LATER RECORD IS TRACED only if the arm was ACCEPTED (a
    //     reserved bit refuses it whole, see `ta_ok_c`), if the mask actually
    //     sets bit 0, and while the ring has room -- a full ring DROPS and
    //     counts, it does not stall the walk.
    //
    // What IS unconditional is the TIMING, and that is the part worth having:
    // CMD.EXEC applies the mask at the arming record's LAST byte and the
    // decoder offers a record at its byte 15, so no record can be offered
    // between the two. The next header cannot complete for sixteen more
    // byte-cycles. Nothing here is a race; the qualifiers are all about
    // whether a store happens at all, never about when.
    //
    // SO THE STORE COUNT IS `N - P`, not `N - 2`: N records in the packet, P
    // the 1-based position of the first ACCEPTED bit-0 arm. `TRACE_SKIP_C` in
    // the smoke bench is P, and it is derived from where the record sits
    // rather than fitted to the answer -- which is why it was wrong by one on
    // first writing and the check caught it.
    //
    // (`spec/commands.zidl`'s DebugTraceArm comment CARRIED the unqualified
    //  sentence and was CORRECTED on 2026-09-20 under owner ruling R108, in the
    //  same commit that granted FORGE.PRIM's five additive `forge_kind` members
    //  and fixed R77's `tmu_mode` comment. One fare, three debts: every edit to
    //  that file changes `ZHAO_ZIDL_SHA256` and forces all five golden captures
    //  to be regenerated through their real producers -- 600 Duo frames, about
    //  an hour -- so the queued comment corrections rode along with the first
    //  change made for a substantive reason. All five captures differ by exactly
    //  68 bytes: the container CRC at [56..59] and the two 32-byte sha fields,
    //  verified byte-wise at the merge, with `abi_version` unmoved.
    //
    //  This paragraph itself was stale FOR ONE COMMIT -- it described the zidl
    //  as uncorrected while sitting in the closure of the very producer run that
    //  corrected it. The ZIDL packet spotted that and could not fix it, because
    //  its brief granted it `spec/commands.zidl` alone and this file was inside
    //  its running producer's closure. Noted because a comment that describes a
    //  debt AFTER the debt is paid is the same false-presence defect this repo
    //  keeps finding in ledgers, one level down.)
    //
    // THIS IS THE RING'S ONLY WRITER, and that is ruling R18's principle (one
    // authority per level) rather than an omission. `zhao_host_regwin`'s tenant
    // 1 can READ `armed` and is refused a write, so a capture is always
    // attributable to the packet that asked for it.
    output logic       dbg_trace_arm_we_o,
    output logic [6:0] dbg_trace_arm_mask_o,
    output logic       dbg_trace_clear_o,

    // ---- evidence ----------------------------------------------------------
    // Every one of these is fired by a directed case in
    // tests/command/cmd_exec_directed.cpp. None is asserted zero without one.
    output logic [31:0] packets_committed_o,
    output logic [31:0] packets_abandoned_o,
    output logic [31:0] views_written_o,
    output logic [31:0] stamps_issued_o,
    output logic [31:0] stamp_overflow_o,
    output logic [31:0] view_range_refused_o,
    output logic [31:0] stamp_src_truncated_o,
    output logic [31:0] draws_issued_o,
    output logic [31:0] draw_overflow_o,
    output logic [31:0] draw_src_truncated_o,
    // R229. `posed_draws_issued_o` counts the subset of `draws_issued_o` that
    // left with `draw_posed_o` high, at the SAME instant and from the SAME
    // dq_head, so the two can never disagree about one draw.
    output logic [31:0] posed_draws_issued_o,
    // R229. A `clip_id` at or above `creature_rules` 2.1's 64 authored slots
    // (`zref::clip_page::kMaxClips`) names a directory row no legal page can
    // contain. REFUSED AND COUNTED, never truncated -- SetPopulation's
    // `plane_nx` law, where narrowing 0x0800 to twelve bits flips a plane.
    //
    // THE REFUSAL DOES NOT DROP THE DRAW, and that is deliberate rather than
    // lenient. The pose cache's own rule 1 already defines what an unusable
    // key does: BAD_ID, "it uses the identity bind pose". So an unrepresentable
    // clip emits the draw with `draw_posed_o` LOW -- an existing, defined,
    // authored behaviour -- rather than making a creature vanish. Refusing the
    // POSE whole is the refusal; refusing the RECORD whole would be a narrowing
    // of function wearing a refusal's clothes.
    output logic [31:0] pose_clip_refused_o,
    // W04. `warp_draws_issued_o` counts the subset of `draws_issued_o` that
    // left with `draw_warp_en_o` high, at the SAME instant and from the SAME
    // dq_head -- the pose counter's discipline, for the same reason.
    output logic [31:0] warp_draws_issued_o,
    // W04 / GEOM.WARP contract section 9's DRAW_INVALID class. A 0x0304 that
    // NAMES A PROGRAM and breaks one of the three between-field rules directive
    // 6.3 states: a mode/resource pair that disagrees, a nonzero `warp_flags`,
    // or a negative displacement bound.
    //
    // AND HERE IT IS THE OPPOSITE OF `pose_clip_refused_o` ABOVE, which is the
    // comparison worth making because the two sit four lines apart and do
    // different things. A bad clip DEGRADES: the cache's rule 1 already defines
    // an unusable key as the bind pose, so the draw survives and only the pose
    // is refused. A bad Warp record cannot degrade to an unwarped draw, because
    // there is no authored behaviour for "the deformation you asked for, minus
    // the deformation" -- it is a confidently wrong shape with nothing
    // downstream able to tell. Section 9 puts this class in the REFUSE column
    // and the draw does not enter the queue.
    output logic [31:0] warp_draw_refused_o,
    output logic [31:0] uploads_issued_o,    // handed to MEM.UPLOAD
    output logic [31:0] upload_overflow_o,   // packets refused: > UPL_Q uploads
    output logic [31:0] post_looks_applied_o,    // SetPost looks handed to POST.COMPOSITE
    output logic [31:0] grade_entries_written_o, // product vectors written into its table
    output logic [31:0] post_refused_o,          // SetPost/SetGradeTable records REFUSED
    output logic [31:0] grade_overflow_o,        // entries refused for want of staging room
    output logic [31:0] trace_arms_applied_o,    // R52: DebugTraceArm records committed
    output logic [31:0] trace_arm_refused_o,     // R52: reserved bits set on the wire
    // A SetView whose `viewport_id` names no viewport in the mode the last
    // contract set. The rect is not written and the bank keeps its previous
    // rectangle (video_rules 3.2); the camera that arrived with it still
    // lands. Distinct from `view_range_refused_o`, which is the BANK select
    // (`view_id`) and is mode-independent -- two different fields, two
    // different refusals, and one counter for both would attribute a bad
    // rectangle to a bad bank.
    output logic [31:0] viewport_range_refused_o,

    // ---- TWOD: SetPlane 0x0306 and DrawSprite 0x0307 ------------------------
    // Owner completion ruling 2026-09-22 item 3. Consumer: `zhao_twod_cmd`,
    // which owns the frame-scoped ring and the rollback; this block stages the
    // FIELDS and hands over the packet's verdict.
    //
    // WHY THESE PRESENT PER RECORD AND NOT PER PACKET, which is the one
    // structural difference from every other arm here. `SetEnvironment` and
    // `SetPopulation` are state and collapse to the last one; `SurfaceStamp`,
    // `DrawForm` and `DrawProcedural` are events and queue HERE. A TWOD frame
    // carries up to sixty-four descriptors at about 335 bits each, and queueing
    // those in this block's flip-flops would cost more than twenty thousand
    // registers for a list the consumer already has to hold in a memory. So the
    // ring lives THERE, in M10K, and its `pkt_commit_o`/`pkt_abandon_o` give it
    // exactly the atomicity the forge arm's `fq_wp`/`fq_cp` give this one --
    // the same two pointers, in the block that owns the storage.
    //
    // THE PRESENTATION IS ONE CYCLE LATE ON PURPOSE. Both records' last field
    // ends at byte 63 of a 64-byte record, so the final byte's capture and
    // `rec_done` land on the SAME edge; presenting at `rec_done` would offer
    // the shadow with its last field one record stale. `*_pend` is set by
    // `rec_done` and the offer is loaded from the shadow on the NEXT cycle,
    // when every byte has settled. The elaboration guards below pin the two
    // assumptions that makes.
    output logic                tpl_valid_o,
    input  logic                tpl_ready_i,
    output logic [ 7:0]         tpl_slot_o,
    output logic [ 7:0]         tpl_role_o,
    output logic [ 7:0]         tpl_blend_o,
    output logic [ 7:0]         tpl_opacity_o,
    output logic [ 7:0]         tpl_format_o,
    output logic [ 7:0]         tpl_wrap_o,
    output logic [ 7:0]         tpl_view_mask_o,
    output logic [ 7:0]         tpl_palette_o,
    output logic [15:0]         tpl_width_o,
    output logic [15:0]         tpl_height_o,
    output logic [15:0]         tpl_flags_o,
    output logic [15:0]         tpl_base_o,
    output logic [ 7:0]         tpl_lstride_o,
    output logic [ 7:0]         tpl_lheight_o,
    output logic signed [31:0]  tpl_a_o,
    output logic signed [31:0]  tpl_b_o,
    output logic signed [31:0]  tpl_c_o,
    output logic signed [31:0]  tpl_d_o,
    output logic signed [31:0]  tpl_u0_o,
    output logic signed [31:0]  tpl_v0_o,
    output logic signed [31:0]  tpl_line_scroll_o,

    output logic                tsp_valid_o,
    input  logic                tsp_ready_i,
    output logic signed [15:0]  tsp_x_o,
    output logic signed [15:0]  tsp_y_o,
    output logic [15:0]         tsp_w_o,
    output logic [15:0]         tsp_h_o,
    output logic [15:0]         tsp_base_o,
    output logic [ 7:0]         tsp_lstride_o,
    output logic [ 7:0]         tsp_lheight_o,
    output logic [ 7:0]         tsp_format_o,
    output logic [ 7:0]         tsp_palette_o,
    output logic [ 7:0]         tsp_blend_o,
    output logic [ 7:0]         tsp_view_mask_o,
    output logic [15:0]         tsp_tint_o,
    output logic [ 7:0]         tsp_order_o,
    output logic [ 7:0]         tsp_flags_o,
    output logic [15:0]         tsp_src_id_o,
    output logic signed [31:0]  tsp_u_o,
    output logic signed [31:0]  tsp_v_o,
    output logic signed [31:0]  tsp_a00_o,
    output logic signed [31:0]  tsp_a01_o,
    output logic signed [31:0]  tsp_a10_o,
    output logic signed [31:0]  tsp_a11_o,

    // The packet's verdict, forwarded to the block that holds the ring. Exactly
    // one of these pulses per packet this block walked to a verdict.
    output logic                twod_pkt_commit_o,
    output logic                twod_pkt_abandon_o,

    // ---- TWOD_PAGE (cartridge kind 15) -> zhao_twod_asset -------------------
    // The SAME pending-upload queue entry `upl_*` carries, forked at the DRAIN
    // by `kind`. Nothing about PublishResource's staging, capacity, overflow or
    // atomicity changes; one head, two destinations, one pop.
    output logic                tld_valid_o,
    input  logic                tld_ready_i,
    output logic [23:0]         tld_index_o,
    output logic [63:0]         tld_hps_addr_o,
    output logic [31:0]         tld_len_o,
    output logic [31:0]         tld_crc_o,
    output logic [15:0]         tld_epoch_o,
    output logic [ 7:0]         tld_dst_slot_o,

    output logic [31:0]         twod_planes_staged_o,
    output logic [31:0]         twod_sprites_staged_o,
    // A record whose offer was still waiting when the NEXT one ended. Records
    // are at least sixty-four bytes apart and `zhao_twod_cmd` holds both
    // readies high permanently, so this reads zero in the console -- and it is
    // reachable with legal stimulus in `tb_cmd_exec_pair` by holding a ready
    // low, which is why it is a counter rather than a comment.
    output logic [31:0]         twod_dropped_o,
    output logic [31:0]         twod_loads_issued_o,
    output logic [31:0] unsupported_o
);

  // Saturating, like every other counter in the composed core: a wrapped
  // counter reads low, and low is the flattering direction.
  `define ZHAO_EXEC_INC(c) if (c != 32'hFFFF_FFFF) c <= c + 32'd1

  // ---- ABI geometry, all of it from the generated package ------------------
  localparam int unsigned SV_VIEW_ID = ZHAO_SET_VIEW_OFF_VIEW_ID;
  localparam int unsigned SV_MAT_LO  = ZHAO_SET_VIEW_OFF_VIEW_PROJECTION;
  localparam int unsigned SV_MAT_HI  = ZHAO_SET_VIEW_OFF_PIXEL_ERROR;  // exclusive
  // `flags` is a u16 and the depth profile is its LOW TWO BITS, so only the
  // low byte is read. From the GENERATED offset like every other field here --
  // the whole point of the block above is that no layout is written by hand.
  localparam int unsigned SV_FLAGS   = ZHAO_SET_VIEW_OFF_FLAGS;

  localparam int unsigned OFF_PATCH = ZHAO_SURFACE_STAMP_OFF_PATCH;
  localparam int unsigned OFF_OPER  = ZHAO_SURFACE_STAMP_OFF_OPERATION;
  localparam int unsigned OFF_TAG   = ZHAO_SURFACE_STAMP_OFF_TAG;
  localparam int unsigned OFF_STR   = ZHAO_SURFACE_STAMP_OFF_STRENGTH;
  // transform2fx declares `fx16 tx; fx16 ty;` first; the generator emits one
  // offset for the struct. The $bits guard below is what keeps these honest.
  localparam int unsigned OFF_TX   = ZHAO_SURFACE_STAMP_OFF_TRANSFORM;
  localparam int unsigned OFF_TY   = ZHAO_SURFACE_STAMP_OFF_TRANSFORM + 4;
  localparam int unsigned OFF_RAD  = ZHAO_SURFACE_STAMP_OFF_RADIUS;
  localparam int unsigned OFF_RING = ZHAO_SURFACE_STAMP_OFF_RING_WIDTH;

  // The record header's source_id, by the same rule.
  localparam int unsigned RH_SRC = ZHAO_SURFACE_STAMP_OFF_H_SOURCE_ID;

  // DrawForm 0x0300, same rule, same package.
  localparam int unsigned OFF_DF_FORM   = ZHAO_DRAW_FORM_OFF_FORM;
  localparam int unsigned OFF_DF_MSET   = ZHAO_DRAW_FORM_OFF_MATERIAL_SET;
  localparam int unsigned OFF_DF_XFORM  = ZHAO_DRAW_FORM_OFF_TRANSFORM;
  localparam int unsigned OFF_DF_VPMASK = ZHAO_DRAW_FORM_OFF_VIEWPORT_MASK;
  localparam int unsigned OFF_DF_WEIGHT = ZHAO_DRAW_FORM_OFF_SEMANTIC_WEIGHT;
  localparam int unsigned OFF_DF_FLAGS  = ZHAO_DRAW_FORM_OFF_FLAGS;

  // DrawPosedForm 0x0305 (R229). ONLY the three fields DrawForm does not have
  // are named here. The other six are read through the OFF_DF_* constants
  // above, because the two records share their first sixteen payload bytes BY
  // RATIFICATION -- and the elaboration guard below is what stops that from
  // being an assumption. Two sets of offsets for one layout is how a block
  // develops a second opinion about where a form handle lives.
  localparam int unsigned OFF_DP_CLIP  = ZHAO_DRAW_POSED_FORM_OFF_CLIP_ID;
  localparam int unsigned OFF_DP_FRAME = ZHAO_DRAW_POSED_FORM_OFF_FRAME_NO;
  localparam int unsigned OFF_DP_SUB   = ZHAO_DRAW_POSED_FORM_OFF_SUB;

  // DrawWarpedForm 0x0304 (owner directive W04). SAME RULE AS ABOVE: only the
  // eleven fields DrawForm does not have are named here; the six it shares are
  // read through the OFF_DF_* constants, and the per-field elaboration guards
  // below are what stop that from being an assumption. Three draw opcodes now
  // read one prefix, which is exactly why there must never be a second set of
  // offsets for it.
  localparam int unsigned OFF_DW_PROG  = ZHAO_DRAW_WARPED_FORM_OFF_WARP_PROGRAM;
  localparam int unsigned OFF_DW_TIME  = ZHAO_DRAW_WARPED_FORM_OFF_TIME;
  localparam int unsigned OFF_DW_PAR0  = ZHAO_DRAW_WARPED_FORM_OFF_PARAMS_0;
  localparam int unsigned OFF_DW_ATT0  = ZHAO_DRAW_WARPED_FORM_OFF_ATTRIBUTES_0;
  localparam int unsigned OFF_DW_ARES  = ZHAO_DRAW_WARPED_FORM_OFF_WARP_ATTRIBUTES;
  localparam int unsigned OFF_DW_AMODE = ZHAO_DRAW_WARPED_FORM_OFF_ATTRIBUTE_MODE;
  localparam int unsigned OFF_DW_WFLG  = ZHAO_DRAW_WARPED_FORM_OFF_WARP_FLAGS;
  localparam int unsigned OFF_DW_BND0  = ZHAO_DRAW_WARPED_FORM_OFF_DISPLACEMENT_BOUND_0;

  // `warp_attribute_mode`'s two members are NOT redeclared here. The generated
  // package already emits them -- `zhao_abi_pkg.sv`'s `WARP_ATTR_INLINE4` /
  // `WARP_ATTR_STREAM4` -- and Verilator said so, VARHIDDEN, the first time
  // this block tried to name them locally. That warning was right and the local
  // copy is deleted rather than waived: two constants for one ABI value is the
  // shape `uncashed_cheques.py` check 3 exists to find, and a shadowing copy is
  // the version that goes stale silently when the .zidl moves.
  //
  // The 6.3 consistency rules below are therefore written against the ENUM the
  // generator produced from the same line of `spec/commands.zidl` the decoder's
  // ZH_ABI_BAD_VALUE check was produced from.

  // `spec/creature_rules.md` 2.1's "64 authored slots", which is also
  // `zref::clip_page::kMaxClips`. NAMED AND EDITABLE rather than implied by the
  // u16 field: it is the ceiling a clip bank's directory must hold, and this
  // block refuses above it rather than truncating.
  localparam int unsigned POSE_MAX_CLIPS = 64;

  // SetView's token request and SetPresentationContract's ceilings (R18/R33).
  localparam int unsigned OFF_SV_GTOK = ZHAO_SET_VIEW_OFF_GEOMETRY_TOKENS;
  localparam int unsigned OFF_SV_FTOK = ZHAO_SET_VIEW_OFF_FRAGMENT_TOKENS;
  // SetView.eye[3] (R63): three fx16 words, lowered onto the projector
  // configuration bus at cfg addresses 19/20/21, where `zhao_view_eye` holds
  // them for TERRAIN.LOD. The offsets come from the generated package like
  // every other field -- this block reads no layout it did not import.
  localparam int unsigned OFF_SV_EYEX = ZHAO_SET_VIEW_OFF_EYE_0;
  localparam int unsigned OFF_SV_EYEY = ZHAO_SET_VIEW_OFF_EYE_1;
  localparam int unsigned OFF_SV_EYEZ = ZHAO_SET_VIEW_OFF_EYE_2;
  // The three cfg addresses those three words go to. Named, not literal, so
  // the map is stated once per block that participates in it.
  localparam int unsigned CFG_EYE_X = 19;
  localparam int unsigned CFG_EYE_Y = 20;
  localparam int unsigned CFG_EYE_Z = 21;

  // SetView.pixel_error and SetPresentationContract.view_count -- the two
  // fields MEASURE.GOVERNOR reads. Imported from the generated package like
  // every other offset; this block reads no layout it did not import.
  // NEITHER GOES ON THE cfg BUS. The eye does because the projector bank is a
  // bank and `zhao_view_eye` is its reader; these two have no bank, so a cfg
  // address for them would invent a holder block to read it back out again.
  localparam int unsigned OFF_SV_PXERR = ZHAO_SET_VIEW_OFF_PIXEL_ERROR;
  localparam int unsigned OFF_PC_VIEWS = ZHAO_SET_PRESENTATION_CONTRACT_OFF_VIEW_COUNT;

  // ---- THE VIEWPORT RECT (cfg 16/17): SetView.viewport_id, lowered ---------
  // `SetView.viewport_id` is a SEPARATE field from `view_id` -- 17 and 16 of
  // the record, the generated package says so -- and until 2026-09-20 this
  // block parsed neither it nor the rectangle it indexes. `view_id` picks the
  // BANK; `viewport_id` picks the RECTANGLE. They are not the same number and
  // a console that conflated them would put view 1 at the origin.
  localparam int unsigned SV_VPORT = ZHAO_SET_VIEW_OFF_VIEWPORT_ID;
  // The two cfg addresses, named for the same reason the eye's three are.
  // `zhao_project_core.sv` reads 16 as {y0[27:16], x0[11:0]} and 17 as
  // {h[27:16], w[11:0]}; `zhao_geom_cull` ignores addr >= 16 and
  // `zhao_view_eye` answers only to 19/20/21, so these two words reach the
  // projector and nobody else.
  localparam int unsigned CFG_VP_ORG = 16;
  localparam int unsigned CFG_VP_EXT = 17;

  // The mode byte of SetPresentationContract. THIS BLOCK ALREADY PARSES THIS
  // RECORD -- the five token ceilings below -- so lifting one more of its
  // fields is the same act, not a second opinion about the console's mode.
  // CMD.SCHEDULER parses the same byte for the raster's own timing; the two
  // agree because the refusal rule here is copied from it verbatim (see the
  // capture).
  localparam int unsigned OFF_PC_MODE = ZHAO_SET_PRESENTATION_CONTRACT_OFF_MODE;

  // THE TABLE, `spec/video_rules.md` section 3.2, differentialled against
  // `zref::render::viewports_of()` (reference/src/zrender/internal.hpp), which
  // has held it since the 2026-08-15 ratification. NAMED CONSTANTS and not
  // literals in the mux: these are the console's picture geometry and they
  // stay editable, which is this repository's standing rule about generated
  // numbers becoming unadjustable ones.
  //
  // Duo's view 1 sits at y = 192 and NOT at x = 256 because video_rules 3
  // makes Duo one logical 256x384 stored surface; x = 256 would describe the
  // DISPLAYED image, which is assembled at scanout and is not what anything
  // upstream of scanout can address.
  localparam int unsigned VP_Z60_W   = 384;
  localparam int unsigned VP_Z60_H   = 240;
  localparam int unsigned VP_STORM_W = 320;
  localparam int unsigned VP_STORM_H = 240;
  localparam int unsigned VP_DUO_W   = 256;
  localparam int unsigned VP_DUO_H   = 192;
  localparam int unsigned VP_DUO_Y1  = 192;
  localparam int unsigned OFF_PC_G0 = ZHAO_SET_PRESENTATION_CONTRACT_OFF_GEOMETRY_TOKENS_0;
  localparam int unsigned OFF_PC_G1 = ZHAO_SET_PRESENTATION_CONTRACT_OFF_GEOMETRY_TOKENS_1;
  localparam int unsigned OFF_PC_F0 = ZHAO_SET_PRESENTATION_CONTRACT_OFF_FRAGMENT_TOKENS_0;
  localparam int unsigned OFF_PC_F1 = ZHAO_SET_PRESENTATION_CONTRACT_OFF_FRAGMENT_TOKENS_1;
  localparam int unsigned OFF_PC_SH = ZHAO_SET_PRESENTATION_CONTRACT_OFF_SHARED_TOKENS;

  // PublishResource 0x0030 (R17), same rule, same package.
  localparam int unsigned OFF_PR_RES   = ZHAO_PUBLISH_RESOURCE_OFF_RESOURCE;
  localparam int unsigned OFF_PR_HLO   = ZHAO_PUBLISH_RESOURCE_OFF_HPS_ADDR_LO;
  localparam int unsigned OFF_PR_HHI   = ZHAO_PUBLISH_RESOURCE_OFF_HPS_ADDR_HI;
  localparam int unsigned OFF_PR_VRAM  = ZHAO_PUBLISH_RESOURCE_OFF_VRAM_DST;
  localparam int unsigned OFF_PR_LEN   = ZHAO_PUBLISH_RESOURCE_OFF_LENGTH;
  localparam int unsigned OFF_PR_CRC   = ZHAO_PUBLISH_RESOURCE_OFF_CRC32C;
  localparam int unsigned OFF_PR_GEN   = ZHAO_PUBLISH_RESOURCE_OFF_NEW_GENERATION;
  localparam int unsigned OFF_PR_EPOCH = ZHAO_PUBLISH_RESOURCE_OFF_EPOCH;
  localparam int unsigned OFF_PR_SLOT  = ZHAO_PUBLISH_RESOURCE_OFF_DST_SLOT;
  localparam int unsigned OFF_PR_KIND  = ZHAO_PUBLISH_RESOURCE_OFF_KIND;

  // SealFramePlan 0x0003 (owner vacation directive section 5), same rule,
  // same package: every offset comes from the generated ABI and never from a
  // literal, so a layout move is a build error rather than a wrong field.
  localparam int unsigned OFF_FP_VIEW  = ZHAO_SEAL_FRAME_PLAN_OFF_VIEW_ID;
  localparam int unsigned OFF_FP_FLAGS = ZHAO_SEAL_FRAME_PLAN_OFF_FLAGS;
  localparam int unsigned OFF_FP_RGEN  = ZHAO_SEAL_FRAME_PLAN_OFF_RESOURCE_GEN;
  localparam int unsigned OFF_FP_VGEN  = ZHAO_SEAL_FRAME_PLAN_OFF_VIEW_GEN;
  localparam int unsigned OFF_FP_GINST = ZHAO_SEAL_FRAME_PLAN_OFF_GIANT_INSTANCE;
  localparam int unsigned OFF_FP_VERTS = ZHAO_SEAL_FRAME_PLAN_OFF_PLAN_VERTS;
  localparam int unsigned OFF_FP_TRIS  = ZHAO_SEAL_FRAME_PLAN_OFF_PLAN_TRIS;
  localparam int unsigned OFF_FP_CKS   = ZHAO_SEAL_FRAME_PLAN_OFF_PLAN_CHUNKS;
  localparam int unsigned OFF_FP_REFS  = ZHAO_SEAL_FRAME_PLAN_OFF_PLAN_REFS;
  localparam int unsigned OFF_FP_GREFS = ZHAO_SEAL_FRAME_PLAN_OFF_GIANT_REFS;

  // SetPost 0x0040 and SetGradeTable 0x0041 (R36), same rule, same package.
  localparam int unsigned OFF_SP_GAIN  = ZHAO_SET_POST_OFF_BLOOM_GAIN;
  localparam int unsigned OFF_SP_FLAGS = ZHAO_SET_POST_OFF_FLAGS;
  localparam int unsigned OFF_SP_AMT   = ZHAO_SET_POST_OFF_FLASH_AMOUNT;
  localparam int unsigned OFF_SP_BR    = ZHAO_SET_POST_OFF_BIAS_R;
  localparam int unsigned OFF_SP_BG    = ZHAO_SET_POST_OFF_BIAS_G;
  localparam int unsigned OFF_SP_BB    = ZHAO_SET_POST_OFF_BIAS_B;
  localparam int unsigned OFF_SP_FLASH = ZHAO_SET_POST_OFF_FLASH;
  localparam int unsigned OFF_SP_INK   = ZHAO_SET_POST_OFF_INK;
  localparam int unsigned OFF_GT_CURVE = ZHAO_SET_GRADE_TABLE_OFF_CURVE;
  localparam int unsigned OFF_GT_FIRST = ZHAO_SET_GRADE_TABLE_OFF_FIRST;
  localparam int unsigned OFF_GT_COUNT = ZHAO_SET_GRADE_TABLE_OFF_COUNT;
  localparam int unsigned OFF_GT_VEC   = ZHAO_SET_GRADE_TABLE_OFF_VECTORS_0;
  localparam int unsigned GT_VEC_BYTES = 72;    // eight entries of nine bytes

  // ---- R52: DebugTraceArm 0xF003 ------------------------------------------
  localparam int unsigned OFF_TA_MASK  = ZHAO_DEBUG_TRACE_ARM_OFF_STAGE_MASK;
  localparam int unsigned OFF_TA_FLAGS = ZHAO_DEBUG_TRACE_ARM_OFF_FLAGS;

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`; a
  // bare module-scope `if` is a syntax error there however clean the lint
  // (CLAUDE.md, "Verilator lint-clean is not Quartus-synthesizable"). And
  // `--lint-only` does not run this block, so it is a build-time guard and is
  // not claimed as a tested one. NOT wrapped in `synthesis translate_off`: the
  // whole point of the Quartus form is that quartus_map evaluates it, and the
  // pragma would put it back exactly where the rule says it must not be.
  initial begin
    if ($bits(zhao_transform2fx_t) != 24 * 8)
      $fatal(1, "zhao_cmd_exec: transform2fx is no longer 24 B; OFF_TX/OFF_TY are stale");
    // 96 until 2026-09-20, 112 since: ruling R63 added `fx16 eye[3]` plus the
    // pad that keeps the record a multiple of the 16-byte alignment. The
    // constant is updated, not relaxed -- this guard's whole value is that it
    // makes a half-done layout move impossible to ship quietly, and a `>=`
    // here would have let exactly that happen.
    if (ZHAO_SET_VIEW_BYTES != 112)
      $fatal(1, "zhao_cmd_exec: SetView record size moved; re-read the offsets");
    // The eye words must land BEFORE the record's last byte, or the commit
    // walk reads a shadow one byte early -- the DrawForm hazard, which that arm
    // needs a bypass for and this one must not.
    if ((OFF_SV_EYEZ + 4) >= ZHAO_SET_VIEW_BYTES)
      $fatal(1, "zhao_cmd_exec: SetView.eye is the record's last field; add the df_flags_c-style bypass");
    // `viewport_id` must arrive AFTER `view_id`, or `sv_view`/`sv_ok` are not
    // settled when it lands and the id goes into the wrong bank slot. Today
    // they are bytes 16 and 17; this guard is what lets the capture assume it
    // rather than re-derive the ordering.
    if (SV_VPORT <= SV_VIEW_ID)
      $fatal(1, "zhao_cmd_exec: SetView.viewport_id no longer follows view_id; sv_view is unsettled when it lands");
    // ... and BEFORE the record's last byte, for the same reason the eye must.
    if ((SV_VPORT + 1) >= ZHAO_SET_VIEW_BYTES)
      $fatal(1, "zhao_cmd_exec: SetView.viewport_id is the record's last field; add the df_flags_c-style bypass");
    // The viewport rect's two cfg addresses must not collide with the eye's
    // three or the profile's one. All six are named constants above, so a
    // collision is an edit away and silent: the projector would take an eye
    // word as a rectangle and centre every vertex somewhere it was not asked.
    if ((CFG_VP_ORG == CFG_EYE_X) || (CFG_VP_ORG == CFG_EYE_Y) || (CFG_VP_ORG == CFG_EYE_Z)
     || (CFG_VP_EXT == CFG_EYE_X) || (CFG_VP_EXT == CFG_EYE_Y) || (CFG_VP_EXT == CFG_EYE_Z)
     || (CFG_VP_ORG == CFG_VP_EXT) || (CFG_VP_ORG == 18) || (CFG_VP_EXT == 18))
      $fatal(1, "zhao_cmd_exec: the viewport rect's cfg addresses collide with the eye's or the profile's");
    // Every rectangle in the table must fit the projector's 12-bit cfg fields.
    // A silent truncation here is a picture drawn to the wrong box with every
    // counter still reading right.
    if ((VP_Z60_W > 4095) || (VP_Z60_H > 4095) || (VP_STORM_W > 4095) || (VP_STORM_H > 4095)
     || (VP_DUO_W > 4095) || (VP_DUO_H > 4095) || (VP_DUO_Y1 > 4095))
      $fatal(1, "zhao_cmd_exec: a viewport table entry exceeds the projector's 12-bit cfg fields");
    if (ZHAO_SURFACE_STAMP_BYTES != 64)
      $fatal(1, "zhao_cmd_exec: SurfaceStamp record size moved; re-read the offsets");
    if ((SV_MAT_HI - SV_MAT_LO) != 64)
      $fatal(1, "zhao_cmd_exec: mat4fx is no longer 16 words");
    if (STAMP_Q < 2)
      $fatal(1, "zhao_cmd_exec: STAMP_Q must be >= 2 (the pointers need a bit)");
    if (DRAW_Q < 2)
      $fatal(1, "zhao_cmd_exec: DRAW_Q must be >= 2 (the pointers need a bit)");
    if (ZHAO_DRAW_FORM_BYTES != 32)
      $fatal(1, "zhao_cmd_exec: DrawForm record size moved; re-read the offsets");
    // Every other record in this block finishes its fields BEFORE its last
    // byte, so a capture never collides with `rec_done`. DrawForm does NOT:
    // `flags` occupies bytes 30..31 of a 32-byte record, so its high byte
    // arrives on the very cycle the record ends. `df_flags_c` below is the
    // combinational answer to that, and it is only correct while this equality
    // holds -- if the layout grows a tail, the register value is already
    // settled at `rec_done` and the bypass becomes wrong rather than needed.
    if ((OFF_DF_FLAGS + 2) != ZHAO_DRAW_FORM_BYTES)
      $fatal(1, "zhao_cmd_exec: DrawForm's flags are no longer its last field; df_flags_c is stale");
    if (ZHAO_DRAW_FORM_OFF_H_SOURCE_ID != RH_SRC)
      $fatal(1, "zhao_cmd_exec: DrawForm's header source_id moved off the shared offset");
    // ---- R229: DrawPosedForm 0x0305 -----------------------------------------
    // THE PREFIX IDENTITY IS LOAD-BEARING AND IS CHECKED, NOT ASSUMED. This arm
    // reads six of its nine fields through DrawForm's offsets; if the ABI ever
    // moves one of them apart, the shared read becomes silently wrong for one
    // of the two opcodes. That is the exact shape of defect this file's other
    // guards exist for, so it gets a guard of its own -- per field, so the
    // message names which one moved.
    if (ZHAO_DRAW_POSED_FORM_BYTES != 48)
      $fatal(1, "zhao_cmd_exec: DrawPosedForm record size moved; re-read the offsets");
    if (ZHAO_DRAW_POSED_FORM_OFF_FORM != OFF_DF_FORM)
      $fatal(1, "zhao_cmd_exec: DrawPosedForm.form no longer shares DrawForm's offset");
    if (ZHAO_DRAW_POSED_FORM_OFF_MATERIAL_SET != OFF_DF_MSET)
      $fatal(1, "zhao_cmd_exec: DrawPosedForm.material_set no longer shares DrawForm's offset");
    if (ZHAO_DRAW_POSED_FORM_OFF_TRANSFORM != OFF_DF_XFORM)
      $fatal(1, "zhao_cmd_exec: DrawPosedForm.transform no longer shares DrawForm's offset");
    if (ZHAO_DRAW_POSED_FORM_OFF_VIEWPORT_MASK != OFF_DF_VPMASK)
      $fatal(1, "zhao_cmd_exec: DrawPosedForm.viewport_mask no longer shares DrawForm's offset");
    if (ZHAO_DRAW_POSED_FORM_OFF_SEMANTIC_WEIGHT != OFF_DF_WEIGHT)
      $fatal(1, "zhao_cmd_exec: DrawPosedForm.semantic_weight no longer shares DrawForm's offset");
    if (ZHAO_DRAW_POSED_FORM_OFF_FLAGS != OFF_DF_FLAGS)
      $fatal(1, "zhao_cmd_exec: DrawPosedForm.flags no longer shares DrawForm's offset");
    if (ZHAO_DRAW_POSED_FORM_OFF_H_SOURCE_ID != RH_SRC)
      $fatal(1, "zhao_cmd_exec: DrawPosedForm's header source_id moved off the shared offset");
    // AND THE OPPOSITE OF DrawForm's HAZARD, stated so it is not copied by
    // habit. DrawForm needs `df_flags_c` because its last field byte IS the
    // record's last byte. DrawPosedForm's last field byte is `sub` at 36 of 48,
    // eleven bytes of pad before `rec_done`, so every capture is settled when
    // the ring write reads it and NO bypass is correct here. `df_flags_c` is
    // already gated on `ZHAO_OP_DRAW_FORM` and must stay that way: applied to
    // this opcode it would inject a byte from the middle of the pad.
    if ((OFF_DP_SUB + 1) >= ZHAO_DRAW_POSED_FORM_BYTES)
      $fatal(1, "zhao_cmd_exec: DrawPosedForm.sub is its last byte; add a df_flags_c-style bypass");
    if (OFF_DP_CLIP < (OFF_DF_FLAGS + 2))
      $fatal(1, "zhao_cmd_exec: DrawPosedForm.clip_id overlaps DrawForm's shared prefix");
    if (POSE_MAX_CLIPS > 65536)
      $fatal(1, "zhao_cmd_exec: POSE_MAX_CLIPS exceeds what a u16 clip_id can name");
    // ---- W04: DrawWarpedForm 0x0304 ----------------------------------------
    // The SAME per-field prefix guards DrawPosedForm gets, for the same reason
    // and with the same per-field messages. This arm reads six of its
    // seventeen fields through DrawForm's offsets; if the ABI ever moves one of
    // them apart, the shared read becomes silently wrong for one of THREE
    // opcodes rather than two.
    if (ZHAO_DRAW_WARPED_FORM_BYTES != 96)
      $fatal(1, "zhao_cmd_exec: DrawWarpedForm record size moved; re-read the offsets");
    if (ZHAO_DRAW_WARPED_FORM_OFF_FORM != OFF_DF_FORM)
      $fatal(1, "zhao_cmd_exec: DrawWarpedForm.form no longer shares DrawForm's offset");
    if (ZHAO_DRAW_WARPED_FORM_OFF_MATERIAL_SET != OFF_DF_MSET)
      $fatal(1, "zhao_cmd_exec: DrawWarpedForm.material_set no longer shares DrawForm's offset");
    if (ZHAO_DRAW_WARPED_FORM_OFF_TRANSFORM != OFF_DF_XFORM)
      $fatal(1, "zhao_cmd_exec: DrawWarpedForm.transform no longer shares DrawForm's offset");
    if (ZHAO_DRAW_WARPED_FORM_OFF_VIEWPORT_MASK != OFF_DF_VPMASK)
      $fatal(1, "zhao_cmd_exec: DrawWarpedForm.viewport_mask no longer shares DrawForm's offset");
    if (ZHAO_DRAW_WARPED_FORM_OFF_SEMANTIC_WEIGHT != OFF_DF_WEIGHT)
      $fatal(1, "zhao_cmd_exec: DrawWarpedForm.semantic_weight no longer shares DrawForm's offset");
    if (ZHAO_DRAW_WARPED_FORM_OFF_FLAGS != OFF_DF_FLAGS)
      $fatal(1, "zhao_cmd_exec: DrawWarpedForm.flags no longer shares DrawForm's offset");
    if (ZHAO_DRAW_WARPED_FORM_OFF_H_SOURCE_ID != RH_SRC)
      $fatal(1, "zhao_cmd_exec: DrawWarpedForm's header source_id moved off the shared offset");
    // DrawForm's `df_flags_c` bypass exists because its last FIELD byte is the
    // record's last byte. This record's last field byte is
    // `displacement_bound[2]`'s high byte at 91 of 96, with four bytes of pad
    // after it, so every capture is settled when the ring write reads it and NO
    // bypass is correct here -- the same statement DrawPosedForm's guard makes,
    // and the guard is what keeps it true rather than remembered.
    if ((OFF_DW_BND0 + 12) >= ZHAO_DRAW_WARPED_FORM_BYTES)
      $fatal(1, "zhao_cmd_exec: DrawWarpedForm.displacement_bound is its last byte; add a df_flags_c-style bypass");
    // The extension must start clear of the sixteen shared bytes, or the two
    // capture arms below would write the same register from one byte.
    if (OFF_DW_PROG < (OFF_DF_FLAGS + 2))
      $fatal(1, "zhao_cmd_exec: DrawWarpedForm.warp_program overlaps DrawForm's shared prefix");
    // The four params and the four attributes are read by ONE indexed loop
    // each, so their elements must be contiguous 4-byte words in declaration
    // order. If the ABI ever spaces them differently the loop reads the wrong
    // bytes silently, which is the flattering direction: it produces numbers.
    if ((ZHAO_DRAW_WARPED_FORM_OFF_PARAMS_1 != OFF_DW_PAR0 + 4)
        || (ZHAO_DRAW_WARPED_FORM_OFF_PARAMS_2 != OFF_DW_PAR0 + 8)
        || (ZHAO_DRAW_WARPED_FORM_OFF_PARAMS_3 != OFF_DW_PAR0 + 12))
      $fatal(1, "zhao_cmd_exec: DrawWarpedForm.params[] is no longer four contiguous words");
    if ((ZHAO_DRAW_WARPED_FORM_OFF_ATTRIBUTES_1 != OFF_DW_ATT0 + 4)
        || (ZHAO_DRAW_WARPED_FORM_OFF_ATTRIBUTES_2 != OFF_DW_ATT0 + 8)
        || (ZHAO_DRAW_WARPED_FORM_OFF_ATTRIBUTES_3 != OFF_DW_ATT0 + 12))
      $fatal(1, "zhao_cmd_exec: DrawWarpedForm.attributes[] is no longer four contiguous words");
    if ((ZHAO_DRAW_WARPED_FORM_OFF_DISPLACEMENT_BOUND_1 != OFF_DW_BND0 + 4)
        || (ZHAO_DRAW_WARPED_FORM_OFF_DISPLACEMENT_BOUND_2 != OFF_DW_BND0 + 8))
      $fatal(1, "zhao_cmd_exec: DrawWarpedForm.displacement_bound[] is no longer three contiguous words");
    if (ZHAO_PUBLISH_RESOURCE_BYTES != 48)
      $fatal(1, "zhao_cmd_exec: PublishResource record size moved; re-read the offsets");
    // Its last field byte (`kind`) must land BEFORE the record's last byte, or
    // the ring write at `rec_done` reads the register one byte early -- the
    // DrawForm hazard, which that arm needs a bypass for and this one does not.
    if ((OFF_PR_KIND + 1) >= ZHAO_PUBLISH_RESOURCE_BYTES)
      $fatal(1, "zhao_cmd_exec: PublishResource's kind is its last byte; add the df_flags_c-style bypass");
    if (ZHAO_SET_PRESENTATION_CONTRACT_BYTES != 48)
      $fatal(1, "zhao_cmd_exec: SetPresentationContract record size moved; re-read the offsets");
    if (UPL_Q < 2 || UPL_PQ < 2)
      $fatal(1, "zhao_cmd_exec: UPL_Q and UPL_PQ must be >= 2 (the pointers need a bit)");
    if (ZHAO_SEAL_FRAME_PLAN_BYTES != 48)
      $fatal(1, "zhao_cmd_exec: SealFramePlan record size moved; re-read the offsets");
    // Every four-byte field must END before the record's last byte, or the
    // commit walk would read a word the stream has not finished delivering.
    // Checked for the LAST one, which is the only one that can straddle.
    if ((OFF_FP_GREFS + 4) > ZHAO_SEAL_FRAME_PLAN_BYTES)
      $fatal(1, "zhao_cmd_exec: SealFramePlan.giant_refs runs past the record");
    if (ZHAO_SET_POST_BYTES != 32)
      $fatal(1, "zhao_cmd_exec: SetPost record size moved; re-read the offsets");
    // Both records finish their fields BEFORE their last byte (ink ends at 29 of
    // 32, the vectors at 91 of 96), so neither needs the DrawForm bypass.
    if ((OFF_SP_INK + 2) >= ZHAO_SET_POST_BYTES)
      $fatal(1, "zhao_cmd_exec: SetPost's ink is its last bytes; add a df_flags_c-style bypass");
    if (ZHAO_SET_GRADE_TABLE_BYTES != 96)
      $fatal(1, "zhao_cmd_exec: SetGradeTable record size moved; re-read the offsets");
    if ((OFF_GT_VEC + GT_VEC_BYTES) >= ZHAO_SET_GRADE_TABLE_BYTES)
      $fatal(1, "zhao_cmd_exec: SetGradeTable's vectors reach its last byte; add a bypass");
    if (OFF_GT_COUNT >= OFF_GT_VEC)
      $fatal(1, "zhao_cmd_exec: SetGradeTable's count must arrive before its vectors");
    if ((GRADE_Q < 2) || ((GRADE_Q & (GRADE_Q - 1)) != 0))
      $fatal(1, "zhao_cmd_exec: GRADE_Q must be a power of two >= 2");
  end

  // ---- the packet walk (glue 3's framing, port for port) -------------------
  logic [31:0] pos;
  logic [31:0] pkt_len_m4_q;
  logic [15:0] rpos, rlen, r_op;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) pkt_len_m4_q <= 32'd0;
    else        pkt_len_m4_q <= pkt_len_i - 32'd4;
  end

  logic take, in_rec_region, rec_done;
  // NOT `pkt_valid_i && pkt_ready_o` -- see the port comment. This is the byte
  // that MOVED, not the byte this block would have accepted.
  assign take          = pkt_valid_i && pkt_fork_ready_i;
  assign in_rec_region = (pos >= 32'd36) && (pos < pkt_len_m4_q);
  // `rlen` is complete from record offset 4 onward, which is exactly the guard
  // glue 3 uses and for the same reason.
  assign rec_done = take && in_rec_region && (rpos >= 16'd4)
                 && ((rpos + 16'd1) == rlen);

  // ---- SetView staging: a shadow of the bank, not of the packet ------------
  logic [31:0] sv_mat [0:1][0:15];
  // The staged depth profile, one per bank view, shadowing the bank's addr-18
  // register exactly as `sv_mat` shadows addresses 0..15. It is NOT a separate
  // dirty bit: a SetView writes the camera and its profile as ONE record, and
  // committing them under two flags would let a view run with this frame's
  // matrix and last frame's profile.
  logic [ 1:0] sv_prof [0:1];
  // The view's token REQUEST, shadowed per view like the profile, and
  // committed under the same dirty bit: one record, one commit (R18).
  logic [31:0] sv_gtok [0:1];
  logic [31:0] sv_ftok [0:1];
  // The view's EYE (R63), shadowed per view exactly like the profile and the
  // token request, and committed under the SAME `sv_dirty` bit. That is the
  // whole reason it rides SetView instead of arriving on its own command: a
  // camera whose matrix is this frame's and whose eye is last frame's would
  // measure LOD against a position the picture was not rendered from, and
  // nothing downstream could detect the mismatch.
  logic [31:0] sv_eyex [0:1];
  logic [31:0] sv_eyey [0:1];
  logic [31:0] sv_eyez [0:1];
  // The view's PIXEL ERROR BUDGET, shadowed per view for the eye's reason and
  // under the eye's dirty bit. MEASURE.GOVERNOR divides by it, so its ZERO is
  // handled where the division is (`zhao_measure_governor.sv` substitutes 1 and
  // records `zero0_r`); this block carries the wire value and does not correct
  // it, because a command executor that quietly repaired a budget would make
  // the governor's zero-guard unreachable and its counter a claim.
  logic [31:0] sv_pxerr [0:1];
  // SetPresentationContract.view_count, one byte, staged raw so the lawfulness
  // test below reads what arrived rather than what was already accepted.
  logic [ 7:0] pc_views;
  // The view's VIEWPORT ID, shadowed per view exactly like the profile and
  // the eye, and committed under the SAME `sv_dirty` bit -- for the same
  // reason: a view whose camera is this frame's and whose rectangle is last
  // frame's would project into a box the picture was not composed for, and
  // nothing downstream could tell.
  logic [ 7:0] sv_vpid [0:1];
  // SetPresentationContract's ceiling, staged whole; `pc_dirty` is its
  // presence in THIS packet. The last contract in a packet wins.
  logic [31:0] pc_g0, pc_g1, pc_f0, pc_f1, pc_sh;
  logic        pc_dirty;
  // THE MODE THE CONTRACT SET -- PERSISTENT across packets, like the ceiling
  // registers beside it and unlike `pc_dirty`. The console's mode is sticky
  // until a contract changes it, so a packet that carries a SetView and no
  // contract indexes the table with the mode the last contract set.
  //
  // WHICH MODE, and why this one. `spec/video_rules.md` 1.1 latches the mode
  // at frame start, effective the NEXT frame, while a SetView commits
  // immediately -- so "the mode on screen" and "the mode the contract set"
  // are two different registers and both exist (`zhao_cmd_scheduler.sv`
  // holds them as `mode_act` and `mode_pend`). video_rules 3.2 records the
  // choice as OPEN and RECOMMENDS the contract's mode; so does
  // FINDINGS-projinput.md D-2. Taken here, for their reason -- the view being
  // configured is the view of the frame that contract governs -- plus one
  // this block can see that they could not: the bank rectangle is LATCHED by
  // this walk and not recomputed per frame, so indexing with the OUTGOING
  // mode writes a rectangle that is wrong from the moment the mode flips and
  // stays wrong until the next SetView. On a Z60 -> Duo switch the outgoing
  // mode makes viewport_id 1 OUT OF RANGE, so view 1 would be refused
  // entirely and keep its reset rectangle. That is not a one-frame blemish.
  logic [ 1:0] pc_mode;
  logic [ 1:0] sv_dirty;
  logic        sv_view;   // the bank view this record targets
  logic        sv_ok;     // ... and whether view_id could address it at all
  logic [23:0] wacc;      // little-endian word assembler: three bytes of
                          // history, like the decoder's pcrc_hist -- the
                          // fourth byte is compared straight off the wire

  logic        sv_in_mat;
  logic [ 5:0] mo;
  assign sv_in_mat = (r_op == ZHAO_OP_SET_VIEW)
                  && (rpos >= 16'(SV_MAT_LO)) && (rpos < 16'(SV_MAT_HI));
  assign mo = 6'(rpos - 16'(SV_MAT_LO));

  // ---- SurfaceStamp staging: a ring of EFFECTS ----------------------------
  localparam int unsigned SQ_PATCH_LO = 0;
  localparam int unsigned SQ_OPER_LO  = 32;
  localparam int unsigned SQ_TAG_LO   = 40;
  localparam int unsigned SQ_STR_LO   = 48;
  localparam int unsigned SQ_TX_LO    = 64;
  localparam int unsigned SQ_TY_LO    = 96;
  localparam int unsigned SQ_RAD_LO   = 128;
  localparam int unsigned SQ_RING_LO  = 160;
  localparam int unsigned SQ_SRC_LO   = 192;
  localparam int unsigned STAMP_W     = 208;
  localparam int unsigned SQW         = $clog2(STAMP_Q);

  logic [31:0] ss_patch, ss_tx, ss_ty, ss_rad, ss_ring;
  logic [ 7:0] ss_oper, ss_tag;
  logic [15:0] ss_str, ss_src;
  logic        ss_src_hi_nz;   // the dropped half of source_id was not zero

  logic [STAMP_W-1:0] sq [0:STAMP_Q-1];
  logic [SQW:0]       sq_wp, sq_rp;
  logic [SQW:0]       sq_occ;
  logic               sq_full;
  assign sq_occ  = sq_wp - sq_rp;
  assign sq_full = (sq_occ >= (SQW+1)'(STAMP_Q));

  logic [STAMP_W-1:0] sq_head;
  assign stamp_patch_o      = sq_head[SQ_PATCH_LO +: 32];
  assign stamp_operation_o  = sq_head[SQ_OPER_LO  +: 8];
  assign stamp_tag_o        = sq_head[SQ_TAG_LO   +: 8];
  assign stamp_strength_o   = sq_head[SQ_STR_LO   +: 16];
  assign stamp_tx_o         = $signed(sq_head[SQ_TX_LO   +: 32]);
  assign stamp_ty_o         = $signed(sq_head[SQ_TY_LO   +: 32]);
  assign stamp_radius_o     = $signed(sq_head[SQ_RAD_LO  +: 32]);
  assign stamp_ring_width_o = $signed(sq_head[SQ_RING_LO +: 32]);
  assign stamp_src_id_o     = sq_head[SQ_SRC_LO   +: 16];

  // ---- DrawForm staging: a ring of EVENTS, for the same reason ------------
  localparam int unsigned DQ_FORM_LO   = 0;
  localparam int unsigned DQ_MSET_LO   = 32;
  localparam int unsigned DQ_XFORM_LO  = 64;
  localparam int unsigned DQ_VPMASK_LO = 96;
  localparam int unsigned DQ_WEIGHT_LO = 104;
  localparam int unsigned DQ_FLAGS_LO  = 112;
  localparam int unsigned DQ_SRC_LO    = 128;
  // R229. The pose rides the SAME entry, so a draw and its pose are latched by
  // one write and dequeued by one read. See the port comment: this is what
  // makes a skew between them structurally impossible rather than merely
  // checked for.
  localparam int unsigned DQ_POSED_LO  = 144;
  localparam int unsigned DQ_CLIP_LO   = 145;
  localparam int unsigned DQ_FRAME_LO  = 161;
  localparam int unsigned DQ_SUB_LO    = 177;
  // W04. ONE BIT in the common draw item, and the eleven-field snapshot lives
  // in the RAM-shaped sidecar below. Directive 7.1 asks for exactly this split
  // BY NAME: "The common draw item holds `warp_enabled` plus a compact
  // descriptor cookie, not an 80-byte Warp record copied through every geometry
  // stage." The cookie here is the draw queue slot itself, which costs nothing
  // to carry because the reader already has it.
  localparam int unsigned DQ_WARP_LO   = 185;
  localparam int unsigned DRAW_W       = 186;
  localparam int unsigned DQW          = $clog2(DRAW_Q);

  // ---- W04's per-draw Warp snapshot, the descriptor sidecar ----------------
  // W05 forbids a mutable global `current_warp` register: "Snapshot every draw.
  // Program handle, time, params, attribute mode, attribute resource and
  // displacement bound belong to THAT draw." Directive 6.2 gives the reason in
  // the negative -- CMD.EXEC groups some state updates before draws, so a
  // last-writer Warp setting "could retroactively change older draws".
  //
  // IT IS INDEXED BY THE DRAW QUEUE SLOT, so its capacity IS the draw queue's
  // capacity. Directive 7.1: "The sidecar capacity must cover the current
  // ordered DRAW queue capacity; it is not permitted to reduce the number of
  // legal draws merely because only two Warp slots were easy to build." Sharing
  // the pointer makes that structural rather than checked -- there is no
  // second capacity to get wrong, and no second overflow rule to declare.
  //
  // WRITTEN BY THE SAME ENABLE AS `dq`, AND THAT IS DELIBERATE RATHER THAN
  // CARELESS. CLAUDE.md's metadata-bank law warns that a DETECTOR whose two
  // operands share one register enable is blind to every timing fault that
  // enable participates in. This is the other side of that coin and R229 states
  // it for the pose: there is no detector here, there is a PAYLOAD, and giving
  // the payload and its draw one enable means no stall can separate them. A
  // checker is not needed for a skew that cannot occur; what would need one is
  // a second, independently clocked path, which is precisely what is refused.
  localparam int unsigned WD_PROG_LO  = 0;    // w=32
  localparam int unsigned WD_TIME_LO  = 32;   // 32
  localparam int unsigned WD_PAR_LO   = 64;   // 128, p0 in the low word
  localparam int unsigned WD_ATTR_LO  = 192;  // 128, a0 in the low word
  localparam int unsigned WD_ARES_LO  = 320;  // w=32
  localparam int unsigned WD_AMODE_LO = 352;  // w=8
  localparam int unsigned WD_BX_LO    = 360;  // w=32
  localparam int unsigned WD_BY_LO    = 392;  // w=32
  localparam int unsigned WD_BZ_LO    = 424;  // w=32
  localparam int unsigned WARP_W      = 456;

  logic [31:0] df_form, df_mset, df_xform;
  logic [ 7:0] df_vpmask, df_weight;
  logic [15:0] df_flags;
  logic        df_src_hi_nz;   // the dropped half of source_id was not zero
  // R229's three additional fields. They are captured ONLY under 0x0305, so a
  // DrawForm can never leave a stale pose behind it: the emit arm below writes
  // `posed` from the OPCODE, not from whether these happen to hold something.
  logic [15:0] dp_clip, dp_frame;
  logic [ 7:0] dp_sub;

  // W04's eleven additional fields. Captured ONLY under 0x0304, by the same
  // rule and for the same reason as the pose above: these bytes are DrawForm's
  // pad region, and capturing them there would let a 0x0300 leave a warp
  // snapshot behind for the next 0x0304 to inherit.
  logic [31:0]        dw_prog, dw_time, dw_ares;
  logic [127:0]       dw_par, dw_attr;
  logic [ 7:0]        dw_amode, dw_wflags;
  logic signed [31:0] dw_bx, dw_by, dw_bz;

  // ALL THREE draw opcodes, named once. Every DrawForm capture below is shared
  // by 0x0305 and 0x0304 because the three records agree byte for byte over
  // that range -- pinned by the per-field elaboration guards above, not by this
  // comment.
  wire df_any_c = (r_op == ZHAO_OP_DRAW_FORM)
               || (r_op == ZHAO_OP_DRAW_POSED_FORM)
               || (r_op == ZHAO_OP_DRAW_WARPED_FORM);

  // ---- W04's DRAW_INVALID, judged at the record's end ----------------------
  // `design/contracts/GEOM.WARP.md` section 9 gives this class one disposition
  // and it is not the pose's: "DRAW_INVALID (bad attribute mode, reserved
  // flags, negative bound, wrong signature) -- refuse BEFORE emitting
  // meshlets." That differs from R229's clip refusal ON PURPOSE. An
  // unrepresentable clip degrades to the bind pose because refusing it would
  // make a creature vanish; a malformed WARP record cannot be degraded the same
  // way, because drawing it unwarped would show a confidently wrong SHAPE and
  // nothing would report it. The record is illegal, so the draw is refused.
  //
  // WHAT IS **NOT** CHECKED HERE, so no reader thinks it was forgotten. The
  // legality of `attribute_mode` itself, and the zero-ness of the four pad
  // bytes, are the GENERATED decoder's (ZH_ABI_BAD_VALUE / the pad map), which
  // is what directive 6.3 means by "payload validation is generated/centralized
  // with the decoder". Re-checking them here would be a second implementation
  // of a ratified rule -- `uncashed_cheques.py` check 3's failure. These three
  // are the ones no generated check can see, because each is a relationship
  // BETWEEN fields or a sign, not a value:
  //
  //   * 6.3: "attribute_mode 0 uses all four inline words and requires
  //     warp_attributes=0" and "attribute_mode 1 requires a resident matching
  //     WARP_ATTRIBUTES resource; inline attribute words must be zero to avoid
  //     unused ambiguous content." Both directions, so neither mode can carry a
  //     value the other mode's consumer would read.
  //   * 6.3: "warp_flags and pads must be zero." `warp_flags` is a FIELD, not a
  //     pad, so the generated pad map does not cover it.
  //   * 6.3 and W11: "displacement bounds must be nonnegative signed fx
  //     values." A negative bound is refused, NEVER absolute-valued into
  //     something usable -- `zhao_geom_warp`'s own `C_NEG_BOUND` says the same
  //     from the other end.
  wire dw_attr_zero_c = (dw_attr == 128'd0);
  wire dw_mode_ok_c   =
      ((dw_amode == WARP_ATTR_INLINE4) && (dw_ares == 32'd0))
   || ((dw_amode == WARP_ATTR_STREAM4) && (dw_ares != 32'd0) && dw_attr_zero_c);
  wire dw_bound_ok_c  = !dw_bx[31] && !dw_by[31] && !dw_bz[31];
  wire dw_ok_c        = dw_mode_ok_c && dw_bound_ok_c && (dw_wflags == 8'd0);

  // A 0x0304 whose `warp_program` is ZERO is an ORDINARY DRAW and is legal.
  // `spec/commands.zidl` says so at the field and W09 is why it must stay that
  // way: "DrawForm disables Warp, performs zero Warp lookups/evaluations, and
  // preserves every existing output." A record that names no program asks for
  // no warp, so it is not held to the warp rules -- checking it against them
  // would refuse a legal draw, which is the narrowing this packet forbids.
  wire dw_armed_c = (r_op == ZHAO_OP_DRAW_WARPED_FORM) && (dw_prog != 32'd0);
  wire dw_bad_c   = dw_armed_c && !dw_ok_c;

  // The pose is representable, judged at the record's end from the captured
  // key. `clip_id` is the only field with a ceiling: `frame_no` past a clip's
  // end and a `sub` phase the bank does not hold are RESOLVABILITY, which the
  // clip bank owns (`zhao_geom_pose_cache`'s rule 1) and this surface has no
  // opinion on.
  wire dp_clip_ok_c = (32'(dp_clip) < 32'(POSE_MAX_CLIPS));

  // THE LAST BYTE OF A DrawForm IS A FIELD BYTE. `df_flags`'s high half
  // arrives on the same cycle `rec_done` fires, so the register still holds
  // the pre-shift value when the ring write reads it. This is the identical
  // bypass `zhao_shell_top_v2`'s glue 3 builds as `w_final`, for the identical
  // reason, and the elaboration guard above is what keeps it honest.
  logic [15:0] df_flags_c;
  always_comb begin
    df_flags_c = df_flags;
    if (in_rec_region && (r_op == ZHAO_OP_DRAW_FORM)
        && (rpos == 16'(OFF_DF_FLAGS + 1)))
      df_flags_c = {pkt_byte_i, df_flags[15:8]};
  end

  logic [DRAW_W-1:0] dq [0:DRAW_Q-1];
  logic [DQW:0]      dq_wp, dq_rp;
  logic [DQW:0]      dq_occ;
  logic              dq_full;
  assign dq_occ  = dq_wp - dq_rp;
  assign dq_full = (dq_occ >= (DQW+1)'(DRAW_Q));

  logic [DRAW_W-1:0] dq_head;
  // W04's sidecar, the same depth and the same pointers as `dq`.
  logic [WARP_W-1:0] wq [0:DRAW_Q-1];
  logic [WARP_W-1:0] wq_head;
  assign draw_form_o           = dq_head[DQ_FORM_LO   +: 32];
  assign draw_material_set_o   = dq_head[DQ_MSET_LO   +: 32];
  assign draw_transform_o      = dq_head[DQ_XFORM_LO  +: 32];
  assign draw_viewport_mask_o  = dq_head[DQ_VPMASK_LO +: 8];
  assign draw_semantic_weight_o = dq_head[DQ_WEIGHT_LO +: 8];
  assign draw_flags_o          = dq_head[DQ_FLAGS_LO  +: 16];
  assign draw_src_id_o         = dq_head[DQ_SRC_LO    +: 16];
  assign draw_posed_o          = dq_head[DQ_POSED_LO];
  assign draw_clip_id_o        = dq_head[DQ_CLIP_LO   +: 16];
  assign draw_frame_no_o       = dq_head[DQ_FRAME_LO  +: 16];
  assign draw_sub_o            = dq_head[DQ_SUB_LO    +: 8];
  assign draw_warp_en_o        = dq_head[DQ_WARP_LO];
  assign draw_warp_program_o   = wq_head[WD_PROG_LO  +: 32];
  assign draw_warp_time_o      = wq_head[WD_TIME_LO  +: 32];
  assign draw_warp_par_o       = wq_head[WD_PAR_LO   +: 128];
  assign draw_warp_attr_o      = wq_head[WD_ATTR_LO  +: 128];
  assign draw_warp_attr_res_o  = wq_head[WD_ARES_LO  +: 32];
  assign draw_warp_attr_mode_o = wq_head[WD_AMODE_LO +: 8];
  assign draw_warp_bx_o        = signed'(wq_head[WD_BX_LO +: 32]);
  assign draw_warp_by_o        = signed'(wq_head[WD_BY_LO +: 32]);
  assign draw_warp_bz_o        = signed'(wq_head[WD_BZ_LO +: 32]);

  // ---- PublishResource staging (R17): a ring of EVENTS, then a PENDING queue
  // An upload is an event (two uploads are two copies), so it stages like a
  // stamp: per packet, bounded by UPL_Q, refused whole on overflow. At commit
  // the staged entries MOVE into `pq`, which is not cleared by the commit and
  // drains to MEM.UPLOAD whenever it is ready -- in any state, including while
  // the next packet stages. That is the one place this block lets a console-
  // visible effect outlive its commit, and it is still AFTER the verdict: an
  // entry reaches `pq` only from EX_UPL, which only a clean verdict enters.
  localparam int unsigned UQ_RES_LO   = 0;
  localparam int unsigned UQ_HLO_LO   = 32;
  localparam int unsigned UQ_HHI_LO   = 64;
  localparam int unsigned UQ_VRAM_LO  = 96;
  localparam int unsigned UQ_LEN_LO   = 128;
  localparam int unsigned UQ_CRC_LO   = 160;
  localparam int unsigned UQ_GEN_LO   = 192;
  localparam int unsigned UQ_EPOCH_LO = 208;
  localparam int unsigned UQ_SLOT_LO  = 224;
  localparam int unsigned UQ_KIND_LO  = 232;
  localparam int unsigned UPL_W       = 240;
  localparam int unsigned UQW         = $clog2(UPL_Q);
  localparam int unsigned PQW         = $clog2(UPL_PQ);

  logic [31:0] pr_res, pr_hlo, pr_hhi, pr_vram, pr_len, pr_crc;
  logic [15:0] pr_gen, pr_epoch;
  logic [ 7:0] pr_slot, pr_kind;

  logic [UPL_W-1:0] uq [0:UPL_Q-1];
  logic [UQW:0]     uq_wp, uq_rp;
  logic [UQW:0]     uq_occ;
  logic             uq_full;
  assign uq_occ  = uq_wp - uq_rp;
  assign uq_full = (uq_occ >= (UQW+1)'(UPL_Q));

  logic [UPL_W-1:0] pq [0:UPL_PQ-1];
  logic [PQW:0]     pq_wp, pq_rp;
  logic [PQW:0]     pq_occ;
  logic             pq_full;
  assign pq_occ  = pq_wp - pq_rp;
  assign pq_full = (pq_occ >= (PQW+1)'(UPL_PQ));

  logic [UPL_W-1:0] pq_head;
  assign pq_head         = pq[pq_rp[PQW-1:0]];
  // THE KIND-15 FORK (completion ruling 2026-09-22 item 3). A TWOD_PAGE's
  // bytes go to `zhao_twod_asset`'s on-chip page store, not to VRAM; every
  // other kind is MEM.UPLOAD's exactly as before. The fork is at the DRAIN and
  // not in the staging, so PublishResource's capacity, overflow, atomicity and
  // field offsets are untouched -- one head, two destinations, one pop.
  logic pq_is_twod_c;
  assign pq_is_twod_c    = (pq_head[UQ_KIND_LO +: 8] == 8'(RESOURCE_KIND_TWOD_PAGE));
  assign upl_valid_o     = (pq_occ != '0) && !pq_is_twod_c;
  assign tld_valid_o     = (pq_occ != '0) &&  pq_is_twod_c;
  assign tld_index_o     = pq_head[UQ_RES_LO + 8 +: 24];
  assign tld_hps_addr_o  = {pq_head[UQ_HHI_LO +: 32], pq_head[UQ_HLO_LO +: 32]};
  assign tld_len_o       = pq_head[UQ_LEN_LO   +: 32];
  assign tld_crc_o       = pq_head[UQ_CRC_LO   +: 32];
  assign tld_epoch_o     = pq_head[UQ_EPOCH_LO +: 16];
  assign tld_dst_slot_o  = pq_head[UQ_SLOT_LO  +: 8];
  // handle32 is {index:24, generation:8} with the INDEX HIGH -- [31:8] -- the
  // packing zcon::detail::handle32, zref::material::Resolver::find and
  // zhao_material_resolve's req_set_index_c all use. The first version of
  // this line took [23:0], and its bench packed the handle the same wrong way,
  // so the round trip agreed with itself; the smoke's MATERIAL_SET would have
  // been published under a key no resolver looks up.
  assign upl_index_o     = pq_head[UQ_RES_LO + 8 +: 24];
  assign upl_kind_o      = pq_head[UQ_KIND_LO  +: 8];
  assign upl_hps_addr_o  = {pq_head[UQ_HHI_LO +: 32], pq_head[UQ_HLO_LO +: 32]};
  assign upl_vram_addr_o = pq_head[UQ_VRAM_LO  +: 32];
  assign upl_len_o       = pq_head[UQ_LEN_LO   +: 32];
  assign upl_epoch_o     = pq_head[UQ_EPOCH_LO +: 16];
  assign upl_dst_slot_o  = pq_head[UQ_SLOT_LO  +: 8];
  assign upl_new_gen_o   = pq_head[UQ_GEN_LO   +: 16];
  assign upl_crc_o       = pq_head[UQ_CRC_LO   +: 32];
  // The handle's generation byte, [7:0] of the staged `resource`: see the
  // port comment for why no port consumes it.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [7:0] pq_handle_gen_unused = pq_head[UQ_RES_LO +: 8];
  /* verilator lint_on UNUSEDSIGNAL */

  // ---- SealFramePlan staging (owner vacation directive section 5) ---------
  // A plan is STATE for the frame it admits: the last VALID one in a packet
  // wins, and it is staged into a shadow (`sp_fp_*`) only when its record ends
  // clean. The capture registers (`fp_*`) are overwritten byte by byte and
  // never leave. This is SetPost's shape, deliberately -- the two commands
  // have the same lifetime and the same abandon rule, so they should not have
  // two different mechanisms.
  logic [ 7:0] fp_view, fp_flags;
  logic [15:0] fp_rgen, fp_vgen, fp_ginst;
  logic [31:0] fp_verts, fp_tris, fp_cks, fp_refs, fp_grefs;

  logic        st_plan_v;
  logic [ 7:0] st_fp_view, st_fp_flags;
  logic [15:0] st_fp_rgen, st_fp_vgen, st_fp_ginst;
  logic [17:0] st_fp_verts, st_fp_tris, st_fp_cks, st_fp_refs, st_fp_grefs;

  // RECORD HYGIENE, AND NOTHING MORE. Two questions only, and both are about
  // the RECORD rather than about the plan:
  //
  //   * are the reserved flag bits zero -- REFUSE, NEVER MASK, exactly as
  //     `sp_ok_c` and `ta_ok_c` above. A flag byte this block does not
  //     understand may mean the host is speaking a later ABI, and masking it
  //     would admit a plan under semantics nobody agreed to;
  //   * does each 32-bit wire field fit the 18-bit seal the console carries.
  //     A field that does not fit cannot be forwarded at all -- truncating it
  //     would turn "65,536 too many vertices" into a small legal number, which
  //     is the flattering direction and exactly the class of error the
  //     validator downstream exists to refuse loudly.
  //
  // EVERYTHING ELSE IS THE VALIDATOR'S. Capacities, the giant's reservation,
  // the generations and the view's existence are `zhao_measure_sealplan`'s
  // questions; it holds the capacities and this block does not.
  logic fp_ok_c;
  assign fp_ok_c = (fp_flags[7:1] == 7'd0)
                && (fp_verts[31:18] == 14'd0)
                && (fp_tris [31:18] == 14'd0)
                && (fp_cks  [31:18] == 14'd0)
                && (fp_refs [31:18] == 14'd0)
                && (fp_grefs[31:18] == 14'd0);

  // ---- SetPost / SetGradeTable staging (R35/R36) ---------------------------
  // A SetPost is STATE, like a SetView: the last VALID one in a packet wins, and
  // it is staged into a shadow (`st_*`) only when its record ends clean. The
  // capture registers (`sp_*`) are overwritten byte by byte and never leave.
  logic [ 7:0] sp_gain, sp_flags, sp_amt;
  logic [15:0] sp_br, sp_bg, sp_bb, sp_flash, sp_ink;
  logic        st_post_v;     // a clean SetPost is staged in this packet
  logic [ 7:0] st_gain, st_amt;
  logic [ 1:0] st_flags;          // the two assigned bits; 7:2 were refused
  logic [ 8:0] st_br, st_bg, st_bb; // nine bits: wider was refused
  logic [15:0] st_flash, st_ink;

  // REFUSE, NEVER MASK. Flags bits 7:2 are unassigned, and the block takes a
  // SIGNED NINE-BIT bias, so a wider value is refused rather than clamped:
  // [-256, 255] is exactly "bits 15:8 all equal bit 8".
  logic sp_ok_c;
  assign sp_ok_c = (sp_flags[7:2] == 6'd0)
                && (sp_br[15:8] == {8{sp_br[8]}})
                && (sp_bg[15:8] == {8{sp_bg[8]}})
                && (sp_bb[15:8] == {8{sp_bb[8]}});

  // ---- DebugTraceArm capture (R52) ----------------------------------------
  // The two wire bytes, captured as they pass. Unlike SetPost there is no
  // shadow: the arming is applied at the record's END rather than after the
  // packet's verdict, and the arm itself says why (the trace surface is
  // speculative by the decoder's own contract).
  //
  // REFUSE, NEVER MASK, exactly as `sp_ok_c` above. `stage_mask` bit 7 is
  // unassigned (DEBUG.TRACE's `arm_mask_i` is seven bits: charter 20.6's seven
  // stages) and `flags` bits 7:1 are unassigned. A record setting either is
  // REFUSED whole and counted -- masking it off would silently arm a different
  // set of stages than the host asked for, and a trace that is not the trace
  // that was requested is worse than no trace.
  logic [7:0] ta_mask, ta_flags;
  logic       ta_ok_c;
  assign ta_ok_c = (ta_mask[7] == 1'b0) && (ta_flags[7:1] == 7'd0);

  // A SetGradeTable is a stream of ENTRIES. Its header (curve, first, count)
  // arrives before its vectors, so each nine-byte entry is validated and written
  // into the staging memory the byte it completes -- never half an entry, and
  // never an entry of a refused record.
  logic [ 7:0] gt_curve, gt_first, gt_count;
  logic [ 3:0] gv_b;          // byte within the entry, 0..8
  logic [ 3:0] gv_k;          // entry within the record, 0..7
  logic [63:0] gv_acc;        // the entry's first eight bytes, little-endian
  logic [ 7:0] gt_size_c;
  logic        gt_ok_c;
  always_comb begin
    unique case (gt_curve)
      8'd0:    gt_size_c = 8'd32;   // R
      8'd1:    gt_size_c = 8'd64;   // G
      8'd2:    gt_size_c = 8'd32;   // B
      default: gt_size_c = 8'd0;    // no such curve
    endcase
  end
  assign gt_ok_c = (gt_size_c != 8'd0) && (gt_count >= 8'd1) && (gt_count <= 8'd8)
                && ({1'b0, gt_first} + {1'b0, gt_count} <= {1'b0, gt_size_c});

  localparam int unsigned GQW = $clog2(GRADE_Q);
  logic [79:0]  gq [0:GRADE_Q-1];   // {sel[1:0], addr[5:0], data[71:0]}
  logic [GQW:0] gq_wp, gq_rp;
  logic         gq_full;
  assign gq_full = ((gq_wp - gq_rp) >= (GQW+1)'(GRADE_Q));

  logic        gq_entry_c;    // this byte completes an entry of the record
  logic        gq_we_c;       // ... and it is written into the staging memory
  logic [79:0] gq_wdata_c;
  logic        in_vec_c;
  assign in_vec_c   = (r_op == ZHAO_OP_SET_GRADE_TABLE)
                   && (rpos >= 16'(OFF_GT_VEC)) && (rpos < 16'(OFF_GT_VEC + GT_VEC_BYTES));
  // `pkt_ready_o` is `st == EX_STAGE`: bytes move only while staging.
  assign gq_entry_c = pkt_ready_o && take && in_rec_region && in_vec_c
                   && (gv_b == 4'd8) && gt_ok_c && ({4'd0, gv_k} < gt_count);
  assign gq_we_c    = gq_entry_c && !gq_full;
  assign gq_wdata_c = {gt_curve[1:0], 6'(gt_first + {4'd0, gv_k}), pkt_byte_i, gv_acc};

  // The memory, written and read in its own block with no reset so it infers
  // an M10K; read REGISTERED at commit.
  logic [79:0] gq_q;
  logic        gq_re_c;
  always_ff @(posedge clk) begin
    if (gq_we_c) gq[gq_wp[GQW-1:0]] <= gq_wdata_c;
    if (gq_re_c) gq_q <= gq[gq_rp[GQW-1:0]];
  end

  // A packet that overflowed the stamp ring is POISONED: it is refused WHOLE at
  // its own verdict, even if the verdict is ZH_ABI_OK. Half of a frame's scars
  // is not a degraded frame, it is a wrong one. A packet that overflowed the
  // DRAW ring is poisoned by the same rule and for a sharper reason: half of a
  // frame's forms is a frame with a creature missing from it.
  logic poisoned;

  // ---- commit ------------------------------------------------------------
  // EX_UPL sits BEFORE EX_DRAW so a packet's uploads start before the draws
  // that may name them. Starting is all it can promise: MEM.UPLOAD publishes
  // asynchronously, and a draw that resolves a not-yet-published resource is
  // MATERIAL.RESOLVE's "a miss STALLS, it never guesses" -- not this block's.
  typedef enum logic [2:0] {
    EX_STAGE,  // walking a packet; NOTHING leaves this block
    EX_TOK,    // the token ceiling, then each view's request (R18)
    EX_CFG,    // draining the view shadow into the matrix bank
    EX_STAMP,  // draining the stamp ring into SURFACE.STAMP
    EX_UPL,    // moving staged uploads into the pending queue
    EX_POST,   // the look and the grading table into POST.COMPOSITE (R36)
    EX_DRAW    // draining the draw ring out of the console
  } ex_e;
  ex_e st;

  // EX_POST's two facts: it has ENTERED (the lease was idle at the door), and a
  // registered read of the staging memory is in flight.
  logic post_in_q, gq_rd_v_q;
  // Held against a pass START from the moment EX_POST enters until it leaves,
  // so a pass that arms meanwhile waits for the whole look and table.
  assign post_look_busy_o = (st == EX_POST) && post_in_q
                         && (st_post_v || gq_rd_v_q || (gq_rp != gq_wp));
  assign gq_re_c = (st == EX_POST) && post_in_q && !st_post_v && (gq_rp != gq_wp) && !gq_rd_v_q;

  logic       cv;   // view being committed
  // FIVE BITS, NOT FOUR, since 2026-09-19: the commit walk is now TWENTY-TWO
  // steps per view -- matrix words 0..15 at cfg addresses 0..15, step 16
  // carrying SetView's depth profile to cfg address 18, steps 17/18/19
  // carrying SetView's eye (R63) to cfg addresses 19/20/21, and steps 20/21
  // carrying the VIEWPORT RECT to cfg addresses 16/17 (2026-09-20). Five bits
  // still suffice; the terminal comparison is `vp_last_c` below and is the
  // only place the walk's length is written down.
  logic [4:0] cw;   // 0..15 matrix, 16 profile, 17..19 eye, 20..21 viewport
  logic [1:0] tk;   // EX_TOK step: 0 contract, 1 view 0, 2 view 1

  // ---- THE VIEWPORT TABLE, evaluated for the view being committed ----------
  // Combinational from `pc_mode` and this view's `viewport_id`. It invents
  // nothing: every number is a named constant above, taken from
  // video_rules 3.2 and the reference oracle's `viewports_of()`.
  //
  // AN OUT-OF-RANGE ID IS REFUSED, NEVER ALIASED (video_rules 3.2). Z60 and
  // Storm have exactly one viewport, Duo has two. On a refusal the walk stops
  // at step 19, the two rect words are never issued, and THE BANK KEEPS ITS
  // PREVIOUS RECTANGLE -- the same law, and the same reason, as
  // `SetView.flags[1:0] == 3` being refused by the bank rather than folded
  // onto a neighbour: one past the end of a table is a diagnosis, not a
  // picture. The matrix, profile and eye of that same SetView still land;
  // refusing the rectangle does not refuse the camera.
  logic        vp_ok_c;
  logic [ 4:0] vp_last_c;
  logic [31:0] vp_org_c, vp_ext_c;

  always_comb begin
    // The packing is `zhao_project_core`'s: cfg 16 = {y0[27:16], x0[11:0]},
    // cfg 17 = {h[27:16], w[11:0]}. The gaps are zero and there is no
    // arithmetic here -- a rectangle is placed, never computed.
    case (pc_mode)
      2'd0: begin  // VIDEO_Z60 -- one viewport, the full canvas
        vp_ok_c  = (sv_vpid[cv] == 8'd0);
        vp_org_c = {4'd0, 12'd0, 4'd0, 12'd0};
        vp_ext_c = {4'd0, 12'(VP_Z60_H), 4'd0, 12'(VP_Z60_W)};
      end
      2'd1: begin  // VIDEO_STORM -- one viewport, the full canvas
        vp_ok_c  = (sv_vpid[cv] == 8'd0);
        vp_org_c = {4'd0, 12'd0, 4'd0, 12'd0};
        vp_ext_c = {4'd0, 12'(VP_STORM_H), 4'd0, 12'(VP_STORM_W)};
      end
      default: begin  // VIDEO_DUO -- two stacked 256x192 view blocks.
        // 2'd3 cannot reach here: `pc_mode` refuses an out-of-range mode byte
        // and holds its previous value, which is the same rule
        // `zhao_cmd_scheduler` applies to the same byte.
        vp_ok_c  = (sv_vpid[cv] <= 8'd1);
        vp_org_c = {4'd0, (sv_vpid[cv] == 8'd1) ? 12'(VP_DUO_Y1) : 12'd0,
                    4'd0, 12'd0};
        vp_ext_c = {4'd0, 12'(VP_DUO_H), 4'd0, 12'(VP_DUO_W)};
      end
    endcase
    // The walk's length, and the ONE place it is written down.
    vp_last_c = vp_ok_c ? 5'd21 : 5'd19;
  end

  // The byte stream is accepted only while staging. During a commit the fork's
  // AND holds CMD.DMA off, which is what makes "nothing escapes early" a
  // STRUCTURAL property rather than a timing argument.
  assign pkt_ready_o = (st == EX_STAGE);

  integer vi, wi;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      pos <= 32'd0; rpos <= 16'd0; rlen <= 16'd0; r_op <= 16'd0;
      for (vi = 0; vi < 2; vi = vi + 1)
        for (wi = 0; wi < 16; wi = wi + 1) sv_mat[vi][wi] <= 32'd0;
        // 2'd0 is WORLD_LONG, the ruling's own zero meaning: a console that
        // never issues a profile behaves as it did before this field existed.
        sv_prof[vi] <= 2'd0;
      for (vi = 0; vi < 2; vi = vi + 1) begin
        sv_gtok[vi] <= 32'd0;
        sv_ftok[vi] <= 32'd0;
        // The origin. `spec/commands.zidl` makes zero a legal, defined eye, so
        // a console that never issues one measures from (0,0,0) rather than
        // from whatever was left in a register.
        sv_eyex[vi] <= 32'd0;
        sv_eyey[vi] <= 32'd0;
        sv_eyez[vi] <= 32'd0;
        // ZERO IS THE RESET AND IT IS NOT A DEFAULT BUDGET. The governor reads
        // zero as "no budget stated" and holds its previous targets; a console
        // that never issues a SetView therefore never makes the governor decide
        // from a number nobody chose.
        sv_pxerr[vi] <= 32'd0;
        // Viewport 0 is legal in every mode, so the reset value is in range
        // for all three and a console that never issues a viewport_id gets
        // the mode's full canvas rather than a refusal.
        sv_vpid[vi] <= 8'd0;
      end
      pc_g0 <= 32'd0; pc_g1 <= 32'd0; pc_f0 <= 32'd0; pc_f1 <= 32'd0; pc_sh <= 32'd0;
      // ONE view until a contract says two -- `video_rules.md` 3.1's floor, and
      // the value `zhao_measure_governor` reads as "view 1 is not presented".
      pc_views         <= 8'd1;
      gov_view_count_o <= 2'd1;
      gov_px_err0_o    <= 32'd0;
      gov_px_err1_o    <= 32'd0;
      view_count_refused_o <= 32'd0;
      // VIDEO_Z60, the same reset mode `zhao_cmd_scheduler` and
      // `zhao_video_mode` both take, so the three agree before any contract.
      pc_mode <= 2'd0;
      pc_dirty <= 1'b0; tk <= 2'd0;
      tok_budget_valid_o <= 1'b0; tok_vreq_valid_o <= 1'b0; tok_vreq_view_o <= 1'b0;
      // R52: DebugTraceArm
      ta_mask <= 8'd0; ta_flags <= 8'd0;
      dbg_trace_arm_we_o <= 1'b0; dbg_trace_arm_mask_o <= 7'd0;
      dbg_trace_clear_o <= 1'b0;
      trace_arms_applied_o <= 32'd0; trace_arm_refused_o <= 32'd0;
      tok_budget_geom0_o <= 32'd0; tok_budget_geom1_o <= 32'd0;
      tok_budget_frag0_o <= 32'd0; tok_budget_frag1_o <= 32'd0;
      tok_budget_shared_o <= 32'd0; tok_vreq_geom_o <= 32'd0; tok_vreq_frag_o <= 32'd0;
      contracts_applied_o <= 32'd0;
      sv_dirty <= 2'd0; sv_view <= 1'b0; sv_ok <= 1'b0; wacc <= 24'd0;
      ss_patch <= 32'd0; ss_tx <= 32'd0; ss_ty <= 32'd0;
      ss_rad <= 32'd0; ss_ring <= 32'd0;
      ss_oper <= 8'd0; ss_tag <= 8'd0; ss_str <= 16'd0; ss_src <= 16'd0;
      ss_src_hi_nz <= 1'b0;
      sq_wp <= '0; sq_rp <= '0; sq_head <= '0;
      df_form <= 32'd0; df_mset <= 32'd0; df_xform <= 32'd0;
      df_vpmask <= 8'd0; df_weight <= 8'd0; df_flags <= 16'd0;
      df_src_hi_nz <= 1'b0;
      dp_clip <= 16'd0; dp_frame <= 16'd0; dp_sub <= 8'd0;   // R229
      // W04. Reset to the INERT snapshot -- program 0 is "no warp", which
      // is the one value that cannot be mistaken for an armed draw.
      dw_prog <= 32'd0; dw_time <= 32'd0; dw_ares <= 32'd0;
      dw_par <= 128'd0; dw_attr <= 128'd0;
      dw_amode <= 8'd0; dw_wflags <= 8'd0;
      dw_bx <= 32'sd0; dw_by <= 32'sd0; dw_bz <= 32'sd0;
      dq_wp <= '0; dq_rp <= '0; dq_head <= '0;
      pr_res <= 32'd0; pr_hlo <= 32'd0; pr_hhi <= 32'd0; pr_vram <= 32'd0;
      pr_len <= 32'd0; pr_crc <= 32'd0; pr_gen <= 16'd0; pr_epoch <= 16'd0;
      pr_slot <= 8'd0; pr_kind <= 8'd0;
      uq_wp <= '0; uq_rp <= '0; pq_wp <= '0; pq_rp <= '0;
      uploads_issued_o <= 32'd0; upload_overflow_o <= 32'd0;
      twod_loads_issued_o <= 32'd0;
      sp_gain <= 8'd0; sp_flags <= 8'd0; sp_amt <= 8'd0;
      sp_br <= 16'd0; sp_bg <= 16'd0; sp_bb <= 16'd0; sp_flash <= 16'd0; sp_ink <= 16'd0;
      st_plan_v <= 1'b0;
      st_fp_view <= 8'd0; st_fp_flags <= 8'd0;
      st_fp_rgen <= 16'd0; st_fp_vgen <= 16'd0; st_fp_ginst <= 16'd0;
      st_fp_verts <= 18'd0; st_fp_tris <= 18'd0; st_fp_cks <= 18'd0;
      st_fp_refs <= 18'd0; st_fp_grefs <= 18'd0;
      fp_view <= 8'd0; fp_flags <= 8'd0;
      fp_rgen <= 16'd0; fp_vgen <= 16'd0; fp_ginst <= 16'd0;
      fp_verts <= 32'd0; fp_tris <= 32'd0; fp_cks <= 32'd0;
      fp_refs <= 32'd0; fp_grefs <= 32'd0;
      plan_valid_o <= 1'b0;
      plan_view_o <= 8'd0; plan_flags_o <= 8'd0;
      plan_res_gen_o <= 16'd0; plan_view_gen_o <= 16'd0; plan_giant_inst_o <= 16'd0;
      plan_verts_o <= 18'd0; plan_tris_o <= 18'd0; plan_chunks_o <= 18'd0;
      plan_refs_o <= 18'd0; plan_giant_refs_o <= 18'd0;
      plans_forwarded_o <= 32'd0; plans_malformed_o <= 32'd0;
      st_post_v <= 1'b0; st_gain <= 8'd0; st_flags <= 2'd0; st_amt <= 8'd0;
      st_br <= 9'd0; st_bg <= 9'd0; st_bb <= 9'd0; st_flash <= 16'd0; st_ink <= 16'd0;
      gt_curve <= 8'd0; gt_first <= 8'd0; gt_count <= 8'd0;
      gv_b <= 4'd0; gv_k <= 4'd0; gv_acc <= 64'd0;
      gq_wp <= '0; gq_rp <= '0;
      post_in_q <= 1'b0; gq_rd_v_q <= 1'b0;
      // THE IDENTITY LOOK until a SetPost says otherwise: no bloom, no grade,
      // no bias, no flash, black ink, and POST.ECHO DISARMED (R35).
      post_bloom_gain_o <= 8'd0; post_grade_valid_o <= 1'b0; post_echo_arm_o <= 1'b0;
      post_bias_r_o <= 9'sd0; post_bias_g_o <= 9'sd0; post_bias_b_o <= 9'sd0;
      post_flash_rgb_o <= 16'd0; post_flash_amt_o <= 8'd0; post_ink_rgb_o <= 16'd0;
      post_pv_we_o <= 1'b0; post_pv_sel_o <= 2'd0; post_pv_addr_o <= 6'd0; post_pv_data_o <= 72'd0;
      post_looks_applied_o <= 32'd0; grade_entries_written_o <= 32'd0;
      post_refused_o <= 32'd0; grade_overflow_o <= 32'd0;
      draw_valid_o <= 1'b0;
      draws_issued_o <= 32'd0; draw_overflow_o <= 32'd0;
      draw_src_truncated_o <= 32'd0;
      posed_draws_issued_o <= 32'd0; pose_clip_refused_o <= 32'd0;   // R229
      wq_head <= '0;                                                 // W04
      warp_draws_issued_o <= 32'd0; warp_draw_refused_o <= 32'd0;    // W04
      poisoned <= 1'b0;
      st <= EX_STAGE; cv <= 1'b0; cw <= 5'd0;
      proj_cfg_we_o <= 1'b0; proj_cfg_view_o <= 1'b0;
      proj_cfg_addr_o <= 5'd0; proj_cfg_data_o <= 32'd0;
      stamp_valid_o <= 1'b0;
      packets_committed_o <= 32'd0; packets_abandoned_o <= 32'd0;
      views_written_o <= 32'd0; stamps_issued_o <= 32'd0;
      stamp_overflow_o <= 32'd0; view_range_refused_o <= 32'd0;
      viewport_range_refused_o <= 32'd0;
      stamp_src_truncated_o <= 32'd0; unsupported_o <= 32'd0;
    end else begin
      proj_cfg_we_o <= 1'b0;   // a write is one cycle wide, always
      plan_valid_o       <= 1'b0;   // the plan hand-off is a one-cycle pulse
      tok_budget_valid_o <= 1'b0;   // both token loads are one-cycle pulses
      tok_vreq_valid_o   <= 1'b0;
      // R52: so is the trace arming -- and `clear_i` MUST be a pulse, not a
      // level. DEBUG.TRACE gates its ring write with `!clear_i` and zeroes both
      // counts while it is high, so a clear left asserted is a ring that stores
      // nothing and reports zero drops: a silently dead instrument, in the
      // flattering direction.
      dbg_trace_arm_we_o <= 1'b0;
      dbg_trace_clear_o  <= 1'b0;
      post_pv_we_o  <= 1'b0;   // so is a table write

      // THE PENDING UPLOAD QUEUE DRAINS IN EVERY STATE. Its entries are
      // committed already; MEM.UPLOAD takes one whenever it is idle.
      if (upl_valid_o && upl_ready_i) begin
        pq_rp <= pq_rp + (PQW+1)'(1);
        `ZHAO_EXEC_INC(uploads_issued_o);
      end else if (tld_valid_o && tld_ready_i) begin
        // The same pop, the other destination. `upl_valid_o` and `tld_valid_o`
        // are mutually exclusive by construction (one head, one kind), so the
        // `else` is belt and braces rather than arbitration.
        pq_rp <= pq_rp + (PQW+1)'(1);
        `ZHAO_EXEC_INC(twod_loads_issued_o);
      end

      unique case (st)

        // ------------------------------------------------------------------
        // STAGE -- walk the packet, capture the fields, touch nothing outside
        // ------------------------------------------------------------------
        EX_STAGE: begin
          if (take) begin
            if (in_rec_region) begin
              // record header
              if      (rpos == 16'd0) r_op[ 7:0] <= pkt_byte_i;
              else if (rpos == 16'd1) r_op[15:8] <= pkt_byte_i;
              else if (rpos == 16'd2) rlen[ 7:0] <= pkt_byte_i;
              else if (rpos == 16'd3) rlen[15:8] <= pkt_byte_i;

              // source_id: the low half is carried, the high half is WATCHED
              if ((rpos >= 16'(RH_SRC)) && (rpos < 16'(RH_SRC + 2)))
                ss_src <= {pkt_byte_i, ss_src[15:8]};
              if ((rpos >= 16'(RH_SRC + 2)) && (rpos < 16'(RH_SRC + 4)) && (pkt_byte_i != 8'd0))
                ss_src_hi_nz <= 1'b1;
              // The DRAW arm keeps its OWN truncation flag rather than sharing
              // `ss_src_hi_nz`: the two counters name two different records,
              // and one flag for both would attribute a form's truncated id to
              // a stamp. Same bytes, same offset, separate accounting.
              if ((rpos >= 16'(RH_SRC + 2)) && (rpos < 16'(RH_SRC + 4)) && (pkt_byte_i != 8'd0))
                df_src_hi_nz <= 1'b1;

              // ---- SetView ------------------------------------------------
              if (r_op == ZHAO_OP_SET_VIEW) begin
                if (rpos == 16'(SV_VIEW_ID)) begin
                  // A u8 against a two-view bank. Refuse, never mask.
                  sv_view <= pkt_byte_i[0];
                  sv_ok   <= (pkt_byte_i < 8'd2);
                  if (pkt_byte_i >= 8'd2) begin
                    `ZHAO_EXEC_INC(view_range_refused_o);
                  end
                end
                // THE VIEWPORT ID. Byte SV_VPORT is 17 and SV_VIEW_ID is 16,
                // so it arrives one byte AFTER the bank select and `sv_view` /
                // `sv_ok` are already settled when this fires -- the same
                // ordering the profile and the matrix words rely on, and it is
                // checked by an elaboration guard rather than assumed.
                // CARRIED WHOLE, all eight bits: the range check belongs at
                // the commit, where the MODE that decides the range is known.
                if ((rpos == 16'(SV_VPORT)) && sv_ok)
                  sv_vpid[sv_view] <= pkt_byte_i;
                // THE DEPTH PROFILE, `flags[1:0]`, owner ruling 2026-08-31 §1.
                // Byte SV_FLAGS is the u16's LOW byte (little-endian) and it
                // arrives AFTER SV_VIEW_ID (18 > 16), so `sv_view`/`sv_ok` are
                // already settled when this fires -- the same ordering the
                // matrix words rely on, and it is checked by the elaboration
                // guard on the record size rather than assumed.
                if ((rpos == 16'(SV_FLAGS)) && sv_ok)
                  sv_prof[sv_view] <= pkt_byte_i[1:0];
                // The token REQUEST (R18). `fragment_tokens` ends on the
                // record's LAST byte, so its final shift lands on the same edge
                // as `rec_done`; that is safe because it goes into a shadow
                // register that nothing reads until EX_TOK.
                if ((rpos >= 16'(OFF_SV_GTOK)) && (rpos < 16'(OFF_SV_GTOK + 4)) && sv_ok)
                  sv_gtok[sv_view] <= {pkt_byte_i, sv_gtok[sv_view][31:8]};
                if ((rpos >= 16'(OFF_SV_FTOK)) && (rpos < 16'(OFF_SV_FTOK + 4)) && sv_ok)
                  sv_ftok[sv_view] <= {pkt_byte_i, sv_ftok[sv_view][31:8]};
                // THE EYE (R63). Same little-endian shift as the tokens, same
                // `sv_ok` gate, same shadow. All three words end before the
                // record's last byte -- the elaboration guard above says so --
                // so the commit walk never reads one early.
                if ((rpos >= 16'(OFF_SV_EYEX)) && (rpos < 16'(OFF_SV_EYEX + 4)) && sv_ok)
                  sv_eyex[sv_view] <= {pkt_byte_i, sv_eyex[sv_view][31:8]};
                if ((rpos >= 16'(OFF_SV_EYEY)) && (rpos < 16'(OFF_SV_EYEY + 4)) && sv_ok)
                  sv_eyey[sv_view] <= {pkt_byte_i, sv_eyey[sv_view][31:8]};
                if ((rpos >= 16'(OFF_SV_EYEZ)) && (rpos < 16'(OFF_SV_EYEZ + 4)) && sv_ok)
                  sv_eyez[sv_view] <= {pkt_byte_i, sv_eyez[sv_view][31:8]};
                // THE PIXEL ERROR BUDGET. Same shift, same gate, same shadow.
                // Byte 84..87 of 96, so it also ends before the record does.
                if ((rpos >= 16'(OFF_SV_PXERR)) && (rpos < 16'(OFF_SV_PXERR + 4)) && sv_ok)
                  sv_pxerr[sv_view] <= {pkt_byte_i, sv_pxerr[sv_view][31:8]};
                if (sv_in_mat) begin
                  wacc <= {pkt_byte_i, wacc[23:8]};
                  if ((mo[1:0] == 2'd3) && sv_ok)
                    sv_mat[sv_view][mo[5:2]] <= {pkt_byte_i, wacc};
                end
              end

              // ---- SetPresentationContract (R18/R33): the ceiling ----------
              if (r_op == ZHAO_OP_SET_PRESENTATION_CONTRACT) begin
                // THE MODE, for the viewport table. The refusal rule is
                // `zhao_cmd_scheduler.sv`'s, verbatim and for its reason:
                // `video_mode` declares members 0..2 ONLY, the Phase-2
                // structural walk in CMD.DMA deliberately does not perform the
                // decoder's BAD_VALUE step, so an unlawful byte CAN arrive
                // here -- and adopting it would index this block's own table
                // out of range. LAST VALID WINS; an unlawful byte holds the
                // previous mode. Not counted here: CMD.SCHEDULER owns the
                // verdict on this byte, and two blocks counting one wire event
                // would report one bad packet as two.
                if ((rpos == 16'(OFF_PC_MODE)) && (pkt_byte_i <= 8'd2))
                  pc_mode <= pkt_byte_i[1:0];
                // THE VIEW COUNT, staged RAW. The lawfulness test is at the
                // record's end rather than here, because a byte refused here
                // would leave nothing to count it against: `pc_views` must
                // still hold the arriving byte for the verdict to look at.
                if (rpos == 16'(OFF_PC_VIEWS)) pc_views <= pkt_byte_i;
                if ((rpos >= 16'(OFF_PC_G0)) && (rpos < 16'(OFF_PC_G0 + 4))) pc_g0 <= {pkt_byte_i, pc_g0[31:8]};
                if ((rpos >= 16'(OFF_PC_G1)) && (rpos < 16'(OFF_PC_G1 + 4))) pc_g1 <= {pkt_byte_i, pc_g1[31:8]};
                if ((rpos >= 16'(OFF_PC_F0)) && (rpos < 16'(OFF_PC_F0 + 4))) pc_f0 <= {pkt_byte_i, pc_f0[31:8]};
                if ((rpos >= 16'(OFF_PC_F1)) && (rpos < 16'(OFF_PC_F1 + 4))) pc_f1 <= {pkt_byte_i, pc_f1[31:8]};
                if ((rpos >= 16'(OFF_PC_SH)) && (rpos < 16'(OFF_PC_SH + 4))) pc_sh <= {pkt_byte_i, pc_sh[31:8]};
              end

              // ---- SurfaceStamp -------------------------------------------
              if (r_op == ZHAO_OP_SURFACE_STAMP) begin
                if ((rpos >= 16'(OFF_PATCH)) && (rpos < 16'(OFF_PATCH + 4)))
                  ss_patch <= {pkt_byte_i, ss_patch[31:8]};
                if (rpos == 16'(OFF_OPER)) ss_oper <= pkt_byte_i;
                if (rpos == 16'(OFF_TAG))  ss_tag  <= pkt_byte_i;
                if ((rpos >= 16'(OFF_STR)) && (rpos < 16'(OFF_STR + 2)))
                  ss_str <= {pkt_byte_i, ss_str[15:8]};
                if ((rpos >= 16'(OFF_TX)) && (rpos < 16'(OFF_TX + 4)))
                  ss_tx <= {pkt_byte_i, ss_tx[31:8]};
                if ((rpos >= 16'(OFF_TY)) && (rpos < 16'(OFF_TY + 4)))
                  ss_ty <= {pkt_byte_i, ss_ty[31:8]};
                if ((rpos >= 16'(OFF_RAD)) && (rpos < 16'(OFF_RAD + 4)))
                  ss_rad <= {pkt_byte_i, ss_rad[31:8]};
                if ((rpos >= 16'(OFF_RING)) && (rpos < 16'(OFF_RING + 4)))
                  ss_ring <= {pkt_byte_i, ss_ring[31:8]};
              end

              // ---- DrawForm 0x0300 AND DrawPosedForm 0x0305 (R229) ---------
              // ONE capture for both, over the sixteen payload bytes the two
              // records share by ratification. The pose fields below are the
              // only thing 0x0305 adds.
              if (df_any_c) begin
                if ((rpos >= 16'(OFF_DF_FORM)) && (rpos < 16'(OFF_DF_FORM + 4)))
                  df_form <= {pkt_byte_i, df_form[31:8]};
                if ((rpos >= 16'(OFF_DF_MSET)) && (rpos < 16'(OFF_DF_MSET + 4)))
                  df_mset <= {pkt_byte_i, df_mset[31:8]};
                if ((rpos >= 16'(OFF_DF_XFORM)) && (rpos < 16'(OFF_DF_XFORM + 4)))
                  df_xform <= {pkt_byte_i, df_xform[31:8]};
                if (rpos == 16'(OFF_DF_VPMASK)) df_vpmask <= pkt_byte_i;
                if (rpos == 16'(OFF_DF_WEIGHT)) df_weight <= pkt_byte_i;
                if ((rpos >= 16'(OFF_DF_FLAGS)) && (rpos < 16'(OFF_DF_FLAGS + 2)))
                  df_flags <= {pkt_byte_i, df_flags[15:8]};
              end

              // ---- DrawPosedForm's animation key alone (R229) --------------
              // Gated on the POSED opcode only: these bytes are DrawForm's pad
              // region, and capturing them there would let a 0x0300 leave a key
              // behind for the next 0x0305 to inherit.
              if (r_op == ZHAO_OP_DRAW_POSED_FORM) begin
                if ((rpos >= 16'(OFF_DP_CLIP)) && (rpos < 16'(OFF_DP_CLIP + 2)))
                  dp_clip  <= {pkt_byte_i, dp_clip[15:8]};
                if ((rpos >= 16'(OFF_DP_FRAME)) && (rpos < 16'(OFF_DP_FRAME + 2)))
                  dp_frame <= {pkt_byte_i, dp_frame[15:8]};
                if (rpos == 16'(OFF_DP_SUB)) dp_sub <= pkt_byte_i;
              end

              // ---- DrawWarpedForm's Warp snapshot alone (W04) --------------
              // Gated on the WARPED opcode only, for the reason immediately
              // above: these bytes are DrawForm's pad region, and a 0x0300
              // capturing them would leave a program handle and a bound behind
              // for the next 0x0304 to inherit. W05's snapshot is only a
              // snapshot if nothing else can write it.
              //
              // The four-word groups are read by an indexed loop rather than
              // four copied lines, and the elaboration guards above are what
              // make that legal: they assert the words are contiguous and in
              // declaration order, so the loop cannot quietly read the wrong
              // bytes if the ABI moves.
              if (r_op == ZHAO_OP_DRAW_WARPED_FORM) begin
                if ((rpos >= 16'(OFF_DW_PROG)) && (rpos < 16'(OFF_DW_PROG + 4)))
                  dw_prog <= {pkt_byte_i, dw_prog[31:8]};
                if ((rpos >= 16'(OFF_DW_TIME)) && (rpos < 16'(OFF_DW_TIME + 4)))
                  dw_time <= {pkt_byte_i, dw_time[31:8]};
                // p0..p3 and a0..a3: one 128-bit shift each, low word first,
                // which is the same little-endian accumulate the 32-bit fields
                // use with four times the run.
                if ((rpos >= 16'(OFF_DW_PAR0)) && (rpos < 16'(OFF_DW_PAR0 + 16)))
                  dw_par <= {pkt_byte_i, dw_par[127:8]};
                if ((rpos >= 16'(OFF_DW_ATT0)) && (rpos < 16'(OFF_DW_ATT0 + 16)))
                  dw_attr <= {pkt_byte_i, dw_attr[127:8]};
                if ((rpos >= 16'(OFF_DW_ARES)) && (rpos < 16'(OFF_DW_ARES + 4)))
                  dw_ares <= {pkt_byte_i, dw_ares[31:8]};
                if (rpos == 16'(OFF_DW_AMODE)) dw_amode  <= pkt_byte_i;
                if (rpos == 16'(OFF_DW_WFLG))  dw_wflags <= pkt_byte_i;
                // The bound is THREE separate words, not one 96-bit shift:
                // each is compared for sign on its own and each is a separate
                // port, so keeping them apart here costs nothing and means the
                // refusal can say which component was negative.
                if ((rpos >= 16'(OFF_DW_BND0)) && (rpos < 16'(OFF_DW_BND0 + 4)))
                  dw_bx <= signed'({pkt_byte_i, dw_bx[31:8]});
                if ((rpos >= 16'(OFF_DW_BND0 + 4)) && (rpos < 16'(OFF_DW_BND0 + 8)))
                  dw_by <= signed'({pkt_byte_i, dw_by[31:8]});
                if ((rpos >= 16'(OFF_DW_BND0 + 8)) && (rpos < 16'(OFF_DW_BND0 + 12)))
                  dw_bz <= signed'({pkt_byte_i, dw_bz[31:8]});
              end

              // ---- PublishResource (R17) ------------------------------------
              if (r_op == ZHAO_OP_PUBLISH_RESOURCE) begin
                if ((rpos >= 16'(OFF_PR_RES)) && (rpos < 16'(OFF_PR_RES + 4)))
                  pr_res <= {pkt_byte_i, pr_res[31:8]};
                if ((rpos >= 16'(OFF_PR_HLO)) && (rpos < 16'(OFF_PR_HLO + 4)))
                  pr_hlo <= {pkt_byte_i, pr_hlo[31:8]};
                if ((rpos >= 16'(OFF_PR_HHI)) && (rpos < 16'(OFF_PR_HHI + 4)))
                  pr_hhi <= {pkt_byte_i, pr_hhi[31:8]};
                if ((rpos >= 16'(OFF_PR_VRAM)) && (rpos < 16'(OFF_PR_VRAM + 4)))
                  pr_vram <= {pkt_byte_i, pr_vram[31:8]};
                if ((rpos >= 16'(OFF_PR_LEN)) && (rpos < 16'(OFF_PR_LEN + 4)))
                  pr_len <= {pkt_byte_i, pr_len[31:8]};
                if ((rpos >= 16'(OFF_PR_CRC)) && (rpos < 16'(OFF_PR_CRC + 4)))
                  pr_crc <= {pkt_byte_i, pr_crc[31:8]};
                if ((rpos >= 16'(OFF_PR_GEN)) && (rpos < 16'(OFF_PR_GEN + 2)))
                  pr_gen <= {pkt_byte_i, pr_gen[15:8]};
                if ((rpos >= 16'(OFF_PR_EPOCH)) && (rpos < 16'(OFF_PR_EPOCH + 2)))
                  pr_epoch <= {pkt_byte_i, pr_epoch[15:8]};
                if (rpos == 16'(OFF_PR_SLOT)) pr_slot <= pkt_byte_i;
                if (rpos == 16'(OFF_PR_KIND)) pr_kind <= pkt_byte_i;
              end

              // ---- SealFramePlan (directive section 5) ---------------------
              if (r_op == ZHAO_OP_SEAL_FRAME_PLAN) begin
                if (rpos == 16'(OFF_FP_VIEW))  fp_view  <= pkt_byte_i;
                if (rpos == 16'(OFF_FP_FLAGS)) fp_flags <= pkt_byte_i;
                if ((rpos >= 16'(OFF_FP_RGEN)) && (rpos < 16'(OFF_FP_RGEN + 2)))
                  fp_rgen <= {pkt_byte_i, fp_rgen[15:8]};
                if ((rpos >= 16'(OFF_FP_VGEN)) && (rpos < 16'(OFF_FP_VGEN + 2)))
                  fp_vgen <= {pkt_byte_i, fp_vgen[15:8]};
                if ((rpos >= 16'(OFF_FP_GINST)) && (rpos < 16'(OFF_FP_GINST + 2)))
                  fp_ginst <= {pkt_byte_i, fp_ginst[15:8]};
                if ((rpos >= 16'(OFF_FP_VERTS)) && (rpos < 16'(OFF_FP_VERTS + 4)))
                  fp_verts <= {pkt_byte_i, fp_verts[31:8]};
                if ((rpos >= 16'(OFF_FP_TRIS)) && (rpos < 16'(OFF_FP_TRIS + 4)))
                  fp_tris <= {pkt_byte_i, fp_tris[31:8]};
                if ((rpos >= 16'(OFF_FP_CKS)) && (rpos < 16'(OFF_FP_CKS + 4)))
                  fp_cks <= {pkt_byte_i, fp_cks[31:8]};
                if ((rpos >= 16'(OFF_FP_REFS)) && (rpos < 16'(OFF_FP_REFS + 4)))
                  fp_refs <= {pkt_byte_i, fp_refs[31:8]};
                if ((rpos >= 16'(OFF_FP_GREFS)) && (rpos < 16'(OFF_FP_GREFS + 4)))
                  fp_grefs <= {pkt_byte_i, fp_grefs[31:8]};
              end

              // ---- SetPost (R36) -------------------------------------------
              if (r_op == ZHAO_OP_SET_POST) begin
                if (rpos == 16'(OFF_SP_GAIN))  sp_gain  <= pkt_byte_i;
                if (rpos == 16'(OFF_SP_FLAGS)) sp_flags <= pkt_byte_i;
                if (rpos == 16'(OFF_SP_AMT))   sp_amt   <= pkt_byte_i;
                if ((rpos >= 16'(OFF_SP_BR)) && (rpos < 16'(OFF_SP_BR + 2)))
                  sp_br <= {pkt_byte_i, sp_br[15:8]};
                if ((rpos >= 16'(OFF_SP_BG)) && (rpos < 16'(OFF_SP_BG + 2)))
                  sp_bg <= {pkt_byte_i, sp_bg[15:8]};
                if ((rpos >= 16'(OFF_SP_BB)) && (rpos < 16'(OFF_SP_BB + 2)))
                  sp_bb <= {pkt_byte_i, sp_bb[15:8]};
                if ((rpos >= 16'(OFF_SP_FLASH)) && (rpos < 16'(OFF_SP_FLASH + 2)))
                  sp_flash <= {pkt_byte_i, sp_flash[15:8]};
                if ((rpos >= 16'(OFF_SP_INK)) && (rpos < 16'(OFF_SP_INK + 2)))
                  sp_ink <= {pkt_byte_i, sp_ink[15:8]};
              end

              // ---- DebugTraceArm (R52) -------------------------------------
              if (r_op == ZHAO_OP_DEBUG_TRACE_ARM) begin
                if (rpos == 16'(OFF_TA_MASK))  ta_mask  <= pkt_byte_i;
                if (rpos == 16'(OFF_TA_FLAGS)) ta_flags <= pkt_byte_i;
              end

              // ---- SetGradeTable (R36) -------------------------------------
              if (r_op == ZHAO_OP_SET_GRADE_TABLE) begin
                if (rpos == 16'(OFF_GT_CURVE)) gt_curve <= pkt_byte_i;
                if (rpos == 16'(OFF_GT_FIRST)) gt_first <= pkt_byte_i;
                if (rpos == 16'(OFF_GT_COUNT)) begin
                  gt_count <= pkt_byte_i;
                  gv_b     <= 4'd0;
                  gv_k     <= 4'd0;
                end
                if (in_vec_c) begin
                  if (gv_b == 4'd8) begin
                    gv_b <= 4'd0;
                    gv_k <= gv_k + 4'd1;
                    // The entry is written by `gq_we_c` (the memory's own
                    // block); a full memory refuses the PACKET whole.
                    if (gq_entry_c) begin
                      if (gq_full) begin
                        poisoned <= 1'b1;
                        `ZHAO_EXEC_INC(grade_overflow_o);
                      end else begin
                        gq_wp <= gq_wp + (GQW+1)'(1);
                      end
                    end
                  end else begin
                    gv_acc <= {pkt_byte_i, gv_acc[63:8]};
                    gv_b   <= gv_b + 4'd1;
                  end
                end
              end

              // ---- the record ends ----------------------------------------
              // Every SetView and SurfaceStamp field lands at or before the
              // last field byte of a well-formed record (83 of 96, 59 of 64),
              // so no capture of theirs collides with this instant. DrawForm
              // is the EXCEPTION -- its `flags` end exactly at byte 31 of 32 --
              // and `df_flags_c` is the bypass that makes the write below read
              // the byte on the wires rather than the register behind it.
              if (rec_done) begin
                if (r_op == ZHAO_OP_SET_VIEW) begin
                  if (sv_ok) sv_dirty[sv_view] <= 1'b1;
                end else if (r_op == ZHAO_OP_SET_PRESENTATION_CONTRACT) begin
                  pc_dirty <= 1'b1;
                end else if (r_op == ZHAO_OP_SURFACE_STAMP) begin
                  if (ss_src_hi_nz) begin
                    `ZHAO_EXEC_INC(stamp_src_truncated_o);
                  end
                  if (sq_full) begin
                    // Declared capacity, refused whole, counted.
                    poisoned <= 1'b1;
                    `ZHAO_EXEC_INC(stamp_overflow_o);
                  end else begin
                    sq[sq_wp[SQW-1:0]] <= {ss_src, ss_ring, ss_rad, ss_ty,
                                           ss_tx, ss_str, ss_tag, ss_oper,
                                           ss_patch};
                    sq_wp <= sq_wp + (SQW+1)'(1);
                  end
                end else if (df_any_c) begin
                  if (df_src_hi_nz) begin
                    `ZHAO_EXEC_INC(draw_src_truncated_o);
                  end
                  // R229. The refusal is judged and counted HERE, at the
                  // record's end, whether or not the queue has room -- an
                  // unrepresentable clip is a property of the record, not of
                  // the backpressure it met. `posed` is written from the
                  // OPCODE and the verdict together, so a 0x0300 is always
                  // bind pose and a refused 0x0305 degrades to bind pose
                  // exactly as `zhao_geom_pose_cache`'s BAD_ID rule does.
                  if ((r_op == ZHAO_OP_DRAW_POSED_FORM) && !dp_clip_ok_c) begin
                    `ZHAO_EXEC_INC(pose_clip_refused_o);
                  end
                  // W04 / contract section 9. DRAW_INVALID is judged HERE, at
                  // the record's end and BEFORE the enqueue, which is what
                  // "refuse before emitting meshlets" means at this block's
                  // level: an entry that never reaches `dq` never reaches
                  // GEOM.DRAWJOB, so no meshlet is emitted for it. Counted
                  // whether or not the queue had room, for the same reason the
                  // clip refusal is: an illegal record is a property of the
                  // record, not of the backpressure it met.
                  if (dw_bad_c) begin
                    `ZHAO_EXEC_INC(warp_draw_refused_o);
                  end
                  // THE REFUSAL IS JUDGED BEFORE THE CAPACITY, AND THE ORDER IS
                  // LOAD-BEARING. Contract section 9 puts DRAW_INVALID's
                  // disposition as "refuse BEFORE emitting meshlets", and a
                  // refused record never needs a queue slot at all -- so asking
                  // `dq_full` about it first would count a `draw_overflow_o`
                  // for a draw that wanted no room, AND POISON THE WHOLE PACKET
                  // over a record that was going to be dropped anyway. One
                  // malformed draw would then cost a frame.
                  //
                  // Written the other way round first, and caught by re-reading
                  // the arm rather than by any gate: every counter still
                  // balanced and every directed case still passed, because no
                  // case presented a full queue and a bad record together. That
                  // is the shape this repo keeps finding -- a wrong answer
                  // nothing is looking at.
                  if (dw_bad_c) begin
                    // Refused whole. The draw is NOT enqueued, NOT degraded to
                    // an unwarped DrawForm, and the packet is NOT poisoned --
                    // one malformed draw is not a malformed frame, and the
                    // other draws in it are lawful.
                    //
                    // WHY NOT DEGRADE, restated where the code does it: an
                    // unwarped draw of a creature whose record asked for a
                    // deformation is a confidently wrong SHAPE that nothing
                    // downstream could detect. Section 9's table puts
                    // DRAW_INVALID in the refuse column and R229's degrade
                    // rule in the other, and this is the difference between
                    // them.
                    ;
                  end else if (dq_full) begin
                    poisoned <= 1'b1;
                    `ZHAO_EXEC_INC(draw_overflow_o);
                  end else begin
                    dq[dq_wp[DQW-1:0]] <= {dw_armed_c,
                                           dp_sub, dp_frame, dp_clip,
                                           ((r_op == ZHAO_OP_DRAW_POSED_FORM)
                                            && dp_clip_ok_c),
                                           ss_src, df_flags_c, df_weight,
                                           df_vpmask, df_xform, df_mset,
                                           df_form};
                    // W05's snapshot, written by THIS enable into THIS slot.
                    // The values are the ones this record carried; a 0x0300 or
                    // a 0x0305 writes the reset values beside its `warp_en`
                    // low, so a stale snapshot cannot survive behind a draw
                    // that did not ask for one.
                    wq[dq_wp[DQW-1:0]] <= dw_armed_c
                        ? {dw_bz, dw_by, dw_bx, dw_amode, dw_ares,
                           dw_attr, dw_par, dw_time, dw_prog}
                        : {WARP_W{1'b0}};
                    dq_wp <= dq_wp + (DQW+1)'(1);
                  end
                end else if (r_op == ZHAO_OP_PUBLISH_RESOURCE) begin
                  if (uq_full) begin
                    // Declared capacity, refused whole, counted: half of a
                    // frame's uploads is a frame drawing a resource that never
                    // arrives.
                    poisoned <= 1'b1;
                    `ZHAO_EXEC_INC(upload_overflow_o);
                  end else begin
                    uq[uq_wp[UQW-1:0]] <= {pr_kind, pr_slot, pr_epoch, pr_gen,
                                           pr_crc, pr_len, pr_vram, pr_hhi,
                                           pr_hlo, pr_res};
                    uq_wp <= uq_wp + (UQW+1)'(1);
                  end
                end else if (r_op == ZHAO_OP_SEAL_FRAME_PLAN) begin
                  // STATE, not an event: the last clean one in the packet wins,
                  // and it reaches the validator only after the packet's
                  // verdict.
                  if (fp_ok_c) begin
                    st_plan_v    <= 1'b1;
                    st_fp_view   <= fp_view;
                    st_fp_flags  <= fp_flags;
                    st_fp_rgen   <= fp_rgen;
                    st_fp_vgen   <= fp_vgen;
                    st_fp_ginst  <= fp_ginst;
                    st_fp_verts  <= fp_verts[17:0];
                    st_fp_tris   <= fp_tris[17:0];
                    st_fp_cks    <= fp_cks[17:0];
                    st_fp_refs   <= fp_refs[17:0];
                    st_fp_grefs  <= fp_grefs[17:0];
                  end else begin
                    `ZHAO_EXEC_INC(plans_malformed_o);
                  end
                end else if (r_op == ZHAO_OP_SET_POST) begin
                  // STATE, not an event: the last clean one in the packet wins.
                  if (sp_ok_c) begin
                    st_post_v <= 1'b1;
                    st_gain <= sp_gain; st_flags <= sp_flags[1:0]; st_amt <= sp_amt;
                    st_br <= sp_br[8:0]; st_bg <= sp_bg[8:0]; st_bb <= sp_bb[8:0];
                    st_flash <= sp_flash; st_ink <= sp_ink;
                  end else begin
                    `ZHAO_EXEC_INC(post_refused_o);
                  end
                end else if (r_op == ZHAO_OP_SET_GRADE_TABLE) begin
                  // Its entries were written as they completed; a refused record
                  // wrote none, and is counted here once.
                  if (!gt_ok_c) begin
                    `ZHAO_EXEC_INC(post_refused_o);
                  end
                end else if (r_op == ZHAO_OP_DEBUG_TRACE_ARM) begin
                  // ---- R52: armed HERE, at the record's end, NOT in a commit
                  // phase -- and that is the one design decision in this arm.
                  //
                  // Every other command in this block stages to a shadow and
                  // commits after the packet's VERDICT, so a corrupt packet
                  // changes no console state. This one deliberately does not,
                  // because the TRACE SURFACE IS ALREADY SPECULATIVE BY
                  // CONTRACT: `zhao_cmd_decoder` raises `rec_valid_o` at record
                  // offset 15 -- the end of the record HEADER -- and its own
                  // port comment says it reports "a record that may still be
                  // rejected", the alternative (buffer and replay) having been
                  // refused as re-introducing the storage it exists to avoid.
                  // The ring therefore traces records of packets that fail.
                  // Arming after the verdict would make the ring unable to
                  // observe the one packet anybody debugging actually cares
                  // about.
                  //
                  // THE ORDERING IS EXACT, not approximate. `rec_done` is the
                  // record's LAST byte; the decoder reported this same record
                  // at its byte 15 and will report the next at ITS byte 15, at
                  // least sixteen cycles later. So: the DebugTraceArm record
                  // itself is NOT traced, and every record after it in the
                  // packet IS -- BUT ONLY UNDER THREE QUALIFIERS, and the
                  // unqualified form of this sentence was an over-broad
                  // guarantee (owner ruling R52, corrected in the zidl under
                  // R108 on 2026-09-20 and corrected here in the same act):
                  //
                  //   1. ONLY IF THE ARM WAS ACCEPTED. A reserved bit refuses
                  //      the record WHOLE and nothing is armed -- which is
                  //      exactly what the committed `-BadTraceArm` smoke
                  //      control exercises, and its own description says so:
                  //      "sets an UNASSIGNED stage_mask bit, DIRECT polarity
                  //      (passes when the record is refused whole and nothing
                  //      is armed)". A control already in the tree was
                  //      demonstrating the case the old sentence denied.
                  //   2. ONLY IF THE MASK SETS BIT 0. Otherwise the stage that
                  //      would trace these records is not selected.
                  //   3. ONLY IF THE RING HAS ROOM. A full ring drops, and a
                  //      dropped record is not a traced one.
                  //
                  // And the "itself is NOT traced" half has its own exception:
                  // a SECOND DebugTraceArm in an already-armed packet IS
                  // traced, because the arm that would have suppressed it has
                  // already taken effect. See the STATE note immediately below
                  // -- the two paragraphs disagreed with each other, in one
                  // file, for as long as the unqualified sentence stood.
                  //
                  // A host reading the ring can rely on the QUALIFIED form.
                  //
                  // STATE, not an event: a second DebugTraceArm later in the
                  // same packet re-arms, and the last one wins.
                  if (ta_ok_c) begin
                    dbg_trace_arm_we_o   <= 1'b1;
                    dbg_trace_arm_mask_o <= ta_mask[6:0];
                    dbg_trace_clear_o    <= ta_flags[0];
                    `ZHAO_EXEC_INC(trace_arms_applied_o);
                  end else begin
                    `ZHAO_EXEC_INC(trace_arm_refused_o);
                  end
                end else if ((r_op != ZHAO_OP_SET_ENVIRONMENT)   // R25: its own block below
                             && (r_op != ZHAO_OP_SET_POPULATION) // R41: likewise
                             && (r_op != ZHAO_OP_TERRAIN_FIELD)  // I34 (a): likewise
           && (r_op != ZHAO_OP_DRAW_PROCEDURAL) // FORGECOMP: its own arm below
                             && (zhao_opcode_record_bytes(r_op) != 32'd0)) begin
                  // A record the ABI defines and this block has no arm for.
                  // Counted rather than narrated, so the distance between the
                  // command surface and the executor is a NUMBER.
                  `ZHAO_EXEC_INC(unsupported_o);
                end
                // An opcode the ABI does not define is CMD.DECODER's to
                // report, and this block deliberately has no opinion on it.

                rpos         <= 16'd0;
                ss_src_hi_nz <= 1'b0;
                df_src_hi_nz <= 1'b0;
              end else begin
                rpos <= rpos + 16'd1;
              end
            end

            if ((pos + 32'd1) >= pkt_len_i) begin
              pos          <= 32'd0;
              rpos         <= 16'd0;
              ss_src_hi_nz <= 1'b0;
              df_src_hi_nz <= 1'b0;
            end else begin
              pos <= pos + 32'd1;
            end
          end

          // ---- the verdict ---------------------------------------------
          // It cannot arrive in any other state: `pkt_ready_o` is low outside
          // EX_STAGE, so CMD.DECODER cannot reach the last byte of a packet
          // while this block is committing the previous one.
          if (verdict_valid_i) begin
            if ((verdict_error_i == ZH_ABI_OK) && !poisoned) begin
              st <= EX_TOK;
              tk <= 2'd0;
              cv <= 1'b0;
              cw <= 5'd0;
            end else begin
              `ZHAO_EXEC_INC(packets_abandoned_o);
              sv_dirty <= 2'd0;
              pc_dirty <= 1'b0;
              sq_wp    <= '0;
              sq_rp    <= '0;
              dq_wp    <= '0;
              dq_rp    <= '0;
              // The STAGING ring only. `pq` holds uploads of packets that
              // already committed; abandoning this one must not cancel those.
              uq_wp    <= '0;
              uq_rp    <= '0;
              // The post state of a refused packet never leaves either.
              st_post_v <= 1'b0;
              // Nor its plan. A frame admitted on a plan from a packet the
              // console refused would be a frame whose quota nobody agreed to.
              st_plan_v <= 1'b0;
              gq_wp     <= '0;
              gq_rp     <= '0;
              poisoned <= 1'b0;
            end
          end
        end

        // ------------------------------------------------------------------
        // COMMIT phase 0 -- the token CEILING, then each view's REQUEST (R18)
        // ------------------------------------------------------------------
        // Three steps, one clock each, in this order and for this reason: the
        // contract's ceiling lands FIRST, so the requests of the same packet
        // are clamped against it rather than against last frame's. A view
        // with no SetView in this packet sends no request and keeps whatever
        // the contract (or its last request) gave it.
        EX_TOK: begin
          unique case (tk)
            2'd0: begin
              if (pc_dirty) begin
                tok_budget_valid_o  <= 1'b1;
                tok_budget_geom0_o  <= pc_g0;
                tok_budget_geom1_o  <= pc_g1;
                tok_budget_frag0_o  <= pc_f0;
                tok_budget_frag1_o  <= pc_f1;
                tok_budget_shared_o <= pc_sh;
                pc_dirty            <= 1'b0;
                `ZHAO_EXEC_INC(contracts_applied_o);
                // THE VIEW COUNT'S VERDICT, taken with the ceiling it arrived
                // beside. `video_rules.md` 3.1 ratifies two views: 1 and 2 are
                // the lawful bytes and everything else -- INCLUDING ZERO, which
                // would present nothing -- holds the previous count and is
                // counted. Adopting the byte would hand MEASURE.GOVERNOR a view
                // count the picture does not have.
                if ((pc_views == 8'd1) || (pc_views == 8'd2))
                  gov_view_count_o <= pc_views[1:0];
                else
                  `ZHAO_EXEC_INC(view_count_refused_o);
              end
              // THE PLAN, HANDED ON FIRST. It is consumed at the next frame
              // begin edge, so the ordering inside a packet cannot matter for
              // correctness -- it is placed in the earliest commit phase
              // anyway, because "the frame's admission agreement is in place
              // before anything else of this packet leaves" is the property a
              // reader will assume, and it costs nothing to make true.
              if (st_plan_v) begin
                plan_valid_o      <= 1'b1;
                plan_view_o       <= st_fp_view;
                plan_flags_o      <= st_fp_flags;
                plan_res_gen_o    <= st_fp_rgen;
                plan_view_gen_o   <= st_fp_vgen;
                plan_giant_inst_o <= st_fp_ginst;
                plan_verts_o      <= st_fp_verts;
                plan_tris_o       <= st_fp_tris;
                plan_chunks_o     <= st_fp_cks;
                plan_refs_o       <= st_fp_refs;
                plan_giant_refs_o <= st_fp_grefs;
                st_plan_v         <= 1'b0;
                `ZHAO_EXEC_INC(plans_forwarded_o);
              end
              tk <= 2'd1;
            end
            2'd1, 2'd2: begin
              if (sv_dirty[tk == 2'd2]) begin
                tok_vreq_valid_o <= 1'b1;
                tok_vreq_view_o  <= (tk == 2'd2);
                tok_vreq_geom_o  <= sv_gtok[tk == 2'd2];
                tok_vreq_frag_o  <= sv_ftok[tk == 2'd2];
              end
              if (tk == 2'd2) st <= EX_CFG;
              tk <= tk + 2'd1;
            end
            default: st <= EX_CFG;
          endcase
        end

        // ------------------------------------------------------------------
        // COMMIT phase 1 -- the view shadow into the matrix bank
        // ------------------------------------------------------------------
        // ISSUE, THEN RETIRE -- one word per two clocks, and the second clock
        // is the point. `proj_cfg_ready_i` exists because the matrix bank has
        // a SECOND writer: the console's own `proj_cfg_*_i` host port. Without
        // a handshake the composer would have to drop one of the two writes
        // and count it, and a dropped matrix word is a silently wrong camera.
        // With it the merge is LOSSLESS in both directions: the host wins the
        // cycle, this block re-presents.
        //
        // CORRECTED 2026-09-21 (gz/cfgarm). This paragraph used to justify the
        // handshake with "the host port ... OWNS cfg addresses 16 and 17 (the
        // viewport rect) THAT NO RATIFIED COMMAND CARRIES". That clause was
        // already false when it was read: the viewport lowering landed on
        // 2026-09-20 as steps 20/21 of this very walk, sixty lines below, and
        // the comment above it says so ("the host port still writes 16/17 and
        // still wins the cycle, but it is now an OVERRIDE rather than the only
        // producer"). One block, two comments, opposite claims -- and the
        // false one is the one a grep for "viewport" reaches first.
        //
        // IT IS LEFT AS A CORRECTION RATHER THAN A DELETION because a comment
        // is a citable source in this tree whether or not anybody meant it to
        // be: `design/prod_manifest.yml`'s `zhao_view_projq88` row refused a
        // composition on exactly this sentence, in exactly these words, and
        // entry I21's blocker 5 then inherited the refusal from the manifest.
        // A caution invented in a comment and quoted by its neighbours is
        // indistinguishable from a ruling.
        //
        // The handshake is NOT stale with it: two writers still exist, the
        // host's override is still a real capability, and removing the
        // handshake would remove function. Only the reason given was wrong.
        //
        // The bubble costs 80 clocks per frame at two full views -- 64 for the
        // matrix words, 4 for the two profile writes and 12 for the two eyes
        // (R63). That is not a throughput question by any measure that matters
        // here: the frame is 1.67M clocks.
        EX_CFG: begin
          if (proj_cfg_we_o) begin
            if (proj_cfg_ready_i) begin
              // Accepted. Retire this word and drop `we` for one cycle.
              // The terminal step is `vp_last_c`: 21 when this view's
              // viewport_id is in range for the mode, 19 when it is not --
              // which is how the rect is REFUSED without refusing the camera
              // that arrived with it.
              if (cw == vp_last_c) begin
                cw           <= 5'd0;
                sv_dirty[cv] <= 1'b0;
                // THE PIXEL ERROR BUDGET LANDS HERE, at the END of the walk,
                // NOT beside the eye at step 17. It is off the cfg bus, so it
                // has no step -- and publishing it at the walk's end is what
                // puts it under the same `sv_dirty` edge as the matrix, the
                // profile, the eye and the rectangle. Publishing it at CAPTURE
                // time instead would move the governor's budget on a record
                // that `verdict_error_i` later abandons -- the clear above sets
                // `sv_dirty <= 2'd0` and no walk ever runs, so nothing else of
                // that record reaches the machine. This field would have been
                // the one exception.
                if (cv) gov_px_err1_o <= sv_pxerr[1];
                else    gov_px_err0_o <= sv_pxerr[0];
                `ZHAO_EXEC_INC(views_written_o);
                if (!vp_ok_c) begin
                  `ZHAO_EXEC_INC(viewport_range_refused_o);
                end
                if (cv) st <= EX_STAMP;
                else    cv <= 1'b1;
              end else begin
                cw <= cw + 5'd1;
              end
            end else begin
              proj_cfg_we_o <= 1'b1;  // refused: hold the identical word
            end
          end else if (sv_dirty[cv]) begin
            proj_cfg_we_o   <= 1'b1;
            proj_cfg_view_o <= cv;
            // STEP 16 IS cfg ADDRESS 18, NOT 16 -- the depth profile. The
            // walk's step numbers and the bus's addresses are deliberately
            // NOT the same sequence, and this mux is the only place that is
            // written down.
            //
            // THE HISTORY, KEPT SHORT BECAUSE IT IS INSTRUCTIVE. Until
            // 2026-09-20 this comment said "addresses 16 and 17 are the
            // viewport rect ... the console's host port owns those two words
            // for now", and before that it justified the absence with "the
            // id-to-rect table is video_rules.md's, not in the ABI at all".
            // THAT JUSTIFICATION ASSERTED A PRESENCE THAT DID NOT EXIST --
            // `video_rules.md` contained no such table (projinput). It does
            // now, section 3.2, and `zref::render::viewports_of()` had held
            // the same rectangles since 2026-08-15 all along.
            //
            // BOTH SENTENCES ARE NOW SPENT: the lowering is the two lines
            // below, and the one decision it needed -- WHICH mode indexes the
            // table, given that video_rules 1.1 latches the mode at frame
            // start while this walk commits immediately -- is TAKEN and
            // recorded at `pc_mode`'s declaration and in 3.2. The host port
            // still writes 16/17 and still wins the cycle, but it is now an
            // OVERRIDE rather than the only producer.
            //
            // STEPS 17/18/19 ARE cfg ADDRESSES 19/20/21 -- the eye (R63),
            // decoded by `zhao_view_eye`, which snoops this same bus.
            // `zhao_project_core` ignores them, which is the property that let
            // the viewport rect and then the profile be added here too.
            // STEPS 20/21 ARE cfg ADDRESSES 16/17 -- the VIEWPORT RECT,
            // landed 2026-09-20. They come LAST so the refusal above can end
            // the walk at 19 without disturbing the matrix, the profile or the
            // eye, and they ride the SAME `sv_dirty` bit as the matrix, so a
            // view's camera and its rectangle cannot land in different frames.
            proj_cfg_addr_o <= (cw == 5'd16) ? 5'd18
                             : (cw == 5'd17) ? 5'(CFG_EYE_X)
                             : (cw == 5'd18) ? 5'(CFG_EYE_Y)
                             : (cw == 5'd19) ? 5'(CFG_EYE_Z)
                             : (cw == 5'd20) ? 5'(CFG_VP_ORG)
                             : (cw == 5'd21) ? 5'(CFG_VP_EXT)
                                             : {1'b0, cw[3:0]};
            proj_cfg_data_o <= (cw == 5'd16) ? {30'd0, sv_prof[cv]}
                             : (cw == 5'd17) ? sv_eyex[cv]
                             : (cw == 5'd18) ? sv_eyey[cv]
                             : (cw == 5'd19) ? sv_eyez[cv]
                             : (cw == 5'd20) ? vp_org_c
                             : (cw == 5'd21) ? vp_ext_c
                                             : sv_mat[cv][cw[3:0]];
          end else begin
            if (cv) st <= EX_STAMP;
            else    cv <= 1'b1;
          end
        end

        // ------------------------------------------------------------------
        // COMMIT phase 2 -- the stamp ring into SURFACE.STAMP
        // ------------------------------------------------------------------
        // One stamp per two clocks, not one per clock: the head is registered
        // and re-presented after each accept. Throughput was not the question
        // this block answers -- a frame's stamps are a handful and the sheet
        // walk behind them is 4,096 texels each.
        EX_STAMP: begin
          if (stamp_valid_o) begin
            if (stamp_ready_i) begin
              stamp_valid_o <= 1'b0;
              sq_rp         <= sq_rp + (SQW+1)'(1);
              `ZHAO_EXEC_INC(stamps_issued_o);
            end
          end else if (sq_occ != '0) begin
            sq_head       <= sq[sq_rp[SQW-1:0]];
            stamp_valid_o <= 1'b1;
          end else begin
            sq_wp <= '0;
            sq_rp <= '0;
            st    <= EX_UPL;
          end
        end

        // ------------------------------------------------------------------
        // COMMIT phase 3 -- staged uploads into the PENDING queue (R17)
        // ------------------------------------------------------------------
        // One per clock. A full pending queue HOLDS the commit rather than
        // dropping an upload: nothing is lost, and the stall is bounded by
        // MEM.UPLOAD draining one request.
        EX_UPL: begin
          if (uq_occ != '0) begin
            if (!pq_full) begin
              pq[pq_wp[PQW-1:0]] <= uq[uq_rp[UQW-1:0]];
              pq_wp <= pq_wp + (PQW+1)'(1);
              uq_rp <= uq_rp + (UQW+1)'(1);
            end
          end else begin
            uq_wp <= '0;
            uq_rp <= '0;
            st    <= EX_POST;
          end
        end

        // ------------------------------------------------------------------
        // COMMIT phase 4 -- the LOOK and the GRADING TABLE (R35/R36)
        // ------------------------------------------------------------------
        // The door is the post lease being IDLE: a pass in flight finishes on
        // the look it started with. Once through the door, `post_look_busy_o`
        // holds any pass that arms from STARTING until the look and every
        // staged table entry are in, so a pass sees all of it or none of it --
        // and this phase never waits on the lease again, because a lease that
        // is armed and held is waiting on THIS, and waiting back would be a
        // deadlock. Before EX_DRAW, so a frame's look is in place before any of
        // its draws leave.
        //
        // The look is one cycle; the table is one entry per TWO clocks (the
        // staging memory's registered read). 128 entries is 256 clocks, once,
        // when a packet carries a table.
        EX_POST: begin
          if (!post_in_q) begin
            if (post_idle_i || (!st_post_v && (gq_rp == gq_wp))) post_in_q <= 1'b1;
          end else if (st_post_v) begin
            post_bloom_gain_o  <= st_gain;
            post_grade_valid_o <= st_flags[0];
            post_echo_arm_o    <= st_flags[1];
            post_bias_r_o      <= $signed(st_br);
            post_bias_g_o      <= $signed(st_bg);
            post_bias_b_o      <= $signed(st_bb);
            post_flash_rgb_o   <= st_flash;
            post_flash_amt_o   <= st_amt;
            post_ink_rgb_o     <= st_ink;
            st_post_v          <= 1'b0;
            `ZHAO_EXEC_INC(post_looks_applied_o);
          end else if (gq_rd_v_q) begin
            post_pv_we_o   <= 1'b1;
            post_pv_sel_o  <= gq_q[79:78];
            post_pv_addr_o <= gq_q[77:72];
            post_pv_data_o <= gq_q[71:0];
            gq_rp          <= gq_rp + (GQW+1)'(1);
            gq_rd_v_q      <= 1'b0;
            `ZHAO_EXEC_INC(grade_entries_written_o);
          end else if (gq_rp != gq_wp) begin
            gq_rd_v_q <= 1'b1;                    // `gq_re_c` issued the read
          end else begin
            gq_wp     <= '0;
            gq_rp     <= '0;
            post_in_q <= 1'b0;
            st        <= EX_DRAW;
          end
        end

        // ------------------------------------------------------------------
        // COMMIT phase 3 -- the draw ring out of the console
        // ------------------------------------------------------------------
        // LAST, and the order is the point rather than a convenience: by the
        // time the first draw leaves, EX_CFG has retired every matrix word
        // this packet carried, so a form is always dispatched against the
        // camera its own packet set. The header argues this at length.
        //
        // One draw per two clocks, the stamp ring's shape and the stamp ring's
        // reason: the head is registered and re-presented after each accept,
        // and a frame's forms are a handful against a meshlet walk of
        // thousands of vertices behind each one.
        //
        // `packets_committed_o` moved here from EX_STAMP. It still counts one
        // per committed packet -- it is the LAST thing a commit does, and the
        // last thing is now this.
        EX_DRAW: begin
          if (draw_valid_o) begin
            if (draw_ready_i) begin
              draw_valid_o <= 1'b0;
              dq_rp        <= dq_rp + (DQW+1)'(1);
              `ZHAO_EXEC_INC(draws_issued_o);
              // R229. Read from `dq_head` -- THE SAME REGISTER the consumer
              // just accepted -- on the same cycle as `draws_issued_o`. Not
              // from the opcode or the staging shadow: those have already
              // moved on to the next record, and a counter differencing a
              // value against a later version of itself is CLAUDE.md's
              // lockstep-corruption shape. `posed_draws_issued_o` is a strict
              // subset of `draws_issued_o` by construction.
              if (draw_posed_o) `ZHAO_EXEC_INC(posed_draws_issued_o);
              // W04, and the SAME rule applied: read the bit out of `dq_head`,
              // the register the consumer just accepted, never out of `r_op`.
              // `warp_draws_issued_o` is a strict subset of `draws_issued_o`.
              if (draw_warp_en_o) `ZHAO_EXEC_INC(warp_draws_issued_o);
            end
          end else if (dq_occ != '0) begin
            dq_head      <= dq[dq_rp[DQW-1:0]];
            // W04. ONE index, ONE enable, ONE cycle -- the draw item and its
            // Warp snapshot are read as a pair, exactly as they were written
            // as a pair. This is the register-enable discipline R229 adopted
            // for the pose, and it is why `draw_warp_*_o` needs no identity
            // tag: there is no cycle in which `dq_head` names one draw and
            // `wq_head` names another.
            wq_head      <= wq[dq_rp[DQW-1:0]];
            draw_valid_o <= 1'b1;
          end else begin
            `ZHAO_EXEC_INC(packets_committed_o);
            dq_wp <= '0;
            dq_rp <= '0;
            st    <= EX_STAGE;
          end
        end

        default: st <= EX_STAGE;
      endcase
    end
  end

  // ==========================================================================
  // R25: SetEnvironment 0x0311 -> zhao_light_env. SELF-CONTAINED ON PURPOSE:
  // it reads the packet walk and the verdict and touches nothing the state
  // machine above owns, so CMD.EXEC's other arms are unchanged by it.
  //
  // THE SAME ATOMICITY AS SetView: fields land in a shadow during STAGE, the
  // record's end marks it pending, and it leaves this block only on a CLEAN
  // verdict (ZH_ABI_OK and not poisoned) -- an abandoned packet's environment
  // is discarded with the rest of it. Several SetEnvironment records in one
  // packet: the last one wins (4a: per-frame global state). All four fields
  // end by byte 23 of a 48-byte record, so no capture collides with
  // `rec_done`, and the elaboration guard below keeps that true.
  // ==========================================================================
  localparam int unsigned OFF_EN_YAW   = ZHAO_SET_ENVIRONMENT_OFF_SUN_YAW;
  localparam int unsigned OFF_EN_PITCH = ZHAO_SET_ENVIRONMENT_OFF_SUN_PITCH;
  localparam int unsigned OFF_EN_SUN   = ZHAO_SET_ENVIRONMENT_OFF_SUN_COLOUR;
  localparam int unsigned OFF_EN_AMB   = ZHAO_SET_ENVIRONMENT_OFF_AMBIENT;
  localparam int unsigned OFF_EN_TMS   = ZHAO_SET_ENVIRONMENT_OFF_TERRAIN_MATERIAL_SET;
  localparam int unsigned OFF_EN_TMI   = ZHAO_SET_ENVIRONMENT_OFF_TERRAIN_MATERIAL_ID;
  initial begin
    if ((OFF_EN_AMB + 2) >= ZHAO_SET_ENVIRONMENT_BYTES)
      $fatal(1, "zhao_cmd_exec: SetEnvironment's ambient reaches the record's last byte; the capture races rec_done");
    // THE SAME GUARD, FOR THE SAME REASON, ON THE TWO FIELDS ADDED 2026-09-26.
    // `en_dirty` is raised by `rec_done`, which lands on the record's LAST
    // byte, so a field whose last byte IS that byte would be captured on the
    // same edge the shadow is declared complete. These two end at byte 42 of
    // 48 and the guard pins it rather than leaving a reader to re-derive it.
    if ((OFF_EN_TMI + 2) >= ZHAO_SET_ENVIRONMENT_BYTES)
      $fatal(1, "zhao_cmd_exec: SetEnvironment's terrain_material_id reaches the record's last byte; the capture races rec_done");
    if ((OFF_EN_TMS + 4) != OFF_EN_TMI)
      $fatal(1, "zhao_cmd_exec: SetEnvironment's terrain material pair is not contiguous; the ABI moved under this decoder");
  end

  logic [15:0] en_yaw, en_pitch, en_sun, en_amb;
  logic [31:0] en_tms;
  logic [15:0] en_tmi;
  logic        en_dirty;
  wire en_byte_c = (st == EX_STAGE) && take && in_rec_region
                && (r_op == ZHAO_OP_SET_ENVIRONMENT);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      en_yaw <= 16'd0; en_pitch <= 16'd0; en_sun <= 16'd0; en_amb <= 16'd0;
      en_tms <= 32'd0; en_tmi <= 16'd0;
      en_dirty <= 1'b0;
      env_valid_o <= 1'b0;
      env_sun_yaw_o <= 16'd0; env_sun_pitch_o <= 16'd0;
      env_sun_colour_o <= 16'd0; env_ambient_o <= 16'd0;
      env_terr_mat_set_o <= 32'd0; env_terr_mat_id_o <= 16'd0;
      envs_issued_o <= 32'd0;
    end else begin
      if (en_byte_c) begin
        if ((rpos >= 16'(OFF_EN_YAW))   && (rpos < 16'(OFF_EN_YAW + 2)))
          en_yaw   <= {pkt_byte_i, en_yaw[15:8]};
        if ((rpos >= 16'(OFF_EN_PITCH)) && (rpos < 16'(OFF_EN_PITCH + 2)))
          en_pitch <= {pkt_byte_i, en_pitch[15:8]};
        if ((rpos >= 16'(OFF_EN_SUN))   && (rpos < 16'(OFF_EN_SUN + 2)))
          en_sun   <= {pkt_byte_i, en_sun[15:8]};
        if ((rpos >= 16'(OFF_EN_AMB))   && (rpos < 16'(OFF_EN_AMB + 2)))
          en_amb   <= {pkt_byte_i, en_amb[15:8]};
        // Little-endian, LSB first, exactly as the four fields above: each new
        // byte enters at the top and the shadow shifts down.
        if ((rpos >= 16'(OFF_EN_TMS))   && (rpos < 16'(OFF_EN_TMS + 4)))
          en_tms   <= {pkt_byte_i, en_tms[31:8]};
        if ((rpos >= 16'(OFF_EN_TMI))   && (rpos < 16'(OFF_EN_TMI + 2)))
          en_tmi   <= {pkt_byte_i, en_tmi[15:8]};
        if (rec_done) en_dirty <= 1'b1;
      end

      if (env_valid_o && env_ready_i) begin
        env_valid_o <= 1'b0;
        `ZHAO_EXEC_INC(envs_issued_o);
      end

      if ((st == EX_STAGE) && verdict_valid_i) begin
        if ((verdict_error_i == ZH_ABI_OK) && !poisoned && en_dirty) begin
          // A newer environment replaces one the consumer has not taken yet:
          // the latest committed state is the state.
          env_valid_o      <= 1'b1;
          env_sun_yaw_o    <= en_yaw;
          env_sun_pitch_o  <= en_pitch;
          env_sun_colour_o <= en_sun;
          env_ambient_o    <= en_amb;
          // ONE RECORD, ONE ENABLE. These two are loaded by the same
          // assignment as the four above, so a consumer cannot be handed this
          // frame's terrain material beside the previous frame's sun -- the
          // metadata-swap shape CLAUDE.md's chapter is about, refused by
          // construction rather than by a counter watching for it.
          env_terr_mat_set_o <= en_tms;
          env_terr_mat_id_o  <= en_tmi;
        end
        en_dirty <= 1'b0;
      end
    end
  end

  // ==========================================================================
  // R41: SetPopulation 0x0303 -> zhao_part_pop. SELF-CONTAINED, for the same
  // reason the environment arm above is: it reads the packet walk and the
  // verdict and touches nothing the state machine owns, so every other arm of
  // CMD.EXEC is unchanged by it.
  //
  // THE SAME ATOMICITY AS SetView AND SetEnvironment: fields land in a shadow
  // during STAGE, the record's end marks it pending, and it leaves this block
  // only on a CLEAN verdict (ZH_ABI_OK and not poisoned). Several
  // SetPopulation records in one packet: the last one wins, which is the same
  // law as a per-frame global. NOTHING IS INTERPRETED HERE -- the flags, the
  // plane's range and the count's range are `zhao_part_pop`'s to judge, so the
  // executor cannot develop a second opinion about the engine's formats.
  //
  // The last field ends at byte 47 of a 48-byte record, which is the LAST
  // byte, so the capture of `pop_flags` and `rec_done` land on the SAME edge.
  // That is safe and it is not an accident: `pop_dirty` is set from `rec_done`
  // in this same block, and the shadow is read at the VERDICT, many cycles
  // later. The elaboration guard below pins the assumption that the record is
  // never shorter than the last field.
  // ==========================================================================
  localparam int unsigned OFF_PP_POP  = ZHAO_SET_POPULATION_OFF_POPULATION;
  localparam int unsigned OFF_PP_OX   = ZHAO_SET_POPULATION_OFF_ORIGIN_X;
  localparam int unsigned OFF_PP_OY   = ZHAO_SET_POPULATION_OFF_ORIGIN_Y;
  localparam int unsigned OFF_PP_OZ   = ZHAO_SET_POPULATION_OFF_ORIGIN_Z;
  localparam int unsigned OFF_PP_CNT  = ZHAO_SET_POPULATION_OFF_ACTIVE_COUNT;
  localparam int unsigned OFF_PP_PC   = ZHAO_SET_POPULATION_OFF_PLANE_C;
  localparam int unsigned OFF_PP_NX   = ZHAO_SET_POPULATION_OFF_PLANE_NX;
  localparam int unsigned OFF_PP_NY   = ZHAO_SET_POPULATION_OFF_PLANE_NY;
  localparam int unsigned OFF_PP_NZ   = ZHAO_SET_POPULATION_OFF_PLANE_NZ;
  localparam int unsigned OFF_PP_FLG  = ZHAO_SET_POPULATION_OFF_FLAGS;
  initial begin
    if ((OFF_PP_FLG + 2) > ZHAO_SET_POPULATION_BYTES)
      $fatal(1, "zhao_cmd_exec: SetPopulation's flags run past the record");
  end

  logic [31:0] pp_pop, pp_ox, pp_oy, pp_oz, pp_cnt, pp_pc;
  logic [15:0] pp_nx, pp_ny, pp_nz, pp_flg;
  logic        pp_dirty;
  wire pp_byte_c = (st == EX_STAGE) && take && in_rec_region
                && (r_op == ZHAO_OP_SET_POPULATION);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      pp_pop <= 32'd0; pp_ox <= 32'd0; pp_oy <= 32'd0; pp_oz <= 32'd0;
      pp_cnt <= 32'd0; pp_pc <= 32'd0;
      pp_nx <= 16'd0; pp_ny <= 16'd0; pp_nz <= 16'd0; pp_flg <= 16'd0;
      pp_dirty <= 1'b0;
      pop_valid_o <= 1'b0;
      pop_population_o <= 32'd0;
      pop_origin_x_o <= 32'd0; pop_origin_y_o <= 32'd0; pop_origin_z_o <= 32'd0;
      pop_active_count_o <= 32'd0; pop_plane_c_o <= 32'd0;
      pop_plane_nx_o <= 16'd0; pop_plane_ny_o <= 16'd0; pop_plane_nz_o <= 16'd0;
      pop_flags_o <= 16'd0;
      pops_issued_o <= 32'd0;
    end else begin
      if (pp_byte_c) begin
        // Little-endian on the wire, so every field shifts DOWN and the new
        // byte enters at the top -- the same assembly the environment arm uses.
        if ((rpos >= 16'(OFF_PP_POP)) && (rpos < 16'(OFF_PP_POP + 4)))
          pp_pop <= {pkt_byte_i, pp_pop[31:8]};
        if ((rpos >= 16'(OFF_PP_OX))  && (rpos < 16'(OFF_PP_OX + 4)))
          pp_ox  <= {pkt_byte_i, pp_ox[31:8]};
        if ((rpos >= 16'(OFF_PP_OY))  && (rpos < 16'(OFF_PP_OY + 4)))
          pp_oy  <= {pkt_byte_i, pp_oy[31:8]};
        if ((rpos >= 16'(OFF_PP_OZ))  && (rpos < 16'(OFF_PP_OZ + 4)))
          pp_oz  <= {pkt_byte_i, pp_oz[31:8]};
        if ((rpos >= 16'(OFF_PP_CNT)) && (rpos < 16'(OFF_PP_CNT + 4)))
          pp_cnt <= {pkt_byte_i, pp_cnt[31:8]};
        if ((rpos >= 16'(OFF_PP_PC))  && (rpos < 16'(OFF_PP_PC + 4)))
          pp_pc  <= {pkt_byte_i, pp_pc[31:8]};
        if ((rpos >= 16'(OFF_PP_NX))  && (rpos < 16'(OFF_PP_NX + 2)))
          pp_nx  <= {pkt_byte_i, pp_nx[15:8]};
        if ((rpos >= 16'(OFF_PP_NY))  && (rpos < 16'(OFF_PP_NY + 2)))
          pp_ny  <= {pkt_byte_i, pp_ny[15:8]};
        if ((rpos >= 16'(OFF_PP_NZ))  && (rpos < 16'(OFF_PP_NZ + 2)))
          pp_nz  <= {pkt_byte_i, pp_nz[15:8]};
        if ((rpos >= 16'(OFF_PP_FLG)) && (rpos < 16'(OFF_PP_FLG + 2)))
          pp_flg <= {pkt_byte_i, pp_flg[15:8]};
        if (rec_done) pp_dirty <= 1'b1;
      end

      if (pop_valid_o && pop_ready_i) begin
        pop_valid_o <= 1'b0;
        `ZHAO_EXEC_INC(pops_issued_o);
      end

      if ((st == EX_STAGE) && verdict_valid_i) begin
        if ((verdict_error_i == ZH_ABI_OK) && !poisoned && pp_dirty) begin
          pop_valid_o        <= 1'b1;
          pop_population_o   <= pp_pop;
          pop_origin_x_o     <= pp_ox;
          pop_origin_y_o     <= pp_oy;
          pop_origin_z_o     <= pp_oz;
          pop_active_count_o <= pp_cnt;
          pop_plane_c_o      <= pp_pc;
          pop_plane_nx_o     <= pp_nx;
          pop_plane_ny_o     <= pp_ny;
          pop_plane_nz_o     <= pp_nz;
          pop_flags_o        <= pp_flg;
        end
        pp_dirty <= 1'b0;
      end
    end
  end

  // ==========================================================================
  // TerrainField 0x0200 -- THE PRODUCER ENTRY I34 NAMES AS BUILD ITEM (a)
  // ==========================================================================
  // The port block above argues what this is and what it is not. What follows
  // is the mechanism, and three things about it are worth stating because the
  // next person will want to change each of them.
  //
  // IT IS NOT A SECOND VALIDATOR, and the temptation here is specific.
  // `spec/commands.zidl` says of the parameter blob: "bytes [0, 4*lane_count)
  // carry lanes, the rest MUST be zero". That is a wire law and it reads like
  // an invitation to check it here. It is not one. CMD.DECODER owns the
  // VERDICT and this block owns the PAYLOAD -- a zero-tail test here would be
  // a second implementation of a ratified rule, the exact failure this file's
  // header forbids twice. If nothing currently enforces that law, the repair
  // belongs in the decoder and the finding belongs in a report; it does not
  // belong in an arm that was only ever meant to lower bytes.
  //
  // ONLY p0..p7 ARE LIFTED, out of a 64-byte blob. field-ir.md 7.1 and the
  // command's own doc comment give Earth eight parameter lanes; the remaining
  // 32 bytes are the mandatory-zero tail. Lifting all sixteen possible lanes
  // would double this ring for bytes the ratified Earth signature has no
  // register for -- and would quietly invent a sixteen-lane contract.
  //
  // THE CAPTURE CANNOT COLLIDE WITH `rec_done`, and that is checked rather
  // than assumed. p7 ends at byte 76 of a 112-byte record, so every field is
  // registered many cycles before the record's last byte arrives and this arm
  // needs none of the DrawForm bypass. The elaboration guard below pins it,
  // so a layout move cannot make the collision appear silently.
  localparam int unsigned OFF_TF_SRC   = ZHAO_TERRAIN_FIELD_OFF_H_SOURCE_ID;
  localparam int unsigned OFF_TF_PROG  = ZHAO_TERRAIN_FIELD_OFF_PROGRAM;
  localparam int unsigned OFF_TF_FOOT  = ZHAO_TERRAIN_FIELD_OFF_FOOTPRINT;
  localparam int unsigned OFF_TF_START = ZHAO_TERRAIN_FIELD_OFF_START_TICK;
  localparam int unsigned OFF_TF_DUR   = ZHAO_TERRAIN_FIELD_OFF_DURATION_TICKS;
  localparam int unsigned OFF_TF_P0    = ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_0;
  localparam int unsigned TF_LANES     = 8;   // p0..p7, the Earth signature

  initial begin
    if (ZHAO_TERRAIN_FIELD_BYTES != 112)
      $fatal(1, "zhao_cmd_exec: TerrainField record size moved; re-read the offsets");
    // rectfx is x0,y0,x1,y1 and the footprint must still be four fx16 sitting
    // immediately before start_tick, or the four captures below shear.
    if ((OFF_TF_FOOT + 16) != OFF_TF_START)
      $fatal(1, "zhao_cmd_exec: TerrainField footprint is no longer four fx16 before start_tick");
    if ((OFF_TF_START + 4) != OFF_TF_DUR)
      $fatal(1, "zhao_cmd_exec: TerrainField start_tick/duration_ticks are no longer adjacent");
    // The collision guard the comment above promises.
    if ((OFF_TF_P0 + 4 * TF_LANES) >= ZHAO_TERRAIN_FIELD_BYTES)
      $fatal(1, "zhao_cmd_exec: TerrainField p0..p7 reach the record's last byte; add a bypass");
    if (TFLD_Q < 2)
      $fatal(1, "zhao_cmd_exec: TFLD_Q must be >= 2 (the pointers need a bit)");
  end

  localparam int unsigned TQ_X0_LO  = 0;
  localparam int unsigned TQ_Z0_LO  = 32;
  localparam int unsigned TQ_X1_LO  = 64;
  localparam int unsigned TQ_Z1_LO  = 96;
  localparam int unsigned TQ_HDL_LO = 128;
  localparam int unsigned TQ_CMD_LO = 160;
  localparam int unsigned TQ_ST_LO  = 176;
  localparam int unsigned TQ_DUR_LO = 208;
  localparam int unsigned TQ_P_LO   = 240;
  localparam int unsigned TQ_TRN_LO = 496;  // source_id did not fit 16 bits
  localparam int unsigned TFLD_W    = 497;
  localparam int unsigned TQW       = $clog2(TFLD_Q);

  logic [31:0] tf_x0, tf_z0, tf_x1, tf_z1, tf_hdl, tf_st, tf_dur;
  logic [31:0] tf_p [0:TF_LANES-1];
  logic [15:0] tf_cmd;
  logic        tf_src_hi_nz;   // the dropped half of source_id was not zero

  logic [TFLD_W-1:0] tq [0:TFLD_Q-1];
  // One bit per queue slot: "this record ends the set the verdict published".
  // Cleared on push, set on the publish that makes the record visible.
  logic [TFLD_Q-1:0] tq_last;
  // The slot the publish must mark: one behind the write pointer, modulo
  // the queue, so the subtraction and the truncation cannot disagree.
  // THREE pointers, not two. `tq_wp` is where staging writes, `tq_cp` is what
  // the VERDICT has published, and `tq_rp` is what the consumer has taken.
  // Nothing between `tq_cp` and `tq_wp` is visible downstream, which is how a
  // packet that is later abandoned leaves no record behind -- the same law
  // "not one console-visible bit moves before verdict_valid_i" that the
  // header argues for the whole block.
  logic [TQW:0] tq_wp, tq_rp, tq_cp;
  logic         tq_ovf;
  wire  [TQW:0] tq_occ  = tq_wp - tq_rp;
  wire          tq_full = (tq_occ >= (TQW+1)'(TFLD_Q));

  wire [TQW-1:0]    tq_wp_m1_c = tq_wp[TQW-1:0] - 1'b1;
  wire [TFLD_W-1:0] tq_head = tq[tq_rp[TQW-1:0]];
  assign tfld_valid_o     = (tq_rp != tq_cp);
  assign tfld_x0_o        = $signed(tq_head[TQ_X0_LO  +: 32]);
  assign tfld_z0_o        = $signed(tq_head[TQ_Z0_LO  +: 32]);
  assign tfld_x1_o        = $signed(tq_head[TQ_X1_LO  +: 32]);
  assign tfld_z1_o        = $signed(tq_head[TQ_Z1_LO  +: 32]);
  assign tfld_handle_o    = tq_head[TQ_HDL_LO +: 32];
  assign tfld_cmd_o       = tq_head[TQ_CMD_LO +: 16];
  assign tfld_start_tick_o= tq_head[TQ_ST_LO  +: 32];
  assign tfld_duration_o  = tq_head[TQ_DUR_LO +: 32];
  assign tfld_params_o    = tq_head[TQ_P_LO   +: 256];
  assign tfld_last_o      = tq_last[tq_rp[TQW-1:0]];

  wire tf_byte_c = (st == EX_STAGE) && take && in_rec_region
                && (r_op == ZHAO_OP_TERRAIN_FIELD);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      tf_x0 <= 32'd0; tf_z0 <= 32'd0; tf_x1 <= 32'd0; tf_z1 <= 32'd0;
      tf_hdl <= 32'd0; tf_st <= 32'd0; tf_dur <= 32'd0; tf_cmd <= 16'd0;
      tf_src_hi_nz <= 1'b0;
      for (int k = 0; k < TF_LANES; k++) tf_p[k] <= 32'd0;
      tq_wp <= '0; tq_rp <= '0; tq_cp <= '0;
      tq_last <= '0;
      tq_ovf <= 1'b0;
      tflds_issued_o <= 32'd0;
      tfld_overflow_o <= 32'd0;
      tfld_src_truncated_o <= 32'd0;
    end else begin
      if (tf_byte_c) begin
        // Little-endian on the wire, so every field shifts DOWN and the new
        // byte enters at the top -- the same assembly the population arm uses.
        if ((rpos >= 16'(OFF_TF_FOOT))      && (rpos < 16'(OFF_TF_FOOT + 4)))
          tf_x0  <= {pkt_byte_i, tf_x0[31:8]};
        if ((rpos >= 16'(OFF_TF_FOOT + 4))  && (rpos < 16'(OFF_TF_FOOT + 8)))
          tf_z0  <= {pkt_byte_i, tf_z0[31:8]};
        if ((rpos >= 16'(OFF_TF_FOOT + 8))  && (rpos < 16'(OFF_TF_FOOT + 12)))
          tf_x1  <= {pkt_byte_i, tf_x1[31:8]};
        if ((rpos >= 16'(OFF_TF_FOOT + 12)) && (rpos < 16'(OFF_TF_FOOT + 16)))
          tf_z1  <= {pkt_byte_i, tf_z1[31:8]};
        if ((rpos >= 16'(OFF_TF_PROG))      && (rpos < 16'(OFF_TF_PROG + 4)))
          tf_hdl <= {pkt_byte_i, tf_hdl[31:8]};
        if ((rpos >= 16'(OFF_TF_START))     && (rpos < 16'(OFF_TF_START + 4)))
          tf_st  <= {pkt_byte_i, tf_st[31:8]};
        if ((rpos >= 16'(OFF_TF_DUR))       && (rpos < 16'(OFF_TF_DUR + 4)))
          tf_dur <= {pkt_byte_i, tf_dur[31:8]};
        // source_id is u32 on the wire and the patch's `fld_add_cmd_i` is 16
        // bits. NARROWED, exactly as the stamp and draw arms narrow it, and
        // the dropped half is COUNTED rather than discarded quietly.
        if ((rpos >= 16'(OFF_TF_SRC))       && (rpos < 16'(OFF_TF_SRC + 2)))
          tf_cmd <= {pkt_byte_i, tf_cmd[15:8]};
        if ((rpos >= 16'(OFF_TF_SRC + 2))   && (rpos < 16'(OFF_TF_SRC + 4))
            && (pkt_byte_i != 8'd0))
          tf_src_hi_nz <= 1'b1;
        for (int k = 0; k < TF_LANES; k++) begin
          if ((rpos >= 16'(OFF_TF_P0 + 4*k)) && (rpos < 16'(OFF_TF_P0 + 4*k + 4)))
            tf_p[k] <= {pkt_byte_i, tf_p[k][31:8]};
        end

        // Every field is registered long before the record's last byte (the
        // elaboration guard pins that), so the push needs no bypass.
        if (rec_done) begin
          if (tq_full) begin
            tq_ovf <= 1'b1;
          end else begin
            tq[tq_wp[TQW-1:0]] <= {tf_src_hi_nz,
                                   tf_p[7], tf_p[6], tf_p[5], tf_p[4],
                                   tf_p[3], tf_p[2], tf_p[1], tf_p[0],
                                   tf_dur, tf_st, tf_cmd, tf_hdl,
                                   tf_z1, tf_x1, tf_z0, tf_x0};
            tq_last[tq_wp[TQW-1:0]] <= 1'b0;
            tq_wp <= tq_wp + 1'b1;
          end
          tf_src_hi_nz <= 1'b0;
        end
      end

      if (tfld_valid_o && tfld_ready_i) begin
        tq_rp <= tq_rp + 1'b1;
        `ZHAO_EXEC_INC(tflds_issued_o);
        // Counted on the way OUT, not on the way in, so an abandoned packet's
        // records never reach this counter. A truncation count that included
        // staged-then-discarded records would measure the wire and claim to
        // measure the console.
        if (tq_head[TQ_TRN_LO]) `ZHAO_EXEC_INC(tfld_src_truncated_o);
      end

      if ((st == EX_STAGE) && verdict_valid_i) begin
        // PUBLISH or ROLL BACK, whole. A packet carrying more TerrainFields
        // than TFLD_Q is refused entire -- never half-applied -- because a
        // section 3.4 sum missing one of its lanes is a plausible wrong
        // terrain, which is the failure this file exists to refuse.
        if ((verdict_error_i == ZH_ABI_OK) && !poisoned && !tq_ovf) begin
          tq_cp <= tq_wp;
          // Mark the set's last record. Written AFTER the push above, so a
          // record staged and published in the same cycle is marked rather
          // than cleared -- it is the set's last record, and the ordering of
          // these two statements is what says so.
          if (tq_wp != tq_cp) tq_last[tq_wp_m1_c] <= 1'b1;
        end else begin
          tq_wp <= tq_cp;
        end
        if (tq_ovf) `ZHAO_EXEC_INC(tfld_overflow_o);
        tq_ovf       <= 1'b0;
        tf_src_hi_nz <= 1'b0;
      end
    end
  end

  // ==========================================================================
  // DrawProcedural 0x0302 -- THE PRIMITIVE FORGE DISPATCH
  //
  // Built to the TerrainField arm's shape above rather than to a new one:
  // byte-accumulate into registers, push at `rec_done`, publish or roll back
  // WHOLE at the verdict. Three pointers, so not one console-visible bit moves
  // before `verdict_valid_i`.
  //
  // ONE ABI FIELD IS DELIBERATELY NOT DECODED, AND SAYING SO IS THE POINT.
  // `screen_error` (offset 48) is real and this arm does not carry it. The
  // FORGE_PROGRAM page AUTHORS `segments` and `sides`, `zhao_forge_prim`
  // refuses out-of-range values on its own port, and no screen-error LOD clamp
  // exists anywhere in the tree. Decoding the field into a register nothing
  // reads is the uncashed-cheque shape this repo has a committed detector for
  // (`tools/budget/uncashed_cheques.py`), so it is left undecoded and NAMED:
  // whoever builds the clamp adds four lines here and a port beside
  // `forge_kind_o`, and nothing else moves.
  //
  // `frame_tick` IS A DECLARED FIELD NOW, not a pad reinterpretation. R241
  // D-TICK-A put it in `pad[11]`'s first two bytes; owner completion ruling 2
  // (2026-09-22) required both pad allocations to be made explicit in
  // `spec/commands.zidl`, so `ZHAO_DRAW_PROCEDURAL_OFF_FRAME_TICK_0` is now a
  // generated offset and the bytes are THE SAME TWO (payload 37..38, record
  // 53..54). Nothing about the decode below moved; what moved is that a nonzero
  // frame_tick is no longer a mandatory-zero pad violation at validation.
  //
  // `material_id` IS THE SECOND HALF OF THE MATERIAL REFERENCE (record 56..57,
  // u16 little-endian) and `material_set` is the WHOLE 32-bit word at record
  // 20..23. This arm reads them as two independent fields and joins them in one
  // queue entry; it does not interpret either, and it must not: the (set, id)
  // key belongs to `zhao_material_window` and the residency law to the handle
  // law, and an executor with a second opinion about them is the fault the
  // ruling names -- "do not read the existing word both as the complete set
  // handle and as its low-16-bit material ID."
  // ==========================================================================
  localparam int unsigned OFF_FG_SRC   = ZHAO_DRAW_PROCEDURAL_OFF_H_SOURCE_ID;
  localparam int unsigned OFF_FG_PROG  = ZHAO_DRAW_PROCEDURAL_OFF_PROGRAM;
  localparam int unsigned OFF_FG_MAT   = ZHAO_DRAW_PROCEDURAL_OFF_MATERIAL_SET;
  localparam int unsigned OFF_FG_KIND  = ZHAO_DRAW_PROCEDURAL_OFF_KIND;
  localparam int unsigned OFF_FG_TICK  = ZHAO_DRAW_PROCEDURAL_OFF_FRAME_TICK_0;
  localparam int unsigned OFF_FG_MID   = ZHAO_DRAW_PROCEDURAL_OFF_MATERIAL_ID;

  // Quartus 17.0 requires an elaboration check inside `initial begin`; and
  // `--lint-only` does not run one, so this is not evidence a clean lint gives.
  // synthesis translate_off
  initial begin
    if (FORGE_Q < 2)
      $fatal(1, "zhao_cmd_exec: FORGE_Q must be >= 2 (the pointers need a bit)");
    // Both two-byte fields must lie INSIDE the record, or this arm is reading
    // bytes that belong to something else.
    if ((OFF_FG_TICK + 2) > ZHAO_DRAW_PROCEDURAL_BYTES)
      $fatal(1, "zhao_cmd_exec: frame_tick's two bytes run past the DrawProcedural record");
    if ((OFF_FG_MID + 2) > ZHAO_DRAW_PROCEDURAL_BYTES)
      $fatal(1, "zhao_cmd_exec: material_id's two bytes run past the DrawProcedural record");
    if (OFF_FG_KIND >= OFF_FG_TICK)
      $fatal(1, "zhao_cmd_exec: forge_kind and frame_tick have moved relative to each other");
    // THE TWO HALVES OF THE MATERIAL REFERENCE MUST NOT OVERLAP. This is the
    // ruling's own prohibition expressed as an elaboration check: if a future
    // layout ever put `material_id` inside the set handle's four bytes, the
    // word would be read twice -- as the complete handle AND as its low half --
    // which is exactly the reading the owner forbade.
    if ((OFF_FG_MID < (OFF_FG_MAT + 4)) && ((OFF_FG_MID + 2) > OFF_FG_MAT))
      $fatal(1, "zhao_cmd_exec: material_id overlaps material_set -- the forbidden double read");
  end
  // synthesis translate_on

  localparam int unsigned FQ_PROG_LO = 0;
  localparam int unsigned FQ_MAT_LO  = 32;
  localparam int unsigned FQ_KIND_LO = 64;
  localparam int unsigned FQ_TICK_LO = 72;
  localparam int unsigned FQ_SRC_LO  = 88;
  localparam int unsigned FQ_TRN_LO  = 104;   // source_id did not fit 16 bits
  // The record index rides the SAME ENTRY as the set handle. That is the whole
  // carriage argument in one line: a queue entry is written by one assignment
  // and read by one pointer, so the pair cannot skew under any backpressure.
  localparam int unsigned FQ_MID_LO  = 105;
  localparam int unsigned FORGE_W    = 121;
  localparam int unsigned FQW        = $clog2(FORGE_Q);

  logic [31:0] fg_prog, fg_mat;
  logic [ 7:0] fg_kind;
  logic [15:0] fg_tick, fg_src, fg_mid;
  logic        fg_src_hi_nz;

  logic [FORGE_W-1:0] fq [0:FORGE_Q-1];
  logic [FQW:0] fq_wp, fq_rp, fq_cp;
  logic         fq_ovf;
  wire  [FQW:0] fq_occ  = fq_wp - fq_rp;
  wire          fq_full = (fq_occ >= (FQW+1)'(FORGE_Q));

  wire [FORGE_W-1:0] fq_head = fq[fq_rp[FQW-1:0]];
  assign forge_valid_o      = (fq_rp != fq_cp);
  assign forge_program_o    = fq_head[FQ_PROG_LO +: 32];
  assign forge_material_o   = fq_head[FQ_MAT_LO  +: 32];
  assign forge_material_id_o= fq_head[FQ_MID_LO  +: 16];
  assign forge_kind_o       = fq_head[FQ_KIND_LO +:  8];
  assign forge_frame_tick_o = fq_head[FQ_TICK_LO +: 16];
  assign forge_src_id_o     = fq_head[FQ_SRC_LO  +: 16];

  wire fg_byte_c = (st == EX_STAGE) && take && in_rec_region
                && (r_op == ZHAO_OP_DRAW_PROCEDURAL);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      fg_prog <= 32'd0; fg_mat <= 32'd0; fg_mid <= 16'd0;
      fg_kind <= 8'd0;  fg_tick <= 16'd0; fg_src <= 16'd0;
      fg_src_hi_nz <= 1'b0;
      fq_wp <= '0; fq_rp <= '0; fq_cp <= '0;
      fq_ovf <= 1'b0;
      forges_issued_o <= 32'd0;
      forge_overflow_o <= 32'd0;
      forge_src_truncated_o <= 32'd0;
    end else begin
      if (fg_byte_c) begin
        // Little-endian on the wire: every field shifts DOWN and the new byte
        // enters at the top, the assembly every other arm here uses.
        if ((rpos >= 16'(OFF_FG_PROG)) && (rpos < 16'(OFF_FG_PROG + 4)))
          fg_prog <= {pkt_byte_i, fg_prog[31:8]};
        if ((rpos >= 16'(OFF_FG_MAT))  && (rpos < 16'(OFF_FG_MAT + 4)))
          fg_mat  <= {pkt_byte_i, fg_mat[31:8]};
        if (rpos == 16'(OFF_FG_KIND))
          fg_kind <= pkt_byte_i;
        if ((rpos >= 16'(OFF_FG_TICK)) && (rpos < 16'(OFF_FG_TICK + 2)))
          fg_tick <= {pkt_byte_i, fg_tick[15:8]};
        // The record index, little-endian like every other field on this walk.
        if ((rpos >= 16'(OFF_FG_MID))  && (rpos < 16'(OFF_FG_MID + 2)))
          fg_mid  <= {pkt_byte_i, fg_mid[15:8]};
        // source_id is u32 on the wire and every consumer's src id is 16 bits.
        // NARROWED like the stamp, draw and terrain-field arms, and the dropped
        // half COUNTED rather than discarded quietly.
        if ((rpos >= 16'(OFF_FG_SRC))     && (rpos < 16'(OFF_FG_SRC + 2)))
          fg_src <= {pkt_byte_i, fg_src[15:8]};
        if ((rpos >= 16'(OFF_FG_SRC + 2)) && (rpos < 16'(OFF_FG_SRC + 4))
            && (pkt_byte_i != 8'd0))
          fg_src_hi_nz <= 1'b1;

        if (rec_done) begin
          if (fq_full) begin
            fq_ovf <= 1'b1;
          end else begin
            // ONE ASSIGNMENT, so the set handle and the record index are the
            // SAME draw's by construction. This is the structural join the
            // ruling's carriage clause asks for -- "capture the pair on the
            // draw's own accepted handshake" -- and it is why no mismatch
            // detector is needed at this seam: there are not two cadences here
            // for one to differ from.
            fq[fq_wp[FQW-1:0]] <= {fg_mid, fg_src_hi_nz, fg_src, fg_tick,
                                   fg_kind, fg_mat, fg_prog};
            fq_wp <= fq_wp + 1'b1;
          end
          fg_src_hi_nz <= 1'b0;
        end
      end

      if (forge_valid_o && forge_ready_i) begin
        fq_rp <= fq_rp + 1'b1;
        `ZHAO_EXEC_INC(forges_issued_o);
        // Counted on the way OUT, so an abandoned packet's records never reach
        // this counter -- the TerrainField arm's reasoning, unchanged.
        if (fq_head[FQ_TRN_LO]) `ZHAO_EXEC_INC(forge_src_truncated_o);
      end

      if ((st == EX_STAGE) && verdict_valid_i) begin
        if ((verdict_error_i == ZH_ABI_OK) && !poisoned && !fq_ovf) begin
          fq_cp <= fq_wp;
        end else begin
          fq_wp <= fq_cp;
        end
        if (fq_ovf) `ZHAO_EXEC_INC(forge_overflow_o);
        fq_ovf       <= 1'b0;
        fg_src_hi_nz <= 1'b0;
      end
    end
  end

  // ==========================================================================
  // TWOD: SetPlane 0x0306 and DrawSprite 0x0307 (completion ruling 2026-09-22)
  // ==========================================================================
  // SELF-CONTAINED, for the reason the environment, population, terrain-field
  // and forge arms below/above are: it reads the packet walk and the verdict
  // and touches nothing the state machine owns, so every other arm of CMD.EXEC
  // is unchanged by it.
  //
  // THE STORAGE IS NOT HERE AND THE ATOMICITY STILL IS. `zhao_twod_cmd` holds
  // the frame's ring in M10K; this block gives it the packet boundary, and the
  // two pointers on the far side do exactly what `fq_wp`/`fq_cp` do on this
  // one. An abandoned packet's descriptors never reach a frame.
  // ==========================================================================
  localparam int unsigned OFF_TP_SLOT = ZHAO_SET_PLANE_OFF_SLOT;
  localparam int unsigned OFF_TP_ROLE = ZHAO_SET_PLANE_OFF_ROLE;
  localparam int unsigned OFF_TP_BLND = ZHAO_SET_PLANE_OFF_BLEND;
  localparam int unsigned OFF_TP_OPAC = ZHAO_SET_PLANE_OFF_OPACITY;
  localparam int unsigned OFF_TP_FMT  = ZHAO_SET_PLANE_OFF_FORMAT;
  localparam int unsigned OFF_TP_WRAP = ZHAO_SET_PLANE_OFF_WRAP;
  localparam int unsigned OFF_TP_VM   = ZHAO_SET_PLANE_OFF_VIEW_MASK;
  localparam int unsigned OFF_TP_PAL  = ZHAO_SET_PLANE_OFF_PALETTE_ID;
  localparam int unsigned OFF_TP_W    = ZHAO_SET_PLANE_OFF_WIDTH;
  localparam int unsigned OFF_TP_H    = ZHAO_SET_PLANE_OFF_HEIGHT;
  localparam int unsigned OFF_TP_FLG  = ZHAO_SET_PLANE_OFF_FLAGS;
  localparam int unsigned OFF_TP_BASE = ZHAO_SET_PLANE_OFF_BASE;
  localparam int unsigned OFF_TP_LSTR = ZHAO_SET_PLANE_OFF_LSTRIDE;
  localparam int unsigned OFF_TP_LHGT = ZHAO_SET_PLANE_OFF_LHEIGHT;
  localparam int unsigned OFF_TP_A    = ZHAO_SET_PLANE_OFF_A;
  localparam int unsigned OFF_TP_B    = ZHAO_SET_PLANE_OFF_B;
  localparam int unsigned OFF_TP_C    = ZHAO_SET_PLANE_OFF_C;
  localparam int unsigned OFF_TP_D    = ZHAO_SET_PLANE_OFF_D;
  localparam int unsigned OFF_TP_U0   = ZHAO_SET_PLANE_OFF_U0;
  localparam int unsigned OFF_TP_V0   = ZHAO_SET_PLANE_OFF_V0;
  localparam int unsigned OFF_TP_LS   = ZHAO_SET_PLANE_OFF_LINE_SCROLL;

  localparam int unsigned OFF_TS_X    = ZHAO_DRAW_SPRITE_OFF_X;
  localparam int unsigned OFF_TS_Y    = ZHAO_DRAW_SPRITE_OFF_Y;
  localparam int unsigned OFF_TS_W    = ZHAO_DRAW_SPRITE_OFF_W;
  localparam int unsigned OFF_TS_H    = ZHAO_DRAW_SPRITE_OFF_H;
  localparam int unsigned OFF_TS_BASE = ZHAO_DRAW_SPRITE_OFF_BASE;
  localparam int unsigned OFF_TS_LSTR = ZHAO_DRAW_SPRITE_OFF_LSTRIDE;
  localparam int unsigned OFF_TS_LHGT = ZHAO_DRAW_SPRITE_OFF_LHEIGHT;
  localparam int unsigned OFF_TS_FMT  = ZHAO_DRAW_SPRITE_OFF_FORMAT;
  localparam int unsigned OFF_TS_PAL  = ZHAO_DRAW_SPRITE_OFF_PALETTE_ID;
  localparam int unsigned OFF_TS_BLND = ZHAO_DRAW_SPRITE_OFF_BLEND;
  localparam int unsigned OFF_TS_VM   = ZHAO_DRAW_SPRITE_OFF_VIEW_MASK;
  localparam int unsigned OFF_TS_TINT = ZHAO_DRAW_SPRITE_OFF_TINT;
  localparam int unsigned OFF_TS_ORD  = ZHAO_DRAW_SPRITE_OFF_ORDER;
  localparam int unsigned OFF_TS_FLG  = ZHAO_DRAW_SPRITE_OFF_FLAGS;
  localparam int unsigned OFF_TS_SRC  = ZHAO_DRAW_SPRITE_OFF_SRC_ID;
  localparam int unsigned OFF_TS_U    = ZHAO_DRAW_SPRITE_OFF_U;
  localparam int unsigned OFF_TS_V    = ZHAO_DRAW_SPRITE_OFF_V;
  localparam int unsigned OFF_TS_A00  = ZHAO_DRAW_SPRITE_OFF_A00;
  localparam int unsigned OFF_TS_A01  = ZHAO_DRAW_SPRITE_OFF_A01;
  localparam int unsigned OFF_TS_A10  = ZHAO_DRAW_SPRITE_OFF_A10;
  localparam int unsigned OFF_TS_A11  = ZHAO_DRAW_SPRITE_OFF_A11;

  // Quartus 17.0 requires an elaboration check inside `initial begin`; and
  // `--lint-only` does not run one, so a clean lint is NOT evidence about any
  // of these. They pin the two facts the one-cycle-late presentation rests on.
  // synthesis translate_off
  initial begin
    if ((OFF_TP_LS + 4) != ZHAO_SET_PLANE_BYTES)
      $fatal(1, "zhao_cmd_exec: SetPlane's last field is no longer its last byte; re-check the late presentation");
    if ((OFF_TS_A11 + 4) != ZHAO_DRAW_SPRITE_BYTES)
      $fatal(1, "zhao_cmd_exec: DrawSprite's last field is no longer its last byte; re-check the late presentation");
  end
  // synthesis translate_on

  logic [ 7:0] tp_slot, tp_role, tp_blend, tp_opac, tp_fmt, tp_wrap, tp_vm, tp_pal;
  logic [15:0] tp_w, tp_h, tp_flags, tp_base;
  logic [ 7:0] tp_lstr, tp_lhgt;
  logic [31:0] tp_a, tp_b, tp_c, tp_d, tp_u0, tp_v0, tp_ls;
  logic        tp_pend;

  logic [15:0] ts_x, ts_y, ts_w, ts_h, ts_base, ts_tint, ts_src;
  logic [ 7:0] ts_lstr, ts_lhgt, ts_fmt, ts_pal, ts_blend, ts_vm, ts_ord, ts_flg;
  logic [31:0] ts_u, ts_v, ts_a00, ts_a01, ts_a10, ts_a11;
  logic        ts_pend;

  wire tp_byte_c = (st == EX_STAGE) && take && in_rec_region
                && (r_op == ZHAO_OP_SET_PLANE);
  wire ts_byte_c = (st == EX_STAGE) && take && in_rec_region
                && (r_op == ZHAO_OP_DRAW_SPRITE);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      tp_slot <= 8'd0; tp_role <= 8'd0; tp_blend <= 8'd0; tp_opac <= 8'd0;
      tp_fmt <= 8'd0; tp_wrap <= 8'd0; tp_vm <= 8'd0; tp_pal <= 8'd0;
      tp_w <= 16'd0; tp_h <= 16'd0; tp_flags <= 16'd0; tp_base <= 16'd0;
      tp_lstr <= 8'd0; tp_lhgt <= 8'd0;
      tp_a <= 32'd0; tp_b <= 32'd0; tp_c <= 32'd0; tp_d <= 32'd0;
      tp_u0 <= 32'd0; tp_v0 <= 32'd0; tp_ls <= 32'd0;
      tp_pend <= 1'b0;
      ts_x <= 16'd0; ts_y <= 16'd0; ts_w <= 16'd0; ts_h <= 16'd0;
      ts_base <= 16'd0; ts_tint <= 16'd0; ts_src <= 16'd0;
      ts_lstr <= 8'd0; ts_lhgt <= 8'd0; ts_fmt <= 8'd0; ts_pal <= 8'd0;
      ts_blend <= 8'd0; ts_vm <= 8'd0; ts_ord <= 8'd0; ts_flg <= 8'd0;
      ts_u <= 32'd0; ts_v <= 32'd0;
      ts_a00 <= 32'd0; ts_a01 <= 32'd0; ts_a10 <= 32'd0; ts_a11 <= 32'd0;
      ts_pend <= 1'b0;
      tpl_valid_o <= 1'b0; tsp_valid_o <= 1'b0;
      tpl_slot_o <= 8'd0; tpl_role_o <= 8'd0; tpl_blend_o <= 8'd0;
      tpl_opacity_o <= 8'd0; tpl_format_o <= 8'd0; tpl_wrap_o <= 8'd0;
      tpl_view_mask_o <= 8'd0; tpl_palette_o <= 8'd0;
      tpl_width_o <= 16'd0; tpl_height_o <= 16'd0; tpl_flags_o <= 16'd0;
      tpl_base_o <= 16'd0; tpl_lstride_o <= 8'd0; tpl_lheight_o <= 8'd0;
      tpl_a_o <= 32'd0; tpl_b_o <= 32'd0; tpl_c_o <= 32'd0; tpl_d_o <= 32'd0;
      tpl_u0_o <= 32'd0; tpl_v0_o <= 32'd0; tpl_line_scroll_o <= 32'd0;
      tsp_x_o <= 16'd0; tsp_y_o <= 16'd0; tsp_w_o <= 16'd0; tsp_h_o <= 16'd0;
      tsp_base_o <= 16'd0; tsp_lstride_o <= 8'd0; tsp_lheight_o <= 8'd0;
      tsp_format_o <= 8'd0; tsp_palette_o <= 8'd0; tsp_blend_o <= 8'd0;
      tsp_view_mask_o <= 8'd0; tsp_tint_o <= 16'd0; tsp_order_o <= 8'd0;
      tsp_flags_o <= 8'd0; tsp_src_id_o <= 16'd0;
      tsp_u_o <= 32'd0; tsp_v_o <= 32'd0;
      tsp_a00_o <= 32'd0; tsp_a01_o <= 32'd0; tsp_a10_o <= 32'd0; tsp_a11_o <= 32'd0;
      twod_pkt_commit_o <= 1'b0; twod_pkt_abandon_o <= 1'b0;
      twod_planes_staged_o <= 32'd0; twod_sprites_staged_o <= 32'd0;
      twod_dropped_o <= 32'd0;
    end else begin
      twod_pkt_commit_o  <= 1'b0;
      twod_pkt_abandon_o <= 1'b0;

      // ---- SetPlane's bytes -------------------------------------------------
      if (tp_byte_c) begin
        // Little-endian on the wire: every multi-byte field shifts DOWN and the
        // new byte enters at the top, the assembly every other arm here uses.
        if (rpos == 16'(OFF_TP_SLOT)) tp_slot  <= pkt_byte_i;
        if (rpos == 16'(OFF_TP_ROLE)) tp_role  <= pkt_byte_i;
        if (rpos == 16'(OFF_TP_BLND)) tp_blend <= pkt_byte_i;
        if (rpos == 16'(OFF_TP_OPAC)) tp_opac  <= pkt_byte_i;
        if (rpos == 16'(OFF_TP_FMT))  tp_fmt   <= pkt_byte_i;
        if (rpos == 16'(OFF_TP_WRAP)) tp_wrap  <= pkt_byte_i;
        if (rpos == 16'(OFF_TP_VM))   tp_vm    <= pkt_byte_i;
        if (rpos == 16'(OFF_TP_PAL))  tp_pal   <= pkt_byte_i;
        if (rpos == 16'(OFF_TP_LSTR)) tp_lstr  <= pkt_byte_i;
        if (rpos == 16'(OFF_TP_LHGT)) tp_lhgt  <= pkt_byte_i;
        if ((rpos >= 16'(OFF_TP_W))    && (rpos < 16'(OFF_TP_W + 2)))
          tp_w     <= {pkt_byte_i, tp_w[15:8]};
        if ((rpos >= 16'(OFF_TP_H))    && (rpos < 16'(OFF_TP_H + 2)))
          tp_h     <= {pkt_byte_i, tp_h[15:8]};
        if ((rpos >= 16'(OFF_TP_FLG))  && (rpos < 16'(OFF_TP_FLG + 2)))
          tp_flags <= {pkt_byte_i, tp_flags[15:8]};
        if ((rpos >= 16'(OFF_TP_BASE)) && (rpos < 16'(OFF_TP_BASE + 2)))
          tp_base  <= {pkt_byte_i, tp_base[15:8]};
        if ((rpos >= 16'(OFF_TP_A))    && (rpos < 16'(OFF_TP_A  + 4)))
          tp_a  <= {pkt_byte_i, tp_a[31:8]};
        if ((rpos >= 16'(OFF_TP_B))    && (rpos < 16'(OFF_TP_B  + 4)))
          tp_b  <= {pkt_byte_i, tp_b[31:8]};
        if ((rpos >= 16'(OFF_TP_C))    && (rpos < 16'(OFF_TP_C  + 4)))
          tp_c  <= {pkt_byte_i, tp_c[31:8]};
        if ((rpos >= 16'(OFF_TP_D))    && (rpos < 16'(OFF_TP_D  + 4)))
          tp_d  <= {pkt_byte_i, tp_d[31:8]};
        if ((rpos >= 16'(OFF_TP_U0))   && (rpos < 16'(OFF_TP_U0 + 4)))
          tp_u0 <= {pkt_byte_i, tp_u0[31:8]};
        if ((rpos >= 16'(OFF_TP_V0))   && (rpos < 16'(OFF_TP_V0 + 4)))
          tp_v0 <= {pkt_byte_i, tp_v0[31:8]};
        if ((rpos >= 16'(OFF_TP_LS))   && (rpos < 16'(OFF_TP_LS + 4)))
          tp_ls <= {pkt_byte_i, tp_ls[31:8]};
        if (rec_done) begin
          if (tp_pend) `ZHAO_EXEC_INC(twod_dropped_o);
          tp_pend <= 1'b1;
        end
      end

      // ---- DrawSprite's bytes ------------------------------------------------
      if (ts_byte_c) begin
        if (rpos == 16'(OFF_TS_LSTR)) ts_lstr  <= pkt_byte_i;
        if (rpos == 16'(OFF_TS_LHGT)) ts_lhgt  <= pkt_byte_i;
        if (rpos == 16'(OFF_TS_FMT))  ts_fmt   <= pkt_byte_i;
        if (rpos == 16'(OFF_TS_PAL))  ts_pal   <= pkt_byte_i;
        if (rpos == 16'(OFF_TS_BLND)) ts_blend <= pkt_byte_i;
        if (rpos == 16'(OFF_TS_VM))   ts_vm    <= pkt_byte_i;
        if (rpos == 16'(OFF_TS_ORD))  ts_ord   <= pkt_byte_i;
        if (rpos == 16'(OFF_TS_FLG))  ts_flg   <= pkt_byte_i;
        if ((rpos >= 16'(OFF_TS_X))    && (rpos < 16'(OFF_TS_X + 2)))
          ts_x    <= {pkt_byte_i, ts_x[15:8]};
        if ((rpos >= 16'(OFF_TS_Y))    && (rpos < 16'(OFF_TS_Y + 2)))
          ts_y    <= {pkt_byte_i, ts_y[15:8]};
        if ((rpos >= 16'(OFF_TS_W))    && (rpos < 16'(OFF_TS_W + 2)))
          ts_w    <= {pkt_byte_i, ts_w[15:8]};
        if ((rpos >= 16'(OFF_TS_H))    && (rpos < 16'(OFF_TS_H + 2)))
          ts_h    <= {pkt_byte_i, ts_h[15:8]};
        if ((rpos >= 16'(OFF_TS_BASE)) && (rpos < 16'(OFF_TS_BASE + 2)))
          ts_base <= {pkt_byte_i, ts_base[15:8]};
        if ((rpos >= 16'(OFF_TS_TINT)) && (rpos < 16'(OFF_TS_TINT + 2)))
          ts_tint <= {pkt_byte_i, ts_tint[15:8]};
        if ((rpos >= 16'(OFF_TS_SRC))  && (rpos < 16'(OFF_TS_SRC + 2)))
          ts_src  <= {pkt_byte_i, ts_src[15:8]};
        if ((rpos >= 16'(OFF_TS_U))    && (rpos < 16'(OFF_TS_U   + 4)))
          ts_u   <= {pkt_byte_i, ts_u[31:8]};
        if ((rpos >= 16'(OFF_TS_V))    && (rpos < 16'(OFF_TS_V   + 4)))
          ts_v   <= {pkt_byte_i, ts_v[31:8]};
        if ((rpos >= 16'(OFF_TS_A00))  && (rpos < 16'(OFF_TS_A00 + 4)))
          ts_a00 <= {pkt_byte_i, ts_a00[31:8]};
        if ((rpos >= 16'(OFF_TS_A01))  && (rpos < 16'(OFF_TS_A01 + 4)))
          ts_a01 <= {pkt_byte_i, ts_a01[31:8]};
        if ((rpos >= 16'(OFF_TS_A10))  && (rpos < 16'(OFF_TS_A10 + 4)))
          ts_a10 <= {pkt_byte_i, ts_a10[31:8]};
        if ((rpos >= 16'(OFF_TS_A11))  && (rpos < 16'(OFF_TS_A11 + 4)))
          ts_a11 <= {pkt_byte_i, ts_a11[31:8]};
        if (rec_done) begin
          if (ts_pend) `ZHAO_EXEC_INC(twod_dropped_o);
          ts_pend <= 1'b1;
        end
      end

      // ---- the offers, one cycle after the record's last byte ---------------
      if (tpl_valid_o && tpl_ready_i) tpl_valid_o <= 1'b0;
      if (tsp_valid_o && tsp_ready_i) tsp_valid_o <= 1'b0;

      if (tp_pend && (!tpl_valid_o || tpl_ready_i)) begin
        tpl_valid_o       <= 1'b1;
        tpl_slot_o        <= tp_slot;
        tpl_role_o        <= tp_role;
        tpl_blend_o       <= tp_blend;
        tpl_opacity_o     <= tp_opac;
        tpl_format_o      <= tp_fmt;
        tpl_wrap_o        <= tp_wrap;
        tpl_view_mask_o   <= tp_vm;
        tpl_palette_o     <= tp_pal;
        tpl_width_o       <= tp_w;
        tpl_height_o      <= tp_h;
        tpl_flags_o       <= tp_flags;
        tpl_base_o        <= tp_base;
        tpl_lstride_o     <= tp_lstr;
        tpl_lheight_o     <= tp_lhgt;
        tpl_a_o           <= signed'(tp_a);
        tpl_b_o           <= signed'(tp_b);
        tpl_c_o           <= signed'(tp_c);
        tpl_d_o           <= signed'(tp_d);
        tpl_u0_o          <= signed'(tp_u0);
        tpl_v0_o          <= signed'(tp_v0);
        tpl_line_scroll_o <= signed'(tp_ls);
        tp_pend           <= 1'b0;
        `ZHAO_EXEC_INC(twod_planes_staged_o);
      end

      if (ts_pend && (!tsp_valid_o || tsp_ready_i)) begin
        tsp_valid_o     <= 1'b1;
        tsp_x_o         <= signed'(ts_x);
        tsp_y_o         <= signed'(ts_y);
        tsp_w_o         <= ts_w;
        tsp_h_o         <= ts_h;
        tsp_base_o      <= ts_base;
        tsp_lstride_o   <= ts_lstr;
        tsp_lheight_o   <= ts_lhgt;
        tsp_format_o    <= ts_fmt;
        tsp_palette_o   <= ts_pal;
        tsp_blend_o     <= ts_blend;
        tsp_view_mask_o <= ts_vm;
        tsp_tint_o      <= ts_tint;
        tsp_order_o     <= ts_ord;
        tsp_flags_o     <= ts_flg;
        tsp_src_id_o    <= ts_src;
        tsp_u_o         <= signed'(ts_u);
        tsp_v_o         <= signed'(ts_v);
        tsp_a00_o       <= signed'(ts_a00);
        tsp_a01_o       <= signed'(ts_a01);
        tsp_a10_o       <= signed'(ts_a10);
        tsp_a11_o       <= signed'(ts_a11);
        ts_pend         <= 1'b0;
        `ZHAO_EXEC_INC(twod_sprites_staged_o);
      end

      // ---- the packet's verdict, forwarded --------------------------------
      // Exactly one pulse per packet this block walked, whether or not it
      // carried a TWOD record: the consumer's commit pointer must follow every
      // packet, or a later abandon would rewind past a frame that has already
      // been sealed.
      if ((st == EX_STAGE) && verdict_valid_i) begin
        if ((verdict_error_i == ZH_ABI_OK) && !poisoned) twod_pkt_commit_o  <= 1'b1;
        else                                             twod_pkt_abandon_o <= 1'b1;
        // A record whose last byte landed in the same cycle as the verdict has
        // not been offered yet; it is offered on the next cycle, AFTER the
        // verdict pulse, so `zhao_twod_cmd` would commit it into the wrong
        // frame. It cannot happen -- `rec_done` is the record's last byte and
        // `verdict_valid_i` is `decode_done_o`, one cycle after the PACKET's
        // last byte, so there is always at least the packet's four trailing
        // bytes between them -- and the pending flags are cleared here so a
        // malformed length that broke that spacing drops the record instead of
        // mis-filing it.
        if (tp_pend || ts_pend) `ZHAO_EXEC_INC(twod_dropped_o);
        tp_pend <= 1'b0;
        ts_pend <= 1'b0;
      end
    end
  end

  `undef ZHAO_EXEC_INC

endmodule : zhao_cmd_exec
