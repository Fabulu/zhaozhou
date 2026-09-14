# Packet D attribute/raster ABI — 2026-09-14

**Status:** implemented and simulation-verified as an excluded characterization boundary; not fitted, selected, or physical evidence.

This note resolves the concrete Packet-D choices left open by
`SHELL-TEXTURE-V3-COMPOSITION-ARCHITECTURE-20260913.md`. It does not change the
A–K order. Packet D remains the versioned binner/tile composition between the
landed Packet-C stage and the existing raster/TILESTORE/RESOLVE path.

## 1. Current reference arithmetic

Current `reference/src/zrender/rast.cpp` uses one deterministic scanline law for
depth, UV, alpha, fog, and Gouraud lanes:

1. Compute `grad_x = div_rhu_s128(dNdx, area2)` once per triangle/attribute.
2. At each global scanline, evaluate the full numerator at the pixel centre
   `(min_x, y)` and compute `row_q = div_rhu_s128(row_n, area2)`.
3. Step a signed 32-bit value by `grad_x` for each later X on that scanline.

`div_rhu_s128(n,d)` is `sat_s32(floor((n + floor(d/2))/d))` for positive `d`.
Negative exact halves round toward positive infinity. The existing unversioned
`zhao_raster_attrdiv` and `zhao_raster_attrstep` implement a different retained
candidate: symmetric half-away-from-zero and exact per-pixel quotient stepping.
They remain executable oracles for that candidate but cannot be called current
`rast.cpp` parity.

Packet D therefore adds:

* `zhao_raster_attrdiv_v2`: the current signed tie-to-positive/saturating divide;
* `zhao_raster_attrgrad_v2`: one X-gradient divide, one full row-start divide per
  covered row, then exact signed-32 stepping from the triangle's global
  scissored `min_x` into the current tile.

Reseeding at every tile's left edge is forbidden. Rounding a fresh quotient at a
tile boundary is not equivalent to accumulating the once-rounded gradient from
`min_x`.

## 2. Packet-D characterization scope

Packet D freezes exactly three live planes:

1. interpolated `invw24`;
2. interpolated signed `u_over_w`;
3. interpolated signed `v_over_w`.

Each numerator plane is 240 bits:

```text
{n0[95:0], dndx[71:0], dndy[71:0]}
```

with relative low-bit layout `[71:0]=dndy`, `[143:72]=dndx`,
`[239:144]=n0`. All three share the accepted positive `area2[46:0]` from
GEOM.SETUP. Packet D keeps vertex RGB/A flat. Gouraud adds four planes later;
AUX-bearing terrain adds world-X/W and world-Z/W later. Packet D must reject or
fault any `aux_required=1` input because its three-plane profile cannot derive
owner-sealed perspective-correct world X/Z. Its 224-bit AUX record is canonical
zero.

This characterization computes U/W and V/W before Early-Z. That is legal for the
functional seam but is not survivor-only pricing. G8A may not claim the eventual
"only survivors pay U/V" saving from this three-lane implementation.

## 3. Exact binner metadata

`zhao_geom_binner_v2` owns one opaque metadata record under the same accepted
triangle identity as its existing triangle entry. The default Packet-D width is
**1,157 bits**:

| Bits | Width | Field |
|---|---:|---|
| `[297:0]` | 298 | flat V3 request fields, preserving package low-bit layout |
| `[345:298]` | 48 | flat `{vertex_rgb,vertex_alpha,effect_tag,stencil_reference}` |
| `[377:346]` | 32 | fragment state |
| `[424:378]` | 47 | positive shared `area2` |
| `[436:425]` | 12 | signed scissored global `min_x` |
| `[676:437]` | 240 | `invw24` numerator plane |
| `[916:677]` | 240 | `u_over_w` numerator plane |
| `[1156:917]` | 240 | `v_over_w` numerator plane |

The 298 low request bits are the existing texture-request layout with U/V
removed: palette generation 8, palette slot 2, response class 2, base alpha 8,
base RGB 24, canonical-zero AUX context 224, AUX-required 1, weight 8, recipe 3,
LOD 8, binding 8, and sample count 2.

Source ID and six screen coordinates remain in the existing 142-bit triangle
entry. Tile origin and first/last are drain-derived. Clear data is frame-scoped;
it is captured at frame begin by the composition rather than repeated per
triangle.

