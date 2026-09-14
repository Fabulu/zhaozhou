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
| raw-response routing and terminal collection | this report §3.5: the held cache record is steered directly into class processing, then `zhao_texture_rsp_dispatch_v2` is the four-input terminal 66-bit collector | any dispatcher placement before palette/bilerp/class processing, or any raw-response/index sidecar in the dispatcher |
| recoverable frame-fault ABI | this report §4.6: public quiet-qualified clear handshake, closed independently enabled owner/dispatcher/metajoin/counter-only baselines including `src_unpub`, same-edge set priority, and explicit reset-only lifetime table | an output-only sticky alias, clearing work/counters/config, or a clear which can erase RCP queue corruption |
| production-accounting disposition and role ownership | `design/prod_manifest.yml`, generated by/checked with `tools/quartus/gen_prod_top.py`, `tools/quartus/check_prod_manifest.py`, and `tools/quartus/check_ownership_roles.py` against the actual selected instantiation closure | prose labels or any second ownership registry/checker path |
| production V3 parameter profile | exact closed `design/prod_manifest.yml` member `production_parameter_overrides: {zhao_texture_island_v3_top: {MIGRATION_SHADOWS: 1'b0}}`, scalar-string/canonical-sized-literal parsing, `gen_prod_top.py`, and both production checkers; emitted accounting assignment is `#(.MIGRATION_SHADOWS(1'b0))` | module defaults, integers/Booleans/unsized literals, duplicate or unknown keys, type/representability drift, inference from the laboratory artifact, or a second profile registry |
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
input  logic                 frame_fault_clear_valid_i;
output logic                 frame_fault_clear_ready_o;
output logic                 frame_fault_o;
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
logical287_rsp       = descriptor_usable ? captured_physical320[286:0] : 287'b0
```

`descriptor_pad_ok` compares stored physical pad bits directly with constant zero; it is not compared with a second value captured by the same enable. The descriptor port is exactly `output logic [31:0] desc_pad_fault_o`, and `desc_read_accept = rd_valid_i && rd_ready_o`. Reset drives the counter to `32'd0`; on `desc_read_accept && (|captured_slice7[39:7])` it updates once as `desc_pad_fault_o <= desc_pad_fault_o + 32'd1`, modulo `2^32`, and otherwise holds. A stalled read response cannot increment it again because only the accepted descriptor-read edge is counted. The same event separately sets the sticky frame fault; the sticky bit is not an alias, saturation bit, clear enable, or clock enable for the counter, and clearing either frame state or the sticky fault does not clear `desc_pad_fault_o`. Only reset clears the counter. The response exposes no bit from a corrupt logical row: when unusable it sends only the independently held owner14 plus canonical-zero `logical287` into the unchanged 301-bit descriptor-to-UV channel, while separately presenting `descriptor_usable=0` to the top trust capture. On `desc_rsp_fire=desc_rsp_valid&&desc_rsp_ready`, the V3 top writes exactly `{descriptor_usable,response_owner_generation}` into its per-slot descriptor-trust sidecar. That sidecar write, not descriptor request acceptance or a stalled response, is the trust-capture event. Pad/generation failure therefore creates D=0 later; it does not immediately guess a mask, issue work, or synthesize completion. Section 3.5's owner-mask authority decides: valid owner-mask generation forces refusals for exact `own_adm_req`; invalid owner-mask generation is reset-lifetime. A legal write can never create nonzero pad; the detector's fire evidence is a committed descriptor mutant which drives exactly one `slice[7][39:7]` write bit high. Its inverse-polarity test requires a `desc_pad_fault_o` delta of exactly one, accepted response-sidecar capture with D=0, owner-mask-valid forced refusals for exact `owner_required_mask_m`, and a separate owner-mask-generation-invalid reset-barrier control. Separate ordinary tests assert all 33 stored/read pad bits stay zero, reset-to-zero, hold without an accepted bad-pad read, one increment despite output stalls, two distinct accepted bad-pad reads producing delta two, and `32'hFFFF_FFFF -> 32'h0000_0000` modulo wrap without changing sticky-fault semantics. Source/elaboration inventory proves `287 -> 320 -> 8*40` and all eight slice offsets.

The V2 descriptor has no allocator, head, tail, credit, or release cursor. It is subordinate owner-keyed storage. The expander transports its accepted row attached to the owner. Sheet handle, patch envelope, world position, page generation, and compatibility witnesses consequently stay immutable for the fragment lifetime without entering a second lifecycle.

#### Descriptor/UV join and page-generation carriage

The unversioned `zhao_texture_uv_join.sv` remains an old-island oracle and is not in Packet B's selected closure. Packet B adds `fpga/rtl/texture/zhao_texture_uv_join_v2.sv`. Its two accepted input records and one output record are exactly:

```text
desc input = {owner14, logical287}                 // 301 bits
UV input   = {owner14, U32, V32}                   // 78 bits
joined365  = {owner14, logical287, U32, V32}       // 365 bits
```

`descriptor_usable` does not widen these records. On the accepted descriptor response, the top writes usability plus that response's owner generation into the generation-tagged per-slot trust sidecar while the unchanged 301-bit descriptor record takes either usable logical287 or canonical zero into the join. The sidecar is read independently at joined owner; it is neither a second generation field in `joined365` nor a lifecycle queue.

There is exactly one binding-page-generation field in this path: `logical287[286:279]`. The descriptor input, V2 join, output hold, expander and resolver never append or carry a second copy. Each input has its own one-entry held register and ready/valid acceptance; neither register changes while occupied. The join may assert output valid only when both registers are occupied and their independently captured 14-bit owners match. The entire 365-bit output record is registered/held and remains bit-stable while `out_valid && !out_ready`; input replacement cannot alter any held output field.

A mismatching occupied pair is **reset-lifetime structural corruption**, not a recoverable frame sticky. The join consumes and discards both bad held inputs as one mismatch event, produces no `joined365`, asserts reset-sticky `uvjoin_owner_mismatch_o`, and accepts no further useful work. It does not invent a refused owner completion: either mismatched token could be false, so normal owner drain is not safe. The V3 top ORs this source into its lifetime structural fault, and Packet H immediately stops new fragment/write admission, enters the reset barrier, resets the island including `zhao_texture_v3own`, and releases the active lease without publication after the barrier's memory/write safety conditions. Neither `frame_fault_clear_i` nor owner-counter baseline capture may clear or baseline this source, and no gate may claim ordinary `quiet_o` drain or a normal frame-clear handshake after it fires. Only reset clears it.

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

Every source status is eight bits. The final status is exactly the bitwise OR of the 8-bit statuses from **committed required TMU planes and the committed required AUX plane**, plus `{7'b0,material_refused}`. Unrequired planes and uncommitted RAM contents contribute nothing. `status[0]` is `SOURCE_REFUSED`; bits `[7:1]` are reserved-zero at current Packet-B producers, but every queue, collector, owner plane, and reduction preserves and ORs all eight bits so later typed status cannot be narrowed. `out_refused_o = out_status_o[0]`. All eight 3-bit recipe encodings are assigned; only a legal-count mismatch sets `material_refused`. Any nonzero final/source status or count refusal raises the recoverable frame fault. The terminal visible error value is the existing loud sample value `RGB=24'hFF00FF, A=8'hFF`, never admitted base colour or a successful partial recipe; raw index still follows the sample-0 rule below. A refused result remains terminal and handshakes normally so the owner releases and the shell can release, rather than publish, the frame lease.

The immutable owner-keyed material row is exactly **46 bits**, low field first:

| bits | width | field |
|---|---:|---|
| `[0]` | 1 | `aux_required` |
| `[2:1]` | 2 | sample count |
| `[5:3]` | 3 | recipe ID |
| `[13:6]` | 8 | recipe weight, unit8 raw/256 |
| `[21:14]` | 8 | admitted base alpha |
| `[45:22]` | 24 | admitted base RGB |

The V3 top—not `zhao_texture_v3own`—owns subordinate per-slot material arrays: exact row46, one separate stored `material_refused` bit, and one separate stored material-owner generation (`GENW`, eight bits selected). It also owns the authoritative `owner_required_mask_m[slot][3:0]` array and its separate owner-mask generation array. All five values are written together only on `own_adm_accept`, under the newly allocated slot/generation; `owner_required_mask_m` captures the exact four-bit `own_adm_req` presented on that admission, without recomputing count/AUX. The owner's internal obligation mask still captures the same existing admission request through its unchanged functional port. Stored `material_refused` is exactly the admission-time legal-count mismatch bit. Every 3-bit recipe value names one of IDs 0–7, so there is no unknown-recipe malformed case. Payload RAM is not reset; generation seals each copy's identity.

The top also owns a subordinate descriptor-trust sidecar with exactly two per-slot fields: captured `descriptor_usable` and captured owner generation. It writes both together only on the accepted descriptor-response handshake, using that response's owner slot/generation; it does not write on descriptor request or a merely offered/stalled response. The sidecar has no allocator, valid bit, queue, or lifecycle cursor. Its stored generation is compared later with the independently joined owner generation, so stale payload is harmless.

At a joined owner, the top independently reads the authoritative owner-mask store, the material fields needed only to validate its copy, and the descriptor-trust sidecar. Define exactly:

```text
O = (stored_owner_mask_generation == joined_owner_generation)
M = (stored_material_owner_generation == joined_owner_generation)
D = captured_descriptor_usable
 && (captured_descriptor_owner_generation == joined_owner_generation)
owner_mask      = owner_required_mask_m[joined_owner.slot]
material_mask   = required_mask(row46.sample_count, row46.aux_required)
descriptor_mask = required_mask(logical287.sample_count, logical287.aux_required)
copies_match_owner = M && D
                  && (material_mask == owner_mask)
                  && (descriptor_mask == owner_mask)
```

The owner-mask generation is the trust root and the disposition is closed:

| O | copy condition | disposition |
|---:|---|---|
| 1 | `copies_match_owner` and stored `material_refused==0` | normal expansion using `owner_mask` |
| 1 | either copy invalid, either copy mask differs from `owner_mask`, or stored `material_refused==1` | use `owner_mask`; force every required source to typed terminal refusal; set recoverable identity/count frame fault |
| 0 | any M/D/mask values | reset-lifetime owner-identity fault; do not feed expander; enter reset barrier |

Thus descriptor and material masks are independently compared with the admission-captured owner mask; they do not vote with each other. As long as O is valid, the top **always uses `owner_mask`**, even when both copies are invalid or both valid copies disagree. The forced path captures exactly that authoritative mask plus `force_all_required_refused=1` into the existing `u_expand` held fragment record; no planner, cache, palette, or Sheet request is permitted, and every owner-required source receives logical issue followed by typed refusal. Only invalid owner-mask generation is reset-lifetime and feeds no expander input.

An empty O mask has no source plane on which expansion can carry refusal, and the owner otherwise becomes combine-ready immediately. The top therefore fences every owner combine ticket behind a generation-tagged joined-validation result. For O=`0000`, a bad descriptor/material copy goes directly through the second-read canonical malformed path and emits loud `SOURCE_REFUSED` without TMU/AUX issue; it cannot retire admitted base colour cleanly before descriptor validation. The pending bit is reset state, so generation wrap cannot mistake an old validation payload for the new owner.

**Row46 does not travel through `zhao_texture_frag_expand_v2`.** The first material read exists only to calculate M/material-mask/count-copy agreement against the owner authority. Expansion receives owner mask and force-refusal control, not recipe, weight, base RGB/A, row46, descriptor/material trust bits, or either copied mask. The reservation lands only those controls in the expander's existing held fragment record, already observed by `q_expand_frag_valid`/`q_expand_idle`; there is no separate top material/owner-mask/trust response valid, queue, or 72nd quiet operand.

At owner combine admission, the top performs a **second**, generation-checked material-array read through a tagged, credit-reserved synchronous pipeline/FIFO. It accepts at most one valid owner combine ticket per clock under ready; acceptance reserves the RAM-result stage and FIFO entry before issuing the read. Every stage and FIFO entry preserves the immutable `{owner14,row46,stored_material_generation}` tuple and holds it under backpressure. The top compares or indexes the returned owner only when that pipeline output is valid; it never reads an invalid output-owner bus. All pipeline/FIFO valid bits, credits, and held output reduce only into `material_read_idle_w`; arithmetic-leaf state reduces only into `combine_leaf_idle_w`. The one existing quiet operand is their explicit conjunction `q_combine_idle`, with request/output valids still covered by `q_combine_req_valid`/`q_combine_rsp_valid`; no 72nd term is added.

A valid second read whose material generation and row-derived mask both agree with the valid authoritative owner-mask store supplies row46 plus the stored count-only `material_refused` bit to arithmetic. If owner-mask generation is valid but the second material read is invalid or its mask disagrees, the pipeline derives canonical malformed control from authoritative O and reads exactly the planes named by `owner_mask`, so required-source status and sample-0 index survive:

```text
canonical_sample_count = decode_prefix_count(owner_mask[2:0]) // 000->0,001->1,011->2,111->3
aux_required           = owner_mask[3]
sample_count           = canonical_sample_count
recipe_id              = (canonical_sample_count == 2) ? 3'd0 : 3'd1
                         // PASSTHRU is illegal for count2; MODULATE is illegal for count0/1/3
recipe_weight          = 8'd0
admitted_base_a        = 8'd0
admitted_base_rgb      = 24'd0
material_refused       = 1'b1
source_plane_read_mask = owner_mask
```

That canonical path is recoverable because the generation-valid owner mask remains authoritative; it sets frame fault, is J1, and emits the loud terminal result with final status exactly `{7'b0,material_refused}` OR committed required source statuses. It reads every and only owner-required plane, preserves their 8-bit statuses and sample-0 index, and reads no stale row bit or live fragment input. If owner-mask generation is invalid at combine admission, trust root is gone: no canonical/normal combine job is admitted and reset-lifetime barrier recovery begins. Stored `material_refused` still means only admission count mismatch; the canonical bit above is freshly derived from the canonical count/recipe pair, not a rewrite of storage.

The second-read usable or canonical diagnostic material record remains immutable after combiner admission. No recipe, count, base, AUX bit, refusal, or generation may be reread from live fragment pins. The owner's admitted `required_mask` remains its commit/ready authority through the existing admission interface, but no material payload, trust bit, generation, read port, or selected mask is added to `zhao_texture_v3own`; Packet B makes no owner functional-port change. The retained Mosaic A/B/weight bytes in the early descriptor remain Mosaic inputs only and cannot override recipe, recipe weight, or admitted base RGB/A in the top-owned material row.

Packet B's compatibility ABI has no Mosaic-enable bit or independently typed terrain material pair. Therefore `u_mosaic` remains an **observation-only, counter/quiet-accounted probe** in this packet: its completed tile/tx/ty/source tuple does not override the sealed binding selector, and wiring it from base-colour bytes would randomize every non-terrain fragment. No Packet-B or standalone-V3 fit may price this prunable probe as future functional Mosaic hardware. The typed terrain/raster seam and its connected characterization remain HOLD for Packet D/G8A.

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
* Final status is the bitwise OR of the full 8-bit statuses from only committed required TMU planes and the committed required AUX plane, plus `{7'b0,material_refused}`. Unrequested planes and uncommitted RAM contents are never consulted; every one of recipe IDs 0–7 is assigned, so `material_refused` means only legal-count mismatch.
* Final raw index is zero at count zero and otherwise exactly committed `sample0.raw_index`. Samples 1/2, AUX, recipe arithmetic, and palette RGB never replace it.
* A wrong-generation return, out-of-range sample, unrequested source, return-before-issue, or duplicate return enters only the owner's typed protocol-fault handling. It marks the frame fatal but does not commit a plane, satisfy a required bit, overwrite a result, or increment a committed-source count. A later correct required return remains owed.

#### Metajoin illegal-index terminal law

Legal TMU sample indices are 0, 1, and 2. If a held raw cache response reaches `zhao_texture_metajoin_v2` with sample index 3, the metajoin accepts and consumes that raw response into a drop sink on the handshake edge, launches **no** pending metadata/descriptor read, emits no class work or owner return, and latches reset-lifetime `metajoin_illegal_sidx3`. Because the claimed owner cannot safely be completed, Packet H stops admission/writes and enters the island/owner reset barrier; no baseline, normal quiet drain, or frame clear applies. The metajoin generation-mismatch counter remains a separate recoverable counter-baselined source. Tests count one raw-response consumption, zero pending reads, zero class/owner returns, and one lifetime fault even under stalls; a mutant which launches the read or treats sidx3 as a recoverable refusal fails.

#### Raw-response steering, terminal collection, and index alignment

`zhao_texture_rsp_dispatch_v2` is frozen **after all class processing**. It is
not a cache-response router. While the versioned cache holds its complete raw
response record stable, top-level ready/valid steering decodes
`route_token[17:16]` and offers that same held record directly to exactly one of
the CLUT, NEAR, BIL, or ERR class-processing paths. Only the selected path may
assert cache-response ready. CLUT palette lookup, direct-nearest formatting,
Mosaic/bilerp work, and every palette/bilerp request and response therefore
occur before the dispatcher. There is no raw-data, metadata, token, or index
sidecar in the dispatcher and no dispatcher state spanning cache input to class
completion.

The synchronous metadata result is a fall-through skid: when the selected class
can accept, the live result and still-held cache response handshake directly;
only a stalled result enters the registered metadata hold. The cache therefore
accepts a hot response every two clocks (metadata launch, then result/accept), not
every three clocks through a mandatory capture-then-release bubble. A committed
three-sample hot-line test requires exact two-clock metadata-result spacing. An
independent sticky boundary detector snapshots a stalled cache data/token record;
change or disappearance before acceptance sets reset-lifetime
`err_rsp_dropped_o`, blocks admissions/writes, survives frame clear, and requires
reset. Its committed observation mutant changes only the detector's boundary view
while the cache is stalled, proving the instrument can fire.

Each class processor terminates in its own held ready/valid offer carrying the
same immutable **66-bit** law:

```text
{route_token18, status8, raw_index8, alpha8, RGB24}
```

The BIL path retains the original expected route token from its accepted request through all lane work. If a bilerp result returns with a different token or on the wrong lane/channel, the class processor discards that colour, keeps the original expected token, and offers exactly `{expected_route_token18,{7'b0,SOURCE_REFUSED},8'h00,8'hFF,24'hFF00FF}` on the held BIL terminal channel. It sets the recoverable bilerp token/channel fault and never emits a clean result under the returned wrong token. The mismatch terminal then follows ordinary collector/owner commit and drain; a later correct result for that already-disposed expected ticket is duplicate protocol traffic, never success.

The four inputs are ordered `{ERR,BIL,NEAR,CLUT}`. `zhao_texture_rsp_dispatch_v2`
is exactly a four-input/one-output terminal collector: it has one independent
held entry per input class, may accept all four class completions on the same
clock, exposes those four occupied bits in fixed `{ERR,BIL,NEAR,CLUT}` order,
and fairly arbitrates them into one held owner-return output. Its round-robin
state advances only when that held output is accepted; output payload and owner
remain bit-stable under owner backpressure. On every class-input acceptance it
independently checks that `route_token.class` equals the physical input class. A
mismatch sets the protocol/frame fault, but the collector neither rewrites nor
drops the tuple: all 66 accepted bits remain bit-for-bit the owner return, so the
fault cannot strand a legitimately named owner and the frame cannot publish.

The top-visible aliases are semantic commitments:

```text
q_dispatch_req_valid = |class_terminal_offer_valid_w[3:0]
q_class_rsp_valid    = dispatch_pending_w[3:0]  // {ERR,BIL,NEAR,CLUT}
q_tmu_return_valid   = dispatch_return_valid_w  // collector held output
```

Thus `q_dispatch_req_valid` is the OR of the four final-class offers into the
collector, not the raw cache response; `q_class_rsp_valid` is the collector's
four pending-entry bits, not class-processor output wires; and
`q_tmu_return_valid` is the collector output offered to `u_own`. `q_cache_rsp_valid`,
all Mosaic/bilerp/palette request/response aliases, and their idle terms describe
work earlier in the flow.

For CLUT8, `raw_index` is the addressed byte; for CLUT4 it is the selected nibble
zero-extended. It is captured beside the route token from the same accepted raw
cache record and remains beside it through palette request, palette response,
class-output hold, collector input hold, arbitration, and owner backpressure.
The palette path carries the original token, index, status, and alpha with the
resulting RGB. Direct-colour and locally refused/error results use index zero.
No stage may reconstruct an index from a live planner address, current metadata
row, palette output, currently offered request, or dispatcher sidecar.

`zhao_texture_material_combine_v3` consumes typed 48-bit owner planes, applies
exactly R9, preserves sample-0 index, and performs required-status OR. Its one
physical paired datapath issues at most one phase each clock. J1/J2/J3 and
`phase_demand=J1+2*J2+3*J3` in section 9.2 are the cadence authority; there is no
one-multi-phase-job-per-clock claim.

Packet B amends `design/contracts/TEXTURE.COMBINE.md` and `reference/include/zref/zref_material.hpp`, creates the normative `design/contracts/TEXTURE.AUX.V2.md`, and changes the new V3 combiner/AUX adapter, terminal collector/class paths, and tests in the same packet. It leaves oracle-only `design/contracts/TEXTURE.AUX.md` and the unversioned old-island leaves unchanged. It amends the cache contract for typed terminal refusal/accounting used by Packet E. Agreement with the old unversioned combiner, raw dispatcher, or AUX contract does not override R9, this post-class collector seam, or AUX V2.

Compatibility is exact and narrow: on PASSTHRU count 1 without AUX and without refusal, old and V3 RGB/A/order/refused must match, and `out_tag_o` remains `frag_ctx_i[15:0]`. On count zero, recipes 1–7, AUX-bearing work, or any refused source, the R9/new-status contract is authoritative and an old-island mismatch is expected evidence of the superseded law, not a reason to copy it.

### 3.6 Ordered output join and sequence-abort drain

There is no associative join. V3 returns the complete result and continuation together in admission order. In the normal state the mapping is direct; a mismatch has an explicit terminal drain path rather than a ready/valid deadlock:

```text
sequence_mismatch  = v3.out_valid && !sequence_matches && !sequence_abort
fragment.valid     = v3.out_valid && sequence_matches && !sequence_abort
v3.out_ready       = sequence_abort
                  || sequence_mismatch
                  || (fragment.ready && sequence_matches)
drop_fire          = v3.out_valid && (sequence_abort || sequence_mismatch)

fragment.addr      = returned.addr
fragment.depth     = returned.invw24
fragment.state     = returned.state
fragment.source_id = returned.source_id
fragment.vertex    = returned vertex RGB/A
fragment.tag       = returned effect tag
fragment.stencil   = returned stencil reference
fragment.texel     = returned RGB/A/raw-index
```

An independent expected-retirement sequence register advances only on a normal downstream fragment handshake. It is compared with the owner-carried admission sequence. The first mismatching held output combinationally suppresses same-edge candidate admission, sets the recoverable frame/lease fault, and latches `sequence_abort`; the mismatch beat is accepted by V3 as `drop_fire` on that edge, never asserted to `RASTER.FRAGMENT`, and therefore can never write TILESTORE. The carried value and expected value have different clock enables—admission versus external retirement—so a held-output timing defect cannot move both operands together and hide itself.

While `sequence_abort` is latched, the raster texture stage stops every new V3 candidate admission, suppresses every fragment write regardless of later sequence values, holds the expected sequence unchanged, and keeps `v3.out_ready=1` so every already-admitted ordered output is consumed and dropped. Each `drop_fire` increments a reset-zero, modulo-`2^32` `sequence_drop_count`; frame windows are bounded below wrap and frame clear does not reset the counter. Because each dropped V3 output handshakes, `zhao_texture_v3own` releases every remaining owner and ordinary V3/data-path drain can complete. After the complete writer drain, Packet H emits RELEASE for the faulted lease and must emit no publication. `sequence_abort` is clearable per-frame state only at the next accepted quiet frame-clear handshake.

The sequence comparator is an instrument, not the ordering mechanism. V3's owner cursor is the mechanism. Simulation independently scoreboards all 160 continuation bits and the complete result, so a correct sequence with a corrupted source ID or depth still fails. The positive control corrupts one carried sequence with later owners already complete and requires: zero fragment writes from the mismatching beat onward, one drop for that beat plus every remaining ordered output, owner emitted/released closure, finite quiet drain, one lease RELEASE, and zero publication. A committed old-gate mutant retaining `v3.out_ready=fragment.ready&&sequence_matches` must strand the mismatched head and fail the drain/drop/release checks.

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
* the `frame_fault_clear_valid_i` request remains asserted until its `frame_fault_clear_ready_o` acceptance at full public quiet;
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
| `q_metajoin_idle` | `metajoin_idle_w`, driven only by `u_metajoin.idle_o` | the one retained metadata-read result and its held response state; this leaf does not own two input holds |
| `q_desc_idle` | `desc_idle_w`, driven only by `u_early_desc.idle_o` | accepted descriptor read, eight captured slices, pad/generation verdict, and held response |
| `q_uvjoin_idle` | `uvjoin_idle_w`, driven only by `u_uv_join.idle_o` | descriptor input hold, UV input hold, and 365-bit output hold |
| `q_expand_idle` | `expand_idle_w`, driven only by `u_expand.idle_o` | fragment work including selected trusted required mask and force-all-refused control (never row46), sample work, AUX work, and local malformed-descriptor/count-refusal disposition |
| `q_bind_idle` | `binding_data_idle_w`, driven only by `u_binding.data_idle_o` | lookup request/read result, pending logical issue disposition, planner offer, and local-refusal offer; configuration state is deliberately separate |
| `q_plan_idle` | `plan_idle_w`, driven only by `u_plan.idle_o` | every planner elastic stage and held cache request |
| `q_cache_idle` | `cache_idle_w`, driven only by `u_cache.idle_o` | access/hit/response stage, blocking miss, fill request, accepted fill awaiting verdict/data, partial fill, and held terminal response |
| `q_dispatch_idle` | `dispatch_idle_w`, driven only by `u_dispatch.idle_o` | four independent terminal-class held entries, round-robin selection state insofar as it owns valid work, and the held owner-return output; no raw cache sidecar or pre-class state |
| `q_mosaic_idle` | `mosaic_idle_w`, driven only by `u_mosaic.idle_o` | all Mosaic stages and held result |
| `q_bilerp_idle` | `&bilerp_lane_idle_w[3:0]`, each bit driven only by the corresponding `u_bilerp[*].idle_o` | every versioned bilerp lane's channel/pipeline/result state |
| `q_palette_idle` | `palette_idle_w`, driven only by `u_palette.idle_o` | palette lookup, generation verdict, read latency, and held 66-bit response tuple |
| `q_palette_cfg_idle` | `palette_cfg_idle_w`, driven only by `u_palette.cfg_idle_o` | no accepted palette programming operation in flight; Packet B defines no acknowledgement channel |
| `q_aux_idle` | `aux_idle_w`, driven only by `u_aux.idle_o` | arithmetic/divider, offer FIFO, issued-identity FIFO, owed Sheet response, local refusal, typed return, and all reserved credits |
| `q_combine_idle` | explicit alias expression `material_read_idle_w && combine_leaf_idle_w`; `combine_leaf_idle_w` is driven only by `u_combine.idle_o` | top-owned tagged/credit-reserved synchronous material-read pipeline/FIFO is empty and the combine leaf has no admitted context, runnable phase, scratch/payload/completion read, arithmetic/writeback, continuation, completion, or held result |

Packet B uses versioned successors wherever the unchanged shared leaf does not already expose this complete observation: `zhao_raster_rcp24_v4`, `zhao_raster_perspuv_pairpipe_v2`, `zhao_texture_metajoin_v2`, `zhao_texture_uv_join_v2`, `zhao_texture_tmu_plan_v2`, `zhao_texture_cache_pipe_v2`, `zhao_texture_mosaic_v2`, `zhao_texture_bilerp_lane_v2`, and `zhao_texture_palette_res_v2`. The already-new descriptor, expander, resolver, dispatcher, AUX V2, and combiner V3 expose the named idle/data-idle ports in their first version. These observation ports may be reductions of existing state only; they may not add a queue, credit, lifecycle transition, or combinational ready path. The corresponding unversioned leaves remain byte-for-byte in the old executable-island closure. `zhao_texture_v3bank`, descriptor RAM, and top-owned material RAM payload bits are not “busy” merely because unreset stale data exists; their independently owned valid/occupancy state is covered by owner/descriptor/UV/expander/combine terms. Material and descriptor-trust sidecar payload bits are stale data, not work; first-read control lands only in the expander hold and second-read row/canonical control lands only in the combiner hold, so no separate top material/trust valid exists. Any stateful helper below a listed instance, including an AUX divider, is included in that instance's `idle_o` and cannot disappear from the equation.

The top also declares these exact one-bit channel aliases. They are assertions/checker inputs as well as equation operands, so a held boundary cannot be hidden inside a broad idle label:

| alias | exact top-visible source | represented channel |
|---|---|---|
| `q_frag_offer_valid` | `frag_valid_i` | top fragment offer, whether or not admission is enabled |
| `q_owner_claim_valid`, `q_owner_ready_valid`, `q_owner_combine_valid`, `q_owner_final_valid` | `owner_claim_valid_w` driven only by `u_own.obs_claim_valid_o`, `owner_ready_valid_w` driven only by `u_own.obs_ready_valid_o`, then `owner_combine_valid_w`, `owner_final_valid_w` from their existing owner interconnects | observation-only owner claim, ready-ticket, combine-ticket, and final-result valids |
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
| `q_cache_rsp_valid` | `cache_rsp_valid_w` | held raw cache terminal response before direct class steering |
| `q_mosaic_req_valid`, `q_mosaic_rsp_valid` | `mosaic_req_valid_w`, `mosaic_rsp_valid_w` | Mosaic request and result valids before terminal collection |
| `q_bilerp_req_valid[3:0]`, `q_bilerp_rsp_valid[3:0]` | `bilerp_req_valid_w[3:0]`, `bilerp_rsp_valid_w[3:0]`, lane index ascending 0..3 | request/result valid for each bilerp lane before terminal collection |
| `q_palette_req_valid`, `q_palette_rsp_valid` | `palette_req_valid_w`, `palette_rsp_valid_w` | palette request and held 66-bit class-result valid before terminal collection |
| `q_palette_cfg_valid`, `q_palette_cfg_rsp_valid` | `pal_load_valid_i`, literal `1'b0` | palette programming command; Packet B has no acknowledgement channel |
| `q_dispatch_req_valid` | `|class_terminal_offer_valid_w[3:0]`, whose vector order is `{ERR,BIL,NEAR,CLUT}` | OR/top-visible final-class offer into the terminal collector; never the raw cache response |
| `q_class_rsp_valid[3:0]` | `dispatch_pending_w[3:0]`, driven only by `u_dispatch.pending_valid_o[3:0]` in fixed `{ERR,BIL,NEAR,CLUT}` order | the collector's four independent occupied-entry bits, not class-processor output wires |
| `q_tmu_return_valid` | `dispatch_return_valid_w`, driven only by `u_dispatch.out_valid_o` | collector's held typed sample return offered to `u_own` |
| `q_aux_req_valid` | `aux_job_valid_w` | logical AUX job valid offered to `u_aux` |
| `q_sheet_req_valid` | `sheet_req_valid_w`, driven only by `u_aux.req_valid_o` | Surface Sheet READ request valid |
| `q_sheet_rsp_owed` | `aux_sheet_rsp_owed_w`, driven only by `u_aux.sheet_rsp_owed_o` | at least one accepted Sheet request still owes its response |
| `q_sheet_rsp_valid` | `pg_valid_i` | external Sheet response physically presented, including unsolicited/malformed traffic |
| `q_aux_refuse_valid`, `q_aux_return_valid` | `aux_refuse_valid_w`, `aux_return_valid_w` | local AUX refusal and typed AUX return valids |
| `q_combine_req_valid`, `q_combine_rsp_valid` | `combine_req_valid_w`, `combine_rsp_valid_w` | combine-job and completed-result valids |
| `q_retire_valid` | `out_valid_o` | top ordered retirement valid |
| `q_cfg_cmd_valid` | `cfg_valid_i` | top binding-config command valid |
| `q_cfg_rsp_valid` | `cfg_rsp_valid_o` | held top binding-config response valid |

Packet B authorizes exactly two new observation-only ports on `zhao_texture_v3own`:

```systemverilog
assign obs_claim_valid_o = c1t_v_q || c1a_v_q;
assign obs_ready_valid_o = q0t_v_q || q0a_v_q || q0i_v_q;
```

They are pure combinational views of existing registered valid bits. They add no state, register enable, ready dependency, credit, arbitration, lifecycle transition, or functional owner interface. `q_owner_claim_valid` and `q_owner_ready_valid` are driven only from these ports; hierarchical reads of the five private `_q` bits are forbidden. Adding the ports changes source observability but preserves the 71 equation operands and their polarity.

`fill_req_valid_o` and `pal_load_valid_i` are also exact **new versioned-top ABI names** frozen by Packet B. The quiet map retains those spellings. They must not be silently replaced by the historical top's port names; the old top and its old interface remain unchanged and distinguishable.

Every `*_w` above is a named `logic` declared in `zhao_texture_island_v3_top` and is the same physical interconnect connected to the producing/consuming module port; it is not a recomputed busy guess. The five explicitly declared expression aliases are `&bilerp_lane_idle_w[3:0]`, `fill_data_valid_i || fill_refused_i`, `|class_terminal_offer_valid_w[3:0]`, `material_read_idle_w && combine_leaf_idle_w`, and the literal-zero palette response placeholder. `material_read_idle_w` is true only when material-read ingress, synchronous read stage, result FIFO, held output, and all reserved read/result credits are empty; `combine_leaf_idle_w` observes only the arithmetic leaf. Each side is independently driven and tested, while their conjunction remains the single existing `q_combine_idle` operand, so the public law stays 60+11. Every other `q_*` is a direct continuous alias of exactly one named port/interconnect. In particular, the dispatch OR reads only the four held final-class offer wires and `q_class_rsp_valid` reads only `u_dispatch.pending_valid_o`; neither may alias `cache_rsp_valid_w`. Hierarchical references to a child's `_q`, pointer, occupancy, or generate-local signal are forbidden.

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
 && !q_mosaic_req_valid
 && !q_mosaic_rsp_valid
 && !(|q_bilerp_req_valid[3:0])
 && !(|q_bilerp_rsp_valid[3:0])
 && !q_palette_req_valid
 && !q_palette_rsp_valid
 && !q_dispatch_req_valid
 && !(|q_class_rsp_valid[3:0])
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

`q_fill_rsp_valid` and `q_sheet_rsp_valid` are excluded from `data_quiet` because unsolicited external offers are not accepted island obligations and must not deadlock activation; they are included in public `quiet_o` so the island never advertises quiet while a response is physically presented. Likewise `q_frag_offer_valid`, binding/palette configuration command/response state, and `binding_seal_pending` are public-quiet terms but not old-work drain terms. Packet B adds no held palette acknowledgement, so `q_palette_cfg_rsp_valid` is a literal-zero structural placeholder; retaining the named operand means a later version which adds an acknowledgement cannot silently escape the checker. The binding activator uses `data_quiet`, not `quiet_o`: a successful END stops new fragment admission, existing accepted work drains, and the active bank/generation changes atomically on the first `data_quiet` edge. That edge clears `binding_seal_pending`, changes the loader to IDLE, and creates the held END response. Public quiet therefore remains low until that response is accepted and every external offer is absent; there is no circular wait. Excluding physical `fill_refused_i` from `data_quiet` is not permission to ignore it: before Packet E it latches the unsupported-refusal lifetime fault and invokes the barrier; after Packet E its accepted typed disposition remains represented by cache/owner state until ordinary drain.

Flattened, `data_quiet` has exactly 60 named Boolean operands and `quiet_o` adds exactly 11 public/configuration operands, for the frozen **71-term public-quiet law**. Repositioning dispatch after class processing changes the meanings and sources of `q_dispatch_req_valid`, `q_class_rsp_valid`, and `q_tmu_return_valid` above, not this count. Neither `frame_fault_clear_valid_i` nor `frame_fault_o` is a quiet operand: the 71-term equation is deliberately independent of both clear request and fault state.

For `zhao_raster_tile_pipe_v2`, outer `pipe_empty` additionally requires:

```text
no Early-Z candidate held
candidate skid level == 0
attribute producer has no covered-pixel record held
V3 input has no unaccepted candidate
V3 quiet_o == 1
RASTER.FRAGMENT idle
```

The swap condition remains `walk done && pending mask empty && no new coverage accept && pipe_empty`, followed by the existing resolve-ready gate. The next non-final triangle for the same tile also waits for this drain in the minimum version. Inter-triangle texture overlap is an optimization only after this composition meets its measured frame budget. `sequence_abort` is a latched frame-control state, not a 72nd V3 quiet operand or an occupancy which may prevent its own drain: while set it overrides candidate admission, fragment write, tile-bank swap, resolve publication, and frame publication, but permits V3 ordered drops until `quiet_o`. Packet H then RELEASES the lease; only the next accepted frame clear removes abort.

Quiet verification is a three-part composition, not a claim that top stimulus can directly park every internal term:

1. **Static top mapping and polarity:** parse the exact 60+11 expressions, prove every table operand has its declared source exactly once with the frozen polarity, prove the two owner observation equations and dispatcher `pending_valid_o` mapping, and run one committed omission/wrong-polarity/source-alias mutant for each of the 71 operands. `quiet_o=q_owner_idle`, owner-private hierarchical reads, material/trust extra valid state, and omitted public channels must fail statically.
2. **Leaf idle positive controls:** each listed leaf's own bench must drive every internal valid/occupancy/credit/held-result class that its `idle_o` promises—including metajoin's one retained read result, expander selected-mask/force hold, material-read pipeline/FIFO busy with combine leaf idle and combine leaf busy with material-read idle, and AUX issued-response credit—and observe `idle_o=0`, then drain and observe 1. A leaf idle detector is not credited until each internal class fires legally or through its committed unreachable-state mutant.
3. **Public-channel runtime:** the V3-top bench directly stalls/asserts only publicly controllable channels (`frag`, fill data/refusal, Sheet response, palette load, binding config/response ready, ordered-output ready) and checks the corresponding public aliases, payload hold, `quiet_o=0`, then complete drain to `quiet_o=1`. End-to-end legal traffic may cover internal terms collectively, but the report does not claim top stimulus isolated every private pipeline bit.

The source-map checker and mutant suite provide exhaustive 71-term coverage; leaf controls validate each aggregate idle instrument; public runtime validates the composed boundary. A zero occupancy/idle indication is not evidence until its own positive control or committed mutant fires. Lifetime-corruption cases remain outside normal quiet recovery.

### 4.6 Canonical recoverable frame-fault ABI

The versioned V3 top has one public recoverable-frame handshake and one public
fault result:

```systemverilog
input  logic frame_fault_clear_valid_i;
output logic frame_fault_clear_ready_o;
output logic frame_fault_o;

assign frame_fault_clear_ready_o = quiet_o;
clear_fire = frame_fault_clear_valid_i && frame_fault_clear_ready_o;
```

`quiet_o` is exactly the 71-term law in section 4.5 and is independent of
`frame_fault_clear_valid_i`, `clear_fire`, and every recoverable or lifetime fault
bit. Clear-ready is not `quiet_o && !frame_fault_o`, and fault state does not
hold quiet low. Conversely, clear acceptance is possible only at full public
quiet: a top fragment/configuration offer, a held response, or an unsolicited
external fill/Sheet response makes `quiet_o=0`, so a held clear request cannot be
accepted through that traffic.

On `clear_fire`, top-local `frame_fault_clear_w` is broadcast as `frame_fault_clear_i` only to versioned children whose interface explicitly declares a clearable per-frame sticky. The same edge clears the top's own recoverable sticky state and captures current baselines for the closed set of counter-only recoverable sources below. A child without the declared clear input is never cleared by implication.

The frame-fault classification is exact:

| class | exact sources | clear/recovery law |
|---|---|---|
| recoverable, direct sticky/event | committed required TMU or AUX 8-bit status nonzero; typed local/cache/class/binding/material/AUX refusal; descriptor pad or generation fault; generation-valid owner-mask with either descriptor/material copy invalid or mismatched; canonical second material-read substitution while owner-mask generation remains valid; AUX envelope/Sheet/credit/protocol fault; binding row/generation/witness/selector/configuration fault; count-only `material_refused`; bilerp token/channel mismatch with expected-token refusal; dispatcher class-mismatch and metajoin generation-mismatch checks where their declared sticky/event fires in addition to a counter; every other child sticky explicitly declared clearable in its versioned interface | sets the top recoverable sticky even if no ordered result exists; `fault_set` has priority over `clear_fire`; only the explicitly declared child sticky clears |
| recoverable, counter-baselined | owner `range`, `stale`, `unsolicited`, `duplicate`, `final`, `issue`, and `src_unpub`; dispatcher class mismatch; metajoin generation mismatch; every additional source which the Packet-B source inventory explicitly classifies as counter-only recoverable | live monotonic counter never clears; separate baseline captures only on `clear_fire`; any live/baseline inequality sets `frame_fault_o` |
| reset-lifetime structural | RCP `qerr`; expander `wq_overflow`; V2 UV-join owner mismatch; invalid authoritative owner-mask generation at joined or combine admission; metajoin illegal sample index 3; a cache response that changes or disappears while stalled (`err_rsp_dropped_o`); before Packet E only, any assertion of ABI-reserved `fill_refused_i`, because the Packet-B cache cannot yet terminate it | never baselined and never connected to `frame_fault_clear_i`; remains asserted until island reset and forces Packet H's reset barrier, owner/island reset, lease RELEASE, and zero publication; Packet E replaces only the unsupported-fill-refusal case with typed recoverable completion |
| shell-local recoverable frame abort | ordered retirement sequence mismatch and its latched `sequence_abort` | stops admission/writes, drains and counts all ordered V3 drops, then RELEASE; clears only for the next accepted quiet frame handshake after the completed abort drain |

The counter-only row is closed by source-inventory set equality: every monotonic counter used as the sole indication of a recoverable fault must appear in the baseline vector, and every baseline operand must resolve to one listed live counter. A new counter-only fault is therefore a schema/table change, not something hidden under “other protocol.” The seven owner families include `src_unpub`; omitting it is a hard checker failure. Dispatcher class mismatch and metajoin generation mismatch remain baselined even when an implementation also supplies a direct set pulse, so a lost pulse cannot erase the frame fault.

Live counters and baselines are independently enabled: each live counter changes only on its own detected event, while its baseline changes only on `clear_fire`. Counters remain modulo `2^32`, and every accepted-frame observation window is bounded to fewer than `2^32` events. Clear never resets or rewrites a live counter. A same-edge live increment and baseline capture leaves the baseline at the pre-increment value and therefore leaves a visible delta.

A clear affects **only** the explicitly clearable per-frame stickies, top recoverable sticky, sequence-abort state after its completed drain, and recoverable comparison baselines. It never clears an owner, valid bit, queue entry, credit, work record, accepted/completed/drop counter, palette/binding configuration, page generation, cache line, lease state, RCP `qerr`, expander `wq_overflow`, UV-owner mismatch, invalid authoritative owner-mask identity, metajoin sidx3, cache-response hold/drop state, or pre-Packet-E unsupported-fill-refusal lifetime state. For every clearable bit the sequential priority is `fault_set` over `clear_fire`.

The complete V3-top result is:

```text
recoverable_counter_delta = any listed live counter != its baseline
lifetime_structural_fault = rcp_qerr || expand_wq_overflow || uvjoin_owner_mismatch
                          || owner_mask_identity_invalid || metajoin_illegal_sidx3
                          || cache_response_hold_violation
                          || unsupported_fill_refusal_pre_e
frame_fault_o              = recoverable_frame_fault_sticky
                           || recoverable_counter_delta
                           || lifetime_structural_fault
```

The `unsupported_fill_refusal_pre_e` term is present in the Packet-B top exactly until Packet E atomically lands typed cache termination and removes that latch/equation operand; it is absent from every Packet-H source closure.

Typed producer/commit events set recoverable state even when corruption prevents an ordered result. A later nonzero ordered status is an additional check, not the only route to `frame_fault_o`. When a lifetime source fires, Packet H does not wait for normal `quiet_o` and does not attempt clear: it stops new admissions and writes, enters the reset barrier, resets the island/owners, and releases the lease without publication. In particular, the consumed UV mismatch cannot be “drained” into an owner completion, expander overflow cannot be waived by baseline capture, invalid authoritative owner-mask generation cannot feed expansion/combine, illegal metajoin sample index 3 is dropped without a metadata read or owner completion, a stalled cache response that changes/disappears cannot leave `err_rsp_dropped_o` low or permit publication, and a pre-Packet-E `fill_refused_i` assertion cannot be ignored or misreported as a normal typed denial that the cache cannot yet terminate. Packet E removes that last lifetime source only when its typed refusal completion and accounting gate lands.

Packet H is the sole system driver of this handshake. It asserts and holds
`frame_fault_clear_valid_i` only for a newly accepted renderer lease/frame after
the prior frame and all old V3 work have drained. After accepting its grant, the
new writer holds first-fragment/frame-begin admission until that clear handshake
occurs. Packet H never clears opportunistically during an old lease, on terminal
release, or merely because the island happens to be idle.

Positive controls are mandatory: hold clear-valid while one 71-term quiet operand (including an unsolicited external response) is nonquiet and observe no acceptance; accept one clear while fully idle; inject each declared clearable child/top fault on the clear edge and require the fault to win; independently increment all seven owner counter families, dispatcher class mismatch, the metajoin generation-mismatch counter, and every inventory-declared counter-only recoverable operand while its baseline holds, then separately capture each baseline while its live operand holds. Dedicated lifetime controls assert RCP `qerr`, expander `wq_overflow`, UV owner mismatch, invalid owner-mask generation at joined read, invalid owner-mask generation at combine admission, metajoin illegal sample index 3, a committed stalled-cache-response overwrite/drop observation mutant, and pre-Packet-E ABI-reserved `fill_refused_i` one at a time. RCP/expander/cache-response-drop/unsupported-fill controls require a legal clear not to lower `frame_fault_o`; the unsupported-fill control also proves the event cannot be silently ignored or counted as a typed cache completion. the UV/owner-mask/metajoin-lifetime controls require no unsafe expander/combine/metadata-read admission followed by reset barrier, owner/island reset, lease RELEASE, and no normal-quiet/clear or publication claim. No output-only sticky alias, omitted `src_unpub`, common-enable counter/baseline pair, or test which clears work/lifetime state may satisfy this ABI.

This section freezes an interface and behavior only. It does not claim the ports,
child clear inputs, baselines, tests, or any resource/timing result are already
implemented.

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

The resolver has a held one-read elastic stage and a reserved disposition slot, so it can accept one request per clock when downstream capacity exists. `force_refuse` is set only for the admission-frozen count-mismatch `material_refused` path and suppresses table/planner/cache access. **The front-end handshake is the logical sample issue**: on the same edge it pulses `iss_tmu_valid_i` with the full handle into `zhao_texture_v3own` and increments `SJ_accepted`. A valid resolved row later increments `SJ_planner_accept` only on planner handshake. Selector overflow, forced refusal, page generation zero/mismatch, invalid row, illegal row bits, or sample-0 witness mismatch enters the local-refusal output and increments `SJ_local_refused` only when that terminal return is accepted by the owner.

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

### 7.4 Guard refusal must terminate—Packet-B reservation, Packet-E adoption

Today's cache fill interface has no refusal response. If an accepted guard request receives a denial verdict, simply withholding data parks the only blocking miss and eventually the island head forever. Packet B therefore reserves the exact versioned-top input name `fill_refused_i` in the ABI and includes its physical assertion in public quiet, but Packet B's cache **cannot terminate or account it**. Before Packet E, any `fill_refused_i` assertion latches `unsupported_fill_refusal_pre_e` as a reset-lifetime fault and enters Packet H's reset barrier; it is not clearable, not a normal denial completion, and cannot be silently ignored.

Packet E replaces that temporary unsupported-input disposition with the implemented terminal law: one `fill_refused_i` pulse, mutually exclusive with the eight data beats, is accepted by the versioned cache for each denied captured texture fill. Only at that Packet-E landing does the event become a typed recoverable refusal which clears the blocking miss and permits ordinary owner/quiet drain.

Under Packet E, on `guard_accept`, the cache/mux request is no longer offered; it waits for the one verdict. On `verdict_deny`, the mux emits exactly one `fill_refused_i` for a captured `TEXTURE_FILL` request. The cache invalidates any partial line, clears its one outstanding fill, increments `fill_jobs_completed` and `fill_jobs_refused` once, and returns a sample result with `SOURCE_REFUSED` and the original route token. V3 commits that terminal response and drains the refused fragment through its normal ordered release; the shell latches the frame fault and, only after complete writer drain, releases rather than publishes the framebuffer lease. Geometry receives its separately typed request refusal; it is never presented as an empty 64-bit beat.

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

The same reset-barrier controller has an internal structural-abort entry used only by V3 reset-lifetime faults. On RCP `qerr`, expander `wq_overflow`, consumed UV-owner mismatch, invalid authoritative owner-mask identity, or metajoin sidx3, it immediately blocks new renderer/V3 admission and framebuffer writes, prevents publication, and waits only for already-issued guard/SDR transactions and physical write acknowledgements to reach a reset-safe boundary—not for `quiet_o`, owner release, or a recoverable clear. It then asserts island reset so all stranded V3 owners/work are discarded, records the lifetime-fault diagnosis, transitions the active WRITING lease to FREE as a fault RELEASE with no READY event, and lets video repeat the previous displayed frame. This controlled island reset does not pretend that the corrupted owner completed and cannot be replaced by `frame_fault_clear_valid_i`. Pre-Packet-E unsupported fill refusal uses the same barrier only in the retained Packet-B/E historical control; Packet H is introduced after E and never includes that superseded behavior in its normal closure or workload.

### 8.2 Terminal events: clean publication versus fault release

Every fatal renderer or blitter event while a lease is live sets `live.fault` under that lease key. For a renderer lease, Packet H first accepts the grant, waits for prior work to be fully quiet, holds the V3 `frame_fault_clear_valid_i` request until its public-quiet `frame_fault_clear_ready_o` handshake, and only then admits frame-begin/fragment work. Any subsequent recoverable V3 `frame_fault_o` assertion—including a typed producer fault which creates no ordered result—sets `live.fault`; no mid-lease clear is permitted. Packet H's normal closure begins after Packet E, so `fill_refused_i` is tested only as a typed recoverable cache completion with ordinary drain. A sequence mismatch separately latches shell/raster `sequence_abort`, suppresses writes, drains/drops all admitted ordered V3 outputs, and then follows the ordinary fault RELEASE path. RCP `qerr`, expander `wq_overflow`, UV-owner mismatch, invalid authoritative owner-mask identity, or metajoin sidx3 instead enters the reset barrier immediately: new admissions/writes stop, the island and owners reset, and the lease releases without waiting for normal V3 quiet or attempting a clear. For the normal clean/recoverable path, the writer may offer a terminal event only after its source, write guard, global arbiter requests, SDR writes, and local output queues have drained and its issued/retired counts agree. The held terminal payload is `{publish,release,writer,slot,generation}` with exactly one outcome bit set; it is stable until accepted.

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

### 8.4 Reset-in-flight and structural-abort contract

A one-domain reset must not make a displayed slot look FREE. Reset therefore enters a cross-domain barrier: stop new leases and writes; flush both event FIFOs and the video pending register; command scanout to the reset/blank source; wait for synchronized `gpu_reset_done` and `vid_blank_done`; only then initialize both slots FREE with generation zero and reopen lease arbitration. If either domain resets while the other is live, the same barrier is entered. Generation zero is not reused while video may still scan an old generation.

For the internal structural-abort entry defined in section 8.1, the barrier's GPU-side admission/write stop and physical-memory safety fence are mandatory even though no external domain reset occurred. It explicitly does not wait for corrupted V3 owner quiet: after safe memory retirement it resets the island/owners, fault-RELEASES the WRITING lease, emits no ready event, and preserves the previous displayed frame. UV mismatch has already consumed its bad join pair and invalid owner-mask identity has fed no expander/combine work, and metajoin sidx3 has launched no metadata read; no synthetic owner completion is generated.

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

After that witness, a normal-path lease whose captured writer fault—including the post-clear V3 `frame_fault_o` level and shell sequence-abort status—is zero offers PUBLISH, and a recoverably faulted/fully drained lease offers RELEASE. Sequence abort reaches this witness only after every ordered V3 output has been consumed/dropped and all owners release. A V3 reset-lifetime source bypasses this normal quiet witness and follows section 8.1/8.4's reset-barrier RELEASE path. A clear handshake is never generated at old-frame completion; Packet H generates it only after accepting the next renderer lease/grant and before admitting that new frame. The existing `render_drain_done_o` may remain a binner milestone for observation, but is neither terminal authority nor a CDC source.

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
S / F             Early-Z survivors admitted to V3 / normal ordered outputs accepted by RASTER.FRAGMENT
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
OWN_REORDER_HELD   compatibility-visible `ev_reorder_held_o`, newly-ready non-head tickets only
CA / CC           texture-cache access jobs accepted / completed
FI / FOK / FREF   fill jobs accepted / completed with eight beats / refused
FB                accepted fill data halfwords
CJ / CD           combiner jobs accepted / completed
PI / PC           combiner phases issued / completed
O                 ordered owner outputs accepted and released normally or dropped
SD                ordered outputs accepted/dropped while sequence_abort is latched
D                 attribute seed and reseed divides accepted
W_issue/W_retire  framebuffer words issued / retired
GA/GOK/GDENY      ENGINE1 guard requests accepted / OK verdicts / deny verdicts
```

`SJ_local_refused` includes selector overflow, absent/mismatched page, invalid row, witness mismatch, and count-mismatch-refused sample jobs terminated without planner access. `AJ_local_refused` includes count-mismatch-refused AUX jobs and degenerate-envelope jobs terminated without a Sheet read; a stale/missing sealed handle instead counts one Sheet acceptance and one MISS completion. Stale, duplicate, out-of-range, pre-issue, or unrequested owner returns increment only their protocol-fault counters; they do not increment `SJ_completed` or `AJ_completed` because they did not satisfy an owner plane.

The present owner exposes only `ev_commits_o`, which combines TMU and AUX commits and therefore cannot prove either equality independently. Packet B permits the following **instrumentation-only** additions to `zhao_texture_v3own`:

```systemverilog
output logic [31:0] ev_tmu_commits_o;   // increments exactly when c4t_v_q is true
output logic [31:0] ev_aux_commits_o;   // increments exactly when c4a_v_q is true
output logic [31:0] ev_reorder_held_o;  // increments for each ticket newly ready while non-head
// existing ev_commits_o remains the cycle-by-cycle compatibility sum of TMU+AUX only
```

A cycle committing both paths increments each typed count by one and `ev_commits_o` by two. `ev_reorder_held_o` independently adds one for each ticket whose ready bit makes a 0→1 transition on that edge while its valid owner is not the current ordered head; head-ready tickets, already-ready tickets, invalid ticket slots, and held cycles add zero. Owner identity is read for this comparison only when the ticket-valid/newly-ready predicate is true, and the top never reads an invalid owner-output bus. `zhao_texture_island_v3_top` maps `ev_reorder_held_o` bit-for-bit to the compatibility-visible reorder-held counter; it does not reconstruct the event. All four counters are reset-zero modulo `2^32`; acceptance windows are bounded below wrap. These ports do not gate or alter claim, issue, commit, ticket creation, final acceptance, emission, release, ready, or ordering. Commit-counter positive controls commit one TMU and one AUX return together, then each kind separately; swapped-source and combined-only mutants fail. Reorder controls create newly ready head and non-head tickets separately, retain a non-head ready ticket for multiple cycles to prove one count, and ready two eligible non-head tickets together where legal to prove per-ticket counting. A mutant counting held level, head readiness, or invalid owner bits must fail. The owner fault counters remain separate. Together with dispatcher class mismatch, metajoin generation mismatch, and every inventory-declared counter-only recoverable source, Packet B exposes live values, captures separate baselines only on `clear_fire`, and treats any delta as a frame fault without clearing a counter. RCP qerr, expander wq overflow, UV-owner mismatch, and pre-E unsupported fill refusal are explicitly excluded from this baseline set because they are reset-lifetime.

After complete fault-free or recoverable drain—including sequence-abort ordered drops—the exact closure below applies. Reset-lifetime structural faults instead record the partial counts at barrier entry and reset stranded island state; they make no ordinary owner-drain equality claim. The recoverable closure is:

```text
S == F + SD == owner_admitted == owner_emitted == owner_released == O
SJ_required == SJ_accepted
SJ_accepted == SJ_planner_accept + SJ_local_refused
SJ_completed == SJ_accepted
SJ_completed == OWN_TMU_COMMIT
AJ_required == AJ_accepted
AJ_accepted == AJ_sheet_accept + AJ_local_refused
AJ_completed == AJ_accepted
AJ_completed == OWN_AUX_COMMIT
OWN_ALL_COMMIT == OWN_TMU_COMMIT + OWN_AUX_COMMIT
OWN_REORDER_HELD == independent scoreboard count of newly-ready valid non-head tickets
CA == CC
FI == FOK + FREF
FB == 8*FOK
CJ == CD == S
PI == PC == phase_demand
GA == GOK + GDENY
W_issue == W_retire                    // required before publish
```

For a fault-free frame `SD=0` and the first equality reduces to `S=F`. In a sequence-aborted frame, every output before the first mismatch may contribute to F, the mismatch and every later ordered output contribute exactly once to SD, `O=F+SD`, and publication remains forbidden.

Every one of those is also partitioned by success/refusal and, where applicable, owner class. A locally refused job counts once as accepted and once as completed, not as a phantom cache/sheet acceptance. An accepted cache hit completes one cache job without a fill; an accepted miss creates exactly one fill job. After Packet E, a denied texture fill contributes one `FREF`, zero `FB`, and one cache-job completion. The guard equality is evaluated only with no request in `WAIT_VERDICT`; counters additionally prove no acceptance receives two verdicts and no verdict occurs without the captured acceptance.

For the current one-physical paired-phase combiner, accepted jobs are classified exactly once after all required-source commits:

```text
source_status_dirty = any committed required TMU/AUX status[7:0] is nonzero
J1 = source_status_dirty
  || material_refused
  || status-clean legal PASSTHRU(count 0/1), ADD_SAT, or MASK job
J2 = status-clean, !material_refused, legal MODULATE, MODULATE2X, LERP,
     or TERRAIN_DETAIL_MASK job
J3 = status-clean, !material_refused, legal TERRAIN_DETAIL_LIGHT job
phase_demand = J1 + 2*J2 + 3*J3
CJ = J1 + J2 + J3
```

The logical `||` clauses define one J1 membership predicate: **any job with any required-source nonzero 8-bit status is J1 regardless of recipe**, and `material_refused` is J1 regardless of recipe; J2/J3 require both legal count and completely clean required-source status. The engine may issue at most one paired phase per clock. A long all-ready, independently backlogged run must issue one phase on every post-fill clock until the runnable phase set drains, and `PI`/`PC` must equal the equation rather than merely produce the right colors. Consequently a homogeneous one-phase stream may demonstrate one completed material job per clock after pipeline fill; a status-clean two-phase stream consumes two issue clocks per job and a status-clean three-phase stream consumes three. **No one-material-job-per-clock claim is legal for the multi-phase recipes unless a later measured architecture adds sufficient physical phase engines and updates this equation.**

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

Top: a generated real-pin/MISR wrapper around `zhao_raster_tile_pipe_v2`, including EDGEWALK, attribute candidate production at the selected parameters, Early-Z, the widened skid, one V3 island explicitly instantiated as `#(.MIGRATION_SHADOWS(1'b0))`, `RASTER.FRAGMENT`, TILESTORE, RESOLVE, and legal cache/AUX responders. This fit selection does not alter or overwrite the laboratory V3 interface artifact at `MIGRATION_SHADOWS=1`.

Questions:

* does the elaborated connected hierarchy contain exactly one lifecycle owner and no TEXJOIN;
* did that hierarchy elaborate V3 with explicit `MIGRATION_SHADOWS=1'b0`, `shadow_present=0`, zero compatibility shadow counters, and no shadow comparator/register state—not a default, laboratory setting, or no-op selector;
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

Top: a new D3-style generated real-pin wrapper around one connected hierarchy containing `zhao_shell_top_v2`, the real ENGINE1 geometry/texture memory share, one V3 instantiated as `#(.MIGRATION_SHADOWS(1'b0))`, and the one shared terrain/projector service. The wrapper drives a legal two-view transaction and consumes every output through registered domain-local sinks. It has its own exact port manifest, explicit-parameter activity smoke test, post-map witness, and source digest; none replaces the laboratory V3 ABI artifact at `MIGRATION_SHADOWS=1`.

Questions:

* does the actual connected render hierarchy fit with **comfortable margin below 30,000 ALMs and 85 DSPs**;
* does it close the GPU clock at 100 MHz and retain the declared video/audio clocks;
* are there exactly one `zhao_project_core`, one V3 island explicitly at `MIGRATION_SHADOWS=1'b0` with `shadow_present=0` and no shadow comparator/counter state, one `zhao_texture_v3own`, and zero TEXJOIN instances;
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

The laboratory V3 interface artifact records every effective parameter, including `MIGRATION_SHADOWS=1, DEPTH=16, CTXW=64, RCTXW=160, AUXCTXW=224, BINDW=8, LODW=8, GENW=8, LANES=4, SRCW=18, DATAW=64, TOKW=18, AUX_TOKW=14, PAL_SLOTS=4, PAL_ENTRIES=256`. It remains the one `zhao_texture_island_v3_top.interface.json` artifact and remains `MIGRATION_SHADOWS=1`; production selection neither overwrites it nor relabels its elaboration.

`MIGRATION_SHADOWS` is an elaboration switch, never a no-op selector. At laboratory value 1 an explicit generate branch elaborates the real shadow comparison datapath and its accepted/compared/mismatch counters, drives the structural witness `shadow_present=1`, and the positive control must move each counter and deliberately fire a mismatch. At production value `1'b0` the shadow comparator, shadow data path, and counter registers do not elaborate; `shadow_present=0` and every exported compatibility shadow-counter value is constant zero. Source/elaboration hierarchy checking rejects a value-1 build with no comparator/counter state, a value-0 build retaining any shadow state, or a conditional mux which selects between identical/no-op paths.

The production/accounting profile lives only in the existing ownership registry `design/prod_manifest.yml`. Its current closed override member is exactly:

```yaml
production_parameter_overrides: {zhao_texture_island_v3_top: {MIGRATION_SHADOWS: 1'b0}}
```

The leaf `1'b0` is a YAML scalar **string** whose canonical text is the sized SystemVerilog bit-vector literal `1'b0`; it is not an integer zero, Boolean, inferred default, or unsized expression. For this selected profile the outer override map contains exactly `zhao_texture_island_v3_top`, its inner map contains exactly `MIGRATION_SHADOWS`, and the value must type-check against the declared parameter and be representable without truncation, extension, X/Z, or signedness reinterpretation. The production-manifest loader rejects duplicate mapping keys before ordinary YAML object construction can erase them. Unknown module, unknown parameter, duplicate, non-string value, noncanonical literal, declared-type mismatch, and unrepresentable value are hard failures.

`tools/quartus/gen_prod_top.py` consumes that exact map and emits the V3 accounting instance with explicit `#(.MIGRATION_SHADOWS(1'b0))`. `tools/quartus/check_prod_manifest.py` reparses the closed registry member, compares it with the generated parameter assignment and elaborated value, and rejects omission/default inference or text/value divergence. `tools/quartus/check_ownership_roles.py` runs on that same generated selected closure and sees the same instance/profile. Positive fixtures independently remove the override, duplicate either key, invent a module/parameter, change the leaf type, use a noncanonical or unrepresentable literal, drop the generated assignment, or emit `1'b1`; each must fail. There is no second profile/ownership registry and no production clone of the laboratory interface JSON. The laboratory artifact remains explicitly `MIGRATION_SHADOWS=1` with `shadow_present=1` and live comparator/counters; the production elaboration must prove `shadow_present=0`, no shadow comparator/counter state, and zero compatibility shadow-counter outputs.

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

Packet E changes `zhao_texture_island_v3_top.sv` even though its port list is preserved: removal of `unsupported_fill_refusal_pre_e` therefore requires regeneration of `top_source_sha256`, the selected source-closure row/hash, custom-tool hashes when their validation changes, and canonical interface bytes. A byte-identical old artifact is stale; laboratory `MIGRATION_SHADOWS=1` and all port names/order remain unchanged.

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

A source/elaboration inventory then proves Early-Z `PAYLOAD_W=410`, skid `W=490`, retirement context 160, owner context 224, typed AUX context 224, descriptor logical image 287, descriptor physical image 320 as eight ascending 40-bit slices with exactly 33 zero pad bits, V2 descriptor/UV `joined365={owner14,logical287,U32,V32}`, top-owned material row 46 plus separate `material_refused` bit and stored owner generation, top-owned descriptor-trust sidecar `{descriptor_usable1,owner_generation8}`, every final-class/collector record 66, and external result 48. It proves that the captured eight-bit page generation exists exactly once in the join as `logical287[286:279]`, with no appended generation port/field, and that the expander copies only that slice into every resolver sample job. It separately proves that the V3 top writes row46, `material_refused`, and stored owner generation together on `own_adm_accept`, captures `{descriptor_usable,owner_generation}` only on descriptor-response handshake, independently forms O/M/D at joined owner, compares material and descriptor masks separately with stored exact `own_adm_req`, always supplies valid owner mask with recoverable forced refusal on any copy failure, and makes only invalid O reset-lifetime. The expander closure contains selected mask/force control and contains no row46 path. The combiner closure contains the tagged/credit-reserved synchronous second material-read pipeline/FIFO, accepts one valid owner ticket per ready clock, preserves owner+row+generation, gates invalid output owner, and implements owner-mask-derived count/AUX, illegal recipe selection (recipe0 for count2; recipe1 otherwise), zero weight/base, forced material refusal, and exact owner-plane status/index preservation. Neither adds a top-held valid/quiet operand or any owner functional port. It also proves `frag_invw24_i` exists and `frag_depth_i` does not exist on the versioned top. The root-port inventory requires exact scalar ports `input logic frame_fault_clear_valid_i`, `output logic frame_fault_clear_ready_o`, and `output logic frame_fault_o`, and retains the Packet-B versioned-top names `fill_req_valid_o` and `pal_load_valid_i` rather than accepting historical aliases; the laboratory elaboration still requires `MIGRATION_SHADOWS=1`. The leaf-interface inventory requires `zhao_texture_early_desc_v2.desc_pad_fault_o` declared exactly `output logic [31:0]`, descriptor response carrying owner14 plus canonicalized logical287 and a separate `descriptor_usable` verdict whose top sidecar capture enable is only response handshake, `zhao_texture_aux_pipe_v2.sheet_rsp_owed_o` declared exactly `output logic`, scalar `output logic` ports `zhao_texture_binding_resolver_v2.{cfg_loader_idle_o,binding_crc_busy_o,binding_seal_pending_o}`, `zhao_texture_rsp_dispatch_v2.pending_valid_o[3:0]` in `{ERR,BIL,NEAR,CLUT}` order, four independent 66-bit terminal inputs plus one held 66-bit output, `zhao_texture_v3own.obs_claim_valid_o` exactly `c1t_v_q||c1a_v_q`, `zhao_texture_v3own.obs_ready_valid_o` exactly `q0t_v_q||q0a_v_q||q0i_v_q`, `zhao_texture_v3own.ev_reorder_held_o[31:0]` with newly-ready valid non-head event logic and invalid-owner gating, no owner functional-port/state/ready use of either observation, `zhao_texture_metajoin_v2.idle_o` covering its one retained read result rather than fictional input holds, reset-lifetime outputs for RCP `qerr`, expander `wq_overflow`, and UV-owner mismatch, and `frame_fault_clear_i` on every versioned child which owns clearable frame sticky state. The dispatch inventory proves cache output steers directly to class processing, palette/bilerp request and response arcs precede the collector, all four final-class offers terminate in the collector, and no raw/index/metadata sidecar enters it. The quiet-source audit requires every versioned idle/data-idle port, exact `q_combine_idle=material_read_idle_w&&combine_leaf_idle_w` as one operand with independent source/positive-control proof for both sides, parses every top-local `q_*` source mapping, proves the owner claim/ready aliases come only from the two exact observation ports and the dispatch OR/`pending_valid_o`/output aliases have the exact sources in section 4.5, proves single-driver connectivity without hierarchical child-state reads, proves every mapped operand occurs with the frozen polarity in exactly one literal equation, and counts exactly 60 `data_quiet` plus 11 additional public terms. Clear-valid and fault must be absent from those 71 terms.

`tools/quartus/check_ownership_roles.py`, reading the existing `design/prod_manifest.yml` ownership registry and the connected generated top, requires exactly one provider of `raster_texture_fragment_lifecycle`: `zhao_texture_v3own`. Expanders, descriptor banks, resolver, class processors, the terminal collector, contexts, skids, and sequence witness declare transport/storage/check roles, not ownership. A test-only duplicate-owner composition marks both V3 ownership and TEXJOIN ownership and must make the checker fail. Generated accounting and later connected-production tops are checked separately; a disconnected selected sibling cannot satisfy connected ownership. The same run verifies that the selected V3 accounting instance carries the registry's explicit `MIGRATION_SHADOWS=1'b0` override; no separate role file is read.

### 12.2 V3, material, AUX, and result gates

Drive the unchanged old island and Packet-B V3 with identical PASSTHRU/count-1/no-AUX traffic and compare RGB/A/legacy-tag/refused order under stalls. All other material arithmetic compares the corrected `zref_material.hpp` directly with owner ruling R9 and the new V3 combiner; the old island is not allowed to outvote that oracle.

Required vectors include zero, one, 127, 128, 254, and 255 in every operand position and cover:

* PASSTHRU counts 0 and 1, and every illegal count;
* recipes 1–5 at exact count 2 and wrong counts 0/1/3;
* recipes 6–7 at exact count 3 and wrong counts 0/1/2;
* exact alpha for all eight recipes, including unchanged `s0.a` on 1–4/6, unit alpha on MASK/detail-mask, negative LERP deltas, and saturation boundaries;
* MODULATE2X's single `(a*b+64)>>7` rounding, MASK's continuous alpha rather than a nonzero gate, and both detail recipes' MODULATE2X first layer;
* every one of the eight assigned recipe IDs, proving no unknown-recipe path exists, and every legal-count mismatch as the sole malformed-material case; a count mismatch generates no planner/cache/Sheet access, one local terminal refusal for each declared required source, full 8-bit status `{7'b0,material_refused}` ORed with committed required TMU/AUX statuses, loud final error, recoverable frame fault, normal ordered release, and J1 classification;
* sample 2 and AUX both required simultaneously, proving the combiner reads sample 2 and only AUX status; changing AUX tag/strength with status zero must not change RGB/A/index, while changing sample 2 must change the two three-sample recipes;
* successful AUX tag/strength arriving in the typed AUX plane and being deliberately unexposed; any test that treats it as RGBA fails;
* each legal recipe with each required TMU source and required AUX source independently returning a nonzero 8-bit status, proving the final full-byte OR and forcing that job into J1; status-clean legal J2/J3 controls prove multi-phase classification is not selected from recipe alone.

Result-path vectors include CLUT8 indices 0 and 255, both CLUT4 nibbles, all direct formats with index zero, palette latency/backpressure, bilerp stalls, and independently delayed final-class offers. The cache holds one raw record while direct token-class steering stalls each class processor in turn; only the selected processor may accept it. Palette and bilerp request/response handshakes must complete before their 66-bit terminal offer can enter the collector. Tests present CLUT/NEAR/BIL/ERR terminal completions singly and all four simultaneously, require four independently held pending entries in fixed `{ERR,BIL,NEAR,CLUT}` order, apply owner-output backpressure, and prove fair round-robin service without starvation. Every accepted input and output record is scoreboarding all 66 bits. A token whose embedded class disagrees with its physical input channel fires the independent protocol/frame detector while still preserving and eventually forwarding all 66 bits. The aliases must observe exactly the OR of final offers, collector pending bits, and collector output; substituting the cache valid at any of the three fails. Final status is the full 8-bit OR of committed required TMU and AUX planes plus `{7'b0,material_refused}`; final index is sample 0 or zero at count zero. The star-disc/halo and alpha-test cases run end to end through `RASTER.FRAGMENT`; forcing index zero, substituting a palette-colour byte, borrowing a later response's index, routing raw cache traffic through the collector, or adding a dispatcher sidecar must fail.

AUX vectors are derived from normative `design/contracts/TEXTURE.AUX.V2.md`, not oracle-only `TEXTURE.AUX.md`. They interleave sheet A/envelope A, sheet B/envelope B, then sheet A under independent descriptor, request, response, and return stalls. They cover same index old/new generation, HIT, MISS, degenerate envelope, wrong opcode, wrong source ID, ALLOCATED/OVERFLOW on READ, duplicate response, response without issue, AUX absent, and AUX required at sample count zero. Each malformed owed response terminates the expected head exactly once and every no-issue response changes no owner state. Contract conformance also proves the 224-bit field offsets, 48-bit AUX plane, issue-before-return, request/response hold, typed counts/quiet, and the absence of any AUX-to-sample-2 data path. A fixture which selects old `TEXTURE.AUX.md` as Packet-B authority must fail the contract-closure checker.

Committed arithmetic/status mutants reproduce each stale alternative independently: alpha multiplication on recipes 1–4, binary MASK, rounded-unit-then-double MODULATE2X, unit rather than MODULATE2X detail first layer, count-zero reading `s0` instead of admitted base, count-zero early bypass for a non-PASSTHRU recipe, an invented unknown-recipe rejection despite the exhaustive 3-bit table, narrowing status to bit 0, omitting required AUX status, or assigning a status-dirty multi-phase recipe to J2/J3. Additional mutants substitute AUX for sample 2; drop/reconstruct raw index after palette lookup; place dispatch before class processing; serialize the four collector input captures; alias pending bits to final-class offer wires; route `q_dispatch_req_valid` from cache valid; ignore token/input-class mismatch; starve one class; or attach a raw/index sidecar to the collector. The ordinary tests assert correct output; inverse-polarity mutant drivers prove each detector/gate can fail.

Output backpressure is applied while later work completes out of order. RGB, alpha, index, status, all 160 retirement bits, and the legacy 16-bit tag view remain stable. The compatibility test specifically drives `frag_ctx_i[15:0] != frag_aux_ctx_i[15:0]`; only the former may appear on `out_tag_o`.

### 12.3 Full-identity stall gate

An independent scoreboard records every accepted V3 fragment as:

```text
external: all 160 retirement-context bits plus every V3 request field
internal descriptor/UV joined365: {owner14, logical287, U32, V32}
  sole joined generation location: logical287[286:279]
internal descriptor-trust sidecar: {owner_slot6, captured_owner_generation8, descriptor_usable1}
internal expander trust control: {owner14, M, D, selected_required_mask4, force_all_required_refused1} // no row46
internal combine material read: {owner14, stored_owner_generation8, row46, material_refused1} // stored or exact canonical substitution
internal sample work: {slot[5:0], generation[7:0], sample_index[1:0], page_generation[7:0] copied from logical287[286:279]}
internal AUX work:    {slot[5:0], generation[7:0]}
```

It then applies long, independently randomized stalls at V3 input, descriptor read, both V2 UV-join inputs, V2 UV-join output, expander trust-control capture/held fragment record, combine-admission second material read/held job, planner/cache response, AUX response, V3 output, fragment input, tile read/write, resolve, and framebuffer output. The A/B join case holds A at the output while B is offered and requires A's owner, all 287 logical bits—including sole generation slice `[286:279]`—and U/V to remain unchanged; the descriptor-trust sidecar must retain A's response-captured usability/generation, the expander hold must retain only A's authoritative owner-mask/force control and no M/D/copy mask/row46, and the later combiner hold must retain A's independently reread row46/refused/generation or canonical diagnostic control. The resolver's expected generation and material scoreboard are admission-time copies, not duplicate late fields. The descriptor-derived and material-derived masks are compared from independently held records. On every stalled cycle the scoreboard checks `valid` and every payload bit, not only the eventually accepted packet. Aggregate accepted/emitted counters are secondary.

Committed renamed mutants cover independent timing/storage faults: the context-read/slot swap changes a held descriptor while retaining its token; the UV-join generation-slice swap retains A's owner/U/V and other logical bits while taking B's `logical287[286:279]`; a late-global mutant overwrites that held slice from current active-page state; a descriptor-trust mutant writes usability/generation on request or while the response is stalled instead of only response handshake; a material swap retains A's joined owner while reading B's generation/mask; a split-write mutant fails to update row46, separate refused bit, and generation together; an O/M/D mutant derives owner authority and both copy checks from one captured enable or recomputes `owner_required_mask_m` instead of storing exact `own_adm_req`; a transport mutant carries row46 through the expander; and a combine mutant reuses the first read rather than performing the second generation-checked read. Normal drivers assert correct hold/order and compare with admission-time independent records; inverse-polarity drivers pass only when the appropriate recoverable or reset-lifetime detector fires. No detector operands may share the corrupted enable. Owner-mask dispositions are exercised directly: O valid with both copies matching gives normal; O valid with material invalid, descriptor invalid, both invalid, material-mask mismatch, descriptor-mask mismatch, or both mismatched always uses exact owner mask and forces refusals; O invalid alone takes the barrier. The descriptor physical-image mutant remains separate: it writes one of the 33 pad bits high; one accepted read must produce `desc_pad_fault_o` delta one, canonical-zero descriptor payload, response-handshake sidecar D=0, then valid O owner-mask forced refusal or invalid O reset barrier—never corrupt row exposure.

UV **owner-token** mismatch is not one of those recoverable carriage mutants. Its dedicated control occupies the two join inputs with different owner14 values, requires the join to consume both without output, asserts reset-lifetime `uvjoin_owner_mismatch_o`, and then requires Packet H's reset barrier to reset the island/owners and RELEASE without publication. It must not wait for owner release/`quiet_o`, accept a normal clear, or synthesize a refused completion.

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
* unpublished source (`src_unpub`);
* simultaneous malformed TMU and AUX returns;
* valid simultaneous TMU and AUX returns, proving typed commits each increment one and combined commits increment two;
* sample-only then AUX-only commits, proving neither typed counter aliases the combined counter;
* locally refused binding/AUX work with logical issue at N and earliest return at N+1;
* generation wrap/drain;
* head incomplete while later owners finish;
* observation-only outputs exactly matching `c1t_v_q||c1a_v_q` and `q0t_v_q||q0a_v_q||q0i_v_q` through every bit combination, with no ready/state/lifecycle change;
* `ev_reorder_held_o` for newly-ready valid non-head tickets only: head-ready, held-ready, invalid-slot, invalid-output-owner, one-event, and legal two-event controls plus compatibility-counter bit-for-bit mapping.

Every named refusal/protocol class remains separately observable. A sum over an unstated subset or the legacy combined commit count is not evidence.

Because a correct work queue cannot overflow under legal stimulus, retain the renamed committed small-queue/full-guard mutant whose inverse-polarity test requires `wq_overflow` to fire. That event is reset-lifetime structural evidence, not a recoverable baseline: the test must keep `frame_fault_o` high across an otherwise legal clear and then verify Packet H's reset-barrier island/owner reset, lease RELEASE, and zero publication.

#### Recoverable frame-fault controls

The canonical top-ABI test holds `frame_fault_clear_valid_i=1` while each of the 71 quiet operands is made nonquiet in isolation, including unsolicited `fill_data_valid_i`/`fill_refused_i` and `pg_valid_i`, and requires `frame_fault_clear_ready_o=0`, no `clear_fire`, and a stable held request. At full idle it requires `frame_fault_clear_ready_o==quiet_o==1`, one accepted clear, no work/counter/config mutation, and deassertion only of explicitly clearable pre-existing frame state. It separately toggles clear-valid and recoverable/lifetime fault levels and proves the literal quiet equation never changes.

Direct-event controls set every declared clearable source in the exact table, including typed nonzero status and count refusal cases intentionally prevented from creating an ordered result. A same-edge clear plus any such fault must leave `frame_fault_o` high. Counter controls independently exercise owner range/stale/unsolicited/duplicate/final/issue/`src_unpub`, dispatcher class mismatch, metajoin generation mismatch, and every source-inventory counter-only recoverable operand: first the live counter changes while its baseline holds and fault asserts, then only `clear_fire` captures the baseline while the live counter holds. Omission, common-enable, swapped-source, or unlisted-counter mutants fail. Observation windows remain below `2^32` wrap.

Lifetime controls are separate. RCP `qerr`, expander `wq_overflow`, and the historical pre-E unsupported `fill_refused_i` remain high across any otherwise legal idle clear and require structural reset. UV owner mismatch consumes the bad pair; metajoin sidx3 consumes the raw cache response without a metadata read; invalid owner-mask generation feeds neither expander nor combiner. Those UV/metajoin/owner-mask controls explicitly make no normal `quiet_o`, owner-drain, or clear acceptance claim. Every lifetime case requires island/owner reset, lease RELEASE, and zero publication/READY event. Packet E owns the paired historical/current control in which the same `fill_refused_i` event changes from pre-E lifetime barrier to one post-E typed cache completion and ordinary recoverable drain; no Packet-H normal workload exercises the pre-E branch. Conversely, an ordinary recoverable sticky or owner-mask-valid descriptor/material copy fault clears after its forced-refusal drain and the next legal idle handshake. Any test which baselines a lifetime source or resets counters, work, configuration, or cache payload on `clear_fire` fails independently.

### 12.6 Raster/tile differential

For representable one-sample fixtures, compare old flat `zhao_raster_tile_pipe` with V2 using a texture/palette fixture which returns the same RGB/A/index. Then compare the complete V2 path to `zref` on varying interpolated depth/UV/color, Early-Z accept/reject, alpha test, star tag, blend/stencil hazards, multi-triangle tiles, bank overlap, and framebuffer backpressure.

Checks include:

* exact accepted fragment order and count;
* exact depth bits at Early-Z, fragment input, and tile write;
* exact source ID through the fragment read response;
* exact 64-bit tile words;
* sequence mismatch latches abort before any wrong write, blocks new candidate admission, drops the mismatch and every remaining ordered V3 output with exact `sequence_drop_count`, releases every owner, and produces RELEASE/no publication after drain;
* exact 256 resolve outputs and tile CRC;
* no swap until all V3 and fragment work drains;
* next-front-bank rendering overlaps previous-back-bank resolve as before.

A committed ordinary-drain mutant which omits V3 quiet from `pipe_empty` must swap early under a delayed texture response and fail the tile/CRC comparison. A separate committed sequence mutant retains the old match-gated V3 ready; the mismatch positive control must show the head stranded and fail exact drops, owner release, finite drain, and lease RELEASE.

### 12.7 Binding and memory gates

Binding tests program sparse and dense pages through BEGIN/WRITE/END/ABORT, write selectors out of order, and independently vary active/staging generations. They verify canonical CRC bytes, duplicate-row rejection, illegal mode/base/palette rejection, bad CRC leaving the old page active, reset generation zero, delayed END acknowledgement, fragment-admission fence at successful seal, and atomic activation only after complete `data_quiet`. While prior owners remain live, accepted fragments alternate selectors and every request must retain its own page/base/mode/class/palette tuple. Dedicated descriptor tests prove `physical320={33'b0,logical287}`, `slice[k]=physical320[k*40 +: 40]` for all `k=0..7`, simultaneous eight-slice write, held eight-slice read, canonical-zero invalid response payload, descriptor-usable plus owner-generation sidecar capture only on response handshake, and independent zero-pad/generation verdicts. The committed one-pad-bit mutant's first accepted bad-pad read must change `desc_pad_fault_o` from reset zero to one exactly once despite response stalls, expose canonical-zero `logical287`, capture D=0 only on the accepted descriptor response, and then obey owner authority: valid O uses exact owner mask for forced refusals regardless of M; invalid O enters the reset-lifetime barrier. Separate counter controls prove hold, delta two for two accepts, and modulo wrap.

The top-owned material/trust test accepts distinct A/B owners with different `own_adm_req`, owner-mask generations, row46 payloads, count-validity bits, material generations, and descriptor usability. It proves owner mask/generation plus row46/refused/material-generation write only and together on `own_adm_accept`, with owner mask bit-for-bit equal to accepted `own_adm_req`, while descriptor usability/generation writes only on descriptor-response handshake and remains stable across a stalled response. Joined-owner controls independently derive O, M, D and compare each copied mask with authoritative owner mask. Tests require: O valid with both copies valid/equal gives normal expansion; O valid with either/both copies invalid or either/both masks mismatching always selects owner mask and forces every owner-required source to terminal refusal; only O invalid feeds no expander and asserts reset-lifetime barrier. The first-read reservation may land only owner mask/force control in the expander hold; source/elaboration inventory fails any row46 or M/D/copy-mask path through expansion.

At owner combine admission the test requires the tagged, credit-reserved synchronous material-read pipeline/FIFO to accept one valid owner ticket per ready clock, preserve owner+row+generation through stalls, and perform a fresh material and owner-mask generation comparison. Backpressure must consume reserved credit without overwriting a held result, and invalid output-owner bits must never be sampled. A valid read supplies exact row46 plus the separate count-only `material_refused`. With valid owner-mask generation, an invalid/mismatched material read must derive count/AUX from owner mask, choose recipe0 for count2 or recipe1 for count0/1/3, zero weight/base, set canonical `material_refused=1`, and read exactly owner-required planes so status/sample0 index survive, yielding J1/refused/loud output. Invalid owner-mask generation admits no combiner and takes the barrier. First-read reuse, row-through-expander, live-pin reread, canonical count/recipe/owner-plane-read violation, missing credit reservation, owner/row/generation swap, invalid-output-owner read, throughput below one accepted ticket per ready clock, or an extra top-held valid/72nd quiet term each fails. The owner module's functional interface and state remain unchanged.

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

Long all-ready homogeneous and mixed streams check every accepted/completed equality, both typed owner commit counts, their compatibility sum, and the exact `J1 + 2*J2 + 3*J3` phase equation in section 9.2. For every recipe, each required TMU/AUX source independently returns a nonzero 8-bit status and must force the whole job into J1; canonical invalid-material cases at owner counts 0/1/2/3 are likewise J1, select recipe1/1/0/1 respectively, preserve exact required statuses/sample0 index, and contribute the lawful material-refusal status bit; J2/J3 controls are legal-count, status-clean, and non-loud. Separate controls duplicate an in-flight sampler job and a combiner phase while keeping output bytes unchanged; both must fail exact job/phase deltas. A simultaneous sample/AUX commit and sample-only/AUX-only runs prove counter independence. The steady-state report names accepted fragments, samples, AUX jobs, cache jobs/fills, combiner jobs, issued/completed phases, typed commits, 8-bit status classes, J1/J2/J3 classification, and ordered outputs. An owner-edge one-per-clock result without those downstream deltas cannot pass.

The structural-quiet gate composes three independent layers rather than pretending top stimulus can isolate all 71 operands. Static source-map parsing and one omission/polarity/source mutant per operand provide exhaustive 60+11 coverage. The owner leaf bench drives `c1t_v_q/c1a_v_q` and `q0t_v_q/q0a_v_q/q0i_v_q` combinations and requires `obs_claim_valid_o`/`obs_ready_valid_o` to equal only the two authorized equations, with no functional behavior change. Leaf benches separately hold reciprocal, metajoin's one retained read result, descriptor/UV/planner/cache/Mosaic/bilerp/palette/combine internal requests, responses, credits, and results; every final-class offer; every collector `pending_valid_o` entry and held output; AUX owed/local-return state; resolver loader/CRC/seal states; expander owner-mask/force control; and the combine material-read pipeline/FIFO. Each leaf requires its own `idle_o=0` positive control and later drain to 1. The V3-top runtime directly drives only public fragment, fill-response, Sheet-response, palette-load, binding-config/response-ready, and ordered-output-ready channels, holds their payloads through backpressure, and observes the resulting public aliases/quiet behavior. Legal end-to-end traffic observes internal aliases collectively but does not claim one-at-a-time private-state control. Each leaf `idle_o` must remain low for its own fired condition; public `quiet_o` must remain low for each publicly driven or legally reached top condition and return high only after complete drain. The source-map checker proves every operand has one exact named source and one equation use, no source is private child state, every vector bit participates, the owner observation equations, `pending_valid_o` spelling, and fifth `q_combine_idle` expression alias are exact; material-read-busy/leaf-idle and material-read-idle/leaf-busy each force the alias low; and the total remains exactly 71. It requires one inverse-polarity omission mutant per operand, plus owner-quiet, both owner-observation aliases, each side of the composite combine-idle alias, pre-class dispatch alias, collector-pending alias, and AUX-issued-credit mutants; a single prose-group mutant is insufficient. `frame_fault_clear_valid_i` and `frame_fault_o` are separately toggled and must never enter or perturb the 71-term equation. The AUX control accepts two Sheet reads and retires their responses one at a time, requiring `sheet_rsp_owed_o` to stay high after the first and fall only after the second, while `idle_o` remains low if an AUX return is still held. These fire controls pass before quiet is cited by a recoverable clear, tile-swap, or binding-activation test; lifetime structural tests do not claim ordinary quiet after corruption.

Required V3-top runtime/mutant gates then compose those layers: legal fragments create owner-mask/descriptor/material copies and exercise normal plus forced-owner-mask refusal; a generation mutant invalidates only authoritative owner mask and must invoke the barrier; a metajoin-sidx3 mutant consumes one raw response with zero metadata reads; a bilerp-token mutant proves expected-token refusal/magenta; combine-read stalls prove one-ticket-per-ready-clock tagging/credit/hold and canonical malformed substitution; owner reorder events map bit-for-bit without invalid-owner reads; laboratory shadow traffic moves compare counters and fires mismatch with `shadow_present=1`; the production elaboration mutant retaining shadow state at `1'b0` fails hierarchy; and Packet E's paired historical/current fill-refusal controls prove atomic latch removal. No top test claims direct access to an unexposed leaf register.

### 12.9 Lease, publication, and CDC gate

The writer-aware slot test covers simultaneous renderer/blitter requests in both round-robin histories, request and grant stalls, both slots, every canvas mode, and base/span boundary addresses. It requires one accepted contender, stable loser payload, no starvation, grant capture of the exact `{writer,slot,generation}`, and both guards accepting only the matching writer inside `[base,base+span)`.

The Packet-H renderer control accepts a new lease/grant only after old-frame drain, asserts and holds V3 `frame_fault_clear_valid_i`, and withholds frame-begin and fragment admission until `frame_fault_clear_ready_o==quiet_o` accepts it. A deliberately presented unsolicited fill or Sheet response keeps clear-ready low; removing it permits exactly one clear handshake for that lease. No clear may occur during old work, on its terminal event, twice for one lease, or without a newly accepted renderer lease. A same-edge recoverable first-frame fault wins and latches the lease fault. A later sequence mismatch suppresses all writes from the bad output onward, drains and counts every ordered drop until owners release, then forces RELEASE. RCP qerr, expander wq overflow, UV-owner mismatch, invalid authoritative owner-mask identity, and metajoin sidx3 each take the reset-barrier path instead: no normal quiet/clear is required or claimed, island/owners reset, the lease RELEASES, and publication/READY remain zero. Packet H is post-E: its only `fill_refused_i` case is typed recoverable completion and ordinary drain; the pre-E lifetime behavior appears solely in Packet E's retained historical mutant/control.

Terminal tests cover clean renderer and blitter publication; typed texture/memory faults after ordinary drain; sequence identity abort after exact ordered-drop/owner-release drain; structural lifetime fault after reset barrier rather than quiet drain; explicit cancel; publication FIFO backpressure; duplicate and stale terminal events; wrong writer; same slot/wrong generation; and generation wrap after drain. Every fault RELEASE produces no READY event. A clean publication produces exactly one READY FIFO write on the same accepted state transition.

CDC tests independently stall both FIFO sides and every video boundary. With no accepted READY event, several frame boundaries must repeat the displayed slot and emit no swap. With one event, video swaps once and echoes the unchanged tuple; GPU frees the previous displayed slot only after the matching swap return. Stale and duplicate swap returns change no state and fire their positive controls.

Committed mutants include: external/current `fb_writer_i` substituted for the captured writer; slot-only terminal comparison; `{slot,generation}` comparison that omits writer; raw writer-done used as the CDC source; fault release enqueued as READY; sequence mismatch match-gating V3 ready or permitting a later fragment write/publication; UV mismatch waiting for ordinary quiet or synthesizing a refused completion; expander/RCP/pre-E-fill lifetime fault accepted as clearable; framebuffer base taken from a caller pin; and swap return freed without generation match. Each has an inverse-polarity fire test for the corresponding independent detector.

Reset is asserted at held request, held grant, active writes, publication backpressure, READY FIFO occupancy, video pending, and swap return. Internal structural-barrier tests separately inject RCP qerr, expander wq overflow, UV bad-pair mismatch, invalid authoritative owner-mask identity, and metajoin sidx3 while owners and memory work are live. The gate requires admission/write stop, only physical memory-safety retirement, island/owner reset without an ordinary quiet/owner-drain claim, lease RELEASE with zero READY/publication, the blank/reset barrier where domain reset requires it, zero surviving pre-reset events, no lease before required reset-done acknowledgements, and a post-barrier clean publication before display resumes.

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

1. A module addition, removal, rename, parameter-interface change, port change, ownership-role change, or selected parameter-profile change updates the existing ownership registry `design/prod_manifest.yml` consumed by `tools/quartus/gen_prod_top.py`, `tools/quartus/check_prod_manifest.py`, and `tools/quartus/check_ownership_roles.py` in that same packet. No parallel registry or checker is created.
2. Disposition follows the checker-proved instantiation graph. A module reachable from a selected accounting root is counted **inside that root and receives no `excluded` row**. `tools/quartus/check_prod_manifest.py` intentionally fails an excluded module which is nevertheless in a selected root's closure.
3. `zhao_texture_island_v3_top` is already a selected **accounting** root. Therefore every Packet-B successor it instantiates—including the versioned reciprocal/perspective/meta/UV joins, descriptor, expander, resolver, planner, cache, pre-collector class processing, terminal collector, Mosaic/bilerp/palette paths, AUX, and combiner—is inside that root, appears in its exact source closure, and is not `excluded:not-yet-adopted`. In particular `zhao_texture_uv_join_v2.sv` is a counted child, never an instantiated-but-excluded leaf. This accounting fact does **not** mean the shell renders through V3 or that production connection/adoption occurred.
4. The selected V3 root uses exactly `production_parameter_overrides: {zhao_texture_island_v3_top: {MIGRATION_SHADOWS: 1'b0}}` in `design/prod_manifest.yml`, with `1'b0` parsed as a canonical scalar string. `tools/quartus/gen_prod_top.py` emits `#(.MIGRATION_SHADOWS(1'b0))`; `tools/quartus/check_prod_manifest.py` rejects duplicate/unknown keys, non-string or noncanonical literals, type/representability failure, missing generation, and elaborated divergence; `tools/quartus/check_ownership_roles.py` checks the same selected instance. The laboratory `zhao_texture_island_v3_top.interface.json` remains a separate ABI artifact at `MIGRATION_SHADOWS=1`; it is not the production profile registry. Only a new module unreachable from every selected accounting root is registered `excluded:not-yet-adopted` (or the exact existing schema equivalent). Fit-only wrappers remain fit-only and never become functional providers merely by appearing in a source list.
5. The same packet regenerates `fpga/rtl/prod/zhao_prod_top.sv` with `tools/quartus/gen_prod_top.py`, refreshes the selected root's exact fit source closure, and runs freshness plus explicit-parameter checks. A zero semantic generated diff is still checked against the changed source/registry state; reliance on a module default is a failure even if that default presently equals the selected value.
6. Every V3 port change additionally updates all direct instantiations and the exact laboratory interface artifact/generator/checker in section 12.1; every V3 top-source change refreshes its raw/source-closure/tool hashes even when port order is unchanged. Packet B therefore adds the three frame-fault ABI ports to that artifact while preserving its explicit `MIGRATION_SHADOWS=1`, and separately preserves the generated accounting instance's exact `#(.MIGRATION_SHADOWS(1'b0))` assignment. Packet E's top-source latch removal refreshes the same laboratory artifact/tool/source hashes and production/accounting closure atomically with cache termination. There is no later manifest-cleanup packet and no SKIP for an unavailable parser/elaborator.
7. Rollback restores RTL, contracts, reference oracle, tests/mutants, `design/prod_manifest.yml` role/profile data, generated accounting top, laboratory interface artifact, fit targets, and source lists from the same packet atomically.

Throughout A–K, `fpga/rtl/common/zhao_shell_top.sv`, its old `zhao_shell_fit_top.sv`, D3 policy/manifest, and D3 receipt remain byte-for-byte historical artifacts. No packet redirects those names.

### Packet A — types and instruments; no behavior switch

**Owns**

* `fpga/rtl/common/zhao_render_texture_pkg.sv`: the 128-bit continuation, typed 224-bit AUX context, 490-bit pretexture packet, 410-bit Early-Z payload, 160-bit retirement context, and 48-bit result;
* `design/prod_manifest.yml` and `tools/quartus/check_ownership_roles.py`: role declarations in the existing ownership registry and the connected-top role census; no second registry or checker path;
* checker self-tests, duplicate-owner fixture, and this packet's ledger/generated/source-closure updates.

**Gate:** exact layout elaboration including named AUX offsets, current-interface census, `design/prod_manifest.yml` role parsing, `tools/quartus/check_ownership_roles.py` pass, and duplicate-owner fire control.

**Rollback:** revert the package, the Packet-A role data in `design/prod_manifest.yml`, checker/fixtures, and closure metadata together. No RTL consumer exists, so the selected hierarchy remains unchanged.

### Packet B — complete and contract the V3 boundary

**Owns**

* `fpga/rtl/texture/zhao_texture_island_v3_top.sv`: rename `frag_depth_i` to `frag_invw24_i`; retain legacy `frag_ctx_i`; add `RCTXW=160`, `AUXCTXW=224`, retirement-context/status/raw-index ports and the exact public `frame_fault_clear_valid_i`/`frame_fault_clear_ready_o`/`frame_fault_o` ABI; retain Packet-B's exact new boundary names `fill_req_valid_o` and `pal_load_valid_i`; implement `MIGRATION_SHADOWS=1` as real lab-only comparator/counter state with `shadow_present=1` and `1'b0` as no elaborated shadow state/zero counters with `shadow_present=0`; implement the 71-term quiet law independently of clear/fault using the two owner observation-only ports and the single composite `q_combine_idle=material_read_idle_w&&combine_leaf_idle_w` operand; map `ev_reorder_held_o` bit-for-bit to the compatibility counter and never read invalid owner-output identity; broadcast `frame_fault_clear_i` only to declared clearable child stickies; implement the closed recoverable baseline table including owner `src_unpub`, dispatcher and metajoin counters; preserve RCP qerr/expander overflow/UV mismatch/owner-mask-generation invalid/metajoin-sidx3 as reset-lifetime sources; own row46/refused/material-generation arrays plus descriptor-response usability/generation sidecar, capture exact authoritative owner mask/generation from `own_adm_req`, compare descriptor/material copies independently against it, force owner-mask refusals on every copy failure with valid O, and make only invalid O reset-lifetime, pass only selected mask/force control through expansion, and perform the second generation-checked row46 read at combine admission with no owner functional-port change; expose full binding-config and Surface Sheet fields; place direct raw-cache class steering and all palette/bilerp processing before the terminal collector; map every quiet alias to the exact leaf output port or named top-visible channel in section 4.5; remove live global binding authority; instantiate owner at context 224/result 48;
* `fpga/rtl/texture/zhao_texture_v3own.sv`: observation/instrumentation only—`ev_tmu_commits_o`, `ev_aux_commits_o`, `ev_reorder_held_o` counting only newly-ready valid non-head tickets, top-visible monotonic `range/stale/unsolicited/duplicate/final/issue/src_unpub` counters, `obs_claim_valid_o=c1t_v_q||c1a_v_q`, and `obs_ready_valid_o=q0t_v_q||q0a_v_q||q0i_v_q`—while retaining combined `ev_commits_o`; no material storage/mask/refusal/generation port, no `frame_fault_clear_i` counter reset, and no functional port, state, ready, credit, lifecycle enable, or transition change;
* new `zhao_texture_early_desc_v2.sv`, `zhao_texture_frag_expand_v2.sv`, `zhao_texture_binding_resolver_v2.sv`, `zhao_texture_aux_pipe_v2.sv`, and `zhao_texture_material_combine_v3.sv`: the exact physical/logical descriptor, 32-bit reset-zero modulo `desc_pad_fault_o`, table/issue, resolver config-state observation ports, AUX-V2 `sheet_rsp_owed_o`, and R9 laws above; each interface explicitly declares whether a per-frame sticky is clearable and gives set-over-clear priority, while counters/work/config remain untouched; expander `wq_overflow` is expressly reset-lifetime and has no clear/baseline path;
* `zhao_texture_rsp_dispatch_v2.sv`: only the post-class four-input/one-output terminal collector—four simultaneous-capable independent held 66-bit entries, exact `pending_valid_o[3:0]` in `{ERR,BIL,NEAR,CLUT}` order, token/input-class validation with the class-mismatch monotonic counter exposed for baseline comparison, fair held owner return, and bit-for-bit 66-bit preservation—with no raw cache sidecar, palette request, bilerp request, or pre-class state;
* new `fpga/rtl/texture/zhao_texture_uv_join_v2.sv`: owner-aligned `joined365={owner14,logical287,U32,V32}` through both input holds and the output hold; captured active-page generation exists only at `logical287[286:279]`, which expansion copies into resolver jobs; a mismatching owner pair is consumed/dropped, sets reset-lifetime `uvjoin_owner_mismatch_o`, and requires Packet H's island reset barrier rather than ordinary drain/clear; the unversioned join remains oracle-only;
* versioned observation-preserving successors `zhao_raster_rcp24_v4.sv`, `zhao_raster_perspuv_pairpipe_v2.sv`, `zhao_texture_metajoin_v2.sv`, `zhao_texture_tmu_plan_v2.sv`, `zhao_texture_cache_pipe_v2.sv`, `zhao_texture_mosaic_v2.sv`, `zhao_texture_bilerp_lane_v2.sv`, and `zhao_texture_palette_res_v2.sv`: exact `idle_o` coverage required by section 4.5, including metajoin's one retained read result; declared clearable fault outputs; baselined metajoin generation-mismatch counter; and reset-lifetime RCP `qerr` plus metajoin sidx3 consume/drop-without-read, with no new lifecycle or ready path; the cache holds its raw response while token class directly ready/valid-steers it into CLUT/NEAR/BIL/ERR processing, and all Mosaic/bilerp/palette request/response work finishes into held 66-bit class terminals before `zhao_texture_rsp_dispatch_v2`; bilerp retains its expected token and converts any token/lane mismatch into expected-token `SOURCE_REFUSED`/magenta plus recoverable fault, never wrong-token clean colour;
* `design/contracts/TEXTURE.COMBINE.md` and `reference/include/zref/zref_material.hpp`: one Zhaozhou-native, not Sacrifice-exact, eight-recipe R9 authority; every 3-bit ID is assigned and only count mismatch sets the separate `material_refused`; PASSTHRU count 0 uses admitted base, only PASSTHRU admits 0/1, recipes 1–5 require 2 and 6–7 require 3; exact signed rescale/direct MODULATE2X/alpha/MASK/two-terrain-first-layer laws; 48-bit result with full 8-bit OR of committed required TMU+AUX status plus material refusal; and one-paired-phase-per-clock demand with every status-dirty/refused job in J1 and only clean legal jobs in J2/J3;
* new normative `design/contracts/TEXTURE.AUX.V2.md`: typed context/plane, Sheet READ import, independent issue/return/accounting/quiet, malformed-response handling, and no-AUX-as-sample-2 law; existing `design/contracts/TEXTURE.AUX.md` remains byte-for-byte oracle-only;
* `design/contracts/TEXTURE.CACHE.md`: Packet B reserves `fill_refused_i` but classifies any pre-E assertion as reset-lifetime unsupported input/reset-barrier recovery; versioned typed terminal-refusal and accepted/completed cache/fill accounting become implementable only in Packet E, without pretending Packet B connected or can normally drain memory denial;
* the draw-side clarification in `design/contracts/SURFACE.SHEET.md`: READ opcode/status/source validation and the existing ready/valid page response used by AUX V2;
* `fpga/rtl/generated/zhao_texture_island_v3_top.interface.json`, `tools/rtl/gen_texture_v3_interface_manifest.py`, `tools/rtl/texture_v3_interface_parser.py`, `tools/rtl/check_texture_v3_interface_manifest.py`, their closed-schema/canonical-byte/self-hash/wide/unpacked/source-vs-elaboration fixtures, the three exact frame-fault ABI ports, the explicit laboratory `MIGRATION_SHADOWS=1` elaboration with real comparator/counters and `shadow_present=1`, and the package-first exact source closure in section 12.1;
* V3/R9/AUX/binding differentials, descriptor-response trust-sidecar timing, top-owned material row/refused/generation, owner-mask-authoritative O/M/D copy-validation law, no-row-through-expander, and second combine-read checks, descriptor physical-pad and V2 UV-join generation/identity stalls, terminal-collector/raw-steering/class-order tests, exact owner-observation/reorder-counter/invalid-owner gates and per-operand 71-term structural-quiet tests, closed recoverable-counter baseline controls, recoverable-clear/same-edge tests, RCP/expander/UV/pre-E-fill lifetime reset-barrier tests, status-dirty J1 cadence tests, interleaved-sheet tests, required-mask tests, and every committed mutant named in sections 12.2, 12.3, 12.5, 12.7, and 12.8;
* `design/prod_manifest.yml`, `tools/quartus/gen_prod_top.py`, `tools/quartus/check_prod_manifest.py`, `tools/quartus/check_ownership_roles.py`, and generated `fpga/rtl/prod/zhao_prod_top.sv`: retain the one registry; encode exactly `production_parameter_overrides: {zhao_texture_island_v3_top: {MIGRATION_SHADOWS: 1'b0}}` with a canonical scalar-string leaf; reject duplicate/unknown/type/representability errors; and emit/check the selected accounting instance as `#(.MIGRATION_SHADOWS(1'b0))`, plus all same-packet role/disposition/source-list updates demanded by the V3 port and instantiation changes.

