# LANE 2 — THE 8 KM TERRAIN WORLD: what it needs, what exists, what is missing

Recon digest. **Comprehension and extraction only — no RTL was read for
modification, no fit was run, nothing was committed.** Written 2026-09-03.

Assigned documents read completely: `reports/Missingterrain`,
`reports/TERRAIN_WORLD_LAYER_ARCHITECTURE.md`,
`reports/BINNER_CAPACITY_FOR_8KM_MAPS.md`, `reports/MATERIAL_ARCHITECTURE.md`,
`spec/terrain_rules.md`, all ten `design/contracts/TERRAIN.*.md`,
`design/contracts/SW.STREAM.md`, `Upheaval/docs/MANA-TERRITORY.md`.

Two documents outside the assignment turned out to **supersede most of it** and
are treated as law here: `reports/OWNER-RULINGS-BUILDABILITY-20260902.md`
(rulings T1–T12, "THE 8 KM TERRAIN WORLD — BINDING RULINGS") and
`reports/NORMALMAP-ARCHITECTURE.md` (2026-09-03 18:13, the newest terrain
document in the tree).

---

## 0. CHRONOLOGY — established from git, not mtimes

`reports/bandwidth` **is not a document.** It is an empty directory holding a
`.gitkeep` from the 2026-08-14 skeleton commit. The docket's citation of it as
an 8 km-world source is a **phantom citation** and should be struck (cf.
`reports/PHANTOM-CITATIONS-AUDIT.md`).

| date (git author) | document | standing |
|---|---|---|
| 2026-08-16 / 08-17 | `spec/terrain_rules.md` | **still law for the FORMAT.** Nothing below contradicts its patch/layer/breach/keel laws |
| 2026-08-18 → 08-24 | `TERRAIN.NORMALS/PROJECT/LOD` contracts | current for those leaves |
| **2026-08-31 11:26** | `reports/Missingterrain` — *"Agent please read: Specs for giant missing piece of terrain"* | **the owner brief.** Diagnosis still correct; its inventory of what is missing is now partly stale |
| 2026-08-31 18:32 | `Upheaval/docs/MANA-TERRITORY.md` | current; explicitly sequenced *behind* the 8 km world |
| 2026-09-02 11:56 | `BINNER_CAPACITY_FOR_8KM_MAPS.md` + addendum | measurements stand; **its conclusion is overruled** — see §6 |
| 2026-09-02 20:31 | `TERRAIN_WORLD_LAYER_ARCHITECTURE.md` | best block-by-block decomposition in the tree, **but all ten of its OPEN questions were ruled 2.5 hours later** |
| **2026-09-02 21:44** | `OWNER-RULINGS-BUILDABILITY-20260902.md` T1–T12 | **BINDING. The definitive 8 km world law.** |
| 2026-09-02 23:00 | `SW.STREAM.md` **filled** (T12) | supersedes "the streaming system is literally still a stub" |
| 2026-09-02 23:09 | `MATERIAL_ARCHITECTURE.md` (R9 freeze) | current for material recipes |
| 2026-09-03 12:24 | `TERRAIN.RESIDENCY.md` + `zhao_terrain_residency_v2.sv` | **residency v2 is BUILT and UNIT_VERIFIED.** Supersedes both the brief and the architecture doc |
| **2026-09-03 18:13** | `NORMALMAP-ARCHITECTURE.md`, `TERRAIN.NORMALMAP.md` | **newest.** Confirms terrain has no colour path at all; names `TERRAIN.SHADE` |

**Net effect: the architecture document is a good map with a stale legend.**
Read T1–T12 first, then that document for the block decomposition.

---

## 1. BUILD ORDER

Dependency-ordered. Everything here sits in **Phase 6** of the ruling set's fit
order — *behind* Phase 2–4 (texture repairs, texture-survivor composition,
physical gates) and *behind* Phase 5 (GEOM.PARAMBUF). The standing reject —
*"do not add further leaf blocks until the texture island is composed and
fit"* — is in the ruling document itself (IMMEDIATE REPOSITORY CHANGES). **This
lane's deliverable is the plan, not the build.**

Legend: **[0] done** · **[C] contract exists, no RTL** · **[R] RTL exists, no
contract/ledger** · **[—] nothing at all**

