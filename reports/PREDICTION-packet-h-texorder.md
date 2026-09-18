# Prediction for `@packet-h-texorder`, written before the fit starts

Two changes, batched deliberately — neither was worth a fit alone, and the
sequencing note in the roadmap says why: at `@packet-h-satstage` the cache
pipe's `valid_r` (53 paths, -2.853) sat **0.056 ns** ahead of the request
queue's `h_d_q` (42 paths, -2.797), so repairing either one alone would have
reported almost nothing.

Baseline `@packet-h-satstage`: ALM 27,583, 63 DSP, 136 M10K, **gpu_clk 77.80
MHz**, worst -2.853, TNS -2,276.4.

## What is in this fit

1. **`zhao_texture_cache_pipe_v2` — compare first, select second.** The mask
   used to be built by selecting the winning lane's tag and *then* comparing
   four lanes against it, 28 bits wide, behind a LANES-deep priority chain.
   The receipt put 6.575 ns of a 12.684 ns path in exactly that span. Now a
   pairwise `(tag, idx)` table is computed from registers beside the priority
   chain and the mask is a one-hot pick from it.
2. **The island's COMBINE fence — evaluate every entry, then select.** It used
   to index a 64-wide fence with the output of `u_own`'s queue read: two
   chained array reads, 5.70 ns of a 12.277 ns path. Now four fence lookups run
   beside each other and `cmb_rp_o` selects. Removes the 1.43 ns read mux from
   the cone; the 4.27 ns of 64-wide select remains.

## Prediction

1. **`valid_r` leaves the top of the band decisively** — not by 0.5 ns but by
   most of 6.575, so it should fall below every endpoint currently listed.
2. The rq family (`h_d_q`, `s_d_q`, `lcnt_q`, `rp_q`, `h_v_q`, `s_v_q` —
   ~110 of 200 paths) improves by **about 0.9 ns**, to roughly -1.9.
3. **The new wall is `walk_q_r` at -2.056**, which neither change touches, or
   the rq family just behind it. **Worst -2.0 to -2.2, gpu_clk 82–84 MHz.**
4. **ALM goes UP, 27,800–28,200.** Cone 1 replaces four 28-bit comparators with
   a 4x4 pairwise table — sixteen, of which six are distinct — and cone 2 turns
   one 64:1 mux into four. Neither is free, and a prediction that only ever
   forecasts savings is not a prediction.
5. DSP 63 and M10K 136 unchanged; neither change touches a multiplier or an
   array.
6. TNS improves to **-800 to -1,400**.

## What would falsify the reasoning rather than the numbers

**If `valid_r` is still the worst path**, the 6.575 ns I attributed to
select-then-compare was not that at all, and the Data Arrival Path was read
wrong — the levels are named `m_tag_c[*]` and `m_mask_c[*]`, so this would be a
surprise worth understanding before any further reordering.

**If the rq family does not move at all**, Quartus has re-factored the four
parallel fence lookups back into "select then look up" — a legal
transformation, since they share structure and the tool optimises for area
unless told otherwise. That would be the more interesting outcome: it would
mean this class of fix needs a registered boundary or a synthesis attribute to
survive, not just a source reordering, and it would cast doubt on cone 1's
mechanism as well.

**If ALM rises by much more than 600**, the pairwise table is being built for
all sixteen ordered pairs rather than the six distinct ones, and the loop
should be written to exploit symmetry.

---

# RESULT: `@packet-h-texorder`, commit 709e22a8, clean tree, 89 sources

```
ALM 27,636   DSP 63   M10K 136   registers 37,578   1,592.8 s   status ok
gpu_clk 79.74 MHz   worst -2.540   TNS -2,077.4   hold +0.243
fmaxByClock: gpu_clk 79.74 | audio_clk 89.27 | vid_clk 104.87
gatingClock gpu_clk   gatingFmaxMhz 79.74   gatingPeriodNs 12.54
```

**The new receipt fields work.** `gatingFmaxMhz` reads 79.74 and agrees with
Quartus's own `gpu_clk` line to the digit, and `fmaxByClock` carries all three
domains — so this row can be read correctly without the `.sta.rpt` beside it.
This is also the first row where `fmaxClock` and `gatingClock` agree again,
because the render path is once more the slowest thing in the design.

