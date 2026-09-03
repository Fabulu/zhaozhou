# Terrain detail normal maps — architecture decision record

Written 2026-09-03, against the owner's ruling of the same day: *"we make it
and see how bad it is. We'll optimize and cut after we have the number anyway
… Normal maps would be a huge gain though."* The gain is about the LOOK; the
number is about the FIT. This record designs for both.

Companion contract: `design/contracts/TERRAIN.NORMALMAP.md`.

---

## Verdict up front

1. **The draft's algebraic split is sound and is kept.** The heightfield
   observation (axis-aligned tangent frame, no frame to build, nothing extra
   to interpolate) is correct and is the whole reason this is affordable.
2. **The draft's RTL and oracle are not buildable as written.** The divider
   returns ZERO for every realistic triangle (simulated, below), the oracle
   overflows int64 on legal input, fragments are associated with triangles by
   timing rather than by identity, and the block has no seams — nothing feeds
   it and nothing consumes its shade.
3. **Half of the draft's cost is not a normal-map cost at all.** The
   per-triangle term `dot(n,L)/|n|` is terrain LIGHTING, which the production
   machine does not have: TERRAIN.NORMALS computes face normals and **no block
   in `design/prod_manifest.yml` consumes them**. That block (TERRAIN.SHADE)
   is needed with or without normal maps and must be counted separately, or
   the normal-map delta measurement will be wrong by ~2x.
4. **The real per-pixel path is cheap here** — estimated **~350 ALM, 2 DSP,
   8 M10K** for the normal-map-attributable delta (plus ~25 ALM at the
   fragment seam), because the fragment-side application factors into three
   saturating adds and the detail texel never touches the TMU. This is still
   the real technique: a genuine per-fragment normal perturbation lit per
   fragment under moving suns. Only the texel's STORAGE is specialised.
5. **The number is very unlikely to be bad.** Against the ~7,578 ALM
   remaining before the charter reserve, the cuttable part is ~5%. The part
   that could actually hurt (TERRAIN.SHADE at ~700-900 ALM, 8-10 DSP) is not
   cuttable, because it is the terrain's light.

---

## The evidence against the draft, measured

### D1 — the restoring divide computes the wrong 32 bits (hard defect)

`zhao_terrain_normalmap.sv` T_DIV runs **32 steps** over a **64-bit**
numerator. A restoring divide produces one quotient bit per step from the
numerator's MSB down, so 32 steps yield the quotient's bits 63..32 — which
for every realistic `dot/|n|` (quotient <= 2^15 by Cauchy-Schwarz) are all
zero. Simulated with the RTL's exact recurrence:

    flat 1 m cell, sun overhead:  n=(0,65536,0), L=(0,32768,0)
      draft divide -> 0        true -> 32768
    tilted cell:                 draft -> 0    true -> 31257
    same recurrence at 64 steps: -> 32768 (correct)

Every triangle would shade to the ambient state. The fix inside the draft's
shape is 64 steps (or a pre-normalised 48); the fix this record chooses is to
delete the serial divider entirely (see TERRAIN.SHADE below).

### D2 — fragments are matched to triangles by timing, not identity

The fragment side reads `base_o` and `tri_src_q` combinationally. When
triangle N+1 is accepted, `tri_src_q` updates immediately but `base_o` still
holds triangle N's value for the next ~66 cycles — so any fragment arriving in
that window is stamped with **N+1's id and N's shade**, provably mismatched.
There is no interlock, no id on the fragment port, and `f_ready_o` never
consults the triangle FSM. In a pipelined rasterizer where fragments of
triangle N drain while N+1 is set up, this is not a corner case; it is the
steady state.

### D3 — the oracle overflows on legal input

`normalmap_length` sums three squares in int64: at the fx16 rails
(`x=y=z=-2^31`, which TERRAIN.NORMALS' contract says legal input CAN produce
— "output saturation is normal and expected inside the domain") the sum is
3*2^62 = 1.38e19 > INT64_MAX. Signed overflow, UB in C++, wrap in the RTL's
matching `sq_c`. Measured: `13835058055282163712 > 9223372036854775807`.

### D4 — three-way rounding disagreement

`normalmap_detail` divides by 32768 with C++ `/` (truncation toward zero);
the RTL uses `>>> 15` (floor). These differ on every negative product — and
BOTH diverge from the machine's one rounding primitive, `rescale` round-half-up
(`spec/qformats.md` §4). The draft would fail its own differential test on the
first negative detail term, and if "fixed" by matching the oracle to the RTL
it would enshrine a third rounding mode the spec explicitly rejects.