The shared unversioned `zhao_texture_aux_pipe.sv`, `zhao_texture_material_combine_v2.sv`, `zhao_texture_rsp_dispatch.sv`, `zhao_texture_uv_join.sv`, `zhao_texture_metajoin.sv`, `zhao_texture_tmu_plan.sv`, `zhao_texture_cache_pipe.sv`, `zhao_texture_mosaic.sv`, `zhao_texture_bilerp_lane.sv`, `zhao_texture_palette_res.sv`, `zhao_raster_rcp24_v3.sv`, `zhao_raster_perspuv_pairpipe.sv`, and `zhao_texture_island_top.sv` are outside Packet B and remain byte-for-byte oracle leaves. Oracle-only `design/contracts/TEXTURE.AUX.md` is likewise unchanged. Because the versioned successors are instantiated by selected accounting root `zhao_texture_island_v3_top`, they are counted inside that root and receive **no excluded rows**. Its accounting instance is emitted from explicit `design/prod_manifest.yml` data with `MIGRATION_SHADOWS=1'b0`; the separately checked laboratory interface artifact remains `MIGRATION_SHADOWS=1`. This is accounting closure only; Packet B still makes no shell connection, production adoption, area saving, or fit claim.

**Gate:** Packet A is already landed green; exact closed-schema source/elaboration laboratory manifest at `MIGRATION_SHADOWS=1` with `shadow_present=1`, real comparator/counter hierarchy, moving-counter/mismatch positive controls, including the three frame-fault ABI ports and exact new `fill_req_valid_o`/`pal_load_valid_i` names, canonical-byte/self-hash/tool-version checks, and fired fixtures; exact production override map with scalar `1'b0`, duplicate/unknown/type/representability negatives, and emitted/elaborated `#(.MIGRATION_SHADOWS(1'b0))` with `shadow_present=0`, zero compatibility shadow counters, and no shadow hierarchy/state; descriptor `287 -> {33'b0,*} -> 320 -> 8*40` packing plus 32-bit reset-zero modulo pad-fault counter independent of sticky fault; top-owned row46/refused/material-generation arrays written on `own_adm_accept`; descriptor usability/generation sidecar written only on descriptor-response handshake; authoritative owner-mask capture/check with recoverable owner-mask forced refusal for every descriptor/material copy invalid/mismatch and reset-lifetime only when owner-mask generation is invalid; no row46 through expander; second generation-checked combine-admission read with canonical diagnostic fallback only when authoritative owner-mask identity O remains valid; no extra quiet term or owner functional-port change; V2 UV join exactly `joined365` with generation only in `logical287[286:279]`, plus bad-pair consumption and reset-barrier-only lifetime recovery; explicit AUX owed and resolver loader/CRC/seal output ports; direct held-cache steering into class processors; palette/bilerp completion before a simultaneous-capable fair four-input terminal collector; exact 66-bit preservation, token/input-class validation, exact `pending_valid_o`, and dispatch OR/pending/output aliases; `obs_claim_valid_o=c1t_v_q||c1a_v_q`, `obs_ready_valid_o=q0t_v_q||q0a_v_q||q0i_v_q`, and `ev_reorder_held_o` newly-ready non-head/invalid-owner law with bit-for-bit top compatibility mapping, all with no functional effect; metajoin idle as one retained read result; exact single-driver source mapping for all 71 quiet operands with clear-valid/fault absent; canonical quiet-qualified frame clear and the closed direct/baselined/lifetime table including owner `src_unpub`, dispatcher class mismatch, metajoin generation mismatch, RCP qerr, expander overflow, reset-lifetime metajoin sidx3, UV mismatch, and pre-E unsupported `fill_refused_i`; R9 arithmetic/count/alpha/AUX-V2 laws with all recipe IDs assigned, only count mismatch malformed, exact 8-bit committed required TMU+AUX status OR, and status-dirty/count-refused/canonical-count-refused J1 and clean-legal/non-loud J2/J3; selector overflow; table programming/CRC/seal/generation/activation; class/palette witness refusal; issue-before-local-return; full Sheet opcode/status/source handling; separate sample/AUX commits; full-context stalls; malformed owner returns; exact job/phase counts; queue-overflow controls; and uninterrupted prepared ordered output. Every zero-valued detector named by acceptance has a legal fire case or committed mutant.

