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

For `aux_required=0`, the entire 224-bit AUX value is canonical zero. For `aux_required=1`, the sheet handle and all four envelope edges are part of the fragment accepted by V3. The handle is the full generation-bearing `handle32`, and the envelope must satisfy `env_x1 > env_x0 && env_z1 > env_z0`. A stale/missing handle or degenerate envelope returns a typed refused AUX completion, sets the sticky frame fault, and issues no read under a guessed/default envelope.

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
| current response class | 2 |
| palette slot / generation | 2 + 8 |
| **total** | **362** |

Early-Z continues to take its explicit 80 bits (address, depth, state, source ID) and carries the remaining **410 bits** opaquely. The existing candidate skid therefore changes from `W=168` to `W=490`. This is a packet-width change, not an Early-Z algorithm change. These wider numbers supersede the earlier 330/250 draft; that draft carried world position but omitted the sheet/envelope identity needed to address two live terrain patches safely.

A free-running 32-bit raster admission sequence is stamped only on the V3 acceptance edge. The complete owner-carried raster retirement context remains 160 bits:

```text
{raster_sequence[31:0], raster_continuation[127:0]}
```

The sequence register advances only on `candidate_valid && v3_frag_ready`. It is stable while the candidate is stalled.

### 3.3 Put raster continuation in the existing owner; seal AUX under that owner

Add independent `RCTXW=160` and `AUXCTXW=224` parameters and these ports to the V3 top:

```systemverilog
input  logic [RCTXW-1:0]   frag_retire_ctx_i;
input  logic [AUXCTXW-1:0] frag_aux_ctx_i;
output logic [RCTXW-1:0]   out_retire_ctx_o;
output logic [7:0]         out_texel_idx_o;
output logic [7:0]         out_status_o;
output logic               quiet_o;
```

The existing 64-bit context meaning survives in `frag_aux_ctx_i[63:0]`: `[31:0]=wx`, `[63:32]=wz`. At owner admission, V3 presents:

```text
adm_ctx = {frag_retire_ctx_i, frag_aux_ctx_i[63:0]} // 224 owner-context bits
```

and instantiates the unchanged generic `zhao_texture_v3own` with that context width. At ordered output:

```text
out_tag_o        = own_out_ctx[15:0]   // deprecated compatibility view: wx[15:0]
out_retire_ctx_o = own_out_ctx[223:64]
```

The full 224-bit AUX record is written on that same `own_adm_accept` into a new owner-keyed `zhao_texture_early_desc_v2`, with the allocated `{slot,generation}`. The descriptor is exactly 279 bits: the existing descriptor's 55 non-context bits plus `AUXCTXW=224`, implemented as statically sliced synchronous banks with one shared write/read address. The old `zhao_texture_early_desc` remains the old-island oracle. V2 is not allocated separately, has no independent head/tail/credit, and accepts a read only for a live matching owner generation. The expander transports the descriptor attached to that owner. That makes sheet handle, patch envelope, and world position immutable for the same lifetime as the fragment without duplicating them in the ordered-output bank.

The owner context bank and its bounded output queue become 224 bits. The owner-sealed early descriptor becomes 279 bits. `zhao_texture_v3own.sv` itself does not need a semantic change; its parameterized context is already admitted once, stored immutably, fetched in order, held under stall, and released on output acceptance.

This is smaller and safer than a 64-entry sideband FIFO beside V3. It also makes owner release and raster-continuation release the same event, while the wider early descriptor remains subordinate owner storage rather than a second owner.

### 3.4 Atomic admission

There is one event:

```text
tex_admit = cand_valid && cand_ready
cand_ready = v3_frag_ready
v3_frag_valid = cand_valid
```

Every V3 operand, the 224-bit typed AUX context, and the 160-bit retirement context are derived from the same held candidate. No branch may accept alone. State in the stage advances only on `tex_admit`; merely offering a valid candidate while V3 is not ready changes nothing.

The context capture must use `own_adm_accept`, the same event that allocates `{slot,generation}`. A local rolling context pointer is forbidden.

### 3.5 Complete V3 result and exact required-source reduction

The V3 owner result changes from 40 to 48 bits:

```text
[47:40] status
[39:32] sample-0 raw index
[31:24] alpha
[23:0]  RGB
```