### D5 — ambient-as-floor contradicts ratified law

The ratified vertex-light law (`spec/sky_and_beams.md` §4a) is additive:
`lit = sat_u8(ambient_c + rescale_u(sun_c*ndl, 8))`. The draft makes ambient
a FLOOR. That is a real design position with a real argument (the draft's
header argues it well) — but it silently re-legislates the environment law
from inside a terrain block. If the floor is wanted, it is a
`spec/sky_and_beams.md` amendment, argued there; this architecture keeps §4a.

### D6 — throughput was never checked against the producer

66+ cycles per triangle, serial, against TERRAIN.PROJECT's one triangle per
3 clocks. At ~10-20k terrain triangles/frame the draft's triangle engine alone
costs 0.7-1.3M of the ~1.67M cycle frame. Even fixed per D1, the serial shape
is the wrong shape.

### D7 — process defects

* `ENFORCED-BY: tests/terrain/terrain_normalmap_directed.cpp` — **that file
  does not exist.** A fresh phantom citation, the exact pattern
  `reports/PHANTOM-CITATIONS-AUDIT.md` counted 35 of, added the day the audit
  landed.
* `zhao_terrain_normalmap.sv` is on disk under `fpga/rtl/` but appears in
  neither `design/prod_manifest.yml` nor `design/blocks.yml`. The manifest
  checker's stated rule ("EVERY module under fpga/rtl appears exactly once")
  will flag it, possibly during the fit that is running now. It needs an
  `excluded: unused` line (or the ledger entry) — not made here per this
  task's constraints.

**What the draft got right, kept:** the split itself; the axis-aligned frame;
the {s8 dz, s8 dx} texel with the exact power-of-two cancellation; no Y
component; strength as a declared look-tuned knob; negative base preserved
until the final clamp.

---

## Options considered

**A. Second TMU sample per fragment (the textbook fetch).** Terrain samples
276,480/frame double to 552,960; the known TMU subtotal goes from 541,640 to
818,120 before creatures and beams, against a cache that was just rebuilt and
closed at 98.66 MHz. Needs a normal-texel format in the TMU (new decode), a
resident mip chain, residency traffic. Cost: ~100-200 ALM of decode plus a
frame-time and cache-pressure bill that is the single most expensive way to
buy 16 bits per fragment. REJECTED — but note TEXJOIN v2 already speaks three
samples per fragment, so if terrain ever needs a UNIQUE (non-tiling) normal
map, the escalation path exists without rearchitecture.

**B. Pack the normal into unused bits of the terrain texel.** Terrain albedo
is CLUT8 — 8 bits, fully used; there are no unused bits. Moving terrain to a
16-bit format to smuggle 8 bits of normal doubles terrain texture footprint
and still yields only 4+4 bits. REJECTED on arithmetic.

**C. Always-resident tiling detail tile in M10K, addressed by the terrain UV
already at the fragment (CHOSEN).** Terrain top-cell UV is world XZ over a
power-of-two pitch (`reference/src/zrender/terrain.cpp` UV law), so the
perspective-correct UV that addresses the albedo addresses a world-anchored
detail tile with zero new interpolants. 64x64x16b = 8 M10K of the ~250 spare.
Zero TMU traffic, zero cache pressure, II=1, fixed latency. Terrain detail is
tiling noise; uniqueness stays the albedo's job. **This is still the real
per-pixel path** — the same math, the same per-fragment perturbation, the same
response to moving suns; only the texel store is specialised. It forfeits
unique-per-texel normals, which option A can restore later if ever wanted.

**D. Bake detail shading into the albedo.** Free, and dead under a moving
sun — the owner's "huge gain" is precisely that the relief responds to light.
Kept only as the post-cut fallback.

**E. Denser tessellation.** Real geometry, real cost in the projector (each
lattice vertex is already projected up to six times pre-WCACHE), and the
detail frequencies wanted here are far below cell size. REJECTED for this
purpose.

---

## The chosen architecture — three pieces, two of them cheap

    TESS ──▶ NORMALS ──▶ TERRAIN.SHADE ──▶ (flat lit colour, per triangle)
                          [REQUIRED ANYWAY]        │ joins PROJECT's output
                                                   ▼ by in-order src_id skip
    PROJECT ──▶ CLIP ──▶ … ──▶ raster attribute path (existing, unchanged)
                                                   │ frag stream with UV
                              ┌────────────────────┤
                              ▼                    ▼
                    TERRAIN.NORMALMAP         TEXJOIN v2 (unchanged)
                    (detail delta, M10K tile)      │
                              │ 16x9 align FIFO    │ retire, alloc order
                              └───────▶ fragment-packet assembly glue:
                                        v'_c = clamp_u8(v_c + delta)
                                                   │
                                              RASTER.FRAGMENT (unchanged)

