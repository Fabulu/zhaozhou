# ARENAID — decision records

Packet **ARENAID**, 2026-09-25. Authority:
`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` §4 (standing, adopted), which
delegates these choices and grants explicit authority to amend record schemas
and to supersede older implementation rulings *provided what is superseded is
stated*.

Format, as the directive requires: **question; chosen option; reason and
alternatives; constraints/cost; code/tests/compatibility consequences.**

---

## D1 — What IS a vertex identity in this console?

**Question.** §4 lists the components an arena vertex id must be assigned from:
source geometry/object generation, draw instance, view, pose/warp state, source
vertex identity, attribute discontinuity. What, concretely, in *this* tree?

**Chosen.** `{ domain, arena, generation, index }` — GEOM.REPLAY's own arena
lookup key, plus the producer domain from `zhao_geom_clipdoor`'s grant.

**Reason.** The console already had this identity and has had it since
GEOM.WCACHE was built; nothing needed inventing. `arena` is a GEOM.GROUP_SEQ
handle opened per meshlet **per visible view**, so it carries view and draw
instance; `generation` separates one use of an arena slot from the next, so the
pair names exactly one (meshlet instance, view) and therefore carries object
generation and pose/warp state — a re-posed instance is a different dispatch;
`index` is the vertex's ordinal among the batch's **decoded** vertices, which
`zhao_geom_vattr`'s header proves is the source vertex identity. An attribute
discontinuity is already a distinct source vertex, hence a distinct index.

**Alternatives rejected.** (a) A position/attribute hash — §4 forbids it in
terms and it answers the wrong question. (b) A new identity minted upstream in
GEOM.ASSEMBLE — a second identity for a thing that already has one, and it would
have to be kept in step with the arena handles, which is the two-operands
failure.

**Cost.** Three port pairs on live blocks (GEOM.REPLAY emits the keys,
GEOM.CLIPDOOR grants them, GEOM.CLIP carries them through the winding flip).

**Consequences.** Two vertices merge **iff** they are the same source vertex of
the same meshlet dispatch in the same view. Two dispatches of the same mesh do
**not** share vertices — correct under §4 ("draw instance" is part of the
identity) and the sharing that R7's arithmetic depends on is within a meshlet
(64 vertices, 126 triangles, 378 references).

---

## D2 — Where is a vertex published: GEOM.REPLAY's output or GEOM.CLIP's?

**Question.** The arena must hold *final* geometry. Which stage is final?

**Chosen.** **GEOM.CLIP's output.**

**Reason.** §4: the arena is "authoritative for the FINAL geometry actually
referenced by the binner and raster consumer". GEOM.CLIP drops primitives, so
publishing at its output publishes only vertices the binner will reference, and
it is where the winding flip has already been applied, so the descriptor names
the corners in the order the raster walks them. It is also the only point where
the forge and particle arms are present — publishing at GEOM.REPLAY's output
would have left two of three producers' geometry out of the arena, which is
"reduced work called equivalent".

**Alternatives rejected.** (a) GEOM.REPLAY's output — one port change instead of
three, but pre-clip, mesh-only, and it leaves I54's pre/post-clip mismatch
exactly where it was. (b) Keep the existing GEOM.ASSEMBLE descriptor tap and add
a vertex tap beside it — two descriptor streams in two namespaces filling one
array.

**Cost.** GEOM.VERTID joins GEOM.CLIP's output fork, so the arena's SDRAM rate
now throttles the raster path. `vid_stall_o` measures it. The existing
descriptor tap already throttled; this adds the vertex records, at an amortised
~0.5 vertex per triangle inside a meshlet rather than 3.

**Consequences.** **Supersedes** the 2026-09-22 composition's choice to tap
`asm_t_*`. The assemble fork is removed and `asm_t_ready` is GEOM.REPLAY's alone
again, which slightly *improves* upstream throughput.

---

## D3 — Who allocates the id?

**Question.** GEOM.VERTID must put a `vertex_id` in a descriptor. Where does the
number come from?