## Scorecard: the two STRUCTURAL predictions were right, three of four NUMBERS were wrong

| # | predicted | measured | |
|---|---|---|---|
| 1 | `valid_r` leaves the band decisively | **53 paths → 0**, better than the -1.369 floor | yes |
| 2 | the rq family improves ~0.9 ns | `h_d_q` 42, `s_d_q` 37, `lcnt_q` 22, `rp_q` 7, `h_v_q`/`s_v_q` 3 — **all gone** | yes |
| 3 | worst -2.0 to -2.2, **82–84 MHz** | **-2.540, 79.74 MHz** | **missed, worse** |
| 4 | ALM 27,800–28,200 | **27,636** (+53) | **missed, better** |
| 5 | DSP 63, M10K 136 unchanged | 63, 136 | yes |
| 6 | TNS -800 to -1,400 | **-2,077** | **missed, worse** |

**164 of the 200 printed paths left the window.** Both fixes did exactly what
they were built to do, and the clock moved 1.94 MHz.

## Why the gain was small, and it is the mulstage pattern again

The wall is now a **single path** at -2.540: an `altsyncram` port-B write-enable
register in `zhao_texture_frag_expand_v2`'s fragment memory, into
`zhao_texture_binding_resolver_v2`'s `read_row_present_q`. At
`@packet-h-satstage` that same endpoint was **-2.048**. It got 0.492 ns WORSE
while everything around it improved.

That is the third time in this campaign: remove a dominant tier and a straggler
inherits the gate, degraded, because the fitter stops spending placement on it.
`base_min_y0_r` did it to `final_sat_r`, `final_sat_r` did it to the texture
band, and now the texture band has done it to a single RAM write-enable.

**Prediction 4 is the same effect seen from the other side.** I forecast ALM up
27,800–28,200 on the reasoning that both changes trade area for depth. It came
in at 27,636, +53. The pairwise table and the four parallel fence lookups cost
almost nothing — because the fitter, freed of the paths it had been fighting,
spent less elsewhere.

## The band is now flat, which changes what a fix is worth

200 paths between **-2.540 and -1.369**. There is no tier left to remove: the
worst endpoint owns ONE path, the second owns 26, and nothing owns more than 32.
**From here each fix buys a fraction of a MHz unless several land together**, and
the honest projection is that 79.74 → 100 MHz is a campaign of many small cones
rather than three more big ones.

## The next four, all named from this receipt

| slack | n | from → to |
|---:|---:|---|
| -2.540 | 1 | `fragment_m` RAM port-B write enable → `binding_resolver_v2|read_row_present_q` |
| -2.025 | 26 | `tile_pipe|plane_dndx_q[2][27]` → `attrgrad|mul_x_r[83]` |
| -1.982 | 1 | `fragment_m` RAM port-B write enable → `aux_pipe_v2|a0_input_fault_q` |
| -1.954 | 1 | `binner|d_meta_r[236]` → `attrgrad|st_r.S_IDLE` |

**`mul_x_r` at 26 paths is the largest group, and it is my own multiply split
coming back.** Having given the two multiplies their own edge, the multiply
itself is now the cost:

```systemverilog
// zhao_raster_attrgrad_v2.sv:112, with job_min_x_i declared signed [11:0]
mul_x_c = dndx_in_c * 96'(job_min_x_i);
```

**Both operands are narrow, and the widths are worse than I first wrote.**
`job_dndx_i` is `signed [71:0]` and `job_min_x_i` is `signed [11:0]`:

```systemverilog
// zhao_raster_attrgrad_v2.sv:103-113
logic signed [95:0] dndx_in_c, mul_x_c;
dndx_in_c = 96'(job_dndx_i);            // 72 bits, sign-extended to 96
mul_x_c   = dndx_in_c * 96'(job_min_x_i);   // 12 bits, sign-extended to 96
```

**The true product is 72 x 12 = 84 bits exact**, which fits in the 96-bit result
with room to spare, so nothing is truncated either way — sign-extending both
operands and multiplying 96 x 96 gives the identical value. The design is
asking for a 96-wide partial-product array to compute an 84-bit number from a
12-bit multiplier.

