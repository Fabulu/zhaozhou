# Zhaozhou Memory Rules — Phase 2 (wave 2)

**Status:** ratified 2026-08-14 (plan W2.1, decisions D2/D3/D10). Single law
for MEM.SDRAM (simulation profile + obligations), MEM.VRAM.ARBITER,
MEM.HPS.BRIDGE (ring descriptors) and MEM.GUARD (Phase-2 region map). The
frozen RTL port types live in `fpga/rtl/common/zhao_pkg.sv`. Where this file
and any other text disagree, this file wins for memory.

---

## 1. SDRAM simulation profile (D2) — the conservative test profile

All Phase-2 RTL is verified against ONE frozen conservative profile
(`fpga/rtl/memory/zhao_sdram_params_pkg.sv`; the numbers are provisional
sim constants, NOT board truth — board probe ZH-004 later emits
`fpga/rtl/generated/sdram_params.svh` from `board_truth.json`; captures
record the profile version, plan R3):

| Parameter | Value | Meaning |
|---|---|---|
| CAS latency | 3 | read-data latency after column activate-burst |
| Burst length | 8 | 8 words per read/write burst |
| tRCD | 3 cycles | ACTIVE-to-read/write delay |
| tRP | 3 cycles | PRECHARGE-to-ACTIVE delay |
| tRC | 9 cycles | ACTIVE-to-ACTIVE (same bank) minimum spacing |
| Refresh | 1 AUTO_REFRESH every 780 sdram cycles | 8192 rows / 64 ms at the conservative clock |
| Clock phase | controller-centred | parameters, not behaviour, for sim |

- The **behavioural SDRAM model** (`sim/models/zhao_sdram_model.sv`) is a
  TESTBENCH component: non-synthesizable, excluded from lint/synthesis
  targets, parameterized by the same params package.
- MEM.SDRAM stays at maturity SPECIFIED (blocked_on: hardware) with banked
  evidence; downstream blocks are verified against THIS profile only.
- ZH-004 obligation list (what the board probe must return to unfreeze):
  exact device ordering code and speed grade; stable sdram clock; sustained
  sequential and strided bandwidth; measured tRCD/tRP/tRC/tRC-overhead and
  refresh accounting; operating temperature range at stress.

## 2. Arbiter policy (D3) — MEM.VRAM.ARBITER

Four client classes at the arbiter; one request channel to MEM.SDRAM.

- **Scanout (VIDEO.SCANOUT fetch) is the STRICT-PRIORITY guaranteed client.**
  It preempts at burst boundaries only (never mid-burst; burst = 8 words).
- **Other guaranteed clients round-robin** among themselves (Phase-2
  members: scanout-fill already priority; the RR set is {command/blit DMA,
  engine/reserved slots}). One outstanding request per client.
- **Best-effort class exists but is EMPTY in Phase 2** (ports reserved).
- **Credit-based at the SDRAM edge, ready/valid at client ports.** A client
  holds credits; each accepted beat consumes one; the controller reissues
  credits as bursts retire.
- **Liveness bound (formal, `mem_vram_arbiter_liveness`):** every guaranteed
  client is granted within
  `B = G · MAX_BURST + REFRESH_OVERHEAD` sdram cycles, where G = number of
  guaranteed clients, MAX_BURST = burst length (8) + activate/precharge
  overhead (tRCD + tRP = 6), REFRESH_OVERHEAD = tRC + tRP = 12.
  Phase-2 bound: B = 2 · 14 + 12 = 40 sdram cycles worst-case grant latency.
- Per-client byte counters (`vram_bytes_by_client`) count accepted payload
  bytes by client id (client id enum in zhao_pkg.sv).
- **Bandwidth budget proof (risk R4):** worst Phase-2 case = Duo scanout
  (159,296 vid-cycles/frame fetch at 2 gpu cycles/vid-cycle ⇒ ≤ 245,760 B
  fetched per 318,592 gpu cycles) + one blit DMA frame (≤ 245,760 B) +
  refresh (≈ 408 refreshes × 12 cycles ≈ 4,896 cycles) — the directed
  `bandwidth_budget` test drives exactly this and asserts ZERO
  scanout-starvation cycles.

## 3. HPS-DDR bridge (D10) — MEM.HPS.BRIDGE

- The FPGA-side core is FUNCTIONAL RTL against a generic burst request/
  response port (`zhao_hps_burst_req_t` / `zhao_hps_burst_rsp_t` in
  zhao_pkg.sv). The framework-AXI adapter is the hardware-lane seam (its
  contract section records the seam; Phase 2 has no AXI RTL).
- **In Verilator the harness C++ IS the HPS** (plan D10): it hosts the frame
  ring, PCM ring, pixel arena, and answers bursts deterministically
  (fixed latency profile: 16 gpu cycles request→first beat, 1 beat/cycle
  thereafter — the sim profile, recorded in captures).
- Bursts are 64-B aligned, 1..64 B length, read or write; one burst in
  flight per client; per-client byte counters `hps_ddr_bytes_by_client`.

