# The command → terrain pipeline — what is left of D4

*2026-09-07. Written against the tree, not against a summary. Every claim below
names the file and line it was read from, and the two places where a number
could not be established from the tree say so instead of guessing.*

---

## Where the 8 km world actually stands

`reports/DOCKET.md` D4 lists four things missing from the 8 km world:

> the world pager, patch-residency manager, composed-height cache, and
> **command→terrain pipeline**.

Three of the four are now built and unit-verified, and the composed suite
exercises them together:

| D4 item | block | state |
|---|---|---|
| world pager | `TERRAIN.SEQ` + `TERRAIN.LOADQ` + `TERRAIN.PAGELOADER` | UNIT_VERIFIED |
| patch-residency manager | `TERRAIN.RESIDENCY` (v2) | UNIT_VERIFIED |
| composed-height cache | `TERRAIN.COMPCACHE` | UNIT_VERIFIED |
| **command→terrain pipeline** | **nothing** | **this document** |

`world_composed_directed` runs the first three in one Verilator top and, as of
today, gets a loaded page all the way to RESIDENT: 158 checks, 0 failures, one
composition defect left, and that one needs an owner ruling rather than code.

**But nothing in the tree turns a command into a frame.** The bench plays the
frame ring by hand — `fr_start`, `fr_epoch`, `fr_patch_count`, `fr_sequence`,
and a `rec_*` stream it constructs in C++. In the console there is no command a
game can issue that makes any of it happen.

---

## The ABI is already ruled, to the byte

`reports/OWNER-RULINGS-BUILDABILITY-20260902.md` T5 fixes it and says
*"opcodes confirmed by the ZIDL generator before commit"*:

```
TerrainEpoch @ 0x0220, size 16
  epoch:u32; op:u8 (BEGIN=0, END_FLUSH=1, ABORT=2); flags:u8;
  reserved:u16; island_table_handle:u32; source_id:u32

SubmitTerrainSet @ 0x0230, size 32
  resource_epoch:u32; list_offset:u32; list_bytes:u32; list_crc32c:u32;
  patch_count:u16; view_mask:u8; flags:u8; sequence:u32; reserved0/1:u32

Patch-list record, 32 B
  island_id:u32; patch_ix:i16; patch_iz:i16; hps_page_addr:u64;
  expected_page_crc32c:u32;
  flags:u16 (REQUIRED, PREFETCH, DYNAMIC, DUAL, HAS_SAVED_F);
  view_mask:u8; priority:u8; source_id:u32; reserved:u32
```

**Neither command is in `spec/commands.zidl`.** That file's terrain block
(`spec/commands.zidl:330-369`) declares `TerrainField 0x0200` and
`SurfaceStamp 0x0210` and stops. `0x0220` and `0x0230` are unallocated, which is
the good news: the ruling's opcodes are free and the ZIDL comment at line 20
already reserves `0x0200-0x02FF` for terrain.

The 32-byte patch-list record is the one piece that IS built: it is
`zref::swstream::PatchRecord`, sorted by `canonical_less` and serialised by
`encode_record`, and `TERRAIN.SEQ` consumes exactly those fields. So the record
does not need designing — only fetching.

---

## THE GAP THAT IS NOT OBVIOUS: the shell framer carries 16 payload bytes

This is the finding that changes the shape of the work, and it was measured off
the RTL rather than assumed.

`zhao_cmd_decoder` does not carry payload at all. Its record port is
`{rec_opcode_o, rec_bytes_o, rec_source_id_o, rec_index_o}`
(`fpga/rtl/command/zhao_cmd_decoder.sv:76-81`) — opcode and bookkeeping, no
body. The body is picked up by the shell's own record framer, and that framer
captures **record bytes 16 through 31 only**:

```systemverilog
// fpga/rtl/common/zhao_shell_top.sv:1712-1716
if ((f_rpos >= 16'd16) && (f_rpos < 16'd32))
  w_final[8*(f_rpos - 16'd16) +: 8] = pkt_byte;
```

and hands the scheduler four words:

```systemverilog
// fpga/rtl/common/zhao_shell_top.sv:1720-1724
assign rec_opcode = recq[...][143:128];
assign rec_w0     = recq[...][31:0];
...
assign rec_w3     = recq[...][127:96];
```

