# PART.TABLE — the species descriptor table and the size/colour curve table

> Ledger: NOT YET REGISTERED in `design/blocks.yml`. Written 2026-09-19 with the
> block; the ledger entry is owed and is named as owed at the foot of this file.
> RTL: `fpga/rtl/particles/zhao_part_table.sv` · tests:
> `tests/particles/part_table_directed.cpp`

## Why this contract exists

Four contracts each handed this table to somebody else and the chain closed on
nobody:

* `PART.UPDATE.md` — *"Reads the species table (on-chip, loaded per frame, owned
  by `PART.STATE`)"*
* `PART.COLLIDE.md` — *"This block owns no memory and performs no fetch"*
* `PART.SPAWN.md` — *"The species table is read as a port"*
* `PART.STATE.md` — *"Species descriptors are read-only and small; they belong
  in an on-chip table loaded per frame"*, while its **Memory ownership** section
  says it owns *"the two particle buffers in HPS DDR **and nothing else on
  chip**"* and its port list has no descriptor port at all.

`fpga/rtl/prod/zhao_console_core.sv` recorded the consequence as gaps **I2** and
**I3**: three built blocks read a table that nothing implements.

This is exactly the failure CLAUDE.md's *"read the SIBLING contract"* section
describes — two blocks' contracts pointing at the same arithmetic and neither
saying who owns it, which is how the projector came to exist twice. **This file
is the answer to that question for the descriptor table: PART.TABLE owns it, and
nobody else stores a descriptor.**

## In

* **the per-frame load** — one word per clock: `{sel, index, event, data}`.
  `sel` picks the update slice, the collide slice, a spawn rule or a curve
  bucket. Never refused for backpressure; refused only for an index outside the
  table.
* **four read indices**, one per consumer view — see **Out**.

## Out

Width-for-width and name-for-name the ports the three consumers already declare.
**These shapes are not negotiated here.** The RTL's header carries the full
port-by-port map; the summary is:

| consumer | index it presents | what it reads back |
|---|---|---|
| `PART.UPDATE` | `spc_index_o`, 7 b | recipe, lifetime, age_mark, drag, grav, strength, cx/cy/cz, p0/p1/p2 |
| `PART.UPDATE` | `crv_index_o`, 4 b | size, colour |
| `PART.COLLIDE` | *(none — species of the record on its input wire)* | response, restitution, friction, damping |
| `PART.SPAWN` | `spc_species_o` + `spc_event_o`, 7 b + 2 b | known, child species, count |

## Memory ownership

**Owns the whole descriptor table and the curve table, and nothing else.**
No other block may hold a descriptor: a second copy is a second answer.

At the production tier (`SPECIES_N` 128, `CRV_N` 16) the stored bits are

    update slice   141 b x 128 = 18,048 b
    collide slice   51 b x 128 =  6,528 b
    spawn rule      13 b x 512 =  6,656 b   (4 events per species)
    curve entry     14 b x  16 =    224 b
                                 ---------
                                  31,456 b

**In RAM, not flip-flops.** The RTL's header carries the arithmetic: roughly
**~870 ALM as MLAB** against roughly **~18,400 ALM as registers plus read
muxes**. Every one of those figures is **shape arithmetic, not a measurement** —
owner ruling 2026-09-18 point 3, *"Logical bits are still not physical M10Ks"* —
and the block has not been through `quartus_map`.

**Ceiling: 1,200 ALM, 0 DSP, 0 M10K.** The zero M10K is not a preference; see
the next section.

## Latency (fixed or variable)

**Zero. Every read is COMBINATIONAL, in the same cycle as its index.** This is
inherited, not chosen, and it is why the table cannot be in M10K:

* `PART.UPDATE` states it in its own port comment and gives the reason — *"A
  bank that registers its read and a stage that holds its request move apart on
  a stall and deliver one record's data with another's metadata"*, which is this
  repository's own metadata-swap defect;
