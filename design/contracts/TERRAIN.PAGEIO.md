# Contract — TERRAIN.PAGEIO (bake's page window: layer D in, layers B and D out)

> Ledger: **no `design/blocks.yml` row yet, deliberately.** A mandatory
> capability that is not built is a gap, and this packet is forbidden to close
> one gap by opening another. The ledger row, the fit-target entry and the
> manifest entry land in the SAME commit as the RTL. Written 2026-09-20 by the
> terrain8 packet; nothing here is built.
>
> Law: `spec/terrain_rules.md` §2 (page layout), §3.3 (cell-state byte and the
> no_bake corner shadow), §3.4 (breach law), §7 (ownership).
> Page offsets: `reference/include/zref/zref_terrain_page.hpp:305-325`.
> Consumer contract: `design/contracts/TERRAIN.BAKE.md`.

---

## 1. Why this exists: FOUR absent owners that are ONE block

Entry I32 in `fpga/rtl/prod/zhao_console_core.sv` refuses to compose
TERRAIN.BAKE. The terrain7 packet (commit `78c644ca`) found the blocker that
had never been recorded — *"`sc_*`, bake's layer-B scar writeback, has no
consumer anywhere"* — and reported that the READ side was solved, because
`u_terrain_pagestream` emits bake's `vtx_base_i`/`vtx_scar_i`/`vtx_bottom_i`
port for port.

**That is true of three of bake's four page ports and false of the fourth, and
the direction of the error is the unflattering one: there is MORE missing, not
less.** Re-checked here rather than inherited, per the standing rule.

`zhao_terrain_bake_v2` touches the page on FOUR ports, not two:

| bake port | page layer | direction | who serves it today |
|---|---|---|---|
| `vtx_base_i` / `vtx_scar_i` / `vtx_bottom_i` (+`vtx_vi_o`/`vtx_vj_o`) | A, B, C | read | `u_terrain_pagestream` — **port for port**, needs only a cursor-match adapter (it PUSHES, bake PULLS) |
| `vtx_nobake_i` | **D** | read | **NOBODY** |
| `cell_state_i` (+`cell_ci_o`/`cell_cj_o`/`cell_valid_i`/`cell_ready_o`) | **D** | read | **NOBODY** |
| `sc_*` (`sc_scar_o`, `sc_vi_o`, `sc_vj_o`, …) | **B** | write | **NOBODY** |
| `cs_*` (`cs_state_o`, `cs_ci_o`, `cs_cj_o`, …) | **D** | write | **NOBODY** |

### The evidence for the layer-D absence, stated so it can be re-run

`zref_terrain_page.hpp:317` puts layer D at page offset **6,598**
(`kLayerDOff`), 32×32 u8, 1,024 bytes. Searched every `.sv`, `.v`, `.txt`,
`.qsf` and `.yml` under `fpga/` for `6598`, `6,598`, `\bD_OFF\b` and
`kLayerDOff`:

* **ZERO hits that are a page offset.** The only matches are `d_off`/`rd_off`,
  unrelated FIELD and CMD.DMA variables that a case-insensitive search picks
  up, and the string `6,598 ALM`, an area figure quoted in three headers.
* `zhao_terrain_pagestream.sv:251` reads exactly `'{A_OFF, B_OFF, C_OFF}` —
  three planes, and layer D is not one of them.
* `zhao_terrain_pageloader` writes WHOLE pages at load and reads none back.
* `zhao_terrain_writeback` reads and writes layer **F** only (`F_OFF = 10694`).

This is the same shape as terrain7's layer-E finding (offset **7,622**, also
zero hits under `fpga/` — re-verified here, still true, the only match being
terrain7's own note at `zhao_console_core.sv:1989`). **Two of the eight page
layers have no reader in the machine, and both are on TERRAIN.BAKE's critical
path.**

### The seam the core names is NOT this, and taking it would narrow function

`zhao_console_core.sv:2546-2551` names a seam that looks like the answer:
bake's `cs_event_o`/`cs_sub_o`/`cs_ci_o`/`cs_cj_o` onto
`zhao_terrain_compcache_front`'s `cs_we_i`/`cs_w_substance_i`/`cs_w_ci_i`/
`cs_w_cj_i`, with only a 6→5 bit address narrowing. It is a real and useful
seam and it is **not a page write**:

* `compcache_front` is an **on-chip M10K mirror of one patch**, double
  buffered. It has no VRAM port at all.
* It stores `logic [1:0] sub_m [2*CELLS]` — the SUBSTANCE field only. §3.3's
  cell-state byte is substance **plus flags**, and `kNoBakeBit` is bit 2. Bake
  itself preserves them: `cs_state_o <= {cell_state_i[7:2], sub_out}`. Through
  this seam the upper six bits are **dropped**, and `cs_state_o` — the byte
  that carries them — has no consumer on it at all.

So wiring that seam alone would make this frame's composed lattice see the new
substance while the page keeps the old one, the flags are lost, and the
deformation dies at the next page load. **A bake whose scar cannot be stored
has not deformed anything.** Both consumers are wanted; they are different
consumers, and only the page write discharges §7.

### Why one block and not two

Every one of the four unserved ports needs the same three things: the
residency **slot**, the **generation**, and a **guard socket on the terrain
page pool**. A block that holds those once can serve all four, and the
alternative — a layer-D reader and a separate page writer — pays for the job
port, the slot arithmetic and the guard verdict machinery twice, and creates a
second place where a bake's identity can be got wrong. `zhao_terrain_bake_v2`
processes **one record at a time** and its two phases are strictly sequential,
so one agent per bake is sufficient by construction.

**And it closes entry I27's first half for free**, which is the part worth
noticing. I27 records that `terr_dm_*` (the deformation mark: slot,
generation, epoch, and the `bd`/`f`/`mips` dirty bits) has no writer, and
terrain7 established that **`zhao_terrain_bake_v2` has no deformation-mark
port of any kind and cannot grow one honestly** — `cmd_patch_id_i` and
`cmd_src_id_i` are not residency handles. This block is the only thing in the
proposal that holds the slot and generation the patch was served under, and it
learns the bake is finished from `bake_done_o`. It is therefore the natural —
and, on present evidence, the only — writer of `terr_dm_*`.

---

## 2. Ports

Two faces. The **bake face** is bake's own ports mirrored, port for port and
width for width — no adaptation, no renaming, because a rename is where a
convention silently becomes a second convention.

```
  clk, rst_n

  ---- configuration ----------------------------------------------------
  zhao_client_e cfg_vram_client_i        // ZHAO_CLIENT_TERRAIN_BUILD
  logic [31:0]  cfg_epoch_i

  ---- job in (the patch this bake is against) --------------------------
  j_valid_i / j_ready_o
  logic [SLOTW-1:0] j_slot_i             // SLOTW = $clog2(SLOTS)+1, one bit
  logic [GENW-1:0]  j_gen_i              //   wider than the pool needs
  logic [31:0]      j_epoch_i
  logic [31:0]      j_src_id_i

  ---- MEM.GUARD client: READ and WRITE on the page pool ----------------
  zhao_guard_req_t guard_req_o           // .write moves; see section 4
  zhao_guard_rsp_t guard_rsp_i
  logic        beat_valid_i              // read beats in
  logic [63:0] beat_data_i
  logic        beat_last_i
  logic [63:0] guard_wdata_o             // write beats out
  logic        guard_wvalid_o
  logic        guard_wready_i
  logic        guard_wlast_o

  ---- the BAKE face: layer D read (dig phase) --------------------------
  logic [5:0] nb_vi_i, nb_vj_i           // -> bake.vtx_vi_o / vtx_vj_o
  logic       nb_req_i
  logic       nb_o                       // -> bake.vtx_nobake_i

  ---- the BAKE face: layer D read (breach phase) -----------------------
  logic [5:0] cell_ci_i, cell_cj_i       // <- bake.cell_ci_o / cell_cj_o
  logic       cell_valid_o               // -> bake.cell_valid_i
  logic       cell_ready_i               // <- bake.cell_ready_o
  logic [7:0] cell_state_o               // -> bake.cell_state_i  (FULL BYTE)

  ---- the BAKE face: layer B write -------------------------------------
  logic               sc_valid_i         // <- bake.sc_valid_o
  logic               sc_ready_o         // -> bake.sc_ready_i
  logic signed [15:0] sc_scar_i
  logic [5:0]         sc_vi_i, sc_vj_i

  ---- the BAKE face: layer D write -------------------------------------
  logic       cs_valid_i                 // <- bake.cs_valid_o
  logic       cs_ready_o
  logic [7:0] cs_state_i                 // THE FULL BYTE, flags included
  logic [5:0] cs_ci_i, cs_cj_i

  ---- retirement -------------------------------------------------------
  logic dig_done_i                       // <- bake.dig_done_o
  logic bake_done_i                      // <- bake.bake_done_o

  ---- the DEFORMATION MARK (entry I27's first half) --------------------
  dm_valid_o / dm_ready_i
  logic [SLOTW-1:0] dm_slot_o
  logic [GENW-1:0]  dm_gen_o
  logic [31:0]      dm_epoch_o
  logic dm_bd_o, dm_f_o, dm_mips_o       // see OWNER DECISION 3

  ---- completion and counters ------------------------------------------
  done_valid_o / done_ready_i / done_ok_o / done_verdict_o[3:0]
  pages_read_o, pages_written_o, guard_denied_o, bursts_read_o,
  bursts_written_o, stale_gen_o, idle_o
```

