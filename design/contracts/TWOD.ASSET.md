# Contract — TWOD.ASSET (the compositor's page and palette loader)

> Ledger: `design/blocks.yml` · owner ZH-071 · phase 11 · maturity BUILT

## Purpose and exclusions

Reads a **TWOD_PAGE** resource out of the HPS arena and writes it into
`TWOD.SAMPLER`'s on-chip page or palette store, under the CRC and epoch laws
`PublishResource` already carries.

**Ratified by the owner's completion ruling of 2026-09-22, item 3**, in its
asset clause:

> *"This includes any required legitimate asset/palette loading producer. **An
> opcode plus descriptors referring to data that only the testbench can inject
> is not completion.** Reuse `PublishResource` and the existing validated
> resource mechanisms wherever applicable."*

**It is not a texture cache, a TMU, a VRAM client or a second uploader.** It
moves bytes to one destination and judges four things about the request.

## Why it reads the ARENA and not VRAM

`zhao_console_core.sv` entry I17: *"there is no **texel page store** in the tree
that a (u, v) can walk into without a VRAM fill agent … That is a real wall and
it is why the new block carries a page store of its own rather than a client."*
`TWOD.SAMPLER` owns its texels **because nothing would fill them from SDRAM**.

So the loader reads the bytes where `PublishResource` already points —
`hps_addr_lo/hi`, the staged bytes in the HPS arena — over the same
`zhao_hps_arbiter_n` socket `MEM.UPLOAD`, `TERRAIN.PAGELOADER`, `PART.STATE`,
`GEOM.LOOM` and `FIELD` share, as **client 6, read-only**.

The alternative, a VRAM read client, needs an arbiter index and a window
`spec/memory_rules.md` does not give the compositor, and would move the same
sixteen kilobytes twice.

## Clock and reset semantics
Single `gpu_clk`, synchronous active-low `rst_n`. Reset abandons a transfer in
flight; the destination region then holds whatever had already landed, which is
the same state a power-up leaves and is why a descriptor must not name a region
no committed load has filled.

## Input and output packet layouts

### In: the upload request, forked by `kind`
`j_*` is field for field the group `upl_*` carries to `MEM.UPLOAD` for every
other kind. **`CMD.EXEC` forks at the pending queue's DRAIN** — one head, two
destinations, one pop — so `PublishResource`'s staging, capacity, overflow,
atomicity and field offsets are untouched.

The fork key is `RESOURCE_KIND_TWOD_PAGE = 15`, a **generated ABI constant**
(`spec/commands.zidl`), because the packer, the fork and the reference model all
have to agree about it and a number written out three times is the
`QFMT_VERSION` 2/3 skew waiting to happen again.

### Out: the sampler's asset write port
```
  ld_page_we_o / ld_page_addr_o / ld_page_data_o   texels
  ld_pal_we_o  / ld_pal_addr_o  / ld_pal_data_o    palette entries
```
`ld_bind_*` is **not** this block's: which page region a descriptor samples is
part of the descriptor, and `TWOD.CMD` writes it.

## The destination map

`spec/cartridge.md` §4f. `PublishResource.dst_slot` selects:

| `dst_slot` | destination | words |
|---|---|---|
| 0..7 | page-store slot *s*, first word `s * (PAGE_WORDS/8)` | `PAGE_WORDS/8` |
| 16..19 | palette slot 0..3 | 256 |
| anything else | **refused and counted** | — |

At the shipped `PAGE_WORDS = 8192` a page slot is 1,024 words (2 KiB) and a
palette slot is 256 RGB565 entries. A body is a raw run of **little-endian
`u16`** — no header, no magic, no dimensions, because **the descriptor already
carries them** and a header repeating them would be a second opinion about the
same facts.

## Every other law is the existing one

* `length` a multiple of 64 — a refusal, never a pad;
* `crc32c` over the staged bytes, `zhao_crc32c_fold`'s law, init all ones and a
  final complement, exactly as `TERRAIN.PAGELOADER` folds it;
* `hps_addr` above 4 GiB **refused rather than narrowed** — `MEM.UPLOAD`'s
  `kUploadSourceUnreachable`, at the same width and for the same reason;
* the base 64-byte aligned, because the bridge's burst is;
* `epoch` checked against the open resource epoch.

## A burst is collected whole and then drained

The bridge delivers one 64-bit beat **per cycle** once granted, and answers
`err` only at the request — a granted read runs to its last beat whatever the
consumer is doing. A store write takes **four cycles per beat** (four 16-bit
words), so draining while the beats land would **drop three beats in four with
every counter still balancing.** The 512-bit landing buffer is the shape
`zhao_geom_loomfeed` uses on the same socket, for the same reason.