**Sixteen bytes. `SubmitTerrainSet` is thirty-two.** Line the ruling's field
order up against what survives:

| bytes | fields | reaches the scheduler? |
|---:|---|---|
| 0–15 | `resource_epoch`, `list_offset`, `list_bytes`, `list_crc32c` | **yes** |
| 16–31 | `patch_count`, `view_mask`, `flags`, `sequence`, `reserved0/1` | **no** |

The half that survives is the half that says *where the list is*. The half that
is lost is the half that says *how long it is, which views it is for, and which
sequence it belongs to* — and `patch_count` is the number ruling T5 calls the
**seal**, the one `TERRAIN.SEQ` stops at and the one the composed suite's phase I
exists to check nobody walks past.

So a terrain command block cannot simply sit behind the existing framer. Either
the framer widens, or the block re-reads the command body from memory, and that
is a decision with consequences on the shell's timing — the comment at
`zhao_shell_top.sv:1680-1683` records that the `f_pos -> recq[*]` family was
**already the largest group of failing setup endpoints on `gpu_clk`** in the
2026-08-24 composed shell fit, at about −0.765 ns. Widening a 144-bit array to
272 bits in front of that is not a free edit, and it is not one to make on the
strength of this paragraph.

### It is not a terrain problem, and that was checked rather than assumed

The paragraph above was first written ending *"`TerrainField 0x0200` carries
`u8 parameters[64]`, which suggests this is a pre-existing hole — but that is a
hypothesis, and the next pass should measure it before believing it."* It was
measured, immediately, and it holds:

```
fpga/rtl/generated/zhao_abi_pkg.sv:438
  // TerrainField 0x0200: 112-B record (implemented).
```

**112 bytes**, of which the framer carries sixteen. So the 16-byte payload window
truncates a command that is already in the ABI and already marked `implemented`,
and `SubmitTerrainSet` would be the second victim rather than the first. That
changes who owns the fix: it is a **command-path** decision, not a terrain one,
and the terrain block should not be the place it gets worked around.

**Still not established from the tree:** whether `rec_bytes_o` on the decoder
reaches the framer in time to distinguish a short command from a long one, and
how `TerrainField`'s consumer gets its other 96 bytes today — it is executed by
the software console rather than by RTL (`spec/commands.zidl:334`), so it may
never have needed the framer at all, which would explain how the limit survived
this long unremarked.

---

## What TERRAIN.CMD has to do

Given a `SubmitTerrainSet`, in order:

1. **Take the epoch.** `resource_epoch` must equal the live `cfg_epoch_i` or the
   command is refused — the same check `TERRAIN.PAGESTREAM` and
   `TERRAIN.PAGELOADER` make, for the same reason.
2. **Read the sealed list.** `list_offset` / `list_bytes` name a run of 32-byte
   records in the HPS staging arena. `list_bytes` must be `32 × patch_count`
   exactly; anything else is a malformed command, not a clamp.
3. **Verify `list_crc32c` over the whole list before acting on any of it.** T5
   calls the list *capture data*, and the determinism ledger anchors replay on
   it. A list that is acted on and then found corrupt has already issued loads.
4. **Start the frame**: `fr_start`, `fr_epoch`, `fr_patch_count`, `fr_sequence`
   into `TERRAIN.SEQ`.
5. **Stream the records** onto SEQ's `rec_*` port in list order, unpacking the
   32-byte record into the ten fields SEQ takes. **List order is the law** —
   `zref::swstream::canonical_less` already sorted it, and re-sorting in hardware
   would be a second ordering to keep in step with the first.
6. **Stop at `patch_count`.** Not at the end of the buffer, not at a sentinel.

And `TerrainEpoch`: BEGIN publishes a new `resource_epoch`, END_FLUSH waits for
the pager to drain, ABORT drops the current set. **What "drain" means in gates —
which counters must be quiet, and for how long — is not ruled anywhere I can
find, and is listed below rather than invented.**

### What it must not do

It does not decide residency, does not load pages, does not compose, does not
re-sort, and does not walk visibility. `SW.STREAM` did the visibility walk on the
HPS and sealed the result; T5 is explicit that **replay does not rerun it**. A
hardware block that recomputed any part of that list would be a second source of
truth for the thing the determinism ledger is anchored on.

