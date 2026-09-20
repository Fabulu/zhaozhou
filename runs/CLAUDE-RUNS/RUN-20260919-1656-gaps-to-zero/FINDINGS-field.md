# FINDINGS — the FIELD packet

Branch `gz/field`, from `1fbfacc9`. Worktree `C:\programmieren\zencrifice\gz-field`.

**Register: 29 at the starting commit, 27 at the last pushed one.**
(The brief said it should read 27 at the start; it read **29**. The brief's
number was one merge stale. No gap was invented or removed to reach it —
`1fbfacc9` itself prints 29.)

| commit | what |
|---|---|
| `f8440281` | I5 and I42 closed: the FLOW adapter and the program doorbell |
| `d7f2c322` | the directed tests, the `ret_overflow_o` mutant and both its controls |
| (this one) | the I34 write-up in the core header, and this file |

---

## Gaps closed

### I42 — the FIELD engine's loader, its directory phases and its second client

Owner rulings **R43** (the doorbell), **R20** (answered and counted) and **R55**
(held, never dropped).

`fpga/rtl/field/zhao_field_doorbell.sv` is SW.STREAM's program-load mailbox on
the R14 pattern R43 names, modelled on `zhao_terrain_jdoorbell` — the shape is
copied, the fields are this seam's. It drives all three parts the entry listed:

* **the loader** (`ld_*`), loader words posted verbatim by the HPS;
* **both directory phases.** The entry left the lookup's owner open. It is the
  HPS's, for the reason FIELD.PROGCACHE's own contract gives — the decode a
  miss requires "belongs to the caller", and `zfield::decode` is software, so
  the caller of a lookup is whoever can act on a miss. Hardware asking and
  software repairing would put two halves of one decision on opposite sides of
  the edge;
* **the second client**, taken by the FLOW adapter below. The entry's stated
  reason for that port — "it is what makes the arbiter's contention reachable
  with legal stimulus" — is better served by two REAL profiles offering in the
  same cycle than by a stand-in.

**The order law is the reason the block is not a FIFO.** A COMMIT for a slot
whose HEADER was not written since the last commit is REFUSED without the
directory ever seeing the hash, ANSWERED with a return record, and COUNTED. It
removes the failure class FIELD.PROGCACHE names (a directory promising a hash
to microcode that is not there), and it is reachable with legal stimulus — the
smoke bench fires it, in the smoke itself.

**Why this is a closure and not a port rename.** `fld_ld_*` at the console edge
was four fields and a 96-bit word with no identity, no ordering law and no
answer — a raw leaf port with nobody on the far side. What is there now is a
contract with a mailbox, an order law, a ticketed return and counters, and the
HPS is genuinely across that edge. It is the same act, with the same owner
ruling behind it, that closed I28 for the journal; the new `fld_db_*` ports are
declared NOT A TIE-OFF in the same words the `terr_jdb_*` group uses.

### I5 — PART.UPDATE's field sample

Owner ruling **R40**.

`fpga/rtl/field/zhao_field_flow_adapter.sv`, client 1 of the one engine.

The entry said the missing piece was the lane binding and that choosing it in a
composition packet "would be inventing an ABI". It is no longer being chosen:
`spec/form/field-ir.md` §7.1 ratifies the flow record (13 in / 7 out, in
declaration order) and R40 rules the mapping onto PART.UPDATE's port —
`sat_s11((v' - v) >> 8)`, seed = the variation byte, dt = 1 tick, host widened
to 13/7. Every constant R40 names is a **parameter** (`ACC_SHIFT`, `ACC_ROUND`,
`DT_FX`, `POS_SHIFT`, `VEL_SHIFT`), because R40 is provisional and says particle
motion is judged by eye.

`ACC_ROUND` is worth one line: R40 writes a shift, which is a floor, and
`zhao_part_update`'s own header warns that a floor biases a direction and
"would show up as drift over a thousand ticks". The default is R40 as written;
the alternative is one parameter away rather than one edit away.

**The units are the tree's, not new ones.** `zhao_part_terrain_tap` carries the
frozen conversion (qformats §10, its own elaboration guard refuses any other
POS_W): `world = origin + (local <<< 8)`. So the widening in is exact, and
R40's `>> 8` out is its precise inverse — the ruling and the frozen format
agree, which is why both shifts are 8 and are separately named.

**The join** is written where the entry said a composer may write it, at
PART.UPDATE, and it carries an identity guard whose two operands are loaded by
*different* enables (`held_rec` captured once at request time, `rec_i` live).
The answer is retired by the record's ACCEPT pulse, not by the offer falling:
PART.STATE can present back to back, so a machine waiting for a gap would carry
record A's acceleration onto record B. That is the metadata-swap shape exactly.

**The rate is a cost, not a gap.** One particle per field run against
PART.UPDATE's one per clock. `part_fld_stall_cycles_o` measures it; the lever is
the gathering front `zhao_field_host`'s header already calls for.

---

## Gap refused

### I34 — TERRAIN.PATCH's field-height lane

