# Owner rulings, 2026-09-19 evening

Answers to `reports/OWNER-DOCKET-20260919.md` and handover §8/§12, given by
Fabian in session RUN-20260919-1656-gaps-to-zero. Items marked **(owner, explicit)**
were chosen by the owner directly. The owner then said *"go with your recommended
answers for now and don't stop to quiz me"*, so items marked **(provisional,
coordinator's recommendation)** stand until the owner revises them. Each is
cheap to reverse and is cited where it is used.

| # | question | ruling |
|---|---|---|
| R1 | PART.COLLIDE terrain normal (I6) — spec §4.4 finite differences vs TERRAIN.NORMALS.md:194 face normals | **Face normal (owner, explicit).** The collision normal is `normalize3_approx(face_normal(...))` of the triangle `spec/terrain_rules.md` §4.3 already picks for the height. `spec/terrain_rules.md` §4.4 is to be amended to say so for collision. `zhao_terrain_heighttap` grows the normal; I6 closes. |
| R2 | GEOM.LIGHT's owner (docket §5) | **`zhao_light_stream` owns vertex light (owner, explicit).** `zhao_geom_light` is superseded; `sky_and_beams.md` §4a is amended to match. Record the supersession in `design/console_inventory.yml` and the ledger. |
| R3 | Third projector port for particles / FORGE.SHADOW (I24, docket §2) | **Keep the time-multiplex (owner, explicit).** No third port in v1. Owed: a written schedule proof that geometry, particles and FORGE.SHADOW's instance-centre 1/w share client A's bandwidth within the frame at the guaranteed content tier. |
| R4 | Third client on `zhao_hps_arbiter` (I26/I27, MEM.UPLOAD) | **Widen to N clients (owner, explicit)**, preserving and re-proving the existing starvation law. Then compose MEM.UPLOAD and the terrain clients. |
| R5 | TERRAIN.NORMALS/SHADE third ModeTri pass (docket §7) | **Change the schedule (owner, explicit).** Keep lit terrain normals. Restructure the sequencer schedule so the third pass fits, with the frame deadline proven. |
| R6 | Island handle width, T10 50 bits vs RTL 32 (docket §6) | **Widen the RTL to 50 bits (owner, explicit).** T10 stands as written. |
| R7 | INPUT.SNAC, GEOM.WARP, POST.ECHO (docket §9–11) | **Build all three (owner, explicit).** They stay mandatory; the 2026-09-18 revocation stands. |
| R8 | TERRAIN.LOD deviation law (I21/I44, docket §3) | **The packet renders both readings side by side and the owner picks by eye (owner, explicit).** Until that happens (provisional, coordinator's recommendation) implement the reading that keeps T8's nested decimation bit-identical on shared vertices. Keep the choice in ONE named, editable constant/selector so the owner's pick is a one-line change, and produce the comparison render. |
| R9 | TEXTURE.TMU / `texture_samples` owner (docket §8) | **(provisional, coordinator's recommendation)** Retire TEXTURE.TMU as a single module in favour of the v3 path, and give `texture_samples` ONE owner: the v3 block that retires a filtered sample to the fragment. It counts samples it actually delivered. The ledger names that block, and the register resolves the capability to it. |
| R10 | Depth-profile port (docket §1), directory key (docket §4) | Already landed (`ac4f293d`, `db46a73e`). |
| R11 | Per-vertex u/v/rgb/alpha transport to GEOM.REPLAY (geom lane) | **(provisional, coordinator's recommendation)** A vertex-attribute store keyed like the arena, M10K-backed, written at the same moment and by the same producers that write the arena position (VDECODE/SKIN for u/v; `zhao_light_stream` per R2 for rgb/alpha). The palette/skin/group payloads are NOT widened. Replay reads it with the same per-triangle lookups, so there is no timing join. |
| R12 | Smoke gate `raster pixels=1536` (geom lane) | **(coordinator)** The bench's injected triangle door is fake stimulus and is removed when GEOM.REPLAY composes. The new pixel expectation must be DERIVED FROM THE REFERENCE render of the fixture, never read off the RTL. Coverage must not become trivial, and `frames_admitted=1` / every-word-retired stay hard. |
| R13 | I21 terrain material: layer E is per CELL, job port per SUBPATCH | **(provisional, coordinator's recommendation)** Join PER TRIANGLE, by the triangle's cell. The per-cell layer-E value is read at tessellation, where the triangle's cell is known, and travels with the triangle. The job port is not widened to carry a subpatch-uniform value that is not true. |
| R14 | TERRAIN.WRITEBACK journal slot and ticket owner (I28) | **(provisional, coordinator's recommendation)** SW.STREAM owns both through a doorbell contract. The HPS writes {journal base, slot, ticket} into a CSR mailbox on the terrain HPS arbiter's client (R4), and hardware acknowledges by returning the ticket. The packet WRITES that contract (design/contracts/) and the matching SW.STREAM sentence, then builds the mailbox. |
| R15 | TERRAIN.BAKE's laws (I32) | **(provisional, coordinator's recommendation)** BAKE consumes SURFACE.STAMP's `stamp_results`, which already carry strength. strength→depth is an ART value and lives in a named, editable constant table (CLAUDE.md rule 6), not a derived law. The 64×64→33×33 resample follows T8's nested decimation: take every second sample plus the shared edge, so shared vertices stay bit-identical. No new dig command. Write both laws into `spec/terrain_rules.md` and give each a zref reference. |
| R16 | Hardware TERRAIN.VISIBLE vs ruling T5 (software visible set) | **(provisional, coordinator's recommendation)** If T5's software visible set is IMPLEMENTED AND TESTED in the tree, the hardware block is a duplicate provider. Record `superseded_by` T5 with the SW file and test cited; removing duplicate providers is an allowed Phase-2 act. If the software side does NOT exist, T5 is an uncashed cheque and the hardware block is composed. Search before deciding, and name what was searched. || R17 | How an upload request reaches MEM.UPLOAD | **(provisional, coordinator's recommendation)** Add a new ratified command, `PublishResource`, carrying `{handle32, hps_addr, vram_dst, len, crc32c, dst_slot, new_gen, kind}`, lowered by CMD.EXEC onto MEM.UPLOAD's request port. It is an ABI addition, so `spec/commands.zidl`, the compiler's emitter, zref and captures all move together. |
| R18 | Token budget: SetView per-view tokens vs SetPresentationContract; percentages vs counts | **(provisional, coordinator's recommendation)** ONE unit: COUNTS at the hardware boundary. The compiler writing percentages is a compiler bug; fix the emitter and do not add a converter in RTL. ONE authority per level: SetPresentationContract sets the CEILING per view (plus `shared_tokens`), and SetView's tokens are that frame's REQUEST, clamped to the ceiling by MEASURE.TOKENS. The governor adjusts within the ceiling and never above it. |
| R19 | Counter catalog after R9 | **(provisional, coordinator's recommendation)** Yes: give each emitter its own catalog id in `spec/counters.md` (e.g. `texture_samples` → v3own only; `mosaic_picks` → TEXTURE.MOSAIC; `sprite_texels` → TWOD.SPRITE). No counter is shared across emitters. |
| R20 | MATERIAL.RESOLVE denied fetch | **(coordinator)** Engineering, not a choice: add an error/denied input on its memory port, and a matching "fetch denied" status in `zref::material::Resolver`. A denied fetch resolves to a defined fault response and is counted. It never hangs. || R21 | R5 revisited: the third ModeTri pass cannot fit (1.87M clocks > 1.67M-clock frame); no sun-direction producer | **(provisional, coordinator, on terrain2's evidence)** R5's GOAL stands (lit terrain normals) and its MEANS changes: compute terrain face normals at the REPLAY stage from the vertex store, with no third tessellation pass. The sun direction gets its producer by ratifying `SetEnvironment` 0x0311 (ABI addition: zidl, emitter, zref, captures move together). SHADE's throughput must be proven against the replay-rate triangle stream. |
| R22 | R8: which LOD deviation reading | **(provisional, coordinator's recommendation; OWNER TO CONFIRM BY EYE from `reports/terrain-lod-readings/lod_readings_contact.png`)** The MESH reading (`DEV_INCLUDE_BOUNDARY` as terrain2 recommends). The morph reading coarsens the corner nearest the camera and flattens ridges. |
| R23 | TERRAIN.ISLAND after R16 | **(provisional, coordinator)** Superseded by T5, like VISIBLE and under the same guarded `superseded_by` (ruling + existing files + built test). R6's 50-bit widening then applies wherever the handle is actually carried in RTL; if nothing carries it, R6 is moot and says so in the ledger. |
| R24 | When LOD deviations are computed | **(provisional, coordinator)** At PAGE LOAD, stored alongside the page in M10K (the owner prefers M10K over ALMs; deviations change only when the page changes or is baked). BAKE re-triggers the recompute for the pages it dirties. || R25 | GEOM.LIGHT descriptor bank (I48), and R21's sun direction | **(provisional, coordinator)** Promote `SetEnvironment` 0x0311 from reserved to IMPLEMENTED. The bank's Q16.16 / u20-gain values are THE LAW; §4a's u8 formula becomes a derived view with a zref bridge. CMD.EXEC lowers SetEnvironment into the bank, and its sun direction also feeds R21's terrain shading. |
| R26 | GEOM.LOD's inputs (the FORGE.SHADOW rung) | **(provisional, coordinator)** Lift the kind-8 freeze for GEOM.LOD's FOUR constants only (bound radius, micro/splat/glint error). Their layout is frozen now and the packer emits them. MEASURE.GOVERNOR additionally emits a per-camera `thresh_q8`. The centre 1/w stream rides client A per the R3 proof. |
| R27 | I12 arena origin | **(provisional, coordinator)** No arena-origin producer is owed in v1: the projector consumes WORLD positions, so nothing reads the origin. Remove the dead port pair and record the ruling. It carries no function, so removing it removes none. |
| R28 | I39/I24 raster_state bits | **(provisional, coordinator)** Ratify a `raster_state u32` layout in the ABI. Cull mode comes FIRST and from the DRAW (DrawForm flags); the remaining bits come from the material set. The packet writes the layout into `spec/commands.zidl` / the PARAMBUF contract with a zref model. |
| R29 | I36 draw job fields | **(provisional, coordinator)** Ratify handle32→desc_addr, format and xform[12] from the existing cartridge/asset page specs. The packet PROPOSES the layout from what those specs already imply, citing each field, and freezes it. |
| R30 | I14 viewport rectangle | **(provisional, coordinator)** Move the viewport-id→rectangle table from `video_rules.md` into the ABI as a fixed table per `video_mode`. SetView's viewport_id indexes it. |
| R31 | D7: replay 2.7× over frame, SKIN.NORM fork 2× over frame | **(coordinator)** The tier is NOT restated; that would narrow function. A RATE packet brings both under the frame with a measured margin (streaming DEPTHQUANT, a decoupled SKIN.NORM, and replay parallelism or pipelining as measurement permits), proved in clocks. The VDECODE-refusal deadlock that starves GROUP_SEQ is a DEFECT: fix it, and add a counter that fires on it. || R32 | MEM.UPLOAD can land only in TERRAIN.PAGE_POOL; RENDER.ASSET_POOL is read-only to all, so an uploaded MATERIAL_SET cannot reach ENGINE1 readers | **(provisional, coordinator)** Give MEM.UPLOAD's guard client a WRITE arm on RENDER.ASSET_POOL, bounded to the resource region the directory publishes, and RE-PROVE `mem_guard_no_escape` with the new arm (a committed mutant that widens the bound must make the proof fail). Capacity comes from the pool's existing budget. The contract's capacity note is updated to cite this ruling. |
| R33 | R18 revisited: no ratified per-frame token capacity exists, so percentages cannot become counts without an invented denominator | **(provisional, coordinator)** Remove percentages from the budget surface entirely: authors and the compiler write token COUNTS, and nothing converts. SetPresentationContract's counts ARE the ceiling (R18); no capacity constant is invented. If a capacity figure is later ratified, it becomes a compiler-side lint, never a hardware input. |
| R34 | POST shares ENGINE0 under the render lease (read arm plus capture write arm) | **(provisional, coordinator)** Confirmed. No new client id; T3's unspent client 5 stays unspent. |
| R35 | POST.ECHO capture window and arming | **(provisional, coordinator)** The window is `0x05C0_0000` (bank 2's reserved tail) as built. Capture is ARMED, not always-on: always-on spends 184 KB of writes per Z60 frame on a feature most frames do not use. The arm travels in R36's `SetPost`. |
| R36 | I17 (a)(b): a carrier for look values and the grading-table load | **(provisional, coordinator)** Ratify a `SetPost` command carrying bloom gain/bias, flash, ink, grade-valid and the echo arm (R35), plus a grading-table load, lowered by CMD.EXEC. ABI addition: zidl, emitter, zref and captures move together. |
| R37 | I17 (c): the tag-to-gather rule (GLOW channel to glow RGB / displacement / ink) | **(provisional, coordinator)** This is an ART law. The packet proposes it from `stars_and_flares.md` §1 with every coefficient in a named, editable constant (CLAUDE.md rule 6) and a zref model, and renders a before/after for the OWNER TO JUDGE BY EYE. The HUD plane store is then built against it. |
| R38 | POST throughput: the lease is busy 730,312 gpu cycles per pass | **(coordinator)** Not accepted as is. Add a measured second outstanding read (and more if measurement says so) and PROVE post fits inside the frame alongside raster and replay (R31), in clocks, with the margin stated. || R39 | The PROTECTED V1 shell `zhao_shell_top.sv` changed hash (00fdd238 → b2782424) because R32 added region inputs to `zhao_mem_guard`, and V1's four guard instances must tie them off to elaborate | **(provisional, coordinator; FLAGGED TO THE OWNER, since the pin file reserves this set to the owner)** RE-PIN, on the 2026-09-18 pin-refresh precedent: the diff is 12 lines of explicit TIE-offs (`res_valid=0`), and V1 behaviour is unchanged. Every site that pins that hash is updated TOGETHER (packet_c/d/e/g, the raster_texture_v3_fit_top test, the shell_fit_receipt_v3 fixture, packet_h_sibling_diff), each with the TIE diff and this ruling recorded beside the pin. The already-stale `zhao_prod_top.sv` pin in packet_c/e is re-pinned the same way, with its own reason. Default port values are NOT used (Quartus 17.0 support unproven). |
| R40 | I5: mapping the FIELD flow-lane output to PART.UPDATE's s11 accelerations | **(provisional, coordinator; particle MOTION is art, so the owner judges the result by eye)** acceleration = `sat_s11((v' - v) >> 8)`, seed = the variation byte, dt = 1 tick, and the host widened to 13 inputs / 7 outputs. The shift and the saturation live in named, editable constants. |
| R41 | I7: particle population origin and collision plane | **(provisional, coordinator)** Ratify `SetPopulation` (origin, plane, active_count), lowered by CMD.EXEC. active_count also seeds PART.STATE's first generation, replacing I1's provisional HPS seed. ABI addition: zidl, emitter, zref and captures move together. |
| R42 | I33: PART.TABLE's per-frame species load | **(provisional, coordinator)** A SPECIES_TABLE page kind, published through PublishResource/MEM.UPLOAD (R17/R32), plus a loader into PART.TABLE. The byte layout is frozen by the packet with a zref model. |
| R43 | I42: FIELD's program loader | **(provisional, coordinator)** A doorbell contract on the R14 pattern: SW.STREAM stages the plan and writes the EXISTING loader words, and CMD.EXEC does only the handle → program-hash lookup that TerrainField needs. |
| R44 | I34: TERRAIN.PATCH's field-height lane | **(provisional, coordinator)** Follows R43. Promote `zhao_probe_walk_earth`/`zhao_probe_patch_acc` out of `fpga/rtl/synth/` to production names and homes, as FIELD v3 was, rather than rebuilding them. The frame-tick uniforms come from CMD.EXEC's tick. |
| R45 | I30: stamp patch handle → terrain directory key | **(provisional, coordinator)** The stamp's patch is resolved by the SAME world→patch law `zhao_terrain_heighttap` implements (powers-of-two pitch, no divider); the directory is keyed by the resulting patch coordinates. No second mapping law. blend_en=0 is the ratified policy. |
| R46 | I1 provisionals: the HPS seeds the first generation; the bridge tag borrows ENGINE1 because the client enum is full | **(provisional, coordinator)** Keep the ENGINE1 tag (T3 keeps id 5 unspent); the first-generation seed moves to R41's active_count when SetPopulation lands. || R47 | R31 follow-up: each side fits the frame alone (vertices 72%, replay 36%), but the meshlet loop is SERIAL (single-bank ASSETFETCH; the dispatcher needs REPLAY idle), so the sum is ~119% | **(coordinator)** The tier is not restated. Build a two-bank ASSETFETCH with meshlet overlap and per-batch double-buffering in VATTR (M10K), and prove the overlapped loop under the frame in clocks, with the margin stated. |
| R48 | Vertex alpha has no producer | **(provisional, coordinator)** Keep `ALPHA_C` = 1.0 (opaque) as a NAMED, EDITABLE constant: no ratified vertex format carries alpha, so nothing is being stubbed. When a format gains alpha, it replaces the constant at the same seam. **AMENDED BY R89, 2026-09-20 (geomseam): R48 STANDS UNCHANGED and the contradiction it had with `design/contracts/FORGE.SHADOW.md` is closed in that document's new section "Where the transparency comes from".** The contract said "ordinary TRANSPARENT geometry" while R48 fixed alpha opaque, and each was cited on its own; the resolution is that `zhao_forge_shadow.sv:295` latches `vtx_alpha_o = strength_q` PER CASTER, so shadow alpha is a FLAT PER-PRIMITIVE value and never wanted the per-vertex slot R48 governs. Its producer is `tri_continuation_tail_i`'s existing `vertex_alpha` into the composed `zhao_raster_blend`, not a fourth attrpack plane and a fourth rasteriser lane. Interpolated per-vertex alpha remains a real, uncommissioned feature and `ALPHA_C` remains its named seam. |
| R49 | SetEnvironment's tint and fog: where they apply | **(provisional, coordinator; ART, so the owner judges by eye)** Tint multiplies the LIT vertex colour at `zhao_light_stream`'s output. Fog is proposed by a packet with a zref model and a before/after render (post stage by depth, or vertex stage by view distance, whichever the existing data supports without a new buffer). Its coefficients are named constants. |
| R50 | R25's compiler emitter was skipped | **(coordinator)** Accepted: Nanquan is provisional (memory: hardware first). The zidl, zref, captures and CMD.EXEC are the ABI's real consumers and all moved. || R51 | No HPS→FPGA register path exists for host readout (I19 histogram window, I45 trace readout) | **(provisional, coordinator)** Ratify a HOST REGISTER WINDOW on the HPS lightweight bridge (a read/write CSR aperture with a frozen address map in `spec/memory_rules.md`). I19's histogram window and I45's readout are its first two tenants. It is guarded like every other client (a region per tenant, no escape). |
| R52 | I45: no command arms DEBUG.TRACE | **(provisional, coordinator)** Ratify `DebugTraceArm` in the reserved 0xF0xx debug range, lowered by CMD.EXEC. ABI addition: zidl, zref and captures move together. |
| R53 | R29 has no owner (draw-job layout: desc_addr, format, xform[12]), and I36, I41, I39, I24 all wait on it | **(coordinator)** Assigned to GEOM pass 3, first in its lane. |
| R54 | `zhao_part_hps` ignores a bridge refusal (`err`) while holding its request; only its header argues this is unreachable | **(coordinator)** Same class as the S1 defects fixed in MEM.UPLOAD and FRAMEBLIT: handle `err` in B_REQ and test it the REAL way (a refusal with no grant). An argument is not a guard. || R55 | The arbiter's pending slot (R4/rule 6b) DROPS a second, different request from a client whose slot is still occupied, and nothing counts it | **(coordinator)** Add a counter that fires on that drop, and a directed case for it; a silent drop is the broken-instrument shape. Also state, per client, whether it may change a request while one is pending, and show STRUCTURALLY (not by timing) that each holder de-asserts before its burst ends, so a held request cannot be served twice. Next packet that owns memory takes it, with R54. |
| R56 | R15's resample has no referent: SURFACE.STAMP's texel centres sit at (2i+1)/128, so layer F is a 64x64 AREA grid a quarter cell off every vertex, with no sample at the far edge | **(provisional, coordinator; the OWNER may prefer to keep the page format frozen)** Take terrain3's option 3: make layer F 65x65 and VERTEX-ALIGNED (+258 B/page, +1.2%). It is exact and crack-free, and a dig that cracks at every patch seam is a visible fault, which is the more expensive kind. The page format moves with its spec, zref and packer in one pass. If the format must stay frozen, fall back to nearest-texel and DECLARE the measured seam error. |
| R57 | R47's MEANS was wrong: a two-bank ASSETFETCH buys nothing measurable; four "only when idle" gates serialise the meshlet loop (305 clk, a 60-clk dead gap, zero overlap, ~115% of a frame) | **(coordinator)** The GOAL stands and the means changes: pipeline REPLAY / GROUP_SEQ / ASSEMBLE across meshlets (banking is necessary, not sufficient), prove the overlapped loop in clocks against POST's measured share (R38 leaves ~969k clk armed), and expect the NEXT lever to be the per-vertex rate, not the loop. |
| R58 | I50 (new): GEOM.LOOM's node stream and camera basis are the ARM's by the 2026-08-31 ruling 6.4, so the gap is a CARRIER | **(provisional, coordinator)** The R14/R43 doorbell pattern: SW.STREAM stages the sorted stream, a CSR mailbox names it, hardware acknowledges. No second transform law. |
| R59 | TERRAIN.LOD's per-page deviation store prices at ~786 kbit = ~77 M10K of 553 (14%) | **(provisional, coordinator)** Acceptable in principle -- M10K is the currency we have and ALMs are the binding constraint -- but not unpriced: the packet must first show the EXACT same law in a smaller form (fewer levels stored, or deviations recomputed per visible patch into a small working set) and take the smaller one if it is bit-identical. Report both sizes. || R60 | The static gate set CANNOT see a directed test that does not COMPILE: it is all Python plus Verilator lint, and a merge broke `cmd_exec_directed`'s braces while every gate stayed green | **(coordinator)** The merge checklist gains "the directed tests BUILD and RUN", and the coordinator builds and runs at least `cmd_exec_directed` plus each lane's own directed test on every merged tree. A test that does not compile is not evidence, and it is silent. |
| R61 | A lone CR inside a line is a latent syntax error; one survives in `fpga/rtl/command/zhao_cmd_exec.sv` (inside a `//` comment, so Verilator tolerates it until a tool splits on CR) | **(coordinator)** Add a CR scan to `check_quartus17_syntax.py` (~5 lines), repair the one hit, and keep the scan in the gate list. Related: `ReadAllLines`/`WriteAllLines` PROMOTES such a CR into a line break, which is how one became a syntax error during a merge. |
| R62 | `cmd_exec_directed`'s two lanes both numbered their cases 17, 18 and 19, so `"case17:"` labels 35 checks across three blocks | **(coordinator)** Renumber the post suite's cases in its next pass, and put the check-label prefix in one place so a collision cannot recur silently. || R63 | D1: TERRAIN.LOD wants the two eyes in world units; `SetView` carries NO camera position, and the bank holds only the FUSED view-projection (an eye would be an unratified 4x4 inverse). Blocks I21 and R13 | **(provisional, coordinator)** Add `fx16 eye[3]` to `SetView 0x0010`, lowered by CMD.EXEC into the matrix bank's free cfg addresses 18+ -- the same one-field-one-arm pattern as R17/R25/R36/R41/R52. ABI addition: zidl, emitter, zref and captures move together. No inverse, and no new boundary traded for the old one. |
| R64 | D2: I44's coarse-height planes have NO consumer anywhere (searched `fpga/rtl` incl. `synth/`, `reference/`, contracts, `blocks.yml`, `spec/`, `tests/`, `tools/`), and T8's decimation is nested and unrounded, so `mip17[i,j] == fine33[2i,2j]` bit for bit | **(provisional, coordinator)** RETIRE the planes and both mip pools as a DUPLICATE PROVIDER (a Phase-2 allowed act), citing the bit-identity proof as a committed test, not as prose. MIPGEN stays composed for RESIDENCY's second completion. If anyone names a real consumer, this reverts in one ledger line. |
| R65 | D3: my R56 was WRONG about the price. The vertex-aligned layer F is +38.3%, not +1.2%: 8,450 B trips three elaboration guards and `zhao_terrain_jdoorbell.sv:143` needs a POWER OF TWO (next is 16,384) | **(provisional, coordinator)** The authorised fallback stands as terrain4 took it -- nearest-texel with the seam error MEASURED and DECLARED by a committed test (+1/4 cell interior, -1/4 at vertex 32, 1/2 cell across every patch seam; 0.25 m / 0.50 m at 1 m pitch). **OWED before this is final: a render of a dig across a patch seam at final resolution for the OWNER'S EYE** -- a half-cell step at every seam is an art defect, and only looking settles whether it reads. If it reads, the format moves (the packer does not exist yet, so it is cheaper now than ever). || R66 | R57 measured like-for-like: GEOM.ASSETFETCH is the real wall. Meshlet period 555 clk; the fetch costs 77.20 clk per 64-byte line = **515%** of the render share, against vertices at 126% and replay at 62%. The split is 11.8% request contention / 32.7% verdict+SDRAM wait / 55.5% beat phase (5.36 clk/beat, against 1.0 ideal) | **(coordinator)** The order is geom4's and it is measurement-led: (1) re-measure with the TERRAIN spine QUIESCED, because the 5.36 clk/beat was taken against 1,002 concurrent terrain bursts and a shared controller; (2) size outstanding reads from THAT number; (3) a second ASSETFETCH bank (555 → ~420 expected); (4) only then the vertex rate (a record is four sequential 64-bit RAM reads, so ≤7.0 clk/vertex means a 256-bit read over four banks -- an M10K-for-ALM trade, which is the currency we have). R57's expectation that the per-vertex rate was next is RETIRED: the fetch is 4x larger. |
| R67 | R30/I14 is a FIXTURE and MODE decision, not wiring: the core gives the host port per-cycle priority, so a lowered viewport rect overwrites the smoke's 32x64 sub-canvas mid-frame; real rects need Duo (two distinct viewport ids) or both Z60 views land in the same tiles, which destroys the gate's ability to see the second view | **(provisional, coordinator)** Move the smoke fixture to DUO for the viewport case, so the two views have distinct ids and distinct tiles, and regenerate the reference-derived pixel count in the SAME commit, stating both numbers. I14 still also needs R26's `pixel_error` and a `proj_en_i` producer, so R30 alone does not close it. |
| R68 | R26 / GEOM.LOD is FOUR sub-builds, not a composition: a frozen kind-8 layout for the four constants (zero hits anywhere in `fpga/rtl` outside the block's own ports), a packer that emits it, a per-instance LodState owner in GEOM.MESHFETCH (which has none -- the fifteenth false-absence claim stands), and the instance-centre 1/w through client A | **(coordinator)** Schedule it as its own packet with those four items named, rather than as a line in a geometry pass. The governor's `thresh_q8` half is small and has no consumer without the rest, so it goes in the same packet. |
| R69 | R58 / I50 (GEOM.LOOM's carrier) is not blocked, only unbuilt: `zhao_part_hps` is the exact pattern and the arbiter is already N-client | **(coordinator)** It needs a block, a frozen record layout, a 5th client and a bench change -- a packet's worth, not a tail item. geom4 was right to stop rather than leave it half-built. || R70 | D4 (hostdbg): I18 / MEASURE.HISTOGRAM's event has NEVER BEEN RATIFIED, and the entry's stated cause is WRONG -- `ev_err_i` is one unsigned magnitude per lane, not a difference against a reference (that is DEBUG.TRACE's port, and I18 is the entry that records a refusal reasoning from the wrong port, now doing it itself). A real producer of the right shape EXISTS, is built and is tested: `zhao_terrain_loddev`'s three 24-bit deviation magnitudes, observing TERRAIN.MIPFEED, which is already composed and needs no camera | **(provisional, coordinator)** Take hostdbg's recommendation A: **ratify the terrain page-load LOD deviation as the v1 histogram metric**, written into `spec/` with the interval's `src_id` recording which source an interval came from, and leave the charter's screen-space pixel error per camera as the v2 refinement the governor will want. Reasons: the ARM's Version-1 job is to predict a refinement threshold from prior counters, and a page deviation IS a candidate error bucket for that decision; B needs a projector-side residual nothing computes, which is GEOM.LOD-class work (R68) and not a wiring job; and the alternative is an organ that stays empty through v1 while its producer sits built and tested on disk. **It is the TERRAIN lane's work, not the debug lane's** -- their stream, their block. Also CORRECT I18's stated reason in `zhao_console_core.sv`, because "no producer exists" will send the next person to build a second one. **Before quoting a traverse, check that the smoke's stimulus drives MIPFEED's fine stream at all** -- hostdbg explicitly did not verify that, and its three pages fault on CRC. |
| R71 | Three merges in this run have SWALLOWED A CLOSING CONSTRUCT and the static gate set saw none of them: `cmd_exec_directed`'s brace (R60), `spt_entry`'s `end`/`endfunction`, and a whole superseded `tbl_load` task dragged back in from an older branch. Every one was found by a ten-minute smoke or a build, not by a gate | **(coordinator)** `run_console_core_smoke.ps1` gains a `-LintOnly` switch that stops after the verilate step, and it joins the merge checklist ahead of the full smoke. The verilate step is about a minute; the sim is ten. A cheap gate that runs FIRST is worth more than an expensive one that runs eventually. |
| R72 | `run_console_core_smoke.ps1` piped every translation unit's compiler output to `Out-Null`, so a failed unit printed `COMPILE FAILED: <path>` and nothing else. Two runs on 2026-09-20 (`-BadTraceArm`, then the FIELD merge's plain run) failed undiagnosably with the reason already discarded | **(coordinator)** Keep the output and print the last 40 lines on failure. This is CLAUDE.md's own Build-note law -- *"never send a build's output to `Out-Null`"* -- broken inside the script that gates every merge, which is the half-fixed shape this file keeps finding: the lesson was written down where it was learned and nowhere else. **A failure report that names the file and withholds the error is worse than a crash, because it looks like information.** And when I hit the first of the two, I compounded it by filtering the run's stdout to three patterns and throwing the message away a second time -- so the tool and the operator made the same mistake in the same hour. |
| R73 | D-A (geomlod): the camera PROJECTION SCALE has no producer anywhere, and it is the last non-engineering blocker on MEASURE.GOVERNOR | **(provisional, coordinator)** DERIVE it, do not add an ABI field: `proj_Q8.8 = rhu(kx_raw * viewport_w / 512)` is exactly the NDC-to-pixel factor `zref::creature::projected_bound_radius_q8` already uses, so it introduces NO NEW LAW and nothing has to be ratified. This is the opposite case to R63, which put the eye on the wire precisely because recovering it would have meant an unratified 4x4 inverse -- here the quantity is already defined by an existing function and the wire would be a second statement of it. Cheaper, and one fewer thing a capture can disagree with. |
| R74 | D-B (geomlod): creature ladder state per INSTANCE or per (INSTANCE, CAMERA)? zref holds one per instance; PART.LADDER's 2026-08-31 section 2.5 rules PARTICLES per camera; nothing ratifies creatures either way | **(provisional, coordinator)** PER (INSTANCE, CAMERA). One shared ladder lets player 1's camera coarsen player 2's creature, which is a Duo FAIRNESS defect under charter section 9 and would be reported as a graphics bug by whoever loses the trade. It costs one index bit and one line now, and a capture has not pinned it yet -- this is the cheapest it will ever be. The packet followed the reference model and NAMED the choice rather than burying it, which is why it is decidable at all. |
| R75 | D-C (geomlod): FORGE.SHADOW's caster is now real and its blocker MOVED to its output -- `vtx_*` is a world-vertex hull and GEOM.SETUP takes screen triangles, with nothing in `fpga/rtl` (incl. `synth/` and the probes) consuming a world-vertex fan | **(provisional, coordinator)** Take the ARENA ROUTE (at most 16 vertices against `GEOM_DEPTH=1089`) rather than building a second geometry path. A second path is a second implementation of the ratified projection arithmetic, which is the duplication `uncashed_cheques.py` check 3 exists to catch and which this repo has already shipped twice. AND the packet was RIGHT NOT TO COMPOSE `lodstate` alone: it would dangle the caster at the core boundary and put the register UP by one -- closing a gap by opening one is not progress, and refusing to do it is the behaviour the protocol asks for. |
| R76 | The `COMPILE FAILED` transient is REAL and it is NOT mine: the geomlod packet hit it independently on `-Mutant` (`...ConstPool__0__Slow.cpp`), reported that the same file **compiles clean by hand with the same flags**, and saw an identical re-run pass. I hit it twice the same morning on two different variants | **(coordinator)** My memory-pressure hypothesis is WEAKENED by their data point, not confirmed: a hand recompile succeeding with identical flags points at a transient in the parallel build, the filesystem or an on-access scanner rather than at resources or at the code. It stays an open question with two independent sightings, and NOBODY gets to call it understood until someone reads an actual error message -- which is now possible, because R72 stopped the script discarding them. Until then: re-run before believing a `COMPILE FAILED`, and run the smoke variants SEQUENTIALLY. |
| R77 | D1 (texmat2): `MaterialSample.modes[3:0]` tmu_mode has NO ratified encoding -- searched `spec/`, `reference/`, contracts, `tests/`, `tools/`, four hits and none of them an encoding. The packet chose `tmu_mode[1:0] == response_class` in one editable parameter, picked so `witness_mismatch_o` still differences two independent sources | **(coordinator, and it is DERIVED rather than chosen)** Ratify **0 CLUT, 1 NEAREST, 2 BILINEAR, 3 reserved**, because that is already the composed island's own numbering: `zhao_texture_island_v3_top.sv:222-224` declares `CLS_CLUT = 2'd0`, `CLS_NEAR = 2'd1`, `CLS_BIL = 2'd2` and has shipped with it. The packet's recommendation is not an invention, it is the constant that was already there. **AND `spec/commands.zidl` CONTRADICTS IT**: line 245 reads *"bits 0-3 tmu_mode (nearest/bilinear/CLUT/direct)"*, which reads exactly like an ordered enumeration and puts CLUT at 2. Anyone implementing from the ABI spec would have got CLUT wrong and the witness would have differenced two copies of the same mistake. Correct that comment in the same commit as the queued DebugTraceArm one -- this is the "real reason" that pays the capture regeneration. |
| R78 | D2 (texmat2): a CLUT material's PALETTE SLOT and GENERATION have no producer. Direct formats are legally zero by the binding row's own law; CLUT is unowned and is counted on `mat_win_clut_unowned_o` | **(provisional, coordinator)** The recommendation to put both in `MaterialSample` is probably right and it is NOT free: that struct is exactly four bytes (`u16 binding_slot`, `u8 binding_generation`, `u8 modes`) with no spare bit, so widening it moves `MaterialRecord`'s layout and every published asset with it. **Check the cheap options FIRST and report both sizes**, which is R59's rule applied here: (a) can the BINDING ROW own the palette, the way it already owns legality for direct formats -- one lookup, no ABI move; (b) `MaterialRecord.control` bits 5-7 and `flags` bits 3-15 are reserved and already in the record. Only if neither holds does the struct widen. A counted `clut_unowned` is an honest gap and is allowed to stay one for a packet longer. |
| R79 | D3 (texmat2): `fb_writer_i` -> `lease_writer_o` DECLINED on new evidence, and this REVERSES the recommendation the host/debug lane made and I relayed | **(coordinator, and my relay was wrong)** Accept the refusal. It is not a rename: `v2_lease_writer` is only meaningful while `v2_lease_valid` -- the shell's own consumer ANDs them -- so the naive substitution admits the blit writer whenever NO lease is live, which WIDENS a framebuffer safety window rather than tightening one. hostdbg judged it "more defensible than the entry admits" and declined on cost; texmat2 declined on correctness, which outranks it. The safe form is `fb_writer := v2_lease_valid && v2_lease_writer`, it belongs to the packet that gives the lease's validity an owner, and it owes a MUTANT because a widened safety window passes every test that a correct one passes. Note the shape for the docket: a substitution that "moves no register entry" was twice assessed on COST before anyone assessed whether it was RIGHT. |
| R80 | The completion fit needs its QUESTIONS named in advance (CLAUDE.md: "a fit nobody could state a question for is a fit that should not run"), and the first-light row shows the trap: `zhao_console_core@console-core-first-light` measured 47,582 ALM / 151 DSP / 306 M10K **on 5CEBA9F31C7, not the target**, with its own note saying the device was chosen ONLY to measure size. Against the real 5CSEBA6U23I7 that is ~113% of 41,910 ALM and 135% of 112 DSP | **(coordinator)** Plan the completion fit as **TWO runs with two different questions**, because at ~113% a fit on the target part is likely to REFUSE TO PLACE and return "does not fit" with no timing map -- the expensive outcome, four hours for one bit of information we can already predict. (1) **On the target 5CSEBA6U23I7: the VERDICT.** Does it fit, and by how much is it over. (2) **On the sizing device 5CEBA9F31C7: the MAP.** Per-hierarchy area, DSP by owner, and the timing picture -- which is the half that tells us what to fix. Run the map first if the verdict is a foregone conclusion. Three things must hold before either: a CLEAN TREE (the first-light row is `treeCleanAtHead: false`, so its digest describes nothing exactly), worst paths SPLIT BY ORIGIN (12 of 12 starting at virtual pins was nearly irrelevant once 1,595 were found starting inside), and `dsp_census.py`'s false "SPECIFIED BUT NOT BUILT" list repaired -- it excludes three blocks that exist, so the pre-fit expectation reads LOW. **And the number that will dominate the fixing is not area: `gpu_clk` came back at 18.5 MHz with -44.06 ns setup slack and -39,647 ns TNS.** |
| R81 | The `COMPILE FAILED` transient (R76) HAS A MECHANISM, and it is not the machine. The POST3 packet reported killing two `verilator_bin` PIDs **before** classifying them, and the survivor was running from the COORDINATOR's checkout with `+define+ZHAO_SMOKE_NO_ECHO_ARM` -- my `-NoEchoArm` smoke. A KILLED `g++` produces exactly R76's signature: no `.o`, **no error text whatever**, the same file compiles clean by hand with identical flags, and an identical re-run passes. Three sightings across two packets and me, all while other lanes were active | **(coordinator)** My memory-pressure hypothesis is RETIRED -- it blamed the machine when the cause was process discipline, which is the flattering direction for an operator. **The rule the fit runner already follows is the rule: never kill by process NAME or by START TIME; classify by command line and parent PID first, and only ever kill your own subtree.** `tools/quartus/run_block_fit.ps1:955-966` carries the incident that taught it -- 2026-09-06, two concurrent fits, the later *"died ten minutes in with no error in its log and no row written -- the signature of an external Stop-Process, not of a Quartus failure"*, **two 50-minute placements lost** -- and scopes its kills to its own parent PID exactly, in both directions. Written down where it was learned and nowhere else, which is this file's most repeated shape. It is now in PACKET-PROTOCOL.md. CONFIRMED in the direction that matters: the terrain6 merge's five forms failed `RC=-1/1/1` under POST3's kills and the identical tree passes clean afterwards. |
| R82 | **FOUR independent instances in one day of discarding the evidence at capture time**: `run_console_core_smoke.ps1` piping every `g++` to `Out-Null` (R72); me filtering `-BadTraceArm`'s stdout to three patterns and losing the message; me doing it AGAIN to a five-form run an hour after ruling on it; and the POST3 packet piping each form through `Select-Object -Last 14`, which scrolls off the very `raster pixels=`/`frames_admitted=` lines the gate list requires it to quote | **(coordinator)** It is not four mistakes, it is one habit, and prose has now lost to it twice in a single day -- so it is a RULE with a mechanism, not a caution. **Capture the FULL output to a file first, then filter the file for display.** `... 2>&1 \| Out-File -Encoding utf8 <log>` then read the log; never `\| Select-String` or `\| Select-Object` on the live stream, and never `\| Out-Null`. The asymmetry is the whole argument: filtering costs nothing when the run passes and costs the entire diagnosis when it fails -- and a failing run is exactly when nobody can go back and get it. A gate that reports a failure it cannot explain is worse than one that crashes, because it looks like information. |
| R83 | D-POST3-B: **R73 is right about the DERIVATION and wrong about the FORMAT, and the error is silent.** `proj = kx*vw/2` gives **443.41** at 60 deg / 512 px, and the port is **Q8.8, which caps at 255.996** -- so it saturates below 90 deg hfov single-view and below **53.13 deg on Duo**, which is an ordinary game camera. A saturated `proj` pegs the LOD ladder at its FINEST rung: maximum cost, no visible symptom, and nothing downstream can tell | **(coordinator)** Amend R73: the derivation stands, the container does not. Take **20-bit Q12.8** (G1's `STEPS` 33 to 37), NOT a unit rescale -- rescaling would move a quantity `zref::creature::projected_bound_radius_q8` already defines, which is the one thing R73 was chosen to avoid. **And note WHY this survived review, because it is the most instructive part:** the contract's single worked example is 221.70 px, **87% of full scale**. Every check anyone wrote passed, the one number in the document sat just inside the cliff, and the failure only appears at camera angles nobody had put in a test. A format validated by one example near its ceiling is not validated. |
| R84 | D-POST3-A: `zhao_part_ladder.sv:85-86` states *"coarser is numerically smaller"* and that is **BACKWARDS** against its own localparams and the `max()` at line 121. The arithmetic ships correctly; the PROSE would invert the policy for whoever implements against it -- and this is the SECOND instance of exactly this inversion in the LOD path | **(coordinator)** Correct the comment, and treat the repetition as the finding: two independent inversions in one subsystem's prose means the convention itself is not written down anywhere authoritative. State it once, in the contract, and have both files cite it. On the port itself: **recommend `size_shift_i`, not `deg -> floor`.** `deg` (0..3, "x2^deg allowed pixel error") is not a rung of a 0..5 SIZE-CHOSEN ladder, and `zhao_part_ladder.sv:108` already calls its own floor reading "AN INTERPRETATION" -- a word that should stop anyone building on it. The derived mapping scales the `*_MIN` thresholds by 2^deg, which the present port cannot express. |
| R85 | I17's stated cause is FALSE in its input half: it refuses on "the PRE-RESOLVE FRAGMENT stream", while `zhao_post_gather.sv:6,79` says **"RESOLVED fragments ... from RASTER.RESOLVE"** -- the stream it actually wants, carrying the shape field for field | **(coordinator)** Correct the entry. The real obstacle is **ONE 8-BIT TIE-OFF**: `zhao_shell_top_v2.sv:1181` discards `fb_tag_o` while `fb_valid`/`rgb565`/`x`/`y`/`last` all leave, and `fb_addr_o` is not even needed because in-tile position is the low 4 bits of `fb_x`/`fb_y` (tile origins verified 16-aligned). That is a materially different and much smaller job than the entry describes, and it is the fifth entry this run whose recorded cause had expired or named the wrong port. **The pattern is now strong enough to act on: a refusal's stated cause must be RE-CHECKED before it is inherited, every time, and this run has never once found that check wasted.** |
| R86 | **`zhao_prod_top` COMPOSES TWO SUPERSEDED MODULES AND NOTHING WAS LOOKING.** `completion_register.py`'s `superseded_in_closure()` only ever asked the CONSOLE CORE's closure; `zhao_prod_top` is a different root and no check asked it. Verified independently by me, not inherited: `zhao_prod_top.sv:3671` instantiates **`zhao_shell_top`** and `:4076` instantiates **`zhao_terrain_bake`**, and `fpga/quartus/prod_fit_sources.txt` contains the v1 files while **`zhao_shell_top_v2.sv` is absent from it entirely** -- the very shell every other lane in this run has been gating against (the paired-diff harness, R79's `fb_writer_i`, I17's `fb_tag_o` tie-off at `zhao_shell_top_v2.sv:1181`). Both are recorded in `console_inventory.yml` as `superseded`, **citing the owner's "only the latest version" ruling verbatim**, while `check_console_inventory` prints *"the latest version is the one wired"* -- true of what it measures, and read by everyone as a claim about the machine | **(coordinator)** This is the owner's one capitalised rule -- *"YOU ONLY GET TO FIT THE LATEST VERSION"* -- broken in the exact place it costs most: **the production fit top is what R80 plans to FIT**, so as it stands the completion fit would measure a machine nobody ships, and its area number would be wrong in the direction nobody questions. It also retroactively qualifies the whole-design map I quoted from `census-20260918` (297 DSP, 105,881 registers): that top contained v1 shell and no v2. **SEQUENCING, because this cannot just be flipped:** `zhao_prod_top.sv` is GENERATED and three lanes are gating on it right now, so TERRAIN7 was right to report rather than flip. Fix the manifest and regenerate **after the three running lanes land and BEFORE the freeze** -- it becomes the FOURTH item on R80's pre-fit checklist, and the largest. And extend the superseded check to EVERY root, not just the core: a blind spot the size of a top-level is not a gap in coverage, it is a second machine nobody audits. |
| R87 | **R59's own price was wrong by 2.4x.** A stale `// 576` comment beside a 704-bit row propagated into TERRAIN.LOD's whole R59 size report (116 M10K published, 141 real), and R59's premise itself used a 16-bit deviation where `DEVW` is 24 and counted no history at all. The true cost is **185 M10K of 553 (33%)**, not the ~77 (14%) the ruling priced | **(coordinator)** The correction stands and it changes the argument R59 rested on: I have been describing M10K as "the resource with real slack" -- against 306 of 553 already used, one block wanting 185 is not a rounding error, and the lookup-for-computation trade that was the obvious lever for the ALM overage is now itself budget-constrained. **Price M10K explicitly in the fit plan; it is no longer the free currency.** The new `localparam_comments` ctest catches the defect class, and note that it was BROKEN IN BOTH DIRECTIONS on first writing -- 13 false positives AND it missed the one true case, because a comma-terminated parameter never resolved. Both halves fixed and the positive control fired, which is the only reason its silence is worth anything. |
| R88 | **My R75 is right in principle and NOT REACHABLE, and the reason is a deadlock with no detector.** The FORGE lane found two blockers neither of us knew when R75 was written. (1) `zhao_geom_vattr.sv:490`'s `done_o` requires `lit_ord_q == uv_ord_q`; a shadow hull has no u/v and no lighting, and `va_done` gates BOTH sides of the GROUP_SEQ->REPLAY handshake (`zhao_console_core.sv:7354` and `:13722`) -- so an arena carrying a hull never releases and the front end stalls, **with no timeout and no counter**. The smoke's own `vattr landings=48 rows=48 colours=48 uv=24` is live proof of the two equalities. (2) **No vertex alpha exists**: the blend ALU is composed but `ALPHA_C` is hardwired opaque, attrpack emits three planes and the rasteriser has three lanes -- so a shadow composed today would be a flat OPAQUE polygon | **(coordinator)** R75's *conclusion* stands and is now load-bearing rather than assumed, because the lane also refuted the premise geomlod refused on: **a world-vertex consumer DOES exist and is composed** -- `zhao_geom_group_seq.v_x_i/v_y_i/v_z_i`, `signed [31:0]`. geomlod searched for `vtx_`-SHAPED names and missed it; the seam is structural, not conventional, and a name search cannot see it. Take the lane's **route B** (a private 16-deep arena with fan replay arbitrated into GEOM.CLIP), which dodges blocker (1) entirely. **AND INDEPENDENTLY OF FORGE.SHADOW: the vattr stall deserves a counter now.** A composed path that can wedge the whole front end with no timeout and no counter is the shape this file's own chapter is about -- nobody will be debugging shadows when it fires. |
| R89 | D-D (forge): **R48 and the FORGE.SHADOW contract contradict each other IN WRITING** about vertex alpha, and the contradiction currently gates the rest of R68 sub-build 4 | **(provisional, coordinator)** Take the **flat-alpha route**. `zhao_forge_shadow.sv:295` already makes alpha constant over the hull, so the honest change is to give `tri_continuation_tail_i`'s EXISTING per-triangle `vertex_alpha` a producer -- not to add a fourth attrpack plane AND a fourth rasteriser lane for a value that does not vary across the primitive. Two written laws disagreeing is an owner-visible defect regardless of which way it goes, so record the resolution in BOTH documents rather than in the RTL: a contract corrected in one place and not the other is how this pair got here. |
| R90 | D-E (forge): I29 needs a **second partial lift of the kind-8 freeze**, exactly R26's shape, and section 4c's `body_off` was designed to make that cheap | **(provisional, coordinator)** Take the partial lift rather than declaring I29 a Phase-12 item. The mechanism already exists and was built for this, and the alternative -- telling the register a mandatory entry is deferred -- needs a citation it does not have. **And note the SIXTEENTH false-absence claim that came with it:** I29 refused on *"no behavioural SDRAM model in this tree"*, which is refuted by the I23 record **~1,400 lines above it in the same file**. Sixteen now, and the rate has not fallen; treat every recorded cause as unverified until re-read, including the ones in entries you wrote yourself. **AMENDED 2026-09-20 (geomseam): THE LIFT IS RIGHT AND IT IS NOT THE WHOLE OBSTACLE, and the part it misses is priced in ALMs on a device already at ~113% of its ceiling.** Two findings, both searched rather than inherited. (a) **Nothing anywhere defines the BYTES.** `zref_creature.hpp` holds C++ structs with `std::vector` members (no wire layout), `spec/creature_rules.md:58-60` holds a size in prose, `zref_creature_page.hpp:23-28` explicitly disclaims freezing them, and `zref_creature.hpp:43-46` calls the quaternion lane format "PROPOSED, NOT FROZEN". So the lift does not UNBLOCK a layout; somebody must AUTHOR one, and that authoring is an ABI freeze. (b) **`zhao_geom_pose_decode.sv:89-92` makes the source fetch COMBINATIONAL BY CONTRACT** -- `bone_idx_o` is pure combinational (:146), the caller must answer in the same cycle and hold stable for every cycle the block sits in `S_FETCH`. That is 5 + 3x32 + 4x16 + 12x32 = ~550 bits per bone, so at 32 bones the producer is a **~17.6 kbit ASYNCHRONOUS-READ store** that a synchronous M10K cannot serve. The PAGE path is genuinely not the obstacle and composing a reader adds no core boundary port (`zhao_geom_ladderbank.sv:81` already carries `PAGE_KIND = 8'd8` and is uncomposed -- two hits in `zhao_console_core.sv`, both comments; `u_geom_mem_adapter`'s requesters A-E are full, so a sixth is an in-core edit). **RECOMMENDATION: grant the lift, and schedule I29 as its own packet with four named items -- author/freeze the body section and a minimal kind-9 frame with a zref model; emit a non-zero `body_off` from `tools/pack/mkcreatureladder.py`; compose the page reader as the sixth adapter requester; and PRICE THE ASYNC-READ PALETTE SOURCE IN ALMS BEFORE BUILDING IT.** If it is not affordable, the thing that has to move is the DECODER's combinational contract, which is a larger decision than the freeze and must not be discovered halfway through the packet. |
| R91 | I34's RECORDED blocker is the smaller of two, and the larger one is a THROUGHPUT wall of R66's class. The carriers lane measured it: the composed Earth front is **the arrangement `FIELD.SEQ.EARTH.md:105` excludes IN WRITING**. `E_ZERO` costs REGS clocks PER POINT plus `E_WRITE` IN_LANES+1, so **>=46 clocks/point** at REGS=32 / IN_LANES=13 against the condemned v2 front's 25. One lane over one patch is 1,089 x 46 = **50,094 clocks against a 10,416 allowance -- about 481%.** All four recorded build items can be built correctly and still miss by ~5x | **(provisional, coordinator)** Take option 3: give the front **the "same program, same uniforms" fast path the contract already asks for** (uniforms loaded once per association), 46 clocks to ~3, i.e. ~3,267 per association and inside the allowance -- **no new architecture, and it is the same lever I5 already needs**, so it is bought once and spent twice. Not option 1 (accept 481% for v1): this is the third throughput wall measured in this run after R57 and R66, and the pattern is that they do not get cheaper later. Not option 2 (swap TERRAIN.PATCH's architecture on R44's literal reading): the FIELD lane already showed R44's MEANS does not fit that seam. **Caveat the lane named and I am keeping attached to the ruling:** `E_ZERO` guards read-before-write, so the fast path needs `zfield::decode` to DECLARE the uniform set -- if that declaration cannot be made honestly, the lever does not exist and this returns to the owner. |
| R92 | `design/console_inventory.yml` is **CRLF while the tree is LF**, so line endings are not uniform even within `design/` -- and any `\n`-anchored script silently misses in some files while working in others | **(coordinator)** This is R61's family and it is the half R61 did not cover: R61's form-5 scan finds a lone CR INSIDE a line; it says nothing about a FILE whose endings differ from its neighbours'. I hit the same thing myself today -- a fix script needed `nl = "\r\n" if "\r\n" in text else "\n"` to match anything at all -- so it is confirmed from two directions in one session. **The failure is silent and selective**, which is worse than uniform breakage: a tool tested on an LF file passes review and then reads nothing from a CRLF one, and reports the zero as a result. Add a `.gitattributes` rule or normalise `design/`, and until then every script that anchors on `\n` across these files must derive its separator rather than assume it. |
| R93 | **`tests/mutants/zhao_terrain_bake_v2_mutant.sv` HAS NO DRIVER.** Grepped `tests/` for its module name: ONE hit, the file itself. Nothing in `tests/CMakeLists.txt` elaborates it, no directed test defines it in, no control asserts it fires. Its header is exemplary -- it names the fault class ("an off-by-one in WHICH row the prefetch lands: every counter still balances, every handshake still completes, and only the breach/heal DECISIONS move... exactly the class of corruption a detector wired to its own enable could never see") and the one substantive line it changes -- and none of that has ever been executed | **(coordinator)** This is CLAUDE.md's "BUILT, INSTALLED NOWHERE" applied to a positive control, and it is the worse variant: an uninstantiated MODULE costs the fitter nothing, but an **unrun MUTANT is cited as evidence**. It also costs maintenance forever -- it drifted on this very merge and I refreshed it, re-applying a mutation onto a body nothing will ever elaborate. **It gets a driver or it gets retired, and the next terrain packet decides which**; what it does not get is another refresh. The rule generalises: `mutant_copy_drift.py` proves a copy is CURRENT and says nothing about whether anything RUNS it, so the two checks are different questions and only one of them is being asked. Worth a second check that every file under `tests/mutants/` is named by something under `tests/`. |

## Open decisions raised 2026-09-20 by the GEOMSEAM lane

Neither is mine to settle; both are cheap to reverse and both are cited above.

**D-GEOMSEAM-A. R90 is under-priced, and the pricing may invert it.** See the
amendment on R90's row. The freeze is not the only obstacle to I29; a
~17.6 kbit ASYNCHRONOUS-READ bone store, forced by `zhao_geom_pose_decode`'s
combinational source contract, may be -- on a device already measured at ~113%
of its ALM ceiling. **Recommendation:** rule the lift as R90 says AND attach the
ALM pricing as a precondition of building, so the packet cannot discover it
halfway through. This is the same shape as `zhao_geom_pose_cache`'s own header
refusing to bury a 1.5 Mbit sizing choice inside a module.

**D-GEOMSEAM-B. A THIRD OWNER ON CLIENT A RE-AUTHORS A RATIFIED LAW, and it is
not a "+1 bit" widening.** Verified first-hand, not inherited:
`zhao_console_core.sv:3966` sets `GEOM_PAY_A_W = 16` and the elaboration guard
at `:6348-6350` proves the rider is `ARENA_W 3 + INDEX_W 12 = 15`;
`zhao_part_project.sv:356` sets `TAG_BIT = PAY_W - 1` and `:508`
`res_is_part_c = a_valid_i && a_payload_i[TAG_BIT]` is a **ONE-BIT TWO-OWNER**
discriminator. With a third owner that single bit stops being able to name the
owner at all: it becomes a two-bit owner field and `geom_tag_collision_o`'s
one-bit two-owner law must be RE-AUTHORED with it. Sizing this as "16 -> 17"
leaves a collision counter that silently stops meaning anything -- R88's own
shape, one block over. FORGE.SHADOW route B, R68's instance-centre 1/w and
R3's time-multiplex all want this widening. **Recommendation:** whichever
packet widens client A owns re-authoring that law AND re-firing the collision
counter in the same commit, with a committed mutant if legal stimulus cannot
reach the new collision state.
| R94 | `design/blocks.yml:5099` contains a FALSE PRESENCE and a FALSE ABSENCE **in the same ledger row**, and I verified both halves myself rather than inheriting them: FORGE.SHADOW declares `reference_model: zref::forge::shadow_hull`, which has **ZERO occurrences anywhere under `reference/`**, while its `tests:` rows say *"PLANNED -- NOT WRITTEN"* and `tests/forge/forge_shadow_directed.cpp` **exists and passes 39 checks** | **(coordinator)** Both directions in one row is the thing to notice. A declared reference model that does not exist is exactly what `uncashed_cheques.py` check 3 reads to find duplicated arithmetic -- so a NAME that resolves to nothing is silently excluded from a duplication check that is one of the few tools we have against a second implementation of ratified maths. And a test declared absent that exists is the sixteenth-false-absence pattern inside the LEDGER rather than inside a comment. **`blocks.yml` is one of the completion register's TWO ROOTS**, so its rows are not documentation: they are inputs to the number this campaign is steered by. Repair the row, and add a check that every `reference_model:` resolves to a symbol that exists and every `tests:` row's stated state matches the tree -- the same shape as the `unpriced_requirements:` repair, which found three rows naming modules that were on disk all along. The geomseam lane recorded it rather than patching it because `blocks.yml` is shared and three lanes were live; that was the right call and this is the follow-through. |
| R95 | **The driverless mutant's HEADER DESCRIBED THE WRONG FAULT**, and that is worse than it being unrun. `zhao_terrain_bake_v2_mutant`'s header says the window DUPLICATES a row; it actually **LAGS BY ONE from row 2 on**. The terrain8 lane built its first fixture from that sentence and it reported **ZERO disagreements against a genuinely broken mutant** | **(coordinator)** R93 said an unrun mutant is worse than an uninstantiated module because it gets cited as evidence. This is the sharper form: **an unrun control is not merely unproven, it is UNCORRECTED** -- nothing has ever pushed back on its prose, so the prose drifts from the mutation it describes and the next person builds a test that passes against a broken machine. I refreshed this very file this morning, preserving that header verbatim on the grounds that "a copy keeps its own", and the header was wrong the whole time. **A mutant is a claim about a fault, and the claim is checkable only by running it.** The lane chose a DRIVER over retirement (the fault is unreachable by legal stimulus) and paired it with `terrain_bake_v2_mutant_negative`, the same driver against PRODUCTION under `WILL_FAIL` -- which proves the driver DISCRIMINATES rather than merely runs. Every mutant on R93's debt list owes both halves. |
| R96 | **PACKET-PROTOCOL.md's own advice was aiming packets at the coordinator's checkout.** It said to read and write with `[IO.File]::ReadAllText/WriteAllText` and said nothing about paths; `[IO.File]` resolves a RELATIVE path against `[Environment]::CurrentDirectory`, which is the session's STARTUP directory and **never changes when you `cd`**. Two packets wrote into `zhaozhou-ceiling-lane-20260912` this way on 2026-09-20 | **(coordinator)** Fixed in the protocol, and the incident shape is the one to keep: **the tell is `git status` coming back CLEAN after a write you just watched succeed.** That is quiet, it looks like nothing happened, and it is the only local signal. Absolute paths always -- `Join-Path $PSScriptRoot` or a literal. And if you do land in someone else's tree, revert with `git apply --reverse`, **never `git checkout --`**: unstaged work has no reflog and BOTH times the coordinator had live uncommitted edits in the files touched. This is my defect, not the packets': I wrote the line, I repeated it in eleven briefs, and two lanes followed it correctly into the wrong tree. |
| R97 | terrain8 CORRECTED terrain7, which I had relayed verbatim into its own brief: **"the read side is SOLVED" is too strong.** `sc_*`'s absent consumer does NOT by itself move three entries, because **layer D has no reader anywhere** (offset 6,598: zero hits under `fpga/`) and bake needs it on TWO ports -- `vtx_nobake_i` and `cell_state_i`, the latter never recorded in any entry. The compcache seam the core names is an ON-CHIP MIRROR storing 2 substance bits and drops all six section 3.3 flag bits | **(coordinator)** The four unserved ports are ONE block, and the lane wrote its contract rather than guessing: `design/contracts/TERRAIN.PAGEIO.md`, deliberately with no `blocks.yml` row until it is built. It is also the only honest writer of I27's `terr_dm_*`. **Note what happened to the chain of custody here**: terrain7 verified a blocker, I relayed it as established, terrain8 re-checked it anyway and found it overstated. That is the fifth time this run that re-checking an inherited cause paid, and the first time the cause was one I had personally vouched for. Keep re-checking, including me. |
| R98 | **R83 is implemented NOWHERE, and `zhao_view_projq88` was built the same day in the Q8.8 container R83 rejects** -- with a saturation counter its own header calls "extreme" that actually fires at **53.13 deg hfov on Duo**. Widening must now land in TWO ports, not one | **(coordinator)** A ruling written in the morning and a block built in the afternoon of the same day, disagreeing, with neither aware of the other -- the coordination failure is MINE. R83 amended R73 in the rulings file and the MEASURE lane was already mid-build against the original. **Rulings that change a FORMAT must be routed to every live lane, not filed.** Fix both ports together. And the chain is longer than anyone has stated: I14's viewport rect -> `zhao_view_projscale` -> `zhao_view_projq88` -> R83 -> MEASURE.GOVERNOR -> TERRAIN.LOD, **five links**, so "cam*_scale_i now exists" is true and nearly useless on its own. |
| R99 | D-PRODTOP-A: `zhao_prod_top` wires `zhao_geom_project` AND `zhao_terrain_project`, superseded by `zhao_proj_subsystem` under owner ruling R3 -- pricing **~12,267 ALM and 66 DSP of projector the console does not contain** while NOT pricing the 9,135 ALUT / 39 DSP one it does. Two written laws collide: `prod_manifest.yml` defers adoption *"after the composed fit closes"*, R86 says the fit must not measure superseded modules | **(coordinator)** I checked which is actually true of the machine before choosing, and it decides itself: **`zhao_proj_subsystem` IS in the console core's closure and both v1 projectors are NOT.** The shipped machine is already correct; only the instrument that measures it is wrong. So the manifest's deferral is not protecting anything -- it is describing a state that no longer exists, and honouring it would mean the completion fit prices two projectors the console does not have and omits the one it does. **ADOPT BEFORE THE FIT.** One manifest edit, then regenerate. The deferral was written before R86 and before the composition; it does not survive either. |
| R100 | D-PRODTOP-B: `zhao_raster_attrdiv_svc` and `zhao_raster_attrstep` both wire `zhao_raster_attrdiv` while the console composes `zhao_raster_attrdiv_v2` -- two rounding laws for one quantity, and the now-fatal superseded check counts both as production roots | **(provisional, coordinator)** Same check, same answer: **`zhao_raster_attrdiv_v2` is in the core's closure and v1 is not.** These two roots are standalone fit targets, so they do not put a wrong module in the shipped machine -- what they do is **produce measurements that describe a rounding law the console does not use**, which is the quieter half of the same defect. Move both to v2. **If that is a BEHAVIOUR change rather than a swap, the difference IS the owner's tie-law decision and must be reported with NUMBERS** -- the same tie-law `zhao_raster_attrwalk` is already waiting on -- not assumed either way. What is NOT acceptable is reclassifying them alongside the ten retained pre-Packet-B oracles to make the gate green: that is making the instrument agree with the tree by editing the instrument. |
| R101 | **A LIVE, SHIPPING CORRECTNESS DEFECT IN THE SHARED FIELD HOST, found by the WARP lane: `cur_out_seen == '0'` tests whether ANY output was written, not whether ALL REQUIRED ones were.** A point that writes 5 of 6 declared lanes reports **SUCCESS**, with the sixth reading the zero that was cleared at grant. That violates W10 in RTL every profile shares -- not Warp's, not new, and not hypothetical | **(coordinator)** This is the most consequential find of the run and it has the signature this file keeps describing: **the author's comment states the exact hazard and then guards only the all-zero case**, and **the proof that it is live was already committed and GREEN** -- `field_host_directed` case 1 loads a program writing 1 of 4 declared lanes and asserts "status is OK". A passing test documenting the defect, for however long it has been there. The lane did NOT add a test asserting the bug, which is right: CLAUDE.md forbids it, because such a test passes only while the defect exists. **Repair before ANYTHING is measured on this host** -- a silent wrong value in a field result is worse than a refusal, and every profile inherits it. The header word has **64 free bits** for a required-output mask, and `mask == 0` preserves today's behaviour exactly, so the repair is additive. Assert the CORRECT behaviour (all declared lanes present, or a counted refusal) and keep the positive control separate. |
| R102 | **`zref::GeomWarp` was a PHANTOM REFERENCE** -- the ledger has named it as GEOM.WARP's `reference_model` since the block was specified and **no such symbol ever existed** (item 9 of `PHANTOM_REFERENCES.md`, so this class was already known and catalogued) | **(coordinator)** Second instance in two lanes, after R94's `zref::forge::shadow_hull`. That there is a FILE listing these means the class was known and nobody was gating on it -- the `.gitignore` shape again: written down, and nothing reads it back. **Make it a check**, as R94 already asks: every `reference_model:` in `blocks.yml` must resolve to a symbol that exists. Until then, `uncashed_cheques.py` check 3 -- one of the few defences against a second implementation of ratified arithmetic -- is silently blind to every block whose declared model is a phantom, which is exactly the set most likely to grow one. The lane built the real `zref::geom_warp` (102 directed checks over real `.zprog` fixtures through the real validator, three mutants fired first), so this one is discharged rather than merely reported. |
| R103 | **NINE of the directive's shared Field prerequisites are ABSENT and one is partial**, verified rather than assumed: P1 lanes (`IN_LANES=13` vs 15, in THREE places), P2 no free client port, P3 sparse output map, P4 all-outputs completion (see R101), P5 per-context uniforms, P7 `INSTR_N=32` vs a 48-op ceiling, P8 handle-to-slot binding, P9. P6 partial. **And P9 means MY R91 RULED ON A LEVER THAT DOES NOT EXIST** -- the lane verified `gz/carriers` carries zero field-RTL delta | **(coordinator)** R91 took the "uniforms once per association" fast path on the strength of the contract asking for it; the contract asks, and nothing implements. My ruling stands as a DIRECTION and must stop being quoted as an available lever. Warp measures **51 + T_run clocks/point against Earth's 49**, so it inherits the 481% wall rather than escaping it -- the third throughput wall this run, and the first that was ruled on before being checked. **The honest position: GEOM.WARP is architected, its reference model is built and tested, and it cannot compose until nine shared Field prerequisites are built.** The lane composed NOTHING rather than tying its Field port off, which would have converted an honestly-absent entry into a tie-off. |

## R104 — the attrdiv v2 adoption is FUNDED, and its blocker is smaller than reported

**2026-09-20, coordinator.** PROJADOPT refused the `zhao_raster_attrdiv` →
`zhao_raster_attrdiv_v2` swap and named the blocker correctly in kind: v1
publishes `rem_o` and both consumers seed the proven step recurrence with it, so
a swap that dropped it would DELETE FUNCTION — the exact move rule 1 forbids.
The refusal was right. **The cost estimate attached to it was not**, and the
difference decides whether this is worth a slot.

**v2 already computes the remainder.** `zhao_raster_attrdiv_v2.sv` carries
`rem_r [48:0]` at line 79 and runs the full restoring recurrence at 148–173
(`rem_shift_c`, the three `d1_c`/`d2_c`/`d3_c` compares, `rem_next_c`). What it
does not do is expose it. So the repair is **publishing an internal signal that
is already correct by construction**, not implementing new arithmetic — and the
distinction matters, because a remainder that has to be written is a
correctness risk needing its own oracle, while one that already exists needs a
port and a width check.

**Two real questions the lane must answer, neither of them arithmetic:**

1. **The width is not the same.** v1's `rem_o` is `[47:0]`; v2's `rem_r` is
   `[48:0]`. After the final restoring step the remainder is less than the
   divisor and should fit, but *should* is not a measurement — **prove the top
   bit is clear at the point of publication**, by stimulus, and if it is not,
   the port is 49 bits and every consumer widens. Do not assume the truncation
   is safe because the mathematics says so; the mathematics describes the
   converged value, and a port publishes whatever is in the register.
2. **The refusal semantics differ.** v1 has `q_overflow_o`; v2 has
   `q_saturated_o` AND `q_error_o`. Those are not the same signal renamed —
   v1 REFUSES where v2 SATURATES. Every consumer of `q_overflow_o` needs an
   explicit mapping decision, written down, and a test that exercises the
   overflow path on both sides. This is the part that can silently change
   behaviour, not the remainder.

**Why this is funded rather than queued behind the gaps.** These two sites —
`zhao_raster_attrdiv_svc` and `zhao_raster_attrstep` — are the LAST
superseded-in-a-production-root pair in the tree (4 → 2 after R99 retired the
two projector shells, and these are the 2). R86 made that check fatal across all
72 roots precisely so the console cannot be fitted while composing a superseded
module. **It is therefore a PRE-FIT BLOCKER, not a gap**, and it outranks queue
position.

**The rounding question is settled and must not be re-litigated.** R100
established over 640,000 sampled pairs that v2 matches
`zref::render::div_rhu_s128` EXACTLY in every sweep, while v1 disagrees on 100%
of negative exact halves with an even divisor — a rate of about 1/(4d), which is
over 10% on a one-subpixel triangle. `spec/qformats.md:53`, `:147` and
`rast.cpp:31-41` all say round-half-up, and the owner ruled this same shape on
2026-09-09 ("qformats governs", `FORGE.PRIM.md:105`). My earlier framing that
"the repo's stated law and zref disagree" was FALSE and is withdrawn. Adopting
v2 is a bug fix, not a preference.

**No owner decision is owed here.** PROJADOPT listed "funding the `rem_o`
repair" as owed to the owner; it is a scheduling call inside a run whose
standing goal is to reach zero and then fit, so I am making it. The owner
decisions that remain genuinely owed are the two contact sheets (R37, R65) and
the seven unresolved `reference_model:` rows.

## R105 — the obituary resolves the corpse: CHECK 5 was blind to comments

**2026-09-20, coordinator, found while merging `gz/projadopt` onto `gz/warp`.**

R94 built `uncashed_cheques.py` CHECK 5 to catch a `reference_model:` naming a
symbol the tree does not have — the defect being that a name resolving to
NOTHING can never collide with another block's, so such a row is silently exempt
from CHECK 3, the one instrument this tree has against a second implementation of
ratified arithmetic. The defect makes the report SHORTER, which is the flattering
direction.

**The check resolved a name by substring against RAW FILE TEXT, comments
included.** `leaf in ref_blob`, where `ref_blob` was every byte of every `.hpp`
and `.cpp` under `reference/`. So the moment anyone writes a header saying "this
replaces the `zref::GeomWarp` phantom", **the obituary resolves the corpse** —
and the more carefully the phantom is documented, the more thoroughly the
detector is disarmed.

**This was one `using` declaration away from happening.**
`reference/include/zref/zref_geom_warp.hpp` names `zref::GeomWarp` in prose at
lines 1 and 6. The warp lane also added a real
`using GeomWarp = geom_warp::GeomWarp;` at `zref` scope, deliberately, so the
ledger's spelling resolves to something a compiler can name. **Had it not, the
GEOM.WARP row would have passed CHECK 5 on the strength of a sentence explaining
that it should not** — and the merge that brought the two halves together is the
first moment either could be seen.

**Demonstrated before repairing, per the broken-instrument law.** The function
was called with a blob containing the name only inside a `//` comment: it
returned no rows. The same name absent entirely reported correctly. So the
positive control that R94 shipped worked, and it worked on precisely the case
that could no longer occur once someone documented the phantom. A gate that
cannot reach the state is not evidence about the state.

**The repair** is `strip_cxx_comments()`, applied inside
`unresolved_reference_models()` rather than only in the loader — that is the one
place the question "does this name resolve" is answered, and every self-test
supplies its own blob. The first version of the repair stripped only in the
loader and `_comment_blindness_self_test()` caught it on the first run, which is
the self-test earning its place within minutes of being written.

**It is not a parser and is not meant to be.** A name in a string literal or in
dead `#if 0` code still resolves; only a compiler settles the general question.
It removes the one failure mode that THE ACT OF DOCUMENTING A PHANTOM creates.

**The repair changes no result today** — CHECK 5 still reports 6 of 92, the same
rows as before, so nothing in the tree was currently resolving on a comment. That
is worth stating plainly rather than dressing the change up as a catch: the value
is prospective, and a hardening that fixes nothing visible is still the right
trade when the mode it closes is one that hides evidence.

The six that remain unresolved are `zref::MeasureHistogram` (MEASURE.HISTOGRAM),
`zref::PostComposite` (POST.COMPOSITE) and four PART.* rows. Each needs the same
per-row call R94 defined: name the law that exists, or REMOVE the key and say in
the row why the block has no reference model. **Inventing a plausible symbol is
the same defect with a better name.**

## R106 — MY ERROR: I relayed an UNMERGED BRANCH's contents as tree state

**2026-09-20, coordinator. This one is mine and it cost a lane real work.**

The FORGECONNECT brief told the packet that R94's ledger repair to FORGE.SHADOW
was already applied — the `reference_model` key removed, `directed:` naming the
real test. **It was not in the tree.** PROJADOPT had done it in `3b2edda8`, on
branch `gz/projadopt`, which I had not yet merged. I read a packet's report,
which was accurate about its own branch, and restated it as a fact about the
shared tree.

The packet found the row still carrying `reference_model: zref::forge::shadow_hull`,
`PLANNED -- NOT WRITTEN` and the wrong counters, re-verified `shadow_hull` itself
(zero hits under `reference/`), and applied the repair independently. **So the
same repair now exists twice**, authored by two lanes that could not see each
other, and I get to reconcile them by hand. It also reported
`FINDINGS-projadopt.md` absent — correctly, for the same reason.

**The rule this establishes.** A packet's report describes ITS BRANCH. Until the
coordinator merges, that work does not exist for anybody else, and a brief that
states it as done is a FALSE PRESENCE of exactly the kind this run keeps finding
in the ledger — with the same shape: it reads as coverage, and it makes the next
worker skip a check. Either merge first and brief from the merged tree, or name
the branch and the commit and tell the packet to VERIFY IN ITS OWN TREE. Never
the bare assertion.

A second cost, avoided only by luck: PROJADOPT deliberately did NOT touch
`counter_catalog`/`counter_ids.lock`, saying it needed a quiet tree because the
list is append-only with ids equal to positions. FORGECONNECT did the append (9
ids, 276–284, nothing renumbered). Had both lanes appended, the merge would have
renumbered every counter after the seam. **Checked before merging rather than
assumed:** a `git log` of projadopt's range over those two paths is empty.

## R107 — Handover §12's FORGE.SHADOW blocker is FALSE ON BOTH HALVES

The handover says FORGE.SHADOW is blocked because "rung: the whole refusal" and
the kind-8 constants "cannot be built at all". FORGECONNECT checked both and
both are spent. **`zhao_geom_lodstate.sv` exists** (`feac837d`, R68 sub-build 3),
and its lines 180–195 are literally titled "the caster out: FORGE.SHADOW's
{world x, z, radius, rung, src_id}". R26 lifted the freeze. Five blocks were
built for this and all five are `pending_compose`.

That is the sixteenth false-absence claim this repo has produced, and it follows
the pattern exactly: **the refusal was true when written and nobody re-asked.**
The handover is a snapshot, not a standing fact, and a blocker quoted from it is
a claim about the past.

**The real blocker is a SEQUENCE, and the packet checked the number rather than
the prose:** `GEOM_PAY_A_W` is still 16 and full (index 12 + arena 3 + tag at bit
15), so R68 sub-build 4 has not landed. It collides with the live projector
lanes, so the packet correctly did not touch it. **D-FC-3 is accepted:** schedule
sub-build 4 after the projector lanes, with the two-bit owner field named as a
deliverable — a third client-A owner breaks `geom_tag_collision_o`'s one-bit
two-owner law. Six blocks then compose as one subsystem in one fit.

## R108 — FORGE.PRIM's five `forge_kind` members are GRANTED, and they pay a fare that is already owed

`forge_kind` has exactly one member while FORGE.PRIM has six families, so the
ABI cannot name five of them. Protocol rule 4 makes an ABI change an owner
decision, and I am ruling it because **the zidl rules its own extension**: its
comments say "new kinds are additive members", and `fog_mode` cites "the
forge_kind member-0 precedent". No `abi_version` bump is implied. Granting five
additive members applies a law the file already states.

**And it settles a queued debt by paying one fare for three.** Every edit to
`spec/commands.zidl` moves `ZHAO_ZIDL_SHA256` and forces all five golden captures
to be regenerated through their real producers — `demo_duo_markers --write` alone
is 600 Duo frames, about an hour. Two corrections have been waiting for someone
to change that file for a real reason: the **DebugTraceArm** comment's
over-broad guarantee, and **R77's `tmu_mode`** comment. This is that reason.
**All three land in ONE commit**, and the regeneration is bought once.

**It closes no gap on its own, and the packet was right to say so.** A forge page
kind must still be frozen, and four of the six families have no evaluator. The
ABI stops being the blocker; it does not become the implementation.

## R109 — FORGE.CLIFF's two rivals are DECIDED BY THE GATE THAT ALREADY EXISTS

`zhao_forge_cliff_ram` is not a child of `zhao_forge_cliff` but a **rival
candidate**, and `console_inventory.yml:109-127` gives both the identical
boilerplate disposition — which is how the rivalry stayed invisible. A ledger
that describes two competing implementations in the same words is not recording
a decision, it is hiding that none was taken.

**The deciding gate exists and has never run: F-CLIFF1**
(`fit_targets.yml:1608-1625`). It is a `quartus_map`, **not a fit** — which
matters, because CLAUDE.md's fit-at-subsystem-boundaries rule is about the 1.5–4
hour placements, and a map is minutes. The question is RAM inference between a
logic implementation and a `_ram` one, and RAM inference is precisely the class
that genuinely needs Quartus rather than Verilator.

**Until it is ruled, NEITHER may be composed** — composing either one would be
choosing by default, and `check_console_inventory.py` G3 would then be enforcing
a decision nobody made. F-CLIFF1 runs in the pre-fit batch.

## R110 — `check_counters.py` is ONE-DIRECTIONAL, and that is why the ledger drifted

It sees a counter DECLARED with no port. It is blind to a port with no
declaration. Of the ten census ports FORGECONNECT named, **it could see one** —
`shadows_skipped`, the declared-but-absent direction — while nine real counters
emitted by live RTL were invisible to it because the ledger simply never
mentioned them.

This is the same structural blindness as R105 and as the metadata-bank detector
in CLAUDE.md: **the check compares two things and only looks one way down the
comparison.** A tool that answers "is everything declared present?" and never
"is everything present declared?" reports a clean ledger for a tree whose ledger
is half missing — and, once again, the defect makes the report SHORTER.

Nine ids appended (276–284, total 285, self-test 4/4, nothing renumbered). All
ten counters were already fired by legal stimulus, so none is owed a mutant —
the packet checked that rather than assuming it. The second direction of
`check_counters.py` is owed and goes to whichever lane next has a quiet tree.

## R111 — R101 is REPAIRED, and the repair is DORMANT: that is the next defect, already

**2026-09-20. FIELDP4 landed the R101 repair at `f98e7f86` and reported the
caveat itself rather than letting it be discovered.** The repair is right, the
evidence is the best this run has produced, and the caveat outranks both.

**What was proved, and how.** Header bits `[32 +: OUT_LANES]` now carry a
required-output mask; `seen == 0` still yields `ST_NO_RESULT`, and
`mask != 0 && seen != 0 && (seen & mask) != mask` yields the new `ST_PARTIAL`
with counter `out_incomplete_o`. 16 substantive RTL lines. The packet verified
the 64-free-bit claim from the decode itself rather than from my brief — under
`LdHeader` the block reads only `[8 +: REGW]` and `[22:16]`.

**54 checks green on the first run is the shape this repo distrusts, and the
packet distrusted it.** It restored the pre-repair RTL and re-ran the identical
driver: **6 of 54 failed, first line `expected 0xF3, got 0x0`** — the shipping
defect executed, not argued. 48 still passed, so the test discriminates on
exactly the repaired behaviour and nothing else. A permanent control stays in
the suite: case 1d runs ONE program twice with only the MASK moved (0b0011 → OK,
0b0111 → refused). That is the correct shape — it asserts the right behaviour in
both polarities, and it is not a test that asserts the bug.

**The blast radius has two halves and the second is the one that matters.**
*Today:* nothing. The smoke asserts `fld_runs_o == 0` and FATALs if it moves;
the F profile is armed at an empty slot. The defect is shipping RTL that is not
presently producing wrong values. *On the first program loaded:* it would. The
new probe `tools/field/zprog_output_coverage.py` measured all three shipped
Earth programs — masks 0x17 / 0x1D / 0x17 — and **every one leaves three of the
console's seven window lanes unwritten**, because output registers are not
contiguous and the capture window is. **Holes are the NORMAL case, not an edge
case**, and the packet says plainly that it had assumed the opposite before
measuring. The stamp adapter reads window lanes 0–1 and the flow adapter lanes
3–5, so a program writing only lane 6 satisfied the OLD test and would have fed
flow three zero velocities — a plausible-looking maximum deceleration. That is
the whole argument for why a silent wrong value beats a refusal for damage.

**AND THE GUARD IS INERT.** Nothing in the tree emits a header word at all, so
`mask == 0` is not merely backward-compatible, it is the only case that occurs.
**A plan writer who omits the mask silently restores the defect and passes every
gate** — which is R105 and R110's family again, a mechanism that reads as
protection while providing none, and the third instance in one day.

**The ruling: emitting the mask is a REQUIREMENT OF THE PLAN PRODUCER, not an
option it may take up, and it needs its own gate before that producer merges.**
FIELDP4 wrote the warning where it will be read — `zhao_field_doorbell.sv` §2b
with the three measured masks, and the FIELD.SEQ.CORE contract — and that is the
right thing to have done, but **prose is what the fit-batching rule needed a
Stop hook for.** Whoever writes the header/plan producer owns: a mask emitted
for every program, a check that refuses a plan whose mask is zero while its
declaration names outputs, and that check SEEN TO FIRE. Until then this entry
stays live in the queue, and `out_incomplete_o` reading zero is not evidence
about the console — it is evidence that nothing has asked yet.

**R101 does NOT close GEOM.WARP's P3.** The capture is still one contiguous
window. That remains one of R103's nine.

**Also found, and it is the ledger defect one level down:**
`field_host_directed`'s own header named three counters that exist nowhere in
`fpga/rtl` — `hdr_clamped_o`, `tbl_oob_o`, and `pc_oob_o`, the last described as
"the committed-mutant case" — plus a case 7 that was never written. **A false
presence inside a TEST header reads as coverage** exactly the way a false
`reference_model` does, and no gate looks there. Corrected.

**No owner decision is owed:** the repair enforces W10 and `field-ir.md` §7.1,
both already in writing.

## R112 — R70 STANDS. My suspicion that it needed reversing was wrong, and the lane proved it first-hand

**2026-09-20.** I briefed TERRAIN9 that R70 might be cheaper to reverse than to
keep. R70 had ratified the histogram's v1 metric as the terrain page-load LOD
deviation (option A), choosing it partly BECAUSE option B — a per-camera
pixel-error residual — needed a quantity nothing computed. R26/R68 then landed a
per-camera pixel-error threshold and R83/R98 widened it to Q12.8, so I reasoned
that B's producer now existed and R70's stated reason was spent.

**It is not the same quantity, and the lane checked the RTL rather than the
prose.** `zhao_measure_governor.sv:206-207`: `px_err*_i` are **INPUTS** —
`SetView.pixel_error`, an AUTHORED BUDGET handed in by the host. `:294-295` are
derived **thresholds**. Option B wants an OBSERVED RESIDUAL, and nothing in this
console computes one. What R26/R68 landed is a budget and a threshold, not a
measurement of error.

**R70 is closed and needs no owner action.** I was right to make the lane
re-ask — an organ fed the wrong quantity is exactly what R70 exists to prevent,
and the cost of asking was one verification. I was wrong about the answer, and
the reason I was wrong is instructive: **"a per-camera pixel-error quantity now
exists" is not the same claim as "the per-camera pixel-error RESIDUAL now
exists", and the names are close enough to swap without noticing.** That is the
mismatched-quantity error the art law warns about, in a register file instead of
a drawing.

## R113 — THE SMOKE'S TERRAIN STIMULUS WAS DEAD, AND PASSING

**This is the biggest instrument finding of the run.** R70's third requirement
was "check that the smoke's stimulus drives TERRAIN.MIPFEED's fine stream at
all" — a requirement written because an earlier lane had quoted a traverse
without verifying it. TERRAIN9 checked. At `c55e0417`:

```
pl loaded=0 faulted=3        mipfeed samples_sent=0
lodfd dev_records=0          hist events=0
SMOKE: PASS
```

**Every terrain number was zero and the bench passed over all of it.** A gate
that cannot reach the state is not evidence about the state, and for an unknown
number of passes this one was cited as though it were.

**The recorded cause of the failure was also wrong.** The bench blamed
`"CRC cannot match"`; the measured truth is `crc_fails=0`, `hdr_ident_fails=3` —
it played a HEADERLESS page and failed the IDENTITY test, not the CRC. And the
bench asserted `"TERRAIN.MIPFEED is not composed"`; it is, at
`zhao_console_core.sv:15281`. **A false-absence claim inside the test harness**,
which is the seventeenth of these and the first found in a bench.

Repaired by giving the played pages a real §2.1 header and their own body CRC
over `[64, 21320)`. The chain now carries a value end to end:
`loaded=3, resident=3, samples_sent=6534, dev_records=48, hist events=144` —
and `raster pixels=2560 / frames_admitted=1` **unchanged**, so R12's
reference-derived pixel count needed no regeneration. That invariance is itself
the evidence that the repair added stimulus without disturbing the raster path.

**Three checks were then found that could only ever have held at zero.** This is
the category to hunt, because each one is green forever and means nothing:

1. **`crc_failures == N_TERR_REC`** used CRC *failures* as a proxy for
   completions *arriving*. It **asserted the stimulus's own defect** and would
   have gone RED on an improved machine — a test that passes only while the
   thing it tests is broken. Repaired.
2. **`hist_events == dev_records`** compared **two different units**. Measured:
   48 records produce 144 events, at 3 valid lanes per record. Repaired to the
   real law plus an aggregation bound.
3. **`stray_samples_o == 0`** — measured **2,726**.

## R114 — `stray_samples_o` is a MISLABELLING, and NOT repairing it was correct

`zhao_terrain_lodfeed` classifies on `fill_active_q`, which is cleared when the
deviation walk RETIRES. Surface-1 samples that arrive after the walk are
therefore filed as "a sample with no start" — 2,726 of them. The block, as
built, **cannot distinguish the two cases**.

TERRAIN9 had a working RTL fix and **reverted it**, because it turned
`terrain_lodhist_directed` check 3 — another lane's committed positive control,
encoding the present meaning — red (expected 8, got 0). **Re-authoring another
lane's control is not a thing a packet should do silently**, and stopping was
the right call. Instead the smoke now asserts a law that is TRUE
(`stray + VERTS*walked <= samples_sent`) with the mislabelling printed by name.

That is the correct shape for a known-wrong instrument: assert what is true,
name what is wrong, and hand the re-authoring to whoever owns the control.
Queued, with the measured number attached so the next lane does not have to
rediscover it.

## R115 — TERRAIN.NORMALMAP has a contract, a ledger row, an oracle and a 4,738-check suite, and NO RATIFIED SPEC SENTENCE

Everything downstream of a specification exists; the specification does not.
This is the inverse of the phantom-reference defect (R94/R105): there the ledger
pointed at something absent, here everything real points back at nothing.

**Owner decision: ratify a spec sentence for TERRAIN.NORMALMAP, or supersede the
block.** It cannot be composed against a law that was never written, and writing
one now to match the implementation would be ratifying whatever got built —
which is how an accident becomes a requirement.

## R116 — R65 IS THE RUN'S HARDEST STOP AND FOUR PACKETS ARE NOW QUEUED BEHIND IT

`reports/terrain-seam-dig/seam_dig_contact.png` is rendered, committed and
pushed. It is waiting on the owner's eye and on nothing else.

**It gates I32, TERRAIN.BAKE's option A, and the terrain PAGE FORMAT itself.**
Six terrain lanes have now closed zero of the same four disconnected blocks —
five before TERRAIN9 and TERRAIN9 itself, which says plainly "I am not the
exception". That is not six failures; it is one blocker seen six times.

**The packer does not exist yet, so the format is cheaper to change now than it
will ever be.** And the render changed the question rather than answering it:
spec §9.3(c) says the half-cell step "does not read as a seam", while the sheet
shows a rim wrong by up to one vertex, everywhere. Under the art law this is not
a measurement question at all — it is a LOOK, and only the owner's look settles
whether that rim reads as a defect.

Escalated with the cost attached: **one look unblocks four packets.**

## R117 — F-CLIFF1 HAS ALREADY RUN. It ran on 18 September, it was a FULL FIT on the TARGET DEVICE, and R109 is superseded

**2026-09-20, coordinator. This corrects a ruling I wrote three hours ago, and
the error in it is the one this run keeps finding.**

R109 said *"the deciding gate exists and has never run: F-CLIFF1"*. I took that
from the FORGECONNECT lane's report and did not re-ask. I then wrote it into
`reports/FIT-PLAN-AT-ZERO.md` as a fit to schedule. **Both statements are
false.** `reports/synthesis/blockpaths/` has carried the receipts since
**Fri 18 Sep 2026**:

```
zhao_forge_cliff_ram@first-measurement.map.rpt / .map.summary
zhao_forge_cliff_ram@first-measurement.fit.rpt / .fit.summary
zhao_forge_cliff_ram@first-measurement.sta.rpt / .setup.rpt / .hold.rpt
zhao_forge_cliff_ram@first-measurement.sources.sha256
```

That is the **seventeenth** false-absence claim in this repository, and the
first one I authored myself. The pattern held exactly: the refusal was true when
written, nobody re-asked, and I repeated it with a ruling number attached, which
made it *more* authoritative rather than less. **A ruling is not a place to
launder an inherited claim.**

I found it only because I went to RUN the gate and looked for the runner.

### The provenance holds — checked before quoting anything

`.sources.sha256` records `1D6C6D07E208B267...` and
`fpga/rtl/forge/zhao_forge_cliff_ram.sv` hashes to `1d6c6d07e208b267...` **at
this commit**. The measurement describes the file that is in the tree today.
This is the discipline from `CLAUDE.md` — read the provenance before the number
— and it is the reason these numbers are usable at all.

### The answer to question (a): PASS

The gate asked whether `inferredMemories` lists **five** memories. It lists
exactly five: `win_mem_rtl_0`, `edge_key_r_rtl_0`, `edge_span_r_rtl_0`,
`prio_mem_r_rtl_0`, `run_mem_r_rtl_0`. Total MLAB memory bits **0**, total block
memory bits **120,964**, 15 RAM blocks. The gate's stated failure condition was
*"flip-flops is the failure"* — the window went to block RAM, not flops.

**With one qualification the gate itself set and which is NOT met:** it demanded
`ramConversionWarnings 0`, and the map reports **four** `Warning (276020)` —
*"Inferred RAM node ... Pass-through logic has been added to match the
read-during-write behavior of the original design."* That is not a conversion
*failure* (the RAMs were inferred), but it is not zero either, it costs logic,
and it means four RAMs have had their read-during-write semantics matched by
added logic rather than by the memory block. **Declared as a breach rather than
rounded away.**

### The answer to question (b), stated at the ONLY stage where both sides exist

**This is where the comparison must be handled carefully**, because the two
sides are not the same kind of number and the gate's own comment warns about it.

| | golden `zhao_forge_cliff` | candidate `zhao_forge_cliff_ram` |
|---|---|---|
| stage | **map-only ESTIMATE, never fitted** | map-only, **and** a full fit |
| comb ALUT (map) | 8,149 | **1,326** |
| registers (map) | 3,875 | **826** |
| DSP | 2 | 2 |
| memory bits | 119,808 | 120,964 |
| **fitted ALM** | **does not exist** | **976** on `5CSEBA6U23I7` |

Like for like, at map stage: **the candidate uses 16% of the golden's
combinational ALUTs and 21% of its registers.**

**I must NOT say "it saves 6,688 ALM."** The golden's 7,664 is a map-only
estimate that `reports/FORGE-CLIFF-BITMAP-RAM-20260910.md:165` explicitly labels
*"an ESTIMATE (map-only), never fitted"*, while 976 is a fitted number. Setting
those two against each other is the mismatched-comparison error — the same shape
as measuring a grounded stance against an aerial drawing — and it would be a
confident number in the flattering direction.

What can be said, and it is enough: **`zhao_forge_cliff` is 7,664 ALM and
18.3% of the whole ALM budget** (`reports/BUDGET_HEATMAP.md:159`), the candidate
is **fitted at 976 ALM, 2% of the device, on the actual target part**, and at
the one stage where both were measured the candidate is smaller by a factor of
six on ALUTs. **ALMs are the binding constraint and the console is at roughly
113% of the device.** This is plausibly the single largest lever in the tree,
and it has been sitting measured and unused for two days because everyone
believed the gate had not run.

### The inferred latch: I called it a defect too fast, and it is not one

The map reports `Info (10041): Inferred latch for "triangles_submitted_o[0]"`.
My first reading was that this blocks adoption. **It does not, and the reason is
worth writing down rather than quietly dropping.**

`triangles_submitted_o` is a plain `always_ff` register: reset to `32'd0` at
line 581, and the only other assignment is at 881–882, `+ 32'd2`, commented
`C3`. **It counts triangles in PAIRS**, so bit 0 is provably constant zero for
all time, and that constant bit is what Quartus latched. It is an artifact of
incrementing a 32-bit counter by two, not a missing branch in combinational
logic.

Cheap cleanup, not a blocker: make the counter 31 bits with a hard-zero LSB, or
count pairs and multiply at the port. Worth doing because a latch cell is real
area and complicates timing closure — but it does not gate the decision.

### The ruling

**R109 is superseded. `zhao_forge_cliff_ram` is the version to adopt**, subject
to two things that are now specific rather than a deferral to an unrun gate:

1. **The four `Warning (276020)` pass-through insertions must be accounted
   for** — either accepted in writing with their cost, or removed by matching
   the RAM's native read-during-write behaviour. The gate asked for zero.
2. **The bit-0 latch cleaned up**, as above.

`console_inventory.yml:109-127` must stop giving the two blocks the identical
boilerplate disposition — a ledger that describes two rival implementations in
the same words is not recording a decision, it is hiding that none was taken.
The golden becomes `superseded` with this ruling and these receipts cited, and
**`zhao_forge_cliff` needs its own fitted row before any final claim about the
saving is made**, because the number everyone will want to quote is
fit-minus-fit and only one half of it exists today.

### And the fit plan is corrected

`reports/FIT-PLAN-AT-ZERO.md` listed F-CLIFF1 as a fit to run. It is done. What
belongs there instead is **a fit of the GOLDEN `zhao_forge_cliff`**, so the
comparison stops being estimate-versus-fit — and that is a cheap leaf fit, not a
console placement.

## R118 — MEASURE.GOVERNOR is blocked at BOTH ends, and composing it would OPEN tie-offs

**2026-09-20, POST3B, re-verified rather than inherited.**

The governor has been sitting in the disconnected eleven with the implicit
assumption that it needs wiring. It needs more than that, in both directions.

**Downstream:** `zhao_terrain_lod`, `zhao_geom_lod` and `zhao_geom_lodstate` are
**all uncomposed** (terrain_lod appears only in the generated pricing top), and —
the part that decides it — **no core boundary port exists that a governor output
could replace.** So composing the governor today does not close a gap; it
**creates** them, because its outputs would terminate at new unowned edges. That
is rule 1 in its least obvious form: the move that looks like progress converts
one disconnected block into several tie-offs and the register gets *worse* while
the diff looks constructive.

**Upstream:** `px_err` and `view_count` need **two** new CMD.EXEC arms, not the
one that was assumed, and `proj` needs I14's viewport rect — which is a different
lane's tie-off.

**Correctly refused.** The governor closes when TERRAIN.LOD composes, and
TERRAIN.LOD is behind R65, which is behind the owner. POST3B kept its worktree so
the follow-up is one commit — **but that follow-up is NOT currently available**,
because it is predicated on TERRAIN9 landing `zhao_terrain_lod` and TERRAIN9
refused all four of its terrain blocks. Re-check before acting on the offer.

## R119 — the governor → PART.LADDER edge is asserted in the LEDGER and realised in NEITHER CONTRACT

Sharper than "nobody chose", which is how I had it.

`MEASURE.GOVERNOR.md`'s output table **names a consumer port for every policy
output** — except `deg0_o`/`deg1_o`, which name none and are annotated "capture /
post-mortem". And **every port it does name belongs to TERRAIN.LOD.** Meanwhile
`PART.LADDER.md` contains the words "deg", "floor" and "degrade" **zero times.**

So `design/blocks.yml` asserts an edge that neither side's contract realises at
port level. A ledger edge with no port on either end is not a design decision
that has not been wired yet — it is a **claim with nothing behind it**, and it is
the same family as a `reference_model:` naming a symbol that does not exist
(R94): it reads as architecture and buys silence from every check that works by
matching two declared things together.

**Ruling: add `p_deg_i[1:0]` and SHIFT the ladder's size thresholds left by
`deg`. Do not build a `deg → floor` table.** POST3B's reasoning is right and it
is a correctness argument, not a preference: **a floor is a clamp, and a clamp
collapses an entire population onto one rung.** Shifting thresholds degrades the
distribution while preserving its shape; flooring it deletes the shape. Both
contracts get the port written into them in the same commit, so the edge exists
in three places or in none.

## R120 — I17's HUD store is 153 of 553 M10K, and it had never been costed on-chip

The I17 entry said **"SDRAM"** and no one had priced the on-chip alternative.
POST3B priced it: **153/553 M10K, 27.7% of the device's memory**.

That number changes the shape of the decision rather than settling it. M10K is
the one budget with slack (ALM is at ~113% and DSP at ~135%), so 27.7% is
*affordable* in a way the ALM equivalent would not be — but it is not free, and
`trade-ALMs-for-M10K` only pays when the lever is **lookup-for-computation**, not
when it is relocating a store. Relocating the HUD from SDRAM to M10K buys
bandwidth and determinism, not ALMs.

**Recorded so the next lane argues from the number instead of from the word
"SDRAM".** The decision stays open and belongs with I17's owner.

## R121 — `mutant_copy_drift.py` READS COMMIT ORDER, so it cannot gate an uncommitted merge

**Found the hard way, on my own merge.**

The gate compares when the mutant and its production module were last committed.
I ran it on a **staged but uncommitted** merge, where git still reported the
*old* doorbell commit, and it returned RC 0 — an answer about the tree *before*
the merge. I recorded that as a green gate. After committing, the same gate
returned **RC 1**: `zhao_field_doorbell_mutant` had gone stale because
`gz/fieldp4` edited `zhao_field_doorbell.sv` at `cb20a231`.

The tool is not wrong and it even announces the condition when it hits the other
half of it — *"1 pairs skipped: one side has uncommitted edits, so there is no
commit order to read."* **A gate whose input is git history cannot be run against
the working tree**, and the failure is silent in the flattering direction: it
reports the *previous* state as current.

**The rule: `mutant_copy_drift.py` runs AFTER the merge commit, not before it.**
Every other gate in the table reads files and is correct on a staged tree; this
one reads history and is not. Added to the gate table with that qualification.

**And the second-order lesson, which is mine.** The reason I believed the stale
green is that my gate loop wrote
`printf "%-34s RC=%d\n" "$(basename $g)" "$?"` — the command substitution runs
**before** `$?` expands and resets it, so **every gate printed RC=0 regardless of
outcome.** I had identified that exact trap in this same session, written it into
the task log, and then repeated it an hour later. Capturing `rc=$?` on its own
line immediately surfaced the drift.

Two instruments lying in the same direction at once is how the run's worst
defects have all looked. Here the shell said "green" about the wrong tree, and
the loop said "green" about nothing at all.

## R122 — THE LAST PRE-FIT BLOCKER IS CLEARED, and R104's cost model was wrong in the EXPENSIVE direction

**2026-09-20, ATTRDIV, at `c077ee48`.**

**`superseded check: 71 production roots CLEAN` — 2 → 0.** Nothing in `fpga/` or
`tests/` instantiates `zhao_raster_attrdiv` any longer. R86's check is the one
that must read zero before the console can honestly be fitted, and it does.
Register 21 → 21, unchanged and correct: R104 framed this as a pre-fit blocker,
not a gap.

### R104's two questions, answered by STIMULUS rather than by argument

**Width.** I ruled that the top bit must be proven clear *by stimulus* because
"the mathematics describes the converged value; a port publishes whatever is in
the register". It was: widest observed `rem_o` is **47 bits** over the widest
legal areas across **2,800 divides**, with bits 48 *and* 47 clear and the new
guard reading zero. **No consumer widens.**

**Refusal semantics.** Split rather than renamed, as suspected. The service
publishes `q_saturated_o` and `q_error_o` and **collapses neither**; ATTRSTEP
refuses on both, reproducing v1 exactly — **because a clamped quotient breaks
`q*A + r == M` and is therefore not a legal seed.** That is a better reason than
the one I gave, which was only that behaviour must not change silently.

### AND THE PART I GOT WRONG, which is the reason to write this down

R104 said v2 *"already computes the remainder … the work is publishing an
internal signal that is already correct by construction, not implementing new
arithmetic."* **The first half is true and the second is not.**

**v1's remainder is mod 2A on a DOUBLED dividend. v2's is mod A.** They are
different quantities. **No fixup connects them** — the consumers' algebra had to
be **re-derived**, not rewired. I had reasoned from "the register exists and
holds a remainder" to "the remainder is the one the consumers need", which is the
mismatched-quantity error this run keeps producing: R112 was the same shape (a
pixel-error *budget* is not a pixel-error *residual*), and so was R98's
`uint16_t` that truncated where the ports saturate.

**`design/prod_manifest.yml:149` already said this, and was more accurate than my
ruling.** I wrote R104 without reading it. Both texts are now corrected.

### Where consumer numbers move — stated, not buried

* **Every negative exact half with an even divisor moves +1 LSB**, at a rate of
  about 1/(4d). This is the bug fix, not a regression: `qformats.md` and
  `rast.cpp` both say round-half-up and v1 did not.
* **Overflow** now answers with a saturated value at the service and still
  **refuses** at ATTRSTEP.
* **Divides per pixel go DOWN** — 69 sign changes, **zero reseeds**, because the
  crossing reseed is gone.
* **Each divide is 2.6–2.8× LONGER: 36 → 103 clocks**, measured at radix 2.

**That last one is a real regression and it is stated in both block headers
rather than hidden.** It is accepted as the price of correct rounding — a
silently wrong quotient on every negative exact half is worse than a slower
correct one — and the window-trick follow-on is named in the headers for whoever
takes it. **Divides/pixel falling partly offsets it, but the two are not the same
unit and must not be netted against each other in any summary.** PHYSICAL FIT
PENDING: the clock cost is a cycle count, and whether it moves Fmax is a
different question that only the fit answers.

## R123 — the lane's OWN FIRST MUTANT WOULD NEVER HAVE FIRED, and it found that before shipping it

The canonical mutation shape for a full-guard is `>=` → `>`. ATTRDIV wrote it,
**measured 0 fires in 360,000 pairs**, and threw it away rather than committing a
positive control that proves nothing.

The shipped mutant uses `>= 2*den` and **fires at 1 after 2 divides**, with the
unmutated build as the negative control.

**This is the committed-mutant law working exactly as intended, one level deeper
than it is usually applied.** `CLAUDE.md` already says a guard you cannot reach
with legal stimulus needs a committed mutant. What this adds: **a mutant is
itself an instrument, and it can be blind too.** A mutation that the design's own
arithmetic never exercises is a green control attached to nothing — the
broken-instrument law applied to the instrument that was supposed to prove the
instrument. **Measure that your mutant fires before you commit it**, and quote
the number of trials over which it did.

Two more from the same lane, both in the flattering direction:

* **A false presence in `raster_attrstep_directed.cpp`'s header** — it claimed
  the divider was its oracle while the test actually restated v1's law locally.
  A test that says it checks against an oracle and does not is worse than one
  that admits it is self-referential, because it reads as independent evidence.
* **An svc gate asserting `ovf == 0` that could never reach the overflow state.**
  The fourth check found this week that can only ever hold at zero.

## R124 — `zhao_raster_attrwalk` was NOT adopted, and the stated blocker is not the real one

ATTRDIV refused to adopt `zhao_raster_attrwalk` in place of ATTRSTEP: it needs
**three pre-computed Euclidean pairs and no seed stage exists.** Searched every
`.sv` for `attrseed`, `attr_seed`, `seed_stage` and `attrwalk`; the only hits are
the file itself. Adopting it would have **deleted function**, so the refusal is
correct.

**But its manifest row still states a blocker that is now settled**, while the
real blocker — the missing seed stage — is not recorded anywhere. A row whose
stated cause has expired is how a refusal survives past its reason, which is the
mechanism behind every one of this repo's seventeen false-absence claims.

**Ruling: correct the row to name the seed stage.** Whoever next looks at
attrwalk should meet the true blocker, not an expired one.

## R125 — the two-mask confusion is now a COMPILE ERROR, and both polarities were seen

**2026-09-20, packet S1, at `15349799`. Register 21 → 21, which directive §20.2
names as the correct outcome for this packet.**

The ZFH2 host-image schema is generated into C++ and SV from one source.
`RequiredMask` (canonical output ordinal) and `WindowMask` (contiguous
capture-window position) are **distinct generated types**, so assigning one to
the other no longer compiles. That was the deliverable: not a comment saying they
differ — the console already had one, and R101 shipped anyway — but a **type**.

**Every claim was demonstrated in both polarities, which is what makes this
usable evidence rather than a green:**

* **Cross-check:** `182 symbols agree` (RC 0) → deliberately mismatched offset →
  `OFFSET MISMATCH ZFH_PM_OFF_REQUIRED_MASK: C++ says 7, SV says 6` (RC 1) →
  restored, RC 0. **Restoration verified by CONTENT, not by re-copying the file.**
* **Compile-fail control:** positive case rc 0; negative case rc 1 with
  `no match for 'operator=' … WindowMask … RequiredMask`.
* **Roundtrip:** 142 checks / 0 failures, then **deliberately fired to 2
  failures with 140 still green** — so the suite discriminates on the thing
  under test rather than collapsing wholesale. FT107 digest
  `0x194E6739818D1D5E` identical across three runs.
* **The 0.10 s green was checked with `-V` rather than quoted.** That is the
  ctest stale-binary lesson applied by a packet before it was bitten by it.

And it corrected the line numbers in its own brief rather than inheriting them:
`required_mask`/`window_mask` appear **nowhere** in `zhao_field_host.sv` (they
are `req_mask_c` / `hdr_outreq`), and my cited `:851` is `out_hit_c` —
`out_idx_c` is `:854`.

**`field-ir.md`'s Formation `(11)` → `(12)`** is fixed, with all five rows
re-counted rather than just the one that was reported.

## R126 — `ZFH_WINDOW_MASK_BITS` must equal composed `OUT_LANES`, and NOTHING CHECKS IT

Verified independently:

* `fpga/rtl/field/zhao_field_host.sv:218` — `parameter int unsigned OUT_LANES = 4`
* `fpga/rtl/prod/zhao_console_core.sv:15869` — `.OUT_LANES(7)`
* the schema fixes `ZFH_WINDOW_MASK_BITS = 7`

Three places, one quantity, **no guard.** A future composition at a different
`OUT_LANES` leaves the schema's window-mask width silently disagreeing with the
hardware's — and the disagreement is invisible, because a mask of the wrong width
still packs, still transmits and still compares. **This is the same shape as the
defect the whole packet exists to prevent**, one level up: two quantities that
must agree, with nothing making them.

**Ruling: H1 adds an elaboration guard IN THE NEW HOST.** S1 correctly did not
touch the retained oracle. Two constraints on how:

1. **Quartus 17.0 rejects a bare module-scope `if`.** The guard goes inside
   `initial begin … end`, or `quartus_map` fails with *"syntax error near text:
   `if`; expecting `endmodule`"*. `check_quartus17_syntax.py` catches this.
2. **`--lint-only` does not run `initial` blocks**, so a clean Verilator lint is
   **no evidence whatever** about an elaboration `$fatal`. Fire it deliberately
   with a wrong parameterisation and watch it fail before quoting its silence.

## R127 — there is a THIRD opcode-shape table, hand-written, and its only guard is a corpus

`reference/src/zfield/zfield_decode.cpp:25-108`, `opMeta()`. Its own comment is
the finding:

> *"op metadata mirrors field-ir.md §2 … duplicated from types.ts by hand ONCE,
> asserted by the fuzz corpus replay (any drift shows up as a decode/interpret
> divergence)."*

So the console now has **three** descriptions of opcode shape — the SV package,
S1's generated C++, and this one — and it is **unmentioned in all 3,108 lines of
the owner's directive**, whose FH21 asks for *one* generated capability table.

**"Asserted by the fuzz corpus replay" is exactly the claim to distrust.** A
corpus catches drift only in the shapes it exercises; for any opcode the corpus
does not reach, the hand-written table can disagree with the generated ones
indefinitely and every test stays green. That is `CLAUDE.md`'s law verbatim — **a
gate that cannot reach the state is not evidence about the state** — and it is
the third instance today, after the smoke's dead terrain stimulus and ATTRDIV's
mutant that fired zero times in 360,000 pairs.

**Ruling: the L1 packet folds `opMeta()` into the generated schema** — it is C++
under `reference/src/zfield/`, which is L1's territory, not F1's (F1 owns the SV
package). **Until then, nobody authors a fourth.** F1 is told it exists so that
adding `OP_RCP` and `OP_RING` does not create one.

**And measure the corpus's reach before retiring the claim.** If it turns out the
corpus never exercised some shape, that is worth recording as its own finding,
because the divergence may already be present and simply never asked about.

## R128 — S1's D3 was TRUE OF ITS BASE and stale by the time it reported

S1 reported `reports/FIELD-REPAIR-PLAN-20260920.md` as **not in git**, with five
packets executing against a file one `git checkout --` would destroy. It was
right to escalate: that would have been a serious failure of mine.

**It is committed, in `9e3023d2`, and pushed.** S1 branched before that commit
landed, so from its base the file genuinely did not exist.

**This is R106 running in the opposite direction, and it is worth naming as its
own case.** R106 was me stating an unmerged branch's contents as tree state. This
is a packet stating its own base's state as current — equally honest, equally
stale, and the fix is the same on both sides: **a report describes the tree its
author saw, and the reader must date it.** Neither party did anything wrong; the
protocol has to carry the dating, not the good intentions.

The packet did exactly the right thing by flagging it loudly rather than assuming
the coordinator had it in hand.

## R129 — THE OBITUARY DEFECT IS A CLASS, NOT AN INCIDENT: found again today in an unrelated tool

**2026-09-20, ENGINE1.** This morning R105 recorded that `uncashed_cheques.py`
CHECK 5 resolved symbol names by substring against **raw file text, comments
included**, so a header saying *"this replaces the `zref::GeomWarp` phantom"*
would itself resolve the phantom. I repaired that one and called it a mode.

**It has now been found a second time, in a completely different tool, written
by different hands, in a different language.**

`tests/render/test_render_texture_packet_e.py:730` asserted the presence of
`a1_render_asset_ro` — **a formal assertion that owner ruling R32 DELETED.** The
gate passed anyway, because the token survived **in a comment in the very file
the test grades.** It was grading prose.

The lane proved it rather than arguing it: deleting the token from that comment —
**touching no assertion** — moved the suite from 3 failures to 4. Removing the
dead marker put it back to 3, now green for a real reason. The three R32
successors were already pinned, so the gate is not weakened.

**Two tools, two languages, same defect: a checker that looks for a NAME in TEXT
rather than for a DECLARATION in CODE.** That is no longer an incident, it is a
class, and the class has a signature worth hunting:

> **Any check whose implementation is "does this string appear in this file"
> will be satisfied by the documentation of the thing's absence.**

And it always fails in the flattering direction — the better the comment
explaining why something was removed, the more reliably the checker reports it
present.

**Standing instruction: when a gate's evidence is a string match, ask what it
matches against.** If the answer is raw file text, it is grading prose, and it
will certify a corpse on the strength of its obituary. Strip comments, or resolve
against a parsed declaration. `uncashed_cheques.py` now does the former; this
Python gate needs the same treatment, and a sweep for others is owed.

## R130 — I COMMISSIONED THE WRONG WORK, by inheriting a blocker for the third time today

**The ENGINE1 packet was built on GEOMPAY4's closing recommendation**, which I
relayed as its reason to exist: *"it needs `ladderbank`, which needs an ENGINE1
share and a page-publication path … recommend one packet doing share +
publication + third arm together."*

**All three of those are reachable, and none of them was the blocker.** ENGINE1
walked it: the publication path is **already composed** (`zhao_part_table_loader`
is a byte-identical `pub_*` template), and requester F is the same mechanical
edit that C, D and E each already were.

**The actual blocker is that GEOM.LODSTATE sits in the MIDDLE OF A CHAIN WITH
BOTH ENDS BLOCKED.** Walking all five port groups: `thresh0_i`/`thresh1_i`'s only
producer in the entire tree is the **uncomposed `zhao_measure_governor`** (R118 —
itself blocked at both ends), and `c_*`'s only consumer is the **uncomposed
`zhao_forge_shadow`** (R89, unresolved). **Composing it closes one gap by opening
two.**

**This is the third time today I have propagated an unverified blocker** — R106
(briefing from an unmerged branch), R117 (a ruling that laundered "the gate never
ran"), and now this. The mechanism is identical every time: **a lane's closing
recommendation is a claim about the tree as that lane saw it, and I convert it
into a work order without re-asking.** A recommendation is evidence about where
to look, never about what to build.

**The fix is procedural, not attitudinal.** A packet brief that exists *because*
of another lane's recommendation must open by instructing the new lane to
**verify the recommendation itself first, and to stop and report if it does not
hold** — which is exactly what ENGINE1 did unprompted, and why it cost one
investigation instead of one wasted implementation.

## R131 — GEOM.PARAMBUF's recorded blocker names the WRONG RULE, and the real one is stronger

GEOMPAY4 reported, and I relayed, that `zhao_geom_parambuf` is blocked because
`spec/memory_rules.md` §5f makes RENDER.ASSET_POOL **read-only** with formal
assertion `a1_render_asset_ro`. I instructed ENGINE1 to re-ask what that rule
actually forbids, since the pool **is** written today via
`PublishResource`/MEM.UPLOAD.

**The rule does not apply to PARAMBUF at all.** Verified in the spec:

| range | region |
|---|---|
| `0x0600_0000 .. 0x063F_FFFF` | **PARAMBUF view 0**, 4 MiB |
| `0x0680_0000 .. 0x069F_FFFF` | shared prefetch / chunk scratch |
| `0x06A0_0000 .. 0x07FF_FFFF` | **RENDER.ASSET_POOL**, read-only |

**Disjoint.** PARAMBUF's arena ends exactly where the asset pool begins, so §5f's
read-only rule governs a different address range entirely. My instruction rested
on a false premise, and the honest-close route I suggested was worse than wrong:
**publishing a parambuf record the way MATERIAL_SET is published is a CATEGORY
ERROR** — those are host-published assets, while parambuf records are **per-frame
fabric output**. The lane said so and declined, invented no second writer, and
weakened no assertion.

**And the real blocker is STRONGER than the recorded one**, which is the
valuable half: by the lane's walk, PARAMBUF's range sits in **no MEM.GUARD region
at all** — so it is unreachable in *both* directions, not read-only in one. A
refusal that gets stronger under re-examination is the rarest outcome in this run
and the most trustworthy.

**D-1, to the owner:** grant the GEOM.PARAMBUF arena a mapped region with a
bounded ENGINE1 write arm, on R32's exact pattern. **But do not schedule it as a
PARAMBUF-closing packet** — the allocator, the quota seal and the frame-fault
path are all unbuilt, and the composed binner's chunk format is incompatible. It
unblocks a subsystem; it is not a wiring job.

## R132 — D-2 ACCEPTED: stop scheduling geometry packets against sub-build 4. The cheapest unlock is R89

ENGINE1's escalation, and it follows directly from R130 and R118.

R68 sub-build 4 has landed (`GEOM_PAY_A_W = 17`, two-bit owner). It was the right
thing to build and it unblocked what it claimed to. **But the geometry blocks
queued behind it are not blocked on payload width** — they are blocked on
`zhao_measure_governor` (R118) and `zhao_forge_shadow` (R89), and those two are
where the chain actually terminates.

**So the next geometry-adjacent slot goes to R89, not to more payload or arm
work.** Scheduling further geometry packets against a blocker that is already
spent is how five terrain lanes closed zero of the same four blocks, and it is
the same shape from a different direction: **the constraint moved and the queue
did not.**

## R133 — R132 IS WITHDRAWN. R89 cannot be implemented, and FORGE.SHADOW is a SUBSYSTEM, not a wiring job

**2026-09-20, FORGESHADOW, at `d33d99a3`. Register 21 → 21: no gap closed, and
none opened.**

I wrote R132 accepting ENGINE1's escalation that *"the cheapest unlock is R89"*
and sent a packet to implement it. **The packet verified the recommendation
before building — because R130's procedural fix told it to — and the
recommendation does not hold.**

**R89's CONSUMER half is true, and is now TRACED rather than asserted.** Ten
hops, no tie-off anywhere in the datapath: `zhao_console_core.sv:5655` → bin_pipe
`[345:298]` → tile_pipe_v2 → texture_stage_v3`:286` → fragment →
`zhao_raster_blend_prod.a_i`. **The plumbing is finished. Only the faucet is
missing.**

**And the faucet cannot be built.** `cast_strength_i` has **no producer
anywhere**: LODSTATE's caster output (`:188-195`) carries no strength; a search
for `strength` across all of `fpga/rtl` returns only SURFACE.STAMP, the FIELD
adapter, rumble, tint, and a particle port already tied to zero; **no ABI command
carries one.** Verified independently here: `cast_strength_i` occurs as an input
on the block, as `strength_q`, and at `zhao_prod_top.sv:752` connected to
`u08_src[28 +: 8]` — **the pricing top's generic stimulus source, which is not a
producer.** Composing it would create a **tie-off**.

**Four blockers, walked port group by port group, not one.** Only `tap_*` is
clear. The chain `zhao_measure_governor` → `zhao_geom_lodstate` →
`zhao_forge_shadow` is three blocks and **none is composed**, with LODSTATE and
FORGE.SHADOW **mutually blocked** — they can only compose together. Route A
**deadlocks** at `zhao_geom_vattr.sv:553`, re-verified first-hand.

**THE REFUSAL THAT MATTERS MOST IS THE ONE IT DID NOT TAKE.** Closing
`tri_continuation_tail_i` from four constants — exactly as `tri_flat_request_i`
was closed — **would have dropped the register by one, with every gate green and
the silicon unchanged.** That is tie-off relocation, the campaign's first
prohibition, and it was available, cheap, and would have looked like progress in
every report this run produces. It was declined and the reason written down.

**And my brief's premise was wrong too:** it pointed the packet at R48/`ALPHA_C`,
and **`ALPHA_C` is not on the blend's path and never was** — it is a 32-bit fx16
into attribute slot 3, which has no interpolator and no lane. A packet
"implementing R89" by replacing that constant would have changed **nothing** while
appearing to close the contradiction.

**Three stale citations corrected**, each true when written: the six blend
instances are `zhao_raster_blend_prod`/`_fin` (the named wrapper is instantiated
**nowhere**); `zhao_geom_vattr.sv:474` is a **blank line**; `:490` is a
localparam while the real `done_o` is `:553` — **and that refusal survives the
correction and is stronger for it.** A near-miss was also recorded rather than
left to trap the next lane: `lad_gov_floor_o` is **not** a producer for
`rung_floor_i` — 3 bits against 2, read from the particle's own attribute record,
serving PART.LADDER's eight-rung ladder.

### D-FORGESHADOW-A — shadow strength gets a NAMED CONSTANT, not an ABI freeze

Accepted as recommended. Shadow strength has no producer and no ABI field, and
the honest treatment is the one R48 gave `ALPHA_C` and that
`zhao_geom_lodstate.sv:180-187` **already prescribes for the sibling quantity**:
a named, editable constant at composition. Under `CLAUDE.md` rule 6 that is a
knob, not a stub — nothing is being hidden, and the seam is named for when a
producer exists. Freezing an ABI field for a value nobody produces would be the
worse move.

### D-FORGESHADOW-B — ACCEPTED: schedule no further FORGE.SHADOW wiring packet

*"Leaving it costs 1 on the register; composing it wrong costs a deadlock behind
a closed gap."* That is the correct trade and it is now a standing instruction.

FORGE.SHADOW is a **subsystem**: LODSTATE and SHADOW together, ladderbank as a
sixth adapter requester, the governor, Route B, and the client-A widening — the
last of which **re-authors a ratified law**. It is not a wiring job and will not
be closed by one.

## R134 — R130's PROCEDURAL FIX WORKED, ONE PACKET AFTER IT WAS WRITTEN

Worth recording because the failures get recorded and the corrections usually do
not.

R130 named my repeated error — converting a lane's closing recommendation into a
work order without re-asking — and prescribed the fix: **a brief that exists
because of another lane's recommendation must instruct the new lane to verify
the recommendation itself first, and to stop and report if it does not hold.**

I put that instruction into FORGESHADOW's brief. **It followed it, found the
premise false, and returned a proof instead of an implementation.** The cost was
one investigation rather than one wasted build plus a wrong composition that
every gate would have passed.

Four times today a lane has corrected a ruling of mine — R98, R100, R104, and now
R132. **That ratio is the system working**, and it only works because the briefs
say to check.

## R135 — THE REMAINING 21 ARE NOT ALL WIRING, AND THE OWNER SHOULD KNOW BEFORE THE NEXT SCOPING DECISION

This is the strategic finding of the day and it is owed upward rather than
decided here.

The standing goal is to drive the mandatory gap count to zero and then fit. The
run has taken it 61 → 21. **But the remaining eleven disconnected blocks are not
eleven wiring jobs.** Measured, across today's lanes:

* **FORGE.SHADOW + GEOM.LODSTATE are MUTUALLY blocked** and compose only
  together, as part of a subsystem that includes the governor, ladderbank, Route
  B, and a ratified-law re-authoring (R133).
* **MEASURE.GOVERNOR is blocked at BOTH ends**, and **no core boundary port
  exists that a governor output could replace** — composing it *creates* gaps
  (R118).
* **GEOM.PARAMBUF's arena is unmapped in both directions**, and needs an
  allocator, a quota seal and a frame-fault path that are all unbuilt (R131).
* **Four terrain blocks are behind R65**, an owner art call, and six lanes have
  now closed zero of them.
* **GEOM.WARP is waiting on nine FIELD prerequisites**, eight of which the
  owner's directive supplies and one of which (P5) it supplies by a weaker
  mechanism than the prerequisite asked for (R103, and the FIELD plan §4).

**So "drive the register to zero" is, for a real fraction of what remains, a
request to BUILD SUBSYSTEMS, not to connect existing ones.** That is not a reason
to stop — it is a reason to say so before the next scoping decision is made on
the assumption that twenty-one wires remain.

**What I am NOT doing:** lowering the bar, redefining a gap, or closing anything
by relocating a tie-off. FORGESHADOW had that move available today, cheap and
green, and declined it. The number stays honest even when that makes it move
slowly.

**The recommendation:** the fit preconditions are otherwise all met —
`superseded check: 71 production roots CLEAN`, and six of the seven items in
`reports/FIT-PLAN-AT-ZERO.md` are satisfied. It is worth the owner deciding
whether to **spend the remaining effort on the FIELD subsystem the directive
commissioned** (which closes I34 and GEOM.WARP, the two the plan can actually
reach) **and then fit with a declared, itemised remainder**, rather than holding
the fit behind blocks that are subsystem builds gated on art calls and unbuilt
allocators. That is the owner's call, not mine; both paths are honest, and the
second one requires the remainder to be **named in the receipt**, not rounded
away.

## R136 — `fld_sat_o` DESCRIBED THE SCALAR ALU ALONE: seven services' saturation terminated in `*_unused`

**2026-09-20, F1.** Not in its brief, not in the owner's directive, and it is a
shipping defect.

**All seven v3 services computed per-lane saturation. `zhao_field_v3_svcpath`
terminated every one of them in an `*_unused` wire, and carried no status port on
the module at all.** So `fld_sat_o` at the console boundary reported the scalar
ALU and nothing else.

**What that means in operation: any long op could saturate in every point, and
the ledger would read clean.** The counter is not wrong — it is structurally
incapable of seeing six-sevenths of what it appears to report, and the direction
of the error is, once again, flattering.

Now connected, and **masked to live lanes in the dispatcher where `s_used_r`
lives** — the mask matters, because an unmasked reduction would have replaced one
wrong answer with another by letting padding lanes vote.

This is the same family as R101 (ANY rather than ALL), R110 (`check_counters.py`
one-directional), R113 (a dead stimulus passing), R118 (a governor with no
boundary port), R123 (a mutant that fires zero times) and R129 (a checker grading
prose). **Seven distinct instruments today, all reading green, all structurally
unable to see the fault they exist for.** That is no longer a run of bad luck; it
is the dominant defect mode of this console, and it should shape what gets built
next: **every new observation port needs a demonstration that it can see the
thing, not merely that it compiles.**

### And the lane committed the same defect in its own tooling, one hour later

Recorded in its own words because it is the most useful thing in the report:

> *"my own smoke watcher grepped only `SMOKE: PASS` and went silent through two
> controls that print `MUTANT PASS` / `BAD_VERTEX PASS`, reporting '2 of 5' while
> four had passed. Same broken-instrument shape I'd just repaired in
> `fld_sat_o`, committed by me in the tooling an hour later."*

**Knowing the defect class does not confer immunity to it.** The lane had just
finished repairing a filter that could not see six of seven cases, and then wrote
a filter that could not see two of five. This is exactly the coordinator's own
day — I documented the `$(basename …)` exit-code trap and then reproduced it an
hour later (R121).

**So the mitigation cannot be vigilance.** It has to be structural: a watcher
that counts *completions* rather than matching a success string, and a gate that
asserts the expected number of verdicts rather than the presence of one.

## R137 — OP_RCP and OP_RING DECLINED WITH EXECUTED EVIDENCE, and one piece of work unblocks both

F1 was asked to add canonical RCP (`0x17`) and a bounded varying-radius RING
(`0x21`). It added neither, and **proved the refusal by driver rather than
asserting it**: FT014's census shows both *refused*, 15 opcodes routed and 4
refused.

* **RCP is blocked by the directive's own line 2269** — status must work before
  an opcode is advertised. And `rcp0` **has no destination**:
  `zhao_field_host.sv:402` is `[2:0] sat_o`, too narrow to carry it. **FT040
  cannot pass for any route until that bit exists.**
* **RING is blocked by RCP**, re-asked rather than inherited, and the blocker
  still holds because `zfield::prepare()` computes the two reciprocals in
  software.

**One piece of work unblocks both**, and it is H1's: widen `sat_o`, with C1
carrying it to the boundary. Recorded in the Wave 2 notes so it is not
rediscovered.

**This is the correct shape for a refusal**: the advertised-but-unrouted opcodes
are now *demonstrated* unrouted by a census with a driver behind it, instead of
being listed in a table that agrees with itself.

### What changed shape, for the packets that follow

* `zhao_field_alu_vec` gained `lane_live_i` and three per-lane status outputs —
  one instantiation site tree-wide (`exec:731`), connected.
* `zhao_field_v3_svcpath` gained four status outputs; `zhao_field_v3_dispatch`
  gained three status inputs and four outputs. One instantiation site each.
* **`zhao_field_v3_engine`'s ports are UNCHANGED**, which is why the host,
  `zhao_prod_top` and `zhao_console_board` were untouched and all three
  generators stayed fresh without regeneration.
* **`zhao_field_ops_pkg.sv` changed COMMENTS ONLY** — verified by diffing comment
  lines out, with no localparam, function or width moved, so S1's C++/SV
  cross-check is unaffected. That verification is exactly what R129 says to
  demand, applied by the lane to its own change.

## R138 — THIRD INSTANCE of the prose-grading defect, in the reference-model auditor itself

**2026-09-20, coordinator, found by the sweep R129 said was owed.**

R129 recorded that a checker resolving a NAME against raw FILE TEXT will be
satisfied by the documentation of the thing's absence, and that it had been
found twice — `uncashed_cheques.py` CHECK 5, and a Python render test asserting
a formal assertion R32 had deleted, which passed because the token survived in a
comment in the file it grades.

**`tools/budget/refmodel_liveness.py` had it too**, and it is the worst possible
host for it: the tool's entire job is deciding which `reference_model:` symbols
the oracle actually contains. `resolve()` searched `p.read_text()` raw, so **a
comment reading "zref::X was removed in R32; do not resurrect it" made zref::X
resolve as PRESENT.**

**Demonstrated before repairing.** A blob containing the name only inside a `//`
comment returned a hit; the same name absent entirely returned none.

**And its existing self-test could never have caught this.** The tool carries a
CANARY — a symbol that must resolve, on the stated and correct principle that
*"this tool reads LOW when broken … so it REFUSES TO RUN unless it can first
resolve a symbol known to exist."* That guard is sound and it is **one-directional**:
it proves the search finds what exists, never that it **rejects prose**, because
the canary resolves either way. **This is R110's shape — a check that only looks
one way down its own comparison — sitting inside the self-test of a tool built
to catch exactly this family.**

Repaired with `strip_cxx_comments()` applied in the loader, plus
`_prose_self_test()` wired into `audit()` beside the canary: five cases, of which
case 4 (a genuine declaration MUST still resolve) is the negative control
without which a checker that rejected everything would pass the other three.

**THE REPAIR CHANGES NO VERDICT TODAY** — 89 resolve, 6 unresolved, before and
after. Nothing in the tree was resolving on prose. Stated plainly rather than
dressed up as a catch: the value is prospective, and closing a mode that hides
evidence is worth doing even when it catches nothing on the day.

**Two independent tools now agree the unresolved set is exactly six** —
`zref::MeasureHistogram`, `zref::PostComposite` and four PART.* rows — which is
worth more than either count alone. Each still needs the per-row call R94
defined: **name the law that exists, or remove the key and say why the block has
no reference model. Inventing a plausible symbol is the same defect with a
better name.**

**The sweep also found `tools/rtl/check_guard_verdict.py` already strips
comments** — so this is not universal, and the tools that got it right are worth
noting alongside the ones that did not.

## R139 — the six unresolved `reference_model:` rows: the NAMESPACES exist, the LAWS do not

**2026-09-20, coordinator.** R94 required a per-row call on every
`reference_model:` that resolves to nothing. Two independent tools now agree the
set is exactly six. This is the investigation that makes those six calls cheap;
it is deliberately NOT the calls themselves.

**What `reference/` actually holds:**

* `zref::part::` (in `zref_particle.hpp`, `zref_particle_soft.hpp`) — structs
  `Particle`, `PolyExpand`, `ExpandedVertex`, `LadderOut`, `SoftRect`; functions
  `expand_polygon`, `ladder_raw`, `ladder_step`, `ladder_want`, `soft_rect`,
  `particle_pack`, `particle_unpack`, `particle_angle16`, `particle_radius`.
* `zref::post::` (in `zref_post.hpp`) — `grade_*`, `glow_*`, `echo`, `look`,
  `capture_addr`, `chunk_of`, `apply_set_post`, `disp_to_pixels`, and more.

**So the oracle is not missing wholesale — it covers DIFFERENT BLOCKS.**
PART.LADDER, PART.EXPAND and PART.SOFT already resolve, against
`zref::part::ladder_want`, `::expand_polygon` and `::soft_rect` respectively.
`zref::post::` likewise serves the grade and echo paths.

**The six that do not resolve name laws that are genuinely absent, not renamed:**

| row | declares | nearest real thing |
|---|---|---|
| PART.COLLIDE | `zref::ParticleCollide` | nothing — no collision law in `zref::part::` |
| PART.SPAWN | `zref::ParticleSpawn` | nothing — no spawn law |
| PART.STATE | `zref::ParticleState` | `particle_pack`/`particle_unpack` exist, but that is a FORMAT, not a state law — **check before assuming either way** |
| PART.UPDATE | `zref::ParticleUpdate` | nothing — no integration law |
| POST.COMPOSITE | `zref::PostComposite` | `zref::post::` has grade/glow/echo/capture, **no composite** |
| MEASURE.HISTOGRAM | `zref::MeasureHistogram` | not examined here |

**The recommendation, and why it is not a decision.** R94's rule is *name the law
that exists, or REMOVE the key and say in the row why the block has no reference
model — inventing a plausible symbol is the same defect with a better name.* On
this evidence five of the six look like removals with a stated reason, and
**PART.STATE is the one that could go either way**, because `particle_pack` /
`particle_unpack` may or may not BE its ratified law rather than merely adjacent
to it. That distinction needs the contract in hand, and getting it wrong in the
"name a plausible symbol" direction is precisely the defect R94 exists to
prevent.

**So this goes to a small packet with the contracts open, not to a coordinator
guess.** The measurement above is the expensive half and it is done: whoever
takes it does not have to re-derive which namespaces exist.

**One caution for that packet.** A `reference_model` naming a symbol that does
not exist is not merely untidy — it buys **silent exemption from CHECK 3**, the
duplicate-ratified-law detector that caught the 66-DSP projector duplication,
because a name resolving to nothing can never collide with another block's. So
each removal must say why the block has no oracle, in the row, where the next
reader meets it.

## R140 — `check_counters.py` now looks BOTH ways, and fixing it exposed a regex that had never worked

**2026-09-20, coordinator. R110's owed repair, plus a defect found only because
I insisted the new check FIRE before trusting it.**

R110 recorded that this tool asked one question — *is every DECLARED counter
presented on a port?* — and was structurally blind to its mirror, *is every
PRESENTED counter declared?* Of ten census ports one lane named on a single
block, **it could see exactly one.**

**The reverse check is added, and it is a REPORT, not a gate.** Making it a gate
would put it red on arrival, and a gate that is red on arrival is one people
learn to skip — which `uncashed_cheques.py` says in as many words about its own
check 5, citing how the v1 FIELD datapath got composed.

### The defect underneath: `COUNTER_SHAPE` had no `re.M`

`counter_shaped_ports()` applies `COUNTER_SHAPE.finditer()` to a **whole file's
text**, and the pattern was compiled **without `re.MULTILINE`**. So `^` anchored
to offset 0 and it matched at most the first line of a file — **returning `[]`
for every real module.**

**The `--suggest` candidate list has therefore been silently empty for as long as
it has existed**, and nobody noticed, because *an empty suggestion list looks
exactly like having no suggestions to make.* That is today's dominant defect mode
once more, in a helper nobody thought to question.

**It was found because the new check's probe returned `[]` for all four
polarities — including the two that had to be non-empty.** Had I probed only the
"should be silent" case, the repair would have shipped as a permanent no-op with
a passing test beside it. **The tell was an instrument that was empty, not a tree
that was clean.**

### Narrowing: a counter COUNTS

Raw 32-bit-output shape gives **365 candidates across 70 blocks**, and the very
first row is `dma_bytes_consumed_o` — which `counter_shaped_ports`' own docstring
names as the canonical 32-bit output that is *not* a counter. 365 rows of mostly
noise is the "red on arrival" failure wearing a different hat.

So the reverse direction keys on the one property a counter cannot fake: it is
**incremented in its own module**, by `name <= name + 1` / `+ 32'd1` or by the
`_INC(...)` wrapper. That narrows it to **153 candidates across 35 blocks**, and
the rows are unmistakably real: `guard_violations`, `evictions_o`,
`leases_granted_o`, `bridge_errs_o`, `crc_fails_o`, `guard_denied_o`,
`hdr_ident_fails_o`. `dma_bytes_consumed_o` is correctly gone.

Probed in both polarities on a synthetic module carrying all three shapes: the
incremented port and the macro-incremented port are reported, the payload is
never reported, and declaring them silences it.

### THE NUMBER, and what it is not

**The ledger names 252 counters. There are 153 more presented on ports it never
names.** So roughly **38% of this console's counter surface is undeclared** — and
it was invisible in both of the ways that matter: the tool did not ask, and the
helper that would have suggested them was returning nothing.

**It is still a LOWER BOUND and still a list of questions, not defects.** A
counter incremented through an alias or inside a generate loop is missed, and
some rows will turn out to be payloads that happen to increment. Each row is
*either the ledger owes it a name, or it is not a counter and the row is noise* —
and that is written into the output, so the next reader is not handed a number
that looks like a verdict.

## R141 — THE ENTIRE gaps-to-zero RUN HAS HAD NO CI. The G0 defect recurred, on a branch name

**2026-09-20, coordinator, found by following the fit guard's instruction to pick
up `reports/DOCKET.md` instead of idling.**

`.github/workflows/ci.yml` triggers on `main`, `zixxtrixx-v8-closeout` and
`hw/**`. The branch every piece of this campaign lands on is
**`claude/ceiling-architecture-20260912`**, which matches none of them.
`gh run list --branch claude/ceiling-architecture-20260912` returns **nothing at
all** — not a failure, not a skip: the workflow has never run on it.

**So around twenty merges, the whole FIELD repair, the ABI regeneration with its
five golden captures, and every instrument repair recorded today have been gated
by local runs alone.** The local gates are real and they are thorough, but they
are the only ones, and nobody has been in a position to say so.

**The part that makes this worth a ruling is the comment sitting directly above
the branch list**, written five days ago by whoever last fixed exactly this:

> *"G0 (2026-09-05): main ALONE was wrong. Every hardware campaign lands on an
> integration branch — the texture-island work, the fit harness fixes and the D19
> docket all sit on `zixxtrixx-v8-closeout`, which never triggered this workflow.
> A green `main` badge said nothing about the branch the work was actually on,
> **which is the same failure as a gate that cannot fire: it reassures without
> checking.**"*

That diagnosis is exact, it is the thesis of this entire run, and **it aged out
within a fortnight** — because the fix was to NAME the branch. A list of names
goes stale every time the work moves; a pattern does not. The repair reassured
about the one branch it named while the campaign walked to the next one.

**This is the ninth instrument today that read green while structurally unable to
see its subject**, and the largest in scope: it was not one counter or one
checker, it was *the entire continuous-integration system* with respect to *all
of this run's work*.

**Fixed by SHAPE, not by name:** `claude/**` added. Naming
`claude/ceiling-architecture-20260912` would merely reset the same clock.

**`gz/**` — the per-packet lanes — was considered and DELIBERATELY LEFT OUT**,
with the reason written into the file so it is not silently "corrected" later.
Those branches push often, three at a time, and a run here is 45–60 minutes;
adding them would queue dozens of concurrent runs behind a pipeline that is
currently **red on `main`**, buying noise rather than signal. The coordinator
gates every packet on the merged tree regardless, and that is the only tree that
has to be correct. It is a cost call, not a claim that lane coverage is
worthless, and the file says which.

**What this does NOT do, stated so the next green badge is not over-read:** CI is
currently **failing on `main`** (run `35509134755`, and the nightly schedule
too). Turning it on for this branch will very likely produce red runs
immediately. **That is information, not damage** — but it means the first runs on
`claude/**` must be read as *"what does CI think of this branch"*, not as a
regression introduced by enabling it. Docket D17's live residual is relevant: CI
pins cppcheck 2.19.0, this machine has 2.20.0, and D17's finding **does not
reproduce on 2.20.0 at all** — same command, same file, different answer. So a
local-versus-CI disagreement on that lane is expected and already documented.

## R142 — F-CLIFF-GOLDEN LANDED: **5,698 ALM, fit-minus-fit, on the target part**

**2026-09-20 18:20, `zhao_forge_cliff@golden-for-F-CLIFF1`, `FIT_RC=0`.**
Read against the four outcome bands committed to `FIT-PLAN-AT-ZERO.md` **before
the number existed**, so the interpretation could not be chosen after the fact.

### Provenance first, per the standing rule

`.sources.sha256` records `445a89ba…` and
`fpga/rtl/forge/zhao_forge_cliff.sv` hashes to `445a89ba…` at this commit. **The
receipt describes the file in the tree today.** Device `5CSEBA6U23I7`, Quartus
17.0.2 — **the same part, tool and stage as the candidate's**, which is the only
thing that makes a subtraction legitimate.

### The numbers

| | golden `zhao_forge_cliff` | candidate `zhao_forge_cliff_ram` | delta |
|---|---|---|---|
| **fitted ALM** | **6,674** (16% of device) | **976** (2%) | **−5,698** |
| fitted registers | 4,025 | 939 | −3,086 |
| DSP | 2 | 2 | — |
| RAM blocks | 14 | 15 | +1 |
| block memory bits | 119,808 | 120,964 | +1,156 |
| map comb ALUT | 8,149 | 1,326 | −6,823 |
| map registers | 3,875 | 826 | −3,049 |

**The band this lands in is "roughly 6,000–8,000 ALM → the map estimate was
sound; adopt, subject to R117's two named items; quote the delta as
fit-minus-fit."** So: **the swap saves 5,698 ALM and 3,086 registers, for one
extra RAM block and about 1.2k memory bits, with DSP unchanged.** That is
**13.6% of the device's 41,910 ALM.**

**And the old map-only row reproduced EXACTLY** — 8,149 ALUT, 3,875 registers,
119,808 memory bits, matching the recorded estimate to the digit. The fitted
6,674 sits ~13% under the map's 7,664 ALM figure, which is the normal direction
for a map estimate. **The estimate was honest; it simply was not a fit, and the
distinction mattered enough to spend fifteen minutes settling.**

### R117'S TWO BLOCKERS ARE BOTH NON-DIFFERENTIAL, and this is the finding I did not expect

I ruled adoption blocked on two items found in the candidate's receipt. **The
golden has both, in identical quantity:**

* **four `Warning (276020)`** RAM pass-through insertions — the golden has
  **exactly four as well**;
* **one inferred latch** — the golden has **exactly one as well**.

**So neither is introduced by the RAM candidate. Both are pre-existing
properties of this block in either implementation**, and R117 framed them as
costs of the swap when they are costs of the *design*. Adopting the candidate
does not add a latch or a pass-through; it removes 5,698 ALM and leaves those
untouched.

**R117 is amended accordingly:** the two items remain worth fixing, but they are
**not adoption blockers** and must not be quoted as the price of the swap. That
was a comparison made against one side only — the same one-sided-comparison
error this run has found nine times in instruments, here committed by me in a
ruling.

### The context, stated carefully so it is not over-read

The console's only composed fit reads **47,582 ALM against the 41,910 ceiling —
5,672 over.** This swap saves **5,698**.

**That near-coincidence must NOT be reported as "the swap closes the gap."**
That 47,582 came from a **dirty tree**, carrying a live metadata-swap defect,
**before this run's twenty-odd repairs**, and `zhao_block_fit.json`'s row for it
**does not contain FIELD at all** — its `.sources.sha256` lists exactly one field
file. Three independent reasons the denominator is wrong. What can honestly be
said is narrower and still large: **this is the biggest single measured ALM lever
found in the campaign, and it is worth about a seventh of the whole device.**

### What this fit does NOT settle

**It measures AREA.** It says nothing about whether the two implementations agree
functionally — that is `tests/forge/forge_cliff_ram_differential.cpp`'s job, and
Verilator's, not Quartus's. Declared in the plan before the run and repeated here
so no one reads a fitted number as a correctness result.

## R143 — THE PRE-FIT PRECONDITION IS UNMET AGAIN, and the register is RIGHT to say so

**2026-09-20, on merging H1.** `superseded check` went **0 → 2**:
`zhao_console_core` and `zhao_prod_top` both wire `zhao_field_host`, now
superseded by `zhao_field_host_v2`.

**This is not a regression in the gate. It is the gate working.** Verified
directly: `zhao_console_core.sv` references `zhao_field_host` **ten times** and
`zhao_field_host_v2` **zero** times. The console genuinely does compose a
superseded module right now, and R86 exists exactly to stop a console being
fitted in that state.

**So I am NOT teaching `completion_register.py` to honour the exception, and the
precondition I declared met four hours ago is UNMET again until C1 composes.**
Saying otherwise would be closing a gate by narrowing it — the campaign's first
prohibition, applied to a gate instead of to RTL.

**H1 did nothing wrong.** Building the new host without composing it is exactly
what the FIELD plan assigns: `zhao_console_core.sv` is C1's **serialised,
never-concurrent** act, the response bus changes width (`OUT_ORDINALS`, not
`OUT_LANES`) and gains three ports, so the composition, both adapters and both
generated tops must move together or the next fit finds a `PINMISSING`. H1 doing
that concurrently is the live-tree hazard this run has already paid for twice.

### TWO GATES NOW DISAGREE, and that is its own defect

* `check_console_inventory.py` → **RC 0.** It reads `version_exceptions` and
  honours H1's entry.
* `completion_register.py` → **RC 1.** Its R86 superseded check **does not read
  that section at all.**

**A reader can now quote whichever gate suits the answer they want.** That is
worse than either verdict alone, and it is the same family as a checker that
looks one way down its own comparison. **Recorded rather than resolved**, because
resolving it means choosing which gate is authoritative, and that is an owner-
level call about what `version_exceptions` is *for*:

* if it means *"this is fine, stop reporting it"* → the register should read it,
  and R86 gets a documented escape hatch;
* if it means *"this is a declared, time-boxed debt"* → the register is right to
  stay fatal and **`check_console_inventory.py` is the one that is too lenient.**

**My recommendation is the second.** An exception that silences the gate which
guards fitting is an exception that will one day be fitted through. A declared
debt that keeps the gate red is self-extinguishing: it cannot be forgotten,
because nothing ships past it.

### H1's own entry is the best-behaved version of this I have seen

It is written as **a work item with a deletion condition**, not a settlement:
*"WHAT REMOVES THIS ENTRY: C1 composing `zhao_field_host_v2` and regenerating
`gen_prod_top.py` and `gen_console_board.py`. Deleting these lines is part of that
packet's definition of done, and if this entry is still here after C1 has landed,
the composition did not happen and this file is the only thing that will say so."*

**That is how an exception should read.** It names who removes it, what removing
it requires, and what its continued presence proves. Deleting it is now part of
C1's definition of done.

**And it carries a parser trap worth keeping:** `version_exceptions` entries
**silently register as NOTHING if nested**, because that section's parser matches
`^  (\S+):\s*(.+)$` — the reason must be on the SAME LINE — while the `modules:`
section directly above wants a nested `why:` block. **Two sections of one file,
two incompatible shapes, and the wrong one fails silently in the reassuring
direction**: the gate goes on failing while the file looks as though it carries
an exception.

## R144 — I CITED TWO RULINGS THAT DID NOT EXIST IN THE LANE'S BASE

H1 reported that **R136 and R137 do not exist**, having searched the rulings
file, all of `reports/`, and every `.md`. It was right about its base: `1a021945`
ends at **R132**.

They exist now — the file runs to R142 with no duplicates, verified — but I wrote
them into H1's brief **as binding** before they were in the commit the lane would
branch from. **I briefed from my working tree's future.**

**Third instance of this family today**, each in a different direction:
* **R106** — me citing an *unmerged branch* as tree state;
* **R128** — a packet citing *its own base* as current;
* **this** — me citing *my own uncommitted rulings* as though a lane could read them.

The common root is that **a citation is a claim about a specific tree, and none
of us has been dating them.** H1 did the right thing and the thing that makes
this cheap: it **verified the underlying facts directly** rather than trusting the
numbers, so the work is sound and only the provenance was wrong.

**The fix is the same one R130 prescribed and it needs extending to citations:**
a brief must cite rulings by *content* as well as number, or state the commit
they landed in. A bare number is unverifiable by the reader and looks
authoritative — which is precisely how R117 laundered an inherited claim.

## R145 — `rcp0` is FOUR unassigned files, not one, and the lane that owned them has closed

**2026-09-20, H1.** I wrote into two briefs that *"one piece of work unblocks
both"* OP_RCP and OP_RING — the `sat_o` widening. **That understated it, and H1
measured the difference rather than accepting the framing.**

`rcp0` has **no port at all** on `svcpath`, `dispatch`, `core` or `engine`. It
stops dead at `zhao_field_v3_svcpath.sv:437` as `nm_rcp0_unconsumed`. So the
chain is **four files**, not one widened field.

**H1 built and proved the DESTINATION** — `num_status_o[3]`, demonstrated in both
polarities and deliberately **not folded into saturation**, because a reciprocal-
by-zero and an arithmetic saturation are different facts and merging them is the
mismatched-quantity error this run keeps finding.

**But the four fabric files are F1's territory, and F1 has closed.** So this is
now **unassigned work sitting between two finished packets** — precisely the kind
of gap that a run loses when both lanes report success and neither owns the seam.
H1 left the recipe in its FINDINGS: it mirrors `sat_rescale` exactly, and the
ports are outputs.

**Routed to C1**, which already owns the composition edits and both generated
tops, so the port chain lands in the same serialised act rather than as a fifth
concurrent lane touching `zhao_console_core.sv`.

**Until it lands, FT040 cannot pass for any route** and OP_RCP/OP_RING stay
correctly unrouted — proven, not asserted, by F1's driver-backed census (15
opcodes routed, 4 refused).

## R146 — a `6'(64) == 0` bound that was CONSTANT-FALSE, and would have silently killed FH06

H1 found a width-truncated comparison that can never be true: `6'(64)` is `0` in
six bits, so the guard it forms is dead. **Had it shipped, it would have disabled
FH06 — uniform outputs as results — entirely and silently**, which is the whole
half of the host that L1 measured to be non-optional: every shipped Earth program
has uniform outputs no window mask can observe.

**It is the oracle's own `7'(128)` defect with the polarity reversed.** The same
truncation mistake, in the same family of code, expressed the other way round —
which means this is a *pattern in this codebase*, not a one-off slip, and a
width-cast comparison against a power of two deserves a look everywhere it
appears.

**A constant-false guard is the exact mirror of today's dominant defect.** Nine
instruments were found reading green while unable to see their fault; this is a
guard that reads green because it can never fire *at all*. Both are silent, both
are flattering, and both are invisible to any test that only exercises the
passing path.

## R147 — the `version_exceptions` PARSER TRAP, recorded because it fails in the reassuring direction

Two sections of `design/console_inventory.yml` want **incompatible shapes**:

* `modules:` wants a **nested `why:` block**;
* `version_exceptions:` wants the reason **on the SAME LINE as the name** — its
  parser at `check_console_inventory.py:153` matches `^  (\S+):\s*(.+)$`.

**Write the nested form in the exceptions section and the entry registers as
NOTHING.** No error, no warning. The gate goes on failing while the file looks as
though it carries an exception — so the reader concludes the gate is broken, and
the truth is that their entry was never read.

**This is a trap that only fires in one direction**, and it is the flattering one
for the file and the punishing one for the person: the document *appears* to have
been updated. H1 wrote the shape requirement into the entry itself, which is the
right place — beside the thing that must obey it, not in a tool's docstring
nobody opens.

## R148 — I MISQUOTED THE OWNER'S DIRECTIVE INTO ITS OPPOSITE, in six briefs

**2026-09-20, found by D1, and this is the worst thing I have done in this run.**

The directive, line 2244, verbatim:

> *"This is a real shared correctness commit **even if** the mandatory gap count
> stays 21 or changes on unrelated work. Do not wait for Warp to exist before
> fixing it. Do not claim the whole shared service repaired when only this
> commit is done."*

**That is a CONCESSION: the work counts even if the number does not move.**

What I wrote into six packet briefs — S1, L1, F1, D1, H1, A1 — citing it as
§20.2:

> *"Your packet is not expected to move the register. Reporting '21 → 21' is a
> correct outcome."*

**That is an INSTRUCTION NOT TO TRY.** The owner said the work is valuable
regardless of the number; I told six lanes the number was not their problem.
Those are not the same sentence and the difference is the whole campaign: the
standing goal is *drive the mandatory gap count to ZERO*, and I handed out
permission to leave it alone.

**Every one of the six reported 21 → 21.** I cannot claim that phrasing caused
it — Wave 1 and Wave 2 are dependency work by design, and each lane named real
blockers rather than shrugging. But **I cannot claim it did not, either**, and
that is precisely the problem with putting a false permission in a brief: it
removes the evidence that would settle the question. A lane that considered a
close and dropped it would have left no trace.

**Compounding it: I attributed the paraphrase to a section number.** "§20.2
says" is checkable, which makes it *more* trusted, not less — the same mechanism
as R117, where a ruling number laundered an inherited claim, and R144, where I
cited two rulings that did not exist in the lane's base. **Three forms of the
same error in one day: a number that makes an unverified claim look
authoritative.**

D1 caught it by doing what the briefs ask lanes to do and what I did not do
myself: it went and read the line.

### The correction, for every remaining brief

Quote the directive **verbatim** and let it mean what it says:

> *"This is a real shared correctness commit even if the mandatory gap count
> stays 21."*

And add the half I had been supplying in the wrong direction: **if a gap is
genuinely closeable within your file set, CLOSE IT — and if it is not, name the
blocker.** "The register did not move" remains an honest report; it was never
supposed to be a target.

**This does not retroactively devalue Wave 1 or Wave 2.** Their refusals were
specific and measured — FORGESHADOW walked five port groups, ENGINE1 traced a
chain blocked at both ends, H1 and D1 each named exactly what C1 must carry.
None of them shrugged. But the permission I gave them should not have been
there, and the remaining briefs will not carry it.

## R149 — S1's GENERATED SV PACKAGE HAS NEVER BEEN LINTED OR ELABORATED BY ANYTHING

D1, while consuming it. `fpga/rtl/field/generated/zhao_field_host_image_pkg.sv`
is emitted by S1's generator and **nothing verilates it, nothing elaborates it,
and no test includes it** — and it lacks the **27 `lint_off` pairs its sibling
generator emits**, which is the tell that it has never been through the linter
that would have demanded them.

**A generated file nothing checks is a file that is correct only by
construction**, and S1's own cross-check proves the C++ and SV sides *agree with
each other*, not that either is valid SystemVerilog. Two wrongs that agree are
exactly the cancelling-errors pattern `CLAUDE.md` names — and here the checker
S1 built cannot see it, because it compares the two generated sides rather than
either against a compiler.

Owed: put the package in a verilate closure so it is elaborated at least once,
and expect the missing `lint_off` pairs to surface immediately.

## R150 — D1 FOUND A DEFECT IN ITS OWN NEW COUNTER, AFTER PUSHING, AND FIXED IT

Recorded because the behaviour is the one this run is trying to make normal.

`hint_overrides_o` differenced against a field that **§10.2 defines as
reserved-zero**, so it was not measuring "the hardware overrode the software
hint" at all — it was measuring **"did not pick slot 0"**. And it was guarded by
a `>=` assertion **that could not fail.**

**A counter measuring the wrong quantity, protected by an assertion that cannot
fire.** Both halves of today's dominant defect in one object, authored by a lane
that had spent the day repairing exactly that. Replaced with
`pin_forced_victim_o`, differenced against a **pin-blind LRU** — a genuinely
different reference — and asserted to fire by exactly one.

Its other false-presence finds, each re-verified rather than inherited:
`prod_fit_sources.txt` is **orphaned** (the real list is `fit_targets.yml`, where
the doorbell sits in **two** blocks); the doorbell is instantiated in **two**
files, not the four a naive grep suggests; `counter_ids.lock` does not govern RTL
evidence ports; and **`zhao_crc32c_fold` already existed**, so its draft
bit-serial CRC would have been both a duplicate of ratified arithmetic and a
64-level timing defect.

## R151 — `zhao_prod_top` WAS FITTING A DIFFERENT MACHINE: the stamp adapter's defaults were live there

**2026-09-20, A1.** My brief flagged `zhao_field_stamp_adapter.sv:101` as
defaulting 12/4 while instantiated 13/7, and called it "a trap waiting for the
next composer". **It was not waiting. It had already fired.**

There are **two** instantiations:

* `zhao_console_core.sv:16203` passes **13/7** explicitly;
* **`zhao_prod_top.sv:579` passed NO override at all** and took the defaults.

So **the production fit top has been building a 384-bit client bus against the
console's 416** — and `zhao_prod_top` is the thing the whole-design census and
the production fit measure. Verified independently here: defaults still read
`IN_LANES = 12`, `OUT_LANES = 4` before A1's fix lands.

**This is R99's shape again, one level down.** R99 found the production top
*pricing two projectors the console does not contain*; this is the production
top *pricing the same adapter at the wrong width*. Both are the same failure:
**the top that gets measured is not the machine that gets composed**, and
nothing compared them.

Fixed both ways, which is right: defaults corrected to 13/7 **and** the composed
selection stated in `production_parameter_overrides`, the same mechanism and the
same reason as the existing `BUILD_HPS_N` row. A default and an override that
agree are cheap; a default nobody notices is what produced this.

**And 12/4 was not arbitrary — it is EARTH's record, copied.** Stamp's own arity
is 8/3. So the wrong number had a plausible origin, which is exactly why it
survived: it looked like it came from somewhere.

## R152 — R40's SUBTRAHEND WAS READ FROM LIVE PINS, and the guard beside it could not have caught that

**A1, in the flow adapter, and it is a live shipping defect.**

R40's law is `sat_s11((v' - v) >> 8)`. The adapter read **`v` from the live pins
at response time** rather than capturing it with the request — so a record that
moved mid-flight computed **`(v' of A) − (v of B)`**: two different records'
velocities subtracted from each other, silently, producing a plausible number.

**The existing guard could not prevent it, and the reason is exactly
`CLAUDE.md`'s two-operand law**: it samples one state *after* the wrong value has
already been latched. A detector downstream of the corruption cannot see the
corruption. That law was written after a metadata bank shipped a record-swapping
defect with a live identity counter beside it reading zero, and this is the same
structure in a different subsystem.

Repaired with **one capture latch**. Test case 8 asserts **7, not 517** — a
discriminating number rather than a pass/fail. Cost **+416 flops, declared**
rather than discovered later.

## R153 — FH26's TWO BINDINGS, DISCRIMINATED BY A SINGLE VALUE

The brief demanded that A1 *"name the case that discriminated them"*, because two
bindings that share a test are one binding with two names. It did:

**Same response, unit strength fx16 1.0 (`0x0001_0000`): `LEGACY` delivers 0
(no brush), `CANONICAL` delivers 65535 (full brush)** — because the legacy bridge
takes the low sixteen bits. A second discriminator on the input side: texel 0
offers `R0 = 0` against `R0 = 512`.

Two **separately elaborated** instances, and case D asserts that a full
4,096-record legacy walk moves **no** canonical counter **and the converse**. That
converse is the half usually skipped, and it is the half that proves the two are
not quietly the same object.

**And `STAMP_BINDING` has no safe default: omission is an elaboration refusal**,
with a committed positive control — written as a *wrapper*, so it cannot go
stale the way a copied mutant does — firing it with the exact text at `:319`,
and the "did not fire" path never reached. **Lint says nothing about it**, as
`CLAUDE.md` predicts for anything inside an `initial` block.

That is FH26 done properly: legacy is a **named mode**, not a value a capsule can
fall into by omission — which is the trap R111 recorded, where `mask == 0` is the
only case that occurs and a plan writer who omits it silently restores the R101
defect.

## R154 — A1 REVERTED ITS OWN PLAN BECAUSE IT WOULD HAVE COST A GAP

Worth recording as the behaviour, not the outcome. A1 had planned two new console
boundary outputs and **dropped them**, because an output nothing reads is a
tie-off in waiting — it would have *increased* the register while looking like
progress.

That is the same judgement FORGESHADOW made in refusing to close
`tri_continuation_tail_i` from four constants, and ENGINE1 made in refusing to
compose a block whose chain was blocked at both ends. **Three lanes independently
declining the flattering move on the same day**, without being asked in the
moment. The briefs carry rule 1; the lanes are applying it unprompted.

**Stale claims it also found**, each re-verified: plan §F2's Formation "(11)" is
already fixed at `field-ir.md:525`; `GEOM.WARP.md:317` said *"there is no Warp
adapter"* and **it amended that row**; `:319` still says P4 is ABSENT although
R101 merged it; and `ops.yml:27` contradicts `blocks.yml:1142`.

**Including one of its own**: it had claimed appending a manifest row would avoid
renumbering — **`gen_prod_top.py` sorts, so it does not** — measured both ways
and corrected its own note. A lane correcting its own published claim, in the
same report, is the standard this run has been trying to set.

## R155 — THREE PIECES OF WORK HAVE NOW FALLEN BETWEEN CLOSED PACKETS. That is my decomposition, not their execution

**2026-09-20, third instance in one day, and the pattern is mine.**

1. **`rcp0`'s port chain (R145).** Four fabric files — `svcpath`, `dispatch`,
   `core`, `engine`. They were F1's; **F1 closed**; H1 built the destination and
   could not reach the chain. Routed to C1 after the fact.
2. **§13.7's TerrainField producer, in `zhao_cmd_exec.sv`.** **No Wave-3 packet
   owns that file**, so E1 cannot close I34 no matter how well it does its own
   work. This is the *hard* blocker E1 reports, and it is a hole in the file-set
   allocation I wrote.
3. **`reference/src/zfield/zfield_plan.cpp:23`.** L1's territory; **L1 closed**;
   E1 found a live defect there and correctly did not reach in.

**Each lane behaved correctly. Each named the seam instead of crossing it.** The
failure is that I assigned file sets by *subsystem* and the work is shaped by
*data path* — so every place a value crosses a subsystem boundary is a file
nobody owns, and it only becomes visible when a lane runs out of road.

**The fix is procedural and cheap: before launching a wave, list every file the
wave's acceptance criteria require to change, and check each has exactly one
owner.** I listed what each packet owned; I never listed what the *work* needed.
Those are different lists, and the difference is where three items fell.

**A fourth is latent and worth naming now:** `spec/commands.zidl` is owned by a
packet that has closed, and W1 may need `DrawWarpedForm`. It was told to ask
rather than spend a second hour of capture regeneration on its own authority —
which is the right instruction, but it is the same shape one step ahead.

## R156 — `zfield_plan.cpp`'s `kGroups = 273` IS NOT A REPORT, IT IS A CLASSIFIER

E1, and this one ships wrong behaviour rather than a wrong number on a page.

`reference/src/zfield/zfield_plan.cpp:23` carries `kGroups = 273`, and it feeds
`hot = bind <= 6000`. With the **correct 297**, a program taking up to **6,527
clocks per association is classified HOT while being over its deadline.**

**So the 273/297 correction was never cosmetic.** Everywhere else it was a count
in a test or a document; here it is the denominator of a *scheduling decision*,
and the stale value makes the classifier optimistic — **flattering direction,
again**. A program that should be refused gets admitted.

**E1 correctly did not reach into L1's file** and handed it over with evidence.
It is now unowned (R155) and goes to the next packet with that file.

**And the correction was in FOUR places, not the one my brief named.** I wrote
"`field_v3_earth_directed.cpp:145`" as though that were the extent of it. E1
found all four. A brief that names one site of a multi-site change invites
exactly the partial fix this run keeps finding.

## R157 — THE PROBES WERE HIDDEN BY **TWO** SETTLED LEDGERS, NOT ONE

C5 recorded that `console_inventory.yml` marked both Earth probes
`disposition: instrument`, and that `instrument` is a **settled** disposition, so
`uncashed_cheques.py` could never flag them — the inventory suppressing the very
cheque R44 wrote.

**E1 found a second one: `prod_manifest.yml` marks them `probe`, also settled.**
So the promotion R44 ordered was invisible in **two independent ledgers at
once**, and fixing either alone would have left the other silencing the tool.

**Two registries, two settled dispositions, one silence.** A single suppressed
signal is an oversight; the same signal suppressed twice in two files is a
*structure* — and it is the ledger-side form of the run's dominant defect, where
the check cannot see the thing because something upstream declared the question
closed.

E1 proved the promotion itself a **no-op by measurement** — comment-stripped
bodies byte-identical after name substitution — which is the right evidence for a
move: not "it still compiles", but "it is the same file".

## R158 — "EVERY STATEMENT WAS TRUE; EVERY CITATION WAS WRONG"

E1's own summary of three references in my brief and in the FIELD plan:

* C5's `console_inventory.yml:241,253` are **two unrelated probes** — the real
  rows are **244 and 256**;
* both `zhao_console_core.sv` line references were **stale**.

**The substance held every time and the pointers did not.** That is the same
failure as R144 (citing rulings absent from a lane's base) and R148 (quoting the
owner's directive into its opposite by paraphrase), and E1 has now given it the
cleanest possible name.

**Line numbers in this tree have a half-life of hours.** Today alone: S1
corrected three of mine, F1 corrected three more, FORGESHADOW corrected three
citations in production RTL, and now E1 corrects three more. **The rule from here
is to cite by SYMBOL or by quoted text, and use a line number only as a hint that
is expected to rot** — which is what the briefs already demand of the lanes and
what I have not been doing myself.

**One more thing E1 did that is worth copying:** it corrupted five files with a
PowerShell array-flattening bug, **caught it immediately from the tool's own
echo**, restored from the index, redid the edit, and **wrote the trap into its
findings for the next agent** rather than quietly fixing it. A mistake recorded
is worth more than a mistake avoided silently.

## R159 — **`completion_register.py` CANNOT SEE A TIED-OFF PORT.** The number this run steers by has a blind spot

**2026-09-20, C1, and this is the most important instrument finding of the
campaign.**

C1 was about to compose GEOM.WARP's client port. Its recorded blocker had
dissolved, and **composing it read as 21 → 20.** It re-measured instead of
banking the win, and found **ten-plus tie-offs behind it**: `v_attr_i` (no
attribute words on the palette→skin bundle), **all nine warp-descriptor ports**
(`warp_en` / `warp_slot` / `warp_par` / `warp_bound` — zero hits, and
`DrawWarpedForm 0x0304` unallocated in `commands.zidl`), and the entire `f_*`
port.

**The register counts DECLARED tie-offs** — it parses the core's own INCOMPLETE
comment block, which is hand-maintained. **So a tie-off nobody writes down is
invisible, and composing a block with undeclared tie-offs makes the count go
DOWN.**

That is the run's dominant defect mode arriving at **the primary metric itself**.
Every gate has been audited today; the *number* has not. And it fails in the
flattering direction by construction: the easiest way to make it drop is the one
thing the campaign forbids.

**Three consequences, and I am recording all of them rather than only the
comfortable one:**

1. **Every "register moved" claim in this run rests on the tie-offs being
   honestly declared.** I believe they are — lanes have consistently declared
   them, and five separate packets today refused a flattering close. But that is
   *trust in the lanes*, not a property of the tool, and it should be said in
   those words.
2. **The fix is not to make the register smarter.** A tool that inferred
   tie-offs would be guessing at intent; the INCOMPLETE block exists because a
   human states what is missing. The fix is a **rule**: if you create a tie-off,
   you declare it **in the same commit**, and a reviewer checks the block against
   the diff whenever a composition lands.
3. **C1 caught this only because it re-measured a blocker it expected to be
   gone.** W1 documented the trap first. Two lanes independently arriving at it
   is why it is a ruling and not a note.

**GEOM.WARP's remaining work is a COMMAND and a DESCRIPTOR PATH, not a
composition.** That is a different packet from the one the plan describes, and
the plan's §4 mapping — which credited the directive with eight of nine
prerequisites — was **my analysis, not a measurement**. It is now corrected by
one that is.

## R160 — the pre-fit blocker is CLEARED, properly this time

**`superseded check: 73 production roots CLEAN`, 2 → 0.** `zhao_field_host_v2`
is composed, and **all 44 lines of the `version_exceptions` entry are deleted** —
H1's own deletion condition met, exactly as it wrote it: *"if this entry is still
here after C1 has landed, the composition did not happen."*

R143 recorded that I would **not** silence the register to make this go away, and
that C1 composing v2 was the only honest clearance. **That is what happened.**
The gate went red, stayed red under a documented exception, and went green
because the thing it was guarding got built — which is what a gate is for.

Also landed: **R145's `rcp0` chain to `num_status_o[3]`**, and GEOM.WARP's **P1**
(the shared input pair 13→15) at three sites **C1 re-measured itself**, because
my brief's line numbers were **~340 lines stale** (R158 again, fourth lane).

**And one thing beyond the brief that was necessary:** the doorbell's load-kind
field went 2→3 bits. `host_v2` implements **eight** kinds, and kinds 4–7
(OUTMAP / ASSOC / INITPROOF / PREPARED) **had no producer at all** — so composing
without it *"would have shipped the ordinal machinery unreachable."* A packet
finding that its own brief under-specified the work, and saying so, is the
behaviour that makes these reports worth reading.

## R161 — R145 SAID FOUR FILES; IT IS THREE, AND THE FOURTH WOULD HAVE BEEN A TIE-OFF

I wrote that `rcp0`'s chain spans four files. **Measured: `v3_core` and
`v3_exec` have ZERO `rcp0` hits.** Adding a port there would have been **a
tie-off created in the act of closing a gap** — the precise move rule 1 forbids,
introduced by my own instruction.

Two further corrections from the same measurement: the engine has **two
parents**, and the oracle carries `.rcp0_o()` empty **with its reason stated**
rather than silently.

**And R151 RECURRED TWICE in this packet.** The generated top again took
`IN_LANES=12` defaults, and **A1's own override row still said 13** after C1
moved the real value to 15. So the production top has now been caught measuring
the wrong machine **three times in one day**, in three different parameters, each
time because a default and an override disagreed and nothing compared them. All
four FIELD width sites are now in `production_parameter_overrides`, **verified in
the generated output** rather than assumed.

## R162 — `mutant_copy_drift.py` IS BLIND TO WRAPPER PORT-LIST DRIFT, BY DESIGN

C1's changes drifted three committed controls. **All three were REGENERATED, not
patched** — the correct treatment, since a hand-patched copy is a copy of
something that no longer exists.

The instructive one: **the console-core wrapper failed LOUDLY at elaboration**,
naming all ten new ports. In C1's words — *"a wrapper can't drift in its body but
its port list can, and `mutant_copy_drift` is blind to that half by design."*

**It was caught by RUNNING the control, not by a gate. The static set was fully
green at the commit that broke it.**

That is the twelfth instrument today found unable to see its own subject, and it
is a reminder that `mutant_copy_drift` — which I have leaned on all day, and
which R121 taught me to run *after* the commit — answers one question only:
*is this copy older than what it copies?* It does not answer *does this copy still
elaborate?* Those are different questions and only the second is evidence.

## R163 — A `_v2` FILE THAT EXISTS BUT IS NOT COMPOSED **CREATES** A VIOLATION. The mirror of R159

**2026-09-20, E1, measured rather than reasoned.**

E1 did not build `zhao_terrain_patch_v2.sv`, and the reason is the finding: **a
file containing nothing but an empty module makes the register report
`composed zhao_terrain_patch superseded by zhao_terrain_patch_v2` across THREE
production roots.**

So the mere existence of a `_v2` name, with no content and no composition,
**manufactures a superseded violation** — because the supersession check keys on
the naming convention, not on whether anything was actually superseded.

**This is the exact mirror of R159.** There, composing a block with undeclared
tie-offs makes the gap count fall *wrongly low*. Here, creating a `_v2` file
makes the superseded count rise *wrongly high*. **Both are the metric responding
to a FILE-SYSTEM FACT rather than a DESIGN FACT**, and the two errors point in
opposite directions, which is why neither is obvious from inside a single packet.

**The rule E1 derived, and it is the right one: a `_v2` must be BORN IN THE
COMMIT THAT COMPOSES IT.** Not created early and wired later. That also explains,
retrospectively, why `zhao_field_host_v2` took the superseded count from 0 to 2
the moment H1 landed it and back to 0 only when C1 composed it — the same
mechanism, seen from the other end, and it cost a documented exception and two
rulings to pass through.

## R164 — THE DIRECTIVE'S SILENCE WAS NARROWER THAN I SAID, AND I REPEATED IT ALL DAY

I have said several times, including in briefs and commit messages, that the
owner's directive *"never mentions"* the existing Earth machinery. **E1 measured
it: that is true ONLY OF THE WALKER.**

* `zhao_probe_walk_earth` — genuinely unmentioned. The original finding stands.
* `zhao_probe_patch_acc` — **3 hits.** The directive does cite it.
* the corrected group count `297` — **7 hits.** The directive uses the right
  number.

**So the directive knew about the accumulator and the count, and not about the
walker.** That is a much more interesting and much more specific fact than "it
never mentions it", and my broader phrasing made the owner's work look less
careful than it is.

**I amplified an architect's finding without re-measuring its scope**, which is
the same error as R148 (paraphrasing a concession into a permission) and R158
(true statements with wrong citations) — a claim that is right at its core and
wrong at its edges, repeated until the edges look load-bearing.

**Two more corrections E1 made to my own brief:**
`tools/field/measure_earth_budget.cpp` and `tests/differential/field_walk_earth_directed.cpp`
**both already existed.** I listed them as things for E1 to own and create.

## R165 — TWO OF I34's RECORDED BLOCKERS HAVE EXPIRED, AND THE ENTRY DOES NOT KNOW

E1, reading the entry's own text at `zhao_console_core.sv:2814-3053`:

1. **"Promoting is an owner call and not a packet's"** — the architecture question
   the entry stopped on **twice** — is **answered by the owner's directive
   §13.1/§13.2, which POSTDATE the entry.** The blocker was real when written and
   the owner has since ruled on it.
2. **"Nothing publishes {handle→hash}"** — this **predates D1's `BIND_PROGRAM`**
   (`zhao_field_doorbell.sv:165, 215, 315`), which landed today. **Re-measure
   before re-quoting.**

**So I34 has been carrying two expired blockers and one live one.** The live one
is §13.7's TerrainField producer, which had no owner until CMDFIELD.

This is the seventeenth-plus instance of the pattern and the clearest statement
of it: **a refusal is a claim about a moment, and this tree moves fast enough
that a blocker written yesterday may be spent today.** The entry is the
authoritative definition — which is exactly why it has to be *re-read*, not
quoted from memory or from a brief that paraphrased it.

**And my own framing of I34 was wrong in the same way.** I told two packets its
hard blocker was a port change on `zhao_terrain_patch`. C1 read the entry and
found it names **four build items, none of which is the port change.** I have
messaged CMDFIELD to read the block and report what it actually requires rather
than building to fit my instruction — because work done to satisfy a wrong brief
is worse than no work.

## R166 — I BUILT A FALSE ALARM AND NEARLY SHIPPED IT AS A CAMPAIGN FINDING

**2026-09-20, me, answering R159. This is the day's most useful mistake because
it runs the OTHER way from every other one.**

R159 found that `completion_register.py` cannot see a tie-off nobody declared,
and said the fix is a rule plus a reviewer check. `tools/design/packet_h_tieoff_audit.py`
is exactly that reviewer check — already written, with the right philosophy
("deliberately dumb about SystemVerilog... it can only be wrong about text") —
and **its targets were two hardcoded Packet-H files. It had never been pointed at
`zhao_console_core.sv`.**

Pointed at the core, it reported **`0 declared, 19 silent`.** Nineteen
undeclared tie-offs in the console, days before the honest fit, in the file whose
INCOMPLETE block feeds the primary metric. That is a dramatic, campaign-relevant,
immediately quotable finding.

**Every one of the nineteen dissolved on reading.** Three separate defects, all
in my instrument, none in the design:

1. **It could see 18 of the core's 88 instantiations.** `INSTANCE` matched only
   `zhao_foo u_foo (`; **70 of the core's instantiations are parameterised**
   (`) u_foo (`). It would have printed a total over one fifth of the file that
   looked like a whole-file audit.
2. **The core declares with `REAL:`, not `TIE:` — 163 uses against 0.** The tool
   knew one dialect and the core speaks the other.
3. **The core writes one reason above a RUN of connections.** A walk-back that
   stops at the first non-comment line credits `ev_tile_i` and calls
   `ev_primitive_i` and `ev_pixel_i` silent. It also split `vp_x0_i` from the
   `vp_y0_i` on the very next line.

Calibrated: **7 declared, 1 reasoned, 10 covered by a group comment, 1 SILENT.**

**19 → 1, and the core turns out to be exemplary.** Its reasons are better than
any `TIE:` line would be: `ev_stage_i (8'd0)` is *"an IDENTITY, not data: it
names which of the charter's seven sources this port is, and there is nothing
for a producer to supply"*; `b_j_flags_i (16'd0)` is *"a passthrough nobody
reads would be worse than this zero, which says plainly that nothing on this
path wants them"*.

**THE LESSON, AND IT IS NOT THE ONE I HAVE BEEN WRITING ALL DAY.** Twelve times
this session I have found an instrument reading GREEN while structurally unable
to see its subject. This one read **RED** while structurally unable to see its
subject. **It is the same disease** — the instrument is not measuring the thing
— but it presents as *diligence*, and that makes it harder to stop, not easier:

* a false green gets believed because **nobody audits good news** (`CLAUDE.md`);
* a false red gets believed because **it looks like the tool doing its job**, and
  because acting on it feels like rigour.

Had I reported "19 undeclared tie-offs in the console core" I would have
commissioned a packet to chase nothing, cast doubt on a file that is doing the
right thing better than my tool knows how to check, and burned a lane on the eve
of the fit. **The cost of a false alarm here is a packet; the cost of believing
it in the report is the owner's trust in every other number I have quoted today.**

**And I nearly confirmed it with a broken cross-check of my own.** To test
whether the 19 were declared centrally, I grepped the INCOMPLETE block with
`sed -n '/INCOMPLETE -- TIED OFF/,/^module\|^endmodule/p'`. `endmodule` is at the
**end of the file**, so that range was lines 406–16,685 — essentially the whole
core, **including the very port-map lines being audited**. Every port "appeared
in the INCOMPLETE block" because it appeared in itself. I wrote, from that,
*"the lanes have been honest, which is what R159 hoped but could not prove"*.

**A reassuring conclusion from a range that covered its own input** — the exact
defect class this run is about, committed by me, while building the instrument
meant to catch it. The block really ends at 4,201. Measured properly, 16 of 18
were NOT named there — which sent me to read them, which is what found the truth.

**Rules from this:**

1. **Before quoting a tool pointed at a NEW file, measure whether it can SEE
   that file.** I counted 88 instantiations against 18 matches. That took one
   `grep -c` and it is the only reason the rest was caught.
2. **A tool firing on EVERYTHING is as broken as one firing on nothing.** "0
   declared" out of 19 is precision at an extreme, and `CLAUDE.md`'s law —
   *"a number that is exactly zero is a broken instrument until proven
   otherwise"* — applies to a zero in the GOOD column just as hard.
3. **Read three of the alarms by hand before reporting the total.** Each read
   cost under a minute; the first one (`vp_x0_i`'s `// REAL:`) already falsified
   the headline.
4. **A convention the tool does not know is not a defect in the file.** The core
   had a systematic, well-kept, better-than-required practice. The gap was in my
   instrument's vocabulary.

## R167 — THE AUDIT IS NOW COMMITTED, CALIBRATED AND SWEPT; ONE REAL FINDING

`packet_h_tieoff_audit.py` takes targets on the command line, knows both
markers, understands the group convention, and keeps the Packet-H ctest
**bit-identical at 39/45 declared, 0 silent** — verified by running the
pre-change version side by side, not asserted.

Its new self-tests are the ones that matter: the parameterised form **with the
old pattern kept in the file as a negative control** so the fix cannot silently
regress, a divider comment that must stay SILENT (the defence against becoming a
comment-grader, R105), and the group comment's bounded reach.

**The one real finding in the console core:** `zhao_console_core.sv:14824`,
`u_material_resolve.dir_valid_i (1'b1)` — the only literal in the file with no
reason anywhere near it. It is almost certainly correct (you never publish an
invalid directory entry, and `dir_we_i` beside it is gated on the publish), so
this is a one-line comment, not a design change. **Not made now: the C1 smoke is
reading the live tree**, and `CLAUDE.md` is explicit that editing RTL under a
running suite makes its answer worthless in both directions.

**A tree-wide sweep found 97 silent literals across 23 files** — the console core
being the cleanest at 1. `zhao_texture_island_v3_top` (23),
`zhao_raster_texture_v3_fit_top` (14) and `zhao_shell_top` (13) lead it.
**These are DOCKET CANDIDATES, not findings.** Every one of those files may have
its own dialect exactly as the core did, and R166 is four hours old. Quoting 97
as a defect count would be the same error at tree scale.

## R168 — A1's ADAPTER CAN BE WIRED TO THE WRONG HOST AND EVERY GATE PASSES

**2026-09-20, W1, and this is the most serious finding of the day because the
code it is about is ALREADY MERGED.**

`zhao_field_warp_adapter`'s host-side ports **match the OLD host name-for-name
and the NEW host in MEANING.** Wire it to the old host and it elaborates, runs,
and passes every gate — while reading **window positions as ordinals**.

> *"Nothing in either port list distinguishes correct from wrong."*

That is the ordinal-vs-window distinction this campaign has been circling all
day: `required_mask` is indexed by canonical output **ordinal**, `hdr_outreq` /
`window_mask` by contiguous capture-**window** position, and `OUTPUT_MAP` is the
translation between them. Two indexings with the same width and the same names.

**Why no existing test can see it:** a CONTIGUOUS program makes ordinal and
window position *identical*, so it is provably incapable of discriminating the
two wirings — **and every other Warp test uses a contiguous program.** W1's
SPARSE case (ordinal 5 living at R21, window lane 5 unwritten) is the only
stimulus in the tree that separates them.

This is `CLAUDE.md`'s **"a gate that cannot reach the state is not evidence about
the state"**, in its purest form yet. The 392 byte-identical paired records that
missed the metadata swap are the same shape: a real workload, honestly run,
structurally unable to enter the state where the defect lives.

**And it is worse than a blind checker, because the blindness is in the TYPE
SYSTEM.** A port list is the one artefact everyone trusts to catch a mis-wiring;
`PINMISSING` exists for exactly this. Here the names agree, the widths agree,
elaboration agrees, and only the *semantics* differ.

**W1's recommendation, which I am adopting as the requirement:** the adapter
must consume **`resp_present_o`**. Today it decides on `resp_status_i == 0`
alone. A status of zero cannot distinguish "this ordinal was not requested" from
"this ordinal was requested and came back zero" — which is the same
two-things-that-look-alike failure one level down.

**Actions, and the first is not optional:**

1. **The SPARSE case must become a committed, named test**, not a case inside
   W1's bench. It is the only discriminator that exists and it must survive the
   packet that wrote it.
2. **Add `resp_present_o` to the adapter's host interface** and decide on it.
3. **A1's adapter is already in the coordinator branch.** This is not a new-work
   item; it is a repair to composed code, and it must land before the fit.

## R169 — W1 REACHED R159 INDEPENDENTLY. THAT IS THREE

W1, unprompted, on `completion_register.py`:

> *"no structural tie-off scan -- its list is parsed from a hand-written comment
> block. A tied-off composition would read CONNECTED and drop the total to 20."*

C1 found this (R159), W1 found it independently, and I then answered it with the
tie-off audit (R166/R167). **Three arrivals at the same defect from three
directions in one day** is why it is a ruling and not a note — and it is the
strongest possible argument that the rule (declare a tie-off in the commit that
creates it) needs enforcing rather than merely writing down.

## R170 — RECLASSIFICATION CANCELS EXACTLY, AND W1 CHECKED THE TOOL BEFORE SAYING SO

GEOM.WARP is **BUILT**: `zhao_geom_warp.sv` and `zhao_field_warp_adapter.sv`,
2,468 directed checks, plus a committed three-module bench joining
`zhao_geom_warp` → `zhao_field_warp_adapter` → `zhao_field_host_v2` — **all three
real** — at `IN_LANES=15 / OUT_ORDINALS=6`, Warp on client 2 of a `CLIENTS=3`
host. 71 checks. `TRANSLATE` moved a vertex to (1111, 1778, 3333) on the real v3
engine.

**Register 21 → 21**, and the composition of that 21 changed: from
9 tie-offs + 11 disconnected + **1 unbuilt** to 9 + **12** + **0**.

W1 did not merely observe the total held — **it read the summing code and
confirmed the two terms cancel by construction** before writing the number down.
That is the difference between reporting a number and understanding it, and it
is what stops "the register did not move" being read as "the packet did nothing".

**It also refused the flattering move explicitly:** *"I did not tie its Field
port off to fake it."* That is the sixth lane today to decline a composition that
would have moved the register dishonestly.

## R171 — OWNER DECISION: `DrawWarpedForm 0x0304`, AND APPEND IT AT THE END

W1 did **not** touch `spec/commands.zidl`, correctly, and made the case instead:

* **W04 mandates the command; W06 forbids every alternative by name; W09 makes
  `DrawForm` disable Warp.**
* **Structurally**, `cmd_draw_*` has no field for a program handle, four
  parameters, a 3-word bound or an attribute resource.
* `0x0304` re-verified free by reading the file.

**Without it a composed GEOM.WARP sits permanently in its W09 bypass — present
and unreachable**, which is a tie-off wearing an opcode's clothes.

**Append it at the END.** Inserting beside `DrawForm` rewrites nine goldens, and
a golden rewrite is a change nobody can review.

### Three plan corrections, measured, and one deferral honestly renamed

W1 re-measured the repair plan's prerequisites rather than inheriting them:

* **P6** still `TABLES=2` — the plan said YES.
* **P8** still absent — the doorbell still sends everything to `D_LOAD`, no
  BIND/SEAL anywhere. The plan said YES.
* **P1** is three core sites, **not** "plus generated tops".
* **P5 is DEFERRED, NOT CLOSED.** The prepared bank is still one flat array;
  what was added is a generation *stamp*, which **detects staleness but does not
  isolate two eligible plans.** Renaming a deferral as a closure is how R159's
  flattering direction gets into a plan instead of a register.

**R103 is discharged on P9** — it claimed R91's fast path "does NOT exist
anywhere in the tree", and W1 built and FIRED it: **slow slot 92 clocks, fast
slot 60, delta exactly 32 = `REGS`**, bit-identical vertex. A discharged ruling
with a measured delta is the right way to close one.

**And it corrected a count that three files agreed on:** the reference check
count is **102, not 95** — wrong in three places that agreed with each other
**while none had run the binary.** Mutual agreement between documents is not
evidence; it is usually just copying.

## R172 — THE TIE-OFF AUDIT COULD NOT SEE A SIGNED LITERAL, AND THAT IS THE FLATTERING DIRECTION

**2026-09-20, hours after R166, in the same tool, found the way R166 says to
find things: BY READING THE OUTPUT BY HAND.**

`LITERAL` matched `16'd0` and did **not** match `18'sd0`. Every **signed**
literal in the tree was invisible to the audit.

It surfaced in `zhao_field_flow_adapter.sv`, which ties off **twelve** inputs of
`zhao_part_record` — a bidirectional codec this instance uses **decode-only**,
with `rec_o` empty and the whole instantiation wrapped in a
`PINCONNECTEMPTY` pragma. Six of the twelve are unsigned and were reported. Six
are `18'sd0` / `11'sd0` / `32'sd0` and **were not**, so the file read as half as
tied-off as it is.

**The direction is the point.** R166 was the tool crying wolf — over-reporting,
loud, and self-correcting because an alarm gets investigated. This is the same
tool **under**-reporting, silent, and it would never have corrected itself:
`CLAUDE.md`'s law is that a broken instrument lies in the direction that makes
the answer look better, smaller or simpler, and **nobody audits good news.**

**Both defects lived in the same file at the same time**, one in each direction.
That is worth saying plainly, because "the tool over-reports" had become a
reason to discount its numbers — and discounting them would have hidden this.

**Tree-wide, corrected: 128 silent literals across 26 files, against the 97
across 23 I docketed four hours ago. The audit was under-counting by a third**,
and I published the 97 as a docket item with confidence.

The fix keeps the **old pattern in the file as a negative control** — it must
still fail where the new one succeeds, so the two cannot silently converge and
leave the positive test proving nothing. The Packet-H ctest is bit-identical at
39/45 with 0 silent, and `zhao_console_core.sv` is unchanged at
`7 / 1 / 10 / 1`, which says the core has no signed tie-offs at all.

**The rule, and it is the one that actually found this:** when you do not trust
an instrument, **read its output by hand against the source.** I was checking the
six rows it DID report, to see whether they were real. They were benign — and
sitting six lines above them were six more the tool had never mentioned.

## R173 — A CANARY THAT CANNOT ENTER THE FAILING STATE IS NOT A CANARY

**2026-09-20, found by TERRCOMP, verified independently here. R172 in a second
tool on the same day.**

`tools/budget/duplicate_functions.py` allowed **one** word before the packed
range, so `function automatic logic signed [31:0] f(...)` — two words — never
matched. Measured over `fpga/rtl`: **524 function headers, 380 matched, 125
INVISIBLE**, and 196 headers carry `signed`.

**The consequence is fit-relevant and it lands days before an area-constrained
fit: duplicated names go 30 → 43.** Thirteen duplications were invisible, and
they are the *signed arithmetic* ones — precisely where duplicated logic costs
DSPs and ALMs. `sub_sat` is defined in **9 files**, `resc16` in **8**.

**But the regex is not the finding. The CANARY is.** This tool's own docstring
says it *"refuses to run unless it first finds a known-good example"* — and it
did that faithfully for its entire life. Its canary is `unit_mul`, declared
`function automatic logic [7:0] unit_mul(...)`: **unsigned, one type word.** It
resolved happily under the broken pattern, so the self-check **passed over the
exact blind spot it existed to guard.**

This is `CLAUDE.md`'s "a detector that has not been shown to FIRE has not been
tested", one level deeper: the detector *had* a self-test, the self-test *did*
fire, and it fired on a case that could not distinguish a working pattern from a
broken one. **A positive control must exercise the failure mode, not merely the
happy path.** Added `SIGNED_CANARY = "mw"` — `logic signed [MATW-1:0]`, the case
the old pattern could not see.

**And a tool fact worth keeping:** the first fix used an unbounded alternation
and **backtracked catastrophically**, running over two minutes on a corpus the
bounded version scans in seconds. The repetition is capped at three words
deliberately.

### The sweep, and its honest result

TERRCOMP recommended sweeping `tools/` for the same shape. Done, and **the
answer is narrower than the recommendation implied, which is worth saying
plainly rather than leaving an alarming number standing.**

Twenty-one tools mention SystemVerilog declarations and never mention `signed`.
**The port-parsing ones are NOT blind**: `check_counters.py` and
`check_port_coverage.py` both use `(?:[\w:]+\s+)*?` — unbounded word repetition
— so `signed` matches as simply another word. Verified by probe, not by
reading: four declarations including `output var logic signed [31:0] x_o` were
fed to both patterns and both resolved every one.

**So the blindness was in exactly two tools, and they share a cause: each
allowed EXACTLY ONE leading type word.** That is the shape to grep for, not the
absence of the string `signed`.

## R174 — R163 AS I WROTE IT IS TOO BROAD

TERRCOMP: **`zhao_terrain_bake_v2` produces no supersession violation and
cannot**, because `zhao_terrain_bake` (the v1) is **instantiated nowhere**.

R163 says a `_v2` file that merely exists manufactures a `superseded` violation.
**That is only true when the v1 is COMPOSED** — `superseded_in_closure()`
reports INSTANTIATED modules with a higher-versioned sibling on disk, and a
module nothing elaborates costs the fitter nothing, which is deliberate (a check
that cries wolf about dead sources is one people learn to skip).

So the correct statement is: **a `_v2` born beside a COMPOSED v1 manufactures a
violation; a `_v2` born beside an uncomposed one does not.** E1's observation
was real — it hit three production roots because `zhao_terrain_patch` *is*
composed. My generalisation dropped the condition that made it true.

**The practical rule survives intact** (a `_v2` is born in the commit that
composes it), and it survives for a better reason than the one I gave.

## R175 — A WARNING IN CAPITALS ON LINE 1 DOES NOT STOP PEOPLE READING THE FILE

`fpga/quartus/prod_fit_sources.txt` opens with:

> `# ORPHANED 2026-09-09. NOTHING READS THIS FILE. Do not edit it and do not`
> `# draw conclusions from it.`

**It has been misread four times in eleven days — including by owner ruling
R86.** The warning is as loud as a warning can be and it is at the very top, and
it did not work, because **a grep hit does not show you line 1.** You get a
matching line from the middle of the file, and it looks exactly like a live
source list.

Verified here: nothing in `tools/` actually *reads* it — but
`completion_register.py` **cited it in a docstring as evidence** that
`zhao_terrain_bake.sv` is carried "into the production fit". That is a
conclusion drawn from the orphaned file, sitting inside the tool that measures
completeness, written by someone who had presumably read the banner.

**Renamed to `prod_fit_sources.ORPHANED.txt`**, so the *path in the grep hit*
carries the warning. The register's citation is updated to the new name and now
says, beside it, that citing this file is the mistake being described.

This is the `.gitignore` lesson in its purest form: **making a hazard invisible
to nobody is not the same as removing it.** The fix for a warning people do not
read is not a louder warning; it is putting the warning where the reader
actually looks.

## R176 — THE RATIFIED LAW PACKAGE IS IMPORTED ONLY BY THE MODULE THAT IS NOT COMPOSED

TERRCOMP's new finding, and it is the one that makes R173 matter.

`zhao_terrain_patch_law_pkg.sv` was created by E1 *this evening*, to hold the
ratified terrain arithmetic **before a v2 could copy it**. TERRCOMP measured who
imports it: **only `zhao_terrain_patch_acc`, which is NOT composed.** The two
modules that ARE composed — `patch` and `lodfeed` — carry their own copies of
`fx_add_sat`, `covers` and the `h16 → fx` conversion.

**So the console today ships three implementations of a law that has exactly one
ratified statement, and the single module that defers to it is the one not in
the closure.**

**And this is precisely what `duplicate_functions.py` exists to find, and
precisely what it could not see**, because those are signed functions (R173).
TERRCOMP put the two together in one commit subject, which is the right
reading: the blind tool is *why* the duplication stood.

## R177 — THE STANDARD TERRCOMP SET, worth copying

It refused all four compositions, changed **zero files**, and is one of the most
useful packets of the campaign. Three habits to copy:

1. **It fired a POSITIVE CONTROL before quoting a null.** Its R115 sweep found
   **0** hits for `normalmap` in `spec/`, so rather than report the zero it ran
   the same sweep on terrain terms and got **14** — proving the sweep could see
   its subject. Nobody asked it to. That is R166's habit arriving by itself.
2. **It corrected three of its own claims, each in its own commit**, and
   diagnosed the single shared cause: *citing a file it had not opened.* One of
   the three, in its words, *"made my own recommendation look better"* — and it
   said so in the subject line.
3. **It re-measured every inherited blocker** and found two of terrain7's spent:
   R83's Q12.8 widening is **DONE** (`PROJW = 20`, with a real producer in
   `zhao_view_projq88`, both v2s wired in `prod_top` and carried in
   `fit_targets.yml`).

**Corrections to me that stand:** the Wave-4 brief's split was "11 disconnected
+ 1 unbuilt" when it is **12 + 0** — `zhao_geom_warp` was missing from my list,
because I wrote the brief from the register's output *before* W1 merged and did
not re-measure after. The same class as R144 and R155: **a brief is a snapshot,
and I keep shipping them as though they were standing facts.**

**Its owner recommendation, recorded and NOT acted on:** R115 — supersede
TERRAIN.NORMALMAP (`cut_order: 1`, and `strength=0` is a bit-exact no-op), which
would drop 21 → 20 honestly. **That is an owner decision and stays one**: taking
it on my own authority would be closing a gap by removing function, which is the
one thing rule 1 forbids however well-evidenced the case.

## R178 — THE BIGGEST AREA WIN OF THE CAMPAIGN WAS A RULING NOBODY EXECUTED

**2026-09-20, FORGE4. −5,698 ALM and −3,086 registers, 13.6% of the device, and
it required no new engineering whatever.**

**R142 ruled "adopt `zhao_forge_cliff_ram`". Nobody carried it out.**
`prod_manifest.yml`'s row named its own discharge condition — *"adopt … only
after that gate"* — and **the gate had already run twice.**

`CLAUDE.md`'s uncashed-cheque chapter describes this exactly: the analysis was
right, the prerequisite was built, the decision was made and written down by
someone who knew precisely what they were deferring, and **the final step was
simply never performed.** The twist that makes this one worse than the
projector: **`uncashed_cheques.py` was reporting it correctly the whole time.**
The tool built to catch this class caught it, printed it, and nobody read it
back. *A detector nobody reads is a detector that does not exist.*

**FORGE4 did the two things that make the number trustworthy.** It verified from
the **primary receipts** rather than the ruling — both `.sources.sha256` digests
match its tree exactly, same device — and it **re-ran the equivalence half**
rather than quoting it, on the correct grounds that a fit settles *area*, not
behaviour: 246 lattices, 752 pages, **0 mismatches**, reproducing the
2026-09-10 cycle figures to the digit.

## R179 — A THIRD REGISTER BLIND SPOT, AND THIS ONE HID A SAVING

**`successor_in()` requires a module's tail to `fullmatch` `v\d+`.**
`zhao_forge_cliff_ram` is a **RIVAL**, not a version — so the candidate was
**structurally invisible to the completion register**, which went on quietly
resolving `FORGE.CLIFF` to the 6,674-ALM module.

Put the three together, because they are one shape seen from three sides:

* **R159** — the register cannot see an **undeclared tie-off**. Hides a **gap**.
* **R173** — `duplicate_functions` cannot see a **signed function**. Hides
  **duplication**.
* **R179** — the register cannot see a **rival implementation**, only a
  higher-numbered version. Hides a **saving**.

**Every one is the flattering direction**, and the third is the most expensive:
it concealed 13.6% of the device for two days while the campaign's stated
blocker was area.

**The consequence for practice:** `blocks.yml`'s `implementation:` line is
**load-bearing, not documentation.** It is the only place a rival can be
declared, because the naming convention cannot express one.

## R180 — `upstream:` IS DESIGN INTENT, NOT A WIRING CLAIM. Three passes read it wrong

`GEOM.SETUP`'s `upstream:` names **six producers, of which exactly one is a
port.** FORGE4 measured the far end, which no earlier pass had done:
`zhao_geom_setup` is composed with **one** triangle arm, **fully occupied** by
GEOM.CLIP through the ATTRPACK fork, **with no arbiter**, and that arm is
**screen space** (`signed [20:0]`) — while PRIM emits index triples, PRIM_EVAL
world fx16, and the cliff a rim edge.

**And the same missing door is already costing a second block:**
`zhao_part_expand` is composed, emits the right shape, and **leaves the core as
boundary I24 for want of exactly that arbiter.**

This is the **third** ledger-edge-read-as-a-port inside one entry. So the rule
is now general and belongs beside the ledger: **an `upstream:` row is a
statement of intent. Reading it as a claim about the tree has misled three
passes**, and the check is always the same — look for the INSTANTIATION, never
the name.

## R181 — R168's REPAIR FOUND THE DEFECT WAS LIVE, NOT MERELY LATENT

WARPFIX, and this upgrades R168 from a hazard to a shipping bug.

R168 was recorded as a **mis-wiring** hazard. It is worse: **the hole is
reachable with legal stimulus in the correctly wired console.** The host answers
`StOk` when the **image's required mask** is satisfied, so an image declaring
five of six ordinals **retires OK with present bit 5 clear** — and an adapter
deciding on `resp_status_i == 0` alone **published the cleared register as a
normal component.** That is W10's *"an absent output must not look like a zero
result"*, shipping.

**The evidence is a run that FAILED**, which is the only kind that counts here:
the new test's exact source against the wrong-wiring arrangement failed 12 of 22
checks — *"ORDINAL 5 CAME FROM R21, NOT FROM THE UNWRITTEN WINDOW LANE 5,
expected 0x63, got 0x0"* — and passed 22 of 22 against the right one. Every
pre-existing Warp test passed against **both**, which is what made R168
invisible for as long as it was.

**Two mutant design choices worth carrying forward:**

1. **The completion rule is deliberately NOT mutated, so the mutant still
   answers `StOk`.** This defect is a *successful* run carrying a wrong number;
   a mutant that answered `StPartial` would be caught by any status check and
   would be **a positive control for the wrong thing**.
2. **The mutant's first case is a CONTIGUOUS negative control** — under a
   contiguous program it is indistinguishable from production. Without it, the
   sparse verdict could be firing on a botched rename or a stale copy rather
   than on the mutation.

**Correction to my brief:** the SPARSE case **was** already registered in ctest
(`warp_field_chain_directed`). R168's narrower wording held — the *case* was not
separately named, so a refactor could have dropped it with everything else still
green — but "not a name in ctest" was simply wrong, and the packet said so.

## R182 — `OUT_LANES` MEANS TWO DIFFERENT THINGS ONE LINE APART, and that is R168's cause still live

WARPFIX repaired the instance and **named the disease, without fixing it**, which
is the right call this close to a fit:

`zhao_field_warp_adapter.OUT_LANES` sizes `resp_out_i`, which is
**ordinal**-indexed. So the bench must pass `.OUT_LANES(W_ORDINALS)` while the
host one line away takes `.OUT_ORDINALS(W_ORDINALS)` **and**
`.OUT_LANES(HOST_WINDOW)`.

`tb_warp_field_chain.sv` already calls this *"the sharpest edge in the whole
composition."*

**Renaming it to `OUT_ORDINALS` is a docket item, not a pre-fit change** — it
touches `prod_manifest.yml` and the fit is the scarce resource. But it must be
recorded as **the cause rather than a tidiness item**: R168 happened because two
quantities of the same width carried names that did not distinguish them, and
that condition is still in the tree.

## R183 — THE HARNESS HAS NOW COST FIVE LANES THEIR REPORT FILE, AND PUT A DANGLING CITATION IN PRODUCTION RTL

TERRCOMP, WARPFIX and FORGE4 all reported that **they cannot write a
`FINDINGS-*.md`** — the harness refuses report `.md` files from a subagent —
following terrain6 and terrain7.

Three consequences, escalating:

1. Findings live in **commit messages**, which are one squash from unreadable
   and are not where the next session looks.
2. **`zhao_console_core.sv` already carries a citation pointing at a
   `FINDINGS-forge.md` that was never written** — the same refusal, recorded in
   production RTL as a reference to nothing.
3. **WARPFIX declined to route around it** by switching tools, and said so.
   That is the correct call and should not be punished: an agent that works
   around an explicit refusal is a worse outcome than a missing file.

**The coordinator transcribes them** — `FINDINGS-terrcomp.md` (988 lines),
`FINDINGS-warpfix.md` and `FINDINGS-forge4.md` are all landed by hand. **That is
a workaround, not a fix**, and the fix belongs at the harness.

## R184 — THE BUDGET'S TOP OPTIMISATION TARGETS ARE MODULES WE DO NOT SHIP

**2026-09-20, flagged by FORGE4, and larger than it reported.**

`reports/BUDGET_HEATMAP.md`'s ALM ranking led with **`zhao_forge_cliff` at 7,664
ALM / 18.3% of the device** — the module the console had **just decided not to
ship**, having adopted `zhao_forge_cliff_ram` at **976 fitted ALM** (R178).

FORGE4 flagged it rather than hand-editing a generated file, which was the right
call, and named two more it had noticed. **Measured while fixing it: six of the
fourteen rows are superseded — the TOP THREE, and five of the top eight:**

| struck row | est. ALM | superseded by |
|---|---:|---|
| `zhao_forge_cliff` | 7,664 | `zhao_forge_cliff_ram` |
| `zhao_terrain_project` | 5,503 | `zhao_proj_subsystem` |
| `zhao_geom_bin_pipe` | 5,299 | `zhao_geom_bin_pipe_v2` |
| `zhao_geom_project` | 5,028 | `zhao_proj_subsystem` |
| `zhao_raster_tile_pipe` | 4,465 | `zhao_raster_tile_pipe_v2` |
| `zhao_terrain_bake` | 2,324 | `zhao_terrain_bake_v2` |

**The real #1 shipped ALM consumer is `zhao_field_seq` at 5,142 / 12.3%.**

**`build_manifest.py` had no notion of a disposition at all.** So the document
this campaign would consult to decide *where to spend area effort* has been
pointing at retired modules — and pointing at the biggest one hardest.

**It is wrong in the flattering-looking direction, which is why it survived.**
It **overstates** the remaining problem, and an overstated area figure reads as
honest bad news, so nobody audits it. Meanwhile it sends the next reader at a
block whose replacement was chosen days ago — the most expensive kind of wasted
effort, because the work looks well-targeted the whole time.

**The rows are STRUCK, not dropped.** The measurement is real, and a table that
silently shortens itself is its own defect — the same reasoning that keeps a
superseded module on disk as a differential oracle. A paragraph beneath names
the struck rows and says plainly why one is not a target.

**The scraper carries a self-check**, because a pattern that matched nothing
would strike no rows and look **exactly** like a tree with no superseded
modules — this repository's single most repeated failure. It resolves **38**.

And it is deliberately **text-scraped rather than YAML-parsed**, with the reason
in its docstring: `console_inventory.yml` carries very long multi-line `why:`
strings, and a parser that choked on one would take the whole heatmap down. Its
failure mode degrades to the old behaviour — the row simply is not struck —
**which is the safe direction for a cosmetic annotation and the WRONG direction
for anything load-bearing. It must not be reused as a gate.**

## R186 — A PACKET RETRACTED ITS OWN HEADLINE, AND NAMED THE INCENTIVE BEHIND IT

**2026-09-20, POSTMEAS, and this is the best single paragraph any lane has
written this campaign.**

It concluded from a **correct measurement** — the spec defines exactly one tag
channel, `GLOW = 0b01` — that R37 covers only one of POST.GATHER's three planes
and therefore could not unblock composition. It filed that as **a new owner
decision**. Then it checked, and **retracted it**:

> R37's proposal decides all three explicitly. `POST.GATHER.md` decision 3 rules
> `c_disp_*` / `c_ink_o` **zero in v1**, and `zref::post::gather` implements it
> with `kChannelReserved2/3` and a `reserved_channel` counter. **There is no new
> owner decision; strike commit 4 §5.**

**The cause is exact, and it is the one TERRCOMP diagnosed the day before, in a
packet that had READ the diagnosis:** it read the R37 section through a
`grep -B4 -A12` window **that skipped the three numbered decisions sitting
between the matched lines.**

**And then it named why that direction of error is the dangerous one:**

> *"My error ran in the direction that made the gap look bigger and my own
> finding look more important. Nobody audits good news, and good news for a
> packet is not good news for the design."*

That is `CLAUDE.md`'s broken-instrument law turned on **the agent's own
incentives**, and it is a genuinely new formulation. The law says a broken tool
lies in the direction that makes the answer look better. **An agent's bias runs
the other way — toward the finding that makes its own work look more
significant** — and the two failure modes are therefore *different* and need
different guards. A lane that finds a NEW BLOCKER has just made its own refusal
more defensible, and that is precisely when it should check hardest.

**The general rule, and it now has three instances in two days: OPEN THE FILE
AND READ THE SECTION, NEVER A GREP WINDOW.** A `-B4 -A12` window is a sample of
a document, and a document's structure — numbered decisions, a retraction, a
banner on line 1 — is exactly what a sample destroys.

POSTMEAS also caught two further wrong numbers of its own by re-counting
(census 90/84 → **88/87**; `check_counters` rows "fifteen" → **119**), and it
validated a null with a **positive control**: its first port sweep saw **zero**
matches because the core writes `input logic` without `var`, so it re-ran
against **283 visible input ports** before trusting the single match it found.

**Corrections to my brief, both accepted:** **R65 is NOT load-bearing for
POST.GATHER** — zero citations in its contract or RTL; it is terrain's. And the
tie-off audit baseline is **8/1/10/0**, not the 7/1/10/1 I quoted, which was the
pre-fix number.

## R187 — THE ARBITER WAS THE SMALLEST OF FOUR BLOCKERS, AND ONE DECISION UNBLOCKS TWO SUBSYSTEMS

**SETUPDOOR, refusing the packet I commissioned on R180's reading.** The *shape*
half of R180 is confirmed; **the conclusion is not**, and the correction is
worth more than the arbiter would have been.

**`zhao_geom_setup`'s arm is not a stream endpoint.** It is one tine of a
**THREE-WAY ORDERED JOIN** — setup's edge functions, `zhao_geom_attrpack`'s
three 240-bit planes, and `u_material_window`'s resolved material *plus an
occupancy accounting* — **pairing by ARRIVAL ORDER with no tag.** The window's
own comment states the invariant a particle breaks: *"There is no fourth outcome
for a triangle in that span."*

Feeding a particle in **deadlocks combinationally**, proved from three assigns
in the core: `st_o_ready` needs `ap_o_valid_w`; ATTRPACK never saw the particle;
SETUP cannot drain; `cl_o_ready` falls; GEOM.CLIP stalls — **and ATTRPACK is fed
only THROUGH GEOM.CLIP.** A closed cycle. The other branch is worse: the
particle's edge functions would join a *mesh* triangle's planes and material,
**skewed by one for the rest of the frame**. Two committed error counters would
fire on every particle, so the interlock is already instrumented.

**THE BINDING BLOCKER IS NOT THE ARBITER. It is the seven-slot attribute
packet** — `GEOM_CLIP_ATTRS = 7` per corner — **the same wall entry (b) already
records for TERRAIN.** A polygon particle has a flat colour, one shared 1/w and
**no texture coordinates by law**; zeroing u/w and v/w would sample texel (0,0)
on every particle.

**TERRAIN AND PARTICLES REACHED THAT WALL INDEPENDENTLY, in two packets that
never spoke. So ONE owner decision — an untextured attribute law — unblocks
BOTH**, and it is now the highest-leverage open item in the campaign.

**Recommendation recorded, not acted on:** the honest door is at **GEOM.CLIP's
input**, not GEOM.SETUP's — it yields winding normalisation, 2A, bbox,
zero-area reject and three-tine lockstep for free.

## R188 — REASONING FROM A PORT WIDTH IS REASONING FROM A PROJECTION

SETUPDOOR's width answer, and it is `CLAUDE.md`'s art law arriving in RTL.

The brief demanded the 22-vs-21-bit question be **answered, not assumed**. The
answer: **the 22nd bit is HEADROOM, never RANGE.**
`zhao_project_core::to_screen_xy` clamps to **±524288** — *"the clamp is the
law"* — and is the only producer of `p_x_i`/`p_y_i`; max offset 4080, so
**max |vertex| = 528368 < 2^20.**

**Proved exhaustively** over all 256 size bytes × four rail corners, asserted on
every vector of both lanes, and — the part that matters — **SEEN TO FIRE**: a
positive control with the clamp premise withdrawn failed **exactly 1 of 757
checks, and only that one.** A control that fails everything proves nothing
about the specific premise.

**And it corrected a shipped header while it was there:** `zhao_part_expand.sv`
reasoned from **the port width** — *a projection of the value rather than its
law*. That is `CLAUDE.md`'s **"measure things that ARE the thing, never a
projection of them"**, written about a creature's body taper derived from a 2D
drawing, recurring verbatim in SystemVerilog.

The practical consequence is the opposite of what the packet was commissioned to
do: a future door may narrow **22 → 21 losslessly and provably**, rather than
widening the arm, the core's `render_ax_i`, both shell tops and the raster **for
a bit that cannot be set.**

**Bonus, both now pinned by committed checks:** the particle fan is
**negatively wound** against setup's stated `2A > 0` precondition, and **size 0
is a zero-area triangle setup does not reject.**

## R189 — EIGHT PACKETS AT 21, AND THAT IS THE REAL STATUS

The register has read **21** across eight consecutive packets: E1, W1, CMDFIELD,
TERRCOMP, WARPFIX, FORGE4, POSTMEAS, SETUPDOOR.

**None of them failed.** Between them they composed `zhao_field_host_v2`, built
GEOM.WARP, adopted a variant worth **5,698 ALM**, repaired a live ordinal
defect, corrected an admission classifier that was admitting programs over their
deadline, and produced the instrument findings this ledger is mostly made of.
**Every refusal came with a measured blocker**, and several blockers were found
to have expired rather than being inherited.

**The honest reading is structural: what remains is not wiring.** Of the twelve
disconnected modules, eight have now been refused by packets that measured
*why*, and the blockers are **missing producers** and **decisions only the owner
can make**. A ninth packet aimed at composition would very likely return 21
again, with a ninth well-measured refusal.

**So the fastest remaining path to zero runs through the owner's inbox**, and
that is where the effort should go: `gz/dossier` is consolidating every open
decision with its evidence, its cost and what it unblocks. **The single
highest-leverage item is the untextured attribute law (R187), because two
subsystems hit it independently.** The cheapest is **R37**, which needs the
owner's *eye* on a contact sheet **already rendered and committed** at
`reports/post-gather-law/gather_law_contact.png`.

## R190 — THE DOSSIER STRUCK NINE DECISIONS AS SPENT. FIVE CAME FROM MY OWN BRIEF

**2026-09-20, DOSSIER. 12 live decisions, 9 struck.**

**Five of the nine were handed to it as live, by me.** My brief listed ten known
items and told it they were "a starting list, NOT a complete one, and some may
be SPENT" — which was the right hedge, and it was also the only reason the
packet went looking. But the underlying fact stands: **half the decisions I
believed were blocking the console were already decided**, and I had been
quoting several of them to the owner.

**The strike that matters most, because it had been disproved once already:**

> **`{handle → hash}` HAS a hardware producer.** Op 3 is FH2 sub-kind 1,
> `BIND_PROGRAM`, and `zhao_field_loader` exports `pub_handle_o` /
> `pub_prog_hash_o` from `obj_handle32[]` / `obj_prog_hash[]`, composed in the
> core. Its own comment says the mux *"costs one mux the descriptor table needs
> anyway"* — **it was built for this consumer.**

**Why it kept being re-asserted is the general lesson.** The sweep searched
`program_hash|prog_hash|programHash` — and **`pub_prog_hash_o` MATCHES
`prog_hash`.** The pattern was never wrong. **The search was run before the
producer landed, and the producer landed the same day.**

**A SEARCH IS A CLAIM ABOUT A MOMENT**, exactly as a refusal is (R165). And
this one is worse than a stale refusal, because **R165 already flagged this
blocker as expired, and the core entry re-asserted it afterwards anyway.** A
disproved blocker came back. So:

* **a zero-hit sweep needs a date**, and it needs re-running before it is
  quoted, not when it was written;
* **and when a blocker is struck, the strike has to land in the ENTRY**, not
  only in a ruling. R165 recorded the expiry in the ledger; the core's own
  INCOMPLETE text went on asserting the opposite, and the core is what the next
  packet reads.

Also struck, each with primary evidence: the cliff decision (made **and**
executed, −5,698 ALM), R83's Q8.8 governor saturation (repaired, `PROJW = 20`),
`forge_kind`'s "EXACTLY ONE member" (R108 granted five), and **R13, which
already answers I21's layer-E join.**

And it confirmed POSTMEAS against my brief: **R65 has zero citations in
POST.GATHER's contract or RTL.** R65 is terrain's; R37 is POST.GATHER's. I had
the two tangled and had said so out loud.

## R191 — I STRUCK A TENTH IN ONE CHECK, AND IT WAS THE DOSSIER'S OWN NEW FINDING

The dossier's headline discovery was a decision nobody had written down — the
**untextured attribute law** — and its sharpest supporting claim was that
FORGE.PRIM's case is a hang:

> *"`zhao_geom_vattr`'s `done_o` ANDs `lit_ord_q == uv_ord_q`, so a hull with
> neither colour nor u/v wedges the entire front end **with no timeout, no abort
> and no counter**."*

**All three absences are false, and I found it in one read** — while verifying
before commissioning a packet to build the missing detector.

`zhao_geom_vattr.sv` carries a section headed **"`done_o` CAN WEDGE THE WHOLE
GEOMETRY FRONT END, AND NOW IT SAYS SO"**, citing **owner ruling R88**, which
had already ruled on precisely this hazard in almost the same words. What is
there:

* **`done_stall_o`**, counting episodes where the store owes something and
  nothing has moved for `STALL_LIMIT` clocks;
* **its two sides clocked by different things, deliberately**, citing
  `CLAUDE.md`'s *"a detector wired to two operands that move together cannot
  fire"*;
* **every term an EVENT, never a busy LEVEL** — *"a hang holds a busy level
  high forever, so a watchdog that trusts one is silent through the hang it
  exists for"*;
* **fired by legal stimulus** in `geom_vattr_directed` cases L and M, and
  asserted **zero** on every clean batch, so no mutant is owed;
* and **no abort, deliberately** — because releasing a batch early would serve
  REPLAY rows that were never written, which is a **policy** change and not a
  block's to make.

**The underlying decision is still live** — the attribute law is genuinely
undecided, and three lanes hit it independently. **What is dead is the alarm
attached to it.**

**Two things this says, and the second is the uncomfortable one:**

1. **A false absence survived a packet whose entire purpose was checking for
   false absences.** The dossier struck nine and introduced a tenth. That is
   not carelessness — it is the same structural pull POSTMEAS named: **a lane is
   rewarded for finding something, and "there is no detector for this hang" is a
   far more compelling sentence than "there is one and it is good."**
2. **I nearly commissioned a packet to build it.** The check that stopped me
   cost one `grep -c` and one `sed`, and it is the check `CLAUDE.md` already
   prescribes — *"before commissioning a new block, grep the tree for the thing
   it replaces"* — written after a vertex arena was built beside an existing one
   that had 58 formal assertions and a committed proof.

**So the verification pass is now its own packet** (`gz/dossiercheck`), briefed
to attack all twelve, with the warning that **its incentive runs the opposite
way from the dossier's**: it is rewarded for striking, so its characteristic
error is striking something genuinely live, and every strike must quote primary
evidence rather than a summary.

## R192 — TWO CONTACT SHEETS ARE NOW THE CAMPAIGN'S CRITICAL PATH

**R65 unblocks the most** — I32, `zhao_terrain_bake_v2` and the terrain page
format — and **six lanes have closed none of those.** **R37** is second, gating
POST.GATHER and I17 and nothing else.

Both sheets are rendered, committed and pushed. **Verified, by hash, in the
commit that is on origin** — because telling the owner "just look at this" and
being wrong about where it is would waste exactly the attention the dossier
exists to save:

```
reports/terrain-seam-dig/seam_dig_contact.png    246,790 bytes
reports/post-gather-law/gather_law_contact.png    38,683 bytes
```

**No agent can advance either by any amount of work.** That is the whole point
and it is worth stating plainly: this is not a task that is hard, or expensive,
or waiting on a fit. It is a task that requires eyes, and the campaign has
produced eight consecutive well-measured refusals partly because nobody has
looked.

## R193 — FOUR MORE DECISIONS THAT WERE BEING CARRIED AS FACTS

The dossier's most useful category: things treated as settled background that
are actually unmade decisions.

* **Terrain's two absent art laws.** **I13 has been mis-scheduled as wiring six
  times.** Six passes read a missing *law* as a missing *connection* — which is
  R180's `upstream:` trap one level up, and it is why I13 keeps costing packets.
* **The devstore at 185 of 553 M10K.** That is **2.4× R59's stated 14%
  premise** — and the dossier names why it survived: *"which is why nobody
  audited it."* A premise stated once, in a ruling, and then treated as a
  measurement. **33% of the device's memory is spoken for by a block whose
  budget line says 14%.**
* **The I21 view-mask reconciliation** — one sentence, unresolved across six
  passes.
* **FH11's width**, ~+6,000 ALM, against a `zhao_block_fit.json` row that
  **does not contain FIELD at all** — a cost quoted from a receipt that does not
  describe the subsystem being priced.

## R194 — THE SEAM DIG: ACCEPTED. The page format is frozen at 64x64

**Fabian, 2026-09-20, by looking:** *"Shipped is fine. Slightly different but
not off."*

The nearest-texel rim stands. `sheet_texel_for_vertex` stays as written and does
**not** become the identity. **The terrain page format is frozen at 64x64**, and
every later terrain block inherits it.

**This is `CLAUDE.md`'s art law doing precisely the job it was written for.**
Every measurable thing about this fallback had been measured, and the
measurements did not decide it: **3.25 m is the dig's full depth**, which sounds
fatal, and it **lands inside a staircase the 1 m lattice already produces**,
which sounds harmless. Four of 99 shared border vertices disagree at the worst
placement the tool can construct. *"Measurement can remove a BIAS; it cannot
choose a VALUE."* The owner's eye chose it in one sentence.

**Unblocked immediately:** **`zhao_terrain_bake_v2`**'s Option A layer-F reader
— whose address generator was *exactly* the contested thing — and the page
format itself.

> **CORRECTION, 2026-09-21.** This ruling originally also claimed I32 was
> unblocked **"directly"**. **That was overstated**, flagged first by
> DOSSIERCHECK and then measured by SEAMDIG, which spent this ruling and
> re-ran all five of I32's blockers in its own tree: **every one is still
> live.** Layer D has no reader anywhere; **zero** consumers exist for
> `res_texel_i` / `res_strength_i` / `res_before_i` in `fpga/` *or* `tests/`,
> so I32's named consumer does not exist as a port at all; `vtx_nobake_i` has
> no producer; `cmd_*` has no producer; and `TERRAIN.PAGEIO` has a written
> contract and **no `design/blocks.yml` row**.
>
> **The owner's decision was still the right one and it was still spent** — the
> reader is built, §9.3's two laws are in RTL for the first time, and the page
> format is frozen. **But "unblocks I32 directly" was my sentence, not a
> measurement**, and it is exactly the R180 shape: reading a blocker's removal
> as a wiring change when what remains is a missing subsystem.

**Saved:** +38.3% page size, three tripped elaboration guards, and a page that
would have nearly doubled (8,450 B → 16,384) because `zhao_terrain_jdoorbell`
requires a power of two.

**R116 called this "not six failures; it is one blocker seen six times."** Six
terrain lanes closed none of it. **The blocker was a question nobody had been
asked** — and the campaign spent days routing around a decision that took one
look. The cost of *not asking* is the finding here, not the answer.

## R195 — THE GATHER LAW: RATIFIED AS PROPOSED, TWO BLUR PASSES

**Fabian, 2026-09-20, by looking:** *"Everything but before looks basically the
same. Pick cheapest."*

Ratified: **`kGlowKnee` 24, `kGlowSlope` 0x1C, tint 255/236/224, `kGlowMaster`
255, TWO blur passes.** Not one coefficient moves.

**"Cheapest" resolves to the proposed law, and the reasoning matters because the
sheet's three axes do not cost the same thing:**

1. **Blur passes are the real cost axis and the only one priced on the sheet.**
   One pass = two sweeps of the 96×60 plane = 11,520 cell-steps. **Two = 23,040,
   which is the number this contract already budgets for Z60.** Five = 57,600 =
   **3.5% of a 1,666,666-clock frame** — for roundness nobody asked for.
2. **`knee` is a cost axis in the OPPOSITE direction, and lower is not cheaper.**
   The contract measured it: **knee 16 has 907 of 5,760 cells contributing
   against 74** — twelve times the work — and its own text says that row is what
   *"the whole image hazes"* looks like. Taking the smallest number in the
   column would have bought a hazier image **and** twelve times the cells.
3. **`bloom_gain` is not a cost at all** — a multiply constant, rescaled per
   frame at runtime by `SetPost.bloom_gain`.

**So the cheapest reading the owner's eye accepts is the renderer's existing
default, unchanged.** Worth stating plainly: *"pick cheapest" did not mean "pick
the lowest number in every column"*, and a packet that had read it that way
would have shipped knee 16 and a hazed frame while believing it was following
instructions.

**Unblocks `zhao_post_gather` and tie-off I17, and nothing else.** The remaining
obstacle is **one 8-bit tie-off**: `zhao_shell_top_v2.sv` discards the resolved
tag as `rp_fb_tag_unused` while every neighbouring field leaves.

## R196 — AN UNPROMPTED ART OBSERVATION: THE MORPH DOES NOT READ

Looking at `reports/terrain-lod-readings/lod_readings_contact.png`, which was
**not** a live decision, the owner said:

> *"Not a real question. **Mesh looks like an awesome canyon-like rig. Morph
> doesn't look like much.** Interesting."*

**Recorded verbatim and NOT acted on, because it is an observation and not yet a
ruling.** But it is the kind of observation this project exists to catch, and it
deserves a look rather than a shrug:

* **The mesh reads well** — that is a positive result nobody had written down,
  about geometry that is already built.
* **The morph does not read.** That is either a transition doing its job
  invisibly (which is success), or **silicon spent on something the eye cannot
  see** (which is the crayon-grain failure from `CLAUDE.md`'s art chapter — the
  grain that *"measured fine and looked like flat plastic"* because it was
  clipped narrower than the light rig's own range).

**Those two readings have opposite consequences and the sheet cannot separate
them**, because a contact sheet of *stills* is the wrong instrument for a
*transition*. `CLAUDE.md` says so directly: judging an animation from stills
finds the typical frame and misses the broken one; what is wanted is a
**trajectory plot of the morph weight against what actually moves on screen**,
or a before/after pair at the same instant.

**Queued as a question, not a defect.** Nothing is changed on the strength of a
one-line reaction to a sheet rendered for another purpose.

# THE REMAINING FOUR DECISIONS, TAKEN BY THE COORDINATOR

**Fabian, 2026-09-20: *"answer dossier questions yourself. use a fable agent if
hard."*** The two that needed an eye he answered himself (R194, R195). These
four are engineering calls with measured costs, and they are taken here.

## R197 — THE UNTEXTURED ATTRIBUTE LAW: **a sanctioned profile, declared by a FLAG**

**Option A. A primitive MAY enter GEOM.CLIP with `u/w` and `v/w` undefined,
provided it DECLARES that it has none.**

Three lanes hit this wall independently without recognising each other —
terrain (I13), FORGE.SHADOW Route A, FORGE.PRIM — which is why it is the
highest-leverage item on the board and why it had never been raised as a
decision at all.

**Why A and not the other two, on the evidence already gathered:**

* **The hardware already wants it.** FORGE.SHADOW Route B's own design note says
  *"u/v unused"* in as many words. A decision that ratifies what the design has
  independently concluded is cheap; one that fights it is not.
* **Option B — "every producer must synthesise u/v" — is expensive AND
  dishonest.** It forces a terrain texture-coordinate law, which is **art
  content and the owner's to author**, and it invents u/v for shadow hulls and
  particles that have none *by law*. **Inventing an art law to unblock wiring is
  backwards**, and this campaign has spent the day learning what happens when a
  packet treats a missing law as a missing connection: I13 has been
  mis-scheduled as wiring **six times** (R193).
* **Option C — a separate untextured path — is a second door**, and SETUPDOOR
  has just measured what a door costs here: GEOM.SETUP's arm is a three-way
  ordered join that deadlocks combinationally on an unexpected producer (R187).
  A fourth lane is the largest option for the least reason.

**THE BINDING CONSTRAINT ON THE DESIGN, and it is not negotiable: the absence
must be DECLARED, never ENCODED.**

`u/w = v/w = 0` is **not** a neutral value — it samples **texel (0,0) on every
primitive**. That is W10's *"an absent output must not look like a zero result"*,
and this campaign has been bitten by exactly that twice **today**: R168, where
an adapter decided on `resp_status_i == 0` and could not tell *"not requested"*
from *"requested and came back zero"*; and R181, where the same hole proved
reachable with legal stimulus. **A sentinel value is the defect. A flag is the
fix.**

So the law is:

1. **A per-primitive flag declares the packet untextured.** Not a magic
   coordinate, not a reserved value, not zero.
2. **Consumers BRANCH on the flag.** When it is set, nothing reads the slot —
   the slot's content is don't-care and must never reach a sampler.
3. **An untextured primitive arriving where a textured one is REQUIRED is
   REFUSED AND COUNTED**, never silently sampled. A counter with a positive
   control, per the standing rule.
4. **`R48`'s `ALPHA_C` is the precedent** — a per-primitive constant attribute
   with a named seam, already ratified, already the shape this wants.

**What it unblocks, stated honestly:** FORGE.SHADOW Route A and
FORGE.PRIM/PRIM_EVAL's attribute wall. **It unblocks only HALF of I13** —
terrain still needs `lit r, g, b`, and it has *one signed 32-bit scalar shade,
not three channels*. That second law is **terrain art content and is NOT taken
here** (dossier §5, which says in its own text "do not rule these yet").

## R198 — FH11's LANE WIDTH: **adopt the SEMANTICS, defer the WIDTH**

**The two are travelling together and they are different kinds of thing.**

* **The SEMANTICS — exact per-point status, no padding contamination — are a
  CORRECTNESS property and cost nothing. Adopted now.**
* **The WIDTH is a PURCHASE. Deferred to a fit.**

**And the headline number was wrong, which is the reason to write this down
rather than just agree with the repair plan.** DOSSIERCHECK measured it:
**`FAB_LANES` is ~+2,200 ALM and ~+12 DSP — the ~+6,000 figure prices the
`FAB_DIST_BANKS` × `FAB_GROUP_PTS` axis instead.** And **FH11 never asks for
four lanes**: *"At LANES=4 …"* is conditional.

So the purchase is smaller than the board believed — but **the principle that
defers it is untouched and is the real finding**:

> **`zhao_block_fit.json`'s `zhao_console_core` row DOES NOT CONTAIN FIELD AT
> ALL.** Its `.sources.sha256` lists exactly one field file.

**Every FIELD area number is additive to a budget that has never measured
FIELD.** Buying width against that row would be `CLAUDE.md`'s *"never compare a
current file to an old measurement"* with the subsystem itself missing from the
baseline — a confident number about a machine the receipt does not describe.

**Deferred, and the deferral CITES the measurement that would discharge it:** a
fit whose source list contains FIELD.

## R199 — A FORGE PROGRAM PAGE KIND: **deferred, and the ceiling is the reason**

**Not frozen now.** The dossier's own ceiling argument decides it and it is
decisive:

> Even with a page kind and the enum, **four of the six forge families have no
> evaluator at all.** `zhao_forge_prim_eval` is the **LIGHTNING evaluator,
> RIBBON family only**. **A page ruling buys one of six.**

R108 already said the same about its own half: *"It closes no gap on its own …
The ABI stops being the blocker; it does not become the implementation."*

**And the second forge block is refused on separate, standing owner authority** —
FORGE.SHADOW under R133 (D-FORGESHADOW-B), re-verified live by FORGE4 today.

Freezing a page format to unblock one family of six, while the evaluators for
the other five do not exist, is **committing a format before the thing that
consumes it exists** — the same asymmetry that made the terrain page format
worth deciding *early* (R194) makes this one worth deciding *late*: **the forge
packer does not exist either, so nothing is being lost by waiting, and a format
frozen ahead of its consumers is a format frozen on guesses.**

## R200 — THE SCOPING CALL: **HOLD. The owner has already answered this one**

**Fit at completion only. No fit now.**

The dossier files this as an open decision and DOSSIERCHECK correctly found that
*"hold for zero"* has **no owner source** — R135 is coordinator-authored and says
so itself. **But both are looking in the wrong place.** The owner's standing
instruction for this session is explicit and current:

> *"Drive the Zhaozhou console's mandatory gap count from 61 to ZERO, then
> freeze that design and run the honest Quartus fit against 5CSEBA6U23I7. …
> **Fit at completion only.**"*

**That is the owner source, and it is the active goal.** DOSSIERCHECK also found
the owner's committed directives require *"a new owner authorization before
running an earlier physical fit"* — so an early fit is not merely undecided, it
is **gated on an authorization nobody has requested.**

**So the decision is HOLD — but the honest part of this ruling is the tension it
must report rather than resolve quietly:**

1. **R189 established that the remaining 21 are not wiring.** Eight packets
   returned 21, every one with a measured blocker, and the blockers are missing
   producers and owner decisions.
2. **Three area decisions (6, 8, 11) are unanswerable without a fit**, because
   nobody knows where the 113% / 135% overage lives — and R198 has just shown
   the one budget row anyone would consult **does not contain FIELD at all.**
3. **So "fit at completion" and "decide the area questions" are, at this moment,
   in tension.** Holding is correct because it is the instruction; **pretending
   there is no cost to holding would not be.**

**What is NOT deferred by this ruling:** composing `zhao_terrain_lod` today
would create roughly **twenty-two** tie-offs, and all four terrain blocks
together *"would have read 21 → 17 and buried roughly fifty undeclared
tie-offs — available on any afternoon, and the campaign's single largest act of
self-deception."* **Holding the fit does not license buying the register down.**

## R201 — R176 WAS RIGHT AND TOO SMALL: five implementations, not three

**TERRLAW, 2026-09-20.** R176 said the ratified terrain law had three
implementations. **Measured, it had five.**

`fx_add_sat` lived in **patch, patch_acc, TESS and VELOCITY** — and per
`module_graph.build()`, **tess is composed in all three production roots and
velocity is in `zhao_prod_top`.** `sp_mask` was in two.

**The packet found the other two only because it measured with
`duplicate_functions.py` instead of reading the count off the ruling.** That
tool could not see signed functions until this morning (R173), which is very
likely why R176 undercounted in the first place — **a ruling written on a blind
instrument's output, corrected by the same instrument once it could see.**

**Two corrections to my brief, both self-diagnosed and both worth keeping:**

* **`lodfeed` has NO functions at all** and carried only the `h16 → fx`
  conversion — not `fx_add_sat`, not `covers`. My brief named it as carrying its
  own copies of all three.
* **`patch_acc` is uncomposed because its two mentions inside the composed
  `zhao_field_v3_exec` are COMMENTS.** The packet's own first reasoning — *"its
  instantiator isn't composed"* — was wrong, and it said so rather than letting
  a right conclusion stand on a wrong premise. That is R180's trap
  (`upstream:` is intent, grep hits are prose) arriving in a packet's own
  analysis.

## R202 — A WIDER VARIANT THAT WASN'T: the extra bit was a duplicated sign

**The near-miss in this packet, and the reason it is worth a ruling.**

`tess`'s `fx_add_sat` took **33-bit inputs summed at 34**. R176's own text warns
that if a composed copy handles a case the package does not, **the package must
grow to cover it** — never the copy narrow. So this looked exactly like the case
that forces the package wider.

**It was not.** Both call sites pass `{x[31], x}`, and all four operands are
`signed [31:0]`. **The 33rd bit was a duplicated sign bit.** Nothing was ever
carried in it; nothing narrowed when it went away. **The package file is
byte-unchanged** — `git diff` against the merge base on that path is empty.

**The lesson is the one `CLAUDE.md` states about port widths and SETUPDOOR
restated today (R188): a width is a PROJECTION of a value, not the value.**
Reading "33 bits" as "a wider quantity" would have grown a ratified law's
container to hold a bit that cannot be set — the same error as widening
`zhao_part_expand`'s arm for a 22nd bit that the clamp makes unreachable.

**And note the direction it would have failed in.** Growing the package would
have *looked* like diligence — honouring the rule that says never narrow — while
committing silicon to nothing. **The rule "never narrow" does not imply "widen
when in doubt"; it implies MEASURE WHAT THE VALUE CAN ACTUALLY BE.**

**No forwarders were left behind**, deliberately, so `duplicate_functions.py`
can still report honestly: **43 → 41** duplicated names, **433 → 429** distinct,
the −4 being exactly the four names removed. A forwarder would have made the
tool report success while the duplication persisted.

## R203 — AND MY OWN BRIEF'S ALM ARGUMENT DOES NOT TRANSFER

**I briefed this packet that the work was area-relevant**, citing plan §14.4's
*"eliminate duplicated ownership/engines by construction"* and the combiner
chapter where fourteen multipliers appeared. **TERRLAW measured and corrected
me:**

> §14.4 is about duplicated **engines**. A `function automatic` elaborates **per
> call site wherever it is declared**, and the law-evaluation sites are
> unchanged in every module — patch 10→10, acc 7→7, velocity 3→3, tess 2→2,
> lodfeed 1→1. **Expect ALM-NEUTRAL.**

**So the value of this work is ONE STATEMENT OF A RATIFIED LAW, not area** — and
that is still worth having, because five copies of a law is five places for it
to drift. But **the number must not be claimed.** If the next fit moves on these
modules, that movement is **something to EXPLAIN, not something to bank**, and a
packet quoting an area win here would be quoting my error back at me.

**Three of TERRLAW's own instruments lied, all caught and recorded:**

1. **Static-linked mutants HUNG at zero user CPU — and a hang reads as "not
   fired".** This is `CLAUDE.md`'s alive-at-zero-CPU tell, in the mutant harness
   rather than in ctest.
2. **The verdict used a case-insensitive `-match`, so `"0 mismatches"` matched
   `"MISMATCH"`** — and it printed **"11 of 11 fired"**. A pattern that matches
   its own negation is the purest form of an instrument that cannot see its
   subject.
3. **Four of seven "mutations" were not mutations** — e.g.
   `{{9{h[15]}}, h[14:0]}` is identical to `{{8{h[15]}}, h}` across all 65,536
   inputs. **A mutant that does not change behaviour is a positive control that
   cannot fire**, and it had been counted as one.

Corrected to **7 real controls, 4 fired**, with the three silences named as
**reachability limits** rather than passes — and each module's header now records
what its differential does **not** cover. Naming an uncovered case is worth more
than a green.

## R204 — A GATE WENT RED ON LINE ENDINGS WHILE THE CONTENT WAS PERFECT

Post-merge, `packet_i_g8b_registration_static` failed with *"manifest hash does
not describe `fpga/rtl/terrain/zhao_terrain_patch_law_pkg.sv`"*, and the G8B
generator refused outright: *"G8B generator input is not checkout-stable LF
text."*

**Nothing was wrong with the content.** `git ls-files --eol` told the whole
story: **`i/lf w/crlf attr/text eol=lf`** — the index was LF, the attribute
demanded LF, and the **working copy was CRLF because the file was written before
the `.gitattributes` pin landed.** `core.autocrlf` did it; TERRLAW added the pin
(line 107) but a pin governs **future checkouts** and does not rewrite a copy
already on disk.

Re-materialising the file made the generator run and reproduce its output
**byte-identically** — `git status` clean afterwards, nothing to commit. The
gate hashes the **working tree**, so a line-ending difference in a file whose
content is correct reads as a manifest mismatch.

**Two things to carry:**

1. **`git ls-files --eol <path>` is the diagnostic**, and it is instant. A hash
   mismatch on a file you did not edit is a line-ending question before it is a
   content question.
2. **Adding an `eol=lf` pin does not fix the copies that already exist.** This
   is the `.gitignore` lesson again — *"making waste invisible to your tooling
   is not the same as removing it"* — in its line-ending costume: **the rule was
   added and nothing renormalised the tree.** Anyone holding a working copy from
   before the pin will hit this same red, and the content will be perfect.

## R205 — THE SAME LAW IN A DIFFERENT FORM IS NOT DUPLICATION

**TERRLAW's closing recommendation, taken.**

`zhao_terrain_field_walk` states §9.1's closed-interval footprint test — the
same law the package now holds — **in HOISTED form: z is tested ONCE PER GROUP
there, not once per lane.** Calling `covers()` from it would evaluate z **four
times** where the hoisted form evaluates it once: **a possible area regression,
in a block that is not even composed.**

**So it was deliberately left alone, and the package header now says so** — at
the exact place a reader will grep for the law and be tempted to finish the job.

**The distinction, which is worth the paragraph it now carries:**

> **The same law in a different form is not duplication. Two statements that
> must move together is duplication.**

These two *must* move together. So the header's note is not an excuse for the
exception — **it is the pointer that says where the second statement lives** if
§9.1 ever changes. An exception recorded without that pointer would be exactly
the hazard `CLAUDE.md` describes: a rule written down without the trap that
comes with it.

**The gate proved itself on this edit.** The package is in the G8B fit closure,
so a **comment-only** change turned `packet_i_g8b_registration_static` red on a
manifest hash mismatch until the generator was re-run. One hash line moved, zero
non-comment lines changed. That is a provenance gate behaving exactly as
designed, and it is worth saying plainly after R204 — where the *same* gate went
red for a reason that had nothing to do with content at all.

### And the evidence behind R176's factoring is NOT UNIFORM across the four modules

TERRLAW stated this in its own commit and it must not be flattened into "proven":

| module | corroboration |
|---|---|
| `zhao_terrain_patch` | **differential, 5 of 5 mutations fired** |
| `zhao_terrain_tess` | **differential** |
| `zhao_terrain_velocity` | **PARTIAL — 1 of 3 call sites reached** |
| `zhao_terrain_lodfeed` | **NOT corroborated by differential** — its deviation walk needs `fill_q == VERTS-1`, which random stimulus never reaches |

For the last two, the load-bearing evidence is the **byte-identical textual
proof** (comment-stripped, after one declared rename) **plus the committed
directed tests that do reach those states.** That is legitimate evidence and it
is a *different* kind from a differential — **and a summary that said "all four
verified by differential" would be false.**

**This is the `ruleViolations: []` shape from `CLAUDE.md`:** a uniform-looking
green across rows that were not all checked the same way. The packet declared
the unevenness itself, unprompted, which is why it is recorded here rather than
discovered later by someone quoting a differential that never ran.

## R206 — THE SWEEP AFTER R204: SEVEN MORE, AND TWO OF THEM FEED THE FIT

R204 ended with *"anyone holding a working copy from before the pin will hit
this same red, and the content will be perfect."* **That was a prediction, so I
swept for it.**

Of **506** files pinned `eol=lf`, **seven** had a CRLF working copy:

```
design/shell_fit_ports.yml
fpga/rtl/geometry/zhao_geom_skin.sv
fpga/rtl/geometry/zhao_geom_wcache.sv
fpga/rtl/particles/zhao_part_record.sv
tests/texture/texture_aux_div6_directed.cpp
tests/texture/texture_v3own_adversarial.cpp
tools/quartus/run_block_fit.ps1
```

**`tools/quartus/run_block_fit.ps1` is the fit script. `design/shell_fit_ports.yml`
is fit ports.** A fit here costs 1.5–4 hours, and it snapshots its sources —
so **discovering a checkout-stability refusal partway through one is the most
expensive place available to discover it.** All seven re-materialised; **`git
status` stayed clean throughout**, which is the proof the content was always
correct and only the working copies were wrong.

**`tools/maintenance/check_eol_worktree.py` makes it a ten-second question**
instead of a diagnosis from a hash mismatch. Two design points:

* **It refuses to list a DIRTY file for re-materialisation**, and says why:
  `git checkout --` discards unstaged work and unstaged work has no reflog. A
  maintenance tool that helpfully offers to destroy a colleague's edits is worse
  than no tool.
* **It must never be registered as a CI gate**, and its docstring says so. CI
  checks out fresh, so every working copy there is correct **by construction** —
  the check would pass forever **without ever having been able to fail**. That
  is this campaign's defining failure shape, and building it deliberately would
  be perverse.

**Proven in both directions before being committed:** RC 0 on the clean tree,
and RC 1 with a CRLF copy planted in a pinned file — which it also correctly
flagged **DIRTY, do not re-materialise**, because planting the fault made the
file dirty. Fault removed, tree clean.

### And I hit the pipeline-exit-code trap AGAIN, reading my own checker

I ran `python check_eol_worktree.py | head -8; echo "RC=$?"` and read **`head`'s**
exit status — printing `RC=0` for a script that had just correctly returned 1.

`CLAUDE.md` names this: *"Read the build's exit code, not the pipeline's."* I
have now done it **three times in this session**, including once while writing a
ruling about instruments that cannot see their subject, and once on a `git push`
where it briefly looked like the push had succeeded when nothing had been read
at all.

**It is worth recording as a habit rather than an incident.** The tell is that
`$?` after a pipe is *always* about the last stage, and the last stage is nearly
always `head`, `tail` or `grep` — which succeed. **So the trap fires exactly
when you are trying to read a long output, i.e. when something interesting is
happening.** The fix that actually works is redirecting to a file and reading
`$?` before touching the file, which is what every gate invocation in this
session's briefs now does.

## R207 — THE MUTANT SMOKE PASSED WHILE PRINTING NINE `%Fatal` LINES

**2026-09-20/21, UNTEX, found while adding a smoke form — and it is a defect in
the gate set this campaign has quoted all day.**

The smoke's mutant verdicts end in `$finish`. **`$finish` runs the named block
to the end of its time step**, and the `ifdef` chain's `else` arm ends at the
terrain verdict — so **every later production check still ran, against the
mutant.**

The new R197 arm therefore printed **nine `%Fatal` lines under a PASSING exit
code.**

**And the sentence that makes this a ruling rather than a bug report:**

> *"the slot-overflow arm only ever survived because production's geometry
> counters happen to agree with it."*

**`-Mutant` has been green all day for a reason that is a coincidence.** It is
in every gate list in every brief I have written, it is one of the six forms I
have required of eleven packets, and I have quoted its green in a dozen commit
messages and several reports to the owner. It was not measuring what I said it
measured.

**This is `CLAUDE.md`'s broken-instrument law at the top of the instrument
stack.** The mutant smoke is the thing that proves the *other* detectors fire —
it is the positive control for the positive controls. A false green there does
not just mislead about one counter; **it launders every "the detector works"
claim that rests on it.**

**Note which direction it failed in, because it is the usual one.** The extra
checks ran against a deliberately broken design and **passed**, so nothing went
red and nobody looked. Had they failed, the form would have gone red on day one
and been fixed in an afternoon. *"Nobody audits good news."*

**Fixed with `disable run;` after each arm's `$finish`, on both arms**, and
re-measured at zero `%Fatal` lines. All seven forms pass at `55451c42`.

**The second instrument defect in the same commit**, and it is the one that
would have wasted a day: `PC_CORE` / `PC_SHELL` knew only **one** wrapper
define, so a second wrapper mutant failed **47 hierarchical probes at
elaboration** — which **reads exactly like a broken core**. A packet meeting that
would have gone looking in production RTL. Both macros now list every wrapper.

**The rule to carry: when you add an arm to a shared verdict chain, check what
runs AFTER your arm.** A `$finish` is not a `return`.

## R208 — "DON'T CARE" WAS A LIE, AND MEASURING IT CHOSE THE DESIGN

The best engineering judgement in this packet, and it is the art law's shape in
a place the art law does not reach.

R197 says the untextured slot's content is **don't-care**. UNTEX checked whether
that was *true* rather than assuming it, and found it is not:

> `zhao_raster_tile_pipe_v2`'s `incoming_range_bad_c` **ORs every lane's
> `q_error_o`**, and `zhao_raster_attrgrad_v2` raises it when a lane's gradient
> divide is refused — **so arbitrary don't-care content in slots 1/2 COULD
> TERMINATE A FRAME** through lanes 1 or 2.

So a design that left the slots genuinely undefined would have shipped **a
constraint every future producer must remember and nothing would enforce** —
"you may leave these undefined, except not *those* values, and the failure is a
terminated frame two subsystems away."

**The fix makes the words true instead of making producers careful:** ATTRPACK
does not latch the slots when the bit is set, and feeds its shared attrsetup
core the zero operand, so the packed planes are the **null plane `{0,0,0}`** —
and `0 / 2A` never errors. **The content is now genuinely irrelevant.**

**This is the same class as R197's own constraint one level down.** R197 forbade
encoding absence as a value because a consumer cannot distinguish it from data.
R208 is the converse: **an absence that is DECLARED still has to be SAFE in the
slot it leaves behind.** Declaring "ignore this" does not make downstream logic
ignore it; only wiring a value that cannot fault does.

**And the schedule is untouched** — `planes == 3 × triangles` still holds, so
this bought its safety without a timing argument.

## R209 — THE PACKET CLASSIFIED ITS OWN DECISION AND INVITED DISAGREEMENT

UNTEX created a named constant at a producer seam — `GEOM_REPLAY_UNTEX_DECL = 0`
— and rather than quietly calling it "not a tie-off", it **stated the
classification, gave the reason, and named where to disagree**:

> *"None. `GEOM_REPLAY_UNTEX_DECL` is a named-constant declaration at a seam
> (R48's shape) and the TRUE value for the producer it describes; recorded at
> the parameter, the port map (`// REAL:`), the contract and here, **so the
> coordinator can disagree with that classification in the open.**"*

**I agree with the classification**, and the reasoning is sound: every REPLAY
triangle is a format-0 record, and format 0 carries u/v — so `0` (TEXTURED) is
not a placeholder for a missing producer, it is the correct value for a producer
that exists. A parameter rather than a `localparam` **specifically so a wrapper
mutant can flip it**, which is what made the positive control possible.

**But the behaviour is the ruling, not the verdict.** R159 exists because the
completion register cannot see an undeclared tie-off, and the mitigation I wrote
was *"declare it in the same commit."* **UNTEX went further: it declared
something it believed was NOT a tie-off, in four places, precisely so the
judgement could be overturned by someone else.** That is the standard — the
register's blind spot is closed not by a better parser but by a lane that writes
down the thing it could have left silent.

## R210 — A BLOCK WITH A CONTRACT AND NO `blocks.yml` ROW IS INVISIBLE TO EVERY GATE

**SEAMDIG, refusing I32.** The sharpest of its five measurements:

> **`TERRAIN.PAGEIO` has a written contract and NO `design/blocks.yml` row, so
> no gate can see it is missing.**

**This is the inverse of every instrument finding in this ledger.** All day the
pattern has been *a checker that cannot see its subject* — a regex without
`re.M`, a canary that is unsigned, a sweep that predates its producer. **This is
a subject that no checker has been told to look for.** `completion_register.py`
walks the ledger; a capability with no row is not *absent from the console*, it
is absent from the **question**.

So the console is missing a block that **three separate core entries depend on**,
and the count has never included it. **21 has never been wrong — it has been
answering a smaller question than anyone reading it assumes.**

**And it makes the next packet obvious and large:** one block closes `sc_*`,
layer D's two reads, **I27's deformation mark and I28's writeback** — *three
core entries under one owner*. **It needs a `blocks.yml` row first**, because
until it has one, building it closes nothing the register can see.

**SEAMDIG deliberately did NOT build the sheet arbiter**, and the reason is the
standard: *"it would add a disconnected block without closing anything, and its
policy between a live stamp and a bake read is a decision, not a wire."*
Eleven lanes have now declined to add a disconnected implementation to look busy.

## R211 — THE FORMAT FREEZE IS ENFORCED IN RTL, NOT MERELY RECORDED

`zhao_terrain_stampdepth.sv` carries **an elaboration guard that refuses
`SheetEdge != 64`, citing R194.**

**That is the right way to spend an owner decision.** R194 froze the terrain page
format at 64×64. A ruling in a document is a claim about intent that the next
parameterisation can silently contradict; **an elaboration guard makes the
decision unrepresentable.** It costs nothing in silicon — but note `CLAUDE.md`'s
warning that `--lint-only` does **not** execute `initial` blocks, so a clean lint
says nothing whatever about it. `check_quartus17_syntax.py` is what keeps it
synthesizable, and it passed.

**Three more things done right in the same block, all unprompted:**

* **No multiplier.** The `(b−a)*fr` interpolation is an explicit four-term
  shift-add, *"so nothing can spend a DSP inside the block that exists to hold
  exactly one."* On a device **39 DSP over**, that is the correct instinct
  applied without being asked.
* **The new mode is ADDITIVE, and it was MEASURED to be.** The sheet arm feeds
  v1's own scar arithmetic — clamp, rails and §3.4 meets **shared, not
  duplicated** (R176's lesson applied the same day it was learned) — and
  `terrain_bake_v2_directed` still passes **267/267 unchanged**. An unchanged
  count on the pre-existing suite is the evidence that a new mode did not quietly
  become a replacement.
* **The counter arrived with BOTH controls in one executable, by stimulus.**
  `sheet_vertices_dug_o` goes **0 → 255** on a sheet record at `cmd_radius_i = 0`
  — a radius at which the disc law *provably* writes nothing, so every moved
  height came from layer F — and **stays at 0** on a disc record carrying the
  same full sheet while that record still digs 109 vertices. No mutant owed.

### R175's rename paid off within hours

`zhao_terrain_bake_v2.sv`'s header cited `fpga/quartus/prod_fit_sources.txt`.
**That file no longer exists** — R175 renamed it to
`prod_fit_sources.ORPHANED.txt` precisely so a grep hit would carry its own
warning, after that file was misread four times in eleven days.

**It worked, and faster than expected:** the very next packet to read that header
found a citation to a path that is gone, and the new name told it why **in the
filename**. A stale pointer that fails loudly beats a live pointer to a file
whose banner nobody reads.

**One grep trap recorded for the next lane:** entry I32 cites `zhao_mem_share_n`,
which is real — but `find -name zhao_mem_share_n.sv` returns **nothing**, because
the module lives inside `zhao_mem_share2.sv`. **Grep for `module <name>`, never
for a file.**

### R207, VERIFIED INDEPENDENTLY BY THE COORDINATOR ON THE MERGED TREE

UNTEX reported the `$finish` defect fixed. **I re-ran both mutant forms myself
rather than quoting its green**, because R207 is precisely a ruling about
trusting a mutant form's exit code:

```
%Fatal lines across BOTH forms : 0        (was NINE on the R197 arm)
nonzero SMOKE_RC               : 0

-Mutant        MUTANT PASS -- terr_pl_slot_overflow_o fired 1 time(s)
-UntexMutant   geom_untex_refused_o=16 (want 16) clip_submitted=0
               setup_submitted=0 raster_pixels=0 matwin[unpub/underflow]=[0 0]
```

**The `-UntexMutant` verdict is stronger than "the counter moved", and that is
the part worth keeping.** It asserts four things at once:

* the counter fired **exactly 16 times**, the predicted number, not merely
  non-zero;
* **`clip_submitted = 0` and `setup_submitted = 0`** — nothing entered
  GEOM.CLIP, so the door refused *before* the pipeline rather than after;
* **`raster_pixels = 0`** — nothing reached the rasteriser;
* **`matwin[unpub/underflow] = [0 0]`** — the material window's accounting
  **held**, which was UNTEX's specific design claim: the refusal sits before the
  window's accounted span, so *"there is no fourth outcome for a triangle in
  that span"* survives.

**A control that proves the absence of four consequences is worth more than one
that proves the presence of a count.** The count alone would be satisfied by a
counter incremented in the wrong place.

## R212 — EVERY `quartus_map` IN THE TREE WAS DEAD, AND THE SYNTAX GATE SAID "NO REJECTED FORMS FOUND"

**2026-09-21, found by POSEABI when its first map run died, fixed and PROVEN
here.**

`quartus_map` aborts on **a SECOND `import` statement in a module header**.
Quartus 17.0 takes **one** item list there. And because `run_block_map.ps1`
compiles **every `.sv` under `fpga/rtl`**, that one file killed **every map, for
every block, tree-wide.**

**`check_quartus17_syntax.py` reported `no Quartus-17.0-rejected forms found`
throughout.** It knew three forms; this is a fourth. It is in every gate list in
every brief I have written, and thirteen packets have quoted its green.

**PROVEN, not asserted:** after the repair, `quartus_map` on
`zhao_field_loader` completes in **77.9 s, RC 0**, and writes its row. Before
it, the same command aborted. `CLAUDE.md` predicted the shape exactly — *"The
failure is fast (33 s) and loud, so it costs a fit rather than a day. The trap
is reporting 'lint: 0' as though it settled synthesizability"* — and Verilator
parses the rejected form without a murmur, which is that law's **fourth
exhibit**.

### The scale was reported as 41 files. Measured, the defect was TWO

POSEABI reported *"41 files use it"* and recommended leaving it alone as too
large to touch and outside its file set. **That was the right instinct about
scope and the wrong number, and the difference is the entire cost of the fix.**

Measured on the merged tree:

* **49 files** use the **LEGAL** one-statement form —
  `import zhao_pkg::*, zhao_abi_pkg::*, zhao_fb_tuple_pkg::*;` — including
  **`zhao_console_core.sv`, which has been through `quartus_map`**. The *form*
  is fine.
* **TWO files** used a second statement: `zhao_field_loader.sv` and
  **`tests/formal/formal_mem_refresh.sv`, which nobody had found.**

**A checker written to the over-broad reading would have turned 49 correct files
red** — R166's lesson, which is why the discriminator is *the second `import`*
and never the presence of one, and why `_MUST_NOT_FLAG` now asserts the console
core's own header must never fire.

### AND MY OWN FIRST PATTERN WAS DEFEATED BY CRLF — R173's shape, one hour later

The check I added **passed its self-test (12 fire / 19 no-fire) and reported a
clean sheet over the live fault it was written to catch.**

`scan_repo()` reads with `newline=""` **deliberately** — form 5 hunts a lone CR,
so universal-newline translation would erase the very thing it looks for. Real
files therefore arrive **with CRLF intact**, and my pattern's `;[ \t]*\n` could
not match across the `\r`. **The `_MUST_FLAG` strings are LF, so the canary
could not enter the blind spot it guarded.**

That is **exactly R173** — `duplicate_functions`' unsigned canary passing over
its signed blind spot — **reproduced by me, in the checker written to fix a
different blindness, within the hour.** The pattern now carries `\r?\n` and
`_MUST_FLAG` carries **the same fault spelled with CRLF**, so it cannot recur.

**The general form, and it is worth more than the fix:** when a scanner
deliberately reads raw bytes, **every pattern in it inherits that decision**.
One regex written against normalised text is enough to blind the tool, and its
LF-only self-test will certify the blindness.

## R213 — THE "113% OF CEILING" FIGURE IS A DIRTY FIT OF A DIFFERENT CHIP

**POSEABI, while sourcing a baseline for its own price. This corrects a number I
have quoted repeatedly, including to the owner.**

The `zhao_console_core@console-core-first-light` row that "~113% of the ALM
ceiling" comes from is:

* **47,582 ALM fitted on `5CEBA9F31C7` — NOT the target part.** The row's own
  field says so: **`notTargetDevice: true`**.
* from a tree with **`treeCleanAtHead: false`** — a dirty fit, which
  `CLAUDE.md`'s receipt chapter says describes no committed state exactly and
  whose digest therefore describes nothing.

**So every area decision quoting 113% has been quoting a dirty fit of a
different chip**, and the campaign's central constraint — *"ALMs are the binding
constraint"* — has been resting on it. The constraint may well still be real;
**the number supporting it is not evidence about `5CSEBA6U23I7`.**

This is `CLAUDE.md`'s own law twice over: *"Read `rtlCleanAtHead` first,
always"*, and *"never compare a current file to an old measurement."* The row
was honest — it **declared** both flaws in its own fields. **Nobody read them.**

**POSEABI did the right thing instead:** it produced **new standalone map rows
on the target device**, `zhao_geom_bonesrc@{sync-m10k,async-derived,async-flat}`,
and said plainly that its numbers are additive to whatever that baseline is.

### And the price inverted the ARRANGEMENT, not the packet

| `SRC_STYLE` | ALM | % of 41,910 | |
|---|---:|---:|---|
| `SYNC_M10K` | **830** | **2.0%** | built |
| `ASYNC_DERIVED` | 6,440 | 15.4% | |
| `ASYNC_FLAT` | **14,056** | **33.5%** | R90's arrangement |

**R90's bit count was exactly right and its conclusion was still wrong**, because
what nobody priced is **the ACCESS, not the storage**: a 32-deep mux over 549
bits costs **21,617 ALUTs against 17,665 registers**, and **Quartus infers NO
MLAB** for the async array — so the optimistic *"it becomes cheap LUT RAM"*
reading is false on this tool and this device.

**R90 warned the decoder's combinational contract might have to move and that it
"must not be discovered halfway through the packet."** It does not have to move:
`bone_idx_o` is a register that changes in exactly two places with a measured
**115.4 cycles** between, so a synchronous read is short by **one cycle and no
more**, and a 2-deep prefetch buys it with ~115 cycles of notice for a 7-cycle
fill. **The decoder is unchanged — not one port.**

**And POSEABI's own first probe was wrong in the flattering direction**, which it
reported: style 2 stored 576 bits/bone while still *deriving* `inv_rest`, so
Quartus pruned the 320 bits nothing read and style 2 **collapsed onto style 1**
— 6,421 against 6,440, *"which looked like a result."* Corrected, style 2 costs
**2.2× the first draft**. The rule it extracted is the keeper:

> **A store is only priced by what is READ out of it.**

## R214 — THE REGISTER WENT 21 → 22, AND IT IS THE BEST NUMBER OF THE CAMPAIGN

**2026-09-21, PAGEIO.** R210 said `TERRAIN.PAGEIO` had a written contract and no
`design/blocks.yml` row, so **no gate could see it was missing** — and therefore
*"21 has never been wrong; it has been answering a smaller question than a
reader assumes."*

**PAGEIO wrote the row first, before any RTL, and the count rose:**

```
before  21   (9 tie-offs + 12 disconnected + 0 unbuilt)
after   22   (9 tie-offs + 13 disconnected + 0 unbuilt)
```

**The campaign's headline number moved AWAY from its goal, and that is the
instrument starting to work.** A gap no instrument could see is not a gap the
console does not have.

**This is the exact inverse of the move rule 1 forbids.** All day the hazard has
been a packet making the number fall by hiding something — composing with
undeclared tie-offs, superseding a capability the owner ruled in, burying fifty
tie-offs to read 21 → 17. **Here a packet made the number RISE by declaring
something nobody had declared**, and it is worth as much as any close.

**Both traps were avoided deliberately**, which is why the row works at all:
the module is `zhao_terrain_pageio` with **no `_vN` suffix**, so `successor_in()`
can resolve it (R179 — a rival implementation is structurally invisible); it
carries **no `implementation:` key**, because `ledger_blocks()`'s own docstring
says that field is unusable; and `upstream:` is written **and labelled** as
design intent (R180).

**What this says about "drive the gap count to ZERO":** the goal is a statement
about the console, not about the integer. **An honest 22 is strictly better than
a 21 that could not see one of its own subsystems**, and the next such row will
move it again. The number is now measuring more of the machine than it was this
morning.

## R215 — THREE DEFECTS THE BENCH FOUND THAT NO COUNTER COULD SEE

PAGEIO's own RTL, found by its directed bench, and all three are shapes this
ledger has been circling:

**1. A READY THAT DOES NOT ACCEPT.** `sc_ready_o` was high in the cycle the cell
read won arbitration — so one scar word was lost per bake, silently. **It
presented as a refused bake** (`V_SHORT_B`), because the partial-plane guard then
correctly refused the page. **A correct verdict, 200 lines from the cause.**
That is `CLAUDE.md`'s *"a wrong diagnosis attached to a right alarm sends the
next person to reshape something that is already correct"*, in a handshake.

**2. EVERY COUNTER AGREED, AND THE PAGE WAS WRONG.** A `bake_done_i` pulse
aborted the last write's in-flight RMW: **1,023 of 1,024 cells landed.** The page
was written, the mark published, `done_ok` high, **and every counter agreed with
every other counter.** One wrong cell is one wrong breach decision — a player's
terrain permanently different from the reference — and **nothing in the design
could see it.**

This is the metadata-bank law (`CLAUDE.md`) in a new place: **counters that
balance perfectly because none of them looks at the field that moved.** The only
instrument that could catch it was a bench comparing against an oracle.

**3.** A write-beat prefetch indexing one word past the buffer — benign today,
out-of-range in a structure meant to infer M10K.

**The lesson PAGEIO's evidence actually supports:** its counters were not weak.
**Five of them were asserted zero on a clean bake and then fired by stimulus**,
and two more were fired by scratchpad mutants that reported **exactly 2 bytes of
layer A and exactly 60 of layer C — the counts the layout predicts**, which is
R95's standard (a control must fire *about* the fault it names, not beside it).
**A block can have excellent instruments and still ship a defect none of them is
shaped to see.** That is why the differential bench exists.

## R216 — BEFORE ESCALATING A DECISION, GREP FOR THE RULING

`design/contracts/TERRAIN.PAGEIO.md` §7 carries "decision 1" as an owner
decision. **It was already answered**, by ruling **T4**, in
`zhao_terrain_writeback.sv`'s own header: *"B and D are NEVER written back."*

**That makes four in two days**: R165 found two of I34's three blockers spent;
R190 found `{handle → hash}` answered by a producer that landed the same day;
DOSSIERCHECK struck four decisions as already-ruled, *"every one of those rulings
landing on 2026-09-20, the day the dossier was written"*; and now a contract's
own open question was closed in another block's header.

**The rule is cheap and it keeps being worth it: search the SUBJECT, not the
title, and search the RTL headers, not only `reports/`.** This tree records
rulings where the code is, and a decision can be spent by a file nobody thought
to open.

## R217 — A CONTRACT RECOMMENDED SOMETHING THE GUARD MAKES A VIOLATION

`TERRAIN.PAGEIO.md` §4 recommends byte-enables for edge handling.
**Structurally unavailable, measured:** `zhao_mem_guard` computes
`be_ok = (req.be == mask_of(req.len))` and gates every request on it; the arbiter
converts `len` to **words**; the SDRAM controller never sees a byte mask. **A
sparse `.be` is not an optimisation, it is a guard violation.**

**And the alternative is SMALLER than the contract feared.** Layer D's 17 bursts
are read anyway, so only **layer B's two edge bursts** cost extra: 19 reads, 52
writes. **The contract was pessimistic about the thing it should not have
recommended** — which is why the packet measured instead of either following it
or refusing it.

### The remaining seam is an OWNER decision, and it is not an arbiter

SEAMDIG called decision 5 an arbiter. **Measured against the real port, that
understates it by three items.** `zhao_surface_sheet`'s `req_*` is a
**control-and-read** port — `OP_ACQUIRE` / `OP_READ` / `OP_RELEASE`, a 32-bit
handle, a separate `pg_*` response stream with `ST_HIT` / `ST_MISS` — while bake
wants a **combinational** lookup. Missing: a **handle lifetime** (a leak costs
one of `Slots` = 2), a **latency adapter** (1,089 round trips per record, or an
8,192-byte second copy of layer F), the arbiter, and:

> **A LAW FOR `ST_MISS`, which is not an engineering question.** Fail the record
> — *the player's action is silently lost*. Dig zero — *a visible no-op*. Or
> fall back to the parametric disc — *a different shape from the one authored*.

**That is an owner decision with three player-visible outcomes**, and PAGEIO
built nothing for the seam. **Its recommendation — take the miss law first,
because the rest are cheap once it is written — is adopted.**

### And it declared a rule it brushed against

> *"that comment-only edit landed while the smoke was running and the core IS in
> its 219-source closure. The write was atomic and semantically null, so no form
> read a half-written file — but it was a rule I brushed against, and I made no
> further edits inside that closure afterwards."*

**Recorded because it was volunteered.** The live-tree rule exists because a
suite reads the working tree and a half-written file produces reds that look
real. A semantically null atomic write is very probably harmless — **and a lane
that reports brushing a rule, unprompted, is worth more than one that never
appears to.**

## R218 — `zhao_post_gather` IS COMPOSED. The owner's gather ruling is silicon

**2026-09-21, POSTGATHER. A real close: 22 → 21 on the merged tree.**

R195 ratified the law by looking; this builds it with **not one coefficient
moved and every one a PARAMETER**, which is what R37 demanded and what keeps it
editable afterwards. Three blocks: the tag law, R5's accumulator **unchanged**,
and the plane at **27 M10K** against I17's estimate of 30.

**The evidence is one-for-one against a number the packet did not produce:**

```
gather frags=2560 [untagged=2560 below_knee=0 lit=0 reserved=0]
       cells_flushed=160  plane written=160 oob=0  miss[gd/gg]=[0 0]
```

2,560 fragments against the raster's own `pixels=2560`; 160 cells is exactly ten
whole tiles; zero misses. **A count that matches an independently produced count
is worth more than a count that matches itself.**

**POSTMEAS's flush-address blocker — which R195 did NOT address — is solved and
cheaper than the proposal.** The tile origin is
`(fb_x − addr[3:0], fb_y − addr[7:4])`, a four-bit subtract on every beat, so
**there is no tile-start pulse and therefore no second thing that can be one
cycle out.** That is a design that REMOVES a failure mode rather than detecting
it.

**And it was fire-tested against the alternative:** a scratchpad copy with a
one-deep origin pipeline **failed 4 of 22 checks and passed the other 18.** The
sentence is the lesson — *a one-register store passes everything except the case
that discriminates it* — and it is why a suite's green says nothing until you
know which of its cases can tell two designs apart.

### DECLARED, NOT HIDDEN: the blur is not built

**R195 ratified two blur passes. Nothing performs them.** The glow reaches the
compositor **cell-quantised**. The packet wrote that into I17, the contract and
the ledger, and priced it: 23,040 cell-steps, ~14 M10K, and a shell post-lease
change.

**And `completion_register.py` REJECTED its first wording of that paragraph.**
The packet's own verdict: *"a phrase that could settle an entry must be a
declaration, not prose. It was right."* **The register refusing a form of words
is the INCOMPLETE block working as designed** — R159 said the fix for its blind
spot was a rule plus a reviewer, and here the tool itself held the line on how a
deferral must be written.

**I17 did not close.** Bullet (c) did; the HUD half needs an owner decision on
**153 of 553 M10K** that nobody has been asked for.

## R219 — WHY `untagged=2560` IS CORRECT, AND IT POINTS AT I20

**The packet's most important finding, and it is about something other than its
own block.**

The shell composes `zhao_raster_tile_pipe_v2`, where the fragment tag comes from
**`tri_continuation_tail_i[15:8]` — a boundary port with NO PRODUCER in this
console (entry I20).** So every fragment is legitimately untagged, and **the glow
cannot be exercised end to end until I20 has a producer.**

The bench cannot paper over it either: it **cannot carry a lit fragment without
breaking its own `$fatal` that the post pass is an identity.**

**So a green here is honest and incomplete at the same time**, and the packet
said so rather than letting `frags=2560` read as a working glow. It costed the
remedy and named its shape — a `-GlowTag` smoke form — without building it.

**This is the seam FORGESHADOW refused to close from four constants on
2026-09-20**, reappearing as the thing that gates a different subsystem's
evidence. **A boundary tie-off is not only a gap in the register; it is a hole in
what every downstream test can prove.**

### Three of its own instruments caught unflattering things

1. **The full-console smoke found a LIVE defect its block bench had missed.** The
   plane's lifetime was taken from the frame tick — and the post pass runs
   **823,547 cycles**, so the tick fires *inside* it: **582,252 misses on a
   complete plane.** Fixed by deleting the port.
2. **Its positive control failed itself.** The folded-law check was scored on
   **red**, where `tint = 255` makes the fold an identity — so it could not fail.
   Scored on all three channels: **1,221,632 differ.** A control run on the one
   channel where the transformation is a no-op is R173's canary in another
   costume.
3. **A sweep header claimed the full walk was too slow, so it sampled 4,094
   points. The full walk takes 2.3 seconds.** A performance excuse nobody had
   timed.

### And `-Mutant` went RED FIRST, which is the good outcome

31 errors, because the wrapper mutant needed the core's new ports. **The mutant's
own 18-day-old header promised it could not go stale silently — and this is the
first time that promise has been DEMONSTRATED rather than asserted.**

R121 and the stale-copy law say a committed control drifts in the flattering
direction. **This one drifted in the loud direction, by design.**

### One judgement I am recording rather than overriding

POSTGATHER did **not** write `blocks.yml` rows for its two new blocks, reasoning
that a row needs a contract and *"inventing one to satisfy a reporting tool is
backwards."*

**That sits in apparent tension with R214**, where PAGEIO's row made the register
rise and that rise was the point. **Both are right, and the distinction is
whether the capability is ALREADY SPECIFIED.** `TERRAIN.PAGEIO` had a written
contract and no row — the row made an existing, specified, absent thing visible.
POSTGATHER's two new blocks are **implementation detail of a capability that
already has a row**, and giving them their own would inflate the denominator
without naming a missing function.

**Declaring a gap is honest; manufacturing a ledger entry to have something to
declare is not.**

## R220 — TWO LANES BROKE THE SAME WRAPPER IN OPPOSITE DIRECTIONS, AND EVERY STATIC GATE STAYED GREEN

**2026-09-21, found by the coordinator running the seven smoke forms on the
merged tree. `-UntexMutant` returned RC 1 with 15 `%Error` lines while all
fifteen static gates read RC 0.**

**The cross product of two lanes landing in the same window:**

* **UNTEX** added `zhao_console_core_untex_decl_mutant.sv`, a wrapper that
  instantiates the real core with `.*` — so **every core port must exist by
  name in the wrapper's own header.**
* **POSTGATHER branched BEFORE that file existed**, then **added seventeen
  `gather_*` ports** to the core and **removed fourteen `post_gd_*` /
  `post_gg_*`** — entry I17's boundary tie-off, which it composed internal.
* It updated the one wrapper it could see. **It could not see the other.**

**So the wrapper was wrong in BOTH directions at once**: short seventeen ports
the core now has, and carrying fourteen the core had dropped.

**`mutant_copy_drift.py` returned RC 0 throughout, correctly.** R162 says why,
and said it a day early:

> *"a wrapper cannot drift in its BODY, but its PORT LIST can, and
> `mutant_copy_drift` is blind to that half BY DESIGN."*

It compares **commit order**. Commit order was fine. **The control simply would
not elaborate**, and nothing static could tell.

### The stale direction is the expensive one, and it is worth separating

A **missing** port reads as *"the mutant is out of date"* — annoying, obvious,
cheap. A **stale** port — one the wrapper still declares after the core dropped
it — produces `Can't find definition of variable: 'post_gd_cx_o'` **against a
name that used to be real**, and that reads as *"the core is broken."*

**I misdiagnosed it in exactly that direction for three tool calls**, assuming
ports needed ADDING and even writing a patch to add them, before checking
whether the core still had them. It did not. **The fix was to delete.** A lane
meeting this alone would have gone looking in production RTL for a regression
that does not exist.

### `tools/design/wrapper_port_parity.py`, registered as `wrapper_port_parity`

The half R162 named as missing, now built: it compares a wrapper's port set
against the real module's **in both directions** and reports `missing` and
`stale` separately, with the diagnosis attached to each — *"fix the WRAPPER,
never the module."*

**Proven in both directions before registration**, because a gate that has not
been seen to fire has not been tested: **RC 1 on a planted missing port AND on a
planted stale one, in the same run; RC 0 restored.** Clean state is
**1264 = 1264** on both wrappers.

**It carries the boundary of its own competence in its docstring**, which
matters more than the check: it compares two port lists **as text**. It cannot
see a width that changed, a direction that flipped, or a body that stopped
meaning what it meant. **It exists because those failures are loud and this one
is silent.** It is not a substitute for elaborating the mutant.

### And the general lesson about merge windows

Every packet in this campaign is briefed to keep its file sets disjoint, and all
of them did. **These two never touched the same file.** UNTEX created a wrapper;
POSTGATHER changed the module that wrapper mirrors. **Disjoint file sets are not
disjoint SEMANTICS**, and the coupling here is a `.*` in a third file neither
lane was looking at.

**That is a coordinator failure, not a lane failure**, and the mitigation is the
gate rather than better briefing — advisory prose loses, as `CLAUDE.md` already
knows from the fit hook. **Any file that mirrors another file's interface needs
a parity check, because the mirror breaks when the original moves and nothing
about the original's change looks wrong.**

## R221 — THE `ST_MISS` LAW: FALL BACK TO THE PARAMETRIC DISC, AND COUNT IT

**Taken by the coordinator under the owner's standing delegation (2026-09-20:
*"answer dossier questions yourself"*).** PAGEIO surfaced this while refusing
decision 5, and it is the same class: a decision with player-visible outcomes
that no packet may take quietly.

**The question:** when bake asks `zhao_surface_sheet` for a layer-F page and the
response is `ST_MISS`, what happens to the player's dig?

| option | what the player sees |
|---|---|
| **Fail the record** | the dig is **silently lost** |
| **Dig zero** | a **visible no-op** — they acted, the ground did not move |
| **Fall back to the parametric disc** | **a crater**, of the shape that shipped before sheets existed |

**RULED: fall back to the parametric disc, and COUNT the fallback.**

**Why, and it follows from two rulings already on the board rather than from
taste:**

1. **The disc is not an invention — it is the RATIFIED v1 LAW.** SEAMDIG
   measured that the sheet mode is **additive**: `terrain_bake_v2_directed`
   passes **267/267 unchanged** with the sheet arm present. So falling back
   reaches behaviour that is already ratified, already tested and already
   shipped. **R197's reasoning applies directly** — option B there was refused
   because it would have forced *inventing* an art law, and the same standard
   forbids inventing a third crater shape here.
2. **The other two make an absence look like a result**, which is W10 and the
   defect this campaign has spent two days finding. *"Fail the record"* loses a
   player's action with nothing to show for it; *"dig zero"* is precisely *"an
   absent output must not look like a zero result"* rendered in terrain. R168
   and R181 are the same shape in silicon, and both were live bugs.

**The fallback MUST be counted**, and that is not optional decoration: a miss is
a residency failure, and an uncounted fallback is a console quietly serving the
wrong crater shape with no way to know how often. **The counter owes a positive
control** — by stimulus if a miss is legally reachable, by a committed mutant if
it is not.

**What this does NOT decide.** PAGEIO measured that decision 5 is *not* an
arbiter and named four missing pieces; this ruling supplies **only the fourth**,
the miss law. **The handle lifetime, the latency adapter and the arbiter remain
engineering**, and PAGEIO's recommendation — *take the miss law first, the rest
are cheap once it is written* — is why this one is answered now.

## R222 — THE HUD STORE IS REFUSED AS POSED, AND THE NUMBER PUT TO ME WAS WRONG IN THE FLATTERING DIRECTION

**2026-09-21, coordinator, under the owner's standing delegation to answer the
dossier's questions.** Entry I17 asked for exactly one thing — *"put 153/553 in
front of the owner instead of inheriting the word SDRAM"* — and it was right to
ask, because the entry before it had eliminated on-chip memory by naming a
TECHNOLOGY rather than producing a NUMBER. **The question was well posed. The
number was not.**

### The arithmetic: 153 is unreachable, the floor is 180

The entry computed `384 x 240 x 17 = 1,566,720` logical bits, divided by an
M10K's 10,240, and got **153.0 — exactly, with no remainder.** *A perfect
division is a tell*, and this repository's own law says so: **precision at a
round number is a tell, not a result.**

A Cyclone V M10K cannot be 17 bits wide. Its aspect ratios are fixed, and this
tree records them twice independently — `reports/FIELD-PROGDIR-20260910.md`
(*"simple-dual-port aspect ratios 256x40, 512x20, ..."*) and
`reports/FORGE-CLIFF-BITMAP-RAM-20260910.md` (*"Cyclone V M10K port shapes:
256x40, 512x20, 1024x10, 2048x5, 4096x2, 8192x1"*). Against 92,160 words of 17
bits:

```
  256x40  ->  1 wide x 360 deep =  360 M10K  (65.1% of 553)
  512x20  ->  1 wide x 180 deep =  180 M10K  (32.5%)
 1024x10  ->  2 wide x  90 deep =  180 M10K  (32.5%)
 2048x5   ->  4 wide x  45 deep =  180 M10K  (32.5%)
 4096x2   ->  9 wide x  23 deep =  207 M10K  (37.4%)
 8192x1   -> 17 wide x  12 deep =  204 M10K  (36.9%)
```

**The floor is 180, and three different aspect ratios reach it independently.**
That robustness matters: this is not one packing guess that a fitter might beat.
**No configuration this device has can deliver 153.** The quoted figure understates
the true cost by 27 blocks — *5% of the entire device* — and it understates it in
the direction that makes the proposal look payable.

**This is plan 14.5 exactly, and the ruling being cited quotes it in its own
limit 3:** *"Physical M10K reserve must be measured, not inferred from logical
bit occupancy."* The entry inferred. It even flagged its own arithmetic as *"the
easy half"* — it was the wrong half.

### The citation was partial, and the omitted part is the governing part

I17 quotes `OWNER-RULING-M10K-CEILINGS-20260918.md`'s headline — *"Using some
more M10K is fine, we have enough, particularly if it saves ALMs"* — and its
19-M10K precedent. **It does not quote that ruling's three stated limits, and
two of them are directly on point:**

* **Limit 3** is the logical-versus-physical rule above, which the proposal
  breaks.
* **Limit 2:** *"It is not permission for full-frame lookup tables or port
  replication... The ruling raises a CEILING; it does not delete the
  ARCHITECTURE."*

And the headline's own mechanism does not reach this case. **The owner
authorised memory that SAVES ALMs** — *"they're our only weapon against our
massive ALM debt."* A HUD frame store saves no ALMs. It is new state. Spending a
third of the weapon on something that does not reduce the debt is the one use of
it the sentence cannot be read to license, on a console sitting at 47,582 ALM
against 41,910 **with FIELD's ~13,700 still additive.**

### RULED

**The full-frame on-chip HUD store is REFUSED at 180 M10K.** Not because 32.5%
is unaffordable in principle — *nobody can say, because no fit has ever measured
this console's M10K occupancy* — but because a number that has never been
measured cannot be spent, and because this particular spend is the wrong use of
the one slack resource.

**And SDRAM is NOT the fallback.** Refusing option A does not ratify option B.
Inheriting the word `SDRAM` is precisely what the entry was trying to stop.

### What was never priced, in the entry's own words

The entry names **three** structures and costs only one. Read its own text:

1. **a frame-resident store** — priced (wrongly), refused here;
2. **a line ring with backpressure** — *worked through and REJECTED*, with a
   real measurement: ten 32-row sprites need 320 lines of a 240-line frame.
   **That refusal is sound and stands.**
3. *"or a display list that can re-walk ONE SCANLINE across many descriptors,
   which is a different block from the one TWOD.SPRITE is"* — **named, and never
   costed at all.**

**The entry put the option it had a number for in front of the owner, and left
the option it had no number for as a subordinate clause.** That is not
dishonesty; it is what happens when one branch is arithmetic and the other is
design work. But it means the decision I was asked to take was a choice between
two of three candidates, with the third unexamined.

**So: price structure 3 before either expensive option is taken.** A bounded
band — B lines, double-buffered so rasterisation and scanout do not collide —
costs `384 x B x 17 x 2` bits, which is **48 M10K at B=32 against 180**, and the
sprite is re-entered per band by the display list rather than trickled one row
per composited line, which is the specific defect that killed structure 2.

**That number is MINE and it is shape arithmetic, so it is exactly the kind of
figure this ruling just refused.** It is offered as *a reason to do the work*,
not as a result. What it establishes is only this: **the gap between 48 and 180
is large enough that nobody should buy 180 without looking.**

### Why this does not leave I17 stranded

The HUD is mandatory v1 and this ruling closes nothing. **Refusing a structure is
not deferring a function**, and the rule against closing gaps by narrowing
applies to me as hard as to any packet. I17 stays open, at its full size, with
its remaining half now stated as *"the re-walkable display list is unpriced"*
rather than *"the owner owes a decision on 153 M10K"* — **a smaller and more
answerable question than the one I was handed.**

**What I am NOT deciding:** whether 180 would be affordable if measured. If the
band is priced and comes back worse than it looks, the frame store returns as a
live candidate **with a real occupancy measurement beside it** and the owner can
be asked properly. The refusal is of *an unmeasured spend justified by a
misquoted ruling*, not of the structure forever.

## R223 — ZERO IS UNREACHABLE WHILE TWO STANDING RULINGS STAND, AND ONE OF THEM IS MINE

**2026-09-21, coordinator.** The goal is *"drive the mandatory gap count from 61
to ZERO, then freeze that design and run the honest Quartus fit."* **The count
has a floor above zero, put there by owner rulings, and nobody has said so.**

This is not a proposal to lower the target. It is the arithmetic the target
implies, produced before a fit is scheduled against a condition that cannot
occur.

### The floor, item by item, with the ruling that creates it

**1. FORGE.SHADOW — owner-parked, and the register cost was ACCEPTED IN WORDS.**
R133's D-FORGESHADOW-B is a standing instruction: *"schedule no further
FORGE.SHADOW wiring packet."* Its reasoning is explicit about the consequence:

> *"Leaving it costs 1 on the register; composing it wrong costs a deadlock
> behind a closed gap."*

**The owner priced the register cost and took it.** That single sentence makes
the goal's zero unreachable, and it predates the goal.

**2 and 3. FORGE.PRIM and FORGE.PRIM_EVAL — deferred by R199, WHICH IS MINE.**
I ruled the forge program page kind deferred five hours ago, on reasoning I
still hold: four of the six forge families have no evaluator, so *"a page ruling
buys one of six"*, and freezing a format ahead of its consumers freezes it on
guesses. **I did not state the consequence at the time, and I should have: both
blocks are on the disconnected list and neither can close while R199 stands.**
A ruling that parks a blocker parks everything behind it, and the packet-facing
half of that was left for somebody else to discover.

**4. `zhao_measure_governor` — transitively parked by R133.** Verified in the
core's own text rather than inferred: its `cam0/1_scale_o` goes to TERRAIN.LOD
and `cam0/1_thresh_q8_o` to `zhao_geom_lodstate`, **and lodstate is inside
R133's parked subsystem** — that ruling names *"LODSTATE and SHADOW mutually
blocked and composable only together."* The core adds the trade plainly:
composing the governor today *"would move MEASURE.GOVERNOR out of the
disconnected list and dangle two output groups at this module's edge — the
register unchanged at best, and a gap closed by opening one."*

**So at least 4 of the 21 cannot close under current rulings.** The honest
statement of where this campaign can land is **17, not 0.**

### What I am deliberately NOT doing about it

`completion_register.py` has a `deferred_or_blocked` bucket that is **excluded
from the total** — line 1499 sums gaps, disconnected, unbuilt, uncited and
unresolvable, and not that one. **So I could move these four into it and report
17, or 0 with a little more of the same.** That mechanism exists and it is one
edit away.

**I am not going to, and the reason is R133's own sentence.** The owner wrote
*"leaving it costs 1 ON THE REGISTER."* He was not merely declining to wire a
block; **he was electing to keep paying for it in the visible number.**
Reclassifying it would overturn the accounting decision while quoting the ruling
that made it.

And it is the exact move this campaign has spent two days refusing. R214 put it
best — all day the hazard has been *"a packet making the number fall by hiding
something"*, and the answer there was a packet that made the number **rise** by
declaring what nobody had declared. **A reclassification dressed as compliance
is the same defect with better paperwork.** The number stays 21.

### The FORGE cluster has now been measured FIVE times for no movement

FORGE.SHADOW, FORGE.PRIM, FORGE.PRIM_EVAL and FORGE.CLIFF were *"refused
together"* and re-argued on 2026-09-19, re-measured by the forge4 packet on
2026-09-20 (**"ALL FOUR REFUSALS SURVIVE. Register 21 -> 21"**), and the
setupdoor packet was then commissioned to build the door the entry asked for and
found **"the arbiter is the SMALLEST of four blockers and closes none of the
other three"** — again 21 -> 21.

**Five passes, zero movement, and the fourth and fifth were commissioned by this
coordinator.** The refusals are not stale; they have been re-verified at the
current commit each time. **A sixth forge packet is waste, and I am recording
that here so the next slot does not get filled with one** — which, having read
the entry and seen "four disconnected blocks in one subsystem", is precisely
what I was about to do.

**FORGE.CLIFF is the one exception and it is engineering, not a ruling:** the
rivalry is decided (R142, adopt `zhao_forge_cliff_ram`, 5,698 ALM) and the
capability is still absent — no page issuer, no solid-window producer, no vdist
master, *"all three re-searched at this commit and all three still absent."*
That is a real build, and a large one.

### What this changes about the fit

**Nothing about whether it runs — everything about what triggers it.** *"Fit at
completion only"* (R200) is a standing owner instruction and holds. But
completion cannot mean zero while R133 and R199 stand, so the fit gate needs
restating as one of:

* **fit at the floor** — every gap closed except the ones with a cited standing
  ruling, which is 17 and is a condition that can actually be met; or
* **reverse R133 and/or R199** — ~~the owner's call, not mine. R133 is the
  owner's own instruction and only he can spend it~~; R199 is mine and I will
  reverse it the moment the evaluators it waits on exist, which is the
  condition I wrote into it.

  **CORRECTED 2026-09-21 by R226, and the struck sentence was wrong in the
  direction that made this ruling sound more final than it is. R133 IS NOT THE
  OWNER'S.** It is a `## R133` prose section, coordinator-authored on
  2026-09-20 from packet FORGESHADOW, and its own first line says so — *"I
  wrote R132 accepting ENGINE1's escalation."* Only **R1–R7** carry
  `(owner, explicit)`. **So the quoted "leaving it costs 1 on the register" is
  the coordinator's reasoning, not the owner pricing anything**, and *both*
  rulings that put a floor under the gap count are coordinator-provisional and
  revisable under the owner's standing delegation. See R226.

**I recommend the first**, and I am not treating that as decided. What I am
doing is refusing to schedule a fit against a number that cannot occur, and
saying so now rather than at hour three of a Quartus run.

## R224 — TAGPROD'S TWO DECISIONS ARE ONE, AND IT IS THE CMD EXECUTOR GAP AGAIN

**2026-09-21, coordinator.** TAGPROD docked two owner decisions from its I20
refusal: *allocate an effect-tag field in the ABI*, and *the vertex colour /
stencil reference pair*. **Both were well found and neither is the decision it
looks like.** Under R216's rule — *grep for the ruling, search the SUBJECT not
the title, and search RTL headers and CONTRACTS, not only `reports/`* — the
answer was already on disk in three places.

### The structural fact that settles it

`tri_continuation_tail_i` is 48 bits:

```
  vertex_rgb        [47:24]   24
  vertex_alpha      [23:16]    8
  effect_tag        [15:8]     8
  stencil_reference [7:0]      8
```

`reference/include/zref/zref_fragment.hpp`'s `struct Frag` — *"one shaded
candidate as RASTER.EARLYZ hands it over"* — carries `vr, vg, vb` (24),
`va` (8), `tag` (8, commented **"the constant-tag source"**) and `sten_ref` (8,
**"stencil reference AND REPLACE value"**). **Twenty-four, eight, eight, eight.**

**The tail IS the reference model's per-triangle constant group, field for
field and width for width.** That is not a coincidence and it is checkable in
one read.

**So the ABI concept is not missing — it is RATIFIED, in the reference model,
for all four fields.** What has no producer is the hardware path that delivers
these constants per draw. **That is one gap, not two, and it is not an ABI
design question.**

### Why "allocate a field in the ABI" is the wrong remedy, measured

TAGPROD proposed `MaterialRecord.flags` bits 3-15, `raster_state[31:2]`, or
`DrawForm.flags` bits 4-15 as reserved room. **The fragment state word has no
room at all.** `State::pack()` allocates every one of its 32 bits:

```
[0] z_test_en   [1] z_write_dis  [2] z_force_far  [4:3] blend
[5] shade_mod   [6] alpha_mod    [7] atest_en     [15:8] atest_ref
[17:16] sten_func [19:18] sten_op [20] tag_write_dis
[21] tag_from_texel  [23:22] tag_channel   [31:24] sten_mask
```

**`[31:24]` is `sten_mask`, not spare.** So the *"raster_state[31:2] has no v1
consumer"* reading needs re-checking against this packing before anyone builds
on it — either the RTL's `raster_state` is a different word from `zref`'s
`State`, or the claim is about consumers rather than allocation. **I am flagging
it rather than resolving it, because I did not measure the RTL side.**

**And the selector is already built on both sides.** The contract
(`RASTER.FRAGMENT.md`) lists `TAG_CHANNEL = GLOW` as part of `sun_additive`'s
*state*, and TAGPROD found the RTL selecting on `tri_fragment_state_i[21]` and
`[23:22]` — **exactly `tag_from_texel` and `tag_channel`.** The machinery that
would *use* a constant tag exists. Only the value's delivery does not.

### RULED

**No new ABI bits are allocated today, and the question is re-docketed where it
belongs: the per-draw constant path, which is the CMD executor gap that entries
I14 and I30 already describe.**

Three reasons, in order of weight:

1. **The values are already ratified** — `Frag`'s four fields. Allocating a
   *second* home for a quantity the reference model already defines is how this
   tree got two projectors. **Before building a carrier, read the contract of
   the block that consumes the same quantity** (`CLAUDE.md`, uncashed-cheque
   check 3).
2. **This is the inverse of R199 and must not be confused with it.** R199
   deferred the forge page kind because *the consumer did not exist*. Here the
   consumer **exists and is proven**: `zhao_post_gather` is composed, and
   TAGPROD's `-GlowTag` form lit **1,062 fragments and 1,344 bloom cells against
   0/0 plain**. So the case for building is stronger — **which is exactly why
   it should be built once, in the right place, rather than twice.**
3. **Two lanes would choose different bits.** CFGARM is measuring I14's executor
   right now and I17's descriptor gap is *"the SAME gap I14 and I30 already
   describe"*. A tag field allocated independently here is a third answer to a
   question one lane is already holding.

**What is NOT deferred:** `sun_additive` cannot bloom until this lands, and
`RASTER.FRAGMENT.md` is explicit that its tag is *constant, not from a texel
index* — *"the sun quad's texture is 64x64 ARGB4444, direct colour, with no CLUT
index to read a strength from."* **That is a player-visible absence with a
ratified cause**, and it is now attached to the executor that will fix it rather
than floating as an unowned ABI question.

### And the vertex colour half is ALREADY RULED, in a contract

*"No provoking-vertex law exists anywhere"* is true as a search result and the
wrong frame. **`design/contracts/FORGE.SHADOW.md` already records the
disposition**, quoting the core back at itself: the vertex colour *"is left at
its constants deliberately rather than invented"*, and building the tail from
four constants would be *"moving a tie-off from a port into the core, which
closes a gap on the register while changing nothing in the silicon."*

**That is R48's pattern, already applied:** a named constant at a seam, held
until a ratified format supplies the value. It is the fourth time in three days
a decision has been found already spent — R165 (two of I34's three blockers),
R190, DOSSIERCHECK's four, R216's T4 — and **the third found in a file that is
not under `reports/`.**

**The provoking-vertex question becomes real only when something produces
per-vertex colour into this port.** Nothing does. Asking it now would freeze a
convention ahead of its producer, which is the same error R199 refused from the
other end.

## R225 — I ALMOST TURNED A GATE RED ON THREE RUNNING PACKETS, AND THE MEASUREMENT THAT STOPPED ME COST TWO GREPS

**2026-09-21, coordinator.** After the configure repair I swept the mutant
directory for the same class of defect. **The sweep produced a dramatic,
confident, wrong answer, and the only reason it is not in the tree is that I
checked one case by hand before acting on it.**

### What the sweep said

`mutant_copy_drift` reports *"57 copies checked … 16 files matched no
production module"*, and `wrapper_port_parity`'s `PAIRS` list holds **two**
entries. Enumerating with the tools' own index and regex: **90 mutant files,
57 copies checked, 14 wrappers, 16 unmatched.** So twelve wrappers looked
uncovered by R220's gate.

Running R220's own `ports()` against all fourteen pairs said **8 of 14 would be
RED** — missing and stale ports across the texture subsystem.

**Eight undetected port-list defects is exactly the finding this campaign has
been rewarding**, and I was one edit from extending `PAIRS` and committing it.

### What was actually true

**Every single "MISSING" name contained `_valid_`.** That is too systematic to
be drift, and it is the tell that stopped me — *a defect does not select for a
substring*.

Two greps settled it:

```
tests/mutants/zhao_texture_frag_expand_v2_mutant.sv:16
    input logic frag_valid_i, output logic frag_ready_o,
```

**TWO PORTS ON ONE LINE.** `_PORT` anchors the captured name to end-of-line, so
it sees `frag_ready_o` and never `frag_valid_i`. Every phantom "missing" port
was the *first* of a pair sharing a line.

And the second grep killed the premise outright: **that file contains no `.*`
at all.** It binds explicitly — `.frag_valid_i(frag_valid_i)`. R220's tool
exists for `.*` wrappers, where *"every port must exist by name in the
wrapper's own header"*. **Applied to an explicit-map wrapper it is not a weak
check, it is a check of the wrong proposition.**

**So all eight reds were false, and committing them would have turned a gate red
on three running packets over nothing** — the precise inverse of the failure
this tree fears most, and worse in one way: a green that hides a defect is
quiet, while a false red *burns three lanes' time and teaches them to skip the
gate.*

### And the gate it is actually installed on is sound — measured, not assumed

```
console core header, shared-line ports : 0
zhao_console_core_untex_decl_mutant, uses of `.*` : 5
```

**Zero shared-line ports and a genuine `.*` binding**, so for its two pairs the
parser's blind spot cannot fire and `1264` is the true count. **R220's gate is
correctly built and correctly aimed. It is simply narrower than its filename
suggests**, and that is a documentation fact, not a defect.

The twelve others fail a different way and **fail LOUDLY**: an explicit map that
misses a new production port is a `PINMISSING` at elaboration — *which is
exactly what broke the configure tonight.* They do not need a text gate; they
need to be elaborated, and they are.

### The one GENUINE hole, and it is latent rather than live

`tests/mutants/zhao_geom_group_seq_mutant.sv` declares its module as
**`zhao_geom_group_seq` — production's exact name.** `mutant_copy_drift` matches
`copy_module.startswith(production + "_")`, which **can never match a name that
EQUALS production**, so the file is counted `unmatched` and **skipped silently.
It is a copy — zero instantiations of the real module — with no drift check of
any kind.**

It is **not stale today**: both files were last committed at `1f5ac60a`, the
same commit. **It is unwatched, which is a different and more patient problem.**

It also breaks the convention that exists to prevent a second failure: every
other mutant here is *"renamed so no source list can elaborate it by mistake"*.
**A file under `tests/mutants/` declaring a production module name can shadow
production in any source list that globs.**

**I am not fixing either today.** Three packets are running and
`mutant_copy_drift` is in all three gate lists; changing its matcher mid-wave is
the live-tree hazard, and the rename touches a file POSEPAGE is working beside.
**Recorded so it is scheduled rather than rediscovered** — and recorded with the
false alarm above it, because the sweep that found the real hole is the same
sweep that nearly committed eight fictional ones.

### The rule this earns

**A finding that is uniform is a finding about your instrument.** Eight
independent defects do not all contain `_valid_`; eight parse failures do.
`CLAUDE.md` already says *"check the heuristic against a case you can verify by
hand before believing the total"* — this is the first time in this campaign that
rule caught something on the way OUT rather than on the way in, and the cost of
obeying it was two greps against a total I had already written down.

## R226 — HUNDREDS OF SITES SAY "OWNER RULING" FOR A COORDINATOR ONE, AND I DID IT TO MY OWN R223 TWO HOURS AGO

**2026-09-21. Found by CFGARM while checking its own citation of R28** — which
is the detail that makes it worth a ruling: **the lane was verifying a claim it
had itself written**, and found the defect underneath it.

### The measurement, and it has grown tonight

CFGARM reported **223 mis-attributed sites** across `fpga/`, `reference/` and
`design/`, **62 in the console core.** At this head, split properly:

```
  "owner ruling R<n>" total          360
  legitimately R1-R7                  67
  MIS-ATTRIBUTED (R8 and up)         293
```

**The two figures measure different things and both are right.** CFGARM counted
only the mis-attributed ones, at `aea45c4a`, several merges back; my first pass
counted *all* citations including the 67 correct ones, which would have
overstated the defect by a fifth. **The honest number is 293, and it went UP
tonight — some of the increase is mine.**

**CFGARM's own sweep is worth quoting on this, because it nearly shipped the
opposite error:** its first tree-wide count returned **zero**, because a nested
`-match` clobbered `$Matches` before the id was captured. It caught that *"only
because a precise zero is a broken instrument until proven otherwise"* and
re-ran with a positive control (81 coordinator ids, R1 correctly excluded).
**A lane auditing citation hygiene nearly published a citation-hygiene number
produced by a broken instrument.**

**The boundary is exact and the file states it in its own preamble:**

> *"Items marked **(owner, explicit)** were chosen by the owner directly. The
> owner then said 'go with your recommended answers for now and don't stop to
> quiz me', so items marked **(provisional, coordinator's recommendation)**
> stand until the owner revises them."*

**Seven rulings — R1 through R7 — are the owner's.** 9 lines carry the marker,
37 carry `(provisional, coordinator)`, and there are 91 table rows plus some
thirty prose sections. **Everything from R8 up is a coordinator recommendation
standing under a delegation.**

### AND R223 IS WRONG, IN THE DIRECTION THAT MADE IT SOUND FINAL

Two hours ago I ruled that zero is unreachable, and wrote:

> *"reverse R133 and/or R199 — the owner's call, not mine. **R133 is the owner's
> own instruction and only he can spend it.**"*

**R133 is not the owner's.** It is a `## R133` prose section dated 2026-09-20,
written by the coordinator from packet FORGESHADOW's findings, and **its own
first line says so**: *"I wrote R132 accepting ENGINE1's escalation."*

So the sentence I built R223's conclusion on — *"Leaving it costs 1 on the
register; composing it wrong costs a deadlock behind a closed gap"* — is **not
the owner pricing a cost and accepting it.** It is the coordinator's reasoning,
recorded in a file whose title says "Owner rulings". I read the title and not
the preamble, and then told the owner that only he could lift it.

**R223 is corrected in place rather than deleted**, because the correction is
the useful artefact.

### What this changes, and what it does not

**CHANGES:** the floor under the gap count is **coordinator-made, not
owner-made.** Both R133 and R199 are provisional recommendations standing under
*"go with your recommended answers for now"*. **So zero is not blocked by the
owner's instructions — it is blocked by two coordinator judgements, one of them
mine, and both revisable by me under the same delegation that created them.**
The fit gate is a question I can answer, not one I must escalate.

**DOES NOT CHANGE:** whether those judgements are *right*. R133's engineering
content is untouched by this — FORGE.SHADOW really is a subsystem (LODSTATE and
SHADOW mutually blocked, composable only together, plus a client-A widening that
re-authors a ratified law), and the cluster has gone **21 → 21 five times**.
R199's content is untouched too. **I am correcting an authority claim, not
overturning a decision**, and the distinction is the whole point: a provisional
ruling can be *argued with on the merits*; an owner instruction can only be
obeyed or escalated. **Mislabelling the first as the second removes the
argument.**

### This is VIEWMASK's finding one level up, and that is why it recurs

Hours ago VIEWMASK found that entry I21's controlling premise traced to **the
core's own commentary**, not to any ruling:

> *"A caution invented in a comment and cited by its neighbours is
> indistinguishable from a ruling. Six passes quoted the comment back as
> ratified law"* — and it outranked ratified spec for five weeks.

**Here the same disease has climbed a level: a coordinator ruling cited as an
owner ruling is indistinguishable from one.** Both are citation-laundering, and
both run in the direction that makes a claim harder to question. **Three hundred
and sixty sites is not a slip; it is a convention nobody chose.**

### What I am doing about it

**Not a tree-wide rewrite.** 360 sites is a mechanical edit across files three
packets are holding, and the phrase binds a packet identically either way.

**What changes is the escalation path, and it goes in every brief from now:**
*when an entry says "owner ruling R<n>", check that ruling's own row before
treating it as unliftable — only R1–R7 are the owner's.* Two of tonight's briefs
already carry it. **And I stop writing "owner ruling R<n>" for my own rulings**,
which I did all evening for R220, R222, R223 and R225.

The owner's file *"has already struck this twice as a one-lane slip"* (CFGARM).
**It is not a slip and it is not one lane. Recording it as a convention defect
is the only way it stops being restruck.**

## R227 — SCALE THE GATE SET TO THE CHANGE. My brief was spending three hours to re-prove that comments do not simulate

**2026-09-21, coordinator.** Two lanes reported comment-only work and then
settled in to run **all eight smoke forms each**. With three lanes contending on
one machine, FIELDLANE measured each form at **10-15 minutes** — so the pair was
committed to roughly **three hours of machine time**, while slowing the third
lane, to establish something a filter settles in one second.

**The filter, run on both branches:**

```
git diff <base>..<branch> -- '*.sv' | non-comment, non-blank lines
  gz/fieldlane   0      (zhao_cmd_exec, zhao_field_host, zhao_console_core)
  gz/projout     0      (zhao_console_core, +212 lines, all comments)
```

**Verilator cannot produce a different simulation from a comment.** Eight forms
were going to re-derive `raster pixels=2560` seven more times each.

### This is my error and it has a name in my own memory

*"Exhaustive validation is an AI failure mode; set a risk-based budget and stop
when the acceptance question is answered."* The brief mandates eight forms
**unconditionally**, which is precisely the default it warns about. **The cost
was invisible while lanes ran alone and became visible only under contention** —
so the defect had been in every brief for the whole campaign and nothing
surfaced it until the machine was busy enough to notice.

### The rule

**A gate earns its runtime by being able to change its answer.** Ask what class
the change is in, then run the gates whose SUBJECT that class can affect:

* **Comment-only RTL** → the static gates, `packet_h_tieoff_audit`,
  `completion_register.py`, and **`-LintOnly`**. Nothing else. **And those four
  are not ceremonial here** — the tie-off audit's subject *is* comments, and
  TAGPROD proved `completion_register.py` will register a **phantom gap** from a
  wrapped line that merely begins with an entry number. A 212-line comment
  insertion is exactly the shape that springs that trap.
* **RTL behaviour changed** → the full eight, as before, with `%Fatal` grepped
  from the logs rather than inferred from exit codes (R207).
* **Ports changed** → add `gen_prod_top`, `gen_console_board`,
  `wrapper_port_parity`, and both `gen_shell_paired_diff` forms. **Tonight
  proved why:** seven `gth_*` ports were added and a mirroring mutant did not
  follow, which aborted `cmake --preset` for every lane.
* **A new block** → its directed bench must BUILD AND RUN (R60), and any counter
  owes a fired positive control (R95).

**Three caveats, because the shortcut has edges and each one is real:**

1. **Uncommenting code is not a comment-only change**, and the filter catches it
   — restored code appears as an added line that is not `//`.
2. **A Verilator pragma lives in a comment.** `// verilator lint_off` changes
   behaviour, which is why `-LintOnly` stays in the minimal set. And
   `CLAUDE.md` records that `// synthesis translate_off` does **not** make
   Verilator skip a block, so a guard in there is live in simulation.
3. **The filter only strips `//` lines**, so an edit inside a `/* */` block
   reads as non-comment and forces the full set. **That errs toward running
   more, which is the safe direction for a shortcut to fail in.**

### What this does NOT license

**Not "skip the gates".** Both lanes' static gates, tie-off audit and register
were run and green, and form 1 was run in full. **The claim is about the
SEVENTH re-run of a form whose input did not move**, not about the first.

And note the asymmetry that makes this worth writing down rather than just
doing: **an over-run gate costs hours and looks like diligence**, so nobody
audits it — the same blind spot as a green that hides a defect, wearing the
opposite costume. `CLAUDE.md`'s broken-instrument law says *nobody audits good
news*; this is its twin, **nobody audits thoroughness.**

## R228 — A PER-VERTEX QUANTITY IS COMPUTED, CARRIED FOUR BLOCKS, AND DELIVERED AS A CONSTANT NOTHING DRIVES

**2026-09-21, PROJOUT, entry I13. Register 22 → 22, comment-only, nothing
built — and it is the most valuable lane of the campaign.**

### The finding

**GEOM.CLIP slots 3–6 have no reader anywhere in `fpga/rtl`.** ATTRPACK packs
three planes; the shell has three plane ports; the tile pipe joins three lanes.
And the chain behind that is worse than a spare slot:

> **GEOM.LIGHT computes per-vertex colour, VATTR stores it, GEOM.CLIP carries it
> in slots 3–5 — and ATTRPACK DROPS IT.** What reaches `frag_vert_rgb_i`, whose
> own port comment reads *"interpolated, lit, tinted and already-fogged vertex
> colour"*, is `post_earlyz.vertex_rgb` ← `job_meta_i[345:298]` ←
> **`tri_continuation_tail_i`, a per-triangle CONSTANT with no producer.**

**The consumer's port comment describes a quantity the console computes and
never delivers.** A reader checking whether lit vertex colour exists finds the
computation, finds the carrier, finds the port comment, and would reasonably
conclude it works.

**This is `CLAUDE.md`'s uncashed-cheque chapter in its purest form** — *"a thing
BUILT is not a thing INSTALLED"* — except the deferral was never even written
down. Nobody decided to drop it. **It is commissioned as packet ATTRLANE, and
the two possible answers are opposites: either the computation is dead ALM to
reclaim on a console 5,672 over its ceiling, or the delivery is severed and
should be rejoined. The packet is forbidden from assuming which.**

### And it corrects R197, which is mine

R197 refused its option B partly because that option *"forces a terrain
texture-coordinate law, which is art content and the owner's to author."*

**The law exists and is frozen.** `reference/src/zrender/terrain.cpp` carries
`u_top[k] = wx[i] >> top_shift` with its own comment *"pitch = 2^k metres → u =
wx >> k"*, and `zhao_texture_mosaic.sv` cites `spec/terrain_rules.md` §6.2 as
**FROZEN 2026-08-16, capture-exact.**

**R197's MECHANISM is untouched** and still unblocks FORGE.SHADOW/PRIM. **Its
premise was over-scoped**: there was no art law to author for terrain, because
terrain's was authored five weeks earlier. **Recorded rather than quietly
narrowed** — R197 stands as a mechanism and its stated reason is wrong for one
of the two subsystems it covers.

### The citation lesson, and it explains SIX passes of blindness

Entry I13 claimed *"a terrain texture-coordinate law does not exist in this
tree"*. **The search was honest. Its SCOPE was `fpga/rtl/terrain/**`, written
one line above a claim about THE TREE** — and `reference/` is in the tree.

> **A search that NAMES ITS SCOPE reads as more rigorous, not less.**

That is the whole mechanism. A bare "I looked and it isn't there" invites
challenge; "I searched `fpga/rtl/terrain/**` and it isn't there" reads as
disciplined work and **nobody checks the gap between the scope and the
conclusion.** Six passes read past it.

**So: when you search for an ABSENCE, state the scope AND ask what is outside
it.** This is the twin of R225 — there a uniform result exposed a broken
instrument; here a *well-documented* instrument produced a conclusion wider than
its own aperture.

### Three more results, each measured

* **I13 and I14 do NOT share a producer** — my premise, tested and rejected.
  `u_geom_clip.tri_valid_i` is `cl_in_valid = mw_t_valid && !cl_in_refuse_c`.
  One chain, no CMD record, no executor.
* **The merge blocker STANDS and is LARGER than the entry claims.** *"A day's
  work once (b) exists"* is false: `u_material_window` sits **in** the stream, so
  a second producer owes three unwritten obligations — it must fire `d_enter_i`
  or `occupancy_q` underflows, is clamped to zero, and **`drained_c` reads TRUE
  with triangles in flight, so the interlock's own drain condition becomes a
  lie**; it must carry `{material_set, material_id}`, which **terrain does not
  have — zero hits in `fpga/rtl/terrain/`** because terrain textures through the
  mosaic; and `err_unpublished_o`'s premise is falsified by any second door.
  *"This is not an argument against R187 — it is the unpaid bill for it."*
* **`invw24` stands; only its COSTING was wrong.** `zhao_geom_depthquant_stream`
  is tag-through over `NSLOT = 16`, so a second client is an arbiter plus a
  demux, **not** a second instance. Costing it as a copy would have repeated the
  projector, and `TERRAIN.PROJECT.md` already holds that receipt.

### And it corrected ITSELF twice, leaving both wrong versions in place

It mis-cited the underflow control, and its **first owner recommendation was
half wrong**: it nearly recommended terrain's colour on the flat `base_rgb`
seam, then found `spec/terrain_rules.md` §6.5 — quoted inside
`zhao_texture_aux.sv` — saying **"tint moved to vertices"**, with the oracle
calling its per-cell tint *"the FLAT stand-in for the Gouraud tint."*

> **Recommending the flat route would have ratified a stand-in that names itself
> one.**

**Both corrections sit BESIDE the wrong versions rather than replacing them**,
which is the practice this ledger wants and rarely gets.

**No mutant was owed and it checked before writing one:**
`err_occupancy_underflow_o` is already fired by stimulus at
`material_window_directed` case 7, asserted exactly 1, with its own cancellation
control alongside.

### The method note, which produced three of the four findings

> **Every one came from asking "who READS this?" rather than "does this
> EXIST?". The entry's own searches were all the second kind and all honest.
> Reading a layout is not reading a reader — and it is the cheaper search, which
> is presumably why it keeps being the one performed.**

**That sentence is the campaign's most transferable result** and it is now in
every brief.

### One bound it could not settle, stated as a bound

The mosaic's consumer is `zhao_texture_island_v3_top`'s `u_mosaic`, **which this
core does not instantiate** — so *"terrain runs textured"* is **ratified intent,
not composed fact.** Whoever builds the UV producer confirms the consumer's
residency first.

## R229 — A RE-MEASUREMENT LANE'S FLATTERING DIRECTION IS INVERTED, and POSEPAGE caught itself in it

**2026-09-21, POSEPAGE, entry I29. Register 21 → 21.** Five commits, one of them
an empty commit whose message *is* the findings document.

### The self-catch, and it is a new shape

Six lanes tonight were sent to re-measure inherited blockers, and five found
expiries. POSEPAGE found none — and nearly reported one anyway:

> **"I nearly filed a rot that does not exist.** My first count gave 198 against
> the entry's 219 — the instrument was `Measure-Object -Line`, which is not a
> line count. **A re-measurement lane's flattering direction is *finding* rot,
> not missing it.** Caught before writing."

**`CLAUDE.md`'s broken-instrument law says the defect always makes the answer
look better, smaller or simpler, because nobody audits good news. That is true
of a BUILDING lane. For an AUDITING lane it is exactly backwards:** its good
news is a *discovery*, its flattering error is a **false positive**, and the
campaign has spent all night rewarding exactly that discovery. **I have been
paying a bounty on found rot for six lanes.**

So the rule needs its mirror stated: **when your job is to find defects, a
found defect is the claim to check hardest.** `zhao_sdram_model.sv` is **219
lines, exactly as cited**, instantiated four times. Nothing had rotted, and that
is a real result.

### What POSEPAGE established about I29

**The entry's own plan would not have closed it.** A page reader plus a sixth
requester gives I29 real bytes **and still no statement of which frame.**

**Blocker (a) — REMOVED, by authoring the half nobody had reached.** The claim
in `zref_creature_page.hpp` that *"a kind-9 frame reader has a layout to read
and needs no lift"* is **true of a FRAME and false of a PAGE**: `creature_rules`
§2.1 freezes frame *contents*, while §5 sketches the container in one clause
with **no magic, version, offsets or alignment**. *A reader cannot read a frozen
frame it cannot locate.* Authored under **R90 item 1's own unreached half** —
`zref_clip_page.hpp`, `tools/pack/mkclipbank.py`, a 704-byte golden, and two
gates **both FIRED** (byte 200 incremented → 2 FAILED, RC 1, naming that byte;
restored, both green). Frame bytes unmoved. **The golden is
`geom_bonesrc_directed`'s own six-bone fixture, so both goldens describe one
creature** — which is how a format freeze avoids becoming a second source of
truth.

**Blocker (b) — `pose_requests` has no carrier, and that is the whole remaining
gap.** `DrawForm 0x0300` carries no animation state; `clip_id`, `frame_no` and
`type_id` have **zero occurrences** under `fpga/rtl/geometry/`; there is no
pose or clip command at any opcode.

### And the deferral was named EIGHTEEN DAYS before the entry was written

`ZHAOZHOU_ANIMATION_HPS_RESIDENCY_ARCHITECTURE.md` — **owner-ratified
2026-09-03**, scope *"the GEOM.POSE memory seam"* — rules the reader's shape in
§10.1, explains in §6 why it must fill whole, and in **§4.2 says in terms that
"the exact command-record representation is deferred."**

**That deferral IS blocker (b).** I29 has never cited the document. Found by
R216's rule — *search the SUBJECT, not the title* — and it is now the **fifth**
decision this week found already answered somewhere nobody opened.

## RULED — D-POSEPAGE-A: take `DrawPosedForm` at 0x0305

**Ratified as recommended**, and it spends §4.2's deferral rather than inventing
anything.

**The four facts were checked rather than assumed, which is why this is
rulable:** 0x0304 **is** spoken for by W04 and not yet in the zidl; the **abi
version does NOT bump** — the "new opcode bumps" reading is superseded five
times over (PublishResource, SetPost and others); **the bytes already reach
VRAM**, since `PublishResource` carries `u8 kind`, **which narrows this decision
to "which frame" alone**; and **`sub` is not padding** — the pose cache's own
header says omitting it returns the wrong palette.

**Why the per-draw form over the `SetPose` state alternative**, which was raised
fairly on SetPopulation's precedent: **pose varies per creature.** R13's clause
is the governing analogy — *"the job port is not widened to carry a
subpatch-uniform value that is not true"* — and a pose placed in STATE is that
same error, a per-draw quantity in a carrier that says it is uniform. A
`SetPose` before every `DrawForm` is the same byte count with an ordering hazard
added. **And `DrawForm` keeping its meaning as bind pose is a clean split that
breaks no existing content.**

**Why this is not R199's trap.** R199 deferred the forge page kind because *the
consumer did not exist* — four of six families had no evaluator. **Here both
ends exist**: `zhao_geom_bonesrc` and `zhao_geom_pose_decode` are built and
tested, and the clip page format is now frozen with a golden and two fired
gates. **The command is the only missing link, which is the R224 situation, not
the R199 one.**

**What this does NOT settle, stated so nobody over-reads it:** §4.2's
request-side representation — *a validated resident handle with generation and
epoch, not a naked slot and frame* — is a **different layer** and remains
undetermined. **Ratifying the command does not by itself enable the RTL
reader**, and POSEPAGE was right to build no reader: a second uncomposed block
beside `zhao_geom_bonesrc` is *"BUILT, INSTALLED NOWHERE"*.

### Two process findings worth keeping

**`RC=-1` with a log that stops mid-sentence is a KILL, not a result.**
`-UntexMutant` returned `RC=-1` from a background task, truncated right after
*"--- smoke run ---"*, with no error text and no process alive — R81's
external-kill signature. Re-run in the foreground it passed with
`geom_untex_refused_o` firing 16 times. **Read a truncated log as a kill before
reading it as a failure.**

**And the paired-diff repair is confirmed installed by an independent lane:**
POSEPAGE verified `packet_h_paired_diff_mutant_fresh` **is now registered in
`build/tests/CTestTestfile.cmake`**, and noted that its own two new ctests are
model-only `add_test`s with **no `verilate()` above them**, so neither can be
silently skipped the way that one was.

## R230 — THE LIT COLOUR IS SEVERED, NOT DEAD. Refused on COST, and the decision needs a LOOK

**2026-09-21, ATTRLANE, entry I13. Register 22 → 22, 213 insertions, zero
deletions, all comments.**

### The verdict

**SEVERED DELIVERY.** ATTRLANE walked the chain by hand rather than grepping a
layout: `zhao_light_stream` → `geom_light_*_o` → `zhao_geom_vattr` (latched into
`c_rgb_q` and **counted** by `colours_written_o`) → `rep_data_o` → GEOM.REPLAY →
**slots 3, 4, 5** → `zhao_geom_clip`, **winding-swapped with the corners** →
`zhao_geom_attrpack.tri_attr_*_i`, **where it arrives full and stops.**

**The lighting is NOT dead silicon, and the proof is elegant: GEOM.CLIP performs
a winding swap on those slots that exists for no other purpose.** A block does
not swap what it does not carry. **Reclaiming it would be removing function, and
that was refused** — which is the answer I wanted the packet to be able to reach
and forbade it from assuming.

### It corrects R224, which is mine

**PROJOUT framed the destination as EITHER slots 3–5 OR the continuation tail's
`vertex_rgb`, and R224 acted on that framing by docketing the tail constants to
the CMD executor. They are the same path.** In
`zhao_raster_tile_pipe_v2`'s attribute `always_comb`, `continuation_w` is
assembled **per fragment**, three lines under `attr_join_q_q[1]` and `[2]`.

**So `vertex_rgb` needs no separate I20 producer.** With lanes 3–5 present that
line becomes a field build off `attr_join_q_q[3..5]` and **nothing downstream of
the tile pipe changes.** R224's disposition of the *other three* tail fields
stands; its `vertex_rgb` clause does not. **The error ran in the direction that
made the work look like two jobs.**

### R89 does not transfer, and that must not be inherited

R89 refused a fourth plane for alpha because the value *"does not vary across
the primitive."* **True of shadow alpha, false of lit vertex colour.** R133 drew
the right line for alpha — *"the plumbing is finished, only the faucet is
missing"*, and the faucet was unbuildable. **Here the plumbing is finished AND
the water is already in the building.**

**This is refused on COST, not on absence.** The next lane must not inherit
*"R89 settled it."*

### The cost, and it caught its own error in the flattering direction

**~1,420 ALM and +24 DSP** for three lanes (488.1 / 463.6 / 467.8 ALM and 8 DSP
per lane, from section 17 of `zhao_raster_texture_v3_fit_top@g8a.fit.rpt`, truth
device, `rtlCleanAtHead: true`). Plus `METAW` **1157 → 1877**, taking the
binner's metadata bank from **29 forty-bit slices to 47** — M10K *and*
per-triangle bank time.

**It first wrote +9 DSP, from the `ATTR_DSP3=1` variant — G8A characterization
only; the console runs `ATTR_DSP3=0`.** Nearly **3× understated**, caught by
reading the production row by hand. **+24 DSP is 21% of the entire 112-DSP
budget**, on a console `BUDGET_HEATMAP` already puts at **185 DSP against 112**.

### And a third built-but-uninstalled stage, found on the way

**`zhao_raster_toon` takes `r_i`/`g_i`/`b_i` as `signed [31:0]` — exactly
`attr_join_q_q[]`'s shape — with `zhao_raster_fog` behind it. Neither is
instantiated by any composed top**, and `prod_manifest.yml` says so outright.
**A whole per-pixel colour stage is authored and waiting on the far side of the
severance.** That settles "subsystem" over "wiring job" and it is deferred
intent, not new scope.

## RULED — the three lanes are NOT taken today, and the decision goes to a LOOK

**+24 DSP on a device already 73 DSP over its ceiling is not a spend I will make
on a coordinator's judgement.** But **I am not deciding it from the DSP table
either**, and `CLAUDE.md`'s first law is why:

> **Measurement never trumps actually looking at things.** *"Component checks
> passing is not likeness evidence… Look at the whole thing, in motion, against
> the concept."*

**Gouraud versus a flat stand-in is a VISUAL question**, and this campaign has
spent the night proving that a number offered in place of a judgement is how
wrong values become unadjustable. **A DSP column is exactly such a number here.**

**And the look is FREE.** The oracle already computes both:
`creature_sim.cpp` carries `gouraud = a.lit && b.lit && c.lit`. **No silicon, no
fit, no commitment is required to produce the comparison** — which is precisely
the *"author by eye, render, look, compare"* loop the art law asks for.

**So: render both readings from the reference model, at final resolution, and
put them in front of the owner.** Commissioned as packet **GOURAUDLOOK**. If the
flat stand-in holds up at 240p, it ships **named as a stand-in** — which
ATTRLANE has already made true in the RTL at both ends of the severance, so the
next reader cannot mistake the slots for spare. If it does not hold up, the
owner has a real cost to weigh against a real picture.

**What is NOT deferred:** the severance is now *named in the RTL at both ends*,
and `GEOM_CLIP_ATTRS` stays at its width. ATTRLANE re-affirmed PROJOUT's refusal
to narrow it, **so the "four unread slots" finding still cannot be mistaken for
spare silicon.**

### Two things ATTRLANE declined, both correctly

**A hardware drop counter — refused because the ORACLE already answers it free.**
Spending silicon on an over-ceiling device to measure a quantity the reference
model computes is the wrong trade, and saying so is better engineering than
building the counter.

**And it started a leaf fit, then stood down**, because `PACKET-PROTOCOL.md`
says *"Do not run Quartus"* and **my brief said "price it in ALM before
building."** The two conflicted and **the protocol won, which is the right
precedence.** *The conflict was mine* — a brief may not quietly license what the
protocol forbids. The number was already on disk, which is where a price should
be looked for first.

## R231 — THE DEPTH LAW WAS ALREADY RATIFIED, AND THE THING I COMMISSIONED LAST NIGHT IS THE OPTION IT REJECTS BY NAME

**2026-09-21, TERRCMD, entries I27/I32. Register 22 → 22.** Found while
measuring something else entirely, which is how the expensive ones arrive.

### The defect, verified in my own tree rather than taken on report

Four pieces, each checked:

1. **`design/contracts/SURFACE.STAMP.md` S3** — *"`stamp_results` carries
   `{texel, tag, strength_after, strength_before}`. `TERRAIN.BAKE` turns stamps
   into layer-B height16 scars and **needs the DELTA, not just the new
   value**; sending `before` costs eight wires and **saves BAKE a second read
   port onto the sheet**."*
2. **The same clause rejects the alternative BY NAME** — *"**Rejected:**
   emitting only the new value and letting BAKE re-read — a second reader on a
   store whose whole rate budget is one texel per clock."*
3. **`zref_terrain_page.hpp`'s `stamp_depth_at_vertex` returns
   `stamp_depth(strength[...])`** — a function of the CURRENT sheet strength
   alone. **Absolute, not a delta.**
4. **`zhao_terrain_bake_v2.sv`: `scar_sum = h_scar + delta16`** — the scar
   **ACCUMULATES**.

**An absolute depth added to an accumulating scar double-digs.** A stamp
re-issued at the same place digs the full depth again; two stamps overlapping
in one frame both dig the accumulated sheet. Under the delta law the same
re-issue correctly digs **nothing**, because op 0 replaces and `before ==
after`.

**And `zhao_terrain_sheetseam` — which I commissioned as packet SHEETSEAM
yesterday and merged — IS the second reader the contract rejects.**

### Why no instrument could have caught it

`sheet_vertices_dug_o`, `fallbacks_o` and `prefetch_beats_o` **all describe a
perfectly healthy read of a sheet that is telling the truth.** The fault is not
in the answer; it is in **which question is asked.** Every counter measures the
answer.

**This is `CLAUDE.md`'s metadata-bank law in its purest form** — *"counters that
balance perfectly because none of them looks at the field that moved"* — and it
is the second time this campaign has produced it. R215 found the same shape in
PAGEIO: 1,023 of 1,024 cells landing, *"the page written, the mark published,
`done_ok` high, and every counter agreeing with every other."*

### THIS IS NOT AN OWNER DECISION. The contract already ruled it.

TERRCMD offered it as **D-TERRCMD-A** and recommended the delta. **I am not
taking it as a decision, because S3 is RATIFIED and decided it already** —
including rejecting the built branch by name, with a **committed mutant
(mutation 8: *"`stamp_results` loses the pre-blend strength (the delta BAKE
needs)"*)** standing guard over it, and with `surf_res_before_o` — **the very
port entry I32 is named after** — existing for this and nothing else.

**RULED: take the DELTA.** It is the only idempotent option, it is what
`surf_res_before_o` was mutation-tested to deliver, and it restores §9.2's
deferral identity. Cost, recorded *before* the decision as it should be: **one
more 1,089-byte M10K half in the seam.**

**R216's rule, for the sixth time this week: the decision was already spent, in
a file nobody opened.**

### And the brief that missed it was mine

`CLAUDE.md` says, in as many words: *"before building a block, read the contract
of every block that consumes or produces the same quantity."* **SHEETSEAM's
brief pointed it at PAGEIO's and SEAMDIG's findings and at §9.3's laws. It did
not point at `SURFACE.STAMP.md`, which owns the other half of the same
quantity.** Nor did R221, which I wrote about the same seam.

**SHEETSEAM did excellent work inside the frame I gave it** — 81 directed
checks, a fired miss counter on three routes, a latency price 7.5× better than
the brief's. **The frame was wrong at its edge, and that is a coordinator
failure, not a lane failure.** It is the same shape as R220: *disjoint file sets
are not disjoint SEMANTICS.*

**Nothing is deleted and nothing is narrowed.** The seam is repaired to carry
`before`, which is what S3 always said it should carry.

### Three more results, and one of them is the campaign's premise collapsing

**`cmd_*` WAS NEVER ONE ABSENCE, and three lanes forwarded it as one.** It is
**thirteen fields. Nine are already live on the core's wires** — `cx`/`cz` from
`cmd_exec_stamp_tx_w`/`_ty_w`, `radius`, `src_id`, the four `env_*` port-for-port
from `u_surface_dispatch` under R45, `dual` from `tps_v_flags`, `patch_id` from
the dispatch key. **Four are absent — `depth_from`, `depth_to`, `cells`,
`depth_sheet` — and each is a DECISION, not a wire**, which is exactly why no
amount of composition work ever reached it.

**My TERRCMD brief's premise — "two entries behind one wall" — is false.** I27
and I32 **do not need the same thing**: `terr_chk_*` waits on
`zhao_terrain_devstore`, and *a `cmd_*` producer would close it **never***.

**And my C4 boundary was satisfiable all along:** `job_handle_i` need not be
synthesised from `cmd_patch_id_i`, because **`stamp_patch_o` IS the ABI's
`handle32[patch]`**, already composed as `cmd_exec_stamp_patch_w`. The value is
present and validated. **C4 is satisfied by CARRYING it, not deriving it.**

### Two rots found, and one sought and honestly NOT found

**I32 calls `TERRAIN.PAGEIO` "NOT BUILT" with "NO `design/blocks.yml` row", in
two places.** It is **63,729 bytes**, at `blocks.yml:2377`, and it consumes
`sc_*` and serves layer D — which I32 says have no consumer and no reader.
**Three of I32's four listed holds are spent, and two lanes had already
inherited "PAGEIO does not exist" from those sentences.**

**And R229 working as intended:** I27's `terr_chk_*` waits on
`zhao_terrain_devstore` composing. The file exists and greps **eight times** in
the core — *"reads exactly like a composition."* **All eight are comments.** The
blocker stands.

> **"I expected to file an expiry and did not."**

**That is the sentence R229 asked for.** A re-measurement lane's flattering
direction is *finding* rot, and this one looked, found the grep that would have
justified the claim, opened the hits, and reported the boring truth.

### And a SECOND brief error from the same lane: the smoke list is short by two

`run_console_core_smoke.ps1`'s `param()` block declares **ten** switches.
`-SkipVerilate` is a modifier, so the forms are **nine plus plain = TEN**:

```
plain  -Mutant  -UntexMutant  -NoTableLoad  -BadDescriptor  -BadVertex
-NoEchoArm  -BadTraceArm  -GlowTag  -LintOnly
```

**Every brief I have written says EIGHT**, omitting **`-NoTableLoad`** and
**`-BadDescriptor`**. So a lane changing RTL behaviour and dutifully running
"all eight" **leaves two controls unrun while reporting a complete sweep** —
and one of the two is `-BadDescriptor`, the inverted control I had just sent a
correction about.

**This is the same shape as the silently-accepted flag, one level up: the LIST
and the SCRIPT disagreed, and only the script is authoritative.** A hardcoded
enumeration in a brief is a copy, and *a copy goes stale in the flattering
direction* — here it reads as a **complete** sweep.

**So the instruction changes from an enumeration to a DERIVATION**, and it goes
in every brief:

```
awk '/^param\(/,/^\)/' tests/prod/run_console_core_smoke.ps1 \
  | grep -oE '\[switch\]\$\w+'
```

**Three brief defects in one night, all mine, all found by lanes:** the gate
list missing `--check --mutant` (which hid a broken configure), the
unconditional eight-form mandate (R227, three hours of contended machine time),
and now the eight-form list itself being wrong. **The common cause is that I
wrote down a snapshot of a thing that moves.**

**And TERRCMD justified its own zero rather than asserting it**, which is the
standard: *"`-LintOnly` is not an inverted control — it elaborates and never
runs the console, so it has no verdict arm to die in. Zero `%Fatal` is the
correct expectation there, not an assumed one."* It gave three independent
confirmations its flag had bound: the verdict line is `-LintOnly`-specific, the
plain run's markers (`raster pixels=2560`, `frames_admitted=1`) are **absent**,
and RC was 0 rather than the **2** the repaired script now returns for an
unknown argument.

## R232 — I29's BLOCKER (b) IS DISCHARGED, and a fifth fact turned a policy into a mechanism

**2026-09-21, POSECMD. Register 22 → 22** — it had already moved before the lane
started, and the lane said so rather than claiming it.

**All four of R229's facts held.** And a **fifth, which the ruling did not
state, decided the design**: `DrawPosedForm`'s first sixteen payload bytes can
be made **byte-identical to `DrawForm`'s**, so CMD.EXEC reads **six of nine
fields through the EXISTING `OFF_DF_*` constants**, pinned by eight per-field
elaboration guards.

**That turns "DrawForm keeps meaning bind pose" from a POLICY I asserted into a
MECHANISM the layout enforces.** My ruling reached the right answer for weaker
reasons than the ones available.

**Built:** 48 bytes, appended per R171 — **zero of the 24 existing goldens
changed, measured not assumed.** `zhao_cmd_decoder` needed **no hand edit**
because it reads the generated size table, **and the test proves it**: a 0x0305
packet returning `ZH_ABI_OK` is impossible if the opcode were unknown. Directed
test **850 checks, RC 0** (from 762); `abi:check` clean across 34 outputs.

**The design point is the one this campaign has been circling.** The pose rides
the **same `draw_valid_o` beat in the same `dq` entry — one enable, so
`CLAUDE.md`'s metadata-swap skew is not REPRESENTABLE.** And **R13's clause
SELECTS this shape rather than forbidding it**: it forbids widening a job port
for a *uniform* value, and pose is per-draw. A refused `clip_id` **degrades to
bind pose rather than dropping the draw**, because refusing the record whole
would make a creature vanish.

**Fire test B reproduced the metadata bank exactly: draw 0 came out wearing
draw 3's key under stall.** The defect `CLAUDE.md` opens with, produced on
demand in a new block.

**Wrapper parity 1270 = 1270** — six names added and BOTH mutants updated. R220
handled without being reminded.

### And a FOURTH brief defect: two of the three generators live elsewhere

> *"Your brief's path for `gen_shell_paired_diff.py` is wrong — it's
> `tools/design/`, not `tools/quartus/`. At the brief's path it returns RC 2
> 'No such file'; **a lane reading exit codes records a failing gate that was
> never run.**"*

Verified: `tools/design/gen_shell_paired_diff.py`, but
`tools/quartus/gen_prod_top.py` and `tools/quartus/gen_console_board.py`. **I
listed all three together by bare name in the ports clause, which invites
inferring one directory for three.** This is the Wave-4 defect returning — *a
gate that cannot be found exits non-zero, and that is NOT a failing gate.*

**And the `%Fatal` correction I sent two lanes was short by one.**
**`-NoTableLoad` is inverted too** — it and `-BadDescriptor` **both pass WITH
one `%Fatal`.** I named only one of them.

### Three instrument findings

1. **The stale-binary trap fired on a RESTORE.** Content verified, `git status`
   clean, and `ninja: no work to do` served mutant B's nine failures **as a
   false red**. Exactly `CLAUDE.md`'s `Copy-Item` timestamp warning, met live.
2. **The smoke bench could not verilate — all ten forms RC 1 in ONE SECOND.**
   *The duration gave it away before the exit code did.*
3. **The two console-core mutant wrappers have DIFFERENT LINE ENDINGS**, one
   CRLF and one LF, **so a scripted patch silently misses one.** A live hazard
   for exactly the R220 repair every port-changing lane now performs.

### What remains, stated honestly

**I29's consumer is untouched and that was correct** — the page reader, the
sixth mem-adapter requester, `form → type_id` resolution, the instance walk,
and `zhao_geom_ladderbank` still uncomposed. **§4.2's resident-handle layer has
no ruling and the next lane meets it immediately.**

**And the lane declared its own unpaid bill:** this block **has not been through
`quartus_map` on this branch.** Lint is 0 and the Quartus-17 syntax gate passes,
**which settles one tool's opinion** (R212). A fit is owed at the next subsystem
boundary, and its question is named: **the ALM cost of `DRAW_W` 144 → 185 across
`DRAW_Q` entries, plus four core ports.**

## R233 — THE HUD BAND IS 12 M10K, R222's PREMISE WAS FALSE, AND I MADE THE VERY ERROR I HAD JUST RULED AGAINST

**2026-09-21, HUDBAND. Zero files changed. Register 22 → 22.** A packet that
built nothing and settled the entry.

### The arithmetic, done properly and cross-validated twice

For an L-line store at 384 wide × 17 bits (`hud_rgb_i[15:0]` plus
`hud_valid_i`, read off the port list), the floor over all six Cyclone V aspect
ratios is exactly **`ceil(0.75 × L)`**.

**It was checked against two numbers this tree stated before the lane arrived,
which is what makes it trustworthy:**

* **L = 240 → 180**, reproducing R222's own table *including* its 360 / 207 /
  204 rows;
* **L = 9 at 16 bits → 7**, which is **POST.COMPOSITE's contract figure** for
  its nine-line ring.

**A formula that reproduces an independently authored contract number is worth
more than one that only reproduces itself.**

**The band needs L = 16 → 12 M10K.** And **the 17th bit is free at every L** —
depth binds, width is slack at 512×20 — so there is no reason to reach for a
transparent colour key.

### My 48 was the right number by the REFUSED METHOD

R222 called for pricing a band and offered **~48 M10K at B=32**, flagging it as
shape arithmetic. **The number is reachable** — B=32 double-buffered is L=64,
and `ceil(0.75 × 64) = 48`. **But the route I wrote down,
`384·B·17·2 ÷ 10,240`, gives 40.8**, which is **logical bits divided by an
M10K's capacity — the exact inference R222 had just refused, three paragraphs
earlier, as the thing that made 153 unreachable.**

**I ruled against the method and then used it in the same ruling.** The lane
caught it, and the distinction matters: *"right number, refused method"* is how
a bad practice survives a correct answer.

### AND R222's PREMISE WAS FALSE. The measurement existed.

R222 refused the frame store partly because *"no fit has ever measured this
console's M10K occupancy."*

**`reports/OWNER-DECISIONS-20260920.md`, dated the day before, line 14:**

```
zhao_console_core@console-core-first-light,  47,582 ALM / 151 DSP / 306 M10K
```

**R222's closing sentence cites 47,582 ALM — from that same row — while
declaring the M10K absent.** One row, two columns, **one quoted and one called
missing.** The same document is already reasoning with it at line 766: *"against
306 of 553 already used."*

**That is the broken-instrument law in the flattering direction, committed by
me:** "nobody has measured it" is a more comfortable refusal than "it is
measured and here is the number."

### The correction STRENGTHENS the refusal, which is why it is safe to record

Every caveat on that row — dirty tree, FIELD absent, *"do not quote it as the
console's size"* — **understates**. So **306 is a FLOOR: usable to REFUSE, never
to LICENSE.**

* floor + FIELD's two clean leaf rows = **344 / 553**
* the double-buffered frame store — **the form that actually works** — is
  **704 / 553. Over the device.**
* and it **cannot coexist** with the 185-M10K TERRAIN deviation store the same
  dossier puts to the owner.

**And the row ran on `5CEBA9F31C7`, not the target part**, so its printed
percentages are wrong by **2.2×** — a trap for anyone quoting the percentage
rather than the count.

## RULED — take the BAND, and this one is mine to take

**This is an engineering choice with a decisive measurement, not a visual
question.** The HUD looks identical under all three structures; only the cost
and the schedule differ. **So unlike R230's Gouraud question, no picture is
owed and no owner ruling is required.**

```
                      M10K        frame cost        verdict
  frame store (2x)    704/553     fits             OVER THE DEVICE
  line ring           ~12         133% of frame    cannot draw a HUD
  BAND (B=4, L=16)      12        11.1% of frame   TAKEN
```

**Structure 2's defect was never the buffer.** Structure 2 costs
**Σ heights × a line-time = 122,880 clk = 133% of frame**; structure 3 costs
**Σ areas = 10,240 clk = 11.1%**, a **9× margin**. What killed it was
**TWOD.SPRITE holding `busy_q` for one whole descriptor — descriptor-major
order.** *The two can use the identical memory.* **A structure rejected for the
cost of its memory was actually rejected for its walk order, and the note said
"ring".**

**~595 ALM, zero DSP**, from ~700 flops counted structurally times the
**measured** 0.849 ALM/register from the console's own fit row. **The sort or
Y-bucket I anticipated is not needed** — a 64-descriptor extent re-walk is 4.2%
of frame at B=4. The cursor needs **no multiply**, and the scanout address is a
**counter**, because `hud_req_*` is a **monotonic sweep, not the random access
I17 calls it.**

**It reuses `zhao_twod_sprite`** via band-clipped descriptors rather than
growing a second walker — *"I17's risk this time is rebuilding what exists, not
missing it."*

### The ONE thing still owed, and it is a law, not a wire

**The band's admission test is stricter than a frame store's and must be decided
BEFORE rasterising, because `TWOD.SPRITE.md` forbids partial sprites.** So there
is a bucket-overflow case, and **it is R221's shape exactly: an absence must not
look like a result.** Dropping a sprite silently is W10 in a HUD.

**Not ruled here** — it needs the same treatment R221 got, and it should be
decided by whoever writes the TWOD.BAND contract, **with the fallback COUNTED**.

**Also still owed:** a TWOD.BAND contract *before* any `blocks.yml` row (R214),
the descriptor record — and note the band **imposes no new requirement on it** —
and **a leaf fit to turn the 12 into a measurement.**

### One more instrument finding

**`RAM-INFERENCE-SCAN.txt` is committed and STALE** — no `post_composite`
section, no WEAK SIGNAL annotation. HUDBAND ran `check_ram_inference.py` live
**with a positive control** rather than quoting the file. `ring_q`'s only flag
is the tool's own known false positive — **and that flat one-write-address shape
is the band buffer's exact shape, sitting inside the 306.**

## R234 — FOUR OWNER DECISIONS, 2026-09-21. **(owner, explicit)**

**These four were chosen by Fabian directly, from a quiz with the evidence
attached. They are `(owner, explicit)` and join R1–R7 as rulings only he can
lift.** R226 found **293 sites** in this tree writing *"owner ruling R<n>"* for
a coordinator-provisional one; **these are not that.** Cite them as his.

---

### D1 — VERTEX COLOUR: **SHIP GOURAUD.** *(owner, explicit)*

The lit per-vertex colour is reconnected. **~1,420 ALM and +24 DSP**, plus
`METAW` 1157 → 1877 (29 → 47 forty-bit M10K slices).

Decided **by looking**, on GOURAUDLOOK's boards, which is what `CLAUDE.md`'s
first law demands and what R230 refused to short-circuit with a DSP column.
**The evidence the owner ruled against:** the free stand-in is not face-Lambert
but a **provoking-vertex pick** that produces a herringbone sawtooth crawling
the body — *"noise, not style"* — and **100.00% of 3,168,243 drawn triangles
take the Gouraud path**, so the stand-in differs on nearly every triangle of
every frame.

**ATTRLANE has already mapped the work precisely:** slots 3–5 arrive **full** at
`zhao_geom_attrpack` and stop; `continuation_w` is assembled per fragment three
lines under `attr_join_q_q[1]`/`[2]`, so **`vertex_rgb` needs no separate I20
producer** and nothing downstream of the tile pipe changes. **R89 does not
transfer** — it refused a fourth plane for a value that *"does not vary across
the primitive"*, and varying across the primitive is what Gouraud IS.

### D2 — THE FIT GATE: **REACH TRUE ZERO FIRST.** *(owner, explicit)*

**Not "fit at the floor."** The campaign continues until the register is
genuinely zero.

**Two consequences follow immediately, and both are now authorised:**

* **`R199 IS REVERSED`** by this decision. I deferred the forge program page
  kind because four of six families have no evaluator; the owner has chosen to
  pay for the evaluators rather than accept the deferral. **The page kind is to
  be frozen and FORGE.PRIM / FORGE.PRIM_EVAL built.**
* **FORGE.SHADOW is commissioned as a SUBSYSTEM packet**, which is exactly what
  R133 said it needed and what no one had ever sent. Its five blockers were
  re-measured today and **all five stand** — LODSTATE and SHADOW mutually
  blocked, ladderbank as a sixth adapter requester, the governor, Route B, and
  a client-A widening that **re-authors a ratified law**.

**R133 is not overturned; it is SPENT.** It said *"it will not be closed by a
wiring job"* and it was right. It never said it could not be closed.

### D3 — M10K: **GRANT THE 185 FOR THE TERRAIN DEVIATION STORE.** *(owner, explicit)*

Under the standing 2026-09-18 ruling that more M10K is fine, particularly where
it saves ALMs.

**Recorded plainly, because the next reader is owed it:** nobody has yet priced
what ALM the deviation store removes. The owner granted it anyway, which is his
call to make. **If a later pass shows it removes no logic, that is a fact worth
surfacing — not a licence to revoke a granted budget.**

### D4 — HUD BAND OVERFLOW: **delegated to the coordinator, and ruled below.**

---

## R235 — THE BAND ADMISSION LAW: refuse the sprite WHOLE, and COUNT it

**Ruled under D4's delegation, on R221's precedent, which the owner endorsed by
choosing it.**

`TWOD.SPRITE.md` forbids partial sprites, and the band's admission test is
stricter than a frame store's, so a sprite that will not fit the budget must be
disposed of **before rasterising**.

**RULED: refuse the sprite WHOLE, and COUNT the refusal.**

The reasoning is R221's, applied to a place players look: **the alternatives
make an ABSENCE LOOK LIKE A RESULT.** Dropping a HUD sprite silently is W10 —
and W10 is not theoretical here, it is the exact defect R168 and R181 found live
in silicon this week. **An uncounted refusal is a HUD that quietly stops telling
the player something, with no way to know how often.**

**Buying headroom instead was refused as paying for a case that may not exist.**
HUDBAND measured a **9× margin**: a full-width 32-row status bar sits *exactly*
at rate, and 40 glyphs over it need 10 lines of burst, which **B=4, L=16 clears
with 12 M10K**. Raising B/L costs `ceil(0.75 × L)` and would buy insurance
against an overflow the schedule says is nearly unreachable — **while leaving
the law unwritten anyway.**

**The counter owes a positive control** (R95: it must DISCRIMINATE, not merely
move). Since a legal workload may never overflow, that control is likely a
**committed mutant** under `tests/mutants/`, per `CLAUDE.md`'s rule for a guard
unreachable by legal stimulus.

---

## WHAT THESE FOUR NOW IMPLY, stated once and not relitigated

**The owner has ruled and the campaign executes.** But the coordinator's job is
to say what follows, and three budgets move together:

```
  DSP     185 (current)  + 24 (Gouraud)          = 209  against 112
  M10K    306 (floor)    + 38 FIELD + 185 + 12   = 541  against 553  (98%)
  ALM     47,582         + 1,420 + ~595 + ~13,700 FIELD = ~63,300  against 41,910
```

**None of this is an argument against the decisions.** It is the statement of
where the ceiling work now has to happen, and the owner's own standing rules
already say how: **never by removing, disconnecting, stubbing or narrowing
mandatory function** — only by architectural sharing, time-multiplexing where a
schedule proves the deadline, exact lookups, RAM-backed state and measured ARM
offload.

**One lever is now nearly spent and that is the new fact.** *"Trade ALMs for
M10K"* has been this campaign's main instrument, and **M10K at 98% can no longer
absorb much.** The DSP savings register (`reports/DSP-SAVINGS-REGISTER-20260918.md`)
prices a freed DSP at **~135 ALM** while demand exceeds supply, so **DSP work is
now the highest-leverage ceiling work available** — and D1 has just added 24 to
the demand side deliberately, for a reason that was looked at.

**Nothing here is deferred and nothing is narrowed.** It is recorded so that the
fit, when it finally runs at true zero, surprises nobody.

## R236 — I MISREAD THE OWNER AND RAN A FIT. It is killed, and the rule he gave instead is bigger than the correction

**2026-09-21. `(owner, explicit)` on both counts.**

### What he said, and what I did with it

> *"I took the expensive options. We just want the full capability. We're over
> budget a hundred times already. The important thing now is to get a fit now.
> A full fit, no caveats. So we can assess the damage."*

**I read "a full fit, no caveats" as an instruction to fit IMMEDIATELY**, treated
*"fit at completion only"* as superseded, and launched a full console fit. **I
then wrote that reading into three durable places** — the handover's §14, the
run's `TASK_LOG.md`, and `PACKET-QUEUE.md` — where the next reader would have
inherited it.

**The correction, in his words:**

> *"You misunderstood the hell out of me. We're not fitting now. We're going
> zero gaps. We want a full composed console. Fit now is useless. We are picking
> all the expensive options to see how big damage is."*

> *"Don't fit now. If you started fit, cancel it. Get all the gaps sorted."*

**The fit is killed** — `quartus_map` stopped, verified clear with a positive
control on the process query, and its artifacts reverted so the tree is clean.

### Why I was wrong, stated so the error is legible

***"No caveats" is a property of the DESIGN being complete, not of the fit
command's flags.*** I optimised the flags — clean tree, real device, FIELD in —
and felt I had earned the phrase. **But the fit I launched measured a console
missing Gouraud, the deviation store, the HUD band, DELTALAW's repair,
FORGE.SHADOW and `zhao_geom_clipread`.** I even wrote, in the same breath,
*"read the number as a FLOOR"* — **which is a caveat, written by me, in the
document announcing a fit with no caveats.** The contradiction was on the page
and I did not see it.

**The expensive options were chosen so that the EVENTUAL fit measures the whole
machine.** Taking them and then fitting early throws away the reason for taking
them.

**"Fit at completion only" was never superseded. D2 stands: reach true zero.**

## THE STANDING RULE — capability wins, and the question is closed

> *"If the question is about keeping capability and not having enough resources
> — we already don't have enough resources."*

**This retires a whole class of questions I have been putting to him**, and it
should change how every packet reasons:

* **Never trade a capability away for a resource number.** Not ALM, not DSP, not
  M10K. Being further over is not a new problem; **losing a function is.**
* **A cost is a fact to record, not a veto.** Price it, write it down, build the
  thing. R222 refusing the HUD frame store on M10K was *structurally* right — a
  cheaper structure existed that kept the capability whole — **but if no cheaper
  structure had existed, the frame store was the answer and the 180 M10K was
  just the bill.**
* **Stop escalating cost tradeoffs as owner decisions.** They are not decisions;
  they have a standing answer. **Escalate LAWS** — art, ABI, semantics, anything
  player-visible — which is what D1's Gouraud boards actually were.

**This is `CLAUDE.md`'s existing prohibition made positive.** That file already
forbids making a resource number smaller by *"removing, disconnecting, stubbing,
narrowing or silently reducing mandatory function"*. **R236 says the same thing
from the front: when the two collide, the function is not the variable.**

### What changes right now

**Nothing is fitted until the register reads zero and the console is fully
composed.** The machine belongs to the packets. **Do not run Quartus.**

**Three packets are live and all three are gap work** — DELTALAW (R231's depth
repair, may close I32), SHADOWSUB (FORGE.SHADOW under D2), GOURAUDBUILD (D1).
**That is the right shape and it continues.**

**And the queue's next items are now unambiguous:** FORGE.PRIM / FORGE.PRIM_EVAL
(unblocked by D2's reversal of R199), the `form -> clip bank` law, the HUD band,
I29's consumer, FORGE.CLIFF. **Cost is not a reason to defer any of them.**

## R237 — A BLOCKER THAT INFLATES THE WORK HAS NO IMMUNE RESPONSE, and that is how FORGE.SHADOW got five passes of zero movement

**2026-09-21, SHADOWSUB's follow-up. Register 22 → 22.** The lane built the
arbiter it had found missing, **and then withdrew its own blocker** — the one I
had written a ruling around.

### The observation, which is new and is the most useful thing in this packet

> **"Both my errors this packet ran in the direction that made the work look
> BIGGER. That is the inverse of the failure `CLAUDE.md` warns about and has
> none of the immune response — a diagnosis that ABSOLVES the design gets
> challenged; a blocker that DEFERS work gets believed and re-quoted. Which is
> how this cluster got five passes of zero movement."**

**`CLAUDE.md`'s law is about diagnoses landing SOFT:** *"the comfortable
explanation arrives first and explains almost all of the evidence… when a
diagnosis means the design is fine, spend the extra five minutes."* **That law
has produced an immune response here — this campaign challenges soft diagnoses
reflexively.**

**There is no such reflex for the opposite.** A blocker that makes work look
larger reads as *conservative*, *rigorous*, *safely pessimistic* — and so it is
**believed, re-quoted, and inherited**. It defers work rather than licensing it,
which feels like the careful direction. **It is not: it is the same failure with
better manners, and it costs whole passes.**

**The evidence is the cluster's own history.** FORGE.SHADOW went **21 → 21 five
times**. Every pass re-verified the blockers and every pass believed them. **The
one lane that tried to build against them found two were wrong, one was a
checkmark over an absence, and one it had authored itself.**

**This joins R229 as the second inverted-flattering-direction law**, and the
pair is now general: *ask which way your error would have to run to be
comfortable, and check that direction hardest — for a builder that is "smaller",
for an auditor it is "I found something", and for a blocker it is "this is
bigger than you thought".*

### What it withdrew, and what that costs me

The lane had reported that **an arena-fill path on client A's RESULT port would
re-author a ratified law**, and **I ruled it out of scope on that word.**

**It is not needed.** `zhao_part_project` takes particle results straight out on
`q_*` as `signed [20:0]` canvas coordinates **with no arena anywhere on that
path**. A client-A client owes **a rider and a demux arm**, not an arena — and
**particles prove it in composed silicon.** The widths already agree:
`q_x_o` is `signed [20:0]` and `zhao_geom_clip.tri_ax_i` is `signed [20:0]`,
R188's headroom finding landing where it is needed.

> **So nothing in this subsystem re-authors a ratified law, and my out-of-scope
> ruling guards nothing.**

**That is now the SECOND time R133's "this needs an owner decision" has
dissolved under measurement** — the first being R3, which already names
FORGE.SHADOW's instance-centre 1/w as one of the three sharers. **Neither of
this subsystem's two supposed law questions was real.**

### What was built, and the defect its own test caught

**`zhao_terrain_tapshare`** — the arbiter the `tap_*` checkmark was hiding.
**The hard part was not arbitration:** the tap's response carries **no tag and
no rider**, so unlike every other shared service here **routing cannot be by
rider and the block must hold the owner itself**. One register rather than a
queue, **licensed by a measured fact** (`req_ready_o = (st_q == S_IDLE)`),
recorded as `SINGLE_FLIGHT_ONLY` and **`$fatal`-guarded so a future pipelined
tap fails to ELABORATE rather than quietly needing a queue.** Response data is
**broadcast, not muxed N ways** — eighteen 32-bit buses × N of ALM for a value
only one client can want.

**Its own directed test found a real defect in its first version:** it could not
grant on the cycle an owner retired, so **every height tap cost a dead cycle —
~8% of the tap's thirteen-state walk, on every tap the console will ever make.**

> **It linted clean with `-Wall`, passed the Quartus-17 gate, and was
> functionally correct. No gate in this tree could have seen it.** Only the
> *"granted on the retiring cycle"* check did.

**That is R60's argument in one incident** — a directed test must BUILD AND RUN,
because the thing it catches is not a lint class.

**97 checks**, grants alternating exactly over 20 contended rounds with the
N−1 bound **exercised rather than asserted**, the discriminating case being an
owner held across a six-cycle walk while the *other* client asserts valid. Plus
an **inverted-polarity positive control** where `stray_rsp_o` fires and the
answer provably reaches nobody. **Silent on legal stimulus, fires on the fault**
— R95 satisfied. The mutant is a copy and was **regenerated after the repair.**

**And both ledger gates correctly REFUSED the file** until it had a disposition
(`G4 UNCLASSIFIED`, `UNACCOUNTED`) — **R210 working as designed.**

### Why the composition did not land, and why that is structural rather than shy

**Adding ports to `zhao_part_project` obliges the core to connect them.** With
no LODSTATE to connect them to, **the only legal connection is a tie-off —
closing no gap and opening one**, which R75 endorses refusing. **There is no
SystemVerilog port default Quartus honours, so there is no third option.**
LODSTATE in turn needs forge_shadow. **One commit or none**, and the lane
declined to land a terminal build at lower quality than the arbiter got.

**R3's proof cannot land yet either, and the lane declined to fake it:**
`tb_part_project` verilates the block **standalone**, so composed multi-client
throughput has never been measured. Quoting the service header's
398,784/1,666,666 *"would be comparing a current design to an old claim"* —
which is `CLAUDE.md`'s "never compare a current file to an old measurement",
refused unprompted.

### Two things for the next lane, and one bears on a DIFFERENT open question

* **The terminal build is ordinary engineering, no law:** fan assembler +
  GEOM.CLIP-door arbiter + material span. **The arbiter is done and waiting.**
* **`zhao_geom_drawjob`'s form index is EXPOSABLE, NOT INVENTABLE — drawjob
  already holds `form_idx_q`.** *That bears directly on POSEREAD's open
  `form -> clip bank` question*, where the concern was that wiring a draw
  through would serve **a correct palette for the wrong animal**. **If the form
  index already exists one block upstream, the cheapest of POSEREAD's three
  options may be cheaper still.** **To be verified by whoever takes it, not
  assumed from here.**

**Still open and genuinely the owner's:** the per-instance ladder's view
selector at `mask == 2'b11` — the lane recommends **paying the index bit**.
**Still mine:** whether to compose the governor with its TERRAIN.LOD group
dangling. **R75 and R223 say hold, and the lane did not overturn that.**

## R238 — A RECEIPT FROM A WRAPPER FIT PRICES THE WRAPPER'S CONSTANTS, and my Gouraud headline was ~900 ALM short

**2026-09-21, GOURAUDBUILD's two corrections to my own brief.** Both were owed
a ruling and had only a commit message. Register 22 → 22; **D1 is BUILT, and
the decision it costs is the owner's R234 "ship Gouraud".**

### Correction 1: the number I quoted was honest, measured, and priced the wrong machine

I briefed **"~1,420 ALM and +24 DSP for three lanes"**. The lane **confirmed it
against the receipt rather than against my brief** —
`reports/synthesis/blockpaths/zhao_raster_texture_v3_fit_top@g8a.fit.rpt`,
*Fitter Resource Utilization by Entity*, three `zhao_raster_attrgrad_v2`
instances summing **1,419.5 ALM and 24 DSP**, `rtlCleanAtHead: true`, truth
device 5CSEBA6U23I7. **The number is real and the tree it came from was clean.**

**It prices the three lanes and nothing that carries their operands.**

> **The G8A wrapper drives `job_meta_w` FROM CONSTANTS.** So in that fit the
> tile pipe's own plane registers **fold away**, and its `u_tile` row shows
> **357.7 ALM / 665 registers** for three lanes' worth of glue — against
> **3 × 240 = 720 plane flip-flops alone**. That row is measuring a
> constant-folded machine.

**No existing receipt can price the carriage**, because every receipt that
exists was taken through that wrapper. Counted by hand from the RTL, outside
the three lanes: `plane_{n0,dndx,dndy}_q` 720, `attr_join_*` 129, attrpack
`va/vb/vc_q` 288, attrpack `n0/dndx/dndy_q` 720, binner `meta_q` + `d_meta_r`
1,440 — **≈ 3,297 flip-flops**, a **~825 ALM floor** at four per ALM and an
honest **~1,000–1,300 ALM** once register-only packing is allowed for.

> **The whole decision is ~2,300–2,700 ALM, +24 DSP, +18 M10K — roughly 900 ALM
> more than the headline I gave the owner when he ruled.** Estimated by hand;
> **Quartus was not run, because `PACKET-PROTOCOL.md` forbids it and the lane
> correctly held that the protocol outranks my brief.**

**The generalisation, and it is new here.** This tree already knows *"never
compare a current file to an old measurement"*. This is a different lie from
the same family: **the receipt is current, the tree was clean, and the
provenance is perfect — and the entity it prices is not the entity that
ships**, because a fit harness that ties a port to a constant hands Quartus
permission to delete the logic behind it. **A wrapper fit measures the wrapper's
assumptions.**

And note **which way it runs**: constant-folding can only make the row
**smaller**. That is R229 and R237's axis again — the comfortable direction,
with a clean digest and a real device stamped on it, which is precisely why
nobody audits it. **Before quoting an entity row, ask what the harness ties
off.** If the answer is "the thing whose registers I am counting", the row is
evidence about the harness.

The M10K half is **not** an estimate and does not share the defect: the bank is
`META_SLICES` independent 40-bit × `TRI_CAP` RAMs, `TRI_CAP = 128` in the
composed shell, **29 → 47 slices = +18 M10K**, arithmetic that no fit is needed
to check. And **per-triangle bank time does not change** — the slices are
written and read in parallel off one `tri_we` / `meta_ra`, so widening the
record costs RAMs, not clocks. That retires ATTRLANE's throughput flag.

### Correction 2: D1 broke a committed positive control, and it was INVERTED, not deleted

**`-GlowTag` is a committed control** that proves a tag on one boundary port
reaches the framebuffer. **D1 breaks it by construction**, because D1 delivers
colour through `tri_continuation_tail_i`'s `vertex_rgb` — **the very field
`-GlowTag` overwrites with `0xb5aab5`**. The form failed exactly as a real
regression does:

```
%Fatal: -GlowTag put 0xb5aab5 on the continuation tail and NOT ONE framebuffer
```

**The lane's repair is the one this tree requires and the one that is easiest
not to make.** Deleting the assertion removes a control; weakening it keeps a
green that measures nothing. It **inverted** it, so the form now asserts the
end-to-end path it can still see:

```
SMOKE: -GlowTag  END TO END: tag 0x7f on one boundary port -> 1062 covered
                 pixel(s) of 2560 resolved -> 1062 LIT by R195's law
```

**And the new number has independent corroboration**: `gouraud 1062 of 2560
drawn word(s) carry a COLOUR` against a **committed baseline of zero**, agreeing
with `gather_frag_lit_o = 1062` from an instrument that **shares no logic with
it**. Two counters, one answer, different silicon — which is what this tree
means by a counter that discriminates.

**This is also the ten-form sweep earning its cost.** The plain form passed
throughout; **only `-GlowTag` could see it**, and my own brief had once carried
an eight-form list. **A control a change invalidates is not a failing test —
it is a test whose question changed**, and the repair is to ask the new
question, out loud, in the assertion.

## R239 — THE FORM KEY EXISTS AND THE QUESTION IS STILL A LAW. FORMIDX verified a passing remark instead of inheriting it, and the decision got BIGGER

**2026-09-21, packet FORMIDX. Register 22 → 22, and I29 reduced in PRECISION
rather than in count** — which is the honest way to report a blocker that got
sharper instead of smaller.

I commissioned this because SHADOWSUB said, in passing, while mapping something
else, that **`zhao_geom_drawjob`'s form index is exposable, not inventable.** I
wrote in R237 that it was *"to be verified by whoever takes it, not assumed from
here."* **It was verified, and that instruction is the reason the answer is
worth anything.**

### The half that is real, and it is more solid than the remark claimed

**`form_idx_q [23:0]` exists** in `zhao_geom_drawjob`, loaded as
`d_form_i[31:8]` in `S_IDLE`.

* **It is the right numbering BY CITATION, not by inference** —
  `reference/include/zref/zref_creature_page.hpp`'s own header section *"THE KEY
  IS THE MESH_STREAM HANDLE INDEX, NOT AN INVENTED TYPE ID"* names that exact
  expression as the hardware key.
* **It is NOT entry I39's failure**, which was the specific risk I named:
  `d_ready_o = (st_q == S_IDLE)`, so the FSM **cannot admit a second draw**
  while the first is in `S_CHECK..S_EMIT`, and the register is loaded by the
  **same enable that admits the draw**. There is no second pipeline stage for it
  to run ahead of. I39's `mf_r_material_id` fails because *"two live wires are
  not a producer"*; this is one wire with one enable.
* **One qualification, recorded rather than glossed:** in `S_IDLE` it holds the
  **previous** draw, so any exposure must be **gated by state and never offered
  bare.**

### And the half that decides it is missing, which is why the shortcut fails

> **`form_idx_q` supplies the REQUESTER's half. POSEREAD's blocker is the
> BANK's half. A comparison with one operand is not a comparison.**

**The two lanes were looking at different seams.** SHADOWSUB's subject was
`zhao_geom_lodstate.j_form_index_i` — the **ladder table** key, which **is
ruled**. POSEREAD's is the **clip bank's identity**, which is not. My hope that
one closed the other was the cheap reading, and it is wrong.

### The finding that moves the cost the WRONG way, which is why I believe it

`zhao_geom_clipread.sv`'s header asserted *"so kind-8 → form IS ruled"*.
**That is true of the ladder TABLE and false of the BODY** — the half that block
actually reads.

**Measured on the committed golden `ladder_page_body_v1.bin`:** 448 bytes,
**three** ladder records (`form_index` 256, 257, 40983) against a `body_off` of
192 naming **one** 6-bone body. And `tools/pack/mkcreatureladder.py:build` makes
that **the FORMAT, not the fixture** — a list of records beside a single bone
list, with `build_body` packing `"<IHBBII"` and **no form index anywhere.**

> **So the wrong-animal failure is reachable through the SKELETON exactly as it
> is through the clip bank, and `bone_mismatch_o` is blind to both whenever the
> bone counts agree.**

**Re-costed against POSEREAD's three options:** (i) is **two fields and two
goldens**, not one — **but both headers carry spare reserved bytes, so no page
grows and no offset moves.** (ii) is *cheaper* for wiring than costed, since
both operands already exist as signals, and **impossible for kind 8** (three
forms, one index). (iii) is worse than costed.

### What is genuinely the owner's, and it is now ONE SENTENCE

> **Is a kind-8 page ONE CREATURE, or a BANK?**

`spec/creature_rules.md` §5 reads it as one creature. `zhao_geom_ladderbank`
reads it as a **`ROWS=16` bank**. **Both are live and they disagree** — and
every costing downstream depends on which is true. **This goes to him as a law,
with the options; it is not decided in a packet.**

### What it declined to build, and it was right twice

**It did not add `j_form_index_o`.** There is **no consumer today** — lodstate
is blocked behind `forge_shadow`, clipread behind this ruling — so the port
would be the **BUILT-INSTALLED-NOWHERE** shape `uncashed_cheques.py` exists to
catch, **billing three generators and a 1281-port parity re-read for zero
function.** SHADOWSUB reached the identical conclusion from the other side, and
**two lanes independently refusing the same cheque is the check working.**

**No ABI invented. No tie-off created. No register rise manufactured.** The diff
is comment-only, and **I verified that here rather than accepting it**: every
added line in both files is a comment.

**Its own confession, which I record because my brief warned about exactly
it:** the lane's first `completion_register` run was **piped into `tail` and
reported RC 0**; bare, it is **1**. The tool told the truth about the pipe. It
caught itself and said so.

## R240 — A STALE BLOCKER DOES NOT ONLY DELAY WORK, IT MISDIRECTS THE SEARCH FOR THE REAL ONE. Three layers of one rot, and the third nearly had me escalating RATIFIED LAW

**2026-09-21, coordinator, found while scoping GEOM.WARP as a candidate packet.
Nothing was built; this is an archaeology finding and it changes what the next
lane does.** Register unmoved at 22.

### Layer 1 — the sentence in production RTL is FALSE

`zhao_console_core.sv`, at the composed `zhao_field_host_v2`, explains why it
passes `.CLIENTS(2)`:

> *"GEOM.WARP prerequisite P2 asks for `CLIENTS(3)` so the warp adapter has a
> port; a third client whose `req_valid_i` is a constant zero is a TIE-OFF, and
> rule 1 forbids creating one even in the service of closing a gap.
> **`zhao_geom_warp.sv` does not exist in this tree**, so there is nothing to
> drive it with."*

**`fpga/rtl/geometry/zhao_geom_warp.sv` exists — 36,227 bytes, built
2026-09-20 by packet FIELDW1** (`3285a290`, *"GEOM.WARP is BUILT, and the
register honestly does not move"*), **alongside
`fpga/rtl/field/zhao_field_warp_adapter.sv` at 33,001 bytes.** The sentence was
true when written and is false now.

### Layer 2 — and this is the part that is NEW

**R165 already says: re-measure a blocker before quoting it.** Apply it here
exactly as written and you get a **confident wrong answer.**

A lane re-measures *"`zhao_geom_warp.sv` does not exist"*, finds that **it
does**, concludes **the blocker has expired**, changes `.CLIENTS(2)` to `(3)`,
composes the adapter — and ships a function that **cannot be reached by any
draw**. Because the real blocker is somewhere else entirely, and it is bigger:

> **`zhao_geom_warp.sv` itself says it: *"there is no `DrawWarpedForm` command,
> so no draw can set `d_warp_en_i`."*** A composed `zhao_geom_warp` under
> today's ABI **sits permanently in its W09 bypass — function present and
> structurally unreachable.** That is not a composition.

**So the stale sentence was not merely out of date. It NAMED THE WRONG
BLOCKER**, and it named one that is **cheap and checkable**, standing in front
of one that is **expensive and easy to miss**. Re-measuring the *stated* blocker
is not the same act as finding the *actual* one.

**The rule that follows, and it is the sharper half of R165:** when a stated
blocker turns out to have expired, **do not read that as "the path is clear".
Read it as evidence the entry stopped being maintained**, and go find what else
holds it. **A blocker sentence that has rotted once is a sentence nobody has
been checking** — the expiry is a signal about the *document*, not only about
the *claim*. The authority is the **contract's timestamped prerequisite
table** (`design/contracts/GEOM.WARP.md`), not a parenthesis in an
instantiation.

### Layer 3 — I then went looking for the real blocker and nearly escalated a question the owner had ALREADY ANSWERED

Having found that GEOM.WARP is unreachable without `DrawWarpedForm`, I was
about to put *"do we add a new draw opcode?"* to the owner as a **law** — an
ABI addition, exactly the class this campaign refuses to invent inside a
packet, so escalating felt not just safe but **correct**.

**It is ratified, and has been since 2026-09-20.**

`reports/OWNER-RATIFICATION-20260920-WARP.md`, decision **W04**:

> *"Add **`DrawWarpedForm` at opcode 0x0304**, 96-byte record / 80-byte payload.
> `DrawForm 0x0300` and raw vertex format 0 stay **byte-for-byte** unchanged."*

Ratified by **Fabian's own commit `4c256137`**, 2026-09-20, whose message is
*"Agent please read — implement this warp architecture **and architect
implement whatever else is missing when it comes up**"*. The ratification file
is explicit that **W01–W18 are new law as of 2026-09-20**. And
`spec/commands.zidl` already knows: *"0x0304 IS SPOKEN FOR — plan W04 allocates
it to `DrawWarpedForm`"*, which is precisely why POSECMD correctly took
**0x0305** for `DrawPosedForm`.

> **The opcode is allocated, the record size is specified, and the owner has
> said in writing to implement it. There is no law question here. There is a
> BUILD.**

**This would have been the FOURTH supposed law question to dissolve under
measurement** — after R3, after SHADOWSUB's arena claim, and after R237 recorded
that *"neither of this subsystem's two supposed law questions was real."* The
pattern is now established well enough to be a standing rule:

> **Before escalating anything as a law, grep `reports/OWNER-RATIFICATION-*.md`
> and the owner's own directive commits.** This tree has a directory of answers
> in it. **Escalating a settled question is not the safe direction — it costs a
> round trip, it spends the owner's attention on something he already spent it
> on, and it stalls a build that was authorised days ago.** R237's law applies
> to me here: *asking* looks conservative and rigorous, which is exactly why it
> does not get challenged.

### What GEOM.WARP actually needs, from the authority rather than the parenthesis

From `design/contracts/GEOM.WARP.md`'s **measured, timestamped** table, itself
corrected on merge under R165 because four rows had been measured against a base
predating packets C1 and D1:

* **P1 — CLOSED.** `.IN_LANES(15)` composed by C1.
* **P2 — ABSENT.** `.CLIENTS(2)`, one site. **A one-line parameter change.**
* **P3, P4 — PRESENT AND NOW COMPOSED** in `zhao_field_host_v2`.
* **P5 — DEFERRED, NOT CLOSED.** `prep_value` is still one flat 64-entry array
  with no context dimension; the per-slot generation stamp **detects** a stale
  prepared scalar but does **not isolate** two simultaneously eligible plans.
  **Satisfied only if Warp never interleaves with Earth inside a frame.**
* **P6 — STILL PARTIAL.** `TABLES = 2` against a fabric carrying 4.
* **P7 — ABSENT.** `.INSTR_N(32)` against a required 48. **A one-line change**,
  though host_v2's own default is 32 as well.
* **P8 — UNMEASURED, and honestly labelled so.** D1 **is** merged now and
  `zhao_field_doorbell.sv` carries `BIND_PROGRAM`; **whether that satisfies
  §9.3's BIND/SEAL has not been measured.** The row says so rather than
  guessing, which is R165 working.
* **P9 — NOW EXISTS**, discharging R103's standing claim that the R91 fast path
  *"does NOT exist anywhere in the tree"*. **Still unmeasured in clocks** — and
  the contract is careful that `51 + T_run` and `19 + T_run` are **FSM
  arithmetic, not benchmarks and not fits.**

**So GEOM.WARP is a real packet and a large one: `DrawWarpedForm` (ratified,
specified) + two one-line parameters + P8 measured + P5/P6 dispositions.** It is
**not** a law question and **not** a cheap close. **It goes in the queue, not to
the owner.**

## R241 — TWO OWNER DECISIONS, 2026-09-21 (second batch). **(owner, explicit)**

**Put to Fabian after each survived R240's ratification check** — that is, after
grepping `reports/OWNER-RATIFICATION-*.md` and the owner's own directive commits
to confirm neither had already been answered. **A third question was returned
for rephrasing and is recorded below as still open.**

### D-TICK-A — `frame_tick` is sourced from `DrawProcedural`'s `pad[11]`

**Decided: per-draw, not broadcast.**

`spec/cartridge.md` §2 declares this open in its own words — *"`frame_tick` is
sourced at DISPATCH, not from here… Where the dispatch gets it is an **open
owner question**… **This section does not decide it**"* — and names the two
candidates. **The owner took `pad[11]`.**

**What that buys, and it is the reason to prefer it:** software can **phase,
pause, scrub or slow each effect independently**. A broadcast frame counter
animates every bolt in lockstep and gives software no way to hold one still
without rewriting its page — and a cartridge page is **immutable and uploaded
once**, so "rewrite the page" is not an option at runtime.

**It costs an ABI reinterpretation and a generated-consumer pass, and the
precedent is already set:** `forge_kind` itself used the **same mandatory-zero
reinterpretation** of a reserved `pad` byte. So this is not a new mechanism,
it is the second use of an existing one.

**`frame_tick == 0` remains a complete, deterministic, static primitive**, which
means **nothing already built becomes wrong** and pages that do not animate need
no change.

### D-LADDER-A — the per-instance ladder PAYS THE CAMERA INDEX BIT

**Decided: one more bit × INSTANCES, so each creature's ladder measures against
the right camera.**

`zhao_geom_drawjob` emits `j_active_mask_o [1:0]`, a two-view **mask**;
`zhao_geom_lodstate`'s `j_view_i` is a **single bit**. **For
`active_mask == 2'b11` there was no honest answer in the tree**, and
`zhao_geom_lodstate`'s own header had already docked it: *"ONE LodState PER
INSTANCE, NOT PER CAMERA — AND THAT IS A DEVIATION… the disagreement is
REPORTED rather than resolved here."*

**The two candidate authorities disagreed and neither ratified creatures.**
`zref::creature` holds ONE LodState and takes ONE threshold; **PART.LADDER's
2026-08-31 §2.5 ruling makes the sibling PARTICLE selection PER CAMERA.**
SHADOWSUB's recommendation was to pay the bit, and the owner agreed.

**The failure mode this buys off** is the one that would never have been
diagnosed: **a creature that pops LOD rungs in the second view for reasons
nothing records.** As SHADOWSUB put it, *"the reference's silence is not a
ruling"* — and **composition is exactly where it stops being reportable:
something must choose, and choosing silently is inventing.**

This also settles the shape by **agreeing with the particle rule rather than
diverging from it**, so creatures and particles now select LOD the same way —
one fewer place where two subsystems do the same thing differently.

### Still open, and returned for rephrasing — THE KIND-8 QUESTION

I put *"is a kind-8 page one creature or a bank?"* to the owner in terms of
records, offsets and goldens. **He sent it back:** *"Please talk to me about
repercussions, not implementation details of this."*

**That correction is right and it is about me, not about the question.** I had
taken FORMIDX's measurement — three ladder records against one 6-bone body, and
that being the FORMAT rather than the fixture — and handed the owner the
**measurement** to rule on. **The owner rules on what the console can DO.** The
byte layout is the consequence of his answer, not the substance of it; presenting
it as the substance asks him to do the engineering and then ratify it.

**It is re-put to him in those terms.** The decision is recorded here as **OPEN**,
and `zhao_geom_clipread`'s wrong-animal blocker stays live until it lands.

## R242 — THE DEVIATION STORE MOVES TO SDRAM. **(owner, explicit, 2026-09-22)**

**Fabian, 2026-09-22:** *"Move deviation store to SDRAM, ignore stale info."*

**This supersedes ruling R24's storage clause** — *"At PAGE LOAD, stored
alongside the page in M10K (the owner prefers M10K over ALMs; deviations change
only when the page changes or is baked)"* — **and R24's "M10K" is the stale
information the owner is instructing us to ignore.** R24's preference was
correct when it was made: M10K was the resource with slack and ALMs were the
binding constraint. **R87 ended that** — *"against 306 of 553 already used, one
block wanting 185 is not a rounding error… Price M10K explicitly in the fit
plan; it is no longer the free currency."*

**Everything else R24 decided stands**: deviations are computed **at page load**,
the key is the **residency page slot** (a compose slot would let a stranger
inherit the hysteresis history), and **BAKE re-triggers the recompute for the
pages it dirties.** **Only the storage MEDIUM changes.**

### The block had already worked this out and left it for the owner

`zhao_terrain_devstore.sv`, under its own heading *"AND A THIRD FORM IS AN OWNER
DECISION, NOT TAKEN HERE"*:

> *"These 185 M10K are per-page **DERIVED** data, which is exactly what
> `spec/memory_rules.md` §5b gives an SDRAM home to for the coarse-height mips
> (TERRAIN.RESIDENT_MIP_POOL, 1,024 × 1,536 B). **176 B per patch of deviations
> (16 × 88 bits) would fit the same pattern at 176 KB of SDRAM and 44 KB/frame
> of read bandwidth, and cost no M10K at all.** It is not taken here because
> ruling R24 says M10K in as many words and because it needs a new guarded
> region, which is an ABI act. Recorded so the owner can spend the 185 M10K
> deliberately or move it — **and at 33% rather than the 14% the ruling was
> given, that choice is materially different from the one R59 was actually
> asked.**"*

**Both of its stated blockers are now gone.** R24's clause is superseded above,
and the guarded-region act is **authorized** — it is a smaller instance of the
2026-09-22 item 4 authorization, which granted ENGINE1 a narrow read/write
window including a *shared prefetch/chunk scratch* and required
`mem_guard_no_escape` to be **extended, not bypassed, with a deliberate fault
that makes the proof fail.** **Follow that pattern exactly.** Terrain's region
is its own; item 4's ENGINE1 ranges are **not** a licence for this client.

### What it is worth, and why the bandwidth is not a close call

**185 M10K returned** — 141 for the deviations plus 44 for the history. On the
coordinator's summed hand counts that takes the console from **~587 to ~402
M10K** against the target's 553, and to about a third of the sizing device's
1,220. **176 KB of SDRAM; 44 KB/frame is ~2.6 MB/s at 60 Hz.**

**That estimate is a sum of six lanes' hand counts, not a fit.** It is the right
order of magnitude and wrong in detail.

### THE SECOND LEVER IS REFUSED AND STAYS REFUSED

There is a further 33 M10K available by packing records at 67 bits instead of
88, because every lattice height reaching `zhao_terrain_loddev` today is
`height16 << 8`, so **every deviation's low seven bits are provably zero.**

**The block refused it and the refusal is right:**

> *"the invariant is upstream, unenforced and invisible: the moment a composed
> height carries a FIELD delta (entry I34's lane) the low bits stop being zero
> and every deviation silently quantises to 128ths **with no counter able to see
> it**. A packing that is exact only while somebody else keeps a promise they
> never made is the broken-instrument shape."*

**Do not take it as part of this move.** It is a correctness hazard dressed as a
saving, and I34's field lane is live work.

### And note which way the error ran, because it is this campaign's pattern

**Every number in that block's own sizing section was wrong until 2026-09-20,
and wrong in the flattering direction** — a record gained a 16-bit field, two
derived localparams kept their old comments (`ROWW` said 576 against 704), and
the published figure was **116 M10K against a real 141**. **R59's premise was
the same error one step further back:** it priced the store at *"~786 kbit =
~77 M10K (14%)"* using a **16-bit** deviation where `DEVW` is 24, and counting
**no history at all**. The real figure is **2.4× the ruling it was decided
against**, and *"14% is affordable and 33% is an argument, which is exactly why
nobody audited it."*

**R59 had asked for the smaller form and to take it if bit-identical. One
smaller form was taken** (the underside's records are never read — TERRAIN.LOD
law 7 gives the underside the top's level, so a second surface can be computed
and thrown away with no effect on one output bit: 326 → 185). **The third form,
the one that actually removes the cost, needed an owner and now has one.**

## R243 — THREE OWNER DECISIONS, 2026-09-23. **(owner, explicit)**

Put to Fabian after checking each against `reports/OWNER-RATIFICATION-*.md` and
the 2026-09-22 rulings, per R240. **None was already answered.**

### D-SDRAM-A — FIX THE MODEL FIRST, THEN THE ARBITER

> **Fabian: *"3 first, then 2. Make the SDRAM model match real JEDEC BL8
> wrapping first so the bug class becomes observable in simulation, then fix the
> arbiter centrally and re-prove the exact liveness bound."***

**The problem, found by PARAMARENA:** a JEDEC **BL8 sequential burst wraps
inside its aligned eight-column block**; `zhao_vram_arbiter` never aligns to it;
**the behavioural SDRAM model reads LINEARLY.** So **the model reads BETTER than
silicon**, no client has ever exercised it, and **no test in this tree can fail
on it** — the optimistic side is the one we simulate.

**The owner rejected both cheap options and the ordering is the whole ruling.**
Aligning each client individually (what ARENAWIRE is doing for the arena) leaves
every future client having to remember, and forgetting yields **correct
simulation and wrong hardware**. Fixing the arbiter first would fix the bug
while **leaving the bug class undetectable**.

**So: make the divergence VISIBLE before repairing it.** Expect the model repair
to turn other things red — **that is the point**, and a red it produces is
evidence, not a regression. Only then move the arbiter's bound, and
**`mem_vram_arbiter_liveness` asserts that bound is EXACT, so it must be
RE-PROVEN, not adjusted.** A lane must show the new bound holds.

**This is the broken-instrument law with the instrument being the simulator
itself:** *"a detector that has not been shown to FIRE has not been tested."*
Here the detector cannot fire by construction until the model tells the truth.


### D-SDRAM-A VERIFIED AT SOURCE, 2026-09-23 — and the divergence is worse-documented than reported

R243 was ruled on **one lane's reading**. Before spending a lane executing it,
the coordinator read the three files itself. **The claim holds, and the shape of
why nobody caught it is the part worth keeping.**

**1. The model increments the column LINEARLY.** `sim/models/zhao_sdram_model.sv`
line 141:

```systemverilog
dq_i_q <= word_at(rd_bank, rd_row, rd_col + 11'(rd_beat));
```

and the write path at line 154 is the same expression. `rd_col` is 11 bits, so
this wraps at the ROW (modulo 2048) and **nowhere else**. A JEDEC BL8 sequential
burst wraps inside its **aligned eight-column block**: column bits [2:0] advance
and **[10:3] are held**. Starting at column 5 the device returns
`5,6,7,0,1,2,3,4`; the model returns `5,6,7,8,9,10,11,12`.

**2. The controller programs BL8 SEQUENTIAL and then issues an arbitrary
column.** `zhao_sdram_ctrl.sv` line 204 writes the mode register as
`13'b0000_00_011_0_011` — `A[2:0]=011` is BL8, `A3=0` is sequential — so the
part is explicitly told to wrap. Line 143 is `req_col = waddr[10:0]`: **the
column is the low bits of the request address, with no alignment enforced
anywhere.**

**3. `design/contracts/MEM.SDRAM.md` contains the words `align`, `wrap` and
`BL8` exactly ZERO times.** The law the silicon obeys is not written down, so no
client could have been told to obey it and no review could have checked it.

**THE INSTRUCTIVE PART: two separate comments state a TRUE fact about the ROW
and are read as covering the BLOCK.** They are the reason this survived.

* The model's header: *"Read bursts wrap within the row (sequential, modulo
  2048); the controller never issues a burst that crosses a row boundary, **so
  the wrap is unreachable in lawful traffic** and exists only so unlawful
  traffic is served, not hung."*
* The arbiter's `burst_words()`: *"min(remaining, 8, row tail) in words (rows
  are 2048 words; **8 divides the row so a tail burst never crosses**)"*.

**Both sentences are true.** Both are about the row. Both conclude with a
reassurance — *"unreachable"*, *"never crosses"* — and that reassurance is
exactly what a reader takes away. `burst_words()` clamps to the **row tail** and
considers the eight-column block nowhere; `pend_addr[k] <= client_req[k].addr`
(line 351) takes the client's byte address verbatim.

This is CLAUDE.md's own law with a new instance: **the comfortable explanation
arrives first and explains ALMOST all of the evidence.** Here it explains the
row and is silent on the block, and the silence reads as coverage. It is also
the broken-instrument law — the defect makes the simulated machine **better
behaved** than the real one, which is the direction nobody audits.

**WHAT IS STILL UNMEASURED, stated so it is not read as settled.** Whether any
*current* client already issues a 16-byte-unaligned burst is **not known** — a
64-byte-aligned request is 16-byte aligned and therefore safe, and most clients
plausibly are. **That question does not need a hand sweep, because the owner's
ordering answers it for free:** once the model wraps like the device, an
unaligned burst returns wrong data and the existing suite says so. **A red the
model repair produces is the measurement, not a regression** — which is the
whole reason *"3 first, then 2"* is the ruling.

**NOT DONE, DELIBERATELY, AND THE REASON IS A STANDING TRAP.** The obvious next
move is to instrument the model with a counter of bursts starting at
`col[2:0] != 0`, converting *"no client has exercised it"* from an argument into
a number **before** any behaviour changes. It was not done today because **two
lanes were mid-gate and every memory bench elaborates that file**: adding a port
to it is the *"a suite reads the LIVE TREE"* trap, and a `PINMISSING` in another
lane's lint reads as that lane's own breakage. **It is the first task of the
D-SDRAM-A lane, ahead of the wrap itself** — instrument, measure, then change
behaviour.

### D-NORMALS-A — DETAIL NORMALS ARE IN v1. COMMISSION THE PYRAMID.

> **Fabian: *"In v1 — commission the pyramid."***

**Why it was asked:** TERRACOMPOSE found `zhao_terrain_normalmap`'s fragment
path **is already composed** (44 raster/texture modules in the core's closure) —
but **the detail-normal pyramid does not exist anywhere: no RTL writer, no
command arm, no asset tool.** Composing the block **would move the register down
by one and change not one pixel**, which is the dishonest close the owner's *"the
right way"* forbids. **The lane refused it and was right to.**

**Authorized, across three layers:** the **asset tool** that builds the pyramid,
the **command arm** that delivers it, and the **RTL writer** that populates it —
then compose `zhao_terrain_normalmap` against real data.

**This is NOT covered by the 2026-09-22 six**, so it is new scope, and it is the
owner choosing capability over a cheaper register. **Terrain gets surface detail
independent of mesh density.**


### D-NORMALS-A SCOPED, 2026-09-23 — THE COMMAND ARM ALREADY EXISTS. Three layers are two.

The ruling authorised three layers: *"the **asset tool** that builds the
pyramid, the **command arm** that delivers it, and the **RTL writer** that
populates it."* Reading the tree before spending a lane, **the middle one is
already built, ratified and in use by three other blocks.** This is a
correction that REMOVES work, which CLAUDE.md calls the most valuable kind.

**WHAT IS ALREADY THERE, and it is more than the escalation implied.**

`fpga/rtl/terrain/zhao_terrain_normalmap.sv` is not a stub. It carries the
**full seven-level pyramid** — `logic [15:0] tile_m [0:PYR_WORDS-1]`, the
64→1 levels packed flat, `zref::terrain::normalmap_pyramid_addr` as the
addressing law — **and it already has the upload port**: `tw_we_i`,
`tw_addr_i`, `tw_data_i`, a flat word address into that layout. It is
zero-DSP, oracle-checked, and `ENFORCED-BY
tests/texture/terrain_normalmap_directed.cpp`.

It is instantiated **nowhere**. Every reference to it in `fpga/rtl/` is a
COMMENT in another file citing it as the M10K-inference precedent. That is the
"BUILT, INSTALLED NOWHERE" uncashed cheque exactly.

**THE DELIVERY PATH IS GENERIC AND SHIPPED.** `PublishResource 0x0030` (owner
ruling R17, 2026-09-19) is ratified and live in `zhao_cmd_exec`. `zhao_mem_upload`
verifies an upload and raises `publish_valid_o` / `publish_tag_o` (the 8-bit
page kind) / `publish_base_o` / `publish_extent_o`. A loader parameterised by
`PAGE_KIND` watches its own kind and streams the page into its block:

| block | `PAGE_KIND` | page |
|---|---|---|
| `zhao_geom_ladderbank` | `8'd8` | creature form |
| `zhao_part_table_loader` | `8'd13` | species table (R42) |
| `zhao_forge_pagebank` | `8'd14` | forge program (R234 D2) |

**So no opcode is invented and none needs to be.** The pyramid travels as
owner-authored DATA through the command that already publishes every other
resource — which is the same answer R42 gave when core entry I33 refused to
invent a species command, and the same shape as R234 D2 and the 2026-09-22
item 3 TWOD page. **Three precedents, one mechanism.**

**A FALSE ALARM WORTH RECORDING, because it nearly became three reported
defects.** Those three `PAGE_KIND` values look wrong against the section-type
table in `spec/cartridge.md` §2, where SPECIES_TABLE is `0x0011` (17) and
FORGE_PROGRAM is `0x0012` (18). A loader matching the wrong tag never fires,
silently — the exact failure this tree keeps finding, so it reads as an obvious
catch.

**It is not one.** `spec/cartridge.md` line 13 says in advance that the section
vocabulary and the **RESOURCE_PAGES kind registry** are different numberings,
and §3 line 93 carries the kind registry explicitly: **13 = species table,
14 = forge program, 8 = creature form.** All three blocks are CORRECT, and
they corroborate each other. The spec warned about this in its own second
paragraph; reporting the "defect" would have meant not reading it.

**THE NEXT FREE KIND IS 16** — the registry's own strikethrough history reads
*"~~Kinds 13-255 reserved~~ ~~Kinds 14-255 reserved~~ ~~Kinds 15-255 reserved~~
Kinds 16-255 reserved"*, so the allocation is maintained and the slot is
unambiguous.

**THE LANE'S WORK, THEN, IS TWO LAYERS AND A COMPOSITION:**

1. **The page kind.** `DETAIL_NORMAL` = kind **16**, backing section `0x0014`,
   a new §4g in `spec/cartridge.md` freezing the envelope: a 64-byte header
   then the flat pyramid words, `{s8 dz, s8 dx}` per texel in
   `normalmap_pyramid_addr` order. 64-byte shaping throughout, for MEM.GUARD's
   read-shape rule — copy §4b's reasoning, it is the same constraint.
2. **The asset tool**, the only genuinely new engineering: build the pyramid
   offline by **averaging SIGNED dx/dz and never normalising** (the module
   header is explicit that normalising is wrong), emitting the page above. The
   oracle has the ADDRESSER (`normalmap_pyramid_addr`) and **not** the builder,
   so the builder needs a `zref` model beside it or the tool becomes a second
   implementation of the layout law.
3. **The loader**, `zhao_terrain_normalloader`, modelled directly on
   `zhao_part_table_loader` — same four publication wires, same guard-client
   shape, `PAGE_KIND = 8'd16` — carrying words from the page to
   `tw_we_i`/`tw_addr_i`/`tw_data_i`. Its payload is SIMPLER than the species
   loader's: a flat 16-bit word at a flat address, with no selector slices.
4. **Compose loader and `zhao_terrain_normalmap` in ONE commit.** This is the
   group-composition method the campaign already proved: a leaf wired in alone
   is a producer with no consumer. Composing the normalmap alone is what
   TERRACOMPOSE correctly refused — *"it would move the register and change not
   one pixel"* — and composing it WITH its loader and a real page is what makes
   that refusal moot.

**And it is a guard client**, so it owes the eight edits a new reading client
costs and an entry in `tools/rtl/check_guard_verdict.py`'s `CLIENTS` **in the
same commit that creates it** — that gate is now green and its coverage audit
is exact, so a new client that is not listed will fail it immediately rather
than silently.

### D-FIT-A — RUN THE LABELLED DIAGNOSTIC FIT NOW, ON THE SIDE

> **Fabian: *"Run labelled first, just let it run on the side while you continue
> working with the 2 subagents."***

**This supersedes "fit at completion only" for this one run**, and does not
retire it: the completion fit is still owed when the register reaches zero.

**Launched 2026-09-23** from a worktree pinned at `18231903`:
`run_block_fit.ps1 -Module zhao_console_core -Device 5CEBA9F31C7
-RowLabel '@diag-incomplete-14gaps'`.

**THE LABEL IS THE OWNER'S OWN INSTRUCTION — *"explicitly label the diagnostic
fit incomplete"*** — and the runner's own comment agrees: *"a measurement
labelled complete is how a wrong number gets believed."* **The row says
INCOMPLETE and names the gap count**, so nobody can later quote it as the
no-caveat number.

**What this row IS and IS NOT.** `5CEBA9F31C7` is a **SIZING device**, not the
target: every row it produces is stamped `sizingDevice` / `notTargetDevice`, and
**its utilisation percentages are meaningless for this project — the ALM count
is the only field such a row is good for**, plus the per-hierarchy map, DSP by
owner and the timing picture. **The truth device remains 5CSEBA6U23I7 and only a
row fitted there may be cited for closure.**

**And it is incomplete in a way that is nameable, not vague:** register **14**,
edge reconciliation **built but not composed** so the console is in
**conservative edge mode** (and that constant is **not crack-free** — see the
EDGERECON finding), the arena's three non-triangle record paths tied at I53–I56,
and FORGE.SHADOW and FORGE.CLIFF uncomposed.

**One tool fact confirmed at launch, and it frees the lanes:** the runner printed
*"snapshot: 258 source(s) copied into the workspace; **the live tree cannot reach
this fit**"* and recorded digest `e3d1a29c7e66`. **So merging lanes while it runs
cannot corrupt it** — the CLAUDE.md live-tree trap does not apply to this
runner's own closure. The pinned worktree was belt-and-braces.



### WRITTEN BEFORE THE RECEIPT ARRIVES — HOW @diag-incomplete-14gaps MUST BE READ

CLAUDE.md: *"when the fit comes back: write down where you were BEFORE reading
it."* This is that note, and it is about the number rather than about the work
in progress, because **the direction of this row's error is decidable now and
will be much harder to argue after a concrete ALM figure is on the table.**

**MEASURED, at HEAD, with the register run BARE: 14 gaps** — 9 tie-offs
(I13, I20, I21, I34, I51, I53, I54, I55, I56) + 5 disconnected
(`zhao_terrain_normalmap`, `zhao_terrain_velocity`, `zhao_terrain_edgerecon`,
`zhao_forge_shadow`, `zhao_forge_cliff_ram`), 0 unbuilt, 0 uncited,
0 unresolvable.

**THE FIT'S CLOSURE WAS READ RATHER THAN ASSUMED.** The `zhao_console_core`
target in `design/fit_targets.yml` declares **258 sources**, matching the
runner's own *"snapshot: 258 source(s)"*. Searched for each disconnected block
by name:

| block | in the fitted closure |
|---|---|
| `zhao_forge_cliff` / `zhao_forge_cliff_ram` | **ABSENT** |
| `zhao_forge_shadow` | **ABSENT** |
| `zhao_terrain_normalmap` | **ABSENT** |
| `zhao_terrain_velocity` | **ABSENT** |
| `zhao_terrain_edgerecon` | **ABSENT** |
| `zhao_geom_lodstate`, `zhao_geom_ladderbank` | **ABSENT** |

**So this row measures a console with none of that silicon in it, and its ALM
number is a FLOOR, not an estimate of the finished machine.** The label says
`incomplete` and names the gap count, which is right; what the label does not
say is the **direction**, and the direction is the whole risk. **A missing
subsystem makes the number read LOW** — the flattering direction, the one
CLAUDE.md says nobody audits. The failure mode is specific and easy to predict:
*"the console came in at N, which is better than first light's 47,582"* — a
sentence that would be true, comparing two different machines.

**AND THE CONVERSE TRAP, because it is the one I nearly set myself.**
`zhao_forge_cliff` is **7,664 ALM, 18.3% of the ALM budget**
(`reports/BUDGET_HEATMAP.md:159`), and the ruling superseding R109 adopts the
rival `zhao_forge_cliff_ram` — **fitted at 976 ALM on the actual target part**,
smaller by a factor of six, *"plausibly the single largest lever in the tree."*
It is one of the five disconnected, so closing it is **−1 on the register and a
very large ALM saving at once**, and that is a real and important lever.

**It is NOT a saving against this receipt.** Neither rival is in the closure, so
the cliff contributes **zero** ALM to the number coming back. Quoting −6,688
against it would be subtracting a block from a total that never contained it —
the mismatched-comparison error, which this file already records twice. The
saving is real against a **future** console that composes the cliff; against
`@diag-incomplete-14gaps` it is zero.

**What the row IS good for**, restated: the ALM count of the composed 258, the
per-hierarchy map, DSP by owner, and the timing picture — on a **sizing** device
(`5CEBA9F31C7`), so **its utilisation percentages are meaningless here** and only
a row fitted on `5CSEBA6U23I7` may be cited for closure.

**What was in progress when it was launched**, so it is not lost: the
`check_guard_verdict` repair (**done** — RC 0 across 30 clients, arm-walker
false-positive fixed with two fired positive controls, commit `f8cd2507`), then
D-SDRAM-A verified at source (`925a4d25`) and D-NORMALS-A scoped (`61c96f69`).
**Next, in order:** collect this receipt into `reports/synthesis/`, then the two
R243 lanes as scoped above, then the FORGE.CLIFF adoption — whose two attached
conditions (the four `Warning (276020)` pass-through insertions accounted for,
and the bit-0 latch on `triangles_submitted_o` cleaned up) are **specific and
already written down**, and whose ruling also asks for **a leaf fit of the
GOLDEN `zhao_forge_cliff`** so the comparison stops being estimate-versus-fit.


## COUNTER IDS HAVE MOVED AGAIN — 46 entries inserted inside a list that is append-only by law

**Found 2026-09-23 by running every `check_*.py` in the tree rather than the
list I remember.** That habit is itself the §15.9 lesson from this campaign
(*"I told two lanes the tree is FULLY GREEN when I meant every gate on my list
is green"*), and it paid immediately: the sweep of 26 gates found **three reds
I was not running**, of which this is the serious one.

**THE LAW.** `spec/counters.md` §2 makes a counter's id its **zero-based
position** in `design/blocks.yml`'s `counter_catalog`, and makes that list
**append-only**: *"an existing index is never renumbered and never reused."*
The id is not decoration — it is what every `.zcap` capture's COUNTERS section
and DEBUG.COUNTERS key by.

**WHAT IS WRONG.** Two lanes inserted new names **inside** the locked prefix
instead of appending:

| commit | lane | inserted | at index |
|---|---|---|---|
| `afe77593` | TERRAIN.EDGERECON | 23 × `pageio_*` | 138 |
| `c5ac4def` | PARAMARENA merge | 23 × `paramarena_*` | 47 |

**46 insertions, and the consequence is measured rather than argued.**
`zhao_pkg.sv` declares `ZHAO_CNT_CMD_DMA_COMMANDS = 198`, and the catalog now
puts `cmd_dma_commands` at index **244**. `ZHAO_CNT_CMD_DMA_HPS_BYTES` 199 vs
245. `ZHAO_CNT_CMD_DMA_DROPS` 200 vs 246. **Exactly +46 on all three** — the
insertion count, arrived at independently. **Every counter from index 47 onward
now has an id that disagrees with the one the RTL emits**, silently.

**THE LAST GREEN IS KNOWN.** Walking all 218 commits that touch `blocks.yml`
with the gate's own parser: the catalog matched the lock exactly at `aafe0db7`
(285 entries), diverged first at `afe77593`, and index 47 itself moved at
`c5ac4def`.

**One further difference is NOT a defect and is recorded so nobody repairs it
twice.** The locked name `post_gather_vram_bytes_by_client` is gone from the
catalog entirely — **deliberately**, and `blocks.yml:431` says so in place:
*"`post_gather_vram_bytes_by_client` WAS HERE and is gone, 2026-09-21."* That
removal needs the lock **regenerated as a deliberate act**, not reverted.

**THE PART THAT SHOULD STING.** `tools/design/check_counter_ids.py` exists for
precisely this failure and its own docstring describes the previous instance:
*"twenty-three commits PREPENDED new entries at the top, and `frame_cycles`,
whose id is 0 … had become catalog index 150."* It was built under owner ruling
R19 to stop exactly this. **It works. It fired. It is a registered ctest
(`counter_ids_append_only`). Nobody ran it.** Two lanes then made the same
mistake again, 23 entries each — the same number as the original.

This is not the broken-instrument law: the instrument is fine. It is the
uncashed-cheque law applied to a gate — **a detector nobody consults is
indistinguishable from a detector that does not exist**, and the failure it was
built to catch recurred at the same magnitude within days.

**THE REPAIR, and why it is NOT being done right now.** It is mechanical: move
the 46 inserted names to **after** the locked prefix, keeping `blocks.yml`'s
first 285 in the lock's exact order, then run `--update-lock` **once**, for the
one documented removal, and commit catalog and lock together.

`design/blocks.yml` is a **shared file** and two lanes are live, one of them
(ARENAWIRE, I53–I56) working in the arena — the very area whose counters are
half the problem. Repairing it under them is the `git add <shared file>` trap
this campaign has already paid for twice. **Both lanes have been told: append at
the END, run the gate bare before committing, and do NOT run `--update-lock`**
— blessing the current catalog as the new baseline would make the id shift
permanent and unrecoverable, which is the one move that turns a repairable
defect into a frozen one. **The repair lands centrally when the lanes merge.**

**The other two reds from the same sweep**, recorded so they are not lost:

* **`tools/rtl/check_v3_banks.py` RC=1 — the gate's OWN SELF-TEST FAILS**:
  *"the registered-template audit did not fire for a primitive that is not in
  the tree … The hand list is then unguarded, which is how a gate quietly
  checks nothing."* A gate that reports its own blindness and is still in the
  tree reporting nothing. This one **is** the broken-instrument law, and it is
  the good version of it: the tool says so out loud.
* **`tools/maintenance/check_git_autocrlf_guard.py` RC=1** — 15 unguarded
  content-dependent git call sites. Lower severity; its own self-test passes
  (7 fire / 22 no-fire).

`check_dual18_map.py` and `check_dual18_atom_routes.py` returned RC=2 on a bare
run because they **require arguments** — that is my invocation, not a red.


### TRIAGE OF THE FOUR check_v3_banks FINDINGS — and a correction to my own commit message

**Commit `6fb106eb`, which re-enabled the gate, described its findings as
*"11,776 bits of payload-shaped state in FABRIC"* beside the observation that
ALM is the binding constraint. **That framing overstates two of the four and
would send the next person chasing an ALM lever that is not there.** Corrected
here, at source, having read each one:

**1. `shadow_metadata_m` (192 x 40 = 7,680 bits) — NOT IN THE CONSOLE'S
SILICON.** It sits inside `if (MIGRATION_SHADOWS) begin : g_migration_shadows`
(`zhao_texture_island_v3_top.sv:1679`), so with the parameter low it is not
elaborated at all. The island's own default is `1'b1`, but **every path that
reaches it passes zero**: `zhao_raster_texture_stage_v3.sv` defaults the
parameter to `1'b0` and forwards it, `zhao_raster_tile_pipe_v2.sv:963` passes
`1'b0` explicitly, and `zhao_raster_tile_pipe_v2.sv:1615` carries a `$fatal`
reading *"Packet-D MIGRATION_SHADOWS=0 exposed shadow state"*. There is even a
committed witness, `tools/design/packet_h_shadow_witness.py`, whose whole
purpose is to refuse the easy version of this check. **The comfortable
explanation is the correct one this time, and it is correct because it is
enforced in four places rather than asserted in a comment.**

**2. `uvw_m` (64 x 64 = 4,096 bits) — ONE WRITE PORT, ONE READ PORT.** Written
at line 923 on admission, read at line 1005, and nowhere else. The comment
above the read says *"THE uvw_m READ, MOVED OUT OF THE RESET BLOCK SO THE ARRAY
CAN INFER"* — somebody has already shaped it deliberately for RAM inference,
and a 1W/1R array is exactly what a simple-dual-port M10K is. **This is a
DECLARATION gap, which is what the gate's own remedy line says**: *"add it to
ACCOUNTED_STORES with its spec section, or mark it with `// V3-BANK: <NAME>`."*

**3. `material_m` V3-MULTIREAD — THIS ONE IS REAL, and it is the one worth
acting on.** Two distinct read addresses, confirmed by reading every reference:
`material_m[uvjoin_data_w[364:359]]` at line 1258 and
`material_m[owner_combine_owner_w[13:8]]` at line 2408. **A simple-dual-port
M10K has one read port and `ramstyle` cannot give it another**, so this array
either duplicates into two M10Ks or lands in fabric as **64 x 46 = 2,944
flops**. That is a genuine cost and a genuine question for a lane.

**4. `material_m` V3-WIDTH — 46 bits against a declared 48.** A spec-versus-RTL
mismatch to reconcile in one direction or the other, not a defect on its face.

**THE GENERAL LESSON, and it applies to the gate I just re-enabled:**
`check_v3_banks` resolves parameters from **the module's own defaults**, never
from the overrides its instantiations pass. That is sound for a standalone
audit and it is exactly why finding 1 reads alarming — the file says
`MIGRATION_SHADOWS = 1'b1` and the console says `1'b0`. **A gate that analyses a
module out of its elaboration context reports the module, not the machine**,
which is this tree's mismatched-comparison law wearing a tooling costume. The
gate is still worth having; its output is a list of questions, not a list of
defects, and three of these four were answered by reading the RTL for ten
minutes.


## R244 — TWO OWNER DECISIONS, 2026-09-23 (second batch). **(owner, explicit)**

### D-FORGESHADOW-C — D-FORGESHADOW-B IS LIFTED, NARROWLY. COMMISSION THE SUBSYSTEM.

> **Fabian: *"FORGE.SHADOW: lift D-FORGESHADOW-B narrowly and commission the
> full subsystem. Do not compose `zhao_forge_shadow` as an isolated wiring job.
> Close it end-to-end with its real producers/consumers and required tests.
> Preserve R3: no third projector port in v1; client A remains time-multiplexed.
> The existing 2-bit client-A owner encoding already reserved a third owner, so
> use that route and produce the written schedule/bandwidth proof R3 requires.
> If closing the subsystem actually requires changing that ratified law rather
> than using the already-authorized multiplex, stop and escalate."***

**AND THE OWNER CORRECTED ME ON THE PREMISE, which is the part worth keeping.**
I reported in `HANDOVER §15.11` that closing FORGE.SHADOW would **"re-author a
ratified law"**, and offered it as the one thing standing between the campaign
and zero that is not work. **That clause is STALE and the tree already said so.**

`design/contracts/FORGE.SHADOW.md:225` — dated **2026-09-21, two days before I
quoted it** — reads:

> *"The **client-A widening** clause is **STRUCK**: that widening was PERFORMED
> under R68 sub-build 4 and **R3 sanctions it**."*

and its §1 is titled **"THE CLIENT-A WIDENING WAS ALREADY PERFORMED, AND R3
SANCTIONS IT"**, continuing:

> *"R3 does not forbid this subsystem's use of client A. **It NAMES it**"* …
> *"`2'd2` and `2'd3` unallocated"* … *"So the instance-centre half of Route B
> needs **NO new law and NO owner decision**."*

**So the law was never the obstacle. R3 keeps client A a time-multiplex and
ANTICIPATES geometry, particles and FORGE.SHADOW sharing it; R68 already widened
the owner field to two bits and left `2'd2` free.** The correct action was to
authorise the subsystem *under* the existing law, not to ask for the law to be
rewritten.

**THIS IS R240 AGAIN, AND I RAN ONLY HALF OF IT.** R240 says a stated blocker
that has expired is evidence the ENTRY STOPPED BEING MAINTAINED, so re-read the
whole entry. I did re-check D-FORGESHADOW-B's *composition* premise — all five
modules exist on disk and not one is instantiated, which is still true — and I
did **not** re-read the CONTRACT, which is where the correction had been
recorded. **Checking one half of a stale entry and reporting the other half
verbatim is how a struck clause gets re-quoted to the owner as a live blocker.**
Thirteenth instance.

**WHAT IS ACTUALLY OUTSTANDING**, now that the law is not:

* **the written schedule / bandwidth proof R3 OWES and that has never been
  produced** — the contract calls this *"two halves and only one of them"*, and
  the owner's ruling requires the missing half;
* **Route B**: the private arena, an arbiter at GEOM.CLIP's door, and the
  absolute→rebased frame conversion;
* **Route A stays dead** — `zhao_geom_vattr`'s `done_o` deadlocks a hull, and
  note the citation drift the contract itself records: *"the citation `:490` is
  a comment banner; `done_o` is `:553`"*;
* **a third request arm on `zhao_part_project` claiming `OWNER_LOD = 2'd2`**,
  which is the arrangement R3 and the block's own header both prescribe.

**The escalation clause is narrow and stands:** if closing it turns out to need
the law changed rather than the authorised multiplex used, **stop and escalate**
rather than deciding it inside a packet.

### D-EARTH-A — MAX_FIELDS STAYS 16. MEASURE BEFORE CUTTING A CAPABILITY.

> **Fabian: *"keep MAX_FIELDS=16 for now. Do not cut it to 4. Sixteen is a
> ratified gameplay/capacity law; four would be a real v1 capability reduction
> just to save estimated area. Let the diagnostic fit tell us whether the bank
> actually lands in ALMs. If it does, first pursue the already-identified
> synchronous-read RAM implementation and spend roughly 8 M10K to remove the
> huge 16→1 register/mux bank. Only revisit MAX_FIELDS after measured
> optimization says we still need a feature cut."***

**The ordering is the ruling and it is this file's own art law in hardware
clothes: the number is an ESTIMATE and the cut is a CAPABILITY.** EARTHADAPT's
~3,350 ALM is **hand-counted, explicitly unfitted**, and the lane said so. Cutting
a ratified gameplay bound from 16 to 4 on the strength of an unmeasured number
— *before Quartus has said whether the bank lands in ALMs at all* — is deciding
a value from a measurement that has not been taken.

**The escape hatch the lane already found is the one to take first:** make the
bank read **synchronous**, adding one state to an ~80–100 cycle field run, and
the 5,200-bit uniform bank plus its 16→1 × 325-bit mux — **~2,600 of the ~3,350**
— becomes roughly **8 M10K** instead. That is the **trade ALMs for M10K** lever
this project already has as a standing rule: **ALM is the binding constraint at
~113% of the target part; M10K has historically had the slack**, and after R242
returned ~185 it sits near ~402 against 553.

**So the order is: read the diagnostic fit → if the bank is in ALMs, do the
synchronous-read RAM → only then, if measured optimisation still says so,
revisit MAX_FIELDS.** A feature cut is the last resort, not the first saving.

**Note what this does NOT license.** The synchronous read *"moves the 15.1
capture state after the directed evidence was taken"*, which is why the lane
correctly left it behind the fit that prices it. It is commissioned by this
ruling, not by a packet's own judgement, and its directed evidence must be
re-taken.


### THE OTHER 32, CLASSIFIED 2026-09-23 — and 10 of them must be RENAMED before they can be given an id

The append-only half of `check_counter_ids` is repaired (`77cdcb02`). What
remains is one class: **32 counters a block declares in its `counters:` row that
are not in `counter_catalog` at all**, so they have no id. Classified here so the
repair is mechanical, and **not performed**, because minting an id is permanent:
the list is append-only, so a name added today can never be removed, only
tombstoned. That is the lesson `post_gather_vram_bytes_by_client` had just
taught, one commit earlier.

**21 ARE PREFIXED AND UNAMBIGUOUS. Appending them at the END is safe and
renumbers nothing:**

| block | counters |
|---|---|
| `TERRAIN.BAKEREC` | `bakerec_*` × **11** |
| `TERRAIN.COMPCACHE` | `compcache_mat_cells`, `compcache_mat_oob` |
| `TERRAIN.EDGERECON` | `terrain_edgerecon_collisions`, `terrain_edgerecon_edges_fallback` |
| `TERRAIN.JOBISSUE` | `terrain_jobissue_ctx_refused`, `terrain_jobissue_ctx_src_mismatch` |
| `TERRAIN.SPDESC` | `terrain_spdesc_door_src_mismatch`, `terrain_spdesc_serve_no_door` |
| `TERRAIN.PAGESTREAM` | `pagestream_cells` |
| `TWOD.PLANE` | `twod_plane_disabled` |
| `TERRAIN.TESS` | `mat_unarmed` — the one judgement call in this group |

**10 ARE BARE GENERIC WORDS AND MUST NOT BE CATALOGUED AS THEY STAND**, and they
come from exactly two blocks:

* **`PART.CLIPFEED`** — `triangles`, `particles`, `dq_refused`, `dq_stray`,
  `range_refused`, `stall_full`
* **`GEOM.CLIPDOOR`** — `granted`, `idle_offered`, `switches`,
  `err_hold_broken`

**`counter_catalog` is a FLAT GLOBAL NAMESPACE keyed by name**, and the id is the
position in it. A catalog holding a bare `triangles` collides with the next block
that wants to count triangles — and the tree already shows the convention it
broke: `forge_prim_triangles_submitted` and `forge_cliff_triangles_submitted` are
both in the catalog, prefixed, precisely so they can coexist. **`granted`,
`switches`, `particles` and `stall_full` are signal names, not console counter
names.**

So the order is: **rename these ten to the prefixed convention in their blocks
first** (`clipfeed_triangles`, `clipdoor_granted`, …), which is an RTL and ledger
change owned by those blocks' authors, **and only then append**. Appending them
as they stand would freeze ten collisions into a list that cannot forget.

**Note what this does NOT say.** It is not evidence that those counters are
wrong, missing or unfired — only that they have no id, which means nothing
outside their own block can name them. **A counter with no id is invisible to
every `.zcap` capture's COUNTERS section and to DEBUG.COUNTERS**, which is the
whole reason the catalog exists.


### A NAMED QUESTION TO ASK THE RECEIPT, written before it lands

CLAUDE.md: *"Name the fit gates in advance. A fit nobody could state a question
for is a fit that should not run."* `@diag-incomplete-14gaps` was launched to
answer "how wrong are we", which is a real question but a vague one. Here is a
specific one it can answer, arrived at from today's `check_v3_banks` repair.

**THE QUESTION: did `zhao_texture_island_v3_top`'s PER-OWNER ARRAYS land in
M10K or in FABRIC?**

`check_v3_banks` reports `material_m` as **V3-MULTIREAD** — two distinct read
addresses:

* `material_m[uvjoin_data_w[364:359]]` at `:1258`, enabled by
  `material_join_accept_c`;
* `material_m[owner_combine_owner_w[13:8]]` at `:2408`, enabled by
  `owner_combine_fire_c`.

Two registered reads with **independent enables**. *"A simple-dual-port M10K has
one read port; `ramstyle` cannot give an M10K an extra port."* So this array
**cannot infer one**, and the same two sites read **four more arrays beside it**
— `material_refused_m`, `material_generation_m`, `owner_required_mask_m` and
`owner_required_mask_generation_m`. The gate named the widest; the shape is
shared. **Order of 3,000+ flops at 64 owners**, if they are indeed in fabric.

**ALL THREE FILES ARE IN THE FIT'S 258-SOURCE CLOSURE** —
`zhao_texture_island_v3_top.sv`, `zhao_texture_v3own.sv` and
`zhao_texture_material_combine_v3.sv` — verified against
`design/fit_targets.yml`, not assumed. **So the per-hierarchy map and the RAM
summary in this receipt price it, and nobody has to fit anything again to find
out.**

**IF THEY ARE IN FABRIC, the repair is the standing lever, not a redesign.**
Owner direction 2026-09-16 is *"spend M10K to buy ALMs"*, and R244 D-EARTH-A
applies exactly that reasoning to the Earth adapter's uniform bank on the same
day. Here it is cheaper still: **duplicate the bank, one copy per reader**, so
each copy has a single read port and both can infer. Two M10Ks in place of
thousands of flops, with no arbitration, no extra cycle and no throughput
change. **ALM is the binding constraint at ~113% of the target part; M10K sits
near ~402 of 553 after R242.**

**Serialising the two reads is the wrong answer here** and is worth naming so
nobody reaches for it: it would cost a cycle on the fragment path to save memory
that is not scarce, which is the trade backwards.

**AND THE HONEST CAVEAT:** this is a *structural* claim about port count, not a
measurement. Quartus may already be duplicating the array itself, in which case
the flops are not there and the finding is a documentation gap rather than an
area one. **That is precisely what the receipt settles**, and it is why the
question is written down before the number arrives rather than after.


## THE RECEIPT — `@diag-incomplete-14gaps`, 2026-09-23. THE FIT FAILED, AND IT STILL ANSWERED THE QUESTION.

**`status: incomplete:failed:quartus_fit.exe`, 8,030 s (2h14m).** Analysis &
Synthesis **succeeded** at 09:09:43; the fitter then ran for two hours and
**never produced an ALM count or an Fmax.** `partialStage:
analysis_and_synthesis`.

**THE PROVENANCE IS CLEAN, AND THIS IS THE FIRST CONSOLE ROW THAT CAN SAY SO.**
`rtlCleanAtHead: true`, `treeCleanAtHead: true`, `sourceCommit 18231903`,
`sourceDigest e3d1a29c7e66` over **258 hashed sources**. R80 demanded exactly
that before either completion run, *"because the first-light row is
`treeCleanAtHead: false`, so its digest describes nothing exactly."* This row
describes a commit exactly.

### LIKE FOR LIKE — same sizing device, same tool, the only honest comparison

| | `console-core-first-light` | `@diag-incomplete-14gaps` | factor |
|---|---|---|---|
| status | `ok` | **`failed:quartus_fit.exe`** | |
| **registers** | 56,031 | **381,585** | **6.8×** |
| **DSP blocks** | 151 | **359** | **2.4×** |
| **block memory bits** | 1,103,456 | **2,960,515** | **2.7×** |
| ALMs | 47,582 | **never produced** | — |
| Fmax | 18.5 MHz | **never produced** | — |
| `treeCleanAtHead` | **false** | **true** | |

### AGAINST THE TARGET, `5CSEBA6U23I7` — 41,910 ALM / 112 DSP / 553 M10K

* **DSP: 359 against 112 — 320% of the target.** Measured, not derived.
* **Memory: 2,960,515 bits against 553 × 10,240 = 5,662,720 — 52%. INSIDE, and
  comfortably.** This is the one piece of good news on the page, and R242's move
  of the deviation store to SDRAM is part of why.
* **ALMs: not measured. But a FLOOR can be derived and it is brutal.** A
  Cyclone V ALM carries **four registers**, so 381,585 registers need **at least
  381,585 / 4 = 95,396 ALMs** — **228% of the target's 41,910 — before a single
  LUT of combinational logic is counted.** Labelled DERIVED, not measured; it is
  a lower bound and the true number can only be larger.

**So the owner's prediction is now evidence rather than expectation.** *"It's
likely the console as architected now will be impossible."* At minimum **2.3×
over on ALMs from registers alone and 3.2× over on DSP** — and that is with
**five subsystems still missing from the closure.**

### THE NUMBERS ARE A FLOOR. Say it every time they are quoted.

The 258 sources contain **none** of `zhao_terrain_normalmap`,
`zhao_terrain_velocity`, `zhao_terrain_edgerecon`, `zhao_forge_shadow`,
`zhao_forge_cliff_ram`, `zhao_geom_lodstate` or `zhao_geom_ladderbank`. The
label says `incomplete` and names the gap count; **the direction is what the
label does not say, and the direction is that every figure above reads LOW.**

### WHAT THIS ROW DOES NOT SETTLE, and one of them is my own fault

* **The placement failure's CAUSE is not recovered.** `run_block_fit.ps1` writes
  its Quartus logs into a per-invocation workspace and removes it, so
  `quartus_fit.exe.log` is gone. **Two hours of machine time produced a failure
  whose error message no longer exists.** I will not guess between DSP
  exhaustion and ALM/register pressure — both are plausible and the log would
  have said. **The runner must preserve stage logs on failure; that is a
  concrete tooling fix and it is the first thing to do before spending another
  fit.**
* **The named question is UNANSWERED.** I asked, before the receipt landed,
  whether `zhao_texture_island_v3_top`'s per-owner arrays landed in M10K or
  fabric. That needs the per-hierarchy map, which the fitter never produced. The
  question stands and is now cheaper to ask than to re-fit.
* **Nothing here is a timing statement.** First light's `gpu_clk` at 18.5 MHz
  with −44 ns setup remains the only timing measurement, and R80 is right that
  *"the number that will dominate the fixing is not area."*

### WHAT TO DO WITH IT, in order

1. **Attribute the 359 DSP without spending a fit.** `tools/budget/dsp_census.py`
   exists for this. R80 records that its "SPECIFIED BUT NOT BUILT" list was
   false — it excluded three blocks that exist — so **the pre-fit expectation
   read LOW** and the census needs that repair checked before its total is
   quoted. **151 → 359 is +208 DSP and something owns them.**
2. **Preserve the fitter log** before the next attempt.
3. **Then optimise against attribution, not against a total.** The owner's
   standing direction is *"spend M10K to buy ALMs"*, and memory at 52% is the
   slack that makes it possible.

**R80's two-run plan still stands and this was run 2 of it — the MAP, on the
sizing device.** It did not produce the map. **Run 1, the VERDICT on the target
part, is now pointless to run**: a design needing ≥95,396 ALMs and 359 DSP
cannot place on 41,910 ALMs and 112 DSP, and four hours would return one bit
everybody can already derive from this page.


### ATTRIBUTING THE 359 DSP — what the census says, what it CANNOT say, and a correction to myself

**`tools/budget/dsp_census.py`, run bare: `DSP 150 COUNTED, 56 rows unpriced`.**
The synthesis measured **359**. **The instrument the project budgets by sees
42% of the DSP that is actually there.**

The census is honest about it — it prints *"PARTIAL MIXED EVIDENCE — not a
floor, not a ceiling"* and names the 56 unpriced rows, of which **28 have no fit
target at all, so nobody can measure them**. What was not known until today is
the **magnitude** of the gap. It is 209 DSP.

**AND R80'S FLAGGED REPAIR WAS NEVER DONE.** The census still reports
**3 rows of `unpriced_requirements:` naming a module that EXISTS** —
`TERRAIN.SHADE`, `FORGE.SHADOW` and `MEM.UPLOAD`, all three on disk while the
manifest calls them "RTL not built". Its own warning is the right one: *"a row
declared absent is a row nobody prices"*, so the total reads **LOW**. R80 named
this as a precondition for the completion fit and it went unrepaired; the fit
ran anyway.

**A CORRECTION TO MYSELF, MADE BEFORE IT COULD BE QUOTED.** Reading the leaf
rows I saw `zhao_project_service`, `zhao_geom_project` and
`zhao_terrain_project` at **33 DSP each** and reached for CLAUDE.md's
uncashed-cheque entry — *"the duplication is gone from the SOURCE. It is NOT
gone from the SILICON"* — and priced it at 33–66 DSP recoverable.

**That is wrong for the console, and one grep of the closure says so.** The
console instantiates **`zhao_proj_subsystem`**, which carries
`zhao_project_service` and therefore **ONE** `zhao_project_core`.
`zhao_geom_project` and `zhao_terrain_project` are **NOT IN THE 258**. The
cheque was cashed for the composed console; the three copies exist as
uncomposed leaf blocks. **The 359 is not projector duplication and nobody
should go looking there.**

**THE LEAF ROWS DO NOT DECOMPOSE THE 359, and it is worth saying why.** Summed,
the 48 blocks with a measured DSP row come to **677** — nearly double the
console's 359. That is not a contradiction, it is the census's own first
sentence: *"composition changes mapping, replication, pruning and packing."*
**Leaf sums are not a breakdown**, and treating them as one is this file's
mismatched-comparison law.

**Two rows are also known-stale in the flattering direction's opposite:**
CLAUDE.md records `zhao_terrain_normals` going from six multipliers to one on
2026-08-24 while the database still carries a **dirty 2026-08-20 row asserting
18 DSP** — so that 18 reads **HIGH**, and it is still 18 in the table today.

### SO THE ATTRIBUTION NEEDS A MAP, AND A MAP IS NOT A FIT

The per-entity breakdown lives in `.map.rpt`'s **"Resource Utilization by
Entity"**. The failed run saved only `.map.summary`, which carries totals and
nothing else — and its workspace, with `quartus_map.exe.log` and the full
`.map.rpt` in it, was removed on exit.

`run_block_fit.ps1` has **`-MapOnly`** and **`-KeepWorkspace`**, and Analysis &
Synthesis for these 258 sources completed in about **30 minutes** against the
fitter's 2h14m. **Launched at the SAME commit `18231903`**, so the attribution
describes exactly the machine the receipt measured rather than a later one —
like for like, which is the whole reason the pinned worktree exists.

**That run answers both open questions at once:** where the 359 DSP and the
381,585 registers live, per entity, and whether
`zhao_texture_island_v3_top`'s per-owner arrays landed in fabric.


## THE ATTRIBUTION — and the map report was on disk the whole time

**`reports/synthesis/blockpaths/zhao_console_core@diag-incomplete-14gaps.map.rpt`,
20.3 MB, harvested at 09:09 by the runner's own *"THE MAP REPORT IS HARVESTED
WHATEVER HAPPENED"* block.** It is `.gitignore`d at line 133 (`*.map.rpt` — a
size rule, not a kind rule), which is why `git status` never showed it and why I
launched a `-MapOnly` run to regenerate something I already had. **That run was
redundant and the redundancy was avoidable: `git status` is not an inventory.**

### THE MISSING ALM NUMBER WAS IN IT

| | measured |
|---|---|
| **combinational ALUTs** | **276,856** |
| dedicated logic registers | 381,585 |
| block memory bits | 2,960,515 |
| DSP blocks | 359 |

**Cyclone V packs 2 ALUTs per ALM, so 276,856 ALUTs need ≥ 138,428 ALMs.**

* **330% of the target's 41,910.**
* **122% of the SIZING device's 113,560** — so the design does not fit the part
  it was measured on, on **logic** as well as on DSP (359 against 342).

**That fully explains the placement failure**, and it is a better answer than
the register floor: 138,428 from ALUTs exceeds the 95,396 the registers alone
demand, so **logic, not flops, sets the floor.** The Quartus log is still gone;
this is arithmetic on the report rather than the fitter's own words.

### WHERE IT LIVES — top entities, **hierarchical totals, DO NOT SUM**

| ALUTs | registers | DSP | entity |
|---|---|---|---|
| 46,819 | 47,149 | 88 | `zhao_shell_top_v2:u_shell` |
| 40,093 | 48,614 | 15 | `zhao_field_host_v2:u_field_host` |
| 34,041 | **100,561** | 0 | **`zhao_geom_drawjob:u_geom_drawjob`** |
| 30,765 | 42,660 | 15 | `zhao_field_v3_engine:u_fabric` |
| 30,519 | 31,737 | 86 | `zhao_geom_bin_pipe_v2:u_render_bin` |
| 29,198 | 28,958 | 80 | `zhao_raster_tile_pipe_v2:u_tile` |
| 15,557 | 38,999 | 3 | `zhao_forge_assemble` |

**`zhao_geom_drawjob` holds 100,561 registers — 26.4% of every flop in the
console — with ZERO DSP.** A job-dispatch block holding a quarter of the
machine's state in fabric is **exactly** what `check_v3_banks`'s §21.8 language
was written for: *"stop before another long fit when a supposedly banked payload
shows up as thousands of registers."* **This is the single largest lever on the
page**, and with memory at **52% of target** it is the standing *"spend M10K to
buy ALMs"* trade in its purest form.

### THE DSP BREAKDOWN, BY KIND — 78 blocks each holding ONE multiply

| mode | blocks |
|---|---|
| Independent 9×9 | 42 |
| Two Independent 18×18 | 134 |
| Independent 18×18 plus 36 | 52 |
| Sum of two 18×18 | 53 |
| **Independent 27×27** | **78** |
| **total** | **359** |

412 multipliers in total (90 signed, 159 unsigned, 163 mixed-sign).

**A Cyclone V DSP holds TWO independent 18×18 but only ONE 27×27.** So the 134
"two independent 18×18" blocks carry **268** multiplies, while the **78 27×27
blocks carry 78**. **Narrowing a 27×27 operand to ≤18 bits lets two share one
block** — up to **~39 blocks** recoverable if all 78 could be narrowed, and any
subset pays proportionally.

**And two small blocks are startlingly DSP-dense:** `zhao_geom_attrpack` at
**45 DSP for 1,184 ALUTs** and `zhao_geom_attrsetup` at **45 DSP for 938
ALUTs**. **90 DSP — 25% of the console's total — in two blocks with ~2,100 ALUTs
between them.** That ratio is the signature of wide multipliers, and they are
the first place to look for 27×27s.

### THE ORDER THIS SUGGESTS, for the optimisation phase

1. **`zhao_geom_drawjob`'s 100,561 registers.** Find what the payload is and
   whether it belongs in M10K. Largest single lever, and memory has the slack.
2. **`zhao_geom_attrpack` / `zhao_geom_attrsetup`'s 90 DSP.** Check operand
   widths against the 78 27×27 count; each pair narrowed to 18×18 frees a block.
3. **`zhao_shell_top_v2` at 46,819 ALUTs and 88 DSP** — the biggest single
   entity, and a shell, so ask what of it the console actually needs.

**None of this is a fit result.** It is Analysis & Synthesis, pre-placement, so
packing, replication and pruning have not happened. **It is attribution, which is
what the optimisation phase needs, and it cost nothing** — the report was
already on disk.


## `zhao_geom_drawjob`: 98,304 FLOPS IN AN ARRAY WHOSE OWN COMMENT SAYS IT IS AN M10K

**The single largest optimisation lead in the tree, and the comment beside it
states the opposite of what was measured.**

### THE MEASUREMENT — certain

`zhao_geom_drawjob:u_geom_drawjob` reports **100,561 dedicated logic registers
and 0 block memory bits**, all of it the block's **own** (`100561 (100561)` — no
sub-entities). That is **26.4% of every flop in the console.**

The array is at `fpga/rtl/geometry/zhao_geom_drawjob.sv:235`:

```systemverilog
logic [383:0] pal_q [XFORMS];      // XFORMS = 256
```

**256 × 384 = 98,304 bits.** The block's remaining logic accounts for the other
~2,257 flops, which is an ordinary size for the rest of it. **The array is the
block.**

### THE COMMENT DIRECTLY ABOVE IT — refuted by the report

> *"the palette. A REGISTERED READ WITH AN ENABLE, so the row holds for the
> whole draw and **Quartus infers M10K rather than 98,304 flops**."*

and at the write:

> *"The array itself has NO reset, which is what lets it infer M10K."*

**Both statements about the shape are true.** The read *is* registered with an
enable (`:408`), the array *has* no reset. **And the inference did not happen.**
The number the comment names as the bad outcome — **98,304 flops** — is exactly
what the synthesis report measured. **This is "the comment is not the mechanism"
with the mechanism's own number written in it.**

### WHY, stated at the right confidence

**EVIDENCE (strong):** Quartus emitted **zero** RAM-inference messages of any
kind about `zhao_geom_drawjob` — not one `Info (276…)`, not even an "uninferred
due to…" rejection. By contrast it explained itself about other blocks in the
same run:

* `zhao_twod_plane:u_twod_plane|pal_q` — *"uninferred due to inappropriate RAM
  size"*
* `zhao_twod_sampler:u_twod_sampler|bind_base_q` — *"uninferred due to
  asynchronous read logic"*

**A rejection with a reason means Quartus considered the array and refused it. No
message at all means it never presented as a RAM candidate.** That is a
different and more informative failure.

**HYPOTHESIS (to be tested by a lane, NOT asserted here):** the write at `:405`
is the reason —

```systemverilog
for (int unsigned k = 0; k < 12; k++)
  pal_q[int'(px_index_i)][32 * k +: 32] <= px_m_i[k];
```

**Twelve separate partial-width slice assignments**, not one write. Functionally
it is a full-width write — all twelve fire under one condition at one address —
but structurally it is twelve partial writes, and the canonical inferable form
is a **single full-width assignment**: build the 384 bits in a combinational
concatenation and write `pal_q[idx] <= pal_wr_c;` once. **That is a small,
well-understood change and it is a hypothesis until a `quartus_map` says
otherwise — which costs minutes, not a fit.**

### WHAT IT IS WORTH

**98,304 bits is ~10 M10K** (98,304 / 10,240 = 9.6). Against the flops it
currently spends, at four registers per Cyclone V ALM, that is on the order of
**24,576 ALMs** — and the **entire target device is 41,910**. Even allowing that
registers pack with logic rather than standing alone, **this one array is a
double-digit percentage of the whole budget.**

**And the memory to pay for it exists:** block memory measured **2,960,515 bits
= 52% of the target's 5,662,720**. The owner's standing direction is *"spend
M10K to buy ALMs"*; **this is that trade with the arithmetic already done.**

### HOW TO CONFIRM IT CHEAPLY, so nobody spends a fit on it

**`quartus_map` alone answers this** — RAM inference is an Analysis & Synthesis
decision, and R109 already records the distinction: *"It is a `quartus_map`, not
a fit … a map is minutes."* Change the write, map the block standalone, and read
two things: whether `zhao_geom_drawjob` appears in the **RAM Summary**, and
whether its register count drops by ~98,000. **Both are in the report the runner
already harvests.**


### CANDIDATE 2 — `zhao_forge_assemble`, ~34,840 bits in flops, with BOTH classic blockers visible

**And the sweep that found it also says the problem is NOT systemic**, which is
the more useful half.

Ranking every entity by its **OWN** registers against its block memory bits:

| own registers | membits | entity |
|---|---|---|
| **100,561** | **0** | `zhao_geom_drawjob` |
| **37,638** | 2,048 | `zhao_forge_assemble` |
| 24,795 | 25,344 | `zhao_field_v3_exec` |
| 11,141 | 17,128 | `zhao_cmd_exec` |
| 5,115 | 85,282 | `zhao_field_host_v2` |
| 4,920 | 571,114 | `zhao_shell_top_v2` |
| 4,491 | 187,308 | `zhao_vertex_arena` |

**Every other register-heavy block carries substantial block memory — their
arrays DID infer.** `zhao_shell_top_v2` holds 571,114 bits against 4,920 own
flops; `zhao_vertex_arena` 187,308 against 4,491. **So this is not a tree-wide
coding problem. It is two blocks**, and one of them is 2.7× the other.

**`zhao_forge_assemble`'s arrays** (`fpga/rtl/forge/zhao_forge_assemble.sv`):

```systemverilog
logic [POSW-1:0] pos_q [MAX_VERTS];   // POSW = 1+21+21 = 43, MAX_VERTS = 520
logic [23:0]     inv_q [MAX_VERTS];
```

**520 × 43 + 520 × 24 = 34,840 bits**, against 37,638 own registers measured —
the arrays plus ordinary control state. **≈ 3.4 M10K if they inferred.**

**THIS ONE'S CAUSE IS BETTER EVIDENCED THAN DRAWJOB'S, because both known
blockers are visible in the source:**

* **`:538` — an ASYNCHRONOUS read.**
  `wire [POSW-1:0] pos_rd_c = pos_q[rd_a_q];` — combinational, not registered.
  **Quartus names exactly this failure elsewhere in this same report**:
  *"`zhao_twod_sampler|bind_base_q` is uninferred due to **asynchronous read
  logic**"*.
* **`:661` — a RESET on the array.** `pos_q[k] <= '0;`. **`zhao_geom_drawjob`'s
  own comment identifies this as fatal** — *"The array itself has NO reset,
  which is what lets it infer M10K"* — so the tree already knows the rule and
  this block breaks it.

**And the same silent signature:** Quartus emitted nine `Info (276…)` messages
about `zhao_forge_assemble`, but the only two "uninferred" ones name a
**sub-module** (`zhao_raster_rcp24_v4:u_rcp|p_k_q`, `p_tok_q`). **Nothing was
said about `pos_q` or `inv_q` at all** — they never presented as candidates,
exactly as in drawjob.

**A caution on sequencing:** the **SHADOWRIDE lane is riding
`zhao_forge_assemble` right now**. This is recorded as a lever for the
optimisation phase, **not handed to that lane** — changing a block's storage
shape underneath a packet composing against it is how two correct changes
produce one broken merge.

**Together the two blocks are ~133,000 bits sitting in flops that ~13 M10K would
hold**, with block memory measured at **52% of the target**. That is the whole
of *"spend M10K to buy ALMs"* in two files.


### THE DSP LEVER: `zhao_geom_attrsetup` CASTS ITS OPERANDS TO THE WIDTH OF THE RESULT

**First, a correction to my own entry two sections up.** I wrote *"90 DSP — 25%
of the console's total — in two blocks with ~2,100 ALUTs between them"*, in the
same page where I had written **"hierarchical totals, DO NOT SUM"**. The map's
full hierarchy name settles it:

```
|zhao_console_core|zhao_geom_attrpack:u_geom_attrpack|zhao_geom_attrsetup:u_attrsetup
```

**`attrsetup` is a CHILD of `attrpack`**, so attrpack's 45 *includes* it. It is
**45 DSP, not 90** — and since attrpack's own DSP is therefore **zero**, **all
45 belong to `zhao_geom_attrsetup`**, a **938-ALUT leaf**. That is still
**12.5% of the console's 359 DSP in one small file**, which is the finding; the
doubling was mine.

### THE CAUSE IS VISIBLE IN FOUR LINES, and it is operand width

The declared widths (`fpga/rtl/geometry/zhao_geom_attrsetup.sv`):

| signal | width |
|---|---|
| `ax_i … cy_i` | **21** signed |
| `va_i, vb_i, vc_i` | **32** signed |
| `cx_bx, ax_cx, bx_ax` | **22** |
| `w0_0 … w2_0` | 46 |
| `n0_c` | 96 |
| `dndx_c, dndy_c` | 72 |

And the multiplies:

```systemverilog
:122  w0_0   = -(46'(cx_bx) * 46'(by_i)) + (46'(cy_by) * 46'(bx_i));
:131  n0_c   = 96'(w0_0) * 96'(va_i) + 96'(w1_0) * 96'(vb_i) + …
:134  dndx_c = ((-(72'(cy_by))) * 72'(va_i) + … ) <<< PIXEL_SHIFT;
```

**Every operand is cast to the width of the RESULT before the multiply.** The
cast sizes the multiplier; the data does not.

* `:122` — a **22 × 21** product, built as **46 × 46**.
* `:131` — a **46 × 32** product, built as **96 × 96**.
* `:134/:136` — a **22 × 32** product, built as **72 × 72**.

A Cyclone V DSP does one **27×27** or two **18×18**. A 46×46 needs roughly
**four** 27×27 partials where 22×21 needs **one**; a 96×96 needs roughly
**sixteen** where 46×32 needs about **four**. **That is where the report's 78
"Independent 27×27" blocks come from.**

**ESTIMATED, WITH THE ARITHMETIC SHOWN — not measured:** at natural widths the
six `:122`-class products need ~6 blocks rather than ~24, the three `:131`-class
~6 rather than ~36 (capped by what 45 actually contains), and the six
`:134`-class ~6 rather than ~18. **Order of 18 blocks against the 45 measured,
so roughly 27 DSP — about 7.5% of the whole console's DSP — from one file.**

### THE FIX IS STANDARD AND THE RISK IS LOW

**Cast the PRODUCT, not the operands.** In SystemVerilog `$signed(a) *
$signed(b)` has width `$bits(a) + $bits(b)` and sign-extends into a wider
target, so the result is unchanged while the multiplier is built at the size the
data needs:

```systemverilog
w0_0 = -46'($signed(cx_bx) * $signed(by_i)) + 46'($signed(cy_by) * $signed(bx_i));
```

**Bit-exactness is the thing to prove, not to assume** — the products are
identical in value, but this block feeds the ratified attribute setup and the
change must be shown equivalent against its oracle, not argued.

### CONFIRM IT WITH A MAP, NOT A FIT

DSP inference is an Analysis & Synthesis decision. **Map `zhao_geom_attrsetup`
standalone before and after and read the DSP Block Usage Summary** — the same
table that produced the 78 27×27 count. **Minutes, and the runner already
harvests the report.**

**Note what this does NOT claim:** Quartus does prune redundant sign-extension
in some cases, so part of the width may already be optimised away. **45 DSP in a
938-ALUT leaf is strong evidence that it was not**, but the map is what settles
it — the same standard applied to `zhao_geom_drawjob` above.


### LEVER 2 DOWNGRADED FROM "STRONG" TO "WEAK", by a sweep that was meant to GENERALISE it

I swept `fpga/rtl` for the wide-cast multiply shape expecting to find more of
it. **The sweep found the shape in ten files and then disconfirmed the
hypothesis**, which is the more useful outcome and the opposite of what I was
looking for.

| file | wide-cast sites | **measured DSP** |
|---|---|---|
| `zhao_geom_quat2mat` | **9** | **1** |
| `zhao_geom_clipread` | 6 | 2 |
| `zhao_geom_attrsetup` | 6 | **45** |
| `zhao_surface_stamp` | 4 | **0** |
| `zhao_raster_rcp24_mul` | 4 | 3 |
| `zhao_geom_mat3x4_mul` | 4 | 3 |
| `zhao_geom_paramarena` | 3 | **0** |
| `zhao_terrain_pagestream` | 2 | **0** |
| `zhao_terrain_loadq` | 2 | **0** |
| `zhao_raster_attrdiv_v2` | — | **0** |

**Nine files carry the pattern and cost between 0 and 3 DSP.
`zhao_geom_quat2mat` has the MOST instances — nine — and uses ONE DSP.** So
**Quartus prunes redundant width as a matter of course**, exactly the caveat I
attached to the finding and then under-weighted. **The casts are not, in
general, expensive.**

**So what IS attrsetup's 45?** Most likely the arithmetic itself. The block
computes three edge functions (2 products each), three interpolated values
(`n0_c`, 3 products at genuinely 46×32) and two gradients (6 products at 22×32)
— **fifteen multiplies, averaging 3 DSP each**, which is an ordinary price for
products of that size. **A 46×32 product needs several 18×18 partials no matter
how it is written.**

**REVISED ESTIMATE: the recoverable part is the six NARROW products at `:122`
(22×21, which should be ~1 DSP each and may be paying ~3), so on the order of
10–12 DSP, not 27.** Still worth having — 3% of the console — but **it is no
longer a headline and it is no longer "strong".** Lever 2's confidence in the
table above should read **weak**, and the number should read **~10**.

**THE DISCIPLINE THAT CAUGHT IT:** the hypothesis predicted that the same shape
elsewhere would cost DSP. It was testable in one grep plus one column of the
report, and it failed. **A lever estimated from reading code and never checked
against the measurement is how a fortnight gets spent on 3%.**

### AND A NEAR-MISS I AM RECORDING RATHER THAN QUIETLY FIXING

While ranking DSP owners I mis-indexed the report's columns and read
**`zhao_geom_vattr|zhao_raster_rcp24_v4` at 104 and 120 DSP** — which would have
made the reciprocal units the dominant consumer at ~250 of 359, and I was one
sentence from reporting it. **Those numbers were ALUT counts from the wrong
field.** The table has ten columns and the full hierarchy name is field 7, not
the last one; an earlier filter on the last field matched nothing, which is what
made me look again.

**The tell was that it did not reconcile**: 250 DSP in one subtree plus
`zhao_shell_top_v2`'s 88 already exceeds the console's own 359. **A number that
cannot fit inside its own total is a parsing error, not a finding.**

### THE DSP DECOMPOSITION, CORRECTLY PARSED — indentation is nesting

```
  359  zhao_console_core
   88    zhao_shell_top_v2          <- 24% of all DSP
   86      zhao_geom_bin_pipe_v2
   80        zhao_raster_tile_pipe_v2
   23          zhao_raster_texture_stage_v3
   45    zhao_geom_attrpack         <- all of it zhao_geom_attrsetup
   39    zhao_proj_subsystem
   33      zhao_project_service / zhao_project_core
   21    zhao_geom_skin_norm
   20    zhao_part_collide
   15    zhao_field_host_v2
```

**The three largest direct children — shell (88), attrpack (45) and the
projector subsystem (39) — are 172 of 359, 48%.** And **80 of the shell's 88 are
`zhao_raster_tile_pipe_v2`**, which no lever above has examined. **That is where
the next DSP question should be asked**, not at attrsetup.


## 96 LANE WORKTREES, AND TWO OF THEM HOLD WORK THE BRANCH DOES NOT

**Found while verifying BURSTTRUTH's closure.** `git worktree list` returns
**99 entries**; 96 are lane worktrees. **Sizes MEASURED only in part, and the partial measurement says my first
estimate read LOW.** Three sampled worktrees came to 0.6, 1.1 and 1.2 GB, from
which I wrote *"roughly 90 GB"*. A later listing of the LARGEST ones found
`gz-procmat` at **3.8 GB** and `gz-layere` at **3.7 GB** — three times the
sample. **So 90 GB is a floor, not a total**, and the true figure is unknown:
the full sum was started and then STOPPED, because it walks ~96 complete
checkouts and no decision here depends on the precision. **What is decided by
the untracked-files question below, not by the size.** Nothing prunes them, which is the `.gitignore` chapter's
exact shape — *"making waste invisible to your tooling is not the same as
removing it"* — except here it was never invisible, just never looked at.

**Disk is at 747 GB free, so this is NOT urgent**, and it is recorded rather
than acted on for a specific reason below.

**93 of 96 are FULLY MERGED** into `claude/ceiling-architecture-20260912`, so
their tracked content is safe and the directories are removable without loss.
**I did not remove them**, because a worktree can hold deliberately-kept
UNTRACKED files — BURSTTRUTH's own report says *"the raw per-run logs are left
uncommitted under `subagents/20260923-bursttruth/`"*. **`git worktree remove`
refuses on modified TRACKED files and says nothing about untracked ones**, so
the obvious safety net does not cover the thing actually at risk. **A sweep here
needs the owner's word, or a rule about what untracked lane output is worth
keeping.**

### THE THREE THAT ARE NOT MERGED

| branch | ahead | age | state |
|---|---|---|---|
| `gz/shadowride` | — | live | **RUNNING RIGHT NOW. Do not touch.** |
| `gz/engine1` | 4 | 3 days | *"both targets REFUSED, and both recorded blockers name…"* — a refusal packet; its value is the recorded blockers |
| `dsf01/divider-fusion` | 2 | 6 days | **see below** |

### DSF-01 — a COMPLETED experiment with a NEGATIVE verdict, and the input to that verdict has since changed

**The packet is exemplary and its verdict is correct as written.** Owner
experiment DSF-01, projector divider compare/subtract fusion, run to a stop on
2026-09-17:

* arithmetic **proved for the full domain including `d = 0`** — unsat;
* **the prover itself proved** — four mutated candidates all `sat`, including
  the two traps the guide names;
* ten projector regressions bit-exact;
* on a **matched seed-2 pair differing by exactly one file** (digests confirm
  it): **−1,492 ALMs, six times the policy threshold**;
* and it turns a leaf that **misses 100 MHz by −31.5 ns TNS into one that meets
  it: 94.79 → 101.49 MHz, setup TNS to zero.**

**It was still rejected, and rightly.** Two hold paths violate at −0.068 and
−0.011 ns — **and the divider is in neither**; both launch from a top-level
input into the configuration matrix. *"What moved was the floorplan."* Under the
policy an unsatisfied hold requirement is a reject, and the packet explicitly
refuses to soften it: *"the guide anticipated that exact temptation and said a
timing trade is rejected even if it saves 1,000 ALMs."*

**WHAT IS WORTH RE-EXAMINING IS NOT THE VERDICT BUT THE EVIDENCE BEHIND IT.**
The packet says so itself: **the hold numbers were measured with VIRTUAL pins,
where a top-level input has no real launch model** — *"which is exactly why the
policy specifies physical pins"* — and **the physical-pin pair CANNOT BE
PRODUCED for this block**: `zhao_geom_project` presents **344 ports**, far more
than the package has, and the fitter fails in ~66 s. **Both halves failed
identically**, which the packet correctly calls better evidence than one failure
— it says the *boundary* is the problem, not the patch.

**So the reject rests on a measurement the packet states is not the one the
policy asks for, taken in a configuration that cannot produce the one it does.**
Resolving it needs a **pin-reducing fit wrapper** (the technique the G8B fit top
uses) — a new artifact with its own correctness question, which the packet
declined to start because the guide says not to broaden the task. **That
restraint was right.**

**WHY IT IS WORTH THE OWNER'S ATTENTION NOW, six days later:** on
2026-09-17 the console's size was an estimate. **It is now measured at ≥138,428
ALMs — 330% of target — with `gpu_clk` at 18.5 MHz.** A lever worth **−1,492
ALMs and +6.7 MHz on a leaf that otherwise misses 100 MHz** is a different
proposition against that number than against a guess. **The decision to
authorise the pin-reducing wrapper — which is what would let the policy actually
answer — is the owner's, and it is not proposed here.**

## REGISTER 13 -> 12: BOTH MERGES LANDED, AND THE CLIFF ENTRY I WROTE WAS WRONG

**CLIFFADOPT merged at `15060551`; PARTDEPTH merged at `0c0aa5d9`. Measured on
the merged tree, BARE: `MANDATORY GAPS REMAINING : 12` = 8 tie-offs +
4 disconnected + 0 unbuilt + 0 uncited + 0 unresolvable. `gate_sweep` RC 0, all
24 gates matching the committed baseline. Tree clean, local == remote.**

**I51 is CLOSED and it moves pixels.** `zhao_part_clipfeed` now carries
`p_depth_test_i` / `p_depth_write_i` and emits `o_frag_state_o`, from
`zhao_part_expand`. The constant it replaced meant `Z_TEST_EN=0` and
`Z_WRITE_DIS=0` -- particles always passed depth and always wrote it -- and the
pass-7 law is the opposite on **both** bits. The state **travels with the record
in the ring**, not read live at the emit, because a live read would pair record
A's geometry with record B's state on a stall **with every accepted/emitted
counter still balancing**. The live-read variant is committed as a mutant and
fires: `offered=8 emitted=8 disagreements=6 held_value_seen=8`.

**CLIFFADOPT discharged both R117/R142 conditions and REFUSED composition, and
it refused three stale premises I put in its brief** -- a golden leaf fit that
had already been run (the **third** false-absence claim on that one gate), a
ledger ask already discharged by `gz/forge4`, and R117's conditions quoted
un-amended when R142 had amended them. **§15.11 has been corrected**:
`zhao_forge_cliff_ram` is **not** "one lane, two named conditions". It has **no
producer** -- `solid...*_o` as an output port is zero hits tree-wide, every
`vdist` in `zhao_console_core.sv` is a comment, and the only live references are
in the generated **pricing** top fed by `assign u09_src = {16{u09_lfsr_q}}`.
**An LFSR is not a producer.** It is the same kind of work as the other three
disconnected blocks, and my entry understated it.

**AND THE COST NOBODY HAD READ, now recorded where the saving is quoted:** both
labelled rows carry full timing fields and every prior reading used the **area
fields only**. Golden hold **+0.263 ns** against candidate hold **-4.140 ns** --
a sign change, and unlike the latch and the four warnings this **is** a cost of
the swap. The saving itself is **fit-minus-fit**: ~976 ALM against ~6,674, so
**5,698 ALM, 13.6% of the device**, both halves `rtlCleanAtHead: true`. The
older *7,664 ALM / 18.3%* figure was an estimate and is superseded.

## A NARROWED `remote.origin.fetch` HAD FROZEN EVERY REMOTE-TRACKING REF

**Found immediately after pushing, because `git push` reported
`a8d4443d..0c0aa5d9` and `git rev-parse origin/claude/ceiling-architecture-20260912`
came back `f98f5846` -- a commit from 2026-09-19.** `git ls-remote` showed the
real remote at the new commit, so nothing was lost; the **instrument** was
broken, not the push.

The cause: `remote.origin.fetch` had been reduced to a single line --

```
+refs/heads/zixxtrixx-v8-closeout:refs/remotes/origin/zixxtrixx-v8-closeout
```

-- with the default `+refs/heads/*:refs/remotes/origin/*` gone. `.git/config`
was last written **today at 14:08**, and the config is in the **common** git
dir, so **all 96 lane worktrees shared it**.

**Two consequences, and the second is the one that matters.** `git fetch origin`
stopped updating `refs/remotes/origin/*` -- and so did `git push`, which updates
a tracking ref only if the remote's fetch refspec maps it. So the tracking refs
stood still at whatever they last held while the real remote moved on.

**"local == remote" has been a closure check in this campaign, and for some
window it was reading a frozen ref.** It is the broken-instrument law in its
purest form: a comparison against a value that cannot change reports agreement
or disagreement with equal confidence and means neither. Here it happened to
read **dis**agreement, which is why it was noticed at all; had HEAD matched the
frozen ref it would have read as a clean sync forever.

**Repaired**: the wildcard refspec is restored, `git fetch` brought
`refs/remotes/origin/*` back (and pulled in 8 branches it had never seen), and
`origin/claude/ceiling-architecture-20260912` now resolves to `0c0aa5d9`,
equal to HEAD. **`git ls-remote` is the truth; a remote-tracking ref is a
cache, and a cache nobody refreshes is a stale number with a reassuring name.**

## ADOPTION: THE OWNER VACATION DIRECTIVE OF 2026-09-23 (recorded ONCE, here)

**`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt`, delivered on the
integration branch as `460296f9` + `ead6f107`, reviewed against `13b22987`,
ADOPTED 2026-09-25.** Its own words: *"The owner's commit or delivery of this
file adopts the whole directive; no second ratification message is required."*

**It supersedes the approval holds.** Recorded here rather than reproduced: the
directive is the single source of truth and it explicitly forbids *"duplicating
this document into multiple competing sources of truth."* Pointers were added at
the top of `CLAUDE.md` and of `reports/HANDOVER-20260919.md`, which is what it
asks for, and nothing else is copied.

**WHAT IT CHANGES ABOUT HOW THIS CAMPAIGN RUNS, one line each:**

* **The approval loop is over.** *"Use your own brain. You are the implementation
  architect, not a relay... Do not stop at another list headed OWNER DECISION."*
  Standing delegated authority over architecture, formats, address maps, command
  extensions, ownership, identity, scheduling, allocation, numeric
  representation, compatibility, validation, test infrastructure and order --
  **including amending specs and older rulings**, provided what is superseded is
  STATED rather than silently reinterpreted.
* **It does not expire** at the end of a session, packet, day or context window.
* **Section 15.13 IS CLOSED.** Every hold it listed is decided in the directive.
* **The limits are explicit, and they are capability limits rather than resource
  limits:** no deleting a feature, no 16->4 fields, no removing Gouraud or detail
  normals, no shrinking the guaranteed giant, no replacing a live path with
  testbench stimulus, no waiving a correctness failure, no calling reduced work
  equivalent. **Shipping target stays `5CSEBA6U23I7`**; a larger diagnostic
  target is a MEASUREMENT target only.
* **A measured impossibility is a finding**, not permission to invent a pass:
  keep the correct slower configuration, record the limit, continue elsewhere.
* **Two implementation workers**, coordinator integrating. Supersedes the older
  one-worker and three-packet staffing notes. Count descendants; no nested spawns
  to evade it; never leave stray jobs impersonating an active lane.
* **Push protocol, and it caught us the day it arrived:** fetch and inspect new
  owner commits BEFORE each integration push. *"A push sends local commits OUT;
  it does not fetch new owner instructions."* A non-fast-forward rejection is a
  signal to integrate -- **never force over an owner commit.** This directive was
  found exactly that way: a push was rejected, and **the rejection was the
  notification.** Nothing else would have told us.

**AND IT CORRECTS THREE THINGS WE HAD WRITTEN, all in the direction the lanes
were already moving:**

1. *"The terrain smoke actually reports valid page CRCs and 128 replayed terrain
   triangles, but all 128 are degenerate. Do not repeat the older claim that CRC
   failures make every terrain path unreachable."* TERRAINUV measured this
   independently on 2026-09-23; the directive ratifies the correction.
2. *"Do not claim a visible shadow from a smoke that never publishes a
   creature-form page."* SHADOWRIDE had said so itself.
3. *"The new bandwidth ledger's worst-on-worst oversubscription is NOT a measured
   board FPS result, and the optimistic case is not a guarantee."* -- section
   15.15's own caveat, ratified, plus the instruction to charge fragmentation,
   turnaround, refresh, queue and drain costs, and the reminder that **a 64-byte
   fabric request is not one 16-byte BL8 burst.**

**THE DECISION RECORD FORMAT for everything from here**, from section 10:
question; chosen option; reason and alternatives; constraints/cost;
code/tests/compatibility consequences. Then execute. Further decisions belong in
the implementation packets themselves, per section 10 item 3 -- not in a new
essay. The directive is explicit that an audit-only packet for a decision already
delegated here is not to be commissioned again.

### DECISION RECORD 1 under the delegation: COMPOSED_NAV MOVES, because the directive's range collides with POST.ECHO

**Question.** Section 1 of the vacation directive allocates
`COMPOSED_MATERIAL [0x058B_0000, 0x05AB_0000)` and
`COMPOSED_NAV [0x05AB_0000, 0x05CB_0000)`, each 2 MiB / 256 slots x 8 KiB, and
instructs: *"Before enacting these ranges, check the LIVE map, guard, allocator,
and branches being integrated. If a newer allocation occupies one, choose another
proved-free range and record it without asking."* Do they collide?

**MATERIAL does not. NAV DOES.** `spec/memory_rules.md:865` places **POST.ECHO's
capture at `0x05C0_0000 .. 0x05C3_BFFF`**, and `fpga/rtl/common/zhao_pkg.sv:242`
carries the same half-open end `0x05C3_C000` in RTL. That sits **inside** the
proposed NAV interval. The directive's own section 5b table still calls
`0x058B_0000..0x05FF_FFFF` *"reserved / unmapped"*, which is why the range looked
free; POST.ECHO was added later in the document, under core entries I15/I16 and
ruling R7, and is the only live allocation in that tail. I swept every
`0x05[8-F]x_xxxx` literal in `spec/`, `design/` and `fpga/rtl/` to confirm it is
the only one.

**Chosen option.**

| region | interval | size |
|---|---|---|
| `TERRAIN.COMPOSED_MATERIAL` | `[0x058B_0000, 0x05AB_0000)` | 2 MiB = 256 x 8 KiB **(unchanged)** |
| `TERRAIN.COMPOSED_NAV` | `[0x05C4_0000, 0x05E4_0000)` | 2 MiB = 256 x 8 KiB **(moved above POST.ECHO)** |

`0x05C4_0000` clears POST.ECHO's half-open end `0x05C3_C000` by 16 KiB and is
256 KiB-aligned. `0x05E4_0000` leaves `0x05E4_0000..0x0600_0000` (1.75 MiB) and
`0x05AB_0000..0x05C0_0000` (1.31 MiB) still reserved.

**Reason, and the alternative I rejected.** The obvious alternative is to keep the
two caches CONTIGUOUS by moving POST.ECHO. I refused it. Contiguity is
impossible below POST.ECHO -- 4 MiB from `0x058B_0000` ends at `0x05CB_0000` and
straddles it -- so keeping them adjacent means relocating a region that carries a
**formal no-escape proof re-run on 2026-09-19**: `mem_guard_no_escape.sby` bmc
PASS at depth 30, cover PASS on every arm, with `a1_echo_not_fb`, `a1_echo_wo`,
`a1_echo_owner` and `a1_echo_lease` all bound to its constant bounds. **Adjacency
buys nothing here** -- both caches are addressed independently by slot index, and
nothing walks from one into the other -- so spending a proof re-derivation to
gain it would be paying a real cost for an aesthetic one.

**Constraints and cost.** No client id is spent; no FB window moves; POST.ECHO is
untouched, so its proof stands unmodified. The two new regions need their own
**scoped** guard permissions -- the directive forbids blanket bank-2 permission,
asset-pool widening, and any request crossing a permitted-range boundary merely
because both endpoints lie in the union -- so the no-escape proof gains covers and
deliberately failing mutants for the new writer and reader rather than a widened
existing rule.

**Consequences.** `spec/memory_rules.md` section 5b gains both rows and section
5-guard gains two permission rows; `zhao_pkg.sv` gains the bounds beside
POST.ECHO's; the guard's formal lane gains the new covers and mutants. **The
implementing packet writes all of that**, per the directive's section 10 item 3 --
this record exists so the verification is not lost if that packet is re-scoped,
and so nobody re-derives the collision.

### DECISION RECORD 2: "HEIGHT USES THE EXISTING COMPOSED-HEIGHT PATH" -- THERE ISN'T ONE YET

**Question.** Section 1 of the directive says *"Height uses the existing
composed-height path. Velocity uses the existing TERRAIN.COMPOSED_VELOCITY region
and a REAL writer plus its intended reader(s)."* Both phrases assume a publication
path exists. Does it?

**Measured, and NO -- for height as well as velocity.**

* **No writer.** `COMPOSED_HEIGHT_BASE`, `ZHAO_TERRAIN_COMPOSED*` and `0x0566`
  return **zero hits across all of `fpga/rtl`**. `COMPOSED_VELOCITY` appears in
  `fpga/rtl` only inside **comments** -- four of them, in the package, the guard,
  the Earth adapter and the composer -- and in no transaction.
* **No guard window either**, and this is the part that changes the scope.
  `zhao_mem_guard.sv:244-251` states the rule and the current state together:
  *"ONE region of the six T2 names in bank 2. RESIDENT_MIP_POOL, COMPOSED_HEIGHT,
  COMPOSED_VELOCITY, WRITEBACK_STAGING and COMPOSED_MIP_POOL **stay unmapped
  until the blocks that touch them exist**."* The guard's live bank-2 windows are
  the page pool (`terrain_ok` / `terrain_rd_ok`) and `TERRAIN.DEVSTORE`
  (`devstore_wr_ok` / `devstore_rd_ok`), and nothing else.
* **The precedent is explicit and recent.** `spec/memory_rules.md:362` on
  DEVSTORE: *"ENACTED, 2026-09-22 ... a window opened WITH its block, never ahead
  of it."*

So the composed caches are **computed and not published**. Entry I34's own note
that section 3.4's `live_top = max(compose_top + SUM field lanes, ...)` now has a
non-empty sum describes **arithmetic reaching a value**, not a lattice reaching
SDRAM.

**Chosen option.** The section-1 packet commissions the composed-cache
**publication path for all four channels, height included**, rather than treating
height as done. Each region's guard window is opened **with its writer** -- four
windows arriving with four writers, on the DEVSTORE precedent -- and **not** as a
block of four permissions ahead of the blocks.

**Reason, and the alternative I rejected.** The tempting alternative is to take
the directive's words at face value, build material and nav only, and report I34
closed with height "already handled". That would be the campaign's signature
failure committed deliberately: the directive itself warns *"Some repository prose
already contradicts newer source; live rechecking remains mandatory"* and *"A DMA
into unused memory is not a consumer."* A height lane that publishes nowhere is
the same defect as a nav lane that publishes nowhere, and closing I34 over it
would put the register down by one while no frame could read a composed height.

**Constraints and cost.** This makes section 1 a subsystem, not a wiring job:
four writers, four scoped guard windows, the no-escape proof extended with covers
and deliberately failing mutants per region, slot lifetime/generation rules, and
a reader for each channel. **Note the geometries differ and must not be assumed
uniform:** `COMPOSED_HEIGHT` and `COMPOSED_VELOCITY` are `256 x 2,304 B` in the
ratified section 5b table, while the directive specifies `256 x 8 KiB` slots for
MATERIAL and NAV (33x33 u32 payload plus presence bitmap and versioned identity
header). Mixed slot sizes in one bank are fine; silently treating them as one
stride is not.

**Consequences.** The directive's phrase *"the existing composed-height path"* is
recorded as **superseded by measurement** rather than reinterpreted -- which is
the form the delegation requires. Nothing about the numerical policy in section
13.3 changes; what changes is that height's destination is commissioned here
instead of assumed. Sequencing and the exact writer/reader shapes are the
packet's to decide.

---

## DECISION RECORD 3 -- SECTION 5 DOES NOT FENCE TERRAIN'S COLOUR, AND THE DIRECTIVE DOES

**Raised by TERRTRI, 2026-09-26**, which flagged that
`reports/OWNER-DECISIONS-20260920.md` section 5 and
`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` appear to disagree about who
owns terrain's lit-colour decision. **It obeyed my brief's fence, said so, and
reported the conflict instead of resolving it.** That was the right call and
this record answers it, because the next terrain packet cannot be briefed
without an answer.

### What section 5 actually is

**It is not a reservation.** Its own header reads *"RECLASSIFIED: PARKED, NOT
LIVE"* and the reclassification says in terms: **"it asks the owner for
nothing."** Its recommendation is *"Do not rule these yet ... not because they
are ripe"*, and it gives one stated reason for law 2: that it *"is entangled
with the texture lane's I49 and should not be ruled in isolation."*

**So section 5 parks two laws for a stated reason, and does not reserve them.**
My TERRTRI brief called it a fence a packet "may not un-park". **That was
over-cautious and I am correcting it here**, not because the outcome changed --
TERRTRI reports the fence cost nothing -- but because a wrong fence in a brief
becomes a wrong premise in the next entry, which is how this campaign has lost
most of its time.

### Both of section 5's premises have expired

* **Law 1 (a terrain texture-coordinate law) has DISSOLVED.**
  `zhao_terrain_uvlane` is built and composed, implementing `terrain_rules` 6.2,
  which was FROZEN and capture-exact all along. There is nothing left to rule.
* **Law 2's stated parking REASON has expired.** It was entanglement with I49 --
  and **I49 was closed and DELETED on 2026-09-20**, the same day section 5 was
  written. A parking whose only stated reason no longer exists is not a parking;
  it is an unmaintained note.

### What governs instead, and it is stricter on the thing that matters

`OWNER_VACATION_DIRECTIVE_2026-09-23` postdates section 5 by three days and
grants standing authority over technical decisions **including amending specs
and rulings, provided what is superseded is STATED**. That authority reaches
section 5, and this record is the statement.

**But the directive fences the OUTCOME directly, and harder than section 5 ever
did.** Its prohibitions include, verbatim: *"NOT authority to delete a feature
... remove Gouraud/detail normals ... or call reduced work equivalent merely to
reach zero or fit a device."*

**Terrain's per-vertex lit colour IS the Gouraud question for terrain.**
`spec/terrain_rules.md` 6.5 makes layer-H tint per-vertex by ratified spec, and
broadcasting the scalar `terr_light_base_o` into slots 3..5 removes Gouraud for
terrain. **That move is prohibited by the directive itself**, independently of
section 5, and TERRTRI refused it on exactly those grounds.

### The ruling

1. **Section 5 no longer parks anything.** Law 1 is dissolved by composition;
   law 2's stated reason is spent. A packet does not need owner input to work on
   terrain's colour, and a brief must stop saying it does.
2. **The ENGINEERING decision is delegated** -- which per-vertex producer, what
   carriage it rides, where it is stored. A packet may take it, with a decision
   record naming what it supersedes.
3. **The CAPABILITY is not negotiable.** Per-vertex terrain colour stands. A
   flat stand-in is prohibited by the directive, and if one is ever the right
   call on the ALM budget it is an ESCALATION, not a packet's choice -- and even
   then it must be NAMED a stand-in in the RTL with `terrain_rules` 6.5 cited
   beside it.
4. **The live blocker is not colour at all.** TERRTRI and TERRAINAUX both land on
   carriage: terrain material identity has zero hits in `fpga/rtl/terrain/`, and
   the mosaic's `{tile_a, tile_b, weight}` is built per SPAN from
   `base_rgb[23:16]`/`[15:8]` plus `recipe_weight`, so terrain's per-CELL
   layer-E triple has no carriage. **Section 5 was never what stood in the way.**

**Superseded by this record:** `reports/OWNER-DECISIONS-20260920.md` section 5's
status as a live parking, and the sentence in
`runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/BRIEF-TERRTRI.md` that reads
*"You may not un-park them."* Section 5's CONTENT is not struck -- its history of
why the laws were absent remains accurate and useful.
