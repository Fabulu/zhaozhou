# GEOM.WARP — Recon and Architecture (2026-09-06)

> Recon-and-architect deliverable. **This file is the entire output**: no RTL,
> no `design/*.yml`, no `spec/`, no test, and no contract file was touched.
> The RCP-tile fit closure (`zhao_field_rcp24_rom.sv`, `zhao_raster_ticketq.sv`,
> `zhao_raster_rcp24_mul.sv`, `zhao_raster_rcp24_v3.sv`) was read-only for this
> lane and was not read at all — nothing here needed it.
>
> **Standing status this document does not change:** GEOM.WARP is DEFERRED by
> owner ruling 2026-08-31 §6.3 and its contract says so in every section.
> Part 2 below is a *revival-ready specification* — the material an
> evidence-backed revival would ratify into `design/contracts/GEOM.WARP.md` —
> not a revival, and not a claim that one is warranted today.

---

# Part 1 — Recon

## 1.1 Verdict: GEOM.WARP exists, is decided, and is deliberately empty

The tasking allowed for "partly specified, partly built, or may not exist at
all". The truth is a fourth thing: **registered, ruled on, and deliberately
unwritten.**

| artefact | state | evidence |
|---|---|---|
| Ledger entry | EXISTS — `maturity: SPECIFIED`, **`deferred: true`, `cut_order: 5`** | `design/blocks.yml:2722–2751` |
| Contract | EXISTS but every section reads "**Deliberately unwritten.** This block is DEFERRED by owner ruling 2026-08-31 section 6.3 (cut-order 5)… Fill this in only if an evidence-backed revival happens." | `design/contracts/GEOM.WARP.md` (all 71 lines) |
| RTL | NONE — no `zhao_geom_warp.sv` in `fpga/rtl/geometry/` (24 files listed; warp absent) | directory listing |
| Reference model | NONE — `zref::GeomWarp` is named by the ledger (`design/blocks.yml:2747`) but no `zref_geom_warp.hpp` exists in `reference/include/zref/` | directory listing |
| Tests | NONE — `tests/geometry/` has no warp test; the ledger's `tests:` lines are aspirations | directory listing |
| Software | NONE — "`GEOM.LOOM`, `GEOM.WARP` … Transform-graph evaluation and Warp8 deformation are unimplemented in software as well" | `reports/REMAINING_BLOCKERS.md:1301` |

The governing ruling, verbatim (`reports/OWNER-RULINGS-COMPLETE-20260831.md:605–617`, §6.3):

> DEFER dedicated v1 hardware. Current real needs are covered by: the bounded
> fixed creature-deform path; Loom transforms; HPS/PC preprocessing where
> necessary. Keep GEOM.WARP as an optional later accelerator, cut-order 5. Do
> not allow it to block conventional geometry or creature completion.

Ratified into the release definition: `design/V1-RELEASE-DEFINITION.md:136` —
"**DEFERRED** — owner ruling 2026-08-31 §6.3, cut-order 5; its contract is
deliberately unwritten." And `reports/OWNER-RULINGS-COMPLETE-20260831.md:1079`
counts it among the blocks "formally closed rather than built".

**What cut-order 5 actually names:** the charter's cut ladder
(`ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md:1902–1913`) lists item 5 as "**Warp8
throughput**" — the *throughput* is the cuttable thing, sitting between
"second world-space 2D plane mode" (4) and "volumetric/post buffer precision"
(6). The ladder's own framing assumes a reduced-rate capability can exist; it
cuts the rate, not the concept. That distinction shapes the whole architecture
below.

## 1.2 Far more is decided than the empty contract suggests

The contract being blank does **not** mean the design space is open. Most of
it is ratified elsewhere, and a designer who missed that would re-decide
settled law:

**(a) The semantics are the Field IR W profile, ratified 1.B-9.**
`spec/form/field-ir.md` §7.1 (line 523) fixes the I/O record exactly:

    warp | id 1 | in:  px,py,pz:fx, nx,ny,nz:fx, a0..a3:fx, time:u32, p0..p3:fx  (14 lanes)
               | out: dx,dy,dz:fx, nx′,ny′,nz′:fx                                 (6 lanes)

All lanes 32-bit Q16.16 ("fx") unless noted. Instruction ceiling **48**
(§7.3, line 536; global hard ceiling 64). All opcodes admitted in v1 (§7.2),
with the per-profile whitelist hook already in the validator. Output is a
**displacement** plus a **replacement normal** — not an absolute position.

