# FINDINGS — I34CLOSE, 2026-09-27

**Branch `gz/i34close`.** Base `b506e208`.
Answers the brief's nine numbered deliverables in order.

---

## THE ONE-LINE ANSWER

**`I34` DOES NOT CLOSE, and the word `BOUNDARY` in its head is stale ANYWAY.**

Both halves are true at once, and that is why six packets have landed between
them. The three ports the register's classification names **do not exist**; and
the entry is nonetheless still open, on **three obligations that appear in no
channel table, in no brief, and in neither material packet's findings.**

**Substance did not move. Prose did not move the number. Register 2 -> 2, bare.**

---

## 1. THE ENUMERATION

Every obligation `I34` places on the console, measured at `b506e208`.
**MET / NOT MET / STALE TEXT**, with the measurement beside it.

### 1a. The four Earth out-lanes (directive 20.8's routing)

| # | channel | state | measurement |
|---:|---|---|---|
| 0 | `height_o` | **MET** | `efa_height` -> `.fld_height_i` (`core:28936`) -> `u_terrain_patch`. `composepub_acceptance` case 2 — **154 checks, 0 failures, rc 0**, re-run by me at this commit. |
| 1 | `velocity_o` | **MET** | `efa_velocity` -> `.a_velocity_i` (`core:29007`) -> `u_terrain_veljoin` -> TERRAIN.VELOCITY -> compcache 4.2 -> spdesc -> heighttap -> `zhao_part_collide`. `terrain_veljoin_directed` **19 checks, rc 0** on the REPAIRED test; `part_terrain_tap_directed` **1297 checks, 0 failed**. |
| 2 | `material_o` | **MET at the FABRIC**, see 1b | `efa_material` -> `.f_material_i` (`core:29370`) -> `u_terrain_matjoin` -> layer-E plane -> compcache SERVE -> TESS -> `tcf_tri_mat_*_w` -> `zmt_encode` -> `.t_material_token_i` (`core:22818`) -> clipfeed -> setup/attrpack -> binner -> mosaic. `composepub_acceptance` case 12, `terrain_matjoin_directed` **50**, `terrain_clipfeed_mat_directed` **41**, all 0 failures. |
| 3 | `nav_cost_o` | **MET by cited ruling + built replacement** | Classified, **citation verified present at four sites** (below). Replacement `zref::nav::Service` built, integrated and tested: `nav_service_directed` **113/0**, `wizards_nav_directed` **77/0**. |

**Nav's classification cites its ruling by document name — verified, not
assumed:** `zhao_field_earth_adapter.sv:490`, `zhao_console_core.sv:29242`,
`design/ops.yml:547`, `design/blocks.yml:7661` all name
`reports/OWNER-DECISION-20260926-I34-NAV.md`. **Deliverable 3's nav item is MET.**

### 1b. THE THREE OBLIGATIONS NOBODY ENUMERATED

These are the reason the entry stays open. **None of them is a channel, and
none of them appears in my brief's channel table.**

| # | obligation | state | measurement |
|---:|---|---|---|
| A | **`TERRAIN.COMPOSED_MATERIAL`** SDRAM derived cache, `[0x058B_0000, 0x05AB_0000)`, 2 MiB | **NOT MET** | **Zero hits across `fpga/rtl/` and zero in `spec/memory_rules.md`.** The 5b region table carries `COMPOSED_HEIGHT`, `COMPOSED_VELOCITY`, `COMPOSED_MIP_POOL` and **no composed-material region at all**. The allocation exists only in `reports/`. **No decision record refuses it.** |
| B | **Directive 13.7's `-FieldActive` positive composed smoke mode** | **NOT MET** | `grep -rn FieldActive` -> **six hits, every one prose**. `run_console_core_smoke.ps1` has no such switch and the string `field` appears in it only in three unrelated comments. The entry itself says so: *"The smoke has eight forms and none of them is that one."* |
| C | **`TERRAIN.COMPOSED_VELOCITY`'s REAL writer** | **DISCHARGED BY A CITED REFUSAL** | `spec/memory_rules.md:450` — decision record, packet COMPOSEPUB 2026-09-25, under the directive's own section 0 delegation. Refused on measured consumer-absence plus a bandwidth finding (baseline 80.17% of frame -> 124.41% with the write side -> 177.49% with the backing read). **Counted as met-by-citation, not as a gap.** |

**Why (A) is live and not superseded, stated with its citation chain, because
this is the load-bearing finding:**