**Writing it narrower in SystemVerilog does not fix it**, and that is worth
stating so nobody tries: `a * b` takes its operand widths from the assignment
context, so any expression assigned to a 96-bit lvalue is a 96 x 96 multiply
however the operands are cast. Quartus can usually prune sign-extension bits;
the receipt says it has not pruned enough here.

The fix is an explicit structural product over the 12-bit operand — twelve
conditional shifted adds, or about six in CSD form — and this repository has
done exactly that once before: D1's offender 1 was closed with *"registered
steps (r4) + CSD columns (r8)"*.


That is the next cone, and unlike cones 1–3 it is arithmetic restructuring with
a bit-exactness obligation, so it needs the differential
(`raster_attrgrad_dsp3_diff`) watching it — which is exactly what that
differential is for and why the tree change earlier in this campaign could lean
on it.

**And cone 3 is still uncashed**: it is committed, it removes `walk_q_r`, and
`walk_q_r` is now at -1.780 — *below* all four of the above. So cone 3 alone
will not move the clock either. It goes in with whatever comes next.

---

## CONE 4, traced and diagnosed: a read-during-write bypass on the machine's worst path

`@packet-h-texorder`'s worst path is one path at **−2.540**, and its launching
register is not anything anyone wrote:

```
  7.965   fragment_m_rtl_0|...|ram_block1a140~PORT_B_WRITE_ENABLE_REG
  8.162   ...|ram_block1a140|portbdataout[22]
  9.399   u_expand|fragment_m~374
 11.319   u_expand|Add0~9      ┐
 12.400   u_expand|Add0~21     ┘  a carry chain
 14.130   u_binding|Mux20~99   ┐
 15.503   u_binding|Mux20~101  │  three mux levels, 2.19 ns
 16.240   u_binding|Mux20~112  ┘
 17.981   -> u_binding|read_row_present_q
```

**A RAM's PORT-B WRITE ENABLE register is launching a data path.** That only
happens when the synthesiser has had to build read-during-write bypass logic:
the read output must reflect a write landing at the same address in the same
cycle, so the write enable becomes part of the read data.

And the source says exactly why:

```systemverilog
// zhao_texture_frag_expand_v2.sv:121, 134, 283
fragment_t fragment_m [FQD];
assign head_c = fragment_m[read_pointer_q];     // COMBINATIONAL read
...
fragment_m[write_pointer_q] <= '{ ... };        // clocked write
```

An `assign` read of an array that an `always_ff` writes forces the bypass. **This
is the same shape as the `uvw_m` fix earlier in this campaign** — moving that
read out of an `always_ff` took the composed shell from 61.52 to 66.03 MHz — and
it is the third time a memory's read structure has turned out to be the binder.

Note also the **clock path is 7.965 ns**, of which 2.454 ns is
`gpu_clk~CLKENA0|outclk` to that RAM's `clk0` and 2.463 ns is the RAM's own
clock-to-out. Before any data moves, eight nanoseconds are spent. Some of that
is unavoidable insertion delay; the 2.463 is the bypass-laden RAM.

### The candidate fix, and the condition it must satisfy

**Tell Quartus there is no read-during-write.** If the design never needs the
new data when reading the address being written, the bypass mux and the
write-enable arc both disappear. On this toolchain that is `ramstyle =
"no_rw_check"` on the array.

**The condition is checkable and is probably already true.** Read-during-write
can only occur when `read_pointer_q == write_pointer_q`, which for this queue
means EMPTY. And the neighbouring lookups in the same file are already gated on
exactly that:

```systemverilog
wire head_aux_pending_c = !queue_empty_c && aux_pending_m[read_pointer_q];
```

`head_c` itself is ungated at line 134, but its consumers build `sample_job_c`,
which is only offered when the queue is non-empty. **So the value returned
during a read-during-write is very likely already a don't-care** — and that is
an assertion to write and run, not an argument to accept. Verilator answers it
in seconds: assert that `head_c` is never consumed in a cycle where
`read_pointer_q == write_pointer_q` and a write is landing.

