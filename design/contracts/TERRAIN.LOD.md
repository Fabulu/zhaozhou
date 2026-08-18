# Contract — TERRAIN.LOD (Terrain LOD selector)

> Ledger: `design/blocks.yml` · owner ZH-050 · phase 6 · maturity SPECIFIED

## Purpose and exclusions

Projected-error LOD selection with the deformed-height cache built once per
frame; consumes governor targets.

Implemented as `fpga/rtl/terrain/zhao_terrain_lod.sv`. For one patch — sixteen
subpatches in a 4 × 4 grid — this block runs the projected-error ladder against
both cameras, applies the charter §9 stability rules (hysteresis, minimum hold,
geomorph) and emits sixteen (or thirty-two, on a dual page) `lod_target`
packets. **The output packet IS `zhao_terrain_tess`'s job port, field for
field**, because a decision that cannot be handed to the tessellator is not a
decision.

Excluded: no tessellation (TERRAIN.TESS), no height evaluation (TERRAIN.PATCH
owns the composed lattice and the coarse mips), no memory port of any kind, no
per-frame history storage (see Notes law 5), no rim or underside LOD divergence
(see Notes law 7), and **no curvature, velocity or semantic weight** (see Notes
law 6 — they are named as missing rather than faked).

**THIS BLOCK'S LAW IS CHOSEN, NOT FOUND, AND THAT IS THE FIRST THING TO KNOW
ABOUT IT.** There is no ratified terrain LOD arithmetic anywhere in this tree.
Charter §11.5 lists what a projected error *combines*; charter §9 lists the
stability properties every LOD path must *have*; `MEASURE.GOVERNOR.md` is a
stub whose only content is "Hysteresis/hold constants provisional until Wound
Lab evidence". Every numbered item under Notes is a decision, recorded with what
it rejected.

## Clock and reset semantics

Single `clk`, active-low async `rst_n` (negedge), `gpu` domain per the ledger.
Reset returns the block to the fill phase with an empty decision store, an empty
output register and zeroed counters. No clock-domain crossing lives here.

The governor's targets (`cam*`, `hyst_i`, `min_hold_i`, `morph_step_i`,
`dual_i`, `edge_*`) are **not registered**: they are sampled as each descriptor
is decided. They must be held stable across a patch job, which is the ordinary
per-frame command-stream discipline.

## Input and output packet layouts

`patch_state` in, ready/valid, **exactly sixteen descriptors per patch, in a
fixed order** — descriptor *n* is the subpatch at `ox = (n & 3)·8`,
`oz = (n >> 2)·8`, i.e. x varies fastest. The order is not carried in the
packet; it is pinned by arrival, which is what makes `ox`/`oz` derivable and the
neighbour lookup meaningful.

| field | width | meaning |
|---|---|---|
| `sp_cx_i` `sp_cy_i` `sp_cz_i` | signed 32 | subpatch centre, fx16 world units |
| `sp_dev1_i` `sp_dev2_i` `sp_dev3_i` | 24 | \|fine − coarse\| height deviation at levels 1–3, fx16 unsigned. Level 0's is **zero by definition** and is not a port |
| `sp_prev_level_i` | 2 | the level currently displayed |
| `sp_prev_morph_i` | 17 | the geomorph factor currently displayed, Q16 |
| `sp_hold_i` | 8 | frames since this level was committed |
| `sp_src_id_i` | 16 | rides this subpatch's decision |

`lod_targets` (the governor), sampled with each descriptor:

| field | width | meaning |
|---|---|---|
| `cam0_x_i` … `cam1_z_i` | signed 32 | the two eyes, fx16 world units |
| `cam0_scale_i` `cam1_scale_i` | 16 | Q8.8 allowed error per unit distance |
| `cam0_en_i` `cam1_en_i` | 1 | per-camera enable |
| `hyst_i` | 16 | Q8.8 hysteresis band; below 256 reads as 256 |
| `min_hold_i` | 8 | frames a level must hold before a change commits |
| `morph_step_i` | 17 | Q16 geomorph advance per frame; **0 = snap** |
| `dual_i` | 1 | the page models an underside |
| `edge_nz_i` `edge_pz_i` `edge_nx_i` `edge_px_i` | 8 each | the adjacent patches' border levels, four 2-bit lanes, indexed along that edge |

