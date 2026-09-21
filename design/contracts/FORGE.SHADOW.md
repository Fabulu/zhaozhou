# Contract — FORGE.SHADOW (Contact shadows, as ordinary geometry)

> Ledger: `design/blocks.yml` · gpu clock · maturity SPECIFIED
> RTL: `fpga/rtl/forge/zhao_forge_shadow.sv` — BUILT and UNIT-TESTED
> (`tests/forge/forge_shadow_directed.cpp`, 39 checks), NOT COMPOSED. The
> "RTL: not built" line that stood here was stale; corrected 2026-09-20 by the
> geomseam packet, which read the file. The ledger's `tests:` rows still say
> `PLANNED -- NOT WRITTEN` and are stale the same way.
> Reference: `zref::forge::shadow_*` — PLANNED AND NOT WRITTEN. Searched
> `reference/` for `shadow_hull` on 2026-09-20: zero hits, so the ledger's
> `reference_model: zref::forge::shadow_hull` names a function nobody wrote.

## Purpose and exclusions

FORGE.SHADOW emits a small terrain-conforming transparent hull under a creature
or object, so it looks **attached to the ground**.

**Written 2026-09-03 from `BORING_3D_FUNDAMENTALS_AUDIT.md` R8.** Full shadow
maps are deliberately absent from this console and that is defensible. What was
missing is any settled replacement for the most basic visual job there is — and
for a game of **floating islands and airborne creatures**, "is that thing
touching the ground or hovering above it" is not a refinement, it is legibility.

The audit's estimate is the reason this is worth doing now: **very small if
done as geometry**, and *"can make more perceptual difference than several
expensive material effects."*

**Exclusions, and they are the whole design:**

* **No shadow map.** No depth pass from the light, no shadow buffer, no second
  view.
* **No new framebuffer and no new raster hardware.** The output is ordinary
  transparent geometry through the main renderer.
* **No shadow unit.** This is a primitive generator, a sibling of
  `FORGE.PRIM`, not a lighting stage.
* **No self-shadowing, no shadows cast onto other creatures, no shadows from
  terrain onto terrain.** Contact only.
* **No occlusion query.** Whether the ground is really below is answered by a
  few height taps, not by visibility.

## The frozen ladder

| range | shadow |
|---|---|
| **near hero** | projected low-poly hull, or an 8–16 vertex ellipse, conformed from a few terrain height taps |
| **near army** | 4–8 vertex blob |
| **mid** | tiny dark splat |
| **far** | none |

The ladder is a **coarseness floor selected by the governor**, exactly as
`PART.LADDER` treats particle representation — the same idea and the same
refusal to let a distant creature spend near-hero geometry.

## Input and output packet layouts

**In**, per shadow caster: `{ world_x, world_z, radius, strength, rung,
src_id }` — where `rung` selects the ladder step and `strength` is a unit8 the
content author owns.

**Terrain height taps in**: the block asks for a small fixed number of heights
around the caster and conforms the hull to them. **Fixed count per rung**, so
the cost is bounded and knowable rather than dependent on terrain roughness.

**Out**: a vertex stream to `GEOM.SETUP`, in a **declared deterministic order**
— the same rule `FORGE.PRIM` obeys, and for the same reason: two orderings
produce the same picture and different capture CRCs.

## Backpressure rules

Ready/valid. It is a background producer: a stalled shadow must never delay a
creature. Under pressure the governor lowers the rung, which is a coarser
shadow rather than a missing one — **a shadow that vanishes is worse than a
crude one**, because the creature appears to take off.

## Memory ownership

None. It reads terrain heights through the ordinary terrain path.

## Q formats and rounding

Positions fx16 world units, as everything else in the geometry path.
`strength` is unit8 (value = raw/256, so 255 is the largest representable and
not 1.0).

## Where the transparency comes from — OWNER RULING R89, and R48 reconciled

**This section exists because two written laws disagreed.** Owner ruling R48
(2026-09-19) fixed the console's vertex alpha at the named constant `ALPHA_C` =
fx16 1.0 (opaque), reasoning *"no ratified vertex format carries alpha, so
nothing is being stubbed"*. This contract's exclusions say the output is
*"ordinary **transparent** geometry through the main renderer"*, and
`zhao_forge_shadow.sv:147` emits a per-vertex `vtx_alpha_o`. A shadow composed
under R48 as written would be a flat **opaque** polygon under every creature —
the two documents contradicted each other, and each was cited on its own.

**R89 resolves it, and both documents now say the same thing.** The resolution
is not a compromise, it is a reading of what this block actually produces:

* `zhao_forge_shadow.sv:295` is `assign vtx_alpha_o = strength_q`, and
  `strength_q` is latched per CASTER. **Alpha is already constant over the
  hull.** The per-vertex port carries a per-primitive quantity.
* So what FORGE.SHADOW needs is a **flat per-triangle alpha**, and the console
  already has carriage for exactly that: `tri_continuation_tail_i`'s
  `vertex_alpha` field, feeding the composed blend ALU (six instances in
  `zhao_raster_fragment.sv:490-510`). Giving that open boundary a producer
  is the whole job.
  **NAME CORRECTED 2026-09-20 (forgeshadow packet), because the old wording
  sends a reader to a grep that returns nothing.** The six instances are
  `zhao_raster_blend_prod` (`:490`, `:493`, `:496`) and `zhao_raster_blend_fin`
  (`:502`, `:505`, `:508`). The line range and the count were exactly right;
  the MODULE NAME was not. **`zhao_raster_blend` itself is instantiated
  nowhere in `fpga/`** — it is the unsplit reference wrapper, kept so the
  formal proof targets shipping logic (`zhao_raster_fragment.sv:358`). Only
  the `_prod` half carries the alpha port, and it is called `a_i`.
* **R48 therefore stands, unamended and true.** No ratified vertex format
  carries alpha and none is being invented here. The alternative — a fourth
  `zhao_geom_attrpack` plane AND a fourth `zhao_raster_tile_pipe_v2` lane, for
  a value that does not vary across the primitive — is a real feature
  (interpolated per-vertex alpha) that should be commissioned as one, not
  smuggled in as part of closing a shadow gap.

**So this contract's `vtx_alpha_o` is hereby declared PER-PRIMITIVE, not
per-vertex.** It is emitted once per caster and every vertex of that caster's
hull carries the same value; a consumer is entitled to sample it once.
Interpolating it is not required and, until the fourth attribute lane exists,
not possible.

Recorded in both places by ruling: here, and against R48 in
`reports/OWNER-RULINGS-20260919-EVENING.md`. *A contract corrected in one place
and not the other is how this pair got here.*

## R89 IS DECIDED, THE CONSUMER IS REAL, AND THE VALUE IT WANTS HAS NO PRODUCER

Added 2026-09-20 by the **forgeshadow** packet, which was scheduled by ruling
R132 on the premise that R89 was FORGE.SHADOW's last blocker and therefore
*"the cheapest remaining unlock in the whole run"*. **That premise does not
hold, and this section is the evidence, so the next lane inherits a verified
list instead of a spent one.** R130's own procedural fix is what produced it:
*a lane's closing recommendation is a claim about the tree as that lane saw
it* — so it was re-asked rather than implemented.

### The half of R89 that is TRUE, and is now traced end to end

**The consumer is real, composed, and complete.** The carriage from the tail's
`vertex_alpha` to the blend's alpha port is **ten hops with no constant, no
tie-off and no dangling bit anywhere inside the datapath**:

| # | file:line | what happens |
|---|---|---|
| 1 | `zhao_console_core.sv:5655` | `input logic [47:0] tri_continuation_tail_i` — **the open boundary** |
| 2 | `zhao_shell_top_v2.sv:188`, `:1143` | pass-through |
| 3 | `zhao_geom_bin_pipe_v2.sv:64`, `:236` | packed into the frozen 1157-bit metadata ABI at bits `[345:298]` |
| 4 | `zhao_geom_binner_v2` | stored and drained opaquely |
| 5 | `zhao_raster_tile_pipe_v2.sv:266` | `incoming_continuation_tail_w = job_meta_i[345:298]` |
| 6 | `zhao_raster_tile_pipe_v2.sv:657-658` | cast to `zhao_raster_continuation_tail_v2_t` |
| 7 | `zhao_raster_texture_stage_v3.sv:286` | `frag_vert_a_o = …post_earlyz.vertex_alpha` |
| 8 | `zhao_raster_tile_pipe_v2.sv:868`, `:957` | into `zhao_raster_fragment.frag_vert_a_i` |
| 9 | `zhao_raster_fragment.sv:737, 722, 461, 701` | `s0_va_r → s1_src_a_r → s2_src_a_r` |
| 10 | `zhao_raster_fragment.sv:490/493/496` | `zhao_raster_blend_prod.a_i` — **the consumer** |

The field law is `zhao_render_texture_pkg.sv:44-54`: `vertex_alpha` is bits
**[23:16]** of the 48, asserted by the package's own one-hot span self-test at
`:784-786`. So R89's sentence *"the console already has carriage for exactly
that"* is **correct and now documented rather than asserted**.

### THE CORRECTION THAT MATTERS: `ALPHA_C` IS NOT ON THIS PATH AND NEVER WAS

