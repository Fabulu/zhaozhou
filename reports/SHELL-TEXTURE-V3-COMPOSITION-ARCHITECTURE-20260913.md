# Shell / Texture V3 composition architecture: the seam is inside the shell

**Date:** 2026-09-13
**Scope:** architecture only; no RTL, manifest, generated-file, build, Quartus, commit, push, or production-adoption work was performed
**Status:** proposed connected boundary; not implemented, not fitted, and not production evidence

## 1. Decision

The smallest truthful connection is:

```text
covered fragments in raster order
  -> exact attribute result needed for Early-Z
  -> RASTER.EARLYZ
  -> existing two-entry candidate skid
  -> one atomic V3 admission carrying the complete raster continuation
  -> zhao_texture_v3own (the only fragment-lifecycle owner)
  -> V3 ordered {status, sample-0 raw index, alpha, RGB, continuation}
  -> identity check and RASTER.FRAGMENT
  -> RASTER.TILESTORE front-bank read/modify/write
  -> bank swap
  -> RASTER.RESOLVE back-bank raster-order read
  -> RASTER.FBWRITE
```

There must be **no second fragment sideband FIFO with its own allocation, fullness, retirement, or release cursor**. The continuation that `RASTER.FRAGMENT` needs belongs in the context bank already controlled by `zhao_texture_v3own`; the owner captures it on its one admission event and returns it on its one ordered release event. A second queue which happened to walk in lockstep would be a second partial lifecycle owner and would recreate the exact disagreement this pass is intended to remove.

`zhao_shell_top` **cannot remain the live byte-for-byte implementation behind an outer wrapper**. Its Early-Z-to-fragment seam is internal to `zhao_raster_tile_pipe`, two hierarchy levels below the shell, and its public render input contains sampled texels rather than the coordinates, bindings, material state, palette identity, and AUX context from which V3 makes a sample. An outer top cannot transform a final `{RGB, A, index}` back into those request operands, cannot intercept the internal candidate stream, and cannot reroute the internal framebuffer path.

The D3 ruling remains valid for the specimen it governs: keep the current `zhao_shell_top` and generated `zhao_shell_fit_top` as the **pre-texture baseline**. The later connected implementation needs a sibling `zhao_shell_top_v2`; that sibling does not instantiate the old shell. Keeping the old bytes is useful baseline preservation, not a way to avoid the versioned shell change.

The selected V3 island remains the sole texture-fragment lifecycle owner. `zhao_raster_texjoin_v2` remains a source and behavioral oracle, but not a connected owner and not a selected physical saving. Retiring its accounting root corrects the census only.

## 2. What exists now

### 2.1 Current shell render boundary

`fpga/rtl/common/zhao_shell_top.sv:232-320` exposes one flat render path. Its triangle offer contains the current setup packet:

| field family | current fields |
|---|---|
| frame/grid | `render_frame_begin_i`, `render_frame_end_i`, `render_grid_w_i[5:0]`, `render_grid_h_i[5:0]` |
| edge equations | three each of `kx[22:0]`, `ky[22:0]`, `kc[47:0]`, plus `tl[2:0]` |
| screen vertices | A/B/C x/y, each signed 21 bits |
| scan bounds | min/max x/y, each signed 12 bits |
| caller identity | `render_src_id_i[15:0]` |
| flat fragment source | `render_fill_word_i[63:0]`, `render_state_i[31:0]`, `render_src_a_i[7:0]` |
| already sampled texel | `render_texel_rgb_i[23:0]`, `render_texel_a_i[7:0]`, `render_texel_idx_i[7:0]` |
| tile/framebuffer | clear word, framebuffer base and stride |

The connected hierarchy at `zhao_shell_top.sv:801-886` is:

```text
zhao_geom_bin_pipe u_render_bin
  -> zhao_raster_fbwrite u_render_fbw
  -> zhao_mem_guard u_guard_render
  -> global arbiter client 2 / ENGINE0
```

There is no V3 instance and no TEXJOIN instance in that hierarchy.

### 2.2 Current binner-to-raster boundary

`zhao_geom_bin_pipe` passes the binner's drain channel straight to `zhao_raster_tile_pipe` with no elastic record between them. The drain carries only:

```text
six signed 21-bit screen coordinates
job_first, job_last
signed tile_x[11:0], tile_y[11:0]
source_id[15:0]
```

The composition supplies the clear/fill/state/alpha/texel fields from module inputs held flat. `zhao_geom_binner` stores only `{source_id, six screen coordinates}` in its triangle RAM (`TRI_ENT_W = 16 + 6*21`). It does not retain an attribute plane, material binding, LOD, palette generation, AUX context, `w`, `invw`, `u_over_w`, or `v_over_w`.

This matters twice. First, those values do not exist at the V3 seam. Second, binner drain happens later than triangle acceptance, so values cannot safely be read from whatever triangle is currently on the shell pins.

### 2.3 Exact current tile path

`zhao_raster_tile_pipe` is currently:

```text
EDGEWALK -> EARLYZ -> zhao_skid2(W=168) -> RASTER.FRAGMENT
         -> TILESTORE -> RESOLVE
```

Coverage expansion emits the lowest covered column first and can offer one fragment each clock after a row mask has been loaded. The current candidate is 168 bits:

| field | bits | source |
|---|---:|---|
| address | 8 | `{row[3:0], col[3:0]}` |
| depth | 24 | `job_fill_word_i[31:8]`, `invw24`, larger is closer |
| state | 32 | flat job state |
| source ID | 16 | binner job source |
| vertex RGB | 24 | `job_fill_word_i[63:40]` |
| vertex alpha | 8 | flat job alpha |
| effect tag | 8 | `job_fill_word_i[39:32]` |
| stencil reference | 8 | `job_fill_word_i[7:0]` |
| sampled RGB | 24 | flat shell input |
| sampled alpha | 8 | flat shell input |
| sampled raw index | 8 | flat shell input |

The last seven fields form the 88-bit opaque Early-Z payload. Early-Z itself sees only address, depth, state, source ID, and that opaque payload. That parameterized payload is the clean insertion point: Early-Z need not learn texture semantics.

`RASTER.FRAGMENT` takes exactly:

```systemverilog
frag_addr_i       [7:0]
frag_depth_i      [23:0]
frag_state_i      [31:0]
frag_src_id_i     [15:0]
frag_vert_rgb_i   [23:0]
frag_vert_a_i     [7:0]
frag_tag_i        [7:0]
frag_sten_ref_i   [7:0]
frag_texel_rgb_i  [23:0]
frag_texel_a_i    [7:0]
frag_texel_idx_i  [7:0]
```

The index is not decoration. Alpha test compares the raw index with `ATEST_REF`, and the star/effect path forms tag strength from `texel_index[5:0]`. RGB and alpha cannot reconstruct it.

The current swap/drain guard is:

```text
!EARLYZ-output-valid && skid-level==0 && RASTER.FRAGMENT-idle
```

Any V3 insertion must add every new owner, queue, interpolation, fill, AUX, and output state to this drain law before a bank may swap.

### 2.4 Exact current V3 boundary

`zhao_texture_island_v3_top` currently accepts one fragment with:

| field | bits |
|---|---:|
| `frag_depth_i` | 24 |
| `frag_u_over_w_i`, `frag_v_over_w_i` | 32 each |
| sample count | 2 |
| base binding selector | 8 |
| LOD, Q4.4 | 8 |
| material recipe | 3 |
| recipe weight | 8 |
| opaque/AUX context | 64 by default |
| AUX-required flag | 1 |
| base RGB / alpha | 24 / 8 |
| supplied response class | 2 |
| palette slot / generation | 2 / 8 at the selected parameters |

It also has two global fixture inputs, `bind_base_i[31:0]` and `bind_mode_i[31:0]`; a one-line-request/eight-halfword cache fill boundary; palette-load inputs; and the AUX sheet request/response boundary with a 14-bit owner token at the selected generation width.

Its ordered output is only:

```text
valid / ready
RGB24
alpha8
caller tag16
refused1
```

The internal owner output already has the full opaque context, but the top exposes only `own_out_ctx[15:0]` as `out_tag_o`. The raw CLUT index exists at `clut_idx_c` and then disappears. Therefore today's V3 output is not a complete `RASTER.FRAGMENT` texel.

The V3 owner is configured as 64 owners, slot width 6, generation width 8, four-entry output and combine queues, and read-late sample planes. Its internal identities are:

```text
owner handle:  {slot[5:0], generation[7:0]}                         = 14 bits
sample handle: {slot[5:0], sample_index[1:0], generation[7:0]}      = 16 bits
route token:   {class[1:0], slot[5:0], sample[1:0], generation[7:0]} = 18 bits
```

`zhao_texture_v3own` alone allocates the owner, records accepted issues, validates terminal returns at full identity, creates combine work, emits in allocation order, and releases on ordered-output acceptance.

One comment on the V3 port calls `frag_depth_i` “w, for the reciprocal.” The connected arithmetic and the frozen perspective law make the semantic value **interpolated `invw24`**: the reciprocal reconstructs the scale used to turn interpolated `u_over_w` and `v_over_w` into U/V. `RASTER.EARLYZ` and `RASTER.FRAGMENT` also name this value `invw24`. The versioned interface must name it `frag_invw24_i`; it must not feed projected `w` to Early-Z or silently keep the ambiguous name.

### 2.5 Current V3 binding limitation

`frag_binding_i` is captured, and the expander's intended per-sample convention is base binding plus sample index. However, the current expander does not carry a binding selector on its request output, and `u_plan` receives the module-level `bind_base_i` and `bind_mode_i` directly. Thus a changing fragment binding is not presently a changing planner base/mode.

This is safe only in a fixture which holds one resolved binding stable for the complete island drain. It is not a production material-table interface. Inserting V3 into raster without fixing this would make the wiring connected while different materials continued to sample one global binding.

The current common-U/V, common-LOD, consecutive-binding convention is also narrower than `MATERIAL_ARCHITECTURE.md`'s eventual explicit `binding[3]`, `uv_set[3]`, and `lod[3]` record. That difference is not secretly solved by this seam. The baseline one-sample terrain recipe can exercise the minimum connection; a multi-sample adoption claim remains blocked until a material differential demonstrates the convention is sufficient or the descriptor is widened explicitly.

### 2.6 Current memory ownership

Global VRAM clients are positional:

| port | identity | current shell use |
|---:|---|---|
| 0 | SCANOUT | scanout reads |
| 1 | BLIT_DMA | framebuffer blit writes |
| 2 | ENGINE0 | render framebuffer writes |
| 3 | ENGINE1 | geometry reads |
| 4 | DEBUG | tied off |
| 5 | deliberately unspent | tied off and reserved |
| 6 | TERRAIN_BUILD | tied off in this shell |

ENGINE1 currently passes read-only requests only in `GEOM.ASSET_POOL`, `0x06A0_0000 <= address < 0x0800_0000`. The shell captures only “scanout or ENGINE1” for returning reads and packs every group of four SDR 16-bit words into a 64-bit beat before geometry sees it.

V3's cache needs a different response shape: one 16-byte-aligned line request, then exactly eight ascending 16-bit beats, with only one fill outstanding and no fill-data ready. It cannot be connected after the current universal 64-bit packer.

### 2.7 Packet-B authority and compatibility ledger

Packet B does not treat agreement between two stale implementations as a specification. Where the present artifacts disagree, authority is frozen in this order:

| question | authority for Packet B | artifact which loses on disagreement |
|---|---|---|
| fragment claim/issue/commit/final/emission/release | `zhao_texture_v3own` and its adversarial owner suite | any wrapper, resolver, AUX pipe, or combiner which tries to infer or duplicate owner lifetime |
| packet widths and named AUX offsets | Packet-A `zhao_render_texture_pkg.sv` | hand-written concatenations or the earlier 330/250 draft |
| material recipes, counts, RGB, alpha, raw index, and status reduction | `reports/MATERIAL_ARCHITECTURE.md`, **FROZEN MATERIAL COMBINER V1, owner ruling R9** | the stale six-recipe `TEXTURE.COMBINE.md`, current `zhao_texture_material_combine_v2.sv`, and current `zref_material.hpp` where they multiply the wrong alpha, gate MASK, use the wrong detail layers, or implement MODULATE2X as rounded-unit-multiply then double |
| Surface Sheet storage protocol: opcodes, handle, response status, source echo, and response hold | `design/contracts/SURFACE.SHEET.md` | the narrower unversioned AUX leaf interface |
| Packet-B AUX adapter, typed owner plane, issue/return, refusal, quiet, and no-sample-2 law | new `design/contracts/TEXTURE.AUX.V2.md`, which normatively imports the READ transaction from `SURFACE.SHEET.md`, plus R9 | `design/contracts/TEXTURE.AUX.md`, which is oracle-only for the unchanged old island, and the inherited old-island `has_aux ? aux : sample2` mux |
| old-island behavior used for a paired migration comparison | unchanged `zhao_texture_island_top` hierarchy together with unchanged `design/contracts/TEXTURE.AUX.md` | no claim that its AUX-as-sample-2 mux, narrow Sheet boundary, or stale material arithmetic is Packet-B product law |
| material/resource identity before frame-local selection | `commands.zidl` plus `MATERIAL.RESOLVE.md`, including full upstream binding-generation validation | Packet A's eight-bit consecutive selector, which is only a frame-local compatibility seam |
| production-accounting disposition | the actual instantiation closure checked by `check_prod_manifest.py` | prose labels such as `excluded:not-yet-adopted` applied to a module which is reachable from a selected accounting root |
| descriptor physical image and page-generation join | this report §§3.3 and 12.3/12.7: `physical320={33'b0,logical287}` and `zhao_texture_uv_join_v2` | implicit RAM padding or late reads of current active generation |
| island quiet | this report §4.5's named signals and literal Boolean equation | semantic “all terms empty,” owner-quiet aliasing, or an unlisted held valid |
| interface-manifest ABI and bytes | this report §12.1's closed schema v1 and canonicalization algorithm | ad hoc parser output, pretty JSON, host/tool defaults, or a hash which includes itself |
| A-to-B start gate | this report §13's explicit landing rule | the generic packet-overlap permission or the repository-wide “develop while fit runs” rule |

The current old island remains an executable oracle for order, stalls, routing, palette/direct decode, and the explicitly common material subset: PASSTHRU count 1 without AUX. It is not the arithmetic oracle for recipes 1–7. Packet B corrects `zref_material.hpp`, the combine contract, the new versioned RTL combiner, and their tests to R9 together. It does **not** edit the shared unversioned `zhao_texture_aux_pipe.sv` or `zhao_texture_material_combine_v2.sv`; `zhao_texture_island_top` therefore remains executable rather than becoming an oracle whose leaves changed underneath it.

Three safe, reversible defaults are chosen because the product ABI does not yet settle them: (1) Packet A's class/palette fields are sample-0 compatibility witnesses, never routing authorities; (2) successful AUX `{tag,strength}` is deliberately validated and then unconsumed in Packet B rather than disguised as sample 2; and (3) the frame-local binding page uses a nonzero eight-bit generation while upstream retains the full resource-generation check. Each choice has an explicit failure or HOLD below and can be widened without changing owner lifetime.

## 3. The versioned seam

### 3.1 New module boundary

The first connected implementation should add `zhao_raster_texture_stage_v3.sv`. It owns only the composition between an accepted post-Early-Z candidate and the existing fragment port:

```text
post-Early-Z candidate
  -> V3 request mapping and atomic admission
  -> one zhao_texture_island_v3_top
  -> ordered identity/status/index/result mapping
  -> exact RASTER.FRAGMENT input channel
```

It does not allocate fragment slots, does not keep a parallel continuation FIFO, does not alter tile words, and does not resolve pixels. `zhao_texture_v3own` remains the only lifecycle authority inside it.

`zhao_raster_tile_pipe_v2.sv` is the versioned composition that replaces the current skid-to-fragment wire with that stage. The current `zhao_raster_tile_pipe.sv` remains the flat-texel oracle. `zhao_geom_bin_pipe_v2.sv` carries the widened triangle metadata needed by the versioned tile pipe; the current bin-pipe remains the flat oracle.

The first shell consumer is `zhao_shell_top_v2.sv`. It is a sibling of the D3 specimen, not an outer wrapper around it. It copies no unrelated behavior differently: command, video timing, audio, and SDR control remain as they are. Its intentional changes are the render input, raster instance, texture memory share, texture configuration, complete render-drain law, and the writer-aware framebuffer lease/CDC contract in section 8; it cannot retain the old external writer authority.

### 3.2 Exact post-Early-Z packet, including typed AUX identity

The minimum packet which actually answers V3, the selected surface sheet, and `RASTER.FRAGMENT` is `zhao_raster_pretex_v2_t`, **490 bits** before the admission sequence is stamped.

The 128-bit raster continuation is:

| field | bits |
|---|---:|
| in-tile address | 8 |
| `invw24` depth | 24 |
| fragment state | 32 |
| source ID | 16 |
| vertex RGB | 24 |
| vertex alpha | 8 |
| effect tag | 8 |
| stencil reference | 8 |
| **total** | **128** |

AUX is a typed, owner-sealed 224-bit value. The declaration order below is intentionally MSB-to-LSB; the named offsets are the contract and prevent another concatenation from reversing X and Z:

```systemverilog
typedef struct packed {
  logic signed [31:0] env_z1;       // [223:192]
  logic signed [31:0] env_z0;       // [191:160]
  logic signed [31:0] env_x1;       // [159:128]
  logic signed [31:0] env_x0;       // [127:96]
  logic        [31:0] sheet_handle; // [95:64], {index[23:0], generation[7:0]}
  logic signed [31:0] wz;           // [63:32]
  logic signed [31:0] wx;           // [31:0]
} zhao_aux_surface_ctx_v2_t;
```

Thus `AUX_WX_LO=0`, `AUX_WZ_LO=32`, `AUX_SHEET_HANDLE_LO=64`, `AUX_ENV_X0_LO=96`, `AUX_ENV_X1_LO=128`, `AUX_ENV_Z0_LO=160`, and `AUX_ENV_Z1_LO=192`. **The existing V3 law is low 32 = world X and high 32 = world Z, i.e. the old 64-bit concatenation is `{wz, wx}`, not `{wx, wz}`.** Accessors use the struct fields or these named offsets; raw numeric slices are forbidden outside the package self-test.

For `aux_required=0`, the entire 224-bit AUX value is canonical zero. For `aux_required=1`, the sheet handle and all four envelope edges are part of the fragment accepted by V3. The handle is the full generation-bearing `handle32`, and the envelope must satisfy `env_x1 > env_x0 && env_z1 > env_z0`. A degenerate envelope returns a typed local refused AUX completion, sets the sticky frame fault, and issues no read under a guessed/default envelope. Handle residency and generation are not guessed locally: the exact sealed handle is presented to Surface Sheet, whose MISS is the terminal stale/missing-handle refusal.

The 362-bit V3 request portion is:

| field | bits |
|---|---:|
| `u_over_w`, `v_over_w` | 32 + 32 |
| sample count | 2 |
| base binding selector | 8 |
| Q4.4 LOD | 8 |
| recipe / weight | 3 + 8 |
| AUX required / typed AUX surface context | 1 + 224 |
| base RGB / alpha | 24 + 8 |
| sample-0 response-class witness | 2 |
| sample-0 palette-slot / generation witness | 2 + 8 |
| **total** | **362** |

Early-Z continues to take its explicit 80 bits (address, depth, state, source ID) and carries the remaining **410 bits** opaquely. The existing candidate skid therefore changes from `W=168` to `W=490`. This is a packet-width change, not an Early-Z algorithm change. These wider numbers supersede the earlier 330/250 draft; that draft carried world position but omitted the sheet/envelope identity needed to address two live terrain patches safely.

A free-running 32-bit raster admission sequence is stamped only on the V3 acceptance edge. The complete owner-carried raster retirement context remains 160 bits:

```text
{raster_sequence[31:0], raster_continuation[127:0]}
```

The sequence register advances only on `candidate_valid && v3_frag_ready`. It is stable while the candidate is stalled.

### 3.3 Put raster continuation in the existing owner; seal AUX under that owner

Retain the existing `CTXW=64` port as a **legacy caller context**, and add independent `RCTXW=160` and `AUXCTXW=224` parameters. The versioned V3 boundary replaces the ambiguous depth spelling and adds:

