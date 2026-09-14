# Contract — TEXTURE.COMBINE (R9 fixed material combiner)

> Ledger: `design/blocks.yml` · gpu clock · Packet-B required candidate:
> `zhao_texture_material_combine_v3`
> Required reference authority: `zref::material::combine`

## Purpose and authority

TEXTURE.COMBINE consumes the texture owner's committed zero-to-three typed TMU
planes plus the independent typed AUX plane and produces the one complete
48-bit texture result accepted by `RASTER.FRAGMENT`.

Owner ruling R9 in `reports/MATERIAL_ARCHITECTURE.md` is the sole arithmetic
authority. Earlier six-recipe prose, `zhao_texture_material_combine_v1/v2`, the
old island, and their matching historical oracle are retained only as migration
oracles. Agreement between stale implementations cannot override R9.

This block does not sample, resolve a material binding, blend the framebuffer,
apply fog, quantise toon bands, or consume AUX tag/strength as colour. It owns
only fixed material arithmetic, required-plane status/index reduction, and its
bounded paired-phase schedule.

## Clock and reset

Single `gpu` clock. Reset is asynchronous active-low assert and synchronous
release. It clears all valid/occupancy/scheduler state and all counters. Source
planes are owned by `zhao_texture_v3own`; the combiner owns no texture memory.

## Inputs and outputs

Parameters are `NCTX=8`, `TAGW=14`, `READ_LATE=0`, and `SLOTW=6` by default.
One held job arrives on `f_valid_i/f_ready_o` with:

```text
f_sample_count_i[1:0]
f_recipe_i[2:0]
f_weight_i[7:0]
f_aux_required_i
f_base_rgb_i[23:0]
f_base_a_i[7:0]
f_tag_i[TAGW-1:0]
```

In copy mode, the four complete 48-bit planes are `f_s0_i`, `f_s1_i`,
`f_s2_i`, and `f_aux_i`. In read-late mode the job supplies `f_slot_i`; the
combiner emits `src_rd_valid_o/src_rd_slot_o` and receives the synchronous
`src_s0_i/src_s1_i/src_s2_i/src_aux_i` values. The mode changes carriage, not
arithmetic, phase demand, status, or ordering.

The held output is `o_valid_o/o_ready_i` with `o_result_o[47:0]` and unchanged
`o_tag_o`. Exact aliases are `o_rgb_o`, `o_a_o`, `o_raw_index_o`,
`o_status_o`, and `o_refused_o`.

```text
result[47:40] = status
result[39:32] = sample-0 raw index
result[31:24] = alpha
result[23:0]  = RGB
out_refused   = status[0]
```

All job fields, source planes, read-late identity, and output fields hold
field-for-field under backpressure. No valid is a function of its own ready.
The accepted tag is never reconstructed or reordered.

## Arithmetic helpers

The helpers operate independently on each RGB byte. Every multiply/add is
widened before shift or saturation:

```text
rescale_s(x,8)   = (x + 128) >>> 8
unit_mul8(a,b)   = (a*b + 128) >> 8
modulate2x8(a,b) = sat_u8((a*b + 64) >> 7)
lerp8(a,b,w)     = sat_u8(a + rescale_s((b-a)*w,8))
```

`rescale_s` uses a signed arithmetic shift, so exact negative and positive ties
round toward positive infinity. MODULATE2X has one direct rounding; applying
unit multiply and then doubling is a different and rejected law. Unit8 remains
raw/256, so 255 is not mathematical 1.0.

## Eight recipes and exact counts

| ID | recipe | legal count | RGB | alpha |
|---:|---|---:|---|---|
| 0 | PASSTHRU | 0 or 1 | count 0: admitted base; count 1: `s0.rgb` | count 0: admitted base; count 1: `s0.a` |
| 1 | MODULATE | exactly 2 | `unit_mul8(s0.rgb,s1.rgb)` | `s0.a` |
| 2 | MODULATE2X | exactly 2 | `modulate2x8(s0.rgb,s1.rgb)` | `s0.a` |
| 3 | LERP | exactly 2 | `lerp8(s0.rgb,s1.rgb,weight)` | `s0.a` |
| 4 | ADD_SAT | exactly 2 | `sat_u8(s0.rgb+s1.rgb)` | `s0.a` |
| 5 | MASK | exactly 2 | `s0.rgb` | `unit_mul8(s0.a,s1.a)` |
| 6 | TERRAIN_DETAIL_LIGHT | exactly 3 | `unit_mul8(modulate2x8(s0.rgb,s1.rgb),s2.rgb)` | `s0.a` |
| 7 | TERRAIN_DETAIL_MASK | exactly 3 | `modulate2x8(s0.rgb,s1.rgb)` | `unit_mul8(s0.a,s2.a)` |

Count zero is legal only for PASSTHRU and reads no TMU sample. It returns the
admitted base, never a null or stale `s0`. Recipes 1–4 do not multiply alpha.
MASK is an alpha product, not a nonzero-alpha Boolean gate. Both terrain recipes
use MODULATE2X for their first detail layer.

Every count other than the exact row above is malformed. It becomes
`material_refused`; it does not degrade to PASSTHRU or read an unrequested
plane. The owner/top separately ensures every declared required source receives
logical issue and a terminal refusal so the malformed fragment still drains.

## Required planes, AUX, status, and raw index

The required sample mask is a function of count:

```text
0 -> 000
1 -> 001
2 -> 011
3 -> 111
required_mask = {aux_required,sample2,sample1,sample0}
```

A TMU plane is `{status8,raw_index8,alpha8,RGB24}`. AUX is separately typed as
`{status8,tag8,strength8,24'd0}`. The combiner reads only AUX status. AUX never
occupies or substitutes for sample 2, and no recipe consumes AUX tag or strength
as RGB, alpha, weight, or raw index.