R48 and R89 are both written as though `ALPHA_C` were the seam that a shadow
alpha would replace. **It is not the same seam.** Searched every occurrence in
the tree:

* `ALPHA_C` is declared **once**, `zhao_geom_vattr.sv:193-194`, as a **32-bit
  fx16** parameter `32'h0001_0000`, and used **once**, `:537`, writing
  `rep_data_o[191:160]` — **per-vertex attribute slot 3**.
* Slot 3 **has no interpolator and no lane**: `zhao_geom_attrpack` emits
  exactly three planes and `zhao_raster_tile_pipe_v2.sv:601` has exactly three
  attribute lanes. `ALPHA_C` is written into a slot nothing reads.
* The blend's `a_i` is an **8-bit unit8** off the continuation tail, by the
  ten-hop chain above. The two never meet.
* A **third** opaque constant, `MAT_BASE_ALPHA_C = 8'hFF`
  (`zhao_console_core.sv:15205`), is the *texture* base alpha in the flat
  request, and is also not `a_i`.

**Three unrelated opaque constants, and none of them drives the blend.** This
does not disturb R89's ruling — flat-alpha is still the right route — but it
does mean the sentence *"`ALPHA_C` remains its named seam"* describes a
different feature (interpolated per-vertex alpha) than the one being produced
here, and a reader who replaces `ALPHA_C` will have changed nothing.

### THE VALUE R89 WANTS TO TRAVERSE HAS NO PRODUCER ANYWHERE

R89 names the producer precisely: *a caster's `strength_q`*. **`strength_q` is
latched from `cast_strength_i`, and `cast_strength_i` has no producer in the
tree, no source in the ladder, and no field in the ABI.**

* **`zhao_geom_lodstate` does not emit it.** Its caster output
  (`zhao_geom_lodstate.sv:188-195`) is exactly
  `{c_valid, c_instance_id, c_x, c_z, c_radius, c_rung, c_view}` — **no
  strength**. The header at `:180-187` is explicit that the block "emits the
  measured quantity and invents no factor".
* **Nothing else produces one.** Searched `strength` across all of `fpga/rtl`:
  every hit is SURFACE.STAMP's u16 (`zhao_cmd_exec.sv:381`), the FIELD stamp
  adapter, `DEBUG_RUMBLE`, `SET_ENVIRONMENT`'s `tint_strength`, or
  `zhao_shell_top_v2.sv:1178`'s `pg_strength_i(8'd0)` — a particle-glue port
  already tied to zero. **None is a shadow strength.**
* **No ratified command carries one.** Searched `spec/` and `design/`: the only
  hits for a shadow strength are this contract's own prose, `:56` and `:99`.

So FORGE.SHADOW's `cast_strength_i` is today **unproduceable**, and composing
the block would make it a **tie-off** — the one thing the campaign forbids. This
is the same shape as `ALPHA_C`'s own origin: a quantity a contract assumes and
no format carries.

### AND R89 WAS NEVER THE ONLY BLOCKER — FOUR REMAIN, ALL VERIFIED FIRST-HAND

R132 scheduled this work believing R89 was the last one. Walking every port
group of `zhao_forge_shadow` (`:106-164`):

| port group | blocker | status |
|---|---|---|
| `tap_*` (`:131-137`) | ~~**NONE — this one is genuinely clear.**~~ **THIS ✅ WAS WRONG (SHADOWSUB, 2026-09-21).** `zhao_terrain_heighttap` does mirror the ports and is composed — and its **single** requester port is **fully occupied by `u_part_terrain_tap`**. A port SHAPE was read as port AVAILABILITY. **The arbiter that answers it is now BUILT AND TESTED** — `zhao_terrain_tapshare`, 97 checks — **and composes with this block.** | 🔧 built, not composed |
| `cast_strength_i` (`:118`) | **no producer anywhere** (above). Owner decision. | ❌ |
| `cast_{x,z,radius,rung,src_id}` (`:115-120`) | `zhao_geom_lodstate`'s `c_*`, and **LODSTATE is not composed**; it needs `zhao_geom_ladderbank`, which needs an ENGINE1 share and a page-publication path. | ❌ |
| `rung_floor_i` (`:125`) | its only producer is the **uncomposed `zhao_measure_governor`** (R118, itself blocked at both ends). Named exactly, so the subsystem packet does not rediscover it: **`deg0_o` / `deg1_o`** (`zhao_measure_governor.sv:314-315`), 2-bit per-camera degradation — the same width and the same meaning as `rung_floor_i`. The governor is instantiated **only** at `zhao_prod_top.sv:2877`, the LFSR census top. | ❌ |
| `vtx_*` (`:142-154`) | **no consumer.** Route A (batch, through `GEOM.GROUP_SEQ`'s `v_*`) **DEADLOCKS** on `zhao_geom_vattr.sv`'s `done_o` — **the citation `:490` is a comment banner; `done_o` is `:553`** (R133 recorded the correction and this table was never updated). Route B (private arena) is unbuilt and needs an arbiter at GEOM.CLIP's door and the absolute→rebased frame conversion. **The "client-A widening" clause is STRUCK: that widening was PERFORMED under R68 sub-build 4 and R3 sanctions it — see the 2026-09-21 section below.** | ❌ |

### The chain is THREE blocks long and none of them is composed

Named end to end, because the shape is what makes it a subsystem rather than a
wiring job — and every link was read first-hand:

```
zhao_measure_governor          zhao_geom_lodstate            zhao_forge_shadow
  cam0/1_thresh_q8_o  ---->  thresh0_i / thresh1_i
  deg0_o / deg1_o  ------------------------------------>  rung_floor_i
                               c_{x,z,radius,rung}  ---->  cast_{x,z,radius,rung}
                                                           cast_strength_i  <- NOTHING