`sc_touched_o`, `sc_meets_o`, `sc_clamped_o`, `sc_src_id_o`, `cs_event_o`,
`cs_sub_o` and `cs_src_id_o` are bake's **observation** outputs, not page
state. They do not enter this block; they go to the trace and counter fabric
exactly as they do today.

---

## 3. What it MUST NOT own

* **It does not compose.** §3.4's `compose_top = max(fx(base)+fx(scar),
  fx(bottom))` is TERRAIN.PATCH's, with `zref::terrain::compose_vertex` its
  oracle. The same sentence is in `zhao_terrain_pagestream.sv`'s header and it
  is the same sentence here.
* **It does not evaluate the breach law.** §3.4's four-corner equality and the
  heal arm are bake's, and `terrain_rules` §7 permits exactly one owner.
* **It does not clamp, rail or saturate.** It moves bytes.
* **It does not decide residency**, publish, or verify the page CRC.
* **It does not choose which surface MIPGEN sees.** That is
  `zhao_terrain_pagestream.sv`'s open ruling and is untouched here.

The ONE law it does own is §3.3's **no_bake corner shadow**, because `nb_o` is
a reduced bit and something must reduce it:

> `nb_o` is the OR of `cell_state[cj][ci] & kNoBakeBit` over the up-to-four
> cells `ci ∈ {vi-1, vi}`, `cj ∈ {vj-1, vj}` that lie inside `[0,31]×[0,31]`.

Its executed statement is `reference/src/zterrain/terrain_core.cpp:168-176`,
inside `bake_dig`. **Note the hazard and do not repeat it:** the only
standalone statement of that reduction today is a TEST helper,
`tests/terrain/bake_dev.hpp:nobake_shadow()`. Building RTL against a test
helper is how a second implementation of a ratified law is born. The reduction
must be lifted into `zref::terrain` as a named function and both the helper
and this block must call it.

---

## 4. Addressing, and the alignment argument written out rather than assumed

`page_base = REGION_BASE + slot_scaled(slot)`, with `slot_scaled()` the
shift-add over the set bits of `PAGE_BYTES` (21,376 = 2^14+2^12+2^9+2^8+2^7 —
five shifts, four adders, **no DSP**) that already appears verbatim in
`zhao_terrain_writeback.sv:323-334`, `zhao_terrain_pagestream.sv:277-287`,
`zhao_terrain_pageloader.sv:228-235` and `zhao_terrain_hdrread.sv:274-281`.
A fifth spelling of the same thing is a fifth thing that can be subtly
different: **copy it, do not re-derive it.**

**Layer D, `D_OFF = 6598`, 1,024 B of u8.** The element is ONE BYTE, so no
sample can cross a 64-bit word or a 64-byte burst — the straddling problem
that forced `zhao_terrain_writeback` to re-lane layer F (`10694 mod 8 = 6`)
**does not arise here at all**, for any alignment. What remains is only which
bursts to fetch: `6598 mod 64 = 6`, so the plane spans bursts 103 through 119
inclusive, **17 bursts** covering `[6592, 7680)`, of which bytes `[6598, 7622)`
are the plane. Check it at elaboration rather than trusting it.

**Layer B, `B_OFF = 2242`, 2,178 B of height16.** `2242` is EVEN and the
element is 2 bytes, so sample *k* sits at even byte `B_OFF + 2k`; a 16-bit
value at an even byte lies wholly inside one 64-bit word (lane 0, 2, 4 or 6,
and 6+2 = 8 fits exactly) and wholly inside one 64-byte staging buffer
(62+2 = 64 fits exactly). This is `zhao_terrain_pagestream.sv`'s own argument
and it holds unchanged for the write direction. `2242 mod 64 = 2`, so the
plane spans bursts 35 through 69 inclusive, **35 bursts**.

**A partial burst is a READ-MODIFY-WRITE.** Both planes begin and end
mid-burst, so the first and last burst of each carry bytes belonging to
neighbouring layers — layer A before B, layer C before D, layer E after D.
Writing a whole burst from a zero-filled buffer would **destroy layer A's last
2 bytes, layer C's last 6 bytes and layer E's first 58 bytes**. The edge
bursts must be read first and merged, or the write must be byte-enabled
(`guard_req_o.be`). Byte enables are preferable and the guard request type
already carries `.be`; whichever is taken, it needs a directed test that
asserts the neighbouring layers are byte-identical after a bake, because this
is a silent corruption of layers nothing in the machine reads back yet.

---

## 5. Memory price — BOTH sizes, per ruling R59's habit

R59's rule is general and is applied here rather than waited for: *show the
same law in a smaller form and take the smaller one if it is bit-identical;
report both sizes.* R87 then priced M10K explicitly — at 306 of 553 already
used it is **no longer the free currency** — so this is not a formality.

| form | buffers | bytes | bits | M10K (10,240 b) |
|---|---|---:|---:|---:|
| **(a) full page window** — also buffers A/B/C in, so bake pulls from here and the cursor adapter and the third `psmux` client both disappear | A,B,C in (6,534) + B out (2,178) + D (1,024) | 9,736 | 77,888 | ~8 |
| **(b) write side + layer D only** — `pagestream` keeps serving A/B/C through a cursor-match adapter and a third `zhao_terrain_psmux` client | B out (2,178) + D (1,024) | 3,202 | 25,616 | ~3 |

The two are **bit-identical in law**: neither changes a value, only where it is
held. Form (b) is smaller in memory by ~5 M10K; form (a) is smaller in
machinery by one adapter and one arbiter client, and removes a class of defect
outright — a PUSH stream feeding a PULL consumer is precisely the cursor-match
seam, and `zhao_terrain_psmux`'s own composition already had to fix one
identity bug of exactly that family (`zhao_console_core.sv:15011-15020`:
reading `tis_*` live would *"stream page N's slot under page M's identity, with
every handshake and every counter agreeing"*).

**Recommendation: (b), on R59's own rule**, with the ~5 M10K saving and the
adapter's cost both stated so the choice is visible and reversible in one
parameter. Do not bury it in a datapath.

**A further halving is available inside (b) and should be measured, not
assumed:** layer D's in and out buffers can be ONE, because the breach phase
reads cell `(ci,cj)` and writes cell `(ci,cj)` in the same z-then-x order,
strictly read-before-write, and §3.4's four-corner conjunction reads the
*meets* bits from bake's own private RAM, never a neighbouring cell's state.
So no cell is re-read after it is written. **That is an argument, and it needs
a directed test that asserts it** — an aliasing bug here changes only breach
decisions, which is the exact fault class `tests/mutants/
zhao_terrain_bake_v2_mutant.sv` exists for, and the exact fault class no
counter can see.

---

## 6. Composition

One new requester on the existing write-capable share. `u_build_share` is
`zhao_mem_share_wr #(.N(3), .CLIENT_ID(6), .RQ(4))` at
`zhao_console_core.sv:10423`, with requesters 0 = MEM.UPLOAD (writes),
1 = TERRAIN.PAGELOADER (writes), 2 = `u_terrain_rdshare` (reads). It already
handles N writers with a retirement ledger, so this is **`.N(3)` → `.N(4)`**,
a widening of a block that has a contract and a test — not a composer's mux.

