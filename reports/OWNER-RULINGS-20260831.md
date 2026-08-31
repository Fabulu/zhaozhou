# OWNER RULINGS — 2026-08-31

Relayed by Fabian from bro's second architecture pass. **This file is the
durable home**; a run folder is the wrong place for anything that must outlive
the pass that received it.

Three of these supersede bro's first pass. Where they do, the superseded version
is noted so nobody re-derives it.

---

## 1. DEPTH — per-view named profiles, not one global pair

| profile | `wmin` | `wmax` | use |
|---|---|---|---|
| 0 `WORLD_LONG` | 1.0 m | 16,384 m | normal gameplay, large islands, long vistas |
| 1 `WORLD_STANDARD` | 0.5 m | 8,192 m | closer camera, smaller battle spaces |
| 2 `CLOSE` | 0.25 m | 2,048 m | creature inspection, cinematics, cramped scenes |
| 3 | reserved | reserved | future evidence-backed profile |

* Zero flags select `WORLD_LONG`, so existing captures keep meaning.
* **`wmax` is a depth CLAMP, not a far-clip plane.** GEOM.PROJECT's row 2 stays
  inert and the culler stays at five planes. Distant islands do not vanish at a
  wall; The Measure, fog and representation control distant visibility.
* **`scale`/`shift` are GENERATED from the profile through the frozen `rcp_u24`
  law.** They are not owner parameters and not artistic values.
* Travels in `SetView.flags` if the ABI audit permits two reserved bits;
  otherwise a small additive view-depth command. Captured and replayed.

Proof obligations: `wmin` → `0xFFFFFF` exactly; monotonic non-increasing over
the legal fx16 interval; `wmax` gives the generated nonzero floor; no
intermediate wrap.