```

`zhao_measure_governor` is instantiated only at `zhao_prod_top.sv:2877`;
`zhao_geom_lodstate` is instantiated **nowhere** in `fpga/rtl/prod/`;
`zhao_geom_ladderbank`, which LODSTATE needs for `a_*`, is instantiated nowhere
at all. All three verified by searching for an instantiation (`^\s*<module>\s+\w+`),
not for a mention — the distinction that made `zhao_forge_shadow` look composed
in `zhao_console_core.sv`, where all four hits are comments.

**A NEAR MISS WORTH NAMING, because the name matches and the quantity does
not.** There *is* a composed governor floor in the console:
`zhao_part_project.sv:332`'s `lad_gov_floor_o`, wired at
`zhao_console_core.sv:12991` to `:13046`'s `p_gov_floor_i`. **It is not a
producer for `rung_floor_i` and must not be wired to one.** It is **3 bits**,
not 2; it is read from the **particle's own attribute record**
(`zhao_part_project.sv:681`, `attr_rd_c[50:48]`), so it is a per-particle stored
floor rather than a camera measurement; and it serves **PART.LADDER's eight-rung
ladder**, a different ladder from the creature one this block's `rung_floor_i`
belongs to. Wiring it would truncate a bit and cross two ladders, and every
gate would stay green.

**LODSTATE and FORGE.SHADOW are MUTUALLY blocked** — `c_*` has no consumer
because FORGE.SHADOW is uncomposed, and `cast_*` has no producer because
LODSTATE is uncomposed. **They can only be composed together, and even then the
other three blockers remain.** So FORGE.SHADOW is a **subsystem packet**, not a
wiring job, and it should not be scheduled as one again.

## THE SUBSYSTEM RE-MEASURED UNDER D2 — SHADOWSUB, 2026-09-21, at `fe1d3ca0`

Owner decision **R234 D2 `(owner, explicit)`** commissioned this as a subsystem
packet, which is what R133 said it needed. **R133 is spent, not overturned, and
it was right that a wiring job cannot close this.** Everything below was
verified by instantiation and by reading the port lists, never by grep on a
mention — the distinction R133 itself established for this cluster.

**The five parked blocks are still uncomposed.** Searched every `.sv` under
`fpga/rtl` for an instantiation at statement position, excluding each module's
own file and every comment:

```
  zhao_geom_ladderbank   0
  zhao_geom_lodstate     0
  zhao_view_projscale    0
  zhao_geom_projradius   1  -- zhao_geom_lodstate.sv only, itself uncomposed
  zhao_measure_governor  1  -- zhao_prod_top.sv only, the GENERATED pricing top
  zhao_forge_shadow      1  -- likewise
