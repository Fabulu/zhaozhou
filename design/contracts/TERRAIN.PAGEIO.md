# Contract — TERRAIN.PAGEIO (bake's page window: layer D in, layers B and D out)

> **STATUS 2026-09-21: BUILT.** `fpga/rtl/terrain/zhao_terrain_pageio.sv`,
> ledger id `TERRAIN.PAGEIO`, `tests/terrain/pageio_rtl_directed.cpp` (79
> checks), fit gate `F-PAGEIO1`, inventory `pending_compose`, manifest
> `not-yet-adopted`. NOT COMPOSED: see §7bis decision 5 and §8.
>
> ~~Ledger: **no `design/blocks.yml` row yet, deliberately.**~~ *Written
> 2026-09-20 by the terrain8 packet. The reasoning was that an unbuilt row is
> itself a gap, so the row should wait for the RTL. **Owner ruling R210 found
> the opposite**: `tools/budget/completion_register.py` walks
> `design/blocks.yml`, so a capability with NO ROW is not absent from the
> console — it is absent from the QUESTION. The register's total was never
> wrong; it was answering a smaller question than its readers assumed. The row
> landed FIRST, before the RTL, and the total ROSE from 21 to 22 when it did.
> That rise is the instrument starting to work.*
>
> *Keep the shape, not the rule: a gap that no instrument can see is worse than
> a gap that every run prints.*
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
(`guard_req_o.be`). ~~Byte enables are preferable and the guard request type
already carries `.be`~~; whichever is taken, it needs a directed test that
asserts the neighbouring layers are byte-identical after a bake, because this
is a silent corruption of layers nothing in the machine reads back yet.

> **CORRECTED 2026-09-21 by the build. BYTE ENABLES ARE STRUCTURALLY
> UNAVAILABLE**, so "whichever is taken" is one option, not two. The request
> type does carry `.be`, and the guard **refuses a sparse one**:
>
> ```systemverilog
> zhao_mem_guard.sv:  be_ok    = (req.be == mask_of(req.len));   // FULL mask
>                     shape_ok = len_ok && be_ok;
> ```
>
> Its own header says byte_enable *"must be the FULL contiguous mask over
> [addr, addr+len) (Phase-2 clients issue whole spans; partial-word masking is
> **NOT in the Phase-2 arbiter**)"*, and `zhao_vram_arbiter.sv` confirms it
> from the other side — it converts `len` to **words** (`words_of()`) and the
> SDRAM controller never sees a byte mask at all. A sparse `be` is a guard
> **violation**, not an optimisation. Every existing client of this guard uses
> exactly one shape: 64-byte aligned address, `len` 64, `be` all ones.
>
> So it is read-modify-write, and **it is smaller than this section feared**.
> Not the edge BURSTS in general: layer D's seventeen bursts are read anyway to
> serve `cell_state_i`, so both of its edge bursts are already held verbatim
> and cost nothing. Only **layer B's two** (bursts 35 and 69) are read for
> their foreign bytes, which is **two extra reads per bake** — 19 reads, 52
> writes.
>
> The directed test this paragraph asks for exists and is the block's headline:
> `pageio_rtl_directed` reads the page image back and asserts layers A, C and E
> and the header are byte-identical. **The detector was fired**, by a mutant
> that skips the two layer-B edge reads: it reported exactly **2** bytes of
> layer A and **60** of layer C — the counts this section's own arithmetic
> predicts — with layer C's 6-byte tail correctly surviving because layer D's
> edge bursts are read. The arithmetic and the detector corroborate each other
> rather than agreeing by construction.

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

> **BUILT 2026-09-21 by the PAGEIO packet.** `fpga/rtl/terrain/zhao_terrain_pageio.sv`,
> with `design/blocks.yml` id `TERRAIN.PAGEIO`, `tests/terrain/pageio_rtl_directed.cpp`
> (79 checks) and fit gate `F-PAGEIO1`. This section is amended BELOW rather
> than rewritten: decision 1 turns out to be already ruled, decision 3's
> settled half is taken and its open half is not, and a FOURTH decision the
> contract did not name is stated and refused. See **§7bis**.

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

## 7bis. What the build found out about §7 — 2026-09-21