**Exact blocker: the handle -> program-hash mapping has no producer, and R43's
sentence assumes one.** R43 gives CMD.EXEC "only the handle -> program-hash
lookup that TerrainField needs", and TerrainField 0x0200 carries
`handle32[program] program`. FIELD.PROGCACHE's contract fixes the directory key
as a **content** hash — "the program hash `CRC32C(code||tables) + instr_count`",
computed by `zfield::programHashOfBytes` — so the handle is not the key, and
CMD.EXEC cannot derive one from the other without reading and hashing the
cartridge page.

**Searched, and named:** `program_hash`, `prog_hash`, `programHash` across all
of `fpga/rtl` including `synth/` — zero hits. `programHashOfBytes` exists only
in `reference/src/zfield/zfield_decode.cpp`. Nothing in hardware publishes
{handle -> hash}.

**Recommendation (owner to confirm).** SW.STREAM owns the mapping: it is the
only party holding both, since it names the program by handle in the plan and
computes the hash to post the commit. A fourth doorbell post kind,
`BIND {handle32, hash32}`, writing a small handle->hash table that CMD.EXEC's
TerrainField arm reads, closes it with **no ABI change and no second hashing
law**. The cheaper alternative — re-key the directory by handle — contradicts
FIELD.PROGCACHE's contract in writing, which is why it was not taken.

I34's two prerequisites are now closed, so what remains is this seam alone. The
full remaining build list is recorded **in the entry itself** rather than only
here, because a run folder is the wrong home for anything durable.

---

## Owner decisions found

### 1. R44's MEANS does not fit this seam (its GOAL still stands)

R44 says to "promote `zhao_probe_walk_earth`/`zhao_probe_patch_acc` out of
`fpga/rtl/synth/` to production names and homes, as FIELD v3 was, rather than
rebuilding them". **Both files were read.** They are the field-major, four-wide,
whole-patch topology of `reports/Fieldv3.md` Phase 4, and this console composes
the vertex-major one:

* `zhao_probe_walk_earth` GENERATES lattice points from two prepared 33-entry
  tables, precisely to "delete that transport" — but `zhao_terrain_patch`
  **already offers the point**, `vtx_valid_i` with `wx_i`/`wz_i`/`vi_i`/`vj_i`.
  On this seam the walker would be a second producer of a coordinate the
  consumer just handed over.
* `zhao_probe_patch_acc` is not a feeder of `terr_pt_fld_*`, it is a
  **replacement for the block behind it**: four M10K banks by vertex mod 4,
  sixteen RAMs, its own height/velocity/material/nav_cost reducers and
  INIT/ACCUM/DRAIN phases. It has no `fld_height` lane because it *is* the
  reducer `zhao_terrain_patch` already is — composed, fit-targeted, in the
  manifest.
* The console's own FIELD parameterisation argues the same way:
  `FAB_GROUP_PTS=1`, because this front holds one point in flight. The probes'
  four-wide group is the configuration that setting rejects, with its reasoning
  written out at the instantiation.

So promoting them is not a wiring act — it is swapping TERRAIN.PATCH's composed
architecture for another one. **That is an owner call.** R44's goal (do not
rebuild what exists) stands, and on this seam the thing that already exists is
TERRAIN.PATCH.

R44's other half is fine: the frame tick exists. `gpu_tick_o` is the shell's
frame boundary (already `core_tick_c` in the core) and a frame id rides beside
it, which PART.SPAWN's `tick_i` consumes today.

### 2. R40's rounding is a floor, and the block it feeds forbids floors

Recorded rather than decided. R40 writes `>> 8`; `zhao_part_update`'s header
says an arithmetic right shift "is a floor, which rounds -0.5 to -1 and +0.5 to
0 — a bias that points particles one way and would show up as drift over a
thousand ticks". Shipped as R40 wrote it, with `ACC_ROUND` as the named,
editable alternative. Particle motion is art and the owner judges by eye.

---

## False-absence claims found

None **asserted** this pass — but one was nearly created and is worth the line.
Entry I5's "nothing in this tree says which fields of the particle128 record
become which registers" had **stopped being true**: `spec/form/field-ir.md` §7.1
ratifies the flow profile's I/O record in full, and R40 supplies the rest. The
refusal survived its own cause. Checking the stated cause before inheriting it
is what closed the gap.

Two absences were confirmed REAL, by naming the search (above): no handle->hash
producer in `fpga/rtl`, and no `zhao_field_*` module already doing the FLOW
adapter's job.

---

## Instrument defects found

### 1. A lone CR promoted to a line break, in my own edit script

`tests/prod/tb_zhao_console_core_smoke.sv` holds **two lone CRs inside comment
lines** — owner ruling R61's hazard, live in the tree today. A text-mode Python
read with universal newlines promoted both to line breaks and split the
comments; the result was a syntax error the working tree showed and nothing
else did.

