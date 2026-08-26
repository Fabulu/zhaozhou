# FIELD v2 — the register file's port count, and why the fit may have failed

> Written 2026-08-26 while `quartus_map` on `zhao_field_v2_front` is still
> running. **Nothing here is measured.** It is the enumeration a fix would be
> built from, written before the report lands so that the report can refute it
> rather than be read to fit it.

## The trigger

`quartus_fit` on `zhao_field_v2_front` ran 5,747.8 s and exited non-zero. The
harness reports budget kills as `timeout` and this was not one, so the tool
errored. The workspace was deleted with the log (`-KeepWorkspace` was not
passed), so the message is gone and the re-run is what will say why.

## What the file is asked to do, counted from the RTL

`fpga/rtl/field/zhao_field_v2_core.sv:177`

```systemverilog
logic signed [31:0] rf [LANES][0:(1<<RFAW)-1];   // 4 lanes x 512 words x 32b
```

### Readers, per lane

| reader | line | when |
| --- | --- | --- |
| `rd_a` | 579 | every issue |
| `rd_b` | 580 | every issue |
| `rd_c` | 581 | every issue |
| `h_rd` | 582 | host read port |

### Writers, per lane

| writer | line | addresses written in ONE clock |
| --- | --- | --- |
| long-op reply | 677–679 | up to **three**: `dst`, `dst+1`, `dst+2` |
| ALU write-back | 708 | one, arbitrary |
| host write | 706 | one, arbitrary |

Naively that is 4 read ports and 5 write ports. Both numbers come down.

## The host port is excluded by protocol, and this is currently UNDECLARED

`zhao_field_v2_front.sv` drives `h_we` only inside `F_FILL` (line 277) and reads
`h_rreg`/`h_rwf` only while draining. The core executes in `F_RUN`. So the host
port and the execution ports are never active in the same cycle.

**That exclusion is real and it is nowhere stated in the core.** The core does
not enforce it, does not assert it, and its port list does not mention it. Any
other instantiation that writes through the host port while wavefronts are
running gets a fifth concurrent writer and a register file that cannot be built
— and nothing in the tree would say so.

Declaring and asserting it is a change to `zhao_field_v2_core.sv`, which is in
the cone the map is reading right now (QUARTUS_GOTCHAS §11 — the fit reads the
live tree), so it is deliberately NOT being made yet.

With the host excluded, the requirement in `F_RUN` is **3 reads and up to 4
writes** per lane, and outside it, 1 read and 1 write.

## Why the four writes are not as bad as four writes

The three reply writes are at **consecutive** addresses. Bank the file by the
low two bits of the register index and `dst`, `dst+1`, `dst+2` land in three
*different* banks, always, for every `dst`. They can never contend with each
other.

That leaves the ALU write-back, at an arbitrary address, which can collide with
exactly one of the three. Per bank the worst case is therefore **two** writers,
not four — and the second one is avoidable, because a long-op reply and an ALU
write-back colliding in the same bank is rare and a one-cycle hold on the ALU
write costs a bubble the interlock machinery can already express.

If that holds, each bank is **1 write, 3 reads**, which is the shape
`zhao_probe_banked_rf` already fit: 375 ALMs, 12 M10K, 96.5 MHz.

### The arithmetic, to be checked against the report and not before

* per lane: 512 words ÷ 4 banks = 128 words × 32 b = 4,096 b → one M10K
* three read ports by replication → 3 M10K per bank
* × 4 banks = 12 M10K per lane
* × 4 lanes = **48 M10K of the 553 on the device**

The probe's measured 12 M10K matching one lane's figure is encouraging and is
*not* evidence — the probe is a different module and may have been parameterised
differently. It is listed so the report can contradict it.

## The failure mode this predicts

Storage that cannot map onto a memory becomes flip-flops and multiplexers.
512 × 32 × 4 = 65,536 flops against roughly 83,820 on a 5CSEBA6U23I7, before
the ALU, the shared units, the sequencer or the front end. That does not fit,
and not fitting is a failure that surfaces deep into placement — which is where
this one surfaced.

## What the map report has to answer

1. Did `rf` infer as RAM, or as registers? (`Total block memory bits`,
   `Total registers`.)
2. If registers — how many, and does the total exceed the device?
3. If RAM — then this hypothesis is **wrong** and the fit failed for another
   reason, and the DSP/ALM lines are where to look next.

Question 3 is the one that matters most. A tidy story that survives because
nobody checked the alternative is the failure mode this file exists to avoid.

## What is NOT in question

The throughput architecture does not depend on the storage shape. The second
read pass, the multi-result reply, the captured-unit interlock and the
multiplier priority chain are all unaffected by whether the file is one memory
or four banks. If this is the defect, it is a rewrite of the storage and its
write arbitration, not of the engine.

Every FIELD v2 throughput figure in this repo — 3.97 vertex-instructions per
clock, 27.8× v1 — is a **simulation** figure and stays one until the block
places.
