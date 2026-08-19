# Contract — TEXTURE.AUX (Restricted auxiliary source)

> Ledger: `design/blocks.yml` · owner ZH-060 · phase 6 · maturity SPECIFIED

## Purpose and exclusions

Restricted aux texel source (surface sheets, light/shadow compare, distortion) — deliberately NOT a general second TMU (§26).

**Excluded, structurally rather than by policy** — see the next section: no
mode word, no format decoder, no palette, no mip selector, no wrap function
and **no filter**. The block owns one address generator (`sample_sheet`'s
world→texel mapping) and the handshake that turns it into a sheet read. It
returns bytes and never interprets them.

## What §26 costs this file

Charter §15 names four uses for the restricted auxiliary source — "terrain
surface sheet; light/mask map; shadow compare; distortion map" — and ends "It
must not become a second unrestricted full TMU." §26 puts "a second
unrestricted TMU" in the REFUSED list, puts **auxiliary filtering at cut-order
2**, and puts **terrain surface sheets on the NEVER CUT list**.

`design/contracts/TEXTURE.TMU.md` already read that refusal correctly for the
primary side: "every sampling mode the machine will ever have has to fit
through this request channel, because there is nowhere else for one to live",
and it made nearest a *special case* of bilinear rather than a second path.
This block is the mirror image of that reasoning:

> **This source returns bytes and never interprets them.**

All four §15 uses are the same operation — "read a byte pair out of a resident
64×64 page at a world position":

| §15 use | how this block serves it |
|---|---|
| terrain surface sheet | layer F's `{tag, strength}`, exactly (terrain_rules §6.5) |
| light/mask map | the strength byte **is** the mask |
| shadow compare | the COMPARE is `RASTER.FRAGMENT`'s — it already owns a threshold test (its contract's "the alpha test is an INDEX test"). A comparator here would be the first component of a second sampler. |
| distortion map | the offset arithmetic belongs to whoever perturbs a coordinate, not to the fetch |

The consequence for §26's cut order is not rhetoric: **there is no filtering
hardware here to cut**, so the block is already at its cut-order-2 floor, and
what remains is the surface-sheet sample, which §26 says must never be cut.
Both halves are met without one conditional.

*Rejected:* a TMU-shaped mode word with a format field, a filter bit and
per-axis wrap. It is the natural design, it would have made this block look
like its sibling, and it is precisely what §26 forbids — every bit of such a
word is a bit somebody later fills in, and the machine acquires its second
unrestricted TMU one field at a time.

## Law FOUND versus law CHOSEN

**The oracle did not exist.** `design/blocks.yml` names
`reference_model: zref::AuxSource`. Checked 2026-08-19: the string `aux`
appeared nowhere under `reference/` — no header, no `.cpp`, no struct, no
comment — and neither did the two test files the ledger points at. The ledger
promised an oracle nobody had written; `reference/include/zref/zref_aux.hpp`
is it, authored in this increment, and its first test lane proves it a
faithful VIEW onto the executed law before anything else runs.

**FOUND — `zref::render::sample_sheet` (reference/src/zrender/terrain.cpp),
the mapping the software console executes every frame:**

* `i = ((s64)w - e0) * 64 / (e1 - e0)`, then an independent per-axis clamp to
  `[0,63]`;
* the `*64` happens BEFORE the divide, in s64 (F2) — dividing first would
  quantise to whole envelopes;
* the C++ `/` TRUNCATES TOWARD ZERO, so a position left of the envelope has a
  quotient of 0 or negative and the clamp lands on 0 either way;
* a degenerate envelope (`ex1 <= ex0 || ez1 <= ez0`) returns 0 **before any
  arithmetic and before any sheet access** (F4);