Final status is the bitwise OR of only committed required-plane statuses plus
`{7'b0,material_refused}`. Bits `[7:1]` are reserved-zero at Packet-B producers
but remain eight-bit data through the reduction. Unrequested planes and
uncommitted storage are never consulted.

Final raw index is zero at count zero and otherwise exactly the committed sample
0 raw index. Samples 1/2, AUX, palette RGB, and recipe arithmetic cannot replace
it.

When final status is nonzero or material is malformed, the terminal visible
value is exactly RGB `24'hFF00FF`, alpha `8'hFF`; status and the sample-0 index
rule remain intact. The result still handshakes normally so the owner releases.
The top's frame-fault mechanism observes every nonzero status and material
refusal even when no useful colour is produced.

## Scheduler and cadence

The required implementation has one physical paired phase engine and may issue
at most one paired phase per clock. One phase can perform up to two independent
product jobs. The exact legal schedule is:

| recipe | paired phases | meaningful product jobs |
|---|---:|---:|
| PASSTHRU | 1 | 0 |
| MODULATE | 2 | 3 |
| MODULATE2X | 2 | 3 |
| LERP | 2 | 3 |
| ADD_SAT | 1 | 0 |
| MASK | 1 | 1 |
| TERRAIN_DETAIL_LIGHT | 3 | 6 |
| TERRAIN_DETAIL_MASK | 2 | 4 |

A refused/malformed or source-status-failed job takes one terminal phase. Define:

```text
J1 = PASSTHRU, ADD_SAT, MASK, or any early refused/status-failed job
J2 = valid MODULATE, MODULATE2X, LERP, or TERRAIN_DETAIL_MASK job
J3 = valid TERRAIN_DETAIL_LIGHT job
phase_demand = J1 + 2*J2 + 3*J3
```

A backlogged runnable phase set must issue one phase per post-fill clock until it
drains. A homogeneous one-phase stream can complete one material per clock after
fill; two- and three-phase recipes consume two and three issue clocks per job.
No one-material-job-per-clock claim is legal for a multi-phase recipe.

The finished `{status,index,scratch}` row and `{context,phase,final}` control are
registered once between arithmetic M and the actual scratch/completion RAM write.
This adds one terminal clock per phase without reducing II=1. Continuation/done
ownership and `phases_completed_o` advance only on that WB write edge; product
and saturation accounting remain facts of the preceding M edge. `idle_o` includes
the WB valid bit.

`idle_o` must be high only when accepted context, runnable phase state,
payload/scratch reads, arithmetic/writeback, continuations, completion state,
and held output are all empty.

## Counters

All are 32-bit reset-zero modulo counters and move only on their named event:

* `jobs_accepted_o`: `f_valid_i && f_ready_o`;
* `jobs_completed_o`: `o_valid_o && o_ready_i`, once per material;
* `phases_issued_o`: every accepted physical paired-phase launch;
* `phases_completed_o`: every corresponding physical phase result/writeback;
* `refused_material_o`: accepted jobs with an illegal recipe/count pairing;
* `saturated_add_o`: accepted ADD_SAT jobs whose RGB result clips;
* `saturated_mul2x_o`: accepted MODULATE2X/detail jobs whose direct 2x result clips;
* `jobs_by_recipe_o[8]`: meaningful physical product jobs, accumulated in the
  element selected by the accepted recipe; per-material demand is exactly
  `0/3/3/3/0/1/6/4`, not one increment per accepted material;

At complete drain the exact accounting is:

```text
CJ = jobs_accepted_o
CD = jobs_completed_o
PI = phases_issued_o
PC = phases_completed_o
CJ == CD
PI == PC == phase_demand
```

Saturation counters count the documented job event rather than each clipping
channel. Stalled outputs and continuations cannot increment twice. Issued and
completed phase counters are distinct instruments; a dropped writeback cannot
make both move together.

## Reference and compatibility boundary

`reference/include/zref/zref_material.hpp` must implement R9
`zref::material::combine`. New V3 tests and product decisions use it.

Historical V1/V2/old-island tests use the retained
`tests/texture/legacy_material_v2_oracle.hpp` under
`zref::legacy_material_v2`. Outside PASSTHRU/count-1/no-AUX success, disagreement
between that oracle and R9 is expected evidence of the superseded arithmetic,
not permission to change R9.

## Verification

Packet B requires:

1. hand-computed corners and exhaustive byte boundaries for all helpers,
   including negative LERP ties and direct MODULATE2X cases that distinguish it
   from unit-multiply-then-double;
2. every recipe and exact legal count, plus every count mismatch;
3. alpha-specific controls for recipes 1–7 and both MASK variants;
4. count-zero admitted-base behavior with no sample read;
5. required-status OR, reserved-bit transport, loud terminal error, and exact
   sample-0 raw index under palette and status variation;
6. AUX present/absent and failing while proving tag/strength never influence
   arithmetic or sample 2;
7. copy/read-late equivalence and full tag/result hold under independent stalls;
8. homogeneous and mixed cadence proving jobs and exact phase demand;
9. renamed committed mutants for every stale arithmetic family, count fallback,
   raw-index replacement, status omission, AUX-as-sample-2, and phase reissue.

Every detector must have a demonstrated positive control. Correct R9 tests must
not use agreement with the old combiner as their oracle.

## Evidence boundary

Packet B's gate must prove functional arithmetic, identity/status/index
carriage, backpressure, and cadence. Packet B contains no Quartus fit and banks
no ALM, DSP, M10K, or Fmax result. The former 650-ALM/0–2-DSP line is a budget
constraint, not evidence. Physical claims wait for G8A after Packets A–E are
green.
