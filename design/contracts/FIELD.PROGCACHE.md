# Contract — FIELD.PROGCACHE (Field program cache)

> Ledger: `design/blocks.yml` · owner ZH-032 · phase 7 · maturity SPECIFIED

## Purpose and exclusions

Cache Field IR microprograms + constant tables with hash/version check; a program that fails validation is safely rejected (charter §19.4), never executed.

## Clock and reset semantics

Single clock `clk`, asynchronous active-low `rst_n`, `gpu` domain per the ledger.
Reset empties the directory (every entry invalid), zeroes the four counters and
the occupancy, and clears both response registers. No clock-domain crossing lives
in this block.

## Input and output packet layouts

**The transaction is TWO-PHASE**, and it has to be:

*Phase A, lookup* — `lu_valid_i` / `lu_ready_o` / `lu_hash_i` (u32, the hash the
command declared). Answers on `lu_resp_valid_o` / `lu_resp_ready_i` with
`lu_hit_o` and `lu_slot_o`.

*Phase B, commit* — only after a miss, once the caller has decoded.
`cm_valid_i` / `cm_ready_o` / `cm_hash_i` (the hash `decode` computed) /
`cm_ok_i` (the decode verdict, one bit). Answers with `cm_inserted_o`,
`cm_slot_o` and `cm_evicted_o`.

One phase would not do. The decode a miss requires costs orders of magnitude more
than the lookup, so decoding every offered program just to discover it was
already resident would make the cache cost more than it saves. The two phases are
where the saving comes from.

`cm_ready_o` is low on any cycle a lookup fires. A commit always follows a miss,
so the phases are naturally sequential for one caller; making that explicit
removes the only interaction between them, which is that both restamp the LRU.

## Backpressure rules

Ready/valid on all four ports. Each phase holds its response until taken:
`lu_ready_o = !lu_resp_valid_o || lu_resp_ready_i`, and the same for commit
(with the lookup interlock above).

Nothing is ever dropped and no request is answered twice. A stalled consumer
simply stops the block accepting new requests.

## Memory ownership

**The directory only.** Sixteen entries of {valid, 32-bit hash, 48-bit LRU
stamp} — about 1,300 flip-flops.

**The program store is NOT owned here**, deliberately. Microcode plus constant
tables for sixteen programs is a memory sizing decision with real alternatives,
and settling it inside this module would spend the budget without anyone
choosing — the same reasoning `zhao_geom_pose_cache` records for palettes. This
block hands out a slot index and the caller owns the store.

## Q formats and rounding

**None.** There is no arithmetic in this block beyond an LRU counter increment
and index comparison. No fixed-point value passes through it.

The one number it handles is the 32-bit program hash, which is an opaque
identifier: it is compared for equality and never interpreted.

## Latency (fixed or variable)

Fixed, one cycle per phase: the directory is registers compared in parallel, so
a lookup answers the cycle after it is accepted, and so does a commit.

Total cost of an acquire that hits: one cycle. Of one that misses: one cycle, plus
whatever the caller's decode costs, plus one more cycle to commit.

## Target throughput

The ledger's line is "1 instruction issue per clock per sequencer", which
describes the sequencers this block feeds rather than this block. What this block
owes them is that a resident program costs a single cycle to find, and it does.

A miss is bounded by the caller's decode, not by anything here.

## Overflow and malformed-input behaviour

**A program that fails validation is rejected and never cached.** That is the
ledger's one hard sentence and it is obeyed literally: on `cm_ok_i` low nothing
is written, no slot is consumed, no entry is evicted, and no LRU stamp moves.
`programs_rejected` counts it.

**A rejection is not a miss.** `misses` counts programs that were decoded AND
inserted. A rejected image was never a cache miss, it was a bad program, and
conflating the two makes the hit rate look worse than it is while hiding a
content bug.

**A rejection is not remembered either** — see the reference's chosen policy 2.
The same bad image offered three times is validated and rejected three times. The
cost is real and is stated rather than hidden; the alternative, a negative cache,
would keep a program rejected after it became valid.