### Decision 1 is NOT an owner decision. Ruling T4 already answered it.

The question above — journal or pool write — is settled, and the sentence was
in the tree the whole time. `zhao_terrain_writeback.sv`'s header states T4 as:

> B and D are **NEVER** written back (the HPS keeps the canonical mirror
> current from the same deterministic commands); layer F has no canonical
> mirror, so it MUST be, and *"wait for journal acknowledgement before the slot
> may enter LOADING"*.

That is the whole answer and it is given as the *reason layer F needs the
doorbell*. The pool write is the whole of it: no second doorbell, no second
ticket space. §7's own recommended reading is the ruled one, and its stated
worry — that "persistent" might mean the journal — is answered by T4 naming the
HPS's deterministic replay as the canonical mirror for exactly these two layers.

**The shape is worth keeping.** The contract asked the owner for a ruling that
existed, in a header its own §4 cites for a different reason. Before escalating
a decision, grep for the ruling.

### Decision 2 is TAKEN, as §7 recommended, and the test asserts it.

A bake does **not** bump the generation: `dm_gen_o <= job_gen_q`. The slot still
holds the same patch and `terr_dm_*`'s per-layer dirty bits are the mechanism
for "this content moved". Bumping would make every handle held across a bake
stale and start `terr_chk_stale_o` firing on live handles.
`pageio_rtl_directed` asserts the echo, so the alternative reading cannot be
adopted silently — it would turn a test red.

### Decision 3: the settled half is taken, the open half is NOT.

`dm_f_o` is **low**: a bake dirties B and D and does not touch the F sheet.
That half is not in doubt and is not an owner decision.

`dm_mips_o` is **high**, and this is a *safe default under an open ruling*, not
an answer. Whether a bake invalidates the mips depends on
`zhao_terrain_pagestream.sv`'s still-open ruling about which surface MIPGEN
sees: mips built from layer A alone are untouched by a bake, mips built from
`compose_top` are invalidated by every one. High is the reading under which a
stale mip can never be shown; the cost of being wrong is regenerating mips that
did not need it. Low would be the reading under which a wrong one can. **They
are the same question and should be answered once, for both blocks.** The
constant is one line beside a comment that says so.

### Decision 5 — NEW, not in §7, and REFUSED here: who serves bake's layer-F read

`zhao_terrain_bake_v2` gained `sheet_texel_o` / `sheet_strength_i` under ruling
R194, and its own port comment says *"WHO SERVES IT is NOT this block and is
not settled"*, naming a scheduler. **This packet did not build it, and the
reason is that it is not a scheduler-shaped problem.** Measured against the
actual port rather than the description:

* `zhao_surface_sheet`'s `req_*` is a **control-and-read port**, not a memory
  read: `req_op_i` carries `OP_ACQUIRE` / `OP_READ` / `OP_RELEASE`, it takes a
  32-bit residency **handle**, and the answer comes back on a separate response
  stream `pg_*` with a **status** — `ST_HIT`, `ST_ALLOCATED`, `ST_OVERFLOW`,
  `ST_MISS`.
* Bake wants a **combinational lookup**: `sheet_texel_o` is combinational on
  the dig cursor and `sheet_strength_i` is sampled with `vtx_valid_i`, on the
  same beat as base/scar/bottom/nobake.
