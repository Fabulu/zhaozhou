# ESCALATION: the console does not fit ANY Cyclone V, and 23% was the wrong question

Coordinator, 2026-09-28. Raised under the vacation directive's own instruction
that a measured conflict is escalated rather than resolved locally. Phase 1 of
the standing goal is complete (the register reads zero); this is about phase 2.

## The measurement

`zhao_console_core`, mapped at HEAD on the shipping part, A&S successful,
0 errors, `rtlCleanAtHead` at `83c63661`:

| | measured | against `5CSEBA6U23I7` |
|---|---:|---:|
| **estimated ALMs** | **222,666** | **531%** |
| combinational ALUTs | 293,886 | 351% |
| registers | 279,210 | 167% |
| block memory bits | 3,387,975 | 60% |
| DSP blocks | 128 | 114% |

The placed fit died on `Error (170011)`: 336,023 combinational nodes against
**227,120**, and 227,120 is the largest die this installation has. Only
`cyclonev` is installed and the biggest Cyclone V part in existence is the
~113,560-ALM class.

**So the design is 5.3x the SHIPPING part and about 2x the LARGEST CYCLONE V
EVER MADE.** No device in this family can place it, at any price.

## Why "shed 23%" was the wrong framing, including mine

I spent this session hunting for 66,766 ALUTs -- the gap to the *sizing* part,
which would only buy a diagnostic number, not a shippable console. **The gap to
the part we actually ship is far larger: 222,666 ALMs against 41,910.** Closing
*that* is not an optimization programme; it is a different machine.

And the optimization levers are genuinely spent. Six candidate classes were
measured against the current map this session:

| class | verdict |
|---|---|
| deep arrays in flip-flops | **EXHAUSTED** -- every one already infers as memory |
| small arrays | correctly in flops (`ladderbank` is 32 entries deep) |
| ROMs | `field_rcp24_rom` needs a PIPELINE STAGE at 13 call sites |
| retained-oracle path | already generate-gated; the binner is load-bearing |
| instance specialisation | already taken by Quartus constant propagation |
| duplicated instances | **~4,000 ALUTs** genuine, not the 40,091 I first counted |

**Even the forbidden lever does not reach.** Cutting raster lane parallelism --
which the directive does not authorise -- would free about 4,000 ALUTs from
`attrgrad_v2`/`attrdiv_v2` at half the lanes. Against 66,766, let alone against
the real gap, it is noise. **There is no feature cut that makes this fit a
Cyclone V**, which means the lanes question I was about to put to you is not
worth your time either.

## The reframe, and it comes from your own stated destination

The recorded destination for this project is **fabricated silicon -- a physical
console** -- with FPGA as *a lane, not the destination*. If that still holds,
then **"does the whole console fit one Cyclone V" may be the wrong acceptance
question**, and has been for some time. A design at 222,666 ALMs is unremarkable
for an ASIC; it is simply far past what this FPGA family can hold.

That would make the FPGA a **per-subsystem validation vehicle** rather than a
whole-console one -- which is, in practice, exactly how this campaign has been
working: every real measurement of the last week (FLOPARRAY, the block census,
the ROM and array work) came from **standalone `-MapOnly` fits**, not from a
console fit, because the console fit has never once completed.

## What I need from you

**1. Is whole-console FPGA placement still an acceptance criterion?**
If yes, this is an architecture-scale reduction programme -- roughly a 5x cut --
and it needs your direction on what the console is allowed to stop being.
If no, then phase 2 of the standing goal should be **redefined** as the
per-subsystem fit set plus the whole-console synthesis estimate, both of which
exist today, and the answer to "where do we stand" is the table above.

**2. If whole-console placement matters, what is it FOR?**
Bring-up on the SuperStation One, a demo, or timing evidence? Each implies a
different reduced target -- a subsystem, a reduced-parameter console, or a
feature-gated build -- and I can produce any of them honestly. What I cannot do
is produce a full-capability console that places on this silicon.

**I am not asking you to choose an optimization; I am asking whether the
acceptance criterion is the right one.** The directive is explicit that a
measured engineering impossibility is a finding rather than permission to invent
a pass, and that reduced work must never be called equivalent to reach a number.
This is that finding, stated before any capability was touched.

## What is NOT blocked and continues regardless

* The register is at **zero** and every gap entry is closed on measured
  evidence.
* Closing it **cost no area** -- ALUTs fell 301,446 -> 293,886 across the work,
  DSP did not move.
* The ~4,000 ALUTs of genuine cross-subsystem duplication
  (`field_v3_normalize`, `field_isqrt`, `geom_mat3x4_mul`) remain available as
  an ordinary packet whenever it is wanted, verified standalone.
* `lane_desync_o` is diagnosed and owed a repair (its arm (a) asserts an
  invariant the console's wiring does not supply).
