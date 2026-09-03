# Contract — MEM.UPLOAD (HPS→VRAM resource upload engine)

> Ledger: `design/blocks.yml` · gpu clock · maturity SPECIFIED
> RTL: not built
> Reference: `zref::mem::upload_verdict` and neighbours
> (`reference/include/zref/zref_mem_upload.hpp`), written before the RTL

## Purpose and exclusions

MEM.UPLOAD moves **immutable resource bytes** from HPS DDR into local SDRAM and
declares them resident, atomically, with a generation that makes a stale copy
detectable.

**Written 2026-09-03 from owner direction.** The animation ruling
(`reports/ZHAOZHOU_ANIMATION_MEMORY_ARCHITECTURE.md`) makes HPS DDR the
authoritative home of every creature clip bank, because — the owner's words —
*"we can't fit the anims into the fast ram"*. That ruling states its hardware
impact is near-zero, and for the **datapath** it is: no new arithmetic, no
render-time path, `GEOM.POSE` unchanged. But it assumes a mover that **does not
exist**, and the recon of 2026-09-03 established that:

* `CMD.DMA` fetches sealed **frame packets** and, since step 6 of the
  DEBUG.FRAMEBLIT integration (2026-08-21), performs **"no blit engine and no
  VRAM writes at all"**;
* `SW.STREAM` is HPS-side software: it stages **complete pages in HPS DDR** and
  hands the hardware a sealed list. It does not cross the bridge;
* `DEBUG.FRAMEBLIT` is the only block that reads HPS and writes SDRAM. It is
  the right SHAPE — 64-byte bursts, a CRC gate, guarded writes — and the wrong
  BLOCK: it is a debug instrument, it is canvas-length locked, and it holds the
  framebuffer lease.

So there is no path for a byte to get from where a resource lives to where the
renderer can read it.

**This block is deliberately GENERAL, not an animation uploader.** Three
consumers need the identical service and each was on its way to inventing it:

| consumer | what it needs uploaded | ruling |
|---|---|---|
| creature animation | clip-bank pages | `ZHAOZHOU_ANIMATION_*` |
| the 8 km terrain world | 21,376-byte terrain pages | T1–T12, `SW.STREAM` |
| Sunder | its own resources | `reports/SUNDER.md` |

All three were competing for ENGINE client **6** (`TERRAIN_BUILD`). One engine
with a tagged request queue resolves that contention instead of ratifying it.

**Exclusions, each a specific refusal:**

* **No compression, decoding, or transformation.** Bytes arrive as they were
  staged. A transform here would make the local copy something the HPS cannot
  reproduce, and the whole architecture rests on both copies being the same
  validated bytes.
* **No eviction policy and no allocation.** The HPS decides what is resident
  and where it goes; this block executes a sealed instruction. Residency
  *policy* is `SW.STREAM`'s; residency *state* is `TERRAIN.RESIDENCY`'s.
* **No blocking of the render path.** This is a background client. A resource
  that is not resident in time is a *deadline fault*, never a stall — see
  Overflow.
* **No framebuffer writes.** That is `RASTER.FBWRITE`'s, and the lease is
  `DEBUG.FRAMEBLIT`'s.
* **No partial visibility.** See the atomicity law below.

## The two laws that are the whole point

### 1. A page becomes resident ATOMICALLY

An upload in progress is invisible. The renderer either sees the previous state
of that slot or the complete new page, never a mixture.

This is not fastidiousness. A half-uploaded clip page is bytes that look like
animation: the decoder would read them, produce a pose, and draw a creature bent
into a shape no artist authored, with nothing anywhere reporting an error. It is
the same failure the repository already legislates against in three other
places — the terrain frame fault (T6), `GEOM.PARAMBUF`'s refusal to publish a
frame with a truncated tail, and `SW.STREAM`'s rule that a half-built page list
is never exposed. The rule is the same one and it is stated once more here
because this block is where the bytes actually move.

The mechanism is a **generation counter per slot**, published only on the
completion beat. A consumer reading a slot mid-upload sees the OLD generation
and uses the old page; there is no window in which it sees the new generation
and old bytes.

### 2. A stale handle is REFUSED, never followed

Every upload carries `{epoch, generation}`. A request naming an epoch that has
closed is rejected and counted, not executed — because an upload that lands
after its epoch has ended writes correct bytes into a slot somebody else now
owns.

`MEM.GUARD` must gain generation rejection to enforce this; today it checks
region bounds only. **That is a required change to an existing block, not part
of this one.**

## Input and output packet layouts

### `UploadRequest` — 32 bytes, from the sealed HPS list

    hps_addr      u64   source in HPS DDR, 64-byte aligned
    vram_addr     u32   destination in local SDRAM, 64-byte aligned
    length        u32   bytes, a multiple of 64
    resource_id   u16   what this is, for counters and faults
    slot          u16   the residency slot being filled
    epoch         u16   the resource epoch this belongs to
    generation    u16   published on completion
    crc32c        u32   over the source bytes

`crc32c` uses the existing `zhao_crc32c_fold` — the console already has one CRC
law and this block does not add a second.

### `UploadComplete` — the only thing a consumer waits on

    slot          u16
    generation    u16
    resource_id   u16
    status        u8    {OK, CRC_FAIL, GUARD_REFUSED, EPOCH_STALE, ABORTED}

## Backpressure rules

Ready/valid on the request queue. The engine takes ENGINE bandwidth as an
ordinary client behind `MEM.VRAM.ARBITER`, at a priority **below** the render
path — a resource upload must never delay a fragment.

