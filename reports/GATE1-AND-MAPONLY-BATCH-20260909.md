# Gate 1, the §D geometry, and lever 4 — with two mistakes of mine

2026-09-09. Results from the cheapest-first batch (`tools/quartus/queue_all.ps1`),
run after two external stops cost a 195-minute island fit, a 174-minute expander
refit, and a 102-minute svc fit killed at ~98% done.

---

## FIT GATE 1: the migration laboratory costs 4,432 registers

Controlled pair, **same bytes** — both rows carry digest `e3b84521e337…` despite
different `sourceCommit`s, because the intervening commits touched only a YAML
and a test file. The digest is the authority, not the commit; the runner's own
header says so.

| | `@g1-lab` (SHADOWS=1) | `@g1-prod` (SHADOWS=0) | delta |
|---|---|---|---|
| registers | 21,065 | **16,633** | **−4,432** |
| blockMemoryBits | 64,762 | 64,122 | −640 |
| DSP | 17 | 17 | **0** |
| virtualPins | 1,840 | 1,840 | 0 |
| map seconds | 653 | 480 | −173 |

**The laboratory is 21% of the lab build's registers and zero DSP.** It does
nothing for the DSP budget.

The attribution is unusually clean:

```
sampmeta_m  64 x 3 x 21                 4032
mj_ref_q 21 + pslot 2 + pgen 8 + v 1      32
seven counters x 32                      224
first-error captures 21 + 21 + 18         60
                              predicted 4348
                               MEASURED 4432   (98.1% accounted for)
```

And `blockMemoryBits` barely moving is the load-bearing detail: **`sampmeta_m`
was never in block RAM.** 4,032 bits of flip-flops. That observation is what
exposed the checker defect below.

---

## §D's memory geometry: confirmed, and my early_desc fix verified

| block | blockMemoryBits | registers | DSP |
|---|---|---|---|
| `zhao_texture_metajoin` | **9,984** | 137 | 0 |
| `zhao_texture_early_desc` | **8,128** | **106** | 0 |
| `zhao_texture_uv_join` | 0 | 211 | 0 |

* **metajoin's 256×40 does fit one block.** 9,984 bits ≤ 10,240, so §D's *"the
  metadata bank's 256 x 40 shape is compatible with one block; that still needs
  confirmation in the actual fit"* is confirmed at the map stage. Not at the
  fitter stage — a MapOnly reports bits, not block counts, and that distinction
  is the correction recorded in `design/fit_targets.yml`.
* **early_desc's 106 registers are the proof that the fix worked.** Before it,
  `mem_q [NSLICE][ROWS]` was a multidimensional unpacked array, which Quartus
  cannot regroup into memory — it would have been ~7,616 bits of flip-flops and
  0 memory bits. 8,128 bits of block memory and 106 registers is the opposite
  shape. Had the fix not landed first, this MapOnly would have reported 0 memory
  bits and read as a puzzle.
* **uv_join is 0 memory bits**, matching its declared intent exactly: pure
  control and a held record.

---

## Lever 4: my hypothesis is REFUTED, and the manifest was already right

I wrote that `zhao_texture_combine`'s 8 DSP were probably the missing
`(* multstyle = "logic" *)` that its sibling carries. The two rows say otherwise:

| | ALM | Fmax | registers | DSP | multstyle |
|---|---|---|---|---|---|
| `zhao_texture_combine` | **494** | **100.12** | 524 | **8** | no |
| `zhao_texture_material_combine_v1` | **1663** | **69.75** | 1269 | **2** | yes |

These are **not one block with a missing pragma.** A 3.4× ALM difference and a
30 MHz gap are two different designs. `combine` is small and fast and spends
DSPs; `v1` is large and slow and spends logic. Adding the attribute to `combine`
would push it *toward* v1's ALM count, not toward a free saving.

So the honest reading is the reverse of my framing: the 8 DSP are **bought**, not
wasted, and lever 4 is off the list. `prod_manifest.yml` line 69 already said
`REFUTED (D19q); delete when v1 is measured` — and it was right before I
re-opened it.

**v1 is now measured, which is what that sentence was waiting for**, and the
trade it implies is worth stating rather than deciding: deleting `combine` in
favour of `v1` saves **6 DSP** and costs **+1,169 ALM and −30 MHz**. Both rows
are `failed:structure` against their own budgets.

---

## MISTAKE 1: an unlabelled MapOnly DESTROYS a full-fit row

`zhao_texture_combine` had a full-fit row — ALM 494, Fmax 100.12, 524 registers.
My batch ran `run_block_fit -MapOnly` with **no `-RowLabel`**, so the map_only
row replaced it under the same module name, discarding the ALM and Fmax a
1,640-second fit had produced.

Recovered from git, because the receipt is version controlled — and that is luck
in the design's favour, not foresight in mine.

**The rule this earns: a MapOnly must always carry a `-RowLabel`.** A map row and
a fit row answer different questions and are not substitutes; letting the cheaper
one take the expensive one's name is a silent downgrade. `@v3-before` and
`@v3-nctx8` in the existing ledger do exactly this correctly, which is how the
convention should have been obvious.

`material_combine_v1`'s MapOnly was still running when this was noticed, so its
full-fit row (ALM 1663 / 69.75 MHz / 1269 registers / 2 DSP, digest
`db2564ed3c28`, same bytes) is banked here before it is overwritten too.

## MISTAKE 2, already fixed: the checker could not see a nested bracket

Gate 1 said `sampmeta_m` was flip-flops. `check_ram_inference.py` said nothing
about it. That disagreement was the clue: its write scan used
`(?:\[[^\]]*\]\s*)+`, which cannot match `sampmeta_m[src[HI:LO]][src[LO+1:LO]]`,
and `if not writes: continue` then dropped the array from **every** rule.

**53 arrays, 296,848 declared bits — about 29 M10K-equivalents — were invisible**,
including three at 65,536 bits each. Fixed with a balanced-bracket scanner and a
`self_fire_test` that proves the scan sees both nested and flat writes before it
may report anything.

The two findings corroborate each other: the measured register delta and the
static prediction now agree, and neither alone would have been enough.