**Rollback:** revert V3 ports and exact versioned boundary names, clear/baseline/fault classification, owner observation/instrumentation ports, top-owned material arrays, descriptor-response trust sidecar, owner-mask-authoritative O/M/D copy-validation law, expander mask/force-only path, and second combine-admission material read, the physical descriptor, bad-pair-consuming UV join, and all versioned quiet/binding/raw-class/terminal-collector/AUX/combine leaves; revert new AUX-V2 and corrected combine/cache contract/reference updates, tests/mutants, laboratory interface generator/parser/checker/artifact, and the same packet's closed `production_parameter_overrides` map, generated `#(.MIGRATION_SHADOWS(1'b0))` accounting instance, role/profile data, fit/source closure, and checker fixtures together. The rollback must restore the prior generated production top rather than leave a default-parameter V3 instance. The old island, unversioned UV join/AUX/dispatch leaves, oracle-only AUX contract, and TEXJOIN experiments remain executable; no raster selected the Packet-B boundary.

### Packet C — synthetic post-Early-Z composition

**Owns**

* `fpga/rtl/raster/zhao_raster_texture_stage_v3.sv`, registered immediately as `excluded:not-yet-adopted`, including latched recoverable `sequence_abort_o`, reset-zero monotonic `sequence_drop_count_o[31:0]`, admission/write suppression, and always-draining V3 output drop mode;
* synthetic candidate/fragment harness, external-sequence/160-bit scoreboard, star/alpha-test differential, mismatch-drain positive control, and old-ready-gate identity mutant;
* the packet's ledger, generated accounting top, and source-closure update.

**Gate:** atomic admission, all 490 candidate bits held under stall, exact normal fragment mapping, one-owner census, no adapter bubble, terminal refusal retirement, and sequence-mismatch abort behavior: no write from the mismatch onward, exact drop count for mismatch plus all remaining V3 outputs, every owner released, finite drain, RELEASE, and no publication. The committed old `out_ready=fragment.ready&&sequence_matches` mutant must strand the head and fail.

**Rollback:** remove the stage/harness, sequence-abort/drop accounting, positive control/mutant, and closure registration together. Existing tile/shell paths remain selected.

### Packet D — versioned tile and binner composition

**Owns**

* `fpga/rtl/raster/zhao_raster_tile_pipe_v2.sv`: 410-bit opaque Early-Z payload, 490-bit skid, attribute bundle, V3 stage, sequence-abort admission/write suppression and ordered-drop drain, full ordinary drain law, TILESTORE/RESOLVE;
* `fpga/rtl/geometry/zhao_geom_binner_v2.sv`: binner-owned metadata at characterization capacity;
* `fpga/rtl/geometry/zhao_geom_bin_pipe_v2.sv`: exact setup/metadata splice;
* attribute/raster differential, early-swap mutant, and same-packet excluded registrations/generated closure.

The three unversioned modules remain unchanged oracles.

**Gate:** ATTRSTEP/ATTRDIV and `zref` differential, flat-compatible raster differential, multi-triangle accumulation, exact source/depth, ordinary swap/drain, sequence-abort no-write/drop-drain propagation, resolve overlap, and full backpressure.

**Rollback:** revert all three V2 compositions and their closure records together; the current flat path remains selected.

### Packet E — guarded ENGINE1 share and refusal completion

**Owns**

* `fpga/rtl/memory/zhao_render_asset_mux.sv`, immediately `excluded:not-yet-adopted`, plus raw-16-bit return demultiplexing;
* Packet-B successor `fpga/rtl/texture/zhao_texture_cache_pipe_v2.sv`: terminal fill-refusal implementation and Packet-B cache accounting; this packet replaces the pre-E reset-lifetime `unsupported_fill_refusal_pre_e` disposition with typed recoverable completion, retaining complete `idle_o`; the unversioned cache used by the old island remains unchanged;
* `fpga/rtl/texture/zhao_texture_island_v3_top.sv`: in the same atomic change, remove the `unsupported_fill_refusal_pre_e` lifetime latch/source, connect `fill_refused_i` to the cache's typed terminal path, and classify accepted refusal/status as recoverable frame state; no intermediate tree may contain typed cache termination plus the old lifetime latch, or neither;
* `fpga/rtl/generated/zhao_texture_island_v3_top.interface.json`, its generator/parser/checker/tool hashes, exact source closure, production/accounting source refresh, and direct instantiations: refresh for the changed top source while preserving the exact `fill_refused_i`, `fill_req_valid_o`, `pal_load_valid_i`, three frame-fault ports, laboratory `MIGRATION_SHADOWS=1`, and production `#(.MIGRATION_SHADOWS(1'b0))` profile;
* `fpga/rtl/common/zhao_pkg.sv`, `fpga/rtl/memory/zhao_mem_guard.sv`, `design/contracts/MEM.GUARD.md`, and `spec/memory_rules.md`: same-address-range ratification and exact accept-then-verdict master law;
* local-arbiter/guard/malformed-fill tests, held-valid-denial and subowner mutants, the retained **historical Packet-B** unsupported-fill-refusal mutant/control, post-E typed recoverable refusal tests, atomic old-latch-removal/source-artifact freshness checks, and closure regeneration.

