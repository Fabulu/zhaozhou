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
- **Aging override (load-bearing, was missing from this section):** a
  guaranteed client whose pending bursts have waited `AGING_OVERRIDE = 20`
  cycles is served ahead of everything, lowest id first. Without it strict
  scanout priority would starve the RR class forever and *no* finite B would
  exist. It is the mechanism the bound below actually rests on.

- **Liveness bound (formal, `mem_vram_arbiter_liveness`):** see §2.1.

### 2.1 The liveness bound B (D3) — corrected 2026-08-16

> **~~SUPERSEDED — the original D3 bound was wrong and its proof had never
> been run.~~**
> ~~`B = G · MAX_BURST + REFRESH_OVERHEAD` sdram cycles, where G = number of
> guaranteed clients, MAX_BURST = burst length (8) + activate/precharge
> overhead (tRCD + tRP = 6), REFRESH_OVERHEAD = tRC + tRP = 12.
> Phase-2 bound: B = 2 · 14 + 12 = 40 sdram cycles worst-case grant latency.~~
>
> Kept visible rather than rewritten (charter honesty discipline). The
> constant was frozen in `zhao_pkg.sv` and cited by `MEM.VRAM.ARBITER`'s
> `RTL_VERIFIED` entry for a full wave while the proof it named had never
> been elaborated. The moment it was, **B = 40 failed**. Ratified for
> re-derivation in `runs/CLAUDE-RUNS/RUN-20260814-2154-wave2-phase2-console-shell/RATIFICATION-arbiter-liveness-bound.md`.
>
> **Two independent errors, which partly cancelled:**
> 1. It budgeted **one burst per client turn**. A 64-byte request is
>    `64 / (BURST_LENGTH · WORD_BYTES) = 64 / 16 =` **4 bursts**, and the
>    arbiter re-arbitrates only at burst boundaries, so a competitor's whole
>    multi-burst request can sit in front of the waiting client.
> 2. `MAX_BURST = 8 + tRCD + tRP = 14` is **not** the worst grant-to-grant
>    span. The bank-conflict full read is PRE, tRP, ACT, tRCD, READ, CAS and
>    the full 8-beat bus burst ⇒ **18** cycles (the ctrl law table in
>    `zhao_sdram_ctrl.sv` derives 12/15/18 read, 10/13/16 write).

**Corrected derivation.** A client accepted at its port waits out `N` whole
burst spans, because the arbiter never preempts mid-burst (D3):

```
N = 1                                            (the burst already in flight)
  + bursts the competitor may still take before it must yield

  scanout (strict priority):  + 1   = 2
      at most ONE aging-override burst from the RR class can be interposed;
      serving it resets that client's age, so a second cannot follow.

  RR class (blit/engine):     + min(BURSTS_PER_REQ, ceil(AGING_OVERRIDE / MAX_BURST_SPAN))
                              = min(4, ceil(20/18)) = min(4, 2) = 2   = 3
      ^^^^^^^^^^^^^^^^ the bursts-per-request factor the old formula omitted:
      with one burst per turn this term is 1 and the bound collapses to the
      old, wrong, 2 spans.

B_noref = N · MAX_BURST_SPAN − 2
```

The `− 2` is the alignment between the port-grant pulse and the arbitration
boundary: the client becomes visible to the selection one cycle after its
grant pulse, and the bound is stated exclusively (`waited < B`).

| | N | B (refresh-free) | + refresh steal | **B operational** |
|---|---|---|---|---|
| scanout (strict priority) | 2 | 2·18 − 2 = **34** | +13 | **47** |
| RR class (blit, engine)   | 3 | 3·18 − 2 = **52** | +13 | **65** |

**Refresh steal = 13.** One AUTO_REFRESH can land inside a wait window (and
only one: `REFRESH_INTERVAL` = 780 ≫ 65). It costs the arbitration boundary
cycle plus tRP + tRC = 12 ⇒ 13 cycles of extra grant latency. Worst placement
is the *last* boundary of the window: a refresh anywhere earlier closes every
row, so the burst behind it is a MISS (15) rather than a CONFLICT (18) and
gives 3 cycles back. An aging-override offer raises `hold_refresh`, so only a
*hard* refresh (`refresh_cnt ≥ 840`) can take that boundary — possible, hence
counted.

**Measured tight bounds (evidence, both directions).** Bisected on
`tests/formal/formal_mem_arbiter.sv` at depth 130 with boolector:

| client | largest FAILING B | smallest PASSING B |
|---|---|---|
| RR class (client 1) | **51** | **52** |
| scanout (client 0)  | **33** | **34** |