* The owner's vacation directive section 1 DESTINATIONS commissions it.
* **DECISION RECORD 1** (`reports/OWNER-RULINGS-20260919-EVENING.md:7934`) swept
  the address map, moved `COMPOSED_NAV` off POST.ECHO, and left
  `TERRAIN.COMPOSED_MATERIAL [0x058B_0000, 0x05AB_0000)` **"(unchanged)"**.
* On **2026-09-26 the owner struck `COMPOSED_NAV` and, in the same breath,
  wrote that `COMPOSED_MATERIAL` is UNAFFECTED and remains live** — in the
  directive's own amended-in-place block and again in `reports/DOCKET.md:33`.
  **The owner looked directly at this pair of regions and deliberately struck
  exactly one of them.** That is the strongest available signal the other stands.
* **Neither material packet ever mentions it.** `grep COMPOSED_MATERIAL` over
  `FINDINGS-MATERIALPATH.md` and `FINDINGS-MATCARRY.md` returns **zero hits in
  both**. The two packets that built the material channel end to end never met
  the owner's destination requirement for it.

**I am not asserting the region must be built as specified.** COMPOSEPUB's
refusal of the sibling regions may well apply here too — the consumer test and
the bandwidth arithmetic are the same shape. **What I am asserting is that
nobody has asked.** (C) is discharged because a packet measured it and wrote
the record. (A) has no record of any kind, and an obligation with no record is
not a discharged obligation — it is an unexamined one.

### 1c. STALE TEXT — the entry describes a console that no longer exists

| claim | where | measurement |
|---|---|---|
| **`-- BOUNDARY`**, naming `terr_pt_fld_valid_i`, `_ready_o`, `_height_i` | the head line, `core:6115-6116` | **STALE TEXT.** Those three identifiers occur in `zhao_console_core.sv` **five times, every one inside a comment, zero as a port declaration.** The surviving `terr_pt_fld_*` ports are three OUTPUTS (`add_accept_o`, `add_reject_o`, `covers_o`). |
| *"section 9.1 list intake (`terr_pt_fld_add_*`)"*, also in the head | `core:6116` | **STALE TEXT.** Closed by FIELDARM 2026-09-22; the eight intake ports left the edge. Only the two `add_*` report outputs remain. |
| *"2 material -> NOBODY. The only channel still open"* | `core:29232-29238`, the adapter's own "TRUE ACCOUNTING" | **STALE TEXT, and self-refuting at thirty lines' distance** — `u_terrain_matjoin` reads `efa_material` at `core:29370`, in the same file, below the comment. |
| *"`ans_present_o` — PRODUCED, NOT CONSUMED"* | `core:29249` | **STALE TEXT.** `efa_present[2]` is read at `core:29369`. |
| *"The three adapter outputs with no consumer are left OPEN at that instantiation"* | `core:13990` | **STALE TEXT.** One is open (`nav_cost_o`), by owner ruling. Two have consumers. |
| **(M7)** *"the composed triple ... STILL HAS NO READER"* | `core:6231`, the entry's OWN NEWEST BLOCK | **STALE TEXT.** MATCARRY landed `tcf_tri_mat_token_c` -> `.t_material_token_i` at `core:22818`. The newest block in a layer-cake entry was already out of date. |

**The register classifies `I34` `boundary` by regex-matching the word `BOUNDARY`
in that prose** (`completion_register.py:62`, `:238`). So the entry is currently
counted as a gap **for a reason that is measurably false**, while being a gap
**for three reasons the entry does not state.** Both errors were live
simultaneously.

---

## 2. THE REGISTER, MEASURED BARE

| | value |
|---|---|
| before (`b506e208`) | **2** — `I34`, `I55`. RC 1 |
| after | **2** — `I34`, `I55`. RC 1 |

`python tools/budget/completion_register.py`, **bare**, exit code read from the
command itself. Mandatory RTL capabilities connected: **113**; `BUILT BUT NOT
CONNECTED` 0, `NOT BUILT AT ALL` 0, `EXCUSED BY AN UNCITED FLAG` 0,
`UNRESOLVABLE` 0.

**`I34` did not close.**

## 3. THE FOUR DEMONSTRATED PATHS

Not required, since the entry does not close — but measured anyway, because the
brief asks and because the next packet should not re-run them. **All seven
tests built and RAN at this commit; check counts are mine, not quoted:**

