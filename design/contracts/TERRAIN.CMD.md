# Contract — TERRAIN.CMD (SubmitTerrainSet, turned into a frame)

> Ledger: `design/blocks.yml` · gpu clock · maturity UNIT_VERIFIED
> RTL: `fpga/rtl/terrain/zhao_terrain_cmd.sv`
> Reference model: `zref::swstream::encode_record` — `reference/include/zref/zref_sw_stream.hpp`
> Tests: `tests/terrain/terrain_cmd_rtl_directed.cpp` (+ `tests/terrain/tb_terrain_cmd.sv`)

## Purpose

Fetch the sealed patch list a `SubmitTerrainSet` names, verify it, and drive
`TERRAIN.SEQ`'s frame ring with the records it contains.

`reports/DOCKET.md` D4 lists four things missing from the 8 km world — the
pager, the residency manager, the composed-height cache and the
**command→terrain pipeline**. The first three are built and unit-verified. This
is the fourth's middle: before it, the composed bench played `fr_start`,
`fr_epoch`, `fr_patch_count` and a `rec_*` stream it constructed in C++, and no
command a game could issue made any of it happen.

## The list is capture data, which decides almost everything

Ruling T5: *"The sealed list is capture data — replay does not rerun the HPS
visibility walk."* `zref::swstream` did the walk, unioned the views, applied
T7's prefetch policy, sorted by `canonical_less` and sealed the result.

So this block **does not sort, merge, deduplicate, filter by view mask or decide
what is required.** It reads bytes in order. Any of those verbs here would be a
second source of truth for the thing the determinism ledger is anchored on, and
its first divergence would be a replay that renders differently on the machine
that recorded it.

## Two passes, because "verify before acting" is a real constraint

T5 gives the list a CRC and the whole point is that a corrupt list must not be
acted on. Acting on record 0 and discovering at record 200 that the CRC is wrong
is not verification — 200 loads have been issued and 200 slots claimed.

| option | cost |
|---|---|
| buffer the list | 32 B × 256 composed patches = 65,536 bits ≈ **7 M10K**, held for one command |
| **read it twice** | 16 KB of HPS traffic against T7's 684 KB of pages — **+2.4%** |

Pass one folds the CRC and emits nothing; pass two emits and folds nothing. A
mismatch after pass one refuses the command with **no record offered to
anybody** — which is a checked claim, not a description: the directed suite
flips a byte in the *last* record and requires zero records and zero frames.

## A record is exactly four beats, and that is arithmetic

`zref::swstream::encode_record` lays the 32 bytes out little-endian in T5's
field order, by hand rather than by `memcpy` — a struct's padding is the
compiler's business and a capture that replays only on the machine that made it
is not capture data.

| bytes | field | beat |
|---:|---|---|
| 0–3 | `island_id` u32 | 0 `[31:0]` |
| 4–5 | `patch_ix` i16 | 0 `[47:32]` |
| 6–7 | `patch_iz` i16 | 0 `[63:48]` |
| 8–15 | `hps_page_addr` u64 | 1 |
| 16–19 | `expected_page_crc32c` u32 | 2 `[31:0]` |
| 20–21 | `flags` u16 | 2 `[47:32]` |
| 22 | `view_mask` u8 | 2 `[55:48]` |
| 23 | `priority` u8 | 2 `[63:56]` |
| 24–27 | `source_id` u32 | 3 `[31:0]` |
| 28–31 | `reserved` u32 | 3 `[63:32]` |

No field straddles a beat and no record straddles a burst — **provided the list
starts 8-byte aligned**, which is checked, because an unaligned list would shear
every field in it and still produce plausible terrain.

## The tail burst is short, deliberately

224 bytes is three 64-byte bursts and one of 32. A block that always asked for
64 would read 32 bytes that are not in the list, fold them, and fail its own
CRC. The bench's played bridge derives its beat count from `len` for the same
reason — the composed world bench's bridge answers eight beats always, and
against that model this block would look broken while being right.

## Measured cost, and the waste in it

Eight records, unstalled: 138 cycles, 12 bursts, 92 beats for a 256-byte list.
The ideal is 8 bursts and 64 beats; the extra comes from pass two **abandoning
the rest of a burst** to hold a record `TERRAIN.SEQ` is not ready for, then
re-requesting from the byte after it. With two records per burst that costs one
re-request per burst.

That is a deliberate trade against a one-record skid buffer, and it is written
down rather than hidden: the block is not on the frame's critical path (the
sequencer never waits on a load) and 28 extra beats per frame is nothing beside
T7's 684 KB of pages. If a measurement ever says otherwise, the skid goes in
`S_HOLD` and nothing else changes.

## Verdicts, all of them fired by the suite

