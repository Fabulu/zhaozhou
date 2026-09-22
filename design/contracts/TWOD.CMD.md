# Contract — TWOD.CMD (the frame-scoped descriptor producer)

> Ledger: `design/blocks.yml` · owner ZH-070 · phase 11 · maturity BUILT

## Purpose and exclusions

Turns `SetPlane 0x0306` and `DrawSprite 0x0307` records into the **sealed
per-frame descriptor list** `TWOD.PLANE` and `TWOD.BAND` consume.

**Ratified by the owner's completion ruling of 2026-09-22, item 3**
(`reports/Zhaozhou_proposed_completion_rulings_2026-09-22.txt`, recorded in
`reports/OWNER-RATIFICATION-20260922-COMPLETION.md`):

> *"Descriptors are frame-scoped. Stage and validate the frame's descriptors,
> then publish a **sealed list** at the boundary that owns that frame, before
> its TWOD pass. **Neither a later packet nor the next frame may mutate the list
> being consumed.** Preserve deterministic order, define ties by command order,
> and provide explicit plane disable behavior and an **empty-frame path that
> cannot retain old HUD contents.**"*

**It is not a sampler, a font engine, a text-layout engine or a glyph cache.**
Text is glyph sprites and software owns the layout — the ruling and
`TWOD.SPRITE.md` both say so. **It is also not a second opinion about a
descriptor's VALUES**: a reserved role, a BACKDROP asking for ALPHA, a
zero-extent sprite and an unknown format are refused by the blocks that own
those rules, on their own counters.

## Clock and reset semantics
Single `gpu_clk`, synchronous active-low `rst_n`. Reset empties the ring and
abandons any publish walk in flight; no descriptor state survives, which is the
same sentence `TWOD.PLANE.md` and `TWOD.SPRITE.md` make and for the same reason
— descriptors are re-sent per frame.

## Input and output packet layouts