```systemverilog
input  logic [23:0]          frag_invw24_i;       // replaces frag_depth_i
input  logic [CTXW-1:0]      frag_ctx_i;          // legacy, unchanged meaning
input  logic [RCTXW-1:0]     frag_retire_ctx_i;
input  logic [AUXCTXW-1:0]   frag_aux_ctx_i;
output logic [RCTXW-1:0]     out_retire_ctx_o;
output logic [7:0]           out_texel_idx_o;
output logic [7:0]           out_status_o;
output logic                 quiet_o;
```

`frag_invw24_i` is the unsigned 24-bit interpolated inverse-W used bit-for-bit by Early-Z, perspective recovery, and `RASTER.FRAGMENT`; it is never projected `w`. `frag_ctx_i` is not repurposed as AUX. At owner admission the context is exactly:

```text
adm_ctx = {frag_retire_ctx_i, frag_ctx_i} // 160 + 64 = 224 bits
```

and at ordered output:

```text
out_retire_ctx_o = own_out_ctx[223:64]
out_tag_o        = own_out_ctx[15:0]       // legacy frag_ctx_i[15:0]
```

The connected raster stage has no legacy caller tag and drives all 64 legacy context bits to canonical zero. The compatibility differential may drive historical nonzero context and must observe exactly the old `out_tag_o`. In particular, world X is **not** exposed as the compatibility tag; the former draft's `out_tag_o = aux.wx[15:0]` is superseded.

The full AUX record and all early material witnesses are written on that same `own_adm_accept` into `zhao_texture_early_desc_v2`, under the allocated `{slot,generation}`. Its logical row is exactly **287 bits**, low field first:

| bits | width | field |
|---|---:|---|
| `[223:0]` | 224 | typed `zhao_aux_surface_ctx_v2_t` |
| `[231:224]` | 8 | LOD Q4.4 |
| `[233:232]` | 2 | sample-0 class witness |
| `[234]` | 1 | `aux_required` |
| `[236:235]` | 2 | sample count |
| `[238:237]` | 2 | sample-0 palette-slot witness |
| `[246:239]` | 8 | sample-0 palette-generation witness |
| `[254:247]` | 8 | retained Mosaic material A byte |
| `[262:255]` | 8 | retained Mosaic material B byte |
| `[270:263]` | 8 | retained Mosaic weight byte |
| `[278:271]` | 8 | base binding selector |
| `[286:279]` | 8 | captured active binding-page generation; zero means no sealed page |

These are the existing descriptor's 55 non-context bits, the full 224-bit AUX record, and one eight-bit page generation. Owner generation remains a separate side array and is compared with the independently offered read generation. The descriptor's logical and physical images are frozen separately; there is no implicit tool padding:

```systemverilog
logic [286:0] logical287;
logic [319:0] physical320;
logic  [39:0] slice [0:7];

physical320 = {33'b0, logical287};
for (int k = 0; k < 8; k++)
  slice[k] = physical320[k*40 +: 40];
```

Thus slice 0 holds `logical287[39:0]`, slices 1–5 hold the next five ascending 40-bit ranges, slice 6 holds `logical287[279:240]`, and slice 7 is exactly `{33'b0,logical287[286:280]}`; the pad is `physical320[319:287] == slice[7][39:7]`. `slice[0:7]` above is packing notation, not permission to infer a multidimensional unpacked RAM: RTL uses eight statically elaborated 40-bit-by-DEPTH held-read banks with constant `k`, plus the separate owner-generation bank. All eight slices are written on the same `own_adm_accept`. The write data for `slice[7][39:7]` is the literal constant `33'b0`, not old RAM data, a replicated payload bit, or a tool-generated width extension. Payload RAM is not reset.

An accepted descriptor read captures all eight slices and the separate stored owner generation under the same read acceptance, holds them unchanged through backpressure, and performs two **independent** checks before exposing a usable logical row:

```text
owner_generation_ok = stored_owner_generation == independently offered owner generation
descriptor_pad_ok    = (|captured_slice7[39:7]) == 1'b0
descriptor_usable    = owner_generation_ok && descriptor_pad_ok
logical287_read      = captured_physical320[286:0] only when descriptor_usable
```

`descriptor_pad_ok` compares stored physical pad bits directly with constant zero; it is not compared with a second value captured by the same enable. The descriptor port is exactly `output logic [31:0] desc_pad_fault_o`, and `desc_read_accept = rd_valid_i && rd_ready_o`. Reset drives the counter to `32'd0`; on `desc_read_accept && (|captured_slice7[39:7])` it updates once as `desc_pad_fault_o <= desc_pad_fault_o + 32'd1`, modulo `2^32`, and otherwise holds. A stalled read response cannot increment it again because only the accepted descriptor-read edge is counted. The same event separately sets the sticky frame fault; the sticky bit is not an alias, saturation bit, clear enable, or clock enable for the counter, and clearing either frame state or the sticky fault does not clear `desc_pad_fault_o`. Only reset clears the counter. The event exposes no bit from the corrupt logical row. The owner-mask copy already stored by `zhao_texture_v3own` is then the only obligation authority: each required sample/AUX receives its normal logical issue and a reserved next-or-later local `SOURCE_REFUSED` return, or an empty required mask creates the refused combine ticket directly. Pad failure therefore cannot turn stale RAM into work and cannot strand an owner. A legal write can never create nonzero pad; the detector's fire evidence is a committed descriptor mutant which drives exactly one `slice[7][39:7]` write bit high. Its inverse-polarity test requires a `desc_pad_fault_o` delta of exactly one and the terminal refusal path to fire. Separate ordinary tests assert all 33 stored/read pad bits stay zero, reset-to-zero, hold without an accepted bad-pad read, one increment despite output stalls, two distinct accepted bad-pad reads producing delta two, and `32'hFFFF_FFFF -> 32'h0000_0000` modulo wrap without changing sticky-fault semantics. Source/elaboration inventory proves `287 -> 320 -> 8*40` and all eight slice offsets.

The V2 descriptor has no allocator, head, tail, credit, or release cursor. It is subordinate owner-keyed storage. The expander transports its accepted row attached to the owner. Sheet handle, patch envelope, world position, page generation, and compatibility witnesses consequently stay immutable for the fragment lifetime without entering a second lifecycle.

#### Descriptor/UV join and page-generation carriage

The unversioned `zhao_texture_uv_join.sv` remains an old-island oracle and is not in Packet B's selected closure. Packet B adds `fpga/rtl/texture/zhao_texture_uv_join_v2.sv`. Its two accepted input records and one output record are exactly:

```text
desc input = {owner14, logical287}                 // 301 bits
UV input   = {owner14, U32, V32}                   // 78 bits
joined365  = {owner14, logical287, U32, V32}       // 365 bits
```

There is exactly one binding-page-generation field in this path: `logical287[286:279]`. The descriptor input, V2 join, output hold, expander and resolver never append or carry a second copy. Each input has its own one-entry held register and ready/valid acceptance; neither register changes while occupied. The join may assert output valid only when both registers are occupied and their independently captured 14-bit owners match. The entire 365-bit output record is registered/held and remains bit-stable while `out_valid && !out_ready`; input replacement cannot alter any held output field. A mismatching pair asserts the independently clocked `uvjoin_owner_mismatch_o`, sets the sticky frame fault, and produces no apparently valid joined work; it is an internal-corruption fire case, not a legal recovery input.

The captured generation remains solely in `logical287[286:279]` from descriptor write through joined output. When `joined365` is accepted, `zhao_texture_frag_expand_v2` consumes the logical row and copies that slice once into each held sample-job record; a sample job does not also carry `logical287`, so no stage holds both a row copy and a generation sidecar for the same job. `zhao_texture_binding_resolver_v2` compares the job byte with the resolver's active-page register. Neither module may sample live `active_page_generation_o`, reconstruct generation from an owner token, or capture it from the later UV offer. The active-page register's enable is the atomic activation edge rather than the join's capture enable. The stalled A/B control captures descriptor A, whose `logical287[286:279]=A`, and UV A, blocks `joined365`, then offers descriptor/UV B with logical generation B; every held A output and sample job must retain A in that one logical slice. Independent mutants (1) overwrite `logical287[286:279]` in a held joined record from current active page and (2) retain A's owner/U/V while taking B's entire generation slice; both must fail payload hold and the resolver comparison/fire control. `zhao_texture_uv_join_v2.idle_o` is true only when both input registers and the 365-bit output register are empty.

The generic owner is instantiated with `CTXW=RCTXW+CTXW=224` and `RESW=48`. This is a parameter and instrumentation use of `zhao_texture_v3own`, not a semantic lifecycle change.

### 3.4 Atomic admission

There is one event:

```text
tex_admit = cand_valid && cand_ready
cand_ready = v3_frag_ready
v3_frag_valid = cand_valid
```

Every V3 operand, the 224-bit typed AUX context, and the 160-bit retirement context are derived from the same held candidate. No branch may accept alone. State in the stage advances only on `tex_admit`; merely offering a valid candidate while V3 is not ready changes nothing.

The context capture must use `own_adm_accept`, the same event that allocates `{slot,generation}`. A local rolling context pointer is forbidden.

### 3.5 Complete V3 result, material authority, and required-source reduction

The Packet-A external result remains exactly 48 bits:

```text
[47:40] status
[39:32] sample-0 raw index
[31:24] alpha
[23:0]  RGB
```

`status[0]` is `SOURCE_REFUSED`; bits `[7:1]` are reserved-zero at every Packet-B producer, but queues and reduction OR all eight bits so a later typed status cannot be silently narrowed. `out_refused_o = out_status_o[0]`. Any nonzero status or malformed material raises the sticky frame fault. The terminal visible error value is the existing loud sample value `RGB=24'hFF00FF, A=8'hFF`, never admitted base colour or a successful partial recipe; raw index still follows the sample-0 rule below. A refused result remains terminal and handshakes normally so the owner releases and the shell can release, rather than publish, the frame lease.

The immutable owner-keyed material row is exactly **46 bits**, low field first:

| bits | width | field |
|---|---:|---|
| `[0]` | 1 | `aux_required` |
| `[2:1]` | 2 | sample count |
| `[5:3]` | 3 | recipe ID |
| `[13:6]` | 8 | recipe weight, unit8 raw/256 |
| `[21:14]` | 8 | admitted base alpha |
| `[45:22]` | 24 | admitted base RGB |

It is written only on `own_adm_accept`, read only under the matching owner, and held through combiner admission. No recipe, count, base, or AUX bit may be reread from live fragment pins. The owner's admitted `required_mask` is the source-obligation authority; the descriptor's count/AUX copies drive expansion and the material row's copies drive combine qualification, but both are captured on that same edge and independently checked against the owner mask. A mismatch is a protocol/frame fault and neither copy may rewrite or satisfy the mask. The retained Mosaic A/B/weight bytes in the early descriptor remain Mosaic inputs only; they cannot override recipe, recipe weight, or admitted base RGB/A in this material row.

#### One arithmetic law

Owner ruling R9 in `reports/MATERIAL_ARCHITECTURE.md` is the sole Packet-B arithmetic authority. The helpers operate independently on each RGB byte; all intermediates are wide enough before shifting or saturation:

```text
rescale_s(x,8)   = (x + 128) >>> 8          // signed arithmetic shift; ties toward +infinity
unit_mul8(a,b)   = (a*b + 128) >> 8
modulate2x8(a,b) = sat_u8((a*b + 64) >> 7)
lerp8(a,b,w)     = sat_u8(a + rescale_s((b-a)*w,8))
```

The complete legal table is:

| ID | recipe | legal count | RGB | alpha |
|---:|---|---|---|---|
| 0 | PASSTHRU | 0 or 1 | count 0: admitted base RGB; count 1: `s0.rgb` | count 0: admitted base alpha; count 1: `s0.a` |
| 1 | MODULATE | exactly 2 | `unit_mul8(s0.rgb,s1.rgb)` | `s0.a` |
| 2 | MODULATE2X | exactly 2 | `modulate2x8(s0.rgb,s1.rgb)` | `s0.a` |
| 3 | LERP | exactly 2 | `lerp8(s0.rgb,s1.rgb,weight)` | `s0.a` |
| 4 | ADD_SAT | exactly 2 | `sat_u8(s0.rgb+s1.rgb)` | `s0.a` |
| 5 | MASK | exactly 2 | `s0.rgb` | `unit_mul8(s0.a,s1.a)` |
| 6 | TERRAIN_DETAIL_LIGHT | exactly 3 | `unit_mul8(modulate2x8(s0.rgb,s1.rgb),s2.rgb)` | `s0.a` |
| 7 | TERRAIN_DETAIL_MASK | exactly 3 | `modulate2x8(s0.rgb,s1.rgb)` | `unit_mul8(s0.a,s2.a)` |

This explicitly rejects the current stale alternatives: recipes 1–4 do not multiply alpha; MASK is not a nonzero-alpha gate; detail first layers use MODULATE2X; and MODULATE2X rounds once as `(a*b+64)>>7`, not unit-multiply followed by doubling. Count zero is legal **only** for PASSTHRU and reads no sample. A count mismatch is frozen at admission as `material_refused`, raises the sticky material/frame fault, and cannot degrade. Its declared required sources each receive a logical issue followed by a local refused terminal return without planner/cache/sheet access; if the declared mask is empty, the owner creates the refused combine ticket directly.

The required-source mask is frozen at admission in owner order `{AUX,sample2,sample1,sample0}`:

```text
sample_count 0 -> sample mask 000
sample_count 1 -> sample mask 001
sample_count 2 -> sample mask 011
sample_count 3 -> sample mask 111
required_mask  = {aux_required, sample_mask}
```

The consequences are exact:

* `sample_count==0` issues no TMU work. Legal PASSTHRU returns admitted base RGB/A and index zero; status is zero when AUX is absent or its required terminal result is HIT, and refused when required AUX fails.
* AUX never substitutes for sample 2. **No recipe ID 0–7 consumes AUX as an RGB or alpha operand.** The inherited `s2 = has_aux ? aux : sample2` mux in the old island/material leaf is expressly superseded for Packet B.
* `aux_required==1` remains an independent owner obligation. The AUX terminal status participates in final status and the owner cannot combine until it commits, even at sample count zero.
* A TMU plane is typed `[47:40]=status, [39:32]=raw_index, [31:24]=alpha, [23:0]=RGB`. An AUX plane is separately typed `[47:40]=status, [39:32]=tag, [31:24]=strength, [23:0]=0`. The combiner reads only AUX status. As the safe reversible Packet-B default, a successful `{tag,strength}` is validated and then **deliberately unconsumed and unexposed**; Packet B makes no completed terrain-surface-effect claim. Adding a typed ordered consumer/output is an explicit later HOLD, not permission to reinterpret these bytes as a colour sample.
* Final status is the bitwise OR of only committed required source statuses plus `{7'b0,material_refused}`. Unrequested planes and uncommitted RAM contents are never consulted.
* Final raw index is zero at count zero and otherwise exactly committed `sample0.raw_index`. Samples 1/2, AUX, recipe arithmetic, and palette RGB never replace it.
* A wrong-generation return, out-of-range sample, unrequested source, return-before-issue, or duplicate return enters only the owner's typed protocol-fault handling. It marks the frame fatal but does not commit a plane, satisfy a required bit, overwrite a result, or increment a committed-source count. A later correct required return remains owed.

#### Response/index alignment

Every class-specific terminal response queue carries one immutable **66-bit** tuple:

```text
{route_token18, status8, raw_index8, alpha8, RGB24}
```

For CLUT8, `raw_index` is the addressed byte; for CLUT4 it is the selected nibble zero-extended. It is captured from the same accepted cache/class response as the route token and held beside that token through palette lookup and palette backpressure. The palette response registers and queues the original token, index, status, alpha, and resulting RGB together. Direct-colour and locally refused results use index zero. No response path may reconstruct an index from a live planner address, current metadata row, palette output, or whichever request is presently offered.

`zhao_texture_rsp_dispatch_v2` carries that tuple; the shared unversioned dispatcher remains untouched. `zhao_texture_material_combine_v3` consumes typed 48-bit planes, applies exactly the table above, preserves sample-0 index, and performs required-status OR. Its paired physical schedule remains one phase for PASSTHRU/ADD_SAT/MASK/refused work, two for MODULATE/MODULATE2X/LERP/TERRAIN_DETAIL_MASK, and three for TERRAIN_DETAIL_LIGHT; the arithmetic change does not license duplicate phase engines or a one-job-per-clock claim.

Packet B amends `design/contracts/TEXTURE.COMBINE.md` and `reference/include/zref/zref_material.hpp`, creates the normative `design/contracts/TEXTURE.AUX.V2.md`, and changes the new V3 combiner/AUX adapter and tests in the same packet. It leaves oracle-only `design/contracts/TEXTURE.AUX.md` and the unversioned old-island leaves unchanged. It amends the cache contract for typed terminal refusal/accounting used by Packet E. Agreement with the old unversioned combiner or AUX contract does not override R9/AUX V2.

Compatibility is exact and narrow: on PASSTHRU count 1 without AUX and without refusal, old and V3 RGB/A/order/refused must match, and `out_tag_o` remains `frag_ctx_i[15:0]`. On count zero, recipes 1–7, AUX-bearing work, or any refused source, the R9/new-status contract is authoritative and an old-island mismatch is expected evidence of the superseded law, not a reason to copy it.

### 3.6 Ordered output join

There is no associative join. V3 returns the complete result and continuation together in admission order. The mapping is direct:

```text
fragment.valid      = v3.out_valid && sequence_matches
v3.out_ready        = fragment.ready && sequence_matches
fragment.addr       = returned.addr
fragment.depth      = returned.invw24
fragment.state      = returned.state
fragment.source_id  = returned.source_id
fragment.vertex     = returned vertex RGB/A
fragment.tag        = returned effect tag
fragment.stencil    = returned stencil reference
fragment.texel      = returned RGB/A/raw-index
```

An independent expected-retirement sequence register advances only on that downstream handshake. It is compared with the owner-carried admission sequence. A mismatch raises a sticky identity fault and blocks the wrong write. The carried value and expected value have different clock enables—admission versus external retirement—so a held-output timing defect cannot move both operands together and hide itself.

The sequence comparator is an instrument, not the ordering mechanism. V3's owner cursor is the mechanism. Simulation independently scoreboards all 160 continuation bits and the complete result, so a correct sequence with a corrupted source ID or depth still fails.

## 4. Ordering, depth, identity, and storage laws

### 4.1 Ordering

The required orders are different and must not be conflated:

1. EDGEWALK expands coverage lowest-column-first in raster order.
2. Early-Z either removes a candidate or preserves survivor order.
3. V3 may execute reciprocal, sample, palette, AUX, and combine work out of order internally.
4. `zhao_texture_v3own` emits survivors in admission order.
5. `RASTER.FRAGMENT` performs front-bank read/modify/write in that same survivor order, including its same-address hazard law.
6. RESOLVE later reads all 256 back-bank addresses in its own fixed tile raster order.

Step 6 is not a continuation of fragment arrival order. It is the intended conversion from painter/fragment updates into a complete tile image.

### 4.2 Depth

One `invw24` quotient is produced before Early-Z. The exact 24 bits which Early-Z tests travel through the owner context and become `RASTER.FRAGMENT.frag_depth_i`; they are not recomputed after texturing. V3 also consumes that same `invw24` as the denominator input for its reciprocal/perspective path.

After the accepted fragment write, depth remains in tile-word bits `[31:8]`, as today. RESOLVE intentionally consumes the tile word but emits RGB565 and effect tag, not depth. “Preserve depth through resolve” therefore means preserve its test/write effect and stored value up to resolve, not invent a depth output the resolve contract does not have.

### 4.3 Source identity

The exact 16-bit source ID enters Early-Z, travels in owner context, reaches `RASTER.FRAGMENT`, and is echoed through TILESTORE's one-cycle read-A transaction. That closes the variable-latency texture seam without substituting V3's internal owner handle for the caller's source identity.

TILESTORE does not store source ID in its fixed 64-bit pixel word. RESOLVE's `start_src_id_i` labels a tile resolve transaction; in a multi-triangle tile it cannot describe the winning source of each pixel. The minimum composition does not claim otherwise. Preserving per-pixel source identity beyond the accepted fragment write would require a parallel source plane or a wider tile word and is a separate contract/resource decision, not texture glue.

### 4.4 Backpressure and hold

The following packets must be stable, field for field, while `valid && !ready`:

