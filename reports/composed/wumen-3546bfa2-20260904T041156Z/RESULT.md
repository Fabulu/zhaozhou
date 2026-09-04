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

### Traced, and the tail is now FLAT

    -0.198  tile_pipe | rs_state.RS_CLEAR      -> tilestore | res_pres_q
    -0.133  debug_frameblit | Equal1~3         -> mem_guard | fwd_req.len[0..5]   x6
    -0.132  geom_binner | ep_r[2][3]           -> geom_binner | epr_r[0][22]      x4
    -0.10x  cmd_dma                                                              x1

**Four unrelated structures, every one inside 0.2 ns.** There is no dominant
offender left. Ten rounds of this effort each began with one structure owning
most of the worst 100; this one begins with the deepest violation being 0.065 ns
worse than the shallowest.

That changes what the next step should be. A single surgery buys at most the
gap between the worst path and the second — here about 0.07 ns — so closing the
last 0.198 ns needs **all four** touched, or the target reconsidered.

### The worst path is ANOTHER cross-block ready chain

Traced, and it is structurally the same defect the skid just fixed:

    tile_pipe  rs_state == RS_CLEAR
      -> ts_clear -> tilestore.clear_valid_i
      -> tilestore.wr_ready_o = !clear_valid_i        <-- the escape
      -> ts_wr_ready -> fragment.wr_ready_i
      -> fragment's s3/s2/s1 retire chain
      -> back into the pipe -> res_pres_q enable      -0.198 ns

`zhao_raster_tilestore.sv:150` is the single line that creates it:

    assign wr_ready_o = !clear_valid_i;

The block's own comment three lines above says *"Nothing here reads a downstream
ready, so there is no valid<-ready path"*, and that is true **within** the
block. But `wr_ready_o` depends on `clear_valid_i`, which is an INPUT — so a
ready depends on another channel's valid, and because both come from the same
upstream state machine the comparator lands in Fragment's retire chain.

**A valid→ready dependency inside one block becomes a cross-block combinational
path once two of its channels share an upstream.** That is the same shape as the
Early-Z chain and it is the reason this one was invisible until Early-Z was
fixed.

The clear and the write are genuinely mutually exclusive — both target the front
bank — so the dependency is not gratuitous. Breaking it means either registering
the clear request or giving the write its own acceptance, and that is a
one-change, one-fit step like the skid was.

### One of them should not be there at all

Six of the twelve violations run from `zhao_debug_frameblit`'s state comparator
into `zhao_mem_guard`'s forwarded request length. **A DEBUG block is on the
console's critical path.** `hps_req_o.valid = (state == B_READ_REQUEST)` gates
the guard's accept, and the guard captures `fwd_req.len` behind it.

Whether a debug blitter should be allowed to constrain production timing is a
scope question rather than a timing one, and it is the cheapest of the four to
answer: if that path can be registered, or the block excluded from the
production top, six of the twelve violations go with it.