### In: the two records, one at a time, from CMD.EXEC
`pl_*` is `SetPlane`'s field set at the GENERATED widths; `sp_*` is
`DrawSprite`'s. They arrive at the record's end plus one cycle (CMD.EXEC's own
late-presentation rule: both records' last field ends at the last byte).

### Out: three destinations, and each is a port-for-port match
```
  d_*        -> zhao_twod_plane.d_*     the sealed plane pair
  s_*        -> zhao_twod_band.d_*      the sealed sprite list, in command order
  ld_bind_*  -> zhao_twod_sampler       which page region each descriptor samples
  atm_slot_o, line_scroll_o -> zhao_twod_sampler
```
Nothing is widened, invented or reconstructed.

## The ring, and why one ring holds both kinds

```
  wp          next write; every accepted record lands here
  cp          commit pointer; a packet's verdict moves it or rewinds wp
  seal_end    the last sealed frame's end -- also the CURRENT frame's start
  seal_start  the window being published: [seal_start, seal_start + seal_len)
```

`RING = 2 * MAX_DESC` entries of **333 bits**, tagged at bit 0. A plane record
and a sprite record share the word; the plane's fields end at 284 and the
sprite's at 312, so the three the two share — the page region — sit above both.

**The alternative was measured and refused.** Giving the plane its own staged
and committed register pair costs `2 slots x 2 copies x 303 bits = 1,212
flip-flops`, about **1,030 ALM** at this console's own measured 0.849 ALM per
register. In the ring the rollback is the same pointer arithmetic the sprites
already need and the flops go away.

**A packet in flight at the seal is not split.** Its staged records sit at
`cp .. wp`, beyond the sealed window, and commit into the next frame whole.

**The ring cannot wrap into the window being published**, and that is
arithmetic rather than a guard: `wp - seal_end < MAX_DESC` by the frame cap,
`seal_end - seal_start <= MAX_DESC` by the same cap one frame earlier, so
`wp - seal_start <= 2*MAX_DESC` — the ring's exact size. **There is deliberately
no wrap counter**: a counter that cannot fire is not evidence. The cap's counter
(`list_overflow_o`) is reachable with legal stimulus and the bench fires it.

## The seal edge comes from TWOD.BAND

`seal_i` is `zhao_twod_band.list_restart_o`, which is that block's own
`restart_c`:

```
  (frame_start_i && !sweeping_q) || (sweep_sync_c && !armed_q)
```

**It is NOT `frame_start_i`.** A tick landing mid-pass is deliberately ignored
by the band — ignoring it is what fixed the composed console's 582,261
underruns. A producer sealing on the tick would, on exactly those frames, append
the new frame's descriptors to the OLD frame's list, silently, with every
counter balancing. That is entry I39's *"two live wires are not a producer"*,
and it is the specific hazard a frame-scoped list invites.

**`zhao_twod_band.desc_mid_sweep_o` is the independent check.** Its two operands
— this block's replay walk and POST.COMPOSITE's raster sweep — are clocked by
different things, which is the property CLAUDE.md's metadata-bank defect
requires be established before a detector's zero is quoted.

## Ordering and ties

The ring is **append-only within a frame**, so the published order IS command
order. The ruling's *"define ties by command order"* therefore costs nothing:
two sprites with equal `order` are published in the order their records were
decoded, and the band composites by last write.

## Planes publish before sprites

Two passes over the same window. **The reason is a race, not tidiness:**
`zhao_twod_sampler` starts prefilling atmosphere lines at the frame tick and a
line is at least `LINE_W` clocks. Pass 1 visits every entry at two clocks each,
so the plane pair is programmed within `2*MAX_DESC + 4` clocks of the seal —
**132 at the shipped MAX_DESC = 64, against 384** for the sampler's first line.
A single interleaved pass would have put a `SetPlane` sitting at entry 63 after
the sampler had already filled most of a line from the previous frame's sky.

## The plane is frame-scoped, and that is the contract's own sentence

`TWOD.PLANE.md`: *"no descriptor state survives a reset, **because descriptors
are re-sent per frame**."* So **a slot the frame's committed packets never named
is DISABLED at the seal**, on `zhao_twod_plane.d_enable_i`, counted on
`slots_auto_disabled_o` and on **neither** refusal counter — a frame's own
intent must never read as a fault.

With the band's generation tag clearing the HUD, that is also the ruling's
**empty-frame path**: a frame carrying no records publishes two disables and an
empty list, and the screen shows the world.

## Backpressure rules
`pl_ready_o` and `sp_ready_o` are **permanently high**. Lowering them would
backpressure the command stream for the least important thing on the screen, and
would make a drop uncountable — a held offer and a new offer are
indistinguishable on the wire. That sentence is `zhao_twod_band`'s, at the same
door, and this block repeats it because it is the same law.

The publish walk honours `d_ready_i` and `s_ready_i` even though both are
constant high today. **A producer that assumes a constant is a producer that
breaks when the constant stops being one.**

## Memory ownership
One simple dual-port memory, `RING x 333` bits. Nothing else. No VRAM client, no
HPS client, no texel.

## Q formats and rounding
**None.** Every field travels at the width the consumer takes it at; this block
performs no arithmetic on a descriptor beyond the pointer walk. The fx16 affine
and UV pass through untouched.

## Latency (fixed or variable)
A record is staged in one cycle. The publish walk is `2*MAX_DESC + 4` clocks for
pass 1, at most 4 for the disables, and `2*MAX_DESC + 2` for pass 2 —
**bounded at `4*MAX_DESC + 6` = 262 clocks** at the shipped parameters, against
a 92,160-clock frame.

## Target throughput
One record per cycle in; one descriptor per two cycles out. Both are far above
what a 64-byte record stream can deliver.

## Overflow and malformed-input behaviour

| condition | behaviour |
|---|---|
| more than `MAX_DESC` records in one frame | **the tail is dropped and counted** (`list_overflow_o`) — the HUD must not fault a frame |
| `slot > 1` | refuse the record, count (`plane_refused_o`) |
| `role`, `blend` or `format` too wide for the consumer's port | refuse, count |
| a reserved bit set in `wrap`, `view_mask`, `flags` | refuse, count |
| `base` at or past `PAGE_WORDS` | refuse, count — never wrapped |
| `lstride` or `lheight` above 15 | refuse, count |
| `flags != 0` on a sprite | refuse, count (`sprite_refused_o`) |
| zero width or height, unknown format, reserved role, BACKDROP not REPLACE | **not refused here** — the consumer owns those rules and the ruling requires they be retained |
| a packet abandoned by CMD.EXEC | `wp` rewinds to `cp`; its descriptors never reach a frame |

## Counters and traces
* `planes_staged_o`, `sprites_staged_o` — accepted into the ring
* `plane_refused_o`, `sprite_refused_o` — **unrepresentable**, counted apart
  from the consumers' value refusals so the two can never be read as each other
* `list_overflow_o` — the frame budget's tail drop
* `packets_committed_o`, `packets_abandoned_o`
* `frames_sealed_o`
* `planes_published_o`, `sprites_published_o`
* `slots_auto_disabled_o` — a lawful disable, never a refusal
* `bind_conflict_o` — two descriptors whose binding slot agrees and whose page
  region does not. **Software aliasing, counted rather than repaired**, because
  repairing it would mean inventing a binding allocator the sampler does not
  have.
* `seal_overrun_o` — a seal arriving while the previous publish walk runs.
  Reachable with legal stimulus (two seals in consecutive cycles), which is why
  it is a counter and not a comment.

## Scalar reference function
**NONE, deliberately, and the argument is `TWOD.BAND.md`'s verbatim.** What this
block owns is a **schedule** — a ring, a commit pointer and a seal window — not
an arithmetic law, and a scalar oracle for it would be the same author's second
implementation of the same schedule. `tools/budget/refmodel_liveness.py` already
names six blocks whose declared symbol exists nowhere; a seventh would be a
cheque nobody intends to cash.

**The stronger check is used instead**: `twod_cmd_chain_directed.cpp` drives
real packet bytes through `CMD.DECODER` and `CMD.EXEC` to composited pixels out
of `POST.COMPOSITE`, which no scalar oracle can stand in for. The descriptor
BEHAVIOURS this block feeds are already owned by `zref::twod::plane_u` and
`zref::twod::sprite_u`, frozen before it existed.

## Directed tests
`tests/compositor/twod_cmd_directed.cpp`.

* a record staged, committed and published; the same record ABANDONED and never
  published — the atomicity case;
* **a packet in flight at the seal is not split**: its earlier records land in
  the next frame with the rest of the packet;
* the auto-disable: a frame naming slot 0 only disables slot 1, on the enable
  path, with neither refusal counter moving;
* the **empty frame**: no records, two disables, an empty list;
* every refusal clause above, one at a time, each on its own counter;
* `list_overflow_o` fired with a legal 65-record frame at `MAX_DESC = 64`, and
  **the first 64 still publish** — the tail is what drops;
* `bind_conflict_o` fired by two sprites sharing `src_id[1:0]` with different
  `base`, and **silent** when they share the same region;
* `seal_overrun_o` fired by two seals in consecutive cycles;
* the publish walk under a stalled `d_ready_i`/`s_ready_i`: **no entry is
  emitted twice**, which is the defect the emit/wait state split exists to
  prevent.

## Randomized differential tests
**None.** There is no oracle to differ against, by the section above. The
directed bench and the chain bench carry the load, and the random lane that
would be worth building here is a fuzz over the RECORD -- which
`tools/abi-gen`'s corpus already covers on the ABI side.

## Formal properties
**A formal lane is PLANNED and no file exists yet**, so it is named without a
path — see `reports/PHANTOM-CITATIONS-AUDIT.md`. Its properties:

* **the published window is a function of the committed records alone**: no
  record staged after the seal appears in the list being published;
* an abandoned packet contributes nothing;
* every frame publishes exactly two plane descriptors (named or disabled);
* handshake hygiene; reset clears the ring.

## Synthesis / resource ceiling
**Unbuilt (no Quartus).** Hand-count, in the unflattering direction:

| part | count | note |
|---|---|---|
| ring memory | `128 x 333` | **9 M10K** at the 256x40 shape — width-bound, so the second half is FREE |
| binding shadow | `8 x 21` + 8 valid | 176 flops |
| pointers, walk, state | ~60 flops | |
| output registers | ~470 flops | the two descriptor groups |
| counters | 13 x 32 | 416 flops |

~1,120 flip-flops at **0.849 ALM/register** (this console's own measured figure)
is **~950 ALM**, plus the comparators. **Ceiling: 1,200 ALM, 0 DSP, ≤ 10 M10K.**

**THE 9 M10K IS ARITHMETIC AND THE FIT IS THE THING THAT KNOWS.** Whether
Quartus infers a 333-bit word as M10K at all, or spends 42,624 registers on it,
is exactly what `design/fit_targets.yml`'s `zhao_twod_cmd` leaf row asks. **It is
NOT part of TWOD.BAND's 24-M10K pixel-band figure** — the owner's own
correction: *"the band contract distinguishes the 24-M10K pixel-band calculation
from additional display-list storage. Do not treat '24 M10K' as an established
all-in HUD cost."*

## Integration capture cases
* **a command-fed two-player HUD** — both regions, text as glyph sprites, at a
  realistic descriptor count, driven by real packet bytes.
* **a disabled next frame** — the sky and the HUD both vanish, and nothing of
  either survives into the empty frame.
* **a malformed descriptor beside legal ones** — the frame completes, the legal
  descriptors draw, and exactly one refusal counter moves.

## Notes

`MAX_DESC` must equal `zhao_twod_band`'s: it is the same budget, and the band's
`desc_overflow_o` would otherwise fire on a list this block believed it had
already capped.