`lod_decisions` out, ready/valid — **exactly `zhao_terrain_tess`'s job port**
(`job_ox_i`, `job_oz_i`, `job_level_i`, `job_lvl_nz_i`/`pz`/`nx`/`px`,
`job_morph_i`, `job_surface_i`, `job_dual_i`, `job_src_id_i`) plus one field:

| field | width | meaning |
|---|---|---|
| `out_hold_o` | 8 | the new hold, for the caller to store |

`tests/terrain/terrain_lod_tess.cpp` wires the two blocks port-for-port with no
adapter; if that stops being true, that file stops compiling.

Plus `lod_rep_count0_o`…`lod_rep_count3_o`, `terrain_triangles_emitted_o` and
`idle_o`.

## Backpressure rules

Ready/valid on both ports. `sp_ready_o` is high only in the fill phase, so a
caller cannot push a seventeenth descriptor into a patch that is being decided
or emitted. `out_valid_o` holds until `out_ready_i`, and the emit pointer does
not advance while it is held, so no packet is dropped or duplicated. Four stall
schedules ride the directed suite (none, alternate, 31-in-32, burst) and both
random lanes stall on a pseudo-random schedule per job.

## Memory ownership

**None.** No VRAM port, no cache, no M10K. The block's entire state is the
sixteen-entry decision store (16 × 43 bits), one latched descriptor, the two
square-root lanes and the output register.

That is a consequence of Notes law 5, and it is the reason the law was chosen:
an internal LOD history would be 1,024 patches × 16 subpatches × 27 bits ≈ 6.6 KB
of M10K that only this block could address, sitting beside the per-patch state
TERRAIN.PATCH already owns.

## Q formats and rounding

**The ladder has no rounding at all, and that is the point.**

Level *L* is admissible for camera *c* when, in raw integers,

```
dev[L] · scale_c  ≤  distance_c · h
```

which is the exact integer form of `dev_metres · (scale/256) ≤ dist_metres · (h/256)`
once both sides are multiplied by 2^24 — `dev` and `distance` are fx16 (÷2^16),
`scale` and `h` are Q8.8 (÷2^8). `h` is **256** for the strict ladder and
`hyst_i` for the relaxed one, so ONE comparator serves both. A rounded
intermediate here would flip the level at a different distance than the
inequality it claims to implement, and the flip point is the only thing a LOD
law is really made of.

`distance` is `isqrt(dsq)` — `spec/qformats.md` §7.2's **exact floor** square
root, the same one `terrain_rules` §3.7 already uses for a distance. Floor, not
round: the flip therefore happens at the floored value, and
`terrain_lod_directed` §2 pins it at `dev = distance − 1`, `= distance` (which
PASSES, the comparison being `≤`) and `= distance + 1`.

**Widths, stated rather than assumed.**

- `dx = cx − ex` is a difference of two s32, so signed 33; a square is ≤ 2^64 and
  the three-term sum needs **66 bits**. It is then saturated to the 64-bit word
  the root takes. The reference forms the same sum in `__int128` and saturates
  identically, so the two agree for **every input word** rather than inside a
  declared envelope.
- The root recurrence's `res` never exceeds 2^33 (it is `2·root·2^k` at step
  *k*), so `res + bit` fits inside 64 bits with room over. 32 fixed steps from
  `bit = 2^62`; the reference's leading `while (bit > num)` normalisation is
  dropped because the loop body simply takes its else branch until `bit` fits,
  which is the same answer in a fixed number of cycles.
- `lhs = dev · scale ≤ 2^24 · 2^16 = 2^40`; `rhs = distance · h ≤ 2^32 · 2^16 =
  2^48`. The comparator is 49 bits.

**The geomorph blend itself is TERRAIN.TESS's**, not this block's: here the
factor is only ever added to, subtracted from or reset, all in Q16 with a
saturating clamp at 65,536.

## Latency (fixed or variable)

Variable, and dominated by one number: **32 cycles of square root per
descriptor**. Per patch: 16 × (1 fill + 32 root + 1 decide) = 544 cycles, then 16
(or 32) emit cycles, so about **560 cycles per patch** at full readiness.

The two square-root lanes run concurrently, so a second camera is free.

## Target throughput

The ledger asks for **1 decision per patch per frame**. At ~560 cycles per patch
this block sustains about 2,900 patches per 1.67 M-clock frame (100 MHz at
60 Hz), against `spec/terrain_rules.md` §4.2's 256 live/visible patches — roughly
**11× the required rate**, and the ledger's target is met with a wide margin.
`terrain_lod_directed` exercises the rate implicitly through its stall schedules;
no separate rate case exists because the margin makes one uninformative.

