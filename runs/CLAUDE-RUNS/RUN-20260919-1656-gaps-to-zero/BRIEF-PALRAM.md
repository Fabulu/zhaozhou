# PALRAM — 98,304 flip-flops that were supposed to be ten M10K

**Branch `gz/palram`. Your own worktree. Push ONLY your branch. I merge.**
Protocol: `PACKET-PROTOCOL.md` in this folder. Read it before you touch anything.

**This is the first packet of the standing goal's phase 3 — damage control and
optimization — and it is the largest single item on the board.**

## The measurement

The full console fit of 2026-09-25 failed in `quartus_fit` with
*"Design contains 336023 blocks of type combinational node. However, the device
contains only 227120 blocks."* — so no placed ALMs and no Fmax. **But
`quartus_map` SUCCEEDED**, and its entity table survives. Run the probe yourself:

```
python tools/budget/map_entity_attrib.py \
    "reports/synthesis/blockpaths/zhao_console_core@edgeclose.map.rpt" \
    --top 24 --drill zhao_geom_drawjob:u_geom_drawjob
```

`reports/synthesis/console_entity_attrib.md` is its committed output, and
`reports/HANDOVER-20260919.md` §15.20 is the write-up. The shape:

| | measured | against `5CSEBA6U23I7` |
|---|---:|---:|
| combinational ALUTs | 294,872 | **352%** |
| dedicated logic registers | 405,872 | **242%** |
| block memory bits | 3,009,171 | **53%** |

**The registers alone need 101,468 ALM — 242% of the device with the
combinational logic at zero.** Memory fits with half the part spare. The
overflow is storage held in flip-flops, and M10K is the slack.

## Your target

**`zhao_geom_drawjob` has NO hierarchy under it at all, holds 100,561 registers
and ZERO block memory bits.** That is **60% of the entire shipping part's
register capacity in one leaf.**

`fpga/rtl/geometry/zhao_geom_drawjob.sv:235` —

```systemverilog
logic [383:0] pal_q [XFORMS];   // XFORMS = 256  ->  98,304 bits
logic         pal_v_q [XFORMS];
```

98,304 + 256 = **98,560 of the 100,561**.

## THE COMMENT ABOVE IT ASSERTS THE THING THAT DID NOT HAPPEN

Read `:229-234` before anything else:

> *"the palette. **A REGISTERED READ WITH AN ENABLE, so the row holds for the
> whole draw and Quartus infers M10K rather than 98,304 flops.** The valid bits
> are separate flops because they must be CLEARED by reset and an asynchronous
> clear on the array would destroy that inference…"*

and `:402`:

> *"The array itself has NO reset, which is what lets it infer M10K."*

**The author wrote the exact number — 98,304 flops — as the thing that would NOT
happen, and that is exactly what happened.** Every precaution in those comments
is correct and present: no reset on the array, a registered read with an enable,
the valid bits split out. The RTL looks like the textbook template. **Quartus
still put it in flip-flops.**

This is the broken-instrument law in a source comment. An assertion about a
synthesis outcome, written at authoring time, never checked against a synthesis
result, and cited ever since as though it were one. **Your first deliverable is
to make that comment either true or honest.**

## What the report does and does not say about WHY

`zhao_geom_drawjob|pal_q` **is not in the map report's uninferred list at all**,
while a dozen sibling arrays are, each with Quartus's reason attached:

```
RAM logic "zhao_twod_plane:u_twod_plane|pal_q"              ... inappropriate RAM size
RAM logic "zhao_twod_sampler:u_twod_sampler|bind_base_q"    ... asynchronous read logic
RAM logic "zhao_terrain_lodshare:u_terrain_lodshare|idq_ix_q" ... asynchronous read logic
RAM logic "...zhao_texture_material_combine_v3:u_combine|newq_m" ... inappropriate RAM size
```

**Quartus never considered it a candidate and did not say why.** I am not
diagnosing that from a report and neither should you — **run the experiment.**
`tools/quartus/run_block_fit.ps1` has a `-MapOnly` mode whose entire purpose is
this; a single module's map is fast. Candidates worth separating, in no
particular order and none of them my conclusion:

* **the width.** 384 bits × 256 deep. A Cyclone V M10K is at most 40 bits wide
  in simple dual-port, so this is ~10 blocks ganged. That is legal and common,
  and it is also exactly the shape `inappropriate RAM size` describes elsewhere.
* **the write.** `:406` is a 12-iteration `for` loop of 32-bit partial-word
  assignments (`pal_q[i][32*k +: 32] <= px_m_i[k]`), which may model as twelve
  byte-enabled writes rather than one word write.
* **the two ports sharing one `always_ff`.** The write and the read at `:404-408`
  are in the same block, which some templates require to be separate.
* **the index expressions.** `int'(px_index_i)` on write against
  `int'(xf_idx_q[XIDXW-1:0])` on read.

**Change ONE thing at a time and map after each.** The whole value of this packet
is a causal answer, not a working array — "I changed four things and it inferred"
teaches the next person nothing and will not survive the next edit.

## The prize, and how to state it honestly