* the mapping is a FLOOR ACROSS THE ENVELOPE and deliberately NOT the
  texel-CENTRE rule `stamp_surface` uses on the write side
  (`wx = ex0 + (ex1-ex0)*(2i+1)/128`). `design/contracts/SURFACE.STAMP.md`
  names that asymmetry as its own tripwire: "a u/v transposition or a dropped
  64× survives both standalone suites and dies there" (F1).

**FOUND but stated only in prose:** charter §15's four uses and its refusal;
§26's cut order and never-cut list; and spec/terrain_rules.md §6.5's budget —
"ONE aux consumer on terrain fragments, because tint moved to vertices",
which is what makes this block's measured rate survivable.

**CHOSEN**, each with the alternative that was rejected. The RTL header and
`zref_aux.hpp` carry the same list with the same numbering.

* **A1 — the texel source is SURFACE.SHEET's read port, not TEXTURE.CACHE.**
  *Rejected:* arbitrating `TEXTURE.CACHE`'s `acc_*` port. That block is
  phase-5 frozen with exactly one access port, no MSHR and no hit-under-miss
  ("the master IS the queue, and it is one deep"), and its contract states
  that withdrawing an offered access and substituting a different one before
  acceptance "is outside what this accounting models" — so an arbiter would
  have to be transaction-atomic and would park every aux sample behind up to
  four line fills, and the hit/miss counters would lie if it were not. The
  cache also returns halfwords with no tag/strength split. The sheet port
  returns exactly the two bytes, needs no arbiter, and
  `zhao_surface_sheet.sv`'s own header already sized its slots for a second
  consumer — "2 = one being stamped while one is being sampled, which is the
  smallest set that does not serialise the two consumers". **This block is
  that second consumer; the sheet was built expecting it and never named it.**
* **A2 — a non-resident sheet reads as ZERO and raises `smp_miss_o`.**
  SURFACE.SHEET's own found law is that "a sheet which has never been stamped
  reads as ZERO everywhere", so an absent sheet reading zero paints the same
  picture as an unstamped one — no scar, which is the truthful answer.
  *Rejected:* stalling the fragment until the handle becomes resident.
  SURFACE.SHEET NEVER EVICTS (its chosen law C2), so a non-resident handle may
  never become resident and the stall would be permanent.
* **A3 — the envelope rides the request.** *Rejected:* an envelope register
  file addressed by patch handle. It is the right shape once a
  patch-descriptor cache exists; today it would be a cache with one client and
  no filler, and it needs a write port and an invalidate the ledger does not
  give this block.
* **A4 — one request in flight, no pipeline** — the shape `zhao_texture_tmu`
  chose, for the same reason: the sheet read cannot start until the divide
  answers, and there is nothing to overlap it with. The cost is stated in
  Target throughput below with the measurement, not hidden.