```

**No blocker of the five has expired.** Two have MOVED, one of them in the
direction nobody was watching.

### 1. THE CLIENT-A WIDENING WAS ALREADY PERFORMED, AND R3 SANCTIONS IT

**This is the correction that matters, because "re-authors a ratified law" is
the sentence that made this a subsystem rather than a build.**

The ratified law is **owner ruling R3 `(owner, explicit)`** — one of the seven
only the owner can lift:

> *"Third projector port for particles / FORGE.SHADOW (I24). **Keep the
> time-multiplex. No third port in v1.** Owed: **a written schedule proof**
> that geometry, particles and FORGE.SHADOW's instance-centre 1/w share client
> A's bandwidth within the frame at the guaranteed content tier."*

**R3 does not forbid this subsystem's use of client A. It NAMES it**, as one of
the three sharers, and what it withholds is a third *port on
`zhao_project_service`* — not a third *client*.

And the widening has already happened. `zhao_part_project.sv`'s header records
it as a deliberate act under **R68 sub-build 4**, in terms:

> *"WHY THE FIELD IS TWO BITS AND NOT ONE. One bit names two owners, and this
> port has a third coming: **`zhao_geom_lodstate` projects the INSTANCE CENTRE
> through the same client A** so `zhao_geom_projradius` can divide by its `w`.
> **Owner ruling R3 keeps client A a time-multiplex, so the third owner is a
> third arm here and not a second projector. The encoding is sized for it
> NOW**, because the failure mode of sizing it later is silent."*

Verified in the RTL, not taken from the header: `PAY_W = 17` (was 16),
`OWNER_W = 2` (was one bit `TAG_BIT`), `OWNER_GEOM = 2'd0`, `OWNER_PART = 2'd1`,
**`2'd2` and `2'd3` unallocated**, with `owner_unroutable_o` counting a result
that carries one and `geom_tag_collision_o` counting a geometry rider that
arrives with any owner bit set. The console mirrors it: `GEOM_PAY_A_W = 17`,
`GEOM_OWNER_W_C = 2`, and a live `initial` elaboration guard requiring
`GEOM_ARENA_W + GEOM_INDEX_W <= GEOM_PAY_A_W - GEOM_OWNER_W_C` (3 + 12 <= 15).

**So the instance-centre half of Route B needs NO new law and NO owner
decision.** What it needs is:

* **a third request arm on `zhao_part_project`** claiming `OWNER_LOD = 2'd2`.
  The block has exactly two input arms today (`g_*` geometry pass-through,
  `p_*` particles) and no third. This is an edit to a composed, verified block,
  so it costs its whole instantiation chain plus every bench — but it is the
  arrangement R3 and the block's own header both prescribe.
* **the written schedule proof R3 OWES and that has never been produced.** It
  is a measurement, not a decision, and it is the one outstanding obligation of
  an owner-explicit ruling in this cluster. **It has two halves and only one of
  them can be measured today, which is worth stating rather than blurring.**
  The RATE half — does the third client's per-frame demand fit inside client
  A's frame budget at the guaranteed content tier — is answerable now, from
  each client's demand against the service's measured throughput, and should be
  answered BEFORE the arm is designed. The FAIRNESS half — does round-robin at
  three starve anyone, and does `zhao_geom_lodstate`'s single-in-flight FSM
  still close its 200-clock evaluation under contention — **cannot be measured
  without the arm**, so it is owed at the same commit that adds it. Quoting the
  first as though it settled the second is the shape this file's own table just
  got caught in.

**The rider is FULL at 17 bits** (3 arena + 12 index + 2 owner), so a FOURTH
owner fits the field but any additional rider *payload* does not. If Route B's
shadow-hull vertices need their own arena address, that is a
`GEOM_PAY_A_W` widening and the elaboration guard will say so — loudly, which
is what it is for.

**I first wrote here that what would genuinely re-author a law is an
arena-fill path on client A's RESULT port, and that whether shadow hulls get an
arena of their own is a real design question. THAT IS WITHDRAWN. THEY DO NOT
NEED ONE, AND COMPOSED SILICON ALREADY PROVES IT.**

`zhao_part_project` takes its particle results **straight out on `q_*`** —
`q_x_o` / `q_y_o` as `signed [20:0]` canvas coordinates, `q_d_o`, size, colour
— with **no arena anywhere on that path**. The only occurrences of `arena` in
that file are the geometry rider's bit layout and one comment. A client A
client is therefore **not obliged to land in an arena**; it is obliged to carry
an owner in the rider and take its results back on a demux arm. Particles do
exactly that, today, in the composed console.

So a shadow hull's route is the particle's, not terrain's: world vertices into
client A under an owner, screen-space vertices back on a demux arm, a small fan
assembler, and GEOM.CLIP's door. **And the widths already agree** —
`q_x_o`/`q_y_o` are `signed [20:0]` and `zhao_geom_clip`'s `tri_ax_i` is
`signed [20:0]`, which is R188's finding that the clamp is the law and the
extra bit is headroom, arriving where it is needed.

**With that, NOTHING in this subsystem re-authors a ratified law.** R133's
sentence has no surviving referent: the owner-field widening was performed
under R68 sub-build 4 and R3 sanctions it, and the arena path it might have
meant is not required. What remains is entirely engineering.

### 2. `tap_*` IS NOT CLEAR — ONE REQUESTER PORT, ALREADY TAKEN

The table above marked this ✅ on the grounds that `zhao_terrain_heighttap`
"mirrors these ports signal for signal and is already composed". **Both halves
of that sentence are true and the conclusion does not follow.**

