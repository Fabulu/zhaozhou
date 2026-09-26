# Contract — GEOM.WARP (Warp application stage)

> Ledger: `design/blocks.yml` · owner ZH-045 · phase 9 · maturity SPECIFIED
> · **mandatory** (owner ruling R7; the 2026-08-31 §6.3 deferral was revoked
> 2026-09-18)

## Authority, and what this contract does and does not claim

This contract is written from the owner directive
`reports/Zhaozhou_GEOM_WARP_Architecture_2026-09-20.txt` (owner commit
`4c256137`, Fabian, 2026-09-20), whose decisions **W01–W18** are ratified in
`reports/OWNER-RATIFICATION-20260920-WARP.md`. Section numbers in square
brackets below are that document's.

**Superseded text, annotated rather than deleted (conflict C02).** Every
section of this contract previously read *"Deliberately unwritten. This block is
DEFERRED by owner ruling 2026-08-31 section 6.3 (cut-order 5)."* That deferral
was correct when written and is now revoked. The reasoning it gave — that
specifying a block nobody is building "would make the design look decided when
it is not" — is still a good rule, and it is why the status table below exists:
this contract states what is SPECIFIED, what is BUILT, and what is BLOCKED, and
never lets the three read alike.

**STATUS AT THIS COMMIT.**