**If that assertion holds, this is a one-attribute change worth ~2.5 ns on the
machine's gating path.** If it does not, the fix is to gate the read or register
it, which costs a cycle and is a protocol change — and then it belongs with
cone 2's remaining 3.4 ns in the "needs a design pass" pile rather than in a
timing round.

### Cone 4's precondition, worked out: it holds at EMPTY and is only GUARDED at FULL

Done by reading, since the assertion cannot be written while a suite has the
tree. The result changes the recommendation, so it was worth doing first.

**Read-during-write needs `read_pointer_q == write_pointer_q`.** The pointers
are `FQPW = $clog2(FQD)` bits — exactly two for `FQD = 4`, **no extra
wrap bit** — and emptiness is tracked separately in `occupancy_q`. So the
pointers coincide in **two** states, not one:

**At EMPTY, the don't-care is real and structural.**

```systemverilog
wire queue_empty_c   = occupancy_q == '0;
assign sample_valid_o = !queue_empty_c && (|head_sample_pending_c);
```

Every `sample_*_o` output is driven unconditionally from `head_c`, but the
handshake's `valid` is gated on `!queue_empty_c`. A write landing while empty
does not change `occupancy_q` until the next edge, so `sample_valid_o` is low in
exactly the cycle the bypass would matter. **Whatever the RAM returns is
ignored.**

**At FULL, it is not structural — it is a guard.** With two-bit pointers,
`write_pointer_q == read_pointer_q` again when the queue holds four, and there
`queue_empty_c` is false, so `sample_valid_o` *can* be high. The only thing
preventing a meaningful read-during-write is that `queue_full_c` blocks the
write.

That guard is almost certainly correct — and this project already knows it is
load-bearing. CLAUDE.md's own case study is **this block**: `wq_overflow_o` is
the counter that *"cannot be reached with legal stimulus while the full-guard is
correct"*, which is why `tests/mutants/zhao_texture_frag_expand_mutant.sv` was
committed to fire it.

### So the recommendation is two changes, not one

**`ramstyle = "no_rw_check"` alone converts a guarded condition into a silent
correctness dependency.** Today, if the full-guard broke, a counter would
increment and a mutant-verified detector would say so. With `no_rw_check` and a
broken guard, the queue would instead return stale data to a live `sample_valid_o`
— one wrong fragment, no counter, no alarm. That is a strictly worse failure
mode bought for timing.

**Add the wrap bit first.** Widen the pointers to `$clog2(FQD) + 1`, index the
array with the low bits, and compare all bits for equality. Then
`read_pointer_q == write_pointer_q` happens **only** when empty, the don't-care
is structural rather than guarded, and `no_rw_check` is unconditionally safe.
Cost: two flip-flops.

That is the standard circular-queue idiom and it is cheap. It also leaves
`occupancy_q` alone, so `queue_empty_c`, `queue_full_c` and every counter keep
their current meaning and their current tests.

### What the next session should do, in order

1. Widen the pointers by one bit; keep `occupancy_q` as the authority for
   empty/full so no existing gate changes meaning.
2. Write the assertion anyway — `head_c` is never consumed while the pointers
   are equal and a write is landing — and watch it pass. A structural argument
   that has not been run is still an argument.
3. Then `ramstyle = "no_rw_check"` on `fragment_m`, and fit it with cone 3,
   which is still uncashed.

**Worth ~2.5 ns on the gating path**, and unlike cone 2's remainder it needs no
protocol change — just a pointer bit that should arguably have been there
anyway.

### Why it is not being done in this session

`zhao_texture_frag_expand_v2.sv` is in the shell closure, which is not a
constraint — block fits snapshot. The actual reason is the honest one: **the
assertion has not been written or run**, and `no_rw_check` on an array whose
don't-care has not been proven is precisely the kind of change that passes every
gate and corrupts one fragment in a million. The next session should write the
assertion first.

---

## CONE 3 REACHES 124 OF 200 PATHS, NOT 30 — AND THE NEXT ROUND IS THREE CHANGES

Measured by walking every Data Arrival Path in `@packet-h-texorder` and asking
which pass through `local_fault_pulse_o`:

```
124 of the 200 printed paths, slack -1.954 .. -1.369, across ten endpoints:

   32  expected_sequence_q[]        5  acc_mask_r[]
   20  walk_q_r[]                   5  owner_err_stale_base_q[]
   17  owner_err_range_base_q[]     4  col_r[]
   13  dispatch_mismatch_base_q[]   3  owner_err_unsol_base_q[]
    8  metadata_genmis_base_q[]     3  owner_err_dup_base_q[]
```

I had credited cone 3 with `walk_q_r`'s 30 paths, because that is the endpoint
the receipt ranked first. **It is 62% of the window.** The fault-OR feeds
`abort_now_w`, which cancels the candidate skid, which frees `attr_join_room_w`,
which drives `attr_q_ready_w` — and that ready gates the entire attribute path,
so one combinational cone sits in front of everything downstream of it.

The tell I missed: grouping by DESTINATION is the right key for ranking a fix,
and it is the wrong key for sizing one. A cone that fans out to ten endpoints
looks like ten small problems.

### What this does and does not change

**It does not change the clock forecast, and my first attempt at saying so was
itself wrong.** I wrote that cone 3 alone would move `gpu_clk` to about
82.6 MHz. It moves it to **79.74 MHz — that is, not at all.** Those 124 paths
span -1.954 to -1.369, and the design's worst path is `read_row_present_q` at
**-2.540**, which is cone 4's and is outside the set entirely. Removing every
one of the 124 leaves -2.540 exactly where it was.

Twice in one page: first sizing a cone by its top-ranked endpoint, then
forecasting a clock from a tier that does not contain the worst path. Both are
the same slip — reading a ranked list as though the thing at the top of one
grouping were the thing at the top of the design.

**It changes what cone 3 is worth on TNS**, which is -2,077 and is the number
that actually measures how much of the design is late. 124 of 200 printed paths,
and the printed window is the tip — the same proportion below it would be
thousands.

### So the next round is three changes, shipped together

| | change | status | what it removes |
|---|---|---|---|
| 3 | binner decides the profile verdicts at write | **committed**, unfitted | 124 of 200 paths, -1.954 .. -1.369 |
| 4 | pointer wrap bit + `no_rw_check` on `fragment_m` | analysed, not started | the -2.540 worst path and -1.982 |
| 5 | `mul_x_c` / `mul_y_c` as a structural 12-bit product | analysed, not started | 26 paths at -2.025 |

**Any one alone is worth almost nothing to the clock; all three together should
clear the printed window.** That is the batching law arriving as a measurement
rather than as advice, and it is the first time in this campaign the next fit's
contents have been decidable in advance from the receipt rather than guessed.

Ordered by risk, lowest first: cone 5 is bit-exact arithmetic with a
differential already watching it; cone 4 needs one pointer bit and an assertion;
cone 3 is already done and needs nothing.

### The three-change round, SIZED — 156 of 200 paths, and the wall after it

Measured with the committed parser rather than another ad-hoc regex, pairing
each of the 200 `Summary of Paths` rows with its own `Data Arrival Path`
section, and asking of each path whether it passes through the cone in question:

| | cone | paths | worst | best |
|---|---|---:|---:|---:|
| 3 | the fault-OR (`local_fault_pulse_o`) — **committed, unfitted** | **124** | −1.954 | −1.369 |
| 5 | the attrgrad multiply (`mul_x_r` / `mul_y_r`) | **28** | **−2.025** | −1.384 |
| 4 | the fragment RAM's read-during-write bypass | **5** | **−2.540** | −1.442 |
| | **union** | **156 of 200** | | |

**44 paths survive all three, and their worst is −1.879** — eight paths ending
at `cfg_rsp_generation_q`, then `cfg_rsp_op_q` at −1.657, `o_a0` at −1.646,
`div_in_ru_q` at −1.383.

So the forecast for the three-change round, stated as arithmetic rather than
hope:

```
now                     gpu_clk 79.74 MHz   worst -2.540
after cones 3 + 4 + 5   gpu_clk ~84.2 MHz   worst -1.879   (1000 / (10 + 1.879))
```