The endpoints of each pair are adjacent, so 52 and 34 are the exact worst
cases — not values that merely happened to pass. Both directions are
committed as `.sby` tasks (`bmc`, `bmc_tight_rr`, `bmc_tight_scanout`), and
the `cover` task carries `c_near_rr` / `c_near_sc` so the asserts cannot pass
by starving the environment instead of the arbiter.

**Scope of the proof (stated, not assumed).** The BMC horizon is 130 cycles,
in which `refresh_cnt` cannot reach even `CNT_PENDING` = 780 — so **34 and 52
are the refresh-free bounds**, and the +13 is analytic (the refresh sequence's
own cycle count is verified cycle-exactly by `mem_sdram_directed`, and
`mem_sdram_refresh_bound` is banked on the hardware gate). The harness
machine-checks this scope claim with `a_horizon_is_refresh_free`: raising the
depth past the refresh interval makes it fire, which is the signal to prove
the composed bound rather than to re-run it.

**The bound is a STEADY-STATE bound**, measured from an acceptance at or after
`init_done`. The arbiter's port handshake does not know about `init_done`, so
a client can be accepted during the ~26-cycle power-on init and the offer then
latched sits un-taken for the whole init sequence. Measuring from such an
acceptance measures initialization, not arbitration — it inflated the scanout
number from 34 to 38 until the harness was gated. Power-on is not covered by B
and never was.

**Scanout re-check (does the correction break "scanout never starves"?).**
No — and it is now proven rather than argued. Scanout is a separate, tighter
case because it is strict priority: the only thing that can get in front of it
is one aging-override burst from the RR class, and serving that burst resets
that client's age so a second cannot follow. Hence N = 2 and B = 34/47 versus
the RR class's 3 and 52/65 — scanout's bound *improved* relative to the
believed-but-false 40. The same harness asserts client 0's bound alongside
client 1's, so "scanout is granted within a finite, known bound" is a proof
obligation of `mem_vram_arbiter_liveness`, not a side remark. The separate
zero-starvation law (`scanout_preempted == 0` under mixed load, §2 bandwidth
budget) is unaffected: it counts non-override grants of offers latched while
scanout was eligible, a mechanism the corrected derivation does not touch, and
the directed `bandwidth_budget` test still asserts zero.

**Note on `REFRESH_URGENT`.** `zhao_sdram_params_pkg` sets it to 40 and its
comment justified that as "= the arbiter liveness bound". That justification
is now stale — it is simply a 40-cycle deferral budget, and it is *not*
re-pinned to 65 here: doing so would move `CNT_HARD` and invalidate the
cycle-exact SDRAM directed tests and the `zref` oracle, which the ratification
did not authorise. Recorded as a follow-up, not silently coupled.
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
| FB slot 0 | `0x0000_0000` | `0x0003_C000` (245,760) | frame display slot 0 (DRAM bank 0) |
| FB slot 1 | `0x0200_0000` (~~`0x0003_C000`~~, superseded — see below) | `0x0003_C000` (245,760) | frame display slot 1 (DRAM bank 1) |
| (unmapped) | everything else — including the hole between the slots | — | any access = violation |

**Bank split (RATIFIED 2026-08-16 — W2.7 composition finding).** Slot 1
originally sat at ~~`0x0003_C000`~~ — the SAME DRAM bank as slot 0 (the
controller takes banks from byte-address bits [26:25]); the superseded value
is kept visible rather than rewritten (charter honesty discipline, the
arbiter-bound precedent). Composing the real scanout-read + blit-write
streams for the first time measured ~82 of 192 Duo lines starved per frame
from read↔write row thrash. Placing slot 1 in bank 1 removes the thrash
structurally (each stream keeps its row open); it does NOT by itself buy
60 Hz full-canvas blit cadence (the W2.7 dossier quantifies what remains —
reports/status/phase2_wave2.md). The two slots are now DISJOINT regions:
the guard checks per-slot containment, and the hole between them is a
violation. Ratification (with conditions: ZH-004 must confirm the real
device's bank mapping, the slot-1 base being the retune knob; the MiSTer
SDRAM integration inherits "FB slots in distinct banks"):
`runs/CLAUDE-RUNS/RUN-20260814-2154-wave2-phase2-console-shell/RATIFICATION-fb-slot-bank-split.md`.

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

## 5b. Local-SDRAM bank 2 — the terrain world layer (ruling T2, 2026-09-02)

The Phase-2 note above says "later phases extend the map (texture/terrain/
particle pools per the charter allocator)". This is that extension for terrain,
ruled rather than proposed.