| part | state |
|---|---|
| Semantics (lanes, application law, bounds, failure taxonomy) | **RATIFIED** — W01–W18 |
| Scalar reference `zref::GeomWarp` | **BUILT AND TESTED** — `reference/include/zref/zref_geom_warp.hpp`, **102** directed checks, 3 fired mutants |
| `DrawWarpedForm 0x0304` ABI | **BUILT 2026-09-21** — `spec/commands.zidl`, 96-byte record / 80-byte payload, generated through every consumer. The generator's own pad map (`ZHAO_PADS_DRAW_WARPED_FORM = {62,63,76,77,78,79}`) agrees with directive 6.1 byte for byte. Of the 27 files in `tests/abi/golden/`, exactly two moved — the new record and `zcap_minimal.zcap`'s two sha fields — so `DrawForm`'s byte-for-byte survival under W04 is **measured, not promised**. |
| CMD.EXEC's 0x0304 arm + per-draw snapshot | **BUILT AND TESTED 2026-09-21** — `zhao_cmd_exec`'s capture arm, the `wq` descriptor sidecar (directive 7.1's split: one `warp_enabled` bit in the draw item, the 456-bit snapshot in a RAM-shaped array on `dq`'s own pointers), and the DRAW_INVALID refusal. `cmd_exec_directed` 850 → **954 checks**, cases 42–48. Case 48 — two warped draws differing only in their snapshot — was **watched to fail** against a planted global-register mutation (9 red, draw 0 receiving draw 1's program) and then watched to pass restored. |
| `zhao_geom_warp.sv` and the application stage | **BUILT AND TESTED 2026-09-20** — `fpga/rtl/geometry/zhao_geom_warp.sv`, 2,468 directed checks against `zref::geom_warp::apply_outputs`, every counter seen to fire, one committed mutant with a measured fire rate |
| The join (descriptor sidecar, meshlet poison/drain) | **NOT BUILT** |
| Composition into `zhao_console_core` | **BLOCKED** — on P1, P2, P7 and the absent command; see the prerequisite table |
| Throughput / resources | **UNMEASURED** — no fit, no cycle count, no claim. **PHYSICAL FIT PENDING.** |

**THE CHECK COUNT WAS WRONG IN TWO PLACES AND IS CORRECTED BY MEASUREMENT.**
This table and §12 both read *95 checks*; `design/blocks.yml` said 95 too.
Built and run 2026-09-20: `geom_warp_reference_directed: 102 checks, 0 failed`.
Ruling R102 said 102 all along. A number repeated from a draft is not a
measurement, and three files agreeing with each other is not corroboration when
none of them ran the binary.

**GEOM.WARP is `BUILT BUT NOT CONNECTED` in the completion register as of
2026-09-20, and the register TOTAL DID NOT MOVE: 21 → 21.**
`completion_register.py:1480-1482` sums `built_not_connected` and `unbuilt`
together, so reclassification cancels exactly. [W18]: "A built block, zero gap
counter, or a picture alone is not sufficient." **The Field request port is NOT
tied off** — "not connected" and "tied off" are different statements, and only
the first is true here.

**AND THE REGISTER CANNOT TELL THE DIFFERENCE.** There is no structural scan for
a port tied to a constant anywhere in `completion_register.py`; its tie-off list
is parsed out of `zhao_console_core.sv`'s own `INCOMPLETE -- TIED OFF` header
comment block (`:67-244`). A composition that tied this block's Field port off
would read **CONNECTED** and drop the total to 20 — green, and wrong. The guard
is this contract, the ledger note and the packet protocol. Not the instrument.

## 1 Purpose and exclusions

Apply validated **W-profile** displacement and replacement-normal results to
post-skin world vertices, **once across both views**, by submitting the Warp
input record to the **one existing Field v3 execution fabric**.

GEOM.WARP is **VIEW-INDEPENDENT, POST-SKIN, PRE-LIGHT / PRE-PROJECTION**. It
adds the returned displacement to the skinned world position, adopts the
returned normal, and publishes one coherent vertex.

It does **not** [directive §A]: instantiate a private `zhao_field_seq`, a v2
fallback or a new v3 engine; implement its own SIN, RING, normalization, noise
or interpreter; add a third projector or another raster path; treat GEOM.LOOM's
transform matrices as vertices; evaluate the same vertex separately for the two
cameras; change DrawForm or raw vertex format 0 by reinterpreting reserved
bytes; assume `source_id` is a vertex identity; or own any material, projection
or simulation state.

**Scope exclusions are not missing Warp function** [§1.6]. Skeletal pose and
cape animation, the existing species deformation rules, Transform Loom,
procedural geometry, particle Flow and terrain Earth all coexist unchanged. No
existing behaviour acquires a Warp unless the application issues the new
command.

## 2 Clock and reset semantics

The existing `gpu_clk` and the production seam's reset style. No new clock
domain. All transfers registered.

Reset and controlled abort **drain or invalidate** outstanding work without
letting a late write reach reused storage [§15.5]. A Warp-only reset must not
reset Earth or Flow; if only the requester is reset, an epoch/tombstone
mechanism refuses late results before they touch new rows. GEOM.WARP never
invents a global engine-reset side effect.

## 3 Input and output records

**Fifteen canonical input lanes** [W01, §5.1] — and the count is fifteen, not
fourteen; see `spec/form/field-ir.md` §7.1 and its correction note.

| lane | name | type | owner |
|---|---|---|---|
| 0–2 | px, py, pz | fx | GEOM.SKIN world position |
| 3–5 | nx, ny, nz | fx | GEOM.SKIN.NORM reduced direction — **not unit** |
| 6–9 | a0–a3 | fx | INLINE4 or STREAM4, §8 |
| 10 | time | u32 | **this** DrawWarpedForm's tick, never a clock read |
| 11–14 | p0–p3 | fx | **this** DrawWarpedForm's parameters |

**Six output lanes**: `dx, dy, dz, nx′, ny′, nz′`. Their physical register
locations come from the decoded program's **output map**. They are **NOT**
assumed to be R15..R20 or one contiguous window [§5.2]; the canonical decoder
owns that map's legality.

Per-draw descriptor, tagged vertex identity and the coherent post-Warp result
are as §13.1–13.6 of the directive.

## 4 Backpressure rules

Reserve join, result and memory storage **before** acceptance [§14.3]. Hold
valid payloads stable while `valid && !ready`. Position and normal are accepted
by their two consumers **exactly once each**.

The downstream fork uses either an atomic fork whose valids are gated by the
*other* consumer's ready, or independent consumed bits in the output hold. It
must **not** simply AND the readys while leaving both valids asserted — [§11.6]
records that this previously duplicated accepted work under backpressure
elsewhere in this console.

No unbounded ready chain from Field through Warp through GROUP_SEQ through the
projector [§19.4].

**The splice constraint, measured in the tree rather than assumed.** The
existing palette→skin AND-fork (`zhao_console_core.sv`) is safe precisely
because neither consumer's `ready` is a function of its own `valid`
(`zhao_geom_skin`: `!busy && (!o_valid_o || o_ready_i)`; `zhao_geom_skin_norm`:
`st_q == S_IDLE`). Any block spliced in **must** keep that property or the
fork's no-deadlock argument stops being true.

## 5 Memory ownership

Bounded descriptor and join state. Optional read-only WARP_ATTRIBUTES reads
through the **existing geometry memory adapter** as an additional *logical*
requester — no second physical bridge, VRAM client identity or resource
directory law [§8.5]. Program, plan and tables are held under explicit leases
in the **common** Field service. No private Field program store, and no second
projection arena.

Physical RAM ports are matched, not inferred [§19.3]: independently written
position/normal/attribute arrays are separated or their writes explicitly
scheduled. An array named RAM does not provide arbitrary simultaneous
addresses.

## 6 Q formats and rounding

* **Position** [W03, §5.3]: `Pout = clamp(sign_extend(Pin) + sign_extend(d),
  INT32_MIN, INT32_MAX)`, formed in at least 33 bits. The existing canonical
  saturating add (`zref::fx_add`). **No** multiplication by dt, no hidden gain,
  no second application of the instance transform.
* **Application saturation is reported SEPARATELY from Field-program
  saturation** [§5.3]. The visible status may OR them; the individual causes
  stay inspectable. A `resp_status != 0` test **must not** turn a defined
  saturated Field answer into a dropped vertex [§5.5].
* **Normal** [W02, W03, §5.4]: the input normal is the existing range-reduced,
  **non-unit** world direction. The program supplies a **replacement** normal.
  Before LIGHT: zero iff all three words are zero; otherwise the **same** common
  arithmetic-right-shift range reduction `zref::skin_world_normal` uses, until
  `max(abs(component)) < 2^30`, with a **widened unsigned** magnitude so
  `abs(INT32_MIN)` does not overflow. At signed-32 width this needs **at most
  two** shifts — verified, not repeated: 2³¹ ≫ 1 = 2³⁰ (still at the ceiling),
  ≫ 1 again = 2²⁹ (below it).
* No extra normalization is introduced on the disabled path. An identity Warp
  (`d = 0`, `n_out = n_in`) over a lawful upstream normal performs **no**
  additional shift and lighting is exact.
* **The normal-deformation law belongs to the Warp program** [W17], not to this
  shell. A program needing a unit normal uses the Field's explicit
  `NORMALIZE3`. The adapter does **not** infer derivatives from position
  samples.

## 7 Latency

Variable with the selected program and the shared execution and memory paths.
The application pipeline's latency is listed **separately** from the
evaluator's. Liveness assumptions are stated explicitly: memory and downstream
consumers must eventually respond, and the Field program is bounded [§14.4]. No
bounded simulation is an unconditional proof of arbitrary external response
time.

## 8 Target throughput

The ledger's declared target is **1 warped vertex per clock**, and [C07, W13]
require three claims to be kept apart and never substituted for one another:

1. **Application II** — the saturating add / normal adaptation / output stage,
   goal II = 1 with ready results and a ready sink;
2. **Field evaluation throughput** — a property of the actual program and the
   shared host;
3. **Whole-frame affordability** — traffic, queues, every other profile,
   geometry, texture, post, and the measured clock.

**Never quote bypass speed as active program speed, and never rewrite a missed
target to pass it.** A miss is annotated honestly and escalated as a throughput
contract amendment [§18.1].

**What is measured today:** nothing. See §15.

## 9 Overflow and malformed-input behaviour

Failure taxonomy [§17], with **distinct** saturating counters — not one
`warp_error` that loses every cause:

| class | disposition |
|---|---|
| `DRAW_INVALID` (bad attribute mode, reserved flags, negative bound, wrong signature) | refuse **before** emitting meshlets |
| `PROGRAM_NOT_RESIDENT` / `STALE_BINDING` / `PLAN_NOT_READY` | fail draw admission; never execute another slot's plan |
| `ATTR_*` (not resident / mesh mismatch / bad header / denied) | refuse prefetch; drain issued reads; release credits |
| `FIELD_EXECUTION_FAULT` / `OUTPUT_INCOMPLETE` / `RESPONSE_IDENTITY_FAULT` | **poison** the batch; drain; do not display |
| `DISPLACEMENT_BOUND_VIOLATION` / `NORMAL_INPUT_WIDTH_FAULT` | poison; preserve attribution and the offending vertex index |
| `NUMERIC_SATURATION` / `RCP_ZERO` | **defined canonical results**, counted separately — *not* transport failures |
| `DEADLINE_EXCEEDED` | existing frame-repeat/fault policy; no partial frame becomes visible |

**W10: publish no partially warped meshlet.** One terminal outcome per
successful decoded vertex; a failure completion may carry placeholder data
**only** while the batch is irrevocably poisoned and blocked from visible
replay. Do not forward a failed point as a successful identity deformation, and
do not make an absent output look like a zero result.

**Bounds** [§5.6]: compare each **returned** displacement against the command's
nonnegative bound using widened absolute values; `|d[k]| > bound[k]` poisons.
**Do not clamp the displacement to the bound and pretend the program computed
that clamp.**

## 10 Counters and traces

`geom_warp_vertices_transformed` already holds a frozen catalog id
(`design/counter_ids.lock`) and has no RTL port. [§20] gives it an **exact**
meaning: **successful ENABLED applications** — not bypasses, and not both camera
landings. A separate bypass counter is kept. The Field engine's instruction
counter is **not** incremented again at the Warp wrapper.

Conservation equations for a completed, non-aborted trace [§20]:

```
accepted successful decoded vertices = bypass outcomes + active-Warp outcomes
active Field requests                = completed Field responses
active outcomes                      = successful applications + failed/poison outcomes
position fork accepts = normal fork accepts = published vertex outcomes
all descriptor references and outstanding credits return to zero
```

Aborted work uses named cancelled/drained/late-refused terms instead of
expecting the clean equations to hold. **Every counter that should be reachable
must be fired by an actual test**; an invariant-violation counter unreachable
under legal stimulus needs a committed mutant **with a driver and a
correct-production negative control**.

## 11 Scalar reference function

**`zref::GeomWarp`** — `reference/include/zref/zref_geom_warp.hpp`. **BUILT.**

A composition over `zfield::interpret` and the existing fixed-point helpers,
containing **no opcode switch** [§21.1]. It validates the Warp signature,
assembles the exact fifteen-word record, interprets, checks the returned
displacement against the declared bound, applies the canonical saturating ADD,
adapts the output normal by the common range-reduction law, and returns output
plus numeric status plus application/transport classification.

`apply_outputs()` is split out deliberately so an RTL differential can push the
**hardware's** six words through the **same** application law and compare only
the half under test — W13's separation applied to correctness rather than to
performance.

## 12 Directed tests

`tests/geometry/geom_warp_reference_directed.cpp` — **BUILT, 102 checks,
passing** (built and run 2026-09-20; this line read "95" and was wrong, as was
`design/blocks.yml`). Covers §22.1 T01–T09 at the reference level: the 15/6 signature and
the explicit refusal of the 14-input legacy typo, profile mismatch, identity
exactness, p3 and all four params, all four attributes including negatives,
signed rails and ±1 LSB with **separate** application status, bound violation
and the `>` (not `≥`) boundary, negative-bound diagnosis, the degenerate normal
and the all-three-zero rule, INT32_MIN range reduction with the **common**
shift, and sparse/high output registers.

Fixtures are real `.zprog` images pushed through the **real** `zfield::decode`
validator: IDENTITY, TRANSLATE, P3_SENTINEL, ATTRIBUTES, ZERO_NORMAL,
SPARSE_OUTPUTS [§21.3].

**THE RTL HALF, ADDED 2026-09-20.**
`tests/geometry/geom_warp_rtl_directed.cpp` — **BUILT, 2,468 checks, passing.**
Every numeric expectation comes from calling `zref::geom_warp::apply_outputs`
with the **hardware's own six output words**; nothing is compared against a C++
restatement of the application law. That is what `apply_outputs` was split out
of `apply` for, and §11 says so. Covers FT092 identity exactness, FT093
non-identity, FT094 both signed rails with application saturation counted
separately, FT095 the `>` (not `≥`) bound boundary in both directions with the
returned displacement preserved, FT096 the reduction including the
`-(2^31 - 1)` case, the all-three-zero degeneracy rule against its
one-nonzero-word discriminator, W09's bypass **measured to perform zero Field
offers**, the three refusals the oracle cannot see (profile, transport fault,
the 64-bit seam) each with a fired counter and the seam's negative control, the
fork under skewed backpressure with independent accept counters, and a
600-vertex randomized differential over hostile values.

**Two defects it found that inspection had not**: a zero-extending `abs33`
(945 of 2,468 checks red on the first run, one cause with two symptoms), and a
Field offer asserted for one cycle before an invalid draw was refused.

`tests/mutants/zhao_geom_warp_shift_oneliner_mutant.sv` + its driver — a
committed positive control, polarity inverted. Measured: **0 of 20,000 random
triples catch it; the directed value −2147483647 does.**

**Owed** (§22.2–22.7): the program-binding and shared-host cases T13–T24, the
join/order/backpressure cases T25–T36, the resource and command-path cases
T37–T47, geometry and visibility T48–T57, abort/liveness T58–T62, and the
controls and performance cases T63–T70. None of these is claimed.

## 13 Randomized differential tests

**Owed.** Legal programs within the admitted classes, hostile values,
randomized valid/ready stalls, repeated `source_id`s, multiple draws, program
and table changes, reset and abort. `tests/geometry/geom_warp_random.cpp` is
named by the ledger and does not exist.

## 14 Formal properties

**Owed.** Ticket and credit conservation, output stability, no reuse while
outstanding, exactly-once fork acceptance, clean/poison seal ordering, bounded
controller progress under the declared service assumptions.

Use abstract arithmetic where necessary. **Do not claim to have proved 48-op
fixed-point execution from a control-only harness** [§25.14]; numeric
correctness stays differential against the common engine.

## 15 Resource / synthesis and integration captures

**No ALM, DSP, RAM or timing figure is claimed, and none exists** [§19.1]. The
directive itself ran no Quartus, no Verilator and no board, so every cost
number in it is an estimate. This contract quotes none as fact.

Intended incremental cost, as an architectural inventory and not a guarantee
[§19.2]: no new execution engine, reciprocal, root, sine, noise or projector;
three saturating position additions and a bounded normal range adaptation;
small direct-indexed join and output queues; RAM-shaped per-draw descriptor
storage sized to the draw-queue contract; optional STREAM4 line buffering; and
common Field metadata/binding support **counted once for all profiles**. The
whole already-present Field v3 engine is **not** charged to Warp, and its
additional lane/register/map support is **not** called free.

Fit policy is unchanged: the complete design only, at the owner-authorized
stage [§19.5].

---

## PREREQUISITES IN THE SHARED FIELD HOST — measured 2026-09-20

[W16]: *"All shared-host limitations are real prerequisites... Do not bury a
second Field loader inside GEOM.WARP to avoid doing this."* Each was **checked
in the tree**, not assumed. Full evidence and file:line citations are in
packet W1's COMMIT MESSAGES on branch `gz/fieldw1` (the harness refuses `.md` under `runs/`; `gz/fieldh1` hit the same refusal at `89bb9bad`, and the earlier warp lane's `FINDINGS-warp.md` was never written for that reason either).

