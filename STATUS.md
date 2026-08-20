# Status — for Fabian

*This file is the channel. I update it and push; you read it here. Newest section
at the top.*

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
