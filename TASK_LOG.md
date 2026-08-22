# Task log

Engineering-side record: what was run, against which commit, and what the
evidence actually was. `STATUS.md` is the owner-facing channel and stays prose;
this file is the audit trail and stays exact.

Newest entry at the top. Every claim here names the command that produced it.
**Everything in this file is simulation, synthesis or fit. No hardware has run
any of it.**

---

## 2026-08-22 -- THE FIT IS DETERMINISTIC. Every A/B this session was signal.

I had been calling the later results "placement variation" without ever having
measured whether this flow varies at all. That was an assumption doing load-
bearing work, so I tested it: re-ran the composed fit on RTL byte-identical to
`ae3ce73` (confirmed with `git diff ae3ce73 HEAD -- fpga/rtl/`, empty).

| | `ae3ce73` | `e6b5fef`, same RTL |
| --- | ---: | ---: |
| setup worst | -0.639 ns | **-0.639 ns** |
| failing endpoints | 125 | **125** |
| hold worst | +0.250 ns | **+0.250 ns** |
| ALMs | 7,648 | **7,648** |

**Identical to the digit.** Quartus's placement is seeded deterministically
here, so two fits of the same source give the same answer.

### What that changes

Every A/B comparison in this session was real signal:

* the header-ladder split really did make the design worse (-1.096 -> -1.472),
  not noise;
* un-sharing the folds really did make it worse (-0.639 -> -1.414), not noise;
* and the six improvements really were improvements.

I had hedged both regressions as "placement variation". They were not variation
-- they were consequences. The reasoning I gave for each still holds (the
fitter's effort moved elsewhere; the extra area cost more than the removed mux
saved), but "variation" was the wrong word and it let me off too lightly.

### What it does NOT change

The design is still placement-BOUND below ~1.5 ns: small structural changes
move the number unpredictably in sign, even though each individual measurement
is repeatable. Deterministic is not the same as insensitive.

### The standing state

    setup   -0.639 ns   125 failing endpoints   1,995 negative-slack paths
    hold    +0.250 ns     0 failing
    ALMs    7,648 of 41,910

Worst path 10.64 ns against a 10 ns budget: 6.4% over. Not closed.

The largest remaining family is 1,383 paths at -0.195 ns from
`scanout|fetch -> mem_guard|guard_violations` -- the guard's range check. I
have deliberately NOT touched it. It is 2% over, it is the one block whose
entire contract is that no memory escape exists, its no-escape proof depends on
the response timing, and the last two structural changes I made both made the
headline worse. That trade is bad at this margin and it is Fabian's call, not
one to make quietly at the end of a long session.

---

## 2026-08-22 -- un-sharing the folds made it WORSE, and is reverted

The census after the -0.639 ns run named the worst path precisely:

    -0.639 ns  hps_arbiter|held_req.client[0] -> cmd_dma|crc_pay_r[28]
    -0.630 ns  hps_arbiter|held_req.client[0] -> cmd_dma|crc_hdr_r[28]

The arbiter's routing decision reaching the CRC through the state-selected mux
on the fold's DATA input -- the consequence of sharing one fold pair between
the header/seed walk and the streaming path. The review Fabian relayed had
warned against exactly that over-sharing, and I had done it anyway.

So I split them: a walk pair on `hw_word`, a streaming pair taking
`hps_rsp_i.data` directly with nothing in between.

**Measured, and it is worse:**

| | shared (`ae3ce73`) | un-shared (`9ac7881`) |
| --- | ---: | ---: |
| setup worst | **-0.639 ns** | -1.414 ns |
| failing endpoints | **125** | 424 |
| ALMs | **7,648** | 8,023 |

Reverted in `692434f`.

### Why this is not a contradiction of the advice

The reasoning was right: a mux in the data path of the thing being shortened is
a real cost, and removing it did shorten that path. But two extra fold
instances are +375 ALMs, and at this margin the fitter's placement dominates.
The design is 3.4x worse by endpoint count for a structurally cleaner circuit.

**That is the second change this session that was well-motivated, did what it
was designed to do, and made the headline worse.** The first was the header
ladder split. Both are the same lesson: below about 1.5 ns, this design's
timing is placement-bound, not structure-bound, and a change that removes real
work can still lose.

The difference in what I did with them: the ladder split was KEPT, because its
own family improved and the degradation was in families it never touched. This
one is REVERTED, because its own cost -- the area -- is what caused the
regression, and the best measured state is the shared one.

### The standing state is `ae3ce73`'s numbers

    setup   -0.639 ns   125 failing endpoints   1,995 negative-slack paths
    hold    +0.250 ns     0 failing
    ALMs    7,648

Worst path 10.64 ns against a 10 ns budget. Not closed.

---

## 2026-08-22 -- MEASURED: -0.639 ns. Best of the campaign, and the census is
## down from 13,651 paths to 1,995.

Composed fit at clean HEAD `ae3ce73`.

| | start | best before | now |
| --- | ---: | ---: | ---: |
| setup worst | -56.374 ns | -1.096 ns | **-0.639 ns** |
| failing endpoints | 3,746 | 172 | **125** |
| negative-slack paths | -- | 13,651 | **1,995** |
| hold | -1.064, 3 failing | +0.251, 0 | **+0.250, 0** |
| ALMs | 9,181 | 7,713 | **7,648** |

**Worst path 65 ns -> 10.64 ns against a 10 ns budget: a 6.5x overrun is now
6.4%.** And the design is ~1,530 ALMs smaller than when the campaign started.

### The remaining census, in full

    1,383  scanout|fetch  -> guard_scan|guard_violations   -0.195
      144  cmd_dma|fetched -> cmd_dma|burst_end            -0.338
       25  cmd_dma|pkt_len_r -> cmd_dma|stream_w           -0.298
        9  cmd_dma|m.M_SEED  -> cmd_dma|crc_hdr_r          -0.471
        8  cmd_dma|need_total -> cmd_dma|burst_end         -0.032
        6  cmd_dma|m.M_SEED  -> cmd_dma|crc_pay_r          -0.480

The `cb -> seed_steps_q` family that was 18,013 paths at -1.324 is gone. Two
thirds of what is left is one scanout-to-guard family at -0.195, which is
within noise of the target.

### The whole campaign, five fixes