* **A5 — both layer-F bytes are returned**, not strength alone.
  terrain_rules §6.5 asks for "tag/strength effects" (plural), charter §12
  spends both bytes, and SURFACE.SHEET's read port already hands back both.
  `sample_sheet` returns strength alone only because its single caller
  (terrain.cpp's `sheet_factor`) needs no more. *Rejected:* matching
  `sample_sheet`'s signature exactly — it would leave the tag byte unreachable
  by any hardware path, so layer F would carry a byte nothing in the machine
  could read.

**LEDGER DISCREPANCY, recorded rather than silently fixed.** The ledger entry
lists `upstream: [RASTER.FRAGMENT]` and `downstream: []`, i.e. it models this
block as having no memory side at all — unlike `TEXTURE.TMU`, whose entry
names `TEXTURE.CACHE`. A block that reads a resident page must master
something, and A1 chose `SURFACE.SHEET`. The ledger's edge list is therefore
incomplete for this block; `blocks.yml` was left untouched in this increment
rather than edited alongside a block that is still SPECIFIED.

## Clock and reset semantics

Single clock `clk` (`clock_domain: gpu`). Asynchronous active-low reset
`rst_n`, synchronously released; every register including the counter is
initialised in the reset arm. One clock domain, no gating, no CDC.

## Input and output packet layouts

### `aux_requests` (in, ready/valid)

| field | width | meaning |
|---|---:|---|
| `req_wx_i`, `req_wz_i` | 32 (signed) | world position, fx16 metres |
| `req_env_x0_i`, `req_env_z0_i`, `req_env_x1_i`, `req_env_z1_i` | 32 (signed) | the patch envelope, fx16 (A3) |
| `req_handle_i` | 32 | handle32 — the sheet to read |
| `req_src_id_i` | 16 | `source_ids: true` |

There is no mode word. That is the design (see §26 above), not an omission.

### the SURFACE.SHEET read master (out + in, ready/valid on each)

`shr_valid_o`/`shr_ready_i` with `{shr_op_o = OpRead, shr_handle_o,
shr_texel_o[11:0] = j*64+i, shr_src_id_o}`; `shp_valid_i`/`shp_ready_o` with
`{shp_status_i, shp_tag_i, shp_strength_i}`. The two channels are serialised
by the FSM, so no tag and no reordering are needed. Hygiene: `shr_valid_o`,
`shp_ready_o`, `req_ready_o` and `smp_valid_o` are functions of registers
only, never of the incoming ready.

### `aux_samples` (out, ready/valid)

| field | width | meaning |
|---|---:|---|
| `smp_tag_o`, `smp_strength_o` | 8 each | layer F (A5) |
| `smp_u_o`, `smp_v_o` | 6 each | the texel the mapping chose |
| `smp_degenerate_o` | 1 | the envelope was degenerate (F4) |
| `smp_miss_o` | 1 | the sheet was not resident (A2) |
| `smp_src_id_o` | 16 | echoes `req_src_id_i` |

## Backpressure rules

Ready/valid on all four channels. One request in flight (A4): `req_ready_o` is
`st_r == ST_IDLE`, so a request is never accepted while another is in the
machine and nothing can be dropped or reordered. A stalled consumer parks the
block in `ST_OUT` holding the sample; a stalled sheet port parks it in
`ST_REQ` holding the offer stable, which is what ready/valid requires and what
`TEXTURE.CACHE`'s and `SURFACE.SHEET`'s accounting assume.
ENFORCED-BY: tests/texture/texture_aux_directed.cpp:test_latency_bound
(three stall patterns on the consumer × three on the sheet port; the stream is
bit-identical under each).

## Memory ownership

**None owned.** The block holds no page, no cache line and no table: about 200
flip-flops of FSM, divider state and the accepted request, plus a 32-bit
counter. Layer F is owned by `SURFACE.SHEET` (`spec/terrain_rules.md` §7: "F
written only by SURFACE.STAMP"), and this block is a READER of it through the
sheet's own port — it never writes and has no write port.

## Q formats and rounding

* `wx`/`wz` and the four envelope words are fx16 (Q16.16, spec/qformats.md §2).
* The numerator lane is 40 bits: `N = (w - e0) * 64` with `|w - e0| ≤ 2³²−1`
  gives `|N| < 2³⁸`, so it cannot wrap for any pair of s32 words. The divisor
  lane is exactly 32 bits unsigned, because `1 ≤ e1 − e0 ≤ 2³²−1` after the
  degenerate test. The remainder lane is 39 bits, which holds both `D << 5`
  and every pre-step remainder.
* **There is no rounding.** The mapping is a truncating division whose result
  is clamped; spec/qformats.md §3's single-rounding law is satisfied
  vacuously. Adding a half-texel bias here would move every scar the player
  can see.

## Latency (fixed or variable)

`variable_bounded:8`, and the bound is **measured, not asserted**. The FSM is
`ST_IDLE → ST_DIV0 → ST_DIV1 → ST_REQ → ST_RSP → ST_OUT`; against a sheet that
answers in one cycle the worst accept-to-retire is **5 clocks**, measured over
a 256-packet stream. A degenerate envelope skips `ST_REQ`/`ST_RSP` entirely
(F4) and retires in 3. Sheet backpressure stretches `ST_REQ`/`ST_RSP`, so the
ledger's bound of 8 is a statement about the pair; the measurement is printed
by the directed lane on every run.
ENFORCED-BY: tests/texture/texture_aux_directed.cpp:test_latency_bound
(`RTL: accept-to-retire is within the ledger's bound of 8`).

## Target throughput

**MISSED, and here is the measurement.** The ledger asks for "1 aux sample per
clock". This shape sustains **one sample per six clocks**: `req_ready_o` is
`st_r == ST_IDLE` and nothing overlaps, so a 256-packet stream against a
one-cycle sheet occupies **exactly 1,536 clocks — 6.00 clocks per sample**,
measured from the first accept to the last retire and asserted, not derived
from the state count.
ENFORCED-BY: tests/texture/texture_aux_directed.cpp:test_latency_bound
(`the sustained rate is one sample per SIX clocks (256 in 1,536)`).

That is the same honest shortfall
`design/contracts/TEXTURE.TMU.md` records for the primary TMU ("the sustained
rate is ONE SAMPLE PER FOUR CLOCKS (direct) or PER SIX (CLUT), not the
ledger's 1 sample per clock").

What makes it survivable is a ratified budget rather than optimism:
spec/terrain_rules.md §6.5 states "ONE aux consumer on terrain fragments,
because tint moved to vertices" — layer H moved to the Gouraud path precisely
so the aux lane would carry one sample per terrain fragment and no more.

Reaching one per clock is a change to the RTL file ONLY — the ports, the
oracle and this contract's law sections are unaffected. It needs the two
dividers pipelined across their six steps and a fetch stage that can hold a
second request; the sheet's read port already answers one per clock.

## Overflow and malformed-input behaviour

**Every bit pattern on every port is a legal packet**; there is no mode word to
misencode and therefore no `mode_error_o` analogue.

* A DEGENERATE OR INVERTED ENVELOPE (`e1 <= e0` on either axis) answers
  `{tag 0, strength 0}` at texel (0,0) with `smp_degenerate_o` set, and issues
  NO SHEET READ AT ALL — so a malformed envelope cannot consume a sheet-port
  cycle. That second half is checked directly rather than inferred from the
  differential.
* A NON-RESIDENT HANDLE answers `{0,0}` with `smp_miss_o` and the texel the
  mapping chose (A2). It is never the previous sample's bytes.
* Positions outside the envelope clamp per axis (F3); `INT32_MIN` and
  `INT32_MAX` on either axis, and the widest representable envelope
  (`INT32_MIN..INT32_MAX`, divisor 2³²−1), are all in the directed and random
  lanes.
* Nothing overflows: the width note under Q formats bounds every lane for
  ANY input word, not merely for legal ones.

## Counters and traces

`texture_samples_o` (32-bit, saturating at `0xFFFF_FFFF`, cleared only by
reset) counts **retired** samples — `smp_valid_o && smp_ready_i` — so a
stalled consumer never double-counts one. `source_ids: true` is honoured on
both the outgoing sheet read (`shr_src_id_o`) and the returned sample
(`smp_src_id_o`), so a trace can follow one fragment through the pair.

The counter name is shared with `TEXTURE.TMU`'s (`counters:
[texture_samples]` in both ledger entries); they count different events on
different blocks and the catalog will need to disambiguate them when both are
wired to `DEBUG.COUNTERS`. Recorded here rather than discovered there.

## Scalar reference function

`zref::AuxSource` (`reference/include/zref/zref_aux.hpp`), authored
2026-08-19 because the ledger's named model did not exist. It is a VIEW onto
`zref::render::sample_sheet`, not a second law, and
`tests/texture/texture_aux_directed.cpp`'s FIRST lane proves the view faithful
before anything else runs — driving a real `zref::render::SurfaceSheet`
through `sample_sheet` and through `zref::aux::AuxSource::sample` over 525
world positions across five envelopes (inside, outside on each side, and
degenerate) and asserting they agree on the strength byte every time. If that
lane is red, nothing after it means anything.

## Directed tests

`tests/texture/texture_aux_directed.cpp`, seven lanes:

1. **the view is faithful** — as above; 0 mismatches over 256 inside, 164
   outside and 105 degenerate samples, and the sweep asserts it reached all
   three classes.
2. **axis anchors** — hand-computed `axis_texel` at the first texel, the last
   texel, both clamp rails, a 128-unit envelope where the FLOOR is visible
   (1→0, 2→1, 3→1), a negative-origin envelope, and both degenerate forms.
3. **RTL at CONSTRUCTED boundaries** — for an envelope whose span is
   deliberately NOT a multiple of 64, both sides of every one of the 63 texel
   boundaries (126 packets, `w = e0 + ceil(k·D/64)` and one less), plus a
   6×6 grid of domain rails including `INT32_MIN`/`INT32_MAX`, plus the widest
   representable envelope.
4. **the degenerate envelope** — four malformed forms answer `{0,0}` with the
   flag, and `sheet_reads() == 0`: F4's second half, which a differential
   alone cannot see.
5. **the miss** — a resident read followed by the SAME texel on a
   non-resident sheet: zero bytes, `smp_miss_o`, and the texel still reported.
6. **handshake, latency, throughput, counter** — as cited above.
7. **composition with the REAL SURFACE.SHEET** — `zhao_texture_aux` masters
   `zhao_surface_sheet`; the test acquires a sheet, writes all 4,096 texels
   through the sheet's own write port, then reads 65 probes back through the
   block's master port and checks them texel-exact, and finally checks that a
   handle the sheet never allocated comes back as a zero miss. This is the
   only place the port semantics are pinned; the C++ `SheetModel` the random
   lanes use is a convenience, not the law.

## Randomized differential tests

`tests/texture/texture_aux_random.cpp`, two lanes plus a miss sweep:

* **lane G (gameplay-shaped)** — 12k packets (120k nightly): 4 m patch
  envelopes at plausible world origins, positions across the envelope plus a
  one-tenth guard band on each side, a sparse scar pattern (one texel in five
  carries a scar), consumer stalls. One fragment in sixteen is SNAPPED to an
  exact texel boundary per axis — not a cheat: a rasterizer steps u in fixed
  increments and cell edges are ordinary traffic, and the exact boundary is
  the only input that distinguishes this mapping's FLOOR from a round.
* **lane L (domain limit)** — 12k packets (120k nightly) uniform over the
  whole int32 domain with one envelope in sixteen inverted, PLUS 4,032
  CONSTRUCTED boundary packets (32 random envelopes with spans from 64 to
  ~2²⁴ raw units × both sides of all 63 boundaries), PLUS a 6×6×6 grid of
  pathological envelopes (fully degenerate, one-axis degenerate, inverted,
  one raw unit wide, the widest representable, straddling zero) against six
  extreme positions.
* **the miss sweep** — 4,096 of lane L's packets re-run against a
  non-resident sheet: every non-degenerate sample must be a zero miss with the
  mapping still correct.

All three COUNT their interesting states and **assert the counts**: both clamp
rails on each axis, the interior, a negative numerator, a saturating
numerator, an exact texel boundary, and non-zero layer-F bytes. Measured on
the committed seeds — lane G: rails 753/840 and 1201/1081, interior 8,431,
neg 656, sat 670, exact boundaries 3,976. Lane L: rails 4088/4142 and
4199/4172, interior 5,213, neg 3,969, sat 4,039, exact boundaries 2,028,
degenerate 108.

## Formal properties

**None, and that is the honest answer for this block.** The only arithmetic
worth a theorem is the six-step restoring divide, and its correctness rests on
a precondition — `N < 64·D`, established by the saturation compare in a
different cycle and carried in a register — so a property over the divider
alone would be conditional on a fact the harness would have to assume, and
proving that fact means proving the FSM. That is a bounded model-check of a
sequential block, not the total scope-complete proof
`zhao_surface_blend` and `zhao_texture_mod255` carry, and it would state less
than the constructed-boundary lanes already do (which drive every one of the
63 boundaries on both sides against the executed reference).

If a property is added later, the shape it wants is the divider factored into
its own combinational module with `N` and `D` free and the precondition
`N < 64·D` ASSUMED, asserting `q == N / D` via a witness decomposition — the
same trick `tests/formal/texture_mod255.sby` uses to state a division without
putting a divider in the property. Recorded rather than left as a gap.

## Synthesis / resource ceiling

Written to the conservative synthesizable subset (charter §2): no indexing of
a function call's result (the `div_sub`/`div_bit` helpers return whole values
that are assigned, never sliced at the call site), no dynamic part-selects, no
unbounded loops. Verilator `-Wall` clean (`lint_texture_aux`).

Expected cost, by inspection — **this block has NOT been through the Quartus
per-block sweep at the time of writing, so nothing here is a synthesis
claim**: two 40-bit subtractors, two 38-bit comparators for the saturation
rail, twelve 39-bit compare-subtract steps (six per axis, three per state so
six live at once), a 6-state FSM, ~200 flops and a 32-bit saturating counter.
No multiplier, no M10K, no ROM. The `<< 6` and the per-step `<< k` are all
constant shifts, i.e. wiring.

## Integration capture cases

`zref::render::sample_sheet` is executed by the software console every frame
and pinned by the committed render goldens (the crack ring of
`tests/render/render_golden.cpp` reaches the screen through it). Those goldens
pin the LAW; this block is differentiated against a view of that law which
lane 1 proves faithful, so a drift here turns the differential lanes red
before it could reach a capture.

**Not yet composed with `RASTER.FRAGMENT`.** The seam that exists today is the
one lane 7 exercises (this block ↔ `SURFACE.SHEET`, both real RTL). Wiring the
request side to a real fragment stream needs the fragment block's aux port,
which it does not yet have. Recorded as missing rather than implied.

## Mutation evidence

Three mutations, one per build, each proved to have RELINKED by the SHA-256 of
the test binaries (a hash that did not change means the mutation did not run).
Clean: directed `4f242cbc…`, random `b27a4530…`.

| # | mutation | directed / random SHA-256 | caught by |
|---|---|---|---|
| M4 | `qu_lo[0] = div_bit(ru_e, du_r, 0)` → `1'b0` (the divider's last step) | `91046953…` / `7800ac56…` | directed lane 3 at its FIRST constructed pair, by name (`the u boundary steps exactly where the FLOOR says it does`); both random lanes |
| M5 | `st_r <= degen_r ? ST_OUT : ST_REQ` → `st_r <= ST_REQ` (a degenerate envelope reads the sheet anyway) | `e2ccd192…` / `bd9c56d6…` | directed lane 4 by name, INCLUDING `a degenerate envelope issues NO sheet read at all` — the check no differential can make; the miss sweep |
| M6 | `shr_texel_o = {tex_v_c, tex_u_c}` → `{tex_u_c, tex_v_c}` (the u/v transposition SURFACE.STAMP.md warns about) | `2c9aebcc…` / `046ae824…` | directed lane 3, lane 7 (the REAL-sheet chain, probe 64) and both random lanes |

M4 is the one uniformly random traffic could not be relied on to catch — it is
wrong only at the exact positions where the quotient steps, and those are
CONSTRUCTED in lane 3 and in lane L rather than hoped for. M5 is the one no
differential can catch at all, because the ANSWER is still zeros for some
inputs; it dies on the sheet-read count.

## Notes

Cut-order 2 (auxiliary filtering, §26).
