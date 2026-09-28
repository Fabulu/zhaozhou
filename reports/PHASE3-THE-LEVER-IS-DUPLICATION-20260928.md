> # CORRECTION, WITHIN THE HOUR: THE 40,091 IS NOT AVAILABLE AND THE TITLE IS WRONG
>
> **I counted instances without checking their SIZES. The `n` column implies
> "n comparable copies" and for two of the largest rows that is false.**
>
> ```
> zhao_vertex_arena      n=4  total 7,694  largest 7,553 = 98%   rest 75, 51, 15
> zhao_field_v3_mulbank  n=2  total 3,328  largest 3,328 = 100%  rest 0
> ```
>
> `vertex_arena` is ONE real instance (`geom_proj_lane -> geom_wcache`) beside
> three `g_copy[*]` siblings that are already memory-backed and cost **141 ALUTs
> between them**. `field_v3_mulbank` is one instance and one that owns NOTHING.
> **Neither is a duplication opportunity, and together they are 11,022 of the
> 40,091 I claimed.**
>
> Of what remains, the evenly-split rows are mostly LANES, which section 3 below
> already ruled off the table: `attrgrad_v2` 6 x ~700 and `attrdiv_v2` 6 x 653
> are raster parallelism, and sharing them cuts capability.
>
> **GENUINE CROSS-SUBSYSTEM DUPLICATION, measured per instance:**
>
> | module | recoverable | note |
> |---|---:|---|
> | `zhao_field_v3_normalize` | ~2,624 | 2,739 + 2,624, FIELD + TERRAIN |
> | `zhao_field_isqrt` | ~776 | 328 + 263 + 263 + 250 |
> | `zhao_geom_mat3x4_mul` | ~581 | 588 + 581, LOOM + POSE_DECODE |
>
> **About 4,000 ALUTs, not 40,091 -- roughly 6% of the 66,766 needed.**
> `crc32c_fold` (x14) and `field_rcp24_rom` (x13) are evenly split but spread
> across many independent consumers, so sharing them needs arbitration and a
> read-port budget; they are not free either.
>
> **AND THE ROM IS NOT THE CHEAP ROW I CALLED IT.** `zhao_field_rcp24_rom` is a
> 256-entry x 31-bit combinational `unique case` -- a SAME-CYCLE lookup. M10K
> inference needs a REGISTERED read, so moving it to memory adds a pipeline
> stage at 13 call sites. At 144 ALUTs per instance for 7,936 bits Quartus has
> already packed it hard. It is a timing change, not a declaration change.
>
> **THE HONEST CONCLUSION, WHICH IS AN ESCALATION.** After checking four
> candidate classes -- deep arrays (exhausted), small arrays (correctly in
> flops), ROMs (need a pipeline stage) and duplicated instances (~4,000 ALUTs)
> -- **there is no identified path to 23% that does not touch capability.** The
> vacation directive is explicit that delegated authority does not extend to
> cutting lanes or calling reduced work equivalent to fit a device, and it says
> a measured conflict of exactly this kind is to be escalated rather than
> resolved locally. **This is that conflict, and it is the deliverable.**

# Phase 3: the state-relocation lever is EXHAUSTED; the remaining one is duplication

Coordinator, 2026-09-28, measured from `@current-20260928` (`5CSEBA6U23I7`,
A&S successful, 0 errors, `rtlCleanAtHead` at `83c63661`).

## 1. The flop-array trade is finished, and that is a measurement

Every deep array left in the tree **already infers as memory**. Checked by
intersecting `check_ram_inference.py --rank` against the current map's
per-entity `reg_own`/`mem`:

| array | bits | owning module `reg_own` | `mem` |
|---|---:|---:|---:|
| `forge_cliff_ram.prio_mem_r` | 65,536 | 562 | 55,428 |
| `audio_fifo.mem` | 65,536 | 268 | 65,536 |
| `post_composite.ring_q` | 55,296 | 1,475 | 64,512 |
| `terrain_residency_v2.keyram` | 27,392 | 1,029 | 150,528 |
| `geom_arenabin.head/tail_ram` | 10,368 ×2 | 1,005 | 291,456 |

**There are no deep arrays sitting in flip-flops.** FLOPARRAY took the last two
(`forge_assemble`, `geom_lodstate`) on 2026-09-26. The remaining 279,210
registers and 293,886 ALUTs are genuine logic and small-table storage.

This confirms the standing guidance from the other direction: *"ALMs are the
binding constraint; memory is the slack, **but the lever is
lookup-for-computation, not relocating state**."* Relocating state is now
measured into a dead end.

**And the small arrays are not a disguised opportunity.** `zhao_geom_ladderbank`
looked ideal -- 5,951 registers, 2,574 ALUTs, zero memory, four arrays the scan
flags with the init-loop shape. Its arrays are `2*ROWS = 32` entries deep
(~4,864 bits total, plus a 512-bit `line_q`), which accounts for its registers
almost exactly. **A 32-deep array is not an M10K candidate** -- one would waste
8,192 of 9,216 bits -- and this is the "small array correctly in flops" case the
ranking tool exists to separate out.

## 2. What is left: 246 free M10K and 32,784 ALUTs of pure computation

Memory headroom: **2,274,745 free bits = 246 M10K**, 40% of the device.

Blocks with **zero memory and ALUTs well above registers** -- computation with no
table at all:

| module | `alut_own` | `reg_own` | dsp |
|---|---:|---:|---:|
| `zhao_terrain_devstore` | 7,070 | 4,418 | 0 |
| `zhao_field_v3_mulbank` | 3,328 | 8 | 4 |
| `zhao_raster_edgewalk` | 2,983 | 942 | 2 |
| `zhao_field_v3_spline` | 2,806 | 1,298 | 0 |
| `zhao_field_v3_normalize` | 2,739 | 1,322 | 0 |
| `zhao_part_update` | 2,152 | 687 | 0 |

Twelve such blocks own **32,784 ALUTs, 11.2% of the design**.

## 3. THE BIGGEST SINGLE LEVER: 40,091 ALUTs IN DUPLICATED INSTANCES

60% of the 66,766 ALUTs that stand between us and a placeable design are in
modules instantiated more than once:

| module | n | ALUTs | distinct parents |
|---|---:|---:|---|
| `zhao_vertex_arena` | 4 | 7,694 | `geom_proj_lane`, `proj_subsystem` |
| `zhao_field_v3_normalize` | 2 | 5,363 | `field_host_v2`, `terrain_heighttap` |
| `zhao_raster_attrgrad_v2` | 6 | 4,286 | `shell_top_v2` |
| `zhao_raster_attrdiv_v2` | 6 | 3,918 | `shell_top_v2` |
| `zhao_field_v3_mulbank` | 2 | 3,328 | `field_host_v2` |
| `zhao_raster_rcp24_v4` | 5 | 2,547 | four parents |
| `zhao_crc32c_fold` | 14 | 2,403 | many |
| `zhao_hps_arbiter_n` | 2 | 2,092 | `shell_top_v2` |
| `zhao_field_rcp24_rom` | 13 | 1,727 | many |
| `zhao_field_v3_ring` | 2 | 1,706 | `field_host_v2` |
| `zhao_geom_mat3x4_mul` | 2 | 1,169 | `geom_loom`, `geom_pose_decode` |
| `zhao_field_isqrt` | 4 | 1,104 | `field_host_v2`, `terrain_heighttap` |

**THESE ARE TWO DIFFERENT THINGS AND MUST NOT BE TREATED AS ONE.**

* **LEGITIMATE PARALLELISM.** `attrgrad_v2` x6 and `attrdiv_v2` x6 are all
  inside `shell_top_v2` -- six raster lanes, by design, for throughput. Sharing
  them cuts capability, which the vacation directive explicitly does NOT
  authorise. **These rows are not available** without an owner decision, and
  quoting the 40,091 total as though they were would be exactly the "reduced
  work called equivalent" the directive forbids.
* **CROSS-SUBSYSTEM DUPLICATION**, which is the projector pattern this
  repository has already solved once: `field_v3_normalize` (FIELD + TERRAIN),
  `field_isqrt` (FIELD + TERRAIN), `geom_mat3x4_mul` (LOOM + POSE_DECODE),
  `vertex_arena` (two projector parents). **These are the real candidates.**

### `zhao_field_rcp24_rom` x13 is the sharpest row

A **ROM** instantiated thirteen times for 1,727 ALUTs. A ROM is a table, the
device has 246 spare M10K, and thirteen copies of one table is the exact shape
"lookup-for-computation" exists to fix.

> **MEASURED 2026-09-28, and the sentence that stood here was wrong.** It read:
> *"It is also the cheapest thing on this list to reason about, because a ROM has
> no state and no throughput contract -- **only a read port count**."*
>
> **That ROM has no read port at all.** `zhao_field_rcp24_rom` is a 256-arm
> `unique case` inside an `always_comb` -- a 256:1 mux over 31 bits. There is no
> memory for Quartus to infer: no clock, no register, nothing. Mapped standalone
> it is **145 comb ALUTs and ZERO memory bits**.
>
> **But the repair is CHEAPER than that correction implies, and that is also
> measured.** A five-row map sweep (`zhao_probe_rcp24_rom`, STYLE=0..3, with a
> plain RAM as positive control -- it fired) shows that **registering the read is
> SUFFICIENT**: Quartus infers `altsyncram … depth 256, width 31, mode ROM`
> straight from the case statement. **145 comb ALUTs -> 0.** No restructuring
> into an indexed array is needed, and the register may sit outside the table.
>
> So the packet is **one cycle of latency at each of 13 call sites and nothing
> else** -- a real contract change, since `zhao_field_v3_normalize` declares
> `latency: fixed:N`, but not a rewrite.
>
> Full numbers, caveats and the probe-design correction:
> `MEASURED-20260928-A-REGISTERED-READ-TURNS-THE-CASE-INTO-A-ROM.md`.

## 4. What this does NOT license

The directive is explicit that delegated authority is **not** permission to
delete a feature, cut 16 fields to 4, remove Gouraud or detail normals, shrink
the guaranteed giant, or call reduced work equivalent to fit a device. Every
lane-parallelism row above is off the table on those grounds until the owner
says otherwise. **A measured engineering impossibility is a finding, not
permission to invent a pass** -- and if consolidation of the legitimate
candidates does not reach 23%, that conflict is an escalation, not a licence.

## 5. The next packet

Consolidate `zhao_field_v3_normalize` and `zhao_field_isqrt` behind a service,
exactly as `zhao_project_service` did for the projector -- a play this tree has
run to completion once already, with the prerequisite cache built and the
second core removed (the map confirms **one** `zhao_project_core` today).

Verify with standalone `-MapOnly`, which is how FLOPARRAY proved -28,527
estimated ALMs without a console fit. **That matters more than usual here: the
console fit CANNOT RUN, so standalone measurement is the only verification
available until the design is 23% smaller.**