| code | meaning |
|---:|---|
| 0 | ok |
| 1 | `resource_epoch` ≠ live epoch |
| 2 | `list_bytes` ≠ 32 × `patch_count` |
| 3 | `patch_count` > `MAX_PATCHES` |
| 4 | the list is not 8-byte aligned |
| 5 | the list runs past the arena |
| 6 | `list_crc32c` mismatch |
| 7 | the bridge reported `err` |
| 8 | `patch_count` == 0 |
| 9 | `sequence` does not fit the ring |

**Every refusal happens before a byte is read.** A command that is going to be
refused must not have issued a burst, or `bridge_errs_o` and `list_bytes_read_o`
measure this block's bookkeeping instead of the fabric — and the suite asserts
`bursts_seen == 0` on all six of the pre-read refusals.

**One command, one completion, always.** The rule `TERRAIN.PAGELOADER`'s
contract states, for the same reason: a refusal that produced silence would
leave whoever issued the command waiting on a frame that will never start.

### Verdict 9 is a width disagreement, not a malformed command

T5's `sequence` is a **u32**; `TERRAIN.SEQ`'s frame ring carries **16 bits**.
Truncating would put two different frames on the same ring sequence and the
determinism ledger would never see it, so a sequence that does not fit is
refused. **Which of the two widths is wrong is not this block's to decide** and
is listed below as an open ruling.

## What the test found that the contract had not

`err` on the **request** channel was ignored. The bridge's own comment is
*"malformed burst / bridge error: nothing issued"* — so `err` there is a refusal
of the request, arriving with no grant, and watching for it only during the beat
stream meant the pulse landed while the block was still waiting, was missed, and
the next cycle granted normally. The command completed happily and the suite
reported **eight records where it wanted none**. Found by firing the verdict
rather than by reading the contract, which is the argument for firing all of
them.

## Exclusions

It does not decide residency, load pages, compose, or walk visibility. It does
not interpret `flags`, `view_mask` or `priority` — those ride the record to
`TERRAIN.SEQ`, which owns what they mean.

**And it does not decode the command.** Its job port takes the fields already
unpacked, because the command seam is not this block's to answer: the shell's
record framer carries **sixteen** payload bytes (`zhao_shell_top.sv:1712`) and
`SubmitTerrainSet`'s payload is thirty-two, so `patch_count` — T5's seal — does
not reach the scheduler today. That is a command-path limit, not a terrain one:
`TerrainField 0x0200` is a 112-byte record and is truncated the same way.
Solving it inside this block would be the terrain lane working around a defect
that belongs to the command lane.

## OWNER RULINGS NEEDED

1. **The command payload seam** — framer widening versus a re-read from memory.
   See `reports/TERRAIN-COMMAND-PIPELINE-20260907.md`; the framer is already the
   worst setup group on `gpu_clk`.
2. **`sequence`: 32 bits or 16?** T5 says u32, the ring says 16. Refused rather
   than truncated, pending.
3. **The set-level `view_mask` and `flags` have no consumer.** T5 gives
   `SubmitTerrainSet` both; `TERRAIN.SEQ`'s ring takes `{epoch, patch_count,
   sequence}` and every *record* carries its own `view_mask`. They are not on
   this block's job port, because accepting and dropping them would look like
   they were handled.
4. **What a bad `list_crc32c` does to the FRAME.** This block refuses the
   command; whether the frame then repeats the previous complete one under T6 or
   proceeds with the previous set is not ruled.
5. **`TerrainEpoch` 0x0220 is not implemented at all** — BEGIN/END_FLUSH/ABORT,
   and what "drain" means in gates.

## Scalar reference function

`zref::swstream::encode_record` and `zref::swstream::PatchRecord`
(`reference/include/zref/zref_sw_stream.hpp`), with `zhao_abi::zhao_crc32c` as
the list CRC oracle — the same function the hardware folds, so there is one
definition rather than two to keep in step.

**The oracle is the encoder read backwards.** Encode a list, put it in the
arena, and require that the ten fields handed to `TERRAIN.SEQ` are field for
field the records that went in, **in order**.

## Evidence

Every way this block can be wrong survives a count: a sheared field is terrain
in the wrong place; a reordered stream renders correctly on the machine that
recorded it; a truncated stream is invisible downstream because `patch_count` is
the seal `TERRAIN.SEQ` stops at anyway; a list acted on before its CRC was
checked has already issued loads.

So the whole stream is compared, field by field, with a fixture in which every
confusion is a different number: each field a distinct function of the record
index, `patch_ix`/`patch_iz` taking **both signs** (they are i16 and a
zero-extending read passes every test drawn from positive coordinates), and
`hps_page_addr` above 2³² so a 32-bit truncation cannot hide.

Even and odd list lengths (the 32-byte tail burst); four stall patterns, one
mostly-ready, on the **record port** — where it matters, because that is what
drives the abandon-and-re-request path; the corrupt list; all nine verdicts; the
bridge failing in both of its shapes; and a good set after eight failures,
because a block that faulted correctly and then never submitted again would pass
everything else.

**77 checks, 0 failures.**