**(b) There is ONE engine, and warp is a profile of it, forever.**
`design/contracts/FIELD.SEQ.WARP.md` ("This is a PROFILE, not a block"): owner
ruling 2026-08-22, one engine five profiles; `kind: profile`,
`implemented_by: FIELD.SEQ.CORE`; "there is no separate FIELD.SEQ.WARP
sequencer in hardware and there is not going to be one." The Field v3
amendment in `design/contracts/FIELD.SEQ.CORE.md` (AMENDED 2026-08-27, from
`reports/Fieldv3.md`) goes further and **already names GEOM.WARP's shape**:

> A profile is a program set plus a STREAM ADAPTER — a small generator that
> produces the varying input lanes directly (… **Warp: vertex
> position/normal** …) and consumes the outputs directly from snooped export
> registers. Adapters generate and consume streams; they NEVER re-implement an
> op, and there are not five engines.

So GEOM.WARP, if built, IS the warp stream adapter plus the displacement
application. Anything else contradicts a ratified ruling.

**(c) The reference law already exists and is singular.**
`reference/include/zref/zref_fieldir.hpp:45–56`: `zref::fieldir::interpret`
forwards to `zfield::interpret` — "the ONE generic Field IR interpreter",
mirrored by the TS interpreter and pinned by committed `.zvec` goldens.
`FIELD.SEQ.WARP.md`'s scalar-reference section names it as this profile's
oracle explicitly, and adds: what is profile-specific is **the lane binding**,
"and it belongs with the blocks that consume the output" — i.e. with GEOM.WARP.
That lane binding is the one genuinely unwritten piece of the W profile.

**(d) The graph position is drawn.** `design/blocks.yml:2733–2734`: upstream
`[GEOM.LOOM, FIELD.SEQ.WARP]`, downstream `[GEOM.PROJECT]`;
`design/blocks.yml:2844` lists GEOM.WARP among GEOM.PROJECT's upstreams, and
GEOM.PROJECT's contract says "The vertices arrive from GEOM.WCACHE or
GEOM.WARP" (`design/contracts/GEOM.PROJECT.md`, Memory ownership). PROJECT's
input packet is fixed and built: `vx/vy/vz` s32 fx16 world position, `view_i`,
`src_id_i` (u16), ready/valid one per beat.

**(e) The throughput arithmetic that forced the deferral is on record.**
`design/budgets/workloads.yml:289–296` (the `zhao_field_seq` unruled note):

> The docket's worked example (120,000 warped vertices x 8 instructions x 8
> clocks = 7,680,000 clocks, i.e. **4.6x the whole frame**) is why one scalar
> cannot express it.

Full-population warp on the v1 scalar walker is arithmetically impossible at
100 MHz / 60 Hz (1,666,667 clocks/frame). The v2 SIMD front was measured off
the production path for good (59.22 MHz Fmax, 27,225 transport clocks per
1,089-vertex association — `FIELD.SEQ.CORE.md` engine table,
`reports/FIELD_V3_COST_MODEL.md` §1). The v3 vector fabric is "the production
path" but its service IIs are **targets — Phase 3 probe gates, not
measurements** (`reports/FIELD_V3_COST_MODEL.md` §1, last rows).

**(f) The creature-deformation need is explicitly NOT this block.**
`reports/CREATURESANDLIGHTS:186–193`: fixed creature deformation
(flatten/spread, radial/follower/none vertex classes) is a bounded sidecar,
`VDECODE → CREATURE.DEFORM → SKIN`, several cycles per *affected* vertex with
an exact identity bypass for the rest — and then, in so many words: "**GEOM.WARP
remains deferred and unnecessary. Arbitrary per-vertex programs sampling wind
fields would be the genuinely expensive version; this bounded sidecar is
not.**" Creature squash-and-stretch, capes and secondary motion are the
sidecar's, Loom's and the sparse-matrix-patch path's. Terrain deformation
(Bore scars, collapse, Volcano rise) is FIELD.SEQ.EARTH/STAMP + TERRAIN.BAKE
territory (`reports/Missingterrain:62,152,179` discusses it entirely in
terrain-organ terms). **What warp uniquely adds to the game** is programmable
*non-terrain, non-skeletal* surface motion: spell-impact ripples through a
creature or structure, banner/foliage wind response beyond what bones justify,
Sacrifice-style spell-warped geometry — the effects layer, not the identity
layer.

