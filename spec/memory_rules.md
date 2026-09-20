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
- Guard acceptance and verdict are different events. A master holds the complete
  request through `req.valid && rsp.ready`, drops valid immediately after that
  edge, then receives exactly one registered `rsp.ok` or `rsp.violation` pulse.
  A denied request occupies no forward slot, so `rsp.ready` may be high during
  its violation pulse; that level is not permission to resubmit it.
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

### What is ENACTED, 2026-09-06 — and what is not

**Exactly one of the six regions above is in `MEM.GUARD` today.**

| Range | Region | Owner | Access |
|---|---|---|---|
| `0x0400_0000` .. `0x054D_FFFF` | `TERRAIN.PAGE_POOL` | `TERRAIN_BUILD` (client 6) | **read + write** |

`zhao_pkg` carries it as `ZHAO_TERRAIN_PAGE_POOL_BASE` / `_SPAN`
(`0x0400_0000` / `0x014E_0000`); 1,024 × 21,376 = 21,889,024 = `0x014E_0000`,
so the half-open end `0x054E_0000` is the ruling's inclusive `0x054D_FFFF` to
the byte. Both are elaboration-time constants — no map input reaches this
window, so unlike the framebuffer one it is **not frame-scoped**, and
`BASE + SPAN` cannot wrap 32 bits.

**Both directions, as TWO ARMS over the SAME constants — and each arrived with
its block, never ahead of it.**

* `terrain_ok` (**write**) landed with `TERRAIN.PAGELOADER`, which deposits pages
  and never reads local SDRAM.
* `terrain_rd_ok` (**read**) landed with `TERRAIN.WRITEBACK` on 2026-09-06.
  Ruling T4 *requires* layer F to be copied to the HPS journal on dirty eviction,
  and T2 puts layer F **inside** the 21,376-byte page — so there is no F pool to
  read and the sheet is reachable only through this window. The read is
  therefore ruled traffic for this same client, not a widening of convenience.

The arms are separate assigns over identical constants. One comparison would
synthesise the same thing; they are kept apart because the **proof** states the
directions separately — `a1_terrain_wr_owner` and `a1_terrain_rd_owner`, each
with its own non-vacuity cover, so a regression names which direction broke and
a merged arm cannot satisfy both covers while proving neither.

**A read cannot alter a framebuffer**, the same argument that carried
`RENDER.ASSET_POOL`, and here it holds twice: a forwarded read carries no write
data, and these constant bounds are disjoint from both FB slots and from the
asset pool. The widening is confined to bank 2.

**A narrower read window was looked for and does not exist cheaply.** Layer F
sits at page byte 10,694, so restricting reads to the F extents needs
`addr mod 21,376` — not a power of two — on the guard's verdict path: the same
obstacle recorded below for state-awareness. Client ID 5 was not spent on a
separate reader, because T3 forbids spending it pre-emptively and a second
client in one region is not narrower anyway.

**The residual the read creates**, stated like the write one: client 6 may read
*any* page in the pool, so a faulty writeback could journal another patch's
scars. MEM.GUARD cannot see that. `TERRAIN.WRITEBACK` answers it by reading the
evicted page's 64-byte header FIRST and refusing (`kSheetHeaderIdent`) before a
single journal byte moves if `{island_id, patch_ix, patch_iz}` does not match
the job — §2.1's "redundancy is a corruption check", spent on exactly the
failure this arm creates.

**The other five bank-2 regions are still unmapped**, `default: pass_ok = 1'b0`.
A window is opened WITH the block that writes it, never ahead of it.

**STATE-AWARENESS IS NOT ENACTED.** "A loader may write only a `LOADING` slot"
needs residency state the guard does not have: one muxed request port, no slot
context, and an address→slot decode by 21,376 (not a power of two) that would
land on the verdict path of the block whose counter enable was already the
largest negative-slack family in the composed shell. The interface that would
carry that state — who owns the bitmap, how it crosses to the guard, what a
stale copy means — is not ruled anywhere and was not invented.

The residual, precisely: a faulty TERRAIN_BUILD may write a page slot other than
the one it was told to load. It cannot reach a framebuffer, the asset pool, the
rest of bank 2, or anything outside the map — the no-escape theorem is unchanged
— so the worst case is corrupt ground for one page, caught by the CRC the
directory checks before publishing. Tightening this to the LOADING slot is
separate, ruled work and it needs the residency→guard interface first.

## 5c. Local-SDRAM bank 3 — GEOM.PARAMBUF (ruling R7)

