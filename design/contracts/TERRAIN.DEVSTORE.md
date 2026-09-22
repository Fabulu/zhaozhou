# TERRAIN.DEVSTORE — the per-resident-page deviation and history store

**Block:** `fpga/rtl/terrain/zhao_terrain_devstore.sv`
**Ledger:** `design/blocks.yml`, id `TERRAIN.DEVSTORE`
**Rulings:** R8 (which reading), R22 (the boundary), **R24** (when, and the
key), R59 (both sizes), R234 D3 (the M10K grant, now spent differently),
**R242** (the medium)
**Law:** `spec/terrain_rules.md` §4.2, `spec/memory_rules.md` §5b
**Region:** `zhao_pkg` `ZHAO_TERRAIN_DEVSTORE_BASE` / `_SPAN`

---

## 1. Why it exists

`zhao_terrain_lod` needs six things per subpatch that it deliberately does not
keep: three stored coarse-level deviations and the previous frame's history
(`prev_level`, `prev_morph`, `hold`). Its own law 5 says so — *"THE HISTORY
RIDES THE PACKET … for the caller to store. REJECTED: an internal history
RAM"*. Something has to be that caller.

The deviations are produced by `zhao_terrain_loddev` (R8) through
`zhao_terrain_lodfeed`, **at page load**, and consumed **per frame** by
`zhao_terrain_spdesc`. This block is the store across that gap.

**Recomputing per frame is not an alternative and the number is measured, not
argued.** `zhao_terrain_loddev` spends ~13,700 clocks on one patch's one
surface. `spec/terrain_rules.md` §4.2 puts 256 live patches in a frame and the
frame is 1.67M gpu clocks, so the visible set is ~3.5M clocks — **twice the
frame, before anything is drawn.**

## 2. The laws this block OWNS

1. **The key is the RESIDENCY PAGE SLOT** (R24). Not a compose slot: a compose
   slot is reused by a different patch from one frame to the next, so the
   history — which *is* the hysteresis — would be inherited by a stranger.
2. **An unwritten slot answers `DEV_MAX`, never zero.** `dev = 0` passes every
   rung of `zhao_terrain_lod`'s ladder, so the patch draws at level 3, the
   coarsest, and mush is indistinguishable from distance. `DEV_MAX` fails every
   rung: full detail. The cost of this fail-safe is triangles; the cost of the
   other one is a wrong picture nobody can attribute.
3. **A memory fault takes the same exit as an unwritten slot.** A guard denial
   or a short burst degrades the patch to "no records" and is counted
   separately. It does **not** abandon the read: a consumer is waiting on
   `r_valid_o`, so an abort would present a memory fault as a hang in
   `zhao_terrain_spdesc`.
4. **A slot with no committed history answers the neutral triple**
   `{level 0, morph 0, hold 0}` and is counted.
5. **An invalidation drops BOTH the records and the history.** `inv_valid_i`
   cannot distinguish a claim from a bake, so the safe direction is taken for
   both; after a bake the patch is drawn at full detail that frame anyway, so
   the hysteresis it loses was not going to be used.

## 3. What it does NOT own

* It does **not compute** a deviation. That is `zhao_terrain_loddev` (R8) with
  `zref::terrain::lod_deviation` as its oracle.
* It does **not decide** a level, a morph or a hold. That is
  `zhao_terrain_lod`; this block stores what it is handed.
* It does **not** decide residency, publish, or check a page CRC.
* It does **not** carry a `src_id` column. The store is read with a slot the
  caller already holds, and `zhao_terrain_spdesc` emits the id it popped beside
  that slot from the compose door.
* It does **not** hold the underside's records. TERRAIN.LOD law 7 gives the
  underside the top's level, so a second surface can be computed and thrown
  away with no effect on one output bit — R59's smaller form, taken.

## 4. The interface