**RE-MEASURED 2026-09-20 BY PACKET W1, AT `55ec050b`, AFTER H1'S HOST LANDED.**
The table below is not inherited: every row was checked in the tree again, and
**four of the nine moved.** Three of the plan's own mappings were wrong, and one
standing ruling turns out to be discharged. The lesson is the ordinary one —
a prerequisite table is a measurement with a timestamp, and this one was taken
before the host it depends on existed.

| # | prerequisite | state, MEASURED |
|---|---|---|
<!-- MERGE NOTE, 2026-09-20, coordinator. W1's measured table is kept below
     because it is the one taken from the RTL rather than from the plan. But it
     was measured against a base that PREDATES packets C1 and D1, so four rows
     were re-measured on the merged tree and corrected in place; each correction
     says so. This is owner ruling R165's rule applied to a contract: a
     measurement is a claim about a moment, and this tree moves fast enough that
     a row written this morning can be spent by evening. -->

| P1 | **15 input lanes.** | **CLOSED 2026-09-20 (packet C1).** `.IN_LANES (15)` in `zhao_console_core.sv` at the composed host. *Corrected by the coordinator on merge: W1 measured `.IN_LANES(13)` against a base predating C1, and W1's own correction below -- that it is three core sites and NOT 'plus generated tops', because `zhao_prod_top` carries no override -- still stands and is the useful half of the row.* Old text:  **ABSENT.** `.IN_LANES(13)` at `zhao_console_core.sv:15930, 16150, 16212` — three sites, and a tree-wide sweep finds **no others**. *Correction:* the repair plan says "three core sites **plus generated tops**"; the generated tops carry **no `IN_LANES` override at all** (`zhao_prod_top.sv:493` instantiates `zhao_field_host` with none). Three sites, not five. |
| P2 | **A free client port.** | **CLOSED 2026-09-22 (packet WARPCOMP).** `.CLIENTS(3)` at the composed `zhao_field_host_v2` in `zhao_console_core.sv`, still one site tree-wide. Client 0 is the S-profile stamp adapter, client 1 the F-profile flow adapter, client 2 `zhao_field_warp_adapter` -- and the widening landed in the SAME COMMIT that gave client 2 a real producer, which was the condition the composer's own comment set: *a third client whose `req_valid_i` is a constant zero is a TIE-OFF, and rule 1 forbids creating one even in the service of closing a gap.* The producer is `zhao_geom_warp`, joined to GEOM.SKIN's position and GEOM.SKIN.NORM's normal and armed per draw through a six-bit descriptor cookie carried in the per-vertex registers of six geometry stages. *Old text, kept because it is a true statement about a tree that no longer exists:* **ABSENT.** `.CLIENTS(2)` at `:15875`, one site tree-wide; both taken by the stamp and flow adapters. |
| P3 | **Sparse output map.** | **PRESENT, AND NOW COMPOSED.** *Corrected on merge: C1 composed `zhao_field_host_v2` into `zhao_console_core`, so the qualifier 'which is not composed' has expired.*  Originally: **PRESENT — in `zhao_field_host_v2`.** `omap_kind[slot][j]` / `omap_index[slot][j]` are the ordinal↔window translation, aliasing falls out for free, and an `OUTPUT_MAP` row naming a `VECTOR_REG` outside the window is a **load-time refusal** (`:402`). H1 closed this. |
| P4 | **ALL-outputs completion.** | **PRESENT, AND NOW COMPOSED.** *Corrected on merge, same reason as P3.*  Originally: **PRESENT — in `zhao_field_host_v2`.** `complete_c = ((seen_next_c & req_mask_c) == req_mask_c)` at `:926` — ALL required ordinals, not ANY — with `StPartial = 8'hF3` at `:468`. R101's defect is repaired and re-planted as a committed mutant. |
| P5 | **Per-context prepared uniforms.** | **MEASURED 2026-09-21. ITS STATED CLAUSE IS DISCHARGED AT THIS COMPOSITION; A NARROWER ONE REPLACES IT.** The array is still flat — `prep_value`/`prep_valid`/`prep_gen` carry no slot dimension — and that much of the old row stands. What was never measured is whether this console can *produce* two simultaneously eligible plans, and **it cannot.** `req_ready_o` is gated on `state == E_IDLE`; the FSM cannot return there without passing `E_RETIRE`, which is itself gated on a **whole-fabric** fence, `fab_active == '0'` across all `PROGS` contexts rather than `fab_active[cur_slot]`. There is exactly one `state`, one `cur_slot`, one `cur_export`, none dimensioned by any parameter, and `prep_value` is read on exactly **one clock per point** — the grant clock — into registers, after which the running point never touches the array again. Loads cannot race that read either: `ld_ready_o = (state == E_IDLE)` and the `E_IDLE` arm takes `ld_valid_i` in strict priority over a grant, so a load and a grant never land together. **Parameter-independent** across `CLIENTS`, `PROGS`, `FAB_LANES` (which replicates one point across lanes and discards the surplus — it does not add points), `FAB_GROUP_PTS`, `FAB_OUTSTANDING` and `CREDITS` (whose overlap is a *retired, already-captured* response waiting in the queue, downstream of `cur_export`). It reopens only on an RTL change that adds a second front — the "gathering front" the console's own `PROGS` note defers. **So Warp interleaving with Earth inside a frame is structurally impossible here, and the clause as written is discharged.**  **AND THE GENERATION CHECK IS NOT LOCKSTEP-BLIND**, which had to be asked: `ggen_c = hdr_assoc_gen[gslot_c]` is written only under `LdAssoc` (kind 5, `ld_data_i[7:0]`, per **slot**) and `prep_gen[i]` only under `LdPrepared` (kind 7, `ld_data_i[47:40]`, per **prep index**). The enables are mutually exclusive by decode, so the two sides of the comparison cannot move together — and the detector has been **seen to fire**: `field_host_v2_directed` FT036 bumps the association generation over an unchanged prepared file and asserts `StBadPrep` with `prep_bad_o` moving by exactly one.  **THE NARROWER OBLIGATION, RECORDED BECAUSE IT IS REAL AND UNGUARDED: PREP-INDEX DISJOINTNESS.** `prep_gen` is indexed by prepared-scalar index while `ggen_c` is indexed by slot, and `omap_index[slot][j]` is chosen by **software**. If a Warp slot and an Earth slot both name prep index 0, the later `LdPrepared` overwrites the earlier value, and the seed check compares `prep_gen[0]` against `hdr_assoc_gen[warp_slot]` — two 8-bit bytes that software assigns and nothing in RTL forces to differ. If they happen to agree, **Warp silently consumes Earth's scalar.** This needs no interleaving; serialisation does not touch it. It is unreachable today only for the weakest possible reason: **nothing produces prepared scalars at all.** `PREPARED_SCALARS` (§0x0022) is defined in `tools/field/gen_field_host_schema.py` and mirrored into `zhao_field_host_image_pkg.sv`, and **neither `tools/field/pack_field_host.cpp` nor `compiler/src/field_ir/serialize.ts` emits it**; `zprog_output_coverage.py` over the three shipped Earth programs reports every declared output satisfied by a vector-register write. A guard quiet because its input does not exist is not a guard that passed. |
| P6 | **Four independent tables.** | **STILL PARTIAL — and RE-MEASURED 2026-09-21, which changed what it COSTS.** `zhao_field_host_v2`'s `TABLES = 2` stands and the composed console passes 2. What is new is the measurement of the other side: the fabric's table store is **hard-wired to four and is not parameterised at all** — `zhao_field_v3_curve.sv` declares `logic [95:0] tbl_ram[0:255]` (4 × TBL_N) and `logic [6:0] meta_n [4]`, and `tl_tbl_i` is a literal `[1:0]` through `zhao_field_v3_engine` and `zhao_field_v3_svcpath`. The host at `TSELW=1` writes `fab_tl_tbl = 2'(ld_addr_i[...])`, zero-extending a one-bit selector into that two-bit port. **So `TABLES=2` saves NO fabric area; it makes tables 2 and 3 of an already-built RAM unaddressable.** Closing it is `TSELW` 1→2 and `LDADDRW` `max(PCW, TSELW+TIDXW)` 7→8 — and the doorbell is **already sized for it**: the console composes `u_field_doorbell` with `.POSTADDRW(8)` against the loader's 7, and it refuses a legacy load that does not fit rather than narrowing it. **DISPOSITION: open, cheap, and not this packet's** — it is a Field-host parameter whose question ("does a shipped Warp program need more than two curve tables?") no composed program can yet ask. |
| P7 | **48-instruction capacity.** | **CLOSED 2026-09-22 (packet WARPCOMP).** `.INSTR_N(48)` and `.PCW(6)` at the composed host, which is decision W14's ceiling. **COST, ARITHMETIC AND NOT A FIT:** `PCW` 5 -> 6, `LDADDRW = max(PCW, TSELW+TIDXW) = max(6, 7)` STAYS 7, so no port width at the host's edge moves and `zhao_field_doorbell`'s `.LDADDRW(7)` still agrees. The uop store grows by 16 x 64-bit words per context at `PROGS = 8` -- 8,192 bits. **THAT IS A CAPACITY NUMBER, NOT AN ALM NUMBER**; whether it infers as M10K or as registers is what a fit answers and arithmetic cannot, and no Quartus was run. **AND `PCW` IS A SEPARATE PARAMETER, NOT A DERIVATION** -- the host keeps them apart and `$fatal`s when they disagree. Moving `INSTR_N` alone is that disagreement, and `run_console_core_smoke -LintOnly` returned **RC 0 in 25 s** over the broken parameterisation and said nothing, because **`--lint-only` does not run `initial` blocks**. The guard fired 205 s into the real smoke: *%Fatal: zhao_field_host_v2: PCW=5 disagrees with clog2(INSTR_N=48)*. A clean lint is not evidence about an elaboration check. *Old text, kept because it is a true statement about a tree that no longer exists:* **ABSENT, CONFIRMED 2026-09-21 by elaboration rather than by grep.** `check_prod_manifest.py` reads the composed parameters out of the Verilator AST and printed `INSTR_N=32'd32` -- the value the console ELABORATES, not the value a line of source says. W14's ceiling is 48. |
| P8 | **Handle → resident-slot binding.** |   **RE-MEASURED 2026-09-22 (packet WARPCOMP), AND THE VERDICT IS UNCHANGED: PARTIAL.** Every claim above was checked again against `fpga/rtl/field/` rather than re-quoted (R165), because the packet that would have been most comfortable believing it is this one -- a blocker that defers work gets believed and re-quoted (R237). It holds: `zhao_field_progcache`'s hash-to-slot directory is reachable only through the doorbell's `post_op = 2` LOOKUP (`fdb_lu_*`), the loader still never issues one, and nothing joins a binding object to a directory slot. **THE CONSEQUENCE FOR WHAT SHIPPED, STATED PLAINLY: GEOM.WARP IS COMPOSED, NOT WORKING.** `d_slot_i` / `d_slot_valid_i` / `d_profile_i` come from the console boundary as `fld_warp_slot_i` / `fld_warp_slot_valid_i` / `fld_warp_prog_profile_i` -- which is NOT a tie-off and is how this console already binds its other two live Field profiles (`fld_flow_slot_i`, `fld_stamp_slot_i`), with one honest difference recorded at the port: for FLOW and STAMP the reason is *no ratified opcode carries it*, and for WARP one DOES. `DrawWarpedForm 0x0304`'s `warp_program` is a `handle32[program]` and `spec/commands.zidl` says it *is resolved by the ONE shared program-binding authority*. Building that resolver in the composer would be this console inventing the authority decision W07 assigns to `fpga/rtl/field/`. **P8 REMAINS THE ONE PREREQUISITE THAT GATES A WORKING WARP, and it is now the ONLY one.** **ONE DEFECT REPAIRED ON THE WAY:** the `zhao_field_loader` `st_idx` reply-identity fault WARPBUILD recorded below is FIXED -- `st_idx` is now written in `L_ACCEPT` from the request's own named object for `K_BIND` (`rq_bind_handle`) and `K_CONTROL` (`rq_bind_prog`), and zeroed for the reserved kind. `field_loader_directed` FT066 asserts it, 189 checks, and it was FIRED first: with the two lines removed the case reports *expected 0x0, got 0x1* on bind(A) after install(A), install(B). **MEASURED 2026-09-21 (packet WARPBUILD). VERDICT: PARTIAL — and the half that is missing is the half P8 is NAMED for.**  **THE BIND HALF IS PRESENT AND STRONGER THAN §9.3 ASKED.** `post_op` 3 is exhaustively decoded (`OpLoad`/`OpCommit`/`OpLookup`/`OpFh2` with a counted `V_BAD_OPERATION` default and a committed control at `tests/mutants/zhao_field_doorbell_op3_alias_mutant.sv`) — the old catch-all `else → D_LOAD` is gone. `BIND_PROGRAM` is not a doorbell state at all: the doorbell is a **credited relay** (`D_FH2`/`D_FH2_RESP`, return record reserved before the post is taken), and the binding lives in `zhao_field_loader`'s `K_BIND` arm, which validates a six-way conjunction against **one exact object** — handle reserved bits, `obj_ready`, `!obj_staging`, `obj_gen`, `obj_handle32`, `obj_epoch` and `obj_full_crc` — refusing anything else as `V_BAD_BINDING`. BIND never resolves by canonical hash, which discharges §9.1's aliasing clause directly.  **THE SEAL HALF IS NOT A BINDING OPERATION.** `L_SEAL` is the only writer of `obj_ready`, and it is reachable **only from the INSTALL arm**. `K_BIND` runs `L_ACCEPT → L_CHECK → L_RETURN` and never enters it. §9.3 wants SEAL to be a *consumed binding operation with its own completion*; here it is an internal state of a different command, with no verb, no ticket and no counter. **`BIND_META` and `UNBIND` do not exist** — the nearest thing to revocation, `VERB_RELEASE_BINDING`, is aliased with two other verbs into one arm that only decrements `obj_pins`; it does not revoke, `obj_ready` stays set and `obj_gen` is not bumped. (§9.3's "never invalidate live work silently" *is* honoured, by a different mechanism: the allocator skips pinned objects and answers `V_NO_CAPACITY`.) The four `post_kind` values §9.3 specifies were **superseded**, not implemented — the RTL follows `Zhaozhou_SHARED_FIELD_Repair_Architecture_2026-09-20.txt` §10.2's encoding, which is a stated choice and is cited in the file.  **AND THE CHAIN P8 IS ABOUT IS BROKEN IN THE MIDDLE.** handle → canonical program is PRESENT (`fh2_resp_handle_o`, `pub_handle_o`/`pub_prog_hash_o`/`pub_gen_o`). resident slot → prepared plan is PRESENT (`hdr_assoc_gen`, `prep_gen`, `hdr_loaded`, `hdr_ipok`). **canonical program → resident slot is ABSENT**: hash→slot lives in `zhao_field_progcache`, reachable only through the legacy `post_op=2` LOOKUP path, and the loader never issues a lookup while the doorbell's FH2 arm never touches `pc_lu_*`. **Nothing joins a binding object to a directory slot**, so §9.2's row field *current resident slot* has **no storage anywhere in the tree**, and `hdr_assoc_gen` (8-bit, software-supplied) is never differenced against `obj_gen` — two generation counters in two modules that nothing compares. The resolution port has no consumer either: `pub_sel_i` is promoted to `fld_ldr_pub_sel_i` with **no in-console driver**, which the core's own note S2 records.  **CONSEQUENCE FOR WARP, STATED PLAINLY:** contract §9's `PROGRAM_NOT_RESIDENT` / `STALE_BINDING` / `PLAN_NOT_READY` refusals **have no hardware that can compute them**, and `zhao_geom_warp`'s `d_slot_i` / `d_slot_valid_i` have no producer that can resolve `DrawWarpedForm.warp_program` into a slot. **P8 is the prerequisite that gates a WORKING warp, as distinct from a composed one.**  **AND ONE DEFECT FOUND ON THE WAY**, recorded here because it is load-bearing for any future BIND: in `zhao_field_loader`, `st_idx` is assigned at **exactly one site** — `L_RESERVE`, the INSTALL path — yet `L_RETURN` unconditionally emits `fh2_resp_slot_o <= st_idx` and builds `fh2_resp_handle_o` from `obj_gen[st_idx]`. **A successful `K_BIND` therefore replies with the slot and generation of whatever object the LAST INSTALL reserved.** It survives testing because the natural stimulus is install-then-bind-the-thing-you-just-installed, where `st_idx` is correct by accident; it diverges on install(A), install(B), bind(A), and on a bind after reset. Not repaired here — `fpga/rtl/field/` is not this packet's file set — but it is a real reply-identity fault and it is written down rather than carried.  Old text, kept because it is a true statement about a tree that no longer exists: **RE-MEASURE BEFORE RE-QUOTING (owner ruling R165).** *Corrected on merge:* W1 measured this against a base where **D1 was not merged**, and said so honestly in the row itself. D1 IS merged now, and `zhao_field_doorbell.sv` carries `BIND_PROGRAM`. What W1 wrote below is therefore a true statement about a tree that no longer exists, and the remaining question -- whether D1's BIND satisfies §9.3's BIND/SEAL -- is UNMEASURED. Old text:  **STILL ABSENT — and the plan says YES.** `zhao_field_doorbell.sv:440-442`: the final `else` still sends **everything** to `D_LOAD`, and `post_op_i` is still `[1:0]` (`:157`), so value 3 remains encodable and not inert. A tree-wide search of `fpga/rtl/field/` for `D_BIND`, `D_SEAL`, `bind_slot` or `handle_to_slot` returns **nothing**. D1's branch exists and is **not merged into this base**. *Correction to the repair plan §4.* |
| P9 | **Per-point cost / the R91 lever.** | **NOW EXISTS — and this DISCHARGES a standing claim.** `zhao_field_host_v2.sv:1167-1171`: *"E_ZERO is skipped entirely under FH08, so the fast path's preload is IN_LANES clocks where the oracle's is REGS + IN_LANES"*, gated on an accepted `INIT_PROOF` (`hdr_ipok[slot]`). **R103 states that the R91 fast path "does NOT exist anywhere in the tree." It does now**, in H1's host. **AND IT IS MEASURED AS OF 2026-09-26 (EARTHMAJOR), which closes the "unmeasured in clocks" clause this row carried.** `tests/field/field_host_depth_census.cpp` drives this host at **`zhao_console_core.sv`'s own twenty parameters** — not the module's defaults, which is the trap `zhao_field_host.sv:115` names — with four clients saturating: the steady-state accept-to-accept interval is **62.00 clocks with the per-point clear ON and 30.00 with it skipped**, so **the fast path is worth 32 clocks, 52 % of a run**, and the census asserts that saving equals `REGS` within 20 %. Both runs answer `StOk` with `out[0] == 42`, which is the check that matters here: the first draft of that case loaded `INIT_PROOF` **after** the header, `LdInitProof` sets `hdr_loaded[slot] <= 1'b0` (`zhao_field_host_v2.sv:1472`), and every run answered `StNoProgram` in one clock — a 62× "speed-up" that was entirely refusals and that the run-count check passed. **Load the proof BEFORE the header, or reload the header after it.** |