| Range | Region | Size |
|---|---|---|
| `0x0600_0000` .. `0x063F_FFFF` | PARAMBUF view 0 | 4 MiB |
| `0x0640_0000` .. `0x067F_FFFF` | PARAMBUF view 1 | 4 MiB |
| `0x0680_0000` .. `0x069F_FFFF` | shared prefetch / chunk scratch | 2 MiB |
| `0x06A0_0000` .. `0x07FF_FFFF` | reserved / unmapped | pending evidence |

See `design/contracts/GEOM.PARAMBUF.md`. ENGINE1 owns the render-geometry
region; the two views are disjoint for the same reason the two FB slots are.

## 5f. The shared render asset pool — Phase 3 / Packet E

§5 promised that "later phases extend the map (texture/terrain/particle pools
per the charter allocator)". Phase 3 opened this window for geometry; Packet E
does **not** widen it. Packet E serializes immutable texture-line reads with the
existing geometry adapter behind one local ENGINE1 mux, preserving the same
client, direction, bounds and global arbiter slot. Client 5 remains unspent.
The original geometry blocker and evidence remain below.

| Range | Region | Size | Owner | Access |
|---|---|---|---|---|
| `0x06A0_0000` .. `0x07FF_FFFF` | `RENDER.ASSET_POOL` | 22 MiB | `ENGINE1` | **read-only** |
| (inside the above) the published-resource region | `RENDER.ASSET_POOL` | `cfg_region_*` | `TERRAIN_BUILD` (MEM.UPLOAD) | **write-only**, ruling R32 |

**Owner ruling R32 (provisional, 2026-09-19).** `ENGINE1` stays the pool's one
READER. `TERRAIN_BUILD` -- MEM.UPLOAD's client -- may WRITE the one
published-resource region (`res_base`, `res_span`: the host-configured region
MEM.UPLOAD bounds every request against), and only while that region lies
wholly inside the pool; a region straddling either edge, or whose end wraps 32
bits, closes the arm whole rather than being clamped. The bound is the guard's
second non-constant one (the framebuffer lease is the first), so it gets the
same discipline: `mem_guard_no_escape` re-proved with a free region, and a
committed mutant that widens the bound makes that proof fail.

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
around it: `a1_map` reads both slots, render assets and terrain instead of
exempting ENGINE1 from scope. `a1_render_asset_ro` and
`a1_render_asset_owner` pin direction/owner; `a1_no_forward_client5` pins the
unspent slot. Separate `c_forward_render_asset_16/32/64` covers prevent the new
texture shape from hiding dead geometry shapes, and `c_client5_denied` proves an
accepted client-5 request reaches the denial verdict. The earlier bmc/cover and
mutation evidence remains in the formal-run ledger under its historical
`a1_asset_ro`/`c_forward_asset` names.

### It is a knob, and what is NOT decided here

`ZHAO_RENDER_ASSET_BASE` / `ZHAO_RENDER_ASSET_SPAN` are the named editable constants in `zhao_pkg`; historical `ZHAO_GEOM_ASSET_*` names are aliases, not a second region authority. Nothing derives the values and the pool can move to any unmapped range.

**Not decided:** the pool's internal layout (descriptors vs index streams vs
vertex records), whether PARAMBUF and assets need separate local arbiter
priorities, and whether 22 MiB is the right size — that wants a real asset set,
not a guess. The region admits reads; how it is carved up is the asset
fetcher's business and is still open.

### 5f.1 A PUBLISHED SLOT IS NAMED BY THE HANDLE INDEX OF THE RESOURCE IT HOLDS

Ruled 2026-09-19. This is the one sentence the paragraph above left open that
something was actually waiting on, and it is a *naming* rule, not a layout one —
the pool's internal carve-up above stays undecided and this does not decide it.

**The rule.** For a resource named by a `handle32 {index:24, generation:8}` and
made resident by `MEM.UPLOAD`, the arena slot the bytes were uploaded into is
NAMED BY THAT RESOURCE'S HANDLE INDEX. So the residency directory for such a
resource is

    key   {index:24}
    row   {slot:8, base:32, extent:32, kind:8}

