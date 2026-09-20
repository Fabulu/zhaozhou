# Owner decisions — the Zhaozhou console, 2026-09-20

**This file supersedes `reports/HANDOVER-20260919.md` §8 and its §13 additions.**
The SPENT section at the end lists which of those items are dead, and why.

Every item was **re-measured against the tree at `8a147d4f`**, not inherited
from the packet that raised it. Five items that arrived in this packet's own
brief as live decisions turned out to be spent; they are struck rather than
repeated. A dossier that lists a dead decision costs exactly the time it exists
to save.

**The device ceiling is 41,910 ALM / 112 DSP / 553 M10K.** It is the pass/fail
line. The only composed measurement that exists —
`zhao_console_core@console-core-first-light`, **47,582 ALM / 151 DSP / 306
M10K**, roughly **113% ALM and 135% DSP** — came from a **dirty tree**
(`treeCleanAtHead: false`) carrying a live metadata-swap defect, before this
run's repairs, and **it does not contain FIELD at all**.
`reports/FIT-PLAN-AT-ZERO.md` says it plainly: *"It is a starting estimate and
nothing more. Do not quote it as the console's size."* It appears below only to
give the ALM figures a scale.

**One thing to know before reading.** On 2026-09-19 you said *"go with your
recommended answers for now and don't stop to quiz me."* Every
**(provisional, coordinator)** ruling in `OWNER-RULINGS-20260919-EVENING.md` is
therefore already live and is **not** in this file. What is here is the residue
that standing authorisation cannot cover:

* **things you must LOOK at** — an art call cannot be delegated;
* **ABI freezes** — irreversible, and a wrong one becomes law;
* **area spends** — on a device already over on two of three budgets.

---

## THE SHAPE OF WHAT REMAINS

**21 mandatory gaps**, unmoved across eight packets: **9 tie-offs** (I13, I14,
I17, I20, I21, I27, I29, I32, I34) and **12 modules built but not connected**
(`zhao_measure_governor`, `zhao_terrain_normalmap`, `zhao_terrain_velocity`,
`zhao_terrain_bake_v2`, `zhao_terrain_lod`, `zhao_geom_warp`,
`zhao_geom_parambuf`, `zhao_forge_shadow`, `zhao_forge_prim`,
`zhao_forge_prim_eval`, `zhao_forge_cliff_ram`, `zhao_post_gather`).
Verified by `completion_register.py` at this commit: **9 + 12 + 0**. Nothing is
unbuilt any more.

Ruling R135 sets this document's agenda:

> **"So 'drive the register to zero' is, for a real fraction of what remains, a
> request to BUILD SUBSYSTEMS, not to connect existing ones."**

**Two of the twenty-one wait on your eye and on nothing else.** Both renders are
made, committed and pushed. No agent can advance either.

**Twelve decisions are live below.** Two are a look. Four are one sentence each.
The rest carry numbers.

---

# 1. THE SEAM DIG — does a rim wrong by one vertex read as a defect?

### LOOK AT `reports/terrain-seam-dig/seam_dig_contact.png`

246,790 bytes, committed `fd169553`, pushed.

## The question

A crater's depth at each terrain vertex is read from a 64x64 layer-F sheet by
**nearest texel**, because the sheet is not vertex-aligned. That makes the rim
wrong by up to one vertex — **everywhere around the crater, not only at patch
seams.**

**Is that acceptable, or does the page format move to a vertex-aligned 65x65?**

## What it unblocks

