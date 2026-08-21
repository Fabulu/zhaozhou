# Phase-2 / Wave-2 status — the console shell and the Duo marker demo (W2.7)

*W2.7 implementation agent, 2026-08-16. This is the wave-2 closing report the
plan (`runs/CLAUDE-RUNS/RUN-20260814-2154-wave2-phase2-console-shell/PLAN.md`
§2 W2.7) requires, and the home of the W2.7 COMPOSITION DOSSIER — the
quantified findings that only appeared when the separately-verified blocks
were wired into one machine.*

---

## 1. What was composed

`fpga/rtl/common/zhao_shell_top.sv` is the Phase-2 console as ONE running
composition: CMD.SCHEDULER + CMD.DMA, two MEM.GUARD instances (scanout
read client, blit write client) + MEM.VRAM.ARBITER + the SDRAM controller,
MEM.HPS.BRIDGE, the full VIDEO subsystem (mode/scanout/scaler/framectl),
INPUT.SNAPSHOT + INPUT.RUMBLE, AUDIO.FIFO, DEBUG.COUNTERS + DEBUG.CRC.
The behavioural SDRAM model rides in the testbench wrapper
(`tests/shell/tb_zhao_shell.sv`); the harness C++
(`tests/shell/shell_harness.hpp`) is SW.RUNTIME.HPS per D10 — it hosts the
FRAME_RING, the pixel arenas, answers bridge bursts on the frozen 16-cycle
profile, and owns the frozen clock phases (vid = gpu/2 coincident-odd,
audio = gpu/4 post-edge).

Ten pieces of real glue live in the shell, each a seam no block wave ever
crossed (numbered in the module header): the SDRAM write-data queue, the
read-beat packer, the record framer with its deadlock-breaking queue, the
slot-ready pending registers, the frame-completion correlator, the mode
CDC, the displayed-byte serializer feeding DEBUG.CRC
(`zhao_displayed_bytes`, NEVER `zhao_canvas_bytes` — the named Duo trap),
the counter provider adapters (spec/counters.md §5 owner table), the
rumble edge converter, and the blit pacer.

## 2. The composition dossier — what integration alone revealed

Every item below was invisible to every pre-W2.7 lane and found by
composing, with the falsifying evidence named.

**D1. The blit write-data seam carried 1/8th of the data.** CMD.DMA's
`guard_wdata_o` sideband presented only the FIRST 8 bytes of every 64-byte
guard write request; the other 56 bytes existed in `blit_buf` and on no
wire — and `cmd_dma_directed`'s "committed bytes bit-exact" check compared
exactly those 8 bytes per request, green while 7/8ths of every frame was
unverifiable. Fixed at `bb13352`: the seam is now a `guard_wvalid_o` beat
stream (ceil(len/8) beats per accepted request); the test captures every
byte; mutation-verified (freezing the beat offset fails it);
`cmd_dma_crc_gate` re-elaborated green with 7/7 covers.

**D2. A scheduler/DMA composition deadlock.** The scheduler backpressures
records while a blit dispatch is pending (`rec_ready = !(blit_v &&
!ready)`), and the DMA can accept that blit only after the SAME packet's
verified stream fully drains. Wired directly, any record after a
DebugFrameBlit wedges the machine. Neither block is wrong in isolation —
W2.6 drove the record port from the harness. The shell's record framer
carries a FRAMER_Q-deep record queue that decouples presentation from
stream consumption, plus a sticky deadlock tripwire (`shell_err_framer_o`)
for packet shapes beyond the queue.

**D3. Both FB slots lived in one DRAM bank — read/write row thrash.**
`FB_SLOT1 = 0x0003_C000` shares bank 0 with slot 0 (banks come from byte
address bits [26:25]). The composed scanout-read + blit-write streams
row-thrash that bank: measured ~82 of 192 Duo lines starved per frame
(41,984 starved vid cycles), display shredded. Fix: the FB-slot BANK SPLIT
(`ZHAO_FB_SLOT1_BASE → 0x0200_0000`, bank 1) — a frozen-interface edit
carrying its sign-off reference
(`RATIFICATION-fb-slot-bank-split.md` — GRANTED 2026-08-16 with the ZH-004 bank-mapping condition),
with the guard/oracle/harness/test/spec shadow updated to the DISJOINT
two-region map and `mem_guard_no_escape` re-elaborated green.