The metadata bank is statically split into 29 ascending 40-bit slices. Its
physical image is `{3'b0,metadata1157}`. Every slice writes on the exact existing
`tri_we`/`tri_wa` event, reads on the exact existing `tri_ra`, and is captured
beside the matching `tri_q` before a drain job becomes valid. A token denial,
triangle-capacity wall, or chunk overflow writes no triangle metadata. No
wrapper-maintained counter or caller cookie exists.

The unversioned binner remains byte-for-byte unchanged and is the tile-order,
first/last, token, overflow, and counter oracle. A committed metadata-address
mutant must demonstrate full-record mismatch after an accepted/denied/accepted
sequence while ordinary binner outputs still match.

## 4. Attribute row broadcast and join

`zhao_raster_tile_pipe_v2` starts all three `zhao_raster_attrgrad_v2` jobs from
the same accepted binner record. A one-entry coverage-row broadcaster captures
each EDGEWALK row once and owns a three-bit delivered mask. Each lane sees its
valid until that lane accepts; the upstream row retires only after all three
accept. ANDing lane readies while broadcasting an ungated valid is forbidden
because one lane could accept alone.

The output bundle is valid only when all three lane outputs are valid. Their
`{row,col,last}` values must match each other. That identity comparison is a
shipping detector: mismatch consumes/drops the bad bundle, latches a recoverable
local raster abort, and then drains all remaining attribute/coverage work to
idle. It must not deassert ready forever. A committed one-lane coordinate mutant
must fire it.

`invw_q` is legal only when it is nonnegative and bits `[31:24]` are zero. An
illegal value follows the same terminating local-abort path; it is never silently
truncated. Legal joined values form:

```text
address    = {row,col}
invw24     = invw_q[23:0]
u_over_w   = u_q
v_over_w   = v_q
```

## 5. Typed Early-Z and Packet-C seam

The tile pipe constructs `zhao_texture_v3_request_v2_t` and
`zhao_raster_continuation_v2_t` through package fields. It then calls
`make_raster_pretex`/`pack_earlyz_payload`; new code may not copy numeric packet
slices.

Early-Z consumes the explicit 80-bit key:

```text
address8 + invw24 + fragment_state32 + source_id16
```

and carries the 410-bit opaque payload:

```text
continuation_tail48 + texture_request362
```

The post-Early-Z channel is exactly 490 bits. Its two-entry skid is drained in
normal mode into one `zhao_raster_texture_stage_v3` at explicit
`MIGRATION_SHADOWS=0`.

Packet D does not reset the skid through a generated/asynchronous reset. In abort
mode it synchronously asserts ready to the skid while suppressing stage valid,
thereby consuming and counting every unadmitted entry. It simultaneously stops
creating new usable candidates and drains remaining coverage/attribute results
into a sink. Packet C continues to drain every already-admitted owner output.

## 6. Ordinary completion and sequence abort

Ordinary tile completion requires all of:

* EDGEWALK done and no held coverage row;
* every attribute lane has finished its job and holds no quotient;
* no Early-Z output is held;
* 490-bit skid level is zero;
* no candidate is offered unaccepted to Packet C;
* Packet-C/V3 `quiet_o` is one;
* the real RASTER.FRAGMENT is idle.

Only then may a non-final triangle return to job idle or a final triangle swap
and start resolve. Current first/last multi-triangle accumulation and front/back
resolve overlap are retained. Stale oracle comments claiming no accumulation do
not override the implemented first/last protocol.

On Packet-C sequence mismatch or local attribute abort:

1. same-edge and later Packet-C admissions are suppressed;
2. every unadmitted 490-bit skid entry is synchronously consumed/cancelled;
3. coverage and attribute producers drain into sinks;
4. an already accepted fragment may finish its pending tile write;
5. no new swap or resolve starts;
6. `zhao_geom_bin_pipe_v2` accepts and sinks remaining binner drain jobs instead
   of starting tiles;
7. quiet is exposed only after binner, attributes, Early-Z, skid, V3, fragment,
   and any resolve already active before the alarm have drained.

A frame-clear request may reach Packet C and clear local recoverable abort only
at that complete quiet boundary. Packet D exposes abort/fault/quiet and exact
cancel/drop counters. It does not implement lease allocation, RELEASE, PUBLISH,
READY CDC, framebuffer-slot ownership, or a reset barrier; Packet H owns those.
The production order is Packet-D cancel/drain first, Packet-H RELEASE later.

## 7. Versioned composition and gates

Packet D adds only:

* `fpga/rtl/raster/zhao_raster_attrdiv_v2.sv`;
* `fpga/rtl/raster/zhao_raster_attrgrad_v2.sv`;
* `fpga/rtl/geometry/zhao_geom_binner_v2.sv`;
* `fpga/rtl/raster/zhao_raster_tile_pipe_v2.sv`;
* `fpga/rtl/geometry/zhao_geom_bin_pipe_v2.sv`;
* focused differentials, test-only source manifests, committed selector mutants,
  excluded manifest rows, CMake registration, and generated-top byte-identity
  proof.

Required executable evidence:

* V2 divider and row-gradient values against current `rast.cpp` semantics,
  including both signs, exact negative halves, saturation, row restart, and a
  cross-tile gradient whose fresh tile-edge division differs;
* V1/V2 binner equivalence for order, first/last, token denial, wall/overflow,
  counters, and full 1,157-bit metadata identity under stalls;
* flat-compatible old/V2 raster equivalence;
* textured V2 raster differential against zref on varying invw/UW/VW, depth,
  palette/raw index, multi-triangle accumulation, framebuffer backpressure, and
  resolve overlap;
* full hold checks at triangle metadata, coverage broadcast, attribute bundle,
  Early-Z output, 490-bit skid, Packet-C output, fragment, tile write, and resolve;
* sequence-identity positive control proving cancellation, `S=F+SD`, owner
  release, no mismatching/later fragment, no new swap, finite full-pipe quiet,
  and successful quiet-clear rebase;
* committed negative-half and omitted-global-min-X-accumulation attribute mutants, each with an exact non-vacuous signature; metadata identity, one-lane coordinate identity, omitted
  V3 quiet in ordinary swap, skipped occupied-skid cancellation, and Packet C's
  retained old-ready deadlock.

All five new RTL roots are immediately `excluded:not-yet-adopted`. Packet D adds
no production fit source, fit target, selected root, Packet-E memory mux, or
Packet-H lease logic. `fpga/rtl/common/zhao_shell_top.sv`, the Packet-B 26-source
interface artifact, and all unversioned binner/tile/attribute RTL remain
byte-for-byte unchanged. No Quartus fit is spent before the named G8A subsystem
boundary.

## 8. Implemented evidence

Packet D landed as three independently reviewable but atomically registered
layers:

* D1: `zhao_raster_attrdiv_v2` and `zhao_raster_attrgrad_v2`, both radix-2 and
  radix-4 current-zref differentials, with negative-half and omitted-min-X
  selector controls;
* D2: `zhao_geom_binner_v2`, exact old/V2 stream/counter parity and all 1,157
  metadata bits through denial, capacity, non-power-of-two chunk overflow and
  stalls, with an adjacent-address metadata mutant;
* D3: `zhao_raster_tile_pipe_v2` and `zhao_geom_bin_pipe_v2`, one 52-source
  closure containing the real old/V2 flat differential, three-plane textured
  path, held five-destination start and three-destination row fanouts, Early-Z,
  Packet C, TILESTORE and RESOLVE.

The final CTest inventory is exactly 13 tests: 5 ordinary and 8 inverse controls.
The D3 healthy lane executes five scenarios and 64,927 assertions in 26,392
clocks. Its five full-chain controls prove coordinate abort, omitted-V3-quiet
135/136-prefix corruption followed by bounded drain, identity cancellation with
`S=F+SD`, old-ready ordered-head deadlock, and skipped-cancel occupied-skid
stranding. D1/D2 add their arithmetic and metadata controls. The complete fresh
native boundary passes 13/13; Packet C remains 4/4 and Packet B 72/72.

Hostile review forced six behavioral repairs and four static-registration
repairs before acceptance. The final source checker reads only active
SystemVerilog after comments and `ifdef/ifndef/elsif/else processing, pins all
five start-valid equations and clear-valid source, exact source manifests,
profiles, diagnostic regexes, and an independent configure-time 13-name CTest
inventory. Production accounting closes 251 modules as 63 selected roots, 78
inside and 110 excluded with three tombstones; regenerated production-top bytes
remain unchanged.

These are simulation and accounting results only. The 29-slice metadata store,
three pre-Early-Z divider lanes, ALM/DSP/M10K cost, Fmax and legal terrain-scale
throughput remain unmeasured until G8A. AUX-on, Gouraud, Packet-E memory sharing,
Packet-H lease/CDC, shell connection and physical behavior remain HOLD.