## 1.3 What is genuinely open

1. **Whether the deferral lifts at all.** The contract demands an
   "evidence-backed revival". No such evidence exists today; nothing in
   `reports/OWNER-RULINGS-20260903-FUNDAMENTALS.md` or the buildability
   rulings touches warp (searched; zero hits).
2. **The W lane binding** — which registers in, which out, per program
   (`FIELD.SEQ.WARP.md`, "What is still open, and it is not hardware").
3. **Where per-vertex amplitudes `a0..a3` come from.** The format-0 vertex
   record has no home for them: bytes 24–31 are reserved-must-be-zero
   (`reference/include/zref/zref_geom.hpp:217–218`), and the vertex format
   belongs to `SW.TOOLS.ASSET` ("one spec, two ends" —
   `reports/REMAINING_BLOCKERS.md:1298`).
4. **Who consumes the warped normal `nx′,ny′,nz′`.** The ledger routes warp
   only to GEOM.PROJECT, which takes *no normal* (`GEOM.LIGHT.md`: "zhao_geom_project.sv
   takes no normal, no colour, no light set"); GEOM.LIGHT's upstreams are
   `[GEOM.SKIN, GEOM.SKIN.NORM]` (`design/blocks.yml:3013`). The W profile's
   normal outputs have **no consumer edge anywhere in the ledger**.
5. **One ledger edge that looks stale:** `GEOM.LOOM → GEOM.WARP` carrying
   `instanced_transforms` (`design/blocks.yml:2733`, inputs line 2731). Matrix
   application is GEOM.SKIN's job (its rigid path takes any 3×4), and LOOM's
   output packet is "the same shape GEOM.SKIN already takes as `a_m_i`/`b_m_i`"
   (`GEOM.LOOM.md`, Out). A warp stage that also applied instance transforms
   would be a second matrix-applier duplicating SKIN. Part 2 excludes it and
   flags the edge for re-examination at revival.

---

# Part 2 — Architecture (revival-ready specification)

Written to the shape of `design/contracts/TERRAIN.COMPCACHE.md` and
`TERRAIN.PAGELOADER.md`. Everything below is *proposed*; adoption requires the
owner to lift §6.3 (Q1) and the contract stubs to be replaced with this text.

## 2.1 Purpose

**GEOM.WARP is the warp stream adapter plus the displacement applier.** It
takes skinned world-space vertices flagged for warping, presents each as the
Field IR W-profile input record, lets the ONE field engine evaluate the
program, and applies the resulting `(dx,dy,dz)` to the position with the house
saturating add — then hands GEOM.PROJECT exactly the packet it already takes.
Unflagged vertices take an **exact, bit-identical identity bypass** with zero
arithmetic, the same law CREATURE.DEFORM's sidecar states for unmarked
vertices (`reports/CREATURESANDLIGHTS:184`).

It is the "Warp: vertex position/normal" adapter the Field v3 ruling names
(`FIELD.SEQ.CORE.md`, AMENDED 2026-08-27) — no more.

## 2.2 Exclusions, and who owns each excluded thing

| Excluded | Owner |
|---|---|
| Op semantics, evaluation, per-op rounding | `FIELD.SEQ.CORE` / `zfield::interpret` (one-engine ruling 2026-08-22; adapters "NEVER re-implement an op") |
| Program storage, decode, validation | `FIELD.PROGCACHE` + the `zfield` decoder (V1–V12) |
| Fixed creature deformation (flatten/spread) | CREATURE.DEFORM sidecar (`reports/CREATURESANDLIGHTS` §2) — a different, cheaper machine before SKIN |
| Transform composition and instancing | `GEOM.LOOM`; matrix application to vertices | `GEOM.SKIN` (rigid path) — GEOM.WARP applies **no matrices**, which is why the ledger's `instanced_transforms` input (blocks.yml:2731) is proposed dropped at revival |
| Skinning, normal blending | `GEOM.SKIN`, `GEOM.SKIN.NORM` |
| Projection, lighting | `GEOM.PROJECT`, `GEOM.LIGHT` |
| Terrain deformation of any kind | FIELD.SEQ.EARTH/STAMP programs + `TERRAIN.BAKE`/patch organs |
| Memory access | **None. No VRAM port, no guard client, no bus master** — same stance as GEOM.SKIN and GEOM.PROJECT ("None" memory-ownership sections). Vertices stream; batch parameters arrive by command; the program lives in PROGCACHE |
| Normal warping (v1 baseline) | Deferred behind Q3 — the profile's `n′` lanes are read and *discarded* until an owner ruling routes them (see 2.9) |

Exclusions are the defect-prevention here: every excluded row above is a thing
the empty ledger entry could plausibly have been read to include.

## 2.3 Position in the graph, and the batch model

    GEOM.SKIN ──► GEOM.WARP ──► GEOM.PROJECT        (warped draws)
    GEOM.SKIN ──► GEOM.WCACHE ─► GEOM.PROJECT       (unchanged; dual-view path)

Warp is **opt-in per batch** (per draw / meshlet), not per frame and not
global. A batch opens with a config write: `{prog_handle, p0..p3, time:u32,
a0..a3 batch constants (Q2), warp_en}`; every vertex of the batch then flows
through. `warp_en=0` batches are pure bypass at one vertex per clock.

Ordering is **strictly in-order, one lane** — a bypass vertex behind a warped
vertex waits. Downstream assembles triangles positionally
(`TERRAIN.COMPCACHE.md`: "the order IS the index"; GEOM.PARAMBUF's positional
record law), so reordering between bypass and warped vertices would silently
re-index geometry. In-order costs throughput and buys correctness; the budget
below prices it honestly.

## 2.4 The arithmetic, exactly

**Inside the engine: none of it is this block's.** The W program is evaluated
under `zfield::interpret`'s law — every multiply, curve, rot and their
roundings belong there and are already golden-pinned. GEOM.WARP contributes
**zero multiplies and zero DSPs**. (The engine's shared multiplier discipline —
"MULTIPLIERS: ONE" — is `FIELD.SEQ.CORE.md`'s, already paid for.)

**In this block: exactly three adds, and each is narrowed alone.**

    warped.x = sat_s32( sx(px) + sx(dx) )     // one s33 sum, one explicit saturate
    warped.y = sat_s32( sx(py) + sx(dy) )
    warped.z = sat_s32( sx(pz) + sx(dz) )

Both operands are s32 Q16.16, the wide sum is 33 bits, and each component is
narrowed **one at a time** with its own saturate — never a wide accumulate
then one narrow. There is no rounding step because no fraction bits move: an
integer add of same-format values is exact, per `zref::fx_add`
(`reference/include/zref/zref_fixp.hpp:182`, which is precisely
`sat_s32_from_s64(a+b)` with the ledger's `add` counter). Amplitude scaling
(`a0..a3 ×` anything) happens **inside the program**, under the engine's
`fx_mul` law — putting a multiply in the applier would be a second statement
of arithmetic the interpreter already owns.

Saturation is counted, never silent: each component saturate increments the
sat ledger, and the engine's own `sat`/`rcp0` status bits (field-ir §6.1) are
accumulated per batch. Widths are *proven trivially* (s32+s32 ≤ s33); there is
nothing here for `QUARTUS_GOTCHAS` §5's wide-operand trap to bite.

**The lane binding this spec pins** (the open item from `FIELD.SEQ.WARP.md`):
inputs load `R0..R13` in field-ir §7.1 record order —
`px,py,pz,nx,ny,nz,a0,a1,a2,a3,time,p0,p1,p2,p3`… which is 15 names for 14
lanes; §7.1's count is 14 with `time` at R10 and `p0..p3` at R10+1..R13 —
the *authoritative* order is the §7.1 table row, taken verbatim. Outputs are
read from the program's declared `out_lanes` (the decoder's `Decoded`
metadata), *not* from fixed registers, on the v1 walker — the walker exposes
the register-file port, so no register pinning is needed. For the v3 adapter,
snooped export registers want the profile to FIX output registers; that is a
validator table edit (§7.2's whitelist-hook pattern) proposed as Q4.

## 2.5 Storage

**This block owns no memory.** Its entire state:

| structure | width × depth | writer | reader | lifetime | cost |
|---|---|---|---|---|---|
| batch config latch | ~330 bits of registers (`prog_handle`, `p0..p3`, `a0..a3`, `time`, `warp_en`, budget residue) | command shell (single writer, only between batches) | the adapter FSM (single reader) | one batch | ALMs only |
| vertex-in-flight latch | 1 × {x,y,z,src_id,view} ≈ 130 bits | accept edge | apply stage | one vertex | ALMs only |
| output skid | 1 entry, ≈ 130 bits | apply stage | GEOM.PROJECT handshake | one beat | ALMs only |

**M10K: 0. DSP: 0. ALM ceiling: 1,200** (adapter FSM + lane-load sequencer +
three 33-bit saturating adders + counters; for scale, GEOM.LOOM's ceiling is
1,500 ALM *including* twelve multiplies, `GEOM.LOOM.md` Synthesis section).
Against the device (41,910 ALM / 553 M10K / 112 DSP), with the texture island
alone at 16,192 ALM and terrain's compcache needing 15 M10Ks against a pool
the full terrain cache already overflows at 161% (`TERRAIN.COMPCACHE.md`),
a zero-M10K, zero-DSP block is the only kind of warp stage this device can
still afford — and the expensive things (register file, shared multiplier,
sin/rcp ROMs, program cache) are the field cone's, **already spent and
shared** whether or not warp ever runs.

There is deliberately no vertex buffer: buffering warped batches would
re-create the M10K-flop inference trap (a 16-bit array became 65,536 flop
bits once; combinational logic before the first register kills inference),
and the rigid one-in-flight design needs none.

## 2.6 Backpressure and refusal behaviour

Ready/valid on both sides, house hygiene (`in_ready_o` independent of
`in_valid_i`; `out_valid_o` independent of `out_ready_i`).

* `v_ready_o` is **low for the whole engine occupancy** of a warped vertex —
  the `!busy` term GEOM.SKIN learned the hard way ("a `v_ready_o` that ignored
  `busy` would let a second vertex overwrite the one in flight and emit a
  plausible skinned vertex belonging to neither", `GEOM.SKIN.md`).
* Bypass vertices flow at one per clock *only while no warped vertex is in
  flight* (the in-order law, 2.3).
* The engine's inputs are **latched on accept** — the walker reads lanes for
  hundreds of cycles, and GEOM.SKIN's contract records exactly why a
  combinational read there is wrong.

**Refusals — each counted, each attributed by `src_id`, none silent:**

| condition | behaviour | counter |
|---|---|---|
| `warp_en` batch with no valid program bound | whole batch degrades to **identity**, loudly | `warp_refused_no_program` |
| bound program's profile ≠ 1 (warp) | batch config refused at bind time, before any vertex | `warp_refused_bad_profile` |
| per-frame warped-vertex budget exhausted (spec constant `WARP_VERTEX_BUDGET`, provisional 4,096) | remaining flagged vertices pass identity | `warp_refused_budget` |
| engine status error on a vertex | the saturated-but-defined engine outputs are applied; status bits accumulate | `warp_status_sat`, `warp_status_rcp0` |

Why identity fallback rather than dropping the stream: a warp vertex is
*geometry* — dropping it holes a mesh, and LOOM's drop-the-whole-stream law
protects a dependency chain warp does not have (each vertex is independent).
Un-warped is the one degraded output that is exactly defined, and **the
counter is the alarm, not the value** — the same argument
`TERRAIN.COMPCACHE.md` makes for substance 3. What keeps this from being a
silent clamp: the refusal counters are read by the differential harness
(2.8) and asserted against the reference's own refusal ledger on every run,
and the charter cut ladder makes the budget constant the *sanctioned* knob —
cutting "Warp8 throughput" (item 5) is lowering `WARP_VERTEX_BUDGET`, with
the degradation visible in a counter, never a hang.

Full counter set: `warped_vertices` (events: vertices accepted AND warped —
not cycles, not beats; the counters-count-cycles defect was found twice in one
block here), `bypass_vertices`, `warp_batches`, the four refusal counters,
`sat_add_events`, `consumer_stall_cycles` (cycles, and *named* cycles).
Every one is asserted somewhere in 2.8; a counter no test reads is
decoration by this repo's own law.

## 2.7 Reference model — compose, do not author

**`zref::GeomWarp`** (the name `design/blocks.yml:2747` already reserves), as
a new thin header `reference/include/zref/zref_geom_warp.hpp`, composing:

1. **lane packing**: build the 14-lane input record in §7.1 order from
   `{vertex, batch config, time}` — owned here, it is the binding this block
   exists to pin;
2. **evaluation**: call `zref::fieldir::interpret`
   (`reference/include/zref/zref_fieldir.hpp:45` — which forwards to
   `zfield::interpret`; NOT a second interpreter, per that header's own
   phantom-reference argument);
3. **application**: `zref::fx_add` per component with the `SatLedger`
   threaded (`reference/include/zref/zref_fixp.hpp:182`);
4. **policy**: the identity-bypass law, the budget, and the refusal taxonomy —
   owned here, nowhere else.

It owns 1, 3 and 4 and explicitly does not own 2 — the same extraction
discipline that put `zhao_project_core` under two projectors so they could
not diverge (`design/blocks.yml:2338`). "RTL matches the oracle" then means
matching the interpreter the TS side mirrors and the committed `.zvec`
goldens pin, which is the strongest oracle this repo has.

## 2.8 Verification plan — each property with the way its test FAILS

Directed (`tests/geometry/geom_warp_directed.cpp`, the path the ledger already
names):

1. **Identity program** (program computes `d = 0`): output bit-equal to
   input, `sat_add_events == 0`. *Fails when:* the adapter loads lanes in the
   wrong order (a nonzero `d` appears because `time` landed in `px`'s
   register), or applies a stale displacement latch from the previous vertex.
   Seen-to-fail check: mutate the lane order in RTL, watch it fail.
2. **Constant displacement**: every vertex shifted by exactly `k`. *Fails
   when:* outputs are read one instruction early — the FIELD.SEQ.CORE
   synchronous-read trap whose symptom is "every chained result reads back as
   the PREVIOUS instruction's answer" (`FIELD.SEQ.CORE.md`). This test exists
   because that failure has already happened once in this cone.
3. **Saturation corner**: vertices at ±(2³¹−1) raw with an outward `d`;
   expect exact saturated values and `sat_add_events` moving by exactly 3
   per vertex. *Fails when:* RTL wraps (sign flips — compare signs, not just
   magnitudes), or the counter counts cycles (run the same case with the
   consumer stalled 10 cycles per beat: the count must not change).
4. **Bypass/warp interleave under stall**: warp, bypass, warp with three
   different `out_ready` duty patterns; outputs byte-identical across all
   three and output count == input count. *Fails when:* the block reorders,
   drops, or **double-emits** — the shell-bench lesson (one meshlet submitted
   15 times from a level-held valid) says to assert the exact count, because
   byte-comparison alone cannot see repeated identical work.
5. **Budget edge**: vertex `WARP_VERTEX_BUDGET` is warped, vertex +1 passes
   identity and `warp_refused_budget` increments by one. *Fails when:* the
   clamp is silent (counter unmoved) or the budget counts beats/cycles
   instead of warped vertices.
6. **No-program and wrong-profile refusals** — and, per the
   broken-instrument law, **every refusal counter is deliberately fired
   once**: a detector that has never been seen to fire has not been tested.
7. **Normal lanes** per the Q3 outcome (baseline: read-and-discard is
   asserted — `n′` must not perturb any output bit).

Randomised (`tests/geometry/geom_warp_random.cpp`): random *valid* W programs
generated through the existing `.zvec` machinery (program-hash-seeded PCG,
field-ir §6.2 — reusing the generator, not writing a second one), random
vertices drawn against declared lane bounds, RTL vs `zref::GeomWarp` on every
output **and every counter**. The consumer's `ready` is driven by a random
duty-cycle schedule that includes long-low and alternating phases — this
repo's own record is 21 checks over a full 15,625-case sweep passing while
answers were dropped, *because every phase held ready high*. The backpressure
phase is where this block's bugs will live (one-in-flight + bypass
interleave), so the random phase asserts, per run: output count == accepted
count, order == input order (via `src_id` sequence, the COMPCACHE trick of
pinning position to a value), and counter deltas == reference deltas.

Mutation sweep to the geometry blocks' existing standard (`GEOM.LOOM.md`,
Randomized section, names the same requirement).

## 2.9 Throughput, honestly

Per-vertex cost on the **v1 scalar walker** (the only measured engine):
≈ 14 lane loads + the walk (docket's ~8 clocks/instruction) + 6 lane reads +
handshake ≈ **8·I + ~24 clocks**. At the docket's 8-instruction example
≈ 88 clocks/vertex; at the 48-instruction ceiling ≈ 408.

With a warp reserve of ~20% of the frame (330k clocks of 1,666,667), that
buys **≈ 3,700 warped vertices/frame at I=8, ≈ 800 at I=48**. That is the
honest v1 tier: one hero creature's spell-hit ripple, a handful of warped
props — *not* scene-wide wind, and four hundred times short of the docket's
120,000-vertex example. The v3 vector fabric adapter raises the population by
whatever the Phase-3 probes actually deliver, and those numbers are targets,
not measurements (`reports/FIELD_V3_COST_MODEL.md` §1). The seam in 2.4 is
engine-agnostic on purpose: the lane binding and the application law are
identical under both engines, so v3 changes the batch scheduler, not the
contract.

The ledger's `target_throughput: 1 warped vertex per clock`
(`design/blocks.yml:2740`) is **unreachable on any existing engine** for any
program longer than one instruction and should be re-stated at revival as
"engine rate; population bounded by `WARP_VERTEX_BUDGET`". (Not edited now —
recon-only lane.)

## 2.10 Honest limits, and what needs the owner

Not established by this document: any fit evidence (the ALM ceiling is a
ceiling, not a fit); the v1 per-vertex cost model (derived, see below); the
v3 adapter's real rate; whether identity-fallback vs batch-drop is the right
refusal aesthetic *in scene* (the art law says look at a degraded frame, not
reason about it); composition with GEOM.WCACHE's dual-view replay (a warped
vertex must be warped identically in both views — replay-after-warp, which
the WCACHE arena's project-once law should give for free, but "should" is
not a capture).

**Owner questions** (Class C under the A/B/C split — game-visible behaviour):

* **Q1 — Revival.** Options: (a) keep deferred, adopt this spec as the
  revival-ready text; (b) revive at the v1 selective tier now; (c) wait for
  v3 probes. **Recommendation: (a).** §6.3's own bar is an evidence-backed
  revival, and the evidence that exists (workloads.yml 4.6×-frame example,
  CREATURESANDLIGHTS "deferred and unnecessary") still points the other way.
  This document exists so that when a game capture shows an effect the
  sidecar+Loom+HPS path cannot do, revival costs days, not weeks.
* **Q2 — Amplitudes `a0..a3`.** (a) per-batch constants, zero format change
  (v1 proposal); (b) a new vertex format with per-vertex amplitude bytes —
  SW.TOOLS.ASSET's spec to write, not ours; (c) derive from `w0`.
  **Recommendation: (a)**; (b) only when an effect demonstrably needs
  per-vertex falloff the program cannot compute from position.
* **Q3 — Warped normals.** (a) position-only v1, `n′` read-and-discarded,
  lighting keeps the unwarped normal; (b) route `n′` to GEOM.LIGHT — a new
  ledger edge and a LIGHT input the current graph does not have.
  **Recommendation: (a)** for v1, with the lighting delta *looked at* in a
  capture before (b) is even priced.
* **Q4 — Fixed W export registers** for v3 snooping: a validator table edit,
  to be agreed with the FPLAN/ops owner, not invented here.

## 2.11 The thing most likely to be wrong in this specification

**The v1 per-vertex cost model (8·I + ~24).** It is derived from the docket's
worked example and v2's measured transport, not measured on the v1 walker with
a real W-shaped program — and v2's measured transport was ~25 clocks/vertex
for a *12-in/4-out* profile; warp is 14-in/6-out through a synchronous-read
register file, so the constant could plausibly be ~40+, halving every
population number in 2.9 and moving the recommended `WARP_VERTEX_BUDGET`.
Every tier claim downstream of that constant moves with it. The first act of
any revival should be a one-afternoon measurement: one committed W program,
one vertex, count the clocks — before a single line of RTL.

---

*Recon sources are cited inline throughout; the load-bearing ones are
`design/blocks.yml:2722–2751`, `design/contracts/GEOM.WARP.md`,
`design/contracts/FIELD.SEQ.WARP.md`, `design/contracts/FIELD.SEQ.CORE.md`
(2026-08-27 amendment), `reports/OWNER-RULINGS-COMPLETE-20260831.md:605–617`,
`ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md:1902–1913`,
`spec/form/field-ir.md` §§5–7, `design/budgets/workloads.yml:289–296`,
`reports/CREATURESANDLIGHTS:186–193`, and
`reference/include/zref/zref_fieldir.hpp`.*