* the widened candidate entering the skid;
* every V3 input field and both contexts;
* both `zhao_texture_uv_join_v2` input records and exact `joined365={owner14,logical287,U32,V32}` output, with page generation only at `logical287[286:279]`;
* every expander/resolver sample job, including captured page generation;
* fill request address;
* the versioned AUX Sheet request `{op,handle,texel,src_id}` and response-ready contract;
* every held class response tuple and V3 ordered RGB/A/index/status/continuation;
* the exact `RASTER.FRAGMENT` input packet;
* fragment tile-read and tile-write requests;
* resolve and framebuffer output packets.

No valid may be a function of its own ready. The direct join adds no valid/ready branch and no acceptance bubble.

### 4.5 Structural quiet, drain, and bank swap

`own_ev_quiet` is necessary but is **not** island quiet. Packet B freezes observable, structural terms rather than the phrase “all pipelines empty.” The following local aliases must exist in `zhao_texture_island_v3_top`; an `idle_o` is one only when every valid bit, occupancy count, issued-but-unreturned credit, continuation, and held output owned by that instance is zero.

| top-local term | exact top-visible source | must be empty/include |
|---|---|---|
| `q_owner_idle` | `own_ev_quiet_w`, driven only by `u_own.ev_quiet_o` | no live owner and no capture, claim, issue bookkeeping, TMU/AUX commit, final, ready, combine, emission, ordered-output, or release entry |
| `q_rcp_idle` | `rcp_idle_w`, driven only by `u_rcp.idle_o` | reciprocal ingress, multiplier stages, ticket queues, and held quotient |
| `q_persp_idle` | `persp_idle_w`, driven only by `u_persp.idle_o` | pair-pipe stages and held perspective U/V result |
| `q_metajoin_idle` | `metajoin_idle_w`, driven only by `u_metajoin.idle_o` | both input holds and joined output hold |
| `q_desc_idle` | `desc_idle_w`, driven only by `u_early_desc.idle_o` | accepted descriptor read, eight captured slices, pad/generation verdict, and held response |
| `q_uvjoin_idle` | `uvjoin_idle_w`, driven only by `u_uv_join.idle_o` | descriptor input hold, UV input hold, and 365-bit output hold |
| `q_expand_idle` | `expand_idle_w`, driven only by `u_expand.idle_o` | fragment work, sample work, AUX work, and local malformed-descriptor disposition |
| `q_bind_idle` | `binding_data_idle_w`, driven only by `u_binding.data_idle_o` | lookup request/read result, pending logical issue disposition, planner offer, and local-refusal offer; configuration state is deliberately separate |
| `q_plan_idle` | `plan_idle_w`, driven only by `u_plan.idle_o` | every planner elastic stage and held cache request |
| `q_cache_idle` | `cache_idle_w`, driven only by `u_cache.idle_o` | access/hit/response stage, blocking miss, fill request, accepted fill awaiting verdict/data, partial fill, and held terminal response |
| `q_dispatch_idle` | `dispatch_idle_w`, driven only by `u_dispatch.idle_o` | credited read/metadata join and every class queue |
| `q_mosaic_idle` | `mosaic_idle_w`, driven only by `u_mosaic.idle_o` | all Mosaic stages and held result |
| `q_bilerp_idle` | `&bilerp_lane_idle_w[3:0]`, each bit driven only by the corresponding `u_bilerp[*].idle_o` | every versioned bilerp lane's channel/pipeline/result state |
| `q_palette_idle` | `palette_idle_w`, driven only by `u_palette.idle_o` | palette lookup, generation verdict, read latency, and held 66-bit response tuple |
| `q_palette_cfg_idle` | `palette_cfg_idle_w`, driven only by `u_palette.cfg_idle_o` | no accepted palette programming operation in flight; Packet B defines no acknowledgement channel |
| `q_aux_idle` | `aux_idle_w`, driven only by `u_aux.idle_o` | arithmetic/divider, offer FIFO, issued-identity FIFO, owed Sheet response, local refusal, typed return, and all reserved credits |
| `q_combine_idle` | `combine_idle_w`, driven only by `u_combine.idle_o` | admitted context, runnable phase, scratch/payload/completion reads, arithmetic/writeback, continuation, completion, and held result |

Packet B uses versioned successors wherever the unchanged shared leaf does not already expose this complete observation: `zhao_raster_rcp24_v4`, `zhao_raster_perspuv_pairpipe_v2`, `zhao_texture_metajoin_v2`, `zhao_texture_uv_join_v2`, `zhao_texture_tmu_plan_v2`, `zhao_texture_cache_pipe_v2`, `zhao_texture_mosaic_v2`, `zhao_texture_bilerp_lane_v2`, and `zhao_texture_palette_res_v2`. The already-new descriptor, expander, resolver, dispatcher, AUX V2, and combiner V3 expose the named idle/data-idle ports in their first version. These observation ports may be reductions of existing state only; they may not add a queue, credit, lifecycle transition, or combinational ready path. The corresponding unversioned leaves remain byte-for-byte in the old executable-island closure. `zhao_texture_v3bank` payload bits and descriptor RAM payload bits are not “busy” merely because unreset stale data exists; their independently owned valid/occupancy state is covered by owner/descriptor terms. Any stateful helper below a listed instance, including an AUX divider, is included in that instance's `idle_o` and cannot disappear from the equation.

The top also declares these exact one-bit channel aliases. They are assertions/checker inputs as well as equation operands, so a held boundary cannot be hidden inside a broad idle label:

| alias | exact top-visible source | represented channel |
|---|---|---|
| `q_frag_offer_valid` | `frag_valid_i` | top fragment offer, whether or not admission is enabled |
| `q_owner_claim_valid`, `q_owner_ready_valid`, `q_owner_combine_valid`, `q_owner_final_valid` | `owner_claim_valid_w`, `owner_ready_valid_w`, `owner_combine_valid_w`, `owner_final_valid_w` | owner claim, ready-ticket, combine-ticket, and final-result interconnect valids |
| `q_rcp_req_valid`, `q_rcp_rsp_valid` | `rcp_req_valid_w`, `rcp_rsp_valid_w` | reciprocal request and quotient response valids |
| `q_persp_req_valid`, `q_persp_rsp_valid` | `persp_req_valid_w`, `persp_rsp_valid_w` | perspective request and U/V response valids |
| `q_metajoin_a_valid`, `q_metajoin_b_valid`, `q_metajoin_rsp_valid` | `metajoin_a_valid_w`, `metajoin_b_valid_w`, `metajoin_rsp_valid_w` | both meta-join input valids and joined response valid |
| `q_desc_req_valid`, `q_desc_rsp_valid` | `desc_req_valid_w`, `desc_rsp_valid_w` | descriptor read request and eight-slice/verdict response valids |
| `q_uvjoin_desc_valid`, `q_uvjoin_uv_valid`, `q_uvjoin_rsp_valid` | `uvjoin_desc_valid_w`, `uvjoin_uv_valid_w`, `uvjoin_rsp_valid_w` | both V2 UV-join input valids and `joined365` output valid |
| `q_expand_frag_valid`, `q_expand_sample_valid`, `q_expand_aux_valid` | `expand_frag_valid_w`, `expand_sample_valid_w`, `expand_aux_valid_w` | expander fragment input and logical sample/AUX output valids |
| `q_bind_req_valid`, `q_bind_plan_valid`, `q_bind_refuse_valid` | `binding_req_valid_w`, `binding_plan_valid_w`, `binding_refuse_valid_w` | resolver lookup input, planner output, and local-refusal output valids |
| `q_plan_req_valid`, `q_plan_cache_valid` | `plan_req_valid_w`, `plan_cache_valid_w` | planner input and cache-access output valids |
| `q_cache_req_valid` | `cache_req_valid_w` | cache-access input valid |
| `q_fill_req_valid` | `fill_req_valid_o` | external cache fill request valid, including pre-guard stall |
| `q_fill_rsp_valid` | `fill_data_valid_i || fill_refused_i` | external fill data or refusal physically presented to cache |
| `q_cache_rsp_valid` | `cache_rsp_valid_w` | cache terminal response valid |
| `q_dispatch_req_valid` | `dispatch_req_valid_w` | dispatcher input response valid |
| `q_class_rsp_valid[3:0]` | `class_rsp_valid_w[3:0]`, assembled in fixed `{ERR,BIL,NEAR,CLUT}` order from the four class-output valid wires | one held tuple per class queue |
| `q_mosaic_req_valid`, `q_mosaic_rsp_valid` | `mosaic_req_valid_w`, `mosaic_rsp_valid_w` | Mosaic request and result valids |
| `q_bilerp_req_valid[3:0]`, `q_bilerp_rsp_valid[3:0]` | `bilerp_req_valid_w[3:0]`, `bilerp_rsp_valid_w[3:0]`, lane index ascending 0..3 | request/result valid for each bilerp lane |
| `q_palette_req_valid`, `q_palette_rsp_valid` | `palette_req_valid_w`, `palette_rsp_valid_w` | palette request and result-tuple valids |
| `q_palette_cfg_valid`, `q_palette_cfg_rsp_valid` | `pal_load_valid_i`, literal `1'b0` | palette programming command; Packet B has no acknowledgement channel |
| `q_tmu_return_valid` | `tmu_return_valid_w` | typed sample return valid offered to `u_own` |
| `q_aux_req_valid` | `aux_job_valid_w` | logical AUX job valid offered to `u_aux` |
| `q_sheet_req_valid` | `sheet_req_valid_w`, driven only by `u_aux.req_valid_o` | Surface Sheet READ request valid |
| `q_sheet_rsp_owed` | `aux_sheet_rsp_owed_w`, driven only by `u_aux.sheet_rsp_owed_o` | at least one accepted Sheet request still owes its response |
| `q_sheet_rsp_valid` | `pg_valid_i` | external Sheet response physically presented, including unsolicited/malformed traffic |
| `q_aux_refuse_valid`, `q_aux_return_valid` | `aux_refuse_valid_w`, `aux_return_valid_w` | local AUX refusal and typed AUX return valids |
| `q_combine_req_valid`, `q_combine_rsp_valid` | `combine_req_valid_w`, `combine_rsp_valid_w` | combine-job and completed-result valids |
| `q_retire_valid` | `out_valid_o` | top ordered retirement valid |
| `q_cfg_cmd_valid` | `cfg_valid_i` | top binding-config command valid |
| `q_cfg_rsp_valid` | `cfg_rsp_valid_o` | held top binding-config response valid |

Every `*_w` above is a named `logic` declared in `zhao_texture_island_v3_top` and is the same physical interconnect connected to the producing/consuming module port; it is not a recomputed busy guess. Except for the three explicitly shown expressions (`&bilerp_lane_idle_w[3:0]`, `fill_data_valid_i || fill_refused_i`, and literal zero), each `q_*` is a direct continuous alias of exactly one named port/interconnect. Hierarchical references to a child's `_q`, pointer, occupancy, or generate-local signal are forbidden.

The binding-control terms are also frozen as signals, not descriptions:

| control term | exact top-visible source |
|---|---|
| `cfg_loader_idle` | `binding_cfg_loader_idle_w`, driven only by `u_binding.cfg_loader_idle_o` |
| `binding_crc_busy` | `binding_crc_busy_w`, driven only by `u_binding.binding_crc_busy_o` |
| `binding_seal_pending` | `binding_seal_pending_w`, driven only by `u_binding.binding_seal_pending_o` |

The source-map audit treats every comma-separated scalar and every vector reduction above as a separate declared operand. It requires exact set equality `Q_source_map == Q_data_quiet ∪ Q_public_quiet`, requires the two equation sets to be disjoint, rejects a `q_*` use without one table source, rejects a mapped `q_*` omitted from both equations, checks single-driver connectivity from the named port/interconnect, and rejects any hierarchical child-state reference. Vector mappings additionally prove every declared bit participates in the reduction.

The following is the literal RTL Boolean law; no generated reduction may replace an omitted operand with a comment:

```systemverilog
data_quiet =
    q_owner_idle
 && q_rcp_idle
 && q_persp_idle
 && q_metajoin_idle
 && q_desc_idle
 && q_uvjoin_idle
 && q_expand_idle
 && q_bind_idle
 && q_plan_idle
 && q_cache_idle
 && q_dispatch_idle
 && q_mosaic_idle
 && q_bilerp_idle
 && q_palette_idle
 && q_aux_idle
 && q_combine_idle
 && !q_owner_claim_valid
 && !q_owner_ready_valid
 && !q_owner_combine_valid
 && !q_owner_final_valid
 && !q_rcp_req_valid
 && !q_rcp_rsp_valid
 && !q_persp_req_valid
 && !q_persp_rsp_valid
 && !q_metajoin_a_valid
 && !q_metajoin_b_valid
 && !q_metajoin_rsp_valid
 && !q_desc_req_valid
 && !q_desc_rsp_valid
 && !q_uvjoin_desc_valid
 && !q_uvjoin_uv_valid
 && !q_uvjoin_rsp_valid
 && !q_expand_frag_valid
 && !q_expand_sample_valid
 && !q_expand_aux_valid
 && !q_bind_req_valid
 && !q_bind_plan_valid
 && !q_bind_refuse_valid
 && !q_plan_req_valid
 && !q_plan_cache_valid
 && !q_cache_req_valid
 && !q_fill_req_valid
 && !q_cache_rsp_valid
 && !q_dispatch_req_valid
 && !(|q_class_rsp_valid[3:0])
 && !q_mosaic_req_valid
 && !q_mosaic_rsp_valid
 && !(|q_bilerp_req_valid[3:0])
 && !(|q_bilerp_rsp_valid[3:0])
 && !q_palette_req_valid
 && !q_palette_rsp_valid
 && !q_tmu_return_valid
 && !q_aux_req_valid
 && !q_sheet_req_valid
 && !q_sheet_rsp_owed
 && !q_aux_refuse_valid
 && !q_aux_return_valid
 && !q_combine_req_valid
 && !q_combine_rsp_valid
 && !q_retire_valid;

quiet_o =
    data_quiet
 && cfg_loader_idle
 && !binding_crc_busy
 && !binding_seal_pending
 && q_palette_cfg_idle
 && !q_frag_offer_valid
 && !q_fill_rsp_valid
 && !q_sheet_rsp_valid
 && !q_palette_cfg_valid
 && !q_palette_cfg_rsp_valid
 && !q_cfg_cmd_valid
 && !q_cfg_rsp_valid;
```

`q_fill_rsp_valid` and `q_sheet_rsp_valid` are excluded from `data_quiet` because unsolicited external offers are not accepted island obligations and must not deadlock activation; they are included in public `quiet_o` so the island never advertises quiet while a response is physically presented. Likewise `q_frag_offer_valid`, binding/palette configuration command/response state, and `binding_seal_pending` are public-quiet terms but not old-work drain terms. Packet B adds no held palette acknowledgement, so `q_palette_cfg_rsp_valid` is a literal-zero structural placeholder; retaining the named operand means a later version which adds an acknowledgement cannot silently escape the checker. The binding activator uses `data_quiet`, not `quiet_o`: a successful END stops new fragment admission, existing accepted work drains, and the active bank/generation changes atomically on the first `data_quiet` edge. That edge clears `binding_seal_pending`, changes the loader to IDLE, and creates the held END response. Public quiet therefore remains low until that response is accepted and every external offer is absent; there is no circular wait.

For `zhao_raster_tile_pipe_v2`, outer `pipe_empty` additionally requires:

```text
no Early-Z candidate held
candidate skid level == 0
attribute producer has no covered-pixel record held
V3 input has no unaccepted candidate
V3 quiet_o == 1
RASTER.FRAGMENT idle
```

The swap condition remains `walk done && pending mask empty && no new coverage accept && pipe_empty`, followed by the existing resolve-ready gate. The next non-final triangle for the same tile also waits for this drain in the minimum version. Inter-triangle texture overlap is an optimization only after this composition meets its measured frame budget.

The quiet gate independently parks every table row and every channel alias high/nonempty while all other terms—including `q_owner_idle`—indicate empty and requires `quiet_o==0`; it then removes that condition and reaches true quiet. It explicitly holds each request, response, config response, and retirement valid under backpressure. A committed `quiet_o=q_owner_idle` mutant, one omission mutant for **each named equation operand** (not one per prose group), and an AUX-issued-credit mutant must fail. A zero occupancy detector is not credited until its legal positive control or committed unreachable-state mutant fires. The checker also parses the Boolean expression and fails a term present in the tables but absent or negated with the wrong polarity in either equation.

## 5. Attribute production is a real prerequisite

The shell cannot manufacture V3 requests from its existing ports. `zhao_geom_setup` explicitly has no depth or attribute gradients. The exact arithmetic does exist as separate, uncomposed blocks:

* `zhao_geom_attrsetup`: one attribute request produces signed `{n0[95:0], dndx[71:0], dndy[71:0]}`;
* `zhao_raster_attrinterp`: exact numerator stepping in raster order;
* `zhao_raster_attrdiv`: exact round-half-away-from-zero quotient;
* `zhao_raster_attrdiv_svc`: ordered tagged parallel divider service;
* `zhao_raster_attrstep`: exact quotient/remainder stepping with seed/reseed divides rather than one divide per pixel.

The minimum live attribute set is:

| stage need | attribute |
|---|---|
| before Early-Z | `invw24` |
| V3 request | `u_over_w`, `v_over_w` |
| fragment shading when Gouraud is enabled | R, G, B, A |
| AUX terrain request | final world X, world Z, normally derived from perspective-correct world-X/W and world-Z/W |

An attribute plane is exactly 240 bits. Plane setup happens once per triangle. Pixel stepping must be driven by the same coverage rows and address cursor as the fragment candidate; independent walkers must compare row/column at every accepted bundle.

The first implementation should use `zhao_raster_attrstep` as the exact arithmetic candidate and retain `ATTRDIV` as its differential oracle. It preserves one accepted covered value per clock **inside a seeded covered-row run**, but pays documented step and row seed/reseed bubbles. The number of parallel lanes and seed dividers is selected by a legal workload trace before a fit, not guessed from the worst-case attribute count.

Early-Z must remain ahead of reciprocal, TMU, palette, AUX, and material combine. It is acceptable for exact numerator/quotient stepping needed to form the candidate to occur before Early-Z; it is not acceptable to sample a texture and then ask whether the fragment was hidden.

AUX world context is not present in the current setup/binner packet. For `aux_required=0`, the typed 224-bit record is canonical zero. Before AUX-bearing terrain can be connected, the shared reciprocal result must produce perspective-correct world X/Z or an independently proven exact upstream service must do so. A flat triangle centroid is not an acceptable substitute.

### 5.1 Owner-sealed terrain AUX identity

An AUX request is addressed by more than U/V. The owner-sealed record is:

```text
owner handle14
sheet handle32 = {patch index24, sheet generation8}
patch envelope = {env_x0, env_x1, env_z0, env_z1}, four signed Q16.16 words
fragment point = {wx, wz}, two signed Q16.16 words
```

On `own_adm_accept`, the complete `zhao_aux_surface_ctx_v2_t` is written beside the owner generation. The expander reads it only through `{slot,generation}` and emits one AUX job carrying owner14 plus the same sheet handle/envelope. No module may source an envelope from current terrain pins or a global “active sheet” register after admission.

Packet B creates the normative artifact **`design/contracts/TEXTURE.AUX.V2.md`**. That contract owns exactly the typed 224-bit context in section 3.2, the independent AUX bit in the required-source mask, logical issue before every terminal return, the 48-bit AUX owner plane `[47:40]=status,[39:32]=tag,[31:24]=strength,[23:0]=0`, the request/response credit and hold law, mandatory `sheet_rsp_owed_o`, the READ validation below, accepted/completed accounting, structural-idle contribution, and the rule that AUX never occupies or substitutes for TMU sample 2. It normatively imports only the Surface Sheet storage transaction—READ opcode, `handle32`, texel, source echo, HIT/MISS and ready/valid hold—from `design/contracts/SURFACE.SHEET.md`; where that storage contract and AUX V2 disagree about the adapter or owner plane, `TEXTURE.AUX.V2.md` owns the adapter and `SURFACE.SHEET.md` owns the store. The existing `design/contracts/TEXTURE.AUX.md` is frozen as **oracle-only** documentation for unchanged `zhao_texture_aux_pipe.sv`/`zhao_texture_island_top`; it is not amended, imported as Packet-B behavior, or permitted to outvote AUX V2.

`zhao_texture_aux_pipe_v2` is a new leaf implementing that new contract; Packet B does not modify the unversioned AUX pipe shared by the old island. Its accepted Surface Sheet request is exactly the `SURFACE.SHEET` packet:

```text
req_op[1:0]     = 2'd1 (READ)
req_handle[31:0]= sealed sheet_handle32
req_texel[11:0] = {sheet_v[5:0], sheet_u[5:0]}
req_src_id[15:0]= {2'b00, owner_handle14}
```

The versioned boundary carries `req_valid/req_ready` and the complete response `pg_valid/pg_ready, pg_op[1:0], pg_status[1:0], pg_tag[7:0], pg_strength[7:0], pg_src_id[15:0]`. It also exposes the mandatory structural-observation port:

```systemverilog
output logic sheet_rsp_owed_o;
```

`sheet_rsp_owed_o` resets low and is exactly `(issued_identity_fifo_occupancy != 0)`: it rises on an accepted Sheet request which creates the FIFO entry and remains high while any accepted READ lacks its one consumed HIT/MISS/malformed terminal response. It falls only as the final owed response is consumed; moving that response into a local AUX return may clear this port, while `idle_o` remains low until the owner accepts the return. The top maps `q_sheet_rsp_owed` only from this port; hierarchical reads of FIFO pointers/occupancy are forbidden.

The V2 boundary does not rely on the old leaf's response-without-ready assumption. AUX admission takes a credit which covers every arithmetic stage, held Sheet offer, issued-identity FIFO entry, response reservation, local-refusal entry, and terminal return until the owner accepts that return. A Sheet request is offered only when both its issued-identity entry and terminal-return capacity are reserved; every request and response payload holds while stalled.

Accepted Sheet requests enter an in-order identity FIFO. For its head, a response is legal only when `pg_op==READ`, `pg_src_id` exactly echoes `{2'b00,owner_handle14}`, and status is HIT or MISS:

* HIT commits typed `{status=0,tag,strength}`;
* MISS commits `{SOURCE_REFUSED,tag=0,strength=0}` and raises the sticky frame fault;
* ALLOCATED or OVERFLOW on a READ, wrong opcode, or wrong source ID is a Sheet protocol fault and commits one terminal `SOURCE_REFUSED` for the **owed FIFO head**, never success for the claimed token;
* a response with no issued FIFO head—including a duplicate after that head was consumed—is accepted into an always-draining protocol-fault sink and increments `sheet_rsp_unsolicited`; it cannot invent an owner or committed completion.

Thus every accepted issued read receives exactly one terminal disposition and no malformed response can park its owner. The Surface Sheet store's full handle lookup remains the generation authority: a stale/missing handle returns MISS. A degenerate envelope emits no Sheet request; the logical AUX issue is nevertheless notified to `zhao_texture_v3own` on expander acceptance, and a reserved local `{SOURCE_REFUSED,tag=0,strength=0}` return is presented no earlier than the following cycle. The same issue-before-return law applies to every other local AUX refusal.

Interleaved live owners may name sheet A/envelope A, sheet B/envelope B, then sheet A again while every stage is stalled independently. The required differential checks U/V, handle, op, source ID, status, tag, and strength for every accepted request and proves A's envelope cannot be paired with B's sheet. A committed slot-swap mutant updates descriptor data from the currently offered owner while retaining the prior owner token; the independent expected record is captured at admission and must fire. Separate stale-generation and same-index/new-generation cases prove that comparing only the patch index is insufficient. Wrong opcode, wrong status, and wrong source each fire their typed owed-response counter; duplicate and response-without-request both fire `sheet_rsp_unsolicited`. None may produce success.

The 64-bit low portion remains exactly `[31:0]=wx`, `[63:32]=wz`; the V2 AUX instantiation uses `.req_wx_i(aux_ctx.wx)` and `.req_wz_i(aux_ctx.wz)`. Raw slices are forbidden in new code. Successful tag/strength reaches the typed AUX owner plane described in section 3.5, where Packet B deliberately consumes status only; this architecture therefore does not claim that the visible terrain effect is connected.

### 5.2 Binner metadata

Because binner acceptance and drain are separated, the attribute/material packet must be stored under the binner's own accepted triangle identity. There are two safe forms:

1. a versioned binner with a parameterized opaque metadata plane written and read on the same `tri_wa`/`tri_ra` as its triangle entry; or
2. a descriptor bank addressed by a binner-exported triangle cookie allocated by the binner itself.

A wrapper-maintained rolling pointer is refused. It could drift on a triangle the binner declines or token-gates.

For the characterization module, `zhao_geom_binner_v2` should use the first form: a statically sliced metadata bank, an exact same-edge write, and an exact same-record drain. The old binner remains the oracle for tile-reference order and overflow behavior.

This does **not** settle the production binner scale. `TRI_CAP=128`, binning at five clocks per one-tile triangle, and frame-end-before-drain are not a legal sink for the full terrain producer described below. That wall must be solved before the final terrain composition; merely widening the metadata of the 128-entry experiment is not adoption.

## 6. Binding and palette composition

A connected V3 cannot use live global `bind_base_i/bind_mode_i` while multiple owners are in flight. Packet B removes those two fixture authorities from the versioned path and instantiates `zhao_texture_binding_resolver_v2`.

### 6.1 Frozen table ABI

Packet A supplies an eight-bit base selector, so the compatibility table has exactly **256 selectors**, two physical banks, and one 75-bit row per selector:

| bits | width | field |
|---|---:|---|
| `[31:0]` | 32 | texture base byte address |
| `[63:32]` | 32 | planner mode |
| `[65:64]` | 2 | palette slot |
| `[73:66]` | 8 | palette generation |
| `[74]` | 1 | valid |

Mode retains the existing planner encoding: format `[2:0]` (`CLUT8=0, RGB565=1, CLUT4=2, ARGB1555=3, ARGB4444=4`), filter `[3]`, wrap-U `[5:4]`, wrap-V `[7:6]` (`REPEAT=0, CLAMP=1, MIRROR=2`), log2 width `[11:8]`, log2 height `[15:12]`, max level `[19:16]`, mip enable `[20]`, reserved `[31:21]`.

A row may become valid only when base is 16-byte aligned, format is 0–4, both wraps are 0–2, reserved bits are zero, both log2 dimensions are at most the selected planner `MAXLOG2=11`, CLUT filter is zero, max level is no greater than `min(log2w,log2h)`, and—safe reversible canonical default—max level is zero when mip is disabled. A direct-colour row must store palette slot/generation as zero. There is no planner sanitization at this boundary: a row which violates any condition is refused by the loader and never marked valid.

Address validation uses the planner's exact packed-chain law. For final legal level `L` (`0` when mip is disabled), let `area_exp=log2w+log2h`, `REP4[0]=0`, and `REP4[L]=(4^L-1)/3` for `L>0`:

```text
level_offset_texel = (L==0) ? 0 : REP4[L] << (area_exp - 2*(L-1))
level_texels       = 1 << (area_exp - 2*L)
max_total_texel    = level_offset_texel + level_texels - 1
max_byte_offset    = 16bpp ? 2*max_total_texel + 1
                   : CLUT4 ? floor(max_total_texel/2)
                   :         max_total_texel
max_line_end       = (base + max_byte_offset) | 15
```

Every intermediate is widened; a carry beyond 32 bits rejects the row. Packet E additionally requires `[base,max_line_end]` to lie within `RENDER.ASSET_POOL`; MEM.GUARD remains the backstop.

Class is a function, never row data or fragment routing state:

```text
CLUT8 or CLUT4                         -> CLS_CLUT (0)
RGB565/ARGB1555/ARGB4444, filter == 0 -> CLS_NEAR (1)
RGB565/ARGB1555/ARGB4444, filter == 1 -> CLS_BIL  (2)
anything else                          -> local refusal; CLS_ERR (3) is never issued
```

### 6.2 Programming, CRC, seal, and activation

The GPU-domain command port is one ready/valid record:

```systemverilog
cfg_valid_i / cfg_ready_o
cfg_op_i[1:0]             // 0 BEGIN, 1 WRITE, 2 END, 3 ABORT
cfg_page_generation_i[7:0]
cfg_selector_i[7:0]
cfg_row_i[74:0]
cfg_crc32_i[31:0]

cfg_rsp_valid_o / cfg_rsp_ready_i
cfg_rsp_op_o[1:0]
cfg_rsp_status_o[3:0]     // 0 OK, 1 BAD_STATE, 2 BAD_GENERATION,
                           // 3 BAD_ROW, 4 DUP_SELECTOR, 5 BAD_CRC,
                           // 6 INTERNAL_PROTOCOL, 7..15 reserved
cfg_rsp_page_generation_o[7:0]
active_page_generation_o[7:0] // zero means no sealed active page

output logic cfg_loader_idle_o;      // 1 iff loader state is IDLE
output logic binding_crc_busy_o;     // 1 iff loader state is CRC_SCAN
output logic binding_seal_pending_o; // 1 iff loader state is SEAL_PENDING
```

These three resolver-V2 outputs are mandatory observation ports, reset respectively to `1,0,0`, and are combinational decodes of the registered loader state. They add no state, ready path, or lifecycle authority. `zhao_texture_island_v3_top` connects them to named top-local wires and never reads hierarchical private child state.

All command fields hold through `cfg_valid && !cfg_ready`; every accepted command produces exactly one held response before another command is accepted. Irrelevant fields for an opcode are required zero and otherwise return BAD_ROW. The state machine is exact:

* reset clears both 256-bit validity masks, sets active generation zero, and leaves row RAM unwritten;
* BEGIN is legal only while loader IDLE, chooses the inactive bank, requires a nonzero generation different from the current active generation, clears only the staging validity mask, and enters LOADING;
* WRITE is legal only in LOADING with the matching staging generation, `row.valid==1`, a legal canonical row, and a selector not already written in this load; it writes one staging row and sets that selector's validity. Invalid selectors are represented by never writing them after BEGIN, not by writing stale payload with valid zero;
* ABORT is legal only in LOADING with the matching staging generation; it discards the staging validity mask, returns OK, and enters IDLE without changing active bank/generation;
* END in LOADING with the matching generation scans the canonical staging image and compares CRC. While scanning, command ready is low and the old active page may continue serving admitted traffic. BAD_CRC discards the staging validity mask, returns BAD_CRC, and leaves the active page untouched;
* a successful END enters `SEAL_PENDING`, withholds new fragment admission, permits no further config command, drains all prior island work, and atomically changes active bank plus active generation on the first `data_quiet` edge. The delayed END response is then `OK` for that now-active generation. Activation never occurs merely because owner quiet is high.

CRC is deterministic and independent of WRITE order. It is CRC-32/ISO-HDLC: reflected polynomial `32'hEDB88320`, init `32'hFFFFFFFF`, xorout `32'hFFFFFFFF`, refin/refout true. The byte stream is the one-byte page generation followed by selectors 0 through 255. Each selector contributes ten bytes, least-significant byte first, from `{5'b0,row[74:0]}`; an invalid selector contributes ten zero bytes regardless of stale RAM contents. The CRC field itself is not in the stream.

An eight-bit page generation is frame-local coherence, not resource identity. It uses 1..255 and may wrap 255→1 only through the same complete-drain activation fence; no descriptor survives that fence. Before selectors are emitted, upstream `MATERIAL.RESOLVE` must still validate every material sample's full resource generation (16 bits where that contract requires it) and build the page. Narrowing that upstream comparison to eight bits is forbidden.

Reset intentionally has no active page. Fragments may still be admitted before a seal so they terminate visibly: their captured page generation is zero and every declared sample is locally refused. A bad update cannot corrupt the previous active page. Mid-frame active-bank writes do not exist.

### 6.3 Lookup, selector overflow, and issue timing

For sample index `i`, the expander computes widening arithmetic:

```text
selector9 = {1'b0,base_selector8} + i
```

If `selector9[8]` is set, that sample is marked for local refusal and the low eight bits are never used as a modulo-256 table address. The resolver front end still accepts one held logical-job record:

```text
sample_handle[15:0]
captured_page_generation[7:0]
selector_overflow[0:0], force_refuse[0:0], binding_selector[7:0]
// selector low bits are ignored when either refusal bit is set
U[31:0], V[31:0], LOD_Q4_4[7:0]
sample-0 class/palette witnesses carried from the descriptor
```

The accepted output to the planner is:

```text
route_token = {derived_class[1:0],sample_handle[15:0]} // 18 bits
base[31:0], mode[31:0]
palette_slot[1:0], palette_generation[7:0]
U[31:0], V[31:0], LOD_Q4_4[7:0]
```

The resolver has a held one-read elastic stage and a reserved disposition slot, so it can accept one request per clock when downstream capacity exists. `force_refuse` is set only for the admission-frozen malformed-material path and suppresses table/planner/cache access. **The front-end handshake is the logical sample issue**: on the same edge it pulses `iss_tmu_valid_i` with the full handle into `zhao_texture_v3own` and increments `SJ_accepted`. A valid resolved row later increments `SJ_planner_accept` only on planner handshake. Selector overflow, forced refusal, page generation zero/mismatch, invalid row, illegal row bits, or sample-0 witness mismatch enters the local-refusal output and increments `SJ_local_refused` only when that terminal return is accepted by the owner.

A local return may be presented no earlier than cycle N+1 after its cycle-N logical issue. It carries `{status=SOURCE_REFUSED,index=0,alpha=8'hFF,RGB=24'hFF00FF}` with the exact sample handle and sets the sticky binding/frame fault. Input ready is withheld unless capacity for that later terminal disposition is already reserved. Issue and return on the same edge, return before the owner saw issue, silent drop, wraparound lookup, and waiting forever on an invalid row are all forbidden.

The `captured_page_generation` byte at this front end must be copied bit-for-bit from `joined365.logical287[286:279]`; there is no generation sidecar on either V2-join input or output, and the resolver may not reread the current page to populate the job. It is compared with the active page at resolver completion as an independent detector as well as an enforcement condition. The held job byte and the active-page register have different enables—descriptor admission/join transport versus atomic activation. Under the activation fence they cannot differ legally; the stalled A/B join controls and a committed early-activation mutant are therefore required to prove both the carriage and detector can fire.

### 6.4 Remove class/palette dual authority

Packet A has one fragment-level response class and palette tuple but can request three samples. Packet B treats those fields as **sample-0 compatibility witnesses only**:

* resolver-derived class and row palette fields are the sole functional values for routing and lookup;
* when count is nonzero, sample 0 compares all three witnesses with its resolved row before planner issue; mismatch locally refuses sample 0 and raises typed malformed-binding/frame fault;
* samples 1 and 2 use only their own resolved rows because Packet A contains no witness for them;
* at count zero all class/palette witness bits must be canonical zero and no lookup occurs;
* direct rows require zero palette fields, so their witnesses are checked against zero rather than ignored.

No multiplexer may select between witness and resolved values. The eventual explicit `MaterialSample[3]` ABI remains upstream; common U/V and LOD plus consecutive selectors are only the Packet-A frame-local seam. Before the connected shell enables sample counts 2/3, the material differential must either prove that convention for every shipped record or a later packet must widen the request to explicit per-sample selector/UV/LOD fields. That product limitation does not weaken the deterministic behavior of Packet B itself.

Palette loads and binding activation are both GPU-domain frame-sealed operations. An admitted owner uses the palette generation from its resolved binding row. A stale/cold palette response is a typed sample refusal, not usable RGB, and no current fragment witness or response-token slice may replace the resolved tuple.

## 7. Memory: share ENGINE1, do not spend client 5

### 7.1 Region law

Ratify the existing bank-3 read-only range as `RENDER.ASSET_POOL`:

```text
base       0x06A0_0000
half-open end 0x0800_0000
span       0x0160_0000
permissions ENGINE1 read-only
contents   geometry descriptors/assets and immutable texture/mip data
```

This allocates no new address and promises no capacity saving. `ZHAO_GEOM_ASSET_BASE/SPAN` can remain temporary aliases while geometry users migrate; the guard has one canonical render-asset predicate. Texture binding sealing must prove every possible line touched by base/mode/mip lies inside the region, and MEM.GUARD remains the deny-by-default backstop.

### 7.2 Local request arbiter and the guard's two-event protocol

Add `zhao_render_asset_mux.sv` before the one ENGINE1 guard. Its local owners are `GEOMETRY` and `TEXTURE_FILL`; they are not global client IDs. It arbitrates only at whole-request boundaries, round-robin when both are waiting, and permits one ENGINE1 request outstanding, matching the global arbiter's rule.

The texture translation is exact:

```text
write  = 0
client = ENGINE1
addr   = fill_addr (must be 16-byte aligned)
len    = 16 bytes
```

Geometry retains its legal 32-byte and 64-byte requests.

MEM.GUARD acceptance and verdict are two different events:

```text
guard_accept = guard_req.valid && guard_rsp.ready        // request captured
verdict_ok   = guard_rsp.ok                              // one-cycle pulse later
verdict_deny = guard_rsp.violation                       // mutually exclusive pulse later
```

`guard_rsp.ready` is the guard's ability to capture a request, not permission to infer the verdict. In particular, a denied request leaves the forward stage empty, so `ready` may still be high during its registered violation pulse. A master which holds `valid` until `ok || violation` submits the same denied request again.

The local mux therefore uses this exact FSM:

```text
IDLE/OFFER:
  select one held local request; hold all fields stable until guard_accept
  on guard_accept: capture {subowner,len,address}; DROP guard_req.valid; -> WAIT_VERDICT
WAIT_VERDICT:
  present no guard request
  require exactly one of {verdict_ok,verdict_deny}
  verdict_ok   -> WAIT_DATA, retaining captured {subowner,len}
  verdict_deny -> emit exactly one typed local refusal, release capture -> IDLE
WAIT_DATA:
  route exactly len/2 raw halfwords to the captured subowner
  release only on the final expected halfword -> IDLE
```

No next local request is offered in `WAIT_VERDICT` even if guard `ready` is high. A verdict without one accepted request, two verdicts for one accept, both verdict bits together, or a second accept before the first verdict is a sticky mux/guard protocol fault.

For a legal request the guard itself retains the forwarded request until global arbiter grant. The local mux's subowner lifetime extends through all returned words; it does not release at `verdict_ok` or global grant. It must not infer destination from response data or a later address.

### 7.3 Return routing before packing

The SDR controller's raw 16-bit return goes first to the destination selected by the captured global client and ENGINE1 subowner:

* SCANOUT: pack four words to the existing 64-bit scan beat;
* ENGINE1/GEOMETRY: pack four words to a 64-bit geometry beat; mark geometry last from captured `len/8` (four beats for 32 bytes, eight for 64);
* ENGINE1/TEXTURE_FILL: pass each raw 16-bit word directly to V3; exactly eight ascending beats, with the eighth releasing the local request.

The texture fill must not travel through the 64-bit packer and be unpacked later. That would add an unnecessary holding protocol and obscure the exact eight-beat contract.

### 7.4 Guard refusal must terminate

Today's cache fill interface has no refusal response. If an accepted guard request receives a denial verdict, simply withholding data parks the only blocking miss and eventually the island head forever. Therefore the versioned cache/adapter boundary adds one terminal `fill_refused_i` pulse mutually exclusive with the eight data beats.

On `guard_accept`, the cache/mux request is no longer offered; it waits for the one verdict. On `verdict_deny`, the mux emits exactly one `fill_refused_i` for a captured `TEXTURE_FILL` request. The cache invalidates any partial line, clears its one outstanding fill, increments `fill_jobs_completed` and `fill_jobs_refused` once, and returns a sample result with `SOURCE_REFUSED` and the original route token. V3 commits that terminal response and drains the refused fragment through its normal ordered release; the shell latches the frame fault and, only after complete writer drain, releases rather than publishes the framebuffer lease. Geometry receives its separately typed request refusal; it is never presented as an empty 64-bit beat.

For `verdict_ok`, exactly eight data beats complete a texture fill. A malformed short fill, ninth beat, data without an accepted-and-approved fill, response sent to the wrong subowner, or completion without a start is a sticky memory fault.

The denial replay positive control holds a denied request's source `valid` high through the cycle before `guard_accept`, then verifies the local mux drops its guard-facing valid immediately and observes exactly one accept, one violation verdict, one local refusal, and one guard-violation increment. A committed mutant keeps guard-facing valid asserted until the verdict; because guard ready remains high on denial, it re-accepts the same request and must fail the exact accept/verdict/refusal counts. This is separate from the ordinary stall control, which holds valid and every field stable only while `!guard_rsp.ready`.

Client 5 remains tied off. A later measured ENGINE1 contention result may reopen that owner ruling; architecture prose is not that evidence.

## 8. Framebuffer lease, CDC, and frame completion

### 8.1 Writer-aware GPU-domain lease contract

