# Composed fit — after the Early-Z skid buffer — 2026-08-31 (round 2)

**A NEGATIVE RESULT, reported as one.** The change is correct, it did exactly
what it was designed to do, and `gpu_clk` went **down**.

    commit    49ad539 (contains ce84b10, the skid)
    device    5CSEBA6U23I7, Cyclone V, provisional — NOT board truth
    tool      Quartus Prime 17.0.2 Lite
    result    PASS analysis/elaboration, synthesis, fitter, TimeQuest

## The numbers

| | round 1 (`a9aeb07`) | round 2 (`49ad539`) | delta |
|---|---|---|---|
| **`gpu_clk` Fmax** | **62.89 MHz** | **60.92 MHz** | **−1.97 (−3.1 %)** |
| worst setup slack | −5.902 ns | −6.416 ns | −0.514 worse |
| failing endpoints | 3,681 | 3,948 | +267 |
| `vid_clk` | 92.74 MHz | 99.52 MHz | +6.78 |
| `audio_clk` | 145.18 MHz | 117.99 MHz | −27.2 |
| hold | positive | positive (0.254 ns) | — |

`audio_clk` and `vid_clk` both moved several MHz in opposite directions with no
change to either. **That is the size of this machine's placement noise**, and it
is the reason a single −3.1 % on `gpu_clk` cannot be read as proof on its own.

## What the change DID do, and it worked

The skid was designed to cut a combinational ready path reaching 256 register
inputs. Round 1's worst 100 were **90 rows ending in
`zhao_raster_earlyz | acc_mask_r[*]`**.

**Early-Z is now completely absent from the worst 100.** The cut succeeded.

## What replaced it — and it is the block that was always underneath

    tilestore | ram1 PORT_B_WRITE_ENABLE_REG
      ->  tilestore | ram0 PORT_B_WRITE_ENABLE_REG      -6.416 ns
      ->  tilestore | rd_byp_data_q[45]                 -6.4xx ns

| destination block | rows in worst 100 |
|---|---|
| `zhao_raster_tilestore` | **70** |
| `zhao_raster_edgewalk` | 30 |

Two things worth naming:

* **`RASTER.TILESTORE` is the common factor in all three fits.** It *launched*
  the critical paths in round 0 (into FRAGMENT) and round 1 (into Early-Z), and
  it is now both source and destination. It was always underneath; two
  consumers were merely worse.
* **EDGEWALK has finally appeared**, at 30 rows — after being ranked FIRST by
  `reports/MHZArchitected` and then being absent from the worst paths of two
  consecutive fits.

## The judgement, and it reverses my first reaction

My first instinct on seeing −3.1 % was to revert. **That is wrong**, and the
path list is why.

Reverting restores 62.89 MHz *and restores Early-Z at −5.902 ns as the ceiling*.
Fixing the tile store afterwards would then hit that ceiling immediately and the
skid would have to be reintroduced. **The skid is prepaid work, not waste.**

The −0.514 ns is best explained as congestion: the skid adds ~336 registers of
payload storage, they place near the tile store, and the tile store's own
internal path is what degraded. That is a hypothesis consistent with the
evidence, **not a measurement** — separating it from placement noise needs a
repeat fit at a different seed, which has not been run.

**KEPT.** With the cost stated rather than hidden.

## What this fit does NOT establish

* **That the skid caused the −1.97 MHz.** One fit is one placement sample, and
  the two untouched clocks moved by +6.8 and −27.2 MHz in the same run.
* **That reverting would restore 62.89 MHz.** Also unmeasured.
* **Anything about the board, or about game capacity.** Still a provisional
  device, still TEST renderer capacities (128 triangles, 1,024 references).

## Next, decided by this report and not by prediction

`RASTER.TILESTORE`. It is 70 of the worst 100, it is the launch point of every
previous round, and its critical path is now internal — RAM write-enable to RAM
write-enable and to the read-bypass register. That is a structure to READ before
changing, because the last two rounds both found the architecture note naming a
structure that was not on the failing paths.

---

## ADDENDUM — the path detail, read rather than inferred

The block-level ranking above says "tilestore, 70 of 100". **Reading the actual
path detail overturns that reading**, and it is the most informative measurement
of the session.

The path is **not tilestore-internal**. It is the read-modify-write loop:

    ram1 portbdataout          (RAM read data out)        7.856 -> 8.009
      -> tilestore | rd_data_o[41]                        +0.782
      -> fragment | u_bb | Add0~5                         +0.842   INTO THE BLEND
      -> mul_left[2] -> Mult0~mac        (DSP multiply)   +3.785   <-- largest
      -> Add2~37 ... long carry chain                     ~+2.0
      -> ram0 PORT_B_WRITE_ENABLE_REG

    launch clock path    7.856 ns
    latch  clock path    5.861 ns
    skew                -1.995 ns   (destination clocked EARLIER)
    data path           14.361 ns
    data required       15.801 ns
    slack               -6.416 ns

So the tile store is the **endpoint of a loop**, not the offender. `RAM read ->
blend -> multiply -> adder chain -> RAM write` happens **in one cycle**, and the
single largest element is `zhao_raster_blend`'s DSP multiply at 3.785 ns.

### What this means for the next surgery, quantitatively

Arrival must fall from 22.217 to 15.801. The clock path contributes 7.856 of
that arrival, so **the data path must go from 14.361 ns to under 7.95 ns — it
must be roughly HALVED.**

That is not a tweak. It is a pipeline split of the read-modify-write loop, which
is exactly `reports/MHZArchitected` step 4:

> Fragment -> read/shade/blend/finish/commit, with a small in-flight address CAM

**The note's prescribed SURGERY was right all along; its RANKING was wrong three
times.** It put EDGEWALK first (absent from two fits, 30 rows in the third) and
Early-Z third (90 rows in round 1). Read the path; trust the note's remedies,
not its order.

The address CAM is not optional bookkeeping — it is the whole difficulty. Once
the loop spans two cycles, a second fragment targeting the same tile address may
enter before the first has written back, and the CAM is what forwards or stalls
it. `raster_fragment_random` already exercises 3,232 same-pixel chains, so the
hazard is real traffic, not a theoretical case.

### And a second, independent problem this exposes

`gpu_clk~CLKENA0` drives **13,682 fanout**, and the launch and latch clock paths
differ by 1.995 ns. **No amount of datapath pipelining recovers skew.** Even a
perfectly split RMW loop leaves ~2 ns of the budget spent before any logic runs.

That is a clocking problem — a clock-enable network, not a block — and it is not
on `MHZArchitected`'s list of five offenders at all. It should be measured
separately before anyone assumes the remaining gap to 100 MHz is all datapath.