**Chosen.** **From `zhao_geom_paramarena`, with its acceptance.** New ports
`pv_accept_o` / `pv_id_o` / `td_accept_o` / `td_id_o`, driven from the same
expressions the arena's intake arm tests.

**Reason.** The alternative — a dense counter in GEOM.VERTID kept in step with
`n_verts_q` — is CLAUDE.md's *"detector wired to two operands that move
together"* in productive form. Two counters, two enables, and a divergence on
the first record the arena discards, with `verts_written_o` and the producer's
count both still balancing because neither looks at the other. The arena is
already the allocation authority; the contract's ownership table says so.

**Alternatives rejected.** (a) The producer allocates and the arena writes at a
producer-supplied address — moves allocation out of the block that owns it. (b)
The producer mirrors the arena's quota logic — the same two-operands defect with
more surface.

**Cost.** Five outputs on the arena (four plus `seal_fire_o`, see D5), all
combinational off existing terms. Both committed mutants carried the ports
forward.

**Consequences.** There is exactly one vertex counter in the console. A sunk
record yields no id, records no row, and is counted at `vid_sunk_o`.

---

## D4 — The collision-safe mapping

**Question.** §4: *"Use a collision-safe mapping; neither a hash nor a CRC alone
proves equality."*

**Chosen.** A **direct-mapped exact table**, `ARENAS × VSLOTS` rows, addressed by
the whole `{arena, index}` part of the key, storing `{epoch, id16}` with a
per-row valid bit in flops.

**Reason.** The row address *is* the key's index part, not a digest of it, and
the discriminator is stored and compared bit for bit. There is no collision
domain to reason about. A key outside the table is **published unshared and
counted**, never truncated into row 0.

**Alternatives rejected.** (a) A hashed/associative cache — §4 forbids relying
on a digest, and a verification tag would then need the full key stored anyway,
at which point the exact table is cheaper. (b) Storing the raw 8-bit
`generation` as the discriminator — **not exact over a frame**: an arena slot
reused 256 times presents the same generation twice and a leftover row would
read as a **hit**. Hence the epoch.

**Cost.** 4 × 64 = **256 rows × 34 bits** in one M10K-shaped array, plus 256
valid flops and 4 × (18 + 8 + 1) bits of per-arena epoch state. No comparators
beyond one 18-bit equality.

**Consequences.** Failure mode of the epoch scheme, if a future producer ever
broke the ordering assumption, is a **miss** — a duplicate record with its own
id — never a wrong hit. The safe direction is structural, not lucky.

---

## D5 — Mapping lifetime, and the eviction proof

**Question.** §4: *"State the mapping lifetime and prove that eviction/reuse
cannot change a still-referenced identity."*

**Chosen.** **One frame**, cleared on the arena's own `seal_fire_o`.

**Reason.** A seal at the arena is a **request** held pending while the drain
completes. Clearing on `render_frame_begin_i` — the edge that *asks* — would
leave GEOM.VERTID describing frame N+1 while the allocator is still filling
frame N, and a stale id would then name a record index belonging to somebody
else. One event, one enable, both sides. GEOM.VERTID additionally suppresses
`pv_valid_o` / `td_valid_o` and drops the in-flight triangle on the seal clock
(`vid_seal_abort_o`), so the arena's intake arm and its seal arm cannot fire on
the same edge.

**The eviction proof.** A row is overwritten only by a corner with the same
`{arena, index}` and a **different epoch**, i.e. a different identity — same
identity, same row, same epoch is a hit and never a write. A different epoch on
arena *a* means GEOM.GROUP_SEQ re-opened *a*, which it cannot do until
GEOM.REPLAY released the handle, which it does not do until it has emitted every
triangle referencing it; GEOM.CLIPDOOR preserves per-client order and GEOM.CLIP
is in order and creates no vertices. So at GEOM.VERTID's input every triangle of
the earlier use precedes every triangle of the later one, and the
still-referenced identity is never the one evicted.