The old shell's provisional external `fb_writer_i` cannot remain an authority in shell V2. Both framebuffer write guards must derive ownership from one GPU-domain record accepted by a versioned `zhao_video_slotmgr_v2`; the existing slot manager and old shell remain unchanged. Writer identity is frozen as:

```text
FBW_BLITTER  = 1'b0
FBW_RENDERER = 1'b1
lease key    = {writer, slot[0:0], generation[15:0]}
```

There are two request channels, one physically owned by each writer. Their payload is the canvas mode; writer is stamped by the arbiter rather than trusted from request data. Each source holds `req_valid` and mode stable until its `req_valid && req_ready`. When both are valid, a retained round-robin bit chooses one, only that channel sees ready, and the bit changes only on acceptance. There is still at most one WRITING lease globally.

A request may be accepted only when there is no live lease or held grant and at least one slot is FREE. On that edge the manager deterministically chooses a FREE slot, increments that slot's existing 16-bit generation modulo 65,536, changes it to WRITING, and captures this immutable live record:

```text
{valid, started, writer, slot, generation, mode, fb_base, fb_span, fault}
```

It then holds one registered grant `{writer,slot,generation,mode,fb_base,fb_span}` until the selected writer accepts it. `started` becomes one only on `grant_valid && grant_ready`; neither writer may issue a framebuffer request before accepting its grant. A request acceptance without exactly one later grant acceptance, a grant accepted by the wrong writer, or a write before `started` is a sticky lease fault.

Base and span are derived, never caller supplied:

```text
slot 0 base = ZHAO_FB_SLOT0_BASE = 32'h0000_0000
slot 1 base = ZHAO_FB_SLOT1_BASE = 32'h0200_0000
span        = zhao_canvas_bytes(captured_mode)
require 0 < span && span <= ZHAO_FB_SLOT_SPAN (32'h0003_C000)
window      = [base, base + span)
```

The allocation span remains `ZHAO_FB_SLOT_SPAN`; the guard window is the captured mode's actual occupancy. Renderer and blitter form absolute addresses from the captured base and relative offsets. Both write guards compare the requester against `live.writer` and the address/length against the same captured window. Shell V2 removes `fb_writer_i` and renderer framebuffer base/span pins as ownership inputs; the byte-identical old shell retains them only in the historical interface. Stride is derived from the same captured canvas mode, so a caller cannot pair one mode's span with another mode's addressing.

The slot lifecycle is exact:

```text
FREE      --lease request accepted/new generation--> WRITING
WRITING   --matching clean publication accepted----> READY
WRITING   --matching drained fault/cancel release---> FREE
READY     --matching display-swap event accepted----> DISPLAYED
DISPLAYED --a different matching READY swap accepted> FREE
```

No other transition is legal outside the coordinated reset in section 8.4.

### 8.2 Terminal events: clean publication versus fault release

Every fatal renderer or blitter event while a lease is live sets `live.fault` under that lease key. The writer may offer a terminal event only after its source, write guard, global arbiter requests, SDR writes, and local output queues have drained and its issued/retired counts agree. The held terminal payload is `{publish,release,writer,slot,generation}` with exactly one outcome bit set; it is stable until accepted.

An event capture is `term_valid && term_ready`. The manager compares all of `{writer,slot,generation}` against the live record and also requires `started` and the writer's independently computed drain witness. The consequences are:

* exact key, drained, `kind=PUBLISH`, and `live.fault==0`: accept a clean publication, change WRITING to READY, and atomically enqueue the ready event described below;
* exact key, drained, `kind=RELEASE`: change WRITING to FREE and enqueue no ready event; this is the required terminal path for a faulted frame and is also the safe explicit cancel path;
* exact key, drained, `kind=PUBLISH`, but `live.fault==1`: safety-demote the event to a fault release, count `publish_on_fault`, change WRITING to FREE, and enqueue no ready event;
* any accepted writer/slot/generation mismatch, duplicate terminal event, or event before grant acceptance: change no slot or live-lease state and increment its independently typed stale/protocol counter. An exact matching event offered before drain sets the sticky early-terminal fault once, sees `term_ready=0`, and holds its complete payload until drain; it is not consumed or counted repeatedly while held.

Release has safety priority if one broken writer asserts publish and release together; exactly one release may be accepted, publication cannot enter READY, and the simultaneous-terminal counter fires. A terminal event from the non-owner is consumed as stale only when the terminal checker has capacity, never interpreted as authority. There is no path from a texture/material/memory fault to a publication pulse.

Generation matching preserves the existing 16-bit VIDEO.SLOTMGR generation in full and writer is part of the key. Matching slot alone, narrowing generation, matching `{slot,generation}` while ignoring writer, or reading a current global writer selector is forbidden. No anti-ABA property is re-proved under a narrower generation. After normal drain:

```text
lease_requests_accepted == grants_accepted
leases_started == clean_publications + fault_or_cancel_releases
```

with a live/held grant represented explicitly when the system is sampled before drain.

### 8.3 Accepted-ready CDC and frame repeat

Only the **accepted clean-publication state transition** may cross from `gpu_clk` to `vid_clk`. Raw `renderer_done`, `blitter_done`, terminal `valid`, and fault release never cross and never toggle readiness.

The crossing is a dual-clock ready-event FIFO carrying the complete `{writer,slot,generation}`. Its GPU write acceptance is the same edge as the WRITING-to-READY transition; therefore publication `term_ready` is withheld while that FIFO cannot accept. The manager may not first mark READY and promise to send the event later. Payload is stored in the FIFO, not placed beside a synchronized pulse.

The video side pops one event into a held pending register. At a legal vertical frame boundary it swaps only if a pending event exists **and** the reverse swap-event FIFO can accept `{writer,slot,generation}` on that same edge. It then scans the new slot and echoes the unchanged tuple to GPU. If no clean ready event is pending, it performs no swap, emits no swap event, frees no slot, and repeats the currently DISPLAYED frame. A refused renderer frame therefore causes an ordinary repeat rather than a partial-frame display.

On the GPU side, a swap event changes state only if its complete tuple names a READY slot with matching writer and generation. On acceptance, that slot becomes DISPLAYED and the previously DISPLAYED slot, if any and different, becomes FREE. A stale, duplicate, wrong-writer, wrong-generation, or non-READY swap event changes no state and increments a typed counter. The tuple stored with the READY slot and the FIFO-returned tuple have different enables, so a CDC payload defect cannot move both operands together.

Required drained equalities are:

```text
clean_publications == ready_fifo_gpu_writes
ready_fifo_vid_reads + ready_fifo_occupancy == ready_fifo_gpu_writes
video_swaps == swap_fifo_vid_writes
swap_fifo_gpu_reads + swap_fifo_occupancy == swap_fifo_vid_writes
accepted_matching_swaps == displayed_transitions
fault_or_cancel_releases produce zero ready_fifo_gpu_writes
```

A pending video event is included on the left as FIFO-consumed but not yet swapped; gates report FIFO and pending occupancy separately rather than forcing an equality at an arbitrary frame phase.

### 8.4 Reset-in-flight contract

A one-domain reset must not make a displayed slot look FREE. Reset therefore enters a cross-domain barrier: stop new leases and writes; flush both event FIFOs and the video pending register; command scanout to the reset/blank source; wait for synchronized `gpu_reset_done` and `vid_blank_done`; only then initialize both slots FREE with generation zero and reopen lease arbitration. If either domain resets while the other is live, the same barrier is entered. Generation zero is not reused while video may still scan an old generation.

No pre-reset ready or swap event survives the FIFO reset, no in-flight writer is published, and no lease is granted between local reset release and barrier completion. The reset test strikes every point—held request, held grant, WRITING, publication backpressure, ready FIFO, video pending, and swap return—and proves that the first post-barrier display comes only from a post-barrier clean publication. Ordinary no-ready operation outside reset continues to repeat the previous complete frame.

### 8.5 Domain placement and render completion

Raster, V3, binding, palette, AUX, lease arbitration, local ENGINE1 arbitration, both write guards, MEM.GUARD, the global arbiter, and SDR control remain on `gpu_clk`. Palette/binding uploads originating in the HPS/command path become GPU-domain ready/valid commands before reaching V3. An AUX sheet provider in another domain requires explicit request and response asynchronous FIFOs carrying the complete token and payload; a synchronized valid bit beside unsynchronized U/V/data is forbidden. If the provider is GPU-domain, no CDC block is added.

The renderer's drain witness is:

```text
binner/source drain complete
AND attribute/raster front end empty
AND V3 quiet and no fill/AUX response outstanding
AND RASTER.FRAGMENT idle
AND final tile resolve accepted by FBWRITE
AND FBWRITE issued words == retired words
```

After that witness, a fault-free lease offers PUBLISH and a faulted lease offers RELEASE. The existing `render_drain_done_o` may remain a binner milestone for observation, but is neither terminal authority nor a CDC source.

D3's three clock constraints and domain-local signature sinks remain the measurement method. The new generated fit wrapper gets its own interface manifest and source hash; the old wrapper is not silently regenerated to point at a different shell.

## 9. Throughput budget

### 9.1 Per-block obligations

The seam must not weaken an existing one-per-clock contract:

| boundary | obligation |
|---|---|
| coverage expansion | one covered fragment offered per clock after a row is loaded |
| Early-Z | one accepted candidate per clock when its output storage is available |
| candidate skid | one accepted and one retired beat per clock |
| V3 owner admission | one owner per clock while credit and reciprocal ingress are available |
| V3 ordered edge | one result per clock when consecutive heads are complete and the consumer is ready |
| binding resolver / TMU planner edge | one accepted sample job per clock when downstream is ready |
| AUX offer and return queues | one accepted job and one terminal return per clock under their documented priority/credit law |
| texture cache hit edge | one access per clock; one blocking miss at a time |
| material combiner engine | one paired phase per clock, not one multi-phase job per clock |
| `RASTER.FRAGMENT` fast path | one accepted fragment per clock where its documented hazards permit |
| TILESTORE | one access per port per clock |
| RESOLVE | one pixel per clock, overlapping the next front-bank tile |

The adapter is required to add zero bubbles to V3 admission and ordered output. It does **not** follow that the whole textured path sustains one fragment per clock.

The selected V3 reciprocal point is `NCTX=12`; its existing source evidence reports 4.38 clocks per reciprocal for the accepted tile point. Sample counts 2/3 create multiple cache/planner requests per fragment, and every cache miss adds eight SDR beats. `RASTER.ATTRSTEP` adds seed/reseed bubbles. These are workload costs, not violations of the one-beat interfaces around them.

The owner deliberately has one conservative full-to-free admission bubble. Do not “repair” that with an output-credit combinational bypass. The no-bubble requirement applies to an already-filled ordered output stream and ordinary non-full admission, not to this documented wrap/full fence.

### 9.2 Accepted/completed job and phase accounting

Owner-edge bubble tests are necessary but cannot detect duplicated sampler or combiner work. Every counter below increments on its named acceptance or terminal event, never on `valid` alone:

```text
C                 covered fragments accepted by Early-Z input
S / F             Early-Z survivors admitted to V3 / ordered outputs accepted
SJ_required       sum of admitted sample_count values
SJ_accepted       logical sample jobs accepted from the owner expander
SJ_planner_accept resolved planner/TMU request handshakes
SJ_local_refused  accepted jobs terminated before planner acceptance
SJ_completed      full-identity sample terminal returns committed by the owner
OWN_TMU_COMMIT     instrumentation-only owner sample-commit count
AJ_required       sum of admitted aux_required bits
AJ_accepted       logical AUX jobs accepted from the owner expander
AJ_sheet_accept   Surface Sheet request handshakes
AJ_local_refused  accepted AUX jobs terminated before a sheet request
AJ_completed      full-identity AUX terminal returns committed by the owner
OWN_AUX_COMMIT     instrumentation-only owner AUX-commit count
OWN_ALL_COMMIT     retained compatibility sum of the two owner commit counts
CA / CC           texture-cache access jobs accepted / completed
FI / FOK / FREF   fill jobs accepted / completed with eight beats / refused
FB                accepted fill data halfwords
CJ / CD           combiner jobs accepted / completed
PI / PC           combiner phases issued / completed
O                 ordered owner outputs accepted and released
D                 attribute seed and reseed divides accepted
W_issue/W_retire  framebuffer words issued / retired
GA/GOK/GDENY      ENGINE1 guard requests accepted / OK verdicts / deny verdicts
```

`SJ_local_refused` includes selector overflow, absent/mismatched page, invalid row, witness mismatch, and malformed-material sample jobs terminated without planner access. `AJ_local_refused` includes malformed-material AUX jobs and degenerate-envelope jobs terminated without a Sheet read; a stale/missing sealed handle instead counts one Sheet acceptance and one MISS completion. Stale, duplicate, out-of-range, pre-issue, or unrequested owner returns increment only their protocol-fault counters; they do not increment `SJ_completed` or `AJ_completed` because they did not satisfy an owner plane.

The present owner exposes only `ev_commits_o`, which combines TMU and AUX commits and therefore cannot prove either equality independently. Packet B permits the following **instrumentation-only** additions to `zhao_texture_v3own`:

```systemverilog
output logic [31:0] ev_tmu_commits_o; // increments exactly when c4t_v_q is true
output logic [31:0] ev_aux_commits_o; // increments exactly when c4a_v_q is true
// existing ev_commits_o remains their cycle-by-cycle compatibility sum
```

A cycle committing both paths increments each typed count by one and `ev_commits_o` by two. All three 32-bit counters wrap modulo `2^32`; acceptance windows are bounded below wrap. These ports do not gate or alter claim, issue, commit, ticket creation, final acceptance, emission, or release. Their independent positive control commits one TMU and one AUX return in the same cycle, then separately commits only each kind; a swapped-source and a combined-only mutant must fail.

After complete drain, the exact closure is:

```text
S == F == owner_admitted == owner_emitted == owner_released == O
SJ_required == SJ_accepted
SJ_accepted == SJ_planner_accept + SJ_local_refused
SJ_completed == SJ_accepted
SJ_completed == OWN_TMU_COMMIT
AJ_required == AJ_accepted
AJ_accepted == AJ_sheet_accept + AJ_local_refused
AJ_completed == AJ_accepted
AJ_completed == OWN_AUX_COMMIT
OWN_ALL_COMMIT == OWN_TMU_COMMIT + OWN_AUX_COMMIT
CA == CC
FI == FOK + FREF
FB == 8*FOK
CJ == CD == S
PI == PC == phase_demand
GA == GOK + GDENY
W_issue == W_retire                    // required before publish
```

Every one of those is also partitioned by success/refusal and, where applicable, owner class. A locally refused job counts once as accepted and once as completed, not as a phantom cache/sheet acceptance. An accepted cache hit completes one cache job without a fill; an accepted miss creates exactly one fill job. A denied texture fill contributes one `FREF`, zero `FB`, and one cache-job completion. The guard equality is evaluated only with no request in `WAIT_VERDICT`; counters additionally prove no acceptance receives two verdicts and no verdict occurs without the captured acceptance.

For the current one-physical paired-phase combiner, accepted jobs are classified exactly once:

```text
J1 = PASSTHRU(count 0/1), ADD_SAT, MASK, or any early refused/malformed job
J2 = valid MODULATE, MODULATE2X, LERP, or TERRAIN_DETAIL_MASK job
J3 = valid TERRAIN_DETAIL_LIGHT job
phase_demand = J1 + 2*J2 + 3*J3
CJ = J1 + J2 + J3
```

The engine may issue at most one paired phase per clock. A long all-ready, independently backlogged run must issue one phase on every post-fill clock until the runnable phase set drains, and `PI`/`PC` must equal the equation rather than merely produce the right colors. Consequently a homogeneous one-phase stream may demonstrate one completed material job per clock after pipeline fill; a two-phase stream consumes two issue clocks per job and a three-phase stream consumes three. **No one-material-job-per-clock claim is legal for the multi-phase recipes unless a later measured architecture adds sufficient physical phase engines and updates this equation.**

Cadence gates run long homogeneous and mixed streams for sample counts 0, 1, 2, and 3; AUX absent/present; cache all-hit, legal fill, and denied fill; and every recipe class. They report `SJ/AJ/CA/FI/CJ/PI` accepted and completed deltas over the steady-state window, plus pipeline fill/drain clocks. This catches duplicate idempotent microjobs and hidden service bubbles that an ordered-output edge test or byte-identical frame cannot see.

Frame acceptance reports elapsed GPU clocks, stall clocks at every service, high-water marks, memory credits, lease contention, and every counter above. It is not computed by adding standalone initiation intervals.

### 9.3 Legal two-view terrain load

The headline terrain stimulus is the legal nonempty `view_mask=2'b11` case with distinct matrices/viewports and the current structural direction `ROWS_PER_PASS=3`, `MATW=18`. The dense count remains explicit:

```text
2 views * 256 patches * 16 subpatches * 81 fills = 663,552 fills/frame
```

The RPP1/II3 point costs:

```text
663,552 * 3 = 1,990,656 clocks
```

which exceeds the approximately 1,666,666-clock 100-MHz/60-Hz compute frame and is not a legal headline. No fit or throughput claim may substitute it for RPP3.

The terrain composition also reports an all-level-0 two-view producer demand of 1,048,576 triangles/frame, while the current binner holds 128 and consumes five clocks per one-tile triangle before drain. Therefore a 128-triangle synthetic G8A raster fit is useful subsystem evidence but not evidence that the legal terrain frame reaches it. The production-scale tile-list/scheduling wall must close first.

Texture fragment/sample counts are not inferred from the 663,552 producer fills. The real two-view trace must measure covered pixels, Early-Z survival, sample-count distribution, cache misses, and AUX demand. The known three-sample material subtotal and older 276,480-fragment estimate remain workload references, not replacements for that trace.

## 10. Keep the one shared terrain/projector path

The final hierarchy must contain the existing `zhao_terrain_pipe` shape:

```text
one zhao_terrain_tess
one zhao_terrain_group_seq
one zhao_project_service
  -> exactly one zhao_project_core shared by geometry client A and terrain client B
three physical projected-arena copies for simultaneous corner reads
```

Its selected-view outputs are serialized into the common clip/setup/attribute/binner/raster path. `out_view_o` remains explicit through viewport/scissor selection and the source/sequence witness. There is one binner/raster/V3 service after the merged stream, not one per view, and no second projector is introduced to make texture integration easier.

The terrain pipe is still a candidate, not an adopted producer. Its own report leaves normals, depth quantization from the carried `w`, and the physical fit open. It also does not currently return the world-space fragment context needed by AUX. These must close before the final shell switch:

1. provide/reuse world normals without a third ModeTri pass that breaks the two-view schedule;
2. carry projected `w` into `GEOM.DEPTHQUANT` and produce the exact `invw24` attribute;
3. carry the vertex attributes/material record needed by ATTRSETUP, including UV and AUX world context;
4. solve the binner/tile scheduling capacity wall without duplicating the projector or texture island;
5. exercise legal masks `01`, `10`, and `11`, with `11` the headline.

Until then, the raster/V3 seam can be complete and fitted as a subsystem, but the full terrain-to-framebuffer path is not installed.

## 11. Resource accounting and fit boundaries

### 11.1 What may not be added or subtracted

The historical shell receipt reporting 12,707 ALMs, 16 DSPs, and 3,214 virtual pins describes the old flat-texel shell at its recorded commit. It is not the new D3 real-pin result and excludes V3.

A standalone V3 fit has different virtual pins, pruning, register endpoints, packing, and no shell sharing. Adding its ALMs or DSPs to the shell row does not produce a connected design number. Likewise, “ALMs containing virtual pins” cannot be subtracted linearly.

The clean TEXJOIN row—3,824 ALMs, 7,151 registers, 4 M10Ks, 0 DSP, 93.12 MHz—is valid evidence about that standalone module. It is not a block physically present in the current shell. Removing its selected manifest root decreases a mixed accounting census but removes no demonstrated connected silicon. **No 3,824-ALM physical saving is booked.**

Source-list membership in `zhao_prod_top` is not connectivity. The only acceptable total is one elaborated connected hierarchy from one clean committed source state.

### 11.2 Necessary fit gates only

Do not fit each packet or each arithmetic nodule. Pre-register only these new questions:

**G8A — `g8a_raster_texture_single_owner_characterization`**

Top: a generated real-pin/MISR wrapper around `zhao_raster_tile_pipe_v2`, including EDGEWALK, attribute candidate production at the selected parameters, Early-Z, the widened skid, one V3 island, `RASTER.FRAGMENT`, TILESTORE, RESOLVE, and legal cache/AUX responders.