| fix | what it was | effect |
| --- | --- | ---: |
| parallel CRC fold | 8 dependent XOR levels per byte, 64 deep per beat | families eliminated |
| FRAMEBLIT rewire | the same chain in a second block | -28 ns family gone |
| CMD.DMA CRC walks | 224 XOR levels behind a command_bytes-derived bound | -55 -> -3 ns |
| r_len pipelining | a subtract fanning out four ways in the cycle used | -3.067 -> -1.096 |
| record-walk split | RAM output -> opcode lookup -> ladder in one cycle | family gone |
| seed as a 3-way decision | an add, min, subtract and two compares for a value that is 0, 16 or 28 | -1.324 -> -0.639 |

Every one was accidental combinational depth. None was evidence the
architecture is too slow, and the design got SMALLER at every step.

### The lesson that generalises

The last fix is the one worth remembering. I had PROVEN the seed length can
only be 0, 16 or 28, used that proof to justify constant fold widths, written
it in a comment -- and then gone on computing the value the long way. The proof
was in the file and the arithmetic ignored it. 18,013 of 20,000 negative-slack
paths ran through that gap.

**A law you have proven is worth nothing until the logic is written in its
terms.** It is now, and the law itself is asserted:

    a_seed_bytes_lawful    seed_bytes_q in {0, 16, 28} when m == M_SEED
    a_payload_end_aligned  payload_end_q[2:0] == 3'b100

### Not closed

125 endpoints still fail. Timing closure means zero.

---

## 2026-08-22 -- THE LADDER SPLIT WORKED AND THE DESIGN GOT WORSE. Both are true.

| | before (`a0ef3ec`) | after (`14e5a21`) |
| --- | ---: | ---: |
| setup worst | -1.096 ns | **-1.472 ns** |
| failing endpoints | 172 | **355** |
| hold | +0.251, 0 failing | **-1.366, 1 failing** |
| ALMs | 7,713 | 7,671 |

**The headline moved the wrong way.** Recorded as such rather than buried.

### The census says the change did what it was for

Full negative-slack census both sides, via `quartus_sta` against the preserved
fit:

| family | before | after |
| --- | ---: | ---: |
| `hdr_win -> seed_steps_q` | 6,973 @ -1.096 | -- gone -- |
| `cb -> seed_steps_q` | -- | 5,619 @ **-0.854** |
| `slot_ram -> done_status` | 81 @ -0.466 | 623 @ **-1.472** |
| `slot_ram -> walk_off` | @ -0.838 | @ **-1.155** |
| `slot_ram -> walk_cnt` | @ -0.564 | @ **-1.082** |
| `scanout\|fetch -> guard_scan` | 31 @ -0.019 | **3,862** @ -0.468 |

The targeted family improved by 0.24 ns and shrank by 1,354 paths. It is no
longer the worst. Every family that overtook it is one the change did not
touch, and one of them -- the scanout-to-guard path -- went from 31 paths at
essentially zero to 3,862.

### What that actually means, and it is the useful finding

**At this margin, fixing one family no longer monotonically improves the
design.** The fitter had been spending its effort holding the header ladder
together; relieved of that, it spent it elsewhere and three other families
drifted past. Nothing regressed structurally -- the RTL those families run
through is unchanged.

The first three fixes were 6.5x, 4x and 3x overruns: real structural depth,
where any improvement had to show. Everything left is within 1.5 ns of the
target, which is placement territory. The remaining work is broad-front, not
another hotspot hunt.

### The change is kept

It removes real work from a cycle that already does seven checks, and its own
family improved. Reverting because an untouched family drifted would be
choosing the number over the reasoning. But it is NOT an improvement to the
design and is not recorded as one.

### The honest state

    setup   -1.472 ns   355 failing endpoints   13,585 negative-slack paths
    hold    -1.366 ns     1 failing

Timing is not closed and the last three measurements have been -3.067, -1.096,
-1.472. The trend is no longer monotone.

---

## 2026-08-22 -- MEASURED: -1.096 ns. The campaign in three steps.

Composed fit at clean HEAD `a0ef3ec`.

| | start | after CRC folds | after r_len pipelining |
| --- | ---: | ---: | ---: |
| setup worst | -56.374 ns | -3.067 ns | **-1.096 ns** |
| failing endpoints | 3,746 | 584 | **172** |
| hold | -1.064, 3 failing | +0.245, 0 | **+0.251, 0** |
| ALMs | 9,181 | 7,633 | 7,713 |

**Worst path 65 ns -> 10.9 ns against a 10 ns budget.** A 6.5x overrun is now
an 11% one, and the design is ~1,470 ALMs smaller than when it started.

### What is left, and it is no longer one thing

    29  slot_ram write-enable -> cmd_dma|walk_off      the record walk
    29  slot_ram write-enable -> cmd_dma|walk_cnt
     7  cmd_dma|wr_off        -> cmd_dma|hdr_win
     7  cmd_dma|cw            -> cmd_dma|crc_pay_r     the folds themselves
     5  cmd_dma|m.M_SEED      -> cmd_dma|crc_pay_r
     5  cmd_dma|cw            -> cmd_dma|crc_hdr_r
     4  cmd_dma|m.M_SEED      -> cmd_dma|crc_hdr_r
     2  hps_arbiter|state     -> cmd_dma|crc_pay_r

and the worst single path:

    -1.096 ns  cmd_dma|hdr_win[29][1] -> cmd_dma|seed_steps_q[0]
               10.902 ns

`hdr_win[29]` is part of command_bytes again, but this time it is not a CRC:
it is the header ladder itself. That one cycle checks magic, ABI version,
reserved flags, four length laws, the header CRC, the epoch, AND now derives
the seed controls. Splitting the ladder across two states is the obvious next
move, and it is a sequencing change rather than an arithmetic one.

The fold paths appear here for the first time at ~1 ns over, which is what a
seven-level XOR tree plus its state mux should cost. They are no longer the
problem; they are simply now visible above the others.

### The shape of the whole campaign

Three structural defects, each found by reading the tool's report rather than
the source:

1. a bit-serial CRC chained 8 bytes deep per beat and 28 deep for a seed;
2. the same chain in a second block;
3. a combinational subtract fanning out four ways in the cycle it was used.

None of them was evidence that the architecture is too slow. All three were
accidental depth, and the design got SMALLER when they were removed.

### Still not closed