A full directory never refuses. It evicts the least-recently-used entry and
counts `evictions`. Unlike the pose cache there is no referenced-this-frame mark,
because a program in use is re-acquired on the cycle it is needed rather than
held across a frame.

## Counters and traces

Four, exactly the ledger's set plus one:

| counter | meaning |
| --- | --- |
| `hits_o` | a lookup found a resident program; nothing was decoded |
| `misses_o` | a decoded, valid program was inserted |
| `programs_rejected_o` | `decode` said no; nothing was cached |
| `evictions_o` | an insert displaced a live entry |

`evictions_o` is not in the ledger's list and is exposed anyway: without it, a
cache that is thrashing and a cache that is comfortable produce the same hit and
miss counts over a frame, and the difference is exactly the thing a sizing
decision needs.

`occupancy_o` reports how many slots are live.

No trace hookup yet.

## Scalar reference function

`zref::field::ProgCache` — `reference/include/zref/zref_progcache.hpp`.

**The ledger previously declared `zref::ProgCache`, which did not exist** — one of
the twenty-five phantoms in `reports/PHANTOM_REFERENCES.md`. This block turned
out to be a hybrid of two of the three kinds that audit identified:

* **the validation half was already law.** `zfield::decode` re-validates a
  `.zprog` image against `spec/form/field-ir.md` §4/§5 with thirteen named error
  classes. The reference FORWARDS to it and does not re-implement a byte of it.
* **the cache half had no law anywhere.** `design/blocks.yml` gives one sentence
  and field-ir.md says nothing about residency, so the policy is CHOSEN and every
  choice is recorded in the reference with the alternative it beat.

## Directed tests

`tests/field/field_progcache_directed.cpp` — 123 checks against
`zref::field::ProgCache`.

**Real programs, not synthetic hashes.** The three committed `.zprog` images
(crater_ring, impact_wave, wave_pool) are the fixtures, and the rejection cases
are those images corrupted — a flipped body byte (`kBadCrc`), broken magic
(`kBadMagic`), a truncated tail (`kBadLength`). A test that invented an
`ok = false` flag would assert its own opinion about what failure looks like
rather than the loader's.

The cache is instantiated with **two** entries for the test. Sixteen is the
shipping size and there are only three real programs to fill it with; at two,
three programs exercise every residency law there is — fill, hit, evict, and the
LRU choice between two live candidates — using images the loader accepts. A
sixteen-entry cache driven by made-up hashes would test more slots and less law.

The LRU section probes **asymmetrically**: it touches one program, inserts a
third, and then asks only about the one that should have survived. Asking about
both would give the same hit and miss totals under either policy, with the roles
swapped — the mistake the pose cache's first eviction test made.

## Randomized differential tests

`tests/field/field_progcache_directed.cpp --random N`. 60 iterations in the fast
lane, 800 nightly; 5,556 checks clean at 100.

Each iteration replays a random sequence drawn from a pool of six images — the
three real programs and three corrupted variants — so hits, misses, evictions and
rejections interleave in orders no directed case would write. Every acquire is
compared, not just the totals at the end.

## Formal properties

None yet.

Worth proving, in rough order of value: (1) a rejected commit leaves the
directory bit-identical — no entry, no stamp, no occupancy change; (2) the
directory never holds two live entries with the same hash, which is what makes a
hit unambiguous; (3) `occupancy` equals the number of valid entries at all times.
All three are small state properties over sixteen entries and are well within
reach of a bounded proof.

## Synthesis / resource ceiling

Not yet fitted; no number is claimed.

By construction: sixteen entries of {1 + 32 + 48} bits is about 1,300 flip-flops,
plus sixteen 32-bit equality comparators and a sixteen-way minimum over the LRU
stamps. No memory, no arithmetic, no DSP.

**Tags in registers compared in parallel is the opposite of the pose cache's
choice, on purpose.** That directory is 128 entries, where parallel comparison
means ~6,300 flip-flops of tag and a sequential scan is cheaper. This one is
sixteen: scanning it would cost sixteen cycles to save almost nothing. The right
answer differs because the size differs, and both headers say so rather than
leaving a reader to guess which is the house style.

