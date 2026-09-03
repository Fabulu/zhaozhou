# LANE 4 DIGEST — Creatures, animation memory, and spectacle

Recon only. No RTL, tests, ledger or manifest was touched; no synthesis was run.
Sources read completely: the two animation architecture documents (both committed
`10035d63`, **09-03 14:13**, the newest owner brief in the tree),
`reports/CREATURESANDLIGHTS` (08-31 10:42), `reports/SUNDER.md` (08-31 14:05),
`reports/DoubleHelixTornado.md` (08-31 16:08), `reports/CapeProvisions.md`
(08-31 09:03), `reports/ZIXXTRIXX_CEL_IN_HARDWARE.md` (08-31 06:58),
`reports/Hardwareguyfixzixxtrixx` (08-28 12:26), `spec/creature_rules.md`
(08-28 01:39), `design/contracts/GEOM.POSE.md` (08-18), `GEOM.SKIN.md` (08-23).

**True date order matters here.** The docket calls `CREATURESANDLIGHTS` "the
unifying document, read after the others", and that is a *reading* instruction,
not a precedence claim: it is 08-31 10:42, which is **older** than SUNDER and
the tornado, and **much older** than the two animation documents. Where
`CREATURESANDLIGHTS` and the animation documents disagree, the animation
documents win. Where `creature_rules.md` (08-28) and `CREATURESANDLIGHTS`
(08-31) disagree on the lighting law, `CREATURESANDLIGHTS` wins — and the
**live reference** (which it defers to) wins over both.

---

## THE ANIMATION MEMORY RULING IN ONE PAGE

Both 09-03 documents state the same law. The reconciled statement:

> **Cartridge storage is cold. HPS/ARM DDR owns the authoritative loaded
> animation library. The 128 MB local SDRAM owns only a pinned render working
> set. `GEOM.POSE` sees only complete, immutable, verified, locally resident
> clip pages, through `MEM.GUARD`, and never waits on HPS DDR.**

Mechanism, in the order it happens:

1. `.zpak` kind-9 clip-bank sections load into HPS DDR and become an
   **immutable asset generation** only after container/version, section
   CRC-32C, resource-page SHA-256, clip-directory/frame/event bounds, and
   optional midpoint/deformation shape checks all pass. Hot reload creates a
   *new* generation; it never mutates bytes under an in-flight frame.
2. `SW.RUNTIME.HPS` exposes future animation demand; `SW.STREAM` is the **sole
   logical writer** of the local animation residency region and owns residency,
   upload scheduling, verification, mapping, pinning and eviction.
3. Upload is a bulk HPS→local-SDRAM copy of a **complete residency unit**. The
   local bytes are bit-identical to the HPS bytes — no conversion, no matrix
   expansion, no semantic rewrite on the way in.
4. Publication is **atomic and post-verification**. `UPLOADING`/`LOADING` is
   never visible to the FPGA. A short or corrupt copy never becomes resident.
5. `SW.CMDBUILD` resolves every animation reference against one complete
   resource-epoch snapshot and **refuses to seal a frame** with an unresolved or
   non-resident reference. Sealing pins every referenced unit.
6. **The publication invariant:** a frame packet may enter `READY` only if every
   animation byte it can cause `GEOM.POSE` to read is already resident,
   immutable and pinned for that frame's resource epoch. Pins survive until
   `DONE → FREE`. Pinned bytes never move, never change, and cannot be evicted.
7. Address reuse **advances a generation / moves to a new resource epoch**, so a
   stale handle can never silently name new content. Frame packets carry
   validated resident resources, never naked long-lived local addresses.
8. A residency miss is **never** a blocking FPGA fetch. `GEOM.POSE` may not
   request cartridge I/O, may not touch HPS DDR, may not wait for an upload, may
   not change residency, may not interpret a partial page, and may not repair
   malformed data. The FPGA never sees a null local pointer, an HPS address
   posing as a VRAM address, a partial clip, a stale generation, or a "stall
   until Linux gives me this" request.
9. **Any creature may carry baked 60 Hz presentation data.** "Hero tier only" is
   dead as an architectural restriction — it was a capacity argument, and
   HPS-backing removes the capacity argument. 30 Hz authored keys remain
   gameplay truth; events stay attached to authored keys; the midpoint is
   presentation data selected by the `sub` phase.
10. Residency granularity, window size, eviction policy and prefetch look-ahead
    are **deliberately not frozen**; whole-bank residency first, pages only if
    traces show pressure.

### What it supersedes