| Range | Region | Shape |
|---|---|---|
| `0x0400_0000` .. `0x054D_FFFF` | `TERRAIN.PAGE_POOL` | 1,024 × 21,376 B |
| `0x054E_0000` .. `0x0565_FFFF` | `TERRAIN.RESIDENT_MIP_POOL` | 1,024 × 1,536 B |
| `0x0566_0000` .. `0x056E_FFFF` | `TERRAIN.COMPOSED_HEIGHT` | 256 × 2,304 B |
| `0x056F_0000` .. `0x0577_FFFF` | `TERRAIN.COMPOSED_VELOCITY` | 256 × 2,304 B |
| `0x0578_0000` .. `0x057F_FFFF` | `TERRAIN.WRITEBACK_STAGING` / journal | 64 × 8 KiB |
| `0x0580_0000` .. `0x0585_FFFF` | `TERRAIN.COMPOSED_MIP_POOL` | 256 × 1,536 B |
| `0x0586_0000` .. `0x05FF_FFFF` | reserved / unmapped | until traces justify |

**There are no separate permanent E/F/H pools** — those layers live *inside* the
21,376-byte page. The two mip pools are **derived caches, not canonical
assets**: losing one costs a regeneration, not data.

**Every region starts DENY-BY-DEFAULT, with state-aware permissions.** This is
stronger than the Phase-2 rule and the difference matters:

* a loader may write **only a `LOADING` slot**;
* active terrain may read **only a ready slot with matching epoch and
  generation**;
* bake and stamp write **only owned layer ranges**.

A permission that depends on the slot's *state* cannot be checked once at
configuration time, which is why the residency directory and the guard are
coupled rather than independent.

## 5c. Local-SDRAM bank 3 — GEOM.PARAMBUF (ruling R7)

| Range | Region | Size |
|---|---|---|
| `0x0600_0000` .. `0x063F_FFFF` | PARAMBUF view 0 | 4 MiB |
| `0x0640_0000` .. `0x067F_FFFF` | PARAMBUF view 1 | 4 MiB |
| `0x0680_0000` .. `0x069F_FFFF` | shared prefetch / chunk scratch | 2 MiB |
| `0x06A0_0000` .. `0x07FF_FFFF` | reserved / unmapped | pending evidence |

See `design/contracts/GEOM.PARAMBUF.md`. ENGINE1 owns the render-geometry
region; the two views are disjoint for the same reason the two FB slots are.

## 5f. The GEOM asset pool — the Phase-3 region (2026-09-04)

§5 promised that "later phases extend the map (texture/terrain/particle pools
per the charter allocator)". **This is that extension for geometry, and it is
the single thing that has kept the geometry front end out of the console.**

| Range | Region | Size | Owner | Access |
|---|---|---|---|---|
| `0x06A0_0000` .. `0x07FF_FFFF` | `GEOM.ASSET_POOL` | 22 MiB | `ENGINE1` | **read-only** |

### Why it was the blocker

`zhao_geom_meshfetch.sv` is the **only** `zhao_guard_req_t` client in the whole
geometry subsystem — every other block takes its bytes through a caller-fed
port. Every region `MEM.GUARD` knew was a FRAME BUFFER region, and its client
case ended `default: pass_ok = 1'b0`. So every meshlet descriptor read was
denied **by design**, and docket D22's "nineteen unwired blocks" was one memory
path wearing eighteen disguises.

### Why bank 3

Banks come from byte-address bits `[26:25]`, and W2.7 measured **~82 of 192 Duo
lines starved** when two streams shared a bank. Banks 0 and 1 are the FB slots
scanout reads every frame; bank 2 is terrain (ruling T2). Bank 3 holds
`GEOM.PARAMBUF` (ruling R7) and reserved `0x06A0_0000..0x07FF_FFFF` "pending
evidence".

Mesh assets are `ENGINE1` render-geometry traffic **exactly as PARAMBUF is** —
same domain, same pipeline phase, already behind one local arbiter (§5d) — so
they serialise against each other rather than thrashing against scanout. That
is the W2.7 lesson applied rather than re-learned.

The pool ends at `0x0800_0000`, exactly the top of the 27-bit VRAM map, so
`BASE + SPAN` cannot wrap 32 bits — the defect the blit clamp exists for.

### What keeps the no-escape theorem intact

This is a **third window**, not a third client admitted to an existing one — the
ENGINE0 change was the latter and this is honestly not. Two things carry it:

* **Read-only.** `!req.write`. Nothing forwarded into this region can alter a
  frame buffer, whatever else is true.
