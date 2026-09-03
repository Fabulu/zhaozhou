# Owner brief, 2026-09-03 — normal maps stay; the animation path is viable

**Relayed by Fabian from bro, 2026-09-03, late.** Two questions were asked:
can the normal-map block be squeezed in, and is the planned path for animation
into the big RAM actually viable given that no connection exists today.

**This file is the durable home.** It is cited from `reports/DOCKET.md`,
`design/contracts/MEM.UPLOAD.md` and `design/contracts/TERRAIN.NORMALMAP.md`,
because the failure this repository keeps repeating is direction that was
written down and not read.

---

## 1. Normal maps: PROTECT THE FEATURE, DISCARD THE DRAFT

> "I would protect the normal-map feature. But we should not squeeze in the
> current normal-map RTL. That draft is fundamentally wrong."

Confirmed independently: the draft's divider produces zero for realistic
triangles, its reference can overflow, and it associates fragment shading with
triangles by timing rather than identity. `zhao_terrain_normalmap.sv` is
quarantined out of `design/prod_manifest.yml` and says so at the top.

| piece | what it actually provides | estimated cost |
|---|---|---|
| **TERRAIN.SHADE** | the terrain's missing ORDINARY lighting | ~730 ALM, 10 DSP, 1 M10K |
| **TERRAIN.NORMALMAP** | the high-frequency moving-light detail | ~380 ALM, 2 DSP, 8 M10K |
| combined | fully lit terrain with per-fragment relief | ~1,110 ALM, 12 DSP, 9 M10K |

> "The crucial discovery is that the 10-DSP piece is not really a normal-map
> expense. Production terrain currently has no proper lighting path at all: its
> normals are calculated, but nothing in the production hierarchy consumes them
> to shade terrain. TERRAIN.SHADE is necessary whether you use normal maps or
> not."

### The proportion that settles the argument

Against a texture-island recovery campaign targeting ~9,000 ALM of excess
structure:

* **normal-map detail alone is ~4% of the known ALM recovery;**
* terrain light plus normal-map detail is ~12%.

> "That does not prove the whole machine fits, but it means normal maps are not
> the thing presently threatening it. The broken texture storage structures
> are."

And on the decision to postpone the production fit:

> "A fit of the current bloated island would answer 'how large is the machine
> with 20,000 bits of RAM accidentally built as flops?' We already know that
> version is not the architecture we intend to ship."

### The challenge to the plan — a Pareto test, not a cut

TERRAIN.SHADE is specified at II=1, one triangle per clock, for ~10 DSP. **Its
producer delivers roughly one triangle every three clocks.** So:

> "one fully pipelined II=1 shade point; one time-shared II=3 shade point. The
> second may substantially reduce the 10-DSP base-light cost while still
> keeping up with the actual producer. That is an inference, not a measured
> result; joins, dropped triangles and bursts may make some headroom
> desirable."

**Do not sacrifice the 2-DSP normal-map delta first.** It runs per fragment and
genuinely needs throughput. The base-light block has the obvious unused
parallelism.

### The recommended order, verbatim

1. Repair cache and TEXJOIN storage so their large arrays really infer M10K.
2. Refit the recovered texture island against its numeric tripwires.
3. Put the new terrain-light and normal-detail law into ZRef and **look at the
   island under a moving sun at 240p**.
4. Fit TERRAIN.SHADE at both an II=1 and an II=3 resource point.
5. Add TERRAIN.NORMALMAP separately so its measured delta cannot be confused
   with the terrain's base lighting.

---

## 2. Animation in HPS RAM: the path is viable, and the route is not imaginary

> "Linux is not in the frame loop. That is the entire point of the residency
> architecture. Once a creature bank is resident, normal animation playback
> generates ZERO HPS traffic."

The intended path:

    cartridge / storage
      -> HPS loader validates clip bank
      -> HPS DDR authoritative bank
      -> SW.STREAM posts upload request
      -> MEM.UPLOAD reads 64-byte HPS bursts
      -> MEM.GUARD + MEM.VRAM.ARBITER
      -> fresh local-SDRAM resident slot
      -> atomic mapping publication + frame pin
      -> GEOM.POSE reads only local SDRAM

### Why the physical connection exists

The MiSTer framework already exposes HPS DDR to the core through a 64-bit
`DDRAM_*` interface (address, burst count, waitrequest, readdata, readdatavalid,
read, writedata, byteenable, write), and underneath it connects to the Cyclone V
HPS multiport SDRAM controller. Cyclone V SoCs support FPGA masters on the HPS's
FPGA-to-HPS SDRAM ports for exactly this traffic.

> "It is not 'maybe the board has some magic route we can invent.' It is 'the
> platform route exists; Zhaozhou has not written the adapter and transaction
> engine yet.'"

`zhao_hps_bridge` already speaks almost the required internal shape — aligned
bursts up to 64 bytes, 64-bit beats, grant/response, one transaction in flight.
And `DEBUG.FRAMEBLIT` already demonstrates the whole conceptual movement: HPS
burst read, 64-byte buffer, guarded local-SDRAM write, retirement accounting,
CRC, atomic publication. Framebuffer-specific and not reusable as-is, but proof
that the data path and its failure handling are not speculative.