For this version, `status[0]` is `SOURCE_REFUSED`; bits `[7:1]` are reserved-zero at every current producer but are carried and bitwise-ORed rather than discarded. `out_refused_o = out_status_o[0]`. Fault-class counters remain separate; this packet does not invent seven new status encodings.

The required-source mask is frozen at admission and uses the owner's existing order `{AUX, sample2, sample1, sample0}`:

```text
sample_count 0 -> sample mask 000
sample_count 1 -> sample mask 001
sample_count 2 -> sample mask 011
sample_count 3 -> sample mask 111
required_mask  = {aux_required, sample_mask}
```

The consequences are exact:

* `sample_count==0` issues no TMU work. `PASSTHRU` returns admitted base RGB/A, raw index zero, and status zero unless AUX was required or the material is malformed. Owner admission with `required_mask==0000` must create combine eligibility without waiting for a nonexistent return.
* If `aux_required==1`, AUX is a required source even when `sample_count==0`: the owner cannot create the combine ticket until the matching AUX terminal return commits. A successful AUX which the selected recipe does not consume arithmetically still contributes status; the boolean does not grant permission to skip its lifecycle. This preserves the current additive required-mask law. A later material revision may replace the boolean with an explicit logical-source selector, but this packet does not silently make that different rule.
* Recipe/count legality follows frozen material v1: PASSTHRU accepts count 0 or 1; MODULATE, MODULATE2X, LERP, ADD_SAT, and MASK require count 2; TERRAIN_DETAIL_LIGHT and TERRAIN_DETAIL_MASK require count 3. A mismatch is a material refusal, sets status bit 0, and never degrades to a plausible smaller recipe. Command/material validation should reject it before sealing; if it reaches V3, V3 still drains it through the ordinary ordered terminal path.
* Each TMU source result is `{status8,index8,alpha8,rgb24}`. CLUT8 returns the addressed byte; CLUT4 returns the addressed nibble zero-extended; direct-color and terminal-error samples return index zero.
* AUX is also stored in a 48-bit owner plane for uniform reduction, with raw index zero. A sheet MISS, stale handle generation, degenerate envelope, or refused sheet response sets `SOURCE_REFUSED`; a successful AUX response leaves status zero.
* Final status is the bitwise OR of **only committed required sources**, plus `{7'b0,material_refused}`. Unrequested planes and uncommitted RAM contents are never read into the reduction.
* Final raw index is zero for count 0 and otherwise exactly committed `sample0.index`. AUX, samples 1/2, recipe arithmetic, and palette RGB never replace it. A bad sample 0 therefore remains visibly bad; no later good sample supplies a plausible index.
* A wrong-generation return, sample index outside the admitted count, AUX return when AUX was not required, return-before-issue, or duplicate return is accepted only into the owner's protocol-fault handling. It increments the independently typed stale/range/unsolicited/issue/duplicate instrument, marks the frame fatal, and **does not set a committed bit, overwrite a result plane, change final status/index, or satisfy the required mask**. A later correct required return is still required for ordinary drain.

`clut_idx_c` is captured into the relevant sample result before palette lookup replaces the index with RGB. `zhao_texture_material_combine_v2` receives the four 48-bit source planes (or their status/index side fields), computes material RGB/A, forwards sample-0 index, and ORs required statuses. Merely widening the owner while leaving the combiner on low 32-bit operands would still lose both facts.

The same packet amends `design/contracts/TEXTURE.COMBINE.md` from its stale six-recipe/32-bit-source wording to all eight frozen recipes, the required-mask/status/index law above, and the actual paired-phase cadence. It amends `design/contracts/TEXTURE.CACHE.md` with the terminal refused fill alternative, sample status, and the accepted/completed/phase counters in section 9. A V3 port change without both contract amendments is incomplete.

The V3 top instantiates the generic owner at `RESW=48` and exposes status and index. Existing 40-bit standalone owner tests continue to elaborate the default owner parameter, so this change does not rewrite the owner experiment's law.

A refused V3 result remains terminal and retirable. The raster stage consumes its loud error value so the machine drains, but raises a sticky frame fault. The slot manager releases rather than publishes that render lease, so video repeats the previous complete frame. Suppressing the handshake on an ordinary V3 refusal would park the ordered head and convert a visible asset fault into a deadlock.

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
* fill request address;
* AUX sheet request U/V/token;
* V3 ordered RGB/A/index/status/continuation;
* the exact `RASTER.FRAGMENT` input packet;
* fragment tile-read and tile-write requests;
* resolve and framebuffer output packets.