### Piece 1 — TERRAIN.SHADE (per-triangle base; NOT a normal-map cost)

New block, PLANNED, needs its own ledger entry and contract. Sits between
TERRAIN.NORMALS and the raster attribute path; consumes the face normal and
the sun; emits the flat lit colour (ratified §4a law:
`lit_c = sat_u8(ambient_c + rescale_u(sun_c*ndl, 8))` with
`ndl = max(0, dot(n,L)/|n|)`).

The divide-by-|n| follows the ratified table+NR precedent (`spec/qformats.md`
§6.1 `rcp_u24`, §7.4 `normalize_approx`), not a serial divider: CLZ-normalise
`n2 = x²+y²+z²` (u65 accumulated exactly; even shift), rsqrt via a 512x18
M10K table + one Newton step in DSPs, multiply by `dot(n,L)`, one rescale.
II=1, latency ~8 — keeps pace with PROJECT's 1 triangle/3 clocks with margin.
Its oracle (`zref::terrain::shade_base`, PLANNED AND NOT WRITTEN) is defined
as that exact pipeline with a fixgen-frozen table and a declared error bound
against `shade_flat_tri`'s exact arithmetic, the §6.1/§7.4 pattern — bound to
be MEASURED in the oracle tests, expected ~1-2 unit8 LSB, invisible at 240p.

Join to the raster: PROJECT's payload rides ~38 stages, so widening it by
24 bits costs ~900 flops; instead SHADE's output enters a small in-order FIFO
joined at PROJECT's output by src_id (both streams are in order; PROJECT drops
behind-eye triangles whole, so the join pops-and-skips on id mismatch). The
flat colour then enters the existing per-vertex colour attribute mechanism
(same colour on all three vertices — planes with zero gradient). **This wiring
gap exists today independent of normal maps: terrain currently has NO colour
path into the rasterizer at all** (`zhao_terrain_project`'s output packet
carries positions, depth, view, mosaic triple — no rgb).

### Piece 2 — TERRAIN.NORMALMAP (the detail delta; the cuttable part)

Exactly as contracted in `design/contracts/TERRAIN.NORMALMAP.md`: bump-in-wire
on the fragment stream beside TEXJOIN, its own 64x64x16 M10K tile addressed by
`u_raw[uv_shift+5:uv_shift]`, one MAC, one rounding:
`delta = rescale_s(strength * SUM_suns(dx*sun_x + dz*sun_z), 23)`, s9 out.

### Piece 3 — the application seam (~25 ALM, no RASTER.FRAGMENT change)

The naive application (`unit_mul(texel, vert)` plus a second product
`texel*delta`) factors: `t*v + t*delta = t*(v + delta)`. So the delta is
added to the three vertex-colour lanes BEFORE the existing modulate, at the
fragment-packet assembly glue after TEXJOIN retirement: three 9-bit saturating
adds. RASTER.FRAGMENT's ports, full state word (all 32 bits are allocated —
there is no room for a DETAIL_SHADE bit, and none is needed) and single
multiplier layer are untouched; delta = 0 is a bit-exact no-op, which is also
the off switch and the cut seam. Alignment rides a 16x9 FIFO written at
TEXJOIN accept, popped at retire — retirement is allocation order, so order
matches by construction. Timing risk: one adder layer added ahead of the
modulate at the s0->s1 transfer; if the fit objects, the add moves one stage
earlier into the glue register, costing nothing.

---

## The six questions, answered

### Q1 — Is the split sound? Does flat-per-triangle base + per-fragment detail read?

The algebra is exact (linearity of the dot product); the only approximation
is the declared un-renormalisation, zero at strength 0. The premise holds:
terrain IS flat-shaded per triangle — `shade_flat_tri` is the ratified law and
the reference shades flat; vertex normals are explicitly unratified
(TERRAIN.NORMALS contract, Notes 1).