```
terrain_veljoin_directed        19 checks             rc 0   (the REPAIRED test)
part_terrain_tap_directed     1297 checks, 0 failed   rc 0
composepub_acceptance          154 checks, 0 failures rc 0
terrain_matjoin_directed        50 checks             rc 0
terrain_clipfeed_mat_directed   41 checks             rc 0
nav_service_directed           113 checks, 0 failures rc 0
wizards_nav_directed            77 checks, 0 failures rc 0
```

## 4. DID SUBSTANCE OR PROSE MOVE IT? — NEITHER

**The register reads 2 before and 2 after. Nothing moved it.** I built no RTL
and I did not touch the head line's classification. There is no prose-only
closure to revert, because there is no closure.

## 5. VELOCITY STILL REACHES ITS CONSUMER

`terrain_veljoin_directed` **19 checks, rc 0**, on the repaired test — run, not
quoted. `part_terrain_tap_directed` **1297 checks, 0 failed**. Neither moved.

## 6. WHAT I FOUND FALSE IN THIS BRIEF AND THIS ENTRY

**The brief is wrong in the place it told me to look for it, and the error is
the campaign's own signature direction — it makes the work look smaller.**

1. **The brief's channel table is presented as the enumeration, and it is not
   the enumeration.** It has four rows, all four of which are MET. **Three
   obligations sit outside it** (1b). A packet that verified all four rows and
   stopped would have closed the entry on a complete-looking table.
2. **The brief reduces nav to "confirm the classification CITES its ruling."**
   The owner's own words are far stronger: *"Keep navigation genuinely open
   until that service is implemented, integrated and tested"*, and standing
   authorization 3, *"I34's nav obligation closes only when its replacement CPU
   service is implemented, integrated and tested."* The citation check is a
   fraction of that bar. **It happens to be MET** — NAVSERVICE did the whole
   thing — but the brief asked for the small half of a large obligation, and if
   NAVSERVICE had cut a corner the brief would not have caught it.
3. **The brief's central suspicion is CORRECT and INSUFFICIENT.** *"Whether
   there is substance behind that word, or the text is simply stale"* is posed
   as a two-way choice. **The answer is BOTH: the text is stale AND there is
   substance.** The three named ports are gone; the entry is still open. Posing
   it as an either/or is exactly what would let the seventh packet close it on
   discovering the first half.
4. **(M7)'s blocker is retired** — `core:6231` versus `core:22818`.
5. **The adapter's "TRUE ACCOUNTING" is false on material**, thirty lines above
   the instance that refutes it (`core:29232` versus `core:29370`).
6. **The entry's "WHAT CLOSING THE REST NEEDS" list (`core:8196-8200`) is
   three-quarters spent**: patch_v2's four-channel clause is retired by
   measurement, the EARTH stream adapter is built, and only 13.7's
   `-FieldActive` survives — which is obligation (B) and the one nobody carried
   forward.

## 7. WHAT I REFUSED, AND WHAT I GOT WRONG

### Refused

* **To close the entry.** Two obligations are NOT MET with no citation
  discharging either. Closing on the four-channel table would have made the
  register lie, which the brief names as the worst outcome available here.
* **To edit the head line's classification.** Rewriting `-- BOUNDARY` into
  anything is a prose act on a register that reads prose, and with substance
  outstanding it is the renamed gap the owner forbids by name. **The stale text
  is recorded here and struck in the entry WITHOUT touching the
  classification**, so the count cannot move on my wording.
* **To build `TERRAIN.COMPOSED_MATERIAL`.** It is an ABI-scale memory-map act
  against a region never enacted into `spec/memory_rules.md` 5b, and
  COMPOSEPUB's sibling refusal suggests the honest answer may be a decision
  record rather than a writer. **That is the next packet's question and it
  should be asked as DECISION-OR-BUILD.**
* **To build `-FieldActive`.** The entry measures five distinct blockers for it
  (`core:7964-7991`), starting with `REGS=32` composed against `crater_ring`'s
  need for 36. It is a packet, not an afternoon, and the entry says so.
* **To touch GEOM.** Nothing of SWAPCLOSE's was edited.

### What I got wrong and caught myself

* **I nearly reported the entry as closable on the strength of 1a alone.** All
  four channels measure MET and every test I ran was green; the table was
  complete and the conclusion was sitting there. What stopped it was the brief's
  own instruction to ask *"WHAT ELSE?"* and then sweeping the entry for the
  words `STILL OWED` — which returned 13.7 in one line. **The enumeration found
  the thing the channel table structurally could not contain.**