---

## Reads, and the arm that does not exist yet

The list lives in the HPS staging arena, so this block is a `MEM.HPS.BRIDGE`
read client — the same shape `TERRAIN.PAGELOADER` uses.

`MEM.GUARD` gained `TERRAIN.PAGE_POOL` write access for
`ZHAO_CLIENT_TERRAIN_BUILD = 6` (ruling T3, 2026-09-06) and a read arm over the
same window the same day. **Whether the patch LIST is inside any window this
client may read is a separate question**, because the list is in the HPS arena
and not in the page pool. `TERRAIN.PAGELOADER` reads the arena through the
bridge rather than through the guard, so the precedent exists — but it should be
read out of `zhao_hps_arbiter.sv` and the bridge contract before this block is
written, not assumed from the shape of a sibling.

---

## The staged build, smallest first

**Stage 0 — the ABI, and nothing else.** Add `TerrainEpoch 0x0220` and
`SubmitTerrainSet 0x0230` to `spec/commands.zidl` exactly as T5 gives them, run
the ZIDL generator, and let the generated `zhao_abi_pkg.sv` and the C++ ABI
header confirm the opcodes and sizes. This is a spec edit and a regeneration; no
RTL. It is also the step T5 explicitly asks for — *"opcodes confirmed by the ZIDL
generator before commit"* — and it is worth doing on its own, because it is the
only step here that cannot be got wrong quietly.

**Stage 1 — the reader, with a played bridge.** A block that takes
`{epoch, list_addr, list_bytes, patch_count, sequence}` on a simple job port,
fetches the list, CRC-checks it, and emits the ten-field record stream. Its bench
plays the bridge and its oracle is `zref::swstream::encode_record` read
backwards: given bytes the encoder produced, the block must emit the fields the
encoder was given. That is a differential with a real oracle and no new law.

**Stage 2 — the frame ring.** `fr_start` / `fr_epoch` / `fr_patch_count` /
`fr_sequence` into `TERRAIN.SEQ`, and the composed suite stops playing the ring
by hand. This is where phase I's *"eight records offered, five declared"* check
becomes a statement about the machine rather than about the bench.

**Stage 3 — the command seam.** Whatever the answer to the 16-vs-32-byte
question is. Deliberately last, because it is the one that touches the shell's
worst timing group and the one that most wants a measurement first.

**Stage 4 — `TerrainEpoch`.** After the ruling below.

Each stage is fittable on its own. None of them is an island run.

---

## OWNER RULINGS NEEDED — listed, not invented

1. **The command payload seam.** Does the shell's record framer widen to carry
   32 payload bytes, or does the terrain command block re-read its own body from
   memory? The framer is already the worst setup group on `gpu_clk`; the
   re-read costs a round trip and a second place the command's bytes exist.

2. **What `TerrainEpoch END_FLUSH` waits for, in gates.** "Drain" is not defined
   anywhere I can find. Which counters must be quiet — loads outstanding,
   writebacks unacknowledged, mips pending — and does a fault during the flush
   abort the epoch or hold it?

3. **What a bad `list_crc32c` does to the frame.** Refuse the command and carry
   on with the previous set, or fault the frame under T6's rule? The two differ
   in whether the player sees a stale frame or a stutter.

4. **Whether `TERRAIN.CMD` may read the HPS arena directly**, or must go through
   an arm that does not exist yet. See above — the precedent is
   `TERRAIN.PAGELOADER`, but a precedent is not a ruling.

5. *(carried, from `TERRAIN.MIPFEED`)* **Which plane is surface 0 for load-time
   mips** — layer A, or `compose_top`. Unchanged by anything here, and still the
   one-mux decision it was this morning.

---

## What this document does not claim

It does not claim the 16-byte framer limit is the only reason a terrain command
cannot be issued today; it claims it is the first one that shows up when you read
the path from the decoder to the scheduler. It does not claim the HPS read arm is
missing; it claims nobody has checked. And it puts no number on Stage 3's timing
cost, because the only honest number would come from a fit and no fit has been
run on a widened framer.