* **tie-off I32** (`surf_res_*`, SURFACE.STAMP's `stamp_results`) — directly.
* **`zhao_terrain_bake_v2`**, whose Option A needs a layer-F reader.
* **the terrain PAGE FORMAT**, which every later terrain block inherits.

R116: *"Six terrain lanes have now closed zero of the same four disconnected
blocks … That is not six failures; it is one blocker seen six times."*
**One look unblocks four queued packets.**

*Stated precisely, because the queue's "four blocks" framing conflates things:*
R65 gates **I32 and `zhao_terrain_bake_v2`**. The other two terrain blocks have
their own blockers — `zhao_terrain_normalmap` is decision 2,
`zhao_terrain_lod` is decisions 6 and 7. **Four packets, not four blocks with
one cause.**

## The evidence already gathered

**The render changed the question rather than answering it**, which is exactly
why it is in front of you. `spec/terrain_rules.md` §9.3(c), ratified after the
render:

> the fallback *"does not produce a visible CRACK ALONG THE SEAM. It produces a
> rim that is wrong by up to one vertex, EVERYWHERE, and the seam is one of the
> places it is wrong."*

Measured on the worst placement the tool can construct (`FINDINGS-terrain7.md`
§4 D1): **4 of 99 shared border vertices disagree, worst tear 3.25 m — the
dig's full depth** — but it lands inside a staircase the 1 m lattice already
produces, and the sheet's DIFF panel shows the two laws differing **all the way
round the crater**, not at the seam.

Law and oracle: `zref::terrain::sheet_texel_for_vertex`,
`zref::terrain::kStampDepthTable`,
`tests/terrain/stamp_to_bake_laws_directed.cpp` (306 checks).

## The cost of each option

| option | cost |
|---|---|
| **Accept the nearest-texel rim** | **Zero.** The reader is buildable immediately; `kStampDepthTable` (§9.3(a)) is format-independent and already ratified. |
| **Move to vertex-aligned 65x65** | **+38.3% page size.** R65 corrected R56's "+1.2%" to this. 8,450 B trips **three elaboration guards**, and `zhao_terrain_jdoorbell.sv` requires a **power of two** — the next is 16,384, so the page nearly doubles. `sheet_texel_for_vertex` then becomes the identity. |

**The packer does not exist yet, so the format is cheaper to change now than it
will ever be.** That asymmetry is why this is asked before the fit.

## Recommendation

**None offered, deliberately.** Every measurable thing about this fallback has
been measured and **the numbers do not decide it**: 3.25 m is the dig's full
depth, which sounds fatal, and it sits inside a staircase the lattice already
has, which sounds harmless. CLAUDE.md's art law reserves this to your eye, and
the coordinator deliberately did not open the sheet and substitute a verdict.
That was the right call and is why the item is still here.

**What follows from "no":** `sheet_texel_for_vertex` becomes the identity — so
the layer-F reader's address generator is **exactly the contested thing**.
Building it now would commit silicon to a format decision you have not made.

---

# 2. TERRAIN.NORMALMAP — ratify a spec sentence, or supersede the block?

## The question

`zhao_terrain_normalmap` has a contract, a ledger row, an oracle and a
**4,738-check suite**, and **no ratified spec sentence anywhere.**
**Ratify one, or supersede the block?**

## What it unblocks

**One of the twelve disconnected modules** — and it is **the only gap on the
board that an owner sentence closes today.** Supersede takes the register
**21 → 20 honestly.**

## The evidence already gathered

R115: *"Everything downstream of a specification exists; the specification does
not. This is the inverse of the phantom-reference defect."*

* `grep -rniE "normalmap|normal[ -]map" spec/` → **0 hits.** The lane fired a
  positive control first — `terrain\.shade|terrain_rules` → **14 hits** — so the
  zero is a result, not a broken instrument.
* The block is **instantiated nowhere at all**: not in the core, not in
  `zhao_prod_top`, not in any bench. It is the only one of terrain's four absent
  even from the generated census top.
* **The cut seam is provably safe.** `strength = 0` is a **bit-exact no-op**
  (`zref::terrain::normalmap_is_noop`), and its ledger row carries
  **`cut_order: 1`** — the only `cut_order: 1` in terrain's set.
* If ratified, the ledger estimate is **~380 ALM / 2 DSP / 8 M10K** for the
  block plus its tap.

**And ratification alone closes nothing**, which is the part that decides it.
The fragment stream it consumes (`f_u_i`, `f_v_i`, `f_lod_i`, `f_detail_i`, with
a `ready`) has no producer. The nearest candidate,
`zhao_geom_bin_pipe_v2`'s `stage_fragment_*`, has **no u, no v, no lod, no
detail and no ready companion** — the core itself calls it *"a structural probe
… not a handshaked stream"*. Closing it needs a **port change on a composed
block**, costing its whole instantiation chain plus every bench.

## The thing to know before choosing

TERRCOMP flagged this, and it is an art fact rather than an engineering one:

> *"the detail normal has **NO Y COMPONENT BY CONSTRUCTION**, so under a sun at
> the zenith the relief fades out. That is declared in the ledger rather than
> discovered, and it is why the look-gate was specified as a **MOVING SUN at
> 240p** rather than a still frame. Under the art law that is a LOOK, and it has
> never been taken."*

So "ratify" does not mean "keep a feature you have seen". **Nobody has ever
watched this effect move.**

## The cost of each option

| option | cost |
|---|---|
| **Ratify** | ~380 ALM / 2 DSP / 8 M10K, **plus a port change on a composed block** and its whole bench chain. And R115's real warning: *"writing one now to match the implementation would be ratifying whatever got built — which is how an accident becomes a requirement."* |
| **Supersede** | **Register 21 → 20 honestly.** Recovers the area. Bit-exact no-op, so nothing on screen changes. Reversible: one ledger line. |

## Recommendation

**Supersede**, on TERRCOMP's evidence and on your own ledger sentence, which it
quotes back: *"normal maps are not the thing presently threatening it. The
broken texture storage structures are."*

The asymmetry is the argument. **Ratifying is irreversible in the way that
matters** — it turns an unreviewed implementation into law, for an effect whose
relief vanishes under a high sun and which nobody has watched in motion.
Superseding is one ledger line, revisitable the moment terrain normal mapping is
something you actually want to look at.

A 4,738-check suite is evidence that the block does what it does. It is not
evidence that it is what the game needs.

---

# 3. THE GATHER LAW — does the proposed bloom read right at 240p?

### LOOK AT `reports/post-gather-law/gather_law_contact.png`

38,683 bytes, committed `a41a46da`, pushed.

## The question

R37 asked for the fragment-tag → glow law to be proposed from
`stars_and_flares.md` §1 with every coefficient in a named editable constant,
modelled in zref, and rendered for your eye. **It has been. Does it read?**

The law's entire content is four numbers: **knee 24, slope 0x1C, tint
255/236/224, master 255** (`zref::post::gather`, written into
`design/contracts/POST.GATHER.md`). The glow borrows the fragment's own colour.
**Displacement and ink are not invented** — channels 0b10/0b11 are unallocated,
contribute nothing, and are counted.

## What it unblocks

* **`zhao_post_gather`** — one of the twelve disconnected modules;
* **tie-off I17** (`post_gd_*`, `post_gg_*`).

**And nothing else.** `PACKET-QUEUE.md` states it exactly: *"Blocks
`zhao_post_gather` and I17, and nothing else."*

*A caution carried from this packet's brief and confirmed:* **R65 is not
load-bearing for POST.GATHER.** R65 is terrain's. The two contact sheets unblock
disjoint things.

## The evidence already gathered

The refusal used to be three objections long and is now **one sentence**, which
is why this is worth your time rather than another packet's:

* **Input — WITHDRAWN.** The old objection named the wrong stream.
  POST.GATHER's input *is* the resolved stream, field for field:
  `zhao_raster_resolve` emits `fb_valid_o`, `fb_rgb565_o`, `fb_tag_o` (8 bits)
  and `fb_addr_o`, and R37's law has the glow borrow `fb_rgb565_o` **on the same
  beat** — nothing joins across a stall. **The real obstacle is one 8-bit
  tie-off:** `zhao_shell_top_v2.sv` discards the tag as `rp_fb_tag_unused` while
  every neighbouring field leaves. One port, not a missing stream.
* **Output — CONCEDED.** The flush-to-random-access store is buildable now that
  `u_twod_sampler`'s atmosphere ring exists and is the same shape.
* **What remains is the ART LAW, and it is yours.**

Re-searched 2026-09-20 rather than inherited: every `reports/**/*.md` for
`gather_law_contact` and `R37`, every `OWNER-RULING*` file, every
`OWNER-DIRECTION-*` file. **No disposition exists anywhere.** There is no render
work outstanding.

## The cost of each option

Unfitted. The post lane's estimate for its composed blocks is **~740 ALM,
0–1 DSP, 3 M10K**, flagged in `FINDINGS-post.md` as an **estimate, not a fit**.
The adapter itself is a knee, a slope, a tint and a master: small.

## Recommendation

**Look, and say yes or no.** If yes, three small things follow and all are
engineering: the adapter (the law), the 8-bit shell tag port (the input), and a
plane store on the sampler's pattern (the output).

**Building the adapter IS committing to the law** — knee, slope, tint and master
are its only content — which is why no packet may do it for you. A bloom curve
can be bit-exact against its zref model and read wrong on a 240p frame under one
key light.

---

# 4. THE UNTEXTURED ATTRIBUTE LAW — the one nobody had written down

**This is the highest-leverage item in the document that has never been raised as
a numbered decision**, and it was found by three independent lanes hitting the
same wall without recognising each other.

## The question

**May a primitive enter GEOM.CLIP with `u/w` and `v/w` undefined — and if so,
what does the rasteriser do with the slot?**

## What it unblocks

Three seams, in three different subsystems, all blocked on this one sentence:

* **tie-off I13** — terrain triangles cannot enter `tri_*` (see below);
* **`zhao_forge_shadow`** — its Route A deadlocks for exactly this reason;
* **`zhao_forge_prim` / `zhao_forge_prim_eval`** — same wall, one family over.

## The evidence already gathered

`GEOM_CLIP_ATTRS = 7` (`zhao_console_core.sv`, `zhao_console_board.sv`).
GEOM.CLIP's ratified per-corner packet is **invw24, u/w, v/w, lit r, g, b,
alpha** — seven slots, and a triangle entering `tri_*` must fill all of them.

**Terrain's side (I13):**

| slot | terrain's producer | state |
|---|---|---|
| screen x/y, behind, src_id, view | `proj_out_*` | present |
| `invw24` | needs a DEPTHQUANT on `proj_out_aw/bw/cw` | the composed one is inside GEOM.VATTR on the geometry lane's tagged schedule |
| **u/w, v/w** | — | **NO PRODUCER ANYWHERE** |
| **lit r, g, b** | `terr_light_base_o` | **one signed 32-bit SCALAR shade, not three channels** |
| alpha | R48's named constant | not a gap |

Searched `fpga/rtl/terrain/**` for `u_over_w`, `v_over_w`, `out_u_o`, `tex_u`:
**zero hits.** The projector carries terrain's `mat_a`/`mat_b`/`weight` — the
Mosaic layer-E triple — which names **which** materials blend, not **where** on
them to sample. **A terrain texture-coordinate law does not exist in this tree.**

**The forge's side, and this is the sharper form.** FORGE.SHADOW's Route A would
inherit `zhao_geom_vattr`'s `done_o`, a six-term AND including
`lit_ord_q == uv_ord_q`: **every vertex needs a colour from GEOM.LIGHT and a
u/v from GEOM.VDECODE. A shadow hull has neither.** `va_done` gates *both* sides
of the GROUP_SEQ → REPLAY handshake, so the batch never releases its arena,
`GEOM_ARENAS` exhausts, and **the whole front end wedges with no timeout, no
abort and no counter.** The tell would be `colours_written_o` frozen while
`uv_staged_o` and `landings_o` climb — **and nothing differences them.** That is
CLAUDE.md's alive-at-zero-CPU shape, in silicon.

**And note what zeroing would do.** `u/w = v/w = 0` is not a neutral value: it
samples **texel (0,0) on every primitive**. A polygon particle or a shadow hull
has a flat colour and no texture coordinates *by law*, so the honest packet is
not "fill the slots", it is "the slot is legitimately absent and the consumer
must know".

## The cost of each option

Unpriced, and that is itself the finding — **the question has never been put, so
nobody has costed the answers.** The shapes differ sharply:

| option | shape |
|---|---|
| **A sanctioned "no texture coordinates" profile** | A flag or a class on the packet; consumers branch. Cheapest, and it is what the hardware already wants — FORGE.SHADOW Route B's design note says *"u/v unused"* in as many words. |
| **Every producer must synthesise u/v** | Forces a terrain texture-coordinate law (art content) and an invented u/v for shadow hulls and particles. Expensive and dishonest. |
| **A separate untextured path into the rasteriser** | A fourth lane and a second door. Largest. |

## Recommendation

**Ratify a sanctioned untextured profile** — one sentence saying a primitive may
declare u/w and v/w absent, and that the sampler is bypassed rather than fed
zero. The reasoning is that **the hardware has already chosen this three times
independently** and each time a lane stopped rather than invent it: terrain has
no coordinate law, the shadow hull authors its own rgb and alpha and uses no u/v,
and the particle case is the same shape. A ruling here is recognising what three
blocks already are, not designing something new.

**What it does NOT settle**, stated so the ruling is not over-read: terrain still
needs a **texture-coordinate law** if terrain is ever to be textured, and a
**scalar-shade-to-lit-rgb law** either way — both carry art content and both are
separately yours (decision 5). This ruling unblocks the *untextured* producers
only.

---

# 5. TERRAIN'S TWO ABSENT LAWS — also never raised as decisions

## The question

**Two sentences, both with art content, neither written anywhere:**

1. **Where on a material does a terrain triangle sample?** (the u/w, v/w law)
2. **How does one scalar shade become lit r, g, b?** (`terr_light_base_o` is one
   signed 32-bit scalar; the packet wants three channels)

## What it unblocks

**tie-off I13**, completely. I13's remaining half (a) — the two-producer
triangle merge into GEOM.CLIP — is *"a day's work once (b) exists, and (b) is
not"*, in the core entry's own words.

## The evidence already gathered

The core's I13 entry states the conclusion precisely:

> *"So this entry is blocked on **TWO ABSENT LAWS and not on one absent wire**,
> and both of them are TERRAIN-lane laws **with art content**."*

Turning a scalar shade into lit rgb needs the material's colour, **which is the
same missing binding `tri_flat_request_i` waits on** — so law 2 is entangled
with the texture lane's I49 and should not be ruled in isolation.

## Recommendation

**Do not rule these yet, and that is the recommendation.** They are listed here
because they were never written down as decisions and would otherwise be
rediscovered a seventh time — not because they are ripe. Under decision 4's
ruling, terrain triangles can enter GEOM.CLIP untextured and I13's merge becomes
buildable; these two laws are then about making terrain *look right*, which is
an art pass with a render, not a gap-closing packet.

**The honest statement is that I13 has been mis-scheduled**: six passes have
treated it as wiring, and it is two art laws wearing a tie-off's clothes.

---

# 6. THE DEVIATION STORE — 185 M10K, 33% of the device's memory

## The question

`zhao_terrain_devstore` holds TERRAIN.LOD's deviations and history. **Do you
spend 185 of 553 M10K on it?**

## What it unblocks

* **tie-off I21** (TERRAIN.GROUP_SEQ's subpatch job port);
* **`zhao_terrain_lod`**;
* the `terr_chk_*` half of **tie-off I27** — the store keys by slot, so
  whoever composes it **owes I27's staleness check in the same commit**, because
  a slot evicted mid-walk would file page A's deviations under page B.

## The evidence already gathered

**R59 priced this at ~77 M10K (14%) and decided against that number. The real
figure is 2.4x higher.** Corrected 2026-09-20 (terrain7) against the block's own
localparams rather than its prose:

R59's "~786 kbit" is `1,024 x 16 x 3 x **16**` — **a 16-bit deviation where
`DEVW` is 24, and no history counted at all.**

```
naive (both surfaces)    2 x 141 + 44 = 326 M10K  (59%)
taken (top only, bit-id)     141 + 44 = 185 M10K  (33%)
R59's stated premise                  =  ~77 M10K  (14%)
```

TERRCOMP's sentence is the one that matters:
**"14% is affordable; 33% is an argument — which is why nobody audited it."**

The same stale record produced three further wrong numbers, all corrected: the
"obvious array" row is **176 M10K not 144**; the 17-bit packing saves **33 not
29%**; the SDRAM form is **176 B/patch, 176 KB, 44 KB/frame — not 144 B, 147 KB,
36 KB.**

## The cost of each option

| option | cost |
|---|---|
| **Compose the store** | **185 / 553 M10K = 33%.** And note *the block's only reader is TERRAIN.LOD*, which the rest of I21 still refuses — the BUILT-INSTALLED-NOWHERE shape. |
| **Do not** | I21 and `zhao_terrain_lod` stay open. |

M10K is the one budget with slack (ALM ~113%, DSP ~135%), and you have ruled in
this currency before — `OWNER-RULING-M10K-CEILINGS-20260918.md`: *"Using some
more M10K is fine, we have enough, particularly if it saves ALMs. They're our
only weapon against our massive ALM debt."*

**But the lever does not apply here.** Your rule is
lookup-for-computation. This store **relocates state**; it buys no ALMs. Same
caveat R120 attaches to the HUD store.

## Recommendation

**Do not decide this before the fit** (decision 12). 33% of device memory is an
argument you should have with a map in front of you, and there is currently no
honest M10K occupancy figure for the console. Composing it today also buys
nothing, because TERRAIN.LOD has five other live blockers behind it.

What this entry does is stop the number being **discovered inside a composition
packet**, which is how R59's 14% came to be decided against a figure that was
wrong by 2.4x.

---

# 7. THE TERRAIN JOB PORT — is a player mask a view mask?

## The question, and it is the shortest on the board

The compose door's view mask is **8 bits** (T5's per-player tag).
`zhao_terrain_group_seq.job_view_mask_i` is **2 bits**, documented *"bit v =
project into view v"*. **Are they the same thing?**

## What it unblocks

**tie-off I21**, and with it `zhao_terrain_lod`'s consumer.

## The evidence already gathered

The chain is composed end to end: T5's `SubmitTerrainSet` carries a per-record
view mask, `zhao_terrain_cmd` emits `rec_view_mask_o`, `zhao_terrain_seq` carries
it as `is_view_mask_o`, and it lands in the core as `tis_view_mask` — **declared,
and waived as unconsumed at the declaration.** So the producer is not absent;
the consumer is.

The core entry refuses to narrow it itself, and the reason is the whole point:

> *"Those are a PLAYER mask and a PROJECTOR VIEW mask. On a two-view machine
> they very probably coincide, and **'very probably coincide' is exactly the
> reasoning that produces a hidden adapter**, so the narrowing is named here and
> not performed."*

## The cost of each option

Near zero either way — this is a sentence, not a build. The cost of getting it
**wrong** is a hidden adapter in the composer, which is the specific failure the
whole tie-off ledger exists to prevent.

## Recommendation

**Rule it explicitly, whichever way.** If a two-view machine means player *p*
maps to view *p* for `p < 2`, say so and the narrowing is legal. If players and
views are independent, the job port needs a reconciliation block and I21 grows.
**Either answer is cheap; the absence is what costs, and it has cost six passes.**

*Note the three remaining fields — `job_mat_a`, `job_mat_b`, `job_weight` — are
NOT this question. Their owner is genuinely unidentified, and R13 has already
provisionally answered where they join: per triangle, by the triangle's cell.
See SPENT.*

---

# 8. THE CREATURE ABI FREEZE (I29) — author the bytes, and price the store first

## The question

R90 granted a second partial lift of the kind-8 / kind-9 freeze. **Two things
follow that R90 does not cover: who authors and freezes the byte layout, and is
a ~17.6 kbit asynchronous-read bone store affordable at ~113% ALM?**

## What it unblocks

**tie-off I29** (`geom_pose_start_i`, `geom_pose_bone_*`, `geom_pose_quat_*`,
`geom_pose_inv_rest_i`, `geom_pose_root_d*`).

**And note it moves the register either way** — if you prefer to hold the
freeze, I29 becomes a Phase-12 item rather than a gap this campaign can close,
and the register should be told so.

## The evidence already gathered

**The lift does not unblock a layout, because there is no layout to unblock:**

* `zref_creature.hpp` holds C++ structs whose members are `std::vector` — **no
  wire layout at all**;
* `spec/creature_rules.md:58-60` holds a size **in prose** (*8 B/bone/frame, 32
  bones ⇒ ≤ 268 B/frame*), with no field order and no alignment;
* `zref_creature_page.hpp:23-28` **explicitly disclaims** freezing them;
* `zref_creature.hpp:43-46` calls the quaternion lane format **"PROPOSED, NOT
  FROZEN"**.

**And the producer is an unpriced asynchronous-read store.**
`zhao_geom_pose_decode.sv:89-92` makes the source fetch **combinational by
contract** and `:146` drives `bone_idx_o` combinationally, so per bone the caller
owes **5 + 3x32 + 4x16 + 12x32 = 549 bits**, held stable for every cycle the
block sits in `S_FETCH`, at `MAX_BONES = 32` — **~17,568 bits a synchronous M10K
cannot serve.** The same file's header spends **thirteen cycles of latency** to
keep 12,288 registers out of ALMs for its ancestor store, and says a
combinational read *"pushes Quartus into logic cells and the block stops
fitting"*.

**Struck, so nobody builds one:** the page path is **not** the obstacle.
`zhao_geom_ladderbank.sv:81` already carries `PAGE_KIND = 8'd8` and walks a
creature-form page as 64-byte lines, and composing a reader adds **no
console-core boundary port** (the memory adapter's requesters A–E are full, so a
sixth is an in-core edit). The recorded "there is no behavioural SDRAM model"
blocker is also false — `sim/models/zhao_sdram_model.sv` is 219 lines with a poke
backdoor, already instantiated against `zhao_shell_top_v2`. That correction was
written **1,400 lines above I29 in the same file** and the entry never read it.

## The cost of each option

**Unpriced, and that is the point.** On a device at ~113% of its ALM ceiling, a
17.6 kbit store forced into logic cells is an owner-visible spend, not a
packaging step.

## Recommendation

**Grant the lift as R90 says, and attach the ALM pricing as a precondition of
building** — so the packet cannot discover the price halfway through. The format
already anticipates the lift: §4c's `body_off` *"names the byte offset at which
that body will begin, so the unfrozen half can be appended later without moving a
byte of the frozen half."*

**If the pricing refuses it, what has to move is the decoder's combinational
contract** — a larger decision than the freeze, and *"not one to discover
halfway through a packet."*

Schedule I29 as its own packet with four named items: author and freeze the body
section plus a minimal kind-9 frame with a zref model; emit a non-zero
`body_off` from `tools/pack/mkcreatureladder.py`; compose the page reader as the
sixth adapter requester; **price the async store in ALMs before building it.**

---

# 9. A FORGE PROGRAM PAGE KIND — R108 left this open explicitly

## The question

**Freeze a forge program page kind in `spec/cartridge.md` §4?**

## What it unblocks

**`zhao_forge_prim`**, and partially `zhao_forge_prim_eval`.

**Be clear about the ceiling**, because it decides whether this is worth doing
now: `zhao_forge_prim_eval` is the **LIGHTNING evaluator, RIBBON family only**.
So even with a page kind and the enum, **four of the six forge families have no
evaluator at all.** A page ruling buys one of six.

## The evidence already gathered

* **There is no forge program page kind.** Every kind in `spec/cartridge.md` §4
  was read: 0x0001 ABI_INFO through 0x0011 SPECIES_TABLE, plus the RESOURCE_PAGES
  families (0 field programs … 4 **terrain patch, a heightfield**, … 9 clip
  bank). **None is a forge program page.**
* **`handle32[forge_program]` occurs in exactly two places tree-wide** — one
  command and its generated ABI table.
* `j_family` / `j_segments` / `j_sides` across all of `fpga/rtl` including
  `synth/`: only the two blocks' own inputs and **the LFSR noise source in the
  generated pricing top**, which is stimulus, not a producer.

**R108 is the ABI half and it is SPENT** — five additive `forge_kind` members
were granted on 2026-09-20, and the core's "EXACTLY ONE member" sentence has been
corrected. R108 says so itself: *"It closes no gap on its own … A forge page kind
must still be frozen, and four of the six families have no evaluator. The ABI
stops being the blocker; it does not become the implementation."*

## The cost of each option

An ABI addition of the ordinary kind: `spec/cartridge.md` §4, the packer, zref
and the captures move together. **The fare is already paid** — R108 established
that any `spec/commands.zidl` edit forces all five golden captures to regenerate
(`demo_duo_markers --write` alone is 600 Duo frames, about an hour), and that
regeneration was bought once in the R108 bundle. A cartridge-side kind is a
separate file, so check whether the same bundle covers it before scheduling.

## Recommendation

**Defer until after the fit**, and this is the one place where the
recommendation is "not yet" for a reason other than cost. The page kind buys
**one family of six**, and FORGE.SHADOW — the forge work with real gameplay
weight — is under your own standing instruction R133 not to schedule further
wiring packets. Freezing a page layout to unblock a sixth of a subsystem you have
paused is the wrong order.

**If you would rather it land now, it is cheap and the analysis is complete.**
Say so and it is one packet.

---

# 10. VERTEX ALPHA — flat per-primitive, or a fourth interpolated plane?

## The question

The console has **no vertex alpha**, and two ratified statements disagree about
whether it needs one. **Does transparency arrive as a flat per-primitive value,
or as an interpolated per-vertex plane?**

## What it unblocks

`zhao_forge_shadow` (behind R133), and transparency for **PART.EXPAND and the
compositor** — which is why it outlives the shadow question.

## The evidence already gathered

* `zhao_raster_blend` **is real and composed, six instances**. *"The blend ALU is
  NOT the missing piece."*
* `zhao_geom_vattr` injects `ALPHA_C = fx16 1.0` (opaque) and its own header says
  **"ALPHA HAS NO PRODUCER"**.
* **`zhao_geom_attrpack` emits exactly three planes and the rasteriser has
  exactly three lanes** — so of the seven ratified attribute slots, **slots 3..6
  (lit r, g, b and alpha) have no interpolator and no carriage at all.** This is
  decision 4's wall seen from the other side.
* R48 fixes material alpha at `8'hFF` reasoning *"no ratified vertex format
  carries alpha"*; FORGE.SHADOW's contract requires *"ordinary TRANSPARENT
  geometry"*. **R48 has since been amended (2026-09-20, geomseam) and the
  contradiction is closed** in FORGE.SHADOW's new section: `zhao_forge_shadow.sv`
  latches `vtx_alpha_o = strength_q` **per caster**, so shadow alpha is a flat
  per-primitive value and never wanted the per-vertex slot R48 governs.

**So the shadow half of this question is answered.** What remains open is the
general carriage, and it is worth one sentence because PART.EXPAND and the
compositor both want it.

## The cost of each option

| option | cost |
|---|---|
| **Flat per-primitive** | Give `tri_continuation_tail_i`'s existing flat `vertex_alpha` a producer, feeding the already-composed blend ALU. **Small**, and R48 stays true. |
| **Interpolated per-vertex** | A **fourth attrpack lane and a fourth rasteriser lane**. A real feature with real area. |

## Recommendation

**Flat per-primitive for v1, and commission interpolated alpha separately if you
want it.** The forge lane's reasoning is right and generalises: alpha is constant
over a shadow hull, constant over a particle, and constant over a composited
sprite. **Interpolated per-vertex alpha is a real feature and should be
commissioned as one, not smuggled in as part of closing a shadow gap.**

---

# 11. THE FIELD LANE WIDTH (FH11) — the largest single area decision open

## The question

FH11 asks for wide useful lanes in the shared FIELD fabric. The console composes
`.FAB_LANES(1)` / `.FAB_GROUP_PTS(1)` with a written argument. **Do you buy the
width?**

## What it unblocks

FIELD throughput, and therefore `zhao_geom_warp` and tie-off I34 **indirectly**.
It closes no gap by itself.

## The evidence already gathered

`reports/FIELD-REPAIR-PLAN-20260920.md` §5 C2 flags it for you rather than
resolving it:

* the reversal is priced at **"about +6,000 ALM on this axis alone"** and
  **"roughly +12 DSP"**;
* the directive's §17.3 concedes a faster correct machine can cost more;
* **the part the directive does not have:** the console is already **5,672 ALM
  and 39 DSP over**, and — the instrument caveat that decides it —
  **`zhao_block_fit.json`'s `zhao_console_core` row does not contain FIELD at
  all.** Its `.sources.sha256` lists exactly one field file. **So every FIELD
  area number is additive to a budget that has never measured FIELD.**

For scale: **+6,000 ALM is 14.3% of the whole device**, and slightly more than
the entire `zhao_forge_cliff` saving (5,698 ALM) just banked.

## The cost of each option

| option | ALM | DSP |
|---|---|---|
| Keep `FAB_LANES=1` | 0 | 0 |
| Adopt FH11's width | **~+6,000** on a device 5,672 over | **~+12** on a budget 39 over |

## Recommendation

**Adopt FH11's SEMANTICS now and defer the WIDTH to a fit** — the repair plan's
own recommendation, and it separates two things travelling together. The
semantics (exact per-point status, no padding contamination) are a correctness
property and cost nothing. **The width is a purchase, and it should not be made
against a budget row that does not contain the subsystem being widened.**

This is decision 12 in miniature: the number needed to answer it does not exist.

---

# 12. THE SCOPING CALL — fit now with a declared remainder, or hold for zero?

## The question

**Run the fit now against an itemised remainder, or hold it until the register
reaches zero?**

## What it unblocks

**No gap.** It governs how the remaining effort is spent, and **three of the
decisions above (6, 8, 11) are currently unanswerable because nobody knows where
the 113% / 135% overage lives.**

## The evidence already gathered

R135, measured across the day's lanes: the remaining gaps are **not twenty-one
wires.**

* **FORGE.SHADOW + GEOM.LODSTATE are MUTUALLY blocked** and compose only
  together, as part of a subsystem including the governor, ladderbank, Route B
  and a ratified-law re-authoring (R133).
* **MEASURE.GOVERNOR is blocked at BOTH ends**, and **no core boundary port
  exists that a governor output could replace** — so composing it **creates**
  gaps (R118). The register would get *worse* while the diff looked constructive.
* **GEOM.PARAMBUF's arena is unmapped in both directions**, needing an allocator,
  a quota seal and a frame-fault path, all unbuilt (R131). Its refusal
  *strengthened* on re-ask: `spec/memory_rules.md` §5f makes RENDER.ASSET_POOL
  read-only under formal assertion `a1_render_asset_ro`.
* **`zhao_terrain_lod` alone would create roughly TWENTY-TWO tie-offs** if
  composed today, and all four terrain blocks together **"would have read
  21 → 17 and buried roughly fifty undeclared tie-offs — available on any
  afternoon, and the campaign's single largest act of self-deception."**
* **GEOM.WARP waits on nine FIELD prerequisites** (R103), eight supplied by the
  directive and one (P5) supplied by a weaker mechanism than asked for.

Against that, **the fit preconditions are otherwise all met.** `superseded check:
72 production roots CLEAN`; six of seven items in `FIT-PLAN-AT-ZERO.md` are
satisfied; **the one unmet precondition is the register itself.** Both structural
gates run clean at this commit — `check_prod_manifest.py` (354 modules, 71 tops)
and `check_console_inventory.py` (354 declared, 213 elaborated, 218 fit sources).

## The cost of each option

| option | cost |
|---|---|
| **Fit now, declared remainder** | The receipt describes a machine missing named function — honest **only if the remainder is named in the receipt, not rounded away.** Buys the ALM/DSP map that decisions 6, 8 and 11 are currently guessing at. |
| **Hold for zero** | Spends the remaining effort on subsystem builds and art calls before anyone knows *where* the overage lives. R80's point stands: **a refusal is not a map.** |

## Recommendation

**Fit now, with the remainder itemised** — R135's own recommendation, not a new
one. `FIT-PLAN-AT-ZERO.md` already names the runs and the question each answers:
**F-CONSOLE-TARGET** (does it place inside 41,910 / 112 / 553?) and
**F-CONSOLE-SIZE** on the larger part (if not, by how much and *where*?).

**R135's caveat is kept and is not decoration.** This is not lowering the bar.
Seven lanes independently refused a dishonest register drop today, each with a
measured blocker; a decision to fit early must not retroactively read as
permission for the move they declined.

---

# 13. TWO LOW-STAKES ITEMS, LISTED SO THEY ARE NOT MISTAKEN FOR BLOCKERS

**Neither blocks anything. Both are one line. Clear them in the same sitting or
ignore them.**

**`OUT_LANES` means two different things one line apart (R182).**
`zhao_field_warp_adapter.OUT_LANES` sizes an **ordinal**-indexed port, so a bench
must pass `.OUT_LANES(W_ORDINALS)` while the host one line away takes
`.OUT_ORDINALS(W_ORDINALS)` **and** `.OUT_LANES(HOST_WINDOW)`.
`tb_warp_field_chain.sv` already calls this *"the sharpest edge in the whole
composition"*, and **it is R168's root cause, still live.** Renaming it to
`OUT_ORDINALS` touches `prod_manifest.yml` and was judged too risky before the
fit. **Recommendation: rename it immediately after the fit, not before.** You may
disagree; the risk is a regenerated manifest, not a design change.

**`prod_fit_sources.txt` has a measured 4-in-4 misread rate.** Its first two
lines say *"ORPHANED … NOTHING READS THIS FILE … do not draw conclusions from
it"*, and it has been misread four times out of four — including **by ruling R86,
where it is load-bearing.** The mechanism: *"the warning is at line 1 and the
evidence is at line 121, and nobody scrolls up."*
**Recommendation: rename it to `prod_fit_sources.ORPHANED.txt`** so every future
search result carries the warning in the path, **and re-examine R86**, which
currently rests on a file that says not to rest anything on it.

---

# SPENT — items from HANDOVER §8/§13 and this packet's brief that are DEAD

**Nine struck. Five of them arrived in this packet's brief as live decisions.**
Each was re-measured, not assumed.

### `{handle → hash}` has a hardware producer — STRUCK

**The brief said: "zero hits tree-wide, re-checked. Recommended: I34's BIND post
kind, `post_op_i` `2'd3`, unspent." Both halves are now false.**

* **`post_op_i 2'd3` is SPENT.** `zhao_field_doorbell.sv` reads *"0 = LOAD WORD,
  1 = COMMIT, 2 = LOOKUP, **3 = FH2. All four encodings are now DEFINED; there is
  no catch-all.**"* Op 3's sub-decode is `0 INSTALL_CAPSULE, **1 BIND_PROGRAM**,
  2 CONTROL, 3 reserved` — **the recommended BIND is built.**
* **The mapping is published.** `zhao_field_loader.sv` holds
  `obj_handle32[]` and `obj_prog_hash[]` and exports
  `pub_handle_o`/`pub_prog_hash_o`/`pub_gen_o` as an indexed read on `pub_sel_i`.
  Its own comment says the mux *"costs one mux **the descriptor table needs
  anyway**"* — it was built for this consumer.
* **It is composed.** `zhao_field_loader` is instantiated in
  `zhao_console_core.sv:16612`, and `fld_ldr_pub_prog_hash_o` leaves the core.

**Why everyone kept re-asserting it:** the searched strings were
`program_hash|prog_hash|programHash`, and **`pub_prog_hash_o` matches
`prog_hash`.** The search was correct when written and the loader landed the same
day. R165 flagged exactly this — *"Re-measure before re-quoting"* — and the core
entry re-asserted it afterwards anyway. **What remains is CMD.EXEC's TerrainField
arm walking `pub_sel_i` to match a handle: engineering, not a decision.**

### The cliff decision — STRUCK, made AND executed

R109 (*"neither may be composed until ruled"*) was superseded by R117: **F-CLIFF1
had already run, on 18 September, as a full fit on the target part.** R142 then
ruled adopt, and FORGE4 executed it (merged `fa5d82df`).

Verified from primary receipts, both `.sources.sha256` digests matching, same
device `5CSEBA6U23I7`:

| | golden `zhao_forge_cliff` | `zhao_forge_cliff_ram` | delta |
|---|---:|---:|---:|
| fitted ALM | **6,674** | **976** | **−5,698** |
| fitted registers | 4,025 | 939 | −3,086 |

**The campaign's largest area result — 13.6% of the device — and it was a ruling
nobody had executed.** Equivalence re-run rather than quoted: 246 lattices, 752
pages, 0 mismatches.

*Two notes. `zhao_forge_cliff_ram` stays on the list of twelve because
**adoption is not composition** — FORGE.CLIFF still has no page issuer. And
`BUDGET_HEATMAP.md` still lists `zhao_forge_cliff` at 7,664 ALM / 18.3% as its
**number-one optimisation target**: a module the console has decided not to ship.
Use 6,674 (the digest-verified receipt); R184 has the heatmap repair.*

### R65 blocking POST.GATHER — STRUCK as a mis-attribution

R65 gates **I32, `zhao_terrain_bake_v2` and the terrain page format**. It has
**zero citations in POST.GATHER's contract or RTL.** POST.GATHER is held by
**R37**. The two sheets unblock disjoint things and should be judged on their own
merits. *(This packet's brief flagged the caution; it is confirmed.)*

### The governor's `proj` format (R83) — STRUCK, repaired

Terrain7 reported `proj0_i` as `[15:0]` Q8.8 capping at 255.996 and saturating at
**53.13° hfov on Duo** — an ordinary game camera — pegging the LOD ladder at its
finest rung with no visible symptom. **That is repaired.**
`zhao_measure_governor.sv` now declares `PROJW = 20` (Q12.8, ceiling 4095.996,
`STEPS` 33 → 37), **and R98 landed both ports in one commit** so the producer
`zhao_view_projq88` and the consumer moved together. No ABI field was added.

*The saturation table still in the file is the ARGUMENT FOR the repair, not
evidence of an outstanding one.* Its lesson stands: **a format validated by one
worked example sitting at 87% of full scale is not validated.**

### `forge_kind` has exactly one member — STRUCK

**R108 granted five additive members on 2026-09-20**, and the core's
capitalised "EXACTLY ONE" sentence — whose line citation had rotted — is
corrected in place. **Keep the page-kind half (decision 9); the enum half is
dead.**

### R13 / the layer-E join point — STRUCK as already answered

I21's text calls the join *"a contract conflict … and it is the owner's"*.
**R13 answered it provisionally on 2026-09-19:** *"Join PER TRIANGLE, by the
triangle's cell … The job port is not widened to carry a subpatch-uniform value
that is not true."* Under your standing authorisation that ruling is live. What
remains is a **layer-E reader** (page offset 7,622, **zero hits under `fpga/`**)
and changes to TESS and GROUP_SEQ — engineering, not a decision.

### The MEASURE.HISTOGRAM metric (I18) — STRUCK

Ruled **R70** (the terrain page-load LOD deviation), challenged, and
**re-affirmed as R112** on first-hand evidence. I18 is closed.

### `fb_writer_i` → `lease_writer_o` — STRUCK, and it reversed

HOSTDBG recommended the substitution on cost. **R79 declined it on
CORRECTNESS**: a naive `fb_writer_i := v2_lease_writer` admits the blit writer
whenever no lease is live, **widening a framebuffer safety window rather than
narrowing it**. The correct form, if ever taken, is
`fb_writer := v2_lease_valid && v2_lease_writer` with a committed mutant. It
moves no register entry either way.

*Worth keeping: cost was assessed twice before anyone asked whether it was right.*

### The viewport table "is not in the ABI" — STRUCK

I14's refusal asserted a **presence** and then refused on it: *"the id-to-rectangle
table is `spec/video_rules.md`'s."* **There was no such table.** Meanwhile
`zref::render::viewports_of()` has held it since the 2026-08-15 ratification, and
two other sites had derived the same rectangles independently. **The table is now
written into `spec/video_rules.md` §3.2.** What remains is the lowering — CMD.EXEC
indexing it — plus one small open choice recorded as OPEN in §3.2: **which mode
indexes the table**, since §1.1 latches the mode at frame start while a `SetView`
commits immediately. The recommendation on file is *the mode the contract set*,
because indexing with the outgoing mode gives a Duo frame's second view a Z60
rectangle for exactly one frame.

*This one cost the entry a month, and the tell was that the refusal asserted
something existed rather than that something was missing.*

---

# WHAT IS NOT AN OWNER DECISION, SAID PLAINLY

So these are not re-raised as questions you owe an answer to:

* **I20** is engineering. Its exact blocker is MATERIAL.RESOLVE's request issue
  point and response join — entry I49, the texture lane's. Two small items ride
  with it and neither needs a ruling first: `base_rgb` is *"the VERTEX's, not the
  material's"* (an art pick a composer must not make silently), and
  `palette_slot`/`palette_generation` for a CLUT row have no producer — **counted
  on `mat_win_clut_unowned_o`, so the absence is loud at the exact moment it
  matters.** Until then the console samples direct-format pages only, by design.
* **I27** is engineering, and harder than recorded. Its `terr_dm_*` writer was
  believed to be TERRAIN.BAKE; **`zhao_terrain_bake_v2` has no deformation-mark
  port of any kind** — no slot, no generation, no epoch, no dirty bit. *"Bake
  never learns the page identity of the patch it digs."* It needs a third block.
  The `terr_chk_*` half rides with decision 6.
* **`zhao_measure_governor`** needs no decision. R73 and R83 removed its last
  two; R119 ruled its PART.LADDER edge (`p_deg_i[1:0]`, **shift the thresholds,
  do not build a floor — a clamp collapses a population onto one rung**). It is
  now a **composition** of three built, uncomposed modules with no known defect —
  but R118's refusal stands un-withdrawn, and composing it today **creates**
  gaps. It closes when TERRAIN.LOD composes.
* **`zhao_geom_warp`** is built and proven and waits on nine FIELD prerequisites.
  **It may not be closed by tying its Field port off**, and W1 was right to
  refuse. P5 must be reported as **deferred with a measured justification**,
  never as closed.
* **A GEOM.SETUP triangle-door arbiter is the largest uncommissioned lever on the
  board.** `zhao_geom_setup` has **one** triangle arm, fully occupied by
  GEOM.CLIP with no arbiter. One arbiter would serve `zhao_forge_prim`,
  `zhao_forge_prim_eval`, FORGE.CLIFF **and boundary I24** — and
  `zhao_part_expand` is already composed, already emits the right shape, and
  already dangles at the core edge for want of that same door. **No lane has
  commissioned it.** This is a packet to schedule, not a question to answer.

---

# THE ONE THING TO DO FIRST

**Open two PNGs.**

```
reports/terrain-seam-dig/seam_dig_contact.png     (R65 — decision 1)
reports/post-gather-law/gather_law_contact.png    (R37 — decision 3)
```

They are the only two items in this document that **no agent can advance by any
amount of work**, and between them they unblock five queued packets. Everything
else here can wait for the fit; these two have been waiting for you.

Then answer decision 2 (TERRAIN.NORMALMAP) in one sentence — it is the only gap
on the board an owner sentence closes today, and it takes the register to 20.