* **I initially counted `COMPOSED_VELOCITY` as a NOT MET obligation.** It is
  not: `spec/memory_rules.md:450` carries a full decision record refusing it
  with measurements. Corrected to met-by-citation before writing it down. The
  lesson is the brief's own: **check whether a refusal's record exists before
  calling an absence a gap** — and the same check is what makes (A) damning,
  because there the record does not exist.

## 8. ANYTHING BELONGING TO SWAPCLOSE

`zhao_geom_setup`'s `IDW` parameter and `GEOM_VID_RIDERW`'s 50 -> 82 widening
(MATCARRY) are **in GEOM's files and already landed**. I read them as evidence
for the material path and **changed nothing**. `tcf_tri_mat_token_c` and
`zhao_terrain_clipfeed` are TERRAIN's and are mine to read; I changed neither.

**Reported, not touched:** the `a_attrpack_setup_same_triangle` assertion
(MATCARRY section 3) now differences the widened identity whose middle 32 bits
are the material token. Its two operands are `zhao_geom_setup`'s s3 register and
`zhao_geom_attrpack`'s `src_id_q` — **two modules, two enables**, so it is not
the co-clocked shape `CLAUDE.md` forbids quoting. That is SWAPCLOSE's file and
its property; I verified the claim's shape and left it alone.

## 9. BRANCH AND COMMITS

**`gz/i34close`**, pushed. Never `--force`, never `--force-with-lease`.

---

## 10. THE CONSOLE SMOKE SHARPENS OBLIGATION (B), MEASURED AT THIS COMMIT

`tests/prod/run_console_core_smoke.ps1`, plain form: **PASS, RC 0**.

```
raster     pixels=2816 bursts=176 ... fatal=0 stream_err=0 overflow=0
renderlease frames_admitted=1
terrcompose ps_lattices=11 ps_vertices=11979 place_patches=1 pt_samples=1089
            cc_records=1089 mat_cells=1024
terrmat     field_composed=0  token_refused=0  held_overrun=0 | cc_mat_cells=1024
terrmat     backed=128 orphan=0 (SetEnvironment terrain_material set=00abcd02 id=2)
mosaic      fills[tileset/mesh/stray]=[23 21 0]  tile[max/or]=[6 7]
```

**`field_composed=0` is the number that matters, and it is the console's own
counter saying obligation (B) is not cosmetic.**

The authored material path is proven end to end in the composed console —
`tile[max/or]=[6 7]` is derived from the page's own layer-E bytes, `mat_cells`
asserts the complete 1,024-cell fill, `stray=0`. **But `field_composed` is ZERO,
because no `TerrainField` is ever issued in any smoke form.**

So the owner's acceptance bar — *"verify that a Field material write changes the
intended consumer"* — is met **at a leaf bench** (`composepub_acceptance` case
12, through the real `zhao_field_earth_adapter`) and is **NOT exercised in the
composed console at all**. That is precisely the hole directive 13.7's
`-FieldActive` exists to close, and it is why (B) is a real obligation rather
than a tidy-up: **the composed console has never once run a field.**

`held_overrun=0` and `token_refused=0` are consistent with that and are not
independent evidence — with no field live they cannot move, which is worth
saying so the next packet does not quote their silence.

## 11. GATES AT THE PUSHED HEAD

| gate | result |
|---|---|
| `completion_register.py` (bare) | **2** (`I34`, `I55`), RC 1 — unchanged from base |
| `check_console_inventory.py` | OK — 408 modules, 296 fit sources |
| `check_prod_manifest.py` | OK — 408 modules, 88 tops |
| `gen_prod_top.py --check` | fresh (88 instances) |
| `gen_console_board.py --check` | FRESH (1616 core ports, 127 parameters) |
| `check_quartus17_syntax.py` | RC 0 |
| `gen_shell_paired_diff.py --check` | fresh (harness and mutant) |
| `check_case_labels.py` | OK, self-test 3 fire / 1 no-fire |
| `check_console_closure_lint.py` (**gate 31**) | OK — no implicit net, no missing module, no missing pin |
| console smoke, plain | **PASS**, `raster pixels=2816`, `frames_admitted=1` |
| `mutant_copy_drift.py` | run AFTER the commit per R121 — see below |
| the seven directed tests | built and RAN, section 3 |

