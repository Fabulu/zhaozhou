# FINDINGS -- pfs2 (particles / FIELD / surface, pass 2)

Branch `gz/pfs2`, from `f70b1d48` ("Rulings R56-R59...", register 33).
Register **33 -> 30**. Five commits, all pushed, all gated.

---

## 1. GAPS CLOSED

| entry | ruling | register |
|---|---|---|
| **I7** -- PART.COLLIDE's plane and the population origin | R41 | 33 -> 32 |
| **I30** -- SURFACE.STAMP's dispatch (envelope + policy) | R45 | 32 -> 31 |
| **I33** -- PART.TABLE's per-frame load | R42 | 31 -> 30 |

Each closed with a REAL producer -> implementation -> consumer -> the value
seen to traverse in the console smoke, and with the core's ports **deleted
rather than driven**: twenty-seven in all (I7's eight, I1's four provisional
`part_seed_*` under R46, I30's nine, I33's six).

### I7 / R41 -- `SetPopulation` 0x0303
Command packet -> CMD.DECODER's verdict -> CMD.EXEC's commit ->
`zhao_part_pop` -> PART.COLLIDE's plane, PART.TERRAIN_TAP's origin, and the
generation store's first-generation seed. The smoke asserts
`part_pop_handle_o` reads back `0x0051C0DE`, which nothing else in the bench
writes, and the particle engine is unchanged by the move (six STICK contacts
against the plane the PACKET named).

### I30 / R45 -- `zhao_surface_dispatch`
The world->patch law `zhao_terrain_heighttap` inverts, applied to the stamp's
own translation on the SAME live pitch wire. No second mapping law.
blend_en = 0 as R45 ratifies, in a named parameter.

### I33 / R42 -- `SPECIES_TABLE` page kind 13 + `zhao_part_table_loader`
The descriptors are DATA the owner authors, published by the PublishResource
the console already executes, read back through a new requester E of
`zhao_geom_mem_adapter`. Nothing in the console reads a descriptor field,
which is what entry I33 refused to do and was right to refuse.

## 2. A DEFECT AND A SILENT DROP

### R54 -- `zhao_part_hps` spun on a bridge refusal
Third instance of the S1 defect fixed in MEM.UPLOAD and DEBUG.FRAMEBLIT at
`525a3f6d`. Repaired: `err` at B_REQ is counted and the request is taken DOWN;
`ERR_RETRY_N` consecutive refusals FAULT the tick; a faulted tick is
TRUNCATED, never hung -- `ps_tick_abort_o` ends PART.STATE's read stream
through a new `tick_abort_i`, and the new generation's length becomes exactly
the records that reached DDR.

**Tested with the REAL bridge doing the refusing**: the bench misaligns the
address on its way into `zhao_hps_bridge`, which is one of the two things the
bridge itself calls malformed, and its own refusal path runs. 132 checks.

### R55 -- the arbiter's pending slot dropped a second request in silence
`pend_dropped_o` + a sticky `pend_dropped_mask_o`. Fired on purpose in
`hps_arbiter_n_directed` case 10, with the negative control beside it (a
HOLDER re-presenting the SAME request must NOT move it). **Per client, may it
change a pending request? NO for all seven**, with the structural reason
written into the arbiter's rule 6c -- six holders whose request fields are
registers inside one state, and CMD.DMA, the one pulser, with one request in
flight by construction. Each holder de-asserts on the GRANT, which the arbiter
pulses one full state before the burst ends, so a held request cannot be
served twice.

## 3. REFUSED, WITH THE BLOCKER

### R40 (I5), R43 (I42), R44 (I34) -- NOT ATTEMPTED, and they are ENTANGLED
This is the finding worth carrying forward, because it is not visible from the
rulings and it changes how the next packet should be scoped.

`zhao_console_core`'s entry **I42** is ONE register entry covering THREE
things: the FIELD program loader (`fld_ld_*`), the directory's two phases
(`fld_pc_*`) **and the engine's SECOND CLIENT** (`fld_req_*` / `fld_resp_*`).
Its own text says client 1 "is also the seam the FLOW and EARTH adapters take
over when I5 and I34 close".

