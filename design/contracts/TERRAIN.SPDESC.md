# TERRAIN.SPDESC — the subpatch descriptor assembler

`fpga/rtl/terrain/zhao_terrain_spdesc.sv` · tests `tests/terrain/terrain_spdesc_directed.cpp`
· built 2026-09-21 (TERRASSEM) · phase 6 · gpu clock

`TERRAIN.LOD` takes sixteen per-subpatch descriptors and emits sixteen level
decisions. **Nothing produced them.** This block is that producer, and it is the
other half of `zhao_console_core.sv` entry **I21** — the half packet JOBISSUE
named on the morning of the same day, after building the issuer and finding
that the issuer was not the hard part.

---

## Why it is a block and not wires in a composer

`zhao_terrain_lodfeed` states the test and this block meets it: the door queue
below is **state**, and the slot/src_id pairing is a **decision**. Neither
belongs in a composer.

## The sixteen fields live in three places

| `zhao_terrain_lod` input | comes from | how |
|---|---|---|
| `sp_dev1_i` / `sp_dev2_i` / `sp_dev3_i` | `zhao_terrain_devstore.r_dev1/2/3_o` | forwarded |
| `sp_prev_level_i` / `sp_prev_morph_i` / `sp_hold_i` | `zhao_terrain_devstore.r_prev_level_o` / `r_prev_morph_o` / `r_hold_o` | forwarded |
| `sp_cy_i` | `zhao_terrain_devstore.r_cy_o` | `raw << 8`, exact (`spec/qformats.md` §9) |
| `sp_cx_i` / `sp_cz_i` | `zhao_terrain_compcache_front.lat_wx_o` / `lat_wz_o` | read at the centre vertex |
| `sp_src_id_i` | the compose door | popped beside the slot |
| `dual_i` (TERRAIN.LOD's, as `patch_dual_o`) | the compose door | popped beside the slot |

**`patch_dual_o` was added 2026-09-21 (packet TERRACOMP) and it is a REPAIR,
not a convenience.** `zhao_terrain_lod` takes `dual_i` as a module-level level
and its own contract lists it among the targets that "must be held stable
across a patch job". The composer's available net was
`zhao_terrain_pagestream`'s live `v_flags_o[DUAL]` — which is the **FILLING**
page's. With the compose cache holding one page filling and another served
those are **different pages**, so the console would have decided each patch
with the *next* patch's underside flag: missing or spurious undersides, with
every counter agreeing. Stability was only half the problem and identity was
the other half. The door queue already carries the {slot, src_id} pair captured
at fill acceptance and popped at serve; the flag is the same fact about the
same page on the same beat, and it costs one bit per entry. It is emitted as a
**level**, not a descriptor field, because `dual` is a property of the page and
is identical for all sixteen subpatches. `terrain_spdesc_directed` case 2
serves a DUAL page while a NON-dual page sits at the door, which is the exact
arrangement a live net gets wrong.

The **centre vertex of subpatch `sp` is `(ox + CENTRE_OFF, oz + CENTRE_OFF)`**,
with `ox = sp[1:0] * SUB_EDGE` and `oz = sp[3:2] * SUB_EDGE` — the encoding
`zhao_terrain_loddev` declares on `dev_sp_o` (`{oz/8, ox/8}`) and computes in
`ox_c`/`oz_c`, and the same vertex `zhao_terrain_lodfeed` sampled `w_cy_o` at.
**That offset is one source of truth**; if it moves it moves in `CENTRE_OFF` and
in lodfeed together, and an elaboration guard refuses a value that walks off the
lattice.

---

## The three absences entry I21 listed, and what each turned out to be

I21 had been re-measured three times. JOBISSUE attempted the build and the
stated blocker dissolved; these three are what it found underneath. **All three
were attempted here before any was believed** (ruling R237), and two were
smaller than stated.

### 1. `sp_cx_i`/`sp_cz_i` "have no producer" — they have one, and it was named

`zhao_terrain_lodfeed`'s own `w_cy_o` port comment says it: *"x and z come free
from TERRAIN.PLACE's placed column/row stream at serve time"*. That stream is
written into `zhao_terrain_compcache_front` on `pos_we_i`/`pos_axis_i`/
`pos_idx_i`/`pos_val_i` and served back on `lat_wx_o`/`lat_wz_o`. **A named
source with no reader is a build, not an absence.**

This block **reads that port**. It does **not** recompute the placement from a
patch origin and a pitch, which is the available shortcut and would be a second
implementation of TERRAIN.PLACE's law — the way two blocks come to disagree
about where a patch is.

And it reads the **serve port**, not the write stream. Observing `pos_we_i`
would require this block to hold its own 33 + 33 positions *and its own
fill/serve parity* — and the parity is a decision
`zhao_terrain_compcache_front` has already made
(`wx_m[(serve_par_q ? LAT_W : 0) + rd_vi_c]`). Reading through the serve port
gets that swap for free and cannot disagree with it.

### 2. `zhao_terrain_devstore` "has no `src_id` column" — true, and it needs none

The store is read with a **slot this block already holds**, and `sp_src_id_o` is
the id popped from the door beside that slot. A column would carry the id in at
page load and out at serve to reach a block that was handed it at the door.

**Nothing is narrowed by this.** `zhao_terrain_lodfeed`'s `w_src_id_o` keeps its
existing reader — MEASURE.HISTOGRAM's event ingress under ruling R70 — and is
not asked to acquire a second one.

### 3. "keyed by SLOT, served by SRC_ID, with no map" — real, and the answer is not a map

This is the one that stands, and it is why the block exists.

A `src_id → slot` map would need **one entry per resident page**, 1,024 of them,
compared associatively against a 16-bit id, because a page stays resident across
frames and is re-composed every frame without being re-loaded. That is a CAM,
and a 1,024 × 16 b CAM to recover a number that was in hand a moment ago is the
wrong shape.

**The slot is known at the compose door.** `zhao_terrain_pagestream` emits
`v_slot_o` and `v_src_id_o` **on the same vertex beat**, and the compose fill
starts on one of those beats. So the pair is captured at the door, queued, and
popped for the patch the cache actually serves — which is
`zhao_terrain_jobissue`'s draw-context queue exactly, one block old and proved
by its own directed test. Its arming law (`serve_seen_q`, one arm per rising
`serve_valid_i`) is **copied rather than re-derived**: two blocks arming off one
event must not have two laws.

---

## The pairing is checked, and the checker can fire

`door_src_mismatch_o` differences the popped `src_id` against `serve_src_id_i`.

CLAUDE.md's metadata-swap chapter is the reason that sentence is not sufficient
on its own, so, explicitly: **the two operands are not clocked by one enable.**
The popped id is written by `dq_push_c` (the compose door's acceptance) and read
by `dq_pop_c`; `serve_src_id_i` is combinational off the cache's **serve
parity**, which moves on the swap. A door that pushed the wrong patch, a swap
that did not happen, and a swap that happened one patch early each move exactly
one of the two. `terrain_spdesc_directed` case 7 fires it with a negative
control beside it.

**A mismatch is reported, not repaired.** This block cannot know which side is
wrong, and dropping the patch would turn an identity fault into a hole. The
descriptor carries the **door's** id, and the counter says so.

---

## It never delays the lattice port's existing client

`zhao_terrain_compcache_front`'s `lat_req_i` **has no ready**. A request is made
and the datum is there the cycle after. There is no back channel, so a
pass-through that held a request up would not delay it — **it would destroy
it**, and TESS would read a stale datum with every counter agreeing.

`zhao_terrain_heighttap` already solved this and its law is copied: **upstream
first, this block's read on the cycles upstream leaves.** `c_lat_req_o` is
`o_lat_req_i` whenever the upstream client is asking, and this block's own
request only on a cycle when it is not. The response needs **no multiplexing at
all** — if the upstream asked, the next cycle's datum is the upstream's; if this
block injected, it is this block's; the two cases are exclusive by construction.

`terrain_spdesc_directed` checks that safety property **on every cycle of every
case**, not in one case: every upstream request reached the cache, and none had
its `vi`/`vj`/`surface` altered.

**Losing the race is a duration, not a fault.** `lat_wait_clocks_o` climbs while
this block wants the bus and the client has it. That is
`zhao_terrain_jobissue`'s correction from the same run, applied before the
mistake instead of after it.

## The composition obligation that comes with it

The block takes `o_lat_*` in and drives `c_lat_*` out, so it **splices into the
existing chain** rather than adding a port to the cache. In
`zhao_console_core.sv` the chain is `u_terrain_tess` → `u_terrain_heighttap`
(`tt_lat_*` → `htp_o_*`) → `u_terrain_compcache` (`htp_c_*`). This block goes at
one of those two seams and the composer must preserve *answer the cycle after
the request*.

---

## Counters

Five faults, all reachable from the block's own boundary with legal stimulus, so
**no committed mutant is owed** (ruling R95) — each is fired on purpose with a
negative control in the same case.

| counter | fires when | case |
|---|---|---|
| `door_refused_o` | a door entry arrives against a full queue | 5 |
| `serve_no_door_o` | a patch is served with no identity queued | 6 |
| `door_src_mismatch_o` | the popped id is not the served id | 7 |
| `patches_unfresh_o` | the store holds no records for the slot | 8 |
| `sp_order_bad_o` | the store's read is out of subpatch order | 9 |

Three measurements, **which are not fault flags and may not be read as ones** —
all three are non-zero in ordinary operation:

| instrument | meaning |
|---|---|
| `store_wait_clocks_o` | clocks armed with the store not yet answering |
| `lat_wait_clocks_o` | clocks wanting the lattice bus while the client has it |
| `assemble_clocks_o` | clocks spent assembling, total |

**The pair to read is either wait counter climbing while
`patches_assembled_o` is flat.** That is a starve; the wait alone is a busy
machine.

### `patches_unfresh_o` records a defect this block's own test found

It was first sampled in `StStart`, on the cycle `r_start_o` is presented.
`zhao_terrain_devstore` loads `r_fresh_q` from `slot_valid_q[r_slot_i]` **inside
its own `R_IDLE: if (r_start_i)` arm**, so on that cycle the port still carries
the *previous* patch's answer. The counter attributed page A's freshness to page
B, silently, in the flattering direction whenever A was fresh.

It is now sampled on the **first record**, when the store is in `R_STREAM`.
Case 8 runs a fresh patch and then an unfresh one and **proves the plant** — it
asserts the start cycle really did carry the other patch's flag — before quoting
the fire, because otherwise "the counter fired" would not distinguish a correct
sample from a lucky one.

It is counted **per patch**; the store's own `read_unwritten_o` is the per-read
number.

---

## Cost

**Hand-counted, not fitted** (ruling R236 — the cost is recorded, it is never a
veto; the owner's instruction for this campaign is not to fit).

No multiplier. No memory. `DOORD × 26 b` of door queue (104 flops at the
default), one latched descriptor (~150 b), a six-state walk, and ten 32-bit
counters. **Order 250 flops, a couple of hundred ALM.** The one arithmetic
operator is `sp[1:0] * SUB_EDGE`, a multiply by a power-of-two constant that
synthesises to wiring.

Throughput: three to four clocks per subpatch, ~56 per patch, against a frame's
1.67 M gpu clocks. `assemble_clocks_o` prints the real number rather than this
document pinning it — a pinned clock count goes stale silently
(`zhao_terrain_lodfeed`'s rule).

## Parameters

`SLOTW` 10 · `DEVW` 24 · `MORPHW` 17 · `LAT_W`/`LAT_H` 33 · `SUBPATCHES` 16 ·
`SUB_EDGE` 8 · `CENTRE_OFF` 4 · `DOORD` 4.

`DOORD` is a **knob, not a derivation**: the compose engine holds at most two
patches (one filling, one served) and the door may be one patch ahead of the
walk, so 4 is two clear — the same size and the same reasoning as
`zhao_terrain_jobissue`'s `CTXD`, whose queue is fed by the same door pulse.

Elaboration guards (inside `initial begin ... end`, because Quartus 17.0 rejects
a module-scope `if` — and `--lint-only` does not run them, which is why the
*shape* is also checked by `tools/quartus/check_quartus17_syntax.py`): 
`SUBPATCHES == 16`, the centre column and row on the lattice, and `DOORD >= 2`.

## Evidence

`terrain_spdesc_directed` — **1,035 checks, 0 failures**, eleven cases. Lint:
0 diagnostics at `-Wall`. `check_quartus17_syntax`: RC 0. Not fitted.

## Status

**BUILT, NOT COMPOSED.** What still stops the TERRAIN group is recorded in the
TERRASSEM findings and is no longer this block: it is `zhao_terrain_lod`'s
*other* inputs — MEASURE.GOVERNOR's six knobs and the four contract-refused
`edge_*` neighbour levels — plus the 185 M10K the store costs.
