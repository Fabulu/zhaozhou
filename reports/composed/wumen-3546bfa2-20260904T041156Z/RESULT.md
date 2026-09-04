# Composed fit — the Early-Z skid, restored and measured — 2026-09-04

**98.06 MHz. The skid paid, and Early-Z is gone from the violations entirely.**

    commit 3546bfa2    device 5CSEBA6U23I7 (provisional, NOT board truth)
    5,313 s wall, fitter 1:11:58 (previous run 1:30:42)

| | r9 `b3bd69b` | r10 `18054414` | **r11, this run** |
|---|---|---|---|
| **`gpu_clk`** | 85.62 | 97.28 | **98.06 MHz** |
| worst setup | −1.679 | −0.280 | **−0.198 ns** |
| **failing endpoints** | 430 | 18 | **12** |
| ALMs | 12,707 | 13,031 | 13,301 |
| DSPs | 16 | 16 | **16** |

## The change did exactly what it was reasoned to do

The previous fit put **17 of 18** violations inside `zhao_raster_earlyz`, on a
ready path travelling backwards from `fragment.s3_addr_r` through the hazard
comparator into the coverage-mask write enable. `zhao_skid2` was reinstated
between Early-Z and Fragment to break that chain.

**Early-Z now appears in none of the violated paths:**

| block | violated paths |
|---|---|
| `zhao_mem_guard` | 6 |
| `zhao_geom_binner` | 4 |
| `zhao_raster_tilestore` | 1 |
| `zhao_cmd_dma` | 1 |
| `zhao_raster_earlyz` | **0** |

The worst path is now a different structure altogether:

    from  zhao_raster_tile_pipe | rs_state.RS_CLEAR~DUPLICATE
    to    zhao_raster_tilestore | res_pres_q            −0.198 ns

## Why this went the other way from round 2

`ce84b107` added this same skid at round 2 and `gpu_clk` FELL, 62.89 → 60.92.
`7a7265a6` removed it at round 4 on a correct measurement: the RMW split had
shortened the chain and it was "still being paid for and no longer buying
anything".

**Nothing regressed to bring it back.** Rounds 5 through 9 and the four EDGEWALK
commits made everything else faster, so a chain that was comfortably slack at
79 MHz became the longest one at 97. The skid costs the same +270 ALMs it always
did; what changed is that it is now buying something.

Both decisions were measurements. A fix that stops paying gets removed on
evidence and earns its place back on later evidence — and the second decision
does not make the first one wrong.

## What is NOT claimed

* **Provisional.** `5CSEBA6U23I7` is a capacity/timing target, not board truth;
  all harness I/O is virtual.
* **This is the shell WITHOUT the geometry front end** (docket D22). Honest for
  what it measured; not the finished console's number.
* 12 endpoints still fail. `timingPassed` is **false**.

## Next, and the offender list is new

`zhao_mem_guard` leads with 6 and has not appeared in any previous round's worst
paths. `zhao_geom_binner` follows with 4. Neither is on `MHZArchitected`'s list
at all — which is the fourth time this effort the report has named a structure
the note did not predict. **Read the paths, not the note.**