172 endpoints fail. The report holds 100 of them. The audio Gray-decode family
that was -14.9 ns has not appeared since the first fix and has never been
confirmed fixed -- it needs the full census, not an inference from its absence.

---

## 2026-08-22 -- MEASURED: -56.374 ns -> -3.067 ns, and the design got SMALLER

Composed fit at clean HEAD `fd262de`.

| | before (`8ff73c9`) | after (`fd262de`) |
| --- | ---: | ---: |
| setup worst | -56.374 ns | **-3.067 ns** |
| setup failing endpoints | 3,746 | **584** |
| hold worst | -1.064 ns, **3 failing** | **+0.245 ns, 0 failing** |
| ALMs | 9,181 | **7,633** |
| registers | 9,576 | 9,704 |

**An 18x reduction in the violation, and 1,548 FEWER ALMs.** The bit-serial CRC
chains were expensive in logic as well as in depth; replacing 224 dependent XOR
levels with two constant-width fold trees gave back area rather than costing it.

**The three hold violations are gone too.** They were placement-dependent on the
GPU/video seam, and with the CRC pressure removed the fitter no longer has to
contort around it. That is an observation, not a fix -- the seam is still
crossed directly and still deserves the architectural answer.

### Every CRC family has left the report

    before:  33  cmd_dma|hdr_win        -> cmd_dma|crc_pay_r      -55.2
             33  hps_arbiter|state      -> frameblit|crc_acc      -28.8
              7  audio_fifo|cnt_gray_sync -> cnt_snap_o.value     -14.9

    after:   48  frameblit|r_len -> frameblit|off                  -3.067
             30  frameblit|r_len -> mem_guard|guard_violations
             15  frameblit|r_len -> mem_guard|fwd_req.addr
              5  frameblit|r_len -> mem_guard|fwd_req.len
              2  frameblit|r_len -> frameblit|state

Not one CRC path remains. The audio Gray-decode family has also dropped out of
the top 100 -- it was -14.9 ns and the new worst is -3.067, so it is either
below the reporting threshold or was inflated by placement pressure. It should
be re-checked in the full census rather than assumed fixed.

### The new worst path is one register's fan-out

    -3.067 ns  zhao_debug_frameblit|r_len[2] -> zhao_debug_frameblit|off[12]
               12.866 ns against a 10 ns budget

Every remaining failing family starts at `r_len`. That is the blit length
feeding `remaining = r_len - off`, then `this_len`, then the offset update and
the guard request's address and length. A subtract, a compare, a select and an
address add, in one cycle, fanning out to four destinations.

**12.9 ns is a 29% overrun, not a 6.5x one.** This is ordinary pipelining work
rather than a structural defect: `remaining` and `this_len` can be registered a
cycle ahead, since `off` only changes at chunk boundaries.

### Credit where it is due

The shape of this fix came from a review Fabian relayed, and two of its points
were things I had got wrong:

1. **Constant fold widths, not a runtime count.** My first cut shared one
   generic instance with a runtime `n_i`, which keeps all nine matrices and
   adds a nine-way mux after the XOR trees. Two instances at constant 8 and 4
   let Quartus discard the rest.
2. **The streaming path needs no shifter.** I had written one for a case that
   cannot occur: `M_PAY_WAIT`'s `wr_off` starts at `fetched` >= 40 and the
   payload starts at 36, so the lower bound never clips.

I verified the load-bearing claim myself rather than taking it: with
`command_bytes % 16 == 0` and the first burst capped at 64, the seed length can
only be 0, 16 or 28 bytes, and `36 + cb` is always 4 mod 8. Both check out.

---

## 2026-08-22 -- MEASURED: the FRAMEBLIT rewire removed its family and did not
## move the headline

Composed fit at clean HEAD `8ff73c9`, all four stages successful.

### Before and after

| | before (`d67621d`) | after (`8ff73c9`) |
| --- | ---: | ---: |
| ALMs | 9,167 | 9,181 (+14) |
| registers | 9,171 | 9,576 |
| setup worst | -55.199 ns | **-56.374 ns** |
| setup failing endpoints | 3,595 | **3,746** |
| hold worst | +0.247 ns | **-1.064 ns, 3 failing** |

### What the rewire actually did, which the summary numbers hide

The failing families, before:

    33  cmd_dma|hdr_win        -> cmd_dma|crc_pay_r     -55.2 .. -53.3
    33  hps_arbiter|state      -> frameblit|crc_acc     -28.8 .. -24.2
     7  audio_fifo|cnt_gray_sync -> cnt_snap_o.value    -14.9 .. -10.6

and after:

    32  cmd_dma|hdr_win        -> cmd_dma|crc_pay_r     -56.4 .. ...
    11  audio_fifo|cnt_gray_sync -> cnt_snap_o.value

**The FRAMEBLIT CRC family is gone.** Thirty-three failing paths at -28.8 ns
eliminated, for +14 ALMs. The fold does what it was built to do.

### And the headline did not move, for a reason that was known in advance

The worst path is CMD.DMA's payload-CRC seed loop, which **still calls the
bit-serial chain** -- it was deliberately left for a second pass. Removing a
-28 ns family cannot improve a -55 ns worst case. The -55.199 -> -56.374 drift
is 2% on a path that was not touched: placement variation, not a regression.

### Two things NOT fully explained, recorded as such

1. **Failing endpoints rose 3,595 -> 3,746** even though a failing family was
   removed. The top-100 path report cannot account for that; it holds 100 of
   3,746. Unexplained.
2. **Three hold violations appeared**, worst -1.064 ns, all of them
   `vid_clk -> gpu_clk`:

       vphase                        -> vph_q
       zhao_video_scaler|stage2.y[6] -> crc_in_sof
       zhao_video_mode|mode_cur[1]   -> eof_pend

   None involves DEBUG.FRAMEBLIT. They sit on the GPU/video seam this project
   deliberately leaves TIMED rather than false-pathed -- the timing report's own
   `knownCdc` note says so -- so hold on them is placement-dependent and was
   +0.247 ns by luck in the previous run rather than by construction. That is
   an explanation of the mechanism, not a dismissal: three real hold
   violations are recorded here and they need a constraint decision, not a
   shrug.

### The reading

The approach is validated and the remaining work is located. CMD.DMA's seed
loop is next, and it is a bigger change than FRAMEBLIT's drop-in: the seed
folds a byte range that starts at 36 and ends wherever `command_bytes` says, so
it needs a multi-cycle walk rather than one instance with `n_i` tied high.