`base` and `extent` are the destination address and length `MEM.UPLOAD` already
bounds-checks against `cfg_region_*` before it writes a byte, so the directory
cannot name a range the guard would refuse; `kind` is the `.zpak` resource kind
that already travels as `publish_tag_o` (`spec/cartridge.md` §4a: 10
`TEXTURE_PAGE` 0x000E, 11 `MATERIAL_SET` 0x000F, 12 `MESH_STREAM` 0x0010);
`slot` is `publish_slot_o`. The generation stays 16-bit and is `MEM.UPLOAD`'s
own — owner ruling D-3's cache tag keys on all sixteen bits, and a directory
that kept only the eight the handle carries would alias every 256th publication.

**Why the INDEX and not the slot.** The index is the name the *game* uses; the
slot is where the allocator happened to put it. A directory keyed on the slot
can only answer "what is in slot 7", which is the question nobody asks — a
consumer holds a handle. Keying on the index means a resolve that finds no row
is a residency fault with a name, which is what lets `MATERIAL.RESOLVE`'s "a
miss STALLS, it never guesses" law mean something: the alternative, resolving
confidently to whatever occupies the slot, is a well-formed record for the
wrong surface with every counter green.

**What this obliges `MEM.UPLOAD` to carry.** The index must ride the REQUEST —
`req_index_i` — and be published beside the slot. It is NOT derivable from
anything else the block holds: `req_tag_i` is 8 bits of "which consumer asked"
and `req_dst_slot_i` is 8 bits of arena slot, and neither is a 24-bit resource
name. A publication that omitted it would name a row nobody can look up.

**SCOPE, stated because the tree contains a second, different slot naming.**
This rule governs resources published by `MEM.UPLOAD` under a handle32. It does
NOT govern:

* **TERRAIN pages.** Ruling T10's handle is `{resource_epoch:u32, slot:u10,
  generation:u8}` — it names its slot *inside the handle* and carries no 24-bit
  index. Terrain residency is `zhao_terrain_residency_v2`'s own 256×4
  set-associative directory on ruling T1's canonical key, and that directory
  keeps its own law.
* **Framebuffer slots.** `DEBUG.FRAMEBLIT`'s `publish_slot_o` is a one-bit FB
  slot consumed by `VIDEO.SLOTMGR`. Same word, different arena, unaffected.

Searched before ruling, because a directory that resolves confidently to the
wrong surface is exactly what this is supposed to prevent: every
`publish_slot`/`publish_tag`/`publish_generation` site in `fpga/`, `tests/`,
`spec/`, `design/` and `reference/`. `MEM.UPLOAD`'s publication has **no
consumer at all** today — the only other `publish_slot_*` in the tree is the
framebuffer pair above — so no block held a conflicting meaning for it.

## 5g. ENGINE0 under the render lease: POST.COMPOSITE's read, POST.ECHO's capture (2026-09-19)

Two ENGINE0 arms, added with the blocks that use them (core entries I15/I16,
owner ruling R7). **No client id is spent** -- client 5 stays unspent (T3) --
and no FB window moves.

| Range | Region | Owner | Access |
|---|---|---|---|
| the leased FB slot window | (unchanged) | `ENGINE0`, while `fb_writer == 1` | **read + write** (was write) |
| `0x05C0_0000` .. `0x05C3_BFFF` | `POST.ECHO` capture | `ENGINE0`, while `fb_writer == 1` | **write-only** |

**The read arm is POST.COMPOSITE's lease, not a new permission.**
`POST.COMPOSITE.md`: "POST.COMPOSITE owns an exclusive framebuffer read/write
lease after resolve and before publication." Post runs inside the RENDER lease
-- after the raster has drained, before anything publishes -- so it carries the
render engine's identity. `zhao_post_lease` shares ENGINE0 between
RASTER.FBWRITE, the source read-back (`zhao_post_fbread`) and POST.ECHO through
one `zhao_mem_share_n`, the §5d/§5f pattern ("a client identity is a PRIVILEGE,
not a slot"). A read cannot alter a frame buffer; what the guard must stop is a
read OUTSIDE the lease, and `a1_engine0_rd_lease` proves it does.

**The capture is bank 2's reserved tail.** Banks 0 and 1 hold the FB slots and
the scanout reads one of them while post writes the other, so a capture there
would thrash a bank against scanout on every other frame (the W2.7 finding).
Bank 3 is full. `0x0586_0000..0x05FF_FFFF` was reserved "until traces justify";
the echo is that traffic. Span = one FB slot (0x3C000), so the largest stored
canvas (Duo, 196,608 B, views stacked) fits and a mode switch never moves it.
Constant bounds, not frame-scoped, disjoint from every other window
(`a1_echo_not_fb`), write-only (`a1_echo_wo`), ENGINE0's alone
(`a1_echo_owner`), lease-gated (`a1_echo_lease`).

**Proof re-run 2026-09-19** (`tests/formal/mem_guard_no_escape.sby`): bmc PASS
at depth 30; cover PASS with every cover reached, including one per ENGINE0 arm
(`c_forward_engine_wr`, `c_forward_engine_rd`, `c_forward_echo`). A scratch
mutant that drops the capture arm's lease term fails `a1_echo_lease` and
`a1_region` -- the new theorems fire.

**The route tripwire** in `zhao_shell_top_v2` learned ENGINE0 reads (lease-gated)
in the same edit. **ENGINE0 writes now wait for their data**: the arbiter sees an
ENGINE0 write only when all its words are queued and not owed (`wf_owed`), the
slot-6 gate applied to the framebuffer queue, because behind a share the
verdict reaches a writer later and the old latency race is no longer won by
construction.
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

**Enacted 2026-09-06.** `zhao_pkg::zhao_client_e` gains
`ZHAO_CLIENT_TERRAIN_BUILD = 3'd6`; 5 is left as a hole in the enum, which IS
the reservation. `zhao_vram_arbiter` widens from five client ports to **seven**
rather than packing terrain into slot 5, because in that block the ARRAY INDEX
IS THE CLIENT ID (`ctrl_req.client = zhao_client_e'(offer_client)`) — slot 3 is
ENGINE1 and slot 4 is DEBUG for exactly that reason. Port 5 is dead by
construction: no arbitration arm names it, and `port_grant[5]` is forced low so
a request there is REFUSED rather than latched into a slot that could never be
served.