**And that is why all three must ship together, now demonstrably.** Cone 4 owns
the worst path but only five paths; cone 5 owns the second-worst and 28; cone 3
owns 124 but none worse than −1.954. Ship cone 3 alone and the clock does not
move at all. Ship cone 4 alone and it moves to 1000/(10+2.025) = 83.2 but leaves
124 late paths and most of the TNS. Only the union both moves the clock and
drains the band.

### What it does not reach

**~84 MHz is not 100**, and the 44 survivors have no dominant endpoint — the
largest group is eight paths. The gap from 84 to 100 is a band starting at
−1.879 with nothing in it worth a dedicated cone, which is the same shape the
texture band had at −2.5 and needed three cones to clear.

The honest projection for the remaining 16 MHz is **several more rounds of three
or four small cones each**, each round worth two to four MHz, found the way
these were: trace the receipt, group by destination to rank, then walk the
arrival details to size. That loop now has a tool for the first half
(`setup_path_census.py`) and none for the second — sizing a cone by how many
paths pass through an intermediate node is what I got wrong twice today, and it
is the obvious next addition to that tool.

---

## WHAT EACH CONE IS ACTUALLY WORTH, AND TWO CORRECTIONS TO WHAT I WROTE ABOVE

Computed by removing each subset from the 200 printed paths and taking the new
worst:

| cones landed | worst | `gpu_clk` | paths removed |
|---|---:|---:|---:|
| none | −2.540 | **79.74** | 0 |
| 3 alone | −2.540 | 79.74 | 124 |
| 5 alone | −2.540 | 79.74 | 28 |
| **4 alone** | −2.025 | **83.16** | **5** |
| 3 + 4 | −2.025 | 83.16 | 129 |
| 4 + 5 | −1.954 | 83.65 | 32 |
| 3 + 4 + 5 | −1.879 | 84.18 | 156 |

**Cone 4 — five paths, and the smallest change of the three — is worth +3.42
MHz on its own. The other two add 1.02 MHz between them.** I wrote earlier that
"all three must ship together"; that was right about cone 3 and cone 5 being
worthless alone, and wrong about the shape. Cone 4 is both the cheapest and
almost all of the value. Cone 3 is worth TNS (−2,077 over 124 of 200 paths) and
no clock; cone 5 is worth 0.5 MHz on top of 4+3 and costs a latency change in
two modules.

### Correction: the extra pointer bit does NOT make `no_rw_check` safe

I recommended widening `fragment_m`'s queue pointers by one bit so that
read-during-write becomes structurally impossible. **It does not.** The RAM's
address is the LOW bits of the pointer, and at FULL those are equal whether or
not there is a wrap bit — `rp = 0, wp = 4` still addresses entry 0 twice. The
extra bit distinguishes full from empty in the LOGIC; it does not change what
address the memory sees.

So `no_rw_check` remains dependent on the full-guard blocking the write, with or
without it. The two flip-flops buy nothing here and the recommendation is
withdrawn.

### And the dependency is not a new one, which changes the verdict

I also wrote that `no_rw_check` "converts a guarded condition into a silent
correctness dependency". Working it through: if the full-guard ever failed and a
write landed while full, it would overwrite the entry at `read_pointer_q` —
**destroying the live head**. That is already a correctness failure, today,
without any attribute. `no_rw_check` does not make correctness depend on
anything that it did not already depend on; it changes what the *symptom* looks
like for a fault that is fatal either way.

And the guard is not un-evidenced: `wq_overflow_o` has a committed mutant
control, `tests/mutants/zhao_texture_frag_expand_mutant.sv`, written precisely
because the state is unreachable while the guard holds — and this session's run
of `test_forge_cliff_ram_mutant_control` is a reminder that those controls do
fire when asked.

**So cone 4 is a one-attribute change, plus an assertion for the empty case that
should be written and watched to pass.** The pointer work is off the list.

### The revised order

1. **Cone 4** — `ramstyle = "no_rw_check"` on `fragment_m`, plus the assertion.
   +3.42 MHz, one line of RTL.
2. **Cone 3** — already committed. Fits alongside for the TNS.
3. **Cone 5** — deferred. 0.5 MHz for a pipeline stage in both `attrgrad_v2`
   and `attrgrad_dsp3`, because the differential compares them cycle by cycle
   and a stage that moves an output edge is visible to it. Not worth it yet.
