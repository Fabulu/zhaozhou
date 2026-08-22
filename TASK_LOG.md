# Task log

Engineering-side record: what was run, against which commit, and what the
evidence actually was. `STATUS.md` is the owner-facing channel and stays prose;
this file is the audit trail and stays exact.

Newest entry at the top. Every claim here names the command that produced it.
**Everything in this file is simulation, synthesis or fit. No hardware has run
any of it.**

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