Does it read? The detail term is CONTINUOUS across triangle edges — the tile
is world-anchored through UV, which is linear in world XZ, and perspective
interpolation of a world-linear function is exact — so **detail normals cannot
add faceting**: facet steps stay exactly as visible as they are today, with
continuous high-frequency variation superimposed. High-frequency structure
typically masks low-frequency banding rather than accenting it. But that is a
prediction, not a look. **The art-law gate: implement the amended oracle in
the zref renderer FIRST, render the island under a moving sun, and the owner
looks — before any RTL is written.** If the facets offend once detail is
present, the escalation is an interpolated per-vertex base shade riding the
EXISTING generic attribute machinery (one more plane through
attrsetup/attrinterp/attrdiv) — which first requires ratifying the vertex-
normal averaging rule that TERRAIN.NORMALS' contract deliberately left open.
The detail path is unchanged under that escalation; nothing here is thrown
away.

### Q2 — Where does the shade get applied?

Named above: the fragment-packet assembly glue between TEXJOIN v2 retirement
and RASTER.FRAGMENT's `frag_*` port. New signal: one s9 delta per fragment,
aligned by a 16x9 FIFO. Application: `v'_c = clamp_u8(v_c + delta)` on the
vertex-colour lanes, then the untouched `unit_mul`. Cost: ~25 ALM of adders
plus ~75 ALM of FIFO. TEXJOIN does NOT carry a second texture sample and does
NOT widen its context; the texture island that just closed at 98.66 MHz is
not touched. RASTER.FRAGMENT is not touched.

### Q3 — Where does the detail texel come from?

The always-resident M10K tile (option C), addressed by the fragment's
existing perspective-correct terrain UV. Zero TMU samples, zero cache
pressure, 8 M10Ks. The second-TMU-sample path (option A) was evaluated and
rejected on the frame-time arithmetic above; it remains the escalation if
unique-per-texel normals are ever wanted, and TEXJOIN v2's three-sample
protocol already accommodates it.

### Q4 — How many bits of normal at 240p?

Justified against the read, not precision: the delta lands in unit8 colour
lanes (256 levels), scaled by strength (typical 0.25-0.5 by eye ->
detail excursion ±32..±128 levels) and by |sun_xz| <= 1. An s8 axis (256
levels) already exceeds what the output can display through that chain, and
{s8, s8} = 16 bits keeps the exact power-of-two cancellation AND matches the
M10K geometry. MORE bits (s10 in a 20-bit M10K word) would be free storage
but purchase nothing visible; FEWER (s4+s4 nibbles) halves the tile to 4
M10Ks, keeps exact scaling (raw/8 is still a power of two), and is the
declared cut lever — at 240p, s4 (16 angle steps per axis) is likely still
invisible against CLUT8 albedo, but that is a look call, so s8 ships first
and s4 is tried by eye if M10Ks get tight.

### Q5 — Multiple moving suns

Per extra sun: per-fragment, one more s8xs16 MAC pair before the shared
rounding (~half a DSP, ~30 ALM); per-triangle in TERRAIN.SHADE, one more
dot(n,L) and one more table multiply (~2-3 DSP, ~80 ALM — the rsqrt and |n|
are sun-independent and shared); one more config word. No M10K. So SUNS=2 is
roughly +3 DSP / +120 ALM total — affordable. The blocker is law, not
hardware: `SetEnvironment 0x0311` carries ONE sun direction/colour, and
multiple suns need an ABI amendment (a second record, or the documented
same-bytes pad-field mechanism). **Sane default: build SUNS=1, keep the
accumulate-before-round datapath shape so SUNS=2 is a parameter plus a spec
amendment,** and let the identity requirement pull the amendment when the sky
work is ready for it.

### Q6 — The cut plan

The cut seam is the delta wire, whose zero state is bit-exact identity.

1. **Soft cut (free, reversible per frame):** `strength = 0`. The look
   reverts exactly; the hardware stays. This is also how the owner A/Bs it.
2. **Hardware trim, in order:** s4+s4 texel (-4 M10K); 32x32 tile (-6 M10K
   from baseline); SUNS stays 1 (no cost, already default).
3. **Full cut:** delete TERRAIN.NORMALMAP, the alignment FIFO and the three
   adders; tie delta's consumers to the pre-add wire. Reclaims ~375 ALM,
   2 DSP, 8 M10K. **TERRAIN.SHADE stays** — it is the terrain's light, not a
   normal-map organ.
4. **Fallback that keeps some gain:** bake the detail's diffuse read into the
   albedo CLUT tiles (option D) — keeps the texture busy-ness (~half the
   visual gain, subjectively), loses all response to moving suns. State this
   to the owner as what "cut" actually costs the look.

---

## Resource estimate, arithmetic shown

Rules of thumb used: 2 register bits/ALM or 2 adder bits/ALM on Cyclone V;
an 8x8 fabric multiply ~35 ALM; DSP does 27x27 or two 18x18.