## Integration capture cases

None yet, and they are blocked on a consumer: the sequencers this block feeds
(`FIELD.SEQ.EARTH` and the other four) are all unbuilt.

The first meaningful capture is a frame in which a spell's program is acquired
once and re-used by several sequencers, with the hit count proving the sharing
actually happened. That is what the cache exists for and it cannot be
demonstrated until something executes a program.

## Notes

Planning split (charter §6A): sequencers may merge/share ALUs post-synthesis; superseded_by will record it.

## NOT STARTED 2026-08-19, and the half of it that has no law

The TERRAIN.VELOCITY increment reached this block and did not start it.
Recorded here rather than only in a run report.

**Half of this block is well specified and large; the other half is
unspecified.** The split matters more than the size.

**The validation half — real law, and a lot of it.** `zfield::decode`
(`reference/src/zfield/zfield_decode.cpp`, 453 lines) is the executed
reference and it resolves. It runs the full V1..V12 rule set of
`spec/form/field-ir.md` 4 on the Dalvik model — reject before any register
write, never trust the bytes:

| rule | what it costs in RTL |
|---|---|
| V1 | magic / version / length — cheap |
| V2 | **two** CRC-32Cs: the body CRC over the image with bytes 24..27 zeroed, and the program hash `CRC32C(code‖tables) + instr_count` (5.4) |
| V3-V4 | profile, flags, and the per-profile instruction ceiling (32/48/48/64/32, global 64) |
| V5 | a walk of the variable-length table section: kinds, knot counts, monotone x |
| V6 | the io-map walk: 16 lanes x 12 bytes, reg/kind/type/name_id/bounds |
| V7 | register < 64 |
| V8 | dst overlaps input or source |
| V9 | **per-opcode imm discipline for all 31 opcodes** — zero where unused, CMP mode <= 5, CURVE/SPLINE/DCURVE imm < table_count, ROT3 imm[1:0] <= 2 |
| V10 | END placement |
| V11 | use-before-def, a dataflow pass with the group semantics of 1.3 |
| V12 | output never defined |

That is a multi-pass byte-stream parser, comparable in size to
`zhao_terrain_bake.sv` and probably larger, and its differential must match
`zfield::decode`'s error ENUM exactly, not merely its accept/reject verdict —
otherwise "safely rejected" is untested.

**The cache half — no law anywhere.** Line size, capacity, associativity,
eviction policy, what a "version check" is beyond the program hash, what
happens to a resident program when its cartridge page is evicted: none of it is
written down. `spec/memory_rules.md` never mentions a program cache.
`spec/form/field-ir.md` mentions "program-cache line sizing" exactly once, as a
CONSEQUENCE of the instruction ceiling, never as a rule. `design/blocks.yml`
gives one sentence and three counters (`progcach_hits`, `progcach_misses`,
`programs_rejected`). Inventing a cache geometry would put a fabrication under
every field program the machine runs, which is precisely what SURFACE.SHEET
and TERRAIN.BAKE refused to do with their own missing laws.

**Two dangling citations to settle before this block is built:**

- `reference_model: zref::ProgCache` **does not resolve.** It appears exactly
  once in the whole tree — in `design/blocks.yml`, naming itself. The executed
  reference for the validation half is `zfield::decode` /
  `zfield::programHashOfBytes`; the cache half has no reference because it has
  no law. Amend the way `zref::TerrainBake` was.
- The purpose line above cites **charter 19.4** for safe rejection. There is no
  19.4 in `ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md`; section 19 is "Command ABI
  and generated interfaces" and has no such subsection. The safe-rejection law
  that DOES exist is `spec/form/field-ir.md` 4's Dalvik model, and the citation
  should be corrected to it.

**Suggested split for a future increment:** build the validator alone, as its
own block, differential against `zfield::decode` over both the committed
`.zprog` fixtures and mutated ones (a fuzz lane that flips one byte and
requires the same error enum). Leave the cache until someone writes its law.