MEM.GUARD already admits it: `zhao_mem_guard.sv:265-275, 340` gates the
terrain page pool on `ZHAO_CLIENT_TERRAIN_BUILD`, with `terrain_ok` requiring
`req.write` and `terrain_rd_ok` requiring `!req.write`. **No guard change, no
new client id, no new region.** That is the whole reason this block is cheap.

Copy `zhao_terrain_writeback.sv`'s **two-cycle guard verdict law** exactly
(`S_?REQ` → `S_?VERD`, `:100-109`): wait on `guard_rsp_i.ready`, a LEVEL, in
one state and test `.ok`/`.violation` in the next. Testing them in one arm
reads every pass as a denial, silently.

---

## 7. OWNER DECISIONS this surfaces — do not decide these inside a packet

**1. Is layer B persisted through the HPS journal, or is a direct pool write
the whole of it?** Layer F's writeback does NOT simply write the pool: it
streams the sheet to an **HPS journal** under the R14 doorbell contract, with
tickets and acknowledgements (`TERRAIN.WRITEBACK.DOORBELL.md`). §2 calls layer
B *"persistent deformation"* in the same words it uses for the surface sheet.
If "persistent" means "survives the session", a pool write is a CACHE write
and layer B needs the journal too — which is a second doorbell, a second
ticket space, and materially more than this contract describes. If it means
"persists for as long as the page is resident", the pool write is complete and
the page's own dirty bit (`dm_bd_o`) carries it to whatever evicts it.
*Recommendation: the second reading, because `terr_dm_bd_i` — a per-layer
dirty bit for B and D — exists on the directory already and would be
meaningless under the first. But it is the owner's sentence to write, and it
changes the block's size by more than a factor of two.*