## Overflow and malformed-input behaviour

**Input domain: the whole word on every field.** Nothing is assumed:

- The squared distance **saturates**, it does not wrap, and the reference
  saturates at the same value. The randomized lane B reaches it (468 saturations
  in a fast run) and the directed suite pins it at `INT32_MIN` eye versus
  `INT32_MAX` centre.
- `hyst_i` **below unity reads as unity**. A governor cannot make the band
  negative and invert the retention rule. Pinned by directed §3.
- `morph_step_i` **of zero SNAPS.** A governor that turns geomorph off gets
  instant level changes, which is the honest reading; the alternative (a factor
  that never reaches unity, so the level never changes) would silently freeze the
  ladder. Pinned by directed §5.
- `sp_prev_morph_i` **above unity is clamped to unity** before anything is done
  with it.
- **`dev` is not assumed monotonic in the level.** Physically it rises; nothing
  enforces it. The ladder therefore asks "which is the *coarsest* level that
  passes", not "which is the *first* that fails", and directed §1 pins that with
  a deliberately non-monotonic vector.
- **`dev[0]` is zero by definition**, which is what guarantees the ladder always
  terminates. It is not a port.
- **With no camera enabled nothing changes.** Stated rather than left to decay
  into "coarsest".

**THE LADDER MOVES ONE RUNG PER FRAME.** A subpatch that wants to move two
levels passes through the one between, because a geomorph can only blend
adjacent levels and because the target is always the *near* edge of the
hysteresis band, never an overshoot. That is a property of the law, not a
limitation of the implementation, and it is why every "where does this subpatch
come to rest" question in the tests iterates. Directed §1 asserts the walk costs
exactly *target* frames.

**The refine branch commits only once the factor has unwound to zero.** A
coarsening in progress is walked back before the finer level is adopted, so the
geometry never jumps mid-blend. That is why the randomized lane B seeds a
morph of exactly zero a quarter of the time — without it the refine commit is
unreachable, and a green lane would not have said so.

## Counters and traces

`lod_rep_count0_o` … `lod_rep_count3_o` are the four lanes of the ledger's
`lod_representation_counts` (charter §1838, "representation counts per LOD"),
counting subpatches emitted at each level. They advance on the **top** decision
only, so a dual page does not double-count a subpatch's representation. All four
saturate rather than wrap (`spec/counters.md` §4).

`terrain_triangles_emitted_o` is the **predicted** triangle total,
`2·(8 >> L)²` = 128 / 32 / 8 / 2 per subpatch. **It is an UPPER BOUND and is
named as one**: void cells emit nothing and the stitch annulus emits fewer than
an unstitched subpatch, so the tessellator's own count is lower. It is here
because charter §9 asks each visible root for an "approximate triangle/vertex/
fragment cost" and this is the only block that knows the level before the
geometry exists.

No counter-catalog id is bound: both names have several claimants and minting an
id is a `spec/counters.md` amendment, not an RTL decision.

## Scalar reference function

`zref::terrain::lod_select` in `reference/include/zref/zref_terrain_lod.hpp`,
with `lod_dsq`, `lod_dist`, `lod_ladder` and `lod_tris`.

The ledger's `reference_model` was **AMENDED** from `zref::TerrainLod` to that
symbol — a deviation from "honour the ledger entry", recorded as one, and the
same move `TERRAIN.TESS`, `TERRAIN.NORMALS` and `TERRAIN.PROJECT` made. It lives
under `zref::terrain::` where the terrain laws already are.

**Unlike this project's other terrain oracles, this header is mostly DEFINITION,
not view**, and it says so at every line. `zref_terrain_normals.hpp` was a
window onto shipped shading; this one states a law nothing had stated. So "RTL
matches oracle" is a weaker claim here than elsewhere, and the directed suite
compensates by checking the law's own properties by hand as well as agreement
with the oracle — see below.

## Directed tests

`tests/terrain/terrain_lod_directed.cpp` — **211 checks**, ten sections:

1. **Every level is reachable**, and the ladder is coarsest-that-passes: four
   targets walked to rest, each asserted to take exactly *target* frames (one
   rung per frame), plus a deliberately non-monotonic `dev` vector.