**I changed COMMENTS ONLY.** No port moved, no parameter moved, no RTL
behaviour changed — which is why `gen_prod_top`, `gen_console_board` and gate 31
are all still fresh across an edit to `zhao_console_core.sv`. I ran them anyway
rather than reasoning that a comment cannot matter: NAVSERVICE's own section 8.6
records a **comment-only edit turning a mutant copy stale**, so "it is only a
comment" is a claim this campaign has already disproved once.

---

## 12. CORRECTION TO MY OWN FINDING (A), AND THE DECISION IT NEEDS

**I wrote in section 1b that "nobody has asked" about
`TERRAIN.COMPOSED_MATERIAL`. That is too strong, and the precise version is
more useful.** Found by sweeping the remaining mentions rather than stopping at
the RTL grep.

**THE MEASUREMENT EXISTS.** `reports/OWNER-ESCALATION-20260926-I34-ADDENDUM-2.md`
FAULT 2 and `BRIEF-FABRICSINK.md` both carry it, from
`tools/budget/sdram_bandwidth.py`:

| | SDRAM cycles | frame |
|---|---:|---|
| today | 330,474 **free** | 19.83% headroom |
| + composed VELOCITY publish | 406,806 **over** | 24.41% oversubscribed |
| + the fill-side read that makes it a consumer | 1,291,542 **over** | 77.49% oversubscribed |

**One 2 B/vertex plane costs 737,280 cycles against 330,474 free — 2.2x the
entire headroom on its own**, and material-as-u32 is about twice velocity's
width. The pair needs on the order of **3.7 M cycles against 330 k free.**

**WHAT DOES NOT EXIST IS THE DECISION RECORD**, and the difference is the whole
point:

* For `COMPOSED_HEIGHT` and `COMPOSED_VELOCITY`, COMPOSEPUB wrote a full
  decision record **into `spec/memory_rules.md:450`** — the place decisions
  bind — with question, chosen option, alternatives, consequences. **That
  obligation is discharged.**
* For `COMPOSED_MATERIAL` the same arithmetic was delivered **as an
  ESCALATION**, which handover 15.30 has already criticised in exactly these
  terms: the escalation *"declared its own default … and under a standing
  vacation directive, saying nothing was always going to be the state. A
  default nobody executes is not a default; it is a second escalation wearing a
  decision's clothes."*
* **And the owner then answered that escalation** — on 2026-09-26 — **striking
  `COMPOSED_NAV` and writing in the same breath that `COMPOSED_MATERIAL` is
  unaffected and remains live.**

**So the state is: a sound measurement, no binding record, and an owner who
looked at this exact pair of regions three days ago and deliberately struck
only the other one.**

### THE DECISION THIS NEEDS, with evidence and a recommendation

Per PACKET-PROTOCOL rule 4, I stop on this gap and write it up rather than
deciding it myself.

**QUESTION.** Does `TERRAIN.COMPOSED_MATERIAL [0x058B_0000, 0x05AB_0000)` get
built, or get a COMPOSEPUB-shaped refusal recorded in `spec/memory_rules.md`?

**MY RECOMMENDATION: record the refusal, in the spec, in COMPOSEPUB's format —
do not build the writer.** The grounds are already measured and are the
owner's own test:

1. **The consumer test fails the same way.** The composed material triple
   already reaches its real consumer **through fabric** — compose cache → TESS →
   token → shell → mosaic, proven per-triangle by MATCARRY and re-measured here.
   An SDRAM plane would be read by nothing, which is precisely *"a DMA into
   unused memory is not a consumer"* and the ground owner ruling **R64** used to
   retire both mip pools.
2. **The bandwidth finding is decisive and is the directive's own escape**:
   *"a measured engineering impossibility is a finding, not permission to invent
   a pass."*
3. **The precedent is exact and six days old** — the sibling regions, same
   section, same packet format.

**WHY I DID NOT JUST WRITE IT.** The owner re-affirmed this specific region as
live **three days ago**, in the act of striking its neighbour. Converting an
owner-re-affirmed destination into a refusal is not the same kind of act as
recording a refusal he has never ruled on, and the delegation covers
implementation choices rather than retiring a destination he has just looked at
and kept. **It is one short decision record and it should be written with that
sentence in front of whoever writes it.**

**AND IT WOULD NOT CLOSE `I34` ANYWAY.** Obligation (B), directive 13.7's
`-FieldActive`, is independent and is NOT MET — with the composed console's own
`field_composed=0` as the measurement. **Both must land before this entry
closes.**