**TERRAIN.SHADE (required regardless — bill to terrain lighting):**

| piece | ALM | DSP | M10K |
|---|---|---|---|
| n2 = three 32x32 squares + u65 adder tree | 100 | 3 | |
| CLZ64 + normalise shift | 80 | | |
| rsqrt table + 1 Newton step (two 18x18) | 60 | 2 | 1 |
| dot(n,L): three 32x16 + adder | 60 | 3 | |
| final product + rescale + §4a colour fold | 80 | 2 | |
| pipeline registers, valids, cfg | 150 | | |
| src_id join FIFO + skip logic at PROJECT out | 200 | | |
| **subtotal** | **~730** | **~10** | **1** |

**TERRAIN.NORMALMAP + seam (the normal-map delta — what the owner is buying):**

| piece | ALM | DSP | M10K |
|---|---|---|---|
| tile RAM 64x64x16 + upload port | 40 | | 8 |
| address gen (uv_shift mux) | 30 | | |
| MAC + rescale + s9 saturate | 80 | 2 | |
| skid + pipeline + cfg | 130 | | |
| alignment FIFO 16x9 | 75 | | |
| three saturating adds at assembly | 25 | | |
| **subtotal** | **~380** | **2** | **8** |

**Grand total ~1,110 ALM (2.6% of device, ~15% of the 7,578-ALM remaining
headroom), ~12 DSP (11% of device — the deliberate DSP-for-ALM trade), 9 M10K
(1.6%).** Contract ceilings: NORMALMAP 500/3/9 (set), SHADE 1,000/12/2 (to be
set in its own contract). Fabric being the tight axis, every multiply here is
in DSP and every bulk bit is in M10K; the ALM spend is registers and adders
only.

**Measurement plan for a clean delta:** three fits of the production top —
(a) as now, (b) + TERRAIN.SHADE, (c) + TERRAIN.NORMALMAP + seam. (b)-(a) is
the terrain-lighting bill; (c)-(b) is the normal-map bill and the number the
owner asked for. Both blocks enter `prod_manifest.yml` as separate `top`
entries so the hierarchical report attributes them.

---

## Oracle amendments required (before any RTL)

1. `normalmap_length`: accumulate the three squares in unsigned/128-bit (the
   reference already uses `__int128` in `shade_flat_tri`'s dot; the same tier
   is available) — or clamp per the 67-bit note in TERRAIN.NORMALS' contract.
   Rails are legal input; UB on legal input is a defect, full stop.
2. `normalmap_detail`: truncating `/32768` -> `rescale_s(., 23)` over the
   summed-suns product, one rounding, s9 saturation.
3. `normalmap_shade`/`normalmap_shade_multi`: superseded by
   `normalmap_apply(v_c, delta) = clamp_u8(v_c + delta)` (PLANNED); the
   ambient floor is withdrawn in favour of ratified §4a additive ambient — if
   the floor is wanted on the look, it goes through a sky_and_beams amendment.
4. `normalmap_base`/`normalmap_length` migrate to a TERRAIN.SHADE oracle
   (`zref::terrain::shade_base`, PLANNED) defined as the table+NR pipeline
   with a fixgen-frozen table and a measured, declared error bound against
   `shade_flat_tri` — the §6.1/§7.4 pattern exactly.
5. Then: implement in the zref renderer, render the island, moving sun,
   240p, and LOOK before building anything in RTL.

## Recommended repo changes (not made here, per task constraints)

* `design/prod_manifest.yml`: add `zhao_terrain_normalmap: unused` under
  `excluded` NOW (the checker will otherwise flag the draft file — possibly
  in the fit currently running); promote to `top` when rebuilt to contract.
* `design/blocks.yml`: register TERRAIN.NORMALMAP (contract exists) and
  TERRAIN.SHADE (contract to be written) with `cut_order` set for NORMALMAP
  and null for SHADE.
* `fpga/rtl/terrain/zhao_terrain_normalmap.sv`: rewrite to the contract; the
  current draft's triangle engine moves to TERRAIN.SHADE's file in table+NR
  form; delete the phantom ENFORCED-BY line.
* `spec/` follow-ups when ratifying: the detail-strength/uv_shift config's ABI
  home (a `SetTerrainDetail` word or `SetEnvironment` pad bytes), the s1.15
  sun-XZ derivation from the record's `angle16` yaw/pitch (one per-frame
  conversion, not per-fragment), and — someday — the second sun.