2. **The flip point is exact**, at `dev = distance − 1`, `= distance` (passes,
   the comparison being `≤`) and `= distance + 1`, with the distance first
   confirmed to BE `zref::isqrt_u64` of the squared distance.
3. **Hysteresis**, as a difference from the un-hysteretic answer: a level inside
   the band is retained, above it refines to the band's near edge, below it
   coarsens to the near edge, and a band below unity reads as unity.
4. **The minimum hold** refuses a change at `hold < min_hold`, permits it at
   exactly `min_hold`, and a refused change still AGES the subpatch — without
   which the hold could never expire.
5. **The geomorph walk as a TRAJECTORY**, feeding each frame's output back in as
   the next frame's history, which is the only way a state machine whose state
   lives outside it can be tested. Coarsen runs to completion in exactly
   65536/step frames and resets both factor and hold; refine adopts the finer
   level at once **with the factor at unity, so nothing jumps at the swap**, then
   walks back to zero; `morph_step = 0` snaps.
6. **The two cameras**: the far camera alone rests at the coarsest level, adding
   a near camera pulls it finer, the pair's answer IS the finer camera's, and
   with none enabled nothing changes.
7. **The neighbour levels at every one of the sixteen positions**, interior and
   border, with every subpatch given a DIFFERENT level so a lookup that reads the
   wrong cell cannot accidentally be right, and with all four edge inputs
   distinct.
8. **The dual page**: thirty-two packets, top then underside, same level, both
   carrying the dual flag.
9. **The counters**, after walking the patch to a known level mix.
10. **Backpressure** under four schedules, `idle_o`, and **the domain limit**
    (`INT32_MIN` eye against `INT32_MAX` centres), with the case first asserted
    to actually saturate the squared distance.

## Randomized differential tests

`tests/terrain/terrain_lod_random.cpp` — 701 checks in a fast run (24 island
runs × 12 frames + 400 limit runs; 200 and 3,000 nightly). Two lanes:

- **Lane A, island-shaped and run as a TRAJECTORY.** Sixteen subpatch centres on
  a real 64 m patch, deviations of centimetres to metres (what a 17×17/9×9 height
  mip produces), a camera quartering its distance each frame from far outside the
  island to right on top of it and back, and a real governor policy. The history
  is fed back frame to frame, so the block is exercised as the state machine it
  is rather than as a pure function sampled at random points. A LOD law's bugs
  live in its transitions.
- **Lane B, domain limit.** Half the runs at the word limit (where the squared
  distance saturates the root's input) and half near the origin (where the
  distance is small, the ladder rejects the coarse levels and the refine branch
  is reachable at all); deviations at the 24-bit rail, scale and hysteresis at
  the 16-bit rail, morph step at the 17-bit rail.

Both lanes assert they reached their states — a green lane that sampled nothing
is how a flooring defect elsewhere in this tree survived 20,000 random triangles:

| assertion | lane A (fast run) | lane B (fast run) |
|---|---|---|
| chose every level 0–3 | 309 / 857 / 1184 / 2258 | 1414 / 1633 / 1621 / 1732 |
| committed a coarsening | 410 | 786 |
| committed a refining | 19 | 267 |
| sat mid-geomorph | 1412 | — |
| had a change refused by the hold | 151 | — |
| saturated the squared distance | **0, asserted** (it is the real regime) | 468 |

Three of those numbers were **zero on the first run and the lane still passed
every differential**, which is the whole argument for coverage assertions: lane A
started every subpatch at the finest level, so the ladder only ever walked
upward and the refine branch was never entered; lane B drew the geomorph factor
uniformly, so it was never zero and a refine could never commit; and lane B's
coordinates were all at the word limit, where every level passes the ladder.

## Formal properties

**None, deliberately.** The two candidates were examined and both were rejected
for the same reason.

The square root's invariant — `res ≤ 2^33` at every step, which is what makes the
64-bit datapath sufficient — is bounded and provable, but stating it requires the
recurrence, so the property would restate the design. It is also *exhaustively*
established by the differential: `zref::isqrt_u64` is an independent
implementation of the same floor root, and lane B compares against it across
6,400 saturating and non-saturating distances per run.

The ladder's own properties (`T_strict ≤ T_relaxed`; the level moves at most one
rung; `dev[0] = 0` makes the ladder total) are all consequences of two-line
combinational expressions a solver would restate rather than cover.