### The finite missing work

1. **Framework adapter** — generic HPS burst signals to MiSTer `DDRAM_*`.
2. **General MEM.UPLOAD state machine** — read a 64-byte chunk, write it through
   the guard/arbiter, accumulate CRC, wait for every write to retire, publish.
3. **Third HPS-arbiter client** — command traffic highest, bulk upload
   background, debug last.
4. **Local-SDRAM resource region** — `MEM.GUARD` maps almost nothing but
   framebuffer windows today. It needs a guarded residency arena: writes allowed
   only to slots in UPLOADING state, reads only for published generations.
5. **Frame pinning** — the scheduler must keep a frame out of READY until every
   animation bank it names is resident and pinned.
6. **HPS software arena** — a physically addressable, non-moving staging arena,
   with CPU-cache coherency handled before the FPGA is told bytes are available.

> "None of those requires changing quaternion decoding, skinning, the animation
> format, or the render-time pose path."

---

## 3. TWO CORRECTIONS TO `MEM.UPLOAD` BEFORE IT BECOMES RTL

Both are now applied to `design/contracts/MEM.UPLOAD.md` and
`reference/include/zref/zref_mem_upload.hpp`.

### 3.1 Generation alone does NOT make an in-place upload atomic

The contract as first written said a consumer sees the old generation and old
page during an upload, and the new generation on completion. **That only holds
if the uploader does not overwrite the old page in place.**

> "Suppose it writes new bytes over the old slot while the slot still
> advertises the old generation. A consumer holding the old generation can
> still read a mixture of old and new bytes. The generation bit does not
> protect the underlying memory."

The correct law:

> **Upload into a fresh, unpinned, unpublished destination slot. Verify the
> complete copy and wait for every local-SDRAM write to retire. Then atomically
> publish a new mapping. Keep the old slot intact until its final frame pin is
> released.**

On CRC failure the new slot holds garbage but is unpublished and discardable;
the old mapping and old bytes are untouched. This is the transactional trick the
framebuffer blit already uses — speculative writes are safe because they target
an inactive, invisible destination.

Without this, the contract's own "old bytes survive CRC failure" guarantee
**cannot be implemented** with one destination slot and one 64-byte buffer.

### 3.2 The source address width disagrees, and the source is unguarded

`UploadRequest` defined `hps_addr` as u64. The generic HPS bridge exports a
**32-bit** HPS address, and the MiSTer core-side interface exposes a **29-bit
64-bit-word** address — effectively a 32-bit byte address.

> "We need to choose one explicit rule: require `hps_addr[63:32] == 0` and
> refuse anything outside the reachable HPS window; or introduce a registered
> upper-address/window translation mechanism. For this board, the first option
> is probably simpler. **But silent truncation is unacceptable.**"

And separately:

> "I would also add an HPS source-region guard. The FPGA bridge can see a broad
> HPS address range; an upload descriptor should only be allowed to read from
> the HPS staging arena registered for the active resource epoch, not arbitrary
> ARM/kernel memory. The current reference verifies the local-SDRAM destination
> but not a source capability."

---

## 4. Bandwidth, and what is genuinely unproven

Whole-bank residency first. **Do not begin with tiny page faults.** Copy the
entire active creature bank, publish it, pin it, leave it alone. Smaller clip
pages only if real traces show whole banks cost too much local SDRAM.

One 64-byte buffer first, because it is simplest to prove. If a physical trace
shows sequential read-then-write leaves bandwidth unused, two ping-pong chunk
buffers let HPS read N+1 overlap the local write of N — an implementation knob,
not an architecture change.

**The unproven parts, stated as unproven:**

* sustained board bandwidth. Terrain streaming assumes a provisional ~41 MB/s
  ceiling and that is not hardware evidence.
* **the installed HPS-DDR capacity on this SuperStation One configuration.** The
  public specification advertises the separate 128 MB BGA SDRAM and does not
  clearly state HPS-DDR capacity. **The board must be probed before software
  commits to a numerical HPS arena size.** The architecture survives a smaller
  HPS memory, because only currently loaded banks need be present.

---

## 5. Confidence, as stated

**Normal maps — high confidence they can stay.** ~380 ALM / 2 DSP / 8 M10K for
the detail organ, no second TMU sample, no cache pressure, a clean zero-delta
cut seam. If DSPs get tight, **lower the base shade's throughput point before
cutting the normal-map detail.**

**Animation in ARM RAM — high confidence the path is technically viable.** The
silicon supports it, the MiSTer framework exposes it, the generic HPS-burst
protocol exists, and frame blit demonstrates the transfer shape. What remains is
integration work, not a research problem.

> "With those corrections, animation in big RAM is not a compromise. It is the
> better memory hierarchy: arbitrary rich animation libraries on ARM, tiny
> deterministic working sets in local SDRAM, and no Linux timing anywhere near
> an active rendered frame."
