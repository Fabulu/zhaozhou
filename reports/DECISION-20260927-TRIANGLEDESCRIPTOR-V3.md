# DECISION — `TriangleDescriptor` v3 carries the **material state**, because neither back end can compute it and no lookup can reconstruct it

METASIDE, 2026-09-27. Branch `gz/metaside`, base `4d11ca7f`.
Standing authority: `reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` §0 and §4
(*"If the old compact records cannot carry the full commissioned function,
introduce a versioned extension or immutable sidecar keyed by the same
identity. … The architect has explicit authority to amend record schemas for
this purpose."*).
Decision-record format per §0: question; chosen option; reason and alternatives;
constraints/cost; code/tests/compatibility consequences.

---

## QUESTION

`zhao_geom_bin_pipe_v2` assembles `job_meta_w` from its own `tri_*` input ports.
`META_FIXED_W` is spelled out in that file as a sum:

```systemverilog
  localparam int unsigned META_FIXED_W  = 298    // tri_flat_request_i
                                        +  48    // tri_continuation_tail_i
                                        +  32    // tri_fragment_state_i
                                        +  47    // tri_area2_i
                                        +  12;   // tri_min_x_i
```

`TriangleDescriptor` v2 gave the walk side the last two. **The first three have
no producer on the walk side at all**, and — this is the part seven packets
missed — **they never had one on the back end either**:

```
grep -c "flat_request|continuation_tail|fragment_state" zhao_geom_setup.sv    -> 0
                                                        zhao_geom_attrpack.sv -> 1   (a comment disclaiming one)
```

They arrive at the binner on **console wires** from `zhao_material_window`.
So: where do 378 of `METAW`'s 1,877 bits come from when the raster is fed from
SDRAM?

---

## THE MEASUREMENT THAT DECIDES IT, AND IT KILLED THE PROPOSED ANSWER

`FINDINGS-swapclose.md` §5.1 recommends a lookup:

> *"The right repair is architectural and it is not a sidecar. The descriptor
> already carries `material_id` … and `zhao_material_window` is a resolver keyed
> by material, so the walk should **re-ask the window per triangle**."*

**It cannot work, and the refutation is two paragraphs further down the same
document.** Not every one of the 378 bits is a function of the material:

| field | authority | is it a function of `material_id`? |
|---|---|---|
| `sample_count`, `material_recipe`, `recipe_weight`, `base_binding` | MATERIAL.RESOLVE's response | **yes** |
| `palette_slot`, `palette_generation`, `response_class` | the binding page | **yes** |
| `effect_tag`, `stencil_reference`, the material's `fragment_state` | the material's declared profile | **yes** |
| **`vertex_alpha`** | **owner ruling R89** — `zhao_forge_shadow.sv:295`, `vtx_alpha_o = strength_q`, latched **PER CASTER** | **NO** |
| **`detail`** | **`zhao_terrain_clipfeed`'s `o_detail_o`**, a per-primitive declaration granted by `u_geom_clipdoor` on the triangle's own beat | **NO** |
| **`base_rgb` / `recipe_weight` under the mosaic** | `st_mat_token`, TERRAIN's per-CELL `{matA, matB, weight}` triple, riding GEOM.SETUP's own registers | **NO** |

SWAPCLOSE's own words, §5.1's last paragraph: *"`vertex_alpha` is the span's
under R89, `detail` is terrain's per-primitive declaration … Part is a keyed
lookup; part is genuinely per-primitive."* **The document recommends a mechanism
it then proves insufficient, three paragraphs apart.** That is HANDOVER §15.35's
own law — *a document that refutes itself is not self-correcting* — recurring
inside the very document that named it.

A material-keyed lookup would therefore reconstruct most of the state and
silently substitute the wrong value for three fields. §4 forbids exactly that:
*"Do not silently overload an existing field, truncate a full handle, or replace
a missing attribute with a convenient zero."*

---

## CHOSEN OPTION

**`TriangleDescriptor` v3 — 48 bytes**, the third sixteen carrying a 128-bit
`MATSTATE` word, declared once in `zhao_pkg` as `ZHAO_MS_*_LO` / `_W`, with
`ZHAO_PARAMBUF_TD_SCHEMA = 3`.

**The first thirty-two bytes are byte-identical to v2** (and their first sixteen
to v1). The amendment is purely additive, which is what makes it a *versioned
extension* in §4's sense rather than a re-layout.

### The 115 bits that vary, and why it is not 378

The 378 bits are a **composition**, and most of it is structurally constant in
this console: `aux_required` is `1'b0`, `aux_surface_ctx` is `224'd0`, the LOD
byte and base alpha are named localparams, five tail bits are a named unused
constant, and the tail's 18-bit arena index is the record's own **address**.

| bits | field | note |
|---|---|---|
| 1 | `valid` | the window's `pub_valid` **at this triangle's beat** |
| 1 | `detail` | per-primitive, terrain's |
| 8 | `vertex_alpha` | R89's, **already composed** with its default |
| 8 + 8 | `effect_tag`, `stencil_reference` | **already composed** |
| 32 | `fragment_state` | **already composed** — the material's or the producer's |
| 2 + 8 + 3 + 8 | `sample_count`, `base_binding`, `material_recipe`, `recipe_weight` | weight **already composed** with the mosaic substitution |
| 24 | `base_rgb` | **already composed** with the mosaic substitution |
| 2 + 2 + 8 | `response_class`, `palette_slot`, `palette_generation` | |
| **115** | | 13 reserved, written zero, with its own detector |

**Six of these are stored ALREADY COMPOSED**, and that is the load-bearing
choice. Each is the result of a *selection* in `zhao_console_core` — the
material's value or a named profile default, the mosaic's per-cell triple or the
span's. Re-running that selection on the walk side would be a **second
expression of a ratified rule**, which is precisely the objection that chose the
time multiplex over a second back end. Storing the result makes the rule exist
once.

### THERE IS ONE ROAD, AND THAT IS STRONGER THAN A CHECKER

The obvious hazard in storing 115 bits of a 378-bit quantity is that somebody
later makes one of the "constant" fields live and forgets this record. **That is
structurally impossible here**, because `zhao_console_core` composes the **live**
path through the *same unpack function* the walk uses:

```
  live state ──pack──► MATSTATE ──┬──► (descriptor, SDRAM, walk) ──► unpack ──► tri_*_i
                                  └──► unpack ──► tri_*_i          (live)
```

A field that is not in the MATSTATE layout cannot reach `zhao_geom_bin_pipe_v2`
on **either** path. A future field that forgets this record therefore **fails to
render at all** rather than rendering wrong — it cannot be a convenient zero on
the walk while being correct on the live path, because there is no second
expression for it to be correct in.

This is deliberately not a checker. CLAUDE.md's chapter on detectors wired to
two operands that move together is exactly the failure a checker here would
have: the natural check compares the walk's reconstruction against the live
composition, and those two would be built by the same code from the same fields,
so the comparison would be blind to the one fault it exists to catch. **A
missing road cannot go blind.**

### THE KEY, AND WHY IT IS SAFE

**There is no key.** The state is *in the record*, at a fixed offset, fetched in
the same burst. There is no second store, no lookup, no eviction and no reuse —
so §4's *"prove that eviction/reuse cannot change a still-referenced identity"*
is satisfied **structurally rather than by argument**. The sidecar-keyed-by-id
form would have owed that proof; this form has nothing that could mis-key.

`u_geom_tidq` is this console's own demonstration of why that matters: it was
**one behind** and mis-attributed **74 of 75** triangles with every range guard
passing. A separate store keyed by triangle id is the same shape as that defect.

The arena index is still needed — the continuation tail carries it at `[41:24]` —
and it is supplied **alongside** the state rather than inside it: from
`u_geom_tidq` on the live side, and on the walk side from **the address the
descriptor was fetched at** (`w_tri_base_q + id * TD_B`). Storing it inside the
record would be a second copy of one fact, which is the reasoning v2 used to keep
`untex` out.

### THE CAPTURE POINT IS PROVEN BY THE WINDOW'S OWN INTERLOCK

The state is captured in `zhao_geom_vertid`, loaded in `S_IDLE` on `tri_valid_i`
— **the same event that loads the whole record**, because *"a field latched on a
DIFFERENT condition from the record it belongs to is this repository's own
metadata-swap defect"* (that block's own comment).

That `mw_pub_*` is the **triangle's own** publication at that beat is not a
latency coincidence. `zhao_material_window`'s header states the interlock and the
console wires it:

```systemverilog
    .d_enter_i  (cl_in_valid && cl_in_ready),                       // GEOM.CLIP's INPUT
    .d_leave_i  (door_tri_valid_w && door_tri_ready_w),             // the door
```

> *"the window never changes what it publishes while ANY triangle is between
> GEOM.CLIP's input and the door … So the span downstream of this block is, at
> every instant, occupied by triangles of ONE material, and that material is the
> published one."*

`zhao_geom_vertid` consumes at GEOM.CLIP's **output** — strictly between those
two events — so the publication it samples is the one the door would have
sampled. `err_unpublished_o` is the counter that watches the one thing which
would falsify this, and it is already composed.

---

## WHY, AND THE ALTERNATIVES REJECTED

### 1. Re-asking `zhao_material_window` per triangle — refused on the table above

Three of the fields are not functions of the material. The mechanism cannot
express them, and using it would put a convenient zero in three places. It also
re-issues a resolve per triangle against a resolver whose own header says a
repeat of the same `{set, id}` is the common case and *"costs nothing at all"* —
i.e. it is sized for span changes, not per-triangle traffic.

### 2. An immutable SIDECAR keyed by triangle id — refused on round trips **and** on identity

§4 permits it by name, so it was live, and the brief for this packet suggested
it. Refused for the two reasons v2's record gives and one more:

* **two fetches per triangle instead of one.** `DECISION-20260927-TRIANGLEDESCRIPTOR-V2.md`
  rejected a sidecar for `area2` on exactly this: *"two guard requests, two
  verdict pairs, two burst round trips, on a consumer side already measured at
  7.18× the on-chip drain's clocks per reference."* This extension is read on
  **every** triangle the raster draws, which is the case that record names as
  the wrong shape for a sidecar.
* **it would owe the eviction/reuse proof** §4 demands, against a keyed store,
  on a console that has already shipped a one-behind id queue.
* it costs the **same** 262,144 bytes of the view, so it buys nothing in space.

### 3. Carrying all 378 bits verbatim — refused on capacity, measured

378 bits is 48 bytes, so the record would be 80 bytes and `TRI_CAP_B` would be
`16,384 × 80 = 1,310,720` against **524,288 spare**. It does not fit, and §0
forbids reaching a fit by shrinking a declared maximum. 225 of those bits are a
constant zero (`aux_required` + `aux_surface_ctx`) and 18 are the record's own
address; storing either would be storing a fact twice.

### 4. "The span had one material, so reuse the last publication" — the forbidden shortcut

True in this fixture, false in general, invisible to every gate we own, and named
by §4 in as many words. It is refused, and — see *Tests* below — it is **committed
as a mutant** so that the refusal is evidence rather than a sentence.

---

## CONSTRAINTS AND COST

### It fits inside `ZHAO_PARAMBUF_VIEW_SPAN`, at full R7 capacity, with 256 KiB spare

At the console's instantiated `MAX_VERTS = 65536`, `MAX_TRIS = 16384`,
`MAX_CHUNKS = 16384`, `PV_STRIDE_B = 32`, `CK_B = 64`, `LAYOUT_ALIGN_B = 16`:

| constant | v2 (`TD_B` = 32) | **v3 (`TD_B` = 48)** |
|---|---|---|
| `VERT_CAP_B` | 2,097,152 | 2,097,152 |
| `TRI_OFF_B` | 2,097,152 | 2,097,152 |
| `TRI_CAP_B` | 524,288 | **786,432** |
| `CHUNK_OFF_B` | 2,621,440 | **2,883,584** |
| `CHUNK_CAP_B` | 1,048,576 | 1,048,576 |
| **`VIEW_USED_B`** | **3,670,016** | **3,932,160** |
| `VIEW_SPAN` | 4,194,304 | 4,194,304 |
| spare | 524,288 | **262,144** |

**No region in `spec/memory_rules.md` §5c moves**, because the growth is absorbed
inside the view's own span. `VIEW0_BASE`, `VIEW1_BASE` and `SCRATCH_BASE` are
unchanged and their elaboration guards still hold.

`TD_B = 48` is a multiple of `BURST_ALIGN_B = 16`, so the arena's
`(TD_B % BURST_ALIGN_B) != 0` guard still passes; `TD_B / 8 = 6` is an exact beat
count, so the divisibility guard is satisfied rather than tripped; and 6 fits the
walker's 4-bit `r_beats_q`. The walker's shift register is `SHW = CK_B * 8 = 512`
bits and the record is 384, so it still loads into the buffer that exists.

### The one real cost, declared rather than absorbed

**SDRAM traffic on the TRIANGLE arm goes 4 beats to 6**, 32 bytes to 48, on both
the write and the walk's read — **+50%** on that arm, additive to v2's own
doubling. At the smoke fixture's 75 triangles that is 1,200 extra bytes per
frame; at `MAX_TRIS = 16384` it is 256 KiB per frame each way.

It is unavoidable in kind: the bytes are the function. What is avoidable and was
avoided is the other 263 bits, which are constants and an address.

### No new SDRAM share slot, and no new client

The descriptor read already issues on `gs_req[2]`, the walker's own socket. A
wider record changes that request's `len` and beat count, not its existence.
Both shares remain full and neither is widened.

### No new console port

`zhao_geom_vertid`, `zhao_geom_parambuf` and `zhao_geom_paramwalk` gain ports;
`zhao_console_core` does not. So R220's two `.*` wrapper mutants are unaffected,
and the smoke bench gains no wire. `zhao_prod_top.sv` and `zhao_console_board.sv`
are regenerated because they instantiate the changed leaves by name.

---

## CONSEQUENCES — CODE, TESTS, COMPATIBILITY

**Code.** `zhao_pkg` gains `ZHAO_TD_MATSTATE_LO/W` and the fourteen `ZHAO_MS_*`
field constants, and moves `ZHAO_PARAMBUF_TD_BYTES` 32 → 48 and
`ZHAO_PARAMBUF_TD_SCHEMA` 2 → 3. `zhao_geom_vertid` gains one input and one
output, loaded by the same `S_IDLE` event as the rest of the record.
`zhao_geom_paramarena`'s encoder gains one `td_bytes_c` assignment;
`zhao_geom_parambuf`'s decoder gains one slice and one reserve detector;
`zhao_geom_paramwalk` gains `t_matstate_o` and `t_arena_id_o` and its `td_buf_q`
goes 256 → 384 bits with `TD_BEATS` 4 → 6, all derived. `zhao_console_core`
gains the pack/unpack function pair and routes **both** paths through it.

**Compatibility.** No ABI change — `spec/commands.zidl` is untouched, so
`npm run abi:check` is not implicated. v2 and v3 descriptors never coexist in one
arena: one build writes the region and the same build reads it back within the
frame, which is the argument v2 and PROJECTEDVERTEX-V2 both made for versioning
at elaboration rather than per record.

**Tests.** The evidence bar is an identity round trip that **fails against v2 and
fails against a hold**:

* every triangle's MATSTATE **distinct**, so a decoder returning a neighbour's
  field fails rather than agreeing with itself — the `tidq` lesson, which is the
  named failure this record exists to make impossible;
* at least one field **set in each of the six composed positions**, so a
  selection that collapses to its default is caught;
* the reserve **nonzero** on one record, to fire the detector, and clear again;
* and a **committed mutant** under `tests/mutants/` implementing §4's forbidden
  shortcut — the walk holding the previous triangle's MATSTATE instead of
  decoding its own — with inverted polarity, so the refusal of *"the span had one
  material, so reuse the last publication"* is **evidence** rather than a
  sentence in a comment.

The three committed arena mutants are COPIES regenerated by
`tools/rtl/gen_paramarena_mutants.py`, which is a command rather than a
transcription, and each is re-fired — v2 found the hard way that a width change
one file away can turn a mutation into a no-op with every instrument green.

---

## WHAT THIS RECORD DOES NOT CLAIM

It does not claim `I55` is closed, and it does not claim that carrying these bits
makes the console draw at `GEOM_WALK_RASTER = 1`.

**Those are two different problems and this packet found that they had been
conflated.** The brief and entry `I55` both state that the walk-arranged frame
stops *because* of this metadata — *"fragments are generated and NONE EVER BLEND,
so the tile pipe never empties"*. `blended_fragments_o` counts fragments whose
write was **not** `BL_REPLACE` (`zhao_raster_fragment.sv:671`); every span in
this fixture is REPLACE; and a **healthy** console frame that draws 2,816 pixels
and resolves 11 tiles reads `frags[covered/blended]=[1216 0]`
(`reports/synthesis/arenabin/smoke_plain_arenainfer.log:75`). **The zero is what
success looks like.** It is a counter, it gates nothing, and nothing in
`ordinary_pipe_empty_w` reads it.

So this record claims one thing: **the 378 bits have no producer on the walk side
and no lookup can reconstruct three of them, and 115 bits carried in the record
they belong to is the mechanism §4 authorises for that.** Whether the walk-arranged
console then draws is measured in METASIDE's FINDINGS, not here.
