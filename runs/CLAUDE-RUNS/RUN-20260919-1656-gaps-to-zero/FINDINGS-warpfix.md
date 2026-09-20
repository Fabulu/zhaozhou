# FINDINGS — WARPFIX (R168, the warp adapter's ordinal-vs-window defect)

**Branch `gz/warpfix`, one commit `7310a59c`. Register 21 before, 21 after.**

> **TRANSCRIBED BY THE COORDINATOR.** The packet's harness blocks a subagent
> from writing report `.md` files — the fourth lane in a row — and it **declined
> to route around an explicit refusal by switching tools**, which is the correct
> call and worth recording as such. Everything below is its reported content.

The register is unchanged **deliberately**. This is a correctness repair to
already-merged code, not a gap closure, and in the packet's own words: *"a
register that fell here would have been the flattering direction and the first
thing to distrust."*

---

## 1. R168 IS WORSE THAN THE RULING SAID: THE HOLE IS REACHABLE WITH LEGAL STIMULUS

R168 was written as a **mis-wiring hazard** — wire the adapter to the old host
and nothing catches it. That understated it.

**In the correctly wired console, the defect is live.** The host answers `StOk`
when the **image's required mask** is satisfied. So an image declaring five of
six ordinals **retires OK with present bit 5 clear** — and a cleared register
sits where `nz'` belongs. An adapter deciding on `resp_status_i == 0` alone
**published that zero as a normal component.**

This is W10's *"an absent output must not look like a zero result"*, shipping.

* **Case D7** drives exactly that.
* **Case D7b is its negative control**: same words, same status, whole mask —
  a case where `nz_o` is *legitimately* zero, **which is precisely why status
  alone could never separate the two.**
* D7 asserts the **correct behaviour**, never the defect (the rule from
  `CLAUDE.md`: do not write a test that asserts the bug).

## 2. THE DISCRIMINATION IS PROVEN BY A RUN THAT FAILED

The new named test's **exact source**, compiled against the wrong-wiring
arrangement — **12 of 22 checks FAILED**:

```
FAIL: A: ORDINAL 5 CAME FROM R21, NOT FROM THE UNWRITTEN WINDOW LANE 5 ...
      expected 0x63, got 0x0
FAIL: B: all six ORDINALS present ...: expected 0x3F, got 0x1F
```

`0x63` is 99 — R21, what the program actually computed. `0x0` is R20, the lane
nothing ever wrote. The same file against the **correct** wiring: **22 checks
passed**, `present=0x3F`, `normal=(77,88,99)`.

**That is the evidence. A passing run proves nothing here** — every existing
Warp test passed against both wirings, which is what made R168 invisible.

The fire test itself was temporary and is **not committed** (target and scratch
source removed, `git status` verified clean). What *is* committed:

| file | what it is |
|---|---|
| `tests/mutants/zhao_field_host_v2_winidx_mutant.sv` | publishes results **window-indexed** — the old host's meaning inside the new host's body |
| `tests/mutants/tb_warp_field_chain_winidx_mutant.sv` | the production bench with **one** substantive substitution |
| `warp_sparse_ordinal_winidx_mutant` | inverted polarity — **passes when the defect is detected**. 11 checks |

### Two mutant design choices worth copying

1. **The completion rule is NOT mutated, so the mutant still answers `StOk`.**
   R168's defect is a *successful* run carrying a wrong number. A mutant that
   answered `StPartial` would be caught by any status check and **prove nothing
   about ordinals** — it would be a positive control for the wrong thing.
2. **The mutant's first case is a CONTIGUOUS negative control.** Under a
   contiguous program the mutant is *indistinguishable* from production
   (`present=0x3F normal=(77,88,99)`). Without it, the sparse verdict could be
   firing on a botched rename or a stale copy rather than on the mutation.

Both drivers build program, vertex, loader words and oracle from **one shared
header**, so "same stimulus" is a property of the code rather than a claim.

## 3. THE PORT CHANGE

Two ports: **`resp_present_i`** (ordinal-indexed) and **`absent_outputs_o`**.
Connected at every instantiation — `zhao_prod_top.sv` (regenerated, 71
instances, no `PINMISSING`, **and the output is read**), `tb_warp_field_chain.sv`,
the mutant bench, and `field_warp_adapter_directed.cpp`. `zhao_console_core.sv`
does not instantiate this adapter, so no core edit was needed.

## 4. IT CHECKED THAT ITS GATES COVERED ITS SUBJECT

Rather than quote a green, the packet verified the gates could *see* its work —
the day's dominant lesson, applied without being asked:

* **`mutant_copy_drift`'s two-line `OK` actually covers the new copy**: paired to
  `zhao_field_host_v2`, 13 substantive diff lines, copy newer than production —
  **one of the 56 checked, not one of the 16 unmatched.**
* **`mutant_drivers.py` credits both new files literally by CMake line**, not by
  a fuzzy `PREFIX` rule that might have matched something else.
* On R172, it patched the signed-literal regex **locally to measure only**, then
  restored it, because the coordinator's branch owns that fix and a second copy
  would conflict. Result: `zhao_field_warp_adapter.sv` **0 silent**;
  `tb_warp_field_chain.sv` **identical before and after** its change.
* **It created no tie-offs**, signed or unsigned, so nothing was owed to the
  core's INCOMPLETE block.

## 5. CORRECTION TO THE COORDINATOR'S BRIEF

**The SPARSE case was already registered in ctest.** `warp_field_chain_directed`
is at `tests/CMakeLists.txt:15803`. My brief said it was "not a name in ctest",
and that was inaccurate.

R168's narrower wording **did** hold: the *case* was not separately named, so a
refactor could have dropped it with every remaining case still passing. But the
stronger claim I made around it was wrong, and the packet said so.

## 6. NOT ACTIONED — A RECOMMENDATION, AND IT IS HALF OF R168 STILL LIVE

**`zhao_field_warp_adapter.OUT_LANES` is misnamed.** It sizes `resp_out_i`,
which is **ordinal**-indexed — so the bench must pass
`.OUT_LANES(W_ORDINALS)` while the host beside it gets
`.OUT_ORDINALS(W_ORDINALS)` **and** `.OUT_LANES(HOST_WINDOW)`.

**Two parameters named `OUT_LANES`, meaning different things, one line apart.**

`tb_warp_field_chain.sv` already calls this *"the sharpest edge in the whole
composition"*. Renaming it to `OUT_ORDINALS` was judged **too risky to land
before the fit** and touches `prod_manifest.yml`, so it is deliberately left.

**It is the same disease R168 was**: two quantities with the same width and
similar names, where nothing in the type system distinguishes correct from
wrong. The repair fixed the instance; the naming that produced it is still there.