---

## 2026-08-22 -- the parallel CRC-32C fold, built and verified

`fpga/rtl/common/zhao_crc32c_fold.sv`. Folds up to eight bytes in ONE shallow
XOR tree instead of sixty-four dependent levels.

    fold_N(c, d) = XOR over set bits i of c of fold_N(1<<i, 0)
               XOR XOR over set bits j of d of fold_N(0, 1<<j)

CRC-32C is linear over GF(2) with no constant term, so each column is a
compile-time constant and the runtime logic is 96 masked 32-bit XORs -- a tree
about seven levels deep. **The columns are derived at elaboration by calling
the bit-serial definition itself**, so the module cannot drift from what it
replaces: change the polynomial and the columns change with it.

`n_i` selects how many leading bytes participate, because no caller always has
eight -- the seed walks a range ending wherever `command_bytes` says, and a
final bridge beat can be partial. Nine matrices, muxed.

### The oracle, and why it is not a restatement

`zhao_abi::zhao_crc32c` wraps the raw running state in an inversion at each
end, so

    zhao_crc32c(c, buf, n) == ~raw_chain(~c, buf, n)

and the raw transform is recovered exactly by undoing both. The test therefore
compares against **the shipped function every other CRC user calls**, not a
second copy of the algorithm written beside it. That distinction is not
academic here: `zhao_field_alu`'s ABS returned the wrong answer for INT32_MIN
and its test restated the law the same wrong way, so the two agreed with each
other through 828 directed and 120,000 random checks.

### Verification

    crc32c_fold_directed        1,178 checks
    crc32c_fold_random          200,000 folds
    crc32c_fold_random_nightly  4,000,000

The directed lane includes **every state basis vector and every data basis
vector at every byte count** -- 32 + 64 columns x 9 counts -- because the module
IS its columns, and a wrong column is exactly a wrong answer for one basis
input that random traffic need never isolate. Plus: n=0 is the identity, bytes
above the count do not participate, and a 256-byte message folded eight at a
time matches the byte-at-a-time chain at every length.

**Mutation sweep: 18 attempted, 18 caught, 0 survivors.** Wrong polynomial,
wrong shift direction, wrong bit tested, seven rounds instead of eight, data
not mixed in, data mixed at the top, high-byte-first ordering, every byte
folded regardless of the count, count off by one, both basis derivations
reversed, either column set dropped, data columns truncated, accumulator seeded
wrong, columns OR-ed instead of XOR-ed, and both count-mux errors.

### What remains

**This is the primitive, verified in isolation. It is not yet wired in.**
CMD.DMA's seed loop and streaming CRC, and DEBUG.FRAMEBLIT's `crc_acc`, still
call the bit-serial chain. Wiring it in is the next step, and the composed fit
is what will say whether it moves the -55.199 ns.

Deliberately stopped here rather than rewiring two verified blocks at the tail
of a long session: the primitive is worth having proven on its own, and the
rewire wants its own differential run against unchanged packet behaviour.

---

## 2026-08-22 -- TIMING DIAGNOSED. It is the CRC, and it is bit-serial.

Re-ran the composed fit with `-KeepWorkspace` to keep `setup_paths.rpt`, on the
principle that cost four wrong theories earlier today: measure, do not infer.

### The failing paths

Worst path, and it names itself:

    -55.199 ns   zhao_cmd_dma:u_dma|hdr_win[28][4]
              -> zhao_cmd_dma:u_dma|crc_pay_r[3]      64.584 ns data delay

`hdr_win[28]` is `command_bytes`. So the chain is: read `cb` out of the header
window, compute `seed_end = 36 + cb`, run the 28 guarded CRC-32C steps of the
payload-CRC seed, land in `crc_pay_r`. **The payload-CRC SEED LOOP**, not the
`crc_final()` header sweep I had guessed at.

Three families among the 100 worst, all on `gpu_clk`:

| paths | from -> to | slack | delay |
| ---: | --- | ---: | ---: |
| 33 | `cmd_dma\|hdr_win` -> `cmd_dma\|crc_pay_r` | -55.2 .. -53.3 | ~63-65 ns |
| 33 | `hps_arbiter\|state.A_IDLE` -> `frameblit\|crc_acc` | -28.8 .. -24.2 | ~34-39 ns |
| 7 | `audio_fifo\|cnt_gray_sync` -> `cnt_snap_o.value` | -14.9 .. -10.6 | ~20-25 ns |

### The cause, and it is one line of shared code

