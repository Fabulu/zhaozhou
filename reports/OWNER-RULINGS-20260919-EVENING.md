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
