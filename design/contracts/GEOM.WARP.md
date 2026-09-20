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
| Scalar reference `zref::GeomWarp` | **BUILT AND TESTED** — `reference/include/zref/zref_geom_warp.hpp`, 95 directed checks, 3 fired mutants |
| `DrawWarpedForm 0x0304` ABI | **NOT BUILT** |
| `zhao_geom_warp.sv` and the join | **NOT BUILT** |
| Composition into `zhao_console_core` | **BLOCKED** — on the shared Field host, below |
| Throughput / resources | **UNMEASURED** — no fit, no cycle count, no claim |

**GEOM.WARP is still `NOT BUILT AT ALL` in the completion register, and this
contract does not change that.** [W18]: "A built block, zero gap counter, or a
picture alone is not sufficient."

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

`tests/geometry/geom_warp_reference_directed.cpp` — **BUILT, 95 checks,
passing.** Covers §22.1 T01–T09 at the reference level: the 15/6 signature and
the explicit refusal of the 14-input legacy typo, profile mismatch, identity
exactness, p3 and all four params, all four attributes including negatives,
signed rails and ±1 LSB with **separate** application status, bound violation
and the `>` (not `≥`) boundary, negative-bound diagnosis, the degenerate normal
and the all-three-zero rule, INT32_MIN range reduction with the **common**
shift, and sparse/high output registers.

Fixtures are real `.zprog` images pushed through the **real** `zfield::decode`
validator: IDENTITY, TRANSLATE, P3_SENTINEL, ATTRIBUTES, ZERO_NORMAL,
SPARSE_OUTPUTS [§21.3].

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
`runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-warp.md`.

| # | prerequisite | state |
|---|---|---|
| P1 | **15 input lanes.** `zhao_field_host` is composed at `IN_LANES(13)`, `OUT_LANES(7)`. | **ABSENT** — 2 lanes short. Outputs suffice. |
| P2 | **A client port.** `CLIENTS=2`; client 0 is the S-profile stamp adapter, client 1 the F-profile flow adapter. | **ABSENT** — no free port; and there is no Warp adapter. |
| P3 | **Sparse output map.** Capture is one CONTIGUOUS window from `hdr_outbase`. | **ABSENT** — §5.2 and T09 require a per-output map. |
| P4 | **ALL-outputs completion.** Completion tests `cur_out_seen == '0'` — i.e. **ANY** output, not all required ones. | **ABSENT, and it is a live defect class**: a point writing 5 of 6 lanes reports success with the sixth reading the cleared zero. That is W10's "an absent output must not look like a zero result", in shipped shared RTL. §12.3 requires a per-output seen bit. |
| P5 | **Per-context prepared uniforms.** The scalar bank is one flat 64-slot array with no context dimension. | **ABSENT** — §12.5 / §9.5 require isolation per simultaneously eligible plan. |
| P6 | **Four independent tables.** The fabric carries 4 with per-table counts; the host exposes `TABLES=2` and commits one table per header write. | **PARTIAL** |
| P7 | **48-instruction capacity.** Warp's ceiling is 48; the host is composed at `INSTR_N=32`. | **ABSENT** — a full-length Warp program does not fit the uop store. |
| P8 | **Handle → resident-slot binding.** Only a content-hash directory exists; the client supplies the raw slot itself. `post_op` is 2 bits with 0/1/2 used — value 3 is **encodable but not inert**, it currently falls through to the LOAD path. | **ABSENT** — §9.3's BIND/SEAL is a protocol extension, not an unused number. |
| P9 | **Per-point cost.** Measured from the FSM: `REGS + IN_LANES + 4 + T_run`; 49 + T_run at today's 32/13, **51 + T_run** at Warp's 15 lanes. The "uniforms once per association" fast path of ruling **R91** does **not** exist anywhere in the tree. | **ABSENT** — and it is the same lever I5 needs. |

**Consequence, stated plainly.** GEOM.WARP cannot be composed as a real Field
client until P1, P2, P3, P4 and P8 are repaired in the **shared** host. Building
`zhao_geom_warp.sv` and tying its Field request port off would move the register
entry from *not built* to *tie-off* — closing a gap by opening one, which the
packet protocol forbids. The block is therefore specified, referenced and tested
here, and **not** composed.

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