* the CURVE index is `PART.UPDATE`'s **advanced** age, computed inside that
  block, and exists nowhere one cycle earlier;
* `PART.SPAWN` presents `{par_q, ev_q}` on entering `S_EVAL` and consumes the
  reply in that cycle. Fetching it earlier means replicating its event priority
  encoder, and that encoder **is** its determinism contract;
* `PART.COLLIDE` emits no index at all — the descriptor is sampled with the
  particle.

A Cyclone V M10K has a mandatory address register, so its minimum read latency
is one clock. MLAB's read can be unregistered. **The M10K variant is priced in
the RTL header** (4 + 2 blocks for the two big slices, ~105 ALM per M10K, below
the owner's own ~200 ALM/M10K ranking bar) and it requires making this block a
lookup STAGE on the record path — a composition change, not a block change.

## What this block does NOT do

* **It does not validate a descriptor it serves.** `PART.UPDATE` refuses an
  out-of-vocabulary recipe, `PART.SPAWN` refuses a count above 16 and an
  out-of-range child species, `PART.STATE` refuses an out-of-range species —
  three blocks, three counters, three tests. A fourth opinion here could
  disagree with them. The one refusal this block owns is a **load** outside the
  table, and nobody else can make it.
* **It has no blanket clear.** `known` is a bit of the spawn word; a species and
  event with no rule is loaded with `known = 0` like any other datum. A
  clearable 512-entry presence vector is ~300 ALM of flip-flops to spare the
  host 512 writes a frame, which is the opposite of what this block is for.
  **A partial load leaves the previous frame's rules in place** and that is the
  host's discipline, stated rather than silently guarded.
* **It has no read-valid channel** for update, collide or curve, because neither
  contract has one. An entry never loaded reads as whatever the array holds.

## The load word

**AUTHORED here.** `reference/include/zref/zref_particle.hpp` says plainly that
*"there is no species table — not in `reference/`, not in `spec/`, not in
`design/`. That is a DATA/ABI question and it is properly the owner's"*. So the
FIELDS come entirely from what the three consumers already read, and the
OFFSETS are this block's own decision, in named localparams, with an elaboration
guard that the map tiles the word exactly.

`base_radius_fx16`, which zref names as the missing per-species datum, is
**deliberately absent**: no built block reads it, and carrying it would be
inventing an ABI ahead of the owner.

## Counters and traces

`loads_update_o`, `loads_collide_o`, `loads_spawn_o`, `loads_curve_o`,
`load_refused_o`. Every one is fired as a DELTA by the directed suite.

**`load_refused_o` is unreachable at the production tier and that is stated
rather than hidden**: 7 bits address exactly 128 species and 4 bits exactly 16
buckets, so no legal index is out of range. The bench runs at `SPECIES_N = 8`
and `CRV_N = 8` for the same reason `PART.STATE`'s runs at `CAPACITY = 8`.

## Status

Built 2026-09-19. Lint-clean under `verilator --lint-only -Wall`, 68 directed
checks, **never through `quartus_map`** — which settles one tool's opinion and
not synthesizability.

**OWED, and named so it is not forgotten:**

1. a `design/blocks.yml` entry with id `PART.TABLE`, subsystem `particles`,
   upstream `CMD.SCHEDULER`, downstream `PART.UPDATE`, `PART.COLLIDE`,
   `PART.SPAWN`. Not written on 2026-09-19 because another agent held the file.
2. composition into `zhao_console_core.sv`, which is what actually closes gaps
   I2 and I3. **The block existing does not close them** — a thing BUILT is not
   a thing INSTALLED.
3. the sentence in `PART.UPDATE.md`, `PART.COLLIDE.md`, `PART.STATE.md` and
   `PART.SPAWN.md` that hands the table to somebody else should be amended to
   name PART.TABLE. Until it is, four contracts still point at the wrong owner.