Questions:

* does the elaborated connected hierarchy contain exactly one lifecycle owner and no TEXJOIN;
* do the widened context/result banks infer as intended;
* does the connected path meet 100 MHz;
* what are connected ALM/register/M10K/DSP totals and entity attribution;
* did the adapter add any bubble or critical path which simulation could not answer?

This is the already named ownership gate made concrete. It runs only after every simulation and mutant gate below passes.

**G8B — `terrain_pipe_rpp3_matw18_characterization`**

Top: `zhao_terrain_pipe_rpp3_matw18_fit_top`, a dedicated registered-pin/MISR wrapper which instantiates exactly:

```systemverilog
zhao_terrain_pipe #(
  .ROWS_PER_PASS(3),
  .MATW(18)
) u_terrain_pipe (...);
```

The wrapper's legal driver exercises nonempty masks `2'b01`, `2'b10`, and headline `2'b11` with distinct matrices/viewports; the receipt workload is the dense `2'b11` case. It must not rely on `zhao_terrain_pipe`'s default `MATW=32`, a fit-target comment, or a runtime convention to establish the parameters. The target source closure names the wrapper rather than raw `zhao_terrain_pipe` as the top.

G8B runs once, only after the terrain normals and depth-quantization prerequisites close and all non-fit RPP3/MATW18 tests pass. Its receipt gate requires a clean committed source/digest, zero virtual pins, exact elaborated parameter values `ROWS_PER_PASS=3` and `MATW=18`, legal-mask activity witness, one shared projector core, timing, hierarchy, and actual ALM/register/M10K/DSP totals. Any area, DSP, or Fmax result is evidence for that exact wrapper only; no saving relative to the ambiguous raw/default target is assumed.

**G8C — `g8c_two_view_shell_texture_composed_characterization`**

Top: a new D3-style generated real-pin wrapper around one connected hierarchy containing `zhao_shell_top_v2`, the real ENGINE1 geometry/texture memory share, one V3, and the one shared terrain/projector service. The wrapper drives a legal two-view transaction and consumes every output through registered domain-local sinks. It has its own exact port manifest, activity smoke test, post-map witness, and source digest.

Questions:

* does the actual connected render hierarchy fit with **comfortable margin below 30,000 ALMs and 85 DSPs**;
* does it close the GPU clock at 100 MHz and retain the declared video/audio clocks;
* are there exactly one `zhao_project_core`, one V3 island, one `zhao_texture_v3own`, and zero TEXJOIN instances;
* are the expected RAMs inferred and are wrapper versus shell entities separately attributable;
* does the clean committed receipt correspond to the same source used by the legal two-view simulation?

A shell-only post-change fit is not added between G8A and G8C unless G8A identifies a shell-specific timing question which G8C cannot diagnose. The old D3 fit is the pre-change baseline; it is not rerun after its specimen changes because its specimen does not change.

Every fit receipt must state `rtlCleanAtHead`, source commit and digest, device/tool/seed, parameters, top, zero virtual pins, rule violations, hierarchy census, timing clocks, and raw-report paths. A labelled row with an empty rule list is not by itself compliance.

The final margin target is a gate, not a forecast. No numeric ALM, DSP, M10K, or MHz saving is predicted by this report.

## 12. Verification gates and positive controls

Each packet's non-fit gates below precede that packet's named subsystem fit: sections 12.1–12.8 precede G8A where applicable, section 12.9 precedes shell-V2/final composition, and section 12.10 precedes G8B and G8C. No fit substitutes for a missing simulation or mutant gate.

### 12.1 Interface, source-order, and ownership gates

The Packet-B interface artifact is exactly:

```text
manifest  fpga/rtl/generated/zhao_texture_island_v3_top.interface.json
generator tools/rtl/gen_texture_v3_interface_manifest.py
parser    tools/rtl/texture_v3_interface_parser.py
checker        tools/rtl/check_texture_v3_interface_manifest.py
schema_id      zhao.texture.interface
schema_version 1
```

#### Closed JSON schema v1

The manifest is not an open-ended JSON document. Its root has **exactly** these members and JSON types; every nested object likewise rejects an extra or missing member, and `null` and floating-point numbers are forbidden everywhere:

| object | exact members and JSON types |
|---|---|
| root | `elaboration` object; `hashes` object; `module` object; `parameters` array; `ports` array; `schema_id` string; `schema_version` integer; `source_closure` array; `tools` object |
| `elaboration` | `argv` array of strings; `cwd` string, exactly `"."`; `parameter_overrides` array; `top_module` string |
| `parameter_overrides[]` | `name` string; `value` object |
| parameter `value` and `parameters[].selected_value` | `kind` string, one of `unsigned_integer`, `signed_integer`, `bit_vector`, `string`; `text` string in canonical SystemVerilog form, with integers represented in base-10 and bit vectors as width-qualified lowercase hexadecimal with no X/Z |
| `hashes` | `canonical_interface_sha256` string; `module_declaration_sha256` string; `top_source_sha256` string; each exactly 64 lowercase hexadecimal characters |
| `module` | `declaration_end_byte_exclusive` integer; `declaration_start_byte` integer; `name` string; `source_path` string |
| `parameters[]` | `declared_kind` string; `declared_type` string; `name` string; `ordinal` integer; `selected_value` object; `source_default_expression` string |
| `ports[]` | `bit_width` integer; `declared_type` string; `direction` string (`input`,`output`,`inout`); `element_width` integer; `name` string; `net_or_var` string (`net`,`variable`); `ordinal` integer; `packed_dimensions` array; `signed` Boolean; `source_expression` string; `unpacked_dimensions` array; `unpacked_element_count` integer |
| either dimensions array element | `direction` string (`ascending`,`descending`); `left` integer; `right` integer; `size` integer; `source_expression` string |
| `source_closure[]` | `kind` string (`systemverilog_package`,`systemverilog_module`); `ordinal` integer; `path` string; `sha256` string of 64 lowercase hex characters |
| `tools` | `checker`, `elaborator`, `generator`, `parser`, and `runtime` objects |
| `tools.checker/generator/parser` | `name` string; repo-relative `path` string; raw-file `sha256` string; semantic `version` string, exactly `"1.0.0"` for schema v1 |
| `tools.elaborator` | `name` string, exactly `"Verilator"`; `version` string, exactly the single LF-trimmed first line returned by the recorded command; `version_command` array, exactly `["verilator","--version"]` |
| `tools.runtime` | `name` string, exactly `"CPython"`; `version` string, exactly `platform.python_version()` with no prefix/suffix |

For `selected_value`, `unsigned_integer.text` matches `0|[1-9][0-9]*`; `signed_integer.text` matches `0|-?[1-9][0-9]*`; `bit_vector.text` is exactly `<width>'h<digits>` with decimal width, lowercase hex, exactly `ceil(width/4)` digits, and zero unused high bits; `string.text` is the decoded NFC string value rather than quoted SystemVerilog source. The kind must agree with the elaborated parameter type. `source_default_expression` and every `source_expression` preserve the exact LF-normalized declaration substring, including internal whitespace and spelling, with no leading/trailing text outside that syntactic item.

`schema_id` is exactly `"zhao.texture.interface"`, `schema_version` is JSON integer `1`, `module.name` and `elaboration.top_module` are exactly `"zhao_texture_island_v3_top"`, and every path is NFC-normalized, `/`-separated, repo-relative, contains neither `.` nor `..` segments, and is case-sensitive. Strings containing non-NFC text are rejected rather than silently normalized. The closed root admits no timestamp, absolute path, host name, environment variable, git worktree state, or tool-cache path. Ordinals begin at zero, are contiguous, and agree with array position. Parameter and port arrays retain source declaration order. `source_closure` retains compile order, package first and selected top last. `elaboration.parameter_overrides` contains every effective parameter once, in the same order as `parameters`; its `{kind,text}` value must equal the selected value. `elaboration.argv` is the exact argument vector used for the elaboration query, with the executable token first, all parameter overrides in parameter order, and all closure paths in source-closure order; no shell-quoted command string is stored.

The selected V3 artifact records every effective parameter, including `MIGRATION_SHADOWS=1, DEPTH=16, CTXW=64, RCTXW=160, AUXCTXW=224, BINDW=8, LODW=8, GENW=8, LANES=4, SRCW=18, DATAW=64, TOKW=18, AUX_TOKW=14, PAL_SLOTS=4, PAL_ENTRIES=256`. A production-profile manifest may select `MIGRATION_SHADOWS=0`, but it is a separately named artifact; it cannot overwrite this laboratory interface.

For dimensions, declaration order is outermost-to-innermost within each packed or unpacked list. `size=abs(left-right)+1`; `direction` is `descending` when `left>right` and `ascending` when `left<right`; a one-element declared range retains the direction implied by its source expression. `element_width` is the product of packed dimension sizes, or 1 when `packed_dimensions=[]`. `unpacked_element_count` is the product of unpacked sizes, or 1 when `unpacked_dimensions=[]`. `bit_width=element_width*unpacked_element_count` and is the elaborator's `$bits(port)`. A port wider than 64 bits remains **one row at its exact integer width**. An unpacked array remains one row and retains all bounds and declared directions. If a later wrapper chunks a port, that separate map orders chunks by declaration ordinal, then unpacked indices in declared left-to-right iteration order, then packed least-significant bit first; it never changes this manifest.

This is the complete pretty-printed shape exemplar used by the schema fixture. It deliberately uses all-zero digest sentinels so copying prose cannot create provenance; the fixture generator must replace every sentinel and the checker must reject this literal example as stale. All object keys below are already in the required lexical order; the accepted artifact itself is emitted as the compact canonical bytes defined after it.

```json
{
  "elaboration": {
    "argv": [
      "verilator",
      "--xml-only",
      "--top-module",
      "zhao_texture_interface_schema_fixture",
      "-GW=96",
      "tests/rtl/fixtures/zhao_texture_interface_schema_fixture.sv"
    ],
    "cwd": ".",
    "parameter_overrides": [
      {
        "name": "W",
        "value": {
          "kind": "unsigned_integer",
          "text": "96"
        }
      }
    ],
    "top_module": "zhao_texture_interface_schema_fixture"
  },
  "hashes": {
    "canonical_interface_sha256": "0000000000000000000000000000000000000000000000000000000000000000",
    "module_declaration_sha256": "0000000000000000000000000000000000000000000000000000000000000000",
    "top_source_sha256": "0000000000000000000000000000000000000000000000000000000000000000"
  },
  "module": {
    "declaration_end_byte_exclusive": 311,
    "declaration_start_byte": 0,
    "name": "zhao_texture_interface_schema_fixture",
    "source_path": "tests/rtl/fixtures/zhao_texture_interface_schema_fixture.sv"
  },
  "parameters": [
    {
      "declared_kind": "parameter",
      "declared_type": "int unsigned",
      "name": "W",
      "ordinal": 0,
      "selected_value": {
        "kind": "unsigned_integer",
        "text": "96"
      },
      "source_default_expression": "96"
    }
  ],
  "ports": [
    {
      "bit_width": 1,
      "declared_type": "logic",
      "direction": "input",
      "element_width": 1,
      "name": "clk_i",
      "net_or_var": "net",
      "ordinal": 0,
      "packed_dimensions": [],
      "signed": false,
      "source_expression": "input wire logic clk_i",
      "unpacked_dimensions": [],
      "unpacked_element_count": 1
    },
    {
      "bit_width": 96,
      "declared_type": "logic",
      "direction": "input",
      "element_width": 96,
      "name": "wide_i",
      "net_or_var": "variable",
      "ordinal": 1,
      "packed_dimensions": [
        {
          "direction": "descending",
          "left": 95,
          "right": 0,
          "size": 96,
          "source_expression": "[W-1:0]"
        }
      ],
      "signed": false,
      "source_expression": "input var logic [W-1:0] wide_i",
      "unpacked_dimensions": [],
      "unpacked_element_count": 1
    },
    {
      "bit_width": 8,
      "declared_type": "logic",
      "direction": "input",
      "element_width": 8,
      "name": "ascending_signed_i",
      "net_or_var": "variable",
      "ordinal": 2,
      "packed_dimensions": [
        {
          "direction": "ascending",
          "left": 0,
          "right": 7,
          "size": 8,
          "source_expression": "[0:7]"
        }
      ],
      "signed": true,
      "source_expression": "input var logic signed [0:7] ascending_signed_i",
      "unpacked_dimensions": [],
      "unpacked_element_count": 1
    },
    {
      "bit_width": 192,
      "declared_type": "logic",
      "direction": "output",
      "element_width": 32,
      "name": "counters_o",
      "net_or_var": "variable",
      "ordinal": 3,
      "packed_dimensions": [
        {
          "direction": "descending",
          "left": 31,
          "right": 0,
          "size": 32,
          "source_expression": "[31:0]"
        }
      ],
      "signed": false,
      "source_expression": "output var logic [31:0] counters_o [2:1][5:7]",
      "unpacked_dimensions": [
        {
          "direction": "descending",
          "left": 2,
          "right": 1,
          "size": 2,
          "source_expression": "[2:1]"
        },
        {
          "direction": "ascending",
          "left": 5,
          "right": 7,
          "size": 3,
          "source_expression": "[5:7]"
        }
      ],
      "unpacked_element_count": 6
    }
  ],
  "schema_id": "zhao.texture.interface",
  "schema_version": 1,
  "source_closure": [
    {
      "kind": "systemverilog_module",
      "ordinal": 0,
      "path": "tests/rtl/fixtures/zhao_texture_interface_schema_fixture.sv",
      "sha256": "0000000000000000000000000000000000000000000000000000000000000000"
    }
  ],
  "tools": {
    "checker": {
      "name": "check_texture_v3_interface_manifest",
      "path": "tools/rtl/check_texture_v3_interface_manifest.py",
      "sha256": "0000000000000000000000000000000000000000000000000000000000000000",
      "version": "1.0.0"
    },
    "elaborator": {
      "name": "Verilator",
      "version": "Verilator 5.x fixture placeholder",
      "version_command": [
        "verilator",
        "--version"
      ]
    },
    "generator": {
      "name": "gen_texture_v3_interface_manifest",
      "path": "tools/rtl/gen_texture_v3_interface_manifest.py",
      "sha256": "0000000000000000000000000000000000000000000000000000000000000000",
      "version": "1.0.0"
    },
    "parser": {
      "name": "texture_v3_interface_parser",
      "path": "tools/rtl/texture_v3_interface_parser.py",
      "sha256": "0000000000000000000000000000000000000000000000000000000000000000",
      "version": "1.0.0"
    },
    "runtime": {
      "name": "CPython",
      "version": "3.x fixture placeholder"
    }
  }
}
```

The source view preserves exact declaration text/order/ranges and computes `module.declaration_start_byte` and `declaration_end_byte_exclusive` against raw top-source bytes. `top_source_sha256`, every `source_closure[].sha256`, and every custom-tool `sha256` are lowercase SHA-256 over raw file bytes exactly as stored; `top_source_sha256` must equal the closure row for `module.source_path`. `module_declaration_sha256` alone uses the byte span after converting CRLF and bare CR to LF, with no trimming and UTF-8 encoding. Tool versions are data, not comments: the checker reruns `verilator --version`, verifies its exact LF-trimmed first line, verifies CPython's exact version, requires custom-tool version `1.0.0`, and recomputes all raw tool hashes.

The canonical-interface digest algorithm is exact:

1. Parse UTF-8 with no BOM; reject duplicate object keys, invalid UTF-8, non-NFC strings, floats, `null`, unknown members, and integers outside the schema's nonnegative constraints except signed dimension bounds.
2. Validate all cross-field equations, ordinals, paths, source/elaboration equality, and hashes except the canonical digest itself.
3. Deep-copy the root and remove **only** member `hashes.canonical_interface_sha256`; retain the now-two-member `hashes` object and every other byte-relevant value. No other hash, tool version, or field is blanked.
4. Serialize the copy with recursive Unicode-code-point lexical object-key ordering; arrays unchanged; UTF-8 non-ASCII emitted directly; only JSON-mandatory quote, backslash, and control-character escapes; lowercase `true`/`false`; base-10 integers with no leading zero or `+`; separators exactly `,` and `:`; and no spaces, BOM, or trailing LF. The normative implementation is Python `json.dumps(payload, ensure_ascii=False, allow_nan=False, sort_keys=True, separators=(",", ":"))` after the NFC/type checks, encoded with `.encode("utf-8")`.
5. Set `hashes.canonical_interface_sha256` to lowercase `sha256(canonical_bytes_from_step_4).hexdigest()`.
6. Serialize the complete root by the same rule for the stored manifest, again with no trailing LF. The stored file's raw SHA-256 is intentionally **not** embedded in itself; freshness is the recomputed step-4 payload digest plus all constituent raw hashes.

The generator performs two independent views: a source-declaration parse and a Verilator elaboration query at the selected values. The checker compares parameter names/order/values and port names/order/directions/kinds/signedness/every dimension/element width/total `$bits`; it then checks exact closure and canonical bytes. Missing Verilator, different Verilator or Python version, an unparseable expression, unknown width, missing/extra/reordered port or parameter, stale source/tool, reordered closure, hand-edited whitespace, a digest computed while retaining its own field, or a manifest not byte-for-byte canonical is a hard nonzero failure, never SKIP. Fixture self-tests include the complete shape above, a >64-bit packed port, ascending/descending packed ranges, two nonzero-bound unpacked dimensions, a signed port, duplicate keys, one self-hash-retained digest, a trailing-newline file, and a deliberate source/elaboration mismatch; each negative fixture must make the checker fail.

The exact Packet-B source closure is package first and top last:

```text
fpga/rtl/common/zhao_render_texture_pkg.sv
fpga/rtl/field/zhao_field_rcp24_rom.sv
fpga/rtl/raster/zhao_raster_ticketq.sv
fpga/rtl/raster/zhao_raster_ticketq_rh.sv
fpga/rtl/raster/zhao_raster_rcp24_mul.sv
fpga/rtl/raster/zhao_raster_rcp24_v4.sv
fpga/rtl/raster/zhao_raster_perspuv_pairpipe_v2.sv
fpga/rtl/texture/zhao_texture_mod255.sv
fpga/rtl/texture/zhao_texture_aux_div6.sv
fpga/rtl/texture/zhao_texture_bilerp_lane_v2.sv
fpga/rtl/texture/zhao_texture_mosaic_v2.sv
fpga/rtl/texture/zhao_texture_palette_res_v2.sv
fpga/rtl/texture/zhao_texture_tmu_plan_v2.sv
fpga/rtl/texture/zhao_texture_cache_pipe_v2.sv
fpga/rtl/texture/zhao_texture_v3bank.sv
fpga/rtl/texture/zhao_texture_v3rq.sv
fpga/rtl/texture/zhao_texture_v3own.sv
fpga/rtl/texture/zhao_texture_metajoin_v2.sv
fpga/rtl/texture/zhao_texture_uv_join_v2.sv
fpga/rtl/texture/zhao_texture_early_desc_v2.sv
fpga/rtl/texture/zhao_texture_frag_expand_v2.sv
fpga/rtl/texture/zhao_texture_binding_resolver_v2.sv
fpga/rtl/texture/zhao_texture_rsp_dispatch_v2.sv
fpga/rtl/texture/zhao_texture_aux_pipe_v2.sv
fpga/rtl/texture/zhao_texture_material_combine_v3.sv
fpga/rtl/texture/zhao_texture_island_v3_top.sv
```

No consumer may compile before `zhao_render_texture_pkg.sv`; every importing file uses an explicit package import rather than copied widths. The closure checker must prove this list equals the recursively elaborated selected-root module closure **plus its imported package files** exactly—no missing and no unreachable extra source—and the selected V3 fit target uses the same ordered compile closure.

A source/elaboration inventory then proves Early-Z `PAYLOAD_W=410`, skid `W=490`, retirement context 160, owner context 224, typed AUX context 224, descriptor logical image 287, descriptor physical image 320 as eight ascending 40-bit slices with exactly 33 zero pad bits, V2 descriptor/UV `joined365={owner14,logical287,U32,V32}`, material row 46, class response tuple 66, and external result 48. It proves that the captured eight-bit page generation exists exactly once in the join as `logical287[286:279]`, with no appended generation port/field, and that the expander copies only that slice into every resolver sample job. It also proves `frag_invw24_i` exists and `frag_depth_i` does not exist on the versioned top. The leaf-interface inventory requires `zhao_texture_early_desc_v2.desc_pad_fault_o` declared exactly `output logic [31:0]`, `zhao_texture_aux_pipe_v2.sheet_rsp_owed_o` declared exactly `output logic`, and scalar `output logic` ports `zhao_texture_binding_resolver_v2.{cfg_loader_idle_o,binding_crc_busy_o,binding_seal_pending_o}`. The quiet-source audit requires every versioned idle/data-idle port, parses every top-local `q_*` source mapping, proves single-driver connectivity without hierarchical child-state reads, and proves every mapped operand occurs with the frozen polarity in exactly one of the literal `data_quiet`/`quiet_o` expressions.