98,304 registers ÷ 4 per ALM ≈ **24,576 ALM, about 59% of the shipping part**,
bought for ~10 M10K out of roughly 259 free. The worked precedent is EARTHRAM:
one 5,120-bit array out of flops for **−2,698 estimated ALMs, −1,531 ALUTs,
−5,181 registers at zero added cycles**, costing 4,880 M10K bits.

**Quote the measurement, not the arithmetic.** The register count is a synthesis
number and the ALM figure above is a division I did on it. Your evidence is a
**before/after `quartus_map` of `zhao_geom_drawjob` alone**, both rows recorded,
and the M10K count from the RAM Summary. This design has never placed, so there
are no ALM figures and no Fmax anywhere in this work — do not produce one.

## WHAT YOU MAY NOT DO, AND IT IS THE OBVIOUS THING

**`XFORMS = 256` IS A RATIFIED CONTENT TIER.** The parameter's own comment:
*"The ruling's content tier is 256 creatures; a transform id at or above this is
REFUSED, never wrapped."* The refusal is a feature — `pal_dropped_o` counts a
node GEOM.LOOM composed that no draw can name, *"counted rather than wrapped: a
wrapped index would overwrite another instance's transform, which is silently
wrong geometry."*

The owner's directive, verbatim: *"This is NOT authority to delete a feature,
reduce 16 fields to 4, remove Gouraud/detail normals, shrink the guaranteed
giant, replace a live path with testbench stimulus, waive a correctness failure,
or call reduced work equivalent merely to reach zero or fit a device."*

**Cutting XFORMS is that move exactly**, and it is the one an area packet reaches
for first. So are: narrowing the 384-bit transform, dropping `pal_v_q`, making
the refusal a wrap, or "temporarily" parameterising the tier down for a
measurement and leaving it. **If you believe the tier has to move, STOP and write
it up — it is the owner's call, not yours and not mine.**

**The conversion must be behaviour-identical.** Same read latency (the current
read already lands in a flop one cycle later, so an M10K registered read costs
nothing), same refusal semantics, same counters, same `pal_writes_o` /
`pal_dropped_o`. If the conversion changes a cycle anywhere, say so in numbers
and justify it; do not let it pass as free.

## Evidence bar

* **A before/after map row for `zhao_geom_drawjob`**, and the RAM Summary lines
  showing the blocks it now infers.
* **Equivalence, not just inference.** The existing `zhao_geom_drawjob` tests
  must pass unchanged, and you should add one that writes and reads the palette
  across the full 256 range including the refusal boundary at index 256. Assert
  the CORRECT behaviour; keep any positive control separate; do not write a test
  that asserts the bug.
* **Verify the whole-console effect is what you claim** by re-running the probe
  against a fresh map of `zhao_console_core` ONLY IF I have told you a fit slot
  is open. **Do NOT start a console fit or a full-device fit on your own** — that
  is 01:54:49 and it is mine to schedule. A `-MapOnly` run of the single block is
  yours and is expected.
* If a guard you touch becomes unreachable with legal stimulus, it needs a
  **committed mutant** under `tests/mutants/`, renamed so no source list
  elaborates it, driver polarity inverted so it passes when the counter FIRES.

## Traps

* **`mutant_copy_drift` keys on COMMIT TIME.** I counted: `zhao_geom_drawjob.sv`
  has **zero** committed mutant copies, so this one does not bite you — but
  verify it before your first edit rather than taking my count, and if you touch
  any OTHER file, check its count first. Editing a file even in a comment stales
  every copy of it, and reverting does not clear it.
* **THE GATES DO NOT BUILD.** `gate_sweep.py` returning 0 means *nothing moved
  against the baseline*. Run `cmake --preset windows-native` yourself, from
  PowerShell with `tools/env/zhao-env.ps1` sourced. **Read the build's exit code,
  not the pipeline's.**
* **After any struct-layout change, recompile every `.cpp` that uses it.** A
  stale object with an old layout looks exactly like a rendering bug.
* **Verilator lint-clean is not Quartus-synthesizable.** `:207` already carries
  the scar — the elaboration guard sits inside `initial begin ... end` because a
  module-scope `if` is a syntax error in Quartus 17.0 and `--lint-only` never
  runs the block. Whatever you write, map it.
* **`--lint-only` does not run `initial` blocks**, and
  `// synthesis translate_off` does NOT make Verilator skip a block.
* **One `ctest` at a time per build tree**; after killing one,
  `rm -f build/Testing/Temporary/CTestCheckpoint.txt
  build/Testing/Temporary/LastTest.log.tmp*` before restarting.

## Deliverable

Commit as you go. Your final commit message is your FINDINGS. State:

1. **WHY Quartus did not infer it**, as a causal answer from a one-change-at-a-
   time experiment, with the map rows for each step.
2. **The before/after numbers** for `zhao_geom_drawjob`: registers, ALUTs, M10K
   blocks, memory bits. Measured, not divided.
3. **What you changed in the RTL**, and the proof it is behaviour-identical.
4. **The comment at `:229` and `:402`** — now true, or now honest. Say which.
5. **Anything you refused**, especially anything that would have moved `XFORMS`
   or the refusal law.
6. **Anything you got wrong and caught yourself.**
7. The branch and commit hash. **Push `gz/palram` only.** Never rebase or push
   the shared branch, and never `--force`.
