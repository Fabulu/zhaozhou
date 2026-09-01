# Composed fit — skid removed — 2026-09-01 (round 5)

**80.3 MHz**, and a decision made on reasoning in round 2 is now settled by
measurement.

    commit 7a7265a    device 5CSEBA6U23I7 (provisional, NOT board truth)

## Six rounds

| | r0 | r1 | r2 | r3 | r4 | **r5** |
|---|---|---|---|---|---|---|
| change | — | modulation off cone | +skid | RMW split ×3 | registered steps | **−skid** |
| **`gpu_clk`** | 53.48 | 62.89 | 60.92 | 64.66 | 79.22 | **80.30** |
| worst setup | −8.697 | −5.902 | −6.416 | −5.466 | −2.623 | **−2.453** |
| endpoints | — | 3,681 | 3,948 | 1,673 | 984 | 1,314 |
| ALMs | 12,569 | 12,532 | — | 12,755 | 12,794 | **12,658** |
| DSPs | 16 | 16 | 16 | 16 | 16 | **16** |

**+50 % overall. 72 % of the original violation closed** (−8.697 → −2.453 ns).

## The skid verdict: it was pure cost, and I kept it two rounds too long

Removing it **gained 1.08 MHz and returned 257 registers and 136 ALMs**.

In round 2 the skid cost 2 MHz and I kept it, arguing it was "prepaid work"
because Early-Z would otherwise be the ceiling. That argument was plausible and
**unverified**, and it stayed unverified for two rounds because I never made it
checkable. It stopped being true at round 3, when the RMW split shortened the
same ready chain independently.

**The lesson is not "the skid was wrong."** It is that a change kept on an
argument needs a scheduled re-test, and the re-test only became cheap once
round 4 changed the surrounding conditions. A decision defended rather than
measured will stay wrong quietly.

`zhao_skid2.sv` remains in the tree — proven, documented, unused. The next block
needing a ready-path cut should not rewrite it.

## Read the endpoint count carefully

**984 → 1,314 while the worst path improved.** That is not a regression: the
slack profile is flattening as the design approaches closure — fewer very bad
paths, more paths clustered just behind the worst. Quoted alone it would look
like one.

## The remaining offender

    tilestore | ram0 PORT_B_WRITE_ENABLE_REG  ->  earlyz | acc_mask_r[*]

| block | rows in worst 100 |
|---|---|
| `zhao_raster_earlyz` | **94** |
| `zhao_raster_resolve` | 6 |

This is the `floor_r`/`acc_mask_r` cone — `MHZArchitected`'s offender 3, the
"256-bit global feedback cone", now unambiguously last in the queue. Its own
prescription is full-detection restructuring so the mask is not a single global
reduction feeding a large register bank.

## Remaining gap: 2.453 ns

Still open, and still not established as pure datapath:
`gpu_clk~CLKENA0` drives ~14,000 endpoints, and the launch/latch skew measured in
round 2 was 1.995 ns — **the same order as the entire remaining violation.**
Before the next RTL surgery, the clock network deserves one measurement of its
own.

---

## ADDENDUM — the remaining 2.453 ns is 82 % SKEW, and the skew is M10K silicon

Measured on this fit's worst path, not inferred:

    Data Arrival    18.355
    Data Required   15.902
    Slack           -2.453
      Clock Skew    -2.018      <-- 82 % of the violation
      Data Delay    10.375      <-- only 0.375 ns over a 10 ns period

**The logic is essentially finished.** Another block surgery on Early-Z would
attack the 0.375, not the 2.018.

### Where the skew comes from — and it is not the clock tree

| | launch (M10K write-enable reg) | latch (ordinary FF) |
|---|---|---|
| `gpu_clk` → CLKCTRL | 2.898 | 0.724 |
| CLKCTRL cell | 0.298 | 0.272 |
| route to the register | 2.321 | 1.905 |
| **register's internal cell** | **2.463** | **0.495** |
| **total** | **7.980** | **5.962** |

The dominant term is **2.463 vs 0.495**: a RAM's write-enable register sits about
2 ns deeper inside the M10K than a fabric flip-flop does. **That is silicon.** No
floorplan, clock constraint or seed changes it.

The 2.898-vs-0.724 difference into CLKCTRL looks alarming and is not — both
paths cross the same buffer, and Quartus already credits **2.566 ns** back as
common-path pessimism.

### What this means for the last item

The remaining violation is **structural**: it is the cost of any path that
launches at an M10K output and lands in fabric inside one cycle.

So Early-Z is **not** a "the 256-bit reduction is too wide" problem, which is how
`MHZArchitected` frames offender 3. It is a **"this path launches from a RAM
write-enable"** problem. The remedy is a register between the tile-store output
and the mask update — the same shape of fix as the FRAGMENT RMW split, which is
what removed this structure from the fragment path in round 3.

**This also revises an earlier claim in this file, and in round 2's:** "no
datapath work recovers skew" is wrong as stated. Datapath work that removes a
RAM-launched path recovers it, because the skew is a property of *that
structure*, not of the clock distribution. Round 3 already demonstrated this and
it was not recognised at the time.

---

## A failed attempt, recorded because the failure is the useful part

The round-5 report above named paths 2 and 3 as
`fragment | s1_addr_r -> earlyz | acc_mask_r` at −2.248 ns. That launch point is
the **same-address hazard comparator added in the RMW split** — my own addition,
placed directly on the ready chain feeding 256 mask bits.

**Two attempts to register it off that path both failed, and it is reverted.**

### Attempt 1 — wrong address

Registered `hazard` computed against `s0_addr_r`. I argued it was safe because
later stages only ever drain, so a stale verdict can only over-stall.

That reasoning was **correct and irrelevant**. The bug was not staleness: on the
cycle a NEW fragment loaded into s0, the registered verdict belonged to the
PREVIOUS occupant. A verdict about the wrong fragment, not a conservative
verdict about the right one. I proved safety for the case I was thinking about
and never examined the load cycle.

8 of 97 directed, 1 of 9 random, 1 of 74 tile-pipe.

### Attempt 2 — right address, still wrong

Compared against the incoming address at the accept edge and refreshed against
`s0_addr_r` otherwise. That fixed the tile-pipe cases (74 and 12 green) and
**still failed 8 of 97 in the fragment's own tests.**

### Reverted, and why that is the right call rather than persistence

* it is **not the worst path** — path 1 is `tilestore -> RESOLVE` at −2.453,
  these are −2.248, so the whole prize is ~0.2 ns;
* it is **my own regression**, so reverting restores a state that is measured,
  not hoped for;
* two failed attempts on a low-value change is the point to stop, not the point
  to try a third variation.

The combinational hazard stays. It costs ~0.2 ns on paths that are not
currently binding.

### The lesson, which is worth more than the 0.2 ns

**"This cannot happen in the real system" is an argument about the CALLER, and
unit tests are deliberately not the caller.**

I justified the hazard check as unreachable — true, `RASTER.TILE_PIPE` drains
between triangles. I then used that same fact to reason loosely about
registering it. But `raster_fragment_random` drives the block **directly** and
reports **3,232 same-pixel chains**: in the unit tests the hazard fires
constantly. The gate I had twice described as unrepresentative of production
traffic is the only thing that exercises this logic at all — and it caught both
attempts immediately.