So:

* **I5** (R40, the FLOW adapter) and **I34** (R44, the EARTH / TERRAIN.PATCH
  lane) are the two candidate drivers for I42's client 1.
* **I42 cannot close without one of them**: a tied-off second client removes
  the arbiter's only reachable contention and with it the positive control for
  two of its counters, which the entry says in as many words.
* Either adapter is worth little without I42's LOADER: with no program
  resident, every run returns ST_NO_PROGRAM.

**They must land in ONE packet.** Doing R43 alone leaves I42 open; doing R40
alone leaves I5 closed against an engine with no program. Budget the FIELD
subsystem as a unit, not as three rulings.

Two further facts for that packet:

* R40's "the host widened to 13 inputs / 7 outputs" widens
  `zhao_field_host`'s `IN_LANES`/`OUT_LANES`, which are **shared with client
  0** -- `zhao_field_stamp_adapter` is parameterised on them and must be
  rebuilt at the same widths. That is a two-client change, not a one-adapter
  change.
* R40 also owes a BEFORE/AFTER RENDER for the owner to judge particle motion
  by eye. I found no particle render path in this tree; that is a
  reel/creature-pipeline job and it should be scoped WITH the packet rather
  than discovered inside it.

### R40's render requirement, restated
Particle MOTION is ART (CLAUDE.md). The acceleration mapping's shift and
saturation must be named editable constants AND the result must be looked at.
Neither happened here, because the mapping was not built.

## 4. OWNER DECISIONS (evidence + recommendation)

**(a) `SetPopulation`'s origin is declared `i32`, not `fx16`.**
`spec/qformats.md` 10 says "origin x/y/z as fx16 **on a 1/256-m grid**", and
`spec/commands.zidl`'s `fx16` is Q16.16 -- sixteen fractional bits, not eight.
The container is the same four bytes either way, so nothing on the wire
depends on the choice; the NAME is what a reader converts by, and Q16.16 would
be wrong by 256. **Recommendation: keep `i32` and amend qformats 10's
wording**, or rule that `fx16` there means "a 32-bit fixed-point word" rather
than Q16.16. The RTL law carried is `zhao_part_terrain_tap`'s own:
`world = origin + (local <<< 8)`.