`zhao_abi_pkg::zhao_crc32c_step` is **BIT-SERIAL**:

    crc = c ^ {24'b0, d};
    for (int i = 0; i < 8; i++)
      crc = (crc >> 1) ^ (crc[0] ? 32'h82F63B78 : 32'b0);

Eight DEPENDENT XOR levels per byte. So:

* FRAMEBLIT and `M_PAY_WAIT` fold 8 bytes per beat = **64 chained levels**
  -> ~38 ns measured;
* the seed loop folds 28 bytes in one cycle = **224 chained levels**
  -> ~64 ns measured.

Both against a 10 ns budget. This is not a placement problem or a fitter
problem; it is a combinational depth problem in code every CRC user shares.

**Spreading the seed loop over more cycles does NOT fix it.** FRAMEBLIT already
does only 8 bytes per cycle and still costs 38 ns. At ~0.6 ns per level the
budget is roughly 16 levels, which is TWO bytes per cycle -- and the streaming
payload CRC has to keep up with 8 bytes per bridge beat, so it cannot be slowed
down at all.

### The fix, which is standard

CRC-32C is a LINEAR function of its input, so folding N bytes is a fixed
GF(2) matrix -- an XOR tree of depth ~log, not 8N dependent levels. Replacing
the bit-serial loop with a parallel form makes 8 bytes per cycle cost a handful
of levels instead of 64.

It is verifiable bit-exactly against what exists: `zhao_crc32c_step` is itself
the oracle, and equivalence over random `(c, data)` is a pure combinational
check that can be driven very hard.

### A caveat that matters, from the same run

    Unconstrained Input Ports       609
    Unconstrained Input Port Paths  13,920

Paths that START at an input port are not analysed. The STREAMING CRC chains
begin at `hps_rsp_i.data`, which is an input port of these blocks, so they fall
in that class. **The -55.199 ns is a lower bound on the problem, not a
measurement of all of it.** The harness's own limitations list already says
unconstrained paths remain reportable; here it materially changes what the
number means.

---

## 2026-08-22 -- the sweep harness could mis-attribute a verdict, and did

### What happened

The Field IR arithmetic-core sweep came back 31/34 with four mutations tagged
"caught by the RANDOM lane only", meaning the directed cases supposedly could
not tell them apart. One of the four was `sub_is_add` -- replacing `a - b` with
`a + b` in the block's own subtract.

That is not a plausible directed-lane miss, and it is not one. Applied by hand:

    FAIL: FIELD.SUB: zero minus INT32_MIN saturates: value:
          expected 0x7FFFFFFF, got 0x80000000
    [field_alu_ops] 9/984 checks FAILED

**The attribution was simply wrong.** All four were CONSECUTIVE in the
mutation order, which is the tell: four independent coverage gaps do not
arrive in a row.

### The flaw in my own guard

Every sweep in this project discards a result when the generated-model hash
equals the BASELINE hash, on the reasoning that an unchanged hash means
Verilator never re-ran. That catches the case it was written for and misses a
worse one:

**a build that silently serves the PREVIOUS mutant's model.** That hash differs
from the baseline, so the guard passes it, and the verdict is recorded against
the wrong mutation. The sweep was sharing the machine with a `quartus_fit` run
at the time, which is exactly when such a race surfaces.

So the guard proved "something was rebuilt", not "THIS mutation was built" --
a weaker statement than the one it was making, which is the same defect shape
as the parity gate earlier today.

### Fixed

The hash must now differ from the baseline **and** from the previous
iteration's hash. A stale model is identical to the previous mutant's, so it is
caught and the sweep aborts rather than reporting.

    if h == base_hash:  DISCARD -- Verilator did not re-run
    if h == prev_hash:  DISCARD -- this build served a stale model and the
                        verdict would be attributed to the wrong mutation

### The actual cause, found after two more wrong theories

I blamed concurrency with the Quartus fitter. Then I blamed a stale-model
race and strengthened the hash guard. Then the strengthened guard fired on a
QUIET machine, which killed that theory too. Then I "proved" one mutation
equivalent by diffing the generated C++ and getting zero lines.

All four were wrong, and they were wrong about the same thing:

    ninja: error: rebuilding 'build.ninja': subcommand failed

**When ninja cannot regenerate build.ninja it builds NOTHING AT ALL**, and it
says so in one line that scrolls past while the rest of its output looks
ordinary. Every mutation was testing whatever binary happened to be lying
around from an earlier build.

Why it could not regenerate: `tests/CMakeLists.txt` reads
`set(VERILATOR_ROOT "$ENV{VERILATOR_ROOT}")` -- from the ENVIRONMENT, at
configure time. Editing `tests/lint/source_list_parity.cmake.in` earlier in the
session marked `build.ninja` stale, and every subsequent build from a shell
without `VERILATOR_ROOT` set tried to reconfigure, failed, and did nothing.

**The zero-line diff that "proved" the equivalence proved only that the model
had not been regenerated.** The equivalence is still true, but it is true by
algebra -- clamping a value already on the rail returns that same value -- and
that argument never needed the diff. The diff was evidence of nothing and it
was quoted as if it were the stronger form.

### Fixed

`build()` sets `VERILATOR_ROOT` explicitly and treats
`rebuilding 'build.ninja'` in ninja's output as FATAL. A sweep that cannot
build must stop, not report.

### Which earlier results this affects

**None of the committed ones.** `build.ninja` only became stale when the
parity-gate template was edited, in commit `f0e101b`. The CMD.DMA RAM sweep
(19/19), the DEBUG.FRAMEBLIT atomicity sweep (20/20) and the sequencer
ALU-dispatch sweep (10/11 + 1 equivalent) were all run and committed BEFORE
that, on a build tree that regenerated normally. They stand.

The arithmetic-core sweep is the only one affected, and its first three
attempts are discarded. The fourth, on a sound build, is the one recorded:
**34 attempted, 31 caught -- every one by the DIRECTED lane -- 3 equivalent by
algebra, 0 real gaps.**

### The lesson, which is the same one as the morning's

Four theories about a failing measurement, each reasoned from the code, none
from the tool's own output. The line that settled it had been printed on every
single run. This is the second time today that reading what the tool actually
said would have replaced an afternoon of inference -- the first was
`Total block memory bits: 0` sitting in a report since the first CMD.DMA fit.

The three survivors are a separate matter and stand on proof rather than on
the sweep: they are the `sat32` boundary mutations, equivalent because clamping
a value already sitting exactly on the rail returns that same value. Written up
in the RTL with the argument, and the ledger bound they do NOT affect lives in
`sat32_fired`, whose own mutations were caught both ways.

---

## 2026-08-22 -- THE COMPOSED SHELL SYNTHESISES. First composed result this
## project has ever had.

    tools/quartus/run_composed_fit.ps1 -NoPush
    run: wumen-f0e101b-20260822T051855Z
    Quartus 17.0.2 Lite, 5CSEBA6U23I7, NUM_PARALLEL_PROCESSORS staged to 1

    PASS source parity: 26 ordered shell sources match tests/CMakeLists.txt.
    RUN analysisAndElaboration  47s    peak virtual memory 4,998 MB
    RUN synthesis             3m26s    peak virtual memory 4,968 MB
    RUN fitter                        peak virtual memory 5,359 MB
    RUN timequest
    exit=0 after 1679.3s   CLEAN_HEAD f0e101b

### IT FITS. IT DOES NOT CLOSE TIMING.

The script prints "PASS analysis/elaboration, synthesis, fitter, and
TimeQuest", and that means the four STAGES ran, not that timing was met. The
report is explicit: `"timingPassed": false`. Both statements are in the same
run and only one of them is the answer to the device question.

**Fitted resources** -- comfortable:

| | composed shell | device |
| --- | ---: | ---: |
| Logic utilization (ALMs) | **9,167** | 41,910 (**21.9%**) |
| registers | 9,571 | |
| block memory bits | 114,688 | |
| RAM blocks | 13 | 553 |
| DSP blocks | 0 | 112 |
| virtual pins | 2,336 | |

**Timing** -- not close:

| analysis | worst slack | failing endpoints |
| --- | ---: | ---: |
| setup | **-55.199 ns** | **3,595** |
| hold | +0.247 ns | 0 |
| recovery / removal | -- | 0 |

And the failure is in ONE domain:

    Slack       TNS         Clock
    -55.199     -12870.651  gpu_clk
      1.468          0.000  vid_clk
     33.665          0.000  audio_clk

`gpu_clk` is targeted at 10 ns. The worst path takes about 65 ns, so it misses
by roughly 6.5x. `vid_clk` and `audio_clk` both pass.

### The caveat, stated but not used as an excuse

Three of the five critical warnings are the same one: the clock ports are fed
by VIRTUAL PINS, so "timing analysis treats input to the clock port as a ripple
clock" -- the clock arrives through general routing rather than a global clock
network. That makes this pessimistic by an amount nobody has measured. It does
not plausibly account for 55 ns.

### What is NOT yet known, and will be measured rather than guessed

Which paths. The obvious suspect is CMD.DMA's `M_HDR_CHK`, which evaluates
`crc_final()` over 32 bytes -- 32 chained CRC-32C steps, on the order of 256
dependent XOR/shift stages -- plus the payload-CRC seed loop over 28 more, all
combinationally in ONE cycle. That is the same shape as the 192-iteration loop
that made this block unsynthesisable, bounded but not shortened.

**That is a hypothesis and it is written down as one.** Four hypotheses about
this exact block were wrong earlier this week, every one an inference about
what the fitter was building, and the thing that settled it was reading the
report. The next step is `report_timing` on the failing endpoints with
`-KeepWorkspace`, not another theory.

### The numbers

| | composed `zhao_shell_top` | device |
| --- | ---: | ---: |
| Estimate of Logic utilization (ALMs needed) | **9,440** | 41,910 (**22.5%**) |
| Combinational ALUT usage for logic | 11,902 | |
| Dedicated logic registers | 9,171 | |
| Total block memory bits | **114,688** | |
| Total MLAB memory bits | 0 | |
| Total DSP Blocks | 0 | 112 |
| Total virtual pins | 2,336 | |

### What this settles

`reports/composed/README.md` named the blocker precisely: **Quartus Error
276003**, registers that cannot convert to RAM megafunctions, from two memories
with asynchronous read logic -- `zhao_scanout_linebuf|mem` (fixed earlier) and
`zhao_cmd_dma|blit_buf` (not fixed).

`blit_buf` left with the blit engine, and `slot_buf` became `slot_ram` with a
registered read earlier today. **No 276003 in this run.** 114,688 block memory
bits, 0 MLAB bits -- the memories are real M10K.

It also demolishes the memory story that shaped months of planning. The record
said composed analysis + synthesis cost **42:33 at a 6.2 GB peak** and was the
reason a second machine was briefed. Measured now: **4m13s total at a 5.0 GB
peak**, on a 23.8 GB machine. The 28.4 GB figure was already known to be a
wildcard-VIRTUAL_PIN bug; the residual 6.2 GB was recorded as "unexplained
rather than settled", and a large part of it was CMD.DMA's 32,768-register
staging array, which no longer exists.

**The handoff brief in reports/composed/README.md is now doubly superseded:
the machine was never needed and the RTL blocker it names is gone.**

### Scope, stated plainly

This is `zhao_shell_top` -- 26 modules: the command spine, video, memory,
debug. It is NOT all 92 ledger blocks, and it is not a programmed board or
fabricated silicon. A composed fit against a provisional device with virtual
I/O says the design maps and closes timing IN THE TOOL. Nothing has run.

---

## 2026-08-22 -- the composed fit's runner could never start, and the tooling
## environment is not reproducible

### The composed fit has never run, and the reason is a two-line script bug

`run_composed_fit.ps1` failed **after 0 seconds**:

    EXCEPTION: Argument transformation for parameter "Processors":
    cannot convert value "-ReportRoot" to type "System.Int32"

Windows PowerShell 5.1 **array splatting supplies POSITIONAL arguments**. The
strings that look like parameter names are handed over as values, so
`@('-ReportRoot', $runDir, ...)` put the literal `-ReportRoot` into
`run_shell_fit.ps1`'s first positional parameter, which is `[int]$Processors`.

Reproduced directly, then fixed by switching to **hashtable splatting**, which
binds by name. This script could not reach `quartus_map` on any invocation, so
the composed fit's own runner had never started a fit.

### Then it found a real defect: the two source lists disagree on ORDER

    Shell source parity failed at index 23:
      CMake='fpga/rtl/debug/zhao_debug_frameblit.sv',
      QSF='fpga/rtl/video/zhao_video_slotmgr.sv'

`zhao_debug_frameblit` and `zhao_video_slotmgr` are swapped between
`tests/CMakeLists.txt` and the QSF.

**And my own gate could not see it.** `source_list_parity` was written earlier
this month to catch exactly this class, and its comment says: *"This gate does
not merge the lists (Quartus needs an ORDER, CMake does not). It asserts the
SETS match."* True of the tools, false of the project --
`run_shell_fit.ps1` compares the lists INDEX BY INDEX and refuses to run.

So a gate written to catch two-statements-of-one-fact was itself a weaker
statement of the fact it guarded. Order checking added; verified with teeth by
swapping the pair back (fails in 0.07 s naming index 22) and restoring.

    PASS source parity: 26 ordered shell sources match tests/CMakeLists.txt.

### FOURTH instance of the same environment defect

Four different tools, one cause: **PowerShell's PATH resolves to a different
toolchain than the one the build tree was created with.**

| tool | resolved to | consequence |
| --- | --- | --- |
| `ctest` | msys2 | all 337 tests reported BAD_COMMAND |
| `git` | msys2 (no autocrlf) | 29 RTL files called modified; `rtlCleanAtHead` never true in 42 rows |
| `cmake` | msys2 | "CXX compiler is unknown", configure refused |
| `verilator` | -- | a reconfigure in the wrong env emptied `VERILATOR_ROOT`, and every lint test fell back to a compiled-in `/yosyshq/...` path that does not exist here |

The last one was self-inflicted: reconfiguring from PowerShell dropped
`VERILATOR_ROOT`, because `tests/CMakeLists.txt` reads it from
`$ENV{VERILATOR_ROOT}` at configure time. 68 lint tests failed until it was
restored and the tree reconfigured with it set.

**The working combination, recorded so it is not rediscovered:**

    ctest    C:/Programmieren/dsstuff/mingw64/bin/ctest.exe
    cmake    C:/Programmieren/dsstuff/mingw64/bin/cmake.exe
    git      Bash's /mingw64/bin/git, or force -c core.autocrlf=true
    configure with VERILATOR_ROOT set, and with the build dir spelled
      exactly as CMakeCache.txt has it (the cache is case-sensitive about it)

    ctest -L fast: 252/252 after the repair.

---

## 2026-08-22 (later) -- DEBUG.FRAMEBLIT to RTL_VERIFIED; two sweeps; the clean flag proven

### DEBUG.FRAMEBLIT mutation sweep -- the atomicity law

    scratchpad/mut_frameblit.py    20 mutations
    attempted=20 caught=20 survived=0

Aimed at the LAW, not the copy: publish before writes retire, publish before
writes issue, publish with no byte check, publish with no CRC, unfinalised CRC,
CRC never accumulated, publish on a lost lease, publish while aborting, release
before writes retire, release a lease never owned, ownership taken before the
checks, aborted beats folded into the CRC, accept any length, blit with no
lease, ignore slot mismatch, ignore high slot bits, ignore a generation move,
either half of the guard verdict ignored, a bridge error forgotten.

Each leaves a working blit and an unsound machine, which is why a
happy-path-only differential would have been green through all twenty.

Advanced UNIT_VERIFIED -> RTL_VERIFIED. Justification against how this project
has used the rung: unit differential against the shipped oracle
`zref::debug::run_blit` **and** the composed path -- instantiated in
`zhao_shell_top` at `u_frameblit`, driven end to end by
`tests/shell/shell_golden.cpp`. Same shape as CMD.SCHEDULER's RTL_VERIFIED,
which cites a system-level demo over its own directed test.

Three lanes green including formal (27 assertions to depth 44, bmc + cover).

    tools/quartus/run_block_fit.ps1 -Module zhao_debug_frameblit
    zhao_debug_frameblit   ok   ALM 962 / 41,910   (2.3%)
                                registers      909
                                blockMemoryBits  0
                                DSPs             0

The block extracted from CMD.DMA precisely so a debug path could not stop the
shell fitting, and it is 2.3% of the device.

### rtlCleanAtHead now carries information

**`rtlCleanAtHead: true` for the first time in the file's history, on row 43.**

All 42 prior rows said false. A field meant to say whether a fit result can be
trusted against the commit it names was answering identically on clean and
dirty trees -- indistinguishable from not having it.

Cause, same class as this session's ctest finding: PowerShell resolves `git` to
whichever binary is first on PATH -- the msys2 one under devkitPro -- which
carries no `core.autocrlf`. A status through it calls every CRLF worktree file
modified: 29 RTL files, **279 insertions against 279 deletions on a 279-line
file**. Every line. Pure line-ending churn. Bash's mingw64 git, with
`autocrlf=true`, calls the same tree clean.

`run_composed_fit.ps1` already documented this exact failure and already forced
`-c core.autocrlf=true`. `run_block_fit.ps1` never inherited it. Fixed.

### Sequencer ALU-dispatch sweep -- does the new coverage discriminate?

    scratchpad/mut_seq_alu.py    11 mutations
    attempted=11 caught=10 survived=1

The opcode-coverage gate had just found SUB/MIN/MAX/ABS issued only by the
random pool and CMP by neither lane. A test written to close a coverage hole is
worth exactly what it discriminates, so this sweep asked whether the new
directed cases can tell a broken dispatch from a working one.

**Every caught mutation was caught by the DIRECTED lane, not only the random
one** -- including both immediate mutations, which is the path CMP's comparison
mode rides on. The sweep reports which lane did the work precisely so "caught
by the 400-program random lane" cannot be mistaken for "the directed case
covers it".

**ONE EQUIVALENT MUTANT, and it is genuinely equivalent.**
`exec_writes = unit_handled ? 1'b1 : alu_writes` -> `1'b1` survives.
`zhao_field_alu` clears `writes_o` in exactly two places: `OP_END`, which also
raises `is_end_o`, and the `default:` refusal, which also raises
`op_unsupported_o`. The write-back guard already carries
`!alu_is_end && !exec_unsupported`, and for an ALU op `exec_unsupported` IS
`alu_unsupported` -- so every case where `alu_writes` is 0 is excluded by a
different term of the same condition. Recorded in the RTL with an ENFORCED-BY
pointing at the source of the guarantee, so it does not read as a hole. The
expression stays because it says what the value MEANS and would stop working
the moment the ALU learns a third non-writing op.

---

## 2026-08-22 -- CMD.DMA staging buffer to block memory; the block fits

### State recovery

    git log --oneline -5          -> 995595f at session start
    ctest -L fast                 -> BAD_COMMAND on all 337 tests

**The suite was not broken.** `PATH` resolved `ctest` to
`c:\devkitPro\msys2\usr\bin\ctest.exe`, while the build tree was configured by
`C:/Programmieren/dsstuff/mingw64/bin/cmake.exe`. The msys2 ctest misreads the
Windows absolute paths in `CTestTestfile.cmake` and concatenates them onto the
working directory:

    Command: "/c/.../build/tests/C:/Programmieren/.../test_cmd_dma_directed.exe"

**The gate is `C:/Programmieren/dsstuff/mingw64/bin/ctest.exe`.** Using it,
baseline was 252/252. Recorded because "the whole suite fails" was one wrong
inference away from a day spent debugging nothing.

### Finding 1 — the formal lane was RED at HEAD (fixed, commit `b9d4101`)

    ctest -R formal_cmd_dma_crc_gate   -> bad state property 3 REACHABLE at k=12

Property 3 is `assert(fetched <= 32'd64)` under `m == M_HDR_CHK` — an assertion
I had added myself to justify bounding the CRC seed loop at 64, with the
comment "this is the whole reason the loop is safe".

It was not safe. The bound holds only if the bridge delivers exactly the beats
requested, and nothing in the RTL enforces that: `zhao_hps_bridge.sv` forwards
the external HPS's `last` straight through (`rsp.last <= hps_rd_last`) without
counting against `busy_len`. The formal harness leaves every response beat
free, so a bridge withholding `last` for nine beats drives `fetched` to 72.

Consequence was bounded — missed bytes fall outside the seed, the payload CRC
mismatches, the packet is rejected — so this was a **false assertion**, not an
exploitable hole. Still false.

Fixed by construction: both burst waits record `burst_end` when they issue and
leave on `last` **or** on having taken the whole burst. `M_PAY_WAIT` got the
same guard though no assertion covers it.

**Which lane enforces it — measured by mutating the guard back:**

| lane | verdict |
| --- | --- |
| `cmd_dma_directed` | passed |
| `cmd_dma_random` (400) | passed |
| `cmd_dma_random_nightly` (5,000) | passed |
| `formal_cmd_dma_crc_gate` | **caught it** |

The C++ bridge model is lawful by construction, so the differential is
structurally blind to this. ENFORCED-BY `tests/formal`, not the sim lanes.

### Finding 2 — the measurement that ended four wrong theories

`blockfit.map.rpt` from the run that synthesised cleanly:

    Estimate of Logic utilization (ALMs needed) : 83,977     device 41,910
    Combinational ALUT usage for logic          : 94,698
    Dedicated logic registers                   : 33,680
    Total block memory bits                     : 0

`Total block memory bits: 0` — no RAM inferred at all. 32,768 of the 33,680
registers were `slot_buf`. The block was a 4,096-entry **register file** with a
variable read address and a variable write address, which is why all three
mux-sharing attempts moved the number by ~0.02%.

Four theories, three 45-minute compiles, all spent reasoning about the source
while this report sat in `output_files` from the first run.

### The change (commit `f5e067e`)

The design note in `reports/REMAINING_BLOCKERS.md` claimed each CRC walk would
become a four-step read loop. **Wrong** — both walks and every header field
live inside bytes 0..63, and the rest of the payload CRC already streams from
the bus in `M_PAY_WAIT`. So:

* `hdr_win` — 64 bytes in registers. The **entire header ladder unchanged**: no
  extra states, no re-timed checks. 512 registers.
* `slot_ram` — 512 x 64b. No initialiser, written by a process with **no
  reset**, **one** registered read port, address muxed across the four states
  that read it (`M_PCRC_RD`, `M_WALK_RD`, `M_STREAM_RD`, and the stream's
  one-word lookahead).

Every multi-byte read fits in one word, so none costs a second access:
`command_bytes` and record lengths are multiples of 16, so `36+cb` and
`36+walk_off` are both 4 mod 8 — and the walk's **two** 16-bit fields land in
the **same** word, one read serving the pair. The `M_PCRC` split from the prior
session was the precondition, not a detour: it is what put the readers in
different states.

Cost: the walk takes two cycles per record instead of one; the stream fetches a
word every eight bytes with seven cycles of slack.

### Evidence

    tools/quartus/run_block_fit.ps1 -Module zhao_cmd_dma
    Quartus 17.0.2 Lite, 5CSEBA6U23I7

|  | before | after | device |
| --- | --- | --- | --- |
| ALMs (fitted) | 83,977 | **3,607** | 41,910 |
| registers | 33,680 | **1,571** | |
| block memory bits | 0 | **32,768** | |
| M10K blocks | 0 | **4** | 553 |
| DSPs | 0 | 0 | 112 |
| MLAB bits | — | 0 | |

"Fitter placement was successful" — a line this block had never produced.
Router estimated average interconnect usage **2%**, peak 28% in one region.
`quartus_map`'s log was 9.8–11 MB on every prior run and is **571 KB** now.

**Reproduced exactly across two independent runs** (1038.5 s and 1181.4 s,
both ALM 3607).

Recorded in `reports/synthesis/zhao_block_fit.json`.

### Mutation sweep — the new RAM seam

    scratchpad/mut_dma_ram.py    19 mutations
    attempted=19 caught=19 survived=0

Forced regeneration (`os.utime` +5 s) with a **generated-model** hash check
(not the linked binary, which is not reproducible); no result accepted without
the hash moving. Preflight caught one two-site mutation before any build.

Covered: wrong word address, dropped header offset, off-by-one word, inverted
half-word select, swapped opcode/length fields, every skipped read cycle, the
stream's lookahead and word crossing, the write-address shift, error beats
taken as data, a too-small header window.

**The restore guard fired** at the end — an mtime race left the model one build
stale. Source was pristine; a forced rebuild returned the model to baseline
hash `4d6eb35acf01daa3`. The guard exists for exactly this.

### Test gaps closed first (the stream rework is the delicate part)

1. `cmd_dma_random` (5,000 packets) **never checked the content** of what it
   streamed — only the verdict, byte count, and that broken packets produced
   nothing. Now compares bit-exact.
2. `pkt_ready_i` was **hardwired to 1** for the whole suite. Now stalls on a
   third of packets, so the word-boundary prefetch meets a consumer that pauses.

Verified with teeth: breaking the word crossing fails **60 of 1,542**.

### Process findings

* `[String]::Replace($old,$new,1)` — .NET has **no count overload**. The call
  threw, execution continued, "MUTANT APPLIED" printed, and ninja's "no work to
  do" was the only tell. An unapplied mutation reads exactly like a surviving
  one.
* Reverting a file to byte-identical content within the same timestamp
  granularity does **not** trigger a ninja rebuild. Forcing mtime is required,
  twice observed this session.

---

## 2026-08-22 — Field IR sequencer: opcode coverage gate

`zhao_field_seq` dispatches all **31** Field IR opcodes (15 in the ALU
including DOT2/DOT3, 16 in the units). Nothing enforced that the differential
had actually issued each one.

Added `opsIssued()` behind `Prog::op()` — the single funnel for every
instruction the test builds — plus a check that every required opcode was seen.

**It failed on its first run**, and the gaps were real:

| opcode | where it was |
| --- | --- |
| SUB, MIN, MAX, ABS | random pool **only** — the directed lane never touched them |
| CMP | **neither lane** — dispatched by the ALU, decoded by the reference, never once executed |

Closed with a directed section covering all five, including **all six CMP
comparison modes** at equal/less/greater and across sign boundaries, and CMP
added to the random pool with its `imm` mode selected in range.

    test_field_seq_directed            906 checks passed
    test_field_seq_directed --random 400   2506 checks passed
    coverage: all 31 required opcodes issued

`opsIssued()` legitimately exceeds the required set — some cases issue a
deliberately invalid opcode to prove the unsupported path. Noted in the code so
it does not read as a discrepancy.