**Gate:** run the retained historical Packet-B mutant/control showing ABI-reserved `fill_refused_i` caused reset-lifetime barrier recovery rather than silence or false completion; it is evidence of the superseded boundary, not Packet-H workload. Then prove the atomic Packet-E state: cache typed termination is present, `unsupported_fill_refusal_pre_e` is absent from V3 top/state/fault equation, changed-top interface/tool/source hashes and direct instantiations are fresh, laboratory/product profiles remain exact, and each denial produces one recoverable typed completion with ordinary owner/quiet drain. The memory gate additionally requires exact 16/32/64-byte traffic, request hold only to `guard_accept`, guard valid low in `WAIT_VERDICT`, exactly one OK/deny verdict per acceptance, eight beats per successful texture fill, fairness, no dropped/misrouted return, and client 5 unavailable.

**Rollback:** atomically restore the geometry-only ENGINE1 route, pre-E cache behavior in which `fill_refused_i` is ABI-reserved but unterminable, and `zhao_texture_island_v3_top.sv` with the reset-lifetime `unsupported_fill_refusal_pre_e` latch/source; restore the matching laboratory interface artifact/tool hashes/source closure, production/accounting refresh, direct instantiations, old region aliases/contracts, ledger, and generated files. It must never leave typed cache termination without removal of the lifetime latch, nor remove the latch while the cache still cannot terminate. No address allocation moves.

