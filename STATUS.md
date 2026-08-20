# Status — for Fabian

*This file is the channel. I update it and push; you read it here. Newest section
at the top.*

---

## 2026-08-20 08:15 — read the redesign proposal. It is right and my answer was wrong

`reports/CMD.DMA_Redesign_Proposal.md` arrived on origin. I have read it in full.
**Adopt it.** It is better than what I proposed, and it identifies a soundness
hole in my suggestion that I had not seen.

### Where I was wrong

I recommended **two passes over DDR** — read once to CRC, read again to commit —
on the grounds that it costs no on-chip memory. The proposal points out that
this is **not sound**: the pixel arena is a raw HPS address with no descriptor,
lease or ownership state, so HPS can mutate it between the two passes.
Pass 1 verifies bytes A, pass 2 commits bytes B, and the CRC certifies nothing.
A classic time-of-check/time-of-use hole, and I walked straight into it.

Making two-pass sound would first require a sealed pixel-arena descriptor that
forbids HPS writes in between — inventing an ownership mechanism to rescue a
design that then still costs double the bandwidth.

### Why its answer is better

It moves the atomicity boundary to where it actually is. The current law says
*no byte is written to VRAM before the CRC passes*, which is what forced a
whole-canvas buffer. But **writes to an inactive, uncommitted framebuffer slot
are not visible to anyone.** The externally meaningful commit is the slot
becoming READY.

So: lease an invisible slot, stream into it one 64-byte chunk at a time, CRC on
the fly, and publish READY only after every write has retired and the CRC
matches. Fail, and the slot is released FREE and never published.

**512 bits of buffer instead of 1,966,080.** Single pass. And it is immune to
the mutation problem — if HPS changes the source mid-read, the CRC fails and the
dirty slot is never shown.

That is the better argument, and it wins on correctness rather than on cost.

### What tonight's synthesis run adds to it

The run finished after the proposal was written, so this is new evidence for it,
and it confirms two of its rules from the tool's own mouth:

- Quartus named **asynchronous read** as the *only* stated reason `blit_buf` is
  uninferred. The proposal's synthesis rules list "no combinational array reads"
  — that is precisely the one that bites.
- **A second block has the identical defect**, outside CMD.DMA entirely:
  `zhao_scanout_linebuf` line 96. The proposal's rule list should be applied
  there too; it is not a CMD.DMA-specific mistake but a house-wide one.
- `zhao_audio_fifo` **did** infer, as a dual-clock RAM, carrying the warning
  that read-during-write behaviour of a dual-clock RAM is undefined and may not
  match the original design. Worth checking separately — that is a correctness
  risk wearing a success.

### Its other two findings, which I had not looked for and which look right

- **The contract claims 1 MiB packets; the RTL caps at 4 KiB** and rejects
  anything larger. There is already a hidden effective maximum.
- **The 1 MiB-per-frame target is physically impossible.** 16,384 bursts at
  (16 + 8) cycles is 393,216 GPU cycles against 217,984 in the shortest mode
  frame. The HPS bridge cannot deliver it regardless of what CMD.DMA does. That
  belongs in the same category as the DSP overrun: a number nobody costed.

### What I have and have not changed

The 64-bit repack I landed (`3a97980`) stands as the emergency cleanup it is:
16.2 GB to 2.7 GB elaboration, all lanes green, and it is what let the composed
synthesis produce a named error at all. It does **not** solve the design
problem, and the proposal says so correctly.

I have **not** started the redesign. It splits a landed block in two, adds a
framebuffer-slot lease with a real state machine, changes the decoder-facing
stream from 8 to 64 bits, and amends a ratified law. That is a wave of work and
it should start deliberately.

---

## 2026-08-20 08:00 — THE COMPOSED SYNTHESIS RUNS. 28.4 GB to 6.2 GB, and Quartus names the fault

The composed shell no longer thrashes. It ran analysis and synthesis to
completion and **failed with a precise, named error** instead of dying:

```
Peak virtual memory: 6352 megabytes      (was 28,400)
Elapsed: 42:33

Info (276014): Found 2 instances of uninferred RAM logic
  Info (276007): RAM logic "zhao_cmd_dma:u_dma|blit_buf" is uninferred
                 due to ASYNCHRONOUS READ LOGIC ... line 239
  Info (276007): RAM logic "zhao_video_scanout:u_scanout|
                 zhao_scanout_linebuf:u_linebuf|mem" is uninferred
                 due to ASYNCHRONOUS READ LOGIC ... line 96
Error (276003): Cannot convert all sets of registers into RAM megafunctions.
  The resulting number of registers remaining in design exceeds the number
  of registers in the device.
```

**This is the whole answer, from the tool, in its own words.** Not a guess.

### What it tells us that we did not know

1. **The fix is SMALLER than I said.** I wrote that `blit_buf` needed both the
   async reset removed *and* a registered read. Quartus lists **only the
   asynchronous read**. One change, not two.