TERRAIN_BUILD is arbitrated **below DEBUG**: served only when nothing else is
pending, never promoted for lateness. Its starvation under sustained load is the
ruled behaviour (the renderer draws a declared proxy and records pressure), not
a defect.

Both proofs were re-run, before and after. `mem_guard_no_escape`: bmc and cover
PASS both times, with `c_forward_terrain` reached at step 4 (non-vacuous).
`mem_vram_arbiter_liveness`: identical verdicts before and after — `bmc` PASS at
RR 52 / scanout 34, `bmc_tight_rr` and `bmc_tight_scanout` FAIL at bound−1 as
they must, `cover` PASS. **The bounds are still exact with seven ports**, which
is the evidence that a background client at the bottom of the order does not
move the guaranteed-client service law.

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

---

## 8. The HOST REGISTER WINDOW (owner ruling R51, 2026-09-20)

**THIS MAP IS FROZEN.** A tenant's region, and every register inside it, is a
number the ARM's debug tooling compiles against. Adding a tenant is additive
(populate the next free region); moving one is a wire change and needs a ruling.

Ruling R51: *"Ratify a HOST REGISTER WINDOW on the HPS lightweight bridge (a
read/write CSR aperture with a frozen address map in `spec/memory_rules.md`).
I19's histogram window and I45's readout are its first two tenants. It is
guarded like every other client (a region per tenant, no escape)."*

### 8.0 Why this is not §3's bridge

`MEM.HPS.BRIDGE` (§3) is the **h2f DATA bridge**: 64-byte-aligned bursts of
64-bit beats, one in flight per client, for moving bulk. A host that wants one
24-bit histogram bin cannot use it — the bin lives in a block's registers, not
in DRAM, and no agent copies it there.

This is the **h2f LIGHTWEIGHT bridge**, the Cyclone V's second, narrow, 32-bit
host port, whose purpose is exactly register access. It is a physical port of
the part, in the same class as the pad inputs and the FRAME_RING word view. In
Verilator the harness is the HPS, as it is for §3.

`DEBUG.COUNTERS` is a third thing and is not this either: it **streams**
`(counter_id, u64)` pairs in ascending catalog order at vblank
(`spec/counters.md`). That is a different protocol serving a different law, and
it is not address-mapped.

### 8.1 The aperture

| property | value |
|---|---|
| base (Cyclone V h2f_lw) | `0xFF20_0000` |
| size | 64 KiB (`AW` = 16 byte-address bits) |
| region stride | 4 KiB (`TENANT_LSB` = 12) |
| regions | 16 (`addr[15:12]`), of which **2 are populated** |
| access | 32-bit words only; `addr[1:0]` must be `00` |
| outstanding | one |