No valid may be a function of its own ready. The direct join adds no valid/ready branch and no acceptance bubble.

### 4.5 Drain and bank swap

For `zhao_raster_tile_pipe_v2`, `pipe_empty` must mean all of the following:

```text
no Early-Z candidate held
candidate skid level == 0
attribute producer has no covered-pixel record held
V3 input has no unaccepted candidate
V3 owner/island quiet (including output reservation/queue)
no cache fill, response, palette, AUX, or combine work belonging to a live owner
no V3 ordered output held
RASTER.FRAGMENT idle
```

The V3 `quiet_o` must be derived from owner quiescence and checked against subordinate queues; it must not be a delayed guess used in admission. The swap condition remains `walk done && pending mask empty && no new coverage accept && pipe_empty`, followed by the existing resolve-ready gate.

The next non-final triangle for the same tile also waits for this drain in the minimum version. That preserves today's simple per-triangle handoff. Inter-triangle texture overlap is an optimization only after this composition meets its measured frame budget.

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

`zhao_texture_aux_pipe` gains the sheet handle as an offer-FIFO field and output. Its accepted surface request is:

```text
req_op       = READ
req_handle   = sealed sheet_handle32
req_texel    = {sheet_v[5:0], sheet_u[5:0]}
req_src_id   = zero_extend(owner_handle14) to 16 bits
```

The sheet response returns its 2-bit status and the same source ID/token. `HIT` supplies tag/strength; `MISS` supplies zero data plus `SOURCE_REFUSED`. The sheet store's full-handle associative lookup is the authority on generation. The AUX pipe must not turn a MISS into a successful all-zero texel. Its existing no-response-ready contract is retained, so its response reservation includes the status bit as well as token/tag/strength.

Interleaved live owners may name sheet A/envelope A, sheet B/envelope B, then sheet A again while every stage is stalled independently. The required differential checks U/V, handle, token, status, tag, and strength for every accepted request and proves A's envelope cannot be paired with B's sheet. A committed slot-swap mutant updates descriptor data from the currently offered owner while retaining the prior owner token; the independent expected record is captured at admission and must fire. Separate stale-generation and same-index/new-generation cases prove that comparing only the patch index is insufficient.

The 64-bit low portion remains exactly `[31:0]=wx`, `[63:32]=wz`; the AUX instantiation uses `.req_wx_i(aux_ctx.wx)` and `.req_wz_i(aux_ctx.wz)`. The current raw slices `.req_wx_i(ctx[31:0])` and `.req_wz_i(ctx[63:32])` are equivalent, but new code uses the typed names so a visually plausible `{wx,wz}` concatenation cannot reverse them.

### 5.2 Binner metadata

Because binner acceptance and drain are separated, the attribute/material packet must be stored under the binner's own accepted triangle identity. There are two safe forms:

1. a versioned binner with a parameterized opaque metadata plane written and read on the same `tri_wa`/`tri_ra` as its triangle entry; or
2. a descriptor bank addressed by a binner-exported triangle cookie allocated by the binner itself.

A wrapper-maintained rolling pointer is refused. It could drift on a triangle the binner declines or token-gates.

For the characterization module, `zhao_geom_binner_v2` should use the first form: a statically sliced metadata bank, an exact same-edge write, and an exact same-record drain. The old binner remains the oracle for tile-reference order and overflow behavior.

This does **not** settle the production binner scale. `TRI_CAP=128`, binning at five clocks per one-tile triangle, and frame-end-before-drain are not a legal sink for the full terrain producer described below. That wall must be solved before the final terrain composition; merely widening the metadata of the 128-entry experiment is not adoption.

## 6. Binding and palette composition

A connected production-facing V3 cannot use one live global `bind_base_i/bind_mode_i` while several owners are in flight.

The minimum resolver packet is:

```text
expanded request:
  sample_handle[15:0]
  binding_selector[7:0]
  U[31:0], V[31:0]
  LOD[7:0]

resolved request:
  route_token {derived_class[1:0], sample_handle[15:0]}
  base[31:0]
  mode[31:0]
  palette_slot[1:0]
  palette_generation[7:0]
  U, V, LOD
```