### Packet F — G8A connected raster/texture characterization

**Owns**

* generated `fpga/rtl/generated/zhao_raster_texture_v3_fit_top.sv` and exact generation manifest, both recording/emitting the explicit product V3 assignment `#(.MIGRATION_SHADOWS(1'b0))` rather than inheriting a default;
* the one G8A `design/fit_targets.yml` entry and receipt parser gates;
* fit-only registration and exact fit source closure.

**Gate:** after A–E simulation/mutant gates are green, run G8A once from a clean committed source. Record connected ALM/register/M10K/DSP/Fmax and hierarchy plus the elaborated explicit `MIGRATION_SHADOWS=1'b0`/`shadow_present=0`/no-shadow-state witness without adding standalone rows. Any resource or timing conclusion is conditional on that receipt.

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

* `fpga/rtl/common/zhao_shell_top_v2.sv`: versioned render input, V2 bin/tile path, V3 status/configuration and the sole system driver for its frame-fault clear handshake, ENGINE1 share, writer-aware leases, derived framebuffer window, and combined drain/fault terminal law; it requests one clear only after a newly accepted renderer lease and old-work drain, applies that accepted clear to both V3 recoverable state and the drained raster `sequence_abort`, withholds the new frame until acceptance, folds recoverable V3/sequence-abort state into that lease, waits for sequence-drop owner closure before RELEASE, and sends RCP qerr/expander overflow/UV mismatch/invalid authoritative owner-mask identity through the reset barrier with island/owner reset and no normal quiet/clear/publication, and treats `fill_refused_i` only through Packet E's typed recoverable path;
* a separately named generated D3-style shell-V2 fit wrapper, port policy, interface manifest, explicit nested-V3 `MIGRATION_SHADOWS=1'b0`/`shadow_present=0`/no-shadow-state witness, freshness/activity checks, and source closure;
* old/new shell differential on unaffected domains and texture-aware render differential.