**Epoch bound.** Every epoch advance forces a miss and therefore a publication;
publications are bounded by the sealed vertex quota, ≤ 65,536. `EPOCH_W = 18`
tops out at 262,143. **A wrap counter is declined** and the reason is arithmetic.

**Consequences.** Tested: case 3 (reopen does not hand out the previous use's
id), case 8 (mid-triangle seal), case 9 (no row survives a seal).

---

## D6 — Clipping lineage

**Question.** §4 requires clipping lineage to be carried explicitly, with
canonical source edge, clip operation and exact shared-edge interpolation.

**Chosen.** **No lineage component**, because this console produces no
clipping-derived vertices. Recorded as a measured absence in
`GEOM.VERTID.md` and in the console's I53 closure ledger.

**Reason.** `zhao_geom_clip`'s own header: *"THE NEAR PLANE IS A WHOLE-PRIMITIVE
REJECTION, NOT A CLIP … GEOM.CLIP never produces more than one triangle for one
triangle in."* It drops and it normalises winding. It has no divider and no
vertex FIFO to interpolate with; the near-plane law is the documented Phase-3
model (`spec/sky_and_beams.md` §1.2, `reference/src/zrender/rast.cpp`).

**Alternatives rejected.** Adding a lineage field "for when a clipper exists" —
a field that can never be set is a lie that reads like evidence, and it would
widen the key everywhere for nothing.

**Consequences.** If a Sutherland–Hodgman clipper is ever built, the lineage
becomes a fourth key component and `zhao_geom_vertid.sv` is the file it is added
to. This is written in the block header and the contract so the next person does
not have to re-derive that the field is missing on purpose.

---

## D7 — The `ProjectedVertex` status byte

**Question.** R7's record carries "invw24 + status byte" and nothing in the
repository ever said what the byte holds.

**Chosen**, under §4's explicit authority to amend record schemas:

    [1:0]  domain           0 MESH, 1 FORGE, 2 PARTICLE, 3 reserved (illegal)
    [2]    untextured       R197's per-primitive declaration at first publication
    [3]    shared_capable   published under a dedupable identity
    [7:4]  reserved, written 0; nonzero is a malformed record

**Reason.** Each field is per-vertex, knowable at publication, and answers a
question a consumer has. `domain` says which producer owns the vertex;
`untextured` tells the one block that reads `u_over_w`/`v_over_w` not to;
`shared_capable` separates "a shared vertex" from "one corner's own vertex",
which is exactly what `vid_index_oob_o` and the unshared domains produce.

**Rejected for the byte:** the near-plane `behind` bit. Post-clip it is zero for
every accepted triangle by construction.

**Declined counter.** An untextured-conflict counter. The declaration is per
producer arm (`GEOM_REPLAY_UNTEX_DECL` is a constant for the only arm with
shareable identities) and identities never span arms, so its two operands would
be the same constant — the shape this repository has been caught building four
times. **Declined at design time, not explained at review time.**

---

## D8 — The colour quantisation and the byte order

**Question.** Lit channels are fx16 in 32-bit slots; `rgba8` is a u32.

**Chosen.** `zref::unit8_from_fx16` — negative → 0, above `0xFFFF` → 255,
otherwise `(v + 128) >> 8` with a 255 rail — and `rgba8 = { a, b, g, r }` with r
in the low byte.

**Reason.** The conversion is **published law**, including the Review C2 clamp
that stops a ~1.0 weight wrapping to 0; inventing a rounding rule here would be
a second implementation of ratified arithmetic. The byte order is LSB-first like
every other packing in this subsystem, and is now **declared** rather than
implied.

**Consequences.** `geom_vertid_directed` case 2 differences every channel
against an independent transcription of `unit8_from_fx16`, and asserts the
`0xFF80` rail explicitly in both the model and the RTL.

---

## D9 — R7's 65,536-vertex tier

**Question.** §4: *"Retain R7's intended 65,536-vertex capacity … a COUNT of
65,536 requires a wider internal count/limit. Use at least 17 bits."*