A role-aware checker rooted at the connected top requires exactly one provider of `raster_texture_fragment_lifecycle`: `zhao_texture_v3own`. Expanders, descriptor banks, resolver, contexts, skids, and sequence witness declare transport/storage/check roles, not ownership. A test-only duplicate-owner composition marks both V3 ownership and TEXJOIN ownership and must make the checker fail. Generated accounting and later connected-production tops are checked separately; a disconnected selected sibling cannot satisfy connected ownership.

### 12.2 V3, material, AUX, and result gates

Drive the unchanged old island and Packet-B V3 with identical PASSTHRU/count-1/no-AUX traffic and compare RGB/A/legacy-tag/refused order under stalls. All other material arithmetic compares the corrected `zref_material.hpp` directly with owner ruling R9 and the new V3 combiner; the old island is not allowed to outvote that oracle.

Required vectors include zero, one, 127, 128, 254, and 255 in every operand position and cover:

* PASSTHRU counts 0 and 1, and every illegal count;
* recipes 1–5 at exact count 2 and wrong counts 0/1/3;
* recipes 6–7 at exact count 3 and wrong counts 0/1/2;
* exact alpha for all eight recipes, including unchanged `s0.a` on 1–4/6, unit alpha on MASK/detail-mask, negative LERP deltas, and saturation boundaries;
* MODULATE2X's single `(a*b+64)>>7` rounding, MASK's continuous alpha rather than a nonzero gate, and both detail recipes' MODULATE2X first layer;
* a malformed material generating no planner/cache/Sheet access, one local terminal refusal for each declared required source, loud final error, sticky frame fault, and normal ordered release;
* sample 2 and AUX both required simultaneously, proving the combiner reads sample 2 and only AUX status; changing AUX tag/strength with status zero must not change RGB/A/index, while changing sample 2 must change the two three-sample recipes;
* successful AUX tag/strength arriving in the typed AUX plane and being deliberately unexposed; any test that treats it as RGBA fails.

Result-path vectors include CLUT8 indices 0 and 255, both CLUT4 nibbles, all direct formats with index zero, palette latency/backpressure, and independently delayed class queues. Every accepted queue record is scoreboarding the full 66-bit tuple. Final status is ORed only across committed required planes; final index is sample 0 or zero at count zero. The star-disc/halo and alpha-test cases run end to end through `RASTER.FRAGMENT`; forcing index zero, substituting a palette-colour byte, or borrowing a later response's index must fail.

AUX vectors are derived from normative `design/contracts/TEXTURE.AUX.V2.md`, not oracle-only `TEXTURE.AUX.md`. They interleave sheet A/envelope A, sheet B/envelope B, then sheet A under independent descriptor, request, response, and return stalls. They cover same index old/new generation, HIT, MISS, degenerate envelope, wrong opcode, wrong source ID, ALLOCATED/OVERFLOW on READ, duplicate response, response without issue, AUX absent, and AUX required at sample count zero. Each malformed owed response terminates the expected head exactly once and every no-issue response changes no owner state. Contract conformance also proves the 224-bit field offsets, 48-bit AUX plane, issue-before-return, request/response hold, typed counts/quiet, and the absence of any AUX-to-sample-2 data path. A fixture which selects old `TEXTURE.AUX.md` as Packet-B authority must fail the contract-closure checker.

Committed arithmetic mutants reproduce each stale alternative independently: alpha multiplication on recipes 1–4, binary MASK, rounded-unit-then-double MODULATE2X, unit rather than MODULATE2X detail first layer, and count-zero early bypass for a non-PASSTHRU recipe. Additional mutants substitute AUX for sample 2 and drop/reconstruct raw index after palette lookup. The ordinary tests assert correct output; inverse-polarity mutant drivers prove each detector/gate can fail.

Output backpressure is applied while later work completes out of order. RGB, alpha, index, status, all 160 retirement bits, and the legacy 16-bit tag view remain stable. The compatibility test specifically drives `frag_ctx_i[15:0] != frag_aux_ctx_i[15:0]`; only the former may appear on `out_tag_o`.

### 12.3 Full-identity stall gate

An independent scoreboard records every accepted V3 fragment as:

```text
external: all 160 retirement-context bits plus every V3 request field
internal descriptor/UV joined365: {owner14, logical287, U32, V32}
  sole joined generation location: logical287[286:279]
internal sample work: {slot[5:0], generation[7:0], sample_index[1:0], page_generation[7:0] copied from logical287[286:279]}
internal AUX work:    {slot[5:0], generation[7:0]}
```

It then applies long, independently randomized stalls at V3 input, descriptor read, both V2 UV-join inputs, V2 UV-join output, planner/cache response, AUX response, V3 output, fragment input, tile read/write, resolve, and framebuffer output. The A/B join case holds A at the output while B is offered and requires A's owner, all 287 logical bits—including sole generation slice `[286:279]`—and U/V to remain unchanged; the resolver's expected generation is the admission-time scoreboard copy, not a duplicate join field. On every stalled cycle the scoreboard checks `valid` and every payload bit, not only the eventually accepted packet. Aggregate accepted/emitted counters are secondary.

Committed renamed mutants cover three independent timing faults: the context-read/slot swap changes a held descriptor while retaining its token; the UV-join generation-slice swap retains A's owner/U/V and other logical bits while taking B's `logical287[286:279]`; and a late-global mutant overwrites that held slice from current active-page state. Normal drivers assert correct hold/order and compare with the admission-time expected record; separate inverse-polarity mutant drivers pass only when the independently clocked mismatch detector fires. No detector operand may share the corrupted enable. The descriptor physical-image mutant is separate: it writes one of the 33 pad bits high; one accepted read must produce `desc_pad_fault_o` delta one, one terminal refusal path, no usable descriptor, and a separately set sticky frame fault while ordinary RTL continues forcing all pad bits zero.

### 12.4 Bubble gate

Preload enough owners and completed results to keep the ordered edge occupied, hold the consumer closed, then release ready. After the documented output-pipeline fill, require one output handshake on every clock until the prepared run drains. Simultaneous output pop and queue reload must work.

A committed no-same-edge-reload mutant must produce a detectable alternating bubble and fail. This control does not demand same-edge admission at the owner's full-to-free fence; that one conservative admission bubble is intentional and counted separately.

### 12.5 Owner response controls

Retain and run all of the owner controls:

* wrong generation;
* sample index 3 / out of requested range;
* unrequested source;
* return before accepted issue;
* duplicate return;
* unauthorized final;
* simultaneous malformed TMU and AUX returns;
* valid simultaneous TMU and AUX returns, proving typed commits each increment one and combined commits increment two;
* sample-only then AUX-only commits, proving neither typed counter aliases the combined counter;
* locally refused binding/AUX work with logical issue at N and earliest return at N+1;
* generation wrap/drain;
* head incomplete while later owners finish.

Every named refusal/protocol class remains separately observable. A sum over an unstated subset or the legacy combined commit count is not evidence.

Because a correct work queue cannot overflow under legal stimulus, retain the renamed committed small-queue/full-guard mutant whose inverse-polarity test requires the overflow counter to fire.

### 12.6 Raster/tile differential

For representable one-sample fixtures, compare old flat `zhao_raster_tile_pipe` with V2 using a texture/palette fixture which returns the same RGB/A/index. Then compare the complete V2 path to `zref` on varying interpolated depth/UV/color, Early-Z accept/reject, alpha test, star tag, blend/stencil hazards, multi-triangle tiles, bank overlap, and framebuffer backpressure.

Checks include:

* exact accepted fragment order and count;
* exact depth bits at Early-Z, fragment input, and tile write;
* exact source ID through the fragment read response;
* exact 64-bit tile words;
* exact 256 resolve outputs and tile CRC;
* no swap until all V3 and fragment work drains;
* next-front-bank rendering overlaps previous-back-bank resolve as before.

A committed drain mutant which omits V3 quiet from `pipe_empty` must swap early under a delayed texture response and fail the tile/CRC comparison.

### 12.7 Binding and memory gates

Binding tests program sparse and dense pages through BEGIN/WRITE/END/ABORT, write selectors out of order, and independently vary active/staging generations. They verify canonical CRC bytes, duplicate-row rejection, illegal mode/base/palette rejection, bad CRC leaving the old page active, reset generation zero, delayed END acknowledgement, fragment-admission fence at successful seal, and atomic activation only after complete `data_quiet`. While prior owners remain live, accepted fragments alternate selectors and every request must retain its own page/base/mode/class/palette tuple. Dedicated descriptor tests prove `physical320={33'b0,logical287}`, `slice[k]=physical320[k*40 +: 40]` for all `k=0..7`, simultaneous eight-slice write, held eight-slice read, and independent zero-pad/generation verdicts. The committed one-pad-bit mutant's first accepted bad-pad read must change `desc_pad_fault_o` from reset zero to one exactly once despite response stalls, expose no logical row, terminally refuse from the owner's required mask, and set the distinct sticky frame fault. Separate counter controls prove hold, delta two for two accepts, and modulo wrap.

The generation-carriage test accepts owner A under page generation A, stalls `zhao_texture_uv_join_v2`, activates/offers distinguishable B only in the mutant/control schedule, and proves every A `joined365.logical287[286:279]` and resolver job still carries A. A current-active late read, a B-generation-slice/A-owner swap, removal or duplication of `logical287[286:279]`, or comparison of two registers clocked by the same join enable must fail independently of final colour.

Selector controls use bases 253, 254, and 255 at counts 1/2/3. A carry produces one logical owner issue and one next-or-later local refusal, zero planner/cache access, and no lookup at wrapped selectors 0 or 1. Invalid row, page generation mismatch, no active page, and sample-0 class/palette mismatch obey the same issue-before-return timing. Count zero requires zero witnesses and performs no lookup. Samples 1/2 deliberately resolve different classes and palette generations to prove the single fragment witnesses never drive them.

Committed binding/descriptor mutants include modulo-256 selector addition, current-active-page late read, V2 UV-join held-generation overwrite, one nonzero descriptor pad write bit, activation on owner quiet instead of structural data quiet, active-bank WRITE, CRC over stale invalid payload, class routed from the fragment witness, palette taken from response-token bits, local refusal in the issue cycle, and local refusal without an issue. Each must fail exact issue/planner/refusal/commit counts as well as payload comparison; late-read-global-binding and generation-swap mutants are not considered covered by output colour alone.

The ENGINE1 local-arbiter test covers geometry lengths 32/64 and texture length 16, both contention orders, guard accept and registered OK/deny verdicts, global stalls, refresh, and interleaved scanout. It checks every raw returned halfword against the captured subowner and request position. For each accepted request it requires exactly one later verdict; it requires no request presentation while `WAIT_VERDICT`, and proves a verdict without acceptance, two verdicts, both verdict bits, and a second pre-verdict acceptance each fire their independently clocked detector.

The denial positive control holds source valid and all fields stable until `guard_accept`, then requires guard-facing valid low during the later violation pulse and exact deltas `{GA,GDENY,local_refusal}={1,1,1}`. Its committed held-valid-denial mutant drops valid on verdict rather than acceptance; with guard ready still high it replays the request and must fail the accept/verdict/refusal counts.

Committed memory mutants must include at least:

* release/change subowner after the first controller burst instead of the whole request;
* route texture through the 64-bit geometry packer;
* omit the eighth-beat release or accept a ninth beat;
* turn a guard refusal into a silent wait;
* hold guard-facing request valid through a denied request's registered verdict.

The guard-region test proves the old geometry addresses and newly ratified texture addresses pass read-only, every write fails, both boundaries fail, and client 5 remains unavailable.

### 12.8 Work/cadence gate

Long all-ready homogeneous and mixed streams check every accepted/completed equality, both typed owner commit counts, their compatibility sum, and the `J1 + 2*J2 + 3*J3` phase equation in section 9.2. Separate controls duplicate an in-flight sampler job and a combiner phase while keeping output bytes unchanged; both must fail exact job/phase deltas. A simultaneous sample/AUX commit and sample-only/AUX-only runs prove counter independence. The steady-state report names accepted fragments, samples, AUX jobs, cache jobs/fills, combiner jobs, issued/completed phases, typed commits, and ordered outputs. An owner-edge one-per-clock result without those downstream deltas cannot pass.

The structural-quiet test then exercises every `q_*` state and channel row in section 4.5 one at a time while all other rows—including `q_owner_idle`—indicate empty. It separately holds reciprocal/descriptor/UV/planner/cache/palette/combine requests and responses, every class bit, fill and Sheet requests, top wire `aux_sheet_rsp_owed_w=1` (driven by the AUX V2 output port) with `pg_valid_i=0`, unsolicited fill/Sheet response inputs, local TMU/AUX refusals, top retirement `out_valid_o`, config command `cfg_valid_i`, held `cfg_rsp_valid_o`, and each resolver observation wire corresponding to `cfg_loader_idle_o=0`, `binding_crc_busy_o=1`, and `binding_seal_pending_o=1`. Public `quiet_o` must remain low in every case and must reach true only after that exact condition is removed. The source-map checker proves every `q_*` operand has one exact named source and one equation use, no source is private child state, and all vector bits participate. It requires one inverse-polarity omission mutant per operand, plus the owner-quiet alias and AUX-issued-credit mutants; a single prose-group mutant is insufficient. The AUX control accepts two Sheet reads and retires their responses one at a time, requiring `sheet_rsp_owed_o` to stay high after the first and fall only after the second, while `idle_o` remains low if an AUX return is still held. These fire controls pass before quiet is cited by a tile-swap or binding-activation test.

### 12.9 Lease, publication, and CDC gate

The writer-aware slot test covers simultaneous renderer/blitter requests in both round-robin histories, request and grant stalls, both slots, every canvas mode, and base/span boundary addresses. It requires one accepted contender, stable loser payload, no starvation, grant capture of the exact `{writer,slot,generation}`, and both guards accepting only the matching writer inside `[base,base+span)`.

Terminal tests cover clean renderer and blitter publication, texture/memory/identity fault release after complete drain, explicit cancel, publication FIFO backpressure, duplicate and stale terminal events, wrong writer, same slot/wrong generation, and generation wrap after drain. Fault release must produce no READY event. A clean publication must produce exactly one READY FIFO write on the same accepted state transition.

CDC tests independently stall both FIFO sides and every video boundary. With no accepted READY event, several frame boundaries must repeat the displayed slot and emit no swap. With one event, video swaps once and echoes the unchanged tuple; GPU frees the previous displayed slot only after the matching swap return. Stale and duplicate swap returns change no state and fire their positive controls.

Committed mutants include: external/current `fb_writer_i` substituted for the captured writer; slot-only terminal comparison; `{slot,generation}` comparison that omits writer; raw writer-done used as the CDC source; fault release enqueued as READY; framebuffer base taken from a caller pin; and swap return freed without generation match. Each has an inverse-polarity fire test for the corresponding independent detector.

Reset is asserted at held request, held grant, active writes, publication backpressure, READY FIFO occupancy, video pending, and swap return. The gate requires the blank/reset barrier, zero surviving pre-reset events, no lease before both reset-done acknowledgements, and a post-barrier clean publication before display resumes.

### 12.10 Terrain composition gate

After the upstream blockers close, run legal masks `01`, `10`, and `11`, with different matrices/viewports and full output stall checks. For mask `11`, require one projector core, one V3, exact view/source/depth/material identity, no per-view state collision, and the dense workload counters. A reduced 128-triangle binner fixture is explicitly labelled subsystem-only and cannot satisfy this gate.

## 13. Staged implementation packets, ownership, and rollback

The acceptance order is strict:

```text
A -> B -> C -> D -> E -> F(G8A) -> G -> H -> I(G8B) -> J(G8C) -> K
```

**Packet A is a hard landing dependency, not an overlap candidate. Before Packet A's complete changeset has landed and its Packet-A gate is green, no Packet-B RTL, generator, parser, checker, fixture, test, mutant, manifest, contract, reference, ledger, fit/source list, or generated-artifact change may begin. Architecture-report editing is the only Packet-B activity permitted before that edge. This specific rule overrides the repository's generic permission to develop a new block while an earlier fit runs and overrides every overlap sentence below.** Packet B starts only from the landed-green Packet-A package widths and role checker; an uncommitted, merely passing, or concurrently changing Packet A does not satisfy the dependency.

After that A-to-B edge, a later packet may be developed while an earlier **independent** fit runs, but it cannot be promoted past its gate or selected by a dependent packet until every predecessor is green. No part of B is independent of A. If an upstream packet is reverted, every dependent packet is reverted in reverse order. There is no supported mixed state with new ports and old manifests, contracts, generated accounting RTL, or source closure.

The following closure law applies to **every** packet, not only final adoption:

1. A module addition, removal, rename, parameter-interface change, or port change updates the production block ledger/manifest consumed by `tools/quartus/check_prod_manifest.py` in that same packet.
2. Disposition follows the checker-proved instantiation graph. A module reachable from a selected accounting root is counted **inside that root and receives no `excluded` row**. `check_prod_manifest.py` intentionally fails an excluded module which is nevertheless in a selected root's closure.
3. `zhao_texture_island_v3_top` is already a selected **accounting** root. Therefore every Packet-B successor it instantiates—including the versioned reciprocal/perspective/meta/UV joins, descriptor, expander, resolver, planner, cache, dispatcher, Mosaic/bilerp/palette paths, AUX, and combiner—is inside that root, appears in its exact source closure, and is not `excluded:not-yet-adopted`. In particular `zhao_texture_uv_join_v2.sv` is a counted child, never an instantiated-but-excluded leaf. This accounting fact does **not** mean the shell renders through V3 or that production connection/adoption occurred.
4. Only a new module unreachable from every selected accounting root is registered `excluded:not-yet-adopted` (or the exact existing schema equivalent). Fit-only wrappers remain fit-only and never become functional providers merely by appearing in a source list.
5. The same packet regenerates `fpga/rtl/prod/zhao_prod_top.sv` with `tools/quartus/gen_prod_top.py`, refreshes the selected root's exact fit source closure, and runs freshness checks. A zero semantic generated diff is still checked against the changed source state.
6. Every V3 port change additionally updates all direct instantiations and the exact interface artifact/generator/checker in section 12.1. There is no later manifest-cleanup packet and no SKIP for an unavailable parser/elaborator.
7. Rollback restores RTL, contracts, reference oracle, tests/mutants, ledger/manifest, generated accounting top, interface artifact, fit targets, and source lists from the same packet atomically.

Throughout A–K, `fpga/rtl/common/zhao_shell_top.sv`, its old `zhao_shell_fit_top.sv`, D3 policy/manifest, and D3 receipt remain byte-for-byte historical artifacts. No packet redirects those names.

### Packet A — types and instruments; no behavior switch

**Owns**

* `fpga/rtl/common/zhao_render_texture_pkg.sv`: the 128-bit continuation, typed 224-bit AUX context, 490-bit pretexture packet, 410-bit Early-Z payload, 160-bit retirement context, and 48-bit result;
* `design/ownership_roles.yml` and `tools/rtl/check_owner_roles.py`: connected-top role census;
* checker self-tests, duplicate-owner fixture, and this packet's ledger/generated/source-closure updates.

**Gate:** exact layout elaboration including named AUX offsets, current-interface census, checker pass, and checker fire control.

**Rollback:** revert the package, checker/fixtures, and closure metadata together. No RTL consumer exists, so the selected hierarchy remains unchanged.

### Packet B — complete and contract the V3 boundary

**Owns**