A proof with nothing to cover is worse than none. Recorded so the next increment
does not re-derive the decision.

## Synthesis / resource ceiling

**Not synthesized.** `fpga/files.qip` is untouched, this block has never been
through Quartus, and nothing here has run on hardware. Structurally, from the
RTL: six signed 33×33 multipliers for the two squared distances, two 64-bit
compare-subtract root lanes, four 49-bit ladder comparators, and 16 × 43 bits of
decision store.

## Integration capture cases

None — no golden capture routes through this block, because the terrain draw
path in `render_frame` is still the software raster and has no LOD.

What exists instead is `tests/terrain/terrain_lod_tess.cpp`: **the real
TERRAIN.LOD drives the real TERRAIN.TESS**, port-for-port with no adapter, over
a real 33 × 33 lattice with relief, from three camera positions each chosen to
give the patch a MIXTURE of levels (a uniform patch would satisfy the stitch law
trivially). It asserts, on the geometry the pair actually emitted:

- **CRACK-FREE**: across every one of the 24 interior subpatch boundaries, the
  two sides emit the IDENTICAL set of vertices on the shared edge. That is the
  assertion neither block can make alone — if the neighbour lookup transposed x
  and z, or read the +z cell where it meant −z, both blocks would still pass
  every isolated test and the island would tear along every boundary.
- **The patch is tiled exactly**: the doubled signed areas sum to −2·A over the
  whole 64 m × 64 m patch — no gaps, no overlaps, every triangle wound the same
  way.
- No subpatch was rejected, and each scene really did mix three distinct levels.

## Notes

**LAWS CHOSEN, NOT FOUND.** Each is also argued in the RTL header and in the
oracle. What was rejected is recorded with each.

1. **The ladder is `dev[L] · scale ≤ distance`, coarsest wins, exact.**
   *Rejected:* a squared-domain comparison that avoids the root. It is exact too,
   but it needs `(dev·scale)²` — a 40 × 40 multiply — where the §7.2 isqrt needs
   32 iterations at a rate the ledger sets at one decision per patch per frame.
   And "camera distance" is §11.5's own word: a block whose contract can name its
   intermediate is worth more than one that saved a multiplier.
2. **Distance is Euclidean eye-to-subpatch-centre, via the §7.2 isqrt.**
   *Rejected:* view-space depth (the perspective-correct projected error). More
   accurate, and rotation-DEPENDENT: a player turning on the spot would
   re-tessellate the ground under their feet. Euclidean distance is
   rotation-invariant, which is what makes the decision stable and what lets one
   deformed-height cache serve both views (§11.5).
3. **The two cameras combine by taking the FINER decision.** Charter §9's Duo
   fairness rule — "one player looking directly into a volcano cannot make the
   other player's army disappear" — gives the ground to whichever camera needs it
   finest.
4. **Hysteresis is a band between a strict and a relaxed ladder**, and the target
   is always the band's near edge. *Rejected:* a distance-based dead-band. It
   needs another 32 bits of per-subpatch history and it does not compose with a
   governor that changes `scale` between frames.
5. **The history rides the packet.** *Rejected:* an internal history RAM (≈6.6 KB
   of M10K only this block could address, beside state TERRAIN.PATCH already
   owns). A block whose contract can say "Memory ownership: none" is worth more
   than one that saved a packet field.
6. **Curvature, velocity and semantic weight are NOT in v1.** §11.5 lists them;
   none has a ratified magnitude, a Q format or a source block that exists
   (TERRAIN.VELOCITY is phase 7). Folding an invented weight into the ladder
   would ratify it by omission. They enter through `dev` — whoever writes the
   mips may bias them — or as a later amendment.
7. **The underside takes the top's level.** `terrain_rules` §5 permits it to be
   coarser EXCEPT along rim boundaries, where it must be equal. Choosing
   "coarser" needs FORGE.CLIFF's rim edge set, which does not exist. Equal
   everywhere satisfies the crack constraint on every edge, at the cost of some
   underside triangles.
8. **The governor's per-camera policy is ONE ratio, not two numbers.** Charter §9
   gives a "projection scale" and a "per-camera pixel-error threshold"; the
   ladder only ever uses their quotient, and carrying the quotient keeps the
   comparison exact. Carrying both would force a division or a rounding inside
   the block for no expressive gain.

**MUTATION-CHECKED.** See the increment's report for the injected defects, the
binary hashes proving each relink, and which lane caught each.