**THE COST NUMBER IS ARITHMETIC ON THE FSM, NOT A RESULT.** `51 + T_run` at
Warp's fifteen lanes comes from `REGS + IN_LANES + 4 + T_run` with the old host's
unconditional clear. With P9's fast path that term becomes `IN_LANES + 4 + T_run`
= **19 + T_run**. Both numbers are FSM arithmetic. Neither is a benchmark and
neither is a fit. **PHYSICAL FIT PENDING**, and the clock count is a Verilator
question that has not been asked yet.

**Consequence, RESTATED 2026-09-21, and it has changed AGAIN — this table moves
faster than the paragraphs under it, which is R165's whole point.** The sentence
below was written when the console did not compose `zhao_field_host_v2`. It does
now. And the item the old text called "one thing no prerequisite table listed"
is closed.

**Where the nine actually stand at this commit:**

* **P1, P3, P4, P9 — CLOSED.** Composed and measured.
* **P5 — DISCHARGED AT THIS COMPOSITION**, structurally, with a narrower
  obligation recorded in its place (prep-index disjointness).
* **P2, P7 — one parameter each, and they land in the composing act.** Neither
  may land alone: a third client whose `req_valid_i` is constant zero is a
  tie-off, and widening the uop store before a Warp program can be admitted
  buys capacity nothing asks for.