**2. Does a bake bump the page's GENERATION?** `dm_gen_o` must carry
something. If a bake produces a new generation, every handle held across it is
stale and `terr_chk_stale_o` starts firing on live handles; if it does not,
two different page contents share one generation and the generation stops
identifying content. *Recommendation: it does NOT bump — the slot still holds
the same patch, and `terr_dm_*`'s own per-layer dirty bits are the mechanism
for "this content moved". But it is unwritten, and the two readings are not
cosmetically different.*

**3. What sets `dm_f_o` and `dm_mips_o`?** A bake dirties B and D. It does not
touch the F sheet. Whether it dirties the MIPS depends on
`zhao_terrain_pagestream.sv`'s **still-open ruling about which surface MIPGEN
sees**: if the mips are built from layer A alone they are untouched by a bake;
if from `compose_top`, every bake invalidates them. *These are the same
question and should be answered once.*

**4. NOT owned here, and flagged only so it is not lost: the layer-E reader.**
Offset 7,622, zero hits under `fpga/`, blocking entry I21 under ruling R13.
Same shape, same page, different consumer, different packet.

---

## 8. What this does NOT close

Honest accounting, because the attraction of this block is that it looks like
it closes three entries and it does not.

* **I32 does not close on this block alone.** Bake also needs its A/B/C reads
  adapted from `pagestream`'s push to its pull (form (b)) or served here (form
  (a)), plus the `stamp_results` → `cmd_*` record seam that entry I32 is
  actually about. This removes the *page* obstacle; the *record* obstacle is
  separate and is ruling R15's depth-table work.
* **I27 does not close.** This gives `terr_dm_*` a writer — the first half.
  The `terr_chk_*` half is unchanged: its first honest caller is
  lodfeed-with-the-devstore, which ruling R87 prices at **185 M10K of 553
  (33%)**, not R59's stated ~77 (14%).
* **I28 is already CLOSED** (rulings R14 and R4, 2026-09-19) and is not
  waiting on this. What is true is that its path has never seen a beat,
  because nothing can dirty a page. This block is what would let it.
* **The register does not move** until bake itself composes, which needs all
  of the above plus a fit.