2. **A SECOND block has the identical defect.** `zhao_scanout_linebuf` line 96,
   `logic [63:0] mem [0:1][0:127]`, same cause. It was never going to infer
   either, and nobody knew.
3. **`zhao_audio_fifo` DID infer**, as a dual-clock RAM — with a warning worth
   following up: *"the read-during-write behaviour of a dual-clock RAM is
   undefined and may not match the behaviour of the original design."* That is a
   correctness risk hiding inside a success.

### Where that leaves it

Both remaining faults are the same shape and both are ordinary RTL work: give
the memory a registered read and let the consumer take the extra cycle. For
`blit_buf` that means reading beat *n+1* while beat *n* is on the wire, which
the beat counter already makes natural.

I have not done it. It changes protocol timing on two landed blocks and wants
doing carefully rather than at the end of a long session — and the open design
question below may make the `blit_buf` version moot.

**Route to a composed fit, in order:** registered read on those two memories →
synthesis passes → the fitter finally reports real composed ALM/DSP/M10K
numbers → Phase 9 can start. The work-PC handoff stays paused; it is very
likely unnecessary now.

---

## 2026-08-20, later — the buffer is fixed enough to matter: 16.2 GB to 2.7 GB

`CMD.DMA`'s `blit_buf` was `logic [7:0] blit_buf [0:245759]` — a byte array
holding 1.97 megabits. **The byte granularity was never used.** Both sides move
aligned 8-byte groups by construction:

- write: `wr_off <= wr_off + 32'd8` from zero, one `hps_rsp_i.data` word
- read: `wdata_off = b_commit + (wbeat << 3)`, and `b_commit` advances by
  `glen_q`, a 64-byte multiple for a canvas blit

So it was a 64-bit memory described as bytes, and the description cost **245,760
elaboration entries instead of 30,720**. Now `logic [63:0] blit_buf [0:30719]`,
one word in and one word out, no loops.

| | before | after |
|---|---:|---:|
| elaboration peak | **16.2 GB** | **2.70 GB** |

**Verified bit-identical.** Verilator `-Wall` clean, and `cmd_dma_directed`,
`cmd_random`, `cmd_random_soak` all pass — plus the `formal_cmd_dma_crc_gate`
proof at 315 s. Byte *i* of the old read was `blit_buf[wdata_off + i]`; the word
at `wdata_off >> 3` holds exactly those eight bytes in the same order.

**A trap I nearly reported through.** The first test run came back green while
the build had actually FAILED: removing the byte loop renumbered Verilator's
unnamed blocks, the generated files went inconsistent, and ctest ran the *old*
binary. So the green was meaningless. Fixed by deleting the stale model
directory and reconfiguring; the rebuilt binary's hash is recorded before the
numbers above were taken. This is the fourth time this session that a stale
build reported success.

**IT ELABORATES.** `peak = 2.70 GB, 515 s, exit 0`. From "never finishes at
16.2 GB" to done in under nine minutes. The composed shell fit is running now
for the first time since this was found.

**Expect the fitter to fail anyway, and that is fine.** Elaboration succeeding
does not make 1.97 Mbit of flip-flops placeable: the device has roughly 84,000
registers and this asks for about two million. What changes is that the failure
should now be a clear "insufficient resources" from the fitter instead of a
28 GB thrash, and it should come with a real resource report naming the
shortfall. A precise refusal is worth far more than an out-of-memory crash.

**Still not an M10K**, and the two reasons are unchanged: the write lives in an
async-reset process and the read is combinational. An M10K has no reset port and
its read is registered. Fixing that needs a one-cycle read lead in the beat
stream, which is a protocol change, and I have not made it.

### The open design question is unchanged

A **1.97 Mbit on-chip staging buffer** would claim roughly **192 of the device's
553 M10K blocks** — about 35% of all on-chip memory — to hold one canvas so the
CRC can be checked before the first byte is committed.

1. **Two passes over DDR.** Read once to CRC, read again to commit. Costs
   bandwidth, costs zero on-chip memory. The data is already in DDR. *My
   preference.*
2. **Keep it, as real block RAM.** Honest, works, spends a third of the device's
   memory on a staging buffer.
3. **Commit optimistically, invalidate on failure.** Cheapest; breaks the "zero
   guard writes on reject" law in the contract.

Nothing here is decided. It is your call.

---

## 2026-08-20, early morning — one block cannot fit, and it explains everything

### THE FINDING

**`CMD.DMA` cannot be synthesized onto this device as written**, and it is the
reason the composed fit needed 28.4 GB.

```systemverilog
parameter int unsigned BLIT_BUF_BYTES = 245760,
...
logic [7:0] blit_buf [0:BLIT_BUF_BYTES-1] = '{default: 8'h00};
```

That is a **245,760-byte on-chip buffer** — 1.97 **megabits**, a whole canvas —
declared as a flat flip-flop array. Elaborating that one module alone takes
**16.2 GB and does not finish in seven minutes**, while `SDRAM.CTRL` and
`VIDEO.MODE` each finish in **0.26 GB**.

