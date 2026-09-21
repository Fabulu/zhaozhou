# Contract — TWOD.BAND (the HUD band store and its admission law)

> Ledger: `design/blocks.yml` · owner ZH-069 (with TWOD.SPRITE) · phase 11
> Written 2026-09-21 (packet BANDBUILD) **before** the ledger row, per owner
> ruling R214.

## Why this block exists

`zhao_console_core.sv` entry I17 states the seam in one sentence:

> TWOD.SPRITE walks in **descriptor** order, one whole sprite at a time;
> `hud_*` is a random access in **raster** order.

POST.COMPOSITE asks for one HUD pixel per composited pixel, sweeping
`(x, y)` monotonically; the sprite walker delivers a whole sprite at a time,
wherever it happens to be on screen. Something has to hold the pixels in
between. **This block is that something, and nothing else.**

Owner ruling **R233** chose which shape it takes, from three that were costed:

```
                        M10K        frame cost        verdict
  frame store (2x)      704/553     fits              OVER THE DEVICE
  line ring (1 line)     ~12        133% of frame     cannot draw a HUD
  BAND (B rows x L)       12        11.1% of frame    TAKEN
```

**Structure 2's defect was never the buffer.** It cost `Σ heights × a
line-time`; the band costs `Σ areas`. The two can use the identical memory —
what separates them is the WALK ORDER, and that is what this block changes.

## What it is NOT

* **Not a second sprite walker.** It REUSES `zhao_twod_sprite` by handing it
  **band-clipped descriptors**. I17 has twice recorded a built block as
  missing, and the risk here is rebuilding one.
* **Not a colour law.** It stores RGB565 as the sampler produced it and
  performs zero colour arithmetic. See *Tint and blend*, below.
* **Not a display-list author.** The descriptor record itself (`twod_sd_*`,
  the CMD seam) is out of this block's scope: `SetPlane` is zero hits in
  `spec/commands.zidl` and the record is an OWNER decision. **The band imposes
  no new requirement on that record** (HUDBAND, endorsed in R233).

## Clock and reset semantics

Single `gpu_clk`, synchronous active-low `rst_n`. Reset clears the display
list, the band state, the bucket and every counter. No descriptor state
survives a reset, because descriptors are re-sent per frame.

## The structure, stated once

```
  twod_sd_*  -->  [ display list, MAX_DESC entries ]
                          |
                          |  once per band: a band-clipped slice per live sprite
                          v
                    zhao_twod_sprite  -->  zhao_twod_sampler  --> sc_*
                                                                   |
                          +----------------------------------------+
                          v
                  [ band store, L rows x LINE_W, 16 bit colour + GENW gen ]
                          |
                          v  rd_req_v_i / rd_x_i / rd_y_i  (a monotonic sweep)
                   POST.COMPOSITE hud_*
```

**B** is the band height in rows and **L** the store height, `L = 4B` in v1.
The store is `L/B` band slots; the filler fills a slot while the scanout drains
another, so the filler may run at most `L/B - 1` bands ahead.

## Memory ownership and its arithmetic

Two RAMs, both on-chip, **no external memory and no client index**.

**The band store.** `LINE_W × L` words of `16 + GENW` bits, `GENW = 24`.

R233's formula is `ceil(0.75 × L)`, and it is right **for a word of 20 bits or
fewer** — an M10K holds 10,240 bits and offers `512 × 20`:

```
  ceil(LINE_W * L / 512)   =   ceil(0.75 * L)      at LINE_W = 384, width <= 20
```

**That formula is trusted because it reproduces two numbers this tree stated
before it was written** (R233): `L = 240 -> 180`, which is R222's own table
including its 360/207/204 rows, and `L = 9 at 16 bits -> 7`, which is
POST.COMPOSITE's own contract figure for its nine-line ring. **It is not the
formula this block pays**, because the word is 40 bits wide and the
configuration is `256 × 40`:

```
  ceil(LINE_W * L / 256)   =   24 M10K            at LINE_W = 384, L = 16