| # | block | state | purpose | in | out | specified by |
|---|---|---|---|---|---|---|
| **0a** | `TERRAIN.RESIDENCY` v2 | **[0]** | 256 sets × 4 ways = 1,024 slots; answers *is this ground resident and is my handle still valid* | `lu/cl/fin/dm/pin/unpin/wb/chk` | handle `{epoch, slot:u10, gen:u8}`, victim, counters | T1/T9/T10; `TERRAIN.RESIDENCY.md` |
| **0b** | `TERRAIN.MIPGEN` | **[0]** | nested-decimation height mips, no averaging (crack safety) | 33×33 lattice, scan order, both surfaces | mip17 (289) + mip9 (81) per surface, 1,480 B | T8; `TERRAIN.MIPGEN.md` |
| **1** | **commit T1–T12 into the senior specs** | **[—]** | put the key, guard map, command ABI and mip law into `spec/terrain_rules.md` + `spec/memory_rules.md` §5b + `spec/commands.zidl` | rulings | law | ruling set, IMMEDIATE REPOSITORY CHANGES |
| **2** | **MEM.GUARD bank-2 region map + `ZHAO_CLIENT_TERRAIN_BUILD = 6`** | **[—]** | the addresses every block below writes to; deny-by-default, **state-aware** (a loader may write only a `LOADING` slot) | region table | grant/deny | **T2** (exact addresses), **T3** |
| **3** | `SW.STREAM` implementation (HPS) | **[C]** | parse `ISLAND_TABLE`; hold canonical B/D; hold the F journal; build + **seal** the deterministic visible/predicted/prefetch list | two cameras, island tables, counters | `TerrainEpoch`, `SubmitTerrainSet`, sealed list bytes | **T12**, T5, T7; `SW.STREAM.md` |
| **4** | `TERRAIN.PAGELOADER` | **[—]** | HPS DDR → local SDRAM page slot, whole 21,376 B, CRC-32C verified, reports the CRC it saw to the directory | job {slot, gen, hps_addr, vram_addr, key} | 334 × 64-B bursts, `fin` + CRC identity | arch doc §2.3; T7, T8 ordering |
| **5** | `TERRAIN.COMPCACHE` + allocator | **[—]** | the 256-slot composed height/velocity cache **and its on-chip patch front** — the missing middle between PATCH and TESS | `patch_state` stream + velocity lane | TESS's registered `lat_*`/`cs_*` port at 1 datum/clock | `terrain_rules` §4.2; **T6**; arch doc §2.5 |
| **6** | **`TERRAIN.SHADE`** | **[—]** | **the terrain's light.** Face normal + sun → flat lit colour per triangle. Without it terrain renders with no colour at all | face normal (NORMALS), sun | `lit_c = sat_u8(ambient_c + rescale_u(sun_c*ndl, 8))`, joined to PROJECT by `src_id` | `NORMALMAP-ARCHITECTURE.md` §"Piece 1" |
| **7** | **terrain colour port on `TERRAIN.PROJECT`** | **[—]** | the wiring gap: `zhao_terrain_project`'s output packet has **no rgb field**, so no terrain colour can reach the rasteriser | SHADE's FIFO | colour on the existing per-vertex attribute planes (zero gradient) | ibid. |
| **8** | `TERRAIN.WRITEBACK` → **F-sheet journal only** | **[—]** | on dirty eviction copy **exactly layer F** to the HPS journal and **wait for the ACK** before the slot may enter `LOADING` | `dirty_F` victim | 8 KiB journal write + ACK | **T4** — *B and D are never written back* |
| **9** | `TERRAIN.SEQ` | **[—]** | the pump: walk the sealed list and drive lookup → (miss: journal → load → mipgen) → compose → LOD → 16/32 TESS jobs → NORMALS → SHADE → PROJECT → GEOM.PARAMBUF | sealed list + grant from `CMD.SCHEDULER` | every terrain seam, **in list order** | arch doc §2.6; T5 (ABI), T6 (pressure order) |
| **10** | composed shell path in one Verilator top | **[—]** | first frame where the island is chosen by a world manager, not registered by hand. Harness supplies only the `FRAME_RING` | — | — | arch doc §5 step 8 |
| **11** | **the 8 km traversal capture** | **[—]** | the acceptance gate: fly ≥8 km, force churn, deform, leave, return; plus teleport, two-island collision, Duo | — | per-frame counter trajectories, run twice, assert identical | `Missingterrain`:177-179; T10; `SW.STREAM.md` |

**Not this lane's to build, but it lands on their doorstep:** `GEOM.PARAMBUF`
(Phase 5, R7, `REFERENCE_COMPLETE`) and `FORGE.CLIFF` (rim walls, `terrain_rules`
§5). `TERRAIN.NORMALMAP` (contract written, **no ledger entry**) is explicitly
the *cuttable* piece — step 6 is not.

**Registration gap worth naming on its own:** `TERRAIN.SHADE`,
`TERRAIN.PAGELOADER`, `TERRAIN.COMPCACHE`, `TERRAIN.SEQ`, `WORLD.DIRECTORY` and
`TERRAIN.WRITEBACK` have **no `design/blocks.yml` entry and no contract**.
`TERRAIN.NORMALMAP` has a contract and no ledger entry. Three blocks were found
this week already built with neither (`GEOM.PARAMBUF`, `TERRAIN.MIPGEN`,
`TERRAIN.RESIDENCY`), so the ledger is a lagging indicator in this subsystem
and should not be used as the inventory.

---

## 2. THE WORLD MODEL — stated with units

### The three sizes, kept straight

| thing | size | what it is |
|---|---|---|
| raster tile | **16×16 pixels** | on-chip framebuffer work area. **Nothing to do with world extent** |
| terrain patch | **32×32 cells on a 33×33 vertex lattice** = **64×64 m** at canonical pitch | the reusable unit the terrain engine processes, over and over |
| subpatch | **8×8 cells**, 16 per patch in a 4×4 grid (x fastest) | the LOD decision unit |
| island | a **sparse directory** of patches | the actual level |
| world | fx16, **≈ ±32 km** | islands live at any altitude inside it |

### Coordinates and numeric law

* Heights: `height16` = **S 1.7.8 metres — ±128 m at ~3.9 mm step** (`qformats`
  §9). Live math in fx16. Heights are **relative to the island datum**, so an
  island sits anywhere in the ±32 km fx16 world while local relief stays ±128 m.
* Cell pitch is **per-island**, restricted to **{0.5, 1.0, 2.0, 4.0} m** as
  `pitch_log2 ∈ {−1, 0, +1, +2}`. Powers of two make world→cell a shift: **no
  division anywhere in the addressing path**. **Canonical battlefield pitch =
  2.0 m** ⇒ patch = 64 m.
* Scan convention throughout: **z-then-x**, row-major within a surface,
  surfaces in order (top, then bottom).

### The canonical key (T1 — supersedes the built prototype)

    { resource_epoch:u32, island_id:u32, patch_ix:i16, patch_iz:i16 }

Pitch is a property of the island table, validated against the page header, and
is **not part of the key**. **No global coordinate projection is identity.**
**Two islands may legally overlap in local patch coordinates** — and there is no
software "islands must not collide" rule to enforce (T9). The `{px, py}`
direct-map prototype would answer island A's lookup with island B's ground.

### The page — 21,376 B stride (21,320 B body + 56 B pad, 334 × 64-B bursts)

| layer | extent | element | bytes | writer |
|---|---|---|---:|---|
| header | — | — | 64 | packer |
| A top base height | 33×33 | height16 | 2,178 | authored, **immutable at runtime** |
| B top scar delta | 33×33 | height16 | 2,178 | `TERRAIN.BAKE` only; **HPS canonical** |
| C bottom height | 33×33 | height16 | 2,178 | authored (the keel) |
| D cell state | 32×32 | u8 | 1,024 | `TERRAIN.BAKE` only (breach/heal); **HPS canonical** |
| E base material | 32×32 | {matA u8, matB u8, weight u8} | 3,072 | read-only to fabric |
| F surface sheet | 64×64 | {tag u8, strength u8} | 8,192 | `SURFACE.STAMP` only; **no HPS mirror → journal** |
| G gameplay grid | 8×8 | {heat, wet, corrupt, hazard} | 256 | sim |
| H vertex tint | 33×33 | RGB565 | 2,178 | read-only. **Nothing consumes it — see §7** |
| | | **total** | **21,320** | |