As flip-flops it needs about two million registers. The device has roughly
**eighty-four thousand**. It is not expensive, it is impossible.

It cannot become block RAM either, as written, because it has **both**
disqualifiers at once:

- written inside `always_ff @(posedge clk or negedge rst_n)` (line 380) — an
  M10K has no reset port
- read **combinationally** at line 361 — an M10K read is registered

The block's own comment shows this was known and deferred: *"BLIT_BUF: the
largest canvas, Duo 245,760 B — the M10K mapping is the synthesis lane."* This
is the first time synthesis ever saw it.

**Why the buffer exists:** the blit engine commits a CRC-verified pixel arena,
and the contract promises rejection *before the first byte* reaches VRAM. You
cannot both stream and guarantee that, so it stores the whole canvas first. The
reason is sound; the implementation is not affordable.

**Options, none chosen — this is a design decision, not a bug fix:**

1. **Two passes over DDR.** Read once to compute the CRC, read again to commit.
   Costs bandwidth, costs *zero* on-chip memory. The data is already in DDR.
2. **Restructure as a real RAM** — 64-bit words with byte enables, clock-only
   process, registered read. Still claims roughly 192 of the device's 553 M10K
   blocks, about 35% of all on-chip memory, for one staging buffer.
3. **Commit optimistically, invalidate on CRC failure.** Cheapest, and it breaks
   the "zero guard writes on reject" law in the contract.

I would take (1). It is the only one that does not spend a third of the device's
memory on a buffer.

### THREE OF MY OWN HYPOTHESES WERE WRONG FIRST

Recorded because the wrong ones cost real time:

| hypothesis | measured |
|---|---|
| The shared packages are expensive to parse | 0.24 GB — free |
| The 22-file source cone is expensive to parse | 0.24 GB — free |
| The `-to *` virtual-pin wildcard is the cost | still 16.38 GB with it fixed |

Our own tooling had claimed for months that a leaf block's "~4.8 GB peak is
spent PARSING the 22-file source cone". **That was false**, and believing it is
part of why the composed fit got handed to your work PC. Corrected in
`tools/quartus/run_block_fit.ps1`.

### EVIDENCE I DESTROYED AND RECOVERED

`run_block_fit.ps1` used to write a report containing **only** the modules of
the current run, so any targeted run wiped everything else. It silently
destroyed the original 18-block shell sweep — including the record that
**`zhao_cmd_dma` already failed at map**, which would have found the above
weeks earlier.

Recovered from git and merged back. The script now merges instead of
overwriting and prints its own arithmetic. **42 blocks on record, 35 measured.**

---

## Where the hardware stands

| | |
|---|---|
| Phases 4–5 | complete in RTL |
| Phase 6 | 10 of 11 |
| Phase 7 | 2 of 4, two refused with reasons |
| Phase 8 | 2 of 3, HISTOGRAM refused with reasons, seam composed |
| Phases 9–11 | **not started** |
| Board | **never probed. Nothing has run on a programmed device.** |

**Capacity, 35 blocks measured against `5CSEBA6U23I7`:**

| resource | used | device | |
|---|---:|---:|---|
| ALMs | 29,526 | 41,910 | 70% |
| **DSP** | **180** | **112** | **161% — over** |
| M10K | 40 | 553 | 7% |

DSP is the binding constraint and now has a budget document
(`design/budgets/dsp.md`). ALMs are padded by ~9,000 virtual pins that vanish on
composition. M10K at 7% is headroom worth spending — moving 8,192 bits out of
flops in `TEXTURE.CACHE` recovered 4,315 ALMs.

Seven blocks still unmeasured: `cmd_dma` (fails), `surface_sheet` (fails in the
fitter), and five timeouts.

---

## Do I need to do anything?

**Right now: no.** Nothing is blocked on you.

The composed-fit handoff for your work PC (`reports/composed/README.md`) is
**worth pausing**. It would hit the same `CMD.DMA` wall, just with more RAM to
thrash in. Better to decide the `blit_buf` question first — then the composed
fit may well run on the development machine and you never need the second one.

If you do want to run it anyway, everything still works and the script now
defaults to `-Processors 1`.

---

## Elsewhere

- **Ten planetside suns are live** on zhaozhou.pages.dev, built the way the
  donor actually builds them: sky and sun share one six-bit intensity plane, so
  the largest sun in the set produces the *cheapest* loop.
- **`zhaozhou-site` is now a private GitHub repo**, so the render gallery and
  its append-only diary are under version control.
- **Sabina's sheets** are in `untitled-game/docs/`:
  `konzeptzeichnungen-anleitung.html` and
  `vereinbarung-konzeptzeichnungen.html`. Both A4, Swiss German, no eszett.
- Sorry about the printer. Free RAM hit **0.6 GB** during the Quartus runs,
  which is almost certainly why the print dialog would not open.
