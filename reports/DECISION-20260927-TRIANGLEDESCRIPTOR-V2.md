# DECISION — `TriangleDescriptor` v2 carries **2A and the scan box**, because `GEOM.SETUP` consumes them and they are not recoverable

MUXBUILD, 2026-09-27. Branch `gz/muxbuild`, base `b4bd4830`.
Standing authority: `reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` §0 and §4
(*"If the old compact records cannot carry the full commissioned function,
introduce a versioned extension or immutable sidecar keyed by the same
identity. … The architect has explicit authority to amend record schemas for
this purpose."*).
Decision-record format per §0: question; chosen option; reason and alternatives;
constraints/cost; code/tests/compatibility consequences.

---

## QUESTION

`reports/DECISION-20260927-I55-SWAP-ARCHITECTURE.md` chose a **time multiplex**:
during the raster drain window the provably-idle `u_geom_setup` /
`u_geom_attrpack` pair is fed from the SDRAM walk instead of from `GEOM.CLIP`.
`reports/DECISION-20260927-PROJECTEDVERTEX-V2.md` then made the six plane inputs
reconstructible from the arena at full precision.

Neither record asked what the **rest** of `zhao_geom_setup`'s input port carries.
It carries three things the ProjectedVertex does not:

```systemverilog
  input  logic signed [47:0] tri_area2_i,      // 2A, subpixel², winding-normalised
  input  logic signed [11:0] tri_min_x_i,      // the §8 SCISSORED scan box
  input  logic signed [11:0] tri_max_x_i,
  input  logic signed [11:0] tri_min_y_i,
  input  logic signed [11:0] tri_max_y_i,
```

The 16-byte `TriangleDescriptor` is three u16 vertex ids, a u16 material, a u32
raster word and a u32 source word — **128 bits, every one of them spoken for**.
So: where do 2A and the scan box come from on the walk side?

---

## THE MEASUREMENT THAT DECIDES IT, AND IT KILLED THE CHEAP ANSWER

The attractive answer was that **2A is redundant with what `GEOM.SETUP` already
emits**. The barycentric identity `E_0(p) + E_1(p) + E_2(p) = 2A` holds for every
`p`, so at the origin `kc0 + kc1 + kc2 = 2A` — and setup outputs all three
constants. On that reading the walk-side feed needs no 2A at all: take it out of
the sum, two 48-bit adds, no multiplier, no schema change.

**It is circular, and `zhao_geom_setup.sv:384-387` is why:**

```systemverilog
      s3_kc0 <= sxprod(s2_p0) - sxprod(s2_p1);
      s3_kc1 <= sxprod(s2_p2) - sxprod(s2_p3);
      s3_kc2 <= s2_area2 - (sxprod(s2_p0) - sxprod(s2_p1))
                         - (sxprod(s2_p2) - sxprod(s2_p3));
```

**`kc2` is DEFINED as `area2 - kc0 - kc1`.** The identity holds in this RTL *by
construction* — which is exactly why it carries no information. Setup computes
only **two** of the three cross products and spends the supplied `2A` to avoid
the third. `tri_area2_i` is therefore **load-bearing input arithmetic, not a
passthrough**, and recovering `2A` from setup's own outputs is recovering it from
itself.

Note the direction, because it is this file's own law. The sum identity is true,
it is elegant, and believing it would have produced a back end whose `kc2` was
`(kc0+kc1+kc2) - kc0 - kc1` — an expression that is **identically `kc2` for any
garbage `area2` whatsoever**. The circuit would have produced plausible planes
from an unconstrained number, every handshake healthy, every counter balanced.
**A derivation that cannot fail is not a derivation.** The check that separated
them was reading four lines of the block being fed.

`out_area2_o` is also a **real consumer**: it is carried to
`RASTER.EDGEWALK`'s job port, so it is not merely setup's private scaffolding.

### And the box is genuinely `GEOM.CLIP`'s, not a min/max

`zhao_geom_clip.sv:247-250` emits the box as *"scan box, inclusive pixels"*, and
its header calls it *"the §8 **scissored** scan box … in whole pixels and
scissored to the viewport"* with *"conversion + scissor clamp of the min/max
(shift, add, compare)"*. It is a subpixel→pixel conversion **and** a clamp
against the viewport rectangle, not `min`/`max` of three corners. Re-deriving it
on the walk side needs the viewport, the conversion law and the clamp law —
three ratified rules, re-expressed.

---

## CHOSEN OPTION

**`TriangleDescriptor` v2 — 32 bytes, declared once in `zhao_pkg`**, with the
second sixteen bytes carrying what `GEOM.CLIP` computed and the v1 record threw
away.

| bit offset | width | field | signed | why |
|---|---|---|---|---|
| 0 | 16 | `v0` | u | v1, unchanged |
| 16 | 16 | `v1` | u | v1, unchanged |
| 32 | 16 | `v2` | u | v1, unchanged |
| 48 | 16 | `material` | u | v1, unchanged |
| 64 | 32 | `raster` | u | v1, unchanged |
| 96 | 32 | `source` | u | v1, unchanged |
| 128 | **48** | `area2` | **s** | `GEOM.SETUP`'s `tri_area2_i`, exact, stored whole |
| 176 | **12** | `min_x` | **s** | the §8 scissored scan box, exact |
| 188 | **12** | `max_x` | **s** | |
| 200 | **12** | `min_y` | **s** | |
| 212 | **12** | `max_y` | **s** | |
| 224 | **32** | reserved | | **written 0; nonzero is malformed** |
| | **256** | | | **32 bytes exactly** |

**The first sixteen bytes are byte-identical to v1.** The amendment is purely
additive, which is what makes it a *versioned extension* in §4's sense rather
than a re-layout.

### Every field is stored at full width and carries NO domain claim

`area2` is s48 because `tri_area2_i` is s48; the box fields are s12 because
`tri_min_x_i` is s12. Nothing is narrowed to a measured domain. This is
`PROJECTEDVERTEX-V2`'s own rule — *"no claim at all beats a claim with a large
margin"* — and here it is free, because 96 bits of function fit in 128 bits of
space with 32 left over.

### `untex` is NOT a new field, and checking that saved one

The obvious seventh field is `zhao_geom_attrpack`'s `tri_untex_i`. **It is
already carried**, in the ProjectedVertex `status` byte:
`zhao_geom_vertid.sv:513` is `pv_status_o = {4'd0, shareable_q, untex_q, dom_q}`,
so `untex` is `status[2]` of any of the triangle's three vertices and the walk
decodes it already. Adding it to the descriptor would have been a **second
storage site for one fact**, which is how two copies come to disagree.

---

## WHY, AND THE ALTERNATIVES REJECTED

### 1. RECOMPUTING `2A` on the walk side — refused on this tree's own law

`2A = (bx-ax)(cy-ay) - (cx-ax)(by-ay)` from three fetched corners is two
signed multiplies. The objection is not the ~2 DSP; it is that
`zhao_geom_clip.sv:460` is

```systemverilog
  assign out_area2_o = flip ? -s3_area : s3_area;
```

— the value is the clip's area **after the winding normalisation that also
swapped B and C**. A second site computing it owes bit-equality with that,
including the flip, at every triangle, every cull mode and every degenerate
case. `zhao_forge_assemble.sv:62-66` states the admissible form:

> A second INSTANCE of one law is not a second law; a second EXPRESSION of it
> would be, and there is none here.

The time multiplex was chosen **precisely because** it is the same silicon and
therefore bit-identical by construction. Bolting a re-derived `2A` onto its input
would reintroduce, at the one port that feeds it, the exact verification burden
the architecture was chosen to avoid. It would also be a second expression of the
scissor law.

### 2. An immutable SIDECAR keyed by triangle id — rejected on round trips

§4 permits *"a versioned extension or immutable sidecar"*, so this was live. A
16-byte sidecar in the view's spare bytes holds the same 96 bits and leaves the
v1 descriptor untouched.

It is rejected because it costs **two fetches per triangle instead of one**: two
guard requests, two verdict pairs, two burst round trips, on a consumer side
already measured at **7.18× the on-chip drain's clocks per reference**. Widening
the record turns a 2-beat read into a 4-beat read on the *same* request. Same
bytes, half the round trips. A sidecar is the right shape when the extension is
optional or rarely read; this extension is read on **every** triangle the raster
draws.

### 3. Shrinking a capacity to pay for it — forbidden, and not needed

§0 forbids reaching a fit by reducing declared maxima. Not invoked: **it fits at
full R7 capacity**, see below.

---

## CONSTRAINTS AND COST

### It fits inside `ZHAO_PARAMBUF_VIEW_SPAN`, at full capacity, with 512 KiB spare

At the console's instantiated `MAX_VERTS = 65536`, `MAX_TRIS = 16384`,
`MAX_CHUNKS = 16384`, `PV_STRIDE_B = 32`, `CK_B = 64`, `LAYOUT_ALIGN_B = 16`:

| constant | v1 (`TD_B` = 16) | **v2 (`TD_B` = 32)** |
|---|---|---|
| `VERT_CAP_B` | 2,097,152 | 2,097,152 |
| `TRI_OFF_B` | 2,097,152 | 2,097,152 |
| `TRI_CAP_B` | 262,144 | **524,288** |
| `CHUNK_OFF_B` | 2,359,296 | **2,621,440** |
| `CHUNK_CAP_B` | 1,048,576 | 1,048,576 |
| **`VIEW_USED_B`** | **3,407,872** | **3,670,016** |
| `VIEW_SPAN` | 4,194,304 | 4,194,304 |
| spare | 786,432 | **524,288** |

**No region in `spec/memory_rules.md` §5c moves**, because the growth is absorbed
inside the view's own span. `VIEW0_BASE`, `VIEW1_BASE` and `SCRATCH_BASE` are
unchanged and their elaboration guards still hold.

`TD_B = 32` is a multiple of `BURST_ALIGN_B = 16`, so the arena's
`(TD_B % BURST_ALIGN_B) != 0` guard still passes, and `TD_B / 8 = 4` is an exact
beat count, so `PROJECTEDVERTEX-V2`'s newly added `m_beats_q` divisibility guard
is satisfied rather than tripped.

### The one real cost, declared rather than absorbed

**SDRAM write traffic on the TRIANGLE arm doubles**: 2 beats to 4, 16 bytes to
32 per triangle. At the smoke fixture's 75 triangles that is 1,200 extra bytes
per frame; at `MAX_TRIS = 16384` it is 256 KiB per frame. **The read side pays
the same doubling on the walk.** It is unavoidable: the bytes are the function,
and the alternative is re-expressing two ratified laws.

This is additive to `PROJECTEDVERTEX-V2`'s declared +33% on the vertex arm. Both
are write-traffic increases on `u_geom_wshare` leg 1, and neither adds a client.

### No new SDRAM share slot, and both shares are full — verified independently

* **`zhao_geom_mem_adapter`: ten requesters `a`..`j`, all ten driven** with real
  producers (MESHFETCH, ASSETFETCH, MATERIAL.RESOLVE, DRAWJOB, PART.TABLE,
  FORGE.PRIM, GEOM.POSE, GEOM.LADDERBANK, TEXTURE.PALETTELOAD,
  TERRAIN.NORMALMAP). Its header line 1 still says *"NINE"* and is stale by one,
  as SWAPBUILD and PVSCHEMA both found. **There is no `N` parameter** — the
  letters are hardcoded ports, so widening is a port pair plus arbitration plus a
  round-robin re-proof, not a parameter bump.
* **`u_geom_wshare` is `zhao_mem_share_wr #(.N(3), .CLIENT_ID(3), .RQ(4))`** and
  all three legs are driven: `gs_req[0] = ma_m_req` (the read adapter's merged
  output), `gs_req[1] = pa_req` (the arena writer), `gs_req[2] = pw_req` (the
  walker).

**Neither share has a free slot, and this decision needs none.** Directive §7
says *"prefer sharing an appropriate existing guaranteed read route"* — the
walk's descriptor read already issues on `gs_req[2]`, the walker's own socket,
and a wider record changes that request's `len` and beat count, not its
existence. The vertex arm that accompanies this schema change issues on the
**same** socket: `zhao_geom_paramwalk`'s FSM already multiplexes three request
kinds (directory, chunk, descriptor) onto one `guard_req_o`, and a fourth kind is
a state-machine addition rather than a client addition.

---

## CONSEQUENCES — CODE, TESTS, COMPATIBILITY

**Code.** The layout is declared **once**, in `fpga/rtl/common/zhao_pkg.sv`, as
`ZHAO_TD_*_LO` / `ZHAO_TD_*_W` beside the v2 ProjectedVertex constants, with
`ZHAO_PARAMBUF_TD_SCHEMA = 2` and an elaboration guard asserting
`ZHAO_TD_END_BIT == ZHAO_PARAMBUF_TD_BYTES * 8`. `ZHAO_PARAMBUF_TD_BYTES` goes
16 → 32. The encoder (`zhao_geom_paramarena`) and the decoder
(`zhao_geom_parambuf`) derive from those constants rather than remaining two
hand-maintained inverses.

`zhao_geom_vertid` is the descriptor's producer and **does not currently receive
2A or the box** — it takes the corners, the attributes, `tri_untex_i`,
`tri_material_i`, `tri_raster_i` and `tri_src_id_i` from the same `GEOM.CLIP`
packet, but not those five fields. It gains five inputs and five outputs. That is
a port change on a leaf, so `zhao_prod_top.sv`, `zhao_console_board.sv` and the
shell paired diff are regenerated and **every bench that instantiates any changed
module is connected** — *"a port on a leaf costs its WHOLE instantiation chain
plus every bench."*

`zhao_geom_paramwalk`'s `td_buf_q` goes 128 → 256 bits and `TD_BEATS` 2 → 4.

**Compatibility.** No ABI change — `spec/commands.zidl` is untouched, so
`npm run abi:check` is not implicated. v1 and v2 descriptors never coexist in one
arena: one build writes the region and the same build reads it back within the
frame, which is the same argument `PROJECTEDVERTEX-V2` made for versioning at
elaboration rather than per record. The first 16 bytes are byte-identical to v1,
so a reader of the old fields is correct without change.

**Tests.** The evidence bar is a round trip that **fails against v1**: a
descriptor whose `area2` and scan box are non-trivial, recovered exactly through
the real guard, arbiter, controller and SDRAM. A v1 record cannot carry those
bytes at all, so the discriminating case is structural rather than a chosen
value. The three committed arena mutants are COPIES and are regenerated by
`tools/rtl/gen_paramarena_mutants.py`, which is a **command, not a transcription**
— and each is re-fired, because `PROJECTEDVERTEX-V2` found the hard way that a
width change one file away can turn a mutation into a no-op with every instrument
green.

---

## TWO STALE NUMBERS FOUND WHILE MEASURING THE CAPACITY

Both are in `zhao_geom_paramarena.sv`, both are unchecked by
`check_localparam_comments` for a reason the file itself states, and both read in
the flattering direction:

* `:602` — `localparam int unsigned PV_B = ZHAO_PARAMBUF_PV_BYTES;   // w=24 bytes`.
  **The package now says 32.** The comment is `PROJECTEDVERTEX-V2`'s own change
  seen from one file away, and it was missed because the checker **skips
  constants whose value comes from a package import**.
* `:654` — `CHUNK_OFF_B ... // 2,359,264`. The value is **2,359,296**, 32 bytes
  higher; `VIEW_USED_B`'s own comment on the next line but one (3,407,872) is
  correct and implies it. The block's comment at `:647-651` explains precisely
  why this constant is unchecked — it reaches `TD_B`, a package import — and then
  the unchecked number is wrong.

Both are corrected in the same commit as this schema change, per CLAUDE.md's
*"when you fix a thing this file names as broken, fix this file in the same
commit."*

---

## WHAT THIS RECORD DOES NOT CLAIM

It does not claim `I55` is closed, that the multiplex is built, or that any pixel
yet depends on bytes that went through SDRAM. It claims that the back end the
multiplex feeds needs **2A and the scissored scan box**, that neither is
recoverable from the v1 record or from `GEOM.SETUP`'s own outputs, and that
carrying them costs 16 bytes per triangle inside a view that has the room.