**D4. Even bank-split, free write interleave starves the serial fetch.**
The W2.2 fetch is request-serial (64-B request, wait all 8 beats, next);
the blit exploits every inter-request gap, leaving the fetch ~2% short of
Duo line rate — the deficit accumulates until the ping-pong limps in a
deterministic 2-of-4-lines-starved pattern. Fix: the shell's BLIT PACER
(glue 10) — the blit client (lawfully — client pacing does not touch the
verified D3 arbiter) offers its request only after scanout has been quiet
8 cycles, batching writes into line tails, Duo border lines and vblank.
Measured: starvation exactly zero at every cadence.

**D5. 60 Hz full-canvas cadence is INFEASIBLE on the machine as frozen.**

> **THE ~338k FIGURE BELOW IS SUPERSEDED AND THE CONCLUSION IS NOT.**
> Added 2026-08-21. The `DEBUG.FRAMEBLIT` redesign streams the fetch and the
> commit instead of running them serially, and measures **~58k gpu cycles
> cheaper** end to end. `reports/BLIT_INTEGRATION_PHASE_SHIFT.md` carries the
> measurement and the branch it was taken on.
>
> D5's conclusion still holds — the commit phase still dominates against a
> 318,592-cycle frame, and the Z60 raw-demand argument in the last bullet is
> untouched by any of this, being a bandwidth proof rather than a scheduling
> one. But the headline NUMBER is no longer the machine's number, and the
> demo's timing expectations are pinned to the old one. Which of those moves
> is Fabian's decision, recorded in that report; this note exists so the
> figure is not read as current in the meantime.
>
> `duo_markers.cpp`'s own header already named "streaming blit CRC" as a
> ratification-scale path that would close 60 Hz. It was right.

The end-to-end cost of one lawful Duo blit, starvation-free, measured
~338k gpu cycles against the 318,592-cycle frame:

- HPS fetch ≈ 93k — 3,072 serial 64-B bursts × (16-cycle frozen D10
  latency + 8 beats + handshake), one in flight per client by
  memory_rules §3 law;
- paced VRAM commit ≈ 245k — the starvation-free write budget is ~42
  bursts per active line plus the blank windows, and the fetch phase burns
  the vblank+border windows (claims are locked to the tick, which fires at
  vblank start, so the fetch always eats the blank gold);
- CMD.SCHEDULER's D8 law closes every packet at its FIRST tick — so every
  full-canvas blit fences STATUS_DEADLINE, at any cadence, in every mode.
  Z60 is worse still: raw demand (138k read + 115k write + 5k refresh =
  258k) EXCEEDS its whole 251,520-cycle frame — over raw SDRAM bandwidth,
  no scheduling can fix it. (The R4 "bandwidth budget" proof was per-LINE
  and never summed a full frame against the packet pipeline.)

Paths that would close 60 Hz, all ratification-scale (none taken here):
wider bridge bursts (64 → 512 B: fetch ≈ 32k, closes Duo with ~8% margin)
and/or pipelined bridge bursts; a streaming blit CRC (forbidden today by
the ratified buffer-then-release gate, `cmd_dma_crc_gate` (b)); decoupling
the claim window from the tick so the fetch phase stops eating vblank.

**The adopted operating point** is the machine's true sustainable cadence
for FULL-CANVAS DEBUG BLITS: one fresh blit per two displayed frames. To
be precise about what this does and does not mean: **the console runs at
60 Hz** — the raster, the tick, pad latching, audio pacing and the
displayed-CRC law all run at full frame rate throughout; what cannot
complete inside one frame is the `DebugFrameBlit` TRANSPORT (HPS fetch +
CRC gate + paced VRAM commit of a whole canvas). This is a debug-blit
cost, not a rendering cost — the Phase-3+ render path (RASTER.RESOLVE
writing tiles it produces on-fabric) is not shaped like a 196,608-byte
host-to-VRAM copy and inherits no 30 Hz ceiling from this finding.
Packet P_f publishes at tick 2f−1, displays FRESH at raster frame 2f+1,
and frame 2f+2 lawfully REPEATS it with an IDENTICAL displayed CRC — the
60 Hz repeat law, mechanically proven 600 times by the gate demo. The
deterministic consequences (deadline_faults = 1 + floor(k/2); every fence
STATUS_DEADLINE) are PINNED exactly by the demo — asserted as the
expected values every tick, never tolerated as drift.