The sibling is registered `excluded:not-yet-adopted`; the old shell files remain byte-identical.

**Gate:** F and G are green; unaffected behavior matches under paired traffic; render differences match the sampled oracle; nested V3 is explicitly `MIGRATION_SHADOWS=1'b0` with `shadow_present=0` and no shadow comparator/counters; no external writer/base authority reaches a guard; every new port is connected; each newly accepted renderer lease produces exactly one held-until-quiet clear handshake before frame admission; a same-edge recoverable fault wins; sequence mismatch suppresses writes, drains/drops all ordered outputs, closes owner/drop counts, and forces RELEASE; each of RCP qerr, expander wq overflow, consumed UV mismatch, invalid authoritative owner-mask identity, and metajoin sidx3 bypasses normal quiet/clear, executes the island/owner reset barrier, RELEASES the lease, and produces no READY/publication; post-E `fill_refused_i` produces one typed recoverable completion and ordinary drain; owner and hierarchy census are exact.

**Rollback:** revert sibling/wrapper/policy/manifest/source entries, lease-to-V3 clear/fault wiring, sequence-abort RELEASE control, and structural-fault reset-barrier entry as one unit. Production and D3 continue to select the historical shell; no orphan clear driver, drop mode, barrier request, or interpretation of `frame_fault_o` remains.

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

