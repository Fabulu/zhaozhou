# Latency budget — a first-class goal, established 2026-08-21

> **Owner's ruling, 2026-08-21:** less latency is an improvement and should be
> a **formal goal**, as long as it does not break anything else. That it was
> never one is an architectural oversight.

This document exists for the same reason `design/budgets/dsp.md` does, and its
opening line is worth rereading:

> "This went unnoticed because **there was no DSP budget document** and the
> per-block fitter reports were never totalled."

Latency had exactly that shape. It was an **accident of other decisions** —
nobody chose it, nobody totalled it, and the one time it improved by 58,000
cycles the improvement was nearly discarded as a test failure. A budget nobody
writes down is a budget nobody defends.

---

## 1. The rule

**Input-to-photon latency is a first-class budget alongside DSP, ALM and
bandwidth.**

1. Any change that moves latency **must say so and by how much**, in its commit
   message and in the block's contract. Silence is not permission.
2. A change that **reduces** latency is a win to be kept, not a test failure to
   be reverted. If a golden or a timing expectation disagrees with a real
   improvement, the golden is what moves — after the measurement is recorded,
   never quietly.
3. A change that **increases** latency needs a stated reason. "It was easier" is
   not one.
4. Latency is **not** to be traded for throughput without the trade being
   written down. The two are different resources and this project has a fill
   rate it can afford and a responsiveness it cannot buy back.

## 2. What is actually measured today

Honest scope: **the blit path is measured; end-to-end input-to-photon is
not.** Stating that plainly is the point of a budget document.

### Frame periods (`spec/video_rules.md`, from `zhao_pkg.sv`)

| Mode | gpu cycles/frame |
|---|---:|
| Z60 | 251,520 |
| Storm | 217,984 |
| Duo | **318,592** |

### WARNING: there are TWO "cycles per frame" numbers and they differ 6.6x

Do not use the table above as a compute budget. It is a **video-domain**
quantity.

`frame_gpu_cycles` is exactly `2 x h_total x v_total` for every mode — verified:
Z60 2 x 125,760 = 251,520; Storm 2 x 108,992 = 217,984; Duo 2 x 159,296 =
318,592, a ratio of exactly 2.000 in all three cases, because
`vid_clk = gpu_clk / 2` (`spec/video_rules.md:11`). It is the raster's own
period, and the scheduler uses it as a **deadline** (`zhao_cmd_scheduler.sv:292`
`dead_lim`).

The **compute** budget is the one the cost models use:
**1,666,667 cycles per 60 Hz frame at the 100 MHz placeholder**
(`spec/terrain_rules.md:525,579`, `spec/sky_and_beams.md:162` — and note those
say *placeholder*: **Phase 0 freezes the clock**, it is not frozen yet).

| | cycles per frame |
| --- | ---: |
| `frame_gpu_cycles`, Z60 — **video/deadline** | 251,520 |
| cost-model frame at 100 MHz — **compute** | 1,666,667 |

**Which one is right is settled by arithmetic, not preference.** GEOM.SKIN's
ruled demand is 120,000 vertices/frame at 10 cycles each = **1,200,000 cycles**.
Against 251,520 that is 477% of the frame — impossible. Against 1,666,667 it is
72% — tight but real. So the fabric clock is not the video-derived 15.09 MHz,
and a block's throughput must be costed against the 100 MHz placeholder.

This matters because a per-block cost summed against 251,520 is wrong by 6.6x in
the direction that makes a block look **unaffordable when it is fine**, or a
budget look met when it is not. Both numbers are called "gpu cycles" and both
are correct for their own purpose.

### The Duo full-canvas blit

| | first blit completes | period |
| --- | ---: | ---: |
| old path (`CMD.DMA`, serial fetch then commit) | 605,308 | 637,184 |
| new path (`DEBUG.FRAMEBLIT`, streaming) | **547,321** | 637,184 |

**~58,000 gpu cycles saved**, which moves the displayed content **one whole
frame earlier**: the picture that used to appear at frame N+1 appears at frame
N. Roughly **16.7 ms** at the 60 Hz field rate.

The saving comes from overlapping the two halves that used to run in sequence:

- HPS fetch ≈ 93k gpu cycles;
- paced VRAM commit ≈ 245k gpu cycles.

The old atomicity rule forbade writing anything before the checksum passed,
which forced them serial. The redesign streams them.

### What this does NOT achieve

**It does not reach 60 Hz fresh frames.** The commit phase still dominates
against a 318,592-cycle frame. The period is unchanged at 637,184 — this is a
**phase** improvement, not a rate one, and D5 in
`reports/status/phase2_wave2.md` still stands as a conclusion.

**Everything here is simulation.** No number in this file has been measured on
a board.

## 3. What is not measured yet, and should be

The gap this document opens deliberately rather than papering over:

- **Controller sample to displayed photon.** The whole chain. Nothing measures
  it end to end today. It is the number that actually matters to a player and
  the one this budget exists to eventually defend.
- **Where the rest of the latency lives.** Command submission, the scheduler's
  tick alignment, the sim step, geometry and raster, resolve, the blit, scanout.
  Only the blit segment above is quantified.
- **Whether the tick alignment costs a frame.** `CMD.SCHEDULER`'s D8 law closes
  every packet at its FIRST tick, and claims are locked to the tick which fires
  at vblank start. That is a scheduling choice with a latency consequence
  nobody has costed.

**None of these should be guessed at.** They want the same treatment the DSP
budget got: measure, total, write down, then argue.

## 4. Standing tasks

1. Add an end-to-end latency measurement to the shell harness, in the same
   spirit as the blit-completion stamp that made the table in §2 possible. The
   harness already stamps every blit completion with the gpu step it happened
   on; that mechanism is the precedent.
2. Total the per-stage contributions into this file once they exist.
3. Report latency in `STATUS.md` whenever it moves, in either direction.

## 5. Why the near-miss is recorded here

The 58,000-cycle improvement arrived looking like a **bug**: 41 of 340 checks
failed in `demos/wound_lab/duo_markers.cpp`, and not one of them was a wrong
pixel. Every failure was a timing assertion calibrated against the slower path.

Had those checks been treated as the authority rather than as a record of a
measurement, the correct response would have looked like "revert the change
that broke the tests" — and a real improvement would have been thrown away to
keep a golden green.

**A test that pins a measured property is a record, not a law.** When the
machine genuinely gets better, the record moves; it just has to move loudly.
That is rule 2 in §1 and it is the whole reason this file exists.