```

**Why the word is 40 bits, and why it is not negotiable.** A band slot is
re-used every `L/B` bands and must not show the previous occupant's pixels.
There are exactly three ways to get that:

* **Clear the slot.** `B × LINE_W` = 1,536 writes per band against 1,536 clocks
  of band drain — the clear alone is 100% of the write port, for the whole
  frame. Measured, not guessed: `BANDS × B × LINE_W` = 92,160 = exactly one
  frame of clocks. Leading by N bands multiplies both sides, so it is
  unaffordable at any lead.
* **A tag that cannot repeat.** Impossible in a fixed width.
* **A per-row GENERATION tag wide enough not to wrap in a session**, which is
  what this block does. `gen_q[row]` is bumped when that row's band opens, and a
  word reads as HUD only when its stored tag equals it. **The whole clear is `B`
  counter increments.** Generations run `1 .. GEN_MAX` and **never 0**, so a
  word that has never been written — an M10K powers up at zero — can never be
  mistaken for a HUD pixel.

**R233's "the 17th bit is free at every L" is true and it buys a VALID BIT. It
does not buy FRESHNESS.** See *The M10K number MOVED* under **Synthesis**, which
also prices the cheaper exact form that was declined.

**A SCRUB WAS BUILT FIRST AND IT WAS UNSOUND**, and it is recorded because the
general form is worth keeping. It wrote generation 0 into idle write cycles over
"the slots the filler and the reader have both finished with" — and in a FIFO
that is running there are no such slots: with `L/B` slots, one is being filled
and the rest hold bands the reader has not reached. It erased bands between the
fill and the read, and the directed bench caught it as two missing bands of a
20-row sprite. **A free resource in a pipelined store is free only in the window
between the last consumer and the next producer, and a full pipeline has no such
window.**

**The display list.** `MAX_DESC` entries of `DESCW` bits — the frozen sprite
descriptor plus the per-sprite state this block adds (cursor `u`/`v`, lifecycle
state, reserved burst). M10K is **width-bound, not depth-bound**, at this
depth: `ceil(DESCW / 40)` M10K for any `MAX_DESC ≤ 256`.

**This is a real cost that R233's 12 did not carry, and it is recorded here
rather than discovered by a fit.** `MAX_DESC = 64` matches R233's re-walk
figure (4.2% of frame at B=4) exactly: 64 descriptors × 60 bands ÷ 92,160
frame clocks.

## Input and output packet layouts

### Display list in (`d_*`)

The frozen TWOD.SPRITE descriptor, field for field:
`{ x, y, w, h, u, v, a00, a01, a10, a11, format, palette, tint, blend,
   view_mask, order, src_id }`. No field is added, widened or dropped.

### Band-clipped slice out (`e_*`)

The **same record**, with three fields substituted:

* `y` ← the first row of the sprite that lies in this band;
* `h` ← the number of the sprite's rows that lie in this band (`≤ B`);
* `u`, `v` ← the sprite's row origin **at that row**, carried in the display
  list as a cursor.

**The cursor needs no multiply in the band loop.** It accumulates by
`(a01, a11)` once per row as the band advances, exactly as the sprite already
accumulates across rows inside a slice. A serial shift-add runs **once per
sprite per frame**, and only when the sprite's first live row is not its own
top row — a sprite clipped by the top of the screen, or one whose descriptor
arrived after the sweep had passed it. `x`, `w` and the four affine deltas are
forwarded unchanged: **the walk is not clipped in X**, because clipping it
would require stepping `u`/`v` by `a00`/`a10` and that is the multiply this
design exists to avoid. Off-screen columns are dropped at the write port and
counted.

### Colour in (`c_*`)

`zhao_twod_sampler`'s `sc_*` group, port for port. The band writes
`c_rgb_i` at `(c_x_i, c_y_i)`; `c_last_i` closes a slice.

### Raster read (`rd_*`)

POST.COMPOSITE's `hud_*` convention, which is RASTER.RESOLVE's: **address out
in cycle N, data in cycle N+1, and the response must hold through a stall.**

The band store is a synchronous memory read **unconditionally**, which gets
that for free: `rd_x_i`/`rd_y_i` do not advance while the compositor's pipe is
stalled, so the same word is re-read and the output is stable. `rd_valid_o` is
derived from the **same RAM word** as `rd_rgb_o` — the generation bits live in
that word — so the two can never disagree. That is deliberate, and it is
`CLAUDE.md`'s metadata-swap law read forwards rather than backwards: here the
two quantities SHOULD move together, because they are one word.

## Backpressure rules

* **`d_ready_o` is always high.** `TWOD.SPRITE.md`: *"descriptor count over the
  frame budget — drop the tail, deterministically by `order`, and count — the
  HUD must not fault a frame."* So the handshake always completes and the
  surplus descriptor is dropped here. Lowering `ready` instead would
  **backpressure the command stream** on a HUD overflow, stalling the whole
  console for the least important thing on screen — and it would make the
  refusal uncountable, because a held offer and a new offer are
  indistinguishable on the wire.
* `e_valid_o`/`e_ready_i` is the sprite walker's own handshake; a slice is held
  until accepted and is never withdrawn.
* `c_ready_o` is high whenever a band is open for fill. The write port is one
  pixel per clock and never stalls the sampler while the band is open.
* The read port has no backpressure — the compositor cannot be told to wait.
  **That is why the admission law exists.**

## THE ADMISSION LAW — owner ruling R235

> **Refuse the sprite WHOLE, and COUNT the refusal.**

`TWOD.SPRITE.md`'s formal property is *"a dropped sprite draws no pixels at
all — never a partial sprite"*, so a sprite that will not fit must be disposed
of **before rasterising**. R235's reasoning is R221's, applied to a place
players look: **the alternatives make an ABSENCE look like a RESULT**, and
dropping a HUD sprite silently is W10.

### The test, exactly

B and L come from a **leaky bucket**: drain rate `LINE_W` pixels per scanned
line, burst `BURST_PX = (L - B) * LINE_W` — the FIFO slack, 4,608 pixels at
B=4, L=16. Two accounts are kept:

```
  rate_q    the committed per-LINE pixel load of the sprites now live
  burst_q   the unreserved part of the bucket