**Gate:** A–I remain green; legal `view_mask=2'b11`, RPP3/MATW18 workload and exact frame/cadence/lease results pass; then one clean G8C fit must show one projector core, one V3 island explicitly elaborated at `MIGRATION_SHADOWS=1'b0` with `shadow_present=0` and no shadow comparator/counter state, one `zhao_texture_v3own`, zero TEXJOIN instances, GPU 100-MHz closure, and **comfortable measured margin below 30,000 ALMs and 85 DSPs**. This is an acceptance condition, not a forecast.

**Rollback:** revert the integration top, wrapper/target, and closure metadata together. A missed fit gate selects nothing and grants no standalone-row arithmetic or TEXJOIN saving.

### Packet K — production/accounting selection, last

Only after J passes, one atomic packet:

* changes the production selection to the connected shell/terrain hierarchy while preserving exactly `production_parameter_overrides: {zhao_texture_island_v3_top: {MIGRATION_SHADOWS: 1'b0}}` in `design/prod_manifest.yml`;
* changes TEXJOIN's ledger state to `excluded:superseded` while retaining RTL, tests, standalone target, and historical receipt;
* regenerates `fpga/rtl/prod/zhao_prod_top.sv`, the one production/ownership manifest, accounting data, and exact production fit source list in the same packet; the emitted selected/accounting V3 assignment remains exactly `#(.MIGRATION_SHADOWS(1'b0))` with a scalar-string registry source rather than default-derived, while the laboratory interface artifact remains `MIGRATION_SHADOWS=1`;
* proves the generated diff selects one connected hierarchy and removes only TEXJOIN's private accounting instance/stimulus;
* reruns manifest freshness, the closed override-schema duplicate/unknown/type/representability negatives, `tools/quartus/check_prod_manifest.py`, `tools/quartus/check_ownership_roles.py`, hierarchy, DSP-census, and uncashed-cheque gates against the selected top, including exact generated text and an elaborated `MIGRATION_SHADOWS=1'b0`/`shadow_present=0`/no-shadow-state witness.

