# FINDINGS — BINARENA (entry I55, the independent producer)

Branch `gz/binarena`. Base `e04d9feb` on `claude/ceiling-architecture-20260912`.

**Register, measured BARE at the last pushed commit: 5** — unchanged from the 5
this packet opened with (`I13`, `I34`, `I55`, `I56`, plus the one disconnected
implementation `zhao_terrain_normalmap`). **`I55` did not close.** What closed
is the blocker WALKSWAP said would have to close first.

---

## 1. WHAT WAS BUILT

`fpga/rtl/geometry/zhao_geom_arenabin.sv` — **GEOM.ARENABIN**, the independent
chunk producer. It bins the post-clip triangle stream against the **arena's own
TriangleDescriptor indices** and writes 64-byte chunk records straight into
`zhao_geom_paramarena`. It reads no binner RAM.

This is the exact shape WALKSWAP's refusal named as the only thing that breaks
the series it measured — *"a producer that fills the external arena WITHOUT the
on-chip one — tile binning performed against arena triangle ids directly"* —
and the shape owner directive §4 requires.

### Where the transpose lives, which is the whole architecture

Binning is a transpose: triangles arrive in submission order, a tile list is
wanted in tile-major order, and something must buffer the difference. The only
real choice is **where**, and directive §4 states the preference outright
(*"may move backing state to SDRAM rather than growing a frame-sized FPGA
register file"*).

| | on-chip state |
|---|---|
| `zhao_geom_binner_v2` (whole-frame reference array) | `ref_ram` 229,376 + `next_ram` 106,496 + `tile_ram` 24,192 = **360,064 bits** |
| `zhao_geom_arenabin` (one partial chunk per tile) | staging 145,152 + directory 32,832 = **177,984 bits** |

Every **full** chunk leaves for SDRAM the moment its fourteenth id lands. The
bound is **per-tile and constant**: the guaranteed giant covering every tile
costs exactly what a single triangle costs. Both figures are counted from the
declarations; **neither has been through `quartus_map`** (R212).

The block's **reference capacity is the arena's sealed chunk quota**. There is
no second local `REF_CAP` to overflow — one bound, in the block that owns the
memory.

### The chain patch, and why it was unavoidable

`zhao_geom_chunkser`'s own header states its assumption precisely: it emits
`next = ck_alloc_id_i + 1` *"because this block is the arena's only chunk
producer AND OFFERS A TILE'S CHUNKS IN ORDER."* True of a tile-major
serialiser; **false of a submission-order binner**, where hundreds of other
tiles' chunks may sit between one of a tile's chunks and the next.

Two alternatives were rejected with reasons, not preferences:

* **hold a tile's chunks on chip until its successor is known** — that is the
  frame-sized array directive §4 says to move to SDRAM, wearing a new name;
* **build the chain backward** — correct as a data structure, wrong as a
  rendering order. `zhao_geom_binner_v2` keeps a *tail* pointer precisely so a
  tile list is FIFO, because the painter's algorithm needs frame-wide
  submission order within a tile. A backward chain delivers a tile
  newest-first and the picture is wrong in a way no counter reads.

So `zhao_geom_paramarena` gained an `lk_*` intake that rewrites bytes 0..7 of
an **already written** chunk. It is **not a second writer**: same write engine,
same `m_addr_q`, charged to the same `wr_words_q`, so a patch cannot be in
flight at publish. **The generation is still stamped by the arena** from
`gen_q` — that law is preserved, not excepted. The patch stops at byte 8 on
purpose; byte 8 is the chunk's first triangle id. Declared cost: one 8-byte
write per non-terminal chunk, at most **+12.5%** on chunk-region write traffic.

### Identity (directive §4) — there is nothing to evict

* **What is stored IS the identity** — the arena's own `td_id_o`, carried
  verbatim at full 18-bit width. No hash, no CRC, no local renumbering, **no
  second namespace**. A binner slot 0..127 cannot reach this block because this
  block has no slots.
* **There is no cache, so there is no eviction.** `stage_ram` is indexed by
  **tile**, not by identity: it is an accumulator, not an associative store.
  Nothing is looked up, so nothing can miss, so nothing can be evicted and
  reallocated under a live reference. That is the directive's *"prove that
  eviction/reuse cannot change a still-referenced identity"* answered
  structurally rather than by argument.
* **Lifetime is ONE FRAME**, bounded by the arena's own `seal_fire_o` and the
  frame end. Checked, not asserted: case 4 covers tile 0 nineteen times in
  frame one and not at all in frame two, and its head is **gone**, not stale.
* **A triangle the arena refused has no index** and is dropped at
  `tris_unnamed_o` rather than given a convenient zero.

---

## 2. THE EVIDENCE — STRUCTURAL, NOT A COUNTER AT ZERO

`geom_arenabin_directed` elaborates the bench with `-GHAVE_ONCHIP=0` and its
source list contains **no `zhao_geom_binner_v2.sv`, no `zhao_geom_chunkser.sv`,
no `zhao_geom_tidq.sv` and no `zhao_geom_arena.sv`**. `tests/CMakeLists.txt`
splits the bench's sources in two and **fails the configure** if one of the
four leaks back into the base list. The chunks that reach SDRAM in that binary
cannot have come from an on-chip reference array, because the circuit contains
none.

That distinction is load-bearing and it is why there are two targets. In the
`HAVE_ONCHIP=0` build the on-chip counters are **generate-tied constants**, so
asserting they read zero would be asserting a constant — the vacuous control
this campaign has caught twice. So the assertion lives in
`geom_arenabin_price`, where the whole legacy path is **compiled in and
clocked** and produced nothing.

**275 checks / 0 failures** (`geom_arenabin_directed`):

* the round trip — 27 references over three tiles, through the real
  `zhao_mem_guard` / `zhao_vram_arbiter` / `zhao_sdram_ctrl` and the DRAM
  model, back out through the real `zhao_geom_paramwalk`, compared against the
  **arena's own descriptor fields**. Four decoy descriptors are allocated and
  never binned, so a producer shipping a local index would decode to the wrong
  descriptor for every triangle **with every range guard still passing** —
  entry I54's named failure, and the reason this test does not read a counter
  for its answer;
* the **chain patch verified in the DRAM bytes**: tile 0's first chunk is not
  terminal, names its successor, carries count 14; the successor terminates
  with the remaining 5; bytes 6..7 carry the **arena's** generation;
* a nameless triangle is dropped, and the tile's list is walked to confirm it
  lost exactly that one;
* `link_illegal_o` **fired**, at a port, with legal stimulus — case 1 asserts
  the same counter is zero on a correct run, which is its negative control. No
  committed mutant is owed;
* two frames, with the directory proven not to outlive its seal.

**470 checks / 0 failures** (`geom_arenabin_price`).

---

## 3. THE PRICE, RE-MEASURED — AND ONE POINT WOULD HAVE LIED

`busy_o` is high for a whole frame, which includes **two sweeps of the
576-entry directory**. At the 27-reference scene that reads **86.41
clocks/ref** — alarming, and not a price: a fixed cost divided by a small
number. Two scene sizes, solved for the line:

```
fixed    = 2,176 clocks/frame      (the two directory sweeps)
marginal =     5.82 clocks/ref
at R7's 32,768-reference giant  ->  5.88 clocks/ref
```

**The 7.3x is the CONSUMER side and this packet did not move it.**
`price_the_swap` re-run at this commit reports **4.12 clocks/ref** (on-chip
drain) and **29.89 clocks/tri** (external walk) — byte-identical to WALKSWAP's
numbers. So the producer at 5.88 clocks/ref sits beside the on-chip drain's
4.12; **the producer is not the expensive half.**

**The new cost is backpressure, and it is measured rather than hidden.** Unlike
`zhao_geom_chunkser`, which reads finished lists and is explicitly outside the
raster path, this block bins the **live** stream and holds `tri_ready_o` low
while writing a chunk. `intake_stall_o`: **8,093 clocks over a 1,600-reference
frame**, about 5 clocks per reference of backpressure on the geometry front
end. A chunk FIFO would hide that behind a depth and was deliberately not
added — the first thing this entry needs is the measurement.

---

## 4. WHAT I REFUSED

**(a) Composing the producer in the console.** Instantiating it retires
`u_geom_chunkser` as the arena's producer, which leaves the serialise pass in
`zhao_geom_binner_v2` — `ser_req_i` and seven `ser_*` outputs, exported through
`zhao_shell_top_v2` as `render_ser_*` — with **no consumer**, turning a live
path into dangling ports and making entry I54's text false. That is a subsystem
retirement on the block the fit budget is tightest on, and this packet is
forbidden a console fit, so doing it here would be an unmeasured claim about
the tightest budget in the design.

**(b) The raster swap. Blocker 2 is untouched and is still 1,749 bits.** This
changes who **fills** the external arena, not who **reads** it. `job_*` still
needs `METAW = 1877` against the 16-byte descriptor's 128, so the swap still
owes a second GEOM.SETUP and GEOM.ATTRPACK back end fed from SDRAM plus the
vertex-fetch arm. Nothing here reduces that.

**(c) Moving `paramwalk dirs/chunks/tris` off zero — and this one was a
choice, not an oversight.** Wiring `walk_valid_i` to a tile sequencer over the
head table would move `chunks_walked_o` and `tris_emitted_o` off zero tomorrow;
`t_ready_i` is already `1'b1`. It would also be **a producer driving into
nothing** — every `t_*` output dangles, so the triangles would be counted and
dropped. That is logic added to make a counter move, which the campaign's first
rule refuses. With a real consumer on `t_*` it is no longer a demonstration —
it **is** the raster swap, and (b) is its price.

The smoke therefore still reports `paramwalk dirs=0 chunks=0 tris=0` beside
`raster pixels=2560`, and the deliverable that asked for that triple to move is
**not met, deliberately**.

---

## 5. FALSE CLAIMS FOUND

**(i) The brief's "on a device at 317% of its ALUT budget" is measured on the
WRONG PART, and the shipping-part number is worse.** `317%` comes from
`reports/synthesis/console_entity_attrib_sizing_post.md:14`, whose own line 3
says `DEVICE: 5CEBA9F31C7` — the **sizing** part. The shipping-part equivalent
is in `console_entity_attrib_shipping.md:14`: **301,446 combinational ALUTs,
360% of ~83,820**, `DEVICE: 5CSEBA6U23I7`. The sizing file states at its own
lines 8-10 that these are *"synthesis estimates, not a placement result...
nothing here should be quoted as either."*

**(ii) The "~97% of its ALUT budget" figure in circulation is not a console fit
at all.** It is a 2026-09-13 **subtotal of per-block estimates** —
`reports/RESOURCE-RESCUE-ROADMAP-CURRENT-20260913.md:451` "40,591 ALM against a
41,910 DEVICE" — and the same document at `:348` records that "floor" was
**withdrawn** as a description of it, with whole domains contributing zero
(Texture 0/7,000, complete FIELD 0/4,500, 34 unpriced blocks). Two numbers in
circulation, 97% and 317%; **neither is a measured console ALM figure on the
shipping part, and no such figure exists in the tree.** The only measured
console ALM row is 47,582 on `5CEBA9F31C7` from a 2026-09-19 tree of 106
sources against today's 286.

**(iii) An instrument defect in `zhao_block_fit.json`, and it lies in the
ALARMING direction — the rarer polarity.** Every `zhao_geom_binner_v2` row
including `@giantrefs-32k` carries `measuredDevice: 5CSEBA6U23I7` **and**
`sizingDevice: 5CSEBA6U23I7` — the same, shipping, part — and then
`notTargetDevice: true` beside a `sizingNote` reading *"fitted on
**5CSEBA6U23I7** ONLY to measure size; the target is **5CSEBA6U23I7** at 41,910
ALM and almsAvailable below is NOT that device."* The note names the same
device twice and contradicts itself, there is no `almsAvailable` key on the row
at all, and the flag is **false**: these rows *are* on the target device. A
gate reading `notTargetDevice` would discard five genuine shipping-part
measurements. Verified by reading the JSON at line 1096 directly.

**(iv) `price_the_swap`'s own header contradicts its labels, and entry I55's
"same unit on both sides" is doing more work than it can carry.** The function's
header says *"This prints clocks per triangle for each"*; it prints
`clocks/ref` for the drain and `clocks/tri` for the walk. The **unit** is in
fact the same (a single-tile walk has exactly one reference per emitted
triangle), so the 7.3x survives — but the two sides differ in **population**
(the drain averages 27 references over three tiles, the walk covers tile 0's
19) and in **divisor convention** (the drain is a span divided by `refs-1`, the
walk an accumulated total divided by `tris`). The `refs-1` convention inflates
the drain figure by ~3.8% at this scene size, i.e. **in the direction that
makes the refusal look stronger**. The conclusion is unaffected; the sentence
"Same unit on both sides" should say "same unit, different populations and
different divisor conventions."

**(v) Two claims in the brief I checked and found TRUE**, recorded because a
verified claim is as useful as a refuted one: `@giantrefs-shipped` and
`@giantrefs-32k` are **both** `rtlCleanAtHead: true`, and
`@refpush-giantrefs32k` is `rtlCleanAtHead: false` — exactly as the brief said.
And entry I55's claim that there is **no shipping-part figure for
`zhao_geom_bin_pipe_v2` anywhere in this tree** is **verified in full**: that
module has zero rows in `zhao_block_fit.json` and zero in `zhao_shell_fit.json`.

---

## 6. WHAT I GOT WRONG AND CAUGHT MYSELF

**(A) My own on-chip state figure was 9,216 bits light, in the flattering
direction.** The block's header, the inventory entry and the manifest row all
said **~169 Kbit**. The real total is **177,984 bits**: I added `nch_ram` (the
per-tile chunk counter that makes `max_tile_chunks_o` measure a tile's list
depth rather than a frame total) about an hour after writing the figure, and
never went back. Caught by recomputing the declarations rather than re-reading
the prose. Corrected in all three places, **with the correction recorded in the
header** rather than silently overwritten, because it went wrong in the
direction this repo's own law says nobody audits. The first commit message on
this branch still carries the old number and cannot be amended; this is the
correction.

**(B) A case-4 failure that read exactly like an RTL bug was my test.** *"frame
two binned exactly one reference: expected 28, got 27"* — which looks like the
producer failing to open a second frame, and I spent real time reasoning about
frame edges before measuring. The publish-wait loop compared the **cumulative**
`frames_published_o` against `>= 1`, so it broke instantly on every frame after
the first and the assertions read counters before the work had happened. The
RTL was correct throughout. Fixed to wait for *this* frame, with the reason
written beside it. **Measuring took one trace print; reasoning took twenty
minutes and got the wrong answer.**

**(C) `check_entry_claims` caught my prose, as the brief warned it would.** My
entry text opened an item with "THE PRODUCER IS NOT COMPOSED", and the nearest
module name to that phrase was `zhao_geom_binner_v2`, which **is** composed. The
tool read the claim as being about the binner. **Reworded, not baselined** — the
sentence now names `zhao_geom_arenabin` as its subject and says in-line why it
is phrased that way round.

**(D) Two defects in my own RTL, found by reading rather than by a gate**, both
recorded at the site:

* the frame-edge block sat **above** the `case` statement. Both write the same
  non-blocking targets and the later assignment wins, so a seal coinciding with
  any state transition would have been silently overwritten by that transition
  and the frame would have started in the wrong state with its directory
  uncleared. Moved below, with `zhao_geom_paramarena`'s own lost-retirement
  incident cited beside it — the identical trap, already written down in that
  file, which is why it was recognisable;
* `intake_open_c` lacked `!done_pend_q`, so a triangle accepted in the cycle
  `A_IDLE` hands over to the flush would have been **silently lost** — and
  invisible, because a reference that was never made cannot be missed by any
  counter.

**(E) A process error: I edited `zhao_console_core.sv` while the four smoke
controls were verilating it.** That makes their answer worthless in both
directions — CLAUDE.md's own rule, broken by me the same session I read it. The
edit was a comment, but a run whose inputs moved underneath it is not evidence,
so the controls were re-run at a frozen tree and only the re-run is quoted.

---

## 7. GATES AT THE PUSHED COMMIT

All run bare, exit codes read directly:

| gate | result |
|---|---|
| `completion_register.py` | **5** (RC 1, normal while gaps remain) — unchanged |
| `check_console_inventory.py` | OK |
| `check_prod_manifest.py` | OK |
| `gen_prod_top.py --check` | fresh (84 instances) |
| `gen_console_board.py --check` | fresh (1,600 core ports) |
| `gen_shell_paired_diff.py --check` | fresh |
| `check_case_labels.py` | OK |
| `check_quartus17_syntax.py` | RC 0 |
| `check_console_closure_lint.py` | 0 findings (**7 PINMISSING before the arena's new pins were connected — gate 31 did exactly its job**) |
| `check_entry_claims.py` | RC 0, nothing baselined |
| `mutant_copy_drift.py` (**after** commit) | OK, no new drift |
| console smoke | PASS, `raster pixels=2560`, `frames_admitted=1` |
| `-Mutant` | RC 0 -- *"MUTANT PASS -- terr_pl_slot_overflow_o fired 1 time(s). The detector works; production's zero is a measurement."* |
| `-BadVertex` | RC 0 -- *"BAD_VERTEX PASS -- one refused record dropped its batch (holes=1, groups_poisoned=2, replay_poisoned=8) and the frame completed"* |
| `-NoEchoArm` | RC 0, SMOKE PASS |
| `-BadTraceArm` | RC 0, SMOKE PASS |
| `geom_chunkser_directed` | 481 checks, 0 failures |
| `geom_paramarena_directed` | 345 checks, 0 failures |
| `geom_arenabin_directed` | 275 checks, 0 failures |
| `geom_arenabin_price` | 470 checks, 0 failures |
| arena mutant pairs (drain/align, fires+silent) | all 4 pass |

`verilator --lint-only -Wall`: 0 diagnostics on `zhao_geom_arenabin` and on
`zhao_geom_paramarena`. **Lint-clean is not synthesizable-clean and neither
block has been through `quartus_map` at this commit** — that is stated rather
than implied, per R212 and CLAUDE.md's own note that two SystemVerilog forms
lint clean and fail Quartus 17.

**No Quartus was run by this packet**, per the fence.

---

## 8. FOR WHOEVER TAKES THIS NEXT

The producer exists, is tested, and is not composed. The next act is a **single
coherent one** and should not be split:

1. instantiate `u_geom_arenabin` driving the arena's `ck_*` and `lk_*`;
2. retire `u_geom_chunkser` **and** the binner's serialise pass together —
   `ser_req_i` tied and seven `ser_*` outputs removed from
   `zhao_shell_top_v2`'s boundary, not left dangling;
3. rewrite entry I54 to describe the producer that then exists;
4. one fit, on the geometry subsystem, to price what (1) and (2) cost each
   other — that is the fit gate, and it is the first one this work has earned.

The raster swap (blocker 2) is a **separate and larger** packet and still owes
a second GEOM.SETUP/GEOM.ATTRPACK back end fed from SDRAM. Nothing in this
packet makes it cheaper, and entry I55 now says so with the producer's numbers
beside it instead of only the refusal.