* **P6 — open, and now known to be nearly free.** It does not save area; it
  makes half an already-built RAM unaddressable.
* **P8 — PARTIAL, and it is the one that matters most.** BIND is present;
  SEAL-as-a-binding-operation, `BIND_META` and `UNBIND` are not; and
  **canonical program → resident slot has no storage anywhere in the tree.**

**SO THE HONEST STATEMENT OF WHAT IS LEFT IS TWO THINGS, NOT A LIST.**

1. **THE CARRIER.** The per-draw snapshot exists at `zhao_console_core`'s
   boundary and `zhao_geom_warp`'s `d_*_i` group is waiting for it; what is
   missing is the ride between them, through `zhao_geom_drawjob`'s `SIDEW`
   bundle and a new `v_side_o` on `zhao_geom_assetfetch`. That is a
   COMPOSITION problem and it is mechanical.
2. **P8'S MIDDLE LINK.** Even with the carrier, `d_slot_i` / `d_slot_valid_i`
   need something that resolves `DrawWarpedForm.warp_program` — a handle — into
   a physical resident slot, and **no such thing exists**. W07 is explicit that
   "a handle is not a content hash; a content hash is not a physical resident
   slot", and the tree currently has the first and third of those three with no
   join between them. **A carrier without P8 gives a composed Warp that can be
   reached and cannot be armed** — better than today's unreachable one, and
   still not W18's end-to-end evidence.