```

At the **first band a sprite touches** — once per sprite per frame, before one
pixel of it is rasterised:

```
  new_rate = rate_q + w
  excess   = (new_rate <= LINE_W) ? 0 : min(w, new_rate - LINE_W)
  need     = excess * rows_remaining_on_screen        (saturating)

  ADMIT  iff  need <= burst_q
     then  rate_q += w ;  burst_q -= need ;  the sprite is LIVE for the frame
     else  the sprite is REFUSED for the whole frame and COUNTED
```

When the sprite's last on-screen row has been emitted, `rate_q -= w` and
`burst_q += need` (capped at `BURST_PX`).

**`excess` is MARGINAL, and that is the whole correctness of it.** Charging
each sprite the full over-rate of the stack it joins would price R233's own
worked example at 78,720 pixels instead of 3,840 and refuse a HUD the console
can draw. The marginal form reproduces R233's two numbers exactly:

* a **full-width 32-row status bar** alone: `excess = 0`, `need = 0` —
  *"sits exactly at rate"*, admitted free;
* **40 glyphs of 8×12 over it**: each charges `8 × 12 = 96`, total
  **3,840 = ten lines of burst**, inside 4,608. Admitted.

**Why this bounds the deficit.** Over any window the store falls behind at
`(Σ live w − LINE_W)⁺` pixels per line, and by construction that is at most
`Σ excess_i`; each sprite contributes its `excess_i` for at most `rows_i`
lines, i.e. `need_i`; and `Σ need_i ≤ BURST_PX` is precisely what admission
enforces. So the filler is never more than `BURST_PX` pixels behind, which is
the FIFO slack, which is `band_underrun_o` never firing.

**Admission is decided ONCE.** A sprite admitted in its first band is drawn in
every later band **without re-testing**, which is what makes "whole" true. The
only refusals are `desc_overflow_o` (the list was full when the descriptor
arrived — also whole, also before rasterising) and
`sprites_refused_budget_o`.

### Overflow and malformed input

| condition | behaviour |
|---|---|
| bucket cannot cover the sprite | **refuse the WHOLE sprite for the frame, count `sprites_refused_budget_o`** (R235) |
| display list full | drop the tail whole, count `desc_overflow_o`; `ready` stays high |
| sprite not for this view | skipped, charged nothing, not an error |
| sprite entirely off the bottom / top | skipped, charged nothing |
| a colour arrives for a row outside the open band | dropped, count `write_oob_o` |
| a colour arrives with x outside `[0, frame_w)` | dropped, count `pixels_clipped_o` — this is edge clipping, not an error |
| the compositor reads a band the filler has not finished | the stale word reads as absent; count `band_underrun_o` |
| the read address is not the monotonic sweep this block was promised | the read still answers from `(x, y)`; count `scan_addr_mismatch_o` |

## Tint and blend — CARRIED, NOT APPLIED, and counted

v1 stores the sampler's colour unmodulated. `sc_tint_o` and `sc_blend_o`
arrive here and are **counted, not applied**: `tint_dropped_o` and
`blend_dropped_o`.

**This is not a narrowing introduced by the band.** `zhao_twod_sampler`
already ships a counter named `tint_unapplied_o` and its header says the tint
is forwarded rather than applied; and a HUD blend against the WORLD colour
cannot be computed here at all, because the world pixel is not in this block —
it is in POST.COMPOSITE, whose `hud_*` stage is a REPLACE
(`o_rgb_o <= hud_valid_i ? hud_rgb_i : c10_q`). Putting a modulation law into
a store would be inventing a colour law in a composer, which is the refusal
entry I17 makes in its own words.

**What the band adds is measurement**: the sampler said "unapplied" per
sample; the band says it per HUD pixel that actually reached the screen, so
the size of the gap is a number rather than a sentence.

## Q formats and rounding

None. The band performs no arithmetic on colour. Positions are integer
pixels; `u`/`v` and the affine deltas are fx16 S15.16, carried and accumulated
exactly as `zhao_twod_sprite` defines them, with the same row-origin rule —
the cursor steps from the ROW ORIGIN, never from the last pixel of a row.

## Latency

* read: 1 cycle, held through a stall (above).
* a slice is emitted within `O(MAX_DESC)` cycles of a band opening.
* first touch of a sprite costs up to `3 × 16` extra cycles of the scan, once
  per sprite per frame, and only when its cursor must be fast-forwarded.

## Target throughput

One HUD pixel written per clock, one HUD pixel read per clock. The re-walk
overhead is `MAX_DESC` cycles per band — **4.2% of frame at MAX_DESC = 64,
B = 4** — and the sprite work is `Σ areas`, 11.1% of frame for R233's worked
HUD, against the line ring's 133%.

## Counters and traces

| port | fires on |
|---|---|
| `descriptors_o` | a descriptor accepted into the list |
| `desc_overflow_o` | a descriptor refused because the list was full |
| `sprites_admitted_o` | a sprite passed admission |
| `sprites_refused_budget_o` | **R235's counter** |
| `slices_emitted_o` | a band-clipped slice handed to the walker |
| `pixels_written_o` | a colour stored |
| `pixels_clipped_o` | a colour dropped at a screen edge |
| `write_oob_o` | a colour dropped for a row outside the open band |
| `band_underrun_o` | a read reached a band the filler had not closed |
| `scan_addr_mismatch_o` | the offered read address left the sweep |
| `tint_dropped_o` | a pixel carried a tint that was not applied |
| `blend_dropped_o` | a pixel carried a non-replace blend that was not applied |
| `order_inversion_o` | a sprite's `order` fell below the one before it |
| `bands_o` | bands closed |

**Every one of these is fired by legal stimulus at this block's own ports** in
`tests/compositor/twod_band_directed.cpp`, because every neighbour of this
block is bench-driven here. Several are **structurally unreachable in the
composed console**, which is why `tests/prod/tb_zhao_console_core_smoke.sv`
asserts them at zero and the directed bench is what makes those zeros evidence
rather than silence — the shape `post_gather_store_directed.cpp` already uses.

**`sprites_refused_budget_o` must DISCRIMINATE, not merely move** (R95). The
bench admits a list that fits **exactly** and requires the counter to stay
zero, then adds one pixel of width and requires exactly one refusal.

### The committed mutant, and what it is evidence ABOUT

`band_underrun_o` is unreachable in the composed console *while the admission
law is correct*, so its silence there is an argument, not a measurement.
`tests/mutants/zhao_twod_band_burst_mutant.sv` is a **wrapper** — it
instantiates the production module with `.*` and one parameter changed,
`BURST_PX` raised past the FIFO slack, which is exactly "admit everything".
Its driver has **inverted polarity: it passes when `band_underrun_o` FIRES**
and when `sprites_refused_budget_o` stays zero, which together say that the
admission law is the thing holding the guarantee up and not a coincidence of
the stimulus.

A wrapper rather than a copy, deliberately: `CLAUDE.md` — *"a wrapper that
instantiates the production module cannot drift, and must not be counted as a
copy"* — and `tools/design/wrapper_port_parity.py` checks the half a wrapper
CAN get wrong.

## Scalar reference function — DECLARED ABSENT, with the reason

**No `reference_model:` is declared, deliberately.** `refmodel_liveness.py`
already names six blocks whose declared oracle symbol exists nowhere; a seventh
would be a cheque nobody intends to cash, and the ledger row would read like a
claim.

What this block owns is a **schedule**, not an arithmetic law: the
marginal-excess bucket, the band order and the generation tag. A scalar oracle
for it would be the same author's second implementation of the same schedule,
which is the weaker of the two checks available. **The stronger one is used
instead**: the directed bench asserts R233's own two PUBLISHED numbers — the
full-width bar at `need = 0` and the 40 glyphs at `Σ need = 3,840` — which were
written down before this block existed. A model that reproduces a number an
independent document stated is worth more than one that reproduces itself; that
is R233's own test for the `ceil(0.75 × L)` formula, applied here.

Sampling, blending and descriptor interpretation are TWOD.SAMPLER's and
TWOD.SPRITE's, already frozen, and are not re-modelled here.

## Directed tests

`tests/compositor/twod_band_directed.cpp`:

* a single sprite lands at the right screen address, read back through the
  sweep;
* a sprite spanning band boundaries is drawn whole, with its cursor correct in
  every band — the case a per-band cursor multiply would get wrong;
* the generation law: a band slot re-used a full lap later shows nothing of its
  previous occupant, **with no clear cycle spent**;
* **admission discriminates**: a list that fits exactly refuses nothing; one
  pixel more refuses exactly one sprite, and that sprite draws **zero** pixels
  in every band, not merely in the band that refused it;
* R233's two worked cases: the full-width status bar at `need = 0`, and 40
  glyphs over it at `Σ need = 3,840`;
* edge clipping on all four sides;
* a read that leaves the sweep fires `scan_addr_mismatch_o` and still answers
  from `(x, y)`;
* a starved colour stream fires `band_underrun_o`.

## Randomized differential tests

Deferred with a reason rather than promised: the differential partner is
`zref::twod::band_u`, and the directed bench above is written against it. A
random lane is worth adding when the descriptor record exists and real display
lists can be generated — today every list is synthetic and the random lane
would be sampling the bench author's imagination.

## Formal properties

**PLANNED, no file exists** — named without a path, per
`reports/PHANTOM-CITATIONS-AUDIT.md`.

* a refused sprite writes no pixel in any band;
* an admitted sprite is never refused later;
* `rd_valid_o` is high only for a word written in the current generation of
  its row;
* handshake hygiene; reset clears staging.

## Synthesis / resource ceiling

**Arithmetic, not a measurement — Quartus has not been run** (owner R236: *"we're
not fitting now, we're going zero gaps"*).

```
  band store        ceil(LINE_W * L / 256)      =  24 M10K at L = 16, DW = 40
  display list      ceil(DESCW / 40)            =  10 M10K at MAX_DESC <= 256
  registers         ~500 flops x 0.849 ALM/reg  =  ~430 ALM
  combinational     bucket, comparators, the shared serial shift-add
  DSP               ZERO -- every multiply is serial shift-add or by a constant