**No-escape is STRUCTURAL, not a bound check.** The tenant index is
`addr[15:12]` and the word offset handed to a tenant is `addr[11:2]` — ten bits
wide. There is no wire on which one tenant could be given another's address, so
the escape is not refused, it is unrepresentable. §5's `MEM.GUARD` compares
against bounds because *its* regions are runtime configuration; these are frozen
at elaboration, so the stronger form is available and is taken.

**Every refusal is ANSWERED and COUNTED. It never hangs** (ruling R20's law):

| condition | counter |
|---|---|
| `addr[1:0] != 00` | `refused_misaligned` |
| `addr[15:12] >= 2` (an unpopulated region) | `refused_unmapped` |
| the tenant refuses (no such register, or a write) | `refused_tenant` |
| the tenant does not answer within 255 cycles | `refused_timeout` |

`reads` and `writes` count what was **asked**, before any verdict — a census of
only the accesses that worked would read low, which is the flattering direction.

**No v1 tenant accepts a write**, and that is a decision rather than an
omission. The one thing a host would want to write, DEBUG.TRACE's arming,
travels in the command stream instead (`DebugTraceArm` 0xF003, ruling R52),
because ruling R18's principle — one authority per level — forbids a second
arming path. The write path is carried to the tenant and refused there, so a
third tenant that wants writes needs no change to the aperture.

### 8.2 Tenant 0 — `0x0000`..`0x0FFF`, MEASURE.HISTOGRAM

Closes `zhao_console_core` header entry I19. Read-only.

| word offset | byte | register |
|---|---|---|
| `0x000`..`0x03F` | `0x000`..`0x0FF` | **bin[n]** — the frozen interval's count for bin `n`, zero-extended from `CW` |
| `0x040` | `0x100` | `snap_total` — events accepted into the interval |
| `0x041` | `0x104` | `{ snap_valid, 15'b0, snap_src_id }` |
| `0x042` | `0x108` | `snap_index` — interval sequence number |
| `0x043` | `0x10C` | `events` |
| `0x044` | `0x110` | `updates` |
| `0x045` | `0x114` | `stall_cycles` |
| `0x046` | `0x118` | `bin_sat` |
| `0x047` | `0x11C` | `fwd_hits` |
| `0x048` | `0x120` | `host_conflict` |
| `0x049` | `0x124` | `snapshots` |
| `0x04A` | `0x128` | `frozen_write` |

**THE ADDRESS IS THE BIN.** There is no select register, deliberately: a
select/data pair is a race in a machine whose accumulator is live — two readers
interleave a select and a read and each gets the other's bin, with nothing able
to notice. Nothing is stateful between accesses here, so nothing can interleave
wrongly.

An offset at or above `2**BINW` is **refused**, not answered with zero: zero
would make a map error indistinguishable from an empty bin.

A bin read is a two-cycle handshake against the live block, not a register mux,
and it can hold off an accumulator group. The histogram counts that itself, on
`host_conflict` — readable at `0x120`, so a host can see the cost it is
imposing.

### 8.3 Tenant 1 — `0x1000`..`0x1FFF`, DEBUG.TRACE

Closes the readout half of header entry I45. Read-only.

| word offset | byte | register |
|---|---|---|
| `0x000`..`0x1FF` | `0x1000`..`0x17FF` | the ring, word-addressed as `{event[5:0], word[2:0]}` |
| `0x200` | `0x1800` | `armed` — the seven-bit stage mask |
| `0x201` | `0x1804` | `count` — events stored |
| `0x202` | `0x1808` | `dropped` — events lost to a full ring |

The eight words per event are `spec/capture_format.md` chunk `0x000A`'s layout:
`0` tile, `1` primitive, `2` pixel, `3` `{rsv[3]=0, stage}`, `4` expected_fx,
`5` actual_fx, `6` source_id, `7` command_seq.

`armed` is **readable and not writable**, which is the useful asymmetry: a host
can confirm what the command stream armed without being able to arm behind its
back.

### 8.4 Regions 2..15 — unpopulated

Refused and counted. They are the additive space for a third tenant.

### 8.5 Test obligations

* `tests/debug/host_regwin_directed.cpp` — the aperture's own directed test:
  every guard fired by legal stimulus, including a tenant that never
  acknowledges (which the composed console cannot produce, and which is a
  legal input at the block's port).
* `tests/prod/run_console_core_smoke.ps1` — the composed proof: the bench is
  the HPS, reads a trace ring word and a histogram register back through the
  aperture, and fires the unmapped, misaligned, no-such-register and
  read-only-write refusals in the running console.