* `fpga/rtl/texture/zhao_texture_island_v3_top.sv`: rename `frag_depth_i` to `frag_invw24_i`; retain legacy `frag_ctx_i`; add `RCTXW=160`, `AUXCTXW=224`, retirement-context/status/raw-index/structural-quiet ports, full binding-config and Surface Sheet fields; map every quiet alias to the exact leaf output port or named top-visible channel in section 4.5; remove live global binding authority; instantiate owner at context 224/result 48;
* `fpga/rtl/texture/zhao_texture_v3own.sv`: only `ev_tmu_commits_o` and `ev_aux_commits_o` instrumentation, while retaining combined `ev_commits_o`; no lifecycle enable or state transition changes;
* new `zhao_texture_early_desc_v2.sv`, `zhao_texture_frag_expand_v2.sv`, `zhao_texture_binding_resolver_v2.sv`, `zhao_texture_rsp_dispatch_v2.sv`, `zhao_texture_aux_pipe_v2.sv`, and `zhao_texture_material_combine_v3.sv`: the exact physical/logical descriptor, 32-bit reset-zero modulo `desc_pad_fault_o`, table/issue, resolver config-state observation ports, tuple, AUX-V2 `sheet_rsp_owed_o`, and R9 laws above;
* new `fpga/rtl/texture/zhao_texture_uv_join_v2.sv`: owner-aligned `joined365={owner14,logical287,U32,V32}` through both input holds and the output hold; captured active-page generation exists only at `logical287[286:279]`, which expansion copies into resolver jobs; the unversioned join remains oracle-only;
* versioned observation-preserving successors `zhao_raster_rcp24_v4.sv`, `zhao_raster_perspuv_pairpipe_v2.sv`, `zhao_texture_metajoin_v2.sv`, `zhao_texture_tmu_plan_v2.sv`, `zhao_texture_cache_pipe_v2.sv`, `zhao_texture_mosaic_v2.sv`, `zhao_texture_bilerp_lane_v2.sv`, and `zhao_texture_palette_res_v2.sv`: exact `idle_o` coverage required by section 4.5, with no new lifecycle or ready path;
* `design/contracts/TEXTURE.COMBINE.md` and `reference/include/zref/zref_material.hpp`: one eight-recipe R9 authority, exact counts/alpha/MASK/detail/count-zero behavior, 48-bit status/index result, and paired cadence;
* new normative `design/contracts/TEXTURE.AUX.V2.md`: typed context/plane, Sheet READ import, independent issue/return/accounting/quiet, malformed-response handling, and no-AUX-as-sample-2 law; existing `design/contracts/TEXTURE.AUX.md` remains byte-for-byte oracle-only;
* `design/contracts/TEXTURE.CACHE.md`: versioned typed terminal-refusal and accepted/completed cache/fill accounting to be implemented at Packet E, without pretending Packet B connected memory denial;
* the draw-side clarification in `design/contracts/SURFACE.SHEET.md`: READ opcode/status/source validation and the existing ready/valid page response used by AUX V2;
* `fpga/rtl/generated/zhao_texture_island_v3_top.interface.json`, `tools/rtl/gen_texture_v3_interface_manifest.py`, `tools/rtl/texture_v3_interface_parser.py`, `tools/rtl/check_texture_v3_interface_manifest.py`, their closed-schema/canonical-byte/self-hash/wide/unpacked/source-vs-elaboration fixtures, and the package-first exact source closure in section 12.1;
* V3/R9/AUX/binding differentials, descriptor physical-pad and V2 UV-join generation/identity stalls, typed-counter and per-operand structural-quiet tests, interleaved-sheet tests, required-mask tests, and every committed mutant named in sections 12.2, 12.3, 12.5, 12.7, and 12.8;
* all same-packet ledger/manifest/generated-accounting/fit-source updates demanded by the V3 port and instantiation changes.

The shared unversioned `zhao_texture_aux_pipe.sv`, `zhao_texture_material_combine_v2.sv`, `zhao_texture_rsp_dispatch.sv`, `zhao_texture_uv_join.sv`, `zhao_texture_metajoin.sv`, `zhao_texture_tmu_plan.sv`, `zhao_texture_cache_pipe.sv`, `zhao_texture_mosaic.sv`, `zhao_texture_bilerp_lane.sv`, `zhao_texture_palette_res.sv`, `zhao_raster_rcp24_v3.sv`, `zhao_raster_perspuv_pairpipe.sv`, and `zhao_texture_island_top.sv` are outside Packet B and remain byte-for-byte oracle leaves. Oracle-only `design/contracts/TEXTURE.AUX.md` is likewise unchanged. Because the versioned successors are instantiated by selected accounting root `zhao_texture_island_v3_top`, they are counted inside that root and receive **no excluded rows**. This is accounting closure only; Packet B still makes no shell connection, production adoption, area saving, or fit claim.

**Gate:** Packet A is already landed green; exact closed-schema source/elaboration manifest, canonical-byte/self-hash/tool-version checks, and fired fixtures; descriptor `287 -> {33'b0,*} -> 320 -> 8*40` packing plus 32-bit reset-zero modulo pad-fault counter independent of sticky fault; V2 UV join exactly `joined365` with generation only in `logical287[286:279]`; explicit AUX owed and resolver loader/CRC/seal output ports; exact single-driver source mapping for every quiet operand with no private-state read; R9 arithmetic/count/alpha/AUX-V2 laws; selector overflow; table programming/CRC/seal/generation/activation; class/palette witness refusal; issue-before-local-return; full Sheet opcode/status/source handling; raw tuple/index alignment; separate sample/AUX commits; literal per-operand structural quiet; full-context stalls; malformed owner returns; exact job/phase counts; queue-overflow controls; and uninterrupted prepared ordered output. Every zero-valued detector named by acceptance has a legal fire case or committed mutant.

**Rollback:** revert V3 port/width consumers, owner instrumentation, the physical descriptor and all versioned quiet/UV/binding/dispatch/AUX/combine leaves, new AUX-V2 and corrected combine/cache contract/reference updates, tests/mutants, interface generator/parser/checker/artifact, and regenerated ledger/manifest/accounting/source closure together. The old island, unversioned UV join/AUX leaves, oracle-only AUX contract, and TEXJOIN experiments remain executable; no raster selected the Packet-B boundary.

### Packet C — synthetic post-Early-Z composition

**Owns**

* `fpga/rtl/raster/zhao_raster_texture_stage_v3.sv`, registered immediately as `excluded:not-yet-adopted`;
* synthetic candidate/fragment harness, external-sequence/160-bit scoreboard, star/alpha-test differential, identity mutant;
* the packet's ledger, generated accounting top, and source-closure update.

**Gate:** atomic admission, all 490 candidate bits held under stall, exact fragment mapping, one-owner census, no adapter bubble, and terminal refusal retirement.

**Rollback:** remove the stage/harness and reverse its closure registration together. Existing tile/shell paths remain selected.

### Packet D — versioned tile and binner composition

**Owns**

* `fpga/rtl/raster/zhao_raster_tile_pipe_v2.sv`: 410-bit opaque Early-Z payload, 490-bit skid, attribute bundle, V3 stage, full drain law, TILESTORE/RESOLVE;
* `fpga/rtl/geometry/zhao_geom_binner_v2.sv`: binner-owned metadata at characterization capacity;
* `fpga/rtl/geometry/zhao_geom_bin_pipe_v2.sv`: exact setup/metadata splice;
* attribute/raster differential, early-swap mutant, and same-packet excluded registrations/generated closure.

The three unversioned modules remain unchanged oracles.

**Gate:** ATTRSTEP/ATTRDIV and `zref` differential, flat-compatible raster differential, multi-triangle accumulation, exact source/depth, swap/drain, resolve overlap, and full backpressure.

**Rollback:** revert all three V2 compositions and their closure records together; the current flat path remains selected.

### Packet E — guarded ENGINE1 share and refusal completion

**Owns**

* `fpga/rtl/memory/zhao_render_asset_mux.sv`, immediately `excluded:not-yet-adopted`, plus raw-16-bit return demultiplexing;
* Packet-B successor `fpga/rtl/texture/zhao_texture_cache_pipe_v2.sv`: terminal fill-refusal implementation and the Packet-B cache accounting, retaining its complete `idle_o`; the unversioned cache used by the old island remains unchanged;
* `fpga/rtl/common/zhao_pkg.sv`, `fpga/rtl/memory/zhao_mem_guard.sv`, `design/contracts/MEM.GUARD.md`, and `spec/memory_rules.md`: same-address-range ratification and exact accept-then-verdict master law;
* local-arbiter/guard/malformed-fill tests, held-valid-denial and subowner mutants, and closure regeneration.

**Gate:** exact 16/32/64-byte traffic; request hold only to `guard_accept`; guard valid low in `WAIT_VERDICT`; exactly one OK/deny verdict per acceptance; one refusal per denial; eight beats per successful texture fill; fairness; no dropped/misrouted return; client 5 unavailable.

**Rollback:** restore the geometry-only ENGINE1 route, old cache boundary, and old region alias/contracts together with ledger/generated/source closure. No address allocation moves.

### Packet F — G8A connected raster/texture characterization

**Owns**

* generated `fpga/rtl/generated/zhao_raster_texture_v3_fit_top.sv` and exact generation manifest;
* the one G8A `design/fit_targets.yml` entry and receipt parser gates;
* fit-only registration and exact fit source closure.

**Gate:** after A–E simulation/mutant gates are green, run G8A once from a clean committed source. Record connected ALM/register/M10K/DSP/Fmax and hierarchy without adding standalone rows. Any resource or timing conclusion is conditional on that receipt.

**Rollback:** remove only the fit wrapper/target/receipt registration. A failed characterization selects nothing and returns diagnosis to the owning earlier packet rather than spawning per-nodule fits.

### Packet G — writer-aware slot lease and accepted-ready CDC

**Owns**

* `fpga/rtl/video/zhao_video_slotmgr_v2.sv`: renderer/blitter arbitration, live `{writer,slot,generation,mode,base,span,fault}` record, terminal validation, and lifecycle counters;
* `fpga/rtl/video/zhao_fb_ready_cdc_v2.sv`: generation-bearing ready and swap FIFOs plus reset barrier;
* the V2 section of `design/contracts/VIDEO.SLOTMGR.md`;
* contention, stale/generation, clean/fault, frame-repeat, CDC/reset tests and committed lease mutants;
* immediate `excluded:not-yet-adopted` registrations and generated/source-closure update.

The existing slot manager remains the old shell's oracle.

**Gate:** every condition in section 12.9, including same-edge accepted publication/READY enqueue, no READY from fault release, exact base/span, reverse swap match, and reset-in-flight barrier.

**Rollback:** revert both V2 modules, contract section, tests, and closure metadata atomically. The old shell/slot manager remains selected.

### Packet H — sibling shell V2

**Owns**

* `fpga/rtl/common/zhao_shell_top_v2.sv`: versioned render input, V2 bin/tile path, V3 status/configuration, ENGINE1 share, writer-aware leases, derived framebuffer window, and combined drain/fault terminal law;
* a separately named generated D3-style shell-V2 fit wrapper, port policy, interface manifest, freshness/activity checks, and source closure;
* old/new shell differential on unaffected domains and texture-aware render differential.

The sibling is registered `excluded:not-yet-adopted`; the old shell files remain byte-identical.

**Gate:** F and G are green; unaffected behavior matches under paired traffic; render differences match the sampled oracle; no external writer/base authority reaches a guard; every new port is connected; owner and hierarchy census are exact.

**Rollback:** revert sibling/wrapper/policy/manifest/source entries as one unit. Production and D3 continue to select the historical shell.

### Packet I — shared terrain prerequisites and fixed G8B target

**Owns**

* the existing terrain path's open normals, projected-`w`/`invw24`, material, world-X/Z, sheet-handle, and patch-envelope carriage;
* the production-scale binner/tile scheduling decision without duplicating projector or texture services;
* generated `fpga/rtl/generated/zhao_terrain_pipe_rpp3_matw18_fit_top.sv`, instantiating explicit `ROWS_PER_PASS=3` and `MATW=18`;
* G8B target, legal-mask activity test, receipt parameter/hierarchy gates, and same-packet manifests/source closure.

**Gate:** legal masks 01/10/11, headline dense two-view counters, exact owner-sealed AUX identity, one shared projector, all simulation gates, then one clean G8B receipt proving the parameters actually elaborated. ALM/DSP/M10K/Fmax are reported values, never assumed savings.

**Rollback:** revert the terrain seam/scheduler decision and G8B wrapper/target/closure together. Shell V2 remains an unselected raster/texture candidate.

### Packet J — final shared composition and G8C

**Owns**

* one integration top joining the single terrain/group/projector path to `zhao_shell_top_v2`, registered `excluded:not-yet-adopted` until Packet K;
* legal two-view end-to-end differential;
* one generated real-pin G8C wrapper/target, activity witness, hierarchy checks, and exact source closure.

**Gate:** A–I remain green; legal `view_mask=2'b11`, RPP3/MATW18 workload and exact frame/cadence/lease results pass; then one clean G8C fit must show one projector core, one V3 island, one `zhao_texture_v3own`, zero TEXJOIN instances, GPU 100-MHz closure, and **comfortable measured margin below 30,000 ALMs and 85 DSPs**. This is an acceptance condition, not a forecast.

**Rollback:** revert the integration top, wrapper/target, and closure metadata together. A missed fit gate selects nothing and grants no standalone-row arithmetic or TEXJOIN saving.

### Packet K — production/accounting selection, last

Only after J passes, one atomic packet:

* changes the production selection to the connected shell/terrain hierarchy;
* changes TEXJOIN's ledger state to `excluded:superseded` while retaining RTL, tests, standalone target, and historical receipt;
* regenerates `fpga/rtl/prod/zhao_prod_top.sv`, production manifest, interface/accounting manifests, and exact production fit source list in the same packet;
* proves the generated diff selects one connected hierarchy and removes only TEXJOIN's private accounting instance/stimulus;
* reruns manifest freshness, owner-role, hierarchy, DSP-census, and uncashed-cheque gates against the selected top.

**Rollback:** revert the selection, TEXJOIN disposition, generated accounting top, manifests, and source list together to Packet J's unadopted state. Packet K performs no deferred repair from A–J and does not turn TEXJOIN's standalone 3,824-ALM row into a physical saving.

## 14. Exit criteria and explicit non-claims

The architecture is ready for implementation when these statements remain true:

* the post-Early-Z seam carries every V3 request operand and every downstream fragment field;
* V3's existing owner context, not a parallel FIFO, owns the continuation lifetime;
* raw sample-0 index and status reach `RASTER.FRAGMENT`, with every 66-bit class tuple aligned through palette latency;
* owner ruling R9 is the sole recipe/count/RGB/alpha authority in contract, RTL, `zref`, and tests;
* normative `design/contracts/TEXTURE.AUX.V2.md` owns Packet-B AUX while old `TEXTURE.AUX.md` remains oracle-only; AUX never substitutes for sample 2, and Packet B explicitly leaves successful tag/strength without a typed visible consumer;
* the early descriptor is exactly `physical320={33'b0,logical287}` with `slice[k]=physical320[k*40 +: 40]`, constant-zero writes, independently checked 33-bit pad, and a separate 32-bit reset-zero modulo `desc_pad_fault_o` incremented once per accepted bad-pad read rather than aliased to sticky frame fault;
* `zhao_texture_uv_join_v2` holds exactly `joined365={owner14,logical287,U32,V32}`; its sole captured page-generation location is `logical287[286:279]`, copied later into resolver jobs for an independently enabled comparison;
* the 75-bit, 256-entry, two-bank binding table has exact programming, CRC, generation, seal, activation, witness, overflow, and local-refusal laws;
* every local binding/AUX refusal is logically issued before its later terminal return;
* `invw24` is preserved bit-for-bit, is named `frag_invw24_i`, and is not confused with projected `w`;
* full caller source identity reaches the fragment RMW transaction;
* every state transition is tied to a ready/valid acceptance;
* full payloads hold under all stalls;
* the output can retire one prepared result per clock without an adapter bubble;
* ENGINE1 locally shares geometry and texture while client 5 remains unspent;
* ENGINE1 request acceptance and its registered verdict are counted separately, and denial completes exactly once;
* fill refusal is terminal rather than a hidden deadlock;
* the GPU-domain lease captures `{writer,slot,generation,mode,base,span}` and both guards use only that record;
* only an accepted clean publication creates a generation-bearing READY CDC event; fault release and no-ready frames repeat the previous complete display;
* accepted/completed fragment, sample, AUX, cache, fill, combiner-job, and combiner-phase counts close at drain, with independent owner TMU/AUX commits and their retained sum;
* V3 quiet is the literal structural conjunction and exhaustive single-driver source map in section 4.5, including every versioned leaf idle, held request/response, AUX V2 `sheet_rsp_owed_o`, config response, retirement valid, and resolver V2 `cfg_loader_idle_o`/`binding_crc_busy_o`/`binding_seal_pending_o`—never private child state or an alias of owner quiet;
* tile swap and frame publication include V3/memory drain;
* both views share one terrain/projector path and one texture island;
* the legal RPP3/MATW18 two-view workload, not RPP1 or a 128-triangle fixture, is the final workload;
* G8B elaborates the parameter-fixed `zhao_terrain_pipe_rpp3_matw18_fit_top`, not raw-module defaults;
* Packet B begins only after Packet A has landed green; before that edge no Packet-B implementation artifact of any kind changes, irrespective of generic overlap permission;
* every changed V3 port is reconciled with the closed schema-v1 interface manifest, exact nested JSON types/order, source/elaboration checker, recorded Verilator/CPython/custom-tool versions, raw hashes, self-field-omitting canonical SHA-256 algorithm, wide/unpacked representation, and package-first closure in Packet B;
* every instantiated Packet-B leaf is counted inside selected accounting root `zhao_texture_island_v3_top` with no contradictory excluded row, while unreachable candidates alone are excluded;
* only clean connected fits decide the comfortable-margin target below 30,000 ALMs and 85 DSPs.

### 14.1 Remaining HOLDs

No Packet-B ABI, arithmetic, AUX role, binding failure, quiet, manifest-disposition, or interface-manifest choice is left unspecified by this report. The remaining HOLDs are genuine implementation, upstream-product, integration, and measurement gates:

1. **Packet-A landing / Packet-B implementation-evidence HOLD:** Packet A must land green before any Packet-B implementation artifact changes. None of the versioned resolver/physical-descriptor/UV-join/quiet-leaf/dispatcher/AUX/combiner changes, owner instrumentation, new AUX-V2 or corrected combine/cache contract/`zref`, interface generator/checker/artifact, differentials, or committed mutants is implemented by this architecture edit. Every Packet-B gate must pass before Packet C may select its interface.
2. **Cache/memory HOLD:** Packet B specifies typed cache refusal, but the guard-denial terminal path and exact cache/fill accounting remain Packet E work; an invalid/denied fill must not be claimed drain-safe before that packet passes.
3. **Explicit material-ABI HOLD:** Packet B deterministically supports Packet A's common-UV/common-LOD/consecutive-selector seam, but connected sample counts 2/3 remain disabled until shipped material records prove that convention or a later packet carries explicit `MaterialSample[3]` selectors, generations, UV sets, and LODs. Full upstream resource-generation validation remains mandatory.
4. **AUX-consumer HOLD:** Packet B validates and accounts successful `{tag,strength}` but deliberately exposes no surface-effect result. A complete terrain surface-effect claim requires a separately typed owner-aligned consumer/output and end-to-end visual contract; AUX may never be smuggled in as sample 2 to clear this HOLD.
5. **Attribute/raster HOLD:** exact `invw24`, UV/W, color, world-X/Z, sheet handle, and envelope carriage, plus Packets C–E and G8A, must close before the raster seam is called connected.
6. **Lease/CDC HOLD:** writer-aware slot arbitration, captured base/span, clean-publish/fault-release behavior, accepted-ready CDC, generation-matched swap return, and reset barrier must pass Packet G before shell V2 can qualify.
7. **Shell/terrain HOLD:** `zhao_shell_top_v2` is not implemented or selected. Normals, projected-`w` depth quantization, owner-sealed terrain identity, and production-scale binner/tile scheduling must close before the parameter-fixed G8B and legal two-view end-to-end gate. The protected `zhao_shell_top.sv` remains unchanged.
8. **Fit/budget/adoption HOLD:** no ALM, DSP, M10K, Fmax, production connection, or physical saving is inferred. G8A, G8B, and G8C require their named clean connected receipts. Only Packet K may atomically select the connected hierarchy and mark TEXJOIN superseded after G8C demonstrates comfortable measured margin below 30,000 ALMs and 85 DSPs.

This report does **not** claim that Packet B RTL exists, that the current shell contains V3, that successful AUX changes a visible effect, that the current terrain candidate can feed the binner at production scale, that the Packet-A selector is the complete three-sample material ABI, that any new module fits, that the whole machine is under budget, or that retiring TEXJOIN saves physical silicon. Those are the gates, not the starting assumptions.