## A failed CRC ZEROES what it wrote

The CRC is known only at the last beat, and buffering sixteen kilobytes to hold
the write back would cost more memory than the store it protects. So words are
written as they land and a bad CRC is followed by a **zeroing pass over exactly
the words this transfer wrote** — bounded by the slot, at most 1,024 writes,
once, on a fault.

**That is the fail-safe direction and the two alternatives are worse.** Leaving
the partial page makes a corrupt transfer **look like art**, which is R221's
exact refusal. Leaving the PREVIOUS page is worse still: the frame draws last
level's HUD while a counter quietly says the new one failed. A zeroed region
samples as colour 0, which is visibly nothing, and `crc_fails_o` /
`regions_zeroed_o` say why.

## Backpressure rules
`j_ready_o` is high only when idle. The handshake **waits** rather than queues:
`CMD.EXEC`'s upload arm already owns a pending queue, and a second one here
would be two places a request can be waiting with no single answer to *where is
it*.

## Memory ownership
Reads the HPS arena through `zhao_hps_arbiter_n` as a read-only client. Writes
**only** `TWOD.SAMPLER`'s page and palette arrays. Owns a 512-bit landing buffer
and nothing else. **No VRAM client, no cache, no texel storage of its own.**

## Q formats and rounding
**None.** A texel word and a palette entry are RGB565 in and RGB565 out; this
block performs zero colour arithmetic, which is the same sentence
`TWOD.SAMPLER` makes about itself.

## Latency (fixed or variable)
Variable, dominated by the bridge. Per 64-byte burst: request, grant, 8 beats,
32 drain cycles. A full 2 KiB page slot is 32 bursts, about **1,300 clocks plus
grant latency** — 1.4 % of a frame.

## Target throughput
One 16-bit word per cycle into the store, one 64-byte burst at a time in
flight. A whole page store refill (16 KiB) is ~10,400 clocks, **11 % of a
frame**, and it is a background client so it yields to every terrain and
particle burst.

## The write is NOT frame-gated, and that is declared here

A load lands whenever the bridge answers. **If a descriptor in the frame that is
drawing samples the region being written, it sees a mix of the old and new
words for that frame.** The store is a simple dual-port memory and no cheap
interlock exists that would not also be able to starve the loader.

`loads_during_pass_o` **measures** it — transfers that overlapped a live TWOD
pass — so the exposure is a number rather than a sentence. Its two operands are
this block's own state and `POST.COMPOSITE`'s `hud_req_v`, which nothing clocks
together. **The safe use is the ordinary one for every asset system ever built:
publish before the frame that draws with it.**

## Overflow and malformed-input behaviour

| condition | behaviour |
|---|---|
| `dst_slot` outside the map | refuse whole, count (`slot_refused_o`) |
| `length` zero, not a multiple of 64, or longer than its slot | refuse whole, count (`len_refused_o`) — **never truncated to fit, never allowed into the neighbouring slot** |
| `hps_addr` above 4 GiB or not 64-byte aligned | refuse whole, count (`addr_refused_o`) |
| `epoch` not the open one | refuse whole, count (`epoch_refused_o`) |
| bridge `err` at the request | counted (`bridge_errs_o`), request taken down and re-offered — a held request the arbiter re-serves would spin with every counter frozen |
| CRC mismatch at the last beat | the written region is **zeroed**, counted twice (`crc_fails_o`, `regions_zeroed_o`) |

**The refusal order is the record's own** — destination, extent, source, epoch
— and each counter names ONE clause, so a refusal reads back to the field that
caused it rather than to *"the loader said no"*.

## Counters and traces
`loads_started_o`, `loads_done_o`, `words_written_o`, `slot_refused_o`,
`len_refused_o`, `addr_refused_o`, `epoch_refused_o`, `crc_fails_o`,
`regions_zeroed_o`, `bridge_errs_o`, `loads_during_pass_o`, `bursts_o`.

## Scalar reference function
`zref::twod::asset_verdict` (ledger `reference_model`). Owns the four refusal
clauses, the destination map and the word order. It does **not** own the CRC —
that is `zhao_crc32c_fold`'s and `zref::mem::upload_verdict`'s, already frozen
and proved.

## Directed tests
`tests/compositor/twod_asset_directed.cpp`.

* a clean 64-byte load into page slot 0, byte for byte, **little-endian word
  order asserted explicitly** — the one place an endianness slip would be
  invisible in a rendered frame and obvious in a texel;
