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

### The worst path, traced from the NODES this time -- and the first trace was wrong

    rs_state.RS_CLEAR -> ts_clear                     1.4 ns   ready chain
      -> u_fragment|s1_retire
      -> u_tilestore|ram1.raddr_a[3]~3                         the READ ADDRESS
      -> Mux1~15 -> Mux1~19 -> Mux1~20 -> Mux1~84     6.3 ns   <-- THE COST
      -> res_pres_eff~0 -> res_pres_q                -0.198 ns

**This file first said the path ran through `wr_ready_o = !clear_valid_i` into
Fragment's retire chain and back into a register enable, and recommended
registering the clear.** That was reasoned from module names and it was wrong in
the part that decides the fix. `res_pres_eff` does not depend on `clear_acc` at
all -- the code says so and a comment above it says so. Reading
`characterization/setup_paths.rpt` node by node shows the truth:

* the ready chain is real but contributes **1.4 of the 9.58 ns**;
* **6.3 ns are the `present[...]` 256:1 multiplexer** -- four LUT levels and
  **4.4 ns of pure routing**;
* the ready chain reaches that mux through its **ADDRESS**, not its enable.

Which is a different defect with a different, cheaper fix.

### And the address it arrives on is one the port cannot supply

`b0_raddr`/`b1_raddr` mux *both* port addresses, because one memory port serves
both roles. But `present1[b1_raddr]` is only ever evaluated under `front_r == 0`,
where `b1_raddr` **is** `res_addr_i`. The bank mux was therefore carrying
`rd_addr_i` into a lookup that can never select it, and Quartus fed the present
mux from the RAM's shared address node -- so Fragment's retire chain reached
`res_pres_q` down a path that is unreachable in the design's own semantics.

Substituting the provably-selected address:

    res_pres_eff = (front_r == 1'b0) ? present1[res_addr_i] : present0[res_addr_i];
    rd_pres_eff  = ... ? present0[rd_addr_i] : present1[rd_addr_i];

**Exactly equivalent by case analysis**, and it removes the port from the cone.
The RAM reads keep the bank addresses, because those genuinely are dual-role.

**This is the third time this effort that reading the paths beat reasoning about
the blocks**, and the first two were also recorded here. The rule earned another
line: *a trace is not traced until it names nodes.*

### The binner's four: an accept decision inside its own advance

Traced from the nodes, and **this one was wrong in the file too** -- it said a
multiply-add between two registers, reasoned from lines 755-757. The report:

    ep_r[2][3]
      -> Add2~69                                    emax[2] = ep_r[2] + off_r[2]
      -> g_edge[2].u_fill|accept_o~5 -> ~6 -> ~7    zhao_raster_fill's predicate
      -> Equal0~2                                   tile_keep = (accept == 3'b111)
      -> ty_r~0                    fanout 109
      -> epr_r[0][22]|sload                         the LOAD ENABLE  -0.132 ns

Six logic levels, no multiplier on the path at all. The structure is

    the accumulator decides ACCEPTANCE,
    acceptance decides ADVANCEMENT,
    advancement updates the ACCUMULATOR

-- all in one cycle. And the cross-lane naming that the previous note flagged as
"most likely a synthesis artefact" is **not an artefact**: `tile_keep` is a
three-lane AND, so lane 2's accept genuinely gates every lane's `epr_r` load.
The caution was right; the guess behind it was not.

**The fix costs more than 0.132 ns is worth today.** Preserving the initiation
rate means speculating the next tile's `emax` for both advance directions -- six
extra adders and six extra `fill` predicates -- so that `tile_keep` is registered
when the cursor moves. Simply registering the accept would cost a cycle per tile,
which the architecture rule forbids. That is a real rework of a verified block,
and round 2 of this effort is the standing reminder that an unmeasured
restructure can cost 2 MHz. It waits for a fit that shows where the tail sits
after the tilestore change.

### Three traces, three corrections, one rule

Every one of the four offenders was first diagnosed from module names and RTL
reading, and **three of the four diagnoses were wrong in the part that picks the
fix**:

| offender | first diagnosis | what the nodes said |
|---|---|---|
| tilestore | ready chain into a register enable | a 256:1 mux fed by an unreachable address |
| binner | multiply-add between registers | an accept predicate inside its own advance |
| binner lanes | "probably a synthesis artefact" | a real three-lane AND |
| frameblit | debug block on the critical path | **correct** -- a wide lease equality |

The one that survived is the one whose claim was about *scope* rather than about
structure. **A trace is not traced until it names nodes**, and
`characterization/setup_paths.rpt` has had those nodes all along.

### One of them should not be there at all

Six of the twelve violations run from `zhao_debug_frameblit`'s state comparator
into `zhao_mem_guard`'s forwarded request length. **A DEBUG block is on the
console's critical path.** `hps_req_o.valid = (state == B_READ_REQUEST)` gates
the guard's accept, and the guard captures `fwd_req.len` behind it.

Whether a debug blitter should be allowed to constrain production timing is a
scope question rather than a timing one, and it is the cheapest of the four to
answer: if that path can be registered, or the block excluded from the
production top, six of the twelve violations go with it.