**Rollback:** revert the selection, TEXJOIN disposition, `design/prod_manifest.yml` role/profile data, generated accounting top, manifests, and source list together to Packet J's unadopted state, restoring the preceding exact scalar-string override map and generated `#(.MIGRATION_SHADOWS(1'b0))` assignment rather than a default-derived instance. Packet K performs no deferred repair from A–J and does not turn TEXJOIN's standalone 3,824-ALM row into a physical saving.

## 14. Exit criteria and explicit non-claims

The architecture is ready for implementation when these statements remain true:

* the post-Early-Z seam carries every V3 request operand and every downstream fragment field;
* V3's existing owner context, not a parallel FIFO, owns the continuation lifetime;
* raw sample-0 index and status reach `RASTER.FRAGMENT`; the held cache response is steered directly by token class into CLUT/NEAR/BIL/ERR processing, every palette/bilerp handshake occurs before terminal collection, and `zhao_texture_rsp_dispatch_v2` accepts simultaneous held class completions into four independent `{ERR,BIL,NEAR,CLUT}` entries, validates class, fairly emits one held owner return, preserves all 66 bits, and owns no raw sidecar;
* owner ruling R9 is the sole Zhaozhou-native—not Sacrifice-exact—recipe authority in contract, RTL, `zref`, and tests: all eight 3-bit recipes are assigned; only count mismatch sets separate `material_refused`; PASSTHRU 0 uses admitted base; only PASSTHRU admits 0/1, recipes 1–5 require 2, and recipes 6–7 require 3; signed rescale ties toward +infinity; MODULATE2X is direct `(a*b+64)>>7`; alpha/MASK/detail laws are exact; final status is the 8-bit OR of committed required TMU+AUX status plus refusal; every status-dirty/count-refused/canonical-count-refused job is J1, J2/J3 are legal and status-clean, and one paired phase per clock is accounted by exact J1/J2/J3 demand;
* normative `design/contracts/TEXTURE.AUX.V2.md` owns Packet-B AUX while old `TEXTURE.AUX.md` remains oracle-only; AUX never substitutes for sample 2, and Packet B explicitly leaves successful tag/strength without a typed visible consumer;
* the early descriptor is exactly `physical320={33'b0,logical287}` with `slice[k]=physical320[k*40 +: 40]`, constant-zero writes, independently checked 33-bit pad, and a separate 32-bit reset-zero modulo `desc_pad_fault_o` incremented once per accepted bad-pad read rather than aliased to sticky frame fault;
* the V3 top captures exact `owner_required_mask_m` plus generation from `own_adm_req` on admission, owns row46/refused/material-generation arrays and descriptor-response `{usable,owner-generation}` sidecar, independently checks both copied masks against valid owner authority, always uses owner mask with forced refusals on any copy failure, and treats only owner-mask-generation invalid as reset-lifetime; row46 never traverses expansion, and the tagged/credit-reserved one-ticket-per-ready-clock combine pipeline performs the second generation-checked read with owner-mask-derived count/AUX, recipe0 for count2 or recipe1 for count0/1/3, zero weight/base, `material_refused1`, and exact owner-plane reads preserving status/index on recoverable invalid material—without extra quiet state or owner functional-port change;
* `zhao_texture_uv_join_v2` holds exactly `joined365={owner14,logical287,U32,V32}`; its sole captured page-generation location is `logical287[286:279]`, copied later into resolver jobs for an independently enabled comparison; an owner-token mismatch consumes the bad pair, sets reset-lifetime structural fault, and requires island/owner reset-barrier RELEASE with no normal quiet/clear/publication claim;
* the 75-bit, 256-entry, two-bank binding table has exact programming, CRC, generation, seal, activation, witness, overflow, and local-refusal laws;
* every local binding/AUX refusal is logically issued before its later terminal return;
* `invw24` is preserved bit-for-bit, is named `frag_invw24_i`, and is not confused with projected `w`;
* full caller source identity reaches the fragment RMW transaction;
* every state transition is tied to a ready/valid acceptance;
* V3 exposes exactly `frame_fault_clear_valid_i`, `frame_fault_clear_ready_o=quiet_o`, and `frame_fault_o`; typed refused/status, descriptor, AUX, binding, material, and protocol sources set recoverable state even without ordered output; direct clearable stickies are declared, same-edge set wins, and the closed baseline set includes owner range/stale/unsolicited/duplicate/final/issue/`src_unpub`, dispatcher class mismatch, metajoin generation mismatch, and every inventory-declared counter-only recoverable source; RCP qerr, expander wq overflow, UV-owner mismatch, invalid authoritative owner-mask generation, metajoin sidx3, and pre-E unsupported `fill_refused_i` are never baselined/cleared and require reset-barrier recovery;
* full payloads hold under all stalls;
* the output can retire one prepared result per clock without an adapter bubble;
* ENGINE1 locally shares geometry and texture while client 5 remains unspent;
* ENGINE1 request acceptance and its registered verdict are counted separately, and denial completes exactly once;
* Packet B only reserves `fill_refused_i`: before Packet E its assertion is reset-lifetime unsupported and enters the barrier; Packet E alone makes it a typed recoverable terminal completion rather than a hidden deadlock;
* the GPU-domain lease captures `{writer,slot,generation,mode,base,span}` and both guards use only that record;
* Packet H alone issues one held-until-quiet V3 clear for each newly accepted renderer lease after old-work drain and blocks the new frame until acceptance; sequence mismatch latches abort, suppresses writes, drains/counts ordered drops until owner release, then RELEASES; lifetime structural faults bypass normal quiet/clear, reset island/owners through the barrier, and RELEASE without publication; its post-E source closure contains no `unsupported_fill_refusal_pre_e`, and `fill_refused_i` is tested only as typed recoverable completion;
* only an accepted clean publication creates a generation-bearing READY CDC event; fault release and no-ready frames repeat the previous complete display;
* accepted/completed fragment, sample, AUX, cache, fill, combiner-job, combiner-phase, and sequence-drop counts close after fault-free/recoverable drain, with `S=F+SD`, independent owner TMU/AUX commits, and their retained sum; reset-lifetime barrier cases explicitly do not claim ordinary owner-drain equality;
* V3 quiet is the literal 60-term `data_quiet` plus 11 additional public/configuration terms and exhaustive single-driver source map in section 4.5, including metajoin's one retained read result, exact owner `obs_claim_valid_o`/`obs_ready_valid_o` equations, the fifth alias `q_combine_idle=material_read_idle_w&&combine_leaf_idle_w` with both sides fired independently, every versioned leaf idle, held request/response, AUX V2 `sheet_rsp_owed_o`, config response, retirement valid, resolver V2 `cfg_loader_idle_o`/`binding_crc_busy_o`/`binding_seal_pending_o`, the OR of final-class collector offers, fixed collector `pending_valid_o` bits, and collector output—never private child state or an alias of owner quiet/cache response; clear-valid and fault remain absent from all 71 terms, and no lifetime-corruption test cites ordinary quiet as recovery evidence;
* tile swap and frame publication include V3/memory drain;
* both views share one terrain/projector path and one texture island;
* the legal RPP3/MATW18 two-view workload, not RPP1 or a 128-triangle fixture, is the final workload;
* G8B elaborates the parameter-fixed `zhao_terrain_pipe_rpp3_matw18_fit_top`, not raw-module defaults;
* Packet B begins only after Packet A has landed green; before that edge no Packet-B implementation artifact of any kind changes, irrespective of generic overlap permission;
* every changed V3 port—including the three exact frame-fault ABI ports—is reconciled with the closed schema-v1 laboratory interface manifest, exact nested JSON types/order, source/elaboration checker, recorded Verilator/CPython/custom-tool versions, raw hashes, self-field-omitting canonical SHA-256 algorithm, wide/unpacked representation, package-first closure, and explicit `MIGRATION_SHADOWS=1` in Packet B;
* `design/prod_manifest.yml` remains the sole production ownership/profile registry and contains exactly `production_parameter_overrides: {zhao_texture_island_v3_top: {MIGRATION_SHADOWS: 1'b0}}` with a canonical scalar-string leaf; `tools/quartus/gen_prod_top.py` emits `#(.MIGRATION_SHADOWS(1'b0))` with `shadow_present=0`, zero compatibility shadow counters, and no shadow state, while the laboratory artifact proves `shadow_present=1` with live comparator/counters; and `tools/quartus/check_prod_manifest.py` plus `tools/quartus/check_ownership_roles.py` reject duplicate/unknown/type/representability/missing/generated-divergence failures; no parallel Packet-A registry/checker is referenced;
* every instantiated Packet-B leaf is counted inside selected accounting root `zhao_texture_island_v3_top` with no contradictory excluded row, while unreachable candidates alone are excluded;
* only clean connected fits decide the comfortable-margin target below 30,000 ALMs and 85 DSPs.

### 14.1 Remaining HOLDs

No Packet-B ABI, arithmetic, AUX role, binding failure, quiet, manifest-disposition, or interface-manifest choice is left unspecified by this report. The remaining HOLDs are genuine implementation, upstream-product, integration, and measurement gates:

1. **Packet-A landing / Packet-B implementation-evidence HOLD:** Packet A must land green before any Packet-B implementation artifact changes. None of the versioned resolver/physical-descriptor/UV-join/quiet-leaf/direct-class-steering/post-class-terminal-collector/AUX/combiner changes; top-owned material arrays, descriptor-response trust sidecar, owner-mask-authoritative O/M/D copy-validation law, expander mask/force-only carriage, and second combine material read; exact owner observation ports; recoverable-clear ports, child declarations, closed counter baselines, or lifetime-fault/reset-barrier classification; pre-E unsupported-fill-refusal handling; owner instrumentation; new AUX-V2 or corrected R9 combine/cache contract/`zref`; laboratory interface generator/checker/artifact at `MIGRATION_SHADOWS=1`; exact scalar-string production override map and generated `#(.MIGRATION_SHADOWS(1'b0))`; differentials; or committed mutants is implemented by this architecture edit. Every Packet-B gate must pass before Packet C may select its interface.
2. **Cache/memory HOLD:** Packet B reserves the exact `fill_refused_i` ABI but cannot terminate it. Before Packet E, any assertion is a reset-lifetime `unsupported_fill_refusal_pre_e` fault requiring reset-barrier island/owner reset and lease RELEASE; it is neither clearable nor a normal denial path and may not be ignored. Packet E alone atomically installs typed recoverable cache termination, removes the V3-top lifetime latch/source, refreshes the laboratory interface/tool/source and production/accounting closure, and adds exact cache/fill accounting plus ordinary drain. An invalid/denied fill must not be claimed normally drain-safe before Packet E passes, and the retained pre-E mutant/control is never part of Packet H's normal closure.
3. **Explicit material-ABI HOLD:** Packet B deterministically supports Packet A's common-UV/common-LOD/consecutive-selector seam, but connected sample counts 2/3 remain disabled until shipped material records prove that convention or a later packet carries explicit `MaterialSample[3]` selectors, generations, UV sets, and LODs. Full upstream resource-generation validation remains mandatory.
4. **AUX-consumer HOLD:** Packet B validates and accounts successful `{tag,strength}` but deliberately exposes no surface-effect result. A complete terrain surface-effect claim requires a separately typed owner-aligned consumer/output and end-to-end visual contract; AUX may never be smuggled in as sample 2 to clear this HOLD.
5. **Attribute/raster HOLD:** exact `invw24`, UV/W, color, world-X/Z, sheet handle, and envelope carriage; top-owned two-source material/descriptor trust law and combine-time reread; Packet C's sequence-abort drop drain; Packets D–E; and G8A must close before the raster seam is called connected.
6. **Lease/CDC HOLD:** writer-aware slot arbitration, captured base/span, Packet H's exactly-one clear handshake for a newly accepted renderer lease after old-work drain, recoverable V3-fault capture, sequence-abort no-write/drop-drain RELEASE, reset-lifetime island/owner barrier RELEASE without a normal quiet/clear claim, clean-publish/fault-release behavior, accepted-ready CDC, generation-matched swap return, and reset safety must pass Packet G/H gates before shell V2 can qualify.
7. **Shell/terrain HOLD:** `zhao_shell_top_v2` is not implemented or selected. Normals, projected-`w` depth quantization, owner-sealed terrain identity, and production-scale binner/tile scheduling must close before the parameter-fixed G8B and legal two-view end-to-end gate. The protected `zhao_shell_top.sv` remains unchanged.
8. **Fit/budget/adoption HOLD:** no ALM, DSP, M10K, Fmax, production connection, or physical saving is inferred. G8A, G8B, and G8C require their named clean connected receipts. Only Packet K may atomically select the connected hierarchy and mark TEXJOIN superseded after G8C demonstrates comfortable measured margin below 30,000 ALMs and 85 DSPs.

This report does **not** claim that Packet B RTL exists; that the post-class collector, descriptor-trust sidecar, top-owned material arrays, owner-mask-authoritative O/M/D copy-validation law, no-row expansion path, combine-time second read, recoverable frame-fault ABI, child clears/baselines, lifetime reset-barrier controls, or explicit production/laboratory profile checks have been implemented; that Packet B can terminate `fill_refused_i` as a typed denial before Packet E; that the current shell contains V3; that successful AUX changes a visible effect; that the current terrain candidate can feed the binner at production scale; that the Packet-A selector is the complete three-sample material ABI; that any new module fits; that the whole machine is under budget; or that retiring TEXJOIN saves physical silicon. Those are the gates, not the starting assumptions. The protected historical shell remains unchanged by statement throughout.