**GEOM.PARAMBUF IS NOT CLOSED BY THIS AND WAS MEASURED RATHER THAN HOPED.** It
was plausible that composing Warp would give `zhao_geom_parambuf` a consumer; it
does not. That block's blocker is an **arena WRITER**: every geometry memory
client is hard-coded read-only in source (`zhao_geom_meshfetch`,
`zhao_geom_assetfetch`, `zhao_geom_mem_adapter`, `zhao_geom_drawjob`,
`zhao_geom_ladderbank`, `zhao_geom_loomfeed`), `spec/memory_rules.md` 5f
declares RENDER.ASSET_POOL read-only with a formal assertion, and none of its
four ledger counters appears in any `.sv`. Warp is a vertex-stream deformer
between SKIN and PROJECT; it neither allocates the arena nor writes SDRAM.
**Treat the two as independent.**

Superseded text, kept because it was true when written: P3 and P4 are built and
P9's lever exists — all three in `zhao_field_host_v2`, which **the console does
not compose**. What still blocks a live composition is **P1, P2 and P7** (all
three one-line parameter changes in `zhao_console_core.sv`, which is C1's file
and C1's act), **P8**, and one thing no prerequisite table listed:

**~~THERE IS NO `DrawWarpedForm` COMMAND, SO NO DRAW CAN EVER ENABLE WARP.~~
CLOSED 2026-09-21 (packet WARPBUILD).** The sentence was true when written and
is the reason it was written: a composed `zhao_geom_warp` under the old ABI
would have sat permanently in its W09 bypass — function present and
structurally unreachable — and that is not a composition. `DrawWarpedForm
0x0304` now exists (W04, 96-byte record), `zhao_cmd_exec` decodes it, and the
per-draw snapshot leaves `zhao_console_core` as `cmd_draw_warp_*_o`.

**WHAT REPLACES IT AS THE BLOCKER, AND IT IS A DIFFERENT KIND OF THING.** The
producer exists and the consumer exists; what is missing is the **carrier
between them**. `zhao_geom_warp` is POST-SKIN, and the geometry front is
pipelined, so draw N's tail vertices overlap draw N+1's head — a single held
register at the skin stage would hand vertex B's position to descriptor A, with
every counter balancing while it happened. Directive 7.1 names the shape: the
draw item carries "`warp_enabled` plus a compact descriptor cookie", and the
cookie rides the **job handshake**, the way `j_side_o` already carries a draw's
raster word (composer entry I39: *"a meshlet cannot then be paired with another
draw's state, because there is no second path for it to arrive on"*). That ride
passes through `zhao_geom_drawjob`'s `SIDEW` bundle — whose `$fatal` guard on
`SIDEW != 72` is there precisely to make a half-done layout move impossible to
ship quietly — and needs a `v_side_o` beside `zhao_geom_assetfetch`'s per-vertex
port, where `src_q` already proves the per-meshlet fanout works.

**DO NOT KEY THAT CARRIER ON `src_id`.** Measured 2026-09-21: `src_id` is
`capture_format.md` §5's `index` field ALONE — a compiler source-registry
ordinal naming an **emit site**, so two draws from one statement share it — with
`kind`/`module` truncated onto `draw_src_truncated_o`. W08 says so from the
other side: *"`source_id` remains attribution, not identity."*

**What W1 did instead of tying the port off.** The block is built, and its Field
request port is a real client port shaped to `zhao_field_warp_adapter`'s declared
consumer contract — that adapter's own header names `zhao_geom_warp.sv` as the
consumer which "DOES NOT EXIST YET, and that is the point: this file is its
prerequisite, not its replacement." Nothing is tied off, and nothing is composed.

## Notes

The ledger's `inputs: [instanced_transforms, displaced_vertices]` and
`upstream: [GEOM.LOOM, ...]` edges are corrected under **conflict C03**: the
**control** dependency on GEOM.LOOM is retained, the **data** dependency is not.
Warp consumes **post-skin vertices and normals**, never LOOM's matrix stream.
Treating those matrices as vertices is on the directive's prohibited list.

`FIELD.SEQ.WARP` remains a **profile**, not a second sequencer or an RTL engine
[§1.2, C02]. Field v3 is the production realization and its canonical ISA,
validator, hash, interpreter and planner remain the arithmetic authorities. A
Warp **stream adapter** is permitted; a private Warp instruction set or a
duplicate interpreter is not.