*(Supersedes pass 1's single `wmin = 0.5 / wmax = 16384`.)*

## 2. ATTRIBUTE INTERPOLATION — prove exact stepping before buying dividers

Do **not** freeze a large per-pixel ATTRDIV farm. First prove and prototype
exact quotient/remainder stepping, using ATTRDIV only for triangle/tile setup
and returning its final remainder. Keep the divider path as oracle and fallback
until composed tests and Quartus evidence pass.

**See `reports/ATTRIBUTE_STEP_FINDING.md` — the stated formula is not the
shipped law, and the correction is small.**

## 3. WORKLOADS — `276,480` gets one meaning

* `276,480` = Z60 pixels × 3.0 conservative **pre-Early-Z overdraw** = 92,160 × 3.
* It is **not** texture layers, **not** divide count, **not** a measured frame.
* Canonical cross-mode pre-Z design target: **320,000 covered fragments/frame**
  (Z60 1× 92,160; Z60 stress 276,480; Duo stress 294,912; Storm stress 307,200).
* Canonical profiles must finish within **1,333,333 clocks**; 1,666,667 is the
  fault boundary, not the design target.
* Post-Z survivors, primary samples, AUX samples and material sample counts are
  **separate trace-derived vectors**, never a guessed survivor fraction.

## 4. BINNER — an external geometry parameter buffer, not a bigger arena

Do not grow the frame-resident M10K triangle arena to game size (~49% of every
M10K for one army). Build `GEOM.PARAMBUF` in local SDRAM:

* projected **unique** vertices per view (~24 B; terrain 1,089 unique vs 6,144
  corners; a meshlet ≤ 64 unique vs ≤ 126 triangles);
* compact triangle descriptors (~16 B, vertex ids not vertices);
* external tile-reference chunks, with the on-chip tile directory keeping only
  head/tail/count plus active tails.

`ENGINE0` stays the framebuffer writer; **`ENGINE1` owns the parameter buffer**
under a new frame-generation-checked `MEM.GUARD` region. On-chip: prefetch
FIFOs, a small projected-vertex cache, and a small **opportunistic** expanded
triangle-context cache (not the rejected full TriangleContext arena).

One tile lifecycle remains one clear and one resolve. No framebuffer readback,
no naive flush-and-continue.

**Overflow law:** The Measure degrades representations before sealing; a
declared content tier must fit its quotas; an unexpected hard overflow faults
the frame, drains, repeats the prior complete frame and records source ids. It
**never** publishes a frame missing an arbitrary tail of the army.

*(Supersedes pass 1's "just put the arena in SDRAM".)*

## 5. MESHFETCH / VDECODE — version the format, land raw first

Freeze a versioned 64-byte-aligned meshlet descriptor now (≤64 unique vertices,
≤126 triangles, u8 local indices, local bounding sphere, material, LOD, CRC).
Build **RAW/CANONICAL vertex format 0 first**. Packed rigid (format 1) and
two-weight skinned (format 2) are additive format ids chosen by an asset
bake-off on Zixxtrixx and ten structurally different creatures — measuring
silhouette differences, cel-band flips, normal angular error, bytes/vertex and
decoder ALM/DSP/Fmax. Bound centre is meshlet-local; radius uses maximum
absolute instance scale; cull only when outside **every** active camera.

## 6. LOOM — stream a topologically sorted graph

ARM/compiler supplies a parent-before-child node stream. The FPGA does **no**
recursion, cycle discovery, matrix inversion or gameplay-event emission.
Keep-world reparenting is computed on ARM between frames. Gait and formation
values come from Form/Field programs; Loom only composes transforms.
Body-patch giant stays prototype-before-silicon.

## 7. FORGE — one bounded topology generator

V1 families: ribbon, radial fan/ring, tube, radial shell, billboard sheet,
terrain cliff/skirt. **Shard burst is a particle population, not a Forge
primitive.** `MAX_SEGMENTS = 64`, `MAX_SIDES = 8`. Subdivision selected before
acceptance; never emit a partial primitive.

## 8. PARTICLES — freeze the stream architecture

Revised 128-bit record: `pos 54, vel 33, age 10, species 7, size 6, spin 6,
flags 4, variation 8`. Lifetime and recipe live in species descriptors.
Randomness is **stateless deterministic hashing**, not a small evolving seed.
Dense ping-pong sequential HPS-DDR streams; survivors compacted, children
appended in deterministic parent order; **existing survivors outrank child
spawns on overflow**. Required tier 32,768 active; 65,536 stretch after board
bandwidth evidence. Ladder: meshlet → triangle/shard → ribbon → sprite → glint
→ culled.

## 9. 2D / POST

One **time-multiplexed** restricted plane engine serving up to two descriptors —
not two copies. CLUT8/RGB565 nearest, affine/line-scroll, repeat/clamp; it must
not become a second TMU. Sprites use the primary TMU, no private sampler.
Quarter-linear-resolution effect buffers (Z60 96×60, Storm 80×60, Duo 128×60),
gathered **during tile resolve** rather than by re-reading the framebuffer.
Bounded scanout-side line compositor, initially ±8 px X and ±4 scanlines.

## 10. DEFERRED — not blocking v1

`INPUT.SNAC` (optional; MiSTer path satisfies the contract) · `MEASURE.HISTOGRAM`
(Measure v2) · dedicated `GEOM.WARP` acceleration (ARM first, cut-order 5) ·
`POST.ECHO` · terrain-class giant hardware binding before a software capture.

## 11. AUTHORITY — three classes

* **Class A — agent decides and proceeds.** Pipeline stages, ready/valid
  placement, FIFO depths, internal tags, cache associativity, arbitration among
  semantically equivalent schedules, unit-count fit frontiers, internal packing
  that is not an external ABI, counters, traces, formal properties, and failure
  handling the charter already implies.
* **Class B — agent proposes with evidence, then adopts the winner.**
  Compressed vertex representation, normal encoding, cache sizes, divider
  radix/unit count, parameter-buffer caches, particle capacity tier, post
  precision above the v1 minimum. Evidence: functional differential, resource/
  Fmax, trace workload, visual or capacity comparison.
* **Class C — owner approval required.** Game-visible behaviour, permanent
  command/cartridge ABI, **numeric law changing captures**, feature cut/defer,
  representation-ladder semantics, content-tier guarantees, removing a console
  capability, changing deterministic order.

## 12. BUILD ORDER

**Wave A — close the conventional renderer.** Green **full** CI (not a targeted
sweep) → depth profiles → exact attribute-step proof and prototype →
`GEOM.PARAMBUF` + ENGINE1 guard → binner emits compact external ids → context/
prefetch caches → TEXJOIN's bounded 0–3 sample sequencer and material combiner →
real pre-Z/post-Z/TMU/parambuf traces → **combined Quartus fit of shell +
renderer + TMU v2 + cache + AUX + Field/Earth**. That fit is existential: those
have never been proven to coexist at 100 MHz with 10% reserve.

**Wave B — conventional geometry assets.** Descriptor, raw format, MESHFETCH
burst path, VDECODE raw, compression bake-off, packed formats, pose/skin/project
through the textured renderer.

**Wave C — spectacle.** Loom, Forge, particles, Twin Horizons, glow/distortion/
post, Wound Lab.

Board truth and SDRAM bandwidth measurement run alongside; no absolute
bandwidth or resource claim before physical measurement.

---

## What this changes about the TMU

Nothing about its semantics — it is oracle-exact, connected through TEXJOIN, and
off the owner's decision list. What remains is optimisation, cache behaviour and
**physical closure**: TMU v2 + cache + TEXJOIN Quartus fit and Fmax, realistic
miss traces, and the combined cone. Functionally real; not yet silicon-proven at
100 MHz.
