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