`zhao_terrain_heighttap` has **exactly one** requester port group —
`req_valid_i`, `req_ready_o`, `req_x_i`, `req_z_i`, `req_surface_i` — and in
`zhao_console_core.sv` every one of them is connected to `htp_req_*`, which is
driven by `u_part_terrain_tap`'s `tap_req_*_o`. **There is no second port and
no arbitration.**

Worse for a would-be second client: **the response carries no tag and no
rider.** `rsp_valid_o` and its eighteen data outputs arrive with nothing saying
whose request they answer, so an arbiter in front of `req_*` must hold the
outstanding owner itself — the `u_terrain_rdshare` shape. **No such arbiter
exists.**

**And the core already half-knows this.** Above `u_part_terrain_tap` it declares
`htp_height`, `htp_nx`, `htp_ny`, `htp_nz` under `lint_off UNUSEDSIGNAL` and
says why:

> *"The point answer's height and normal are the SERVICE's answer to its
> requester; this requester evaluates its own points from the cell... **FORGE.SHADOW,
> the point answer's other customer, is not composed** (its own blocker, the
> creature rung, is in the FORGE.SHADOW header)."*

So the console names FORGE.SHADOW as the point answer's customer, names a
*different* blocker, and **nobody looked at the requester port**. The point
answer is live silicon that nothing reads — R228's shape exactly — and the ✅
above is why the contention was never measured. **Ask who READS this, not
whether it EXISTS.**

### 3. R197's UNTEXTURED DOOR IS BUILT AND COMPOSED — the u/v half is DISCHARGED

Route A and Route B were both recorded as blocked by an attribute wall that a
shadow hull cannot climb, because it has no `u/v` by law. **Owner ruling R197
sanctioned the declared-untextured profile, and the mechanism has since been
built, composed and given a fired positive control.** Verified first-hand:

* `zhao_geom_clip` carries `tri_untex_i` through three pipeline stages to
  `out_untex_o`; `zhao_geom_attrpack` branches on `tri_untex_i`.
* `zhao_console_core.sv` implements R197 law 3 at GEOM.CLIP's input door —
  `cl_in_untex_c`, `cl_in_refuse_c`, and `geom_untex_refused_o` counting each
  refused triangle. The producer's declaration is the named seam
  `GEOM_REPLAY_UNTEX_DECL`.
* `tests/mutants/zhao_console_core_untex_decl_mutant.sv` is the committed
  positive control, driven by `run_console_core_smoke.ps1 -UntexMutant` with
  inverted polarity, because no legal stimulus can move that counter.

And the core states the intent this contract needs, unprompted:

> *"It is also the door R187 names for every non-mesh producer — 'the honest
> door is at GEOM.CLIP's input' — so **the arbiter that eventually admits
> particles and shadow hulls will present its `untex` bit to THIS gate**, not
> to a second copy of it downstream... When a second producer is arbitrated
> into this door its own bit is muxed here beside its triangle, on the same
> handshake."*