**D6. Smaller composition corrections.** (i) The CMD.DMA contract said "No
VRAM writes" — written before the blit engine existed and false for the
shipped RTL; corrected with the seam law. (ii) The plan's "48 interface
lines = checker + frame counter" collides with the ratified border-black
law (video_rules §3.1: border rows are hardware-black at scanout) — the
interface strip lives in the top 8 rows of each 256×192 view instead.
(iii) DEBUG.COUNTERS' header sketch wires CMD.DMA's snap channels; they
duplicate catalog ids 1/2 (scheduler-owned per counters.md §5) and id 29
is the bridge's — the shell wires the §5 owner table and leaves the DMA
channels unconsumed, documented in the shell header. (iv) The
`zhao_frame_tick`↔raster mapping: tick k is the vswap decision at line 244
of raster frame k−1 — the demo's protocol mirror encodes it after the
probe measured the off-by-one in the first model.

## 3. The Phase-2 gate, criterion by criterion

Plan W2.7 normative acceptance, each stated met / unmet:

| Criterion | Verdict |
|---|---|
| "Two controllers move independent 2D markers in Duo" as an executable ctest | **MET** — `shell_duo_markers` (demos/wound_lab/duo_markers.cpp): 600 marker frames on the full machine, PCG pad streams through INPUT.SNAPSHOT, marker law `pos += clamp(analog>>12, −8..+8)` wall-clamped, markers at 30 Hz content rate |
| ∀f: RTL displayed-stream CRC == zref-composed canvas CRC | **MET** — every displayed frame (startup, fresh, repeat) asserted against `zref::render::displayed_crc32c`; repeats CRC-identical (the 60 Hz law, 600 proofs) |
| PadFrame sequences gapless | **MET** — at full 60 Hz, bit-equal to `zref::PadSnapshot`, `input_sequence_gaps == 0` |
| Audio stream bit-equal to oracle | **MET** — PCM out equals the fed `zref::MixerTone` stream; `audio_underruns == 0` |
| Counters starvation/underrun/gaps/rumble-drops zero | **MET** — absolute zeros, plus a pinned constant starvation baseline (0) |
| Counters deadline_faults zero | **NOT MET — INFEASIBLE** on the composed machine as frozen (dossier D5). The demo pins the exact closed-form fault count instead; closing this at 60 Hz requires one of the named ratification-scale changes |
| Committed trajectory hash in `captures/golden/wave2/duo_markers.zcap` | **MET** — CRC-32C over the 600 displayed CRCs as a COUNTERS entry (capture-local id 0xFFFF), byte-identity-verified on every run |
| Per-mode goldens `{z60,storm,duo}_10frame.zcap` | **MET** — 10 sealed packets each at the sustainable cadence, FRAME_PACKET + FRAMEBUFFER_EXPECTED per packet + CONTROLLER_SNAPSHOT + COUNTERS, `shell_golden_replay` verifies byte-identity |
| Nightly soak (10,000-frame Verilator surrogate) | **MET** — `shell_duo_markers_soak`: PCG jitter on publish timing, pads, rumble |
| Status report + blocked_on_hardware update + maturity consolidation | **MET** — this file; see §5 |

**The gate verdict, stated plainly:** the console runs, the demo is
capture-exact and executable, but the plan's zero-deadline-fault 60 Hz
letter cannot be satisfied by ANY software on this machine as frozen. The
gate is closed as "met with the deadline-fault criterion replaced by its
pinned closed form", pending the architect's ruling on the dossier — the
honest alternative readings are (a) accept the sustainable cadence as the
Phase-2 machine's law, or (b) ratify one of the D5 paths and re-run W2.7's
demo at 60 Hz.

## 4. Verification (counts, never percentages — all personally observed)