`zhao_texture_frag_expand` must emit the selected binding (`base binding + sample index` for the currently implemented convention) and the 16-bit sample handle separately. A one-request-per-clock `zhao_texture_binding_resolver` then reads a frame-sealed binding record, derives class from resolved format/filter, and feeds `zhao_texture_tmu_plan`. The issue notification to V3 occurs on **resolved planner acceptance**, not on insertion into the resolver. An invalid binding produces an issue plus terminal refused completion with the same full handle, so it can retire.

Palette slot and generation belong to the resolved binding, not to response routing bits. The current per-fragment pair may remain as a compatibility input for the paired differential, but the connected path must either prove all required samples intentionally share it or move the pair into the resolved record.

Binding-table writes and palette generation changes are frame-sealed GPU-domain commands. No owner may observe a table generation change between issue and return. If the table is double-buffered, the active generation joins the accepted material packet; a mid-frame live-table write is a sticky asset fault.

No claim about explicit three-UV-set materials is made here. Before sample counts 2/3 are enabled in the connected shell, the material differential must either prove common UV/LOD plus consecutive bindings for every legal recipe or widen the accepted descriptor to explicit per-sample fields. The one-sample baseline does not prove that question.

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
AJ_required       sum of admitted aux_required bits
AJ_accepted       logical AUX jobs accepted from the owner expander
AJ_sheet_accept   Surface Sheet request handshakes
AJ_local_refused  accepted AUX jobs terminated before a sheet request
AJ_completed      full-identity AUX terminal returns committed by the owner
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

`SJ_local_refused` includes invalid sealed binding/material requests that issue a matching terminal refusal without touching the planner. `AJ_local_refused` includes invalid sealed handle/envelope requests terminated without a sheet read. Stale, duplicate, out-of-range, pre-issue, or unrequested returns increment only their protocol-fault counters; they do not increment `SJ_completed` or `AJ_completed` because they did not satisfy an owner plane.

After complete drain, the exact closure is:

```text
S == F == owner_admitted == owner_emitted == owner_released == O
SJ_required == SJ_accepted
SJ_accepted == SJ_planner_accept + SJ_local_refused
SJ_completed == SJ_accepted
AJ_required == AJ_accepted
AJ_accepted == AJ_sheet_accept + AJ_local_refused
AJ_completed == AJ_accepted
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

### 12.1 Interface and ownership gates

1. A source/elaboration inventory proves the V2 raster candidate fields and widths exactly, including Early-Z `PAYLOAD_W=410`, skid `W=490`, retirement context 160, owner context 224, typed AUX context 224, early descriptor 279, and result 48.
2. A role-aware checker rooted at the connected top requires exactly one provider of `raster_texture_fragment_lifecycle`. `zhao_texture_v3own` is that provider. Expanders, descriptor banks, context banks, skids, and the sequence witness declare transport/storage/check roles, not ownership.
3. A test-only duplicate-owner composition marks both V3 ownership and TEXJOIN ownership for that role. The checker must fail. Counting module containment without role metadata is insufficient.
4. Generated production/accounting tops are checked separately; a disconnected selected sibling cannot satisfy connected ownership.

### 12.2 V3 differential and result gates

Drive the old island oracle and V3 with identical supported traffic and compare existing RGB/A/tag/refused order. Separately compare the new index/status/context fields against the frozen TMU/material laws:

* CLUT8 indices including 0 and values whose RGB has no identifying relationship;
* both CLUT4 nibbles;
* all direct formats, which must return index 0;
* every sample count and recipe, including PASSTHRU count 0 with no TMU issue and count 0 plus required AUX;
* exact `{AUX,sample2,sample1,sample0}` required masks, proving unrequested/stale/duplicate/pre-issue returns neither commit nor satisfy a bit;
* status OR across only committed required sources plus material refusal, and final index equal to sample-0 index or zero at count 0;
* interleaved live sheet A/envelope A, sheet B/envelope B, then sheet A again under independent descriptor, offer, and response stalls;
* same sheet index with old/new generation, sheet MISS, envelope swap, degenerate envelope, AUX absent, and AUX required;
* palette stale/cold and invalid binding terminal refusal;
* output backpressure while later work completes out of order.

The star-disc/halo and alpha-test cases run end to end through `RASTER.FRAGMENT`; forcing index zero or substituting a palette-color byte must fail them.

### 12.3 Full-identity stall gate

An independent scoreboard records every accepted V3 fragment as:

```text
external: all 160 retirement-context bits plus every V3 request field
internal sample work: {slot[5:0], generation[7:0], sample_index[1:0]}
internal AUX work:    {slot[5:0], generation[7:0]}
```

It then applies long, independently randomized stalls at V3 input, planner/cache response, AUX response, V3 output, fragment input, tile read/write, resolve, and framebuffer output. On every stalled cycle it checks `valid` and every payload bit, not only the eventually accepted packet. Aggregate accepted/emitted counters are secondary.

A committed renamed context-read/slot-swap mutant changes a held record while keeping its token/counters plausible. The normal driver asserts correct hold and order; a separate inverse-polarity mutant driver passes only when the independent mismatch detector fires. The detector operands must not share the corrupted enable.

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
* valid simultaneous TMU and AUX returns;
* generation wrap/drain;
* head incomplete while later owners finish.

The six refusal/error classes remain separately observable. A sum over an unstated subset is not evidence.

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

Binding tests alternate materials every accepted fragment while prior owners remain live; each request must use its own sealed base/mode/palette generation. A late-read-global-binding mutant must fail.

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

Long all-ready homogeneous and mixed streams check every accepted/completed equality and the `J1 + 2*J2 + 3*J3` phase equation in section 9.2. Separate controls duplicate an in-flight sampler job and a combiner phase while keeping output bytes unchanged; both must fail exact job/phase deltas. The steady-state report names accepted fragments, samples, AUX jobs, cache jobs/fills, combiner jobs, issued/completed phases, and ordered outputs. An owner-edge one-per-clock result without those downstream deltas cannot pass.

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

A packet may be developed while an earlier independent fit runs, but it cannot be promoted past its gate or selected by a dependent packet until every predecessor is green. If an upstream packet is reverted, every dependent packet is reverted in reverse order. There is no supported mixed state with new ports and old manifests, contracts, generated accounting RTL, or source closure.

The following closure law applies to **every** packet, not only final adoption:

1. A module addition, removal, rename, parameter-interface change, or port change updates the production block ledger/manifest consumed by `tools/quartus/check_prod_manifest.py` in that same packet.
2. The same packet regenerates `fpga/rtl/prod/zhao_prod_top.sv` with `tools/quartus/gen_prod_top.py` and regenerates/checks the production fit's exact source closure. A zero semantic diff is still freshness-checked against the changed source state.
3. Every new module not instantiated by the selected production hierarchy is registered immediately as `excluded:not-yet-adopted` (or the existing schema's exact equivalent carrying that reason). Merely listing it as a source is not adoption. Fit-only wrappers are registered as fit-only, never production providers.
4. Every V3 port change additionally updates all direct instantiations, the V3 interface manifest, the accounting top, and its source list in that packet. There is no later manifest-cleanup packet.
5. Rollback restores RTL, contracts, ledger/manifest, generated accounting top, fit targets, and source lists from the same packet atomically.

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

* `fpga/rtl/texture/zhao_texture_island_v3_top.sv`: `RCTXW=160`, `AUXCTXW=224`, retirement-context/status/raw-index/quiet ports and `RESW=48`;
* `fpga/rtl/texture/zhao_texture_early_desc_v2.sv`, `fpga/rtl/texture/zhao_texture_frag_expand.sv`, and `fpga/rtl/texture/zhao_texture_aux_pipe.sv`: atomic full AUX handle/envelope capture and carriage;
* `fpga/rtl/texture/zhao_texture_material_combine_v2.sv`: exact required-source reduction, sample-0 index, status OR, accepted/completed job and phase counters;
* `fpga/rtl/texture/zhao_texture_binding_resolver.sv`: sealed per-request binding carriage and accepted-issue timing;
* `design/contracts/TEXTURE.COMBINE.md`: all eight recipes, exact count laws, 48-bit source/result semantics, and paired-phase cadence;
* `design/contracts/TEXTURE.CACHE.md`: versioned terminal-refusal interface and accepted/completed cache/fill accounting to be implemented in Packet E;
* `design/contracts/SURFACE.SHEET.md`: generation-bearing handle/status behavior used by owner-sealed AUX;
* V3/AUX differentials, interleaved-sheet tests, required-mask tests, renamed committed mutants, and all same-packet manifest/accounting/source-closure regeneration required by the V3 port change.

`zhao_texture_v3own.sv` remains the sole lifecycle owner and semantically unchanged; its standalone default-width suite remains. Any new resolver/descriptor module is `excluded:not-yet-adopted` immediately.

**Gate:** count-0 and AUX-required behavior, all recipe/count combinations, raw index/status, interleaved sheet/envelope generations, full-context stalls, malformed returns, exact job/phase counts, queue-overflow control, and uninterrupted prepared ordered output.

**Rollback:** revert every V3 port/width consumer, all three contract amendments, tests/mutants, and the regenerated manifest/accounting/source closure together. The old island/TEXJOIN experiments remain available; no raster selects the new boundary.

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
* `fpga/rtl/texture/zhao_texture_cache_pipe.sv` terminal fill-refusal implementation and the Packet-B cache accounting;
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
* raw sample-0 index and status reach `RASTER.FRAGMENT`;
* `invw24` is preserved bit-for-bit and is not confused with projected `w`;
* full caller source identity reaches the fragment RMW transaction;
* every state transition is tied to a ready/valid acceptance;
* full payloads hold under all stalls;
* the output can retire one prepared result per clock without an adapter bubble;
* ENGINE1 locally shares geometry and texture while client 5 remains unspent;
* ENGINE1 request acceptance and its registered verdict are counted separately, and denial completes exactly once;
* fill refusal is terminal rather than a hidden deadlock;
* the GPU-domain lease captures `{writer,slot,generation,mode,base,span}` and both guards use only that record;
* only an accepted clean publication creates a generation-bearing READY CDC event; fault release and no-ready frames repeat the previous complete display;
* accepted/completed fragment, sample, AUX, cache, fill, combiner-job, and combiner-phase counts close at drain;
* tile swap and frame publication include V3/memory drain;
* both views share one terrain/projector path and one texture island;
* the legal RPP3/MATW18 two-view workload, not RPP1 or a 128-triangle fixture, is the final workload;
* G8B elaborates the parameter-fixed `zhao_terrain_pipe_rpp3_matw18_fit_top`, not raw-module defaults;
* every changed V3 port and every unadopted module is reconciled with the production manifest, generated accounting top, and source closure in its own packet;
* only clean connected fits decide the comfortable-margin target below 30,000 ALMs and 85 DSPs.

### 14.1 Remaining HOLDs

The architecture decision does not clear implementation or production adoption. The explicit HOLDs are:

1. **V3/raster HOLD:** Packets A–E and their differentials, source-status/index reduction, owner-sealed AUX descriptor, guard-denial controls, and G8A connected receipt do not yet exist as this report's evidence.
2. **Attribute/material HOLD:** exact `invw24`, UV/W, color, world-X/Z, sheet handle, and envelope carriage must close; sample counts 2/3 remain disabled until common-UV/LOD/consecutive-binding sufficiency is proven or the descriptor is explicitly widened.
3. **Lease/CDC HOLD:** writer-aware slot arbitration, captured base/span, clean-publish/fault-release behavior, accepted-ready CDC, generation-matched swap return, and reset barrier must pass Packet G before shell V2 can qualify.
4. **Shell HOLD:** `zhao_shell_top_v2` is not implemented or selected; `zhao_shell_top.sv` remains the byte-identical live historical specimen until the final selection packet.
5. **Terrain HOLD:** normals, projected-`w` depth quantization, owner-sealed terrain identity, and production-scale binner/tile scheduling must close before the parameter-fixed G8B and legal two-view end-to-end gate.
6. **Fit/budget HOLD:** no ALM, DSP, M10K, Fmax, or physical saving is inferred. G8A, G8B, and G8C must each produce its named clean connected receipt; production requires G8C's comfortable measured margin below 30,000 ALMs and 85 DSPs.
7. **Adoption/accounting HOLD:** no candidate becomes production and TEXJOIN is not marked superseded until Packet K atomically selects the connected hierarchy and regenerates every manifest/accounting/source-closure artifact.

This report does **not** claim that the current shell contains V3, that the current terrain candidate can feed the current binner at production scale, that the selected material interface already covers every three-sample recipe, that any new module fits, that the whole machine is under budget, or that retiring TEXJOIN saves physical silicon. Those are the gates, not the starting assumptions.