Exactly one thing: any reading of `spec/creature_rules.md` §2.2 or
`design/contracts/GEOM.POSE.md` implying the animation library permanently
occupies local SDRAM — the wording *"VRAM stores clips compressed"* (still live
in `spec/creature_rules.md:83` and in the GEOM.POSE contract header as *"Clips
live in VRAM compressed"*). It changes **nothing** about: kind-9 semantics, the
30 Hz gameplay clock, hard-cut transitions, event-tag timing, `quat16` format
(qformats §7.6 / QFMT_VERSION 2), decode-on-fetch, the type-grouped
pose-sharing economy, the no-per-limb-upload rule, full/micro geometry formats,
or determinism. The rejection of baking all poses to 3×4 matrices stands.

### THE ONE PLACE THE TWO DOCUMENTS DIFFER — the miss behaviour

This is a real divergence, not a wording variance, and it needs an owner call.

| | `..._MEMORY_ARCHITECTURE.md` §9 | `..._HPS_RESIDENCY_ARCHITECTURE.md` §6.1 |
|---|---|---|
| Base behaviour on a late unit | **The frame is not published.** The previous complete frame repeats under the existing hard-60-Hz late-frame law. Fault recorded. | The HPS **must choose a deterministic degradation before sealing**. |
| Is a fallback ladder required? | **No.** "The base architecture requires no such fallback and remains correct by repeating the previous complete frame." A later content-declared fallback *may* be added, and must then be explicit, deterministic and capture-visible. | **Yes, one of a ladder**, and the ladder is per-instance: hold the previous valid pose for *that instance*; use an always-resident rest/bind pose; drop to splat or glint; omit the visual instance while simulation continues; **or** decline to publish. "The exact fallback ladder is a content-tier decision and remains to be frozen." |
| Granularity | whole frame | **per instance** |

Both agree the choice happens on the HPS **before sealing**, and that the FPGA
never guesses and never waits. They disagree on whether v1 owes a per-instance
degradation path or may simply repeat the frame. **Take the MEMORY document's
position as the v1 law** — it is the more conservative reading, it needs no new
frame-packet field, and it is the behaviour `CMD.SCHEDULER` already implements
(`deadline_faults++`, video repeats, contract §"Overflow"). The RESIDENCY
document's ladder is then exactly what it calls itself: a content-tier decision
not yet frozen. Recording this as an open owner question rather than picking
silently would also be defensible; what is *not* defensible is implementing the
per-instance ladder as though it were already law.

Two smaller, harmless differences, recorded so nobody "discovers" them later:

* **State-machine names.** MEMORY: `ABSENT → HPS_READY → UPLOADING → RESIDENT →
  PINNED → RESIDENT → EVICTING → HPS_READY`, plus an explicit `FAILED`.
  RESIDENCY: `ABSENT → LOADING → VERIFIED → RESIDENT → PINNED →
  RESIDENT/EVICTABLE → EVICTING → ABSENT`. Same machine. RESIDENCY names the
  verification step as a state; MEMORY names the HPS-backed rest state and the
  failure state. Merge them; do not implement two.
* **Counter vocabulary.** Two non-overlapping name sets for the same telemetry
  (`anim_resident_bytes_vram` vs "current and peak resident animation bytes",
  etc.). MEMORY explicitly says names may adapt to the generated trace
  vocabulary but the information may not disappear. Pick the RESIDENCY spellings
  (they are concrete identifiers) and keep MEMORY's trace-event list
  (`ANIM_PREFETCH/RESIDENT/PIN/UNPIN/EVICT/RESIDENCY_FAULT`).

### Capacity arithmetic both documents agree on

One authored skeletal key = `12 + 8B` bytes; at the 32-bone ceiling **268 B**.
A midpoint companion is roughly another key's worth. A pathological frame of 128
distinct pose tuples reads ≈ **34,304 B** of compact pose input, ≈ **2.1 MB/s**
of local-SDRAM pose-source traffic at 60 Hz before burst overhead. **A stable
battle whose banks are resident generates zero animation upload traffic**,
however many pose requests are served. Normal playback must never generate one
HPS transaction per creature per frame.

The charter §7 pool *"meshlets, LOD data and animation data: 24 MB"* is
reinterpreted: it holds the **active animation residency window**, not the
loaded library, and stays dynamic.

---

## HARDWARE CONSEQUENCES

Both documents claim "no new render-time hardware path". **That claim is true as
stated and materially misleading as read.** It is true that no new *arithmetic*
is needed, that `GEOM.POSE`'s datapath, the `quat16` decode, the skinning maths
and the decoded-pose cache's *logical* key are untouched, and that no
`MEM.HPS.BRIDGE → GEOM.POSE` edge may be added. It is false that the impact is
near zero: **the control plane needs five changes, one of them an RTL edit to a
block that already exists, and one of them a block that does not exist at all.**

Verified against the tree, not against the documents' own summaries.

| Block | Required change | Status |
|---|---|---|
| **`GEOM.POSE` (`zhao_geom_pose_cache.sv`)** | **Add `sub` to the cache tag.** The built tag word is `{lru, frame, clip, type}` (line 132) and `acquire` is `(type, clip_slot, frame)` — **no `sub`**. The reference `zref::creature::PoseBank` *does* carry `uint8_t sub` in its `Slot` and in `acquire(type, slot, frame, sub = 0)`. So the ruling's claim that baked 60 Hz "does not change the pose-cache tuple law, which already distinguishes `sub`" is true of the reference and **false of the silicon**. As built, an authored key and its 60 Hz midpoint alias to one tag and the cache hands back the wrong palette. Cost: +8 tag bits × 128 slots (1,024 bits) and one `acq_sub_i` port. Tiny; **mandatory**, and it is a correctness bug the moment any bank ships a midpoint. | **RTL edit required. Not optional. Not yet done.** |
| **`GEOM.POSE` datapath / decode** | None. `pose_requests + local clip_pages → bone_matrices/palette handle` is unchanged. No HPS client, no animation AXI fetcher, no page-fault stall path. | **Correct as built.** |
| **`GEOM.POSE` contract text** | Delete *"Clips live in VRAM compressed"*; replace with "complete, immutable, verified clip pages already resident in local SDRAM under the frame's resource epoch"; add the explicit exclusion "no direct HPS-DDR access and no page-fault stall path". | Spec edit; **not done**. |
| **HPS→local-SDRAM asset upload engine** | **Does not exist as a production block.** `MEM.HPS.BRIDGE` fans out only to `CMD.DMA` and `PART.STATE`. `CMD.DMA` is explicitly *"no blit engine and no VRAM writes at all"* since 2026-08-21. The only block in the tree that reads HPS DDR and writes local SDRAM is **`DEBUG.FRAMEBLIT`** — which is exactly the right *shape* (64-byte HPS bursts → guarded local writes → CRC-32C over the source → publish-after-retire) and exactly the wrong *block*: debug-tier, destination locked to a framebuffer slot, `byte_len` rejected unless it equals `canvas_bytes(mode)`, and it holds the MEM.GUARD framebuffer **write lease**. It cannot be reused as-is; it is strong evidence the mechanism is cheap and already proven once. Both documents say a generalised uploader "may be required if the final platform lacks a suitable one" — **the platform lacks one.** | **New block (or a production promotion of the FRAMEBLIT mechanism). NOT built. This is the load-bearing gap.** |
| **`MEM.HPS.ARBITER`** | Today it is a **two-client** arbiter (`CMD.DMA`, `DEBUG.FRAMEBLIT`) over a bridge with **one burst in flight**. An asset uploader is a **third requester**, so this block changes and re-fits, and animation prefetch then contends with sealed-frame-packet fetch on a single-burst bridge. Its protocol traps must be honoured: the bridge request is a **one-cycle pulse** (holding it is a request-while-busy violation) and a **write burst gets no response at all** — it ends on `wr_last`. | **Change required. Not done.** |
| **`MEM.GUARD`** | Two changes. (a) **Append an animation residency region** with read-only grant to `GEOM.POSE` — the contract says Phase 2 has exactly two FB regions and *"later phases APPEND regions, never reshape the law"*, so this is in-pattern. (b) **Generation/epoch rejection.** Both documents require the guard to reject a stale generation or epoch; today the guard checks `addr ≥ base ∧ addr+len ≤ end` for the client's owned region plus a **named-writer lease**, and the only generation in the picture belongs to `VIDEO.SLOTMGR`'s framebuffer lease. Stale-handle rejection is **new capability**, in the guard or relocated to grant time in `CMD.SCHEDULER`. Re-run `mem_guard_no_escape`. | **Change required. Not done.** Placement is an open decision. |
| **`MEM.VRAM.ARBITER` / `spec/memory_rules.md` clients** | Mostly already paid for: **`ZHAO_CLIENT_TERRAIN_BUILD = 6`** exists (ruling T3, §5d) as a *best-effort / background* client for "HPS→local page loads, F-sheet writeback, prefetch, and staging". That is precisely the animation-upload client, so no new client id is needed — but memory_rules already carries the standing obligation: **re-run the MEM.ARBITER liveness and guard proofs after adding client 6**, and the arbiter's best-effort class is *empty in Phase 2 (ports reserved)*. Note the collision: SUNDER wants the same client 6. | **Seam reserved; proofs pending; contention with D7 unresolved.** |
| **`CMD.SCHEDULER`** | Must associate **resource pins with sealed frame slots** and release them only on the completion-fence transition. Today it owns the `FREE → ARM_WRITING → READY → FPGA_RUNNING → DONE → FREE` FSM, grants VRAM regions per frame, and carries `BeginFrame`'s single epoch — there is **no resource-pin table and no per-resource epoch**. The RESIDENCY document assigns this to `CMD.SCHEDULER`/frame-slot ownership explicitly. Cheap (a small pin table), but it is a change to an `RTL_VERIFIED` block with a formal FSM property. | **Change required. Not done.** |
| **`CMD.SCHEDULER` deadline behaviour** | **No change** under the recommended v1 law — the existing "late seal ⇒ previous frame repeats, `deadline_faults++`, no fence for the dead frame" is exactly the required behaviour. This is the one place the ruling genuinely costs nothing. | **Correct as built.** |
| **`GEOM.SKIN`** | **No change, and no change is permitted.** It fits at **89.65 MHz, 9 DSPs, 2,225 ALMs, 1,696 registers, II = 12 → 124,514 vertices/frame (104% of the 120,000 target)** at `MUL_LANES = 3`. Doubling to 18 DSPs buys 13% throughput and makes the clock *worse* (89.65 → 84.61). Input is position-only (`v_x/y/z`, `v_w0`, `v_rigid`, `v_src_id`, two pre-selected matrices); output is position + `src_id`. **The word "normal" does not occur in the contract.** *"Nothing more may be bolted onto its output"* is load-bearing. | **Frozen. Do not touch.** |
| **`DEBUG.COUNTERS` / trace** | New residency telemetry. Most of it is HPS-side and software-visible; `hps_ddr_bytes_by_client` and `vram_bytes_by_client` already exist and already discriminate the new client. | Additive; **not done**. |
| **`design/blocks.yml`** | Clarify `SW.STREAM → local animation residency → GEOM.POSE`. **Do not add `MEM.HPS.BRIDGE → GEOM.POSE`.** `SW.STREAM` is `SPECIFIED` with `downstream: []` and an empty `maturity_log` — the block that the entire ruling rests on is a stub with the note "leaf". | **Not done.** |
| **`SW.STREAM.md`** | The contract's TODO ownership sections are exactly what both documents were written to fill: backing store, upload, verification, mapping, pinning, eviction, failure. | **Not done.** |

**Bottom line on the "no new hardware" claim:** no new *maths*, no new
*render-time* path, no new dependency inside a frame — those hold. But the
ruling needs **one new production block (the HPS→VRAM uploader), a third client
on `MEM.HPS.ARBITER`, an appended `MEM.GUARD` region plus generation checking, a
pin table in `CMD.SCHEDULER`, and an RTL tag-width edit in
`zhao_geom_pose_cache.sv`.** Four of those five are re-fits and re-proofs of
`RTL_VERIFIED` blocks on the tight fabric axis. Small individually; not zero,
and not free of formal-property work.

### Staging both documents agree on

Stage A specs only (no hardware) → Stage B HPS authoritative store + asset
generations → Stage C **whole-bank residency** and bulk copy, deliberately
coarse, proving ownership and deadline laws before paging → Stage D
trace-driven refinement → Stage E final `GEOM.POSE` integration through
`MEM.GUARD`, proving no direct HPS edge exists in the composed design.

---

## THE CREATURE PRESENTATION LANE

`CREATURESANDLIGHTS` is the unifying statement, and its central idea is a
**granularity ladder** — each expensive thing collapses at the level where it is
cheapest, and nothing is evaluated at a finer granularity than it needs.

| Work | Granularity | Where |
|---|---|---|
| Secondary motion (cape, hair, tails, tassels) | **per bone**, ≤12 patched bones/instance | HPS / shared runtime, deterministic fixed 60 Hz |
| Point-light geometry reduction (delta, distance, normalise, attenuation) | **per creature or per meshlet** (≤64 verts + local bound sphere) | light-context setup, not the vertex loop |
| Lighting accumulation | **per unique vertex** — then cached in `GEOM.PARAMBUF` and reused by every tile reference | `GEOM.CREATURE.LIGHT` |
| Toon quantisation | **once per surviving cel fragment** | `RASTER.TOON` |
| Outlines | **once per view**, from an explicit written mask | `RASTER.INKSTORE` + `POST.INK` |

Pipeline order, and the order is the design:

```
MESHFETCH → VDECODE → CREATURE.DEFORM (bind/model space)
  → base-pose / sparse-patch matrix selection
  → SKIN.POS ∥ SKIN.NORM
  → CREATURE.LIGHT (all selected lights accumulate into vertex RGB)
  → PROJECT / CLIP → PARAMBUF → BINNER / EDGEWALK / ATTRSTEP
  → Early-Z → TEXJOIN / TMU / material combiner
  → RASTER.TOON → FRAGMENT / TILESTORE ∥ INKSTORE metadata
  → RESOLVE (RGB565 + effect tag + full-res ink seed)
  → POST.INK (exterior classification + dilation) → POST.COMPOSITE / scanout
```

Deformation happens in bind space, secondary bones modify the *pose*, position
and normal are skinned **once**, all lights accumulate before interpolation,
toon bands quantise the interpolated light, texture modulates the banded
lighting, fog applies, and ink overlays at the very end of post.
`RASTER.FRAGMENT` stays ignorant of how the creature was animated or lit, and
`TEXJOIN` still emits one final texel packet after its bounded 0–3 samples.
**No creature shader is invented anywhere in this lane.**

### The lighting-law repair — CONFIRMED, and it costs more than the spec admits

The docket's claim is correct, and I verified it in the live code rather than
taking either document's word.

* **The old law**, still written as LAW in `spec/creature_rules.md` §2.x item 2
  (08-27): `lam = (w0·clamp(N·L_b0) + w1·clamp(N·L_b1) + 32) >> 6` — light
  pulled back into each bone's bind space, Lambert taken **per bone**, clamped
  per bone, and the **scalar** results blended. The spec is explicit: *"no
  renormalisation anywhere, which is the cheap form the silicon increment would
  build."*
* **The live reference** is `skin_normal_lambert` in
  `reference/src/zcreature/creature_core.cpp:550`, called from
  `creature_sim.cpp:876`, and labelled in-source **"V10 structural repair"** and
  **"transformed-normal blend → normalise → one clamp"**: transform the packed
  s8×3 bind normal by both bone matrices, blend with `w0`/`w1` keeping the full
  weighted direction, range-reduce, `isqrt_u64` the magnitude, take **one** dot,
  divide by the magnitude with one rounding, clamp once.
* **Why it changed** (the source comment, and it is the art law in miniature):
  the old path *"is not the deformed surface normal: illumination followed
  influence weights, and pure/near-pure regions appeared as arbitrary bright
  patches that changed as neighbouring bones disagreed."*

**The live reference becomes the law**, and `spec/creature_rules.md` §2.x item 2
must be rewritten. **But the same spec's "what the silicon increment now costs"
paragraph is now wrong, and nobody has said so.** It lists per vertex: 2×2
fixed-point dots, the `w0`/`w1` blend, and the rig's 3 saturating mul-adds — no
square root and no divide, because the law it was costing had neither. The live
law needs, per lit vertex, **an integer magnitude (`isqrt`) and a divide by that
magnitude**, at the 120,000-vertex target. `CREATURESANDLIGHTS` independently
reaches the right structural answer — build **`SKIN.NORM`** as a parallel narrow
pipeline (the bind normal is only s8 per component, so much narrower products
than the 32×32 position farm), **renormalise once**, emit a compact world
normal, join the position engine by sequence tag, and then every light performs
only `lambert = max(0, dot(world_normal, L))`. It also flags that the reference
calls `skin_normal_lambert` **three times** (key, fill, point) and therefore
repeats the blend and the normalise per light: *"the hardware should not
reproduce that structure."* Refactoring the reference to skin the normal once and
reuse it is step 3 of its own implementation order.

**So the true cost of the repair is one new normalising normal pipeline, not a
free spec edit** — and it must not be bolted onto `GEOM.SKIN`'s 89.65 MHz
output. Needs M10K? No; a sequence-tag join FIFO at most.

### Toon — the one thing actually built

`RASTER.TOON` RTL exists (`fpga/rtl/raster/zhao_raster_toon.sv`, 346 lines,
11/11 tests). It rescales each lane by `q/mean` rather than replacing light with
the band value, so the Cool Cross blue fill stays blue in shadow:
`(30000, 45000, 90000) → (27272, 40909, 81818)`, 1 : 1.5 : 3 preserved.
Measured **4.11 clocks per surviving cel fragment → 405,515/frame** against a
320,000 stress profile; it took four versions (201 → 39 → 9.29 → 4.11) and the
middle two were the same mistake — *measuring a block that issues one job and
waits tells you how long a job takes, not how many it can do.*

Two arithmetic details decide whether the creature looks like itself: the
division **truncates toward zero** (C++ semantics, *not*
`zhao_raster_attrdiv`'s round-half-away-from-zero — reusing the attribute
divider would be off by one on most fragments, invisible in a screenshot and
fatal to a capture CRC); and the mean's own `/3` truncates too **and it decides
the band**, so one code of drift there moves a band edge, which is a visible
line on the creature.

**Two cautions, and they are the CLAUDE.md gate law in the wild.** `RASTER.TOON`
has **no entry in `design/blocks.yml`** — I grepped; it is unledgered RTL. And
`CREATURESANDLIGHTS` is explicit that it *"has not been entered into the
committed per-block Quartus map/fit table and was not present in the 53.48 MHz
composed fit"* — **4.11 clocks is throughput evidence, not Fmax evidence.** Its
own implementation order says: put it through standalone map/fit and integrate
after `ATTRSTEP`.

Multiple lights do **not** multiply toon cost: everything becomes one RGB value
per vertex, three lanes are interpolated, and one ratio-preserving band
operation runs per surviving fragment whether the colour came from two lights or
eight.

### Ink — "the genuinely awkward one" (D14)

The shipped contour is **not** an inverted hull and **not** an edge detector. It
builds the visible creature mask, floods the background from the viewport border
with **four-neighbour** connectivity, treats only reached background as
exterior, dilates the creature outward through that exterior with
**eight-neighbour** connectivity, never overwrites a creature pixel, and
**leaves enclosed holes unoutlined** — which is why Zixxtrixx's S-shaped
opening stays clean. Inverted hull is a **measured fallback, not the answer**: it
outlines inner holes, shows meshlet seams, varies width with orientation, and
misbehaves under the spring deformation.

Hardware shape:

* **Do not infer the mask from final RGB. Write it explicitly.** A parallel
  ping-pong `RASTER.INKSTORE` indexed exactly like the colour tile store —
  **not** a widening of the frozen 64-bit tile word (24-bit RGB / 8-bit effect
  tag / 24-bit depth / 8-bit stencil). A nibble per visible pixel suffices:
  `[1:0]` ink group, `[3:2]` width code (none/1/2/4).
* Write rules on a depth-passing opaque fragment: inked creature writes group +
  width; a closer ordinary opaque surface writes zero and occludes the ink;
  translucent/additive depth-preserving effects leave the metadata alone;
  alpha-tested fragments write only when they survive.
* **Full resolution is mandatory** — a 1 px outline cannot use the quarter-res
  glow/distortion buffers. A packed 4-bit seed map is small: Storm 320×240 =
  37.5 KiB, Z60 384×240 = 45 KiB, Duo 512×240 = 60 KiB. **Storage is not the
  problem; connectivity is.**
* The exact algorithm is a **scanline-span flood**, not a pixel queue and not
  repeated whole-screen sweeps: 1-bit subject mask from nonzero seeds; subject
  and exterior bitsets in banked on-chip RAM; seed every background run touching
  an active viewport border; push horizontal spans `{y, x0, x1}` into a
  **sequential** FIFO in local SDRAM; pop, scan neighbouring rows for unvisited
  background runs, mark exterior, enqueue. Each background pixel is marked once.
  On overflow, **suppress ink for that view and count it — never guess
  connectivity and accidentally fill an enclosed hole.**
* Width is then four bounded eight-neighbour max-propagation passes
  (`next_radius = max(neighbour_radius − 1)`, propagating only through exterior
  pixels, carrying group and priority), final ring condition
  `exterior && propagated_radius > 0`. ≈492,000 pixel ops for Duo, fewer
  otherwise. This is exactly the union of adaptive Chebyshev dilations.
* **Duo is two separate view rectangles.** Each seeds from its own border; the
  centre boundary is not a route from one player's exterior into the other's.
* Creature-to-creature: four groups (0 none / 1 player-hero / 2 ordinary /
  3 boss), flood on the union, one-pixel group-discontinuity rule preserves the
  player's boundary. Full 8-bit per-object outline ownership is deferred and
  should not be a v1 requirement without a visual case proving four groups fail.
* Post ordering: compute distortion/refraction source coordinate → sample colour
  **and ink** with that same displaced coordinate → bloom/haze/grade the colour
  → **overlay ink last**. A global white flash normally goes before ink,
  preserving the dark drawn line; changing that is an art decision.
* Transaction law: colour framebuffer, effect buffers and ink sidecars share one
  framebuffer lease and generation, and a frame is publishable only after colour
  writes, ink classification, dilation and sidecar writes have **all** retired.
  This is a genuine extension of the framebuffer transaction (step 11 of the
  implementation order) — the slot manager's whole purpose is preventing a
  partially written presentation bundle from becoming visible.

M10K: yes, this one wants real on-chip RAM — banked subject/exterior bitsets and
a row window. It is the one creature feature with an honest M10K appetite, and
it is *also* the feature most likely to be implemented incorrectly if treated
casually.

### Cape and secondary motion (D6)

**No cloth processor, no per-vertex wind evaluation, no cloth-vs-world
collision, no `GEOM.WARP` dependency, no uploading a fresh 32-matrix palette per
creature per frame.** Four provisions, all small:

1. **Reserve bones now.** 6 for an ordinary waist/thigh cape, 8 for a long
   dramatic one, as **two columns × three or four rows** — a single centre chain
   can flap fore-and-aft but cannot billow asymmetrically, curl round one side,
   or twist during a turn. 150–300 visible triangles, more for a thin closed
   shell with an independently coloured inside. Two-weight skinning covers it
   exactly. Hero budget: body/weapon/face 20–22 + cape 6 + hair 3–4 = **29–32**,
   inside the frozen 32-bone limit.
2. **The one genuinely missing feature: per-instance pose overrides.**
   `GEOM.POSE` caches by `{type, clip, frame}` — right for armies, wrong for a
   cape in wind, because mutating the shared palette's cape bones mutates every
   instance sharing that pose. v1 transport is a bounded **sparse matrix
   patch**: `{instance_id, bone_mask[32], final_skin_matrix[patched_count]}`,
   computed on the HPS with the same fixed-point library authentic PC and ZEMU
   use. A maximal 12-bone patch is **12 × 48 = 576 B per hero per frame ≈
   34.6 KB/s**; a hundred creatures averaging four patched bones ≈ **1.15 MB/s**.
   The unmodified body bones stay shared, so the pose-cache economy survives.
   **Where the mux goes matters:** `GEOM.SKIN` explicitly *"does not index the
   palette"* — it receives two already-selected matrices, and a palette port's
   fan-in *"would dominate this block's timing for no gain."* So patch-first
   selection is a **new upstream selector stage**, never a `GEOM.SKIN` change.
   `GEOM.LOOM` is conceptually the right eventual home (parent-before-child
   composition of a topologically sorted graph supplied by ARM/compiler) and
   **must not become a cloth simulator**; it is specified only.
3. **A deterministic fixed-timestep spring solver in the shared HPS/PC/ZEMU
   runtime.** Six or eight moving nodes is trivial software. Inputs: authored
   target pose, root linear and angular acceleration, world wind (relative wind
   = world wind − character velocity), gravity, stiffness, damping, bend/twist
   limits, gust noise. The authored animation supplies *intention* (tucked in a
   roll, lifted in a jump, spread during a spell); the solver supplies wind and
   inertia around that target. Baking flutter into every clip would look canned
   on a character you stare at constantly and would not respond to a different
   wind direction.
4. **Authored collision proxies and clip-specific targets.** One upper-back
   plane/capsule, two hip/thigh capsules, maybe a pelvis capsule, plus angular
   limits stopping the upper cape folding through the shoulders; then re-enforce
   segment lengths. **No self-collision, no triangle collision, no arbitrary
   world collision in v1.** *"A cape occasionally brushing through a wall is
   tolerable; a cape permanently living inside the wizard's arse is not."*

Frozen content limits: **≤8 bones per secondary group, ≤3 groups on one hero,
≤12 patched bones per instance, ≤6 body collision proxies.**

Wind needs **one** sample near the shoulders (optionally a second at the lower
edge), from the ordinary world wind function, a Field program, or scripted
forces — **not** a per-vertex flow-field sample, and **not** a dependency on
completing a Flow accelerator (`FIELD.SEQ.FLOW` is a program/profile on the
shared Field engine, not a separate unit). The generalisation is deliberate: the
same solver serves ponytails, braids, coat tails, belts, straps, tails, floppy
ears, antennae, tassels and creature tendrils. Build **secondary-motion bones,
not cape support.**

The cape's inside surface: the rasteriser's coverage path already flips a
negative-area triangle rather than dropping it, so the reverse side will not
vanish — but a paper-thin surface carries one authored normal and its cel bands
read wrong from behind. Model the hero cape as a **very thin closed shell**
(outward-normal outside, slightly offset inward-normal inside with a darker or
contrasting colour, narrow stitched silhouette edge). That avoids a two-sided
cel-lighting rule entirely for the cost of trivial geometry on one character.

### Deformation — `GEOM.CREATURE.DEFORM`, deliberately not `GEOM.WARP`

A narrow, already-referenced machine: sample carries `{flatten, spread}`;
per-vertex metadata carries `{centre, carrier point, radial/follower/none,
cardinal axis, strength}`. A radial vertex contracts along one authored axis and
expands across the perpendicular plane; a follower rides the carrier's
contraction without being crushed; **unmarked vertices and zero samples take an
exact identity bypass**; normals get the inverse-transpose correction and
integer renormalisation. **Pack it factored** — group carries centre/axis/role,
vertex carries `group_id` + strength — not as the C++ `DeformVertex` per vertex;
Zixxtrixx's rings share centres naturally.

It gets **its own ready/valid stage with an internal tag between VDECODE and
SKIN**. Do **not** fuse it into `GEOM.SKIN`'s 89.65 MHz path. Because position
skinning accepts a weighted vertex only every 12 clocks, the deform stage may
take several cycles per *affected* vertex without limiting the lane, provided it
pipelines or bypasses ordinary vertices immediately. `GEOM.WARP` stays deferred
and unnecessary — arbitrary per-vertex programs sampling wind fields would be
the genuinely expensive version.

### Multi-light — separate "lights in the world" from "lights on a creature"

**The FPGA must never loop over all emitters.** The shared runtime owns a
spatial light catalogue (`{stable source_id, world position, inner/outer radius,
RGB gain, authored priority, flags}`), a spatial grid finds candidates, and for
each visible creature the runtime deterministically selects the strongest
relevant lights by estimated intensity at the creature's bound, projected
importance, authored priority, **stable source-ID tie-breaking, hysteresis and a
minimum hold** so light sets do not flicker between near-equal candidates.
Losers accumulate into a **clamped coloured spill ambient**. Big lightning or
explosion flashes use a **global view/world flash envelope** rather than
consuming a slot on every creature. Glowing particles, star sprites and magical
glows already have a cheap path — they write the existing effect-tag channel and
post blooms them, so **a hundred glowing sparks are not a hundred diffuse
lights.**

Point-light geometry reduces **before** the vertex loop: one light context per
meshlet and selected light (centre → delta → distance → normalised direction →
folded RGB × attenuation), then all ≤64 of that meshlet's vertices share it.
Hero meshlets want a **compiler-enforced radius ceiling** so the approximation
stays close when a spell is near. The existing exact per-vertex point light
(delta, `isqrt`, normalise, attenuation *per vertex*) is *"an excellent quality
oracle and a poor multi-light silicon architecture"* and stays as the comparison
oracle for a three-way bake-off (exact per-vertex / per-creature context /
per-meshlet context). Per-meshlet is expected to survive 240p well; if one very
close light still shows, reserve an **exact-near slot for the hero's strongest
light only.**

Light core: `L0` three products for the dot → `L1` add and clamp Lambert →
`L2` Lambert × RGB and accumulate. **Six DSPs** accept one full coloured
contribution per clock after filling. Accumulate in a widened lane, **clamp
once, and quantise to the existing 1/16 gain ladder once after the complete
sum** — never per light; the reference sums ambient + key + fill + point before
`quant_shade`. Ambient and spill need no dot products.

Face-character preservation: the live reference blends **~80% smooth vertex
Lambert + 20% triangle-face Lambert** (`kSmoothMixNum = 819`/1024), which stops
coherent normals making a hand-cut creature look like smooth plastic. Full 80/20
for world key, world fill, and the strongest local light on near/hero creatures;
remaining local lights may use the smooth normal only. **"Drop local face terms"
is the first lighting degradation step, before removing a whole light.**

What local lights explicitly do **not** imply: shadow maps, per-pixel normals,
normal maps, per-light specular, or fragment shaders. Coarse light occlusion may
be evaluated on the HPS for the selected top-K and folded into intensity; the
FPGA does not cast spell shadows.

### The Measure — degradation order (per view, deterministic)

Drop optional second/third material samples → remove local-light face terms →
reduce local lights 6 → 4 → 2 → 0 → collapse omitted lights into coloured spill
→ reduce ordinary-creature ink 4 → 2 → 1 px → merge ordinary ink groups → move
creatures down mesh → micro-mesh → splat → glint.

Protected: **the player keeps at least a 1 px outline**; the player keeps
secondary motion at the hero rung; world ambient/key lighting remains; **one
view's spell storm cannot consume the other view's guaranteed tier**; and
degradation never mutates gameplay state. Light count, ink tier and material
sample count should become explicit parts of the representation contract the
governor and token architecture already want.

### The representation ladder carries the cel *identity*, not the hero *form*

| importance | representation |
|---|---|
| hero / close | full mesh, `bake60`, full toon, exact 2–4 px exterior ink |
| near army | reduced mesh, same ramp, 1–2 px ink |
| mid | micro-mesh, simplified bands, 1 px silhouette |
| small | prebaked cel splat with painted outline |
| tiny | animated glint |

200 × 1,930 tris = **386,000 triangles** before terrain, objects, spells or
particles. *"That is not a 60 Hz content tier and should not become one."*
**Hundreds of cel-shaded Zixxtrixx-class creatures: yes. Hundreds of
simultaneous full-detail hero Zixxtrixxes: no, deliberately.** The economies
that make it work: all instances share one atlas; instances on the same
`{type, clip, frame, sub}` share one decoded pose; far splats bypass the toon
service entirely because the banding is baked in.

### The prerequisite that governs the whole lane

The composed renderer reached **53.48 MHz**, not 100. At 53.48 MHz, position
skinning at II = 12 delivers only **~74,000 weighted vertices/frame — already
below the 120,000 demand before any lighting.** *"The light architecture is not
the reason for the problem; restoring the intended clock is the prerequisite."*
Every guarantee in this lane — four lights on a near creature, six on the hero —
is an architecture target, not a hardware-backed promise, until the timing
surgery lands and the full composed fit (shell + renderer + TMU/cache/AUX +
Field/Earth + creature deform/skin/light/toon + ink/post) is measured.

### Current state, honestly

| Capability | State |
|---|---|
| Smooth three-band toon | RTL exists, 11/11, streamed rate passes — **unledgered, unfitted** |
| Ambient + key + fill lighting | working reference only |
| One dynamic point light | working reference, expensive per vertex |
| Radial squash/spread deformation | complete fixed-point reference + asset metadata |
| Sparse cape/hair pose patch | specification only |
| Proper exterior-only ink | exact software reference only |
| Position skinning | RTL + provisional standalone fit (89.65 MHz) |
| **Normal skinning** | **reference only — the RTL outputs positions, not normals** |
| Pose cache + decode | **RTL exists** (`zhao_geom_pose_cache.sv`, `zhao_geom_pose_decode.sv`) — ledger still says `REFERENCE_COMPLETE`, and the tag lacks `sub` |
| Complete creature lane in shell | not built, not fitted |

Implementation order the document itself sets: (1) finish the 53 MHz repipeline
to ~100 MHz with reserve; (2) make the blended-and-renormalised normal law
authoritative; (3) refactor the reference to skin a normal once; (4) build and
fit `SKIN.NORM`; (5) `CREATURE.DEFORM` as an independent stage; (6) sparse
patches in HPS/PC/ZEMU + patch-first selection; (7) one-light
`CREATURE.LIGHT`, then the 4/6/8 frontier and per-meshlet setup; (8) `RASTER.TOON`
standalone fit, integrate after `ATTRSTEP`; (9) `INKSTORE` + full-res seeds at
resolve; (10) scanline-span exterior + four-pass dilation; (11) extend the
framebuffer transaction; (12) the complete physical fit. **Only after step 12
does "four lights guaranteed, six on the hero" graduate from architecture target
to console promise.**

Risk, split honestly into capacity vs difficulty (the document's own correction
of an earlier single conflated number):

| Feature | Capacity | Engineering / semantic |
|---|---|---|
| Toon bands | 1/10 | 3/10 — still needs physical fit |
| Secondary bones | 1/10 | 3/10 — the pose-override seam |
| Fixed radial deform | 2/10 | 3/10 — exact normals/followers/bypass |
| Four local creature lights | 3/10 | 4/10 — light sets, normal lane, fit |
| Six hero lights | 4–5/10 | 4/10 |
| Eight on every full creature | 7/10 | 5/10 — **do not guarantee it** |
| Proper exterior ink | 2–3/10 | 5/10 — exact connectivity + transaction |
| General cloth / per-vertex programs | 8–9/10 | 9/10 — deliberately excluded |

---

## SPECTACLE

### Sunder (D7) — P2, "a stretch goal, but one we really want"

**Authorisation:** the owner's words are *"a stretch goal … shouldn't cost too
much so please consider it"* and *"as a stretch goal if we still have silicon
left. We'll think about the actual software implementation later once we know
what we can and can't do."* The document's own recommendation: **architect now,
implement after the 53 MHz repair and the first complete base-machine fit.** So:
**architecture authorised, RTL is not.**

Why it fits: each terrain column is exactly one solid interval `bottom … top`,
or void. For a cut plane `cut_y = a·x + b·z + c`,
`remaining_top = max(bottom, min(old_top, cut_y))` and the severed cap is
`{chunk_top = old_top, chunk_bottom = cut_y, solid where old_top > cut_y}`.
**Both results are still one interval per column, so both remain legal
dual-heightfield terrain.** `a = b = 0` gives a flat nub; nonzero gives a
diagonal one. This is *far* cleaner than generic mesh slicing, and the
mountain-cap case is *"unusually clean."*

The insight the whole feature turns on: **the missing abstraction is a movable
terrain body, not a new terrain engine.** The existing `body_patch` seam
(written for a terrain-bodied giant: transform world queries into node-local
space, evaluate ordinary terrain, transform normal and velocity back out, rigid
+ uniform scale only) *already is* the beginning of this architecture — a
severed island half is the same abstraction. Generalise to a `TerrainBody`
table; **body 0 is the static world with an identity transform.**

Proposed blocks: `TERRAIN.FRACTURE` (page splitter), `TERRAIN.CUTTXN` (atomic
transaction), `TERRAIN.BODY` (descriptor table + transform + identity bypass),
`FORGE.CUTFACE` (exact boundary-cell clipper), and optionally
`TERRAIN.COMPONENT` and `TERRAIN.BODYQUERY`.

Hardware asks and cost class (the document is explicit these are architectural
shape, **not synthesis numbers**):

* `TERRAIN.BODY` — descriptor RAM, body tags, transform selection, identity
  bypass, **reusing shared geometry transform and projection. Class: small.**
  **Critically: do not add another terrain projector** — shared projection is
  already identified as a major silicon lever, so a moving body enters the
  *shared* geometry projector after its local-to-world transform.
* `TERRAIN.FRACTURE` — sequential reads/writes, comparisons, clamps, counters,
  line buffers, one plane-arithmetic lane. **Small to moderate, few or no
  permanently dedicated DSPs.** For a vertical split, most pages belong wholly
  to one side and are **not copied at all** — the new body directories simply
  adopt the existing page handles; only pages the fracture crosses need
  reconstruction. The terrain page is 21,376 B and already streams whole, so
  this is far friendlier than random per-voxel editing.
* `FORGE.CUTFACE` — the exact-boundary generator: terrain cell → two triangular
  prisms → bounded tetrahedral decomposition → evaluate the cut plane at
  vertices → **fixed marching-tetrahedra case table** → emit retained geometry
  for side A, side B, and the identical cut face with opposite winding. Fixed
  topology table, one iterative intersection divider, small vertex scratch,
  sequential meshlet writer. **Class: moderate.** Shared terrain edges must
  compute the intersection **once** under an exact fixed-point division law with
  an ownership rule, so adjacent cells generate the same vertex **bit-for-bit** —
  that is what stops cracks. Output becomes **ordinary RAW meshlets in local
  SDRAM** on the normal MESHFETCH → VDECODE → PROJECT → PARAMBUF → raster path:
  **no private renderer.**
* `TERRAIN.COMPONENT` (optional) — streaming run-length connected components
  (rows → runs → prior-row comparison → union labels → merge across patch
  borders → accumulate area/bounds/page ownership), union tables in SDRAM, small
  row window and union cache on chip. **Moderate control/RAM, no important DSP
  demand — and explicitly the first hardware feature to cut** if silicon or
  validation time is tight, because a single clean split does not need it: the
  cut descriptor already knows side A from side B.
* Optional mass/inertia accumulator — the boundary clipper already produces
  bounded tetrahedra, so one time-multiplexed MAC lane can accumulate volume,
  centre of mass, AABB, first moments, approximate principal inertia and exposed
  cut area. Event-rate work, so **one small MAC lane, not a physics unit.**
  Expose the results; **body simulation stays on the HPS.** A tiny ballistic
  integrator may exist as a **lab option, never gameplay truth.**
* `TERRAIN.BODYQUERY` (highest tier, optional) — broadphase an FPGA particle
  against a bounded moving-body AABB table, inverse-transform into body space,
  ordinary column lookup, transform normal and surface velocity back out. Lets
  sparks and dust bounce off a moving island half. **Units, navigation and
  canonical gameplay collision stay software-owned.**

Atomic publication (`TERRAIN.CUTTXN`) is the law: validate source generation →
claim scratch → build pages → build cut meshlets → CRCs/bounds/quotas →
validate all outputs → **publish new body directories at a frame boundary** →
release old references when no reader owns them. Until commit the game keeps
displaying intact terrain; on any failure, discard scratch, leave the original
untouched, record job/source id and reason. **The job may span multiple frames
and the 60 Hz renderer never waits for it** — a cut spell's anticipation, flash,
dust and shockwave can hide the latency, but the hardware contract promises only
atomic publication, not a cinematic delay. The player must never see both the
old mountain and the detached cap.

Memory: **client 6 `TERRAIN_BUILD`** (this is the same reserved client the
animation upload path wants — a real conflict to resolve, not a coincidence),
with the law *scanout and active rendering always win; refresh stays guaranteed;
fracture runs only from bounded leftover credits; fracture pauses at burst
boundaries.* On-chip storage limited to two or three terrain rows, cut
descriptors, one boundary-cell polyhedron scratchpad, a mesh-output FIFO, the
body table and transaction state — **pages, labels, cut meshes and scratch all
live in local SDRAM. No dependence on spare M10K for world-sized geometry.**

Six seams to reserve **now**, because retrofitting after packet and cache
formats freeze would be painful: `body_id` + generation in terrain patch jobs;
runtime terrain page **v2** (the v1 cell-state byte has five reserved bits that
v1 requires to be zero — v2 may assign `substance[1:0]`, `no_bake`,
`top_is_cut_surface`, `bottom_is_cut_surface`, `mesh_owned_cell`, surface class,
which is what lets the nub use exposed rock instead of grass and the cap's
underside use a fresh-cut material); a generated RAW meshlet pool; a reserved
local-SDRAM client and guard region for transactional construction; an atomic
terrain-directory publication mechanism; and a cut-surface stream **independent
of whether a plane stepper, the HPS, or a Field program produces it**
(*"exploit the Field machinery without making fracture silicon hostage to the
Field scheduler again"*).

Tiers, independently removable: **0** reserved seam → **1** Terrain Body →
**2** Planar Fracture → **3** Exact Cut Faces → **4** Surface/Slab Cuts →
**5** Automatic Fracture → **6** Full spectacle. Cut order: component detection
first, then moving-body particle collision, bounded repeated cutting,
curved/uploaded surfaces, hardware mass/inertia, the exact cut accelerator —
**never cut the Terrain Body seam.** Even with almost no spare fabric, **tiers
0–1 alone** let HPS software generate a flying mountain cap that Mantle renders
correctly. That is the graceful failure mode: the same body/page/render
architecture stays usable and software does only the fracture construction.

Build the **flying mountain cap first** — an impossibly clean magical cut, a
flat or angled exposed face, the original top preserved, a huge chunk moving
through the air, dust and lighting, and a permanently changed mountain
afterwards — because it avoids the hard questions (which half owns each unit, do
units inherit platform velocity, are Earth fields world- or body-local, are live
fields baked before separation, how are projectiles and navigation re-binned,
what happens when a chunk leaves the resident streaming region). Those are HPS
world-management semantics, not FPGA capacity. The sane first law: **at
separation, bake all affected live terrain fields, classify entities by their
support column, attach them to the resulting body, allow translation with
limited rotation, and make tiny components debris.** Translation-only already
looks extraordinary and requires no normal or geometry transform at all.

Known honest costs: **the current bake RTL is a radial paraboloid dig stamp — a
plane cut is not secretly available behind a flag**, and needs either a cheap
exact clip-to-plane bake path or HPS-side page construction plus upload (the
faster route to the first demo). `FORGE.CLIFF` **plans** rim walls but its
wall-vertex emission stage **is not written**, so through-cut side walls do not
exist yet. Exact oblique cut faces should be their own generator, not an
increasingly tortured `FORGE.CLIFF`. And the tessellator rejects coarse
subpatches containing void, so **the fracture seam stays at fine resolution** —
sensible, since the two unbroken halves stay coarse and the dramatic part gets
the geometry, but a long cut across an island consumes noticeable terrain budget.
A top-down diagonal renders as a 2 m staircase at the canonical pitch, which may
read as a rough fracture (desirable) or need a generated exact cut-face mesh
(for a magically precise sword plane) while the cell mask stays the conservative
gameplay representation.

Verification must be **structural, not screenshot-based**:
`volume(A) + volume(B) = volume(source)`; `interiors(A) ∩ interiors(B) = ∅`;
cut-face vertices identical and windings opposite; every output cell has
`top ≥ bottom` or is void. Plus: plane exactly through a lattice vertex; plane
exactly along a terrain edge; coplanar top or bottom; horizontal, sloped,
vertical and near-vertical; crossing patch borders; every supported pitch; with
authored and breached voids; through an existing scar; transaction reset or
overflow halfway; stale source generation; both cameras on opposite sides;
**identity terrain body reproducing the pre-body exact frame CRC**; moving body
streamed out and back in; body collapsing to a rigid proxy.

Explicitly outside: arbitrary caves, unrestricted Boolean CSG, voxel terrain,
unlimited fragments with rigid-body collision, general deformable rigid bodies.
*"A general 3D SDF that produces several separated vertical intervals still
falls outside Mantle"* and becomes generated mesh / Wound geometry.

**And the caveat the document states about itself:** *"we cannot call it
affordable yet. The only current composed fit uses 30% ALMs on an incomplete,
test-capacity cone and reaches just 53.48 MHz rather than 100 MHz. That result
cannot certify one additional stretch feature."*

### Double-helix tornado (D8) — P2, a WISH with a sequencing condition

**Authorisation:** *"This is a diversion … try to get a render out of it after
finishing the 53 MHz and whatever else important follows right after."*
**Explicitly sequenced after D1 (the MHz work). It is a wish and a site
showcase, not authorised console work.** Note the deliverable the owner actually
asked for is **a render for the Zhaozhou site**, not RTL.

**No new hardware is requested anywhere in this document.** It is a composition
proof: every part rides machinery already planned.

* **Terrain helix — the strong fit.** Two feet at
  `T1 = C(t) + R(cosθ, sinθ)`, `T2 = C(t) − R(cosθ, sinθ)`, with noise so it is
  not sterile. Each foot issues a **persistent bake/stamp by distance
  travelled** (every ~1–2 m), and successive overlapping stamps make a
  continuous trench. An 8–12 m footprint touches only a handful of 64 m
  patches — *vastly* under `BAKE_PATCH_BUDGET = 64`, which was sized for
  Volcano-scale effects. Mantle's persistent scar + breach machinery is
  *"almost tailor-made"* for cumulative destruction. Cost 2–3/10.
* **Composition, not one thing:** ~50% procedural geometry/ribbons, ~35% soft
  particles/sprites, ~15% polygon debris. **Not** a giant opaque cone
  (*"2001-era video-game tornado technology"* — looks solid, the ground
  intersection reads fake, close-up reveals the geometry, the other tornado
  vanishes behind it too cleanly, creatures inside become awkwardly occluded).
  **Not** pure billboards either — two tornadoes circling each other get seen
  from weird angles including from above, and billboards betray themselves. An
  opaque shell may exist only as a faint dark inner spindle.
* **Ribbons** are what `FORGE.PRIM` is meant to be good at (its vocabulary
  already lists ribbons, tubes, radial shells, rings, billboard sheets, spline
  walls, cones — *"basically a tornado construction kit"*). 3–5 helical ribbons
  per tornado, 24–40 vertical samples each, ~150–300 tris/tornado; both
  tornadoes' principal shape is **500–1,000 procedural triangles**, nothing
  beside armies and terrain. Being real 3D geometry, they read correctly from
  above, from the side, when one passes behind the other, up close, and in Duo
  where the two cameras see completely different angles.
* **Dust** is soft sprites on the already-planned cheap endpoint — one soft
  sprite per clock, no multiplier, no memory, a few adders and comparators;
  100–300 per tornado, aggressively reduced by screen-space LOD. Their motion
  (tangential swirl + inward pull + upward lift + noise) is exactly the **Flow
  profile of Field**, so **two vortex centres do not imply two hardware
  engines.** *"The tornado isn't an animated model. It's a bounded procedural
  vortex field generating a visible population."*
* **Debris** rides `PART.EXPAND` — projected particles become little geometry
  triangles at one per clock on essentially three adders and shifts. 30–100
  meaningful fragments across both tornadoes; polygon shards near the viewer,
  degrading to sprites and glints down the particle ladder. Dirt clods, rocks,
  structure fragments, leaves, bones, shattered terrain chunks. *"That's what
  will sell the mass."*
* **THE ONE REAL CONSTRAINT — alpha overdraw, 5/10, the only thing above 3/10.**
  Twelve translucent shells × two tornadoes × half the screen each turns cheap
  geometry into expensive fragment work. *"That's the trap."* Use **sparse
  ribbons that imply volume** (3 dark dust ribbons, 2 pale highlight ribbons,
  gaps between, lots of secondary particles) and stay visually porous — which
  both looks more turbulent and reduces fill. **At 240p your brain does a lot of
  the work.** The governing law: **tornado visual density is governed by screen
  coverage, not by a fixed particle count** — *"if both tornadoes are tiny
  on-screen: go fucking nuts. If one fills 70% of a Duo viewport: reduce ribbon
  count, particles and transparent layers aggressively."* The Measure is built
  for exactly this.
* **Gameplay needs no CFD** — two moving conical/cylindrical force volumes,
  radial inward + tangential + upward force per unit, HPS-owned: small creatures
  lifted, medium dragged and orbited, giants shoved but not lifted, projectiles
  bent, debris sucked in. The Field Flow profile supplies the same law to the
  visible particles, so units and particles appear to obey one vortex **without
  one gigantic hardware fluid simulation.**
* **The pair movement must be nasty, not polite:** orbital radius swinging
  10 → 22 → 6 → 18 m, orbit speed accelerating and slowing, each tornado leaning
  outward then snapping inward, phase approximately opposite but imperfect. So
  the terrain tracks are not pretty sine waves — **a violent braided scar.**
* **The ground contact matters more than the top** — dense dirt spray, rock
  shards, dust ring, ground-darkening surface sheet, terrain deformation, strong
  tangential motion.
* **And the point of the whole thing:** after ~10 s the tornadoes vanish and the
  two intertwined trenches **stay**, buildings along the path are gone, thin
  terrain may be punched through, creatures are scattered, debris has fallen into
  the void. *"Level 10 changes the topology of the world. Level 9 should leave
  the world looking like something terrible happened here."* You can read the
  map afterwards and reconstruct where the spell went.

It uses Field, Myriad, Forge and permanent Mantle deformation **without
requiring a new subsystem** — which is why it is the right showcase and why it
carries no hardware ask of its own.

---

## NUMBERS THAT MUST NOT BE INVENTED

Reserved for the owner, or explicitly deferred to measurement. Anything below
that appears in a commit as a chosen constant without an owner ruling or a trace
behind it is a violation.

**Animation memory (both 09-03 documents, §15 / §12 / §10):**

1. Residency granularity — whole-bank vs per-clip vs fixed-size page.
2. Local animation resident-window size. *"No fixed animation-cache size is
   frozen here."* Determined from traces after multiple creature types exist,
   baked 60 Hz sizes are known, real-board HPS→SDRAM upload performance is
   measured, and representative battle residency is captured.
3. Eviction algorithm.
4. Prefetch look-ahead duration and the prediction algorithm — the architecture
   freezes **who guarantees residency**, not how it predicts.
5. **The precise fallback ladder** on a missed residency deadline (and, per the
   divergence above, whether v1 has one at all).
6. Whether every creature is baked at 60 Hz **by default**.
7. Exact HPS DDR capacity, and measured board bridge latency/bandwidth —
   *"board-truth obligations."*
8. Further animation compression.
9. Physical implementation and block placement of the generic upload engine.
10. Byte layout / packed field widths of `AnimationPageId` and
    `AnimationResidentHandle` — *"an ABI decision"*; the documents freeze
    semantics, not packing.
11. Whether the 128-tuple pose cache is adequate — *"judged by measured
    mixed-species battle traces, not by total roster size."*

**Creature presentation:**

12. **Cel fog ordering** — `ZIXXTRIXX_CEL_IN_HARDWARE.md`: *"One decision that
    is not mine."* The general law fogs vertex colour before rasterisation; cel
    wants unfogged light → toon → texture → fog. It **contradicts** the general
    per-vertex fog law and is a **Class C visual-semantics change** needing an
    explicit recorded amendment, *"rather than smuggled into the
    implementation."*
13. **When the Gouraud/lighting increment lands in RTL** — *"the owner's
    scheduling decision"* (`spec/creature_rules.md`).
14. `kSmoothMixNum` = 819/1024 — the smooth/face blend is *"the owner's control
    over how much hand-cut read survives."* A named editable constant, per the
    art law.
15. Local light counts: **4 guaranteed** for a full-detail near creature,
    **6 target** for the main character and bosses, **8 hard format ceiling
    "enabled only when measured room exists"**, and *"do not promise eight on
    every full-detail creature simultaneously."* These are commitments awaiting
    the full physical fit, not facts.
16. Ink colour ≈ `{26, 24, 22}` and the adaptive width thresholds
    (≤120 px → 1 px; 120–200 → toward 2; 200–360 → toward 4; ≥360 → 4 px):
    authored art values.
17. Whether a global white flash precedes ink — *"an art decision."*
18. Full 8-bit per-object outline ownership vs four ink groups — *"I would not
    make it a v1 requirement without a visual case proving that the four-group
    solution fails."*
19. Pose-palette residence: **not M10K.** One 28-bone decoded pose = 28 × 48 =
    **1,344 B**; a 128-tuple cache ≈ **168 KiB** of matrices, *"a quarter of the
    device's M10K for pose data."* The RTL states the same bound from the other
    end: 128 × 32 × 12 × 32 bits = **1.5 Mbit ≈ 28% of the 553-block M10K
    budget for one cache** — and `zhao_geom_pose_cache.sv` deliberately **does
    not own the palettes**, emitting a verdict and a slot index so the caller
    owns the store, *"because burying it inside this module would settle it
    silently."* Tags and the current pose stage on chip; decoded palettes live in
    the local-SDRAM hot region. Alternatives named as owner choices: fewer
    tuples, palettes in SDRAM at a bandwidth cost, or a narrower matrix format.

**Sunder:**

20. Terrain-body table capacity — 16 baseline / 32 stretch, *"must be fitted
    rather than frozen."*
21. Runtime terrain page **v2** cell-state bit assignment and its semantic
    command — **Class C, needs an explicit versioned ruling. Do not casually
    assign an opcode or change v1 bytes in place.**
22. Cut material / cut material set and the newly-exposed-surface sheet tags
    (fresh rock, glowing magical cut, crystal interior, burning fracture).
23. Component-count bound (*"up to 16 material components per fracture job"* is
    a suggestion) and the debris-vs-body size threshold.
24. Which optional tiers survive — the whole tier ladder is an owner call
    against remaining silicon.

**Tornado:**

25. Ribbon/particle/debris counts are all screen-coverage-governed, not fixed:
    *"tornado visual density is governed by screen coverage, not by a fixed
    particle count."* The 3–5 ribbons, 24–40 samples, 100–300 particles and
    30–100 debris figures are sketches for a render, not budgets.
26. Mana territory (`Upheaval/docs/MANA-TERRITORY.md`) reserves **five
    numbers/questions** explicitly for the owner. Out of this lane's scope; noted
    so they are not lost.

---

## CONTRADICTIONS AND STALE TEXT

Ordered by how much damage each does if it goes unrepaired.

1. **`sub` is missing from the built pose-cache tag.** The animation ruling says
   baked 60 Hz *"does not change the pose-cache tuple law, which already
   distinguishes `sub`."* `zref::creature::PoseBank` does
   (`acquire(type, slot, frame, sub = 0)`, `uint8_t sub` in `Slot`).
   `zhao_geom_pose_cache.sv` does **not** — the tag word is
   `{lru, frame, clip, type}` and `acquire` takes three fields. As built, an
   authored key and its midpoint alias and the cache returns the wrong palette.
   **The document's central "no hardware change" claim rests on a premise that
   is false of the silicon.** Cheapest real fix in the tree; must not be found by
   an artist wondering why every other frame is wrong.
2. **The lighting law in `spec/creature_rules.md` is superseded by the live
   reference but still reads as LAW.** §2.x item 2 mandates blending clamped
   scalar Lamberts with *"no renormalisation anywhere"*; the live
   `skin_normal_lambert` blends the normal vector, normalises via `isqrt_u64`,
   then takes one clamped Lambert, and is labelled a **V10 structural repair**
   that removed bright patches at mixed-weight joints. **Repair before lighting
   RTL freezes.**
3. **…and the same section's silicon costing is now wrong.** Its *"what the
   silicon increment now costs"* list has no square root and no divide, because
   it was costing the superseded law. The live law needs a magnitude and a
   divide per lit vertex at 120,000 vertices/frame. Nobody has restated the
   cost. This is the single most likely place for a confident wrong number to
   enter a fit estimate.
4. **`GEOM.SKIN` outputs positions, not normals — confirmed, and there is no
   normal lane at all.** The contract's input table is position + weight +
   rigid flag + `src_id` + two pre-selected matrices; output is position +
   `src_id`; the word "normal" does not appear. So the corrected lighting law
   has **no hardware path whatsoever** today, and `SKIN.NORM` is entirely new
   work. Confirmed too: **89.65 MHz, 9 DSPs, 2,225 ALMs, 1,696 registers,
   II = 12 → 124,514 verts/frame (104%)** and *"nothing more may be bolted onto
   its output"* — with the measured reason it is a real ceiling: 18 DSPs buys
   13% throughput and *lowers* the clock to 84.61 MHz.
5. **`GEOM.POSE` caches by `{type, clip, frame}` — right for armies, wrong for
   a cape in wind.** Confirmed against the RTL, not just the docket. The fix is
   the sparse per-instance patch, and **the patch mux belongs upstream**, because
   `GEOM.SKIN` explicitly refuses a palette port on timing grounds. (Note items
   1 and 5 point the same way: the built tag is a 3-tuple, and the ruling needs a
   4-tuple *plus* a per-instance override seam beside it.)
6. **"VRAM stores clips compressed" is still live in two places** —
   `spec/creature_rules.md:83` and the `GEOM.POSE.md` header (*"Clips live in
   VRAM compressed"*). This is precisely the wording both 09-03 documents
   supersede. Until edited, the contract and the ruling say opposite things about
   where the animation library lives.
7. **The GEOM.POSE bake-rejection derivation loses its premise.** Its argument
   is *"ten resident types = 19.7 MB baked = 82% of the whole 24 MB
   meshlet+LOD+animation pool."* Under the ruling that pool no longer holds the
   loaded library, so the arithmetic no longer means what it says. The
   **conclusion still stands** — both documents keep the rejection, and are
   careful that baked *presentation keys* (compact clip data) are a different
   and much smaller proposal than baked *decoded 3×4 matrices for every pose*.
   But the derivation must be restated on the new premise, not left to look like
   evidence it no longer is.
8. **`RASTER.TOON` is unledgered RTL.** `fpga/rtl/raster/zhao_raster_toon.sv`
   exists with 11/11 tests and there is **no `RASTER.TOON` entry in
   `design/blocks.yml`** (nor `POST.INK` / `TILESTORE.INK`, which are not built).
   `CREATURESANDLIGHTS` independently confirms it is absent from the committed
   per-block map/fit table and was not in the 53.48 MHz composed fit. **Built,
   fast, and unaccounted for** — exactly the situation where "the gates pass"
   stops meaning anything.
9. **`GEOM.POSE`'s ledger maturity is stale.** `blocks.yml` says
   `REFERENCE_COMPLETE` (2026-08-17, pinned `bd1c733`), but
   `zhao_geom_pose_cache.sv` (331 lines) and `zhao_geom_pose_decode.sv` (363
   lines) exist. The ledger understates what is built — which matters here,
   because the block the animation ruling calls unchanged is further along than
   the ledger admits, and its tag is wrong.
10. **Client 6 is claimed twice.** `spec/memory_rules.md` §5d reserves
    `ZHAO_CLIENT_TERRAIN_BUILD = 6` as the background client for *"HPS→local
    page loads, F-sheet writeback, prefetch, and staging"*; SUNDER wants it for
    transactional terrain construction; the animation upload path needs exactly
    that description. Either they share it with an arbitration policy, or a
    second best-effort client id is needed. Unresolved, and memory_rules already
    carries the standing obligation to **re-run the MEM.ARBITER liveness and
    guard proofs after adding client 6.**
11. **The residency-miss divergence between the two 09-03 documents** — detailed
    above. Same date, same commit, both binding, materially different v1
    obligations (whole-frame repeat vs a per-instance fallback ladder).
12. **Stress coverage sits below the content tier.** Both documents' stress
    matrices top out at *"64–128 active creature instances"* and *"at least 50
    creature types"*, and the `GEOM.POSE` Phase-9 gate says *"64–128 active
    creatures."* The content tier to size against is **256 creatures, 128 per
    player in Duo** — so Duo at full roster is 256 instances, twice the largest
    case any of these documents actually exercises. Not a contradiction in law,
    but the verification plan does not reach the tier, and *"acceptance requires
    preserved pose/pixel goldens"* on a population half the size proves less
    than it appears to.
13. **`SW.STREAM` is a stub carrying the whole ruling.** `blocks.yml`:
    `maturity: SPECIFIED`, `maturity_log: []`, `downstream: []`, `notes: leaf`.
    Both documents assign it sole ownership of backing store, upload,
    verification, mapping, pinning, eviction and failure policy, and its
    contract's ownership sections are still TODO. The block the architecture
    rests on does not meaningfully exist yet.
14. **`Hardwareguyfixzixxtrixx` (08-28) is a one-line owner correction that is
    still outstanding as far as this lane can tell:** *"you put the zixxtrixx
    instructions in the wrong folder, there's a newer one. Please fix."* It
    concerns creature-art paths owned by the peer session, so I did not touch it
    — but it is the CLAUDE.md law "instructions are not delivered until they are
    read" firing again, and someone should confirm it landed.