**(b) `SPECIES_TABLE` took page kind 13 and section type 0x0011.**
Next free in both registries; `spec/cartridge.md` 4b is new and freezes the
layout. If the owner wants a different number it is one constant in three
places (`zref_species_page.hpp`, `zhao_part_table_loader`'s `PAGE_KIND`
parameter, and the console's `PART_KIND_SPECIES_TABLE`).

**(c) `ERR_RETRY_N` = 4.** The boundary between "the bridge was busy" and
"this burst will never be accepted". Nothing in the protocol distinguishes
them, so it is a judgement and it is a named parameter.

**(d) A faulted particle tick SWAPS to the truncated generation** rather than
replaying the previous one. Replaying ages no particle, which is a worse lie
than a shorter generation. One `else` to reverse.

**(e) The stamp's `blend` and `age_shift` are 0.** R45 ratifies only
`blend_en = 0`; the other two take the values that make an executor-issued
stamp behave exactly as the ABI describes it. Both are parameters.

## 5. FALSE CLAIMS AND INSTRUMENT DEFECTS FOUND

**The tree could not CONFIGURE at the base commit, and two gates were RED.**
`cmake --preset windows-native` failed with four `%Warning-PINMISSING` on
`build_hps_wr_*`, twice -- once for `tests/shell/zhao_shell_paired_diff.sv`
and once for its committed mutant. Those ports were added to
`zhao_shell_top_v2` at `ae7e8d37`, **a commit whose own subject says "WIP I26
... (BLOCKED, not for merge)" and which is on the shared branch**.
`packet_h_paired_diff_fresh` said STALE. Both files were
regenerated/refreshed in `bce0e43d`. **Another lane fixed the same thing
independently in `e87b17da`** -- the coordinator will have a trivial conflict
there, and the duplicated work is itself evidence that a "not for merge"
commit reached the shared branch.

**The paired-diff mutant is the CLAUDE.md stale-copy law with a twist worth
recording.** It went RED rather than quietly green, because the drift was
missing PINS -- an elaboration error, not a behaviour difference. Its header
said "GENERATED, do not edit" and nothing about being a mutant or needing a
refresh; it now says both.

**A measurement that did not move, and was NOT the stale-binary trap.** The
smoke's covered-texel count is 60 before and after the stamp envelope changed
from the bench's [-32 m, +32 m] to the patch's [0 m, 32 m). It is an identity:
halving the envelope quarters the texel area AND puts the disc in a corner, so
a quarter of the annulus is covered by a grid four times denser. It took
PRINTING the rectangle to be sure the change had happened -- the envelope is
now in the smoke's output for that reason.

**Three PowerShell backtick injections in one session**, each producing an
invisible control character inside a source comment (backtick-b -> backspace,
backtick-v -> vertical tab, backtick-t -> TAB). CLAUDE.md warns about this and
it still happened three times. The habit that caught them, run after every
here-string write:

    ([regex]::Matches($t,"[\x00-\x08\x0B\x0C\x0E-\x1F\x09]")).Count

**Recommend adding that sweep to a gate**; it is two lines and it catches a
class of damage that no lint sees.

**`String.Replace` replaces EVERY occurrence** -- bit once, adding a source
file to three fit lists instead of two. The tell was a source appearing under
a leaf fit that does not instantiate it.

## 6. RESOURCE COST (estimates; no fit run, per the packet rule)

Three new blocks, all small, all register-and-counter:

* `zhao_part_pop` -- the population descriptor bank. ~12 registers of payload,
  five 32-bit counters, no multiplier, no memory.
* `zhao_surface_dispatch` -- combinational shifts and a saturate, three
  counters. No multiplier, no memory.
* `zhao_part_table_loader` -- one 512-bit line register, a seven-state walk,
  six counters. No multiplier, no memory.

Plus: `zhao_geom_mem_adapter` widened from four requesters to five (the
share's round robin lengthens by one line; requester E reads THREE lines per
published species page); `zhao_part_hps` gained a fourth tick state and three
counters; `zhao_hps_arbiter_n` gained N+1 flops and one 32-bit counter.

Nothing here should be visible against the 47,582 ALM figure. The FIELD lane
(section 3) is where the real number is.

## 7. COMMITS

| hash | what |
|---|---|
| `bce0e43d` | regenerate the paired-diff harness and refresh its mutant (NOT my lane; the tree could not configure) |
| `72e1cf70` | R54 + R55 |
| `85a59cf1` | R41 -- SetPopulation 0x0303, I7 closes (33 -> 32) |
| `9a08923d` | R45 -- SURFACE.DISPATCH, I30 closes (32 -> 31) |
| *(this)* | R42 -- SPECIES_TABLE + the loader, I33 closes (31 -> 30) |

## 8. GATES AT THE LAST COMMIT

`completion_register` 30 - `check_console_inventory` OK -
`check_prod_manifest` OK - `gen_prod_top --check` fresh -
`gen_console_board --check` fresh - `mutant_copy_drift` OK -
`check_quartus17_syntax` RC 0 - `npm run abi:check` clean (30 outputs) -
smoke PASS `raster pixels=2560` `frames_admitted=1` - `-Mutant` PASS -
`-BadVertex` PASS - `-NoTableLoad` (repointed to the bad-magic species page)
PASS with inverted polarity.

Directed tests: `part_hps_directed` 132 - `hps_arbiter_n_directed` 137 -
`hps_arbiter_directed` 69 - `part_pop_directed` 253 -
`surface_dispatch_directed` 1072 - `part_table_loader_directed` 118 -
`part_table_directed` 68 - `geom_mem_adapter_directed` 35 -
`cmd_exec_directed` 590 - `part_state_directed` 78 -
`part_state_tick_boundary` 13 - `part_state_capacity_backstop` 9 -
`part_state_child_order_control` 5 (inverted polarity).