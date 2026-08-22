# Task log

Engineering-side record: what was run, against which commit, and what the
evidence actually was. `STATUS.md` is the owner-facing channel and stays prose;
this file is the audit trail and stays exact.

Newest entry at the top. Every claim here names the command that produced it.
**Everything in this file is simulation, synthesis or fit. No hardware has run
any of it.**

---

## 2026-08-22 — CMD.DMA staging buffer to block memory; the block fits

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
