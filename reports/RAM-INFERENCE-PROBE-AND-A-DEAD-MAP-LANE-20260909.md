# The RAM-inference probe found a dead map lane, reproduced register merging, and did NOT settle its own question

2026-09-09. A probe built to answer one question about `zhao_raster_perspuv_svc`
answered three other things instead, and honestly did not answer the one it was
for. All of it is recorded because two of the three are more useful than the
original question.

## 1. The map lane had been dead since 2026-08-30

`run_block_map.ps1` compiles **every** `.sv` under `fpga/rtl` — deliberately, per
its own header, because a per-module cone list "buys nothing and costs a
maintenance surface that has already drifted once." So a single unparseable file
fails Analysis & Synthesis for whatever unrelated module is being mapped.

Two files did:

| file | error | added |
|---|---|---|
| `fpga/rtl/field/zhao_field_alu_vec.sv:102` | `Error (10170)` — `for (genvar l = 0; ...)`, then `Error (10112)` ignoring the design unit | 2026-08-30 |
| `fpga/rtl/synth/zhao_probe_v3_exec.sv:348` | `Error (10733)` — `s1_uop_r.op` used sixteen lines before `uop_t s1_uop_r;` | 2026-08-30 |

**Same day. Neither had ever been through `quartus_map`** — zero rows in the fit
ledger, zero in the map ledger. Both lint clean, one with a passing differential
test. `CLAUDE.md`'s rule as an incident rather than a warning: *a block that has
never been through quartus_map has not been shown to be synthesizable, however
clean its lint.*

Both repaired. `alu_vec` takes the established `genvar` hoist plus explicit
`generate`/`endgenerate` (renamed to `gl`, because module scope made `l` shadow
the `int l` in the desync loop — a real `VARHIDDEN` hazard). `v3_exec` moves only
the `assign` below the pipeline declarations; the declaration stays above the
`always_comb` that reads it. No logic changed in either.

### Three things about why it survived

**Manifest exclusion does not exclude a file from the map's source list.**
`zhao_field_alu_vec` is `frozen  FIELD v1` — the differential reference,
instantiated by no production path, correctly carrying no fit target. Every one
of those facts reads as "cannot affect a measurement" and none is true here.
**Frozen means unshipped, not uncompiled.**

**The lesson had already been learned seven times.** `crc32c_fold`,
`field_v3_len`, `field_v3_mulbank`, `field_v3_normalize`, `field_v3_ring_svc`,
`raster_attrdiv_svc` and `raster_toon_div` all carry a comment warning about the
exact parser limitation, and all were fixed. `alu_vec` was written afterwards
without it. That is the third instance in one day of a lesson applied where it
was learned and nowhere else — see also `core.autocrlf` and the §3.4 tripwires.
A comment in seven files is not a check, so `tools/quartus/check_quartus17_syntax.py`
now is one: 3 fire / 6 no-fire self-test cases, 213 files scanned, with a
comment-stripper so the seven files that *describe* the bad form are not reported
as having it.

**My wrapper reported `wrapper_rc=0` on both failed maps.** Quartus's own report
said `Analysis & Synthesis Status ; Failed`. The pipeline's status, not the
build's — `CLAUDE.md`'s trap, and I nearly logged exit 0 as success. Every check
of a Quartus run in this document reads the report, not the shell.

## 2. Register merging, reproduced in a minimal case

The probe's five variants were written identically, and Quartus collapsed four of
them:

```
arr_b[i][b]  Merged with  arr_a[i][b]     224 registers
arr_c[i][b]  Merged with  arr_a[i][b]     224 registers
arr_e[i][b]  Merged with  arr_a[i][b]     224 registers
arr_d                     not merged        0
```

The merged array then carried the **union of their read sites** — two continuous
assigns plus two `always_ff` reads, four read addresses — so it could not be a
dual-port memory whatever its width, read style or reset.

**This is exactly the mechanism found hours earlier in `perspuv`'s `e_mant`,**
where 384 registers of a deliberate per-axis split were merged back because both
copies were written from one source on one clock. The probe reproduces it in
fifteen lines. That is independent corroboration of a finding that had rested on
one map report, and it is why the flaw is recorded rather than quietly fixed.

**And merging is PER BIT.** v2 salted each variant with a distinct XOR constant;
`arr_c` still lost 496 rows, because the salts differed only in bits 0–4 and bits
5–31 remained provably equal. With five variants **no set of constants can differ
pairwise in every bit** — pigeonhole, two values per bit position. A clean version
needs separate write-data ports, not distinct constants.

## 3. What the probe did and did not establish

**Result, both runs:** `arr_d` inferred — 16×32, Simple Dual Port, 512 bits — and
was the only one. Its distinguishing feature is that it is the only array **not
cleared by the async reset**.

**And that does not transfer to the island, because the premise is false there.**
Checking `perspuv`'s reset branch directly:

```
434:  e_val[i]  <= 1'b0;
435:  e_have[i] <= 2'b00;
```

Those are the **only** two arrays cleared on reset, and they are also the two with
multiple write addresses. `e_num_u`, `e_mant_u`, `e_k`, `e_q_u` and `e_tag` are
**not** reset-cleared — each has exactly one write site. So `e_num_u` already has
the property that made `arr_d` infer, and still does not infer.

**The reset hypothesis is therefore ruled out for the island.** That is a real
result: it removes the most obvious candidate, and it removes it by reading the
RTL rather than by another map. `check_ram_inference.py`'s
*"written from an ASYNC-RESET process"* flag is about the **process** carrying a
reset, not about the array being cleared in it — which is precisely why its
header calls the signal weak and says it has measured false positives.

**The discriminator between `e_tag` (inferred) and `e_num_u` (not) remains
OPEN.** Both are 16 deep, single write address at `tail_q`, single read address,
neither reset-cleared. The remaining visible difference is read style —
`e_tag` via a continuous `assign` at a registered index, `e_num_u` inside an
`always_ff` at a combinational index — and the probe cannot speak to that because
its continuous-assign variants were merged.

**I am not going to guess at it.** Three speculations were corrected today: a
stale Mosaic DSP figure, an overstated seed claim, and now this reset hypothesis
— which I had put in the probe as the leading candidate.

## What the clean experiment needs, if it is ever worth running

Two arrays, identical in every respect except read style, each with **its own
write-data port** so neither can be merged, plus one `e_tag`-shaped positive
control with its own port. Three arrays, one factor, no merging. One map, ~80 s.

It is cheap, and the reason to weigh it rather than just do it is that the prize
is bounded: `perspuv`'s token table is 3,376 bits. Converting all of it would
remove on the order of 3,000 registers from a **7,285-register** overage — real,
and not sufficient on its own.

## What this changes

Nothing about sequencing. The map lane is repaired, which matters beyond this
question: it is the cheap instrument for every RAM-inference and DSP-count
question in the repository, and it had been silently unusable for ten days.

The register breach diagnosis stands unchanged: systemic, 9 of 11 components over
budget, largest single component a token table the §3.3 budget never priced.