* a multi-burst load spanning four bursts, with the beats delivered at one per
  cycle and the drain four cycles behind — the case a drain-as-you-land design
  would fail while every counter balanced;
* a load into a palette slot;
* **each refusal clause on its own counter**, one at a time;
* a load one word longer than its slot: refused whole, nothing written;
* **a CRC failure**: the region is zeroed, `crc_fails_o` and `regions_zeroed_o`
  both move, and a subsequent sample of that region returns colour 0;
* a bridge `err` at the request: counted, re-offered, and the load completes;
* `loads_during_pass_o` fired by starting a load with `pass_active_i` high, and
  **silent** when it is not.

## Randomized differential tests
**Folded into the directed bench rather than given a file of its own.** Its
`sweep_verdicts` case walks every `dst_slot` 0..255 and a boundary set of
lengths and addresses against `zref::twod::asset_verdict`, which is exhaustive
over the field that matters rather than random over it. **It reports the refusal
mix, and a run that never refuses would be testing half of it.**

## Formal properties
**A formal lane is PLANNED and no file exists yet** — see
`reports/PHANTOM-CITATIONS-AUDIT.md`. Its properties:

* **no write outside the destination region** named by `dst_slot`, for any
  request accepted or refused;
* a refused request performs zero writes and zero bursts;
* handshake hygiene; reset leaves the request port ready.

## Synthesis / resource ceiling
**Unbuilt (no Quartus).** Hand-count, in the unflattering direction:

| part | count | note |
|---|---|---|
| landing buffer | 512 flops | eight 64-bit beats |
| CRC state | 32 flops | plus `zhao_crc32c_fold`'s combinational GF(2) matrix |
| addressing, counters, FSM | ~160 flops | |
| evidence counters | 12 x 32 | 384 flops |

~1,090 flip-flops at 0.849 ALM/register is **~925 ALM**, plus the fold's XOR
tree — which is the only thing here that could surprise, and is what
`design/fit_targets.yml`'s `zhao_twod_asset` leaf row asks. **Ceiling: 700 ALM
beyond the fold, 0 DSP, 0 M10K.**

### THE HAND COUNT BREACHES THAT CEILING BY ~225 ALM, AND IT IS DECLARED RATHER THAN RESOLVED

Written out because the two numbers above are one line apart and a reader who
subtracts them is doing work the contract should have done. **~925 against 700
is a 32% overrun on an estimate, and the estimate was made in the unflattering
direction on purpose**, so the true figure could land either side of it. What
is not honest is leaving a ceiling in place that the block's own hand count
already fails and calling the row complete.

It is stated and not fixed for the reason this repository gives for every such
deferral: **the ceiling is an aspiration and the fit is the thing that knows.**
Moving the ceiling to 950 so the row passes would be changing a number to make
a check succeed, which is the one edit that is never allowed here. Moving the
DESIGN before a fit has measured it would be optimising against an estimate.

**The levers, so whoever runs the fit is not starting from nothing:**

* **The landing buffer is 512 of the 1,090 flops, and it exists for a timing
  reason rather than a functional one.** The bridge delivers one 64-bit beat
  per cycle once granted; a store write takes four cycles per beat. Draining
  while beats land would drop three in four, so the buffer holds a whole
  8-beat burst. A narrower store port, or a second write port, removes most of
  it — and both are changes to TWOD.SAMPLER, not to this block.
* **The twelve evidence counters are 384 flops.** They are not negotiable as a
  group — each one is a refusal clause the ratification requires be
  distinguishable — but a console-wide counter width below 32 bits would take
  a third of them back everywhere at once, which is a budget decision and not
  this contract's to make.

Neither is attempted here. **`design/fit_targets.yml`'s `zhao_twod_asset` leaf
row is where this question gets an answer**, and until it runs, the number
above is an ESTIMATE and is marked as one everywhere it appears.

## Integration capture cases
* **a published glyph sheet drawn as text** — the whole path from
  `PublishResource` to composited pixels, with no texel injected anywhere.
* **a corrupted page** — one byte of the staged bytes flipped; the HUD draws
  black where the glyphs were and the two counters say why.
* **a load during a pass** — the same page published mid-frame; the instrument
  fires and the frame completes.

## Notes

Client 6 of `u_terr_hps_arb` is the **lowest** index, so a burst of terrain page
loads makes a HUD install wait, visibly, in `terr_hps_c6_wait_cycles_o`. It
cannot deadlock by POSITION IN TIME — client 5's argument: a TWOD page is staged
before the frame that draws with it, and no terrain, particle, node or field
client waits on a HUD texel.