**So the `u/v` half of the vertex-consumer blocker is discharged and the door
is already the right shape.** What remains for `vtx_*` is the **arbiter** at
that door and the **material-window span** — `cl_in_refuse_c` refuses an
untextured primitive whenever `mw_pub_sample_count != 0`, so a shadow hull must
arrive under a published zero-sample material, and the window's occupancy
accounting (R187's three-way ordered join) is what decides that. That is a real
piece of work and it is smaller than the wall this table recorded.

**Route A's stated cause is also wrong on the merits, and the correction does
not rescue it.** `done_o`'s binding term is `(lit_ord_q == uv_ord_q)` — a COUNT
EQUALITY, which a hull supplying neither satisfies trivially — not a
requirement that a lit rgb and a u/v exist. Route A's actual obstacle is that
`zhao_geom_vattr` is fed by the meshlet batch protocol (`batch_i`, `op_valid_i`
arenas, decoded `uv_valid_i`/`lit_valid_i` streams) that a shadow hull has no
producer for. **Route B remains the route; the reason recorded for preferring
it was not the reason.**

### 4. TWO BLOCKERS NOBODY HAD MEASURED, both on GEOM.DRAWJOB's job seam

`zhao_geom_lodstate` taps the DRAWJOB → MESHFETCH handshake for
`{j_instance_id_i, j_form_index_i[23:0], j_cx/cy/cz_i, j_view_i}`. The
handshake is composed and carries **neither of the last two**:

* **No form index.** `zhao_geom_drawjob` emits `j_desc_addr_o [26:0]`,
  `j_format_o [7:0]` (the VERTEX format, not the form), `j_generation_o` and
  `j_stream_base_o`. `zhao_geom_ladderbank`'s key is the **MESH_STREAM handle
  index** — the value that arrives on `upl_publish_index_o [23:0]` and that
  DRAWJOB writes into its residency directory — and **the job does not carry it
  out.** Closing this is a new output on a composed block, and therefore its
  whole instantiation chain plus every bench.
* **No view index, and this one is an OWNER DECISION.** DRAWJOB emits
  `j_active_mask_o [1:0]`, a two-view **mask**. `lodstate`'s `j_view_i` is a
  single bit selecting *which camera's threshold the instance's one ladder is
  measured against*. For `active_mask == 2'b11` there is no honest answer in
  the tree. **`zhao_geom_lodstate`'s own header already docks this** — *"ONE
  LodState PER INSTANCE, NOT PER CAMERA — AND THAT IS A DEVIATION... the
  disagreement is REPORTED rather than resolved here. It is an owner decision,
  and the cost of changing it is one more index bit on the store."* Composition
  is where it stops being reportable: something must choose, and choosing
  silently is inventing.

### 5. LADDERBANK IS A SIXTH ADAPTER REQUESTER, AND THE WRAPPER IS FIXED AT FIVE

`zhao_geom_mem_adapter` takes **no parameters**. It is a fixed A–E wrapper —
`s_req[0..4]` and an inner `zhao_mem_share_n #(.N(5))` — with the five slots
held by MESHFETCH, ASSETFETCH, MATERIAL.RESOLVE, DRAWJOB and PART.TABLE.LOADER.
A sixth requester is `f_*` ports, an `s_req[5]` assign and `.N(6)`; the
round-robin law is bounded at `N-1` turns and must be re-proved at six, not
assumed.

LADDERBANK's trigger is easier than the entry implies: `upl_publish_valid_o`,
`_tag_o`, `_base_o` and `_extent_o` are already core outputs and already feed
`u_part_table_loader` with exactly the four-signal shape LADDERBANK declares.
The kind-dispatch pattern to copy is the port-level `valid && (tag == KIND)`
used for DRAWJOB's directory and MATERIAL.RESOLVE; LADDERBANK's `PAGE_KIND` is
`8'd8`, CREATURE_FORM.

### THE BUILD ORDER, for whoever takes this next

**Nothing in this chain composes alone.** Every link's outputs terminate on the
next link, so composing any prefix dangles an edge and closes a gap by opening
one — the trade R75 endorsed refusing and the one R223 records against the
governor by name. **It is one commit or none.** In dependency order:

1. **Discharge R3's owed schedule proof** (standalone, no composed file
   touched, no tie-off created).
2. `zhao_view_projscale` — pure cfg-bus snoop, 0 DSP, 0 M10K, back-pressures
   nothing. Feeds `zhao_view_projq88` (also uncomposed) and `lodstate`'s
   `kx/vw`.