| Face | Ports | Shape |
|---|---|---|
| producer | `w_valid_i`/`w_ready_o`, `w_slot_i`, `w_sp_i`, `w_dev1/2/3_i`, `w_cy_i`, `w_patch_done_o` | ready/valid, records in subpatch order 0..15 |
| invalidate | `inv_valid_i`, `inv_slot_i` | pulse |
| consumer | `r_start_i`/`r_ready_o`, `r_slot_i`, `r_valid_o`/`r_ready_i`, `r_sp_o`, `r_dev1/2/3_o`, `r_cy_o`, `r_prev_level_o`, `r_prev_morph_o`, `r_hold_o`, `r_fresh_o` | one start, sixteen descriptors |
| writeback | `h_valid_i`/`h_ready_o`, `h_level_i`, `h_morph_i`, `h_hold_i` | ready/valid, same order |
| memory | `guard_req_o`/`guard_rsp_i`, `beat_*_i`, `guard_w*` | `zhao_guard_req_t`, one shape only |

**`w_ready_o` is NOT constant.** Before R242 it was tied high; the store now
backpressures for the length of one 64-byte burst every fourth record.
`zhao_terrain_loddev` honours `dev_ready_i`, so this is a handshake the
producer already implements, and a record arrives roughly every 800 clocks
against a burst of order fifteen.

## 5. The storage (ruling R242, 2026-09-22)

> *Fabian: "Move deviation store to SDRAM, ignore stale info."*

R24's storage clause — *"stored alongside the page in M10K (the owner prefers
M10K over ALMs)"* — is superseded and **only** that clause. R87 ended M10K's
slack. **185 M10K returned: 141 for the deviations, 44 for the history.**

One slot owns **320 bytes** in two sub-pools, both with power-of-two strides so
every address is a shift:

```
deviations   DEV_BASE  + slot*256   4 bursts   16 records x 16 B
history      HIST_BASE + slot*64    1 burst    2+MORPHW+8 = 27 b x 16 = 54 B
```

**A record is 88 bits padded to 128**, so a 64-byte burst is exactly four
records and no record straddles a boundary. The padding buys ~896 flops of
staging against 80 KiB of a reserved region; ALMs are the binding constraint
and this SDRAM is not.

**The 67-bit packing stays REFUSED** (R242 keeps the refusal): it is exact only
while every lattice height remains `height16 << 8`, an invariant that is
upstream, unenforced and invisible, and entry I34's field lane is live work.

**One request shape:** 64-byte aligned, `len` 64, `be` all ones. MEM.GUARD
refuses a sparse `be` and `zhao_vram_arbiter` never sees a byte mask at all.

## 6. Request identity, and the checks that can actually fire

`m_addr_q` is latched when the engine takes an op and is the only source of
`guard_req_o.addr`. **No queued request is re-validated against a later view of
`r_slot_i` or `w_slot_i`** — the defect this repository records as its worst,
where a bank registered its read unconditionally and delivered response A's
data with B's metadata while every counter balanced.

| counter | sees | fired by |
|---|---|---|
| `read_unwritten_o` | a slot served before its records exist | reading an unwalked slot |
| `hist_unwritten_o` | a slot served before its history exists | ditto; silent afterwards |
| `hist_step_bad_o` | the writeback out of step with the descriptor stream | a seventeenth history record |
| `guard_denied_o` | MEM.GUARD refused a request | a played guard set to deny |
| `short_burst_o` | a read burst ended before its eighth beat | a played guard set to truncate |
| `stray_beat_o` | a beat with no read in flight — the **wrong-demux** fault | an injected beat |
| `slot_addr_bad_o` | the held address is not the one this patch needs | **committed mutant** |

`hist_step_bad_o` and `slot_addr_bad_o` each difference two registers loaded by
**different enables**, which is what makes them capable of firing at all.
`slot_addr_bad_o` is unreachable with legal stimulus, so it owes a committed
mutant: `tests/mutants/zhao_terrain_devstore_addrlatch_mutant.sv`, driven with
inverted polarity by `terrain_devstore_addrmut_fires`, with
`terrain_devstore_addrmut_silent` as the negative control that shows the macro
seam engaged.

