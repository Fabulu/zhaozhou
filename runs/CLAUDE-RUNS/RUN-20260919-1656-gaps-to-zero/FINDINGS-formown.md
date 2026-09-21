# FINDINGS — packet FORMOWN

**Branch:** `gz/formown`, from `claude/ceiling-architecture-20260912` at
`9f7e2670`. Worktree `C:\programmieren\zencrifice\gz-formown`. Never rebased.

**Spec:** `reports/Zhaozhou_kind8_kind9_proposed_owner_ruling_2026-09-21.txt`,
pushed by the owner with the message *"Agent please read - The ruling you
wanted"*. 213 lines, read in full and implemented. This document records what
was built, what was measured, and the one thing in the ruling that was **not**
implemented and why.

**Register, run BARE (the pipe's RC lies):**

| | MANDATORY GAPS REMAINING | breakdown | RC |
|---|---|---|---|
| before | **24** | 9 tie-offs + 15 disconnected + 0 unbuilt + 0 uncited + 0 unresolvable | 1 |
| after | **24** | identical | 1 |

**I29 is REDUCED, not closed**, and the count is honestly unchanged. Blocker
(c) — *form -> resident page is unruled on both page kinds* — is answered and
gone. I29 itself remains because the ruling forbids closing it on these fields
alone (section 5: *"do not call I29 closed merely because these fields
exist"*), and because the composition and the instance walk are still absent.

---

## What was built

### 1. The bytes (ruling section 2)

One aligned little-endian `u32` per semantic `u24`. Bits 23:0 the owner's
MESH_STREAM form index; bits 31:24 **must be zero** and are REFUSED rather
than masked — masking would let two stored words mean one form.

| page | header | offset | version |
|---|---|---|---|
| kind-8 BODY | `TCB8` | bytes 16..19 | **v1 -> v2** |
| kind-9 CLIP_BANK | `ZCLP` | bytes 20..23 | **v1 -> v2** |
| kind-8 outer page + ladder record | `ZCFM` | — | **stays v1** |

Both headers are **still 64 bytes**. The words went into existing padding, so
`clip_page_v2.bin` is the same 704 bytes `clip_page_v1.bin` was, no bone
record / clip record / frame stride moved, `body_off` keeps its meaning, and
the reader needs no extra header transaction — it already read a whole line.

**Index zero is not a sentinel.** A body and bank owned by form 0 build, adopt
and serve a draw of form 0; a draw of form A against them is refused like any
other mismatch. Asserted in both the reference test and the RTL test.

### 2. Versions split rather than bumped (ruling section 2)

`zhao_geom_clipread` had ONE `VERSION` constant. It now has `FORM_VERSION`
(1), `BODY_VERSION` (2) and `CLIP_VERSION` (2), for the ruling's stated
reason: *"merely changing that single constant to 2 would reject the still-v1
outer header."* A bodyless outer-v1 ladder page stays legal and the bench
asserts it raises **no** fault counter at all.

The v1 goldens are still committed, still identifiable (`body::body_version()`,
`clip_page::page_version()`), and are read **by the RTL** as pages it must
refuse — which is how *"unowned v1 data is not accepted on the posed-render
path"* becomes a measurement instead of a sentence.

### 3. Packers require an explicit owner (ruling section 2)

`mkcreatureladder.py --body-owner` is REQUIRED beside `--bones`, refuses a high
byte, and refuses an owner with **no ladder record on the page**.
`mkclipbank.py --owner` (or `owner_form_index` in the JSON) is required and
refuses a high byte. `zref::creature_page::body_owner_in_ladder` is the one
statement of the page-consistency rule and `build_with_body` enforces it.

**The goldens' owner is `0x000101` — `GOLDEN_RECORDS`' SECOND row, on
purpose.** Row zero is `0x000100`. So "read the owner word" and "read row zero"
produce different bytes, and `--check` asserts they differ: a packer or reader
that quietly infers from row zero is caught by the fixture rather than by an
argument. The ruling forbids inferring from row zero, from a matching bone
count, from the publication index and from loader order; none of those is
evidence and a packer has no honest way to guess.

### 4. End-to-end association, producer and consumer TOGETHER (ruling section 3)

**Producer.** `zhao_geom_drawjob.j_form_idx_o` is `form_idx_q` itself —
`d_form_i[31:8]`, `spec/memory_rules.md` 5f.1's residency key — **exposed, not
re-derived**. All 24 bits. Driven from the register ONLY while `j_valid_o` is
high, because in that block's `S_IDLE` it still holds the PREVIOUS draw; the
core entry had predicted exactly this and said it must be qualified by state
and never offered bare. `geom_drawjob_directed` records it on the handshake,
requires it on every emitted job, and counts every cycle it is non-zero while
`j_valid_o` is low (**0**).

It reaches the core edge as `geom_job_valid_o` / `geom_job_form_idx_o`, beside
R229's pose key and for that paragraph's own reason: an output nobody reads
lets synthesis delete the registers behind it and the next fit prices the lane
at zero.

**Consumer.** `zhao_geom_clipread.p_form_idx_i`, landed in the same commit. The
block refuses any request whose form is not the form BOTH resident sections
name, counting it on a new `owner_mismatch_o` — deliberately **not**
`clip_miss_o` (a miss is a question this bank could have answered) and **not**
`not_resident_o` (something IS loaded; it belongs to somebody else).

**Three identities, reported separately.** `res_body_owner_o` /
`res_clip_owner_o` are FORM identity; `res_*_index_o` / `res_*_gen_o` are
RESOURCE identity. The directed test loads one creature under two different
resource indices so the two cannot be read as one number.

### 5. Two things that would have made the check blind, and were avoided

**The comparison has three operands with three independent loads.**
`p_form_idx_i` arrives combinationally from the draw, `body_owner_q` is written
in `S_BR_FILL` out of a kind-8 publication, `clip_owner_q` in `S_CD_ROW` out of
a kind-9 one. No single register enable moves two sides of it. Comparing the
two pages to *each other* instead would pass for two pages of one foreign
creature — which is the ruling's test D, and it is asserted.

**Ownership is adopted with the payload, not at the header.** The first version
of this RTL wrote `body_owner_q` in `S_BH_HDR`. That is wrong in a way no test
would have shown on legal traffic: a read DENIED between the header and the
first record store leaves `body_v_q` still 1 (the old skeleton) while
`body_owner_q` already holds the NEW page's identity — a live store and a fresh
owner belonging to two different creatures, with every counter green. It is now
staged in `owner_stage_q` and committed only beside `body_v_q`/`clip_v_q`. Case
14 (the ruling's test F) denies the third read of a foreign body and requires
the reported owner to still be the resident one's, then requires the
half-landed form to be refused.

### 6. The pose cache's two obligations (ruling section 4)

**4a, width.** `acq_type_i` and the stored tag were 16 bits against a 24-bit
form index, so `0x000100` and `0x010100` were ONE cache line. `TYPE_W` now
parameterises the field, **default and production 24**, with an elaboration
guard refusing anything outside 16..24 — fired at `-GTYPE_W=8` by
`geom_pose_cache_elab_guard` under `WILL_FAIL`, because `--lint-only` does not
run `initial` blocks. The alias is asserted distinct on the **hit** path as
well as the miss path: an insert that stored 24 bits with a comparison that
read 16 passes only the second.

**4b, asset lifetime.** `acq_body_idx_i`, `acq_body_gen_i` and
`acq_clip_idx_i` join the clip generation. **No field is compared to another
field anywhere in the file** — body and clip generations belong to independent
publications. Six probes, each holding form, clip, frame and sub-phase
constant:

| what moves | verdict |
|---|---|
| body generation alone | MISS |
| body resource index alone (same generation number) | MISS |
| clip resource index alone (same generation number) | MISS |
| clip generation, differing only above bit 7 | MISS |
| body generation, differing only above bit 7 | MISS |
| both generations *equal* | ordinary MISS, then ordinary HIT |

...and the warm entry **still hits afterwards**. Without that last one, a cache
that dropped the original on every probe would produce the same six MISS
verdicts and be broken in the opposite direction.

### 7. The positive control (ruling section 6, test J)

`tests/mutants/zhao_geom_clipread_ownerblind_mutant.sv` — one substantive line,
`owner_ok_c` becomes `1'b1`. Renamed so no source list can elaborate it. The
driver's polarity is inverted: it passes when the fault occurs.

`owner_mismatch_o` **is** reachable by legal stimulus (it fires five times in
the directed test), so this mutant is not here because the guard is
unreachable. It is here because the ruling requires the NEGATIVE TESTS to be
shown to depend on the comparison rather than on something else refusing the
same traffic — those pages are legal, adopt cleanly, and share bone count, clip
id, frame count, frame number and sub-phase.

Measured: `frames=1 owner_mismatch=0 bone_mismatch=0 quat_words=6`. Draw A is
served from creature B's clip bank and six quaternion words of a foreign
palette reach the store with every counter reading correct.

**The plant is proven before the result is quoted.** The driver reads the
mutant's own source and requires the mutated line PRESENT *and* the production
expression ABSENT — either alone is satisfied by a file containing both, and a
fire test compiled against an unmutated copy passes while measuring nothing.

---

## Evidence

| test | before | after |
|---|---|---|
| `geom_clipread_directed` | 443 checks | **762** |
| `clip_page_directed` | 46 checks | **58**, 17 verdicts fired |
| `geom_pose_cache_directed` | 61 checks | **76** |
| `geom_pose_cache_random` | — | 1,976 checks |
| `geom_drawjob_directed` | 287 checks | **289** |
| `geom_ladderbank_directed` | 545 checks | 545 (unchanged, still green) |
| `geom_bonesrc_directed` | 6 cases | 6 cases (moved to the v2 golden) |
| `geom_bonesrc_latefetch_mutant` | fires | fires (53 palette elements wrong) |
| `geom_clipread_ownerblind_mutant` | — | **11 checks, all inverted** |
| `geom_pose_cache_elab_guard` | — | **fires** at `-GTYPE_W=8` |
| `mkcreatureladder --check` | 11 refusals | **15** |
| `mkclipbank --check` | 10 refusals | **12** |

`geom_clipread_directed`'s counter line after the run:

```
bodies=8 clips=10 frames=15 dropped=1 bad_magic=4 truncated=1 misaligned=1
bad_bones=1 mismatch=1 overflow=1 not_rigid=1 rsv_nz=3 denied=2 clip_miss=1
frame_oob=1 not_resident=2 owner_mismatch=5
```

The ruling's section 6 cases A–J map onto cases 11–14 of that file; **B, C and
D run at identical bone counts, clip ids, frame counts and frame numbers**, and
`bone_mismatch_o` is asserted SILENT through all of them. `kFormA = 0x000123`
and `kFormB = 0x010123` differ ONLY in bits 23:16, so every one of those
refusals would have been a HIT on a 16-bit key — asserted directly.

---

## Gates run, and why that set

Always-on, all RC 0: `check_console_inventory`, `check_prod_manifest`,
`check_quartus17_syntax` (582 files, 13 fire / 22 no-fire self-test),
`check_case_labels`, `mutant_copy_drift` (run AFTER each commit, R121; 60
copies, OK), `mutant_drivers`, `uncashed_cheques`, `refmodel_liveness`,
`duplicate_functions`, `check_counters`, `check_findings_citations`,
`packet_h_tieoff_audit`. `completion_register` RC 1 with 24 gaps, which is
normal while gaps remain.

**Ports changed**, so all three generators: `gen_prod_top` and
`gen_console_board` reported STALE, were regenerated, and re-checked FRESH;
`gen_shell_paired_diff --check` and `--check --mutant` FRESH throughout. The
regeneration diff was read — `zhao_prod_top` gained `u17_j_form_idx_o` and the
pose cache's four new ports, with their entries in the fold expression
renumbered, which is what a correct regeneration looks like.
`wrapper_port_parity` moved **1280 -> 1282** and both `zhao_console_core`
wrapper mutants were updated in the same commit.

**ABI not touched** — no opcode, field or size moved, so `npm run abi:check`
was not required. `0x0304` and `0x0305` were not disturbed.

**No Quartus.** Standing instruction, and no question here needs one:
correctness, handshake behaviour, field routing and atomicity under
backpressure are Verilator questions and answered in seconds.

---

## Cost, recorded as a fact

* **Pages: zero growth.** 8 bytes of existing header padding per page kind, no
  extra header transaction.
* **Ownership logic:** registers, equality and control. No DSP.
* **Pose cache tag:** 192 bits against 120 at `TUPLES=128`, i.e. **+9,216
  logical bits** — 1,024 for 4a's eight form bits, 8,192 for 4b's two resource
  indices and body generation. This is a LOGICAL bit count and is **not** an
  M10K or ALM figure. Physical RAM packing, the wider comparator, the four
  extra input ports and the two new core boundary ports are what a fit measures
  and nothing else does.
* **A fit is owed** when `zhao_geom_clipread` and `zhao_geom_bonesrc` compose —
  that is the subsystem boundary, and it was already owed before this packet.

---

## What in the ruling was NOT implemented, and exactly why

**One thing, and it is section 2's page-consistency rule as an RTL check.**

> *"Validate that a body-bearing kind-8 page contains a ladder record for its
> body owner."*

Implemented in the **reference and the packers** (`body_owner_in_ladder`,
`build_with_body`, `mkcreatureladder --body-owner`), where the sentence sits in
the ruling's own paragraph about packers, and asserted in
`geom_clipread_directed` case 12 against the golden. It is **not** checked by
`zhao_geom_clipread`, and that is deliberate: the block jumps from the outer
header straight to `body_off` and never walks the ladder table, which is
`zhao_geom_ladderbank`'s job one block over. Making it walk the records would
duplicate that block's decode inside this one — a second implementation of a
ladder reader, which is the shape this tree strikes hardest — to re-check a
property the packer cannot emit a page violating. If a future owner wants it in
silicon, `zhao_geom_ladderbank` already has the records and the comparison
belongs there, against `res_body_owner_o`.

Everything else in the ruling is implemented: section 1 (three identities),
section 2 (bytes, versions, packers, goldens, v1 identifiability, bodyless
outer-v1 legality), section 3 (producer and consumer together, all 24 bits,
request lifetime, staged adoption, refusal counted rather than treated as a
miss), 4a, 4b, section 5 (no multi-body object database was grown; the staging
tier is untouched and I29 is not claimed closed), section 6 tests A–J, and
section 7 (costs recorded, no Quartus, no fit-saving or complete-I29 claim).

**Two things the ruling forbids and that were NOT done**, stated because their
absence is the point: no bank handle was added to `DrawPosedForm` to evade the
association check, and no second game-facing type identity was introduced.

---

## Live lanes

File sets touched: `reference/include/zref/zref_creature_page.hpp`,
`zref_clip_page.hpp`, `tools/pack/mkcreatureladder.py`,
`tools/pack/mkclipbank.py`, `fpga/rtl/geometry/zhao_geom_clipread.sv`,
`zhao_geom_drawjob.sv`, `zhao_geom_pose_cache.sv`, `tests/geometry/*`,
`tests/mutants/*`, `tests/golden/creature_clip/`,
`tests/golden/creature_ladder/`, `spec/cartridge.md`,
`design/contracts/GEOM.POSE.md`, and the shared
`fpga/rtl/prod/zhao_console_core.sv`, `tests/CMakeLists.txt`,
`design/console_inventory.yml`, `design/prod_manifest.yml`.

**No overlap with WARPBUILD** (GEOM.WARP — geometry/field) or **TERRASSEM**
(TERRAIN's `sp_*` assembler): nothing here touches `zhao_geom_warp*`,
`zhao_field_*` or `zhao_terrain_*`. The shared files are touched in distinct
regions — the I29 entry and the R229 port block in the core, the GEOM.POSE
block in `tests/CMakeLists.txt`, the `zhao_geom_clipread` / `zhao_geom_bonesrc`
entries in both YAMLs. The three GENERATED artifacts (`zhao_prod_top.sv`,
`zhao_console_board.sv`, `zhao_shell_paired_diff.sv` + mutant) must be resolved
by **REGENERATING**, never by merging text.

## Superseded

`FINDINGS-formidx.md`'s re-costed options for the form -> page question are
superseded by the owner ruling. `FINDINGS-poseread.md` and
`FINDINGS-posepage.md` remain accurate about everything except the sentence
"form -> clip bank is unruled", which was true when written and is not now.