* **Constant bounds.** No map input is consulted, so unlike the blit window it
  is not frame-scoped and cannot be moved by a grant.

`tests/formal/formal_mem_guard.sv` widened **with** the region rather than
around it: `a1_map` now reads `slot0 || slot1 || asset` instead of exempting
ENGINE1 from a two-slot assertion, which is the shape that keeps a proof green
by shrinking what it covers. Added: `a1_asset_ro` (no forward into the pool is
ever a write) and `c_forward_asset` (non-vacuity — the exact failure this
harness's own header records having shipped once). **bmc and cover both pass,
and deleting `!req.write` fails `a1_asset_ro` and `a1_region` at step 4.**

### It is a knob, and what is NOT decided here

`ZHAO_GEOM_ASSET_BASE` / `ZHAO_GEOM_ASSET_SPAN` are named editable constants in
`zhao_pkg`; nothing derives them and the pool can move to any unmapped range.

**Not decided:** the pool's internal layout (descriptors vs index streams vs
vertex records), whether PARAMBUF and assets need separate local arbiter
priorities, and whether 22 MiB is the right size — that wants a real asset set,
not a guess. The region admits reads; how it is carved up is the asset
fetcher's business and is still open.

## 5d. Memory clients — one addition (ruling T3)

`ENGINE0` (framebuffer / render write) and `ENGINE1` (render-geometry domain,
including GEOM.PARAMBUF and active terrain page/composed traffic behind a local
arbiter) are **unchanged**.

**Added: `ZHAO_CLIENT_TERRAIN_BUILD = 6`** — a **best-effort / background**
client for HPS→local page loads, F-sheet writeback, prefetch, and staging or
journal traffic.

**It does not join guaranteed round-robin merely because a page is late.** When
a page is not ready the renderer uses a declared proxy and records pressure,
rather than stealing scanout or render service. A streaming miss is a picture
problem for one frame; a starved scanout is a broken console.

**Client ID 5 remains available** for a measured split if board evidence proves
ENGINE1 arbitration is the limiter. **Do not spend it pre-emptively.**

Re-run the MEM.ARBITER liveness and guard proofs after adding client 6.

## 5e. The canonical terrain key (ruling T1)

    { resource_epoch:u32, island_id:u32, patch_ix:i16, patch_iz:i16 }

**Patch pitch is a property of the island table**, validated against the page
header, and is **not part of the lookup key**. No global coordinate projection
is identity.

**Two islands may legally overlap in local patch coordinates.** That is the
clause that makes `island_id` load-bearing rather than decorative, and any
residency interface without it is **superseded** — including the direct-mapped
prototype in `fpga/rtl/terrain/zhao_terrain_residency.sv`, which keys on
`{px, py}` alone.

Residency mapping is **256 sets × 4 ways** (T9), set index **CRC-8/ATM,
polynomial `0x07`, initial 0**, over the little-endian bytes of
`{island_id, patch_ix, patch_iz}`, with `resource_epoch[7:0]` xored into the
final byte. The full key is stored in every way.

## 5f. The permanent terrain command ABI (ruling T5)

Two versioned commands. **Opcodes are confirmed by the ZIDL generator before
commit**, not by this table.

    TerrainEpoch @ 0x0220, size 16
      epoch:u32; op:u8 (BEGIN=0, END_FLUSH=1, ABORT=2); flags:u8;
      reserved:u16; island_table_handle:u32; source_id:u32

    SubmitTerrainSet @ 0x0230, size 32
      resource_epoch:u32; list_offset:u32; list_bytes:u32; list_crc32c:u32;
      patch_count:u16; view_mask:u8; flags:u8; sequence:u32; reserved0:u32;
      reserved1:u32

    Patch-list record, 32 B
      island_id:u32; patch_ix:i16; patch_iz:i16; hps_page_addr:u64;
      expected_page_crc32c:u32;
      flags:u16 (REQUIRED, PREFETCH, DYNAMIC, DUAL, HAS_SAVED_F);
      view_mask:u8; priority:u8; source_id:u32; reserved:u32

**One `SubmitTerrainSet` covers the whole required + prefetch set.** Do not emit
one `DrawProcedural` per patch and do not overload an existing terrain field
command.

**Canonical order:** required before prefetch; smaller priority first;
view-union key; `island_id` ascending; `patch_iz` ascending; `patch_ix`
ascending; `source_id` ascending.

**The sealed list is capture data — replay does not rerun the HPS visibility
walk.** A replay that produces different list bytes has already failed before
any pixel is compared.

The software side of all of this is `design/contracts/SW.STREAM.md`.

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