Derived, not streamed: height mips 17×17 + 9×9 per surface = **1,480 B** in a
1,536 B record, generated **on the FPGA** by `TERRAIN.MIPGEN` (T8).

### Column law and composition

Every cell is exactly one of `SOLID` (one interval `[bottom, top]`),
`VOID_AUTHORED`, `VOID_BREACHED`, or `OUT` (absent patch = open sky). Lattice
values exist at **all** 33×33 vertices regardless of neighbouring substance —
void corners are what rim walls hang from.

    compose_top[v] = max( fx(base[v]) + fx(scar[v]), fx(bottom[v]) )
    live_top[v]    = max( compose_top[v] + Σ field height lanes (command order), fx(bottom[v]) )

**Live fields touch the top surface only.** The bottom changes exclusively at
bake time. Fixed diagonal i00–i11; triangle pick `u >= v ? A : B`, ties to A;
the diagonal is single-valued by construction. Reference symbol:
`zref::terrain::column_query`.

**The anti-drift law (§4.1):** field programs are evaluated **only at lattice
vertices**, by the one zfield interpreter, and *every* consumer — render, sim
height query, particle collision, velocity, normals, nav — reads the **same
composed lattice** on the **same triangulation**. There is no shader copy. This
exists because Sacrifice's Erupt rebound was 2.0 in sim and 3.0 in the shader.

### LOD

`TERRAIN.LOD` decides **16 subpatches per patch** (32 on a dual page) against
**both** cameras, with charter §9 hysteresis / minimum-hold / geomorph. Four
levels; **stride = `1 << level`**, clamped to {1, 2, 4, 8} because a subpatch
edge must land on a lattice vertex. Its output packet **IS** `TERRAIN.TESS`'s
job port field-for-field. Crack safety comes from decimation, not averaging: a
coarse vertex *is* a fine vertex, bit for bit.

**Triangles per patch by level (my arithmetic from the 8×8 subpatch and the
stride rule — DERIVED, not measured; the tessellator's own bound is ≤128
triangles per subpatch per surface):**

| level | stride | tris/subpatch | tris/patch (top) | ×2 if dual |
|---:|---:|---:|---:|---:|
| 0 | 1 | 128 | **2,048** | 4,096 |
| 1 | 2 | 32 | 512 | 1,024 |
| 2 | 4 | 8 | 128 | 256 |
| 3 | 8 | 2 | **32** | 64 |

This 64× spread between a near patch and a far one is the single most important
number missing from the binner analysis (§6).

### Residency and the honest limit on "8 km"

* A dense **8×8 km plate at 2 m pitch** = 125² = **15,625 pages × 21,376 B =
  318.5 MiB** — 2.5× the entire 128 MB local SDRAM. **It can never be resident,
  and the design does not try.**
* The design is **1,024 local page slots** = 21,889,024 B = **20.9 MiB** =
  **4.19 km² of registered ground at 2 m pitch, 16.8 km² at 4 m**.
* Residency is **sparse**: an absent directory entry is open sky — no page, no
  sheet, no draw. Sacrifice paid storage and draw for its void square; we do not.
* Therefore: an 8 km world of **scattered floating islands** — the world this
  console is for — fits. A long **narrow** 8 km island fits. A solid 8×8 km
  plate **streams permanently**, and at speed shows counted misses at the
  leading edge.
* Donor comparison at 2 m pitch: **5× finer** deformation lattice than
  Sacrifice's 10 m, **25.6× finer** vertical step, **16.1×** the resident cells.

### Working set and prefetch (T7)

Current visible set **for both views**; a **one-patch Moore ring**; the
predicted visible set **30 frames (0.5 s) ahead** from camera velocity; explicit
gameplay-required patches. **Union the views before deduplication.** Order:
required current → predicted visible → neighbour ring → canonical ties.

### Determinism

The sealed list **is capture data**: `list_crc32c` over byte-identical list
bytes produced by the canonical order (required before prefetch; smaller
priority first; view-union key; `island_id` asc; `patch_iz` asc; `patch_ix` asc;
`source_id` asc). **Replay does not rerun the HPS visibility walk** (T5) — a
replay producing different list bytes has already failed before any pixel is
compared.

---

## 3. WHAT IS MISSING — block by block

### Category A — nothing exists (no RTL, no contract, no ledger entry)

1. **`TERRAIN.SEQ`** — the pump. Its absence is *why the island is "the unit
   under test made visible"*. Walks the sealed list and drives every seam in
   list order. **A patch that is not resident when its turn comes is skipped and
   counted, never stalled on** (arch doc §2.6) — one frame of missing ground,
   loudly counted, beats a deadline fault.