**`check_quartus17_syntax.py`'s CR scan covers `fpga/rtl` only.** `tests/` is
unguarded, and that is where the two live. Recommend widening the scan — the
file it would have caught is a bench the whole prod gate depends on. Every edit
script in this packet now reads and writes bytes.

### 2. The slot-overflow mutant wrapper was 40 ports stale and could not say so

`tests/mutants/zhao_console_core_slot_overflow_mutant.sv` was regenerated from
production as its own header instructs. It failed **loudly** (elaboration), not
silently, exactly as its header predicts — that is the correct failure direction
for a wrapper and the reason `mutant_copy_drift.py` is right to exclude them.
Worth recording that the only thing that ran it was this packet.

### 3. `zhao_part_record` was a census top that had become a double count

Instantiating the one codec in the FLOW adapter made it visible:
`zhao_part_record` was countable standalone only because its three particle-lane
instantiators are all `not-yet-adopted`. It is now counted inside its parent,
which is `prod_manifest.yml`'s own rule. **The census total is unchanged** — one
instance either way. `check_prod_manifest.py` caught it immediately and named
the instantiator, which is a gate working exactly as intended.

---

## Guards added, and whether each was seen to FIRE

| guard | fired by | how |
|---|---|---|
| `fld_db_commits_refused_o` | legal stimulus | `field_doorbell_directed` cases 4 and 6, **and the console smoke bench itself** |
| `fld_db_post_stalls_o` | legal stimulus | `field_doorbell_directed` case 5, with all five words then proven delivered |
| `fld_db_ret_overflow_o` | **committed mutant** | `tests/mutants/zhao_field_doorbell_mutant.sv` + `field_doorbell_mutant_control` (inverted polarity): `posted=8 ret_overflow_o=1` |
| (its negative control) | same stimulus, unmutated | `field_doorbell_directed` case 8: credit stops the drain at exactly RETQ (`lookups=4`), counter stays 0 |
| `part_fld_saturations_o` | legal stimulus | `field_flow_adapter_directed` case 3 |
| `part_fld_noprog_o` / `faults_o` / `bypassed_o` | legal stimulus | cases 5, 6, 4 — each counted apart, so a merged "no sample" total cannot hide which |
| `part_fld_rec_changed_o` | both halves | 7(a) asserts the correct behaviour across a back-to-back stream; 7(b) fires it by moving the record under a held answer |

Both halves are present for every counter asserted zero. A positive control
without its negative half proves only that some register somewhere can be
incremented — which is why case 8 exists.

---

## Gates at the pushed commit

| gate | result |
|---|---|
| `completion_register.py` | **27** (RC 1, normal), down from 29 |
| `check_console_inventory.py` | OK |
| `check_prod_manifest.py` | OK |
| `gen_prod_top.py --check` | fresh (67 instances) |
| `gen_console_board.py --check` | fresh (1187 core ports) |
| `mutant_copy_drift.py` | OK — no new drift |
| `check_quartus17_syntax.py` | RC 0 |
| console-board lint (waived set) | silent RC 0 — **it was RED at my branch point; see below** |
| `run_console_core_smoke.ps1` | **PASS**, `raster pixels=2560`, `frames_admitted=1` |
| `run_console_core_smoke.ps1 -Mutant` | **PASS** — `terr_pl_slot_overflow_o` fired 1 time |
| `field_doorbell_directed` | **48 checks passed** |
| `field_flow_adapter_directed` | **50 checks passed** |
| `field_doorbell_mutant_control` | **1 check passed** (inverted polarity) |

New RTL linted with an explicit `-Wall` (`verilate()` does not pass it), and
both new blocks have registered lint targets.

**Not run: Quartus.** The coordinator fits at completion. What this packet adds
is area-relevant and unmeasured, and the honest statement is that it is
unmeasured: two new blocks, plus the host's IN_LANES 12 to 13 and OUT_LANES 4 to
7, which widens the preload loop by one clock per point and both adapters'
request buses. The console's own FIELD note already records that neither FIELD
configuration fits; nothing here changes that argument, and nothing here should
be quoted as having measured it.


---

## One gate repaired that was not mine

The console-board lint was **already RED at 1fbfacc9**, my branch point, and
nothing in this run had noticed. Eight PINCONNECTEMPTY warnings, all from
TERRAIN.MIPGEN's retired coarse-height pins — terrain5 landed owner ruling
R64 (retire the planes) and left the eight pins named-but-empty on purpose, to
make the retirement visible at the point of use. That is the right call, and
the linter cannot tell a deliberate empty pin from a forgotten one.

Verified as inherited rather than assumed: git show 1fbfacc9: on the core
already contains .m17_valid_o(), and .m9_h_o     (),.

Repaired with a waiver **scoped to those eight pins**, citing R64 beside them —
deliberately not file-wide, so a pin left empty by accident anywhere else in the
module still fails. That property is the whole value of the check.

The reason for repairing someone else edits rather than only reporting them:
the gate is on every packet list in this run, and a gate that is red for a
reason nobody owns is a gate people learn to skip. It is two lines and trivially
revertible if the terrain lane wants it differently.