* The core's composition annotates the port `// REAL: SURFACE.STAMP is the only
  requester`, and `zhao_console_core.sv` says of it *"one `req_*` channel with
  no arbitration"*.

So four things are missing, and only the third is an arbiter:

1. **A handle, and its lifetime.** Who issues `OP_ACQUIRE` for the patch's
   sheet page, and who issues `OP_RELEASE`? A bake that acquires and never
   releases leaks one of `Slots` (default **2**) pages.
2. **A latency adapter.** A ready/valid request with a separate response stream
   cannot answer on the beat bake samples. Either bake's dig stalls per vertex
   — 1,089 round trips per record — or something prefetches the 64×64 sheet,
   which is 8,192 bytes and a second copy of layer F on chip.
3. **The arbiter proper.** SURFACE.STAMP writes the same sheet in the same
   frame. Its policy between a live stamp and a bake read is a **decision**: a
   bake that reads a half-applied stamp digs a shape the player did not make,
   and a stamp that waits for a bake drops frames.
4. **A law for `ST_MISS`**, which is the one that is not an engineering
   question at all. If the sheet page is not resident when a `cmd_depth_sheet_i`
   record digs, the vertex gets *something*, and the options are not
   cosmetically different: **fail the record** (the deformation does not
   happen, and the player's action is silently lost), **dig zero** (the
   deformation happens with no depth, which is a visible no-op), or **fall back
   to the parametric disc** (§9.3's other law, which is a different shape).

**Recommendation, and it is only that:** decision 4 first, because it is the
one with a player-visible consequence, and the other three are cheap once it is
written. Do not take 1–3 without it — an arrangement built around a miss policy
nobody chose will have chosen one.

---

> ### ANSWERED AND BUILT, 2026-09-21 (sheetseam). Owner ruling **R221**.
>
> **The recommendation was taken as written.** R221 ruled decision 4 first:
> *"fall back to the parametric disc, and COUNT the fallback"*, on the ground
> that the disc is **the ratified v1 law** and not an invention — SEAMDIG
> measured the sheet mode additive, `terrain_bake_v2_directed` passing 267/267
> unchanged — while the other two options *"make an absence look like a
> result"*. Decisions 1–3 are then `design/contracts/TERRAIN.SHEETSEAM.md`,
> `fpga/rtl/terrain/zhao_terrain_sheetseam.sv` and
> `fpga/rtl/surface/zhao_surface_sheetshare.sv`, with 81 checks in
> `tests/terrain/sheetseam_rtl_directed.cpp`.
>
> **Three of the four items above came out different from this section's own
> framing, and the differences are recorded here rather than only in the new
> contract, because this is the page the next reader opens:**
>
> * **Item 1 has no answer because it has no question.** The bake reader
>   issues `OP_READ` and **nothing else** — never `OP_ACQUIRE`, never
>   `OP_RELEASE` — so it cannot leak a slot, because it never holds one. This
>   is forced rather than frugal: `do_acquire_new` in `zhao_surface_sheet.sv`
>   sets `dir_live`, runs the 4,096-cycle clear sweep and answers
>   `ST_ALLOCATED`, i.e. it **allocates a blank sheet**. Serving a bake from
>   that is R221's explicitly refused *"dig zero"* with a status code that says
>   HIT, and it costs one of `Slots = 2` taken from the only block
>   `terrain_rules` §7 allows to write layer F. `OP_READ` is the only opcode
>   that reports residency without changing it.
>
> * **Item 2's second figure is 7.5× too large.** *"the 64×64 sheet, which is
>   8,192 bytes and a second copy of layer F on chip"* — bake has **no tag
>   port**, so half of that is a plane this consumer cannot see; and §9.3(b)'s
>   address law `ti = (vi >= 32) ? 63 : 2*vi` means the 33×33 lattice can
>   address only **1,089 of the 4,096 texels**. The prefetch is **1,089 bytes,
>   one M10K**, not 65,536 bits and seven. It is not a second copy of layer F;
>   it is layer F resampled onto the lattice. Measured at **1,091 cycles per
>   record**, uncontended.
>
> * **Item 3's stated hazard is not an arbiter's to solve.** *"a bake that
>   reads a half-applied stamp digs a shape the player did not make"* is a
>   FRAME-ORDER question — bake runs in §9.2's bake window, after the stamp
>   pass — and no arbiter priority changes it. What the arbiter must actually
>   do is not starve either client, so the policy is `zhao_terrain_psmux`'s
>   round robin, adopted rather than re-decided. Measured: the stamp waited
>   **0 cycles** and the prefetch went 1,091 → 1,092.
>
> **What does NOT change:** §7bis's closing sentence and §8 below are both
> still exactly right. I32 does not close, because `cmd_*` still has no
> producer.

**This is why entry I32 does not close on this block.** The *page* obstacle is
removed; the *record* obstacle (`cmd_*` has no producer — `zhao_terrain_cmd`
emits a patch-directory record, not a bake record) and this *sheet* obstacle
are separate, and one of them is an owner's sentence to write.

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