```

**Ceiling: 1,400 ALM, 0 DSP, ≤ 36 M10K.** A leaf fit row in
`design/fit_targets.yml` asks exactly the question this arithmetic cannot
answer.

### The M10K number MOVED, and that is this contract's main correction

**R233's `ceil(0.75 × L)` is right for a word of 20 bits or fewer** — an M10K is
512 × 20 — and it reproduces R222's table and POST.COMPOSITE's contract figure
exactly, which is why it was trustworthy. **It stops being the right formula the
moment the word needs more than 20 bits**, and freshness makes it need 40:

```
  R233 as posed     16 bit colour + 1 valid, 512 x 20   ceil(0.75 * L) = 12
  as built          16 bit colour + 24 gen,  256 x 40   ceil(LINE_W*L/256) = 24
```

**"The 17th bit is free at every L" is true and it buys a VALID BIT. It does not
buy FRESHNESS**, and freshness is not optional: a band slot re-used every `L/B`
bands must not show the previous occupant's pixels, a clear costs 100% of the
write port for the whole frame (measured: `BANDS × B × LINE_W` = exactly one
frame of clocks), and no tag of fixed width is exact forever.

**The cheaper exact form is priced and NOT taken**, so the owner can buy it: a
separate one-bit-per-pixel **presence plane** (`LINE_W × L` bits, 1 M10K) can be
cleared a WORD at a time — 77 writes per band instead of 1,536 — which keeps the
colour store at 20 bits and 12 M10K, for **13 + 10 = 23 total against 34**. It
costs a write-combining buffer on the colour path, because setting one bit of a
20-bit mask word is a read-modify-write. It was declined because this block has
to be right the first time with no fit available to check it, not because it is
worse.

**R236 governs the number:** *"if the question is about keeping capability and
not having enough resources — we already don't have enough resources."* The
34 M10K is a fact to record. It is not a veto, and it is not a reason to shrink
`MAX_DESC` or `L` below what the HUD needs.

### One declared limit: a SOFT reset does not clear the store

`rst_n` clears the per-row generation counters and nothing can clear 6,144
words, so after a mid-session soft reset the generation sequence restarts at 1
and a word written before the reset can read fresh. **The console's reset is a
CONFIGURATION reset and an M10K comes up zero, which never matches** — value 0
is reserved and no live row ever carries it. The exposure is a soft reset
without reconfiguration, and it is stated here rather than left to be found.
`tests/compositor/twod_band_directed.cpp` starts each case from a configured
device for exactly this reason, and says so.

### And the generation tag is a BOUND, not an absence

24 bits gives 16,777,215 opens before a row's tag repeats, and a row is opened
`MAX_H / L` = 15 times per frame — about 1.1 million frames, five hours at
60 Hz. A word would have to survive that unwritten and then be read at exactly
the aliasing generation. Stated so the next reader inherits the number rather
than the word "never".