**`stray_beat_o` is the one the address round trip structurally cannot
replace.** `zhao_mem_share_wr` broadcasts beat data and demuxes only
`beat_valid`, so a wrong demux delivers another requester's bytes with every
request/response counter still balancing. Beat COUNT is the only witness.

## 7. Evidence

* `tests/terrain/terrain_lodpath_directed.cpp` — 472 checks, the full chain
  `lodfeed -> loddev -> devstore -> played fabric -> read`, with a **real**
  `zhao_mem_guard` watching every request. Case 2 proves the value traverses
  bit-for-bit against `zref::terrain::lod_deviation`; cases 8–13 prove the
  traffic counts, the guard's silence, and every fault counter above.
* `tests/terrain/terrain_devstore_addrmut.cpp` — the mutant pair.
* `tests/formal/mem_guard_no_escape.sby` — the region's theorems, with
  `c_forward_devstore_wr` / `_rd` reached, and
  `tests/formal/mem_guard_devbound_mutant.sby` the deliberate fault.
* `tests/memory/mem_guard_directed.cpp` — the window's edges, straddles and
  wrong clients, cross-checked against `zref::MemoryGuard`.

### What the CONSOLE SMOKE witnesses, and what it does not

Said precisely, because "an otherwise green smoke whose upstream fixture never
reaches the new path does not prove the path."

**The WRITE path IS exercised by real console stimulus.** Measured in the
plain form, 2026-09-22:

```
pl    loaded=3 faulted=0 crc_fails=0 guard_denied=0
mip   mipfeed pages_mipped=3 samples_sent=6534
lodfd lattices_walked=3 dev_records=48 -> hist events=144 updates=48
      probe ... socket contention=10 retire_unowned=0 wbeat_unowned=0
```

Three pages load, three lattices are walked, and **48 deviation records reach
this block's write port** — twelve 64-byte guard write bursts into
`TERRAIN.DEVSTORE` through the **real** `zhao_mem_guard` in the composed
console, on a five-requester `zhao_mem_share_wr`. `wbeat_unowned = 0` is a live
`$fatal` in that bench, so a write beat from a requester that did not own the
channel would stop the run.

**A stale claim, corrected here rather than inherited.**
`tb_zhao_console_core_smoke.sv` still says *"every terrain page this bench
plays FAILS ITS CRC"*. It does not: `loaded=3 faulted=0 crc_fails=0`. The page
header loop landed 2026-09-20 and the sentence was never updated — and it was
wrong about the cause even when it was true (the faults were `hdr_ident`, not
CRC, as that file's own later note records).

**The READ path is NOT exercised there.** No compose job is issued in this
fixture, so `r_start_i` never rises and no record is ever read back. That half
rests on `terrain_lodpath_directed` and on the formal proof, not on the smoke.

## 8. Open, and stated rather than left to be found

* **The counters are sunk at the console boundary.** `zhao_console_core`
  declares them and nothing reads them from a board. The honest closure is
  MEASURE.GOVERNOR's catalog taking them, which is a register-widening act.
* **The read is on ruling T3's BACKGROUND client.** Client 6 sits below every
  guaranteed client, and this read is frame-critical. A frame whose page loads
  saturate the socket delays LOD decisions; `rd_wait_clocks_o` measures it.
  Spending client id 5 is forbidden pre-emptively by T3 and is an ABI act R242
  did not authorise.
* **The window is spatially the WHOLE store, not the slot a job owns.** The
  same state-awareness gap MEM.GUARD's page-pool arm records, for the same
  reason. The residual is one page drawn at the wrong LOD, which
  `slot_addr_bad_o` watches from inside the block.
* **No fit has measured this block.** The M10K delta is derived from the
  parameters and the flop delta is hand-counted; neither is a Quartus number.