The HPS bridge side takes `MEM.HPS.ARBITER`'s backpressure. **That arbiter is
two-client over a one-burst bridge today and this engine is a third**; the
arbitration change is a required modification to `MEM.HPS.ARBITER` and is called
out in Notes rather than assumed away.

## Memory ownership

Reads HPS DDR through `MEM.HPS.BRIDGE`. Writes local SDRAM through
`MEM.VRAM.ARBITER`, inside regions declared in `MEM.GUARD`. It owns no memory
of its own beyond its request queue and one burst buffer.

**The guard map needs an appended region for the animation working set.** Its
size is a capacity decision that belongs to the owner and to a measured trace,
and is deliberately not invented here.

## Q formats and rounding

None. This block moves bytes and changes no bit.

## Latency (fixed or variable)

`variable` — SDRAM behind two arbiters. Latency is not a property a consumer may
depend on: the architecture's contract is that the HPS guarantees residency
*ahead of the deadline*, which is why the owner's rationale — demand is known
roughly a millisecond in advance — is what makes the whole design work. **This
block promises throughput and atomicity, never latency.**

## Target throughput

Sustained 64-byte bursts at the arbiter's granted rate. **The required rate is
not stated here because it must come from a trace, not from arithmetic.** Two
provisional ~41 MB/s figures exist in the repository (terrain streaming, and
bake) and have never been summed with an animation figure that does not yet
exist. Producing that combined budget is the `WordOfCaution` end-to-end capacity
obligation, which is the oldest unmet obligation in the repository and blocks
the Field core count as well.

## Overflow and malformed-input behaviour

* **A CRC mismatch fails the upload.** The slot keeps its old generation and old
  bytes, and the fault is counted with its `resource_id`. Bad bytes are never
  published.
* **A destination outside the guard map is refused and counted.** It is not
  clamped into range: a clamped address writes real bytes into a real slot
  belonging to somebody else.
* **A stale epoch is refused and counted.**
* **A length that is not a multiple of 64, or an unaligned address, is
  malformed** and refused. Alignment is a producer contract, not something to be
  fixed up here.
* **On any refusal the frame behaves as the animation ruling now says**: if a
  required resource is not resident by the deadline the **frame is not
  published**, the previous complete frame repeats under the hard-60-Hz late
  law, and the fault is recorded. **Owner ruling 2026-09-03: whole-frame, not a
  per-instance degradation ladder.**

## Scalar reference function

`zref::mem::upload_verdict`, with `upload_in_guard`, `upload_aligned`,
`upload_bursts` and `upload_visible_generation`
(`reference/include/zref/zref_mem_upload.hpp`) — **written 2026-09-03, before
the RTL.**

The ledger's schema requires every block to cite a `zref::` symbol, so
registering this block with no oracle would have manufactured exactly the
phantom citation `reports/PHANTOM-CITATIONS-AUDIT.md` exists to count. Writing
the symbol was cheaper than the alternative.

It owns the two things that can be silently wrong — the acceptance predicate and
the burst decomposition — and one that is easy to state and easy to get wrong in
RTL: **the visible generation during an upload**. It does not own bandwidth,
arbitration, residency policy or eviction, none of which has a scalar law.

Note `upload_in_guard` compares in **64 bits on purpose**: `vram_addr + length`
in 32 bits can wrap, and a wrapped sum compares as a small number, so a request
running off the end of the arena would read as comfortably inside it.

## Directed tests

**Planned and not written.** Named without paths for that reason:

* atomicity — a consumer reading a slot on every clock of an upload sees the old
  generation until the completion beat, and never a mixed page;
* a CRC mismatch leaves the old generation and old bytes intact;
* a destination one byte outside the guard map is refused, and the memory it
  would have written is unchanged;
* a stale epoch is refused;
* unaligned address and non-multiple-of-64 length refused;
* burst decomposition exact at 64 B, 65 B (refused), and the maximum length;
* backpressure — the render path's bandwidth is never reduced below its
  guaranteed share while an upload is in flight;
* abort mid-upload leaves the slot at its old generation.

## Randomized differential tests

Planned: random `{addr, length, epoch}` against the scalar acceptance predicate,
with a deliberate malformed fraction, and a coverage guard that the stimulus
actually reached each refusal class — the repository has repeatedly shipped
random tests that never hit their interesting case.

## Integration capture cases

None on hardware. No board, no programmed device.

## Notes — required changes to OTHER blocks

This block cannot work alone. Each of these is somebody else's contract:

1. **`MEM.HPS.ARBITER`** — two-client today over a one-burst bridge; needs a
   third client and a priority rule that keeps the command stream ahead of bulk
   resource traffic.
2. **`MEM.GUARD`** — needs the appended resource region **and** generation
   rejection, which it does not do today.
3. **`CMD.SCHEDULER`** — needs a resource-pin table so a frame is sealed only
   when every referenced resource is resident and pinned. Without it, nothing
   enforces the ruling's frame-publication law.
4. **`GEOM.POSE` / `zhao_geom_pose_cache.sv`** — its tag is
   `{lru, frame, clip, type}` with **no `sub`**, while the reference
   `zref::creature` carries `sub`, the half-key phase. With baked 60 Hz
   presentation data — which the ruling permits for any creature — a key and its
   midpoint alias, and the cache returns the wrong palette. **This is a live
   defect independent of this block** and is the smallest of the four fixes.
5. **ENGINE client 6** (`TERRAIN_BUILD`) — currently claimed by terrain,
   animation and Sunder. This engine becomes the single client and the consumers
   become tagged requesters.