## 4. Ring descriptors (HPS-hosted rings the FPGA consumes)

All rings live in HPS DDR; the FPGA never writes a descriptor another side
owns (no shared mutable structures, charter law). Descriptors are 32 B,
4-aligned, little-endian.

### 4.1 FRAME_RING (triple-buffered frame packets, charter §7.2)

3 slots × `FRAME_SLOT_BYTES` (1 MiB) each, preceded by a 3-entry descriptor
table:

```
+0  u32 state      // 0=FREE, 1=ARM_WRITING, 2=READY, 3=DONE (charter 7.4 names)
+4  u32 sequence   // packet sequence (frame header mirror)
+8  u32 resource_epoch
+12 u32 byte_len   // sealed packet length (40 + N)
+16 u32 payload_crc32c   // mirror of the packet's payload CRC (cross-check)
+20 u32 reserved   // 0
+24 u64 frame_crc32c     // displayed-stream CRC the HPS expects back (0 = none)
+32 (next descriptor)
```

Slot bodies follow the descriptor table at a 4-KiB-aligned base. The HPS
writes a slot FREE→ARM_WRITING→READY (sealing per capture_format.md §3);
the FPGA (CMD.SCHEDULER) claims READY→FPGA_RUNNING and returns DONE→FREE
via a bridge write. Ownership transitions are single-word writes of `state`
— the only word both sides ever write, never simultaneously.

### 4.2 PCM_RING (audio, spec/audio_rules.md §3)

```
+0  u32 capacity_pairs   // power of two
+4  u32 reserved         // 0
+8  u64 host_write_ptr   // pairs written (monotonic, mod 2^64)
+16 u64 fpga_read_ptr    // pairs consumed (FPGA-owned word)
+24 u64 reserved2        // 0
```

Ring data starts at the next 64-B boundary after the descriptor. Pointers
are free-running u64 (no wrap cases in Phase 2); each side reads only the
other's pointer (gray-coded at the CDC).

### 4.3 Pixel arena (DebugFrameBlit source)

The blit source is a plain HPS buffer at `src_addr_hps`; no descriptor —
the command record itself carries address, length, and the expected CRC-32C
that the DMA verifies before the first byte commits to VRAM (CMD.DMA law).

## 5. Guard region map (MEM.GUARD, Phase 2)

VRAM byte addresses (u32), 8-B granule. Phase 2 allocates exactly:

| Region | Base | Size (bytes) | Owner |
|---|---|---|---|
| FB slot 0 | `0x0000_0000` | `0x0003_C000` (245,760) | frame display slot 0 |
| FB slot 1 | `0x0003_C000` | `0x0003_C000` (245,760) | frame display slot 1 |
| (unmapped) | `0x0007_8000` … | — | any access = violation |

- Both slots are sized for the LARGEST canvas (Duo) regardless of the
  active mode: a mode switch never moves a slot (spec/video_rules.md §3).
- Every fabric VRAM access flows through the guard: request carries
  `{client, write, addr, len}`; the guard checks
  `addr ≥ base ∧ addr + len ≤ end` for the client's OWNED region and
  forwards to the arbiter, else drops the request (nothing is written),
  pulses `guard_violation`, and counts (formal `mem_guard_no_escape`).
- Region ownership in Phase 2: a client owns the slot it was granted for
  the frame (blit DMA writes exactly the slot named by `DebugFrameBlit
  .dst_slot` and exactly `byte_len` = `canvas_bytes(mode)` bytes — any other
  length is rejected before the first byte); scanout owns both slots
  read-only.
- Later phases extend the map (texture/terrain/particle pools per the
  charter allocator); Phase 2 ships ONLY the two FB regions — everything
  else is a violation by construction.

## 6. Interface summary (frozen in zhao_pkg.sv)

- `zhao_guard_req_t` / `zhao_guard_rsp_t` — client→guard request
  `{valid, write, client, addr, len, byte_enable}` and response `{ok,
  violation}`.
- `zhao_arb_req_t` / `zhao_arb_rsp_t` — guard→arbiter credit port
  `{valid, write, client, addr, len}` / `{grant, credits}`.
- `zhao_hps_burst_req_t` / `zhao_hps_burst_rsp_t` — bridge bursts (§3).
- `zhao_client_e` — client id enum (scanout, blit_dma, engine0, engine1,
  debug) driving the per-client counters.

## 7. Test obligations (directed at W2.5)

- Directed: refresh steals counted (deterministic refresh schedule);
  scanout preempts at a burst boundary (not mid-burst); guard rejects
  out-of-region writes and NOTHING is written (shadow-memory check).
- Random three-way: RTL arbiter + sdram ctrl + behavioural model vs
  `zref::VramArbiter` / `zref::SdramController` oracles — grant ORDER
  equality plus a 64 KiB shadow memory integrity compare.
- Bandwidth budget test (§2 worst case, zero starvation).
- Formal: `mem_vram_arbiter_liveness` (the B bound), `mem_guard_no_escape`,
  `mem_sdram_refresh_bound` (banked — blocked_on hardware gate).