**Chosen.** `td_sealed_vertices_i` → **u18**; `zhao_geom_paramwalk`'s
`w_verts_q` → u18 with its `0xFFFF` **saturation removed**; `MAX_VERTS` →
**65,536**.

**Reason and what is superseded.** `GEOM.PARAMBUF.md`'s section *"One declared
divergence from the tier table above"* and `zhao_geom_paramarena`'s matching
header paragraph both recorded the lost vertex as permanent, reasoning that
"the seal 65,536 is not expressible in the port the legality rule is tested
against". That was a true statement about a **port width** and a false statement
about the architecture. The allocation side never needed the change —
`n_verts_q`, `q_verts_q` and `publish_verts_o` have been 18 bits all along —
which is why this was a decoder-port defect wearing an allocator's clothes. 18
rather than the required minimum 17, to match the ports it is compared against.

**Consequences.** **The record layout did not move.** A `vertex_id` is still u16
and still names 0..65,535, which is exactly 65,536 ids. Allocation stride and
serialized record size remain separate concepts, and so now do a serialized *id*
and an internal *count*. `GEOM.PARAMBUF.md`'s divergence section is rewritten to
say so.

---

## D10 — The metadata the 16-byte descriptor cannot carry

**Question.** §4: *"Preserve all mandatory colour, alpha, UV/perspective, fog,
cull, material-set, material-record and fragment metadata … If the old compact
records cannot carry the full commissioned function, introduce a versioned
extension or immutable sidecar keyed by the same identity. Do not silently
overload an existing field, truncate a full handle, or replace a missing
attribute with a convenient zero."*

**Chosen.** **Specify** a `TriangleExt` sidecar — versioned, immutable, 32
bytes, keyed by the same `triangle_id` the arena allocates — in
`design/contracts/GEOM.VERTID.md`, and **leave its writer to entry I54**, which
owns triangle serialisation.

**Reason.** The `TriangleDescriptor` is 16 bytes and R7 freezes it. Six
per-primitive quantities present on GEOM.CLIP's beat have nowhere to go:
`material_set` (u32 handle), `material_mode` (2), `frag_state` (u32),
`vertex_alpha` (8), `quality_tier` (8) and the per-primitive `untex` bit.
Building the sidecar's writer here would put two owners on the triangle record;
specifying it here and building it in I54 keeps one.

**What was NOT done, and is not hidden.** No field is overloaded, no handle is
truncated, and nothing missing is written as a convenient zero. `source_id`
remains a genuinely 16-bit id zero-extended to u32 — not a narrowed 32-bit
handle. **The shortfall pre-existed this packet and is not widened by it**; what
changed is that it is now declared, with a specified remedy and a named owner,
instead of being invisible.

---

## D11 — Which counters, and which were declined

**Twelve counters, all fired by `tests/geometry/geom_vertid_directed.cpp` with
legal stimulus at the block's own ports. None owes a mutant** — which is itself
a design choice: the keys, the allocator replies and the seal are **inputs**, so
every fault the block detects is presentable.

`vid_tris_o`, `vid_refs_o`, `vid_published_o`, `vid_reused_o`,
`vid_unshared_o`, `vid_opens_o`, `vid_sunk_o`, `vid_index_oob_o`,
`vid_key_split_o`, `vid_id_unnameable_o`, `vid_seal_abort_o`, `vid_stall_o`.

**Declined:**

* **epoch wrap** — the bound is arithmetic (≤ 65,536 reachable advances against
  2^18), so the counter would be a term nobody could move;
* **untextured conflict** — its two operands would be the same constant (D7);
* **a producer-side vertex count** — that is D3; the whole point is that there
  is only one.

`vid_key_split_o` is worth a line because it is the opposite case: it watches an
assumption about the **producer** (all three corners of a mesh triangle share
`{arena, generation}`) that holds by construction inside GEOM.REPLAY but is not
checkable from inside it. It is unreachable in the composed console and
**reachable at GEOM.VERTID's ports**, so the bench fires it and the console's
zero means something.