3. `zhao_measure_starve` (uncomposed) → the governor's `starved0/1_i`.
4. **The governor's upstream** — `px_err0/1_i` and `view_count_i` still need
   CMD.EXEC arms (R118, re-checked here and still true), and the frame pulse is
   `core_tick_c` (`= gpu_tick_o`, FRAMECTL's boundary), which also serves
   `lodstate`'s `frame_i`.
5. **The governor's TERRAIN.LOD output group** — `cam0/1_scale_o`,
   `targets_valid_o`, `cam*_en_o`, `hyst_o`, `min_hold_o`, `morph_step_o`,
   `src_id_o` — has no consumer until `zhao_terrain_lod` composes. **This is
   the trade R223 names and it has not moved.** `zhao_terrain_lodfeed` is
   composed and is NOT it: it is the deviation feed and consumes no governor
   output.
6. The two DRAWJOB seam ports (§4), one of which is an owner decision.
7. `zhao_geom_mem_adapter` at six (§5), then `zhao_geom_ladderbank`.
8. The third client-A arm (§1), then `zhao_geom_lodstate`. **The arm cannot
   land on its own**: adding ports to `zhao_part_project` obliges
   `zhao_console_core` to connect them, and with no LODSTATE to connect them to
   that is a new tie-off — a gap opened to close none. It lands in LODSTATE's
   commit or not at all.
9. **`zhao_terrain_tapshare` IS BUILT** (SHADOWSUB, 2026-09-21) — the arbiter
   §2 says is missing. 97 checks in `terrain_tapshare_directed` plus an
   inverted-polarity positive control for `stray_rsp_o`; `pending_compose` in
   `console_inventory`, `not-yet-adopted` in `prod_manifest`. It is NOT
   composed, because an arbiter in front of a port with one user is cost
   without capability. It composes in the same commit as `zhao_forge_shadow`,
   whose `cast_strength_i` comes from a named constant — **R133's
   D-FORGESHADOW-A, accepted, still the right treatment.**
10. The shadow fan assembler, the GEOM.CLIP-door arbiter and the shadow
    material span (§3). **This is the terminal link and the largest remaining
    build.** It is blocked by NO law: the door exists and carries the `untex`
    bit, the core's own comment says the arbiter that admits shadow hulls
    presents its bit to that gate, and the projection route is the particle's
    `q_*` shape rather than an arena.

**Steps 5 and 6b are decisions, not builds.** They are docked in the findings
with the evidence attached rather than taken here.

### WHAT R3'S SCHEDULE PROOF STILL OWES, and why it is not in this packet

The RATE half needs a bench holding `zhao_part_project` against the real
`zhao_proj_subsystem` with every arm saturated. `tb_part_project` drives the
block **standalone** — its `verilate()` sources are `zhao_part_project.sv` and
`zhao_part_record.sv`, no service and no core — so the composed multi-client
throughput has never been measured, and cannot be from any bench in this tree
until the third arm exists. Quoting `zhao_project_service`'s header figure of
398,784 of 1,666,666 clocks as the answer would be comparing a current design
to an old claim, which is exactly what `CLAUDE.md` says never to do with a
measurement.

**So both halves of R3's proof land with the arm** — which is a smaller
statement than it looks, because the arm, LODSTATE and the proof are one commit
anyway, for the tie-off reason in step 8.

### What was deliberately NOT done, and why

Closing `tri_continuation_tail_i` the way `tri_flat_request_i` was closed
(`zhao_console_core.sv:15228`) was considered and **refused**. That closure is
legitimate because a *majority* of the flat request's fields come from a real
composed producer (`mw_pub_*`, MATERIAL.RESOLVE) and only the unproduceable
ones are named constants with rulings. **The tail has no such producer for any
of its four fields** — `zhao_console_core.sv:15198-15202` says so in terms, that
the vertex colour "is left at its constants deliberately rather than invented".
Building the tail from four constants would be **moving a tie-off from a port
into the core**, which closes a gap on the register while changing nothing in
the silicon. That is the campaign's first prohibition and it is also the
flattering move, which is why it is written down here rather than just avoided.

## Latency (fixed or variable)

`variable` — a height tap takes the terrain path's latency.

## Overflow and malformed-input behaviour

* **A caster with no ground beneath it** — over a void cell, off the island
  edge, or above the keel — emits **no shadow**, and that is correct rather
  than a fault. An airborne creature over a chasm should not have a shadow
  pasted at some default height.
* **A radius of zero** emits nothing.
* **Height taps that disagree wildly** (a cliff edge under the caster) still
  conform: the hull follows them. It may look odd on a knife-edge, and that is
  a content problem, not a hardware refusal.

## THE ONE THING THAT MUST NOT BE GOT WRONG

**Depth bias, and it must be authored rather than accidental.** CLAUDE.md's
ground-contact law applies directly: clipping through the ground must be
authored, never accidental, and *a belly resting at exactly zero reads as
hovering*. A shadow at exactly the terrain height z-fights with it; a shadow
biased too far reads as floating detached from its caster.

So the bias is a **named, editable constant per rung**, and its correctness is
decided **by looking** — a shadow that measures right and looks detached is
wrong.

## Scalar reference function

**PLANNED AND NOT WRITTEN**: `zref::forge::shadow_hull(rung, x, z, radius,
heights[])` returning the vertex list in emission order, and
`zref::forge::shadow_rung(distance, governor_floor)`.

## Directed tests

**PLANNED AND NOT WRITTEN**: each rung's exact vertex and triangle count; the
emission order deterministic and identical across two runs and under stalls; a
caster over a void emitting nothing; the governor floor overriding a near rung;
and the height-tap count fixed per rung regardless of terrain.

## Randomized differential tests

Planned, against the scalar model, over caster positions including island edges
and breach holes.

## Integration capture cases

None on hardware. **And a look-gate, because this is art**: a creature walking
across flat ground, a slope, a cliff edge and a breach, at 240p, watched in
motion. A contact shadow either sells the contact or it does not, and no
measurement decides that.

## Synthesis / resource ceiling

Expected **very small**: a vertex generator with a fixed table per rung, plus
the tap request logic. It has no arithmetic beyond placing vertices. The audit's
own assessment: *"may cost essentially zero new raster hardware."*

## Notes

The material architecture already mentions precise shadows as terrain-conforming
polygons; this contract is the **automatic, per-creature** case that had no
rule. The two are compatible: an authored precise shadow is content, this is
the default every creature gets for free.