- 600-frame gate demo: **24,629 checks passed / 0 failed** (1,202 displayed
  frames; every displayed CRC == the zref composition including 600
  CRC-identical repeats; 95.8M PCM pairs bit-equal; faults/fences pinned in
  closed form) — plus the ctest lane variants of the same binary:
  `shell_duo_markers_fast` (40 markers) and `shell_golden_replay`
  (byte-identity, 3 modes) green inside the fast lane.
- fast lane (final tree): **78 pass / 1 skip / 0 fail** — the single skip
  is `format_check` on a machine without the pinned clang-format (CI's
  LLVM-15 job remains the format gate).
- mem lane after the bank split: **12 pass / 0 skip / 0 fail** (directed +
  random + bandwidth + bridge); video differential lane on the re-based
  oracle: **16 pass / 0 skip / 0 fail** including the `_full` soaks.
- formal, re-elaborated this wave: `cmd_dma_crc_gate` (beat-stream cone,
  bmc + cover, 7/7 covers), `mem_guard_no_escape` (disjoint map, bmc +
  cover), `cmd_scheduler_slot_fsm`, `audio_fifo_bounds`,
  `input_snapshot_atomic`, `formal_lane` — 6 pass / 0 skip / 0 fail; the
  heavier arbiter/video set re-ran green as well (TASK_LOG has the final
  wall times).
- compiler workspace 216 pass / 0 fail; ledger 40 / fixgen 14 / abi-gen 20
  pass, 0 fail; `abi:check` clean (25 outputs), `tables:check` clean (10);
  `ledger:check` green (V1–V17, V19–V20, staleness; 88 blocks / 40 ops /
  10 formal runs) with the §5 promotions staged.
- the golden captures verify with the INDEPENDENT reader too:
  `zhao-capture verify` — duo_markers.zcap all 603 sections, the three
  10-frame captures all 23 sections each, sealed packets structurally
  valid.
- OFFICIAL 600-marker gate on the final binary: **24,630 checks passed /
  0 failed** in 34m40s, capture byte-identical to the committed
  duo_markers.zcap.
- OFFICIAL 10,000-displayed-frame soak (`--soak 5000`, PCG jitter on
  publish timing / pads / rumble): **205,026 checks passed / 0 failed** —
  5,000 published frames, 10,002 ticks, 10,003 displayed CRCs, 796.7M PCM
  pairs bit-equal, byte-delta jitter never left the ±8 KiB window; wall
  2h28m, clean exit.
- nightly label: the dedicated ctest lane was reaped by the task runner at
  ~65 min (an infra kill mid-lane with 81 tests green and zero failures at
  the point of death — not a test failure); every remaining
  nightly-labeled test was independently green this session (video `_full`
  soaks, cmd_random_soak, the gate runs, golden replay, and the two
  leftovers re-run 2 pass / 0 skip / 0 fail).

## 5. Maturity consolidation

- CMD.SCHEDULER, CMD.DMA, DEBUG.COUNTERS, DEBUG.CRC: **UNIT_VERIFIED →
  RTL_VERIFIED** — the composed shell demo is the RTL-level integration
  evidence the W2.6 promotions lacked (600 frames against the zref-composed
  expectation on the full machine, plus the re-elaborated formal runs).
- SW.RUNTIME.HPS: the D10 harness-as-HPS now exists and drives the machine
  (see ledger notes; the block's own maturity advances only with its
  contract-cited evidence, not by this report).
- MEM.SDRAM stays SPECIFIED (blocked_on: hardware); the shell exercises the
  controller against the behavioural model only — evidence banked.
- `reports/blocked_on_hardware.md` gains the ZH-004 bank-mapping obligation
  (the bank split assumes byte-address bit 25 selects the bank on the real
  device).

## 6. For the next integrator

- The shell's tripwires (`shell_err_*`, `cnt_cat_violation_o`,
  `guard_violations_o`, `model_error`) are asserted zero by every scenario;
  treat any nonzero as a composition bug, not noise.
- `tests/shell/shell_probe.cpp` is the bring-up diagnostic that measured
  every number in this dossier — keep it.
- The linebuf abort-outside-vblank hazard (video_rules §8.1) remains fenced
  by system timing; the shell aborts only via dec/mode-flush, inside the
  law.
- `REFRESH_URGENT` derivation (queue item 4) is untouched by this wave.