2. **`TERRAIN.PAGELOADER`** — the only thing that can move a page into VRAM.
   Every law it needs is written (page format, CRC-32C over bytes [64, 21320),
   whole-page streaming, the bridge's frozen 64-B port, guard discipline).
3. **`TERRAIN.COMPCACHE`** + allocator — the missing middle. **Both**
   `TERRAIN.PATCH` and `TERRAIN.TESS` name it as their missing upstream. Cannot
   be on-chip: 256 × 2,178 B × 2 (height + velocity) = **1,089 KiB = 8.92 Mbit =
   161 % of the device's entire 5.53 Mbit of M10K.** It lives in bank 2; the
   *patch front* (the double-buffered on-chip staging TESS's registered port
   requires) is **≈16 M10K ≈ 2.9 %** and is the same storage the Field-v3
   amendment already identifies as per-patch scratch — not a second copy.
4. **`TERRAIN.SHADE`** — **the terrain's light.** Not a normal-map luxury; the
   newest document is explicit that this is *"not cuttable, because it is the
   terrain's light"* at ~700–900 ALM and 8–10 DSP. Oracle
   `zref::terrain::shade_base` is **PLANNED AND NOT WRITTEN**.
5. **`TERRAIN.WRITEBACK` (F-journal only)** — T4 shrank this block. B and D are
   never written back; **layer F is, and its ACK is a hard barrier.**
6. **`WORLD.DIRECTORY`** — folded into `SW.STREAM` by T12; no separate block.

### Category B — contract exists, no RTL

7. **`SW.STREAM`** (`SPECIFIED`). **The brief's "literally still a stub" is
   stale** — T12 filled it on 2026-09-02 23:00. But note what the contract
   itself refuses to guess, and which each block below it needs: **the
   `ISLAND_TABLE` binary layout, the sparse page-map encoding, the journal's
   on-disk format, and the staging area's size.** Four unruled items, listed
   rather than invented. **This is now the top of the critical path.**
8. **`TERRAIN.NORMALMAP`** — contract written, no ledger entry, and the RTL on
   disk is flagged: `T_DIV` runs 32 steps over 64 bits (~66+ cycles/triangle
   serial, against PROJECT's 1 per 3 clocks), and its ENFORCED-BY test file is
   named but the claim is that the file does not do what it claims. **Cuttable
   by design** (delta = 0 is a bit-exact no-op).

### Category C — RTL exists and is a real organ (not a harness toy)

`TERRAIN.PATCH`, `TESS`, `LOD`, `NORMALS`, `PROJECT`, `VELOCITY`, `BAKE`,
`BAKE_DELTA` — all `UNIT_VERIFIED`, with pairwise compositions green (TESS→
NORMALS at 41,731 checks; PATCH at 14,730). `LOD` was designed for many patch
jobs from the start. **`RESIDENCY` v2 and `MIPGEN` are also now real** — both
`UNIT_VERIFIED`, both written to rulings, both landed after the owner brief.

### Category D — RTL exists and must NOT be integrated

`zhao_terrain_residency.sv` (the direct-map prototype). **X8: "must not be
integrated as the world directory."** Kept as the thing v2 is measured against.
**The exclusion is already enacted:** `prod_manifest.yml:167` — *"zhao_terrain_residency:
superseded by zhao_terrain_residency_v2 (256x4 ways)"* — with v2 listed at `:91`.
v1 also stores its directory in **flop arrays with a 1,024-entry async reset**,
which is the fit hazard v2's 256-clock synchronous init sweep exists to remove.
Two independent reasons: wrong key (no `island_id`) and wrong mapping (a 2,048 m
collision period — *"which sounds rare and is exactly what an 8 km traversal
does"*).

### The RTL reality — verified against the built tree

**Not one of the twelve modules in `fpga/rtl/terrain/` has a VRAM, SDRAM, AXI or
Avalon port.** Zero matches for `avalon|axi_|sdram|readdata|writedata|waitrequest|burstcount`
across the directory. Every occurrence of "VRAM" in that directory is a comment
saying the port is *deliberately absent*. The docket's claim is not merely
architectural — it is literal: **no terrain block in this console can touch
memory.** Each one is a stream processor whose lattice reads are answered by a
testbench:

* `zhao_terrain_tess` emits `lat_req_o/lat_vi_o/lat_vj_o` and the harness returns
  `lat_h_i/lat_wx_i/lat_wz_i` — that request port is exactly what
  `TERRAIN.COMPCACHE` (build step 5) must answer.
* `zhao_terrain_bake` emits `vtx_vi_o/vtx_vj_o` and the harness returns
  `vtx_base_i/vtx_scar_i/vtx_bottom_i/vtx_nobake_i`. Its own header: *"NOT IN
  THIS BLOCK, deliberately: no VRAM port and no residency directory."*
* `zhao_terrain_patch:98` — *"NOT IN THIS BLOCK, deliberately: no VRAM port, no
  page loader, no page CRC"*, and `:72` notes its upstream `FIELD.SEQ.EARTH`
  *"whose contract is still a stub."*
* `zhao_terrain_velocity:127` — the 2 B/vertex §4.2 store *"belongs to whoever
  owns the VRAM page."*
* **Both residency blocks are purely bookkeeping tables.** Neither moves a byte;
  they answer resident/slot/generation and hand back an eviction descriptor so
  *somebody else* does the work. v2's RAMs are internal inferred M10K metadata
  banks, not an external bus.

**There are only three instantiating shells in the whole tree, and none is a
composition:**

| shell | what it is |
|---|---|
| `fpga/rtl/prod/zhao_prod_top.sv` | **generated area-counting harness.** Its own header: *"This is a RESOURCE top, not the console. Blocks are not wired to each other, so no timing number here means anything. Each instance is fed by its own seeded LFSR."* Nine terrain blocks instantiated, every input from an LFSR, every output XOR-folded into `fold_o` |
| `fpga/rtl/synth/zhao_pair_tess_normals.sv` | *"CHARACTERIZATION WRAPPER, not a console block… NOT INSTANTIATED BY THE CONSOLE"* |
| `fpga/rtl/common/zhao_shell_top.sv` | **the actual Phase-2 console shell — contains ZERO terrain.** `grep terrain` returns nothing. `fpga/quartus/shell_fit/zhao_shell_fit.qsf:199`: *"# blocks -- no terrain, texture, geometry or raster."* |

So the 53.48 MHz fit contained neither an 8 km island nor the terrain pipeline —
the brief was right, and it is verifiable from the QSF comment.

`zhao_terrain_bake_delta` is the **only** terrain module instantiated by real
sibling RTL (`zhao_terrain_bake.sv:378`). Everything else is fed by a harness.

**One block is on disk and known broken.** `zhao_terrain_normalmap.sv:49-56`:
*"STATUS: DRAFT, NOT IN THE PRODUCTION MANIFEST, AND KNOWN WRONG… `base_o` comes
out ZERO for every realistic triangle and the whole effect would silently do
nothing"*, and `:58` declines to cite a test *"on purpose: none is written, and
citing one that is not would be a fresh phantom citation on the same day as the
audit."* `prod_manifest.yml:209` excludes it: *"unused draft pending TERRAIN.SHADE
/ TERRAIN.NORMALMAP split"* — so **`TERRAIN.SHADE` is already named in the
production manifest as the thing that must exist first.**

### Category E — plumbing, not blocks

The bank-2 guard map (T2 gives exact addresses),
`ZHAO_CLIENT_TERRAIN_BUILD = 6` (T3), the two new commands (T5), and the F
journal's write grant in `MEM.HPS.BRIDGE` (whose granted writes are currently
descriptor words + trace extents only).

---

## 4. CAPACITY AND BANDWIDTH — every number with its basis

The repo's standing rule is that capacities are sized from real traces, not
arithmetic. Marked honestly. **DERIVED** = correct arithmetic over a chosen
premise; the premise is where these fail, not the sum.

| number | value | basis | note |
|---|---|---|---|
| patch = 32×32 cells / 33×33 lattice | — | **RATIFIED** (charter §11.1) | |
| cell pitch set | {0.5, 1, 2, 4} m | **RATIFIED** (`terrain_rules` §1.3) | frozen in that wave |
| page stride | 21,376 B | **RATIFIED** (§2) | 334 × 64-B bursts |
| resident slots | 1,024 | **RATIFIED** (charter §12) | |
| dense 8×8 km plate | 318.5 MiB | **DERIVED**, sound | 15,625 × 21,376 B; independently derived twice |
| resident ground | 4.19 km² @ 2 m / 16.8 km² @ 4 m | **DERIVED**, sound | 1,024 × (64 m)² |
| terrain memory budget | 14.38 MiB sheets + 8.95 MiB hot cache | **DERIVED** (§8) | sums check; pools have real headroom |
| bank-2 region addresses | T2 table | **RULED** | *"reserved/unmapped until traces justify"* — honest |
| composed cache | 256 patches | **RULED (T6)**, and **reinterpreted** | **T6: the 256 are patches needing LIVE composed height this frame — NOT a cap on visible terrain.** Static/baked visible pages render from resident page layers and consume **no** slot. The architecture doc read it as the visible cap |
| composed cache on-chip cost | 8.92 Mbit = **161 % of device M10K** | **DERIVED**, decisive | forces it off-chip; not arguable |
| patch front (on-chip) | ≈16 M10K ≈ 2.9 % | **DERIVED** | shares Field-v3's per-patch scratch |
| streaming ceiling | **32 pages/frame ≈ 41 MB/s** | **GUESSED — provisional, explicitly not a board claim** | `terrain_rules` §7 forbids freezing it before ZH-004; T7 repeats that. Board counters may reduce it immediately |
| loader bridge cost | 334 × 24 = 8,016 cycles/page ≈ 80 µs; 32 pages = **15.4 %** of frame | **DERIVED from the SIM bridge profile** | sim profile, not board |
| `MAX_PATCH_FIELDS` | **16** | **DERIVED from the donor worst case** (8 wizards × Erupt + 8 Quakes) | an *intake correctness bound*, **not** an affordability certificate. The doc says so |
| worst legal patch field cost | 557,568 instructions ≈ **33 % of one frame for ONE patch** | **DERIVED** | frame-level budget explicitly **NOT COSTED** — needs `FIELD.SEQ.EARTH` pinned (still a stub) |
| `BAKE_PATCH_BUDGET` | **64** patch-bakes/frame | **DERIVED** (49 worst Volcano + 15 slack) | |
| bake engine cost | 64 × 1,089 = 69,696 cycles ≈ **4.2 %** of frame | **DERIVED** | |
| bake VRAM traffic | 10,760 B/patch-bake → 673 KiB/frame ≈ **41.3 MB/s** | **DERIVED, and self-flagged "AFFORDABILITY: NOT COSTED"** | *"coincidentally the same order as §7's also-provisional streaming worst case"* — two provisional 41 MB/s figures that have never been added together |
| LOD throughput | ~784 clocks/patch → **~2,100 patches/frame**, ~8× the 256 required | **DERIVED** (16 × 48 + 16) | contract: *"no separate rate case exists because the margin makes one uninformative."* Cited as evidence in the brief and the docket; it is arithmetic, not a trace |
| LOD resources | 3 DSP (from 28) | **MEASURED** | a genuine measured fit result |
| TESS rate | 3.56 cycles/triangle @ level 0; **7.31 with morph** | **MEASURED** (`terrain_tess_directed` §10) | morph nearly doubles the cost — real |
| tris/patch by LOD level | 2,048 / 512 / 128 / 32 | **DERIVED** (mine, from 8×8 subpatch + stride) | see §6 |
| rim edges, worst case | 2,112 per patch (checkerboard breach); typical 32–64 | **DERIVED** | `FORGE.CLIFF` clamp provisional **512 quads** |
| binner shipped caps | 128 tris / 1,024 refs (256 chunks × 4) | **SHIPPED PARAMETER** | sized to prove the laws |
| binner load | see §6 | **MEASURED** through the shipped `zref::Binner` | the tool counts exactly what hardware would store |
| kMesh budget | **32 machine-wide; ≥16 per active Duo view; giant separate** | **RULED (R7)** | |
| giant reference reserve | **≥32,768 tile references** | **RULED (R7)** | *"the giant is never silently truncated"* |
| PARAMBUF acceptance tier | 32,768 vertices / 8,192 tris / 65,536 refs per view | **RULED (R7)** | preferred: 65,536 / 16,384 / 131,072 in 4 MiB |
| 3-sample terrain frame | 1,094,600 samples of 1,666,667 clocks | **DERIVED** | 276,480 = *conservative pre-Early-Z covered fragments, Z60, 3.0× overdraw* (R2/R5) |
| tileset cost | 1.33 MiB each; 4 resident = 5.3 MiB = 11 % of the 48 MB pool | **DERIVED** | |

**The one place arithmetic is masquerading as evidence:** the **~2,100 LOD patch
decisions per frame**. It is `16 × 48 + 16` at a 100 MHz *placeholder* clock,
with no rate test, and it is quoted in both `Missingterrain` and the docket as
the proof that the terrain engine "was never a one-patch toy". The *conclusion*
is right for other reasons (the block genuinely takes 16 descriptors per patch
and its output is TESS's job port). The *number* is not a trace.

**The second: two independent provisional 41 MB/s figures.** Streaming (§7) and
bake (§9.2) each arrive at ~41 MB/s by different routes, each explicitly refuses
to be frozen before ZH-004, and **nothing anywhere adds them**. A frame that is
both streaming hard and baking a Volcano wants ~82 MB/s of terrain traffic plus
scanout plus render. That sum does not exist in any document.

---

## 5. THE BINNER PROBLEM — the real required numbers

**Shipped:** `TRI_CAP = 128`, `CHUNKS = 256` × `CHUNK_REFS = 4` = **1,024 tile
references**, `GRID_W/H = 24/24` (Z60 384×240 needs 24 × 15 = 360 tiles).
The overflow wall is **already safe and already instrumented** — the triangle is
abandoned, `overflow_o` latches, `triangles_culled_o` counts. Nothing there is a
defect; the numbers are sized to prove the laws.

**Measured through the shipped `zref::Binner`** (`tools/render/count_bin_load.cpp`)
— which *is* GEOM.BINNER's binning law, so it counts exactly what the hardware
would store. Z60 384×240, 24×15 tiles, no camera/visibility/LOD, so each row is
an upper bound for its own geometry and a **lower** bound for a real frame:

| scene | triangles | ×128 | references | ×1,024 | deepest tile |
|---|---:|---:|---:|---:|---:|
| sky backdrop (2 triangles) | 2 | 0.0× | **396** | 0.4× | 2 |
| one terrain patch, level 0 | 2,048 | **16.0×** | 4,080 | 4.0× | 12 |
| creature army, 200 × 96 | 19,200 | **150×** | 23,912 | **23.4×** | **341** |
| giant near camera, 126 tris | 126 | **1.0×** | **25,704** | **25.1×** | 126 |

**The finding that matters is that the two limits fail independently.** The
giant *fits* in `TRI_CAP` (1.0×) and destroys the arena (25×). The army destroys
`TRI_CAP` (150×) and is comparatively easy on references (23×) — because an army
is many *small* things. **Two triangles can eat 40 % of the arena** (the sky
backdrop). There is no single number to raise: a triangle budget and a reference
budget are different resources with different worst cases.

**Also measured:** `TRI_ENT_W = 142` bits/triangle, and the cheaper-record idea
is dead — edge coefficients are `kx`/`ky` at 23 bits and `kc` at 48 = **282
bits/triangle, twice** the current record. **There is no narrower record to
find.** And `max_tile_list_depth_o` reports 215–341 on the army — an 86-chunk
pointer chase per tile — **and nothing reads it.**

**The 2026-09-02 addendum, also measured:** the 49 %-of-device figure assumed no
LOD, and `zref::LodRung` is `{kMesh, kMicro, kSplat, kGlint}` where kSplat and
kGlint are **a billboard quad — two triangles**. A realistic 256-creature mix is
**9–14× cheaper** (1,792–2,720 triangles vs 24,576), which puts a frame-resident
arena at 16–21 % of device M10K.

### Its conclusion is overruled, and this is the contradiction to carry forward

The addendum concludes *"on these numbers a bigger constant is exactly what will
do."* **R7, written the same day, rules the opposite:**

> The production binner does **not** grow a frame-sized on-chip triangle arena.
> […] *The LOD measurement is useful evidence that ordinary armies are cheaper
> than the all-kMesh assumption, but it does not revoke this architecture.*

Projected vertices, compact triangle descriptors and tile-reference chunks live
in **local SDRAM bank 3** under ENGINE1; on-chip keeps only the tile directory,
active chunk tails, prefetch FIFOs, a small projected-vertex cache and an
opportunistic expanded-context cache. **One tile clears and resolves exactly
once. No framebuffer readback. No arbitrary tail truncation.** The answer is
`GEOM.PARAMBUF`, and the ruling's answer to the kMesh question is blunt: *"Do
not grow the on-chip arena; build GEOM.PARAMBUF."*

### The gap nobody has measured: terrain under LOD

Every binner row for terrain is **level 0** — 2,048 triangles for one patch. By
§2's table a level-3 patch is **32**. Nobody has run the binner over a *frame* of
terrain with the LOD ladder applied and the underside emitted. The two open
numbers:

* **triangles per frame for terrain** — bounded above by 256 live patches ×
  2,048 × 2 surfaces ≈ **1.05 M** if everything were near and dual (absurd), and
  below by 32/patch at distance. The real figure is a *camera* question and it is
  the one number the whole terrain arena depends on.
* **references per frame for terrain** — worse than triangles, because near
  terrain triangles are large and cross several tiles each.

The binner document's own instruction still stands and is still unexecuted:
*"Run those scenes through `zref` first and count triangles and (triangle, tile)
pairs per frame in software. That costs nothing, needs no RTL."* **Do that for
terrain-with-LOD before any terrain arena number is chosen.** It is exactly the
lawful use of measurement — on the comparison side, checking a premise, not
choosing a value.

---

## 6. DEFORMATION — what each effect requires of the hardware

| effect | mechanism | what the hardware must provide |
|---|---|---|
| **Transient waves** (Erupt, Quake) | ordinary **live earth field programs** with hard finite footprints; raised-cosine + polynomial envelopes; one sin LUT | per-patch field list (≤ **`MAX_PATCH_FIELDS = 16`**), composition in **command order**, a **composed-cache slot per touched patch**. A faithful single-program Erupt sketches at 36–38 instructions — **over the 32-op earth ceiling** — so composite effects **phase-split** into per-phase programs (grow ≤10, wave ≤32). **A transient wave can never punch a permanent hole** |
| **Bore scars / collapse / breaches** | **only `TERRAIN.BAKE`** breaches. A `SOLID`, `no_bake = 0` cell becomes `VOID_BREACHED` iff `compose_top[v] == fx(bottom[v])` **exactly, at all four corner vertices**. A later bake raising any corner **heals** it | bake writes layers B and D; **read+write B, read A and C for the compose test, read+write D = 10,760 B per patch-bake**. `BAKE_PATCH_BUDGET = 64`/frame with a **FIFO deferral law** that is state-exact by the incremental-scaling identity (`from→mid` then `mid→to` ≡ `from→to`). Breaches are born at a bake event — the frame the impact FX masks the discrete transition. **`no_bake` protection has a one-cell halo**, because the clamp is vertex-level and corners are shared |
| **Volcano rise** | explicitly **NOT a live field** — an incremental **bake sequence** of stamp records, every frame for the cast duration | the load-bearing distinction: it pressures the **bake budget**, never the field list. Footprint ~330 m ⇒ **6×6 = 36 patches aligned, 7×7 = 49 worst** (closed-interval rule; the "5×5 if aligned" aside is arithmetically impossible — 320 m < 328 m). 49 sustained every frame ≈ 3.2 % of a frame and ≈ 31.6 MB/s |
| **Rotated terrain sheets / detached islands / towing** | an island is a **datum** (`world3` origin + fx16 y) plus a sparse patch grid; heights are datum-relative | moving an island is moving its **table entry**, not its pages — the format already supports it. But **the key contains `island_id`, not world coordinates**, so a towed island keeps its residency identity for free. **Nothing in the tree specifies island transform *animation*** — no rotation field on the island table was found. This is a genuine unwritten piece for `MANA-TERRITORY`'s "detach, tow" verbs |
| **Rim walls / undersides** | `FORGE.CLIFF`: one quad per rim edge, top at the **tessellator's own stitched edge vertices**, bottom at the bottom lattice | crack law: **along a rim boundary the underside LOD must equal the top LOD**. Clamp to a declared budget (provisional 512 quads), degrade by **merging contiguous collinear spans first** (a merge never bridges a notch, so the silhouette keeps every hole), then by keeping greatest max-vertex 1/w. Textured: strata tile **id 240**, underside **id 241** |
| **Keels** (what makes a hole read as a hole) | `KEEL_FLOOR = 50 m` (donor parity is the *floor*); `KEEL_DEPTH = min(max(50, R/2), 126 − max(0, peak))`; bitten-apple profile `thickness = KEEL_DEPTH × (0.4 + 0.6(1 − q))`, `q = (d/R)²` | authored offline into layer C. Rim keeps 40 % of the keel. **Islands with R > ~250 m saturate the ±128 m format** — stated honestly |
| **Bottom-surface live deformation** | **not in v1.** A rippling underside needs a format revision (a new live lane) | noted as possible v2 |
| **True caves / arches / tunnels** | **not expressible** — one solid interval per column. Column-runs (option d) was rejected: the stitch case matrix squares and LOD collapse of a 2-run column *is* a popping cave | these are charter §11.7 **Wounds** — meshlet plugs socketed into void cells. **The void mask IS the Wound socket**; plug format deferred, blocks nothing |
| **Mana territory presentation** | `owner_id` + `strength` per **gameplay cell** (layer G scale, 8×8 per patch, 4×4 cells each) | *"Presentation is a terrain material input… a per-cell scalar feeding terrain texturing/colour; it is **not** a new lighting model and must not become one. It should cost the terrain path a **lookup**, not a pipeline."* Propagation, connectivity flood and the damage envelope are **game simulation on the gameplay grid**, never inferred from anything the rasteriser produced. **Newly exposed or reconstructed terrain begins neutral** — so permanent deformation tears influence off the map. **Explicitly sequenced behind the 8 km world and not a reason to reorder it.** Open (owner's): gameplay-cell size and its relation to the 32×32-cell patch |
| **Nav after deformation** | breach/heal marks the owning gameplay cells dirty; `SW.CPUCOLL` re-reads | *"Sacrifice never updated nav after deformation — we do, and it costs a dirty-bit, not a system."* |

---

## 7. CONTRADICTIONS

**C1. Terrain has no lighting path — the conclusion is CONFIRMED, but one
supporting detail must be corrected before it is repeated.**

**The correction, checked against the tree:** `zhao_terrain_normals` **IS** named
in `design/prod_manifest.yml:85`, and also in `fpga/quartus/prod_fit_sources.txt:101`
and `design/fit_targets.yml:215`. It is instantiated in `zhao_prod_top.sv:3345`,
not only by the pair probe. So *"instantiated only by a leaf-fit probe, and
nothing in prod_manifest consumes it"* is **literally false** and should not be
restated in that form.

**The conclusion survives intact anyway, for a better reason.** `zhao_prod_top`
is a generated LFSR area harness in which *"blocks are not wired to each other"*.
So **no block in the tree consumes `nx_o/ny_o/nz_o` in a wired data path.**
`zhao_terrain_project` declares the matching `ax_i..cz_i` packet and `blocks.yml`
records `TERRAIN.NORMALS downstream: [TERRAIN.PROJECT]` — **and nothing anywhere
connects them.** `TERRAIN.NORMALS` is in exactly the same position as the other
eight terrain blocks: listed and area-counted in production, never functionally
composed. None is special; none is wired. The honest statement is *"listed in
the manifest, never composed"*, not *"absent from the manifest"*.

And the newest document's own framing is the one to quote, because it is about
the port rather than the manifest:

> *"terrain currently has **NO colour path into the rasterizer at all**"* —
> `zhao_terrain_project`'s output packet carries positions, depth, view and the
> Mosaic triple, **and no rgb**.

That is confirmed independently: PROJECT's port list is `out_ax_o…out_cy_o`,
`out_behind_o`, `out_src_id_o`, `out_ad/bd/cd_o`, `out_view_o`,
`out_mat_a_o/out_mat_b_o/out_weight_o`. No colour field exists, so no terrain
colour *could* reach the rasteriser even if SHADE were built today.

The documents' *intent* is unambiguous that lighting exists: `terrain_rules`
§4.4 says normals are derived from the composed lattice by finite differences at
tessellation time, *dirty patches only*, precisely because Sacrifice kept
pre-crater normals forever. The missing block is named — **`TERRAIN.SHADE`**,
~700–900 ALM, 8–10 DSP — and it is *"not cuttable, because it is the terrain's
light."* Its oracle is **planned and not written**, and it has no ledger entry.
So: the intent is written, the organ is built, the consumer does not exist, and
the port it would feed does not exist either. **Two items, steps 6 and 7 of the
build order.**

**C2. Flat-shaded terrain versus a Gouraud-interpolated vertex tint — a real
conflict.** `MATERIAL_ARCHITECTURE.md` and `terrain_rules` §6.4 both say layer H
(33×33 RGB565) is the lightmap heir, *"rides the Gouraud path and costs no
sampler"*. `NORMALMAP-ARCHITECTURE.md` (newer) says *"terrain IS flat-shaded per
triangle — `shade_flat_tri` is the ratified law and the reference shades flat;
vertex normals are explicitly unratified"*, and has `TERRAIN.SHADE` emit **one
flat colour placed on all three vertices as planes with zero gradient**. **A
zero-gradient colour plane cannot carry a per-vertex tint.** Under the newest
document, **layer H — 2,178 B/patch, 2.1 MiB across the resident set — has no
consumer and no path.** Either the flat law drops layer H, or `TERRAIN.SHADE`
must modulate by an interpolated tint, which requires ratifying the vertex-normal
averaging rule `TERRAIN.NORMALS` deliberately left open. **Needs an owner ruling;
it is not decidable from the documents.**

**C3. `Missingterrain`'s missing-list is partly stale — do not build from it.**
It names four missing things; two now exist. **"Patch-residency manager"** is
built (`residency_v2`, `UNIT_VERIFIED`, 38 checks, including two the prototype
cannot express: overlapping islands, and a dirty victim held non-resident until
the journal ACKs). **"SW.STREAM is literally still a stub"** was true on 08-31
and false from 09-02 23:00. Also **`TERRAIN.MIPGEN` now exists** and was not in
anyone's missing-list. **Docket D4 still carries the 08-31 text and should be
re-swept.**

**C4. The architecture document's ten OPEN questions are all ruled.** It was
written 2026-09-02 20:31; T1–T12 landed 21:44. Anyone reading it as current will
re-litigate settled law and, worse, build to superseded answers:

| its OPEN | ruled by | the answer |
|---|---|---|
| 1 residency key | **T1** | `{resource_epoch, island_id, patch_ix, patch_iz}`; overlapping islands legal; the built interface is **superseded** |
| 2 guard map | **T2** | exact bank-2 addresses, deny-by-default, **state-aware** |
| 3 memory client | **T3** | `ZHAO_CLIENT_TERRAIN_BUILD = 6`, **best-effort**, does *not* join guaranteed round-robin because a page is late. Client 5 stays unspent |
| 4 writeback vs mirror | **T4** | **no B/D writeback**; F journal with an ACK barrier; three separate dirty bits, not one |
| 5 command ABI | **T5** | **one `SubmitTerrainSet`** for the whole set — *not* one `DrawProcedural` per patch |
| 6 cache overflow | **T6** | 5-step deterministic pressure order; > 256 **required** dynamic ⇒ frame fault, repeat prior frame, record source IDs |
| 7 prefetch policy | **T7** | both views unioned, Moore ring, **30 frames ahead**, 32 pages/frame ceiling |
| 8 mip side | **T8** | **FPGA**, `TERRAIN.MIPGEN`, exact decimation. *HPS does not implement a second mip law* |
| 9 collision law | **T9** | **direct map rejected**; 256 sets × 4 ways, CRC-8/ATM set index; no software non-collision rule |
| 10 teardown | **T11** | **full reset is not level unload**; `TerrainEpoch END_FLUSH` drains, waits pins to zero, flushes every `dirty_F`, waits ACKs |

**C5. The architecture document's hazard finding is now moot but its lesson is
not.** §2.2 found a genuine same-cycle `fin`+`claim` port hazard in the
prototype and made it an integration obligation. T10 makes it a **block rule**
(*"same-cycle old FIN/DIRTY/UNPIN can never modify a newly reserved occupant"*)
and v2's suite tests it. The hazard was real; the block it was found in is not
the block being shipped.

**C6. Binner addendum versus R7** — §5. *"A bigger constant is exactly what will
do"* against *"the production binner does not grow a frame-sized on-chip
arena."* R7 is later, is a ruling, and explicitly acknowledges the LOD
measurement without conceding to it.

**C7. `reports/bandwidth` does not exist.** Empty directory, `.gitkeep`,
2026-08-14. Strike the citation.

**C8. A documented prose defect in the LOD scale semantics.** `blocks.yml` (at
`MEASURE.GOVERNOR`) records that `zref_terrain_lod.hpp` documents `scale` as
*"allowed error per unit distance, larger = coarser"*, which is **backwards**
relative to the ladder implemented in the same file *and* in
`zhao_terrain_lod.sv` (`dev * scale <= distance * h`, so larger scale = **finer**).
The arithmetic is consistent; the prose is inverted. Whoever tunes the governor
against the prose will tune the wrong way.

---

## 8. THE ACCEPTANCE GATE

Not a static image. The **8 km traversal capture**: fly rapidly across ≥8 km of
authored world, force residency churn, deform patches, leave them beyond the
1,024-slot horizon, return, and verify:

* geometry **bit-exact against zref** on every composed patch, **both views**;
* **scars and breaches identical on return** — this is what measures T4's
  journal barrier, and it is the property no single-frame check can see: *a
  dirty F sheet lost without a journal ACK silently heals terrain the player
  destroyed*, unrecoverably, because the data is already overwritten;
* **zero stale-handle *consumptions*** (stale *detections* are expected and
  healthy);
* LOD/stitch **crack-free across every streaming boundary** crossed;
* the sealed list bytes replay byte-identically (replay does **not** rerun the
  visibility walk);
* plus **teleport**, **two-island collision at identical local coordinates**,
  and **Duo**.

Judged the repo's way, per CLAUDE.md: **per-frame counter trajectories**, not a
handful of evenly-spaced stills — a flat `collisions` line *is* "it never
thrashed" — and **frames sampled by badness** (max skip count, max load
latency), never by index. Run twice, assert identical.

---

## 9. THE HONEST SUMMARY

The owner brief's central judgement survives everything above and should be
quoted rather than paraphrased:

> *The terrain engine's organs exist. The circulatory system that feeds them an
> island does not.*

What has changed since 08-31 is that the circulatory system is now **fully
specified** (T1–T12 + `SW.STREAM.md`) and **two of its pieces are built**
(`RESIDENCY` v2, `MIPGEN`). What has *not* changed is that **no page has ever
been moved into VRAM by hardware, no visible list has ever been sealed, no
composed cache exists, and no terrain triangle has ever reached the rasteriser
with a colour on it.**

And the sequencing is not this lane's to change: **Phase 6.** Texture repairs,
the texture-survivor composition and the physical gates come first, then
`GEOM.PARAMBUF`, then this. The standing reject is explicit — *do not add
further leaf blocks until the texture island is composed and fit.*

The two things that can be done **now, at zero fabric cost**, and that would
sharpen every number above:

1. **Commit T1–T12 into the senior specs** (`terrain_rules`, `memory_rules` §5b,
   `commands.zidl`) so the architecture document's superseded OPEN list stops
   being read as current, and re-sweep docket **D4**.
2. **Run `zref::Binner` over a frame of terrain with the LOD ladder and the
   underside applied.** It needs no RTL, costs nothing, and it is the single
   missing input to every terrain arena decision. Every terrain row in the
   binner analysis is a level-0 patch, and the ladder spans 64× between level 0
   and level 3.
